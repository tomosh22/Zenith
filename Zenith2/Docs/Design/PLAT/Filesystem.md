# PLAT design: virtual filesystem and asynchronous I/O

This file is part of the PLAT design family. The entry document is `Zenith2/Docs/Design/PLAT.md`. It specifies virtual paths and their validation, the four mounts and their precedence, mount sources, the mount manifest, synchronous file access, `Zenith_AsyncIO` with its priorities, chunked scheduling and cancellation, atomic writes, and low-disk detection and behaviour. The SPEC baseline is `Zenith2/Docs/Architecture/PlatformLayer.md` → "Virtual filesystem", "Asynchronous I/O" and "Low-disk behaviour"; path-traversal protection is a trust-boundary control of `Zenith2/Docs/Architecture.md` → "Security architecture", which PLAT implements and the SEC area owns as a boundary (`Zenith2/Docs/Design/SEC.md` (pending; its "Public API" section)).

## Scope of this file

| Topic | Heading here |
|---|---|
| the path type, its validation rules and their order | "Virtual paths" |
| mount kinds, sources, the public directory-source factory, resolution, the escape check, case policy, file identity stamps | "Mounts", "File identity stamps" |
| the install layout manifest, the public update-manifest verifier and patch verification | "The mount manifest", "Update manifest API", "Patch mount verification" |
| synchronous reads, listing and stat | "Synchronous file access" |
| files and directories named on the command line | "Command-line host files", "Directory-grant reads" |
| the asynchronous I/O service, including reads from an unmounted source | "Asynchronous I/O", "Reads from an unmounted source" |
| free-space checks and the low-disk state | "Low-disk detection and behaviour" |

The requirement each heading specifies is listed in `Zenith2/Docs/Design/PLAT.md` → "Requirement index".

<!-- informative -->
Zenith's file access was a namespace of free functions over raw OS paths (`Zenith_FileAccess::ReadFile(const char*)`), with the Android asset manager and the writable directory set through globals and no traversal check at all: a path from content could name any file the process could open. Zenith2 has no raw-path file API in runtime code: every access starts from a validated `Zenith_VirtualPath`, resolves through mounts, and is checked against its mount root after the OS has resolved links.
<!-- /informative -->

## Virtual paths

### Zenith_VirtualPath

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_VirtualFileSystem.h

enum Zenith_PathRoot : uint8
{
	PATH_ROOT_CONTENT = 1,           // "content:/" — read-only, resolved through loose, patch, package
	PATH_ROOT_USER,                  // "user:/"    — the user mount alone: config/, saves/, cache/, logs/, crash/; devtools/ outside Shipping (R10)
};

inline constexpr uint32 uVIRTUAL_PATH_MAX_BYTES = 512;
inline constexpr uint32 uVIRTUAL_PATH_MAX_COMPONENT_BYTES = 255;
inline constexpr uint32 uVIRTUAL_PATH_MAX_COMPONENTS = 64;

/// @contract value Zenith_VirtualPath
/// Ownership: Plain value; the text is stored inline (no allocation).
/// Thread affinity: IMMUTABLE.
///   Members non-const: a path is a copy-assigned value (GetParent walks, records that hold one).
/// Invalidation: None.
/// Units: Byte lengths of UTF-8 text.
/// Errors: Construction only through Zenith_ParseVirtualPath, Root and Append, which return Zenith_PathError; a
///   default-constructed path is invalid and every file operation given one returns FILE_ERROR_INVALID_PATH.
/// Version: since Z2 Stage 2. The text form is persisted (saves reference user paths, cooked data content paths);
///   the in-memory layout may change in any build.
/// @endcontract
class Zenith_VirtualPath
{
public:
	constexpr Zenith_VirtualPath() = default;
	static Zenith_VirtualPath Root(Zenith_PathRoot eRoot);      // "content:/" or "user:/", for listing
	bool IsValid() const;
	bool IsRoot() const;
	Zenith_PathRoot GetRoot() const;
	Zenith_StringView GetText() const;          // "content:/Config/game.config.json"
	Zenith_StringView GetRelative() const;      // "Config/game.config.json"
	Zenith_StringView GetFileName() const;      // "game.config.json"; empty for a root
	Zenith_StringView GetExtension() const;     // "json": after the last '.' of the file name
	uint32 GetComponentCount() const;
	Zenith_VirtualPath GetParent() const;       // the root's parent is the root
	Zenith_Result<Zenith_VirtualPath, Zenith_PathError> Append(Zenith_StringView strRelative) const;
	bool IsWithin(const Zenith_VirtualPath& xDirectory) const;   // component-wise prefix
	friend bool operator==(const Zenith_VirtualPath&, const Zenith_VirtualPath&);
	friend std::strong_ordering operator<=>(const Zenith_VirtualPath&, const Zenith_VirtualPath&);  // byte order of the text
private:
	char m_acText[uVIRTUAL_PATH_MAX_BYTES + 1] = {};
	uint16 m_uLength = 0;
	uint16 m_uRelativeOffset = 0;
	uint8 m_uComponentCount = 0;
	Zenith_PathRoot m_eRoot = PATH_ROOT_CONTENT;
	bool m_bValid = false;
};
static_assert(sizeof(Zenith_VirtualPath) <= 520);

Zenith_Result<Zenith_VirtualPath, Zenith_PathError> Zenith_ParseVirtualPath(Zenith_StringView strPath);
```

`Append` validates its argument as a relative path of the same rules (it is parsed as `<root>:/<existing relative>/<argument>`), so a joined path is exactly as safe as a parsed one; `Root` is the only way to name a root directory, because a root-only string fails rule R7.

### Validation rules and order

`Zenith_ParseVirtualPath` applies these rules in this order and returns the first failure, so every input has exactly one error (the table restates `Zenith2/Docs/Architecture/PlatformLayer.md` → "Path validation" and adds the order and the rows marked elaboration):

| # | Rule | Rejected with |
|---|---|---|
| R1 | the input is at most 512 bytes (checked first, on raw bytes, so a hostile input costs O(512) at most) | `PATH_ERROR_TOO_LONG` |
| R2 | the input is valid UTF-8: no overlong encoding, no surrogate code point (U+D800–U+DFFF), nothing above U+10FFFF, no truncated sequence; and contains no NUL, no C0 control (U+0001–U+001F), no DEL (U+007F) and no C1 control (U+0080–U+009F) | `PATH_ERROR_ENCODING` |
| R3 | the input begins with exactly `content:/` or `user:/` (case-sensitive); the empty string fails here | `PATH_ERROR_BAD_ROOT` |
| R4 | no `\` anywhere | `PATH_ERROR_SEPARATOR` |
| R5 | after the root: the first character is not `/` (no `content://…`), and no `:` appears anywhere (this also rejects drive letters and Windows alternate data streams) | `PATH_ERROR_ABSOLUTE` |
| R6 | (elaboration) no character from `<`, `>`, `"`, `\|`, `?`, `*` | `PATH_ERROR_FORBIDDEN_CHARACTER` |
| R7 | splitting the remainder on `/` yields no empty component (from `//`, a trailing `/` or a root-only input) and no component that is `.` or `..` | `PATH_ERROR_TRAVERSAL` |
| R8 | every component is at most 255 bytes, and there are at most 64 components | `PATH_ERROR_TOO_LONG` |
| R9 | no component is a Windows reserved device name (table "Reserved device names"), begins with a space, or ends with `.` or a space | `PATH_ERROR_RESERVED_NAME` |
| R10 | a `user:/` path's first component is exactly one of `config`, `saves`, `cache`, `logs` and `crash` in every build, or `devtools` in a build where `Zenith_IsShippingBuild()` is false (table "User namespace roots") | `PATH_ERROR_USER_NAMESPACE` |

- Rule R7 is the traversal rule: with no `.` or `..` component and no empty component, a validated path can only name a descendant of its root, whatever the platform.
- Percent signs, `~`, full-width solidus U+FF0F and other look-alike characters are ordinary characters: nothing in Zenith2 decodes, expands or folds them, so they cannot become separators or parent references.
- Validation is total: every input either parses or yields exactly one `Zenith_PathError`, never an assert (`Zenith2/Docs/Contracts.md` → "Errors and results", "Untrusted input").

The table "User namespace roots" is rule R10's list (the per-build rule is SEC's ruling, `Zenith2/Docs/Design/SEC/PathTraversal.md` → "Rules confirmed by the ruling"):

| First component | Accepted in | Example paths |
|---|---|---|
| `config` | every build | `user:/config/user.config.json`, `user:/config/input.bindings.json` |
| `saves` | every build | `user:/saves/resume.zsave`; ONLINE's sync state under `user:/saves/online/` |
| `cache` | every build | `user:/cache/pso/` |
| `logs` | every build | `user:/logs/rhi_report.json` |
| `crash` | every build | DEVT's crash reports, pending reports and Crashpad database (`Zenith2/Docs/Design/DEVT/Crash.md` → "Crash report files"), RHI's `user:/crash/device_loss_<utc>.json` (`Zenith2/Docs/Design/RHI/Recovery.md` → "Device-loss report") |
| `devtools` | non-Shipping builds only (`Debug`, `Release`, `Sanitize`, `Hwasan`); refused in `Shipping` and `ShippingTest` | DEVT's development file writer root `user:/devtools/` (`Zenith2/Docs/Design/DEVT/Console.md` → "Zenith_DevFileWriter header"), CINE's `user:/devtools/captures/<name>/` (`Zenith2/Docs/Design/CINE/Formats.md` → "Format index") |

- R10 compares the first component's bytes, case-sensitively, with the five every-build names, and then with `devtools` only inside `if constexpr (!Zenith_IsShippingBuild())` (`ZenithCore/Zenith_BuildInfo.h`, `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Build information"), in `Internal/Zenith_VirtualPath.cpp`. No preprocessor conditional is involved and no type, member or table differs between configurations (Non-negotiable 11, `Zenith2/Docs/Contracts.md` → "No layout-changing conditionals", [Z2-N-CONTRACT-441], [Z2-N-CONTRACT-444]). This is the mechanism of the loose mount's Shipping refusal (§ "Mount kinds and roots", § "Directory-grant reads"), of the development listener and file watcher (`Zenith2/Docs/Design/PLAT/Platform.md` → "Shipping exclusion") and of DEVT's development services (`Zenith2/Docs/Design/DEVT/Console.md` → "Shipping absence of development services").
- In a Shipping build (`Shipping`, and `ShippingTest`, where `Zenith_IsShippingBuild()` is also true, `Zenith2/Docs/Architecture/BuildSystem.md` [Z2-N-ARCH-231]) every `user:/devtools/…` input fails R10 with `Zenith_PathError{ PATH_ERROR_USER_NAMESPACE, 6 }`: the byte offset is that of the first component, which follows the 6-byte `user:/`. `Append` re-parses its result, so it refuses the same paths.
- The refusal changes no data format a Shipping build reads ([Z2-N-CONTRACT-445]): every writer under `user:/devtools/` is a development service absent from Shipping (`Zenith2/Docs/Design/DEVT/Console.md` → "Shipping absence of development services"), and DEVT registers its `user:/devtools/` crash attachments in non-Shipping builds only (`Zenith2/Docs/Design/DEVT/Crash.md` → "Attachments"). `crash` is accepted in Shipping because crash capture is not a development service.

<!-- assumption: pending ARCH -->
`Zenith2/Docs/Architecture/PlatformLayer.md` → "Mounts and precedence" still lists the user mount's top-level directories as `config/`, `saves/`, `cache/` and `logs/`; its rule row ("`user:/` paths start with an allowed top-level directory") needs no change. PLAT assumes Architecture's list gains `crash/` (every build) and `devtools/` (non-Shipping builds), as cross-area row SEC1 of `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` records.
<!-- /assumption -->

### Normalisation

There is no normalisation step. A path that passes R1–R10 is already canonical: it has one separator form, no dot components, no duplicate separators and no trailing separator, so `Zenith_ParseVirtualPath` never rewrites its input and two different accepted strings always name different paths. Unicode normalisation is not applied: comparison is by bytes. Content paths are produced by the cook, which rejects source asset names that are not in Unicode Normalisation Form C (`COOK_ERROR_NAME_NOT_NFC`, `Zenith2/Docs/Design/ASSET/Database.md` → "Scan algorithm"), so the same visible name always has the same bytes.

### Reserved device names

A component is reserved when the part of it before its first `.`, compared case-insensitively over ASCII letters, equals one of:

| Group | Names |
|---|---|
| devices | `CON`, `PRN`, `AUX`, `NUL`, `CONIN$`, `CONOUT$` |
| serial ports | `COM0` … `COM9`, and `COM¹`, `COM²`, `COM³` (superscript digits U+00B9, U+00B2, U+00B3, which Windows also treats as device names) |
| parallel ports | `LPT0` … `LPT9`, and `LPT¹`, `LPT²`, `LPT³` |

The rule applies on every platform, so a path accepted on Android is accepted on Windows.

## Mounts

### Mount kinds and roots

```cpp
enum Zenith_MountKind : uint8
{
	MOUNT_KIND_PACKAGE = 1,          // precedence 1
	MOUNT_KIND_PATCH,                // precedence 2
	MOUNT_KIND_LOOSE,                // precedence 3; non-Shipping builds only
	MOUNT_KIND_USER,                 // precedence 4; the only writable mount; never serves content:/
};
```

| Mount | Precedence | Writable | Windows sources | Android sources | Present in |
|---|---|---|---|---|---|
| `package` | 1 | no | the archives the mount manifest lists under `<install>/Content/` | the APK's assets (`mount-manifest.json` and archives in `assets/`), install-time Play Asset Delivery packs (in the APK splits, read through the same asset manager), fast-follow and on-demand packs once downloaded (directories in app storage) | every build (a Shipping build with no valid manifest stops at startup, exit code 3) |
| `patch` | 2 | no | `<install>/Patches/<version>/` archives, after "Patch mount verification" | the patch directory `<internal storage>/Patches/<version>/`, after "Patch mount verification" | every build |
| `loose` | 3 | no | `--assets-root <dir>`, else `${ZENITH_OUT}/Cooked/<Game>/windows/` | the directory pushed with `adb push` (`<external files dir>/Loose/`) when present | non-Shipping builds only |
| `user` | 4 | yes | `SDL_GetPrefPath("Zenith2", <Game>)` | the app's internal storage (`SDL_GetAndroidInternalStoragePath()`) | every build |

`<install>` is `Zenith_Platform::GetBasePath()`. The loose mount's `Zenith_MountSource` is created only when `Zenith_IsShippingBuild()` is false: the Shipping build's mount step skips the code path entirely, and `MOUNT_KIND_LOOSE` given to `AddSource` in a Shipping build returns `FILE_ERROR_MOUNT_NOT_ALLOWED` (`Zenith2/Docs/Architecture.md` → "Security architecture", "No debugger or remote tools in shipping").

### Mount sources

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_VirtualFileSystem.h

struct Zenith_SourceEntry
{
	uint64 m_ulSize = 0;            // bytes a reader receives (the stored bytes; see m_uCodec)
	uint64 m_ulUncompressedSize = 0;
	uint32 m_uCodec = 0;            // 0 = stored; otherwise the Zenith_CodecID the consumer decompresses with (ASSET)
	uint64 m_ulOpaque = 0;          // source-private (archive entry index, directory lookup cookie)
	bool m_bDirectory = false;
};

struct Zenith_SourceFile { uint64 m_aulOpaque[2] = {}; };   // a per-request open file of one source

/// @contract interface Zenith_MountSource
/// Ownership: Created by its factory (the directory and APK sources by ZenithPlatform, the directory source also through
///   the public Zenith_CreateDirectoryMountSource, the archive source by ASSET's Zenith_CreateArchiveMountSource in
///   ZenithAssets, PACK's container and install-tree sources by ZenithPackage), passed to
///   Zenith_VirtualFileSystem::AddSource as a Zenith_Owned and owned by the mount table from then on; a source that is
///   never mounted is owned by its creator, which keeps it alive until every ReadFromSource naming it has completed.
/// Thread affinity: ANY_THREAD. Lookup and Open may be called concurrently from the main thread and every I/O thread;
///   one Zenith_SourceFile is used by one thread at a time.
/// Invalidation: A Zenith_SourceEntry is valid while the source is mounted; a source is destroyed only after every
///   request that opened a file from it has completed (the mount table's snapshot reference count).
/// Units: Offsets and sizes in bytes.
/// Errors: Zenith_Result<…, Zenith_FileError>: FILE_ERROR_NOT_FOUND, FILE_ERROR_ESCAPES_MOUNT, FILE_ERROR_CASE_MISMATCH,
///   FILE_ERROR_ARCHIVE_CORRUPT (an archive entry whose bounds fail the parser limits), FILE_ERROR_OS; never an assert.
/// Reentrancy: NOT_APPLICABLE. It invokes no callbacks except List's visitor, which may not call into the source.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_MountSource
{
public:
	virtual ~Zenith_MountSource() = default;
	virtual Zenith_StringView GetDescription() const = 0;   // "base.zpak", "loose:<dir>" for logs
	virtual Zenith_Result<Zenith_SourceEntry, Zenith_FileError> Lookup(Zenith_StringView strRelative) const = 0;
	virtual Zenith_Result<Zenith_SourceFile, Zenith_FileError> Open(const Zenith_SourceEntry& xEntry,
		Zenith_StringView strRelative) const = 0;
	virtual Zenith_Result<uint64, Zenith_FileError> Read(Zenith_SourceFile& xFile, uint64 ulOffset,
		Zenith_MutableByteSpan xDestination) const = 0;                   // bytes read; 0 at end of entry
	virtual void Close(Zenith_SourceFile& xFile) const = 0;
	virtual Zenith_Result<void, Zenith_FileError> List(Zenith_StringView strDirectory,
		Zenith_PfnDirectoryVisitor pfnVisitor, void* pUserData) const = 0;
	// The identity stamp of strRelative (§ "File identity stamps"), taken by path: the call opens its own handle, stamps it
	// and closes it. false means "no stamp: hash the file"; the default answers false, so an archive source, the APK source
	// and every test source never claim an identity. const like Lookup and Open, because every consumer holds a
	// const Zenith_MountSource&.
	virtual bool GetIdentityStamp(Zenith_StringView strRelative, Zenith_FileIdentityStamp& xOut) const
	{
		(void)strRelative;
		(void)xOut;
		return false;
	}
};

