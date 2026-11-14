# UI design: test catalogue

Section file of `Zenith2/Docs/Design/UI.md`. Every UI test by its ID (`Test_<Type>_<Behaviour>`, doctest name `"<Library>/<Type>/<Behaviour>"`, `Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"`), the file it lives in, what it asserts and the gate step that runs it. Tests are pass or fail only; no count is pinned ([Z2-N-CONTRACT-408]). GPU tests create a device directly on WARP and lavapipe with validation clean ([Z2-N-REND-034], [Z2-N-ARCH-793]); a test that needs only those software devices is not `needs-gpu`. Headless tests run with no renderer at all ([Z2-N-ARCH-785]; Rendering.md "Headless and device loss"). No test compares against a committed binary artefact: fonts, atlases and images are generated in the test, and the only recorded expectations are the SA break offsets and the Noto shaping goldens, recorded with independent tools (ICU4C, `hb-shape`) by two `needs-human` recording tickets, the first before the D-SEL-TEXT ruling and the second after it (Text.md "Text conformance corpus"). No test substitutes a double for a concrete class: tests drive the real `Zenith_UISystem`, `Zenith_UIContext`, `Zenith_Localization`, `Zenith_GlyphCache`, `Flux_UploadRing` and `Flux_UIRenderer` with in-memory inputs, PLAT's accessibility service is its recording implementation (a real implementation selected by configuration), and fault paths use the fault injector of `Zenith_UISystemDesc`. Every test name below is one registered CTest ([Z2-N-ARCH-179]); a case that iterates documents, modes or groups uses doctest `SUBCASE`s inside one named test, never a name pattern.

## Scope of this file

| Group | Heading |
|---|---|
| where tests live | "Test targets and files" |
| inputs every test uses | "Fixtures" |
| unit tests per subsystem | "Unit tests" |
| layout expectations | "Layout golden suite" |
| text expectations | "Shaping golden suite" |
| GPU | "Render tests" |
| module, worlds, scripts, replays | "Integration tests" |
| parsers | "Fuzz targets" |
| performance | "Benchmarks and harnesses" |
| stage acceptance | "Acceptance runs" |
| which step runs what | "Gate steps and labels" |

## Test targets and files

