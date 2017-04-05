
#include "mozilla/ArrayUtils.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/IntegerPrintfMacros.h"
// #include "mozilla/mozalloc.h"
#include "mozilla/PodOperations.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/SizePrintfMacros.h"
#include "mozilla/Sprintf.h"
#include "mozilla/TimeStamp.h"

#ifdef XP_WIN
#include <direct.h>
#include <process.h>
#endif
#include <errno.h>
#include <fcntl.h>
#if defined(XP_WIN)
#include <io.h> /* for isatty() */
#endif
#include <locale.h>
#if defined(MALLOC_H)
#include MALLOC_H /* for malloc_usable_size, malloc_size, _msize */
#endif
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef XP_UNIX
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsfun.h"
#include "jsobj.h"
#include "jsprf.h"
#include "jsscript.h"
#include "jstypes.h"
#include "jsutil.h"
#ifdef XP_WIN
#include "jswin.h"
#endif
#include "jswrapper.h"

#include "builtin/ModuleObject.h"
#include "builtin/TestingFunctions.h"
#include "frontend/Parser.h"
#include "gc/GCInternals.h"
#include "jit/InlinableNatives.h"
#include "jit/Ion.h"
#include "jit/JitcodeMap.h"
#include "jit/OptimizationTracking.h"
#include "jit/arm/Simulator-arm.h"
#include "js/Debug.h"
#include "js/GCAPI.h"
#include "js/Initialization.h"
#include "js/StructuredClone.h"
#include "js/TrackedOptimizationInfo.h"
#include "perf/jsperf.h"
#include "threading/ConditionVariable.h"
#include "threading/LockGuard.h"
#include "threading/Thread.h"
#include "vm/ArgumentsObject.h"
#include "vm/AsyncFunction.h"
#include "vm/Compression.h"
#include "vm/Debugger.h"
#include "vm/HelperThreads.h"
#include "vm/Monitor.h"
#include "vm/MutexIDs.h"
#include "vm/Shape.h"
#include "vm/SharedArrayObject.h"
#include "vm/StringBuffer.h"
#include "vm/Time.h"
#include "vm/TypedArrayObject.h"
#include "vm/WrapperObject.h"
#include "wasm/WasmJS.h"

#include "jscompartmentinlines.h"
#include "jsobjinlines.h"

#include "vm/ErrorObject-inl.h"
#include "vm/Interpreter-inl.h"
#include "vm/Stack-inl.h"

using namespace js;

using mozilla::ArrayLength;
using mozilla::Atomic;
using mozilla::MakeScopeExit;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::NumberEqualsInt32;
using mozilla::PodCopy;
using mozilla::PodEqual;
using mozilla::TimeDuration;
using mozilla::TimeStamp;

#include "LogUtils.h"
#include "Trace.h"
#include <jni.h>

static void
markupStateInternally();
namespace {
static bool
GCAndSweep(JSContext* cx, unsigned argc, Value* vp);
static bool
CallNative(JSContext* cx, unsigned argc, Value* vp);
static bool
CallNativeModule(JSContext* cx, unsigned argc, Value* vp);
static bool
CallNativeComponent(JSContext* cx, unsigned argc, Value* vp);
static bool
CallAddElement(JSContext* cx, unsigned argc, Value* vp);
static bool
SetTimeoutNative(JSContext* cx, unsigned argc, Value* vp);
static bool
NativeLog(JSContext* cx, unsigned argc, Value* vp);
static bool
NotifyTrimMemory(JSContext* cx, unsigned argc, Value* vp);
static bool
MarkupState(JSContext* cx, unsigned argc, Value* vp);
static JNIEnv*
getJNIEnv();

static jclass jBridgeClazz;
static jclass jWXJSObject;
static jclass jWXLogUtils;
static jmethodID jCallAddElementMethodId;
static jmethodID jDoubleValueMethodId;
static jmethodID jSetTimeoutNativeMethodId;
static jmethodID jCallNativeMethodId;
static jmethodID jCallNativeModuleMethodId;
static jmethodID jCallNativeComponentMethodId;
static jmethodID jLogMethodId;
static jobject jThis;
static JavaVM* sVm = NULL;

class GlobalObjectHelper
{
public:
  static void initWXEnvironment(JNIEnv* env,
                                jobject params,
                                JSContext* cx,
                                HandleObject globalObject);
  static bool initFunction(JSContext* cx, HandleObject obj);

  static void DefineString(JSContext* cx,
                           HandleObject object,
                           const char* name,
                           JNIEnv* env,
                           jstring str);
};

class ScopedJStringUTF8
{
public:
  ScopedJStringUTF8(JNIEnv* env, jstring);
  ~ScopedJStringUTF8();
  const char* getChars();

private:
  JNIEnv* m_env;
  jstring m_jstring;
  const char* m_chars;
};

class ScopedJString
{
public:
  ScopedJString(JNIEnv* env, jstring);
  ~ScopedJString();
  const jchar* getChars();
  size_t getCharsLength();

private:
  JNIEnv* m_env;
  jstring m_jstring;
  const uint16_t* m_chars;
  size_t m_len;
};

#ifdef SPIDERMONKEY_PROMISE
using JobQueue = GCVector<JSObject*, 0, SystemAllocPolicy>;
struct ShellAsyncTasks
{
  explicit ShellAsyncTasks(JSContext* cx)
    : outstanding(0)
    , finished(cx)
  {
  }

