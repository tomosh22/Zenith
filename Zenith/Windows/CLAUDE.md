# Windows Platform Layer

## Overview

Windows-specific implementations for windowing (GLFW), threading (CRITICAL_SECTION), and callstack capture (DbgHelp).

## Files

- `Zenith_Windows_Window.h` - GLFW-based window (singleton)
- `Zenith_OS_Include.h` - Platform aggregator, defines `Zenith_Mutex`/`Zenith_Semaphore` aliases
- `Multithreading/Zenith_Windows_Multithreading.h` - Mutex and semaphore wrappers
- `Callstack/Zenith_Windows_Callstack.h` - DbgHelp-based stack trace capture

## Window (Zenith_Window)

Singleton wrapping `GLFWwindow*`.

| Function | Description |
|----------|-------------|
| `Inititalise(title, w, h)` | Create window (static factory) |
| `GetInstance()` | Singleton accessor (asserts initialized) |
| `GetNativeWindow()` | Returns `GLFWwindow*` for Vulkan surface |
| `BeginFrame()` | Poll events, update state |
| `ShouldClose()` | GLFW close flag |
| `GetSize(w&, h&)` | Window dimensions |
| `GetMousePosition(out&)` | Mouse position |
| `IsKeyDown(key)` | Key state |
| `ToggleCaptureCursor()` | Toggle mouse capture |
| `SetVSync(bool)` | VSync control |
| `GetGLFWMemoryAllocated()` | GLFW allocation tracking |

## Multithreading

### Zenith_Windows_Mutex_T\<bEnableProfiling\>

Template wrapper around `CRITICAL_SECTION`. Default alias `Zenith_Windows_Mutex` uses `true` (profiling enabled). `Zenith_Mutex_NoProfiling` uses `false`.

| Function | Description |
|----------|-------------|
| `Lock()` | `EnterCriticalSection` (blocking) |
| `TryLock()` | `TryEnterCriticalSection` (non-blocking, returns bool) |
| `Unlock()` | `LeaveCriticalSection` |

Critical sections are recursive (same thread can acquire multiple times).

### Zenith_Windows_Semaphore

Wraps Windows `HANDLE` semaphore.

| Function | Description |
|----------|-------------|
| Constructor | `CreateSemaphoreA(initial, max)` |
| `Wait()` | `WaitForSingleObject` (blocking) |
| `TryWait()` | Non-blocking wait |
| `Signal()` | `ReleaseSemaphore` |

## Callstack (Zenith_Windows_Callstack)

Uses DbgHelp API for symbol resolution.

| Function | Description |
|----------|-------------|
| `Initialise()` | `SymInitialize` setup |
| `Capture(frames, max, skip)` | `CaptureStackBackTrace` |
| `Symbolicate(addr, frame&)` | `SymFromAddr` + `SymGetLineFromAddr64` |

Thread-safe via dedicated non-profiling mutex (DbgHelp is not thread-safe).

## Platform Aliases (Zenith_OS_Include.h)

```cpp
#define Zenith_Mutex Zenith_Windows_Mutex
#define Zenith_Mutex_NoProfiling Zenith_Windows_Mutex_T<false>
#define Zenith_Semaphore Zenith_Windows_Semaphore
```
