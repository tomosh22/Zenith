#pragma once
#include "Flux/Flux_Enums.h"
#include <string>
#include <utility>
#include <vector>

// Texture compression options for export
enum class TextureCompressionMode
{
	Uncompressed,	// RGBA8 uncompressed
	BC1,			// DXT1 - RGB, no alpha (6:1 compression)
	BC1_Alpha,		// DXT1 - RGB + 1-bit punch-through alpha (6:1 compression)
	BC3,			// DXT5 - RGB + smooth alpha (4:1 compression)
	BC5				// 3Dc/ATI2 - RG only, ideal for normal maps (4:1 compression)
};

// What the bytes MEAN, which decides two things the format alone used to hide:
// the on-disk format's sRGB twin (BC1_RGB_SRGB rather than BC1_RGB_UNORM, so the
// sampler decodes the EOTF in hardware and the shader receives linear), and the
// mip filter (an sRGB chain is decoded to linear, averaged, and re-encoded; a
// byte average of sRGB darkens every level). Every displayed colour -- base
// colour, emissive, UI icon -- is SRGB. Every data map -- normal, roughness,
// metallic, occlusion, height, mask -- is Linear and is sampled raw.
enum class TextureColourSpace
{
	Linear,
	SRGB
};

namespace Zenith_Tools_TextureExport
{
	// What a source texture's bytes MEAN. This is the ONE thing a bare directory
	// walk cannot work out for itself, so it is DECLARED per texture rather than
	// guessed -- see TextureUsageManifest below.
	//
	// It used to be inferred from the filename ("normal" in the basename -> BC5).
	// That is why Dawnmere lost every terrain shadow: the inference correctly
	// turned the terrain normal maps into BC5, the terrain shaders still decoded
	// three channels, blue read 0, the shading normal pointed into the ground and
	// NdotL went to zero. A guess that is right about the bytes and silent about
	// everything downstream is worse than no guess -- nothing failed, and the
	// symptom surfaced days later as "the houses stopped casting shadows".
	enum class TextureUsage
	{
		BaseColour,          // displayed colour        -> BC1 sRGB (BC3 if the source carries alpha)
		BaseColourMasked,    // + punch-through alpha   -> BC1_Alpha sRGB
		NormalMap,           // tangent-space normal    -> BC5 linear, Z reconstructed at sample time
		LinearData,          // roughness/metal/AO/...  -> BC1 linear
		UncompressedColour,  // atlas / LUT that must survive exactly, displayed
		UncompressedData     // ...and the linear twin
	};

	// The ONE mapping from a declared usage to the export pair. Everything else
	// reads this; there is no second place a usage becomes a format.
	void ResolveUsage(TextureUsage eUsage, TextureCompressionMode& eCompressionOut, TextureColourSpace& eColourSpaceOut);
	const char* TextureUsageToken(TextureUsage eUsage);
	bool TextureUsageFromToken(const std::string& strToken, TextureUsage& eUsageOut);

	// The declarations for ONE walked asset root, parsed from
	// `<root>/TextureUsage.ztexdecl`. Line-based and human-diffable:
	//
	//     # comment
	//     Textures/Terrain/Grass/normal.jpg    NORMAL_MAP
	//
	// The path is relative to the root, forward-slashed, matched case-insensitively
	// (Windows source trees are not case-stable). The file is COMMITTED even though
	// `**/Assets/**` is gitignored -- `.ztexdecl` is re-included the same way
	// `.zscen` and `.znavmesh` are, because a declaration that a fresh clone lacks
	// is a declaration that gets re-guessed.
	class TextureUsageManifest
	{
	public:
		// Parses `strText`. Returns false and fills m_strError on a malformed line
		// (unknown usage token, missing token, duplicate path) -- there is no
		// tolerant mode, because every tolerance here is a silent wrong export.
		bool ParseText(const std::string& strText);
		// Loads `<strRootDir>/TextureUsage.ztexdecl`. Missing file -> false.
		bool LoadForRoot(const std::string& strRootDir);

		// Declared usage for a root-relative path, or false when undeclared.
		bool TryGetUsage(const std::string& strRelativePath, TextureUsage& eUsageOut) const;

