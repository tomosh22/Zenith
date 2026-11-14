# UI requirement catalogue

## Purpose

This catalogue breaks the UI inventory items (`Zenith2/Docs/Requirements/Inventory.md`, section "UI UI, localization, accessibility") into testable requirements `Z2-R-UI-NNN`. Each names the inventory items it covers, the stage whose acceptance it serves, its classification, its governing decisions and budget rows, the artefact that proves it and the design headings that specify it. The design is `Zenith2/Docs/Design/UI.md` and its section files under `Zenith2/Docs/Design/UI/`; the design entry's "Requirement index" is the reverse map. The catalogue is two files with one numbering: this file holds the purpose, the requirements of Stages 1 to 3, the coverage of inventory items (over both files) and the exclusions; `Zenith2/Docs/Requirements/UI/Stage4Plus.md` holds the requirements of Stages 4 to 6.

## Requirements

### Z2-R-UI-001 Module library target

- **Statement:** `ZenithUI` is a `KIND MODULE` static library with `MODULE_NAME UI` in `Zenith2/Engine/ZenithUI/`, declares `SUPPORTED ZENITH_GPU` and exactly the twelve `DEPENDS` entries of "Libraries UI code depends on" (the Architecture row plus `ZenithVertexFormat` and `ZenithRHI`), contains no platform-specific source, and no Player links it unless the game selects the module.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Libgraph` (declared edges and link surface), `SourceLayout` and `CompileCoverage` on the ticket that lands the library.
- **Design:** Zenith2/Docs/Design/UI.md → "Target summary"; Zenith2/Docs/Design/UI.md → "Libraries UI code depends on"; Zenith2/Docs/Design/UI.md → "ZenithUI files"

### Z2-R-UI-002 Module entry points

- **Statement:** `Zenith_Module_UI.h` declares exactly `Zenith_Module_UI_RegisterTypes` and `Zenith_Module_UI_Start`; RegisterTypes registers the component, the six event types, the two world resources, the six payload structs and the configuration keys, in that order.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_UIModule_StartSequence` (`Zenith_Integration_UI`, `Test`); `ApiContractCheck` over the header.
- **Design:** Zenith2/Docs/Design/UI/PublicAPI.md → "Zenith_Module_UI header"; Zenith2/Docs/Design/UI.md → "Module start sequence"

### Z2-R-UI-003 Start steps unwind

- **Statement:** Each of the eleven module start steps and the six world attach steps has a failure-injection test that fails that step alone and asserts the module error, no leak and no registration, callback or thread left behind.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** The seventeen unwinding tests of Tests.md "Integration tests": `Test_UIModule_StartStep01FailureUnwinds`, `Test_UIModule_StartStep02FailureUnwinds`, `Test_UIModule_StartStep03FailureUnwinds`, `Test_UIModule_StartStep04FailureUnwinds`, `Test_UIModule_StartStep05FailureUnwinds`, `Test_UIModule_StartStep06FailureUnwinds`, `Test_UIModule_StartStep07FailureUnwinds`, `Test_UIModule_StartStep08FailureUnwinds`, `Test_UIModule_StartStep09FailureUnwinds`, `Test_UIModule_StartStep10FailureUnwinds`, `Test_UIModule_StartStep11FailureUnwinds`; `Test_UIModule_AttachStep1FailureUnwinds`, `Test_UIModule_AttachStep2FailureUnwinds`, `Test_UIModule_AttachStep3FailureUnwinds`, `Test_UIModule_AttachStep4FailureUnwinds`, `Test_UIModule_AttachStep5FailureUnwinds`, `Test_UIModule_AttachStep6FailureUnwinds` (`Zenith_Integration_UI`, `Test`).
- **Design:** Zenith2/Docs/Design/UI.md → "Module start sequence"; Zenith2/Docs/Design/UI.md → "World attach and detach"; Zenith2/Docs/Design/UI.md → "Construction and shutdown"

### Z2-R-UI-004 Public headers carry contracts

