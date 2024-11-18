@ECHO OFF
PUSHD "%~dp0"
rmdir /q /s "obj"
rmdir /q /s "lib"
ndk-build NDK_PROJECT_PATH=null NDK_APPLICATION_MK=./Application.mk APP_PLATFORM=android-29 APP_BUILD_SCRIPT=./Android.mk NDK_OUT=./obj NDK_LIBS_OUT=./lib
pause
POPD