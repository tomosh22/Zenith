# Android Platform Layer

## Overview

Android-specific implementations for windowing (ANativeWindow), threading (pthreads), and callstack capture (libunwind/dladdr). Mirrors the Windows platform layer API.

## Files

- `Zenith_Android_Window.h` - ANativeWindow wrapper (singleton)
- `Zenith_OS_Include.h` - Platform aggregator, defines `Zenith_Mutex`/`Zenith_Semaphore` aliases
- `Multithreading/Zenith_Android_Multithreading.h` - pthread mutex and POSIX semaphore wrappers
- `Callstack/Zenith_Android_Callstack.h` - libunwind/dladdr-based stack trace capture
- `FileAccess/Zenith_Android_FileAccess.cpp` - Android file access (AAssetManager for APK assets, filesystem fallback for writable storage)
- `Zenith_Android_DebugBreak.cpp` - `Zenith_DebugBreak()` via `raise(SIGTRAP)` (with assert-capture support)
- `Zenith_Android_Main.cpp` - `android_main()` entry point + the activity/cmd-pipe loop
- `Zenith_Android_PlatformStdio.cpp` - POSIX-backed file opening and temporary files (the Android half of `Core/Zenith_PlatformStdio.h`; the Windows half is `Zenith_Windows_PlatformStdio.cpp`)
- `Zenith_Android_PlatformEnvironment.cpp` - POSIX-backed environment-variable reads (the Android half of `Core/Zenith_PlatformEnvironment.h`)

## Window (Zenith_Window)

Singleton wrapping `ANativeWindow*`. Requires `SetAndroidApp()` before initialization.

| Function | Description |
|----------|-------------|
| `SetAndroidApp(android_app*)` | Set native activity (must call before Initialise) |
| `Initialise(title, w, h)` | Create window (static factory) |
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
- Touch UP = clears internal `m_bTouchDown` flag only (no release callback)

Cursor capture functions are no-ops on Android.

## Multithreading

### Zenith_Android_Mutex

Wraps `pthread_mutex_t`. Template `Zenith_Android_Mutex_T<bool bEnableProfiling = true>`; `Lock()` has two explicit specializations: `<true>` with profiling zone markers, `<false>` without.

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
#define Zenith_Mutex Zenith_Android_Mutex_T<true>
#define Zenith_Mutex_NoProfiling Zenith_Android_Mutex_T<false>
#define Zenith_Semaphore Zenith_Android_Semaphore
```

Note: `Zenith_Mutex` maps to `Zenith_Android_Mutex_T<true>` (profiling enabled), while `Zenith_Mutex_NoProfiling` maps to `Zenith_Android_Mutex_T<false>` (profiling disabled). Both use the same template class with a different parameter value.

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
| `#ifdef ZENITH_TOOLS` | Assimp import, editor, hot reload, asset export (never defined on Android) |
| Platform-specific CRT behaviour | Put the implementation in matching `Windows/` and `Android/` translation units; do not branch on compiler macros in shared code |

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
- `TilePuzzle_Rules.h` / `TilePuzzle_Solver.h` - C++20 `std::popcount` is portable across MSVC and Clang

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

### ABIs: arm64-v8a (devices) and x86_64 (the emulator)

Both ABIs are built from one axis, `ZenithAndroidAbi.All` in
`Build/Sharpmake_Common.cs`, mirrored Gradle-side by
`Build/zenith_android_abis.gradle`. Gradle merges every ABI that has actually
been built into a single APK.

**x86_64 is the only way to run on the local emulator.** Google's QEMU2 emulator
cannot host an arm64 guest on an x86_64 host at all (`"Avd's CPU Architecture
'arm64' is not supported by the QEMU2 emulator on x86_64 host"`), so a dev box
with no physical ARM device can only exercise Android through x86_64.

Config names carry the ABI: `Vulkan_<abi_token>_vs2022_<Debug|Release>_Agde_False`
with platform `Android-<abi-dir>`. Mind the two spellings — the config token uses
underscores (`arm64_v8a`), the on-disk ABI directory uses a dash (`arm64-v8a`);
they coincide only for `x86_64`.

### Debug APK validation layer

`ZENITH_FLUX_PROFILING` is defined unconditionally in `Zenith.h`, so
`GetRequiredInstanceExtensions` asks for `VK_EXT_debug_utils`. On Android that
extension is commonly supplied by the **validation layer**, not the platform
loader. `Zenith_Vulkan::CreateInstance` treats the diagnostics extension as
optional, so a layer-less APK still boots, but you want the layer for useful
validation diagnostics:

```
pwsh ./Build/download_validation_layer.ps1 -Game <Game> -Abi x86_64
```

It stages into `Games/<Game>/Android/app/src/debug/jniLibs/<abi>/`.

### Deployment Steps

```
1. Build\regen.ps1                  # Regenerate solutions (per-game + engine)
2. Build FluxCompiler (Win64)       # Build shader compiler
3. Run FluxCompiler.exe             # Generate .spv + .spv.refl
4. pwsh Build\download_validation_layer.ps1 -Game <Game> -Abi x86_64   # debug only
5. msbuild Games\<Game>\<game>_agde.sln /t:<Game> ^
     /p:Configuration=Vulkan_x86_64_vs2022_Debug_Agde_False /p:Platform=Android-x86_64
   # AGDE runs Gradle itself and emits the APK under
   #   Games/<Game>/Build/output/agde/x86_64_vs2022_debug_agde_false/
6. adb install -r -t <that>.apk     # -t is REQUIRED: AGP marks debug APKs testOnly
7. adb shell am start -n com.zenith.<game>/android.app.NativeActivity
```

Two staging models are in play, and which one a game uses is per-game:

