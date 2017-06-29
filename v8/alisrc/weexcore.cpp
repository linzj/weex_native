#include "LogUtils.h"
#include "android/log.h"
#include "jni.h"

#include "V8DefaultPlatform.h"
#include "V8ScriptRunner.h"

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <time.h>
#include <v8-profiler.h>
#include <v8.h>
#include "Trace.h"

jclass jBridgeClazz;
jmethodID jCallAddElementMethodId;
jmethodID jDoubleValueMethodId;
jmethodID jSetTimeoutNativeMethodId;
jmethodID jCallNativeMethodId;
jmethodID jCallNativeModuleMethodId;
jmethodID jCallNativeComponentMethodId;
jmethodID jLogMethodId;
jobject jThis;
JavaVM* sVm = NULL;

// for makeIdleNotification
static const size_t SHORT_TERM_IDLE_TIME_IN_MS = 10;
static const size_t LONG_TERM_IDLE_TIME_IN_MS = 1000;
static const unsigned long MIN_EXECJS_COUNT = 100;
static const unsigned long MAX_EXECJS_COUNT = LONG_MAX;
static unsigned long execJSCount = 0;
static unsigned long lastExecJSCount = 0;
static const int samplingBegin = 5;
static const int denomValue = 3;
static unsigned long samplingExecJSCount = 0;
static const int samplingExecJSCountMax = 10;
static bool idle_notification_FLAG = true;

