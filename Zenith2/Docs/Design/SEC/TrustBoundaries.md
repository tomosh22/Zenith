# SEC design: trust boundaries

This file is part of the SEC design family; the entry document is `Zenith2/Docs/Design/SEC.md`. It specifies the trust-boundary model: every place where bytes from outside the process image, or from a less-trusted part of it, cross into Zenith2 code, what is trusted at each, what is validated, where, and what happens on failure. The SPEC baseline is `Zenith2/Docs/Architecture.md` → "Security architecture" (its table "Trust boundaries", [Z2-N-ARCH-951], [Z2-N-ARCH-952], [Z2-N-ARCH-955], [Z2-N-ARCH-956], [Z2-N-ARCH-966]); this file elaborates that table into nine boundaries and records the confirmations the Stage 1 security-architecture ruling makes.

## Scope of this file

| Topic | Heading |
|---|---|
| the record every boundary carries, classes and tiers | "Boundary model" |
| the nine boundaries | "The nine boundaries" |
| what the model assumes about the machine | "Trust preconditions" |
| the verified-cache confirmation items | "Verified-cache confirmation" |
| ONLINE's assumptions answered | "Online-services confirmation" |
| the rules every development listener follows | "Remote-tool session rules" |
| what a save does and does not promise | "Saves and integrity" |
| what the model knowingly does not defend | "Accepted residual risks", "Non-goals" |

## Boundary model

### Boundary record fields

Every boundary below is written as one record with these fields, and every parser of the boundary appears in `Zenith2/Docs/Design/SEC/ParserInventory.md` with the boundary's class (its `boundary` field in `Zenith2/Tools/Security/parser_inventory.json`):

| Field | Meaning |
|---|---|
| Source | where the bytes come from, as paths, mounts, processes or services |
| Trusted | what the engine relies on without checking it, and why that reliance is sound |
| Validated | every check made before the bytes are used |
| Where | the function or step that performs each check, with its owning design heading |
| Never | what the boundary refuses outright |
| Failure | the defined behaviour when a check fails |
| Evidence | the tests and acceptance items that prove the controls |

<!-- elaboration -->
The Architecture table names seven boundaries. SEC adds two that the table leaves implicit: **tool inputs** (DCC sources, authoring documents, shared caches, clipboard and child-process output, which reach tools and never a Player) and **process IPC** (the Windows crash helper and the out-of-process importer workers). Both are added to the Architecture table by core-doc row X413 (`C:/dev/Zenith2-drafts/reviews/CROSS_DOC_ITEMS.md`).

### Boundary classes and tiers

The class is the value of the inventory's `boundary` field; the tier sets the fuzz campaign duration (`Zenith2/Docs/Design/SEC/Fuzzing.md` → "Stage acceptance campaigns"):

| # | Class (`boundary` value) | Tier | Why this tier |
|---|---|---|---|
| B1 | `shipped-content` | 2 | trusted origin, untrusted bytes: corruption and toolchain defects, not an attacker below the install-root precondition |
| B2 | `downloaded-content` | 1 | reaches the device from outside the package; every byte is attacker-controlled until its signature or hash is checked |
| B3 | `user-files` | 1 | written and rewritten outside the engine's control (other tools, other devices, copied or shared files) |
| B4 | `scripts` | 2 | bytecode integrity is the mount's (B1 or B2); behaviour is contained by the sandbox |
| B5 | `remote-tools` | 3 | development builds only, loopback by default, session token required |
| B6 | `network` | 1 | any peer can send any datagram |
| B7 | `online` | 1 | responses of services and SDKs outside the programme |
| B8 | `tool-inputs` | 3 | reaches host tools only, never a Player |
| B9 | `process-ipc` | 1 | a second process whose inputs must not let a third party steer it |

A parser reached through more than one class takes the lowest tier number of those classes (tier 1 is the longest campaign). A parser whose only class is `remote-tools` or `tool-inputs` is tier 3.

## The nine boundaries

### B1 shipped content

