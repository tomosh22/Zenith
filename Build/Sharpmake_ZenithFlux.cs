using Sharpmake;
using System;
using System.IO;

// ZenithFlux — leaf static library (Phase 4d — see Sharpmake_ZenithCore.cs for
// the pattern this file replicates). Holds exactly Zenith/Flux — 84 translation
// units across its ~30 subsystem subdirectories (Backend, Decals, DeferredShading,
// DynamicLights, Fog, Gizmos, HDR, HiZ, IBL, InstancedMeshes, MeshAnimation,
// MeshGeometry, Particles, Present, Primitives, Quads, RenderGraph, RenderViews,
// SDFs, SSAO, SSGI, SSR, SceneGraph, Shadows, Skybox, Slang, TAA, Terrain, Text,
// Translucency, UnifiedMesh, Vegetation, plus the top-level Flux_*.cpp/Zenith_*.cpp)
// + the definition-free PCH create TU (Internal/Zenith_FluxPCH.cpp). 84 + 1 = 85
// compiled TUs. Previously globbed straight into the monolithic aggregate
// ZenithProject. Zenith/Flux/Shaders/** (.slang) are project members via the
// SourceFilesExtensions ".slang" entry every ZenithBaseProject already declares,
// but are never compiled (SourceFilesCompileExtensions is .cpp/.c only) — they
// simply need to belong to exactly one project, and this is it.
//
// THE PCH: >= 4 compiled TUs justifies a binary PCH under this refactor's rule.
// ZenithFlux CREATEs its own Zenith.pch from Internal/Zenith_FluxPCH.cpp (a
// one-line `#include "Zenith.h"`, nothing else — mirrors
// Zenith/Scripting/Internal/Zenith_ScriptingPCH.cpp; it MUST stay definition-free,
// see that file's own header comment for why). The 84 real TUs /Yu against it.
//
// ★ REGISTRAR-TU SURVIVAL (Zenith/Flux/CLAUDE.md documents a past incident):
// several TUs here (including Flux_FeatureRegistry.cpp) carry self-registering
// ZENITH_TEST registrars whose only reference is their own static initializer —
// /OPT:REF dead-strips an .obj with no OTHER inbound reference and silently
// drops its test registrations with it. That risk is about which .obj members
// the final EXE link pulls in, which is unchanged by this split (the same 84
// .obj land in zenithflux.lib now instead of zenith.lib, and the exe link still
// receives every engine .lib as an input via ZenithEngineLibs.All) — but it is
// exactly what verification step 7 (the unit gate must report precisely 2706)
// exists to catch if this reasoning is ever wrong for a future TU.
//
// THE Flux <-> AssetHandling CYCLE — BROKEN IN PHASE 4i, NOT MERELY DECLARED.
// Measured on master 2026-09-20: 74 `#include "AssetHandling/..."` directives
// across 51 distinct files HERE (Decals/DeferredShading/DynamicLights/Fog/
// Gizmos/HDR/HiZ/IBL/InstancedMeshes/MeshAnimation/Particles/Present/Primitives/
// Quads/RenderGraph/RenderViews/SDFs/SSAO/SSGI/SSR/Shadows/Skybox/TAA/Terrain/
// Text/Translucency/UnifiedMesh/Vegetation), against 12 directives across 11
// files the OTHER way — of which only FOUR are headers, and headers are the
// only ones that make a *header* cycle. (Phase 4d's own comment said "23
// against 11"; the 23 was a build-relevant subset, the 11 was never narrowed to
// headers, and it counted FILES where this line counts DIRECTIVES —
// Zenith_TextureAsset.cpp:7 and :8 both include Flux_GraphicsImpl.h, a
// pre-existing duplicate. Both figures here include .Tests.inl on both sides,
// so the two populations are finally comparable.)
//
// Phase 4d declared that pair one-way, Flux -> AssetHandling, because the two
// files AssetHandling reached back into looked immovable: Flux_AnimatorControllerDef.h
// and Flux_GrassTypeTable.h pull in Flux_AnimationStateMachineDef.h /
// Flux_AnimationLayer.h and Flux_GrassTypes.h respectively, so relocating them
// alone drags three more across or leaves the edge in place either way. That
// rider is STILL TRUE — and still beside the point, because the two headers
// that actually dominated the reverse direction were never those two:
//   Zenith_TextureAsset.h -> Flux/Flux.h        (the heavy umbrella; the
//                                                AssetHandling header Flux
//                                                includes MOST often — so
//                                                "Flux -> TextureAsset.h ->
//                                                Flux/Flux.h" was a direct
//                                                cycle through the asset layer)
//   Zenith_MeshAsset.h    -> Flux/Flux_Buffers.h
//
// Phase 4i split Zenith/AssetHandling into two libraries along the measured
// Flux-reachability line instead of moving files:
//   ZenithAssetRegistry  — identifiers, lookup, lifetime. NO header it owns
//                          names a Flux type, directly or transitively.
//   ZenithAssetHandling  — the 9 units whose .h or .cpp reaches Flux.
// THIS PROJECT'S EDGE THEREFORE MOVED, from ZenithAssetHandling to
// ZenithAssetRegistry (below). Declared graph:
//     ZenithAssetRegistry <- ZenithFlux <- ZenithAssetHandling.
// Acyclic, with no module cycle left involving the registry half.
//
// The remaining edge is one-way and legitimate: this project's #includes of
// GPU-half headers (Zenith_TextureAsset.h, Zenith_MaterialAsset.h,
// Zenith_MeshAsset.h, Zenith_MeshGeometryAsset.h, Zenith_FontAsset.h,
// Zenith_AnimationAsset.h, ...) resolve off the plain "/Zenith" include path
// every leaf already carries, and any out-of-line GPU-half symbol resolves at
// the final game exe link, where ZenithAssetHandling is always present (a
// mandatory row in Sharpmake_EngineLibs.cs). Note "symbol", not
// "Zenith_*Asset:: member": the one most worth naming is a FREE function --
// Flux_Grass.cpp calls Zenith_GrassTypeTableAsset_ForceLink(), so ZenithFlux
// reaches UP into the library that defines it. That is sound (the call IS the
// undefined external that pulls the object, and link.exe resolves across every
// archive on the exe line), but it is the sharp edge here: that TU
// self-registers and hosts NO .Tests.inl, so if it were ever dead-stripped no
// unit count would move and no gate would say a word. This is the SAME shape
// as EntityComponent -> Flux and backend -> Flux, both already one-way here.
// Do NOT add a ZenithFlux -> ZenithAssetHandling edge back "for completeness":
// Sharpmake will not tolerate the cycle, and the exe link does not need it.
// See Sharpmake_ZenithAssetRegistry.cs for the full measurement and
// Zenith/AssetHandling/CLAUDE.md for the narrative.
//
// DEPENDENCY (direct #include edges beyond AssetHandling, same "declare what is
// directly used" convention every Phase 4b/4c/4d leaf follows):
// Flux.h #includes Profiling/Zenith_Profiling.h directly; several TUs
// (Flux_Decals.cpp, Flux_DeferredShading.cpp, Flux.cpp, Flux_Terrain.cpp, ...)
// #include DebugVariables/Zenith_DebugVariables.h directly;
// Flux_RenderGraph.cpp #includes TaskSystem/Zenith_TaskSystem.h directly;
// Flux_Terrain.cpp #includes ZenithECS/Zenith_Entity.h + Zenith_Scene.h +
// Zenith_SceneData.h + Zenith_SceneSystem.h directly; Flux_Grass.cpp
// (Vegetation/) #includes FileAccess/Zenith_FileAccess.h directly. So this lib depends on
// ZenithCore (mandatory) + ZenithMaths + ZenithDataStream + ZenithFileAccess +
// ZenithProfiling + ZenithDebugVariables + ZenithTaskSystem + ZenithECS +
// ZenithAssetRegistry (Phase 4i — was ZenithAssetHandling; see the CYCLE
// paragraph above). No GLFW/Vulkan-API edge: grep across every Flux .cpp
// finds zero direct `vkXxx(...)` calls and zero GLFW usage — Flux is
// backend-agnostic (Flux_BackendTypes.h / the concept-conformance layer under
// Backend/ is the seam), so AddGlfwLibs/AddVulkanBackendLibs are NOT called
// here; the (not-yet-extracted) Vulkan-backend library is where those calls
// actually live, still aggregate-owned for now.
//
// SLANG — THE ONE CONDITIONAL LIBRARY EDGE: only
// Zenith/Flux/Slang/Flux_SlangCompiler.cpp calls the slang API (guarded at its
// own line 8 by `#if defined(ZENITH_WINDOWS) && defined(ZENITH_VULKAN)`), so
// ZenithFlux is the right owner of slang.lib. AddSlangLibs (Sharpmake_Common.cs)
// is called ONLY under win64 && RenderBackend.Vulkan below — Null and D3D12
// configs have no slang lib path at all (ConfigureCommonLibraryPaths never
// added one for them historically either), so calling it unconditionally would
// break every Null_/D3D12_ link. The translation unit itself needs no extra
// per-configuration or per-project exclude of any kind: its own `#if` already
// compiles to an empty TU (bar the guarded includes) on those configs.
//
// SHADER_SOURCE_ROOT / ENGINE_ASSETS_DIR — NEEDED DIRECTLY, NOT JUST VIA THE
// PCH: Flux.cpp, Flux/Slang/Flux_ShaderHotReload.cpp and
// Flux/Slang/Flux_SlangCompiler.cpp all reference SHADER_SOURCE_ROOT directly
// (unguarded), and Flux/Particles/Flux_Particles.cpp references
// ENGINE_ASSETS_DIR directly. Both defines previously existed only on the
// aggregate ZenithProject and each GameProject; this is the first leaf lib
// whose OWN compiled TUs (not merely a PCH-transitive include) reference
// either one, so both must be set here explicitly — same win64/agde branch,
// same values, as ZenithProject's (Sharpmake_Zenith.cs) and mirroring how
// Sharpmake_ZenithDebugVariables.cs already had to do this for
// SHADER_SOURCE_ROOT alone (it only reaches the define via an #include, never
// a compiled reference of its own).
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithFlux".
[Sharpmake.Generate]
public class ZenithFluxLibProject : ZenithBaseProject
{
	public ZenithFluxLibProject()
	{
		Name = "ZenithFlux";

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

		SourceRootPath = RootPath + "/Zenith/Flux";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithFlux\[target.Platform]\[conf.Name]";

		// Per-lib PCH: ZenithFlux CREATEs Zenith.pch from Internal/Zenith_FluxPCH.cpp.
		conf.PrecompHeader = "Zenith.h";
		conf.PrecompSource = "Zenith_FluxPCH.cpp";

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);

