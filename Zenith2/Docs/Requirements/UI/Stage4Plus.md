# UI requirement catalogue: Stages 4 to 6

## Purpose

The second file of the UI catalogue: the requirements whose `Stage` is S4, S5 or S6, in the same format and the same `Z2-R-UI-NNN` numbering as `Zenith2/Docs/Requirements/UI.md`, which holds the purpose of the whole catalogue, the requirements of Stages 1 to 3, the coverage of inventory items (over both files) and the exclusions.

## Requirements

### Z2-R-UI-037 Fixed-extent virtualization

- **Statement:** A fixed-extent list materialises only the viewport plus overscan items at any offset, for any item count up to `uUI_LIST_MAX_ITEMS` (1,048,576); a larger LIST is clamped to it and counted.
- **Covers:** Z2-I-UI-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** Z2-B-UI-002
- **Acceptance evidence:** `Test_UIVirtualList_MaterialisesWindowOnly` (including the clamp), case `ListFixedExtent`, `Bench_UI_VirtualList`; `Test_UINav_VirtualListScrollsBeforeFocus`.
- **Design:** Zenith2/Docs/Design/UI/Layout.md → "Fixed-extent lists"

### Z2-R-UI-038 Variable-extent virtualization

- **Statement:** A variable-extent list keeps prefix sums in a Fenwick tree, measures materialised items, and anchors scrolling when an item above the viewport changes size.
- **Covers:** Z2-I-UI-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UIVirtualList_FenwickMatchesLinearSum`, case `ListVariableExtent`.
- **Design:** Zenith2/Docs/Design/UI/Layout.md → "Variable-extent lists"

### Z2-R-UI-039 Item recycling

- **Statement:** Recycled list items rebind without reallocation and keep focus on the list with the item index.
- **Covers:** Z2-I-UI-15, Z2-I-UI-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UIVirtualList_RecyclingPreservesFocusIndex`.
- **Design:** Zenith2/Docs/Design/UI/Layout.md → "Item recycling"

### Z2-R-UI-052 Bidirectional resolution

- **Statement:** Paragraphs resolve UAX #9 embedding levels with isolates, rules P2 and P3 for AUTO direction, and L1 whitespace handling.
- **Covers:** Z2-I-UI-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UITextCorpus_Bidi`.
- **Design:** Zenith2/Docs/Design/UI/Text.md → "Bidirectional resolution"

### Z2-R-UI-053 Visual reordering

- **Statement:** Lines reorder by rule L2 at shaping-run granularity after line breaking: the line's shaping runs, several of which may share one level, are the units permuted, and each run's shaper output is copied unchanged, so no glyph is reversed twice and consecutive same-level runs of an RTL sentence appear in reversed logical order; a fragment that rule L1 splits off a run with a level of the other parity is re-emitted in its new level's order with its cluster map rebuilt; the cluster map follows the emitted order; paired brackets mirror.
- **Covers:** Z2-I-UI-07, Z2-I-UI-26
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UITextCorpus_Bidi` (Hebrew, Arabic, the mixed-direction multi-glyph case, the RTL sentence with a bold middle word, three level-1 shaping runs, and the L1 trailing-white-space case) and `Test_UICaret_HitTestRoundTrip` (the mixed, bold-middle-word and L1 cases).
- **Design:** Zenith2/Docs/Design/UI/Text.md → "Line layout and reordering"

### Z2-R-UI-054 Caret in bidi text

- **Statement:** Caret positions and hit testing round-trip for LTR, RTL and mixed paragraphs, with logical and visual movement as specified.
- **Covers:** Z2-I-UI-07, Z2-I-UI-26
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UICaret_HitTestRoundTrip`.
- **Design:** Zenith2/Docs/Design/UI/Text.md → "Caret and selection"

### Z2-R-UI-111 Locale tags

- **Statement:** `Zenith_ParseLocale` accepts exactly the BCP 47 subset, canonicalises case and rejects everything else.
- **Covers:** Z2-I-UI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UILoc_ParseLocale`.
- **Design:** Zenith2/Docs/Design/UI/Localization.md → "Locales and negotiation"

