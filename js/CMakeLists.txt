cmake_minimum_required(VERSION 2.8.12)
set(ION_SOURCES
    src/builtin/AtomicsObject.cpp
    src/builtin/Eval.cpp
    src/builtin/Intl.cpp
    src/builtin/MapObject.cpp
    src/builtin/ModuleObject.cpp
    src/builtin/Object.cpp
    src/builtin/Profilers.cpp
    src/builtin/Promise.cpp
    src/builtin/Reflect.cpp
    src/builtin/ReflectParse.cpp
    src/builtin/SIMD.cpp
    src/builtin/SymbolObject.cpp
    src/builtin/TestingFunctions.cpp
    src/builtin/TypedObject.cpp
    src/builtin/WeakMapObject.cpp
    src/builtin/WeakSetObject.cpp
    src/devtools/sharkctl.cpp
    src/ds/LifoAlloc.cpp
    src/ds/MemoryProtectionExceptionHandler.cpp
    src/frontend/BytecodeCompiler.cpp
    src/frontend/BytecodeEmitter.cpp
    src/frontend/FoldConstants.cpp
    src/frontend/NameFunctions.cpp
    src/frontend/ParseNode.cpp
    src/frontend/TokenStream.cpp
    src/gc/Allocator.cpp
    src/gc/Barrier.cpp
    src/gc/GCTrace.cpp
    src/gc/Iteration.cpp
    src/gc/Marking.cpp
    src/gc/Memory.cpp
    src/gc/MemoryProfiler.cpp
    src/gc/Nursery.cpp
    src/gc/RootMarking.cpp
    src/gc/Statistics.cpp
    src/gc/Tracer.cpp
    src/gc/Verifier.cpp
    src/gc/Zone.cpp
    src/irregexp/NativeRegExpMacroAssembler.cpp
    src/irregexp/RegExpAST.cpp
    src/irregexp/RegExpEngine.cpp
    src/irregexp/RegExpInterpreter.cpp
    src/irregexp/RegExpMacroAssembler.cpp
    src/irregexp/RegExpParser.cpp
    src/irregexp/RegExpStack.cpp
    src/jit/AliasAnalysis.cpp
    src/jit/AliasAnalysisShared.cpp
    src/jit/AlignmentMaskAnalysis.cpp
    src/jit/BacktrackingAllocator.cpp
    src/jit/Bailouts.cpp
    src/jit/BaselineBailouts.cpp
    src/jit/BaselineCacheIR.cpp
    src/jit/BaselineCompiler.cpp
    src/jit/BaselineDebugModeOSR.cpp
    src/jit/BaselineFrame.cpp
    src/jit/BaselineFrameInfo.cpp
    src/jit/BaselineIC.cpp
    src/jit/BaselineInspector.cpp
    src/jit/BaselineJIT.cpp
    src/jit/BitSet.cpp
    src/jit/BytecodeAnalysis.cpp
    src/jit/C1Spewer.cpp
    src/jit/CacheIR.cpp
    src/jit/CodeGenerator.cpp
    src/jit/CompileWrappers.cpp
    src/jit/Disassembler.cpp
    src/jit/EagerSimdUnbox.cpp
    src/jit/EdgeCaseAnalysis.cpp
    src/jit/EffectiveAddressAnalysis.cpp
    src/jit/ExecutableAllocator.cpp
    src/jit/FlowAliasAnalysis.cpp
    src/jit/FoldLinearArithConstants.cpp
    src/jit/InstructionReordering.cpp
    src/jit/Ion.cpp
    src/jit/IonAnalysis.cpp
    src/jit/IonBuilder.cpp
    src/jit/IonCaches.cpp
    src/jit/IonOptimizationLevels.cpp
    src/jit/JitcodeMap.cpp
    src/jit/JitFrames.cpp
    src/jit/JitOptions.cpp
    src/jit/JitSpewer.cpp
    src/jit/JSONSpewer.cpp
    src/jit/LICM.cpp
    src/jit/Linker.cpp
    src/jit/LIR.cpp
    src/jit/LoopUnroller.cpp
    src/jit/Lowering.cpp
    src/jit/MacroAssembler.cpp
    src/jit/MCallOptimize.cpp
    src/jit/MIR.cpp
    src/jit/MIRGraph.cpp
    src/jit/MoveResolver.cpp
    src/jit/OptimizationTracking.cpp
    src/jit/PerfSpewer.cpp
    src/jit/ProcessExecutableMemory.cpp
    src/jit/RangeAnalysis.cpp
    src/jit/Recover.cpp
    src/jit/RegisterAllocator.cpp
    src/jit/RematerializedFrame.cpp
    src/jit/Safepoints.cpp
    src/jit/ScalarReplacement.cpp
    src/jit/shared/BaselineCompiler-shared.cpp
    src/jit/shared/CodeGenerator-shared.cpp
    src/jit/shared/Lowering-shared.cpp
    src/jit/SharedIC.cpp
    src/jit/Sink.cpp
    src/jit/Snapshots.cpp
    src/jit/StupidAllocator.cpp
    src/jit/TypedObjectPrediction.cpp
    src/jit/TypePolicy.cpp
    src/jit/ValueNumbering.cpp
    src/jit/VMFunctions.cpp
    src/jit/WasmBCE.cpp
    src/jsalloc.cpp
    src/jsapi.cpp
    src/jsbool.cpp
    src/jscntxt.cpp
    src/jscompartment.cpp
    src/jsdate.cpp
    src/jsexn.cpp
    src/jsfriendapi.cpp
    src/jsfun.cpp
    src/jsgc.cpp
    src/jsiter.cpp
    src/jsnativestack.cpp
    src/jsnum.cpp
    src/jsobj.cpp
    src/json.cpp
    src/jsopcode.cpp
    src/jsprf.cpp
    src/jspropertytree.cpp
    src/jsscript.cpp
    src/jsstr.cpp
    src/jswatchpoint.cpp
    src/jsweakmap.cpp
    src/perf/jsperf.cpp
    src/proxy/BaseProxyHandler.cpp
    src/proxy/CrossCompartmentWrapper.cpp
    src/proxy/DeadObjectProxy.cpp
    src/proxy/OpaqueCrossCompartmentWrapper.cpp
    src/proxy/Proxy.cpp
    src/proxy/ScriptedProxyHandler.cpp
    src/proxy/SecurityWrapper.cpp
    src/proxy/Wrapper.cpp
    src/threading/Mutex.cpp
    src/vm/ArgumentsObject.cpp
    src/vm/ArrayBufferObject.cpp
    src/vm/AsyncFunction.cpp
    src/vm/Caches.cpp
    src/vm/CallNonGenericMethod.cpp
    src/vm/CharacterEncoding.cpp
    src/vm/CodeCoverage.cpp
    src/vm/Compression.cpp
    src/vm/DateTime.cpp
    src/vm/Debugger.cpp
    src/vm/DebuggerMemory.cpp
    src/vm/EnvironmentObject.cpp
    src/vm/ErrorObject.cpp
    src/vm/ForOfIterator.cpp
    src/vm/GeneratorObject.cpp
    src/vm/GlobalObject.cpp
    src/vm/HelperThreads.cpp
    src/vm/Id.cpp
    src/vm/Interpreter.cpp
    src/vm/JSONParser.cpp
    src/vm/MemoryMetrics.cpp
    src/vm/NativeObject.cpp
    src/vm/ObjectGroup.cpp
    src/vm/PIC.cpp
    src/vm/Printer.cpp
    src/vm/Probes.cpp
    src/vm/ProxyObject.cpp
    src/vm/Realm.cpp
    src/vm/ReceiverGuard.cpp
    src/vm/RegExpObject.cpp
    src/vm/RegExpStatics.cpp
    src/vm/Runtime.cpp
    src/vm/SavedStacks.cpp
    src/vm/Scope.cpp
    src/vm/SelfHosting.cpp
    src/vm/Shape.cpp
    src/vm/SharedArrayObject.cpp
    src/vm/SharedImmutableStringsCache.cpp
    src/vm/SPSProfiler.cpp
    src/vm/Stack.cpp
    src/vm/Stopwatch.cpp
    src/vm/String.cpp
    src/vm/StringBuffer.cpp
    src/vm/StructuredClone.cpp
    src/vm/Symbol.cpp
    src/vm/TaggedProto.cpp
    src/vm/Time.cpp
    src/vm/TypedArrayObject.cpp
    src/vm/TypeInference.cpp
    src/vm/UbiNode.cpp
    src/vm/UbiNodeCensus.cpp
    src/vm/UbiNodeShortestPaths.cpp
    src/vm/UnboxedObject.cpp
    src/vm/Unicode.cpp
    src/vm/Value.cpp
    src/vm/WeakMapPtr.cpp
    src/vm/Xdr.cpp
    src/wasm/AsmJS.cpp
    src/wasm/WasmBaselineCompile.cpp
    src/wasm/WasmBinaryFormat.cpp
    src/wasm/WasmBinaryIterator.cpp
    src/wasm/WasmBinaryToAST.cpp
    src/wasm/WasmBinaryToExperimentalText.cpp
    src/wasm/WasmBinaryToText.cpp
    src/wasm/WasmCode.cpp
    src/wasm/WasmCompartment.cpp
    src/wasm/WasmCompile.cpp
    src/wasm/WasmFrameIterator.cpp
    src/wasm/WasmGenerator.cpp
    src/wasm/WasmInstance.cpp
    src/wasm/WasmIonCompile.cpp
    src/wasm/WasmJS.cpp
    src/wasm/WasmModule.cpp
    src/wasm/WasmSignalHandlers.cpp
    src/wasm/WasmStubs.cpp
    src/wasm/WasmTable.cpp
    src/wasm/WasmTextToBinary.cpp
    src/wasm/WasmTextUtils.cpp
    src/wasm/WasmTypes.cpp
    src/builtin/RegExp.cpp
    src/frontend/Parser.cpp
    src/gc/StoreBuffer.cpp
    src/jsarray.cpp
    src/jsatom.cpp
    src/jsdtoa.cpp
    src/jsmath.cpp
    src/jsutil.cpp
    src/vm/Initialization.cpp
    src/vm/TraceLogging.cpp
    src/vm/TraceLoggingGraph.cpp
    src/vm/TraceLoggingTypes.cpp
    src/threading/posix/ConditionVariable.cpp
    src/threading/posix/MutexImpl.cpp
    src/threading/posix/Thread.cpp
    src/perf/pm_stub.cpp
    src/vm/PosixNSPR.cpp
)
set(ION_ARM_SOURCES
    src/jit/arm/Architecture-arm.cpp
    src/jit/arm/Assembler-arm.cpp
    src/jit/arm/Bailouts-arm.cpp
    src/jit/arm/BaselineCompiler-arm.cpp
    src/jit/arm/BaselineIC-arm.cpp
    src/jit/arm/CodeGenerator-arm.cpp
    src/jit/arm/disasm/Constants-arm.cpp
    src/jit/arm/disasm/Disasm-arm.cpp
    src/jit/arm/Lowering-arm.cpp
    src/jit/arm/MacroAssembler-arm.cpp
    src/jit/arm/MoveEmitter-arm.cpp
    src/jit/arm/SharedIC-arm.cpp
    src/jit/arm/Trampoline-arm.cpp
)

