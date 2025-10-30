#include "Zenith.h"

#include <filesystem>

int main()
{
	static const char* aszShaderExtensions[] =
	{
		"vert",
		"frag",
		"comp"
	};

	for (auto& xFile : std::filesystem::recursive_directory_iterator(SHADER_SOURCE_ROOT))
	{
		const wchar_t* wszFilename = xFile.path().c_str();
		size_t ulLength = wcslen(wszFilename);
		char* szFilename = new char[ulLength + 1];
		wcstombs(szFilename, wszFilename, ulLength);
		szFilename[ulLength] = '\0';
		for (const char* szExtension : aszShaderExtensions)
		{
			if (strstr((const char*)xFile.path().extension().generic_u8string().c_str(), szExtension))
			{
				std::string strCommand = "%VULKAN_SDK%/Bin/glslc.exe " + std::string(szFilename) + " -g --target-env=vulkan1.3 -I" + SHADER_SOURCE_ROOT + " -o " + std::string(szFilename) + ".spv";
				system(strCommand.c_str());
			}
		}
	}
	__debugbreak();
}