# Zenith2 engineering contracts: memory mechanisms

<!-- informative -->
This file belongs to the `Zenith2/Docs/Contracts.md` document family and shares its normative tag family `Z2-N-CONTRACT` and its grammar. It holds the mechanism specifications of `Zenith2/Docs/Contracts.md → "Ownership and allocation"`, whose allocation tiers and core memory header table stay in the entry document. Cites of the entry document's sections are written `Zenith2/Docs/Contracts.md → "<heading>"`; code snippets omit their contract blocks for brevity, and a `<!-- elaboration -->` marker is a review aid.
<!-- /informative -->

## Zenith_Heap

<!-- elaboration -->
```cpp
enum Zenith_AllocError : uint8
{
	ALLOC_ERROR_OUT_OF_MEMORY = 1,          // the backing allocator failed
	ALLOC_ERROR_SIZE_OVERFLOW,              // size or rounding arithmetic overflowed
	ALLOC_ERROR_ALIGNMENT_UNSUPPORTED,      // alignment not a power of two, or above ulHEAP_MAX_ALIGNMENT
	ALLOC_ERROR_CAPACITY_EXCEEDED,          // an arena or pool reached its hard cap
};

enum Zenith_OOMResponse : uint8
{
	OOM_RESPONSE_RETRY,     // the handler released memory; retry the allocation
	OOM_RESPONSE_ABORT,     // terminate through ZENITH_FATAL(FATAL_CODE_OUT_OF_MEMORY)
};

struct Zenith_OOMInfo
{
	uint64 m_ulRequestedBytes = 0;
	uint64 m_ulAlignment = 0;
	uint32 m_uAttempt = 0;                 // 1 on the first failure of this request
	const char* m_szAllocatorName = nullptr; // "general heap", or the arena or pool name
};

using Zenith_PfnOOMHandler = Zenith_OOMResponse (*)(const Zenith_OOMInfo& xInfo, void* pUserData);

struct Zenith_HeapDesc
{
	Zenith_PfnOOMHandler m_pfnOOMHandler = nullptr;  // nullptr selects the default handler
	void* m_pOOMUserData = nullptr;
	uint32 m_uMaxOOMRetries = 3;
	uint64 m_ulEmergencyReserveBytes = 16ull * 1024 * 1024;  // released by the default handler on first OOM
	uint64 m_ulFailAfterAllocations = 0;          // fault injection: 0 = off; N = the Nth allocation fails
};

struct Zenith_HeapStats
{
	uint64 m_ulLiveBytes = 0;          // usable bytes currently allocated
	uint64 m_ulLiveAllocations = 0;
	uint64 m_ulPeakBytes = 0;
	uint64 m_ulTotalAllocations = 0;
	uint64 m_ulFailedAllocations = 0;
};

constexpr uint64 ulHEAP_MAX_ALIGNMENT = 4096;

class Zenith_Heap
{
public:
	explicit Zenith_Heap(const Zenith_HeapDesc& xDesc);   // tries to reserve the emergency block; never fails
	~Zenith_Heap();                                        // reports leaks (Debug), releases the reserve
	Zenith_Heap(const Zenith_Heap&) = delete;
	Zenith_Heap& operator=(const Zenith_Heap&) = delete;

	Zenith_Result<void*, Zenith_AllocError> TryAllocate(uint64 ulSize, uint64 ulAlignment);
	void* Allocate(uint64 ulSize, uint64 ulAlignment);   // never nullptr
	void Free(void* pMemory);                                            // nullptr is a no-op
	uint64 GetUsableSize(const void* pMemory) const;
	Zenith_HeapStats GetStats() const;
	uint64 ReportLeaks() const;   // Debug: logs every live allocation, returns the count; else 0

private:
	// backing-allocator state, OOM state, and the Debug leak side table pointer (nullptr outside Debug builds)
};

void Zenith_InstallRootHeap(Zenith_Heap* pxHeap);     // nullptr uninstalls
Zenith_Heap& Zenith_GetRootHeap();      // ZENITH_CHECK: installed
```

- [Z2-N-CONTRACT-194] `Zenith_Heap` is a thin wrapper over the backing allocator selected by D-SEL-HEAP (open, Stage 1); it stores no per-allocation header, and `Free` takes no size because the backing allocator tracks sizes internally.
- [Z2-N-CONTRACT-195] `Zenith_Heap::Free` accepts exactly the pointers `Allocate` and `TryAllocate` returned, and nullptr; freeing any other pointer is a programmer error that the sanitizer builds catch natively.
- [Z2-N-CONTRACT-196] A size of 0 is allocated as 1 byte; an alignment that is not a power of two or exceeds `ulHEAP_MAX_ALIGNMENT` fails with `ALLOC_ERROR_ALIGNMENT_UNSUPPORTED` from `TryAllocate` and is a `ZENITH_CHECK` failure in `Allocate`.
- [Z2-N-CONTRACT-197] `Zenith_Heap::TryAllocate` never invokes the OOM handler; `Zenith_Heap::Allocate` invokes it on failure and never returns nullptr.
- [Z2-N-CONTRACT-198] Exactly one `Zenith_Heap` is installed as the root heap per process, it is installed before any other engine object exists, and it is uninstalled after the last engine object is destroyed.
- [Z2-N-CONTRACT-199] The heap constructor is infallible: when the emergency reserve cannot be allocated the heap runs without one, logs the fact, and the default handler then aborts on the first failure.
- [Z2-N-CONTRACT-200] `m_ulFailAfterAllocations` is a runtime field present in every configuration; test mains set it to drive allocation-failure injection, and the Player never sets it.

