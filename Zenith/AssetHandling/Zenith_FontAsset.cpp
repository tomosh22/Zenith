#include "Zenith.h"
#include "AssetHandling/Zenith_FontAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "DataStream/Zenith_DataStream.h"
#include "Core/Zenith_Engine.h"
#include "Flux/Text/Flux_TextImpl.h"

// Default metrics tuned to reproduce the engine's pre-MSDF LiberationMono
// constants exactly, so UI layout doesn't shift when the runtime font asset
// isn't loaded (boot order, headless tests, pre-renderer-init UI rebuild).
const Zenith_FontAsset::FontMetrics Zenith_FontAsset::s_xDefaultMetrics = {
	/*fEmAdvance                = */ 0.55f,
	/*fLayoutAscenderCorrection = */ 0.25f,
	/*fAscent                   = */ 0.8f,
	/*fLineHeight               = */ 1.2f,
};

const Zenith_FontAsset::FontMetrics& Zenith_FontAsset::GetDefaultMetrics()
{
	return s_xDefaultMetrics;
}

const Zenith_FontAsset::FontMetrics& Zenith_FontAsset::GetActiveOrDefaultMetrics()
{
	// Defensive null-checks at every step; engine may be mid-construction,
	// torn down, or never initialised at all (headless test contexts).
	if (Flux_TextImpl* pxText = g_xEngine.TryGetText())
	{
		if (Zenith_FontAsset* pxFont = pxText->GetFontHandle().Resolve())
		{
			return pxFont->GetMetrics();
		}
	}
	return s_xDefaultMetrics;
}

const Zenith_FontGlyphMetric* Zenith_FontAsset::FindGlyph(u_int32 uCodepoint) const
{
	if (uCodepoint < m_uFirstCodepoint || uCodepoint > m_uLastCodepoint)
	{
		return nullptr;
	}
	// Constant-time lookup. Contiguity invariant is asserted at load.
	return &m_xGlyphs.Get(static_cast<u_int>(uCodepoint - m_uFirstCodepoint));
}