long getCPUTime() {
  struct timespec ts;
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
  return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static void ReportException(v8::Isolate* isolate, v8::TryCatch* try_catch,
                            jstring jinstanceid, const char* func);
static bool ExecuteJavaScript(v8::Isolate* isolate,
                              v8::Local<v8::String> source,
                              bool report_exceptions);
static void makeIdleNotification(v8::Isolate* isolate);
static void resetIdleNotificationCount();
static void takeHeapSnapshot(const char* filename);
static void timerTraceProfilerInMainThread(const char* name, int status);

static v8::Local<v8::Context> CreateShellContext(v8::Local<v8::ObjectTemplate>);
static void callNative(const v8::FunctionCallbackInfo<v8::Value>& args);
static void setTimeoutNative(const v8::FunctionCallbackInfo<v8::Value>& args);
static void nativeLog(const v8::FunctionCallbackInfo<v8::Value>& name);
static void notifyTrimMemory(const v8::FunctionCallbackInfo<v8::Value>& args);
static void notifySerializeCodeCache(
    const v8::FunctionCallbackInfo<v8::Value>& args);
static void markupState(const v8::FunctionCallbackInfo<v8::Value>& args);
static const char* getCacheDir(JNIEnv* env);
extern "C" {
    extern const char* cache_dir;
}


static v8::Persistent<v8::Context> V8context;
static v8::Isolate* globalIsolate;
static bool v8_platform_inited = false;
static v8::platform1::V8DefaultPlatform* v8_platform;

class AsciiOutputStream : public v8::OutputStream {
 public:
  AsciiOutputStream(FILE* stream) : m_stream(stream) {}

  virtual int GetChunkSize() { return 51200; }

  virtual void EndOfStream() {}

  virtual WriteResult WriteAsciiChunk(char* data, int size) {
    if (NULL == m_stream) return kAbort;

    const size_t len = static_cast<size_t>(size);
    size_t offset = 0;
    while (offset < len && !feof(m_stream) && !ferror(m_stream)) {
      offset += fwrite(data + offset, 1, len - offset, m_stream);
    }

    return offset == len ? kContinue : kAbort;
  }

 private:
  FILE* m_stream;
};

static void timerTraceProfilerInMainThread(const char* name, int status) {
  if (!status) {
    TRACE_EVENT_BEGIN0("v8", name);
  } else {
    TRACE_EVENT_END0("v8", name);
  }
}

JNIEnv* getJNIEnv() {
  JNIEnv* env = NULL;
  if ((sVm)->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK) {
    return JNI_FALSE;
  }
  return env;
}

const char* ToCString(const v8::String::Utf8Value& value) {
  return *value ? *value : "<string conversion failed>";
}

const char* jstringToCString(JNIEnv* env, jstring str) {
  if (str != NULL) {
    const char* c_str = env->GetStringUTFChars(str, NULL);
    return c_str;
  }
  return "";
}

v8::Local<v8::Value> jString2V8String(JNIEnv* env, jstring str) {
  if (str != NULL) {
    const char* c_str = env->GetStringUTFChars(str, NULL);
    if (c_str) {
      v8::Local<v8::Value> ret = v8::String::NewFromUtf8(globalIsolate, c_str);
      env->ReleaseStringUTFChars(str, c_str);
      return ret;
    }
  }
  return v8::String::NewFromUtf8(globalIsolate, "");
}

extern "C" {
void jString2Log(JNIEnv* env, jstring instance, jstring str) {
  if (str != NULL) {
    const char* c_instance = env->GetStringUTFChars(instance, NULL);
    const char* c_str = env->GetStringUTFChars(str, NULL);
    if (c_str) {
      LOGA("jsLog>>> instance :%s,c_str:%s", c_instance, c_str);
    }
    env->ReleaseStringUTFChars(instance, c_instance);
    env->ReleaseStringUTFChars(str, c_str);
  }
}

jstring v8String2JString(JNIEnv* env, v8::String::Value& value) {
  if (value.length() == 0) {
    return env->NewStringUTF("");  // 降级到使用NewStringUTF来创建一个""字符串
  } else {
    return env->NewString(*value, value.length());
  }
}

void setJSFVersion(JNIEnv* env) {
  v8::Isolate* isolate = globalIsolate;
  v8::HandleScope handleScope(isolate);
  v8::Isolate::Scope isolate_scope(isolate);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, V8context);
  v8::Context::Scope ctx_scope(context);

  v8::Local<v8::Object> global = context->Global();
  v8::Local<v8::Function> getJSFMVersion;
  v8::Local<v8::Value> version;
  getJSFMVersion = v8::Local<v8::Function>::Cast(
      global->Get(v8::String::NewFromUtf8(isolate, "getJSFMVersion")));
  version = getJSFMVersion->Call(global, 0, NULL);
  v8::String::Utf8Value str(version);

  jmethodID tempMethodId = (env)->GetMethodID(jBridgeClazz, "setJSFrmVersion",
                                              "(Ljava/lang/String;)V");
  LOGA("init JSFrm version %s", ToCString(str));
  jstring jversion = (env)->NewStringUTF(*str);
  (env)->CallVoidMethod(jThis, tempMethodId, jversion);
  env->DeleteLocalRef(jversion);
}

jint native_execJSService(JNIEnv* env, jobject object, jstring script) {
  if (globalIsolate != NULL && script != NULL) {
    const char* scriptStr = env->GetStringUTFChars(script, NULL);
    v8::HandleScope handleScope(globalIsolate);
    v8::Local<v8::String> source =
        v8::String::NewFromUtf8(globalIsolate, scriptStr);
    base::debug::TraceScope traceScope("weex", "execJSService");
    if (scriptStr == NULL || !ExecuteJavaScript(globalIsolate, source, true)) {
      LOGE("jsLog JNI_Error >>> scriptStr :%s", scriptStr);
      return false;
    }
    env->ReleaseStringUTFChars(script, scriptStr);
    return true;
  }
  return false;
}

void native_takeHeapSnapshot(JNIEnv* env, jobject object, jstring name) {
    return;
}

jint native_initFramework(JNIEnv* env, jobject object, jstring script,
                          jobject params) {
  jThis = env->NewGlobalRef(object);
  jclass c_params = env->GetObjectClass(params);
  // no flush to avoid SIGILL
  // const char* str= "--noflush_code_incrementally --noflush_code
  // --noage_code";
  const char* str =
      "--noflush_code --noage_code --nocompact_code_space"
      " --expose_gc --code-comments";
  {
      cache_dir = getCacheDir(env);
      LOGE("cache_dir: %s", cache_dir);
      std::string path(cache_dir);
      std::string stdoutlog = path + "/stdout.log";
      std::string stderrlog = path + "/stderr.log";
      freopen(stdoutlog.c_str(), "w", stdout);
      freopen(stderrlog.c_str(), "w", stderr);
  }
  v8::V8::SetFlagsFromString(str, strlen(str));

  // The embedder needs to tell v8 whether needs to
  // call 'v8::V8::InitializePlatform' with a new v8 platform.
  jmethodID jNeedInitV8Id =
      env->GetMethodID(c_params, "getNeedInitV8", "()Ljava/lang/String;");
  jstring jNeedInitV8Str =
      (jstring)env->CallObjectMethod(params, jNeedInitV8Id);
  const char* c_needInitV8Str = env->GetStringUTFChars(jNeedInitV8Str, NULL);
  if (!v8_platform_inited && c_needInitV8Str != NULL &&
      !strcmp(c_needInitV8Str, "1")) {
    v8_platform = v8::platform1::CreateV8DefaultPlatform();
    v8::V8::InitializePlatform(v8_platform);
    v8_platform_inited = true;
  }

  v8::V8::Initialize();
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  globalIsolate = v8::Isolate::New(create_params);
  globalIsolate->Enter();
  globalIsolate->SetEventLogger(timerTraceProfilerInMainThread);
  using base::debug::TraceEvent;
  TraceEvent::StartATrace(env);
  v8::Isolate* isolate = globalIsolate;
  base::debug::TraceScope traceScope("weex", "initFramework");
  v8::HandleScope handleScope(isolate);

  v8::Local<v8::ObjectTemplate> WXEnvironment;
  WXEnvironment = v8::ObjectTemplate::New(isolate);

  jmethodID m_platform =
      env->GetMethodID(c_params, "getPlatform", "()Ljava/lang/String;");
  jobject platform = env->CallObjectMethod(params, m_platform);
  WXEnvironment->Set(v8::String::NewFromUtf8(isolate, "platform"),
                     jString2V8String(env, (jstring)platform));
  env->DeleteLocalRef(platform);

  jmethodID m_osVersion =
      env->GetMethodID(c_params, "getOsVersion", "()Ljava/lang/String;");
  jobject osVersion = env->CallObjectMethod(params, m_osVersion);
  WXEnvironment->Set(v8::String::NewFromUtf8(isolate, "osVersion"),
                     jString2V8String(env, (jstring)osVersion));
  env->DeleteLocalRef(osVersion);

  jmethodID m_appVersion =
      env->GetMethodID(c_params, "getAppVersion", "()Ljava/lang/String;");
  jobject appVersion = env->CallObjectMethod(params, m_appVersion);
  WXEnvironment->Set(v8::String::NewFromUtf8(isolate, "appVersion"),
                     jString2V8String(env, (jstring)appVersion));
  env->DeleteLocalRef(appVersion);

  jmethodID m_weexVersion =
      env->GetMethodID(c_params, "getWeexVersion", "()Ljava/lang/String;");
  jobject weexVersion = env->CallObjectMethod(params, m_weexVersion);
  WXEnvironment->Set(v8::String::NewFromUtf8(isolate, "weexVersion"),
                     jString2V8String(env, (jstring)weexVersion));
  env->DeleteLocalRef(weexVersion);

  jmethodID m_deviceModel =
      env->GetMethodID(c_params, "getDeviceModel", "()Ljava/lang/String;");
  jobject deviceModel = env->CallObjectMethod(params, m_deviceModel);
  WXEnvironment->Set(v8::String::NewFromUtf8(isolate, "deviceModel"),
                     jString2V8String(env, (jstring)deviceModel));
  env->DeleteLocalRef(deviceModel);

  // jmethodID m_appName =
  //     env->GetMethodID(c_params, "getAppName", "()Ljava/lang/String;");
  // jobject appName = env->CallObjectMethod(params, m_appName);
  // WXEnvironment->Set(v8::String::NewFromUtf8(isolate, "appName"),
  //                    jString2V8String(env, (jstring)appName));
  // env->DeleteLocalRef(appName);

  jmethodID m_deviceWidth =
      env->GetMethodID(c_params, "getDeviceWidth", "()Ljava/lang/String;");
  jobject deviceWidth = env->CallObjectMethod(params, m_deviceWidth);
  WXEnvironment->Set(v8::String::NewFromUtf8(isolate, "deviceWidth"),
                     jString2V8String(env, (jstring)deviceWidth));
  env->DeleteLocalRef(deviceWidth);

  jmethodID m_deviceHeight =
      env->GetMethodID(c_params, "getDeviceHeight", "()Ljava/lang/String;");
  jobject deviceHeight = env->CallObjectMethod(params, m_deviceHeight);
  WXEnvironment->Set(v8::String::NewFromUtf8(isolate, "deviceHeight"),
                     jString2V8String(env, (jstring)deviceHeight));
  env->DeleteLocalRef(deviceHeight);

  jmethodID m_options =
      env->GetMethodID(c_params, "getOptions", "()Ljava/lang/Object;");
  jobject options = env->CallObjectMethod(params, m_options);
  jclass jmapclass = env->FindClass("java/util/HashMap");
  jmethodID jkeysetmid =
      env->GetMethodID(jmapclass, "keySet", "()Ljava/util/Set;");
  jmethodID jgetmid = env->GetMethodID(
      jmapclass, "get", "(Ljava/lang/Object;)Ljava/lang/Object;");
  jobject jsetkey = env->CallObjectMethod(options, jkeysetmid);
  jclass jsetclass = env->FindClass("java/util/Set");
  jmethodID jtoArraymid =
      env->GetMethodID(jsetclass, "toArray", "()[Ljava/lang/Object;");
  jobjectArray jobjArray =
      (jobjectArray)env->CallObjectMethod(jsetkey, jtoArraymid);
  env->DeleteLocalRef(jsetkey);
  if (jobjArray != NULL) {
    jsize arraysize = env->GetArrayLength(jobjArray);
    for (int i = 0; i < arraysize; i++) {
      jstring jkey = (jstring)env->GetObjectArrayElement(jobjArray, i);
      jstring jvalue = (jstring)env->CallObjectMethod(options, jgetmid, jkey);
      if (jkey != NULL) {
        const char* c_key = env->GetStringUTFChars(jkey, NULL);
        WXEnvironment->Set(v8::String::NewFromUtf8(isolate, c_key),
                           jString2V8String(env, jvalue));
        env->DeleteLocalRef(jkey);
        if (jvalue != NULL) {
          env->DeleteLocalRef(jvalue);
        }
      }
    }
    env->DeleteLocalRef(jobjArray);
  }
  env->DeleteLocalRef(options);

  resetIdleNotificationCount();

  V8context.Reset(isolate, CreateShellContext(WXEnvironment));

  if (script != NULL) {
    const char* scriptStr = env->GetStringUTFChars(script, NULL);
    if (scriptStr == NULL ||
        !ExecuteJavaScript(globalIsolate,
                           v8::String::NewFromUtf8(isolate, scriptStr), true)) {
      return false;
    }

    setJSFVersion(env);
    env->ReleaseStringUTFChars(script, scriptStr);
  }
  env->DeleteLocalRef(c_params);

  return true;
}

/**
 * Called to execute JavaScript such as . createInstance(),destroyInstance ext.
 *
 */
jint native_execJS(JNIEnv* env, jobject jthis, jstring jinstanceid,
                   jstring jnamespace, jstring jfunction, jobjectArray jargs) {
  if (globalIsolate == NULL) {
    return false;
  }

  v8::Isolate* isolate = globalIsolate;
  v8::HandleScope handleScope(isolate);
  v8::Isolate::Scope isolate_scope(isolate);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, V8context);
  v8::Context::Scope ctx_scope(context);
  v8::Local<v8::Object> global = context->Global();

  v8::TryCatch try_catch;

  if (jfunction == NULL || jinstanceid == NULL) {
    LOGE("execJS function is NULL!");
    return false;
  }
  int length = 0;
  if (jargs != NULL) {
    length = env->GetArrayLength(jargs);
  }
  v8::Local<v8::Value> obj[length ? length : 1];

  jclass jsObjectClazz = env->FindClass("com/taobao/weex/bridge/WXJSObject");
  for (int i = 0; i < length; i++) {
    jobject jArg = env->GetObjectArrayElement(jargs, i);

    jfieldID jTypeId = env->GetFieldID(jsObjectClazz, "type", "I");
    jint jTypeInt = env->GetIntField(jArg, jTypeId);

    jfieldID jDataId =
        env->GetFieldID(jsObjectClazz, "data", "Ljava/lang/Object;");
    jobject jDataObj = env->GetObjectField(jArg, jDataId);
    if (jTypeInt == 1) {
      if (jDoubleValueMethodId == NULL) {
        jclass jDoubleClazz = env->FindClass("java/lang/Double");
        jDoubleValueMethodId =
            env->GetMethodID(jDoubleClazz, "doubleValue", "()D");
        env->DeleteLocalRef(jDoubleClazz);
      }
      jdouble jDoubleObj =
          env->CallDoubleMethod(jDataObj, jDoubleValueMethodId);
      obj[i] = v8::Number::New(isolate, (double)jDoubleObj);
    } else if (jTypeInt == 2) {
      jstring jDataStr = (jstring)jDataObj;
      obj[i] = jString2V8String(env, jDataStr);
    } else if (jTypeInt == 3) {
      v8::TryCatch try_catch;
      v8::Local<v8::Value> jsonObj[1];
      v8::Local<v8::Object> json;
      v8::Local<v8::Function> json_parse;
      json = v8::Local<v8::Object>::Cast(
          global->Get(v8::String::NewFromUtf8(isolate, "JSON")));
      json_parse = v8::Local<v8::Function>::Cast(
          json->Get(v8::String::NewFromUtf8(isolate, "parse")));
      jsonObj[0] = jString2V8String(env, (jstring)jDataObj);
      v8::Local<v8::Value> ret = json_parse->Call(json, 1, jsonObj);
      obj[i] = ret;
      if (try_catch.HasCaught()) {
        v8::String::Utf8Value utf8Obj(jsonObj[0]->ToString());
        ReportException(globalIsolate, &try_catch, jinstanceid,
                        ToCString(utf8Obj));
        env->DeleteLocalRef(jDataObj);
        env->DeleteLocalRef(jArg);
        env->DeleteLocalRef(jsObjectClazz);
        return false;
      }
    }
    env->DeleteLocalRef(jDataObj);
    env->DeleteLocalRef(jArg);
  }
  env->DeleteLocalRef(jsObjectClazz);

  const char* func = env->GetStringUTFChars(jfunction, 0);
  v8::Local<v8::Function> function;
  base::debug::TraceScope traceScope("weex", "exeJS", "function", func);
  v8::Local<v8::Value> result;
  if (jnamespace == NULL) {
    function = v8::Local<v8::Function>::Cast(
        global->Get(v8::String::NewFromUtf8(isolate, func)));
    result = function->Call(global, length, obj);
  } else {
    v8::Local<v8::Object> master = v8::Local<v8::Object>::Cast(
        global->Get(jString2V8String(env, jnamespace)));
    function = v8::Local<v8::Function>::Cast(
        master->Get(jString2V8String(env, jfunction)));
    result = function->Call(master, length, obj);
  }

  makeIdleNotification(isolate);

  if (result.IsEmpty()) {
    assert(try_catch.HasCaught());
    ReportException(globalIsolate, &try_catch, jinstanceid, func);
    env->ReleaseStringUTFChars(jfunction, func);
    return false;
  }
  env->ReleaseStringUTFChars(jfunction, func);
  static unsigned exe_JSCount = 0;
  if (((++exe_JSCount) & 127) == 0) {
    isolate->ScheduleSaveCacheOnIdle();
  }
  for (int i = 0; i < 10; ++i) {
    if (!v8_platform->PumpMessageLoop(globalIsolate)) break;
  }
  return true;
}
}