		// SLANG — conditional on win64 && Vulkan ONLY. See the file header
		// comment. No AddGlfwLibs/AddVulkanBackendLibs/AddD3D12Libs call: this
		// lib is renderer-agnostic (see the DEPENDENCY paragraph) and must not
		// link or propagate glfw3_mt/vulkan-1/d3d12/dxgi/dxguid.
		if (target.Platform == Platform.win64 && target.RenderBackend == RenderBackend.Vulkan)
		{
			AddSlangLibs(conf, target);
		}

		// The PCH (Zenith.h) transitively pulls imgui + Flux/Vulkan/vma/Jolt
		// headers, and Flux's own headers (Flux_Backend.h, Flux_Fwd.h,
		// Backend/Flux_IndirectDraw.h) reference Vulkan types directly —
		// include paths only, no libs linked (no direct vkXxx(...) calls exist
		// in any Flux .cpp, see the DEPENDENCY paragraph).
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		conf.Output = Configuration.OutputType.Lib;

		// MANDATORY: ZenithCore — see Sharpmake_ZenithCore.cs's header comment.
		conf.AddPublicDependency<ZenithCoreLibProject>(target);

		// Direct edges — see the file header comment's DEPENDENCY paragraph.
		conf.AddPublicDependency<ZenithMathsLibProject>(target);
		conf.AddPublicDependency<ZenithDataStreamLibProject>(target);
		conf.AddPublicDependency<ZenithFileAccessLibProject>(target);
		conf.AddPublicDependency<ZenithProfilingLibProject>(target);
		conf.AddPublicDependency<ZenithDebugVariablesLibProject>(target);
		conf.AddPublicDependency<ZenithTaskSystemLibProject>(target);
		conf.AddPublicDependency<ZenithECSLibProject>(target);

