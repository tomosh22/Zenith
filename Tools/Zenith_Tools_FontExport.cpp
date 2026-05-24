#include "Zenith.h"
#include "Zenith_Tools_FontExport.h"
#include "Zenith_Tools_TextureExport.h"
#include "AssetHandling/Zenith_FontAsset.h"
#include "DataStream/Zenith_DataStream.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#pragma warning(push, 0)
#include "msdf-atlas-gen/msdf-atlas-gen.h"
#pragma warning(pop)
#include "Memory/Zenith_MemoryManagement_Enabled.h"

#include <vector>

extern const char* Project_GetName();

// Atlas + bake parameters. Tuned for printable ASCII with a 4px MSDF range —
// sharp at any render size via the shader's fwidth-based AA.
//
// 48 px/em + 512x512 atlas: chosen so complex glyphs (LiberationMono 'l' has a
// curved hook at the bottom-left foot) have enough atlas pixels for MSDF's
// channel-disagreement errors to be rare. 32 px/em produced visible stray-pixel
// artifacts on 'l'-style hooks because the serif was only 1-2 atlas pixels wide.
static constexpr int      sk_iAtlasSize       = 512;
static constexpr double   sk_dPxPerEm         = 48.0;
static constexpr double   sk_dPxRange         = 4.0;
static constexpr double   sk_dMiterLimit      = 1.0;
static constexpr double   sk_dMaxCornerAngle  = 3.0;
static constexpr u_int32  sk_uFirstCodepoint  = 32;   // space
static constexpr u_int32  sk_uLastCodepoint   = 126;  // tilde
static constexpr u_int32  sk_uGlyphCount      = sk_uLastCodepoint - sk_uFirstCodepoint + 1;

// Tuned visual offset for Zenith_UILayoutGroup baseline correction; NOT derived
// from typographic ascent. See Zenith_FontAsset::FontMetrics docs.
static constexpr float    sk_fLayoutAscenderCorrection = 0.25f;

void Zenith_Tools_FontExport::ExportFromFile(const std::string& strTTFPath, const std::string& strZFontPath, const std::string& strAtlasPath)
{
	ExportFromFile(strTTFPath, strZFontPath, strAtlasPath, sk_iAtlasSize, static_cast<float>(sk_dPxPerEm), static_cast<float>(sk_dPxRange));
}