| Field | Content |
|---|---|
| Source | the archives the mount manifest lists under `<install>/Content/` (Windows); the APK's assets and install-time Play Asset Delivery packs read through the asset manager (Android); in non-Shipping builds the loose mount (`--assets-root`, else `${ZENITH_OUT}/Cooked/<Game>/<platform>/`) |
| Trusted | the origin: these bytes were produced by the programme's cook and packager and are protected by the install-root and device preconditions ("Trust preconditions"); Android additionally verifies the APK's v2/v3 signature at install |
| Validated | the mount manifest's schema and the JSON limits with a 1 MiB cap (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount manifest schema"); the archive checks A1 to A7, including A5's rule that every entry name parses as a virtual path (`Zenith2/Docs/Design/ASSET/Formats.md` → "Archive checks"); every cooked file's versioned header, cook version and section table (`Zenith2/Docs/Design/ASSET/Formats.md` → "Checks"); each kind's reader checks and the parser limits of `Zenith2/Docs/Architecture.md` → "Security architecture"; the script bytecode container's load-time verification (`Zenith2/Docs/Scripting.md` → "Shipping bytecode compatibility") |
| Where | PLAT's mount-manifest reader at startup step 5; ASSET's archive reader and header parser; every kind loader listed in `Zenith2/Docs/Design/SEC/ParserInventory.md` |
| Never | a per-file signature on the base package (none exists; the install-root precondition carries the integrity); content outside the four mounts (modding is excluded, Z2-D-075); the loose mount in Shipping ([Z2-N-ARCH-922]) |
| Failure | a `Zenith_Result` error: an asset falls back to its kind's fallback and is logged (`Zenith2/Docs/Design/ASSET.md` → "Failure handling"); an invalid or absent mount manifest stops a Shipping build at startup with exit code 3; a cooked version mismatch is `VERSION_MISMATCH` and the content is re-cooked (Z2-D-048) |
| Evidence | every tier 2 fuzz target of the inventory; the at-limit and limit + 1 tests ([Z2-N-ARCH-953]); `Test_SecPath_ArchiveEntryTraversalRefused` and `Test_SecPath_MountManifestArchiveNameRefused` (`Zenith2/Docs/Design/SEC/Tests.md` → "Path-matrix composition tests") |

### B2 patches and downloaded content

| Field | Content |
|---|---|
| Source | committed patches `<install>/<patchRoot>/<MAJOR.MINOR.PATCH>/` and their `update.zman`; Windows DLC `<install>/DLC/<name>/` and its `dlc.zman`; incoming bundles in `<install>/.incoming/` before the applier commits them; Play fast-follow and on-demand asset packs in app storage at Play's `AssetPackLocation.assetsPath()` |
| Trusted | nothing until verified. Patches and DLC: the Ed25519 signature over every byte before the signature, against the public key compiled into the Player, binds the manifest's file list, sizes and SHA-256 values; once verified, a file whose identity stamp is unchanged is trusted without a re-hash ([Z2-N-ARCH-955], [Z2-N-ARCH-966]). Packs: Play's signing plus the SHA-256 recorded in the base `package.zpkm` ([Z2-N-ARCH-956]) |
| Validated | the update manifest checks U0 to U11 (`Zenith2/Docs/Design/PACK/Formats.md` → "Update manifest checks") in the order of `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Rule order"; for packs, the verification pipeline (`Zenith2/Docs/Design/PACK/Algorithms.md` → "Verification pipeline") |
| Where | PLAT's `Zenith_VerifyUpdateManifest` and its three halves in `Zenith2/Engine/ZenithPlatform/Internal/Security/` (the patch mount at startup step 5); PACK's applier at the closing action of startup step 4 (steps B4 and B6); PACK's Windows DLC mounting; PACK's asset pack installer |
| Never | an archive mounted before its manifest verifies; a partly mounted patch; an unlisted file hashed or mounted (U0); a downloader of the programme's own (Z2-D-027) |
| Failure | the whole update is rejected, the previous content stays mounted, the reason is logged ([Z2-N-ARCH-961]); a pack whose hash differs stays unmounted and is reported to the game ([Z2-N-ARCH-956]) |
| Evidence | the tamper suite (`Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Tamper suite"); `Test_PatchMount_TamperCasesRejected` (PLAT); PACK's update applier tests; the Stage 6 security evidence ([Z2-N-ARCH-960]) |