/**
 * this function is to execute a section of JavaScript content.
 */
bool ExecuteJavaScript(v8::Isolate* isolate, v8::Local<v8::String> source,
                       bool report_exceptions) {
  v8::Isolate::Scope isolate_scope(isolate);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, V8context);
  v8::Context::Scope ctx_scope(context);
  v8::TryCatch try_catch;
  if (source.IsEmpty()) {
    if (report_exceptions) ReportException(isolate, &try_catch, NULL, "");
    return false;
  }
  v8::Local<v8::String> name = v8::String::NewFromUtf8(isolate, "(weex)");
  v8::Local<v8::Script> script = v8::Script::Compile(source, name);
  if (script.IsEmpty()) {
    if (report_exceptions) ReportException(isolate, &try_catch, NULL, "");
    return false;
  } else {
    v8::Local<v8::Value> result = script->Run();
    if (result.IsEmpty()) {
      assert(try_catch.HasCaught());
      if (report_exceptions) ReportException(isolate, &try_catch, NULL, "");
      return false;
    } else {
      assert(!try_catch.HasCaught());
      return true;
    }
  }
}

void reportException(jstring jInstanceId, const char* func,
                     const char* exception_string) {
  JNIEnv* env = getJNIEnv();
  jstring jExceptionString = env->NewStringUTF(exception_string);
  jstring jFunc = env->NewStringUTF(func);
  jmethodID tempMethodId = env->GetMethodID(
      jBridgeClazz, "reportJSException",
      "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
  env->CallVoidMethod(jThis, tempMethodId, jInstanceId, jFunc,
                      jExceptionString);
  env->DeleteLocalRef(jExceptionString);
  env->DeleteLocalRef(jFunc);
}

/**
 *  This Function will be called when any javascript Exception
 *  that need to print log to notify  native happened.
 */
void ReportException(v8::Isolate* isolate, v8::TryCatch* try_catch,
                     jstring jinstanceid, const char* func) {
  v8::HandleScope handle_scope(isolate);
  v8::String::Utf8Value exception(try_catch->Exception());
  v8::Local<v8::Message> message = try_catch->Message();
  if (message.IsEmpty()) {
    // V8 didn't provide any extra information about this error; just
    // print the exception.
    LOGE(" ReportException : %s", ToCString(exception));
  } else {
    v8::String::Utf8Value filename(message->GetScriptResourceName());
    const char* filename_string = ToCString(filename);
    int lineNum = message->GetLineNumber();
    LOGE(" ReportException :%s:%i: %s", filename_string, lineNum,
         ToCString(exception));
    // Print line of source code.
    v8::String::Utf8Value stack_trace(try_catch->StackTrace());
    if (stack_trace.length() > 0) {
      const char* stack_trace_string = ToCString(stack_trace);
      LOGE(" ReportException : %s", stack_trace_string);
    }
  }
  reportException(jinstanceid, func, ToCString(exception));
}

/**
 *  This Function is a built-in function that JS bundle can execute
 *  to call native module.
 */
static void callNative(const v8::FunctionCallbackInfo<v8::Value>& args) {
  base::debug::TraceScope traceScope("weex", "callNative");
  JNIEnv* env = getJNIEnv();
  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, V8context);
  // instacneID args[0]
  jstring jInstanceId = NULL;
  if (!args[0].IsEmpty()) {
    v8::String::Utf8Value instanceId(args[0]);
    jInstanceId = env->NewStringUTF(*instanceId);
  }
  // task args[1]
  jbyteArray jTaskString = NULL;
  if (!args[1].IsEmpty() && args[1]->IsObject()) {
    v8::Local<v8::Value> obj[1];
    v8::Local<v8::Object> global = context->Global();
    v8::Local<v8::Object> json;
    v8::Local<v8::Function> json_stringify;
    json = v8::Local<v8::Object>::Cast(
        global->Get(v8::String::NewFromUtf8(isolate, "JSON")));
    json_stringify = v8::Local<v8::Function>::Cast(
        json->Get(v8::String::NewFromUtf8(isolate, "stringify")));
    obj[0] = args[1];
    v8::Local<v8::Value> ret = json_stringify->Call(json, 1, obj);
    v8::String::Utf8Value str(ret);

    int strLen = strlen(ToCString(str));
    jTaskString = env->NewByteArray(strLen);
    env->SetByteArrayRegion(jTaskString, 0, strLen,
                            reinterpret_cast<const jbyte*>(ToCString(str)));
  } else if (!args[1].IsEmpty() && args[1]->IsString()) {
    v8::String::Utf8Value tasks(args[1]);
    int strLen = strlen(*tasks);
    jTaskString = env->NewByteArray(strLen);
    env->SetByteArrayRegion(jTaskString, 0, strLen,
                            reinterpret_cast<const jbyte*>(*tasks));
  }
  // callback args[2]
  jstring jCallback = NULL;
  if (!args[2].IsEmpty()) {
    v8::String::Utf8Value callback(args[2]);
    jCallback = env->NewStringUTF(*callback);
  }

  if (jCallNativeMethodId == NULL) {
    jCallNativeMethodId =
        env->GetMethodID(jBridgeClazz, "callNative",
                         "(Ljava/lang/String;[BLjava/lang/String;)I");
  }

  int flag = 0;
  if (jThis) {
    flag = env->CallIntMethod(jThis, jCallNativeMethodId, jInstanceId,
                              jTaskString, jCallback);
  }
  if (flag == -1) {
    LOGE("instance destroy JFM must stop callNative");
  }
  env->DeleteLocalRef(jTaskString);
  env->DeleteLocalRef(jInstanceId);
  env->DeleteLocalRef(jCallback);

  args.GetReturnValue().Set(v8::Integer::New(isolate, flag));
}