### Z2-R-UI-112 Locale negotiation

- **Statement:** The requested list is negotiated against the project's locales by the four steps, and each locale's fallback chain is as specified.
- **Covers:** Z2-I-UI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UILoc_Negotiation`.
- **Design:** Zenith2/Docs/Design/UI/Localization.md → "Locales and negotiation"

### Z2-R-UI-113 String tables and keys

- **Statement:** Keys are namespace-qualified and hashed with `Zenith_Hash64`; one `.zloc` per locale merges the engine namespace; lookup walks the loaded chain in O(log n).
- **Covers:** Z2-I-UI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** Z2-B-UI-001
- **Acceptance evidence:** `Test_UILoc_Negotiation`, `Fuzz_UICookedLocalization`.
- **Design:** Zenith2/Docs/Design/UI/Localization.md → "String tables and keys"; Zenith2/Docs/Design/UI/Documents.md → "Cooked localization table format"

### Z2-R-UI-114 Message compilation

- **Statement:** Every message construct compiles to the documented bytecode, and every syntax and consistency error yields its `LOCALIZATION_ERROR_*` code.
- **Covers:** Z2-I-UI-08, Z2-I-UI-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_UILoc_CompileEveryOpcode`, `Test_UILoc_CompileErrors`, `Fuzz_UIMessageSyntax`.
- **Design:** Zenith2/Docs/Design/UI/Documents.md → "Message syntax"; Zenith2/Docs/Design/UI/Documents.md → "Message bytecode"

### Z2-R-UI-115 Message formatting

- **Statement:** Formatting resolves arguments by name, isolates text arguments, marks missing keys and arguments, and allocates only from the supplied arena or span.
- **Covers:** Z2-I-UI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UILoc_FormatArguments`.
- **Design:** Zenith2/Docs/Design/UI/Localization.md → "Message formatting"; Zenith2/Docs/Design/UI/Localization.md → "Argument formatting"

### Z2-R-UI-116 Plural rules from CLDR

- **Statement:** Cardinal and ordinal rules from the pinned CLDR evaluate every CLDR sample to its category, with exact cases first and offsets applied.
- **Covers:** Z2-I-UI-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UILoc_PluralRules_MatchCLDRSamples`, `Fuzz_UIPluralRule`.
- **Design:** Zenith2/Docs/Design/UI/Localization.md → "Plural rules"; Zenith2/Docs/Design/UI/Documents.md → "Plural rule programs"

### Z2-R-UI-117 Select

- **Statement:** Select arguments choose the keyword case or `other`, which every translation must provide.
- **Covers:** Z2-I-UI-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UILoc_CompileErrors` (missing other) and `Test_UILoc_FormatArguments`.
- **Design:** Zenith2/Docs/Design/UI/Localization.md → "Select"

### Z2-R-UI-118 Rich-text tags

- **Statement:** Message tags produce only emphasis, palette colour, inline prompts, inline sprites and breaks, and translations keep the source's object multiset.
- **Covers:** Z2-I-UI-08, Z2-I-UI-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UILoc_CompileEveryOpcode`, `Test_UIDocumentModel_ValidatesEveryRule` (rule 96).
- **Design:** Zenith2/Docs/Design/UI/Localization.md → "Rich-text tags in messages"

### Z2-R-UI-119 Number formatting

- **Statement:** Numbers format with the locale's separators, grouping, minimum grouping digits and native digits, rounding half to even.
- **Covers:** Z2-I-UI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UILoc_NumberFormatting`.
- **Design:** Zenith2/Docs/Design/UI/Localization.md → "Number formatting"

### Z2-R-UI-120 Locale switching

- **Statement:** A locale switch commits at a frame boundary after its tables load, re-measures all text and re-resolves layout direction, and keeps the previous locale if a table fails to load.
- **Covers:** Z2-I-UI-08, Z2-I-UI-26
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_UILoc_LocaleSwitchAtFrameBoundary`, `Bench_UI_MenuFrame` (switch case).
- **Design:** Zenith2/Docs/Design/UI/Localization.md → "Locale switching at run time"

