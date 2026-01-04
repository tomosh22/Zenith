# Collections

Custom container implementations optimized for engine use.

## Files

- `Zenith_Vector.h` - Dynamic array
- `Zenith_MemoryPool.h` - Fixed-capacity object pool
- `Zenith_CircularQueue.h` - Fixed-capacity FIFO queue

## Zenith_Vector<T>

Dynamic array with automatic resizing. Provides standard array operations: push/pop, indexed access, iteration, reserve, clear. Supports serialization via `WriteToDataStream()` and `ReadFromDataStream()`.

Custom iterator pattern: construct iterator with vector, call `Done()` to check completion, `Next()` to advance, `GetData()` to access element.

## Zenith_MemoryPool<T, uCount>

Fixed-capacity object pool for frequent alloc/dealloc scenarios. Template parameters specify element type and compile-time capacity. Pre-allocates single contiguous block with free-list tracking. O(1) allocation and deallocation. In-place construction via placement new. Validates pointers on deallocate with bounds checking and double-free detection.

Non-copyable. Ideal for frequently recycled objects like tasks, entities, command buffers.

## Zenith_CircularQueue<T, uCapacity>

Fixed-capacity FIFO queue with circular buffer. Template parameter specifies compile-time capacity. Stack-allocated buffer. O(1) enqueue/dequeue using modulo arithmetic for wrapping. Methods return bool for success/failure (no exceptions). Not thread-safe - requires external synchronization for multi-threaded use.

## Container Selection

| Container | Capacity | Growth | Use Case |
|-----------|----------|--------|----------|
| Vector | Dynamic | Exponential | General-purpose dynamic arrays |
| MemoryPool | Fixed | None | Object recycling, known max count |
| CircularQueue | Fixed | None | FIFO queue, producer-consumer (with external sync) |

## Key Concepts

**Serialization:** Vector supports DataStream serialization for saving/loading container contents.

**Iterator Safety:** Vector iterator pattern prevents out-of-bounds access via `Done()` check.
