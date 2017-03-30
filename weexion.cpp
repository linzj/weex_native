/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2017 Apple Inc. All rights reserved.
 *  Copyright (C) 2006 Bjoern Graf (bjoern.graf@gmail.com)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */
#include "config.h"

#include "ArrayBuffer.h"
#include "ArrayPrototype.h"
#include "BuiltinExecutableCreator.h"
#include "BuiltinNames.h"
#include "ButterflyInlines.h"
#include "CodeBlock.h"
#include "Completion.h"
#include "ConfigFile.h"
#include "DOMJITGetterSetter.h"
#include "DOMJITPatchpoint.h"
#include "DOMJITPatchpointParams.h"
#include "Disassembler.h"
#include "Exception.h"
#include "ExceptionHelpers.h"
#include "GetterSetter.h"
#include "HeapProfiler.h"
#include "HeapSnapshotBuilder.h"
#include "InitializeThreading.h"
#include "Interpreter.h"
#include "JIT.h"
#include "JSArray.h"
#include "JSArrayBuffer.h"
#include "JSCInlines.h"
#include "JSFunction.h"
#include "JSInternalPromise.h"
#include "JSInternalPromiseDeferred.h"
#include "JSLock.h"
#include "JSModuleLoader.h"
#include "JSNativeStdFunction.h"
#include "JSONObject.h"
#include "JSProxy.h"
#include "JSSourceCode.h"
#include "JSString.h"
#include "JSTypedArrays.h"
#include "JSWebAssemblyCallee.h"
#include "LLIntData.h"
#include "LLIntThunks.h"
#include "ObjectConstructor.h"
#include "ParserError.h"
#include "ProfilerDatabase.h"
#include "ProtoCallFrame.h"
#include "ReleaseHeapAccessScope.h"
#include "SamplingProfiler.h"
#include "ShadowChicken.h"
#include "StackVisitor.h"
#include "StrongInlines.h"
#include "StructureInlines.h"
#include "StructureRareDataInlines.h"
#include "SuperSampler.h"
#include "TestRunnerUtils.h"
#include "TypeProfilerLog.h"
#include "WasmFaultSignalHandler.h"
#include "WasmMemory.h"
#include "WasmPlan.h"
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <type_traits>
#include <wtf/CommaPrinter.h>
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StringPrintStream.h>
#include <wtf/text/StringBuilder.h>

#if OS(WINDOWS)
#include <direct.h>
#else
#include <unistd.h>
#endif

#if HAVE(READLINE)
// readline/history.h has a Function typedef which conflicts with the WTF::Function template from WTF/Forward.h
// We #define it to something else to avoid this conflict.
#define Function ReadlineFunction
#include <readline/history.h>
#include <readline/readline.h>
#undef Function
#endif

#if HAVE(SYS_TIME_H)
#include <sys/time.h>
#endif

#if HAVE(SIGNAL_H)
#include <signal.h>
#endif

#if COMPILER(MSVC)
#include <crtdbg.h>
#include <mmsystem.h>
#include <windows.h>
#endif

#if PLATFORM(IOS) && CPU(ARM_THUMB2)
#include <arm/arch.h>
#include <fenv.h>
#endif

#if !defined(PATH_MAX)
#define PATH_MAX 4096
#endif

using namespace JSC;
using namespace WTF;
#include "LogUtils.h"
#include "Trace.h"
#include <jni.h>

