# SCOPE — requirement catalogue

## Purpose

This catalogue breaks the SCOPE inventory items of `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1" into testable requirements. The design they trace to is `Zenith2/Docs/Design/SCOPE.md` and its section files; each requirement carries exactly the eight labelled lines of the catalogue format. SCOPE's own rulings are cited by their labels Z2-D-133 to Z2-D-140 (`Zenith2/Docs/Design/SCOPE.md` → "Rulings this design makes") until their decision-log entries are appended.

## Requirements

**Group: Scope records and the register.** The Stage 1 scope records, the scope register and the drafting obligations on the deferred trees.

### Z2-R-SCOPE-001 Scope register file

- **Statement:** `Zenith2/Docs/Scope/ScopeRegister.json` exists with schema `zenith2.scope-register` version 1 and holds one boundary row for each of consoles, xr, modding, local-multiplayer, dedicated-servers, online-backends, console-xr and xr-platforms, each with its classification, stage, decisions, rulings, owner, inventory items, explicit requirement IDs or ID ranges of named catalogues, design headings and enforcement entries, plus a `meta` list, so that every SCOPE requirement is attributed exactly once.
- **Covers:** Z2-I-SCOPE-10
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-070, Z2-D-071, Z2-D-072, Z2-D-075, Z2-D-076
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_ScopeRegister_Valid` in the `Test` step (label `z2-cmake`)
- **Design:** `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "The scope register"

### Z2-R-SCOPE-002 Register validity check

- **Statement:** `Pwsh_ScopeRegister_Valid` runs the twelve steps of "Register checks" over the tracked register on every `Test` run, reading only `### Z2-R-` headings and their Stage and Classification lines (never group lines), accepting a ruling as either an `SC-R<n>` label of the rulings table or a decision-log `Z2-D-NNN` heading, and fails, naming the JSON pointer and the step, on any violation, reporting every violation before it exits.
- **Covers:** Z2-I-SCOPE-10, Z2-I-SCOPE-11
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-070
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_ScopeRegister_Valid` and `Pwsh_ScopeRegister_ValidFixtures` (one generated fixture per failing step) in the `Test` step
- **Design:** `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Register checks"

### Z2-R-SCOPE-003 Deferred boundaries carry a requirement tree

- **Statement:** Every register boundary classified `deferred` has at least one requirement in its catalogue at its stage classified `deferred` or `required`, every `excluded` boundary at least one `excluded` requirement with a decision and an enforcement entry, and every `research` boundary a `research` requirement; register check step 9 fails otherwise.
- **Covers:** Z2-I-SCOPE-11
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-070
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_ScopeRegister_ValidFixtures` cases for steps 9 and 10; the coverage gate step
- **Design:** `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Deferred scopes and their trees"; `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Register checks"

### Z2-R-SCOPE-004 Consoles scope record

- **Statement:** A Stage 1 `needs-human` decision ticket confirms the consoles record (options, chosen option 3, reason) as the record of Z2-D-071 and appends ruling Z2-D-133 to the decision log when not yet appended, appending the overlay entry only when the human has approved flagged proposal SCOPE-1 (pending Q98), and replaces the register row's rulings by their entry IDs.
- **Covers:** Z2-I-SCOPE-01, Z2-I-SCOPE-10
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-071
- **Budgets:** none
- **Acceptance evidence:** the Stage 1 consoles decision ticket's diff; `Pwsh_ScopeRegister_Valid` after it
- **Design:** `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Consoles record"; `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Stage 1 SCOPE decision tickets"

### Z2-R-SCOPE-005 XR scope record

- **Statement:** A Stage 1 `needs-human` decision ticket confirms the XR record as the record of Z2-D-072 and appends rulings Z2-D-136 to Z2-D-139 when not yet appended, and SC-R3 once the human has confirmed the XR narrowing and the console-headset exclusion (pending Q99), replacing the register row's rulings by their entry IDs.
- **Covers:** Z2-I-SCOPE-04, Z2-I-SCOPE-10
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** the Stage 1 XR decision ticket's diff; `Pwsh_ScopeRegister_Valid` after it
- **Design:** `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "XR record"; `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Stage 1 SCOPE decision tickets"

### Z2-R-SCOPE-006 Modding scope record

- **Statement:** A Stage 1 `needs-human` decision ticket confirms the modding record as the record of Z2-D-075 and the register row names its enforcement entries [Z2-N-ARCH-262], [Z2-N-ARCH-264] and [Z2-N-ARCH-274].
- **Covers:** Z2-I-SCOPE-08, Z2-I-SCOPE-10
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-075
- **Budgets:** none
- **Acceptance evidence:** the Stage 1 modding decision ticket's diff; register check step 10
- **Design:** `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Modding record"

### Z2-R-SCOPE-007 Local multiplayer scope record

- **Statement:** A Stage 1 `needs-human` decision ticket confirms the local multiplayer record as the record of Z2-D-076 and the register row names PLAT's slot-0 rule, RPROD's single main view and `Test_ScopeExclusion_LocalDevicesShareSlotZero` as enforcement.
- **Covers:** Z2-I-SCOPE-09, Z2-I-SCOPE-10
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-076
- **Budgets:** none
- **Acceptance evidence:** the Stage 1 local multiplayer decision ticket's diff; register check step 10
- **Design:** `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Local multiplayer record"

### Z2-R-SCOPE-008 Dedicated servers scope record

- **Statement:** A Stage 1 `needs-human` decision ticket confirms the dedicated servers record as the record of Z2-D-073, with NET named as the tree's owner and the register row listing NET's Stage 7 requirements Z2-R-NET-108 to Z2-R-NET-158 explicitly.
- **Covers:** Z2-I-SCOPE-10
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** the Stage 1 dedicated servers decision ticket's diff; `Pwsh_ScopeRegister_Valid` steps 7 and 9
- **Design:** `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Dedicated servers record"

### Z2-R-SCOPE-009 Online backends scope record

- **Statement:** A Stage 1 `needs-human` decision ticket confirms the online backends record as the record of Z2-D-074, with ONLINE named as the tree's owner, the register row listing ONLINE's Stage 6 backend requirements explicitly, and the console platform-services backends assigned to SCOPE's console tree.
- **Covers:** Z2-I-SCOPE-10
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** the Stage 1 online backends decision ticket's diff; `Pwsh_ScopeRegister_Valid`
- **Design:** `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Online backends record"

### Z2-R-SCOPE-010 Register landing ticket

- **Statement:** An ordinary Stage 1 implementation ticket lands the register, both register checks, their fixture generator and their registrations in `Zenith2/CMake/Tests/ZenithCMakeTests.cmake`, and the six scope decision tickets are blocked by it.
- **Covers:** Z2-I-SCOPE-10
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-070
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_ScopeRegister_Valid`, `Pwsh_ScopeRegister_ValidFixtures` registered and passing in the `Test` step
- **Design:** `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Stage 1 SCOPE decision tickets"; `Zenith2/Docs/Design/SCOPE.md` → "Repository files outside ZenithXR"

### Z2-R-SCOPE-011 Console tickets carry no NDA material

- **Statement:** Every console ticket, design section and tracked file of the main repository names only Zenith2 types, seams, neutral behaviours and neutral certification categories, and never a vendor SDK identifier, figure, file format, path or requirement text; this is enforced by review alone, because no gate can hold NDA vocabulary.
- **Covers:** Z2-I-SCOPE-02
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-071
- **Budgets:** none
- **Acceptance evidence:** review of each console ticket (A5 and A7); no mechanical check exists or is claimed
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "What the main repository may hold"

### Z2-R-SCOPE-012 Console tickets blocked by their platform's SDK-access ticket

- **Statement:** Every console ticket of a platform is `blocks`-linked from that platform's SDK-access ticket with the reason `backlog: <platform> SDK access`, and from no other platform's.
- **Covers:** Z2-I-SCOPE-03
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-071
- **Budgets:** none
- **Acceptance evidence:** the coverage gate step's link and dependency checks over the filed board
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "SDK-access tickets"

### Z2-R-SCOPE-013 XR tickets specified against OpenXR 1.1

