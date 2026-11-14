# UI design: the element tree, styles, layout and virtualization

This file is part of the UI design family; the entry document is `Zenith2/Docs/Design/UI.md`. It specifies the runtime the retained UI is built on: the choice D-SEL-UIRT (open, Stage 1/2) makes and what either candidate implements, the element tree of a screen instance, canvas units and scaling, style resolution, the layout algorithm step by step with its complexity, incremental invalidation, large-list virtualization, data-binding evaluation, animations, world-space panels and the conformance cases that pin the semantics. Documents are `Zenith2/Docs/Design/UI/Documents.md`; text layout is `Zenith2/Docs/Design/UI/Text.md`; input and focus are `Zenith2/Docs/Design/UI/Interaction.md`.

## Scope of this file

| Topic | Section |
|---|---|
| D-SEL-UIRT and the two candidates | "Runtime selection" |
| the per-screen element arrays | "Element tree model" |
| units, scale modes, UI scaling, the safe area | "Canvas units and scaling", "Safe area and surface changes" |
| the cascade | "Style resolution" |
| stack, grid, anchored, text, scroll; rounding; complexity | "Layout algorithm" |
| dirty propagation | "Invalidation and incremental layout" |
| lists of up to 1,048,576 items | "Large-list virtualization" |
| right-to-left layout | "Logical direction and mirroring" |
| reading bound values | "Data binding evaluation" |
| transitions and keyframe animations | "Animations and transitions" |
| world-space screens | "World-space panels" |
| the golden cases | "Layout conformance cases" |

## Runtime selection

