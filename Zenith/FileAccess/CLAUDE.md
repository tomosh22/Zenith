# File Access System

## Overview

Platform-agnostic file I/O via namespace functions. Platform implementations in `Windows/FileAccess/` and `Android/FileAccess/`.

## Files

- `Zenith_FileAccess.h` - Namespace API and file extension constants

## Public API

| Function | Description |
|----------|-------------|
| `InitialisePlatform(void*)` | Setup platform access (Android: AAssetManager pointer) |
| `ReadFile(const char*)` | Read file, return allocated buffer (caller must free) |
| `ReadFile(const char*, uint64_t&)` | Read with size output |
| `WriteFile(const char*, const void*, uint64_t)` | Write data (tools-only on Android) |
| `FileExists(const char*)` | Check file existence |
| `FreeFileData(char*)` | Free buffer from ReadFile |

## File Extension Constants

| Constant | Value |
|----------|-------|
| `ZENITH_TEXTURE_EXT` | `.ztxtr` |
| `ZENITH_MESH_EXT` | `.zmesh` |
| `ZENITH_MATERIAL_EXT` | `.zmtrl` |
| `ZENITH_PREFAB_EXT` | `.zprfb` |
| `ZENITH_SCENE_EXT` | `.zscen` |

## Constants

- `ZENITH_MAX_PATH_LENGTH = 1024`

## Platform Implementations

- **Windows:** Standard filesystem API (`Windows/FileAccess/Zenith_Windows_FileAccess.cpp`)
- **Android:** AAssetManager for read, standard I/O for write (`Android/FileAccess/Zenith_Android_FileAccess.cpp`). Write is tools-only.

## Key Patterns

- Null-terminated C strings for paths
- Caller owns memory from `ReadFile` - must call `FreeFileData`
- Read failures return `nullptr`
- Android write restricted to tools builds