- **Statement:** Every Stage 7 XR ticket names the OpenXR 1.1 functions, structures and enumerants it realises, as this family names them, and calls no vendor XR API.
- **Covers:** Z2-I-SCOPE-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** review of each XR ticket (A5); `Test_XrSeam_ApiVersionRefused` at Stage 7
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "OpenXR version and extensions"; `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "The OpenXR API seam"

### Z2-R-SCOPE-014 SDK access gates only its own platform

- **Statement:** A console platform whose SDK-access ticket is not Done blocks only that platform's tickets; the XR, networking and ray-tracing expansions and the other consoles start without it.
- **Covers:** Z2-I-SCOPE-03, Z2-I-SCOPE-11
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-071
- **Budgets:** none
- **Acceptance evidence:** the Roadmap's Stage 7 entry criteria as clarified by the SCOPE cross-doc item; the coverage gate step finds no cross-platform SDK link
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Console platform set"; `Zenith2/Docs/Design/SCOPE/Consoles.md` → "SDK-access tickets"

**Group: Exclusions.** Excluded and research boundaries, each with its rationale and decision.

### Z2-R-SCOPE-015 Modding excluded

- **Statement:** Zenith2 has no third-party mod loading of code, scripts, content packs or data overrides (rationale: each option contradicts static modules, in-repo games, per-version recooks or the sandbox's trust boundary).
- **Covers:** Z2-I-SCOPE-08
- **Stage:** —
- **Classification:** excluded
- **Decisions:** Z2-D-075
- **Budgets:** none
- **Acceptance evidence:** no ticket; enforcement [Z2-N-ARCH-262], [Z2-N-ARCH-264], [Z2-N-ARCH-274] through `ForbiddenCheck`
- **Design:** `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Modding record"

### Z2-R-SCOPE-016 Local multiplayer and split-screen excluded

- **Statement:** Zenith2 has no second local player, split-screen or shared-screen co-op; every local device belongs to player slot 0 and render views are never per-player (rationale: per-player views, input, UI and audio in every system for no flagship slice).
- **Covers:** Z2-I-SCOPE-09
- **Stage:** —
- **Classification:** excluded
- **Decisions:** Z2-D-076
- **Budgets:** none
- **Acceptance evidence:** no ticket; enforcement by PLAT's slot-0 rule, RPROD's single main view and `Test_ScopeExclusion_LocalDevicesShareSlotZero`
- **Design:** `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Local multiplayer record"

### Z2-R-SCOPE-017 Console headsets excluded

- **Statement:** Zenith2 supports no console's own headset (rationale: it needs a console SDK and a second, non-OpenXR integration, the per-vendor coupling the XR record rejects; ruling SC-R3, pending Q99, which also carries the human's A4 classification of this exclusion beyond the inventory).
- **Covers:** Z2-I-SCOPE-04
- **Stage:** —
- **Classification:** excluded
- **Decisions:** Z2-D-072, Z2-D-071
- **Budgets:** none
- **Acceptance evidence:** no ticket; `Test_XrModule_AndroidPlatformRefused` (its console case: `XR` with `playstation5` is refused from the declared platform list, overlay present or not)
- **Design:** `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "XR record"; `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Console build presets and game.json"

### Z2-R-SCOPE-018 Standalone headsets and passthrough ruled by D-XR-PLATFORMS

- **Statement:** Whether Stage 7 adds a standalone Android OpenXR headset as a second validated Android device, and passthrough with a non-opaque blend mode, is decided by D-XR-PLATFORMS (open, Stage 7) through a harness build of WorkloadIndoor on the headset and a `needs-human` ruling, never by an implementation ticket.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** research
- **Decisions:** Z2-D-072, Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** the D-XR-PLATFORMS harness ticket's CTest smoke and the ruling ticket's report under `Zenith2/Docs/Research/Scope/`
- **Design:** `Zenith2/Docs/Design/SCOPE.md` → "Open decisions SCOPE owns"

**Group: XR readiness.** The checks Stage 2 to Stage 6 tickets satisfy so Stage 7 XR adds code without changing an earlier contract.

### Z2-R-SCOPE-019 Readiness register rows and presence check

- **Statement:** The register lists every readiness check of the XR readiness design with its owner heading, stage and tests, and `Pwsh_ScopeRegister_ReadinessTestsPresent` fails a stage-N acceptance run when a test of a row required by stage N is absent or declared more than once.
- **Covers:** Z2-I-SCOPE-06, Z2-I-SCOPE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_ScopeRegister_ReadinessTestsPresent` (label `z2-acceptance-scope`) in every stage acceptance from Stage 2
- **Design:** `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "How a readiness check binds a ticket"; `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Register checks"

### Z2-R-SCOPE-020 XRR-V1 stereo kind reserved

- **Statement:** From Stage 2 `VIEW_KIND_STEREO` is declared and rejected by `Flux_BuildViewList` with `FLUX_VIEW_ERROR_KIND_UNAVAILABLE`, counted, with every other view of the frame unaffected, until Stage 7 replaces the check.
- **Covers:** Z2-I-SCOPE-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_View_StereoRejected` (RPROD) in the `Test` step; its presence at acceptance
- **Design:** `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Multi-view checks"

### Z2-R-SCOPE-021 XRR-V2 independent view histories

- **Statement:** From Stage 2 each view's persistent histories are its own, so adding a view never changes another view's TAA history.
- **Covers:** Z2-I-SCOPE-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-072, Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_View_EditorHistoryIndependentOfMain` (RPROD)
- **Design:** `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Multi-view checks"

### Z2-R-SCOPE-022 XRR-V3 distinct stable eye view IDs

- **Statement:** From Stage 2 view IDs of (`VIEW_KIND_STEREO`, owner, 0), (…, 1) and (`VIEW_KIND_MAIN`, owner, 0) are pairwise distinct and stable across frames and call order for 1,000 owners.
- **Covers:** Z2-I-SCOPE-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-072, Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_ScopeMultiView_StereoEyeIdsDistinct` and `Test_View_IDStableAcrossListOrder` in the `Test` step
- **Design:** `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Multi-view checks"; `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Readiness tests SCOPE owns"

### Z2-R-SCOPE-023 XRR-V4 two-view group without cross-talk

- **Statement:** From Stage 5 a two-view group renders each view identically to rendering it alone while sharing culling.
- **Covers:** Z2-I-SCOPE-06
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_View_GroupSharesCullingWithoutCrossTalk` (RPROD)
- **Design:** `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Multi-view checks"

### Z2-R-SCOPE-024 XRR-V5 off-centre projection parity

- **Statement:** From Stage 5 every shading path renders off-centre views correctly: with the neighbourhood-wide view features cleared, two half-views with off-centre frusta equal the halves of the symmetric view within FLIP 0.02 outside an 8-pixel seam band, and depth within 1 ULP of D24.
- **Covers:** Z2-I-SCOPE-06
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_ScopeMultiView_SplitFrustumCropParity` on WARP and lavapipe in the `Test` step
- **Design:** `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Multi-view checks"; `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Readiness tests SCOPE owns"

### Z2-R-SCOPE-025 XRR-V6 output texture rotation

- **Statement:** From Stage 5 a texture view may name a different output texture each frame from a ring of three, each frame writing the texture it names, with histories keyed on the view ID alone and a released texture rejected with `FLUX_VIEW_ERROR_INVALID_REQUEST`.
- **Covers:** Z2-I-SCOPE-06
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-072, Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_ScopeMultiView_OutputTextureRotation` in the `Test` step
- **Design:** `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Multi-view checks"; `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Readiness tests SCOPE owns"

### Z2-R-SCOPE-026 XRR-V7 one pacing wait bounds run-ahead

- **Statement:** From Stage 3 the frame loop's run-ahead is bounded by the one pacing wait on the render-submit thread, so a frame source can take that wait's place.
- **Covers:** Z2-I-SCOPE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-072, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_FramePacer_LatencySlotBoundsRunAhead` (RPROD)
- **Design:** `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Multi-view checks"

### Z2-R-SCOPE-027 XRR-V8 world-space panels

- **Statement:** From Stage 4 world-space UI panels map canvas points to world points and hit-test rays within 0.01 canvas units.
- **Covers:** Z2-I-SCOPE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_UIWorldPanel_CanvasToWorld` (UI)
- **Design:** `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Multi-view checks"

