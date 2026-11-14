# CORE design: memory implementation

This file is part of the CORE design document family. The entry document is `Zenith2/Docs/Design/CORE.md`. It specifies the implementation of ZenithCore's memory tier beyond what `Zenith2/Docs/Contracts/Memory.md` already fixes: the general-heap backing on both D-SEL-HEAP (open, Stage 1) paths, statistics, fault injection, the out-of-memory relay into the memory governor, the arena and pool internals, `Zenith_Function`, the generational slot table that backs every CORE-owned handle, and the fault-injector test support. It never restates a `Zenith2/Docs/Contracts/Memory.md` bullet; it cites the heading and adds what a worker needs to write the code.

## Scope of this file

| Topic | Already fixed by | Added here |
|---|---|---|
| `Zenith_Heap` public API, error and OOM types | `Zenith2/Docs/Contracts/Memory.md` → "Zenith_Heap" | backing selection, per-platform backing calls, statistics, fault-injection counter, emergency reserve, default handler steps |
| `Zenith_Owned`, `Zenith_New`, `Zenith_TryNew`, `Zenith_Delete` | `Zenith2/Docs/Contracts/Memory.md` → "Zenith_Owned", "Zenith_New Zenith_TryNew and Zenith_Delete", "Owned conversion to a base" | file placement, the example file, the test list |
| OOM handling | `Zenith2/Docs/Contracts/Memory.md` → "OOM handler" | the `Zenith_OOMPressureFlag` relay object and its protocol |
| arenas | `Zenith2/Docs/Contracts/Memory.md` → "Zenith_Arena" | block list layout, marker semantics, accounting, counters |
| pools | `Zenith2/Docs/Contracts/Memory.md` → "Zenith_Pool" | page layout, free-list layout, quarantine ring |
| leak table, sanitizer rules, defect tests | `Zenith2/Docs/Contracts/Memory.md` → "Debug leak side table", "Sanitizer instrumentation", "ASan poisoning rules", "HWASan tagging rules", "Required intentional-defect tests" | nothing beyond file placement |
| `Zenith_Function` | named by `Zenith2/Docs/Contracts/Memory.md` → "Zenith_Arena" as specified in CORE | the whole type |
| generational handles | `Zenith2/Docs/Contracts.md` → "Handle kinds" | `Zenith_SlotTable<T>`, `Zenith_SlotID` |
| failure injection | `Zenith2/Docs/Contracts/Memory.md` → "Fallible construction" | `Zenith_TestFaultInjector`, the fault-point enumeration helper |

## Heap backing

### Backing selection

The backing is chosen once per configuration and never at run time:

| Configuration | Backing | Reason |
|---|---|---|
| sanitizer builds (`win-asan` `Sanitize`, `linux-tsan` `Sanitize`, `android-arm64-hwasan` `Hwasan`) | system allocator, always | the sanitizers instrument the system allocator natively (`Zenith2/Docs/Contracts/Memory.md` → "Sanitizer instrumentation") |
| every other configuration, before the D-SEL-HEAP ruling | system allocator | the established alternative is the default until a measurement shows otherwise |
| every other configuration, after the ruling | the ruled backing: the system allocator, or mimalloc | D-SEL-HEAP (open, Stage 1) |

The selection is a behavioural difference, never a layout difference (`Zenith2/Docs/Contracts.md` → "No layout-changing conditionals"). It is written as one `constexpr` function in `Zenith2/Engine/ZenithCore/Internal/Memory/Zenith_HeapBacking.h`:

```cpp
enum Zenith_HeapBackingKind : uint8
{
	HEAP_BACKING_KIND_SYSTEM = 1,
	HEAP_BACKING_KIND_MIMALLOC,
};

constexpr Zenith_HeapBackingKind Zenith_GetHeapBackingKind()
{
	if constexpr (Zenith_IsSanitizerBuild())
	{
		return HEAP_BACKING_KIND_SYSTEM;
	}
	return uZENITH_HEAP_RULED_BACKING;   // constexpr in Zenith_HeapBacking.h; HEAP_BACKING_KIND_SYSTEM until the D-SEL-HEAP ruling edits it
}
```

The ruling ticket of D-SEL-HEAP edits the value of `uZENITH_HEAP_RULED_BACKING` in the same diff that appends its ruling to `Zenith2/Docs/DecisionLog.md`. When mimalloc wins, the ruling ticket also adds the mimalloc entry to `Zenith2/ThirdParty/manifest.json` with owner `ZenithCore` and adds `THIRD_PARTY mimalloc` to ZenithCore's `zenith_add_library` call; mimalloc is then linked in every preset (including sanitizer presets, where it is compiled from source and never called), so no source list differs between presets.

### Backing calls per platform

`Zenith_HeapBacking.h` declares four private functions implemented per platform and per backing; each is the only place its backing call appears (`Zenith2/Docs/Contracts/Memory.md` → "Bare allocation boundary").

```cpp
// Zenith2/Engine/ZenithCore/Internal/Memory/Zenith_HeapBacking.h
void* Zenith_BackingAllocate(uint64 ulSize, uint64 ulAlignment);   // nullptr on failure
void Zenith_BackingFree(void* pMemory);                                            // nullptr is a no-op
uint64 Zenith_BackingUsableSize(const void* pMemory);
void Zenith_BackingShutdown(Zenith_HeapBackingState& xState);   // frees the side table's storage (Windows system backing)
```

There is no backing initialisation call: the heap constructor's only backing call is the emergency reserve's allocation ("Emergency reserve and the default handler"), and the Windows side table is allocated by the first over-aligned allocation, so constructing a heap cannot fail on any platform ([Z2-N-CONTRACT-199]).

| Platform file | System backing | mimalloc backing |
|---|---|---|
| `Internal/Windows/Zenith_HeapBacking_Windows.cpp` | alignment ≤ 16: `malloc`, `free`, `_msize`; alignment > 16: `_aligned_malloc`, `_aligned_free`, `_aligned_msize(p, uAlignment, 0)`, with the block recorded in the over-aligned side table (below) | `mi_malloc_aligned`, `mi_free`, `mi_usable_size` for every alignment |
| `Internal/Android/Zenith_HeapBacking_Android.cpp` | alignment ≤ 16: `malloc`; alignment > 16: `posix_memalign`; `free` and `malloc_usable_size` for both | as Windows |
| `Internal/Linux/Zenith_HeapBacking_Linux.cpp` | as Android | not built: `linux-tsan` is a sanitizer build and uses the system backing only |

<!-- informative -->
The Windows CRT frees an `_aligned_malloc` block only with `_aligned_free`, and `_msize` does not accept one. Because `Zenith_Heap::Free` takes a bare pointer (`Zenith2/Docs/Contracts/Memory.md` → "Zenith_Heap"), the system backing on Windows must recover the family of a block from the pointer alone. A side table keyed by address does that without adding a header to any allocation and without changing any object's layout; it exists on the Windows system path only, because bionic, glibc and mimalloc free every alignment through one call.
<!-- /informative -->

### Over-aligned side table

The over-aligned side table exists only in the Windows system backing.

