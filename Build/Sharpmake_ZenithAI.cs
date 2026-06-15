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
	// Renderer-agnostic leaf static lib — must not link or propagate glfw/vulkan/slang.
	protected override bool LinksRendererBackend => false;

	public ZenithAILibProject()
	{
		Name = "ZenithAI";

		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan | RenderBackend.D3D12
		});

		AddTargets(new ZenithTarget
		{
			Platform = Platform.agde,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan,
			AndroidBuildTargets = Android.AndroidBuildTargets.arm64_v8a
		});

		// The whole AI directory.
		SourceRootPath = RootPath + "/Zenith/AI";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		ConfigurePlatformExcludes(conf, target);

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
		ConfigureCommonLibraryPaths(conf, target);

		// The PCH (Zenith.h) transitively pulls imgui + Flux/Vulkan/vma/Jolt headers;
		// AI also includes Physics headers (Zenith_Physics.h / _Fwd.h). Include paths
		// only — no libs linked (cross-leaf symbols resolve at the consuming link).
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/Vulkan");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		conf.Output = Configuration.OutputType.Lib;

		// L0 + L1 + sibling-leaf edges. ZenithPhysics re-exports ZenithECS+ZenithBase,
		// so those are technically transitive; stated explicitly to mirror ZenithProject.
		conf.AddPublicDependency<ZenithBaseLibProject>(target);
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