### Z2-R-SCOPE-028 XRR-O1 UI composite into a supplied target

- **Statement:** From Stage 3 the UI composite draws into the target its view supplies, including a texture-output view, scaling by the canvas-to-output ratio.
- **Covers:** Z2-I-SCOPE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_ScopeMultiView_UiCompositeIntoTextureView` in the `Test` step
- **Design:** `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Other subsystems XR touches"; `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Readiness tests SCOPE owns"

### Z2-R-SCOPE-029 XRR-I1 scripts read actions only

- **Statement:** From Stage 3 the `Input` Luau namespace exposes actions and contexts only and no device read.
- **Covers:** Z2-I-SCOPE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_InputScriptApi_ExposesActionsOnly` (PLAT)
- **Design:** `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Input checks"

### Z2-R-SCOPE-030 XRR-I2 XR control paths reserved

- **Statement:** From Stage 3 `/xr/<left|right|head>/<control>` paths parse and a binding naming one fails with `INPUT_ERROR_UNSUPPORTED_DEVICE_KIND`, until Stage 7 replaces the check.
- **Covers:** Z2-I-SCOPE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_ControlPath_XrPathsReserved` (PLAT)
- **Design:** `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Input checks"

### Z2-R-SCOPE-031 XRR-I3 POSE action type reserved

- **Statement:** From Stage 3 a `Pose` action is refused by the bindings loader with `INPUT_ERROR_UNSUPPORTED_ACTION_TYPE` and the reserved POSE command encoding decodes to a refused, counted command, until Stage 7 replaces the check.
- **Covers:** Z2-I-SCOPE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-072, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ScopeInput_PoseActionTypeReserved` in the `Test` step
- **Design:** `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Input checks"; `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Readiness tests SCOPE owns"

### Z2-R-SCOPE-032 XRR-I4 runtime-created devices

- **Statement:** From Stage 3 input devices created at run time deliver their controls as transitions in call order and destroying one synthesises releases.
- **Covers:** Z2-I-SCOPE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualDevice_CreateSetDestroy` (PLAT)
- **Design:** `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Input checks"

### Z2-R-SCOPE-033 XRR-I5 simulation haptics through commands

- **Statement:** From Stage 3 simulation haptics reach devices only through committed commands, and a rolled-back script plays nothing.
- **Covers:** Z2-I-SCOPE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_Haptics_CommandsRolledBackPlayNothing` (PLAT)
- **Design:** `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Input checks"

### Z2-R-SCOPE-034 Local devices share player slot zero

- **Statement:** From Stage 3 every input command produced from local devices carries player slot 0, whichever local device produced it.
- **Covers:** Z2-I-SCOPE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-076
- **Budgets:** none
- **Acceptance evidence:** `Test_ScopeExclusion_LocalDevicesShareSlotZero` in the `Test` step
- **Design:** `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Exclusion checks SCOPE owns"

### Z2-R-SCOPE-035 Readiness retirement at Stage 7

- **Statement:** The Stage 7 tickets that implement the stereo kind and the XR input additions replace the retiring readiness tests with the names of the retirement table in the register and the test tree in the same diff.
- **Covers:** Z2-I-SCOPE-06, Z2-I-SCOPE-07
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_ScopeRegister_ReadinessTestsPresent` in the Stage 7 acceptance
- **Design:** `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Retirement at Stage 7"

**Group: XR module and startup.** The Stage 7 ZenithXR module, its loader and the pre-device provider.

### Z2-R-SCOPE-036 ZenithXR module library

- **Statement:** `ZenithXR` is a `KIND MODULE` library with `MODULE_NAME XR`, the 14th module of Architecture's module list with its library-census row and its `openxr-loader` third-party row, supported on `ZENITH_WINDOWS` alone, whose `DEPENDS` and root headers are exactly those of the design, and whose OpenXR headers appear only in `Internal/Windows/Zenith_OpenXrApiLoader_Windows.cpp`.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072, Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Libgraph`, `SourceLayout`, `CompileCoverage`, `HeaderSelfContained`; `Test_XrModule_StartRegistersInOrder`
- **Design:** `Zenith2/Docs/Design/SCOPE.md` → "Targets"; `Zenith2/Docs/Design/SCOPE.md` → "ZenithXR files"; `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Header list"

### Z2-R-SCOPE-037 OpenXR loader dependency

- **Statement:** The OpenXR loader is pinned as the manifest entry `openxr-loader`, built as a shared library linked through its import library and staged beside the executable, so no Zenith2 code calls `LoadLibrary*` for XR.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-082, Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `ForbiddenCheck` ([Z2-N-ARCH-274]); `Libgraph` third-party confinement; `Test_XrRuntime_SessionReachesFocused` loads the staged loader
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "The OpenXR API seam"

### Z2-R-SCOPE-038 XR on Windows only

- **Statement:** A `game.json` listing `XR` with `android` or a console platform fails the configure with `Games/<Name>/game.json: modules: XR is supported on windows only`.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072, Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_XrModule_AndroidPlatformRefused` (a `z2-cmake` fixture)
- **Design:** `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Zenith_Module_XR header"

### Z2-R-SCOPE-039 Device-interop provider before the device

- **Statement:** With `XR` selected, the factory named by the optional `m_pfnCreateDeviceInterop` member of the XR module's descriptor is called as the last action of startup step 11, creates the OpenXR instance and system, fills the RHI interop request inside step 12 before the factory call and receives the device's interop handles after it, adding no startup step, no enumerator and no module entry point.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_XrInterop_VulkanCallbacksCreateDevice`, `Test_XrInterop_D3D12AdapterLuid`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Startup and the device-interop provider"; `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Interfaces of lower libraries"

### Z2-R-SCOPE-040 Missing runtime falls back to mono

- **Statement:** With no OpenXR loader or runtime, no headset, a missing extension or version, or in a headless or replaying run, or with `xr.enabled` false, startup never fails: the provider is disabled or waiting, the device is created as without XR, stereo cameras render mono and the exit code is unchanged; an invalid `xr.*` key or scripted-runtime document is exit code 2 at step 11 and an allocation failure of the provider exit code 4.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrModule_DisabledWhenNoRuntime`, `Test_XrModule_DisabledInHeadlessAndReplay`
- **Design:** `Zenith2/Docs/Design/SCOPE.md` → "Failure handling"; `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Startup and the device-interop provider"

### Z2-R-SCOPE-041 Startup unwinds every step

- **Statement:** An injected failure at each of the provider steps P2 to P9 and the session steps S1 to S4 leaves every handle created before it destroyed and the documented state.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrSession_CreateFailsAtStep_Unwinds`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Startup and the device-interop provider"

**Group: XR instance and graphics.** Instance, system and the two graphics bindings.

### Z2-R-SCOPE-042 Instance creation against OpenXR 1.1

- **Statement:** The instance is created with API version 1.1.0, the game's name and version as application info and exactly the extensions of the extension table for the selected backend, and a runtime without 1.1 disables XR with `ZENITH_XR_ERROR_API_VERSION_UNSUPPORTED`.
- **Covers:** Z2-I-SCOPE-04, Z2-I-SCOPE-05
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrSeam_ApiVersionRefused`, `Test_XrSeam_ExtensionCheckPerBackend`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "OpenXR version and extensions"

### Z2-R-SCOPE-043 Head-mounted stereo system

