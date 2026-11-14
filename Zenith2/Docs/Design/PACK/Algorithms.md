# PACK design: algorithms and state machines

This file is part of the PACK design family; the entry document is `Zenith2/Docs/Design/PACK.md`. It specifies the crash-consistency model, the install and recovery state machines of the update applier, the install lock and instance guard, the asset-pack state machine of `Zenith_AssetPackInstaller`, the package service's startup, update classification and installed-update migration, the update compatibility rules, the partition, chunking and dictionary algorithms of the packer, the patch diff, event publication, and the consistency invariants every test asserts. Types are `Zenith2/Docs/Design/PACK/PublicAPI.md`; formats are `Zenith2/Docs/Design/PACK/Formats.md`.

## Scope of this file

| Topic | Heading |
|---|---|
| what survives power loss and a kill, and how each step is made durable | "Durability model" |
| the applier: install, reject, clean, remove, sweep, recover | "Install state machine", "Recovery table", "Pending installs at startup" |
| one installer at a time, and never under a running instance | "Install lock and instance guard" |
| Android content updates (through Play only) | "Android content updates" |
| asset-pack delivery, verification, mounting, removal, the shutdown drain | "Asset pack state machine" and its subsections, "Shutdown drain" |
| the service's creation at the close of startup step 11, its entitlement provider and Windows DLC | "Package service startup", "Entitlement provider", "Windows DLC mounting" |
| first run, patch, update, downgrade | "Update classification", "Installed-update migration" |
| what an update may change | "Update compatibility rules" |
| the packer | "Partition algorithm", "Chunking", "Dictionary policy", "Patch diff" |
| simulation visibility, the snapshot a new world receives, the content a replay mounts | "Event publication and replay", "World snapshot", "Replay pack set" |
| what "consistent" means | "Consistency invariants" |

## Durability model

<!-- elaboration -->
Two interruptions are distinguished, because they lose different things and both must be survived:

| Interruption | What survives | Modelled in tests by |
|---|---|---|
| process kill (task manager, `adb shell am kill`, a crash, the Android low-memory killer) | every write and every metadata operation the process issued, because the OS page cache and the file system's in-memory metadata outlive the process; open handles and every lock the process held are released | `Zenith_MemoryInstallFileSystem::SimulateProcessKill` |
| power loss (battery pulled, OS crash) | file data made durable by `FlushFile`; metadata operations up to a point in issue order that is at least the last `FlushDirectory` | `Zenith_MemoryInstallFileSystem::SimulatePowerLoss`, `SimulatePowerLossAfterMetadata` |

The supported file systems (NTFS and ReFS on Windows; ext4 and F2FS on Android; ext4, XFS, Btrfs and tmpfs on the `linux-tsan` host) all keep metadata in an ordered log: NTFS's `$LogFile`, ext4's jbd2 journal, F2FS's checkpointed node chain, XFS's and Btrfs's logs. Two facts follow and the model is exactly them:

1. **A metadata operation is atomic.** Creating a file, removing a file or a link, creating or removing a directory, and renaming a file or a directory each either happened or did not after a power loss. A rename is one log record: after a power loss the entry is at the old name or at the new one, never at both and never at neither, and no directory is ever referenced by two parents.
2. **Metadata persists in issue order.** After a power loss the surviving metadata operations are a prefix of the operations issued, in issue order: every operation before the last completed `FlushDirectory` (of any directory) survives, and the operations after it survive up to an arbitrary point. File data is not ordered with metadata: bytes written since a file's last `FlushFile` are lost, so a file whose creation survived holds the bytes of its last `FlushFile`, or zero bytes when it was never flushed (the delayed-allocation outcome of ext4 and F2FS, and the lazy writer's of NTFS).

The model never needs, and the applier never relies on, the stronger accident that any directory's flush commits the whole log: every durable step still flushes exactly the directories POSIX requires (the parent of every created, removed or renamed entry, both parents of a rename), and `Test_UpdateApplier_FlushesEveryAffectedParent` checks that from the memory file system's operation trace. One exemption is part of the rule: an entry whose parent directory is itself removed later in the same durable step (every entry inside a trash tree, which "delete a trash tree" empties depth first) needs no flush of that parent, because after a power loss the surviving prefix either still holds the parent, which recovery deletes again (row 9, TRASHED with its trash name present), or no longer reaches it; only the parent of the tree's root, `<kind root>/.trash`, is flushed, once, at the end of the step. A volume of any other file system (FAT32, exFAT, a network share) gives neither fact; the applier refuses it ("Pending installs at startup" step 1).

The memory file system implements the model:

- It numbers every metadata operation (`Open` with `INSTALL_OPEN_MODE_CREATE_NEW`, `Rename`, `RemoveFile`, `RemoveLink`, `CreateDirectory`, `RemoveEmptyDirectory`, and `CreateLink` in tests) from 1 in issue order and keeps them in a log; `Write`, `Truncate` and file length changes are data, not metadata.
- `FlushDirectory` records the durable metadata count D as the number of metadata operations issued so far; `FlushFile` copies the file's current bytes to its durable bytes.
- `SimulatePowerLossAfterMetadata(k)` rebuilds the tree from the state at creation by replaying metadata operations 1 … max(k, D) (k beyond the issued count is clamped to it), gives every file its durable bytes (zero bytes when never flushed), discards every node no directory reaches, and releases every open file and every lock. `SimulatePowerLoss()` is `SimulatePowerLossAfterMetadata(D)`.
- `SimulateProcessKill()` keeps every operation and every byte and releases every open file and lock of every process view.
- `SetRootReadOnly(true)` makes every mutation return `INSTALL_ROOT_READ_ONLY` and models the host's read-only fallback of `AcquireLock` exactly (a SHARED request on an existing lock file succeeds, a missing file or an EXCLUSIVE request is `INSTALL_ROOT_READ_ONLY`).
- A fixture written with `WriteWholeFile(…, MEMORY_FILE_DURABILITY_DURABLE)` is part of the state at creation (an installed patch from an earlier session); one written with `MEMORY_FILE_DURABILITY_UNFLUSHED` is created through ordinary `CreateDirectory`, `Open`, `Write` and `Close` calls with no flush, which is how a launcher, an unzip or a person delivers a bundle.

Host mapping (`Internal/Windows/Zenith_HostInstallFileSystem_Windows.cpp`, `Internal/Linux/…`, `Internal/Android/…`):

