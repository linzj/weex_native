#include "jni.h"
#include "android/log.h"
#include "LogUtils.h"

#include <libplatform/libplatform.h>
#include <v8.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

static jclass jBridgeClazz;
static jobject jThis;
static JavaVM* sVm = NULL;

static void ReportException(v8::Isolate* isolate,
                            v8::TryCatch* try_catch,
                            jstring jinstanceid,
                            const char* func);

static bool ExecuteJavaScript(v8::Isolate* isolate,
                              v8::Local<v8::String> source,
                              bool report_exceptions);

static v8::Local<v8::Context> CreateShellContext();

static void callNative(const v8::FunctionCallbackInfo<v8::Value>& args);

static void setTimeoutNative(const v8::FunctionCallbackInfo<v8::Value>& args);

static void nativeLog(const v8::FunctionCallbackInfo<v8::Value>& name);

static v8::Platform* g_platform;
static v8::Persistent<v8::Context> V8context;
static v8::Isolate* globalIsolate;
static v8::ArrayBuffer::Allocator* g_array_buffer_allocator;
static v8::Local<v8::Object> json;
static v8::Local<v8::Function> json_parse;
static v8::Local<v8::Function> json_stringify;

static v8::Local<v8::ObjectTemplate> WXEnvironment;

static JNIEnv* getJNIEnv() {
  JNIEnv* env = NULL;
  if ((sVm)->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK) {
    return JNI_FALSE;
  }
  return env;
}

static const char* ToCString(const v8::String::Utf8Value& value) {
  return *value ? *value : "<string conversion failed>";
}

static v8::Local<v8::Value> jString2V8String(JNIEnv* env, jstring str) {
  v8::Isolate* isolate = globalIsolate;
  if (str != NULL) {
    const char* c_str = env->GetStringUTFChars(str, NULL);
    if (c_str) {
      v8::Local<v8::Value> ret = v8::String::NewFromUtf8(isolate, c_str);
      env->ReleaseStringUTFChars(str, c_str);
      return ret;
    }
  }
  return v8::String::NewFromUtf8(isolate, "");
}

static void jString2Log(JNIEnv* env, jstring instance, jstring str) {
  if (str != NULL) {
    const char* c_instance = env->GetStringUTFChars(instance, NULL);
    const char* c_str = env->GetStringUTFChars(str, NULL);
    if (c_str) {
      LOGA("jsLog>>> instance :%s,c_str:%s", c_instance, c_str);
    }
  }
}

static bool RunFromCache(v8::Isolate* isolate, const char* scriptStr) {
  v8::Isolate::Scope isolate_scope(isolate);
  v8::Context::Scope ctx_scope(v8::Local<v8::Context>::New(isolate, V8context));
  v8::HandleScope handleScope(isolate);
  FILE* f;
  f = fopen("/sdcard/cache.data", "rb");
  if (!f) {
    LOGE("unable to open cache file: %s", strerror(errno));
    return false;
  }

  fseek(f, 0, SEEK_END);
  size_t size = ftell(f);
  rewind(f);
  uint8_t* buffer = new uint8_t[size];
  if (size != fread(buffer, 1, size, f)) {
    delete[] buffer;
    fclose(f);
    LOGE("unable to read enough data");
    return false;
  }
  fclose(f);
  v8::ScriptCompiler::CachedData* cacheData =
      new v8::ScriptCompiler::CachedData(
          buffer, size, v8::ScriptCompiler::CachedData::BufferOwned);
  v8::ScriptCompiler::Source source(v8::String::NewFromUtf8(isolate, scriptStr),
                                    cacheData);
  v8::Local<v8::UnboundScript> unboundScript =
      v8::ScriptCompiler::CompileUnbound(isolate, &source,
                                         v8::ScriptCompiler::kConsumeCodeCache);
  v8::TryCatch tryCatch(isolate);
  v8::MaybeLocal<v8::Value> result =
      unboundScript->BindToCurrentContext()->Run(isolate->GetCurrentContext());
  if (result.IsEmpty()) {
    assert(tryCatch.HasCaught());
    // Print errors that happened during execution.
    ReportException(isolate, &tryCatch, NULL, "");
  } else {
    assert(!tryCatch.HasCaught());
  }

  LOGE("run with cache!");
  return true;
}

