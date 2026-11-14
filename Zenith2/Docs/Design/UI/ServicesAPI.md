# UI design: public API of text, fonts, localization, accessibility, scripting, editing and the GPU side

This file is part of the UI design family; the entry document is `Zenith2/Docs/Design/UI.md`. It continues `Zenith2/Docs/Design/UI/PublicAPI.md` (the header inventory, the module, context, document and style headers) with the remaining public headers of `ZenithUI`, the UI-owned public header of `ZenithFlux` and the tool-side headers, each with its declarations and one contract block per type.

## Scope of this file

| Topic | Section |
|---|---|
| text layout, the edit buffer | "Zenith_UIText header" |
| fonts, the glyph cache, baking | "Zenith_UIFont header", "Zenith_UIFontBake header" |
| localization | "Zenith_UILocalization header" |
| accessibility | "Zenith_UIAccessibility header" |
| the Luau surface | "Zenith_UIScriptApi header" |
| visual authoring | "Zenith_UIEditorModel header" |
| the GPU side in ZenithFlux | "Flux_UI header" |
| ZenithCook files | "Tool-side headers" |

## Zenith_UIText header

```cpp
// Zenith2/Engine/ZenithUI/Zenith_UIText.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_Arena.h"
#include "ZenithCore/Zenith_Heap.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Colour.h"
#include "ZenithUI/Zenith_UITypes.h"
#include "ZenithUI/Zenith_UIFont.h"            // Zenith_FontStack, Zenith_GlyphKey (Zenith_UIFont.h includes no text header)

/// @contract enum Zenith_TextDirection
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3 (AUTO resolves to the locale's direction until bidi lands in Z2 Stage 4).
/// @endcontract
enum Zenith_TextDirection : uint8 { TEXT_DIRECTION_AUTO = 0, TEXT_DIRECTION_LTR, TEXT_DIRECTION_RTL };

/// @contract enum Zenith_TextAlign
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_TextAlign : uint8 { TEXT_ALIGN_START = 0, TEXT_ALIGN_END, TEXT_ALIGN_CENTRE, TEXT_ALIGN_JUSTIFY };

/// @contract enum Zenith_TextWrap
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_TextWrap : uint8 { TEXT_WRAP_NONE = 0, TEXT_WRAP_WORD, TEXT_WRAP_ANYWHERE };

/// @contract enum Zenith_TextOverflow
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_TextOverflow : uint8 { TEXT_OVERFLOW_VISIBLE = 0, TEXT_OVERFLOW_CLIP, TEXT_OVERFLOW_ELLIPSIS };

/// @contract value Zenith_TextStyle
/// Ownership: Plain value; m_pxFonts is borrowed from the font registry for the style's lifetime.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: m_pxFonts until the stack is released (a locale switch rebuilds stacks between frames).
/// Units: m_fSize in canvas units per em; m_fLineHeight a multiple of the font's ascent + descent; m_fLetterSpacing in em.
/// Errors: None; non-finite or non-positive sizes are clamped to 1 at use (counted).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_TextStyle
{
	const Zenith_FontStack* m_pxFonts = nullptr;
	float m_fSize = 16.0f;
	uint16 m_uWeight = 400;               // 100–900
	bool m_bItalic = false;
	Zenith_Colour m_xColour{ 1.0f, 1.0f, 1.0f, 1.0f };
	float m_fLineHeight = 1.2f;
	float m_fLetterSpacing = 0.0f;
	Zenith_Colour m_xOutlineColour{ 0.0f, 0.0f, 0.0f, 0.0f };
	float m_fOutlineWidth = 0.0f;          // em
	Zenith_Colour m_xShadowColour{ 0.0f, 0.0f, 0.0f, 0.0f };
	Zenith_Vector2 m_xShadowOffset;        // em
	bool m_bTabularNumbers = false;        // the OpenType 'tnum' feature
	bool m_bUnderline = false;
};

/// @contract value Zenith_TextSpan
/// Ownership: Plain value; a style range over the paragraph's UTF-8 bytes.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: Byte offsets into the paragraph text.
/// Errors: None; spans are validated by Layout (TEXT_LAYOUT_ERROR_BAD_SPAN).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_TextSpan
{
	uint32 m_uByteStart = 0;
	uint32 m_uByteEnd = 0;
	Zenith_TextStyle m_xStyle;
	uint32 m_uInlineObject = 0;           // non-zero: the span is one U+FFFC replaced by an inline object (prompt glyph, icon)
	Zenith_UISize m_xInlineSize;           // the inline object's size, canvas units
};

/// @contract value Zenith_TextParagraphDesc
/// Ownership: Plain value; text and spans are borrowed for the Layout call.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: Canvas units; m_fMaxWidth ≤ 0 means unbounded.
/// Errors: Validated by Layout.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_TextParagraphDesc
{
	Zenith_StringView m_strText;           // valid UTF-8; U+2029 and LF separate paragraphs
	Zenith_Span<const Zenith_TextSpan> m_axSpans;   // sorted, non-overlapping, covering every byte
	float m_fMaxWidth = 0.0f;
	uint32 m_uMaxLines = 0;               // 0 unbounded
	float m_fMaxHeight = 0.0f;             // ≤ 0 unbounded
	Zenith_TextAlign m_eAlign = TEXT_ALIGN_START;
	Zenith_TextWrap m_eWrap = TEXT_WRAP_WORD;
	Zenith_TextOverflow m_eOverflow = TEXT_OVERFLOW_VISIBLE;
	Zenith_TextDirection m_eDirection = TEXT_DIRECTION_AUTO;
	Zenith_StringView m_strLanguage;       // BCP 47 tag for shaping and line breaking; the active locale when empty
	bool m_bMeasureOnly = false;           // produce metrics without glyph positions (layout measure pass)
	float m_fPixelScale = 1.0f;            // pixels per canvas unit: glyph positions snap to it when the caller requests snapping
};

/// @contract value Zenith_TextGlyph
/// Ownership: Plain value in the layout's arena.
/// Thread affinity: IMMUTABLE; members non-const: an element its producer writes in place in an arena or Zenith_Array.
/// Invalidation: With the layout.
/// Units: Canvas units relative to the paragraph origin (top-left of the first line box); x is the pen position.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_TextGlyph
{
	uint32 m_uGlyphKey = 0;               // index into the layout's glyph-key table (font face, glyph ID)
	float m_fX = 0.0f;
	float m_fY = 0.0f;                     // baseline
	float m_fAdvance = 0.0f;
	uint32 m_uClusterByte = 0;            // the first byte of the cluster this glyph belongs to
	uint16 m_uSpan = 0;                   // index into the paragraph's spans
	uint8 m_uLevel = 0;                   // bidi embedding level
	uint8 m_uFlags = 0;                   // bit 0 cluster start, bit 1 ellipsis, bit 2 inline object, bit 3 notdef
};

/// @contract value Zenith_TextLine
/// Ownership: Plain value in the layout's arena.
/// Thread affinity: IMMUTABLE; members non-const: an element its producer writes in place in an arena or Zenith_Array.
/// Invalidation: With the layout.
/// Units: Canvas units.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_TextLine
{
	uint32 m_uByteStart = 0;
	uint32 m_uByteEnd = 0;                // excludes the break's trailing whitespace and newline
	uint32 m_uFirstGlyph = 0;             // glyphs of a line are stored in visual order
	uint32 m_uGlyphCount = 0;
	float m_fBaseline = 0.0f;
	float m_fAscent = 0.0f;
	float m_fDescent = 0.0f;
	float m_fWidth = 0.0f;                 // the advance width of the visible content
	float m_fX = 0.0f;                     // the line's x offset after alignment
	bool m_bTruncated = false;
	bool m_bHardBreak = false;
};

/// @contract value Zenith_TextCaret
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: a value type its holders assign and reassign.
/// Invalidation: Valid against the layout it came from.
/// Units: m_uByte a byte offset at a grapheme-cluster boundary; positions in canvas units.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_TextCaret
{
	uint32 m_uByte = 0;
	bool m_bTrailingAffinity = false;      // at a bidi boundary: the caret belongs to the run before the byte
	uint32 m_uLine = 0;
	float m_fX = 0.0f;
	float m_fTop = 0.0f;
	float m_fBottom = 0.0f;
};

/// @contract value Zenith_TextLayout
/// Ownership: Plain value whose spans live in the arena given to Layout (or in the layout cache's own storage for cached
///   layouts the engine hands out).
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: With its arena or cache entry; a cached layout lives until the end of the Update that obtained it.
/// Units: Canvas units.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_TextLayout
{
	Zenith_Span<const Zenith_TextLine> m_axLines;
	Zenith_Span<const Zenith_TextGlyph> m_axGlyphs;
	Zenith_Span<const Zenith_GlyphKey> m_axGlyphKeys;
	Zenith_UISize m_xSize;                 // width: widest line; height: last line's bottom
	float m_fMinContentWidth = 0.0f;       // the widest unbreakable segment
	float m_fMaxContentWidth = 0.0f;       // the width with no wrapping
	float m_fFirstBaseline = 0.0f;
	uint8 m_uBaseLevel = 0;               // 0 LTR, 1 RTL
	bool m_bTruncated = false;
	uint32 m_uMissingGlyphs = 0;          // code points with no font in the stack (drawn as .notdef)
};

/// @contract enum Zenith_TextLayoutError
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: TEXT_LAYOUT_ERROR_BAD_UTF8 (invalid UTF-8), _BAD_SPAN (spans unsorted, overlapping, not covering, or past the
///   text), _NO_FONT (a span without a font stack), _TOO_LONG (more than uTEXT_MAX_PARAGRAPH_BYTES), _OUT_OF_MEMORY (the
///   arena is exhausted), _SHAPER (the shaping backend failed).
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_TextLayoutError : uint8
{
	TEXT_LAYOUT_ERROR_BAD_UTF8 = 1, TEXT_LAYOUT_ERROR_BAD_SPAN, TEXT_LAYOUT_ERROR_NO_FONT, TEXT_LAYOUT_ERROR_TOO_LONG,
	TEXT_LAYOUT_ERROR_OUT_OF_MEMORY, TEXT_LAYOUT_ERROR_SHAPER,
};
inline constexpr uint32 uTEXT_MAX_PARAGRAPH_BYTES = 65536;
constexpr const char* Zenith_ToString(Zenith_TextLayoutError eError);

/// @contract type Zenith_TextLayoutEngine
/// Ownership: Owned by Zenith_UISystem; holds the shaping backend (D-SEL-TEXT, open, Stage 1/2), the bidi backend, the
///   Unicode property tables, the line-break segmenter and the layout cache.
/// Thread affinity: ANY_THREAD. Layout is reentrant across threads: each call uses the caller's arena and a per-thread
///   shaping scratch obtained from a fixed pool of uTEXT_SHAPER_SLOTS (16) slots; the cache is partitioned per UI context
///   (CachedLayout takes the context's partition), so no lock is taken.
/// Invalidation: Layouts from CachedLayout are valid until the end of the owning context's Update.
/// Units: Canvas units.
/// Errors: Zenith_Result<Zenith_TextLayout, Zenith_TextLayoutError>; malformed text never asserts ([Z2-N-CONTRACT-472]).
/// Version: since Z2 Stage 3. Deterministic: identical inputs, fonts and backend give identical layouts.
/// @endcontract
class Zenith_TextLayoutEngine
{
public:
	Zenith_Result<Zenith_TextLayout, Zenith_TextLayoutError> Layout(const Zenith_TextParagraphDesc& xDesc,
		Zenith_Arena& xArena);
	// The layout cache of Text.md "Text layout cache": uContextTag selects the context's partition (created by
	// Zenith_UIContext::Create, released with it); a hit returns the stored layout, a miss runs Layout into the partition's
	// blocks. The result is valid until the end of that context's Update. Only the owning context's thread calls it.
	Zenith_Result<const Zenith_TextLayout*, Zenith_TextLayoutError> CachedLayout(const Zenith_TextParagraphDesc& xDesc,
		uint16 uContextTag);
	Zenith_TextCaret HitTest(const Zenith_TextLayout& xLayout, Zenith_StringView strText, Zenith_Vector2 xPoint) const;
	Zenith_TextCaret CaretAt(const Zenith_TextLayout& xLayout, Zenith_StringView strText, uint32 uByte,
		bool bTrailingAffinity) const;
	Zenith_TextCaret MoveCaretVisual(const Zenith_TextLayout& xLayout, Zenith_StringView strText,
		const Zenith_TextCaret& xCaret, sint32 iSteps) const;               // left/right through visual order
	uint32 NextGraphemeBoundary(Zenith_StringView strText, uint32 uByte) const;
	uint32 PreviousGraphemeBoundary(Zenith_StringView strText, uint32 uByte) const;
	uint32 NextWordBoundary(Zenith_StringView strText, uint32 uByte) const;
	uint32 PreviousWordBoundary(Zenith_StringView strText, uint32 uByte) const;
	Zenith_Span<const Zenith_UIRect> GetSelectionRects(const Zenith_TextLayout& xLayout, Zenith_StringView strText,
		uint32 uByteStart, uint32 uByteEnd, Zenith_Arena& xArena) const;
};
inline constexpr uint32 uTEXT_SHAPER_SLOTS = 16;

/// @contract functions Zenith_UIRepairUtf8
/// Ownership: Writes into the caller's arena; owns nothing.
/// Thread affinity: ANY_THREAD.
/// Invalidation: The returned view lives as long as the arena allocation (or aliases strText when nothing was repaired).
/// Units: Bytes.
/// Errors: Never fails on malformed input: every maximal ill-formed subsequence (the Unicode "U+FFFD substitution of maximal
///   subparts" practice) becomes U+FFFD; returns TEXT_LAYOUT_ERROR_OUT_OF_MEMORY only when the arena cannot hold the copy.
/// Version: since Z2 Stage 3. The decoding step every text-value path runs before Layout (Text.md "Input and decoding").
/// @endcontract
Zenith_Result<Zenith_StringView, Zenith_TextLayoutError> Zenith_UIRepairUtf8(Zenith_StringView strText,
	Zenith_Arena& xArena, uint32& uRepairedOut);

/// @contract type Zenith_TextEditBuffer
/// Ownership: Owned by the text-field element state of a UI context, or by a test; the UTF-8 text lives in the general heap
///   with a fixed capacity chosen at Create.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: GetText views are invalid after the next mutating call.
/// Units: Byte offsets at grapheme-cluster boundaries.
/// Errors: Insert returns false and changes nothing when the result would exceed the capacity or contain invalid UTF-8,
///   NUL or a control character other than LF in a multiline buffer; nothing asserts.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_TextEditBuffer
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_TextEditBuffer>, Zenith_UIError> Create(Zenith_Heap& xHeap,
		uint32 uCapacityBytes, bool bMultiline);
	Zenith_StringView GetText() const;
	uint32 GetCaret() const;
	uint32 GetAnchor() const;                         // selection is [min(anchor, caret), max)
	bool Insert(Zenith_StringView strText);             // replaces the selection
	void DeleteBackward(const Zenith_TextLayoutEngine& xEngine, bool bWord);
	void DeleteForward(const Zenith_TextLayoutEngine& xEngine, bool bWord);
	void SetCaret(uint32 uByte, bool bExtendSelection);              // snapped to a grapheme boundary
	void SelectAll();
	void SetComposition(Zenith_StringView strText, sint32 iCursor, sint32 iSelectionLength);
	void EndComposition();
	Zenith_StringView GetComposition() const;
	Zenith_StringView GetSelectedText() const;
	void Clear();
};
```

