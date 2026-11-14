# SEC design: path-traversal protection and test matrix

This file is part of the SEC design family; the entry document is `Zenith2/Docs/Design/SEC.md`. It owns path-traversal protection as a trust-boundary control: the vectors an attacker can use, every source of a name or path in Zenith2, the matrix that names, for every applicable (source, vector) pair, the test that proves the vector is refused, the composed tests SEC adds, and the gate rule that confines OS file, process, socket and dynamic-loading APIs. The mechanism is PLAT's and is not re-specified: `Zenith2/Docs/Architecture/PlatformLayer.md` → "Path validation" ([Z2-N-ARCH-923], [Z2-N-ARCH-924], [Z2-N-ARCH-936]), `Zenith2/Docs/Architecture.md` → "Security architecture" ([Z2-N-ARCH-954]), and `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Validation rules and order", "Escape check after resolution", "Case policy", "Command-line host files".

## Scope of this file

| Topic | Heading |
|---|---|
| layered controls and where each sits | "Controls in depth" |
| the vector catalogue V1 to V19 | "Vector catalogue" |
| the source catalogue P1 to P20 | "Source catalogue" |
| the matrix | "The matrix" |
| SEC's own end-to-end tests | "Composed traversal tests" |
| the gate rule for raw OS APIs | "Confined OS APIs" |
| what the Stage 1 ruling confirms | "Rules confirmed by the ruling" |
| residuals and upkeep | "Accepted residuals for paths", "Matrix maintenance" |

## Controls in depth

| Layer | Control | Owner and heading |
|---|---|---|
| L1 syntax | `Zenith_ParseVirtualPath` rules R1 to R10 in order; no normalisation step, so an accepted string is canonical | PLAT, `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Validation rules and order", "Normalisation" |
| L2 namespace | `content:/` resolves through loose, patch and package and never the user mount; `user:/` resolves through the user mount alone and only below `config`, `saves`, `cache`, `logs`, `crash` and, outside Shipping, `devtools` (row SEC1, applied by PLAT's R10 patch) | PLAT, `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Resolution algorithm" |
| L3 resolution | the OS path is formed inside `ZenithPlatform/Internal/` alone; the opened handle's final path must lie inside the mount root's own final path; the case policy compares bytes of the final path | PLAT, `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Escape check after resolution", "Case policy" |
| L4 format names | names inside formats (archive entries, manifest file lists, DLC and pack names, slot names, cloud object names) are validated as single virtual-path components or by a stricter grammar before any lookup | ASSET A5, PACK "Name rules" and U0/U7/U8, SAVE `Zenith_ParseSaveSlotName`, ONLINE's object-name grammar |
| L5 grants | the only host paths outside the four mounts are command-line grants (non-Shipping), PACK's install file system and the directory sources PACK creates, each confined to its own root ([Z2-N-ARCH-923]) | PLAT, PACK |
| L6 creation | writes create new files only where a grant or the user mount allows; `CREATE_NEW` and `O_CREAT \| O_EXCL` for grant files; atomic writes through `.z2atomic` temporaries | PLAT, `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Atomic writes" |
| L7 API confinement | no Zenith2 source outside the allowlisted directories calls a raw OS file, process, socket or dynamic-loading API ("Confined OS APIs") | SEC, the `ForbiddenCheck` rule F24 (hand-off SEC-P4) |

## Vector catalogue