/**
 *  This Function is a built-in function that JS bundle can execute
 *  to call native module.
 *  String instanceId, String module, String method, Object[] arguments, Object
 * options
 */
void callNativeModule(const v8::FunctionCallbackInfo<v8::Value>& args) {
  base::debug::TraceScope traceScope("weex", "callNativeModule");
  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, V8context);
  JNIEnv* env = getJNIEnv();

  // instacneID args[0]
  jstring jInstanceId = NULL;
  if (!args[0].IsEmpty()) {
    v8::String::Utf8Value instanceId(args[0]);
    jInstanceId = env->NewStringUTF(*instanceId);
  }

  // module args[1]
  jstring jmodule = NULL;
  if (!args[1].IsEmpty()) {
    v8::String::Utf8Value module(args[1]);
    jmodule = env->NewStringUTF(*module);
  }

  // method args[2]
  jstring jmethod = NULL;
  if (!args[2].IsEmpty()) {
    v8::String::Utf8Value method(args[2]);
    jmethod = env->NewStringUTF(*method);
  }

  // arguments args[3]
  jbyteArray jArgString = NULL;
  if (!args[3].IsEmpty() && args[3]->IsObject()) {
    v8::Local<v8::Value> obj[1];
    v8::Local<v8::Object> global = context->Global();
    v8::Local<v8::Object> json;
    v8::Local<v8::Function> json_stringify;
    json = v8::Local<v8::Object>::Cast(
        global->Get(v8::String::NewFromUtf8(isolate, "JSON")));
    json_stringify = v8::Local<v8::Function>::Cast(
        json->Get(v8::String::NewFromUtf8(isolate, "stringify")));
    obj[0] = args[3];
    v8::Local<v8::Value> ret = json_stringify->Call(json, 1, obj);
    v8::String::Utf8Value str(ret);

    int strLen = strlen(ToCString(str));
    jArgString = env->NewByteArray(strLen);
    env->SetByteArrayRegion(jArgString, 0, strLen,
                            reinterpret_cast<const jbyte*>(ToCString(str)));
  }

  // arguments args[4]
  jbyteArray jOptString = NULL;
  if (!args[4].IsEmpty() && args[4]->IsObject()) {
    v8::Local<v8::Value> obj[1];
    v8::Local<v8::Object> global = context->Global();
    v8::Local<v8::Object> json;
    v8::Local<v8::Function> json_stringify;
    json = v8::Local<v8::Object>::Cast(
        global->Get(v8::String::NewFromUtf8(isolate, "JSON")));
    json_stringify = v8::Local<v8::Function>::Cast(
        json->Get(v8::String::NewFromUtf8(isolate, "stringify")));
    obj[0] = args[4];
    v8::Local<v8::Value> ret = json_stringify->Call(json, 1, obj);
    v8::String::Utf8Value str(ret);

    int strLen = strlen(ToCString(str));
    jOptString = env->NewByteArray(strLen);
    env->SetByteArrayRegion(jOptString, 0, strLen,
                            reinterpret_cast<const jbyte*>(ToCString(str)));
  }

  if (jCallNativeModuleMethodId == NULL) {
    jCallNativeModuleMethodId =
        env->GetMethodID(jBridgeClazz, "callNativeModule",
                         "(Ljava/lang/String;"
                         "Ljava/lang/String;"
                         "Ljava/lang/String;"
                         "[B"
                         "[B)"
                         "Ljava/lang/Object;");
  }
  jobject result = NULL;
  if (jThis) {
    result = env->CallObjectMethod(jThis, jCallNativeModuleMethodId, jInstanceId,
                                   jmodule, jmethod, jArgString, jOptString);
  }
  v8::Local<v8::Value> ret;

  do {
    if (result == NULL) {
        break;
    }

    jclass jsObjectClazz = env->FindClass("com/taobao/weex/bridge/WXJSObject");
    jfieldID jTypeId = env->GetFieldID(jsObjectClazz, "type", "I");
    jint jTypeInt = env->GetIntField(result, jTypeId);
    jfieldID jDataId =
        env->GetFieldID(jsObjectClazz, "data", "Ljava/lang/Object;");
    jobject jDataObj = env->GetObjectField(result, jDataId);

    if (jDataObj == NULL) {
        break;
    }

    if (jTypeInt == 1) {
      if (jDoubleValueMethodId == NULL) {
        jclass jDoubleClazz = env->FindClass("java/lang/Double");
        jDoubleValueMethodId =
            env->GetMethodID(jDoubleClazz, "doubleValue", "()D");
        env->DeleteLocalRef(jDoubleClazz);
      }
      jdouble jDoubleObj = env->CallDoubleMethod(jDataObj, jDoubleValueMethodId);
      ret = v8::Number::New(isolate, (double)jDoubleObj);
    } else if (jTypeInt == 2) {
      jstring jDataStr = (jstring)jDataObj;
      ret = jString2V8String(env, jDataStr);
    } else if (jTypeInt == 3) {
      v8::Local<v8::Value> jsonObj[1];
      v8::Local<v8::Object> global = context->Global();
      v8::Local<v8::Object> json;
      v8::Local<v8::Function> json_parse;
      json = v8::Local<v8::Object>::Cast(
          global->Get(v8::String::NewFromUtf8(isolate, "JSON")));
      json_parse = v8::Local<v8::Function>::Cast(
          json->Get(v8::String::NewFromUtf8(isolate, "parse")));
      jsonObj[0] = jString2V8String(env, (jstring)jDataObj);
      ret = json_parse->Call(json, 1, jsonObj);
    }

    env->DeleteLocalRef(jDataObj);
  } while (0);

  if (jInstanceId != NULL) {
    env->DeleteLocalRef(jInstanceId);
  }
  if (jmodule != NULL) {
    env->DeleteLocalRef(jmodule);
  }
  if (jmethod != NULL) {
    env->DeleteLocalRef(jmethod);
  }
  if (jArgString != NULL) {
    env->DeleteLocalRef(jArgString);
  }
  if (jOptString != NULL) {
    env->DeleteLocalRef(jOptString);
  }

  return args.GetReturnValue().Set(ret);
}