## Zenith_UIFont header

```cpp
// Zenith2/Engine/ZenithUI/Zenith_UIFont.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithAssets/Zenith_AssetTypes.h"
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithRHI/Zenith_RHIShaderInterface.h"   // Zenith_RHIDescriptorHandle (Zenith_GlyphPlacement::m_xTexture)
#include "ZenithUI/Zenith_UITypes.h"

/// @contract value Zenith_FontMetrics
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: None.
/// Units: Em fractions (font units divided by unitsPerEm).
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_FontMetrics
{
	float m_fAscent = 0.8f;                // OS/2 sTypoAscender when USE_TYPO_METRICS is set, else hhea ascender
	float m_fDescent = 0.2f;               // positive
	float m_fLineGap = 0.0f;
	float m_fCapHeight = 0.7f;
	float m_fXHeight = 0.5f;
	float m_fUnderlinePosition = -0.1f;
	float m_fUnderlineThickness = 0.05f;
};

/// @contract value Zenith_FontFaceInfo
/// Ownership: Plain value read from the cooked face table.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: None.
/// Units: m_uWeight 100–900; m_fEmPixels and m_fRangePixels in atlas pixels.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_FontFaceInfo
{
	uint16 m_uWeight = 400;
	bool m_bItalic = false;
	uint16 m_uUnitsPerEm = 1000;
	uint32 m_uGlyphCount = 0;
	Zenith_FontMetrics m_xMetrics;
	float m_fEmPixels = 40.0f;
	float m_fRangePixels = 6.0f;
	uint32 m_uBakedGlyphCount = 0;
};

/// @contract type Zenith_FontAsset
/// Ownership: The runtime object of ASSET_KIND_FONT (row 12 of Zenith2/Docs/Design/ASSET.md "Asset kinds"; not GPU
///   resident: the kind loader creates no GPU resource), created by UI's font loader; owns the faces' font programs, glyph
///   tables and baked atlas page pixels in the general heap until DropBakedPixels.
/// Thread affinity: IMMUTABLE once ACTIVE, except DropBakedPixels (the UI memory consumer, main thread); members
///   non-const: its loader writes them after constructing the object.
/// Invalidation: Spans it returns are valid while a handle is held; GetBakedPagePixels returns an empty span after
///   DropBakedPixels.
/// Units: As Zenith_FontFaceInfo.
/// Errors: None after activation.
/// Version: since Z2 Stage 3. Cooked format version 1 (Zenith2/Docs/Design/UI/Documents.md "Cooked font format").
/// @endcontract
class Zenith_FontAsset
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_FONT;
	Zenith_StringView GetFamilyName() const;
	uint32 GetFaceCount() const;
	const Zenith_FontFaceInfo& GetFace(uint32 uFace) const;
	uint32 SelectFace(uint16 uWeight, bool bItalic) const;      // the CSS font-matching rule, Text.md
	Zenith_ByteSpan GetFontProgram(uint32 uFace) const;
	bool HasCodePoint(uint32 uFace, uint32 uCodePoint) const;    // O(log ranges) over the coverage section
	uint32 GetBakedPageCount() const;
	Zenith_ByteSpan GetBakedPagePixels(uint32 uPage) const;
	void DropBakedPixels();
private:
	friend class Zenith_UIFontLoader;
	void* m_pData = nullptr;
};

/// @contract type Zenith_FontStack
/// Ownership: Built and owned by the font registry inside Zenith_UISystem: an ordered fallback list of (font asset, face
///   selection) entries for one theme font token in the active locale (Zenith2/Docs/Design/UI/Text.md "Font fallback").
/// Thread affinity: IMMUTABLE between frames; members non-const: the font registry rebuilds it in place; rebuilt only in
///   the prologue on a locale or font change.
/// Invalidation: Pointers to a stack are invalid after the prologue that rebuilt it.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_FontStack
{
public:
	uint32 GetEntryCount() const;
	const Zenith_FontAsset* GetFont(uint32 uEntry) const;    // null while the asset is not ACTIVE
	Zenith_StringView GetToken() const;
private:
	friend class Zenith_FontRegistry;
	void* m_pData = nullptr;
};

/// @contract value Zenith_GlyphKey
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: a value type its holders assign and reassign.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_GlyphKey
{
	uint32 m_uFontIndex = 0;              // the font registry's index of the font asset
	uint16 m_uFace = 0;
	uint16 m_uReserved = 0;
	uint32 m_uGlyph = 0;                  // glyph ID within the face
	friend constexpr bool operator==(Zenith_GlyphKey, Zenith_GlyphKey) = default;
};

/// @contract value Zenith_GlyphPlacement
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: m_xTexture is valid for the frame that obtained it.
/// Units: m_afPlane in em relative to the pen position and baseline (+Y down); m_auUV UNORM16 of the page.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_GlyphPlacement
{
	float m_afPlane[4] = {};               // left, top, right, bottom
	uint16 m_auUV[4] = {};
	Zenith_RHIDescriptorHandle m_xTexture; // the page's SRV; the null handle when not resident
	float m_fRangeEm = 0.0f;               // the distance range in em, for the screen-pixel range
	bool m_bEmpty = false;                 // whitespace and zero-area glyphs: no quad
};

/// @contract enum Zenith_FontError
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: FONT_ERROR_CORRUPT (a section fails its checks), _UNSUPPORTED_PROGRAM (the font program fails to open in the
///   shaping backend), _TOO_MANY_GLYPHS, _PAGE_LIMIT, _BAD_FACE (a face index outside the table), _OUT_OF_MEMORY.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_FontError : uint8
{
	FONT_ERROR_CORRUPT = 1, FONT_ERROR_UNSUPPORTED_PROGRAM, FONT_ERROR_TOO_MANY_GLYPHS, FONT_ERROR_PAGE_LIMIT,
	FONT_ERROR_BAD_FACE, FONT_ERROR_OUT_OF_MEMORY,
};
constexpr const char* Zenith_ToString(Zenith_FontError eError);

/// @contract type Zenith_GlyphCache
/// Ownership: Owned by Zenith_UISystem; owns the glyph table (key → placement), the dynamic atlas pages' CPU pixels and the
///   per-page GPU textures through Flux_UIRenderer (Zenith2/Docs/Design/UI/Text.md "Glyph cache and atlases").
/// Thread affinity: ANY_THREAD_EXCEPT(ProcessRequests, ReleaseUnused). Lookup and Request are called from UI extractors
///   of any world: the table is a fixed-capacity open-addressing map published once per frame (read-only during the
///   frame), and requests are appended to per-context request lists that ProcessRequests merges on the main thread.
/// Invalidation: Placements from Lookup are valid for the frame.
/// Units: As Zenith_GlyphPlacement.
/// Errors: Request never fails: a full request list drops the request (counted ui.glyphs.requests_dropped) and the glyph is
///   requested again by the next frame that draws it.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_GlyphCache
{
public:
	bool Lookup(Zenith_GlyphKey xKey, Zenith_GlyphPlacement& xOut) const;
	void Request(uint16 uContextTag, Zenith_GlyphKey xKey);
	void ProcessRequests(uint32 uBudgetGlyphs);          // main thread, prologue: MSDF generation and packing
	void ReleaseUnused(uint32 uAgeFrames);               // memory pressure
	uint32 GetResidentPageCount() const;
	uint32 GetDynamicGlyphCount() const;
};
```