		size_t GetCount() const { return m_xEntries.size(); }
		const std::string& GetError() const { return m_strError; }
		// Every declared path, in declaration order -- lets the walk report an
		// entry whose file has since been renamed or deleted.
		const std::vector<std::pair<std::string, TextureUsage>>& GetEntries() const { return m_xEntries; }

		// The filename the manifest is expected under, inside a walked root.
		static const char* Filename() { return "TextureUsage.ztexdecl"; }

	private:
		std::vector<std::pair<std::string, TextureUsage>> m_xEntries;
		std::string m_strError;
	};

	// The ONE mapping from (compression, colour space) to an on-disk TextureFormat.
	// BC5 is two-channel data and has no sRGB twin: it stays UNORM whatever colour
	// space is asked for.
	TextureFormat ResolveExportFormat(TextureCompressionMode eCompression, TextureColourSpace eColourSpace);

	// Export texture with specified compression mode (PNG, JPG, JPEG). A source
	// with an alpha channel upgrades BC1 to BC3 (same colour space).
	void ExportFromFile(std::string strFilename, const char* szExtension,
		TextureCompressionMode eCompression = TextureCompressionMode::Uncompressed,
		TextureColourSpace eColourSpace = TextureColourSpace::Linear);

	// Export an image preserving bit depth (16-bit/32-bit float, via stb).
	// Single-channel sources stay single-channel (R16_UNORM / R32_SFLOAT) for
	// heightmaps; multi-channel sources export as RGBA8. Heightmaps are data, so
	// the whole path is Linear.
	void ExportFromHeightmapImageFile(const std::string& strFilename, TextureCompressionMode eCompression = TextureCompressionMode::Uncompressed);

	// Export raw RGBA8 data as ONE level, uncompressed. For textures that must NOT
	// carry a mip chain: the MSDF font atlas (naive downsampling breaks the median
	// reconstruction the text shader depends on) and point-sampled tables.
	void ExportFromData(const void* pData, const std::string& strFilename, int32_t iWidth, int32_t iHeight, TextureFormat eFormat);

	// Export raw texture data as ONE level with an explicit format and bytes per
	// pixel. For non-RGBA8 payloads the mip builder cannot filter (R16_UNORM /
	// R32_SFLOAT heightmaps).
	void ExportFromDataWithFormat(const void* pData, const std::string& strFilename, int32_t iWidth, int32_t iHeight, TextureFormat eFormat, size_t ulBytesPerPixel);

	// Export raw RGBA8 data BC-compressed with a full offline mip chain.
	// fAlphaCoverageCutoff > 0 opts into coverage-preserving alpha mips (Castano):
	// each level's alpha is scaled so the fraction of texels above the cutoff
	// matches mip 0 -- pass the material's alpha cutoff for MASKED foliage, or a
	// distant tree loses its leaves as the box filter averages its alpha away.
	void ExportFromDataCompressed(const void* pRGBAData, const std::string& strFilename, int32_t iWidth, int32_t iHeight,
		TextureCompressionMode eCompression,
		TextureColourSpace eColourSpace = TextureColourSpace::Linear,
		float fAlphaCoverageCutoff = 0.0f);

	// Export raw RGBA8 data as a .ztxtr v2 with a full offline-baked mip chain but
	// NO BC compression, keeping an explicit uncompressed format (RGBA8_SRGB /
	// RGBA8_UNORM). The colour space follows the format (IsSRGBFormat), so an
	// RGBA8_SRGB chain is filtered in linear. For procedural maps whose texel
	// values must survive exactly (a packed RM map the terrain reads .gb from, a
	// leaf mask whose 8-bit alpha edge BC3 would quantise).
	void ExportFromDataV2Uncompressed(const void* pRGBAData, const std::string& strFilename, int32_t iWidth, int32_t iHeight,
		TextureFormat eFormat, float fAlphaCoverageCutoff = 0.0f);

	// Helper to determine if a format is BC compressed
	bool IsCompressedFormat(TextureFormat eFormat);

	// Get bytes per block for compressed formats (returns bytes per pixel for uncompressed)
	uint32_t GetBytesPerBlockOrPixel(TextureFormat eFormat);
}