extern "C" {

jint Java_com_taobao_weex_bridge_WXBridge_initFramework(JNIEnv* env,
                                                        jobject object,
                                                        jstring script,
                                                        jobject params) {
  jThis = (env)->NewGlobalRef(object);
  v8::Platform* v8platform = v8::platform::CreateDefaultPlatform();
  v8::V8::InitializePlatform(v8platform);
  v8::V8::Initialize();
  g_platform = v8platform;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  g_array_buffer_allocator = create_params.array_buffer_allocator;
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  globalIsolate = isolate;
  v8::HandleScope handleScope(isolate);

  WXEnvironment = v8::ObjectTemplate::New(isolate);

  jclass c_params = env->GetObjectClass(params);

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

  jmethodID m_appName =
      env->GetMethodID(c_params, "getAppName", "()Ljava/lang/String;");
  jobject appName = env->CallObjectMethod(params, m_appName);
  WXEnvironment->Set(v8::String::NewFromUtf8(isolate, "appName"),
                     jString2V8String(env, (jstring)appName));

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

  V8context.Reset(isolate, CreateShellContext());

  const char* scriptStr = (env)->GetStringUTFChars(script, NULL);
  if (scriptStr == NULL) {
    return false;
  }
  bool retValue = true;
  if (!RunFromCache(globalIsolate, scriptStr)) {
    LOGE("failed run with cache!");
    if (!ExecuteJavaScript(globalIsolate,
                           v8::String::NewFromUtf8(globalIsolate, scriptStr),
                           true)) {
      retValue = false;
    }
  }
  env->ReleaseStringUTFChars(script, scriptStr);

  env->DeleteLocalRef(c_params);
  return retValue;
}

/**
 * Called to execute JavaScript such as . createInstance(),destroyInstance ext.
 *
 */
jint Java_com_taobao_weex_bridge_WXBridge_execJS(JNIEnv* env,
                                                 jobject this1,
                                                 jstring jinstanceid,
                                                 jstring jnamespace,
                                                 jstring jfunction,
                                                 jobjectArray jargs) {
  v8::Isolate* isolate = globalIsolate;
  v8::HandleScope handleScope(isolate);
  v8::Local<v8::Context> context(V8context.Get(isolate));
  v8::Isolate::Scope isolate_scope(globalIsolate);
  v8::Context::Scope ctx_scope(context);
  v8::TryCatch try_catch;
  int length = env->GetArrayLength(jargs);
  v8::Local<v8::Value> obj[length];

  jclass jsObjectClazz = (env)->FindClass("com/taobao/weex/bridge/WXJSObject");
  for (int i = 0; i < length; i++) {
    jobject jArg = (env)->GetObjectArrayElement(jargs, i);

    jfieldID jTypeId = (env)->GetFieldID(jsObjectClazz, "type", "I");
    jint jTypeInt = env->GetIntField(jArg, jTypeId);

    jfieldID jDataId =
        (env)->GetFieldID(jsObjectClazz, "data", "Ljava/lang/Object;");
    jobject jDataObj = env->GetObjectField(jArg, jDataId);
    if (jTypeInt == 1) {
      jclass jDoubleClazz = (env)->FindClass("java/lang/Double");
      jmethodID jDoubleValueId =
          (env)->GetMethodID(jDoubleClazz, "doubleValue", "()D");
      jdouble jDoubleObj = (env)->CallDoubleMethod(jDataObj, jDoubleValueId);
      obj[i] = v8::Number::New(isolate, (double)jDoubleObj);
      env->DeleteLocalRef(jDoubleClazz);
    } else if (jTypeInt == 2) {
      jstring jDataStr = (jstring)jDataObj;
      obj[i] = jString2V8String(env, jDataStr);
    } else if (jTypeInt == 3) {
      v8::Local<v8::Value> jsonObj[1];
      v8::Local<v8::Object> global = context->Global();
      json = v8::Local<v8::Object>::Cast(
          global->Get(v8::String::NewFromUtf8(isolate, "JSON")));
      json_parse = v8::Local<v8::Function>::Cast(
          json->Get(v8::String::NewFromUtf8(isolate, "parse")));
      jsonObj[0] = jString2V8String(env, (jstring)jDataObj);
      v8::Local<v8::Value> ret = json_parse->Call(json, 1, jsonObj);
      obj[i] = ret;
    }
    env->DeleteLocalRef(jDataObj);
    env->DeleteLocalRef(jArg);
  }
  env->DeleteLocalRef(jsObjectClazz);

  const char* func = (env)->GetStringUTFChars(jfunction, 0);
  v8::Local<v8::Object> global = context->Global();
  v8::Local<v8::Function> function;
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
  if (result.IsEmpty()) {
    assert(try_catch.HasCaught());
    ReportException(globalIsolate, &try_catch, jinstanceid, func);
    env->ReleaseStringUTFChars(jfunction, func);
    env->DeleteLocalRef(jfunction);
    return false;
  }
  env->ReleaseStringUTFChars(jfunction, func);
  env->DeleteLocalRef(jfunction);
  return true;
}
}

