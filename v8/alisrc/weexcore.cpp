#include "jni.h"
#include "android/log.h"
#include "LogUtils.h"

#include <v8.h>
#include <libplatform/libplatform.h>
#include <v8-platform.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstring>
#include <stddef.h>
#define ENABLE_PROFILER 0
#if ENABLE_PROFILER
#include <v8-profiler.h>
#include <string>
#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#endif  // ENABLE_PROFILER
#define ENABLE_HEAP_SNAPSHOT 0
#if ENABLE_HEAP_SNAPSHOT
#include <v8-profiler.h>
#include <signal.h>
#endif  // ENABLE_HEAP_SNAPSHOT
#define ENABLE_TIMEREVENT 0
#if ENABLE_TIMEREVENT
#include <string>
#include <unordered_map>
#include <vector>
#include <time.h>
#include <signal.h>

typedef std::tr1::unordered_map<std::string, unsigned> TimerMap;
struct StackItem {
  std::string name;
  struct timespec start_time;
};

static TimerMap s_timermap;
static std::vector<StackItem> s_stack;

static void increaseTime(const StackItem& item) {
  struct timespec endtime;
  clock_gettime(CLOCK_MONOTONIC, &endtime);
  unsigned elapseus =
      static_cast<double>(endtime.tv_sec - item.start_time.tv_sec) * 1e6 +
      static_cast<double>(endtime.tv_nsec - item.start_time.tv_nsec) / 1e3;
  s_timermap[item.name] += elapseus;
}

static void myLogEventCallback(const char* name, int event) {
  if (event == 0) {
    // start
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    s_stack.push_back({});
    auto& b = s_stack.back();
    LOGD("name: %s\n", name);
    b.name.assign(name);
    b.start_time = ts;
  } else {
    // end
    for (auto i = s_stack.rbegin(); i != s_stack.rend(); ++i) {
      if (i->name == name) {
        increaseTime(*i);
        s_stack.erase(i.base() - 1);
        break;
      }
    }
  }
}

static void printsignalHandler(int, siginfo_t*, void*) {
  for (auto& p : s_timermap) {
    __android_log_print(ANDROID_LOG_DEBUG, "linzj", "%s: %u.", p.first.c_str(),
                        p.second);
  }
}

static void clearsignalHandler(int, siginfo_t*, void*) {
  for (auto& p : s_timermap) {
    p.second = 0;
  }
}

static void initTimerSignal(void) {
  int myprintsignal, myclearsignal;
  myprintsignal = SIGRTMIN + 8;
  myclearsignal = myprintsignal + 1;
  struct sigaction sa = {0};
  sa.sa_sigaction = printsignalHandler;
  int r = sigaction(myprintsignal, &sa, NULL);
  sa.sa_sigaction = clearsignalHandler;
  r = sigaction(myclearsignal, &sa, NULL);
  __android_log_print(ANDROID_LOG_DEBUG, "linzj",
                      "handler installed: %d, SIGRTMAX: %d, myprintsignal: %d, "
                      "myclearsignal: %d, error: %s",
                      (int)r, SIGRTMAX, myprintsignal, myclearsignal,
                      strerror(errno));
}
#endif

jclass jBridgeClazz;
jobject jthis;
JavaVM* sVm = NULL;

void ReportException(v8::Isolate* isolate,
                     v8::TryCatch* try_catch,
                     jstring jinstanceid,
                     const char* func);
bool ExecuteString(v8::Isolate* isolate,
                   v8::Handle<v8::String> source,
                   bool print_result,
                   bool report_exceptions);
v8::Local<v8::Context> CreateShellContext();
void callNative(const v8::FunctionCallbackInfo<v8::Value>& args);
void setTimeoutNative(const v8::FunctionCallbackInfo<v8::Value>& args);
void nativeLog(const v8::FunctionCallbackInfo<v8::Value>& args);

v8::Persistent<v8::Context> V8context;
v8::Isolate* globalIsolate;
v8::Handle<v8::Object> json;
v8::Handle<v8::Function> json_parse;
v8::Handle<v8::Function> json_stringify;

#if ENABLE_HEAP_SNAPSHOT
static int mysignal;
static __attribute__((constructor)) void init(void);

class MyOutputStream : public v8::OutputStream {
 public:
  MyOutputStream();
  ~MyOutputStream();

  v8::OutputStream::WriteResult WriteAsciiChunk(char* data, int size);
  void EndOfStream();

