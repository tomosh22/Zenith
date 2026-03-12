#include "Zenith.h"
#pragma warning(disable: 4005) // APIENTRY macro redefinition (GLFW vs Windows SDK)
#include "Core/Memory/Zenith_MemoryManagement_Disabled.h"
#include "Flux/Slang/Flux_SlangCompiler.h"
#include "DataStream/Zenith_DataStream.h"

#include <filesystem>
#include <fstream>
#include <unordered_set>

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

static void WriteReflection(const std::string& strSpvPath, const Flux_ShaderReflection& xReflection)
{
	std::string strReflPath = strSpvPath + ".refl";
	Zenith_DataStream xReflStream;
	xReflection.WriteToDataStream(xReflStream);
	xReflStream.WriteToFile(strReflPath.c_str());
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

	// Collect all shader files first, so we can pair vert+frag for combined compilation
	// Paired compilation ensures Slang preserves GLSL set/binding qualifiers correctly
	std::vector<std::filesystem::path> axShaderFiles;
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

		if (bIsShader)
		{
			axShaderFiles.push_back(xFile.path());
		}
	}

	// Track which files have been compiled as part of a pair
	std::unordered_set<std::string> xCompiledPaths;

	// First pass: compile vert+frag pairs together
	for (const auto& xPath : axShaderFiles)
	{
		std::string strExtension = xPath.extension().string();
		if (strExtension != ".vert")
		{
			continue;
		}

		// Look for matching .frag file
		std::filesystem::path xFragPath = xPath;
		xFragPath.replace_extension(".frag");

		if (!std::filesystem::exists(xFragPath))
		{
			continue;
		}

		std::string strVertPath = xPath.string();
		std::string strFragPath = xFragPath.string();

		printf("Compiling (paired): %s + %s\n", strVertPath.c_str(), strFragPath.c_str());

		Flux_SlangGraphicsPipelineResult xResult;
		if (Flux_SlangCompiler::CompileGraphicsPipeline(strVertPath, strFragPath, xResult))
		{
			// Write vertex SPIR-V and reflection
			std::string strVertSpvPath = strVertPath + ".spv";
			WriteSpirv(strVertSpvPath, xResult.m_axVertexSpirv);
			WriteReflection(strVertSpvPath, xResult.m_xVertexReflection);

			// Write fragment SPIR-V and reflection
			std::string strFragSpvPath = strFragPath + ".spv";
			WriteSpirv(strFragSpvPath, xResult.m_axFragmentSpirv);
			WriteReflection(strFragSpvPath, xResult.m_xFragmentReflection);

			printf("  -> Success (vert: %u bytes, frag: %u bytes, %u bindings)\n",
				   static_cast<u_int>(xResult.m_axVertexSpirv.GetSize() * sizeof(uint32_t)),
				   static_cast<u_int>(xResult.m_axFragmentSpirv.GetSize() * sizeof(uint32_t)),
				   xResult.m_xVertexReflection.GetBindings().GetSize());
			uSuccessCount += 2;

			xCompiledPaths.insert(strVertPath);
			xCompiledPaths.insert(strFragPath);
		}
		else
		{
			printf("  -> FAILED: %s\n", xResult.m_strError.c_str());
			uFailCount += 2;

			xCompiledPaths.insert(strVertPath);
			xCompiledPaths.insert(strFragPath);
		}
	}

	// Second pass: compile remaining shaders individually (compute, unpaired vert/frag, tesc, tese, geom)
	for (const auto& xPath : axShaderFiles)
	{
		std::string strPath = xPath.string();
		if (xCompiledPaths.count(strPath))
		{
			continue;
		}

		std::string strExtension = xPath.extension().string();
		std::string strOutputPath = strPath + ".spv";

		printf("Compiling: %s\n", strPath.c_str());

		SlangShaderStage eStage = GetShaderStage(strExtension);
		Flux_SlangCompileResult xResult;

		if (Flux_SlangCompiler::Compile(strPath, eStage, xResult))
		{
			WriteSpirv(strOutputPath, xResult.m_axSpirv);
			WriteReflection(strOutputPath, xResult.m_xReflection);

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
