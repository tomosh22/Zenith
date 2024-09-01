#include "Flux/Flux_Enums.h"
namespace Zenith_Tools_TextureExport
{
	void ExportFromFile(std::string strFilename, const char* szExtension);
	void ExportFromData(const void* pData, const std::string& strFilename, int32_t iWidth, int32_t iHeight, ColourFormat eFormat);
}