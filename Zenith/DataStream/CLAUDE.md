# DataStream - Binary Serialization

## Overview

`Zenith_DataStream` is the engine's core binary serialization system. It provides a cursor-based read/write interface for serializing data to and from binary buffers. Used throughout the engine for scene files, assets, prefabs, and network data.

## Files

- `Zenith_DataStream.h` - Header-only implementation (all logic is inline/template)
- `Zenith_DataStream.cpp` - Includes precompiled header only
- `Zenith_StreamEnvelope.h` / `Zenith_StreamEnvelope.cpp` - Reusable binary header ("envelope") for typed-asset payloads (see below)

## API

### Construction
- `Zenith_DataStream()` - Allocates 1024-byte owned buffer
- `Zenith_DataStream(ulSize)` - Allocates owned buffer of specified size
- `Zenith_DataStream(pData, ulSize)` - Wraps external buffer (does NOT take ownership)
- Move semantics supported, copy is deleted

### Read/Write
- `WriteData(pData, ulSize)` / `ReadData(pData, ulSize)` - Raw bytes (`Write()` / `Read()` are thin aliases for these)
- `operator<<(value)` / `operator>>(value)` - Type-dispatched serialization
- `SetCursor()` / `GetCursor()` / `SkipBytes()` - Cursor management
- `GetCapacity()` - Buffer capacity in bytes; for an owned write stream this is NOT the bytes written (use `GetCursor()` for that)
- `GetRemainingBytes()` - `GetCapacity() - GetCursor()`; the budget a length-prefixed block can check a count against BEFORE reserving. Capacity-based exactly like `GetCapacity()`, so on an owned write stream it is the space left in the allocation, not the bytes left to read
- `HasReadFailure()` - Has any read on this stream been refused since the last reset point (see **Read failure** below)
- `MarkCorrupt(szReason)` - The public door onto the same flag, for SEMANTIC corruption the stream cannot see itself (an out-of-range enum, a count that cannot fit the remaining bytes)
- `OwnsData()` - True for growable engine-owned storage; false for fixed-capacity wrapped external storage
- `GetData()` (const and mutable) - Direct access to the underlying buffer pointer
- `IsValid()` - True if buffer is non-null with non-zero size (useful after `ReadFromFile`)

### File I/O
- `ReadFromFile(szFilename)` - Load file contents into stream
- `WriteToFile(szFilename)` - Write cursor position worth of data to file

## Serialization Protocol

The `<<`/`>>` operators use SFINAE to dispatch:
- **Trivially copyable types** (int, float, enums, PODs): Raw memcpy of `sizeof(T)` bytes
- **Non-trivially copyable types**: Calls `T::WriteToDataStream()` / `T::ReadFromDataStream()`

Custom types must implement:
```cpp
void WriteToDataStream(Zenith_DataStream& xStream) const;
void ReadFromDataStream(Zenith_DataStream& xStream);
```

### STL Type Support
Built-in `<<`/`>>` specializations for:
- `std::vector<T>` - Size prefix (u_int) then elements
- `std::pair<T1, T2>` - First then second
- `std::string` - Length prefix (u_int) then character data
- `std::unordered_map<T1, T2>` - Count prefix then key-value pairs

## Buffer Management

- **Owned buffers** (`m_bOwnsData = true`): Auto-resize by doubling on overflow. Freed in destructor.
- **External buffers** (`m_bOwnsData = false`): No resize capability. Assert on overflow.
- **Move semantics**: Move construction and move assignment transfer the data pointer, capacity, cursor, ownership flag **and the read-failure flag** unchanged — a stream that failed a read is still a failed stream after it is moved. The moved-from stream is reset to a null pointer, zero capacity/cursor, non-owning state and a CLEAR read-failure flag, so its destructor is harmless. Both are explicit field lists: a new member has to be added to both by hand.
- Uses `Zenith_MemoryManagement::Allocate/Reallocate/Deallocate` for memory.

## Safety

