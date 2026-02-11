#include "Zenith.h"
#include "Flux/Flux_Types.h"

bool IsCompressedFormat(TextureFormat eFormat)
{
	return eFormat == TEXTURE_FORMAT_BC1_RGB_UNORM ||
		   eFormat == TEXTURE_FORMAT_BC1_RGBA_UNORM ||
		   eFormat == TEXTURE_FORMAT_BC3_RGBA_UNORM ||
		   eFormat == TEXTURE_FORMAT_BC5_RG_UNORM ||
		   eFormat == TEXTURE_FORMAT_BC7_RGBA_UNORM;
}

// Returns bytes per 4x4 block for compressed formats
uint32_t CompressedFormatBytesPerBlock(TextureFormat eFormat)
{
	switch (eFormat)
	{
	case TEXTURE_FORMAT_BC1_RGB_UNORM:
	case TEXTURE_FORMAT_BC1_RGBA_UNORM:
		return 8u;  // 8 bytes per 4x4 block
	case TEXTURE_FORMAT_BC3_RGBA_UNORM:
	case TEXTURE_FORMAT_BC5_RG_UNORM:
	case TEXTURE_FORMAT_BC7_RGBA_UNORM:
		return 16u; // 16 bytes per 4x4 block
	default:
		return 0u;
	}
}

// Calculate total size in bytes for a compressed texture
size_t CalculateCompressedTextureSize(TextureFormat eFormat, uint32_t uWidth, uint32_t uHeight)
{
	const uint32_t uBlocksX = (uWidth + 3) / 4;
	const uint32_t uBlocksY = (uHeight + 3) / 4;
	return static_cast<size_t>(uBlocksX) * uBlocksY * CompressedFormatBytesPerBlock(eFormat);
}

uint32_t ColourFormatBitsPerPixel(TextureFormat eFormat)
{
	switch (eFormat)
	{
	case TEXTURE_FORMAT_RGBA8_UNORM:
		return 32u;
	case TEXTURE_FORMAT_BGRA8_SRGB:
		return 32u;
	case TEXTURE_FORMAT_BGRA8_UNORM:
		return 32u;
	case TEXTURE_FORMAT_R16G16B16A16_SFLOAT:
		return 64u;
	case TEXTURE_FORMAT_R16G16B16A16_UNORM:
		return 64u;
	case TEXTURE_FORMAT_R32G32B32A32_SFLOAT:
		return 128u;
	case TEXTURE_FORMAT_R32G32B32_SFLOAT:
		return 96u;
	case TEXTURE_FORMAT_R16_UNORM:
		return 16u;
	case TEXTURE_FORMAT_R32_SFLOAT:
		return 32u;
	case TEXTURE_FORMAT_R16G16_SFLOAT:
		return 32u;
	case TEXTURE_FORMAT_R32G32_SFLOAT:
		return 64u;
	default:
		Zenith_Assert(false, "Unrecognised colour format");
		return 0u;
	}
}

uint32_t ColourFormatBytesPerPixel(TextureFormat eFormat)
{
	return ColourFormatBitsPerPixel(eFormat) / 8u;
}

uint32_t DepthStencilFormatBitsPerPixel(TextureFormat eFormat)
{
	switch (eFormat)
	{
	case TEXTURE_FORMAT_D32_SFLOAT:
		return 32;
	default:
		Zenith_Assert(false, "Unrecognised depth/stencil format");
		return 0u;
	}
}
