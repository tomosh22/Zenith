# UI design: text shaping, bidirectional text, line breaking, fonts and glyph atlases

This file is part of the UI design family; the entry document is `Zenith2/Docs/Design/UI.md`. It specifies the text stack: the D-SEL-TEXT (open, Stage 1/2) candidates behind ZenithUI's internal interfaces, the Unicode data ZenithUI generates, the paragraph pipeline step by step (decoding, bidi, script itemization, style runs, font fallback, shaping, line breaking, reordering, alignment, truncation, metrics), carets and selection, the text edit model, the font registry, the glyph cache with its MTSDF atlases and dynamic-glyph state machine, the layout cache and the conformance corpus. The public declarations are `Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIText header"`, "Zenith_UIFont header" and "Zenith_UIFontBake header"; the cooked font format is `Zenith2/Docs/Design/UI/Documents.md → "Cooked font format"`.

## Scope of this file

| Topic | Section |
|---|---|
| D-SEL-TEXT candidates and the internal backend interfaces | "Text stack selection" |
| generated Unicode property tables | "Unicode data" |
| one paragraph from text to positioned glyphs | "Paragraph pipeline" |
| caret positions, hit testing, selection | "Caret and selection" |
| text-field editing | "Text editing" |
| font assets, faces, stacks | "Font registry and faces" |
| atlas packing, dynamic glyphs, MSDF, upload | "Glyph cache and atlases" |
| reuse across frames | "Text layout cache" |
| the multilingual test corpus | "Text conformance corpus" |

## Text stack selection

<!-- elaboration: the T1/T2 × B1/B2 candidates behind four internal interfaces -->
D-SEL-TEXT (open, Stage 1/2) chooses the shaping and rasterisation stack and the bidi implementation (`Zenith2/Docs/Architecture.md → "Named selection decisions"`):

| Candidate | Shaping | Outlines and metrics | Distance fields | Bidi and line segmentation |
|---|---|---|---|---|
| T1 + B1 | HarfBuzz | FreeType (`FT_Load_Glyph` with `FT_LOAD_NO_SCALE`, `FT_Outline_Decompose`) | msdfgen core (no `msdfgen-ext`) | SheenBidi for UAX #9; ZenithUI's UAX #14 breaker and dictionary segmenter |
| T1 + B2 | HarfBuzz | FreeType | msdfgen core | an ICU4X subset (`icu_capi`: bidi, line and word segmenters with the dictionary data of its own build) |
| T2 + B1 | HarfBuzz | HarfBuzz itself (`hb_font_draw_glyph`, `hb_ot_metrics_get_position`) | msdfgen core | SheenBidi; ZenithUI's breaker |
| T2 + B2 | HarfBuzz | HarfBuzz | msdfgen core | the ICU4X subset |

The libraries are owned by ZenithUI ([Z2-N-ARCH-200]), confined to `Zenith2/Engine/ZenithUI/Internal/Text/Backends/`, and reached through four internal interfaces so the ruling changes one directory:

```cpp
// Zenith2/Engine/ZenithUI/Internal/Text/Zenith_UITextBackends.h (Internal)
struct Zenith_UIShapedGlyph { uint32 m_uGlyph; uint32 m_uCluster; sint32 m_iAdvanceX; sint32 m_iOffsetX; sint32 m_iOffsetY; bool m_bUnsafeToBreak; };
struct Zenith_UIShapeRequest
{
	uint32 m_uFace;                     // the registry's face handle
	Zenith_Span<const uint32> m_auCodePoints;   // the paragraph's code points
	uint32 m_uItemStart, m_uItemLength; // the run, inside the full context
	uint32 m_uScript;                   // ISO 15924 as four ASCII bytes
	Zenith_StringView m_strLanguage;
	bool m_bRightToLeft;
	uint32 m_uFeatureMask;              // bit 0 kern, 1 liga, 2 clig, 3 calt, 4 tnum
};
class Zenith_UIShaper
{
public:
	virtual ~Zenith_UIShaper() = default;
	virtual Zenith_Result<uint32, Zenith_FontError> OpenFace(Zenith_ByteSpan xProgram, uint32 uIndex) = 0;
	virtual void CloseFace(uint32 uFace) = 0;
	virtual Zenith_Result<uint32, Zenith_TextLayoutError> Shape(const Zenith_UIShapeRequest& xRequest,
		uint32 uSlot, Zenith_Span<Zenith_UIShapedGlyph> axOut) = 0;   // units: font units; returns the glyph count
	virtual uint32 GetNominalGlyph(uint32 uFace, uint32 uCodePoint) const = 0;   // 0 when absent
	virtual uint32 GetLigatureCarets(uint32 uFace, uint32 uGlyph, Zenith_Span<sint32> aiOut) const = 0;
};
// The outline commands a source emits, in font units, y up; one contour starts with MoveTo and ends with Close.
class Zenith_UIOutlineSink
{
public:
	virtual ~Zenith_UIOutlineSink() = default;
	virtual void MoveTo(float fX, float fY) = 0;
	virtual void LineTo(float fX, float fY) = 0;
	virtual void QuadTo(float fControlX, float fControlY, float fX, float fY) = 0;
	virtual void CubicTo(float fControl1X, float fControl1Y, float fControl2X, float fControl2Y, float fX, float fY) = 0;
	virtual void Close() = 0;
};
class Zenith_UIOutlineSource
{
public:
	virtual ~Zenith_UIOutlineSource() = default;
	// Contours as move/line/quadratic/cubic commands in font units; false for a glyph without an outline (space).
	virtual Zenith_Result<bool, Zenith_FontError> GetOutline(uint32 uFace, uint32 uGlyph,
		Zenith_UIOutlineSink& xSink) = 0;
	virtual Zenith_FontMetrics GetMetrics(uint32 uFace) const = 0;
};
class Zenith_UIBidi
{
public:
	virtual ~Zenith_UIBidi() = default;
	// UAX #9 rules P2–P3 (when eBase is AUTO), X1–X10, W1–W7, N0–N2, I1–I2 over one paragraph.
	virtual Zenith_Result<uint8, Zenith_TextLayoutError> ResolveLevels(Zenith_Span<const uint32> auCodePoints,
		Zenith_TextDirection eBase, Zenith_Span<uint8> auLevelsOut, Zenith_Arena& xScratch) = 0;   // returns the paragraph level
};
class Zenith_UILineSegmenter
{
public:
	virtual ~Zenith_UILineSegmenter() = default;
	// One break class per code point boundary: 0 none, 1 allowed, 2 mandatory (UAX #14, with SA dictionary segmentation);
	// auBreaksOut has n + 1 entries for n code points, entry i the boundary before code point i ("Corpus file format").
	virtual Zenith_Result<void, Zenith_TextLayoutError> FindBreaks(Zenith_Span<const uint32> auCodePoints,
		Zenith_StringView strLanguage, Zenith_Span<uint8> auBreaksOut, Zenith_Arena& xScratch) = 0;
	virtual Zenith_Result<void, Zenith_TextLayoutError> FindWordBoundaries(Zenith_Span<const uint32> auCodePoints,
		Zenith_Span<uint8> auBoundariesOut, Zenith_Arena& xScratch) = 0;   // UAX #29 words
};
```

