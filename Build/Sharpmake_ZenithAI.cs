using Sharpmake;
using System;
using System.IO;

// ZenithAI — the AI leaf static library. Carved out of the monolithic engine lib
// the same way ZenithECS (L1), ZenithPhysics, and ZenithBase (L0) were, WITHOUT
// changing runtime behaviour. It holds the AI CORE: behaviour trees, navigation
// (navmesh + pathfinding + agent), perception, squad tactics, the blackboard, and
// the AI debug-variable toggles.
//
// LAYERING: ZenithAI is a strict leaf over ZenithBase (L0) + ZenithECS (L1) +
// ZenithPhysics (AI uses Zenith_Physics::Get() raycasts; Physics is a sibling
// lower leaf). It must NOT name g_xEngine, Flux, UI, Editor, AssetHandling, Prefab,
// or any concrete component — those engine-side needs are routed through the
// Zenith_AIWorldHooks seam (AI/Zenith_AIWorldHooks.h), wired by the engine. The
// concrete Zenith_AIAgentComponent + the hook-install glue + the navmesh geometry
// collector live engine-side in Zenith/EntityComponent. Proven by SentinelAI +
// the dumpbin FORBIDDEN_EXTERNALS scan.
//
// THE PCH: like the other leaf libs, ZenithAI compiles its OWN per-lib Zenith.pch
// from a trivial Create TU (Internal/Zenith_AIPCH.cpp = `#include "Zenith.h"`).
[Sharpmake.Generate]
public class ZenithAILibProject : ZenithBaseProject
{
	// Renderer-agnostic leaf static lib — must not link or propagate
	// glfw/vulkan/slang, so ConfigureAll below calls none of the
	// renderer-backend library helpers.
	public ZenithAILibProject()
	{
		Name = "ZenithAI";

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

		// The whole AI directory.
		SourceRootPath = RootPath + "/Zenith/AI";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		// Distinct intermediate dir so this lib's .obj + private Zenith.pch never
		// collide with the other libs (all rooted in Build\).
		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithAI\[target.Platform]\[conf.Name]";

		// Per-lib PCH: ZenithAI CREATEs Zenith.pch from Zenith_AIPCH.cpp.
		conf.PrecompHeader = "Zenith.h";
		conf.PrecompSource = "Zenith_AIPCH.cpp";

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf lib must not link or propagate the renderer backend.

		// Phase 5.3 CORRECTION: the comment this replaced claimed "the PCH
		// (Zenith.h) transitively pulls imgui + Flux/Vulkan/vma/Jolt headers".
		// Zenith.h itself pulls none of them (grep: its closure is 17 project
		// headers + ~202 glm headers, zero imgui/Flux/Vulkan/vma/Jolt). The
		// Jolt path below is real, but for a DIFFERENT and narrower reason:
		// five files here (Zenith_NavMeshAgent.cpp, Zenith_NavMeshGenerator.Tests.inl,
		// Zenith_PerceptionSystem.cpp, Zenith_TacticalPoint.cpp,
		// Zenith_AIWorldHooks.h) #include "Physics/Zenith_Physics.h", which itself
		// #includes <Jolt/Jolt.h> + <Jolt/Physics/Collision/ContactListener.h>
		// directly — so THIS path must stay (verified: removing it breaks the
		// build). vma/RenderBackends/imgui-docking had no such justification —
		// grep finds zero imgui/vulkan/vma references anywhere under Zenith/AI —
		// and are removed, verified by a green SentinelAI build in every
		// RenderBackend x win64 plus a green `zenith build engine`.
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");

		conf.Output = Configuration.OutputType.Lib;

		// L0 + L1 + sibling-leaf edges. ZenithCore is MANDATORY for every
		// engine lib (see Sharpmake_ZenithCore.cs — Zenith_LogFileSinkWrite
		// must resolve for every TU). ZenithMaths/ZenithDataStream/
		// ZenithFileAccess are direct dependencies too — the AI core's own
		// TUs use Zenith_Maths types heavily (steering/perception geometry),
		// DataStream (behaviour-tree / blackboard serialization) and
		// FileAccess (asset loading seams). ZenithPhysics re-exports
		// ZenithECS + all four L0 leaves, so those edges are technically
		// transitive; stated explicitly to mirror ZenithProject and to
		// document AI's actual direct usage.
		conf.AddPublicDependency<ZenithCoreLibProject>(target);
		conf.AddPublicDependency<ZenithMathsLibProject>(target);
		conf.AddPublicDependency<ZenithDataStreamLibProject>(target);
		conf.AddPublicDependency<ZenithFileAccessLibProject>(target);
		conf.AddPublicDependency<ZenithECSLibProject>(target);
		conf.AddPublicDependency<ZenithPhysicsLibProject>(target);

		// Tools define: ZenithAI's debug-draw code is ZENITH_TOOLS-gated, so the binary
		// .pch + the True-config compile must carry the define. Mirrors the other leaves.
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
