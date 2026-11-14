# SEC design: shipping-build exposure audit

This file is part of the SEC design family; the entry document is `Zenith2/Docs/Design/SEC.md`. It specifies how Zenith2 proves that a Shipping build exposes no debugger, remote tool or development service ([Z2-N-ARCH-958]): the inventory of every development service with its Shipping mechanism, the four audit layers, the forbidden-symbol and forbidden-string lists with their schemas and matching rules, the listener-import scan, the positive controls that prove each scan can fail, the release-time audit tool, the Android manifest audit, and where each layer runs. The mechanism each service uses is its owner's (`Zenith2/Docs/Design/SHIP/Release.md` → "Development services in Shipping" states the one admitted pattern); the gate halves are protected and are PROC hand-offs SEC-P5 to SEC-P7, and the scan logic itself (the exposure script and its matcher functions) is protected too, as PROC hand-off SEC-P9 ("Release-time audit tool").

## Scope of this file

| Topic | Heading |
|---|---|
| what must be absent, per service | "Development-service inventory" |
| the four layers | "Audit layers" |
| symbols | "Forbidden symbol list" |
| strings | "Forbidden string list" |
| listening sockets | "Listener-import scan" |
| proving the scans can fail | "Positive controls" |
| the release-time tool | "Release-time audit tool" |
| Android | "Android manifest audit" |
| behaviour | "ShippingTest behaviour cases" |
| placement | "Gate and release placement" |

## Development-service inventory

Every development service, remote tool and development-only input path, with what removes it from Shipping, its forbidden symbols ("Consolidated entries") and its behaviour test in the `ShippingTest` configuration:

