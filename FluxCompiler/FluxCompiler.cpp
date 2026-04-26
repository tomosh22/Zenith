#include "Zenith.h"
#pragma warning(disable: 4005) // APIENTRY macro redefinition (GLFW vs Windows SDK)
#include "Core/Memory/Zenith_MemoryManagement_Disabled.h"
#include "Flux/Slang/Flux_SlangCompiler.h"
#include "Flux/Slang/Flux_ShaderRegistry.h"
#include "DataStream/Zenith_DataStream.h"

#include "Flux/Slang/Flux_CodeGenerator.h"

#include <filesystem>
#include <fstream>
#include <vector>

// Stub functions for standalone FluxCompiler
const char* Project_GetGameAssetsDirectory() { return ""; }
const char* Project_GetName() { return "FluxCompiler"; }

#ifdef ZENITH_TOOLS
// Stub for editor logging - FluxCompiler doesn't have an editor console
void Zenith_EditorAddLogMessage(const char*, int, Zenith_LogCategory) {}
#endif

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

	// Make the shader source root available to ISession::loadModule so
	// registry entries can use module names like "Quads/Flux_TexturedQuad".
	Flux_SlangCompiler::AddSearchPath(SHADER_SOURCE_ROOT);

	// =====================================================================
	// PASS 0 — Slang program registry
	//
	// For every entry in the registry, compile the .slang module via the
	// modern session API, write per-stage SPIR-V and v2 reflection, capture
	// reflection for codegen, and then run codegen to refresh the C++
	// headers under Zenith/Flux/Shaders/Generated/. Failures here are
	// hard errors — registry entries must round-trip cleanly or the build
	// is broken.
	// =====================================================================
	u_int uRegistrySuccess = 0;
	u_int uRegistryFailure = 0;
	const u_int uRegistryCount = Flux_ShaderRegistry::GetProgramCount();

	std::vector<Flux_ShaderReflection>            axOwnedReflections(uRegistryCount);
	std::vector<Flux_CodeGenerator::ProgramReflection> axProgramRefl(uRegistryCount);

	for (u_int u = 0; u < uRegistryCount; u++)
	{
		const Flux_ShaderRegistryEntry& xEntry = Flux_ShaderRegistry::GetProgramByIndex(u);
		printf("Compiling (registry): %s [module=%s]\n", xEntry.m_szName, xEntry.m_szModuleName);

		Flux_SlangProgramDesc xDesc;
		Flux_ShaderRegistry::DescribeProgram(xEntry.m_eId, xDesc);

		Flux_SlangProgramResult xResult;
		if (!Flux_SlangCompiler::CompileProgram(xDesc, xResult))
		{
			printf("  -> FAILED: %s\n", xResult.m_strError.c_str());
			uRegistryFailure++;
			continue;
		}

		// Per-stage artifact emission. Stem is module + "." + entry so two
		// stages from the same module don't collide on disk.
		const std::string strRoot = SHADER_SOURCE_ROOT;
		auto fnEmitStage = [&](const Zenith_Vector<uint32_t>& axSpirv, const std::string& strStem)
		{
			if (axSpirv.GetSize() == 0) return;
			const std::string strSpvPath  = strRoot + strStem + ".spv";
			WriteSpirv(strSpvPath, axSpirv);
			WriteReflection(strSpvPath, xResult.m_xReflection);
		};

		if (xEntry.m_szVertexEntry)   fnEmitStage(xResult.m_axVertexSpirv,   Flux_ShaderRegistry::GetVertexArtifactStem(xEntry.m_eId));
		if (xEntry.m_szFragmentEntry) fnEmitStage(xResult.m_axFragmentSpirv, Flux_ShaderRegistry::GetFragmentArtifactStem(xEntry.m_eId));
		if (xEntry.m_szComputeEntry)  fnEmitStage(xResult.m_axComputeSpirv,  Flux_ShaderRegistry::GetComputeArtifactStem(xEntry.m_eId));

		axOwnedReflections[u] = xResult.m_xReflection;
		axProgramRefl[u].m_eId         = xEntry.m_eId;
		axProgramRefl[u].m_pxReflection = &axOwnedReflections[u];

		printf("  -> Success (%u bindings)\n", xResult.m_xReflection.GetBindings().GetSize());
		uRegistrySuccess++;
	}

	if (uRegistryFailure == 0 && uRegistryCount > 0)
	{
		std::string strGenDir = std::string(SHADER_SOURCE_ROOT) + "Generated/";
		std::filesystem::create_directories(strGenDir);
		if (Flux_CodeGenerator::WriteAllHeaders(strGenDir.c_str(),
												  axProgramRefl.data(),
												  static_cast<u_int>(axProgramRefl.size())))
		{
			printf("Codegen: refreshed %u program(s) into %s\n", uRegistrySuccess, strGenDir.c_str());
		}
		else
		{
			printf("Codegen: FAILED to write generated headers\n");
			uRegistryFailure++;
		}
	}

	printf("\n==========================================\n");
	printf("Compilation complete:\n");
	printf("  Slang registry: %u succeeded, %u failed\n", uRegistrySuccess, uRegistryFailure);

	Flux_SlangCompiler::Shutdown();

	return (uRegistryFailure > 0) ? 1 : 0;
}