 private:
  FILE* f_;
};

MyOutputStream::MyOutputStream() : f_(NULL) {
  f_ = fopen("/sdcard/heap.heapsnapshot", "wb");
}

MyOutputStream::~MyOutputStream() {
  if (f_) {
    fclose(f_);
  }
}

v8::OutputStream::WriteResult MyOutputStream::WriteAsciiChunk(char* data,
                                                              int size) {
  if (!f_) {
    return v8::OutputStream::kAbort;
  }
  if (1 != fwrite(data, size, 1, f_)) {
    return v8::OutputStream::kAbort;
  }
  return v8::OutputStream::kContinue;
}

void MyOutputStream::EndOfStream() {}

static void heapsignalHandler(int, siginfo_t*, void*) {
  v8::HandleScope handleScope(globalIsolate);
  v8::HeapProfiler* hp = globalIsolate->GetHeapProfiler();
  MyOutputStream os;
  const v8::HeapSnapshot* snapshot =
      hp->TakeHeapSnapshot(v8::String::NewFromUtf8(globalIsolate, "heap"));
  snapshot->Serialize(&os, v8::HeapSnapshot::kJSON);
}

void init(void) {
  mysignal = SIGRTMIN + 7;
  struct sigaction sa = {0};
  sa.sa_sigaction = heapsignalHandler;
  int r = sigaction(mysignal, &sa, NULL);
  __android_log_print(
      ANDROID_LOG_DEBUG, "linzj",
      "handler installed: %d, SIGRTMAX: %d, mysignal: %d, error: %s", (int)r,
      SIGRTMAX, mysignal, strerror(errno));
}
#endif  // ENABLE_HEAP_SNAPSHOT
v8::Handle<v8::ObjectTemplate> WXEnvironment;

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

v8::Local<v8::String> jstring2V8String(JNIEnv* env,
                                       jstring str,
                                       v8::Isolate* isolate) {
  if (str != NULL) {
    const char* c_str = env->GetStringUTFChars(str, NULL);
    if (c_str) {
      v8::Local<v8::String> ret = v8::String::NewFromUtf8(isolate, c_str);
      env->ReleaseStringUTFChars(str, c_str);
      return ret;
    }
  }
  return v8::String::Empty(isolate);
}

