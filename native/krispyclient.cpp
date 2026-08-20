#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#define LOG_TAG "KrispyClientNative"

#define LOGI(...) \
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#define LOGE(...) \
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static ANativeWindow* g_window = nullptr;

static EGLDisplay g_display = EGL_NO_DISPLAY;
static EGLSurface g_surface = EGL_NO_SURFACE;
static EGLContext g_context = EGL_NO_CONTEXT;

static int g_width = 0;
static int g_height = 0;

static bool g_initialized = false;

static bool initEgl() {

    if (g_initialized) {
        return true;
    }

    if (g_window == nullptr) {
        LOGE("initEgl: ANativeWindow is null");
        return false;
    }

    g_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    if (g_display == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay failed");
        return false;
    }

    EGLint major = 0;
    EGLint minor = 0;

    if (!eglInitialize(g_display, &major, &minor)) {
        LOGE(
            "eglInitialize failed: 0x%04x",
            eglGetError()
        );

        g_display = EGL_NO_DISPLAY;
        return false;
    }

    LOGI(
        "EGL initialized: %d.%d",
        major,
        minor
    );

    const EGLint configAttributes[] = {
        EGL_RENDERABLE_TYPE,
        EGL_OPENGL_ES3_BIT,

        EGL_SURFACE_TYPE,
        EGL_WINDOW_BIT,

        EGL_RED_SIZE,
        8,

        EGL_GREEN_SIZE,
        8,

        EGL_BLUE_SIZE,
        8,

        EGL_ALPHA_SIZE,
        8,

        EGL_DEPTH_SIZE,
        24,

        EGL_NONE
    };

    EGLConfig config = nullptr;
    EGLint numConfigs = 0;

    if (!eglChooseConfig(
            g_display,
            configAttributes,
            &config,
            1,
            &numConfigs
        ) || numConfigs == 0) {

        LOGE(
            "eglChooseConfig failed: 0x%04x",
            eglGetError()
        );

        eglTerminate(g_display);
        g_display = EGL_NO_DISPLAY;

        return false;
    }

    const EGLint contextAttributes[] = {
        EGL_CONTEXT_CLIENT_VERSION,
        3,
        EGL_NONE
    };

    g_context = eglCreateContext(
        g_display,
        config,
        EGL_NO_CONTEXT,
        contextAttributes
    );

    if (g_context == EGL_NO_CONTEXT) {

        LOGE(
            "eglCreateContext failed: 0x%04x",
            eglGetError()
        );

        eglTerminate(g_display);
        g_display = EGL_NO_DISPLAY;

        return false;
    }

    g_surface = eglCreateWindowSurface(
        g_display,
        config,
        g_window,
        nullptr
    );

    if (g_surface == EGL_NO_SURFACE) {

        LOGE(
            "eglCreateWindowSurface failed: 0x%04x",
            eglGetError()
        );

        eglDestroyContext(
            g_display,
            g_context
        );

        g_context = EGL_NO_CONTEXT;

        eglTerminate(g_display);
        g_display = EGL_NO_DISPLAY;

        return false;
    }

    if (!eglMakeCurrent(
            g_display,
            g_surface,
            g_surface,
            g_context
        )) {

        LOGE(
            "eglMakeCurrent failed: 0x%04x",
            eglGetError()
        );

        eglDestroySurface(
            g_display,
            g_surface
        );

        eglDestroyContext(
            g_display,
            g_context
        );

        g_surface = EGL_NO_SURFACE;
        g_context = EGL_NO_CONTEXT;

        eglTerminate(g_display);
        g_display = EGL_NO_DISPLAY;

        return false;
    }

    g_initialized = true;

    LOGI("KrispyClient EGL initialized successfully");

    return true;
}

