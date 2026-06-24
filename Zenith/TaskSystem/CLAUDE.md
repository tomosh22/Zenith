# Task System

## Files

- `Zenith_TaskSystem.h` - Task and data-parallel task declarations
- `Zenith_TaskSystem.cpp` - Implementation

## Overview

Shared task queue system for parallelizing CPU work across a worker thread pool. A single centralized circular queue (protected by one global mutex) is drained by all workers, with semaphore synchronization.

## Core Classes

### Zenith_Task
Executes a single function on a worker thread. Constructor takes profile index, function pointer, and user data. Provides `WaitUntilComplete()` for synchronization.

### Zenith_DataParallelTask
ONE task executed N times. Extends Zenith_Task. Invocation indices claimed atomically via fetch-add across worker threads. Optional "calling thread participates" allows the submitting thread to run invocations too.

### Zenith_TaskSystem
Submission API on the engine instance. Call `g_xEngine.Tasks().SubmitTask()` or `SubmitDataParallelTask()` to queue work. A task is recycled (resubmittable) by `WaitUntilComplete()` or by a failed submit (queue full).

## Configuration

Task system dynamically creates worker threads at initialization: one per hardware thread minus 1 (the main thread is reserved), with at least 1 worker and capped at 16. Formula: `(hardware_concurrency > 1) ? (hardware_concurrency - 1) : 1`, then clamped to a maximum of 16.

The task queue has a fixed capacity of `uMAX_TASKS` (128); a submit fails (returning the task as resubmittable) when the queue is full.

`GetNumWorkerThreads()` returns the worker count for sizing a `Zenith_DataParallelTask`'s invocation count. It may be 0 before `Initialise()` / on a single-core box, so callers must clamp to at least 1.

## Related Components

- `Zenith_Multithreading` in `Core/Multithreading/` - Thread registration and ID tracking
- `Zenith_Mutex` - Platform mutex wrapper (alias defined in `Windows/Zenith_OS_Include.h`, implementation in `Windows/Multithreading/`)
- `Zenith_Semaphore` - Platform semaphore wrapper (alias defined in `Windows/Zenith_OS_Include.h`, implementation in `Windows/Multithreading/`)
- `Zenith_CircularQueue` in `Collections/` - Circular queue implementation (not thread-safe, requires external synchronization)

## Key Concepts

**Profiling Integration:** Every task tagged with profile index for automatic performance tracking.

**Reusable Tasks:** Command lists and other systems reuse task objects across frames to avoid allocation overhead.

**Calling Thread Participation:** Zenith_DataParallelTask can optionally have the submitting thread run invocations itself, useful when the main thread would otherwise idle.
