# SEC design — Security & robustness

The design of the SEC area: the trust-boundary model of Zenith2 (scripts, assets, remote tools, updates, user files, the network, online services, tool inputs and process IPC), the programme-wide parser and fuzz inventory with its conformance rule and gate step, the fuzzing policy, path-traversal protection as a tested matrix, the shipping-build exposure audit, and authenticated update manifests with their key lifecycle. SEC owns rules, inventories, audits and a small amount of code (the Ed25519 verification primitive inside ZenithPlatform, a PowerShell module and tools, CMake functions, an integration suite); the mechanisms it audits belong to their areas, whose designs SEC cites and never re-specifies. Workers implement against this file and its companions; the requirement catalogue is `Zenith2/Docs/Requirements/SEC.md`.

## Purpose and scope

### What SEC owns

| Capability | Inventory items | Specified in |
|---|---|---|
| the trust-boundary model: nine boundaries, each with its source, what is trusted, what is validated and where, its failure behaviour and evidence | Z2-I-SEC-01, Z2-I-SEC-02, Z2-I-SEC-03, Z2-I-SEC-04 | `Zenith2/Docs/Design/SEC/TrustBoundaries.md` → "The nine boundaries" |
| the confirmations other areas wait on: the verified-cache stamp set, its trust precondition and the keyed-record escalation (row PK27); ONLINE's security assumptions (row ON15); the remote-tool session rules | Z2-I-SEC-03, Z2-I-SEC-04 | `Zenith2/Docs/Design/SEC/TrustBoundaries.md` → "Verified-cache confirmation", "Online-services confirmation", "Remote-tool session rules" |
| the one inventory of every parser of external bytes and every fuzz target, each with its owner ticket; the parser-limit conformance rule and the SecurityConformance gate step | Z2-I-SEC-05, Z2-I-SEC-06 | `Zenith2/Docs/Design/SEC/ParserInventory.md`; `Zenith2/Docs/Design/SEC/ConformanceCheck.md` |
| the programme-wide fuzzing policy: harness and corpus rules, tool-side fuzz targets, sanitizer configurations, run-time budgets per gate and acceptance, crash triage into tickets | Z2-I-SEC-06 | `Zenith2/Docs/Design/SEC/Fuzzing.md` |
| path-traversal protection as a control in depth, the vector and source catalogues, the test matrix, the composed tests, the confined-API rule | Z2-I-SEC-07 | `Zenith2/Docs/Design/SEC/PathTraversal.md` |
| authenticated update manifests: the verification rules and their order, the Ed25519 primitive and its selection, the public key's provenance, the key lifecycle and ceremony, replay and downgrade, the tamper suite | Z2-I-SEC-08 | `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` |
| no debugger and no remote tool in Shipping: the development-service inventory, the symbol, string and import scans with positive controls, the release-time audit tool, the Android manifest audit | Z2-I-SEC-09, Z2-I-SEC-10 | `Zenith2/Docs/Design/SEC/ShippingExposure.md` |
| the security evidence of every stage acceptance and of Stage 6 ([Z2-N-ARCH-960]) | Z2-I-SEC-01 to Z2-I-SEC-10 | "Stage plan"; `Zenith2/Docs/Design/SEC/Tests.md` → "Acceptance items" |

### Explicit exclusions