static void destroyEgl() {

    if (g_display == EGL_NO_DISPLAY) {
        g_initialized = false;
        return;
    }

    eglMakeCurrent(
        g_display,
        EGL_NO_SURFACE,
        EGL_NO_SURFACE,
        EGL_NO_CONTEXT
    );

    if (g_surface != EGL_NO_SURFACE) {
        eglDestroySurface(
            g_display,
            g_surface
        );
    }

    if (g_context != EGL_NO_CONTEXT) {
        eglDestroyContext(
            g_display,
            g_context
        );
    }

    eglTerminate(g_display);

    g_display = EGL_NO_DISPLAY;
    g_surface = EGL_NO_SURFACE;
    g_context = EGL_NO_CONTEXT;

    g_initialized = false;

    LOGI("KrispyClient EGL destroyed");
}

static void renderFrame() {

    if (!g_initialized) {
        return;
    }

    glViewport(
        0,
        0,
        g_width,
        g_height
    );

    glClearColor(
        0.03f,
        0.03f,
        0.03f,
        1.0f
    );

    glClear(GL_COLOR_BUFFER_BIT);

    if (!eglSwapBuffers(
            g_display,
            g_surface
        )) {

        LOGE(
            "eglSwapBuffers failed: 0x%04x",
            eglGetError()
        );
    }
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_krispyclient_launcher_NativeBridge_getNativeVersion(
    JNIEnv* env,
    jobject
) {
    return env->NewStringUTF(
        "KrispyClient Native Core 0.3"
    );
}

extern "C"
JNIEXPORT void JNICALL
Java_com_krispyclient_launcher_NativeBridge_nativeSurfaceCreated(
    JNIEnv* env,
    jobject,
    jobject surface
) {

    LOGI("nativeSurfaceCreated called");

    if (surface == nullptr) {
        LOGE("nativeSurfaceCreated: surface is null");
        return;
    }

    if (g_initialized) {
        destroyEgl();
    }

    if (g_window != nullptr) {
        ANativeWindow_release(g_window);
        g_window = nullptr;
    }

    g_window = ANativeWindow_fromSurface(
        env,
        surface
    );

    if (g_window == nullptr) {
        LOGE("ANativeWindow_fromSurface failed");
        return;
    }

    LOGI("ANativeWindow acquired");

    if (!initEgl()) {
        LOGE("EGL initialization failed");

        ANativeWindow_release(g_window);
        g_window = nullptr;

        return;
    }

    renderFrame();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_krispyclient_launcher_NativeBridge_nativeSurfaceChanged(
    JNIEnv*,
    jobject,
    jint width,
    jint height
) {

    g_width = static_cast<int>(width);
    g_height = static_cast<int>(height);

    LOGI(
        "nativeSurfaceChanged: %dx%d",
        g_width,
        g_height
    );

    renderFrame();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_krispyclient_launcher_NativeBridge_nativeSurfaceDestroyed(
    JNIEnv*,
    jobject
) {

    LOGI("nativeSurfaceDestroyed called");

    destroyEgl();

    if (g_window != nullptr) {
        ANativeWindow_release(g_window);
        g_window = nullptr;
    }

    LOGI("KrispyClient surface destroyed");
}

extern "C"
JNIEXPORT void JNICALL
Java_com_krispyclient_launcher_NativeBridge_nativeResume(
    JNIEnv*,
    jobject
) {
    LOGI("KrispyClient native resume");
}

extern "C"
JNIEXPORT void JNICALL
Java_com_krispyclient_launcher_NativeBridge_nativePause(
    JNIEnv*,
    jobject
) {
    LOGI("KrispyClient native pause");
}

extern "C"
JNIEXPORT void JNICALL
Java_com_krispyclient_launcher_NativeBridge_nativeDestroy(
    JNIEnv*,
    jobject
) {

    LOGI("KrispyClient native destroy");

    destroyEgl();

    if (g_window != nullptr) {
        ANativeWindow_release(g_window);
        g_window = nullptr;
    }

    LOGI("KrispyClient native destroyed");
}
