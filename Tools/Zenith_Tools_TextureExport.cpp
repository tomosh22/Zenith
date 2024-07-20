#include "Zenith.h"
#include "Flux/Flux.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

static std::string ShaderDataTypeToString(ShaderDataType eType)
{
	switch (eType)
	{
	case SHADER_DATA_TYPE_FLOAT:
		return "Float";
	case SHADER_DATA_TYPE_FLOAT2:
		return "Float2";
	case SHADER_DATA_TYPE_FLOAT3:
		return "Float3";
	case SHADER_DATA_TYPE_FLOAT4:
		return "Float4";
	case SHADER_DATA_TYPE_UINT4:
		return "UInt4";
	default:
		Zenith_Assert(false, "Unknown data type");
		return "";
	}
}

static void Export(std::string strFilename, const char* szExtension)
{
	int32_t iWidth, iHeight, iNumChannels;
	uint8_t* pData = stbi_load(strFilename.c_str(), &iWidth, &iHeight, &iNumChannels, STBI_rgb_alpha);

	size_t ulFindPos = strFilename.find(szExtension);
	strFilename.replace(ulFindPos, strlen(szExtension), "ztx");

	FILE* pxFile = fopen(strFilename.c_str(), "wb");
	char cNull = '\0';

	fputs(std::to_string(iWidth).c_str(), pxFile);
	fwrite(&cNull, 1, 1, pxFile);

	fputs(std::to_string(iHeight).c_str(), pxFile);
	fwrite(&cNull, 1, 1, pxFile);

	//#TO_TODO: 3d textures
	fputs(std::to_string(1u).c_str(), pxFile);
	fwrite(&cNull, 1, 1, pxFile);

	//#TO_TODO: other texture formats
	fputs("RGBA8", pxFile);
	fwrite(&cNull, 1, 1, pxFile);

	size_t ulDataSize = iWidth * iHeight * 1 /*depth*/ * 4 /*bytes per pixel*/;
	fwrite(pData, ulDataSize, 1, pxFile);

	fclose(pxFile);
}


void ExportAllTextures()
{
	for (auto& xFile : std::filesystem::recursive_directory_iterator(ASSETS_DIR))
	{
		const wchar_t* wszFilename = xFile.path().c_str();
		size_t ulLength = wcslen(wszFilename);
		char* szFilename = new char[ulLength + 1];
		wcstombs(szFilename, wszFilename, ulLength);
		szFilename[ulLength] = '\0';

		constexpr char* aszExtensions[] =
		{
			"png",
			"jpg",
			"jpeg"
		};
		for (const char* szExt : aszExtensions)
		{
			if (!strcmp(szFilename + strlen(szFilename) - strlen(szExt), szExt))
			{
				std::string strFilename(szFilename);
				Export(strFilename, szExt);
			}
		}
		
	}
}