- HarfBuzz is common to every candidate because no alternative named in the plan shapes the corpus; the ruling decides FreeType versus HarfBuzz-only outlines, and SheenBidi with ZenithUI's breaker versus the ICU4X subset.
- Every backend call is allocation-bounded: HarfBuzz and FreeType get custom allocation hooks that draw from a per-slot scratch arena of `uTEXT_SHAPER_SLOTS` (16) slots, each 1 MiB, so a malicious font cannot exhaust the heap ([Z2-N-ARCH-951]); an exhausted slot fails the call with `TEXT_LAYOUT_ERROR_SHAPER`.
- The selection criteria, their order of comparison and the harness and ruling split are `Zenith2/Docs/Design/UI.md → "Selection decisions UI owns"`, which adopts DecisionLog's acceptance rule for D-SEL-TEXT. Shaping goldens are not a selection criterion: HarfBuzz is common to every candidate, so the candidates differ in outlines, bidi and segmentation alone.
- The msdfgen shape builder `Zenith_UIMsdfShapeSink` (`Internal/Text/Zenith_UIMSDF.cpp`) implements `Zenith_UIOutlineSink`, so both outline sources feed one MSDF generator.

## Unicode data

ZenithUI generates its Unicode property tables at configure time from the Unicode Character Database 16.0, pinned as the third-party data entry `unicode-ucd` (licence Unicode-3.0; integration mode `data`, cross-area row UI9), through `zenith_add_library(ZenithUI … GENERATES Zenith_UIUnicodeTables.generated.inc)`, which writes `${ZENITH_OUT}/generated/ZenithUI/Zenith_UIUnicodeTables.generated.inc` (never in the source tree, `Zenith2/Docs/Architecture/BuildSystem.md`), and the CMake script `Zenith2/Engine/ZenithUI/Internal/Generate/Zenith_UIUnicodeTables.cmake`:

