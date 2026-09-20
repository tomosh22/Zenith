using Sharpmake;
using System;
using System.IO;

// SentinelAI — leaf-proof executable for the ZenithAI extraction.
//
// Proves ZenithAI is a self-contained leaf over ZenithCore (formerly
// ZenithBase, dissolved Phase 4a) + ZenithECS + ZenithPhysics, with NO
// undefined ENGINE externals. It links EXACTLY
// zenithai.lib + zenithphysics.lib + zenithecs.lib + zenithcore.lib (the four
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
	// Leaf-proof exe — must link ONLY the leaf libs, never the renderer
	// backend, so ConfigureAll below calls none of the renderer-backend
	// library helpers.
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
			RenderBackend = RenderBackend.Vulkan | RenderBackend.D3D12 | RenderBackend.Null
		});
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\SentinelAI\[target.Platform]\[conf.Name]";

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf-proof exe must not link or propagate the renderer backend.

		// PCH header transitively pulls imgui + Flux/Vulkan/vma/Jolt headers; the
		// driver includes Physics/AI headers. Include paths only (no libs beyond the
		// four leaf deps below).
		// Phase 5.3 CORRECTION: vma/RenderBackends/imgui-docking removed -- grep
		// finds zero imgui/vulkan/vma references anywhere this sentinel's
		// main.cpp or the leaf lib it links reaches. The Jolt path stays: the
		// leaf's own headers #include <Jolt/...> directly. Verified by a green
		// build in every RenderBackend x win64.
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");

		conf.Output = Configuration.OutputType.Exe;

		// THE WHOLE POINT: link EXACTLY the AI leaf + its leaf chain, nothing more.
		// ZenithAI re-exports ZenithPhysics -> ZenithECS -> ZenithCore (+
		// ZenithMaths/ZenithDataStream/ZenithFileAccess) publicly, so the
		// ZenithCore edge here is technically transitive — stated explicitly
		// per the mandatory rule (Sharpmake_ZenithCore.cs:
		// Zenith_LogFileSinkWrite must resolve for every TU) and to make the
		// "links only these four" contract self-documenting.
		conf.AddPublicDependency<ZenithAILibProject>(target);
		conf.AddPublicDependency<ZenithPhysicsLibProject>(target);
		conf.AddPublicDependency<ZenithECSLibProject>(target);
		conf.AddPublicDependency<ZenithCoreLibProject>(target);
	}
}
