# DataStream - Binary Serialization

## Overview

`Zenith_DataStream` is the engine's core binary serialization system. It provides a cursor-based read/write interface for serializing data to and from binary buffers. Used throughout the engine for scene files, assets, prefabs, and network data.

## Files

- `Zenith_DataStream.h` - Header-only implementation (all logic is inline/template)
- `Zenith_DataStream.cpp` - Includes precompiled header only

## API

### Construction
- `Zenith_DataStream()` - Allocates 1024-byte owned buffer
- `Zenith_DataStream(ulSize)` - Allocates owned buffer of specified size
- `Zenith_DataStream(pData, ulSize)` - Wraps external buffer (does NOT take ownership)
- Move semantics supported, copy is deleted

### Read/Write
- `WriteData(pData, ulSize)` / `ReadData(pData, ulSize)` - Raw bytes
- `operator<<(value)` / `operator>>(value)` - Type-dispatched serialization
- `SetCursor()` / `GetCursor()` / `SkipBytes()` - Cursor management

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
- Uses `Zenith_MemoryManagement::Allocate/Reallocate/Deallocate` for memory.

## Safety

- **Sanity limits**: Vector deserialization caps at 100M elements, strings at 1MB
- **Bounds checks**: Both debug asserts and runtime safety checks on all reads
- **Null checks**: All read/write operations validate pointers
- **Resize failure handling**: Prevents infinite loop if reallocation fails

## Key Patterns

- `WriteToFile` writes from start to cursor position (not full buffer)
- After `ReadFromFile`, cursor is at 0 - ready to deserialize
- Cursor is NOT automatically reset after operations - manage it manually
- `IsValid()` checks both non-null pointer and non-zero size (useful after file load)