`Zenith_RHIDescriptorHandle` is RHI's shader-interface value type (`ZenithRHI/Zenith_RHIShaderInterface.h`). `Zenith_UIFont.h` includes that header itself, so the header is self-contained (`HeaderSelfContained`), and `ZenithUI` declares `ZenithRHI` in its `DEPENDS` (`Zenith2/Docs/Design/UI.md → "Libraries UI code depends on"`): [Z2-N-ARCH-167] requires the edge for a referenced symbol even though ZenithFlux, a declared dependency, reaches ZenithRHI already, and `Libgraph` fails a used-but-undeclared edge.

## Zenith_UIFontBake header

```cpp
// Zenith2/Engine/ZenithUI/Zenith_UIFontBake.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Array.h"
#include "ZenithCore/Zenith_Arena.h"           // Zenith_Arena (the bake scratch of Zenith_BakeFontFamily, Zenith_GenerateGlyphMSDF)
#include "ZenithUI/Zenith_UIFont.h"

/// @contract value Zenith_FontBakeFace
/// Ownership: Plain value; m_xProgram views the caller's font file bytes for the call.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: As Zenith_FontFaceInfo.
/// Errors: Validated by Zenith_BakeFontFamily.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_FontBakeFace
{
	Zenith_ByteSpan m_xProgram;            // a TrueType or CFF OpenType font (sfnt); collections select m_uCollectionIndex
	uint32 m_uCollectionIndex = 0;
	uint16 m_uWeight = 400;
	bool m_bItalic = false;
};

/// @contract value Zenith_FontBakeDesc
/// Ownership: Plain value; spans borrowed for the call.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: Pixels for m_uPageSize, m_fEmPixels, m_fRangePixels, m_uPadding.
/// Errors: Zenith_BakeFontFamily returns FONT_ERROR_* for an invalid value (page size not 512, 1024 or 2048; em pixels outside
///   16 … 128; range outside 2 … 16; more than 8 faces).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_FontBakeDesc
{
	Zenith_StringView m_strFamilyName;
	Zenith_Span<const Zenith_FontBakeFace> m_axFaces;
	Zenith_Span<const uint32> m_auCodePoints;   // the baked set, sorted ascending, unique
	uint16 m_uPageSize = 1024;
	float m_fEmPixels = 40.0f;
	float m_fRangePixels = 6.0f;
	uint16 m_uPadding = 1;
	uint32 m_uMaxPages = 16;
};

/// @contract value Zenith_FontBakeOutput
/// Ownership: Plain value; the arrays are the caller's.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_FontBakeOutput
{
	Zenith_Array<uint8>* m_pxSections = nullptr;   // receives the section payloads of the cooked font format, in table order
	uint32 m_uPagesWritten = 0;
	uint32 m_uGlyphsBaked = 0;
	uint32 m_uCodePointsMissing = 0;               // requested code points no face covers (reported by the cook)
};

/// @contract functions Zenith_FontBakeFunctions
/// Ownership: Write into the caller's arrays; allocate scratch from the caller's arena.
/// Thread affinity: ANY_THREAD; concurrent calls are independent.
/// Invalidation: None.
/// Units: Pixels.
/// Errors: Zenith_FontError; untrusted font bytes never assert ([Z2-N-ARCH-951]).
/// Version: since Z2 Stage 3. Deterministic within one build: the same inputs and pinned backend versions give byte-identical
///   output from the same executable, and Zenith_GenerateGlyphMSDF is the code path the runtime's dynamic glyphs use, so within
///   one build a baked glyph and the same glyph generated dynamically are bit-identical (Test_FontBake_BakedEqualsDynamic bakes
///   and generates in one test executable). Across builds (the cook on the Windows host, dynamic generation on the S24) they are
///   visually equivalent, not bit-identical: a quantised channel may differ by one step (Zenith2/Docs/Design/UI/Text.md
///   "MSDF generation"); no rule depends on cross-build identity.
/// @endcontract
Zenith_Result<void, Zenith_FontError> Zenith_BakeFontFamily(const Zenith_FontBakeDesc& xDesc,
	Zenith_FontBakeOutput& xOut, Zenith_Arena& xScratch);
Zenith_Result<void, Zenith_FontError> Zenith_GenerateGlyphMSDF(Zenith_ByteSpan xProgram, uint32 uCollectionIndex,
	uint32 uGlyph, float fEmPixels, float fRangePixels, uint16 uPadding, Zenith_MutableByteSpan xOutRGBA, uint16& uWidthOut,
	uint16& uHeightOut, float (&afPlaneOut)[4], Zenith_Arena& xScratch);
```

## Zenith_UILocalization header