  size_t outstanding;
  Vector<JS::AsyncTask*> finished;
};
#endif // SPIDERMONKEY_PROMISE
struct ShellContext
{
  explicit ShellContext(JSContext* cx);
#ifdef SPIDERMONKEY_PROMISE
  JS::PersistentRootedValue promiseRejectionTrackerCallback;
  JS::PersistentRooted<JobQueue> jobQueue;
  ExclusiveData<ShellAsyncTasks> asyncTasks;
  bool drainingJobQueue;
#endif // SPIDERMONKEY_PROMISE
  JSContext* cx_;
};

ScopedJStringUTF8::ScopedJStringUTF8(JNIEnv* env, jstring _jstring)
  : m_env(env)
  , m_jstring(_jstring)
  , m_chars(nullptr)
{
}

ScopedJStringUTF8::~ScopedJStringUTF8()
{
  if (m_chars)
    m_env->ReleaseStringUTFChars(m_jstring, m_chars);
}

const char*
ScopedJStringUTF8::getChars()
{
  if (m_chars)
    return m_chars;
  m_chars = m_env->GetStringUTFChars(m_jstring, nullptr);
  return m_chars;
}

ScopedJString::ScopedJString(JNIEnv* env, jstring _jstring)
  : m_env(env)
  , m_jstring(_jstring)
  , m_chars(nullptr)
  , m_len(0)
{
}

ScopedJString::~ScopedJString()
{
  if (m_chars)
    m_env->ReleaseStringChars(m_jstring, m_chars);
}

const jchar*
ScopedJString::getChars()
{
  if (m_chars)
    return m_chars;
  m_chars = m_env->GetStringChars(m_jstring, nullptr);
  m_len = m_env->GetStringLength(m_jstring);
  return m_chars;
}

size_t
ScopedJString::getCharsLength()
{
  if (m_chars)
    return m_len;
  m_len = m_env->GetStringLength(m_jstring);
  return m_len;
}

static JSString*
jString2JSValue(JNIEnv* env, JSContext* cx, jstring str)
{
  ScopedJString scopedString(env, str);
  const jchar* chars = scopedString.getChars();
  size_t len = scopedString.getCharsLength();
  return JS_NewUCStringCopyN(cx, (const char16_t*)chars, len);
}

static jstring
getArgumentAsJString(JNIEnv* env, JSContext* cx, CallArgs& args, int argument)
{
  jstring ret = nullptr;
  if (argument >= args.length())
    return nullptr;
  HandleValue val = args[argument];
  JSString* s = JS::ToString(cx, val);
  if (s) {
    RootedString rs(cx, s);
    char* chars = JS_EncodeStringToUTF8(cx, rs);
    if (!chars)
      return nullptr;
    ret = env->NewStringUTF(chars);
    js_free(chars);
  } else {
    abort();
  }
  return ret;
}

static bool
parseToObject(JNIEnv* env, JSContext* cx, jstring from, MutableHandleValue val)
{
  ScopedJString scopestr(env, from);
  const jchar* str = scopestr.getChars();
  size_t strLen = scopestr.getCharsLength();
  return JS_ParseJSON(cx, (const char16_t*)str, strLen, val);
}

static bool
JSONCreator(const char16_t* aBuf, uint32_t aLen, void* aData)
{
  mozilla::Vector<char16_t>* result =
    static_cast<mozilla::Vector<char16_t>*>(aData);
  if (!result->append(static_cast<const char16_t*>(aBuf),
                      static_cast<uint32_t>(aLen))) {
    LOGE("unable to append jchar in function: JSONCreator");
    return false;
  }
  return true;
}

static jbyteArray
getArgumentAsJByteArrayJSON(JNIEnv* env,
                            JSContext* cx,
                            CallArgs& args,
                            int argument)
{
  jbyteArray ba = nullptr;
  RootedValue val(cx, args[argument]);
  if (val.isObject()) {
    mozilla::Vector<char16_t> result;
    if (!JS_Stringify(
          cx, &val, nullptr, JS::NullHandleValue, JSONCreator, &result)) {
      return nullptr;
    }
    // FIXME: shit copies.
    mozilla::Range<char16_t> range(result.begin(), result.length());
    auto utf8 = CharsToNewUTF8CharsZ(cx, range);
    size_t strLen = strlen(utf8.c_str());
    ba = env->NewByteArray(strLen);
    env->SetByteArrayRegion(
      ba, 0, strLen, reinterpret_cast<const jbyte*>(utf8.c_str()));
  }
  return ba;
}

static jbyteArray
getArgumentAsJByteArray(JNIEnv* env,
                        JSContext* cx,
                        CallArgs& args,
                        int argument)
{
  jbyteArray ba = nullptr;
  HandleValue val = args[argument];
  if (val.isString()) {
    RootedString jsstring(cx, val.toString());
    char* chars = JS_EncodeStringToUTF8(cx, jsstring);
    if (!chars)
      return nullptr;
    int strLen = strlen(chars);
    UTF8CharsZ autoStr(chars, strLen);
    ba = env->NewByteArray(strLen);
    env->SetByteArrayRegion(
      ba, 0, strLen, reinterpret_cast<const jbyte*>(chars));
  } else {
    ba = getArgumentAsJByteArrayJSON(env, cx, args, argument);
  }
  return ba;
}

static bool
GCAndSweep(JSContext* cx, unsigned argc, Value* vp)
{
  JS_GC(cx);
  return true;
}

static bool
CallNative(JSContext* cx, unsigned argc, Value* vp)
{
  base::debug::TraceScope traceScope("weex", "callNative");

  CallArgs args = CallArgsFromVp(argc, vp);

  JNIEnv* env = getJNIEnv();
  // instacneID args[0]
  jstring jInstanceId = getArgumentAsJString(env, cx, args, 0);
  // task args[1]
  jbyteArray jTaskString = getArgumentAsJByteArray(env, cx, args, 1);
  // callback args[2]
  jstring jCallback = getArgumentAsJString(env, cx, args, 2);

  if (jCallNativeMethodId == NULL) {
    jCallNativeMethodId = env->GetMethodID(
      jBridgeClazz, "callNative", "(Ljava/lang/String;[BLjava/lang/String;)I");
  }

  int flag = env->CallIntMethod(
    jThis, jCallNativeMethodId, jInstanceId, jTaskString, jCallback);
  if (flag == -1) {
    LOGE("instance destroy JFM must stop callNative");
  }
  env->DeleteLocalRef(jTaskString);
  env->DeleteLocalRef(jInstanceId);
  env->DeleteLocalRef(jCallback);

  args.rval().set(Int32Value(flag));
  return true;
}

static bool
CallNativeModule(JSContext* cx, unsigned argc, Value* vp)
{
  base::debug::TraceScope traceScope("weex", "callNativeModule");
  CallArgs args = CallArgsFromVp(argc, vp);

  JNIEnv* env = getJNIEnv();
  // instacneID args[0]
  jstring jInstanceId = getArgumentAsJString(env, cx, args, 0);

  // module args[1]
  jstring jmodule = getArgumentAsJString(env, cx, args, 1);

  // method args[2]
  jstring jmethod = getArgumentAsJString(env, cx, args, 2);

  // arguments args[3]
  jbyteArray jArgString = getArgumentAsJByteArrayJSON(env, cx, args, 3);
  // arguments args[4]
  jbyteArray jOptString = getArgumentAsJByteArrayJSON(env, cx, args, 4);

  if (jCallNativeModuleMethodId == NULL) {
    jCallNativeModuleMethodId =
      env->GetMethodID(jBridgeClazz,
                       "callNativeModule",
                       "(Ljava/lang/String;Ljava/lang/"
                       "String;Ljava/lang/String;[B[B)Ljava/"
                       "lang/Object;");
  }

  jobject result = env->CallObjectMethod(jThis,
                                         jCallNativeModuleMethodId,
                                         jInstanceId,
                                         jmodule,
                                         jmethod,
                                         jArgString,
                                         jOptString);
  MutableHandleValue ret = args.rval();

  jfieldID jTypeId = env->GetFieldID(jWXJSObject, "type", "I");
  jint jTypeInt = env->GetIntField(result, jTypeId);
  jfieldID jDataId = env->GetFieldID(jWXJSObject, "data", "Ljava/lang/Object;");
  jobject jDataObj = env->GetObjectField(result, jDataId);
  if (jTypeInt == 1) {
    if (jDoubleValueMethodId == NULL) {
      jclass jDoubleClazz = env->FindClass("java/lang/Double");
      jDoubleValueMethodId =
        env->GetMethodID(jDoubleClazz, "doubleValue", "()D");
      env->DeleteLocalRef(jDoubleClazz);
    }
    jdouble jDoubleObj = env->CallDoubleMethod(jDataObj, jDoubleValueMethodId);
    ret.setNumber((double)jDoubleObj);

  } else if (jTypeInt == 2) {
    jstring jDataStr = (jstring)jDataObj;
    ret.setString(jString2JSValue(env, cx, jDataStr));
  } else if (jTypeInt == 3) {
    if (!parseToObject(env, cx, (jstring)jDataObj, ret)) {
      // TODO: print exception
    }
  }
  env->DeleteLocalRef(jDataObj);
  env->DeleteLocalRef(jInstanceId);
  env->DeleteLocalRef(jmodule);
  env->DeleteLocalRef(jmethod);
  env->DeleteLocalRef(jArgString);
  env->DeleteLocalRef(jOptString);
  return true;
}

static bool
CallNativeComponent(JSContext* cx, unsigned argc, Value* vp)
{
  base::debug::TraceScope traceScope("weex", "callNativeComponent");
  JNIEnv* env = getJNIEnv();
  CallArgs args = CallArgsFromVp(argc, vp);

  // instacneID args[0]
  jstring jInstanceId = getArgumentAsJString(env, cx, args, 0);

  // module args[1]
  jstring jcomponentRef = getArgumentAsJString(env, cx, args, 1);

  // method args[2]
  jstring jmethod = getArgumentAsJString(env, cx, args, 2);

  // arguments args[3]
  jbyteArray jArgString = getArgumentAsJByteArrayJSON(env, cx, args, 3);

  // arguments args[4]
  jbyteArray jOptString = getArgumentAsJByteArrayJSON(env, cx, args, 4);

  if (jCallNativeComponentMethodId == NULL) {
    jCallNativeComponentMethodId = env->GetMethodID(
      jBridgeClazz,
      "callNativeComponent",
      "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[B[B)V");
  }

  env->CallVoidMethod(jThis,
                      jCallNativeComponentMethodId,
                      jInstanceId,
                      jcomponentRef,
                      jmethod,
                      jArgString,
                      jOptString);

  env->DeleteLocalRef(jInstanceId);
  env->DeleteLocalRef(jcomponentRef);
  env->DeleteLocalRef(jmethod);
  env->DeleteLocalRef(jArgString);
  env->DeleteLocalRef(jOptString);
  args.rval().setBoolean(true);
  return true;
}

static bool
CallAddElement(JSContext* cx, unsigned argc, Value* vp)
{
  base::debug::TraceScope traceScope("weex", "callAddElement");
  JNIEnv* env = getJNIEnv();
  CallArgs args = CallArgsFromVp(argc, vp);
  // instacneID args[0]
  jstring jInstanceId = getArgumentAsJString(env, cx, args, 0);
  // instacneID args[1]
  jstring jref = getArgumentAsJString(env, cx, args, 1);
  // dom node args[2]
  jbyteArray jdomString = getArgumentAsJByteArray(env, cx, args, 2);
  // index  args[3]
  jstring jindex = getArgumentAsJString(env, cx, args, 3);
  // callback  args[4]
  jstring jCallback = getArgumentAsJString(env, cx, args, 4);
  if (jCallAddElementMethodId == NULL) {
    jCallAddElementMethodId =
      env->GetMethodID(jBridgeClazz,
                       "callAddElement",
                       "(Ljava/lang/String;Ljava/lang/"
                       "String;[BLjava/lang/String;Ljava/lang/"
                       "String;)I");
  }

  int flag = env->CallIntMethod(jThis,
                                jCallAddElementMethodId,
                                jInstanceId,
                                jref,
                                jdomString,
                                jindex,
                                jCallback);
  if (flag == -1) {
    LOGE("instance destroy JFM must stop callNative");
  }
  env->DeleteLocalRef(jInstanceId);
  env->DeleteLocalRef(jref);
  env->DeleteLocalRef(jdomString);
  env->DeleteLocalRef(jindex);
  env->DeleteLocalRef(jCallback);
  args.rval().set(Int32Value(flag));
  return true;
}

static bool
SetTimeoutNative(JSContext* cx, unsigned argc, Value* vp)
{
  base::debug::TraceScope traceScope("weex", "setTimeoutNative");
  JNIEnv* env = getJNIEnv();
  CallArgs args = CallArgsFromVp(argc, vp);
  // callbackId
  jstring jCallbackID = getArgumentAsJString(env, cx, args, 0);

  // time
  jstring jTime = getArgumentAsJString(env, cx, args, 1);

  if (jSetTimeoutNativeMethodId == NULL) {
    jSetTimeoutNativeMethodId =
      env->GetMethodID(jBridgeClazz,
                       "setTimeoutNative",
                       "(Ljava/lang/String;Ljava/lang/String;)V");
  }
  env->CallVoidMethod(jThis, jSetTimeoutNativeMethodId, jCallbackID, jTime);
  env->DeleteLocalRef(jCallbackID);
  env->DeleteLocalRef(jTime);
  args.rval().setBoolean(true);
  return true;
}

static bool
NativeLog(JSContext* cx, unsigned argc, Value* vp)
{
  JNIEnv* env;
  bool result = false;
  CallArgs args = CallArgsFromVp(argc, vp);

  args.rval().setBoolean(true);

  if (args.length() == 0) {
    return true;
  }
  RootedString sb(cx, JS::ToString(cx, args[0]));
  for (int i = 1; i < args.length(); i++) {
    RootedString right(cx, JS::ToString(cx, args[i]));
    sb = JS_ConcatStrings(cx, sb, right);
  }

  do {
    env = getJNIEnv();
    char* utf8 = JS_EncodeStringToUTF8(cx, sb);
    if (!utf8)
      break;
    jstring str_msg = env->NewStringUTF(utf8);
    js_free(utf8);
    if (jWXLogUtils != NULL) {
      if (jLogMethodId == NULL) {
        jLogMethodId = env->GetStaticMethodID(
          jWXLogUtils, "d", "(Ljava/lang/String;Ljava/lang/String;)V");
      }
      if (jLogMethodId != NULL) {
        jstring str_tag = env->NewStringUTF("jsLog");
        // str_msg = env->NewStringUTF(s);
        env->CallStaticVoidMethod(jWXLogUtils, jLogMethodId, str_tag, str_msg);
        result = true;
        env->DeleteLocalRef(str_msg);
        env->DeleteLocalRef(str_tag);
      }
    }
  } while (false);
  return true;
}

static bool
NotifyTrimMemory(JSContext* cx, unsigned argc, Value* vp)
{
  JS_GC(cx);
  return true;
}

static bool
MarkupState(JSContext* cx, unsigned argc, Value* vp)
{
  markupStateInternally();
  return true;
}

void
GlobalObjectHelper::DefineString(JSContext* cx,
                                 HandleObject object,
                                 const char* name,
                                 JNIEnv* env,
                                 jstring str)
{
  ScopedJString scopestr(env, str);
  const jchar* ustr = scopestr.getChars();
  size_t strLen = scopestr.getCharsLength();
  RootedString jsvalue(cx, JS_NewUCStringCopyN(cx, (char16_t*)ustr, strLen));
  JS_DefineProperty(cx, object, name, jsvalue, JSPROP_PERMANENT);
}

void
GlobalObjectHelper::initWXEnvironment(JNIEnv* env,
                                      jobject params,
                                      JSContext* cx,
                                      HandleObject globalObject)
{
  RootedObject WXEnvironment(cx, JS_NewObject(cx, nullptr));
#define ADDSTRING(name)                                                        \
  DefineString(cx, WXEnvironment, #name, env, (jstring)name)

  jclass c_params = env->GetObjectClass(params);

  jmethodID m_platform =
    env->GetMethodID(c_params, "getPlatform", "()Ljava/lang/String;");
  jobject platform = env->CallObjectMethod(params, m_platform);
  ADDSTRING(platform);
  env->DeleteLocalRef(platform);

  jmethodID m_osVersion =
    env->GetMethodID(c_params, "getOsVersion", "()Ljava/lang/String;");
  jobject osVersion = env->CallObjectMethod(params, m_osVersion);
  ADDSTRING(osVersion);
  env->DeleteLocalRef(osVersion);

  jmethodID m_appVersion =
    env->GetMethodID(c_params, "getAppVersion", "()Ljava/lang/String;");
  jobject appVersion = env->CallObjectMethod(params, m_appVersion);
  ADDSTRING(appVersion);
  env->DeleteLocalRef(appVersion);

  jmethodID m_weexVersion =
    env->GetMethodID(c_params, "getWeexVersion", "()Ljava/lang/String;");
  jobject weexVersion = env->CallObjectMethod(params, m_weexVersion);
  ADDSTRING(weexVersion);
  env->DeleteLocalRef(weexVersion);

  jmethodID m_deviceModel =
    env->GetMethodID(c_params, "getDeviceModel", "()Ljava/lang/String;");
  jobject deviceModel = env->CallObjectMethod(params, m_deviceModel);
  ADDSTRING(deviceModel);
  env->DeleteLocalRef(deviceModel);

  jmethodID m_appName =
    env->GetMethodID(c_params, "getAppName", "()Ljava/lang/String;");
  jobject appName = env->CallObjectMethod(params, m_appName);
  ADDSTRING(appName);
  env->DeleteLocalRef(appName);

  jmethodID m_deviceWidth =
    env->GetMethodID(c_params, "getDeviceWidth", "()Ljava/lang/String;");
  jobject deviceWidth = env->CallObjectMethod(params, m_deviceWidth);
  ADDSTRING(deviceWidth);
  env->DeleteLocalRef(deviceWidth);

  jmethodID m_deviceHeight =
    env->GetMethodID(c_params, "getDeviceHeight", "()Ljava/lang/String;");
  jobject deviceHeight = env->CallObjectMethod(params, m_deviceHeight);
  ADDSTRING(deviceHeight);
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
        GlobalObjectHelper::DefineString(cx, WXEnvironment, c_key, env, jvalue);
        env->DeleteLocalRef(jkey);
        if (jvalue != NULL) {
          env->DeleteLocalRef(jvalue);
        }
      }
    }
    env->DeleteLocalRef(jobjArray);
  }
  env->DeleteLocalRef(options);
  JS_DefineProperty(
    cx, globalObject, "WXEnvironment", WXEnvironment, JSPROP_PERMANENT);
}

