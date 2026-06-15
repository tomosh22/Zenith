using Sharpmake;
using System;
using System.IO;

// SentinelPhysics — leaf-proof executable for the ZenithPhysics extraction.
//
// Proves ZenithPhysics is a self-contained leaf over ZenithBase + ZenithECS +
// Jolt, with NO undefined ENGINE externals. It links EXACTLY zenithphysics.lib +
// zenithecs.lib + zenithbase.lib (the three AddPublicDependency calls below) and
// NOTHING else — no ZenithProject (the aggregate engine lib), no Flux, no AI, no
// game. ZenithPhysics OWNS the Jolt backend, so every JPH:: symbol resolves from
// within zenithphysics.lib. Its single TU (Tests/SentinelPhysics/main.cpp) drives
// the Physics core end-to-end (Initialise -> Update -> Raycast -> Shutdown)
// without ever naming g_xEngine / Flux / AI / a concrete component. If the Physics
// core secretly depended on an engine symbol, THIS link would fail with an
// unresolved external — so a green SentinelPhysics build IS the leaf proof.
//
// Modelled on Sharpmake_SentinelECS.cs (same Exe / win64-False-only shape, same
// PCH-as-plain-include single-TU handling, same leaf-only LinksRendererBackend).
[Sharpmake.Generate]
public class SentinelPhysicsProject : ZenithBaseProject
{
	// Leaf-proof exe — must link ONLY the leaf libs, never the renderer backend.
	protected override bool LinksRendererBackend => false;

	public SentinelPhysicsProject()
	{
		Name = "SentinelPhysics";
		SourceRootPath = RootPath + "/Tests/SentinelPhysics";

		// Win64, runtime-only (ToolsEnabled.False) — a desktop link-proof. The
		// ZENITH_TOOLS / ZENITH_DEBUG_VARIABLES branches of the shared PCH route
		// logging + memory-debug-UI through engine/editor symbols that live in the
		// aggregate, not the leaf libs; a runtime config sheds them so the proof
		// reduces to the documented L0 platform shims in sentinel_platform.cpp.
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

		// Distinct intermediate dir so the sentinel's obj never collides with the
		// aggregate / leaf obj trees (all rooted in Build\).
		conf.IntermediatePath = @"[conf.ProjectPath]\obj\SentinelPhysics\[target.Platform]\[conf.Name]";

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		ConfigureCommonLibraryPaths(conf, target);

		// PCH header transitively pulls imgui + Flux/Vulkan/vma/Jolt headers, and the
		// driver includes Physics/Zenith_Physics.h (Jolt) directly — include paths
		// only (no libs linked beyond the three leaf deps below).
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/Vulkan");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		conf.Output = Configuration.OutputType.Exe;

		// THE WHOLE POINT: link EXACTLY the Physics leaf + the ECS leaf + the base
		// leaf, nothing more. ZenithPhysics re-exports ZenithECS + ZenithBase
		// publicly, so the latter two are technically transitive — stated explicitly
		// to make the "links only these three" contract self-documenting.
		conf.AddPublicDependency<ZenithPhysicsLibProject>(target);
		conf.AddPublicDependency<ZenithECSLibProject>(target);
		conf.AddPublicDependency<ZenithBaseLibProject>(target);
	}
}