static void markupStateInternally();
namespace {

static EncodedJSValue JSC_HOST_CALL functionGCAndSweep(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionCallNative(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionCallNativeModule(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionCallNativeComponent(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionCallAddElement(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionSetTimeoutNative(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionNativeLog(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionNotifyTrimMemory(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionMarkupState(ExecState*);
static JNIEnv* getJNIEnv();

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

class GlobalObject : public JSGlobalObject {
private:
    GlobalObject(VM&, Structure*);

public:
    typedef JSGlobalObject Base;

    static GlobalObject* create(VM& vm, Structure* structure)
    {
        GlobalObject* object = new (NotNull, allocateCell<GlobalObject>(vm.heap)) GlobalObject(vm, structure);
        object->finishCreation(vm);
        return object;
    }

    static const bool needsDestruction = false;

    DECLARE_INFO;
    static const GlobalObjectMethodTable s_globalObjectMethodTable;

    static Structure* createStructure(VM& vm, JSValue prototype)
    {
        return Structure::create(vm, 0, prototype, TypeInfo(GlobalObjectType, StructureFlags), info());
    }

    static RuntimeFlags javaScriptRuntimeFlags(const JSGlobalObject*) { return RuntimeFlags::createAllEnabled(); }

    void initWXEnvironment(JNIEnv* env, jobject params);
    void initFunction();

protected:
    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
        addFunction(vm, "gc", functionGCAndSweep, 0);
    }

    void addFunction(VM& vm, JSObject* object, const char* name, NativeFunction function, unsigned arguments)
    {
        Identifier identifier = Identifier::fromString(&vm, name);
        object->putDirect(vm, identifier, JSFunction::create(vm, this, arguments, identifier.string(), function));
    }

    void addFunction(VM& vm, const char* name, NativeFunction function, unsigned arguments)
    {
        addFunction(vm, this, name, function, arguments);
    }

    void addConstructableFunction(VM& vm, const char* name, NativeFunction function, unsigned arguments)
    {
        Identifier identifier = Identifier::fromString(&vm, name);
        putDirect(vm, identifier, JSFunction::create(vm, this, arguments, identifier.string(), function, NoIntrinsic, function));
    }

    void addString(VM& vm, JSObject* object, const char* name, String&& value)
    {
        Identifier identifier = Identifier::fromString(&vm, name);
        JSString* jsString = jsNontrivialString(&vm, WTFMove(value));
        object->putDirect(vm, identifier, jsString);
    }

    void addValue(VM& vm, JSObject* object, const char* name, JSValue value)
    {
        Identifier identifier = Identifier::fromString(&vm, name);
        object->putDirect(vm, identifier, value);
    }

    void addValue(VM& vm, const char* name, JSValue value)
    {
        addValue(vm, this, name, value);
    }
};

class SimpleObject : public JSNonFinalObject {
public:
    SimpleObject(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    typedef JSNonFinalObject Base;
    static const bool needsDestruction = false;

    static SimpleObject* create(VM& vm, JSGlobalObject* globalObject)
    {
        Structure* structure = createStructure(vm, globalObject, jsNull());
        SimpleObject* simpleObject = new (NotNull, allocateCell<SimpleObject>(vm.heap, sizeof(SimpleObject))) SimpleObject(vm, structure);
        simpleObject->finishCreation(vm);
        return simpleObject;
    }

    static void visitChildren(JSCell* cell, SlotVisitor& visitor)
    {
        SimpleObject* thisObject = jsCast<SimpleObject*>(cell);
        ASSERT_GC_OBJECT_INHERITS(thisObject, info());
        Base::visitChildren(thisObject, visitor);
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    DECLARE_INFO;
};

const ClassInfo SimpleObject::s_info = { "SimpleObject", &Base::s_info, nullptr, CREATE_METHOD_TABLE(SimpleObject) };

class ScopedJString {
public:
    ScopedJString(JNIEnv* env, jstring);
    ~ScopedJString();
    const char* getChars();

private:
    JNIEnv* m_env;
    jstring m_jstring;
    const char* m_chars;
};

const ClassInfo GlobalObject::s_info = { "global", &JSGlobalObject::s_info, nullptr, CREATE_METHOD_TABLE(GlobalObject) };
const GlobalObjectMethodTable GlobalObject::s_globalObjectMethodTable = {
    &supportsRichSourceInfo,
    &shouldInterruptScript,
    &javaScriptRuntimeFlags,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

GlobalObject::GlobalObject(VM& vm, Structure* structure)
    : JSGlobalObject(vm, structure, &s_globalObjectMethodTable)
{
}

ScopedJString::ScopedJString(JNIEnv* env, jstring _jstring)
    : m_env(env)
    , m_jstring(_jstring)
    , m_chars(nullptr)
{
}

ScopedJString::~ScopedJString()
{
    if (m_chars)
        m_env->ReleaseStringUTFChars(m_jstring, m_chars);
}

const char* ScopedJString::getChars()
{
    if (m_chars)
        return m_chars;
    m_chars = m_env->GetStringUTFChars(m_jstring, nullptr);
    return m_chars;
}

String jString2String(JNIEnv* env, jstring str)
{
    if (str != NULL) {
        ScopedJString scopedstr(env, str);
        const char* c_str = scopedstr.getChars();
        return String::fromUTF8(c_str);
    }
    return String("");
}

static JSValue jString2JSValue(JNIEnv* env, ExecState* state, jstring str)
{
    String s = jString2String(env, str);
    if (s.isEmpty()) {
        return jsEmptyString(&state->vm());
    } else if (s.length() == 1) {
        return jsSingleCharacterString(&state->vm(), s[0]);
    }
    return jsNontrivialString(&state->vm(), WTFMove(s));
}

static jstring getArgumentAsJString(JNIEnv* env, ExecState* state, int argument)
{
    jstring ret = nullptr;
    JSValue val = state->argument(argument);
    if (!val.isUndefined()) {
        String s = val.toWTFString(state);
        ret = env->NewStringUTF(s.utf8().data());
    }
    return ret;
}

static jbyteArray getArgumentAsJByteArrayJSON(JNIEnv* env, ExecState* state, int argument)
{
    jbyteArray ba = nullptr;
    JSValue val = state->argument(argument);
    VM& vm = state->vm();
    if (val.isObject()) {
        String str = JSONStringify(state, val, 0);
        JSC::VM& vm = state->vm();
        auto scope = DECLARE_CATCH_SCOPE(vm);
        if (UNLIKELY(scope.exception())) {
            scope.clearException();
            return nullptr;
        }
        CString strData = str.utf8();
        int strLen = strData.length();
        ba = env->NewByteArray(strLen);
        env->SetByteArrayRegion(ba, 0, strLen,
            reinterpret_cast<const jbyte*>(strData.data()));
    }
    return ba;
}

static JSValue parseToObject(ExecState* state, JSValue val, NakedPtr<Exception>& returnedException)
{
    JSGlobalObject* globalObject = state->lexicalGlobalObject();
    VM& vm = state->vm();
    PropertyName json(Identifier::fromString(&vm, "JSON"));
    PropertyName parse(Identifier::fromString(&vm, "parse"));
    JSValue jsonObject = globalObject->get(state, json);
    JSValue parseFunction = jsonObject.toObject(state)->get(state, parse);
    MarkedArgumentBuffer args;
    args.append(val);
    CallData callData;
    CallType callType = getCallData(parseFunction, callData);
    JSValue ret = call(globalObject->globalExec(), parseFunction, callType, callData, globalObject, args, returnedException);
    return ret;
}

static jbyteArray getArgumentAsJByteArray(JNIEnv* env, ExecState* state, int argument)
{
    jbyteArray ba = nullptr;
    JSValue val = state->argument(argument);
    if (val.isString()) {
        String str(val.toWTFString(state));
        CString strData = str.utf8();
        int strLen = strData.length();
        ba = env->NewByteArray(strLen);
        env->SetByteArrayRegion(ba, 0, strLen,
            reinterpret_cast<const jbyte*>(strData.data()));
    } else {
        ba = getArgumentAsJByteArrayJSON(env, state, argument);
    }
    return ba;
}

EncodedJSValue JSC_HOST_CALL functionGCAndSweep(ExecState* exec)
{
    JSLockHolder lock(exec);
    exec->heap()->collectAllGarbage();
    return JSValue::encode(jsNumber(exec->heap()->sizeAfterLastFullCollection()));
}

EncodedJSValue JSC_HOST_CALL functionCallNative(ExecState* state)
{
    base::debug::TraceScope traceScope("weex", "callNative");

    JNIEnv* env = getJNIEnv();
    VM& vm = state->vm();
    //instacneID args[0]
    jstring jInstanceId = getArgumentAsJString(env, state, 0);
    //task args[1]
    jbyteArray jTaskString = getArgumentAsJByteArray(env, state, 1);
    //callback args[2]
    jstring jCallback = getArgumentAsJString(env, state, 2);

    if (jCallNativeMethodId == NULL) {
        jCallNativeMethodId = env->GetMethodID(jBridgeClazz,
            "callNative",
            "(Ljava/lang/String;[BLjava/lang/String;)I");
    }

    int flag = env->CallIntMethod(jThis, jCallNativeMethodId, jInstanceId, jTaskString, jCallback);
    if (flag == -1) {
        LOGE("instance destroy JFM must stop callNative");
    }
    env->DeleteLocalRef(jTaskString);
    env->DeleteLocalRef(jInstanceId);
    env->DeleteLocalRef(jCallback);

    return JSValue::encode(jsNumber(flag));
}

EncodedJSValue JSC_HOST_CALL functionCallNativeModule(ExecState* state)
{
    base::debug::TraceScope traceScope("weex", "callNativeModule");
    VM& vm = state->vm();

    JNIEnv* env = getJNIEnv();
    //instacneID args[0]
    jstring jInstanceId = getArgumentAsJString(env, state, 0);

    //module args[1]
    jstring jmodule = getArgumentAsJString(env, state, 1);

    //method args[2]
    jstring jmethod = getArgumentAsJString(env, state, 2);

    // arguments args[3]
    jbyteArray jArgString = getArgumentAsJByteArrayJSON(env, state, 3);
    //arguments args[4]
    jbyteArray jOptString = getArgumentAsJByteArrayJSON(env, state, 4);

    if (jCallNativeModuleMethodId == NULL) {
        jCallNativeModuleMethodId = env->GetMethodID(jBridgeClazz,
            "callNativeModule",
            "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[B[B)Ljava/lang/Object;");
    }

    jobject result = env->CallObjectMethod(jThis, jCallNativeModuleMethodId, jInstanceId, jmodule, jmethod, jArgString, jOptString);
    JSValue ret;

    jfieldID jTypeId = env->GetFieldID(jWXJSObject, "type", "I");
    jint jTypeInt = env->GetIntField(result, jTypeId);
    jfieldID jDataId = env->GetFieldID(jWXJSObject, "data", "Ljava/lang/Object;");
    jobject jDataObj = env->GetObjectField(result, jDataId);
    if (jTypeInt == 1) {
        if (jDoubleValueMethodId == NULL) {
            jclass jDoubleClazz = env->FindClass("java/lang/Double");
            jDoubleValueMethodId = env->GetMethodID(jDoubleClazz, "doubleValue", "()D");
            env->DeleteLocalRef(jDoubleClazz);
        }
        jdouble jDoubleObj = env->CallDoubleMethod(jDataObj, jDoubleValueMethodId);
        ret = jsNumber((double)jDoubleObj);

    } else if (jTypeInt == 2) {
        jstring jDataStr = (jstring)jDataObj;
        ret = jString2JSValue(env, state, jDataStr);
    } else if (jTypeInt == 3) {
        JSValue val = jString2JSValue(env, state, (jstring)jDataObj);
        NakedPtr<Exception> returnedException;
        ret = parseToObject(state, val, returnedException);
        if (returnedException) {
            // TODO: print exception
        }
    }
    env->DeleteLocalRef(jDataObj);
    env->DeleteLocalRef(jInstanceId);
    env->DeleteLocalRef(jmodule);
    env->DeleteLocalRef(jmethod);
    env->DeleteLocalRef(jArgString);
    env->DeleteLocalRef(jOptString);
    return JSValue::encode(ret);
}

EncodedJSValue JSC_HOST_CALL functionCallNativeComponent(ExecState* state)
{
    base::debug::TraceScope traceScope("weex", "callNativeComponent");
    JNIEnv* env = getJNIEnv();

    //instacneID args[0]
    jstring jInstanceId = getArgumentAsJString(env, state, 0);

    //module args[1]
    jstring jcomponentRef = getArgumentAsJString(env, state, 1);

    //method args[2]
    jstring jmethod = getArgumentAsJString(env, state, 2);

    // arguments args[3]
    jbyteArray jArgString = getArgumentAsJByteArrayJSON(env, state, 3);

    //arguments args[4]
    jbyteArray jOptString = getArgumentAsJByteArrayJSON(env, state, 4);

    if (jCallNativeComponentMethodId == NULL) {
        jCallNativeComponentMethodId = env->GetMethodID(jBridgeClazz,
            "callNativeComponent",
            "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[B[B)V");
    }

    env->CallVoidMethod(jThis, jCallNativeComponentMethodId, jInstanceId, jcomponentRef, jmethod, jArgString, jOptString);

    env->DeleteLocalRef(jInstanceId);
    env->DeleteLocalRef(jcomponentRef);
    env->DeleteLocalRef(jmethod);
    env->DeleteLocalRef(jArgString);
    env->DeleteLocalRef(jOptString);
    return JSValue::encode(jsBoolean(true));
}

EncodedJSValue JSC_HOST_CALL functionCallAddElement(ExecState* state)
{
    base::debug::TraceScope traceScope("weex", "callAddElement");
    JNIEnv* env = getJNIEnv();
    //instacneID args[0]
    jstring jInstanceId = getArgumentAsJString(env, state, 0);
    //instacneID args[1]
    jstring jref = getArgumentAsJString(env, state, 1);
    //dom node args[2]
    jbyteArray jdomString = getArgumentAsJByteArray(env, state, 2);
    //index  args[3]
    jstring jindex = getArgumentAsJString(env, state, 3);
    //callback  args[4]
    jstring jCallback = getArgumentAsJString(env, state, 4);
    if (jCallAddElementMethodId == NULL) {
        jCallAddElementMethodId = env->GetMethodID(jBridgeClazz,
            "callAddElement",
            "(Ljava/lang/String;Ljava/lang/String;[BLjava/lang/String;Ljava/lang/String;)I");
    }

    int flag = env->CallIntMethod(jThis, jCallAddElementMethodId, jInstanceId, jref, jdomString, jindex,
        jCallback);
    if (flag == -1) {
        LOGE("instance destroy JFM must stop callNative");
    }
    env->DeleteLocalRef(jInstanceId);
    env->DeleteLocalRef(jref);
    env->DeleteLocalRef(jdomString);
    env->DeleteLocalRef(jindex);
    env->DeleteLocalRef(jCallback);
    return JSValue::encode(jsNumber(flag));
}

EncodedJSValue JSC_HOST_CALL functionSetTimeoutNative(ExecState* state)
{
    base::debug::TraceScope traceScope("weex", "setTimeoutNative");
    JNIEnv* env = getJNIEnv();
    //callbackId
    jstring jCallbackID = getArgumentAsJString(env, state, 0);

    //time
    jstring jTime = getArgumentAsJString(env, state, 1);

    if (jSetTimeoutNativeMethodId == NULL) {
        jSetTimeoutNativeMethodId = env->GetMethodID(jBridgeClazz,
            "setTimeoutNative",
            "(Ljava/lang/String;Ljava/lang/String;)V");
    }
    env->CallVoidMethod(jThis, jSetTimeoutNativeMethodId, jCallbackID, jTime);
    env->DeleteLocalRef(jCallbackID);
    env->DeleteLocalRef(jTime);
    return JSValue::encode(jsBoolean(true));
}

EncodedJSValue JSC_HOST_CALL functionNativeLog(ExecState* state)
{
    JNIEnv* env;
    bool result = false;

    StringBuilder sb;
    for (int i = 0; i < state->argumentCount(); i++) {
        sb.append(state->argument(i).toWTFString(state));
    }

    if (!sb.isEmpty()) {
        env = getJNIEnv();
        String s = sb.toString();
        jstring str_msg = env->NewStringUTF(s.utf8().data());
        if (jWXLogUtils != NULL) {
            if (jLogMethodId == NULL) {
                jLogMethodId = env->GetStaticMethodID(jWXLogUtils, "d", "(Ljava/lang/String;Ljava/lang/String;)V");
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
    }
    return JSValue::encode(jsBoolean(true));
}

EncodedJSValue JSC_HOST_CALL functionNotifyTrimMemory(ExecState* state)
{
    return functionGCAndSweep(state);
}

EncodedJSValue JSC_HOST_CALL functionMarkupState(ExecState* state)
{
    markupStateInternally();
    return JSValue::encode(jsUndefined());
}

// Use SEH for Release builds only to get rid of the crash report dialog
// (luckily the same tests fail in Release and Debug builds so far). Need to
// be in a separate main function because the jscmain function requires object
// unwinding.

#if COMPILER(MSVC) && !defined(_DEBUG)
#define TRY __try {
#define EXCEPT(x) \
    }             \
    __except (EXCEPTION_EXECUTE_HANDLER) { x; }
#else
#define TRY
#define EXCEPT(x)
#endif

static String exceptionToString(JSGlobalObject* globalObject, JSValue exception)
{
    StringBuilder sb;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

#define CHECK_EXCEPTION()           \
    do {                            \
        if (scope.exception()) {    \
            scope.clearException(); \
            return String();        \
        }                           \
    } while (false)

    sb.append(String::format("Exception: %s\n", exception.toWTFString(globalObject->globalExec()).utf8().data()));

    Identifier nameID = Identifier::fromString(globalObject->globalExec(), "name");
    CHECK_EXCEPTION();
    Identifier fileNameID = Identifier::fromString(globalObject->globalExec(), "sourceURL");
    CHECK_EXCEPTION();
    Identifier lineNumberID = Identifier::fromString(globalObject->globalExec(), "line");
    CHECK_EXCEPTION();
    Identifier stackID = Identifier::fromString(globalObject->globalExec(), "stack");
    CHECK_EXCEPTION();

    JSValue nameValue = exception.get(globalObject->globalExec(), nameID);
    CHECK_EXCEPTION();
    JSValue fileNameValue = exception.get(globalObject->globalExec(), fileNameID);
    CHECK_EXCEPTION();
    JSValue lineNumberValue = exception.get(globalObject->globalExec(), lineNumberID);
    CHECK_EXCEPTION();
    JSValue stackValue = exception.get(globalObject->globalExec(), stackID);
    CHECK_EXCEPTION();

    if (nameValue.toWTFString(globalObject->globalExec()) == "SyntaxError"
        && (!fileNameValue.isUndefinedOrNull() || !lineNumberValue.isUndefinedOrNull())) {
        sb.append(String::format(
            "at %s:%s\n",
            fileNameValue.toWTFString(globalObject->globalExec()).utf8().data(),
            lineNumberValue.toWTFString(globalObject->globalExec()).utf8().data()));
    }

    if (!stackValue.isUndefinedOrNull())
        sb.append(String::format("%s\n", stackValue.toWTFString(globalObject->globalExec()).utf8().data()));

#undef CHECK_EXCEPTION
    return sb.toString();
}

void GlobalObject::initWXEnvironment(JNIEnv* env, jobject params)
{
    VM& vm = this->vm();
    JSNonFinalObject* WXEnvironment = SimpleObject::create(vm, this);
#define ADDSTRING(name) \
    addString(vm, WXEnvironment, #name, jString2String(env, (jstring)name))

    jclass c_params = env->GetObjectClass(params);

    jmethodID m_platform = env->GetMethodID(c_params, "getPlatform", "()Ljava/lang/String;");
    jobject platform = env->CallObjectMethod(params, m_platform);
    ADDSTRING(platform);
    env->DeleteLocalRef(platform);

    jmethodID m_osVersion = env->GetMethodID(c_params, "getOsVersion", "()Ljava/lang/String;");
    jobject osVersion = env->CallObjectMethod(params, m_osVersion);
    ADDSTRING(osVersion);
    env->DeleteLocalRef(osVersion);

    jmethodID m_appVersion = env->GetMethodID(c_params, "getAppVersion", "()Ljava/lang/String;");
    jobject appVersion = env->CallObjectMethod(params, m_appVersion);
    ADDSTRING(appVersion);
    env->DeleteLocalRef(appVersion);

    jmethodID m_weexVersion = env->GetMethodID(c_params, "getWeexVersion", "()Ljava/lang/String;");
    jobject weexVersion = env->CallObjectMethod(params, m_weexVersion);
    ADDSTRING(weexVersion);
    env->DeleteLocalRef(weexVersion);

    jmethodID m_deviceModel = env->GetMethodID(c_params, "getDeviceModel", "()Ljava/lang/String;");
    jobject deviceModel = env->CallObjectMethod(params, m_deviceModel);
    ADDSTRING(deviceModel);
    env->DeleteLocalRef(deviceModel);

    jmethodID m_appName = env->GetMethodID(c_params, "getAppName", "()Ljava/lang/String;");
    jobject appName = env->CallObjectMethod(params, m_appName);
    ADDSTRING(appName);
    env->DeleteLocalRef(appName);

    jmethodID m_deviceWidth = env->GetMethodID(c_params, "getDeviceWidth", "()Ljava/lang/String;");
    jobject deviceWidth = env->CallObjectMethod(params, m_deviceWidth);
    ADDSTRING(deviceWidth);
    env->DeleteLocalRef(deviceWidth);

    jmethodID m_deviceHeight = env->GetMethodID(c_params, "getDeviceHeight",
        "()Ljava/lang/String;");
    jobject deviceHeight = env->CallObjectMethod(params, m_deviceHeight);
    ADDSTRING(deviceHeight);
    env->DeleteLocalRef(deviceHeight);

    jmethodID m_options = env->GetMethodID(c_params, "getOptions", "()Ljava/lang/Object;");
    jobject options = env->CallObjectMethod(params, m_options);
    jclass jmapclass = env->FindClass("java/util/HashMap");
    jmethodID jkeysetmid = env->GetMethodID(jmapclass, "keySet", "()Ljava/util/Set;");
    jmethodID jgetmid = env->GetMethodID(jmapclass, "get", "(Ljava/lang/Object;)Ljava/lang/Object;");
    jobject jsetkey = env->CallObjectMethod(options, jkeysetmid);
    jclass jsetclass = env->FindClass("java/util/Set");
    jmethodID jtoArraymid = env->GetMethodID(jsetclass, "toArray", "()[Ljava/lang/Object;");
    jobjectArray jobjArray = (jobjectArray)env->CallObjectMethod(jsetkey, jtoArraymid);
    env->DeleteLocalRef(jsetkey);
    if (jobjArray != NULL) {
        jsize arraysize = env->GetArrayLength(jobjArray);
        for (int i = 0; i < arraysize; i++) {
            jstring jkey = (jstring)env->GetObjectArrayElement(jobjArray, i);
            jstring jvalue = (jstring)env->CallObjectMethod(options, jgetmid, jkey);
            if (jkey != NULL) {
                const char* c_key = env->GetStringUTFChars(jkey, NULL);
                addString(vm, WXEnvironment, c_key, jString2String(env, jvalue));
                env->DeleteLocalRef(jkey);
                if (jvalue != NULL) {
                    env->DeleteLocalRef(jvalue);
                }
            }
        }
        env->DeleteLocalRef(jobjArray);
    }
    env->DeleteLocalRef(options);
    addValue(vm, "WXEnvironment", WXEnvironment);
}

void GlobalObject::initFunction()
{
    VM& vm = this->vm();
    const HashTableValue JSEventTargetPrototypeTableValues[] = {
        { "callNative", JSC::Function, NoIntrinsic, { (intptr_t) static_cast<NativeFunction>(functionCallNative), (intptr_t)(3) } },
        { "callNativeModule", JSC::Function, NoIntrinsic, { (intptr_t) static_cast<NativeFunction>(functionCallNativeModule), (intptr_t)(5) } },
        { "callNativeComponent", JSC::Function, NoIntrinsic, { (intptr_t) static_cast<NativeFunction>(functionCallNativeComponent), (intptr_t)(5) } },
        { "callAddElement", JSC::Function, NoIntrinsic, { (intptr_t) static_cast<NativeFunction>(functionCallAddElement), (intptr_t)(5) } },
        { "setTimeoutNative", JSC::Function, NoIntrinsic, { (intptr_t) static_cast<NativeFunction>(functionSetTimeoutNative), (intptr_t)(2) } },
        { "nativeLog", JSC::Function, NoIntrinsic, { (intptr_t) static_cast<NativeFunction>(functionNativeLog), (intptr_t)(5) } },
        { "notifyTrimMemory", JSC::Function, NoIntrinsic, { (intptr_t) static_cast<NativeFunction>(functionNotifyTrimMemory), (intptr_t)(0) } },
        { "markupState", JSC::Function, NoIntrinsic, { (intptr_t) static_cast<NativeFunction>(functionMarkupState), (intptr_t)(0) } },
    };
    reifyStaticProperties(vm, JSEventTargetPrototypeTableValues, *this);
}

JNIEnv* getJNIEnv()
{
    JNIEnv* env = NULL;
    if ((sVm)->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK) {
        return JNI_FALSE;
    }
    return env;
}
}

static RefPtr<VM> globalVM;
static Strong<JSGlobalObject> _globalObject;

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

long getCPUTime()
{
    struct timespec ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static void ReportException(JSGlobalObject* globalObject, JSValue exception, jstring jinstanceid,
    const char* func);
static bool ExecuteJavaScript(JSGlobalObject* globalObject,
    const String& source,
    bool report_exceptions);
static void makeIdleNotification(JSGlobalObject* globalObject);
static void resetIdleNotificationCount();

void jString2Log(JNIEnv* env, jstring instance, jstring str)
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

void setJSFVersion(JNIEnv* env, JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    PropertyName getJSFMVersionProperty(Identifier::fromString(&vm, "getJSFMVersion"));
    ExecState* state = globalObject->globalExec();
    JSValue getJSFMVersionFunction = globalObject->get(state, getJSFMVersionProperty);
    MarkedArgumentBuffer args;
    CallData callData;
    CallType callType = getCallData(getJSFMVersionFunction, callData);
    NakedPtr<Exception> returnedException;
    JSValue version = call(globalObject->globalExec(), getJSFMVersionFunction, callType, callData, globalObject, args, returnedException);
    if (returnedException) {
        ReportException(globalObject, returnedException.get(), nullptr, "");
    }
    String str = version.toWTFString(state);

    jmethodID tempMethodId = env->GetMethodID(jBridgeClazz,
        "setJSFrmVersion",
        "(Ljava/lang/String;)V");
    CString utf8 = str.utf8();
    LOGA("init JSFrm version %s", utf8.data());
    jstring jversion = env->NewStringUTF(utf8.data());
    env->CallVoidMethod(jThis, tempMethodId, jversion);
    env->DeleteLocalRef(jversion);
}

jint native_execJSService(JNIEnv* env,
    jobject object,
    jstring script)
{
    JSGlobalObject* globalObject = _globalObject.get();
    if (script != NULL) {
        ScopedJString scopedJString(env, script);
        const char* scriptStr = scopedJString.getChars();
        String source = String::fromUTF8(scriptStr);
        VM& vm = *globalVM.get();
        JSLockHolder locker(&vm);
        if (scriptStr == NULL || !ExecuteJavaScript(globalObject, source, true)) {
            LOGE("jsLog JNI_Error >>> scriptStr :%s", scriptStr);
            return false;
        }
        return true;
    }
    return false;
}

static void native_takeHeapSnapshot(JNIEnv* env,
    jobject object,
    jstring name)
{
}

static jint native_initFramework(JNIEnv* env,
    jobject object,
    jstring script,
    jobject params)
{
    Options::enableRestrictedOptions(true);

    // Initialize JSC before getting VM.
    WTF::initializeMainThread();
    JSC::initializeThreading();
#if ENABLE(WEBASSEMBLY)
    JSC::Wasm::enableFastMemory();
#endif

    globalVM = VM::create(LargeHeap);
    VM& vm = *globalVM.get();
    JSLockHolder locker(&vm);

    int result;

    GlobalObject* globalObject = GlobalObject::create(vm, GlobalObject::createStructure(vm, jsNull()));
    _globalObject.set(vm, globalObject);
    jThis = env->NewGlobalRef(object);

    using base::debug::TraceEvent;
    TraceEvent::StartATrace(env);
    base::debug::TraceScope traceScope("weex", "initFramework");
    globalObject->initWXEnvironment(env, params);
    globalObject->initFunction();
    resetIdleNotificationCount();
    if (script != NULL) {
        ScopedJString scopedJString(env, script);
        const char* scriptStr = scopedJString.getChars();
        String source = String::fromUTF8(scriptStr);
        if (scriptStr == NULL || !ExecuteJavaScript(globalObject, source, true)) {
            return false;
        }

        setJSFVersion(env, globalObject);
    }
    return true;
}

/**
 * Called to execute JavaScript such as . createInstance(),destroyInstance ext.
 *
 */
jint native_execJS(JNIEnv* env,
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
    MarkedArgumentBuffer obj;
    JSGlobalObject* globalObject = _globalObject.get();
    VM& vm = *globalVM.get();
    JSLockHolder locker(&vm);
    ExecState* state = globalObject->globalExec();

    for (int i = 0; i < length; i++) {
        jobject jArg = env->GetObjectArrayElement(jargs, i);

        jfieldID jTypeId = env->GetFieldID(jWXJSObject, "type", "I");
        jint jTypeInt = env->GetIntField(jArg, jTypeId);

        jfieldID jDataId = env->GetFieldID(jWXJSObject, "data", "Ljava/lang/Object;");
        jobject jDataObj = env->GetObjectField(jArg, jDataId);
        if (jTypeInt == 1) {
            if (jDoubleValueMethodId == NULL) {
                jclass jDoubleClazz = env->FindClass("java/lang/Double");
                jDoubleValueMethodId = env->GetMethodID(jDoubleClazz, "doubleValue", "()D");
                env->DeleteLocalRef(jDoubleClazz);
            }
            jdouble jDoubleObj = env->CallDoubleMethod(jDataObj, jDoubleValueMethodId);
            obj.append(jsNumber((double)jDoubleObj));

        } else if (jTypeInt == 2) {
            jstring jDataStr = (jstring)jDataObj;
            obj.append(jString2JSValue(env, state, jDataStr));
        } else if (jTypeInt == 3) {
            JSValue jsonObj;
            jsonObj = jString2JSValue(env, state, (jstring)jDataObj);
            NakedPtr<Exception> returnedException;
            JSValue o = parseToObject(state, jsonObj, returnedException);
            obj.append(o);
            if (returnedException) {
                String s = jsonObj.toWTFString(state);
                ReportException(globalObject, returnedException.get(), jinstanceid, s.utf8().data());
                env->DeleteLocalRef(jDataObj);
                env->DeleteLocalRef(jArg);
                return false;
            }
        }
        env->DeleteLocalRef(jDataObj);
        env->DeleteLocalRef(jArg);
    }

    String func = jString2String(env, jfunction);
    base::debug::TraceScope traceScope("weex", "exeJS", "function", func.utf8().data());
    Identifier funcIdentifier = Identifier::fromString(&vm, func);

    JSValue function;
    JSValue result;
    if (jnamespace == NULL) {
        function = globalObject->get(state, funcIdentifier);
    } else {
        String _namespace = jString2String(env, jnamespace);
        Identifier namespaceIdentifier = Identifier::fromString(&vm, _namespace);
        JSValue master = globalObject->get(state, namespaceIdentifier);
        function = master.toObject(state)->get(state, funcIdentifier);
    }
    CallData callData;
    CallType callType = getCallData(function, callData);
    NakedPtr<Exception> returnedException;
    JSValue ret = call(state, function, callType, callData, globalObject, obj, returnedException);

    makeIdleNotification(globalObject);

    if (returnedException) {
        ReportException(globalObject, returnedException.get(), jinstanceid, func.utf8().data());
        return false;
    }
    return true;
}

/**
 * this function is to execute a section of JavaScript content.
 */
bool ExecuteJavaScript(JSGlobalObject* globalObject,
    const String& source,
    bool report_exceptions)
{
    SourceOrigin sourceOrigin(String::fromUTF8("(weex)"));
    NakedPtr<Exception> evaluationException;
    JSValue returnValue = evaluate(globalObject->globalExec(), makeSource(source, sourceOrigin), JSValue(), evaluationException);
    if (report_exceptions && evaluationException) {
        ReportException(globalObject, evaluationException.get(), nullptr, "");
    }
    if (evaluationException)
        return false;
    return true;
}

void reportException(jstring jInstanceId, const char* func, const char* exception_string)
{
    JNIEnv* env = getJNIEnv();
    jstring jExceptionString = env->NewStringUTF(exception_string);
    jstring jFunc = env->NewStringUTF(func);
    jmethodID tempMethodId = env->GetMethodID(jBridgeClazz,
        "reportJSException",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    env->CallVoidMethod(jThis, tempMethodId, jInstanceId, jFunc, jExceptionString);
    env->DeleteLocalRef(jExceptionString);
    env->DeleteLocalRef(jFunc);
}

/**
 *  This Function will be called when any javascript Exception
 *  that need to print log to notify  native happened.
 */
static void ReportException(JSGlobalObject* globalObject, JSValue exception, jstring jinstanceid,
    const char* func)
{
    String exceptionInfo = exceptionToString(globalObject, exception);
    LOGE(" ReportException : %s", exceptionInfo.utf8().data());
    reportException(jinstanceid, func, exceptionInfo.utf8().data());
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
bool needIdleNotification()
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

void makeIdleNotification(JSGlobalObject* globalObject)
{
    if (!needIdleNotification()) {
        return;
    }
    functionGCAndSweep(globalObject->globalExec());
}

static void markupStateInternally()
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
void resetIdleNotificationCount()
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
    { "execJSService",
        "(Ljava/lang/String;)I",
        (void*)native_execJSService }
};

static int registerNativeMethods(JNIEnv* env,
    const char* className,
    JNINativeMethod* methods,
    int numMethods)
{
    if (jBridgeClazz == NULL) {
        LOGE("registerNativeMethods failed to find class '%s'", className);
        return JNI_FALSE;
    }
    if ((env)->RegisterNatives(jBridgeClazz, methods, numMethods) < 0) {
        LOGE("registerNativeMethods failed to register native methods for class '%s'",
            className);
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

static int registerNatives(JNIEnv* env)
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

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    LOGD("begin JNI_OnLoad");
    JNIEnv* env;
    /* Get environment */
    if ((vm)->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK) {
        return JNI_FALSE;
    }

    sVm = vm;
    jclass tempClass = env->FindClass(
        "com/taobao/weex/bridge/WXBridge");
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

void JNI_OnUnload(JavaVM* vm, void* reserved)
{
    LOGD("beigin JNI_OnUnload");
    JNIEnv* env;
    _globalObject.clear();

    globalVM.release();
    /* Get environment */
    if ((vm)->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK) {
        return;
    }
    env->DeleteGlobalRef(jBridgeClazz);
    env->DeleteGlobalRef(jWXJSObject);
    env->DeleteGlobalRef(jWXLogUtils);
    env->DeleteGlobalRef(jThis);

    using base::debug::TraceEvent;
    TraceEvent::StopATrace(env);
    LOGD(" end JNI_OnUnload");
}