- **Statement:** Every public type of the seventeen ZenithUI headers and of `Flux_UI.h` carries one contract block, every fallible public function returns `Zenith_Result`, and no third-party or STL header reaches a public header.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck`, `HeaderSelfContained` and `ForbiddenCheck` on every header-landing ticket.
- **Design:** Zenith2/Docs/Design/UI.md → "Rules across the UI API"; Zenith2/Docs/Design/UI/PublicAPI.md → "Header list"

### Z2-R-UI-005 API-only slices first

- **Statement:** The five API-only slices land, with contract blocks and stub definitions, before the implementations that consume them, so dependent areas compile against them.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** The slice tickets of "API-only slices" pass `Build` and `ApiContractCheck` with stubs; dependent tickets are `blocks`-linked behind them.
- **Design:** Zenith2/Docs/Design/UI.md → "API-only slices"; Zenith2/Docs/Design/UI.md → "Ticket slices and ordering"

### Z2-R-UI-006 Error enumerations

- **Statement:** `Zenith_UIErrorCode`, `Zenith_TextLayoutError`, `Zenith_FontError` and `Zenith_LocalizationErrorCode` carry exactly the codes listed, values stable once shipped, each with a `Zenith_ToString` name; `Zenith_UIError` is 16 bytes.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_UIError_SixteenBytes` (`ZenithUI_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/UI.md → "Error enumerations"; Zenith2/Docs/Design/UI/PublicAPI.md → "Zenith_UITypes header"

### Z2-R-UI-007 Configuration keys are presentation only

- **Statement:** Every key of "Configuration keys" outside `ui.access.*` and `ui.captions.*` (whose registration is Z2-R-UI-130's) is registered from RegisterTypes with its type, range and default, none is simulation-affecting, and a change applies at the next frame prologue.
- **Covers:** Z2-I-UI-10, Z2-I-UI-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_UIModule_StartSequence` asserts the registered keys and their flags; `Test_UIEvents_ReplayIdentical` replays with every `ui.*` key registered at Stage 3 set to a non-default value and matches hashes (the `ui.access.*` and `ui.captions.*` keys join that replay at Stage 4).
- **Design:** Zenith2/Docs/Design/UI.md → "Configuration keys"; Zenith2/Docs/Design/UI/PublicAPI.md → "Zenith_UISystem header"

### Z2-R-UI-008 Screens are entities

- **Statement:** A screen is opened by an entity carrying `Zenith.UI.Screen` (properties 1–10, authority `PHASE_LATE_SCRIPTS`) and closed by removing it or the entity; the context mirrors components at extraction and opens no screen any other way in a game build.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_UIScreenComponent_PropertyIDsStable` (`ZenithUI_Tests`) and `Test_UIScreens_SpawnFromPrefab` (`Zenith_Integration_UI`), `Test`.
- **Design:** Zenith2/Docs/Design/UI/PublicAPI.md → "Zenith_UIComponents header"; Zenith2/Docs/Design/UI/Interaction.md → "Script bindings"

### Z2-R-UI-009 One context per world

- **Statement:** Each attached world gets one `Zenith_UIContext` whose tag is the world tag; contexts share the process `Zenith_UISystem` and nothing else, and only the input world's context receives input.
- **Covers:** Z2-I-UI-01, Z2-I-UI-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_UIWorlds_ContextsIndependent` (`Zenith_Integration_UI`, `Test`).
- **Design:** Zenith2/Docs/Design/UI.md → "World attach and detach"; Zenith2/Docs/Design/UI/Interaction.md → "Input claims"

### Z2-R-UI-010 World resources and owners

- **Statement:** The two world resources (`Zenith.UI.Requests`, `Zenith.UI.Captions`) exist per world with the owning phases, writers and readers of "World resources", neither is saved, and the extractor is the sole consumer of `Zenith.UI.Requests`; decoded UI publications are module state of each world, written only at tick-boundary step 1 and read and emptied only by that world's `UI.Events`, so the publication handler writes no world state.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_UIModule_StartSequence` asserts the resources and their phases; the frame graph's declared-access validation passes in every integration test.
- **Design:** Zenith2/Docs/Design/UI.md → "World resources"; Zenith2/Docs/Design/UI.md → "Frame-graph placement"; Zenith2/Docs/Design/UI/Interaction.md → "Publication path"

### Z2-R-UI-011 Frame-graph placement

- **Statement:** UI work runs only in the claimant callbacks inside `Platform.Pump`, the publication handler at tick-boundary step 1, `UI.Events@W` (phase 1), `UI.Captions@W` (a C++ event handler in phase 9) and the extractor `UI.Extract@W` (phase 12, before `Render.Seal@W0`) with exactly the declared accesses.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** Z2-B-UI-002
- **Acceptance evidence:** `Test_UIFrameGraph_ExtractorBeforeSeal` (`Zenith_Integration_UI`, `Test`).
- **Design:** Zenith2/Docs/Design/UI.md → "Frame-graph placement"; Zenith2/Docs/Design/UI/Interaction.md → "Frame flow"

### Z2-R-UI-012 Context single-owner threading

- **Statement:** A `Zenith_UIContext` is touched by one thread at a time (Pump on the main thread, then its world's extractor), and `Zenith_UISystem` services serve concurrent extractors with leaf locks never held across a call out of ZenithUI; because ZenithUI is a `ZENITH_GPU` library absent from `linux-tsan`, no race detector covers it, and the evidence is the lock-order checker and a multi-world concurrent-extraction stress test.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** The lock-order checker over `Zenith_UILockOrder.inc` and `Test_UIContext_ConcurrentWorldsExtract` (`ZenithUI_Tests`, `Test`); `Test_UIEvents_WorkerCountInvariant`.
- **Design:** Zenith2/Docs/Design/UI.md → "Thread affinity per type"; Zenith2/Docs/Design/UI.md → "Lock-order entries"

### Z2-R-UI-013 Memory consumer

- **Statement:** The UI system reports host and device bytes to the memory governor and sheds unused dynamic pages, cache entries beyond their minimum and unreferenced faces on a pressure transition.
- **Covers:** Z2-I-UI-05, Z2-I-UI-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** Z2-B-UI-001
- **Acceptance evidence:** `Test_UIMemory_ConsumerReportsBytes` (`Zenith_Integration_UI`) and `Test_GlyphCache_ShedsUnderPressure` (`ZenithUI_Tests`), `Test`.
- **Design:** Zenith2/Docs/Design/UI.md → "Module start sequence"; Zenith2/Docs/Design/UI.md → "Failure table"; Zenith2/Docs/Design/UI/Text.md → "Glyph cache and atlases"

### Z2-R-UI-014 Five authoring document types

- **Statement:** The UI, theme, font family, localization project and string table document types are registered tool-side at schema version 1 with canonical schemas, and each has a v1 migration fixture pair.
- **Covers:** Z2-I-UI-01, Z2-I-UI-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_UIDocumentMigration_Fixtures` (`ZenithCook_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/UI/Documents.md → "Canonical schemas and document types"; Zenith2/Docs/Design/UI/Documents.md → "Migration chains and fixtures"

### Z2-R-UI-015 UI document schema

- **Statement:** A UI document accepts exactly the root members, element members, element types, text values, bindings, navigation and accessibility members of the schema, and preserves unknown members on save.
- **Covers:** Z2-I-UI-01, Z2-I-UI-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_UIDocumentModel_ValidatesEveryRule` and `Test_UIDocumentMigration_Fixtures`, `Test`.
- **Design:** Zenith2/Docs/Design/UI/Documents.md → "UI document schema"; Zenith2/Docs/Design/UI/Documents.md → "Element types"

### Z2-R-UI-016 Theme documents

- **Statement:** A theme declares styles with selectors, palettes and tokens, sprites and animations as specified, and every palette token is defined in `default`.
- **Covers:** Z2-I-UI-01, Z2-I-UI-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_UIDocumentModel_ValidatesEveryRule` (rules 70–75) and `Test_UIStyle_PaletteSwitchIsTableSwap`, `Test`.
- **Design:** Zenith2/Docs/Design/UI/Documents.md → "Theme document schema"; Zenith2/Docs/Design/UI/Documents.md → "Palettes and tokens"

### Z2-R-UI-017 Style property catalogue

- **Statement:** Style properties carry the stable IDs, value grammars, inheritance and animatability of the catalogue; `Zenith_UIStyleProperty` has exactly one enumerator per catalogue ID, `UI_STYLE_Z_INDEX` (30), `UI_STYLE_LAYOUT_DIRECTION` (49) and `UI_STYLE_MIRROR_IN_RTL` (84) included; `Zenith_UILayoutDirection` and the keyword enumerations (`Zenith_UILayoutKind`, `Zenith_UIStackDirection`, `Zenith_UIJustify`, `Zenith_UIAlign`, `Zenith_UIPosition`, `Zenith_UIImageFit`, `Zenith_UIPointerEvents`) equal the catalogue's keyword indices; and `Zenith_UIComputedStyle` has one field for every catalogue property (lengths as `Zenith_UILength`, track and transition lists as `Zenith_UIStyleSpan`, the text properties in `m_xText` and the paragraph fields), so layout, hit testing, paint, mirroring, the image swap and the animation sampler read every resolved value from it.
- **Covers:** Z2-I-UI-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_UIStyle_CascadeOrder` (one property of every catalogue group read back from its computed-style field) and `Test_UIStyle_InheritedProperties`; `Test_UIDocumentCompile_SectionsMatchFormat` round-trips the IDs 30, 49 and 84 (`ZenithUI_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/UI/Documents.md → "Style property catalogue"; Zenith2/Docs/Design/UI/PublicAPI.md → "Zenith_UIStyle header"

### Z2-R-UI-018 Document validation rules

- **Statement:** `Zenith_UIDocumentModel::Validate` reports rules 1–35 of the validation table (document structure, elements, styles, paths, instances) with their severities; an error fails the cook item and a warning is reported.
- **Covers:** Z2-I-UI-01, Z2-I-UI-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_UIDocumentModel_ValidatesEveryRule` (one fixture per rule, `ZenithUI_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/UI/Documents.md → "Validation rules"

### Z2-R-UI-019 Cooked UI document format

- **Statement:** `ui.document` writes `.zui` with ASSET's header, sections 1–18, the 64-byte header record (including safe area, focus scope and cancel action), 64-byte element records, the style block encoding, the TYPEDATA records of every element type, the EVENTS entries and 48-byte binding records, byte-identically for identical inputs.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_UIDocumentCompile_SectionsMatchFormat` and `Test_UIDocumentCompile_Deterministic`, `Test`.
- **Design:** Zenith2/Docs/Design/UI/Documents.md → "Cooked UI document format"; Zenith2/Docs/Design/UI/Documents.md → "Typed data records"; Zenith2/Docs/Design/UI.md → "Cook steps and derived-data keys"

### Z2-R-UI-020 Instances flatten at cook

- **Statement:** Instances and templates are flattened into the cooked element table with overrides applied, depth at most 8 and no cycles.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_UIDocumentCompile_FlattensInstances` and layout case `InstanceFlattening`, `Test`.
- **Design:** Zenith2/Docs/Design/UI/Documents.md → "Templates and instances"; Zenith2/Docs/Design/UI/Layout.md → "Layout conformance cases"

### Z2-R-UI-021 Document loader validates

- **Statement:** The `UI.Document` loader validates every section bound and record index and fails a corrupt file with `UI_ERROR_DOCUMENT_CORRUPT`, never asserting; the screen stays unopened.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_UIDocumentLoad_RejectsCorrupt` (`ZenithUI_Tests`) and `Fuzz_UICookedDocument` (`win-asan` smoke), `Test`.
- **Design:** Zenith2/Docs/Design/UI.md → "Asset kind loaders"; Zenith2/Docs/Design/UI.md → "Failure table"

### Z2-R-UI-022 Binding manifest

- **Statement:** `ui.manifest` writes `UI/BindingManifest.zbin` listing exactly the bound (type, property) pairs, sorted, with a check value, and the extractor declares exactly those reads.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_UIBindingManifest_ListsEveryBoundProperty` (`ZenithCook_Tests`, `z2-ui-cooked`) and `Fuzz_UIBindingManifest`.
- **Design:** Zenith2/Docs/Design/UI/Documents.md → "Binding manifest file"; Zenith2/Docs/Design/UI.md → "World attach and detach"

### Z2-R-UI-023 Cook steps registered

- **Statement:** The Stage 3 UI cook steps (`ui.document`, `ui.theme`, `ui.font`, `ui.manifest`) and the `ui.fontfamily` importer register once through `Zenith_RegisterUICookSteps` and `Zenith_RegisterUIImporters` with the derived-data key inputs listed; the two localization steps are Z2-R-UI-182's.
- **Covers:** Z2-I-UI-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_UICookSteps_RegisterOnce` (`ZenithCook_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/UI.md → "Cook steps and derived-data keys"; Zenith2/Docs/Design/UI/ServicesAPI.md → "Tool-side headers"

### Z2-R-UI-024 Parser limits

- **Statement:** Every cooked UI format reader enforces the parser limits of the table before allocating.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** The fuzz targets of Tests.md "Fuzz targets" (`win-asan` smoke in `Test`, full duration at stage acceptance).
- **Design:** Zenith2/Docs/Design/UI/Documents.md → "Parser limits"; Zenith2/Docs/Design/UI/Tests.md → "Fuzz targets"

### Z2-R-UI-025 Runtime selection ruling

- **Statement:** The D-SEL-UIRT `needs-human` ruling ticket, blocked by the harness ticket and by the Stage 1 per-system budget ticket for UI, measures both layout engines inside the shared shell on the RTX 3070 desktop and the S24, runs the selection test executable on Windows and on the S24, and applies the neutral, conjunctive acceptance rule in its order of comparison: the CSS and ZenithUI-semantics cases of the harness reports, with documented shims; then a candidate meeting both `Z2-B-UI-002` and `Z2-B-UI-003` in W-UI2D on the S24 beats one that misses either; then, between two that meet both, the lower `Z2-B-UI-002` time beyond the recorded spread; then binary size; when no candidate passes, or none passing meets every row, it records the shortfall for the human to decide. It commits both reports and the measurements under `Zenith2/Docs/Research/`, appends the ruling with the next free `Z2-D-NNN`, deletes the losing engine with its selection test and, when the custom runtime wins, RmlUi's `Zenith2/ThirdParty/manifest.json` entry, confirms the winner's licence fields, and widens `Test_UILayoutGolden_AllCases` and its variants to every case on the winner against the absolute expectations.
- **Covers:** Z2-I-UI-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2), Z2-D-084, Z2-D-097
- **Budgets:** Z2-B-UI-002, Z2-B-UI-003
- **Acceptance evidence:** The ruling ticket's committed reports (Windows and S24) under `Zenith2/Docs/Research/` and its `Zenith2/Docs/DecisionLog.md` entry (`needs-human`); after the deletion every UI test of the `Test` step still passes and the manifest holds no entry of the losing engine.
- **Design:** Zenith2/Docs/Design/UI.md → "Selection decisions UI owns"; Zenith2/Docs/Design/UI/Layout.md → "Runtime selection"

### Z2-R-UI-026 Element table

- **Statement:** A context stores elements in the SoA element table of "Element tree model", bounded by `ui.elements.maxPerContext`, refusing a screen beyond it with `UI_ERROR_ELEMENT_LIMIT`.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** Z2-B-UI-001
- **Acceptance evidence:** `Test_UILayout_ElementLimit`, `Test`.
- **Design:** Zenith2/Docs/Design/UI/Layout.md → "Element tree model"

### Z2-R-UI-027 Canvas units and scaling

- **Statement:** dp and reference scale modes, the UI scale and the platform display scale combine as specified, with the safe area applied to the root.
- **Covers:** Z2-I-UI-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Layout cases `ReferenceScaleMatch`, `DpScaleUIScale`, `SafeAreaInset` in `Test_UILayoutGolden_AllCases`.
- **Design:** Zenith2/Docs/Design/UI/Layout.md → "Canvas units and scaling"; Zenith2/Docs/Design/UI/Layout.md → "Safe area and surface changes"

### Z2-R-UI-028 Style cascade

- **Statement:** Computed styles follow the nine-step cascade, inheritance, state variants and per-palette token resolution, computed for every catalogue property by the shell's `Zenith_UIStyleResolver` for both D-SEL-UIRT candidates, so the paint path, hit testing and the semantic tree read one computed style whichever candidate is compiled.
- **Covers:** Z2-I-UI-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UIStyle_CascadeOrder` (every step, and one property of every catalogue group), `Test_UIStyle_InheritedProperties`, `Test_UIStyle_StateVariants`, `Test`.
- **Design:** Zenith2/Docs/Design/UI/Layout.md → "Style resolution"; Zenith2/Docs/Design/UI/Layout.md → "What both candidates implement"

### Z2-R-UI-029 Stack layout

- **Statement:** Stack layout implements grow, shrink, wrap, justify, align (including baseline), reverse, gaps and percentages exactly as the algorithm states.
- **Covers:** Z2-I-UI-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** The `Stack*` cases of `Test_UILayoutGolden_AllCases`.
- **Design:** Zenith2/Docs/Design/UI/Layout.md → "Stack layout"; Zenith2/Docs/Design/UI/Layout.md → "Size resolution"

### Z2-R-UI-030 Grid layout

- **Statement:** Grid layout implements fixed, fr, auto and min-content tracks, spans and auto placement as the algorithm states.
- **Covers:** Z2-I-UI-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** The `Grid*` cases of `Test_UILayoutGolden_AllCases`.
- **Design:** Zenith2/Docs/Design/UI/Layout.md → "Grid layout"

### Z2-R-UI-031 Anchored and overlay layout

- **Statement:** Anchored layout implements corner, stretch and fit-content anchors with pivots, and overlay children.
- **Covers:** Z2-I-UI-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Cases `AnchoredCorners`, `AnchoredStretch`, `AnchoredFitContent`, `OverlayChildren`.
- **Design:** Zenith2/Docs/Design/UI/Layout.md → "Anchored layout"

### Z2-R-UI-032 Intrinsic sizes and text leaves

- **Statement:** Intrinsic min- and max-content sizes, aspect ratio and text leaves measured through the text engine follow the rules.
- **Covers:** Z2-I-UI-02, Z2-I-UI-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2), D-SEL-TEXT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Cases `AspectRatio`, `TextWrapWord`, `TextWrapAnywhere`, `TextEllipsisMaxLines`, `TextAlignments`.
- **Design:** Zenith2/Docs/Design/UI/Layout.md → "Intrinsic sizes"; Zenith2/Docs/Design/UI/Layout.md → "Text leaves"

### Z2-R-UI-033 Pixel snapping

- **Statement:** Untransformed edges snap so adjacent elements share pixel edges.
- **Covers:** Z2-I-UI-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Case `PixelSnapping`.
- **Design:** Zenith2/Docs/Design/UI/Layout.md → "Rounding and pixel snapping"

### Z2-R-UI-034 Linear layout cost

- **Statement:** Layout is linear in the elements visited and incremental: a change inside a fixed-size boundary lays out only that subtree.
- **Covers:** Z2-I-UI-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** Z2-B-UI-002
- **Acceptance evidence:** `Test_UILayout_Complexity_Linear`, `Test_UILayout_IncrementalTouchesBoundaryOnly`, case `NestedBoundaries`.
- **Design:** Zenith2/Docs/Design/UI/Layout.md → "Complexity and bounds"; Zenith2/Docs/Design/UI/Layout.md → "Invalidation and incremental layout"

### Z2-R-UI-035 Layout conformance suite

- **Statement:** Every case of the conformance table lays out within 0.01 canvas units of its committed hand-derived expectation with the generated test font from the D-SEL-UIRT ruling on, on the winner; while D-SEL-UIRT is open, the shell cases gate on each compiled candidate in an invariant form that asserts only what the shell computes relative to that candidate's own layout, their absolute expectations are reported by the selection test as class `shell`, and the CSS and ZenithUI-semantics cases are reported by the selection test.
- **Covers:** Z2-I-UI-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UILayoutGolden_AllCases` (`ZenithUI_Tests`, `Test`): the shell cases per candidate in their invariant form before the ruling, every case against its absolute expectation after it.
- **Design:** Zenith2/Docs/Design/UI/Layout.md → "Layout conformance cases"; Zenith2/Docs/Design/UI/Tests.md → "Layout golden suite"

### Z2-R-UI-036 Scroll containers

- **Statement:** Scroll containers compute their extent, clamp offsets after layout, offset children and scroll focused elements into view with an 8 dp margin.
- **Covers:** Z2-I-UI-02, Z2-I-UI-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Case `ScrollContent`; `Test_UIPointer_ScrollVersusPressThreshold`.
- **Design:** Zenith2/Docs/Design/UI/Layout.md → "Scroll containers"; Zenith2/Docs/Design/UI/Interaction.md → "Scrolling"

### Z2-R-UI-040 Data binding evaluation

- **Statement:** Bindings convert every value kind as specified, read component properties at extraction only through the declared reads, and hold a committed control value for up to 30 frames.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_UIBinding_ConvertsEveryKind`, `Test_UIBinding_CommittedValueHoldsThirtyFrames`.
- **Design:** Zenith2/Docs/Design/UI/Layout.md → "Data binding evaluation"; Zenith2/Docs/Design/UI/Interaction.md → "Activation and value controls"

### Z2-R-UI-041 Animations and transitions

- **Statement:** Theme animations and transitions sample keys with the five easings on presentation time.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UIAnimation_KeysAndEasing`.
- **Design:** Zenith2/Docs/Design/UI/Layout.md → "Animations and transitions"

### Z2-R-UI-042 World-space panels

- **Statement:** A world-space screen lays out on a canvas of size × pixels-per-metre, maps canvas to world through its entity transform, and hit-tests along the camera ray.
- **Covers:** Z2-I-UI-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UIWorldPanel_CanvasToWorld`, case `WorldPanelCanvas`.
- **Design:** Zenith2/Docs/Design/UI/Layout.md → "World-space panels"; Zenith2/Docs/Design/UI/Interaction.md → "Hit testing"

### Z2-R-UI-043 Text stack selection ruling

- **Statement:** The D-SEL-TEXT `needs-human` ruling ticket, blocked by the harness ticket and by the Stage 1 per-system budget ticket for UI, measures the candidate stacks on the RTX 3070 desktop and the S24, runs the selection test executable on Windows and on the S24, and applies DecisionLog's criteria conjunctively in the order of comparison of "Selection decisions UI owns": the corpus reports (the SA cases against the recorded ICU4C offsets and the pseudo-localized long strings included) and the fallback chain on both platforms; then a stack that both fits `Z2-B-UI-001` and meets `Z2-B-UI-002` in W-UI2D on the S24 beats one that misses either; then, between two that meet both, the lower `Z2-B-UI-002` time beyond the recorded spread, then the smaller resident bytes, then binary size; when no stack passes, or none passing meets every row, it records the shortfall for the human to decide. It commits both reports and the measurements under `Zenith2/Docs/Research/`, appends the ruling with the next free `Z2-D-NNN`, deletes the losing stack with its selection test and its `Zenith2/ThirdParty/manifest.json` entries (SheenBidi or the ICU4X subset; FreeType when T2 wins), confirms the licence fields of the winner's entries, and adds `Test_UITextCorpus_Breaks` on the winner.
- **Covers:** Z2-I-UI-05, Z2-I-UI-06, Z2-I-UI-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2), Z2-D-084, Z2-D-097
- **Budgets:** Z2-B-UI-001, Z2-B-UI-002
- **Acceptance evidence:** The ruling ticket's committed reports (Windows and S24) under `Zenith2/Docs/Research/` and its `Zenith2/Docs/DecisionLog.md` entry (`needs-human`); after the deletion every UI test of the `Test` step still passes and the manifest holds no entry of the losing stack.
- **Design:** Zenith2/Docs/Design/UI.md → "Selection decisions UI owns"; Zenith2/Docs/Design/UI/Text.md → "Text stack selection"

### Z2-R-UI-044 Generated Unicode tables

- **Statement:** Unicode properties come from two-stage tables generated at build time from the pinned UCD and never committed, covering every code point.
- **Covers:** Z2-I-UI-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UIUnicode_TablesMatchSamples`, `Test_UIUnicode_TablesCoverEveryPlane`.
- **Design:** Zenith2/Docs/Design/UI/Text.md → "Unicode data"

### Z2-R-UI-045 UTF-8 repair before layout

- **Statement:** Every text-value path (messages, bound strings, literals, pseudo text, platform text and pasted text) passes through `Zenith_UIRepairUtf8`, which replaces each maximal ill-formed subsequence with U+FFFD, so malformed text never fails layout; `Layout` called directly with unrepaired text returns `TEXT_LAYOUT_ERROR_BAD_UTF8`.
- **Covers:** Z2-I-UI-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UIUtf8_DecodeReplacesInvalid` (`ZenithUI_Tests`, `Test`), `Fuzz_UITextLayout`.
- **Design:** Zenith2/Docs/Design/UI/Text.md → "Input and decoding"; Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIText header"

### Z2-R-UI-046 Shaping

- **Statement:** Each run is shaped with its face, script, language, direction and feature mask, and clusters map back to the source text, as the hand-derived test-font cases and the `hb-shape` goldens of the Noto corpus show.
- **Covers:** Z2-I-UI-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UITextCorpus_TestFontShaping` (hand-derived, every gate) and `Test_UITextCorpus_Shaping` against the goldens recorded with `hb-shape` (a missing golden fails), `ZenithUI_Tests`, `Test`.
- **Design:** Zenith2/Docs/Design/UI/Text.md → "Shaping"; Zenith2/Docs/Design/UI/Text.md → "Text conformance corpus"; Zenith2/Docs/Design/UI/Tests.md → "Shaping golden suite"

### Z2-R-UI-047 Font fallback per cluster

- **Statement:** Each cluster takes the first face of the stack, locale overrides, script families and engine fallbacks that covers it, else `.notdef` from the first face.
- **Covers:** Z2-I-UI-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UIFontStack_FallbackOrder`, `Test_UITextCorpus_Fallback`.
- **Design:** Zenith2/Docs/Design/UI/Text.md → "Font fallback"; Zenith2/Docs/Design/UI/Localization.md → "Font selection per locale"

### Z2-R-UI-048 Line breaking

- **Statement:** Lines break at UAX #14 opportunities with dictionary breaking for SA scripts and locale-dependent CJK strictness, falling back to grapheme breaks for unbreakable runs.
- **Covers:** Z2-I-UI-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UITextCorpus_Breaks`, added on the winner by the D-SEL-TEXT ruling ticket (hand-written offsets outside SA ranges; SA offsets recorded with ICU4C by the Stage 2 recording ticket of Z2-R-UI-173; a missing record fails), and `Test_UITextLayout_WrapAndEllipsis` (`ZenithUI_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/UI/Text.md → "Line breaking"

### Z2-R-UI-049 Grapheme boundaries

- **Statement:** Caret movement, deletion and truncation respect UAX #29 extended grapheme clusters, including emoji sequences and Indic conjuncts.
- **Covers:** Z2-I-UI-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UITextCorpus_Graphemes`, `Test_UITextEdit_Operations`.
- **Design:** Zenith2/Docs/Design/UI/Text.md → "Caret and selection"; Zenith2/Docs/Design/UI/Text.md → "Text editing"

### Z2-R-UI-050 Truncation and ellipsis

- **Statement:** `maxLines` with ellipsis truncates at a grapheme boundary and inserts U+2026 in the line's direction.
- **Covers:** Z2-I-UI-05, Z2-I-UI-25
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Case `TextEllipsisMaxLines`; `Test_UITextLayout_WrapAndEllipsis`.
- **Design:** Zenith2/Docs/Design/UI/Text.md → "Truncation and ellipsis"

### Z2-R-UI-051 Paragraph limit

- **Statement:** A paragraph above `uTEXT_MAX_PARAGRAPH_BYTES` fails with `TEXT_LAYOUT_ERROR_TOO_LONG` rather than truncating.
- **Covers:** Z2-I-UI-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UITextLayout_ParagraphLimit`.
- **Design:** Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIText header"; Zenith2/Docs/Design/UI/Text.md → "Input and decoding"

### Z2-R-UI-055 Text editing buffer

- **Statement:** `Zenith_TextEditBuffer` implements insertion, deletion by grapheme, by code point in Brahmic scripts and by word, selection replacement, composition, clipboard truncation and a byte capacity, and keeps no undo history (Ctrl+Z is not consumed).
- **Covers:** Z2-I-UI-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UITextEdit_Operations`.
- **Design:** Zenith2/Docs/Design/UI/Text.md → "Text editing"; Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIText header"

### Z2-R-UI-056 Text layout cache

- **Statement:** Unchanged paragraphs are not reshaped: the cache key covers text and layout-affecting style only, per context, bounded by entries and bytes.
- **Covers:** Z2-I-UI-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2)
- **Budgets:** Z2-B-UI-002
- **Acceptance evidence:** `Test_UITextCache_SteadyStateShapesNothing`, `Test_UITextCache_KeyIgnoresColour`.
- **Design:** Zenith2/Docs/Design/UI/Text.md → "Text layout cache"

### Z2-R-UI-057 Font family bake

- **Statement:** The font importer bakes the declared code-point set into `.zfont` with face and glyph records and MSDF page payloads whose header and check value match the format.
- **Covers:** Z2-I-UI-05, Z2-I-UI-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2), Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_FontBake_PageFormatAndCheck`, `Test_FontBake_PageLimit`.
- **Design:** Zenith2/Docs/Design/UI/Documents.md → "Cooked font format"; Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIFontBake header"

### Z2-R-UI-058 Baked equals dynamic

- **Statement:** Within one build, a glyph generated at run time has texels identical to the same glyph baked at cook time; across builds (cook host and device) pages are visually equivalent, differing by at most one quantisation step.
- **Covers:** Z2-I-UI-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_FontBake_BakedEqualsDynamic`.
- **Design:** Zenith2/Docs/Design/UI/Text.md → "MSDF generation"

### Z2-R-UI-059 Atlas packing

- **Statement:** Skyline packing places glyphs without overlap with a one-texel gutter.
- **Covers:** Z2-I-UI-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_GlyphCache_SkylinePacking`.
- **Design:** Zenith2/Docs/Design/UI/Text.md → "Atlas packing"

### Z2-R-UI-060 Dynamic glyph state machine

- **Statement:** Dynamic glyphs follow the state machine with a per-frame generation limit and least-recently-drawn page eviction; a pending glyph draws nothing and does not reflow its line.
- **Covers:** Z2-I-UI-05, Z2-I-UI-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2)
- **Budgets:** Z2-B-UI-001
- **Acceptance evidence:** `Test_GlyphCache_DynamicStateMachine`, `Test_UIDrawList_PendingGlyphsSkipped`.
- **Design:** Zenith2/Docs/Design/UI/Text.md → "Dynamic glyph state machine"