## Zenith_Owned

<!-- elaboration -->
```cpp
template<typename TDerived, typename TBase>
concept Zenith_OwnedConvertible = std::is_base_of_v<TBase, TDerived>
	&& std::has_virtual_destructor_v<TBase>
	&& !std::is_same_v<std::remove_cv_t<TBase>, std::remove_cv_t<TDerived>>;

template<typename T>
class Zenith_Owned
{
public:
	constexpr Zenith_Owned() = default;
	constexpr Zenith_Owned(std::nullptr_t) {}
	Zenith_Owned(const Zenith_Owned&) = delete;
	Zenith_Owned& operator=(const Zenith_Owned&) = delete;
	Zenith_Owned(Zenith_Owned&& xOther);                 // xOther becomes null
	Zenith_Owned& operator=(Zenith_Owned&& xOther);      // deletes the current object first
	template<typename TDerived> requires Zenith_OwnedConvertible<TDerived, T>
	Zenith_Owned(Zenith_Owned<TDerived>&& xOther);       // ZENITH_CHECK: same address
	~Zenith_Owned();                                     // Zenith_Delete(m_pxObject)

	T* Get() const { return m_pxObject; }
	T& operator*() const;                  // ZENITH_ASSERT: non-null
	T* operator->() const;                 // ZENITH_ASSERT: non-null
	explicit operator bool() const { return m_pxObject != nullptr; }
	void Reset();                                        // Zenith_Delete, then null

private:
	template<typename> friend class Zenith_Owned;
	template<typename U, typename... TArgs> friend Zenith_Owned<U> Zenith_New(TArgs&&... xArgs);
	template<typename U, typename... TArgs>
	friend Zenith_Result<Zenith_Owned<U>, Zenith_AllocError> Zenith_TryNew(TArgs&&... xArgs);
	explicit Zenith_Owned(T* pxAdopted) : m_pxObject(pxAdopted) {}

	T* m_pxObject = nullptr;
};
static_assert(sizeof(Zenith_Owned<int>) == sizeof(void*));
```

- [Z2-N-CONTRACT-201] `Zenith_Owned<T>` is exactly `{T*}`: the same layout in every configuration, move-only, with no reference count, no deleter and no header.
- [Z2-N-CONTRACT-202] A `Zenith_Owned<T>` is created by `Zenith_New<T>` or `Zenith_TryNew<T>` and by nothing else; its adopting constructor is private.
- [Z2-N-CONTRACT-203] Nothing but the general heap backs a `Zenith_Owned`; an arena or pool object is never wrapped in one.
- [Z2-N-CONTRACT-204] `Zenith_Owned` exposes no `Release()`; ownership moves only by move construction or move assignment, and a raw owning pointer exists only inside ZenithCore's memory implementation.
- [Z2-N-CONTRACT-205] `Zenith_Owned<T>` is a valid element of Zenith2 containers, which move it and never copy it.

## Zenith_New Zenith_TryNew and Zenith_Delete

<!-- elaboration -->
```cpp
template<typename T, typename... TArgs>
Zenith_Owned<T> Zenith_New(TArgs&&... xArgs);

template<typename T, typename... TArgs>
Zenith_Result<Zenith_Owned<T>, Zenith_AllocError> Zenith_TryNew(TArgs&&... xArgs);

template<typename T>
void Zenith_Delete(T* pxObject);     // called by Zenith_Owned alone
```

`Zenith_New<T>(xArgs…)` runs these steps:

1. `static_assert(!std::is_array_v<T>)`, `static_assert(std::is_constructible_v<T, TArgs...>)`, `static_assert(alignof(T) <= ulHEAP_MAX_ALIGNMENT)`.
2. `void* pMemory = Zenith_GetRootHeap().Allocate(sizeof(T), alignof(T));` which reaches the OOM handler on failure.
3. `T* pxObject = ::new (pMemory) T(std::forward<TArgs>(xArgs)...);`
4. `return Zenith_Owned<T>(pxObject);`