| # | Vector | Example inputs | Refused by |
|---|---|---|---|
| V1 | parent reference | `../x`, `a/../../b`, `..` | R7 |
| V2 | current reference or empty component | `./a`, `a//b`, `a/`, the root alone | R7 |
| V3 | absolute or root-confused path | `/etc/passwd`, `content://x`, a leading separator after the root | R3, R5 |
| V4 | drive letter, UNC or device namespace | `C:/x`, `\\server\share`, `\\?\C:\x`, `\\.\PhysicalDrive0` | R3, R4, R5 |
| V5 | alternate data stream | `a.zsave:stream`, `a:$DATA` | R5 |
| V6 | backslash separator | `a\..\b` | R4 |
| V7 | encoding tricks | overlong `C0 AE`, `C0 AF`; NUL; C0 and C1 controls; lone surrogates; code points above U+10FFFF; truncated sequences | R2 |
| V8 | reserved device names | `CON`, `nul.txt`, `COM¹`, `CONIN$`, `LPT9` in any case | R9 |
| V9 | trailing dot or space, leading space | `a.`, `a `, ` a` | R9 |
| V10 | length and count overflow | 513 bytes, a 256-byte component, 65 components | R1, R8 |
| V11 | link escape | a symbolic link, junction or mount point inside a mount root pointing outside it | the escape check (L3) |
| V12 | hard link | a hard link inside a mount root to a file outside it | not refused: residual R1 ("Accepted residuals for paths") |
| V13 | aliasing | a case variant of an existing name on Windows; an NTFS 8.3 short name (`PROGRA~1`) | the case policy (L3) |
| V14 | look-alike and encoded characters | `%2e%2e`, U+FF0F full-width solidus, U+2215, `~` | nothing decodes them: they stay literal names, never separators or parent references |
| V15 | namespace escape | `user:/bin/x`; a user-mount file at a content path's relative name; mounting a user directory into another mount | R10 and the resolution algorithm (L2) |
| V16 | check-to-use race | a link swapped in between validation and open | L3 checks the handle actually opened, never the name |
| V17 | archive or bundle entry slip | an archive entry `../x`; a manifest entry `sub/x` or `..\x`; an unlisted file or subdirectory in a bundle | ASSET A5 and A6; PACK U0 and U7 |
| V18 | overwrite or truncation of an existing file | a grant naming an existing file for writing; a name that exists in `--evidence-dir` | L6 |
| V19 | wildcard and reserved punctuation (the wildcard-injection vector of `FindFirstFileW` and shells) | `a*b`, `a?b`, `<x>`, `a"b`, `a\|b` | R6 |

## Source catalogue

Every way a name or path enters Zenith2 code:

