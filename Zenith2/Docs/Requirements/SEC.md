# SEC requirement catalogue

The requirement catalogue of the SEC area (Security & robustness). Every requirement is one testable obligation with the inventory items it covers, its stage (the stage whose acceptance it serves), its classification, governing decisions, budget rows, acceptance evidence and the design headings that specify it. The design is `Zenith2/Docs/Design/SEC.md` and its split files under `Zenith2/Docs/Design/SEC/`.

## Purpose

SEC owns the trust-boundary model, the programme-wide parser and fuzz inventory with its conformance rule and gate step, the fuzzing policy, path-traversal protection as a tested matrix, the shipping-build exposure audit, and authenticated update manifests with their key lifecycle (`Zenith2/Docs/Design/SEC.md` → "What SEC owns"). Every item `Z2-I-SEC-01` … `Z2-I-SEC-10` is covered below.

Evidence phrases: "the PROC `needs-human` ticket of hand-off SEC-P<n> (leaf)" names a protected gate edit recorded in `C:/dev/Zenith2-drafts/findings/A5_proc_handoffs.md`, section SEC, which is that requirement's leaf ticket (ruling BUDGET-KIND-LEAF); "row SEC<n>" is a row of `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`; every named test is specified in `Zenith2/Docs/Design/SEC/Tests.md` or in its owner's design. No requirement asks a worker to run a build or a test.

## Requirements

### Z2-R-SEC-001 Stage 1 security-architecture ruling

- **Statement:** A `needs-human` Stage 1 ruling ticket appends exactly one `Z2-D-NNN` entry to `Zenith2/Docs/DecisionLog.md` in its own diff, recording the confirmations listed under the SEC design's Stage 1 plan.
- **Covers:** Z2-I-SEC-01, Z2-I-SEC-02, Z2-I-SEC-03, Z2-I-SEC-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-084, Z2-D-097
- **Budgets:** none
- **Acceptance evidence:** the Stage 1 security-architecture ruling ticket's `Z2-D-NNN` entry in `Zenith2/Docs/DecisionLog.md`, checked by the coverage gate step in acceptance mode `-Stage 1`
- **Design:** Zenith2/Docs/Design/SEC.md → "Stage 1"; Zenith2/Docs/Design/SEC.md → "Design rulings SEC records"

### Z2-R-SEC-002 Nine trust boundaries with complete records

- **Statement:** The trust-boundary model has exactly the nine boundaries B1 to B9, each recorded with its source, what is trusted, what is validated, where, what is never accepted, the failure behaviour and the evidence.
- **Covers:** Z2-I-SEC-01, Z2-I-SEC-02, Z2-I-SEC-03, Z2-I-SEC-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** the Stage 1 security-architecture ruling ticket's `Z2-D-NNN` entry in `Zenith2/Docs/DecisionLog.md`, checked by the coverage gate step in acceptance mode `-Stage 1`; core-doc row X413 adding the tool-input and process-IPC rows to the Architecture table
- **Design:** Zenith2/Docs/Design/SEC/TrustBoundaries.md → "Boundary record fields"; Zenith2/Docs/Design/SEC/TrustBoundaries.md → "The nine boundaries"

### Z2-R-SEC-003 Boundary classes and tiers

- **Statement:** Every parser carries the boundary classes it is reached through, and its tier is the lowest tier of those classes (tier 1: downloaded content, user files, network, online, process IPC; tier 2: shipped content, scripts; tier 3: remote tools, tool inputs).
- **Covers:** Z2-I-SEC-01, Z2-I-SEC-02, Z2-I-SEC-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** SecurityConformance check SC02 over `parser_inventory.json` from Stage 2; the Stage 1 security-architecture ruling ticket's `Z2-D-NNN` entry in `Zenith2/Docs/DecisionLog.md`, checked by the coverage gate step in acceptance mode `-Stage 1`
- **Design:** Zenith2/Docs/Design/SEC/TrustBoundaries.md → "Boundary classes and tiers"; Zenith2/Docs/Design/SEC/ParserInventory.md → "Boundary classes and tiers used here"

### Z2-R-SEC-004 Shipped content boundary

- **Statement:** Shipped content is trusted for its origin only: every cooked byte passes its format's versioned header, section-table and parser-limit checks, an invalid mount manifest stops a Shipping start with exit code 3, and no content outside the package, patch, loose (non-Shipping) and user mounts is ever mounted.
- **Covers:** Z2-I-SEC-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-075
- **Budgets:** none
- **Acceptance evidence:** `Test_SecPath_ArchiveEntryTraversalRefused`, `Test_SecPath_MountManifestArchiveNameRefused` in `Zenith_Integration_Security` (`Test` step); the tier 2 fuzz smokes
- **Design:** Zenith2/Docs/Design/SEC/TrustBoundaries.md → "B1 shipped content"

### Z2-R-SEC-005 Downloaded content authenticated before mounting

- **Statement:** No patch archive, Windows DLC file or delivered asset pack is mounted before its signed manifest (patches, DLC) or the base package's recorded hash (packs) has verified it, and any verification failure leaves the previous content mounted.
- **Covers:** Z2-I-SEC-04
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** the tamper suite of `Zenith_Integration_Security` in `Run-Committed.ps1 -Acceptance -Stage 6`
- **Design:** Zenith2/Docs/Design/SEC/TrustBoundaries.md → "B2 patches and downloaded content"; Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Rejection and logging"

### Z2-R-SEC-006 User files untrusted and never executable

- **Statement:** Saves, user configuration, the resume state, caches, PACK's install-state files, pipeline caches and replay logs are parsed as hostile input under parser limits, never carry executable content, and a failed check is a `Zenith_Result` error with the defined recovery, never an assert.
- **Covers:** Z2-I-SEC-02, Z2-I-SEC-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** the tier 1 fuzz smokes in `Sanitizers -Preset win-asan`; `Test_SecPath_SaveSlotNameRefused`
- **Design:** Zenith2/Docs/Design/SEC/TrustBoundaries.md → "B3 user files"

### Z2-R-SEC-007 Saves carry no integrity claim

- **Statement:** Save files, replays and cloud objects are protected against corruption by length prefixes and checksums only; Zenith2 makes no tamper-resistance claim for them, and any byte sequence presented as a save yields a world obeying the game's invariants or a `Zenith_SaveError`.
- **Covers:** Z2-I-SEC-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** SAVE's four fuzz targets and format-limit tests in their gate steps
- **Design:** Zenith2/Docs/Design/SEC/TrustBoundaries.md → "Saves and integrity"

### Z2-R-SEC-008 Script bytecode only from authenticated mounts

- **Statement:** In Shipping, Luau bytecode loads only from the package and patch mounts, a Player never compiles source, and no script reaches the filesystem, processes, sockets, dynamic libraries, the debugger or another VM.
- **Covers:** Z2-I-SEC-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-017
- **Budgets:** none
- **Acceptance evidence:** `Test_SecScript_BytecodeOnlyFromPackageAndPatch` in `win-shipping-test`; SCRIPT's sandbox tests
- **Design:** Zenith2/Docs/Design/SEC/TrustBoundaries.md → "B4 scripts"

### Z2-R-SEC-009 Remote tools development-only and loopback

- **Statement:** The DAP server, the remote diagnostics server, the editor live link and Tracy exist only in non-Shipping builds, bind the loopback interface unless `devtools.bindAddress` is given on the command line (never for the live link), and accept a connection only after the session-token handshake (Tracy excepted).
- **Covers:** Z2-I-SEC-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_DevListener_LoopbackByDefault`, `Test_Script_Dap_TokenRequired`, `Test_EditorLiveLink_WrongTokenRejected`; `Test_SecShipping_ListenerRefusedForEveryService`
- **Design:** Zenith2/Docs/Design/SEC/TrustBoundaries.md → "B5 remote tools"; Zenith2/Docs/Design/SEC/TrustBoundaries.md → "Remote-tool session rules"

### Z2-R-SEC-010 Session token entropy and secrecy

- **Statement:** Every development listener's session token is at least 128 bits from the OS cryptographic random source, fresh per process start, and appears only in the local log (and EDIT's access-controlled endpoint file), never in a crash report, telemetry, frame dump, remote response, replay or save.
- **Covers:** Z2-I-SEC-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** PLAT's and EDIT's listener tests extended by row SEC2; the forbidden-string scan proving no handshake literal reaches Shipping
- **Design:** Zenith2/Docs/Design/SEC/TrustBoundaries.md → "Remote-tool session rules"

### Z2-R-SEC-011 Constant-time token comparison

- **Statement:** A received session token is compared with every byte of the expected token accumulated without early exit and the length mismatch folded into the same result.
- **Covers:** Z2-I-SEC-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** PLAT's listener test `Test_DevListener_TokenComparedWholly` and EDIT's equivalent, added by row SEC2 (`Test` step)
- **Design:** Zenith2/Docs/Design/SEC/TrustBoundaries.md → "Remote-tool session rules"

### Z2-R-SEC-012 Handshake deadline and lockout

- **Statement:** A connection whose first message is not the exact handshake within 5,000 ms is closed, and five failed handshakes within 10 s stop the listener accepting for 60 s, counted in `devtools.listener.lockouts`.
- **Covers:** Z2-I-SEC-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** PLAT's listener tests added by row SEC2
- **Design:** Zenith2/Docs/Design/SEC/TrustBoundaries.md → "Remote-tool session rules"

### Z2-R-SEC-013 Non-loopback bind warning

- **Statement:** A development listener bound to an address other than `127.0.0.1` logs `devtools: <service> listening on <address>:<port> beyond loopback` at Warning at every start.
- **Covers:** Z2-I-SEC-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** PLAT's listener test added by row SEC2
- **Design:** Zenith2/Docs/Design/SEC/TrustBoundaries.md → "Remote-tool session rules"; Zenith2/Docs/Design/SEC.md → "Log lines"

### Z2-R-SEC-014 Network boundary for the loopback proof

- **Statement:** Every datagram NET receives passes NET's packet validation order under its wire limits, with every count, length and offset checked against the remaining body before use, and a failing datagram is dropped and counted without an assert.
- **Covers:** Z2-I-SEC-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** the smokes of NET's five fuzz targets (PI-NET-001 to PI-NET-005); NET's wire format tests (`Zenith2/Docs/Design/NET/Tests.md`)
- **Design:** Zenith2/Docs/Design/SEC/TrustBoundaries.md → "B6 network"

### Z2-R-SEC-015 Online services boundary confirmed

- **Statement:** ONLINE's assumptions S1 to S9 hold as SEC confirms them: backend responses and cloud objects are tier 1 untrusted input, ONLINE holds no secret, personal data stays out of logs, replays, crash reports and saves, and development seams are absent from Shipping.
- **Covers:** Z2-I-SEC-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** ONLINE's twelve fuzz targets and Shipping cases; SecurityConformance SC15
- **Design:** Zenith2/Docs/Design/SEC/TrustBoundaries.md → "Online-services confirmation"; Zenith2/Docs/Design/SEC/TrustBoundaries.md → "B7 online services"

### Z2-R-SEC-016 Tool inputs never reach a Player

- **Statement:** DCC sources, authoring documents, shared derived-data entries, clipboard content, child-process output, automation lines and command-line host paths are read only by host tools or non-Shipping options, under their parser limits, and none reaches a Shipping Player.
- **Covers:** Z2-I-SEC-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** the `Libgraph` Shipping closure check ([Z2-N-ARCH-241]); the tier 3 fuzz smokes
- **Design:** Zenith2/Docs/Design/SEC/TrustBoundaries.md → "B8 tool inputs"

### Z2-R-SEC-017 Process IPC trusts inherited handles only

- **Statement:** The Windows crash helper trusts only the handles passed in its handle list, never a PID or path argument, validates the section's magic, version, nonce and parent PID, and exits with code 3 on any failed check.
- **Covers:** Z2-I-SEC-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CrashHelper_ForgedSectionRefused`, `Test_CrashHelper_UnInheritedLaunchExits3`; `Zenith_Fuzz_CrashReport`
- **Design:** Zenith2/Docs/Design/SEC/TrustBoundaries.md → "B9 process IPC"

