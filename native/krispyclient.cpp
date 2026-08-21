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

static int g_width = 1;
static int g_height = 1;

static GLuint g_program = 0;
static GLuint g_vao = 0;
static GLuint g_vbo = 0;

static bool g_initialized = false;

/*
 * IMPORTANT:
 * #version MUST be the first character of the shader source.
 * Do not put whitespace, comments, or blank lines before it.
 */

static const char* VERTEX_SHADER =
    "#version 300 es\n"
    "layout(location = 0) in vec2 a_position;\n"
    "\n"
    "void main() {\n"
    "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
    "}\n";

static const char* FRAGMENT_SHADER =
    "#version 300 es\n"
    "precision mediump float;\n"
    "\n"
    "out vec4 outColor;\n"
    "\n"
    "void main() {\n"
    "    outColor = vec4(0.15, 0.55, 1.0, 1.0);\n"
    "}\n";


/*
 * Forward declarations
 */

static void destroyRenderer();
static void destroyEgl();


/*
 * Shader compilation
 */

static GLuint compileShader(
    GLenum type,
    const char* source
) {
    if (source == nullptr) {
        LOGE("Shader source is null");
        return 0;
    }

    GLuint shader = glCreateShader(type);

    if (shader == 0) {
        LOGE(
            "glCreateShader failed: 0x%04x",
            glGetError()
        );

        return 0;
    }

    glShaderSource(
        shader,
        1,
        &source,
        nullptr
    );

    glCompileShader(shader);

    GLint compiled = GL_FALSE;

    glGetShaderiv(
        shader,
        GL_COMPILE_STATUS,
        &compiled
    );

    if (compiled != GL_TRUE) {

        GLint logLength = 0;

        glGetShaderiv(
            shader,
            GL_INFO_LOG_LENGTH,
            &logLength
        );

        if (logLength > 0) {

            char* log = new char[logLength + 1];

            log[0] = '\0';

            glGetShaderInfoLog(
                shader,
                logLength,
                nullptr,
                log
            );

            LOGE(
                "Shader compilation failed:\n%s",
                log
            );

            delete[] log;
        } else {
            LOGE(
                "Shader compilation failed with no info log"
            );
        }

        glDeleteShader(shader);

        return 0;
    }

    LOGI("Shader compiled successfully");

    return shader;
}


/*
 * OpenGL program
 */

static bool createProgram() {

    GLuint vertexShader = compileShader(
        GL_VERTEX_SHADER,
        VERTEX_SHADER
    );

    if (vertexShader == 0) {
        LOGE("Vertex shader creation failed");
        return false;
    }

    GLuint fragmentShader = compileShader(
        GL_FRAGMENT_SHADER,
        FRAGMENT_SHADER
    );

    if (fragmentShader == 0) {

        LOGE("Fragment shader creation failed");

        glDeleteShader(vertexShader);

        return false;
    }

    g_program = glCreateProgram();

    if (g_program == 0) {

        LOGE(
            "glCreateProgram failed: 0x%04x",
            glGetError()
        );

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return false;
    }

    glAttachShader(
        g_program,
        vertexShader
    );

    glAttachShader(
        g_program,
        fragmentShader
    );

    glLinkProgram(g_program);

    GLint linked = GL_FALSE;

    glGetProgramiv(
        g_program,
        GL_LINK_STATUS,
        &linked
    );

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (linked != GL_TRUE) {

        GLint logLength = 0;

        glGetProgramiv(
            g_program,
            GL_INFO_LOG_LENGTH,
            &logLength
        );

        if (logLength > 0) {

            char* log = new char[logLength + 1];

            log[0] = '\0';

            glGetProgramInfoLog(
                g_program,
                logLength,
                nullptr,
                log
            );

            LOGE(
                "Program linking failed:\n%s",
                log
            );

            delete[] log;
        } else {
            LOGE(
                "Program linking failed with no info log"
            );
        }

        glDeleteProgram(g_program);

        g_program = 0;

        return false;
    }

    LOGI("OpenGL shader program linked successfully");

    return true;
}


/*
 * Triangle geometry
 */

static bool createTriangle() {

    const GLfloat vertices[] = {
         0.0f,  0.65f,
        -0.65f, -0.55f,
         0.65f, -0.55f
    };

    glGenVertexArrays(
        1,
        &g_vao
    );

    glGenBuffers(
        1,
        &g_vbo
    );

    if (g_vao == 0 || g_vbo == 0) {

        LOGE(
            "Failed to create VAO/VBO"
        );

        return false;
    }

    glBindVertexArray(g_vao);

    glBindBuffer(
        GL_ARRAY_BUFFER,
        g_vbo
    );

    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(vertices),
        vertices,
        GL_STATIC_DRAW
    );

    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        2 * sizeof(GLfloat),
        nullptr
    );

    glBindBuffer(
        GL_ARRAY_BUFFER,
        0
    );

    glBindVertexArray(0);

    LOGI(
        "Triangle geometry created"
    );

    return true;
}


