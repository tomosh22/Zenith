# CORE design: containers

This file is part of the CORE design document family (entry: `Zenith2/Docs/Design/CORE.md`). It fixes the container set of ZenithCore, the justification record of each custom container (`Zenith2/Docs/Contracts.md` → "Containers and standard library policy"), every container's full API with its contract block, the algorithms behind them, and the oracle, property, failure and overflow tests each one carries.

## Scope of this file

Contracts already fix the container rules: contract fields ([Z2-N-CONTRACT-281] to [Z2-N-CONTRACT-284]), two forms of every allocating operation ([Z2-N-CONTRACT-285]), the allocation tier ([Z2-N-CONTRACT-286]), merge-point capacity for simulation-facing shared containers ([Z2-N-CONTRACT-287]), and the oracle, property, failure and overflow tests ([Z2-N-CONTRACT-288] to [Z2-N-CONTRACT-291]). The bounded-queue policy enumeration and descriptor are `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" (subsection "Bounded queues"). This file adds the set, the APIs, the algorithms and the records.

## Container set

<!-- elaboration -->
| Container | Header | Purpose | Storage tier | Thread affinity | Justification record |
|---|---|---|---|---|---|
| `Zenith_Span<T>`, `Zenith_ByteSpan`, `Zenith_MutableByteSpan` | `Zenith_Span.h` | non-owning contiguous view; every API that takes or returns a run of elements (a trivially copyable `value` view: it owns and allocates nothing, so its contract block is kind `value` and carries none of the four container fields) | none (views only) | `IMMUTABLE` value | JR-CORE-SPAN |
| `Zenith_Array<T>` | `Zenith_Array.h` + `.inl` | growable contiguous array | the root heap, or a caller-supplied `Zenith_Arena&` | `OWNER_THREAD` | JR-CORE-ARRAY |
| `Zenith_FixedArray<T, uSIZE>` | `Zenith_FixedArray.h` | exactly `uSIZE` elements inline, the layout of a C array `T[uSIZE]`; reflected fixed arrays and Luau `Zenith.FixedArray<T>` views | inline | `OWNER_THREAD` | JR-CORE-FIXEDARRAY |
| `Zenith_InlineArray<T, uCAPACITY>` | `Zenith_InlineArray.h` | 0 … `uCAPACITY` elements inline, never allocating | inline | `OWNER_THREAD` | JR-CORE-INLINEARRAY |
| `Zenith_HashMap<K, V>`, `Zenith_HashSet<K>` | `Zenith_HashMap.h` + `.inl` | associative lookup with deterministic iteration | root heap or arena | `OWNER_THREAD` | JR-CORE-HASHMAP |
| `Zenith_RingBuffer<T>` | `Zenith_RingBuffer.h` + `.inl` | single-thread bounded FIFO with a declared full policy | root heap or arena | `OWNER_THREAD` | JR-CORE-RINGBUFFER |
| `Zenith_BoundedQueue<T>` | `Zenith_BoundedQueue.h` + `.inl` | multi-producer multi-consumer bounded queue for dedicated threads (I/O, decode, upload staging, log drain) | root heap | `ANY_THREAD` | JR-CORE-BOUNDEDQUEUE |
| `Zenith_SpscRing<T>` | `Zenith_SpscRing.h` | wait-free single-producer single-consumer ring over caller storage, the programme's one SPSC primitive (ruling Q80: AUDIO's real-time channels, DEVT's profiler and debug-draw rings) | none (caller storage) | one producer and one consumer at a time | JR-CORE-SPSCRING |
| `Zenith_TripleBuffer<T>` | `Zenith_TripleBuffer.h` | wait-free latest-value hand-over over three caller slots (AUDIO's snapshot, meter and state channels) | none (caller storage) | one producer and one consumer at a time | JR-CORE-TRIPLEBUFFER |
| `Zenith_BitArray` | `Zenith_BitArray.h` | a fixed-capacity bit set, sized once at initialisation | root heap or arena | `OWNER_THREAD` | JR-CORE-BITARRAY |
| `Zenith_SlotTable<T>` | `Zenith_SlotTable.h` | generational slot store behind CORE-owned handle kinds | root heap | `OWNER_THREAD` | JR-CORE-SLOTTABLE (`Zenith2/Docs/Design/CORE/Memory.md` → "Zenith_SlotTable") |
| `Zenith_Function<TSignature>` | `Zenith_Function.h` | arena-stored type-erased callable | caller's arena | `IMMUTABLE` value | JR-CORE-FUNCTION (`Zenith2/Docs/Design/CORE/Memory.md` → "Zenith_Function") |
| `Zenith_String`, `Zenith_StringView` | `Zenith_String.h`, `Zenith_StringView.h` | owning and non-owning UTF-8 text | root heap or arena / none | `OWNER_THREAD` / `IMMUTABLE` | JR-CORE-STRING (`Zenith2/Docs/Design/CORE/Text.md` → "Zenith_String") |
| `Zenith_Sort`, `Zenith_StableSort`, `Zenith_LowerBound`, `Zenith_UpperBound` | `Zenith_Sort.h` | deterministic sorting and searching over spans | stable sort: caller's arena | pure functions | JR-CORE-SORT |

Deliberately not in the set, each because no consumer needs it and an unjustified container is custom infrastructure without a record: a linked list, a deque, a small-buffer-optimised array, a sorted map, a priority queue (the scheduler's ready list is JOBS's), a concurrent pool ([Z2-N-CONTRACT-237] names it a separate container with its own record), and a concurrent hash map. A ticket that needs one adds it with its record (Z2-D-041).

The element types of every container are move-constructible and destructible (`static_assert(std::is_move_constructible_v<T> && std::is_destructible_v<T>)`). A move during growth cannot fail, because exceptions are disabled (Z2-D-045), so no `noexcept` is asked of the element type; a `nothrow` trait would reject every engine type, since no engine move constructor is declared `noexcept`.

## Justification records

<!-- elaboration -->
Each record below is appended to `Zenith2/Docs/DecisionLog.md` under "Decisions ruled after the seed", with the next free `Z2-D-NNN` taken at the time of the diff, by the ticket that adds the container's first line of code (`Zenith2/Docs/Contracts.md` → "Containers and standard library policy", [Z2-N-CONTRACT-278]; Z2-D-097). The label `JR-CORE-<NAME>` is this document's name for the record until it has its ID; the landing ticket replaces the label here with the ID in the same diff. A record whose measured evidence needs the reference hardware lands provisionally — `Status: Provisional`, `Measured evidence: pending — <harness> run recorded by <ruling>` and `Decision: adopt provisionally` — and the `needs-human` measurement ruling completes the same entry in place, rewriting its Status to `Settled` whether it adopts or rejects (the Q37 (a) shape; Contracts amendments X92 and X132). JR-CORE-FIXEDARRAY needs no measurement and lands `Status: Settled` with `Decision: adopt`; JR-CORE-CHARCONV and JR-CORE-COMPILERSUPPORT are ruled by a person when appended and land `Status: Settled`, the `<charconv>` record with only its throughput line pending. The benchmarks named are cases of the harness `Zenith2/Benchmarks/CoreContainers/` (`Zenith_Bench_CoreContainers`, built with `zenith_add_benchmark`, its `--smoke` run registered in CTest), measured at least 10 repetitions with medians and 95% confidence intervals on the reference desktop and the S24 (`Zenith2/Docs/Architecture.md` → "Named selection decisions", [Z2-N-ARCH-983]), with raw data under `Zenith2/Docs/Research/CoreContainers/`.

Standing facts every record inherits and does not re-argue: an STL type never appears in a runtime public header ([Z2-N-CONTRACT-272]), so an STL alternative could serve only `Internal/` code; exceptions are forbidden outside the Luau firewall (Z2-D-045), so an alternative whose failure path is an exception fails the allocation-failure contract; STL smart pointers are forbidden (Z2-D-040).

| Record | Kind | Standard alternative | Improvement claimed | Measured evidence (harness case) | Maintenance cost | Acceptance criteria | Contract header | Decision |
|---|---|---|---|---|---|---|---|---|
| JR-CORE-SPAN | container | `std::span` | usable in public headers; `uint64` count; bounds `ZENITH_ASSERT`; no header outside the allowed list | `Span_IndexedSum` against a raw pointer loop | 150 lines, 1 test file, no platform surface; owner CORE | no measurable difference from the raw loop in `win-release` (median within 2%) | `Zenith_Span.h` | adopt |
| JR-CORE-ARRAY | container | `std::vector` | root-heap or arena tier; `Try` forms with the strong guarantee; no exceptions; public-header safe | `Array_PushBack_1M_u32`, `Array_PushBack_1M_String`, `Array_RandomErase` against `std::vector` | 600 lines, oracle/property/failure tests; owner CORE | median push and iteration throughput within 10% of `std::vector` on both platforms | `Zenith_Array.h` | adopt |
| JR-CORE-FIXEDARRAY | container | `std::array`, C arrays | public-header safe; bounds `ZENITH_ASSERT`; the reflected fixed-array layout | none needed: identical layout; compile-time `static_assert(sizeof == N × sizeof(T))` | 120 lines; owner CORE | layout assertion holds on every preset | `Zenith_FixedArray.h` | adopt |
| JR-CORE-INLINEARRAY | container | `std::inplace_vector` (C++26, not available in C++20) | bounded variable size with no allocation, for hot paths and fixed-capacity bookkeeping | `InlineArray_PushPop` against `Zenith_Array` with a reserved capacity | 250 lines; owner CORE | no allocation in any operation (heap statistics unchanged) | `Zenith_InlineArray.h` | adopt |
| JR-CORE-HASHMAP | container | `std::unordered_map`, `std::unordered_set` | open addressing (no per-node allocation); iteration order a function of the operation sequence alone, never of hash values or capacity ([Z2-N-ARCH-884]); `Try` forms; hash per D-SEL-HASH (open, Stage 1) | `HashMap_InsertFind_u64_1M`, `HashMap_FindMiss_u64_1M`, `HashMap_StringKey_100k`, `HashMap_EraseChurn` against `std::unordered_map` | 900 lines, the largest CORE container; owner CORE | median find throughput at least 1.5× `std::unordered_map` for `uint64` keys on both platforms; memory per entry at most `std::unordered_map`'s | `Zenith_HashMap.h` | adopt |
| JR-CORE-RINGBUFFER | container | `std::deque` used as a queue | bounded with a declared full policy and the queue counters ([Z2-N-CONTRACT-342] to [Z2-N-CONTRACT-345]); contiguous storage | `RingBuffer_PushPop_1M` against `std::deque` | 300 lines; owner CORE | median throughput at least `std::deque`'s | `Zenith_RingBuffer.h` | adopt |
| JR-CORE-BOUNDEDQUEUE | threading primitive | a mutex plus `std::deque`, or no standard facility (C++20 has no concurrent queue) | lock-free bounded MPMC (sequence-numbered cells); the full policies; job-worker blocking checks ([Z2-N-CONTRACT-315]) | `BoundedQueue_MPMC_4x4` and `_8x8` against a `Zenith_Mutex` + `Zenith_RingBuffer` queue | 400 lines, `linux-tsan` stress; owner CORE | median throughput at least 2× the locked queue at 8 producers and 8 consumers on the reference desktop; TSan clean | `Zenith_BoundedQueue.h` | adopt |
| JR-CORE-SPSCRING | threading primitive | CORE's `Zenith_BoundedQueue<T>` (MPMC, a CAS per push, 64-byte cells, a profiler hook on blocking waits, no caller storage) | wait-free with no CAS and no per-cell padding; caller storage, so no allocation ever; calls no hook, log or system call, so usable on the audio device thread and inside profiler hooks; whole-span push and pop in at most two runs | `SpscRing_PushPop_16B`, `SpscRing_PushPop_192B` against `Zenith_BoundedQueue` at one producer and one consumer | 250 lines, header-only, `linux-tsan` stress; owner CORE | median push plus pop at most 50% of the bounded queue's on the reference desktop and the S24; memory per element `sizeof(T)`; `linux-tsan` clean | `Zenith_SpscRing.h` | adopt provisionally |
| JR-CORE-TRIPLEBUFFER | threading primitive | a `Zenith_Mutex`-protected double buffer (forbidden on the audio device thread) or `Zenith_BoundedQueue` (no DROP_OLDEST) | wait-free latest-value hand-over with no copy at publish or acquire | `TripleBuffer_PublishAcquire_53KB` | 120 lines, header-only, `linux-tsan` stress; owner CORE | median `AcquireLatest` + `GetFront` at most 20 ns on the S24; `linux-tsan` clean | `Zenith_TripleBuffer.h` | adopt provisionally |
| JR-CORE-BITARRAY | container | `std::vector<bool>`, `std::bitset` | fixed capacity sized at run time; word-level find-first and population count; tier choice | `BitArray_FindFirstSet_1M` against a `std::vector<bool>` scan | 250 lines; owner CORE | median find-first at least 8× the `std::vector<bool>` scan | `Zenith_BitArray.h` | adopt |
| JR-CORE-SLOTTABLE | container | none in the STL | generational slots, FIFO reuse, stable addresses, retirement at generation exhaustion | `SlotTable_AllocResolveFree_1M` | 350 lines; owner CORE | resolve median below 5 ns on the reference desktop | `Zenith_SlotTable.h` | adopt |
| JR-CORE-FUNCTION | container | `std::function` | no heap allocation; trivially destructible arena storage for graph captures ([Z2-N-CONTRACT-232]) | `Function_CreateInvoke_1M` against `std::function` | 150 lines; owner CORE | zero heap allocations per create or invoke | `Zenith_Function.h` | adopt |
| JR-CORE-STRING | container | `std::string`, `std::string_view` | public-header safe; UTF-8 validity invariant; tier choice; `Try` forms | `String_AppendFormat_100k` against `std::string` + `std::to_chars` | 400 lines with the view; owner CORE | median append within 15% of `std::string` | `Zenith_String.h` | adopt |
| JR-CORE-SORT | container (algorithm companion) | `std::sort`, `std::stable_sort` (`<algorithm>` is outside the allowed headers) | usable from public-header templates; deterministic for identical input on every platform; stable sort from caller scratch | `Sort_1M_u64`, `StableSort_1M_Pair` against `std::sort`/`std::stable_sort` | 300 lines; owner CORE | median within 20% of the standard algorithms | `Zenith_Sort.h` | adopt |
| JR-CORE-CHARCONV | standard facility | `<charconv>` (`std::to_chars`) in `ZenithCore/Internal/Text/` only | the shortest round-trip float formatting and exact integer conversion, locale-independent and deterministic; a custom Ryu would duplicate it | output identity asserted by `Test_Format_DoubleShortest_RoundTrips` on every preset (`Format_DoubleShortest_1M` throughput is reported for information only) | none beyond the allowlist entry; owner CORE | output bytes identical on Windows and Android for the fixture set of `Test_Format_DoubleShortest_RoundTrips` | `Zenith_Format.h` | the standard facility wins; `<charconv>` is listed in `Zenith2/Tools/Gates/forbidden_allowlist.json` for `Zenith2/Engine/ZenithCore/Internal/Text/` with the record's ID ([Z2-N-CONTRACT-273]) |

## Zenith_Span

`Zenith2/Docs/Contracts.md` → "Ownership and allocation" (table "Core memory headers") fixes `Zenith_Span<T>` as a trivially copyable `{T* m_pxData; uint64 m_ulCount;}` with a bounds-checked `operator[]`, `GetData()`, `GetCount()` and `Subspan()`. The full declaration:

```cpp
// Zenith2/Engine/ZenithCore/Zenith_Span.h
/// @contract value Zenith_Span
/// Ownership: Non-owning view; the viewed storage's owner keeps it alive for the span's use
///   (Zenith2/Docs/Contracts/Memory.md "Non-owning references"). Never allocates.
/// Thread affinity: IMMUTABLE. Access to the elements follows the viewed storage's rules. Members non-const: a span is
///   default-constructed and reassigned (xSpan = xSpan.Subspan(…), span members of descs and records).
/// Invalidation: Invalid whenever the viewed storage is freed, reallocated or reset; see each container's
///   Invalidation field for which operations do that.
/// Units: Counts in elements; AsBytes counts bytes.
/// Errors: operator[], First, Last and Subspan out of range are ZENITH_ASSERT; a null pointer with a non-zero
///   count is ZENITH_ASSERT at construction.
/// Version: since Z2 Stage 1. Source API.
/// @endcontract
template<typename T>
struct Zenith_Span
{
	T* m_pxData = nullptr;
	uint64 m_ulCount = 0;