/**
 *  This Function is a built-in function that JS bundle can execute
 *  to call native module.
 *  String instanceId, String module, String method, Object[] arguments, Object
 * options
 */
void callNativeComponent(const v8::FunctionCallbackInfo<v8::Value>& args) {
  base::debug::TraceScope traceScope("weex", "callNativeComponent");

  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, V8context);
  JNIEnv* env = getJNIEnv();

  // instacneID args[0]
  jstring jInstanceId = NULL;
  if (!args[0].IsEmpty()) {
    v8::String::Utf8Value instanceId(args[0]);
    jInstanceId = env->NewStringUTF(*instanceId);
  }

  // module args[1]
  jstring jcomponentRef = NULL;
  if (!args[1].IsEmpty()) {
    v8::String::Utf8Value componentRef(args[1]);
    jcomponentRef = env->NewStringUTF(*componentRef);
  }

  // method args[2]
  jstring jmethod = NULL;
  if (!args[2].IsEmpty()) {
    v8::String::Utf8Value method(args[2]);
    jmethod = env->NewStringUTF(*method);
  }

  // arguments args[3]
  jbyteArray jArgString = NULL;
  if (!args[3].IsEmpty() && args[3]->IsObject()) {
    v8::Local<v8::Value> obj[1];
    v8::Local<v8::Object> global = context->Global();
    v8::Local<v8::Object> json;
    v8::Local<v8::Function> json_stringify;
    json = v8::Local<v8::Object>::Cast(
        global->Get(v8::String::NewFromUtf8(isolate, "JSON")));
    json_stringify = v8::Local<v8::Function>::Cast(
        json->Get(v8::String::NewFromUtf8(isolate, "stringify")));
    obj[0] = args[3];
    v8::Local<v8::Value> ret = json_stringify->Call(json, 1, obj);
    v8::String::Utf8Value str(ret);

    int strLen = strlen(ToCString(str));
    jArgString = env->NewByteArray(strLen);
    env->SetByteArrayRegion(jArgString, 0, strLen,
                            reinterpret_cast<const jbyte*>(ToCString(str)));
  }

  // arguments args[4]
  jbyteArray jOptString = NULL;
  if (!args[4].IsEmpty() && args[4]->IsObject()) {
    v8::Local<v8::Value> obj[1];
    v8::Local<v8::Object> global = context->Global();
    v8::Local<v8::Object> json;
    v8::Local<v8::Function> json_stringify;
    json = v8::Local<v8::Object>::Cast(
        global->Get(v8::String::NewFromUtf8(isolate, "JSON")));
    json_stringify = v8::Local<v8::Function>::Cast(
        json->Get(v8::String::NewFromUtf8(isolate, "stringify")));
    obj[0] = args[4];
    v8::Local<v8::Value> ret = json_stringify->Call(json, 1, obj);
    v8::String::Utf8Value str(ret);

    int strLen = strlen(ToCString(str));
    jOptString = env->NewByteArray(strLen);
    env->SetByteArrayRegion(jOptString, 0, strLen,
                            reinterpret_cast<const jbyte*>(ToCString(str)));
  }

  if (jCallNativeComponentMethodId == NULL) {
    jCallNativeComponentMethodId =
        env->GetMethodID(jBridgeClazz, "callNativeComponent",
                         "(Ljava/lang/String;"
                         "Ljava/lang/String;"
                         "Ljava/lang/String;"
                         "[B"
                         "[B)"
                         "V");
  }

  if (jThis) {
    env->CallVoidMethod(jThis, jCallNativeComponentMethodId, jInstanceId,
                        jcomponentRef, jmethod, jArgString, jOptString);
  }
  env->DeleteLocalRef(jInstanceId);
  env->DeleteLocalRef(jcomponentRef);
  env->DeleteLocalRef(jmethod);
  env->DeleteLocalRef(jArgString);
  env->DeleteLocalRef(jOptString);
  args.GetReturnValue().Set(v8::Boolean::New(isolate, true));
}