- **Statement:** The system is the head-mounted display form factor with the primary stereo view configuration of exactly two views and the opaque blend mode; a runtime without a headset enters `RETRY_WAIT`, retries `xrGetSystem` every `xr.retrySeconds` and, when the headset appears, recreates the device before the session.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrSession_HeadsetAfterLaunchRecreatesDevice`, `Test_XrLayers_OpaqueBlendModeOnly`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Startup and the device-interop provider"

### Z2-R-SCOPE-044 Vulkan graphics binding

- **Statement:** On Vulkan the runtime creates the instance and device through `XR_KHR_vulkan_enable2` callbacks the RHI calls, the RHI uses the physical device the runtime names, the API version lies in the runtime's range, and the session binds the graphics queue family and queue 0.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072, Z2-D-013
- **Budgets:** none
- **Acceptance evidence:** `Test_XrInterop_VulkanCallbacksCreateDevice` on lavapipe
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Graphics bindings"

### Z2-R-SCOPE-045 D3D12 graphics binding

- **Statement:** On D3D12 the RHI selects the adapter with the LUID of `xrGetD3D12GraphicsRequirementsKHR` at or above its minimum feature level, overriding `rhi.adapter` with a log line, and the session binds the device and the graphics queue; a LUID matching no adapter disables XR.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072, Z2-D-013
- **Budgets:** none
- **Acceptance evidence:** `Test_XrInterop_D3D12AdapterLuid` on WARP
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Graphics bindings"

### Z2-R-SCOPE-046 Graphics queue rule

- **Statement:** Every OpenXR call that may submit to the bound queue (`xrEndFrame`, `xrWaitSwapchainImage`, `xrReleaseSwapchainImage`) runs on the render-submit thread, the only thread that submits to the graphics queue, and no other queue is bound.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrFrame_CycleOrderPerFrame` with `Zenith_XrCallGuard`; `Test_XrSeam_CallGuardDetectsOverlap`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Graphics bindings"; `Zenith2/Docs/Design/SCOPE.md` → "Frame placement"

**Group: XR session.** The session lifecycle, events and recovery.

### Z2-R-SCOPE-047 Session state machine

- **Statement:** The session follows every row of the session state table: `xrBeginSession` and `xrEndSession` issued in the prologue that observes READY and STOPPING, frame pacing by the runtime exactly from READY to STOPPING, and one `XR.SessionChanged` per transition.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrSession_StateMachineTable`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Session state machine"

### Z2-R-SCOPE-048 Bounded event polling

- **Statement:** Each prologue polls at most 64 OpenXR events, handles every event type of the event table, and defers the rest to the next prologue, counted.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrSession_EventsBoundedPerPrologue`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Event handling"

### Z2-R-SCOPE-049 Focus loss pauses worlds

- **Statement:** With `xr.pauseWhenUnfocused` true, leaving FOCUSED adds `WORLD_PAUSE_REASON_XR_UNFOCUSED` to every live world and re-entering FOCUSED removes it.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_XrSession_UnfocusedAddsPauseReason`; `Device_Xr_FocusAndRemoval`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Session state machine"

### Z2-R-SCOPE-050 Session loss retries

- **Statement:** A session loss destroys every session handle and the XR controller devices with releases, renders mono, and retries session creation every `xr.retrySeconds` without blocking a frame.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrSession_LossRetriesAfterInterval`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Loss recovery and shutdown"

### Z2-R-SCOPE-051 Instance loss recreates instance then device

- **Statement:** An instance loss destroys the session and instance; every `xr.retrySeconds` a new instance and system are tried, and once they exist the device is recreated through RHI's recovery path so it is the runtime's device, before a session is created.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrSession_InstanceLossRecreatesInstanceThenDevice`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Loss recovery and shutdown"

### Z2-R-SCOPE-052 Orderly XR shutdown

- **Statement:** At shutdown step 6 the frame source is cleared at a render-idle point, the session is asked to exit and waited for at most `xr.shutdownWaitMs`, imports are released before their swapchains; in step 9 the provider is told before and after the device's destruction and destroys the instance after the device.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrSwapchain_ImagesImportedOnceAndReleased` (no live handle at device destruction); `Test_XrRuntime_SessionReachesFocused` (clean end)
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Loss recovery and shutdown"; `Zenith2/Docs/Design/SCOPE.md` → "Startup and shutdown"

### Z2-R-SCOPE-053 Result mapping

- **Statement:** Every OpenXR result ZenithXR can receive maps to the outcome of the result-mapping table, and order-violation results also assert in debug builds.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_XrSeam_ScriptedRuntimeEnforcesCallOrder` and the failure cases of the session and frame tests
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Result mapping"

**Group: XR frame cycle.** One XR frame per engine frame, its placement and its pacing.

### Z2-R-SCOPE-054 Frame cycle placement

- **Statement:** Each XR frame k runs `xrWaitFrame` at the present of frame k − 1, `xrLocateViews` and image acquisition on the build node of frame k, `xrBeginFrame` and image waits at its submit, and image release and `xrEndFrame` at its present, exactly once each.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_XrFrame_CycleOrderPerFrame` on WARP and lavapipe
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Frame cycle"; `Zenith2/Docs/Design/SCOPE.md` → "Frame placement"

### Z2-R-SCOPE-055 Display time from the waited frame

- **Statement:** Each frame's `xrEndFrame` display time and its `xrLocateViews` time equal the predicted display time of that frame's `xrWaitFrame`.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrFrame_DisplayTimeFromWaitFrame`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Frame cycle"

### Z2-R-SCOPE-056 Not-rendered and skipped frames

- **Statement:** A frame with `shouldRender` false, an invalid eye pose or an image-wait timeout still begins and ends with zero layers, releases every acquired image and renders mono, counted.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrFrame_ShouldRenderFalseSubmitsZeroLayers`, `Test_XrFrame_ImageWaitTimeoutSkipsFrame`, `Test_XrFrame_InvalidPoseFallsBackToMono`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Frame cycle"

### Z2-R-SCOPE-057 Runtime paces the frame loop

- **Statement:** While a session runs the frame pacer is in `FLUX_PACING_MODE_EXTERNAL`, `xrWaitFrame` is the loop's one pacing wait, and the latency slot still bounds run-ahead to `r.framesInFlight`.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_XrFrame_ExternalPacingSkipsPacerWait`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Pacing and the desktop mirror"

### Z2-R-SCOPE-058 Late-latched eye poses

- **Statement:** Each eye's world-from-eye matrix is the camera rig pose times the recentre transform times the eye pose located at the frame's predicted display time, and those poses never reach simulation.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_XrFrame_EyePosesComposeRigAndRecentre`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Reference spaces"; `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Frame cycle"

### Z2-R-SCOPE-059 Device loss recreates the session

- **Statement:** On a device loss the provider's `OnDeviceDestroying` reaches the system through its registered device listener, which destroys the swapchains, the session and the XR controller devices before the lost device is destroyed; from `LOST` through `RESUMING` no OpenXR frame call is made and no frame is submitted; after the device's recreation the provider supplies a fresh interop request and the session and swapchains are recreated on the new device.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrFrame_DeviceLossRecreatesSession`; `Device_Xr_DeviceLossRecovery`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Frame cycle"; `Zenith2/Docs/Design/SCOPE.md` → "Failure handling"

### Z2-R-SCOPE-060 Desktop mirror

- **Statement:** With `xr.mirror = leftEye` the desktop window shows the left eye's image copied and letterboxed by the present pass with an unpaced present mode, and with `none` the cleared colour, never a second scene render.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrFrame_ExternalPacingSkipsPacerWait` (the mirror copy pass counted, no second main view); `Device_Xr_OpenWorldSliceTraversal`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Pacing and the desktop mirror"

**Group: XR swapchains and layers.** Images, their import, recreation and the composition layers.

### Z2-R-SCOPE-061 Swapchain format preference

- **Statement:** Eye colour, eye depth and UI quad swapchains take the first offered format of the preference table, no colour format is `ZENITH_XR_ERROR_SWAPCHAIN_FORMAT`, and no depth format turns depth submission off.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072, Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** `Test_XrSwapchain_FormatPreference`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Swapchains"

### Z2-R-SCOPE-062 Swapchain extent

- **Statement:** Each eye's image extent is the recommended extent times `xr.renderScale`, rounded, clamped to the runtime's maximum and to at least 64, with one sample and one array layer.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrSwapchain_ExtentFromRecommendedAndScale`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Swapchains"

### Z2-R-SCOPE-063 Images imported as externally owned textures

- **Statement:** Each swapchain image is imported once as an externally owned RHI texture with OpenXR's release state and released through the device before its swapchain is destroyed, leaving no live handle at device destruction.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_XrSwapchain_ImagesImportedOnceAndReleased` on WARP and lavapipe
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Swapchains"

### Z2-R-SCOPE-064 Swapchain recreation

