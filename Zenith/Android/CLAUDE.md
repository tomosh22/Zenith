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
| `GetDisplayScale()` | Logical→physical pixel ratio from the activity's density bucket (160 dpi ≡ 1.0; ANY/NONE/0 ≡ 1.0) |
| `GetMousePosition(out&)` | The B3 projected primary-pointer position (owned by `Zenith_Input`, not by the window) |
| `OnTouchEvent(rawAction, pointerId, x, y)` | Platform funnel: raw `AMOTION_EVENT_ACTION_*` → an engine touch event |
| `OnKeyEvent(action, keyCode)` | Platform funnel: `AKEYCODE_BACK` down → a SYSTEM_BACK event |
| `OnLifecycleEvent(bArmed)` | `false` raises a LIFECYCLE_RESET barrier, `true` re-arms |

## Input (the real behaviour — read `Zenith/Input/CLAUDE.md` for the layers)

**Every funnel above only TRANSLATES AND ENQUEUES.** Nothing on this path mutates
input state; the drain at frame-contract step 3 does that, after the swapchain
acquire. That is what makes an event delivered while the app is mid-resize
survive to the next real frame instead of being silently consumed.

### Full multi-touch

`OnInputEvent` handles `AINPUT_EVENT_TYPE_MOTION` with the **whole** pointer
set, not just index 0:

* Source is filtered to `AINPUT_SOURCE_TOUCHSCREEN`, and the tool type to
  FINGER or STYLUS, so a mouse / trackball / joystick motion cannot masquerade
  as a finger.
* `DOWN` / `UP` name exactly ONE pointer. For the primary pair the index is 0;
  for `POINTER_DOWN` / `POINTER_UP` it is carried in the action word
  (`AMOTION_EVENT_ACTION_POINTER_INDEX_MASK/_SHIFT`). **Reading index 0 for a
  POINTER_DOWN attributes the second finger's coordinates to the first** — that
  bug is precisely what kept this platform single-touch.
* `MOVE` carries every live pointer at once, so each is forwarded under its own
  `AMotionEvent_getPointerId`.
* `CANCEL` cancels the lot.

Secondary fingers live in `Zenith_Pointers` (8 slots, generation handles,
claims, taps). Coordinates are RAW SURFACE PIXELS.

### ★ B3: the first touch feeds the mouse view — PERMANENT

The FIRST touch drives the mouse view: position plus
`ZENITH_MOUSE_BUTTON_LEFT` held / press / release, maintained by `Zenith_Input`
at drain. A second finger never steals it.

**This is a permanent design decision, not a compatibility shim.** It is what
keeps every `INPUT_BINDING_MOUSE_BUTTON` row and every unmigrated
`IsMouseButtonHeld` consumer working on a touch device — including whole games
(Combat, RenderTest) that register no TOUCH profile at all. It is claim-aware:
a mouse-button transition fed by a pointer is suppressed at step 10e if a UI
widget claimed that pointer, so a tap the HUD consumed never also reaches
gameplay. Do not "clean it up".

Cursor capture functions are no-ops on Android.

### Keys, and the conditional Back consume

`AINPUT_EVENT_TYPE_KEY` is handled — the old "motion only" claim is dead. There
is still no Android-keycode → Zenith-keycode table, so the only key with meaning
today is the hardware/gesture **BACK**, and it is deliberately NOT a key: it is
system NAVIGATION, so it rides its own `INPUT_EVENT_SYSTEM_BACK` event type and
never lands in the key domain (it is also excluded from the action layer's
activity detection, so pressing Back cannot switch the active input profile).

**Whether Back is CONSUMED is a QUESTION, never a constant.** `OnInputEvent`
returns 1 for `AKEYCODE_BACK` only when
`Zenith_Window::GetInstance()->HasSystemBackBinding()` is true; otherwise it
returns 0 and the platform's own behaviour stands. An unconditional `return 0`
was right while nothing consumed Back — swallowing it then would have left the
activity with no way to finish.