/**
 *  This Function is a built-in function that JS bundle can execute
 *  to call native module.
 */
void callAddElement(const v8::FunctionCallbackInfo<v8::Value>& args) {
  base::debug::TraceScope traceScope("weex", "callAddElement");
  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, V8context);
  JNIEnv* env = getJNIEnv();

  // instacneID args[0]
  jstring jInstanceId = NULL;
  if (!args[0].IsEmpty()) {
    v8::String::Utf8Value instanceId(args[0]);
    jInstanceId = env->NewStringUTF(*instanceId);
  }
  // instacneID args[1]
  jstring jref = NULL;
  if (!args[1].IsEmpty()) {
    v8::String::Utf8Value ref(args[1]);
    jref = env->NewStringUTF(*ref);
  }
  // dom node args[2]
  jbyteArray jdomString = NULL;
  if (!args[2].IsEmpty() && args[2]->IsObject()) {
    v8::Local<v8::Value> obj[1];
    v8::Local<v8::Object> global = context->Global();
    v8::Local<v8::Object> json;
    v8::Local<v8::Function> json_stringify;
    json = v8::Local<v8::Object>::Cast(
        global->Get(v8::String::NewFromUtf8(isolate, "JSON")));
    json_stringify = v8::Local<v8::Function>::Cast(
        json->Get(v8::String::NewFromUtf8(isolate, "stringify")));
    obj[0] = args[2];
    v8::Local<v8::Value> ret = json_stringify->Call(json, 1, obj);
    v8::String::Utf8Value str(ret);

    int strLen = strlen(ToCString(str));
    jdomString = env->NewByteArray(strLen);
    env->SetByteArrayRegion(jdomString, 0, strLen,
                            reinterpret_cast<const jbyte*>(ToCString(str)));
  } else if (args[2].IsEmpty() && args[2]->IsString()) {
    v8::String::Utf8Value tasks(args[2]);

    int strLen = strlen(*tasks);
    jdomString = env->NewByteArray(strLen);
    env->SetByteArrayRegion(jdomString, 0, strLen,
                            reinterpret_cast<const jbyte*>(*tasks));
  }
  // index  args[3]
  jstring jindex = NULL;
  if (!args[3].IsEmpty()) {
    v8::String::Utf8Value index(args[3]);
    jindex = env->NewStringUTF(*index);
  }
  // callback  args[4]
  jstring jCallback = NULL;
  if (!args[4].IsEmpty()) {
    v8::String::Utf8Value callback(args[4]);
    jCallback = env->NewStringUTF(*callback);
  }
  if (jCallAddElementMethodId == NULL) {
    jCallAddElementMethodId = env->GetMethodID(jBridgeClazz, "callAddElement",
                                               "(Ljava/lang/String;"
                                               "Ljava/lang/String;"
                                               "[B"
                                               "Ljava/lang/String;"
                                               "Ljava/lang/String;)"
                                               "I");
  }

  int flag = env->CallIntMethod(jThis, jCallAddElementMethodId, jInstanceId,
                                jref, jdomString, jindex, jCallback);
  if (flag == -1) {
    LOGE("instance destroy JFM must stop callNative");
  }
  env->DeleteLocalRef(jInstanceId);
  env->DeleteLocalRef(jref);
  env->DeleteLocalRef(jdomString);
  env->DeleteLocalRef(jindex);
  env->DeleteLocalRef(jCallback);
  args.GetReturnValue().Set(v8::Integer::New(isolate, flag));
}

/**
 * set time out function
 */
void setTimeoutNative(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  base::debug::TraceScope traceScope("weex", "setTimeoutNative");
  JNIEnv* env = getJNIEnv();
  // callbackId
  v8::String::Utf8Value callbackID(args[0]);
  jstring jCallbackID = env->NewStringUTF(*callbackID);

  // time
  v8::String::Utf8Value time(args[1]);
  jstring jTime = env->NewStringUTF(*time);

  if (jSetTimeoutNativeMethodId == NULL) {
    jSetTimeoutNativeMethodId =
        env->GetMethodID(jBridgeClazz, "setTimeoutNative",
                         "(Ljava/lang/String;Ljava/lang/String;)V");
  }
  if (jThis) {
    env->CallVoidMethod(jThis, jSetTimeoutNativeMethodId, jCallbackID, jTime);
  }
  env->DeleteLocalRef(jCallbackID);
  env->DeleteLocalRef(jTime);
  args.GetReturnValue().Set(v8::Boolean::New(isolate, true));
}

/**
 * JS log output.
 */