<!-- elaboration: the shared-shell architecture, which sharpens D-SEL-UIRT -->
D-SEL-UIRT (open, Stage 1/2) is "RmlUi or a custom retained UI" (`Zenith2/Docs/Architecture.md → "Named selection decisions"`). ZenithUI sharpens it, through a shared shell, to **RmlUi's layout and style engine versus ZenithUI's own**: whichever wins, the public API of `Zenith2/Docs/Design/UI/PublicAPI.md`, the authoring documents, the cooked formats, text, rendering, input, focus, navigation, the semantic accessibility tree, large-list virtualization, world-space panels, RTL mirroring ("Logical direction and mirroring"), localization and the script surface are ZenithUI's own code in both candidates, so every capability DecisionLog's acceptance rule lists is provided by the shell and both candidates provide it equally; the decision selects only the code behind `Zenith_UIRuntimeBackend` that resolves styles for and computes the boxes of each screen's element tree. The sharpening is requested for DecisionLog.md (`Zenith2/Docs/Design/UI.md → "Cross-area reconciliation"`) and stays the human's to overrule (option (b) of the question: RmlUi as the full runtime behind ZenithUI's API).

The acceptance rule is neutral; no candidate is the default (`Zenith2/Docs/Design/UI.md → "Selection decisions UI owns"` gives it in full): the selection set is every **CSS** and **ZenithUI semantics** case of "Layout conformance cases", which `Test_UIRuntimeSelection_ConformanceBothCandidates` runs on both candidates and reports without judging; a ZenithUI-semantics case encodes behaviour CSS flexbox and grid do not have, so a candidate may meet it through a documented shim (a pre- or post-layout correction inside its backend directory, listed in the harness report with its cost) or fails it; a candidate failing a case of the set is out; then the candidates that meet both `Z2-B-UI-002` and `Z2-B-UI-003` in the W-UI2D-scale benchmark on the S24 beat those that do not, then the lower `Z2-B-UI-002` time, then binary size. **Shell** cases are not in the selection set: they run on each compiled candidate in `Test_UILayoutGolden_AllCases`, which while D-SEL-UIRT is open asserts only what the shell computes, relative to that candidate's own layout (`Zenith2/Docs/Design/UI/Tests.md → "Layout golden suite"`), because every shell case's rectangles also pass through the candidate's `Layout`; their absolute expectations are compared by the selection test and reported as class `shell`, recorded and not judged.

### What both candidates implement

The internal interface both candidates implement, `Zenith2/Engine/ZenithUI/Internal/Runtime/Zenith_UIRuntimeBackend.h`:

```cpp
// Internal: one implementation is compiled, per the D-SEL-UIRT ruling.
class Zenith_UIRuntimeBackend
{
public:
	virtual ~Zenith_UIRuntimeBackend() = default;
	virtual Zenith_Result<uint32, Zenith_UIError> CreateScreen(const Zenith_UIDocumentAsset& xDocument,
		Zenith_UIElementTable& xTable) = 0;                       // fills the element table; returns the backend screen ID
	virtual void DestroyScreen(uint32 uBackendScreen) = 0;
	virtual void ResolveStyles(uint32 uBackendScreen, Zenith_UIElementTable& xTable, const Zenith_UIStyleInputs& xInputs) = 0;
	virtual void Layout(uint32 uBackendScreen, Zenith_UIElementTable& xTable, const Zenith_UICanvas& xCanvas,
		Zenith_UITextMeasurer& xMeasurer) = 0;                    // writes boxes for elements with LAYOUT_DIRTY subtrees
	virtual void OnElementsInserted(uint32 uBackendScreen, uint32 uFirst, uint32 uCount) = 0;   // list items
	virtual void OnElementsRemoved(uint32 uBackendScreen, uint32 uFirst, uint32 uCount) = 0;
};
```

- `Zenith_UIElementTable` is the engine-owned structure of "Element tree model": the backend reads types, styles and state flags from it and writes computed styles and boxes into it, so hit testing, draw lists, focus and the semantic tree read one representation regardless of the backend.
- `Zenith_UITextMeasurer` is ZenithUI's text engine behind the four measure calls of "Text leaves", so text is shaped, broken and drawn by ZenithUI's text stack in both candidates (`Zenith2/Docs/Design/UI/Text.md → "Paragraph pipeline"`).
- The normative semantics are this file's: every case of "Layout conformance cases" produces the expected rectangles within 0.01 canvas units, and the D-SEL-UIRT harness runs that suite against each candidate (`Zenith2/Docs/Design/UI.md → "Selection decisions UI owns"`).
- Style resolution is shared. `Zenith_UIStyleResolver` is shell code (`Internal/Zenith_UIStyleResolver.h`, `.cpp`), never part of a candidate: before the backend's `ResolveStyles`, the shell resolves every catalogue property of every `STYLE_DIRTY` element through the nine steps of "Cascade order" into `Zenith_UIComputedStyle` (`Zenith2/Docs/Design/UI/PublicAPI.md → "Zenith_UIStyle header"`, one field per property), which the paint path, hit testing, focus, the semantic tree and the animation sampler read, whichever candidate is compiled. A candidate's `ResolveStyles` only hands the layout-affecting properties to its own engine: the custom runtime reads them from the computed style ("The custom runtime"); the RmlUi integration runs RmlUi's own cascade for them ("The RmlUi integration").
- Ownership: slice 7 of `Zenith2/Docs/Design/UI.md → "Ticket slices and ordering"` writes the shell resolver, both implementations of this interface (`Internal/Runtime/Custom/`, `Internal/Runtime/RmlUi/`) and the candidate selection that the per-candidate `SUBCASE`s, the selection test and `Zenith_Bench_UI --candidate <name>` use; the D-SEL-UIRT harness ticket writes no candidate code.

The internal types the interface names, declared in `Zenith2/Engine/ZenithUI/Internal/Zenith_UIElementTable.h` and `Internal/Layout/Zenith_UILayoutTypes.h` (Internal; both candidates and the shell include them):

```cpp
// Internal/Zenith_UIElementTable.h — the SoA table of "Element tree model"; every array has m_uCapacity entries.
struct Zenith_UIMeasureCache
{
	float m_fMinContentWidth = -1.0f;    // < 0: not measured since the last MEASURE_DIRTY
	float m_fMaxContentWidth = -1.0f;
	float m_afHeightForWidthKey[2] = { -1.0f, -1.0f };   // the two most recent widths, quantised to 1/64 unit
	float m_afHeightForWidth[2] = { 0.0f, 0.0f };
	float m_fFirstBaseline = 0.0f;
};
struct Zenith_UIElementTable
{
	uint32 m_uCount = 0;                // rows in use (document elements + materialised list blocks)
	uint32 m_uCapacity = 0;             // document element count + item pool rows
	uint8* m_auType = nullptr;          // Zenith_UIElementType
	uint32* m_auParent = nullptr;
	uint32* m_auFirstChild = nullptr;
	uint32* m_auNextSibling = nullptr;
	uint32* m_auCookedIndex = nullptr;
	uint32* m_auFlags = nullptr;        // "Element tree model" bits
	uint16* m_auState = nullptr;        // Zenith_UIStateFlags
	Zenith_UIComputedStyle* m_axComputed = nullptr;
	Zenith_UIRect* m_axBorderBox = nullptr;
	Zenith_UIRect* m_axContentBox = nullptr;
	Zenith_UIRect* m_axClip = nullptr;
	Zenith_UIMeasureCache* m_axMeasure = nullptr;
	uint32* m_auTextLayout = nullptr;
	Zenith_Vector2* m_axScroll = nullptr;
	sint32* m_aiItem = nullptr;
	uint8* m_auResolvedDirection = nullptr;   // Zenith_UILayoutDirection after "Logical direction and mirroring" step 1
	void* m_pBlock = nullptr;            // the single allocation every array points into
};

// Internal/Layout/Zenith_UILayoutTypes.h
struct Zenith_UIStyleInputs
{
	const Zenith_UIDocumentAsset* m_pxDocument = nullptr;   // STYLES, PALETTES, METRICS sections
	uint32 m_uPaletteIndex = 0;          // the active palette order's first palette (Documents.md "Palettes and tokens")
	uint32 m_uFallbackPaletteMask = 0;   // bit i: palette i is consulted after the first (high contrast, default)
	float m_fTextScale = 1.0f;            // ui.access.textScale
	float m_fFocusRingScale = 1.0f;       // ui.access.focusRingScale
	bool m_bRightToLeftLocale = false;    // Zenith_Localization::IsRightToLeft(), for layoutDirection auto
	uint64 m_ulPresentationTimeUs = 0;   // samples transitions and animations
};
struct Zenith_UICanvas
{
	Zenith_UIRect m_xRootBox;             // canvas units: the canvas inset by the safe area when canvas.safeArea is true
	float m_fPixelsPerUnit = 1.0f;        // s of "Canvas units and scaling"
	bool m_bWorldPanel = false;
};
class Zenith_UITextMeasurer
{
public:
	virtual ~Zenith_UITextMeasurer() = default;
	// uElement: a row of the table; every call lays out (or reuses from the context's text cache) the element's paragraph
	virtual float MinContentWidth(const Zenith_UIElementTable& xTable, uint32 uElement) = 0;
	virtual float MaxContentWidth(const Zenith_UIElementTable& xTable, uint32 uElement) = 0;
	virtual float HeightForWidth(const Zenith_UIElementTable& xTable, uint32 uElement, float fWidth) = 0;
	virtual float FirstBaseline(const Zenith_UIElementTable& xTable, uint32 uElement, float fWidth) = 0;
};
```

The shell's `Zenith_UIContextTextMeasurer` (`Internal/Text/Zenith_UITextMeasurer.cpp`) implements the measurer over `Zenith_TextLayoutEngine` with the context's cache partition; every call is O(paragraph) on a cache miss and O(1) on a hit.

### The custom runtime

`Zenith2/Engine/ZenithUI/Internal/Runtime/Custom/`: `Zenith_UICustomBackend.cpp` (the interface), `Zenith_UILayoutStack.cpp`, `Zenith_UILayoutGrid.cpp`, `Zenith_UILayoutAnchored.cpp`, `Zenith_UILayoutPass.cpp` ("Layout algorithm"). No third-party code and no style resolver: its `ResolveStyles` resolves nothing itself and its layout reads the layout-affecting fields of the shell's computed style (`m_axComputed`) directly, so the custom runtime's style engine is the shell's.

### The RmlUi integration

<!-- elaboration: the RmlUi integration as a layout and style engine -->
`Zenith2/Engine/ZenithUI/Internal/Runtime/RmlUi/`, owning the RmlUi manifest entry (pinned at the ruling, `THIRD_PARTY RmlUi`, usage runtime, owner ZenithUI):

| RmlUi interface | ZenithUI implementation |
|---|---|
| `Rml::SystemInterface` | `GetElapsedTime` from the context's presentation time; `LogMessage` to the `ui.runtime` log category; `TranslateString` returns the input (ZenithUI localizes before text reaches RmlUi) |
| `Rml::FileInterface` | reads only the RML and RCSS strings of the document's RMLUI section (`Zenith2/Docs/Design/UI/Documents.md → "UI document sections"`); any other path fails |
| `Rml::RenderInterface` | not used for drawing: ZenithUI builds draw lists from the element table ("Draw list construction" of Rendering.md); the integration renders no RmlUi geometry, and its `CompileGeometry` records nothing |
| `Rml::FontEngineInterface` | every text element is an RmlUi custom element (`Zenith_UIRmlTextElement`, registered through an `Rml::ElementInstancer`) whose intrinsic size comes from `Zenith_UITextMeasurer`; RmlUi's own font engine is never initialised |
| element boxes | after `Rml::Context::Update`, each element's border box (`Rml::Element::GetBox`, `GetAbsoluteOffset`) is copied into the element table |

<!-- elaboration: RmlUi's style engine runs its own cascade for the layout-affecting properties, under the "layout and style engine" sharpening of D-SEL-UIRT -->
**Cook-time input.** The RMLUI section (`Zenith2/Docs/Design/UI/Documents.md → "UI document sections"`, kind 15) is cooked into every document while D-SEL-UIRT is open and after a ruling that selects RmlUi; its writer is `Internal/Runtime/RmlUi/Zenith_UIRmlUiSection.cpp` (no RmlUi header: it writes text), which `Zenith_CompileUIDocument` calls while the RmlUi candidate is compiled. It holds one RML element per element (`<div>` for panels and controls, `<zenith-text>` for text), an `id` attribute carrying the element index for lookup only (no RCSS rule selects by it), and a `class` attribute, and an RCSS style sheet that reproduces cascade steps 1 and 3 to 6 of "Cascade order" for the layout-affecting properties (IDs 1 to 29 and 40 to 48) through the mapping below, using only class selectors of equal specificity (0, 1, 0) and pseudo-classes, so that source order and specificity give exactly ZenithUI's order:

1. `.z`, on every element: the catalogue defaults of the mapped properties (step 1), among them `display: flex; flex-direction: column; flex-grow: 0; flex-shrink: 1; flex-basis: auto; min-width: 0; min-height: 0`.
2. `.k<m>`, one per distinct sequence of an element's `typeDefaults` classes followed by its `class` list: the cook merges the base blocks of that sequence in order (steps 3 and 4) into one rule, because CSS ignores the order of names in a `class` attribute while ZenithUI's steps apply them in order, so two elements with classes `a b` and `b a` get two composite rules.
3. `.e<index>`, one per element with an inline block (step 5), emitted after every `.k<m>`.
4. For each state in the fixed priority order of step 6 (focused, focusVisible, hover, pressed, checked, selected, editing, invalid, disabled): every `.k<m>:z-<state>` (the merged state blocks of that class sequence), then every `.e<index>:z-<state>` (the inline state block). A pseudo-class adds (0, 1, 0), so every state rule outranks every base rule and, among state rules, source order applies the higher-priority state last, as step 6 does. The custom pseudo-class names `z-focused`, `z-focus-visible`, `z-hover`, `z-pressed`, `z-checked`, `z-selected`, `z-editing`, `z-invalid` and `z-disabled` keep RmlUi's own `:hover`, `:active` and `:focus` (which RmlUi would set from input it never receives) out of the cascade.

No rule uses an `id` selector and no element carries a `style` attribute, whose specificity would put step 5 above step 6. A layout-affecting property whose authored value is a metric token or a binding is not emitted; it is pushed at run time.

**Run-time input.** Every frame, after the shell's resolver ran, the integration's `ResolveStyles` pushes for each `STYLE_DIRTY` element what no cook-time sheet can know: the nine state flags as pseudo-classes (`Rml::Element::SetPseudoClass("z-<state>", bSet)`); the member `visible` false as the inline property `display: none` (`SetProperty`, removed with `RemoveProperty` when it is true again); and, as inline properties (`SetProperty`, which outrank every rule as cascade steps 7 to 9 outrank steps 1 to 6), every layout-affecting value the shell resolved from a token, a binding, an active transition or animation (step 7) or the margin exchange of "Logical direction and mirroring" step 2, removing each when its source stops applying. RmlUi's `Rml::Context::Update` then cascades and lays out; the shell's computed style stays the one every other subsystem reads, and a disagreement between RmlUi's resolved layout values and the shell's is visible only as rectangles in the conformance cases.

`grid` containers, which RCSS lacks, are custom elements whose children the integration positions with the custom grid algorithm of "Grid layout". `Test_UIRmlUiSection_RcssCascadeOrder` checks the generated sheet's rule order, selectors and pseudo-class names (`Zenith2/Docs/Design/UI/Tests.md → "Style and layout"`).

| ZenithUI property | RCSS | Notes |
|---|---|---|
| `width`, `height`, `min*`, `max*` | same names, `px` for canvas units (the RmlUi context's dp ratio is 1), `%` unchanged | `minContent`/`maxContent` become `auto` on a `display: inline-block` wrapper; the conformance cases that need them are the shim's or fail (the harness reports which) |
| `margin`, `padding`, `borderWidth` | `margin`, `padding`, `border-width` | |
| `cornerRadius` | `border-radius` | |
| `layout: stack` + `direction` | `display: flex; flex-direction` | |
| `justify`, `alignItems`, `alignSelf`, `wrap`, `gap` | `justify-content`, `align-items`, `align-self`, `flex-wrap`, `gap` | |
| `grow`, `shrink`, `basis` | `flex-grow`, `flex-shrink`, `flex-basis` | |
| `position: anchored` + anchors | `position: absolute` with `left`/`top`/`right`/`bottom` computed by the cook from anchors and offsets | pivot placement of auto-sized anchored elements needs the integration's post-layout correction pass (a shim, reported) |
| `minWidth`/`minHeight` default 0 | `min-width: 0; min-height: 0` emitted on every flex item | CSS's automatic minimum (`min-width: auto`) is switched off explicitly, so ZenithUI's no-automatic-minimum rule is met without a shim |
| `layoutDirection` | not emitted | the shell's mirroring pass applies to both candidates ("Logical direction and mirroring"); its step 2 margin exchange reaches RmlUi as inline margins |
| `clip` | `overflow: hidden` | |
| `zIndex` | not emitted | paint order is the shell's draw list (Rendering.md "Paint order") |
| visual and text properties | not emitted | resolved by the shell's cascade alone ("Style resolution"); ZenithUI draws and measures text |

## Element tree model

Each screen instance owns a `Zenith_UIElementTable` (`Internal/Zenith_UIElementTable.h`): structure-of-arrays storage, one row per element, allocated once at instantiation from the context's heap with capacity = the document's element count plus the list item pool (see "Item recycling"):

| Array | Element type | Meaning |
|---|---|---|
| `m_auType` | `uint8` | `Zenith_UIElementType` |
| `m_auParent`, `m_auFirstChild`, `m_auNextSibling` | `uint32` | tree links (0xFFFFFFFF none) |
| `m_auCookedIndex` | `uint32` | the cooked element record it was instantiated from |
| `m_auFlags` | `uint32` | bits 0–3 dirty (STYLE, MEASURE, LAYOUT, PAINT), 4 visible, 5 enabled, 6 focusable, 7 layout boundary, 8 list item, 9 inert (inside a closed or invisible ancestor) |
| `m_auState` | `uint16` | `Zenith_UIStateFlags` |
| `m_axComputed` | `Zenith_UIComputedStyle` | "Style resolution" output of the shell's resolver: one field per catalogue property, the only per-element style storage, read by both candidates' layouts and by every other subsystem |
| `m_axBorderBox`, `m_axContentBox`, `m_axClip` | `Zenith_UIRect` | layout output, canvas units, relative to the screen canvas |
| `m_axMeasure` | `Zenith_UIMeasureCache` | the last intrinsic sizes and height-for-width answers |
| `m_auTextLayout` | `uint32` | the element's text layout slot in the context's text cache partition |
| `m_axScroll` | `Zenith_Vector2` | scroll offsets (scroll and list elements) |
| `m_aiItem` | `sint32` | the list item index of a list-item element, −1 otherwise |
| `m_auResolvedDirection` | `uint8` | the element's resolved layout direction, LTR or RTL ("Logical direction and mirroring") |

The element order is depth-first pre-order, which is also paint order and tab order's tie-break, so a traversal is a linear scan with the next-sibling links skipping subtrees.

<!-- informative -->
Zenith's canvas owned elements in two lists (all elements and roots) and nesting needed a dedicated reparent call; adding a child without it leaked or walked twice, and writing visibility every frame kept layout groups permanently dirty. Zenith2 instantiates the whole tree from the cooked document in one allocation, never reparents at run time (instances and list items are pre-sized blocks), and every binding application compares the new value with the old before setting a dirty bit ("Data binding evaluation").
<!-- /informative -->

## Canvas units and scaling

| Mode | Pixels per canvas unit `s` | Canvas size |
|---|---|---|
| `dp` | `s = m_fDisplayScale × ui.scale` | `surface pixels / s` |
| `reference` | `sw = W / referenceWidth`, `sh = H / referenceHeight`, `s = exp((1 − match) × ln sw + match × ln sh) × ui.scale` | `surface pixels / s` |
| world panel | `s = 1` (canvas pixels are canvas units) | `m_xWorldSizeMetres × m_fPixelsPerMetre` |

- `m_fDisplayScale` is PLAT's (`Zenith2/Docs/Design/PLAT/Platform.md → "Units points pixels and dp"`); UI scaling multiplies it by `ui.scale` (0.75 … 2.0, Z2-I-UI-10; `Zenith2/Docs/Design/UI.md → "Configuration keys"`), as PLAT's text states.
- Text sizes additionally multiply by `ui.access.textScale` (1.0 … 2.0) for elements whose `textScalePolicy` is `scale` (the default); the layout sees the scaled font size, so containers grow with the text.
- A change of `s` (a display-scale record, a UI-scale setting change, a surface resize) marks every root of the context `MEASURE_DIRTY | LAYOUT_DIRTY` and invalidates the text cache partition's pixel-snapped entries.

## Style resolution

### Cascade order

For one element, `Zenith_UIStyleResolver::Resolve` (shell code, `Internal/Zenith_UIStyleResolver.cpp`, run for both candidates) builds `Zenith_UIComputedStyle` (every catalogue property, `Zenith2/Docs/Design/UI/PublicAPI.md → "Zenith_UIStyle header"`) by applying, in this order, each step overriding a property set by a preceding one:

1. property defaults (the catalogue's Default column, `Zenith2/Docs/Design/UI/Documents.md → "Style property catalogue"`);
2. inherited properties: the parent's computed value for every property whose Inherit column is "yes" (opacity multiplies);
3. the theme's `typeDefaults` classes for the element's type, in order, base blocks;
4. the element's `class` list, in order, base blocks;
5. the inline `style` block;
6. for each state in the fixed priority order focused, focusVisible, hover, pressed, checked, selected, editing, invalid, disabled whose flag is set: the classes' state blocks in class order, then the inline state block;
7. animated values: active transitions and the active keyframe animation ("Animations and transitions");
8. accessibility adjustments: font size × text scale (policy `scale`), focus-ring width × `m_fFocusRingScale`;
9. direction: `layoutDirection` (inherited) resolves `auto` to the parent's resolved direction, and at a screen root to RTL when `m_bRightToLeftLocale` is set and LTR otherwise; the result, `UI_LAYOUT_DIRECTION_LTR` or `UI_LAYOUT_DIRECTION_RTL` (`Zenith2/Docs/Design/UI/PublicAPI.md → "Zenith_UIStyle header"`), is stored in `Zenith_UIComputedStyle::m_eLayoutDirection` and in `m_auResolvedDirection` (`zIndex`, 30, and `mirrorInRtl`, 84, resolve through steps 1 to 7 like any other non-inherited property, into `m_iZIndex` and `m_bMirrorInRtl`).

Resolution cost is O(blocks applied × properties per block). The computed style is cached; `STYLE_DIRTY` is set when the element's state flags, class list, inline block (a binding feeding a style member), palette selection, or an ancestor's inherited value changes (a theme change reaches a context only as a re-cooked document, whose hot reload re-opens the screen).

### Inheritance

Inherited properties are the text properties (96–114), `layoutDirection` (49), `pointerEvents` and `focusRing*`; `opacity` composes multiplicatively down the tree; every other property is per element.

### State variants

State flags are maintained by Interaction.md (hover, pressed, focus, focusVisible, editing), by bindings (checked, selected, disabled through `enabled`) and by validation (`invalid`: a text field whose `purpose` rejects its content). A state change sets `STYLE_DIRTY` on the element and, when a state block changes an inherited property, on its descendants.

### Tokens and palettes at resolution

- Colour tokens resolve through the active palette order of `Zenith2/Docs/Design/UI/Documents.md → "Palettes and tokens"`; the context's palette selection is an index into the PALETTES section, so a colour-vision or high-contrast switch is `STYLE_DIRTY` on every element with no document reload.
- Metric tokens resolve from the METRICS section and are layout-affecting.
- An unresolvable token (impossible after cook validation) resolves to the property default and counts `ui.style.token_unresolved`.

## Layout algorithm

### Pass structure

`Zenith_UILayoutPass::Run(screen)` performs, for each layout boundary marked `LAYOUT_DIRTY`, in pre-order:

1. **Measure** (bottom-up, memoized): for each element in the boundary's subtree with `MEASURE_DIRTY`, compute its intrinsic min-content and max-content border-box sizes (§ "Intrinsic sizes"), and clear its height-for-width cache.
2. **Arrange** (top-down): starting at the boundary with its fixed border box, lay out each container's children with the algorithm of its `layout` (stack, grid, overlay) and anchored children by "Anchored layout", writing border, content and clip boxes, recursing into children with their final sizes.
3. **Finish**: clear dirty bits, update scroll extents, mark `PAINT_DIRTY`.

A screen root is always a layout boundary with the canvas (minus the safe area for screen-space screens) as its border box.

### Size resolution

For an element E with containing block content size (cw, ch) — the parent's content box, or the grid area, or the anchored rectangle:

| Specified | Resolved width (height symmetric with ch) |
|---|---|
| length `L` | `L` |
| `p%` | `p / 100 × cw`; when cw is indefinite (an auto-sized parent's measure pass), treated as `auto` |
| `auto` | context-dependent: stretched by the parent's alignment, or fit-content = `min(max-content, max(min-content, available))` |
| `minContent`, `maxContent` | the intrinsic size |

then clamped: `w = max(minWidth, min(maxWidth, w))` (min wins over max). Margins, padding and borders resolve horizontally against cw and vertically against ch; `aspectRatio` R, when set and exactly one of width and height is auto, sets the auto one to `other × R` (width) or `other / R` (height) before clamping.

### Intrinsic sizes

| Element | Min-content width | Max-content width | Height for width w |
|---|---|---|---|
| `text`, and a control's text label | the paragraph's widest unbreakable segment (`m_fMinContentWidth`) | the paragraph laid out unwrapped (`m_fMaxContentWidth`) | the paragraph laid out at w (Text.md "Paragraph pipeline"), cached per element keyed by w quantised to 1/64 unit |
| `image` | 0 | the sprite or texture rectangle in texels × 1 unit per texel | `w × rect height / rect width` |
| stack, row direction | sum of children's min-content margin widths plus gaps when not wrapping; the largest child min-content when wrapping | sum of children's max-content margin widths plus gaps | the arranged height at w |
| stack, column direction | the largest child min-content margin width | the largest child max-content margin width | sum of children's heights at w plus gaps |
| grid | sum over columns of each column's min-content track size plus gaps | sum of max-content track sizes plus gaps | the row track sum at w |
| overlay | the largest child | the largest child | the largest child height at w |
| `slider`, `progress`, `toggle` | the theme's control metric `control.minWidth` (default 48) | the same | `control.height` (default 32) |
| `textField` | `control.minWidth` | the content's max-content or `control.textFieldWidth` (default 240), the larger | one line height (multiline: `maxLines` or 4 line heights) |
| `scroll`, `list` | 0 along scroll axes | the content's max-content along non-scroll axes | as stack |

Padding and borders are added to every row; an anchored child contributes nothing to its parent's intrinsic size.

### Stack layout

For a stack container with direction row (column swaps width and height, and x and y; the reverse directions place items from the main end), inner main size `A` (the content width, possibly indefinite), and in-flow visible children `c₁ … cₙ` in document order:

1. **Base size**: for each child, `B_i` = its `basis` when it is a length or a percentage of a definite A; else its specified main size when definite; else its max-content width. Its hypothetical size `H_i = clamp(B_i, minWidth_i, maxWidth_i)`. The default `minWidth` is 0, so no automatic minimum applies (an author sets `minWidth: "minContent"` to keep a label unshrunk); this is CSS flexbox with `min-width: 0` on every item, which RCSS expresses (the integration emits it).
2. **Lines**: with `wrap` false, one line holds every child. With `wrap` true and definite A, a greedy pass opens a new line when adding `H_i + margins_i` (plus a gap when the line is not empty) would exceed A; every line holds at least one child.
3. **Flexible lengths**, per line, with `F = A − Σ(H_i + margins_i) − gaps`: when F > 0 and Σ grow > 0, every child gets `H_i + F × grow_i / Σgrow`; a child whose result exceeds its max is frozen at its max and the loop repeats with the unfrozen children over the remaining free space; when F < 0 and Σ(shrink_i × B_i) > 0, every child gets `H_i + F × (shrink_i × B_i) / Σ(shrink × B)`; a child that falls below its min is frozen at its min and the loop repeats. The loop ends when an iteration freezes nothing, after at most n iterations. When A is indefinite, F is 0.
4. **Cross sizes**: a child whose `alignSelf` (or the container's `alignItems` when `auto`) is `stretch` and whose cross size is auto gets the line's cross size minus its margins, clamped; otherwise its cross size is its specified size, or its height for its resolved main size.
5. **Line cross size**: with one line and a definite inner cross size, that size; otherwise the largest child cross margin size of the line.
6. **Main positions**: per line, the leftover `L = A − Σ(final main + margins) − gaps` is distributed by `justify`: `start` (0 before), `centre` (L/2 before), `end` (L before), `spaceBetween` (L/(n−1) between items; start when n = 1), `spaceAround` (L/n around each, half at the ends), `spaceEvenly` (L/(n+1) everywhere); a negative L behaves as `start` for the space modes and is applied as stated for `centre` and `end` (overflow on both sides or the start side).
7. **Cross positions**: within its line, a child is placed at the line start (`start`, `stretch`), centre, end, or by `baseline`: every baseline child of the line is aligned on the largest first-baseline offset (text: the first line's baseline; containers: their first baseline child's, recursively; others: the bottom of the content box).
8. **Lines** stack along the cross axis from the content start, separated by `rowGap`.
9. **Auto container size**: an auto main size is `Σ final main + margins + gaps` (single line) or the widest line; an auto cross size is the sum of line cross sizes plus row gaps; both clamped by the container's own min and max.

### Grid layout

For a grid container with column tracks `T₁ … T_m` (`gridColumns`) and rows from `gridRows` followed by implicit `auto` rows:

1. **Placement**: children with explicit `gridColumn` and `gridRow` occupy those cells with their spans; the remaining children are placed in document order into the first free cell area of their span scanning rows top-down and columns left-right from a cursor that never moves backwards (no dense packing); a column span larger than m is clamped to m.
2. **Column sizing**: fixed tracks take their length; `minContent`, `maxContent` and `auto` tracks take the largest min-content (`minContent`) or max-content (`maxContent`, `auto`) width of the span-1 children they hold; a child spanning several tracks with no fr track adds the shortfall `max(0, its size − Σ spanned sizes − gaps)` divided equally among its spanned auto tracks; the free space `A − Σ non-fr tracks − gaps` (0 when negative or A indefinite) is divided among fr tracks in proportion to their factors.
3. **Row sizing**: as columns, with each child's height for its column area's width.
4. **Item placement**: each child's area is the union of its cells; within it the child is stretched or aligned by its `alignSelf` on both axes (ZenithUI has one alignment property where CSS grid has `align-self` and `justify-self`; this is ZenithUI semantics, and the cases that rely on it are marked so in "Layout conformance cases").

Complexity O(children × span + tracks).

### Anchored layout

A child with `position: anchored` is removed from its parent's flow and placed against the parent's content box P (origin `(Px, Py)`, size `(Pw, Ph)`), per axis (x shown):

| Case | Width | Left edge |
|---|---|---|
| `width` is not auto | the resolved width | `Px + anchorMin.x × Pw + offsetMin.x − pivot.x × width` |
| `width` auto and `anchorMin.x < anchorMax.x` (stretch) | `(Px + anchorMax.x × Pw + offsetMax.x) − (Px + anchorMin.x × Pw + offsetMin.x)`, at least 0 | `Px + anchorMin.x × Pw + offsetMin.x` |
| `width` auto and `anchorMin.x = anchorMax.x` | fit-content against `Pw` | `Px + anchorMin.x × Pw + offsetMin.x − pivot.x × width` |

Overlay containers place every in-flow child as if anchored with anchors [0, 0]–[1, 1] and zero offsets (stretch), unless the child's size is specified, in which case it is placed at the content origin by its `alignSelf` on both axes. Anchored children are arranged after their in-flow siblings, so they may depend on the parent's final content box.

### Logical direction and mirroring

<!-- elaboration: RTL mirroring as one placement-mirroring pass of the shared shell -->
An element's layout context is right to left when its resolved direction (cascade step 9 of "Style resolution": `layoutDirection` `auto`, `ltr` or `rtl`, inherited, `auto` at the root following the active locale's direction) is RTL. Every rule of this file is written for LTR; an RTL context is laid out by the same rules, with the margin exchange of step 2 before the backend's `Layout` and the placement mirroring of step 3 after it, both applied by the shared shell (`Internal/Layout/Zenith_UIMirror.cpp`), so both candidates mirror identically:

1. **Resolve**: the cascade writes `m_auResolvedDirection` for every element.
2. **Lay out in LTR, margins exchanged where the flow is RTL**: before the backend runs, for each element whose parent's resolved direction is RTL, the computed `margin` left and right components are exchanged, because a margin belongs to the flow of the parent that places the element. Nothing else is exchanged: `padding`, `borderWidth` and `cornerRadius` stay physical, since step 3 moves an element's box without reflecting its interior. Anchors, offsets, pivots, grid columns, stack order and alignment are not touched: the backend computes the LTR layout of that tree. The custom runtime reads the exchanged margins from the computed style; the RmlUi integration receives them as inline margins ("The RmlUi integration", run-time input).
3. **Mirror placements top-down**: the shell walks the element table in pre-order (parents before children). For each element E with parent P: let d = E's LTR border-box x minus P's LTR content-box x, the offset the backend gave E inside P. When P's resolved direction is RTL, E's final border-box x is `P'cx + Pcw − d − Ew`, where P'cx is P's final content-box x (P was placed earlier in the walk), Pcw P's content width and Ew E's width; otherwise it is `P'cx + d`. E's content box moves by the same horizontal delta as its border box (so its padding and border keep their physical sides); its clip box is recomputed from its moved padding box intersected with P's final clip. The screen root is never moved. Widths, heights and every y are unchanged. This places every child of an RTL element mirrored inside its parent and every child of an LTR element unmirrored, whatever the depth, so an LTR island (an element with `layoutDirection: ltr` inside an RTL parent) has its own box mirrored by its RTL parent, keeps its children in LTR order inside that box, and an RTL element inside the island again mirrors its own children. Cost O(elements of the screen) for a screen whose root is RTL or holds an RTL element; a screen with no RTL element skips the walk.
   Equivalence with reflecting whole subtrees: for an element G at depth two below an RTL root R (G inside C inside R, both RTL), step 3 gives `R'cx + Rcw + (pl − pr) − gx − gw` in R's content coordinates, where pl and pr are C's left and right padding plus border: the reflection `x' = W − x − w` of the LTR layout exactly when C's horizontal edges are symmetric, and CSS's physical-padding result when they are not.
4. **What this yields**, for an RTL context, with no author change: a stack with `direction` `row` places its first child at the right (`rowReverse` at the left), `justify: start` packs to the right, and `alignItems: start` of a `column` stack aligns to the right edge; a grid's column 1 is the rightmost and auto placement fills each row right to left; an anchored element's `anchorMin.x`, `anchorMax.x`, `offsetMin.x`, `offsetMax.x` and `pivot.x` behave as measured from the right edge; overlay alignment `start` is the right. This is CSS's behaviour for `direction: rtl` with physical margins, padding, borders and corner radii: step 2's exchange is what keeps a mirrored child's physical left margin on its left (an LTR leaf with margin left 24 and right 8 inside an RTL row keeps 24 units of space on its left), and step 3's translation keeps every element's own padding, border and radii on their physical sides.
5. **What never flips**: `translate`, `rotate`, `shadowOffset` and `textShadowOffset` (visual, author-controlled); glyph order and glyph shapes inside text leaves (the paragraph pipeline orders glyphs by bidi, `Zenith2/Docs/Design/UI/Text.md → "Line layout and reordering"`, and `textAlign: start` and `end` resolve against the paragraph's base direction, which is its element's resolved direction unless `textDirection` overrides it); images, unless the element sets `mirrorInRtl: true` (property 84), which swaps its u0 and u1 (Rendering.md "Quad generation per element"); scroll offsets are stored as distances from the inline start, so `0` shows the right end of RTL content.
6. **Hit testing, focus and navigation** read the mirrored boxes, so directional navigation's "right" is the screen's right in both directions, as players expect, and tab order stays document order (the inline sequence).

The hand-derivable expectation: for a document whose edges are left-right symmetric, the RTL rectangles are the reflection `x' = W − x − w` of the LTR expected rectangles about the root box (W its width), which `Test_UILayoutGolden_MirroredMatchesReflection` asserts for every case of the table below marked "Mirrored" yes (those cases' documents use left-right symmetric margins, padding, borders and corner radii, a property the test checks before comparing), and `MirrorAsymmetricEdges`, `MirrorLtrIsland` and `MirrorLtrLeafInRtlRow` give hand-written RTL expectations, derived from steps 2 and 3, for asymmetric edges, an LTR island and an LTR leaf with asymmetric margins in an RTL flow.

### Text leaves

`Zenith_UITextMeasurer` serves four calls to the layout: `MinContentWidth`, `MaxContentWidth`, `HeightForWidth(w)` and `FirstBaseline(w)`; each lays out the element's paragraph (its text value formatted in the active locale, its spans from the computed style and rich-text tags) through `Zenith_TextLayoutEngine` in measure-only mode and caches the result. The arrange pass's final width triggers one full layout (with glyphs) when the element is painted.

### Scroll containers

A `scroll` (or `list`) element lays its content out with an indefinite size along its scroll axes, so the content takes its max-content (stack) or summed (list) extent; its viewport is its content box; the scroll extent is `max(0, content − viewport)` per axis; the offset is clamped to `[0, extent]` after every layout; children are positioned at `−offset` and clipped to the viewport. A scroll container is a layout boundary when its own size does not depend on its content (a definite or stretched size).

### Rounding and pixel snapping

- Layout is computed in `float` canvas units with no rounding.
- The draw list converts to pixels with `s` and snaps an element's rectangle edges independently (`x₀ = round(x × s)`, `x₁ = round((x + w) × s)`, round half to even) whenever the element's accumulated transform is a translation and an axis scale, so adjacent elements share edges without gaps or overlaps; rotated or skewed elements are not snapped.
- Text baselines snap to whole pixels on screen-space screens; glyph x positions are not snapped (the distance field keeps sub-pixel positions sharp).

### Complexity and bounds

| Step | Cost for a boundary subtree of n elements |
|---|---|
| style resolution of dirty elements | O(n × properties applied) |
| measure | O(n), each element once per dirty layout, text dominated by shaping (cached) |
| stack arrange | O(n) plus the freeze loop, at most O(k²) for a container of k children; k ≤ 4,096 by the parser limits |
| grid arrange | O(k × span + tracks) per container |
| total | O(n) amortised for incremental updates; a full layout of the W-UI2D content scale fits Z2-B-UI-002 (pending, Stage 1 per-system budget ticket for UI) |

Recursion depth is bounded by the document depth limit (64); the arrange pass uses an explicit stack of that depth, never native recursion beyond it.

## Invalidation and incremental layout

| Change | Marks |
|---|---|
| a layout-affecting style value, text content, visibility, a child list change (list items), an image's intrinsic size, a font becoming ACTIVE | `MEASURE_DIRTY` on the element and every ancestor up to and including the nearest layout boundary, and `LAYOUT_DIRTY` on that boundary |
| a paint-only property (colour, opacity, visual transform, focus ring), a state change that alters only paint properties | `PAINT_DIRTY` on the element |
| scroll offset | `LAYOUT_DIRTY` on the scroll element (its children move), no measure |
| surface, UI scale, text scale, locale, palette | every root `MEASURE_DIRTY | LAYOUT_DIRTY` (palette: `STYLE_DIRTY`, then layout only when a metric token changed) |

An element is a layout boundary when its width and height are both definite lengths independent of its content, when it is anchored with both sizes fixed, when it is a scroll container with a content-independent size, and always for a screen root. The context's `Update` re-arranges only dirty boundaries; `Zenith_UIContextStats::m_uElementsLaidOut` reports how many elements the pass touched, which the incremental-layout tests assert.

## Large-list virtualization

<!-- elaboration: fixed and Fenwick-tree extents, recycled blocks -->
A `list` element binds `items` to a LIST value (a fixed array or a container of structs of a component property) of count N ≤ `uUI_LIST_MAX_ITEMS` (1,048,576; a larger count is clamped to it and counted `ui.list.items_clamped`); only the items intersecting the viewport, plus `overscan` items on each side, exist as elements.

### Fixed-extent lists

With `itemExtent` E and gap g along the list axis: content extent = `N × E + (N − 1) × g` (0 when N = 0); the materialised range is `[max(0, ⌊o / (E + g)⌋ − overscan), min(N, ⌈(o + V) / (E + g)⌉ + overscan))` for offset o and viewport V; item i is placed at `i × (E + g)`. Every quantity is O(1), so scrolling to any item, `UI.ScrollTo`, and accessibility positions cost nothing proportional to N.

### Variable-extent lists

Without `itemExtent`, each item's measured extent is kept in an array of N floats, unknown entries estimated as the running mean of the measured extents (initially the template's max-content extent); prefix sums are kept in a Fenwick tree, so an item's offset and the item at an offset are O(log N). Scroll anchoring: when a measured extent of an item before the first visible item differs from its estimate, the offset is adjusted by the difference in the same Update, so the first visible item does not move on screen. Memory: 8 bytes per item (extent and tree), within Z2-B-UI-001.

### Item recycling

- The list owns a pool of item blocks, each a copy of the template's element rows, pre-allocated at instantiation: pool size = `⌈V_max / E_min⌉ + 2 × overscan + 2`, where V_max is the list's maximum viewport (its max size, or the canvas) and E_min the template's minimum extent, capped at 512 blocks.
- An item leaving the materialised range returns its block to the free list; an item entering takes a block, sets its rows' `m_auItem`, increments the block's item generation (stale `Zenith_UIElementRef`s of the old item then fail to resolve), re-evaluates its `@item.*` bindings and marks the block `MEASURE_DIRTY`.
- Focus on an item that leaves the range moves to the list element itself, keeping the item index, and returns to the item when it re-enters; directional navigation past the last materialised item scrolls by one item and materialises it before moving focus (Interaction.md "Directional navigation algorithm").
- The semantic tree reports `m_uSetSize = N` and the item's 1-based position, so a screen reader announces "item 12,345 of 1,000,000" without materialising other items.

## Data binding evaluation

Once per frame per screen, in the world's UI extractor, before layout:

1. For each binding slot, read the source value: a component property of the selected entity (self: the screen entity; parent: its scene parent through SCENE's hierarchy; singleton: the lowest stable simulation ID among entities with the component, computed once per frame per type; persistent: through the world's persistent-ID lookup) through the extractor's declared reads (`Zenith2/Docs/Design/UI/Documents.md → "Binding manifest file"`), or a configuration value; an absent source yields the slot's `default`.
2. Convert to the slot's `Zenith_UIValueKind`; a conversion failure yields the default and counts `ui.binding.conversion_failed`.
3. Compare with the slot's previous value (bitwise for numbers and handles, ID equality for strings, element-wise for lists up to the materialised items); on no change, stop.
4. On change, store the value and, for each dependent (element, member) of the slot's dependency list built at instantiation, apply the member: text content → `MEASURE_DIRTY` (the text cache key changes); `visible`, `enabled`, `checked`, `value`, `selected` → the corresponding flag or control value and its dirty bit; an image → `MEASURE_DIRTY` when the intrinsic size changes, `PAINT_DIRTY` otherwise; LIST count changes → the list's content extent and range.

Cost: O(slots + dependents of changed slots). A control the player is dragging (a slider being dragged, a text field being edited) ignores its bound value until the interaction commits, so the player's input is not fought by the stale simulation value of the previous tick.

## Animations and transitions

- Each element holds at most 8 active transitions `{ property, from, to, startUs, durationUs, easing }` and one active animation `{ name, startUs }`; starting a ninth transition replaces the oldest.
- `Update` samples them at the frame's presentation time: transition value = `from + (to − from) × ease(clamp((now − start) / duration, 0, 1))` per component (colours in linear space); an animation's value interpolates its keys around `t = ((now − start) / duration)` (mod 1 when looping).
- A transition starts when a state change alters a transitioned property: `from` is the current displayed value (so interrupted transitions do not jump), `to` the new resolved value.
- `UI.PlayAnimation` requests (Interaction.md "Requests from simulation") start the named animation on the element at the next Update.
- Reduced motion (Accessibility.md "Reduced motion"): every transition's duration is 0; a non-essential animation shows its last key's style statically; an essential animation (`essential: true`, or `essentialMotion` on the element) plays as an opacity cross-fade alone.
- Animations of layout-affecting properties mark `MEASURE_DIRTY` every sampled frame; the cook's validation warns (rule 23, diagnostic `ui.doc.layout_animation`) because they cost layout every frame.

## World-space panels

A screen with `m_eSpace = UI_SCREEN_SPACE_WORLD`:

- Its canvas is `m_xWorldSizeMetres × m_fPixelsPerMetre` canvas units, scale 1, no safe area.
- Its canvas maps onto the entity's local XY plane: canvas (0, 0) (top-left) at local `(−w/2, +h/2, 0)` metres, +x canvas to +X local, +y canvas to −Y local, the panel facing +Z (the model front of `Zenith2/Docs/Design/CORE/Maths.md → "Coordinate systems"`); `m_xCanvasToWorld = World · T(−w/2, h/2, 0) · S(1/ppm, −1/ppm, 1)`, World the interpolated world transform of the frame (`Zenith2/Docs/Design/SCENE/Transforms.md → "Interpolation data"`).
- Layout, style and text are identical to screen space; the draw list emits its batches into the world batches with the panel's matrix (Rendering.md "UIWorldSpace pass").
- Hit testing projects the pointer through the main camera of the previous frame: the ray from the inverse of `Zenith_UIFrameParams::m_xMainViewProjection` at the pointer pixel intersects the panel plane; a hit inside the canvas rectangle is a canvas-unit point fed to the panel's hit test (Interaction.md "Hit testing"); panels are tested nearest first along the ray, before screen-space screens of lower layer.

## Safe area and surface changes

- A screen-space root's border box is the canvas inset by `m_xSafeAreaPixels / s` when the document's `canvas.safeArea` is true (the default), and the full canvas otherwise (backgrounds that bleed under cut-outs).
- `PLATFORM_EVENT_WINDOW_SAFE_AREA_CHANGED`, display-scale records and window resizes (`Zenith2/Docs/Design/PLAT/Platform.md → "Display scale changes"`, "Safe area") reach the context through `SetSurface` from the module's claimant callback in the same Pump; the soft keyboard's inset on Android moves the safe area, and a focused text field is scrolled into the remaining area (Interaction.md "Text fields and IME").

## Layout conformance cases

Each case is a UI document under `Zenith2/Engine/ZenithUI/Tests/Fixtures/Layout/<Case>.ui.json` with the expected rectangles in `<Case>.expected.json` (element id → `[x, y, width, height]` in canvas units), laid out at the surface size the case names using the generated test font of `Zenith2/Docs/Design/UI/Tests.md → "Fixtures"` (monospace, 500 units per em advance, ascent 800, descent 200, so every text size is derivable by hand). The suite `Test_UILayoutGolden_AllCases` runs every case on the winning runtime from the D-SEL-UIRT ruling on, and the shell cases in their invariant form on each candidate before it; tolerance 0.01 units.

| Case | Asserts | Semantics | Mirrored |
|---|---|---|---|
| `StackColumnFixed` | three fixed-height children stacked with gap 8 and padding 16 | CSS | yes |
| `StackRowGrowEqual` | grow 1:1:1 divides the free width equally | CSS | yes |
| `StackRowGrowWeighted` | grow 1:2:1 with one child's maxWidth freezing it and the rest redistributing | CSS | yes |
| `StackRowShrinkWeighted` | shrink by shrink × basis with a minWidth freeze | ZenithUI semantics | no |
| `StackRowWrap` | greedy line wrap with rowGap and gap | CSS | yes |
| `StackJustifyEach` | the six justify modes, including negative leftover for centre and end | CSS | yes |
| `StackAlignEach` | start, centre, end, stretch and baseline cross alignment with mixed font sizes | CSS | no |
| `StackReverse` | rowReverse and columnReverse placement from the main end | CSS | yes |
| `StackAutoSizeParent` | an auto-sized parent takes its children's sum plus gaps, clamped by its max | CSS | no |
| `StackPercentChildren` | percent widths of a definite parent; percent of an indefinite parent behaving as auto | CSS | no |
| `AspectRatio` | one auto dimension from aspectRatio, then clamped | CSS | no |
| `MarginsPaddingBorders` | edge resolution horizontally against width and vertically against height | CSS | no |
| `GridFixedAndFr` | fixed plus fr columns, gaps | CSS | yes |
| `GridAutoTracks` | auto tracks from max-content children; minContent tracks | ZenithUI semantics | no |
| `GridSpanning` | a two-column span distributing its shortfall over auto tracks | ZenithUI semantics | no |
| `GridAutoPlacement` | auto placement around explicit items with the cursor never moving back | CSS | yes |
| `AnchoredCorners` | the four corners with fixed sizes and pivots | ZenithUI semantics | yes |
| `AnchoredStretch` | stretch anchors with offsets | ZenithUI semantics | yes |
| `AnchoredFitContent` | equal anchors with auto size and pivot | ZenithUI semantics | yes |
| `OverlayChildren` | overlay stretching and aligned fixed-size children | ZenithUI semantics | yes |
| `TextWrapWord` | a paragraph wrapped at word boundaries at a fixed width | CSS | no |
| `TextWrapAnywhere` | a long unbreakable word broken at grapheme boundaries | CSS | no |
| `TextEllipsisMaxLines` | maxLines 2 with an ellipsis at the end of line 2 | CSS | no |
| `TextAlignments` | start, end, centre and justify in LTR, and start and end in an RTL (Hebrew test glyphs) paragraph | CSS | no |
| `TextScaleAccessibility` | text scale 1.5 enlarges the text and its auto-sized container; a `fixed` policy element keeps its size | CSS | no |
| `ScrollContent` | scroll extent and clamped offset; children offset by the scroll | CSS | no |
| `ListFixedExtent` | the materialised range at three offsets with overscan 2 | shell | no |
| `ListVariableExtent` | estimate, measured extents and scroll anchoring after a measured item above the viewport grows | shell | no |
| `ReferenceScaleMatch` | reference mode at match 0, 0.5 and 1 on 1280 × 720 and 2340 × 1080 | shell | no |
| `DpScaleUIScale` | dp mode at display scale 2.625 (S24) and UI scale 1.25 | shell | no |
| `SafeAreaInset` | a root inset by a 96-pixel top and 48-pixel left safe area | shell | no |
| `PixelSnapping` | adjacent elements at fractional positions share snapped pixel edges | shell | no |
| `VisibilityCollapse` | a hidden child takes no space and its siblings close the gap | CSS | no |
| `InstanceFlattening` | an instance with overrides lays out as the flattened tree | shell | no |
| `NestedBoundaries` | a change inside a fixed-size panel re-lays only that boundary (`m_uElementsLaidOut` equals the subtree size) | ZenithUI semantics | no |
| `WorldPanelCanvas` | a 2 × 1 m panel at 512 units per metre has a 1024 × 512 canvas and maps corners to the expected world points | shell | no |
| `MirrorAsymmetricEdges` | an RTL row stack (root 400 wide) whose children have different left and right margins, padding and corner radii: each child's margin box is mirrored inside the row with its physical margins kept (step 2), and each child's content box sits inside its border box by its physical left padding and border (step 3); expected rectangles written by hand from "Logical direction and mirroring" | shell | — |
| `MirrorLtrIsland` | an RTL column (`alignItems: start`, content box 400 wide) holding a 200-wide `layoutDirection: ltr` row with padding left 10 and right 0 and three 40-wide children: the row's border box is mirrored inside the column (x 200 to 400, at the column's right), its own padding stays physical (content box x 210 to 400), and its children keep LTR order inside it (x 210, 250 and 290) | shell | — |
| `MirrorLtrLeafInRtlRow` | an RTL row (content box 400 wide) holding, first, an LTR text leaf 100 wide with margin left 24 and right 8: its border box is at x 400 − 8 − 100 = 292 when unexchanged would give 276, its margin box spans 268 to 400 with 24 units on its physical left, and the next sibling sits left of that margin | shell | — |
| `MirrorPhysicalOffsets` | `translate` and `shadowOffset` keep their sign in RTL; an image with `mirrorInRtl` swaps its UVs in the draw list | shell | — |
| `TextScaleTwoNoOverflow` | every text case of this table at text scale 2.0: no text leaf overflows its container unless the container scrolls or the text truncates | CSS | no |

"Semantics" says which acceptance class a case belongs to for D-SEL-UIRT: **CSS** cases are behaviour CSS flexbox and grid (with `min-width: 0` emitted) define, so RmlUi is expected to meet them unaided; **ZenithUI semantics** cases encode behaviour CSS lacks (anchors and pivots, the overlay container, shrink weighting with ZenithUI's freeze order, spanning shortfall distribution over auto tracks, one alignment property for both grid axes, layout boundaries), which a candidate meets natively or through a documented shim; **shell** cases exercise code both candidates share (scaling, safe area, snapping, flattening, world panels, virtualization, mirroring), but every shell case's rectangles also pass through the candidate's `Layout`, so a candidate that fails a CSS or ZenithUI-semantics behaviour the case's content uses would also miss the case's absolute rectangles. They are not selection criteria. While D-SEL-UIRT is open, `Test_UILayoutGolden_AllCases` runs the shell cases on each compiled candidate (one `SUBCASE` per candidate, failing the gate) in an invariant form that asserts only what the shell computes, relative to the same candidate's own base layout (the per-case invariants of `Zenith2/Docs/Design/UI/Tests.md → "Layout golden suite"`), and `Test_UIRuntimeSelection_ConformanceBothCandidates` runs the CSS and ZenithUI-semantics cases on each and reports them, together with the shell cases' absolute expectations as class `shell`, recorded and not judged; the ruling ticket deletes the losing candidate and the selection test and widens `Test_UILayoutGolden_AllCases` to every case on the winner (`Zenith2/Docs/Design/UI/Tests.md → "Layout golden suite"`). `ListFixedExtent` and `ListVariableExtent` land with virtualization (slice 16) and `WorldPanelCanvas` with world-space panels (slice 12), after the ruling (`Zenith2/Docs/Design/UI.md → "Stage plan"`).
