export NDK_ROOT=`dirname $(which ndk-build)`
echo $NDK_ROOT
export APILEVEL=21
export BINARY_PATH=`dirname $(find $NDK_ROOT/toolchains $NDK_ROOT/build  -name 'arm*' -name '*-g++' | sort  -r| head -n 1)`/../arm-linux-androideabi/bin
HOST=$(uname -s)-$(uname -m)
export HOST=$(echo $HOST | tr '[:upper:]' '[:lower:]')
export TOOLCHAIN_PATH=$NDK_ROOT/toolchains/llvm/prebuilt/$HOST/bin
TARGET_ARCH=ARM
export ION_DEFINITION="XP_UNIX=1 ENABLE_ION=1 ENABLE_SIMD=1 ENABLE_SHARED_ARRAY_BUFFER=1 JS_CODEGEN_${TARGET_ARCH}=1 JS_TRACE_LOGGING=1 SPIDERMONKEY_PROMISE=1 JS_DEFAULT_JITREPORT_GRANULARITY=3 JS_POSIX_NSPR=1 JS_CPU_${TARGET_ARCH}=1 MOZILLA_VERSION=\"52.0.1\""
mkdir -p build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTARGET_ARCH=${TARGET_ARCH} .. && cmake --build .
