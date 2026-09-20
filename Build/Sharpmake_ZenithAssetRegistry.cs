using Sharpmake;
using System;
using System.IO;

// ZenithAssetRegistry — the LOWER half of the Zenith/AssetHandling split
// (Phase 4i). This is the library the Flux <-> AssetHandling cycle ends at.
//
// ═════════════════════════════════════════════════════════════════════════
// WHY THIS PROJECT EXISTS
// ═════════════════════════════════════════════════════════════════════════
// Phase 4d extracted Zenith/AssetHandling whole, as ONE library, and declared
// the genuine header cycle with Flux ONE-WAY: ZenithFlux -> ZenithAssetHandling,
// the heavier direction. That worked (a static lib resolves no external symbol
// at its own build step) but left the cycle in place — the declared graph was
// acyclic only because the lighter direction was left undeclared.
//
// Phase 4i splits the directory into two libraries along a MEASURED line, so
// the lower half can sit BELOW Flux honestly:
//
//   ZenithAssetRegistry   (this project)     — identifiers, lookup, lifetime.
//                                              Its own headers name no Flux
//                                              type. ZenithFlux depends on IT.
//   ZenithAssetHandling   (the GPU half)     — the asset classes that own Flux
//                                              types BY VALUE. Depends on this
//                                              project AND on ZenithFlux.
//
// Declared graph: ZenithAssetRegistry <- ZenithFlux <- ZenithAssetHandling.
// Acyclic. Do NOT add a ZenithFlux -> ZenithAssetHandling edge "for
// completeness": Sharpmake will not tolerate the cycle, and Flux does not need
// it (see THE REMAINING EDGE below).
//
// ═════════════════════════════════════════════════════════════════════════
// THE SPLIT LINE, AS MEASURED (2026-09-20) — read this before moving a unit
// ═════════════════════════════════════════════════════════════════════════
// A unit belongs to the GPU half if its .h OR its .cpp reaches a Flux/**
// header, because either one creates a build edge at library granularity.
//
// Four headers under Zenith/AssetHandling #include a Flux/** header directly:
//
//   Zenith_AnimatorControllerAsset.h  -> Flux/MeshAnimation/Flux_AnimatorControllerDef.h  (owns Flux_AnimatorControllerDef by value)
//   Zenith_GrassTypeTableAsset.h      -> Flux/Vegetation/Flux_GrassTypeTable.h            (owns Flux_GrassTypeTable by value)
//   Zenith_MeshAsset.h                -> Flux/Flux_Buffers.h                              (owns Flux_VertexBuffer/Flux_IndexBuffer/Flux_BufferLayout by value)
//   Zenith_TextureAsset.h             -> Flux/Flux.h  (the umbrella)                      (owns Flux_SurfaceInfo/Flux_VRAMHandle/Flux_ShaderResourceView by value)
//
// The last two dominate — Zenith_TextureAsset.h is the AssetHandling header
// Flux #includes most often, so "Flux -> Zenith_TextureAsset.h -> Flux/Flux.h"
// was a direct cycle straight through the asset layer. None of these four is
// forward-declarable away: the members are BY VALUE, not by pointer. (Phase 4d
// considered relocating the first two instead and rejected it — they pull in
// Flux_AnimationStateMachineDef.h + Flux_AnimationLayer.h and Flux_GrassTypes.h
// respectively, so the move drags three more files across or leaves the edge
// exactly where it was. That analysis was correct; it just addressed the two
// files that matter LEAST.)
//
// A further five .cpp reach Flux directly without their header doing so
// (Zenith_AnimationAsset.cpp, Zenith_BoneMaskAsset.cpp, Zenith_FontAsset.cpp,
// Zenith_MaterialAsset.cpp, Zenith_MeshGeometryAsset.cpp), which is why the
// GPU half is NINE units and not four.
//
// MEASURED AND RECORDED: **no header owned by THIS project reaches Flux,
// directly or transitively through its own headers.** The lower half's header
// closure is CLOSED. Verified by walking every intra-directory #include:
//   Zenith_Asset.h              -> Core/Zenith_Result.h
//   Zenith_AssetHandle.h        -> Zenith_Asset.h, Zenith_AssetRegistry.h
//   Zenith_AssetRegistry.h      -> Core/**, Collections/Zenith_HashMap.h  (forward-declares Zenith_TextureAsset & co.)
//   Zenith_AssetTypeIds.h       -> (nothing)
//   Zenith_BehaviourGraphAsset.h-> Zenith_Asset.h, Scripting/Zenith_BehaviourGraph.h
//   Zenith_FileWatcher.h        -> Collections/**, Core/**
//   Zenith_HumanProportions.h   -> Maths/Zenith_Maths.h
//   Zenith_Image.h              -> Collections/Zenith_Vector.h
//   Zenith_MaterialParamTable.h -> Maths/Zenith_Maths.h
//   Zenith_ModelAsset.h         -> Zenith_Asset.h, Zenith_AssetHandle.h, Collections/**
//   Zenith_PropertyTuning.h     -> Core/Zenith_PropertySystem.h
//   Zenith_SkeletonAsset.h      -> Zenith_Asset.h, Maths/**, Collections/**
//   Zenith_SkinDeform.h         -> Zenith_HumanProportions.h, Maths/Zenith_Maths.h
// Zenith_AssetRegistry.h's FORWARD DECLARATIONS of Zenith_TextureAsset /
// Zenith_MaterialAsset / Zenith_MeshAsset / Zenith_SkeletonAsset /
// Zenith_ModelAsset / Zenith_Prefab are what make that closure possible, and
// they are load-bearing — replacing any of them with an #include would put a
// Flux type back in this library's header closure and undo this whole phase.
//
// ═════════════════════════════════════════════════════════════════════════
// THE REMAINING EDGE — HONEST ABOUT WHAT IS *NOT* GONE
// ═════════════════════════════════════════════════════════════════════════
// Three of this project's TUs #include a GPU-half header, and therefore
// compile Flux headers off the include path:
//
//   Zenith_AssetRegistry.cpp  — includes all 10 concrete asset headers; the
//                               loader-table registration is what it is FOR.
//   Zenith_AssetHandle.cpp    — the explicit handle template instantiations,
//                               which need each complete type.
//   Zenith_SkinDeform.cpp     — includes Zenith_MeshAsset.h.
//   (+ Zenith_AssetRegistry.Tests.inl, compiled into Zenith_AssetRegistry.cpp.)
//
// That is COMPILATION, not a declared library edge, and it is the same
// include-path-only shape Sharpmake_ZenithDebugVariables.cs already documents.
// The out-of-line Zenith_TextureAsset:: / Zenith_MeshAsset:: symbols those TUs
// reference resolve at the final exe link, where ZenithAssetHandling is always
// present (it is a mandatory row in Sharpmake_EngineLibs.cs). Note also that
// the shared Zenith.h PCH itself transitively reaches Flux for EVERY TU in the
// engine, so "this library does not compile a Flux header" was never available
// to claim and is not claimed here. What IS true, and is the deliverable: the
// DECLARED module graph no longer contains a cycle involving the registry half,
// and no header this project owns names a Flux type.
//
// ═════════════════════════════════════════════════════════════════════════
// MEMBERSHIP — SHARED SOURCE ROOT, PARTITIONED BY SourceFilesExcludeRegex
// ═════════════════════════════════════════════════════════════════════════
// NOT A SINGLE FILE MOVED, and not a single #include spelling changed. 251
// files across Zenith/, Games/, Tools/ and Tests/ #include
// "AssetHandling/<something>.h"; relocating a header into a subdirectory would
// break every one of them for zero benefit, since project membership is what
// the library split actually needs.
//
// So this project and ZenithAssetHandling SHARE the Zenith/AssetHandling
// source root and partition it with constructor-time SourceFilesExcludeRegex —
// a MEMBERSHIP exclude ("which project owns this file"), the legitimate
// mechanism in this codebase (Sharpmake_ZenithImGui.cs, Sharpmake_ZenithJolt.cs
// and Sharpmake_FreeType.cs all use it for exactly this). It is NOT
// conf.SourceFilesBuildExcludeRegex, the per-configuration BUILD exclude, whose
// call-site count Phase 4h.3 drove to zero across the whole codebase and which
// Tools/check_project_membership.ps1 now fails on outright.
//
// Every file under Zenith/AssetHandling is owned by EXACTLY ONE of the two
// projects: each .Tests.inl follows its own .cpp, each .h follows its unit, and
// each project carries its OWN definition-free PCH create TU under Internal/
// (this one's is Internal/Zenith_AssetRegistryPCH.cpp; ZenithAssetHandling
// keeps the pre-existing Internal/Zenith_AssetHandlingPCH.cpp). The two exclude
// lists are exact complements — if you add a unit to this directory you must
// add it to exactly one of them, or check_project_membership.ps1's UNOWNED
// check (fatal) will say so.
//
// ★ THE REGEXES ARE PREFIX-SENSITIVE AND EVERY ONE ENDS IN `\.` ON PURPOSE.
// "Zenith_Asset" is a prefix of Zenith_AssetHandle, Zenith_AssetRegistry,
// Zenith_AssetTypeIds AND Zenith_AssetHandlingPCH; "Zenith_MaterialAsset" and
// "Zenith_MaterialParamTable" land on OPPOSITE sides of the split; so does
// "Zenith_MeshAsset" vs nothing but itself, while "Zenith_AnimationAsset" and
// "Zenith_AnimatorControllerAsset" share eight characters. Requiring the dot
// immediately after the unit name is what keeps each pattern to its own unit's
// {.h, .cpp, .inl, .Tests.inl} set.
//
// ═════════════════════════════════════════════════════════════════════════
// DEPENDENCY (direct #include edges, the "declare what is directly used"
// convention every Phase 4b/4c/4d leaf follows)
// ═════════════════════════════════════════════════════════════════════════
//   ZenithCore      — MANDATORY for every Zenith library (Zenith_LogFileSinkWrite;
//                     see Sharpmake_ZenithCore.cs). Also a real direct edge here:
//                     Zenith_Asset.h / Zenith_AssetRegistry.h / Zenith_FileWatcher.h /
//                     Zenith_PropertyTuning.h all #include a Core/** header.
//   ZenithMaths     — Zenith_HumanProportions.h, Zenith_MaterialParamTable.h,
//                     Zenith_SkeletonAsset.h, Zenith_SkinDeform.h.
//   ZenithDataStream— Zenith_MaterialParamTable.cpp, Zenith_ModelAsset.cpp,
//                     Zenith_PropertyTuning.cpp, Zenith_SkeletonAsset.cpp.
//   ZenithScripting — Zenith_BehaviourGraphAsset.h wraps a Zenith_GraphDefinition.
//                     Scripting never names AssetHandling back (its own CLAUDE.md,
//                     enforced by the architecture gate), so this is a clean
//                     one-way edge and not a second cycle.
// NO ZenithFlux edge, ever. That is this project's entire reason to exist.
// NO ZenithECS edge: nothing under Zenith/AssetHandling #includes ZenithECS.
// Zenith_AssetRegistry.cpp and Zenith_ModelAsset.cpp additionally #include
// Profiling/Zenith_Profiling.h
// and Zenith_AssetRegistry.Tests.inl #includes Prefab/Zenith_Prefab.h; neither
// is declared, exactly as the undivided ZenithAssetHandling did not declare
// them before this split — include-path-only, resolved at the exe link. Left
// unchanged deliberately: adding edges is a separate question from breaking
// the cycle, and ZenithPrefab sits ABOVE this library in the layer map.
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithAssetRegistry".
[Sharpmake.Generate]
public class ZenithAssetRegistryLibProject : ZenithBaseProject
{
	// Renderer-agnostic leaf: must not link or propagate glfw/vulkan/slang, so
	// ConfigureAll below calls none of the AddGlfwLibs/AddVulkanBackendLibs/
	// AddSlangLibs/AddD3D12Libs helpers.
	public ZenithAssetRegistryLibProject()
	{
		Name = "ZenithAssetRegistry";

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

		// MEMBERSHIP: own the 13 registry units (12 compiled .cpp + the
		// header-only Zenith_AssetTypeIds.h) + this project's own PCH create
		// TU. Everything below is the GPU half, owned by ZenithAssetHandling —
		// see Sharpmake_ZenithAssetHandling.cs's exactly-complementary list.
		SourceFilesExcludeRegex.Add(@"AssetHandling\\Zenith_AnimationAsset\.");
		SourceFilesExcludeRegex.Add(@"AssetHandling\\Zenith_AnimatorControllerAsset\.");
		SourceFilesExcludeRegex.Add(@"AssetHandling\\Zenith_BoneMaskAsset\.");
		SourceFilesExcludeRegex.Add(@"AssetHandling\\Zenith_FontAsset\.");
		SourceFilesExcludeRegex.Add(@"AssetHandling\\Zenith_GrassTypeTableAsset\.");
		SourceFilesExcludeRegex.Add(@"AssetHandling\\Zenith_MaterialAsset\.");
		SourceFilesExcludeRegex.Add(@"AssetHandling\\Zenith_MeshAsset\.");
		SourceFilesExcludeRegex.Add(@"AssetHandling\\Zenith_MeshGeometryAsset\.");
		SourceFilesExcludeRegex.Add(@"AssetHandling\\Zenith_TextureAsset\.");

		// The OTHER project's PCH create TU. Two projects creating Zenith.pch
		// from the same source in the same directory would be ambiguous; each
		// owns exactly one.
		SourceFilesExcludeRegex.Add(@"AssetHandling\\Internal\\Zenith_AssetHandlingPCH\.cpp");
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		// DISTINCT intermediate path — two projects creating Zenith.pch in one
		// obj dir is MSB8028.
		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithAssetRegistry\[target.Platform]\[conf.Name]";

		// Per-lib PCH: 12 compiled TUs clears the ">= 4 TUs justifies a binary
		// PCH" rule. Internal/Zenith_AssetRegistryPCH.cpp is a one-line
		// `#include "Zenith.h"` and MUST stay definition-free — AGDE/clang
		// precompiles the whole PrecompSource via -xc++-header, so a
		// definition-bearing create TU bakes its symbols into the .pch and
		// every /Yu consumer re-emits them.
		conf.PrecompHeader = "Zenith.h";
		conf.PrecompSource = "Zenith_AssetRegistryPCH.cpp";

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf lib must not link or propagate the renderer backend.

		// The shared Zenith.h PCH transitively pulls imgui + Flux/Vulkan/vma/
		// Jolt headers for every TU here, and three of this project's own TUs
		// reach Flux/Flux.h through a GPU-half header (see THE REMAINING EDGE
		// in the file header comment) — include paths only, no libs linked.
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		// SHADER_SOURCE_ROOT — THE REGISTRY HALF NEEDS THIS TOO, and that is
		// measured, not assumed. The undivided ZenithAssetHandling's own header
		// comment recorded the C2065 that forced the define being reported
		// against Zenith_SkinDeform.cpp AND Zenith_TextureAsset.cpp on a Null_
		// config — and Zenith_SkinDeform.cpp is on THIS side of the split (it
		// #includes Zenith_MeshAsset.h -> Flux/Flux_Buffers.h). The route is
		// the one Sharpmake_ZenithDebugVariables.cs already documents: a Flux
		// header reaches the active backend's Pipeline header
		// (Zenith_Null_Pipeline.h / Zenith_D3D12_Pipeline.h), both of which
		// reference SHADER_SOURCE_ROOT directly and unguarded. Grepping this
		// directory for the token predicts nothing — ZenithUI needed the define
		// with zero textual occurrences anywhere in its own tree. Mirrors
		// ZenithProject's identical win64/agde branch exactly (same value, same
		// "" on agde).
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
		// Deliberately NO ZenithFlux edge; that is the deliverable.
		conf.AddPublicDependency<ZenithMathsLibProject>(target);
		conf.AddPublicDependency<ZenithDataStreamLibProject>(target);
		conf.AddPublicDependency<ZenithScriptingLibProject>(target);

		// ODR LOCKSTEP on ZENITH_TOOLS — carried verbatim from the undivided
		// ZenithAssetHandling. ZENITH_PROFILING_ENABLED and
		// ZENITH_MEMORY_TRACKING_LEVEL come from ConfigureCommonSettings above,
		// which every library in this engine calls.
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
