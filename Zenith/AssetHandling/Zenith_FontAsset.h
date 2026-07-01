#pragma once

#include "AssetHandling/Zenith_Asset.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

class Flux_TextImpl;

// Per-glyph entry in the .zfont binary metrics file.
//
// Coordinate contract:
//   Atlas UVs are in [0,1], TOP-LEFT origin (engine Y-down convention).
//   Plane bounds are in em units, RELATIVE TO BASELINE, Y-down.
//     i.e. positive Y is DOWN. baseline is y = 0.
//     an ascender's TOP has a NEGATIVE plane-T.
//   Runtime computes a glyph's screen quad by adding plane bounds to
//   (cursorX, baselineY), where baselineY starts at the text-root + ascent.
//
// Ink-less glyphs (e.g. space, codepoint 32) MUST be present in the table
// with non-zero m_fAdvance and all atlas/plane bounds zero. The renderer
// detects this and skips emitting a quad while still advancing the cursor.
struct Zenith_FontGlyphMetric
{
	u_int32 m_uCodepoint;
	float   m_fAtlasU0, m_fAtlasV0, m_fAtlasU1, m_fAtlasV1;
	float   m_fPlaneL,  m_fPlaneT,  m_fPlaneR,  m_fPlaneB;
	float   m_fAdvance;
};
static_assert(sizeof(Zenith_FontGlyphMetric) == 40, ".zfont glyph metric layout drift breaks the on-disk binary format");

// .zfont file layout (binary, little-endian, written via Zenith_DataStream):
//
//   u_int32   uMagic            = Zenith_FontAsset::sk_uMagic ('FONT')
//   u_int32   uVersion          = Zenith_FontAsset::sk_uVersion
//   float     fEmSize                       // px the atlas was baked at
//   float     fAtlasPxRange                 // msdfgen "range" in atlas pixels
//   float     fAtlasWidth, fAtlasHeight     // px
//   float     fAscent, fDescent, fLineHeight        // em units, Y-down
//   float     fEmAdvance                    // em units; UI uses this as default
//                                           //   "monospace cell width" for layout
//   float     fLayoutAscenderCorrection     // em units, Y-down. tuned visual
//                                           //   offset for layout-group baseline
//                                           //   alignment; NOT a derived metric.
//   u_int32   uFirstCodepoint, uLastCodepoint   // inclusive
//   u_int32   uGlyphCount                   // must equal uLast - uFirst + 1
//   Zenith_FontGlyphMetric  axGlyphs[uGlyphCount]   // sorted, contiguous codepoints
//   std::string             strAtlasPath    // e.g. "engine:Textures/Font/FontAtlas.ztxtr"
class Zenith_FontAsset : public Zenith_Asset
{
public:
	static constexpr u_int32 sk_uMagic    = 0x544E4F46;   // 'F' 'O' 'N' 'T' in little-endian
	static constexpr u_int32 sk_uVersion  = 1;

	// Font-wide metrics consumed by UI layout. Stable across reloads of the
	// same font; subscribe-once-and-cache is fine.
	struct FontMetrics
	{
		float fEmAdvance;                 // em units. UI cell-width fallback / default.
		float fLayoutAscenderCorrection;  // em units, Y-down. Layout-group baseline tuning.
		float fAscent;                    // em units, Y-down. True typographic ascent.
		float fLineHeight;                // em units.
	};

	Zenith_FontAsset() = default;
	~Zenith_FontAsset() override = default;

	Zenith_FontAsset(const Zenith_FontAsset&) = delete;
	Zenith_FontAsset& operator=(const Zenith_FontAsset&) = delete;

	ZENITH_ASSET_TYPE_NAME(Zenith_FontAsset)

	// Returns nullptr if the codepoint isn't in the bake range.
	const Zenith_FontGlyphMetric* FindGlyph(u_int32 uCodepoint) const;

	const FontMetrics&        GetMetrics() const            { return m_xMetrics; }
	float                     GetEmSize() const             { return m_fEmSize; }
	float                     GetAtlasPxRange() const       { return m_fAtlasPxRange; }
	Zenith_Maths::Vector2     GetAtlasSize() const          { return { m_fAtlasW, m_fAtlasH }; }
	const TextureHandle&      GetAtlasTexture() const       { return m_xAtlasTexture; }
	u_int32                   GetGlyphCount() const         { return static_cast<u_int32>(m_xGlyphs.GetSize()); }

	// Static fallback used when no font asset is loaded (boot order, headless
	// tests, pre-renderer-init UI rebuild). Values match the engine's pre-MSDF
	// hard-coded LiberationMono constants so existing layouts measure identically.
	static const FontMetrics& GetDefaultMetrics();

	// Static safe accessor: returns the active engine font's metrics if available,
	// otherwise GetDefaultMetrics(). No engine subsystem deref if not initialised;
	// safe from any code path including engine construction / shutdown / headless.
	//
	// Thread safety: intentionally non-atomic. UI layout runs on the main thread
	// and the metrics block is set once at asset-load time and never mutated.
	// If a future feature swaps the active font at runtime, add a generation
	// counter and require main-thread callers.
	static const FontMetrics& GetActiveOrDefaultMetrics();

private:
	template<typename U> friend struct Zenith_AssetLoadTraits;   // DoLoad calls private LoadFromFile

	// Loads the .zfont (and its referenced atlas .ztxtr as a procedural texture).
	// Path is "engine:..." or "game:..." form; resolved via Zenith_AssetRegistry::ResolvePath.
	Zenith_Status LoadFromFile(const std::string& strPrefixedPath);

	Zenith_Vector<Zenith_FontGlyphMetric> m_xGlyphs;        // contiguous codepoints; index = codepoint - m_uFirstCodepoint
	TextureHandle                         m_xAtlasTexture;  // procedural texture; no mips (see CreateFromData)
	FontMetrics                           m_xMetrics{};
	float                                 m_fEmSize        = 0.f;
	float                                 m_fAtlasPxRange  = 0.f;
	float                                 m_fAtlasW        = 0.f;
	float                                 m_fAtlasH        = 0.f;
	u_int32                               m_uFirstCodepoint = 0;
	u_int32                               m_uLastCodepoint  = 0;

	static const FontMetrics s_xDefaultMetrics;
};