| Not SEC | Owner | Rationale |
|---|---|---|
| the virtual filesystem, path validation, mounts, the escape check, command-line grants, the patch mount and `Zenith_VerifyUpdateManifest` with its three halves | PLAT | SEC owns the rules (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Patch mount verification" names them SEC's); PLAT implements them |
| the update and DLC manifest layout, U0 to U11, the applier, the verified cache, Windows DLC mounting, the signer, pack verification | PACK | SEC consolidates and confirms; it does not re-specify a format |
| key custody, the release profile, `Get-Zenith2UpdateSigningKey`, Authenticode and APK signing, the package checklists | SHIP | SEC adds rules and checks the release pipeline runs |
| fuzzing conventions, the campaign runner, the crash handler and the crash helper | DEVT | SEC sets durations, tiers and triage; DEVT's runner executes them |
| the Luau sandbox, budgets, bytecode verification, the DAP server | SCRIPT | SEC records the script boundary; SCRIPT implements it |
| every format's parser and its limits | the format's owner | SEC's inventory names them; the numbers stay in the owners' designs and in `Zenith2/Docs/Architecture.md` → "Security architecture" |
| modding and third-party content | none | modding is excluded (Z2-D-075); no mount accepts it |
| dynamic plugins | none | excluded (Z2-D-024); the confined-API rule keeps dynamic loading out of runtime code |
| anti-cheat, DRM, save authentication, protection against the device owner | none | single-player scope (`Zenith2/Docs/Design/SEC/TrustBoundaries.md` → "Non-goals") |
| networked multiplayer integrity | NET (Stage 7) | deferred with networked multiplayer (Z2-D-073); SEC records the Stage 3 loopback boundary only |

### Inventory items covered

Z2-I-SEC-01 to Z2-I-SEC-10, all required. The coverage table is `Zenith2/Docs/Requirements/SEC.md` → "Coverage of inventory items".

### Document family

| File | Holds | Its headings |
|---|---|---|
| `Zenith2/Docs/Design/SEC.md` (this file) | the skeleton, ownership, layout, API, formats, the algorithm index, errors, diagnostics, failure handling, budgets, stage plan, decisions, the requirement index | the headings of this file |
| `Zenith2/Docs/Design/SEC/TrustBoundaries.md` | the boundary model and the confirmations | Scope of this file · Boundary model · Boundary record fields · Boundary classes and tiers · The nine boundaries · B1 shipped content · B2 patches and downloaded content · B3 user files · B4 scripts · B5 remote tools · B6 network · B7 online services · B8 tool inputs · B9 process IPC · Trust preconditions · Install-root precondition · Device precondition · Same-user precondition · Verified-cache confirmation · Stamp set confirmed · Records unauthenticated · Keyed-record escalation · Distribution-layout audit · Online-services confirmation · Remote-tool session rules · Saves and integrity · Accepted residual risks · Non-goals |
| `Zenith2/Docs/Design/SEC/ParserInventory.md` | the inventory | Scope of this file · Inventory rules · Boundary classes and tiers used here · Entry columns · Core and platform parsers · Script reflection and scene parsers · Asset and geometry parsers · Rendering parsers · Animation physics and AI parsers · Streaming save and audio parsers · UI and 2D parsers · Environment VFX and cinematics parsers · Packaging and update parsers · Developer tool parsers · Online and network parsers · Content and authoring parsers · Conformance findings · Tier totals and campaign estimate |
| `Zenith2/Docs/Design/SEC/Fuzzing.md` | the fuzzing policy | Scope of this file · What the policy adds to the DEVT conventions · Harness rules · Corpus and dictionary rules · Stem naming rule · Tool-side fuzz targets · Sanitizer configurations · win-asan with libFuzzer · linux-tsan replay with UBSan · HWASan replay on the S24 · Run-time budgets · Gate smoke · Smoke selection · Smoke step budget · Stage acceptance campaigns · Campaign wall-time estimate · Crash triage · Finding records · Deduplication key · Severity mapping · Finding ticket body · Finding lifecycle · Regression rule · Campaign evidence |
| `Zenith2/Docs/Design/SEC/PathTraversal.md` | path-traversal protection | Scope of this file · Controls in depth · Vector catalogue · Source catalogue · The matrix · Matrix rows for runtime sources · Matrix rows for development and tool sources · Composed traversal tests · Confined OS APIs · Confined API classes · confined_apis.json · Rules confirmed by the ruling · Accepted residuals for paths · Matrix maintenance |
| `Zenith2/Docs/Design/SEC/ShippingExposure.md` | the exposure audit | Scope of this file · Development-service inventory · Audit layers · Forbidden symbol list · shipping_forbidden_symbols.json schema · Symbol normalisation and matching · Consolidated entries · Forbidden string list · shipping_forbidden_strings.json schema · Initial string entries · Listener-import scan · Positive controls · Release-time audit tool · Test-Zenith2ShippingExposure command line · Audit algorithm · Audit report · Android manifest audit · ShippingTest behaviour cases · Gate and release placement |
| `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` | update authentication | Scope of this file · What is signed and what is checked · Verification rules · Rule order · Rules per manifest kind · Rejection and logging · Ed25519 verification primitive · Zenith_Ed25519 header · Primitive algorithm · Published test keys · Ed25519 implementation selection · Candidates · Criteria · Acceptance rule · Selection harness · Public key provenance · update_keys.json · Key resolution at configure · Composition-table key · Fail-closed defaults · Key lifecycle · Key states · Key ceremony · Ed25519KeyCeremony tool · Rotation · Compromise response · Replay and downgrade · Play-delivered packs · Tamper suite |
| `Zenith2/Docs/Design/SEC/ConformanceCheck.md` | the SecurityConformance step | Scope of this file · Inputs · Command line and exit codes · Checks · Messages · Report file · Fixtures · Gate placement and budget · PROC hand-off |
| `Zenith2/Docs/Design/SEC/Tests.md` | every SEC test and acceptance item | Scope of this file · Test targets and gate steps · Ed25519 primitive tests · Integration suite tests · Path-matrix composition tests · Update tamper tests · Shipping behaviour tests · PowerShell tests · CMake script-mode tests · Gate-step fixture tests · Acceptance items · Review focus for SEC tickets |

## Dependencies

### Libraries SEC code lives in

| Library or tool | SEC's part | Uses |
|---|---|---|
| ZenithPlatform | `Internal/Security/Zenith_Ed25519.h`, `.cpp`; the public `Zenith_UpdateKeyRules.h`; their tests and fuzz target | ZenithCore (`Zenith_ByteSpan`); the admitted Ed25519 third-party entry (`THIRD_PARTY`, joined by the implementing ticket) |
| `Zenith2/Tools/Security/` | the registries (data only), the triage, layout-audit and key-ceremony tools and their module | pwsh 7, the pinned JDK, SHIP's `Zenith2Release.psm1` and `Get-Zenith2ToolPath` |
| `Zenith2/Tools/Gates/Security/` (protected; hand-off SEC-P9) | the scan logic gates run: the registry readers, the exposure script and its matchers, the update-key check | pwsh 7, the pinned LLVM tools through `Get-Zenith2ToolPath` |
| `Zenith2/CMake/ZenithSecurity.cmake` | `zenith_security_resolve_update_key` | CMake `string(JSON)`; `execute_process` of the update-key check |
| `Zenith2/Tests/Security/` | `Zenith_Integration_Security` | ZenithCore, ZenithPlatform, ZenithCompression and its backends, ZenithAssets, ZenithPackage (its header-only test support), ZenithSave, ZenithScript |
| `Zenith2/Benchmarks/Ed25519/` | the selection harness | the two candidate entries |

### Headings SEC relies on

| Topic | Heading |
|---|---|
| the security architecture SEC elaborates | `Zenith2/Docs/Architecture.md` → "Security architecture" |
| third-party admission | `Zenith2/Docs/Architecture.md` → "Third-party dependencies" |
| results, untrusted input, assertions | `Zenith2/Docs/Contracts.md` → "Errors and results" |
| gate steps and categories | `Zenith2/Docs/Contracts.md` → "Enforceability matrix" |
| ForbiddenCheck rules | `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates" |
| path validation | `Zenith2/Docs/Architecture/PlatformLayer.md` → "Path validation" |
| targets, fuzz targets, presets, the matrix | `Zenith2/Docs/Architecture/BuildSystem.md` → "Test, conformance and fuzz targets", "The ZENITH_SUPPORTED matrix", "The declared_deps.json record" |
| the composition table and command line | `Zenith2/Docs/Architecture/CompositionRoot.md` → "The generated composition table", "Command-line options" |
| the script boundary | `Zenith2/Docs/Scripting.md` → "Sandboxing and security", "Shipping bytecode compatibility" |
| the stage acceptances | `Zenith2/Docs/Roadmap.md` → "Stage 1 Architecture validation", "Stage 6 Shipping qualification", "Acceptance evidence" |
| budget rows | `Zenith2/Docs/Budgets.md` → "Gate budget rows", "Row index" |

### Areas whose designs SEC consolidates

| Area | SEC consolidates or confirms | Heading relied on |
|---|---|---|
| PLAT | rules R1 to R10, the escape check, the case policy, command-line grants and the evidence directory, the development listener, the patch mount | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Validation rules and order", "Escape check after resolution", "Command-line host files", "Patch mount verification"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Loopback socket", "Shipping exclusion" |
| PACK | the update manifest and its checks, the verified cache and its three confirmation items (row PK27), the applier, the signer | `Zenith2/Docs/Design/PACK/Formats.md` → "Update manifest checks", "Verified stamp file"; `Zenith2/Docs/Design/PACK/Algorithms.md` → "Verified cache"; `Zenith2/Docs/Design/PACK/Tooling.md` → "Ed25519 signer" |
| SCRIPT | the sandbox, bytecode from authenticated mounts, the DAP server's exposure | `Zenith2/Docs/Design/SCRIPT/Debugger.md` → "DAP service ownership and lifetime"; `Zenith2/Docs/Design/SCRIPT/Tests.md` → "Module binding and sandbox tests" |
| ASSET | cook and loader limits, archive checks, fuzz targets | `Zenith2/Docs/Design/ASSET/Formats.md` → "Archive checks"; `Zenith2/Docs/Design/ASSET/Tests.md` → "Fuzz targets" |
| DEVT | remote tools absent from Shipping, Tracy loopback only, the crash helper's IPC rules, fuzz conventions and the campaign runner | `Zenith2/Docs/Design/DEVT/Console.md` → "Shipping absence of development services"; `Zenith2/Docs/Design/DEVT/Crash.md` → "Crash helper security contract"; `Zenith2/Docs/Design/DEVT/QualityTools.md` → "Fuzzing", "Fuzz campaign runner" |
| SHIP | key custody and the signing hand-off, the Shipping configuration, the package checklists, hardening flags, fuzz registration, the test registry's functions (where `zenith_tool_fuzz_target` lives), the selection-candidate pins | `Zenith2/Docs/Design/SHIP/Release.md` → "Key custody", "Update manifest signing hand-off", "Development services in Shipping"; `Zenith2/Docs/Design/SHIP/Qualification.md` → "Security and compatibility items"; `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Hardening flags", "Fuzz target registration", "Registry functions"; `Zenith2/Docs/Design/SHIP/Manifest.md` → "Selection-candidate entries" |
| MOD | static modules, no dynamic plugins, the command-line parser, the composition table that will carry the update key | `Zenith2/Docs/Design/MOD.md` → "Composition validation at startup" |
| SAVE | saves as untrusted user files, format limits | `Zenith2/Docs/Design/SAVE/Formats.md` → "Format limits" |
| ONLINE | the nine security assumptions (row ON15) | `Zenith2/Docs/Design/ONLINE.md` → "Security assumptions" |
| NET | the security assumptions S1 to S6, the packet validation order and wire limits, the identity save section, the Stage 3 Shipping guard, five fuzz targets (row NT4) | `Zenith2/Docs/Design/NET.md` → "Security assumptions"; `Zenith2/Docs/Design/NET/Loopback.md` → "Packet validation order", "Wire limits"; `Zenith2/Docs/Design/NET/Foundations.md` → "Identity across save and restore", "Shipping and the headless roles" |
| CINE | the cinematic capture, created only outside Shipping (hand-off CINE-P4) | `Zenith2/Docs/Design/CINE/Video.md` → "Capture algorithm" |
| CONTENT | out-of-process importers and their limits | `Zenith2/Docs/Design/CONTENT/Importers.md` → "Parser limits" |
| EDIT | the live-link endpoint | `Zenith2/Docs/Design/EDIT/Automation.md` → "Endpoint security" |
| PROC | the gate mechanisms SEC's content feeds: the symbol scan, the sanitizer step and its path selection, the acceptance runs | `Zenith2/Docs/Design/PROC.md` → "Coordination with SEC" |
| every area with a parser | its inventory row | `Zenith2/Docs/Design/SEC/ParserInventory.md` |

### Areas that depend on SEC

| Area | What it uses |
|---|---|
| PLAT | the verification rules V1 to V9 its verifier executes; `Zenith_Ed25519Verify` and its files, which SEC owns inside ZenithPlatform (PLAT's row P29); the confirmation of R1 to R10 and the escape check (its revalidation ticket's decision set); the R10 extension (row SEC1) |
| PACK | the confirmation of the stamp set, the trust precondition and the escalation; the verification rules |
| SHIP | the Stage 6 security evidence of qualification item S1; the full fuzz duration (row SH8); the exposure audit tool for W5, A6 and the new W16, W17, A15 to A18 |
| DEVT | the durations, tiers and triage its runner applies |
| CORE | the `.hex` regression decoder and replay helper SEC specifies for `ZenithTestSupport` (row SEC18) |
| NET | the confirmation of S1 to S6 as boundary B6's record, the inventory rows of its five parsers, the stem rule for its fuzz files (row SEC17) |
| every area with a parser or fuzz target | the inventory rules and the SecurityConformance step |
| every area with a development service | the exposure inventory and the forbidden lists |

### Upward interfaces SEC declares

None. SEC's code is a leaf primitive inside ZenithPlatform and host-side tooling; it calls no higher library and registers no callback.

### Assumed interfaces pending other areas

<!-- assumption: pending PLAT -->
SEC assumes PLAT's verifier takes the form `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Patch mount verification" and `Z2T-PLAT-092` now declare: `Zenith_DecodeUpdateManifest` (V2 to V4), `Zenith_CheckUpdateManifestTarget` (V5) and `Zenith_VerifyUpdateManifestFiles` (V9 with the optional stamp lookup) in `Zenith2/Engine/ZenithPlatform/Zenith_UpdateManifest.h`, calling `Zenith_Ed25519Verify` for V3. SEC owns `Internal/Security/Zenith_Ed25519.h` and `.cpp` and the function name `Zenith_Ed25519Verify` (ruling SEC-R1-RULINGS R8, because the canonical-S and zero-key rules are SEC's); PLAT's row P29 of the cross-area notes records that `Z2T-PLAT-092` no longer creates the two files, no longer names `Zenith_Ed25519_Verify` and no longer edits `THIRD_PARTY`, and holds PLAT-092 on SEC's implementation ticket (A5 row PB28) until SEC's A5 batch files it; row SEC16 acknowledges P29 and commits SEC's batch to that link. SEC also assumes the R10 extension of row SEC1 and the session rules T3, T4 and T6 of row SEC2.
<!-- /assumption -->

<!-- assumption: pending MOD -->
SEC assumes MOD's composition table carries the update key and its static assertion, that `zenith_add_game` calls `zenith_security_resolve_update_key` and selects one of its two generated include files per configuration through the compile definition `ZENITH_UPDATE_KEY_INC` (row SEC5 (a); `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Key resolution at configure", "Composition-table key"), that MOD's change to `zenith_add_game` and the composition table is blocked by SEC's Stage 2 ticket that lands `zenith_security_resolve_update_key`, `Zenith_UpdateKeyRules.h` and the empty `update_keys.json` (row SEC5 (a)), and that MOD's test-APK push carries the four items of `Zenith2/Docs/Design/SEC/Fuzzing.md` → "HWASan replay on the S24": `Tests/Fuzz/Regressions`, `Tests/Fuzz/Corpus`, every `Tests/Fuzz/<Stem>.count` and the generated-seed root (row SEC5 (d)).
<!-- /assumption -->

<!-- assumption: pending DEVT, SHIP -->
SEC assumes DEVT's campaign runner gains `-Inventory` and `-Stage`, `campaign.json` findings gain `log` and `topFrames`, and the regression replay reads `.hex` regressions and the seed corpus (row SEC3); and that SHIP adds `toolFuzzTargets[]` to `declared_deps.json`, implements `zenith_tool_fuzz_target` as a test-registry function with `zenith_cook_fuzz_target` as its ZenithCook wrapper and keeps `Tests/Fuzz/**` out of every tool's `_Tests` and `_Objects` targets (row SEC6 (g)), adds the release checks W16, W17, A15 to A18 calling the protected `Zenith2/Tools/Gates/Security/Test-Zenith2ShippingExposure.ps1` (row SEC6 (h)), the `Zenith2/CMake/ZenithSecurity.cmake` ownership row, and the release-time comparison of `update_keys.json` with the profile (row SEC6), and pins the two Ed25519 harness candidates at Stage 1 (row SEC15).
<!-- /assumption -->

<!-- assumption: pending PROC -->
SEC assumes PROC's concurrently drafted gate mechanisms as `Zenith2/Docs/Design/PROC.md` → "Coordination with SEC" states them: PROC owns the format of `shipping_forbidden_symbols.json` and the `Libgraph` rule LG9 that reads it (`Zenith2/Docs/Design/PROC/StructuralGates.md` → "shipping_forbidden_symbols.json"), the `Sanitizers` step with its path selection over `sanitizer_paths.json` (`Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Path selection", "Sanitizer path selection"), and the acceptance invocation of DEVT's campaign runner with SEC's durations; SEC owns the content. SEC's hand-offs SEC-P1 to SEC-P9 extend those mechanisms (row SEC10); PROC's rule F24 (`Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck rule table") is SEC's confined-API rule, and its dynamic-loading allow records equal F22's (row SEC19).
<!-- /assumption -->