using Zenith_PfnDirectoryVisitor = void (*)(Zenith_StringView strName, const Zenith_SourceEntry& xEntry, void* pUserData);

/// @contract value Zenith_DirectorySourceDesc
/// Ownership: Plain value; the views are borrowed for the factory call and copied into the source.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None.
/// Units: None.
/// Errors: Validated by Zenith_CreateDirectoryMountSource.
/// Version: since Z2 Stage 5. Source API.
/// @endcontract
struct Zenith_DirectorySourceDesc
{
	Zenith_StringView m_strDescription;          // GetDescription's text ("pack:world_textures"); empty: "dir:<host root>"
	Zenith_Heap* m_pxHeap = nullptr;             // never null: the source, its root strings and its open-file records
	Zenith_FaultInjector* m_pxFaultInjector = nullptr;   // tests: the fault points of the escape check and the stamp
	uint32 m_uVolumeKindOverride = 0;           // tests only: 0 queries the OS; otherwise a Zenith_FileVolumeKind the stamp gate sees
};

// A read-only directory source over a host directory outside the four mount roots (ZenithPackage's delivered-pack
// locations, Windows DLC directories, the simulated delivery's source root; [Z2-N-ARCH-923]). strHostRoot is UTF-8, absolute,
// an existing directory; it is canonicalised once (GetFinalPathNameByHandleW, realpath) and every later open is checked
// against that canonical root by § "Escape check after resolution" and § "Case policy".
Zenith_Result<Zenith_Owned<Zenith_MountSource>, Zenith_FileError> Zenith_CreateDirectoryMountSource(
	Zenith_StringView strHostRoot, const Zenith_DirectorySourceDesc& xDesc);
