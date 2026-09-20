using Sharpmake;
using System;
using System.IO;

// ZenithTools — leaf executable-support static library (Phase 4g — see
// Sharpmake_ZenithCore.cs for the pattern this file replicates, and
// Sharpmake_ZenithEditor.cs for this phase's other new library and the
// closest-shaped sibling: both are win64 x ToolsEnabled.True-only leaves).
// Holds exactly the repo-root Tools/ directory — 17 translation units
// (Zenith_Tools_AnimMigrate.cpp, …AssimpConvert.cpp, …BushAssetExport.cpp,
// …FallenTreeAssetExport.cpp, …FontExport.cpp, …GlbImport.cpp,
// …GltfExport.cpp, …GrassAssetExport.cpp, …HumanModelExport.cpp,
// …HumanSkinBind.cpp, …MeshExport.cpp, …MeshoptDecode.cpp,
// …RockAssetExport.cpp, …TerrainExport.cpp, …TestAssetExport.cpp,
// …TextureExport.cpp, …TreeAssetExport.cpp — see Tools/CLAUDE.md for what
// each one does) + the definition-free PCH create TU
// (Internal/Zenith_ToolsPCH.cpp). 17 + 1 = 18 compiled TUs.
//
// ═══════════════════════════════════════════════════════════════════════
// ADOPTING THE ORPHAN — WHAT THIS PHASE CHANGES ABOUT THIS FILE.
// ═══════════════════════════════════════════════════════════════════════
// ZenithToolsProject has existed since before this phase but was added to NO
// solution — `check_project_membership.ps1` has reported it as the repo's one
// orphan project (plus 7 orphan-duplicate findings) since Phase 0, because
// nothing ever compiled it. That also means its ConfigureAll was never
// actually validated end to end: it called neither ConfigureCommonSettings
// nor ConfigureCommonIncludePaths (missing the render-backend define
// ZENITH_VULKAN/D3D12/NULL that Zenith_BackendAliases.h #errors without, the
// C++20 standard option, warnings-as-errors, ZENITH_PROFILING_ENABLED,
// ZENITH_MEMORY_TRACKING_LEVEL — everything every other leaf gets from the
// two shared helpers), had no PCH, no distinct IntermediatePath, and no
// exclude for Tools/Middleware/freetype or Tools/Middleware/msdf-atlas-gen
// (221 + 47 vendored .cpp/.c that would have compiled a SECOND time into this
// project — FreeTypeProject/MsdfgenProject/MsdfAtlasGenProject already build
// them as separate Sharpmake projects linked as prebuilt .libs — an ODR
// violation that was silently dormant only because this project never built)
// or Tools/Middleware/assimp/include/stb_vorbis.c (already linked inside
// assimp-vc143-mt[d].lib, so recompiling it here would duplicate-symbol at
// link time). All of that is fixed below, rewriting this file onto the same
// ConfigureCommonSettings/ConfigureCommonIncludePaths foundation every other
// leaf uses instead of the ad hoc partial config it had.
//
// Fixing the orphan half of the bug needs two changes beyond this file:
//   * Drop this project's own MeshGeometry AdditionalSourceRootPaths (this
//     project now depends on ZenithFluxLibProject, which owns
//     Zenith/Flux/MeshGeometry whole, instead of compiling a second copy of
//     Flux_MeshGeometry.cpp / Flux_MeshInstance.cpp here).
//   * The aggregate (Sharpmake_Zenith.cs) drops its own
//     AdditionalSourceRootPaths.Add(RootPath + "/Tools") entirely, so this
//     project becomes the SOLE owner of the repo-root Tools tree instead of
//     the two of them compiling the same 17 files in the same ToolsEnabled.True
//     win64 configuration.
//
// ═══════════════════════════════════════════════════════════════════════
// WIN64 x TOOLS=TRUE ONLY.
// ═══════════════════════════════════════════════════════════════════════
// This project declares exactly ONE target set: Platform.win64 x
// ToolsEnabled.True (all three RenderBackend fragments, both Optimizations).
// No agde AddTargets call, and ToolsEnabled carries only True (previously
// True | False, which was never reachable either way since the project was
// never added to a solution). `conf.AddProject` / `conf.AddPublicDependency`
// THROW at Sharpmake-run time for a target the callee itself never declares,
// so every call site that adds this project (via ZenithEngineLibs.All) must
// guard with the same predicate ZenithEditor uses:
// `t => t.Platform == Platform.win64 && t.ToolsEnabled == ToolsEnabled.True`.
// No TU here #includes Flux_GraphicsImpl.h or a Pipeline header DIRECTLY —
// but several reach RenderBackends/Null/Zenith_Null_Pipeline.h
// TRANSITIVELY through Flux/MeshGeometry headers, and that header
// references SHADER_SOURCE_ROOT unguarded (see the "SHADER_SOURCE_ROOT"
// paragraph in ConfigureAll below — a grep-only check on the direct
// #include graph is not sufficient, exactly as this phase's brief warned).
// All three RenderBackend fragments are declared uniformly regardless
// (Tools code itself is backend-neutral; the define need is a header-DAG
// accident, not a genuine backend dependency).
//
// THE PCH: >= 4 compiled TUs justifies a binary PCH under this refactor's
// rule. ZenithTools CREATEs its own Zenith.pch from
// Internal/Zenith_ToolsPCH.cpp (a one-line `#include "Zenith.h"`, nothing
// else — mirrors Zenith/Editor/Internal/Zenith_EditorPCH.cpp; it MUST stay
// definition-free, see that file's own header comment for why). Every real
// TU here begins with `#include "Zenith.h"` (Zenith_Tools_MeshExport.cpp has
// an `#ifndef NOMINMAX` guard ahead of it, but MSVC's /Yu ignores any text
// preceding the precompiled-header include, and this exact file already
// compiled successfully under the aggregate's own PCH before this phase —
// verified unaffected).
//
// MESHGEOMETRY — NOW A DEPENDENCY, NOT A SECOND COPY: 6 files here
// (…BushAssetExport.cpp, …FallenTreeAssetExport.cpp, …RockAssetExport.cpp,
// …TerrainExport.cpp, …TestAssetExport.cpp/.h, …TreeAssetExport.cpp)
// #include Flux/MeshGeometry/** headers. ZenithFluxLibProject
// (Sharpmake_ZenithFlux.cs) owns that directory whole, so this project
// depends on it instead of re-globbing the two .cpp files itself — see the
// "ADOPTING THE ORPHAN" paragraph above.
//
// DEPENDENCY (direct #include edges beyond MeshGeometry, same "declare what
// is directly used" convention every phase-4 leaf follows — grep across
// every Tools/*.cpp and *.h for headers outside Tools/): AssetHandling/
// (18 files — every exporter publishes through Zenith_AssetRegistry /
// Zenith_*Asset) directly; Flux/ (14 files, including the MeshGeometry
// headers above) directly; TaskSystem/Zenith_TaskSystem.h directly
// (Zenith_Tools_TerrainExport.cpp); DataStream/ (9 files) directly;
// FileAccess/ (8 files) directly; Maths/ (5 files) directly; Core/ (3 files)
// directly; Collections/ (8 files, owned by ZenithCore alongside Core/ — see
// Sharpmake_ZenithCore.cs) directly; and EntityComponent/
// Zenith_ModelComponent.h directly (Zenith_Tools_MeshExport.cpp). So this
// lib depends on ZenithCore (mandatory) + ZenithMaths + ZenithDataStream +
// ZenithFileAccess + ZenithTaskSystem + ZenithAssetHandling + ZenithFlux +
// ZenithEntityComponent. No ZenithECS/ZenithPhysics/ZenithAI/ZenithScripting/
// ZenithInput/ZenithProfiling/ZenithDebugVariables/ZenithPrefab/
// ZenithWindows/ZenithUI edge: grep finds no direct #include of any of those
// directories' headers anywhere under Tools/.
//
// msdf-atlas-gen / freetype — GENUINELY NEEDED, UNLIKE ZenithEditor:
// Zenith_Tools_FontExport.cpp #includes "msdf-atlas-gen/msdf-atlas-gen.h"
// directly (ExportDefaultFontAtlas's MSDF bake) — the only Tools/Editor
// consumer of either dependency this phase (see Sharpmake_ZenithEditor.cs's
// own header comment for why Editor does NOT need this block: it has zero
// references to any of the three). This project therefore keeps the
// freetype/msdf-atlas-gen include paths and the MsdfAtlasGenProject edge that
// used to sit on the aggregate's ToolsEnabled.True branch (it transitively
// brings in MsdfgenProject + FreeTypeProject).
//
// ZENITH_ROOT — NEEDED DIRECTLY HERE, AND (CORRECTION) STILL ON THE
// AGGREGATE TOO: Zenith_Tools_HumanModelExport.cpp, …MeshExport.cpp and
// …TextureExport.cpp reference ZENITH_ROOT directly, unguarded
// (repo-root-relative path construction for asset staging) — this project
// genuinely needs its own copy of the define. Grep across Zenith/Editor
// finds zero hits of the same token — see Sharpmake_ZenithEditor.cs's own
// header comment — so ZENITH_ROOT does NOT move to ZenithEditor. It was
// ALSO grep-assumed (incorrectly, by this phase's own brief) to move OFF the
// aggregate entirely; build-verification found EngineComposition/
// Zenith_UnitTests.Tests.inl (#included from the aggregate's own
// Zenith_Core.cpp) references it too, unguarded — a real C2065 on a clean
// engine build otherwise. See Sharpmake_Zenith.cs's own
// ToolsEnabled.True-block comment: the aggregate keeps its own
// `conf.Defines.Add("ZENITH_ROOT=...")`, unchanged from before this phase,
// alongside this project's copy.
//
// ASSIMP — the pre-existing library-file link (assimp-vc143-mt[d].lib) is
// unchanged; only its SOURCE-tree exclusions grew the three entries the
// "ADOPTING THE ORPHAN" paragraph names (freetype/msdf-atlas-gen whole trees
// + stb_vorbis.c), matching the aggregate's own identical exclude set.
//
// The class is named ZenithToolsProject (not *Lib*) — pre-existing name kept
// unchanged so nothing referencing it elsewhere needs to change; the OUTPUT
// lib is named "ZenithTools".
[Sharpmake.Generate]
public class ZenithToolsProject : ZenithBaseProject
{
	public ZenithToolsProject()
	{
		Name = "ZenithTools";
		SourceRootPath = RootPath + "/Tools";

		// Source discovery recursively walks SourceRootPath and filters by the
		// configured C/C++ extensions; .gitignore does not constrain Sharpmake's
		// walk. Vault is a separate Node/Markdown app, so exclude its whole tree
		// before a dependency package can contribute native-looking sources.
		SourceFilesExcludeRegex.Add(@".*[\\/]Vault[\\/].*");

		// Tools are Windows-only, and (this phase) Tools-enabled-only — see
		// the file header comment's "WIN64 x TOOLS=TRUE ONLY" paragraph.
		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True,
			RenderBackend = RenderBackend.Vulkan | RenderBackend.D3D12 | RenderBackend.Null
		});

		// Assimp excludes
		SourceFilesExcludeRegex.Add(@".*assimp-5.4.2\\test.*");
		SourceFilesExcludeRegex.Add(@".*assimp-5.4.2\\tools.*");
		SourceFilesExcludeRegex.Add(@".*assimp-5.4.2\\port.*");
		SourceFilesExcludeRegex.Add(@".*zstream_test.*");
		SourceFilesExcludeRegex.Add(@".*zfstream.cpp.*");
		SourceFilesExcludeRegex.Add(@".*zip\\test.*");
		SourceFilesExcludeRegex.Add(@".*inflate86.*");
		SourceFilesExcludeRegex.Add(@".*minizip.*");
		SourceFilesExcludeRegex.Add(@".*iostream\\test.*");
		SourceFilesExcludeRegex.Add(@".*blast\\.*");
		SourceFilesExcludeRegex.Add(@".*puff\\.*");
		SourceFilesExcludeRegex.Add(@".*testzlib\\.*");
		SourceFilesExcludeRegex.Add(@".*untgz\\.*");
		// assimp's stb_vorbis.c is already compiled INTO assimp-vc143-mt[d].lib
		// (the prebuilt library this project links, below) — compiling it again
		// here would duplicate-symbol at link time. Mirrors the aggregate's own
		// identical exclude (Sharpmake_Zenith.cs). Dormant bug fix — see the
		// "ADOPTING THE ORPHAN" header-comment paragraph.
		SourceFilesExcludeRegex.Add(@".*stb_vorbis\.c.*");

		// MSDF font deps: source-vendored but built externally as their own
		// Sharpmake static-lib projects (Sharpmake_FreeType.cs,
		// Sharpmake_Msdfgen.cs, Sharpmake_MsdfAtlasGen.cs) and linked as
		// prebuilt .libs (see ConfigureAll's MsdfAtlasGenProject dependency
		// below) — exclude their full source trees from this project's own
		// /Tools glob so they don't get compiled a second time (ODR
		// violation otherwise). Mirrors the aggregate's own identical
		// exclude (Sharpmake_Zenith.cs). Dormant bug fix — see the
		// "ADOPTING THE ORPHAN" header-comment paragraph.
		SourceFilesExcludeRegex.Add(@".*Tools\\Middleware\\freetype\\.*");
		SourceFilesExcludeRegex.Add(@".*Tools\\Middleware\\msdf-atlas-gen\\.*");

		// Other excludes
		SourceFilesExcludeRegex.Add(@".*VulkanSDK.*");
		SourceFilesExcludeRegex.Add(@".*FluxCompiler.*");
		SourceFilesExcludeRegex.Add(@".*glm-master.*");

		// Jolt Physics excludes — dead code today (Tools/Middleware has no
		// JoltPhysics-5.4.0 subtree, so these never match anything under this
		// project's own /Tools SourceRootPath), kept only because they are
		// harmless and mirror the aggregate's historical list; not touched by
		// this phase.
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\Build.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\Docs.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\UnitTests.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\Samples.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\TestFramework.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\PerformanceTest.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\JoltViewer.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\HelloWorld.*");

		SourceFilesExcludeRegex.Add(@".*cmake.*");

		// Deliberately NO AdditionalSourceRootPaths.Add(/Zenith/Flux/MeshGeometry)
		// any more — ZenithFluxLibProject owns that directory whole; this
		// project depends on it instead (below). See the file header
		// comment's "MESHGEOMETRY" paragraph.
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithTools\[target.Platform]\[conf.Name]";

		// Per-lib PCH: ZenithTools CREATEs Zenith.pch from
		// Internal/Zenith_ToolsPCH.cpp.
		conf.PrecompHeader = "Zenith.h";
		conf.PrecompSource = "Zenith_ToolsPCH.cpp";

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf lib must not link or propagate the renderer backend — it
		// is boot-phase asset-bake code with no device, no render graph and
		// no scene (see Tools/CLAUDE.md).

		// Tools-specific include paths.
		conf.IncludePaths.Add(RootPath + "/Zenith");
		conf.IncludePaths.Add(RootPath + "/Zenith/Core");
		conf.IncludePaths.Add(RootPath + "/Middleware/glm-master");
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware");
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware/assimp/include");
		conf.IncludePaths.Add(RootPath + "/Middleware/stb");
		conf.IncludePaths.Add(RootPath + "/Middleware");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");
		conf.IncludePaths.Add(RootPath + "/Middleware/glfw-3.4.bin.WIN64/include");
		conf.IncludePaths.Add(RootPath + "/Middleware/VulkanSDK/1.3.280.0/Include");
		conf.IncludePaths.Add(RootPath + "/Zenith/Windows");
		conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends");
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");

		// msdf-atlas-gen / freetype — see the file header comment's own
		// paragraph on why this project (unlike ZenithEditor) genuinely
		// needs them.
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware/freetype/include");
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen");
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen/msdfgen");
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen/msdfgen/Config");

		conf.Defines.Add("ZENITH_TOOLS");
		conf.Defines.Add("OPENDDLPARSER_BUILD");

		// Compute actual paths using SharpmakeCsPath (Build directory)
		string zenithRoot = new DirectoryInfo(SharpmakeCsPath).Parent.FullName.Replace('\\', '/');

		// ZENITH_ROOT — needed directly by 3 TUs here. See the file header
		// comment's own paragraph on why this belongs to ZenithTools, not
		// ZenithEditor.
		conf.Defines.Add($"ZENITH_ROOT=\"{zenithRoot}/\"");

		// Asset paths for tools (these are defaults - game projects can override)
		// Note: Tools need these paths for asset import/export
		string engineAssetRoot = (zenithRoot + "/Zenith/Assets/");
		conf.Defines.Add($"ENGINE_ASSETS_DIR=\"{engineAssetRoot}\"");
		// GAME_ASSETS_DIR is set to a placeholder - specific game paths come from the game project
		conf.Defines.Add("GAME_ASSETS_DIR=\"./Assets/\"");

		// SHADER_SOURCE_ROOT — CORRECTION found while build-verifying this
		// phase's Null_ leg: the header-comment's grep for a direct
		// Flux_GraphicsImpl.h / Pipeline-header #include across Tools/*.cpp
		// came back empty, and was (wrongly) treated as proof this project
		// does not need the define. Several Tools TUs (…RockAssetExport.cpp,
		// …MeshExport.cpp, …HumanModelExport.cpp, …TerrainExport.cpp,
		// …TextureExport.cpp, …GlbImport.cpp, …TreeAssetExport.cpp) reach
		// Flux/MeshGeometry headers that transitively pull in
		// RenderBackends/Null/Zenith_Null_Pipeline.h, which references
		// SHADER_SOURCE_ROOT directly, unguarded — a Null_..._True build
		// fails C2065 without it. Exactly the hazard this phase's own brief
		// warned about (ZenithUI needed the same define with zero textual
		// occurrences anywhere in its own directory) — "grep the include
		// graph, not just the token" turned out not to be enough either;
		// only a real Null_ build surfaced it. Win64-only project (no agde
		// target declared at all), so a single unconditional define, same
		// value as every other leaf's win64 branch.
		string shaderSourceRoot = zenithRoot + "/Zenith/Flux/Shaders/";
		conf.Defines.Add($"SHADER_SOURCE_ROOT=\"{shaderSourceRoot}\"");

		conf.Output = Configuration.OutputType.Lib;

		// Library paths
		conf.LibraryPaths.Add(RootPath + "/Tools/Middleware/assimp/lib");

		if (target.Optimization == Optimization.Debug)
		{
			conf.LibraryFiles.Add("assimp-vc143-mtd.lib");
		}
		else
		{
			conf.LibraryFiles.Add("assimp-vc143-mt.lib");
		}

		// MANDATORY: ZenithCore — see Sharpmake_ZenithCore.cs's header comment.
		conf.AddPublicDependency<ZenithCoreLibProject>(target);

		// Direct edges — see the file header comment's DEPENDENCY paragraph.
		conf.AddPublicDependency<ZenithMathsLibProject>(target);
		conf.AddPublicDependency<ZenithDataStreamLibProject>(target);
		conf.AddPublicDependency<ZenithFileAccessLibProject>(target);
		conf.AddPublicDependency<ZenithTaskSystemLibProject>(target);
		conf.AddPublicDependency<ZenithAssetHandlingLibProject>(target);

		// MeshGeometry now arrives through ZenithFlux instead of a second
		// compiled copy — see the file header comment's "MESHGEOMETRY"
		// paragraph.
		conf.AddPublicDependency<ZenithFluxLibProject>(target);

		conf.AddPublicDependency<ZenithEntityComponentLibProject>(target);

		// MSDF font deps as Sharpmake static-lib project deps (tools-only).
		// MsdfAtlasGenProject transitively brings in MsdfgenProject + FreeTypeProject.
		conf.AddPublicDependency<MsdfAtlasGenProject>(target);
	}
}