### Z2-R-UI-121 Font selection per locale

- **Statement:** Font tokens resolve per locale and script with Han glyph forms chosen by locale, and the cook reports uncovered code points per locale.
- **Covers:** Z2-I-UI-06, Z2-I-UI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-TEXT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** `Test_UIFontStack_FallbackOrder`; the `ui.localization` cook report.
- **Design:** Zenith2/Docs/Design/UI/Localization.md → "Font selection per locale"

### Z2-R-UI-122 Extraction sources

- **Statement:** `ui.extract` collects keys from documents, `UI.Loc` literals, localized data-defined properties and sink contributions, and lists dynamic call sites.
- **Covers:** Z2-I-UI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UIExtraction_CollectsEverySource` (`ZenithCook_Tests`, `z2-ui-cooked`).
- **Design:** Zenith2/Docs/Design/UI/Localization.md → "Extraction sources"

### Z2-R-UI-123 Extraction report

- **Statement:** The extraction report lists every key's uses, source text and per-locale translation and staleness state.
- **Covers:** Z2-I-UI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UIExtraction_CollectsEverySource`.
- **Design:** Zenith2/Docs/Design/UI/Localization.md → "Extraction report and exchange files"

### Z2-R-UI-124 XLIFF exchange

- **Statement:** XLIFF 2.0 export and import round-trip unchanged locales, record source hashes, reject units that break placeholders and write accepted units through the journaled document write.
- **Covers:** Z2-I-UI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_UIXliff_RoundTrip`, `Fuzz_UIXliff`.
- **Design:** Zenith2/Docs/Design/UI/Localization.md → "Extraction report and exchange files"

### Z2-R-UI-125 Pseudo-localization modes

- **Statement:** Accented, mirrored and long modes transform only text parts of messages and keep arguments, tags and plural selection intact.
- **Covers:** Z2-I-UI-24, Z2-I-UI-25, Z2-I-UI-26
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UILoc_PseudoModes`.
- **Design:** Zenith2/Docs/Design/UI/Localization.md → "Pseudo-localization and stress modes"

### Z2-R-UI-126 Long-string stress

- **Statement:** Every engine document lays out under each pseudo mode at text scale 2.0 with no overflowing text leaf.
- **Covers:** Z2-I-UI-25
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UILoc_StressAllEngineDocuments`, `Test_UILayoutGolden_PseudoLong`.
- **Design:** Zenith2/Docs/Design/UI/Localization.md → "Localization validation"; Zenith2/Docs/Design/UI/Tests.md → "Layout golden suite"

### Z2-R-UI-128 Localization validation

- **Statement:** The cook enforces rules 50, 82, 92, 96, 97 and 98 with their severities.
- **Covers:** Z2-I-UI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_UIDocumentModel_ValidatesEveryRule` (localization rules).
- **Design:** Zenith2/Docs/Design/UI/Localization.md → "Localization validation"; Zenith2/Docs/Design/UI/Documents.md → "Validation rules"

### Z2-R-UI-129 Corrupt bytecode is safe