### B3 user files

| Field | Content |
|---|---|
| Source | the user mount: `user:/saves/` (save slots, the resume state, ONLINE's `user:/saves/online/` state), `user:/config/` (the user configuration layer, input bindings), `user:/cache/` (PACK's install record, ONLINE's derivable state), `user:/logs/`; DEVT's crash reports and development outputs (see row SEC1 of `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` for their namespace); PACK's install-root state (`.zenith2-install/install-journal.zinj`, `install-state.zins`, `verified.zinv`) and its delivery state file; RHI's persistent pipeline caches; replay logs given with `--replay` (non-Shipping); imported cloud objects |
| Trusted | nothing: every byte may have been written by another program, another device, another release or a person. The player may edit their own saves, which the engine does not prevent ("Saves and integrity") |
| Validated | parser limits; length prefixes and checksums (CRC-32C) before decoding; version checks and forward migration before use (Z2-D-048); reflected-type and schema checks; namespace rules R1 to R10 for every path inside them |
| Where | every tier 1 entry of `Zenith2/Docs/Design/SEC/ParserInventory.md` |
| Never | executable content: no script bytecode in a save, replay or cloud object ([Z2-N-SCRIPT-486]); a path from a user file reaching an OS file API unvalidated ([Z2-N-ARCH-954]) |
| Failure | a `Zenith_Result` error, never an assert ([Z2-N-CONTRACT-472]); a corrupt save is reported to the game and set aside by SAVE's corruption recovery; a corrupt cache is deleted and rebuilt ([Z2-N-REND-205]); a refused install state resets PACK's downgrade floor to the committed directories |
| Evidence | the tier 1 fuzz targets; SAVE's format-limit tests; the composed tests of `Zenith2/Docs/Design/SEC/Tests.md` → "Path-matrix composition tests" |

### B4 scripts

| Field | Content |
|---|---|
| Source | Luau bytecode in cooked `.zlbc` containers from the package and patch mounts; in non-Shipping builds, bytecode in the loose mount compiled by the tools through `ZenithScriptCompiler` ([Z2-N-ARCH-962]) |
| Trusted | that bytecode came from an authenticated or package mount, because Luau's loader does not verify bytecode ([Z2-N-SCRIPT-486], [Z2-N-ARCH-957]); nothing about a script's behaviour |
| Validated | the bytecode container header and its load-time verification fields, the module manifest and the binding-set hash (`Zenith2/Docs/Scripting.md` → "Shipping bytecode compatibility"); the sandbox environment, safepoint budgets, memory caps and the wall-clock watchdog (`Zenith2/Docs/Scripting.md` → "Sandboxing and security", "Budgets safepoints and the watchdog") |
| Where | ZenithScript's module loader and VM creation |
| Never | a script reaching the filesystem, processes, environment variables, sockets, dynamic libraries, raw addresses, another world, another VM's heap or the debugger ([Z2-N-SCRIPT-482]); a script loading code ([Z2-N-SCRIPT-483]); a Player compiling source ([Z2-N-ARCH-962]) |
| Failure | a controlled Luau error and the transaction rollback of `Zenith2/Docs/Scripting.md` → "Commit and failure semantics"; a refused container leaves the module unloaded with `SCRIPT_ERROR_LOAD` |
| Evidence | SCRIPT's sandbox tests (`Zenith2/Docs/Design/SCRIPT/Tests.md` → "Module binding and sandbox tests"); PI-SCRIPT-001 to PI-SCRIPT-003 fuzz targets; `Test_SecScript_BytecodeOnlyFromPackageAndPatch` |

### B5 remote tools

| Field | Content |
|---|---|
| Source | the Luau DAP server (SCRIPT, `Zenith2/Docs/Design/SCRIPT/Debugger.md` → "Transport and framing"), the remote diagnostics server (DEVT, `Zenith2/Docs/Design/DEVT/Console.md` → "Remote protocol"), the editor live link (EDIT, tool only, `Zenith2/Docs/Design/EDIT/Automation.md` → "Endpoint security"), the Tracy client (DEVT), on Android the same servers reached through `adb forward` |
| Trusted | the local developer who can read the session token from the local log (or EDIT's endpoint file), on the loopback interface |
| Validated | the session rules of "Remote-tool session rules"; each protocol's framing and size caps; each request's own validation (for example `m_bRemoteAllowed` and the `FILE_GET` namespace restriction) |
| Where | PLAT's `Zenith_DevListener` (`Zenith2/Docs/Design/PLAT/Platform.md` → "Loopback socket") for the DAP and diagnostics servers; EDIT's endpoint for the live link |
| Never | any of them in a Shipping build ([Z2-N-ARCH-958]); a non-loopback bind without `devtools.bindAddress` on the command line ([Z2-N-ARCH-959]) |
| Failure | the connection is closed, counted and logged; repeated handshake failures close the listener for 60 s ("Remote-tool session rules", rule T4) |
| Evidence | `Zenith2/Docs/Design/SEC/ShippingExposure.md` (every layer); PLAT's listener tests; the fuzz targets PI-SCRIPT-005 and PI-DEVT-004 |

### B6 network

| Field | Content |
|---|---|
| Source | the Stage 3 two-process loopback transport of the multiplayer foundations; the Stage 7 sessions (`Zenith2/Docs/Architecture.md` → "Multiplayer foundations") |
| Trusted | nothing |
| Validated | NET's packet validation order (size 24 to 1,200 bytes, magic, CRC-32C, version, kind and flags, body length, session, sequence window, then the kind's decoder with overflow-checked counts) under NET's wire limits, which replace the "network messages (Stage 3)" row of the parser limits through cross-doc item X394 (`Zenith2/Docs/Design/NET/Loopback.md` → "Packet validation order", "Wire limits"); publication payloads and NET's 24-byte save section are validated by their own decoders (`Zenith2/Docs/Design/NET/Foundations.md` → "Identity across save and restore") |
| Where | ZenithNet (`Zenith2/Docs/Design/NET.md`), over PLAT's `Zenith_DatagramSocket` (row NT1); SEC confirms NET's security assumptions S1 to S6 (`Zenith2/Docs/Design/NET.md` → "Security assumptions") as this boundary's record, and rows SEC12 and SEC17 of the cross-area notes carry SEC's requests to NET |
| Never | a command decoded without its length checked against the datagram; an assert on received bytes; any network listener in a Shipping build, and any NET socket there before Stage 7 (NET's Shipping guard forces the role offline, `Zenith_NetTransport::Create` is a forbidden symbol, and the E4 import scan refuses `bind`: `Zenith2/Docs/Design/SEC/ShippingExposure.md` → "Listener-import scan") |
| Failure | the datagram is dropped and counted; a peer sending 8 malformed packets within one second is disconnected |
| Evidence | NET's five fuzz targets, PI-NET-001 to PI-NET-005 of `Zenith2/Docs/Design/SEC/ParserInventory.md` → "Online and network parsers"; `Test_NetHeadless_ShippingForcesOffline` |

### B7 online services

| Field | Content |
|---|---|
| Source | completion payloads of the Steam and Google Play backends (Stage 6); the offline implementation's files (`user:/saves/online/`) |
| Trusted | nothing in the payloads; entitlement answers are accepted without server verification in single-player scope ("Online-services confirmation", S6) |
| Validated | ONLINE's completion rules and format checks (`Zenith2/Docs/Design/ONLINE.md` → "Security assumptions") |
| Where | ZenithOnline and its backends |
| Never | a credential, session ticket or purchase token held by native code (ONLINE's S3) |
| Failure | the request completes with an error; an index that fails a check deletes nothing |
| Evidence | ONLINE's twelve fuzz targets; "Online-services confirmation" |

### B8 tool inputs

| Field | Content |
|---|---|
| Source | DCC sources (glTF, images, audio) imported by out-of-process importers; authoring documents (scenes, prefabs, schemas, UI, animation, 2D, CHAR, WORLD, VFX and cinematic documents, `.meta` sidecars, `game.json`); the shared derived-data cache; the editor clipboard; source-control child-process output; automation scripts and live-link lines; the host files and directories named on the command line in non-Shipping builds (`--record`, `--replay`, `--input-script`, `--config-dump`, `--evidence-dir`, `--script-migration-fixtures`, `--assets-root`) |
| Trusted | that the person running the tool chose the input; not the bytes (assets come from third parties, documents arrive through merges, a shared cache is written by other machines) |
| Validated | CONTENT's importer limits and crash isolation (`Zenith2/Docs/Design/CONTENT/Importers.md` → "Parser limits"); document validation under the JSON limits; DDC payload verification (`Zenith2/Docs/Design/ASSET/Cook.md` → "Validation"); command-line grants (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Command-line host files") |
| Where | ZenithContentCore, ZenithCook, ZenithAuthoring, ZenithEditor, ZenithEditorCore, ZenithDocument and ZenithDocumentHost |
| Never | a tool input reaching a Player; a command-line host path in Shipping (the parser rejects the options) |
| Failure | an import or document error with a diagnostic; an importer crash is a failed import, never a tool crash |
| Evidence | the tier 3 entries of the inventory; tool fuzz targets built through `zenith_tool_fuzz_target` (`Zenith2/Docs/Design/SEC/Fuzzing.md` → "Tool-side fuzz targets") |

### B9 process IPC

| Field | Content |
|---|---|
| Source | the Windows crash helper started as `--zenith-crash-helper` with inherited handles (`Zenith2/Docs/Design/DEVT/Crash.md` → "Crash helper security contract"); the out-of-process importer workers' results (CONTENT) |
| Trusted | only the handles the parent passed through `PROC_THREAD_ATTRIBUTE_HANDLE_LIST`; never a PID or path argument ([Z2-N-ARCH-965]) |
| Validated | the crash helper's section magic, version, 128-bit nonce and parent PID against the inherited process handle; every section string bounded and escaped; importer results under CONTENT's limits |
| Where | DEVT's crash helper; CONTENT's importer host |
| Never | `PROCESS_DUP_HANDLE` on the parent handle; an inheritable handle outside the one `CreateProcessW` call |
| Failure | the crash helper exits with code 3 on any failed check |
| Evidence | `Zenith_Fuzz_CrashReport`; DEVT's crash helper tests |

## Trust preconditions

### Install-root precondition

The base package (`<install>/Content/`), the patch and DLC directories and `<install>/.zenith2-install/` are writable only by principals that can also replace the executable `<install>/<Game>.exe`. Under it, every check that the engine skips (the base package carries no per-file signature; a verified file whose stamp is unchanged is not re-hashed) is one an attacker able to bypass it could have made pointless by replacing the executable. The precondition holds for every supported Windows layout when the installer, zip or launcher creates `<install>` with inherited permissions and no per-directory override; the Stage 6 distribution-layout audit ("Distribution-layout audit") checks it per layout.

### Device precondition

On Android, the APK, the app-private storage (`Patches/`, `.zenith2-install/`, the user mount) and Play's asset pack locations are writable only by the app's UID, the package manager and root. A rooted device is outside the model.

### Same-user precondition

User files are writable by the player's own account. The engine does not defend a player's game against the player (no anti-cheat, no save authentication); it does defend the process against a hostile file the player received from elsewhere (a downloaded save, a shared replay, a cloud object written by a modified client): such a file can change game state only through the game's own rules, never memory, control flow or files outside the user mount.

## Verified-cache confirmation

The Stage 1 security-architecture ruling confirms the three items of `Zenith2/Docs/Design/PACK/Algorithms.md` → "Verified cache" item 6, as [Z2-N-ARCH-966] requires, with the content below. PACK's and PLAT's designs are not re-specified here; this section is SEC's answer to row PK27 of the cross-area notes.

### Stamp set confirmed

(a) The stamp set is confirmed as `Zenith_FileIdentityStamp` (PLAT's type, row PK27 (a)): size, last-write time and change time in nanoseconds since the Unix epoch, the 64-bit volume serial (Windows) or `st_dev` (Linux, Android), and the 128-bit file ID (Windows `FileIdInfo`) or `st_ino` in the low 8 bytes (Linux, Android). The confirmation carries four conditions, each already in PACK's and PLAT's designs and each a requirement of SEC's catalogue:

| # | Condition | Why |
|---|---|---|
| C1 | a stamp is recorded only when the stamps taken before the first read and after the last read of the hash are equal in all five fields | the bracket, not one handle, catches a replacement (identity changes) and an in-place write or rename-back (change time advances) |
| C2 | a first stamp is taken only when `max(last-write, change)` is at least 100 ms older than the current time on the stamps' scale (the racy-timestamp rule) | a write in the same timestamp tick as the previous change would otherwise leave two equal stamps around different bytes |
| C3 | a stamp is answered only on a supported volume (NTFS or ReFS on Windows; ext4, XFS, Btrfs or tmpfs on Linux; ext4 or F2FS on Android); every other volume answers "hash it" | FUSE, sdcardfs, adoptable storage and FAT-family volumes synthesise or lack change time and identity |
| C4 | a record is keyed by the SHA-256 of the exact manifest bytes just verified, and the manifest's signature and targets are checked at every start before the lookup is consulted | a substituted, re-signed or older manifest never matches a record |

Two known effects only cost a re-hash and are accepted: `st_dev` of `/data` can change across a boot or an OTA, and a metadata-only change (an SELinux relabel, `chmod`, an xattr write, a Windows archive-bit write) advances the change time without changing a byte.

### Records unauthenticated

(b) `verified.zinv` and PACK's delivery state file are unauthenticated user state, protected only by the write permission of the install root (Windows) or the app-private storage (Android). A party able to write them can rewrite an archive and write a record holding its new stamp beside the genuine SHA-256 of the untouched signed manifest; the mount then skips the hash. The signature is checked at every start but does not bind a file's bytes once its record is trusted. This is sound exactly while the install-root and device preconditions hold, because that party can already replace the executable.

### Keyed-record escalation

(c) If any supported distribution layout makes `.zenith2-install/` more writable than the executable, the records are keyed before that layout ships:

| Element | Design |
|---|---|
| secret | 32 bytes from the OS random source (`BCryptGenRandom` with `BCRYPT_USE_SYSTEM_PREFERRED_RNG`, `getrandom`), created at the first install that needs it |
| where it lives | outside the install root: Windows, a DPAPI-protected blob (`CryptProtectData`, user scope) at `user:/cache/package/record-key.bin`; Android, the app-private `files/` directory (already outside any shared location) |
| what is keyed | each directory record of `verified.zinv` and each pack record of the delivery state file gains a 32-byte HMAC-SHA-256 over the record's bytes and its directory path, under the secret |
| verification | a record whose MAC does not verify is treated as absent (re-hash), never as an error |
| format change | a version bump of the two formats (PACK), refused-and-rebuilt by the old reader's version rule |

The escalation is not built while every supported layout satisfies the precondition; its trigger is a failed row of the distribution-layout audit, and the release that would ship such a layout is blocked until the keyed records land.

### Distribution-layout audit

At Stage 6 qualification, and again for every new distribution layout, a person runs `Test-Zenith2InstallLayout.ps1` (`Zenith2/Docs/Design/SEC.md` → "Release-time and acceptance tools") on an installed copy and records the result in the qualification record:

| Check | Windows | Android |
|---|---|---|
| L1 | the effective write principals of `<install>\.zenith2-install`, `<install>\Patches` and `<install>\DLC` (from `Get-Acl`, inherited and explicit ACEs with `Write`, `Modify`, `FullControl`, `CreateFiles` or `AppendData`) are a subset of those of `<install>\<Game>.exe` | `run-as <package> stat -c %a files/.zenith2-install` reports no group or other write bit |
| L2 | the volume of `<install>` is NTFS or ReFS (else every stamp is "hash it", which is safe and is reported) | the internal storage volume is ext4 or F2FS |
| L3 | no reparse point inside `<install>` points outside it | not applicable |

A failed L1 triggers the keyed-record escalation for that layout.

## Online-services confirmation

SEC's answer to row ON15 of the cross-area notes; each item is confirmed or tightened, and the tightening is a requirement of SEC's catalogue that ONLINE's tickets satisfy:

| # | ONLINE's assumption | SEC |
|---|---|---|
| S1 | backend responses are untrusted bytes | confirmed; every validator is a tier 1 inventory entry with its fuzz target |
| S2 | cloud objects are untrusted user files | confirmed as class `user-files`; SAVE's full verification and migration run at import, and nothing executable crosses |
| S3 | ONLINE holds no secret | confirmed and tightened: the forbidden-string scan (`Zenith2/Docs/Design/SEC/ShippingExposure.md` → "Initial string entries") lists no ONLINE secret because none exists; a configuration key whose final dot-separated component, lower-cased, ends with `token`, `secret`, `password` or `ticket` (`online.sessionToken`) is refused by the SecurityConformance check SC15 |
| S4 | personal data stays out of logs, replays, crash reports and saves | confirmed; tightened by the remote-tool rule T5 (no account value in a remote response) |
| S5 | the offline implementation never contacts a network | confirmed; complemented by the listener-import scan of every Shipping image |
| S6 | client-side entitlement answers are not tamper-proof | accepted residual R3 ("Accepted residual risks") |
| S7 | development seams are absent from Shipping behaviour | tightened: each seam's entry symbol is a row of `shipping_forbidden_symbols.json` (`Zenith2/Docs/Design/SEC/ShippingExposure.md` → "Consolidated entries", the ONLINE rows) |
| S8 | no account data crosses accounts | confirmed; ONLINE's own tests are the evidence |
| S9 | the install secret is a secret of the device | confirmed: 32 bytes from the OS random source, never synced, logged or published |

## Remote-tool session rules

Every development listener (PLAT's `Zenith_DevListener` and EDIT's live-link endpoint) follows these rules; rules T3, T4 and T6 tighten the current designs and are requested by row SEC2 of the cross-area notes:

| # | Rule |
|---|---|
| T1 | the session token is at least 128 bits from the OS cryptographic random source (`BCryptGenRandom`, `getrandom`), fresh per process start, written as lower-case hex (PLAT: 32 hex digits; EDIT: 64) |
| T2 | the first message of a connection must arrive within 5,000 ms and be exactly the protocol's handshake (`ZENITH2-TOKEN <token>\n` for PLAT's listener; `{"hello": 1, "token": "<64 hex>"}` for the live link); anything else closes the connection |
| T3 | the received token is compared in constant time: every byte of the expected token is compared (XOR accumulated into one `uint8`), the length mismatch is folded into the same result, and no early exit depends on the content |
| T4 | five failed handshakes within 10 s stop the listener accepting connections for 60 s, counted in `devtools.listener.lockouts` |
| T5 | the token is written once to the local log at Info and, for EDIT, to its access-controlled endpoint file; it never appears in a crash report, telemetry, frame dump, remote response, replay log or saved file |
| T6 | binding is `127.0.0.1` unless `devtools.bindAddress` is given on the command line (EDIT: never); a non-loopback bind logs `devtools: <service> listening on <address>:<port> beyond loopback` at Warning at every start |
| T7 | each protocol caps its message: DAP per SCRIPT's framing rules, the remote protocol's 1 MiB payload, the live link's 1 MiB line; a message above its cap closes the connection before it is buffered whole |
| T8 | every received byte is untrusted and its decoder is a fuzzed inventory entry |
| T9 | no listener exists in a Shipping build ([Z2-N-ARCH-958]) |

The Tracy client is the documented exception of [Z2-N-ARCH-959] (loopback only, started only by `devtools.tracy.enabled` on the command line, no token because Tracy's protocol has none, absent from Shipping); rules T1 to T5 do not apply to it.

## Saves and integrity

- A save file carries length prefixes, CRC-32C section checksums and versioned migration (`Zenith2/Docs/Design/SAVE/Formats.md` → "Format limits"). These detect corruption; they authenticate nothing, and Zenith2 makes no tamper-resistance claim for saves, replays or cloud objects.
- A save may be edited by its owner; the engine's obligation is that any byte sequence presented as a save yields either a restored world that obeys every invariant the game's rules enforce, or a `Zenith_SaveError`, never undefined behaviour.
- Saves never carry executable content ([Z2-N-SCRIPT-486]); a behaviour checkpoint is decoded against the declared schema ([Z2-N-SCRIPT-485]).

## Accepted residual risks

| # | Residual | Why accepted | Recorded where |
|---|---|---|---|
| R1 | a hard link planted inside a mount root is indistinguishable from a regular file | planting it needs write access to the mount root, which the install-root and device preconditions already grant | PLAT's escape-check source comment; `Zenith2/Docs/Design/SEC/PathTraversal.md` → "Accepted residuals for paths" |
| R2 | a party able to write the install root can forge a verified-cache record | that party can replace the executable ([Z2-N-ARCH-966]) | "Records unauthenticated" |
| R3 | client-side entitlement answers can be bypassed by a modified client | single-player scope, no server of the programme's own (Z2-D-073, Z2-D-074) | "Online-services confirmation" |
| R4 | the base package carries no per-file signature | the install-root and device preconditions | B1 |
| R5 | a tool-only reader without a fuzz target is exercised only by mutated-corpus tests until its `zenith_tool_fuzz_target` lands | tool inputs reach no Player; the gap is listed in `Zenith2/Docs/Design/SEC/ParserInventory.md` → "Conformance findings" with its owner | the inventory |
| R6 | a compromised update key can sign patches for every build of its release line | a key is never changed inside a line; the compromise response revokes the record and ships a new release line with a new key, re-signing DLC for it (`Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Key replacement", "Compromise response") | the key runbook |
| R7 | deleting PACK's install state resets the downgrade floor to the committed directories | the install record still classifies the downgrade at the close of startup step 11 and reports it | `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Replay and downgrade" |
| R8 | development builds accept patches signed with a published test key | development builds are not distributed; Shipping refuses a published test key at configure | `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Fail-closed defaults" |
| R9 | Tracy has no session token | loopback only, opt-in on the command line, absent from Shipping ([Z2-N-ARCH-959]) | "Remote-tool session rules" |
| R10 | the parsers of libraries `linux-tsan` does not build (every GPU-matrix library and module, `ZenithContentCore`, the `ZENITH_TOOLS_HOST` tools) get no UBSan replay | each is fuzzed under ASan in every smoke and campaign, and every tier 1 and tier 2 one that targets Android is replayed under HWASan on the S24 at each stage acceptance from Stage 3; a CPU-only replay executable would need the GPU modules' headers | `Zenith2/Docs/Design/SEC/Fuzzing.md` → "linux-tsan replay with UBSan" |

## Non-goals

<!-- informative -->
Zenith2 is a single-player production engine. It does not provide anti-cheat, DRM, code obfuscation, save encryption or authentication, or protection against a local administrator, a debugger attached by the device owner, or root on Android. Networked multiplayer integrity (Stage 7) will need server authority and replication checks that the multiplayer foundations already shape; it is outside this model until Stage 7's NET design.
<!-- /informative -->