| Property | Source file | Values used |
|---|---|---|
| General_Category | `UnicodeData.txt` | combining marks, separators, controls, default-ignorables' categories |
| Script, Script_Extensions | `Scripts.txt`, `ScriptExtensions.txt` | itemization (UAX #24) |
| Line_Break | `LineBreak.txt` | the 48 classes of UAX #14 |
| East_Asian_Width | `EastAsianWidth.txt` | LB30's F, W, H resolution |
| Grapheme_Cluster_Break, Indic_Conjunct_Break | `auxiliary/GraphemeBreakProperty.txt`, `DerivedCoreProperties.txt` | UAX #29 extended grapheme clusters (rule GB9c) |
| Word_Break | `auxiliary/WordBreakProperty.txt` | word boundaries |
| Extended_Pictographic, Emoji_Presentation | `emoji/emoji-data.txt` | GB11, LB30b, emoji font fallback |
| Default_Ignorable_Code_Point | `DerivedCoreProperties.txt` | coverage tests, caret skipping |
| Bidi_Class, Bidi_Mirroring_Glyph, Bidi_Paired_Bracket | `UnicodeData.txt`, `BidiMirroring.txt`, `BidiBrackets.txt` | the B1 candidate's mirroring check and bracket pairs in itemization |

Each property is a two-stage table: stage 1 maps `codePoint >> 8` (4,352 entries for U+0000…U+10FFFF) to a 16-bit block index; stage 2 holds deduplicated 256-entry blocks of `uint8` values; lookup is two array reads. The generator writes the tables with a header comment carrying the UCD version and the input file hashes. `Test_UIUnicode_TablesMatchSamples` is the independent oracle: at test time it parses the pinned UCD files themselves (read through the `thirdparty:` content root, row UI9) with a separate, line-oriented reader in the test and compares 200 sampled code points per property, plus every range boundary listed in the files, against the generated tables; nothing derived from the generator is committed. `Test_UIUnicode_TablesCoverEveryPlane` checks the last code point.

## Paragraph pipeline

<!-- elaboration: the paragraph pipeline's step order and rules -->
`Zenith_TextLayoutEngine::Layout(desc, arena)` runs the steps below. Every step is O(n) in the paragraph's code points n unless stated; scratch comes from the caller's arena; a failure returns its error with nothing published.

### Input and decoding

<!-- elaboration: where malformed UTF-8 becomes U+FFFD -->
0. Every text-value path repairs its text before it reaches `Layout`: localized messages, bound string values, literal and pseudo text, platform text events and pasted text pass through `Zenith_UIRepairUtf8` (`Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIText header"`), which replaces each maximal ill-formed subsequence with U+FFFD and counts `ui.text.utf8_repaired`, so malformed text from any source displays as U+FFFD and never fails layout.
1. Validate: `m_strText` is valid UTF-8 (`Zenith_ValidateUtf8`, `Zenith2/Docs/Design/CORE/Text.md → "Unicode functions"`), at most `uTEXT_MAX_PARAGRAPH_BYTES`; spans sorted, non-overlapping, covering every byte, each starting at a code-point boundary, each with a font stack; otherwise `TEXT_LAYOUT_ERROR_BAD_UTF8`, `_TOO_LONG`, `_BAD_SPAN` or `_NO_FONT`. `TEXT_LAYOUT_ERROR_BAD_UTF8` is therefore returned only to a caller that calls `Layout` directly with unrepaired text (a test, a tool), never on a UI path.
2. Decode into arrays of code points and their byte offsets.
3. Split at paragraph separators (U+2029, LF, CR, CR LF, U+0085): each sub-paragraph runs steps 4–9 independently; lines from all sub-paragraphs stack in order, and a separator is a mandatory line break that belongs to the line it ends.

### Bidirectional resolution

4. Resolve embedding levels with `Zenith_UIBidi::ResolveLevels`: base direction `m_eDirection` (LTR → level 0, RTL → 1, AUTO → rules P2 and P3: the first strong character, skipping isolates; with none, the active locale's direction from `Zenith_Localization::IsRightToLeft()`). Explicit formatting characters (LRE, RLE, LRO, RLO, PDF, LRI, RLI, FSI, PDI) are honoured and kept as zero-width characters.
- Until bidirectional text lands (inventory item Z2-I-UI-07, Stage 4), the Stage 3 pipeline sets every level to the base level: an RTL paragraph is laid out right to left as one run, mixed-direction text is not reordered, and the corpus cases that need reordering are Stage 4 tests.

### Script itemization

5. Assign each code point a script (UAX #24): its Script value, except that Common (`Zyyy`) and Inherited (`Zinh`) take the script of the preceding code point (the following one at the paragraph start); a code point whose Script_Extensions contain the preceding run's script continues that run; a closing paired bracket takes the script of its opening bracket from a 64-entry bracket stack, so `(שלום)` keeps the brackets with the Hebrew run.

### Style runs and rich text

6. Style runs are the spans of the paragraph desc: the element's computed text style, split by the rich-text tags of the formatted message (`<b>`, `<i>`, `<u>`, `<c=token>`; Documents.md "Message syntax") into spans with weight 700, italic, underline or the token's colour; inline objects (`<prompt>`, `<img>`) are one U+FFFC each with the object's size, laid out as a single unbreakable cluster with Line_Break class ID (an ideograph: break opportunities on both sides).

### Font fallback

7. For each extended grapheme cluster (UAX #29 with GB9c), choose its font:
   1. candidates: the span's `Zenith_FontStack` entries in order (the style's `fontFamily` tokens resolved for the active locale), then the families the localization project lists for the cluster's script (`fonts.scripts`, Documents.md "Localization project document schema"), then the engine default family stack;
   2. within a family, the face by the CSS font-matching rule: italic faces first when italic is requested (else upright first), then by weight — a desired weight w in [400, 500] tries weights from w upward to 500, then below w downward, then above 500 upward; w < 400 tries downward then upward; w > 500 upward then downward; no synthetic bold or oblique is produced;
   3. a face covers the cluster when its COVERAGE section contains every code point of the cluster except default-ignorables (ZWJ, ZWNJ, U+FE00–U+FE0F, U+E0100–U+E01EF, U+200E, U+200F and the bidi formatting characters);
   4. the first covering face wins; an emoji cluster (Extended_Pictographic with Emoji_Presentation or U+FE0F) prefers a family the stack marks as emoji, otherwise the general order;
   5. when no face covers the cluster, the first face of the stack is used with glyph 0 (`.notdef`), the glyph is flagged, `m_uMissingGlyphs` and `ui.text.missing_glyphs` count it, and a development build logs each missing code point once per process.
   A per-paragraph memo maps (stack, first code point) to the chosen face so repeated characters cost one lookup.

### Shaping

8. Segment the paragraph into shaping runs at every change of (face, size, script, embedding level, span); shape each run with `Zenith_UIShaper::Shape`, passing the whole sub-paragraph as context (HarfBuzz's pre- and post-context), the script, the language (`m_strLanguage`, else the active locale's language subtag), the direction from the level's parity and the features: `kern`, `liga`, `clig`, `calt` always, `tnum` when `m_bTabularNumbers`. Advances and offsets scale by `size / unitsPerEm`.
- Letter spacing (`m_fLetterSpacing` em × size) is added after the last glyph of each cluster, except in runs whose script joins cursively (`Arab`, `Syrc`, `Mong`, `Nkoo`, `Phag`, `Mand`, `Adlm`, `Rohg`), where it is 0, because inter-letter space breaks joining.
- A run is shaped at most twice: once in step 8, and once in step 9 for each fragment of a run split by a line break at a position the shaper flagged unsafe to break (`HB_GLYPH_FLAG_UNSAFE_TO_BREAK`).

### Line breaking

9. Line break opportunities come from `Zenith_UILineSegmenter::FindBreaks`: UAX #14 of Unicode 16.0 (rules LB1 to LB31 with LB30a and LB30b, the default tailoring) with complex-context (SA class) ranges of Thai, Lao, Khmer and Myanmar segmented by a dictionary into words — ZenithUI's maximal-matching segmenter over the dictionary word lists pinned in `unicode-ucd`'s companion data entry `icu-dictionaries` (B1), or the ICU4X segmenter (B2). Mandatory breaks are BK, CR, LF, NL and the paragraph separators. Wrapping, with the line's available width W (`m_fMaxWidth`, unbounded when ≤ 0):
   1. `TEXT_WRAP_NONE`: only mandatory breaks.
   2. `TEXT_WRAP_WORD`: segments are the text between opportunities; a greedy pass accumulates segment widths in logical order (a segment's width is the sum of its clusters' advances; trailing white space of a line hangs and does not count towards W); when adding the next segment would exceed W on a non-empty line, the line breaks before it; a single segment wider than W stays on its own line and overflows.
   3. `TEXT_WRAP_ANYWHERE`: as word, and a segment wider than W is split at grapheme-cluster boundaries into pieces that fit (each line at least one cluster).
   4. After the breaks are chosen, every run crossing a break at an unsafe-to-break glyph is re-shaped as two fragments and the fragments' widths replace the run's; when a re-shaped line no longer fits, it is kept (the pass never iterates), so the result is deterministic.
   5. U+00AD (soft hyphen) is an opportunity; when a line breaks there, a hyphen glyph (U+2010, else U+002D) shaped with the run's face is appended; there is no automatic hyphenation.
   6. CJK lines follow the UAX #14 default rules (no break before closing punctuation and small kana of class CJ resolved as NS, the strict line-breaking default for `ja`, normal for `zh` and `ko`, chosen by the language).

### Line layout and reordering

<!-- elaboration: L2 over shaping runs, so the shaper's visual order is never reversed twice and same-level runs still reverse -->
10. For each line:
   1. Apply UAX #9 rule L1: trailing white space and segment and paragraph separators reset to the paragraph level, splitting runs where the level changes. A fragment that L1 splits off a shaping run keeps the shaper's glyphs (white space and separators take part in no shaping interaction, so it is not re-shaped), but when its new level's parity differs from the run's (two spaces and a tab ending an RTL run at the end of an LTR line), the shaper's order is wrong for it: its glyphs are re-emitted in logical order, which is the visual order of an even level (and in reverse logical order for an odd one), and its cluster map is rebuilt for that order in step 5. A fragment whose parity is unchanged keeps the shaper's order, so after this step every run's glyph order matches its own level's direction, which step 4's argument needs.
   2. List the line's **runs** in logical order: its shaping runs, or run fragments after step 9's split at the line's ends. Each run has exactly one embedding level, because step 8 segments at every level change; several consecutive runs may share a level (a `<b>` word or a `<c=…>` span inside an Arabic sentence, or a fallback-face change, each start a new shaping run at the same level).
   3. Reorder the runs by L2 at run granularity: let h be the highest level on the line and m the lowest odd level; for each level k from h down to m, reverse every maximal sequence of consecutive runs whose levels are k or higher. Runs are the unit permuted, never level runs, so three level-1 runs A, B, C of one RTL sentence appear as C, B, A; the order of glyphs inside a run is never touched here.
   4. Emit each run's glyphs at its visual position, left to right, with a pen advancing from 0, in exactly the order the shaper returned them: HarfBuzz returns an RTL run's glyphs already in visual (left-to-right) order, and an LTR run's in logical order, which is also visual.
      Why no glyph is reversed twice: character-level L2 reverses a character at level L ≥ m exactly L − m + 1 times, an odd count exactly when L is odd (m is odd), and never moves a character below m, whose level is even. So the characters inside any one run (one level) end in reversed logical order for an odd level and in logical order for an even one, which is the order the shaper already produced for that run's direction. Every sequence reversed at threshold k is a union of whole runs (a run's level is either ≥ k or not), so permuting whole runs in step 3 reproduces the character-level permutation between runs. Step 3 moves runs and step 4 copies each run unchanged, so the only reversal any glyph undergoes is the shaper's, for RTL runs.
   5. Build the cluster map from the shaper's cluster values: for each run, walk its glyphs in the emitted order and record, per cluster byte, the visual x range covering that cluster's glyphs (a cluster's glyphs are contiguous in HarfBuzz output in either direction; in an RTL run the cluster bytes decrease as x increases). `Zenith_TextGlyph::m_uClusterByte` holds it per glyph and bit 0 of `m_uFlags` marks the leftmost glyph of each cluster in visual order.
   Mirrored characters are handled by the shaper's RTL mirroring (`rtlm` and the Bidi_Mirroring_Glyph substitution HarfBuzz performs).

### Alignment and justification

11. With the paragraph's available width W (for an unbounded width, the widest line's width):
- `TEXT_ALIGN_START`: x = 0 for a level-0 paragraph, `W − width` for a level-1 paragraph; `TEXT_ALIGN_END` the reverse; `TEXT_ALIGN_CENTRE`: `(W − width) / 2`.
- `TEXT_ALIGN_JUSTIFY`: every line except the last and lines ending in a mandatory break distributes `W − width` equally over its expansion opportunities (U+0020, U+00A0, U+3000 between clusters); a line of ideographs (class ID, CJ, H2, H3) with no spaces distributes it between clusters; a line with no opportunity is start-aligned; justification never applies to cursive-joining runs' interior.

### Truncation and ellipsis

12. When `m_uMaxLines` lines are placed and text remains, or the next line's bottom would exceed `m_fMaxHeight`:
- `TEXT_OVERFLOW_VISIBLE`: every line is kept (the element's clip may cut them).
- `TEXT_OVERFLOW_CLIP`: the remaining lines are dropped; `m_bTruncated` is set.
- `TEXT_OVERFLOW_ELLIPSIS`: on the last kept line, clusters are removed from its logical end (its visual end for the paragraph's direction) at grapheme boundaries until `width + ellipsisWidth ≤ W`, then U+2026 is appended, shaped with the face of the last remaining cluster (three U+002E when that face lacks U+2026); the ellipsis glyph is flagged; `m_bTruncated` is set. A single-line paragraph wider than W truncates the same way.

### Vertical metrics

13. A line's ascent and descent are the maxima over its runs of `face ascent × size` and `face descent × size`; inline objects contribute their height above the baseline (prompts are centred on the cap height). With `m_fLineHeight` L, the line box height is `L × (ascent + descent)`, the extra `(L − 1) × (ascent + descent)` split equally above and below (half-leading); the first line's top is 0, its baseline `halfLeading + ascent`, and each next line starts at the previous line's bottom. An empty line (consecutive separators) uses the span's primary face metrics.

### Output records

14. Write `Zenith_TextLayout`: lines with byte ranges, baseline, ascent, descent, width, x offset and flags; glyphs in visual order per line with positions in canvas units, cluster byte, span, level and flags; the glyph-key table (font registry index, face, glyph ID) deduplicated per layout; min-content width (the widest segment of step 9's segmentation) and max-content width (the widest line with only mandatory breaks), computed in the same pass; the first baseline; the base level; the truncation flag; the missing-glyph count. In measure-only mode the glyph array is not written.

## Caret and selection

- Caret stops are extended grapheme cluster boundaries; inside a ligature glyph that covers several clusters, the stops are placed at the shaper's ligature caret positions (`GetLigatureCarets`, from GDEF), else at equal fractions of the glyph's advance.
- All caret and selection operations read the cluster map of step 10.5, so they agree with the glyph order by construction.
- `CaretAt(byte, affinity)`: find the line holding the byte; at a boundary between two runs of different levels there are two visual positions, and the affinity picks the run before (trailing) or after (leading); LTR clusters put the caret at their left edge, RTL clusters at their right edge.
- `HitTest(point)`: the line whose box contains the point's y (the nearest line when outside), then the cluster boundary nearest to the point's x in visual order; the affinity is the side of the hit cluster.
- `MoveCaretVisual(caret, steps)`: moves through the line's visual order one cluster per step, crossing to the next or previous line at the ends (visual movement, as browsers do for arrow keys); Home and End go to the line's visual start and end; up and down keep the caret's x and hit-test the adjacent line.
- Word boundaries (UAX #29 words, with dictionary segmentation for SA ranges) drive Ctrl+arrow movement and double-click selection.
- `GetSelectionRects(start, end)`: per line, the union of the visual extents of the clusters whose bytes are in [start, end); a bidirectional line may give several rectangles; the line box's top and bottom bound them vertically.

## Text editing

`Zenith_TextEditBuffer` holds the field's UTF-8 text in a buffer of fixed capacity (`maxBytes`), the caret and anchor byte offsets (always at grapheme boundaries) and the composition (pre-edit) string kept separately from the committed text:

| Operation | Behaviour |
|---|---|
| `Insert(text)` | replaces the selection; refuses (false, nothing changed) invalid UTF-8, NUL, control characters other than LF (multiline) or TAB (refused), and a result above capacity; a single-line field converts LF and CR to U+0020 before validation |
| `DeleteBackward` | with a selection, deletes it; otherwise deletes the previous grapheme cluster, except in Brahmic scripts (`Deva`, `Beng`, `Guru`, `Gujr`, `Orya`, `Taml`, `Telu`, `Knda`, `Mlym`, `Sinh`, `Thai`, `Laoo`, `Khmr`, `Mymr`), where it deletes the previous code point so a dependent vowel sign can be corrected; with the word flag, deletes to the previous word boundary |
| `DeleteForward` | deletes the next grapheme cluster, or to the next word boundary |
| `SetCaret(byte, extend)` | snaps to the nearest grapheme boundary at or before the byte; `extend` keeps the anchor |
| composition | `SetComposition` stores the IME's pre-edit text, cursor and highlighted clause (PLAT's `TEXT_INPUT_EVENT_COMPOSITION`, `Zenith2/Docs/Design/PLAT/Input.md → "Composition events"`); the field displays committed text with the composition inserted at the caret, underlined, the highlighted clause with a thicker underline; `EndComposition` drops it; a commit inserts the committed text and ends it |
| password purpose | the field displays U+2022 per grapheme cluster, copy and cut are disabled, and the semantic tree's value is masked (`UI_A11Y_PASSWORD`) |
| clipboard | Ctrl+C, Ctrl+X, Ctrl+V (Cmd on a platform that reports it) through PLAT's `Zenith_Clipboard` (`Zenith2/Docs/Design/PLAT/Platform.md → "Clipboard"`), on the main thread inside the claimant callback; pasted text is validated like `Insert` and truncated at the last grapheme boundary that fits |

The field keeps no undo history (the plan asks for none in UI; undo is EDIT's, for documents); Ctrl+Z is not consumed by the field, so it reaches the presentation contexts below. Requirement Z2-R-UI-055 and `Test_UITextEdit_Operations` cover the operations of this table and nothing else.

## Font registry and faces

`Zenith_FontRegistry` (`Internal/Text/Zenith_FontRegistry.h`, owned by `Zenith_UISystem`):

- Font assets get a process-stable registry index in first-reference order (at most 256 fonts, 8 faces each); the index is the `m_uFontIndex` of `Zenith_GlyphKey`, so glyph keys never name an asset handle.
- It holds a counted handle of every font the open documents' folded themes name, the localization project's font selection for the active locale, the script fallbacks and the engine default stack reference, requesting them at `ASSET_PRIORITY_CRITICAL` when the locale is committed.
- It opens a face in the shaper lazily, on the first layout that selects it, from the font asset's program bytes; opening a face is a main-thread prologue operation (`Zenith_UISystem::BeginFrame` opens faces requested by the previous frame's layouts), and until a face is open its clusters fall back to the next candidate for that frame and the paragraph is re-laid out when the face opens (`MEASURE_DIRTY` on the elements that used the fallback).
- It rebuilds every `Zenith_FontStack` in the prologue when a referenced font becomes ACTIVE or FAILED, or the locale changes; a stack entry whose asset is not ACTIVE is skipped.
- A FAILED font asset resolves to its kind's SUBSTITUTE fallback, the engine default family (`Zenith2/Docs/Design/ASSET/Cook.md → "Engine fallbacks"`), so text always renders in some face.

## Glyph cache and atlases

### Atlas packing

<!-- elaboration: skyline packing and the dynamic-glyph state machine -->
Pages are square RGBA8 MTSDF images of the family's page size (`Zenith2/Docs/Design/UI/Documents.md → "MSDF atlas page payload"`). Glyph cells are packed with the skyline bottom-left algorithm:

1. A page keeps a skyline: a list of horizontal segments `(x, y, width)` covering the page width, initially one segment at y = 0.
2. To place a cell of w × h (the glyph's image plus `padding` on every side), try every segment index i as the left end: the cell's y is the maximum skyline height over `[x_i, x_i + w)`; reject positions where `y + h` exceeds the page height or `x_i + w` the page width; choose the position with the lowest top `y + h`, ties by the lowest x.
3. Insert the cell: add a segment `(x_i, y + h, w)`, trim or remove the segments it covers, merge adjacent segments of equal height.
4. A cell that fits no position on the current page opens the next page (baked: up to `maxPages`, else `FONT_ERROR_PAGE_LIMIT`; dynamic: "Dynamic glyph state machine").

Baking places glyphs sorted by cell height descending, then glyph ID ascending, which packs densely and makes the cooked pages a pure function of the inputs. Cost O(segments) per placement.

### Dynamic glyph state machine

A glyph not in the baked set of its face (CJK beyond the baked set, user-entered text, a code point of a script fallback family) is generated at run time into a dynamic page when the family allows it (`dynamic: true`) and a renderer exists:

| State | Event | Next | Action |
|---|---|---|---|
| ABSENT | a layout draws it (`Request`) | REQUESTED | append to the context's request list (duplicates within a frame ignored) |
| REQUESTED | `ProcessRequests` with budget left (never headless: with no renderer no glyph leaves REQUESTED for GENERATED, see below) | GENERATED | `Zenith_GenerateGlyphMSDF` into a free cell of the current dynamic page's CPU pixels; the page is marked dirty |
| REQUESTED | `ProcessRequests` with the budget used up | REQUESTED | kept for the next frame (the per-frame budget is `ui.text.dynamicGlyphsPerFrame`) |
| REQUESTED | the outline is empty or generation fails | EMPTY | a placement with no image (`m_bEmpty`); counted `ui.glyphs.generation_failed` when it failed |
| GENERATED | `EndFrame`: the page's upload slot is granted | UPLOADING | `CreatePage` with the page's full pixels (at most `ui.atlas.pageUploadsPerFrame` pages per frame); the previous texture of the page stays published |
| UPLOADING | `IsPageResident(new)` | RESIDENT | publish the new texture for every glyph of the page; release the previous texture (deferred deletion) |
| UPLOADING | `CreatePage` failed | GENERATED | retried at the next `EndFrame`; counted `ui.atlas.upload_failed` |
| RESIDENT | its page is recycled (below) | ABSENT | the glyph is requested again by the next frame that draws it |
| any | the font asset unloads | ABSENT | the font's dynamic glyphs are dropped; pages with no live glyph are released |

- A dynamic page is full when no cell fits; a new dynamic page is opened while fewer than `ui.atlas.maxDynamicPages` exist; otherwise the dynamic page whose glyphs were drawn least recently (each page keeps the last frame any of its glyphs was looked up) is recycled when that frame is at least 120 frames ago; if none qualifies, the request stays REQUESTED and counts `ui.atlas.dynamic_full`.
- A glyph in REQUESTED, GENERATED or UPLOADING draws nothing (its layout and metrics are unaffected, so text never reflows when glyphs arrive); `Zenith_UIContextStats::m_uMissingGlyphs` counts such glyphs per frame; a paragraph that needs a new glyph shows it at most `1 + ⌈queued / budget⌉` frames after its first draw.
- Headless (no renderer, `Zenith2/Docs/Design/UI/Rendering.md → "Headless and device loss"`): the glyph cache creates no page and generates no MSDF; a lookup reports every glyph EMPTY for drawing; layout, measurement and hit testing are unchanged, because they read face metrics and advances, never pages.

### MSDF generation

`Zenith_GenerateGlyphMSDF`:

1. Read the outline (`Zenith_UIOutlineSource::GetOutline`) in font units; an empty outline returns an empty glyph.
2. Scale `k = emPixels / unitsPerEm`; pad `p = ⌈rangePixels / 2⌉ + padding`; cell size `w = ⌈(xMax − xMin) × k⌉ + 2p`, `h = ⌈(yMax − yMin) × k⌉ + 2p`, at most 255 × 255 (a larger glyph is scaled down by an integer factor and its plane bounds scaled up, flagged).
3. Build the msdfgen `Shape` from the contours (lines, quadratic and cubic segments), `normalize()`, colour edges with `edgeColoringByDistance` at an angle threshold of 3 radians and seed 0.
4. Generate the multi-channel and true signed distances (`generateMTSDF`) with range `rangePixels` pixels, the transformation `scale k`, `translate (p/k − xMin, p/k − yMin)` with the y axis flipped to rows top-down, and msdfgen's edge-priority error correction.
5. Quantise each channel: `round_half_even(clamp(0.5 + d / rangePixels, 0, 1) × 255)` into RGBA8 (A the true distance).
6. Plane bounds in em: left `(xMin × k − p) / emPixels`, top `−(yMax × k + p) / emPixels` (+Y down), right and bottom correspondingly.

The generation is a pure function of the program bytes, glyph and parameters within one build: baked and dynamic glyphs use this one function, so on one build they are bit-identical (`Test_FontBake_BakedEqualsDynamic` bakes and generates in the same test executable). Across builds (the cook on the Windows host, dynamic generation on the S24) floating-point code generation differs, so a baked page and a page generated on the device are visually equivalent, not bit-identical: the quantised channels may differ by one step, which moves an edge by at most `rangePixels / 255` pixels, below the antialiasing width; no rule depends on cross-build identity.

### Page upload and residency

- Page creations, static and dynamic together, are limited to `ui.atlas.pageUploadsPerFrame` per frame, so UI's page upload traffic per frame is at most that count × the page bytes (8 MiB at the defaults), measured against `Z2-B-RHI-006` (`Zenith2/Docs/Design/UI.md → "Rows measured against"`); a page beyond the limit waits for the next frame.
- When a font asset becomes ACTIVE (observed in the prologue), the glyph cache creates one GPU page per baked page through `Flux_UIRenderer::CreatePage` with the asset's page pixels and registers every baked glyph's placement; placements reference a page's descriptor only after `IsPageResident` is true, so no frame samples an unfilled texture ([Z2-N-REND-548]).
- A dynamic page keeps its CPU pixels (1 MiB per 512², 4 MiB per 1024², 16 MiB per 2048²), and every change to it is published by creating a new texture from the full pixels and releasing the old one once the new one is resident: the reallocate-and-swap pattern of `Zenith2/Docs/Rendering/GpuResources.md → "Texture streaming swap"`, because `Flux_PersistentReadOnly` textures are never written in place ([Z2-N-REND-554]).
- At memory pressure WARNING the cache recycles every dynamic page not drawn for 300 frames; at CRITICAL it also drops the CPU pixels of baked pages (`Zenith_FontAsset::DropBakedPixels`) whose GPU page is resident, the "decoded CPU copies of GPU-resident assets" of the Assets row of `Zenith2/Docs/Architecture.md → "Memory governor"` ("Shedding per subsystem").
- Device loss: the renderer's recovery recreates no UI page itself; at the first frame after recovery the glyph cache recreates every page from CPU pixels, and a baked page whose pixels were dropped is restored by requesting a reload of its font asset (`Zenith_AssetRegistry::RequestReload`), its glyphs EMPTY for drawing until then.

## Text layout cache

- Each UI context has its own partition of the cache (no sharing, no lock): at most `ui.text.layoutCacheEntries` entries (default 2,048) and a byte cap of one eighth of the UI memory row's host share (Z2-B-UI-001, pending), whichever is reached first.
- Key: `Zenith_Hash64` over the text bytes, every span's style hash (font stack identity, size, weight, italic, colour excluded because it does not affect layout, line height, letter spacing, tabular numbers), the width quantised to 1/64 unit, max lines, max height, wrap, overflow, align, direction, language, the localization revision (a locale switch), the font registry revision (a face opening or a font becoming ACTIVE) and the pixel scale for snapped layouts; equality of the key's inputs is checked on a hit, so a hash collision never returns a wrong layout.
- Entries live in fixed 4 KiB blocks from a `Zenith_Pool` (a layout larger than one block chains blocks); eviction is least recently used; a lookup returns a layout valid until the end of the current `Update`.
- `Zenith_UIContextStats::m_uTextLayouts` and `m_uTextCacheHits` report misses and hits; a frame with no text change shapes nothing (`Test_UITextCache_SteadyStateShapesNothing`).

## Text conformance corpus

`Zenith2/Engine/ZenithUI/Tests/Fixtures/Text/Corpus/` holds one JSON file per case group, in the format of "Corpus file format" below, and the group index `Groups.json`; each case gives the text, the style (family from the corpus fonts or the test font, size, direction, width) and the expectations:

| Group | Cases | Expectations |
|---|---|---|
| `Latin.json` | accents (precomposed and combining), `fi`/`fl` ligatures, kerning pairs (`AV`, `To`), soft hyphen, NBSP | glyph IDs, clusters and advances (shaping golden); break offsets |
| `TestFont.json` | the test font's `fi` ligature, `AV` kern, `e` + U+0301 mark, a Hebrew word | glyph IDs, clusters, advances and offsets written by hand |
| `Greek.json`, `Cyrillic.json` | final sigma, Serbian italic forms by language `sr` | shaping golden |
| `Arabic.json` | joining forms, lam-alef ligature, harakat, numbers in RTL, tatweel, Persian `fa` digits | shaping golden; visual order |
| `Hebrew.json` | niqqud marks, mixed Hebrew and Latin, parentheses mirroring | visual order; caret positions |
| `Bidi.json` | the UAX #9 conformance subset: explicit embeddings, isolates (FSI with numbers), neutrals between runs, trailing whitespace (L1); an RTL sentence of the test font's Hebrew letters with a `<b>` middle word (three shaping runs at level 1: the bold word uses the weight-700 copy of the test font family); an LTR paragraph `a ` + U+202B (RLE) + two test-font Hebrew letters + two spaces + a tab, whose trailing spaces and tab are level 1 inside the embedding and reset to level 0 by L1 | levels per code point and visual order, hand-derivable from the rules (the bold case: the three runs in reversed order, each run's glyphs in the shaper's RTL order; the L1 case: `a`, the space, the two Hebrew letters in reversed order, then the two spaces and the tab in logical order, with their cluster map and caret positions) |
| `Devanagari.json` | conjuncts, reph, pre-base matra, nukta | shaping golden; grapheme boundaries (GB9c) |
| `Thai.json` | sentences without spaces built only from words of the pinned `icu-dictionaries` Thai word list, tone marks above vowels | SA dictionary break offsets recorded with ICU4C's line break iterator by the Stage 2 recording ticket, before the D-SEL-TEXT ruling (item 4 below, "Corpus file format"); shaping golden |
| `CJK.json` | Han, Kana, Hangul; kinsoku (no break before 、。」); `ja` strict versus `zh` normal | break offsets; advances |
| `Emoji.json` | ZWJ families, skin-tone modifiers, flags (regional indicator pairs), keycaps | grapheme boundaries; fallback to the emoji family |
| `Pseudo.json` | pseudo-localized long strings (`qps-ploc`) and mirrored (`qps-plocm`), laid out in the test font (its accented letters are `.notdef` there, advance 500 like every spacing glyph, so every line count is derivable by hand) | wrap and ellipsis line counts and truncation flags, hand-written; part of the D-SEL-TEXT selection set, because they depend on each stack's `FindBreaks` alone |
| `Fallback.json` | Latin text with embedded Arabic, CJK and an unsupported code point | per-cluster face selection; one `.notdef` |

<!-- elaboration: the shaping oracles: hand-derived cases over the generated test font, and goldens recorded with an independent tool -->
Shaping is checked against two oracles, neither of which is the code under test:

1. **Hand-derived cases over the generated test font** (`Tests/Fixtures/Text/Corpus/TestFont.json`, the font of `Zenith2/Docs/Design/UI/Tests.md → "Fixtures"`): the test font builder writes, besides its fixed glyph set, a GSUB ligature `f` + `i` → glyph `fi` (ID 200), a GPOS pair kern `A` `V` of −100 units, a mark-to-base attachment for U+0301 on `e` (the base anchor on glyph `e` at (250, 700) font units, the mark anchor on the U+0301 glyph at (250, 700)), and an RTL run of the test font's Hebrew range. The expected glyph IDs, clusters, advances and offsets of `fi`, `AV`, `é` (decomposed) and a three-letter Hebrew word are written by hand from those tables and the 500-unit advance (`Test_UITextCorpus_TestFontShaping`): for `e` + U+0301, the mark's pen position is the base origin plus the base advance of 500 (the mark glyph's own advance is 0, GDEF class 3), and anchoring puts the mark's origin at base origin + 250 − 250 = the base origin, so HarfBuzz gives the mark x offset 0 − 500 = −500 and y offset 700 − 700 = 0, that is offsets (−500, 0) and advances (500, 0); `TestFont.json`'s description of that case states the two anchors and these offsets. These run in every gate and prove the pipeline's wiring (feature mask, direction, cluster mapping, offsets) independently of any recorded data.
2. **Recorded goldens for the Noto corpus**: shaping output for real fonts cannot be derived by hand, so it is recorded (golden files are hand-written only where the output is derivable; this output is not) by a `needs-human` recording ticket that runs `hb-shape` of the pinned HarfBuzz release (the HarfBuzz project's own command-line tool, built from the same pinned source by the recording ticket's human, an independent program from ZenithUI's pipeline) on the pinned Noto fonts with exactly the parameters ZenithUI passes for each case: `--script`, `--language`, `--direction`, `--features=kern,liga,clig,calt[,tnum]` and `--output-format=json`, and commits the output as `<Group>.expected.json` (glyph ID, cluster, x advance, x and y offset per glyph). The human also renders each case's sample with a reference text renderer of their choice (a browser with the same font files) and confirms the glyph sequence looks right before committing; the ticket's description names that check. The recording ticket is scheduled after the D-SEL-TEXT ruling, in the Stage 3 text slice, so the goldens never take part in the selection.
3. **A missing golden fails**: `Test_UITextCorpus_Shaping` reads `<Group>.expected.json` for every group whose entry in the corpus index `Tests/Fixtures/Text/Corpus/Groups.json` has `"shapingGolden": true`; a missing or unparsable golden file fails the test with the group's name. `Groups.json` lists every group from slice 3 on, and every corpus test, the selection test included, enumerates the groups through it; the flag is false for every group until the Noto shaping-golden recording ticket (slice 13) commits the goldens and sets it for each group it records, so the gate never runs a shaping group without its golden, and a group is never reported as passing on reduced assertions.

4. **SA break offsets, recorded before the ruling**: SA dictionary segmentation is the one Thai behaviour in which the bidi and segmentation candidates differ (ZenithUI's maximal-matching segmenter over `icu-dictionaries` in B1, the ICU4X segmenter in B2), so it is part of the D-SEL-TEXT comparison, and its oracle depends on no candidate. The offsets are not derivable by hand (they depend on a dictionary and ICU's longest-match heuristics), so, following the golden-file rule (hand-written goldens only for derivable output), they are recorded by a `needs-human` recording ticket in the Stage 2 sprint, blocked by slice 3's ticket that commits the corpus case texts and blocking the D-SEL-TEXT harness ticket (`Zenith2/Docs/Design/UI.md → "Ticket slices and ordering"`, slice 4). It runs a committed record case of the committed tree, as every recording ticket of the programme does (the precedent is GEOM's QTangent fixture recording): on a clean checkout at a commit containing slice 3, the human builds `ZenithCook_Tests` (`Build -Preset win-debug`), runs the skipped record case `Test_UITextCorpus_SaBreaks_Record` with `--no-skip` and the environment variable `ZENITH2_UI_RECORD_SA_BREAKS` naming the corpus directory, which records through the pinned `icu4c` entry, the ICU4C release that publishes the pinned `icu-dictionaries` data, every SA case's `breaks` member and the group's `icuVersion` and `recordedWith` members ("Corpus file format"), and commits the corpus files alone; no engine code is in the diff. An upgrade of `icu4c` and `icu-dictionaries`, which move together, re-runs the same record case in its own `needs-human` ticket. Before recording, the human checks every word of every case against the pinned word list and removes a case that uses a word absent from it, writing the case and the reason into the group's `removed` member, so no case is removed after any candidate has run. The selection test compares each candidate's `Zenith_UILineSegmenter::FindBreaks` output with the recorded offsets, and after the ruling `Test_UITextCorpus_Breaks` compares the winner's; a listed SA case without recorded offsets is a harness error, never a pass. The limit is stated: ZenithUI guarantees SA segmentation equal to ICU4C's on the corpus, not on arbitrary text.

- The corpus fonts are the Noto families pinned as the data entry `noto-fonts` (OFL-1.1, cross-area row UI9); the shaping goldens depend on the pinned font and HarfBuzz versions, so a font or HarfBuzz upgrade re-records them with `hb-shape` in its own `needs-human` ticket; there is one golden per group, not one per candidate.
- The expectations that follow from the Unicode rules alone (bidi levels, visual order, grapheme, word and line break offsets outside SA ranges) are written by hand in the case files and need no recording; `Bidi.json` includes a mixed-direction case (an LTR sentence containing a Hebrew word whose second cluster is a base plus a niqqud mark, two glyphs) whose visual glyph order and caret x positions are written by hand, and the RTL case with a bold middle word (three level-1 shaping runs), whose expected visual run order is the reverse of the logical one.
- `Test_UITextCorpus_Shaping`, `Test_UITextCorpus_Bidi`, `Test_UITextCorpus_Breaks`, `Test_UITextCorpus_Graphemes` and `Test_UITextCorpus_Fallback` run the groups, and `Test_UITextSelection_CorpusBothCandidates` compares the candidates while D-SEL-TEXT is open (`Zenith2/Docs/Design/UI/Tests.md → "Shaping golden suite"` says which exists when).

### Corpus file format

<!-- elaboration: the corpus schema shared by slice 3, the SA recording, the selection harness, the ruling and slice 13 -->
Every file under `Tests/Fixtures/Text/Corpus/` is UTF-8 JSON without a byte-order mark; slice 3 writes them by hand, the SA recording ticket adds the `breaks` members of the SA groups and the members that state how they were recorded, and slice 13 adds the shaping goldens and their flags. The group index:

```json
{ "groups": [ { "group": "Thai", "file": "Thai.json", "sa": true, "shapingGolden": false } ] }
```

| Member | Meaning |
|---|---|
| `group` | the group name; `<group>.expected.json` is its shaping golden |
| `file` | the case file, in the corpus directory |
| `sa` | true for a group whose every case's `breaks` is recorded with ICU4C (today `Thai.json` alone); false where the breaks are hand-written |
| `shapingGolden` | true once the slice-13 recording committed `<group>.expected.json`; `Test_UITextCorpus_Shaping` runs exactly the groups with true |

A case file:

```json
{
  "group": "Thai",
  "description": "free text: what the group covers",
  "fonts": [ "thirdparty:noto-fonts/<path>/NotoSansThai-Regular.ttf" ],
  "icuVersion": "",
  "recordedWith": "",
  "removed": [ { "id": "th-07", "reason": "uses a word absent from the pinned Thai word list" } ],
  "cases": [
    {
      "id": "th-01",
      "text": "<UTF-8 text>",
      "style": { "family": "Noto Sans Thai", "size": 20, "weight": 400, "italic": false, "direction": "auto",
                 "width": 0, "language": "th", "features": [ "kern", "liga", "clig", "calt" ],
                 "wrap": "word", "overflow": "visible", "maxLines": 0,
                 "spans": [ { "byteStart": 0, "byteEnd": 4, "weight": 700, "italic": false } ] },
      "expect": {
        "glyphs": [ { "glyph": 200, "cluster": 0, "advanceX": 500, "offsetX": 0, "offsetY": 0 } ],
        "levels": [ 0 ], "visualOrder": [ 0 ], "graphemes": [ 2 ], "words": [ 3 ],
        "breaks": { "allowed": [ 9 ], "mandatory": [] },
        "faces": [ { "byte": 0, "face": "Noto Sans Thai 400", "notdef": false } ],
        "lines": 1, "truncated": false, "caretX": [ 0, 10 ]
      }
    }
  ]
}
```

| Member | Meaning |
|---|---|
| `fonts` | the corpus face files the group's families come from, as `thirdparty:` paths of `noto-fonts` (row UI9); empty for groups in the test font |
| `icuVersion`, `recordedWith` | SA groups only, written by the recording: the ICU4C version (`U_ICU_VERSION`) and the record case's procedure string (below); empty in slice 3's files |
| `removed` | SA groups only: the cases the human removed before recording, each with its reason; a removed case's `id` is absent from `cases` |
| `style.family` | a family of `fonts`, or `test` and `test-bold` for the test font family's weights 400 and 700 (`Zenith2/Docs/Design/UI/Tests.md → "Fixtures"`) |
| `style.size`, `weight`, `italic` | canvas units per em; 100 to 900; bool (the defaults: 16, 400, false) |
| `style.direction` | `auto`, `ltr` or `rtl`, the paragraph's `Zenith_TextDirection` |
| `style.width`, `wrap`, `overflow`, `maxLines` | `Zenith_TextParagraphDesc::m_fMaxWidth` (0 unbounded), `m_eWrap` (`none`, `word`, `anywhere`), `m_eOverflow` (`visible`, `clip`, `ellipsis`), `m_uMaxLines` |
| `style.language` | the BCP 47 tag passed to shaping and line breaking; empty for none |
| `style.features` | the shaper features, a subset of `kern`, `liga`, `clig`, `calt`, `tnum`; absent means the four always on |
| `style.spans` | optional style ranges over the text's UTF-8 bytes (the bold middle word); bytes outside every span take the case's style |
| `expect.glyphs` | hand-written shaping output (`TestFont.json` alone): per glyph in the shaper's output order, the glyph ID, cluster (UTF-8 byte offset), x advance and x and y offsets, in font units; the Noto groups' shaping goldens are `<group>.expected.json`, never this member |
| `expect.levels` | the embedding level of each code point, in logical order |
| `expect.visualOrder` | the logical code-point indices of the laid-out line in left-to-right visual order |
| `expect.graphemes`, `expect.words` | UTF-8 byte offsets of grapheme-cluster and word boundaries strictly inside the text, ascending |
| `expect.breaks` | `allowed` and `mandatory`: UTF-8 byte offsets of line-break opportunities strictly inside the text, ascending; 0 and the text's byte length are never listed |
| `expect.faces` | per grapheme cluster, its first byte, the chosen face as `<family> <weight>` (with ` italic` when italic), and whether it drew `.notdef` |
| `expect.lines`, `expect.truncated` | the line count and `m_bTruncated` of the laid-out paragraph |
| `expect.caretX` | the caret x position of every caret position in logical order, 0 and the end included, canvas units |

A case lists only the members of `expect` its group asserts; a test reading a member a case omits skips that assertion for that case and reports nothing about it, and a group whose table row above names an expectation lists that member in every case.

**Break offsets and `FindBreaks`.** `Zenith_UILineSegmenter::FindBreaks` writes n + 1 classes for n code points: entry i is the boundary before code point i (entry 0 the text start, entry n its end). A comparison reads entries 1 to n − 1 alone: entry i with class 1 contributes the UTF-8 byte offset of code point i to `allowed`, with class 2 to `mandatory`, and class 0 contributes nothing; the two lists must equal the case's `breaks` members exactly. The same conversion serves the hand-written breaks of every non-SA group and the recorded ones.

**The SA record case.** `Test_UITextCorpus_SaBreaks_Record` (`Zenith2/Tools/ZenithCook/Tests/UI/Zenith_UISaBreakRecord_Tests.cpp`, `ZenithCook_Tests`, skipped with `doctest::skip()`) runs only with `--no-skip` and `ZENITH2_UI_RECORD_SA_BREAKS` set to the absolute corpus directory, and fails without it. For every group of `Groups.json` with `"sa": true` and every case of its `cases`, it creates `icu::BreakIterator::createLineInstance(icu::Locale("th"), status)` of the `icu4c` entry (the C++ API of the release that publishes the pinned `icu-dictionaries`), sets the case text converted to UTF-16 (`icu::UnicodeString::fromUTF8`), iterates `next()` until `icu::BreakIterator::DONE`, classes each boundary b as mandatory when `getRuleStatus()` is at least `UBRK_LINE_HARD` (100) and allowed otherwise, converts b from a UTF-16 index to a UTF-8 byte offset, drops 0 and the text's byte length, and writes the two ascending lists as the case's `breaks`; it writes `U_ICU_VERSION` into `icuVersion` and the procedure string `icu4c <version> BreakIterator::createLineInstance(th); next(); getRuleStatus() >= UBRK_LINE_HARD is mandatory; UTF-16 index to UTF-8 offset; 0 and end dropped` into `recordedWith`, and rewrites each file with its members in the order above, two-space indentation and LF line endings, changing no other member. A failing `status` fails the case naming the group and case. The recording function is shared with `Test_UITextCorpus_SaBreakRecorderConverts`, which runs in every gate (`Zenith2/Docs/Design/UI/Tests.md → "Shaping golden suite"`), so the converter is tested before any human runs the record case.