| # | Source | Validated by | Builds |
|---|---|---|---|
| P1 | `content:/` paths in cooked data, scripts and configuration | L1, L2, L3 | every build |
| P2 | `user:/` paths: save slots, configuration, caches, logs | L1, L2, L3 | every build |
| P3 | `.zpak` archive entry names | ASSET A5 and A6, then L1 | every build |
| P4 | the mount manifest's `package[].archive`, `assetPacks[].archives`, `patchRoot` and pack names | PLAT's manifest rules (R2 to R9 on one component; the pack-name grammar) | every build |
| P5 | update and DLC manifest file names, DLC names, patch directory names | PACK U0, U7, U8 and "Name rules"; PLAT's patch-directory parse | every build |
| P6 | Play's `AssetPackLocation.assetsPath()` | a directory source confined to that root by L3 | Android |
| P7 | PACK's install file system: `Patches/`, `DLC/`, `.zenith2-install/`, `Content/mount-manifest.json` | the install file system's own escape check, no link followed ([Z2-N-ARCH-923]) | every build |
| P8 | command-line host files `--record`, `--replay`, `--input-script`, `--config-dump` | `Zenith_CommandLineFileGrant` constructed by `Zenith_CommandLine` alone; OS canonicalisation; UTF-8 without controls | non-Shipping |
| P9 | command-line host directories `--evidence-dir`, `--script-migration-fixtures` | directory grants; single-component names (R2, R4, R5, R6, R7, R9); create-new only (evidence), read-only (fixtures) | non-Shipping |
| P10 | `--assets-root` | the loose mount root; L3 below it | non-Shipping |
| P11 | development file writer names (DEVT's `asset.dump`, telemetry, captures and evidence, CINE's captures, ADVGFX's `r.vt.dump`) and the remote `FILE_LIST` and `FILE_GET` paths | DEVT's segment rules; namespace `user:/devtools/` and `user:/crash/` (row SEC1) | non-Shipping (file list, get, writer); crash reports every build |
| P12 | file-watcher change paths | mapped back to `content:/` and dropped when `Zenith_ParseVirtualPath` fails | non-Shipping |
| P13 | ONLINE cloud object names and `online.offline.cloudRoot` | ONLINE's object-name grammar; one component of `[a-z0-9_-]`, 1 to 32 bytes, not an R9 name | every build |
| P14 | Luau module names and `require` literals | SCRIPT's manifest validation and literal-only `require` | every build |
| P15 | the crash helper's crash directory and file stem | derived by the helper itself, never received ([Z2-N-ARCH-965]) | Windows, every build |
| P16 | glTF external `uri` values | CONTENT's read callback with relative URIs only, the 4,096-byte URI limit, resolution below the source's directory | tools |
| P17 | editor and automation file arguments, live-link step paths, source-control output paths | EDIT's command argument validation; game-relative paths under the game root | tools |
| P18 | save slot names | `Zenith_ParseSaveSlotName`: lower-case `[a-z0-9_-]` | every build |
| P19 | `package.simulatedDelivery.root` | a directory source confined to that root | non-Shipping |
| P20 | console command path arguments (`ecs.dump <worldTag> <path>`) | DEVT's development file writer, below `user:/devtools/` | non-Shipping |

## The matrix

A cell names the test that proves the vector is refused for the source; `—` means the vector cannot be expressed through the source (stated in the "Why not applicable" column). V12 (hard links) is refused by no layer and is the accepted residual R1 ("Accepted residuals for paths"), so its cell is `— (residual R1)` in every runtime row and `notApplicable: "residual R1"` in the registry. Every named test is the owner's unless it begins `Test_Sec`, which is SEC's ("Composed traversal tests"). SecurityConformance check SC13 verifies that every test named here exists (`Zenith2/Docs/Design/SEC/ConformanceCheck.md` → "Checks").

### Matrix rows for runtime sources

| Source | V1–V2, V6 | V3–V5 | V7 | V8–V9 | V19 | V10 | V11, V16 | V12 | V13 | V14 | V15 | V17 | V18 | Why not applicable |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| P1 | `Test_VirtualPath_EachRuleRejects`, `Test_VirtualPath_CorpusExpectations` | `Test_VirtualPath_EachRuleRejects` | `Test_VirtualPath_EncodingEdgeCases` | `Test_VirtualPath_ReservedNamesEveryForm` | `Test_VirtualPath_EachRuleRejects` | `Test_VirtualPath_EachRuleRejects` | `Test_VirtualFileSystem_SymlinkEscapeRejected`, `Test_VirtualPath_ResolvedNeverEscapesRoot` | — (residual R1) | `Test_VirtualFileSystem_CaseMismatchOnWindows`, `Test_VirtualFileSystem_ShortNameAliasRejected` | `Test_VirtualPath_CorpusExpectations` | `Test_VirtualFileSystem_UserNeverShadowsContent` | — | — | V17: not a container; V18: `content:/` is read-only |
| P2 | `Test_VirtualPath_EachRuleRejects` | `Test_VirtualPath_EachRuleRejects` | `Test_VirtualPath_EncodingEdgeCases` | `Test_VirtualPath_ReservedNamesEveryForm` | `Test_VirtualPath_EachRuleRejects` | `Test_VirtualPath_EachRuleRejects` | `Test_SecPath_UserMountLinkEscapeRefused` | — (residual R1) | `Test_VirtualFileSystem_CaseMismatchOnWindows` | `Test_VirtualPath_CorpusExpectations` | `Test_SecPath_UserNamespaceEnforced` | — | `Test_AsyncIO_AtomicWriteInterrupted` | V17: not a container |
| P3 | `Test_AssetArchive_EachCheckRejects`, `Test_SecPath_ArchiveEntryTraversalRefused` | `Test_SecPath_ArchiveEntryTraversalRefused` | `Test_SecPath_ArchiveEntryTraversalRefused` | `Test_SecPath_ArchiveEntryTraversalRefused` | `Test_SecPath_ArchiveEntryTraversalRefused` | `Test_AssetArchive_LimitsAtAndAboveLimit` | — | — (residual R1) | `Test_SecPath_ArchiveEntryCaseIsExact` | `Test_SecPath_ArchiveEntryTraversalRefused` | — | `Test_AssetArchive_EachCheckRejects` | — | V11, V16: entries are table names, no OS path; V15: archives serve `content:/` only; V18: read-only |
| P4 | `Test_MountManifest_EachRule`, `Test_SecPath_MountManifestArchiveNameRefused` | `Test_SecPath_MountManifestArchiveNameRefused` | `Test_MountManifest_EachRule` | `Test_SecPath_MountManifestArchiveNameRefused` | `Test_SecPath_MountManifestArchiveNameRefused` | `Test_MountManifest_EachRule` | `Test_SecPath_MountManifestArchiveLinkRefused` | — (residual R1) | — | — | — | — | — | V13: names are compared after L3 opens the archive; V14: literal; V15: one mount only; V17: names are single components; V18: read-only |
| P5 | `Test_UpdateApplier_UnlistedEntry_Refused`, `Test_SecPath_UpdateManifestEntryNameRefused` | `Test_SecPath_UpdateManifestEntryNameRefused` | `Test_SecPath_UpdateManifestEntryNameRefused` | `Test_SecPath_UpdateManifestEntryNameRefused` | `Test_SecPath_UpdateManifestEntryNameRefused` | `Test_SecPath_UpdateManifestEntryNameRefused` | `Test_UpdateApplier_BundleWithJunction_DeletesLinkOnly` | — (residual R1) | — | — | `Test_SecPath_DlcNameMismatchRefused` | `Test_UpdateApplier_UnlistedEntry_Refused`, `Test_SecPath_PatchDirectoryNameIgnored` | — | V13: names are lower-case grammar; V14: literal; V18: the applier only renames sealed bundles |
| P6 | — | — | — | — | — | — | `Test_SecPath_PackLocationLinkEscapeRefused` | — (residual R1) | — | — | — | — | — | the root is OS-provided and only L3 applies; names inside are archive entries (P3) |
| P7 | `Test_HostInstallFileSystem_PathEscapeRefused` | `Test_HostInstallFileSystem_PathEscapeRefused` | `Test_HostInstallFileSystem_PathEscapeRefused` | `Test_HostInstallFileSystem_PathEscapeRefused` | — | `Test_HostInstallFileSystem_PathEscapeRefused` | `Test_MemoryInstallFileSystem_Links_ReportedNotFollowed`, `Test_SecPath_JunctionInPatchRootRefused` | — (residual R1) | — | — | — | — | — | V13–V15, V19: paths are built from validated names; V17, V18: covered at P5 |
| P13 | `Test_OnlineCloudFormats_ObjectNameGrammar` | `Test_OnlineCloudFormats_ObjectNameGrammar` | `Test_OnlineCloudFormats_ObjectNameGrammar` | `Test_OnlineConformance_InvalidNamesRefused` | `Test_OnlineCloudFormats_ObjectNameGrammar` | `Test_OnlineCloudFormats_ObjectNameGrammar` | — | — (residual R1) | — | — | `Test_OnlineOffline_NoCloudWithoutRoot` | — | `Test_OnlineCloud_UploadNameCollisionVerified` | V11, V16: resolved through P2; V13, V14: the grammar admits lower-case ASCII only |
| P14 | `Test_Script_Module_ManifestValidation` | `Test_Script_Module_ManifestValidation` | `Test_Script_Module_ManifestValidation` | `Test_SecScript_ModuleNameRules` | `Test_SecScript_ModuleNameRules` | `Test_Script_Module_ManifestValidation` | — | — (residual R1) | — | `Test_Script_Module_RequireLiteralOnly` | `Test_SecScript_BytecodeOnlyFromPackageAndPatch` | — | — | V11, V16: resolved through P1; V17: modules are manifest entries, not a container; V18: read-only |
| P15 | — | — | `Test_CrashHelper_SectionStringsBoundedAndEscaped` | — | — | `Test_CrashHelper_SectionStringsBoundedAndEscaped` | `Test_CrashHelper_ForgedSectionRefused` | — (residual R1) | — | — | — | — | — | the helper receives no path ([Z2-N-ARCH-965]) |
| P18 | `Test_SaveSlotName_Rules` | `Test_SaveSlotName_Rules` | `Test_SaveSlotName_Rules` | `Test_SaveSlotName_Rules` | `Test_SaveSlotName_Rules` | `Test_SaveSlotName_Rules` | — | — (residual R1) | `Test_SecPath_SaveSlotNameRefused` | `Test_SaveSlotName_Rules` | `Test_SecPath_SaveSlotNameRefused` | — | — | V11, V16: resolved through P2; V17: not a container; V18: atomic writes (P2) |

### Matrix rows for development and tool sources

| Source | Applicable vectors | Tests | Why the others are not applicable |
|---|---|---|---|
| P8 | V3, V4, V7, V11, V18 | `Test_CommandLineHostFile_GrantOnly`, `Test_InputScript_LoadsFromGrantOrContentPath` | a grant is a local operator path, canonicalised by the OS, so V1, V2 and V6 are the operator's choice and not a traversal; the grant cannot be produced from content (V15) |
| P9 | V1, V2, V3, V4, V5, V6, V7, V8, V9, V18, V19 | `Test_BudgetRunRecord_EvidenceFilesAppendOnly`, `Test_SecPath_EvidenceDirNameRefused` | names are single components |
| P10 | V11, V16 | `Test_VirtualFileSystem_SymlinkEscapeRejected`, `Test_VirtualFileSystem_LooseAbsentInShipping` | the root is the operator's choice; paths below it are P1 |
| P11 | V1, V2, V3, V4, V5, V6, V7, V8, V9, V15, V19 | `Test_DevFileWriter_NameRules`, `Test_DevToolsServer_FileGetRestricted` | resolved through P2 |
| P12 | V1 to V10, V19 | `Test_FileWatcher_CoalescesAndMapsToVirtualPaths` | every reported path is re-parsed as P1 and dropped on failure |
| P16 | V1, V3, V4, V7, V10, V11 | `Test_GltfSource_ExternalBuffersThroughCallback`, `Test_GltfSource_UriEscapeRefused` (pending, row SEC9) | CONTENT resolves below the source file's directory |
| P17 | V1, V3, V4, V7, V11 | `Test_EditorHostPaths_ResolveFromExecutable`, `Test_EditorLiveLink_LoopbackOnly` | paths are game-relative and the editor is a tool |
| P19 | V11, V16 | `Test_SecPath_PackLocationLinkEscapeRefused` (the same directory source) | the root is a configuration key of non-Shipping builds |
| P20 | V1 to V9, V15, V19 | `Test_DevFileWriter_NameRules` | the argument passes through the development file writer |

## Composed traversal tests

SEC's tests exercise a whole path from the hostile name to the refusal, through real objects of several libraries on the headless path. They live in `Zenith2/Tests/Security/Zenith_SecPath_Tests.cpp` (suite `Zenith_Integration_Security`, `Zenith2/Docs/Design/SEC/Tests.md` → "Path-matrix composition tests"), build every hostile fixture in the test from the documented byte layouts or JSON (no committed binary), and run in the `Test` step on `win-debug`, `win-release` and `linux-tsan` (tests marked Windows run there only):

| Test | Asserts |
|---|---|
| `Test_SecPath_ArchiveEntryTraversalRefused` | an archive whose table holds one entry from each of `../x`, `a//b`, `/x`, `C:/x`, `a:s`, `a\b`, `\xC0\xAE\xC0\xAE/x`, `CON`, `a.`, `a*b`, `a?b`, `%2e%2e/x` (the last valid, the others not), built with ASSET's archive layout, is refused by `Zenith_CreateArchiveMountSource` with `FILE_ERROR_ARCHIVE_CORRUPT` for each invalid entry, and the archive with only `%2e%2e/x` mounts and resolves `content:/%2e%2e/x` to a literal entry |
| `Test_SecPath_ArchiveEntryCaseIsExact` | an archive holding `Textures/Rock.ztex` resolves that path and not `textures/rock.ztex`, on every host |
| `Test_SecPath_MountManifestArchiveNameRefused` | a mount manifest whose `package[0].archive` is in turn `../base.zpak`, `sub/base.zpak`, `C:base.zpak`, `CON.zpak`, `base*.zpak` and `base.zpak.` fails with `FILE_ERROR_MANIFEST_INVALID` naming `package[0].archive`, and the package mount is left empty |
| `Test_SecPath_MountManifestArchiveLinkRefused` | an archive name that is a symbolic link (Windows: a junction to a directory, a file symbolic link) to a file outside `Content/` fails the escape check and is not mounted |
| `Test_SecPath_UpdateManifestEntryNameRefused` | a correctly signed `update.zman` (signed in process with RFC 8032's TEST 1 seed through PACK's test signer) whose file table lists one of `../evil.zpak`, `sub/x.zpak`, `C:x`, `x:s`, `x\y`, an overlong-encoded dot, `NUL.zpak`, `x?.zpak`, `x.` or a 256-byte name is refused at U7 with `UPDATE_MANIFEST_INVALID`, and no listed file is hashed |
| `Test_SecPath_DlcNameMismatchRefused` | `DLC/expansion_isles/dlc.zman` whose name field is `expansion_isles2`, `../x` or `EXPANSION_ISLES` is refused at U8 and the DLC is not mounted |
| `Test_SecPath_PatchDirectoryNameIgnored` | directories `1.0.2/../1.0.3`, `1.0`, `01.0.1`, `1.0.1 ` and `1.0.1.` under `Patches/` are ignored with a warning, never opened |
| `Test_SecPath_JunctionInPatchRootRefused` | (Windows) a junction `Patches/1.0.4` to a directory outside the install root, holding a validly signed manifest, is never followed: the install file system refuses it and the patch mount does not mount it; (Linux) the same with a symbolic link |
| `Test_SecPath_PackLocationLinkEscapeRefused` | a directory source created over a temporary pack location containing a link to outside it refuses every read through the link with `FILE_ERROR_ESCAPES_MOUNT` |
| `Test_SecPath_UserMountLinkEscapeRefused` | a link planted in a temporary user directory (`saves/x.zsave` pointing outside) fails reads and writes with `PATH_ERROR_ESCAPES_MOUNT`, and an atomic write through it creates nothing outside the root |
| `Test_SecPath_UserNamespaceEnforced` | `user:/bin/x`, `user:/Saves/x` and `user:/../x` are refused with their rules' errors; `user:/saves/x`, `user:/config/x`, `user:/cache/x` and `user:/logs/x` parse |
| `Test_SecPath_SaveSlotNameRefused` | slot names `../a`, `A`, `con`, `a.b`, `a/b` and a 65-byte name are refused by the save system's write and load entry points with their `Zenith_SaveError`, and nothing is written |
| `Test_SecPath_EvidenceDirNameRefused` | `Zenith_CreateFileInCommandLineHostDirectory` with `../x`, `a/b`, `a\b`, `C:x`, `CON`, `a?b` and an existing name each fails, and the directory is unchanged |
| `Test_SecScript_ModuleNameRules` | a module manifest naming a module `../Engine/x`, `x.luau.`, `x\|y` or `CON` is refused by `Zenith_ScriptModuleManifest::Load` |
| `Test_SecScript_BytecodeOnlyFromPackageAndPatch` | in the `ShippingTest` configuration, a module present only in a loose-mount directory is not found, and the same module in a package archive loads |

## Confined OS APIs

### Confined API classes

Rule F24 of `ForbiddenCheck` (hand-off SEC-P4; `Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck rule table"; core-doc row X414 adds it to `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates", table "ForbiddenCheck rules", beside PROC's F22 and F23) rejects these calls in every Zenith2 source outside the directories its allowlist names, with comments and string literals stripped and tokens matched as whole identifiers, like rules F1 to F21.

- A token is a hit when it is called (the identifier, optional blanks, then `(`) or its address is taken (`&` then the identifier).
- An identifier preceded by `.`, `->` or a `::` whose left side is not `std` (with optional blanks between) is a member or another scope's name and is not a hit, so `xFile.open(…)`, `pxSocket->bind(…)` and `Zenith_Path::remove(…)` never match, while `std::remove(…)`, `::open(…)` and a bare `open(…)` do.
- A token ending `*` in the table matches every identifier with that prefix (`_spawn*` matches `_spawnl` and `_spawnvpe`).

| Class | Tokens |
|---|---|
| file | `CreateFileA`, `CreateFileW`, `CreateFile2`, `OpenFile`, `NtCreateFile`, `fopen`, `fopen_s`, `_wfopen`, `_wfopen_s`, `freopen`, `open`, `_open`, `_wopen`, `openat`, `creat`, `opendir`, `FindFirstFileW`, `FindFirstFileExW`, `CreateDirectoryW`, `DeleteFileW`, `RemoveDirectoryW`, `CopyFileW`, `CopyFile2`, `MoveFileW`, `MoveFileExW`, `ReplaceFileW`, `CreateHardLinkW`, `CreateSymbolicLinkW`, `rename`, `renameat`, `unlink`, `unlinkat`, `remove`, `mkdir`, `rmdir`, `symlink`, `link`, `AAssetManager_open` |
| process | `CreateProcessA`, `CreateProcessW`, `CreateProcessAsUserW`, `ShellExecuteW`, `ShellExecuteExW`, `WinExec`, `system`, `_wsystem`, `popen`, `_popen`, `_spawn*`, `_wspawn*`, `fork`, `vfork`, `execv`, `execve`, `execvp`, `posix_spawn`, `posix_spawnp` |
| socket | `socket`, `bind`, `listen`, `accept`, `accept4`, `connect`, `WSAStartup`, `WSASocketW`, `getaddrinfo` |
| dynamic loading | `LoadLibrary`, `LoadLibraryA`, `LoadLibraryW`, `LoadLibraryEx`, `LoadLibraryExA`, `LoadLibraryExW`, `GetProcAddress`, `dlopen`, `dlsym` |

The dynamic-loading class's allow records are exactly the directories of PROC's rule F22 and [Z2-N-ARCH-274]: `ZenithPlatform/Internal/` and every `ZenithRHI_*/Internal/`. A `LoadLibrary*` or `dlopen` call is therefore allowed or refused by F22 and F24 alike, and PROC's step reports a call both rules match as one finding carrying both IDs (`F22+F24`; `Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck rule table"); F24 alone covers `GetProcAddress` and `dlsym`, in the same two directories.

### confined_apis.json

`Zenith2/Tools/Gates/confined_apis.json` (protected; created and changed only by PROC tickets):

```json
{
    "schema": "zenith2.confined-apis",
    "schemaVersion": 1,
    "classes": {
        "file": ["CreateFileW", "fopen"],
        "process": ["CreateProcessW"],
        "socket": ["socket", "bind"],
        "dynamic-loading": ["LoadLibraryW", "dlopen"]
    },
    "allow": [
        { "class": "file", "dir": "Zenith2/Engine/ZenithPlatform/Internal/Windows/", "owner": "PLAT", "decision": "Z2-D-042", "why": "the host-file table behind the virtual filesystem" },
        { "class": "socket", "dir": "Zenith2/Engine/ZenithPlatform/Internal/DevServices/", "owner": "PLAT", "decision": "Z2-D-042", "why": "the loopback development listener and client" }
    ]
}
```

- `classes` holds the token lists of the table above (the example shows a subset); `allow` holds one record per (class, directory) pair, each a repo-root-relative directory ending `/`, the owning area, a decision ID that exists in `Zenith2/Docs/DecisionLog.md` (Z2-D-042, the hard-gate decision this rule extends, until the Stage 1 security-architecture ruling's `Z2-D-NNN` exists; that ruling's ticket re-points every record to it), and the reason.
- The initial allow records: file APIs in `ZenithPlatform/Internal/Windows/`, `Internal/Android/`, `Internal/Linux/` (the host-file table), `ZenithPackage/Internal/Windows/`, `Internal/Android/`, `Internal/Linux/` (the install file system), `ZenithCore/Internal/Windows/`, `Internal/Android/`, `Internal/Linux/` (the test child-process facility of ruling X349, its read-own-binary half), `Zenith2/Tools/**` and every `Tests/**` directory (tools and tests use the STL and host files freely); process APIs in `ZenithPlatform/Internal/Windows/CrashHandler/` (the crash helper launch), `ZenithCore/Internal/Windows/`, `Internal/Linux/`, `Internal/Android/` (the test child-process facility), `Zenith2/Tools/**`; socket APIs in `ZenithPlatform/Internal/DevServices/` (the development listener and client), `ZenithPlatform/Internal/Windows/`, `Internal/Android/` and `Internal/Linux/` (PLAT's Shipping-capable `Zenith_DatagramSocket` of row NT1, which NET's transport uses) and the ONLINE backends' `Internal/` directories that need them (none in Stage 4; the Steam and Google Play backends declare theirs in their own tickets); dynamic-loading APIs in `ZenithPlatform/Internal/` and `ZenithRHI_*/Internal/` (volk's loader, the Agility SDK and the debug-layer queries), as stated under "Confined API classes".
- Third-party sources are not Zenith2 sources and are not scanned (their roots are excluded as F16 excludes them).

## Rules confirmed by the ruling

The Stage 1 security-architecture ruling confirms, as PLAT's revalidation ticket requires (row PB6 of the A5 cross-area blockers):

1. Rules R1 to R10 in their order, with the error of the first failing rule reported, and no normalisation step.
2. The escape check after resolution on the opened handle, and the case policy's byte comparison on Windows.
3. The namespace of R10, extended by row SEC1 with `crash` (every build) and `devtools` (non-Shipping builds only, where `Zenith_IsShippingBuild()` is false; in Shipping a `devtools` first component fails with `PATH_ERROR_USER_NAMESPACE`), which admits every current writer of `user:/devtools/`: DEVT (`asset.dump`, telemetry, captures, evidence), CINE (captures) and ADVGFX (`r.vt.dump`), each through the development file writer.
4. The exceptions of [Z2-N-ARCH-923] as the complete list of host paths outside the four mounts.
5. The confined-API rule F24 and its initial allowlist.

## Accepted residuals for paths

- R1 (hard links): a hard link inside a mount root to a file outside it is not detectable by path; planting one needs write access to the mount root, which the install-root, device and same-user preconditions already grant (`Zenith2/Docs/Design/SEC/TrustBoundaries.md` → "Trust preconditions"). PLAT documents it in the escape-check source.
- The loose mount and every command-line grant exist only in non-Shipping builds; a developer who passes a path chose it.

## Matrix maintenance

- A ticket that adds a source of names or paths adds its row to "Source catalogue" and "The matrix" in the same diff, with the tests that fill each applicable cell.
- A ticket that renames a test the matrix names edits the matrix in the same diff; SecurityConformance check SC13 fails while a named test does not exist.
- A new vector is added only through SEC's design and the Stage 1 ruling's successor decision, and every runtime row then gains its cell.