- **Statement:** A malformed compiled message formats as missing without crashing.
- **Covers:** Z2-I-UI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_UILoc_InterpreterRejectsCorruptBytecode`, `Fuzz_UICookedLocalization`.
- **Design:** Zenith2/Docs/Design/UI/Documents.md → "Message bytecode"

### Z2-R-UI-130 Accessibility settings keys

- **Statement:** The accessibility and caption keys exist with their ranges and defaults and apply at the next prologue; `ui.access.reducedMotion`, `ui.access.highContrast` and `ui.access.textScale` take the value of PLAT's `platform.accessibility.*` preference key whenever their own winning provenance is the engine default (no game, user or command-line value), so an untouched setting follows the OS and a player's setting always wins.
- **Covers:** Z2-I-UI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_UIModule_StartSequence` (key registration); `Test_UIA11yBridge_PublishesWithinPlatformLimits` (an OS preference change reaches the setting only while no layer above the engine default holds a value for `ui.access.reducedMotion`); `Test_UIEvents_ReplayIdentical` with settings changed. The preference case's ticket is blocked by Z2T-PLAT-105, which carries the `os:accessibility` platform-layer source that fills `platform.accessibility.*`.
- **Design:** Zenith2/Docs/Design/UI/Accessibility.md → "Accessibility settings"; Zenith2/Docs/Design/PLAT/Accessibility.md → "Preference keys and the platform layer"

### Z2-R-UI-131 Engine accessibility settings screen

- **Statement:** The engine ships an accessibility settings screen built only from config-bound controls with `engine.access.*` strings.
- **Covers:** Z2-I-UI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UIA11y_AllFocusablesReachable` and `Test_UIA11y_TreeAudit` (one `SUBCASE` per audited document, the settings screen included).
- **Design:** Zenith2/Docs/Design/UI/Accessibility.md → "Accessibility settings"

### Z2-R-UI-132 Semantic roles and states

- **Statement:** Each element type maps to the role, states and actions of the roles table.
- **Covers:** Z2-I-UI-18
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UIA11y_RolesAndStates`.
- **Design:** Zenith2/Docs/Design/UI/Accessibility.md → "Roles and states"

### Z2-R-UI-133 Accessible names

- **Statement:** Accessible names derive from label, labelledBy, text content and prompt labels in that order; descriptions from hints; values per role.
- **Covers:** Z2-I-UI-18
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UIA11y_NameDerivationOrder`.
- **Design:** Zenith2/Docs/Design/UI/Accessibility.md → "Names and descriptions"

### Z2-R-UI-134 Stable semantic tree

- **Statement:** Node IDs are stable across frames, the revision changes only on a real change, and no tree is built without a consumer.
- **Covers:** Z2-I-UI-18, Z2-I-UI-19
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-UI-002
- **Acceptance evidence:** `Test_UIA11y_TreeStableIDsAndRevision`, `Test_UIA11y_SkippedWithoutConsumer`.
- **Design:** Zenith2/Docs/Design/UI/Accessibility.md → "Semantic tree"

### Z2-R-UI-135 Screen-reader bridge

- **Statement:** UI implements PLAT's `Zenith_AccessibilityTreeProvider`, registered with `SetTreeProvider` at module start step 10 when `Zenith_Platform::GetAccessibility()` is non-null (publishing an empty view at revision 0, since no world is attached) and unregistered before the module is destroyed; it publishes the input world's tree with `NotifyTreeChanged` under one module-wide revision that increments for each publication, when that context's tree changed or the input world changed or detached (then the new world's tree or an empty one), never under a context's revision; its tree meets PLAT's validation rules V1 to V9 by construction (a pre-order prefix of at most 4,096 nodes, frontmost screen first, the rest counted `ui.a11y.nodes_capped`; texts at most 512 bytes with U+0000 replaced by U+FFFD; no focused node when the focused element is omitted; heading levels copied); a tree larger than the publish buffer is deferred one frame while the buffer grows on the main thread, counted; with an OS screen reader active and the bridge on, the tree and announcements go to PLAT's service and built-in narration is silent; OS-requested actions take the navigation input paths.
- **Covers:** Z2-I-UI-19
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UIA11yBridge_PublishesWithinPlatformLimits`, `Test_UIA11yBridge_RepublishesOnInputWorldChange`, `Test_UIFlow_ScreenReader_Game2D`, `Test_UIFlow_ScreenReader_OpenWorldSlice` and `Test_UIFlow_EngineSettingsRoute` against PLAT's recording service (`Zenith_Integration_UI`, `Test`). The ticket is blocked by PLAT's accessibility-service tickets of the PLAT post-signoff patch (cross-area rows UI5 and P21): the API slice Z2T-PLAT-103 and the core and recording backend Z2T-PLAT-104, which carry `Zenith_AccessibilityService`, `Zenith_AccessibilityTreeProvider` and the recording implementation with its test access (`Zenith2/Docs/Design/PLAT/Accessibility.md`).
- **Design:** Zenith2/Docs/Design/UI/Accessibility.md → "Platform accessibility bridge"; Zenith2/Docs/Design/PLAT/Accessibility.md → "Zenith_AccessibilityService"