<!-- assumption: pending NET -->
SEC confirms NET's security assumptions S1 to S6 (`Zenith2/Docs/Design/NET.md` → "Security assumptions") as the record of boundary B6 (`Zenith2/Docs/Design/SEC/TrustBoundaries.md` → "B6 network"), inventories NET's five parsers as PI-NET-001 to PI-NET-005 with the limits of `Zenith2/Docs/Design/NET/Loopback.md` → "Wire limits" (which replace the Architecture row "network messages (Stage 3)" through cross-doc item X394) and the save section of `Zenith2/Docs/Design/NET/Foundations.md` → "Identity across save and restore" (row NT4), and lists `Zenith_NetTransport::Create` as development service D21. SEC assumes NET renames its fuzz files to `Tests/Fuzz/<Stem>.cpp` and answers the Stage 7 question whether a Shipping game that selects `Net` may import `bind` (row SEC17).
<!-- /assumption -->

<!-- assumption: pending CORE -->
SEC assumes CORE adds `Zenith2/Engine/ZenithCore/Tests/Support/Zenith_FuzzRegression.h` to `ZenithTestSupport` with `Zenith_DecodeHexRegression` and `Zenith_ForEachRegression` (row SEC18; `Zenith2/Docs/Design/SEC/Fuzzing.md` → "HWASan replay on the S24").
<!-- /assumption -->

## Library and file layout

### SEC files in ZenithPlatform