extern "C" {
jint Java_com_taobao_weex_bridge_WXBridge_initFramework(JNIEnv* env,
                                                        jobject this1,
                                                        jstring jscripts,
                                                        jobject params) {
  jthis = (env)->NewGlobalRef(this1);
  const char* scriptStr = (env)->GetStringUTFChars(jscripts, NULL);

  bool icu_return = v8::V8::InitializeICU();
  LOGD("InitializeICU return %d", icu_return);
  v8::Platform* platformV8 = v8::platform::CreateDefaultPlatform();
  v8::V8::InitializePlatform(platformV8);

  icu_return = v8::V8::Initialize();
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  LOGD("Initialize return %d", icu_return);
  globalIsolate = v8::Isolate::New(create_params);
#if ENABLE_TIMEREVENT
  globalIsolate->SetEventLogger(myLogEventCallback);
  initTimerSignal();
#endif  // ENABLE_TIMEREVENT

  LOGD("GetCurrent return %p", v8::Isolate::GetCurrent());
  v8::HandleScope handleScope(globalIsolate);

  WXEnvironment = v8::ObjectTemplate::New(globalIsolate);

  jclass c_params = env->GetObjectClass(params);

  jmethodID m_platform =
      env->GetMethodID(c_params, "getPlatform", "()Ljava/lang/String;");
  jobject platform = env->CallObjectMethod(params, m_platform);
  WXEnvironment->Set(v8::String::NewFromUtf8(globalIsolate, "platform"),
                     jstring2V8String(env, (jstring)platform, globalIsolate));

  jmethodID m_osVersion =
      env->GetMethodID(c_params, "getOsVersion", "()Ljava/lang/String;");
  jobject osVersion = env->CallObjectMethod(params, m_osVersion);
  WXEnvironment->Set(v8::String::NewFromUtf8(globalIsolate, "osVersion"),
                     jstring2V8String(env, (jstring)osVersion, globalIsolate));

  jmethodID m_appVersion =
      env->GetMethodID(c_params, "getAppVersion", "()Ljava/lang/String;");
  jobject appVersion = env->CallObjectMethod(params, m_appVersion);
  WXEnvironment->Set(v8::String::NewFromUtf8(globalIsolate, "appVersion"),
                     jstring2V8String(env, (jstring)appVersion, globalIsolate));

  jmethodID m_weexVersion =
      env->GetMethodID(c_params, "getWeexVersion", "()Ljava/lang/String;");
  jobject weexVersion = env->CallObjectMethod(params, m_weexVersion);
  WXEnvironment->Set(
      v8::String::NewFromUtf8(globalIsolate, "weexVersion"),
      jstring2V8String(env, (jstring)weexVersion, globalIsolate));

  jmethodID m_deviceModel =
      env->GetMethodID(c_params, "getDeviceModel", "()Ljava/lang/String;");
  jobject deviceModel = env->CallObjectMethod(params, m_deviceModel);
  WXEnvironment->Set(
      v8::String::NewFromUtf8(globalIsolate, "deviceModel"),
      jstring2V8String(env, (jstring)deviceModel, globalIsolate));

  jmethodID m_appName =
      env->GetMethodID(c_params, "getAppName", "()Ljava/lang/String;");
  jobject appName = env->CallObjectMethod(params, m_appName);
  WXEnvironment->Set(v8::String::NewFromUtf8(globalIsolate, "appName"),
                     jstring2V8String(env, (jstring)appName, globalIsolate));

  jmethodID m_deviceWidth =
      env->GetMethodID(c_params, "getDeviceWidth", "()Ljava/lang/String;");
  jobject deviceWidth = env->CallObjectMethod(params, m_deviceWidth);
  WXEnvironment->Set(
      v8::String::NewFromUtf8(globalIsolate, "deviceWidth"),
      jstring2V8String(env, (jstring)deviceWidth, globalIsolate));

  jmethodID m_deviceHeight =
      env->GetMethodID(c_params, "getDeviceHeight", "()Ljava/lang/String;");
  jobject deviceHeight = env->CallObjectMethod(params, m_deviceHeight);
  WXEnvironment->Set(
      v8::String::NewFromUtf8(globalIsolate, "deviceHeight"),
      jstring2V8String(env, (jstring)deviceHeight, globalIsolate));

  V8context.Reset(globalIsolate, CreateShellContext());
  ExecuteString(globalIsolate,
                v8::String::NewFromUtf8(globalIsolate, scriptStr), true, true);
  return true;
}

void jstring2Log(JNIEnv* env, jstring instance, jstring str) {
  if (str != NULL) {
    const char* c_instance = env->GetStringUTFChars(instance, NULL);
    const char* c_str = env->GetStringUTFChars(str, NULL);
    if (c_str) {
      LOGA("jsLog>>> instance :%s,c_str:%s", c_instance, c_str);
    }
  }
}

#if ENABLE_PROFILER
static int g_profiler_id;
static const char* stdstringprintf(std::string& s, const char* str, ...) {
  va_list ap;
  va_start(ap, str);
  vsnprintf(const_cast<char*>(s.data()), s.capacity(), str, ap);
  va_end(ap);
  return s.data();
}

static void saveFunction(std::string& content,
                         const v8::CpuProfileNode* node,
                         uint64_t& ts) {
  ts++;
  int childrenCount = node->GetChildrenCount();
  uint64_t currentTS = ts;
  for (int i = 0; i < childrenCount; ++i) {
    saveFunction(content, node->GetChild(i), ts);
  }
  ts += node->GetHitCount() * 1000 + 1;
  std::string buf(256, 0);
  stdstringprintf(buf,
                  "{\"cat\":\"%s\", \"ts\":%"
                  "llu"
                  ", \"pid\": 1, "
                  "\"ph\":\"X\", \"name\":\"%.128s\", \"dur\": %"
                  "llu"
                  "},",
                  "profile", currentTS,
                  ToCString(v8::String::Utf8Value(node->GetFunctionName())),
                  ts - currentTS);
  content.append(buf.data());
}

static void saveProfile(const v8::CpuProfile* profile, const char* title) {
  std::string content;
  content.append("{\"traceEvents\": [");
  uint64_t ts = 0;
  saveFunction(content, profile->GetTopDownRoot(), ts);
  content.erase(content.end() - 1);
  content.append("]}");
  std::string path("/sdcard/");
  path.append(title);
  path.append(".cpuprofile");
  FILE* f = fopen(path.c_str(), "w");
  if (f) {
    fprintf(f, "%s", content.c_str());
    fclose(f);
    __android_log_print(ANDROID_LOG_DEBUG, "linzj", "saved to open file: %s\n",
                        path.c_str());
  } else {
    __android_log_print(ANDROID_LOG_DEBUG, "linzj", "fails to open file: %s\n",
                        path.c_str());
  }
}
#endif

jint Java_com_taobao_weex_bridge_WXBridge_execJS(JNIEnv* env,
                                                 jobject this1,
                                                 jstring jinstanceid,
                                                 jstring jnamespace,
                                                 jstring jfunction,
                                                 jobjectArray jargs) {
  v8::HandleScope handleScope(globalIsolate);
  v8::Isolate::Scope isolate_scope(globalIsolate);
  v8::Context::Scope ctx_scope(
      v8::Local<v8::Context>::New(globalIsolate, V8context));
  v8::TryCatch try_catch;
  int length = env->GetArrayLength(jargs);
  v8::Handle<v8::Value> obj[length];
  //	LOGA("jsLog>>> execJSbegin++++++++++++++++++++++++");
  //	jstring2Log(env,jinstanceid,jfunction);
  jclass jsobjectclazz = (env)->FindClass("com/taobao/weex/bridge/WXJSObject");
  for (int i = 0; i < length; i++) {
    jobject jarg = (env)->GetObjectArrayElement(jargs, i);
    //获取js类型
    jfieldID jtypeid = (env)->GetFieldID(jsobjectclazz, "type", "I");
    jint jtypeint = env->GetIntField(jarg, jtypeid);

    jfieldID jdataid =
        (env)->GetFieldID(jsobjectclazz, "data", "Ljava/lang/Object;");
    jobject jdataobj = env->GetObjectField(jarg, jdataid);
    if (jtypeint == 1) {
      jclass jdoubleclazz = (env)->FindClass("java/lang/Double");
      jmethodID jdoublevalueid =
          (env)->GetMethodID(jdoubleclazz, "doubleValue", "()D");
      jdouble jdoubleobj = (env)->CallDoubleMethod(jdataobj, jdoublevalueid);
      obj[i] = v8::Number::New(globalIsolate, (double)jdoubleobj);
      //			LOGA("jsLog>>> instance
      //jdoubleobj:%d",jdoubleobj);
    } else if (jtypeint == 2) {
      jstring jdatastr = (jstring)jdataobj;
      obj[i] = jstring2V8String(env, jdatastr, globalIsolate);
      //			jstring2Log(env, jinstanceid,jdatastr);
    } else if (jtypeint == 2) {
      jstring jdatastr = (jstring)jdataobj;
      obj[i] = jstring2V8String(env, jdatastr, globalIsolate);
      //		    obj[i]=
      //v8::String::NewFromUtf8((env)->GetStringUTFChars(jdatastr,NULL));
    } else if (jtypeint == 3) {
      v8::Handle<v8::Value> jsonobj[1];
      v8::Handle<v8::Object> global =
          v8::Local<v8::Context>::New(globalIsolate, V8context)->Global();
      json = v8::Handle<v8::Object>::Cast(
          global->Get(v8::String::NewFromUtf8(globalIsolate, "JSON")));
      json_parse = v8::Handle<v8::Function>::Cast(
          json->Get(v8::String::NewFromUtf8(globalIsolate, "parse")));
      jsonobj[0] = v8::String::NewFromUtf8(
          globalIsolate, (env)->GetStringUTFChars((jstring)jdataobj, 0));
      v8::Handle<v8::Value> ret = json_parse->Call(json, 1, jsonobj);
      obj[i] = ret;
      //			jstring2Log(env, jinstanceid,(jstring)jdataobj);
    }
  }

  const char* func = (env)->GetStringUTFChars(jfunction, 0);
  v8::Handle<v8::Object> global =
      v8::Local<v8::Context>::New(globalIsolate, V8context)->Global();
  v8::Handle<v8::Function> function;
  v8::Handle<v8::Value> result;
  if (jnamespace == NULL) {
#if ENABLE_PROFILER
    v8::CpuProfiler& profiler = *globalIsolate->GetCpuProfiler();
    char title[256];
    snprintf(title, 256, "%s_%d", func, g_profiler_id++);
    v8::Local<v8::String> v8Title(
        v8::String::NewFromUtf8(globalIsolate, title));
    profiler.StartProfiling(v8Title, false);
    struct timespec t1, t2;
    clock_gettime(CLOCK_MONOTONIC, &t1);
#endif
    function = v8::Handle<v8::Function>::Cast(
        global->Get(v8::String::NewFromUtf8(globalIsolate, func)));
    result = function->Call(global, length, obj);
#if ENABLE_PROFILER
    clock_gettime(CLOCK_MONOTONIC, &t2);
    __android_log_print(ANDROID_LOG_DEBUG, "linzj", "exec %s using %lf.", func,
                        static_cast<double>(t2.tv_sec - t1.tv_sec) +
                            static_cast<double>(t2.tv_nsec - t1.tv_nsec) / 1e9);
    const v8::CpuProfile* profile = profiler.StopProfiling(v8Title);
    saveProfile(profile, title);
    const_cast<v8::CpuProfile*>(profile)->Delete();
#endif
  } else {
    v8::Handle<v8::Object> master =
        v8::Handle<v8::Object>::Cast(global->Get(v8::String::NewFromUtf8(
            globalIsolate, ((env)->GetStringUTFChars(jnamespace, 0)))));
    function =
        v8::Handle<v8::Function>::Cast(master->Get(v8::String::NewFromUtf8(
            globalIsolate, ((env)->GetStringUTFChars(jfunction, 0)))));
    result = function->Call(master, length, obj);
  }
  if (result.IsEmpty()) {
    assert(try_catch.HasCaught());
    ReportException(globalIsolate, &try_catch, jinstanceid, func);
    return false;
  }
  return true;
}
}