### Z2-R-UI-136 Built-in narration

- **Statement:** With no screen reader and narration on, focus changes speak composed utterances through the service's text to speech at the narration rate.
- **Covers:** Z2-I-UI-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UIA11y_NarrationComposition`. The ticket is blocked by PLAT's accessibility-service tickets of the PLAT post-signoff patch (cross-area rows UI5 and P21): the API slice Z2T-PLAT-103 and the core and recording backend Z2T-PLAT-104, which carry `Zenith_AccessibilityService`, `Zenith_AccessibilityTreeProvider` and the recording implementation with its test access (`Zenith2/Docs/Design/PLAT/Accessibility.md`).
- **Design:** Zenith2/Docs/Design/UI/Accessibility.md → "Narration composition"; Zenith2/Docs/Design/PLAT/Accessibility.md → "Zenith_AccessibilityService"

### Z2-R-UI-137 Live regions and announcements

- **Statement:** Live regions speak debounced changes with polite or assertive priority, `UI.Announce` speaks without moving focus, and neither talks over a pending screen-reader utterance.
- **Covers:** Z2-I-UI-19, Z2-I-UI-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UIA11y_LiveRegionsDebounce`, `Test_UIRequests_FocusScrollAnnounce`. The ticket is blocked by PLAT's accessibility-service tickets of the PLAT post-signoff patch (cross-area rows UI5 and P21): the API slice Z2T-PLAT-103 and the core and recording backend Z2T-PLAT-104, which carry `Zenith_AccessibilityService`, `Zenith_AccessibilityTreeProvider` and the recording implementation with its test access (`Zenith2/Docs/Design/PLAT/Accessibility.md`).
- **Design:** Zenith2/Docs/Design/UI/Accessibility.md → "Live regions"; Zenith2/Docs/Design/PLAT/Accessibility.md → "Zenith_AccessibilityService"

### Z2-R-UI-138 Caption cue queue

- **Statement:** Caption cues from commands and cue events fill an 8-cue queue that evicts lowest priority oldest first and expires cues at start plus duration, identically in replays.
- **Covers:** Z2-I-UI-11, Z2-I-UI-23
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_UICaptionQueue_EvictsLowestPriorityOldest`, `Test_UICaptions_FromCommandsAndCueEvents`.
- **Design:** Zenith2/Docs/Design/UI/Accessibility.md → "Caption cues"

### Z2-R-UI-139 Caption presentation

- **Statement:** Captions render in a caption area or the engine caption screen with the size, background, speaker and line rules and the reading-time floor, gated by the subtitle and sound settings.
- **Covers:** Z2-I-UI-11, Z2-I-UI-23
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UICaptions_FromCommandsAndCueEvents` (draw-list assertions).
- **Design:** Zenith2/Docs/Design/UI/Accessibility.md → "Caption presentation"

### Z2-R-UI-140 Reduced motion