### Z2-R-UI-061 Headless glyph cache

- **Statement:** Headless (no renderer, no Flux) the glyph cache creates no page and generates no MSDF, every lookup is EMPTY for drawing, and layout and metrics are unaffected.
- **Covers:** Z2-I-UI-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_GlyphCache_HeadlessNeverGenerates`.
- **Design:** Zenith2/Docs/Design/UI/Text.md → "Page upload and residency"; Zenith2/Docs/Design/UI/Rendering.md → "Headless and device loss"

### Z2-R-UI-062 Font loader and fallback family

- **Statement:** The `UI.Font` loader validates `.zfont`; a missing or corrupt font substitutes the engine default family.
- **Covers:** Z2-I-UI-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Fuzz_UICookedFont` and `Test_UIDocumentLoad_MissingFontSubstitutes` (`z2-ui-cooked`).
- **Design:** Zenith2/Docs/Design/UI.md → "Asset kind loaders"; Zenith2/Docs/Design/UI.md → "Failure table"

### Z2-R-UI-063 Text corpus fixtures and recorded goldens

- **Statement:** The corpus groups are committed text over the pinned Noto fonts and the generated test font, in the corpus file format of the design (group files with their cases, styles and per-group expectation members; break offsets as UTF-8 byte offsets strictly inside the text, `allowed` for class-1 and `mandatory` for class-2 boundaries; the index `Groups.json` listing every group from slice 3, through which every corpus test enumerates); the Noto shaping goldens are recorded with `hb-shape` of the pinned HarfBuzz by a `needs-human` recording ticket scheduled after the D-SEL-TEXT ruling, one golden per group, which sets each recorded group's `shapingGolden` flag, and a flagged group without its recorded file fails; the SA break offsets are recorded before the ruling (Z2-R-UI-173).
- **Covers:** Z2-I-UI-05, Z2-I-UI-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Slice 3's committed corpus files and `Groups.json`; the recording ticket's committed `<Group>.expected.json` files and flags; `Test_UITextCorpus_Shaping` against them (`ZenithUI_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/UI/Text.md → "Text conformance corpus"; Zenith2/Docs/Design/UI/Tests.md → "Shaping golden suite"

### Z2-R-UI-064 Claimant registration

- **Statement:** The UI module registers its presentation claimant with the input system at start and unregisters it at shutdown; only the input world's context is fed.
- **Covers:** Z2-I-UI-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_UIClaimant_ClaimsOnlyHitPointers`, `Test_UIModule_StartSequence`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Input claims"; Zenith2/Docs/Design/UI.md → "Module start sequence"

### Z2-R-UI-065 Pointer claims

- **Statement:** A pointer press on a hit-testable element of an interactive screen, or anywhere on a blocking screen, is claimed for its whole gesture; other presses reach gameplay.
- **Covers:** Z2-I-UI-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_UIClaimant_ClaimsOnlyHitPointers`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Input claims"; Zenith2/Docs/Design/UI/Interaction.md → "Input arbitration with gameplay"

### Z2-R-UI-066 Hit testing order

- **Statement:** Hit testing visits screen-space screens by descending layer, then world panels nearest first, elements in reverse paint order with clip pruning, pointer-events modes and rounded corners.
- **Covers:** Z2-I-UI-17, Z2-I-UI-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UIHitTest_OrderAndPruning`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Hit testing"

### Z2-R-UI-067 Pointer state machine

- **Statement:** Each pointer follows every row of the pointer state machine, including the 8 dp threshold between activation and scrolling, cancel paths and inertia.
- **Covers:** Z2-I-UI-03, Z2-I-UI-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UIPointer_StateMachine`, `Test_UIPointer_ScrollVersusPressThreshold`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Pointer state machine"

### Z2-R-UI-068 Focus scopes

- **Statement:** Only qualifying screens (visible, interactive, `focusScope: auto`, a focusable element, not covered by a blocking screen) take keyboard and gamepad focus; `Engine.UI` is on the presentation stack exactly while a scope exists.
- **Covers:** Z2-I-UI-04, Z2-I-UI-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UIClaimant_PushesEngineUIWithFocusScope`, `Test_UIFocus_StateMachine`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Focus scopes"

### Z2-R-UI-069 Focus state machine

- **Statement:** Focus follows every row of the focus state machine, including remembered focus, initial focus, recovery from hidden or removed elements and requests from simulation.
- **Covers:** Z2-I-UI-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UIFocus_StateMachine`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Focus state machine"

### Z2-R-UI-070 Focus visibility

- **Statement:** The focus ring shows after navigation, tab or submit input and hides after a pointer press.
- **Covers:** Z2-I-UI-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UIFocus_StateMachine` (visibility rows).
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Focus visibility"

### Z2-R-UI-071 Directional navigation

- **Statement:** Directional navigation applies explicit targets, focus groups, the half-plane test and the score `primary + 4 × orthogonal gap + 0.1 × centre offset` with document-order ties and wrap.
- **Covers:** Z2-I-UI-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UINav_DirectionalScoring`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Directional navigation algorithm"

### Z2-R-UI-072 Tab order

- **Statement:** Next and previous follow (`nav.order`, document order), wrapping, with `nav.trap` confining the sequence.
- **Covers:** Z2-I-UI-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UINav_TabOrderAndTrap`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Tab order"

### Z2-R-UI-073 Navigation repeat

- **Statement:** Held navigation repeats after `ui.input.repeatDelayMs` every `ui.input.repeatIntervalMs`, from the dominant axis of `UI.Navigate` beyond 0.5.
- **Covers:** Z2-I-UI-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UIClaimant_PushesEngineUIWithFocusScope` (repeat cases).
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Input claims"

### Z2-R-UI-074 Control activation

- **Statement:** Each control type activates, changes value and emits exactly the event of the activation table; disabled elements receive nothing.
- **Covers:** Z2-I-UI-01, Z2-I-UI-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UIControls_ActivationTable`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Activation and value controls"

### Z2-R-UI-075 Cancel handling

- **Statement:** Cancel reverts an editing text field; otherwise it emits `UI.Cancelled` and is claimed only when the active screen declares `cancelEvent`.
- **Covers:** Z2-I-UI-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UIControls_CancelClaimsOnlyWithCancelEvent`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Activation and value controls"

### Z2-R-UI-076 Config-bound controls

- **Statement:** Controls bound to `config` sources write through `Zenith_Config::SetUserValue` and never publish an event; binding a simulation-affecting key is rule 46.
- **Covers:** Z2-I-UI-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_UIControls_ActivationTable` (config rows) and `Test_UIDocumentModel_ValidatesEveryRule` (rule 46).
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Activation and value controls"; Zenith2/Docs/Design/UI/Documents.md → "Validation rules"

### Z2-R-UI-077 Text fields and IME

- **Statement:** A text field begins a `Zenith_TextInput` session with its purpose and area, applies composition and commits, reports caret area changes, scrolls above the soft keyboard and ends on commit, cancel, focus loss or close.
- **Covers:** Z2-I-UI-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UITextField_SessionLifecycle`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Text fields and IME"

### Z2-R-UI-078 Keyboard consumed while editing

- **Statement:** While a field edits, `Engine.TextEntry` consumes the keyboard for gameplay and UI reads only the editing keys.
- **Covers:** Z2-I-UI-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UITextField_SessionLifecycle`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Text fields and IME"; Zenith2/Docs/Design/UI/Interaction.md → "Input arbitration with gameplay"

### Z2-R-UI-079 Input arbitration

- **Statement:** Every arbitration row holds: what UI claims is invisible to the simulation mapper, and everything else reaches it unchanged and recorded.
- **Covers:** Z2-I-UI-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_UIClaimant_ClaimsOnlyHitPointers`, `Test_UIEvents_ReplayIdentical`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Input arbitration with gameplay"

### Z2-R-UI-080 Input prompts

- **Statement:** Prompts show the active scheme's bindings through a main-thread cache refreshed on scheme, binding or screen-set changes, drawn as theme sprites or key caps.
- **Covers:** Z2-I-UI-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UIPrompts_CacheRefreshOnSchemeChange`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Input prompts and remapping"

### Z2-R-UI-081 Rebind controls

- **Statement:** A `rebind` element starts PLAT's rebind for its action, scheme and index, suspends navigation meanwhile, shows success or conflict, and resets one action or all when its `reset` member says so.
- **Covers:** Z2-I-UI-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UIRebind_Flow`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Input prompts and remapping"

### Z2-R-UI-082 On-screen controls

- **Statement:** On-screen sticks and buttons drive the virtual device `ui`, whose values reach simulation through the game's bindings and are recorded.
- **Covers:** Z2-I-UI-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_UIOnScreenControls_VirtualDevice`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "On-screen controls"

### Z2-R-UI-083 Publication payload

- **Statement:** UI events and the resume record are encoded into the payload layout of at most 448 bytes; text that is ill-formed UTF-8 is refused at encode, and decoding rejects every malformed payload without asserting.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_UIPublication_RoundTripsEveryKind`, `Test_UIPublication_RejectsMalformed`, `Test_UIPublication_RejectsIllFormedText`, `Fuzz_UIPublication`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Event kinds and payloads"; Zenith2/Docs/Design/UI/PublicAPI.md → "Zenith_UIEvents header"

### Z2-R-UI-084 Text event truncation

- **Statement:** A committed text longer than 256 bytes is published truncated at a grapheme boundary and counted.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_UIPublication_TextTruncatesAtGrapheme`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Text fields and IME"; Zenith2/Docs/Design/UI/Interaction.md → "Event kinds and payloads"

### Z2-R-UI-085 Events reach scripts next tick

- **Statement:** A UI event made in frame k is delivered as a `Zenith.UI.*` event in the first tick boundary after, through the publication path, with element, event, item and value.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_UIEvents_ReachScriptsNextTick`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Publication path"

### Z2-R-UI-086 Publications replay

- **Statement:** Replays publish the recorded UI payloads at their recorded ticks, no live viewer input reaches the sink (Z2-R-UI-175), and simulation hashes reproduce exactly.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_UIEvents_ReplayIdentical`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Publication path"

### Z2-R-UI-087 Worker-count invariance

- **Statement:** Event order and simulation hashes are identical with 1, 2 and 8 workers, including two worlds publishing in the same prologue.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_UIEvents_WorkerCountInvariant` (two-world case included).
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Publication path"

### Z2-R-UI-088 Paused clock defers events

- **Statement:** Events made while a world is paused publish, in order, at the first tick after resumption, up to `ui.events.maxInFlight − 1` outstanding ordinary events per world (one slot is the resume record's).
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_UIEvents_PausedWorldDefersPublication`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Publication path"

### Z2-R-UI-089 Pause menu resume

- **Statement:** A `resumeWorld` presentation action (a button's `presentationAction` or a screen's `cancelAction`) on a world whose `MENU` reason is requested first parks a resume record through the reserved publication slot and only when that record was accepted clears `WORLD_PAUSE_REASON_MENU`, and nothing else, on the input world's control; a refused record leaves the control untouched and counts `ui.events.resume_refused`; the ordinary event follows; UI never sets a pause reason.
- **Covers:** Z2-I-UI-01, Z2-I-UI-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_UIPauseMenu_ResumeClearsMenuReason` (`Zenith_Integration_UI`, `Test`).
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Pause menus"; Zenith2/Docs/Design/UI.md → "World attach and detach"

### Z2-R-UI-090 Stale and dropped events

- **Statement:** An event whose screen was destroyed is dropped and counted; ordinary events beyond `ui.events.maxPerFrame − 1` per frame or `ui.events.maxInFlight − 1` outstanding are dropped and counted, and the control returns to its bound value.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_UIEvents_StaleScreenDropped`, `Test_UIEvents_ChannelFullDropsAndCounts` (`Zenith_Integration_UI`, `Test`).
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Publication path"; Zenith2/Docs/Design/UI.md → "Failure table"

### Z2-R-UI-091 Two-way bindings

- **Statement:** A self binding with `writeBack` records a `SetProperty` command with the converted value at the phase-1 sync point; a failed conversion is counted.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_UIEvents_WriteBackSetsProperty`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Two-way bindings"

### Z2-R-UI-092 Command kinds

- **Statement:** The six UI command kinds are registered with their payload structs, allowed in phases 2–10, and the playback of the four request kinds fills `Zenith.UI.Requests` (the two caption kinds' playback is Z2-R-UI-138's).
- **Covers:** Z2-I-UI-01, Z2-I-UI-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_UIRequests_FocusScrollAnnounce`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Command kinds"

### Z2-R-UI-093 Requests are presentation only

- **Statement:** Requests take effect at the next extraction, unknown targets are counted and ignored, and no request writes simulation state.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_UIRequests_FocusScrollAnnounce`, `Test_UIEvents_ReplayIdentical`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Command kinds"

### Z2-R-UI-094 Luau UI namespace

- **Statement:** The `UI` Luau table registers `Loc` (pure) and the six buffered functions with the field-command side-effect mask; a failed invocation records no request.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_UIScriptApi_FunctionsRegistered`, `Test_UIScriptApi_BufferedRequestsDiscardedOnFailure`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Script bindings"; Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIScriptApi header"

### Z2-R-UI-095 Script misuse is a Luau error

- **Statement:** Script misuse of the `UI` table raises a Luau error with a code of UI's block ZS2100–ZS2199 (ZS2101–ZS2106, one meaning each, with the exact texts of "Luau errors and asserts") and never asserts; UI raises no code of another block.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_UIScriptApi_MisuseCodes` (`ZenithUI_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/UI.md → "Luau errors and asserts"; Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIScriptApi header"

### Z2-R-UI-096 Paint order

- **Statement:** Draw lists paint screens by layer and open order and elements depth first with siblings in ascending `zIndex` (style property 30, ties by document order), each element in the part order with the focus ring above its children; elements with `visible` false emit nothing.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UIDrawList_PaintOrder`, `Test_UIDrawList_ZIndexAndClipVocabulary`.
- **Design:** Zenith2/Docs/Design/UI/Rendering.md → "Paint order"

### Z2-R-UI-097 Quads per part

- **Statement:** Each element part emits exactly the quads and modes of the quad table.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UIDrawList_QuadsPerPart`.
- **Design:** Zenith2/Docs/Design/UI/Rendering.md → "Quad generation per element"

### Z2-R-UI-098 Clipping

- **Statement:** Elements with `clip: true` push clips that become scissors; quads wholly outside are not emitted; rotated clips use their bounding rectangle.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UIDrawList_ClipCullsOutside`, `Test_UIDrawList_ZIndexAndClipVocabulary`, `Test_UIRender_ScissorPerBatch`.
- **Design:** Zenith2/Docs/Design/UI/Rendering.md → "Clipping"

### Z2-R-UI-099 Batching

- **Statement:** Batches break exactly on scissor, texture or SDF permutation changes, never reordering paint.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** Z2-B-UI-003
- **Acceptance evidence:** `Test_UIDrawList_BatchBreaks`.
- **Design:** Zenith2/Docs/Design/UI/Rendering.md → "Batching"

### Z2-R-UI-100 Quad record layout

- **Statement:** `Flux_UIQuad` is 64 bytes with the offsets of the layout table, identical to the reflected Slang struct, and its half fields are written with ZenithVertexFormat's `Zenith_F32ToF16`.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-054
- **Budgets:** none
- **Acceptance evidence:** `Test_UIQuad_LayoutMatchesShader` (`ZenithFlux_Tests`, WARP and lavapipe) and `Test_UIQuad_HalfFieldsUseVertexCodec` (`ZenithUI_Tests`).
- **Design:** Zenith2/Docs/Design/UI/Rendering.md → "Flux_UIQuad layout"

### Z2-R-UI-101 Render-world UI lists

- **Statement:** The extractor writes one `Flux_RenderUIList` per world per frame with the fields of the record table, including its world tag, truncating at a batch boundary beyond the quad limit; a full render-world slot draws no UI that frame and is counted.
- **Covers:** Z2-I-UI-01, Z2-I-UI-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** Z2-B-RPROD-003
- **Acceptance evidence:** `Test_UIDrawList_TruncatesAtBatchBoundary`, `Test_UIFrameGraph_ExtractorBeforeSeal`, `Test_UIExtract_SlotFullDrawsNothing`.
- **Design:** Zenith2/Docs/Design/UI/Rendering.md → "Render-world records"

### Z2-R-UI-102 UI renderer

- **Statement:** `Flux_UIRenderer` is created once through `Flux_Renderer::EnableUI` at module start; in `Flux.BuildGraph` it declares the frame's quad bytes with `DeclareFrameData` at step 4, writes them only at step 11 after `ExtendReservation`, keeps a per-frame quad cap, and a deferred frame writes no UI byte.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-055
- **Budgets:** Z2-B-UI-003, Z2-B-RHI-006
- **Acceptance evidence:** `Test_UIRenderer_WritesAfterReservation` (`ZenithFlux_Tests`, WARP and lavapipe) and `Test_UIRender_CrossBackend`.
- **Design:** Zenith2/Docs/Design/UI/Rendering.md → "Flux UI renderer"

### Z2-R-UI-103 UIComposite pass

- **Statement:** `Flux_AddUIComposite` keeps Rendering's signature and declares one raster pass that draws each batch with its permutation, parameters and scissor, premultiplied.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-054, Z2-D-055
- **Budgets:** Z2-B-UI-003
- **Acceptance evidence:** `Test_UIRender_SolidCoverage`, `Test_UIRender_ScissorPerBatch`, `Test_UIRender_PremultipliedBlending`.
- **Design:** Zenith2/Docs/Design/UI/Rendering.md → "UIComposite implementation"

### Z2-R-UI-104 Shape and glyph shading

- **Statement:** The pixel stage implements the coverage rules of every quad mode, including MTSDF glyphs with outlines.
- **Covers:** Z2-I-UI-01, Z2-I-UI-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UIRender_SolidCoverage`, `Test_UIRender_GradientLinearInterpolation`, `Test_UIRender_GlyphMSDFCoverage`.
- **Design:** Zenith2/Docs/Design/UI/Rendering.md → "Shader programs"

### Z2-R-UI-105 UIWorldSpace pass

- **Statement:** World-space panels draw at the transparent extension point, depth-tested without depth writes with the comparison of the view's `Flux_DepthMode` (standard Z by default, Z2-D-061), back to front, as unlit emissive surfaces scaled by pre-exposure.
- **Covers:** Z2-I-UI-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-061
- **Budgets:** Z2-B-UI-003
- **Acceptance evidence:** `Test_UIRender_WorldSpaceOcclusion` (standard-Z and reversed-Z views), `Test_UIWorldSpace_MovingPanelGhostingBounded`.
- **Design:** Zenith2/Docs/Design/UI/Rendering.md → "UIWorldSpace pass"

### Z2-R-UI-106 Atlas pages on the GPU

- **Statement:** Atlas pages are persistent read-only textures; dynamic pages are replaced by reallocate-and-swap; at most `ui.atlas.pageUploadsPerFrame` pages are created per frame, bounding UI's page upload bytes per frame, with old descriptors valid until retirement.
- **Covers:** Z2-I-UI-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-054
- **Budgets:** Z2-B-RHI-009, Z2-B-RHI-006
- **Acceptance evidence:** `Test_UIRender_AtlasSwapKeepsOldDescriptorValid`.
- **Design:** Zenith2/Docs/Design/UI/Rendering.md → "Atlas pages on the GPU"

### Z2-R-UI-107 Colour and HDR output

- **Statement:** UI colours decode from sRGB and pass through `Flux_EncodeOutput`, which in SDR returns the plain linear value, blended linearly through the sRGB view and encoded once by the hardware; UI never encodes twice. The HDR10 and scRGB modes are Z2-R-UI-180's.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UIRender_PremultipliedBlending` (SDR stores 188 for 50 % white over black, ±1).
- **Design:** Zenith2/Docs/Design/UI/Rendering.md → "Colour and HDR output"

### Z2-R-UI-108 Device loss

- **Statement:** After device loss every atlas page is recreated from its CPU copy and text renders identically after at most one frame of pending glyphs.
- **Covers:** Z2-I-UI-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UIRender_DeviceLossRecreatesPages`.
- **Design:** Zenith2/Docs/Design/UI/Rendering.md → "Headless and device loss"; Zenith2/Docs/Design/UI.md → "Failure table"

### Z2-R-UI-109 Cross-backend UI parity

- **Statement:** The UI parity scene, a menu and its text drawn by `UIComposite` alone, renders on both software devices, each image passes every analytic oracle at its sampled pixels of `UIComposite` output, and the two images differ at every sampled pixel by at most the sum of the two devices' oracle tolerances there, a derived bound that no budget row supplies; world panels, whose pixels pass through TAA and the output transform, are not part of the scene and are checked per device by `Test_UIRender_WorldSpaceOcclusion`.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-UI-003
- **Acceptance evidence:** `Test_UIRender_CrossBackend` (a GPU case of `Zenith_Integration_UI` on WARP and lavapipe, `Test`).
- **Design:** Zenith2/Docs/Design/UI/Tests.md → "Render tests"

### Z2-R-UI-110 Headless UI runs

- **Statement:** Headless (the runtime configuration without Flux) no UI renderer, page or pass exists, while documents, layout, events, requests, localization, the accessibility tree and draw-list construction still run.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UIModule_HeadlessRunsWithoutRenderer` and every `Zenith_Integration_UI` test outside its GPU cases (headless runtime, `Test`).
- **Design:** Zenith2/Docs/Design/UI/Rendering.md → "Headless and device loss"; Zenith2/Docs/Design/UI.md → "Failure table"

### Z2-R-UI-127 RTL mirroring

- **Statement:** In a right-to-left layout context (`layoutDirection`, `auto` following the locale) the shell lays out in LTR with the margins of every element whose parent is RTL exchanged, then, top-down, places each child of an RTL element mirrored inside its parent's final content box while padding, borders and corner radii stay physical; so stacks, grids, anchors and alignment mirror, an LTR island's own box mirrors while its children keep LTR order inside it, an LTR child of an RTL flow keeps its physical margins, and visual offsets, glyphs and images (unless `mirrorInRtl`) do not flip; for symmetric documents the RTL rectangles equal `x' = W − x − w` of the LTR ones.
- **Covers:** Z2-I-UI-26
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UILayoutGolden_MirroredMatchesReflection` (every "Mirrored" case and the four `Mirror*` cases, `MirrorLtrIsland` and `MirrorLtrLeafInRtlRow` among them; the four `Mirror*` shell cases per candidate in their invariant form while D-SEL-UIRT is open), `ZenithUI_Tests`, `Test`; the mirroring pass lands in slice 7 (Stage 2 sprint, fix version `Z2 Stage 3`), and the locale-driven RTL validation of a localized game is Z2-R-UI-128 at Stage 4.
- **Design:** Zenith2/Docs/Design/UI/Layout.md → "Logical direction and mirroring"; Zenith2/Docs/Design/UI/Layout.md → "Layout conformance cases"

### Z2-R-UI-154 Counters and zones

- **Statement:** UI registers every counter of the "Counters" table and the profiler zones listed, and each counter moves in the scenario that should move it.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-UI-002, Z2-B-UI-003
- **Acceptance evidence:** `Test_UIDiagnostics_CountersAndZones` (`Zenith_Integration_UI`); `Bench_UI_MenuFrame` reads the zones.
- **Design:** Zenith2/Docs/Design/UI.md → "Counters"; Zenith2/Docs/Design/UI.md → "Profiler zones"

### Z2-R-UI-155 Debug views and frame dump

- **Statement:** The UI debug views are configuration keys settable from the command-line layer (so DEVT's console toggles them) and draw through the draw list; UI's `ui` section registered with DEVT's frame dump writes the listed members per world.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UIDiagnostics_DebugViewsAndDump` (`Zenith_Integration_UI`).
- **Design:** Zenith2/Docs/Design/UI.md → "Debug views"; Zenith2/Docs/Design/UI.md → "Frame-dump entries"

### Z2-R-UI-156 Failure table behaviours

- **Statement:** Every row of the failure table behaves as stated, without asserting on untrusted input, and each row names the test that asserts it.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** The tests of the failure table's "Test" column, row by row: `Test_UIDocumentLoad_RejectsCorrupt`, `Test_UIDocumentLoad_HotReloadRepairsScreen`, `Test_UIDocumentLoad_MissingFontSubstitutes`, `Test_UIFontStack_FallbackOrder`, `Test_GlyphCache_DynamicStateMachine`, `Test_GlyphCache_PageCreateFailureRetries`, `Test_GlyphCache_ShedsUnderPressure`, `Test_UILoc_LocaleSwitchAtFrameBoundary`, `Test_UIExtract_SlotFullDrawsNothing`, `Test_UIEvents_ChannelFullDropsAndCounts`, `Test_UIPauseMenu_ResumeClearsMenuReason`, `Test_UIEvents_ReplayIdentical`, `Test_UIRequests_FocusScrollAnnounce`, `Test_UIRender_DeviceLossRecreatesPages`, `Test_UIModule_HeadlessRunsWithoutRenderer`, `Test_UIA11y_NoServiceKeepsSettings`, `Test_UITextField_SessionLifecycle`, `Test_UIEvents_ReplayCorruptPayloadDropped`, `Test_UIContext_OpenScreenOutOfMemory` and the seventeen start and attach unwinding tests.
- **Design:** Zenith2/Docs/Design/UI.md → "Failure table"

### Z2-R-UI-157 Per-system budget ticket

- **Statement:** The Stage 1 per-system budget ticket for UI allocates `Z2-B-UI-001` to `Z2-B-UI-003` and the four pending rows of "Pending row needs" from reference-engine data, with no harness half, and reconciles ContentSpecs `## UI` with `Z2-B-UI-001` in its allocation table.
- **Covers:** Z2-I-UI-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-UI-001, Z2-B-UI-002, Z2-B-UI-003
- **Acceptance evidence:** The ticket's committed rows in `Zenith2/Docs/Budgets.md` and its allocation table with the ContentSpecs `## UI` reconciliation ([Z2-N-BUDGET-419]).
- **Design:** Zenith2/Docs/Design/UI.md → "Rows owned"; Zenith2/Docs/Design/UI.md → "Pending row needs"; Zenith2/Docs/Design/UI.md → "Content specifications and quality matrix rows"

### Z2-R-UI-158 UI benchmark harness

- **Statement:** The D-SEL harness tickets build `Zenith_Bench_UI` with the Stage 2 benchmarks of the slice table (the menu in steady state, the HUD driven through `Zenith_UIContext::SetBinding`, the composite, paragraph shaping, memory and dynamic glyphs), synthesising W-UI2D-scale scenes and compiling both candidates of an open selection, with a registered `--smoke` CTest that runs every benchmark compiled into the harness for three frames; no Stage 2 benchmark needs a slice outside the Stage 2 pull-forward.
- **Covers:** Z2-I-UI-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2), D-SEL-UIRT (open, Stage 1/2), Z2-D-084
- **Budgets:** Z2-B-UI-001, Z2-B-UI-002, Z2-B-UI-003
- **Acceptance evidence:** The registered `Zenith_Bench_UI --smoke` CTest in the `Test` step.
- **Design:** Zenith2/Docs/Design/UI/Tests.md → "Benchmarks and harnesses"; Zenith2/Docs/Design/UI.md → "Selection decisions UI owns"