		// Flux -> AssetREGISTRY (Phase 4i): the LOWER half of the old
		// AssetHandling, which no longer reaches Flux from any header it owns —
		// so this edge is a plain downward dependency, not the heavier
		// direction of a cycle. See the file header comment's CYCLE paragraph.
		// ZenithAssetHandling (the GPU half) depends on THIS project, not the
		// other way round; do not re-add an edge to it here.
		conf.AddPublicDependency<ZenithAssetRegistryLibProject>(target);

		// SHADER_SOURCE_ROOT / ENGINE_ASSETS_DIR — see the file header comment.
		// Mirrors ZenithProject's (Sharpmake_Zenith.cs) identical win64/agde
		// branch exactly (same value, same "" on agde) so every other build.
		string zenithRoot = Path.GetFullPath(Path.Combine(SharpmakeCsPath, "..")).Replace('\\', '/');
		string engineAssetRoot = zenithRoot + "/Zenith/Assets/";
		if (target.Platform == Platform.win64)
		{
			conf.Defines.Add($"ENGINE_ASSETS_DIR=\"{engineAssetRoot}\"");
			string shaderSourceRoot = zenithRoot + "/Zenith/Flux/Shaders/";
			conf.Defines.Add($"SHADER_SOURCE_ROOT=\"{shaderSourceRoot}\"");
		}
		else if (target.Platform == Platform.agde)
		{
			// On Android, assets/shaders are bundled into the APK by Gradle;
			// AAssetManager expects relative paths, so both roots are empty —
			// identical to ZenithProject's own agde branch.
			conf.Defines.Add("ENGINE_ASSETS_DIR=\"\"");
			conf.Defines.Add("SHADER_SOURCE_ROOT=\"\"");
		}

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