| # | Service | Owner | Shipping mechanism | Forbidden symbols | Behaviour test |
|---|---|---|---|---|---|
| D1 | Luau DAP debugger server | SCRIPT | created only under `if constexpr (!Zenith_IsShippingBuild())` (`Zenith2/Docs/Design/SCRIPT/Debugger.md` → "DAP service ownership and lifetime") | `Zenith_ScriptDap` (prefix) | `Test_Script_Dap_AbsentFromShipping` |
| D2 | Luau source compiler | SCRIPT | no Shipping link closure contains `ZenithScriptCompiler`, `Luau.Compiler` or `Luau.Ast` ([Z2-N-ARCH-241]) | `ZenithScriptCompiler` (prefix), `Luau::compile` (prefix), `Luau::Parser` (prefix) | the `Libgraph` closure check |
| D3 | development listener and client | PLAT | `DEV_SERVICE_ERROR_NOT_IN_SHIPPING` at every entry (`Zenith2/Docs/Design/PLAT/Platform.md` → "Shipping exclusion") | `Zenith_DevListenerImpl::Open`, `Zenith_DevClientImpl::Connect` | `Test_DevListener_ShippingReturnsNotInShipping` |
| D4 | file watcher | PLAT | as D3 | `Zenith_FileWatcherImpl::Start` | the watcher's `ShippingTest` case (PLAT) |
| D5 | remote diagnostics server | DEVT | `Zenith_DevToolsServer::Create` refuses in Shipping (`Zenith2/Docs/Design/DEVT/Console.md` → "Shipping absence of development services") | `Zenith_DevToolsServer::HandleMessage` | DEVT's `ShippingTest` cases (`Zenith2/Docs/Design/DEVT/Tests.md` → "ShippingTest cases") |
| D6 | development console and cheat commands | DEVT | `Zenith_DevConsole::Create` refuses; every console command, cheats included, exists only behind it | `Zenith_DevConsole::ExecuteStatement` | as D5 |
| D7 | development file writer | DEVT | not constructed; `Write` refuses. Every file written under `user:/devtools/` goes through `Zenith_DevFileWriterUserMount::Write`: DEVT's `asset.dump`, telemetry, captures and evidence, CINE's captures and ADVGFX's `r.vt.dump` (row SEC1), so one symbol covers every development writer | `Zenith_DevFileWriterUserMount::Write` | as D5 |
| D8 | DEVT frame dump | DEVT | not created | `Zenith_DevFrameDump::WriteFrame` | as D5 |
| D9 | Tracy client | DEVT | not linked in `Shipping` and `ShippingTest` ([Z2-N-ARCH-124]) | `___tracy_startup_profiler`, `___tracy_shutdown_profiler` | as D5 |
| D10 | graph panel dump reader | GRAPH | not registered | `Zenith_GraphPanelProvider::ReadDumpFile` | as D5 |
| D11 | GPU capture tools (RenderDoc, PIX) | RHI | refused in Shipping; the configuration key is ignored | `Zenith_VulkanRenderDocTool::Load`, `Zenith_D3D12PixTool::Load`, `Zenith_D3D12RenderDocTool::Load` | `Test_Validation_ShippingIgnoresKey` |
| D12 | GPU validation layers | RHI | never enabled in Shipping | none (a configuration path, not an entry point) | `Test_Validation_ShippingIgnoresKey` |
| D13 | loose mount and `--assets-root` | PLAT, MOD | `AddSource(MOUNT_KIND_LOOSE, …)` refuses; the option is `COMMAND_LINE_ERROR_UNKNOWN_OPTION` | none | `Test_VirtualFileSystem_LooseAbsentInShipping`, `Test_CommandLine_AssetsRootRefusedUnderShippingRules` |
| D14 | command-line host files and directories, the budget-run mode | PLAT, MOD, DEVT | the parser rejects the options; the open functions refuse | none | `Test_CommandLineHostFile_GrantOnly` (its Shipping half) |
| D15 | editor live link and automation endpoint | EDIT | tool only; no Player link closure contains a tool target ([Z2-N-ARCH-241]) | none (closure check) | `Test_EditorLiveLink_LoopbackOnly` (tool side) |
| D16 | simulated asset pack delivery and `package.simulatedDelivery.root` | PACK | non-Shipping only | the simulated delivery's entry symbol (pending, row SEC13) | PACK's Shipping case (pending, row SEC13) |
| D17 | ASSET test kinds | ASSET | `Zenith_AssetRegistry::Create` refuses `m_bAllowTestKinds` in Shipping | none | ASSET's registry `ShippingTest` case |
| D18 | ONLINE scripted backends and offline test access | ONLINE | refused in Shipping (`Zenith2/Docs/Design/ONLINE.md` → "Security assumptions", S7) | each seam's entry symbol (pending, row SEC8) | ONLINE's Shipping cases |
| D19 | run telemetry | DEVT | the key is refused in Shipping | none | as D5 |
| D20 | cinematic capture and export | CINE | `Zenith_CinematicCapture` is created at Start step 7 only when `!Zenith_IsShippingBuild()` (an `if constexpr`), so its code is unreferenced, so unlinked, in Shipping (`Zenith2/Docs/Design/CINE/Video.md` → "Capture algorithm"; hand-off CINE-P4) | `Zenith_CinematicCapture::Begin` (prefix), `Zenith_ExportCinematicCapture` (prefix) | `Test_CineCapture_AbsentInShipping` |
| D21 | the Stage 3 network transport | NET | `Zenith_Module_Net_Start` forces `net.role` to `offline` in Shipping and creates no socket, setting `NET_ERROR_NOT_IN_SHIPPING` in the module note (`Zenith2/Docs/Design/NET/Foundations.md` → "Shipping and the headless roles"; row NT9), until Stage 7 lifts the guard with authenticated sessions | `Zenith_NetTransport::Create` | `Test_NetHeadless_ShippingForcesOffline` |

Present in Shipping by design, and outside this audit: the on-screen profiler overlay, off by default (Z2-D-005); the profiler in counters-only mode; the debug-draw recorder; the log file sink, off by default; the crash handler and the Windows crash helper, whose IPC is boundary B9.

## Audit layers

| Layer | What it proves | Where it runs |
|---|---|---|
| E1 behaviour | each service's public entry refuses or creates nothing when `Zenith_IsShippingBuild()` is true | the `ShippingTest` cases of "ShippingTest behaviour cases", in the `Test` step's `win-shipping-test` preset |
| E2 symbols | no listed entry symbol survives into a Shipping image | the `Libgraph` step's rule LG9 ([Z2-N-ARCH-964]; `Zenith2/Docs/Design/PROC/StructuralGates.md` → "Rules"), its entries filed by hand-off SEC-P5; the release verification (SHIP W5, A6) |
| E3 strings | no listed byte sequence (a handshake literal, a published test key) occurs in a Shipping image | the `Libgraph` step's Shipping check (hand-off SEC-P6); the release verification (SHIP W16, A15, row SEC6) |
| E4 imports | a Shipping image imports no API that accepts connections, and before Stage 7 none that binds a socket | the `Libgraph` step's Shipping check (hand-off SEC-P7); the release verification (SHIP W17, A16, row SEC6) |

