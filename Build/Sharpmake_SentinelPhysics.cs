using Sharpmake;
using System;
using System.IO;

// SentinelPhysics — leaf-proof executable for the ZenithPhysics extraction.
//
// Proves ZenithPhysics is a self-contained leaf over ZenithCore (formerly
// ZenithBase, dissolved Phase 4a) + ZenithECS + ZenithJolt (the vendored Jolt
// backend, split into its own leaf library at Phase 4c — see
// Sharpmake_ZenithJolt.cs), with NO undefined ENGINE externals. It links
// EXACTLY zenithphysics.lib + zenithecs.lib + zenithcore.lib + zenithjolt.lib
// (the four AddPublicDependency calls below) and NOTHING else — no
// ZenithProject (the aggregate engine lib), no Flux, no AI, no game.
// ZenithPhysics no longer OWNS the Jolt backend as of Phase 4c (it used to,
// via AdditionalSourceRootPaths); it now LINKS it as a declared dependency,
// so every JPH:: symbol resolves from zenithjolt.lib instead of from inside
// zenithphysics.lib itself — a stricter proof than before the split, since a
// missing edge here now fails exactly the way a missing engine edge would.
// Its single TU (Tests/SentinelPhysics/main.cpp) drives the Physics core
// end-to-end (Initialise -> Update -> Raycast -> Shutdown) without ever
// naming g_xEngine / Flux / AI / a concrete component. If the Physics core
// secretly depended on an engine symbol, THIS link would fail with an
// unresolved external — so a green SentinelPhysics build IS the leaf proof.
//
// Modelled on Sharpmake_SentinelECS.cs (same Exe / win64-False-only shape, same
// PCH-as-plain-include single-TU handling, same leaf-only renderer-backend
// abstinence -- calls none of the AddGlfwLibs/AddVulkanBackendLibs/
// AddSlangLibs/AddD3D12Libs helpers).
[Sharpmake.Generate]
public class SentinelPhysicsProject : ZenithBaseProject
{
	// Leaf-proof exe — must link ONLY the leaf libs, never the renderer
	// backend, so ConfigureAll below calls none of the renderer-backend
	// library helpers.
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
			RenderBackend = RenderBackend.Vulkan | RenderBackend.D3D12 | RenderBackend.Null
		});
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		// Distinct intermediate dir so the sentinel's obj never collides with the
		// aggregate / leaf obj trees (all rooted in Build\).
		conf.IntermediatePath = @"[conf.ProjectPath]\obj\SentinelPhysics\[target.Platform]\[conf.Name]";

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf-proof exe must not link or propagate the renderer backend.

		// PCH header transitively pulls imgui + Flux/Vulkan/vma/Jolt headers, and the
		// driver includes Physics/Zenith_Physics.h (Jolt) directly — include paths
		// only (no libs linked beyond the three leaf deps below).
		// Phase 5.3 CORRECTION: vma/RenderBackends/imgui-docking removed -- grep
		// finds zero imgui/vulkan/vma references anywhere this sentinel's
		// main.cpp or the leaf lib it links reaches. The Jolt path stays: the
		// leaf's own headers #include <Jolt/...> directly. Verified by a green
		// build in every RenderBackend x win64.
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");

		conf.Output = Configuration.OutputType.Exe;

		// THE WHOLE POINT: link EXACTLY the Physics leaf + the ECS leaf +
		// ZenithCore + ZenithJolt, nothing more. ZenithPhysics re-exports
		// ZenithECS + ZenithCore/ZenithMaths/ZenithDataStream/ZenithFileAccess
		// + (as of Phase 4c) ZenithJolt publicly, so the ZenithCore and
		// ZenithJolt edges here are technically transitive — stated explicitly
		// per the mandatory rule (Sharpmake_ZenithCore.cs: Zenith_LogFileSinkWrite
		// must resolve for every TU) and to make the "links only these"
		// contract self-documenting. The ZenithJolt edge is new at Phase 4c:
		// before the Jolt split every JPH:: symbol resolved from inside
		// zenithphysics.lib itself, so this link did not need it named; now
		// that Jolt is its own archive, omitting this edge would still LINK
		// (the transitive re-export covers it) but would misstate what this
		// sentinel actually proves — that a leaf-only link set, listed in
		// full, resolves with nothing hidden.
		conf.AddPublicDependency<ZenithPhysicsLibProject>(target);
		conf.AddPublicDependency<ZenithECSLibProject>(target);
		conf.AddPublicDependency<ZenithCoreLibProject>(target);
		conf.AddPublicDependency<ZenithJoltLibProject>(target);
	}
}