```cpp
// Zenith2/Engine/ZenithUI/Zenith_UILocalization.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_Arena.h"
#include "ZenithCore/Zenith_Array.h"           // Zenith_Array<uint8> (Zenith_CompileMessage, Zenith_CompilePluralRule)
#include "ZenithAssets/Zenith_AssetTypes.h"

/// @contract value Zenith_LocaleID
/// Ownership: Plain value: a canonical BCP 47 tag of at most 31 bytes, stored inline.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: None.
/// Units: None.
/// Errors: Zenith_ParseLocale returns LOCALIZATION_ERROR_BAD_LOCALE for a tag outside the grammar of
///   Zenith2/Docs/Design/UI/Localization.md "Locales and negotiation".
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_LocaleID
{
	char m_acTag[32] = {};                 // "en-GB", "zh-Hans-CN", "qps-ploc"
	uint8 m_uLength = 0;
	Zenith_StringView GetTag() const;
};

/// @contract enum Zenith_PluralCategory
/// Ownership: Plain enumeration (CLDR plural categories).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4; values stable (compiled messages store them).
/// @endcontract
enum Zenith_PluralCategory : uint8
{
	PLURAL_CATEGORY_ZERO = 0, PLURAL_CATEGORY_ONE, PLURAL_CATEGORY_TWO, PLURAL_CATEGORY_FEW, PLURAL_CATEGORY_MANY,
	PLURAL_CATEGORY_OTHER,
};

/// @contract value Zenith_MessageArg
/// Ownership: Plain value; m_strText views caller storage for the Format call.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: None.
/// Errors: None; an argument of the wrong kind formats as its text form and counts ui.loc.arg_kind_mismatch.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_MessageArg
{
	uint64 m_ulNameHash = 0;              // Zenith_Hash64 of the argument name
	uint8 m_uKind = 0;                    // 1 integer, 2 number, 3 text, 4 string key (looked up), 5 select keyword
	sint64 m_iValue = 0;
	double m_dValue = 0.0;
	Zenith_StringView m_strText;
};

/// @contract type Zenith_LocalizationTableAsset
/// Ownership: The runtime object of ASSET_KIND_LOCALIZATION_TABLE (row 20 of Zenith2/Docs/Design/ASSET.md "Asset kinds"):
///   one locale's compiled strings, plural rules and number symbols; created by UI's loader; immutable.
/// Thread affinity: IMMUTABLE once ACTIVE; members non-const: its loader writes them after constructing the object.
/// Invalidation: Views are valid while a handle is held.
/// Units: None.
/// Errors: Load failures are the loader's.
/// Version: since Z2 Stage 3. Cooked format version 1 (Zenith2/Docs/Design/UI/Documents.md "Cooked localization table format").
/// @endcontract
class Zenith_LocalizationTableAsset
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_LOCALIZATION_TABLE;
	const Zenith_LocaleID& GetLocale() const;
	uint32 GetMessageCount() const;
	bool HasMessage(uint64 ulKeyHash) const;       // O(log n) over the sorted key index
private:
	friend class Zenith_UILocalizationLoader;
	void* m_pData = nullptr;
};

/// @contract enum Zenith_LocalizationErrorCode
/// Ownership: Plain enumeration, the code of Zenith_LocalizationError.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Zenith2/Docs/Design/UI/Localization.md "Localization validation" lists when each is returned.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_LocalizationErrorCode : uint8
{
	LOCALIZATION_ERROR_BAD_LOCALE = 1, LOCALIZATION_ERROR_UNKNOWN_KEY, LOCALIZATION_ERROR_SYNTAX,
	LOCALIZATION_ERROR_UNKNOWN_ARGUMENT, LOCALIZATION_ERROR_ARGUMENT_MISMATCH, LOCALIZATION_ERROR_PLURAL_CATEGORY,
	LOCALIZATION_ERROR_MISSING_OTHER, LOCALIZATION_ERROR_NESTING, LOCALIZATION_ERROR_TAG, LOCALIZATION_ERROR_TOO_LONG,
	LOCALIZATION_ERROR_BUFFER, LOCALIZATION_ERROR_CORRUPT, LOCALIZATION_ERROR_RULE_SYNTAX,
};

/// @contract value Zenith_LocalizationError
/// Ownership: Plain value, 8 bytes, text-free.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: m_uOffset a byte offset into the message source; m_uLine its 1-based line in the authoring table (0 at run time).
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_LocalizationError
{
	const Zenith_LocalizationErrorCode m_eCode = LOCALIZATION_ERROR_SYNTAX;
	const uint8 m_uReserved = 0;
	const uint16 m_uLine = 0;
	const uint32 m_uOffset = 0;
};

/// @contract type Zenith_Localization
/// Ownership: Owned by Zenith_UISystem; holds the active locale, the negotiated fallback chain, the counted handles of the
///   active and source-locale tables, and the pseudo-localization mode.
/// Thread affinity: ANY_THREAD_EXCEPT(SetLocale, CommitPendingLocale). Lookup and Format read the tables published at the
///   last CommitPendingLocale (prologue), which never changes during a frame.
/// Invalidation: Formatted text lives in the caller's arena or buffer.
/// Units: Bytes.
/// Errors: Format returns LOCALIZATION_ERROR_BUFFER when the output does not fit (the output then holds a truncated prefix at
///   a code-point boundary); an unknown key formats the missing-key marker (development: ⟦key⟧; Shipping: ⟦#hash⟧,
///   Localization.md "Message formatting"), counting ui.loc.missing, and returns LOCALIZATION_ERROR_UNKNOWN_KEY.
/// Version: since Z2 Stage 3 (lookup); message formatting and plural selection since Z2 Stage 4.
/// @endcontract
class Zenith_Localization
{
public:
	const Zenith_LocaleID& GetActiveLocale() const;
	bool IsRightToLeft() const;                         // the active locale's script direction
	Zenith_Result<void, Zenith_LocalizationError> SetLocale(const Zenith_LocaleID& xRequested);
	bool CommitPendingLocale();                         // prologue: true when a switch took effect
	Zenith_Result<Zenith_StringView, Zenith_LocalizationError> Format(uint64 ulKeyHash,
		Zenith_Span<const Zenith_MessageArg> axArgs, Zenith_Arena& xArena) const;
	Zenith_Result<uint32, Zenith_LocalizationError> FormatTo(uint64 ulKeyHash,
		Zenith_Span<const Zenith_MessageArg> axArgs, Zenith_Span<char> xOut) const;
	Zenith_PluralCategory SelectPlural(double dValue, uint32 uFractionDigits, bool bOrdinal) const;
	uint32 FormatNumber(double dValue, uint8 uStyle, uint8 uMinFractionDigits, uint8 uMaxFractionDigits,
		Zenith_Span<char> xOut) const;                                // uStyle 0 decimal, 1 integer, 2 percent
};

/// @contract functions Zenith_LocalizationFunctions
/// Ownership: Write into caller buffers.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: Zenith_LocalizationError with the offset of the first failure; the source text is untrusted and never asserts.
/// Version: since Z2 Stage 3 (ParseLocale), Z2 Stage 4 (the rest). Compiled bytes are deterministic.
/// @endcontract
Zenith_Result<Zenith_LocaleID, Zenith_LocalizationError> Zenith_ParseLocale(Zenith_StringView strTag);
Zenith_Result<void, Zenith_LocalizationError> Zenith_CompileMessage(Zenith_StringView strSource,
	Zenith_Span<const uint64> aulDeclaredArgs, uint8 uPluralCategoryMask, Zenith_Array<uint8>& xOut);
Zenith_Result<void, Zenith_LocalizationError> Zenith_CompilePluralRule(Zenith_StringView strRule,
	Zenith_Array<uint8>& xOut);
bool Zenith_EvaluatePluralRule(Zenith_ByteSpan xProgram, double dValue, uint32 uFractionDigits);
```

## Zenith_UIAccessibility header

