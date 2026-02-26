# Android Platform Layer

## Overview

Android-specific implementations for windowing (ANativeWindow), threading (pthreads), and callstack capture (libunwind/dladdr). Mirrors the Windows platform layer API.

## Files

- `Zenith_Android_Window.h` - ANativeWindow wrapper (singleton)
- `Zenith_OS_Include.h` - Platform aggregator, defines `Zenith_Mutex`/`Zenith_Semaphore` aliases
- `Multithreading/Zenith_Android_Multithreading.h` - pthread mutex and POSIX semaphore wrappers
- `Callstack/Zenith_Android_Callstack.h` - libunwind/dladdr-based stack trace capture

## Window (Zenith_Window)

Singleton wrapping `ANativeWindow*`. Requires `SetAndroidApp()` before initialization.

| Function | Description |
|----------|-------------|
| `SetAndroidApp(android_app*)` | Set native activity (must call before Inititalise) |
| `Inititalise(title, w, h)` | Create window (static factory) |
| `GetInstance()` | Singleton accessor (non-asserting for async setup) |
| `GetNativeWindow()` | Returns `ANativeWindow*` for Vulkan surface |
| `BeginFrame()` | Process events |
| `IsWindowReady()` | Check if ANativeWindow is available |
| `GetSize(w&, h&)` | Window dimensions |
| `OnTouchEvent(action, x, y)` | Process touch (0=DOWN, 1=UP, 2=MOVE) |

### Touch-to-Mouse Emulation

Touch events are translated to `ZENITH_MOUSE_BUTTON_1` presses for compatibility:
- Touch DOWN = mouse button press
- Touch MOVE = mouse position update
- Touch UP = mouse button release

Cursor capture functions are no-ops on Android.

## Multithreading

### Zenith_Android_Mutex

Wraps `pthread_mutex_t`. No profiling template (single variant).

| Function | Description |
|----------|-------------|
| `Lock()` | `pthread_mutex_lock` |
| `TryLock()` | `pthread_mutex_trylock` |
| `Unlock()` | `pthread_mutex_unlock` |

### Zenith_Android_Semaphore

Wraps POSIX `sem_t` with max value tracking.

| Function | Description |
|----------|-------------|
| Constructor | `sem_init(initial)`, stores max |
| `Wait()` | `sem_wait` (blocking) |
| `TryWait()` | `sem_trywait` |
| `Signal()` | `sem_post` |

## Callstack (Zenith_Android_Callstack)

Uses `_Unwind_Backtrace` for capture, `dladdr` + `__cxa_demangle` for symbol resolution. Thread-safe via `pthread_mutex_t`.

**Limitation:** Function names only, no file/line info (Android NDK limitation).

## Platform Aliases (Zenith_OS_Include.h)

```cpp
#define Zenith_Mutex Zenith_Android_Mutex
#define Zenith_Mutex_NoProfiling Zenith_Android_Mutex
#define Zenith_Semaphore Zenith_Android_Semaphore
```

Note: Both mutex aliases map to the same type (no profiling variant on Android).

## Android Build & Deployment

### Build System (Sharpmake AGDE)

Android builds use AGDE (Android Game Development Extension) targets in Visual Studio. Key Sharpmake configuration:

**Sharpmake_Common.cs:**
- Defines `ZENITH_ANDROID`, `VULKAN_HPP_NO_EXCEPTIONS`
- Sets C++20 via `Options.Agde.Compiler.CppLanguageStandard.Cpp20` (post-processed to `cpp2a` by `Sharpmake_Build.bat` since AGDE only accepts `cpp2a`)
- Adds Vulkan SDK and Android platform include paths
- Adds `Zenith/Android/NativeGlue` include path for `android_native_app_glue.h`

**Sharpmake_Zenith.cs:**
- `ENGINE_ASSETS_DIR=""` and `SHADER_SOURCE_ROOT=""` for AGDE (empty strings so AAssetManager uses relative paths)
- Excludes ImGui backends, Editor directory on AGDE
- Excludes `NativeGlue/` from PCH (plain C file)

**Sharpmake_Games.cs:**
- `GAME_ASSETS_DIR=""` for AGDE
- Output type is `Dll` (.so shared library)
- Links Android system libraries: `-landroid`, `-llog`, `-lvulkan`