### Z2-R-UI-159 W-UI2D workload scale

- **Statement:** The Stage 1 workload scale ticket for W-UI2D sets its content scale and scripted path as a `needs-human` ruling.
- **Covers:** Z2-I-UI-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-CONTENT-009
- **Acceptance evidence:** The committed `Zenith2/Docs/Research/Workloads/W-UI2D.md` and the ruling.
- **Design:** Zenith2/Docs/Design/UI.md → "Stage table"

### Z2-R-UI-161 Input-to-photon contribution

- **Statement:** A UI button press is claimed in the frame's Pump and drawn in the same frame, so UI adds no frame to input-to-photon latency.
- **Covers:** Z2-I-UI-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-RPROD-027
- **Acceptance evidence:** `Bench_UI_InputToPhoton`, added with its smoke run by slice 10's ticket and measured at stage acceptance (`needs-human` device steps).
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Frame flow"

### Z2-R-UI-163 Binding validation rules

- **Statement:** The validator reports rules 40–50, 52 and 53 (bindings, configuration bindings, localization keys, literals, input actions) with their severities; rule 51 is Z2-R-UI-181's.
- **Covers:** Z2-I-UI-01, Z2-I-UI-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_UIDocumentModel_ValidatesEveryRule` (rules 40–50, 52, 53, `ZenithUI_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/UI/Documents.md → "Validation rules"