	constexpr Zenith_Span() = default;
	constexpr Zenith_Span(T* pxData, uint64 ulCount);
	template<uint64 ulN> constexpr Zenith_Span(T (&axArray)[ulN]);
	constexpr operator Zenith_Span<const T>() const requires (!std::is_const_v<T>);

	constexpr T* GetData() const { return m_pxData; }
	constexpr uint64 GetCount() const { return m_ulCount; }
	constexpr uint64 GetByteCount() const { return m_ulCount * sizeof(T); }
	constexpr bool IsEmpty() const { return m_ulCount == 0; }
	constexpr T& operator[](uint64 ulIndex) const;               // ZENITH_ASSERT ulIndex < m_ulCount
	constexpr Zenith_Span Subspan(uint64 ulOffset, uint64 ulCount) const;  // ZENITH_ASSERT in range
	constexpr Zenith_Span Subspan(uint64 ulOffset) const;         // to the end
	constexpr Zenith_Span First(uint64 ulCount) const;
	constexpr Zenith_Span Last(uint64 ulCount) const;
	constexpr T* begin() const { return m_pxData; }
	constexpr T* end() const { return m_pxData + m_ulCount; }
};

using Zenith_ByteSpan = Zenith_Span<const uint8>;
using Zenith_MutableByteSpan = Zenith_Span<uint8>;

template<typename T> Zenith_ByteSpan Zenith_AsBytes(Zenith_Span<const T> xSpan);      // requires trivially copyable T
template<typename T> Zenith_MutableByteSpan Zenith_AsWritableBytes(Zenith_Span<T> xSpan);
template<typename T> Zenith_ByteSpan Zenith_AsBytesOf(const T& xValue);               // requires trivially copyable T
static_assert(sizeof(Zenith_Span<int>) == 16);
```

- `Subspan(ulOffset, ulCount)` checks `ulOffset ≤ m_ulCount && ulCount ≤ m_ulCount − ulOffset` (the subtraction form, which cannot overflow).
- `Zenith_AsBytes` is used only for data whose byte image is its format by design (hash inputs, GPU uploads, canonical serializations); it is never a serialization of a C++ object layout (Z2-D-038).

## Zenith_Array

```cpp
// Zenith2/Engine/ZenithCore/Zenith_Array.h
constexpr uint64 ulARRAY_MAX_BYTES = 1ull << 47;       // 128 TiB: no platform can satisfy more

