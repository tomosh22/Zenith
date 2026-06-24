# File Access System

## Overview

Platform-agnostic file I/O via namespace functions. Platform implementations in `Windows/FileAccess/` and `Android/FileAccess/`.

## Files

- `Zenith_FileAccess.h` - Namespace API and file extension constants

## Public API

| Function | Description |
|----------|-------------|
| `InitialisePlatform(void*)` | Setup platform access (Android: AAssetManager pointer) |
| `SetWritableDirectory(const char*)` | Set writable dir for relative-path resolution (Android: internalDataPath; no-op on Windows) |
| `ReadFile(const char*)` | Read file, return allocated buffer (caller must free) |
| `ReadFile(const char*, uint64_t&)` | Read with size output |
| `ReadPrefix(const char*, void*, uint64_t)` | Read exactly `ulSize` bytes into caller buffer for cheap header-peek (magic + version); returns false if fewer bytes available; no full-file allocation |
| `WriteFile(const char*, const void*, uint64_t)` | Write data (uses filesystem; on Android relative paths resolve against the writable dir set via `SetWritableDirectory`) |
| `FileExists(const char*)` | Check file existence |
| `FreeFileData(char*)` | Free buffer from ReadFile |

## File Extension Constants

| Constant | Value |
|----------|-------|
| `ZENITH_TEXTURE_EXT` | `.ztxtr` |
| `ZENITH_MESH_EXT` | `.zmesh` |
| `ZENITH_MESH_ASSET_EXT` | `.zasset` |
| `ZENITH_MATERIAL_EXT` | `.zmtrl` |
| `ZENITH_PREFAB_EXT` | `.zprfb` |
| `ZENITH_SCENE_EXT` | `.zscen` |
| `ZENITH_PARTICLES_EXT` | `.zptcl` |
| `ZENITH_BGRAPH_EXT` | `.bgraph` |
| `ZENITH_SKELETON_EXT` | `.zskel` |
| `ZENITH_MODEL_EXT` | `.zmodel` |
| `ZENITH_ANIMATION_EXT` | `.zanim` |
| `ZENITH_SAVE_EXT` | `.zsave` |
| `ZENITH_META_EXT` | `.zmeta` |

## Constants

- `ZENITH_MAX_PATH_LENGTH = 1024`

## Platform Implementations

- **Windows:** Standard filesystem API (`Zenith/Windows/FileAccess/Zenith_Windows_FileAccess.cpp`)
- **Android:** AAssetManager for read, standard I/O for write (`Zenith/Android/FileAccess/Zenith_Android_FileAccess.cpp`). `ReadFile`/`ReadPrefix` try `AAssetManager` first (APK assets) before falling back to the filesystem. `WriteFile` is unconditional (not tools-gated) and writes to the filesystem, resolving relative paths against the writable dir.

## Key Patterns

- Null-terminated C strings for paths
- Caller owns memory from `ReadFile` - must call `FreeFileData`
- Read failures return `nullptr`
- `SetWritableDirectory` resolves relative paths against the writable dir — required on Android (e.g. internalDataPath), a no-op on Windows