bool ExecuteString(v8::Isolate* isolate,
                   v8::Handle<v8::String> source,
                   bool print_result,
                   bool report_exceptions) {
  v8::Isolate::Scope isolate_scope(isolate);
  v8::Context::Scope ctx_scope(v8::Local<v8::Context>::New(isolate, V8context));
  v8::TryCatch try_catch;
  v8::Handle<v8::Script> script = v8::Script::Compile(source);
  if (script.IsEmpty()) {
    // Print errors that happened during compilation.
    if (report_exceptions)
      ReportException(isolate, &try_catch, NULL, "");
    return false;
  } else {
    v8::Handle<v8::Value> result = script->Run();
    if (result.IsEmpty()) {
      assert(try_catch.HasCaught());
      // Print errors that happened during execution.
      if (report_exceptions)
        ReportException(isolate, &try_catch, NULL, "");
      return false;
    } else {
      assert(!try_catch.HasCaught());
      if (print_result && !result->IsUndefined()) {
        // If all went well and the result wasn't undefined then print
        // the returned value.
        //				v8::String::Utf8Value str(result);
      }
      return true;
    }
  }
}

void reportException(jstring jinstanceid,
                     const char* func,
                     const char* exception_string) {
  JNIEnv* env = getJNIEnv();
  jstring jexception_string = (env)->NewStringUTF(exception_string);
  jstring jfunc = (env)->NewStringUTF(func);
  jmethodID tempMethodId = (env)->GetMethodID(
      jBridgeClazz, "reportJSException",
      "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
  (env)->CallVoidMethod(jthis, tempMethodId, jinstanceid, jfunc,
                        jexception_string);
}
void ReportException(v8::Isolate* isolate,
                     v8::TryCatch* try_catch,
                     jstring jinstanceid,
                     const char* func) {
  v8::HandleScope handle_scope(isolate);
  v8::String::Utf8Value exception(try_catch->Exception());
  v8::Handle<v8::Message> message = try_catch->Message();
  if (message.IsEmpty()) {
    // V8 didn't provide any extra information about this error; just
    // print the exception.
    LOGE(" ReportException : %s", ToCString(exception));
  } else {
    v8::String::Utf8Value filename(message->GetScriptResourceName());
    const char* filename_string = ToCString(filename);
    int linenum = message->GetLineNumber();
    LOGE(" ReportException :%s:%i: %s", filename_string, linenum,
         ToCString(exception));
    // Print line of source code.
    v8::String::Utf8Value stack_trace(try_catch->StackTrace());
    if (stack_trace.length() > 0) {
      const char* stack_trace_string = ToCString(stack_trace);
      LOGE(" ReportException : %s", stack_trace_string);
    }
  }
  //上报异常信息到java端
  reportException(jinstanceid, func, ToCString(exception));
}

void callNative(const v8::FunctionCallbackInfo<v8::Value>& args) {
  LOGD(" callNative");
  JNIEnv* env = getJNIEnv();
  jstring jtaskString = NULL;
  if (args[1]->IsObject()) {
    LOGD("args[1] is object");
    v8::Handle<v8::Value> obj[1];
    v8::Handle<v8::Object> global =
        v8::Local<v8::Context>::New(globalIsolate, V8context)->Global();
    json = v8::Handle<v8::Object>::Cast(
        global->Get(v8::String::NewFromUtf8(globalIsolate, "JSON")));
    json_stringify = v8::Handle<v8::Function>::Cast(
        json->Get(v8::String::NewFromUtf8(globalIsolate, "stringify")));
    obj[0] = args[1];
    v8::Handle<v8::Value> ret = json_stringify->Call(json, 1, obj);
    v8::String::Utf8Value str(ret);
    jtaskString = (env)->NewStringUTF(ToCString(str));
    LOGD(" callNative is object");
  } else if (args[1]->IsString()) {
    LOGD("args[1] is String");
    v8::String::Utf8Value tasks(args[1]);
    jtaskString = (env)->NewStringUTF(*tasks);
  }

  v8::String::Utf8Value instanceId(args[0]);
  jstring jcallback = NULL;
  if (!args[2].IsEmpty()) {
    v8::String::Utf8Value instanceId(args[2]);
    jcallback = (env)->NewStringUTF(*instanceId);
  }
  jstring jinstanceId = (env)->NewStringUTF(*instanceId);
  jmethodID tempMethodId = (env)->GetMethodID(
      jBridgeClazz, "callNative",
      "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
  (env)->CallVoidMethod(jthis, tempMethodId, jinstanceId, jtaskString,
                        jcallback);
  args.GetReturnValue().Set(v8::True(args.GetIsolate()));
}

void setTimeoutNative(const v8::FunctionCallbackInfo<v8::Value>& args) {
  LOGD("setTimeoutNative");
  JNIEnv* env = getJNIEnv();
  // callbackId
  v8::String::Utf8Value callbackID(args[0]);
  jstring jcallbackID = (env)->NewStringUTF(*callbackID);

  // time
  v8::String::Utf8Value time(args[1]);
  jstring jtime = (env)->NewStringUTF(*time);

  jmethodID tempMethodId =
      (env)->GetMethodID(jBridgeClazz, "setTimeoutNative",
                         "(Ljava/lang/String;Ljava/lang/String;)V");
  (env)->CallVoidMethod(jthis, tempMethodId, jcallbackID, jtime);
  LOGD("setTimeoutNative end");
  args.GetReturnValue().Set(v8::True(args.GetIsolate()));
}

// js log
void nativeLog(const v8::FunctionCallbackInfo<v8::Value>& args) {
  LOGD("begin nativelog");

  char s[1000] = "";  //
  char* cp = s;
  int available_len = sizeof(s) - 1;
  for (int i = 0; i < args.Length(); i++) {
    v8::String::Utf8Value str(args[i]);
    // const char* cstr = *str;
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
  //	}catch(int ss){
  //		LOGA("jsLog>>>>:%s",s);
  //	}
  args.GetReturnValue().Set(v8::True(args.GetIsolate()));
}

// Creates a new execution environment containing the built-in
// functions.
v8::Local<v8::Context> CreateShellContext() {
  // Create a template for the global object.
  v8::Handle<v8::ObjectTemplate> global =
      v8::ObjectTemplate::New(globalIsolate);
  // Bind the global 'callNative' function to the C++ Print callback.
  global->Set(v8::String::NewFromUtf8(globalIsolate, "callNative"),
              v8::FunctionTemplate::New(globalIsolate, callNative));
  global->Set(v8::String::NewFromUtf8(globalIsolate, "setTimeoutNative"),
              v8::FunctionTemplate::New(globalIsolate, setTimeoutNative));
  // Bind the 'getType' function
  global->Set(v8::String::NewFromUtf8(globalIsolate, "nativeLog"),
              v8::FunctionTemplate::New(globalIsolate, nativeLog));

  global->Set(v8::String::NewFromUtf8(globalIsolate, "WXEnvironment"),
              WXEnvironment);

  return v8::Context::New(globalIsolate, NULL, global);
}

/* This function will be call when the library first be load.
 * You can do some init in the libray. return which version jni it support.
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

  LOGD("end JNI_OnLoad");
  return JNI_VERSION_1_4;
}

void JNI_OnUnload(JavaVM* vm, void* reserved) {
  LOGD("beigin JNI_OnUnload");
  V8context.Reset();
  v8::V8::Dispose();
  LOGD(" end JNI_OnUnload");
}
