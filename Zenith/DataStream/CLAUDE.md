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
- **Move semantics**: Move construction and move assignment transfer the data pointer, capacity, cursor, and ownership flag unchanged. The moved-from stream is reset to a null pointer, zero capacity/cursor, and non-owning state, so its destructor is harmless.
- Uses `Zenith_MemoryManagement::Allocate/Reallocate/Deallocate` for memory.

## Safety

- **Sanity limits**: Vector deserialization caps at 100M elements, strings at 1MB
- **Bounds checks**: Both debug asserts and runtime safety checks on all reads
- **Null checks**: All read/write operations validate pointers
- **Resize failure handling**: Prevents infinite loop if reallocation fails

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
