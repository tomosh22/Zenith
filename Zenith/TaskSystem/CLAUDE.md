# Task System

## Files

- `Zenith_TaskSystem.h` - Task and TaskArray declarations
- `Zenith_TaskSystem.cpp` - Implementation

## Overview

Work-stealing task system for parallelizing CPU work across worker threads. Uses lock-free circular queue and semaphore synchronization.

## Core Classes

### Zenith_Task
Executes a single function on a worker thread. Constructor takes profile index, function pointer, and user data. Provides `WaitUntilComplete()` for synchronization.

### Zenith_TaskArray
Data-parallel work distribution. Extends Zenith_Task. Work items distributed atomically via fetch-add across worker threads. Optional "submitting thread joins" allows main thread to participate in work.

### Zenith_TaskSystem
Static submission API. Call `SubmitTask()` or `SubmitTaskArray()` to queue work.

## Configuration

Worker threads defined in `Core/ZenithConfig.h` as `FLUX_NUM_WORKER_THREADS = 8`.

Task system dynamically creates `min(hardware_concurrency - 1, 16)` threads at initialization.

## Related Components

- `Zenith_Multithreading` in `Core/Multithreading/` - Thread registration and ID tracking
- `Zenith_Mutex` in `Windows/Multithreading/` - Platform mutex wrapper
- `Zenith_Semaphore` in `Windows/Multithreading/` - Platform semaphore wrapper
- `Zenith_CircularQueue` in `Collections/` - Lock-free queue implementation

## Key Concepts

**Profiling Integration:** Every task tagged with profile index for automatic performance tracking.

**Reusable Tasks:** Command lists and other systems reuse task objects across frames to avoid allocation overhead.

**Submitting Thread Joining:** TaskArray can optionally have submitting thread participate in work execution, useful when main thread would otherwise idle.