void nativeLog(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  JNIEnv* env;
  bool result = false;

  v8::Local<v8::String> accumulator = v8::String::Empty(isolate);
  for (int i = 0; i < args.Length(); i++) {
    v8::Local<v8::String> str_arg = args[i]->ToString();
    if (!str_arg.IsEmpty()) {
      accumulator = v8::String::Concat(accumulator, str_arg);
    }
  }
  if (!accumulator.IsEmpty()) {
    env = getJNIEnv();
    v8::String::Value arg(accumulator);
    jstring str_msg = v8String2JString(env, arg);
    jclass clazz = env->FindClass("com/taobao/weex/utils/WXLogUtils");
    if (clazz != NULL) {
      if (jLogMethodId == NULL) {
        jLogMethodId = env->GetStaticMethodID(
            clazz, "d", "(Ljava/lang/String;Ljava/lang/String;)V");
      }
      if (jLogMethodId != NULL) {
        jstring str_tag = env->NewStringUTF("jsLog");
        // str_msg = env->NewStringUTF(s);
        env->CallStaticVoidMethod(clazz, jLogMethodId, str_tag, str_msg);
        result = true;
        env->DeleteLocalRef(str_msg);
        env->DeleteLocalRef(str_tag);
      }
      env->DeleteLocalRef(clazz);
    }
  }

  args.GetReturnValue().Set(v8::Boolean::New(isolate, result));
}

/**
 * V8 would have a chance to guide GC heuristic,
 * then V8 could clean up the memory.
 */

static void notifyTrimMemoryInternally(v8::Isolate* isolate) {
  if (!isolate) return;

  base::debug::TraceScope traceScope("weex", "notifyTrimMemoryInternally");
  bool finished = false;
  const int maxCount = 5;
  for (int i = 0; i < maxCount && !finished; i++) {
    isolate->ContextDisposedNotification();
    finished = isolate->IdleNotification(LONG_TERM_IDLE_TIME_IN_MS);
  }
}

static void notifyTrimMemory(const v8::FunctionCallbackInfo<v8::Value>& args) {
  notifyTrimMemoryInternally(args.GetIsolate());
}

static void notifySerializeCodeCache(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  weex::V8ScriptRunner::serializeCache();
}

/**
 * Markup states.
 */
void markupStateInternally() {
  if (execJSCount + samplingBegin == MAX_EXECJS_COUNT) {
    execJSCount = MIN_EXECJS_COUNT;
  }
  lastExecJSCount = execJSCount;
  idle_notification_FLAG = true;
  samplingExecJSCount = 0;
}

static void markupState(const v8::FunctionCallbackInfo<v8::Value>& args) {
  markupStateInternally();
}

/**
 * Creates a new execution environment containing the built-in functions.
 *
 */
v8::Local<v8::Context> CreateShellContext(
    v8::Local<v8::ObjectTemplate> WXEnvironment) {
  v8::Isolate* isolate = globalIsolate;
  // Create a template for the global object.
  v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New();

  // Bind the global 'callNative' function to the C++  callNative.
  global->Set(v8::String::NewFromUtf8(isolate, "callNative"),
              v8::FunctionTemplate::New(isolate, callNative));

  // Bind the global 'callNativeModule' function to the C++  callNativeModule.
  global->Set(v8::String::NewFromUtf8(isolate, "callNativeModule"),
              v8::FunctionTemplate::New(isolate, callNativeModule));

  // Bind the global 'callNativeComponent' function to the C++
  // callNativeComponent.
  global->Set(v8::String::NewFromUtf8(isolate, "callNativeComponent"),
              v8::FunctionTemplate::New(isolate, callNativeComponent));

  // Bind the global 'callAddElement' function to the C++  callNative.
  global->Set(v8::String::NewFromUtf8(isolate, "callAddElement"),
              v8::FunctionTemplate::New(isolate, callAddElement));

  // Bind the global 'setTimeoutNative' function to the C++ setTimeoutNative.
  global->Set(v8::String::NewFromUtf8(isolate, "setTimeoutNative"),
              v8::FunctionTemplate::New(isolate, setTimeoutNative));

  // Bind the global 'nativeLog' function to the C++ Print callback.
  global->Set(v8::String::NewFromUtf8(isolate, "nativeLog"),
              v8::FunctionTemplate::New(isolate, nativeLog));

  // Bind the global 'notifyTrimMemory' function
  // to the C++ function 'notifyTrimMemory'
  global->Set(v8::String::NewFromUtf8(isolate, "notifyTrimMemory"),
              v8::FunctionTemplate::New(isolate, notifyTrimMemory));

  global->Set(v8::String::NewFromUtf8(isolate, "notifySerializeCodeCache"),
              v8::FunctionTemplate::New(isolate, notifySerializeCodeCache));

  global->Set(v8::String::NewFromUtf8(isolate, "markupState"),
              v8::FunctionTemplate::New(isolate, markupState));

  // Bind the global 'WXEnvironment' Object.
  global->Set(v8::String::NewFromUtf8(isolate, "WXEnvironment"), WXEnvironment);

  return v8::Context::New(isolate, NULL, global);
}

static void takeHeapSnapshot(const char* filename) {
  if (globalIsolate == NULL) {
    return;
  }

  LOGA("begin takeHeapSnapshot: %s", filename);
  FILE* fp = fopen(filename, "w");
  if (NULL == fp) {
    return;
  }

  v8::Isolate* isolate = globalIsolate;
  v8::HandleScope handleScope(isolate);
  v8::HeapProfiler* profiler = isolate->GetHeapProfiler();
  const v8::HeapSnapshot* heapSnap = profiler->TakeHeapSnapshot();
  AsciiOutputStream stream(fp);
  heapSnap->Serialize(&stream, v8::HeapSnapshot::kJSON);

  fclose(fp);
  const_cast<v8::HeapSnapshot*>(heapSnap)->Delete();
  LOGA("end takeHeapSnapshot");
}

/**
 * Make a decision whether to tell v8 idle notifications
 *
 * It's not a good idea to notify a v8 idle notification
 * every time of calling execJS(), because too many times
 * of notifications have a little performance impact.
 *
 * If there are too many times of calling execJS, don't
 * tell v8 idle notifications. We add simple strategies
 * to check if there are too many times of calling execJS
 * within the period of rendering the current bundle:
 *   a) don't trigger notifications if execJSCount is less
 *      than 100, because jsfm initialization wound take
 *      almost 100 times of calling execJS().
 *   b) We begin to record the times of calling execJS,
 *      if the times are greater than upper limit of 10,
 *      we decide not to tell v8 idle notifications.
 */
