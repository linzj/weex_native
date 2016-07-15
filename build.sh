export TARGET_ARCH=armeabi-v7a
cd v8 && ./build.sh
cd ../ && mkdir -p aliout/armeabi-v7a
cd aliout/armeabi-v7a/ && arm-linux-androideabi-strip ../../v8/out/android_arm.release/obj.target/alisrc/libweexcore.so -o libweexcore.so
arm-linux-androideabi-strip ../../v8/out/android_arm.release/buildcache -o buildcache