### Z2-R-SEC-018 Install-root and device preconditions

- **Statement:** The model's skipped checks (no per-file signature on the base package, stamp-trusted files) are justified by the install-root precondition on Windows and the device precondition on Android, both stated and audited.
- **Covers:** Z2-I-SEC-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** the Stage 1 security-architecture ruling ticket's `Z2-D-NNN` entry in `Zenith2/Docs/DecisionLog.md`, checked by the coverage gate step in acceptance mode `-Stage 1`
- **Design:** Zenith2/Docs/Design/SEC/TrustBoundaries.md → "Install-root precondition"; Zenith2/Docs/Design/SEC/TrustBoundaries.md → "Device precondition"; Zenith2/Docs/Design/SEC/TrustBoundaries.md → "Same-user precondition"

### Z2-R-SEC-019 Verified-cache stamp set confirmed

- **Statement:** The Stage 1 ruling confirms the verified-cache stamp set (size, last-write time, change time, volume and 128-bit file ID) together with the four conditions C1 to C4 under which a stamp may skip the hash.
- **Covers:** Z2-I-SEC-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** the Stage 1 security-architecture ruling ticket's `Z2-D-NNN` entry in `Zenith2/Docs/DecisionLog.md`, checked by the coverage gate step in acceptance mode `-Stage 1`; PACK's and PLAT's stamp tests (row PK27 (e))
- **Design:** Zenith2/Docs/Design/SEC/TrustBoundaries.md → "Stamp set confirmed"

### Z2-R-SEC-020 Verified-cache records unauthenticated

- **Statement:** `verified.zinv` and the delivery state file are unauthenticated user state protected only by the install root's or app-private storage's write permission, and the manifest signature does not bind a stamp-trusted file's bytes.
- **Covers:** Z2-I-SEC-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** the Stage 1 security-architecture ruling ticket's `Z2-D-NNN` entry in `Zenith2/Docs/DecisionLog.md`, checked by the coverage gate step in acceptance mode `-Stage 1`
- **Design:** Zenith2/Docs/Design/SEC/TrustBoundaries.md → "Records unauthenticated"

### Z2-R-SEC-021 Keyed-record escalation

- **Statement:** If a supported distribution layout makes `.zenith2-install/` more writable than the executable, that layout ships only after each stamp record carries an HMAC-SHA-256 under a 32-byte secret kept outside the install root, a record whose MAC fails being treated as absent.
- **Covers:** Z2-I-SEC-04
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** the distribution-layout audit record of every supported layout at Stage 6 (no failed L1, or the escalation landed)
- **Design:** Zenith2/Docs/Design/SEC/TrustBoundaries.md → "Keyed-record escalation"

### Z2-R-SEC-022 Distribution-layout audit