### Z2-R-UI-164 Navigation and accessibility validation rules

- **Statement:** The validator reports rules 60–65 (navigation targets, initial focus, accessible names, contrast over the theme's declared palettes, touch target size, labelledBy) with their severities; the contrast of the automatic colour-vision correction is Z2-R-UI-142's.
- **Covers:** Z2-I-UI-03, Z2-I-UI-18
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_UIDocumentModel_ValidatesEveryRule` (rules 60–65).
- **Design:** Zenith2/Docs/Design/UI/Documents.md → "Validation rules"; Zenith2/Docs/Design/UI/Accessibility.md → "Accessibility validation"

### Z2-R-UI-165 Theme validation rules

- **Statement:** The validator reports rules 70–75 (class and token grammar, extends chains, palette completeness, font tokens) with their severities.
- **Covers:** Z2-I-UI-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_UIDocumentModel_ValidatesEveryRule` (rules 70–75).
- **Design:** Zenith2/Docs/Design/UI/Documents.md → "Validation rules"; Zenith2/Docs/Design/UI/Documents.md → "Theme document schema"

### Z2-R-UI-166 Font family validation rules

- **Statement:** The font importer reports rules 80–84 (unopenable faces, duplicate faces, uncovered baked code points, page limit, MSDF ranges) with their severities.
- **Covers:** Z2-I-UI-05, Z2-I-UI-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_UIDocumentModel_ValidatesEveryRule` (rules 80–84) and `Test_FontBake_PageLimit`.
- **Design:** Zenith2/Docs/Design/UI/Documents.md → "Validation rules"; Zenith2/Docs/Design/UI/Documents.md → "Font family document schema"

### Z2-R-UI-167 Text stack selection harness

- **Statement:** The D-SEL-TEXT harness ticket, scheduled in the Stage 2 sprint and blocked by the SA break-offset recording and by slices 2, 5, 7 and 9, whose code its selection set and benchmarks run, writes no candidate code (the bidi and segmentation candidates are slice 3's, the outline candidates slice 5's) and adds the text benchmarks of `Zenith_Bench_UI` with its smoke CTest and `Test_UITextSelection_CorpusBothCandidates`, which runs the test-font cases, the Bidi group's levels, the Graphemes, Fallback, Pseudo and Breaks groups (every SA case against the recorded ICU4C offsets) and the outline equivalence check on both stacks and writes the per-stack report; the test fails only on a harness error, never because a stack loses, and the ticket records no measurement.
- **Covers:** Z2-I-UI-05, Z2-I-UI-06, Z2-I-UI-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2), Z2-D-084
- **Budgets:** Z2-B-UI-001, Z2-B-UI-002
- **Acceptance evidence:** `Test_UITextSelection_CorpusBothCandidates` (`ZenithUI_Tests`) and the `Zenith_Bench_UI --smoke` CTest, both in `Test`.
- **Design:** Zenith2/Docs/Design/UI.md → "Selection decisions UI owns"; Zenith2/Docs/Design/UI/Tests.md → "Shaping golden suite"

### Z2-R-UI-168 Runtime selection harness

- **Statement:** The D-SEL-UIRT harness ticket, scheduled in the Stage 2 sprint and blocked by slices 7, 8 and 9, writes no candidate code (both layout and style engines are slice 7's, behind `Zenith_UIRuntimeBackend` inside the shared shell) and adds the menu, HUD and composite benchmarks of `Zenith_Bench_UI` with its smoke CTest and `Test_UIRuntimeSelection_ConformanceBothCandidates`, which runs the CSS and ZenithUI-semantics cases and the shell cases' absolute expectations (class `shell`, recorded and not judged) on both engines and writes per-case results by class and every shim with its code size to the harness report; the test fails only on a harness error, never because an engine loses, the shell cases gate on each engine in `Test_UILayoutGolden_AllCases` only in their invariant form, and the ticket records no measurement.
- **Covers:** Z2-I-UI-01, Z2-I-UI-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2), Z2-D-084
- **Budgets:** Z2-B-UI-002, Z2-B-UI-003
- **Acceptance evidence:** `Test_UIRuntimeSelection_ConformanceBothCandidates` (`ZenithUI_Tests`) and the `Zenith_Bench_UI --smoke` CTest, both in `Test`.
- **Design:** Zenith2/Docs/Design/UI.md → "Selection decisions UI owns"; Zenith2/Docs/Design/UI/Layout.md → "Runtime selection"; Zenith2/Docs/Design/UI/Layout.md → "Layout conformance cases"

### Z2-R-UI-169 W-UI2D measured at Stage 3

- **Statement:** W-UI2D is measured with the Stage 3 features on the reference desktop with both backends and on the S24, and the UI rows hold.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-083
- **Budgets:** Z2-B-UI-001, Z2-B-UI-002, Z2-B-UI-003
- **Acceptance evidence:** The Stage 3 acceptance run through `Run-Committed.ps1 -Acceptance`, with the device steps `needs-human`.
- **Design:** Zenith2/Docs/Design/UI/Tests.md → "Acceptance runs"; Zenith2/Docs/Design/UI.md → "Rows owned"

### Z2-R-UI-170 Pause-menu replay

- **Statement:** A replay passes a `MENU` pause exactly where the recorded run resumed: under `--replay` the live `resumeWorld` action allocates nothing and is ignored, and UI clears `MENU` in prologue step 1 when the recorded publications of the world's next tick hold a resume record; because replays drive the clock in external-tick mode from the log, that tick follows the same frame boundary as live, so the replay's ticks and hashes match the recording.
- **Covers:** Z2-I-UI-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_UIPauseMenu_ResumeClearsMenuReason` (replay half, `Zenith_Integration_UI`, `Test`).
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Pause menus"; Zenith2/Docs/Design/JOBS.md → "Determinism obligations"

### Z2-R-UI-171 Quality matrix rows

- **Statement:** UI supplies the Stage 1 QualityMatrix ticket its `## UI features` rows (MSDF text, world-space panels, HDR UI, reduced motion, colour-vision correction) with techniques per tier on Windows and the S24, fallbacks, acceptance kinds and requirement IDs.
- **Covers:** Z2-I-UI-16, Z2-I-UI-21, Z2-I-UI-22
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** The `## UI features` rows of `Zenith2/Docs/QualityMatrix.md` written by the Stage 1 QualityMatrix ticket.
- **Design:** Zenith2/Docs/Design/UI.md → "Content specifications and quality matrix rows"

### Z2-R-UI-173 SA break offsets recorded before the text ruling

- **Statement:** A `needs-human` recording ticket in the Stage 2 sprint, blocked by the ticket that commits the corpus case texts and the SA record case and blocking the D-SEL-TEXT harness ticket, runs the committed record case `Test_UITextCorpus_SaBreaks_Record` on a clean checkout, which records with ICU4C's line break iterator (the pinned `icu4c` entry, the ICU release that publishes the pinned `icu-dictionaries` data) the `allowed` and `mandatory` UTF-8 break offsets of every SA case of the corpus and the group's `icuVersion` and `recordedWith` members, after the human removed any case that uses a word absent from the pinned word list with its reason, and commits the corpus files alone, so the comparison of SA segmentation in D-SEL-TEXT has an oracle that depends on no candidate and a re-recording after an upgrade repeats the same procedure.
- **Covers:** Z2-I-UI-05, Z2-I-UI-26
- **Stage:** S2
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2), Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** The recording ticket's committed `breaks`, `icuVersion`, `recordedWith` and `removed` members of the SA group files (`needs-human`); `Test_UITextSelection_CorpusBothCandidates` and `Test_UITextCorpus_Breaks` treat a listed SA case without `breaks` as a harness error (`ZenithUI_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/UI/Text.md → "Text conformance corpus"; Zenith2/Docs/Design/UI/Tests.md → "Shaping golden suite"; Zenith2/Docs/Design/UI.md → "Ticket slices and ordering"