| Game | `jniLibs.srcDirs` | Needs `deploy_android.ps1`? |
|---|---|---|
| TilePuzzle, Zenithmon | `zenithJniLibDirs(<buildType>)` — straight at the pinned AGDE OutDir | No; the AGDE path above is self-contained |
| Combat, DevilsPlayground, RenderTest | `['jniLibs']` — hand-staged `app/jniLibs/<abi>/` | **Yes**, run it before Gradle |

`deploy_android.ps1` (`-Abi <abi dir name>|all`, ABIs from the shared axis)
stages both the game `.so` and the NDK's `libc++_shared.so` per ABI. All five
games take `abiFilters` from `Build/zenith_android_abis.gradle`, so an ABI that
was never built is simply absent from the APK rather than an error.

### Reading engine logs on device

`Zenith_Log`/`Warning`/`Error` route to **logcat** on Android (an app's stdout is
discarded, which used to make the engine completely silent on device). Tags are
`Zenith.<Category>` — note the categories are mixed-case (`Zenith.Vulkan`,
`Zenith.Core`, `Zenith.Renderer`), not upper-case:

```
adb logcat -s Zenith.Vulkan Zenith.Core        # one or more categories
adb logcat | Select-String "Zenith\."          # everything the engine logs
```

### Selecting the JDK (per machine, NOT per repo)

No game's `gradle.properties` pins `org.gradle.java.home` — it is an absolute,
machine-local path, and once set Gradle will NOT fall back to `JAVA_HOME`, so a
tracked pin hard-fails on every other machine. Set it once, for every game, in
your own `%USERPROFILE%\.gradle\gradle.properties`:

```properties
org.gradle.java.home=C\:/Program Files/Android/Android Studio/jbr
```

Or point `JAVA_HOME` at a JDK Gradle 8.13 accepts (17–23; it rejects 24+ with
`Unsupported class file major version`).

### Gradle Configuration

Each game has `Games/<Game>/Android/app/build.gradle` that bundles:
- `../../Assets` - Game-specific assets
- `../../../../Zenith/Assets` - Engine assets (fonts, default textures, etc.)

TilePuzzle and Zenithmon additionally bundle `../../../../Zenith/Flux/Shaders` - pre-compiled shaders (`.spv` + `.spv.refl`) for runtime loading. Android has no runtime shader compilation, so a game that omits this cannot render.

### Known Constraints

- **No RTTI** - `typeid` unavailable; use `Zenith_TypeIndex::Of<T>()` for type identification
- **No exceptions** - `try`/`catch` forbidden; use `std::error_code` overloads or return values
- **No `std::function`** - Use function pointers
- **No Slang runtime** - Shaders must be pre-compiled via FluxCompiler
- **No Assimp** - Asset import is `#ifdef ZENITH_TOOLS` only; use pre-baked `.zanim`/`.zmesh` formats
- **Save data** - `Zenith_SaveData` writes under `internalDataPath`. `android_main`
  also `chdir()`s the process there, so plain RELATIVE paths (the boot-profile /
  memory dumps, the unit-test batch's `TestData/`) now resolve to a writable
  location. Without that chdir the cwd is `/`, which is read-only, and
  `std::filesystem` throws with exceptions disabled — i.e. it terminated the
  process mid-`Zenith_Init`.
- **Missing files are RECOVERABLE, not assertions** - `Zenith_Android_FileAccess`
  uses `Zenith_Check` (log + continue) for a failed open, matching the Windows
  sibling, and zeroes the `ulSize` out-param on the failure path. Callers pair
  the two (`Zenith_DataStream::ReadFromFile` asserts
  `m_pData != nullptr || m_ulDataSize == 0`), and the asset registry's contract
  is that `Get<T>()` of a missing path is a clean null.
- **Reading engine-owned files in tests** - go through `Zenith_FileAccess`, never a
  raw `std::ifstream`: on Android the file may be inside the APK, reachable only
  via AAssetManager. (This is what made the IBL shader-source tests fail there.)
- **Surface rotation is handled by the COMPOSITOR, not by pre-rotation** -
  `preTransform` declares the rotation the app has already baked into its
  content, and Zenith bakes none. `Zenith_Vulkan_Swapchain::Initialise` used to
  pass `capabilities.currentTransform` anyway, which is a lie, so any activity
  whose `screenOrientation` differed from the panel's NATIVE orientation rendered
  **sideways** — every landscape game on a portrait phone.

  It now requests **`IDENTITY`** whenever `supportedTransforms` offers it
  (`Flux_SwapchainPolicy::SelectPreTransform`), and the presentation engine
  applies the rotation itself. Correct on every orientation, so a game is free to
  declare whatever `screenOrientation` it wants — only TilePuzzle is `portrait`;
  Combat, DevilsPlayground, RenderTest and Zenithmon are all `landscape`, i.e.
  rotated relative to a portrait-native panel, and all correct.
  If a surface ever refuses `IDENTITY` the code falls back to `currentTransform`
  (the one value the spec guarantees) and logs a warning naming both masks, so a
  sideways image is never unexplained.

  **The cost:** the compositor does an extra rotation pass on a rotated surface,
  where true *pre-rotation* — baking the transform into the projection, viewport
  and scissor — would be free. That remains the optimisation, and it has to be
  threaded through shared render code used by every platform. Correctness first.
  The decision logic is backend-neutral and unit-tested in every config
  (`Flux/Flux_SwapchainPolicy.Tests.inl`), including the Null CI build.
- **Wall-clock perf budgets in tests don't transfer here** - the emulator is far
  slower than a dev box, and per-dispatch overhead can dominate to the point
  that a benchmark's phases converge. `GraphComponent::ThousandEntityUpdateBenchmark`
  is `ZENITH_SKIP`-ped on Android for exactly this reason; it stays fully
  enforced on desktop, where its budgets were calibrated.