/// @contract container Zenith_Array
/// Ownership: Owns its elements. Storage comes from the root heap, or from the Zenith_Arena passed to the
///   arena constructor, which outlives the array. Move-only.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Every operation that can grow the storage (PushBack, EmplaceBack, Insert, Reserve, Resize and
///   their Try forms, TryCopyFrom) invalidates every pointer, reference, span and iterator into the array when
///   it reallocates, which it does when the new count exceeds GetCapacity(); Insert and RemoveAt also
///   invalidate those at and after the index; RemoveAtSwap invalidates those at the index and the last element;
///   PopBack those of the last element; Clear all of them; Reserve below the capacity invalidates nothing.
/// Units: Counts in elements.
/// Errors: Try forms return Zenith_Result<…, Zenith_AllocError>; out-of-range indices are ZENITH_ASSERT;
///   PopBack or Back on an empty array is ZENITH_ASSERT.
/// Version: since Z2 Stage 1. Source API.
/// Alignment: elements at alignof(T), which is at most ulHEAP_MAX_ALIGNMENT (static_assert).
/// Overflow: at most min(ulARRAY_MAX_BYTES / sizeof(T), 2^48) elements; every size computation is
///   overflow-checked and fails with ALLOC_ERROR_SIZE_OVERFLOW from Try forms and ZENITH_CHECK otherwise.
/// Allocation failure: Try forms leave the array unchanged (strong guarantee); other forms route failure
///   through the root heap's OOM handler (heap tier) or terminate with FATAL_CODE_ARENA_EXHAUSTED (arena tier).
/// Complexity: indexed access O(1); PushBack and EmplaceBack amortised O(1); Insert and RemoveAt O(n − index);
///   RemoveAtSwap and PopBack O(1); Reserve and Resize O(n); Clear O(n) destructor calls.
/// Example: Zenith2/Engine/ZenithCore/Tests/Examples/Zenith_Array_Example.cpp
/// @endcontract
template<typename T>
class Zenith_Array
{
public:
	constexpr Zenith_Array() = default;                                 // root heap; allocates nothing
	explicit Zenith_Array(Zenith_Arena& xArena);                         // arena tier; allocates nothing
	Zenith_Array(const Zenith_Array&) = delete;
	Zenith_Array& operator=(const Zenith_Array&) = delete;
	Zenith_Array(Zenith_Array&& xOther);                                 // xOther becomes empty, same tier
	Zenith_Array& operator=(Zenith_Array&& xOther);                      // destroys and frees the current contents first
	~Zenith_Array();

	Zenith_Result<void, Zenith_AllocError> TryReserve(uint64 ulCapacity);
	void Reserve(uint64 ulCapacity);
	Zenith_Result<void, Zenith_AllocError> TryResize(uint64 ulCount);   // new elements value-initialised
	void Resize(uint64 ulCount);
	Zenith_Result<T*, Zenith_AllocError> TryPushBack(const T& xValue) requires std::is_copy_constructible_v<T>;
	Zenith_Result<T*, Zenith_AllocError> TryPushBack(T&& xValue);
	T& PushBack(const T& xValue) requires std::is_copy_constructible_v<T>;
	T& PushBack(T&& xValue);
	template<typename... TArgs> Zenith_Result<T*, Zenith_AllocError> TryEmplaceBack(TArgs&&... xArgs);
	template<typename... TArgs> T& EmplaceBack(TArgs&&... xArgs);
	Zenith_Result<T*, Zenith_AllocError> TryInsert(uint64 ulIndex, T&& xValue);   // ZENITH_ASSERT ulIndex ≤ count
	T& Insert(uint64 ulIndex, T&& xValue);
	Zenith_Result<void, Zenith_AllocError> TryCopyFrom(Zenith_Span<const T> xSource) requires std::is_copy_constructible_v<T>;
	void PopBack();
	void RemoveAt(uint64 ulIndex);                                      // preserves order
	void RemoveAtSwap(uint64 ulIndex);                                  // moves the last element into ulIndex
	void Clear();                                                        // keeps the capacity
	void Release();                                                      // Clear, then frees the storage (heap tier)

	uint64 GetCount() const;
	uint64 GetCapacity() const;
	bool IsEmpty() const;
	T* GetData();
	const T* GetData() const;
	T& operator[](uint64 ulIndex);
	const T& operator[](uint64 ulIndex) const;
	T& Front();
	T& Back();
	Zenith_Span<T> AsSpan();
	Zenith_Span<const T> AsSpan() const;
	T* begin();
	T* end();
	const T* begin() const;
	const T* end() const;
	Zenith_Arena* GetArena() const;                        // nullptr for the heap tier
private:
	T* m_pxData = nullptr;
	uint64 m_ulCount = 0;
	uint64 m_ulCapacity = 0;
	Zenith_Arena* m_pxArena = nullptr;
};
static_assert(sizeof(Zenith_Array<int>) == 32);
```

Growth algorithm (`Grow(ulRequired)`, shared by every growing operation):

1. If `ulRequired ≤ m_ulCapacity`, return success.
2. `ulNew = max(ulRequired, m_ulCapacity + m_ulCapacity / 2, max(4, 64 / sizeof(T)))` computed with overflow checks; clamp is never applied silently: if `ulNew > max count`, use `ulRequired`; if `ulRequired > max count`, fail `ALLOC_ERROR_SIZE_OVERFLOW`.
3. Allocate `ulNew × sizeof(T)` bytes at `alignof(T)` from the tier with its `TryAllocate`.
4. Move-construct each element into the new storage in index order, destroy each old element in index order, free the old block (heap tier) or abandon it (arena tier: the arena reclaims it at its next reset, and the arena's used bytes include it).
5. Only after step 3 succeeded does any state change, which gives the strong guarantee.

- `Insert` at `ulIndex` grows first, then move-constructs the last element one place up and move-assigns the rest downward from the back, then assigns the new value; `RemoveAt` move-assigns upward and destroys the last.
- An arena-tier array never frees; `Release` on an arena-tier array only destroys the elements and forgets the block.

<!-- informative -->
The 1.5× growth factor reuses freed heap blocks better than doubling for arrays that grow in place, and in arena tier it bounds the abandoned bytes to twice the final storage.
<!-- /informative -->

## Zenith_FixedArray

```cpp
// Zenith2/Engine/ZenithCore/Zenith_FixedArray.h
/// @contract container Zenith_FixedArray
/// Ownership: Owns exactly uSIZE elements stored inline; the layout is that of the C array T[uSIZE].
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None; the elements live as long as the object and never move.
/// Units: Counts in elements.
/// Errors: operator[] out of range is ZENITH_ASSERT; nothing else can fail.
/// Version: since Z2 Stage 1. Source API.
/// Alignment: alignof(Zenith_FixedArray<T, uSIZE>) == alignof(T); element i at offset i × sizeof(T).
/// Overflow: uSIZE is at least 1 and uSIZE × sizeof(T) fits in uint32 (static_assert).
/// Allocation failure: Never allocates.
/// Complexity: every access O(1); Fill O(uSIZE).
/// @endcontract
template<typename T, uint32 uSIZE>
struct Zenith_FixedArray
{
	T m_axElements[uSIZE];

	static constexpr uint32 GetCount() { return uSIZE; }
	constexpr T& operator[](uint32 uIndex);               // ZENITH_ASSERT uIndex < uSIZE
	constexpr const T& operator[](uint32 uIndex) const;
	constexpr T* GetData() { return m_axElements; }
	constexpr const T* GetData() const { return m_axElements; }
	constexpr Zenith_Span<T> AsSpan();
	constexpr Zenith_Span<const T> AsSpan() const;
	constexpr void Fill(const T& xValue);
	constexpr T* begin();
	constexpr T* end();
	constexpr const T* begin() const;
	constexpr const T* end() const;
	friend constexpr bool operator==(const Zenith_FixedArray&, const Zenith_FixedArray&) = default;
};
static_assert(sizeof(Zenith_FixedArray<float, 3>) == 12);
```

- `Zenith_FixedArray` is an aggregate, so it is trivially copyable, trivially destructible and standard-layout exactly when `T` is; a reflected fixed-array property (`PROPERTY_TYPE_FIXED_ARRAY`, `Zenith2/Docs/Architecture.md` → "Reflection and runtime type registration") of a C++-registered component is declared as `Zenith_FixedArray<T, N>`, and its layout matches the data-defined `array<T, N>` layout of `Zenith2/Docs/Architecture/DataDefinedComponents.md` → "The schema file" (N × size(T), alignment of T).
- The Luau view `Zenith.FixedArray<T>` (`Zenith2/Docs/Scripting/Bindings.md` → "Value marshalling") indexes a `Zenith_FixedArray<T, N>` field 1-based with `#a == N`; the view's bounds checks are the binding's controlled Luau errors, never this type's asserts.

## Zenith_InlineArray