set(ION_PRIVATE_DEFINITIONS
#EXPORT_JS_API=1
)

set(JSAUTOKW_INPUTS
${CMAKE_CURRENT_SOURCE_DIR}/src/vm/Keywords.h
)
add_custom_command(
    OUTPUT ${ION_INCLUDES}/jsautokw.h
    MAIN_DEPENDENCY ${JSAUTOKW_INPUTS}
    DEPENDS src/jsautokw.py
    COMMAND python ./js/src/jsautokw.py ${JSAUTOKW_INPUTS} > ${ION_INCLUDES}/jsautokw.h
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    VERBATIM)

set(SELFHOST_OUTPUT
${ION_INCLUDES}/selfhosted.out.h
)

set(SELFHOST_INPUT
    ${CMAKE_CURRENT_SOURCE_DIR}/src/js.msg
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/TypedObjectConstants.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/SelfHostingDefines.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/Utilities.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/Array.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/Classes.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/Date.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/Error.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/Function.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/Generator.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/Intl.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/IntlData.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/Iterator.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/Map.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/Module.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/Number.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/Object.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/Reflect.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/RegExp.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/RegExpGlobalReplaceOpt.h.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/RegExpLocalReplaceOpt.h.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/String.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/Set.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/Sorting.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/TypedArray.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/TypedObject.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/WeakMap.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/WeakSet.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/Promise.js
)

