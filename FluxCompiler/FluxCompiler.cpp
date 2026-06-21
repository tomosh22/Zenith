#include "Zenith.h"
#pragma warning(disable: 4005) // APIENTRY macro redefinition (GLFW vs Windows SDK)
#include "Core/Memory/Zenith_MemoryManagement_Disabled.h"
#include "Flux/Slang/Flux_SlangCompiler.h"
#include "Flux/Slang/Flux_ShaderCatalog.h"
#include "DataStream/Zenith_DataStream.h"

#include "Flux/Slang/Flux_CodeGenerator.h"
#include "Flux/Flux_FeatureRegistry.h"    // CreateDefaultSnapshotForValidation + parity
#include "Core/Zenith_GraphicsOptions.h" // Project_SetGraphicsOptions stub signature

#include <filesystem>
#include <fstream>
#include <vector>
#include <set>
#include <algorithm>

// Stub functions for standalone FluxCompiler
const char* Project_GetGameAssetsDirectory() { return ""; }
const char* Project_GetName() { return "FluxCompiler"; }

// FluxCompiler links zenith.lib but never boots the engine, so the game-project
// hooks Zenith_Engine references must be satisfied with empty stubs (mirrors the
// no-op editor-log stub below). Without these the tool fails to LINK — a
// pre-existing breakage (stale stub set) independent of the shader migration.
void Project_SetGraphicsOptions(Zenith_GraphicsOptions&) {}
void Project_RegisterGameComponents() {}
void Project_Shutdown() {}
// Defined unconditionally: FluxCompiler.cpp is compiled WITHOUT ZENITH_TOOLS, but
// it links the tools-enabled zenith.lib, whose Zenith_Engine::InitialiseProject
// references these two editor-only hooks. (In _False builds they go unreferenced.)
void Project_InitializeResources() {}
void Project_RegisterEditorAutomationSteps() {}
void Project_LoadInitialScene() {} // referenced by the non-tools InitialiseProject branch

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