**The question is asked DOWNWARDS, of the device layer.** `Zenith_Input` carries
a sticky flag that `Zenith_InputActions::RegisterBinding` raises when a game
registers an `INPUT_BINDING_SYSTEM_BACK` row (Zenithmon's `Cancel`); the window
class forwards the read exactly as it forwards the funnels. `android_main` is at
layer 0 and Input is at layer 1, so it can no more include an `Input/` header
than it can walk the action layer's registered actions — an earlier version did
walk them and was a tracked `layer-up` architecture violation. Before the game's
registrations run, the flag reads false, which is the safe direction: an un-bound
Back backgrounds the app exactly as it always did.

`INPUT_BINDING_SYSTEM_BACK` is **mask-exempt**: no profile can mask it out.

### Lifecycle barriers

`NotifyInputLifecycle(false)` on `APP_CMD_LOST_FOCUS` / `APP_CMD_PAUSE` /
`APP_CMD_TERM_WINDOW` raises a `LIFECYCLE_RESET`; `true` on
`APP_CMD_GAINED_FOCUS` / `APP_CMD_RESUME` re-arms. A RESET releases every held
key and pad button (emitting the release edges), cancels every live pointer
(raising CANCEL edges), zeroes the pad axes and DISARMS the layer — ordinary
events are discarded until the ARM. **An app that is paused, backgrounded, or
has lost its window must not come back with a finger still "down".** Android is
`INPUT_RECONCILE_CANCEL_EVENT_FED` for the same reason: its devices are
event-fed with no live oracle to resync against after a FIFO overflow, so the
only safe answer is to cancel (Windows resyncs instead).

Both calls are guarded on engine initialisation — the glue delivers commands
before `Zenith_Init` has allocated the subsystems, and `g_xEngine` accessors are
undefined until then.

### Touch-target sizing

`GetDisplayScale()` feeds `Zenith_Pointers::GetDisplayScale()`, which the tap
threshold (15 logical px of excursion) and
`Zenith_UIElement::ResolveTouchTargetRect` (slop + the 57-logical-px minimum
touch target) scale by. Authored UI values are LOGICAL pixels everywhere.

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

### Native STL, page-size alignment, and AGP version

Every game links a **static** STL (not the NDK's shared `c++_shared`) — set
via `UseOfStl = "cpp_static"` in `Build/Sharpmake_Games.cs`. **★ The token is
`cpp_static`, not the NDK-CMake-style `c++_static`** — this MSBuild property
belongs to Google's own AGDE toolset, which uses its own vocabulary (matching
Sharpmake's own emitted default, `cpp_shared`) and silently *ignores* an
unrecognized value instead of erroring, falling back to shared STL. A first
attempt at `c++_static` built clean, packaged clean, and even looked correct
under `llvm-readelf -l` (16 KB segment alignment comes from a separate linker
flag, so that check passed regardless) — but the `.so`'s own `NEEDED` list
still named `libc++_shared.so`, which the APK no longer shipped, so it
crashed on launch with `UnsatisfiedLinkError`. **Verify any change here with
`llvm-readelf -d <so> | grep NEEDED`, not just a successful build or a
zip/alignment check** — none of those would have caught this.

Each APK contains exactly one native `.so`, so there is nothing else in the
process that would need to *share* one copy of the runtime, and static
sidesteps a real problem: the NDK's own prebuilt `libc++_shared.so` predates
16 KB page-size alignment, so bundling it trips Google Play's `AGDE1112`
packaging check. Statically linking the STL into the single game `.so`
removes that prebuilt file from the APK entirely.
Getting `AGDE1112` to clear fully also needed: the linker flag
`-Wl,-z,max-page-size=16384` (same file) so the game `.so` itself is
16 KB-ELF-aligned; `android:extractNativeLibs="false"` in each manifest +
`packagingOptions.jniLibs.useLegacyPackaging = false` in each `app/build.gradle`
so the `.so` is stored uncompressed rather than extracted to disk at install
time; and `android.experimental.enable16kPageSizeSupport=true` in each
`gradle.properties`, which needs **AGP ≥ 8.5.1** (all five games are pinned to
`8.7.3`; AGP 8.2.0 silently ignores that property and never actually
page-aligns the stored entry, even though the other three pieces are in
place). Verify alignment directly rather than trusting any single tool: `llvm-
readelf -l <so>` should show `Align 0x4000` on every `LOAD` segment, and
`unzip -v <apk>` should show the `.so` as `Stored`, not a compression ratio.

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

**★ For the hand-staged three, step 5 above needs to run TWICE, with staging
in between.** The first `msbuild` invocation compiles the native `.so` but
`app/jniLibs/<abi>/` is still empty, so Gradle packages an APK with zero
native code (`mergeAgdeReleaseNativeLibs NO-SOURCE`) — with the dedicated
`agdeRelease` build type carrying `signingConfig signingConfigs.debug`, that
zero-native-code first pass still packages and signs successfully. Production
`release` deliberately remains free for a real release/upload key; without the
separate local variant AGP emits `app-release-unsigned.apk`, which previously
made AGDE's `AGDE1102` fire while looking for its configured package output.
Run
`pwsh Build\deploy_android.ps1 <debug|release> <Game>` to copy the just-built
`.so` into `app/jniLibs/<abi>/`, then run `msbuild` again. **A second
`msbuild` invocation alone is not reliable proof the staged `.so` made it into
the APK** — MSBuild's own AGDE packaging target can decide nothing changed
(the staged file sits outside what it watches for staleness) and skip
re-invoking Gradle, silently leaving the empty-native-code APK on disk. Verify
with `unzip -v <apk> | grep '\.so$'` (see previous section) or by running
Gradle directly: `cd Games\<Game>\Android; .\gradlew.bat :app:assembleAgdeRelease`.

`deploy_android.ps1` (`-Abi <abi dir name>|all`, ABIs from the shared axis)
stages the game `.so` per ABI (see previous section for why there's no NDK
`libc++_shared.so` to stage alongside it). All five games take `abiFilters`
from `Build/zenith_android_abis.gradle`, so an ABI that was never built is
simply absent from the APK rather than an error.

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
