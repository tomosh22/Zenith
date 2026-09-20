using Sharpmake;
using System;
using System.IO;

// SentinelScripting — leaf-proof executable for ZenithScripting (Phase 5.2).
//
// Proves the Behaviour Graph interpreter is a self-contained leaf over
// ZenithCore + ZenithDataStream + ZenithMaths + ZenithECS (its declared
// dependency set — see Sharpmake_ZenithScripting.cs's own DEPENDENCY
// paragraph), with NO undefined ENGINE externals. It links EXACTLY
// zenithscripting.lib + zenithecs.lib + zenithdatastream.lib +
// zenithmaths.lib + zenithcore.lib (the five AddPublicDependency calls
// below) and NOTHING else — no ZenithProject, no Flux, no Physics, no AI, no
// UI, no Editor, no game. Its single TU (Tests/SentinelScripting/main.cpp)
// drives the Scripting core with real, out-of-line calls (a
// Zenith_GraphBlackboard value round-trip, plus bringing up the ECS
// SceneSystem to exercise the one direct ZenithECS edge
// Zenith_BehaviourGraph.cpp itself declares — ResolveTargetEntity) without
// ever naming g_xEngine / Flux / Physics / AI / a concrete component. If the
// Scripting core secretly depended on an engine symbol, THIS link would fail
// with an unresolved external — so a green SentinelScripting build IS the
// leaf proof.
//
// Modelled on Sharpmake_SentinelECS.cs (same Exe / win64-False-only shape,
// same PCH-as-plain-include single-TU handling, same leaf-only
// renderer-backend abstinence).
[Sharpmake.Generate]
public class SentinelScriptingProject : ZenithBaseProject
{
	public SentinelScriptingProject()
	{
		Name = "SentinelScripting";
		SourceRootPath = RootPath + "/Tests/SentinelScripting";

		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan | RenderBackend.D3D12 | RenderBackend.Null
		});
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\SentinelScripting\[target.Platform]\[conf.Name]";

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf-proof exe must not link or propagate the renderer backend.

		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		conf.Output = Configuration.OutputType.Exe;

		// THE WHOLE POINT: link EXACTLY the Scripting leaf + its declared
		// dependency set, nothing more. ZenithScripting re-exports
		// ZenithCore/ZenithMaths/ZenithDataStream/ZenithECS publicly, so the
		// last four edges here are technically transitive — stated
		// explicitly per the mandatory rule (Sharpmake_ZenithCore.cs) and to
		// make the "links only these" contract self-documenting, exactly as
		// every other sentinel already does.
		conf.AddPublicDependency<ZenithScriptingLibProject>(target);
		conf.AddPublicDependency<ZenithECSLibProject>(target);
		conf.AddPublicDependency<ZenithDataStreamLibProject>(target);
		conf.AddPublicDependency<ZenithMathsLibProject>(target);
		conf.AddPublicDependency<ZenithCoreLibProject>(target);

		// No ZENITH_TOOLS branch: this project is ToolsEnabled.False only.
	}
}