static const JSFunctionSpecWithHelp global_functions[] = {
  JS_FN_HELP("callNative", CallNative, 0, 0, "", ""),
  JS_FN_HELP("callNativeModule", CallNativeModule, 0, 0, "", ""),
  JS_FN_HELP("callNativeComponent", CallNativeComponent, 0, 0, "", ""),
  JS_FN_HELP("callAddElement", CallAddElement, 0, 0, "", ""),
  JS_FN_HELP("setTimeoutNative", SetTimeoutNative, 0, 0, "", ""),
  JS_FN_HELP("nativeLog", NativeLog, 0, 0, "", ""),
  JS_FN_HELP("notifyTrimMemory", NotifyTrimMemory, 0, 0, "", ""),
  JS_FN_HELP("markupState", MarkupState, 0, 0, "", ""),
  JS_FS_HELP_END
};

bool
GlobalObjectHelper::initFunction(JSContext* cx, HandleObject globalObject)
{
  return JS_DefineFunctionsWithHelp(cx, globalObject, global_functions);
}

JNIEnv*
getJNIEnv()
{
  JNIEnv* env = NULL;
  if ((sVm)->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK) {
    return JNI_FALSE;
  }
  return env;
}

static void
WarningReporter(JSContext* cx, JSErrorReport* report)
{
  LOGE("ION WARNING: %s", report->message().c_str());
}