/**
 * this function is to execute a section of JavaScript content.
 */
static bool ExecuteJavaScript(v8::Isolate* isolate,
                              v8::Local<v8::String> source,
                              bool report_exceptions) {
  v8::Isolate::Scope isolate_scope(isolate);
  v8::Local<v8::Context> context(V8context.Get(isolate));
  v8::Context::Scope ctx_scope(context);
  v8::TryCatch try_catch;
  if (source.IsEmpty()) {
    if (report_exceptions)
      ReportException(isolate, &try_catch, NULL, "");
    return false;
  }
  v8::Local<v8::String> name = v8::String::NewFromUtf8(isolate, "(weex)");
  v8::Local<v8::Script> script = v8::Script::Compile(source, name);
  if (script.IsEmpty()) {
    if (report_exceptions)
      ReportException(isolate, &try_catch, NULL, "");
    return false;
  } else {
    v8::Local<v8::Value> result = script->Run();
    if (result.IsEmpty()) {
      assert(try_catch.HasCaught());
      if (report_exceptions)
        ReportException(isolate, &try_catch, NULL, "");
      return false;
    } else {
      assert(!try_catch.HasCaught());
      return true;
    }
  }
}

static void reportException(jstring jInstanceId,
                            const char* func,
                            const char* exception_string) {
  JNIEnv* env = getJNIEnv();
  jstring jExceptionString = (env)->NewStringUTF(exception_string);
  jstring jFunc = (env)->NewStringUTF(func);
  jmethodID tempMethodId = (env)->GetMethodID(
      jBridgeClazz, "reportJSException",
      "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
  (env)->CallVoidMethod(jThis, tempMethodId, jInstanceId, jFunc,
                        jExceptionString);
  env->DeleteLocalRef(jExceptionString);
  env->DeleteLocalRef(jFunc);
}

/**
 *  This Function will be called when any javascript Exception
 *  that need to print log to notify  native happened.
 */
static void ReportException(v8::Isolate* isolate,
                            v8::TryCatch* try_catch,
                            jstring jinstanceid,
                            const char* func) {
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
  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::Context> context(V8context.Get(isolate));
  JNIEnv* env = getJNIEnv();
  jstring jTaskString = NULL;
  if (args[1]->IsObject()) {
    v8::Local<v8::Value> obj[1];
    v8::Local<v8::Object> global = context->Global();
    json = v8::Local<v8::Object>::Cast(
        global->Get(v8::String::NewFromUtf8(isolate, "JSON")));
    json_stringify = v8::Local<v8::Function>::Cast(
        json->Get(v8::String::NewFromUtf8(isolate, "stringify")));
    obj[0] = args[1];
    v8::Local<v8::Value> ret = json_stringify->Call(json, 1, obj);
    v8::String::Utf8Value str(ret);
    jTaskString = (env)->NewStringUTF(ToCString(str));
  } else if (args[1]->IsString()) {
    v8::String::Utf8Value tasks(args[1]);
    jTaskString = (env)->NewStringUTF(*tasks);
  }

  v8::String::Utf8Value instanceId(args[0]);
  jstring jCallback = NULL;
  if (!args[2].IsEmpty()) {
    v8::String::Utf8Value instanceId(args[2]);
    jCallback = (env)->NewStringUTF(*instanceId);
  }
  jstring jInstanceId = (env)->NewStringUTF(*instanceId);
  jmethodID tempMethodId = (env)->GetMethodID(
      jBridgeClazz, "callNative",
      "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I");
  int flag = (env)->CallIntMethod(jThis, tempMethodId, jInstanceId, jTaskString,
                                  jCallback);
  if (flag == -1) {
    LOGE("instance destroy JFM must stop callNative");
  }

  env->DeleteLocalRef(jTaskString);
  env->DeleteLocalRef(jInstanceId);
  env->DeleteLocalRef(jCallback);

  args.GetReturnValue().Set(v8::Integer::New(isolate, flag));
}

/**
 * set time out function
 */
static void setTimeoutNative(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  JNIEnv* env = getJNIEnv();
  // callbackId
  v8::String::Utf8Value callbackID(args[0]);
  jstring jCallbackID = (env)->NewStringUTF(*callbackID);

  // time
  v8::String::Utf8Value time(args[1]);
  jstring jTime = (env)->NewStringUTF(*time);

  jmethodID tempMethodId =
      (env)->GetMethodID(jBridgeClazz, "setTimeoutNative",
                         "(Ljava/lang/String;Ljava/lang/String;)V");
  (env)->CallVoidMethod(jThis, tempMethodId, jCallbackID, jTime);
  env->DeleteLocalRef(jCallbackID);
  env->DeleteLocalRef(jTime);
  args.GetReturnValue().Set(v8::Boolean::New(isolate, true));
}

/**
 * JS log output.
 */
static void nativeLog(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  char s[1000] = "";
  int available_len = sizeof(s) - 1;
  for (int i = 0; i < args.Length(); i++) {
    v8::String::Utf8Value str(args[i]);
    int append_len = strlen(*str) + 3;
    if (append_len < available_len) {
      strcat(s, *str);
      strcat(s, " | ");
      available_len -= append_len;
    } else {
      strncat(s, *str, available_len);
      break;
    }
  }
  LOGA("jsLog>>>>:%s", s);

  JNIEnv* env = getJNIEnv();
  jclass clazz = NULL;
  jstring str_msg = NULL;
  jstring str_tag = NULL;
  jmethodID mid_static_method;
  clazz = (env)->FindClass("com/taobao/weex/utils/WXLogUtils");
  if (clazz == NULL) {
    args.GetReturnValue().Set(v8::Boolean::New(isolate, false));
    return;
  }

  mid_static_method = (env)->GetStaticMethodID(
      clazz, "d", "(Ljava/lang/String;Ljava/lang/String;)V");
  if (mid_static_method == NULL) {
    args.GetReturnValue().Set(v8::Boolean::New(isolate, false));
    return;
  }
  str_tag = env->NewStringUTF("jsLog");
  str_msg = (env)->NewStringUTF(s);
  (env)->CallStaticVoidMethod(clazz, mid_static_method, str_tag, str_msg);

  (env)->DeleteLocalRef(clazz);
  env->DeleteLocalRef(str_tag);
  (env)->DeleteLocalRef(str_msg);

  return args.GetReturnValue().Set(v8::Boolean::New(isolate, true));
}

/**
 * Creates a new execution environment containing the built-in functions.
 *
 */
static v8::Local<v8::Context> CreateShellContext() {
  v8::Isolate* isolate = globalIsolate;
  // Create a template for the global object.
  v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);

  // Bind the global 'callNative' function to the C++  callNative.
  global->Set(v8::String::NewFromUtf8(isolate, "callNative"),
              v8::FunctionTemplate::New(isolate, callNative));

  // Bind the global 'setTimeoutNative' function to the C++ setTimeoutNative.
  global->Set(v8::String::NewFromUtf8(isolate, "setTimeoutNative"),
              v8::FunctionTemplate::New(isolate, setTimeoutNative));

  // Bind the global 'nativeLog' function to the C++ Print callback.
  global->Set(v8::String::NewFromUtf8(isolate, "nativeLog"),
              v8::FunctionTemplate::New(isolate, nativeLog));

  // Bind the global 'WXEnvironment' Object.
  global->Set(v8::String::NewFromUtf8(isolate, "WXEnvironment"), WXEnvironment);

  return v8::Context::New(isolate, NULL, global);
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
  jclass tempClass = (env)->FindClass("com/taobao/weex/bridge/WXBridge");
  jBridgeClazz = (jclass)(env)->NewGlobalRef(tempClass);
  env->DeleteLocalRef(tempClass);
  LOGD("end JNI_OnLoad");
  return JNI_VERSION_1_4;
}

void JNI_OnUnload(JavaVM* vm, void* reserved) {
  LOGD("beigin JNI_OnUnload");
  V8context.Reset();
  if (globalIsolate)
    globalIsolate->Dispose();
  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();
  if (g_platform)
    delete g_platform;
  if (g_array_buffer_allocator)
    delete g_array_buffer_allocator;
  JNIEnv* env;
  /* Get environment */
  if ((vm)->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK) {
    return;
  }
  env->DeleteGlobalRef(jBridgeClazz);
  env->DeleteGlobalRef(jThis);
  LOGD(" end JNI_OnUnload");
}
