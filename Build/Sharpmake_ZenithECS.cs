using Sharpmake;
using System;
using System.IO;

// ZenithECS — the L1 static library (ECS-leaf extraction, Phase 8). Carved out
// of the monolithic engine lib the same way the old ZenithBase (L0) was,
// WITHOUT changing runtime behaviour. ZenithBase was itself dissolved in
// Phase 4a into four single-directory leaf libs (ZenithCore/ZenithMaths/
// ZenithDataStream/ZenithFileAccess — see Sharpmake_ZenithCore.cs); ZenithECS
// now depends on those four directly instead. It holds the ECS CORE only: the
// contents of
// Zenith/EntityComponent EXCEPT the engine-side glue that physically lives in
// that directory but belongs to the aggregate engine lib (the concrete
// Components/ subtree, the editor "Add Component" registry, the component-meta
// registration TU, and the main-camera resolver — all of which name concrete
// components / Flux types and so cannot live in the leaf).
//
// The kept ECS-core .cpp set is:
//   Internal/Zenith_SceneSystem_Registry.cpp
//   Internal/Zenith_SceneSystem_Operations.cpp
//   Internal/Zenith_SceneSystem_Lifecycle.cpp
//   Internal/Zenith_SceneSystem_Callbacks.cpp
//   Internal/Zenith_SceneSystem_EntityOwnership.cpp
//   Zenith_ComponentMeta.cpp
//   Zenith_Entity.cpp
//   Zenith_EventSystem.cpp
//   Zenith_Scene.cpp
//   Zenith_SceneData.cpp
//   Zenith_SceneData_Serialization.cpp
//   Zenith_ECS.cpp            (the PCH-create TU, added for this lib)
//
// LAYERING: ZenithECS depends on the four L0 leaf libs (ZenithCore/
// ZenithMaths/ZenithDataStream/ZenithFileAccess). It does NOT depend on the
// aggregate engine lib — that is the whole point of the leaf-extraction. The
// L0->L1 edges are the four AddPublicDependency<...> calls below.
//
// THE PCH: like ZenithCore, this lib compiles its OWN per-lib Zenith.pch from a
// trivial Create TU (Zenith_ECS.cpp, which is just `#include "Zenith.h"`). The
// binary .pch cannot be shared across projects that compile with different
// flags (e.g. ZENITH_TOOLS), so every consumer builds its own from the shared
// header. ZenithECS CREATEs Zenith.pch (PrecompiledHeader=Create on
// Zenith_ECS.cpp); its other TUs compile /Yu"Zenith.h" against it.
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithECS".
//
// NOTE ON SOURCE PARTITION: Phase 9 partitioned the ECS-core .cpp set from the
// aggregate — ZenithProject's ConfigureAll build-EXCLUDEs the whole
// Zenith/ZenithECS/ subtree, so the ECS-core compiles ONLY here, never in
// both libs (no duplicate-symbol state).
[Sharpmake.Generate]
public class ZenithECSLibProject : ZenithBaseProject
{
	// L1 leaf static lib (ECS core) — renderer-agnostic; must not link or
	// propagate glfw/vulkan/slang (that is the whole point of the leaf
	// boundary), so ConfigureAll below calls none of the renderer-backend
	// library helpers.
	public ZenithECSLibProject()
	{
		Name = "ZenithECS";

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

		// The whole ECS directory. Membership is then narrowed to the ECS-core
		// set by the engine-side excludes below. The "Zenith.h" PrecompHeader
		// lookup resolves through ConfigureCommonIncludePaths (which adds
		// /Zenith + /Zenith/Core), exactly as it does for the aggregate.
		SourceRootPath = RootPath + "/Zenith/ZenithECS";

		// The ECS leaf was physically relocated into Zenith/ZenithECS/, so this
		// source root contains EXACTLY the ECS-core set -- there are no engine-side
		// files here to exclude. The files that USE the ECS but are NOT part of the
		// leaf (the concrete Components/ subtree, Zenith_ComponentMeta_Registration.cpp,
		// Zenith_CameraResolve.cpp, and the editor Zenith_ComponentEditorRegistry.cpp) stay
		// behind in Zenith/EntityComponent/ and compile into the aggregate engine lib.
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		// Distinct intermediate dir. ZenithECS, ZenithBase and the aggregate
		// Zenith all live in the Build dir and otherwise default to the SAME obj
		// tree (obj\[platform]\[conf]) — fatal because each CREATEs a Zenith.pch
		// there and they would clobber each other (MSB8028 + PCH-collision).
		// Give ZenithECS its own obj subtree so its .obj + Zenith.pch are
		// private (mirrors ZenithBase's objZenithBase).
		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithECS\[target.Platform]\[conf.Name]";

		// Per-lib PCH: ZenithECS CREATEs Zenith.pch from Zenith_ECS.cpp. Every
		// other ZenithECS TU compiles with /Yu"Zenith.h" against it.
		conf.PrecompHeader = "Zenith.h";
		conf.PrecompSource = "Zenith_ECS.cpp";

		// Common configuration — identical knobs to the engine lib so the ECS
		// TUs compile under the exact same flags/defines/include paths they did
		// inside the monolith (no behavioural delta).
		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf lib must not link or propagate the renderer backend.

		// Phase 5.3 CORRECTION: the comment this replaced claimed "the PCH
		// (Zenith.h) transitively pulls imgui + Flux/Vulkan/vma/Jolt headers".
		// It does not — grep against Zenith/Core/Zenith.h finds zero imgui.h /
		// Flux.h / Vulkan / Jolt / vma includes anywhere in its closure (the
		// closure is 17 project headers plus ~202 glm headers), and
		// Zenith/ZenithECS/**/*.{h,cpp} itself references none of those four
		// vendored trees either (grep: zero hits). All four IncludePaths.Add
		// calls this comment used to justify were therefore dead weight, not
		// verified-necessary — proven by removing them and getting a green
		// SentinelECS build in every RenderBackend (Vulkan_/Null_/D3D12_) x
		// win64, plus a green `zenith build engine` (Phase 5 verification).
		// Deleted rather than kept "just in case".

		conf.Output = Configuration.OutputType.Lib;

		// Depend on the four Phase 4a leaf libs that replaced ZenithBase (the
		// L0 set). This is the L0->L1 edge. PUBLIC so the edge re-exports
		// through dependents the same way ZenithProject re-exports each of
		// them. ZenithECS uses all four directly (FileAccess in the scene
		// serializer's file I/O seam, DataStream heavily throughout scene/
		// component serialization, Maths for transform math, Core for
		// logging/asserts/collections) — see Sharpmake_ZenithCore.cs for why
		// ZenithCore specifically is a MANDATORY dependency of every lib.
		conf.AddPublicDependency<ZenithCoreLibProject>(target);
		conf.AddPublicDependency<ZenithMathsLibProject>(target);
		conf.AddPublicDependency<ZenithDataStreamLibProject>(target);
		conf.AddPublicDependency<ZenithFileAccessLibProject>(target);

		// Tools define: ZenithECS carries no tools-only code, but the PCH it
		// compiles (Zenith.h) branches on ZENITH_TOOLS, so the binary .pch must
		// be built with the same define as the consumers in a True config —
		// otherwise the per-lib PCH would diverge. Keep it in lockstep with the
		// engine lib (mirrors ZenithBase).
		if (target.ToolsEnabled == ToolsEnabled.True && target.Platform == Platform.win64)
		{
			conf.Defines.Add("ZENITH_TOOLS");

			// The ZENITH_TOOLS PCH branch may pull tools/editor headers; mirror
			// the aggregate's tools include paths so the per-lib PCH resolves
			// identically in True. Include paths only (no tool libs linked).
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/assimp/include");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/freetype/include");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen/msdfgen");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen/msdfgen/Config");

			// Exclude Tools from the PCH create (no tools sources here anyway).
			conf.PrecompSourceExcludeFolders.Add(RootPath + "/Tools");
		}
	}
}