### Z2-R-UI-174 Resume capacity reserved

- **Statement:** Each world's per-frame and in-flight publication limits keep one slot for a `UI_EVENT_KIND_WORLD_RESUMED` record (ordinary events get `ui.events.maxPerFrame − 1` and `ui.events.maxInFlight − 1`, the ordinary in-flight count being `GetOutstandingCount() − 1` while `Zenith_UIWorldRecord::m_bResumeOutstanding` is set), at most one resume record is outstanding per world (the flag is set when the record's `Park` returns true and cleared when the handler decodes it), the claimant recognises a resume action by the drained event record's `m_uPresentationAction`, and a channel full of ordinary events never prevents a pause menu from resuming or lets `MENU` clear without its record in the log.
- **Covers:** Z2-I-UI-01, Z2-I-UI-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_UIEvents_ChannelFullDropsAndCounts` and `Test_UIPauseMenu_ResumeClearsMenuReason` (the full-channel and refused-record cases), `Zenith_Integration_UI`, `Test`.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Publication path"; Zenith2/Docs/Design/UI/Interaction.md → "Pause menus"; Zenith2/Docs/Design/UI.md → "Configuration keys"

### Z2-R-UI-175 Live input allocates nothing under replay

- **Statement:** Under `--replay` UI drops every live event before calling `AllocateCompletionID` and counts it (`ui.replay.live_events_ignored`), so live viewer input never advances the world's completion-ID counter, which every source kind of the world shares; the IDs UI allocated live in the recorded run are reproduced by the replaying table, which adopts the recorded next completion ID at each tick boundary (cross-doc item X278), so every subsequent allocation of the world, a recomputed kind's included, receives its recorded ID and no replay halts on a divergence.
- **Covers:** Z2-I-UI-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_UIEvents_ReplayIdentical` (live clicks and drags injected during the replay allocate nothing; an additive scene load requested after a recorded click gets its recorded completion ID and publication tick; no divergence halt), `Zenith_Integration_UI`, `Test`. Its ticket is blocked by the SCRIPT ticket that lands X278 in [Z2-N-SCRIPT-341] and the replay mode of the asynchronous result table, and stays `draft` until then.
- **Design:** Zenith2/Docs/Design/UI/Interaction.md → "Publication path"; Zenith2/Docs/Design/UI/Interaction.md → "Pause menus"