**Sharpmake_Build.bat:**
- Post-processes all `*_agde.vcxproj` files to replace `<CppLanguageStandard>cpp20</CppLanguageStandard>` with `cpp2a` (AGDE doesn't accept `cpp20`)

### Platform-Specific Code Guards

Code that is Windows-only or unavailable on Android uses these guards:

| Guard | Purpose |
|-------|---------|
| `#ifdef ZENITH_WINDOWS` | Slang shader compiler, `Zenith_Main()` loop, `_aligned_malloc`, MSVC intrinsics |
| `#ifdef ZENITH_TOOLS` | Assimp import, editor, hot reload, OpenCV (never defined on Android) |
| `#ifdef _MSC_VER` | MSVC-specific intrinsics like `__popcnt` (use `__builtin_popcount` on Clang) |

Key files with Android guards:
- `Flux_SlangCompiler.cpp` - Slang SDK includes and `Flux_SlangCompiler` methods wrapped in `#ifdef ZENITH_WINDOWS`; `Flux_ShaderReflection` methods are unconditional
- `Flux.cpp` - Slang init/shutdown in `#ifdef ZENITH_WINDOWS`
- `Zenith_Vulkan_Pipeline.cpp` - Runtime shader compilation in `#ifdef ZENITH_WINDOWS`; pre-compiled `.spv` + `.spv.refl` loading is unconditional
- `Zenith_Main.cpp` / `Zenith_Core.h` - `Zenith_Main()` polling loop wrapped in `#ifdef ZENITH_WINDOWS` (Android uses its own main loop in `Zenith_Android_Main.cpp`)
- `Zenith_MemoryManagement.cpp` - `posix_memalign()` instead of `aligned_alloc()` on non-Windows
- `Zenith_BTDecorators.cpp` - `REPEAT_INFINITE` instead of Windows `INFINITE` macro
- `Zenith_AssetRegistry.h` - `Zenith_TypeIndex` (compile-time type IDs via static address) replaces `std::type_index`/`typeid` (RTTI is disabled)
- `Zenith_FileWatcher.cpp` - `std::error_code` overloads instead of `try`/`catch` (exceptions are disabled)
- `Flux_ParticleEmitterConfig.h` - Explicit `static_cast<float>(RAND_MAX)` to avoid Clang implicit conversion warning
- `TilePuzzle_Rules.h` - `TILEPUZZLE_POPCNT` macro dispatches to `__popcnt` (MSVC) or `__builtin_popcount` (Clang)

### Shader System

Android does not have the Slang shader compiler at runtime. Shaders must be pre-compiled offline.

**Offline compile step (Windows):**
1. Build and run `FluxCompiler` (Windows tool)
2. FluxCompiler compiles all `.vert`/`.frag`/`.comp` files via Slang to `.spv` (SPIR-V) and writes companion `.spv.refl` files containing serialized `Flux_ShaderReflection` binding data
3. Both `.spv` and `.spv.refl` files live in `Zenith/Flux/Shaders/` alongside the source

**Android runtime:**
- Pre-compiled `.spv` and `.spv.refl` files are bundled into the APK via Gradle asset sources
- `Zenith_Vulkan_Shader::Initialise()` loads `.spv` files and deserializes `.spv.refl` to populate `Flux_ShaderReflection`
- `Flux_ShaderBinder` works identically using the deserialized reflection data

**Important:** After any shader source changes, re-run FluxCompiler before building the Android APK.

### NativeGlue

`Zenith/Android/NativeGlue/` contains `android_native_app_glue.h` and `.c` copied from the NDK (`$(ANDROID_NDK_ROOT)/sources/android/native_app_glue/`). These provide the `ANativeActivity_onCreate` entry point and event loop infrastructure. The `.c` file is excluded from PCH.

### Deployment Steps

```
1. Sharpmake_Build.bat              # Regenerate solutions
2. Build FluxCompiler (Win64)       # Build shader compiler
3. Run FluxCompiler.exe             # Generate .spv + .spv.refl
4. Build zenith_agde.sln            # Build AGDE solution
5. deploy_android.bat debug Game    # Stage .so + libc++_shared.so
6. cd Games/Game/Android
7. gradlew assembleDebug            # Build APK
8. adb install -r app/build/outputs/apk/debug/app-debug.apk
9. adb shell am start -n com.zenith.game/android.app.NativeActivity
```

### Gradle Configuration

Each game has `Games/<Game>/Android/app/build.gradle` that bundles:
- `../../Assets` - Game-specific assets
- `../../../../Zenith/Assets` - Engine assets (fonts, default textures, etc.)
- `../../../../Zenith/Flux/Shaders` - Pre-compiled shaders (`.spv` + `.spv.refl`)

### Known Constraints

- **No RTTI** - `typeid` unavailable; use `Zenith_TypeIndex::Of<T>()` for type identification
- **No exceptions** - `try`/`catch` forbidden; use `std::error_code` overloads or return values
- **No `std::function`** - Use function pointers
- **No Slang runtime** - Shaders must be pre-compiled via FluxCompiler
- **No Assimp** - Asset import is `#ifdef ZENITH_TOOLS` only; use pre-baked `.zanim`/`.zmesh` formats
- **Save data** - `Zenith_SaveData::Load` may fail (no writable working directory without using `internalDataPath`)
