#include "Zenith.h"
#include "Core/Memory/Zenith_MemoryManagement_Disabled.h"
#include "Flux/Slang/Flux_SlangCompiler.h"

#include <filesystem>
#include <fstream>

// Stub functions for standalone FluxCompiler
const char* Project_GetGameAssetsDirectory() { return ""; }
const char* Project_GetName() { return "FluxCompiler"; }

#ifdef ZENITH_TOOLS
// Stub for editor logging - FluxCompiler doesn't have an editor console
void Zenith_EditorAddLogMessage(const char*, int, Zenith_LogCategory) {}
#endif

static SlangShaderStage GetShaderStage(const std::string& strExtension)
{
	if (strExtension == ".vert")
	{
		return SLANG_SHADER_STAGE_VERTEX;
	}
	else if (strExtension == ".frag")
	{
		return SLANG_SHADER_STAGE_FRAGMENT;
	}
	else if (strExtension == ".comp")
	{
		return SLANG_SHADER_STAGE_COMPUTE;
	}
	else if (strExtension == ".tesc")
	{
		return SLANG_SHADER_STAGE_TESSELLATION_CONTROL;
	}
	else if (strExtension == ".tese")
	{
		return SLANG_SHADER_STAGE_TESSELLATION_EVALUATION;
	}
	else if (strExtension == ".geom")
	{
		return SLANG_SHADER_STAGE_GEOMETRY;
	}
	return SLANG_SHADER_STAGE_VERTEX;
}

static void WriteSpirv(const std::string& strPath, const Zenith_Vector<uint32_t>& axSpirv)
{
	std::ofstream xFile(strPath, std::ios::binary);
	if (xFile.is_open())
	{
		xFile.write(reinterpret_cast<const char*>(axSpirv.GetDataPointer()), axSpirv.GetSize() * sizeof(uint32_t));
		xFile.close();
	}
}

int main()
{
	printf("FluxCompiler - Slang-based Shader Compiler\n");
	printf("==========================================\n\n");
	fflush(stdout);

	printf("Initializing Slang compiler...\n");
	fflush(stdout);

	Flux_SlangCompiler::Initialise();

	if (!Flux_SlangCompiler::IsInitialised())
	{
		printf("ERROR: Failed to initialize Slang compiler\n");
		return 1;
	}

	static const char* aszShaderExtensions[] =
	{
		".vert",
		".frag",
		".comp",
		".tesc",
		".tese",
		".geom"
	};

	u_int uSuccessCount = 0;
	u_int uFailCount = 0;

	for (auto& xFile : std::filesystem::recursive_directory_iterator(SHADER_SOURCE_ROOT))
	{
		if (!xFile.is_regular_file())
		{
			continue;
		}

		std::string strExtension = xFile.path().extension().string();

		bool bIsShader = false;
		for (const char* szExt : aszShaderExtensions)
		{
			if (strExtension == szExt)
			{
				bIsShader = true;
				break;
			}
		}

		if (!bIsShader)
		{
			continue;
		}

		std::string strPath = xFile.path().string();
		std::string strOutputPath = strPath + ".spv";

		printf("Compiling: %s\n", strPath.c_str());

		SlangShaderStage eStage = GetShaderStage(strExtension);
		Flux_SlangCompileResult xResult;

		if (Flux_SlangCompiler::Compile(strPath, eStage, xResult))
		{
			// NOTE: SPV writing disabled - Slang optimizes out unused varyings
			// causing vertex/fragment interface mismatches.
			// Keeping glslc-compiled .spv files until Phase 5 (native Slang conversion).
			// Slang is still used for reflection data extraction.
			// WriteSpirv(strOutputPath, xResult.m_axSpirv);
			printf("  -> Success (%u bytes, %u bindings)\n",
				   static_cast<u_int>(xResult.m_axSpirv.GetSize() * sizeof(uint32_t)),
				   xResult.m_xReflection.GetBindings().GetSize());
			uSuccessCount++;
		}
		else
		{
			printf("  -> FAILED: %s\n", xResult.m_strError.c_str());
			uFailCount++;
		}
	}

	printf("\n==========================================\n");
	printf("Compilation complete: %u succeeded, %u failed\n", uSuccessCount, uFailCount);

	Flux_SlangCompiler::Shutdown();

	return (uFailCount > 0) ? 1 : 0;
}
