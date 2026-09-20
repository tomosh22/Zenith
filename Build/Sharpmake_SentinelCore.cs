using Sharpmake;
using System;
using System.IO;

// SentinelCore — leaf-proof executable for the Core cluster (Phase 5.2).
//
// Proves ZenithCore + ZenithMaths + ZenithDataStream + ZenithFileAccess link
// with NOTHING above them — not even ZenithECS. This is the floor every other
// sentinel (and every other leaf lib) already stands on: SentinelECS/Physics/AI
// each declare an explicit "MANDATORY: ZenithCore" edge specifically because
// Zenith_LogFileSinkWrite (defined in Zenith.cpp, inside ZenithCore) is called
// from the inline Zenith_LogImpl in the shared Zenith.h PCH header that every
// TU in every project compiles against — so this is the one cluster EVERY
// other sentinel already indirectly proves a slice of. SentinelCore is the
// only one that proves the four-library cluster ALONE, with no ZenithECS (or
// anything else) riding along.
//
// It links EXACTLY zenithcore.lib + zenithmaths.lib + zenithdatastream.lib +
// zenithfileaccess.lib and NOTHING else — no ZenithECS, no ZenithProject, no
// Flux, no Physics, no AI, no game. Its single TU (Tests/SentinelCore/main.cpp)
// drives all four libraries with real, out-of-line calls (logging, a
// DataStream write/read round-trip that allocates through ZenithCore's memory
// manager, a Maths free-function, and a stubbed FileAccess call) without ever
// naming g_xEngine, ZenithECS, Flux, Physics or AI. If any of the four secretly
// depended on something above this cluster, THIS link would fail with an
// unresolved external — so a green SentinelCore build IS the leaf proof.
//
// Modelled on Sharpmake_SentinelECS.cs (same Exe / win64-False-only shape,
// same PCH-as-plain-include single-TU handling, same leaf-only renderer-backend
// abstinence — calls none of the AddGlfwLibs/AddVulkanBackendLibs/
// AddSlangLibs/AddD3D12Libs helpers).
[Sharpmake.Generate]
public class SentinelCoreProject : ZenithBaseProject
{
	public SentinelCoreProject()
	{
		Name = "SentinelCore";
		SourceRootPath = RootPath + "/Tests/SentinelCore";

		// Win64, runtime-only (ToolsEnabled.False) — a desktop link-proof, same
		// shape as every other sentinel. See Sharpmake_SentinelECS.cs's own
		// comment for why a runtime config is what reduces the proof to the
		// documented L0 platform shims in sentinel_platform.cpp.
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

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\SentinelCore\[target.Platform]\[conf.Name]";

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf-proof exe must not link or propagate the renderer backend.

		// PCH header transitively pulls imgui + Flux/Vulkan/vma/Jolt headers —
		// include paths only (no libs linked). Mirrors every other sentinel.
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		conf.Output = Configuration.OutputType.Exe;

		// THE WHOLE POINT: link EXACTLY the four Core-cluster leaves, nothing
		// more. No ZenithECS, no ZenithProject, no Flux, no Physics, no AI, no
		// game deps.
		conf.AddPublicDependency<ZenithCoreLibProject>(target);
		conf.AddPublicDependency<ZenithMathsLibProject>(target);
		conf.AddPublicDependency<ZenithDataStreamLibProject>(target);
		conf.AddPublicDependency<ZenithFileAccessLibProject>(target);

		// No ZENITH_TOOLS branch: this project is ToolsEnabled.False only.
	}
}
