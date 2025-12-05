#include "Flux/Flux_Enums.h"

// Texture compression options for export
enum class TextureCompressionMode
{
	Uncompressed,	// RGBA8 uncompressed (default, backwards compatible)
	BC1,			// DXT1 - RGB, no alpha (6:1 compression)
	BC1_Alpha,		// DXT1 - RGB + 1-bit alpha (6:1 compression)
	BC3,			// DXT5 - RGB + smooth alpha (4:1 compression)
	BC5				// 3Dc/ATI2 - RG only, ideal for normal maps (4:1 compression)
};

namespace Zenith_Tools_TextureExport
{
	// Export texture with specified compression mode
	void ExportFromFile(std::string strFilename, const char* szExtension, TextureCompressionMode eCompression = TextureCompressionMode::Uncompressed);
	
	// Export raw texture data (uncompressed only - for procedural textures)
	void ExportFromData(const void* pData, const std::string& strFilename, int32_t iWidth, int32_t iHeight, TextureFormat eFormat);
	
	// Export raw texture data with compression
	void ExportFromDataCompressed(const void* pRGBAData, const std::string& strFilename, int32_t iWidth, int32_t iHeight, TextureCompressionMode eCompression);
	
	// Helper to determine if a format is BC compressed
	bool IsCompressedFormat(TextureFormat eFormat);
	
	// Get bytes per block for compressed formats (returns bytes per pixel for uncompressed)
	uint32_t GetBytesPerBlockOrPixel(TextureFormat eFormat);
}