- **Statement:** A render-scale change, a recommended-size change or a governor CRITICAL step recreates the swapchains at a render-idle point, new before old, keeping the old ones on failure and retrying next prologue.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrSwapchain_RecreateOnScaleChange`, `Test_XrSwapchain_GovernorCriticalStepsScale`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Swapchains"; `Zenith2/Docs/Design/SCOPE.md` → "Memory and the governor"

### Z2-R-SCOPE-065 Projection layer

- **Statement:** The projection layer carries, per eye, exactly the pose and field of view located for that frame and the whole acquired image, in the active reference space.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrLayers_ProjectionUsesLocatedViews`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Composition layers"

### Z2-R-SCOPE-066 Depth submission

- **Statement:** With `xr.submitDepth` and `XR_KHR_composition_layer_depth`, each projection view chains its depth image with the eye's near and far planes, swapped for reversed Z.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072, Z2-D-061
- **Budgets:** none
- **Acceptance evidence:** `Test_XrLayers_DepthInfoChainedAndSwappedForReversedZ`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Composition layers"

### Z2-R-SCOPE-067 UI quad layer

- **Statement:** With `xr.uiLayer = quad` the screen-space UI is composited into the quad swapchain and submitted as a head-locked quad at `xr.uiQuadDistanceMetres` with `xr.uiQuadWidthMetres` width, dropped and counted when the runtime's layer limit is reached.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrLayers_QuadDroppedAtLayerLimit`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Composition layers"

**Group: XR spaces.** Reference spaces, recentring and the play area.

### Z2-R-SCOPE-068 Reference space choice

- **Statement:** The active reference space is STAGE, else LOCAL_FLOOR, else LOCAL with `xr.referenceSpace = auto`, an explicit preference falling back in the same order, and the VIEW space always exists.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrSpaces_ReferenceSpaceFallback`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Reference spaces"

### Z2-R-SCOPE-069 Recentre

- **Statement:** A recentre request applied in the next prologue rotates by the head's negative yaw and, in LOCAL and LOCAL_FLOOR, moves the head's horizontal position to the origin, applies to eye and controller poses alike, and publishes `XR.Recentred`.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrSpaces_RecentreLocalAndStage`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Recentre and play area"

### Z2-R-SCOPE-070 Play area

- **Statement:** The stage bounds are queried at session start and on their change event and published as `XR.PlayAreaChanged`, invalid when the runtime reports none.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrSpaces_PlayAreaPublished`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Recentre and play area"

**Group: XR input.** The OpenXR device source under PLAT's input model.

### Z2-R-SCOPE-071 Control-mirror action set

- **Statement:** One action set `zenith_mirror` holds one action per XR catalogue control kind with the left and right subaction paths, plus the haptic action, attached once at session creation, with grip and aim action spaces per hand.
- **Covers:** Z2-I-SCOPE-04, Z2-I-SCOPE-07
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrInput_MirrorDrivesVirtualDevices`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrInput.md` → "The control-mirror action set"; `Zenith2/Docs/Design/SCOPE/XrInput.md` → "XR control catalogue"

### Z2-R-SCOPE-072 Suggested bindings per profile

- **Statement:** Bindings are suggested for the five interaction profiles of the suggested-bindings table, the constant table equals the design table cell by cell, a refused profile is skipped and logged, and at least one accepted profile is required.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrInput_BindingTableMatchesDesign`, `Test_XrInput_RefusedProfileSkipped`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrInput.md` → "Suggested bindings"

### Z2-R-SCOPE-073 Controls into XR controller devices

- **Statement:** Before each platform pump the source syncs the mirror actions and pushes each changed control value to the `left`, `right` and `head` XR controller devices through `SetVirtualControl`, so the frame's ticks see them, and an inactive action rests once.
- **Covers:** Z2-I-SCOPE-04, Z2-I-SCOPE-07
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_XrInput_MirrorDrivesVirtualDevices`, `Test_XrInput_InactiveActionsRestOnce`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrInput.md` → "Per-frame input flow"

### Z2-R-SCOPE-074 Pose sampling for simulation

- **Statement:** Grip, aim and head poses are located at the last predicted display time plus one period, recentred, and pushed as POSE values only when both validity bits are set.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_XrInput_PoseSampleTime`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrInput.md` → "Poses for simulation"

### Z2-R-SCOPE-075 Interaction profile changes

- **Statement:** On an interaction-profile change the hand devices are created or destroyed with their hand's profile and PLAT receives the deduplicated active profile paths.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrInput_DevicesFollowProfiles`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrInput.md` → "Interaction profile changes"

### Z2-R-SCOPE-076 Controller haptics

- **Statement:** A haptic effect on an XR controller device becomes one `xrApplyHapticFeedback` with the larger motor amplitude and the effect's duration on the hand's subaction path, a zero amplitude a stop, and nothing outside FOCUSED.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrInput_HapticMapping`; `Device_Xr_ControllersAndHaptics`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrInput.md` → "Haptic output"

### Z2-R-SCOPE-077 Replay without a runtime

- **Statement:** A run recorded with the scripted runtime replays with no runtime to the same per-tick world hashes, because XR reaches simulation only through input commands and recorded notifications.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_XrInput_ReplayWithoutRuntimeMatches`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrInput.md` → "Replay and determinism"; `Zenith2/Docs/Design/SCOPE.md` → "Determinism obligations"

**Group: XR script surface.** The XR Luau namespace, its events and misuse codes.

### Z2-R-SCOPE-078 XR Luau namespace

- **Statement:** Behaviours read `XR.GetSessionInfo()` and `XR.GetPlayArea()` from the world's last published notifications and request a recentre with `XR.RequestRecentre()`, which takes effect only when the invocation commits.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072, Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_XrScript_SessionInfoFromPublishedState`, `Test_XrScript_RecentreCommitsOnly`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Script-facing API"

### Z2-R-SCOPE-079 XR notifications as recorded publications

- **Statement:** The three XR event types are 64-byte `ASYNC_SOURCE_XR` recorded publications parked in the prologue, at most eight outstanding per world with the oldest unparked one of a kind replaced and counted, delivered at the tick boundary and emitted in phase 1.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_XrScript_NotificationQueueBound`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Event types"

### Z2-R-SCOPE-080 XR misuse raises SCRIPT generic codes

- **Statement:** Misuse of the `XR` namespace raises SCRIPT's catalogued ZS1510 (argument count) or ZS1702 (not available in a chunk system) with their texts and never asserts.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-045, Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrScript_MisuseCodes`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Misuse codes"

### Z2-R-SCOPE-081 XR configuration keys

- **Statement:** The fifteen `xr.*` keys are registered with their types, defaults, ranges and layers, none simulation-affecting, and `xr.runtime = scripted` is refused in Shipping builds.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrScriptFormat_ShippingRefused`; the configuration dump of `Test_XrModule_StartRegistersInOrder`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Configuration keys"

**Group: XR diagnostics and seam.** Counters, logs, panels, the seam and the scripted runtime.

### Z2-R-SCOPE-082 XR counters and zones

- **Statement:** ZenithXR publishes the counters and profiling zones of the diagnostics design, with `Xr.WaitFrame` excluded from the frame's CPU work.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** the counter assertions of the frame and session tests; the `xr` panel in `Device_Xr_OpenWorldSliceTraversal`
- **Design:** `Zenith2/Docs/Design/SCOPE.md` → "Counters"; `Zenith2/Docs/Design/SCOPE.md` → "Profiling zones"

### Z2-R-SCOPE-083 XR log lines, panel and console commands

- **Statement:** ZenithXR logs exactly the messages of the log table, rate-limited where stated, registers the `xr` panel and the five `xr.*` console commands in development builds only, and no command changes simulation state.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_XrModule_StartRegistersInOrder` (development registrations); the log assertions of `Test_XrModule_DisabledWhenNoRuntime`
- **Design:** `Zenith2/Docs/Design/SCOPE.md` → "Log categories and messages"; `Zenith2/Docs/Design/SCOPE.md` → "Console commands"

### Z2-R-SCOPE-084 XR memory consumer

- **Statement:** The swapchain image bytes are reported to the governor as rendering memory, and CRITICAL lowers the render scale by 0.1 per entry to at least 0.7, restored at NORMAL.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrSwapchain_GovernorCriticalStepsScale`
- **Design:** `Zenith2/Docs/Design/SCOPE.md` → "Memory and the governor"

### Z2-R-SCOPE-085 OpenXR API seam

- **Statement:** Every OpenXR call goes through `Zenith_OpenXrApi`, implemented by the loader-backed and the scripted implementation, and `Zenith_XrCallGuard` asserts in debug builds that no two call contexts overlap.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrSeam_CallGuardDetectsOverlap` (`z2-death-assert`)
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "The OpenXR API seam"

