using Sharpmake;
using System;
using System.IO;

// SentinelFlux — leaf-proof executable for the Flux cluster (Phase 5.2).
//
// Proves ZenithFlux + ZenithAssetHandling + the ACTIVE render backend link
// without reaching Editor/EntityComponent/UI. Unlike SentinelECS/Physics/
// AI/Scripting, this sentinel is deliberately NOT renderer-abstinent: the
// whole point of "the Flux cluster" is the render-backend boundary, so it
// adds the target's active backend project (ZenithRenderBackendVulkan /
// ZenithRenderBackendD3D12 / ZenithRenderBackendNull — each declares ONLY
// its own RenderBackend value in its own AddTargets, so the edge is
// conditional on target.RenderBackend below, the same predicated-entry
// pattern Sharpmake_EngineLibs.cs already uses). It links ZenithFlux's own
// declared dependency set (Sharpmake_ZenithFlux.cs's DEPENDENCY paragraph:
// ZenithCore, ZenithMaths, ZenithDataStream, ZenithFileAccess, ZenithProfiling,
// ZenithDebugVariables, ZenithTaskSystem, ZenithECS, ZenithAssetHandling) plus
// the active backend, and NOTHING else — no ZenithUI, no ZenithEntityComponent,
// no ZenithEditor, no ZenithProject, no game.
//
// GPU-FREE BY DESIGN: the sentinel proves LINKING, not live rendering. Its
// single TU (Tests/SentinelFlux/main.cpp) drives Flux with a real, out-of-line,
// GPU-free call (Zenith_RenderBus::RecordDrawCall — grep confirms zero
// g_xEngine references in Zenith/Flux/Zenith_RenderBus.cpp, unlike almost
// every other Flux top-level .cpp) rather than attempting to create a live
// device/window, which the render backend's own API requires and which a
// headless link-proof exe has no business doing. The active-backend
// dependency is therefore present in the link (proving the declared edge
// resolves with no undefined symbol) without main() calling a
// backend-specific function directly — see the file's own comment at the
// conditional AddPublicDependency calls below for why that is still an
// honest proof, not a vacuous one: ZenithRenderBackendVulkan itself calls
// AddVulkanBackendLibs in ITS OWN ConfigureAll, so depending on it still
// pulls vulkan-1.lib/slang.lib into this exe's link inputs on a Vulkan
// target, exercising the same "does the backend's OWN declared link set
// resolve" question the leaf-proof pattern asks everywhere else.
[Sharpmake.Generate]
public class SentinelFluxProject : ZenithBaseProject
{
	public SentinelFluxProject()
	{
		Name = "SentinelFlux";
		SourceRootPath = RootPath + "/Tests/SentinelFlux";

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

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\SentinelFlux\[target.Platform]\[conf.Name]";

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// Deliberately NO AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/
		// AddD3D12Libs call HERE: those are called inside the active render
		// backend project's OWN ConfigureAll (see the file header comment).
		// Calling them again here would not be wrong, but it would blur the
		// "the backend project's own declared link set is what resolves"
		// framing this sentinel exists to test.

		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		// ENGINE_ASSETS_DIR / SHADER_SOURCE_ROOT — ZenithFlux's own TUs
		// reference both directly (Flux.cpp, Flux/Slang/*, Flux/Particles/
		// Flux_Particles.cpp — see Sharpmake_ZenithFlux.cs's own header
		// comment), so linking zenithflux.lib needs both defined here too,
		// same win64-only branch (this sentinel declares no agde target).
		string zenithRoot = Path.GetFullPath(Path.Combine(SharpmakeCsPath, "..")).Replace('\\', '/');
		conf.Defines.Add($"ENGINE_ASSETS_DIR=\"{zenithRoot}/Zenith/Assets/\"");
		conf.Defines.Add($"SHADER_SOURCE_ROOT=\"{zenithRoot}/Zenith/Flux/Shaders/\"");

		conf.Output = Configuration.OutputType.Exe;

		// ZenithFlux's own declared dependency set (mirrors
		// Sharpmake_ZenithFlux.cs's DEPENDENCY paragraph exactly).
		conf.AddPublicDependency<ZenithFluxLibProject>(target);
		conf.AddPublicDependency<ZenithAssetHandlingLibProject>(target);
		conf.AddPublicDependency<ZenithCoreLibProject>(target);
		conf.AddPublicDependency<ZenithMathsLibProject>(target);
		conf.AddPublicDependency<ZenithDataStreamLibProject>(target);
		conf.AddPublicDependency<ZenithFileAccessLibProject>(target);
		conf.AddPublicDependency<ZenithProfilingLibProject>(target);
		conf.AddPublicDependency<ZenithDebugVariablesLibProject>(target);
		conf.AddPublicDependency<ZenithTaskSystemLibProject>(target);
		conf.AddPublicDependency<ZenithECSLibProject>(target);

		// THE ACTIVE RENDER BACKEND — conditional, because each backend
		// project declares ONLY its own RenderBackend value in its own
		// AddTargets (ZenithRenderBackendVulkanLibProject declares Vulkan
		// only, etc.) — an unconditional AddPublicDependency here would throw
		// at Sharpmake-run time for the other two targets, exactly the
		// reason every predicated row in Sharpmake_EngineLibs.cs exists.
		if (target.RenderBackend == RenderBackend.Vulkan)
		{
			conf.AddPublicDependency<ZenithRenderBackendVulkanLibProject>(target);
		}
		else if (target.RenderBackend == RenderBackend.D3D12)
		{
			conf.AddPublicDependency<ZenithRenderBackendD3D12LibProject>(target);
		}
		else if (target.RenderBackend == RenderBackend.Null)
		{
			conf.AddPublicDependency<ZenithRenderBackendNullLibProject>(target);
		}

		// No ZENITH_TOOLS branch: this project is ToolsEnabled.False only.
	}
}