- **Statement:** With reduced motion on, transitions apply at once, non-essential animations do not play, essential ones fade opacity only, and scrolling, caret and requests follow the table.
- **Covers:** Z2-I-UI-21
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UIAnimation_ReducedMotionSnapsTransforms`.
- **Design:** Zenith2/Docs/Design/UI/Accessibility.md → "Reduced motion"

### Z2-R-UI-141 Colour-vision palettes

- **Statement:** Colour tokens resolve through the active colour-vision palette, then high contrast, then default; without a palette the Machado correction is applied to theme colours only.
- **Covers:** Z2-I-UI-22
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UIA11y_ColourVisionCorrection`, `Test_UIStyle_PaletteSwitchIsTableSwap`.
- **Design:** Zenith2/Docs/Design/UI/Accessibility.md → "Colour-vision modes and contrast"

### Z2-R-UI-142 Contrast validation

- **Statement:** Rule 63 checks every text colour in every palette against the WCAG thresholds.
- **Covers:** Z2-I-UI-22, Z2-I-UI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UIA11y_ContrastRule`, `Test_UIDocumentModel_ValidatesEveryRule` (rule 63).
- **Design:** Zenith2/Docs/Design/UI/Accessibility.md → "Colour-vision modes and contrast"; Zenith2/Docs/Design/UI/Accessibility.md → "Accessibility validation"

### Z2-R-UI-143 Text scaling

- **Statement:** Text scale multiplies font sizes and em lengths and reflows layout, truncating where configured.
- **Covers:** Z2-I-UI-10, Z2-I-UI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-UIRT (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Case `TextScaleAccessibility`; `Test_UILayoutGolden_TextScaleTwoNoOverflow`.
- **Design:** Zenith2/Docs/Design/UI/Accessibility.md → "UI scaling and text scaling"

### Z2-R-UI-144 Touch target size

- **Statement:** Rule 64 warns on interactive elements below 44 × 44 dp for touch-targeting projects.
- **Covers:** Z2-I-UI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UIDocumentModel_ValidatesEveryRule` (rule 64).
- **Design:** Zenith2/Docs/Design/UI/Accessibility.md → "UI scaling and text scaling"

### Z2-R-UI-145 Accessibility audit

- **Statement:** Every audited document has every focusable reachable by navigation and a semantic tree with no unnamed interactive node.
- **Covers:** Z2-I-UI-18, Z2-I-UI-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UIA11y_AllFocusablesReachable`, `Test_UIA11y_TreeAudit` (`ZenithUI_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/UI/Accessibility.md → "Accessibility validation"

### Z2-R-UI-146 Screen-reader flow

- **Statement:** The Game2D and OpenWorldSlice routes complete with navigation and submit only, every focus change producing a named utterance; the gate runs each route up to its `gateTicks` and the Stage 4 acceptance runs it in full.
- **Covers:** Z2-I-UI-27
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-021
- **Budgets:** none
- **Acceptance evidence:** `Test_UIFlow_ScreenReader_Game2D`, `Test_UIFlow_ScreenReader_OpenWorldSlice` (`Zenith_Integration_UI`, `Test`) and the human Narrator and TalkBack run (`needs-human`, Stage 4 acceptance). The ticket is blocked by PLAT's accessibility-service tickets of the PLAT post-signoff patch (cross-area rows UI5 and P21): the API slice Z2T-PLAT-103 and the core and recording backend Z2T-PLAT-104, which carry `Zenith_AccessibilityService`, `Zenith_AccessibilityTreeProvider` and the recording implementation with its test access (`Zenith2/Docs/Design/PLAT/Accessibility.md`).
- **Design:** Zenith2/Docs/Design/UI/Accessibility.md → "End-to-end accessible flows"; Zenith2/Docs/Design/PLAT/Accessibility.md → "Zenith_AccessibilityService"

### Z2-R-UI-147 Remapped-controls flow

- **Statement:** The routes complete after every route action is rebound through the settings screen, with prompts showing the new bindings.
- **Covers:** Z2-I-UI-28, Z2-I-UI-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-021
- **Budgets:** none
- **Acceptance evidence:** `Test_UIFlow_RemappedControls_Game2D`, `Test_UIFlow_RemappedControls_OpenWorldSlice` and the human gamepad remapping run (`needs-human`, Stage 4).
- **Design:** Zenith2/Docs/Design/UI/Accessibility.md → "End-to-end accessible flows"