```cpp
// Zenith2/Engine/ZenithCore/Zenith_InlineArray.h
/// @contract container Zenith_InlineArray
/// Ownership: Owns 0 … uCAPACITY elements stored inline in raw storage.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: PopBack and RemoveAt invalidate pointers to the removed element and, for RemoveAt, those
///   after it; RemoveAtSwap those at the index and the last element; Clear all; nothing ever relocates otherwise.
/// Units: Counts in elements.
/// Errors: TryPushBack and TryEmplaceBack return ALLOC_ERROR_CAPACITY_EXCEEDED when full;
///   PushBack and EmplaceBack when full are ZENITH_CHECK (there is no allocation to hand to an OOM handler);
///   out-of-range indices are ZENITH_ASSERT.
/// Version: since Z2 Stage 1. Source API.
/// Alignment: elements at alignof(T).
/// Overflow: uCAPACITY at least 1 and at most 65,535, and uCAPACITY × sizeof(T) at most 64 KiB (static_assert).
/// Allocation failure: Never allocates; capacity exhaustion is the only failure.
/// Complexity: push, pop, index O(1); RemoveAt O(count − index); Clear O(count).
/// @endcontract
template<typename T, uint32 uCAPACITY>
class Zenith_InlineArray
{
public:
	constexpr Zenith_InlineArray() = default;
	Zenith_InlineArray(const Zenith_InlineArray& xOther) requires std::is_copy_constructible_v<T>;
	Zenith_InlineArray& operator=(const Zenith_InlineArray& xOther) requires std::is_copy_constructible_v<T>;
	Zenith_InlineArray(Zenith_InlineArray&& xOther);
	Zenith_InlineArray& operator=(Zenith_InlineArray&& xOther);
	~Zenith_InlineArray();
	Zenith_Result<T*, Zenith_AllocError> TryPushBack(T&& xValue);
	T& PushBack(T&& xValue);
	template<typename... TArgs> Zenith_Result<T*, Zenith_AllocError> TryEmplaceBack(TArgs&&... xArgs);
	template<typename... TArgs> T& EmplaceBack(TArgs&&... xArgs);
	void PopBack();
	void RemoveAt(uint32 uIndex);
	void RemoveAtSwap(uint32 uIndex);
	void Clear();
	uint32 GetCount() const;
	static constexpr uint32 GetCapacity() { return uCAPACITY; }
	bool IsFull() const;
	T& operator[](uint32 uIndex);
	const T& operator[](uint32 uIndex) const;
	Zenith_Span<T> AsSpan();
	Zenith_Span<const T> AsSpan() const;
private:
	alignas(T) uint8 m_auStorage[sizeof(T) * uCAPACITY];
	uint32 m_uCount = 0;
};
```

- `PushBack` on a full array is a deviation from [Z2-N-CONTRACT-285]'s OOM routing, carried as `/// @deviation Allocation failure:` on the declaration, because an inline container has nothing to release; the deviation is part of JR-CORE-INLINEARRAY.

## Zenith_HashMap and Zenith_HashSet

```cpp
// Zenith2/Engine/ZenithCore/Zenith_HashMap.h
/// @contract functions Zenith_HashTraits
/// Ownership: Stateless trait specialisations.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. A change of Hash output changes no serialized data, because hash values are
///   never persisted or iterated by.
/// @endcontract
template<typename K> struct Zenith_HashTraits;
// Every specialisation declares, all static and:
//   uint64 Hash(const K& xKey);
//   bool Equal(const K& xA, const K& xB);
// and, for each heterogeneous lookup type L it admits (Zenith_String admits Zenith_StringView):
//   uint64 Hash(const L& xLookup);            // equal to Hash(K) for every K that Equal(K, L) accepts
//   bool Equal(const K& xKey, const L& xLookup);
// Find, Contains, Get and Remove accept any TLookup the traits admit; any other TLookup fails to compile.
// Specialisations in this header: every integral type, every enum, Zenith_StringView (by content),
// Zenith_String (by content, heterogeneous with Zenith_StringView), Zenith_StringID (by value),
// Zenith_PersistentID (its 16 bytes). ZenithReflect specialises it for Zenith_TypeID and Zenith_PropertyID
// in Zenith_ReflectIDs.h; any library may specialise it for its own key types.

/// @contract container Zenith_HashMap
/// Ownership: Owns keys and values. Storage (a dense entry array and a slot index) comes from the root heap,
///   or from the Zenith_Arena passed to the arena constructor. Move-only.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Insert, Emplace, FindOrInsert and Reserve invalidate every pointer, reference and iterator
///   when they grow (count reaches 3/4 of the slot count, or the entry array reallocates); Remove invalidates
///   pointers to the removed entry and to the entry that was last in iteration order; Clear invalidates all.
/// Units: Counts in entries.
/// Errors: Try forms return Zenith_Result<…, Zenith_AllocError>; Get of an absent key is ZENITH_ASSERT;
///   Find returns nullptr for an absent key.
/// Version: since Z2 Stage 2. Source API. Iteration order is part of the contract (below).
/// Alignment: entries at alignof(Zenith_HashMapEntry<K, V>).
/// Overflow: at most 2^31 − 1 entries; size arithmetic overflow-checked (ALLOC_ERROR_SIZE_OVERFLOW / ZENITH_CHECK).
/// Allocation failure: Try forms leave the map unchanged; other forms use the OOM handler (heap tier) or
///   FATAL_CODE_ARENA_EXHAUSTED (arena tier).
/// Complexity: Find, Insert and Remove expected O(1) and worst O(n) with linear probing at load ≤ 3/4;
///   growth O(n); iteration O(n). Hash: Zenith_HashTraits<K>::Hash, which for built-in keys is Zenith_Hash64
///   of the key's little-endian bytes (D-SEL-HASH, open, Stage 1).
/// @endcontract
template<typename K, typename V>
struct Zenith_HashMapEntry
{
	K m_xKey;
	V m_xValue;
};

template<typename K, typename V, typename TTraits = Zenith_HashTraits<K>>
class Zenith_HashMap
{
public:
	constexpr Zenith_HashMap() = default;
	explicit Zenith_HashMap(Zenith_Arena& xArena);
	Zenith_HashMap(const Zenith_HashMap&) = delete;
	Zenith_HashMap& operator=(const Zenith_HashMap&) = delete;
	Zenith_HashMap(Zenith_HashMap&& xOther);
	Zenith_HashMap& operator=(Zenith_HashMap&& xOther);
	~Zenith_HashMap();

	Zenith_Result<void, Zenith_AllocError> TryReserve(uint32 uCount);
	void Reserve(uint32 uCount);
	Zenith_Result<V*, Zenith_AllocError> TryInsert(const K& xKey, V&& xValue);   // overwrites an existing key's value
	V& Insert(const K& xKey, V&& xValue);
	template<typename... TArgs> Zenith_Result<V*, Zenith_AllocError> TryEmplace(const K& xKey, TArgs&&... xArgs);
	Zenith_Result<Zenith_HashMapInsertion<V>, Zenith_AllocError> TryFindOrInsert(const K& xKey);   // value-initialised when new
	template<typename TLookup> V* Find(const TLookup& xKey);
	template<typename TLookup> const V* Find(const TLookup& xKey) const;
	template<typename TLookup> bool Contains(const TLookup& xKey) const;
	template<typename TLookup> V& Get(const TLookup& xKey);                   // ZENITH_ASSERT present
	template<typename TLookup> bool Remove(const TLookup& xKey);              // false when absent
	void Clear();
	uint32 GetCount() const;
	bool IsEmpty() const;
	Zenith_Span<Zenith_HashMapEntry<K, V>> GetEntries();                     // iteration order
	Zenith_Span<const Zenith_HashMapEntry<K, V>> GetEntries() const;
	Zenith_Result<void, Zenith_AllocError> TryGetKeyOrder(Zenith_Array<uint32>& axEntryIndices) const
		requires requires (const K& a, const K& b) { { a < b } -> std::convertible_to<bool>; };
	Zenith_HashMapEntry<K, V>* begin();
	Zenith_HashMapEntry<K, V>* end();
private:
	Zenith_Array<Zenith_HashMapEntry<K, V>> m_axEntries;   // dense, in iteration order
	uint64* m_pulSlots = nullptr;                          // bits 0–31: entry index + 1 (0 = empty); bits 32–63: hash high word
	uint32 m_uSlotMask = 0;                                // slot count − 1 (slot count a power of two ≥ 16), or 0 before the first insert
};

template<typename V>
struct Zenith_HashMapInsertion
{
	V* m_pxValue = nullptr;
	bool m_bInserted = false;
};

template<typename K, typename TTraits = Zenith_HashTraits<K>>
class Zenith_HashSet
{
public:
	// the Zenith_HashMap API with V absent: TryInsert(const K&) -> Zenith_Result<bool, Zenith_AllocError> (true when new),
	// Insert, Contains, Remove, Clear, GetCount, GetKeys() -> Zenith_Span<const K>, TryGetKeyOrder, begin, end.
private:
	Zenith_HashMap<K, Zenith_HashSetEmpty, TTraits> m_xMap;   // Zenith_HashSetEmpty is a one-byte empty struct
};
```

Algorithms:

1. **Hash split.** `uint64 h = TTraits::Hash(key)`; the probe start is `h & m_uSlotMask` (low bits); the stored fragment is `h >> 32`.
2. **Find.** From the start slot, probe linearly: an empty slot ends the search (absent); a slot whose fragment equals the key's and whose entry's key compares `Equal` is the hit.
3. **Insert.** Find; on a hit overwrite the value. Otherwise grow if `count + 1 > slots × 3 / 4` (or allocate 16 slots on first insert), append the entry to `m_axEntries` (growing it first, with the strong guarantee: both allocations are made before any state changes), and write `(index + 1) | (fragment << 32)` into the first empty slot of the probe.
4. **Grow.** Allocate the doubled slot array; re-insert every entry index by recomputing its hash, in entry order; free the old array. Entry order is unchanged.
5. **Remove.** Find the slot `s` of the key and its entry index `e`. Backward-shift delete: walk `j = s + 1 …` while slot `j` is occupied and its ideal position is not in the cyclic range `(s, j]`, move slot `j` into the hole and continue from `j`; clear the final hole. Then if `e` is not the last entry, move the last entry into `e`, find the slot that holds `last index + 1` (probe from its hash) and rewrite it to `e + 1`; pop the last entry.
6. **Iteration order.** The order of `GetEntries()` is the dense entry order: insertion order, with each removal moving the then-last entry into the removed position. It is a function of the sequence of Insert and Remove calls alone, never of hash values, slot count, capacity, addresses or the platform.
7. **Key order.** `TryGetKeyOrder` fills the entry indices sorted by `operator<` on keys with `Zenith_Sort`; simulation code that iterates a hashed container iterates in this order ([Z2-N-ARCH-884]), which review confirms.