/*
 * Renderer initialization
 */

static bool initRenderer() {

    if (!createProgram()) {

        LOGE(
            "Failed to create OpenGL program"
        );

        return false;
    }

    if (!createTriangle()) {

        LOGE(
            "Failed to create triangle"
        );

        if (g_program != 0) {

            glDeleteProgram(
                g_program
            );

            g_program = 0;
        }

        return false;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    LOGI(
        "Native renderer initialized"
    );

    return true;
}


/*
 * Renderer destruction
 */

static void destroyRenderer() {

    if (g_vbo != 0) {

        glDeleteBuffers(
            1,
            &g_vbo
        );

        g_vbo = 0;
    }

    if (g_vao != 0) {

        glDeleteVertexArrays(
            1,
            &g_vao
        );

        g_vao = 0;
    }

    if (g_program != 0) {

        glDeleteProgram(
            g_program
        );

        g_program = 0;
    }

    LOGI(
        "Native renderer destroyed"
    );
}


/*
 * EGL initialization
 */

static bool initEgl() {

    if (g_initialized) {
        return true;
    }

    if (g_window == nullptr) {

        LOGE(
            "initEgl: ANativeWindow is null"
        );

        return false;
    }

    g_display = eglGetDisplay(
        EGL_DEFAULT_DISPLAY
    );

    if (g_display == EGL_NO_DISPLAY) {

        LOGE(
            "eglGetDisplay failed: 0x%04x",
            eglGetError()
        );

        return false;
    }

    EGLint major = 0;
    EGLint minor = 0;

    if (!eglInitialize(
            g_display,
            &major,
            &minor
        )) {

        LOGE(
            "eglInitialize failed: 0x%04x",
            eglGetError()
        );

        g_display = EGL_NO_DISPLAY;

        return false;
    }

    LOGI(
        "EGL %d.%d",
        major,
        minor
    );


    /*
     * Request OpenGL ES 3.
     */

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
        ) ||
        numConfigs == 0) {

        LOGE(
            "eglChooseConfig failed: 0x%04x",
            eglGetError()
        );

        eglTerminate(
            g_display
        );

        g_display = EGL_NO_DISPLAY;

        return false;
    }


    /*
     * Create ES 3 context.
     */

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

        eglTerminate(
            g_display
        );

        g_display = EGL_NO_DISPLAY;

        return false;
    }


    /*
     * Create window surface.
     */

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

        eglTerminate(
            g_display
        );

        g_display = EGL_NO_DISPLAY;

        return false;
    }


    /*
     * Make context current.
     */

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

        eglTerminate(
            g_display
        );

        g_display = EGL_NO_DISPLAY;

        return false;
    }


    /*
     * Print GPU information.
     */

    const GLubyte* glVersion =
        glGetString(GL_VERSION);

    const GLubyte* glRenderer =
        glGetString(GL_RENDERER);

    const GLubyte* glslVersion =
        glGetString(GL_SHADING_LANGUAGE_VERSION);

    const GLubyte* glVendor =
        glGetString(GL_VENDOR);

    LOGI(
        "OpenGL ES: %s",
        glVersion
            ? reinterpret_cast<const char*>(glVersion)
            : "unknown"
    );

    LOGI(
        "Renderer: %s",
        glRenderer
            ? reinterpret_cast<const char*>(glRenderer)
            : "unknown"
    );

    LOGI(
        "GLSL: %s",
        glslVersion
            ? reinterpret_cast<const char*>(glslVersion)
            : "unknown"
    );

    LOGI(
        "Vendor: %s",
        glVendor
            ? reinterpret_cast<const char*>(glVendor)
            : "unknown"
    );


    /*
     * Initialize renderer.
     */

    if (!initRenderer()) {

        LOGE(
            "Renderer initialization failed"
        );

        destroyEgl();

        return false;
    }

    g_initialized = true;

    LOGI(
        "KrispyClient EGL + renderer initialized"
    );

    return true;
}


/*
 * EGL destruction
 */

