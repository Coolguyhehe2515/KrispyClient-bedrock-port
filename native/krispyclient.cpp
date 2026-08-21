#include <jni.h>

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <cstdio>

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


// ============================================================
// Forward declarations
// ============================================================

static void destroyEgl();
static void destroyRenderer();
static void renderFrame();


// ============================================================
// Shaders
// ============================================================

static const char* VERTEX_SHADER = R"(
#version 300 es

layout(location = 0) in vec2 a_position;

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);
}
)";


static const char* FRAGMENT_SHADER = R"(
#version 300 es

precision mediump float;

out vec4 outColor;

void main()
{
    outColor = vec4(
        0.15,
        0.55,
        1.0,
        1.0
    );
}
)";


// ============================================================
// GL error helper
// ============================================================

static void checkGlError(const char* location)
{
    GLenum error;

    while ((error = glGetError()) != GL_NO_ERROR)
    {
        LOGE(
            "OpenGL error at %s: 0x%04x",
            location,
            error
        );
    }
}


// ============================================================
// Shader compilation
// ============================================================

static GLuint compileShader(
    GLenum type,
    const char* source
)
{
    GLuint shader = glCreateShader(type);

    if (shader == 0)
    {
        LOGE("glCreateShader failed");
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

    if (compiled != GL_TRUE)
    {
        GLint logLength = 0;

        glGetShaderiv(
            shader,
            GL_INFO_LOG_LENGTH,
            &logLength
        );

        if (logLength > 0)
        {
            char* log = new char[logLength + 1];

            log[0] = '\0';

            glGetShaderInfoLog(
                shader,
                logLength,
                nullptr,
                log
            );

            log[logLength] = '\0';

            LOGE(
                "Shader compilation failed:\n%s",
                log
            );

            delete[] log;
        }
        else
        {
            LOGE("Shader compilation failed with no log");
        }

        glDeleteShader(shader);

        return 0;
    }

    LOGI("Shader compiled successfully");

    return shader;
}


// ============================================================
// Program creation
// ============================================================

static bool createProgram()
{
    LOGI("Creating OpenGL shader program");

    GLuint vertexShader = compileShader(
        GL_VERTEX_SHADER,
        VERTEX_SHADER
    );

    if (vertexShader == 0)
    {
        return false;
    }

    GLuint fragmentShader = compileShader(
        GL_FRAGMENT_SHADER,
        FRAGMENT_SHADER
    );

    if (fragmentShader == 0)
    {
        glDeleteShader(vertexShader);

        return false;
    }

    g_program = glCreateProgram();

    if (g_program == 0)
    {
        LOGE("glCreateProgram failed");

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

    glBindAttribLocation(
        g_program,
        0,
        "a_position"
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

    if (linked != GL_TRUE)
    {
        GLint logLength = 0;

        glGetProgramiv(
            g_program,
            GL_INFO_LOG_LENGTH,
            &logLength
        );

        if (logLength > 0)
        {
            char* log = new char[logLength + 1];

            log[0] = '\0';

            glGetProgramInfoLog(
                g_program,
                logLength,
                nullptr,
                log
            );

            log[logLength] = '\0';

            LOGE(
                "Program linking failed:\n%s",
                log
            );

            delete[] log;
        }
        else
        {
            LOGE("Program linking failed with no log");
        }

        glDeleteProgram(g_program);

        g_program = 0;

        return false;
    }

    LOGI(
        "OpenGL shader program created successfully"
    );

    checkGlError("createProgram");

    return true;
}


// ============================================================
// Triangle
// ============================================================

static bool createTriangle()
{
    LOGI("Creating triangle geometry");

    const GLfloat vertices[] =
    {
         0.0f,  0.70f,

        -0.70f, -0.60f,

         0.70f, -0.60f
    };

    glGenVertexArrays(
        1,
        &g_vao
    );

    glGenBuffers(
        1,
        &g_vbo
    );

    if (g_vao == 0)
    {
        LOGE("glGenVertexArrays failed");

        return false;
    }

    if (g_vbo == 0)
    {
        LOGE("glGenBuffers failed");

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

    checkGlError("createTriangle");

    LOGI(
        "Triangle geometry created successfully"
    );

    return true;
}


// ============================================================
// Renderer initialization
// ============================================================

static bool initRenderer()
{
    LOGI("Initializing renderer");

    if (!createProgram())
    {
        LOGE(
            "Failed to create shader program"
        );

        return false;
    }

    if (!createTriangle())
    {
        LOGE(
            "Failed to create triangle"
        );

        return false;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    checkGlError("initRenderer");

    LOGI(
        "Native renderer initialized"
    );

    return true;
}


// ============================================================
// Renderer destruction
// ============================================================

static void destroyRenderer()
{
    LOGI("Destroying renderer");

    if (g_vbo != 0)
    {
        glDeleteBuffers(
            1,
            &g_vbo
        );

        g_vbo = 0;
    }

    if (g_vao != 0)
    {
        glDeleteVertexArrays(
            1,
            &g_vao
        );

        g_vao = 0;
    }

    if (g_program != 0)
    {
        glDeleteProgram(
            g_program
        );

        g_program = 0;
    }

    LOGI(
        "Native renderer destroyed"
    );
}


// ============================================================
// EGL initialization
// ============================================================

static bool initEgl()
{
    if (g_initialized)
    {
        LOGI(
            "EGL already initialized"
        );

        return true;
    }

    if (g_window == nullptr)
    {
        LOGE(
            "initEgl: ANativeWindow is null"
        );

        return false;
    }

    // Get actual window dimensions.

    int windowWidth =
        ANativeWindow_getWidth(g_window);

    int windowHeight =
        ANativeWindow_getHeight(g_window);

    if (windowWidth > 0)
    {
        g_width = windowWidth;
    }

    if (windowHeight > 0)
    {
        g_height = windowHeight;
    }

    LOGI(
        "Native window size: %dx%d",
        g_width,
        g_height
    );


    // --------------------------------------------------------
    // EGL display
    // --------------------------------------------------------

    g_display = eglGetDisplay(
        EGL_DEFAULT_DISPLAY
    );

    if (g_display == EGL_NO_DISPLAY)
    {
        LOGE(
            "eglGetDisplay failed: 0x%04x",
            eglGetError()
        );

        return false;
    }


    // --------------------------------------------------------
    // EGL initialize
    // --------------------------------------------------------

    EGLint major = 0;
    EGLint minor = 0;

    if (!eglInitialize(
            g_display,
            &major,
            &minor
        ))
    {
        LOGE(
            "eglInitialize failed: 0x%04x",
            eglGetError()
        );

        g_display = EGL_NO_DISPLAY;

        return false;
    }

    LOGI(
        "EGL version: %d.%d",
        major,
        minor
    );


    // --------------------------------------------------------
    // Choose config
    // --------------------------------------------------------

    const EGLint configAttributes[] =
    {
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
        ))
    {
        LOGE(
            "eglChooseConfig failed: 0x%04x",
            eglGetError()
        );

        eglTerminate(g_display);

        g_display = EGL_NO_DISPLAY;

        return false;
    }

    if (numConfigs == 0)
    {
        LOGE(
            "eglChooseConfig returned zero configs"
        );

        eglTerminate(g_display);

        g_display = EGL_NO_DISPLAY;

        return false;
    }

    LOGI(
        "EGL config selected"
    );


    // --------------------------------------------------------
    // OpenGL ES 3 context
    // --------------------------------------------------------

    const EGLint contextAttributes[] =
    {
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

    if (g_context == EGL_NO_CONTEXT)
    {
        LOGE(
            "eglCreateContext failed: 0x%04x",
            eglGetError()
        );

        eglTerminate(g_display);

        g_display = EGL_NO_DISPLAY;

        return false;
    }

    LOGI(
        "EGL OpenGL ES 3 context created"
    );


    // --------------------------------------------------------
    // Window surface
    // --------------------------------------------------------

    g_surface = eglCreateWindowSurface(
        g_display,
        config,
        g_window,
        nullptr
    );

    if (g_surface == EGL_NO_SURFACE)
    {
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

    LOGI(
        "EGL window surface created"
    );


    // --------------------------------------------------------
    // Make context current
    // --------------------------------------------------------

    if (!eglMakeCurrent(
            g_display,
            g_surface,
            g_surface,
            g_context
        ))
    {
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

    LOGI(
        "EGL context is current"
    );


    // --------------------------------------------------------
    // Swap interval
    // --------------------------------------------------------

    eglSwapInterval(
        g_display,
        1
    );


    // --------------------------------------------------------
    // OpenGL information
    // --------------------------------------------------------

    const GLubyte* version =
        glGetString(GL_VERSION);

    const GLubyte* renderer =
        glGetString(GL_RENDERER);

    const GLubyte* vendor =
        glGetString(GL_VENDOR);

    const GLubyte* shading =
        glGetString(GL_SHADING_LANGUAGE_VERSION);

    LOGI(
        "OpenGL version: %s",
        version ? reinterpret_cast<const char*>(version) : "NULL"
    );

    LOGI(
        "OpenGL renderer: %s",
        renderer ? reinterpret_cast<const char*>(renderer) : "NULL"
    );

    LOGI(
        "OpenGL vendor: %s",
        vendor ? reinterpret_cast<const char*>(vendor) : "NULL"
    );

    LOGI(
        "GLSL version: %s",
        shading ? reinterpret_cast<const char*>(shading) : "NULL"
    );


    // --------------------------------------------------------
    // Renderer
    // --------------------------------------------------------

    if (!initRenderer())
    {
        LOGE(
            "Renderer initialization failed"
        );

        destroyEgl();

        return false;
    }

    g_initialized = true;

    LOGI(
        "KrispyClient EGL + renderer initialized successfully"
    );

    return true;
}


// ============================================================
// EGL destruction
// ============================================================

static void destroyEgl()
{
    LOGI(
        "destroyEgl()"
    );

    if (g_display == EGL_NO_DISPLAY)
    {
        g_initialized = false;

        return;
    }

    // The OpenGL objects must be destroyed
    // while the context is still current.

    if (g_context != EGL_NO_CONTEXT)
    {
        if (!eglMakeCurrent(
                g_display,
                g_surface,
                g_surface,
                g_context
            ))
        {
            LOGE(
                "Failed to make EGL context current during destroy"
            );
        }

        destroyRenderer();
    }

    eglMakeCurrent(
        g_display,
        EGL_NO_SURFACE,
        EGL_NO_SURFACE,
        EGL_NO_CONTEXT
    );

    if (g_surface != EGL_NO_SURFACE)
    {
        eglDestroySurface(
            g_display,
            g_surface
        );

        g_surface = EGL_NO_SURFACE;
    }

    if (g_context != EGL_NO_CONTEXT)
    {
        eglDestroyContext(
            g_display,
            g_context
        );

        g_context = EGL_NO_CONTEXT;
    }

    eglTerminate(
        g_display
    );

    g_display = EGL_NO_DISPLAY;

    g_initialized = false;

    LOGI(
        "EGL destroyed"
    );
}


// ============================================================
// Render frame
// ============================================================

static void renderFrame()
{
    if (!g_initialized)
    {
        LOGE(
            "renderFrame called while renderer is not initialized"
        );

        return;
    }

    if (g_display == EGL_NO_DISPLAY)
    {
        LOGE(
            "renderFrame: invalid EGL display"
        );

        return;
    }

    if (g_surface == EGL_NO_SURFACE)
    {
        LOGE(
            "renderFrame: invalid EGL surface"
        );

        return;
    }

    if (g_context == EGL_NO_CONTEXT)
    {
        LOGE(
            "renderFrame: invalid EGL context"
        );

        return;
    }


    // Make absolutely sure this thread owns the context.

    if (!eglMakeCurrent(
            g_display,
            g_surface,
            g_surface,
            g_context
        ))
    {
        LOGE(
            "renderFrame: eglMakeCurrent failed: 0x%04x",
            eglGetError()
        );

        return;
    }


    // Update dimensions.

    int windowWidth =
        ANativeWindow_getWidth(g_window);

    int windowHeight =
        ANativeWindow_getHeight(g_window);

    if (windowWidth > 0)
    {
        g_width = windowWidth;
    }

    if (windowHeight > 0)
    {
        g_height = windowHeight;
    }

    if (g_width <= 0)
    {
        g_width = 1;
    }

    if (g_height <= 0)
    {
        g_height = 1;
    }


    LOGI(
        "Rendering frame: %dx%d",
        g_width,
        g_height
    );


    // Viewport.

    glViewport(
        0,
        0,
        g_width,
        g_height
    );


    // Clear screen.

    glClearColor(
        0.03f,
        0.03f,
        0.03f,
        1.0f
    );

    glClear(
        GL_COLOR_BUFFER_BIT
    );

    checkGlError("glClear");


    // Use shader.

    glUseProgram(
        g_program
    );

    checkGlError("glUseProgram");


    // Bind triangle.

    glBindVertexArray(
        g_vao
    );

    checkGlError("glBindVertexArray");


    // Draw.

    glDrawArrays(
        GL_TRIANGLES,
        0,
        3
    );

    checkGlError("glDrawArrays");


    // Unbind.

    glBindVertexArray(0);

    glUseProgram(0);


    // Present.

    if (!eglSwapBuffers(
            g_display,
            g_surface
        ))
    {
        LOGE(
            "eglSwapBuffers failed: 0x%04x",
            eglGetError()
        );

        return;
    }

    LOGI(
        "Frame presented successfully"
    );
}


// ============================================================
// JNI: version
// ============================================================

extern "C"
JNIEXPORT jstring JNICALL
Java_com_krispyclient_launcher_NativeBridge_getNativeVersion(
    JNIEnv* env,
    jobject
)
{
    return env->NewStringUTF(
        "KrispyClient Native Renderer 0.5"
    );
}


// ============================================================
// JNI: surface created
// ============================================================

extern "C"
JNIEXPORT void JNICALL
Java_com_krispyclient_launcher_NativeBridge_nativeSurfaceCreated(
    JNIEnv* env,
    jobject,
    jobject surface
)
{
    LOGI(
        "========================================"
    );

    LOGI(
        "nativeSurfaceCreated called"
    );

    LOGI(
        "========================================"
    );

    if (surface == nullptr)
    {
        LOGE(
            "Surface is null"
        );

        return;
    }


    // Destroy old EGL state.

    if (g_initialized ||
        g_display != EGL_NO_DISPLAY)
    {
        destroyEgl();
    }


    // Release old native window.

    if (g_window != nullptr)
    {
        ANativeWindow_release(
            g_window
        );

        g_window = nullptr;
    }


    // Convert Java Surface to ANativeWindow.

    g_window = ANativeWindow_fromSurface(
        env,
        surface
    );

    if (g_window == nullptr)
    {
        LOGE(
         