<!-- informative -->
A dense entry array with an index table (the layout of CPython's compact dict and of several engine hash maps) gives cache-friendly iteration and makes the iteration order independent of hashing, which open addressing over entries in place cannot. Linear probing with backward-shift deletion is the algorithm the Debug leak table already uses, so CORE maintains one probing scheme.
<!-- /informative -->

- Hashing untrusted keys: the hash has no secret seed, so a map keyed by attacker-controlled strings could be driven to O(n²); the JSON reader therefore detects duplicate keys by sorting, not by hashing (`Zenith2/Docs/Design/CORE/Text.md` → "JSON reader"), and no parser of untrusted input uses a `Zenith_HashMap` keyed by input text without a bound on the key count taken from `Zenith2/Docs/Architecture.md` → "Security architecture" (table "Parser limits").

## Zenith_RingBuffer

```cpp
// Zenith2/Engine/ZenithCore/Zenith_QueuePolicy.h
// Zenith_QueueFullPolicy and Zenith_BoundedQueueDesc: exactly the declarations of Zenith2/Docs/Contracts.md
// "Threading and concurrency contract" (subsection "Bounded queues"), with contract blocks.

/// @contract enum Zenith_QueueError
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: QUEUE_ERROR_FULL (the queue is at its hard capacity under REJECT or GROW_THEN_REJECT, or its
///   element was discarded under DROP_NEWEST), QUEUE_ERROR_OUT_OF_MEMORY (a GROW policy failed to allocate).
/// Version: since Z2 Stage 1.
/// @endcontract
enum Zenith_QueueError : uint8
{
	QUEUE_ERROR_FULL = 1,
	QUEUE_ERROR_OUT_OF_MEMORY,
};

/// @contract value Zenith_QueueCounterNames
/// Ownership: Three string literals, made by ZENITH_QUEUE_COUNTER_NAMES("<queue name>").
/// Thread affinity: IMMUTABLE. Members non-const: Zenith_RingBuffer holds a copy and is move-assignable.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 1.
/// @endcontract
struct Zenith_QueueCounterNames
{
	const char* m_szFull;          // "<name>.full"
	const char* m_szDropped;       // "<name>.dropped"
	const char* m_szHighWater;     // "<name>.high_water"
};
#define ZENITH_QUEUE_COUNTER_NAMES(NameLiteral) \
	Zenith_QueueCounterNames{ NameLiteral ".full", NameLiteral ".dropped", NameLiteral ".high_water" }

struct Zenith_QueueStats
{
	uint64 m_ulFullEvents = 0;
	uint64 m_ulDropped = 0;
	uint32 m_uHighWater = 0;
};
```

```cpp
// Zenith2/Engine/ZenithCore/Zenith_RingBuffer.h
/// @contract container Zenith_RingBuffer
/// Ownership: Owns its elements; storage from the root heap or an arena. Move-only.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: TryPush under a GROW policy invalidates every pointer returned by Peek when it grows;
///   TryPop and DROP_OLDEST invalidate the popped or overwritten element's pointer.
/// Units: Counts in elements.
/// Errors: TryPush returns Zenith_Result<void, Zenith_QueueError>; TryPop returns false when empty;
///   a BLOCK_PRODUCER or GROW_THEN_BLOCK policy is ZENITH_CHECK at construction (a single-thread ring cannot
///   block); a FATAL policy at capacity terminates with FATAL_CODE_QUEUE_FULL.
/// Version: since Z2 Stage 1. Source API.
/// Alignment: elements at alignof(T).
/// Overflow: capacities at most 2^31; m_uHardCapacity ≥ m_uInitialCapacity ≥ 1 (ZENITH_CHECK at construction).
/// Allocation failure: construction allocates nothing; the first push allocates m_uInitialCapacity elements,
///   and failure there or during growth returns QUEUE_ERROR_OUT_OF_MEMORY and leaves the ring unchanged.
/// Complexity: TryPush and TryPop O(1) (amortised O(1) under GROW policies); Clear O(count).
/// @endcontract
template<typename T>
class Zenith_RingBuffer
{
public:
	Zenith_RingBuffer(const Zenith_BoundedQueueDesc& xDesc, const Zenith_QueueCounterNames& xNames);
	Zenith_RingBuffer(const Zenith_BoundedQueueDesc& xDesc, const Zenith_QueueCounterNames& xNames, Zenith_Arena& xArena);
	~Zenith_RingBuffer();
	Zenith_RingBuffer(Zenith_RingBuffer&& xOther);
	Zenith_RingBuffer& operator=(Zenith_RingBuffer&& xOther);
	Zenith_Result<void, Zenith_QueueError> TryPush(T&& xValue);
	bool TryPop(T& xOut);
	T* PeekFront();           // nullptr when empty
	T& operator[](uint32 uIndexFromFront);   // ZENITH_ASSERT in range
	void Clear();
	uint32 GetCount() const;
	uint32 GetCapacity() const;
	Zenith_QueueStats GetStats() const;
};
```

Push behaviour per policy when `count == capacity`:

| Policy | Action | Counters |
|---|---|---|
| `QUEUE_FULL_POLICY_REJECT` | return `QUEUE_ERROR_FULL` | `.full` +1 |
| `QUEUE_FULL_POLICY_GROW_THEN_REJECT` | if capacity < hard capacity: grow to `min(capacity × 2, hard)`, linearising the elements from the front; else as REJECT | `.full` +1 only when rejecting |
| `QUEUE_FULL_POLICY_DROP_OLDEST` | destroy the front element, then push | `.dropped` +1 |
| `QUEUE_FULL_POLICY_DROP_NEWEST` | discard the pushed element, return `QUEUE_ERROR_FULL` | `.dropped` +1 |
| `QUEUE_FULL_POLICY_FATAL` | `ZENITH_FATAL(FATAL_CODE_QUEUE_FULL, …)` naming the queue | — |

- Counter publication: whenever a counter value changes (a full event, a drop, or a new high-water mark), the ring calls `Zenith_DispatchCounter` with the matching name, the new cumulative value and `COUNTER_KIND_CUMULATIVE` ([Z2-N-CONTRACT-345], [Z2-N-CONTRACT-590]), which does nothing when no counter hook is installed; high-water changes are monotonic, so the calls are rare after warm-up.

## Zenith_BoundedQueue

```cpp
// Zenith2/Engine/ZenithCore/Zenith_BoundedQueue.h
/// @contract container Zenith_BoundedQueue
/// Ownership: Owns its elements; one allocation from the root heap at Create. Created through Create as a
///   Zenith_Owned<Zenith_BoundedQueue<T>> (it holds atomics and semaphores and never moves).
/// Thread affinity: ANY_THREAD. Push and Pop are callable concurrently from any number of threads;
///   PushWait and PopWait are callable only from threads that are not job workers.
/// Invalidation: None; elements are moved in and out, never referenced in place.
/// Units: Counts in elements.
/// Errors: Create returns Zenith_Result<…, Zenith_QueueError> (QUEUE_ERROR_OUT_OF_MEMORY); TryPush returns
///   QUEUE_ERROR_FULL under REJECT and DROP_NEWEST; FATAL terminates with FATAL_CODE_QUEUE_FULL; a GROW or
///   DROP_OLDEST policy, or a hard capacity that is not a power of two in 2 … 2^30, is ZENITH_CHECK at Create;
///   PushWait or PopWait on a job-worker thread is ZENITH_CHECK wherever ZENITH_ASSERTS_ENABLED=1.
/// Version: since Z2 Stage 2. Source API.
/// Alignment: cells at max(alignof(T), 64) so producers and consumers do not share a cache line per cell.
/// Overflow: hard capacity at most 2^30 cells.
/// Allocation failure: only Create allocates; its failure leaves nothing allocated.
/// Complexity: TryPush and TryPop O(1) expected, lock-free; PushWait and PopWait block on a semaphore.
/// Locks: none; the two semaphores are `Zenith_SemaphoreNoProfiling` members, not mutexes, and have no lock-order row.
/// @endcontract
template<typename T>
class Zenith_BoundedQueue
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_BoundedQueue>, Zenith_QueueError> Create(
		const Zenith_BoundedQueueDesc& xDesc, const Zenith_QueueCounterNames& xNames);
	~Zenith_BoundedQueue();                  // destroys remaining elements
	Zenith_Result<void, Zenith_QueueError> TryPush(T&& xValue);
	void PushWait(T&& xValue);               // QUEUE_FULL_POLICY_BLOCK_PRODUCER: waits for space
	bool TryPop(T& xOut);
	bool PopWait(T& xOut, uint64 ulTimeoutUs);   // false on timeout
	void WakeAllWaiters();                   // shutdown: every blocked PopWait returns false promptly
	uint32 GetApproximateCount() const;
	Zenith_QueueStats GetStats() const;
};
```

Algorithm (the bounded MPMC queue with per-cell sequence numbers):

1. Cells `c[i]` for `i` in `[0, N)`, `N` the hard capacity; each cell holds `std::atomic<uint64> m_ulSequence` initialised to `i` and raw storage for one `T`.
2. `TryPush`: `pos = m_ulEnqueue.load(relaxed)`; loop: `seq = c[pos & (N−1)].m_ulSequence.load(acquire)`; if `seq == pos`, `compare_exchange_weak(m_ulEnqueue, pos, pos + 1, relaxed)`; on success move-construct into the cell and `store(pos + 1, release)` into its sequence, done; if `seq < pos` the queue is full; otherwise reload `pos`.
3. `TryPop`: symmetric with `m_ulDequeue`, expecting `seq == pos + 1`; on success move the value out, destroy the cell's `T`, and store `pos + N` into the sequence.
4. Full handling: REJECT and DROP_NEWEST return `QUEUE_ERROR_FULL` (counting `.full` or `.dropped`); FATAL terminates; BLOCK_PRODUCER applies only to `PushWait`: increment `m_uProducerWaiters`, retry `TryPush`, and if still full wait on `m_xSpaceSemaphore`, then loop; each successful pop posts `m_xSpaceSemaphore` once when `m_uProducerWaiters > 0` (a surplus post only causes one extra retry).
5. `PopWait` mirrors step 4 with `m_uConsumerWaiters` and `m_xItemSemaphore`, and returns false after `ulTimeoutUs` or after `WakeAllWaiters`.
6. `PushWait` and `PopWait` check the Debug thread state's job-worker flag through `Zenith_CheckBlockingAllowed("Zenith_BoundedQueue")` before blocking ([Z2-N-CONTRACT-315]), and report the blocked interval with a `Zenith_BlockingWaitScope<true>` named `Zenith_BoundedQueue` around the semaphore wait, which dispatches `Zenith_DispatchBlockingWait` with the measured wait after it returns. The queue's two semaphores are `Zenith_SemaphoreNoProfiling`, so one blocked wait is one record, by the queue's name, and the queue holds no profiled primitive a hook could reach.

## Zenith_SpscRing

<!-- elaboration -->
`Zenith_SpscRing<T>` is the programme's one single-producer single-consumer primitive (ruling Q80). AUDIO's real-time channels use it, as do DEVT's per-thread profiler event rings and debug-draw recorder rings (cross-area rows AU6 and DV23; DEVT keeps no ring of its own).

It is header-only and wait-free, and calls nothing: no profiler hook, no log, no system call, and it names no synchronisation primitive, profiled or not. That makes it usable on the audio device thread and inside the profiler hooks, where it is one of the ZenithCore sources a hook dispatch reaches (`Zenith2/Docs/Design/CORE/Threading.md` → "Lock profiling and hook dispatch"). It lives in ZenithCore because a concurrency primitive needs `linux-tsan` coverage ([Z2-N-CONTRACT-357]), and ZenithAudio is never built in that tree.

The declaration below is AUDIO's specification (`Zenith2/Docs/Design/AUDIO.md` → "Assumed interfaces pending other areas"), adopted with two changes:

- AUDIO's `Zenith_MutableSpan<T>` is CORE's `Zenith_Span<T>` with a non-const `T`.
- A test-access type is added for the index-wrap test.

```cpp
// Zenith2/Engine/ZenithCore/Zenith_SpscRing.h
#include "Zenith_Span.h"
#include "Zenith_QueuePolicy.h"     // Zenith_QueueStats

/// @contract container Zenith_SpscRing
/// Ownership: Borrows caller-owned storage of exactly N elements, N a power of two in [2, 2^20], given to Init;
///   allocates nothing, ever. Neither copyable nor movable (it holds atomics the two sides address).
/// Thread affinity: ONE PRODUCER AT A TIME and ONE CONSUMER AT A TIME, the two concurrent with each other. The
///   producing thread may differ between calls provided every producer call made by one thread happens-before every
///   producer call made by the next (a frame-graph edge, a semaphore post and wait, or another release/acquire
///   pair hands the role over); the same rule holds for the consumer. Producer calls: TryPush, PushSpan,
///   GetWritableCount. Consumer calls: TryPop, PopSpan, GetReadableCount. GetCapacity and GetStats are ANY_THREAD.
/// Invalidation: None; elements are copied in and copied out, never referenced in place.
/// Units: Counts in elements.
/// Errors: TryPush returns false when the ring holds N elements (the owner's declared policy is
///   QUEUE_FULL_POLICY_DROP_NEWEST: the ring counts the drop); PushSpan pushes as many as fit and counts the rest as
///   dropped; TryPop returns false when empty; Init with storage whose count is not a power of two in [2, 2^20], or
///   a second Init, is ZENITH_CHECK; any other member before Init is ZENITH_ASSERT.
/// Version: since Z2 Stage 2. Source API (header-only).
/// Alignment: the producer index and the consumer index live on separate 64-byte cache lines; storage at alignof(T).
/// Overflow: indices are free-running uint32; N divides 2^32, so wrapping is harmless.
/// Allocation failure: not applicable (no allocation).
/// Complexity: O(1) per element, wait-free; PushSpan and PopSpan copy at most two contiguous runs.
/// Locks: none. No profiler hook, no log, no system call.
/// @endcontract
template<typename T>
class Zenith_SpscRing
{
	static_assert(std::is_trivially_copyable_v<T>);
public:
	Zenith_SpscRing() = default;
	Zenith_SpscRing(const Zenith_SpscRing&) = delete;
	Zenith_SpscRing& operator=(const Zenith_SpscRing&) = delete;
	void Init(Zenith_Span<T> axStorage);                             // before either side's first call
	bool TryPush(const T& xValue);                     // producer
	uint32 PushSpan(Zenith_Span<const T> axValues);   // producer: as many as fit, in order
	bool TryPop(T& xOut);                              // consumer
	uint32 PopSpan(Zenith_Span<T> axOut);             // consumer: up to the span's count
	uint32 GetReadableCount() const;                  // consumer
	uint32 GetWritableCount() const;                  // producer
	uint32 GetCapacity() const;                       // N (0 before Init)
	Zenith_QueueStats GetStats() const;                // ANY_THREAD, relaxed reads
private:
	friend struct Zenith_SpscRingTestAccess;
	T* m_pxStorage = nullptr;
	uint32 m_uMask = 0;                                  // N − 1; 0 before Init
	alignas(64) std::atomic<uint32> m_uWrite{ 0 };       // producer-owned, read by the consumer
	uint32 m_uCachedRead = 0;                            // producer-owned copy of m_uRead
	std::atomic<uint64> m_ulDropped{ 0 };                // producer increments (relaxed)
	alignas(64) std::atomic<uint32> m_uRead{ 0 };        // consumer-owned, read by the producer
	uint32 m_uCachedWrite = 0;                           // consumer-owned copy of m_uWrite
	std::atomic<uint32> m_uHighWater{ 0 };               // consumer updates (relaxed)
};

// Tests only: Zenith2/Engine/ZenithCore/Zenith_SpscRing.h, after the class.
struct Zenith_SpscRingTestAccess
{
	template<typename T> static void SeedIndices(Zenith_SpscRing<T>& xRing, uint32 uIndex);   // empty ring: both indices and caches = uIndex
};
static_assert(sizeof(Zenith_SpscRing<uint32>) == 192 && alignof(Zenith_SpscRing<uint32>) == 64);
```

`std::atomic` and `std::is_trivially_copyable_v` come from `<atomic>` and `<type_traits>`, two of the allowed standard headers. `Zenith_SpscRingTestAccess::SeedIndices` asserts that the ring is initialised and empty. Production code never names it; the ForbiddenCheck test-access rule applies to it as to every `…TestAccess` type.

Every step below is one statement; N is `m_uMask + 1`.

| Operation | Steps |
|---|---|
| `Init` | `ZENITH_CHECK(m_pxStorage == nullptr)`; `ZENITH_CHECK(count is a power of two in [2, 2^20])`; store the pointer and `m_uMask = count − 1` (plain stores: the ring is published to both sides by whatever hands them their roles) |
| `TryPush` | `w = m_uWrite.load(relaxed)`; if `w − m_uCachedRead == N`, refresh `m_uCachedRead = m_uRead.load(acquire)`; still full → `m_ulDropped.fetch_add(1, relaxed)`, return false; else copy into `storage[w & mask]`, `m_uWrite.store(w + 1, release)`, return true |
| `PushSpan` | as `TryPush` for the whole span: `free = N − (w − cachedRead)`, refreshed once with acquire when smaller than the span's count; copy `min(free, count)` elements in at most two contiguous runs (the run up to the end of the storage, then the run from its start); one `m_uWrite.store(w + copied, release)`; add `count − copied` to `m_ulDropped` (relaxed) when non-zero; return `copied` |
| `TryPop` | `r = m_uRead.load(relaxed)`; if `r == m_uCachedWrite`, refresh `m_uCachedWrite = m_uWrite.load(acquire)`; still empty → false; else copy out of `storage[r & mask]`; when `m_uCachedWrite − r` exceeds `m_uHighWater.load(relaxed)`, `m_uHighWater.store(m_uCachedWrite − r, relaxed)`; `m_uRead.store(r + 1, release)`; return true |
| `PopSpan` | as `TryPop` for up to the span's count: one acquire refresh when the cached count is smaller than the span, copy in at most two runs, the high-water update once with the readable count before the copy, one release store; return the count popped |
| `GetReadableCount` / `GetWritableCount` | `m_uWrite.load(acquire) − m_uRead.load(relaxed)` / `N − (m_uWrite.load(relaxed) − m_uRead.load(acquire))` |
| `GetStats` | `{ m_ulFullEvents = 0, m_ulDropped = m_ulDropped.load(relaxed), m_uHighWater = m_uHighWater.load(relaxed) }` |

Capacity semantics: the ring holds exactly N elements (full when `write − read == N`), not N − 1.

Counters ([Z2-N-CONTRACT-345]): the ring calls no hook. Its owner publishes `<queue>.full`, `<queue>.dropped` and `<queue>.high_water` from `GetStats` on a control-side thread. `<queue>.full` is always 0 under DROP_NEWEST and is published for uniformity.

## Zenith_TripleBuffer

<!-- elaboration -->
`Zenith_TripleBuffer<T>` is a wait-free latest-value hand-over: the producer always has a back slot to write, the consumer always reads a complete front slot, and nothing is copied at publish or acquire. AUDIO's mixer snapshot, meter frame and main-thread state use it (row AU6). It is the depth-1 `QUEUE_FULL_POLICY_DROP_OLDEST` queue of [Z2-N-ARCH-434].

```cpp
// Zenith2/Engine/ZenithCore/Zenith_TripleBuffer.h
#include "Zenith_Span.h"
#include "Zenith_QueuePolicy.h"     // Zenith_QueueStats

/// @contract container Zenith_TripleBuffer
/// Ownership: Borrows caller-owned storage of exactly three T given to the constructor; allocates nothing; not
///   copyable or movable. The buffer never writes slot contents: the owner initialises all three slots to a valid
///   value before the consumer's first GetFront.
/// Thread affinity: ONE PRODUCER AT A TIME (BeginWrite, PublishWrite) and ONE CONSUMER AT A TIME (AcquireLatest,
///   GetFront), handed over exactly as Zenith_SpscRing's roles; GetStats ANY_THREAD.
/// Invalidation: The reference BeginWrite returns is valid until PublishWrite; the reference GetFront returns until
///   the next AcquireLatest that returns true.
/// Units: None.
/// Errors: A span whose count is not 3 is ZENITH_CHECK. A publish that replaces an unconsumed one counts the older
///   as dropped (QUEUE_FULL_POLICY_DROP_OLDEST, depth 1).
/// Version: since Z2 Stage 2. Source API (header-only).
/// Alignment: the shared index byte on its own 64-byte cache line.
/// Overflow: the publish and drop counts are uint64.
/// Allocation failure: not applicable.
/// Complexity: O(1), wait-free; no copy at publish or acquire.
/// Locks: none; no hook, no log, no system call.
/// @endcontract
template<typename T>
class Zenith_TripleBuffer
{
	static_assert(std::is_trivially_copyable_v<T>);
public:
	explicit Zenith_TripleBuffer(Zenith_Span<T> axSlots);   // exactly 3, initialised by the owner; slot 0 is the initial front
	Zenith_TripleBuffer(const Zenith_TripleBuffer&) = delete;
	Zenith_TripleBuffer& operator=(const Zenith_TripleBuffer&) = delete;
	T& BeginWrite();               // producer: the back slot; again before PublishWrite returns the same slot
	void PublishWrite();                         // producer: the back slot becomes the newest
	bool AcquireLatest();          // consumer: true when a newer slot became the front
	const T& GetFront() const;     // consumer: slot 0's contents until the first acquire
	Zenith_QueueStats GetStats() const;   // ANY_THREAD
private:
	T* m_pxSlots = nullptr;
	uint8 m_uBack = 2;                                   // producer-owned
	uint8 m_uFront = 0;                                  // consumer-owned
	alignas(64) std::atomic<uint8> m_uMiddle{ 1 };       // slot index in bits 0–1, bit 2 = FRESH
	std::atomic<uint64> m_ulPublished{ 0 };
	std::atomic<uint64> m_ulSuperseded{ 0 };
};
static_assert(sizeof(Zenith_TripleBuffer<uint32>) == 128 && alignof(Zenith_TripleBuffer<uint32>) == 64);
```

| Operation | Steps |
|---|---|
| constructor | `ZENITH_CHECK(axSlots.GetCount() == 3)`; store the pointer; front 0, middle 1 (not fresh), back 2 |
| `BeginWrite` | return `m_pxSlots[m_uBack]` |
| `PublishWrite` | `prev = m_uMiddle.exchange(m_uBack \| FRESH, acq_rel)`; when `prev & FRESH`, `m_ulSuperseded.fetch_add(1, relaxed)`; `m_uBack = prev & 3`; `m_ulPublished.fetch_add(1, relaxed)` |
| `AcquireLatest` | when `(m_uMiddle.load(relaxed) & FRESH) == 0` return false; `prev = m_uMiddle.exchange(m_uFront, acq_rel)`; `m_uFront = prev & 3`; return true (the exchange's acquire half makes the producer's writes to that slot visible) |
| `GetFront` | return `m_pxSlots[m_uFront]` |
| `GetStats` | `{ m_ulFullEvents = 0, m_ulDropped = m_ulSuperseded.load(relaxed), m_uHighWater = m_ulPublished.load(relaxed) > 0 ? 1 : 0 }` |

## Zenith_BitArray

A fixed-capacity bit set: its bit count is chosen once, at initialisation, and never grows. The script compiler's per-manifest acceptance set `m_xAccepted` (`Zenith2/Docs/Scripting/PublicTypes.md` → "ZenithScriptCompiler public types") is one.

```cpp
// Zenith2/Engine/ZenithCore/Zenith_BitArray.h
/// @contract container Zenith_BitArray
/// Ownership: Owns ceil(bits / 64) uint64 words from the root heap, or from the Zenith_Arena passed to the
///   arena constructor. Move-only. The capacity is fixed by TryInitialise or Initialise.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: TryInitialise and Initialise invalidate every GetWords span; nothing else does.
/// Units: Bit indices and counts.
/// Errors: TryInitialise returns Zenith_Result<void, Zenith_AllocError>; an index ≥ GetBitCount() is
///   ZENITH_ASSERT; Initialise failure routes through the OOM handler (heap tier) or FATAL_CODE_ARENA_EXHAUSTED.
/// Version: since Z2 Stage 2. Source API.
/// Alignment: words at alignof(uint64).
/// Overflow: at most 2^40 bits; the word-count computation is overflow-checked.
/// Allocation failure: TryInitialise leaves the previous contents and capacity unchanged.
/// Complexity: Set, Clear, Test, Assign O(1); SetAll, ClearAll, CountSet, FindFirstSet, FindFirstClear,
///   ForEachSet O(bits / 64) using std::popcount and std::countr_zero.
/// @endcontract
class Zenith_BitArray
{
public:
	constexpr Zenith_BitArray() = default;                      // 0 bits, root heap
	explicit Zenith_BitArray(Zenith_Arena& xArena);             // 0 bits, arena tier
	Zenith_BitArray(Zenith_BitArray&& xOther);
	Zenith_BitArray& operator=(Zenith_BitArray&& xOther);
	Zenith_BitArray(const Zenith_BitArray&) = delete;
	Zenith_BitArray& operator=(const Zenith_BitArray&) = delete;
	~Zenith_BitArray();
	Zenith_Result<void, Zenith_AllocError> TryInitialise(uint64 ulBitCount, bool bValue);
	void Initialise(uint64 ulBitCount, bool bValue);
	void Set(uint64 ulBit);
	void Clear(uint64 ulBit);
	void Assign(uint64 ulBit, bool bValue);
	bool Test(uint64 ulBit) const;
	void SetAll();
	void ClearAll();
	uint64 CountSet() const;
	uint64 FindFirstSet(uint64 ulFromBit) const;     // GetBitCount() when none
	uint64 FindFirstClear(uint64 ulFromBit) const;   // GetBitCount() when none
	template<typename TVisitor> void ForEachSet(TVisitor&& xVisitor) const;   // ascending bit order
	uint64 GetBitCount() const;
	Zenith_Span<const uint64> GetWords() const;
	bool operator==(const Zenith_BitArray& xOther) const;       // same count and bits
private:
	uint64* m_pulWords = nullptr;
	uint64 m_ulBitCount = 0;
	Zenith_Arena* m_pxArena = nullptr;
};
```

- Invariant: bits at and above `GetBitCount()` in the last word are always zero, so `CountSet`, `operator==` and `GetWords` never see stale bits; `SetAll` masks the last word.
- Bit `i` is bit `i % 64` of word `i / 64`, so `GetWords()` is a stable little-endian bit image usable as a hash input.

## Sorting and searching

```cpp
// Zenith2/Engine/ZenithCore/Zenith_Sort.h
/// @contract functions Zenith_Sort
/// Ownership: Operate on the caller's span; Zenith_StableSort takes its scratch from the caller's arena.
/// Thread affinity: ANY_THREAD. Each call touches only its arguments.
/// Invalidation: Sorting permutes the elements in place; pointers into the span keep addressing the same
///   positions, which then hold different elements.
/// Units: None.
/// Errors: Zenith_TryStableSort returns Zenith_Result<void, Zenith_AllocError> (scratch allocation) and leaves
///   the span unchanged on failure; a comparator that is not a strict weak ordering is a programmer error,
///   caught by ZENITH_ASSERT on the heap-sort fallback's bounds.
/// Version: since Z2 Stage 1. The output for identical input is identical on every platform and build.
/// @endcontract
template<typename T, typename TLess> void Zenith_Sort(Zenith_Span<T> xSpan, TLess&& xLess);
template<typename T> void Zenith_Sort(Zenith_Span<T> xSpan);                       // operator<
template<typename T, typename TLess>
Zenith_Result<void, Zenith_AllocError> Zenith_TryStableSort(Zenith_Span<T> xSpan, Zenith_Arena& xScratch, TLess&& xLess);
template<typename T, typename TLess> void Zenith_StableSort(Zenith_Span<T> xSpan, Zenith_Arena& xScratch, TLess&& xLess);
template<typename T, typename TKey, typename TLess>
uint64 Zenith_LowerBound(Zenith_Span<const T> xSpan, const TKey& xKey, TLess&& xLess);
template<typename T, typename TKey, typename TLess>
uint64 Zenith_UpperBound(Zenith_Span<const T> xSpan, const TKey& xKey, TLess&& xLess);
template<typename T, typename TLess> bool Zenith_IsSorted(Zenith_Span<const T> xSpan, TLess&& xLess);
```

`Zenith_Sort` is an introsort with no randomness: ranges of at most 16 elements are insertion-sorted; otherwise the pivot is the median of the first, middle and last elements, the range is partitioned (Hoare partition with the pivot moved to the front), the smaller side is recursed and the larger looped; when the recursion depth exceeds `2 × floor(log2(n))` the range is heap-sorted. `Zenith_StableSort` is a bottom-up merge sort with a scratch buffer of `n` elements from the arena (runs of 32 insertion-sorted first). Both are deterministic: the same input sequence and comparator give the same output sequence on every platform.

## Container tests

Every container carries the four Contract-mandated tests ([Z2-N-CONTRACT-288] to [Z2-N-CONTRACT-291]) plus its own cases. The oracle is the STL equivalent named in the table (the tests may use the STL); the counting type and generator are `Zenith2/Engine/ZenithCore/Tests/Support/` (`Zenith2/Docs/Design/CORE/Memory.md` → "Fault-injection support").

| Container | Test file | Oracle | Container-specific tests (besides `_MatchesOracle`, `_Properties`, `_TryFailure_LeavesUnchanged`, `_SizeOverflow_IsRejected`) |
|---|---|---|---|
| `Zenith_Span` | `Zenith_Span_Tests.cpp` | `std::span` | `Test_Span_Subspan_OutOfRange_Asserts` (`z2-death-assert`), `Test_Span_ConstConversion_Compiles`, `Test_Span_AsBytes_CountsBytes` |
| `Zenith_Array` | `Zenith_Array_Tests.cpp` | `std::vector` | `Test_Array_ArenaTier_NeverFreesToHeap` (heap live count unchanged by growth), `Test_Array_Growth_StrongGuarantee` (a failed growth leaves elements, count and capacity identical), `Test_Array_RemoveAtSwap_MovesLast`, `Test_Array_MoveOnlyElement_OwnedPointer` (an array of `Zenith_Owned<T>` grows and destroys each once), `Test_Array_Index_OutOfRange_Asserts` (`z2-death-assert`) |
| `Zenith_FixedArray` | `Zenith_FixedArray_Tests.cpp` | `std::array` | `Test_FixedArray_LayoutMatchesCArray` (size, alignment and offsets equal `T[N]` for five `T`), `Test_FixedArray_Index_OutOfRange_Asserts` (`z2-death-assert`) |
| `Zenith_InlineArray` | `Zenith_InlineArray_Tests.cpp` | `std::vector` bounded to the capacity | `Test_InlineArray_Full_TryPushFails`, `Test_InlineArray_NeverAllocates` (heap statistics unchanged across 100,000 operations), `Test_InlineArray_PushFull_Checks` (`z2-death-check`) |
| `Zenith_HashMap`, `Zenith_HashSet` | `Zenith_HashMap_Tests.cpp`, `Zenith_HashSet_Tests.cpp` | `std::unordered_map`, `std::unordered_set` (content), plus a `std::vector` model of the dense order | `Test_HashMap_IterationOrder_IndependentOfCapacity` (the same operation sequence after `Reserve(16)` and `Reserve(65536)` iterates identically), `Test_HashMap_IterationOrder_IdenticalAcrossRuns` (a fixed sequence's order matches a committed expected list), `Test_HashMap_RemoveBackwardShift_KeepsAllFindable`, `Test_HashMap_HeterogeneousLookup_StringView`, `Test_HashMap_KeyOrder_SortsByKey`, `Test_HashMap_Collisions_AllKeysRetained` (a traits type hashing every key to 0), `Test_HashSet_GetKeys_MatchesIterationOrder` (`GetKeys()` equals the keys visited by iteration, in the same order) |
| `Zenith_RingBuffer` | `Zenith_RingBuffer_Tests.cpp` | `std::deque` | one test per policy: `Test_RingBuffer_Reject_ReturnsFull`, `Test_RingBuffer_GrowThenReject_GrowsToHardCap`, `Test_RingBuffer_DropOldest_OverwritesFront`, `Test_RingBuffer_DropNewest_DiscardsPushed`, `Test_RingBuffer_Fatal_Terminates` (`z2-death-check`), `Test_RingBuffer_BlockPolicy_Checks` (`z2-death-check`), `Test_RingBuffer_Counters_PublishedOnChange` |
| `Zenith_BoundedQueue` | `Zenith_BoundedQueue_Tests.cpp` | a `std::mutex` + `std::deque` model | `Test_BoundedQueue_MPMC_NoLossNoDuplicate` (`z2-stress`, 2, 8 and hardware-concurrency producers and consumers, 10^6 items, each item consumed exactly once), `Test_BoundedQueue_BlockProducer_WakesOnPop`, `Test_BoundedQueue_PopWait_TimesOut`, `Test_BoundedQueue_WakeAllWaiters_ReleasesConsumers`, `Test_BoundedQueue_PushWaitOnJobWorker_Checks` (`z2-death-check`), `Test_BoundedQueue_NonPowerOfTwo_Checks` (`z2-death-check`); the stress case also runs under `linux-tsan` |
| `Zenith_SpscRing` | `Zenith_SpscRing_Tests.cpp`, `Zenith_SpscRing_DefectTests.cpp` | a single-thread `std::deque` model with the same capacity and DROP_NEWEST | `Test_SpscRing_ApiSurface_Layout` (size 192, alignment 64, not copyable, the member signatures), `Test_SpscRing_FifoOrderAndExactCapacity` (N pushes succeed, the N+1th returns false and counts one drop, pops return the pushed order), `Test_SpscRing_IndicesWrapPast32Bits` (both indices seeded at `2^32 − 3` through `Zenith_SpscRingTestAccess::SeedIndices`, then 3N pushes and pops in order), `Test_SpscRing_SpansAcrossTheWrap` (`PushSpan` and `PopSpan` across the storage end copy two runs; a too-long `PushSpan` copies what fits and counts the rest dropped), `Test_SpscRing_StatsCountDropsAndHighWater`, `Test_SpscRing_InitRejectsBadCapacity` (`z2-death-check`: capacity 3, 0, a second `Init`), `Test_SpscRing_UseBeforeInit_Asserts` (`z2-death-assert`), `Test_SpscRing_StressRoleHandOver` (`z2-stress`, also `linux-tsan`: 10^7 sequence-numbered elements with 2, 8 and hardware-concurrency threads; with 2, one producer and one consumer; with more, the producer and consumer roles each rotate among threads every 4,096 elements through a `Zenith_Semaphore` chain; every element received once, in order); the four mandated tests are `Test_SpscRing_MatchesOracle`, `Test_SpscRing_Properties`, `Test_SpscRing_TryFailure_LeavesUnchanged` (a full ring's contents are unchanged by a refused push) and `Test_SpscRing_SizeOverflow_IsRejected` (`z2-death-check`: storage of 2^21 elements) |
| `Zenith_TripleBuffer` | `Zenith_TripleBuffer_Tests.cpp`, `Zenith_TripleBuffer_DefectTests.cpp` | a model holding the newest published value | `Test_TripleBuffer_ApiSurface_Layout` (size 128, alignment 64, not copyable), `Test_TripleBuffer_InitialFrontIsSlotZero`, `Test_TripleBuffer_LatestWinsAndCountsSuperseded`, `Test_TripleBuffer_RepeatedBeginWriteReturnsSameSlot`, `Test_TripleBuffer_StressNoTornRead` (`z2-stress`, also `linux-tsan`: a producer writing self-checksummed 4 KiB values, the consumer verifying every acquired value, the producer role rotating among workers, 10^6 publishes, 2, 8 and hardware-concurrency threads); the four mandated tests are `Test_TripleBuffer_MatchesOracle`, `Test_TripleBuffer_Properties`, `Test_TripleBuffer_TryFailure_LeavesUnchanged` (`AcquireLatest` with nothing fresh returns false and keeps the front) and `Test_TripleBuffer_SizeOverflow_IsRejected` (`z2-death-check`: spans of 2 and 4 slots) |
| `Zenith_BitArray` | `Zenith_BitArray_Tests.cpp` | `std::vector<bool>` | `Test_BitArray_TailBitsStayZero`, `Test_BitArray_FindFirst_AcrossWords`, `Test_BitArray_ReinitialiseFailure_KeepsContents`, `Test_BitArray_Index_OutOfRange_Asserts` (`z2-death-assert`) |
| `Zenith_Sort` | `Zenith_Sort_Tests.cpp` | `std::sort`, `std::stable_sort` (results compared as sorted sequences; stability compared exactly) | `Test_Sort_AdversarialInputs_Sorted` (sorted, reverse, all-equal, organ-pipe, median-of-three killer, 10^6 elements), `Test_Sort_Deterministic_IdenticalOutput`, `Test_StableSort_PreservesEqualOrder`, `Test_StableSort_ScratchFailure_LeavesUnchanged` |

The benchmarks of "Justification records" live in `Zenith2/Benchmarks/CoreContainers/Zenith_Bench_CoreContainers.cpp`; their `--smoke` run is the CTest test `Bench_CoreContainers_Smoke`.