static bool
BuildId(JS::BuildIdCharVector* buildId)
{
  // The browser embeds the date into the buildid and the buildid is embedded
  // in the binary, so every 'make' necessarily builds a new firefox binary.
  // Fortunately, the actual firefox executable is tiny -- all the code is in
  // libxul.so and other shared modules -- so this isn't a big deal. Not so
  // for the statically-linked JS shell. To avoid recompiling js.cpp and
  // re-linking 'js' on every 'make', we use a constant buildid and rely on
  // the shell user to manually clear the cache (deleting the dir passed to
  // --js-cache) between cache-breaking updates. Note: jit_tests.py does this
  // on every run).
  const char buildid[] = "WEEX";
  return buildId->append(buildid, sizeof(buildid));
}

static void
my_LargeAllocFailCallback(void* data)
{
  JSContext* cx = (JSContext*)data;
  JSRuntime* rt = cx->runtime();

  if (!cx->isJSContext())
    return;

  MOZ_ASSERT(!rt->isHeapBusy());

  JS::PrepareForFullGC(cx);
  AutoKeepAtoms keepAtoms(cx->perThreadData);
  rt->gc.gc(GC_NORMAL, JS::gcreason::SHARED_MEMORY_LIMIT);
}

static void
SetStandardCompartmentOptions(JS::CompartmentOptions& options)
{
  options.behaviors().setVersion(JSVERSION_DEFAULT);
  options.creationOptions().setSharedMemoryAndAtomicsEnabled(false);
}