### Z2-R-UI-148 Reduced-motion flow

- **Statement:** The routes complete with reduced motion and no transform animation.
- **Covers:** Z2-I-UI-29
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-021
- **Budgets:** none
- **Acceptance evidence:** `Test_UIFlow_ReducedMotion_Game2D`, `Test_UIFlow_ReducedMotion_OpenWorldSlice` and the human reduced-motion run (`needs-human`, Stage 4).
- **Design:** Zenith2/Docs/Design/UI/Accessibility.md → "End-to-end accessible flows"

### Z2-R-UI-149 Colour-blind flow

- **Statement:** The routes complete under each colour-vision mode with every drawn text above the contrast threshold.
- **Covers:** Z2-I-UI-30
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-021
- **Budgets:** none
- **Acceptance evidence:** `Test_UIFlow_ColourVision_Game2D`, `Test_UIFlow_ColourVision_OpenWorldSlice` (one `SUBCASE` per mode) and the human colour-vision run (`needs-human`, Stage 4).
- **Design:** Zenith2/Docs/Design/UI/Accessibility.md → "End-to-end accessible flows"

### Z2-R-UI-150 Flow routes are text

- **Statement:** Accessible-flow routes are committed text route files with a `gateTicks` bound, driven through a PLAT virtual device so input takes the production path; the slice routes are written by GAME's slice tickets and the engine-settings route by UI.
- **Covers:** Z2-I-UI-27, Z2-I-UI-28, Z2-I-UI-29, Z2-I-UI-30
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-021
- **Budgets:** none
- **Acceptance evidence:** The route files under `Zenith2/Tests/UIAccessibility/Routes/` and `Test_UIFlow_EngineSettingsRoute` (`Test`).
- **Design:** Zenith2/Docs/Design/UI/Accessibility.md → "End-to-end accessible flows"

### Z2-R-UI-151 Editor model

- **Statement:** `Zenith_UIEditorModel` supports the reversible edit operations, validation after each edit and canonical save through the document system.
- **Covers:** Z2-I-UI-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_UIEditorModel_EditsReversible`, `Test_UIEditorModel_ValidatesAfterEdit` (`ZenithUI_Tests`) and EDIT's authoring walkthrough (Stage 4 acceptance).
- **Design:** Zenith2/Docs/Design/UI.md → "Editor surfaces"; Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIEditorModel header"

### Z2-R-UI-152 Preview context

- **Statement:** The editor can create a preview context at any surface size, locale, palette, pseudo mode and text scale, open the model's compiled bytes in it with `OpenScreenFromBytes` (validated as the loader validates), and pick elements by hit test.
- **Covers:** Z2-I-UI-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_UIEditorModel_PreviewContextSettings`, `Test_UIContext_OpenScreenFromBytesValidates` (`ZenithUI_Tests`) and EDIT's UI authoring walkthrough (`needs-human`, Stage 4).
- **Design:** Zenith2/Docs/Design/UI.md → "Editor surfaces"; Zenith2/Docs/Design/UI/PublicAPI.md → "Zenith_UIContext header"; Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIEditorModel header"

### Z2-R-UI-153 Widget and property catalogues

