#include <jni.h>

extern "C"
JNIEXPORT jstring JNICALL
Java_com_krispyclient_launcher_NativeBridge_getNativeVersion(
        JNIEnv* env,
        jobject /* thiz */) {

    return env->NewStringUTF(
        "KrispyClient Native v0.1"
    );
}