Zenith_Status Zenith_FontAsset::LoadFromFile(const std::string& strPrefixedPath)
{
	const std::string strAbsPath = Zenith_AssetRegistry::ResolvePath(strPrefixedPath);
	if (strAbsPath.empty())
	{
		Zenith_Warning(LOG_CATEGORY_ASSET, "Zenith_FontAsset: empty resolved path for '%s'", strPrefixedPath.c_str());
		return Zenith_ErrorCode::INVALID_ARGUMENT;
	}

	Zenith_DataStream xStream;
	xStream.ReadFromFile(strAbsPath.c_str());
	if (!xStream.IsValid())
	{
		Zenith_Warning(LOG_CATEGORY_ASSET, "Zenith_FontAsset: failed to read .zfont '%s'", strAbsPath.c_str());
		return Zenith_ErrorCode::FILE_NOT_FOUND;
	}

	u_int32 uMagic = 0, uVersion = 0;
	xStream >> uMagic;
	xStream >> uVersion;
	Zenith_Assert(uMagic == sk_uMagic, "bad .zfont header magic (got 0x%08X, want 0x%08X) at %s",
		uMagic, sk_uMagic, strAbsPath.c_str());
	Zenith_Assert(uVersion == sk_uVersion, "unsupported .zfont version %u (want %u) at %s",
		uVersion, sk_uVersion, strAbsPath.c_str());
	if (uMagic != sk_uMagic || uVersion != sk_uVersion)
	{
		return Zenith_ErrorCode::CORRUPT_DATA;
	}

	float fAscent = 0.f, fDescent = 0.f, fLineHeight = 0.f, fEmAdvance = 0.f, fLayoutAscenderCorrection = 0.f;
	xStream >> m_fEmSize;
	xStream >> m_fAtlasPxRange;
	xStream >> m_fAtlasW;
	xStream >> m_fAtlasH;
	xStream >> fAscent;
	xStream >> fDescent;
	xStream >> fLineHeight;
	xStream >> fEmAdvance;
	xStream >> fLayoutAscenderCorrection;

	m_xMetrics.fAscent                   = fAscent;
	m_xMetrics.fLineHeight               = fLineHeight;
	m_xMetrics.fEmAdvance                = fEmAdvance;
	m_xMetrics.fLayoutAscenderCorrection = fLayoutAscenderCorrection;
	(void)fDescent;

	u_int32 uGlyphCount = 0;
	xStream >> m_uFirstCodepoint;
	xStream >> m_uLastCodepoint;
	xStream >> uGlyphCount;

	// Contiguity invariant: FindGlyph relies on index = codepoint - first.
	Zenith_Assert(m_uLastCodepoint >= m_uFirstCodepoint,
		".zfont: last codepoint %u < first %u", m_uLastCodepoint, m_uFirstCodepoint);
	Zenith_Assert(uGlyphCount == (m_uLastCodepoint - m_uFirstCodepoint + 1),
		".zfont: charset is non-contiguous (count %u != range size %u)",
		uGlyphCount, m_uLastCodepoint - m_uFirstCodepoint + 1);
	if (uGlyphCount == 0 || uGlyphCount > 65536)
	{
		Zenith_Warning(LOG_CATEGORY_ASSET, "Zenith_FontAsset: implausible glyph count %u at %s", uGlyphCount, strAbsPath.c_str());
		return Zenith_ErrorCode::CORRUPT_DATA;
	}

	m_xGlyphs.Clear();
	for (u_int32 u = 0; u < uGlyphCount; ++u)
	{
		Zenith_FontGlyphMetric xMetric;
		xStream.ReadData(&xMetric, sizeof(xMetric));
		// Sortedness / monotonic-codepoint invariant.
		Zenith_Assert(xMetric.m_uCodepoint == m_uFirstCodepoint + u,
			".zfont glyph %u has codepoint %u (expected %u contiguous)",
			u, xMetric.m_uCodepoint, m_uFirstCodepoint + u);
		// Y-down convention sanity (only for glyphs with ink — whitespace bounds are 0).
		Zenith_Assert(xMetric.m_fPlaneT <= xMetric.m_fPlaneB,
			"plane bounds Y-down convention violated for codepoint %u", xMetric.m_uCodepoint);
		m_xGlyphs.PushBack(xMetric);
	}

	// Space (codepoint 32) MUST be present and MUST have a non-zero advance.
	const Zenith_FontGlyphMetric* pxSpace = FindGlyph(' ');
	Zenith_Assert(pxSpace != nullptr, ".zfont missing space glyph");
	Zenith_Assert(pxSpace && pxSpace->m_fAdvance > 0.f, ".zfont space has zero advance");

	std::string strAtlasPrefixedPath;
	xStream >> strAtlasPrefixedPath;
	if (strAtlasPrefixedPath.empty())
	{
		Zenith_Warning(LOG_CATEGORY_ASSET, "Zenith_FontAsset: .zfont has empty atlas path");
		return Zenith_ErrorCode::CORRUPT_DATA;
	}

	// --- Load the atlas .ztxtr as a procedural texture (no mips). ---
	// Route through the single .ztxtr parser (no GPU upload) rather than
	// hand-parsing the stream: there is exactly one .ztxtr parser engine-wide, and
	// it reads the envelope header (if any) before the payload. Hand-parsing would
	// misread the envelope magic as iWidth/iHeight if the font-atlas exporter ever
	// adopts the enveloped v1/v2 format. Today the atlas is headerless single-mip,
	// which LoadCPUData reads via its legacy branch (identical byte layout).
	const std::string strAtlasAbsPath = Zenith_AssetRegistry::ResolvePath(strAtlasPrefixedPath);
	Flux_SurfaceInfo xAtlasInfo;
	Zenith_Vector<uint8_t> xAtlasBytes;
	if (!Zenith_TextureAsset::LoadCPUData(strAtlasAbsPath, xAtlasInfo, xAtlasBytes).IsOk())
	{
		Zenith_Warning(LOG_CATEGORY_ASSET, "Zenith_FontAsset: failed to read atlas '%s'", strAtlasAbsPath.c_str());
		return Zenith_ErrorCode::FILE_NOT_FOUND;
	}

	// MSDF atlas MUST be RGBA8 with the exact dimensions in the .zfont metadata.
	// Catches stale-artefact mismatches at load instead of as garbled text on screen.
	Zenith_Assert(xAtlasInfo.m_eFormat == TEXTURE_FORMAT_RGBA8_UNORM,
		"font atlas '%s' must be RGBA8 (got format %d)", strAtlasAbsPath.c_str(), static_cast<int>(xAtlasInfo.m_eFormat));
	Zenith_Assert(static_cast<float>(xAtlasInfo.m_uWidth) == m_fAtlasW && static_cast<float>(xAtlasInfo.m_uHeight) == m_fAtlasH,
		"font atlas dims (%ux%u) don't match .zfont metadata (%.0fx%.0f)",
		xAtlasInfo.m_uWidth, xAtlasInfo.m_uHeight, m_fAtlasW, m_fAtlasH);

	const size_t ulExpected = static_cast<size_t>(xAtlasInfo.m_uWidth) * static_cast<size_t>(xAtlasInfo.m_uHeight) * 4;
	if (xAtlasBytes.GetSize() < ulExpected)
	{
		Zenith_Warning(LOG_CATEGORY_ASSET, "Zenith_FontAsset: atlas '%s' payload too small (%zu < %zu bytes)",
			strAtlasAbsPath.c_str(), static_cast<size_t>(xAtlasBytes.GetSize()), ulExpected);
		return Zenith_ErrorCode::CORRUPT_DATA;
	}

	Flux_SurfaceInfo xSurfaceInfo{};
	xSurfaceInfo.m_uWidth        = xAtlasInfo.m_uWidth;
	xSurfaceInfo.m_uHeight       = xAtlasInfo.m_uHeight;
	xSurfaceInfo.m_uDepth        = 1;
	xSurfaceInfo.m_uNumLayers    = 1;
	xSurfaceInfo.m_uNumMips      = 1;
	xSurfaceInfo.m_eFormat       = xAtlasInfo.m_eFormat;
	xSurfaceInfo.m_eTextureType  = TEXTURE_TYPE_2D;
	xSurfaceInfo.m_uMemoryFlags  = 1 << MEMORY_FLAGS__SHADER_READ;

	auto xhAtlas = Zenith_AssetRegistry::Create<Zenith_TextureAsset>();
	Zenith_TextureAsset* pxAtlas = xhAtlas.GetDirect();
	if (!pxAtlas)
	{
		Zenith_Warning(LOG_CATEGORY_ASSET, "Zenith_FontAsset: failed to create procedural atlas");
		return Zenith_ErrorCode::GPU_UPLOAD_FAILED;
	}

	// MSDF mips generated naïvely break the median reconstruction. Disable mips;
	// the fwidth-based AA shader handles minification correctly without them.
	const bool bCreated = pxAtlas->CreateFromData(xAtlasBytes.GetDataPointer(), xSurfaceInfo, /*bCreateMips=*/false);

	if (!bCreated)
	{
		Zenith_Warning(LOG_CATEGORY_ASSET, "Zenith_FontAsset: CreateFromData failed");
		return Zenith_ErrorCode::GPU_UPLOAD_FAILED;
	}

	m_xAtlasTexture.Set(pxAtlas);

	Zenith_Log(LOG_CATEGORY_ASSET,
		"Loaded font: %s (%u glyphs, atlas %.0fx%.0f, em=%.1fpx, range=%.1fpx)",
		strAbsPath.c_str(), uGlyphCount, m_fAtlasW, m_fAtlasH, m_fEmSize, m_fAtlasPxRange);

	return true;
}
