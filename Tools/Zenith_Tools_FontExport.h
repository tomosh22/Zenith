#pragma once

#include <string>
#include <cstdint>

namespace Zenith_Tools_FontExport
{
	// Export font atlas from TTF file
	// - Generates 512x512 RGBA texture with 10x10 character grid
	// - Characters 32-131 (ASCII printable range)
	// - White text with black shadow baked in
	void ExportFromFile(const std::string& strTTFPath, const std::string& strOutputPath);

	// Export with custom parameters
	void ExportFromFile(const std::string& strTTFPath, const std::string& strOutputPath,
					   uint32_t uAtlasSize, float fFontSize, float fShadowOffset);
}

// Export the default engine font atlas
void ExportDefaultFontAtlas();