```cpp
// Zenith2/Engine/ZenithUI/Zenith_UIAccessibility.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithUI/Zenith_UITypes.h"

/// @contract enum Zenith_UIColourVision
/// Ownership: Plain enumeration; the value of ui.access.colourVision.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4; values stable.
/// @endcontract
enum Zenith_UIColourVision : uint8
{
	UI_COLOUR_VISION_NONE = 0, UI_COLOUR_VISION_PROTANOPIA, UI_COLOUR_VISION_DEUTERANOPIA, UI_COLOUR_VISION_TRITANOPIA,
	UI_COLOUR_VISION_ACHROMATOPSIA,
};

/// @contract value Zenith_UIAccessibilitySettings
/// Ownership: Plain value resolved from the ui.access.*, ui.captions.* and ui.scale keys and the platform's system
///   preferences (Zenith2/Docs/Design/UI/Accessibility.md "Accessibility settings").
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: None.
/// Units: Scales are multipliers; m_fNarrationRate a speech-rate multiplier; opacity 0–1.
/// Errors: None.
/// Version: since Z2 Stage 3 (UI scale and text scale); the rest since Z2 Stage 4. Presentation-only: no simulation code
///   reads it (Z2-D-019).
/// @endcontract
struct Zenith_UIAccessibilitySettings
{
	float m_fUIScale = 1.0f;
	float m_fTextScale = 1.0f;
	bool m_bReducedMotion = false;
	bool m_bHighContrast = false;
	Zenith_UIColourVision m_eColourVision = UI_COLOUR_VISION_NONE;
	bool m_bNarration = false;             // built-in text-to-speech narration
	bool m_bScreenReaderBridge = true;     // publish the semantic tree to an active OS screen reader
	float m_fNarrationRate = 1.0f;
	float m_fFocusRingScale = 1.0f;
	bool m_bSubtitles = true;
	bool m_bSoundCaptions = false;
	float m_fCaptionScale = 1.0f;
	float m_fCaptionBackgroundOpacity = 0.75f;
	bool m_bSpeakerNames = true;
	bool m_bSpeakerColours = true;
};

/// @contract enum Zenith_UIRole
/// Ownership: Plain enumeration; the accessibility role of a semantic node.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4; values stable (cooked documents store them).
/// @endcontract
enum Zenith_UIRole : uint8
{
	UI_ROLE_NONE = 0, UI_ROLE_GROUP, UI_ROLE_TEXT, UI_ROLE_HEADING, UI_ROLE_IMAGE, UI_ROLE_BUTTON, UI_ROLE_CHECKBOX,
	UI_ROLE_SLIDER, UI_ROLE_TEXT_FIELD, UI_ROLE_LIST, UI_ROLE_LIST_ITEM, UI_ROLE_TAB_LIST, UI_ROLE_TAB, UI_ROLE_DIALOG,
	UI_ROLE_PROGRESS, UI_ROLE_COMBO_BOX, UI_ROLE_ALERT, UI_ROLE_STATUS, UI_ROLE_SCROLL_AREA,
};

/// @contract enum Zenith_UIA11yStateFlags
/// Ownership: Plain enumeration of bits.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_UIA11yStateFlags : uint16
{
	UI_A11Y_FOCUSED = 1u << 0, UI_A11Y_FOCUSABLE = 1u << 1, UI_A11Y_DISABLED = 1u << 2, UI_A11Y_CHECKED = 1u << 3,
	UI_A11Y_CHECKABLE = 1u << 4, UI_A11Y_SELECTED = 1u << 5, UI_A11Y_EXPANDED = 1u << 6, UI_A11Y_COLLAPSED = 1u << 7,
	UI_A11Y_READ_ONLY = 1u << 8, UI_A11Y_PASSWORD = 1u << 9, UI_A11Y_LIVE_POLITE = 1u << 10, UI_A11Y_LIVE_ASSERTIVE = 1u << 11,
	UI_A11Y_HIDDEN = 1u << 12,
};

/// @contract value Zenith_UIA11yNode
/// Ownership: Plain value in the tree's arena; strings view that arena.
/// Thread affinity: IMMUTABLE; members non-const: an element its producer writes in place in an arena or Zenith_Array.
/// Invalidation: With the arena.
/// Units: m_xBoundsPixels in surface pixels (world-space panels: projected bounds through the main camera).
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_UIA11yNode
{
	uint64 m_ulID = 0;                    // stable across frames: Hash64(context tag, screen owner ID, element uid)
	uint32 m_uParent = 0xFFFFFFFFu;       // index into the node array; the root's is 0xFFFFFFFF
	Zenith_UIRole m_eRole = UI_ROLE_NONE;
	uint16 m_uState = 0;                  // Zenith_UIA11yStateFlags
	Zenith_StringView m_strName;
	Zenith_StringView m_strValue;          // slider value text, text field content (masked for passwords), progress percent
	Zenith_StringView m_strDescription;    // the hint
	Zenith_UIRect m_xBoundsPixels;
	uint32 m_uPositionInSet = 0;          // 1-based among siblings of the same role, 0 when not applicable
	uint32 m_uSetSize = 0;
	double m_dRangeMin = 0.0, m_dRangeMax = 0.0, m_dRangeValue = 0.0;
	uint8 m_uActions = 0;                 // bit 0 activate, 1 increment, 2 decrement, 3 scroll into view, 4 set focus
	uint8 m_uHeadingLevel = 0;            // 1–6 for UI_ROLE_HEADING (accessibility.heading), 0 otherwise; copied to PLAT's
	                                       //   Zenith_AccessibilityNode::m_uHeadingLevel (validation rule V9)
};

/// @contract value Zenith_UIA11yTree
/// Ownership: Plain value; spans in the caller's arena.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: With the arena.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_UIA11yTree
{
	Zenith_Span<const Zenith_UIA11yNode> m_axNodes;   // depth-first, parents before children
	uint32 m_uFocused = 0xFFFFFFFFu;
	uint64 m_ulRevision = 0;                         // the context's revision: increments whenever any node changed since the
	                                                  //   previous build; never passed to PLAT, which receives the bridge's
	                                                  //   module-wide revision (Accessibility.md "Platform accessibility bridge")
};

/// @contract enum Zenith_UINarrationPriority
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_UINarrationPriority : uint8 { UI_NARRATION_POLITE = 0, UI_NARRATION_ASSERTIVE = 1 };

/// @contract value Zenith_UINarrationRequest
/// Ownership: Plain value in the drain arena.
/// Thread affinity: IMMUTABLE; members non-const: an element its producer writes in place in an arena or Zenith_Array.
/// Invalidation: With the arena.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_UINarrationRequest
{
	Zenith_StringView m_strText;
	Zenith_UINarrationPriority m_ePriority = UI_NARRATION_POLITE;
	bool m_bInterrupt = false;             // focus changes interrupt the previous focus utterance
	uint64 m_ulNodeID = 0;                // the node the utterance describes, 0 for announcements
};
```

## Zenith_UIScriptApi header

```cpp
// Zenith2/Engine/ZenithUI/Zenith_UIScriptApi.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_StringID.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithReflect/Zenith_ScriptBinding.h"
#include "ZenithUI/Zenith_UITypes.h"

/// @contract type Zenith_UIScriptApi
/// Ownership: Static functions only; the binding thunks AddFunction generates call them
///   (Zenith2/Docs/Scripting/Bindings.md "Binding registration and the thunk ABI").
/// Thread affinity: ANY_THREAD. Called on the thread of the invocation, through its Zenith_ScriptInvocationContext.
/// Invalidation: None.
/// Units: Seconds for caption durations (converted to ticks with the invocation's tick duration).
/// Errors: Zenith_Result<…, Zenith_ScriptThunkError>: script misuse is SCRIPT_THUNK_ERROR_MISUSE with a code of UI's block
///   ZS2100–ZS2199, each code, trigger and exact text as listed in Zenith2/Docs/Design/UI.md "Luau errors and asserts"
///   (ZS2101 malformed key, ZS2102 caption kind, ZS2103 announcement priority, ZS2104 caption duration, ZS2105 caption
///   priority, ZS2106 list item index); a full per-entity side-effect cap is the SCRIPT error it returns.
///   Misuse never asserts ([Z2-N-CONTRACT-473]).
/// Reentrancy: NOT_APPLICABLE. No function invokes a callback.
/// Version: since Z2 Stage 3 (Loc, Focus, ScrollTo, PlayAnimation); Z2 Stage 4 (Announce, ShowCaption, ClearCaptions).
/// @endcontract
class Zenith_UIScriptApi
{
public:
	static Zenith_Result<Zenith_StringID, Zenith_ScriptThunkError> Loc(Zenith_ScriptInvocationContext& xContext,
		Zenith_StringID xKey);
	static Zenith_Result<void, Zenith_ScriptThunkError> Focus(Zenith_ScriptInvocationContext& xContext,
		Zenith_EntityHandle xScreen, Zenith_StringID xElement);
	static Zenith_Result<void, Zenith_ScriptThunkError> ScrollTo(Zenith_ScriptInvocationContext& xContext,
		Zenith_EntityHandle xScreen, Zenith_StringID xElement, double dItem);
	static Zenith_Result<void, Zenith_ScriptThunkError> PlayAnimation(Zenith_ScriptInvocationContext& xContext,
		Zenith_EntityHandle xScreen, Zenith_StringID xElement, Zenith_StringID xAnimation);
	static Zenith_Result<void, Zenith_ScriptThunkError> Announce(Zenith_ScriptInvocationContext& xContext,
		Zenith_StringID xKey, Zenith_StringID xPriority);
	static Zenith_Result<void, Zenith_ScriptThunkError> ShowCaption(Zenith_ScriptInvocationContext& xContext,
		Zenith_StringID xKey, Zenith_StringID xSpeakerKey, Zenith_StringID xKind, double dSeconds, double dPriority);
	static Zenith_Result<void, Zenith_ScriptThunkError> ClearCaptions(Zenith_ScriptInvocationContext& xContext,
		Zenith_StringID xKind);
};

/// @contract functions Zenith_UIScriptRegistration
/// Ownership: Adds functions to the caller's binding table.
/// Thread affinity: MAIN_THREAD, module start step 4, before the table is sealed.
/// Invalidation: None.
/// Units: None.
/// Errors: None at the call; the table's Seal reports a collision ([Z2-N-SCRIPT-567]).
/// Version: since Z2 Stage 3.
/// @endcontract
void Zenith_RegisterUIScriptBindings(Zenith_ScriptBindingTable& xTable);
```

| Luau function | Contexts | Effect | Command kind or result |
|---|---|---|---|
| `UI.Loc(key)` | system, behaviour, load | `PURE` | the key as an interned string; validates the key grammar; the marker the extractor finds (`Zenith2/Docs/Design/UI/Localization.md → "Extraction sources"`) |
| `UI.Focus(ctx, screen, element)` | behaviour, system | `BUFFERED` (`uSIDE_EFFECT_MASK_FIELD_COMMAND`) | `UI.Focus` |
| `UI.ScrollTo(ctx, screen, element, item)` | behaviour, system | `BUFFERED` | `UI.ScrollTo` |
| `UI.PlayAnimation(ctx, screen, element, animation)` | behaviour, system | `BUFFERED` | `UI.PlayAnimation` |
| `UI.Announce(ctx, key, priority)` | behaviour, system | `BUFFERED` | `UI.Announce`; `priority` is `"polite"` (default) or `"assertive"` |
| `UI.ShowCaption(ctx, key, speaker, kind, seconds, priority)` | behaviour, system | `BUFFERED` | `UI.ShowCaption`; `kind` is `"subtitle"` or `"sound"`, `speaker` may be `""` |
| `UI.ClearCaptions(ctx, kind)` | behaviour, system | `BUFFERED` | `UI.ClearCaptions`; `kind` `"subtitle"`, `"sound"` or `"all"` |