### Z2-R-SCOPE-086 Scripted runtime format

- **Statement:** The scripted runtime reads `zenith2.xr-script` version 1, refuses a document breaking any of its fifteen rules with `ZENITH_XR_ERROR_INVALID_SCRIPT` and the rule number, delivers frame-0 entries at the first poll after session creation preceded by IDLE, and enforces OpenXR's call order and the graphics-device constraint of real runtimes.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrScriptFormat_ValidDocumentParses`, `Test_XrScriptFormat_EveryRuleRejects`, `Test_XrSeam_ScriptedRuntimeEnforcesCallOrder`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Scripted runtime"

### Z2-R-SCOPE-087 Script format fuzzing

- **Statement:** The script parser survives libFuzzer with no crash, leak or allocation beyond the 1 MiB input cap.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Zenith_Fuzz_XrScript` in `win-asan` and the acceptance fuzz campaign
- **Design:** `Zenith2/Docs/Design/SCOPE/XrTests.md` → "Format and fuzz tests"

### Z2-R-SCOPE-088 Gate tests on the scripted runtime

- **Statement:** Every `ZenithXR_Tests` case runs headless on WARP and lavapipe against the scripted runtime under label `z2-xr`, needing no headset and no installed runtime.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** PROC's Stage 7 line `Test.ps1 -Label z2-xr` and the `Test` step
- **Design:** `Zenith2/Docs/Design/SCOPE/XrTests.md` → "Test targets"; `Zenith2/Docs/Design/SCOPE/XrTests.md` → "Gate steps per test group"

**Group: XR integration and evidence.** Real runtimes, the device matrix, budgets and the multiview decision.

### Z2-R-SCOPE-089 Real-runtime integration

- **Statement:** On a GPU host with an OpenXR runtime the session reaches FOCUSED within 10 s and submits 600 frames with validation clean on both backends, and on a host without one each case exits 77 and is reported skipped, never passed.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrRuntime_SessionReachesFocused`, `Test_XrRuntime_BothBackends` in the windowed GPU configuration
- **Design:** `Zenith2/Docs/Design/SCOPE/XrTests.md` → "Real-runtime integration"

### Z2-R-SCOPE-090 Reference device matrix runs

- **Statement:** Each device run of the device-run table is performed on every row of the reference device matrix and recorded under `Zenith2/Docs/Research/Scope/XrDeviceRuns/`.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** the `needs-human` device-run records
- **Design:** `Zenith2/Docs/Design/SCOPE/XrTests.md` → "Reference device matrix"; `Zenith2/Docs/Design/SCOPE/XrTests.md` → "Device runs"

### Z2-R-SCOPE-091 XR budget rows