- **Statement:** The widget and style-property catalogues describe every element type and property with defaults for the authoring tool.
- **Covers:** Z2-I-UI-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_UIEditorModel_CataloguesComplete` (`ZenithUI_Tests`).
- **Design:** Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIEditorModel header"; Zenith2/Docs/Design/UI/Documents.md → "Style property catalogue"

### Z2-R-UI-160 W-UI2D measured at Stage 4

- **Statement:** W-UI2D is measured again with the Stage 4 features on the reference desktop with both backends and on the S24, and the UI rows hold.
- **Covers:** Z2-I-UI-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-083
- **Budgets:** Z2-B-UI-001, Z2-B-UI-002, Z2-B-UI-003
- **Acceptance evidence:** The stage acceptance run through `Run-Committed.ps1 -Acceptance`, with the device steps `needs-human`.
- **Design:** Zenith2/Docs/Design/UI/Tests.md → "Acceptance runs"; Zenith2/Docs/Design/UI.md → "Rows owned"

### Z2-R-UI-162 Sustained mobile run

- **Statement:** W-UI2D sustained on the S24 and the fuzz targets at full duration pass at Stage 6.
- **Covers:** Z2-I-UI-01
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** Z2-B-UI-002
- **Acceptance evidence:** The Stage 6 acceptance run.
- **Design:** Zenith2/Docs/Design/UI.md → "Stage table"

### Z2-R-UI-172 Accessibility hand-off buffers

- **Statement:** The extractor copies the semantic tree and narration requests into context-owned buffers (a double-buffered tree, a 32-request narration ring) that the next Pump reads, so nothing PLAT or Pump reads lives in the extractor's frame arena.
- **Covers:** Z2-I-UI-18, Z2-I-UI-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UINarration_RingSurvivesArenaReset` (`ZenithUI_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/UI/Accessibility.md → "Narration composition"; Zenith2/Docs/Design/UI/Accessibility.md → "Platform accessibility bridge"

### Z2-R-UI-177 Stage 4 benchmarks

- **Statement:** Slice 14's ticket adds `Bench_UI_MenuLocaleSwitch` and the locale tables of `Bench_UI_Memory`, and slice 16's ticket adds `Bench_UI_VirtualList` (a 100,000-item list scrolled at 4,000 dp/s), each running in the harness's smoke CTest from that ticket on and measured at the Stage 4 acceptance.
- **Covers:** Z2-I-UI-08, Z2-I-UI-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** Z2-B-UI-001, Z2-B-UI-002
- **Acceptance evidence:** The `Zenith_Bench_UI --smoke` CTest (`Test`) from those tickets on; the Stage 4 acceptance measurement (`needs-human` device steps).
- **Design:** Zenith2/Docs/Design/UI/Tests.md → "Benchmarks and harnesses"; Zenith2/Docs/Design/UI.md → "Ticket slices and ordering"

### Z2-R-UI-180 HDR10 and scRGB UI output

- **Statement:** In HDR10 output `Flux_EncodeOutput` returns PQ of the Rec.2020 value at `r.hdr.uiNits` for 1.0, blended in PQ code values, and in scRGB output the linear value × `r.hdr.uiNits` / 80, blended linearly; UI never encodes twice.
- **Covers:** Z2-I-UI-01
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UIRender_HDR10Encoding`, the scRGB case of `Test_UIRender_PremultipliedBlending` (UI's `ZenithFlux_Tests` files, WARP and lavapipe, `Test`).
- **Design:** Zenith2/Docs/Design/UI/Rendering.md → "Colour and HDR output"

### Z2-R-UI-181 Message argument validation rule

- **Statement:** The validator reports rule 51 (`args` naming an argument the source message does not declare, or omitting one it declares) against the source message's compiled argument list.
- **Covers:** Z2-I-UI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_UIDocumentModel_ValidatesEveryRule` (rule 51, `ZenithUI_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/UI/Documents.md → "Validation rules"

### Z2-R-UI-182 Localization cook steps registered

- **Statement:** The `ui.localization` and `ui.extract` cook steps register once through `Zenith_RegisterUICookSteps` with the derived-data key inputs listed, joining the Stage 3 steps.
- **Covers:** Z2-I-UI-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_UICookSteps_RegisterOnce` (`ZenithCook_Tests`, `Test`), extended by the two localization steps.
- **Design:** Zenith2/Docs/Design/UI.md → "Cook steps and derived-data keys"
