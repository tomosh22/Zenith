# UI design: retained UI, text, localization and accessibility

Entry document of the UI area. UI is the `ZenithUI` module and the UI-owned files inside ZenithFlux and ZenithCook: retained screens built from authored documents, layout, styling, text shaping and font rendering, input claims, focus and navigation, localization, captions and accessibility, and the GPU passes that draw it. The section files under `Zenith2/Docs/Design/UI/` hold the detail; this file holds the scope, the top-level contracts and the indexes.

## Purpose and scope

### What UI owns

- The `ZenithUI` module (`KIND MODULE`, `MODULE_NAME UI`): the process-wide `Zenith_UISystem` (text engine, font registry, glyph cache, localization) and one `Zenith_UIContext` per world, holding that world's open screens.
- Screens as ECS entities: the component `Zenith.UI.Screen` names a cooked UI document and its presentation state; spawning, layering and closing a screen are ordinary recorded simulation writes.
- The retained runtime behind the context: a shared shell (focus, navigation, controls, the semantic tree, virtualization, world-space panels, RTL mirroring, rendering, text) around the layout and style engine D-SEL-UIRT (open, Stage 1/2) selects, RmlUi's or ZenithUI's own (`Zenith2/Docs/Design/UI/Layout.md → "Runtime selection"`); style resolution, layout (stack, grid, anchored), data binding, animations.
- Text: Unicode data, UTF-8 decoding, bidi, itemization, font fallback, shaping, line breaking, truncation, caret and editing (D-SEL-TEXT (open, Stage 1/2)), MSDF glyph atlases (baked and dynamic).
- Input: the presentation claimant that takes pointer, navigation and text input from PLAT before gameplay, hit testing, the pointer and focus state machines, directional navigation, controls, text fields with IME sessions, input prompts, rebinding UI, on-screen touch controls.
- The two crossings with simulation: UI events published as recorded asynchronous results (`ASYNC_SOURCE_UI`, `Zenith2/Docs/Scripting.md → "Asynchronous source kind registry"`) and delivered as ECS events; simulation-to-UI requests as six command kinds; component bindings read at extraction.
- Localization: locales, string tables, the message format with plural and select, number formatting, extraction, XLIFF exchange, pseudo-localization.
- Accessibility: settings, the semantic tree and its provider for PLAT's accessibility service, narration, live regions, captions and subtitles, reduced motion, colour-vision modes, contrast, scaling, the accessible-flow acceptance suites.
- The GPU side in ZenithFlux: `Flux_UI.h`, `Flux_UIRenderer`, the `UIComposite` pass (Rendering's per-pass table) and the `UIWorldSpace` pass, the UI Slang programs.
- The cook side in ZenithCook: five authoring document types, their migration chains, the font importer, the UI cook steps, the binding manifest, extraction and XLIFF.
- Asset kinds `UI.Font` (12), `UI.Document` (13) and `UI.LocalizationTable` (20) and their loaders.
- The workload W-UI2D: its Stage 1 workload scale ticket is UI's ([Z2-N-BUDGET-195]).

### Explicit exclusions

| Not UI's | Owner |
|---|---|
| input devices, actions, bindings, contexts, IME sessions, display metrics, safe area, windowing | PLAT (`Zenith2/Docs/Design/PLAT/Input.md`, `Zenith2/Docs/Design/PLAT/Platform.md`) |
| speech synthesis, the OS accessibility APIs (UI Automation, `AccessibilityNodeProvider`, SAPI, `TextToSpeech`), OS accessibility preferences and the recording implementation tests use | PLAT: `Zenith_AccessibilityService`, its value types, the snapshot, every OS API, the OS preferences and the recording implementation (`Zenith2/Docs/Design/PLAT/Accessibility.md → "Ownership split with UI"`); the Java side of the Android provider is MOD's Gradle template |
| loading, residency, hot reload and the cooked-file header of assets | ASSET (`Zenith2/Docs/Design/ASSET.md`) |
| textures used as sprites (GEOM's texture kind; UI only references them) | GEOM |
| the frame graph's GPU front end, per-view composition, extraction ordering rules | RPROD |
| output encoding, tonemapping, HDR metadata | LIGHT (`Zenith2/Docs/Design/LIGHT/ColourManagement.md → "UI and HDR"`) |
| the Luau binding ABI and invocation transactions | SCRIPT |
| the editor's own tool interface and the UI authoring tool built on `Zenith_UIEditorModel` | EDIT (`Zenith2/Docs/Design/EDIT.md`, pending), Z2-D-079 |
| the debug overlay, the profiler UI, the frame dump and the console | DEVT (`Zenith2/Docs/Design/DEVT.md`); UI registers a frame-dump section and declares console-settable debug keys |
| sprites, tilemaps and 2D cameras | TWOD (`Zenith2/Docs/Design/TWOD.md`, pending) |
| dialogue systems and caption timing data | AUDIO and CINE (pending); UI presents their cues |
| pause arbitration and game-state flow (menus open screens; GAME owns what they mean); UI only clears the `MENU` reason a pause menu set | GAME (`Zenith2/Docs/Design/GAME/WorldHost.md → "Pause arbitration"`; rows GA13, UI19) |
| dates, currencies, units and compact numbers in messages | not provided: games format these in content; recorded in "Excluded and deferred" of the requirements |

### Inventory items covered

| Item | Where |
|---|---|
| Z2-I-UI-01 retained UI | `Zenith2/Docs/Design/UI/Layout.md → "Runtime selection"`, `Zenith2/Docs/Design/UI/PublicAPI.md → "Zenith_UIContext header"` |
| Z2-I-UI-02 UI layout | `Zenith2/Docs/Design/UI/Layout.md → "Layout algorithm"` |
| Z2-I-UI-03 navigation | `Zenith2/Docs/Design/UI/Interaction.md → "Directional navigation algorithm"`, "Tab order" |
| Z2-I-UI-04 focus | `Zenith2/Docs/Design/UI/Interaction.md → "Focus model"` |
| Z2-I-UI-05 text shaping | `Zenith2/Docs/Design/UI/Text.md → "Shaping"` |
| Z2-I-UI-06 font fallback | `Zenith2/Docs/Design/UI/Text.md → "Font fallback"` |
| Z2-I-UI-07 bidirectional text | `Zenith2/Docs/Design/UI/Text.md → "Bidirectional resolution"` |
| Z2-I-UI-08 localization extraction | `Zenith2/Docs/Design/UI/Localization.md → "Localization extraction"` |
| Z2-I-UI-09 pluralization | `Zenith2/Docs/Design/UI/Localization.md → "Plural rules"` |
| Z2-I-UI-10 UI scaling | `Zenith2/Docs/Design/UI/Layout.md → "Canvas units and scaling"`, `Zenith2/Docs/Design/UI/Accessibility.md → "UI scaling and text scaling"` |
| Z2-I-UI-11 captions | `Zenith2/Docs/Design/UI/Accessibility.md → "Caption cues"` |
| Z2-I-UI-12 input remapping | `Zenith2/Docs/Design/UI/Interaction.md → "Input prompts and remapping"` |
| Z2-I-UI-13 accessibility settings | `Zenith2/Docs/Design/UI/Accessibility.md → "Accessibility settings"` |
| Z2-I-UI-14 UI visual authoring | `Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIEditorModel header"`, "Authoring and cooking" below |
| Z2-I-UI-15 large-list virtualization | `Zenith2/Docs/Design/UI/Layout.md → "Large-list virtualization"` |
| Z2-I-UI-16 world-space UI | `Zenith2/Docs/Design/UI/Layout.md → "World-space panels"`, `Zenith2/Docs/Design/UI/Rendering.md → "UIWorldSpace pass"` |
| Z2-I-UI-17 input arbitration | `Zenith2/Docs/Design/UI/Interaction.md → "Input arbitration with gameplay"` |
| Z2-I-UI-18 semantic accessibility information | `Zenith2/Docs/Design/UI/Accessibility.md → "Semantic tree"` |
| Z2-I-UI-19 screen-reader hooks | `Zenith2/Docs/Design/UI/Accessibility.md → "Platform accessibility bridge"` |
| Z2-I-UI-20 TTS hooks | `Zenith2/Docs/Design/UI/Accessibility.md → "Narration composition"` |
| Z2-I-UI-21 reduced motion | `Zenith2/Docs/Design/UI/Accessibility.md → "Reduced motion"` |
| Z2-I-UI-22 colour-blind modes | `Zenith2/Docs/Design/UI/Accessibility.md → "Colour-vision modes and contrast"` |
| Z2-I-UI-23 subtitles and captions | `Zenith2/Docs/Design/UI/Accessibility.md → "Caption presentation"` |
| Z2-I-UI-24 pseudo-localization | `Zenith2/Docs/Design/UI/Localization.md → "Pseudo-localization and stress modes"` |
| Z2-I-UI-25 long strings | same, long mode, and the layout suite's pseudo-long variants |
| Z2-I-UI-26 RTL | `Zenith2/Docs/Design/UI/Text.md → "Line layout and reordering"`, `Zenith2/Docs/Design/UI/Localization.md → "Localization validation"` |
| Z2-I-UI-27 to Z2-I-UI-30 accessible flows | `Zenith2/Docs/Design/UI/Accessibility.md → "End-to-end accessible flows"` |

### Document family

| File | Holds | Headings |
|---|---|---|
| `Zenith2/Docs/Design/UI/PublicAPI.md` | the header list and the module, types, components, events, system, context, input, draw-list, document and style headers | Scope of this file; Header list; Zenith_Module_UI header; Zenith_UITypes header; Zenith_UIComponents header; Zenith_UIEvents header; Zenith_UISystem header; Zenith_UIContext header; Zenith_UIInput header; Zenith_UIDrawList header; Zenith_UIDocument header; Zenith_UIStyle header |
| `Zenith2/Docs/Design/UI/ServicesAPI.md` | text, fonts, baking, localization, accessibility, script and editor headers, `Flux_UI.h`, the tool-side headers | Scope of this file; Zenith_UIText header; Zenith_UIFont header; Zenith_UIFontBake header; Zenith_UILocalization header; Zenith_UIAccessibility header; Zenith_UIScriptApi header; Zenith_UIEditorModel header; Flux_UI header; Tool-side headers |
| `Zenith2/Docs/Design/UI/Documents.md` | authoring schemas, validation rules, cooked formats | Authoring document family; Common authoring rules; UI document schema; Theme document schema; Style property catalogue; Font family document schema; Localization project document schema; String table document schema; Message syntax; Canonical schemas and document types; Migration chains and fixtures; Validation rules; Cooked UI document format (including Typed data records); Cooked font format; Cooked localization table format; Binding manifest file; Parser limits |
| `Zenith2/Docs/Design/UI/Layout.md` | runtime selection, element table, styles, layout, invalidation, virtualization, bindings, animation, world panels, the layout conformance cases | Runtime selection (What both candidates implement, The custom runtime, The RmlUi integration); Element tree model; Canvas units and scaling; Style resolution; Layout algorithm (including Logical direction and mirroring); Invalidation and incremental layout; Large-list virtualization; Data binding evaluation; Animations and transitions; World-space panels; Safe area and surface changes; Layout conformance cases |
| `Zenith2/Docs/Design/UI/Text.md` | the text stack, Unicode data, the paragraph pipeline, caret, editing, fonts, glyph cache, the corpus | Text stack selection; Unicode data; Paragraph pipeline; Caret and selection; Text editing; Font registry and faces; Glyph cache and atlases; Text layout cache; Text conformance corpus |
| `Zenith2/Docs/Design/UI/Interaction.md` | frame flow, claims, hit testing, pointer and focus, navigation, controls, text fields, arbitration, prompts, on-screen controls, the simulation crossings | Frame flow; Input claims; Hit testing; Pointer state machine; Focus model; Directional navigation algorithm; Tab order; Activation and value controls; Scrolling; Text fields and IME; Input arbitration with gameplay; Input prompts and remapping; On-screen controls; UI events to simulation (Event kinds and payloads, Publication path, Pause menus, Two-way bindings); Requests from simulation |
| `Zenith2/Docs/Design/UI/Rendering.md` | draw lists, the quad record, render-world records, the GPU side | Draw list construction; Flux_UIQuad layout; Render-world records; Flux UI renderer; UIComposite implementation; UIWorldSpace pass; Shader programs; Atlas pages on the GPU; Colour and HDR output; Headless and device loss |
| `Zenith2/Docs/Design/UI/Accessibility.md` | settings, semantic tree, narration, captions, motion, colour, scaling, validation, flows | Accessibility settings; Semantic tree; Narration; Captions and subtitles; Reduced motion; Colour-vision modes and contrast; UI scaling and text scaling; Accessibility validation; End-to-end accessible flows |
| `Zenith2/Docs/Design/UI/Localization.md` | locales, tables, formatting, switching, fonts per locale, extraction, pseudo modes, validation | Locales and negotiation; String tables and keys; Message formatting; Number formatting; Locale switching at run time; Font selection per locale; Localization extraction; Pseudo-localization and stress modes; Localization validation |
| `Zenith2/Docs/Design/UI/Tests.md` | the test catalogue | Test targets and files; Fixtures; Unit tests; Layout golden suite; Shaping golden suite; Render tests; Integration tests; Fuzz targets; Benchmarks and harnesses; Acceptance runs; Gate steps and labels |
| `Zenith2/Docs/Requirements/UI.md` | the requirement catalogue `Z2-R-UI-NNN`, first file: the purpose, the requirements of Stages 1 to 3, the coverage of inventory items over both files and the exclusions | Purpose; Requirements; Coverage of inventory items; Excluded and deferred |
| `Zenith2/Docs/Requirements/UI/Stage4Plus.md` | the catalogue's second file, one numbering with the first: the requirements of Stages 4 to 6 | Purpose; Requirements |

### Conventions

- "Presentation" means state that never reaches simulation: layout, focus, hover, scroll offsets, animation time, text, accessibility output. Simulation reaches presentation through component reads and commands, and presentation reaches simulation only through recorded publications; every UI requirement on determinism follows from that split (Z2-D-019, Z2-D-053).
- Canvas units are dp unless a document uses `reference` scale mode (Layout.md "Canvas units and scaling"); pixels are surface pixels.
- "The extractor" is `UI.Extract@W<tag>`; "the claimant" is `Zenith_UIInputClaimant`.

## Dependencies

### Libraries UI code depends on

`ZenithUI` declares exactly twelve entries: `ZenithRuntime`, `ZenithFlux`, `ZenithShadersGenerated`, `ZenithRHI`, `ZenithPlatform`, `ZenithAssets`, `ZenithScene`, `ZenithECS`, `ZenithReflect`, `ZenithVertexFormat`, `ZenithCore`, `ZenithProfiling`: the row of `Zenith2/Docs/Architecture.md → "Library dependency graph"` plus `ZenithVertexFormat`, which the extractor needs for `Zenith_F32ToF16` (`Zenith2/Docs/Design/UI/Rendering.md → "Flux_UIQuad layout"`), and `ZenithRHI`, whose shader-interface value type `Zenith_RHIDescriptorHandle` the public `Zenith_GlyphPlacement` holds and the draw-list builder writes into `Flux_UIBatch::m_xTexture` (`Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIFont header"`), so `Zenith_UIFont.h` includes `ZenithRHI/Zenith_RHIShaderInterface.h` directly and [Z2-N-ARCH-167] requires the declared edge even though ZenithFlux reaches ZenithRHI already. Both additions are downward edges to libraries below every module, so the graph stays a DAG; UI requests them for that row ("Cross-area reconciliation"). It depends on no ZenithJobs API directly (its frame work runs in nodes the runtime schedules), on no ZenithScript header (script bindings register through `Zenith_ModuleStartContext::RegisterScriptBindings` with ZenithReflect's binding table), and on no ZenithDocument header at run time (authoring documents are ZenithCook's).

| Dependency | What UI uses |
|---|---|
| ZenithCore | containers, `Zenith_Span`, `Zenith_StringView`, `Zenith_StringTable` (`Zenith2/Docs/Design/CORE/Text.md`), `Zenith_Hash64`, arenas, `Zenith_ConfigStore` and `Zenith_ConfigSchema`, `Zenith_MemoryGovernor`, the fault injector, `Zenith_Colour` |
| ZenithReflect | type registration of the component, events, payloads and resources; the script binding table |
| ZenithECS, ZenithScene | systems, queries, events, command buffers, stable simulation IDs, prefabs carrying screens |
| ZenithAssets | asset kind loaders, counted handles, the cooked-file header |
| ZenithPlatform | `Zenith_InputSystem` claimant and rebinding API, `Zenith_InputDevices` virtual devices, `Zenith_TextInput`, display metrics and safe area, the preferred locale, `Zenith_AccessibilityService`, its value types and `Zenith_AccessibilityTreeProvider` (`Zenith2/Docs/Design/PLAT/Accessibility.md → "Zenith_AccessibilityService"`), the `platform.accessibility.*` preference keys |
| ZenithVertexFormat | `Zenith_F32ToF16` for the quad record's half fields |
| ZenithRHI | `Zenith_RHIDescriptorHandle` (`ZenithRHI/Zenith_RHIShaderInterface.h`), held by `Zenith_GlyphPlacement` and written into `Flux_UIBatch` |
| ZenithFlux, ZenithShadersGenerated | the render-world writer, `Flux_UIRenderer` and the UI passes (UI-owned files), the generated program headers |
| ZenithRuntime | the module interfaces (`Zenith2/Docs/Design/MOD.md → "Zenith_Module.h"`), the module publication sink (row UI1, pending MOD), `Zenith_WorldControl` (`Zenith2/Docs/Design/GAME/WorldHost.md → "Pause arbitration"`) |
| ZenithProfiling (DEVT's files) | `Zenith_DevFrameDumpSection` (`Zenith2/Docs/Design/DEVT/Console.md → "DEVT frame dump"`) |
| ZenithProfiling | zones and counters |

### Third-party code and data

| Entry | Licence | Use | Decision |
|---|---|---|---|
| HarfBuzz | MIT | shaping (both D-SEL-TEXT candidates) | D-SEL-TEXT (open, Stage 1/2) |
| FreeType | FTL | outline loading for MSDF generation (candidate T1) | D-SEL-TEXT (open, Stage 1/2) |
| msdfgen | MIT | MTSDF generation | D-SEL-TEXT (open, Stage 1/2) |
| SheenBidi or an ICU4X subset | Apache-2.0 / Unicode-3.0 | bidi (candidates B1, B2) | D-SEL-TEXT (open, Stage 1/2) |
| RmlUi | MIT | the retained runtime candidate | D-SEL-UIRT (open, Stage 1/2) |
| `noto-fonts` (data) | OFL-1.1 | engine fallback families and the text corpus fonts | row UI9 |
| `unicode-ucd` (data) | Unicode-3.0 | the generated property tables | row UI9 |
| `cldr-json` (data) | Unicode-3.0 | plural rules, number symbols, likely subtags, character order | row UI9 |
| `icu-dictionaries` (data) | Unicode-3.0 | Thai, Lao, Khmer and Burmese break dictionaries | row UI9 |
| `icu4c` (`prebuilt`, `usage: tests`, owner `ZenithCook`) | Unicode-3.0 | the SA break record case alone, linked through `TEST_THIRD_PARTY` into `ZenithCook_Tests`; pinned to the ICU release that publishes the pinned `icu-dictionaries` data, so the two entries move together | the golden-file rule: an independent oracle for a non-derivable output |

All runtime code entries are compiled only into `ZenithUI/Internal` (Z2-D-082), and `icu4c` reaches no runtime target; the data entries need the manifest integration mode `data` and a `thirdparty:` content root (row UI9).

### Headings this area relies on

| Heading | For |
|---|---|
| `Zenith2/Docs/Design/PLAT/Input.md → "Presentation claims"` | the claimant interface |
| `Zenith2/Docs/Design/PLAT/Input.md → "Engine actions and contexts"` | `Engine.UI` and `Engine.TextEntry` |
| `Zenith2/Docs/Design/PLAT/Input.md → "Text input API"` | IME sessions |
| `Zenith2/Docs/Design/PLAT/Input.md → "Binding display"` | prompts |
| `Zenith2/Docs/Design/PLAT/Input.md → "Rebind operation"` | the rebind control |
| `Zenith2/Docs/Design/PLAT/Input.md → "Zenith_InputDevices API"` | the virtual device |
| `Zenith2/Docs/Design/RPROD/Views.md → "View extensions"` | the world-space pass |
| `Zenith2/Docs/Design/RPROD/Renderer.md → "Flux.BuildGraph"` | where `PrepareFrame` (step 4) and `WriteFrame` (step 11) run (row UI7 (b)) |
| `Zenith2/Docs/Design/RPROD/Renderer.md → "Flux_Renderer header"` | `Flux_Renderer::EnableUI` |
| `Zenith2/Docs/Design/RPROD/Views.md → "Flux_DepthMode header"` | the world-space depth comparison |
| `Zenith2/Docs/Design/RHI/Memory.md → "Build-node upload order"`, `Zenith2/Docs/Rendering/GpuResources.md → "Upload ring"` | the quad data path |
| `Zenith2/Docs/Rendering.md → "Shared producers"` | `Flux_FrameShared::m_pxUIFrame` ([Z2-N-REND-995]) |
| `Zenith2/Docs/Design/LIGHT/ColourManagement.md → "Output transforms"` | the output view formats and blending space |
| `Zenith2/Docs/Rendering/PerPass/Common.md → "UIComposite"` | the per-pass table UI implements |
| `Zenith2/Docs/Design/LIGHT/ColourManagement.md → "UI and HDR"` | `Flux_EncodeOutput` |
| `Zenith2/Docs/Design/LIGHT/Lighting.md → "Light units"` | pre-exposure for world panels |
| `Zenith2/Docs/Scripting/Bindings.md → "Binding registration and the thunk ABI"` | the Luau surface |
| `Zenith2/Docs/Design/MOD.md → "Zenith_Module.h"` | entry points and contexts |
| `Zenith2/Docs/Scripting.md → "Asynchronous source kind registry"` | `ASYNC_SOURCE_UI` ([Z2-N-SCRIPT-924], [Z2-N-SCRIPT-925]) |
| `Zenith2/Docs/Scripting/MisuseCatalogue.md → "Code ranges"` | UI's block ZS2100–ZS2199 ([Z2-N-SCRIPT-926]) |
| `Zenith2/Docs/Design/GAME/WorldHost.md → "Pause arbitration"` | `Zenith_WorldControl`, the `MENU` reason |
| `Zenith2/Docs/Design/DEVT/Console.md → "DEVT frame dump"`, `→ "Configuration variables"` | the `ui` dump section, console-settable debug keys |
| `Zenith2/Docs/Design/JOBS.md` (the `sim.*` keys) | clock pause semantics for publication |

### Upward-callback interfaces

UI declares no interface for a lower layer. It implements interfaces declared below it: `Zenith_InputClaimant` (PLAT), `Zenith_AccessibilityTreeProvider` (PLAT, `Zenith2/Docs/Design/PLAT/Accessibility.md → "Zenith_AccessibilityTreeProvider"`), `Flux_ViewExtension` (ZenithFlux), `Zenith_MemoryConsumer` (CORE), `Zenith_DevFrameDumpSection` (DEVT, in ZenithProfiling), and the module publication sink's handler interface (ZenithRuntime, pending MOD; row UI1), through which SCRIPT's runtime-side forwarder reaches UI's handler at tick-boundary step 1. Inside ZenithUI the internal interfaces `Zenith_UIRuntimeBackend`, `Zenith_UITextMeasurer`, `Zenith_UIShaper`, `Zenith_UIOutlineSource`, `Zenith_UIOutlineSink`, `Zenith_UIBidi` and `Zenith_UILineSegmenter` (Layout.md "What both candidates implement", Text.md "Text stack selection") isolate the selection candidates; they are `Internal/` and not part of the API.

### Areas that consume UI

| Area | Uses |
|---|---|
| EDIT (pending) | `Zenith_UIEditorModel`, a preview `Zenith_UIContext`, the widget and property catalogues, the document validators (row UI15) |
| GAME | screens on entities, events, requests; UI clears the menu pause reason (GA13) |
| AUDIO | `ui.locale` read by name; captions through the game script calling `UI.ShowCaption` (AU16); the extraction sink for dialogue keys |
| CINE (pending) | caption cue events (row UI4), the extraction sink |
| TWOD | nothing at the API level (row UI16) |
| DEVT | UI's `ui` frame-dump section and its console-settable debug keys; DEVT's overlay is drawn by RPROD's epilogue after `FLUX_EXTENSION_POINT_OVERLAY`, which follows UIComposite, so it is not an extension-point pass (row UI16, corrected by DEVT's DV21) |
| games | UI documents, themes, fonts, localization projects, the `UI` Luau namespace |

## Library and file layout

### Target summary

| Target | Kind | Path | `ZENITH_SUPPORTED` |
|---|---|---|---|
| `ZenithUI` | `KIND MODULE`, `MODULE_NAME UI` | `Zenith2/Engine/ZenithUI/` | `ZENITH_GPU` (`win`, `win-asan`, `android-arm64`, `android-x86_64`; never `linux-tsan`, `Zenith2/Docs/Architecture/BuildSystem.md`) |
| ZenithFlux UI files | part of `ZenithFlux` | `Zenith2/Engine/ZenithFlux/Flux_UI.h`, `Internal/UI/` | as ZenithFlux |
| ZenithCook UI files | part of ZenithCook | `Zenith2/Tools/ZenithCook/Internal/UI/` | as ZenithCook (`ZENITH_TOOLS_HOST`) |
| ZenithAuthoring UI files | part of the tool library `ZenithAuthoring` | `Zenith2/Tools/Authoring/Zenith_AuthoringUI.h`, `Zenith2/Tools/Authoring/Internal/UI/` | as ZenithAuthoring (`ZENITH_TOOLS_HOST`) |
| `ZenithUI_Tests` | tests | `Zenith2/Engine/ZenithUI/Tests/` | as ZenithUI |
| `Zenith_Integration_UI` | integration tests | `Zenith2/Tests/UI/`, `Zenith2/Tests/UIAccessibility/` | as ZenithUI |
| `Zenith_Bench_UI` | benchmark harness (`zenith_add_benchmark`, with its `--smoke` CTest) | `Zenith2/Benchmarks/UI/` | `ZENITH_GPU` (it links ZenithUI); full runs on the Release configuration of `win` and `android-arm64` |

### ZenithUI files

| Path (under `Zenith2/Engine/ZenithUI/`) | Holds |
|---|---|
| the 17 root headers of PublicAPI.md "Header list" | the public API |
| `Internal/Zenith_UIModule.h`, `.cpp` | `Zenith_UIModule` (the module instance), start and attach steps |
| `Internal/Zenith_UISystem.cpp`, `Internal/Zenith_UIConfig.cpp` | the system, configuration keys |
| `Internal/Zenith_UIContext.cpp`, `Internal/Zenith_UIElementTable.h`, `.cpp` | the context and the SoA element table |
| `Internal/Zenith_UIStyleResolver.h`, `.cpp` | the shell's style resolver: the cascade of every catalogue property into `Zenith_UIComputedStyle`, for both D-SEL-UIRT candidates (Layout.md "What both candidates implement") |
| `Internal/Layout/Zenith_UILayout.h`, `.cpp`, `Zenith_UILayoutTypes.h`, `Zenith_UIMirror.cpp`, `Zenith_UIVirtualList.h`, `.cpp` | the shell's layout entry, the internal layout types, the RTL reflection pass, virtualization |
| `Internal/Runtime/Zenith_UIRuntimeBackend.h`, `Internal/Runtime/Custom/*.cpp`, `Internal/Runtime/RmlUi/*.cpp` (with `Zenith_UIRmlUiSection.cpp`, the RMLUI section writer) | the D-SEL-UIRT candidates, both written by slice 7 (the losing candidate is deleted by the ruling ticket) |
| `Internal/Zenith_UIBindings.cpp`, `Internal/Zenith_UIAnimation.cpp` | binding evaluation, animation |
| `Internal/Text/Zenith_UITextBackends.h`, `Internal/Text/Backends/*.cpp` (the D-SEL-TEXT candidates: bidi and segmentation written by slice 3, outlines by slice 5), `Internal/Text/Zenith_UIParagraph.cpp`, `Zenith_UILineBreak.cpp`, `Zenith_UICaret.cpp`, `Zenith_UITextEdit.cpp`, `Zenith_UITextCache.cpp`, `Zenith_UITextMeasurer.cpp`, `Zenith_UIUtf8.cpp` | the text stack, the layout's text measurer, UTF-8 repair |
| `Internal/Text/Zenith_FontRegistry.h`, `.cpp`, `Zenith_GlyphCache.cpp`, `Zenith_UIAtlasPacker.cpp`, `Zenith_UIMSDF.cpp` | fonts and atlases |
| `Internal/Generate/Zenith_UIUnicodeTables.cmake`; its output `${ZENITH_OUT}/generated/ZenithUI/Zenith_UIUnicodeTables.generated.inc` (a `GENERATES` path, outside the source tree, never committed) | Unicode tables (Text.md "Unicode data") |
| `Internal/Localization/Zenith_UILocalization.cpp`, `Zenith_UIMessageInterpreter.cpp`, `Zenith_UIPlural.cpp`, `Zenith_UINumberFormat.cpp`, `Zenith_UIPseudo.cpp` | localization |
| `Internal/Input/Zenith_UIInputClaimant.cpp`, `Zenith_UIFocus.cpp`, `Zenith_UINavigation.cpp`, `Zenith_UIHitTest.cpp`, `Zenith_UIControls.cpp`, `Zenith_UIOnScreenControls.cpp` | input |
| `Internal/Zenith_UIPublication.cpp`, `Internal/Zenith_UIEventsSystem.cpp`, `Internal/Zenith_UICommandKinds.cpp`, `Internal/Zenith_UICaptionsSystem.cpp` | the simulation crossings |
| `Internal/Zenith_UIExtractor.cpp`, `Internal/Zenith_UIDrawList.cpp`, `Internal/Zenith_UIWorldExtension.cpp` | extraction, draw lists, the view extension |
| `Internal/Accessibility/Zenith_UIA11yTree.cpp`, `Zenith_UIA11yBridge.cpp`, `Zenith_UINarration.cpp`, `Zenith_UIColourVision.cpp`, `Zenith_UIContrast.cpp` | accessibility, the provider PLAT's service calls |
| `Internal/Zenith_UIDocumentLoader.cpp`, `Internal/Zenith_UIFontLoader.cpp`, `Internal/Zenith_UILocalizationLoader.cpp` | the three kind loaders |
| `Internal/Zenith_UIScriptApi.cpp`, `Internal/Zenith_UIEditorModel.cpp`, `Internal/Zenith_UIFrameDumpSection.cpp` | the Luau table, the editor model, the `ui` frame-dump section |
| `Internal/Content/` | engine content, each source with its committed `.meta`: under `UI/`, `Default.uitheme.json`, `AccessibilitySettings.ui.json`, `Captions.ui.json` and `Fonts/` (the default family and the Noto fallback family documents); `Localization/localization.json` and the `engine` tables |
| `Tests/**` | `ZenithUI_Tests` (Tests.md) |

No `Internal/Windows|Android|Linux` files: UI is platform-neutral; platform accessibility code is PLAT's (`Zenith2/Docs/Design/PLAT/Accessibility.md → "Ownership split with UI"`).

### ZenithFlux UI files

`Zenith2/Engine/ZenithFlux/Flux_UI.h`; `Internal/UI/Flux_UIFrame.h`, `Flux_UIRenderer.cpp`, `Flux_UIComposite.cpp`, `Flux_UIWorldSpace.cpp`; the body of `Flux_Renderer::EnableUI` in RPROD's renderer source (RPROD's "add your own Create step" pattern); tests `Zenith2/Engine/ZenithFlux/Tests/UI/**`. They include no ZenithUI header (ZenithFlux sees no module; row UI7).

### ZenithCook UI files

`Zenith2/Tools/ZenithCook/Internal/UI/`: `Zenith_UICookSteps.h`, `.cpp`; `Zenith_UIImporters.h`, `.cpp` (the `ui.fontfamily` importer); `Zenith_UIExtraction.h`, `.cpp`; `Zenith_UIXliff.h`, `.cpp`; `Zenith_UIManifest.cpp`; tests `Zenith2/Tools/ZenithCook/Tests/UI/**`, among them the SA break record case `Zenith_UISaBreakRecord_Tests.cpp` (`Zenith2/Docs/Design/UI/Text.md → "Corpus file format"`), the one UI test that links the usage-tests manifest entry `icu4c`. The cook steps call the document-type registration of `ZenithAuthoring` (below), which ZenithCook declares in its own `DEPENDS`.

### ZenithAuthoring UI files

By the orchestrator's ruling on the tool library (`Zenith2/Docs/Design/EDIT.md → "Tool-side code the editor needs"`, cross-area row ED43; the core rows are X257), the UI code both ZenithCook and ZenithEditor call lives in the static tool library `ZenithAuthoring`, in UI's own subdirectory:

<!-- assumption: pending ARCH (X257) -->
| Path | Holds |
|---|---|
| `Zenith2/Tools/Authoring/Zenith_AuthoringUI.h` | UI's root header, included as `#include "Authoring/Zenith_AuthoringUI.h"`: `Zenith_RegisterUIDocumentTypes`, `Zenith_GetUICanonicalSchema`, `Zenith_UIDocumentType`, `uUI_DOCUMENT_SCHEMA_VERSION` (`Zenith2/Docs/Design/UI/ServicesAPI.md → "Tool-side headers"`) |
| `Zenith2/Tools/Authoring/Internal/UI/Zenith_UIDocumentTypes.cpp` | the five document types, their canonical schemas and migration chains (moved from ZenithCook's `Internal/UI/`) |
| `Zenith2/Tools/Authoring/Tests/UI/Zenith_UIMigration_Tests.cpp` | `Test_UIDocumentMigration_Fixtures` (`ZenithAuthoring_Tests`) |
| `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/UI/<Type>/` | `v1.json` and `v1.expected.json` per document type |
<!-- /assumption -->

The move adds no `DEPENDS` entry: `ZenithAuthoring` already declares `ZenithDocument` and `ZenithUI` (EDIT's call). The editor's UI authoring tool reaches the canonical schema through this header, never through ZenithCook's `Internal/`.

### Shader modules and engine content

`Zenith2/Shaders/Passes/UI/UIDraw.slang`, `Zenith2/Shaders/Passes/UI/UIWorldSpace.slang`, `Zenith2/Shaders/Common/UIShapes.slang` (Rendering.md "Shader programs"); engine content as above: ASSET maps `Internal/Content/<p>` of a linked library to `content:/Engine/<p>` (`Zenith2/Docs/Design/ASSET.md` → "Content root mapping"), so the `UI/` subdirectory is `content:/Engine/UI/`.

## Public API

### Header map

The headers, their declarations and stages are PublicAPI.md "Header list"; the full declarations are in `Zenith2/Docs/Design/UI/PublicAPI.md` and `Zenith2/Docs/Design/UI/ServicesAPI.md`.

### Rules across the UI API

- Every public type carries one contract block; every fallible function returns `Zenith_Result<T, E>`; no third-party header and no STL header reaches a public header.
- `Zenith_UISystem` and `Zenith_UIContext` are concrete classes created by `Create` factories returning `Zenith_Result<Zenith_Owned<…>>`; the runtime and text candidates are swapped behind internal interfaces, never through the public API.
- Untrusted input (documents, cooked files, payloads, script arguments, platform text) never asserts: it returns an error or is counted and dropped.
- No public type stores a pointer into another world; screens are addressed by `Zenith_UIScreenHandle` (context tag, index, generation) in presentation and by entity in simulation.

### Configuration keys

Registered by `Zenith_RegisterUIConfigKeys` from `Zenith_Module_UI_RegisterTypes` (a module registers its keys there, before any layer is read); every key is presentation, none is simulation-affecting:

| Key | Type, range, default | Field of `Zenith_UIConfig` |
|---|---|---|
| `ui.scale` | f32 0.75–2.0, 1.0 | `m_fUIScale` |
| `ui.locale` | string (BCP 47 subset), empty | `m_strLocale` |
| `ui.loc.pseudo` | u8 0–3, 0 | `m_uPseudoLocalization` |
| `ui.loc.pseudoLongFactor` | f32 1.0–3.0, 1.4 | `m_fPseudoLongFactor` |
| `ui.access.*` (`textScale` 1.0–2.0, …), `ui.captions.*` | Accessibility.md "Accessibility settings" | `m_xAccessibility` |
| `ui.text.sdfSharpness` | f32 0.5–2.0, 1.0 | `m_fSDFSharpness` |
| `ui.text.dynamicGlyphsPerFrame` | u32 1–256, 16 | `m_uDynamicGlyphsPerFrame` |
| `ui.text.layoutCacheEntries` | u32 64–65,536, 2,048 | `m_uLayoutCacheEntries` |
| `ui.atlas.maxDynamicPages` | u32 1–64, 8 | `m_uMaxDynamicPages` |
| `ui.atlas.pageUploadsPerFrame` | u32 1–16, 2 | `m_uPageUploadsPerFrame` |
| `ui.headless.canvasWidth`, `ui.headless.canvasHeight` | u32 64–16,384, 1,920 × 1,080 | `m_uHeadlessCanvasWidth`, `m_uHeadlessCanvasHeight` |
| `ui.events.maxPerFrame` | u32 2–1,024, 64 | `m_uMaxEventsPerFrame` (publications per world per frame; one of them is reserved for a resume record, so ordinary events get `maxPerFrame − 1`, Interaction.md "Publication path") |
| `ui.events.maxInFlight` | u32 16–4,096, 256 | `m_uMaxEventsInFlight` (outstanding publications per world, one reserved for a resume record, Interaction.md "Publication path") |
| `ui.screens.maxPerWorld` | u32 1–1,024, 64 | `m_uMaxScreensPerWorld` |
| `ui.elements.maxPerContext` | u32 1,024–1,048,576, 65,536 | `m_uMaxElementsPerContext` |
| `ui.input.dragThresholdDp` | f32 1–64, 8 | `m_fDragThresholdDp` |
| `ui.input.repeatDelayMs`, `ui.input.repeatIntervalMs` | u32 50–2,000, 400; 20–1,000, 80 | `m_uNavRepeatDelayMs`, `m_uNavRepeatIntervalMs` |
| `ui.scroll.wheelStepDp` | f32 1–1,000, 48 | `m_fWheelStepDp` |
| `ui.world.luminanceNits` | f32 1–10,000, 100 | `m_fWorldSpaceLuminanceNits` |
| `ui.world.minGlyphPixels` | f32 0–64, 6 | `m_fWorldMinGlyphPixels` |
| `ui.debug.bounds`, `ui.debug.focus`, `ui.debug.batches`, `ui.debug.atlas`, `ui.debug.a11y`, `ui.debug.pseudo` | bool, false; settable from the command-line layer, so DEVT's console `set` reaches them | `m_uDebugViewMask` ("Debug views"): bits 0 to 5 in the order of this cell, every other bit 0 |

A changed key applies at the next frame prologue (`ApplyConfig`); limits that size pools (`maxPerWorld`, `maxPerContext`, `layoutCacheEntries`, `maxDynamicPages`) apply to contexts created after the change.

### Handle kinds used

| Handle | Kind | Serialized |
|---|---|---|
| `Zenith_UIScreenHandle` | generational, presentation-only | never: simulation names screens by entity |
| `Zenith_UIElementRef` | screen handle + element index + generation | never |
| `Zenith_EntityHandle` in request payloads | ECS generational | as ECS specifies (stable ID in saves and replays) |
| asset handles of fonts, documents, tables | ASSET counted handles | as ASSET specifies |
| `Flux_UIAtlasPage` | persistent read-only texture | never |

## Data structures and formats

### Format index

| Format | Where |
|---|---|
| UI, theme, font family, localization project, string table JSON | Documents.md (schemas, rules, migration) |
| `.zui` (`ZUID`, kind 13) | Documents.md "Cooked UI document format" |
| `.zfont` (`ZFNT`, kind 12) and MSDF page payloads (`ZMSD`) | Documents.md "Cooked font format" |
| `.zloc` (`ZLOC`, kind 20) | Documents.md "Cooked localization table format" |
| `UI/BindingManifest.zbin` (`ZUIM`) | Documents.md "Binding manifest file" |
| the UI publication payload | Interaction.md "Event kinds and payloads" |
| `Flux_UIQuad` | Rendering.md "Flux_UIQuad layout" |
| the extraction report, XLIFF 2.0 | Localization.md "Extraction report and exchange files" |
| accessible-flow routes | Accessibility.md "End-to-end accessible flows" |

All binary formats are little-endian; cooked files use ASSET's 64-byte header and 40-byte section table; every count and offset is validated against the section bounds before use.

### Asset kind loaders

| Kind | Code | Type | Ext | Magic | GPU-resident | Fallback | Loader |
|---|---|---|---|---|---|---|---|
| `UI.Font` | 12 | `Zenith_FontAsset` | `.zfont` | `ZFNT` | no (pages are uploaded by the glyph cache) | SUBSTITUTE (the engine default family) | `Zenith_UIFontLoader`: validates sections, keeps face programs and page payloads CPU-resident, registers faces with the font registry |
| `UI.Document` | 13 | `Zenith_UIDocumentAsset` | `.zui` | `ZUID` | no | FAIL (the screen stays unopened, counted) | `Zenith_UIDocumentLoader`: validates every record index, resolves dependent fonts, textures and tables as counted dependencies (the theme is folded into the `.zui` at cook time and is never a dependency) |
| `UI.LocalizationTable` | 20 | `Zenith_LocalizationTableAsset` | `.zloc` | `ZLOC` | no | FAIL (the previous locale stays) | `Zenith_UILocalizationLoader`: validates sections and the key index ordering |

Registered at module start step 2 with `Zenith_AssetRegistry`'s kind-loader registration; hot reload of a document re-opens every screen using it at the next extraction with focus kept by element uid. The `ui.font` cook step declares the engine default family (`content:/Engine/UI/Fonts/Default.fontfamily.json`, Noto Sans) as the role-0 engine fallback of `UI.Font` (`Zenith2/Docs/Design/ASSET/Cook.md → "Engine fallbacks"`, ASSET's row AS26).

### World resources

<!-- elaboration: two world resources; decoded publications are module state -->
| Resource | Type | Owning phase | Written by | Read by |
|---|---|---|---|---|
| `Zenith.UI.Requests` | bounded queue of 256 `Zenith_UIRequestKind`-tagged requests, `QUEUE_FULL_POLICY_DROP_NEWEST` | `PHASE_EVENTS` (9), the type's registered `m_eResourceOwningPhase`; no system of phase 9 writes it | command playback at the sync points after phases 2–10; the extractor's drain | the extractor (sole consumer, drains each frame) |
| `Zenith.UI.Captions` | `uUI_CAPTION_QUEUE_CAPACITY` cues | `PHASE_EVENTS` (9), the type's registered `m_eResourceOwningPhase` | `UI.Captions@W<tag>` (phase 9, its declared `WriteResource`), `UI.ShowCaption` and `UI.ClearCaptions` playback, expiry by `UI.Captions@W<tag>` | the extractor |

Each type registers exactly one owning phase ([Z2-N-ARCH-731]), and attach step 1 passes that phase to `AddWorldResource`. The writes outside a phase-9 system are admitted on two other grounds, never as a second owning phase: command playback (steps 5 to 7 of the sync points) mutates storage only through `Zenith_WorldPlaybackAccess` ([Z2-N-ARCH-716]), and a module command's playback finds the world's resource through `GetWorldTag`, as a playback write rather than a system's declared access, so the authority check of `Zenith2/Docs/Design/ECS/Scheduling.md → "Check order"` step 5, which reads systems' declared writes, never sees it; and the extractor's drain of `Zenith.UI.Requests` is the sole-consumer epilogue drain of [Z2-N-ARCH-727], the exception that step 5 names.

Neither is saved (captions and requests are transient); both replay identically because their writers are recorded commands and deterministic systems. Decoded UI publications are not a world resource: the publication handler writes them into the module's per-world buffer `Zenith_UIWorldRecord::m_xPublished`, which only `UI.Events@W<tag>` reads and empties in the same tick (Interaction.md "Publication path"), so the handler writes no world state and needs no tick-boundary access. There is no further per-world UI resource: `UI.Events` finds a screen by scanning its query's rows, so no stable-ID map is kept.

### In-memory sizes that matter

| Structure | Size |
|---|---|
| `Flux_UIQuad` | 64 bytes |
| cooked element record | 64 bytes; binding record 48 bytes; face record 64 bytes; glyph record 32 bytes |
| `Zenith_UIError` | 16 bytes |
| UI publication payload | 40 + names + text, at most 448 bytes |
| the per-world publication buffer | `ui.events.maxInFlight` decoded records of 472 bytes (256 at the default: 118 KiB per world) |
| element table per element | about 180 bytes of SoA columns plus computed style (Layout.md "Element tree model") |
| an MSDF page | 1,024 × 1,024 × 4 = 4 MiB device memory |

## Algorithms and state machines

### Algorithm index

| Algorithm | Where |
|---|---|
| style cascade | Layout.md "Style resolution" |
| layout passes, stack, grid, anchored, text leaves, scroll, snapping | Layout.md "Layout algorithm" |
| incremental layout | Layout.md "Invalidation and incremental layout" |
| virtualization (fixed and Fenwick-tree variable extents, recycling) | Layout.md "Large-list virtualization" |
| paragraph pipeline (decode, bidi, itemize, fallback, shape, break, reorder, align, truncate) | Text.md "Paragraph pipeline" |
| skyline packing, MSDF generation, page upload | Text.md "Glyph cache and atlases" |
| hit testing, directional navigation, tab order | Interaction.md |
| publication path | Interaction.md "Publication path" |
| draw list construction and batching | Rendering.md "Draw list construction" |
| locale negotiation, message interpretation, plural selection, number formatting | Localization.md |
| narration composition, colour-vision correction, contrast | Accessibility.md |

### State machines

Pointer (Interaction.md "Pointer state machine"), focus (Interaction.md "Focus state machine"), dynamic glyph (Text.md "Dynamic glyph state machine"), text editing (Text.md "Text editing"), locale switch (Localization.md "Locale switching at run time"), module lifecycle (below).

### Module start sequence

`Zenith_Module_UI_Start(xContext)` performs, each step with a failure-injection value and reverse unwinding on failure ([Z2-N-CONTRACT-340]):

1. Read `Zenith_UIConfig` from `xContext.GetConfig()` (`Zenith_ReadUIConfig`); out-of-range values were already clamped by the schema.
2. Register the three asset kind loaders with `xContext.GetAssets()`.
3. Request the engine content: the engine localization project's tables for the negotiated locale chain and the engine fallback font families (asynchronous; the module is usable before they load, text draws when fonts are resident). The engine default theme is not requested: the `ui.document` step folds it into every `.zui` whose document names no theme (Documents.md "Authoring document family").
4. Register the `UI` Luau table (`Zenith_RegisterUIScriptBindings` into a `Zenith_ScriptBindingTable`, then `RegisterScriptBindings`).
5. Declare the asynchronous source with `DeclareScriptAsyncSource(ASYNC_SOURCE_UI, uUI_PUBLICATION_MAX_BYTES)`; the enumerator's value is the registry's (`Zenith2/Docs/Scripting.md → "Asynchronous source kind registry"`, [Z2-N-SCRIPT-925]) and UI writes none of its own.
6. When `GetRenderer()` is non-null: `Flux_Renderer::EnableUI(xDesc)` (RPROD's declaration, UI's body; Rendering.md "Flux UI renderer") and `RegisterViewExtension` of UI's world-space extension; headless, nothing (Rendering.md "Headless and device loss").
7. Create `Zenith_UISystem` with the heap, string table, asset registry, renderer (null headless) and platform locale; register the claimant with `Zenith_InputSystem::RegisterClaimant` (row UI6, pending PLAT); create the virtual device `ui` when the binding manifest lists on-screen controls; register the `ui` frame-dump section with DEVT's frame dump when one exists (row UI23, pending DEVT and MOD).
8. Register the six command kinds (Interaction.md "Command kinds").
9. Register the memory consumer (`RegisterMemoryConsumer`; host bytes and device bytes of `Zenith_UISystem`).
10. When `GetPlatform()->GetAccessibility()` is non-null: `SetTreeProvider` with UI's `Zenith_AccessibilityTreeProvider` (Stage 4; Accessibility.md "Platform accessibility bridge"; a refusal fails the step).
11. Construct `Zenith_UIModule` owning everything above and return it.

### World attach and detach

`Zenith_UIModule::AttachWorld(xContext)`:
1. add the two world resources (`Zenith.UI.Requests`, `Zenith.UI.Captions`) and create the module's per-world record (`Zenith_UIWorldRecord`, holding the publication buffer);
2. register `UI.Events@W<tag>` (phase 1, reads `Zenith.UI.Screen`, emits the six event types, records commands; reads the per-world publication buffer, module state, Interaction.md "Publication path");
3. register `UI.Captions@W<tag>` (a C++ event handler in `PHASE_EVENTS` (9), Stage 4; receives the caption cue events of row UI4 in their phase-9 delivery ([Z2-N-ARCH-478]), writes `Zenith.UI.Captions`);
4. add the extractor `UI.Extract@W<tag>` (phase 12, Before `Render.Seal@W0`, reads `Zenith.UI.Screen` and the binding manifest's declared properties, sole consumer of `Zenith.UI.Requests`, reads `Zenith.UI.Captions`);
5. create the world's `Zenith_UIContext` (context tag = world tag) with the surface of the main window (or the headless canvas);
6. declare the UI source on the world's module publication sink (`GetPublicationSink()`, then `DeclareSource(ASYNC_SOURCE_UI, …)` with UI's publication handler; row UI1, pending MOD and SCRIPT, Interaction.md "Pause menus"), and keep the world's `Zenith_WorldControl` pointer from `GetWorldControl()` for `resumeWorld` actions (row UI19, GAME's row GA5 (a), pending MOD).

`DetachWorld(uWorldTag)` reverses them: the sink discards the source's outstanding results, the context and its screens are destroyed, the per-world record is freed, the registrations are undone by the runtime's journal.

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| `Zenith_UISystem` | `ANY_THREAD_EXCEPT(SetLocale, ApplyConfig, BeginFrame, EndFrame, OnPressureLevelChanged)`: those on the main thread in the prologue; text, glyph and localization lookups from any extractor concurrently (internal reader locks) |
| `Zenith_UIContext` | `OWNER_THREAD`, owned in turn by the main thread (Platform.Pump: input feed, `SwapAccessibilityBuffers`, `DrainNarration`) and the world's extractor node (Update, BuildDrawList, BuildA11yTree, `StageAccessibilityPublication`); never concurrently ([Z2-N-ARCH-384] places extraction after the frame's last tick, and Pump precedes the graph); PLAT's service calls the accessibility provider's `GetTree()` only on the main thread, inside `NotifyTreeChanged` from UI's own Pump callback, and copies the front buffer into its snapshot before returning, so no other thread reads UI memory (`Zenith2/Docs/Design/PLAT/Accessibility.md → "The published snapshot"`) |
| `Zenith_TextLayoutEngine`, `Zenith_GlyphCache`, `Zenith_Localization` | as the system: concurrent readers, main-thread writers |
| `Zenith_UIEditorModel` | `OWNER_THREAD` (the editor's main thread) |
| value types, events, payloads | `IMMUTABLE` |
| `Flux_UIRenderer` | page functions `ANY_THREAD`; `PrepareFrame`, `ImportQuadChunk`, `WriteFrame`, `GetFrame` on `Flux.BuildGraph` |
| the claimant | main thread, inside `Platform.Pump` |

### Frame-graph placement

| Node | Subgraph, phase | Declared access |
|---|---|---|
| `Platform.Pump` (PLAT's node, UI's claimant callbacks) | PROLOGUE | the input world's context, the UI system's frame state |
| tick boundary of each world, step 1 | SIMULATION | UI's publication handler (through the sink's forwarder): appends to the module's per-world buffer; writes no world state |
| `UI.Events@W<tag>` | SIMULATION, phase 1 | read `Zenith.UI.Screen`; read and empty the per-world publication buffer (module state of that world, ordered after the tick boundary by [Z2-N-ARCH-383]); emit the six `Zenith.UI.*` events; commands |
| `UI.Captions@W<tag>` | SIMULATION, `PHASE_EVENTS` (9) | a C++ event handler of the caption cue events; write `Zenith.UI.Captions` |
| `UI.Extract@W<tag>` | EPILOGUE, phase 12, Before `Render.Seal@W0` | read `Zenith.UI.Screen`, the manifest's properties, `Zenith.UI.Captions`; sole consumer of `Zenith.UI.Requests`; the world's render-world writer partition |
| `Flux.BuildGraph` (RPROD's node) | render | `Flux_UIRenderer::PrepareFrame` (step 4), `UIComposite` and the extension's `UIWorldSpace` (steps 8–9), `WriteFrame` (step 11) |

### Lock-order entries

`Zenith_UISystem` takes one reader-writer lock per service (glyph cache, text cache partitions are per context and unlocked, localization tables); they are leaves in the global lock-ordering table (taken last, never held across a call out of ZenithUI), each constructed with its level from `Zenith2/Engine/ZenithUI/Internal/Zenith_UILockOrder.h` (`constexpr uint16` `uLOCK_LEVEL_UI_FONT_REGISTRY` 800, `uLOCK_LEVEL_UI_GLYPH_CACHE` 810, `uLOCK_LEVEL_UI_LOCALIZATION` 820, the rows of `Zenith2/Docs/Contracts.md` → "Lock ordering table", [Z2-N-CONTRACT-318]), which the lock-order checker enforces. Page creation calls `Flux_PersistentResources` (lock 480) without holding a UI lock. Concurrency evidence: ZenithUI is `ZENITH_GPU`, so it is never built in `linux-tsan` and no race detector covers it; the lock-order checker and `Test_UIContext_ConcurrentWorldsExtract` (two worlds' extractors on 8 workers, results equal to a serial run) are the evidence, and `win-asan` covers memory errors only.

### Construction and shutdown

Construction is the module start and attach sequences above. Shutdown: detach every world (reverse attach), then the module instance's destructor unregisters the claimant, the accessibility tree provider (`SetTreeProvider(nullptr)`) and the view extension, destroys the virtual device (synthesising releases), releases engine content handles, destroys `Zenith_UISystem` (pages released through the renderer), and the runtime's journal undoes the registrations.

### Cancellation

UI work is not cancellable mid-frame and needs no cancellation: every step is bounded per frame (glyph generation and uploads have per-frame limits, layout is incremental). Pending asset loads are cancelled by releasing their handles; a text input session is ended (`Zenith_TextInput::End`) when its field or screen goes away; a rebind in progress is cancelled with `CancelRebind` when its element is hidden.

### Error enumerations

| Enumeration | Header | Codes |
|---|---|---|
| `Zenith_UIErrorCode` | `Zenith_UITypes.h` | `UI_ERROR_INVALID_DESC` (1) a descriptor out of range; `OUT_OF_MEMORY`; `STALE_HANDLE`; `SCREEN_LIMIT`; `ELEMENT_LIMIT`; `DOCUMENT_NOT_READY` (the asset is still loading); `DOCUMENT_CORRUPT`; `DOCUMENT_VERSION` (a cooked version this build does not read); `UNKNOWN_ELEMENT`; `UNKNOWN_BINDING`; `BINDING_TYPE`; `NOT_FOCUSABLE`; `FONT_MISSING`; `LOCALE_UNSUPPORTED`; `RENDERER_UNAVAILABLE`; `ATLAS_FULL`; `PUBLICATION_FULL`; `AUTHORING_INVALID`; `EDIT_REJECTED`; `RUNTIME_BACKEND` (20) the runtime candidate failed |
| `Zenith_TextLayoutError` | `Zenith_UIText.h` | `TEXT_LAYOUT_ERROR_BAD_UTF8`, `BAD_SPAN`, `NO_FONT`, `TOO_LONG`, `OUT_OF_MEMORY`, `SHAPER` |
| `Zenith_FontError` | `Zenith_UIFont.h` | `FONT_ERROR_CORRUPT`, `UNSUPPORTED_PROGRAM`, `TOO_MANY_GLYPHS`, `PAGE_LIMIT`, `BAD_FACE`, `OUT_OF_MEMORY` |
| `Zenith_LocalizationErrorCode` | `Zenith_UILocalization.h` | the thirteen codes of ServicesAPI.md "Zenith_UILocalization header" |
| module errors | `Zenith_ModuleError` (MOD) | `MODULE_ERROR_REGISTRATION`, `MODULE_ERROR_OUT_OF_MEMORY` |

### Luau errors and asserts

<!-- elaboration: UI's misuse block and its catalogue rows -->
Script misuse raises a Luau error, never an assert. UI's catalogued codes are the block ZS2100–ZS2199, which `Zenith2/Docs/Scripting/MisuseCatalogue.md → "Code ranges"` already reserves for UI ([Z2-N-SCRIPT-926]; ANIM holds ZS2000–ZS2099 and GAME ZS4100–ZS4199); its row names the codes in use, ZS2101 to ZS2106, and points at this heading, which holds the one meaning of each:

| Code | Kind | Trigger | Text |
|---|---|---|---|
| ZS2101 | MISUSE | a key argument of `UI.Loc`, `UI.Announce` or `UI.ShowCaption` (key or speaker key) outside the key grammar of Localization.md "String tables and keys" | `UI.<Function> expects a localization key 'namespace.name', got '<Value>'` |
| ZS2102 | MISUSE | a caption kind other than `"subtitle"` or `"sound"` (`UI.ShowCaption`), or other than those and `"all"` (`UI.ClearCaptions`) | `UI.<Function> kind must be "subtitle", "sound"<OrAll>, got '<Value>'` |
| ZS2103 | MISUSE | an announcement priority other than `"polite"` or `"assertive"` | `UI.Announce priority must be "polite" or "assertive", got '<Value>'` |
| ZS2104 | MISUSE | a caption duration that is not finite or lies outside (0, 600] seconds | `UI.ShowCaption duration must be a finite number of seconds in (0, 600], got <Value>` |
| ZS2105 | MISUSE | a caption priority that is not an integer from 0 to 255 | `UI.ShowCaption priority must be an integer from 0 to 255, got <Value>` |
| ZS2106 | MISUSE | a `UI.ScrollTo` item index that is not an integer from −1 to 1,048,575 | `UI.ScrollTo item must be an integer from -1 to 1048575, got <Value>` |

`<Function>` is the Luau function name, `<OrAll>` is `, "all"` for `UI.ClearCaptions` and empty otherwise, and `<Value>` the argument as Luau's `tostring` prints it. `nil` where an entity is required is SCRIPT's ZS1104, an argument of the wrong Luau type is the binding layer's own error, and a call from a phase where the command kind is not allowed is SCRIPT's; UI raises no code outside its block. `Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIScriptApi header"` cites this table, and `Test_UIScriptApi_MisuseCodes` raises each code with its exact text. Unknown element names, destroyed screens, non-screen entities and unknown animations are not errors at the call: they are counted when the request is applied. Asserts guard only programmer errors inside ZenithUI (a context used from two threads, a broken internal invariant).

## Diagnostics and profiling

### Counters

Every counter UI defines, by group (registered with the profiler at module start; `flux.ui.quads_dropped` by ZenithFlux's UI files):

| Group | Counters |
|---|---|
| events and replay | `ui.events.dropped`, `ui.events.resume_refused`, `ui.events.stale_screen`, `ui.events.text_truncated`, `ui.events.text_rejected`, `ui.events.text_intern_failed`, `ui.events.decode_failed`, `ui.events.write_back_failed`, `ui.replay.live_events_ignored`, `ui.replay.live_resume_ignored` |
| requests and captions | `ui.requests.dropped`, `ui.requests.stale`, `ui.requests.unknown_element`, `ui.requests.unknown_animation`, `ui.captions.evicted` |
| screens, layout, bindings, styles | `ui.screens.open`, `ui.screen.document_unavailable`, `ui.layout.elements_laid_out`, `ui.list.items_clamped`, `ui.binding.conversion_failed`, `ui.style.token_unresolved` |
| draw | `ui.draw.quads`, `ui.draw.batches`, `ui.draw.quads_truncated`, `ui.draw.slot_full`, `flux.ui.quads_dropped` |
| text and glyphs | `ui.text.missing_glyphs`, `ui.text.shaped_paragraphs`, `ui.text.cache_hits`, `ui.text.utf8_repaired`, `ui.glyphs.generated`, `ui.glyphs.generation_failed`, `ui.glyphs.requests_dropped`, `ui.atlas.dynamic_full`, `ui.atlas.upload_failed` |
| localization | `ui.loc.missing`, `ui.loc.missing_arg`, `ui.loc.arg_kind_mismatch`, `ui.loc.locale_load_failed` |
| input | `ui.text_field.begin_refused` |
| accessibility | `ui.a11y.nodes`, `ui.a11y.nodes_capped`, `ui.a11y.publish_deferred`, `ui.a11y.publish_rejected`, `ui.a11y.speech_failed`, `ui.a11y.utterances`, `ui.a11y.narration_dropped` |

Cook diagnostics are not counters: `ui.doc.layout_animation` (rule 23's warning) and `ui.extract.dynamic_key` are reported in the cook report.

### Log categories

`ui` (module lifecycle), `ui.runtime` (the runtime candidate), `ui.text`, `ui.font`, `ui.loc`, `ui.input`, `ui.a11y`, `ui.cook` (tool side). Each category logs a given (key, cause) once per session to avoid floods.

### Profiler zones

`UI.Claimant.Pointers`, `UI.Claimant.Actions`, `UI.System.BeginFrame`, `UI.System.EndFrame`, `UI.Events`, `UI.Captions`, `UI.Extract` with children `UI.Extract.Sync`, `UI.Update.Styles`, `UI.Update.Layout`, `UI.Update.Text`, `UI.Extract.DrawList`, `UI.Extract.A11y`; GPU zones `UIComposite`, `UIWorldSpace`; `UI.Glyphs.Generate`, `UI.Atlas.Upload`.

### Debug views

`ui.debug.bounds` (outline every element's border box, colour by type), `ui.debug.focus` (draw the navigation candidates and scores of the last move), `ui.debug.batches` (tint each batch), `ui.debug.atlas` (show atlas pages), `ui.debug.a11y` (draw semantic node bounds and names), `ui.debug.pseudo` (the pseudo modes). They are configuration keys declared settable from `CONFIG_LAYER_COMMAND_LINE`, so DEVT's console `set` toggles them for the session (`Zenith2/Docs/Design/DEVT/Console.md → "Configuration variables"`); the extractor reads them and they draw through the same draw list.

### On-screen profiler rows

<!-- elaboration: UI's overlay tab (orchestrator rulings DEVT-AREA-REGISTRATION, DEVT-AREA-REGISTRATION-2) -->
The UI module registers the tab `ui` with `Zenith_ProfilerOverlay::AddRowPanel` through `Zenith_ModuleStartContext::GetProfilerOverlay()` at start step 7 (null in Shipping and headless: nothing registered), as `Zenith2/Docs/Design/DEVT/Overlay.md` asks of every area whose features land in Stages 3 to 5. Its rows, all of exactness `exact`, unit `PROFILER_UNIT_COUNT` and scale 1.0 with no limit or budget row unless the table says otherwise:

| Row | Expression | Kind | Unit, scale, limit, budget |
|---|---|---|---|
| UI memory | `ui.memory_bytes` | bar | `PROFILER_UNIT_BYTES`, scale 1.0; limit and budget value both the `Win RAM` value of Z2-B-UI-001 in bytes, as a numeric literal (the row is allocated before the tab lands); budget row `Z2-B-UI-001` |
| Screens open | `ui.screens.open` | value | |
| Elements laid out | `ui.layout.elements_laid_out` | sparkline | |
| Quads, Batches | `ui.draw.quads`, `ui.draw.batches` | value | |
| Paragraphs shaped | `ui.text.shaped_paragraphs` | sparkline | |
| Glyphs generated | `ui.glyphs.generated` | sparkline | |
| Events dropped | `ui.events.dropped` | value | |
| Semantic nodes | `ui.a11y.nodes` | value | |

### Frame-dump entries

UI's `Zenith_UIFrameDumpSection` implements DEVT's `Zenith_DevFrameDumpSection` (`Zenith2/Docs/Design/DEVT/Console.md → "DEVT frame dump"`) with the member name `ui`; DEVT calls its `Write` on the main thread at frame-loop step 3. It writes one object per attached world: open screens with document paths and states, the focused element, element counts, quad and batch counts, the active locale and palette, and the last 16 published events (kept in a 16-entry ring per world record).

<!-- assumption: pending DEVT, MOD (UI23) -->
A module reaches DEVT's frame dump through `Zenith_ModuleStartContext::GetDevFrameDump()` (null in Shipping and in tests that omit it, like `GetDevConsole()`), registers its section with `Zenith_DevFrameDump::AddSection` at start step 7 and removes it before the module instance is destroyed.
<!-- /assumption -->

### Cook report members

`ui.documents` (per document: element count, validation diagnostics), `ui.fonts` (faces, baked glyphs, pages), `ui.localization` (per locale: messages, missing, stale, uncovered code points), `ui.extract` (the extraction summary).

## Authoring and cooking

### What UI authors

UI documents, themes, font families, localization projects and string tables: canonical JSON with versioned schemas (Z2-D-038), migrated forward with preview, backup and unknown-field preservation (Z2-D-048).

### Editor surfaces

EDIT builds the visual UI authoring tool (Z2-D-079) on what UI provides (row UI15):
- `Zenith_UIEditorModel`: an editable in-memory UI document with reversible edit operations (insert, remove, move, set member, set style, bind, rename), validation after each edit, and canonical save through the document system;
- a preview `Zenith_UIContext` laid out at a chosen surface size, locale, palette, pseudo mode and text scale, with element picking by `HitTest`;
- the widget catalogue (`Zenith_UIWidgetDesc` per element type with its members and defaults) and the style property catalogue (`Zenith_UIPropertyDesc`);
- validation diagnostics with document paths for inline display.

### Cook steps and derived-data keys

| Step | Input | Output | Derived-data key inputs |
|---|---|---|---|
| `ui.document` | `.ui.json` | `.zui`, with the document's theme (or the engine default theme) folded in | document bytes, the bytes of the theme and of its `extends` chain, referenced string-table source keys, the step version |
| `ui.theme` | `.uitheme.json` | no cooked file: validates each theme, which `ui.document` folds into every `.zui` that names it (classes, palettes, metric tokens, sprites, animations) | theme bytes, step version |
| `ui.font` | `.fontfamily.json` and face files | `.zfont` | face file hashes, baked code-point set, MSDF parameters, msdfgen version |
| `ui.localization` | the project and every string table | one `.zloc` per locale | all tables of the locale chain, CLDR version, step version |
| `ui.extract` | every document, script and contributing step | the extraction report | inputs' hashes |
| `ui.manifest` | every cooked `.zui` | `UI/BindingManifest.zbin` | the cooked documents' binding records |

Steps register through `Zenith_RegisterUICookSteps`; the importer `ui.fontfamily` and the sprite preset `ui.sprite` (row UI11) are UI's.

### Migration and re-cook

Each document type starts at version 1 with an identity migration fixture; a schema change adds a migration step and a fixture pair in its own ticket. Cooked formats re-cook on version change (no runtime reading of old cooked versions: `UI_ERROR_DOCUMENT_VERSION`).

## Failure handling

### Failure table

| Failure | Behaviour | Test |
|---|---|---|
| a UI document fails to load or is corrupt | the screen does not open (`UI_ERROR_DOCUMENT_CORRUPT`, counted `ui.screen.document_unavailable`, logged once); the entity's component keeps its document reference; a hot reload that fixes it opens the screen | `Test_UIDocumentLoad_RejectsCorrupt`, `Test_UIDocumentLoad_HotReloadRepairsScreen` |
| a document's dependent font is missing | the font kind's SUBSTITUTE fallback (the engine default family) renders the text | `Test_UIDocumentLoad_MissingFontSubstitutes` |
| a glyph is missing from every face in the stack | `.notdef` from the first face, counted `ui.text.missing_glyphs` | `Test_UIFontStack_FallbackOrder` |
| dynamic glyph generation fails | the glyph draws as `.notdef`, counted `ui.glyphs.generation_failed`, retried after a locale or font change | `Test_GlyphCache_DynamicStateMachine` |
| atlas page creation fails (device memory, pressure) | the page stays CPU-side, glyphs pending, retried next frame (Text.md "Page upload and residency") | `Test_GlyphCache_PageCreateFailureRetries` |
| memory pressure | `OnPressureLevelChanged` sheds unused dynamic pages, the layout caches beyond their minimum, and unreferenced faces | `Test_GlyphCache_ShedsUnderPressure` |
| a localization table fails to load | the previous locale stays; a missing key formats as the marker | `Test_UILoc_LocaleSwitchAtFrameBoundary`, `Test_UILoc_FormatArguments` |
| a UI list does not fit the render-world slot | that world draws no UI this frame (`ui.draw.slot_full`) | `Test_UIExtract_SlotFullDrawsNothing` |
| the publication limits are reached (per frame or in flight) | an ordinary event beyond `maxPerFrame − 1` per frame or `maxInFlight − 1` outstanding is dropped and counted `ui.events.dropped`; the control returns to its bound value; the reserved slot still admits a resume record, so a pause menu always resumes | `Test_UIEvents_ChannelFullDropsAndCounts`, `Test_UIPauseMenu_ResumeClearsMenuReason` |
| a resume record cannot be parked (the reserved slot is in use, or the sink refuses) | the `resumeWorld` action does not touch the world's control: the menu stays open and paused, `ui.events.resume_refused` counts, and the log holds neither a record nor a clear, so a replay stays consistent | `Test_UIPauseMenu_ResumeClearsMenuReason` |
| live input during `--replay` | dropped before any completion ID is allocated, counted `ui.replay.live_events_ignored`; a live `resumeWorld` counts `ui.replay.live_resume_ignored` | `Test_UIEvents_ReplayIdentical`, `Test_UIPauseMenu_ResumeClearsMenuReason` |
| a request names a destroyed screen or unknown element | counted, ignored | `Test_UIRequests_FocusScrollAnnounce` |
| device loss | pages are recreated from CPU copies (Rendering.md "Headless and device loss"); text is pending for about a frame | `Test_UIRender_DeviceLossRecreatesPages` |
| headless (no renderer) | layout, events and accessibility run; no page is created and no MSDF generated; draw lists are still built for tests | `Test_UIModule_HeadlessRunsWithoutRenderer`, `Test_GlyphCache_HeadlessNeverGenerates` |
| no platform accessibility service | no tree is published; built-in narration is unavailable; settings remain | `Test_UIA11y_NoServiceKeepsSettings` |
| an input session is refused (another field holds IME) | the field does not enter editing, counted | `Test_UITextField_SessionLifecycle` |
| a corrupt or malicious payload in a replay | decode fails, the event is dropped and counted `ui.events.decode_failed`, the replay continues | `Test_UIEvents_ReplayCorruptPayloadDropped`, `Test_UIPublication_RejectsMalformed` |
| allocation failure inside the context | the screen open fails with `UI_ERROR_OUT_OF_MEMORY`; existing screens are unaffected | `Test_UIContext_OpenScreenOutOfMemory` |
| a start or attach step fails | the steps before it unwind in reverse | the eleven start-step and six attach-step unwinding tests listed by name in Tests.md "Integration tests" |
| budget breach (layout time) | reported by the budget machinery; UI does not degrade behaviour on its own | none in UI: the per-system budget run reports it (`Zenith2/Docs/Budgets.md`) |

## Test strategy

### Test targets and gate steps

Targets, fixtures and steps are Tests.md "Test targets and files", "Fixtures" and "Gate steps and labels". Every named test is in Tests.md; no test pins a count, no test compares against a committed binary, and no test doubles a concrete class.

### Test catalogue index

| Group | Tests.md heading |
|---|---|
| unit tests per subsystem | "Unit tests" |
| layout expectations | "Layout golden suite" |
| text shaping and Unicode rules | "Shaping golden suite" |
| GPU | "Render tests" |
| module, replays, worker counts | "Integration tests" |
| parsers | "Fuzz targets" |
| performance | "Benchmarks and harnesses" |
| stage acceptance | "Acceptance runs" |

## Budget rows

### Rows owned

| Row | Name | Status | UI's part |
|---|---|---|---|
| `Z2-B-UI-001` | UI, fonts and glyph atlases | pending (Stage 1 per-system budget ticket for UI) | host bytes (documents, element tables, caches, fonts' CPU copies, tables) and device bytes (atlas pages), dual-resident, `MEMORY_SUBSYSTEM_ASSETS` share, MP-SUBMEM on W-UI2D and WS-SLICE |
| `Z2-B-UI-002` | UI update and layout CPU | pending (same ticket) | claimant callbacks, `UI.Events`, `UI.Captions`, the extractor, MP-CPUZONE; inside CPU-SIM's sum |
| `Z2-B-UI-003` | UI composite GPU | pending (same ticket) | `UIComposite` and `UIWorldSpace`, MP-GPUPASS |

The budget ticket has no harness half (`Zenith2/Docs/Budgets.md → "Per-system producer shape"`); it is blocked by the CORE, JOBS and RPROD rulings ([Z2-N-BUDGET-023]).

### Rows measured against

| Row | Why |
|---|---|
| `Z2-B-RPROD-027` input-to-photon latency | UI input claimed in Pump and drawn in the same frame |
| `Z2-B-PACK-004` install size Game2D | fonts and tables in the package |
| `Z2-B-CONTENT-009` source-asset quota WorkloadUI2D | W-UI2D content |
| `Z2-B-CORE-012` string intern capacity | interned element, event and key names |
| `Z2-B-RHI-009` bindless heap capacity | one descriptor per atlas page |
| `Z2-B-ASSET-009` critical asset load latency | the engine fonts and tables at start |
| `Z2-B-RPROD-003` render extraction CPU | the extractor's span |
| `Z2-B-RHI-006` upload ring hard cap | up to 16 MiB of quad data per frame at the default `m_uMaxQuadsPerFrame`, and up to `ui.atlas.pageUploadsPerFrame` atlas pages (8 MiB at the defaults) |

### Pending row needs

Requested of Budgets.md in cross-doc item X237 (cross-area row UI18), producer "Stage 1 per-system budget ticket for UI":

| Proposed row | Unit | Procedure | Workloads | Bound or exclusion |
|---|---|---|---|---|
| Dynamic glyph latency (first use to resident) | frames | `Bench_UI_DynamicGlyphs` | W-UI2D | not a bound |
| UI draw quads per frame | count | MP-COUNTER | W-UI2D, WS-SLICE | not a memory bound |
| UI publications per tick | count | MP-COUNTER | W-UI2D | within `ui.events.maxPerFrame` |
| UI layout cache bytes | MB | MP-SUBMEM | W-UI2D | within `Z2-B-UI-001` |

<!-- elaboration: the cross-backend tolerance is derived, not measured -->
UI requests no render-test tolerance row. `Test_UIRender_CrossBackend` lands with the UI renderer (slice 9) and needs its bound when it lands, which no Stage 1 budget ticket can measure and no worker may record (only `needs-human` tickets record run evidence). Its bound is therefore derived from the analytic oracles each device already passes (`Zenith2/Docs/Design/UI/Tests.md → "Render tests"`): at every pixel an oracle evaluates, both devices are within that oracle's stated tolerance of the analytic value, so they differ from each other by at most twice that tolerance (2/255 on solid interiors and gradients, 2 × (1/255 + 0.06) of coverage on shape edges, 0.2 of coverage on glyph edges, each encoded to the target format by the test), and the test asserts exactly those per-pixel bounds. No pixel outside an oracle's set is compared.

### Content specifications and quality matrix rows

<!-- elaboration: UI's rows for the two Stage 1 production-quality documents -->
- **ContentSpecs.** The Stage 1 ContentSpecs ticket writes `## UI` with the columns of [Z2-N-BUDGET-417] (UI elements, text runs, visible glyphs and draw batches maximum per slice or screen); UI's per-system budget ticket reconciles those figures with `Z2-B-UI-001` (bytes: element table rows × about 180 bytes, text cache blocks, the slice's font pages and tables) and `Z2-B-UI-002`/`-003` (the batch and glyph maxima are the inputs of `Bench_UI_MenuFrame` and `Bench_UI_Composite`), in its allocation table, as [Z2-N-BUDGET-419] requires. Requirement Z2-R-UI-157 carries it.
- **QualityMatrix.** The Stage 1 QualityMatrix ticket writes `## UI features` ([Z2-N-BUDGET-403] to [Z2-N-BUDGET-409]); UI's features, one row per (feature, tier) for every tier of `## Quality tiers`, with the Feature IDs the ticket assigns in UI's range `Z2-Q-UI-NNN`:

| Feature | Requirements | Windows technique | S24 technique | Fallback, trigger | Acceptance kind, reference |
|---|---|---|---|---|---|
| MSDF text (screen space) | Z2-R-UI-058, -104 | MTSDF glyphs, 1,024² pages, `ui.text.sdfSharpness` 1.0 | the same | none (one technique on every tier), stated | `golden`: the Game2D slice's menu capture, with the FLIP and per-channel thresholds the QualityMatrix ticket writes into the row ([Z2-N-BUDGET-407]) |
| world-space panels | Z2-R-UI-042, -105 | `UIWorldSpace` at the transparent extension point, depth test of the view's mode | the same; glyphs fade below `ui.world.minGlyphPixels` | none | `golden`: the OpenWorldSlice capture with a world panel |
| HDR UI | Z2-R-UI-180 | `Flux_EncodeOutput` in HDR10 or scRGB at `r.hdr.uiNits` | HDR10 at `r.hdr.uiNits` | SDR output, when the surface lacks HDR (LIGHT's output-mode fallback) | `human`: the Game2D slice in HDR10 (LIGHT's acceptance item) |
| reduced motion | Z2-R-UI-140, -148 | transitions 0, essential animations as fades | the same | none | `human`: the reduced-motion flow segment |
| colour-vision correction | Z2-R-UI-141, -149 | theme palette, else the CPU Machado correction | the same | none | `human`: the colour-blind flow segment |


## Stage plan

### Stage table

| Stage | UI deliverables | Gated by |
|---|---|---|
| S1 | the Stage 1 per-system budget ticket for UI (with the ContentSpecs reconciliation); the workload scale ticket for W-UI2D (`needs-human` ruling TASK, [Z2-N-BUDGET-195]); UI's rows for the Stage 1 QualityMatrix and ContentSpecs tickets ("Content specifications and quality matrix rows") | CORE, JOBS, RPROD rulings for the budget ticket |
| S2 | the D-SEL-TEXT and D-SEL-UIRT harness tickets (`Zenith_Bench_UI` with its smoke CTest and the selection tests that run the conformance cases on both candidates and report; the candidates themselves are written by the pulled-forward slices 3, 5 and 7) and their two `needs-human` ruling tickets, ruled in Stage 2 as Roadmap allows for a "Stage 1 or 2" decision not ruled in Stage 1 (`Zenith2/Docs/Roadmap.md → "Stage 2 decisions"`); the SA break-offset recording ticket (`needs-human`, ICU4C) that the D-SEL-TEXT harness needs; the Stage 2 pull-forward below | the W-UI2D scale ticket (the benchmark's scenes); row UI9 (the manifest integration mode `data` and the `thirdparty:` content root, X236 (9)), which slices 3 and 5 and the corpus need for `unicode-ucd`, `icu-dictionaries` and `noto-fonts` |
| S3 | Z2-I-UI-01 to -06, -10, -12, -16, -17: the module, documents, cook steps, layout, styles, bindings, focus and navigation, controls, text without bidi reordering beyond LTR plus the Hebrew test runs, font fallback, MSDF atlases, input claims and arbitration, text fields with IME, prompts and the rebind control, on-screen controls, world-space panels, the shell's RTL mirroring pass (slice 7, with its `Mirror*` cases), UIComposite and UIWorldSpace, events and requests, settings for UI scale; the shaping-golden recording ticket (`needs-human`, after the D-SEL-TEXT ruling) | D-SEL-TEXT, D-SEL-UIRT (open, Stage 1/2) ruled; rows UI1, UI6, UI7, UI8 |
| S4 | Z2-I-UI-07 to -09, -11, -13 to -15, -18 to -30: bidi and RTL text, the RTL validation of localized games (the mirroring pass itself is S3), the localization pipeline (extraction, plurals, XLIFF, pseudo modes), captions and subtitles, the accessibility settings screen, the semantic tree and the PLAT bridge, narration, reduced motion, colour-vision modes, contrast validation, virtualization, the editor model for EDIT's authoring tool, the accessible flows (automated halves in the gate, human halves at the Stage 4 acceptance) | rows UI4, UI5, UI14, UI15, UI24 (row UI9 already gates Stage 2, and `cldr-json` arrives with it); GAME's slices and routes for the flows |
| S5 | HDR10 and scRGB UI output (Z2-R-UI-180, with LIGHT's HDR output transforms); Stage 4 acceptance re-runs on the new render paths (`PathParity` for UIWorldSpace) | RPROD's Stage 5 paths |
| S6 | W-UI2D sustained on the S24; fuzz targets at full duration | shipping qualification |
| S7 | none: UI has no deferred inventory item | — |

<!-- elaboration: the Stage 2 pull-forward the selection harnesses need -->
**Stage 2 pull-forward.** The harnesses measure real code, so the slices they run on are scheduled in the `Stage 2 — Foundation` sprint with their fix version kept at `Z2 Stage 3` (`Zenith2/Docs/Roadmap.md → "Stage 2 pull-forward"`: "Stage 3 tickets that depend on no unresolved Stage 2 decision are scheduled in `Stage 2 — Foundation` with fix version `Z2 Stage 3`"): slices 1 (API-only headers and module skeleton), 2 (document types, cook-side compile), 3 (Unicode tables, the test font builder, the text interfaces, the text corpus case files), 5 (fonts, bake, glyph cache, MSDF), 7 (element table, style resolution, the shared layout pass, mirroring, the layout golden suite), 8 (bindings, the binding manifest, animations) and 9 (draw lists, `Flux_UIRenderer`, `UIComposite`) of "Ticket slices and ordering". None of them depends on an unresolved Stage 2 decision: the candidates' code is written by exactly three slices, each owning the candidate directories its interfaces isolate, until the rulings delete the losers, and slice 8's binding evaluation is shell code. Slice 3 writes both implementations of `Zenith_UIBidi` and `Zenith_UILineSegmenter` under `Internal/Text/Backends/` (B1: SheenBidi with ZenithUI's UAX #14 breaker and dictionary segmenter; B2: the ICU4X subset); slice 5 writes both implementations of `Zenith_UIOutlineSource` (T1: FreeType; T2: HarfBuzz's own outline extraction) and the HarfBuzz `Zenith_UIShaper` both share; slice 7 writes the shell's style resolver and both `Zenith_UIRuntimeBackend` implementations (`Internal/Runtime/Custom/`, `Internal/Runtime/RmlUi/`) with the candidate selection the per-candidate `SUBCASE`s and `Zenith_Bench_UI --candidate <name>` use. The two harness tickets write only the selection tests, `Zenith_Bench_UI` with its benchmarks and its smoke CTest, and no candidate code; the per-stack and per-candidate gate tests start with the slice that compiles the candidates (slice 5 for the text corpus tests, slice 7 for the layout suite). Slices 3 and 5 read `unicode-ucd`, `icu-dictionaries` and `noto-fonts` through the `thirdparty:` content root, so row UI9 gates the pull-forward (the S2 row above), not Stage 4. Everything else of Stage 3 stays in the Stage 3 sprint, and the Stage 2 harnesses run only what the pulled slices provide; a benchmark or conformance case that needs a subsequent slice is added by that slice's ticket:

| Harness item | Kind | Code it runs | Pulled slices it needs | In the Stage 2 harness |
|---|---|---|---|---|
| `Bench_UI_MenuFrame` (steady state; its 300 text leaves are `literal` text of a development document, since Stage 2 has no localization tables, `Zenith2/Docs/Design/UI/Tests.md → "Benchmarks and harnesses"`) | D-SEL-UIRT benchmark, `Z2-B-UI-002` | documents, styles, layout, text measurement, draw lists | 1, 2, 3, 5, 7, 9 | yes |
| `Bench_UI_HudFrame` (20 values per frame through `Zenith_UIContext::SetBinding`; its bound text in literal mode) | D-SEL-UIRT benchmark, `Z2-B-UI-002` | as above plus binding application | 1, 2, 3, 5, 7, 8, 9 | yes |
| `Bench_UI_ShapeParagraphs` | D-SEL-TEXT benchmark, `Z2-B-UI-002` | the paragraph pipeline on each stack | 1, 3, 5 | yes |
| `Bench_UI_Memory` | D-SEL-TEXT and budget benchmark, `Z2-B-UI-001` | the Game2D fonts, documents and caches resident (slice 14's ticket adds the locale tables) | 1, 2, 3, 5, 7 | yes |
| `Bench_UI_Composite` | D-SEL-UIRT benchmark, `Z2-B-UI-003` | `UIComposite` on the menu | 1, 2, 3, 5, 7, 9 | yes |
| `Bench_UI_DynamicGlyphs` | budget benchmark (pending row "Dynamic glyph latency") | dynamic glyphs and page upload | 1, 3, 5, 9 | yes |
| `Bench_UI_InputToPhoton` | budget benchmark, `Z2-B-RPROD-027` | the claimant, hit testing, controls | slice 10 (Stage 3) | no: added by slice 10's ticket |
| `Bench_UI_MenuLocaleSwitch` | budget benchmark, `Z2-B-UI-002` | a locale switch on the menu | slice 14 (Stage 4) | no: added by slice 14's ticket |
| `Bench_UI_VirtualList` | budget benchmark, `Z2-B-UI-002` | virtualization | slice 16 (Stage 4) | no: added by slice 16's ticket |
| CSS and ZenithUI-semantics cases of `Zenith2/Docs/Design/UI/Layout.md → "Layout conformance cases"` | the D-SEL-UIRT selection set (`Test_UIRuntimeSelection_ConformanceBothCandidates`, reporting) | each candidate's style resolution and layout | 1, 2, 3, 5, 7 | yes |
| shell cases of the same table except `ListFixedExtent`, `ListVariableExtent` and `WorldPanelCanvas` | gate cases of the shell on each compiled candidate (`Test_UILayoutGolden_AllCases`), in an invariant form relative to that candidate's own layout (`Zenith2/Docs/Design/UI/Tests.md → "Layout golden suite"`); their absolute expectations are reported by the selection test as class `shell` | shell code common to both candidates, over each candidate's layout | 1, 2, 3, 5, 7, 9 | yes; not a selection criterion |
| `ListFixedExtent`, `ListVariableExtent`; `WorldPanelCanvas` | shell gate cases | virtualization; world-space panels | slice 16 (Stage 4); slice 12 (Stage 3) | no: added by those slices' tickets |
| `TestFont.json`, Graphemes, Fallback, `Pseudo.json` in the test font, Bidi levels, Breaks (with the SA cases) and the outline equivalence check | the D-SEL-TEXT selection set (`Test_UITextSelection_CorpusBothCandidates`, reporting) | each stack's outline source, bidi backend and line segmenter, the shared shaper wiring | 1, 3, 5, and the SA break-offset recording | yes |

### API-only slices

1. `Zenith_Module_UI.h`, `Zenith_UITypes.h`, `Zenith_UIComponents.h`, `Zenith_UIEvents.h` and `ZenithFlux/Flux_UI.h` with contract blocks and stub definitions, so GAME, EDIT and RPROD tickets compile against them.
2. `Zenith_UIContext.h`, `Zenith_UIInput.h`, `Zenith_UIDrawList.h` before the runtime selection lands.
3. `Zenith_UIText.h`, `Zenith_UIFont.h` before D-SEL-TEXT is ruled.
4. `Zenith_UILocalization.h` and `Zenith_UIAccessibility.h` declarations at Stage 3, implementations at Stage 4.
5. `Zenith_UIEditorModel.h` before EDIT's authoring tickets.

### Ticket slices and ordering

1. API-only slice 1; the module skeleton with start and attach steps and their unwinding tests.
2. Document types, schemas, validation, migration fixtures (cook side).
3. Unicode table generation; the test font builder; text stack interfaces and both bidi and line-segmentation candidates (B1, B2); the text corpus case files in the format of `Zenith2/Docs/Design/UI/Text.md → "Corpus file format"` (`Thai.json` without its `breaks`, `icuVersion` and `recordedWith` members) and `Groups.json` listing every group with `"shapingGolden": false`; the SA break record case `Test_UITextCorpus_SaBreaks_Record`, its converter test `Test_UITextCorpus_SaBreakRecorderConverts` and the usage-tests manifest entry `icu4c` they link.
4. Three tickets in order: the SA break-offset recording (`needs-human`, blocked by slice 3: the human runs the committed record case over every SA case of the corpus files, Text.md "Corpus file format"); the D-SEL-TEXT harness (blocked by the recording and by slices 2, 5, 7 and 9, whose code its selection set and its benchmarks run: `Bench_UI_Memory` needs 2 and 7, `Bench_UI_DynamicGlyphs` needs 9, the corpus needs 5's outline sources; it writes `Test_UITextSelection_CorpusBothCandidates`, `Zenith_Bench_UI`'s text benchmarks and the smoke CTest, and no candidate code); then the `needs-human` ruling ticket, which appends the ruling and commits the reports; then an ordinary cleanup ticket that deletes the losing stack, its manifest entries and the selection test, and adds the per-group corpus tests on the winner (`Zenith2/Docs/Design/UI/Tests.md → "Shaping golden suite"`).
5. Fonts: importer, bake, `.zfont`, loader, registry, glyph cache, dynamic glyphs, MSDF; both outline candidates (T1, T2) and the shared HarfBuzz shaper.
6. D-SEL-UIRT harness (blocked by slices 7, 8 and 9; it writes `Test_UIRuntimeSelection_ConformanceBothCandidates` with its per-class report and the W-UI2D-scale menu, HUD and composite benchmarks, and no candidate code: both layout engines are slice 7's); then the `needs-human` ruling ticket, which appends the ruling and commits the reports; then an ordinary cleanup ticket that deletes the losing engine, its manifest entry (RmlUi when the custom runtime wins) and the selection test, and widens `Test_UILayoutGolden_AllCases` and its variants to every case on the winner against the absolute expectations (`Zenith2/Docs/Design/UI/Tests.md → "Layout golden suite"`).
7. Element table, the shell's style resolver, both `Zenith_UIRuntimeBackend` implementations (custom and RmlUi, with the RMLUI section writer and the candidate selection), the shared layout pass, mirroring, invalidation; the layout golden suite with its shell cases per candidate.
8. Bindings, binding manifest, animations.
9. Draw lists, `Flux_UIRenderer` (`EnableUI`'s body, blocked by RPROD's `EnableUI` declaration and Create-step tickets), `UIComposite`, render tests.
10. Claimant, hit testing, pointer and focus machines, navigation, controls, text fields, prompts, rebind, on-screen controls; `Bench_UI_InputToPhoton`.
11. Publications (after rows UI1 and UI22), `UI.Events`, command kinds, the Luau table and its misuse codes; replay, pause-menu and worker-count tests.
12. World-space panels and `UIWorldSpace`; the `WorldPanelCanvas` layout case.
13. The Noto shaping-golden recording (`needs-human`, after the D-SEL-TEXT ruling is applied; it commits the goldens and sets `"shapingGolden": true` in `Groups.json` for every group it records), then an ordinary ticket landing `Test_UITextCorpus_Shaping` over them.
14. Localization: tables, interpreter, plurals, numbers, switching, extraction, XLIFF, pseudo modes; `Bench_UI_MenuLocaleSwitch` and the locale tables of `Bench_UI_Memory` (Stage 4).
15. Bidi and RTL completion (Stage 4).
16. Virtualization; the `ListFixedExtent` and `ListVariableExtent` layout cases and `Bench_UI_VirtualList` (Stage 4).
17. Accessibility: settings screen, tree, bridge (after PLAT's accessibility-service tickets, `Zenith2/Docs/Design/PLAT/Accessibility.md`), narration, captions, reduced motion, colour vision, validation (Stage 4).
18. Editor model (Stage 4, before EDIT's UI authoring tickets).
19. Accessible flows: the engine-settings route, then each slice's flows after GAME's route ticket for it (row UI24).

## Decisions

### Settled decisions

| Decision | Effect on UI |
|---|---|
| Z2-D-019 | determinism: UI reaches simulation only through recorded publications and commands |
| Z2-D-053 | asynchronous results publish at tick boundaries in completion order |
| Z2-D-024 | UI is a statically linked module |
| Z2-D-038 | JSON authoring, versioned cooked formats |
| Z2-D-048 | migrate, never drop |
| Z2-D-079 | visual UI authoring (EDIT builds it on UI's editor model) |
| Z2-D-021 | the flagship slices the accessible flows run on |
| Z2-D-082 | third-party code only in `Internal/` |
| Z2-D-034 | Zenith Hungarian naming |
| Z2-D-045 | no exceptions outside the Luau firewall: `Zenith_Result` for recoverable errors, asserts for programmer errors only, untrusted input never asserts |
| Z2-D-083 | stage acceptance runs |
| Z2-D-097 | rulings appended to the decision log in the ticket's diff |

### Open decisions

| Decision | Stage | What it gates |
|---|---|---|
| D-SEL-TEXT (open, Stage 1/2) | 1/2; UI schedules its ruling in Stage 2 | the text stack candidates T1/T2 × B1/B2 |
| D-SEL-UIRT (open, Stage 1/2) | 1/2; UI schedules its ruling in Stage 2 | RmlUi's layout and style engine or ZenithUI's own, inside the shared shell |

### Selection decisions UI owns

<!-- elaboration: the acceptance rules adopt DecisionLog's criteria and add an order of comparison; the D-SEL-UIRT scope is sharpened through a shared shell, recorded for DecisionLog.md in cross-doc item X251 -->
Each selection is two tickets (Z2-D-084): an ordinary harness ticket the loop runs, which writes `Zenith_Bench_UI` with a registered `--smoke` CTest and the selection test that runs the selection's conformance set in the `Test` step on both candidates, written beforehand by slices 3 and 5 (text) or 7 (runtime) ("Stage table"); and a `needs-human` ruling ticket, blocked by the harness ticket and by the Stage 1 per-system budget ticket for UI (which turns `Z2-B-UI-001`, `-002` and `-003` from pending into bounds that step 2 can compare against), in which the human measures on the RTX 3070 desktop and the S24, runs the selection test executable on Windows and on the S24 (`android-arm64`, where the gate's `Test` step does not run it) and commits both reports under `Zenith2/Docs/Research/`, appends the ruling to `Zenith2/Docs/DecisionLog.md` with the next free `Z2-D-NNN` and confirms the licence fields of the winner's entries in its report, which is DecisionLog's "licences are recorded in the dependency inventory". A third, ordinary cleanup ticket blocked by the ruling deletes the losing candidate with its `Zenith2/ThirdParty/manifest.json` entries (SheenBidi or the ICU4X subset; FreeType when T2 wins; RmlUi when the custom runtime wins), the selection test and the candidate test-access header, and adds or widens the winner's tests: a `needs-human` ticket carries no worker code. No worker records a measurement. Both pairs are scheduled in the Stage 2 sprint ("Stage plan"); D-SEL-TEXT's pair is preceded by the SA break-offset recording ticket ("Ticket slices and ordering", slice 4).

A selection test is the comparison's instrument, not its judge: a candidate failing a conformance case is the data step 1 of its rule reads, so the test records it and passes. It writes, on every run, the report `<ZENITH_TEST_GENERATED_ROOT>/Selection/RuntimeSelection.json` or `…/TextSelection.json` (the target's generated root of `Zenith2/Docs/Architecture/BuildSystem.md`, [Z2-N-ARCH-250]; never committed): per candidate, per case, the case's class and pass or fail with its first differing value, every shim used with its code size in bytes, and the per-class totals. It fails only on a harness error: a candidate whose backend cannot be created (`UI_ERROR_RUNTIME_BACKEND`) or that crashes, a case file missing, unparsable or not executed on every candidate, or a report that is not written or does not parse back with one entry per (candidate, case). It never fails because a candidate loses, and it asserts nothing about which candidate passes step 1; when neither does, the ruling records the shortfall. The ruling ticket's human commits that report with the measurements.

| Decision | Candidates | Criteria (DecisionLog's, adopted verbatim) | Acceptance rule (order of comparison) |
|---|---|---|---|
| D-SEL-TEXT | T1: HarfBuzz + FreeType + msdfgen; T2: HarfBuzz with its own outline extraction + msdfgen; bidi B1: SheenBidi; B2: an ICU4X subset (Text.md "Text stack selection") | shapes the test corpus (Arabic, Hebrew, Devanagari, Thai, CJK, pseudo-localized long strings) correctly; supports a font fallback chain; fits `Z2-B-UI-001`; meets `Z2-B-UI-002` in W-UI2D on the S24; licences recorded | 1. Conformance: every case of `Test_UITextSelection_CorpusBothCandidates`'s reports passes for the stack on Windows and on the S24 (the shaping wiring through the test font, the Bidi group's levels, graphemes, line and SA dictionary breaks against the recorded ICU4C offsets, fallback, the pseudo-localized long strings of `Pseudo.json` laid out in the test font, and outline equivalence; HarfBuzz is common, so shaping goldens are not a criterion), and the stack supports the fallback chain; a stack failing either is out. 2. Budgets, conjunctively: among the stacks passing step 1, a stack that both fits `Z2-B-UI-001` and meets `Z2-B-UI-002` in the W-UI2D-scale benchmark on the S24 beats every stack that misses either row. 3. Only between two stacks that both meet both rows: the lower measured `Z2-B-UI-002` time wins when the difference exceeds the benchmark's run-to-run spread recorded in the report; 4. otherwise the smaller resident bytes against `Z2-B-UI-001`; 5. otherwise the smaller added binary size on the S24. No candidate is the default. When no stack passes step 1, or no stack passing it meets every row, the ruling records the shortfall and the human decides. |
| D-SEL-UIRT | RmlUi's layout and style engine; ZenithUI's own (Layout.md "Runtime selection"), each inside the shared shell | DecisionLog's: retained layout, navigation and focus, a semantic accessibility tree, large-list virtualization, world-space UI, rendering through Flux's render graph and visual authoring (all provided by the shared shell for both candidates); `Z2-B-UI-002` and `Z2-B-UI-003` in W-UI2D on the S24 | 1. Conformance: every CSS case and every ZenithUI-semantics case of "Layout conformance cases" in `Test_UIRuntimeSelection_ConformanceBothCandidates`'s report passes for the candidate; a ZenithUI-semantics case may be met natively or through a documented shim inside the candidate's backend directory, listed in the report with its code size; a candidate failing any case unmet by a shim is out. Shell cases are not part of this set: while the decision is open they gate only in an invariant form relative to each candidate's own layout, and their absolute expectations are reported as class `shell`, recorded and not judged (`Zenith2/Docs/Design/UI/Tests.md → "Layout golden suite"`). 2. Budgets, conjunctively: among the candidates passing step 1, a candidate that meets both `Z2-B-UI-002` and `Z2-B-UI-003` in the W-UI2D-scale benchmark on the S24 beats every candidate that misses either row. 3. Only between two candidates that both meet both rows: the lower `Z2-B-UI-002` time wins when the difference exceeds the recorded run-to-run spread; 4. otherwise the smaller added binary size on the S24. No candidate is the default. When no candidate passes step 1, or no candidate passing it meets every row, the ruling records the shortfall and the human decides. |

### Design rulings owned by UI

- Screens are entities; contexts are per world; the UI system is per process.
- Scripts read no presentation state; UI events are publications, requests are commands.
- Accessibility and localization settings are presentation configuration, never simulation-affecting.
- A pause menu is resumed by UI clearing the `MENU` pause reason in a presentation action, never by a script (Interaction.md "Pause menus", GAME's row GA13).
- No committed binary test artefacts: fonts and images are generated in tests; only the SA break offsets and the Noto shaping goldens are recorded, as text, with independent tools (ICU4C through a committed record case, `hb-shape`) by two `needs-human` tickets: the SA offsets before the D-SEL-TEXT ruling, the shaping goldens after it.
- A pause menu's resume is recorded as a UI publication before the world's `MENU` reason is cleared, from a publication slot reserved for it, so every live resume has a log record and replays resume (Interaction.md "Pause menus").
- Under `--replay`, live viewer input is dropped before a completion ID is allocated, so it never moves the world's shared completion-ID counter; that the counter reproduces the IDs UI allocated in the recorded run is the replaying table's adoption of the recorded next completion ID ([Z2-N-ARCH-794], cross-doc item X278; Interaction.md "Publication path").
- Style resolution is one pass of the shared shell for both D-SEL-UIRT candidates; RmlUi's style engine cascades the layout-affecting properties from cook-generated RCSS and run-time pushes (Layout.md "The RmlUi integration").
- RTL mirroring is one pass of the shared shell: margins exchanged in RTL flows, then each child of an RTL element mirrored inside its parent's content box, padding, borders and radii staying physical (Layout.md "Logical direction and mirroring").

## Requirement index

The catalogue is two files with one numbering: `Zenith2/Docs/Requirements/UI.md` (Stages 1 to 3, the coverage table and the exclusions) and `Zenith2/Docs/Requirements/UI/Stage4Plus.md` (Stages 4 to 6), split by stage as the brief's two-file convention allows so that neither exceeds 2,000 lines.

| Requirement | Title | Stage | Design headings |
|---|---|---|---|
| Z2-R-UI-001 | Module library target | S3 | Zenith2/Docs/Design/UI.md → "Target summary"; Zenith2/Docs/Design/UI.md → "Libraries UI code depends on"; Zenith2/Docs/Design/UI.md → "ZenithUI files" |
| Z2-R-UI-002 | Module entry points | S3 | Zenith2/Docs/Design/UI/PublicAPI.md → "Zenith_Module_UI header"; Zenith2/Docs/Design/UI.md → "Module start sequence" |
| Z2-R-UI-003 | Start steps unwind | S3 | Zenith2/Docs/Design/UI.md → "Module start sequence"; Zenith2/Docs/Design/UI.md → "World attach and detach"; Zenith2/Docs/Design/UI.md → "Construction and shutdown" |
| Z2-R-UI-004 | Public headers carry contracts | S3 | Zenith2/Docs/Design/UI.md → "Rules across the UI API"; Zenith2/Docs/Design/UI/PublicAPI.md → "Header list" |
| Z2-R-UI-005 | API-only slices first | S3 | Zenith2/Docs/Design/UI.md → "API-only slices"; Zenith2/Docs/Design/UI.md → "Ticket slices and ordering" |
| Z2-R-UI-006 | Error enumerations | S3 | Zenith2/Docs/Design/UI.md → "Error enumerations"; Zenith2/Docs/Design/UI/PublicAPI.md → "Zenith_UITypes header" |
| Z2-R-UI-007 | Configuration keys are presentation only | S3 | Zenith2/Docs/Design/UI.md → "Configuration keys"; Zenith2/Docs/Design/UI/PublicAPI.md → "Zenith_UISystem header" |
| Z2-R-UI-008 | Screens are entities | S3 | Zenith2/Docs/Design/UI/PublicAPI.md → "Zenith_UIComponents header"; Zenith2/Docs/Design/UI/Interaction.md → "Script bindings" |
| Z2-R-UI-009 | One context per world | S3 | Zenith2/Docs/Design/UI.md → "World attach and detach"; Zenith2/Docs/Design/UI/Interaction.md → "Input claims" |
| Z2-R-UI-010 | World resources and owners | S3 | Zenith2/Docs/Design/UI.md → "World resources"; Zenith2/Docs/Design/UI.md → "Frame-graph placement"; Zenith2/Docs/Design/UI/Interaction.md → "Publication path" |
| Z2-R-UI-011 | Frame-graph placement | S3 | Zenith2/Docs/Design/UI.md → "Frame-graph placement"; Zenith2/Docs/Design/UI/Interaction.md → "Frame flow" |
| Z2-R-UI-012 | Context single-owner threading | S3 | Zenith2/Docs/Design/UI.md → "Thread affinity per type"; Zenith2/Docs/Design/UI.md → "Lock-order entries" |
| Z2-R-UI-013 | Memory consumer | S3 | Zenith2/Docs/Design/UI.md → "Module start sequence"; Zenith2/Docs/Design/UI.md → "Failure table"; Zenith2/Docs/Design/UI/Text.md → "Glyph cache and atlases" |
| Z2-R-UI-014 | Five authoring document types | S3 | Zenith2/Docs/Design/UI/Documents.md → "Canonical schemas and document types"; Zenith2/Docs/Design/UI/Documents.md → "Migration chains and fixtures" |
| Z2-R-UI-015 | UI document schema | S3 | Zenith2/Docs/Design/UI/Documents.md → "UI document schema"; Zenith2/Docs/Design/UI/Documents.md → "Element types" |
| Z2-R-UI-016 | Theme documents | S3 | Zenith2/Docs/Design/UI/Documents.md → "Theme document schema"; Zenith2/Docs/Design/UI/Documents.md → "Palettes and tokens" |
| Z2-R-UI-017 | Style property catalogue | S3 | Zenith2/Docs/Design/UI/Documents.md → "Style property catalogue"; Zenith2/Docs/Design/UI/PublicAPI.md → "Zenith_UIStyle header" |
| Z2-R-UI-018 | Document validation rules | S3 | Zenith2/Docs/Design/UI/Documents.md → "Validation rules" |
| Z2-R-UI-019 | Cooked UI document format | S3 | Zenith2/Docs/Design/UI/Documents.md → "Cooked UI document format"; Zenith2/Docs/Design/UI/Documents.md → "Typed data records"; Zenith2/Docs/Design/UI.md → "Cook steps and derived-data keys" |
| Z2-R-UI-020 | Instances flatten at cook | S3 | Zenith2/Docs/Design/UI/Documents.md → "Templates and instances"; Zenith2/Docs/Design/UI/Layout.md → "Layout conformance cases" |
| Z2-R-UI-021 | Document loader validates | S3 | Zenith2/Docs/Design/UI.md → "Asset kind loaders"; Zenith2/Docs/Design/UI.md → "Failure table" |
| Z2-R-UI-022 | Binding manifest | S3 | Zenith2/Docs/Design/UI/Documents.md → "Binding manifest file"; Zenith2/Docs/Design/UI.md → "World attach and detach" |
| Z2-R-UI-023 | Cook steps registered | S3 | Zenith2/Docs/Design/UI.md → "Cook steps and derived-data keys"; Zenith2/Docs/Design/UI/ServicesAPI.md → "Tool-side headers" |
| Z2-R-UI-024 | Parser limits | S3 | Zenith2/Docs/Design/UI/Documents.md → "Parser limits"; Zenith2/Docs/Design/UI/Tests.md → "Fuzz targets" |
| Z2-R-UI-025 | Runtime selection ruling | S2 | Zenith2/Docs/Design/UI.md → "Selection decisions UI owns"; Zenith2/Docs/Design/UI/Layout.md → "Runtime selection" |
| Z2-R-UI-026 | Element table | S3 | Zenith2/Docs/Design/UI/Layout.md → "Element tree model" |
| Z2-R-UI-027 | Canvas units and scaling | S3 | Zenith2/Docs/Design/UI/Layout.md → "Canvas units and scaling"; Zenith2/Docs/Design/UI/Layout.md → "Safe area and surface changes" |
| Z2-R-UI-028 | Style cascade | S3 | Zenith2/Docs/Design/UI/Layout.md → "Style resolution"; Zenith2/Docs/Design/UI/Layout.md → "What both candidates implement" |
| Z2-R-UI-029 | Stack layout | S3 | Zenith2/Docs/Design/UI/Layout.md → "Stack layout"; Zenith2/Docs/Design/UI/Layout.md → "Size resolution" |
| Z2-R-UI-030 | Grid layout | S3 | Zenith2/Docs/Design/UI/Layout.md → "Grid layout" |
| Z2-R-UI-031 | Anchored and overlay layout | S3 | Zenith2/Docs/Design/UI/Layout.md → "Anchored layout" |
| Z2-R-UI-032 | Intrinsic sizes and text leaves | S3 | Zenith2/Docs/Design/UI/Layout.md → "Intrinsic sizes"; Zenith2/Docs/Design/UI/Layout.md → "Text leaves" |
| Z2-R-UI-033 | Pixel snapping | S3 | Zenith2/Docs/Design/UI/Layout.md → "Rounding and pixel snapping" |
| Z2-R-UI-034 | Linear layout cost | S3 | Zenith2/Docs/Design/UI/Layout.md → "Complexity and bounds"; Zenith2/Docs/Design/UI/Layout.md → "Invalidation and incremental layout" |
| Z2-R-UI-035 | Layout conformance suite | S3 | Zenith2/Docs/Design/UI/Layout.md → "Layout conformance cases"; Zenith2/Docs/Design/UI/Tests.md → "Layout golden suite" |
| Z2-R-UI-036 | Scroll containers | S3 | Zenith2/Docs/Design/UI/Layout.md → "Scroll containers"; Zenith2/Docs/Design/UI/Interaction.md → "Scrolling" |
| Z2-R-UI-037 | Fixed-extent virtualization | S4 | Zenith2/Docs/Design/UI/Layout.md → "Fixed-extent lists" |
| Z2-R-UI-038 | Variable-extent virtualization | S4 | Zenith2/Docs/Design/UI/Layout.md → "Variable-extent lists" |
| Z2-R-UI-039 | Item recycling | S4 | Zenith2/Docs/Design/UI/Layout.md → "Item recycling" |
| Z2-R-UI-040 | Data binding evaluation | S3 | Zenith2/Docs/Design/UI/Layout.md → "Data binding evaluation"; Zenith2/Docs/Design/UI/Interaction.md → "Activation and value controls" |
| Z2-R-UI-041 | Animations and transitions | S3 | Zenith2/Docs/Design/UI/Layout.md → "Animations and transitions" |
| Z2-R-UI-042 | World-space panels | S3 | Zenith2/Docs/Design/UI/Layout.md → "World-space panels"; Zenith2/Docs/Design/UI/Interaction.md → "Hit testing" |
| Z2-R-UI-043 | Text stack selection ruling | S2 | Zenith2/Docs/Design/UI.md → "Selection decisions UI owns"; Zenith2/Docs/Design/UI/Text.md → "Text stack selection" |
| Z2-R-UI-044 | Generated Unicode tables | S3 | Zenith2/Docs/Design/UI/Text.md → "Unicode data" |
| Z2-R-UI-045 | UTF-8 repair before layout | S3 | Zenith2/Docs/Design/UI/Text.md → "Input and decoding"; Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIText header" |
| Z2-R-UI-046 | Shaping | S3 | Zenith2/Docs/Design/UI/Text.md → "Shaping"; Zenith2/Docs/Design/UI/Text.md → "Text conformance corpus"; Zenith2/Docs/Design/UI/Tests.md → "Shaping golden suite" |
| Z2-R-UI-047 | Font fallback per cluster | S3 | Zenith2/Docs/Design/UI/Text.md → "Font fallback"; Zenith2/Docs/Design/UI/Localization.md → "Font selection per locale" |
| Z2-R-UI-048 | Line breaking | S3 | Zenith2/Docs/Design/UI/Text.md → "Line breaking" |
| Z2-R-UI-049 | Grapheme boundaries | S3 | Zenith2/Docs/Design/UI/Text.md → "Caret and selection"; Zenith2/Docs/Design/UI/Text.md → "Text editing" |
| Z2-R-UI-050 | Truncation and ellipsis | S3 | Zenith2/Docs/Design/UI/Text.md → "Truncation and ellipsis" |
| Z2-R-UI-051 | Paragraph limit | S3 | Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIText header"; Zenith2/Docs/Design/UI/Text.md → "Input and decoding" |
| Z2-R-UI-052 | Bidirectional resolution | S4 | Zenith2/Docs/Design/UI/Text.md → "Bidirectional resolution" |
| Z2-R-UI-053 | Visual reordering | S4 | Zenith2/Docs/Design/UI/Text.md → "Line layout and reordering" |
| Z2-R-UI-054 | Caret in bidi text | S4 | Zenith2/Docs/Design/UI/Text.md → "Caret and selection" |
| Z2-R-UI-055 | Text editing buffer | S3 | Zenith2/Docs/Design/UI/Text.md → "Text editing"; Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIText header" |
| Z2-R-UI-056 | Text layout cache | S3 | Zenith2/Docs/Design/UI/Text.md → "Text layout cache" |
| Z2-R-UI-057 | Font family bake | S3 | Zenith2/Docs/Design/UI/Documents.md → "Cooked font format"; Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIFontBake header" |
| Z2-R-UI-058 | Baked equals dynamic | S3 | Zenith2/Docs/Design/UI/Text.md → "MSDF generation" |
| Z2-R-UI-059 | Atlas packing | S3 | Zenith2/Docs/Design/UI/Text.md → "Atlas packing" |
| Z2-R-UI-060 | Dynamic glyph state machine | S3 | Zenith2/Docs/Design/UI/Text.md → "Dynamic glyph state machine" |
| Z2-R-UI-061 | Headless glyph cache | S3 | Zenith2/Docs/Design/UI/Text.md → "Page upload and residency"; Zenith2/Docs/Design/UI/Rendering.md → "Headless and device loss" |
| Z2-R-UI-062 | Font loader and fallback family | S3 | Zenith2/Docs/Design/UI.md → "Asset kind loaders"; Zenith2/Docs/Design/UI.md → "Failure table" |
| Z2-R-UI-063 | Text corpus fixtures and recorded goldens | S3 | Zenith2/Docs/Design/UI/Text.md → "Text conformance corpus"; Zenith2/Docs/Design/UI/Tests.md → "Shaping golden suite" |
| Z2-R-UI-064 | Claimant registration | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Input claims"; Zenith2/Docs/Design/UI.md → "Module start sequence" |
| Z2-R-UI-065 | Pointer claims | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Input claims"; Zenith2/Docs/Design/UI/Interaction.md → "Input arbitration with gameplay" |
| Z2-R-UI-066 | Hit testing order | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Hit testing" |
| Z2-R-UI-067 | Pointer state machine | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Pointer state machine" |
| Z2-R-UI-068 | Focus scopes | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Focus scopes" |
| Z2-R-UI-069 | Focus state machine | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Focus state machine" |
| Z2-R-UI-070 | Focus visibility | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Focus visibility" |
| Z2-R-UI-071 | Directional navigation | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Directional navigation algorithm" |
| Z2-R-UI-072 | Tab order | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Tab order" |
| Z2-R-UI-073 | Navigation repeat | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Input claims" |
| Z2-R-UI-074 | Control activation | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Activation and value controls" |
| Z2-R-UI-075 | Cancel handling | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Activation and value controls" |
| Z2-R-UI-076 | Config-bound controls | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Activation and value controls"; Zenith2/Docs/Design/UI/Documents.md → "Validation rules" |
| Z2-R-UI-077 | Text fields and IME | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Text fields and IME" |
| Z2-R-UI-078 | Keyboard consumed while editing | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Text fields and IME"; Zenith2/Docs/Design/UI/Interaction.md → "Input arbitration with gameplay" |
| Z2-R-UI-079 | Input arbitration | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Input arbitration with gameplay" |
| Z2-R-UI-080 | Input prompts | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Input prompts and remapping" |
| Z2-R-UI-081 | Rebind controls | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Input prompts and remapping" |
| Z2-R-UI-082 | On-screen controls | S3 | Zenith2/Docs/Design/UI/Interaction.md → "On-screen controls" |
| Z2-R-UI-083 | Publication payload | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Event kinds and payloads"; Zenith2/Docs/Design/UI/PublicAPI.md → "Zenith_UIEvents header" |
| Z2-R-UI-084 | Text event truncation | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Text fields and IME"; Zenith2/Docs/Design/UI/Interaction.md → "Event kinds and payloads" |
| Z2-R-UI-085 | Events reach scripts next tick | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Publication path" |
| Z2-R-UI-086 | Publications replay | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Publication path" |
| Z2-R-UI-087 | Worker-count invariance | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Publication path" |
| Z2-R-UI-088 | Paused clock defers events | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Publication path" |
| Z2-R-UI-089 | Pause menu resume | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Pause menus"; Zenith2/Docs/Design/UI.md → "World attach and detach" |
| Z2-R-UI-090 | Stale and dropped events | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Publication path"; Zenith2/Docs/Design/UI.md → "Failure table" |
| Z2-R-UI-091 | Two-way bindings | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Two-way bindings" |
| Z2-R-UI-092 | Command kinds | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Command kinds" |
| Z2-R-UI-093 | Requests are presentation only | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Command kinds" |
| Z2-R-UI-094 | Luau UI namespace | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Script bindings"; Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIScriptApi header" |
| Z2-R-UI-095 | Script misuse is a Luau error | S3 | Zenith2/Docs/Design/UI.md → "Luau errors and asserts"; Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIScriptApi header" |
| Z2-R-UI-096 | Paint order | S3 | Zenith2/Docs/Design/UI/Rendering.md → "Paint order" |
| Z2-R-UI-097 | Quads per part | S3 | Zenith2/Docs/Design/UI/Rendering.md → "Quad generation per element" |
| Z2-R-UI-098 | Clipping | S3 | Zenith2/Docs/Design/UI/Rendering.md → "Clipping" |
| Z2-R-UI-099 | Batching | S3 | Zenith2/Docs/Design/UI/Rendering.md → "Batching" |
| Z2-R-UI-100 | Quad record layout | S3 | Zenith2/Docs/Design/UI/Rendering.md → "Flux_UIQuad layout" |
| Z2-R-UI-101 | Render-world UI lists | S3 | Zenith2/Docs/Design/UI/Rendering.md → "Render-world records" |
| Z2-R-UI-102 | UI renderer | S3 | Zenith2/Docs/Design/UI/Rendering.md → "Flux UI renderer" |
| Z2-R-UI-103 | UIComposite pass | S3 | Zenith2/Docs/Design/UI/Rendering.md → "UIComposite implementation" |
| Z2-R-UI-104 | Shape and glyph shading | S3 | Zenith2/Docs/Design/UI/Rendering.md → "Shader programs" |
| Z2-R-UI-105 | UIWorldSpace pass | S3 | Zenith2/Docs/Design/UI/Rendering.md → "UIWorldSpace pass" |
| Z2-R-UI-106 | Atlas pages on the GPU | S3 | Zenith2/Docs/Design/UI/Rendering.md → "Atlas pages on the GPU" |
| Z2-R-UI-107 | Colour and HDR output (SDR) | S3 | Zenith2/Docs/Design/UI/Rendering.md → "Colour and HDR output" |
| Z2-R-UI-108 | Device loss | S3 | Zenith2/Docs/Design/UI/Rendering.md → "Headless and device loss"; Zenith2/Docs/Design/UI.md → "Failure table" |
| Z2-R-UI-109 | Cross-backend UI parity | S3 | Zenith2/Docs/Design/UI/Tests.md → "Render tests" |
| Z2-R-UI-110 | Headless UI runs | S3 | Zenith2/Docs/Design/UI/Rendering.md → "Headless and device loss"; Zenith2/Docs/Design/UI.md → "Failure table" |
| Z2-R-UI-111 | Locale tags | S4 | Zenith2/Docs/Design/UI/Localization.md → "Locales and negotiation" |
| Z2-R-UI-112 | Locale negotiation | S4 | Zenith2/Docs/Design/UI/Localization.md → "Locales and negotiation" |
| Z2-R-UI-113 | String tables and keys | S4 | Zenith2/Docs/Design/UI/Localization.md → "String tables and keys"; Zenith2/Docs/Design/UI/Documents.md → "Cooked localization table format" |
| Z2-R-UI-114 | Message compilation | S4 | Zenith2/Docs/Design/UI/Documents.md → "Message syntax"; Zenith2/Docs/Design/UI/Documents.md → "Message bytecode" |
| Z2-R-UI-115 | Message formatting | S4 | Zenith2/Docs/Design/UI/Localization.md → "Message formatting"; Zenith2/Docs/Design/UI/Localization.md → "Argument formatting" |
| Z2-R-UI-116 | Plural rules from CLDR | S4 | Zenith2/Docs/Design/UI/Localization.md → "Plural rules"; Zenith2/Docs/Design/UI/Documents.md → "Plural rule programs" |
| Z2-R-UI-117 | Select | S4 | Zenith2/Docs/Design/UI/Localization.md → "Select" |
| Z2-R-UI-118 | Rich-text tags | S4 | Zenith2/Docs/Design/UI/Localization.md → "Rich-text tags in messages" |
| Z2-R-UI-119 | Number formatting | S4 | Zenith2/Docs/Design/UI/Localization.md → "Number formatting" |
| Z2-R-UI-120 | Locale switching | S4 | Zenith2/Docs/Design/UI/Localization.md → "Locale switching at run time" |
| Z2-R-UI-121 | Font selection per locale | S4 | Zenith2/Docs/Design/UI/Localization.md → "Font selection per locale" |
| Z2-R-UI-122 | Extraction sources | S4 | Zenith2/Docs/Design/UI/Localization.md → "Extraction sources" |
| Z2-R-UI-123 | Extraction report | S4 | Zenith2/Docs/Design/UI/Localization.md → "Extraction report and exchange files" |
| Z2-R-UI-124 | XLIFF exchange | S4 | Zenith2/Docs/Design/UI/Localization.md → "Extraction report and exchange files" |
| Z2-R-UI-125 | Pseudo-localization modes | S4 | Zenith2/Docs/Design/UI/Localization.md → "Pseudo-localization and stress modes" |
| Z2-R-UI-126 | Long-string stress | S4 | Zenith2/Docs/Design/UI/Localization.md → "Localization validation"; Zenith2/Docs/Design/UI/Tests.md → "Layout golden suite" |
| Z2-R-UI-127 | RTL mirroring | S3 | Zenith2/Docs/Design/UI/Layout.md → "Logical direction and mirroring"; Zenith2/Docs/Design/UI/Layout.md → "Layout conformance cases" |
| Z2-R-UI-128 | Localization validation | S4 | Zenith2/Docs/Design/UI/Localization.md → "Localization validation"; Zenith2/Docs/Design/UI/Documents.md → "Validation rules" |
| Z2-R-UI-129 | Corrupt bytecode is safe | S4 | Zenith2/Docs/Design/UI/Documents.md → "Message bytecode" |
| Z2-R-UI-130 | Accessibility settings keys | S4 | Zenith2/Docs/Design/UI/Accessibility.md → "Accessibility settings"; Zenith2/Docs/Design/PLAT/Accessibility.md → "Preference keys and the platform layer" |
| Z2-R-UI-131 | Engine accessibility settings screen | S4 | Zenith2/Docs/Design/UI/Accessibility.md → "Accessibility settings" |
| Z2-R-UI-132 | Semantic roles and states | S4 | Zenith2/Docs/Design/UI/Accessibility.md → "Roles and states" |
| Z2-R-UI-133 | Accessible names | S4 | Zenith2/Docs/Design/UI/Accessibility.md → "Names and descriptions" |
| Z2-R-UI-134 | Stable semantic tree | S4 | Zenith2/Docs/Design/UI/Accessibility.md → "Semantic tree" |
| Z2-R-UI-135 | Screen-reader bridge | S4 | Zenith2/Docs/Design/UI/Accessibility.md → "Platform accessibility bridge"; Zenith2/Docs/Design/PLAT/Accessibility.md → "Zenith_AccessibilityService" |
| Z2-R-UI-136 | Built-in narration | S4 | Zenith2/Docs/Design/UI/Accessibility.md → "Narration composition"; Zenith2/Docs/Design/PLAT/Accessibility.md → "Zenith_AccessibilityService" |
| Z2-R-UI-137 | Live regions and announcements | S4 | Zenith2/Docs/Design/UI/Accessibility.md → "Live regions"; Zenith2/Docs/Design/PLAT/Accessibility.md → "Zenith_AccessibilityService" |
| Z2-R-UI-138 | Caption cue queue | S4 | Zenith2/Docs/Design/UI/Accessibility.md → "Caption cues" |
| Z2-R-UI-139 | Caption presentation | S4 | Zenith2/Docs/Design/UI/Accessibility.md → "Caption presentation" |
| Z2-R-UI-140 | Reduced motion | S4 | Zenith2/Docs/Design/UI/Accessibility.md → "Reduced motion" |
| Z2-R-UI-141 | Colour-vision palettes | S4 | Zenith2/Docs/Design/UI/Accessibility.md → "Colour-vision modes and contrast" |
| Z2-R-UI-142 | Contrast validation | S4 | Zenith2/Docs/Design/UI/Accessibility.md → "Colour-vision modes and contrast"; Zenith2/Docs/Design/UI/Accessibility.md → "Accessibility validation" |
| Z2-R-UI-143 | Text scaling | S4 | Zenith2/Docs/Design/UI/Accessibility.md → "UI scaling and text scaling" |
| Z2-R-UI-144 | Touch target size | S4 | Zenith2/Docs/Design/UI/Accessibility.md → "UI scaling and text scaling" |
| Z2-R-UI-145 | Accessibility audit | S4 | Zenith2/Docs/Design/UI/Accessibility.md → "Accessibility validation" |
| Z2-R-UI-146 | Screen-reader flow | S4 | Zenith2/Docs/Design/UI/Accessibility.md → "End-to-end accessible flows"; Zenith2/Docs/Design/PLAT/Accessibility.md → "Zenith_AccessibilityService" |
| Z2-R-UI-147 | Remapped-controls flow | S4 | Zenith2/Docs/Design/UI/Accessibility.md → "End-to-end accessible flows" |
| Z2-R-UI-148 | Reduced-motion flow | S4 | Zenith2/Docs/Design/UI/Accessibility.md → "End-to-end accessible flows" |
| Z2-R-UI-149 | Colour-blind flow | S4 | Zenith2/Docs/Design/UI/Accessibility.md → "End-to-end accessible flows" |
| Z2-R-UI-150 | Flow routes are text | S4 | Zenith2/Docs/Design/UI/Accessibility.md → "End-to-end accessible flows" |
| Z2-R-UI-151 | Editor model | S4 | Zenith2/Docs/Design/UI.md → "Editor surfaces"; Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIEditorModel header" |
| Z2-R-UI-152 | Preview context | S4 | Zenith2/Docs/Design/UI.md → "Editor surfaces"; Zenith2/Docs/Design/UI/PublicAPI.md → "Zenith_UIContext header"; Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIEditorModel header" |
| Z2-R-UI-153 | Widget and property catalogues | S4 | Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIEditorModel header"; Zenith2/Docs/Design/UI/Documents.md → "Style property catalogue" |
| Z2-R-UI-154 | Counters and zones | S3 | Zenith2/Docs/Design/UI.md → "Counters"; Zenith2/Docs/Design/UI.md → "Profiler zones" |
| Z2-R-UI-155 | Debug views and frame dump | S3 | Zenith2/Docs/Design/UI.md → "Debug views"; Zenith2/Docs/Design/UI.md → "Frame-dump entries" |
| Z2-R-UI-156 | Failure table behaviours | S3 | Zenith2/Docs/Design/UI.md → "Failure table" |
| Z2-R-UI-157 | Per-system budget ticket | S1 | Zenith2/Docs/Design/UI.md → "Rows owned"; Zenith2/Docs/Design/UI.md → "Pending row needs"; Zenith2/Docs/Design/UI.md → "Content specifications and quality matrix rows" |
| Z2-R-UI-158 | UI benchmark harness | S2 | Zenith2/Docs/Design/UI/Tests.md → "Benchmarks and harnesses"; Zenith2/Docs/Design/UI.md → "Selection decisions UI owns" |
| Z2-R-UI-159 | W-UI2D workload scale | S1 | Zenith2/Docs/Design/UI.md → "Stage table" |
| Z2-R-UI-160 | W-UI2D measured at Stage 4 | S4 | Zenith2/Docs/Design/UI/Tests.md → "Acceptance runs"; Zenith2/Docs/Design/UI.md → "Rows owned" |
| Z2-R-UI-161 | Input-to-photon contribution | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Frame flow" |
| Z2-R-UI-162 | Sustained mobile run | S6 | Zenith2/Docs/Design/UI.md → "Stage table" |
| Z2-R-UI-163 | Binding validation rules | S3 | Zenith2/Docs/Design/UI/Documents.md → "Validation rules" |
| Z2-R-UI-164 | Navigation and accessibility validation rules | S3 | Zenith2/Docs/Design/UI/Documents.md → "Validation rules"; Zenith2/Docs/Design/UI/Accessibility.md → "Accessibility validation" |
| Z2-R-UI-165 | Theme validation rules | S3 | Zenith2/Docs/Design/UI/Documents.md → "Validation rules"; Zenith2/Docs/Design/UI/Documents.md → "Theme document schema" |
| Z2-R-UI-166 | Font family validation rules | S3 | Zenith2/Docs/Design/UI/Documents.md → "Validation rules"; Zenith2/Docs/Design/UI/Documents.md → "Font family document schema" |
| Z2-R-UI-167 | Text stack selection harness | S2 | Zenith2/Docs/Design/UI.md → "Selection decisions UI owns"; Zenith2/Docs/Design/UI/Tests.md → "Shaping golden suite" |
| Z2-R-UI-168 | Runtime selection harness | S2 | Zenith2/Docs/Design/UI.md → "Selection decisions UI owns"; Zenith2/Docs/Design/UI/Layout.md → "Runtime selection"; Zenith2/Docs/Design/UI/Layout.md → "Layout conformance cases" |
| Z2-R-UI-169 | W-UI2D measured at Stage 3 | S3 | Zenith2/Docs/Design/UI/Tests.md → "Acceptance runs"; Zenith2/Docs/Design/UI.md → "Rows owned" |
| Z2-R-UI-170 | Pause-menu replay | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Pause menus"; Zenith2/Docs/Design/JOBS.md → "Determinism obligations" |
| Z2-R-UI-171 | Quality matrix rows | S1 | Zenith2/Docs/Design/UI.md → "Content specifications and quality matrix rows" |
| Z2-R-UI-172 | Accessibility hand-off buffers | S4 | Zenith2/Docs/Design/UI/Accessibility.md → "Narration composition"; Zenith2/Docs/Design/UI/Accessibility.md → "Platform accessibility bridge" |
| Z2-R-UI-173 | SA break offsets recorded before the text ruling | S2 | Zenith2/Docs/Design/UI/Text.md → "Text conformance corpus"; Zenith2/Docs/Design/UI/Tests.md → "Shaping golden suite"; Zenith2/Docs/Design/UI.md → "Ticket slices and ordering" |
| Z2-R-UI-174 | Resume capacity reserved | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Publication path"; Zenith2/Docs/Design/UI/Interaction.md → "Pause menus"; Zenith2/Docs/Design/UI.md → "Configuration keys" |
| Z2-R-UI-175 | Live input allocates nothing under replay | S3 | Zenith2/Docs/Design/UI/Interaction.md → "Publication path"; Zenith2/Docs/Design/UI/Interaction.md → "Pause menus" |
| Z2-R-UI-176 | Stage 2 pull-forward | S2 | Zenith2/Docs/Design/UI.md → "Stage table"; Zenith2/Docs/Design/UI.md → "Ticket slices and ordering"; Zenith2/Docs/Design/UI/Tests.md → "Benchmarks and harnesses" |
| Z2-R-UI-177 | Stage 4 benchmarks | S4 | Zenith2/Docs/Design/UI/Tests.md → "Benchmarks and harnesses"; Zenith2/Docs/Design/UI.md → "Ticket slices and ordering" |
| Z2-R-UI-178 | Shared style resolution for both runtime candidates | S3 | Zenith2/Docs/Design/UI/Layout.md → "What both candidates implement"; Zenith2/Docs/Design/UI/Layout.md → "The RmlUi integration"; Zenith2/Docs/Design/UI/Documents.md → "UI document sections" |
| Z2-R-UI-179 | SA break-offset record case | S2 | Zenith2/Docs/Design/UI/Text.md → "Corpus file format"; Zenith2/Docs/Design/UI/Tests.md → "Shaping golden suite"; Zenith2/Docs/Design/UI.md → "ZenithCook UI files" |
| Z2-R-UI-180 | HDR10 and scRGB UI output | S5 | Zenith2/Docs/Design/UI/Rendering.md → "Colour and HDR output" |
| Z2-R-UI-181 | Message argument validation rule | S4 | Zenith2/Docs/Design/UI/Documents.md → "Validation rules" |
| Z2-R-UI-182 | Localization cook steps registered | S4 | Zenith2/Docs/Design/UI.md → "Cook steps and derived-data keys" |

## Cross-area reconciliation

Rows UI1 to UI24 of `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`, and the cross-doc items UI needs (`C:/dev/Zenith2-drafts/reviews/CROSS_DOC_ITEMS.md`):

| Row | To | Need |
|---|---|---|
| UI1 | MOD, SCRIPT, ARCH | UI's needs against the single module publication sink, in the shape its peers assume (`Zenith_WorldAttachContext::GetPublicationSink()` → `Zenith_ModulePublicationSink&`, `Park(id, sourceKind, publicationKind, payload)` returning `bool`), plus UI's extras: (a) the source declaration with in-flight and size limits and a ZenithRuntime-declared handler; (b) `AllocateCompletionID` on the main thread in the prologue; (c) `GetOutstandingCount`, `IsReplaying`, `GetNextTick`; (d) `PeekRecorded` for the pause rule; (e) under `--replay`, allocation and parking both refused for this RECORDED kind (UI never calls either for live input; the refusal is a backstop); (f) the replaying table's adoption of the recorded next completion ID at each tick boundary, X278 (Interaction.md "Publication path") |
| UI2 | SCRIPT | closed: answered by `Zenith2/Docs/Scripting.md → "Asynchronous source kind registry"` (`ASYNC_SOURCE_UI`), and X236 (1), applied, adds UI to [Z2-N-SCRIPT-343]'s list of C++-consumed kinds |
| UI3 | MOD, PLAT | a readable input-world flag so a module knows which world receives input |
| UI4 | MOD, CINE | a caption cue event type declared in ZenithRuntime for C++ caption sources (cinematics); AUDIO's captions use the script route of AU16, adopted |
| UI5 | PLAT (and MOD's Gradle template) | answered by PLAT's post-signoff patch, `Zenith2/Docs/Design/PLAT/Accessibility.md`: `Zenith_Platform::GetAccessibility()`, `Zenith_AccessibilityService`, its value types, `Zenith_AccessibilityTreeProvider` (called only on the main thread inside `SetTreeProvider` and `NotifyTreeChanged`, its tree copied into PLAT's snapshot), the tree validation rules V1 to V9, the `platform.accessibility.*` preference keys UI reads for its `ui.access.*` defaults, and the recording backend (`platform.accessibility.backend=recording`) with `Zenith_AccessibilityTestAccess`; UI revision 2 conforms to PLAT's row P21, delta by delta (Accessibility.md "Platform accessibility bridge", "Accessibility settings", "Semantic tree"); UI's bridge and flow tickets are blocked by PLAT's API slice Z2T-PLAT-103 and its core and recording backend Z2T-PLAT-104, and the preference case of `Test_UIA11yBridge_PublishesWithinPlatformLimits` by Z2T-PLAT-105; the bridge publishes under one module-wide revision, never a context's, because PLAT's revision rules run per provider registration (Accessibility.md "Platform accessibility bridge") |
| UI6 | PLAT | claimant registration by the module (PLAT's text says the composition root registers it) |
| UI7 | RPROD, ARCH | (a) answered: `Flux_Renderer::EnableUI` is RPROD's declaration, UI writes its body (RPROD's "add your own Create step"); (b) `Flux.BuildGraph` calls `PrepareFrame` in step 4 and `WriteFrame` in step 11; (c) ownership of ZenithFlux's UI files (X236 (6)) |
| UI8 | REND, RPROD | (a) answered: `Flux_FrameShared::m_pxUIFrame` ([Z2-N-REND-995]); (b) `Flux_UIDrawParams` gains `m_uOutputMode`, `m_fNitsForOne` (X236 (3), applied); (c) a `UIWorldSpace` per-pass table with the view's depth comparison (X236 (4), applied: `Zenith2/Docs/Rendering/PerPass/Common.md → "UIWorldSpace"`); (d) the view's source world tag on `Flux_ViewContext`; (e) world-space UI at `FLUX_EXTENSION_POINT_TRANSPARENT` |
| UI9 | SHIP, ARCH, ASSET | the third-party manifest integration mode `data` and the `thirdparty:` content root (X236 (9)) |
| UI10 | ARCH (Contracts) | the font family and localization project documents are UI's authoring documents in the contract lists (X236 (8), applied: [Z2-N-CONTRACT-552]) |
| UI11 | CONTENT | the import preset `ui.sprite` |
| UI12 | ASSET | the registered document types row of its migration table |
| UI13 | ARCH | parser-limit rows for UI's cooked formats (X236 (7), applied: `Zenith2/Docs/Architecture.md → "Parser limits"`) |
| UI14 | ECS | an optional "localized" flag on string-key fields of data-defined components |
| UI15 | EDIT | what UI provides for the authoring tool, including `Zenith_UIContext::OpenScreenFromBytes` for the preview |
| UI16 | TWOD, DEVT | corrected per DEVT's DV21: no text API crosses layers; DEVT's overlay is drawn by RPROD's epilogue after the OVERLAY point, not as an extension-point pass |
| UI17 | ASSET | cook command-line verbs `--export-xliff`, `--import-xliff` registered by UI's steps; the Noto shaping goldens are recorded with `hb-shape` and the SA break offsets with the committed record case of `ZenithCook_Tests`, so no recording is a cook verb or a test-executable mode of `ZenithUI_Tests` |
| UI18 | Budgets | the four pending rows, filed as X237 (the render-test tolerance row withdrawn by UI revision 2) |
| UI19 | GAME, MOD | GA13 adopted: UI clears `WORLD_PAUSE_REASON_MENU` through the `Zenith_WorldControl` of `GetWorldControl()` (GA5 (a)) from a `resumeWorld` presentation action |
| UI20 | AUDIO | AU16 adopted: `ui.locale` is UI's STRING key; captions take the script route through `UI.ShowCaption`; AUDIO's dialogue cook step contributes subtitle keys through `Zenith_UIExtractionSink` |
| UI21 | ASSET, LIGHT, PLAT | acknowledgements of AS26, L22 and P15; no change asked |
| UI22 | GAME | the replay rule for `MENU`: GAME's "Pause arbitration" says a replay needs no pause record except that a `MENU` clear is replayed from UI's recorded resume publication (Interaction.md "Pause menus") |
| UI23 | DEVT, MOD | `Zenith_ModuleStartContext::GetDevFrameDump()` so a module registers a frame-dump section |
| UI24 | GAME | the slice route files `Zenith2/Tests/UIAccessibility/Routes/<Slice>.route.json` for the accessible flows, written by GAME's slice tickets |
| ED43 | EDIT (UI answers) | moved: `Zenith_RegisterUIDocumentTypes` and `Zenith_GetUICanonicalSchema` live in `ZenithAuthoring` (`Zenith2/Tools/Authoring/Internal/UI/`, root header `Zenith_AuthoringUI.h`; "ZenithAuthoring UI files") |

| Cross-doc item | Target | What UI needs |
|---|---|---|
| X236 | Scripting.md, MisuseCatalogue.md, Rendering/PerPass/Common.md, Rendering.md, Architecture.md, Contracts.md, Architecture/BuildSystem.md | (1) UI in [Z2-N-SCRIPT-343]; (2) UI's block ZS2100–ZS2199 with codes ZS2101–ZS2106; (3)–(9) as filed; (10) `ZenithVertexFormat` and `ZenithRHI` in the `ZenithUI` row of "Library dependency graph" ("Libraries UI code depends on"). Applied: (2) earlier, and (1), (3), (4), (5), (7), (8) and (10) by core-doc patch e; open: (6) and (9) |
| X237 | Budgets.md | the four pending rows of "Pending row needs" (UI revision 2 withdrew the render-test tolerance row: the bound is derived) |
| X251 | DecisionLog.md | the D-SEL-UIRT sharpening (shared shell, neutral acceptance, the selection set of CSS and ZenithUI-semantics cases; shell cases gate only as invariants relative to each candidate's own layout while the decision is open, their absolute expectations reported as class `shell`; RmlUi's style engine cascades the layout-affecting properties, the shell's resolver every property for both) and, for both entries, the conjunctive order of comparison with its no-candidate clause, as "Selection decisions UI owns" states them |

## Assumed interfaces pending other areas

Every interface another area does not yet provide is fenced where it is used, with its row:

| Assumption | Where it is fenced |
|---|---|
| the module publication sink (UI1) and the world control accessor (UI19) | `Zenith2/Docs/Design/UI/Interaction.md → "Pause menus"` |
| the `ZenithAuthoring` library rows (ED43, X257) | "ZenithAuthoring UI files" above |
| the input-world accessor (UI3) and module claimant registration (UI6) | `Zenith2/Docs/Design/UI/Interaction.md → "Input claims"` |
| RPROD's step 4 and 11 calls and the view's source world tag (UI7 (b), UI8 (d)) | `Zenith2/Docs/Design/UI/Rendering.md → "Flux UI renderer"`, `Zenith2/Docs/Design/UI/ServicesAPI.md → "Flux_UI header"` |
| the frame-dump accessor (UI23) | "Frame-dump entries" above |
| the caption cue event (UI4) | below |

<!-- assumption: pending MOD, CINE (UI4) -->
A ZenithRuntime event type `Zenith.Runtime.CaptionCueEvent` carrying a `Zenith_UICaptionCue`-shaped payload (key, speaker key, kind, priority, speaker colour, duration), emitted in phases 2–8 by C++ caption sources such as cinematic playback, so it is delivered in phase 9 of the same tick to `UI.Captions@W<tag>`; AUDIO uses the script route instead (AU16).
<!-- /assumption -->

<!-- assumption: pending ECS (UI14) -->
Data-defined component schemas (`*.schema.json`, Z2-D-020) accept an optional `"localized": true` on string-key fields, reflected so the cook's extraction reads those values as localization keys (Localization.md "Extraction sources").
<!-- /assumption -->