static bool
global_enumerate(JSContext* cx, HandleObject obj)
{
#ifdef LAZY_STANDARD_CLASSES
  return JS_EnumerateStandardClasses(cx, obj);
#else
  return true;
#endif
}

static bool
global_resolve(JSContext* cx, HandleObject obj, HandleId id, bool* resolvedp)
{
#ifdef LAZY_STANDARD_CLASSES
  if (!JS_ResolveStandardClass(cx, obj, id, resolvedp))
    return false;
#endif
  return true;
}

static bool
global_mayResolve(const JSAtomState& names, jsid id, JSObject* maybeObj)
{
  return JS_MayResolveStandardClass(names, id, maybeObj);
}

static ShellContext*
GetShellContext(JSContext* cx)
{
  ShellContext* sc = static_cast<ShellContext*>(JS_GetContextPrivate(cx));
  MOZ_ASSERT(sc);
  return sc;
}

#ifdef SPIDERMONKEY_PROMISE
static JSObject*
ShellGetIncumbentGlobalCallback(JSContext* cx)
{
  return JS::CurrentGlobalOrNull(cx);
}

static bool
ShellEnqueuePromiseJobCallback(JSContext* cx,
                               JS::HandleObject job,
                               JS::HandleObject allocationSite,
                               JS::HandleObject incumbentGlobal,
                               void* data)
{
  ShellContext* sc = GetShellContext(cx);
  MOZ_ASSERT(job);
  return sc->jobQueue.append(job);
}

static bool
ShellStartAsyncTaskCallback(JSContext* cx, JS::AsyncTask* task)
{
  ShellContext* sc = GetShellContext(cx);
  task->user = sc;

  ExclusiveData<ShellAsyncTasks>::Guard asyncTasks = sc->asyncTasks.lock();
  asyncTasks->outstanding++;
  return true;
}

static bool
ShellFinishAsyncTaskCallback(JS::AsyncTask* task)
{
  ShellContext* sc = (ShellContext*)task->user;

  ExclusiveData<ShellAsyncTasks>::Guard asyncTasks = sc->asyncTasks.lock();
  MOZ_ASSERT(asyncTasks->outstanding > 0);
  asyncTasks->outstanding--;
  return asyncTasks->finished.append(task);
}
#endif // SPIDERMONKEY_PROMISE

