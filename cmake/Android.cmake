# Toolchain config for Android NDK.
# This is expected to be used with a standalone Android toolchain (see
# docs/STANDALONE-TOOLCHAIN.html in the NDK on how to get one).
#
# Usage:
# mkdir build; cd build
# cmake ..; make
# mkdir android; cd android
# cmake -DLLVM_ANDROID_TOOLCHAIN_DIR=/path/to/android/ndk \
#   -DCMAKE_TOOLCHAIN_FILE=../../cmake/platforms/Android.cmake ../..
# make <target>
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)
SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_TOOLCHAIN_PATH $ENV{TOOLCHAIN_PATH})
SET(CMAKE_BINARY_PATH $ENV{BINARY_PATH})
SET(CMAKE_GCC_PATH $ENV{GCC_PATH})
SET(CMAKE_HOST_NAME $ENV{HOST})
SET(CMAKE_TARGET_NAME $ENV{TARGET})
SET(CMAKE_TARGET_ABI_NAME $ENV{TARGET_ABI})
SET(CMAKE_CLANG_CFLAGS $ENV{CLANG_CFLAGS})
SET(CMAKE_ANDROID_SYSROOT $ENV{NDK_ROOT}/platforms/android-$ENV{APILEVEL}/arch-${CMAKE_TARGET_NAME})
SET(CMAKE_ANDROID_STL_INCLUDE $ENV{NDK_ROOT}/sources/cxx-stl/llvm-libc++/include)
SET(CMAKE_ANDROID_STL_BITS_INCLUDE $ENV{NDK_ROOT}/sources/cxx-stl/llvm-libc++abi/include)
SET(CMAKE_TARGET_WORD_BITS $ENV{TARGET_WORD_BITS})
SET(CMAKE_C_COMPILER clang)
SET(CMAKE_CXX_COMPILER clang++)
SET(CMAKE_FIND_ROOT_PATH ${CMAKE_BINARY_PATH})
SET(CMAKE_AR llvm-ar)
SET(CMAKE_RANLIB llvm-ranlib)
SET(CMAKE_STRIP ${CMAKE_BINARY_PATH}/strip)

SET(ANDROID "1" CACHE STRING "ANDROID" FORCE)

SET(ANDROID_COMMON_FLAGS "${CMAKE_CLANG_CFLAGS} --sysroot=${CMAKE_ANDROID_SYSROOT} -isystem ${CMAKE_ANDROID_STL_INCLUDE} -isystem ${CMAKE_ANDROID_STL_BITS_INCLUDE} -B${CMAKE_BINARY_PATH} -ffunction-sections -fdata-sections -fomit-frame-pointer -gcc-toolchain ${CMAKE_GCC_PATH} -flto -fwhole-program-vtables -mfloat-abi=softfp -mfpu=neon")
SET(CMAKE_C_FLAGS "${ANDROID_COMMON_FLAGS}" CACHE STRING "toolchain_cflags" FORCE)
SET(CMAKE_CXX_FLAGS "${ANDROID_COMMON_FLAGS} -std=gnu++1y -fno-exceptions -fno-rtti" CACHE STRING "toolchain_cxxflags" FORCE)
SET(CMAKE_ASM_FLAGS "${ANDROID_COMMON_FLAGS}" CACHE STRING "toolchain_asmflags" FORCE)
SET(CMAKE_EXE_LINKER_FLAGS "-O2 -pie -fuse-ld=lld" CACHE STRING "toolchain_exelinkflags" FORCE)
SET(CMAKE_SHARED_LINKER_FLAGS "-Wl,--gc-sections -Wl,--build-id=sha1 -fuse-ld=lld -Wl,--icf=all -mfloat-abi=softfp -mfpu=neon -flto -fwhole-program-vtables -Wl,--lto-O2" CACHE STRING "toolchain_exelinkflags" FORCE)
SET(CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS "-shared")
SET(CMAKE_SHARED_LIBRARY_PREFIX "lib")
SET(CMAKE_SHARED_LIBRARY_SUFFIX ".so")
SET(CMAKE_SHARED_LIBRARY_SONAME_C_FLAG "")
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-Os -g -DNDEBUG")
SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-Os -g -DNDEBUG")
link_libraries($ENV{DEFAULT_LIBRARY})
link_directories(
$ENV{NDK_ROOT}/sources/cxx-stl/llvm-libc++/libs/${CMAKE_TARGET_ABI_NAME}
)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

SET(CMAKE_SKIP_BUILD_RPATH  TRUE)

set(PORT JSCOnly)
set(WTF_CPU_$ENV{WTF_CPU} 1)
set(JavaScriptCore_LIBRARY_TYPE STATIC)