| Property | Value |
|---|---|
| layout | open addressing, linear probing, keys are block addresses (`uint64`, 0 = empty), value is the alignment exponent (`uint8`) |
| capacity | 0 until the first over-aligned allocation, which allocates 1,024 entries; a power of two from then on, doubling when the load factor would exceed 0.5 |
| deletion | backward-shift, so no tombstones accumulate (the same algorithm as the leak table, `Zenith2/Docs/Contracts/Memory.md` → "Debug leak side table") |
| storage | from `HeapAlloc(GetProcessHeap(), …)` directly, never from the root heap, so recording never recurses |
| lock | `uLOCK_LEVEL_HEAP_ALIGNED_TABLE` = 980, short-hold, no same-level pairing (a new row of `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", table "Lock ordering table", added by the ticket that lands this file) |
| fast path | an `std::atomic<uint64>` count of live over-aligned blocks; `Free` consults the table only when the count is non-zero and the pointer is 32-byte aligned |
| failure | a failure of the first allocation or of a growth makes the allocating call fail as a backing failure (`TryAllocate` returns `ALLOC_ERROR_OUT_OF_MEMORY`; `Allocate` enters the OOM path) before the aligned block is allocated; the table is unchanged |

`Zenith_BackingFree(p)` on the Windows system path runs: if the live count is zero or `p % 32 != 0`, call `free(p)`; otherwise take the lock, probe for `p`; if found, remove the entry, decrement the count, release the lock and call `_aligned_free(p)`; if not found, release the lock and call `free(p)`. `Zenith_BackingUsableSize` follows the same lookup and calls `_msize(p)` or `_aligned_msize(p, uAlignment, 0)` with the recorded alignment and offset 0.

`Zenith_HeapBackingState` is a private member of `Zenith_Heap`, so it is declared in the public header `Zenith_Heap.h` ([Z2-N-CONTRACT-026]: a public class holds no `Internal/` type by value). Its constructor is defined in `Internal/Memory/Zenith_Heap.cpp`, where it constructs the lock from the `Internal/` constant `uLOCK_LEVEL_HEAP_ALIGNED_TABLE` ([Z2-N-CONTRACT-318]); the public header names no lock level. It is identical in every configuration, and the members a backing does not use stay zero:

```cpp
// Zenith2/Engine/ZenithCore/Zenith_Heap.h
/// @contract type Zenith_HeapBackingState
/// Ownership: A by-value member of Zenith_Heap only; constructed and destroyed with the heap.
/// Thread affinity: ANY_THREAD; m_ulAlignedLiveCount is atomic and the side table is guarded by m_xAlignedTableLock.
/// Invalidation: None.
/// Units: Entries for the capacity; blocks for the live count.
/// Errors: None; the constructor allocates nothing.
/// Version: since Z2 Stage 2. Layout identical in every configuration (ForbiddenCheck F12).
/// @endcontract
struct Zenith_HeapBackingState
{
	Zenith_HeapBackingState();            // Internal/Memory/Zenith_Heap.cpp: m_xAlignedTableLock{ Zenith_LockDesc{ "heap.aligned_table", uLOCK_LEVEL_HEAP_ALIGNED_TABLE, true, false } }
	void* m_pAlignedTable = nullptr;               // Windows system backing: the over-aligned side table's storage, null until first use
	uint32 m_uAlignedTableCapacity = 0;           // 0 until first use, then a power of two of at least 1,024
	std::atomic<uint64> m_ulAlignedLiveCount{ 0 }; // live over-aligned blocks (the fast-path filter)
	Zenith_Mutex m_xAlignedTableLock;              // constructed by the constructor above
	void* m_pEmergencyReserve = nullptr;           // the reserve block, until released
};
```

The mimalloc backing and the Android and Linux system backings use only `m_pEmergencyReserve`.

### Statistics

`Zenith_HeapStats` (`Zenith2/Docs/Contracts/Memory.md` → "Zenith_Heap") is maintained with `std::atomic<uint64>` fields updated with `memory_order_relaxed`, because the statistics order nothing:

| Field | Updated when |
|---|---|
| `m_ulLiveBytes` | `+= Zenith_BackingUsableSize(p)` after a successful allocation; `-=` the same value before the block is freed |
| `m_ulLiveAllocations` | `+= 1` / `-= 1` at the same points |
| `m_ulPeakBytes` | a compare-exchange loop raising it to the new `m_ulLiveBytes` when that exceeds it |
| `m_ulTotalAllocations` | `+= 1` on every successful allocation |
| `m_ulFailedAllocations` | `+= 1` on every failed attempt, including an injected one and including each OOM retry that fails |

`GetStats()` reads each atomic once; the returned snapshot may be torn across fields and is documented so in the type's contract block.

### Fault-injection counter

`Zenith_HeapDesc::m_ulFailAfterAllocations` (`Zenith2/Docs/Contracts/Memory.md` → "Zenith_Heap") is copied into an `std::atomic<uint64> m_ulFaultCountdown` at construction. Each `TryAllocate` and `Allocate` call, before calling the backing, performs `fetch_sub(1)` when the countdown is non-zero; the call whose decrement takes the value from 1 to 0 fails as if the backing had returned nullptr (so `Allocate` reaches the OOM handler and `TryAllocate` returns `ALLOC_ERROR_OUT_OF_MEMORY`), and every other call proceeds. `Zenith_Heap::SetFaultCountdown(uint64 ulFailAfter)` re-arms the countdown; it is used only by test code (`Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates", "Test separation": the method is public API reached by tests, not a test switch).

- The countdown counts heap calls of the heap instance it belongs to, on every thread; a failure-injection test that needs a deterministic step runs its factory on one thread.

### Emergency reserve and the default handler

The heap constructor calls `Zenith_BackingAllocate(m_ulEmergencyReserveBytes, 16)` once; a nullptr result is logged through `Zenith_WriteDiagnosticLine` (the log router does not exist yet) and the heap runs without a reserve (`Zenith2/Docs/Contracts/Memory.md` → "Zenith_Heap").

`Allocate(ulSize, ulAlignment)` runs these steps:

1. Validate the alignment (`ZENITH_CHECK`, per `Zenith2/Docs/Contracts/Memory.md` → "Zenith_Heap"); treat size 0 as 1.
2. Try the backing (after the fault countdown); on success record statistics and the Debug leak entry and return.
3. On failure, if the calling thread's Debug thread state says it is already inside the OOM handler, terminate through `ZENITH_FATAL(FATAL_CODE_OUT_OF_MEMORY, …)` at once.
4. Take `uLOCK_LEVEL_HEAP_OOM`; set the thread's in-handler flag; for attempt `a` from 1 to `m_uMaxOOMRetries`: build `Zenith_OOMInfo{ ulSize, ulAlignment, a, "general heap" }`, call the handler; on `OOM_RESPONSE_ABORT` break; on `OOM_RESPONSE_RETRY` retry the backing and on success clear the flag, release the lock and return.
5. Clear the flag, release the lock and terminate through `ZENITH_FATAL(FATAL_CODE_OUT_OF_MEMORY, …)` with the `Zenith_OOMInfo` fields and `GetStats()` in the report.

The default handler (`m_pfnOOMHandler == nullptr`), in `Internal/Memory/Zenith_DefaultOOMHandler.cpp`:

1. If `m_pOOMUserData` is non-null, treat it as `Zenith_OOMPressureFlag*` and call `Raise()` on it (below).
2. If the emergency reserve is still held, free it to the backing, mark it released, and return `OOM_RESPONSE_RETRY`.
3. Otherwise return `OOM_RESPONSE_ABORT`.

### OOM pressure relay

<!-- elaboration -->
The default handler raises the memory governor's critical pressure flag through the user-data pointer installed in `main` (`Zenith2/Docs/Contracts/Memory.md` → "OOM handler"). The governor does not exist when the heap is constructed (startup step 8 against step 1 of `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence"), so the pointer names a relay object that `main` owns and the governor binds to.

```cpp
// Zenith2/Engine/ZenithCore/Zenith_MemoryGovernor.h
/// @contract type Zenith_OOMPressureFlag
/// Ownership: A local of main, declared before the root heap and destroyed after it; passed to the heap as
///   Zenith_HeapDesc::m_pOOMUserData and to the governor through BindOOMPressureFlag.
/// Thread affinity: ANY_THREAD. Raise is called from the OOM handler on the allocating thread.
/// Invalidation: None.
/// Units: None.
/// Errors: None; neither member can fail.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
struct Zenith_OOMPressureFlag
{
	void Raise();                     // m_uRaisedCount.fetch_add(1, memory_order_release)
	uint32 Consume();  // m_uRaisedCount.exchange(0, memory_order_acquire)
	std::atomic<uint32> m_uRaisedCount{ 0 };
};
```

- `Raise` allocates nothing and takes no lock, so it is safe inside the OOM handler under `uLOCK_LEVEL_HEAP_OOM`.
- `Zenith_MemoryGovernor::BindOOMPressureFlag(Zenith_OOMPressureFlag&)` stores the pointer; `Update` calls `Consume()` each frame prologue, and a non-zero value forces `MEMORY_PRESSURE_CRITICAL` for the governor's hysteresis dwell (`Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Level evaluation").
- The flag is a plain struct of one atomic with constant initialisation; it allocates nothing and runs no code at construction, so declaring it before the root heap does not create an engine object before the heap (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence", rule [Z2-N-ARCH-846]).

## Owned pointer placement

| File | Content |
|---|---|
| `Zenith2/Engine/ZenithCore/Zenith_Owned.h` | exactly the declarations of `Zenith2/Docs/Contracts/Memory.md` → "Zenith_Owned" and "Zenith_New Zenith_TryNew and Zenith_Delete", with `Zenith_New` and `Zenith_TryNew` declared before the class (`Zenith2/Docs/Contracts.md` → "Ownership and allocation", [Z2-N-CONTRACT-254]) |
| `Zenith2/Engine/ZenithCore/Zenith_Owned.inl` | the template definitions, included at the end of `Zenith_Owned.h` |
| `Zenith2/Engine/ZenithCore/Tests/Zenith_Owned_Tests.cpp` | the owned-pointer tests of "Memory tests" |
| `Zenith2/Engine/ZenithCore/Tests/Zenith_Owned_DefectTests.cpp` | `Test_Owned_BaseConversion_SameAddress_Checks` (suite `z2-death-check`) |
| `Zenith2/Engine/ZenithCore/Tests/Examples/Zenith_Owned_Example.cpp` | the WorkerGuide's own/move/reset example and the interface-plus-factory example, compiled |

`Zenith_Owned<T>` includes `Zenith_Heap.h` for `Zenith_GetRootHeap`; placement `new` inside `Zenith_Owned.inl` is allowed by rule F2 (`Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates", "ForbiddenCheck rules").

## Arena internals

The public API and allocation steps are `Zenith2/Docs/Contracts/Memory.md` → "Zenith_Arena". The private layout, in `Internal/Memory/Zenith_Arena.cpp` with members in `Zenith_Arena.h`'s private section (identical in every configuration):

| Member | Type | Meaning |
|---|---|---|
| `m_xDesc` | `Zenith_ArenaDesc` | copied at construction |
| `m_pxBlocks` | `Zenith_ArenaBlock*` | head of a singly linked list of blocks, in allocation order |
| `m_pxCurrent` | `Zenith_ArenaBlock*` | the block the cursor is in |
| `m_ulCursor` | `uint64` | byte offset of the next free byte in `m_pxCurrent`'s usable range |
| `m_ulUsedBytes` | `uint64` | nominal bytes (requested sizes plus alignment padding) since the last reset |
| `m_ulReservedBytes` | `uint64` | sum of nominal block sizes currently held |
| `m_ulHighWaterBytes` | `uint64` | maximum of `m_ulUsedBytes` since construction |
| `m_uLiveNonTrivial` | `uint32` | live objects constructed with a non-trivial destructor |
| `m_uBlockCount` | `uint32` | blocks currently held |
| `m_xSanitizerState` | `Zenith_SanitizerArenaState` | the last allocation's tag and the current block's free tag (HWASan); unused bytes elsewhere |

`Zenith_ArenaBlock` is a 32-byte record placed at the start of each block's root-heap allocation: `{ Zenith_ArenaBlock* m_pxNext; uint64 m_ulNominalBytes; uint64 m_ulUsableOffset; uint8 m_uFreeTag; uint8 m_auPadding[7]; }`. The block record is allocator bookkeeping inside memory the arena owns, not a per-allocation header.

- A marker (`Zenith_ArenaMarker`) records `{ block index, cursor, live non-trivial count }`; `ResetToMarker` walks to the marker's block, moves the cursor back, poisons or retags every released range per `Zenith2/Docs/Contracts/Memory.md` → "ASan poisoning rules" and "HWASan tagging rules", and keeps every block.
- `ResetToMarker` with a marker from a different arena, or a marker beyond the cursor, is a `ZENITH_CHECK` failure.
- Counters: the arena's owner publishes `<name>.used_bytes`, `<name>.reserved_bytes` and `<name>.high_water_bytes` each frame through `Zenith_DispatchCounter`, using `Zenith_ArenaDesc::m_szName`; the arena itself calls no hook.

## Pool internals

The public API is `Zenith2/Docs/Contracts/Memory.md` → "Zenith_Pool". Layout of one page (one root-heap allocation):

| Offset | Content |
|---|---|
| 0 | `Zenith_PoolPage` record: `{ Zenith_PoolPage* m_pxNext; uint32 m_uFirstSlotIndex; uint32 m_uFreeCount; uint8 m_uFreeTag; uint8 m_auPadding[7]; }` (24 bytes) |
| 24 | out-of-line free list: `uint32 m_auFreeSlots[m_uSlotsPerPage]` (a stack of free slot indices local to the page) |
| after the free list | out-of-line tag bytes: `uint8 m_auSlotTags[m_uSlotsPerPage]` (HWASan tags; present and unused in other presets) |
| rounded up to the slot alignment | `m_uSlotsPerPage` slots of the preset's stride (ASan: rule A4; HWASan: rule H12; otherwise `align_up(slotBytes, slotAlignment)`) |
| end | the guard granule of rule H11 (16 bytes, every preset, so the page size is identical in every preset) |

- Slot allocation pops the first page with free slots; freeing pushes the slot index back onto its page's free stack after the quarantine (`Zenith2/Docs/Contracts/Memory.md` → "Zenith_Pool", [Z2-N-CONTRACT-236]).
- The quarantine is a FIFO ring of `min(64, m_uMaxSlots / 4, m_uSlotsPerPage)` slot pointers held inline in the pool object (`Zenith_FixedArray<void*, 64>` storage, only the first `quarantine size` entries used), active in sanitizer builds and bypassed (size 0) elsewhere; the object layout is the same in every preset.
- The page owning a slot is found by `(slot address − page base) / page bytes` over a `Zenith_Array<Zenith_PoolPage*>` sorted by address, with binary search; `Free` of a pointer outside every page, or of a slot on its page's free stack, is `ZENITH_ASSERT` (`Zenith2/Docs/Contracts/Memory.md` → "Zenith_Pool").

## Zenith_Function

<!-- elaboration -->
`Zenith_Function<TSignature>` is ZenithCore's arena-stored, type-erased, trivially destructible callable (`Zenith2/Docs/Contracts/Memory.md` → "Zenith_Arena", [Z2-N-CONTRACT-232]). The render graph stores raster lambdas as `Zenith_Function<void(Flux_RasterContext&)>` (`Zenith2/Docs/Rendering.md` → "Render graph") and the CPU frame graph stores node captures the same way.

```cpp
// Zenith2/Engine/ZenithCore/Zenith_Function.h
template<typename TSignature> class Zenith_Function;

/// @contract value Zenith_Function
/// Ownership: A two-pointer value. The capture lives in the Zenith_Arena passed to Create or TryCreate; the
///   function object does not own it. Copying the value copies the two pointers.
/// Thread affinity: IMMUTABLE. Invoking it follows the callable's own rules; the value itself never changes.
///   Members non-const: it is default-constructed null and reassigned (descs, containers, callback slots).
/// Invalidation: Invalid once the arena that holds its capture is reset to before the capture, or destroyed.
/// Units: None.
/// Errors: TryCreate returns Zenith_Result<Zenith_Function, Zenith_AllocError> (the arena's error). Invoking a
///   null Zenith_Function is ZENITH_ASSERT. A capture that is not trivially destructible, not
///   move-constructible, or not invocable with the signature fails a static_assert at the Create site.
/// Version: since Z2 Stage 2. Source API; never serialized.
/// @endcontract
template<typename R, typename... TArgs>
class Zenith_Function<R(TArgs...)>
{
public:
	constexpr Zenith_Function() = default;
	template<typename TCallable>
	static Zenith_Function Create(Zenith_Arena& xArena, TCallable&& xCallable);
	template<typename TCallable>
	static Zenith_Result<Zenith_Function, Zenith_AllocError> TryCreate(Zenith_Arena& xArena, TCallable&& xCallable);
	static constexpr Zenith_Function FromFunctionPointer(R (*pfnFunction)(TArgs...));
	R operator()(TArgs... xArgs) const;
	constexpr bool IsNull() const { return m_pfnInvoke == nullptr; }
private:
	using PfnInvoke = R (*)(const void* pCapture, TArgs... xArgs);
	PfnInvoke m_pfnInvoke = nullptr;
	const void* m_pCapture = nullptr;
};
static_assert(sizeof(Zenith_Function<void()>) == 2 * sizeof(void*));
```

`Create<TCallable>` runs these steps:

1. `using TStored = std::decay_t<TCallable>`; `static_assert(std::is_trivially_destructible_v<TStored>)`, `static_assert(std::is_move_constructible_v<TStored>)`, `static_assert(std::is_invocable_r_v<R, const TStored&, TArgs...>)`. No `nothrow` trait is used: a lambda is not `noexcept` unless declared so, and engine code declares nothing `noexcept`.
2. When `TStored` is an empty class (`std::is_empty_v`), store no capture: `m_pCapture = nullptr` and the invoke thunk default-constructs nothing (it invokes a `static constexpr TStored` value).
3. Otherwise `void* pMemory = xArena.Allocate(sizeof(TStored), alignof(TStored))` (`TryCreate` uses `TryAllocate`), then `::new (pMemory) TStored(std::forward<TCallable>(xCallable))`; no live non-trivial count changes, because the type is trivially destructible.
4. `m_pfnInvoke` is a thunk `[](const void* p, TArgs... a) -> R { return (*static_cast<const TStored*>(p))(std::forward<TArgs>(a)...); }`.

- `operator()` evaluates `ZENITH_ASSERT(m_pfnInvoke != nullptr, …)` and calls the thunk; it allocates nothing.
- `Zenith_Function` is itself trivially copyable and trivially destructible, so it may be stored in arenas, in graph captures and in `Zenith_Array` elements.

<!-- informative -->
`std::function` is forbidden in runtime public headers and heap-allocates its captures; Zenith's old render graph stored `std::function` per pass, which was one of the heap allocations per frame the audit found. `Zenith_Function` costs one arena allocation per non-empty capture and none per call.
<!-- /informative -->

## Zenith_SlotTable

<!-- elaboration -->
Generational handles are the shared-reference mechanism in every configuration (Z2-D-040; `Zenith2/Docs/Contracts.md` → "Handle kinds"). Each handle kind keeps its own distinct type and bit layout in its declaring library; `Zenith_SlotTable<T>` is the reusable generational store that a library uses to implement a handle kind whose layout is a 32-bit slot plus a 32-bit generation (the operation table, the callback registry, the asset registry's slots and any future table). The entity table (24/28/12 bits) and device handles (32/24/8 bits) keep their own tables because their bit layouts differ.

```cpp
// Zenith2/Engine/ZenithCore/Zenith_SlotTable.h
/// @contract value Zenith_SlotID
/// Ownership: Plain value naming one slot of one Zenith_SlotTable; copying extends nothing.
/// Thread affinity: IMMUTABLE. Members non-const: handle types hold one and are reassigned.
/// Invalidation: Stale once the slot is freed; resolution then fails in every configuration.
/// Units: None.
/// Errors: None; resolution errors are the table's.
/// Version: since Z2 Stage 2. Never serialized (Zenith2/Docs/Contracts.md "Handle kinds").
/// @endcontract
struct Zenith_SlotID
{
	uint32 m_uSlot = 0;
	uint32 m_uGeneration = 0;           // 0 is never a live generation
	constexpr bool IsNull() const { return m_uGeneration == 0; }
	friend constexpr bool operator==(Zenith_SlotID, Zenith_SlotID) = default;
};

/// @contract enum Zenith_HandleError
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: HANDLE_ERROR_NULL (a null ID was resolved), HANDLE_ERROR_STALE (the slot's generation moved on),
///   HANDLE_ERROR_FOREIGN (the slot index is outside this table), HANDLE_ERROR_TABLE_FULL (Allocate at the
///   hard cap), HANDLE_ERROR_OUT_OF_MEMORY (a page could not be allocated).
/// Version: since Z2 Stage 2. Enumerators keep their meaning once shipped.
/// @endcontract
enum Zenith_HandleError : uint8
{
	HANDLE_ERROR_NULL = 1,
	HANDLE_ERROR_STALE,
	HANDLE_ERROR_FOREIGN,
	HANDLE_ERROR_TABLE_FULL,
	HANDLE_ERROR_OUT_OF_MEMORY,
};
constexpr const char* Zenith_ToString(Zenith_HandleError eError);
constexpr Zenith_ErrorCategory Zenith_GetErrorCategory(Zenith_HandleError eError);

struct Zenith_SlotTableDesc
{
	const char* m_szName = "slots";
	uint32 m_uSlotsPerPage = 256;       // power of two
	uint32 m_uMaxSlots = 0;             // hard cap; from a Budgets row or derived in the owner's contract block
};

/// @contract container Zenith_SlotTable
/// Ownership: Owned by the library implementing a handle kind; pages come from the root heap.
/// Thread affinity: OWNER_THREAD. A library needing concurrent access wraps it with its own lock row.
/// Invalidation: Allocate never moves an existing value (paged storage), so a T* from Resolve stays valid
///   until that slot is freed; Free invalidates the freed slot's pointer and ID only.
/// Units: None.
/// Errors: Zenith_Result<…, Zenith_HandleError>; resolution never asserts.
/// Version: since Z2 Stage 2. Source API.
/// Alignment: values are aligned to alignof(T) (at most ulHEAP_MAX_ALIGNMENT).
/// Overflow: at most m_uMaxSlots slots (at most 2^31); a slot whose generation reaches 0xFFFFFFFF is retired
///   and never reissued.
/// Allocation failure: TryAllocate returns HANDLE_ERROR_OUT_OF_MEMORY or HANDLE_ERROR_TABLE_FULL and leaves
///   the table unchanged; Allocate routes page-allocation failure through the OOM handler and a full table
///   through ZENITH_CHECK.
/// Complexity: TryAllocate, Resolve and Free O(1); no operation iterates the table except ForEachLive (O(slots)).
/// @endcontract
template<typename T>
class Zenith_SlotTable
{
public:
	explicit Zenith_SlotTable(const Zenith_SlotTableDesc& xDesc);   // allocates nothing
	~Zenith_SlotTable();                                            // ZENITH_CHECK: no live slots
	Zenith_SlotTable(const Zenith_SlotTable&) = delete;
	Zenith_SlotTable& operator=(const Zenith_SlotTable&) = delete;
	template<typename... TArgs>
	Zenith_Result<Zenith_SlotID, Zenith_HandleError> TryAllocate(TArgs&&... xArgs);
	template<typename... TArgs>
	Zenith_SlotID Allocate(TArgs&&... xArgs);
	Zenith_Result<T*, Zenith_HandleError> Resolve(Zenith_SlotID xID);
	Zenith_Result<const T*, Zenith_HandleError> Resolve(Zenith_SlotID xID) const;
	Zenith_Result<void, Zenith_HandleError> Free(Zenith_SlotID xID);   // runs ~T
	uint32 GetLiveCount() const;
	template<typename TVisitor> void ForEachLive(TVisitor&& xVisitor);              // ascending slot order
};
```

Algorithm:

1. Slot state: per slot `{ uint32 m_uGeneration; uint32 m_uNextFree; bool m_bLive; }` in a per-page state array beside the page's value storage (values are constructed in place with placement `new`).
2. Free list: a FIFO over slot indices (`m_uFreeHead`, `m_uFreeTail`, linked through `m_uNextFree`), so a freed slot is reused as late as possible, which lengthens the window in which a stale ID is caught.
3. `TryAllocate`: pop the FIFO head; if empty and the slot count is below `m_uMaxSlots`, allocate a new page (all its slots appended to the FIFO); if the count is at the cap, `HANDLE_ERROR_TABLE_FULL`. Generation of a fresh slot is 1. Construct `T` in place; mark live.
4. `Resolve`: `IsNull()` → `HANDLE_ERROR_NULL`; `m_uSlot ≥ slot count` → `HANDLE_ERROR_FOREIGN`; not live or generation differs → `HANDLE_ERROR_STALE`.
5. `Free`: resolve as above; run `~T`; increment the generation; if it is now `0xFFFFFFFF` leave the slot retired (not appended to the FIFO); otherwise append it to the FIFO tail.

## Fault-injection support

`Zenith_FaultInjector` is the interface of `Zenith2/Docs/Contracts/Memory.md` → "Fallible construction" ([Z2-N-CONTRACT-255]), declared in `Zenith2/Engine/ZenithCore/Zenith_FaultInjector.h` with exactly that declaration and this contract block:

```cpp
/// @contract interface Zenith_FaultInjector
/// Ownership: Implemented by tests; passed by pointer in a factory's desc (nullptr in production); the test
///   owns the instance for the factory call's duration.
/// Thread affinity: ANY_THREAD. Implementations used by multi-threaded factories are thread-safe.
/// Invalidation: None.
/// Units: None.
/// Errors: ShouldFail cannot fail; true makes the calling factory take its failure path at that fault point.
/// Reentrancy: NOT_APPLICABLE. The interface invokes no callbacks.
/// Version: since Z2 Stage 1. Source API.
/// @endcontract
```

Test support in `Zenith2/Engine/ZenithCore/Tests/Support/` (built into `ZenithTestSupport`, never into a runtime target):

| File | Declares | Behaviour |
|---|---|---|
| `Zenith_TestFaultInjector.h` | `class Zenith_TestFaultInjector final : public Zenith_FaultInjector` | `ShouldFail(u)` returns true exactly when `u == m_uFailPoint` and records every point it was asked about in call order (`GetVisitedPoints()`), under a `std::mutex` (tests may use the STL) |
| `Zenith_FaultSweep.h` | `template<typename TFactory> void Zenith_RunFaultSweep(TFactory&& xFactory)` | runs the factory once with no fault to collect the visited points, then once per visited point with that point failing; for each run asserts the factory's error, zero leaks from a fresh test heap (`Zenith_Heap::GetStats().m_ulLiveAllocations` unchanged), no thread left running (the factory's thread count query) and no registration left behind (the registry's live count); then repeats the whole sweep for allocation failures by setting `SetFaultCountdown(n)` for `n` from 1 to the number of allocations of the successful run |
| `Zenith_TestEntry_SDLMain.cpp`, `Zenith_TestOutputLineBuffer.h` | the Android test entry `SDL_main` and the line buffer of its tagged output route | "Test main and the Android test entry"; used by MOD's test APKs |
| `Zenith_CountingType.h` | `Zenith_CountingType` and `Zenith_CountingStats` | counts constructions, copies, moves and destructions per instance ID, for container tests ([Z2-N-CONTRACT-291]) |
| `Zenith_PropertyGen.h` | `Zenith_PropertyGen` | a seeded generator (`Zenith_RandomSource`-based) of operation sequences and values; prints the seed and operation index on failure |
| `Zenith_TestMain.cpp`, `Zenith_RunTestMain.cpp`, `Zenith_TestMain.h` | the test `main` and the body it forwards to ("Test main and the Android test entry") | [Z2-N-CONTRACT-406]: installs a root heap, installs no profiler hooks, runs doctest, excludes the death and defect suites unless one case is named, exits 3 on any leak |

## Test main and the Android test entry

The shared test main of [Z2-N-CONTRACT-406] is split so that two entry points run the same body: the ordinary `main` of every test executable, and an Android `SDL_main` through which MOD's test APKs host an `android-arm64` test executable inside the shared Player's activity (`Zenith2/Docs/Design/MOD/GameBuild.md` → "The Apk target"; cross-area row MB23). Both live in `Zenith2/Engine/ZenithCore/Tests/Support/` and are therefore part of `ZenithTestSupport` ([Z2-N-ARCH-170], [Z2-N-ARCH-255]); neither contains a platform conditional (rule F11), because the only Android-specific call, the logcat write, is ZenithCore's `Zenith_WriteTaggedDiagnosticLine` (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Diagnostic output and backtraces").

### Test main declarations

```cpp
// Zenith2/Engine/ZenithCore/Tests/Support/Zenith_TestMain.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_StringView.h"

enum Zenith_TestOutputRoute : uint8
{
	TEST_OUTPUT_ROUTE_STDOUT = 1,         // doctest writes to std::cout (the ordinary main)
	TEST_OUTPUT_ROUTE_TAGGED_DIAGNOSTIC,  // each complete output line goes to Zenith_WriteTaggedDiagnosticLine(szTEST_OUTPUT_TAG, line)
};

inline constexpr const char* szTEST_OUTPUT_TAG = "Zenith2Test";                  // the logcat tag MOD's device steps read
inline constexpr const char* szTEST_OUTPUT_ROUTE_OPTION = "--zenith-test-output=";  // value "stdout" or "tagged"
inline constexpr uint32 uTEST_OUTPUT_LINE_MAX_BYTES = 2048;                   // a longer line is written in pieces of this size

/// @contract functions Zenith_TestMain
/// Ownership: Zenith_RunTestMain creates, installs, uninstalls and destroys the root heap itself; it owns nothing
///   after it returns.
/// Thread affinity: MAIN_THREAD (the thread that entered main or SDL_main), once per process.
/// Invalidation: None.
/// Units: None.
/// Errors: returns doctest's result, or 3 when the root heap reports a leak ([Z2-N-CONTRACT-406]); an unknown
///   value of szTEST_OUTPUT_ROUTE_OPTION is reported with Zenith_WriteDiagnosticLine and returns 2 before doctest runs.
/// Reentrancy: NONE. A test case never calls it.
/// Version: since Z2 Stage 3. Source API (test support only).
/// @endcontract
int Zenith_RunTestMain(int iArgc, char** ppszArgv, Zenith_TestOutputRoute eRoute);

// Test access to step 2 alone (Zenith_TestMain_Tests.cpp); not called by any entry other than Zenith_RunTestMain.
struct Zenith_TestMainAccess
{
	// Copies ppszArgv into ppszOut (capacity uOutCapacity) without the route option, sets eRouteInOut when the option is
	// present, and returns the new argument count; returns -1 for an unknown option value or when the copy does not fit.
	static int ParseRouteOption(int iArgc, char** ppszArgv, char** ppszOut, uint32 uOutCapacity,
		Zenith_TestOutputRoute& eRouteInOut);
};
```

### Test main files

| File | Contents |
|---|---|
| `Tests/Support/Zenith_TestMain.cpp` | the file holds only `int main(int iArgc, char** ppszArgv) { return Zenith_RunTestMain(iArgc, ppszArgv, TEST_OUTPUT_ROUTE_STDOUT); }` |
| `Tests/Support/Zenith_RunTestMain.cpp` | defines `DOCTEST_CONFIG_IMPLEMENT` and `Zenith_RunTestMain`: the body `main` had before the split, moved unchanged (root heap first, the `test-suite-exclude` filter unless a suite or case is named, no profiler hooks, `ReportLeaks` and exit 3, the heap uninstalled last), plus the steps below |
| `Tests/Support/Zenith_TestEntry_SDLMain.cpp` | `extern "C" ZENITH_EXPORT_C_ENTRY int SDL_main(int iArgc, char** ppszArgv) { return Zenith_RunTestMain(iArgc, ppszArgv, TEST_OUTPUT_ROUTE_TAGGED_DIAGNOSTIC); }` |
| `Tests/Support/Zenith_TestOutputLineBuffer.h` | `class Zenith_TestOutputLineBuffer final : public std::streambuf` (tests may use the STL): buffers bytes in a `uTEST_OUTPUT_LINE_MAX_BYTES` array, calls its `void (*)(Zenith_StringView)` sink once per `\n` (the line without it) or when the array fills, and `Flush()` emits a trailing partial line; `overflow`, `xsputn` and `sync` only |

`ZENITH_EXPORT_C_ENTRY` is defined in `Zenith2/Engine/ZenithCore/Internal/Zenith_CompilerSupport.h`, the one file allowed to test a compiler macro (rule F11, JR-CORE-COMPILERSUPPORT): `__attribute__((visibility("default")))` when `__clang__` or `__GNUC__` is defined, and empty under MSVC. Every Zenith2 target compiles with `-fvisibility=hidden` on clang (`Zenith2/Docs/Architecture/BuildSystem.md` → "Clang flags for the android and linux-tsan presets"), so without it the symbol would not be exported from MOD's `<Test>_ApkLib` shared object.

### Test main steps

`Zenith_RunTestMain(iArgc, ppszArgv, eRoute)` runs:

0. Call `Zenith_ApplyCrtMathMode()` (`Zenith2/Docs/Design/CORE/Maths.md` → "The CRT maths mode"), before anything else; the main of the test executable before the split ran the same call first.
1. Install the root heap exactly as the moved body did; from here the steps are those of the scaffold's main, with steps 2 to 4 inserted before doctest's `applyCommandLine`.
2. Scan the arguments for one starting with `szTEST_OUTPUT_ROUTE_OPTION`; `stdout` selects `TEST_OUTPUT_ROUTE_STDOUT` and `tagged` selects `TEST_OUTPUT_ROUTE_TAGGED_DIAGNOSTIC`, overriding `eRoute`; any other value writes `Zenith_TestMain: unknown --zenith-test-output value '<v>'` with `Zenith_WriteDiagnosticLine`, uninstalls the heap and returns 2. The option is removed from the vector handed to doctest (a copy of the pointer array on the stack, at most 256 entries; more is the same diagnostic and 2), so doctest never sees it.
3. For `TEST_OUTPUT_ROUTE_TAGGED_DIAGNOSTIC`: construct a `Zenith_TestOutputLineBuffer` whose sink calls `Zenith_WriteTaggedDiagnosticLine(szTEST_OUTPUT_TAG, strLine)`, wrap it in a `std::ostream`, and pass it to `doctest::Context::setCout`; for `TEST_OUTPUT_ROUTE_STDOUT` doctest keeps `std::cout`.
4. Apply the suite-exclusion filter and the command line, run doctest.
5. `Flush()` the line buffer (tagged route) before the leak report, so the last summary line is written before `ZENITH LEAK:` lines; then the leak rule and the heap uninstall exactly as before.

- On Android the tagged route is what MOD's test APK uses: `SDL_main` is called by the activity's SDL thread with the `org.zenith2.args` extras as `ppszArgv`, so `--test-case=<Name>` selects a case and every doctest line reaches logcat as tag `Zenith2Test`. On a host the same route writes `Zenith2Test: <line>` to stderr, which the CTest entry `ZenithCore_Tests.TaggedOutputRoute` ("Memory tests") checks.
- `Zenith_TestEntry_SDLMain.cpp` is compiled into `ZenithTestSupport` on every platform and is never extracted from the archive unless a link names `SDL_main` (`-Wl,--undefined=SDL_main` on MOD's `<Test>_ApkLib`); an ordinary test executable references `main` alone, and `Zenith_TestMain.cpp` and `Zenith_TestEntry_SDLMain.cpp` are separate objects so neither entry is pulled by the other.
- `zenith_core_output_route_case(TARGET NAME PASS_REGEX <regex> ARGS <arg>...)`, in the ZenithCore block of `Zenith2/CMake/Tests/ZenithCMakeTests.cmake`, is `add_test(NAME ${TARGET}.${NAME} COMMAND $<TARGET_FILE:${TARGET}> ${ARGS})` with `PASS_REGULAR_EXPRESSION ${PASS_REGEX}`, `LABELS "z2-test-main"` and `TIMEOUT 60`; it registers in every configuration and in every preset that runs CTest.
- The archive therefore holds two entry objects but no executable links both: a test executable gets `main` (it is the undefined symbol the C runtime start-up code references), an APK library gets `SDL_main`.

## Test child processes

<!-- elaboration -->
Some tests can only prove their claim across a process boundary:

- SCENE's on-disk interruption matrix launches the test executable as a child that crashes mid-write.
- SAVE's power-loss test kills a child that is writing a save.
- SAVE's build-ID test reads its own executable file.

Calling the process API from test code would need a platform conditional, which rule F11 forbids in every Zenith2 file. `Zenith2/Docs/Architecture/BuildSystem.md` [Z2-N-ARCH-162] allows `Windows`, `Android` and `Linux` directories only under `Internal/`.

The orchestrator ruled option (a) of cross-doc item X349 (2026-10-10). One launcher is shared by every library's tests. It is declared in ZenithTestSupport, the one test-support tree that has translation units. Its per-platform bodies live in ZenithCore's own `Internal/Windows/`, `Internal/Android/` and `Internal/Linux/` files, behind an `Internal/` header that only ZenithTestSupport includes. So F11 stays absolute and the glob-partition table needs no new row: the platform files are ordinary ZenithCore platform files, and no runtime code calls them.

### Test child process declarations

```cpp
// Zenith2/Engine/ZenithCore/Tests/Support/Zenith_TestProcess.h   (ZenithTestSupport)
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_String.h"

constexpr uint32 uTEST_PROCESS_MAX_VARIABLES = 16;
constexpr uint32 uTEST_PROCESS_MAX_EXTRA_ARGUMENTS = 8;
constexpr uint64 ulTEST_PROCESS_MAX_OUTPUT_BYTES = 1ull << 20;   // 1 MiB of captured standard output

enum Zenith_TestProcessError : uint8
{
	TEST_PROCESS_ERROR_UNSUPPORTED = 1,      // Android: a test runs inside an APK and cannot launch its own executable
	TEST_PROCESS_ERROR_BAD_ARGUMENT,         // empty case name, too many variables or arguments, '=' in a name
	TEST_PROCESS_ERROR_LAUNCH_FAILED,        // the OS refused the launch (CreateProcessW, posix_spawn) or a pipe
	TEST_PROCESS_ERROR_TIMED_OUT,            // Wait's timeout elapsed and the child is still running
	TEST_PROCESS_ERROR_OUTPUT_TOO_LARGE,     // the child wrote more than ulTEST_PROCESS_MAX_OUTPUT_BYTES; it was killed
	TEST_PROCESS_ERROR_OUT_OF_MEMORY,
};

struct Zenith_TestProcessVariable
{
	const char* m_szName = nullptr;           // set in (or replaced in) the child's copy of the parent's environment
	const char* m_szValue = nullptr;
};

struct Zenith_TestProcessDesc
{
	const char* m_szTestCase = nullptr;                              // passed as --test-case=<name> --no-skip
	Zenith_Span<const Zenith_TestProcessVariable> m_axVariables;
	Zenith_Span<const char* const> m_axExtraArguments;               // after the two options above, in order
	bool m_bCaptureStdout = false;                                   // otherwise the child inherits the parent's
};

struct Zenith_TestProcessExit
{
	sint32 m_iExitCode = 0;          // the exit status; 128 + signal number for a child a signal ended (POSIX)
	bool m_bKilledByParent = false;   // Kill (or Wait's output cap, or the destructor) ended it; m_iExitCode is then −1
};

/// @contract type Zenith_TestProcess
/// Ownership: Owns one child process of the running test executable and its standard-output pipe; created
///   through LaunchSelf as a Zenith_Owned. The destructor kills and reaps a child that is still running, so no
///   child outlives its object.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: GetStdout's view is valid until the object is destroyed.
/// Units: Timeouts in milliseconds; output in bytes.
/// Errors: LaunchSelf returns TEST_PROCESS_ERROR_UNSUPPORTED on Android, BAD_ARGUMENT, LAUNCH_FAILED or
///   OUT_OF_MEMORY; Wait returns TIMED_OUT (the child keeps running) or OUTPUT_TOO_LARGE. Nothing asserts.
/// Version: since Z2 Stage 2. Test support only: no runtime or tool target links ZenithTestSupport.
/// @endcontract
class Zenith_TestProcess
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_TestProcess>, Zenith_TestProcessError> LaunchSelf(
		const Zenith_TestProcessDesc& xDesc);
	~Zenith_TestProcess();
	Zenith_Result<Zenith_TestProcessExit, Zenith_TestProcessError> Wait(uint32 uTimeoutMs);
	void Kill();                                   // terminate now and reap; a no-op once the child has exited
	bool HasExited() const;
	Zenith_StringView GetStdout() const;   // empty unless m_bCaptureStdout; complete after exit
};

bool Zenith_IsTestProcessSupported();      // false on Android
// Launch, wait up to uTimeoutS, kill past it: the exit code, or TIMED_OUT after the kill.
Zenith_Result<sint32, Zenith_TestProcessError> Zenith_RunChildTestCase(
	const char* szTestCase, Zenith_Span<const Zenith_TestProcessVariable> axVariables, uint32 uTimeoutS);
// The running test executable's absolute path, UTF-8 (GetModuleFileNameW; readlink of /proc/self/exe).
Zenith_Result<Zenith_String, Zenith_TestProcessError> Zenith_GetTestExecutablePath();
```

```cpp
// Zenith2/Engine/ZenithCore/Internal/Zenith_ProcessLaunch.h   (included only by Tests/Support/Zenith_TestProcess.cpp)
struct Zenith_ProcessHandles { uint64 m_ulProcess = 0; uint64 m_ulStdoutRead = 0; };
enum Zenith_ProcessPollResult : uint8 { PROCESS_POLL_RUNNING = 1, PROCESS_POLL_EXITED };
bool Zenith_PlatformProcessSupported();
bool Zenith_PlatformGetOwnExecutablePath(Zenith_String& strOut);
bool Zenith_PlatformLaunchProcess(Zenith_StringView strExecutable, Zenith_Span<const Zenith_StringView> axArguments,
	Zenith_Span<const Zenith_StringView> axAssignments, bool bCaptureStdout, Zenith_ProcessHandles& xOut);   // "NAME=value" each
Zenith_ProcessPollResult Zenith_PlatformPollProcess(Zenith_ProcessHandles& xHandles, uint32 uSliceMs,
	Zenith_String* pstrStdout, sint32& iExitCode);   // drains available output into *pstrStdout
void Zenith_PlatformKillProcess(Zenith_ProcessHandles& xHandles);    // terminate, reap, close
```

### Test child process platform matrix

| Step | Windows (`Internal/Windows/Zenith_ProcessLaunch_Windows.cpp`) | Linux (`Internal/Linux/Zenith_ProcessLaunch_Linux.cpp`) | Android (`Internal/Android/Zenith_ProcessLaunch_Android.cpp`) |
|---|---|---|---|
| own path | `GetModuleFileNameW(nullptr, …)` growing to 32,768 units, then `Zenith_Utf16ToUtf8` | `readlink("/proc/self/exe", …)` growing to 64 KiB | returns false |
| arguments | one command line: each argument in double quotes, backslashes before a quote doubled and the quote written `\"` (the `CommandLineToArgvW` rules), UTF-16 through `Zenith_Utf8ToUtf16` | an `argv` array of NUL-terminated copies | — |
| environment | `GetEnvironmentStringsW`, copied with every entry whose name (case-insensitive) is assigned removed, then the assignments appended, as one `CREATE_UNICODE_ENVIRONMENT` block | `environ`, copied with every entry whose name is assigned removed, then the assignments appended | — |
| launch | `CreatePipe` (inheritable write end) when capturing, `STARTF_USESTDHANDLES` with the parent's stdin and stderr, `CreateProcessW` with `bInheritHandles = TRUE`, then the parent closes the write end and the thread handle | `pipe2(O_CLOEXEC)` when capturing, `posix_spawn_file_actions_adddup2` of the write end onto 1, `posix_spawn`, then the parent closes the write end | — |
| poll (one slice) | `PeekNamedPipe` + `ReadFile` of what is available, then `WaitForSingleObject(process, uSliceMs)`; exited → drain the pipe to EOF, `GetExitCodeProcess` | `poll` on the pipe for up to `uSliceMs`, `read` what is available, `waitpid(…, WNOHANG)`; exited → drain to EOF; `WIFEXITED` → `WEXITSTATUS`, `WIFSIGNALED` → 128 + `WTERMSIG` | — |
| kill | `TerminateProcess(process, 1)`, `WaitForSingleObject(INFINITE)`, close the handles | `kill(pid, SIGKILL)`, `waitpid(pid, …, 0)` retried on `EINTR`, close the pipe | — |
| supported | true | true | false |

All three files are ordinary ZenithCore platform files. The Linux row is the one `linux-tsan` runs.

### Test child process steps

1. `LaunchSelf`:
   1. Returns `UNSUPPORTED` when `Zenith_PlatformProcessSupported()` is false.
   2. Validates the desc: the case name is non-empty; at most 16 variables and 8 extra arguments; no `=` in a name. Otherwise it returns `BAD_ARGUMENT`.
   3. Builds the path with `Zenith_PlatformGetOwnExecutablePath`. It builds the arguments `[path, "--test-case=<name>", "--no-skip", extra…]` and the `NAME=value` assignments as `Zenith_String`s on the root heap; an allocation failure returns `OUT_OF_MEMORY` with nothing launched.
   4. Calls `Zenith_PlatformLaunchProcess`; false returns `LAUNCH_FAILED`.
   5. The child's test main applies the same suite-exclusion rule as any run that names a case, so exactly the named case runs (doctest's `--no-skip` admits a `doctest::skip()` child case).
2. `Wait(uTimeoutMs)`:
   1. Calls `Zenith_PlatformPollProcess` in 10 ms slices until the child exits or the elapsed `Zenith_GetMonotonicTimeUs` time exceeds the timeout.
   2. Exited → returns the exit with `m_bKilledByParent = false`.
   3. Timeout → `TIMED_OUT`; the child keeps running and a later `Wait` or `Kill` may follow.
   4. Captured output above the cap → `Kill`, then `OUTPUT_TOO_LARGE`.
   5. Output is drained on every slice, so a child that fills the pipe buffer never deadlocks against the parent.
3. `Kill` calls `Zenith_PlatformKillProcess` and records the exit as `{ −1, true }`. The destructor calls `Kill` when the child has not exited.
4. `Zenith_RunChildTestCase(szCase, axVariables, uTimeoutS)` is `LaunchSelf` (no capture), then `Wait(uTimeoutS × 1000)`. On `TIMED_OUT` it calls `Kill` and returns `TIMED_OUT`; otherwise it returns the exit code.
5. The parent passes everything a child needs through variables or arguments. Doctest gives a test case no `argv`, so child cases read their variables with `std::getenv` (test code may use the STL).

## Factory fault points

Every CORE factory unwinds exactly what it built when a step fails ([Z2-N-CONTRACT-340]); each fallible step has its own test named `Test_<Type>_Create_FailsAt<Step>_Unwinds` ([Z2-N-CONTRACT-341]); for a factory not named `Create`, `Create` in the test name is the factory's name (`Test_LogRouter_StartDrainThread_…`, `Test_JsonDocument_Parse_…`, `Test_JsonDocument_CreateEmpty_…`). Allocation steps are failed through the heap fault countdown ([Z2-N-CONTRACT-225]), numbered in the order the factory performs them; a non-allocation step is failed through a `Zenith_FaultInjector` fault point declared in the type's public header ([Z2-N-CONTRACT-255]). ZenithCore's fault-point values carry the library tag `0x434F` (`CO`) in their high half and are unique across the library (`uFAULT_POINT_THREAD_NATIVE_CREATE` = `0x434F0001`, `uFAULT_POINT_LOG_ROUTER_DRAIN_THREAD` = `0x434F0002`, `uFAULT_POINT_GOVERNOR_LOW_MEMORY_HANDLE` = `0x434F0003`), as other libraries' values carry theirs, so a factory that passes its injector to a nested factory (the log router to `Zenith_Thread::Create`) names every step unambiguously. Each test asserts the factory's error, zero leaks from the test heap, no thread left running and no registration left behind, using `Zenith_RunFaultSweep`.

| Factory | Steps in order (countdown step, or fault point) | Tests |
|---|---|---|
| `Zenith_StringTable::Create` | desc validation (not a fault point: `STRING_ERROR_INVALID_DESC`); 1 object; 2 page-pointer array; 3 intern map's initial slots; 4 first text chunk | `Test_StringTable_Create_RejectsInvalidDesc`, `Test_StringTable_Create_FailsAtObject_Unwinds`, `_FailsAtPageArray_Unwinds`, `_FailsAtInternMap_Unwinds`, `_FailsAtTextChunk_Unwinds` |
| `Zenith_LogRouter::Create` | 1 object; 2 ring; 3 category-override table | `Test_LogRouter_Create_FailsAtObject_Unwinds`, `_FailsAtRing_Unwinds`, `_FailsAtOverrideTable_Unwinds` |
| `Zenith_LogRouter::StartDrainThread` | fault point `uFAULT_POINT_LOG_ROUTER_DRAIN_THREAD` (in `Zenith_LogRouter.h`) before `Zenith_Thread::Create`, then the thread's own steps (its start block, its native creation) | `Test_LogRouter_StartDrainThread_FailsAtThread_Unwinds` (returns `THREAD_ERROR_CREATE_FAILED`, no thread left, the buffered records still in the ring and output by a following `DrainForTest`); in production the composition root treats the failure as fatal (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router") |
| `Zenith_ConfigSchema::Create` | 1 object; 2 key table | `Test_ConfigSchema_Create_FailsAtObject_Unwinds`, `_FailsAtKeyTable_Unwinds` |
| `Zenith_ConfigStore::Create` | 1 object; 2 per-key layer slots; 3 name index | `Test_ConfigStore_Create_FailsAtObject_Unwinds`, `_FailsAtSlots_Unwinds`, `_FailsAtNameIndex_Unwinds` |
| `Zenith_OperationTable::Create` | 1 object; 2 slot array | `Test_OperationTable_Create_FailsAtObject_Unwinds`, `_FailsAtSlots_Unwinds` |
| `Zenith_BoundedQueue<T>::Create` | 1 object; 2 cells | `Test_BoundedQueue_Create_FailsAtObject_Unwinds`, `_FailsAtCells_Unwinds` |
| `Zenith_JsonDocument::Parse` | 1 object; 2 the backend's allocations (every one, swept); 3 node array; 4 text arena blocks (every one, swept) | `Test_JsonDocument_Parse_FailsAtObject_Unwinds`, `_FailsAtBackend_Unwinds`, `_FailsAtNodes_Unwinds`, `_FailsAtText_Unwinds` (each returns `JSON_READ_ERROR_OUT_OF_MEMORY`) |
| `Zenith_JsonDocument::CreateEmpty` | 1 object; 2 root node | `Test_JsonDocument_CreateEmpty_FailsAtObject_Unwinds`, `_FailsAtRoot_Unwinds` |
| `Zenith_MemoryGovernor::Create` | budget validation (not a fault point: `MEMORY_ERROR_BAD_BUDGETS`); 1 object (`Zenith_TryNew`; `MEMORY_ERROR_OUT_OF_MEMORY`); fault point `uFAULT_POINT_GOVERNOR_LOW_MEMORY_HANDLE` (Windows, in `Zenith_MemoryGovernor.h`) before `CreateMemoryResourceNotification`, whose failure does not fail `Create` (it disables that source and logs a warning) | `Test_MemoryGovernor_Create_FailsAtObject_Unwinds`, `Test_MemoryGovernor_Create_LowMemoryHandleUnavailable_Warns` |
| `Zenith_Thread::Create` | 1 start block; fault point `uFAULT_POINT_THREAD_NATIVE_CREATE` (in `Zenith_Multithreading.h`) before the native creation (`CreateThread`, `pthread_create`) | `Test_Thread_Create_FailsAtStartBlock_Unwinds`, `Test_Thread_Create_FailsAtNativeCreate_Unwinds` (returns `THREAD_ERROR_CREATE_FAILED`, the start block freed, no thread created) |

## Memory tests

Every test below lives in `Zenith2/Engine/ZenithCore/Tests/`. The defect and death rows of `Zenith2/Docs/Contracts/Memory.md` → "Required intentional-defect tests" are required as listed there and are not repeated here; this table adds the tests of the mechanisms this file specifies.

| Test ID | File | Asserts | Suite, presets |
|---|---|---|---|
| `Test_Heap_ZeroSize_AllocatesOneByte` | `Zenith_Heap_Tests.cpp` | `TryAllocate(0, 8)` succeeds, `GetUsableSize ≥ 1` | default, every preset |
| `Test_Heap_Alignment_AllPowersToMax` | `Zenith_Heap_Tests.cpp` | every power of two from 1 to 4096 returns an aligned block; 8192 and 3 fail with `ALLOC_ERROR_ALIGNMENT_UNSUPPORTED` | default, every preset |
| `Test_Heap_OverAligned_FreeRoutesToAlignedFree` | `Zenith_Heap_Tests.cpp` | 10,000 interleaved 16- and 64-aligned allocations and frees in random order leave `m_ulLiveAllocations` at 0 and the over-aligned count at 0 | default, every preset |
| `Test_Heap_OverAligned_UsableSize_Reported` | `Zenith_Heap_Tests.cpp` | a 100-byte, 64-aligned block reports `GetUsableSize ≥ 100`, and its free leaves `m_ulLiveBytes` at its starting value | default, every preset |
| `Test_Heap_Stats_TrackLivePeakTotal` | `Zenith_Heap_Tests.cpp` | each statistic after a scripted sequence equals the expected value computed from `GetUsableSize` | default, every preset |
| `Test_Heap_FaultCountdown_FailsNthOnly` | `Zenith_Heap_Tests.cpp` | with countdown 3, allocations 1, 2, 4 succeed and 3 fails with `ALLOC_ERROR_OUT_OF_MEMORY`; `m_ulFailedAllocations == 1` | default, every preset |
| `Test_Heap_DefaultHandler_ReleasesReserveThenAborts` | `Zenith_Heap_DefectTests.cpp` | a heap whose backing is forced to fail (countdown 1 re-armed each call) first retries once after releasing the reserve, then terminates with `FATAL_CODE_OUT_OF_MEMORY` | `z2-death-check`, every preset |
| `Test_Heap_OOMHandler_RetryAfterRelease_Succeeds` | `Zenith_Heap_Tests.cpp` | a custom handler that re-arms nothing and returns `OOM_RESPONSE_RETRY` once makes `Allocate` succeed on the retry; `Zenith_OOMInfo::m_uAttempt == 1` | default, every preset |
| `Test_Heap_OOMInHandler_Terminates` | `Zenith_Heap_DefectTests.cpp` | an allocation from inside the handler terminates at once | `z2-death-check`, every preset |
| `Test_Heap_OOMRelay_RaisesPressureFlag` | `Zenith_Heap_Tests.cpp` | the default handler with a `Zenith_OOMPressureFlag` user data increments it; `Consume()` returns 1 then 0 | default, every preset |
| `Test_Owned_MoveOnly_NullsSource` | `Zenith_Owned_Tests.cpp` | after a move the source is null and the destructor runs once | default, every preset |
| `Test_Owned_Reset_RunsDestructorAndFrees` | `Zenith_Owned_Tests.cpp` | a counting type's destructor ran once and the heap's live count dropped by one | default, every preset |
| `Test_Owned_TryNew_FailureConstructsNothing` | `Zenith_Owned_Tests.cpp` | with countdown 1, `Zenith_TryNew` returns `ALLOC_ERROR_OUT_OF_MEMORY` and the counting type's construction count is unchanged | default, every preset |
| `Test_Owned_BaseConversion_FirstBase_Converts` | `Zenith_Owned_Tests.cpp` | an `Owned<Derived>` whose base is the first base converts and deletes through the base's virtual destructor | default, every preset |
| `Test_Owned_SizeIsOnePointer` | `Zenith_Owned_Tests.cpp` | `sizeof(Zenith_Owned<T>) == sizeof(void*)` for five unrelated `T` | default, every preset |
| `Test_Arena_Allocate_WithinBlock_ReturnsAlignedMemory` | `Zenith_Arena_Tests.cpp` | for alignments 1, 8, 16, 64 and 4096 within one 1,024-byte block the returned pointer is aligned and allocations do not overlap | default, every preset |
| `Test_Arena_TryAllocate_PastCap_FailsAndLeavesArenaUnchanged` | `Zenith_Arena_Tests.cpp` | with a 256-byte cap, a second 200-byte `TryAllocate` returns `ALLOC_ERROR_CAPACITY_EXCEEDED` and `GetUsedBytes`, `GetReservedBytes` and `GetHighWaterBytes` are unchanged | default, every preset |
| `Test_Arena_ResetToMarker_KeepsEarlierAllocations` | `Zenith_Arena_Tests.cpp` | data written before the marker survives `ResetToMarker`; `GetUsedBytes` returns to the marker's value | default, every preset |
| `Test_Arena_ResetToMarker_WithLiveObject_Checks` | `Zenith_Arena_DefectTests.cpp` | a live non-trivial object after the marker makes `ResetToMarker` fail `ZENITH_CHECK` | `z2-death-check`, every preset |
| `Test_Arena_LargeAllocation_GetsOwnBlock` | `Zenith_Arena_Tests.cpp` | an allocation larger than `m_ulBlockBytes` succeeds within the cap and `GetReservedBytes` grows by its block | default, every preset |
| `Test_Arena_Reset_KeepsRetainedBlocks` | `Zenith_Arena_Tests.cpp` | after growth to 4 blocks and `Reset`, with `m_uRetainedBlocks = 2`, the reserved bytes equal two nominal blocks | default, every preset |
| `Test_Arena_Allocate_AtCap_IsFatal` | `Zenith_Arena_DefectTests.cpp` | `Allocate` past `m_ulMaxBytes` terminates with `FATAL_CODE_ARENA_EXHAUSTED` naming the arena ([Z2-N-CONTRACT-227]) | `z2-death-check`, every preset |
| `Test_Arena_CapOutcomes_IdenticalAcrossPresets` | `Zenith_Arena_Tests.cpp` | [Z2-N-CONTRACT-261] | default, every preset |
| `Test_Pool_FreeForeignPointer_Asserts` | `Zenith_Pool_DefectTests.cpp` | `Free` of a heap pointer fails `ZENITH_ASSERT` | `z2-death-assert` |
| `Test_Pool_DoubleFree_Asserts` | `Zenith_Pool_DefectTests.cpp` | a second `Free` of the same slot fails `ZENITH_ASSERT` | `z2-death-assert` |
| `Test_Function_CaptureInArena_Invokes` | `Zenith_Function_Tests.cpp` | a lambda capturing three values returns their sum; the arena's used bytes grew by the capture size | default, every preset |
| `Test_Function_EmptyCapture_AllocatesNothing` | `Zenith_Function_Tests.cpp` | a captureless lambda leaves `GetUsedBytes` unchanged | default, every preset |
| `Test_Function_TryCreate_PastCap_Fails` | `Zenith_Function_Tests.cpp` | an arena at its cap makes `TryCreate` return `ALLOC_ERROR_CAPACITY_EXCEEDED` | default, every preset |
| `Test_Function_InvokeNull_Asserts` | `Zenith_Function_DefectTests.cpp` | invoking a default-constructed function fails `ZENITH_ASSERT` | `z2-death-assert` |
| `Test_SlotTable_NullDoesNotResolve` | `Zenith_SlotTable_Tests.cpp` | `HANDLE_ERROR_NULL` | default, every preset including `win-release` |
| `Test_SlotTable_StaleAfterDestroy_DoesNotResolve` | `Zenith_SlotTable_Tests.cpp` | `HANDLE_ERROR_STALE` after `Free`, also after the slot is reused | default, every preset |
| `Test_SlotTable_ForeignOwner_DoesNotResolve` | `Zenith_SlotTable_Tests.cpp` | an ID from another table with a larger slot index gives `HANDLE_ERROR_FOREIGN` | default, every preset |
| `Test_SlotTable_GenerationExhaustion_RetiresSlot` | `Zenith_SlotTable_Tests.cpp` | a slot freed at generation `0xFFFFFFFE` is never reissued; the next allocation takes a different slot | default, every preset |
| `Test_SlotTable_FifoReuse_DelaysReissue` | `Zenith_SlotTable_Tests.cpp` | with 8 slots, freeing slot 0 then allocating 7 times never returns slot 0 before the other free slots | default, every preset |
| `Test_SlotTable_PointerStableAcrossGrowth` | `Zenith_SlotTable_Tests.cpp` | a `T*` obtained before 1,000 further allocations still addresses the same value | default, every preset |
| `Test_SlotTable_ForEachLive_AscendingOrder` | `Zenith_SlotTable_Tests.cpp` | after allocating 10 slots and freeing slots 2, 5 and 7, `ForEachLive` visits the seven live values once each in ascending slot order | default, every preset |
| `Test_SlotTable_MatchesOracle` | `Zenith_SlotTable_Tests.cpp` | [Z2-N-CONTRACT-288] against an `std::unordered_map` keyed by `(slot, generation)` | default, every preset |
| `Test_SlotTable_Properties` | `Zenith_SlotTable_Tests.cpp` | [Z2-N-CONTRACT-289]: live count bookkeeping, no double construction or destruction | default, every preset |
| `Test_SlotTable_TryFailure_LeavesUnchanged` | `Zenith_SlotTable_Tests.cpp` | [Z2-N-CONTRACT-290] for page allocation and the hard cap | default, every preset |
| `Test_SlotTable_SizeOverflow_IsRejected` | `Zenith_SlotTable_Tests.cpp` | a desc with `m_uMaxSlots > 2^31` fails `ZENITH_CHECK` at construction | `z2-death-check` |
| `Test_FaultSweep_SelfTest_DetectsLeak` | `Zenith_FaultSweep_Tests.cpp` | a deliberately leaking fake factory makes the sweep report the failing point | default, every preset |
| `Test_TestOutputLineBuffer_SplitsLines` | `Zenith_TestMain_Tests.cpp` | writes of `"a\nb"`, `"c\n\n"` and a 5,000-byte run with no newline give the sink `a`, `bc`, the empty line, then two 2,048-byte pieces, and `Flush()` the last 904 bytes; nothing is emitted twice | default, every preset |
| `Test_TestMain_RouteOption_Parsed` | `Zenith_TestMain_Tests.cpp` | the option scanner of step 2 (`Zenith_TestMainAccess::ParseRouteOption`) maps `stdout` and `tagged`, removes the option from the copy, leaves every other argument in order, and returns -1 for `--zenith-test-output=xml` and for 257 arguments into a 256-entry copy | default, every preset |
| `ZenithCore_Tests.TaggedOutputRoute` (CTest entry, not a doctest case, registered with `zenith_core_output_route_case`) | `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` | `ZenithCore_Tests --zenith-test-output=tagged --test-case=Test_TestOutputLineBuffer_SplitsLines` passes with `PASS_REGULAR_EXPRESSION` `Zenith2Test: \[doctest\] test cases:.*1 passed`, so the route that `SDL_main` selects runs end to end on a host | `win-debug`, `linux-tsan` |
| `Zenith_Integration_Memory.TaggedOutputRoute` (CTest entry, `zenith_core_output_route_case`) | `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` | the cross-system governor suite run with `--zenith-test-output=tagged --test-case=Test_MemoryGovernor_SubsystemReservation_BootedRuntime` passes on the same tagged summary line, so the route serves an integration-suite executable as well as a library's tests | `win-debug`, `linux-tsan` |
| `ZenithCore_Tests.UnknownOutputRoute` (CTest entry, `zenith_core_output_route_case`) | `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` | `ZenithCore_Tests --zenith-test-output=xml` prints `Zenith_TestMain: unknown --zenith-test-output value 'xml'` and runs no test case (the entry passes on that `PASS_REGULAR_EXPRESSION`, whatever the exit code, as `zenith_core_selftest_case` entries do; the exit code 2 is asserted by `Test_TestMain_RouteOption_Parsed`'s -1 return) | `win-debug` |
| `Test_TestProcess_ChildExitCodeReturned` | `Zenith_TestProcess_Tests.cpp` | `Zenith_RunChildTestCase("ZenithCore/TestProcessChild/Behave", { ZENITH_TEST_CHILD_MODE = exit, ZENITH_TEST_CHILD_CODE = 77 }, 60)` returns 77, and code 0 returns 0 | default, `win-debug`, `win-release`, `linux-tsan` |
| `Test_TestProcess_StdoutCaptured` | `Zenith_TestProcess_Tests.cpp` | with `m_bCaptureStdout`, a child in mode `echo` prints `ZENITH_TEST_CHILD_LINE=<value>` and `GetStdout()` contains exactly that line; a child printing 300 KiB is captured whole (the pipe is drained while it runs) | default, same presets |
| `Test_TestProcess_VariablesAndArgumentsReachChild` | `Zenith_TestProcess_Tests.cpp` | a variable replacing one the parent has, a new variable, and the extra arguments `a b`, `"q"`, `c:\x\` and an empty string reach a child in mode `args`, which echoes them verbatim | default, same presets |
| `Test_TestProcess_TimeoutThenKill` | `Zenith_TestProcess_Tests.cpp` | a child in mode `sleep` makes `Wait(200)` return `TEST_PROCESS_ERROR_TIMED_OUT` with the child still running; `Kill()` then reports `{ −1, true }`; `Zenith_RunChildTestCase` with a 1 s timeout returns `TIMED_OUT` within 5 s | default, same presets |
| `Test_TestProcess_DestructorKillsRunningChild` | `Zenith_TestProcess_Tests.cpp` | destroying the object of a sleeping child kills it: the child's `ZENITH_TEST_CHILD_PIDFILE` process no longer exists within 5 s | default, same presets |
| `Test_TestProcess_OutputCapExceeded` | `Zenith_TestProcess_Tests.cpp` | a child in mode `flood` (unbounded output) makes `Wait` return `TEST_PROCESS_ERROR_OUTPUT_TOO_LARGE` and the child is gone | default, same presets |
| `Test_TestProcess_BadArguments_Rejected` | `Zenith_TestProcess_Tests.cpp` | an empty case name, 17 variables, 9 extra arguments and a name containing `=` return `TEST_PROCESS_ERROR_BAD_ARGUMENT` and launch nothing; on Android every launch returns `TEST_PROCESS_ERROR_UNSUPPORTED` and `Zenith_IsTestProcessSupported()` is false | default, every preset |
| `Test_TestProcess_ExecutablePathIsSelf` | `Zenith_TestProcess_Tests.cpp` | `Zenith_GetTestExecutablePath()` names an existing file whose bytes begin with `MZ` on Windows and `ELF` on Linux, and a child in mode `path` prints the same path | default, `win-debug`, `linux-tsan` |
| `Test_TestProcess_LaunchSelf_FailsAtArguments_Unwinds` | `Zenith_TestProcess_Tests.cpp` | with the heap countdown at 1, `LaunchSelf` returns `TEST_PROCESS_ERROR_OUT_OF_MEMORY`, launches no child and leaks nothing | default, every preset |
| `Test_TestProcessChild_Behave` | `Zenith_TestProcess_Tests.cpp` | the child role, `* doctest::skip()`: reads `ZENITH_TEST_CHILD_MODE` (`exit`, `echo`, `args`, `sleep`, `flood`, `path`) and behaves as the parent cases above require; never run by discovery | skipped |
