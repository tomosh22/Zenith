#pragma once

#include <string>
#include <cstdint>

namespace Zenith_Tools_FontExport
{
	// Export an MSDF font atlas + metrics sidecar from a TTF file.
	// - Generates a 256x256 RGBA8 MSDF atlas via msdf-atlas-gen + msdfgen + FreeType
	// - Charset: printable ASCII (codepoints 32..126; 95 glyphs incl. space)
	// - Writes the atlas as a .ztxtr at strAtlasPath and the per-glyph metrics
	//   as a .zfont at strZFontPath. The .zfont stores an engine-prefixed
	//   reference to the atlas; bake-time and runtime paths must agree.
	void ExportFromFile(const std::string& strTTFPath, const std::string& strZFontPath, const std::string& strAtlasPath);

	// As above with custom atlas pixel dims, em-px, and SDF range. Defaults are
	// 256/32/4 which are tuned for ASCII at typical UI sizes.
	void ExportFromFile(const std::string& strTTFPath, const std::string& strZFontPath, const std::string& strAtlasPath,
					   uint32_t uAtlasSize, float fPxPerEm, float fPxRange);
}

// Export the default engine font atlas (LiberationMono → engine assets).
void ExportDefaultFontAtlas();
