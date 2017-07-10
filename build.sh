export TARGET_ARCH=armeabi-v7a
export BUILD_TYPE=release
cd v8 && ./build.sh
cd ../ && mkdir -p aliout/armeabi-v7a
cd aliout/armeabi-v7a/ && arm-linux-androideabi-strip ../../v8/out/android_arm.$BUILD_TYPE/obj.target/alisrc/libweexjsc.so -o libweexjsc.so
#arm-linux-androideabi-strip ../../v8/out/android_arm.$BUILD_TYPE/buildcache -o buildcache