No function returns presentation state: scripts read nothing from the UI (focus, hover, layout, text, accessibility settings), because those depend on presentation inputs that the replay log does not record (Z2-D-019); what the player did reaches scripts only as the six event types.

## Zenith_UIEditorModel header

```cpp
// Zenith2/Engine/ZenithUI/Zenith_UIEditorModel.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_JsonDocument.h"
#include "ZenithUI/Zenith_UIDocument.h"
#include "ZenithUI/Zenith_UIStyle.h"
#include "ZenithUI/Zenith_UIContext.h"

class Zenith_UISystem;

/// @contract enum Zenith_UIEditOperationKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_UIEditOperationKind : uint8
{
	UI_EDIT_INSERT_ELEMENT = 1,     // insert a subtree (JSON) under a parent at an index
	UI_EDIT_REMOVE_ELEMENT,         // remove a subtree; the inverse inserts it back with the same uids
	UI_EDIT_MOVE_ELEMENT,           // reparent and/or reorder
	UI_EDIT_SET_MEMBER,             // set one member (JSON pointer relative to the element) to a JSON value
	UI_EDIT_CLEAR_MEMBER,
	UI_EDIT_SET_OVERRIDE,           // set an instance override keyed by the target element's uid
	UI_EDIT_CLEAR_OVERRIDE,
};

/// @contract value Zenith_UIEditOperation
/// Ownership: Plain value; m_pxValue borrows a JSON node of the caller's document for the Apply call.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: m_uIndex a child index.
/// Errors: Validated by Apply.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_UIEditOperation
{
	Zenith_UIEditOperationKind m_eKind = UI_EDIT_SET_MEMBER;
	Zenith_PersistentID m_xTarget;         // the element uid acted on (the parent for INSERT)
	Zenith_PersistentID m_xNewParent;      // MOVE
	uint32 m_uIndex = 0;                  // INSERT and MOVE: the child index
	Zenith_StringView m_strPointer;        // SET_MEMBER, CLEAR_MEMBER, the overrides' member pointer
	const Zenith_JsonDocument* m_pxValueDocument = nullptr;
	Zenith_JsonNode m_xValue;
};

/// @contract value Zenith_UIEditResult
/// Ownership: Plain value; m_xInverse's JSON lives in the model's undo arena until the model is destroyed or Compact runs.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: m_xInverse until Compact.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_UIEditResult
{
	Zenith_UIEditOperation m_xInverse;     // applying it restores the exact prior document bytes
	bool m_bLayoutAffected = false;
	bool m_bStructureChanged = false;
};

/// @contract value Zenith_UIPropertyDesc
/// Ownership: constexpr data of ZenithUI's widget catalogue.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: As the property's unit string.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_UIPropertyDesc
{
	const char* const m_szPointer = nullptr;     // "style/padding", "layout/gap", "text/key"
	const char* const m_szLabelKey = nullptr;    // engine localization key for the editor label
	const uint8 m_uValueType = 0;               // 1 number, 2 integer, 3 bool, 4 colour, 5 enum, 6 string, 7 asset path, 8 edges, 9 vector2
	const char* const* const m_aszEnumValues = nullptr;
	const uint32 m_uEnumCount = 0;
	const float m_fMin = 0.0f, m_fMax = 0.0f;
	const bool m_bBindable = false;              // may take an "@binding" value
	const bool m_bAffectsLayout = false;
};

/// @contract value Zenith_UIWidgetDesc
/// Ownership: constexpr data of the widget catalogue.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_UIWidgetDesc
{
	const Zenith_UIElementType m_eType = UI_ELEMENT_PANEL;
	const char* const m_szTypeName = nullptr;    // the authoring "type" string
	const char* const m_szDefaultJson = nullptr; // the subtree a palette drop inserts (uids generated at insertion)
	const Zenith_Span<const Zenith_UIPropertyDesc> m_axProperties = {};
	const bool m_bContainer = false;
};

/// @contract type Zenith_UIEditorModel
/// Ownership: Created by EDIT's UI editor as a Zenith_Owned per open UI document; owns the Zenith_UIDocumentModel, an undo arena
///   for inverses and a preview Zenith_UIContext; Recompile compiles the model into its section bytes and reopens the preview
///   screen from them with Zenith_UIContext::OpenScreenFromBytes (no asset is created), keeping focus by element uid.
/// Thread affinity: OWNER_THREAD (the editor's main thread).
/// Invalidation: Element indices of the inner model are invalid after a structural Apply; uids are not.
/// Units: Canvas units for HitTestElement.
/// Errors: Apply returns UI_ERROR_EDIT_REJECTED with m_uDetail the failing validation rule and leaves the document unchanged;
///   Recompile returns the compile errors; nothing asserts on authored data.
/// Version: since Z2 Stage 4. Tool-facing source API; EDIT's command system wraps each Apply and its inverse in an undo
///   transaction (Zenith2/Docs/Design/EDIT.md, pending).
/// @endcontract
class Zenith_UIEditorModel
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_UIEditorModel>, Zenith_UIError> Create(Zenith_UISystem& xSystem,
		Zenith_Heap& xHeap);
	Zenith_Result<void, Zenith_UIError> Load(const Zenith_JsonDocument& xDocument, Zenith_StringView strPath,
		const Zenith_UIValidationContext& xContext);
	Zenith_Result<Zenith_UIEditResult, Zenith_UIError> Apply(const Zenith_UIEditOperation& xOperation);
	Zenith_Result<void, Zenith_UIError> Recompile(const Zenith_UIValidationContext& xContext);
	Zenith_UIContext& GetPreview();                 // laid out at the preview surface the editor sets
	Zenith_PersistentID HitTestElement(Zenith_Vector2 xPixel) const;   // the deepest element uid, null if none
	const Zenith_UIDocumentModel& GetModel() const;
	Zenith_Result<void, Zenith_UIError> Save(Zenith_JsonDocument& xOut) const;   // canonical member order
	void Compact();                                               // drops inverses older than the editor's undo depth
	static Zenith_Span<const Zenith_UIWidgetDesc> GetWidgetCatalogue();
};
```

## Flux_UI header

The GPU side of UI lives in ZenithFlux (UI-owned files, cross-area row UI7), because the render graph, the persistent resources and the pass contexts are ZenithFlux's and ZenithFlux sees no module ([Z2-N-ARCH-076]). `Flux_RenderUIList` is the element type of `Flux_RenderWorldSection::m_axUILists` and of `Flux_RenderWorldWriter::AddUIList` (`Zenith2/Docs/Rendering.md → "Purpose and scope"`, heading "The render world hand-off"); `Flux_RenderWorld.h` includes this header for it.