- **Sanity limits**: Vector deserialization caps at 100M elements, strings at 1MB
- **Bounds checks**: Both debug asserts and runtime safety checks on all reads
- **Null checks**: All read/write operations validate pointers
- **Resize failure handling**: Prevents infinite loop if reallocation fails
- **Wrap-safe cursor arithmetic**: `ReadData` budgets with `ulSize > GetCapacity() - GetCursor()`, never `GetCursor() + ulSize > GetCapacity()`. A hostile size out of a corrupt file makes the ADDITION wrap to a small number that passes the check and reaches a ~2^64-byte `memcpy`; the subtraction cannot underflow because `m_ulCursor <= m_ulDataSize` holds at every mutation of either

## Read failure

Every read that cannot be satisfied — a null buffer, a bounds overflow, a length prefix over its sanity cap — logs, leaves the cursor exactly where it was, and sets a sticky read-failure flag. `HasReadFailure()` reports it; `MarkCorrupt()` sets it for corruption only the caller can recognise.

**★ THE FLAG IS A REPORT, NOT A GATE.** A read taken with the flag already set behaves exactly as it would without it: still bounds-checked, still individually harmless, still advancing the cursor on success. It has to. Three production loops read to EOF and advance ONLY via the next read — `Zenith_Telemetry`, `Zenith_AssetRegistry`'s `.zdata` type name, `Flux_GrassTypeTable`'s measuring pass — and would spin forever if a set flag short-circuited reads.

The only readers that BRANCH on the flag are the composite ones (`std::vector`, `std::string`, `std::unordered_map`, and the `.zanim` key/tangent block readers in `Flux_AnimationClip.cpp`), and only inside their own block: a count prefix that could not be READ is not a count, so they reserve nothing and loop zero times; an element that did not land is not appended. Cursor behaviour on failure is unchanged.

**Two reset points, and both are rewinds:**

| Reset point | Why |
|---|---|
| `SetCursor()` | The repo's only "rewind and read it again" gesture. Two long-lived member streams are re-read from cursor 0 on every use and never see `ReadFromFile` — `Zenith_Prefab::m_xComponentData` and `Zenith_ComponentMeta`'s property overrides — so without this one bad read would poison a prefab for the life of the process. `Zenith_ComponentMeta`'s seek-to-the-next-component also relies on it to drop the previous component's failure. |
| `ReadFromFile()` | The buffer is replaced wholesale; whatever the previous contents failed to read says nothing about the new bytes. |

**Never advance the cursor to EOF to "compensate" for a refusal.** `Zenith_Tools_AnimMigrate` detects a truncated `.zanim` by comparing `GetCursor()` against `GetCapacity()` after a parse it believes succeeded, so a reader that parked the cursor at the end would report a truncated file as intact.

## Stream Envelope

`Zenith_StreamEnvelope.h` provides a reusable binary header that prefixes a typed-asset payload, generalizing the bespoke magic+version blocks that live inline in paths like `Zenith_SceneData` and `Zenith_AssetRegistry`.

- `Zenith_StreamHeader` struct - 4 `u_int` fields: magic, envelope version, asset type id, schema version
- Constants: `uSTREAM_ENVELOPE_MAGIC = 0x5A4E5448` ("ZNTH"), `uSTREAM_ENVELOPE_VERSION_CURRENT = 1`
- `Zenith_WriteStreamHeader(xStream, uAssetTypeId, uSchemaVersion)` - writes the header at the current cursor, before the payload
- `Zenith_ReadStreamHeader(xStream, uExpectedTypeId)` - non-destructive peek returning `Zenith_Result<Zenith_StreamHeader>`; on any error (`BAD_MAGIC` / `VERSION_MISMATCH` / `INVALID_ARGUMENT`) it restores the cursor so a legacy headerless stream can be rewound and read by the old path

**Adopters:** every typed binary asset — `.ztxtr` / `.zmtrl` / `.zmesh` / `.zskel` / `.zmodel` — now leads with this envelope. Their asset-type-ids and current schema versions are centralized in `AssetHandling/Zenith_AssetTypeIds.h`, and each reads through a status-returning `ParseStream` that rewinds to the pre-envelope layout on `BAD_MAGIC`. See `AssetHandling/CLAUDE.md` → "Typed-Asset Serialization".

## Key Patterns

- `WriteToFile` writes from start to cursor position (not full buffer)
- After `ReadFromFile`, cursor is at 0 - ready to deserialize
- Cursor is NOT automatically reset after operations - manage it manually
- `IsValid()` checks both non-null pointer and non-zero size (useful after file load)