- **Statement:** At Stage 6 every supported distribution layout is audited with `Test-Zenith2InstallLayout.ps1` (L1 write principals of `.zenith2-install`, `Patches` and `DLC` within the executable's, L2 volume kind, L3 no escaping reparse point) and the report is in the qualification record.
- **Covers:** Z2-I-SEC-04, Z2-I-SEC-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Sec_InstallLayout_WritePrincipals`; the Stage 6 layout reports (`needs-human` step)
- **Design:** Zenith2/Docs/Design/SEC/TrustBoundaries.md → "Distribution-layout audit"

### Z2-R-SEC-023 Accepted residuals recorded

- **Statement:** Every residual the model accepts (R1 to R9) is recorded with why it is accepted and where, and none is accepted silently.
- **Covers:** Z2-I-SEC-01, Z2-I-SEC-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-022
- **Budgets:** none
- **Acceptance evidence:** the Stage 1 security-architecture ruling ticket's `Z2-D-NNN` entry in `Zenith2/Docs/DecisionLog.md`, checked by the coverage gate step in acceptance mode `-Stage 1`
- **Design:** Zenith2/Docs/Design/SEC/TrustBoundaries.md → "Accepted residual risks"; Zenith2/Docs/Design/SEC/TrustBoundaries.md → "Non-goals"

### Z2-R-SEC-024 One parser inventory

- **Statement:** Every parser of external bytes and every fuzz target is one row of the SEC inventory and one entry of `Zenith2/Tools/Security/parser_inventory.json` with the same ID, naming its library, classes, tier, limits location, fuzz target, owner tickets and status.
- **Covers:** Z2-I-SEC-05, Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** SecurityConformance checks SC01, SC02, SC09
- **Design:** Zenith2/Docs/Design/SEC/ParserInventory.md → "Inventory rules"; Zenith2/Docs/Design/SEC/ParserInventory.md → "Entry columns"; Zenith2/Docs/Design/SEC.md → "Parser inventory registry"

### Z2-R-SEC-025 Parser inventory registry format

- **Statement:** `parser_inventory.json` is `zenith2.parser-inventory` version 1 with the fields, rules and ordering of the registry specification.
- **Covers:** Z2-I-SEC-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Sec_ParserInventory_CommittedFileValid` (`Test` step); SC01
- **Design:** Zenith2/Docs/Design/SEC.md → "Parser inventory registry"

### Z2-R-SEC-026 Every inventory entry names an owner

- **Statement:** Every inventory entry names at least one owner ticket Draft-ID, or `pending:<AREA>` while that area's tickets are not drafted.
- **Covers:** Z2-I-SEC-05, Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-068
- **Budgets:** none
- **Acceptance evidence:** SC02
- **Design:** Zenith2/Docs/Design/SEC/ParserInventory.md → "Inventory rules"

### Z2-R-SEC-027 Limits located in one place

- **Statement:** Each entry's limits are a row of the Architecture parser-limits table or, while that row is pending a core-doc batch, the owner's heading with status `pending-row`, and every Architecture parser-limits row is named by an entry.
- **Covers:** Z2-I-SEC-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** SC03, SC04; core-doc row X413 for the missing rows
- **Design:** Zenith2/Docs/Design/SEC/ParserInventory.md → "Inventory rules"; Zenith2/Docs/Design/SEC/ConformanceCheck.md → "Checks"

### Z2-R-SEC-028 Limit constants and limit tests named

- **Statement:** Each entry names its limit constants, each declared `inline constexpr` in its library, and the tests that exercise every limit at the limit and at limit + 1.
- **Covers:** Z2-I-SEC-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** SC05, SC06
- **Design:** Zenith2/Docs/Design/SEC/ConformanceCheck.md → "Checks"; Zenith2/Docs/Design/SEC.md → "Parser inventory registry"

### Z2-R-SEC-029 Untrusted parse discipline

- **Statement:** Every inventoried parser validates every header, size, count and offset against the input length with overflow-checked arithmetic before use and returns a `Zenith_Result` error, never an assert or undefined behaviour, on invalid input.
- **Covers:** Z2-I-SEC-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** the fuzz smokes (rule H6: an assert reached is a finding); the campaigns
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "Harness rules"; Zenith2/Docs/Design/SEC/TrustBoundaries.md → "B3 user files"

### Z2-R-SEC-030 Declared sizes checked before allocation

- **Statement:** An allocation sized from a declared count or length is checked against the parser's limits before it is made, so the smoke and the campaign of every fuzz target report no finding of kind `oom` under `-rss_limit_mb=2048`.
- **Covers:** Z2-I-SEC-05, Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** no `oom` finding in any smoke or campaign `campaign.json`
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "Harness rules"

### Z2-R-SEC-031 One JSON reader and one UTF-8 validator

- **Statement:** Every untrusted JSON byte sequence is parsed by `Zenith_JsonDocument::Parse` under the JSON limits and every untrusted text by CORE's UTF-8 validation, and no area adds a second JSON parser for untrusted input.
- **Covers:** Z2-I-SEC-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** inventory rows PI-CORE-001 and PI-CORE-002 and their fuzz targets; SC09 refuses an uninventoried parser target
- **Design:** Zenith2/Docs/Design/SEC/ParserInventory.md → "Core and platform parsers"

### Z2-R-SEC-032 Covered and outside-scope entries bounded

- **Statement:** Only tier 3 entries may be `covered` (naming existing covering entries) or `outside-scope`, and an accepted finding names a decision ID present in the decision log.
- **Covers:** Z2-I-SEC-05, Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-022
- **Budgets:** none
- **Acceptance evidence:** SC12
- **Design:** Zenith2/Docs/Design/SEC/ParserInventory.md → "Entry columns"; Zenith2/Docs/Design/SEC/ConformanceCheck.md → "Checks"

### Z2-R-SEC-033 SecurityConformance gate step

- **Statement:** The protected gate step `SecurityConformance` runs checks SC01 to SC16 on every ticket from Stage 2, exits 0, 1 or 2 as specified, writes `SecurityConformance.json`, and passes known findings while failing every new nonconformance.
- **Covers:** Z2-I-SEC-05, Z2-I-SEC-06, Z2-I-SEC-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-042, Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** the PROC `needs-human` ticket of hand-off SEC-P1 (leaf) and its fixture assert script
- **Design:** Zenith2/Docs/Design/SEC/ConformanceCheck.md → "Checks"; Zenith2/Docs/Design/SEC/ConformanceCheck.md → "Command line and exit codes"; Zenith2/Docs/Design/SEC/ConformanceCheck.md → "PROC hand-off"

### Z2-R-SEC-034 SecurityConformance fixtures

- **Statement:** Every check SC01 to SC16 has a fixture tree that fails exactly that check, a passing tree passes, a known finding passes, and a failed configure exits 2.
- **Covers:** Z2-I-SEC-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** `Test-SecurityConformance.ps1` cases `SecConformance_*`, landed by the PROC `needs-human` ticket of hand-off SEC-P1 (leaf)
- **Design:** Zenith2/Docs/Design/SEC/ConformanceCheck.md → "Fixtures"

### Z2-R-SEC-035 New parsers land complete

- **Statement:** A ticket that adds or renames a parser adds its inventory row and registry entry, its Architecture row or the core-doc row requesting it, its named constants, its limit tests and its fuzz source in the same diff.
- **Covers:** Z2-I-SEC-05, Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-085
- **Budgets:** none
- **Acceptance evidence:** SC03 to SC09 on that ticket's own gate run
- **Design:** Zenith2/Docs/Design/SEC/ParserInventory.md → "Inventory rules"

### Z2-R-SEC-036 Conformance findings tracked

- **Statement:** Each inventory gap, naming break and dead target is listed as a finding F1 to F7 with its owner and requested change, and SecurityConformance fails when a listed status no longer matches the tree.
- **Covers:** Z2-I-SEC-05, Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-022
- **Budgets:** none
- **Acceptance evidence:** SC11
- **Design:** Zenith2/Docs/Design/SEC/ParserInventory.md → "Conformance findings"

### Z2-R-SEC-037 Every non-exempt parser has a fuzz target

- **Statement:** Every inventory entry other than `covered`, `outside-scope` and a stemless `gap` has `Tests/Fuzz/<Stem>.cpp` in a library whose target exists in `win-asan`, or in a tool that registers it through `zenith_tool_fuzz_target` or its ZenithCook wrapper `zenith_cook_fuzz_target`.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** SC07, SC10
- **Design:** Zenith2/Docs/Design/SEC/ParserInventory.md → "Inventory rules"; Zenith2/Docs/Design/SEC/ConformanceCheck.md → "Checks"

### Z2-R-SEC-038 Fuzz stem naming

- **Statement:** A fuzz stem matches `^[A-Z][A-Za-z0-9]*(_[A-Z][A-Za-z0-9]*)*$`, is at most 48 characters, does not begin with `Fuzz` or `Zenith`, names its format or entry function and is unique programme-wide.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** SC08
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "Stem naming rule"

### Z2-R-SEC-039 Harness rules

- **Statement:** Every fuzz harness defines only `LLVMFuzzerTestOneInput`, reaches the parser through its entry function, does no file, socket, clock or environment access, keeps no state across inputs, checks the heap statistics unchanged after each input, seeds randomness from the input, and asserts invariants only.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** finding kinds `leak` and `assert` in the smokes; the confined-API rule F24 for file and socket access
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "Harness rules"

### Z2-R-SEC-040 Multi-parser harness selection

- **Statement:** A harness that drives several parsers selects one with its first input byte, and each driven parser is its own inventory entry naming the same stem.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** SC02, SC07
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "Harness rules"

### Z2-R-SEC-041 Seed corpus rules

- **Statement:** Each target has at least one valid seed per driven parser and one seed per enumerated check, text seeds committed under `Corpus/<Stem>/` (at most 64 KiB, 256 files), binary seeds generated by a `z2-fixture-setup` case and passed to the smoke.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** the smokes' corpus argument; each library's `Test_<Stem>_Regressions` replaying the seeds
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "Corpus and dictionary rules"

### Z2-R-SEC-042 Regression inputs as hex text

- **Statement:** Every fixed finding's input is committed as `Tests/Fuzz/Regressions/<Stem>/<sha1>.hex` (lower-case hex, 64 bytes per line, LF), carried in the fix ticket's body only when it is at most 4,096 bytes and otherwise committed by the recording ticket, and replayed by `Test_<Stem>_Regressions` through CORE's `Zenith_DecodeHexRegression`.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Sec_FuzzTriage_HexRoundTrip`, `Pwsh_Sec_FuzzTriage_BodiesWellFormed`; each library's `Test_<Stem>_Regressions` (`Test` step)
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "Corpus and dictionary rules"; Zenith2/Docs/Design/SEC/Fuzzing.md → "Finding ticket body"

### Z2-R-SEC-043 Dictionaries for binary formats

- **Statement:** Every binary format with a magic number has `Tests/Fuzz/<Stem>.dict` holding its magic, version and kind values and section tags, and every JSON parser's dictionary holds its schema keys.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** the registry's `dict` field checked against the file by SC07
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "Corpus and dictionary rules"

### Z2-R-SEC-044 Tool-side fuzz targets

- **Statement:** SHIP's `zenith_tool_fuzz_target`, with `zenith_cook_fuzz_target` as its ZenithCook wrapper, records every declaration in `declared_deps.json`'s `toolFuzzTargets[]` in every preset, refuses a `TOOL_DIR` that is not the tool's directory of the table "Tool directories", and, in `win-asan`, builds `Zenith_Fuzz_<Stem>` from the tool's fuzz source and listed Internal sources with the generic rule's smoke properties, failing the configure for a missing source, a main TU or a dependency absent from `win-asan`; `Tests/Fuzz/**` is in no tool's `_Tests` or `_Objects` target.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `CMake_Sec_ToolFuzzTargetDeclarations` (`Test` step), written by SHIP's helper ticket (row SEC6 (g)); SC10 over the ASSET and VFX calls
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "Tool-side fuzz targets"; Zenith2/Docs/Design/SEC.md → "CMake functions"

### Z2-R-SEC-045 Fuzz smoke sanitizer options

- **Statement:** Every `Zenith_Fuzz_<Stem>.Smoke` sets, through its CTest `ENVIRONMENT` property, `ASAN_OPTIONS=halt_on_error=1:detect_leaks=0:suppressions=<repo>/Zenith2/Tools/Gates/Suppressions/asan.supp:abort_on_error=1:allocator_may_return_null=1:symbolize=1`, the suppression path being the one the `Sanitizers` step uses.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** Z2-B-PROC-025
- **Acceptance evidence:** `CMake_Sec_ToolFuzzTargetDeclarations`; SHIP's generic fuzz rule properties (row SEC6 (b))
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "win-asan with libFuzzer"

### Z2-R-SEC-046 Seed and regression replay under UBSan

- **Statement:** In every `ZENITH_HOST` preset where its library is built, each fuzzed library's `Test_<Stem>_Regressions` replays its regressions, committed seeds and generated seeds, under the `UBSAN_OPTIONS` [Z2-N-ARCH-220] fixes in `linux-tsan`; an entry's registry field `ubsan` is true exactly when its library is built in `linux-tsan`.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** Z2-B-PROC-026
- **Acceptance evidence:** `Sanitizers -Preset linux-tsan` running the regression tests; `Pwsh_Sec_ParserInventory_CommittedFileValid` for the `ubsan` field; residual R10 for the libraries `linux-tsan` does not build
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "linux-tsan replay with UBSan"; Zenith2/Docs/Design/SEC.md → "Parser inventory registry"

### Z2-R-SEC-047 HWASan replay on the S24

- **Statement:** From Stage 3 every stage acceptance runs the regression tests of every tier 1 and tier 2 library targeting Android in the HWASan test APK on the S24, as a `needs-human` device step, after MOD's push of the four items of the push table (regressions, corpus, count files, generated seeds).
- **Covers:** Z2-I-SEC-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-010, Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** the device run record of each stage acceptance from Stage 3; MOD's push (row SEC5 (d))
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "HWASan replay on the S24"

### Z2-R-SEC-048 Smoke selection by the diff

- **Statement:** The `Sanitizers -Preset win-asan` step runs the 60-second smoke of a target only when the diff touches its owning library or tool, its fuzz files, a library in its `smokeAlso`, or its registry entry (compared by `id` against the merge base), and CI runs every smoke.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-028, Z2-D-128
- **Budgets:** Z2-B-PROC-025
- **Acceptance evidence:** the PROC `needs-human` ticket of hand-off SEC-P2 (leaf)
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "Smoke selection"

### Z2-R-SEC-049 Smoke step budget

- **Statement:** With J = min(8, half the logical processors) smokes concurrent, the smokes take ceil(N/J) × 60 s, and when that exceeds 960 s each smoke runs max(15, floor(960 / ceil(N/J))) s with a log line, so the waves never take more than 960 s above the floor.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-028, Z2-D-128
- **Budgets:** Z2-B-PROC-025
- **Acceptance evidence:** the PROC `needs-human` ticket of hand-off SEC-P2 (leaf); Z2-B-PROC-025's allocation
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "Smoke step budget"

### Z2-R-SEC-050 Fuzz-owning libraries trigger the sanitizer step

- **Statement:** Every library and tool directory that owns a fuzz target is in the `win-asan` list of `sanitizer_paths.json` and so in the `Zenith2Memory` category's paths, so a diff to any parser triggers the smoke.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-032, Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** the PROC `needs-human` ticket of hand-off SEC-P3 (leaf)
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "Gate smoke"

### Z2-R-SEC-051 Campaign durations per tier

- **Statement:** Each stage acceptance from Stage 2 runs every target present in the tree for its tier's duration from the registry: 600, 180 and 60 s at Stages 2 to 5 and 3,600, 1,800 and 600 s at Stage 6 for tiers 1, 2 and 3, with `-Jobs 8`, and no area's design states another duration.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** the campaign evidence record of every stage acceptance (`needs-human` step)
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "Stage acceptance campaigns"

### Z2-R-SEC-052 Campaign runner reads the registry

- **Statement:** DEVT's campaign runner takes `-Inventory` and `-Stage`, reading each target's duration, `maxLen` and dictionary from `parser_inventory.json`.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** DEVT's runner tests extended by row SEC3
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "Stage acceptance campaigns"

### Z2-R-SEC-053 Finding records carry log and frames

- **Statement:** Every campaign finding records its kind, input, SHA-1, reproduction, log path and the first three symbolised frames outside the sanitizer runtime.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** DEVT's runner tests extended by row SEC3; `Pwsh_Sec_FuzzTriage_DedupAndSeverity`
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "Finding records"

### Z2-R-SEC-054 Finding deduplication and severity

- **Statement:** Findings are deduplicated by `<Stem>|<kind>|<f1>|<f2>|<f3>`, keeping the smallest input, and given the severity of the tier-by-kind table.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Sec_FuzzTriage_DedupAndSeverity` (`Test` step)
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "Deduplication key"; Zenith2/Docs/Design/SEC/Fuzzing.md → "Severity mapping"

### Z2-R-SEC-055 Finding tickets

- **Statement:** `New-Zenith2FuzzFindingTickets.ps1` writes, per deduplicated finding, a `needs-human` recording body, a BUG fix body with the ten sections blocked by it, the `.hex` input and `findings.json`, and files nothing.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-068
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Sec_FuzzTriage_BodiesWellFormed` (`Test` step)
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "Finding ticket body"; Zenith2/Docs/Design/SEC.md → "Release-time and acceptance tools"

### Z2-R-SEC-056 Finding lifecycle

- **Statement:** Every finding moves through the states NEW, TRIAGED, FILED, RECORDED, FIXED and VERIFIED (or DUPLICATE, DISCARDED, ACCEPTED) of the lifecycle table, and only a tier 3 `timeout` or `oom` finding may be ACCEPTED, by a ruling.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-097
- **Budgets:** none
- **Acceptance evidence:** SC12 for accepted findings; the stage acceptance's campaign item
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "Finding lifecycle"

### Z2-R-SEC-057 Findings block the stage acceptance

- **Statement:** A reproduced tier 1 finding blocks its stage acceptance until VERIFIED, a tier 2 or 3 finding until VERIFIED or ACCEPTED, and a smoke failure in a ticket's gate is fixed in that ticket.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** the campaign evidence record of each stage acceptance
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "Regression rule"

### Z2-R-SEC-058 Campaign evidence record

- **Statement:** Each stage acceptance's campaign is recorded through DEVT's evidence record with `campaign.json`, its summary, the inventory's SHA-256 and the evidence fingerprint, and passes only when every target present in the tree ran its duration and no unaccepted finding is open.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** the evidence record of each stage acceptance from Stage 2
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "Campaign evidence"

### Z2-R-SEC-059 Campaign wall-time rows

- **Statement:** The wall time of the Stage 2 to 5 and the Stage 6 campaigns is measured and reported against two pending budget rows whose producer is the Stage 1 security-architecture ruling.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** the two rows allocated by the Stage 1 ruling (core-doc row X415) and each campaign summary
- **Design:** Zenith2/Docs/Design/SEC.md → "Pending new rows"; Zenith2/Docs/Design/SEC/Fuzzing.md → "Campaign wall-time estimate"

### Z2-R-SEC-060 Path-traversal matrix complete

- **Statement:** For every runtime source P1 to P7, P13 to P15 and P18, every vector V1 to V18 has a cell naming its tests or a reason it cannot be expressed, recorded in `path_matrix.json`.
- **Covers:** Z2-I-SEC-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** SC13 over `path_matrix.json`
- **Design:** Zenith2/Docs/Design/SEC/PathTraversal.md → "The matrix"; Zenith2/Docs/Design/SEC.md → "Path matrix registry"

### Z2-R-SEC-061 Vector catalogue

- **Statement:** The vectors V1 to V18 (parent and current references, absolute and device paths, streams, separators, encodings, reserved names, trailing characters, lengths, links, hard links, aliases, look-alikes, namespace escapes, races, entry slip and overwrites) are each refused by the named control or recorded as a residual.
- **Covers:** Z2-I-SEC-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** the Stage 1 security-architecture ruling ticket's `Z2-D-NNN` entry in `Zenith2/Docs/DecisionLog.md`, checked by the coverage gate step in acceptance mode `-Stage 1`
- **Design:** Zenith2/Docs/Design/SEC/PathTraversal.md → "Vector catalogue"

### Z2-R-SEC-062 Source catalogue

- **Statement:** Every source of a name or path in Zenith2 is one of P1 to P20 with its validating control, and a ticket adding a source adds its catalogue and matrix rows in the same diff.
- **Covers:** Z2-I-SEC-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-085
- **Budgets:** none
- **Acceptance evidence:** SC13 on the matrix registry
- **Design:** Zenith2/Docs/Design/SEC/PathTraversal.md → "Source catalogue"; Zenith2/Docs/Design/SEC/PathTraversal.md → "Matrix maintenance"

### Z2-R-SEC-063 Matrix tests exist

- **Statement:** Every test named in `path_matrix.json` exists as a `TEST_CASE`, unless its cell carries `pending` with a cross-area row.
- **Covers:** Z2-I-SEC-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** SC13
- **Design:** Zenith2/Docs/Design/SEC/ConformanceCheck.md → "Checks"; Zenith2/Docs/Design/SEC/PathTraversal.md → "Matrix maintenance"

### Z2-R-SEC-064 Path matrix registry format

- **Statement:** `path_matrix.json` is `zenith2.path-matrix` version 1 with one record per source and the cell rules of the registry specification.
- **Covers:** Z2-I-SEC-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Sec_ParserInventory_CommittedFileValid`
- **Design:** Zenith2/Docs/Design/SEC.md → "Path matrix registry"

### Z2-R-SEC-065 Integration suite target

- **Statement:** `Zenith_Integration_Security` is one `zenith_add_integration_tests(Security …)` call depending only on `ZENITH_ALL` libraries, so it runs in `win`, `win-asan`, `linux-tsan` and `win-shipping-test`.
- **Covers:** Z2-I-SEC-07, Z2-I-SEC-08, Z2-I-SEC-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** the `Configure`, `Build` and `Test` steps; `CompileCoverage`
- **Design:** Zenith2/Docs/Design/SEC.md → "Integration suite and benchmark"; Zenith2/Docs/Design/SEC/Tests.md → "Test targets and gate steps"

### Z2-R-SEC-066 Archive and manifest name composition tests

- **Statement:** Hostile archive entries, mount-manifest archive names and archive links are refused through the real archive reader and virtual filesystem, and a literal `%2e%2e` entry resolves as a name.
- **Covers:** Z2-I-SEC-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_SecPath_ArchiveEntryTraversalRefused`, `Test_SecPath_ArchiveEntryCaseIsExact`, `Test_SecPath_MountManifestArchiveNameRefused`, `Test_SecPath_MountManifestArchiveLinkRefused` (`Test` step)
- **Design:** Zenith2/Docs/Design/SEC/PathTraversal.md → "Composed traversal tests"

### Z2-R-SEC-067 Update and install-tree composition tests

- **Statement:** Hostile update-manifest entry names, DLC names, patch directory names, a junction or link in the patch root and a link in a pack location are refused through the real verifier, applier, install file system and directory source.
- **Covers:** Z2-I-SEC-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Test_SecPath_UpdateManifestEntryNameRefused`, `Test_SecPath_DlcNameMismatchRefused`, `Test_SecPath_PatchDirectoryNameIgnored`, `Test_SecPath_JunctionInPatchRootRefused`, `Test_SecPath_PackLocationLinkEscapeRefused` (`Test` step)
- **Design:** Zenith2/Docs/Design/SEC/PathTraversal.md → "Composed traversal tests"

### Z2-R-SEC-068 User mount composition tests

- **Statement:** Links planted in the user directory, names outside the user namespace, hostile save slot names, evidence-directory names and module names are refused end to end, and nothing is written outside the root.
- **Covers:** Z2-I-SEC-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_SecPath_UserMountLinkEscapeRefused`, `Test_SecPath_UserNamespaceEnforced`, `Test_SecPath_SaveSlotNameRefused`, `Test_SecPath_EvidenceDirNameRefused`, `Test_SecScript_ModuleNameRules`, `Test_SecPath_EveryCorpusSeedThroughResolve` (`Test` step)
- **Design:** Zenith2/Docs/Design/SEC/PathTraversal.md → "Composed traversal tests"; Zenith2/Docs/Design/SEC/Tests.md → "Path-matrix composition tests"

### Z2-R-SEC-069 Confined OS APIs

- **Statement:** No Zenith2 source outside the directories `confined_apis.json` allows calls or takes the address of a raw file, process, socket or dynamic-loading API of the confined classes, enforced as `ForbiddenCheck` rule F24.
- **Covers:** Z2-I-SEC-07, Z2-I-SEC-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-042, Z2-D-024, Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** the PROC `needs-human` ticket of hand-off SEC-P4 (leaf), with core-doc row X414; its fixtures `F24-*`
- **Design:** Zenith2/Docs/Design/SEC/PathTraversal.md → "Confined OS APIs"; Zenith2/Docs/Design/SEC/PathTraversal.md → "Confined API classes"

### Z2-R-SEC-070 Confined API allowlist

- **Statement:** `confined_apis.json` is `zenith2.confined-apis` version 1 listing each class's tokens and one allow record per class and directory with owner, decision and reason, starting from the initial allowlist, whose dynamic-loading records are exactly F22's directories.
- **Covers:** Z2-I-SEC-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** the PROC `needs-human` ticket of hand-off SEC-P4 (leaf)
- **Design:** Zenith2/Docs/Design/SEC/PathTraversal.md → "confined_apis.json"; Zenith2/Docs/Design/SEC/PathTraversal.md → "Confined API classes"

### Z2-R-SEC-071 Path rules confirmed

- **Statement:** The Stage 1 ruling confirms R1 to R10 in order with no normalisation, the escape check on the opened handle, the case policy, and the exceptions of [Z2-N-ARCH-923] as the complete list of host paths outside the four mounts.
- **Covers:** Z2-I-SEC-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** the Stage 1 security-architecture ruling ticket's `Z2-D-NNN` entry in `Zenith2/Docs/DecisionLog.md`, checked by the coverage gate step in acceptance mode `-Stage 1`; PLAT's revalidation ticket Z2T-PLAT-003 blocked by it
- **Design:** Zenith2/Docs/Design/SEC/PathTraversal.md → "Rules confirmed by the ruling"

### Z2-R-SEC-072 User namespace for crash and development outputs

- **Statement:** Rule R10 admits `crash` in every build and `devtools` only where `Zenith_IsShippingBuild()` is false, so DEVT's and CINE's outputs resolve and a Shipping build refuses `user:/devtools/`.
- **Covers:** Z2-I-SEC-07, Z2-I-SEC-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_SecShipping_UserNamespaceDevtoolsRefused`; PLAT's `Test_VirtualPath_EachRuleRejects` amended by row SEC1
- **Design:** Zenith2/Docs/Design/SEC/PathTraversal.md → "Rules confirmed by the ruling"; Zenith2/Docs/Design/SEC/ParserInventory.md → "Conformance findings"

### Z2-R-SEC-073 Hard-link residual documented

- **Statement:** The hard-link residual R1 is accepted only under the install-root, device and same-user preconditions and is documented in PLAT's escape-check source.
- **Covers:** Z2-I-SEC-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-022
- **Budgets:** none
- **Acceptance evidence:** the escape-check source comment, a review-focus item of PLAT's ticket Z2T-PLAT-013
- **Design:** Zenith2/Docs/Design/SEC/PathTraversal.md → "Accepted residuals for paths"

### Z2-R-SEC-074 Verification rules in order

- **Statement:** Every update and DLC manifest is checked in the order V1 to V9, with no field beyond V2's interpreted before the V3 signature check and the first failure stopping verification.
- **Covers:** Z2-I-SEC-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** the tamper suite; `Test_PatchMount_TamperCasesRejected`
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Rule order"; Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Rules per manifest kind"

### Z2-R-SEC-075 Signature range and form

- **Statement:** The signature is pure Ed25519 over exactly the bytes `[0, total − 64)` against the 32-byte compiled-in key, so a truncated or extended file fails.
- **Covers:** Z2-I-SEC-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Test_SecUpdate_TruncatedRefused`, `Test_SecUpdate_AppendedByteRefused`, `Test_SecUpdate_FlippedSignatureByteRefused`
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "What is signed and what is checked"; Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Rule order"

### Z2-R-SEC-076 Checks at every start

- **Statement:** At every start V1 to V6 run for every committed patch candidate and Windows DLC and V9 runs with the stamp lookup.
- **Covers:** Z2-I-SEC-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** Z2-B-SHIP-013, Z2-B-SHIP-014
- **Acceptance evidence:** `Test_SecUpdate_StampFieldChangeRehashes`; PLAT's `Test_PatchMount_StampSkipAndRehash`
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Rule order"

### Z2-R-SEC-077 Whole-update rejection with a rule tag

- **Statement:** A failed rule rejects the whole update, mounts none of it, keeps the previous content and logs the version, rule and U-check without any file byte.
- **Covers:** Z2-I-SEC-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** every `Test_SecUpdate_*` case asserts the log line and the content still served
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Rejection and logging"

### Z2-R-SEC-078 Key never from runtime input

- **Statement:** The update public key is the composition table's 32 bytes and is never read from a file, configuration layer, environment variable or command line.
- **Covers:** Z2-I-SEC-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** SHIP's W10 on every packaged executable; the confined-API rule F24
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Rule order"; Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Composition-table key"

### Z2-R-SEC-079 Ed25519 primitive

- **Statement:** `Zenith_Ed25519Verify` returns false for a wrong span size, an over-long message, an all-zero key and S ≥ L before calling the admitted implementation, allocates nothing, holds no state and is the only signature check.
- **Covers:** Z2-I-SEC-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-046, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Test_Ed25519_SizesRefused`, `Test_Ed25519_ZeroKeyRefused`, `Test_Ed25519_RejectsMutations`, `Test_Ed25519_NoAllocation` (`Test`, `Sanitizers` steps)
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Zenith_Ed25519 header"; Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Primitive algorithm"

### Z2-R-SEC-080 Ed25519 conformance vectors

- **Statement:** RFC 8032 TEST 1 to TEST 3 verify, and each of the 512 signature bits, each of the 256 key bits and each bit of the first, middle and last message byte, flipped alone, is rejected.
- **Covers:** Z2-I-SEC-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Test_Ed25519_Rfc8032Vectors`, `Test_Ed25519_RejectsMutations`
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Published test keys"; Zenith2/Docs/Design/SEC/Tests.md → "Ed25519 primitive tests"

### Z2-R-SEC-081 Ed25519 fuzz target

- **Statement:** `Zenith_Fuzz_Ed25519Verify` fuzzes the primitive with arbitrary signatures and messages against TEST 1's key in `win-asan`.
- **Covers:** Z2-I-SEC-06, Z2-I-SEC-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** its smoke in `Sanitizers -Preset win-asan`
- **Design:** Zenith2/Docs/Design/SEC/Tests.md → "Ed25519 primitive tests"

### Z2-R-SEC-082 Published test keys and key predicates

- **Statement:** `Zenith_UpdateKeyRules.h` holds the three RFC 8032 public keys and the `consteval` predicates `Zenith_IsPublishedTestUpdateKey` and `Zenith_IsZeroUpdateKey`, which never emit the key bytes into an image.
- **Covers:** Z2-I-SEC-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateKeyRules_PublishedKeysDetected`; the forbidden-string scan of Shipping images
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Zenith_Ed25519 header"; Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Published test keys"

### Z2-R-SEC-083 Ed25519 implementation selection

- **Statement:** The Stage 1 ruling admits Monocypher 4.0.2, else libsodium 1.0.20, else a Zenith-owned verifier, by criteria K1 to K8 and the acceptance rule, moving the admitted entry to `owner: ZenithPlatform`, `usage: runtime` and removing the other.
- **Covers:** Z2-I-SEC-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-084, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** the Stage 1 security-architecture ruling ticket's `Z2-D-NNN` entry in `Zenith2/Docs/DecisionLog.md`, checked by the coverage gate step in acceptance mode `-Stage 1`; `Zenith2/Docs/Research/Security/Ed25519Selection.md`
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Candidates"; Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Criteria"; Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Acceptance rule"

### Z2-R-SEC-084 Ed25519 selection harness

- **Statement:** `Zenith_Bench_Ed25519` runs the vectors, the mutation set and the timed verifications for each candidate, writes `zenith2.bench-ed25519` and has a `--smoke` CTest.
- **Covers:** Z2-I-SEC-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** the harness's CTest smoke in the `Test` step
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Selection harness"

### Z2-R-SEC-085 Update key record

- **Statement:** `Zenith2/Tools/Security/update_keys.json` is `zenith2.update-keys` version 1 holding only public keys, each record carrying `publicKey` (base64), `publicKeyHex` and `publicKeySha256`, with at most one active key per game and release line and every rule of its table, committed at Stage 2 with no record.
- **Covers:** Z2-I-SEC-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** SC14; `Pwsh_Sec_ParserInventory_CommittedFileValid`; `Pwsh_Sec_UpdateKeys_Rules`
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "update_keys.json"

### Z2-R-SEC-086 Update key resolution at configure

- **Statement:** `zenith_security_resolve_update_key` registers the key file, the check script and the published-key list as `CMAKE_CONFIGURE_DEPENDS`, checks the string rules, runs the PowerShell key check with absolute paths and a 60-second timeout, and writes the active key (or 32 zero bytes with a warning) to `Zenith_UpdateKey.Shipping.generated.inc` and RFC 8032 TEST 1's key (or the active key with `ZENITH_UPDATE_KEY_IN_DEVELOPMENT=RELEASE`) to `Zenith_UpdateKey.Development.generated.inc`, failing the configure on any rule.
- **Covers:** Z2-I-SEC-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** `CMake_Sec_UpdateKeyResolution` (`Test` step, `win`); the PROC `needs-human` ticket of hand-off SEC-P9 (leaf) for the PowerShell check
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Key resolution at configure"

### Z2-R-SEC-087 Key in the composition table

- **Statement:** Each game's generated composition TU initialises its 32-byte key from the include file the configuration's `ZENITH_UPDATE_KEY_INC` definition names, states a static assertion that no Shipping build carries a published test key, and the composition root passes the key to the patch mount, the applier and DLC mounting.
- **Covers:** Z2-I-SEC-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** MOD's composition generator tests extended by row SEC5 (a), whose change SEC's Stage 2 ticket blocks; the Shipping build of every game
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Composition-table key"; Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Key resolution at configure"

### Z2-R-SEC-088 Fail-closed Shipping key

- **Statement:** A Shipping build without an active key refuses every patch and DLC, a published test key cannot reach a Shipping build, and development builds use TEST 1's key.
- **Covers:** Z2-I-SEC-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Test_SecUpdate_ZeroKeyRefusesEverything`; `CMake_Sec_UpdateKeyResolution`
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Fail-closed defaults"

### Z2-R-SEC-089 Key review path

- **Statement:** `update_keys.json`, `parser_inventory.json`, `path_matrix.json` and `android_permissions.json` are in the Z2 categories' `reviewPaths`, so a change to any of them needs a revision-bound review.
- **Covers:** Z2-I-SEC-08, Z2-I-SEC-05
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-049, Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** the PROC `needs-human` ticket of hand-off SEC-P8 (leaf)
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "update_keys.json"

### Z2-R-SEC-090 Key ceremony

- **Statement:** Every shipped game and release line has a key generated by the ceremony on an offline machine, encrypted under PBES2 with PBKDF2-HMAC-SHA256 of 600,000 iterations and AES-256-CBC, proven decryptable by SHIP's `Get-Zenith2UpdateSigningKey`, stored on two media in two places, and its public record committed.
- **Covers:** Z2-I-SEC-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** the `needs-human` ceremony ticket per (game, release line) and its committed record; SHIP's W10
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Key ceremony"

### Z2-R-SEC-091 Key ceremony tool

- **Statement:** `Ed25519KeyCeremony.java` with `New-Zenith2UpdateKey.ps1` reads the password from a named environment variable, generates and encrypts the key, refuses to overwrite, prints the public key and SHA-256, and zeroes the password and PKCS#8 bytes.
- **Covers:** Z2-I-SEC-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Sec_KeyCeremony_RoundTrip` (`Test` step)
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Ed25519KeyCeremony tool"

### Z2-R-SEC-092 Key states and no in-line rotation

- **Statement:** Keys move through GENERATED, ACTIVE, RETIRED and REVOKED; one key serves one game and release line for the line's whole life, and every key replacement, planned or after a compromise, starts a new release line.
- **Covers:** Z2-I-SEC-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** SC14 on every record change (at most one active record per line, no key reused); the release record of the new line
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Key states"; Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Key replacement"

### Z2-R-SEC-093 Compromise response

- **Statement:** A compromised key is revoked with a reason and replaced by a ceremony and a new base build of a new release line, the DLC that should reach the new line is re-signed for it, and the release notes tell players to update.
- **Covers:** Z2-I-SEC-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** the runbook's dry run recorded at the Stage 6 acceptance (`needs-human`)
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Compromise response"

### Z2-R-SEC-094 Replay and downgrade protection

- **Statement:** A patch for another build, an older patch of the running line and a DLC revision below the installed one are refused, and the downgrade-floor reset by a deleted install state is reported by the install record.
- **Covers:** Z2-I-SEC-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Test_SecUpdate_WrongBuildRefused`, `Test_SecUpdate_ReplayedOlderPatchRefused`; PACK's applier tests
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Replay and downgrade"

### Z2-R-SEC-095 Play-delivered packs verified before mounting

- **Statement:** A fast-follow or on-demand pack mounts only after its archives match the base package's recorded SHA-256 (or its unchanged stamps under the confirmed conditions), and a mismatch leaves it unmounted and reported.
- **Covers:** Z2-I-SEC-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** PACK's asset pack installer tests; the S24 device run of the Stage 6 acceptance
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Play-delivered packs"

### Z2-R-SEC-096 Tamper suite

- **Statement:** The seventeen tamper cases (bit flips in header, file table, strings and signature; wrong key; non-canonical S; zero key; truncation; appended byte; wrong build; wrong release line; replay; changed and swapped archives; unlisted file; stamp-field changes; DLC name) are each rejected with their rule.
- **Covers:** Z2-I-SEC-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Test_SecUpdate_*` in `Zenith_Integration_Security` (`Test` step) and in `Run-Committed.ps1 -Acceptance -Stage 6`
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Tamper suite"; Zenith2/Docs/Design/SEC/Tests.md → "Update tamper tests"

### Z2-R-SEC-097 Development-service inventory

- **Statement:** Every development service, remote tool and development input path is a row D1 to D19 with its owner, Shipping mechanism, forbidden symbols and behaviour test, and a ticket adding one adds its row in the same diff.
- **Covers:** Z2-I-SEC-09, Z2-I-SEC-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** the forbidden-symbol entries of each row (SEC-P5) and the owners' `ShippingTest` cases
- **Design:** Zenith2/Docs/Design/SEC/ShippingExposure.md → "Development-service inventory"

### Z2-R-SEC-098 Debugger absent from Shipping

- **Statement:** No Shipping image contains the Luau DAP server, the Luau compiler or parser, or a `Zenith_ScriptDap` symbol.
- **Covers:** Z2-I-SEC-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Dap_AbsentFromShipping`; the `Libgraph` Shipping symbol and closure checks
- **Design:** Zenith2/Docs/Design/SEC/ShippingExposure.md → "Development-service inventory"

### Z2-R-SEC-099 Remote tools absent from Shipping

- **Statement:** No Shipping image contains the development listener or client, the remote diagnostics server, the development console, the file writer, the frame dump, Tracy, the graph dump reader or the GPU capture tools.
- **Covers:** Z2-I-SEC-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** the owners' `ShippingTest` cases; the `Libgraph` Shipping symbol check
- **Design:** Zenith2/Docs/Design/SEC/ShippingExposure.md → "Development-service inventory"

### Z2-R-SEC-100 Forbidden symbol list format

- **Statement:** `shipping_forbidden_symbols.json` keeps PROC's format (`entries` of `symbol`, `match`, `owner`, `design`), and SEC's consolidated table records each entry's service row and requirement.
- **Covers:** Z2-I-SEC-09, Z2-I-SEC-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-042, Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** the PROC `needs-human` tickets of hand-offs SEC-P5 (leaf, the list) and SEC-P9 (leaf, `Pwsh_Sec_ShippingExposure_ListSchemaRefused`)
- **Design:** Zenith2/Docs/Design/SEC/ShippingExposure.md → "shipping_forbidden_symbols.json schema"

### Z2-R-SEC-101 Symbol normalisation and matching

- **Statement:** Symbols are demangled and normalised to their scope-qualified names and matched exactly or by prefix, any match failing with the entry, owner and service named.
- **Covers:** Z2-I-SEC-09, Z2-I-SEC-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Sec_ShippingExposure_SymbolMatch` (`Test` step)
- **Design:** Zenith2/Docs/Design/SEC/ShippingExposure.md → "Symbol normalisation and matching"

### Z2-R-SEC-102 Consolidated forbidden symbols

- **Statement:** The forbidden-symbol list holds every entry of the consolidated table, CINE's two capture prefixes and NET's `Zenith_NetTransport::Create` included, and PACK's simulated delivery and ONLINE's seams add theirs when declared.
- **Covers:** Z2-I-SEC-09, Z2-I-SEC-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-042, Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** the PROC `needs-human` ticket of hand-off SEC-P5 (leaf), with CINE-P4 and NT9
- **Design:** Zenith2/Docs/Design/SEC/ShippingExposure.md → "Consolidated entries"; Zenith2/Docs/Design/SEC/ShippingExposure.md → "Development-service inventory"

### Z2-R-SEC-103 Forbidden string scan

- **Statement:** No Shipping image contains any byte sequence of `shipping_forbidden_strings.json` (as UTF-8 or UTF-16LE text, or exact bytes), scanned over the whole file.
- **Covers:** Z2-I-SEC-09, Z2-I-SEC-10, Z2-I-SEC-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-042, Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** the PROC `needs-human` ticket of hand-off SEC-P6 (leaf); `Pwsh_Sec_ShippingExposure_StringScan`
- **Design:** Zenith2/Docs/Design/SEC/ShippingExposure.md → "Forbidden string list"; Zenith2/Docs/Design/SEC/ShippingExposure.md → "Initial string entries"

### Z2-R-SEC-104 Listener-import scan

- **Statement:** No Shipping image imports `bind`, `listen`, `accept`, `WSAAccept`, `AcceptEx` (or Winsock ordinals 1, 2, 13) on Windows or `bind`, `listen`, `accept`, `accept4` on Android, until the Stage 7 ticket that answers row SEC17 (c) changes the `bind` rule.
- **Covers:** Z2-I-SEC-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-042, Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** the PROC `needs-human` tickets of hand-offs SEC-P7 (leaf, the gate) and SEC-P9 (leaf, `Pwsh_Sec_ShippingExposure_ImportScan`)
- **Design:** Zenith2/Docs/Design/SEC/ShippingExposure.md → "Listener-import scan"

### Z2-R-SEC-105 Positive controls

- **Statement:** Each scan is run against the Release image of `Minimal` and must find its control items, a missing control failing the scan as broken.
- **Covers:** Z2-I-SEC-09, Z2-I-SEC-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** the PROC `needs-human` tickets of hand-offs SEC-P5 to SEC-P7 (leaf, the `Libgraph` control run) and SEC-P9 (leaf, `Pwsh_Sec_ShippingExposure_PositiveControlMissing`)
- **Design:** Zenith2/Docs/Design/SEC/ShippingExposure.md → "Positive controls"

### Z2-R-SEC-106 Release-time exposure audit tool

- **Statement:** The protected `Zenith2/Tools/Gates/Security/Test-Zenith2ShippingExposure.ps1` runs layers E2 to E4 (or the positive controls) on one image, writes `zenith2.shipping-exposure`, exits 0, 1 or 2, and is the one implementation the gate and SHIP's release checks call.
- **Covers:** Z2-I-SEC-09, Z2-I-SEC-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-042, Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** the PROC `needs-human` ticket of hand-off SEC-P9 (leaf), with the `Pwsh_Sec_ShippingExposure_*` tests and their negative controls
- **Design:** Zenith2/Docs/Design/SEC/ShippingExposure.md → "Test-Zenith2ShippingExposure command line"; Zenith2/Docs/Design/SEC/ShippingExposure.md → "Audit algorithm"; Zenith2/Docs/Design/SEC/ShippingExposure.md → "Audit report"; Zenith2/Docs/Design/SEC/ShippingExposure.md → "Release-time audit tool"

### Z2-R-SEC-107 Release checks run the audit

- **Statement:** SHIP's release verification runs the audit for W5, W16, W17, A6, A15 and A16 on every packaged image and refuses the release on a finding.
- **Covers:** Z2-I-SEC-09, Z2-I-SEC-10
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** SHIP's release check tests extended by row SEC6; the Stage 6 exposure reports
- **Design:** Zenith2/Docs/Design/SEC/ShippingExposure.md → "Gate and release placement"

### Z2-R-SEC-108 Android manifest audit

- **Statement:** A Shipping APK's permissions equal the allowlisted set of its modules and it exports no component beyond the launcher activity and allowlisted ones.
- **Covers:** Z2-I-SEC-10
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** SHIP's A17 and A18 (row SEC6) on every Android package
- **Design:** Zenith2/Docs/Design/SEC/ShippingExposure.md → "Android manifest audit"

### Z2-R-SEC-109 Shipping behaviour composition tests

- **Statement:** In `ShippingTest` the listener refuses for every service name, the loose mount and both host-file opens refuse, and `user:/devtools/` is refused.
- **Covers:** Z2-I-SEC-09, Z2-I-SEC-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_SecShipping_ListenerRefusedForEveryService`, `Test_SecShipping_DevelopmentInputsRefused`, `Test_SecShipping_UserNamespaceDevtoolsRefused` in `win-shipping-test` (`Test` step)
- **Design:** Zenith2/Docs/Design/SEC/ShippingExposure.md → "ShippingTest behaviour cases"; Zenith2/Docs/Design/SEC/Tests.md → "Shipping behaviour tests"

### Z2-R-SEC-110 Stage 6 security hardening evidence

- **Statement:** The Stage 6 acceptance ticket holds every Stage 6 acceptance item of the SEC test design as its security hardening evidence.
- **Covers:** Z2-I-SEC-01, Z2-I-SEC-02, Z2-I-SEC-03, Z2-I-SEC-04, Z2-I-SEC-05, Z2-I-SEC-06, Z2-I-SEC-07, Z2-I-SEC-08, Z2-I-SEC-09, Z2-I-SEC-10
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-083, Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** the acceptance items of `Zenith2/Docs/Design/SEC/Tests.md` marked Stage 6, collected by the Stage 6 acceptance ticket; SHIP's qualification item S1
- **Design:** Zenith2/Docs/Design/SEC/Tests.md → "Acceptance items"; Zenith2/Docs/Design/SEC.md → "Stage 6"

### Z2-R-SEC-111 Stage 2 to 5 security acceptance items

- **Statement:** Every stage acceptance from Stage 2 holds that stage's campaign and, from Stage 3, the HWASan replay and the exposure positive controls.
- **Covers:** Z2-I-SEC-06, Z2-I-SEC-09, Z2-I-SEC-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** each stage acceptance ticket's SEC items
- **Design:** Zenith2/Docs/Design/SEC/Tests.md → "Acceptance items"

### Z2-R-SEC-112 SEC files in ZenithPlatform

- **Statement:** SEC's C++ is exactly `Zenith_UpdateKeyRules.h`, `Internal/Security/Zenith_Ed25519.h` and `.cpp` with their tests and fuzz target, created by SEC's tickets and no other area's, and only `Zenith_Ed25519.cpp` includes the admitted implementation's header.
- **Covers:** Z2-I-SEC-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Libgraph` third-party confinement; `SourceLayout`; PLAT-092 re-pointed by PLAT's row P29
- **Design:** Zenith2/Docs/Design/SEC.md → "SEC files in ZenithPlatform"

### Z2-R-SEC-113 SEC API-only slice

- **Statement:** `Internal/Security/Zenith_Ed25519.h` lands as an API-only ticket in the Stage 5 sprint, before PLAT's verifier ticket.
- **Covers:** Z2-I-SEC-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** `HeaderSelfContained`, `ApiContractCheck` on the slice
- **Design:** Zenith2/Docs/Design/SEC.md → "API-only slices"

### Z2-R-SEC-114 Security tooling directory

- **Statement:** `Zenith2/Tools/Security/` holds only the registries, the triage, layout-audit and key-ceremony scripts, the Java tool and text fixtures, and no logic any gate step runs, and its assert script runs in the `Test` step through `zenith_register_pwsh_test`.
- **Covers:** Z2-I-SEC-05, Z2-I-SEC-06, Z2-I-SEC-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** the `Pwsh_Sec_*` tests of `Test-Zenith2Security.ps1` (`Test` step, label `z2-pwsh`); `SourceLayout`; core-doc row X413 for the layout
- **Design:** Zenith2/Docs/Design/SEC.md → "Tools Security directory"; Zenith2/Docs/Design/SEC/Tests.md → "PowerShell tests"

### Z2-R-SEC-115 Zenith2Security module

- **Statement:** `Zenith2Security.psm1` exports the hex converters and the deduplication and severity functions with the stated parameters, returning collections with `,@(…)` and reading optional fields through `PSObject.Properties`.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** the `Pwsh_Sec_FuzzTriage_*` tests
- **Design:** Zenith2/Docs/Design/SEC.md → "Zenith2Security module"

### Z2-R-SEC-116 Fuzz smoke libFuzzer options

- **Statement:** Every fuzz smoke runs with `-max_total_time`, `-rss_limit_mb=2048`, `-timeout=10` and `-max_len=<maxLen>`, and each smoke whose target has a dictionary passes `-dict=<dict>`.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** SHIP's generic fuzz rule properties (row SEC6); `CMake_Sec_ToolFuzzTargetDeclarations`
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "win-asan with libFuzzer"

### Z2-R-SEC-117 Wildcard and reserved punctuation refused

- **Statement:** Vector V19 (`<`, `>`, `"`, `|`, `?`, `*` in a name) is refused by rule R6 for every runtime source that can express it, each such matrix cell naming its test.
- **Covers:** Z2-I-SEC-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** the V19 cells of the runtime matrix; SC13
- **Design:** Zenith2/Docs/Design/SEC/PathTraversal.md → "Vector catalogue"; Zenith2/Docs/Design/SEC/PathTraversal.md → "Matrix rows for runtime sources"

### Z2-R-SEC-118 NET parsers inventoried

- **Statement:** NET's packet decoder, SNAPSHOT fragment decoder, INPUT decoder, publication payload decoder and identity save section are inventory rows PI-NET-001 to PI-NET-005 at tier 1 with NET's limits headings, each with a fuzz target whose file follows the stem rule.
- **Covers:** Z2-I-SEC-05, Z2-I-SEC-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** SC07, SC08 and SC11 (finding F8 until NET renames its files, row SEC17); the smokes of NET's five targets
- **Design:** Zenith2/Docs/Design/SEC/ParserInventory.md → "Online and network parsers"; Zenith2/Docs/Design/SEC/ParserInventory.md → "Conformance findings"

### Z2-R-SEC-119 Ed25519 harness candidates pinned

- **Statement:** A `needs-human` Stage 1 SHIP ticket pins `monocypher` 4.0.2 and `libsodium` 1.0.20 as harness candidates owned by `Zenith_Bench_Ed25519` with `usage: tools`, and the selection harness ticket is blocked by it.
- **Covers:** Z2-I-SEC-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** SHIP's pin ticket (row SEC15); the harness's configure in every preset
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Selection harness"; Zenith2/Docs/Design/SEC.md → "Stage 1"

### Z2-R-SEC-120 Shared regression replay helper

- **Statement:** `ZenithTestSupport` provides `Zenith_DecodeHexRegression` (rule C5), `Zenith_ForEachRegression` and `Zenith_ReadRegressionCount`, and every `Test_<Stem>_Regressions` replays through them.
- **Covers:** Z2-I-SEC-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** CORE's helper ticket (row SEC18) and its tests
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "HWASan replay on the S24"; Zenith2/Docs/Design/SEC/Fuzzing.md → "Corpus and dictionary rules"

### Z2-R-SEC-121 Cinematic capture absent from Shipping

- **Statement:** The cinematic capture (D20) creates nothing in a Shipping build, and its two entry prefixes are on the forbidden-symbol list.
- **Covers:** Z2-I-SEC-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `Test_CineCapture_AbsentInShipping` (`Test` step, `win-shipping-test`); the `Libgraph` Shipping check once CINE-P4 lands
- **Design:** Zenith2/Docs/Design/SEC/ShippingExposure.md → "Development-service inventory"

### Z2-R-SEC-122 Zenith2SecurityGates module

- **Statement:** The protected `Zenith2/Tools/Gates/Security/Zenith2SecurityGates.psm1` exports the registry readers and the symbol, string and import matchers with the stated parameters, and no other protected step or release script defines a function with the name of one of its exports.
- **Covers:** Z2-I-SEC-05, Z2-I-SEC-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-042, Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** the PROC `needs-human` ticket of hand-off SEC-P9 (leaf); `Test-Security.ps1`, which greps `Zenith2/Tools/Gates/**` and SHIP's release scripts for each exported name
- **Design:** Zenith2/Docs/Design/SEC.md → "Zenith2SecurityGates module"; Zenith2/Docs/Design/SEC.md → "Gates Security directory"

### Z2-R-SEC-123 Committed seeds are published keys

- **Statement:** A diff that commits an Ed25519 seed or private key anywhere in the repository appends its public key to `aauZENITH_PUBLISHED_TEST_KEYS` and to `shipping_forbidden_strings.json` in the same change.
- **Covers:** Z2-I-SEC-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** review of every diff adding a seed (SEC's review focus); `Test_UpdateKeyRules_PublishedKeysDetected`
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Published test keys"

### Z2-R-SEC-124 RFC 8032 vectors checked by a person

- **Statement:** A `needs-human` Stage 6 ticket compares the three seeds, public keys, messages and signatures of the published test keys against RFC 8032 § 7.1 and records the comparison before the API-only slice lands.
- **Covers:** Z2-I-SEC-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** that ticket's recorded comparison
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Published test keys"; Zenith2/Docs/Design/SEC.md → "Stage 6"

### Z2-R-SEC-125 Update keys checked in PowerShell

- **Statement:** The protected `Test-Zenith2UpdateKeys.ps1` checks every rule of the update-key table, including that `publicKey` decodes to the bytes of `publicKeyHex` and that `publicKeySha256` is their SHA-256, imports no module, and both the configure and SC14 run it from Stage 2.
- **Covers:** Z2-I-SEC-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** the PROC `needs-human` ticket of hand-off SEC-P9 (leaf) and `Pwsh_Sec_UpdateKeys_Rules`
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "update_keys.json"; Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Key resolution at configure"

### Z2-R-SEC-126 Fuzz campaign options

- **Statement:** Every campaign run adds `-use_value_profile=1 -print_final_stats=1` to the smoke's options and sets the smoke's `ASAN_OPTIONS` through DEVT's runner.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** DEVT's runner tests extended by row SEC3
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "win-asan with libFuzzer"

### Z2-R-SEC-127 Replay count never vacuous

- **Statement:** Every stem with a `fuzz.stem` commits `Tests/Fuzz/<Stem>.count`, at least 1, equal to its committed regressions plus committed corpus files plus generated seeds, and its `Test_<Stem>_Regressions` fails when the file is missing, is 0 or differs from the replayed total, on every host and on the device.
- **Covers:** Z2-I-SEC-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** SC07 (presence and floor); each library's `Test_<Stem>_Regressions`; the device run record
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "HWASan replay on the S24"

### Z2-R-SEC-128 NET transport absent from Shipping

- **Statement:** NET's Stage 3 transport (D21) creates no socket in a Shipping build, and `Zenith_NetTransport::Create` is on the forbidden-symbol list until Stage 7 lifts the guard.
- **Covers:** Z2-I-SEC-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-042, Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_NetHeadless_ShippingForcesOffline` (`Test` step, `win-shipping-test`); the `Libgraph` Shipping check once NT9 lands
- **Design:** Zenith2/Docs/Design/SEC/ShippingExposure.md → "Development-service inventory"

### Z2-R-SEC-129 Ed25519 differential against the test signer

- **Statement:** 1,000 messages signed by PACK's independent test signer verify through `Zenith_Ed25519Verify`, and their single-bit mutations do not.
- **Covers:** Z2-I-SEC-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Test_SecEd25519_DifferentialAgainstTestSigner`
- **Design:** Zenith2/Docs/Design/SEC/Tests.md → "Integration suite tests"

### Z2-R-SEC-130 Static tool libraries fuzzable in win-asan

- **Statement:** `ZenithAuthoring`, `ZenithEditorCore` and `ZenithNewGameScaffold` are built in `win-asan` as `ZENITH_WINDOWS` targets, so a parser they host is fuzzed through `zenith_tool_fuzz_target` with the library in `DEPENDS`; otherwise its fuzz target lives in the engine library that owns the parsed format.
- **Covers:** Z2-I-SEC-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011, Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** SHIP's preset row (SEC20) and core-doc item X444; SC10 over ANIM's and TWOD's calls
- **Design:** Zenith2/Docs/Design/SEC/Fuzzing.md → "Tool-side fuzz targets"

### Z2-R-SEC-131 One published-key list

- **Statement:** The published update keys are exactly the `hex` entries of `shipping_forbidden_strings.json` whose `id` ends `-public-key`; the configure and the PowerShell key check read that list, and the C++ array `aauZENITH_PUBLISHED_TEST_KEYS` holds the same keys in the same order.
- **Covers:** Z2-I-SEC-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Sec_PublishedKeysAgree` (`Test` step); `Pwsh_Sec_UpdateKeys_Rules`
- **Design:** Zenith2/Docs/Design/SEC/UpdateAuthentication.md → "Published test keys"

## Coverage of inventory items

| Item | Requirements |
|---|---|
| Z2-I-SEC-01 | Z2-R-SEC-001, Z2-R-SEC-002, Z2-R-SEC-003, Z2-R-SEC-008, Z2-R-SEC-023, Z2-R-SEC-110 |
| Z2-I-SEC-02 | Z2-R-SEC-001, Z2-R-SEC-002, Z2-R-SEC-003, Z2-R-SEC-004, Z2-R-SEC-006, Z2-R-SEC-007, Z2-R-SEC-016, Z2-R-SEC-110 |
| Z2-I-SEC-03 | Z2-R-SEC-001, Z2-R-SEC-002, Z2-R-SEC-009, Z2-R-SEC-010, Z2-R-SEC-011, Z2-R-SEC-012, Z2-R-SEC-013, Z2-R-SEC-017, Z2-R-SEC-110 |
| Z2-I-SEC-04 | Z2-R-SEC-001, Z2-R-SEC-002, Z2-R-SEC-005, Z2-R-SEC-018, Z2-R-SEC-019, Z2-R-SEC-020, Z2-R-SEC-021, Z2-R-SEC-022, Z2-R-SEC-023, Z2-R-SEC-110 |
| Z2-I-SEC-05 | Z2-R-SEC-003, Z2-R-SEC-006, Z2-R-SEC-014, Z2-R-SEC-015, Z2-R-SEC-024, Z2-R-SEC-025, Z2-R-SEC-026, Z2-R-SEC-027, Z2-R-SEC-028, Z2-R-SEC-029, Z2-R-SEC-030, Z2-R-SEC-031, Z2-R-SEC-032, Z2-R-SEC-033, Z2-R-SEC-034, Z2-R-SEC-035, Z2-R-SEC-036, Z2-R-SEC-089, Z2-R-SEC-110, Z2-R-SEC-114, Z2-R-SEC-118, Z2-R-SEC-122 |
| Z2-I-SEC-06 | Z2-R-SEC-024, Z2-R-SEC-026, Z2-R-SEC-030, Z2-R-SEC-032, Z2-R-SEC-033, Z2-R-SEC-035, Z2-R-SEC-036, Z2-R-SEC-037, Z2-R-SEC-038, Z2-R-SEC-039, Z2-R-SEC-040, Z2-R-SEC-041, Z2-R-SEC-042, Z2-R-SEC-043, Z2-R-SEC-044, Z2-R-SEC-045, Z2-R-SEC-046, Z2-R-SEC-047, Z2-R-SEC-048, Z2-R-SEC-049, Z2-R-SEC-050, Z2-R-SEC-051, Z2-R-SEC-052, Z2-R-SEC-053, Z2-R-SEC-054, Z2-R-SEC-055, Z2-R-SEC-056, Z2-R-SEC-057, Z2-R-SEC-058, Z2-R-SEC-059, Z2-R-SEC-081, Z2-R-SEC-110, Z2-R-SEC-111, Z2-R-SEC-114, Z2-R-SEC-115, Z2-R-SEC-116, Z2-R-SEC-118, Z2-R-SEC-120, Z2-R-SEC-126, Z2-R-SEC-127, Z2-R-SEC-130 |
| Z2-I-SEC-07 | Z2-R-SEC-033, Z2-R-SEC-060, Z2-R-SEC-061, Z2-R-SEC-062, Z2-R-SEC-063, Z2-R-SEC-064, Z2-R-SEC-065, Z2-R-SEC-066, Z2-R-SEC-067, Z2-R-SEC-068, Z2-R-SEC-069, Z2-R-SEC-070, Z2-R-SEC-071, Z2-R-SEC-072, Z2-R-SEC-073, Z2-R-SEC-110, Z2-R-SEC-117 |
| Z2-I-SEC-08 | Z2-R-SEC-022, Z2-R-SEC-065, Z2-R-SEC-074, Z2-R-SEC-075, Z2-R-SEC-076, Z2-R-SEC-077, Z2-R-SEC-078, Z2-R-SEC-079, Z2-R-SEC-080, Z2-R-SEC-081, Z2-R-SEC-082, Z2-R-SEC-083, Z2-R-SEC-084, Z2-R-SEC-085, Z2-R-SEC-086, Z2-R-SEC-087, Z2-R-SEC-088, Z2-R-SEC-089, Z2-R-SEC-090, Z2-R-SEC-091, Z2-R-SEC-092, Z2-R-SEC-093, Z2-R-SEC-094, Z2-R-SEC-095, Z2-R-SEC-096, Z2-R-SEC-103, Z2-R-SEC-110, Z2-R-SEC-112, Z2-R-SEC-113, Z2-R-SEC-119, Z2-R-SEC-123, Z2-R-SEC-124, Z2-R-SEC-125, Z2-R-SEC-129, Z2-R-SEC-131 |
| Z2-I-SEC-09 | Z2-R-SEC-097, Z2-R-SEC-098, Z2-R-SEC-100, Z2-R-SEC-101, Z2-R-SEC-102, Z2-R-SEC-103, Z2-R-SEC-105, Z2-R-SEC-106, Z2-R-SEC-107, Z2-R-SEC-109, Z2-R-SEC-110, Z2-R-SEC-111, Z2-R-SEC-121, Z2-R-SEC-128 |
| Z2-I-SEC-10 | Z2-R-SEC-065, Z2-R-SEC-069, Z2-R-SEC-072, Z2-R-SEC-097, Z2-R-SEC-099, Z2-R-SEC-100, Z2-R-SEC-101, Z2-R-SEC-102, Z2-R-SEC-103, Z2-R-SEC-104, Z2-R-SEC-105, Z2-R-SEC-106, Z2-R-SEC-107, Z2-R-SEC-108, Z2-R-SEC-109, Z2-R-SEC-110, Z2-R-SEC-111, Z2-R-SEC-114, Z2-R-SEC-122 |

## Excluded and deferred

No SEC requirement is excluded or deferred: all ten inventory items are required. Out of scope by settled decision, and therefore carrying no requirement: modding and third-party content (Z2-D-075), dynamic plugins (Z2-D-024), and networked multiplayer integrity beyond the Stage 3 loopback (Z2-D-073, deferred with NET). Anti-cheat, DRM and save authentication are non-goals of the single-player scope (`Zenith2/Docs/Design/SEC/TrustBoundaries.md` → "Non-goals") and are not inventory items.