```cpp
// Zenith2/Engine/ZenithFlux/Flux_UI.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Matrix.h"
#include "ZenithRHI/Zenith_RHIShaderInterface.h"
#include "ZenithFlux/Flux_RenderGraph.h"
#include "ZenithFlux/Flux_ViewContext.h"
#include "ZenithFlux/Flux_PersistentResources.h"

struct Flux_RenderWorld;      // ZenithFlux/Flux_RenderWorld.h, which includes this header
struct Flux_ViewTargets;      // ZenithFlux/Flux_ViewExtension.h (RPROD)
class Flux_UploadRing;        // ZenithFlux/Internal/Upload/Flux_UploadRing.h (RHI): referenced, never by value
struct Flux_UIFrame;          // ZenithFlux/Internal/UI/Flux_UIFrame.h: consumed only inside ZenithFlux

/// @contract enum Flux_UIQuadMode
/// Ownership: Plain enumeration; the low 8 bits of Flux_UIQuad::m_uModeFlags, read by UIDraw.slang and UIWorldSpace.slang.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; the shader draws an unknown mode as nothing.
/// Version: since Z2 Stage 3; values shared with the Slang constants of Zenith2/Shaders/Common/UIShapes.slang.
/// @endcontract
enum Flux_UIQuadMode : uint8
{
	FLUX_UI_QUAD_SOLID = 0,          // rounded rectangle with optional border, solid fill
	FLUX_UI_QUAD_GRADIENT = 1,       // rounded rectangle with a two-stop linear gradient along local x (flag bit 8: along y)
	FLUX_UI_QUAD_IMAGE = 2,          // textured quad (UVs into the batch texture), tinted, rounded-corner masked
	FLUX_UI_QUAD_GLYPH = 3,          // MTSDF glyph with optional outline (alpha channel) — requires bPERM_SDF_TEXT
	FLUX_UI_QUAD_SHADOW = 4,         // blurred rounded-rectangle shadow (analytic Gaussian approximation)
	FLUX_UI_QUAD_FOCUS_RING = 5,     // a ring of m_hBorderWidth outside the rounded rectangle
};

/// @contract value Flux_UIQuad
/// Ownership: Plain value, 64 bytes, scalar layout identical to the Slang struct Flux_UIQuadGPU ([FluxShared], Zenith2/Docs/
///   Rendering/ShaderSystem.md "Shared GPU structs"); written into the render world's arena by the UI extractor, copied into
///   the frame's upload ring by the UI renderer.
/// Thread affinity: IMMUTABLE; members non-const: an element its producer writes in place in an arena or Zenith_Array.
/// Invalidation: With the render-world slot.
/// Units: Pixels (screen space: surface pixels; world space: panel canvas pixels); colours sRGB-encoded RGBA8 straight alpha.
/// Errors: None.
/// Version: since Z2 Stage 3. The layout is fixed by Zenith2/Docs/Design/UI/Rendering.md "Flux_UIQuad layout".
/// @endcontract
struct Flux_UIQuad
{
	float m_afOrigin[2];             // the quad's local (0, 0) corner after the element transform
	float m_afAxisX[2];              // the pixel vector from local (0, 0) to local (1, 0)
	float m_afAxisY[2];              // the pixel vector from local (0, 0) to local (0, 1)
	uint16 m_auUV[4];               // UNORM16 u0, v0, u1, v1
	uint32 m_uColour;               // fill, glyph or tint colour (sRGB8 RGBA, R in bits 0–7)
	uint32 m_uColour2;              // gradient end colour, or border colour
	uint32 m_uModeFlags;            // bits 0–7 Flux_UIQuadMode; bit 8 gradient along y; bit 9 has border; bit 10 outline;
	                                 // bits 11–31 zero (Rendering.md "Flux_UIQuad layout")
	uint16 m_hCornerRadius;         // half: pixels, the uniform corner radius (non-uniform radii emit per-corner quads)
	uint16 m_hBorderWidth;          // half: pixels
	uint16 m_hSoftness;             // half: pixels of shadow blur or edge softness
	uint16 m_hScreenPxRange;        // half: glyphs, the distance range in screen pixels
	uint16 m_hSizeX;                // half: the local size in pixels along AxisX (shape SDF evaluation)
	uint16 m_hSizeY;                // half
	uint16 m_hOutlineWidth;         // half: glyphs, outline width in distance units (0–0.5)
	uint16 m_hReserved;             // 0
	uint32 m_uOutlineColour;        // glyph outline colour, sRGB8 RGBA
};
static_assert(sizeof(Flux_UIQuad) == 64);

/// @contract value Flux_UIBatch
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: an element its producer writes in place in an arena or Zenith_Array.
/// Invalidation: With the render-world slot or the arena it lives in.
/// Units: Scissor in pixels; quad indices into the list's quad span.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_UIBatch
{
	uint32 m_uFirstQuad = 0;
	uint32 m_uQuadCount = 0;
	sint32 m_aiScissor[4] = {};     // x, y, width, height; width 0 = unclipped
	Zenith_RHIDescriptorHandle m_xTexture;   // the atlas page or image; the null handle for untextured batches
	bool m_bSDFText = false;         // selects bPERM_SDF_TEXT = true
};

/// @contract value Flux_RenderUIList
/// Ownership: Plain value in the render world: spans point into the same render-world slot arena, written by
///   Flux_RenderWorldWriter::AddUIList, which copies the quads and batches ([Z2-N-REND-008]).
/// Thread affinity: IMMUTABLE after Render.Seal; members non-const: its producer default-constructs it and fills it
///   member by member.
/// Invalidation: With the render-world slot.
/// Units: Pixels.
/// Errors: AddUIList returns FLUX_RENDER_WORLD_ERROR_SLOT_FULL when the copy does not fit; the UI extractor then counts
///   ui.draw.slot_full and the frame draws no UI for that screen set.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_RenderUIList
{
	Zenith_Span<const Flux_UIQuad> m_axQuads;
	Zenith_Span<const Flux_UIBatch> m_axScreenBatches;
	Zenith_Span<const Flux_UIBatch> m_axWorldBatches;
	Zenith_Span<const Zenith_Matrix4> m_axWorldPanelMatrices;   // canvas pixels → world metres, one per world panel
	Zenith_Span<const uint32> m_auWorldPanelFirstBatch;        // index into m_axWorldBatches; panel i ends where i + 1 starts
	uint32 m_uSurfaceWidth = 0;
	uint32 m_uSurfaceHeight = 0;
	uint32 m_uOutputViewKindMask = 0;                          // views that composite this list (MAIN | EDITOR game view)
	uint16 m_uWorldTag = 0;                                    // the extracting world; matched against the view's source world
	float m_fWorldLuminanceNits = 100.0f;
};
inline constexpr uint32 uFLUX_UI_MAX_QUADS_PER_LIST = 65536;

/// @contract value Flux_UIAtlasPageDesc
/// Ownership: Plain value; m_xPixels views the caller's bytes for the CreatePage call, which copies them into upload staging.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: Texels.
/// Errors: Validated by CreatePage (FLUX_RESOURCE_ERROR_* of RHI).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_UIAtlasPageDesc
{
	uint16 m_uWidth = 1024;
	uint16 m_uHeight = 1024;
	Zenith_ByteSpan m_xPixels;       // RGBA8 MTSDF, rows top-down, tightly packed (width × height × 4 bytes)
	bool m_bColour = false;          // a colour-image page (sRGB) rather than a distance-field page (UNORM)
};

/// @contract value Flux_UIAtlasPage
/// Ownership: Plain value naming a persistent read-only texture the UI renderer created; released with ReleasePage.
/// Thread affinity: IMMUTABLE; members non-const: a value type its holders assign and reassign.
/// Invalidation: Invalid after ReleasePage; its descriptor stays valid until deferred deletion retires it.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_UIAtlasPage
{
	Flux_PersistentReadOnlyTexture m_xTexture;
};

/// @contract value Flux_UIRendererDesc
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: Counts.
/// Errors: Validated by Flux_Renderer::EnableUI.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_UIRendererDesc
{
	uint32 m_uMaxPages = 256;
	uint32 m_uMaxQuadsPerFrame = 262144;
	float m_fSDFSharpness = 1.0f;
};

/// @contract type Flux_UIRenderer
/// Ownership: Created and owned by Flux_Renderer when the UI module's start calls Flux_Renderer::EnableUI (cross-area row UI7);
///   lives until the renderer's destruction; owns the atlas page bookkeeping and the per-frame UI frame data.
/// Thread affinity: ANY_THREAD_EXCEPT(PrepareFrame, ImportQuadChunk, WriteFrame, GetFrame). CreatePage, ReleasePage and IsPageResident are ANY_THREAD
///   (they forward to Flux_PersistentResources, lock 480); PrepareFrame, ImportQuadChunk, WriteFrame and GetFrame run on the
///   Flux.BuildGraph node, in the build-node upload order (Zenith2/Docs/Design/RHI/Memory.md "Build-node upload order"):
///   PrepareFrame declares (RPROD step 4), ImportQuadChunk imports (pass setup), WriteFrame writes after ExtendReservation
///   (RPROD step 11); WriteFrame before the frame's ExtendReservation asserts inside Flux_UploadRing::GetFrameData.
/// Invalidation: The Flux_UIFrame pointer is valid for the frame being built.
/// Units: Pixels.
/// Errors: CreatePage returns Flux_ResourceError (out of device memory, memory pressure); the UI glyph cache then keeps the
///   page CPU-side and retries at the next frame (Zenith2/Docs/Design/UI/Text.md "Page upload and residency").
/// Version: since Z2 Stage 3. Source API.
/// @endcontract
class Flux_UIRenderer
{
public:
	Zenith_Result<Flux_UIAtlasPage, Flux_ResourceError> CreatePage(const Flux_UIAtlasPageDesc& xDesc);
	void ReleasePage(Flux_UIAtlasPage xPage);
	bool IsPageResident(const Flux_UIAtlasPage& xPage) const;
	uint64 GetPageBytes() const;
	void PrepareFrame(const Flux_RenderWorld& xWorld, Flux_UploadRing& xRing);   // step 4: list bases + DeclareFrameData
	Flux_RGBuffer ImportQuadChunk(Flux_RenderGraph& xGraph);      // pass setup: xRing.ImportFrameChunk
	void WriteFrame(Flux_UploadRing& xRing);                                    // step 11, after FITS or EXTENDED
	const Flux_UIFrame* GetFrame() const;
};

// Flux_UIFrame is defined in Internal/UI/Flux_UIFrame.h (below); Flux_FrameShared::m_pxUIFrame and GetFrame name it by pointer.

/// @contract functions Flux_UIPassFunctions
/// Ownership: Declare graph passes; own nothing.
/// Thread affinity: The Flux.BuildGraph node.
/// Invalidation: None.
/// Units: Pixels.
/// Errors: Graph diagnostics only.
/// Version: since Z2 Stage 3. Flux_AddUIComposite keeps Rendering's signature exactly (Zenith2/Docs/Rendering/PerPass/Common.md
///   "UIComposite"); Flux_AddUIWorldSpace is called by the UI module's view extension at FLUX_EXTENSION_POINT_TRANSPARENT.
/// @endcontract
void Flux_AddUIComposite(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_RGTexture& xOutput);
void Flux_AddUIWorldSpace(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);
```

`Flux_RenderWorld`, `Flux_ViewTargets`, `Flux_UploadRing` and `Flux_UIFrame` are forward-declared because the public header uses them by reference or pointer only; the upload ring is an `Internal/` type of ZenithFlux, so the frame record that holds its `Flux_FrameDataRange` by value is internal too:

```cpp
// Zenith2/Engine/ZenithFlux/Internal/UI/Flux_UIFrame.h (Internal; included by Flux_UIRenderer.cpp, Flux_UIComposite.cpp, Flux_UIWorldSpace.cpp)
#pragma once
#include "ZenithFlux/Flux_UI.h"
#include "ZenithFlux/Internal/Upload/Flux_UploadRing.h"

/// @contract value Flux_UIFrame
/// Ownership: Owned by Flux_UIRenderer in the graph arena of the frame being built; referenced by Flux_FrameShared::m_pxUIFrame
///   (Zenith2/Docs/Rendering.md "Shared producers", [Z2-N-REND-995]).
/// Thread affinity: Written by PrepareFrame (lists, bases, range) and WriteFrame (m_uQuadBaseIndex) on Flux.BuildGraph;
///   IMMUTABLE for the record jobs.
/// Invalidation: With the frame.
/// Units: Pixels.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_UIFrame
{
	Flux_FrameDataRange m_xQuadData;                 // the ticket of DeclareFrameData(Σquads × 64, 64, "UIComposite")
	uint32 m_uQuadBaseIndex = 0;                    // GetFrameData(m_xQuadData).m_ulOffset / 64, set by WriteFrame
	Zenith_Span<const Flux_RenderUIList> m_axLists;  // the kept lists of every section, ascending world tag
	Zenith_Span<const uint32> m_auListQuadBase;     // first quad of each kept list, relative to m_uQuadBaseIndex
};
```

 The world tag of a list is `Flux_RenderUIList::m_uWorldTag`, so `Flux_UIFrame` keeps no separate tag array.

<!-- assumption: pending RPROD (UI7 (b), UI8 (d)) -->
RPROD's `Flux.BuildGraph` calls `PrepareFrame` in its step 4 and `WriteFrame` in its step 11 (`Zenith2/Docs/Design/RPROD/Renderer.md → "Flux.BuildGraph"`), and `Flux_ViewContext` carries the view's source world tag; rows UI7 (b) and UI8 (d) of the cross-area notes carry them. `Flux_Renderer::EnableUI` is RPROD's declaration (`Zenith2/Docs/Design/RPROD/Renderer.md → "Flux_Renderer header"`; UI's ticket writes its body in RPROD's renderer files) and `Flux_FrameShared::m_pxUIFrame` is Rendering's (`Zenith2/Docs/Rendering.md → "Shared producers"`, [Z2-N-REND-995]); neither is assumed.
<!-- /assumption -->

## Tool-side headers

| Path | Declares | Called by |
|---|---|---|
| `Zenith2/Tools/ZenithCook/Internal/UI/Zenith_UICookSteps.h` | `Zenith_Result<void, Zenith_CookError> Zenith_RegisterUICookSteps(Zenith_CookStepRegistry& xRegistry, Zenith_CookServices& xServices)` (the one signature of `Zenith2/Docs/Design/ASSET/Cook.md → "Step registration functions"`) | `Zenith_CookServices::Create`, in UI's module order |
| `Zenith2/Tools/ZenithCook/Internal/UI/Zenith_UIImporters.h` | `Zenith_Result<void, Zenith_ImportError> Zenith_RegisterUIImporters(Zenith_ImporterRegistry& xRegistry)` — the `ui.fontfamily` importer | the cook services, after ASSET's passthrough importers |
| `Zenith2/Tools/Authoring/Zenith_AuthoringUI.h` (the tool library `ZenithAuthoring`, `Zenith2/Docs/Design/UI.md → "ZenithAuthoring UI files"`; included as `#include "Authoring/Zenith_AuthoringUI.h"`, implemented in `Zenith2/Tools/Authoring/Internal/UI/Zenith_UIDocumentTypes.cpp`) | `Zenith_Result<void, Zenith_DocumentError> Zenith_RegisterUIDocumentTypes(Zenith_MigrationRegistry& xRegistry)`, `const Zenith_CanonicalSchema& Zenith_GetUICanonicalSchema(Zenith_UIDocumentType eType)`, `enum Zenith_UIDocumentType : uint8 { UI_DOCUMENT_TYPE_UI = 0, UI_DOCUMENT_TYPE_THEME, UI_DOCUMENT_TYPE_FONT_FAMILY, UI_DOCUMENT_TYPE_LOCALIZATION_PROJECT, UI_DOCUMENT_TYPE_STRING_TABLE, UI_DOCUMENT_TYPE_COUNT }`, `inline constexpr uint32 uUI_DOCUMENT_SCHEMA_VERSION = 1` | ZenithCook's composition root and ZenithEditor (both link `ZenithAuthoring`), before `Zenith_MigrationRegistry::Freeze` (ENV's tool-side precedent, `Zenith2/Docs/Design/ENV/Bake.md → "Environment document types"`) |
| `Zenith2/Tools/ZenithCook/Internal/UI/Zenith_UIXliff.h` | `Zenith_Result<void, Zenith_CookError> Zenith_WriteXliffExport(const Zenith_UIExtractionResult& xResult, Zenith_StringView strTargetLocale, Zenith_Array<char>& xOut)`, `Zenith_Result<Zenith_UIXliffImportReport, Zenith_CookError> Zenith_ReadXliffImport(Zenith_ByteSpan xXliff, Zenith_DocumentStorage& xStorage)` | the `ui.extract` step and the `--import-xliff` verb |
| `Zenith2/Tools/ZenithCook/Internal/UI/Zenith_UIExtraction.h` | `Zenith_UIExtractedKey`, `Zenith_UIExtractionSink`, `Zenith_UIExtractionResult` (`Zenith2/Docs/Design/UI/Localization.md → "Extraction sources"`) | every cook step that contributes keys; the `ui.extract` step |

The tool-side headers carry these contract blocks, in the format of `Zenith2/Docs/Contracts.md → "Contract blocks per type"`:

```cpp
/// @contract functions Zenith_UICookStepRegistration
/// Ownership: Registers the six UI cook steps into the caller's registry; owns nothing.
/// Thread affinity: MAIN_THREAD of the cook process, before the registry is frozen.
/// Invalidation: None.
/// Units: None.
/// Errors: COOK_ERROR_* of ASSET when a step name is already registered; never asserts on the registry's state.
/// Version: since Z2 Stage 3.
/// @endcontract

/// @contract functions Zenith_UIImporterRegistration
/// Ownership: Registers the ui.fontfamily importer; owns nothing.
/// Thread affinity: MAIN_THREAD of the cook process, after ASSET's passthrough importers.
/// Invalidation: None.
/// Units: None.
/// Errors: IMPORT_ERROR_* of ASSET on a duplicate importer name.
/// Version: since Z2 Stage 3.
/// @endcontract

/// @contract enum Zenith_UIDocumentType
/// Ownership: Plain enumeration; the row of Documents.md "Canonical schemas and document types".
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; values never move.
/// Version: since Z2 Stage 3.
/// @endcontract

/// @contract functions Zenith_UIDocumentTypes
/// Ownership: Zenith_RegisterUIDocumentTypes adds five migration-registry rows; Zenith_GetUICanonicalSchema returns a static schema.
/// Thread affinity: MAIN_THREAD before Zenith_MigrationRegistry::Freeze; Zenith_GetUICanonicalSchema ANY_THREAD.
/// Invalidation: The schema reference lives for the process.
/// Units: None.
/// Errors: DOCUMENT_ERROR_* of SCENE on a duplicate stable name; an out-of-range type is ZENITH_ASSERT (a programmer error, tools only).
/// Version: since Z2 Stage 3.
/// @endcontract

/// @contract value Zenith_UIExtractedKey
/// Ownership: Plain value; the views borrow the contributing step's strings for the AddKey call, which copies them.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: Lines; em for m_fMaxWidthEm.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract

/// @contract type Zenith_UIExtractionSink
/// Ownership: Owned by the cook services for one cook; borrowed by every step that contributes keys.
/// Thread affinity: ANY_THREAD: AddKey takes the sink's lock, a leaf of the cook's lock order (ASSET).
/// Invalidation: Invalid after the ui.extract step has run; an AddKey after that is counted and ignored.
/// Units: None.
/// Errors: None: a key outside the key grammar is recorded as an undefined key for the report.
/// Reentrancy: NONE. AddKey invokes no callback.
/// Version: since Z2 Stage 4.
/// @endcontract

/// @contract value Zenith_UIExtractionResult
/// Ownership: Plain value; spans live in the ui.extract step's arena for the step.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: With the step's arena.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract

/// @contract value Zenith_UIXliffRejectedUnit
/// Ownership: Plain value in the import report.
/// Thread affinity: IMMUTABLE; members non-const: an element its producer writes in place in an arena or Zenith_Array.
/// Invalidation: With the report.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract

/// @contract value Zenith_UIXliffImportReport
/// Ownership: Plain value; spans live in the arena of the --import-xliff verb.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: With that arena.
/// Units: Counts of units.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract

/// @contract functions Zenith_UIXliff
/// Ownership: Zenith_WriteXliffExport appends to the caller's array; Zenith_ReadXliffImport writes accepted units through the document storage's journaled write.
/// Thread affinity: The cook's main thread.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: COOK_ERROR_* on malformed XLIFF (parser limits of Documents.md "Parser limits"); a unit that fails Zenith_CompileMessage is listed as rejected and changes nothing.
/// Version: since Z2 Stage 4.
/// @endcontract
```