## Forbidden symbol list

### shipping_forbidden_symbols.json schema

The file's format is PROC's, which owns the `Libgraph` step that reads it (`Zenith2/Docs/Design/PROC/StructuralGates.md` → "shipping_forbidden_symbols.json"): `schema` `zenith2.shipping-forbidden-symbols`, `schemaVersion` 1, and an `entries` array of `{ "symbol", "match", "owner", "design" }` objects, `match` being `exact` or `prefix`. SEC adds no field; the service row and the requirement of each entry are recorded in "Consolidated entries", which is SEC's judgement of which symbols are development entry points, while every entry is filed by a PROC ticket copying the owning area's hand-off ([Z2-N-ARCH-963]). `Zenith2/Tools/Gates/Security/Test-Zenith2ShippingExposure.ps1` reads the file in that format.

### Symbol normalisation and matching

1. Windows: list the public symbols of the image's PDB with `llvm-pdbutil dump -publics`, demangle each with `llvm-undname`. Android: list the defined symbols of the unstripped shared object with `llvm-nm --defined-only --demangle`.
2. Normalise each demangled name: remove the return type, the calling convention (`__cdecl`, `__stdcall`), access specifiers (`public:`, `private:`), the parameter list and everything after it (`const`, `noexcept`, reference qualifiers), and every template argument list; keep the scope-qualified name (`Zenith_DevConsole::ExecuteStatement`). A C symbol is its name as linked (`___tracy_startup_profiler`).
3. An entry with `match` `exact` matches a normalised name equal to its `symbol` (any signature, as PROC's format states); `prefix` matches one that begins with `symbol`.
4. Any match fails the check with `shipping exposure: <image>: forbidden symbol <normalised name> (entry <symbol>, owner <owner>)`.

### Consolidated entries

The complete list at Stage 6, gathered from every area's hand-off (each line keeps its owner's requirement):

| `symbol` | `match` | Owner | Service | Requirement | From |
|---|---|---|---|---|---|
| `Luau::Parser` | prefix | SCRIPT | D2 | Z2-R-SCRIPT-015 | SCRIPT-P3 |
| `Luau::compile` | prefix | SCRIPT | D2 | Z2-R-SCRIPT-015 | SCRIPT-P3 |
| `ZenithScriptCompiler` | prefix | SCRIPT | D2 | Z2-R-SCRIPT-015 | SCRIPT-P3 |
| `Zenith_CinematicCapture::Begin` | prefix | CINE | D20 | Z2-R-CINE-132 | CINE-P4 |
| `Zenith_D3D12PixTool::Load` | exact | RHI | D11 | Z2-R-RHI-113 | RHI-P6 |
| `Zenith_D3D12RenderDocTool::Load` | exact | RHI | D11 | Z2-R-RHI-113 | RHI-P6 |
| `Zenith_DevClientImpl::Connect` | exact | PLAT | D3 | Z2-R-PLAT-139 | PLAT-P6 |
| `Zenith_DevConsole::ExecuteStatement` | exact | DEVT | D6 | Z2-R-DEVT-119 | DEVT-P2 |
| `Zenith_DevFileWriterUserMount::Write` | exact | DEVT | D7 | Z2-R-DEVT-119 | DEVT-P2 |
| `Zenith_DevFrameDump::WriteFrame` | exact | DEVT | D8 | Z2-R-DEVT-119 | DEVT-P2 |
| `Zenith_DevListenerImpl::Open` | exact | PLAT | D3 | Z2-R-PLAT-139 | PLAT-P1 |
| `Zenith_DevToolsServer::HandleMessage` | exact | DEVT | D5 | Z2-R-DEVT-119 | DEVT-P2 |
| `Zenith_ExportCinematicCapture` | prefix | CINE | D20 | Z2-R-CINE-132 | CINE-P4 |
| `Zenith_FileWatcherImpl::Start` | exact | PLAT | D4 | Z2-R-PLAT-139 | PLAT-P1 |
| `Zenith_GraphPanelProvider::ReadDumpFile` | exact | GRAPH | D10 | Z2-R-DEVT-119 | DEVT-P2 |
| `Zenith_NetTransport::Create` | exact | NET | D21 | Z2-R-NET-043 | NT9 |
| `Zenith_ScriptDap` | prefix | SCRIPT | D1 | Z2-R-SCRIPT-015 | SCRIPT-P3 |
| `Zenith_VulkanRenderDocTool::Load` | exact | RHI | D11 | Z2-R-RHI-113 | RHI-P6 |
| `___tracy_shutdown_profiler` | exact | DEVT | D9 | Z2-R-DEVT-119 | DEVT-P2 |
| `___tracy_startup_profiler` | exact | DEVT | D9 | Z2-R-DEVT-119 | DEVT-P2 |