### Z2-R-UI-176 Stage 2 pull-forward

- **Statement:** Slices 1, 2, 3, 5, 7, 8 and 9 of "Ticket slices and ordering" are scheduled in the `Stage 2 — Foundation` sprint with fix version `Z2 Stage 3`, gated by the manifest data mode and content root of row UI9, and the two harness tickets are blocked by the slices whose code they run; the Stage 2 harnesses run only the benchmarks and conformance cases those slices provide, and every benchmark or case that needs a subsequent slice is added by that slice's ticket.
- **Covers:** Z2-I-UI-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2), D-SEL-UIRT (open, Stage 1/2), Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** The filed tickets of those slices (sprint `Stage 2 — Foundation`, fix version `Z2 Stage 3`) and the harness tickets' blockers on them, checked by the drafts checker's coverage run; the `Zenith_Bench_UI --smoke` CTest at Stage 2 runs exactly the Stage 2 benchmarks of the slice table (`Test`).
- **Design:** Zenith2/Docs/Design/UI.md → "Stage table"; Zenith2/Docs/Design/UI.md → "Ticket slices and ordering"; Zenith2/Docs/Design/UI/Tests.md → "Benchmarks and harnesses"

### Z2-R-UI-178 Shared style resolution for both runtime candidates

- **Statement:** The shell's `Zenith_UIStyleResolver` (`Internal/Zenith_UIStyleResolver.{h,cpp}`, part of no candidate) resolves every catalogue property of every element into `Zenith_UIComputedStyle` for both D-SEL-UIRT candidates; the custom runtime's layout reads the layout-affecting fields from it; the RmlUi integration's `ResolveStyles` runs RmlUi's own cascade for the layout-affecting properties (IDs 1 to 29 and 40 to 48) from the cook-generated RCSS of the RMLUI section, which reproduces cascade steps 1 and 3 to 6 with class selectors of equal specificity (`.z`, composite `.k<m>` rules per class sequence, `.e<index>` inline rules, then the state rules in the fixed priority order under the `z-<state>` pseudo-classes) and never an `id` selector or a `style` attribute, and pushes the run-time inputs each frame (state flags as pseudo-classes, `visible` as `display: none`, token, binding, animated and margin-exchanged values as inline properties); the RMLUI section is cooked into every document while D-SEL-UIRT is open and after an RmlUi ruling.
- **Covers:** Z2-I-UI-01, Z2-I-UI-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UIStyle_CascadeOrder` (the shell resolver, one run whichever candidates are compiled); `Test_UIRmlUiSection_RcssCascadeOrder` (the generated sheet's rule order, selectors and pseudo-class names) and `Test_UIDocumentCompile_SectionsMatchFormat` (the RMLUI section present while D-SEL-UIRT is open), `ZenithUI_Tests`, `Test`; the RmlUi candidate's layouts of the conformance cases in the D-SEL-UIRT selection report.
- **Design:** Zenith2/Docs/Design/UI/Layout.md → "What both candidates implement"; Zenith2/Docs/Design/UI/Layout.md → "The RmlUi integration"; Zenith2/Docs/Design/UI/Documents.md → "UI document sections"

### Z2-R-UI-179 SA break-offset record case

- **Statement:** Slice 3 commits the skipped record case `Test_UITextCorpus_SaBreaks_Record` in `ZenithCook_Tests`, linked to the usage-tests manifest entry `icu4c` alone (the ICU release that publishes the pinned `icu-dictionaries`, moving with it), which, run with `--no-skip` and `ZENITH2_UI_RECORD_SA_BREAKS`, records every case of every SA group of `Groups.json` with `BreakIterator::createLineInstance(Locale("th"))` over the case text as UTF-16, classes a boundary whose rule status is at least `UBRK_LINE_HARD` as mandatory and every other as allowed, converts UTF-16 indices to UTF-8 byte offsets, drops 0 and the end, and writes the `breaks`, `icuVersion` and `recordedWith` members and nothing else; its recording function is checked in every gate on texts whose ICU4C breaks follow from UAX #14 alone.
- **Covers:** Z2-I-UI-05, Z2-I-UI-26
- **Stage:** S2
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UITextCorpus_SaBreakRecorderConverts` (`ZenithCook_Tests`, `Test`); the record case's output, committed by the recording ticket of Z2-R-UI-173 (`needs-human`).
- **Design:** Zenith2/Docs/Design/UI/Text.md → "Corpus file format"; Zenith2/Docs/Design/UI/Tests.md → "Shaping golden suite"; Zenith2/Docs/Design/UI.md → "ZenithCook UI files"