| Operation | Windows | Linux and Android |
|---|---|---|
| `Open` | `CreateFileW` with `FILE_SHARE_READ \| FILE_SHARE_WRITE \| FILE_SHARE_DELETE`, `FILE_FLAG_OPEN_REPARSE_POINT` (a link is opened as itself and refused, `PATH_INVALID`); READ is `GENERIC_READ`, CREATE_NEW and READ_WRITE are `GENERIC_READ \| GENERIC_WRITE` (`FlushFileBuffers` needs `GENERIC_WRITE`); a READ_WRITE open of a file carrying `FILE_ATTRIBUTE_READONLY` clears that attribute first (`SetFileAttributesW`); then the escape check of `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Escape check after resolution" on the handle (`GetFinalPathNameByHandleW` under the root) | `openat` component by component from the root's descriptor with `O_NOFOLLOW \| O_CLOEXEC` (a link component fails `ELOOP`: `PATH_INVALID`); then the `/proc/self/fd` escape check |
| `FlushFile` | `FlushFileBuffers` | `fsync` |
| `FlushDirectory` | `CreateFileW(dir, GENERIC_READ \| GENERIC_WRITE, share all, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS)` then `FlushFileBuffers` (both rights and the flag are required) | `open(dir, O_RDONLY \| O_DIRECTORY \| O_NOFOLLOW)` then `fsync` |
| `Rename` | `MoveFileExW(from, to, bReplace ? MOVEFILE_REPLACE_EXISTING : 0)`; durability comes from the `FlushDirectory` calls that follow, never from `MOVEFILE_WRITE_THROUGH`, which only affects a move performed as a copy and a delete | `renameat` (bReplace false: `renameat2(RENAME_NOREPLACE)`, or a prior `fstatat` returning `ENOENT` where the kernel lacks it) |
| `RemoveFile` | `DeleteFileW`, after clearing `FILE_ATTRIBUTE_READONLY` | `unlinkat(…, 0)` |
| `RemoveLink` | `RemoveDirectoryW` on a directory junction or directory symbolic link (removes the reparse point, never the target), `DeleteFileW` on a file symbolic link | `unlinkat(…, 0)` |
| `RemoveEmptyDirectory` | `RemoveDirectoryW` | `unlinkat(…, AT_REMOVEDIR)` |
| `List`, `Stat` | `FindFirstFileExW(FindExInfoBasic)`: `m_bLink` from `FILE_ATTRIBUTE_REPARSE_POINT`, `m_bDirectory` only for a non-link directory | `fstatat(…, AT_SYMLINK_NOFOLLOW)`: `m_bLink` from `S_ISLNK` |
| `QueryVolumeKind` | `GetVolumeInformationByHandleW` on the root: `NTFS`, `ReFS` | `fstatfs` on the root: `f_type` `0xEF53` ext4, `0xF2F52010` F2FS, `0x58465342` XFS, `0x9123683E` Btrfs, `0x01021994` tmpfs |
| `GetIdentityStamp` | `GetFileInformationByHandleEx(FileBasicInfo)` (`LastWriteTime`, `ChangeTime`, converted to ns since the Unix epoch) and `(FileIdInfo)` (volume serial number, 128-bit file ID) on the handle, `GetFileSizeEx` for the size | `fstat`: `st_size`, `st_mtim`, `st_ctim`, `st_dev`, `st_ino` |
| `AcquireLock` | `LockFileEx` on byte 0 of the lock file (opened `GENERIC_READ \| GENERIC_WRITE`, `OPEN_ALWAYS`, share all), `LOCKFILE_FAIL_IMMEDIATELY`, plus `LOCKFILE_EXCLUSIVE_LOCK` for exclusive; `ERROR_LOCK_VIOLATION` is `INSTALL_LOCKED`. Read-only fallback: when that open fails with `ERROR_ACCESS_DENIED` or `ERROR_WRITE_PROTECT`, a SHARED request reopens an existing file `GENERIC_READ`, `OPEN_EXISTING` and locks it shared (`LockFileEx` needs only read access); a missing file, or an EXCLUSIVE request, is `INSTALL_ROOT_READ_ONLY` | `open(O_RDWR \| O_CREAT \| O_CLOEXEC, 0600)` and `flock(LOCK_EX \| LOCK_NB)` or `flock(LOCK_SH \| LOCK_NB)`; `EWOULDBLOCK` is `INSTALL_LOCKED`. Read-only fallback: on `EACCES` or `EROFS`, a SHARED request opens an existing file `O_RDONLY \| O_CLOEXEC` and `flock(LOCK_SH \| LOCK_NB)` (`flock` works on a read-only descriptor); a missing file, or an EXCLUSIVE request, is `INSTALL_ROOT_READ_ONLY` |

Transient refusals: on Windows a scanner or the indexer routinely holds a freshly extracted file open without `FILE_SHARE_DELETE`, so `Rename`, `RemoveFile`, `RemoveLink` and `RemoveEmptyDirectory` failing with `ERROR_ACCESS_DENIED` or `ERROR_SHARING_VIOLATION` are retried three more times, 100 ms apart (`Zenith_SleepUs(100000)`; startup step 4 runs on the main thread before the job system exists), before the step fails with `INSTALL_IO`; the attempts are counted in `package.install.transient_retries`. Linux and Android have no such refusal and never retry.

### Durable step rules

| Step | Sequence | After power loss |
|---|---|---|
| seal a bundle | for the bundle's manifest and every other regular file the bundle directory lists: `Open(READ_WRITE)`, `FlushFile`, `Close`; then `FlushDirectory(<bundle dir>)`; then `FlushDirectory(<kind root>/.incoming)` | the bundle's bytes and names are durable exactly as they were read; nothing is renamed |
| write the journal | write `install-journal.zinj.tmp` (`Open(CREATE_NEW)` after a `RemoveFile` of a stale one, `Write`, `FlushFile`, `Close`); `Rename(tmp, install-journal.zinj, replace)`; `FlushDirectory(.zenith2-install)` | the old journal or the new one, never a mix; a stray `.tmp` is deleted by recovery |
| write the install state | the same sequence for `install-state.zins` ("Install state file" in Formats.md) | the old state or the new one |
| write the verified stamp file | read and decode the current `verified.zinv`, merge ("Verified cache" item 4), then the same sequence for `verified.zinv` ("Verified stamp file" in Formats.md); best-effort: any error deletes the `.tmp`, is logged and counted in `package.install.stamp_write_failures`, and the operation continues with the previous file untouched | the old file or the new one; either only makes a mount re-hash more, never less |
| move a directory | `Rename(from, to, no replace)`; `FlushDirectory(parent of to)`; `FlushDirectory(parent of from)` | the directory at `from` or at `to`, never both, never neither |
| ensure a directory | `CreateDirectory(d)` (existing: success); `FlushDirectory(parent of d)` | `d` exists or not; ensuring again is harmless |
| delete a trash tree | depth first: every link `RemoveLink`, every regular file `RemoveFile`, every emptied directory `RemoveEmptyDirectory`, the tree's root last; never descending into an entry whose `m_bLink` is set; then `FlushDirectory(<kind root>/.trash)` | a partly deleted tree under `.trash`, which recovery finishes deleting |
| delete the journal | `RemoveFile(install-journal.zinj)`; `FlushDirectory(.zenith2-install)` | the journal or no journal |

Every mutation the applier makes is one row of this table; `Zenith_UpdateApplier::Step` performs exactly one row, so a test can interrupt between any two (`Test_UpdateApplier_PowerLossAtEveryStep_Converges`). Sealing changes no name and needs no journal: an interruption during it leaves a bundle that is re-verified at the next start.

## Install state machine

The applier's operations are install a patch, install a DLC revision, reject an incoming bundle, remove a DLC (tests and distributors only, "Windows DLC mounting"), and sweep obsolete patches. None copies bundle bytes: the bundle was delivered into `.incoming` on the same volume, is sealed and verified in place, and is renamed into place. Verification is read-only and needs no journal; every mutation is journaled.

### Install phases and events

| Phase (journal) \ Event | journal written | target rename durable | every move record TRASHED | every trash tree DELETED | an I/O error | power loss or kill |
|---|---|---|---|---|---|---|
| (none) | `BeginInstall` verified → COMMITTING; `BeginInstall` refused → REJECTING; `BeginRemoveDlc` or `BeginRemoveObsolete` → REMOVING | — | — | — | the operation is not started; error returned | nothing to recover |
| COMMITTING | — | install state written, the verified stamp file written (best-effort), then → COMMITTED (journal rewritten) | — | — | stays COMMITTING; error returned; the next start's recovery continues | recovery rows 4–8 |
| COMMITTED | — | — | with no record: the journal is deleted; with a PENDING record (moving starts): → CLEANING after the first move's journal rewrite; a DLC install whose one record is already TRASHED (moved out before the commit, flag bit 0): its trash tree is deleted, then the journal is rewritten with the record DELETED and phase CLEANING | — | stays; error returned | recovery row 9 |
| CLEANING | — | — | (deleting continues) | → journal deleted (done) | stays; error returned | recovery row 9 |
| REJECTING | — | — | (deleting continues) | → journal deleted (done) | stays | recovery row 9 |
| REMOVING | — | — | (deleting continues) | → journal deleted (done) | stays | recovery row 9 |

### Install steps per operation

`BeginInstall(kind, name)` performs the read-only part and the sealing, then one durable step:

| # | Action | Fails with |
|---|---|---|
| B1 | `List(<kind root>/.incoming/<name>)` | `UPDATE_INCOMPLETE` when absent |
| B2 | `Open`, `Read`, `Close` of the manifest (`update.zman` or `dlc.zman`), at most `96 + 64 × 65,536 + 1,048,576 + 64` bytes | `UPDATE_INCOMPLETE` when absent, `UPDATE_MANIFEST_INVALID` above the cap |
| B3 | `Zenith_DecodeUpdateManifest` (U1–U3, U7, PLAT, cross-area row PK14), then `Zenith_CheckUpdateManifestTarget` (U4, U5, U8), then PACK's U6, U10 and U11 | the check's error (Formats.md "Update manifest checks") |
| B4 | U0 over B1's listing, in two halves: (a) every entry is the manifest or a listed file name, and every one is a regular file (no subdirectory, no link, no other name); (b) every listed file name is present | (a) `UPDATE_MANIFEST_INVALID` (`package: install <name>: unlisted entry <entry>` or `: link <entry>`), a refusal; (b) `UPDATE_INCOMPLETE` (`package: install <name>: listed file <file> absent; left for the next start`, once per start), not a refusal |
| B5 | "seal a bundle" | `INSTALL_IO`: the bundle is left in `.incoming` untouched for the next start, not refused |
| B6 | every listed file's stamp through `Zenith_InstallTreeMountSource` over the bundle; `Zenith_VerifyUpdateManifestFiles` (U9) over that source with no stamp lookup (every file hashed); every stamp again, compared with the first ("Verified cache" items 2 and 4); the first stamps and the manifest's SHA-256 kept for the commit | `UPDATE_FILE_MISMATCH` for a hash or a size that differs (a refusal); `UPDATE_INCOMPLETE` for a stamp that changed during verification (`package: install <name>: <file> changed during verification; left for the next start`, no journal written, counted in `m_uIncompleteBundles`), not a refusal |
| B7 | "ensure a directory" `<kind root>/.trash`; then write the journal: COMMITTING (B3–B6 passed) or REJECTING (B3, B4 (a) or B6 failed) | — |

A failure of B3, B4 (a) or B6's hash or size check refuses the bundle: B7 writes REJECTING with the error code and the bundle's `.incoming/<name>` as its one move record, and `BeginInstall` returns the refusal; a failure of B1, B2's absence, B4 (b), B5 or B6's stamp bracket (a file whose stamp changed while it was hashed) starts nothing and leaves the bundle where it is, so a bundle a person is still extracting in place under its final name is installed at a later start once it is whole. Only B6 refusals follow a seal: the REJECTING journal of a B3 or B4 (a) refusal is written without B5, and its record is still durable, because the move to trash is itself a durable step.

A patch install of version `v` (incoming `Patches/.incoming/v/`), the journal carrying M move records (0 … 1,024, "Obsolete directories"):

| # | Durable step | Phase after |
|---|---|---|
| 1 | B7: the journal COMMITTING, target `v`, M records, each PENDING (or TRASHED for a leftover trash tree) | COMMITTING |
| 2 | move `Patches/.incoming/v` → `Patches/v` | COMMITTING |
| 3 | write the install state: release line, highest patch = max(the state's, `v`), swept build ID unchanged | COMMITTING |
| 3b | write the verified stamp file (best-effort): `Patches/v`'s record from B6, the surviving records of other directories ("Verified cache" item 4) | COMMITTING |
| 4 | the journal COMMITTED | COMMITTED |
| 5, 7, … (one per PENDING record) | move the record's directory to `Patches/.trash/<generation>` | — |
| 6, 8, … | the journal: that record TRASHED, phase CLEANING | CLEANING |
| then, one pair per record | delete its trash tree; then the journal: that record DELETED | CLEANING |
| last | delete the journal | — |

With M = 0 (a first patch), step 4 is followed by the last step.

A DLC install of `<name>` revision `r` (incoming `DLC/.incoming/<name>/`):

| # | Durable step | Phase after |
|---|---|---|
| 1 | B7: the journal COMMITTING; one move record `DLC/<name>` PENDING when `DLC/<name>` exists (M = 1), else none (M = 0); flag bit 0 clear | COMMITTING |
| 2 | (M = 1) move `DLC/<name>` → `DLC/.trash/<g>` | COMMITTING |
| 3 | (M = 1) the journal: flag bit 0 set, the record TRASHED | COMMITTING |
| 4 | move `DLC/.incoming/<name>` → `DLC/<name>` | COMMITTING |
| 4b | write the verified stamp file (best-effort): `DLC/<name>`'s record from B6 replacing the old revision's, the surviving records of other directories | COMMITTING |
| 5 | the journal COMMITTED | COMMITTED |
| 6 | (M = 1) delete the trash tree | COMMITTED |
| 7 | (M = 1) the journal: the record DELETED, phase CLEANING | CLEANING |
| last | delete the journal | — |

A refused bundle: B7 writes REJECTING with one record (`<kind root>/.incoming/<name>`); then move it to trash, the journal TRASHED, delete the tree, the journal DELETED, delete the journal; the rejection is reported to every world as `Package.UpdateFailed` in its snapshot ("World snapshot"). A DLC removal (`BeginRemoveDlc`) and an obsolete sweep (`BeginRemoveObsolete`, "Obsolete directories") write REMOVING with their records and run the same move, rewrite, delete, rewrite pairs.

### Obsolete directories

The obsolete set is fixed when its journal is written, in path byte order, so recovery never re-decides it:

| Rule | Directories | Read to decide |
|---|---|---|
| O1 | every `Patches/<w>` whose name is a version of another release line than the running build's | the name only |
| O2 | every `Patches/<w>` of the running line whose `update.zman` fails `Zenith_DecodeUpdateManifest` (U1–U3: it can never mount under the compiled-in key), or decodes with a target build ID other than the running build's (a patch of the previous full release of this line) | the manifest and its signature, never its files |
| O3 | at a patch install of `v`: every `Patches/<w>` of the running line with `w < v`; in a sweep: every `Patches/<w>` of the running line lower than the highest committed `Patches/<v>` of that line (left by an earlier truncated set) | the name only |
| O4 | every `Patches/.trash/<g>` and `DLC/.trash/<g>` left by an earlier operation, as a record already TRASHED with its own generation | the name only |

A patch install's journal carries O1–O4. The sweep (`BeginRemoveObsolete`) carries O1–O4 and runs only when the install state's swept build ID or release line differs from the running build's ("Pending installs at startup" step 7), so a full release within a line (a new build ID, `UPDATE_KIND_WITHIN_LINE`) removes the previous build's patches at its first start, and a start with nothing to sweep reads no manifest. A patch of the running line with `w > v` makes `v` not newer (U10), and that install is refused before any of this.

A journal holds at most 1,024 move records (Formats.md "Install journal header"), and the sets O1–O4 have no bound of their own. When a set (a patch install's O1–O4, or a sweep's) holds more than 1,024 directories, the journal carries the first 1,024 in path byte order and the rest wait: the pass logs `package: <n> obsolete directories left for the next start`, counts them in `m_uDeferredObsolete` and `package.install.obsolete_deferred`, and writes the install state's swept build ID as all zero at step 8.3, so the next start's cheap check fails and that start sweeps again; the excess shrinks by 1,024 per start. A deferred directory is never mountable over the committed patch (O1 and O2 cannot mount; an O3 directory is lower than the committed version, and the patch mount mounts the highest), so deferring it never changes what mounts.

### Recovery table

`Zenith_UpdateApplier::Recover` runs at every start that holds both locks ("Install lock and instance guard"), before any new operation. "Present" and "absent" are read with `Stat`; a link at any of these names counts as present and is never followed.

| # | Found at start | Recovery |
|---|---|---|
| 1 | no journal; a stray `install-journal.zinj.tmp`, `install-state.zins.tmp` or `verified.zinv.tmp` | delete the `.tmp` files; nothing else |
| 2 | a journal failing J1–J6 | log `package: install journal corrupt (<check>); rebuilding from directories`; delete every `Patches/.trash/*` and `DLC/.trash/*` tree; leave every `.incoming` bundle for the install pass, which re-seals and re-verifies it; delete the journal |
| 3 | a journal passing J1–J6 whose records contradict the directories: a PENDING record with both its source and its trash name present, or a target present at both its incoming and its committed name | log `package: install journal inconsistent with the install tree (<path>)`, then as row 2; this state needs a second writer that the instance guard excludes |
| 4 | COMMITTING, patch, `Patches/.incoming/v` present and `Patches/v` absent | redo step 2, then continue at step 3 |
| 5 | COMMITTING, patch, `Patches/v` present and `Patches/.incoming/v` absent | the rename happened; continue at step 3 (the install state's maximum makes a repeat harmless; step 3b keeps the previous file's record of `v` when it carries the SHA-256 of `Patches/v/update.zman`, else rebuilds it with B6's stamp, hash and stamp pass over `Patches/v`, "Verified cache" item 4) |
| 6 | COMMITTING, patch, both absent | the sealed bundle was deleted by someone else after step 1: log `package: install <v>: bundle vanished before commit`, delete the journal, count `m_uRecoveredBackward` |
| 7 | COMMITTING, DLC, M = 1, flag bit 0 clear | `DLC/<name>` present and its trash name absent: redo step 2; `DLC/<name>` absent and the trash name present: continue at step 3; both absent: row 3 |
| 8 | COMMITTING, DLC, flag bit 0 set or M = 0 | `DLC/.incoming/<name>` present and `DLC/<name>` absent: continue at step 4; `DLC/<name>` present and the incoming name absent: continue at step 4b (which keeps the previous file's record of `DLC/<name>` when it carries the SHA-256 of the committed `dlc.zman`, else rebuilds it with B6's stamp, hash and stamp pass over `DLC/<name>`); both present: row 3; both absent: when M = 1, move the trash name back to `DLC/<name>` (a durable move) so the old revision stays installed, then delete the journal; when M = 0, delete the journal |
| 9 | COMMITTED, CLEANING, REJECTING or REMOVING | for each record in order: PENDING with its source present and its trash name absent: move it, then rewrite it TRASHED; PENDING with its source absent and its trash name present: rewrite it TRASHED (the move happened before the rewrite); PENDING with both absent: rewrite it DELETED; TRASHED with its trash name present: delete the tree, then rewrite it DELETED; TRASHED with its trash name absent: rewrite it DELETED; DELETED: nothing. Then delete the journal |

- Recovery is idempotent: running it again after any interruption of itself reaches the same final state (`Test_UpdateApplier_RecoveryInterruptedAtEveryStep_Converges`).
- Recovery never mounts a committed patch, and verifies one only to rebuild a lost stamp record (rows 4, 5 and 8, read-only, "Verified cache" item 4); whether it mounts is decided by the patch mount at startup step 5 (`Zenith2/Docs/Design/PLAT/Filesystem.md → "Patch mount verification"`), so a patch interrupted in any phase is mounted whole or not at all.
- Trash generations are assigned when a journal is first written (B7, `BeginRemoveDlc`, `BeginRemoveObsolete`): g0 is one more than the greatest decimal `<g>` among the names under `Patches/.trash/` and `DLC/.trash/` together (1 when both are empty or absent); the k-th PENDING record of the journal (k = 0, 1, … in record order) gets the trash generation g0 + k, and an O4 record keeps the `g` of its own name. The header's generation starts at g0 + M and increases by one at every rewrite. A generation is therefore unique among the trash names present under both roots and the records of the one journal, which is all J6 requires; it is not unique for ever (once a journal is deleted and the trash emptied, a later journal may start from 1 again), and nothing needs it to be, because no record or file refers to a trash name after its tree is deleted. A trash name present before an operation is never mistaken for one of its records, since every new record's generation is above every present name.

### Pending installs at startup

`Zenith_RunPendingInstalls(desc)` runs as the closing action of startup step 4, on the main thread, before the virtual filesystem exists. Its file system (`m_pxFiles`) is the bridge's, which outlives the call and keeps the instance guard until shutdown step 10b:

1. `QueryVolumeKind()`; a volume outside the supported set of "Durability model" (Windows NTFS and ReFS; Android ext4 and F2FS; Linux ext4, XFS, Btrfs and tmpfs) logs `package: install root on an unsupported volume (<name>); installs disabled` once and returns: nothing is installed, recovered or locked, and the patch mount still mounts whatever verifies. This is the one return without the instance guard that needs none: every process refuses to install, recover or sweep on such a volume, so no process can change the tree under another.
2. Take `INSTALL_LOCK_APPLY` exclusively: `AcquireLock(APPLY, EXCLUSIVE)` is retried every 50 ms for at most `m_uLockWaitMs` (30,000 by default, so at most 600 waits; tests pass 0 for one attempt and no wait), each wait being one call of the desc's wait hook with this step's poll index, 1, 2, … (`m_pfnWait`, null: `Zenith_SleepUs(50000)`; startup step 4 runs on the main thread before the job system exists). `INSTALL_ROOT_READ_ONLY` logs `package: install root is read-only; nothing installed` once and continues at step 10 (nothing is touched). A wait that times out logs `package: another process is installing; this start waits for it`, sets `m_eFirstError` to `INSTALL_LOCKED` and continues at step 10.
3. Take `INSTALL_LOCK_RUNNING` exclusively, once, non-blocking. `INSTALL_LOCKED` means another instance of this install is running: log `package: another instance is running; updates wait for the next start`, set `m_bOtherInstanceRunning`, and continue at step 9.
4. The cheap check: `List(.zenith2-install)`; when `install-state.zins` is listed, `Open`, `Read` (48 bytes) and `Close` it; `List(<patch root>/.incoming)` and `List(<dlc root>/.incoming)` (absent directories are empty). When no journal and no `.tmp` is listed, no name without a leading `.` is in either incoming directory, and the state decodes with the running build's release line and swept build ID, continue at step 9: the common start costs the 12 file-system calls of "Complexity" and writes nothing.
5. `Recover`.
6. Free space: `QueryFreeBytes() ≥ m_ulLowDiskMarginBytes` (64 MiB; a journal is at most 64 + 24 × 1,024 + 65,536 bytes and every move is a rename); below it, log, set `LOW_DISK` and continue at step 9 with the incoming bundles left in place. Recovery has already run, because it only renames and deletes.
7. When the state's swept build ID or release line differs from the running build's, or the state is absent or refused: `BeginRemoveObsolete` (O1–O4, at most 1,024 records, "Obsolete directories") and `Step` until done, when its set is not empty.
8. When `m_bInstallIncoming` (false in every Android build, "Android content updates"; there the incoming directories are listed only to log `package: Patches/.incoming ignored on Android` once when non-empty):
   1. the names of `<patch root>/.incoming/` without a leading `.` that are `MAJOR.MINOR.PATCH` (others logged once and left), in descending version order: `BeginInstall(PATCH)` and `Step` until done; the first commit makes every lower incoming version not newer, so each is refused and removed in turn. When `m_ulPackageContentHash` is 0 (the bridge could not read `Content/mount-manifest.json`'s `contentHash`), U11 cannot be checked, so no patch bundle is examined: every one is left in `.incoming` for a start that knows the base package (`package: patch bundles left: base package identity unknown`, once per start), because committing a patch of another base would trash the mountable older patch (O3) and leave only the base (I2);
   2. the names of `<dlc root>/.incoming/` without a leading `.` that obey the DLC name rule, in byte order: `BeginInstall(DLC)` and `Step` until done;
   3. write the install state with the running release line, the highest committed patch and the running build ID as the swept build ID ("write the install state"), when any of the three changed; the swept build ID is written all zero instead when this pass deferred obsolete directories ("Obsolete directories"). This sub-step also runs when `m_bInstallIncoming` is false, after step 7.
9. Hand the instance guard over when step 2 took APPLY: when step 3 succeeded, `ReleaseLock(RUNNING)` and `AcquireLock(RUNNING, SHARED)` (no other process can take RUNNING exclusively in between, because it would first need APPLY, which this process still holds); when step 3 failed, `AcquireLock(RUNNING, SHARED)`, which succeeds because every other holder holds it shared.
10. `ReleaseLock(APPLY)` when held. When step 2 did not take APPLY (a timeout, or a read-only root), the guard wait: `AcquireLock(RUNNING, SHARED)`; while it returns `INSTALL_LOCKED` (another process holds RUNNING exclusively, which it does only between its own steps 3 and 9, that is while it recovers, sweeps or installs), call the wait hook and retry, every 50 ms and with no upper bound. The poll index restarts at 1 in this step (it does not continue step 2's): poll p is the p-th `INSTALL_LOCKED` answer of this step, the hook is called with p, and `package: waiting for another process to finish installing` is logged at poll 1 and at every poll divisible by 100 (every 5 s; startup has no window yet at step 4); the report's `m_uGuardWaitPolls` is the last p, and each poll is counted in `package.install.guard_waits`. The wait ends when the installing process hands its guard over at its step 9, so this process then mounts that process's result at step 5, never the tree mid-install. A read-only root opens an existing `running` file read-only and locks it shared (`AcquireLock`'s read-only fallback, "Durability model" host mapping); only when that file does not exist on a read-only root does the wait return `INSTALL_ROOT_READ_ONLY`, which logs `package: install root is read-only and has no instance lock; running without the instance guard` and leaves `m_bInstanceGuardHeld` false. No other path reaches step 5 without RUNNING held.
11. Return the report; the runtime bridge logs it (`package: pending installs: <installed> installed, <rejected> rejected, <recovered> recovered, <obsolete> obsolete removed`) and keeps one `Package.UpdateFailed` record per rejection, at most `uPACKAGE_MAX_REPORTED_REJECTIONS` (16; any further rejection is logged and counted in `package.events.dropped`), which every world receives in its snapshot ("World snapshot").

### Install lock and instance guard

<!-- elaboration -->
Two lock files under `<root>/.zenith2-install/`, each locked on its byte 0:

| Lock | File | Held | Guarantees |
|---|---|---|---|
| `INSTALL_LOCK_APPLY` | `lock` | exclusively by the one process in steps 2–10 of "Pending installs at startup" | at most one process installs, recovers or sweeps at a time |
| `INSTALL_LOCK_RUNNING` | `running` | shared by every running instance from step 9 or 10 until shutdown step 10b; exclusively only by an installer that found no other instance, from its step 3 to its step 9 | no process installs, recovers or sweeps while another instance of the same install runs, and no instance runs while another process installs |

A single two-mode lock is not enough, because neither `LockFileEx` nor `flock` converts exclusive to shared atomically: between the release and the shared re-acquisition another process could take the lock exclusively and move the first process's mounted patch to `.trash`. With two files the conversion of RUNNING happens while APPLY is held, and every installer takes APPLY before RUNNING, so the window is closed and the order cannot deadlock. A second launch of a running game therefore always plays with the current content, its installs and recovery waiting for the next start at which it is alone; a kill or a power loss releases both locks with the process.

The converse also holds: a process that cannot take APPLY (it timed out behind a long install, or its root is read-only) never continues to startup step 5 without RUNNING held shared. It waits at step 10 for as long as the installer holds RUNNING exclusively, so it starts on the installer's result; and because it then holds RUNNING shared, a later launch that takes APPLY fails its own step 3 and installs nothing while it runs. The wait has no upper bound by ruling (PACK-R2): an install that takes minutes delays the second launch's startup by minutes, logged every 5 s, rather than letting it mount a tree that is being renamed and deleted. The only processes that run without the guard are the two that cannot be harmed and say so: an unsupported volume (no process installs on it, step 1) and a read-only root that has no `running` file (logged at step 10).

`Zenith_UpdateApplier::Create` refuses `PACKAGE_ERROR_INSTALL_LOCKED` unless its file system holds APPLY and RUNNING exclusively (`HoldsLock`), so no code path mutates the install tree without both. The bridge releases RUNNING in shutdown step 10b, before `Zenith_Platform` is destroyed (`Zenith2/Docs/Contracts.md` → "Shutdown ownership order" step 10b).

### Android content updates

In every Android build (Debug, Release and Shipping) every content update arrives through Google Play or the build itself: a new app version (an installed update) or the asset packs of that version. `Zenith_RunPendingInstalls` runs with `m_bInstallIncoming` false: recovery and the obsolete sweep only, both of which find nothing on a device no build ever installed to, at the cost of the cheap check. The reasons: Z2-D-027 rejects a custom content downloader as a duplicate of Play Asset Delivery; a patch carries Luau bytecode, which Play's policy treats as code that must come through Play; and in Debug and Release builds no tool of this programme produces an Android patch bundle (the patch builder reads the Windows release and its WINDOWS layout, Tooling.md "Patch builder"), while the base package's identity that U11 needs is not readable at startup step 4 on Android (the mount manifest is an APK asset, reached only through the virtual filesystem created at step 5). The applier's behaviour on an Android-shaped root is still proved, platform-independently, by the update applier tests over the memory file system on every host preset; no on-device patch flow is claimed.

## Asset pack state machine

`Zenith_AssetPackInstaller` holds one record per deliverable unit of `package.zpkm`: a unit of kind FAST_FOLLOW, ON_DEMAND or DLC without the PRESENT flag, which exists only in the ANDROID_BUNDLE layout and in the development-only SIMULATED layout (Formats.md "Package manifest tables"; "Simulated deliverable units"). Every other unit is either present with the package (the PRESENT flag: base, folded and install-time units, reported MOUNTED by the service with no record) or a Windows DLC directory ("Windows DLC mounting"). Each record has:

| Field | Meaning |
|---|---|
| state | `Zenith_AssetPackState` |
| target | for UNMOUNTING, CANCELLING and STOPPING_VERIFICATION: the state entered when the wait ends (NOT_INSTALLED, DELIVERED, REMOVING or AWAITING_ENTITLEMENT) |
| intent | `NONE`, `WANT_INSTALLED`, `WANT_REMOVED`; persisted in `delivery-state.zdls` |
| attempt | fetches issued in this process |
| `m_bVerifyRetryUsed` | the one remove-and-refetch after a failed verification has been used; cleared only by a game request |
| `m_bRefetchAfterRemove`, `m_bFetchAfterUnmount` | a fetch to issue when the removal or the unmount completes |
| `m_bHeldGameRequest` | the A1 withheld in AWAITING_ENTITLEMENT came from a game request ("Asset pack transitions", guard on A1); read by G's A1, cleared when it is issued or the pack leaves AWAITING_ENTITLEMENT |
| `m_uNetworkRetries`, `m_ulNetworkRetryAtUs` | automatic re-fetches after a network failure in this process, and the next one's due time |
| last Play status | the latest `Zenith_DeliveryStatus` applied, kept in every state |
| verification operation, mount source IDs | owned per "Verification pipeline" and "Mounting and unmounting" |
| `m_pxCurrentBlock`, draining list | the release block of the current mount operation, and the blocks of rolled-back mounts still being released ("Mounting and unmounting") |

### Asset pack events

| # | Event | Source |
|---|---|---|
| F | `RequestFetch` | the game (`Package.RequestPack` command); automatic: auto fast-follow, resume of `WANT_INSTALLED`, the re-fetches the tables name |
| C | `RequestCancel` | the game |
| R | `RequestRemove` | the game, `WANT_REMOVED` at startup |
| Q | `RequestConfirmation` | the game (`Package.Confirm` command) |
| P1 | Play `PENDING` | delivery update |
| P2 | Play `DOWNLOADING` | delivery update (bytes) |
| P3 | Play `TRANSFERRING` | delivery update (percent) |
| P4 | Play `WAITING_FOR_WIFI` | delivery update: the download waits for Wi-Fi; the user may consent to cellular data in Play's confirmation dialog |
| P5 | Play `REQUIRES_USER_CONFIRMATION` | delivery update: the download needs the user's consent (for example an app not installed from Play); takes precedence over P4 |
| P6 | Play `COMPLETED` | delivery update |
| P7 | Play `FAILED` with any error except `NETWORK_ERROR`, or a request failure (`fetch`, `getPackStates` task failure) | delivery update or result |
| P7N | Play `FAILED` with `NETWORK_ERROR` | delivery update or result |
| P8 | Play `CANCELED` | delivery update |
| P9 | Play `NOT_INSTALLED` or `UNKNOWN` | delivery update |
| V1 | verification succeeded | the verification pipeline |
| V2 | verification failed | the verification pipeline |
| V3 | verification stopped: every read and hash job of a cancelled verification reported complete | the verification pipeline |
| M1 | every archive mounted | the mount step of the same prologue |
| M2 | a mount failed | the mount step |
| X | every archive source released | the release block ("Mounting and unmounting") |
| D1 | `REMOVED` | the bridge's `removePack` success (a result cell) |
| D2 | `REMOVE_FAILED` | the bridge's `removePack` failure (a result cell) |
| E | entitlement revoked or refused: the unit's answer became DENIED | the entitlement query, re-asked when its revision changes |
| G | entitlement granted: the unit's answer became GRANTED | the same |
| S | a verification slot became free and this pack is the DELIVERED pack with the lowest unit index | the verification pipeline |
| N | the network retry is due: `m_ulNetworkRetryAtUs` passed, or PLAT reported connectivity regained, while the application is in `APP_STATE_RUNNING` | the prologue's frame input |

E and G are edge-triggered: at each revision change the installer re-asks every entitlement-gated unit and raises E when the answer became DENIED (from GRANTED or UNKNOWN) and G when it became GRANTED (from DENIED or UNKNOWN); a change to UNKNOWN raises nothing. A pack entering AWAITING_ENTITLEMENT whose answer is already GRANTED takes G at once. Units that are not entitlement-gated never receive E or G.

### Asset pack transitions

Actions: **A1** issue `Fetch`, intent `WANT_INSTALLED`, attempt + 1 (a game request also clears `m_bVerifyRetryUsed` and `m_uNetworkRetries`); **A2** issue `Cancel`; **A3** start verification: the cache check of "Verified cache" item 3, which raises V1 at once when the pack's record and every archive's stamp match, else the "Verification pipeline"; **A4** mount ("Mounting and unmounting"); **A5** unmount: `RemoveSource` of every recorded source; **A6** issue `Remove`; **A7** cancel the verification operation; **A8** issue `ShowConfirmation`; **A9** schedule a network retry: `m_uNetworkRetries` + 1, `m_ulNetworkRetryAtUs` = now + `package.networkRetrySeconds` × 1,000,000; **A10** re-issue `Fetch` without changing the attempt (Play resumes from its own offset); **A11** queue `Package.ConfirmationRequired` with its reason (P4 cellular, P5 consent).

Guard on A1: for an entitlement-gated unit whose answer is not GRANTED, a game request answering DENIED is refused synchronously with `PACKAGE_ERROR_ENTITLEMENT_DENIED` and changes nothing; an automatic A1 answering DENIED moves the pack to NOT_INSTALLED with the error recorded; an answer UNKNOWN moves the pack to AWAITING_ENTITLEMENT instead of issuing the fetch. The withheld A1 sets intent `WANT_INSTALLED` and marks the state file dirty, so a kill while the pack waits keeps the request (the next start's resume asks the query again), but it issues no `Fetch`, adds nothing to the attempt and clears neither `m_bVerifyRetryUsed` nor `m_uNetworkRetries`; it sets `m_bHeldGameRequest` when it came from a game request. G's A1 (the AWAITING_ENTITLEMENT row of the second table) is then a game request exactly when `m_bHeldGameRequest` is set, and clears the flag.

Shorthands used in the cells: **VFY**: VERIFYING with A3 when fewer than `m_uMaxConcurrentVerifications` packs are VERIFYING or STOPPING_VERIFICATION, else DELIVERED (always DELIVERED after `BeginShutdown`, by the shutdown rule below); **HOLD or VFY**: AWAITING_ENTITLEMENT (files kept) when the unit is entitlement-gated and its answer is not GRANTED, else VFY (in replay mode a unit of R is never held: "Replay pack set" step 2); **RETRY**: WAITING_FOR_NETWORK with A9 when the intent is `WANT_INSTALLED` and `m_uNetworkRetries` < `uPACKAGE_MAX_NETWORK_RETRIES` (3), else FAILED with `NETWORK_UNAVAILABLE`; **ROLLBACK**: REMOVING with A6, intent `NONE`; **BUSY**: the request returns `PACKAGE_ERROR_PACK_BUSY` synchronously and changes nothing; **(T)** after a state: the state with that target; **matching**: P1 → PENDING, P2 → DOWNLOADING, P3 → TRANSFERRING, with the status snapshot updated. Every transition queues one `Package.PackStateChanged` event and marks the state file dirty; a cell that keeps the state but updates the snapshot ("stays") queues the event only when the progress changed by at least 1 %; a request cell that changes only the intent ("intent `NONE`") queues no event and marks the state file dirty. A cell `—` changes nothing: for a delivery, pipeline, entitlement or retry event it is counted in `package.delivery.ignored_events`; for a request (F, C, R) the request returns success and nothing is counted. Every Play update also updates the last Play status, in every state.

There are 26 events. Q (`RequestConfirmation`) is global, not per pack: its rule follows the first table. The per-pack tables hold the other 25 events in 23 columns (P1, P2 and P3 share one column, whose "matching" cell gives three different states).

Requests and Play updates:

| State | F | C | R | P1 / P2 / P3 | P4 | P5 | P6 | P7 | P7N | P8 | P9 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| UNKNOWN | REQUESTED, A1 | BUSY (the pack's state is not known yet) | REMOVING, A6, intent `WANT_REMOVED` | intent `WANT_REMOVED`: CANCELLING (REMOVING), A2; else matching | intent `WANT_REMOVED`: CANCELLING (REMOVING), A2; else WAITING_FOR_WIFI, A11 | intent `WANT_REMOVED`: CANCELLING (REMOVING), A2; else WAITING_FOR_CONFIRMATION, A11 | intent `WANT_REMOVED`: REMOVING, A6; else HOLD or VFY | intent `WANT_INSTALLED`: REQUESTED, A1; else FAILED | intent `WANT_INSTALLED`: RETRY; else FAILED (`NETWORK_UNAVAILABLE`) | intent `WANT_INSTALLED`: REQUESTED, A1; else NOT_INSTALLED | intent `WANT_INSTALLED`: REQUESTED, A1; intent `WANT_REMOVED`: NOT_INSTALLED, intent `NONE`; else NOT_INSTALLED |
| NOT_INSTALLED | REQUESTED, A1 | intent `NONE` | — | matching (Play started the pack itself) | WAITING_FOR_WIFI, A11 | WAITING_FOR_CONFIRMATION, A11 | HOLD or VFY | FAILED | FAILED (`NETWORK_UNAVAILABLE`) | — | — |
| REQUESTED | — | CANCELLING (REMOVING), A2, intent `NONE` | CANCELLING (REMOVING), A2, intent `WANT_REMOVED` | matching | WAITING_FOR_WIFI, A11 | WAITING_FOR_CONFIRMATION, A11 | HOLD or VFY | FAILED | RETRY | ROLLBACK | — |
| PENDING, DOWNLOADING, TRANSFERRING | — | CANCELLING (REMOVING), A2, intent `NONE` | CANCELLING (REMOVING), A2, intent `WANT_REMOVED` | matching | WAITING_FOR_WIFI, A11 | WAITING_FOR_CONFIRMATION, A11 | HOLD or VFY | FAILED | RETRY | ROLLBACK | intent `WANT_INSTALLED` and attempt < 3: REQUESTED, A1; else NOT_INSTALLED |
| WAITING_FOR_WIFI | — | CANCELLING (REMOVING), A2, intent `NONE` | CANCELLING (REMOVING), A2, intent `WANT_REMOVED` | matching (consent given, or Wi-Fi arrived) | stays | WAITING_FOR_CONFIRMATION, A11 | HOLD or VFY | FAILED | RETRY | ROLLBACK | as DOWNLOADING |
| WAITING_FOR_CONFIRMATION | — | CANCELLING (REMOVING), A2, intent `NONE` | CANCELLING (REMOVING), A2, intent `WANT_REMOVED` | matching (consent given) | WAITING_FOR_WIFI, A11 | stays | HOLD or VFY | FAILED | RETRY | ROLLBACK | as DOWNLOADING |
| WAITING_FOR_NETWORK | REQUESTED, A1 | ROLLBACK | REMOVING, A6, intent `WANT_REMOVED` | matching (Play resumed by itself) | WAITING_FOR_WIFI, A11 | WAITING_FOR_CONFIRMATION, A11 | HOLD or VFY | FAILED | stays (the scheduled retry stands) | ROLLBACK | stays |
| DELIVERED | — | — (completion wins, [Z2-N-CONTRACT-337]) | REMOVING, A6, intent `WANT_REMOVED` | — | — | — | — | — | — | — | NOT_INSTALLED, intent `NONE` |
| VERIFYING | — | — | STOPPING_VERIFICATION (REMOVING), A7, intent `WANT_REMOVED` | — | — | — | — | — | — | — | STOPPING_VERIFICATION (NOT_INSTALLED), A7 |
| VERIFIED | — | — | REMOVING, A6, intent `WANT_REMOVED` | — | — | — | — | — | — | — | NOT_INSTALLED, intent `NONE` |
| MOUNTED | — | — | UNMOUNTING (REMOVING), A5, intent `WANT_REMOVED` | — | — | — | pack version differs from the mounted one: UNMOUNTING (DELIVERED), A5; else — | — | — | — | UNMOUNTING (NOT_INSTALLED), A5 |
| UNMOUNTING (T) | `m_bFetchAfterUnmount` set, intent `WANT_INSTALLED` | — | T := REMOVING, intent `WANT_REMOVED`, `m_bFetchAfterUnmount` cleared | — | — | — | T NOT_INSTALLED: T := DELIVERED; else stays (a removal or a revocation keeps its target) | — | — | — | T := NOT_INSTALLED unless T is REMOVING |
| CANCELLING (T) | BUSY | — | T := REMOVING, intent `WANT_REMOVED` | — | T AWAITING_ENTITLEMENT: AWAITING_ENTITLEMENT; else REMOVING, A6 (a waiting download is removed rather than cancelled) | as P4 | intent `WANT_REMOVED`: REMOVING, A6; T AWAITING_ENTITLEMENT: AWAITING_ENTITLEMENT (files kept); else VFY (completion wins) | T AWAITING_ENTITLEMENT: AWAITING_ENTITLEMENT; else REMOVING, A6 | as P7 | T AWAITING_ENTITLEMENT: AWAITING_ENTITLEMENT; else REMOVING, A6 | T AWAITING_ENTITLEMENT: AWAITING_ENTITLEMENT; else NOT_INSTALLED, intent `NONE` |
| REMOVING | BUSY | — | — | — | — | — | — | — | — | — | — |
| FAILED | REQUESTED, A1 | intent `NONE` (files kept) | REMOVING, A6, intent `WANT_REMOVED` | matching (Play retrying) | WAITING_FOR_WIFI, A11 | WAITING_FOR_CONFIRMATION, A11 | HOLD or VFY | stays, error updated | RETRY (else stays) | NOT_INSTALLED | NOT_INSTALLED |
| AWAITING_ENTITLEMENT | — (the request is held) | NOT_INSTALLED, intent `NONE` (files kept) | REMOVING, A6, intent `WANT_REMOVED` | stays | stays | stays | stays | stays | stays | stays | stays |
| STOPPING_VERIFICATION (T) | BUSY | — | T := REMOVING, intent `WANT_REMOVED` | — | — | — | — | — | — | — | T := NOT_INSTALLED unless T is REMOVING |

Q, the confirmation request: `RequestConfirmation` is valid while at least one pack is WAITING_FOR_WIFI or WAITING_FOR_CONFIRMATION and no confirmation dialog is open: it issues A8 once for all of them (Play's dialog covers every waiting pack) and returns success; otherwise it returns `PACKAGE_ERROR_PACK_BUSY` and changes nothing. Play answers with P1–P3 for the packs the user released, or leaves them waiting when the user declined; the confirmation result arrives in the global confirmation cell, closes the dialog and is logged (`package: confirmation accepted`, `package: confirmation declined`, or for `CONFIRMATION_NOT_REQUIRED` and `NETWORK_UNRESTRICTED` `package: confirmation not needed`), changing no pack state.

Pipeline, removal, entitlement and retry events:

| State | V1 | V2 | V3 | M1 | M2 | X | D1 | D2 | E | G | S | N |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| UNKNOWN | — | — | — | — | — | — | — | — | — | — | — | — |
| NOT_INSTALLED | — | — | — | — | — | — | — | — | — | — | — | — |
| REQUESTED, PENDING, DOWNLOADING, TRANSFERRING, WAITING_FOR_WIFI, WAITING_FOR_CONFIRMATION | — | — | — | — | — | — | — | — | CANCELLING (AWAITING_ENTITLEMENT), A2 | — | — | — |
| WAITING_FOR_NETWORK | — | — | — | — | — | — | — | — | AWAITING_ENTITLEMENT (the retry dropped) | — | — | REQUESTED, A10 |
| DELIVERED | — | — | — | — | — | — | — | — | AWAITING_ENTITLEMENT (files kept) | — | VERIFYING, A3 | — |
| VERIFYING | HOLD, else VERIFIED and A4 in the same prologue | the verification-failure rule below | — | — | — | — | — | — | STOPPING_VERIFICATION (AWAITING_ENTITLEMENT), A7 | — | — | — |
| VERIFIED | — | — | — | MOUNTED | FAILED (`MOUNT_FAILED`): the archives added so far are removed (A5 without waiting; files kept; the mount's release block drains, "Mounting and unmounting") | — | — | — | AWAITING_ENTITLEMENT (files kept) | — | — | — |
| MOUNTED | — | — | — | — | — | — | — | — | UNMOUNTING (AWAITING_ENTITLEMENT), A5 | — | — | — |
| UNMOUNTING (T) | — | — | — | — | — | T REMOVING: `m_bRefetchAfterRemove` set when `m_bFetchAfterUnmount` is, REMOVING, A6; T NOT_INSTALLED: `m_bFetchAfterUnmount` ? REQUESTED, A1 : NOT_INSTALLED, intent `NONE`; T DELIVERED: VFY; T AWAITING_ENTITLEMENT: AWAITING_ENTITLEMENT; in every case `m_bFetchAfterUnmount` is then cleared | — | — | T := AWAITING_ENTITLEMENT unless T is REMOVING | T AWAITING_ENTITLEMENT: T := DELIVERED | — | — |
| CANCELLING (T) | — | — | — | — | — | — | — | — | stays (a requested cancel or removal completes; a revocation's target is already AWAITING_ENTITLEMENT) | — | — | — |
| REMOVING | — | — | — | — | — | — | `m_bRefetchAfterRemove`: cleared, REQUESTED, A1; else NOT_INSTALLED, intent `NONE` | FAILED (`DELIVERY_FAILED`), `m_bRefetchAfterRemove` cleared, intent unchanged (a `WANT_REMOVED` intent is retried at the next start) | — | — | — | — |
| FAILED | — | — | — | — | — | — | — | — | — | — | — | — |
| AWAITING_ENTITLEMENT | — | — | — | — | — | — | — | — | NOT_INSTALLED, intent `NONE`, error `ENTITLEMENT_DENIED` | last Play status COMPLETED: VFY; else REQUESTED, A1 | — | — |
| STOPPING_VERIFICATION (T) | — | — | T REMOVING: REMOVING, A6; T NOT_INSTALLED: NOT_INSTALLED, intent `NONE`; T AWAITING_ENTITLEMENT: AWAITING_ENTITLEMENT; T DELIVERED (shutdown only): DELIVERED | — | — | — | — | — | T := AWAITING_ENTITLEMENT unless T is REMOVING | — | — | — |

- The verification-failure rule (V2): with `m_bRemoveOnVerifyFailure` and `m_bVerifyRetryUsed` clear: set `m_bVerifyRetryUsed` and `m_bRefetchAfterRemove`, record `VERIFY_FAILED`, REMOVING with A6, and at D1 the automatic re-fetch of the REMOVING row; otherwise FAILED (`VERIFY_FAILED`), files kept. `m_bVerifyRetryUsed` is cleared only by a game request, so corrupt data costs at most one automatic removal and re-fetch per game request (`Test_AssetPackInstaller_CorruptDelivery_RemovedAndRefetchedOnce`).
- A cancel clears the intent: every C cell that is neither BUSY nor `—` sets intent `NONE`, so a pack the user cancelled is never resumed by "Fetch policy" at the next start, even when the process is killed while CANCELLING; an E in CANCELLING keeps the target REMOVING, so G never re-fetches a cancelled pack.
- A6 is issued only while no release block of the pack is draining ("Mounting and unmounting"): a cell that names A6 while one is draining enters REMOVING and issues the `Remove` at the first prologue at which every block of the pack is fully released.
- Leaving VERIFYING or STOPPING_VERIFICATION frees a verification slot, and S is raised for the DELIVERED pack with the lowest unit index in the same prologue; after `BeginShutdown` S is never raised (the shutdown rule).
- A Play update whose sequence is not greater than the last one applied to the same cell of the pack is dropped and counted in `package.delivery.stale_updates` (the delivery's sequence is strictly increasing across every cell).
- An apply-time failure of a buffered script command (a request refused BUSY, `ENTITLEMENT_DENIED`, `NOT_DELIVERABLE`, `UNKNOWN_PACK`) queues one `Package.PackStateChanged` with the pack's unchanged state and the error, so the script that requested it learns why ("Script-facing API" in PublicAPI.md); an event carrying an error is never coalesced ("Event publication and replay" step 2).
- Shutdown rule: `BeginShutdown` (shutdown step 4) moves every MOUNTED pack to UNMOUNTING (NOT_INSTALLED) with A5 and every VERIFYING pack to STOPPING_VERIFICATION (DELIVERED) with A7, and leaves every intent as it is. From then on nothing starts and nothing is mounted, packs and Windows DLC alike:
  1. no delivery call is issued: A1, A2, A6, A8 and A10 are suppressed, and the state they would lead to is entered without the call;
  2. no verification starts and nothing mounts: A3 and A4 are suppressed, VFY resolves to DELIVERED, and HOLD or VFY to AWAITING_ENTITLEMENT or DELIVERED;
  3. S is never raised, so no pack leaves DELIVERED; E, G and N are never raised (`PumpShutdown` re-asks no entitlement and reads no frame input) and no Play update is polled, so no P event, M1, M2, D1 or D2 occurs;
  4. no intent changes, and `m_bFetchAfterUnmount` and `m_bRefetchAfterRemove` are ignored;
  5. every request (F, C, R, Q) returns `PACKAGE_ERROR_SHUTTING_DOWN` and changes nothing.

  The only events `PumpShutdown` raises are V2, V3 and X, so the reachable cells, re-derived under the five overrides, are exactly these (the persisted intents are the ones the game left, and the next start resumes them, "Fetch policy"):

  | State at the event | V2 | V3 | X |
  |---|---|---|---|
  | STOPPING_VERIFICATION (T) | — (counted) | T DELIVERED: DELIVERED; T NOT_INSTALLED: NOT_INSTALLED; T REMOVING: REMOVING, no `Remove` issued; T AWAITING_ENTITLEMENT: AWAITING_ENTITLEMENT | — |
  | UNMOUNTING (T) | — | — | T DELIVERED (a new Play version that arrived before `BeginShutdown`): DELIVERED, no verification; T NOT_INSTALLED: NOT_INSTALLED, no fetch; T REMOVING: REMOVING, no `Remove` issued; T AWAITING_ENTITLEMENT: AWAITING_ENTITLEMENT |
  | any other state | — | — | — |

  A pack DELIVERED, REMOVING, CANCELLING, AWAITING_ENTITLEMENT or in any download state at `BeginShutdown` stays in it. No `AddSource` and no verification read is issued after `BeginShutdown`; only the reads already in flight complete. `Test_AssetPackInstaller_ShutdownCancelsVerification` asserts every reachable cell.
- `Test_AssetPackInstaller_Transitions_EveryCell` covers the 19 states (the grouped rows expanded to each state) against the 25 per-pack events (the P1 / P2 / P3 column expanded to its three events), 19 × 13 + 19 × 12 = 475 cells. Each cell expands into cases: the states with a target into one row per target (UNMOUNTING 4, CANCELLING 2, STOPPING_VERIFICATION 3; its DELIVERED target arises only under the shutdown rule), giving 25 rows and 625 base cases, and each conditional cell and shorthand into its arms (VFY 2, HOLD or VFY 3, RETRY 2, every `X: …` clause and its `else` one each), 56 more: 681 cases. The test compares each case with a literal expected table transcribed by hand from this section, never read from the implementation's cell array, and asserts each case's next state, target, intent, issued actions, request result and queued event; the global Q rule is asserted by `Test_AssetPackInstaller_ConfirmationFlow`.

### Play status and error mapping

The Java bridge passes Play's integers unchanged; `Internal/Android/Zenith_PlayAssetPackDelivery_Android.cpp` maps them:

| `AssetPackStatus` | Value | `Zenith_DeliveryStatus` | Event |
|---|---|---|---|
| `UNKNOWN` | 0 | `DELIVERY_STATUS_UNKNOWN` | P9 |
| `PENDING` | 1 | `PENDING` | P1 |
| `DOWNLOADING` | 2 | `DOWNLOADING` | P2 |
| `TRANSFERRING` | 3 | `TRANSFERRING` | P3 |
| `COMPLETED` | 4 | `COMPLETED` | P6 |
| `FAILED` | 5 | `FAILED` | P7, or P7N with `NETWORK_ERROR` |
| `CANCELED` | 6 | `CANCELED` | P8 |
| `WAITING_FOR_WIFI` | 7 | `WAITING_FOR_WIFI` | P4 |
| `NOT_INSTALLED` | 8 | `NOT_INSTALLED` | P9 |
| `REQUIRES_USER_CONFIRMATION` | 9 | `REQUIRES_USER_CONFIRMATION` | P5 |
| any other | — | `UNKNOWN`, counted in `package.delivery.unknown_status` | P9 |

| `AssetPackErrorCode` | Value | `Zenith_DeliveryErrorCode` | `Zenith_PackageError` in FAILED |
|---|---|---|---|
| `APP_UNAVAILABLE` | −1 | `APP_UNAVAILABLE` | `DELIVERY_UNAVAILABLE` |
| `PACK_UNAVAILABLE` | −2 | `PACK_UNAVAILABLE` | `DELIVERY_FAILED` |
| `INVALID_REQUEST` | −3 | `INVALID_REQUEST` | `DELIVERY_FAILED` |
| `DOWNLOAD_NOT_FOUND` | −4 | `DOWNLOAD_NOT_FOUND` | `DELIVERY_FAILED` |
| `API_NOT_AVAILABLE` | −5 | `API_NOT_AVAILABLE` | `DELIVERY_UNAVAILABLE` |
| `NETWORK_ERROR` | −6 | `NETWORK_ERROR` | `NETWORK_UNAVAILABLE` (P7N) |
| `ACCESS_DENIED` | −7 | `ACCESS_DENIED` | `DELIVERY_FAILED` |
| `INSUFFICIENT_STORAGE` | −10 | `INSUFFICIENT_STORAGE` | `INSUFFICIENT_STORAGE` |
| `PLAY_STORE_NOT_FOUND` | −11 | `PLAY_STORE_NOT_FOUND` | `DELIVERY_UNAVAILABLE` |
| `NETWORK_UNRESTRICTED` | −12 | `NETWORK_UNRESTRICTED` | none: an answer to a confirmation request, logged and dropped |
| `APP_NOT_OWNED` | −13 | `APP_NOT_OWNED` | `DELIVERY_FAILED` |
| `CONFIRMATION_NOT_REQUIRED` | −14 | `CONFIRMATION_NOT_REQUIRED` | none: an answer to a confirmation request, logged and dropped |
| `UNRECOGNIZED_INSTALLATION` | −15 | `UNRECOGNIZED_INSTALLATION` | `DELIVERY_UNAVAILABLE` |
| `INTERNAL_ERROR` | −100 | `INTERNAL_ERROR` | `DELIVERY_FAILED` |
| any other | — | `INTERNAL_ERROR` | `DELIVERY_FAILED` |

<!-- informative -->
Play's documentation for the pinned `com.google.android.play:asset-delivery` (`AssetPackStatus`, `AssetPackErrorCode`, `AssetPackManager`) states the meanings this mapping relies on: `WAITING_FOR_WIFI` is "waiting for Wi-Fi to become available before proceeding", and the app "can ask the user to download … over cellular data"; `REQUIRES_USER_CONFIRMATION` "requires user consent to be downloaded", which "can happen if the current app version was not installed by Play", and takes precedence over waiting for Wi-Fi; `showConfirmationDialog(Activity)` covers packs in either status and supersedes the deprecated `showCellularDataConfirmation`, which covers `WAITING_FOR_WIFI` only; `NETWORK_ERROR` is a failure to reach Play's servers, reported as `FAILED`, so a network loss reaches the installer as P7N or as a stalled DOWNLOADING, never as P4; `NETWORK_UNRESTRICTED` and `CONFIRMATION_NOT_REQUIRED` answer a confirmation request that had nothing to confirm; `fetch` "fails if the app is not in the foreground", which is why N is raised only in `APP_STATE_RUNNING`; `cancel` cancels only active downloads, which is why CANCELLING removes a waiting download. The pinned version's reference pages are re-read by the Stage 6 ticket that pins the library (cross-area row PK7), and a difference changes this table in that ticket's diff.
<!-- /informative -->

### Delivery update slots

<!-- elaboration -->
Play calls the bridge's `AssetPackStateUpdateListener` on the Android UI thread, and the request listeners (`getPackStates`, `fetch`, `removePack`, `showConfirmationDialog`) on Play's task threads. The native callbacks never touch installer state: each writes one cell of a fixed table, under `uLOCK_LEVEL_PACKAGE_DELIVERY_UPDATES` (650, short-hold), stamping it with the next value of one strictly increasing sequence, and returns:

| Cell | Count | Written by | Coalescing |
|---|---|---|---|
| state cell | one per pack | the state listener, `getPackStates` results, `cancel`'s returned states | latest wins: every Play state update carries the pack's whole state (status, error, bytes, percent, version tag), so the latest supersedes the others |
| result cell | one per pack | `removePack` success (`REMOVED`) or failure (`REMOVE_FAILED`), a `fetch` or `getPackStates` task failure (`REQUEST_FAILED` with the error) | none needed: at most one asynchronous result per pack is outstanding (a `Remove` is issued only on entry to REMOVING, which issues nothing else; a fetch only from a state awaiting Play's first report); an overwrite of an unpolled result is a defect, counted in `package.delivery.result_overwritten` and asserted 0 by the tests |
| confirmation cell | one | the `showConfirmationDialog` task (`CONFIRMATION_RESULT` with the result code and error) | one dialog at a time: A8 is not issued while a dialog is open |

`Poll` copies every cell whose sequence is newer than the cell's last poll, in ascending sequence across all cells, and clears its dirty flag; the installer applies them in that order, so a removal result and a subsequent state update of the same pack are applied in the order Play produced them. No update can overflow the table, because each source owns its cell. `Close()` (the Play delivery's destructor, after the Java bridge's `shutdown()` returned) makes every subsequent write a no-op counted in `package.delivery.writes_after_close`, which the C++ side never reaches by construction ("Java bridge" in Tooling.md) and `Test_DeliverySlotTable_CloseRefusesLaterWrites` checks. The simulated delivery writes the same table from `Poll` itself.

### Verification pipeline

A delivered pack is verified before it is mounted ([Z2-N-ARCH-956], verified once and re-hashed when its stamps change, core-doc row X366), by this pipeline whenever A3's cache check ("Verified cache" item 3) does not match: every archive of the unit must have the base `package.zpkm`'s byte count and SHA-256 (the manifest of the base package, never a patched one: Formats.md "Package manifest").

1. `GetLocation` gives the assets host path; for each archive of the unit, `Lookup` through a directory source over that path (`Zenith_CreateDirectoryMountSource`, cross-area row PK2) must report the manifest's byte count, else V2 with `VERIFY_FAILED` (`package: pack <name>: <archive>: size <n>, expected <m>`).
2. At most `m_uMaxConcurrentVerifications` packs are VERIFYING or STOPPING_VERIFICATION at once; a pack waiting for a slot is DELIVERED.
3. Per archive, `m_uVerifyBuffersPerArchive` buffers of `m_uVerifyChunkBytes` are taken from the installer's verification arena (sized at Create: concurrency × buffers × chunk, 8 MiB with the defaults, part of the package service resident memory row). Reads are issued through `Zenith_AsyncIO::ReadFromSource` against the not-yet-mounted directory source at `IO_PRIORITY_BACKGROUND` into a completion queue the installer owns (cross-area row PK2).
4. At each prologue the installer drains the completion queue. When the archive's next-offset buffer is filled and no hash job of that archive is running, it submits one background job (`bg/Package.Verify`) that feeds the contiguous filled buffers, in offset order, to the archive's `Zenith_Sha256Stream` and marks them free; the stream is used by one job at a time and handed over at job boundaries. Freed buffers are refilled by new reads at the next prologue.
5. After the last chunk, the digest is compared with the manifest's; a mismatch is V2. When every archive of the unit matched, V1.
6. A7 sets the job's cancel flag and requests cancellation of the in-flight reads; V3 is raised only after every read and the job have reported complete, so no buffer is freed while it is written ([Z2-N-CONTRACT-335]).

Throughput is bounded by `buffers × chunk` per archive per frame, 4 MiB per frame with the defaults (about 240 MB/s per archive at 60 fps, 480 MB/s with two concurrent packs), which the verification throughput floor row measures.

### Verified cache

Q90 is ruled (a) (HUMAN RULED 2026-10-16, `C:/dev/Zenith2-drafts/findings/OPEN_QUESTIONS_FOR_HUMAN.md` row Q90): a file verified once is not re-hashed while its identity stamp is unchanged, for delivered packs, committed patches and Windows DLC alike. The signature of every update manifest (U1–U3) and its targets (U4–U6, U8, U11) are still checked at every start, a pack's version and manifest identity are still compared at every start, and the cooked files' own header checks remain. `Zenith2/Docs/Architecture.md` → "Security architecture" carries the ruling, amended in core-doc patch 2026-10-16g (row X366 of `C:/dev/Zenith2-drafts/reviews/CROSS_DOC_ITEMS.md`): [Z2-N-ARCH-955] and [Z2-N-ARCH-956] say "verified once, re-hashed when its stamps change", [Z2-N-ARCH-966] places the record inside the trust boundary "patches and downloaded content", and row X376 restates [Z2-N-ARCH-966]'s premise as item 6 below does. `package.reverifyOnStartup` (bool, false) turns every skip off for a run: every pack, patch file and DLC file is then re-hashed in full, and the records are still written.

1. **Stamp.** `Zenith_FileIdentityStamp` (48 bytes, PLAT's type, cross-area row PK27 (a); its serialized form is Formats.md "Delivery state records") is the file's size, last-write time and change time in nanoseconds since the Unix epoch, and its identity: on Windows `FileBasicInfo.LastWriteTime` and `.ChangeTime` (100 ns units since 1601, converted as `(t − 116,444,736,000,000,000) × 100`) and `FileIdInfo` (the 64-bit volume serial number and the 128-bit file ID) through `GetFileInformationByHandleEx` on an open handle; on Linux and Android `fstat`'s `st_size`, `st_mtim` and `st_ctim` (`tv_sec × 10^9 + tv_nsec`), `st_dev` and `st_ino` (in the low 8 bytes of the identity, the high 8 bytes zero). A user process can set the last-write time (`SetFileTime`, `utimensat`) but not the change time or the file identity, so a rewrite that restores the last-write time still changes the stamp. A directory rename on the same volume changes neither the change time nor the identity of the files inside it (NTFS keeps each file's reference number; ext4, F2FS, XFS and Btrfs keep their inodes), so a stamp taken in `.incoming` stays valid after the commit's rename; renaming a file itself advances its own change time. Stamps are taken through `Zenith_MountSource::GetIdentityStamp(strRelative, xOut) const` (PK27 (a)): PLAT's directory source implements it with the calls above, `Zenith_InstallTreeMountSource` (PublicAPI.md "Zenith_UpdateApplier header") forwards it to `Zenith_InstallFileSystem::GetIdentityStamp` through an open, stamp and close inside the call, and every other source (an archive source) answers false, which always means "hash it". Both implementations answer false (directory source) or `PACKAGE_ERROR_NOT_SUPPORTED` (install file system) unless the file's volume is in the supported set of "Durability model" (`fstatfs` ext4 or F2FS on Android; ext4, XFS, Btrfs or tmpfs on Linux; NTFS or ReFS on Windows), so a pack location on FUSE, sdcardfs or adoptable storage, where change time and inode are synthesised, is always hashed. Two Android caveats are accepted because each only costs a re-hash, which is safe: `st_dev` of `/data` is a device-mapper minor that can change across a boot or an OTA, and an SELinux relabel, a `chmod`, an xattr write or a link-count change advances `st_ctim` without changing a byte.
2. **When it is taken.** By path (each `GetIdentityStamp` opens its own handle), immediately before a file's first read and again immediately after its last read, while the bytes are read through other opens. The guarantee is the bracket, not a single handle: any replacement of the file between the two stamps changes the file identity, any in-place write or rename-back changes the change time, and either difference is caught. What is recorded is the first stamp, and only when the two are equal. A difference for a pack is V2 with the reason `changed during verification`; for a bundle at B6 it is not a refusal: the bundle is left in `.incoming` for the next start (B6 in "Install steps per operation"), because a metadata-only touch (a backup tool's archive bit, a cloud-sync client, the indexer, a relabel) changes the change time of a correct file. The racy-timestamp rule (git's): a first stamp is taken only when `max(last-write, change)` is at least 100 ms older than the current time on the stamps' scale (`Zenith_InstallFileSystem::GetCurrentFileTimeNs` for B6 and recovery, the host's real-time clock for packs and PLAT); otherwise the verifier waits one prologue (packs) or sleeps 100 ms once with `Zenith_SleepUs(100000)` (B6, and recovery's re-stamp of item 4) before taking it, and a file still within 100 ms after that wait is verified without being recorded. A write landing in the same timestamp tick as the file's previous change therefore cannot leave two equal stamps around different bytes. PLAT applies the same rule to its own stamps (PK27 (c)).
3. **Packs.** After a full verification succeeds (V1 of the pipeline), the pack's delivery-state record is written with verified 1, Play's pack version, the base `package.zpkm`'s content hash, the SHA-256 of the UTF-8 assets host path and one stamp per archive (Formats.md "Delivery state records"). A3 begins with the cache check unless `package.reverifyOnStartup` is set: when the record is verified, its pack version equals Play's current one, its manifest content hash equals the running `package.zpkm`'s and its location digest equals the SHA-256 of `GetLocation`'s path, the installer takes each archive's current stamp through the directory source over that path (no read is issued) and, when every one equals its record, raises V1 in the same prologue without reading, counting one `package.verify.cache_hits` per archive; any difference, an unverified record or a failed stamp query runs the full pipeline of "Verification pipeline". The record's verification fields are cleared (verified 0, its stamps dropped, the state file dirty) when the pack enters REMOVING, NOT_INSTALLED or FAILED, at V2, and at a P6 whose pack version differs from the record's; AWAITING_ENTITLEMENT keeps them, because its files are kept.
4. **Patches and Windows DLC.** The applier records the stamps of every file it verified. B6 takes, through `Zenith_InstallTreeMountSource`, every listed file's stamp before `Zenith_VerifyUpdateManifestFiles` hashes it and again after (item 2), and keeps the first set in memory as a `Zenith_VerifiedDirectory` (at most 65,536 files) with the SHA-256 of the manifest's bytes. The commit's step "write the verified stamp file" (patch install step 3b, DLC install step 4b of "Install steps per operation") reads the current `verified.zinv` from disk (absent or refused: empty), decodes it into a `Zenith_VerifiedStampSet`, drops every record whose directory is absent (`Stat`) or is a move record of this journal, replaces the target's record with B6's, applies the overflow rule (Formats.md "Verified stamp file") and writes the encoding. The step is best-effort: on any error it deletes `verified.zinv.tmp`, logs `package: verified stamps not written (<error>)`, counts `package.install.stamp_write_failures`, leaves the previous file untouched and lets the commit continue, so a cache write can never hold an install in COMMITTING; the target then has no record. When this process did not run B6 for the target (recovery continuing at step 3 or 4 after a kill, rows 4, 5 and 8 of "Recovery table"), the target's record is taken from the previous file when it carries the SHA-256 of the committed manifest; otherwise recovery rebuilds it with B6's stamp, hash and stamp pass over the committed directory (read-only, one full hash on the crash path only) and writes it, or writes none when a file fails the hash or changed during the pass (the mount then refuses or re-hashes it). At the close of startup step 4 the bridge reads the file (one `Open`, a `Read` of at most `ulPACKAGE_MAX_VERIFIED_FILE_BYTES`, `Close`) and creates a `Zenith_VerifiedStampFile` from it, a `Zenith_UpdateStampLookup` (PLAT's interface, PK27 (b)), which the composition root passes to PLAT's patch mount at step 5 (PK27 (d)) and the bridge to the service's DLC mounting at step 11; a missing or refused file, or `package.reverifyOnStartup`, passes none. Both mounts always run U1–U8 and U11 and then `Zenith_VerifyUpdateManifestFiles` with the lookup (PK27 (c)), which re-hashes a listed file only when the lookup has no record of it, the record's manifest digest differs from the SHA-256 of the manifest bytes just verified, or its current stamp differs from the record in any of its five fields; a skipped file is counted (PLAT's `vfs.patch.stamp_skips` for the patch mount, `package.verify.cache_hits` for DLC). A committed directory with no record (a failed best-effort write, a target dropped by the overflow rule) is re-hashed in full at every start until a later commit records it, logged once per start (`package: <directory>: no verified stamps; re-hashed in full`). Records of directories that no longer exist are ignored by the lookup and dropped at the next write.
5. **Migration.** The built-in migration step `Package.ClearVerifiedCache` ("Built-in migration steps") clears every pack record's verification fields after an update of kind CONTENT, WITHIN_LINE, NEW_LINE or DOWNGRADE, so every delivered pack is re-verified in full against the new `package.zpkm` before it mounts. `verified.zinv` needs no step: a new full release on Windows replaces the base package only, and each record names its own manifest's digest.
6. **Trust precondition.** The stamp skip trusts `verified.zinv`, and for packs the delivery state file, exactly as much as the files they cover. Both are unauthenticated user state: anyone able to write the install root (Windows) or the app-private storage (Android) can rewrite an archive and write a record holding its new stamp together with the genuine SHA-256 of the untouched signed manifest, and the mount then skips the hash. The Ed25519 signature is checked at every start but does not bind a file's bytes once its record is trusted. Q90 (a) stands because that party can already replace the executable, which the boundary "patches and downloaded content: authenticated before mounting" does not defend against ([Z2-N-ARCH-966], restated by row X376). The Stage 1 security-architecture ticket (SEC, pending) confirms three items: (a) the stamp set of item 1; (b) that the records are unauthenticated and protected only by the write permission of the install root or the app-private storage, never more writable than the executable in any supported layout; (c) the escalation if a distribution layout ever makes `.zenith2-install/` more writable than the executable: a keyed record, a MAC over each record under a secret kept outside the install root.

<!-- informative -->
The cold-boot cost the cache saves is measured, not assumed: the pending row "Cold-boot update verification time" (entry document → "Pending new rows") measures W-Load with a patched install, an installed DLC and a delivered pack, at the first start after the install (full hash) and at the second (stamp checks), each reported separately.
<!-- /informative -->

### Mounting and unmounting

Mount (A4), on the main thread in the prologue. One mount operation first allocates, from the installer's `m_pxHeap`, one `Zenith_PackReleaseBlock` (an `Internal/Delivery/` heap block of 24 bytes: an atomic owner count, an atomic released count, the expected count E, the unit index and the link of the pack's draining list) with E = the unit's archive count and one owner for the installer; the record's `m_pxCurrentBlock` points at it. Then, for each archive of the unit in archive order:

1. wrap a directory source over the assets path in a `Zenith_PackContainerSource` (an `Internal/Delivery/` class implementing `Zenith_MountSource` by forwarding to the inner source), allocated from `m_pxHeap`; the wrapper co-owns this operation's block: it adds one owner at construction and, in its destructor, increments the block's released count (release ordering) and drops its owner; the last owner to drop frees the block, so a wrapper destroyed after the installer (the virtual filesystem's destruction in shutdown step 10b) touches only memory it co-owns;
2. `Zenith_CreateArchiveMountSource(container, archiveName, *m_pxArchiveReader)` (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetArchive header"`), which runs ASSET's archive checks and allocates the archive source's tables;
3. `Zenith_VirtualFileSystem::AddSource(MOUNT_KIND_PACKAGE, source, unit mount order)` (`Zenith2/Docs/Design/PLAT/Filesystem.md → "Zenith_VirtualFileSystem API"`), recording the source ID.

A failure at any archive, an allocation failure of the block or of a wrapper included (`MOUNT_FAILED`), removes the archives already added (M2) and sets the block's E to the number of wrappers created; the block is moved to the pack's draining list (below). On M1 and on every X, `RunPrologue` reports `m_bMountsChanged`, and the runtime bridge calls `Zenith_AssetRegistry::OnMountsChanged`, so every load of the pack's assets that failed `ASSET_ERROR_NOT_INSTALLED` is retried after the next catalogue swap (`Zenith2/Docs/Design/ASSET/Loading.md → "Retry after a mount change"`), and an unmounted DLC's catalogue fragment leaves the merged catalogue at the same swap.

Between two `AddSource` calls of one pack, an I/O thread may resolve a path against a table that holds some of the pack's archives and not others. That is consistent: each asset is one archive entry; an asset whose hard dependency sits in a not-yet-added archive of the same unit fails that dependency with `NOT_INSTALLED` and is retried after the mount change, as for any undelivered content; and no load of the pack's assets is issued for the first time before the catalogue swap that follows `OnMountsChanged`.

Unmount (A5): `RemoveSource` for every recorded source ID, in reverse order; PLAT destroys each source when the last request resolved against it completes (`Zenith2/Docs/Design/PLAT/Filesystem.md → "Zenith_VirtualFileSystem API"`), and each destruction reaches its container wrapper. X fires at the first prologue (or `PumpShutdown`) at which the current block's released count equals its own E; the installer then drops its owner of that block and clears `m_pxCurrentBlock`, so the next A4 starts from a fresh block and a later unmount never reads an earlier mount's count. In-flight reads therefore always complete against files that still exist; the files are deleted (A6) only after X.

The draining list: an M2 rollback (A5 without waiting) does not wait for X; its block, with E = the wrappers it created, moves from `m_pxCurrentBlock` to the head of the record's draining list (an intrusive singly linked list through the blocks, so moving allocates nothing). At each prologue the installer drops its owner of every draining block whose released count reached its E and unlinks it. Late releases of a rolled-back mount therefore increment only their own block, which nothing compares with another mount's count, and no A6 of the pack is issued while its draining list is not empty (the A6 rule of "Asset pack transitions"), so a file a rolled-back mount's in-flight read still reads is never deleted (I5). A Windows DLC mount uses the same block, wrapper and draining list ("Windows DLC mounting").

Allocation: A4 is the only runtime allocation of the installer after Create: one block, one wrapper per archive (at most 100, `uPACKAGE_MAX_ARCHIVES_PER_UNIT`) and ASSET's archive source per archive, from the installer's heap; it happens at most once per pack per verification, and its failure is M2 with `PACKAGE_ERROR_MOUNT_FAILED`, never an assert.

### Shutdown drain

`BeginShutdown` (shutdown step 4) applies the shutdown rule of "Asset pack transitions": A7 on every VERIFYING pack and A5 on every MOUNTED pack and every mounted Windows DLC (PLAT admits `RemoveSource` at shutdown step 4, cross-area row PK14). No prologue runs after shutdown has begun (Contracts step 1 stops the frames), so the events that complete the wait are raised by `PumpShutdown`: it drains the installer's I/O completion queue and the finished hash jobs (raising V3 for every verification whose reads and job all reported complete), compares each current block's released count with its E (raising X), applies the resulting cells, drops every fully released draining block, and returns `IsShutdownComplete()`, which is true when no pack is VERIFYING, STOPPING_VERIFICATION or UNMOUNTING. Because the shutdown rule suppresses A3, A4 and S, no cell `PumpShutdown` applies can start a verification or a mount, so every pack the drain resolves stays resolved and `IsShutdownComplete()` never turns false again. `Zenith_RuntimePackage_BeginShutdown` loops `PumpShutdown`, then `Zenith_SleepUs(1000)` (the hash jobs run on the job system's workers, alive until step 7), until it returns true or `package.shutdownDrainMs` (1,000 by default, 0 … 5,000) has passed since `BeginShutdown`. At the bound it logs `package: shutdown: <n> packs still <state> after <ms> ms` per pack and continues, which is safe: no file is deleted at shutdown (A6 is suppressed), every unreleased wrapper co-owns its block, and a cancelled read or hash job finishes within one chunk; the installer's destructor (step 6) still waits for every outstanding read and job of its verification arena, as [Z2-N-CONTRACT-335] requires, by draining its completion queue until it is empty.

The delivery state write: after the loop, `Zenith_RuntimePackage_BeginShutdown` writes `delivery-state.zdls` only when the state is dirty, that is when a change since the last per-change write of Formats.md "Delivery state file" is pending; the shutdown rule changes no intent, so only a change of the last prologue can be pending. It first waits for a write already in flight, within the same `package.shutdownDrainMs` bound, then issues one `WriteAtomic` (at most 32 + 256 × 80 + 25,600 × 48 + 16,384 = 1,265,696 bytes) and waits for it. On RHI's device-loss clean exit (`bDeviceLossExit`, passed by the composition root exactly when it called `Zenith_RuntimeSave_RequestAutosaveForExit`, `Zenith2/Docs/Architecture/CompositionRoot.md → "The frame loop"` step 4) it neither writes nor waits: the last per-change write is at most one frame old, so at most the intents changed by the last prologue are lost, and the next start re-queries Play and resumes the intents persisted one frame earlier. PACK's part of the clean exit is then the drain alone, at most `package.shutdownDrainMs` (1,000 ms), whose waits are on cancelled reads (each one storage call, already issued) and on source releases, inside the 3 s that SAVE's device-loss split leaves to the rest of the shutdown order (`Zenith2/Docs/Design/SAVE/Algorithms.md → "Shutdown sequence"`); PACK adds no new storage call to it. `Test_AssetPackInstaller_ShutdownWithMountedPack_ReleasesBeforeDestroy` and `Test_AssetPackInstaller_ShutdownCancelsVerification` drive `PumpShutdown`.

### Fetch policy

- Auto fast-follow: with `m_bAutoFetchFastFollow`, every FAST_FOLLOW pack in NOT_INSTALLED or FAILED at Create receives an automatic F once per process; Play normally starts them itself after install, and this covers a start before Play did or after a failure.
- Resume at startup: every pack whose persisted intent is `WANT_INSTALLED` and whose first reported state is NOT_INSTALLED, FAILED or CANCELED receives an automatic F (the UNKNOWN row of "Asset pack transitions"); one whose intent is `WANT_REMOVED` receives R. A cancelled pack's intent is `NONE` (every C cell clears it), so it is never resumed. Under `--replay` nothing is resumed and nothing is fetched automatically ("Replay pack set").
- Attempts: automatic re-fetches after NOT_INSTALLED stop after attempt 3 of a process; network retries stop after `uPACKAGE_MAX_NETWORK_RETRIES` (3) per process; a game request always issues a new attempt and clears both limits' state for the pack.
- Order: fetches issued in one prologue go to Play in one `fetch` call, in `package.zpkm` unit order; Play schedules them.

### Network retry

A network loss during a Play download surfaces as FAILED with `NETWORK_ERROR` (P7N), or as a DOWNLOADING that stops advancing until Play itself continues. For P7N with intent `WANT_INSTALLED` the installer enters WAITING_FOR_NETWORK and schedules a retry (A9); N re-issues the fetch (A10), and Play resumes from the bytes it already holds.

- N is raised in `RunPrologue` from its frame input (`Zenith_InstallerFrameInput`): when `m_ulNowUs ≥ m_ulNetworkRetryAtUs`, or when `m_bConnectivityRegained` is true, and only while `m_bForeground` (the platform's `APP_STATE_RUNNING`), because `fetch` fails in the background.
- `m_bConnectivityRegained` is true when PLAT's `Zenith_Platform::GetConnectivityRevision()`, which counts OFFLINE to ONLINE transitions only, advanced since the bridge's previous prologue (`Zenith2/Docs/Design/PLAT/Platform.md` → "Connectivity"); otherwise the back-off of `package.networkRetrySeconds` (30 s by default) alone governs.
- After the third retry fails, the pack is FAILED with `NETWORK_UNAVAILABLE`; the game's next request starts again.
- The simulated delivery's NETWORK_LOSS fault reproduces exactly this: FAILED with `NETWORK_ERROR`, then every fetch fails the same way at the next poll until `ResolveNetworkLoss` ("Simulated delivery").

### Initial query

`Zenith_AssetPackInstaller::Create` issues `RequestStates` for every pack and, inside the service's Create, pumps `Poll` for at most `m_uInitialQueryTimeoutMs` (500 ms), yielding between polls (`Zenith_SleepUs(1000)`), so packs that are delivered are verified and mounted as early as the pipeline allows. A pack whose delivery-state record still matches ("Verified cache" item 3) passes the cache check in the first prologue that sees it COMPLETED and mounts there; a pack that needs the full pipeline normally finishes after startup step 17, and its assets resolve `ASSET_ERROR_NOT_INSTALLED` with ASSET's fallback until it mounts. A pack whose state has not arrived stays UNKNOWN and follows the tables when it does.

### Simulated delivery

`Zenith_SimulatedAssetPackDelivery` behaves as Play does for the installer, deterministically, with no thread. It has two storage modes, chosen by its desc:

| Mode | Selected when | Pack files |
|---|---|---|
| copying | `m_pxFiles` is not null (tests only; the runtime bridge never passes a file system) | copied from `<source root>/<pack>/` to `<target root>/<pack>/<packVersion>/assets/` through `m_pxFiles` (the memory file system, or the host one over a test's temporary directory); `GetLocation` returns the target path |
| in place | `m_pxFiles` is null (the development runtime with `package.simulatedDelivery`) | never written: `GetLocation` returns `<source root>/<pack>/` once COMPLETED; download progress is only counted; CORRUPT_BYTE and TRUNCATE faults are refused at Create (`INVALID_DESC`) |

So no Player writes outside the user mount: the in-place mode reads the development source root through PLAT's directory source, the read-only exception [Z2-N-ARCH-923] names.

1. `Fetch` marks a pack PENDING (sequence + 1); each subsequent `Poll` advances it by `m_ulBytesPerPoll` bytes (copying them in the copying mode), reporting DOWNLOADING with the byte counts, then one TRANSFERRING update, then COMPLETED.
2. A scripted fault applies when the pack's delivered bytes reach `m_ulAtByte` in the fault's attempt: NETWORK_LOSS reports FAILED with `NETWORK_ERROR`, keeps the bytes, and sets the delivery offline, so every `Fetch` until `ResolveNetworkLoss` reports FAILED with `NETWORK_ERROR` at the next poll and a fetch after it resumes from the same byte; FAIL reports FAILED with the fault's error and keeps the partial bytes; CORRUPT_BYTE and TRUNCATE alter the delivered bytes so that verification fails; WAIT_FOR_WIFI reports WAITING_FOR_WIFI and REQUIRE_CONFIRMATION reports REQUIRES_USER_CONFIRMATION, each until `ShowConfirmation` is answered with acceptance (`SetConfirmationAnswer(true)`, the default), which reports the result in the confirmation cell and resumes; REMOVE_FAILS makes the next `Remove` report REMOVE_FAILED once.
3. `Cancel` reports CANCELED and keeps the partial bytes (as Play may); `Remove` deletes `<target root>/<pack>/` (copying mode), forgets the pack's progress, and reports REMOVED in the pack's result cell.
4. `SimulateRequesterRestart` forgets every cell the requester has not polled, keeps the copied bytes and the download in progress, and answers the next `RequestStates` with the current state, as Play does for a relaunched app.

### Simulated deliverable units

A Windows-hosted run has no deliverable unit under the WINDOWS layout, which folds every non-DLC unit and marks it PRESENT, and an ANDROID_BUNDLE `package.zpkm` on Windows fails `CheckIdentity`'s layout check. Development and test runs that exercise delivery on a Windows (or Linux) host therefore use one more layout, `PACKAGE_LAYOUT_SIMULATED` (4), never shipped. This section is its one statement; the other sections refer here.

1. **Packaging.** With `package.configuration` Debug or Release, a Windows `--package` run also writes `<package dir>/Simulated/`, published with the other layouts (Tooling.md "Package build steps" steps 6–10):
   - `Content/`: the base and INSTALL_TIME archives of the ANDROID_BUNDLE partition (the unfolded unit set), their `mount-manifest.json` listing them under `package[]`, and in the base archive a `Package/package.zpkm` of layout SIMULATED;
   - `Packs/<unit>/<archive>`: the archives of every FAST_FOLLOW, ON_DEMAND and DLC unit, exactly as the bundle's packs hold them;
   - `package.zpkm`: a plain copy of that manifest, read by development runs that mount no package (item 3).
   The SIMULATED manifest carries the ANDROID_BUNDLE partition's units and archives, with flag bit 2 set exactly on the base and INSTALL_TIME units (check M11), and the identity of the Windows build: its build ID, release line and engine cook version, and the content hash of `Simulated/Content/`'s base catalogue. A Shipping package writes no `Simulated/`.
2. **Selecting the root.** `package.simulatedDelivery.root` names the `Simulated/` directory, as an absolute host path or relative to the assets root; it is read only when `package.simulatedDelivery` is set, and never in a Shipping build.
3. **Unit list.** "Package service startup" step 1 reads `content:/Package/package.zpkm`; when it is absent, `m_bShippingRules` is false and `package.simulatedDelivery` is set, the service reads `<root>/package.zpkm` instead (absent or invalid there: no units and a warning, as for a missing manifest).
4. **Identity.** `CheckIdentity` accepts layout SIMULATED only with `bCompareBuildID` false (outside Shipping rules) and on a Windows or Linux host, against the host identity: the release line and platform always, and the content hash against `m_ulPackageContentHash` when the manifest came from the mounted package. A manifest read from `<root>/package.zpkm` under item 3 is not compared with the mounted development tree's content hash, which is not the partition's base. An ANDROID_BUNDLE manifest is never accepted on Windows.
5. **Delivery.** A SIMULATED manifest's FAST_FOLLOW, ON_DEMAND and DLC units are deliverable (`IsDeliverable`), and the bridge creates the in-place simulated delivery ("Simulated delivery") with `m_strSourceRoot` = `<root>/Packs`. The installer, its verification against the SIMULATED manifest's digests, and the mounts are the ones an ANDROID_BUNDLE run uses.

So a packaged test run boots from a copy of `Simulated/Content/` (Z2T-PACK-059's fixture), and a development run from the loose cooked tree with the root set (Z2T-PACK-069's gametest).

### Sideload builds

A sideload APK carries the ANDROID_SIDELOAD layout's `package.zpkm`, whose every non-DLC unit has the PRESENT flag and which lists no DLC unit (Formats.md "Package manifest tables"), and a mount manifest listing every non-DLC archive under `package[]`: the service finds no deliverable unit and creates no delivery and no installer, and every unit reads MOUNTED. A bundle build whose Java bridge is missing (`Zenith_CreateAssetPackDelivery_Play` returns `PACKAGE_ERROR_DELIVERY_UNAVAILABLE`) runs with its deliverable packs FAILED (`DELIVERY_UNAVAILABLE`), logged once.

## Package service startup

`Zenith_PackageService::Create` runs as the closing action of startup step 11 (after the asset registry and SAVE's system exist, before the RHI device), on the main thread:

| # | Step | On failure |
|---|---|---|
| 1 | read `content:/Package/package.zpkm` (`ReadWhole`, at most 5,374,016 bytes) and `Zenith_PackageManifest::Parse`; when absent outside Shipping rules with `package.simulatedDelivery` set, `<package.simulatedDelivery.root>/package.zpkm` ("Simulated deliverable units" item 3) | with `m_bShippingRules`: return the error (startup exit code 3); otherwise a service with no units and a warning |
| 2 | `CheckIdentity` against the pre-patch identity: the build ID (Shipping rules only, `m_bShippingRules`, as PLAT compares it), `Zenith_BuildIdentity::m_ulPackageContentHash` (the mount manifest's `contentHash`, which a patch never changes, cross-area row PK14) and the layout's platform against `m_ePlatform` (layout SIMULATED: "Simulated deliverable units" item 4) | log; deliveries and DLC disabled for the run |
| 3 | read `user:/cache/package/install-record.zinr`; `Zenith_ClassifyUpdate` ("Update classification") | a corrupt record classifies as FIRST_RUN |
| 4 | run the installed-update migration when the kind is not NONE or the record's phase is PENDING ("Installed-update migration"); the update info (kind, from, to) is kept for every world's snapshot | a failing step is counted and its fallback runs; Create continues |
| 5 | Windows: examine every DLC directory and mount every one that verifies and whose entitlement the forwarding query answers GRANTED ("Windows DLC mounting"; PLAT admits `AddSource` here, cross-area row PK14); at this step the query answers UNKNOWN unless the composition has no `Online` module ("Entitlement provider"), so an entitled DLC normally mounts at the first prologue after step 15 | per DLC: logged, its state FAILED with the error (worlds learn it from their snapshot) |
| 6 | when a deliverable unit exists: create the installer (initial query, auto fast-follow, resume) | log; packs FAILED (`DELIVERY_UNAVAILABLE`) |
| 7 | report `m_bMountsChanged` so the bridge calls `OnMountsChanged` before step 15's pinned loads | — |

Events the service and the installer queue before the first prologue are not published: no world has received its snapshot yet, and every world's snapshot carries the state they describe ("World snapshot").

### Entitlement provider

The service's `m_pxEntitlements` is the bridge's `Zenith_ForwardingEntitlementQuery` (`Zenith2/Docs/Design/PACK/PublicAPI.md` → "Zenith_EntitlementQuery header"), owned by the runtime bridge from the closing action of step 11 until shutdown step 6. It answers from one provider, or UNKNOWN for every ID while it has none:

| Point | Provider set | Who |
|---|---|---|
| closing action of step 11, before `Create` | `Zenith_ConfigEntitlementQuery` over `package.grantedEntitlements` when the composition (fixed since step 6) lists no `Online` module; otherwise none | the bridge (`Zenith_RuntimePackage_Create`) |
| step 15, during the `Online` module's `Start` | ONLINE's `Zenith_OnlinePackEntitlements`, through `Zenith_ModuleStartContext::ProvideEntitlementQuery`, which MOD implements by calling `Zenith_RuntimePackage_SetEntitlementProvider` (cross-area row PK21) | MOD, for ONLINE |
| end of step 15, after `StartAll` | `Zenith_ConfigEntitlementQuery` when the forwarder still has no provider (a composition whose `Online` module provided nothing, for example before ONLINE's Stage 6 slice) | the bridge (`Zenith_RuntimePackage_AfterModulesStarted`, cross-area row PK21) |
| shutdown step 4, after `Zenith_RuntimePackage_BeginShutdown` returned | none: every ID answers UNKNOWN, so nothing the service runs before its destruction in step 6 calls ONLINE's query, which step 6 destroys with its module first | the bridge |

The forwarder's revision is `(the number of provider changes) × 2^48 + (the provider's revision mod 2^48)`, so every provider change is a revision change and the service re-evaluates every entitlement-gated unit at its next prologue (events E and G of "Asset pack events"; "Windows DLC mounting" step 3). Moving from no provider to a provider raises G for every unit it grants and E for every unit it denies, whose cells end a pack waiting in AWAITING_ENTITLEMENT NOT_INSTALLED with `ENTITLEMENT_DENIED` and leave a NOT_INSTALLED pack as it is (no gated pack is verified or mounted while its answer is UNKNOWN, by the HOLD rule and the guard on A1; a gated pack Play is already downloading at Create follows the download cells until P6 holds it); the reset at shutdown step 4 is a change to UNKNOWN and raises nothing. `Test_PackageService_EntitlementProviderSwitch_RemountsDlc` and `Test_RuntimePackage_EntitlementProviderLifecycle` assert the table.

### Windows DLC mounting

For each `DLC/<name>/` directory (byte order of names; `.incoming` and `.trash` skipped), through a directory source over `<install>/DLC/<name>`:

1. read `dlc.zman`; `Zenith_DecodeUpdateManifest` (U1–U3, U7), `Zenith_CheckUpdateManifestTarget` (U4, U5, U8: the name against the directory), U6 by PACK, then U0 over `List(DLC/<name>)` of the bridge's install file system (the service desc's `m_pxInstallFiles`, which reports links): an extra entry, a subdirectory or a link refuses the mount (`UPDATE_MANIFEST_INVALID`) and a missing listed file is `UPDATE_INCOMPLETE`; then `Zenith_VerifyUpdateManifestFiles` (U9) through the directory source with the service's `Zenith_UpdateStampLookup` (null under `package.reverifyOnStartup` or without a valid `verified.zinv`), which re-hashes only the files whose stamp differs from the record ("Verified cache" item 4). Any failure leaves the DLC unmounted (`package: dlc <name>: <error>`) and the directory untouched (the runtime never deletes under `<install>`); the time of the hashes that do run is part of the cold-boot row;
2. find the DLC's unit in `package.zpkm` by name; a DLC with no unit, a unit that is not kind DLC, or a mount order other than the unit's is `PACKAGE_ERROR_DLC_INCOMPATIBLE`;
3. ask the entitlement query for the unit's entitlement: GRANTED mounts, DENIED leaves it unmounted (state NOT_INSTALLED with `ENTITLEMENT_DENIED`), UNKNOWN re-asks at every prologue until it resolves;
4. mount its archives in file-table order as "Mounting and unmounting" does (one release block per mount, the same draining list), with the unit's order;
5. the DLC's catalogue fragment merges at `OnMountsChanged`; an external reference that does not resolve makes the merge fail (`CATALOGUE_INVALID`, `Zenith2/Docs/Design/ASSET/Formats.md → "Catalogue fragments"`); the service polls `Zenith_AssetRegistry::GetCatalogueRefreshResult()` (cross-area row PK16) at each prologue after a mount change, and a result naming the DLC's fragment unmounts the DLC and reports `DLC_INCOMPATIBLE`.

Each change of a DLC's state queues one `Package.DlcStateChanged`; a change to MOUNTED carries the mounted revision's result content hash in the payload's bytes-done field (Formats.md "Package event payload"), which the replay of a recording compares ("Replay pack set" step 2). A revoked entitlement (revision change, now DENIED) unmounts the DLC at the next prologue (and the catalogue drops its fragment at the swap that follows the mount change). Removing a Windows DLC is the distributor's job: a launcher, Steam or the player deletes `DLC/<name>` while the game is not running; the runtime never deletes under `<install>`, and `BeginRemoveDlc` exists for the applier's tests and for a distributor tool built on the library.

## Update classification

`Zenith_ClassifyUpdate(record, running)`, in this order:

| # | Condition | Kind |
|---|---|---|
| 1 | no record, or a record refused by R1–R5 | `FIRST_RUN` |
| 2 | identity equal | `NONE` |
| 3 | release line (major, minor) lower than the record's | `DOWNGRADE` |
| 4 | release line higher | `NEW_LINE` |
| 5 | build ID differs | `WITHIN_LINE` |
| 6 | same build ID; patch version lower than the record's, or lower than its highest patch seen | `DOWNGRADE` |
| 7 | same build ID; content hash differs | `CONTENT` |
| 8 | otherwise (composition hash differs with everything else equal) | `WITHIN_LINE` |

Row 6 and the floor of U10 rely on one numbering rule, recorded as ruling R16 of the entry document and enforced by SHIP's release pipeline (cross-area row PK25): every release and every patch of a release line carries a version greater than every earlier release and patch of that line, whichever build it targets. The patch builder's version is the new release's own `version` (Tooling.md "Patch builder" step 1), so the rule is one rule over the release versions SHIP publishes. A full release of the line therefore continues the line's numbering (after patch 1.2.5 of build A, build B's release is 1.2.6 or later, and its first patch 1.2.7 or later): its patches always clear the install state's highest patch (U10) and the record's highest patch seen (row 6), which both keep counting across builds, and U5 alone keeps another build's patch out. A build that broke the rule would see every patch numbered at or below the floor refused `UPDATE_NOT_NEWER` and classified DOWNGRADE, which is the documented consequence, not a recovery path.

## Installed-update migration

An installed update (a Play update of the app, a new full release on Windows, a mounted patch) changes the running identity. At Create step 4:

| Record phase \ Classified kind | NONE | any other kind |
|---|---|---|
| DONE | nothing | W1: write the record {identity running, previous the record's identity, last update the kind, phase PENDING}; run the registry for the kind; W2: rewrite with phase DONE and the highest patch seen updated; the update info is kept for every world's snapshot (`Package.Updated`) |
| PENDING (a kill or power loss during the previous migration) | re-run the registry for the record's last update kind; W2 | as DONE (the newer update supersedes the interrupted one) |

- W1 and W2 are `WriteAtomic` writes waited for on the main thread (startup, not a job). A kill after W1 re-runs every step, which is why steps are idempotent.
- The highest patch seen: W2 writes the maximum of the record's value and the running patch version for CONTENT, WITHIN_LINE and NONE, and the running patch version itself for NEW_LINE and DOWNGRADE (a new release line, or a lower one the player reinstalled, starts its own count).
- A FIRST_RUN writes the record directly with phase DONE and the running patch version as the highest seen, and runs no step (there is nothing to migrate).
- A W1 or W2 refused for low disk or any I/O error is logged (`package: install record write failed: <error>`), the migration result stands for the run, and Create continues; the next start classifies again from the record that is on disk.
- Save migration is SAVE's, applied when a save loads (`Zenith2/Docs/Design/SAVE/Algorithms.md → "Migration pipeline"`); the resume state of the previous build is discarded by PLAT's identity check (`Zenith2/Docs/Design/PLAT/Android.md → "Resume identity"`). PACK's migration covers PACK's own state and any owner's cache that registers a step.

### Built-in migration steps

| Step | Order | Kinds | Effect | Fallback directory |
|---|---|---|---|---|
| `Package.ClearVerifiedCache` | 10 | CONTENT, WITHIN_LINE, NEW_LINE, DOWNGRADE | clears every pack record's verification fields (verified 0, stamps dropped), so every delivered pack is re-verified in full against the new `package.zpkm` before it mounts ("Verified cache" item 5) | none (the state file is rewritten) |
| `Package.ForgetRemovedUnits` | 20 | WITHIN_LINE, NEW_LINE, DOWNGRADE | drops state records of packs no longer in `package.zpkm`; Play deletes their data with the update | none |
| `Package.ReportUpdate` | 1000 | every kind except NONE and FIRST_RUN | records the kind and the from and to versions in the service's update info, which every world's snapshot publishes as one `Package.Updated` ("World snapshot") | none |

PACK registers exactly these three steps. `verified.zinv` needs none: each of its records names the SHA-256 of its own directory's manifest, and the base package is never in it.

Other owners' steps are registered by the composition root in its call at the close of step 11, with the owner's cache directory as fallback; a failing step's fallback deletes `user:/cache/<dir>/` wholesale, which every `user-state` owner can rebuild.

## Update compatibility rules

| Change between installed and new | Full release (Windows full package, Play app update) | Content patch | DLC revision | Asset pack |
|---|---|---|---|---|
| executable code (build ID) | yes | no: the patch builder refuses (`PATCH_EXECUTABLE_CHANGED`); the verifier refuses at mount (U5) | yes (DLC targets any build of the line) | with its app version only |
| release line | yes (patches of the old line become obsolete and are deleted at the next start, O1) | no (`PATCH_INCOMPATIBLE_LINE`, U4) | no (U4) | with its app version |
| engine cook version | only with a new release line (`PATCH_COOK_VERSION_CHANGED_IN_LINE` refuses a release that changes it inside a line) | no (U6) | no (U6) | with its app version |
| composition hash (modules, backends) | yes | no (`PATCH_COMPOSITION_CHANGED`) | no: a DLC is verified against the base catalogue at merge | with its app version |
| data-defined schemas (`schemas.zreg`), script modules list | yes | yes (base files may change; the patch ships the new ones) | no (`DLC_SCHEMA_CHANGE`: the registry freezes at step 10, before DLC mounts at the close of step 11) | no (schemas are base files) |
| cooked assets | yes | yes, at file granularity, overriding the package (and a pack's entries) at patch precedence | additions only (the DLC's own assets) | yes |
| `package.zpkm` | yes (it describes each full release's base package) | never: it always describes the base package, so a patch neither ships it nor re-digests a pack (`Patch diff` step 3) | never | never |
| saves | migrated by SAVE within a major release line ([Z2-N-CONTRACT-524]) | unchanged | unchanged | unchanged |
| formats of class `migrate` or `user-state` | never shipped | never shipped (`PATCH_FORBIDDEN_FILE`) | never shipped | never shipped |

- Within one release line the engine cook version never changes, which is what makes a DLC cooked for the line valid on every build of it ([Z2-N-CONTRACT-528]: "a patch applies only within one release line").
- A patch is cumulative against the release line's base package, whose content hash it names (U11): the patch mount mounts only the highest verified version, never a chain (`Zenith2/Docs/Design/PLAT/Filesystem.md → "Patch mount verification"`).
- Identity rule, one for every reader: `package.zpkm`, the mount manifest's `contentHash`, `Zenith_BuildIdentity::m_ulPackageContentHash` and a patch's base content hash all name the base package; a patch's result content hash, `Zenith_BuildIdentity::m_ulContentHash` and the install record's identity name the effective (patched) content; `CheckIdentity` and pack verification use the first, update classification uses the second.
- Downgrades are refused for patches (U10) and accepted for full releases (a player reinstalling an older version), which classifies as DOWNGRADE and lets SAVE refuse saves of a newer major.

## Partition algorithm

Input: the shipping-profile cooked tree's catalogue (every entry with GUID, kind, logical path, object path, bytes, hard dependencies), its file entries, its global outputs, and the recipe. Output: a unit for every file.

1. **Rule assignment.** For each asset in GUID order, the first recipe unit (array order) whose `include` matches its logical path, whose `exclude` does not, and whose `kinds` filter (when present) contains its kind; otherwise the base unit. Reason `rule <unit index>` or `default base`.
2. **Global files.** Every file entry and global output of the cooked tree (bytecode, `schemas.zreg`, shader containers, the PSO precache set, merged configuration, dictionaries, `assets.zcat`, `package.zpkm`) is base. DLC catalogue fragments are their DLC unit's.
3. **DLC integrity.** A non-DLC asset with a hard dependency on a DLC asset is `PACKAGE_BUILD_ERROR_DEPENDENCY_IN_DLC`; a DLC asset with a hard dependency on another DLC unit's asset is `PACKAGE_BUILD_ERROR_DLC_CROSS_DEPENDENCY`. A soft dependency may cross any unit: an undelivered target resolves `NOT_INSTALLED` at load.
4. **Promotion.** The always-present set is the base unit and every INSTALL_TIME unit (Play installs install-time packs with the app; on Windows and in a sideload APK every non-DLC unit is folded into the package anyway). An asset `X` in a FAST_FOLLOW or ON_DEMAND unit `U` is promoted to base when any hard dependent of `X` lies outside `U` (in base, in an INSTALL_TIME unit, or in another deliverable unit), because that dependent can be present while `U` is not. An asset of the base unit or of an INSTALL_TIME unit is never promoted: it is present whenever any dependent is. A worklist starts with every asset; promoting `X` pushes `X`'s hard dependencies; the loop ends when the worklist is empty. Each asset moves at most once (only towards base), so the work is O(E + D). Reason `promoted by <dependent GUID>`.
5. **Catalogues.** The base catalogue `assets.zcat` is rewritten to list every non-DLC asset (so an undelivered pack's assets are known and resolve `NOT_INSTALLED`) and no DLC asset; each DLC unit gets `Catalogues/<unit>.zcat`, a fragment of its assets with external references for its dependencies outside it (ASSET's writer, cross-area row PK6).
6. **Report.** `partition.json` (Formats.md "Size report and partition report").

## Chunking

Per unit: the unit's files sorted by path bytes; archives filled in that order; a new archive starts when the next entry would make the current one exceed `chunkTargetBytes` (header, padding and table included); an entry larger than the target is alone; more than 100 archives in one unit is `PACKAGE_BUILD_ERROR_TOO_MANY_ARCHIVES`. A one-archive unit's archive is `<unit>.zpak`, otherwise `<unit>-00.zpak` onwards. Archive bytes follow "Archives PACK writes" in Formats.md.

## Dictionary policy

The `pack.dictionaries` cook step (COMPRESS: "Dictionary training in the cook", owned by PACK) runs in `COOK_PHASE_GLOBAL_POST` of a shipping-profile cook, after GEOM's and RPROD's global steps and before `asset.catalogue` (`Zenith2/Docs/Design/ASSET/Cook.md → "Cook command line"`):

1. For each recipe class with `dictionary: true`: when the cook key `pack.dictionaryBase` names the release line's base package directory, the class's dictionary is not trained: its `.zdict` is read from that package's base archives (the entry whose GUID the derivation of step 3 gives) and used unchanged, so every patch of a line re-encodes with the base release's dictionaries and an unchanged file re-encodes to identical bytes ("Patch builder" in Tooling.md sets the key; a base package without the entry is `PACKAGE_BUILD_ERROR_COOKED_TREE_INVALID`). Otherwise the samples are the decoded sections of every cooked file of the class's kinds, files in GUID order, sections in index order, until 134,217,728 sample bytes (the remaining files are not sampled; the cap keeps training time bounded and the choice deterministic).
2. `Zenith_Compression::TrainDictionary` with the class's zstd level and the 112,640-byte capacity (`Zenith2/Docs/Design/COMPRESS.md → "Dictionary creation and training"`).
3. The dictionary asset's GUID is derived, not random: the first 16 bytes of `Zenith_Sha256Digest("Zenith.Package.Dictionary/" + recipe id + "/" + class name)` with the version-4 and variant bits set (the derivation SCENE uses for derived authoring IDs), so an incremental and a clean cook name it identically.
4. It is written as a `.zdict` cooked file (kind 17, `ASSET_KIND_COMPRESSION_DICTIONARY`, format version 1) into the base unit, with exactly two sections in this order: section 0 the 8-byte parameters record `{ uint16 codec ID (zstd), int16 level, uint32 dictionary bytes }` little-endian, section 1 the dictionary bytes; both stored (codec 0), loaded by ASSET's dictionary loader (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_BuiltInAssets header"`).
5. Every section of every file of the class is re-encoded with the dictionary through the cook's cooked-file rewrite (cross-area row PK6), which adds the dictionary's GUID to the file's hard dependencies (ASSET loads a dictionary as an `ASSET_KIND_COMPRESSION_DICTIONARY` hard dependency before decoding) and updates the file's output hash before the catalogue is written.
6. A class whose samples total less than 1,048,576 bytes gets no dictionary (training on less is not worth the per-file dictionary reference); the size report records it.
7. A section whose owner requires it to stay stored for byte-addressable reads (AUDIO's streamed sound data, row AU21; the cook-side query `Zenith_AssetKindSectionsMustStayStored` requested from ASSET by cross-area row PK16) is never recompressed by the profile or re-encoded with a dictionary, whatever the recipe says; a recipe class naming such a kind with a codec other than `none` is `PACKAGE_BUILD_ERROR_RECIPE_INVALID`.

## Patch diff

`ZenithCook --package-patch` (Tooling.md "Patch builder") computes a content patch from the line's base package and the new package (both WINDOWS layout):

1. Both package trees' `package.zpkm` must agree on build ID, release line, engine cook version and layout, and both `release.json` files on composition hash; the new version must be greater than the base's (`PATCH_NOT_NEWER`).
2. Build a map path → data hash from every entry of every base archive (ASSET's entry tables, read with `Zenith_CreateArchiveMountSource` over the package directory and listing).
3. For every entry of every new non-DLC archive: `Package/package.zpkm` is skipped and listed in the report's `skippedPaths` (it always describes the base package); any other entry is included when its path is absent from the map or its data hash differs. A file of class `migrate` or `user-state`, or any `.zman`, is `PATCH_FORBIDDEN_FILE`.
4. No included entry is `PATCH_EMPTY`.
5. Sort the included entries by path; chunk them into `patch-NN.zpak` with the recipe's chunk target.
6. Write the unsigned `update.zman` body (kind PATCH, target build ID, release line, version, cook version, base content hash = the base `package.zpkm`'s content hash, result content hash = the new package's catalogue content hash, the archives sorted by name with their SHA-256) and hand it to the signer.
7. Write `patch-report.json`: included paths, skipped paths, paths present in the base and absent from the new package (a patch cannot delete package content; these become unreferenced because the patched catalogue does not list them), byte totals.

## Event publication and replay

Package events reach simulation through the world's asynchronous result table, the one path [Z2-N-ARCH-792] admits, in the shape PLAT's platform events use ([Z2-N-ARCH-791]): source kind `ASYNC_SOURCE_PACKAGE` (its value is assigned by SCRIPT's registry, `Zenith2/Docs/Scripting.md → "Asynchronous source kind registry"`, requested by cross-area row PK13; PACK writes no number), publication kind `PUBLICATION_KIND_GENERIC_RECORDED`, the 96-byte payload of Formats.md "Package event payload".

1. **World binding.** At each world's creation (`Zenith2/Docs/Design/GAME/WorldHost.md → "World creation"` step 4, a PACK body in MOD's step, cross-area rows PK15 and PK22) the bridge creates the world's `Zenith_RuntimePackageWorldRecord`; registers its member `Zenith_PackageStateResource` as the world resource `Zenith.Package.State` (owning phase `PHASE_INPUT`; initial value of "World snapshot"); and, on the world's `Zenith_AsyncResultTable`, calls `DeclareSource(ASYNC_SOURCE_PACKAGE, uPACKAGE_EVENT_QUEUE_CAPACITY, 96)` and `SetHandler(ASYNC_SOURCE_PACKAGE, &record)` ([Z2-N-SCRIPT-343]; the record itself is the handler, since `Zenith_RuntimePackageWorldRecord` derives from `Zenith_AsyncPublicationHandler`). The record binds the table pointer once (MOD's sink-binding rule), is marked "needs snapshot", and is retired with the table.
2. **Queueing.** The installer and the service queue `Zenith_PackageEvent` values during prologue step 1b into a queue of `uPACKAGE_EVENT_QUEUE_CAPACITY` (512) with `QUEUE_FULL_POLICY_DROP_OLDEST`, counters `package.events.full`, `.dropped`, `.high_water` ([Z2-N-CONTRACT-345]). Coalescing applies only to state events without an error: a newer `PACK_STATE_CHANGED` or `DLC_STATE_CHANGED` whose error is 0 replaces a queued event of the same kind and unit whose error is 0 (the latest state wins). An event that carries an error (an apply-time refusal, a failure) is never replaced and never replaces another, so a refusal followed in the same frame by a state change of the same pack publishes both, in that order. The bridge's queue is what worlds see; the installer's `axEventsOut` of `RunPrologue` is uncoalesced (one event per transition, the transitions made by request members in the sync point emitted first in the next `RunPrologue`), which is what the installer's tests read.
3. **Allocation and completion (live runs).** At the end of prologue step 1b, for each attached world in tag order: first, when the world is marked "needs snapshot", its snapshot ("World snapshot"); then each queued event in queue order, except, for a world that received its snapshot in this prologue, the state events without an error (the snapshot already carries the state they describe). Each publication is `AllocateCompletionID(ASYNC_SOURCE_PACKAGE)` on the main thread in the frame prologue (which [Z2-N-SCRIPT-341] admits for a RECORDED kind), then `Complete(id, PUBLICATION_KIND_GENERIC_RECORDED, payload)`. A world whose outstanding package publications (allocated and not yet published, counted by its record) reached `uPACKAGE_EVENT_QUEUE_CAPACITY` − 1, for example a world paused for many frames, keeps further events in its record's deferral queue (one latest state event per unit plus up to 64 other events, the oldest of those dropped and counted in `package.events.dropped`), allocated in order at the following prologues as capacity frees; deferrals are counted in `package.events.deferred`. The deferral queue keeps arrival order: a state event of a unit removes that unit's older deferred state event and takes its own arrival position. `package.events.deferred` + 1 for every event that enters a deferral queue (a replacing state event included), and `package.events.dropped` + 1 for every other event dropped as the oldest when a 65th arrives. Events queued before the first prologue are not published at all ("Package service startup").
4. **Publication.** At the world's next tick boundary that runs, the table publishes every parked result in completion-ID order with every other source's results (Z2-D-053), so a package event and a platform event of one frame publish in the order their IDs were allocated; the replay recorder records each publication ([Z2-N-SCRIPT-341]). PACK's handler (`Zenith_RuntimePackageWorldRecord::OnPublished`, tick-boundary step 1) decodes the payload (a malformed one, possible only from a corrupt log, is dropped and counted in `package.events.decode_failed`), updates the record's `Zenith_PackageStateResource` (the unit's state, error and bytes, a DLC unit's bytes staying 0 because its payload's bytes-done field carries a content hash; the update kind and versions for UPDATED), and appends the `Package.*` event to the world's event buffer as PLAT's handler does, delivered in phase 9 of that tick. The resource is the record's own storage, written only by this handler before any phase of the tick runs, so every system of the tick reads constant values (the pattern of GAME's `Zenith.Runtime.WorldState`); the handler changes no entity and no structure.
5. **Replay.** Under `--replay` the bridge allocates nothing and completes nothing for live events (Q89 (a), ORCHESTRATOR RULED 2026-10-09), takes no snapshot of its own, and the buffered `Package.*` commands of the replayed scripts are not applied to the installer; the replay source supplies the recorded `ASYNC_SOURCE_PACKAGE` publications (every world's recorded snapshot included) at their recorded ticks, the world's counter follows each `REPLAY_RECORD_TICK` record's next completion ID ([Z2-N-ARCH-794]), and the same handler runs, so `Package.GetPackState`, `Package.GetUpdateKind` and the events reproduce exactly without Play. The content a replay mounts is fixed by "Replay pack set".

### World snapshot

A world created at any time (the startup world at step 16, a transition target, an isolated or a restored world, which GAME creates at run time) learns the package state from one snapshot of recorded publications, allocated for it at the first prologue step 1b that follows its creation and never again. A world created at startup step 16 receives it at the first frame's step 1b; a world GAME creates in prologue step 7 of frame k (a transition target, an isolated or a restored world, `Zenith2/Docs/Design/GAME/WorldHost.md → "Prologue step 7 procedure"`) misses frame k's step 1b, which has already run, and receives it at frame k + 1's:

| Order | Publications | Payload |
|---|---|---|
| 1 | one `PACKAGE_EVENT_UPDATED`, when the run's update kind is neither NONE nor FIRST_RUN | the kind and the from and to versions of the service's update info |
| 2 | one `PACKAGE_EVENT_UPDATE_FAILED` per rejection of this start's pending pass, in report order, at most `uPACKAGE_MAX_REPORTED_REJECTIONS` (16) | the target kind, the name and the error |
| 3 | one `PACKAGE_EVENT_PACK_STATE_CHANGED` per deliverable unit, in unit order | the unit's current `Zenith_AssetPackStatus` (state, error, bytes) at this prologue, after the installer's step |
| 4 | one `PACKAGE_EVENT_DLC_STATE_CHANGED` per Windows DLC unit, in unit order | the DLC's current state and error |

The units of rows 3 and 4 are exactly the units without the PRESENT flag, at most 256 by check M12 (Formats.md "Package manifest checks"), so a snapshot holds at most 1 + 16 + 256 = 273 publications and always fits a new world's 511 free slots in one prologue. When a deferred state event of a unit is waiting in the world's deferral queue at the snapshot, it is dropped: the snapshot item reads a state at least as recent.

Before its snapshot publishes, a world reads the resource's initial value, set by `Zenith_RuntimePackage_AttachWorld`: update kind `None` with zero versions, and state UNKNOWN with zero bytes for every unit the snapshot lists. A unit with the PRESENT flag is never in the resource: `Package.GetPackState` and `Package.IsMounted` answer `"Mounted"` and true for it from the immutable manifest. So a world created at tick 300, after a pack was mounted, reads the pack as `"Unknown"` until the first tick boundary after the first prologue step 1b that follows its creation, and `"Mounted"` from then on (a world created in prologue step 7 of frame k that ticks in frame k reads `"Unknown"` for those ticks too, since its snapshot is allocated only at frame k + 1), and its `Package.GetUpdateKind` equals the startup world's (`Test_RuntimePackage_LateWorldReceivesSnapshot`). The UPDATED and UPDATE_FAILED events reach each world exactly once, through its snapshot; nothing else queues them. The resource's properties are all transient, so SAVE never captures it (`Zenith2/Docs/Design/SAVE/Formats.md` → "Resources section" writes only resources with a serialized property) and a world restored from a save learns the state from its own snapshot.

### Replay pack set

A replay must mount the content the recording had, or every result a replay recomputes from content (`PUBLICATION_KIND_GENERIC_RECOMPUTED` asset and scene loads) can differ while the replayed `Package.*` state says otherwise. The identity check of [Z2-N-ARCH-784] already fixes the build, the composition and the effective content hash (so the base `package.zpkm` and the mounted patch); the delivered packs and the Windows DLC are fixed by PACK:

1. Under `--replay`, `Zenith_RuntimePackage_Create` reads every recorded `ASYNC_SOURCE_PACKAGE` publication of the log through `Zenith_ReplayLogReader::ForEachPublication` (assumption pending SAVE, cross-area row PK24), decodes it, and forms R, the set of units for which some publication of any world carries state MOUNTED (a `PACK_STATE_CHANGED` or a `DLC_STATE_CHANGED`).
2. The installer is created in replay mode: no automatic F, no resume of persisted intents, no R from `WANT_REMOVED`, no game command (commands are not applied under `--replay`); every unit of R that Play reports COMPLETED (or the simulated delivery holds) is verified and mounted, and no unit outside R is ever mounted. In replay mode the installer applies neither HOLD nor the guard on A1 to a unit of R, and never raises E or G for any unit: the recording's MOUNTED publication is the evidence of the grant at recording time, for an entitlement-gated Android pack exactly as for a Windows DLC, so a gated unit of R goes from P6 to VFY whatever the forwarder answers (in a composition with the `Online` module it has no provider until step 15, which runs after this wait, and answers UNKNOWN), and a gated unit outside R is never mounted. Windows DLC mounting mounts exactly the DLC units of R, without asking the entitlement query, and only the revision the recording mounted: the last recorded MOUNTED `DLC_STATE_CHANGED` publication of the DLC carries the mounted revision's result content hash (`dlc.zman` offset 56) in its bytes-done field (Formats.md "Package event payload"), and a DLC whose installed `dlc.zman` names another hash refuses the replay with exit code 5 (`package: replay needs dlc <name> with content hash <h>, installed <i>`), because a DLC targets any build of the line and its bytes are not fixed by the effective content hash. An Android pack needs no such check: its bytes are verified against the build's own `package.zpkm`.
3. `Zenith_RuntimePackage_Create` pumps the installer (`RunPrologue` with no event allocation, `Zenith_SleepUs(1000)` between pumps) until every unit of R is MOUNTED, for at most `package.replayMountTimeoutMs` (60,000 by default, 1,000 … 600,000). A unit of R that is not delivered, fails verification, or is not mounted by the bound refuses the replay with startup exit code 5, the code of [Z2-N-ARCH-784] (`package: replay needs pack <name>, which is <state>`).
4. R stays mounted for the whole replay. Mounting a pack before the tick at which the recording mounted it is equivalent for every recorded result: a load that failed in the recording (a pack not yet mounted) is a recorded failure the replay takes from the log, and a load that succeeded is recomputed against the same verified bytes.

`Test_RuntimePackage_ReplayPublishesRecorded` asserts steps 1–3, including the refusal when a pack of R is absent, the mount of an entitlement-gated pack of R while the forwarder has no provider, and the refusal of a Windows DLC whose installed revision differs from the recorded one.

## Consistency invariants

These are the assertions "the game left in a consistent state" means; every interruption test asserts all that apply.

| # | Invariant |
|---|---|
| I1 | A pack's archives are mounted only after every archive of its unit matched the base `package.zpkm`'s byte count and SHA-256 in this process, or in an earlier process whose delivery-state record still matches the pack version, the manifest content hash, the location and every archive's stamp ("Verified cache" item 3) |
| I2 | At every point of every interruption, the patch version the patch mount would mount (the highest `Patches/<v>` whose manifest verifies) is the version before the install or the version it installs, never another and never none when one of them verified; a patch version directory exists only as the atomic rename of a bundle the applier sealed and verified |
| I3 | After an install completes, at most one patch version of the running line remains, and no version of another line or build, unless more than 1,024 obsolete directories were found, in which case the excess remains, is never mountable over the committed version, and is swept by later starts ("Obsolete directories") |
| I4 | After recovery and the pending pass, the journal is absent and no `.trash` tree remains beyond the deferred excess of I3; every `.incoming` entry is installed, refused (gone), or left in place: a dot name, an unsupported name (logged), a bundle missing a listed file (`UPDATE_INCOMPLETE`), a bundle that could not be sealed (`INSTALL_IO`), or, with the base package's identity unknown, every patch bundle |
| I5 | A pack's files are deleted only after every source of its archives was released by the virtual filesystem; no read is torn |
| I6 | Simulation observes pack, update and DLC changes only at tick boundaries, through `ASYNC_SOURCE_PACKAGE` publications in completion-ID order, whose payloads are recorded |
| I7 | An asset of an unmounted pack resolves `ASSET_ERROR_NOT_INSTALLED` and follows ASSET's fallback policy; no entity is created or lost because a pack is absent; nothing asserts |
| I8 | Recovery and the installer's resume are idempotent: any number of further interruptions and restarts converge on the same state |
| I9 | A DLC (a Windows DLC directory or an entitlement-gated Android pack) is mounted only while its entitlement is GRANTED and its bytes verified, except under `--replay`, where exactly the recording's mounted units are mounted, gated ones included, with no entitlement asked, and a Windows DLC only at the recorded revision ("Replay pack set") |
| I10 | After Create returns, the install record's identity equals the running identity, or the failed record write was logged and the next start classifies from the record on disk |
| I11 | No install, recovery or sweep follows a link: a link inside the install tree is refused (U0, `Open`) or removed as itself, so nothing outside the install root is deleted |
| I12 | No process installs, recovers or sweeps while another instance of the same install is running, and no instance reaches startup step 5 while another process installs: an instance that could not take `INSTALL_LOCK_APPLY` waits, without bound, for `INSTALL_LOCK_RUNNING` shared ("Pending installs at startup" step 10); only an unsupported volume and a read-only root without a `running` file run without the guard, each logged |
| I13 | Every directory record of `verified.zinv` carries the SHA-256 of that directory's manifest as B6 or recovery verified it, and a file record equal to the file's current stamp, in a directory whose current manifest has that digest, implies the file's bytes hash to the manifest's listed SHA-256 and size; so no changed or never-verified file is ever counted as verified, at any interruption point |

## Complexity

| Algorithm | Cost |
|---|---|
| `Zenith_RunPendingInstalls` with nothing pending | 12 file-system calls (one volume query, three lock acquisitions and two releases, three `List`, one 48-byte read as `Open`, `Read`, `Close`), none of which writes |
| verification of a bundle or a pack | O(bytes) hashing, at the rate the verification throughput floor row Z2-B-PACK-010 measures; O(F log F) for the sorted file checks |
| install, reject, clean, sweep | O(M) renames and O(files in the trashed trees) deletes; the sweep's O2 reads one manifest per patch directory of the line, at most once per build |
| installer prologue | O(cells polled + packs) plus the verification jobs |
| the verified cache's skip | one stamp query per file (no read) and an O(log D + log F) lookup per patch or DLC file; the full hash only for files whose stamp differs |
| the guard wait of "Pending installs at startup" step 10 | one `AcquireLock` call per 50 ms for as long as another process installs; no write |
| a world's snapshot | at most 273 completion-ID allocations and completions, once per world |
| the shutdown drain | at most `package.shutdownDrainMs` iterations of `PumpShutdown` (O(packs + completions)) and a 1 ms sleep |
| the replay pack set | O(P) over the log's package publications at `Create`, then the verification of R's packs |
| partition | O(E + D) with the worklist; E entries, D dependencies |
| chunking and archive writing | O(F log F) sort plus O(bytes) copy and hash |
| patch diff | O(entries of both packages) with a hash map, plus O(changed bytes) |