D16 and D18 add their entries when PACK's and ONLINE's tickets declare the symbols (rows SEC13, SEC8). The table is in byte order of `symbol`; PROC's file lists the same entries. NET's `Zenith_NetTransport::Create` leaves the list at Stage 7, in the ticket that lifts NET's Shipping guard together with authenticated sessions; the E4 `bind` rule leaves with it ("Listener-import scan").

## Forbidden string list

### shipping_forbidden_strings.json schema

`Zenith2/Tools/Gates/shipping_forbidden_strings.json` (protected; created and changed by PROC tickets):

```json
{
    "schema": "zenith2.shipping-forbidden-strings",
    "schemaVersion": 1,
    "strings": [
        {
            "id": "dev-token-handshake",
            "text": "ZENITH2-TOKEN ",
            "owner": "PLAT",
            "why": "the development listener's handshake literal exists only in its development-only translation unit",
            "positiveControl": true
        },
        {
            "id": "rfc8032-test2-public-key",
            "hex": "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c",
            "owner": "SEC",
            "why": "a published test key compiled into a Shipping Player would accept patches anyone can sign",
            "positiveControl": false
        }
    ]
}
```

| Field | Rule |
|---|---|
| `id` | `^[a-z0-9-]{1,64}$`, unique |
| `text` or `hex` | exactly one: `text` is matched as its UTF-8 bytes and as its UTF-16LE bytes; `hex` is an even-length lower-case byte string matched exactly; 4 to 256 bytes (a `text` entry's UTF-16LE form is at most 512 bytes) |
| `owner`, `why` | the area and the reason |
| `positiveControl` | `true` when the bytes must occur in the Release image of the `Minimal` game ("Positive controls") |

The whole image file is scanned (every section), so a literal placed in any section is found.

### Initial string entries

| `id` | Bytes | Owner | `positiveControl` | Why |
|---|---|---|---|---|
| `dev-token-handshake` | text `ZENITH2-TOKEN ` | PLAT | true | the handshake literal of `Zenith_DevListener`; present in Release, so the scan is proved able to find it |
| `dap-set-breakpoints` | text `setBreakpoints` | SCRIPT | true | the DAP request name, present only in the DAP server's translation units |
| `rfc8032-test1-public-key` | hex `d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a` | SEC | true | the development default update key (`Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Fail-closed defaults"); present in Release by design, refused in Shipping |
| `rfc8032-test2-public-key` | hex `3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c` | SEC | false | a published test key |
| `rfc8032-test3-public-key` | hex `fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025` | SEC | false | a published test key |
| `rfc8032-test1-seed` | hex `9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60` | SEC | false | a published private seed; tests only |

## Listener-import scan

| Platform | Command | Fails when |
|---|---|---|
| Windows | `llvm-readobj --coff-imports <Game>.exe` (delay-load imports included) | the image imports from `WS2_32.dll`, `WSOCK32.dll` or `MSWSOCK.dll` (names compared case-insensitively) any of `bind`, `listen`, `accept`, `WSAAccept`, `AcceptEx`, or by ordinal `WS2_32.dll` 1 (`accept`), 2 (`bind`) or 13 (`listen`) |
| Android | `llvm-readelf --dyn-syms lib<Game>.so` | an undefined symbol is `bind`, `listen`, `accept` or `accept4` |

The failure message is `shipping exposure: <image>: imports <symbol> from <library>`. The scan proves there is no listening socket in a Shipping image whatever its symbols are called; outgoing connections that a platform SDK makes for itself are not listeners and are not refused.

PLAT's `Zenith_DatagramSocket` (row NT1) is a Shipping-capable UDP socket in `ZenithPlatform/Internal/<Platform>/` whose `Open` calls `bind`. Until Stage 7, NET's Shipping guard leaves it unreferenced in every Shipping executable, so the linker strips it and the `bind` rule stays as written: a Shipping image that imports `bind` before Stage 7 is a finding, and the scan is the proof that the guard and the dead-stripping both hold. Whether a Stage 7 Shipping game that selects `Net` may import `bind` (a UDP socket bound for authenticated sessions, never `listen` or `accept`), and how the scan then tells it from a listener, is a question routed to NET as row SEC17 (c), answered in the Stage 7 session-security ticket.

## Positive controls

A scan that cannot fail proves nothing, so each layer is run once against the Release image of the `Minimal` game, where the development services exist by design (SHIP's D2), and must find:

| Layer | Must find in the Release image |
|---|---|
| E2 | `Zenith_DevListenerImpl::Open` and `Zenith_DevConsole::ExecuteStatement` |
| E3 | every string entry whose `positiveControl` is true |
| E4 | `bind` and `listen` (the development listener) |

A missing positive control fails the check with `shipping exposure: positive control missing: <layer> <item>` and names the scan as broken rather than the image as clean.

## Release-time audit tool

### Test-Zenith2ShippingExposure command line

```
pwsh -NoProfile -File Zenith2/Tools/Gates/Security/Test-Zenith2ShippingExposure.ps1
	-Image <path to <Game>.exe or the unstripped lib<Game>.so>
	-Symbols <path to the retained PDB (Windows); omitted on Android, where -Image is the unstripped object>
	-Platform windows|android
	[-PositiveControl]               # run the positive controls against a Release image instead
	[-SymbolList <file>] [-StringList <file>]   # default: the committed lists under Zenith2/Tools/Gates/
	[-Out <report.json>]
```

Exit codes: 0 clean (or, with `-PositiveControl`, every control found); 1 a finding; 2 a usage error or a missing tool (`llvm-pdbutil`, `llvm-undname`, `llvm-nm`, `llvm-readobj`, `llvm-readelf`, located through `Get-Zenith2ToolPath` of SHIP's setup module). SHIP's release verification calls it for W5, W16 and W17 (Windows) and A6, A15 and A16 (Android) (row SEC6 (h)); the `Libgraph` step's Shipping check calls the same script (hand-offs SEC-P5 to SEC-P7), so the gate and the release share one implementation.

The script and the matcher functions it uses (`ConvertTo-Zenith2NormalisedSymbol`, `Find-Zenith2ForbiddenSymbol`, `Find-Zenith2ForbiddenString`, `Find-Zenith2ListenerImport`, in the module `Zenith2/Tools/Gates/Security/Zenith2SecurityGates.psm1`) live under the protected `Zenith2/Tools/Gates/Security/` (Z2-D-128), because a gate's scan logic that an ordinary ticket could edit would let a worker narrow a scan while the positive controls stay green. PROC hand-off SEC-P9 creates them from this file; SEC's ordinary tickets never edit them. Their tests, `Zenith2/Tools/Gates/Tests/Security/Test-Security.ps1` over fixtures under `Zenith2/Tools/Gates/Tests/Security/Fixtures/`, include one negative-control fixture per scan: an image excerpt with a forbidden UTF-16LE string straddling a chunk boundary ("Audit algorithm" step 3), a Windows import listing that imports `bind` by ordinal only, and a symbol listing whose forbidden name differs from the entry only in its template arguments; each must be reported.

### Audit algorithm

1. Validate the arguments and both lists against their schemas; any error is exit 2 with the field named.
2. E2: run the symbol listing of the platform, normalise every name, match every entry; collect findings.
3. E3: read the image file into memory in 64 MiB chunks with an overlap between chunks of the longest pattern's length minus one (511 bytes for the 512-byte UTF-16LE form of a 256-byte `text`), so a pattern straddling a chunk boundary is found exactly once; for every entry build the byte patterns (UTF-8 and UTF-16LE for `text`, the bytes for `hex`) and search each chunk with a byte-wise search; collect findings with the file offset.
4. E4: run the import listing of the platform and match the rules of "Listener-import scan"; collect findings.
5. With `-PositiveControl`, invert steps 2 to 4 for the control items of "Positive controls": a control item not found is a finding; findings of non-control items are ignored.
6. Write the report and exit.

The cost is linear in the image size and the number of entries: a 200 MiB image and 32 patterns take two passes over the file in practice.

### Audit report

`zenith2.shipping-exposure` version 1:

```json
{
    "schema": "zenith2.shipping-exposure",
    "schemaVersion": 1,
    "image": "OpenWorldSlice.exe",
    "imageSha256": "<64 hex>",
    "platform": "windows",
    "mode": "audit",
    "symbolList": "<64 hex: SHA-256 of the list used>",
    "stringList": "<64 hex>",
    "findings": [
        { "layer": "E2", "item": "Zenith_DevConsole::ExecuteStatement", "detail": "public symbol" }
    ],
    "result": "clean"
}
```

`mode` is `audit` or `positive-control`; `result` is `clean`, `findings` or `controls-missing`.

## Android manifest audit

Two release checks complete the Android exposure picture (row SEC6 asks SHIP to add them to its Android package checklist as A17 and A18; MOD's Gradle template is their input, row SEC5):

| Check | Command | Passes when |
|---|---|---|
| A17 permissions | `aapt2 dump permissions <apk>` | the permission set equals the union of `Zenith2/Tools/Security/android_permissions.json` entries for the game's selected modules, and contains no `android.permission.INTERNET` unless an entry for a selected module allows it |
| A18 exported components | `aapt2 dump xmltree --file AndroidManifest.xml <apk>` | every component with `android:exported="true"` is the launcher activity or is listed in the same file |

```json
{
    "schema": "zenith2.android-permissions",
    "schemaVersion": 1,
    "permissions": [
        { "name": "android.permission.INTERNET", "module": "Online", "buildTypes": ["shipping"], "why": "the Google Play games backend's SDK", "owner": "ONLINE" }
    ],
    "exportedComponents": [
        { "class": "org.zenith2.player.Zenith2Activity", "module": "core", "why": "the launcher activity", "owner": "MOD" }
    ]
}
```

The `debug` and `hwasan` build types are not audited; the development `release` build type declares `INTERNET` for the listeners reached through `adb forward` and is never distributed. SHIP's A7 (not debuggable) already covers the debugger flag.

## ShippingTest behaviour cases

The E1 evidence is each owner's `ShippingTest` cases. SEC adds composed checks that no single owner makes (`Zenith2/Docs/Design/SEC/Tests.md` → "Shipping behaviour tests"): `Test_SecShipping_ListenerRefusedForEveryService` (the listener refuses for every service name), `Test_SecShipping_DevelopmentInputsRefused` (the loose mount and both host-file opens refuse), `Test_SecShipping_UserNamespaceDevtoolsRefused` and `Test_SecScript_BytecodeOnlyFromPackageAndPatch`; the update key's Shipping rule is proved at configure by `CMake_Sec_UpdateKeyResolution` and at compile time by the composition table's static assertion.

## Gate and release placement

| Check | Gate step | Release verification | Acceptance |
|---|---|---|---|
| E1 | `Test` (`win-shipping-test`) | — | every stage acceptance from Stage 3 |
| E2 | `Libgraph` Shipping check (SEC-P5) | SHIP W5, A6 | Stage 6 security evidence, every game |
| E3 | `Libgraph` Shipping check (SEC-P6) | SHIP W16, A15 | as E2 |
| E4 | `Libgraph` Shipping check (SEC-P7) | SHIP W17, A16 | as E2 |
| positive controls | `Libgraph` (once per run, against the Release `Minimal` image) | the release verification's dry run | as E2 |
| A17, A18 | — | SHIP A17, A18 | as E2 |