## Coverage of inventory items

| Item | Requirements |
|---|---|
| Z2-I-UI-01 | Z2-R-UI-001, Z2-R-UI-002, Z2-R-UI-003, Z2-R-UI-004, Z2-R-UI-005, Z2-R-UI-006, Z2-R-UI-008, Z2-R-UI-009, Z2-R-UI-010, Z2-R-UI-011, Z2-R-UI-012, Z2-R-UI-014, Z2-R-UI-015, Z2-R-UI-016, Z2-R-UI-018, Z2-R-UI-019, Z2-R-UI-020, Z2-R-UI-021, Z2-R-UI-022, Z2-R-UI-024, Z2-R-UI-025, Z2-R-UI-026, Z2-R-UI-040, Z2-R-UI-041, Z2-R-UI-074, Z2-R-UI-083, Z2-R-UI-084, Z2-R-UI-085, Z2-R-UI-086, Z2-R-UI-087, Z2-R-UI-088, Z2-R-UI-089, Z2-R-UI-090, Z2-R-UI-091, Z2-R-UI-092, Z2-R-UI-093, Z2-R-UI-094, Z2-R-UI-095, Z2-R-UI-096, Z2-R-UI-097, Z2-R-UI-098, Z2-R-UI-099, Z2-R-UI-100, Z2-R-UI-101, Z2-R-UI-102, Z2-R-UI-103, Z2-R-UI-104, Z2-R-UI-107, Z2-R-UI-109, Z2-R-UI-110, Z2-R-UI-154, Z2-R-UI-155, Z2-R-UI-156, Z2-R-UI-157, Z2-R-UI-158, Z2-R-UI-159, Z2-R-UI-160, Z2-R-UI-162, Z2-R-UI-163, Z2-R-UI-168, Z2-R-UI-169, Z2-R-UI-174, Z2-R-UI-175, Z2-R-UI-176, Z2-R-UI-178, Z2-R-UI-180 |
| Z2-I-UI-02 | Z2-R-UI-015, Z2-R-UI-017, Z2-R-UI-028, Z2-R-UI-029, Z2-R-UI-030, Z2-R-UI-031, Z2-R-UI-032, Z2-R-UI-033, Z2-R-UI-034, Z2-R-UI-035, Z2-R-UI-036, Z2-R-UI-168, Z2-R-UI-178 |
| Z2-I-UI-03 | Z2-R-UI-036, Z2-R-UI-067, Z2-R-UI-071, Z2-R-UI-072, Z2-R-UI-073, Z2-R-UI-074, Z2-R-UI-075, Z2-R-UI-145, Z2-R-UI-164 |
| Z2-I-UI-04 | Z2-R-UI-039, Z2-R-UI-068, Z2-R-UI-069, Z2-R-UI-070, Z2-R-UI-092 |
| Z2-I-UI-05 | Z2-R-UI-013, Z2-R-UI-032, Z2-R-UI-043, Z2-R-UI-044, Z2-R-UI-045, Z2-R-UI-046, Z2-R-UI-048, Z2-R-UI-049, Z2-R-UI-050, Z2-R-UI-051, Z2-R-UI-055, Z2-R-UI-056, Z2-R-UI-057, Z2-R-UI-058, Z2-R-UI-059, Z2-R-UI-060, Z2-R-UI-061, Z2-R-UI-063, Z2-R-UI-104, Z2-R-UI-106, Z2-R-UI-108, Z2-R-UI-166, Z2-R-UI-167, Z2-R-UI-173, Z2-R-UI-179 |
| Z2-I-UI-06 | Z2-R-UI-013, Z2-R-UI-043, Z2-R-UI-047, Z2-R-UI-057, Z2-R-UI-060, Z2-R-UI-062, Z2-R-UI-121, Z2-R-UI-166, Z2-R-UI-167 |
| Z2-I-UI-07 | Z2-R-UI-043, Z2-R-UI-052, Z2-R-UI-053, Z2-R-UI-054, Z2-R-UI-063, Z2-R-UI-167 |
| Z2-I-UI-08 | Z2-R-UI-111, Z2-R-UI-112, Z2-R-UI-113, Z2-R-UI-114, Z2-R-UI-115, Z2-R-UI-118, Z2-R-UI-119, Z2-R-UI-120, Z2-R-UI-121, Z2-R-UI-122, Z2-R-UI-123, Z2-R-UI-124, Z2-R-UI-128, Z2-R-UI-129, Z2-R-UI-163, Z2-R-UI-177, Z2-R-UI-181 |
| Z2-I-UI-09 | Z2-R-UI-114, Z2-R-UI-116, Z2-R-UI-117 |
| Z2-I-UI-10 | Z2-R-UI-007, Z2-R-UI-027, Z2-R-UI-143 |
| Z2-I-UI-11 | Z2-R-UI-138, Z2-R-UI-139 |
| Z2-I-UI-12 | Z2-R-UI-080, Z2-R-UI-081, Z2-R-UI-118, Z2-R-UI-147 |
| Z2-I-UI-13 | Z2-R-UI-007, Z2-R-UI-076, Z2-R-UI-130, Z2-R-UI-131, Z2-R-UI-142, Z2-R-UI-143, Z2-R-UI-144 |
| Z2-I-UI-14 | Z2-R-UI-014, Z2-R-UI-018, Z2-R-UI-023, Z2-R-UI-151, Z2-R-UI-152, Z2-R-UI-153, Z2-R-UI-182 |
| Z2-I-UI-15 | Z2-R-UI-037, Z2-R-UI-038, Z2-R-UI-039, Z2-R-UI-177 |
| Z2-I-UI-16 | Z2-R-UI-042, Z2-R-UI-066, Z2-R-UI-101, Z2-R-UI-105, Z2-R-UI-171 |
| Z2-I-UI-17 | Z2-R-UI-009, Z2-R-UI-064, Z2-R-UI-065, Z2-R-UI-066, Z2-R-UI-067, Z2-R-UI-068, Z2-R-UI-077, Z2-R-UI-078, Z2-R-UI-079, Z2-R-UI-082, Z2-R-UI-089, Z2-R-UI-161, Z2-R-UI-170, Z2-R-UI-174 |
| Z2-I-UI-18 | Z2-R-UI-132, Z2-R-UI-133, Z2-R-UI-134, Z2-R-UI-145, Z2-R-UI-164, Z2-R-UI-172 |
| Z2-I-UI-19 | Z2-R-UI-134, Z2-R-UI-135, Z2-R-UI-137 |
| Z2-I-UI-20 | Z2-R-UI-136, Z2-R-UI-137, Z2-R-UI-172 |
| Z2-I-UI-21 | Z2-R-UI-140, Z2-R-UI-171 |
| Z2-I-UI-22 | Z2-R-UI-016, Z2-R-UI-141, Z2-R-UI-142, Z2-R-UI-165, Z2-R-UI-171 |
| Z2-I-UI-23 | Z2-R-UI-138, Z2-R-UI-139 |
| Z2-I-UI-24 | Z2-R-UI-125 |
| Z2-I-UI-25 | Z2-R-UI-050, Z2-R-UI-125, Z2-R-UI-126 |
| Z2-I-UI-26 | Z2-R-UI-053, Z2-R-UI-054, Z2-R-UI-120, Z2-R-UI-125, Z2-R-UI-127, Z2-R-UI-173, Z2-R-UI-179 |
| Z2-I-UI-27 | Z2-R-UI-146, Z2-R-UI-150 |
| Z2-I-UI-28 | Z2-R-UI-147, Z2-R-UI-150 |
| Z2-I-UI-29 | Z2-R-UI-148, Z2-R-UI-150 |
| Z2-I-UI-30 | Z2-R-UI-149, Z2-R-UI-150 |

## Excluded and deferred

No UI inventory item is excluded or deferred: all thirty are required at Stage 3 or Stage 4. Outside the inventory, these capabilities are deliberately not provided, so no requirement asks for them:

| Capability | Reason |
|---|---|
| date, time, currency, unit and compact-number formatting in messages | games format such values in content or in Luau before passing them as text arguments; the message format stays the ICU subset of `Zenith2/Docs/Design/UI/Documents.md → "Message syntax"` |
| an HTML or CSS document model, or script inside UI documents | documents are declarative; behaviour lives in Luau gameplay modules (Z2-D-077) |
| vertical text layout (CJK top-to-bottom) | not needed by the flagship slices (Z2-D-021); horizontal layout covers every supported locale |
| offscreen group opacity for translucent containers | each quad blends individually (`Zenith2/Docs/Design/UI/Rendering.md → "Paint order"`) |
| rounded clip masks | clips are rectangular (`Zenith2/Docs/Design/UI/Rendering.md → "Clipping"`) |
