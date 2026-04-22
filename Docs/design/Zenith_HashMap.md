# Zenith_HashMap / Zenith_HashSet — Design

## Context

18+ `#TODO: Replace std::unordered_map with engine hash map` comments sit across hot paths (`Flux_RenderGraph`, `Zenith_EventSystem`, `Zenith_Vulkan_MemoryManager`, `Zenith_PerceptionSystem`, `Zenith_SceneData`, etc.). The `Collections/` module already provides `Zenith_Vector`, `Zenith_MemoryPool`, and `Zenith_CircularQueue` with consistent conventions (placement-new zone protection, `Zenith_MemoryManagement::Allocate`, DataStream serialisation, custom iterator pattern, no STL iterators). The hash map/set is the missing companion.

Target users:
- Render Graph (`Flux_BarrierKey` → `ResourceAccess` / `bool`, `u_int64` edge sets)
- Event System (`Zenith_EventHandle` → `Subscription`)
- Vulkan MemoryManager (`u_int64` probe cache)
- AI Perception (`uint64_t` agent/target tables)
- AssetRegistry (`std::string` → asset pointer, `Zenith_TypeIndex` → loader fn)
- FileWatcher, ScriptComponent, Blackboard (string-keyed maps)
- Scene Data entity-component mapping (`TypeID` → pool index)

## Goals

1. **API-compatible migration target** — swapping `std::unordered_map<K,V>` for `Zenith_HashMap<K,V>` at call sites is mechanical (same methods, equivalent semantics).
2. **Consistent with `Zenith_Vector`** — same allocator routing, same zone discipline, same iterator pattern, same serialisation hooks.
3. **Works with existing custom hash specialisations** — `Flux_BarrierKey` already ships a `std::hash<Flux_BarrierKey>` specialisation; we reuse it by default.
4. **Correct before fast** — open-addressing with linear probing. Robin Hood is nice but adds complexity; defer unless measurement shows probe clustering.

## Non-goals

- Not a general-purpose STL replacement. No iterators compatible with `<algorithm>`.
- No `node_type`/extract API.
- No unique ownership semantics beyond what keys/values already provide.
- No lock-free / thread-safe variant. Callers synchronise externally, same as `Zenith_Vector`.

## API surface

### `Zenith_HashMap<K, V, Hasher = Zenith_Hash<K>>`

```cpp
// Construction
Zenith_HashMap();                              // default capacity
explicit Zenith_HashMap(u_int uInitialCapacity);
Zenith_HashMap(const Zenith_HashMap&);
Zenith_HashMap(Zenith_HashMap&&);
Zenith_HashMap& operator=(const Zenith_HashMap&);
Zenith_HashMap& operator=(Zenith_HashMap&&);
~Zenith_HashMap();

// Size / capacity
u_int GetSize() const;
u_int GetCapacity() const;
bool IsEmpty() const;

// Mutation
V& Insert(const K& xKey, const V& xValue);     // replaces existing
V& Insert(const K& xKey, V&& xValue);
template<typename... Args>
V& Emplace(const K& xKey, Args&&... args);
bool Remove(const K& xKey);                    // returns true if erased
void Clear();
void Reserve(u_int uNewCapacity);

// Lookup
V& Get(const K& xKey);                         // asserts if missing
const V& Get(const K& xKey) const;
V* TryGet(const K& xKey);                      // nullptr if missing
const V* TryGet(const K& xKey) const;
bool Contains(const K& xKey) const;

// Convenience (matches std::unordered_map)
V& operator[](const K& xKey);                  // default-constructs V if missing

// Iteration — custom iterator matching Zenith_Vector pattern
class Iterator
{
public:
    explicit Iterator(const Zenith_HashMap& xMap);
    void Next();
    bool Done() const;
    const K& GetKey() const;
    const V& GetValue() const;
    V& GetValueMutable();
};

// Serialisation
void ReadFromDataStream(Zenith_DataStream& xStream);
void WriteToDataStream(Zenith_DataStream& xStream) const;
```

### `Zenith_HashSet<K, Hasher = Zenith_Hash<K>>`

Same surface with no value type. `Insert(K)` / `Remove(K)` / `Contains(K)` / `Iterator::GetKey()`.

### `Zenith_Hash<K>` — the hash traits

