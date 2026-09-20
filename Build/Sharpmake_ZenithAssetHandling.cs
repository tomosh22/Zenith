using Sharpmake;
using System;
using System.IO;

// ZenithAssetHandling — leaf static library (Phase 4d — see Sharpmake_ZenithCore.cs
// for the pattern this file replicates). Holds exactly Zenith/AssetHandling — 21
// translation units (Zenith_Asset.cpp, Zenith_AssetHandle.cpp, Zenith_AssetRegistry.cpp,
// Zenith_AnimationAsset.cpp, Zenith_AnimatorControllerAsset.cpp, Zenith_BehaviourGraphAsset.cpp,
// Zenith_BoneMaskAsset.cpp, Zenith_FileWatcher.cpp, Zenith_FontAsset.cpp,
// Zenith_GrassTypeTableAsset.cpp, Zenith_HumanProportions.cpp, Zenith_Image.cpp,
// Zenith_MaterialAsset.cpp, Zenith_MaterialParamTable.cpp, Zenith_MeshAsset.cpp,
// Zenith_MeshGeometryAsset.cpp, Zenith_ModelAsset.cpp, Zenith_PropertyTuning.cpp,
// Zenith_SkeletonAsset.cpp, Zenith_SkinDeform.cpp, Zenith_TextureAsset.cpp) + the
// definition-free PCH create TU (Internal/Zenith_AssetHandlingPCH.cpp). Previously
// globbed straight into the monolithic aggregate ZenithProject. Single flat
// directory — no subdirectories, no AdditionalSourceRootPaths needed.
//
// THE PCH: >= 4 compiled TUs justifies a binary PCH under this refactor's rule.
// ZenithAssetHandling CREATEs its own Zenith.pch from
// Internal/Zenith_AssetHandlingPCH.cpp (a one-line `#include "Zenith.h"`, nothing
// else — mirrors Zenith/Scripting/Internal/Zenith_ScriptingPCH.cpp; it MUST stay
// definition-free, see that file's own header comment for why). The 21 real TUs
// /Yu against it.
//
// THE Flux <-> AssetHandling CYCLE — DECLARED ONE-WAY, NOT BROKEN: 11 files here
// (Zenith_AnimationAsset.cpp, Zenith_AnimatorControllerAsset.h, Zenith_BoneMaskAsset.cpp,
// Zenith_FontAsset.cpp, Zenith_GrassTypeTableAsset.h, Zenith_MaterialAsset.cpp,
// Zenith_MeshAsset.{h,cpp}, Zenith_MeshGeometryAsset.cpp, Zenith_TextureAsset.{h,cpp})
// #include a Flux/** header directly — the asset classes own Flux types (or Flux
// resource handles) BY VALUE, not by pointer, so these are not forward-declarable
// away. ZenithFlux (Sharpmake_ZenithFlux.cs) declares the single project-level
// dependency edge, Flux -> AssetHandling (23 include edges that direction against
// these 11 the other way — the heavier direction wins the edge). This project
// deliberately does NOT AddPublicDependency<ZenithFluxLibProject> — that would
// make the edge genuinely circular in Sharpmake's dependency graph, not just in
// #include terms. Compiling this static lib never needs Flux's .lib (lib.exe
// does not link); the "/Zenith" include path every leaf already adds via
// ConfigureCommonIncludePaths is enough for these 11 files' Flux/** #includes to
// resolve, exactly the same include-path-only reasoning
// Sharpmake_ZenithDebugVariables.cs already documents for its own Flux_GraphicsImpl.h
// include. Any Flux_:: symbol referenced out-of-line resolves at the final game
// exe link, where ZenithFlux is always present transitively (it is a mandatory
// row in Sharpmake_EngineLibs.cs, depended on by the aggregate). See
// Zenith/Flux/CLAUDE.md and Zenith/AssetHandling/CLAUDE.md for the documented
// direction and why the reverse edges are accepted rather than designed away —
// the real fix (splitting AssetHandling into handle/registry vs GPU-resource
// halves) is a design change for its own future ticket.
//
// DEPENDENCY (direct #include edges, same "declare what is directly used"
// convention every Phase 4b/4c leaf follows): Zenith_FontAsset.h / Zenith_HumanProportions.h /
// Zenith_MaterialAsset.h / Zenith_MaterialParamTable.h / Zenith_MeshAsset.h /
// Zenith_MeshGeometryAsset.h / Zenith_SkeletonAsset.h / Zenith_SkinDeform.h all
// #include Maths/Zenith_Maths.h directly; Zenith_AnimatorControllerAsset.cpp /
// Zenith_BoneMaskAsset.cpp / Zenith_FontAsset.cpp / Zenith_MaterialAsset.cpp /
// Zenith_MaterialParamTable.cpp / Zenith_MeshAsset.cpp / Zenith_ModelAsset.cpp /
// Zenith_PropertyTuning.cpp / Zenith_SkeletonAsset.cpp / Zenith_TextureAsset.cpp
// all #include DataStream/Zenith_DataStream.h or Zenith_StreamEnvelope.h directly;
// Zenith_BehaviourGraphAsset.h #includes Scripting/Zenith_BehaviourGraph.h
// directly (the .bgraph asset wraps a Zenith_GraphDefinition). So this lib
// depends on ZenithCore (mandatory) + ZenithMaths + ZenithDataStream +
// ZenithScripting. No ZenithECS edge: nothing under Zenith/AssetHandling
// #includes ZenithECS (verified — grep for "ECS" across the directory's headers
// and sources finds none). Scripting does not depend back on AssetHandling
// (its own CLAUDE.md: "it never names Flux, Physics, AssetHandling, or any
// concrete component" — enforced by the architecture gate), so this edge is a
// clean one-way addition, not a second cycle.
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithAssetHandling".
[Sharpmake.Generate]
public class ZenithAssetHandlingLibProject : ZenithBaseProject
{
	// Leaf static lib — renderer-agnostic (see the file header comment on the
	// declared Flux -> AssetHandling edge); must not link or propagate
	// glfw/vulkan/slang, so ConfigureAll below calls none of the
	// AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs helpers.
	public ZenithAssetHandlingLibProject()
	{
		Name = "ZenithAssetHandling";

		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan | RenderBackend.D3D12 | RenderBackend.Null
		});

		AddTargets(new ZenithTarget
		{
			Platform = Platform.agde,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan,
			AndroidBuildTargets = ZenithAndroidAbi.All
		});

		SourceRootPath = RootPath + "/Zenith/AssetHandling";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithAssetHandling\[target.Platform]\[conf.Name]";

		// Per-lib PCH: ZenithAssetHandling CREATEs Zenith.pch from
		// Internal/Zenith_AssetHandlingPCH.cpp.
		conf.PrecompHeader = "Zenith.h";
		conf.PrecompSource = "Zenith_AssetHandlingPCH.cpp";

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf lib must not link or propagate the renderer backend.

		// The PCH (Zenith.h) transitively pulls imgui + Flux/Vulkan/vma/Jolt
		// headers, and several TUs here (e.g. Zenith_BehaviourGraphAsset.cpp)
		// #include imgui.h directly and Flux/** headers directly (see the file
		// header comment) — include paths only, no libs linked.
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		// SHADER_SOURCE_ROOT — found via a Null_ config build (C2065
		// 'SHADER_SOURCE_ROOT': undeclared identifier, reported against
		// Zenith_SkinDeform.cpp / Zenith_TextureAsset.cpp), exactly the defect
		// class Sharpmake_ZenithDebugVariables.cs already documents:
		// Zenith_TextureAsset.cpp #includes Flux/Flux_GraphicsImpl.h directly
		// (see the file header comment), which reaches the active backend's
		// Pipeline header (Zenith_Null_Pipeline.h / Zenith_D3D12_Pipeline.h),
		// and both reference SHADER_SOURCE_ROOT directly, unguarded. This
		// define previously existed only on the aggregate ZenithProject and
		// each GameProject, neither of which this standalone leaf lib is.
		// Mirrors ZenithProject's own identical win64/agde branch exactly
		// (same value, same "" on agde).
		string zenithRoot = Path.GetFullPath(Path.Combine(SharpmakeCsPath, "..")).Replace('\\', '/');
		if (target.Platform == Platform.win64)
		{
			string shaderSourceRoot = zenithRoot + "/Zenith/Flux/Shaders/";
			conf.Defines.Add($"SHADER_SOURCE_ROOT=\"{shaderSourceRoot}\"");
		}
		else if (target.Platform == Platform.agde)
		{
			conf.Defines.Add("SHADER_SOURCE_ROOT=\"\"");
		}

		conf.Output = Configuration.OutputType.Lib;

		// MANDATORY: ZenithCore — see Sharpmake_ZenithCore.cs's header comment.
		conf.AddPublicDependency<ZenithCoreLibProject>(target);

		// Direct edges — see the file header comment's DEPENDENCY paragraph.
		// Deliberately NO ZenithFlux edge (see the CYCLE paragraph above).
		conf.AddPublicDependency<ZenithMathsLibProject>(target);
		conf.AddPublicDependency<ZenithDataStreamLibProject>(target);
		conf.AddPublicDependency<ZenithScriptingLibProject>(target);

		if (target.ToolsEnabled == ToolsEnabled.True && target.Platform == Platform.win64)
		{
			conf.Defines.Add("ZENITH_TOOLS");

			conf.IncludePaths.Add(RootPath + "/Tools/Middleware");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/assimp/include");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/freetype/include");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen/msdfgen");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen/msdfgen/Config");

			conf.PrecompSourceExcludeFolders.Add(RootPath + "/Tools");
		}
	}
}