bool needIdleNotification() {
  if (!idle_notification_FLAG) {
    return false;
  }

  if (execJSCount == MAX_EXECJS_COUNT) execJSCount = MIN_EXECJS_COUNT;

  if (execJSCount++ < MIN_EXECJS_COUNT) {
    return false;
  }

  if (execJSCount == MIN_EXECJS_COUNT) {
    lastExecJSCount = execJSCount;
  }

  if (execJSCount == lastExecJSCount + samplingBegin) {
    samplingExecJSCount = 0;
  } else if (execJSCount > lastExecJSCount + samplingBegin) {
    if (samplingExecJSCount == MAX_EXECJS_COUNT) {
      samplingExecJSCount = 0;
    }
    if (++samplingExecJSCount >= samplingExecJSCountMax) {
      idle_notification_FLAG = false;
    }

    // Also just notify every three times of calling execJS in sampling.
    if (samplingExecJSCount % denomValue == 0) {
      return true;
    } else {
      return false;
    }
  }

  // Just notify idle notifications every three times
  // of calling execJS.
  if (execJSCount % denomValue == 0) {
    return true;
  } else {
    return false;
  }
}

void makeIdleNotification(v8::Isolate* isolate) {
  if (!needIdleNotification()) {
    return;
  }

  isolate->IdleNotification(LONG_TERM_IDLE_TIME_IN_MS);
}

// It's not necessary to make idle notifications every time
// of invoking init_framework.
void resetIdleNotificationCount() {
  execJSCount = 0;
  idle_notification_FLAG = true;
}

////////////////////////////////////////////
static const char* gBridgeClassPathName = "com/taobao/weex/bridge/WXBridge";
static JNINativeMethod gMethods[] = {
    {"initFramework", "(Ljava/lang/String;Lcom/taobao/weex/bridge/WXParams;)I",
     (void*)native_initFramework},
    {"execJS",
     "(Ljava/lang/String;Ljava/lang/String;"
     "Ljava/lang/String;[Lcom/taobao/weex/bridge/WXJSObject;)I",
     (void*)native_execJS},
    {"takeHeapSnapshot", "(Ljava/lang/String;)V",
     (void*)native_takeHeapSnapshot},
    {"execJSService", "(Ljava/lang/String;)I", (void*)native_execJSService}};

static int registerNativeMethods(JNIEnv* env, const char* className,
                                 JNINativeMethod* methods, int numMethods) {
  jclass clazz = (env)->FindClass("com/taobao/weex/bridge/WXBridge");
  if (clazz == NULL) {
    LOGE("registerNativeMethods failed to find class '%s'", className);
    return JNI_FALSE;
  }
  if ((env)->RegisterNatives(clazz, methods, numMethods) < 0) {
    LOGE(
        "registerNativeMethods failed to register native methods for class "
        "'%s'",
        className);
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

static int registerNatives(JNIEnv* env) {
  return registerNativeMethods(env, gBridgeClassPathName, gMethods,
                               sizeof(gMethods) / sizeof(gMethods[0]));
}

/**
 * This function will be call when the library first be load.
 * You can do some init in the lib. return which version jni it support.
 */

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  LOGD("begin JNI_OnLoad");
  JNIEnv* env;
  /* Get environment */
  if ((vm)->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK) {
    return JNI_FALSE;
  }

  sVm = vm;
  jclass tempClass = env->FindClass("com/taobao/weex/bridge/WXBridge");
  jBridgeClazz = (jclass)env->NewGlobalRef(tempClass);
  env->DeleteLocalRef(tempClass);

  if (registerNatives(env) != JNI_TRUE) {
    return JNI_FALSE;
  }

  LOGD("end JNI_OnLoad");
  return JNI_VERSION_1_4;
}

void JNI_OnUnload(JavaVM* vm, void* reserved) {
  LOGD("beigin JNI_OnUnload");
  V8context.Reset();
  if (globalIsolate) {
    globalIsolate->Dispose();
  }
  v8::V8::ShutdownPlatform();
  v8::V8::Dispose();
  JNIEnv* env;
  /* Get environment */
  if ((vm)->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK) {
    return;
  }
  env->DeleteGlobalRef(jBridgeClazz);
  env->DeleteGlobalRef(jThis);

  using base::debug::TraceEvent;
  TraceEvent::StopATrace(env);
  LOGD(" end JNI_OnUnload");
}

const char* getCacheDir(JNIEnv* env)
{
    jclass activityThreadCls, applicationCls, fileCls;
    jobject applicationObj, fileObj, pathStringObj;
    jmethodID currentApplicationMethodId, getCacheDirMethodId, getAbsolutePathMethodId;
    static std::string storage;
    const char* tmp;
    const char* ret = nullptr;
    activityThreadCls = env->FindClass("android/app/ActivityThread");
    if (!activityThreadCls || env->ExceptionOccurred()) {
        goto no_class;
    }
    currentApplicationMethodId = env->GetStaticMethodID(activityThreadCls, "currentApplication", "()Landroid/app/Application;");
    if (!currentApplicationMethodId || env->ExceptionOccurred()) {
        goto no_currentapplication_method;
    }
    applicationObj = env->CallStaticObjectMethod(activityThreadCls, currentApplicationMethodId, nullptr);
    if (!applicationObj || env->ExceptionOccurred()) {
        goto no_application;
    }
    applicationCls = env->GetObjectClass(applicationObj);
    getCacheDirMethodId = env->GetMethodID(applicationCls, "getCacheDir", "()Ljava/io/File;");
    if (!getCacheDirMethodId || env->ExceptionOccurred()) {
        goto no_getcachedir_method;
    }
    fileObj = env->CallObjectMethod(applicationObj, getCacheDirMethodId, nullptr);
    if (!fileObj || env->ExceptionOccurred()) {
        goto no_file_obj;
    }
    fileCls = env->GetObjectClass(fileObj);
    getAbsolutePathMethodId = env->GetMethodID(fileCls, "getAbsolutePath", "()Ljava/lang/String;");
    if (!getAbsolutePathMethodId || env->ExceptionOccurred()) {
        goto no_getabsolutepath_method;
    }
    pathStringObj = env->CallObjectMethod(fileObj, getAbsolutePathMethodId, nullptr);
    if (!pathStringObj || env->ExceptionOccurred()) {
        goto no_path_string;
    }
    tmp = env->GetStringUTFChars(reinterpret_cast<jstring>(pathStringObj), nullptr);
    storage.assign(tmp);
    env->ReleaseStringUTFChars(reinterpret_cast<jstring>(pathStringObj), tmp);
    ret = storage.c_str();
no_path_string:
no_getabsolutepath_method:
    env->DeleteLocalRef(fileCls);
    env->DeleteLocalRef(fileObj);
no_file_obj:
no_getcachedir_method:
    env->DeleteLocalRef(applicationCls);
    env->DeleteLocalRef(applicationObj);
no_application:
no_currentapplication_method:
    env->DeleteLocalRef(activityThreadCls);
no_class:
    env->ExceptionDescribe();
    env->ExceptionClear();
    return ret;
}
