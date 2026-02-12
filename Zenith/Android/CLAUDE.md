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