| Target | Sources | Links | Runs in |
|---|---|---|---|
| `ZenithUI_Tests` | `Zenith2/Engine/ZenithUI/Tests/**` | ZenithUI and its declared dependencies, `ZenithTestSupport` | `Test` in the `win` and `win-asan` presets (the module is `ZENITH_GPU`, so not `linux-tsan`); no renderer: every case is headless |
| `ZenithFlux_Tests` (UI's files) | `Zenith2/Engine/ZenithFlux/Tests/UI/**` | as RPROD's | `Test` on WARP and lavapipe |
| `ZenithCook_Tests` (UI's files) | `Zenith2/Tools/ZenithCook/Tests/UI/**` | ZenithCook's object library; the usage-tests manifest entry `icu4c` through `TEST_THIRD_PARTY`, for the SA break record case and its converter test alone (Text.md "Corpus file format") | `Test` on `win-debug` |
| `ZenithAuthoring_Tests` (UI's files) | `Zenith2/Tools/Authoring/Tests/UI/**` | the tool library `ZenithAuthoring` (`Zenith2/Docs/Design/UI.md → "ZenithAuthoring UI files"`) | `Test` on `win-debug` |
| `Zenith_Integration_UI` | `Zenith2/Tests/UI/**` (the accessible-flow suite under `Zenith2/Tests/UIAccessibility/` is part of it; the GPU cases under `Zenith2/Tests/UI/Render/`) | ZenithRuntime, the UI module, ZenithScript | `Test` (headless runtime: no Flux, PLAT's recording accessibility service); a case marked WARP or lavapipe creates an RHI device directly on that software device ([Z2-N-ARCH-793]), a `Flux_Renderer` over it and the UI module with that renderer, because it needs ZenithUI's glyph cache or text stack and a renderer together, which neither `ZenithUI_Tests` (no renderer) nor `ZenithFlux_Tests` (ZenithFlux sees no module) can link |
| fuzz executables | `Zenith2/Engine/ZenithUI/Tests/Fuzz/*.cpp`, `Zenith2/Tools/ZenithCook/Tests/UI/Fuzz/*.cpp` | libFuzzer | `win-asan` smoke; full duration at stage acceptance ([Z2-N-ARCH-952]) |
| benchmarks | `Zenith2/Benchmarks/UI/` (`Zenith_Bench_UI`, `zenith_add_benchmark`, preset set `ZENITH_GPU`) | ZenithUI, ZenithRuntime | its `--smoke` CTest in `Test`; full runs by the `needs-human` ruling tickets and stage acceptance |

## Fixtures

| Fixture | Form |
|---|---|
| the test font | built in memory by the header-only `Zenith_BuildUITestFont` (`Zenith2/Engine/ZenithUI/Tests/Support/Zenith_UITestFontBuilder.h`, [Z2-N-ARCH-255]): a TrueType font with 1,000 units per em, ascent 800, descent 200, line gap 0; every spacing glyph has advance 500 and the combining marks U+0300 and U+0301 have advance 0 (mark glyphs, GDEF class 3); glyphs are boxes (an outer square contour of 100–400 × 0–700 with an inner counter) for printable ASCII, U+00A0, U+2026, U+05D0–U+05EA (Hebrew letters, strong RTL), U+05B4 (a Hebrew point, advance 0), U+0300, U+0301 and `.notdef`, plus glyph 200 `fi`; its only layout tables are a GSUB `liga` ligature `f` `i` → 200, a GPOS `kern` pair `A` `V` of −100 units and a GPOS `mark` attachment of U+0301 to `e` (the base anchor on `e` at (250, 700) font units and the mark anchor on U+0301 at (250, 700), so the shaped mark has offsets (−500, 0) and advance 0, Text.md "Text conformance corpus" item 1), so every advance, offset, line height and break position is derivable by hand (Text.md "Text conformance corpus") |
| the test font family | `Zenith_BuildUITestFontFamily`: a family document naming the test font at weight 400 and a second copy at weight 700 |
| UI documents | committed hand-written text under `Zenith2/Engine/ZenithUI/Tests/Fixtures/` (`Layout/`, `Interaction/`, `Accessibility/`, `Themes/`) |
| localization | committed text: `Zenith2/Engine/ZenithUI/Tests/Fixtures/Localization/` (a project with `en-GB`, `fr-FR`, `ar`, `ja-JP`, `ru-RU`, `pl-PL` and their tables, messages covering every opcode) |
| text corpus | committed text: `Zenith2/Engine/ZenithUI/Tests/Fixtures/Text/Corpus/` (Text.md "Text conformance corpus") |
| corpus fonts | the third-party data entry `noto-fonts` (row UI9), fetched by the dependency setup, never committed |
| Unicode and CLDR data | the third-party data entries `unicode-ucd` and `cldr-json` (row UI9) |
| migration fixtures | `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/UI/<Type>/v1.json` and `v1.expected.json` for the five UI document types ([Z2-N-CONTRACT-548]; `<Type>` the stable name's last part: `UIDocument`, `ThemeDocument`, `FontFamilyDocument`, `LocalizationProjectDocument`, `StringTableDocument`) |
| cooked fixture set | the CTest setup entry `UI_CookedFixtures` (`ZenithCook_Tests --test-suite=z2-fixture-setup --test-case=Fixture_UI_Generate`, `FIXTURES_SETUP UICookedFixtures`, file `Zenith2/Tools/ZenithCook/Tests/UI/Zenith_UIFixtures_Generate.cpp`) writes a fixture game `UIFixture` under `${ZENITH_OUT}/test-fixtures/UI/Games/UIFixture/` (a full `game.json` selecting `UI` with content roots `Assets`, `Localization` and `Scenes`, the Blank template's start scene, the test font as face files, the fixture documents, the localization fixture project in `Localization/`), runs ZenithCook's assign mode over it (`--game-root ${ZENITH_OUT}/test-fixtures/UI/Games --assign-guids`), failing if that run writes anything under ZenithUI's engine content root, and cooks it in process into `${ZENITH_OUT}/test-fixtures/UI/Cooked/`; the root is named by `ZENITH2_UI_FIXTURES` ([Z2-N-ARCH-257]); the doctest suite `z2-ui-cooked` requires it in three entries, `ZenithCook_Tests[z2-ui-cooked]`, `ZenithUI_Tests[z2-ui-cooked]` and `Zenith_Integration_UI[z2-ui-cooked]`; nothing generated is committed |
| W-UI2D cooked fixture set | the setup entry `UIWORKLOAD_CookedFixtures` (`ZenithCook_Tests --test-suite=z2-fixture-setup --test-case=Fixture_UIWorkloadUI2D_Cook`, `FIXTURES_SETUP UIWORKLOADCookedFixtures`, file `Zenith2/Tools/ZenithCook/Tests/UI/Zenith_UIWorkloadFixture_Cook.cpp`) build-cooks the tracked `Zenith2/Games/WorkloadUI2D/` (its sources carry committed `.meta` files) into `${ZENITH_OUT}/test-fixtures/UIWORKLOAD/Cooked/`, named by `ZENITH2_UI_WORKLOAD_FIXTURES`; the suite `z2-uiworkload-cooked` of `Zenith_Integration_UI` requires it; a separate area token, so neither set's setup empties the other's directory |

## Unit tests

### Types events and components

| Test ID | File | Asserts |
|---|---|---|
| `Test_UIPublication_RoundTripsEveryKind` | `Tests/Zenith_UIEvents_Tests.cpp` | encode then decode of each of the seven publication kinds (the six event kinds and `UI_EVENT_KIND_WORLD_RESUMED`) with maximal names and text gives equal fields; the payload size is 40 + E + V + T |
| `Test_UIPublication_RejectsIllFormedText` | same | a text with an unpaired surrogate encoding (ED A0 80), an overlong form or a truncated sequence is refused at encode, the event dropped and `ui.events.text_rejected` counted |
| `Test_UIPublication_RejectsMalformed` | same | truncated payloads, lengths beyond the payload, E or V above 64, T above 256, a nonzero reserved field and an unknown kind each decode to an error |
| `Test_UIPublication_TextTruncatesAtGrapheme` | same | a 300-byte text with a multi-code-point grapheme across byte 256 truncates before that grapheme |
| `Test_UIScreenComponent_PropertyIDsStable` | `Tests/Zenith_UIComponents_Tests.cpp` | the registered property IDs 1–10 and type name `Zenith.UI.Screen` are unchanged |
| `Test_UICaptionQueue_EvictsLowestPriorityOldest` | same | nine cues into a capacity-8 queue evict as Accessibility.md "Caption cues" states; expiry at start + duration |
| `Test_UIError_SixteenBytes` | `Tests/Zenith_UITypes_Tests.cpp` | `sizeof(Zenith_UIError)` and every error code's name |

### Documents and cooking

| Test ID | File | Asserts |
|---|---|---|
| `Test_UIDocumentModel_ValidatesEveryRule` | `Tests/Zenith_UIDocument_Tests.cpp` | one minimal violating fixture per rule of Documents.md "Validation rules" reports exactly that rule with its severity |
| `Test_UIDocumentCompile_SectionsMatchFormat` | same | compiling a fixture writes the sections, the header record (including `safeArea`, `focusScope` and `cancelAction` at offsets 21–23), element records (64 bytes), style blocks, binding records (48 bytes), a TYPEDATA record of every element type with its documented size and field offsets (the button's `presentationAction` among them) and the EVENTS entries of Documents.md "Cooked UI document format" at their offsets, and, while D-SEL-UIRT is open, the RMLUI section (kind 15); a style block holding `zIndex`, `layoutDirection` and `mirrorInRtl` is written with the IDs 30, 49 and 84 (`UI_STYLE_Z_INDEX`, `UI_STYLE_LAYOUT_DIRECTION`, `UI_STYLE_MIRROR_IN_RTL`) and loads back without `UI_ERROR_DOCUMENT_CORRUPT` |
| `Test_UIDocumentCompile_Deterministic` | same | compiling twice, and with shuffled object member order, gives identical bytes |
| `Test_UIDocumentCompile_FlattensInstances` | same | an instance with overrides at depth 3 compiles to the flattened tree with overrides applied |
| `Test_UIDocumentLoad_RejectsCorrupt` | same | each section truncated, an out-of-range index, a TYPEDATA record past the section, a header byte outside its values, a bad check value → `UI_ERROR_DOCUMENT_CORRUPT`, no crash; the screen stays unopened and `ui.screen.document_unavailable` counts |
| `Test_UIDocumentLoad_MissingFontSubstitutes` | same (`z2-ui-cooked`) | a document whose dependent font asset fails to load renders its text in the engine default family (the kind's SUBSTITUTE fallback) and opens normally |
| `Test_UIDocumentLoad_HotReloadRepairsScreen` | same (`z2-ui-cooked`) | a screen whose document was corrupt opens at the next extraction after a hot reload delivers a valid document, with focus restored by element uid |
| `Test_UIDocumentMigration_Fixtures` | `Zenith2/Tools/Authoring/Tests/UI/Zenith_UIMigration_Tests.cpp` (`ZenithAuthoring_Tests`) | each `v1.json` migrates to `v1.expected.json` (identity) and preserves unknown members |
| `Test_UICookSteps_RegisterOnce` | `ZenithCook/Tests/UI/Zenith_UICookSteps_Tests.cpp` | `ui.document`, `ui.theme`, `ui.font`, `ui.localization`, `ui.extract`, `ui.manifest` register; registering twice is an error |
| `Test_UIBindingManifest_ListsEveryBoundProperty` | same (`z2-ui-cooked`) | the manifest of the fixture game lists exactly the bound (type, property) pairs, sorted, with a valid check |
| `Test_FontBake_BakedEqualsDynamic` | `Tests/Zenith_UIFontBake_Tests.cpp` | a glyph baked into a static page and the same glyph generated dynamically have identical MTSDF texels |
| `Test_FontBake_PageFormatAndCheck` | same | the page payload header (magic `ZMSD`, version 1, sizes, check) and the face and glyph records match Documents.md "Cooked font format" |
| `Test_FontBake_PageLimit` | same | a code-point set that overflows `maxPages` fails with `FONT_ERROR_PAGE_LIMIT` |

### Style and layout

| Test ID | File | Asserts |
|---|---|---|
| `Test_UIStyle_CascadeOrder` | `Tests/Zenith_UIStyle_Tests.cpp` | each of the nine cascade steps of Layout.md "Cascade order" overrides the one before, in a fixture with one property set at every level; step 9 resolves `layoutDirection` from the parent and the locale into `m_eLayoutDirection` (never `UI_LAYOUT_DIRECTION_AUTO`); `zIndex` and `mirrorInRtl` set in a class and overridden inline reach `m_iZIndex` and `m_bMirrorInRtl`; one property of every group of the catalogue, set in a class and overridden by a state block, reads back from its `Zenith_UIComputedStyle` field (`width` → `m_xWidth` as a `Zenith_UILength` of unit `UI_LENGTH_UNIT_PERCENT`, `grow` → `m_fGrow`, `pivot` → `m_xPivot`, `gridColumnSpan` → `m_uGridColumnSpan`, `justify` → `m_eJustify`, `gridColumns` → `m_xGridColumns`'s track count, `backgroundGradient` → `m_xBackgroundGradient`, `pointerEvents` → `m_ePointerEvents`, `textOverflow` → `m_eTextOverflow`, `transition` → `m_xTransition`'s duration); the resolver is shell code, so the test runs once, whichever candidates are compiled |
| `Test_UIRmlUiSection_RcssCascadeOrder` | `Tests/Zenith_UIRmlUi_Tests.cpp` (the RmlUi candidate's tests: deleted with it by a ruling for the custom runtime) | for a fixture whose element 3 has classes `b a`, element 4 classes `a b`, both with inline blocks and `hover` and `disabled` state blocks, the RMLUI section's RCSS holds, in this order, one composite class rule per distinct class sequence (`.k0` for `b a`, `.k1` for `a b`, each the cook-time merge of its classes' base blocks in class order), one `.e<index>` rule per element with an inline block, then for each state in the fixed priority order the composite rules' state blocks and the elements' inline state blocks, under the pseudo-class names `z-focused` … `z-disabled`; it contains no `#` selector and no `style` attribute, and no layout-affecting property whose value is a metric token or a binding |
| `Test_UIStyle_InheritedProperties` | same | inherited properties (font, colour, direction) flow to children; non-inherited do not |
| `Test_UIStyle_StateVariants` | same | hover, pressed, focusVisible, disabled and checked variants apply in the documented precedence |
| `Test_UIStyle_PaletteSwitchIsTableSwap` | same | switching palette changes resolved colours with zero style-block reparses (`Zenith_UIContextStats`) |
| `Test_UILayout_Complexity_Linear` | `Tests/Zenith_UILayout_Tests.cpp` | element-visit counts for trees of 1k, 10k, 60k elements grow linearly (ratio within 1.1 of the element ratio) |
| `Test_UILayout_IncrementalTouchesBoundaryOnly` | same | a text change under a fixed-size boundary lays out only that subtree |
| `Test_UILayout_ElementLimit` | same | a context beyond `m_uMaxElementsPerContext` refuses the screen with `UI_ERROR_ELEMENT_LIMIT` |
| `Test_UIContext_OpenScreenOutOfMemory` | `Tests/Zenith_UIContext_Tests.cpp` | with the fault injector failing the context heap's allocation, opening a screen fails with `UI_ERROR_OUT_OF_MEMORY` and the screens already open draw unchanged |
| `Test_UIContext_OpenScreenFromBytesValidates` | same | `OpenScreenFromBytes` opens a preview screen from compiled bytes, and refuses corrupt bytes with `UI_ERROR_DOCUMENT_CORRUPT` exactly as the loader does |
| `Test_UIContext_ConcurrentWorldsExtract` | same | two worlds' extractors run concurrently on 8 workers for 1,000 frames; each world's draw lists and trees equal those of a serial run, and the lock-order checker reports no violation (the multi-world concurrency evidence for ZenithUI; no race detector covers a `ZENITH_GPU` library) |
| `Test_UIVirtualList_FenwickMatchesLinearSum` | `Tests/Zenith_UIVirtualization_Tests.cpp` | random extent updates keep prefix sums equal to a linear reference |
| `Test_UIVirtualList_MaterialisesWindowOnly` | same | a 1,000,000-item list materialises viewport + 2 × overscan items at any offset; a LIST of 1,048,577 items is clamped to `uUI_LIST_MAX_ITEMS` and counts `ui.list.items_clamped` |
| `Test_UIVirtualList_RecyclingPreservesFocusIndex` | same | focus on a recycled item stays on the list with its index |
| `Test_UIBinding_ConvertsEveryKind` | `Tests/Zenith_UIBinding_Tests.cpp` | every value kind into every compatible element member; incompatible pairs were rejected at compile time (rule 41) |
| `Test_UIBinding_CommittedValueHoldsThirtyFrames` | same | a committed slider value shows until the bound value equals it or 30 frames pass |
| `Test_UIBinding_SingletonPicksLowestStableID` | same | `entity: "singleton"` reads the entity with the lowest stable simulation ID among three carrying the component, and follows when that entity is destroyed |
| `Test_UIBinding_SingletonDuplicateIDAsserts` | same (`z2-death-assert`) | two entities reporting one stable simulation ID, which ECS makes impossible, trip the binding evaluator's `ZENITH_ASSERT` rather than picking either |
| `Test_UIAnimation_KeysAndEasing` | `Tests/Zenith_UIAnimation_Tests.cpp` | sampled values of every easing at t = 0, 0.25, 0.5, 0.75, 1 |
| `Test_UIAnimation_ReducedMotionSnapsTransforms` | same | with reduced motion, transform transitions apply at once, non-essential animations show the final key, essential ones fade opacity alone |
| `Test_UIWorldPanel_CanvasToWorld` | `Tests/Zenith_UIWorldSpace_Tests.cpp` | panel corners map to world points; ray hit testing returns canvas coordinates within 0.01 units |

### Text and fonts

| Test ID | File | Asserts |
|---|---|---|
| `Test_UIUnicode_TablesMatchSamples` | `Tests/Zenith_UIUnicode_Tests.cpp` | the generated tables' values for 200 sampled code points per property and every range boundary equal the pinned UCD files, parsed at test time by the test's own reader (Text.md "Unicode data") |
| `Test_UIUnicode_TablesCoverEveryPlane` | same | every code point U+0000–U+10FFFF has a value for every property |
| `Test_UIUtf8_DecodeReplacesInvalid` | `Tests/Zenith_UIText_Tests.cpp` | `Zenith_UIRepairUtf8` turns overlong forms, surrogates, truncated sequences and bytes above F4 into U+FFFD, one per maximal invalid subpart, and counts them; a bound string with such bytes displays U+FFFD; `Layout` called directly with unrepaired text returns `TEXT_LAYOUT_ERROR_BAD_UTF8` |
| `Test_UITextLayout_TestFontAdvances` | same | a string in the test font at 20 px has width 10 px × code points; line height 20 px |
| `Test_UITextLayout_WrapAndEllipsis` | same | the Layout.md text cases at the paragraph level, including `…` substitution |
| `Test_UITextLayout_ParagraphLimit` | same | a paragraph above `uTEXT_MAX_PARAGRAPH_BYTES` fails with its error, without truncation |
| `Test_UICaret_HitTestRoundTrip` | `Tests/Zenith_UICaret_Tests.cpp` | for every caret position of LTR, RTL and mixed paragraphs, the caret rectangle's hit test returns that position; logical and visual movement per Text.md "Caret and selection"; the mixed case is an LTR sentence holding a Hebrew word whose second cluster is two glyphs (a letter and U+05B4): its glyphs appear once in visual order (no double reversal), its cluster map and caret x positions equal the hand-written values; the RTL case with a bold middle word (three level-1 shaping runs) shows the runs in reversed logical order with each run's glyphs in the shaper's order, and its caret positions round-trip across both run boundaries; the L1 case of `Bidi.json` (an LTR paragraph whose embedded RTL run ends in two spaces and a tab that L1 resets to level 0) shows those three glyphs after the Hebrew letters in logical order, with their cluster map and caret positions rebuilt for that order (Text.md "Line layout and reordering" step 10.1) |
| `Test_UITextEdit_Operations` | `Tests/Zenith_UITextEdit_Tests.cpp` | every operation of Text.md "Text editing": insert, delete by grapheme, by code point in a Brahmic script and by word, selection replace, composition pre-edit and commit, clipboard truncation and the byte capacity; Ctrl+Z is not consumed |
| `Test_UIFontStack_FallbackOrder` | `Tests/Zenith_UIFont_Tests.cpp` | the stack order for locale, script fallback and engine default; a code point no face covers lays out as `.notdef` of the first face and counts `ui.text.missing_glyphs` |
| `Test_GlyphCache_SkylinePacking` | `Tests/Zenith_UIGlyphCache_Tests.cpp` | 10,000 random rectangles pack without overlap inside the page with a one-texel gutter |
| `Test_GlyphCache_DynamicStateMachine` | same | every transition of Text.md "Dynamic glyph state machine", including the per-frame generation limit and eviction of unused glyphs |
| `Test_GlyphCache_HeadlessNeverGenerates` | same | with no renderer, every lookup reports EMPTY for drawing, no page is created and no MSDF is generated, while layout metrics equal a run with a renderer |
| `Test_GlyphCache_ShedsUnderPressure` | same | WARNING releases dynamic pages unused for 300 frames and the layout caches beyond their minimum; CRITICAL also drops baked CPU pixels of resident pages and unreferenced faces; glyphs in use stay resident |
| `Test_UITextCache_SteadyStateShapesNothing` | `Tests/Zenith_UITextCache_Tests.cpp` | a second frame of unchanged text shapes zero paragraphs |
| `Test_UITextCache_KeyIgnoresColour` | same | changing colour alone hits the cache; changing size misses |

### Localization

| Test ID | File | Asserts |
|---|---|---|
| `Test_UILoc_ParseLocale` | `Tests/Zenith_UILocalization_Tests.cpp` | canonical casing, the 31-byte limit, rejected forms |
| `Test_UILoc_Negotiation` | same | each step of Localization.md "Locales and negotiation" with the fixture project |
| `Test_UILoc_CompileEveryOpcode` | same | every message construct compiles to the documented bytecode |
| `Test_UILoc_CompileErrors` | same | each `LOCALIZATION_ERROR_*` from a minimal message |
| `Test_UILoc_FormatArguments` | same | integer, number, percent, fixed styles; isolates around text arguments; missing arguments; a missing key formats `⟦key⟧` in a development build and `⟦#hash⟧` with the Shipping flag set, counting `ui.loc.missing`; an argument of the wrong kind formats as text and counts `ui.loc.arg_kind_mismatch` |
| `Test_UILoc_PluralRules_MatchCLDRSamples` | same | every cardinal and ordinal rule of the pinned CLDR evaluates each of its own samples to its category |
| `Test_UILoc_NumberFormatting` | same | `en-GB` 1,234,567.5; `fr-FR` narrow no-break space grouping; `hi-IN` 12,34,567; `es-ES` minimum grouping; `ar` native digits |
| `Test_UILoc_LocaleSwitchAtFrameBoundary` | same | a switch requested mid-frame commits at the next prologue after the tables load; a failed load keeps the previous locale |
| `Test_UILoc_PseudoModes` | same | accented, mirrored and long transforms leave arguments, tags and plural selection intact; long mode expands the longest translation by `ui.loc.pseudoLongFactor` |
| `Test_UILoc_InterpreterRejectsCorruptBytecode` | same | out-of-bounds operands format as missing, no crash |
| `Test_UIExtraction_CollectsEverySource` | `ZenithCook/Tests/UI/Zenith_UIExtraction_Tests.cpp` (`z2-ui-cooked`) | documents, `UI.Loc` literals with lines, a localized data-defined property and a sink contribution all appear; a dynamic key is listed |
| `Test_UIXliff_RoundTrip` | same | export then import of an unchanged locale changes nothing; a translated unit is written with its source hash; a unit breaking a placeholder is rejected |
| `Test_UILoc_StressAllEngineDocuments` | `Tests/Zenith_UILocalizationStress_Tests.cpp` | every engine document under each pseudo mode at text scale 2.0 lays out with no overflowing text leaf |

### Input focus and navigation

| Test ID | File | Asserts |
|---|---|---|
| `Test_UIHitTest_OrderAndPruning` | `Tests/Zenith_UIInput_Tests.cpp` | Interaction.md "Hit testing": layers, reverse paint order, clip pruning, `passThrough`, rounded corners, blocking screens |
| `Test_UIPointer_StateMachine` | same | every row of Interaction.md "Pointer state machine" from a scripted transition list |
| `Test_UIPointer_ScrollVersusPressThreshold` | same | movement within 8 dp activates; beyond it scrolls without activation |
| `Test_UIFocus_StateMachine` | `Tests/Zenith_UIFocus_Tests.cpp` | every row of Interaction.md "Focus state machine" |
| `Test_UINav_DirectionalScoring` | same | fixture grids where the expected target follows the score formula by hand, ties by document order, explicit targets, groups and wrap |
| `Test_UINav_TabOrderAndTrap` | same | `nav.order`, document order, wrap and `nav.trap` |
| `Test_UINav_VirtualListScrollsBeforeFocus` | same | navigating past the last materialised item scrolls one item and focuses it |
| `Test_UIControls_ActivationTable` | `Tests/Zenith_UIControls_Tests.cpp` | each row of Interaction.md "Activation and value controls" emits the right event once |
| `Test_UIControls_CancelClaimsOnlyWithCancelEvent` | same | Cancel is claimed only when the active screen declares `cancelEvent` |
| `Test_UIClaimant_ClaimsOnlyHitPointers` | `Tests/Zenith_UIClaimant_Tests.cpp` | through the real `Zenith_InputSystem` with a virtual device: presses on interactive elements are claimed, presses elsewhere reach the mapper |
| `Test_UIClaimant_PushesEngineUIWithFocusScope` | same | `Engine.UI` is on the presentation stack exactly while a focus scope exists; `focusScope: never` screens never push it |
| `Test_UITextField_SessionLifecycle` | same | Begin with the field's purpose and area, composition, commit, cancel, a refused Begin |
| `Test_UIRebind_Flow` | same | activation starts a rebind with the element's action, scheme and index; success, conflict and reset display; navigation suppressed meanwhile |
| `Test_UIOnScreenControls_VirtualDevice` | same | a stick and a button drive `/virtual/ui/*` values that the mapper turns into actions |
| `Test_UIPrompts_CacheRefreshOnSchemeChange` | same | prompt cache refreshes on a scheme change and on a binding change; the extractor reads only the cache |

### Accessibility

| Test ID | File | Asserts |
|---|---|---|
| `Test_UIA11y_RolesAndStates` | `Tests/Zenith_UIAccessibility_Tests.cpp` | the role and state of every element type per Accessibility.md "Roles and states" |
| `Test_UIA11y_NameDerivationOrder` | same | label, labelledBy, text content, prompt label, in order |
| `Test_UIA11y_TreeStableIDsAndRevision` | same | node IDs equal across frames; the revision changes only on a real change |
| `Test_UIA11y_SkippedWithoutConsumer` | same | no consumer → no tree built (stats) |
| `Test_UIA11y_NarrationComposition` | same | utterances for button, checkbox, slider, list item in `en-GB` and `fr-FR` |
| `Test_UIA11y_LiveRegionsDebounce` | same | polite debounce 500 ms; assertive interrupts; announcements queue |
| `Test_UIA11y_ContrastRule` | same | WCAG ratios for known colour pairs; large-text threshold |
| `Test_UIA11y_ColourVisionCorrection` | same | the correction for each mode applied to a known palette equals the Machado matrices' result within 1/255 |
| `Test_UINarration_RingSurvivesArenaReset` | same | narration requests and the tree's back buffer copied by the extractor are intact after the extractor's arena is reset and are read by the next Pump; a full ring drops the oldest announcement and counts `ui.a11y.narration_dropped` |
| `Test_UIA11y_AllFocusablesReachable` | `Tests/Zenith_UIAccessibilityAudit_Tests.cpp` | one `SUBCASE` per audited document (Accessibility.md "Accessibility validation"): every focusable element is reachable by directional navigation from the initial focus |
| `Test_UIA11y_TreeAudit` | same | one `SUBCASE` per audited document: no unnamed interactive node; every node's bounds inside the surface |

### Draw lists and script bindings

| Test ID | File | Asserts |
|---|---|---|
| `Test_UIDrawList_PaintOrder` | `Tests/Zenith_UIDrawList_Tests.cpp` | Rendering.md "Paint order", including focus ring above children and zIndex |
| `Test_UIDrawList_QuadsPerPart` | same | the quads of Rendering.md "Quad generation per element" for each part |
| `Test_UIDrawList_BatchBreaks` | same | batches break exactly on scissor, texture and SDF changes |
| `Test_UIDrawList_ClipCullsOutside` | same | quads wholly outside a clip are not emitted |
| `Test_UIDrawList_TruncatesAtBatchBoundary` | same | beyond `uFLUX_UI_MAX_QUADS_PER_LIST` the list ends at a batch boundary and counts |
| `Test_UIDrawList_PendingGlyphsSkipped` | same | glyphs with pending placements emit nothing and line metrics are unchanged |
| `Test_UIQuad_HalfFieldsUseVertexCodec` | same | the f16 fields equal `Zenith_F32ToF16` of their float inputs for three values per field, and a size above 65,504 pixels clamps |
| `Test_UIDrawList_ZIndexAndClipVocabulary` | same | siblings paint and hit-test in ascending `zIndex` with document-order ties; `clip: true` pushes a clip; the `shadow*` properties emit one SHADOW quad; an element with `visible` false emits nothing |
| `Test_UIScriptApi_FunctionsRegistered` | `Tests/Zenith_UIScriptApi_Tests.cpp` | every function of the UI Luau table is registered with its purity and side-effect mask |
| `Test_UIScriptApi_BufferedRequestsDiscardedOnFailure` | same | a script invocation that errors after `UI.Focus` records no request |
| `Test_UIScriptApi_MisuseCodes` | same | each code ZS2101–ZS2106 of the entry document's "Luau errors and asserts" is raised by its trigger with its exact text and kind `MISUSE`; nothing asserts |

### Editor model

| Test ID | File | Asserts |
|---|---|---|
| `Test_UIEditorModel_EditsReversible` | `Tests/Zenith_UIEditorModel_Tests.cpp` | every edit operation applied then undone restores the canonical document bytes; redo reapplies it |
| `Test_UIEditorModel_ValidatesAfterEdit` | same | an edit that breaks a rule reports that rule's diagnostic with its document path and leaves the model editable |
| `Test_UIEditorModel_CataloguesComplete` | same | the widget catalogue has one entry per `Zenith_UIElementType` and the property catalogue one per style property, with defaults equal to the schema's |
| `Test_UIEditorModel_PreviewContextSettings` | same | a preview context at a given surface size, locale, palette, pseudo mode and text scale lays out as a game context with the same settings, and picking returns the hit element's uid |

## Layout golden suite

`Test_UILayoutGolden_AllCases` (`Tests/Zenith_UILayoutGolden_Tests.cpp`) runs the cases of Layout.md "Layout conformance cases": each `<Case>.ui.json` is compiled in memory, laid out with the test font at its named surface size, and every element's rectangle is compared with `<Case>.expected.json` within 0.01 canvas units. The expectations are committed text written by hand from the rules (the test font makes text sizes derivable). Which cases run depends on whether D-SEL-UIRT is open:

- **While D-SEL-UIRT is open** (both layout engines compiled, from slice 7's ticket, which writes both `Zenith_UIRuntimeBackend` implementations, to the ruling ticket), `Test_UILayoutGolden_AllCases` and the mirrored variant run the **shell** cases only, once per compiled candidate (a `SUBCASE` per candidate), and assert only what the shell computes, relative to the same candidate's own base layout, never the case's absolute hand-written rectangles: every shell case's rectangles pass through the candidate's `Layout`, so an absolute comparison would red the gate for a candidate that loses a CSS or ZenithUI-semantics behaviour the case's content happens to use. The per-case invariants are below. The absolute expectation of each shell case is compared by `Test_UIRuntimeSelection_ConformanceBothCandidates` instead and joins its report as class `shell`, recorded and not judged. The text cases the pseudo-long and text-scale variants iterate are all CSS cases, so those two variants register their cases only from the ruling on. The **CSS** and **ZenithUI semantics** cases run only inside the selection test, which reports and does not judge.
- **After the ruling**, whose ticket deletes the losing engine and the selection test, the four tests run every case on the winner against its absolute hand-written expectation and fail the gate on any mismatch.

The shell invariants, each evaluated on one candidate with that candidate's own layouts as the reference (0.01 canvas units):

| Shell case | Invariant while D-SEL-UIRT is open |
|---|---|
| `ReferenceScaleMatch`, `DpScaleUIScale` | the canvas size and `s` equal the formulas of Layout.md "Canvas units and scaling" for the case's surface and settings, and each rectangle equals the same candidate's layout of the same document laid out directly at that canvas size |
| `SafeAreaInset` | the root's border box equals the canvas inset by the safe area, and every child rectangle equals the same candidate's layout of the document under a root of that size, translated by the inset's origin |
| `PixelSnapping` | each snapped edge equals `round(x × s)` (round half to even) of the same candidate's unsnapped edge, and adjacent elements share their snapped edges |
| `MirrorAsymmetricEdges`, `MirrorLtrIsland`, `MirrorLtrLeafInRtlRow` | the RTL rectangles equal steps 2 and 3 of Layout.md "Logical direction and mirroring", applied by the test's own implementation of those two steps to the same candidate's LTR rectangles of the margin-exchanged tree (the document with each margin exchange of step 2 written into its LTR form) |
| `MirrorPhysicalOffsets` | each quad's offset from the same candidate's RTL border box equals the authored `translate` and `shadowOffset` with their signs, and the `mirrorInRtl` image's quad has u0 and u1 swapped |
| `InstanceFlattening` | the flattened element table equals the expected structure (types, parents, overrides applied), and its rectangles equal the same candidate's layout of the hand-flattened document `InstanceFlattening.flat.ui.json` |

`ListFixedExtent`, `ListVariableExtent` and `WorldPanelCanvas` land with slices 16 and 12, after the ruling, and gate on their absolute expectations from the start.

| Test ID | Variation | Expectation source |
|---|---|---|
| `Test_UILayoutGolden_AllCases` | as authored | the expected files |
| `Test_UILayoutGolden_PseudoLong` | the text cases under pseudo-long mode, one `SUBCASE` each | no text leaf overflows its container unless it scrolls or truncates |
| `Test_UILayoutGolden_TextScaleTwoNoOverflow` | text scale 2.0, one `SUBCASE` per text case | same overflow rule |
| `Test_UILayoutGolden_MirroredMatchesReflection` | RTL locale, one `SUBCASE` per case marked "Mirrored" | rectangles equal `x' = W − x − w` of the LTR expected rectangles (Layout.md "Logical direction and mirroring"); the four `Mirror*` cases compare with their hand-written RTL files; while D-SEL-UIRT is open it runs the four `Mirror*` shell cases alone, per candidate, in the invariant form above |
| `Test_UIRuntimeSelection_ConformanceBothCandidates` | each D-SEL-UIRT candidate, while both exist (the harness ticket adds it; the ruling's cleanup ticket deletes it with the losing candidate) | the CSS and ZenithUI-semantics cases on both candidates, compared with the expected files, and the absolute expectations of the shell cases as class `shell` (recorded, not part of step 1 of the acceptance rule); per candidate, per case, the class, pass or fail with the first differing rectangle, and each shim used with its code size are written to `<ZENITH_TEST_GENERATED_ROOT>/Selection/RuntimeSelection.json` (`Zenith2/Docs/Design/UI.md → "Selection decisions UI owns"`). The test passes whatever the candidates' results; it fails only on a harness error: a candidate's `Zenith_UIRuntimeBackend` that cannot be created (`UI_ERROR_RUNTIME_BACKEND`), a crash, a case file missing or unparsable, a case not executed on every candidate, or a report that is not written or does not parse back with exactly one entry per (candidate, case) |

## Shaping golden suite

`Test_UITextCorpus_TestFontShaping`, `Test_UITextCorpus_Shaping`, `Test_UITextCorpus_Bidi`, `Test_UITextCorpus_Breaks`, `Test_UITextCorpus_Graphemes` and `Test_UITextCorpus_Fallback` (`Tests/Zenith_UITextCorpus_Tests.cpp`) run the corpus groups of Text.md "Text conformance corpus", and `Test_UITextSelection_CorpusBothCandidates` compares the D-SEL-TEXT candidates. Which exists when:

| Test ID | Lands with | Runs on | Compares |
|---|---|---|---|
| `Test_UITextCorpus_TestFontShaping` | slice 5 (fonts) | each compiled stack, one `SUBCASE` per stack while D-SEL-TEXT is open; the winner after | the test font's ligature, kern, mark and Hebrew cases with hand-written glyph IDs, clusters, advances and offsets (the mark at (−500, 0)); the gate's oracle for the shaping pipeline's wiring, which HarfBuzz makes common to every stack; needs no recorded data |
| `Test_UITextCorpus_Graphemes`, `Test_UITextCorpus_Fallback` | slice 5 | each compiled stack while D-SEL-TEXT is open; the winner after | the hand-written expectations of the Graphemes and Fallback groups (shell code: UAX #29 from ZenithUI's tables, per-cluster face selection) |
| `Test_UITextSelection_CorpusBothCandidates` | the D-SEL-TEXT harness ticket (slice 4, blocked by the SA recording ticket and by slices 2, 5, 7 and 9, whose code its benchmarks run); deleted by the ruling's cleanup ticket | both stacks | `TestFont.json`; the Bidi group's levels per code point from each stack's `Zenith_UIBidi::ResolveLevels` (visual order is the shell's step 10, common to both); the Graphemes and Fallback groups; `Pseudo.json`'s line counts and truncation flags in the test font (its accented letters are `.notdef` of advance 500 there, so every count is derivable by hand and depends only on each stack's `FindBreaks`); the Breaks group from each stack's `Zenith_UILineSegmenter::FindBreaks`, compared as Text.md "Corpus file format" states: the hand-written offsets outside SA groups and the ICU4C offsets the Stage 2 recording ticket committed for every case of an SA group; and the outline equivalence check (each stack's outline of every corpus glyph rasterised to an MSDF equals the other's within one quantisation step). Shaping goldens take no part, because HarfBuzz is common to both. Per stack, per case, pass or fail with the first differing value, goes to `<ZENITH_TEST_GENERATED_ROOT>/Selection/TextSelection.json`. The test passes whatever the stacks' results; it fails only on a harness error: a stack that cannot be created or crashes, a case file missing or unparsable, a listed SA case without recorded offsets, a case not executed on both stacks, or a report not written or not parsing back with one entry per (stack, case) |
| `Test_UITextCorpus_Breaks` | the D-SEL-TEXT ruling's cleanup ticket | the winner | every Breaks case, SA ranges against the recorded ICU4C offsets, the rest against the hand-written offsets |
| `Test_UITextCorpus_Bidi` | slice 15 (bidi and RTL, Stage 4) | the winner | levels and visual order of the Bidi group, the bold-middle-word case and the L1 trailing-white-space case included |
| `Test_UITextCorpus_Shaping` | the ordinary ticket that follows the Noto shaping-golden recording ticket (slice 13, Stage 3, after the ruling) | the winner | every group whose `Groups.json` entry has `"shapingGolden": true` (set by that recording ticket) against its `<Group>.expected.json`, recorded with the pinned HarfBuzz's `hb-shape` (one golden per group, never per candidate) |
| `Test_UITextCorpus_SaBreakRecorderConverts` | slice 3 (`ZenithCook_Tests`, `Zenith2/Tools/ZenithCook/Tests/UI/Zenith_UISaBreakRecord_Tests.cpp`) | ICU4C, not a candidate | the recording function of the SA record case over three in-memory texts whose ICU4C results follow from UAX #14 alone: `ab cd` gives `allowed` [3] and no `mandatory`; `ab` LF `cd` gives `mandatory` [3]; U+1D400 (four UTF-8 bytes, two UTF-16 units), space, `b` gives `allowed` [5], the UTF-16 index 3 converted to the UTF-8 offset 5; in every case neither 0 nor the text's byte length is listed |
| `Test_UITextCorpus_SaBreaks_Record` | slice 3 (same file) | ICU4C | the record case, skipped (`doctest::skip()`), run only by the `needs-human` recording ticket (Text.md "Corpus file format") |

Every corpus test enumerates the groups through `Groups.json`, which slice 3 commits with every group listed. A group whose entry has `"shapingGolden": true` and whose `<Group>.expected.json` is missing or unparsable fails `Test_UITextCorpus_Shaping`, naming the group; a case of a group with `"sa": true`, not listed in its `removed`, that has no `breaks` member fails `Test_UITextCorpus_Breaks`, naming the case, and is a harness error of the selection test; there is no reduced-assertion mode.

## Render tests

Run on WARP and lavapipe with validation clean; each image is checked against an oracle computed in the test from the quad records, never against a committed image. The tests that need only ZenithFlux (hand-built quad records, the UI renderer, the upload ring) are UI's files of `ZenithFlux_Tests` under `Zenith2/Engine/ZenithFlux/Tests/UI/`:

| Test ID | Asserts |
|---|---|
| `Test_UIQuad_LayoutMatchesShader` | the reflected offsets of `Flux_UIQuadGPU` equal `offsetof` for every field of `Flux_UIQuad` |
| `Test_UIRender_SolidCoverage` | rounded rectangles with borders: pixels fully inside equal the colour, fully outside the background, and edge pixels have coverage within 1/255 + 0.06 of the analytic area |
| `Test_UIRender_GradientLinearInterpolation` | gradient pixels equal the linear-space interpolation encoded for output within 1/255 |
| `Test_UIRender_ScissorPerBatch` | no pixel outside a batch's scissor changes |
| `Test_UIRender_PremultipliedBlending` | in SDR output (an `RGBA8_SRGB` target), a 50 % white quad over black stores 188 (`round(255 × encode_sRGB(0.5))`, ±1), the linear-light blend of Rendering.md "Colour and HDR output"; in scRGB output it stores 0.5 × `r.hdr.uiNits` / 80 |
| `Test_UIRender_HDR10Encoding` | in HDR10 output mode, white UI encodes to PQ of `r.hdr.uiNits` |
| `Test_UIRender_AtlasSwapKeepsOldDescriptorValid` | a dynamic page replaced in frame k still samples correctly for quads written before the swap |
| `Test_UIRender_WorldSpaceOcclusion` | a world panel behind an opaque box is hidden where the box covers it and visible elsewhere, once on a standard-Z D24S8 view (`LESS_OR_EQUAL`) and once on a reversed-Z view (`GREATER_OR_EQUAL`), the comparison taken from each view's `Flux_DepthMode` |
| `Test_UIRenderer_WritesAfterReservation` | through the real `Flux_UploadRing`, `PrepareFrame` declares Σquads × 64 bytes and writes nothing; after `ExtendReservation`, `WriteFrame` places the quads at the allocation's offset; a deferred frame leaves the ring range unchanged and draws no UI |
| `Test_UIWorldSpace_MovingPanelGhostingBounded` | a panel moving 4 px per frame under TAA leaves no pixel more than 3 px behind its true edge brighter than 10% of the panel colour |

The tests that need ZenithUI's glyph cache, text stack or test font with a renderer are GPU cases of `Zenith_Integration_UI` under `Zenith2/Tests/UI/Render/`, each creating its WARP or lavapipe device directly ("Test targets and files"):

| Test ID | File | Asserts |
|---|---|---|
| `Test_UIRender_GlyphMSDFCoverage` | `Zenith2/Tests/UI/Render/Zenith_UIRenderGlyphs_Tests.cpp` | glyphs of the test font at 12, 24, 96 px: interior texels opaque, counters transparent, edge coverage within 0.1 of the outline's analytic coverage |
| `Test_GlyphCache_PageCreateFailureRetries` | same | with `CreatePage` failing through the fault injector, the page stays CPU-side with its glyphs pending, `ui.atlas.upload_failed` counts, and the next frame's retry makes them resident |
| `Test_UIRender_DeviceLossRecreatesPages` | same | after a simulated device loss, every page is recreated from its CPU copy and text renders identically after one frame of pending glyphs |
| `Test_UIRender_CrossBackend` | `Zenith2/Tests/UI/Render/Zenith_UIRenderParity_Tests.cpp` | the UI parity scene (a menu and its text, drawn by `UIComposite` alone, and only `UIComposite` output is sampled; a world panel is not part of it, because its pixels pass through `UIWorldSpace`'s luminance scale, TAA and tonemapping, which no oracle above covers, and the world-panel path is checked on each device by `Test_UIRender_WorldSpaceOcclusion` instead) renders on both devices, each image passes the oracles of the tests above at its sampled pixels, and at every sampled pixel the two images differ by at most the sum of the two devices' oracle tolerances there: 2/255 on solid interiors and gradients, 2 × (1/255 + 0.06) of coverage on shape edges, 0.2 of coverage on glyph edges, each converted to the target's encoding by the test; the bound is derived, not measured, so no budget row supplies it (`Zenith2/Docs/Design/UI.md → "Pending row needs"`); unsampled pixels are not compared |

## Integration tests

Files under `Zenith2/Tests/UI/`, on the headless runtime (no Flux) unless marked:

| Test ID | Asserts |
|---|---|
| `Test_UIModule_StartSequence` | the module start steps run in order; types, config keys, command kinds, extractor, systems, claimant and (on WARP) the view extension register |
| `Test_UIModule_HeadlessRunsWithoutRenderer` | with `GetRenderer()` null: no `Flux_UIRenderer`, no view extension, no page, no pass; documents, layout, events, requests and the accessibility tree still run, draw lists build with no glyph quads, and the extractor writes nothing |
| `Test_UIModule_StartStep01FailureUnwinds` | failing start step 1 (read the configuration) alone through the fault injector returns its error and leaves no registration, callback, device or thread |
| `Test_UIModule_StartStep02FailureUnwinds` | failing start step 2 (register the asset kind loaders) alone through the fault injector returns its error and leaves no registration, callback, device or thread |
| `Test_UIModule_StartStep03FailureUnwinds` | failing start step 3 (request the engine content) alone through the fault injector returns its error and leaves no registration, callback, device or thread |
| `Test_UIModule_StartStep04FailureUnwinds` | failing start step 4 (register the UI Luau table) alone through the fault injector returns its error and leaves no registration, callback, device or thread |
| `Test_UIModule_StartStep05FailureUnwinds` | failing start step 5 (declare ASYNC_SOURCE_UI) alone through the fault injector returns its error and leaves no registration, callback, device or thread |
| `Test_UIModule_StartStep06FailureUnwinds` | failing start step 6 (enable the UI renderer and register the view extension (WARP)) alone through the fault injector returns its error and leaves no registration, callback, device or thread |
| `Test_UIModule_StartStep07FailureUnwinds` | failing start step 7 (create Zenith_UISystem, the claimant and the virtual device) alone through the fault injector returns its error and leaves no registration, callback, device or thread |
| `Test_UIModule_StartStep08FailureUnwinds` | failing start step 8 (register the six command kinds) alone through the fault injector returns its error and leaves no registration, callback, device or thread |
| `Test_UIModule_StartStep09FailureUnwinds` | failing start step 9 (register the memory consumer) alone through the fault injector returns its error and leaves no registration, callback, device or thread |
| `Test_UIModule_StartStep10FailureUnwinds` | failing start step 10 (connect the accessibility service) alone through the fault injector returns its error and leaves no registration, callback, device or thread |
| `Test_UIModule_StartStep11FailureUnwinds` | failing start step 11 (construct Zenith_UIModule) alone through the fault injector returns its error and leaves no registration, callback, device or thread |
| `Test_UIModule_AttachStep1FailureUnwinds` | failing attach step 1 (add the world resources) alone fails `AttachWorld`, undoes the steps before it and leaves the world without UI registrations |
| `Test_UIModule_AttachStep2FailureUnwinds` | failing attach step 2 (register UI.Events) alone fails `AttachWorld`, undoes the steps before it and leaves the world without UI registrations |
| `Test_UIModule_AttachStep3FailureUnwinds` | failing attach step 3 (register UI.Captions) alone fails `AttachWorld`, undoes the steps before it and leaves the world without UI registrations |
| `Test_UIModule_AttachStep4FailureUnwinds` | failing attach step 4 (add the extractor) alone fails `AttachWorld`, undoes the steps before it and leaves the world without UI registrations |
| `Test_UIModule_AttachStep5FailureUnwinds` | failing attach step 5 (create the world's context) alone fails `AttachWorld`, undoes the steps before it and leaves the world without UI registrations |
| `Test_UIModule_AttachStep6FailureUnwinds` | failing attach step 6 (declare the UI source on the publication sink and keep the world control) alone fails `AttachWorld`, undoes the steps before it and leaves the world without UI registrations |
| `Test_UIEvents_ReachScriptsNextTick` | a click published in frame k delivers `UI.Activated` to a script in the first tick after; element, event and screen match |
| `Test_UIEvents_ReplayIdentical` | a recorded session with clicks, slider drags and text commits replays with identical simulation hashes; live clicks and drags injected during the replay publish nothing, allocate no completion ID and count `ui.replay.live_events_ignored`, and the replay never halts on a divergence (at every tick boundary the world's live counter, `Zenith_AsyncResultTable::GetNextCompletionID`, is at most the recorded next completion ID, which the table then adopts: Interaction.md "Publication path", [Z2-N-ARCH-794], X278); a script in a tick after a recorded click requests a recomputed result, an additive scene load (`ASYNC_SOURCE_SCENE_LOAD`), and its completion ID and its publication tick in the replay equal the recording's; a second replay with every `ui.*` and `ui.access.*` key set to a non-default value (scale, locale, text scale, reduced motion, colour vision, narration) gives the same hashes. Its ticket is blocked by the SCRIPT ticket that lands X278 |
| `Test_UIEvents_ReplayCorruptPayloadDropped` | a replay log whose UI payload is malformed drops that event, counts `ui.events.decode_failed` and continues; the following ticks run |
| `Test_UIEvents_ChannelFullDropsAndCounts` | at the defaults, 64 ordinary events in one frame publish 63 and drop one (the 64th slot is the resume record's); with the world paused, the 256th outstanding ordinary event is dropped; both count `ui.events.dropped` and the control returns to its bound value; with 255 ordinary events outstanding a `resumeWorld` action still publishes its resume record |
| `Test_UIEvents_WorkerCountInvariant` | 1, 2 and 8 workers produce identical event order and simulation hashes, with two worlds publishing in the same prologue (completion IDs ordered per world) |
| `Test_UIEvents_PausedWorldDefersPublication` | a click during a `GAMEPLAY` pause publishes at the first tick after resume, in order with clicks made after it, and its replay delivers it at the same tick with no input |
| `Test_UIPauseMenu_ResumeClearsMenuReason` | with the world paused for `MENU`, activating a `resumeWorld` button (and, in a second case, Cancel on a `cancelAction: resumeWorld` screen) clears only that reason at the next prologue, the resume record and then `UI.Activated` publish in the first resumed tick; the replay of the session, run with no input, passes the paused span by the replay rule of Interaction.md "Pause menus", publishes the resume record at the recorded tick and matches every tick's hash; a live `resumeWorld` during the replay counts `ui.replay.live_resume_ignored` and changes nothing; with 255 ordinary events outstanding in the paused world the resume record still publishes ahead of the dropped ordinary event, `MENU` clears and the session's replay resumes at the same tick; with a resume record still outstanding (the world also paused for `GAMEPLAY`) and `MENU` requested again by the test through `Zenith_WorldControl::AddPauseReasons`, a resume press leaves the control untouched and counts `ui.events.resume_refused` (Interaction.md "Pause menus") |
| `Test_UIEvents_StaleScreenDropped` | a screen destroyed between click and tick drops the event and counts it |
| `Test_UIEvents_WriteBackSetsProperty` | a two-way bound slider sets its self property through a command at the phase-1 sync point |
| `Test_UIRequests_FocusScrollAnnounce` | `UI.Focus`, `UI.ScrollTo`, `UI.Announce` from a script take effect in the next extraction; a request naming a destroyed screen is counted |
| `Test_UICaptions_FromCommandsAndCueEvents` | `UI.ShowCaption` and a cue event both reach the caption queue and the caption screen draws them |
| `Test_UIScreens_SpawnFromPrefab` | a prefab with `Zenith.UI.Screen` opens its screen in the world's context and closing it releases its document |
| `Test_UIWorlds_ContextsIndependent` | two worlds' contexts lay out and extract independently; only the input world receives input |
| `Test_UIExtract_SlotFullDrawsNothing` | (WARP) a render-world slot too small for the world's UI list: the world draws no UI that frame, `ui.draw.slot_full` counts, the next frame with room draws normally |
| `Test_UIA11y_NoServiceKeepsSettings` | with the platform's accessibility backend `none` (`Zenith_Platform::GetAccessibility()` null), no tree is published and no narration spoken; settings still apply and the semantic tree is still built for a test request |
| `Test_UIA11yBridge_PublishesWithinPlatformLimits` | with PLAT's recording backend (`--set platform.accessibility.backend=recording`), a context of 5,000 focusable elements across two screens publishes a tree of exactly 4,096 nodes, the pre-order prefix that holds the frontmost screen complete, PLAT rejects no publication (its validation rules V1 to V9), `ui.a11y.nodes_capped` counts the omitted nodes; with focus on an omitted node the published view's `m_uFocused` is `uACCESSIBILITY_NO_NODE` and no node carries the focused state; a button whose localized name contains U+0000 publishes that character as U+FFFD; a `HEADING` text of level 3 publishes `m_uHeadingLevel` 3; the first publication of that tree, larger than the publish buffer's initial capacity, is staged in the following frame and counts `ui.a11y.publish_deferred` once; an OS activate action injected with `Zenith_AccessibilityTestAccess::InjectAction` on a published node publishes the same `UI.Activated` a press would, a failing `Speak` (the recording backend reporting no voice) counts `ui.a11y.speech_failed`, and an OS preference change of `platform.accessibility.reducedMotion` reaches `ui.access.reducedMotion` at the next prologue only while no layer above the engine default holds a value for `ui.access.reducedMotion` (Accessibility.md "Platform accessibility bridge", "Accessibility settings"). Its ticket is blocked by Z2T-PLAT-103, Z2T-PLAT-104 and, for the preference case, Z2T-PLAT-105 (the `os:accessibility` platform-layer source) |
| `Test_UIA11yBridge_RepublishesOnInputWorldChange` | with PLAT's recording backend: before any world attaches, start step 10's `SetTreeProvider` publishes an empty tree at revision 0; world A attaches as the input world and its tree publishes; world B attaches, its context's own tree revision below A's, and becomes the input world: the next Pump publishes B's tree with no `ui.a11y.publish_rejected` and no PLAT rejection; B detaches with no input world left: the next Pump publishes an empty tree; across the run the record log's `ACCESSIBILITY_RECORD_TREE_PUBLISHED` revisions strictly increase and each published view's `m_ulRevision` equals the bridge's module-wide revision, never a context's (Accessibility.md "Platform accessibility bridge") |
| `Test_UIFrameGraph_ExtractorBeforeSeal` | the extractor runs before `Render.Seal@W0` for every world tag |
| `Test_UIDiagnostics_CountersAndZones` | every counter of the entry document's "Counters" is registered and moves in the scenario that should move it; the profiler zones appear in a captured frame |
| `Test_UIDiagnostics_DebugViewsAndDump` | each debug view key, set through the configuration store's command-line layer, adds draw-list quads only while enabled; the `ui` section registered with DEVT's frame dump writes the listed members per world |
| `Test_UIMemory_ConsumerReportsBytes` | the memory consumer reports host and device bytes within 5% of a sum over fonts, pages, documents and caches |

The accessible flows (Accessibility.md "End-to-end accessible flows") live under `Zenith2/Tests/UIAccessibility/` in the same target:

| Test ID | Asserts |
|---|---|
| `Test_UIFlow_EngineSettingsRoute` | the four flows' checks over the engine accessibility settings route |
| `Test_UIFlow_ScreenReader_Game2D`, `Test_UIFlow_ScreenReader_OpenWorldSlice` | the screen-reader flow of that slice's route up to `gateTicks` (full route with `--ui-flow-full`) |
| `Test_UIFlow_RemappedControls_Game2D`, `Test_UIFlow_RemappedControls_OpenWorldSlice` | the remapped-controls flow |
| `Test_UIFlow_ReducedMotion_Game2D`, `Test_UIFlow_ReducedMotion_OpenWorldSlice` | the reduced-motion flow |
| `Test_UIFlow_ColourVision_Game2D`, `Test_UIFlow_ColourVision_OpenWorldSlice` | the colour-blind flow, one `SUBCASE` per mode |

## Fuzz targets

| Target | Input |
|---|---|
| `Fuzz_UIDocumentJson.cpp` | UI, theme, font family, localization project and string table JSON through the document readers and validators |
| `Fuzz_UICookedDocument.cpp` | `.zui` bytes through the loader |
| `Fuzz_UICookedFont.cpp` | `.zfont` bytes, including page payloads |
| `Fuzz_UICookedLocalization.cpp` | `.zloc` bytes, then `Format` of every key with random arguments |
| `Fuzz_UIMessageSyntax.cpp` | message sources through `Zenith_CompileMessage` |
| `Fuzz_UIPluralRule.cpp` | CLDR rule strings through `Zenith_CompilePluralRule` and evaluation |
| `Fuzz_UIPublication.cpp` | payloads through `Zenith_DecodeUIPublication` |
| `Fuzz_UITextLayout.cpp` | random UTF-8 (valid and invalid) with random styles and widths through the paragraph pipeline with the test font |
| `Fuzz_UIBindingManifest.cpp` | manifest bytes |
| `Fuzz_UIXliff.cpp` (ZenithCook) | XLIFF bytes through `Zenith_ReadXliffImport` with an in-memory storage |

Each asserts no crash, no sanitizer report, and a `Zenith_Result` error for every rejected input; `Fuzz_UITextLayout` additionally asserts that every caret position round-trips and that line widths never exceed the requested width except for a single unbreakable grapheme.

## Benchmarks and harnesses

`Zenith2/Benchmarks/UI/` (`Zenith_Bench_UI`, `zenith_add_benchmark(UI …)`, which also registers its `--smoke` CTest run) is built by the D-SEL harness tickets in the Stage 2 sprint (`Zenith2/Docs/Design/UI.md → "Selection decisions UI owns"`), which write the harness, its benchmarks and its smoke CTest and no candidate code (the candidates are written by slices 3, 5 and 7, `Zenith2/Docs/Design/UI.md → "Stage table"`); while a selection is open the harness links both candidates of that selection and `--candidate <name>` selects one; the per-system budget ticket reuses it for the budget rows. Its scenes are synthesised at the W-UI2D scale that the Stage 1 workload scale ticket fixes in `Zenith2/Docs/Research/Workloads/W-UI2D.md`. The smoke run executes every benchmark compiled into the harness for three frames with each compiled candidate and asserts only that each completes and reports a finite time, so the gate proves the harness works without measuring anything. Stage 2 has no localization tables (they land with slice 14), so the Stage 2 benchmark documents (the menu's 300 text leaves, the HUD's bound text) are development documents (header flag bit 2, Documents.md "Header record") whose text content records are of form 3, `literal`, and whose string-valued bindings are STRING_ID bindings without the text-mode-key flag (binding record flags bit 1 clear), so every text is displayed as given; keyed text is first measured by slice 14's `Bench_UI_MenuLocaleSwitch`, which uses a keyed copy of the menu document, and by the locale tables slice 14 adds to `Bench_UI_Memory`. At Stage 2 the harness holds exactly the benchmarks whose code the Stage 2 pull-forward provides (`Zenith2/Docs/Design/UI.md → "Stage plan"`, the slice table); a benchmark marked with a subsequent slice is added, with its smoke run, by that slice's ticket.

| Benchmark | Measures | Row | Added by |
|---|---|---|---|
| `Bench_UI_MenuFrame` | update + layout + draw list for a 2,000-element menu with 300 text leaves, steady state | `Z2-B-UI-002` | the D-SEL-UIRT harness (Stage 2) |
| `Bench_UI_HudFrame` | 200-element HUD with 20 values changed per frame through `Zenith_UIContext::SetBinding` | `Z2-B-UI-002` | the D-SEL-UIRT harness (Stage 2) |
| `Bench_UI_Composite` | GPU time of UIComposite for the menu at 1080p and 4K, and the S24 | `Z2-B-UI-003` | the D-SEL-UIRT harness (Stage 2) |
| `Bench_UI_ShapeParagraphs` | cold shaping of the corpus per script | `Z2-B-UI-002` | the D-SEL-TEXT harness (Stage 2) |
| `Bench_UI_Memory` | resident host and device bytes for the Game2D fonts, documents and caches (and the locale tables once slice 14 lands) | `Z2-B-UI-001` | the D-SEL-TEXT harness (Stage 2); slice 14 adds the tables |
| `Bench_UI_DynamicGlyphs` | latency from first use to resident for 500 new CJK glyphs | pending row "Dynamic glyph latency" (UI.md "Pending row needs") | the D-SEL-TEXT harness (Stage 2) |
| `Bench_UI_InputToPhoton` | contribution of UI to input-to-photon latency for a button press | `Z2-B-RPROD-027` | slice 10 (Stage 3) |
| `Bench_UI_MenuLocaleSwitch` | the menu of `Bench_UI_MenuFrame` for the frames after a locale switch, until the new tables are committed and every text leaf is re-measured | `Z2-B-UI-002` | slice 14 (Stage 4) |
| `Bench_UI_VirtualList` | 100,000-item list scrolled at 4,000 dp/s | `Z2-B-UI-002` | slice 16 (Stage 4) |

The measured reports are written only by `needs-human` tickets (the two ruling tickets, the stage acceptance runs), never by the harness tickets.

## Acceptance runs

| Run | Stage | Kind |
|---|---|---|
| W-UI2D measured on the reference desktop (both backends) and the S24, Stage 3 rows | 3 | stage acceptance, `needs-human` device steps |
| W-UI2D measured again with the Stage 4 features (bidi, localization, accessibility, virtualization) | 4 | stage acceptance, `needs-human` device steps |
| the four accessible flows' human halves (Narrator, TalkBack, gamepad remapping, reduced motion, each colour-vision mode) | 4 | `Zenith2Acceptance`, `needs-human` |
| HDR10 UI blending review (LIGHT's acceptance item of `Zenith2/Docs/Design/LIGHT/ColourManagement.md → "UI and HDR"`) | 4 | `needs-human` visual review, LIGHT's |
| SA break offsets recorded with ICU4C | 2 (before the D-SEL-TEXT harness and ruling) and each upgrade of the dictionaries or ICU4C | `needs-human` recording |
| Noto shaping goldens recorded with `hb-shape` | 3 (after the D-SEL-TEXT ruling) and each upgrade of HarfBuzz or the fonts | `needs-human` recording |
| D-SEL-TEXT and D-SEL-UIRT measurements on the RTX 3070 desktop and the S24 | 2 (the ruling tickets) | `needs-human` ruling |
| localization review of the flagship slices in `ar`, `ja-JP` and pseudo modes | 4 | `needs-human` visual review |
| UI visual authoring walkthrough with EDIT (Z2-D-079) | 4 | scripted walkthrough, `needs-human` |

## Gate steps and labels

| Group | Step |
|---|---|
| unit, integration, cook, localization, layout and shaping suites | `Test` |
| render tests | `Test` on WARP and lavapipe (UI's `ZenithFlux_Tests` files and the GPU cases of `Zenith_Integration_UI`); `Test_UIRender_CrossBackend` compares the two software devices itself with its derived bound, so UI adds no parity scene to `BackendParity` |
| D-SEL harness smoke | `Test` (`Zenith_Bench_UI --smoke`) |
| shader lints of `UIDraw.slang`, `UIWorldSpace.slang`, `UIShapes.slang` | `ShaderChecks` |
| fuzz smoke | `Test` in `win-asan` |
| public headers | `ApiContractCheck`, `HeaderSelfContained` |
| layering (ZenithUI's declared dependencies, ZenithFlux's UI files including no module header) | `Libgraph` |
| sanitizers | `Sanitizers -Preset win-asan` at stage acceptance (memory errors only); no race detector covers ZenithUI, a `ZENITH_GPU` library absent from `linux-tsan`, so concurrency is evidenced by the lock-order checker and `Test_UIContext_ConcurrentWorldsExtract` |
| benchmarks and acceptance | stage acceptance through `Run-Committed.ps1 -Acceptance` (Z2-D-083) |

Labels: no UI unit, integration or render test needs `needs-gpu`, because the software devices need no physical GPU; the human acceptance items above carry `needs-human`.