```

`Zenith_CreateDirectoryMountSource` is the public factory of the same directory source the four mounts use (`Internal/Zenith_DirectoryMountSource.cpp`); it adds no second implementation:

| Step | Action | Error |
|---|---|---|
| 1 | `m_pxHeap` null, `strHostRoot` empty, longer than 4,096 bytes, not valid UTF-8 or holding a C0 control | `FILE_ERROR_INVALID_PATH` |
| 2 | the root is relative (Windows: no drive letter or `\\?\` prefix; POSIX: no leading `/`) | `FILE_ERROR_INVALID_PATH` |
| 3 | open the root as a directory (`CreateFileW` with `FILE_FLAG_BACKUP_SEMANTICS`; `open(O_RDONLY \| O_DIRECTORY \| O_CLOEXEC)`); absent | `FILE_ERROR_NOT_FOUND`; a file: `FILE_ERROR_NOT_A_DIRECTORY`; any other failure `FILE_ERROR_OS` |
| 4 | record the canonical root (the handle's final path) for the escape check; close the handle | `FILE_ERROR_OS` |
| 5 | allocate the source from `m_pxHeap` | `FILE_ERROR_OUT_OF_MEMORY` |

- The source is read-only by construction: it implements no write member, and `Zenith_AsyncIO`'s writes reach only the user mount.
- It may be passed to `AddSource` (any read-only mount kind; PACK mounts it as `MOUNT_KIND_PACKAGE` behind ASSET's archive source), read through `Zenith_AsyncIO::ReadFromSource` while unmounted (§ "Zenith_AsyncIO API"), or used directly by its owner; its thread affinity is the interface's (ANY_THREAD).
- `GetIdentityStamp` is implemented (§ "File identity stamps"); `List` and `Lookup` never follow a link out of the root.

| Source | Implementation | Mount kinds |
|---|---|---|
| directory | `Internal/Zenith_DirectoryMountSource.cpp` over `Internal/Windows/Zenith_HostFile_Windows.cpp`, `Internal/Android/Zenith_HostFile_Android.cpp`, `Internal/Linux/Zenith_HostFile_Linux.cpp`; the public factory `Zenith_CreateDirectoryMountSource` | loose, user, patch directories; through the factory, PACK's delivered-pack and Windows DLC directories (PACK wraps each in its container source and ASSET's archive source before `AddSource`) |
| APK assets | `Internal/Android/Zenith_ApkAssetMountSource.cpp` over `AAssetManager` (obtained once at mount time from the SDL activity through JNI) | package on Android |
| archive (`.zpak`) | the PACK archive reader in ZenithAssets (format and parser limits: `Zenith2/Docs/Design/PACK.md` (pending; its "Data structures and formats" section); `Zenith2/Docs/Architecture.md` → "Security architecture", "Parser limits") | package, patch |

### Resolution algorithm

`Zenith_VirtualFileSystem::Resolve(const Zenith_VirtualPath&) → Zenith_Result<Zenith_ResolvedPath, Zenith_PathError>`:

1. Take a snapshot of the mount table: under `uLOCK_LEVEL_FILESYSTEM_MOUNTS` (600, short-hold, `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", "Lock ordering table") copy the current table pointer and increment its reference count; release the lock.
2. `user:/` path: search the user mount's source alone. `content:/` path: search the mounts in the order loose, patch, package (highest precedence first), skipping absent mounts; the user mount is never searched.
3. Within a mount, search its sources in descending `order` (the manifest's), then descending registration order; the first `Lookup` that succeeds wins. `FILE_ERROR_NOT_FOUND` continues the search; any other error (`ESCAPES_MOUNT`, `CASE_MISMATCH`, `ARCHIVE_CORRUPT`, `OS`) stops it and is returned mapped to the path error of the same name (`PATH_ERROR_ESCAPES_MOUNT`, `PATH_ERROR_CASE_MISMATCH`), so a hostile entry in a higher mount is never silently bypassed.
4. Nothing found: `PATH_ERROR_NOT_FOUND`.
5. The result records the mount kind, the source index and description, and the entry; `Zenith_ResolvedPath` keeps the table snapshot referenced until it is destroyed.

```cpp
/// @contract type Zenith_ResolvedPath
/// Ownership: Move-only value holding a reference on the mount-table snapshot it was resolved in.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None while held; mount changes after resolution do not affect it.
/// Units: Sizes in bytes.
/// Errors: None.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_ResolvedPath
{
public:
	Zenith_MountKind GetMount() const;
	Zenith_StringView GetSourceDescription() const;
	const Zenith_SourceEntry& GetEntry() const;
};
```

The user mount never shadows content (rule of `Zenith2/Docs/Architecture/PlatformLayer.md` → "Mounts and precedence"): step 2 never consults it for `content:/`, and no API can mount a user-directory source into another mount.

### Escape check after resolution

Every OS path is formed inside `ZenithPlatform/Internal/` alone, from the mount root's OS path and the validated relative path (with `/` converted to the OS separator on Windows). Because a validated path cannot contain a parent reference, an escape can only come from a symbolic link, a junction or a mount point inside the mount root; the check therefore inspects the file the OS actually opened, never the name:

| Platform | Steps |
|---|---|
| Windows | `CreateFileW` on `\\?\<root>\<relative>` (`FILE_SHARE_READ \| FILE_SHARE_DELETE`, `FILE_FLAG_BACKUP_SEMANTICS` for directories); `GetFinalPathNameByHandleW(hFile, …, FILE_NAME_NORMALIZED \| VOLUME_NAME_DOS)`; the result must start with the mount root's own final path (resolved the same way once at mount time) followed by `\`, compared with `CompareStringOrdinal(…, TRUE)`; otherwise close the handle and fail `FILE_ERROR_ESCAPES_MOUNT` |
| Android, Linux | `open(<root>/<relative>, O_RDONLY \| O_CLOEXEC)`; `readlink("/proc/self/fd/<fd>")`; the result must start with the mount root's `realpath` followed by `/`; otherwise close and fail `FILE_ERROR_ESCAPES_MOUNT` |
| APK assets | not applicable: the asset manager has no links and no parent references |
| archives | not applicable: archive entries are names in a table of contents, validated by the archive reader as virtual-path components |

- The sequence is fixed and never uses the requested name after the open: open the file or directory; take the final path of the handle; run the prefix check; then run the case policy's byte comparison (§ "Case policy") on the same final path.
- Directory creation for a write creates one component at a time and runs the same sequence on each created directory's handle, so a link planted in the user directory cannot redirect a write outside it and an 8.3 alias cannot create through a name Android would reject.
- A link that stays inside its mount root is allowed.
- The check runs on every open, in every build; its cost is one syscall per open and is part of the pending new row "Async I/O critical read latency" (entry document → "Pending new rows").
- A hard link cannot be told from a regular file by path; the mount roots are directories the engine creates or ships, and planting a hard link needs write access to them, which the trust boundary already grants (`Zenith2/Docs/Architecture.md` → "Security architecture", "Trust boundaries"); this residual is accepted and documented in the escape-check source.

### Case policy

- Virtual paths are case-sensitive everywhere. The Android package and every archive are case-sensitive stores; Windows NTFS is case-insensitive.
- To make a path that works on Windows fail on Windows when it would fail on Android, the Windows directory source compares, after the escape check, the final path's components below the root with the requested components by bytes. The final path comes from `GetFinalPathNameByHandleW(FILE_NAME_NORMALIZED)`, which always returns long names, so an NTFS 8.3 short-name alias (`PROGRA~1`) is resolved to its long name before the comparison:
  - equal bytes: the open proceeds;
  - a difference only in ASCII or Unicode case: `FILE_ERROR_CASE_MISMATCH` (reported by `Resolve` as `PATH_ERROR_CASE_MISMATCH`);
  - any other difference (a short-name alias, or any other OS alias that names the same file by different bytes): `FILE_ERROR_NAME_MISMATCH` (`PATH_ERROR_NAME_MISMATCH`), so a path that opens only through an alias on Windows fails exactly as it fails on Android;
  - either failure logs both spellings.
- The user mount applies the same rule, so saves written on Windows reload on Android.

### File identity stamps

A file identity stamp lets a verifier skip re-hashing a file it already verified while the file is provably the same file with the same bytes ([Z2-N-ARCH-955], [Z2-N-ARCH-956]: "verified once, re-hashed when its stamps change"). PLAT owns the type, the directory source's implementation and the patch mount's use; PACK records stamps in `.zenith2-install/verified.zinv` and the delivery state file (`Zenith2/Docs/Design/PACK/Algorithms.md` → "Verified cache") and implements the same mapping in its install file system, so the two agree on one file.

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_FileIdentity.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Clock.h"

/// @contract value Zenith_FileIdentityStamp
/// Ownership: Plain value, 48 bytes, no padding.
/// Thread affinity: IMMUTABLE.
///   Members non-const: filled field by field from the OS query and copy-assigned into stamp records.
/// Invalidation: None.
/// Units: m_ulBytes in bytes; m_ulLastWriteNs and m_ulChangeNs in nanoseconds since 1970-01-01T00:00:00Z.
/// Errors: None; equality is member-wise over all five members (operator==).
/// Version: since Z2 Stage 5. Serialized by PACK in verified.zinv and delivery-state.zdls as these 48 bytes,
///   little-endian, in member order; a layout change is a format-version bump of both PACK files.
/// @endcontract
struct Zenith_FileIdentityStamp
{
	uint64 m_ulBytes = 0;
	uint64 m_ulLastWriteNs = 0;
	uint64 m_ulChangeNs = 0;
	uint64 m_ulVolume = 0;              // Windows: the 64-bit volume serial number; POSIX: st_dev
	uint8 m_auFileID[16] = {};          // Windows: FILE_ID_128; POSIX: st_ino in bytes 0–7 (little-endian), bytes 8–15 zero
	friend constexpr bool operator==(const Zenith_FileIdentityStamp&, const Zenith_FileIdentityStamp&) = default;
};
static_assert(sizeof(Zenith_FileIdentityStamp) == 48);

/// @contract enum Zenith_FileVolumeKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 5. Values keep their meaning once shipped; PACK's Zenith_InstallVolumeKind maps one to one.
/// @endcontract
enum Zenith_FileVolumeKind : uint8
{
	FILE_VOLUME_KIND_OTHER = 1,          // anything not listed below: never stamped
	FILE_VOLUME_KIND_NTFS,
	FILE_VOLUME_KIND_REFS,
	FILE_VOLUME_KIND_EXT4,
	FILE_VOLUME_KIND_F2FS,
	FILE_VOLUME_KIND_XFS,
	FILE_VOLUME_KIND_BTRFS,
	FILE_VOLUME_KIND_TMPFS,
};

inline constexpr uint64 ulFILE_IDENTITY_RACY_WINDOW_NS = 100000000;   // 100 ms (the racy-timestamp rule)

// The current time on the stamps' scale: ZenithCore's UTC clock (Zenith_GetUtcTime, microseconds since the Unix epoch)
// times 1,000. ZenithPlatform reads no OS clock of its own (Zenith2/Docs/Design/CORE/Diagnostics.md → "Clocks"); the
// microsecond resolution is far below the 100 ms window. ANY_THREAD; never fails.
inline uint64 Zenith_GetCurrentFileTimeNs()
{
	return static_cast<uint64>(Zenith_GetUtcTime().m_iUnixMicroseconds) * 1000;
}

// True when xStamp may be compared or recorded: max(m_ulLastWriteNs, m_ulChangeNs) is at least
// ulFILE_IDENTITY_RACY_WINDOW_NS older than ulNowNs. A stamp in the future (clock skew) is racy.
constexpr bool Zenith_IsStampSettled(const Zenith_FileIdentityStamp& xStamp, uint64 ulNowNs)
{
	const uint64 ulNewest = xStamp.m_ulLastWriteNs > xStamp.m_ulChangeNs ? xStamp.m_ulLastWriteNs : xStamp.m_ulChangeNs;
	return ulNewest <= ulNowNs && ulNowNs - ulNewest >= ulFILE_IDENTITY_RACY_WINDOW_NS;
}
```

The directory source's `GetIdentityStamp(strRelative, xOut)` (also through `Zenith_CreateDirectoryMountSource`):

1. Validate `strRelative` as `Lookup` does; any failure answers false.
2. Open the file for reading with the open sequence of § "Escape check after resolution" (the escape check and the case policy's comparison run on the handle); any failure answers false.
3. Volume gate: the volume kind of the handle's volume, queried once per source at its first stamp and cached (Windows `GetVolumeInformationByHandleW`'s file-system name: `NTFS` → NTFS, `ReFS` → REFS; Android and Linux `fstatfs`'s `f_type`: `0xEF53` → EXT4, `0xF2F52010` → F2FS, `0x58465342` → XFS, `0x9123683E` → BTRFS, `0x01021994` → TMPFS; anything else, or a failed query, → OTHER), or `m_uVolumeKindOverride` when non-zero. Supported: NTFS and REFS on Windows; EXT4 and F2FS on Android; EXT4, XFS, BTRFS and TMPFS on Linux. Any other kind (FUSE, sdcardfs, adoptable storage, FAT, a network share) closes the handle and answers false, because on those file systems the change time and the identity are synthesised.
4. Read the stamp from the open handle:

| Member | Windows | Android, Linux |
|---|---|---|
| `m_ulBytes` | `GetFileSizeEx` | `fstat` `st_size` |
| `m_ulLastWriteNs` | `GetFileInformationByHandleEx(FileBasicInfo)` `LastWriteTime` as `(t − 116,444,736,000,000,000) × 100` | `st_mtim.tv_sec × 10^9 + st_mtim.tv_nsec` |
| `m_ulChangeNs` | `FileBasicInfo.ChangeTime`, converted the same way | `st_ctim`, converted the same way |
| `m_ulVolume` | `GetFileInformationByHandleEx(FileIdInfo)` `VolumeSerialNumber` | `st_dev` |
| `m_auFileID` | `FileIdInfo.FileId` (16 bytes as stored) | `st_ino`, little-endian in bytes 0–7, bytes 8–15 zero |

5. Close the handle; any OS call that failed in step 4 answers false; otherwise true with `xOut` filled.

- A user process can set the last-write time (`SetFileTime`, `utimensat`) but not the change time or the file identity, so a rewrite that restores the last-write time still changes the stamp; a directory rename on the same volume changes neither of a contained file's identity nor its change time (NTFS keeps the file reference number; ext4, F2FS, XFS and Btrfs keep the inode), so PACK's stamp taken in `.incoming` stays valid after its commit rename.
- Android caveats, both accepted because each only costs a re-hash, which is safe: `st_dev` of `/data` is a device-mapper minor that can change across a reboot or an OTA, and an SELinux relabel, a `chmod`, an extended-attribute write or a link-count change advances `st_ctim` without changing a byte. Fast-follow and on-demand pack locations that Play places on adoptable storage, FUSE or sdcardfs fail the volume gate and are always hashed.
- The stamp is taken by path, so it brackets reads made through other opens: a verifier stamps a file immediately before its first read and again immediately after its last, and treats a difference as "changed during verification" (§ "Patch mount verification" step 4).
- SEC's confirmation of this stamp set, with its conditions C1–C4 (the bracket, the racy rule, the volume gate, records keyed by the verified manifest's digest), is `Zenith2/Docs/Design/SEC/TrustBoundaries.md` → "Stamp set confirmed"; every condition is a step of this section or of "Update manifest API".
- The racy-timestamp rule (git's): a stamp is compared with a record, or offered to be recorded, only when `Zenith_IsStampSettled(stamp, Zenith_GetCurrentFileTimeNs())`; an unsettled file is hashed and reported unsettled, so a write landing in the same timestamp tick as the file's previous change can never leave two equal stamps around different bytes.
- Each stamp is one open, two or three information calls and one close, measured inside PACK's pending row Z2-B-PACK-011 "Cold-boot update verification time" (`Zenith2/Docs/Design/PACK.md` → "Pending new rows").

### Zenith_VirtualFileSystem API

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_VirtualFileSystem.h

struct Zenith_BuildIdentity
{
	uint8 m_auBuildID[16] = {};                            // the executable's build identity (the ZRPL build ID)
	Zenith_StringView m_strReleaseLine;                     // "MAJOR.MINOR", from game.json version
	uint64 m_ulCompositionHash = 0;                        // Zenith_GameComposition::m_ulCompositionHash
	uint64 m_ulContentHash = 0;                            // the effective content hash: the mount manifest's contentHash,
	                                                        // replaced by the mounted patch's result content hash (offset 56 of
	                                                        // its update.zman) when the patch mount mounts one
	uint64 m_ulPackageContentHash = 0;                     // the mount manifest's contentHash, never changed by a patch: the base
	                                                        // package's identity (U11, PACK's CheckIdentity); 0 when no manifest
};

struct Zenith_VirtualFileSystemDesc
{
	Zenith_Platform* m_pxPlatform = nullptr;                // base and pref paths, the Android activity
	Zenith_StringView m_strGameName;
	Zenith_StringView m_strAssetsRootOverride;              // --assets-root; empty when absent
	Zenith_ByteSpan m_xUpdatePublicKey;                     // 32 bytes of the compiled-in Ed25519 key (Player data)
	Zenith_BuildIdentity m_xBuild;                          // the running build, for patch verification; Create fills both
	                                                        // content hashes and GetBuildIdentity returns the result
	const Zenith_UpdateStampLookup* m_pxUpdateStamps = nullptr;   // the patch mount's stamp records (PACK's verified.zinv,
	                                                        // Zenith_RuntimePackage_GetUpdateStampLookup); null: hash every file.
	                                                        // Read only inside Create, never stored (§ "Patch mount verification")
	uint32 m_uResolveCacheEntries = 16384;                 // vfs.resolveCacheEntries
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_FaultInjector* m_pxFaultInjector = nullptr;      // tests
};

/// @contract type Zenith_VirtualFileSystem
/// Ownership: Created by Zenith_VirtualFileSystem::Create in startup step 5 and owned by Zenith_Runtime; destroyed in
///   shutdown after Contracts step 10, after Zenith_Config (Zenith2/Docs/Design/PLAT.md "Threads PLAT owns and shutdown").
/// Thread affinity: ANY_THREAD for Resolve, Stat, Exists and List; ReadWhole and ReadRange are blocking and are
///   ANY_THREAD_EXCEPT job workers (a call from a job worker fails ZENITH_CHECK wherever ZENITH_ASSERTS_ENABLED=1,
///   through the blocking-wait check of Zenith2/Docs/Contracts.md → "Zenith_Multithreading"); AddSource and
///   RemoveSource are MAIN_THREAD in the frame prologue, in the closing action of startup step 11 (PACK's Windows DLC
///   mounts) and in shutdown step 4 (PACK's unmounts), and at no other point (MAIN_THREAD is checked; the admitted
///   points are the callers' contract, listed in "Mount changes outside the prologue" below); GetBuildIdentity is
///   ANY_THREAD (immutable after Create).
/// Invalidation: Views returned by List, ReadWhole and Stat live in the caller's arena.
/// Units: Sizes and offsets in bytes.
/// Errors: Zenith_Result<…, Zenith_PathError> for Resolve and Zenith_Result<…, Zenith_FileError> for every other member.
/// Reentrancy: NOT_APPLICABLE except List, whose visitor may not call List or AddSource.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_VirtualFileSystem
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_VirtualFileSystem>, Zenith_FileError> Create(
		const Zenith_VirtualFileSystemDesc& xDesc);
	Zenith_Result<Zenith_ResolvedPath, Zenith_PathError> Resolve(const Zenith_VirtualPath& xPath) const;
	Zenith_Result<Zenith_FileStat, Zenith_FileError> Stat(const Zenith_VirtualPath& xPath) const;
	bool Exists(const Zenith_VirtualPath& xPath) const;
	Zenith_Result<Zenith_Span<const Zenith_DirectoryEntry>, Zenith_FileError> List(const Zenith_VirtualPath& xDirectory,
		Zenith_Arena& xArena) const;                // union over mounts for content:/, sorted, highest precedence per name
	Zenith_Result<Zenith_ByteSpan, Zenith_FileError> ReadWhole(const Zenith_VirtualPath& xPath, Zenith_Arena& xArena,
		uint64 ulMaxBytes) const;                  // ulMaxBytes at most vfs.syncReadMaxBytes (64 MiB)
	Zenith_Result<uint64, Zenith_FileError> ReadRange(const Zenith_VirtualPath& xPath, uint64 ulOffset,
		Zenith_MutableByteSpan xDestination) const;
	Zenith_Result<Zenith_MountSourceID, Zenith_FileError> AddSource(Zenith_MountKind eMount,
		Zenith_Owned<Zenith_MountSource> xSource, sint32 iOrder);
	Zenith_Result<void, Zenith_FileError> RemoveSource(Zenith_MountSourceID xSource);
	const Zenith_BuildIdentity& GetBuildIdentity() const;   // both content hashes as Create resolved them
	Zenith_StringView GetMountRootHostPath(Zenith_MountKind eMount) const;   // development diagnostics
	Zenith_Span<const Zenith_MountInfo> DescribeMounts(Zenith_Arena& xArena) const;   // the dump and the overlay
};

struct Zenith_MountSourceID
{
	uint32 m_uIndex = 0;                  // slot in the mount's source list, plus 1; 0 is null
	uint32 m_uGeneration = 0;             // incremented when the slot is reused, so a removed source's ID is stale
	constexpr bool IsNull() const { return m_uIndex == 0; }
};

struct Zenith_MountInfo                  // one row per source, highest precedence first
{
	Zenith_MountKind m_eMount;
	Zenith_MountSourceID m_xSource;
	sint32 m_iOrder;
	Zenith_StringView m_strDescription;    // the source's GetDescription (an archive name, a directory host path)
	uint64 m_ulEntryCount;                // 0 when the source cannot count cheaply (a directory)
};

struct Zenith_FileStat { uint64 m_ulSize; uint64 m_ulStoredSize; uint32 m_uCodec; bool m_bDirectory; Zenith_MountKind m_eMount; };
struct Zenith_DirectoryEntry { Zenith_StringView m_strName; bool m_bDirectory; uint64 m_ulSize; Zenith_MountKind m_eMount; };
```

- Mount changes outside the prologue: `AddSource` and `RemoveSource` are also admitted in the closing action of startup step 11 (PACK's Windows DLC mounting, `Zenith2/Docs/Design/PACK/Algorithms.md` → "Windows DLC mounting") and in shutdown step 4 (PACK's unmount of every delivered pack and Windows DLC, → "Shutdown drain"); the snapshot reference count makes each as safe as a prologue change, because no frame runs at either point and every in-flight read keeps the table it resolved against. No other caller changes mounts after `Create`: fast-follow and on-demand asset packs are mounted and unmounted by PACK's installer alone, which verifies them itself (§ "Mount manifest schema").
- `AddSource` and `RemoveSource` build a new mount table, swap it in under the mount lock, and release the previous table when its snapshot reference count reaches zero; an in-flight request keeps reading from the table it resolved against, so a mount change never tears a read.
- The resolution cache maps a `content:/` path to its resolution (mount, source, entry) with `vfs.resolveCacheEntries` entries in a clock-replacement table. It caches only successful resolutions to the package and patch mounts, whose contents change only through `AddSource` and `RemoveSource`; it is cleared on every such mount change. A negative result (`PATH_ERROR_NOT_FOUND`) is never cached, and a resolution to the loose mount is never cached, because the developer rewrites that directory while the game runs and no watcher is guaranteed to exist; so the cache never changes a result, only its cost.

## The mount manifest

### Mount manifest schema

The package mount is built from the install's mount manifest, which PACK's packager writes (and the cook writes into each cooked output for development runs) and PLAT reads. Location: `<install>/Content/mount-manifest.json` on Windows, the APK asset `mount-manifest.json` on Android, `${ZENITH_OUT}/Cooked/<Game>/<platform>/mount-manifest.json` for development cooks.

```json
{
	"schema": "zenith2.mount-manifest",
	"schemaVersion": 1,
	"game": "OpenWorldSlice",
	"buildId": "5d1f0a3c9e7b22485d1f0a3c9e7b2248",
	"releaseLine": "1.0",
	"contentHash": "a3c9e7b22485d1f0",
	"package": [
		{ "archive": "base.zpak", "order": 0 },
		{ "archive": "world.zpak", "order": 10 }
	],
	"assetPacks": [
		{ "name": "world_textures", "delivery": "installTime", "archives": ["world_textures.zpak"], "order": 20 },
		{ "name": "cinematics", "delivery": "onDemand", "archives": ["cinematics.zpak"], "order": 30 }
	],
	"patchRoot": "Patches"
}
```

| Key | Type | Rule |
|---|---|---|
| `schema`, `schemaVersion` | string, integer | `zenith2.mount-manifest`, `1` |
| `game` | string | equals the running game's name |
| `buildId` | 32 lowercase hex digits | the build the content was packaged for; must equal the running build's ID in Shipping builds |
| `releaseLine` | string `MAJOR.MINOR` | the release line patches must target |
| `contentHash` | 16 lowercase hex digits | the cooked-content hash, recorded in replays and the resume state |
| `package[]` | objects | `archive` a file name (one virtual-path component, validated by R2–R9) relative to the manifest's directory; `order` an integer −1,000,000 to 1,000,000 |
| `assetPacks[]` | objects (Android) | `name` `^[a-z][a-z0-9_]{0,63}$`; `delivery` `installTime`, `fastFollow` or `onDemand`; `archives` as `package`; `order` |
| `patchRoot` | string | a single directory name under `<install>` (Windows) or internal storage (Android) |

- The manifest is untrusted input: parsed under the JSON limits and a 1 MiB cap; any rule failure is `FILE_ERROR_MANIFEST_INVALID` with the key path logged; in Shipping builds an invalid or absent manifest stops startup with exit code 3, and in development builds the package mount is left empty with a warning (the loose mount serves development content).
- Install-time asset packs are mounted with the package. Fast-follow and on-demand packs are not mounted by PLAT: PACK's `Zenith_AssetPackInstaller` owns their installation, verification and mounting (`Zenith2/Docs/Design/PACK/Algorithms.md` → "Verification pipeline", "Mounting and unmounting"). It verifies each delivered pack against the base `package.zpkm` through a directory source from `Zenith_CreateDirectoryMountSource` read with `Zenith_AsyncIO::ReadFromSource` before the pack is mounted ([Z2-N-ARCH-956]), then adds each archive with `AddSource(MOUNT_KIND_PACKAGE, …, <unit order>)` in the frame prologue and removes it with `RemoveSource`. PLAT reads `assetPacks[]` only to validate it and to mount the `installTime` entries; it keeps no per-pack state and exposes no asset-pack mount call.

### Update manifest API

`update.zman` (a patch) and `dlc.zman` (a Windows DLC) share PACK's layout (`Zenith2/Docs/Design/PACK/Formats.md` → "Update manifest header", "Update manifest tables and signature"). PLAT owns the verifier under SEC's rules, declared publicly so PACK's applier, PACK's Windows DLC mounting and ZenithCook's `--package-verify` run exactly the checks the patch mount runs. The checks are named as in `Zenith2/Docs/Design/PACK/Formats.md` → "Update manifest checks": PLAT runs U1–U5, U7, U8, U9 and, in the patch mount only, U11; U0, U6 and U10 are PACK's.

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_UpdateManifest.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_Sha256.h"
#include "ZenithPlatform/Zenith_FileIdentity.h"
#include "ZenithPlatform/Zenith_VirtualFileSystem.h"   // Zenith_MountSource, Zenith_BuildIdentity

enum Zenith_UpdateManifestKind : uint16 { UPDATE_MANIFEST_KIND_PATCH = 1, UPDATE_MANIFEST_KIND_DLC = 2 };

/// @contract enum Zenith_UpdateVerifyError
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 5. Values keep their meaning once shipped; new values are appended.
/// @endcontract
enum Zenith_UpdateVerifyError : uint8
{
	UPDATE_VERIFY_ERROR_FORMAT = 1,          // U1, U2, U7: size, magic, version, kind, reserved fields, totals, strings, paths, flags
	UPDATE_VERIFY_ERROR_SIGNATURE,           // U3: the Ed25519 signature does not verify against the key
	UPDATE_VERIFY_ERROR_RELEASE_LINE,        // U4
	UPDATE_VERIFY_ERROR_TARGET_BUILD,        // U5: PATCH for another build; DLC with a non-zero target
	UPDATE_VERIFY_ERROR_DIRECTORY_NAME,      // U8: PATCH version or directory name; DLC name against the directory
	UPDATE_VERIFY_ERROR_BASE_CONTENT,        // U11 in the patch mount: the base content hash differs from the package's, or is unknown
	UPDATE_VERIFY_ERROR_FILE_MISSING,        // U9: a listed file is absent from the source, or is a directory
	UPDATE_VERIFY_ERROR_FILE_SIZE,           // U9: a listed file's size differs
	UPDATE_VERIFY_ERROR_FILE_HASH,           // U9: a listed file's SHA-256 differs
	UPDATE_VERIFY_ERROR_FILE_CHANGED,        // U9: the file's stamp after its last read differs from the one before its first
	UPDATE_VERIFY_ERROR_IO,                  // a source Lookup, Open or Read failed with FILE_ERROR_OS or FILE_ERROR_ARCHIVE_CORRUPT
	UPDATE_VERIFY_ERROR_INVALID_ARGUMENT,    // a key or digest that is not 32 bytes, or a scratch buffer below 65,536 bytes
	UPDATE_VERIFY_ERROR_KIND,                // a DLC manifest read by the patch mount
};
constexpr const char* Zenith_ToString(Zenith_UpdateVerifyError eError);

struct Zenith_UpdateManifestFileEntry           // decoded from one 64-byte file-table entry; views into the caller's bytes
{
	Zenith_StringView m_strName;                // one path component, rules R2–R9
	uint64 m_ulBytes = 0;
	Zenith_Sha256Value m_xSha256;
	bool m_bArchive = false;                    // flag bit 0: added as a source, in file-table order
};

/// @contract value Zenith_UpdateManifestInfo
/// Ownership: Plain value of views into the manifest bytes given to Zenith_DecodeUpdateManifest; no allocation.
/// Thread affinity: IMMUTABLE. Members non-const: the manifest parser fills it field by field.
/// Invalidation: Every view (the name, the two tables, GetFile's names) is valid while those bytes are.
/// Units: Byte counts; versions as decimal integers.
/// Errors: Only Zenith_DecodeUpdateManifest produces one, and only after U1–U3 and U7 pass.
/// Version: since Z2 Stage 5. Mirrors format version 1 of the update manifest.
/// @endcontract
struct Zenith_UpdateManifestInfo
{
	Zenith_UpdateManifestKind m_eKind = UPDATE_MANIFEST_KIND_PATCH;
	uint16 m_uFormatVersion = 0;
	uint32 m_uFileCount = 0;
	uint8 m_auTargetBuildID[16] = {};
	uint16 m_uReleaseMajor = 0;
	uint16 m_uReleaseMinor = 0;
	uint16 m_uVersionMajor = 0;
	uint16 m_uVersionMinor = 0;
	uint16 m_uVersionPatch = 0;                // PATCH: the patch version; DLC: the content revision
	uint32 m_uEngineCookVersion = 0;
	uint64 m_ulBaseContentHash = 0;
	uint64 m_ulResultContentHash = 0;
	Zenith_StringView m_strName;                // DLC; empty for PATCH
	sint32 m_iMountOrder = 0;
	uint64 m_ulTotalBytes = 0;
	Zenith_ByteSpan m_xFileTable;               // 64 × m_uFileCount bytes
	Zenith_ByteSpan m_xStringTable;
	Zenith_UpdateManifestFileEntry GetFile(uint32 uIndex) const;   // uIndex < m_uFileCount, else ZENITH_ASSERT
};

/// @contract interface Zenith_UpdateStampLookup
/// Ownership: Implemented above ZenithPlatform (PACK's Zenith_VerifiedStampFile over verified.zinv); borrowed by
///   Zenith_VerifyUpdateManifestFiles for one call and by Zenith_VirtualFileSystem::Create for the patch mount.
/// Thread affinity: ANY_THREAD; FindStamp allocates nothing and takes no lock.
/// Invalidation: None while borrowed.
/// Units: Times in ns since the Unix epoch.
/// Errors: false means "no record": the file is hashed.
/// Reentrancy: FindStamp calls nothing back.
/// Version: since Z2 Stage 5. Source API.
/// @endcontract
class Zenith_UpdateStampLookup
{
public:
	virtual ~Zenith_UpdateStampLookup() = default;
	// strDirectory is install-relative ("Patches/1.2.3", "DLC/<name>"); xManifestSha256 the 32-byte SHA-256 of the manifest's
	// bytes; true with the recorded stamp when that directory is recorded with exactly that digest and lists strFile.
	virtual bool FindStamp(Zenith_StringView strDirectory, Zenith_ByteSpan xManifestSha256, Zenith_StringView strFile,
		Zenith_FileIdentityStamp& xOut) const = 0;
};

struct Zenith_UpdateFileCheckReport
{
	uint32 m_uFilesHashed = 0;
	uint32 m_uFilesSkipped = 0;                // a settled stamp equal to its record: never read
	uint32 m_uFilesUnsettled = 0;              // a stamp within the 100 ms racy window: hashed, never skipped
	uint64 m_ulBytesHashed = 0;
	uint32 m_uFailedFileIndex = 0xFFFFFFFFu;   // the file-table index of the failure; 0xFFFFFFFF when none
};

// U1, U2, U3 (before any further field is interpreted), U7. Succeeds for a manifest of another build or release line.
Zenith_Result<Zenith_UpdateManifestInfo, Zenith_UpdateVerifyError> Zenith_DecodeUpdateManifest(Zenith_ByteSpan xBytes,
	Zenith_ByteSpan xPublicKey);
// U4, U5, U8 against the running build and the name of the directory the manifest was read from.
Zenith_Result<void, Zenith_UpdateVerifyError> Zenith_CheckUpdateManifestTarget(const Zenith_UpdateManifestInfo& xInfo,
	const Zenith_BuildIdentity& xBuild, Zenith_StringView strDirectoryName);
// U9 over xSource (a bundle or a committed directory), in file-table order, stopping at the first failure. pxLookup null:
// hash every listed file (strDirectory and xManifestSha256 are then unused); strDirectory is the install-relative lookup
// key ("Patches/1.2.3", "DLC/<name>"); xManifestSha256 is the 32-byte SHA-256 of the manifest bytes just decoded;
// xScratch is the caller's read buffer, at least 65,536 bytes, so the call allocates nothing; the skip count is
// xReportOut.m_uFilesSkipped.
Zenith_Result<void, Zenith_UpdateVerifyError> Zenith_VerifyUpdateManifestFiles(const Zenith_UpdateManifestInfo& xInfo,
	const Zenith_MountSource& xSource, const Zenith_UpdateStampLookup* pxLookup, Zenith_StringView strDirectory,
	Zenith_ByteSpan xManifestSha256, Zenith_MutableByteSpan xScratch, Zenith_UpdateFileCheckReport& xReportOut);
// The three halves in order with a null lookup (every file hashed); xInfoOut is filled when the decode succeeded,
// whatever the later result.
Zenith_Result<void, Zenith_UpdateVerifyError> Zenith_VerifyUpdateManifest(Zenith_ByteSpan xBytes, Zenith_ByteSpan xPublicKey,
	const Zenith_BuildIdentity& xBuild, Zenith_StringView strDirectoryName, const Zenith_MountSource& xSource,
	Zenith_MutableByteSpan xScratch, Zenith_UpdateManifestInfo& xInfoOut, Zenith_UpdateFileCheckReport& xReportOut);
```

- Every function is ANY_THREAD, allocates nothing, and never asserts on file bytes, because the manifest and the files are untrusted (`Zenith2/Docs/Architecture.md` → "Security architecture", "Parser limits").
- `Zenith_VirtualFileSystem.h` includes `Zenith_FileIdentity.h` (for `Zenith_MountSource::GetIdentityStamp`) and forward-declares `class Zenith_UpdateStampLookup;` (for `Zenith_VirtualFileSystemDesc::m_pxUpdateStamps`). The two headers therefore include in one direction only.

`Zenith_DecodeUpdateManifest` runs these steps:

1. `xPublicKey` is not 32 bytes: `INVALID_ARGUMENT`.
2. U1. The input must be at least 225 bytes, with magic `ZMAN`, format version 1, kind 1 or 2, and every reserved header field zero. Otherwise `FORMAT`.
3. U2. Total bytes (offset 80) must equal the length of `xBytes`. F must be 1 … 65,536 and S at most 1,048,576. `96 + 64F + S + 64` must equal the total, with every addition overflow-checked in `uint64`. Otherwise `FORMAT`.
4. U3. Call SEC's `Zenith_Ed25519Verify(bytes [0, total − 64), the last 64 bytes, xPublicKey)` (`Internal/Security/Zenith_Ed25519.h`, SEC's file in ZenithPlatform; pure Ed25519, RFC 8032, no context, no pre-hash; `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Zenith_Ed25519 header"). False is `SIGNATURE`. No later field is read before this check passes. This is SEC's rule V3.
5. U7. Every check below must hold; any failure is `FORMAT`.
   - **Every entry:**
     - the path offset and length lie inside the string table;
     - the length is 1 … 255;
     - the bytes pass rules R2–R9 of § "Validation rules and order" as one component;
     - names are strictly increasing in byte order, so sorted and unique;
     - the flags have no bit other than 0, and bit 0 is set exactly when the name ends `.zpak`;
     - the bytes are at most 64 GiB;
     - the reserved bytes are zero.
   - **DLC:**
     - the name lies in the string table and is 1 … 64 bytes;
     - it obeys PACK's DLC name rule `^[a-z][a-z0-9_]{0,63}$`;
     - the mount order is 1,000 … 1,000,000;
     - the version patch (the revision) is 1 … 65,535.
   - **PATCH:**
     - the name offset, the name length and the mount order are zero;
     - the version patch is 1 … 65,535.
6. Return the info.

`Zenith_CheckUpdateManifestTarget` checks in this order:

1. **U4** (`RELEASE_LINE`). The release line, formatted `<major>.<minor>` in decimal without leading zeros, equals `xBuild.m_strReleaseLine` byte for byte.
2. **U5** (`TARGET_BUILD`). PATCH: the target build ID equals `xBuild.m_auBuildID`. DLC: all 16 bytes are zero.
3. **U8** (`DIRECTORY_NAME`). PATCH: version major and minor equal the release major and minor, and `strDirectoryName` equals `<major>.<minor>.<patch>` in decimal without leading zeros. DLC: `strDirectoryName` equals the name.

`Zenith_VerifyUpdateManifestFiles` first checks its arguments, then checks each entry in file-table order:

1. **Arguments.** `xScratch` below 65,536 bytes, or a non-null `pxLookup` with an `xManifestSha256` that is not 32 bytes, is `INVALID_ARGUMENT` before any I/O.
2. **Lookup.** Call `xSource.Lookup(name)`.
   - `FILE_ERROR_NOT_FOUND`, or a directory entry: `FILE_MISSING`.
   - Any other error: `IO`.
   - A size other than the entry's bytes: `FILE_SIZE`.
3. **First stamp.** Call `xSource.GetIdentityStamp(name, xFirst)`. False means the file has no stamp.
4. **Skip.** The file is skipped when all of these hold:
   - a lookup is given;
   - the file has a stamp;
   - `Zenith_IsStampSettled(xFirst, Zenith_GetCurrentFileTimeNs())`;
   - `pxLookup->FindStamp(strDirectory, xManifestSha256, name, xRecord)` returns true;
   - `xRecord == xFirst`, in all five members, the size included.

   A skipped file is counted in `m_uFilesSkipped`, and the check continues with the next entry without reading it. An unsettled stamp is counted in `m_uFilesUnsettled` and the file is hashed.
5. **Hash.** `Open`, then `Read` into `xScratch` until the entry's bytes are consumed, feeding CORE's `Zenith_Sha256Stream`, then `Close`.
   - A read error: `IO`.
   - An early end of entry: `FILE_SIZE`.
   - The digest compares unequal with the entry's `Zenith_Sha256Value`: `FILE_HASH`.
6. **Second stamp**, when the first existed. Call `GetIdentityStamp` again. False, or a stamp unequal to `xFirst`, is `FILE_CHANGED` (the bracket of § "File identity stamps").
7. **Count.** Add the file to `m_uFilesHashed` and its bytes to `m_ulBytesHashed`.

On any failure, `m_uFailedFileIndex` names the entry.

Cost:
- each skipped file costs one `Lookup`, one stamp and one `FindStamp`;
- every other file costs one full read and two stamps;
- the total is O(total bytes of the files hashed).

`Zenith_VerifyUpdateManifest` runs decode, target and files, in that order, and returns the first failure. The callers map the errors:
- PACK maps them as `Zenith2/Docs/Design/PACK/Formats.md` → "Update manifest checks" states; its B6 maps `FILE_CHANGED` to `UPDATE_INCOMPLETE` and leaves the bundle in place;
- the patch mount and the DLC mount treat `FILE_CHANGED` as a mismatch.

The implementation:
- `Zenith_DecodeUpdateManifest`, `Zenith_CheckUpdateManifestTarget` and `Zenith_VerifyUpdateManifestFiles` are implemented in `Zenith2/Engine/ZenithPlatform/Internal/Security/Zenith_UpdateManifestVerifier.cpp`;
- U3 calls SEC's `Zenith_Ed25519Verify`. SEC owns `Internal/Security/Zenith_Ed25519.h` and `.cpp`, the only files that include the Ed25519 implementation the selection ruling admits to the third-party manifest (`Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Ed25519 implementation selection"; `Zenith2/Docs/Architecture.md` → "Third-party dependencies"); PLAT's verifier includes SEC's internal header and nothing of the library;
- the rules are the SEC area's, V1 to V9 in SEC's order (`Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Rule order"); the U-names here are PACK's names for the same checks.

### Patch mount verification

`Zenith_VirtualFileSystem::Create` builds the patch mount after the package mount, in startup step 5. Patches are installed only before the virtual filesystem exists, by PACK's applier in the closing action of startup step 4 ([Z2-N-ARCH-921]; `Zenith2/Docs/Design/PACK/Algorithms.md` → "Pending installs at startup"). A patch delivered while the game runs mounts at the next start, so the patch mount changes only inside `Create`.

1. `m_xBuild.m_ulPackageContentHash` is the package mount's manifest `contentHash`, or 0 when the package mount has no valid manifest. `m_xBuild.m_ulContentHash` starts equal to it.
2. List the directories of `<install>/<patchRoot>/`; on Android, list the internal storage root's `<patchRoot>/` instead.
   - Names beginning with `.` (`.incoming`, `.trash`) are skipped silently.
   - Every other name must parse as `MAJOR.MINOR.PATCH` in decimal, each part 0 … 65,535 without leading zeros. Other names are ignored with `patch ignored: <name>: not a version`.
   - A name of a release line other than `m_xBuild.m_strReleaseLine` is skipped; PACK's sweep removes it.
3. Take the candidates in descending version order. For each candidate `v`, work through a directory source over `<install>/<patchRoot>/<v>/`, created by the internal factory behind `Zenith_CreateDirectoryMountSource`:
   1. `ReadWhole` `update.zman`, at most 5,243,040 bytes (`96 + 64 × 65,536 + 1,048,576 + 64`). An absent or larger file rejects the candidate.
   2. Take the SHA-256 of those bytes (`Zenith_Sha256Digest`); it keys the stamp records.
   3. Call `Zenith_DecodeUpdateManifest(bytes, m_xUpdatePublicKey)`. A DLC kind is `KIND`.
   4. Call `Zenith_CheckUpdateManifestTarget(info, m_xBuild, "<v>")` (U4, U5, U8).
   5. U11: `m_ulPackageContentHash` must be non-zero and equal the manifest's base content hash; otherwise `BASE_CONTENT`.
   6. U9: call `Zenith_VerifyUpdateManifestFiles(info, source, m_pxUpdateStamps, "<patchRoot>/<v>", digest, a 1 MiB scratch from m_pxHeap, report)`.
      - Its skips are added to `vfs.patch.stamp_skips`.
      - Its hashed files and bytes are added to `vfs.patch.files_hashed` and `vfs.patch.bytes_hashed`.
      - `FILE_CHANGED` rejects the candidate like any other mismatch.
   7. Mount. For each entry with `m_bArchive`, in file-table order, call the archive factory over the candidate's directory source; the resulting source joins the patch mount. A factory error removes the archives already added and rejects the candidate.
4. The first candidate that passes every step is mounted, and `m_xBuild.m_ulContentHash` becomes its result content hash.
   - Every higher candidate that failed is logged once as `patch rejected: <version>: <rule>`, with SEC's rule tag (for example `V3 (U3 signature)`; `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Rejection and logging"), or `<file error>` for a read or factory failure; no byte of the file is logged.
   - Each is counted in `vfs.patches_rejected`.
   - Nothing of a failed candidate is mounted.
5. No candidate passes: the patch mount has no source, and the package content stays in effect.
6. `m_pxUpdateStamps` is read only during these steps and is never stored. The runtime bridge destroys the lookup at shutdown step 6, long before the virtual filesystem, and no later member of `Zenith_VirtualFileSystem` reads it.
7. The signature and the targets (U1–U5, U7, U8, U11) run at every start. Only U9's hashing is skipped, per file, while the file's stamp is unchanged ([Z2-N-ARCH-955], [Z2-N-ARCH-956]).
8. The patch mount runs no U0 check. Files the directory holds beyond the listed ones are never mounted, because only listed archives become sources.

## Synchronous file access

- `ReadWhole`, `ReadRange`, `Stat`, `Exists` and `List` exist for startup (configuration, bindings, the mount manifest, the schema registry), tools, the I/O threads' own work and dedicated threads; frame work uses `Zenith_AsyncIO`.
- `ReadWhole` refuses entries larger than its `ulMaxBytes` argument (at most `vfs.syncReadMaxBytes`, 64 MiB) with `FILE_ERROR_TOO_LARGE` before allocating.
- A synchronous read from a job worker is a blocking wait in a job, a programmer error caught by the blocking-wait check wherever asserts are enabled (`Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", "Zenith_Multithreading").
- `List` of a `content:/` directory returns the union of every mount's entries for that directory, one entry per name from its highest-precedence mount, sorted by byte order.

## Command-line host files

Some command-line options name files on the host rather than virtual paths:
- `--replay`, `--record`, `--input-script` and `--config-dump` name one file each (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Command-line options"; [Z2-N-ARCH-923] admits exactly these four under a grant);
- the budget-run `--evidence-dir` names a directory into which the run writes its evidence files and from which it reads its two inputs `rows.json` and `fingerprint.json` (`Zenith2/Docs/Budgets.md` → "Measurement procedure catalogue", [Z2-N-BUDGET-105]);
- from Stage 3, `--script-migration-fixtures` names a read-only directory of a game's behaviour-state migration fixtures (`Zenith2/Docs/Design/MOD.md` → "The gametest format"; [Z2-N-SCRIPT-400]).

They are local operator input, not content, saves, configuration or network data.

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_HostFile.h
enum Zenith_HostFileMode : uint8 { HOST_FILE_MODE_READ = 1, HOST_FILE_MODE_WRITE_NEW };

// What a directory grant admits ([Z2-N-ARCH-923]); NONE for the four single-file grants.
enum Zenith_HostDirectoryAccess : uint8
{
	HOST_DIRECTORY_ACCESS_NONE = 0,          // a file grant
	HOST_DIRECTORY_ACCESS_EVIDENCE,          // --evidence-dir: create new files; read only rows.json and fingerprint.json
	HOST_DIRECTORY_ACCESS_READ_ONLY,         // --script-migration-fixtures: list and read, never write
};

inline constexpr uint64 ulHOST_DIRECTORY_READ_MAX_BYTES = 1048576;    // a file opened through a directory grant
inline constexpr uint32 uHOST_DIRECTORY_LIST_MAX_ENTRIES = 1024;      // regular files one listing returns
inline constexpr uint32 uHOST_DIRECTORY_LIST_MAX_DEPTH = 2;           // the directory and its immediate subdirectories

struct Zenith_HostDirectoryEntry
{
	Zenith_StringView m_strRelativePath;     // "Behaviour/v3.zbst": one or two components joined by '/'
	uint64 m_ulBytes = 0;
};

class Zenith_CommandLine;                 // ZenithRuntime's; forward-declared only, no runtime header is included

/// @contract type Zenith_CommandLineFileGrant
/// Ownership: Constructed only by Zenith_CommandLine (its friend), one per host-path option given, and owned by it for
///   the process lifetime; everyone else receives a const reference (Zenith_CommandLine::GetFileGrant).
/// Thread affinity: IMMUTABLE. Members non-const: Zenith_CommandLine assigns a parsed grant over its placeholder.
/// Invalidation: The path view is valid while main's argument strings are, that is for the process lifetime.
/// Units: None.
/// Errors: None; the open functions validate and canonicalise the path.
/// Version: since Z2 Stage 2. Source API; non-Shipping builds only (the parser rejects the options in Shipping).
/// @endcontract
class Zenith_CommandLineFileGrant
{
public:
	Zenith_StringView GetPath() const;   // the argument as given, a view into main's argv; canonicalised at open
	bool IsDirectory() const;            // GetDirectoryAccess() != HOST_DIRECTORY_ACCESS_NONE
	Zenith_HostDirectoryAccess GetDirectoryAccess() const;
	uint8 GetOptionTag() const;        // the runtime's Zenith_CommandLineFileOption value, for diagnostics only
	bool IsValid() const;
private:
	friend class Zenith_CommandLine;
	Zenith_CommandLineFileGrant(Zenith_StringView strPath, bool bDirectory, uint8 uOptionTag);   // true: EVIDENCE
	Zenith_CommandLineFileGrant(Zenith_StringView strPath, Zenith_HostDirectoryAccess eAccess, uint8 uOptionTag);
	Zenith_StringView m_strPath;
	Zenith_HostDirectoryAccess m_eDirectoryAccess = HOST_DIRECTORY_ACCESS_NONE;
	uint8 m_uOptionTag = 0;
};


/// @contract type Zenith_HostFile
/// Ownership: A move-only value returned by the two open functions below; closes its OS handle on destruction or Close.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Invalid after Close or a move.
/// Units: Bytes and byte offsets.
/// Errors: FILE_ERROR_ACCESS_DENIED, FILE_ERROR_SHORT_READ, FILE_ERROR_OS (the OS code logged); never asserts.
/// Version: since Z2 Stage 2. Source API; non-Shipping builds only (Shipping returns FILE_ERROR_MOUNT_NOT_ALLOWED from the opens).
/// @endcontract
class Zenith_HostFile
{
public:
	Zenith_HostFile(Zenith_HostFile&& xOther);
	Zenith_HostFile& operator=(Zenith_HostFile&& xOther);
	~Zenith_HostFile();
	Zenith_Result<uint64, Zenith_FileError> GetSize() const;
	Zenith_Result<uint64, Zenith_FileError> Read(uint64 ulOffset, Zenith_MutableByteSpan xDestination);
	Zenith_Result<void, Zenith_FileError> Append(Zenith_ByteSpan xBytes);     // HOST_FILE_MODE_WRITE_NEW only
	Zenith_Result<void, Zenith_FileError> Flush();
	void Close();
};

Zenith_Result<Zenith_HostFile, Zenith_FileError> Zenith_OpenCommandLineHostFile(const Zenith_CommandLineFileGrant& xGrant,
	Zenith_HostFileMode eMode);
// --evidence-dir: creates one new file directly inside the granted directory; strFileName is a single path component
// checked by rules R2, R4, R5, R6, R7 and R9 of "Validation rules and order" (no separator, no traversal, no reserved name).
Zenith_Result<Zenith_HostFile, Zenith_FileError> Zenith_CreateFileInCommandLineHostDirectory(
	const Zenith_CommandLineFileGrant& xDirectoryGrant, Zenith_StringView strFileName);
// Opens one existing regular file inside a directory grant for reading (HOST_FILE_MODE_READ), at most
// ulHOST_DIRECTORY_READ_MAX_BYTES; the rules per access kind are the table of "Directory-grant reads".
Zenith_Result<Zenith_HostFile, Zenith_FileError> Zenith_OpenFileInCommandLineHostDirectory(
	const Zenith_CommandLineFileGrant& xDirectoryGrant, Zenith_StringView strRelativePath);
// Lists the regular files of a HOST_DIRECTORY_ACCESS_READ_ONLY grant to depth 2, sorted by byte order of the relative
// path; the views live in xArena.
Zenith_Result<Zenith_Span<const Zenith_HostDirectoryEntry>, Zenith_FileError> Zenith_ListCommandLineHostDirectory(
	const Zenith_CommandLineFileGrant& xDirectoryGrant, Zenith_Arena& xArena);
```

- `Zenith_CommandLineFileGrant` is PLAT's type, declared here in `Zenith_HostFile.h` (`Zenith2/Docs/Architecture/PlatformLayer.md` → "Settled rules and ownership"); it is constructed only by `Zenith_CommandLine` (ZenithRuntime; `Zenith2/Docs/Design/MOD.md` → "Zenith_CommandLine.h" hands it out through `GetFileGrant`) for the argument of one of the options above, so no path from content, saves, configuration, scripts or the network can reach this function.
- The path is canonicalised by the OS (`GetFullPathNameW`, `realpath`), must be valid UTF-8 without control characters, and is opened with `HOST_FILE_MODE_READ` or created new (`CREATE_NEW`, `O_CREAT | O_EXCL`), never truncating an existing file.
- In Shipping builds both open functions return `FILE_ERROR_MOUNT_NOT_ALLOWED`, and those options are rejected by the command-line parser.
- A directory grant is created only for `--evidence-dir` (`HOST_DIRECTORY_ACCESS_EVIDENCE`, through the `bool` constructor, whose `true` means EVIDENCE) and `--script-migration-fixtures` (`HOST_DIRECTORY_ACCESS_READ_ONLY`, through the access constructor).
- The evidence directory is created by the runtime if absent. `Zenith_CreateFileInCommandLineHostDirectory` refuses a name that already exists (`CREATE_NEW`, `O_CREAT | O_EXCL`), and refuses a READ_ONLY grant with `FILE_ERROR_ACCESS_DENIED` before touching the host.

### Directory-grant reads

`Zenith_OpenFileInCommandLineHostDirectory` and `Zenith_ListCommandLineHostDirectory` are the directory-grant read functions of [Z2-N-ARCH-923]. Before any other check, both return:
- `FILE_ERROR_MOUNT_NOT_ALLOWED` in a Shipping build (`if constexpr (Zenith_IsShippingBuild())`);
- `FILE_ERROR_NOT_A_DIRECTORY` for a file grant or an invalid grant.

| Access | `Zenith_OpenFileInCommandLineHostDirectory` admits | `Zenith_ListCommandLineHostDirectory` |
|---|---|---|
| `HOST_DIRECTORY_ACCESS_EVIDENCE` | exactly the names `rows.json` and `fingerprint.json`, compared by bytes; any other name, including any name holding `/` or `\`, is `FILE_ERROR_ACCESS_DENIED` | `FILE_ERROR_ACCESS_DENIED` |
| `HOST_DIRECTORY_ACCESS_READ_ONLY` | a relative path of one or two components joined by one `/`, each passing rules R2, R4, R5, R6, R7, R8 and R9 of "Validation rules and order" (no `\`, no `..` or `.`, no empty component, no reserved name, at most 255 bytes), the whole at most 512 bytes; a failure is `FILE_ERROR_INVALID_PATH` | the listing below |

Open steps, after the table's name rule:
1. Canonicalise the grant's directory as `Zenith_OpenCommandLineHostFile` canonicalises a path (`GetFullPathNameW`, `realpath`). A missing directory is `FILE_ERROR_NOT_FOUND`.
2. Open `<directory>/<relative>` read-only (`CreateFileW` with `GENERIC_READ`, `FILE_SHARE_READ` and `OPEN_EXISTING`; `open(O_RDONLY | O_CLOEXEC)`). An absent file is `FILE_ERROR_NOT_FOUND`, a refusal `FILE_ERROR_ACCESS_DENIED`, any other failure `FILE_ERROR_OS`.
3. Run the escape check of "Escape check after resolution" on the handle against the canonical directory (`FILE_ERROR_ESCAPES_MOUNT`). Then run the case policy's byte comparison of the components below it (`FILE_ERROR_CASE_MISMATCH`, `FILE_ERROR_NAME_MISMATCH`), so a fixture that opens only through a case alias on Windows fails exactly as it does on Linux.
4. A file that is not a regular file is `FILE_ERROR_NOT_A_FILE`; one larger than `ulHOST_DIRECTORY_READ_MAX_BYTES` is `FILE_ERROR_TOO_LARGE`. Each failing check closes the handle.
5. Return the `Zenith_HostFile`. `Read` and `GetSize` work; `Append` and `Flush` return `FILE_ERROR_ACCESS_DENIED`.

Listing steps, for a READ_ONLY grant:
1. Canonicalise and open the directory as above.
2. Enumerate it (`FindFirstFileExW` and `FindNextFileW`; `opendir` and `readdir`), never following a link. A reparse point, a symbolic link or a junction is skipped and counted; it is neither listed nor descended into.
3. A regular file at depth 1 is listed as `<name>`. A subdirectory at depth 1 is enumerated once more, and its regular files are listed as `<subdirectory>/<name>`. Nothing deeper is visited.
4. A name that fails the component rules of the table is skipped, with one log line `host directory: skipped <name>: <rule>`.
5. More than `uHOST_DIRECTORY_LIST_MAX_ENTRIES` regular files is `FILE_ERROR_TOO_LARGE`, and nothing is returned. Each entry's size is the enumeration's own size field.
6. Sort the entries by byte order of the relative path and copy their strings into `xArena` (`FILE_ERROR_OUT_OF_MEMORY` when the arena is exhausted).

- [Z2-N-ARCH-923] admits one read-open of each of the two evidence inputs. That count is the budget-run driver's usage (`Zenith2/Docs/Design/DEVT/Evidence.md` → "Budget-run inputs"). PLAT restricts the names and the size, and keeps no per-process count, because a grant is immutable.
- MOD's migration-fixture run lists its grant and opens each `<Behaviour>/v<n>.zbst` it selects (at most 1,024 files of at most 1 MiB; `Zenith2/Docs/Design/MOD.md` → "The gametest format"). Both limits are the constants above, so a listing that passes never yields a file that the open refuses for size.

The four file options and the two directory grants are admitted by `Zenith2/Docs/Architecture/PlatformLayer.md` → "Path validation" ([Z2-N-ARCH-923]):
- the evidence directory receives files only through `Zenith_CreateFileInCommandLineHostDirectory` and is read only through `Zenith_OpenFileInCommandLineHostDirectory`;
- the migration-fixture directory is read only through `Zenith_ListCommandLineHostDirectory` and `Zenith_OpenFileInCommandLineHostDirectory`, and is never written;
- `Zenith2/Docs/Architecture/CompositionRoot.md` → "Command-line options" lists `--evidence-dir` with the grant `COMMAND_LINE_FILE_EVIDENCE_DIR` and `--script-migration-fixtures` with `COMMAND_LINE_FILE_SCRIPT_MIGRATION_FIXTURES`; `IsDirectory()` is true for both.

## Asynchronous I/O

### Zenith_AsyncIO API

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_AsyncIO.h

enum Zenith_IOPriority : uint8
{
	IO_PRIORITY_CRITICAL,      // blocks visible progress now (the startup scene, a streamed cell in view)
	IO_PRIORITY_HIGH,
	IO_PRIORITY_NORMAL,
	IO_PRIORITY_LOW,
	IO_PRIORITY_BACKGROUND,    // prefetch
	IO_PRIORITY_COUNT,
};

struct Zenith_IORequest
{
	Zenith_VirtualPath m_xPath;
	uint64 m_ulOffset = 0;
	uint64 m_ulBytes = 0;                        // 0 = to end of entry
	Zenith_MutableByteSpan m_xDestination;        // owned by the requester until completion
	Zenith_IOPriority m_ePriority = IO_PRIORITY_NORMAL;
	Zenith_IOCompletionQueue* m_pxCompletions = nullptr;   // optional: receives one record at completion
	uint64 m_ulUserTag = 0;                      // echoed in the completion record
};

struct Zenith_IOCompletion
{
	uint64 m_ulIssueID;                          // Zenith_OperationHandle::GetIssueID of the request
	uint64 m_ulUserTag;
	Zenith_OperationStatus m_eStatus;             // SUCCEEDED, FAILED or CANCELLED
	Zenith_IOError m_xError;                      // FAILED only; otherwise m_eCode is 0 (never an enumerator)
	uint64 m_ulBytesTransferred;
};

/// @contract container Zenith_IOCompletionQueue
/// Ownership: Owned by a consumer (the streamer, the save system, the input system); it must outlive every request
///   that names it.
/// Thread affinity: ANY_THREAD for the I/O threads' pushes; Drain is OWNER_THREAD.
/// Invalidation: The span returned by Drain is valid until the next Drain.
/// Units: None.
/// Errors: None: Zenith_AsyncIO reserves a slot when a request naming the queue is accepted and refuses the request
///   with IO_ERROR_QUEUE_FULL when no slot is free, so a completion is never lost and the queue never overflows.
/// Version: since Z2 Stage 2. Source API.
/// Alignment: alignof(Zenith_IOCompletion).
/// Overflow: capacity fixed at construction, at most 65,536.
/// Allocation failure: Create returns IO_ERROR_OUT_OF_MEMORY; nothing allocates afterwards.
/// Complexity: push and Drain O(1) per record; lock-free multi-producer single-consumer ring.
/// @endcontract
class Zenith_IOCompletionQueue
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_IOCompletionQueue>, Zenith_IOError> Create(uint32 uCapacity,
		Zenith_Heap& xHeap);
	Zenith_Span<const Zenith_IOCompletion> Drain(Zenith_Arena& xArena);   // in completion order
};

struct Zenith_AsyncIODesc
{
	uint32 m_uThreadCount = 2;                   // io.threadCount, 1 to 4
	uint32 m_uQueueCapacity = 1024;              // io.queueCapacity, per priority
	uint32 m_uChunkBytes = 1048576;              // io.chunkBytes, 64 KiB to 16 MiB, a power of two
	uint64 m_ulLowDiskMarginBytes = 67108864;    // io.lowDiskMarginBytes
	uint32 m_uLowDiskPollSeconds = 30;           // io.lowDiskPollSeconds
	uint32 m_uLowDiskClearFactor = 2;            // io.lowDiskClearFactor
	Zenith_VirtualFileSystem* m_pxFiles = nullptr;
	Zenith_Platform* m_pxPlatform = nullptr;      // low-disk events are queued to it
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_FreeSpaceSource* m_pxFreeSpace = nullptr;   // tests: a fake volume; nullptr = the OS query
	Zenith_FaultInjector* m_pxFaultInjector = nullptr; // tests: interrupt a write at a named step
};

/// @contract type Zenith_AsyncIO
/// Ownership: Created by Zenith_AsyncIO::Create after the virtual filesystem (startup step 5) and owned by
///   Zenith_Runtime; owns m_uThreadCount I/O threads, joined by its destructor in shutdown step 8.
/// Thread affinity: ANY_THREAD. Job workers submit and poll; nothing a job calls blocks.
/// Invalidation: A request's destination span (reads) or source span (writes) must stay valid and untouched by the
///   requester until its operation reports complete; after completion Zenith_AsyncIO never touches it again.
/// Units: Sizes and offsets in bytes; priorities as Zenith_IOPriority.
/// Errors: Submission returns Zenith_Result<Zenith_OperationHandle, Zenith_IOError> (IO_ERROR_QUEUE_FULL,
///   IO_ERROR_READ_ONLY_MOUNT, IO_ERROR_INVALID_REQUEST, IO_ERROR_SHUTTING_DOWN); execution failures are reported by
///   the operation's FAILED status with a Zenith_IOError from GetError; nothing asserts on file state or sizes.
/// Reentrancy: NOT_APPLICABLE. It invokes no callbacks; completions are records.
/// Locks: uLOCK_LEVEL_IO_QUEUES (610, short-hold) for the priority queues; it takes uLOCK_LEVEL_FILESYSTEM_MOUNTS (600)
///   only through Resolve, never while holding the queue lock.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_AsyncIO
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_AsyncIO>, Zenith_IOError> Create(const Zenith_AsyncIODesc& xDesc);
	~Zenith_AsyncIO();

	Zenith_Result<Zenith_OperationHandle, Zenith_IOError> Read(const Zenith_IORequest& xRequest);
	// A read from a source that is not mounted (PACK's pack verification, [Z2-N-ARCH-956]): strRelative is resolved by
	// xSource.Lookup alone, never through the mounts; xSource must outlive the operation (§ "Reads from an unmounted source").
	Zenith_Result<Zenith_OperationHandle, Zenith_IOError> ReadFromSource(const Zenith_MountSource& xSource,
		Zenith_StringView strRelative, uint64 ulOffset, Zenith_MutableByteSpan xDestination, Zenith_IOPriority ePriority,
		Zenith_IOCompletionQueue* pxCompletions, uint64 ulUserTag);
	Zenith_Result<Zenith_OperationHandle, Zenith_IOError> WriteAtomic(Zenith_VirtualPath xPath,
		Zenith_ByteSpan xBytes, Zenith_IOPriority ePriority);   // user mount only; temp, flush, rename
	Zenith_Result<Zenith_OperationHandle, Zenith_IOError> WriteAtomicWithCompletion(Zenith_VirtualPath xPath,
		Zenith_ByteSpan xBytes, Zenith_IOPriority ePriority, Zenith_IOCompletionQueue* pxCompletions, uint64 ulUserTag);
	Zenith_Result<Zenith_OperationHandle, Zenith_IOError> Delete(Zenith_VirtualPath xPath, Zenith_IOPriority ePriority);
	Zenith_Result<Zenith_OperationHandle, Zenith_IOError> Rename(Zenith_VirtualPath xFrom, Zenith_VirtualPath xTo,
		Zenith_IOPriority ePriority);                           // user mount, same top-level directory
	Zenith_Result<Zenith_OperationHandle, Zenith_IOError> CreateDirectory(Zenith_VirtualPath xPath,
		Zenith_IOPriority ePriority);

	Zenith_IOError GetError(const Zenith_OperationHandle& xOperation) const;       // FAILED operations
	uint64 GetBytesTransferred(const Zenith_OperationHandle& xOperation) const;
	Zenith_LowDiskState GetLowDiskState() const;
	Zenith_Result<uint64, Zenith_IOError> QueryFreeBytes() const;                 // the user mount's volume
	Zenith_IOStats GetStats() const;                                              // the counters below
};

struct Zenith_IOStats                    // cumulative since Create unless the member says otherwise
{
	uint32 m_auQueueDepth[5] = {};         // current depth per Zenith_IOPriority
	uint32 m_auQueueHighWater[5] = {};
	uint64 m_aulQueueFull[5] = {};
	uint64 m_ulBytesRead = 0, m_ulBytesWritten = 0;
	uint64 m_ulRequestsCompleted = 0, m_ulRequestsCancelled = 0, m_ulRequestsFailed = 0, m_ulPreemptions = 0;
	uint64 m_ulLowDiskRefusals = 0, m_ulStaleTempsRemoved = 0;
	uint64 m_ulLastCriticalLatencyUs = 0;  // the latest critical request's queue-to-completion time
	uint32 m_uLiveOperations = 0;          // the operation table's live count
};

/// @contract interface Zenith_FreeSpaceSource
/// Ownership: Implemented by tests; passed in the desc; outlives the Zenith_AsyncIO.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: None.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_FreeSpaceSource
{
public:
	virtual ~Zenith_FreeSpaceSource() = default;
	virtual uint64 GetFreeBytes() = 0;
};
```

Every asynchronous operation is issued from one `Zenith_OperationTable` that `Zenith_AsyncIO` owns (`m_uMaxOperations` = five times `io.queueCapacity`), as `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_OperationHandle implementation" specifies: `Issue` returns the requester's `Zenith_OperationHandle` and the executor's move-only `Zenith_OperationTicket`; the ticket travels in the request record (a per-slot side array indexed by `GetSlotID().m_uSlot`, because the queue records are trivially copyable) and the I/O thread calls `SetRunning`, `SetProgress`, `IsCancelRequested` and one of `CompleteSucceeded`, `CompleteFailed(m_eCode)` or `CompleteCancelled`. The failure code is the `Zenith_IOErrorCode`; the whole `Zenith_IOError` record (path detail and OS code) is kept in the same side array and read with `GetError`. A table exhausted by live operations returns `IO_ERROR_QUEUE_FULL` at submission.

### Reads from an unmounted source

`ReadFromSource` is `Read` with the resolution step replaced by the caller's source:

- **Submission.** Validation follows `Read`'s rules, with these differences:
  - `strRelative` must pass rules R2 and R4–R9 of § "Validation rules and order" as a relative path of at most 512 bytes, with no root; otherwise `IO_ERROR_INVALID_REQUEST`;
  - `xDestination` must be non-empty, and its length is the byte count (`m_ulBytes` of `Read` does not exist here);
  - `ulOffset + xDestination.Size()` must not overflow;
  - after shutdown has begun, the call returns `IO_ERROR_SHUTTING_DOWN`.
- **Scheduling.** The request joins `ePriority`'s queue like any read. It never enters the in-flight set, it is chunked by `io.chunkBytes` and it can be preempted.
- **Execution.** The I/O thread calls `xSource.Lookup(strRelative)`, then `Open`, then `Read`, chunk by chunk, into `xDestination` at `ulOffset`, then `Close`. Errors map as § "Filesystem error enumerations" maps a source's errors:
  - `FILE_ERROR_NOT_FOUND` is `IO_ERROR_NOT_FOUND`;
  - `FILE_ERROR_ESCAPES_MOUNT` and `FILE_ERROR_CASE_MISMATCH` are `IO_ERROR_PATH`;
  - `FILE_ERROR_ARCHIVE_CORRUPT` is `IO_ERROR_ARCHIVE_CORRUPT`;
  - `FILE_ERROR_OS` is `IO_ERROR_READ`;
  - an entry that ends before `ulOffset + xDestination.Size()` is `IO_ERROR_SHORT_READ`, with the bytes read reported.
- **Completion and cancellation.** These are exactly `Read`'s (§ "Cancellation", § "Completion delivery"). The completion record carries `ulUserTag`.
- **Lifetime.** `Zenith_AsyncIO` holds no reference on `xSource`. The caller keeps the source alive until every operation naming it has completed. PACK's installer destroys a verification's sources only after V3, when every read has reported complete (`Zenith2/Docs/Design/PACK/Algorithms.md` → "Verification pipeline" step 6). A source destroyed earlier is a programmer error (a use-after-free); `Test_AsyncIO_ReadFromSourceUnmounted` destroys its source only after the last completion and runs clean under `win-asan`.
- **Thread affinity.** The call is ANY_THREAD, and `xSource` must be ANY_THREAD too. The directory source is; an install-tree source is not, so it is never passed here.

### Request lifetime

| Step | Where | What happens |
|---|---|---|
| submit | the calling thread | validate (below); issue an operation slot; reserve a completion slot when a queue is named; push onto the priority queue under the queue lock; wake one I/O thread (its `Zenith_Semaphore`) |
| queued | the priority queue | `OPERATION_STATUS_PENDING` |
| started | an I/O thread | `OPERATION_STATUS_RUNNING`; resolve; open |
| chunks | the I/O thread | read or write one chunk per step, updating progress (bytes done / total) |
| complete | the I/O thread | close the file; set SUCCEEDED, FAILED (with the error) or CANCELLED; push the completion record |

Validation at submit: a `content:/` path given to a write returns `IO_ERROR_READ_ONLY_MOUNT`; a read destination shorter than `m_ulBytes` (when non-zero) returns `IO_ERROR_DESTINATION_TOO_SMALL`; an invalid path, an empty destination with `m_ulBytes > 0`, or `m_ulOffset + m_ulBytes` overflowing returns `IO_ERROR_INVALID_REQUEST`; after shutdown began, `IO_ERROR_SHUTTING_DOWN`. None of these asserts, because sizes and offsets come from cooked tables of contents.

### I/O scheduler algorithm

Each I/O thread repeats:

1. Wait on its semaphore until work exists or shutdown is requested.
2. Under the queue lock: scan the queues from the highest priority down and, within a queue, from its head, and take the first request none of whose target paths is in the in-flight set; insert its target paths into the set; release the lock. The in-flight set has one definition: it holds the resolved user-mount path of every started `WriteAtomic` and `Delete` (its target) and both resolved paths of every started `Rename` (its source `xFrom` and its target `xTo`); `CreateDirectory` and reads never enter it and never wait on it (a read of a file being replaced sees the old file or the new one, because W7 is an atomic rename). Preempted requests sit at the head of their queue (§ "Chunking and preemption"). When every queued request is blocked on an in-flight target, the thread waits on its semaphore again: it never re-queues a request or spins. The scan is bounded by the total queue depth (at most five times `io.queueCapacity`) and is O(1) in the common case of no in-flight writes.
3. If the operation's cancel flag is set, complete it `CANCELLED` without any I/O and go to 2 (a request cancelled while queued never touches the file system).
4. Execute the next chunk of the request (a read chunk, or a write step of "Atomic writes").
5. After the chunk: if the cancel flag is set, stop at this boundary (§ "Cancellation"); if the request is finished, complete it, remove its target paths from the in-flight set under the queue lock, and signal one waiting I/O thread when a queued request names one of them; otherwise, if a request of strictly higher priority is queued, preempt (§ "Chunking and preemption"); else go to 4.

Ordering guarantees: requests start in priority order, and within one priority in submission order (a preempted request resumes before any request of its priority submitted after it), with one exception: a request whose target path is in the in-flight set is passed over, and requests of its priority submitted after it may start before it. For one target path the promise is exactly this: two operations submitted at the same priority start, and therefore land, in submission order; across priorities a higher-priority operation submitted after a queued lower-priority one on the same path may start first, and the lower one then lands last. Every PLAT writer of one file uses one priority (`Zenith2/Docs/Design/PLAT/Configuration.md` → "Persisting the user layer"; `Zenith2/Docs/Design/PLAT/Input.md` → "Persisting user bindings") so its writes land in submission order; with several I/O threads, requests of one priority may complete out of order. There is no ageing: `IO_PRIORITY_BACKGROUND` work waits while higher priorities have work, which is the intended behaviour for prefetch. Queue capacity is `io.queueCapacity` (1,024) per priority with `QUEUE_FULL_POLICY_REJECT`; before rejecting, the submitting thread removes the cancelled requests of that queue under the lock (completing them `CANCELLED`), so cancelled work never causes `IO_ERROR_QUEUE_FULL`. The counters `io.queue.<priority>.full`, `io.queue.<priority>.dropped` (equal to `.full`: a rejected request is the dropped one) and `io.queue.<priority>.high_water` are the bounded-queue triple of [Z2-N-CONTRACT-345] (`Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", "Bounded queues"); `io.queue.<priority>.depth` is PLAT's own gauge beside them.

### Chunking and preemption

- A read of more than `io.chunkBytes` (1 MiB) is executed as successive chunk reads; a write is executed as successive chunk writes followed by the flush and rename steps.
- At a chunk boundary, a running request of priority P yields to a queued request of priority below P's value (a strictly higher priority): the running request's progress (next offset, open file) is kept, and it is pushed back at the head of its own priority queue, where it resumes before every request submitted to that priority after it.
- The rename step of a write is never preempted and never cancelled once it has started.
- The chunk size bounds the latency a `IO_PRIORITY_CRITICAL` request sees behind lower-priority work to one chunk per I/O thread plus its own time; that latency is the pending new row "Async I/O critical read latency" (entry document → "Pending new rows").

### Cancellation

| Cancelled when | Behaviour | Reported |
|---|---|---|
| queued | removed without any I/O at its dequeue or at the next queue-full sweep | `OPERATION_STATUS_CANCELLED`; the destination was never touched |
| reading a chunk | the chunk's system call completes; its data may have been written into the destination; no further chunk is read | `OPERATION_STATUS_CANCELLED` only after the file is closed and the destination is no longer touched (`Zenith2/Docs/Architecture/PlatformLayer.md` → "Asynchronous I/O") |
| writing a chunk (before rename) | the chunk completes; the temporary file is closed and deleted; the target is untouched | `OPERATION_STATUS_CANCELLED` after the deletion |
| renaming | the cancellation is ignored; the write completes | `OPERATION_STATUS_SUCCEEDED` (`Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", "Cancellation completion": an operation that completes before it observes a cancellation reports success) |
| the handle destroyed while incomplete | `RequestCancel` then detach (`Zenith2/Docs/Contracts.md` → "Handle kinds", "Zenith_OperationHandle"); the operation proceeds as a cancellation above and its slot is released when it completes | no requester observes it; its completion record, if a queue was named, is still pushed |

A cancelled operation releases what it holds (the open file, the temporary file, the completion reservation if no record is pushed) on its I/O thread before reporting completion, and owns nothing afterwards.

### Completion delivery

- A requester polls its `Zenith_OperationHandle` (`IsComplete`, `GetStatus`, `GetProgress`) or drains its `Zenith_IOCompletionQueue` at a point it chooses.
- I/O completion is not simulation-visible by itself. A subsystem whose I/O results affect simulation (streaming loads, save completion) drains its queue into the world's pending-result set and publishes the results at the next tick boundary in completion-ID order (Z2-D-053, `Zenith2/Docs/Scripting.md` → "Merge-time capacity and asynchronous results"); a failed I/O is a non-reproducible result whose payload the replay log records.
- The completion record order in a queue is the I/O threads' completion order; consumers that need a deterministic order sort by the completion ID they assigned, never by record order.

### Atomic writes

`WriteAtomic(xPath, xBytes, ePriority)` writes a whole file so that an interruption at any point leaves the complete old file or the complete new file:

| Step | Action | On failure |
|---|---|---|
| W1 | validate: `user:/` root; not a directory; `xBytes` at most `io.maxWriteBytes` (256 MiB) | `IO_ERROR_INVALID_REQUEST` |
| W2 | free-space check (§ "Free-space checks") | `IO_ERROR_LOW_DISK`; nothing touched |
| W3 | create missing parent directories one component at a time, each verified on its handle by the escape check and the case policy's comparison | `IO_ERROR_PATH` (`PATH_ERROR_ESCAPES_MOUNT`, `PATH_ERROR_CASE_MISMATCH` or `PATH_ERROR_NAME_MISMATCH`) or `IO_ERROR_WRITE`; directories created so far stay (empty directories are harmless) |
| W4 | create `<target>.z2atomic` beside the target (`CREATE_ALWAYS` over a stale one; `O_CREAT \| O_TRUNC`), verify its handle by the escape check and the case policy's comparison | `IO_ERROR_PATH` (`PATH_ERROR_ESCAPES_MOUNT`, `PATH_ERROR_CASE_MISMATCH` or `PATH_ERROR_NAME_MISMATCH`), `IO_ERROR_WRITE` |
| W5 | write `xBytes` in chunks (cancellable between chunks) | `IO_ERROR_DISK_FULL` (`ERROR_DISK_FULL`, `ENOSPC`, `EDQUOT`), `IO_ERROR_WRITE`; the temporary file is deleted |
| W6 | flush and close (`FlushFileBuffers`; `fsync`) | as W5 |
| W7 | rename over the target: `MoveFileExW(tmp, target, MOVEFILE_REPLACE_EXISTING \| MOVEFILE_WRITE_THROUGH)`; `rename(tmp, target)` then `fsync` of the directory | `IO_ERROR_WRITE`; the temporary file is deleted, the target is untouched |

- The `.z2atomic` suffix belongs to `WriteAtomic` alone; the multi-document journaled write of `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind" ("Journaled atomic write") uses `.z2tmp` and a `*.z2journal`, is built by SAVE and the tools on top of these operations, and is never touched by PLAT's sweep.
- At `Zenith_AsyncIO::Create`, every `*.z2atomic` file under the user mount (a write interrupted by a crash or power loss) is deleted and counted in `io.stale_temps_removed`; the old file it would have replaced is intact by construction.
- Two writes to the same target are serialised by the in-flight set of step 2 of "I/O scheduler algorithm": the second is passed over until the first completes. They land in submission order when submitted at one priority; across priorities the ordering promise of that section applies.

### Other user-mount operations

| Operation | Behaviour |
|---|---|
| `Delete` | removes a file (never a directory) of the user mount; a missing file succeeds (idempotent) |
| `Rename` | moves `xFrom` (its source) to `xTo` (its target): both paths in the user mount and under the same top-level directory (`saves/` to `saves/`); replaces an existing target atomically as W7 does; both paths enter the in-flight set |
| `CreateDirectory` | creates the directory and its missing parents as W3 does; an existing directory succeeds |

Each runs as one request of the given priority, subject to the same validation, escape check and cancellation rules (cancellation before the system call only).

## Low-disk detection and behaviour

### Free-space checks

- Before every write step that can grow the volume (W2 of "Atomic writes", `CreateDirectory`), the I/O thread queries the free bytes of the user mount's volume (`GetDiskFreeSpaceExW` with the caller-available count; `statvfs` with `f_bavail × f_frsize`), or the test's `Zenith_FreeSpaceSource`.
- When free bytes minus the write's size would fall below `io.lowDiskMarginBytes` (64 MiB by default), the write is refused with `IO_ERROR_LOW_DISK` before any file is created or opened, so the target and its directory are untouched (`Zenith2/Docs/Architecture/PlatformLayer.md` → "Low-disk behaviour").
- A write that fails with the OS's disk-full error despite the check (another process consumed the space) deletes its temporary file, reports `IO_ERROR_DISK_FULL`, and enters the low-disk state with reason `LOW_DISK_REASON_DISK_FULL`.
- Reads, deletes and renames are never refused for low disk, so a player can always free space by deleting saves.

### Low-disk state machine

```cpp
enum Zenith_LowDiskState : uint8 { LOW_DISK_STATE_NORMAL = 1, LOW_DISK_STATE_LOW };
enum Zenith_LowDiskReason : uint8 { LOW_DISK_REASON_WRITE_REFUSED = 1, LOW_DISK_REASON_POLL, LOW_DISK_REASON_DISK_FULL };
```

| State | Write refused (margin) | Disk-full error | Poll: free < margin | Poll: free ≥ margin × `io.lowDiskClearFactor` |
|---|---|---|---|---|
| NORMAL | → LOW; queue `PLATFORM_EVENT_LOW_DISK` (reason WRITE_REFUSED, free bytes, margin) | → LOW; queue `PLATFORM_EVENT_LOW_DISK` (DISK_FULL) | → LOW; queue `PLATFORM_EVENT_LOW_DISK` (POLL) | — |
| LOW | stays; `io.low_disk_refusals` + 1; no new event | stays; counted | stays | → NORMAL; queue `PLATFORM_EVENT_LOW_DISK_CLEARED` |

- The poll runs at `Create` and then on an idle I/O thread every `io.lowDiskPollSeconds` (30 s); the clear threshold's factor (2) is hysteresis, so the state does not flap around the margin.
- Events are queued to `Zenith_Platform` (the low-disk state queue of `Zenith2/Docs/Design/PLAT/Platform.md` → "Event queue and overflow") and appear at the next pump.

### Surfacing to the game

1. `PLATFORM_EVENT_LOW_DISK` and `PLATFORM_EVENT_LOW_DISK_CLEARED` are world-visible: the runtime bridge publishes them to every world at the next tick boundary, logged in the replay log with their payload, and scripts observe the events `Platform.LowDisk` (fields `FreeBytes`, `MarginBytes`, `Reason`) and `Platform.LowDiskCleared` in phase 9 (`Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Script-facing input API").
2. UI and the game's settings screens read `Zenith_AsyncIO::GetLowDiskState()` and `QueryFreeBytes()` directly for presentation.
3. The subsystem whose write was refused receives `IO_ERROR_LOW_DISK` in its operation and reports its own failure to the game (a save failure is SAVE's event, `Zenith2/Docs/Design/SAVE.md` (pending; its "Failure handling" section)); PLAT never retries a refused write.

## Filesystem error enumerations

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_VirtualFileSystem.h
enum Zenith_PathErrorCode : uint8
{
	PATH_ERROR_BAD_ROOT = 1,             // R3
	PATH_ERROR_ENCODING,                 // R2
	PATH_ERROR_SEPARATOR,                // R4
	PATH_ERROR_TRAVERSAL,                // R7
	PATH_ERROR_ABSOLUTE,                 // R5
	PATH_ERROR_RESERVED_NAME,            // R9
	PATH_ERROR_TOO_LONG,                 // R1, R8
	PATH_ERROR_USER_NAMESPACE,           // R10
	PATH_ERROR_ESCAPES_MOUNT,            // the escape check after resolution
	PATH_ERROR_NOT_FOUND,                // Resolve found no mount holding the path
	// added by this design, after Architecture's enumerators:
	PATH_ERROR_FORBIDDEN_CHARACTER,      // R6
	PATH_ERROR_CASE_MISMATCH,            // the case policy
	PATH_ERROR_SOURCE_FAILED,            // a source's Lookup failed with FILE_ERROR_OS or FILE_ERROR_ARCHIVE_CORRUPT
	PATH_ERROR_NAME_MISMATCH,            // the final path differs from the request other than by case (an 8.3 alias)
};

struct Zenith_PathError                  // an error record (Architecture's declaration), 8 bytes
{
	Zenith_PathErrorCode m_eCode;
	uint32 m_uByteOffset;               // the offending byte or component start in the input path; 0 after resolution
};

enum Zenith_FileError : uint8
{
	FILE_ERROR_INVALID_PATH = 1,         // a default-constructed (invalid) Zenith_VirtualPath
	FILE_ERROR_NOT_FOUND,
	FILE_ERROR_ESCAPES_MOUNT,
	FILE_ERROR_CASE_MISMATCH,
	FILE_ERROR_NOT_A_FILE,
	FILE_ERROR_NOT_A_DIRECTORY,
	FILE_ERROR_ACCESS_DENIED,
	FILE_ERROR_TOO_LARGE,
	FILE_ERROR_SHORT_READ,               // the entry ended before the requested range
	FILE_ERROR_ARCHIVE_CORRUPT,
	FILE_ERROR_MANIFEST_INVALID,
	FILE_ERROR_MOUNT_NOT_ALLOWED,        // the loose mount or a command-line file in a Shipping build
	FILE_ERROR_MOUNT_FAILED,             // a mount root could not be created or opened
	FILE_ERROR_TOO_MANY_SOURCES,         // more than 256 sources in one mount
	FILE_ERROR_OS,                       // any other OS failure; the OS code is logged
	FILE_ERROR_OUT_OF_MEMORY,
	FILE_ERROR_NAME_MISMATCH,            // see PATH_ERROR_NAME_MISMATCH
};

// Zenith2/Engine/ZenithPlatform/Zenith_AsyncIO.h
enum Zenith_IOErrorCode : uint8
{
	IO_ERROR_QUEUE_FULL = 1,
	IO_ERROR_LOW_DISK,
	IO_ERROR_NOT_FOUND,
	IO_ERROR_ACCESS,                     // access denied by the OS
	IO_ERROR_READ,                       // any other OS read failure; m_uOSError holds the code
	IO_ERROR_WRITE,                      // any other OS write, flush or rename failure; m_uOSError holds the code
	IO_ERROR_READ_ONLY_MOUNT,            // a write to a content:/ path
	IO_ERROR_PATH,                       // the path failed validation or resolution; m_xPath carries the detail
	// added by this design, after Architecture's enumerators:
	IO_ERROR_DISK_FULL,                  // the OS reported the volume full despite the low-disk check
	IO_ERROR_SHORT_READ,
	IO_ERROR_DESTINATION_TOO_SMALL,
	IO_ERROR_ARCHIVE_CORRUPT,
	IO_ERROR_INVALID_REQUEST,
	IO_ERROR_SHUTTING_DOWN,
	IO_ERROR_OUT_OF_MEMORY,
};

struct Zenith_IOError                    // an error record (Architecture's declaration), 16 bytes
{
	Zenith_IOErrorCode m_eCode;
	Zenith_PathError m_xPath;            // valid for IO_ERROR_PATH
	uint32 m_uOSError;                  // GetLastError / errno, 0 when not applicable
};

constexpr const char* Zenith_ToString(Zenith_PathErrorCode eError);
constexpr const char* Zenith_ToString(Zenith_FileError eError);
constexpr const char* Zenith_ToString(Zenith_IOErrorCode eError);
```

The error types follow `Zenith2/Docs/Architecture/PlatformLayer.md` → "Path validation" and "Asynchronous I/O": `Zenith_PathError` and `Zenith_IOError` are error records whose first member is the code (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Results and the error taxonomy"), and the enumerators this design adds follow Architecture's, so no shipped value moves. The mapping at the resolution boundary is explicit (`Zenith2/Docs/Contracts.md` → "Errors and results"): `PATH_ERROR_NOT_FOUND` → `FILE_ERROR_NOT_FOUND` → `IO_ERROR_NOT_FOUND`; `PATH_ERROR_ESCAPES_MOUNT` → `FILE_ERROR_ESCAPES_MOUNT` → `IO_ERROR_PATH` with `m_xPath.m_eCode = PATH_ERROR_ESCAPES_MOUNT`; `PATH_ERROR_CASE_MISMATCH` → `FILE_ERROR_CASE_MISMATCH` → `IO_ERROR_PATH` with `PATH_ERROR_CASE_MISMATCH`; `PATH_ERROR_NAME_MISMATCH` → `FILE_ERROR_NAME_MISMATCH` → `IO_ERROR_PATH` with `PATH_ERROR_NAME_MISMATCH`; every other path error → `FILE_ERROR_INVALID_PATH` → `IO_ERROR_PATH` with its code; `PATH_ERROR_SOURCE_FAILED` → `FILE_ERROR_OS` or `FILE_ERROR_ARCHIVE_CORRUPT` (the source's own error) → `IO_ERROR_READ` (with the OS code) or `IO_ERROR_ARCHIVE_CORRUPT`; `FILE_ERROR_ACCESS_DENIED` → `IO_ERROR_ACCESS`; `FILE_ERROR_OS` → `IO_ERROR_READ` or `IO_ERROR_WRITE` by the operation's direction.

## Filesystem tests

Tests live in `Zenith2/Engine/ZenithPlatform/Tests/` and run in the `Test` step on `win-debug`, `win-release` and `linux-tsan`; the table restates `Zenith2/Docs/Architecture/PlatformLayer.md` → "Required tests" for its filesystem rows and adds the rest.

| Test | File | Asserts |
|---|---|---|
| `Test_VirtualPath_EachRuleRejects` | `Zenith_VirtualPath_Tests.cpp` | one input per rule R1–R10 is rejected with its error code; 50 valid paths parse; an input violating R2 and R7 reports R2 (the order) |
| `Test_VirtualPath_UserCrashRootEveryBuild` | `Zenith_VirtualPath_Tests.cpp` (also built and run in the Shipping test build) | `user:/crash/a.dmp`, `user:/crash/crashpad/x` and `user:/crash/device_loss_20261017T120000Z.json` parse with `PATH_ROOT_USER` in every configuration it runs in; `user:/Crash/x` and `user:/crashes/x` fail with `Zenith_PathError{ PATH_ERROR_USER_NAMESPACE, 6 }` |
| `Test_VirtualPath_UserDevtoolsRootOutsideShipping` | `Zenith_VirtualPath_Tests.cpp` (a non-Shipping build) | `user:/devtools/frames/1.json` and `user:/devtools/captures/intro/0001.png` parse with `PATH_ROOT_USER`; `Zenith_VirtualPath::Root(PATH_ROOT_USER).Append("devtools/x")` succeeds; `user:/DevTools/x` fails with `Zenith_PathError{ PATH_ERROR_USER_NAMESPACE, 6 }` |
| `Test_VirtualPath_UserDevtoolsRefusedInShipping` | `Zenith_VirtualPath_Tests.cpp` (Shipping test build) | `user:/devtools/frames/1.json` and `user:/devtools/x` fail with exactly `Zenith_PathError{ PATH_ERROR_USER_NAMESPACE, 6 }`; `Zenith_VirtualPath::Root(PATH_ROOT_USER).Append("devtools/x")` fails with `PATH_ERROR_USER_NAMESPACE`; `user:/crash/x` and `user:/saves/x` parse |
| `Test_VirtualPath_ReservedNamesEveryForm` | `Zenith_VirtualPath_Tests.cpp` | every name of "Reserved device names" in upper, lower and mixed case, with and without an extension, including the superscript forms and `CONIN$`, is rejected; `CONSOLE.txt` and `COM10` are accepted |
| `Test_VirtualPath_EncodingEdgeCases` | `Zenith_VirtualPath_Tests.cpp` | overlong `/` (`C0 AF`) and `.` (`C0 AE`), a surrogate, U+110000, a truncated sequence, NUL, U+0085 are rejected with `PATH_ERROR_ENCODING` |
| `Test_VirtualPath_AppendValidates` | `Zenith_VirtualPath_Tests.cpp` | `Append("../x")`, `Append("a//b")`, `Append("C:/x")` fail with the parse errors; `Append("a/b")` succeeds |
| `Test_VirtualPath_Fuzz` (`Zenith_Fuzz_VirtualPath`) | `Tests/Fuzz/VirtualPath.cpp` | libFuzzer over `Zenith_ParseVirtualPath` finds no crash and no accepted path containing a `..`, `.` or empty component, a `\`, or a `:` after the root; every accepted path re-parses from its `GetText()` to an equal path |
| `Test_VirtualPath_ResolvedNeverEscapesRoot` | `Zenith_VirtualFileSystem_Tests.cpp` | a property test: 100,000 generated accepted paths resolved against a temporary root containing links to outside never open a file outside it |
| `Test_VirtualFileSystem_PrecedenceOrder` | `Zenith_VirtualFileSystem_Tests.cpp` | a file present in package, patch and loose resolves to loose; without loose, to patch; within a mount the higher `order` wins |
| `Test_VirtualFileSystem_UserNeverShadowsContent` | `Zenith_VirtualFileSystem_Tests.cpp` | a user-directory file at a content path's relative name is never returned for `content:/` |
| `Test_VirtualFileSystem_SymlinkEscapeRejected` | `Zenith_VirtualFileSystem_Tests.cpp` | a symbolic link (Windows: a junction and a symbolic link) pointing outside a mount root fails with `PATH_ERROR_ESCAPES_MOUNT`; a link inside the root resolves |
| `Test_VirtualFileSystem_ShortNameAliasRejected` | `Zenith_VirtualFileSystem_Tests.cpp` (Windows, on a volume with 8.3 names enabled) | `content:/<SHORT~1>/x` naming an existing long-named directory fails with `PATH_ERROR_NAME_MISMATCH`, while the long name resolves; on Linux the same request is not found |
| `Test_VirtualFileSystem_ResolveCacheTransparent` | `Zenith_VirtualFileSystem_Tests.cpp` | 10,000 resolutions with the cache enabled and disabled return identical results; a mount change clears the cache before the next resolution; a file added to the loose mount after a miss, and a loose file removed after a hit, resolve correctly at once with no watcher attached |
| `Test_VirtualFileSystem_CaseMismatchOnWindows` | `Zenith_VirtualFileSystem_Tests.cpp` | on Windows a request differing only in case fails with `PATH_ERROR_CASE_MISMATCH`; on Linux it is not found |
| `Test_VirtualFileSystem_HostileHigherMountStopsSearch` | `Zenith_VirtualFileSystem_Tests.cpp` | an escaping entry in the loose mount is reported, never bypassed to the package entry |
| `Test_VirtualFileSystem_MountSwapDoesNotTearReads` | `Zenith_VirtualFileSystem_Tests.cpp` | 1,000 reads in flight across 100 `AddSource`/`RemoveSource` swaps all complete against the table they resolved in; no source is destroyed while referenced |
| `Test_VirtualFileSystem_LooseAbsentInShipping` | `Zenith_VirtualFileSystem_Tests.cpp` (Shipping test build) | `AddSource(MOUNT_KIND_LOOSE, …)` returns `FILE_ERROR_MOUNT_NOT_ALLOWED`; `--assets-root` is not consulted |
| `Test_VirtualFileSystem_SyncReadOnJobWorkerChecks` | `Zenith_VirtualFileSystem_DefectTests.cpp` (`z2-death-check`) | `ReadWhole` from a thread marked as a job worker fails the blocking-wait check |
| `Test_VirtualFileSystem_ListUnionSorted` | `Zenith_VirtualFileSystem_Tests.cpp` | a directory present in two mounts lists each name once, from the higher mount, in byte order |
| `Test_VirtualFileSystem_Create_FailsAtEachStep_Unwinds` | `Zenith_VirtualFileSystem_Tests.cpp` | the fault sweep over mount-root creation, manifest read, source creation |
| `Test_MountManifest_EachRule` | `Zenith_MountManifest_Tests.cpp` | one fixture per key rule fails with `FILE_ERROR_MANIFEST_INVALID` naming the key path; a valid manifest builds the package sources in order |
| `Test_MountManifest_ShippingRequiresManifest` | `Zenith_MountManifest_Tests.cpp` (Shipping test build) | an absent manifest stops creation with the startup-validation error |
| `Test_MountManifest_InstallTimePacksOnly` | `Zenith_MountManifest_Tests.cpp` | a manifest listing an `installTime`, a `fastFollow` and an `onDemand` pack mounts the install-time pack's archives in the package mount at its order and nothing of the other two; `DescribeMounts` shows no source for them |
| `Test_PatchMount_HighestVerifiedMounted` | `Zenith_PatchMount_Tests.cpp` | versions 1.0.2 (bad signature), 1.0.1 (valid), 1.0.0 (valid): 1.0.1 is mounted, 1.0.2 rejected and logged, never partly mounted |
| `Test_PatchMount_TamperCasesRejected` | `Zenith_PatchMount_Tests.cpp` | a flipped byte, a wrong key, a wrong target build, a wrong release line and a replayed older manifest are each rejected (with SEC's verifier fixtures) |
| `Test_PatchMount_DirectoryNameAndBaseContent` | `Zenith_PatchMount_Tests.cpp` | a valid 1.0.3 manifest in a directory named `1.0.4` or `1.00.3` is rejected (`DIRECTORY_NAME`); a manifest whose base content hash differs from the package's is rejected (`BASE_CONTENT`), and with no package manifest (`m_ulPackageContentHash` 0) no patch mounts; after 1.0.2 mounts, `GetBuildIdentity()` reports its result content hash in `m_ulContentHash` and the package's in `m_ulPackageContentHash`; a DLC manifest in a patch directory is rejected (`KIND`); `.incoming` and `.trash` are skipped without a log line |
| `Test_PatchMount_StampSkipAndRehash` | `Zenith_PatchMount_Tests.cpp` | through a test `Zenith_UpdateStampLookup` and a directory source whose stamps are real: a recorded patch mounts with no file hashed (`vfs.patch.stamp_skips` = file count, `vfs.patch.files_hashed` 0); table-driven over the five stamp members, a record differing in exactly one of `m_ulBytes`, `m_ulLastWriteNs`, `m_ulChangeNs`, `m_ulVolume` or `m_auFileID` (one byte flipped by the test's encoder) re-hashes exactly that file; a record under another manifest digest, and a null lookup, re-hash every file; a file written within 100 ms of the mount is hashed (unsettled); a signature failure is refused whatever the lookup says; a source on a forced unsupported volume kind answers false and every file is hashed; the lookup pointer is not read after `Create` returns (the test destroys it and then reads every file) |
| `Test_UpdateManifest_ErrorToString` | `Zenith_UpdateManifest_Tests.cpp` | every `Zenith_UpdateVerifyError` enumerator is spelled `"UPDATE_VERIFY_ERROR_<NAME>"`; an out-of-range value is `"<unknown>"` |
| `Test_UpdateManifest_FileEntryDecode` | `Zenith_UpdateManifest_Tests.cpp` | `GetFile` decodes a hand-built entry (name, bytes, digest, archive flag) at offsets 0, 4, 6, 8 and 16, little-endian |
| `Test_FileIdentity_SettledWindow` | `Zenith_UpdateManifest_Tests.cpp` | `Zenith_IsStampSettled` at 100,000,000 ns of age is true, at 99,999,999 false, for a future stamp false, and is decided by the later of the two times; stamps differing in one byte compare unequal |
| `Test_UpdateManifest_DecodeRules` | `Zenith_UpdateManifest_Tests.cpp` | one fixture per U1, U2, U3 and U7 rule fails with its error (224 bytes, a wrong magic, version 2, kind 3, a reserved byte, a total off by one, F 0 and 65,537, S 1,048,577, an overflowing size sum, one signature bit, the wrong key, an unsorted pair, a duplicate name, `..` as a name, a `.zpak` name without flag bit 0, a DLC name `Bad`, a DLC order 999); a PATCH and a DLC of another build decode; a 31-byte key is `INVALID_ARGUMENT`; no field after the header is read before U3 (a fault point between U2 and U3 never sees a table read) |
| `Test_UpdateManifest_TargetChecks` | `Zenith_UpdateManifest_Tests.cpp` | U4, U5 and U8 in that order: release line `1.1` against `1.0`, a PATCH for another build, a DLC with a non-zero target, a PATCH version 1.1.3 on line 1.0, directory names `1.0.3`/`1.0.03`, a DLC name against its directory; the first failing check is the one reported |
| `Test_UpdateManifest_FileChecks` | `Zenith_UpdateManifest_Tests.cpp` | over a memory-backed test source: a missing file, a directory in place of a file, a short file, a long file, one flipped byte, an `IO` read failure and a 65,535-byte scratch each report their error and `m_uFailedFileIndex`; a stamp that changes between the two bracket calls is `FILE_CHANGED`; the report's hashed counts and bytes equal the sums |
| `Test_UpdateManifest_HalvesComposeToWhole` | `Zenith_UpdateManifest_Tests.cpp` | for 16 fixtures (valid and failing at each half) `Zenith_VerifyUpdateManifest` returns exactly the first error of decode, then target, then files, and fills the info whenever the decode succeeded |
| `Test_DirectorySource_PublicFactoryRules` | `Zenith_VirtualFileSystem_Tests.cpp` | `Zenith_CreateDirectoryMountSource` refuses an empty, relative, non-UTF-8 or 4,097-byte root (`FILE_ERROR_INVALID_PATH`), a missing root (`NOT_FOUND`) and a file (`NOT_A_DIRECTORY`); a source over a temporary directory outside the four roots looks up, opens, reads and lists its files; a link inside it pointing outside fails `FILE_ERROR_ESCAPES_MOUNT`; the source mounts with `AddSource(MOUNT_KIND_PACKAGE, …)` and resolves |
| `Test_DirectorySource_IdentityStampMapping` | `Zenith_VirtualFileSystem_Tests.cpp` | the stamp of a file equals the OS values read independently (`GetFileInformationByHandleEx`, `fstat`); a rewrite of equal bytes that restores the last-write time (`SetFileTime`, `utimensat`) changes `m_ulChangeNs`; renaming the parent directory leaves every contained stamp unchanged; replacing the file by rename changes `m_auFileID`; `m_uVolumeKindOverride` = OTHER answers false; `Zenith_IsStampSettled` is false for a stamp 99 ms old and for one in the future, true for one 100 ms old |
| `Test_AsyncIO_ReadFromSourceUnmounted` | `Zenith_AsyncIO_Tests.cpp` | 16 `ReadFromSource` reads of a 4 MiB file from a factory source that is never mounted, at `IO_PRIORITY_BACKGROUND` into a completion queue, return the file's bytes with their user tags; a critical `Read` submitted meanwhile preempts them at a chunk boundary; a read cancelled in flight reports `CANCELLED` only after its destination is untouched; a relative path with `..`, a root prefix or a `\` is `IO_ERROR_INVALID_REQUEST` at submission; a read past the end is `IO_ERROR_SHORT_READ` with the bytes read; the source is destroyed after the last completion and `win-asan` reports nothing |
| `Test_CommandLineHostFile_GrantOnly` | `Zenith2/Engine/ZenithRuntime/Tests/Zenith_CommandLineHostFile_Tests.cpp` (built into `ZenithRuntime_Tests`, which links ZenithRuntime and so can obtain a grant from the real constructor; `ZenithPlatform_Tests` cannot, and PLAT adds no test-access seam to the grant) | `Zenith_CommandLine::Parse` of `--record <new temporary file>` and `--config-dump <existing file>` yields grants through `GetFileGrant(COMMAND_LINE_FILE_RECORD)` and `GetFileGrant(COMMAND_LINE_FILE_CONFIG_DUMP)`; the first opens with `HOST_FILE_MODE_WRITE_NEW` and appends; `HOST_FILE_MODE_WRITE_NEW` on the second refuses the existing file and leaves it unchanged; in the Shipping test build `Parse` rejects `--record`, so no grant exists |
| `Test_CommandLineHostDirectory_EvidenceReads` | `Zenith_HostFileGrant_Tests.cpp` (grants built through the file's test-only `Zenith_CommandLine` friend, as `Test_CommandLineFileGrant_Accessors` builds them) | an EVIDENCE grant over a temporary directory opens `rows.json` and `fingerprint.json` for reading and returns their bytes; `other.json`, `sub/rows.json`, `..\rows.json` and `ROWS.json` are `FILE_ERROR_ACCESS_DENIED`; a 1 MiB + 1 byte `rows.json` is `FILE_ERROR_TOO_LARGE`; `Append` on the opened file is `FILE_ERROR_ACCESS_DENIED`; `Zenith_ListCommandLineHostDirectory` on the grant is `FILE_ERROR_ACCESS_DENIED`; a file grant given to either function is `FILE_ERROR_NOT_A_DIRECTORY`; the Shipping test build returns `FILE_ERROR_MOUNT_NOT_ALLOWED` |
| `Test_CommandLineHostDirectory_ReadOnlyListAndOpen` | `Zenith_HostFileGrant_Tests.cpp` | a READ_ONLY grant over a tree `A/v1.zbst`, `A/v2.zbst`, `B/v1.zbst`, `top.txt`, `A/deep/x.zbst`, a link `L` to an outside directory and a file `CON.txt`: the listing is exactly `A/v1.zbst`, `A/v2.zbst`, `B/v1.zbst`, `top.txt` in byte order with their sizes (no depth-3 file, no link, no reserved name); opening `A/v2.zbst` returns its bytes; `A/../top.txt`, `A//v1.zbst`, `A/deep/x.zbst` (three components) and `/top.txt` are `FILE_ERROR_INVALID_PATH`; `L/x` (a file in the outside directory) is `FILE_ERROR_ESCAPES_MOUNT` and nothing is read; `a/v1.zbst` is `FILE_ERROR_CASE_MISMATCH` on Windows and `FILE_ERROR_NOT_FOUND` on Linux; 1,025 files is `FILE_ERROR_TOO_LARGE`; `Zenith_CreateFileInCommandLineHostDirectory` on the grant is `FILE_ERROR_ACCESS_DENIED` and creates nothing |
| `Test_AsyncIO_PriorityOrder` | `Zenith_AsyncIO_Tests.cpp` | with one I/O thread held busy, queued critical requests start before queued low ones, and FIFO holds within a priority |
| `Test_AsyncIO_PreemptAtChunkBoundary` | `Zenith_AsyncIO_Tests.cpp` | a 64 MiB background read yields to a critical read at the next chunk boundary and resumes before background requests submitted after it |
| `Test_AsyncIO_CancelQueuedAndInFlight` | `Zenith_AsyncIO_Tests.cpp` | a queued cancel does no I/O (the fault-injecting source records no open); an in-flight cancel reports cancelled only after the destination is no longer touched (a guard-page destination proves no write after completion) |
| `Test_AsyncIO_HandleDestroyedCancelsAndDetaches` | `Zenith_AsyncIO_Tests.cpp` | destroying an incomplete handle cancels; the slot is released when the operation completes; the completion record still arrives |
| `Test_AsyncIO_QueueFullRejects` | `Zenith_AsyncIO_Tests.cpp` | the 1,025th queued request of one priority returns `IO_ERROR_QUEUE_FULL`; cancelled entries are swept before rejecting |
| `Test_AsyncIO_CompletionQueueReserved` | `Zenith_AsyncIO_Tests.cpp` | a completion queue of capacity 4 accepts 4 requests and refuses the fifth with `IO_ERROR_QUEUE_FULL`; draining frees slots |
| `Test_AsyncIO_SubmitValidation` | `Zenith_AsyncIO_Tests.cpp` | a write to `content:/`, an overflowing offset, a short destination and a submission after shutdown each return their error without asserting |
| `Test_AsyncIO_AtomicWriteInterrupted` | `Zenith_AsyncIO_Tests.cpp` | a fault injected at W3, W4, W5, W6 and W7 each leaves the old file intact and no `.z2atomic` behind; after a simulated crash (the temporary file left), the next `Create` removes it |
| `Test_AsyncIO_SameTargetWritesSerialised` | `Zenith_AsyncIO_Tests.cpp` | two writes to one target at one priority with two I/O threads land in submission order; a LOW write followed by a HIGH write of the same path, both queued, start HIGH first and the LOW write lands last; a queued `Delete` of a path whose `Rename` target is in flight waits; while the first is in flight the second thread completes an unrelated queued read and then sleeps on its semaphore (its wake count stays below 3: no spin); the passed-over write starts within one completion of the first |
| `Test_AsyncIO_DeleteRenameCreateDirectory` | `Zenith_AsyncIO_Tests.cpp` | delete of a missing file succeeds; rename across top-level directories returns `IO_ERROR_INVALID_REQUEST`; rename replaces atomically; `CreateDirectory` creates missing parents with the escape check |
| `Test_AsyncIO_LowDiskRefusesWrite` | `Zenith_AsyncIO_Tests.cpp` | a fake volume below the margin refuses the write, creates no file, and raises `PLATFORM_EVENT_LOW_DISK` at the next pump |
| `Test_AsyncIO_LowDiskStateMachine` | `Zenith_AsyncIO_Tests.cpp` | every cell of "Low-disk state machine": one event per entry into LOW, refusals counted while LOW, cleared only at twice the margin |
| `Test_AsyncIO_DiskFullMidWriteCleansUp` | `Zenith_AsyncIO_Tests.cpp` | an injected `ENOSPC` at the second chunk deletes the temporary file, reports `IO_ERROR_DISK_FULL`, enters LOW |
| `Test_AsyncIO_ReadsNeverRefusedForLowDisk` | `Zenith_AsyncIO_Tests.cpp` | in the LOW state reads, deletes and renames succeed |
| `Test_AsyncIO_ShutdownCancelsAndJoins` | `Zenith_AsyncIO_Tests.cpp` | destruction with 500 queued and 2 in-flight requests completes every operation (cancelled or succeeded) and joins both threads |
| `Test_AsyncIO_CancelStress1000Cycles` | `Zenith_AsyncIO_Tests.cpp` (`z2-stress`) | 1,000 issue-then-cancel cycles at randomised stages (queued, mid-chunk, mid-write) leave no open file, no temporary file, no operation slot and no allocation behind (`Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", "Cancellation completion") |
| `Test_AsyncIO_StressUnderTsan` | `Zenith_AsyncIO_Tests.cpp` (`z2-stress`, run under `linux-tsan` by the `Sanitizers` step) | 8 submitter threads, 2 and 4 I/O threads, 100,000 mixed-priority reads and writes over a temporary user directory with random cancellations and handle destructions: every operation completes exactly once, each destination guard pattern is intact, the statistics balance, and TSan and UBSan report nothing |
| `Test_AsyncIO_Create_FailsAtEachStep_Unwinds` | `Zenith_AsyncIO_Tests.cpp` | the fault sweep over queue allocation, thread creation and the stale-temporary sweep; no thread left running |

### Path-traversal fuzz corpus

The seed corpus of `Zenith_Fuzz_VirtualPath` lives in `Zenith2/Engine/ZenithPlatform/Tests/Fuzz/Corpus/VirtualPath/`, one input per file (the file name is the case's name), and the fuzz smoke run and the stage-acceptance run both start from it:

| Seed file | Input (shown escaped) | Expected |
|---|---|---|
| `dotdot` | `content:/../x` | `PATH_ERROR_TRAVERSAL` |
| `dotdot_mid` | `content:/a/../../b` | `PATH_ERROR_TRAVERSAL` |
| `dot` | `user:/saves/./a` | `PATH_ERROR_TRAVERSAL` |
| `double_sep` | `content:/a//b` | `PATH_ERROR_TRAVERSAL` |
| `trailing_sep` | `content:/a/` | `PATH_ERROR_TRAVERSAL` |
| `backslash` | `content:/a\..\b` | `PATH_ERROR_SEPARATOR` |
| `drive` | `content:/C:/Windows/win.ini` | `PATH_ERROR_ABSOLUTE` |
| `ads` | `user:/saves/a.zsave:stream` | `PATH_ERROR_ABSOLUTE` |
| `short_name` | `content:/PROGRA~1/x` | accepted by the parser; the fuzz harness's resolution pass against its fixture root rejects it on Windows with `PATH_ERROR_NAME_MISMATCH` |
| `leading_slash` | `content://etc/passwd` | `PATH_ERROR_ABSOLUTE` |
| `unc` | `\\?\C:\x` | `PATH_ERROR_BAD_ROOT` |
| `overlong_dot` | `content:/\xC0\xAE\xC0\xAE/x` | `PATH_ERROR_ENCODING` |
| `overlong_slash` | `content:/a\xC0\xAFb` | `PATH_ERROR_ENCODING` |
| `nul` | `content:/a\x00b` | `PATH_ERROR_ENCODING` |
| `c1` | `content:/a\xC2\x85b` | `PATH_ERROR_ENCODING` |
| `surrogate` | `content:/\xED\xA0\x80` | `PATH_ERROR_ENCODING` |
| `percent` | `content:/%2e%2e/x` | accepted (a literal file name `%2e%2e`) |
| `fullwidth_solidus` | `content:/a\xEF\xBC\x8Fb` | accepted (U+FF0F is not a separator) |
| `con` | `user:/saves/con` | `PATH_ERROR_RESERVED_NAME` |
| `con_ext` | `user:/saves/CoN.txt` | `PATH_ERROR_RESERVED_NAME` |
| `com_super` | `user:/saves/COM\xC2\xB9.zsave` | `PATH_ERROR_RESERVED_NAME` |
| `conin` | `user:/logs/CONIN$` | `PATH_ERROR_RESERVED_NAME` |
| `trailing_dot` | `user:/saves/a.` | `PATH_ERROR_RESERVED_NAME` |
| `trailing_space` | `user:/saves/a ` | `PATH_ERROR_RESERVED_NAME` |
| `user_namespace` | `user:/../../x` | `PATH_ERROR_TRAVERSAL` |
| `user_other_dir` | `user:/bin/x` | `PATH_ERROR_USER_NAMESPACE` |
| `user_crash` | `user:/crash/a.dmp` | accepted in every build |
| `user_devtools` | `user:/devtools/x` | accepted where `Zenith_IsShippingBuild()` is false; `PATH_ERROR_USER_NAMESPACE` at offset 6 in the Shipping test build |
| `too_long` | `content:/` followed by 504 `a` | `PATH_ERROR_TOO_LONG` |
| `long_component` | `content:/` followed by 256 `a` | `PATH_ERROR_TOO_LONG` |
| `many_components` | `content:/a` followed by 64 repetitions of `/a` | `PATH_ERROR_TOO_LONG` |
| `root_case` | `Content:/a` | `PATH_ERROR_BAD_ROOT` |
| `wildcard` | `content:/a*` | `PATH_ERROR_FORBIDDEN_CHARACTER` |

`Test_VirtualPath_CorpusExpectations` (in `Zenith_VirtualPath_Tests.cpp`) reads every seed file and asserts its expected result, so the corpus is also a regression table in the ordinary `Test` step; for `user_devtools` it asserts the expectation of the configuration it runs in, selected by `if constexpr (Zenith_IsShippingBuild())`, and the Shipping half is `Test_VirtualPath_UserDevtoolsRefusedInShipping`.

### linux-tsan async I/O stress

`Test_AsyncIO_StressUnderTsan` is change-triggered through the `Sanitizers -Preset linux-tsan` gate line of the `Zenith2Memory` category, whose paths must include `Zenith2/Engine/ZenithPlatform/Internal/AsyncIO/**` and `Zenith2/Engine/ZenithPlatform/Internal/Zenith_VirtualFileSystem*.cpp` for a PLAT diff to trigger it (a PROC edit of `zagent.project.json`, entry document → "Pending new rows"); until that edit lands, the stress runs in every stage acceptance's full sanitizer set (`Zenith2/Docs/Roadmap.md` → "Acceptance evidence").
