# Save Data System

## Overview

Engine-level save data system for persisting game state to disk. Built on `Zenith_DataStream` (serialization) and `Zenith_FileAccess` (file I/O). Cross-platform (Windows + Android).

## Files

- `Zenith_SaveData.h` - Public API (namespace `Zenith_SaveData`)
- `Zenith_SaveData.cpp` - Implementation

## Public API

| Function | Description |
|----------|-------------|
| `Initialise(szGameName)` | Set up save directory for this game |
| `GetSaveDirectory()` | Get platform-specific writable save path |
| `Save(szSlot, uVersion, pfnWrite, pUserData)` | Write game data to a named slot |
| `Load(szSlot, pfnRead, pUserData)` | Read game data from a named slot |
| `SlotExists(szSlot)` | Check if a save file exists |
| `DeleteSlot(szSlot)` | Remove a save file |
| `ComputeCRC32(pData, ulSize)` | CRC32 utility |

## File Format

```
[4 bytes] Magic (0x5A454E53 = "ZENS")
[4 bytes] Format version
[4 bytes] Game version (for data migration)
[4 bytes] CRC32 of payload
[8 bytes] Payload size
[8 bytes] Timestamp
[N bytes] Payload data
```

## Usage Pattern

```cpp
// Initialize once at startup
Zenith_SaveData::Initialise("MyGame");

// Define save data struct
struct MySaveData { int iLevel; float fScore; };

// Write callback (function pointer, NOT std::function)
static void WriteSave(Zenith_DataStream& xStream, void* pxUserData) {
    MySaveData* pxData = static_cast<MySaveData*>(pxUserData);
    xStream << pxData->iLevel;
    xStream << pxData->fScore;
}

// Read callback
static void ReadSave(Zenith_DataStream& xStream, uint32_t uGameVersion, void* pxUserData) {
    MySaveData* pxData = static_cast<MySaveData*>(pxUserData);
    xStream >> pxData->iLevel;
    xStream >> pxData->fScore;
}

// Save
MySaveData xData = { 5, 1234.5f };
Zenith_SaveData::Save("autosave", 1, WriteSave, &xData);

// Load
MySaveData xLoaded;
if (Zenith_SaveData::Load("autosave", ReadSave, &xLoaded)) {
    // xLoaded now contains saved data
}
```

## Save Directory

- Windows: `%APPDATA%/Zenith/<GameName>/`
- Android: `<internal files>/Zenith/<GameName>/`

## Key Design Decisions

- Function pointer callbacks (no `std::function`) per engine convention
- Game version in header enables data migration across updates
- CRC32 checksum detects file corruption
- Named slots allow multiple save files (e.g. "autosave", "save_0")
- Field-by-field serialization recommended over bulk memcpy for forward-compatibility