add_custom_command(
    OUTPUT ${SELFHOST_OUTPUT}
    MAIN_DEPENDENCY ${SELFHOST_INPUT}
    DEPENDS src/builtin/embedjs.py
            ${CMAKE_SOURCE_DIR}/pyaction.py
    COMMAND PYTHONPATH=${CMAKE_SOURCE_DIR}/js/src/builtin python pyaction.py generate_selfhosted ${SELFHOST_OUTPUT} ${SELFHOST_INPUT}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    VERBATIM)

add_library(js
STATIC
${ION_SOURCES}
${ION_${TARGET_ARCH}_SOURCES}
${SELFHOST_OUTPUT}
${ION_INCLUDES}/jsautokw.h)

target_compile_definitions(js
PRIVATE
${ION_PRIVATE_DEFINITIONS})

target_include_directories(js
PUBLIC
src)

target_compile_options(js
PRIVATE
-Wno-invalid-offsetof)

target_link_libraries(js
z
mfbt
fdlibm
misc)

#set(SHELLMODULELOADER_OUTPUT
#${ION_INCLUDES}/shellmoduleloader.out.h)
#
#set(SHELLMODULELOADER_INPUT
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/js.msg
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/shell/ModuleLoader.js
#)
#
#set(SHELL_SOURCES
#    src/shell/js.cpp
#    src/shell/jsoptparse.cpp
#    src/shell/jsshell.cpp
#    src/shell/OSObject.cpp
#    ${SHELLMODULELOADER_OUTPUT}
#)
#
#add_custom_command(
#    OUTPUT ${SHELLMODULELOADER_OUTPUT}
#    MAIN_DEPENDENCY ${SHELLMODULELOADER_INPUT}
#    DEPENDS src/builtin/embedjs.py
#            ${CMAKE_SOURCE_DIR}/pyaction.py
#    COMMAND PYTHONPATH=${CMAKE_SOURCE_DIR}/js/src/builtin python pyaction.py generate_shellmoduleloader ${SHELLMODULELOADER_OUTPUT} ${SHELLMODULELOADER_INPUT}
#    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
#    VERBATIM)
#
#add_executable(ionshell
#${SHELL_SOURCES})
#
#target_include_directories(ionshell
#PRIVATE
#src)
#
#target_link_libraries(ionshell
#js)
#
#target_compile_options(ionshell
#PRIVATE
#-Wno-invalid-offsetof)
#
#add_custom_command(
#    TARGET ionshell
#    POST_BUILD
#    COMMAND ${CMAKE_STRIP} $<TARGET_FILE:ionshell> -o ${CMAKE_SOURCE_DIR}/ionshell
#    VERBATIM
#)