| File | Content |
|---|---|
| `Zenith2/Engine/ZenithPlatform/Zenith_UpdateKeyRules.h` | the public constants and `consteval` predicates of `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Zenith_Ed25519 header" |
| `Zenith2/Engine/ZenithPlatform/Internal/Security/Zenith_Ed25519.h` | the internal declaration of `Zenith_Ed25519Verify` |
| `Zenith2/Engine/ZenithPlatform/Internal/Security/Zenith_Ed25519.cpp` | "Primitive algorithm"; the only TU that includes the admitted implementation's header |
| `Zenith2/Engine/ZenithPlatform/Tests/Zenith_Ed25519_Tests.cpp` | the primitive tests |
| `Zenith2/Engine/ZenithPlatform/Tests/Zenith_UpdateKeyRules_Tests.cpp` | the key-rule tests |
| `Zenith2/Engine/ZenithPlatform/Tests/Fuzz/Ed25519Verify.cpp`, `Ed25519Verify.dict` | the fuzz target |

PLAT owns the directory `Internal/Security/` and its verifier files; these files are SEC's, as PACK owns files in other targets (`Zenith2/Docs/Design/PACK.md` → "Files PACK owns in other targets"), and no PLAT ticket creates them (PLAT's row P29).

### Tools Security directory

`Zenith2/Tools/Security/` holds no compiled source (it is not a `zenith_add_tool` directory, so `SourceLayout` has nothing to own there; core-doc row X413 adds it to the repository layout):

| File | Content |
|---|---|
| `parser_inventory.json` | "Parser inventory registry" |
| `path_matrix.json` | "Path matrix registry" |
| `update_keys.json` | `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "update_keys.json" |
| `android_permissions.json` | `Zenith2/Docs/Design/SEC/ShippingExposure.md` → "Android manifest audit" |
| `Zenith2Security.psm1` | the module of "Zenith2Security module": the triage and `.hex` functions, which no gate runs |
| `New-Zenith2FuzzFindingTickets.ps1` | `Zenith2/Docs/Design/SEC/Fuzzing.md` → "Finding ticket body" |
| `Test-Zenith2InstallLayout.ps1` | `Zenith2/Docs/Design/SEC/TrustBoundaries.md` → "Distribution-layout audit" |
| `New-Zenith2UpdateKey.ps1`, `Ed25519KeyCeremony.java` | `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Key ceremony", "Ed25519KeyCeremony tool" |
| `Tests/Test-Zenith2Security.ps1`, `Tests/Fixtures/**` | `Zenith2/Docs/Design/SEC/Tests.md` → "PowerShell tests" (text fixtures only) |

### Gates Security directory

`Zenith2/Tools/Gates/Security/` is protected (Z2-D-128): PROC hand-off SEC-P9 creates it from this design, and no ordinary ticket edits it, because a gate's logic that an ordinary ticket could edit would let a worker weaken the gate while its fixtures stay green; the data the logic reads stays in `Zenith2/Tools/Security/` under `reviewPaths` (SEC-P8). The configure (`zenith_security_resolve_update_key`) and SHIP's release verification execute, and never edit, scripts of this directory, and the scripts import nothing from `Zenith2/Tools/Gates/Common/`; PROC records this configure-to-gate-directory edge as an exception to its rule SL9 that the gate directory is never part of the build, and adds the directory to its tree and to `Test-GateScriptsExist` (row SEC21):

| File | Content |
|---|---|
| `Zenith2SecurityGates.psm1` | the module of "Zenith2SecurityGates module" |
| `Test-Zenith2ShippingExposure.ps1` | `Zenith2/Docs/Design/SEC/ShippingExposure.md` → "Release-time audit tool" |
| `Test-Zenith2UpdateKeys.ps1` | the PowerShell half of `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "update_keys.json"; exit 0 valid, 1 a rule failed (first line the rule's message), 2 usage |
| `../Tests/Security/Test-Security.ps1`, `../Tests/Security/Fixtures/**` | `Zenith2/Docs/Design/SEC/Tests.md` → "Gate-step fixture tests"; the path matches PROC's `Tests/*/Test-*.ps1` discovery glob, so `Run-GateSelfTests.ps1` runs it |

### CMake files

| File | Content |
|---|---|
| `Zenith2/CMake/ZenithSecurity.cmake` | `zenith_security_resolve_update_key` (SHIP owns the directory; row SEC6 adds the ownership row); `zenith_tool_fuzz_target` and its wrapper are SHIP's test-registry functions (row SEC6 (g)) |
| `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` (SEC's block) | `include(ZenithSecurity)`, the `zenith_register_pwsh_test` and `zenith_register_script_test` calls of `Zenith2/Docs/Design/SEC/Tests.md` |
| `Zenith2/CMake/Tests/Security/UpdateKeyResolution.cmake`, `ToolFuzzTargets.cmake` | the CMake script-mode tests (the second written by SHIP's helper ticket) |

### Integration suite and benchmark

| Directory | Target |
|---|---|
| `Zenith2/Tests/Security/` | `Zenith_Integration_Security`: `CMakeLists.txt` with the one `zenith_add_integration_tests` call of `Zenith2/Docs/Design/SEC/Tests.md` → "Test targets and gate steps"; `Zenith_SecPath_Tests.cpp`, `Zenith_SecUpdate_Tests.cpp`, `Zenith_SecShipping_Tests.cpp`, `Zenith_SecScript_Tests.cpp`; header-only `Support/Zenith_SecFixtures.h` (archive, manifest and bundle builders over the documented layouts) |
| `Zenith2/Benchmarks/Ed25519/` | `Zenith_Bench_Ed25519` |

### Protected files SEC specifies

Each is created or changed by a PROC `needs-human` ticket (Z2-D-128) from the specification named; no ordinary SEC ticket edits it:

| File | Hand-off | Specification |
|---|---|---|
| `Zenith2/Tools/Gates/SecurityConformance.ps1`, `Tests/Test-SecurityConformance.ps1`, `Tests/SecurityConformance/**` | SEC-P1 | `Zenith2/Docs/Design/SEC/ConformanceCheck.md` |
| `Zenith2/Tools/Gates/Sanitizers.ps1` (smoke selection by the diff and the step budget, extending PROC's path selection) | SEC-P2 | `Zenith2/Docs/Design/SEC/Fuzzing.md` → "Smoke selection", "Smoke step budget" |
| `Zenith2/Tools/Gates/sanitizer_paths.json` and the `Zenith2Memory` paths in `zagent.project.json` | SEC-P3 | `Zenith2/Docs/Design/SEC/Fuzzing.md` → "Gate smoke" |
| `Zenith2/Tools/Gates/ForbiddenCheck.ps1` (rule F24), `confined_apis.json` | SEC-P4 | `Zenith2/Docs/Design/SEC/PathTraversal.md` → "Confined OS APIs" |
| `Zenith2/Tools/Gates/shipping_forbidden_symbols.json` (the consolidated entries, in PROC's format) | SEC-P5 | `Zenith2/Docs/Design/SEC/ShippingExposure.md` → "Forbidden symbol list" |
| `Zenith2/Tools/Gates/shipping_forbidden_strings.json`, the `Libgraph` step's string scan | SEC-P6 | `Zenith2/Docs/Design/SEC/ShippingExposure.md` → "Forbidden string list" |
| the `Libgraph` step's listener-import scan and positive controls | SEC-P7 | `Zenith2/Docs/Design/SEC/ShippingExposure.md` → "Listener-import scan", "Positive controls" |
| `zagent.project.json` (`reviewPaths` gains `Zenith2/Tools/Security/update_keys.json`, `parser_inventory.json`, `path_matrix.json` and `android_permissions.json`) | SEC-P8 | `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "update_keys.json" |
| `Zenith2/Tools/Gates/Security/**` and `Tests/Security/Test-Security.ps1`, `Tests/Security/Fixtures/**` | SEC-P9 | `Zenith2/Docs/Design/SEC.md` → "Gates Security directory" |

### ZENITH_SUPPORTED matrix

SEC's ZenithPlatform files take ZenithPlatform's `ZENITH_ALL`; `Zenith_Fuzz_Ed25519Verify` exists in `win-asan` only; `Zenith_Integration_Security` declares no `SUPPORTED` and takes `ZENITH_HOST` narrowed by its `DEPENDS` (all `ZENITH_ALL`), so it exists in `win`, `win-asan` and `linux-tsan`, and in `win-shipping-test` through the `ShippingTest` configuration; `Zenith_Bench_Ed25519` takes the benchmark default.

## Public API

### API map

| Header or tool | Declares | Consumers |
|---|---|---|
| `ZenithPlatform/Zenith_UpdateKeyRules.h` | `uZENITH_UPDATE_KEY_BYTES`, `uZENITH_PUBLISHED_TEST_KEY_COUNT`, `aauZENITH_PUBLISHED_TEST_KEYS`, `Zenith_IsPublishedTestUpdateKey`, `Zenith_IsZeroUpdateKey` | MOD's generated composition TU; SEC's tests |
| `ZenithPlatform/Internal/Security/Zenith_Ed25519.h` | `Zenith_Ed25519Verify`, `uED25519_PUBLIC_KEY_BYTES`, `uED25519_SIGNATURE_BYTES`, `ulED25519_MAX_MESSAGE_BYTES` | PLAT's verifier; ZenithPlatform's tests |
| `Zenith2/CMake/ZenithSecurity.cmake` | `zenith_security_resolve_update_key` | MOD's `zenith_add_game` |
| SHIP's test registry | `zenith_tool_fuzz_target`, `zenith_cook_fuzz_target` (SEC's specification) | areas' registry blocks for tool-side parsers |
| `Zenith2/Tools/Gates/Security/Zenith2SecurityGates.psm1` (protected) | the readers and matchers below | the protected steps, SHIP's release verification, SEC's pwsh tests |
| `Zenith2/Tools/Security/Zenith2Security.psm1` | the triage functions below | SEC's tools |

### Zenith_Ed25519 and Zenith_UpdateKeyRules

The declarations, their contract blocks and the algorithm are `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Zenith_Ed25519 header", "Primitive algorithm"; they are the only C++ API SEC adds.

### CMake functions

```cmake
zenith_security_resolve_update_key(<Game> <ReleaseLine> <OutShippingInc> <OutDevelopmentInc>)
zenith_tool_fuzz_target(<Stem> TOOL <ToolTarget> TOOL_DIR <directory> SOURCES <path>... DEPENDS <Library>... [THIRD_PARTY <ManifestName>...])
zenith_cook_fuzz_target(<Stem> SOURCES <path>... DEPENDS <Library>... [THIRD_PARTY <ManifestName>...])
```

Their algorithms are `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Key resolution at configure" and `Zenith2/Docs/Design/SEC/Fuzzing.md` → "Tool-side fuzz targets".

### Zenith2SecurityGates module

`Zenith2/Tools/Gates/Security/Zenith2SecurityGates.psm1` (protected, SEC-P9) exports, each with `Set-StrictMode -Version Latest` and optional fields read through `PSObject.Properties[…]`:

| Function | Parameters | Returns |
|---|---|---|
| `Read-Zenith2ParserInventory` | `-Path <file>` | the registry object; throws a terminating error naming the field when the schema is violated |
| `Read-Zenith2PathMatrix` | `-Path <file>` | the matrix object, likewise |
| `Read-Zenith2UpdateKeys` | `-Path <file>` | the key records, after invoking `Test-Zenith2UpdateKeys.ps1` on the file (the one implementation of every rule of "update_keys.json"; a failure is a terminating error with the script's first line) |
| `ConvertTo-Zenith2NormalisedSymbol` | `-Demangled <string>` | the normalised qualified name ("Symbol normalisation and matching") |
| `Find-Zenith2ForbiddenSymbol` | `-Names <string[]> -List <object>` | the matching entries, as `,@(…)` so a single match stays an array |
| `Find-Zenith2ForbiddenString` | `-Path <file> -List <object>` | `{ id, offset, encoding }` records |
| `Find-Zenith2ListenerImport` | `-ImportListing <string[]> -Platform windows\|android` | `{ symbol, library }` records |

### Zenith2Security module

`Zenith2/Tools/Security/Zenith2Security.psm1` exports the functions of the campaign triage, which no gate runs, under the same conventions:

| Function | Parameters | Returns |
|---|---|---|
| `ConvertTo-Zenith2HexRegression` | `-Bytes <byte[]>` | the `.hex` text of rule C5 |
| `ConvertFrom-Zenith2HexRegression` | `-Text <string>` | the bytes; refuses any character outside `[0-9a-f\n]` |
| `Get-Zenith2FuzzDedupKey` | `-Finding <object> -Stem <string>` | the key of "Deduplication key" |
| `Get-Zenith2FuzzSeverity` | `-Kind <string> -Tier <int>` | the severity string |

### Release-time and acceptance tools

| Tool | Command line | Specification |
|---|---|---|
| `Test-Zenith2ShippingExposure.ps1` (protected, `Zenith2/Tools/Gates/Security/`) | `-Image -Symbols -Platform [-PositiveControl] [-SymbolList] [-StringList] [-Out]` | `Zenith2/Docs/Design/SEC/ShippingExposure.md` → "Test-Zenith2ShippingExposure command line" |
| `Test-Zenith2UpdateKeys.ps1` (protected, `Zenith2/Tools/Gates/Security/`) | `-Path <update_keys.json>`; exit 0 valid, 1 a rule failed, 2 usage | `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "update_keys.json" |
| `New-Zenith2FuzzFindingTickets.ps1` | `-Campaign <campaign.json> -Inventory <parser_inventory.json> -Out <dir>`; exit 0 written, 1 a malformed campaign, 2 usage | `Zenith2/Docs/Design/SEC/Fuzzing.md` → "Finding ticket body" |
| `Test-Zenith2InstallLayout.ps1` | `-InstallRoot <dir> -Platform windows\|android [-Package <name>] [-Out <report.json>]`; exit 0 L1 to L3 pass, 1 a failure, 2 usage | `Zenith2/Docs/Design/SEC/TrustBoundaries.md` → "Distribution-layout audit" |
| `New-Zenith2UpdateKey.ps1` | `-Game -ReleaseLine -PasswordEnv -Out`; exit codes of the Java tool | `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Key ceremony" |

### Handle kinds used

None. SEC's C++ code takes byte spans and returns a value.

## Data structures and formats

### Format index

| File | Schema | Owner | Specified in |
|---|---|---|---|
| `parser_inventory.json` | `zenith2.parser-inventory` 1 | SEC | "Parser inventory registry" |
| `path_matrix.json` | `zenith2.path-matrix` 1 | SEC | "Path matrix registry" |
| `update_keys.json` | `zenith2.update-keys` 1 | SEC | `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "update_keys.json" |
| `android_permissions.json` | `zenith2.android-permissions` 1 | SEC | `Zenith2/Docs/Design/SEC/ShippingExposure.md` → "Android manifest audit" |
| `shipping_forbidden_symbols.json` | `zenith2.shipping-forbidden-symbols` 1 | PROC's format, SEC's content (protected) | `Zenith2/Docs/Design/SEC/ShippingExposure.md` → "shipping_forbidden_symbols.json schema" |
| `shipping_forbidden_strings.json` | `zenith2.shipping-forbidden-strings` 1 | SEC (protected) | `Zenith2/Docs/Design/SEC/ShippingExposure.md` → "shipping_forbidden_strings.json schema" |
| `confined_apis.json` | `zenith2.confined-apis` 1 | SEC (protected) | `Zenith2/Docs/Design/SEC/PathTraversal.md` → "confined_apis.json" |
| the exposure report | `zenith2.shipping-exposure` 1 | SEC | `Zenith2/Docs/Design/SEC/ShippingExposure.md` → "Audit report" |
| the conformance report | `zenith2.security-conformance` 1 | SEC | `Zenith2/Docs/Design/SEC/ConformanceCheck.md` → "Report file" |
| `findings.json` | `zenith2.fuzz-findings` 1 | SEC | `Zenith2/Docs/Design/SEC/Fuzzing.md` → "Finding ticket body" |
| `.hex` regressions | rule C5 | SEC | `Zenith2/Docs/Design/SEC/Fuzzing.md` → "Corpus and dictionary rules" |
| the Ed25519 harness report | `zenith2.bench-ed25519` 1 | SEC | `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Selection harness" |
| the encrypted key and its public record | PKCS#8 PBES2 PEM; `update-ed25519.pub.json` | SEC | `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Ed25519KeyCeremony tool" |

Every SEC JSON file is UTF-8 without a byte-order mark, LF endings, four-space indentation (never tabs, as in every JSON example of this design), keys in the order shown, arrays sorted by their key field, so a one-entry change is a one-object diff.

### Parser inventory registry

```json
{
    "schema": "zenith2.parser-inventory",
    "schemaVersion": 1,
    "durations": {
        "stage2to5": { "1": 600, "2": 180, "3": 60 },
        "stage6": { "1": 3600, "2": 1800, "3": 600 }
    },
    "configKeyAllow": [],
    "parsers": [
        {
            "id": "PI-PLAT-001",
            "parser": "Zenith_ParseVirtualPath",
            "library": "ZenithPlatform",
            "sources": ["Zenith2/Engine/ZenithPlatform/Internal/Zenith_VirtualPath.cpp"],
            "boundary": ["shipped-content", "user-files", "scripts", "network"],
            "tier": 1,
            "limitsRows": [],
            "limitsDesign": "Zenith2/Docs/Architecture/PlatformLayer.md → \"Path validation\"",
            "constants": ["uVIRTUAL_PATH_MAX_BYTES", "uVIRTUAL_PATH_MAX_COMPONENT_BYTES", "uVIRTUAL_PATH_MAX_COMPONENTS"],
            "limitTests": ["Test_VirtualPath_EachRuleRejects"],
            "fuzz": { "stem": "VirtualPath", "maxLen": 4096, "smokeAlso": ["ZenithCore"], "dict": false, "ubsan": true },
            "owner": ["Z2T-PLAT-007"],
            "status": ["active", "limits-in-design"],
            "finding": null,
            "covered": [],
            "acceptedFindings": []
        }
    ]
}
```

| Field | Rule |
|---|---|
| `durations` | seconds per tier for the two campaign kinds (`Zenith2/Docs/Design/SEC/Fuzzing.md` → "Stage acceptance campaigns") |
| `configKeyAllow` | `{ key, decision, why }` records exempting a configuration key from SC15 |
| `parsers[].id` to `owner` | the columns of `Zenith2/Docs/Design/SEC/ParserInventory.md` → "Entry columns"; `sources` lists the parser's implementation files (a finding ticket edits them) |
| `limitsRows` | the first cells of the Architecture rows that hold the parser's limits, backticks removed, in table order; empty only for a `property` entry or one whose status carries `pending-row` or `limits-in-design` |
| `limitsDesign` | a `path → "heading"` citation; required when `limitsRows` is empty and the entry is not `property`, optional beside rows |
| `constants`, `limitTests` | the names SC05 and SC06 look for; empty only for `property` entries |
| `fuzz` | null for `covered`, `outside-scope` and stemless `gap` entries; otherwise `stem`, `maxLen` (default 65,536), `smokeAlso`, whether a dictionary exists, and `ubsan`: `true` exactly when the owning library is built in `linux-tsan` (per `declared_deps.json` `supported`), so `Test_<Stem>_Regressions` also replays under UBSan (`Zenith2/Docs/Design/SEC/Fuzzing.md` → "linux-tsan replay with UBSan") |
| `status` | one or more values of "Entry columns" |
| `finding` | `F1` to `F8` when `status` holds `gap`, `naming` or `dead-target`, else null |
| `covered` | the IDs that cover a `covered` entry |
| `acceptedFindings` | `{ key, decision }` of findings a ruling accepted |

### Path matrix registry

```json
{
    "schema": "zenith2.path-matrix",
    "schemaVersion": 1,
    "sources": [
        {
            "id": "P3",
            "cells": [
                { "vectors": ["V1", "V2", "V6"], "tests": ["Test_AssetArchive_EachCheckRejects", "Test_SecPath_ArchiveEntryTraversalRefused"] },
                { "vectors": ["V11", "V16"], "notApplicable": "entries are table names, no OS path" }
            ]
        },
        {
            "id": "P16",
            "cells": [
                { "vectors": ["V1", "V3"], "tests": ["Test_GltfSource_UriEscapeRefused"], "pending": "SEC9" }
            ]
        }
    ]
}
```

Each source of `Zenith2/Docs/Design/SEC/PathTraversal.md` → "Source catalogue" has one record; every vector V1 to V19 appears in exactly one cell of every runtime source (V12 as the `notApplicable` cell "residual R1" in each) (P1 to P7, P13 to P15, P18), either with tests or with `notApplicable`; `pending` names the cross-area row that adds a test not yet specified by its owner.

### Other SEC files

The remaining formats are specified where the format index points; none of them is read by a Player, and none is a cooked format, so none enters PACK's binary format registry.

## Algorithms and state machines

### Algorithm index

| Algorithm | Complexity | Specified in |
|---|---|---|
| update verification rules V1 to V9 | linear in the manifest and the files hashed | `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Rule order" |
| Ed25519 verification | one double-scalar multiplication plus SHA-512 over the message | `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Primitive algorithm" |
| update-key resolution at configure | linear in the key records | `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Key resolution at configure" |
| SecurityConformance checks SC01 to SC16 | linear in the tracked and untracked source bytes plus the registries | `Zenith2/Docs/Design/SEC/ConformanceCheck.md` → "Checks" |
| tool-side fuzz target creation | constant per call | `Zenith2/Docs/Design/SEC/Fuzzing.md` → "Tool-side fuzz targets" |
| fuzz smoke selection and duration | linear in the changed paths times the targets | `Zenith2/Docs/Design/SEC/Fuzzing.md` → "Smoke selection", "Smoke step budget" |
| finding triage: deduplication and severity | linear in the findings | `Zenith2/Docs/Design/SEC/Fuzzing.md` → "Deduplication key", "Severity mapping" |
| shipping exposure audit | linear in the image size and the list sizes | `Zenith2/Docs/Design/SEC/ShippingExposure.md` → "Audit algorithm" |
| distribution-layout audit | linear in the ACL entries | `Zenith2/Docs/Design/SEC/TrustBoundaries.md` → "Distribution-layout audit" |
| key ceremony | constant | `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Ed25519KeyCeremony tool" |

### State machine index

| Machine | Specified in |
|---|---|
| finding lifecycle (NEW, TRIAGED, DUPLICATE, DISCARDED, FILED, RECORDED, FIXED, VERIFIED, ACCEPTED) | `Zenith2/Docs/Design/SEC/Fuzzing.md` → "Finding lifecycle" |
| update key states (GENERATED, ACTIVE, RETIRED, REVOKED) | `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Key states" |

## Threading lifetime and error contract

### Thread affinity

`Zenith_Ed25519Verify` is ANY_THREAD and reentrant: PLAT's patch mount calls it on the main thread at startup step 5, PACK's applier at the closing action of step 4 and PACK's DLC mounting at prologues; it holds no state. The tools are single-threaded scripts.

### Frame-graph placement

None. SEC's runtime code runs only inside the callers' startup and prologue steps; it adds no node, no access declaration and no frame cost.

### Lock-order entries

None: nothing SEC adds takes a lock.

### Shutdown

Nothing to shut down: the primitive has no state, and the admitted implementation has none (criterion K5).

### Error reporting

| Producer | Error form |
|---|---|
| `Zenith_Ed25519Verify` | `bool`; the caller maps false to its `Zenith_UpdateVerifyError` (PLAT) |
| CMake functions | `message(FATAL_ERROR "Zenith2: …")` with the messages of their algorithms; a missing active key is a `WARNING` |
| SecurityConformance | exit 0, 1, 2 and the `SCNN` messages |
| PowerShell tools | the exit codes of "Release-time and acceptance tools"; terminating errors naming the field for schema violations |

SEC defines no `Zenith_Result` error enumeration of its own: every runtime failure it specifies surfaces through the caller's enumeration (PLAT's `Zenith_UpdateVerifyError`, PACK's `Zenith_PackageError`, `Zenith_PathError`).

### Luau errors versus asserts

SEC exposes nothing to Luau, so it raises no Luau error and reserves no misuse-code block. `Zenith_Ed25519Verify` never asserts on its inputs; its callers' untrusted-input rule is [Z2-N-CONTRACT-472].

## Diagnostics and profiling

### Counters SEC relies on

SEC adds no runtime counter. The security-relevant counters it cites, which the on-screen profiler and the evidence read from their owners:

| Counter | Owner | Meaning |
|---|---|---|
| `vfs.patches_rejected` | PLAT | patch candidates refused at startup |
| `vfs.patch.stamp_skips` | PLAT | patch files trusted by stamp |
| `package.verify.cache_hits` | PACK | pack archives and DLC files trusted by stamp |
| `package.install.stamp_write_failures` | PACK | best-effort stamp writes that failed |
| `devtools.listener.lockouts` | PLAT (rule T4, row SEC2) | listeners paused after repeated handshake failures |
| `editor.liveLink.rejected` | EDIT | live-link handshakes refused |

### Log lines

| Line | Emitted by |
|---|---|
| `patch rejected: <version>: <rule> (<U-check> <reason>)` | PLAT's patch mount (rule tags of "Rejection and logging") |
| `devtools: <service> listening on <address>:<port> beyond loopback` | the development listener (rule T6) |
| `Zenith2: <Game> <ReleaseLine>: no active update key; Shipping builds refuse every patch and DLC` | the configure |
| `Sanitizers: fuzz smoke shortened to <s> s for <N> targets` | the `Sanitizers` step |

### Gate and tool reports

`SecurityConformance.json`, `zenith2.shipping-exposure` reports, `findings.json`, DEVT's `campaign.json`, and the distribution-layout reports, each described where the format index points.

### Telemetry

None: no security event is sent anywhere; failures are local log lines and counters.

## Authoring and cooking

### What SEC authors

The registries (`parser_inventory.json`, `path_matrix.json`, `update_keys.json`, `android_permissions.json`) are hand-edited JSON in the ticket that changes what they describe; there is no editor surface. The protected lists are authored by PROC tickets.

### Cook and release integration

- The cook's validation is the first line of boundary B1 (`Zenith2/Docs/Design/ASSET/Cook.md` → "Validation"); SEC adds no cook step.
- The configure resolves the update key per game and release line; the release pipeline (SHIP) signs manifests through PACK's signer, runs the exposure audit on every packaged image and checks the compiled key (W10).
- The key ceremony is a person's procedure (`Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Key ceremony").

### Editor surfaces

None.

### Migration hooks

None: SEC's files are versioned by `schemaVersion`, and a version change is a diff that migrates every committed file in place; no SEC file is an authoring document under `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind".

## Failure handling

### Failure table

| Failure | Behaviour |
|---|---|
| a manifest fails any of V1 to V9 | the whole update is rejected, previous content stays mounted, the rule is logged ([Z2-N-ARCH-961]) |
| a Shipping build has no active update key | the zero key: every patch is rejected, every DLC unmounted, the base game runs |
| a published test key reaches a Shipping configuration | the configure fails and the static assertion fails the compile |
| `update_keys.json` is malformed | the configure fails naming the rule; SC14 fails the gate |
| the admitted Ed25519 entry fails to build in a preset | the build fails; no fallback verifier exists |
| a fuzz smoke finds a defect in a ticket's gate | the ticket's `Sanitizers` step fails; the ticket fixes it ("Regression rule") |
| a campaign finds a defect | the triage tool writes the recording and fix bodies; the stage acceptance waits ("Regression rule") |
| a new parser lacks an inventory entry, a constant, a limit test or a fuzz source | SecurityConformance fails naming the check |
| a development symbol, string or listener import survives into a Shipping image | the `Libgraph` step fails; the release verification refuses the package |
| a positive control is missing | the scan is reported broken and fails |
| a distribution layout fails L1 | the layout is blocked until the keyed-record escalation lands |
| a key is compromised | "Compromise response" |
| allocation failure inside a parser on hostile input | a `Zenith_Result` error of that parser (H5); a fuzz finding of kind `oom` otherwise |
| device loss, low disk, cancellation | not applicable to SEC's code; the callers' designs cover them |
| budget breach of a campaign wall-time row | reported (BR-REPORT); the human may lengthen the window or run the campaign on a second machine |

## Test strategy

### Test summary

| Group | Tests | Specified in |
|---|---|---|
| primitive | `Test_Ed25519_*`, `Test_UpdateKeyRules_PublishedKeysDetected`, `Zenith_Fuzz_Ed25519Verify` | `Zenith2/Docs/Design/SEC/Tests.md` → "Ed25519 primitive tests" |
| path composition | `Test_SecPath_*`, `Test_SecScript_*` | `Zenith2/Docs/Design/SEC/Tests.md` → "Path-matrix composition tests" |
| update tamper | `Test_SecUpdate_*` | `Zenith2/Docs/Design/SEC/Tests.md` → "Update tamper tests" |
| Shipping behaviour | `Test_SecShipping_*` | `Zenith2/Docs/Design/SEC/Tests.md` → "Shipping behaviour tests" |
| tooling | `Pwsh_Sec_*`, `CMake_Sec_*` | `Zenith2/Docs/Design/SEC/Tests.md` → "PowerShell tests", "CMake script-mode tests" |
| protected step | `SecConformance_*` | `Zenith2/Docs/Design/SEC/Tests.md` → "Gate-step fixture tests" |
| acceptance | the campaigns, HWASan replays, exposure audits, tamper suite, layout audits, key records | `Zenith2/Docs/Design/SEC/Tests.md` → "Acceptance items" |

### Which gate step runs each group

`Test` runs the primitive tests, the integration suite (`win`, `win-shipping-test`), the PowerShell and CMake tests; `Sanitizers -Preset win-asan` runs the fuzz smoke and the suite under ASan; `Sanitizers -Preset linux-tsan` runs the suite and the primitive tests under TSan and UBSan; `SecurityConformance` runs from Stage 2 (SEC-P1); `Libgraph`'s Shipping check runs the exposure scans (SEC-P5 to SEC-P7); stage acceptance runs the acceptance items.

## Budget rows

### Rows SEC is measured against

| Row | Why |
|---|---|
| Z2-B-PROC-025 (Step timeout Sanitizers win-asan, pending) | the fuzz smokes run inside it; its producer sizes it by "Smoke step budget" |
| Z2-B-PROC-026 (Step timeout Sanitizers linux-tsan, pending) | the replay tests run inside it |
| Z2-B-PROC-006 (Acceptance phase timeout, pending) | the acceptance runs that include SEC's suites |
| Z2-B-SHIP-013, Z2-B-SHIP-014 (cold boot Windows, S24) | V1 to V6 and V9's stamp checks run at every start; their cost is part of W-Load and of PACK's pending row "Cold-boot update verification time" |

### Pending new rows

Requested from `Zenith2/Docs/Budgets.md` through core-doc row X415:

| Proposed row | Name | Unit | Procedure | Platform | Breach | Producer |
|---|---|---|---|---|---|---|
| proposed S1 | Fuzz campaign wall time Stages 2 to 5 | h | the campaign runner's summary, every target at its tier's Stage 2 to 5 duration with `-Jobs 8` | Win | BR-REPORT | the SEC Stage 1 security-architecture ruling ticket |
| proposed S2 | Fuzz campaign wall time Stage 6 | h | as S1 at the Stage 6 durations | Win | BR-REPORT | the SEC Stage 1 security-architecture ruling ticket |
| proposed S3 | Step timeout SecurityConformance | min | the step's wall time on a cold tree | Win | the step's hard timeout | the Stage 2 PROC ticket landing SecurityConformance |

## Stage plan

### Stage 1

- The Stage 1 security-architecture ruling ticket (`needs-human`, kind `decision`; Roadmap Stage 1 deliverable 15). It rules: the trust-boundary model and its tiers; the parser-limit conformance rule and the inventory; the fuzz tiers and durations; R1 to R10, the escape check and the R10 extension; the confined-API rule; the exposure layers; the verification rules V1 to V9; the key model (one key per game and release line, `update_keys.json`, fail-closed defaults); the three verified-cache confirmation items; the remote-tool session rules; and the Ed25519 implementation by "Ed25519 implementation selection", from the harness's report. It appends its `Z2-D-NNN` entry to `Zenith2/Docs/DecisionLog.md` in its own diff (Z2-D-097) and blocks PLAT's revalidation (row PB6) and SHIP's Ed25519 consumer (row ShB8).
- The Ed25519 selection harness (ordinary harness ticket, Z2-D-084), blocking the ruling, and itself blocked by SHIP's `needs-human` pin ticket for `monocypher` 4.0.2 and `libsodium` 1.0.20 (row SEC15), without which the harness's `THIRD_PARTY` names fail every configure.
- The two pending campaign rows are allocated by the ruling.

### Stage 2

- The update-key chain, so that SecurityConformance check SC14 and every game's configure are green from their first run, carrying only the published TEST 1 key (development) and the zero key (Shipping): `update_keys.json` committed with `"keys": []`, `Zenith_UpdateKeyRules.h` and its tests, `zenith_security_resolve_update_key` with `CMake_Sec_UpdateKeyResolution` and `CMake_Sec_PublishedKeysAgree`, and the update-key half of SEC-P9 (`Test-Zenith2UpdateKeys.ps1`). The ticket blocks MOD's `zenith_add_game` and composition-table change (row SEC5 (a)); none of it needs Ed25519. Real keys arrive with each release line's ceremony at Stage 6.
- `parser_inventory.json` and `path_matrix.json` committed with every row of `Zenith2/Docs/Design/SEC/ParserInventory.md` and `Zenith2/Docs/Design/SEC/PathTraversal.md`; the readers of `Zenith2SecurityGates.psm1` (SEC-P9) and `Pwsh_Sec_ParserInventory_CommittedFileValid`.
- SHIP's `zenith_tool_fuzz_target`, the `zenith_cook_fuzz_target` wrapper over it, the `Tests/Fuzz/**` exclusion and `CMake_Sec_ToolFuzzTargetDeclarations` (row SEC6 (g)), before or in `Z2T-ASSET-035`, whose call sites keep their form.
- PROC hand-offs SEC-P1 (SecurityConformance), SEC-P2 (smoke selection), SEC-P3 (`Zenith2Memory` paths), SEC-P4 (rule F24) and the reader half of SEC-P9, landing with the Stage 2 sanitizer activation.
- `Zenith_Integration_Security` with the path composition tests whose components exist in Stage 2 (PLAT, ASSET, SAVE, SCRIPT's manifest at Stage 3 joins then).
- The first campaign at the Stage 2 acceptance, and the triage tool with its tests.

### Stage 3

- The exposure audit: `Test-Zenith2ShippingExposure.ps1` and its tests (the scan half of SEC-P9); SEC-P5 to SEC-P7; the `Test_SecShipping_*` cases; CORE's `Zenith_FuzzRegression.h` (row SEC18) and MOD's push of the fuzz directories (row SEC5 (d)) before the HWASan replay at the Stage 3 acceptance.
- The remote-tool session rules applied by PLAT and EDIT (row SEC2).

### Stage 4

- The ONLINE confirmations and their inventory rows; the CONTENT and authoring rows' tool fuzz targets (rows SEC4, SEC9).

### Stage 6

- The `needs-human` check of the three RFC 8032 § 7.1 vectors against the RFC's text (ruling SEC-R1-RULINGS R11; `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Published test keys"), before the API-only slice.
- `Zenith_Ed25519Verify` and its tests (sprint `Stage 5 — Rendering and simulation completion`, fix version `Z2 Stage 6`, before PLAT's Z2T-PLAT-092, which it blocks: rows P29 (b) and SEC16), SEC-P8.
- The tamper suite; the key ceremony tool and `Pwsh_Sec_KeyCeremony_RoundTrip`; `Test-Zenith2InstallLayout.ps1`.
- The `needs-human` key ceremony of every shipped (game, release line), the distribution-layout audit, the Stage 6 campaign and the exposure audit of every packaged image: the security hardening evidence ([Z2-N-ARCH-960]).

### Stage 7

Networked multiplayer's boundary (B6 beyond loopback) is re-ruled by the Stage 7 NET design; SEC has no Stage 7 requirement of its own.

### API-only slices

`Internal/Security/Zenith_Ed25519.h` lands as an API-only ticket in the Stage 5 sprint, so PLAT's verifier ticket can compile against it while the implementation lands; `Zenith_UpdateKeyRules.h` has already landed with the Stage 2 update-key chain.

## Decisions

### Settled decisions governing SEC

| Decision | How it governs SEC |
|---|---|
| Z2-D-010 | the platforms the audits and the HWASan replay cover |
| Z2-D-011 | `win-asan` with libFuzzer, `linux-tsan` with UBSan, HWASan on the S24 |
| Z2-D-016, Z2-D-017 | the script boundary and the exception firewall |
| Z2-D-022 | SEC's challenges to settled choices go through flagged proposals |
| Z2-D-024, Z2-D-075 | no dynamic plugins, no modding: no third-party code boundary |
| Z2-D-027 | Play distribution and installed updates; no downloader of the programme's own |
| Z2-D-028 | the 60-second smoke within the gate budget |
| Z2-D-038 | versioned binary formats with parser limits |
| Z2-D-042 | the hard gates SEC's scans extend |
| Z2-D-045 | results, never asserts, on untrusted input |
| Z2-D-046 | the primitive holds no state |
| Z2-D-065 | evidence fingerprints for the campaign and audit records |
| Z2-D-073, Z2-D-074 | network and online boundaries in scope as foundations and adapters |
| Z2-D-082 | the Ed25519 entry pinned and confined to `Internal/` |
| Z2-D-083 | acceptance runs outside the loop |
| Z2-D-084 | the selection split into a harness and a ruling |
| Z2-D-097 | the ruling appended to the decision log in its own diff |
| Z2-D-128 | every protected edit is a PROC `needs-human` ticket |

### Open decisions SEC depends on

- D-SEL-JSON (open, Stage 1): the JSON reader backend; its acceptance includes parser-limit enforcement, which SEC's inventory row PI-CORE-001 relies on.
- D-SEL-CRASH (open, Stage 2): the crash realisation whose IPC is boundary B9.
- D-LUAUDBG (open, Stage 1): the DAP client; the server's exposure rules stand whichever client is chosen.

### Selection decisions SEC owns

The Ed25519 implementation, ruled inside the Stage 1 security-architecture ruling with the candidates, criteria and acceptance rule of `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Ed25519 implementation selection".

### Design rulings SEC records

These are the elaborations a reviewer judges first; the Stage 1 ruling confirms each:

| # | Ruling | Where |
|---|---|---|
| S-R1 | nine boundaries, two beyond the Architecture table (tool inputs, process IPC) | `Zenith2/Docs/Design/SEC/TrustBoundaries.md` → "Boundary model" |
| S-R2 | three tiers, set by boundary class, with per-tier campaign durations | `Zenith2/Docs/Design/SEC/Fuzzing.md` → "Stage acceptance campaigns" |
| S-R3 | a cooked-format reader is class `shipped-content` even behind a patch, because it reads only authenticated bytes | `Zenith2/Docs/Design/SEC/ParserInventory.md` → "Asset and geometry parsers" |
| S-R4 | regression inputs as `.hex` text, recorded by a `needs-human` ticket before the fix | `Zenith2/Docs/Design/SEC/Fuzzing.md` → "Corpus and dictionary rules", "Finding ticket body" |
| S-R5 | one tool-side fuzz helper, `zenith_tool_fuzz_target` (SHIP's, with ASSET's `zenith_cook_fuzz_target` as its wrapper), builds tool-side parsers in `win-asan` | `Zenith2/Docs/Design/SEC/Fuzzing.md` → "Tool-side fuzz targets" |
| S-R6 | one update key per game and release line for the line's whole life, committed publicly, resolved at configure, fail-closed in Shipping; a key replacement starts a new release line | `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Public key provenance" |
| S-R7 | the canonical-S check in the wrapper, whatever implementation is admitted | `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Primitive algorithm" |
| S-R8 | a string scan and a listener-import scan beside the symbol scan, each with a positive control | `Zenith2/Docs/Design/SEC/ShippingExposure.md` → "Audit layers" |
| S-R9 | the confined-API rule F24 | `Zenith2/Docs/Design/SEC/PathTraversal.md` → "Confined OS APIs" |
| S-R10 | the keyed-record escalation designed and dormant, triggered by the layout audit | `Zenith2/Docs/Design/SEC/TrustBoundaries.md` → "Keyed-record escalation" |
| S-R11 | the R10 extension for `crash` and `devtools` | `Zenith2/Docs/Design/SEC/PathTraversal.md` → "Rules confirmed by the ruling" |
| S-R12 | a gate's scan logic lives under the protected `Zenith2/Tools/Gates/**`; only data stays under `Zenith2/Tools/Security/` | `Zenith2/Docs/Design/SEC.md` → "Gates Security directory" |

## Requirement index

Every requirement of `Zenith2/Docs/Requirements/SEC.md` and the design headings that specify it:

| Requirement | Title | Stage | Design (paths relative to `Zenith2/Docs/Design/`) |
|---|---|---|---|
| Z2-R-SEC-001 | Stage 1 security-architecture ruling | S1 | SEC.md → "Stage 1"; SEC.md → "Design rulings SEC records" |
| Z2-R-SEC-002 | Nine trust boundaries with complete records | S1 | SEC/TrustBoundaries.md → "Boundary record fields"; SEC/TrustBoundaries.md → "The nine boundaries" |
| Z2-R-SEC-003 | Boundary classes and tiers | S1 | SEC/TrustBoundaries.md → "Boundary classes and tiers"; SEC/ParserInventory.md → "Boundary classes and tiers used here" |
| Z2-R-SEC-004 | Shipped content boundary | S2 | SEC/TrustBoundaries.md → "B1 shipped content" |
| Z2-R-SEC-005 | Downloaded content authenticated before mounting | S6 | SEC/TrustBoundaries.md → "B2 patches and downloaded content"; SEC/UpdateAuthentication.md → "Rejection and logging" |
| Z2-R-SEC-006 | User files untrusted and never executable | S2 | SEC/TrustBoundaries.md → "B3 user files" |
| Z2-R-SEC-007 | Saves carry no integrity claim | S3 | SEC/TrustBoundaries.md → "Saves and integrity" |
| Z2-R-SEC-008 | Script bytecode only from authenticated mounts | S3 | SEC/TrustBoundaries.md → "B4 scripts" |
| Z2-R-SEC-009 | Remote tools development-only and loopback | S3 | SEC/TrustBoundaries.md → "B5 remote tools"; SEC/TrustBoundaries.md → "Remote-tool session rules" |
| Z2-R-SEC-010 | Session token entropy and secrecy | S3 | SEC/TrustBoundaries.md → "Remote-tool session rules" |
| Z2-R-SEC-011 | Constant-time token comparison | S3 | SEC/TrustBoundaries.md → "Remote-tool session rules" |
| Z2-R-SEC-012 | Handshake deadline and lockout | S3 | SEC/TrustBoundaries.md → "Remote-tool session rules" |
| Z2-R-SEC-013 | Non-loopback bind warning | S3 | SEC/TrustBoundaries.md → "Remote-tool session rules"; SEC.md → "Log lines" |
| Z2-R-SEC-014 | Network boundary for the loopback proof | S3 | SEC/TrustBoundaries.md → "B6 network" |
| Z2-R-SEC-015 | Online services boundary confirmed | S4 | SEC/TrustBoundaries.md → "Online-services confirmation"; SEC/TrustBoundaries.md → "B7 online services" |
| Z2-R-SEC-016 | Tool inputs never reach a Player | S3 | SEC/TrustBoundaries.md → "B8 tool inputs" |
| Z2-R-SEC-017 | Process IPC trusts inherited handles only | S3 | SEC/TrustBoundaries.md → "B9 process IPC" |
| Z2-R-SEC-018 | Install-root and device preconditions | S1 | SEC/TrustBoundaries.md → "Install-root precondition"; SEC/TrustBoundaries.md → "Device precondition"; SEC/TrustBoundaries.md → "Same-user precondition" |
| Z2-R-SEC-019 | Verified-cache stamp set confirmed | S1 | SEC/TrustBoundaries.md → "Stamp set confirmed" |
| Z2-R-SEC-020 | Verified-cache records unauthenticated | S1 | SEC/TrustBoundaries.md → "Records unauthenticated" |
| Z2-R-SEC-021 | Keyed-record escalation | S6 | SEC/TrustBoundaries.md → "Keyed-record escalation" |
| Z2-R-SEC-022 | Distribution-layout audit | S6 | SEC/TrustBoundaries.md → "Distribution-layout audit" |
| Z2-R-SEC-023 | Accepted residuals recorded | S1 | SEC/TrustBoundaries.md → "Accepted residual risks"; SEC/TrustBoundaries.md → "Non-goals" |
| Z2-R-SEC-024 | One parser inventory | S2 | SEC/ParserInventory.md → "Inventory rules"; SEC/ParserInventory.md → "Entry columns"; SEC.md → "Parser inventory registry" |
| Z2-R-SEC-025 | Parser inventory registry format | S2 | SEC.md → "Parser inventory registry" |
| Z2-R-SEC-026 | Every inventory entry names an owner | S2 | SEC/ParserInventory.md → "Inventory rules" |
| Z2-R-SEC-027 | Limits located in one place | S2 | SEC/ParserInventory.md → "Inventory rules"; SEC/ConformanceCheck.md → "Checks" |
| Z2-R-SEC-028 | Limit constants and limit tests named | S2 | SEC/ConformanceCheck.md → "Checks"; SEC.md → "Parser inventory registry" |
| Z2-R-SEC-029 | Untrusted parse discipline | S2 | SEC/Fuzzing.md → "Harness rules"; SEC/TrustBoundaries.md → "B3 user files" |
| Z2-R-SEC-030 | Declared sizes checked before allocation | S2 | SEC/Fuzzing.md → "Harness rules" |
| Z2-R-SEC-031 | One JSON reader and one UTF-8 validator | S2 | SEC/ParserInventory.md → "Core and platform parsers" |
| Z2-R-SEC-032 | Covered and outside-scope entries bounded | S2 | SEC/ParserInventory.md → "Entry columns"; SEC/ConformanceCheck.md → "Checks" |
| Z2-R-SEC-033 | SecurityConformance gate step | S2 | SEC/ConformanceCheck.md → "Checks"; SEC/ConformanceCheck.md → "Command line and exit codes"; SEC/ConformanceCheck.md → "PROC hand-off" |
| Z2-R-SEC-034 | SecurityConformance fixtures | S2 | SEC/ConformanceCheck.md → "Fixtures" |
| Z2-R-SEC-035 | New parsers land complete | S2 | SEC/ParserInventory.md → "Inventory rules" |
| Z2-R-SEC-036 | Conformance findings tracked | S2 | SEC/ParserInventory.md → "Conformance findings" |
| Z2-R-SEC-037 | Every non-exempt parser has a fuzz target | S2 | SEC/ParserInventory.md → "Inventory rules"; SEC/ConformanceCheck.md → "Checks" |
| Z2-R-SEC-038 | Fuzz stem naming | S2 | SEC/Fuzzing.md → "Stem naming rule" |
| Z2-R-SEC-039 | Harness rules | S2 | SEC/Fuzzing.md → "Harness rules" |
| Z2-R-SEC-040 | Multi-parser harness selection | S2 | SEC/Fuzzing.md → "Harness rules" |
| Z2-R-SEC-041 | Seed corpus rules | S2 | SEC/Fuzzing.md → "Corpus and dictionary rules" |
| Z2-R-SEC-042 | Regression inputs as hex text | S2 | SEC/Fuzzing.md → "Corpus and dictionary rules"; SEC/Fuzzing.md → "Finding ticket body" |
| Z2-R-SEC-043 | Dictionaries for binary formats | S2 | SEC/Fuzzing.md → "Corpus and dictionary rules" |
| Z2-R-SEC-044 | Tool-side fuzz targets | S2 | SEC/Fuzzing.md → "Tool-side fuzz targets"; SEC.md → "CMake functions" |
| Z2-R-SEC-045 | Fuzz smoke sanitizer options | S2 | SEC/Fuzzing.md → "win-asan with libFuzzer" |
| Z2-R-SEC-046 | Seed and regression replay under UBSan | S2 | SEC/Fuzzing.md → "linux-tsan replay with UBSan"; SEC.md → "Parser inventory registry" |
| Z2-R-SEC-047 | HWASan replay on the S24 | S3 | SEC/Fuzzing.md → "HWASan replay on the S24" |
| Z2-R-SEC-048 | Smoke selection by the diff | S2 | SEC/Fuzzing.md → "Smoke selection" |
| Z2-R-SEC-049 | Smoke step budget | S2 | SEC/Fuzzing.md → "Smoke step budget" |
| Z2-R-SEC-050 | Fuzz-owning libraries trigger the sanitizer step | S2 | SEC/Fuzzing.md → "Gate smoke" |
| Z2-R-SEC-051 | Campaign durations per tier | S2 | SEC/Fuzzing.md → "Stage acceptance campaigns" |
| Z2-R-SEC-052 | Campaign runner reads the registry | S2 | SEC/Fuzzing.md → "Stage acceptance campaigns" |
| Z2-R-SEC-053 | Finding records carry log and frames | S2 | SEC/Fuzzing.md → "Finding records" |
| Z2-R-SEC-054 | Finding deduplication and severity | S2 | SEC/Fuzzing.md → "Deduplication key"; SEC/Fuzzing.md → "Severity mapping" |
| Z2-R-SEC-055 | Finding tickets | S2 | SEC/Fuzzing.md → "Finding ticket body"; SEC.md → "Release-time and acceptance tools" |
| Z2-R-SEC-056 | Finding lifecycle | S2 | SEC/Fuzzing.md → "Finding lifecycle" |
| Z2-R-SEC-057 | Findings block the stage acceptance | S2 | SEC/Fuzzing.md → "Regression rule" |
| Z2-R-SEC-058 | Campaign evidence record | S2 | SEC/Fuzzing.md → "Campaign evidence" |
| Z2-R-SEC-059 | Campaign wall-time rows | S2 | SEC.md → "Pending new rows"; SEC/Fuzzing.md → "Campaign wall-time estimate" |
| Z2-R-SEC-060 | Path-traversal matrix complete | S2 | SEC/PathTraversal.md → "The matrix"; SEC.md → "Path matrix registry" |
| Z2-R-SEC-061 | Vector catalogue | S1 | SEC/PathTraversal.md → "Vector catalogue" |
| Z2-R-SEC-062 | Source catalogue | S2 | SEC/PathTraversal.md → "Source catalogue"; SEC/PathTraversal.md → "Matrix maintenance" |
| Z2-R-SEC-063 | Matrix tests exist | S2 | SEC/ConformanceCheck.md → "Checks"; SEC/PathTraversal.md → "Matrix maintenance" |
| Z2-R-SEC-064 | Path matrix registry format | S2 | SEC.md → "Path matrix registry" |
| Z2-R-SEC-065 | Integration suite target | S2 | SEC.md → "Integration suite and benchmark"; SEC/Tests.md → "Test targets and gate steps" |
| Z2-R-SEC-066 | Archive and manifest name composition tests | S2 | SEC/PathTraversal.md → "Composed traversal tests" |
| Z2-R-SEC-067 | Update and install-tree composition tests | S6 | SEC/PathTraversal.md → "Composed traversal tests" |
| Z2-R-SEC-068 | User mount composition tests | S3 | SEC/PathTraversal.md → "Composed traversal tests"; SEC/Tests.md → "Path-matrix composition tests" |
| Z2-R-SEC-069 | Confined OS APIs | S2 | SEC/PathTraversal.md → "Confined OS APIs"; SEC/PathTraversal.md → "Confined API classes" |
| Z2-R-SEC-070 | Confined API allowlist | S2 | SEC/PathTraversal.md → "confined_apis.json"; SEC/PathTraversal.md → "Confined API classes" |
| Z2-R-SEC-071 | Path rules confirmed | S1 | SEC/PathTraversal.md → "Rules confirmed by the ruling" |
| Z2-R-SEC-072 | User namespace for crash and development outputs | S2 | SEC/PathTraversal.md → "Rules confirmed by the ruling"; SEC/ParserInventory.md → "Conformance findings" |
| Z2-R-SEC-073 | Hard-link residual documented | S2 | SEC/PathTraversal.md → "Accepted residuals for paths" |
| Z2-R-SEC-074 | Verification rules in order | S6 | SEC/UpdateAuthentication.md → "Rule order"; SEC/UpdateAuthentication.md → "Rules per manifest kind" |
| Z2-R-SEC-075 | Signature range and form | S6 | SEC/UpdateAuthentication.md → "What is signed and what is checked"; SEC/UpdateAuthentication.md → "Rule order" |
| Z2-R-SEC-076 | Checks at every start | S6 | SEC/UpdateAuthentication.md → "Rule order" |
| Z2-R-SEC-077 | Whole-update rejection with a rule tag | S6 | SEC/UpdateAuthentication.md → "Rejection and logging" |
| Z2-R-SEC-078 | Key never from runtime input | S6 | SEC/UpdateAuthentication.md → "Rule order"; SEC/UpdateAuthentication.md → "Composition-table key" |
| Z2-R-SEC-079 | Ed25519 primitive | S6 | SEC/UpdateAuthentication.md → "Zenith_Ed25519 header"; SEC/UpdateAuthentication.md → "Primitive algorithm" |
| Z2-R-SEC-080 | Ed25519 conformance vectors | S6 | SEC/UpdateAuthentication.md → "Published test keys"; SEC/Tests.md → "Ed25519 primitive tests" |
| Z2-R-SEC-081 | Ed25519 fuzz target | S6 | SEC/Tests.md → "Ed25519 primitive tests" |
| Z2-R-SEC-082 | Published test keys and key predicates | S6 | SEC/UpdateAuthentication.md → "Zenith_Ed25519 header"; SEC/UpdateAuthentication.md → "Published test keys" |
| Z2-R-SEC-083 | Ed25519 implementation selection | S1 | SEC/UpdateAuthentication.md → "Candidates"; SEC/UpdateAuthentication.md → "Criteria"; SEC/UpdateAuthentication.md → "Acceptance rule" |
| Z2-R-SEC-084 | Ed25519 selection harness | S1 | SEC/UpdateAuthentication.md → "Selection harness" |
| Z2-R-SEC-085 | Update key record | S2 | SEC/UpdateAuthentication.md → "update_keys.json" |
| Z2-R-SEC-086 | Update key resolution at configure | S2 | SEC/UpdateAuthentication.md → "Key resolution at configure" |
| Z2-R-SEC-087 | Key in the composition table | S2 | SEC/UpdateAuthentication.md → "Composition-table key"; SEC/UpdateAuthentication.md → "Key resolution at configure" |
| Z2-R-SEC-088 | Fail-closed Shipping key | S6 | SEC/UpdateAuthentication.md → "Fail-closed defaults" |
| Z2-R-SEC-089 | Key review path | S6 | SEC/UpdateAuthentication.md → "update_keys.json" |
| Z2-R-SEC-090 | Key ceremony | S6 | SEC/UpdateAuthentication.md → "Key ceremony" |
| Z2-R-SEC-091 | Key ceremony tool | S6 | SEC/UpdateAuthentication.md → "Ed25519KeyCeremony tool" |
| Z2-R-SEC-092 | Key states and no in-line rotation | S6 | SEC/UpdateAuthentication.md → "Key states"; SEC/UpdateAuthentication.md → "Key replacement" |
| Z2-R-SEC-093 | Compromise response | S6 | SEC/UpdateAuthentication.md → "Compromise response" |
| Z2-R-SEC-094 | Replay and downgrade protection | S6 | SEC/UpdateAuthentication.md → "Replay and downgrade" |
| Z2-R-SEC-095 | Play-delivered packs verified before mounting | S6 | SEC/UpdateAuthentication.md → "Play-delivered packs" |
| Z2-R-SEC-096 | Tamper suite | S6 | SEC/UpdateAuthentication.md → "Tamper suite"; SEC/Tests.md → "Update tamper tests" |
| Z2-R-SEC-097 | Development-service inventory | S3 | SEC/ShippingExposure.md → "Development-service inventory" |
| Z2-R-SEC-098 | Debugger absent from Shipping | S3 | SEC/ShippingExposure.md → "Development-service inventory" |
| Z2-R-SEC-099 | Remote tools absent from Shipping | S3 | SEC/ShippingExposure.md → "Development-service inventory" |
| Z2-R-SEC-100 | Forbidden symbol list format | S3 | SEC/ShippingExposure.md → "shipping_forbidden_symbols.json schema" |
| Z2-R-SEC-101 | Symbol normalisation and matching | S3 | SEC/ShippingExposure.md → "Symbol normalisation and matching" |
| Z2-R-SEC-102 | Consolidated forbidden symbols | S3 | SEC/ShippingExposure.md → "Consolidated entries"; SEC/ShippingExposure.md → "Development-service inventory" |
| Z2-R-SEC-103 | Forbidden string scan | S3 | SEC/ShippingExposure.md → "Forbidden string list"; SEC/ShippingExposure.md → "Initial string entries" |
| Z2-R-SEC-104 | Listener-import scan | S3 | SEC/ShippingExposure.md → "Listener-import scan" |
| Z2-R-SEC-105 | Positive controls | S3 | SEC/ShippingExposure.md → "Positive controls" |
| Z2-R-SEC-106 | Release-time exposure audit tool | S3 | SEC/ShippingExposure.md → "Test-Zenith2ShippingExposure command line"; SEC/ShippingExposure.md → "Audit algorithm"; SEC/ShippingExposure.md → "Audit report"; SEC/ShippingExposure.md → "Release-time audit tool" |
| Z2-R-SEC-107 | Release checks run the audit | S6 | SEC/ShippingExposure.md → "Gate and release placement" |
| Z2-R-SEC-108 | Android manifest audit | S6 | SEC/ShippingExposure.md → "Android manifest audit" |
| Z2-R-SEC-109 | Shipping behaviour composition tests | S3 | SEC/ShippingExposure.md → "ShippingTest behaviour cases"; SEC/Tests.md → "Shipping behaviour tests" |
| Z2-R-SEC-110 | Stage 6 security hardening evidence | S6 | SEC/Tests.md → "Acceptance items"; SEC.md → "Stage 6" |
| Z2-R-SEC-111 | Stage 2 to 5 security acceptance items | S2 | SEC/Tests.md → "Acceptance items" |
| Z2-R-SEC-112 | SEC files in ZenithPlatform | S6 | SEC.md → "SEC files in ZenithPlatform" |
| Z2-R-SEC-113 | SEC API-only slice | S6 | SEC.md → "API-only slices" |
| Z2-R-SEC-114 | Security tooling directory | S2 | SEC.md → "Tools Security directory"; SEC/Tests.md → "PowerShell tests" |
| Z2-R-SEC-115 | Zenith2Security module | S2 | SEC.md → "Zenith2Security module" |
| Z2-R-SEC-116 | Fuzz smoke libFuzzer options | S2 | SEC/Fuzzing.md → "win-asan with libFuzzer" |
| Z2-R-SEC-117 | Wildcard and reserved punctuation refused | S2 | SEC/PathTraversal.md → "Vector catalogue"; SEC/PathTraversal.md → "Matrix rows for runtime sources" |
| Z2-R-SEC-118 | NET parsers inventoried | S3 | SEC/ParserInventory.md → "Online and network parsers"; SEC/ParserInventory.md → "Conformance findings" |
| Z2-R-SEC-119 | Ed25519 harness candidates pinned | S1 | SEC/UpdateAuthentication.md → "Selection harness"; SEC.md → "Stage 1" |
| Z2-R-SEC-120 | Shared regression replay helper | S3 | SEC/Fuzzing.md → "HWASan replay on the S24"; SEC/Fuzzing.md → "Corpus and dictionary rules" |
| Z2-R-SEC-121 | Cinematic capture absent from Shipping | S3 | SEC/ShippingExposure.md → "Development-service inventory" |
| Z2-R-SEC-122 | Zenith2SecurityGates module | S2 | SEC.md → "Zenith2SecurityGates module"; SEC.md → "Gates Security directory" |
| Z2-R-SEC-123 | Committed seeds are published keys | S6 | SEC/UpdateAuthentication.md → "Published test keys" |
| Z2-R-SEC-124 | RFC 8032 vectors checked by a person | S6 | SEC/UpdateAuthentication.md → "Published test keys"; SEC.md → "Stage 6" |
| Z2-R-SEC-125 | Update keys checked in PowerShell | S2 | SEC/UpdateAuthentication.md → "update_keys.json"; SEC/UpdateAuthentication.md → "Key resolution at configure" |
| Z2-R-SEC-126 | Fuzz campaign options | S2 | SEC/Fuzzing.md → "win-asan with libFuzzer" |
| Z2-R-SEC-127 | Replay count never vacuous | S3 | SEC/Fuzzing.md → "HWASan replay on the S24" |
| Z2-R-SEC-128 | NET transport absent from Shipping | S3 | SEC/ShippingExposure.md → "Development-service inventory" |
| Z2-R-SEC-129 | Ed25519 differential against the test signer | S6 | SEC/Tests.md → "Integration suite tests" |
| Z2-R-SEC-130 | Static tool libraries fuzzable in win-asan | S2 | SEC/Fuzzing.md → "Tool-side fuzz targets" |
| Z2-R-SEC-131 | One published-key list | S2 | SEC/UpdateAuthentication.md → "Published test keys" |