```cpp
template<typename K>
struct Zenith_Hash
{
    u_int64 operator()(const K& xKey) const noexcept
    {
        // Default: delegate to std::hash<K>. This keeps existing
        // std::hash<Flux_BarrierKey> specialisations working after migration.
        return static_cast<u_int64>(std::hash<K>{}(xKey));
    }
};
```

Users can specialise `Zenith_Hash<MyKey>` when they want to bypass STL (e.g., for types that don't have a `std::hash` specialisation). Specialisations are discovered via ADL/partial ordering.

**Key types exercised day-one:** `u_int`, `u_int32`, `u_int64`, `void*`, `std::string`, `Flux_BarrierKey`, `Zenith_EventHandle`, `Zenith_EntityID`, `Zenith_TypeIndex` — all have `std::hash` specialisations or trivially will.

## Implementation details

### Storage layout

Parallel arrays in a single heap allocation:
- `K* m_pxKeys`
- `V* m_pxValues`
- `u_int8* m_puMeta` — per-slot state byte: `{ EMPTY, OCCUPIED, TOMBSTONE }` + (later) the low 6 bits of hash for fast compare

Single contiguous block, split by offset: `sizeof(K) * N + sizeof(V) * N + sizeof(u_int8) * N` (rounded for alignment). Allocated via `Zenith_MemoryManagement::Allocate`.

Placement-new for key/value construction; manual destructor calls on removal. No per-entry heap node allocation (matches `Zenith_Vector`).

### Collision strategy

**Linear probing.** On collision, step forward by 1 until an EMPTY or TOMBSTONE slot is found.

- **Rationale:** cache-friendly; simple invalidation rules; no per-entry metadata beyond the state byte.
- **Tombstones:** lookup probes past them; insert reuses the first tombstone seen in the probe chain.
- **Load factor ceiling:** 0.75 (rehash + double when exceeded).
- **Rehash behaviour:** triggered by insertion crossing the load factor OR by explicit `Reserve`. Walks existing occupied slots and re-inserts into the new table. Tombstones are cleared during rehash.

Future consideration: Robin Hood probing (swap-on-insert based on displacement). Adds complexity; defer unless the render graph's `m_xAttachmentNeedsClear` shows probe-chain pathology in profiling. Any migration is transparent to callers.

### Hash → slot mapping

`slot = hash & (capacity - 1)` (capacity kept a power of two). Slot-byte high bit distinguishes EMPTY (0) from non-empty (1); remaining bits reserved for future hash caching.

### Iterator invalidation

- **Invalidated by:** `Insert` (if rehash triggered), `Reserve`, `Clear`, destructor.
- **Not invalidated by:** `Get`, `TryGet`, `Contains`, `Remove` (tombstones are fine).
- **Detection:** generation counter (`m_uGeneration`) incremented on rehash/clear. Iterator snapshots it; asserts on mismatch — matches `Zenith_Vector::Iterator`.
- **Order is unspecified but stable across non-mutating calls.**

### Serialisation

`WriteToDataStream`: emit size, then iterate and emit `(K, V)` pairs. No layout/hash info — the reader rebuilds the table.

`ReadFromDataStream`: read size, assert within `uMAX_REASONABLE_SIZE` (matches `Zenith_Vector`), `Clear` + `Reserve(size)`, read pairs, `Insert` each. Handles hasher/capacity changes across versions without layout coupling.

## Memory and zone discipline

Exactly mirrors `Zenith_Vector.h`:

```cpp
#ifdef ZENITH_PLACEMENT_NEW_ZONE
#define ZENITH_HASHMAP_ZONE_WAS_SET
#else
#define ZENITH_PLACEMENT_NEW_ZONE
#endif
#include "Memory/Zenith_MemoryManagement_Disabled.h"

// ... entire class body ...

#ifndef ZENITH_HASHMAP_ZONE_WAS_SET
#undef ZENITH_PLACEMENT_NEW_ZONE
#endif
#undef ZENITH_HASHMAP_ZONE_WAS_SET
#include "Memory/Zenith_MemoryManagement_Enabled.h"
```

All allocations route through `Zenith_MemoryManagement::Allocate` / `Deallocate`. Never `new[]` / `delete[]` (per `Core/CLAUDE.md` — mixing `malloc`/`new[]` families causes heap corruption). Placement-new for in-place construction, explicit destructor calls on remove.

## Assertions

Per project convention (`Zenith_Assert` is always defined; asserts + surrounding code compile away together):

- `Insert` / `Emplace`: assert capacity overflow detection against `uMAX_SAFE_CAPACITY = UINT_MAX / (sizeof(K) + sizeof(V) + 1)`
- `Get`: assert key present
- `Iterator::Next/Done/GetKey/GetValue`: assert generation match
- `ReadFromDataStream`: assert size ≤ 100,000,000 (matches `Zenith_Vector`)

## File layout

- `Zenith/Collections/Zenith_HashMap.h` — primary container + `Zenith_Hash<K>` default template
- `Zenith/Collections/Zenith_HashSet.h` — thin wrapper over `Zenith_HashMap<K, EmptyValue>` OR a standalone parallel implementation (TBD during implementation — start with wrapper, measure, promote to standalone if the dummy-value overhead matters)
- `Zenith/UnitTests/Zenith_CollectionsTests.cpp` — new file, unit tests

Both headers are included directly by callers; neither is added to `Zenith.h` (the PCH already has `<unordered_map>` / `<unordered_set>`, which will remain until all migrations complete). After all migrations are done, a future PR can evaluate removing those STL includes from `Zenith.h`.

## Testing

Unit tests in `Zenith_CollectionsTests.cpp`:

1. **Basic correctness:** insert/get/contains/remove round-trips with `u_int`, `std::string`, `Flux_BarrierKey` keys
2. **Collision handling:** force 20+ keys hashing to the same slot (custom hasher returning constant); verify all retrievable
3. **Rehash:** insert past load factor; verify all existing keys still retrievable; verify `GetCapacity()` doubled
4. **Tombstone handling:** insert A, remove A, insert B (possibly same slot), lookup B, lookup A returns missing
5. **Iterator:** iterate a populated map; confirm every inserted pair visited exactly once; assert iterator fires on post-insert use
6. **Iterator invalidation:** construct iterator, trigger rehash via insert, next `Done()` / `Next()` fires assertion
7. **Move semantics:** move-construct, move-assign; source is empty, destination has the data
8. **Copy semantics:** copy-construct, copy-assign; independent modifications
9. **DataStream round-trip:** insert 100 entries, write, clear, read, confirm all present
10. **HashSet parity:** basic insert/contains/remove tests for the set variant
11. **`operator[]`:** access missing key returns default-constructed value; subsequent access returns same reference

## Rollout sequence

1. Land the two headers + unit tests (this plan's T0.1 Step B).
2. Validation migration: ONE call site — pick `Flux_RenderGraph.h:510` `m_xAttachmentClearAssigned` (`std::unordered_set<Flux_BarrierKey>`). Proves the custom-hash integration path.
3. Smoke test: boot any game with tools enabled, one frame, no Vulkan validation errors.
4. Downstream migrations (T1.4): RenderGraph full migration, EventSystem, ProbeCache, etc. — one PR per subsystem.

## Open questions that the implementation must resolve

1. **Does `Zenith_EntityID` (used in `s_xSelectedEntityIDs` and other editor sets) already have a `std::hash` specialisation?** Grep shows yes — reuse it via default `Zenith_Hash`.
2. **Should `Zenith_HashSet` be a thin wrapper over `Zenith_HashMap<K, char>` or a standalone template?** Start with wrapper; measure if the dummy-value overhead shows up in profiling before promoting.
3. **How does Halstead/complexity budgeting treat this file?** Likely ~400–500 LOC total. Well under the `CheckComplexity.bat` ceilings. Re-run the tool post-implementation to confirm.

## Risks

- **Hash quality for `Flux_BarrierKey`:** existing `std::hash<Flux_BarrierKey>` uses Boost-style combine on pointer + packed mip/layer. Good enough; no change needed.
- **Iterator stability during migration:** `std::unordered_map` callers often iterate and erase concurrently. `Zenith_HashMap::Remove` does not invalidate iterators (tombstones), matching semantics. Must be verified in the RenderGraph migration — one of the hotter iterate-and-modify sites.
- **Template bloat:** each `K,V` instantiation produces a copy of all methods. The `Zenith_Vector` approach is the same; accept it. If binary size regression shows up, consider type-erasing the allocator path in a private base.