- **Statement:** The five pending XR rows (frame interval measured by MP-XRFRAME over the pacer's `xrEndFrame` interval ring, missed periods, pose-to-display interval, CPU, swapchain memory) are adopted by the Stage 7 XR budget ticket and measured on the reference matrix, with the missed-period row at most 1%.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Test_XrRuntime_MissedPeriodsBounded`; the Stage 7 budget ticket's report
- **Design:** `Zenith2/Docs/Design/SCOPE.md` → "Rows SCOPE owns"

### Z2-R-SCOPE-092 Single-pass multiview ruled by D-XR-MULTIVIEW

- **Statement:** Whether the stereo pair renders with single-pass multiview is decided by D-XR-MULTIVIEW (open, Stage 7) from a harness measuring per-eye GPU time on the RTX 3070, adopting multiview only for a saving of at least 15% with bit-identical stereo images.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** research
- **Decisions:** Z2-D-072, Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** the harness ticket's CTest smoke under `Zenith2/Benchmarks/XrMultiview/` and the ruling ticket's report
- **Design:** `Zenith2/Docs/Design/SCOPE.md` → "Open decisions SCOPE owns"

**Group: Consoles.** The Stage 7 console trees, per platform, specified without NDA material.

### Z2-R-SCOPE-093 PlayStation 5 SDK-access ticket

- **Statement:** A `needs-human` SDK-access ticket for PlayStation 5 is Done before any other PlayStation 5 ticket starts: agreement in force, SDK installed, a devkit reachable and the overlay configured.
- **Covers:** Z2-I-SCOPE-03, Z2-I-SCOPE-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-071
- **Budgets:** none
- **Acceptance evidence:** the ticket's needs-human record; the coverage gate step's links
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "SDK-access tickets"

### Z2-R-SCOPE-094 Xbox Series SDK-access ticket

- **Statement:** A `needs-human` SDK-access ticket for Xbox Series X and S is Done before any other Xbox Series ticket starts: agreement in force, SDK installed, a devkit reachable and the overlay configured.
- **Covers:** Z2-I-SCOPE-03, Z2-I-SCOPE-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-071
- **Budgets:** none
- **Acceptance evidence:** the ticket's needs-human record; the coverage gate step's links
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "SDK-access tickets"

### Z2-R-SCOPE-095 Nintendo Switch 2 SDK-access ticket

- **Statement:** A `needs-human` SDK-access ticket for Nintendo Switch 2 is Done before any other Switch 2 ticket starts: agreement in force, SDK installed, a devkit reachable and the overlay configured.
- **Covers:** Z2-I-SCOPE-03, Z2-I-SCOPE-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-071
- **Budgets:** none
- **Acceptance evidence:** the ticket's needs-human record; the coverage gate step's links
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "SDK-access tickets"

### Z2-R-SCOPE-096 Private platform overlay

- **Statement:** Proposed by flagged proposal SCOPE-1 (pending Q98): each console platform's code and NDA-covered documents live in its overlay at `Zenith2/Platforms/<Platform>/`, ignored by the main repository and denied to loop workers, built by the same CMake tree when present and declared-but-unavailable when absent.
- **Covers:** Z2-I-SCOPE-01, Z2-I-SCOPE-02
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-071
- **Budgets:** none
- **Acceptance evidence:** a configure without the overlay listing the console targets as unavailable (`z2-cmake` fixture `CMake_Platforms_AbsentOverlayDeclaresUnavailable`); the overlay's configure in its preset
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "The platform overlay"

### Z2-R-SCOPE-097 Console presets and matrix macros

- **Statement:** Each console's configure and build presets are defined in its overlay's own `CMakePresets.json`, included from one developer-local `Zenith2/CMakeUserPresets.json`, never the main `CMakePresets.json`, with a `ZENITH_SUPPORTED` macro naming the preset, and `ZENITH_GPU` includes the console presets only where their overlays exist.
- **Covers:** Z2-I-SCOPE-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-071, Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** `CMake_Platforms_AbsentOverlayDeclaresUnavailable`; the overlay preset's `Build` line (PROC)
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Console build presets and game.json"

### Z2-R-SCOPE-098 Console platform values in game.json

- **Statement:** `game.json` schema version 2 accepts `playstation5`, `xboxseries` and `switch2` in `platforms`, migrating version 1 files unchanged, and configures a game whose console overlay is absent for its other platforms.
- **Covers:** Z2-I-SCOPE-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-071, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** MOD's game.json fixture runner cases for the three values and the migration
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Console build presets and game.json"

### Z2-R-SCOPE-099 Console platform layer

- **Statement:** Each console implements the platform layer behind `Zenith_Platform` with its display, its suspend and resume within the platform's deadline following the Android lifecycle model, and its background mode as the background state, without changing the interface.
- **Covers:** Z2-I-SCOPE-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-071
- **Budgets:** none
- **Acceptance evidence:** `Device_Console_<Platform>_Lifecycle`; the overlay's platform tests
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements"

### Z2-R-SCOPE-100 Console input devices

- **Statement:** Each console's controllers are `INPUT_DEVICE_KIND_GAMEPAD` devices of one local user with haptics and their glyph family, with no second local player.
- **Covers:** Z2-I-SCOPE-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-071, Z2-D-076
- **Budgets:** none
- **Acceptance evidence:** `Device_Console_<Platform>_Lifecycle` (controller removed and replaced)
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements"

### Z2-R-SCOPE-101 Console filesystem and save storage

- **Statement:** Each console mounts its installed content as the package mount and its save area as the user mount, and implements `Zenith_SaveStorage` with SAVE's journaled atomic write over the platform's save-data model.
- **Covers:** Z2-I-SCOPE-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-071
- **Budgets:** none
- **Acceptance evidence:** the overlay's run of SAVE's platform storage tests; `Device_Console_<Platform>_Lifecycle` (suspend mid-save)
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements"

### Z2-R-SCOPE-102 Console accounts

- **Statement:** Each console reports its signed-in user, sign-out and user switch as PLAT account signals with one bound user.
- **Covers:** Z2-I-SCOPE-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-071, Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Device_Console_<Platform>_Lifecycle` (user switch mid-session)
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements"

### Z2-R-SCOPE-103 Console RHI realisation

- **Statement:** Each console's RHI is realised as D-CONSOLE-RHI (open, Stage 7) rules for it, after a baseline-equivalence check of every GPU baseline item on the devkit.
- **Covers:** Z2-I-SCOPE-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-071, Z2-D-013
- **Budgets:** none
- **Acceptance evidence:** the overlay's baseline-equivalence harness and the D-CONSOLE-RHI ruling per platform
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements"

### Z2-R-SCOPE-104 Console shader target

- **Statement:** Each console's shaders compile offline to its bytecode as D-CONSOLE-SHADERS (open, Stage 7) rules, with every program of the shader set compiling and the generated layouts unchanged.
- **Covers:** Z2-I-SCOPE-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-071, Z2-D-025, Z2-D-015
- **Budgets:** none
- **Acceptance evidence:** the overlay's shader-set compile harness and the D-CONSOLE-SHADERS ruling per platform
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements"

### Z2-R-SCOPE-105 Console audio device

- **Statement:** Each console implements an audio device backend reporting focus and interruption as device events.
- **Covers:** Z2-I-SCOPE-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-071
- **Budgets:** none
- **Acceptance evidence:** the overlay's audio device tests; `Device_Console_<Platform>_FlagshipPlaythroughs`
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements"

### Z2-R-SCOPE-106 Console crash capture

- **Statement:** Each console implements crash capture behind `Zenith_CrashHandler`.
- **Covers:** Z2-I-SCOPE-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-071
- **Budgets:** none
- **Acceptance evidence:** the overlay's crash test run on the devkit
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements"

### Z2-R-SCOPE-107 Console memory and budgets

- **Statement:** Each console feeds the governor's pressure levels from its memory figures and adopts its memory, frame-rate and package-size rows through its Stage 7 budget ticket, with only the adopted numbers in the main repository.
- **Covers:** Z2-I-SCOPE-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-071, Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** the per-platform budget ticket's report; `Device_Console_<Platform>_FlagshipPlaythroughs`
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements"; `Zenith2/Docs/Design/SCOPE.md` → "Rows SCOPE owns"

### Z2-R-SCOPE-108 Console packaging

- **Statement:** Each console packages, patches and adds DLC through PACK's install file system and asset-pack delivery interfaces without restructuring them.
- **Covers:** Z2-I-SCOPE-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-071
- **Budgets:** none
- **Acceptance evidence:** the overlay's package and patch run on the devkit
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements"

### Z2-R-SCOPE-109 Console online services

- **Statement:** Each console implements its platform services (identity, entitlements, achievements, cloud saves) as a `Zenith_OnlineServices` backend under ONLINE's backend contract.
- **Covers:** Z2-I-SCOPE-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-071, Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** ONLINE's conformance suite run against the backend in the overlay preset
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements"

### Z2-R-SCOPE-110 Console cook target

- **Statement:** Each console registers its cook target and texture formats through ASSET's cook-step registration from its overlay's tool library.
- **Covers:** Z2-I-SCOPE-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-071, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** the overlay's clean-versus-incremental cook equivalence run
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements"

### Z2-R-SCOPE-111 Console certification map

- **Statement:** Each console's certification requirements are mapped in its overlay onto the neutral categories of the certification table, and a requirement no behaviour satisfies becomes a neutrally titled ticket of its owner area.
- **Covers:** Z2-I-SCOPE-01, Z2-I-SCOPE-02
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-071
- **Budgets:** none
- **Acceptance evidence:** `Device_Console_<Platform>_CertificationPrecheck`
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Certification mapping"

### Z2-R-SCOPE-112 Console gate line

- **Statement:** Each console's build line `Build.ps1 -Preset <console-preset>` is added by PROC's per-platform ticket blocked by that platform's SDK-access ticket and SCOPE's preset ticket.
- **Covers:** Z2-I-SCOPE-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-071, Z2-D-032
- **Budgets:** none
- **Acceptance evidence:** PROC's console gate ticket; the overlay preset's green `Build` line
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Console tests and device runs"

### Z2-R-SCOPE-113 Console devkit runs

- **Statement:** Each console's three devkit runs (flagship playthroughs, lifecycle, certification precheck) pass and their outcomes are mirrored to the main board.
- **Covers:** Z2-I-SCOPE-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-071, Z2-D-021
- **Budgets:** none
- **Acceptance evidence:** the `needs-human` devkit run records in the overlay and their board outcomes
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Console tests and device runs"

### Z2-R-SCOPE-114 Console seams exist before Stage 7

- **Statement:** Every seam of the neutral-seams table exists with its Windows and Android implementations by the stage its owner lands it, and no main-repository code names a console.
- **Covers:** Z2-I-SCOPE-01
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-071
- **Budgets:** none
- **Acceptance evidence:** the owners' tests; `ForbiddenCheck` F11; review
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Console readiness today"

### Z2-R-SCOPE-115 SCOPE Stage 7 acceptance evidence

- **Statement:** The Stage 7 acceptance collects the `z2-xr` tests, the real-runtime integration, the XR device runs, the measured XR rows, and per console with SDK access its devkit runs and adopted rows, with `Pwsh_ScopeRegister_ReadinessTestsPresent` passing.
- **Covers:** Z2-I-SCOPE-01, Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-071, Z2-D-072, Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** the Stage 7 acceptance run record
- **Design:** `Zenith2/Docs/Design/SCOPE.md` → "Stage 7"; `Zenith2/Docs/Design/SCOPE.md` → "Test summary"

**Group: Device recreation, console line and test observability.** The headset-after-launch recreation, the console line without its overlay, human-only console work and the scripted runtime's observability.

### Z2-R-SCOPE-116 Headset after launch recreates the device

- **Statement:** When the first headset appears after launch, ZenithXR completes the system checks and requests a device recreation through RHI's recovery path, not counted as a device loss, and creates the session only on the recreated device the runtime required.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrSession_HeadsetAfterLaunchRecreatesDevice`; `Device_Xr_HeadsetAfterLaunch`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Loss recovery and shutdown"; `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Session state machine"

### Z2-R-SCOPE-117 Console build line exits 3 without its overlay

- **Statement:** The console build line exits 3 (`GATE_EXIT_ENVIRONMENT`) with an environment message when its overlay is absent, never success, and runs only in the overlay's own checkout, so no main-board category carries it.
- **Covers:** Z2-I-SCOPE-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-071, Z2-D-032
- **Budgets:** none
- **Acceptance evidence:** the `Test-Build.ps1` case `Test_Build_ConsolePresetAbsentOverlayExits3` (absent overlay exits 3; PROC row SP17); `CMake_Platforms_AbsentOverlayDeclaresUnavailable`
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Console build presets and game.json"; `Zenith2/Docs/Design/SCOPE/Consoles.md` → "The platform overlay"

### Z2-R-SCOPE-118 Console tickets are human-only

- **Statement:** Every console ticket is `needs-human` and no overlay content reaches a worker prompt, ticket body, board record or main-repository doc; with Q98's recommended option the overlay path is also denied to loop workers.
- **Covers:** Z2-I-SCOPE-02, Z2-I-SCOPE-03
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-071
- **Budgets:** none
- **Acceptance evidence:** A5 and A7 review of every console ticket's `needs-human` label; with Q98 (a), the PROC edit of row SP17 that adds `Zenith2/Platforms/**` to the loop's denied read paths
- **Design:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "What the main repository may hold"

### Z2-R-SCOPE-119 Scripted runtime observability

- **Statement:** Every named XR test is expressible through the scripted runtime document and observable through `Zenith_XrTestAccess`, whose call log records the function, frame, result and the argument summary the call-log table defines for at most the last 4,096 calls.
- **Covers:** Z2-I-SCOPE-04
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_XrFrame_FirstFrameNoDeadlock`, `Test_XrLayers_ProjectionUsesLocatedViews`, `Test_XrInput_RefusedProfileSkipped`
- **Design:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Scripted runtime"; `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Zenith_XrTestAccess header"

## Coverage of inventory items

| Item | Requirements |
|---|---|
| Z2-I-SCOPE-01 | Z2-R-SCOPE-004, Z2-R-SCOPE-093, Z2-R-SCOPE-094, Z2-R-SCOPE-095, Z2-R-SCOPE-096, Z2-R-SCOPE-097, Z2-R-SCOPE-098, Z2-R-SCOPE-099, Z2-R-SCOPE-100, Z2-R-SCOPE-101, Z2-R-SCOPE-102, Z2-R-SCOPE-103, Z2-R-SCOPE-104, Z2-R-SCOPE-105, Z2-R-SCOPE-106, Z2-R-SCOPE-107, Z2-R-SCOPE-108, Z2-R-SCOPE-109, Z2-R-SCOPE-110, Z2-R-SCOPE-111, Z2-R-SCOPE-112, Z2-R-SCOPE-113, Z2-R-SCOPE-114, Z2-R-SCOPE-115, Z2-R-SCOPE-117 |
| Z2-I-SCOPE-02 | Z2-R-SCOPE-011, Z2-R-SCOPE-096, Z2-R-SCOPE-111, Z2-R-SCOPE-118 |
| Z2-I-SCOPE-03 | Z2-R-SCOPE-012, Z2-R-SCOPE-014, Z2-R-SCOPE-093, Z2-R-SCOPE-094, Z2-R-SCOPE-095, Z2-R-SCOPE-118 |
| Z2-I-SCOPE-04 | Z2-R-SCOPE-005, Z2-R-SCOPE-017, Z2-R-SCOPE-018, Z2-R-SCOPE-036, Z2-R-SCOPE-037, Z2-R-SCOPE-038, Z2-R-SCOPE-039, Z2-R-SCOPE-040, Z2-R-SCOPE-041, Z2-R-SCOPE-042, Z2-R-SCOPE-043, Z2-R-SCOPE-044, Z2-R-SCOPE-045, Z2-R-SCOPE-046, Z2-R-SCOPE-047, Z2-R-SCOPE-048, Z2-R-SCOPE-049, Z2-R-SCOPE-050, Z2-R-SCOPE-051, Z2-R-SCOPE-052, Z2-R-SCOPE-053, Z2-R-SCOPE-054, Z2-R-SCOPE-055, Z2-R-SCOPE-056, Z2-R-SCOPE-057, Z2-R-SCOPE-058, Z2-R-SCOPE-059, Z2-R-SCOPE-060, Z2-R-SCOPE-061, Z2-R-SCOPE-062, Z2-R-SCOPE-063, Z2-R-SCOPE-064, Z2-R-SCOPE-065, Z2-R-SCOPE-066, Z2-R-SCOPE-067, Z2-R-SCOPE-068, Z2-R-SCOPE-069, Z2-R-SCOPE-070, Z2-R-SCOPE-071, Z2-R-SCOPE-072, Z2-R-SCOPE-073, Z2-R-SCOPE-074, Z2-R-SCOPE-075, Z2-R-SCOPE-076, Z2-R-SCOPE-077, Z2-R-SCOPE-078, Z2-R-SCOPE-079, Z2-R-SCOPE-080, Z2-R-SCOPE-081, Z2-R-SCOPE-082, Z2-R-SCOPE-083, Z2-R-SCOPE-084, Z2-R-SCOPE-085, Z2-R-SCOPE-086, Z2-R-SCOPE-087, Z2-R-SCOPE-088, Z2-R-SCOPE-089, Z2-R-SCOPE-090, Z2-R-SCOPE-091, Z2-R-SCOPE-092, Z2-R-SCOPE-115, Z2-R-SCOPE-116, Z2-R-SCOPE-119 |
| Z2-I-SCOPE-05 | Z2-R-SCOPE-013, Z2-R-SCOPE-042 |
| Z2-I-SCOPE-06 | Z2-R-SCOPE-019, Z2-R-SCOPE-020, Z2-R-SCOPE-021, Z2-R-SCOPE-022, Z2-R-SCOPE-023, Z2-R-SCOPE-024, Z2-R-SCOPE-025, Z2-R-SCOPE-026, Z2-R-SCOPE-027, Z2-R-SCOPE-028, Z2-R-SCOPE-035 |
| Z2-I-SCOPE-07 | Z2-R-SCOPE-019, Z2-R-SCOPE-029, Z2-R-SCOPE-030, Z2-R-SCOPE-031, Z2-R-SCOPE-032, Z2-R-SCOPE-033, Z2-R-SCOPE-035, Z2-R-SCOPE-071, Z2-R-SCOPE-073 |
| Z2-I-SCOPE-08 | Z2-R-SCOPE-006, Z2-R-SCOPE-015 |
| Z2-I-SCOPE-09 | Z2-R-SCOPE-007, Z2-R-SCOPE-016, Z2-R-SCOPE-034 |
| Z2-I-SCOPE-10 | Z2-R-SCOPE-001, Z2-R-SCOPE-002, Z2-R-SCOPE-004, Z2-R-SCOPE-005, Z2-R-SCOPE-006, Z2-R-SCOPE-007, Z2-R-SCOPE-008, Z2-R-SCOPE-009, Z2-R-SCOPE-010 |
| Z2-I-SCOPE-11 | Z2-R-SCOPE-002, Z2-R-SCOPE-003, Z2-R-SCOPE-014 |

## Excluded and deferred

| Requirement | Classification | Rationale and decision |
|---|---|---|
| Z2-R-SCOPE-015 Modding excluded | excluded | Zenith2 has no third-party mod loading of code, scripts, content packs or data overrides (rationale: each option contradicts static modules, in-repo games, per-version recooks or the sandbox's trust boundary). (Z2-D-075) |
| Z2-R-SCOPE-016 Local multiplayer and split-screen excluded | excluded | Zenith2 has no second local player, split-screen or shared-screen co-op; every local device belongs to player slot 0 and render views are never per-player (rationale: per-player views, input, UI and audio in every system for no flagship slice). (Z2-D-076) |
| Z2-R-SCOPE-017 Console headsets excluded | excluded | Zenith2 supports no console's own headset (rationale: it needs a console SDK and a second, non-OpenXR integration, the per-vendor coupling the XR record rejects; ruling SC-R3, pending Q99, which also carries the human's A4 classification of this exclusion beyond the inventory). (Z2-D-072, Z2-D-071) |
| Z2-R-SCOPE-018 Standalone headsets and passthrough ruled by D-XR-PLATFORMS | research | Whether Stage 7 adds a standalone Android OpenXR headset as a second validated Android device, and passthrough with a non-opaque blend mode, is decided by D-XR-PLATFORMS (open, Stage 7) through a harness build of WorkloadIndoor on the headset and a `needs-human` ruling, never by an implementation ticket. (Z2-D-072, Z2-D-010) |
| Z2-R-SCOPE-092 Single-pass multiview ruled by D-XR-MULTIVIEW | research | Whether the stereo pair renders with single-pass multiview is decided by D-XR-MULTIVIEW (open, Stage 7) from a harness measuring per-eye GPU time on the RTX 3070, adopting multiview only for a saving of at least 15% with bit-identical stereo images. (Z2-D-072, Z2-D-084) |

Every requirement classified `deferred` is a Stage 7 requirement with leaf tickets in the Stage 7 sprint: the XR tree (58 requirements: 036 to 091, 116, 119), the console tree (22 requirements: 093 to 113 and 117, each blocked by its platform's SDK-access ticket), and 2 more (035 and 115: the readiness retirement and the Stage 7 acceptance evidence).