void Zenith_Tools_FontExport::ExportFromFile(const std::string& strTTFPath, const std::string& strZFontPath, const std::string& strAtlasPath,
											 uint32_t uAtlasSize, float fPxPerEm, float fPxRange)
{
	using namespace msdf_atlas;

	msdfgen::FreetypeHandle* pxFt = msdfgen::initializeFreetype();
	if (!pxFt)
	{
		Zenith_Warning(LOG_CATEGORY_TOOLS, "msdf-atlas-gen: failed to initialise FreeType");
		return;
	}

	msdfgen::FontHandle* pxFont = msdfgen::loadFont(pxFt, strTTFPath.c_str());
	if (!pxFont)
	{
		Zenith_Warning(LOG_CATEGORY_TOOLS, "msdf-atlas-gen: failed to load font %s", strTTFPath.c_str());
		msdfgen::deinitializeFreetype(pxFt);
		return;
	}

	// Charset = printable ASCII 32..126 (95 glyphs). Excludes 127 (DEL) on purpose;
	// extended-ASCII 127-131 from the old bitmap atlas wasn't used in practice.
	Charset xCharset;
	for (u_int32 u = sk_uFirstCodepoint; u <= sk_uLastCodepoint; ++u)
	{
		xCharset.add(u);
	}

	std::vector<GlyphGeometry> xGlyphs;
	FontGeometry xFontGeometry(&xGlyphs);
	const int iLoaded = xFontGeometry.loadCharset(pxFont, 1.0, xCharset);
	if (iLoaded < static_cast<int>(sk_uGlyphCount))
	{
		Zenith_Warning(LOG_CATEGORY_TOOLS, "msdf-atlas-gen: loaded only %d of %u glyphs from %s",
			iLoaded, sk_uGlyphCount, strTTFPath.c_str());
	}

	for (GlyphGeometry& xGlyph : xGlyphs)
	{
		xGlyph.edgeColoring(&msdfgen::edgeColoringInkTrap, sk_dMaxCornerAngle, 0);
	}

	// Pack into a fixed atlas (no auto-fit — we want predictable 256x256 output
	// so the runtime side asserts on dimension mismatch are simple).
	TightAtlasPacker xPacker;
	xPacker.setDimensions(static_cast<int>(uAtlasSize), static_cast<int>(uAtlasSize));
	xPacker.setScale(static_cast<double>(fPxPerEm));
	xPacker.setPixelRange(static_cast<double>(fPxRange));
	xPacker.setMiterLimit(sk_dMiterLimit);
	// Default spacing is 0, which makes adjacent glyph atlas boxes touch.
	// Linear filtering at the edge of one glyph's UV rect then samples into
	// the next glyph's SDF data, manifesting as e.g. a stray dot in the
	// corner of narrow glyphs like 'l' when '.' is packed nearby.
	// 2 pixels of black gutter is enough to absorb the half-texel sample.
	xPacker.setSpacing(2);
	const int iPackRemaining = xPacker.pack(xGlyphs.data(), static_cast<int>(xGlyphs.size()));
	if (iPackRemaining > 0)
	{
		Zenith_Warning(LOG_CATEGORY_TOOLS,
			"msdf-atlas-gen: %d glyphs failed to pack into %ux%u atlas — bump dimensions or shrink px/em",
			iPackRemaining, uAtlasSize, uAtlasSize);
		msdfgen::destroyFont(pxFont);
		msdfgen::deinitializeFreetype(pxFt);
		return;
	}

	int iAtlasW = 0, iAtlasH = 0;
	xPacker.getDimensions(iAtlasW, iAtlasH);

	// Generate the MSDF.
	// NOTE: MSVC has a parser issue if `&msdfGenerator` is used here as the
	// non-type template arg (C2102 deep inside the template body). Decay
	// the function to a pointer by passing the name alone.
	using AtlasGenerator = ImmediateAtlasGenerator<float, 3, msdfGenerator, BitmapAtlasStorage<byte, 3>>;
	AtlasGenerator xGenerator(iAtlasW, iAtlasH);
	GeneratorAttributes xAttribs;
	// MSDF edge-coloring can produce pixels where two channels agree they're
	// "inside" but the third (correctly) says "outside" — the median then says
	// "inside" and we render stray ink (notably a "dot" at LiberationMono 'l's
	// bottom-left). Edge-priority correction won't catch this; INDISCRIMINATE
	// + ALWAYS_CHECK_DISTANCE plus a scanline-topology verification pass
	// reliably eliminates it.
	xAttribs.config.errorCorrection.mode = msdfgen::ErrorCorrectionConfig::INDISCRIMINATE;
	xAttribs.config.errorCorrection.distanceCheckMode = msdfgen::ErrorCorrectionConfig::ALWAYS_CHECK_DISTANCE;
	xAttribs.scanlinePass = true;
	xGenerator.setAttributes(xAttribs);
	xGenerator.setThreadCount(4);
	xGenerator.generate(xGlyphs.data(), static_cast<int>(xGlyphs.size()));

	const msdfgen::BitmapConstRef<byte, 3> xMSDF = xGenerator.atlasStorage();
	Zenith_Assert(xMSDF.width == iAtlasW && xMSDF.height == iAtlasH, "atlas storage dimensions disagree with packer");

	// Convert msdfgen's RGB Y-up bitmap to Zenith's RGBA8 Y-down layout.
	// msdfgen pixel (0,0) is bottom-left; engine pixel (0,0) is top-left.
	// Atlas UVs we write into .zfont below are flipped to match.
	const size_t ulPixelCount = static_cast<size_t>(iAtlasW) * static_cast<size_t>(iAtlasH);
	std::vector<u_int8> axRGBA(ulPixelCount * 4, 0);
	for (int iY = 0; iY < iAtlasH; ++iY)
	{
		const int iSrcY = iAtlasH - 1 - iY;
		const byte* pSrcRow = xMSDF.pixels + 3 * iSrcY * iAtlasW;
		u_int8* pDstRow = axRGBA.data() + 4 * static_cast<size_t>(iY) * iAtlasW;
		for (int iX = 0; iX < iAtlasW; ++iX)
		{
			pDstRow[4 * iX + 0] = pSrcRow[3 * iX + 0];
			pDstRow[4 * iX + 1] = pSrcRow[3 * iX + 1];
			pDstRow[4 * iX + 2] = pSrcRow[3 * iX + 2];
			pDstRow[4 * iX + 3] = 255;
		}
	}

	// .ztxtr output — caller supplies the exact target path.
	Zenith_Tools_TextureExport::ExportFromData(axRGBA.data(), strAtlasPath, iAtlasW, iAtlasH, TEXTURE_FORMAT_RGBA8_UNORM);

	// Gather metrics for the .zfont.
	const msdfgen::FontMetrics& xMsdfMetrics = xFontGeometry.getMetrics();
	const double dEmSize           = xMsdfMetrics.emSize > 0.0 ? xMsdfMetrics.emSize : 1.0;
	const float  fAscentEm         = static_cast<float>( xMsdfMetrics.ascenderY   / dEmSize);
	const float  fDescentEm        = static_cast<float>( xMsdfMetrics.descenderY  / dEmSize);
	const float  fLineHeightEm     = static_cast<float>( xMsdfMetrics.lineHeight  / dEmSize);

	// Build sorted, contiguous GlyphMetric array indexed by (codepoint - first).
	std::vector<Zenith_FontGlyphMetric> axMetrics(sk_uGlyphCount);
	for (u_int32 u = 0; u < sk_uGlyphCount; ++u)
	{
		axMetrics[u].m_uCodepoint = sk_uFirstCodepoint + u;
		// Default to all-zero bounds; whitespace stays this way, ink-bearing
		// glyphs get overwritten in the loop below. Advance is filled per-glyph.
		axMetrics[u].m_fAtlasU0 = 0.f; axMetrics[u].m_fAtlasV0 = 0.f;
		axMetrics[u].m_fAtlasU1 = 0.f; axMetrics[u].m_fAtlasV1 = 0.f;
		axMetrics[u].m_fPlaneL  = 0.f; axMetrics[u].m_fPlaneT  = 0.f;
		axMetrics[u].m_fPlaneR  = 0.f; axMetrics[u].m_fPlaneB  = 0.f;
		axMetrics[u].m_fAdvance = 0.f;
	}

	double dEmAdvanceSum = 0.0;
	u_int32 uEmAdvanceCount = 0;

	for (const GlyphGeometry& xGlyph : xGlyphs)
	{
		const u_int32 uCp = static_cast<u_int32>(xGlyph.getCodepoint());
		if (uCp < sk_uFirstCodepoint || uCp > sk_uLastCodepoint)
		{
			continue;
		}
		Zenith_FontGlyphMetric& xOut = axMetrics[uCp - sk_uFirstCodepoint];

		// Advance: msdfgen reports advance in font units (geometryScale=1.0);
		// divide by emSize to convert to em units, matching the engine's
		// "all per-glyph metrics in em" convention.
		xOut.m_fAdvance = static_cast<float>(xGlyph.getAdvance() / dEmSize);
		dEmAdvanceSum  += xOut.m_fAdvance;
		++uEmAdvanceCount;

		if (xGlyph.isWhitespace())
		{
			// Space etc.: zero bounds, non-zero advance. Renderer will skip
			// emitting a quad but still apply advance.
			continue;
		}

		// Plane bounds: msdfgen returns (l, b, r, t) in font units, Y-up baseline-origin.
		// Convert to em units, Y-down, baseline-relative — see Zenith_FontAsset.h.
		double dPlaneL = 0.0, dPlaneB = 0.0, dPlaneR = 0.0, dPlaneT = 0.0;
		xGlyph.getQuadPlaneBounds(dPlaneL, dPlaneB, dPlaneR, dPlaneT);
		xOut.m_fPlaneL =  static_cast<float>(dPlaneL / dEmSize);
		xOut.m_fPlaneR =  static_cast<float>(dPlaneR / dEmSize);
		xOut.m_fPlaneT = -static_cast<float>(dPlaneT / dEmSize);   // Y flip
		xOut.m_fPlaneB = -static_cast<float>(dPlaneB / dEmSize);   // Y flip
		Zenith_Assert(xOut.m_fPlaneT <= xOut.m_fPlaneB,
			"plane bounds Y-down convention violated after conversion (codepoint %u)", uCp);

		// Atlas bounds: msdfgen returns (l, b, r, t) in pixels, Y-up bottom-left origin.
		// Convert to engine UVs (Y-down, top-left origin, [0,1]).
		double dAtlasL = 0.0, dAtlasB = 0.0, dAtlasR = 0.0, dAtlasT = 0.0;
		xGlyph.getQuadAtlasBounds(dAtlasL, dAtlasB, dAtlasR, dAtlasT);
		const double dAtlasH = static_cast<double>(iAtlasH);
		const double dAtlasW = static_cast<double>(iAtlasW);
		xOut.m_fAtlasU0 = static_cast<float>(dAtlasL / dAtlasW);
		xOut.m_fAtlasU1 = static_cast<float>(dAtlasR / dAtlasW);
		xOut.m_fAtlasV0 = static_cast<float>((dAtlasH - dAtlasT) / dAtlasH);  // V flip
		xOut.m_fAtlasV1 = static_cast<float>((dAtlasH - dAtlasB) / dAtlasH);  // V flip
	}

	// Sanity asserts on the produced metrics: space must have a non-zero advance.
	{
		const Zenith_FontGlyphMetric& xSpace = axMetrics[' ' - sk_uFirstCodepoint];
		Zenith_Assert(xSpace.m_fAdvance > 0.f, "exporter produced zero advance for space glyph");
	}

	// fEmAdvance is the "cell width" the engine assumes for every char in
	// layout / measurement calls. Historically hard-coded to 0.55 in
	// Flux_TextImpl.h. Preserve that exact value so existing UI layouts
	// (TilePuzzle tutorials, button widths, Sokoban HUD) don't overflow.
	//
	// We ALSO overwrite every glyph's per-glyph advance with the same value
	// below, forcing monospace rendering at the legacy spacing — otherwise
	// rendering uses natural advances (~0.6 for LiberationMono) while layout
	// uses 0.55, and the mismatch makes text overflow its layout box.
	//
	// For a future proportional font (with per-glyph kerning) the right answer
	// is to plumb MeasureRunPx through UI; for now monospace-at-0.55 matches
	// the engine's existing baked-in assumption end-to-end.
	(void)uEmAdvanceCount;
	(void)dEmAdvanceSum;
	const float fEmAdvance = 0.55f;
	for (Zenith_FontGlyphMetric& xMetric : axMetrics)
	{
		xMetric.m_fAdvance = fEmAdvance;
	}

	// Write the .zfont. Layout must match the spec in Zenith_FontAsset.h.
	Zenith_DataStream xStream;
	xStream << Zenith_FontAsset::sk_uMagic;
	xStream << Zenith_FontAsset::sk_uVersion;
	xStream << static_cast<float>(fPxPerEm);             // fEmSize (px per em the atlas was baked at)
	xStream << static_cast<float>(fPxRange);             // fAtlasPxRange
	xStream << static_cast<float>(iAtlasW);
	xStream << static_cast<float>(iAtlasH);
	xStream << fAscentEm;
	xStream << fDescentEm;
	xStream << fLineHeightEm;
	xStream << fEmAdvance;
	xStream << sk_fLayoutAscenderCorrection;
	xStream << sk_uFirstCodepoint;
	xStream << sk_uLastCodepoint;
	xStream << sk_uGlyphCount;
	xStream.WriteData(axMetrics.data(), axMetrics.size() * sizeof(Zenith_FontGlyphMetric));

	// Path to the atlas, stored with engine: prefix so runtime can resolve it
	// across machines. The atlas file we wrote above lives next to the .zfont's
	// output dir; assume the caller will commit it at engine:Textures/Font/FontAtlas.ztxtr.
	const std::string strAtlasEnginePath = "engine:Textures/Font/FontAtlas.ztxtr";
	xStream << strAtlasEnginePath;

	xStream.WriteToFile(strZFontPath.c_str());

	Zenith_Log(LOG_CATEGORY_TOOLS,
		"Exported MSDF font: %s + %s (%dx%d atlas, %u glyphs, em=%.1fpx, range=%.1fpx)",
		strZFontPath.c_str(), strAtlasPath.c_str(), iAtlasW, iAtlasH, sk_uGlyphCount, fPxPerEm, fPxRange);

	msdfgen::destroyFont(pxFont);
	msdfgen::deinitializeFreetype(pxFt);
}

void ExportDefaultFontAtlas()
{
	const std::string strFontPath  = std::string(ENGINE_ASSETS_DIR) + "Fonts/LiberationMono-Regular.ttf";
	const std::string strZFontPath = std::string(ENGINE_ASSETS_DIR) + "Fonts/LiberationMono.zfont";
	const std::string strAtlasPath = std::string(ENGINE_ASSETS_DIR) + "Textures/Font/FontAtlas.ztxtr";

	Zenith_Tools_FontExport::ExportFromFile(strFontPath, strZFontPath, strAtlasPath);
}