`Zenith_TryNew<T>` performs the same steps with `TryAllocate` and returns `Zenith_Err(ALLOC_ERROR_OUT_OF_MEMORY)` (or the heap's error) without constructing anything. `Zenith_Delete(pxObject)` returns on nullptr, otherwise `static_assert(sizeof(T) > 0)` (rejecting an incomplete type), calls `pxObject->~T()`, then `Zenith_GetRootHeap().Free(pxObject)`.

- [Z2-N-CONTRACT-206] `Zenith_New`, `Zenith_TryNew` and `Zenith_Delete` implement the steps listed above, in every configuration.
- [Z2-N-CONTRACT-207] A type allocated with `Zenith_New` or `Zenith_TryNew` is constructible from the arguments used (step 1), with no exception specification ([Z2-N-CONTRACT-582]; exceptions are off, so a constructor cannot throw); constructors perform no fallible work (see "Fallible construction").
- [Z2-N-CONTRACT-208] `Zenith_Delete` is called from `Zenith_Owned.h` and from nowhere else; `ForbiddenCheck` rejects any other call site.
- [Z2-N-CONTRACT-209] `Zenith_Owned` of an incomplete type is declared only where its destructor is not instantiated; the `static_assert` in `Zenith_Delete` rejects deleting an incomplete type.

## Owned conversion to a base

- [Z2-N-CONTRACT-210] `Zenith_Owned<TDerived>` converts to `Zenith_Owned<TBase>` only when `TBase` has a virtual destructor (the `Zenith_OwnedConvertible` concept); there is no RTTI and no `dynamic_cast`.
- [Z2-N-CONTRACT-211] The converting constructor evaluates `ZENITH_CHECK(static_cast<const void*>(static_cast<TBase*>(pxDerived)) == static_cast<const void*>(pxDerived))` for a non-null pointer in every configuration, because the heap frees by the original pointer.
- [Z2-N-CONTRACT-212] A conversion that fails the same-address check is a programmer error; the fix is to make the target base the first base class, never to adjust the pointer.
- [Z2-N-CONTRACT-213] The test `Test_Owned_BaseConversion_SameAddress_Checks` is a death test (see `Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"`) converting through a second base at a non-zero offset and expecting the `ZENITH_CHECK` report in every configuration.

## OOM handler

- [Z2-N-CONTRACT-214] `Zenith_New`, `Zenith_Heap::Allocate`, and every non-`Try` allocating operation of arenas, pools and containers route allocation failure to the root heap's OOM handler.
- [Z2-N-CONTRACT-215] The OOM handler runs on the allocating thread, is serialised by the heap's OOM lock (`uLOCK_LEVEL_HEAP_OOM`), allocates nothing from the root heap, and acquires no lock at a level below `uLOCK_LEVEL_HEAP_OOM`.
- [Z2-N-CONTRACT-216] On `OOM_RESPONSE_RETRY` the heap retries the allocation; after `m_uMaxOOMRetries` retries, or on `OOM_RESPONSE_ABORT`, it terminates through `ZENITH_FATAL(FATAL_CODE_OUT_OF_MEMORY, …)` with the `Zenith_OOMInfo` fields and the heap statistics in the report.
- [Z2-N-CONTRACT-217] An allocation failure on a thread that is already inside the OOM handler terminates at once through `ZENITH_FATAL(FATAL_CODE_OUT_OF_MEMORY, …)`.
<!-- elaboration -->
- [Z2-N-CONTRACT-218] The default handler releases the emergency reserve on the first failure and returns `OOM_RESPONSE_RETRY`, then returns `OOM_RESPONSE_ABORT`; it raises the memory governor's critical pressure flag through the user-data pointer installed in `main`, so shedding runs at the next sync point, never inside the handler (see `Zenith2/Docs/Architecture.md → "Memory governor"`).

## Non-owning references

- [Z2-N-CONTRACT-219] A non-owning reference is a plain `T&` or `T*`, scoped to the duration of a call or to a lifetime the holder's contract block documents; there is no `Zenith_Ptr` and no weak-pointer type.
- [Z2-N-CONTRACT-220] A reference to an object shared between systems or across frames (entities, components, assets, GPU resources, operations) is a generational handle, never a stored `T*` (Z2-D-040).
- [Z2-N-CONTRACT-221] A stored `T*` member names its target's owner and the guarantee that the target outlives the holder, in the holder's contract block `Ownership` field; review confirms the lifetime reasoning of every new owner.

## Fallible construction

- [Z2-N-CONTRACT-222] A type whose construction can fail has a constructor that performs no fallible work and is either private or takes a private passkey type (`PrivateTag`) so that `Zenith_TryNew` can call it, and a static factory returning `Zenith_Result<Zenith_Owned<T>, E>` (or `Zenith_Result<T, E>` for a movable value type) that performs each fallible step.
- [Z2-N-CONTRACT-223] Each partially constructed member is released by its own owner: members are `Zenith_Owned`, handles or containers whose destructors release what was built, so a factory that fails at step N returns its error and the partially built object's destructor unwinds steps 1 … N−1 exactly.
- [Z2-N-CONTRACT-224] A destructor tolerates every partially initialised state its factory can leave behind.
<!-- elaboration -->
- [Z2-N-CONTRACT-225] Every fallible construction step has a failure-injection test: allocation steps through `Zenith_HeapDesc::m_ulFailAfterAllocations`, other steps through a `Zenith_FaultInjector*` in the factory's desc (`nullptr` in production) whose `bool ShouldFail(uint32 uFaultPointID)` is consulted at each named fault point; the test enumerates every fault point and asserts no leak and no half-registered state (see `Zenith2/Docs/Contracts.md → "Threading and concurrency contract"`).
- [Z2-N-CONTRACT-255] `Zenith_FaultInjector` is an interface in ZenithCore's `Zenith_FaultInjector.h`: `class Zenith_FaultInjector { public: virtual ~Zenith_FaultInjector() = default; virtual bool ShouldFail(uint32 uFaultPointID) = 0; };`; each factory declares its fault points in its public header as `constexpr uint32 uFAULT_POINT_<TYPE>_<STEP>` values.
- [Z2-N-CONTRACT-262] Fault-point values carry a 16-bit library prefix in their high half and a value unique within that library in their low half (ZenithCore uses `0x434F`, "CO", so its points are `0x434F0001`, `0x434F0002`, …; COMPRESS `0x434D`, ECS `0x4543`, Reflect `0x5246`), so one `ShouldFail(uint32)` value space stays unambiguous when a factory forwards its injector into another library's nested factory; the registry is the `uFAULT_POINT_*` constants themselves, with no central table, and a library picks a prefix no other library's constants use.

<!-- informative -->
A factory in this shape reads as follows (the full example lives in `Zenith2/Docs/WorkerGuide.md`):
<!-- /informative -->

```cpp
Zenith_Result<Zenith_Owned<Zenith_Streamer>, Zenith_StreamError> Zenith_Streamer::Create(
	const Zenith_StreamerDesc& xDesc, Zenith_FileSystem& xFiles)
{
	Zenith_Result<Zenith_Owned<Zenith_Streamer>, Zenith_AllocError> xAlloc = Zenith_TryNew<Zenith_Streamer>(PrivateTag{}, xFiles);
	if (xAlloc.IsError())
	{
		return Zenith_Err(STREAM_ERROR_OUT_OF_MEMORY);   // a different error enumeration is mapped explicitly
	}
	Zenith_Owned<Zenith_Streamer> xStreamer = std::move(xAlloc).TakeValue();
	ZENITH_TRY(xStreamer->CreateQueues(xDesc));      // on failure xStreamer's destructor releases the queues built so far
	ZENITH_TRY(xStreamer->CreateWorkers(xDesc));
	return xStreamer;
}
```

## Zenith_Arena

<!-- elaboration -->
```cpp
struct Zenith_ArenaDesc
{
	const char* m_szName = "arena";
	uint64 m_ulBlockBytes = 64ull * 1024;     // size of the first and of each growth block
	uint64 m_ulMaxBytes = 0;                  // hard cap on nominal block bytes (sanitizer overhead excluded); 0 means m_ulBlockBytes
	uint32 m_uRetainedBlocks = 1;             // blocks kept by Reset()
};

struct Zenith_ArenaMarker
{
	uint32 m_uBlockIndex = 0;
	uint64 m_ulOffset = 0;
	uint32 m_uLiveNonTrivial = 0;
};

class Zenith_Arena
{
public:
	explicit Zenith_Arena(const Zenith_ArenaDesc& xDesc);   // allocates nothing
	~Zenith_Arena();                                         // ZENITH_CHECK: no live non-trivial objects
	Zenith_Arena(const Zenith_Arena&) = delete;
	Zenith_Arena& operator=(const Zenith_Arena&) = delete;

	Zenith_Result<void*, Zenith_AllocError> TryAllocate(uint64 ulSize, uint64 ulAlignment);
	void* Allocate(uint64 ulSize, uint64 ulAlignment);   // never nullptr
	Zenith_ArenaMarker GetMarker() const;
	void ResetToMarker(const Zenith_ArenaMarker& xMarker);
	void Reset();
	uint64 GetUsedBytes() const;
	uint64 GetReservedBytes() const;
	uint64 GetHighWaterBytes() const;
};

template<typename T, typename... TArgs>
T* Zenith_Construct(Zenith_Arena& xArena, TArgs&&... xArgs);
template<typename T, typename... TArgs>
Zenith_Result<T*, Zenith_AllocError> Zenith_TryConstruct(Zenith_Arena& xArena, TArgs&&... xArgs);
template<typename T>
T* Zenith_ConstructArray(Zenith_Arena& xArena, uint64 ulCount);   // value-initialised
template<typename T>
void Zenith_Destroy(Zenith_Arena& xArena, T* pxObject);
```

`Zenith_Arena::TryAllocate` runs these steps:

1. Reject a non-power-of-two alignment or one above `ulHEAP_MAX_ALIGNMENT` with `ALLOC_ERROR_ALIGNMENT_UNSUPPORTED`; treat size 0 as 1.
2. In sanitizer builds raise the alignment to 8 (ASan) or 16 (HWASan) and add the redzone or granule rounding of "ASan poisoning rules" and "HWASan tagging rules"; every addition is overflow-checked and fails with `ALLOC_ERROR_SIZE_OVERFLOW`.
3. Align the current block's cursor up; if the aligned end fits in the block, advance the cursor and go to step 6.
4. Otherwise move to the next retained block, or allocate a new block of `max(m_ulBlockBytes, size + alignment + redzone)` from the root heap with `TryAllocate`, provided the nominal total (defined in the bullet on `m_ulMaxBytes` below) stays within `m_ulMaxBytes`; an allocation larger than `m_ulBlockBytes` gets a block of its own.
5. Fail with `ALLOC_ERROR_CAPACITY_EXCEEDED` when the cap would be exceeded, or with the heap's error when the block allocation fails.
6. Unpoison or tag the returned range, update the high-water mark, and return the pointer.

- [Z2-N-CONTRACT-226] `Zenith_Arena` implements the allocation steps listed above; allocation is O(1) amortised and a failed `TryAllocate` leaves the arena unchanged.
- [Z2-N-CONTRACT-227] `Zenith_Arena::Allocate` routes a failed block allocation through the OOM handler and terminates through `ZENITH_FATAL(FATAL_CODE_ARENA_EXHAUSTED, …)`, naming the arena and its high-water mark, when the cap is reached; arena caps are budget rows, so exhausting one is a defect.
- [Z2-N-CONTRACT-228] The arena constructor allocates nothing; the first block is allocated by the first allocation, so construction is infallible.
- [Z2-N-CONTRACT-261] `m_ulMaxBytes` caps the preset-independent nominal figure: the sum of the requested sizes plus the padding of the requested alignments, which `GetUsedBytes()` reports, and the nominal block sizes, which `GetReservedBytes()` reports. Sanitizer overhead (granule rounding, redzones, guard granules) is outside the cap, and blocks are requested from the root heap with room for it, so the sequence of `TryAllocate` outcomes is identical in every preset; `Test_Arena_CapOutcomes_IdenticalAcrossPresets` runs one fixed sequence in every preset and asserts the same successes and failures.
- [Z2-N-CONTRACT-229] Objects in an arena are created with `Zenith_Construct`, `Zenith_TryConstruct` or `Zenith_ConstructArray` and ended with `Zenith_Destroy`; a type that is not trivially destructible increments the arena's live non-trivial count on construction and decrements it on `Zenith_Destroy`.
- [Z2-N-CONTRACT-230] `Reset()` and `ResetToMarker()` evaluate `ZENITH_CHECK` that no non-trivially-destructible object constructed after the reset point is still live, in every configuration.
- [Z2-N-CONTRACT-231] `Reset()` poisons or retags every used range, keeps the first `m_uRetainedBlocks` blocks, returns the rest to the root heap, and moves the cursor to the first block's start; `ResetToMarker()` poisons or retags from the marker to the cursor and keeps every block.
- [Z2-N-CONTRACT-232] Graph captures (render-graph pass lambdas, CPU-frame-graph node captures, and every `Zenith_Function<TSignature>` stored in an arena) are trivially destructible, enforced by `static_assert(std::is_trivially_destructible_v<…>)` at the capture site; `Zenith_Function` is ZenithCore's arena-stored type-erased callable, specified in `Zenith2/Docs/Design/CORE.md` and, being custom infrastructure, carrying its own justification record.
- [Z2-N-CONTRACT-233] An arena is used by one thread at a time; parallel work uses one arena per worker, and a cross-thread hand-off happens at a graph edge or sync point.

## Zenith_Pool

<!-- elaboration -->
```cpp
struct Zenith_PoolDesc
{
	const char* m_szName = "pool";
	uint32 m_uSlotBytes = 0;          // Zenith_RawPool; Zenith_Pool<T> uses sizeof(T)
	uint32 m_uSlotAlignment = 8;      // Zenith_Pool<T> uses alignof(T)
	uint32 m_uSlotsPerPage = 256;
	uint32 m_uMaxSlots = 0;           // hard cap; 0 means m_uSlotsPerPage
};

class Zenith_RawPool
{
public:
	explicit Zenith_RawPool(const Zenith_PoolDesc& xDesc);   // allocates nothing
	~Zenith_RawPool();                                         // ZENITH_CHECK: no live slots
	Zenith_RawPool(const Zenith_RawPool&) = delete;
	Zenith_RawPool& operator=(const Zenith_RawPool&) = delete;

	Zenith_Result<void*, Zenith_AllocError> TryAllocate();
	void* Allocate();             // never nullptr
	void Free(void* pSlot);                     // ZENITH_ASSERT: pSlot belongs to this pool and is live
	uint32 GetLiveSlots() const;
	uint32 GetCapacitySlots() const;    // m_uMaxSlots in every preset
};

template<typename T>
class Zenith_Pool
{
public:
	explicit Zenith_Pool(const Zenith_PoolDesc& xDesc);   // m_uSlotBytes and m_uSlotAlignment are taken from T
	template<typename... TArgs> T* Create(TArgs&&... xArgs);
	template<typename... TArgs> Zenith_Result<T*, Zenith_AllocError> TryCreate(TArgs&&... xArgs);
	void Destroy(T* pxObject);                  // runs ~T, then frees the slot
private:
	Zenith_RawPool m_xRawPool;
};
```

- [Z2-N-CONTRACT-234] A pool hands out fixed-size slots from pages allocated from the root heap; allocation and free are O(1), and a failed `TryAllocate` leaves the pool unchanged.
- [Z2-N-CONTRACT-235] The pool's free list lives out of line, as a per-page `uint32` slot-index array, so the allocator never reads or writes a freed slot's memory, which stays poisoned or retagged.
- [Z2-N-CONTRACT-236] In sanitizer builds a freed slot waits in a FIFO quarantine of `min(64, m_uMaxSlots / 4, m_uSlotsPerPage)` slots before reuse, so a use after free is caught before the slot is handed out again; quarantined slots are additional to `m_uMaxSlots` (the pool allocates up to one extra page to hold them), so the number of slots a caller can hold live is exactly `m_uMaxSlots` in every preset, and `GetCapacitySlots()` returns `m_uMaxSlots` in every preset.
- [Z2-N-CONTRACT-237] A pool is used by one thread at a time; a concurrent pool is a separate CORE container with its own justification record.

## Bare allocation boundary

- [Z2-N-CONTRACT-238] Bare `new`, `delete`, `new[]`, `delete[]`, `malloc`, `calloc`, `realloc`, `free`, `_aligned_malloc`, `_aligned_free`, `aligned_alloc`, `posix_memalign`, `operator new`, `operator delete`, `VirtualAlloc` and `mmap` appear only inside `Zenith2/Engine/ZenithCore/Internal/Memory/`, which implements `Zenith_Heap`, the arenas and the pools; `ForbiddenCheck` fails any other occurrence.
- [Z2-N-CONTRACT-239] Placement `new` appears only in ZenithCore's memory and container headers and their `Internal/` sources; all other code constructs through `Zenith_New`, `Zenith_Construct`, pool `Create` or a container.
- [Z2-N-CONTRACT-240] A third-party library's allocation hooks (Jolt, PhysX, Luau, flecs, EnTT, VMA, D3D12MA, SDL3, ImGui, Tracy, zstd, zlib, miniaudio) are routed to the root heap or to an arena by its wrapper under `Internal/`, where the library offers a hook.

## Debug leak side table

<!-- informative -->
The plan's side table maps an address to its size and source location. `Zenith_New<T>(TArgs&&...)` cannot take a defaulted `std::source_location` parameter after its argument pack; a `ZENITH_NEW(T, …)` macro forwarding `std::source_location::current()` to a `Zenith_NewAt<T>(Zenith_AllocSite, …)` would. That alternative was rejected because it adds a macro to every allocation site in the engine and still names only the immediate caller, while captured return addresses give the whole call path and leave `Zenith_New` a plain function template.
<!-- /informative -->

- [Z2-N-CONTRACT-241] In Debug builds the general heap keeps a side table keyed by allocation address, holding the size, the alignment, an allocation sequence number and up to eight return addresses captured inside `Zenith_Heap::Allocate` and `Zenith_Heap::TryAllocate`, with `RtlCaptureStackBackTrace` on Windows and `_Unwind_Backtrace` on Android and Linux, skipping the heap's own frames; outside Debug builds the table pointer is nullptr and nothing is recorded.
- [Z2-N-CONTRACT-256] The side table is an open-addressing hash map with linear probing, keyed by the allocation address hashed with a 64-bit multiplicative mix, with a power-of-two capacity starting at 65,536 entries and doubling when the load factor would exceed 0.5; deletions use backward-shift removal, so no tombstones accumulate.
- [Z2-N-CONTRACT-242] The side table adds no per-allocation header and changes no object's layout in any configuration.
- [Z2-N-CONTRACT-243] The side table's own storage comes from the backing allocator directly, never from the root heap, so recording an allocation never recurses; it is guarded by `uLOCK_LEVEL_HEAP_LEAK_TABLE`, the highest level in the lock ordering table.
- [Z2-N-CONTRACT-244] `Zenith_Heap::ReportLeaks()` takes `uLOCK_LEVEL_HEAP_LEAK_TABLE`, copies the live entries into a buffer allocated from the backing allocator, releases the lock, sorts the copy by sequence number and only then logs, so no log-sink lock is acquired while the leak-table lock is held; the heap destructor calls it after every other thread has been joined.
- [Z2-N-CONTRACT-257] On Windows each reported entry prints its size and its first return address outside ZenithCore's memory code symbolised to file and line through dbghelp (`SymInitializeW`, `SymFromAddrW`, `SymGetLineFromAddrW64`), with the executable's PDB beside it.
- [Z2-N-CONTRACT-258] On Android and Linux each reported entry prints its return addresses as `<module>+0x<offset>` lines, with each module path and load base found through `dladdr`; `Zenith2/Tools/Setup/Symbolize-Leaks.ps1` turns a captured report into file and line with `llvm-symbolizer` from the pinned NDK or LLVM against the unstripped libraries of that build.
<!-- elaboration -->
- [Z2-N-CONTRACT-245] A test executable whose root heap reports one or more leaks at shutdown exits with code 3, so CTest fails the run; the Player in a Debug build logs the report and exits normally.

## Sanitizer instrumentation

- [Z2-N-CONTRACT-246] The general heap is instrumented natively by ASan (`win-asan`) and HWASan (the S24 HWASan build type), because sanitizer builds use the system allocator.
- [Z2-N-CONTRACT-247] Arenas and pools poison and unpoison through `__asan_poison_memory_region` and `__asan_unpoison_memory_region` under ASan, and tag through `__hwasan_tag_memory` and `__hwasan_tag_pointer` under HWASan, following the two rule tables below.
- [Z2-N-CONTRACT-248] The sanitizer calls live in `Zenith2/Engine/ZenithCore/Internal/Memory/Zenith_SanitizerMemory.cpp`, compiled to empty inline stubs outside sanitizer builds; this conditional changes behaviour and no layout (see `Zenith2/Docs/Contracts.md → "No layout-changing conditionals"`).
- [Z2-N-CONTRACT-249] A suppression file for any sanitizer is allowed only with a `Z2-D-` decision ID recorded in the file beside each suppression (Z2-D-011).

## ASan poisoning rules

<!-- elaboration -->
| # | Event | Action |
|---|---|---|
| A1 | a block or page is acquired from the root heap | poison its whole range |
| A2 | an allocation of `n` bytes is returned at `p` | unpoison exactly `[p, p + n)`; ASan's shadow encodes the partial last 8-byte granule |
| A3 | arena allocation placement | `p` is 8-aligned; 16 poisoned redzone bytes follow `p + n` before the next allocation may start |
| A4 | pool slot stride | `align_up(slotBytes, 8) + 16`, the trailing 16 bytes of every slot staying poisoned |
| A5 | a pool slot is freed | poison the whole slot, then enqueue it in the quarantine |
| A6 | `Reset()` or `ResetToMarker()` | poison every range released |
| A7 | a block or page is returned to the root heap | unpoison its whole range first, so the general heap's own instrumentation takes over |

- [Z2-N-CONTRACT-250] Arenas and pools under ASan follow rules A1 to A7 of the table "ASan poisoning rules".

## HWASan tagging rules

<!-- elaboration -->
| # | Event | Action |
|---|---|---|
| H0 | a block or page is acquired from the root heap | retag its whole range with a fresh free tag `f ∈ 16 … 255`, recorded per block or page; unallocated granules keep a free tag until allocated |
| H1 | granule | 16 bytes; every allocation starts 16-aligned (the effective alignment is `max(requested, 16)`) |
| H2 | size rounding | the tagged size is `round_up(n, 16)`, computed overflow-checked: `n > UINT64_MAX − 15` fails with `ALLOC_ERROR_SIZE_OVERFLOW` |
| H3 | tag source | a pointer tag `t` is drawn from 16 … 255 by a per-allocator xorshift32 sequence; values 1 … 15 are reserved for short-granule encoding and 0 means untagged |
| H4 | neighbour rule | `t` differs from the tags of both physically adjacent granules within the same block or page, whether they belong to live allocations or carry a free tag (the granule before a block or page belongs to the root heap and is not consulted) (re-drawn on collision); pools store one tag byte per slot out of line |
| H5 | full granules | `__hwasan_tag_memory(p, t, floor(n / 16) * 16)` |
| H6 | short granule | when `r = n % 16` is non-zero, the last granule's memory tag is set to `r` and byte 15 of that granule stores `t`; that byte is written by a function compiled with `__attribute__((no_sanitize("hwaddress")))` |
| H7 | returned pointer | `__hwasan_tag_pointer(p, t)` |
| H8 | free or reset | retag the whole rounded range with a fresh `t' ∈ 16 … 255` that differs from `t` and from the tags of both physically adjacent granules, as full granules (the short-granule encoding is cleared) |
| H9 | block or page returned to the root heap | retag the whole range with the tag of the pointer the root heap returned for it, then free that original pointer |
| H10 | address arithmetic | allocator bookkeeping compares untagged addresses (`__hwasan_tag_pointer(p, 0)`) |
| H11 | guard granule | the last granule of every block or page is a guard granule: it is never allocated and keeps the free tag `f`; the size requested from the root heap for a block or page is `round_up(requested, 16) + 16` |
| H12 | pool slot stride | `round_up(slotBytes, 16)` (at least 16), so byte `n` of a slot whose size is a whole number of granules is the next slot's first granule or the page's guard granule |

- [Z2-N-CONTRACT-251] Arenas and pools under HWASan follow rules H0 to H12 of the table "HWASan tagging rules".
- [Z2-N-CONTRACT-259] The allocator satisfies H4 and H8 from its own records and never reads the shadow (the public HWASan interface offers no tag read): an arena keeps the tag of its last allocation and the free tag of its current block, and a pool keeps one tag byte per slot and the free tag of each page.

## Required intentional-defect tests

<!-- informative -->
Each defect test performs a deliberate memory error. A passing test is one where the sanitizer reports the error; the process then aborts, so each test runs as its own CTest entry (see `Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"`). The positive allocation test runs in every configuration.
<!-- /informative -->

| Test ID | doctest name | Defect or check | Presets | Expected output |
|---|---|---|---|---|
| `Test_Arena_OverflowPastBlock_IsCaught` | `ZenithCore/Arena/OverflowPastBlock_IsCaught` | write one byte past the end of the last allocation that fills a block, where "fills" means its A3 redzone (ASan) or its last granule (HWASan) reaches the block's usable end; under ASan that byte is the arena-poisoned redzone, under HWASan the block's guard granule (H11) | `win-asan`, S24 HWASan | `ERROR: AddressSanitizer: use-after-poison` or `ERROR: HWAddressSanitizer: tag-mismatch` |
| `Test_Arena_OverflowPastAllocation_IsCaught` | `ZenithCore/Arena/OverflowPastAllocation_IsCaught` | write byte `n` of an `n`-byte allocation followed by another allocation | `win-asan`, S24 HWASan | as above |
| `Test_Arena_UseAfterReset_IsCaught` | `ZenithCore/Arena/UseAfterReset_IsCaught` | read an allocation after `Reset()` | `win-asan`, S24 HWASan | as above |
| `Test_Arena_UseAfterResetToMarker_IsCaught` | `ZenithCore/Arena/UseAfterResetToMarker_IsCaught` | read an allocation made after a marker, after `ResetToMarker()` | `win-asan`, S24 HWASan | as above |
| `Test_Pool_UseAfterFree_IsCaught` | `ZenithCore/Pool/UseAfterFree_IsCaught` | read a slot after `Free` | `win-asan`, S24 HWASan | as above |
| `Test_Pool_FillFreeRefill_AtCap_Succeeds` | `ZenithCore/Pool/FillFreeRefill_AtCap_Succeeds` | allocate `m_uMaxSlots` slots, free all, allocate `m_uMaxSlots` again; every allocation succeeds and the next fails with `ALLOC_ERROR_CAPACITY_EXCEEDED` | every preset | test passes |
| `Test_Pool_Alloc1_15_16_17Bytes` | `ZenithCore/Pool/Alloc1_15_16_17Bytes` | for slot sizes 1, 15, 16 and 17: allocate two adjacent slots, write and read every byte `[0, n)` of each, assert alignment and non-overlap | every preset | no sanitizer report; test passes |
| `Test_Pool_OverflowByOne_1Bytes_IsCaught` | `ZenithCore/Pool/OverflowByOne_1Bytes_IsCaught` | write byte 1 of a 1-byte slot (short granule, `r = 1`) | `win-asan`, S24 HWASan | as above |
| `Test_Pool_OverflowByOne_15Bytes_IsCaught` | `ZenithCore/Pool/OverflowByOne_15Bytes_IsCaught` | write byte 15 of a 15-byte slot (short granule, `r = 15`) | `win-asan`, S24 HWASan | as above |
| `Test_Pool_OverflowByOne_16Bytes_IsCaught` | `ZenithCore/Pool/OverflowByOne_16Bytes_IsCaught` | write byte 16 of a 16-byte slot (the next granule), including when the slot is the page's last, where that granule is the page's guard granule (H11) | `win-asan`, S24 HWASan | as above |
| `Test_Pool_OverflowByOne_17Bytes_IsCaught` | `ZenithCore/Pool/OverflowByOne_17Bytes_IsCaught` | write byte 17 of a 17-byte slot (short granule, `r = 1`) | `win-asan`, S24 HWASan | as above |
| `Test_Heap_UseAfterDelete_IsCaught` | `ZenithCore/Heap/UseAfterDelete_IsCaught` | read through a pointer kept past `Zenith_Owned::Reset()` | `win-asan`, S24 HWASan | `heap-use-after-free` or `tag-mismatch` |
| `Test_HwasanTag_SizeRoundingOverflow_IsRejected` | `ZenithCore/HwasanTag/SizeRoundingOverflow_IsRejected` | an arena's `TryAllocate(UINT64_MAX − 7, 16)` returns `ALLOC_ERROR_SIZE_OVERFLOW` (granule rounding under HWASan, block-size arithmetic elsewhere) and leaves the arena unchanged | every preset | test passes |
| `Test_HwasanTag_RetagOnFree_ChangesTag` | `ZenithCore/HwasanTag/RetagOnFree_ChangesTag` | the pointer tag after free-and-reallocate of one slot differs from the first tag, and every tag is within 16 … 255 | S24 HWASan | test passes |
| `Test_Heap_LeakTable_ReportsLeak` | `ZenithCore/Heap/LeakTable_ReportsLeak` | a heap instance (not the root heap) reports one deliberately leaked allocation with its size and a symbolised site | Debug builds | test passes |

- [Z2-N-CONTRACT-252] The tests in the table "Required intentional-defect tests" are required, live in `Zenith2/Engine/ZenithCore/Tests/Zenith_Arena_DefectTests.cpp`, `Zenith_Pool_DefectTests.cpp`, `Zenith_Heap_DefectTests.cpp` (the defect rows) and `Zenith_Pool_Tests.cpp`, `Zenith_Arena_Tests.cpp`, `Zenith_Heap_Tests.cpp` (the rows that pass normally), and each passes in every preset listed for it.
- [Z2-N-CONTRACT-253] A change to ZenithCore's memory implementation runs the defect tests in `win-asan` through the change-triggered `Sanitizers` gate step of the `Zenith2Memory` category; the HWASan rows run on the S24 in each stage acceptance.