static bool
DrainJobQueue(JSContext* cx)
{
#ifdef SPIDERMONKEY_PROMISE
  ShellContext* sc = GetShellContext(cx);
  if (sc->drainingJobQueue)
    return true;

  // Wait for any outstanding async tasks to finish so that the
  // finishedAsyncTasks list is fixed.
  while (true) {
    AutoLockHelperThreadState lock;
    if (!sc->asyncTasks.lock()->outstanding)
      break;
    HelperThreadState().wait(lock, GlobalHelperThreadState::CONSUMER);
  }

  // Lock the whole time while copying back the asyncTasks finished queue so
  // that any new tasks created during finish() cannot racily join the job
  // queue.  Call finish() only thereafter, to avoid a circular mutex
  // dependency (see also bug 1297901).
  Vector<JS::AsyncTask*> finished(cx);
  {
    ExclusiveData<ShellAsyncTasks>::Guard asyncTasks = sc->asyncTasks.lock();
    finished = Move(asyncTasks->finished);
    asyncTasks->finished.clear();
  }

  for (JS::AsyncTask* task : finished)
    task->finish(cx);

  // It doesn't make sense for job queue draining to be reentrant. At the
  // same time we don't want to assert against it, because that'd make
  // drainJobQueue unsafe for fuzzers. We do want fuzzers to test this, so
  // we simply ignore nested calls of drainJobQueue.
  sc->drainingJobQueue = true;

  RootedObject job(cx);
  JS::HandleValueArray args(JS::HandleValueArray::empty());
  RootedValue rval(cx);
  // Execute jobs in a loop until we've reached the end of the queue.
  // Since executing a job can trigger enqueuing of additional jobs,
  // it's crucial to re-check the queue length during each iteration.
  for (size_t i = 0; i < sc->jobQueue.length(); i++) {
    job = sc->jobQueue[i];
    AutoCompartment ac(cx, job);
    {
      JS::AutoSaveExceptionState scopedExceptionState(cx);
      JS::Call(cx, UndefinedHandleValue, job, args, &rval);
    }
    sc->jobQueue[i].set(nullptr);
  }
  sc->jobQueue.clear();
  sc->drainingJobQueue = false;
#endif // SPIDERMONKEY_PROMISE
  return true;
}

static bool
initGlobalObject(JNIEnv* env, jobject params, JSContext* cx, HandleObject glob)
{
  JSAutoCompartment ac(cx, glob);

#ifndef LAZY_STANDARD_CLASSES
  if (!JS_InitStandardClasses(cx, glob))
    return false;
#endif
  bool succeeded;
  if (!JS_SetImmutablePrototype(cx, glob, &succeeded))
    return false;
  if (!GlobalObjectHelper::initFunction(cx, glob))
    return false;
  GlobalObjectHelper::initWXEnvironment(env, params, cx, glob);

  JS_FireOnNewGlobalObject(cx, glob);
  return true;
}

ShellContext::ShellContext(JSContext* cx)
  :
#ifdef SPIDERMONKEY_PROMISE
  promiseRejectionTrackerCallback(cx, NullValue())
  , asyncTasks(mutexid::ShellAsyncTasks, cx)
  , drainingJobQueue(false)
  ,
#endif // SPIDERMONKEY_PROMISE
  cx_(cx)
{
}

static const JSClassOps global_classOps = {
  nullptr,          nullptr,        nullptr,           nullptr,
  global_enumerate, global_resolve, global_mayResolve, nullptr,
  nullptr,          nullptr,        nullptr,           JS_GlobalObjectTraceHook
};

static const JSClass global_class = { "global",
                                      JSCLASS_GLOBAL_FLAGS,
                                      &global_classOps };
}

static JSContext* globalContext = nullptr;
static JS::Heap<JSObject*> _globalObject;

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