// Delete generated artifacts (.spv / .spv.refl under the shader root, and
// Generated/<Subsystem>.h) that the current catalog no longer produces — e.g.
// a deleted/renamed program's stale SPIR-V or a removed subsystem's header.
// Canonical-Tools-build only (gated by the caller); never runs when a compile
// failed (caller checks uRegistryFailure==0) so every live program has fresh
// artifacts and nothing live is mistaken for stale.
static void PruneStaleArtifacts(const std::string& strRoot)
{
	namespace fs = std::filesystem;
	auto fnNorm = [](std::string s)
	{
		for (char& c : s) if (c == '\\') c = '/';
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)::tolower(c); });
		return s;
	};

	std::set<std::string> xExpectedArtifacts; // normalised .spv / .spv.refl paths
	std::set<std::string> xExpectedHeaders;   // <Subsystem>.h filenames (lowercased)

	const u_int uCount = Flux_ShaderCatalog::GetProgramCount();
	for (u_int u = 0; u < uCount; u++)
	{
		const Flux_ShaderDecl& xDecl = Flux_ShaderCatalog::GetProgramByIndex(u);
		auto fnAdd = [&](const std::string& strStem)
		{
			xExpectedArtifacts.insert(fnNorm(strRoot + strStem + ".spv"));
			xExpectedArtifacts.insert(fnNorm(strRoot + strStem + ".spv.refl"));
		};
		if (xDecl.m_szVertexEntry)   fnAdd(Flux_ShaderCatalog::GetVertexArtifactStem(xDecl));
		if (xDecl.m_szFragmentEntry) fnAdd(Flux_ShaderCatalog::GetFragmentArtifactStem(xDecl));
		if (xDecl.m_szComputeEntry)  fnAdd(Flux_ShaderCatalog::GetComputeArtifactStem(xDecl));
		if (xDecl.m_szSubsystem)     xExpectedHeaders.insert(fnNorm(std::string(xDecl.m_szSubsystem) + ".h"));
	}

	u_int uPruned = 0;

	// Stale .spv / .spv.refl anywhere under the shader root.
	for (const auto& xEntry : fs::recursive_directory_iterator(strRoot))
	{
		if (!xEntry.is_regular_file()) continue;
		const std::string strPath = fnNorm(xEntry.path().string());
		const bool bRefl = strPath.size() >= 9 && strPath.compare(strPath.size() - 9, 9, ".spv.refl") == 0;
		const bool bSpv  = !bRefl && strPath.size() >= 4 && strPath.compare(strPath.size() - 4, 4, ".spv") == 0;
		if (!bSpv && !bRefl) continue;
		if (xExpectedArtifacts.find(strPath) == xExpectedArtifacts.end())
		{
			std::error_code ec; fs::remove(xEntry.path(), ec);
			printf("Prune: removed stale artifact %s\n", xEntry.path().string().c_str());
			uPruned++;
		}
	}

	// Stale Generated/<Subsystem>.h.
	const std::string strGen = strRoot + "Generated/";
	if (fs::exists(strGen))
	{
		for (const auto& xEntry : fs::directory_iterator(strGen))
		{
			if (!xEntry.is_regular_file()) continue;
			const std::string strFile = xEntry.path().filename().string();
			if (strFile.size() < 2 || strFile.compare(strFile.size() - 2, 2, ".h") != 0) continue;
			if (xExpectedHeaders.find(fnNorm(strFile)) == xExpectedHeaders.end())
			{
				std::error_code ec; fs::remove(xEntry.path(), ec);
				printf("Prune: removed stale header Generated/%s\n", strFile.c_str());
				uPruned++;
			}
		}
	}

	printf("Prune: %u stale generated file(s) removed\n", uPruned);
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
	// Validate the catalog + its parity against the feature set BEFORE any
	// compile. CreateDefaultSnapshotForValidation builds a feature-registry
	// snapshot WITHOUT booting the engine (the RegisterFeature trampolines only
	// reach g_xEngine when called, never during registration). Abort on failure —
	// this is the build-time guard for "forgot a catalog include / RegisterFeature
	// line". Same single source list as the engine, so no drift.
	// =====================================================================
	{
		std::string strErr;
		if (!Flux_ShaderCatalog::Validate(strErr))
		{
			printf("ERROR: shader catalog invalid: %s\n", strErr.c_str());
			return 1;
		}
		const Flux_FeatureRegistry xSnapshot = Flux_FeatureRegistry::CreateDefaultSnapshotForValidation();
		if (!Flux_ShaderCatalog::ValidateFeatureParity(xSnapshot, strErr))
		{
			printf("ERROR: shader catalog/feature parity failed: %s\n", strErr.c_str());
			return 1;
		}
		printf("Catalog validated: %u programs, feature parity OK\n\n", Flux_ShaderCatalog::GetProgramCount());
	}

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
	const u_int uRegistryCount = Flux_ShaderCatalog::GetProgramCount();

	std::vector<Flux_ShaderReflection>            axOwnedReflections(uRegistryCount);
	std::vector<Flux_CodeGenerator::ProgramReflection> axProgramRefl(uRegistryCount);

	for (u_int u = 0; u < uRegistryCount; u++)
	{
		const Flux_ShaderDecl& xEntry = Flux_ShaderCatalog::GetProgramByIndex(u);
		printf("Compiling (registry): %s [module=%s]\n", xEntry.m_szName, xEntry.m_szModuleName);

		Flux_SlangProgramDesc xDesc;
		Flux_ShaderCatalog::DescribeProgram(xEntry, xDesc);

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

		if (xEntry.m_szVertexEntry)   fnEmitStage(xResult.m_axVertexSpirv,   Flux_ShaderCatalog::GetVertexArtifactStem(xEntry));
		if (xEntry.m_szFragmentEntry) fnEmitStage(xResult.m_axFragmentSpirv, Flux_ShaderCatalog::GetFragmentArtifactStem(xEntry));
		if (xEntry.m_szComputeEntry)  fnEmitStage(xResult.m_axComputeSpirv,  Flux_ShaderCatalog::GetComputeArtifactStem(xEntry));

		axOwnedReflections[u] = xResult.m_xReflection;
		axProgramRefl[u].m_pxDecl       = &xEntry;
		axProgramRefl[u].m_pxReflection = &axOwnedReflections[u];

		printf("  -> Success (%u bindings)\n", xResult.m_xReflection.GetBindings().GetSize());
		uRegistrySuccess++;
	}

	// Codegen + the destructive prune run ONLY in the canonical Tools=True
	// FluxCompiler (the full shader set). A Tools=False FluxCompiler compiles its
	// reduced set above but must not regenerate headers from the reduced catalog
	// nor delete tools-only artifacts.
	const bool bCanonical = Flux_ShaderCatalog::IsCanonicalToolsBuild();
	if (uRegistryFailure == 0 && uRegistryCount > 0 && bCanonical)
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

		// Remove artifacts/headers the current catalog no longer produces.
		if (uRegistryFailure == 0)
		{
			PruneStaleArtifacts(std::string(SHADER_SOURCE_ROOT));
		}
	}
	else if (!bCanonical)
	{
		printf("Codegen + prune SKIPPED (non-canonical / Tools=False build — reduced shader set)\n");
	}

	printf("\n==========================================\n");
	printf("Compilation complete:\n");
	printf("  Slang registry: %u succeeded, %u failed\n", uRegistrySuccess, uRegistryFailure);

	Flux_SlangCompiler::Shutdown();

	return (uRegistryFailure > 0) ? 1 : 0;
}
