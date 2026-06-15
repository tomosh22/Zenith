using Sharpmake;
using System;
using System.IO;

// SentinelAI — leaf-proof executable for the ZenithAI extraction.
//
// Proves ZenithAI is a self-contained leaf over ZenithBase + ZenithECS +
// ZenithPhysics, with NO undefined ENGINE externals. It links EXACTLY
// zenithai.lib + zenithphysics.lib + zenithecs.lib + zenithbase.lib (the four
// AddPublicDependency calls below) and NOTHING else — no ZenithProject (the
// aggregate), no Flux, no game. The AI core reaches the engine only through the
// Zenith_AIWorldHooks function-pointer seam (null here -> safe no-ops). Its single
// TU (Tests/SentinelAI/main.cpp) drives the AI core (navmesh build + pathfind +
// generator + blackboard + agent + manager tick) without ever naming g_xEngine /
// Flux / a concrete component. If the AI core secretly depended on an engine
// symbol, THIS link would fail — so a green SentinelAI build IS the leaf proof.
//
// Modelled on Sharpmake_SentinelPhysics.cs / Sharpmake_SentinelECS.cs.
[Sharpmake.Generate]
public class SentinelAIProject : ZenithBaseProject
{
	protected override bool LinksRendererBackend => false;

	public SentinelAIProject()
	{
		Name = "SentinelAI";
		SourceRootPath = RootPath + "/Tests/SentinelAI";

		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan | RenderBackend.D3D12
		});
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		ConfigurePlatformExcludes(conf, target);

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\SentinelAI\[target.Platform]\[conf.Name]";

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		ConfigureCommonLibraryPaths(conf, target);

		// PCH header transitively pulls imgui + Flux/Vulkan/vma/Jolt headers; the
		// driver includes Physics/AI headers. Include paths only (no libs beyond the
		// four leaf deps below).
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/Vulkan");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		conf.Output = Configuration.OutputType.Exe;

		// THE WHOLE POINT: link EXACTLY the AI leaf + its leaf chain, nothing more.
		// ZenithAI re-exports ZenithPhysics -> ZenithECS -> ZenithBase publicly, so
		// the latter three are technically transitive — stated explicitly to make the
		// "links only these four" contract self-documenting.
		conf.AddPublicDependency<ZenithAILibProject>(target);
		conf.AddPublicDependency<ZenithPhysicsLibProject>(target);
		conf.AddPublicDependency<ZenithECSLibProject>(target);
		conf.AddPublicDependency<ZenithBaseLibProject>(target);
	}
}
