# Collections

Custom container implementations optimized for engine use.

## Files

- `Zenith_Vector.h` - Dynamic array
- `Zenith_HashMap.h` - Open-addressed hash map
- `Zenith_MemoryPool.h` - Fixed-capacity object pool
- `Zenith_CircularQueue.h` - Fixed-capacity FIFO queue

## Zenith_Vector<T>

Dynamic array with automatic resizing. Provides standard array operations: push/pop, indexed access, iteration, reserve, clear. Supports serialization via `WriteToDataStream()` and `ReadFromDataStream()`.

Custom iterator pattern: construct iterator with vector, call `Done()` to check completion, `Next()` to advance, `GetData()` to access element.

## Zenith_HashMap<K, V, Hasher>

Open-addressed hash map with linear probing and tombstone-based deletion. Dynamic capacity (lazy first-insert allocation, power-of-two growth). The default `Zenith_Hash<K>` trait delegates to `std::hash<K>`, so any type with a `std::hash` specialisation (including `Zenith_EntityID`) works out of the box. Specialise `Zenith_Hash<MyKey>` to bypass STL hash for types that don't ship one.

API:
- `Insert(k, v)` / `Emplace(k, args...)` — insert or overwrite
- `operator[](k)` — std::unordered_map-style default-construct-on-miss
- `TryGet(k)` — pointer-or-null lookup (preferred over throwing `Get`)
- `Get(k)` — asserts on miss
- `Contains(k)` — bool lookup
- `Remove(k)` — bool returning success
- `GetSize()`, `GetCapacity()`, `IsEmpty()`, `Clear()`, `Reserve()`
- `WriteToDataStream` / `ReadFromDataStream` for save/load
- `Iterator` — generation-guarded; asserts on mid-iteration rehash, so erase-while-iterating must use a two-pass collect-then-remove pattern

Iteration order is *slot order*, dependent on hash distribution and current capacity — it is not insertion-order and differs from `std::unordered_map`'s bucket order. Don't rely on iteration order for correctness.

## Zenith_MemoryPool<T, uCount>

Fixed-capacity object pool for frequent alloc/dealloc scenarios. Template parameters specify element type and compile-time capacity. Pre-allocates single contiguous block with free-list tracking. O(1) allocation and deallocation. In-place construction via placement new. Validates pointers on deallocate with bounds checking and double-free detection.

Non-copyable. Ideal for frequently recycled objects like tasks, entities, command buffers.

## Zenith_CircularQueue<T, uCapacity>

Fixed-capacity FIFO queue with circular buffer. Template parameter specifies compile-time capacity. Stack-allocated buffer. O(1) enqueue/dequeue using modulo arithmetic for wrapping. Methods return bool for success/failure (no exceptions). Not thread-safe - requires external synchronization for multi-threaded use.

## Container Selection

| Container | Capacity | Growth | Use Case |
|-----------|----------|--------|----------|
| Vector | Dynamic | Exponential | General-purpose dynamic arrays |
| HashMap | Dynamic | Power-of-two | Key-value lookup; replaces `std::unordered_map` |
| MemoryPool | Fixed | None | Object recycling, known max count |
| CircularQueue | Fixed | None | FIFO queue, producer-consumer (with external sync) |

## Key Concepts

**Serialization:** Vector supports DataStream serialization for saving/loading container contents.

**Iterator Safety:** Vector iterator pattern prevents out-of-bounds access via `Done()` check.
