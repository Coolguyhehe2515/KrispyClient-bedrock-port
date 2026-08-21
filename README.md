
# KrispyClient

<p align="center">
  <b>Aurora-powered Bedrock client experiment</b>
</p>

<p align="center">
  Native Android client/renderer project built with C++ and OpenGL ES.
</p>

---

## About

**KrispyClient** is an experimental Minecraft: Bedrock Edition client project focused on building a custom Android-native client layer.

The project is being developed from the ground up rather than relying on a traditional Bedrock mod loader.

KrispyClient is strongly inspired by the idea of **Aurora** — a clean, modern, lightweight experience with a focus on native rendering and performance.

> KrispyClient = Aurora.

---

## Current Status

The project is currently in the early development stage.

### Native Renderer

The current native layer already includes:

- Android JNI bridge
- `ANativeWindow` integration
- EGL initialization
- OpenGL ES 3.x context
- GLSL ES shaders
- Shader compilation
- Shader linking
- VAO/VBO creation
- OpenGL draw calls
- EGL buffer swapping
- Native lifecycle handling
- Android Logcat debugging

Current renderer test:

```text
+-----------------------------+
|                             |
|            / \              |
|           /   \             |
|          /     \             |
|         /_______\            |
|                             |
+-----------------------------+

The triangle is used as a basic rendering test before moving toward the actual client renderer.


---

Architecture

KrispyClient currently consists of several layers:

KrispyClient
│
├── Android Launcher
│   ├── MainActivity
│   └── NativeBridge
│
├── Native Layer
│   └── C++
│
├── Rendering
│   ├── EGL
│   ├── OpenGL ES
│   └── GLSL ES
│
└── Future Client Features
    ├── HUD
    ├── FPS Counter
    ├── CPS Counter
    ├── Keystrokes
    ├── Resource Pack Manager
    └── Client UI


---

Native Renderer

The native renderer is written in C++ and compiled as an Android shared library:

libkrispyclient.so

Target architecture:

arm64-v8a

The current native build uses:

C++17

Android NDK

EGL

OpenGL ES 3

JNI


Example build target:

aarch64-linux-android28


---

Rendering Pipeline

The current rendering pipeline is:

Android Surface
      │
      ▼
ANativeWindow
      │
      ▼
EGLDisplay
      │
      ▼
EGLContext
      │
      ▼
OpenGL ES 3.x
      │
      ├── Vertex Shader
      │
      ├── Fragment Shader
      │
      ├── VAO
      │
      └── VBO
      │
      ▼
eglSwapBuffers()
      │
      ▼
Android Surface


---

Project Goals

KrispyClient aims to eventually provide a custom Bedrock client experience with features such as:

Custom client UI

FPS counter

CPS counter

Keystrokes

Custom HUD

Resource pack switching

Client settings

Performance improvements

Native rendering components

Custom visual effects

Aurora-inspired design


The project is still experimental, so APIs and architecture may change significantly.


---

Why Native?

Minecraft Bedrock does not provide the same modding ecosystem as Minecraft Java Edition.

Instead of depending on a Java-style mod loader, KrispyClient is experimenting with a native Android approach.

The goal is to build the required native components using:

C++
JNI
EGL
OpenGL ES
Android NDK

This allows KrispyClient to interact with Android's native graphics stack directly.


---

Development

Requirements

You will need:

Android SDK

Android NDK

Java 17

C++17 compatible compiler

Git

Android device/emulator


The GitHub Actions workflow can also build the native library automatically.


---

GitHub Actions

The repository includes a GitHub Actions workflow for building the native library.

The workflow prepares:

Android SDK
Android NDK
Java 17

and then compiles:

native/krispyclient.cpp

into:

build/arm64-v8a/libkrispyclient.so


---

Repository Structure

KrispyClient-bedrock-port/
│
├── .github/
│   └── workflows/
│       └── build.yml
│
├── native/
│   └── krispyclient.cpp
│
├── app/
│   └── ...
│
├── build/
│   └── arm64-v8a/
│
├── README.md
└── ...


---

Roadmap

Phase 1 — Native Foundation

[x] Android project

[x] JNI bridge

[x] Native library

[x] EGL initialization

[x] OpenGL ES context

[x] Shader compilation

[x] VAO/VBO setup

[ ] Stable triangle renderer

[ ] Rendering loop


Phase 2 — Client Layer

[ ] Client UI

[ ] HUD system

[ ] FPS counter

[ ] CPS counter

[ ] Keystrokes

[ ] Settings system

[ ] About KrispyClient page


Phase 3 — Bedrock Integration

[ ] Native integration research

[ ] Runtime integration

[ ] Rendering integration

[ ] Resource pack integration

[ ] Client-side systems


Phase 4 — Aurora

[ ] Aurora-inspired UI

[ ] Aurora visual system

[ ] Custom animations

[ ] Performance optimizations

[ ] Final client branding



---

Branding

KrispyClient uses Aurora as its main visual identity.

The design direction is based around:

KrispyClient
      │
      ▼
   AURORA
      │
      ├── Clean
      ├── Modern
      ├── Lightweight
      └── Native

The Aurora identity will gradually become more prominent as the client UI and rendering systems are developed.


---

Disclaimer

KrispyClient is an independent experimental project.

It is not affiliated with Mojang Studios or Microsoft.

Minecraft is a trademark of Mojang Studios.


---

Development Status

Native foundation       ██████████░░░░░░░░░░  50%
Rendering                ██████░░░░░░░░░░░░░░  30%
Client systems           ██░░░░░░░░░░░░░░░░░░  10%
Bedrock integration      ░░░░░░░░░░░░░░░░░░░░   0%
Aurora UI                ░░░░░░░░░░░░░░░░░░░░   0%

> This project is actively being developed. Expect things to break.




---

KrispyClient

Built around Aurora.

K R I S P Y C L I E N T

          A U R O R A