long
getCPUTime()
{
  struct timespec ts;
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
  return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static void
ReportException(JSContext* cx,
                HandleValue exception,
                jstring jinstanceid,
                const char* func);
static bool
ExecuteJavaScript(JSContext* cx,
                  JNIEnv* env,
                  jstring source,
                  bool report_exceptions);
static void
makeIdleNotification();
static void
resetIdleNotificationCount();

static void
jString2Log(JNIEnv* env, jstring instance, jstring str)
{
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

static void
setJSFVersion(JNIEnv* env, JSContext* cx, HandleObject globalObject)
{
  JSAutoCompartment ac(cx, globalObject);
  RootedValue version(cx);
  if (!JS_CallFunctionName(cx,
                           globalObject,
                           "getJSFMVersion",
                           HandleValueArray::empty(),
                           &version)) {
    LOGE("failed to call getJSFMVersion function");
    return;
  }
  if (JS_IsExceptionPending(cx)) {
    RootedValue exception(cx);
    JS_GetPendingException(cx, &exception);
    ReportException(cx, exception, nullptr, "");
    JS_ClearPendingException(cx);
    return;
  }
  if (!version.isString()) {
    return;
  }
  RootedString info(cx, version.toString());
  char* utf8 = JS_EncodeStringToUTF8(cx, info);

  jmethodID tempMethodId =
    env->GetMethodID(jBridgeClazz, "setJSFrmVersion", "(Ljava/lang/String;)V");
  LOGA("init JSFrm version %s", utf8);
  jstring jversion = env->NewStringUTF(utf8);
  js_free(utf8);
  env->CallVoidMethod(jThis, tempMethodId, jversion);
  env->DeleteLocalRef(jversion);
}

jint
native_execJSService(JNIEnv* env, jobject object, jstring script)
{
  JSObject* globalObject = _globalObject;
  if (script != NULL) {
    if (!ExecuteJavaScript(globalContext, env, script, true)) {
      LOGE("jsLog JNI_Error >>> scriptStr :%s", "");
      return false;
    }
    DrainJobQueue(globalContext);
    return true;
  }
  return false;
}

static void
native_takeHeapSnapshot(JNIEnv* env, jobject object, jstring name)
{
}

static jint
native_initFramework(JNIEnv* env,
                     jobject object,
                     jstring script,
                     jobject params)
{
  if (globalContext)
    return false;
  if (!JS_Init()) {
    LOGE("JS_Init failed");
    return false;
  }
  SetFakeCPUCount(2);
  size_t nurseryBytes = JS::DefaultNurseryBytes;

  /* Use the same parameters as the browser in xpcjsruntime.cpp. */
  JSContext* cx = JS_NewContext(JS::DefaultHeapMaxBytes, nurseryBytes);
  if (!cx) {
    LOGE("new context failed");
    return false;
  }
  globalContext = cx;
  JS::SetWarningReporter(cx, WarningReporter);

  JS::ContextOptionsRef(cx)
    .setBaseline(true)
    .setIon(true)
    .setAsmJS(true)
    .setWasm(true)
    .setWasmAlwaysBaseline(true)
    .setNativeRegExp(true)
    .setUnboxedArrays(true);
  JS_SetGCParameter(cx, JSGC_MAX_BYTES, 0xffffffff);

  // size_t availMem = 1024 * 1024 * 1024;
  // JS_SetGCParametersBasedOnAvailableMemory(cx, availMem);
  JS::SetBuildIdOp(cx, BuildId);
  if (!JS::InitSelfHostedCode(cx)) {
    LOGE("failed to init self host code");
    return false;
  }
  UniquePtr<ShellContext> sc_ = MakeUnique<ShellContext>(cx);
  if (!sc_)
    return 1;
  JS_SetContextPrivate(cx, sc_.release());

  ShellContext* sc = GetShellContext(cx);
#if defined(SPIDERMONKEY_PROMISE)
  sc->jobQueue.init(cx, JobQueue(SystemAllocPolicy()));
  JS::SetEnqueuePromiseJobCallback(cx, ShellEnqueuePromiseJobCallback);
  JS::SetGetIncumbentGlobalCallback(cx, ShellGetIncumbentGlobalCallback);
  JS::SetAsyncTaskCallbacks(
    cx, ShellStartAsyncTaskCallback, ShellFinishAsyncTaskCallback);
#endif // SPIDERMONKEY_PROMISE
  JS::SetLargeAllocationFailureCallback(
    cx, my_LargeAllocFailCallback, (void*)cx);
  JSAutoRequest ar(cx);
  JS::CompartmentOptions options;
  SetStandardCompartmentOptions(options);
  RootedObject glob(
    cx,
    JS_NewGlobalObject(
      cx, &global_class, nullptr, JS::DontFireOnNewGlobalHook, options));
  if (!glob)
    return false;
  if (!initGlobalObject(env, params, cx, glob))
    return false;
  jThis = env->NewGlobalRef(object);
  _globalObject = glob;
  int result;

  using base::debug::TraceEvent;
  TraceEvent::StartATrace(env);
  base::debug::TraceScope traceScope("weex", "initFramework");
  resetIdleNotificationCount();
  if (script != NULL) {
    if (!ExecuteJavaScript(cx, env, script, true)) {
      return false;
    }

    setJSFVersion(env, cx, glob);
  }
  DrainJobQueue(globalContext);
  return true;
}

/**
 * Called to execute JavaScript such as . createInstance(),destroyInstance ext.
 *
 */
jint
native_execJS(JNIEnv* env,
              jobject jthis,
              jstring jinstanceid,
              jstring jnamespace,
              jstring jfunction,
              jobjectArray jargs)
{
  if (jfunction == NULL || jinstanceid == NULL) {
    LOGE("native_execJS function is NULL");
    return false;
  }

  int length = 0;
  if (jargs != NULL) {
    length = env->GetArrayLength(jargs);
  }
  JSContext* cx = globalContext;
  JSAutoCompartment ac(cx, _globalObject);
  RootedObject globalObject(cx, _globalObject);
  JS::AutoValueVector argv(cx);

  for (int i = 0; i < length; i++) {
    jobject jArg = env->GetObjectArrayElement(jargs, i);

    jfieldID jTypeId = env->GetFieldID(jWXJSObject, "type", "I");
    jint jTypeInt = env->GetIntField(jArg, jTypeId);

    jfieldID jDataId =
      env->GetFieldID(jWXJSObject, "data", "Ljava/lang/Object;");
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
      argv.append(DoubleValue((double)jDoubleObj));

    } else if (jTypeInt == 2) {
      jstring jDataStr = (jstring)jDataObj;
      argv.append(StringValue(jString2JSValue(env, cx, jDataStr)));
    } else if (jTypeInt == 3) {
      RootedValue returnedValue(cx);
      bool okay = parseToObject(env, cx, (jstring)jDataObj, &returnedValue);
      if (!okay)
        argv.append(UndefinedValue());
      else
        argv.append(returnedValue);
      if (!okay && JS_IsExceptionPending(cx)) {
        RootedValue exception(cx);
        JS_GetPendingException(cx, &exception);
        ScopedJStringUTF8 scopedStr(env, (jstring)jDataObj);
        ReportException(cx, exception, jinstanceid, scopedStr.getChars());
        JS_ClearPendingException(cx);
        env->DeleteLocalRef(jDataObj);
        env->DeleteLocalRef(jArg);
        return false;
      }
    }
    env->DeleteLocalRef(jDataObj);
    env->DeleteLocalRef(jArg);
  }

  ScopedJStringUTF8 func(env, jfunction);
  base::debug::TraceScope traceScope(
    "weex", "exeJS", "function", func.getChars());

  RootedValue result(cx);
  if (jnamespace == NULL) {
    base::debug::TraceScope traceScope("weex", "CallFunctionName normal");
    JS_CallFunctionName(cx, globalObject, func.getChars(), argv, &result);
  } else {
    base::debug::TraceScope traceScope("weex", "CallFunctionName namespace");
    RootedValue masterValue(cx);
    ScopedJStringUTF8 _namespace(env, jnamespace);
    JS_GetProperty(cx, globalObject, _namespace.getChars(), &masterValue);
    if (!masterValue.isObject()) {
      LOGE("namespace is not a js object");
      abort();
    }
    RootedObject masterObject(cx, &masterValue.toObject());

    JS_CallFunctionName(cx, masterObject, func.getChars(), argv, &result);
  }
  bool rval = true;
  if (JS_IsExceptionPending(cx)) {
    RootedValue exception(cx);
    JS_GetPendingException(cx, &exception);
    ReportException(cx, exception, jinstanceid, func.getChars());
    JS_ClearPendingException(cx);
    rval = false;
  }
  {
    base::debug::TraceScope traceScope("weex", "DrainJobQueue");
    DrainJobQueue(cx);
  }
  {
    base::debug::TraceScope traceScope("weex", "makeIdleNotification");
    makeIdleNotification();
  }

  return rval;
}

/**
 * this function is to execute a section of JavaScript content.
 */
bool
ExecuteJavaScript(JSContext* cx,
                  JNIEnv* env,
                  jstring source,
                  bool report_exceptions)
{
  JSAutoCompartment ac(cx, _globalObject);
  JS::AutoSaveExceptionState scopedExceptionState(cx);
  CompileOptions option(cx);
  ScopedJString scopedScriptSource(env, source);
  const uint16_t* str = scopedScriptSource.getChars();
  size_t strLen = scopedScriptSource.getCharsLength();
  RootedValue returnValue(cx);
  bool evaluted =
    Evaluate(cx, option, (const char16_t*)str, strLen, &returnValue);
  if (report_exceptions && JS_IsExceptionPending(cx)) {
    RootedValue exception(cx);
    JS_GetPendingException(cx, &exception);
    ReportException(cx, exception, nullptr, "");
  }
  return evaluted;
}

static void
reportException(jstring jInstanceId,
                const char* func,
                const char* exception_string)
{
  JNIEnv* env = getJNIEnv();
  jstring jExceptionString = env->NewStringUTF(exception_string);
  jstring jFunc = env->NewStringUTF(func);
  jmethodID tempMethodId = env->GetMethodID(
    jBridgeClazz,
    "reportJSException",
    "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
  env->CallVoidMethod(
    jThis, tempMethodId, jInstanceId, jFunc, jExceptionString);
  env->DeleteLocalRef(jExceptionString);
  env->DeleteLocalRef(jFunc);
}

/**
 *  This Function will be called when any javascript Exception
 *  that need to print log to notify  native happened.
 */
static void
ReportException(JSContext* cx,
                HandleValue exception,
                jstring jinstanceid,
                const char* func)
{
  if (exception.isObject()) {
    RootedObject exceptionObject(cx, &exception.toObject());
    JSErrorReport* ep = JS_ErrorFromException(cx, exceptionObject);
    LOGE(" ReportException : %s", ep->message().c_str());
    reportException(jinstanceid, func, ep->message().c_str());
  } else {
    LOGE(" ReportException : unknown");
    reportException(jinstanceid, func, "unknown");
  }
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
bool
needIdleNotification()
{
  if (!idle_notification_FLAG) {
    return false;
  }

  if (execJSCount == MAX_EXECJS_COUNT)
    execJSCount = MIN_EXECJS_COUNT;

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

void
makeIdleNotification()
{
  if (!needIdleNotification()) {
    return;
  }
  JS_GC(globalContext);
}

static void
markupStateInternally()
{
  if (execJSCount + samplingBegin == MAX_EXECJS_COUNT) {
    execJSCount = MIN_EXECJS_COUNT;
  }
  lastExecJSCount = execJSCount;
  idle_notification_FLAG = true;
  samplingExecJSCount = 0;
}

// It's not necessary to make idle notifications every time
// of invoking init_framework.
void
resetIdleNotificationCount()
{
  execJSCount = 0;
  idle_notification_FLAG = true;
}

////////////////////////////////////////////
static const char* gBridgeClassPathName = "com/taobao/weex/bridge/WXBridge";
static JNINativeMethod gMethods[] = {
  { "initFramework",
    "(Ljava/lang/String;Lcom/taobao/weex/bridge/WXParams;)I",
    (void*)native_initFramework },
  { "execJS",
    "(Ljava/lang/String;Ljava/lang/String;"
    "Ljava/lang/String;[Lcom/taobao/weex/bridge/WXJSObject;)I",
    (void*)native_execJS },
  { "takeHeapSnapshot",
    "(Ljava/lang/String;)V",
    (void*)native_takeHeapSnapshot },
  { "execJSService", "(Ljava/lang/String;)I", (void*)native_execJSService }
};

static int
registerNativeMethods(JNIEnv* env,
                      const char* className,
                      JNINativeMethod* methods,
                      int numMethods)
{
  if (jBridgeClazz == NULL) {
    LOGE("registerNativeMethods failed to find class '%s'", className);
    return JNI_FALSE;
  }
  if ((env)->RegisterNatives(jBridgeClazz, methods, numMethods) < 0) {
    LOGE(
      "registerNativeMethods failed to register native methods for class '%s'",
      className);
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

static int
registerNatives(JNIEnv* env)
{
  return registerNativeMethods(env,
                               gBridgeClassPathName,
                               gMethods,
                               sizeof(gMethods) / sizeof(gMethods[0]));
}

/**
 * This function will be call when the library first be load.
 * You can do some init in the lib. return which version jni it support.
 */

jint
JNI_OnLoad(JavaVM* vm, void* reserved)
{
  LOGD("begin JNI_OnLoad");
  JNIEnv* env;
  /* Get environment */
  if ((vm)->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK) {
    return JNI_FALSE;
  }

  sVm = vm;
  jclass tempClass = env->FindClass("com/taobao/weex/bridge/WXBridge");
  jBridgeClazz = (jclass)env->NewGlobalRef(tempClass);

  tempClass = env->FindClass("com/taobao/weex/bridge/WXJSObject");
  jWXJSObject = (jclass)env->NewGlobalRef(tempClass);

  tempClass = env->FindClass("com/taobao/weex/utils/WXLogUtils");
  jWXLogUtils = (jclass)env->NewGlobalRef(tempClass);

  env->DeleteLocalRef(tempClass);
  if (registerNatives(env) != JNI_TRUE) {
    return JNI_FALSE;
  }

  LOGD("end JNI_OnLoad");
  return JNI_VERSION_1_4;
}

void
JNI_OnUnload(JavaVM* vm, void* reserved)
{
  LOGD("beigin JNI_OnUnload");
  JNIEnv* env;

  /* Get environment */
  if ((vm)->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK) {
    return;
  }
  env->DeleteGlobalRef(jBridgeClazz);
  env->DeleteGlobalRef(jWXJSObject);
  env->DeleteGlobalRef(jWXLogUtils);
  env->DeleteGlobalRef(jThis);
  if (globalContext)
    delete GetShellContext(globalContext);
  JS_ShutDown();
  using base::debug::TraceEvent;
  TraceEvent::StopATrace(env);
  LOGD(" end JNI_OnUnload");
}