static void destroyEgl() {

    if (g_display == EGL_NO_DISPLAY) {

        g_surface = EGL_NO_SURFACE;
        g_context = EGL_NO_CONTEXT;
        g_initialized = false;

        return;
    }

    /*
     * OpenGL objects must be deleted
     * while the context is still current.
     */

    if (g_context != EGL_NO_CONTEXT) {

        eglMakeCurrent(
            g_display,
            g_surface,
            g_surface,
            g_context
        );

        destroyRenderer();
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

    eglTerminate(
        g_display
    );

    g_display = EGL_NO_DISPLAY;
    g_surface = EGL_NO_SURFACE;
    g_context = EGL_NO_CONTEXT;

    g_initialized = false;

    LOGI(
        "EGL destroyed"
    );
}


/*
 * Render frame
 */

static void renderFrame() {

    if (!g_initialized) {
        return;
    }

    if (g_display == EGL_NO_DISPLAY ||
        g_surface == EGL_NO_SURFACE ||
        g_context == EGL_NO_CONTEXT) {

        return;
    }

    if (!eglMakeCurrent(
            g_display,
            g_surface,
            g_surface,
            g_context
        )) {

        LOGE(
            "eglMakeCurrent during render failed: 0x%04x",
            eglGetError()
        );

        return;
    }

    glViewport(
        0,
        0,
        g_width,
        g_height
    );

    /*
     * Dark background.
     */

    glClearColor(
        0.03f,
        0.03f,
        0.03f,
        1.0f
    );

    glClear(
        GL_COLOR_BUFFER_BIT
    );


    /*
     * Draw blue triangle.
     */

    glUseProgram(
        g_program
    );

    glBindVertexArray(
        g_vao
    );

    glDrawArrays(
        GL_TRIANGLES,
        0,
        3
    );

    glBindVertexArray(0);

    glUseProgram(0);


    /*
     * Present frame.
     */

    if (!eglSwapBuffers(
            g_display,
            g_surface
        )) {

        LOGE(
            "eglSwapBuffers failed: 0x%04x",
            eglGetError()
        );

        return;
    }

    LOGI(
        "Frame rendered successfully"
    );
}


/*
 * JNI: version
 */

extern "C"
JNIEXPORT jstring JNICALL
Java_com_krispyclient_launcher_NativeBridge_getNativeVersion(
    JNIEnv* env,
    jobject
) {

    return env->NewStringUTF(
        "KrispyClient Native Renderer 0.5"
    );
}


/*
 * JNI: surface created
 */

extern "C"
JNIEXPORT void JNICALL
Java_com_krispyclient_launcher_NativeBridge_nativeSurfaceCreated(
    JNIEnv* env,
    jobject,
    jobject surface
) {

    LOGI(
        "Surface created"
    );

    if (surface == nullptr) {

        LOGE(
            "Surface is null"
        );

        return;
    }

    if (g_initialized) {

        destroyEgl();
    }

    if (g_window != nullptr) {

        ANativeWindow_release(
            g_window
        );

        g_window = nullptr;
    }

    g_window = ANativeWindow_fromSurface(
        env,
        surface
    );

    if (g_window == nullptr) {

        LOGE(
            "ANativeWindow_fromSurface failed"
        );

        return;
    }

    g_width = ANativeWindow_getWidth(
        g_window
    );

    g_height = ANativeWindow_getHeight(
        g_window
    );

    if (g_width <= 0) {
        g_width = 1;
    }

    if (g_height <= 0) {
        g_height = 1;
    }

    LOGI(
        "Surface size: %dx%d",
        g_width,
        g_height
    );

    if (!initEgl()) {

        LOGE(
            "EGL initialization failed"
        );

        ANativeWindow_release(
            g_window
        );

        g_window = nullptr;

        return;
    }

    renderFrame();
}


/*
 * JNI: surface changed
 */

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

    if (g_width <= 0) {
        g_width = 1;
    }

    if (g_height <= 0) {
        g_height = 1;
    }

    LOGI(
        "Surface changed: %dx%d",
        g_width,
        g_height
    );

    renderFrame();
}


/*
 * JNI: surface destroyed
 */

extern "C"
JNIEXPORT void JNICALL
Java_com_krispyclient_launcher_NativeBridge_nativeSurfaceDestroyed(
    JNIEnv*,
    jobject
) {

    LOGI(
        "Surface destroyed"
    );

    destroyEgl();

    if (g_window != nullptr) {

        ANativeWindow_release(
            g_window
        );

        g_window = nullptr;
    }
}


/*
 * JNI: resume
 */

extern "C"
JNIEXPORT void JNICALL
Java_com_krispyclient_launcher_NativeBridge_nativeResume(
    JNIEnv*,
    jobject
) {

    LOGI(
        "Native resume"
    );
}


/*
 * JNI: pause
 */

extern "C"
JNIEXPORT void JNICALL
Java_com_krispyclient_launcher_NativeBridge_nativePause(
    JNIEnv*,
    jobject
) {

    LOGI(
        "Native pause"
    );
}


/*
 * JNI: destroy
 */

extern "C"
JNIEXPORT void JNICALL
Java_com_krispyclient_launcher_NativeBridge_nativeDestroy(
    JNIEnv*,
    jobject
) {

    LOGI(
        "Native destroy"
    );

    destroyEgl();

    if (g_window != nullptr) {

        ANativeWindow_release(
            g_window
        );

        g_window = nullptr;
    }
}
