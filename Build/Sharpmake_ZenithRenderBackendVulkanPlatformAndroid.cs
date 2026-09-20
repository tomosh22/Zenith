using Sharpmake;
using System;
using System.IO;

// ZenithRenderBackendVulkanPlatformAndroid — leaf static library (Phase
// 4h.3 — see Sharpmake_ZenithCore.cs for the pattern this file replicates,
// and Sharpmake_ZenithRenderBackendVulkanPlatformWindows.cs for the win64
// mirror image and the shared "WHY THIS EXISTS" rationale). Holds exactly
// Zenith/RenderBackends/Vulkan/Platform/Android — ONE translation unit
// (Zenith_Android_Vulkan.cpp), implementing
// Zenith_Vulkan_Platform::GetRequiredInstanceExtensions() / CreateSurface()
// (declared in ZenithRenderBackendVulkan's own Vulkan/Zenith_Vulkan_Platform.h)
// via vkCreateAndroidSurfaceKHR() and an `extern`-declared
// Zenith_Android_GetNativeWindow() (implemented in ZenithAndroid's own
// window source, resolved only at the final game .so link — see the
// "UNDECLARED SYMBOL" paragraph below).
//
// AGDE ONLY — no win64 AddTargets call at all, the mirror image of
// ZenithRenderBackendVulkanPlatformWindows' win64-only shape.
// `conf.AddProject`/`conf.AddPublicDependency` THROW at Sharpmake-run time
// if the target being configured is not one the callee itself declares, so
// this project is wired into Build/Sharpmake_EngineLibs.cs with an explicit
// `t => t.Platform == Platform.agde` predicate rather than any consumer
// growing its own `if` — the same shape ZenithAndroidLibProject's own entry
// already uses.
//
// NO PCH: one compiled TU is well under this refactor's >= 4-TU threshold
// for a binary PCH (see Sharpmake_ZenithCore.cs's header comment for the
// rule). Zenith_Android_Vulkan.cpp still begins with `#include "Zenith.h"`,
// so it resolves normally as a plain header include without /Yc/Yu.
//
// DEPENDENCY: Zenith_Android_Vulkan.cpp #includes
// "Vulkan/Zenith_Vulkan_Platform.h" directly (the interface it implements,
// owned by ZenithRenderBackendVulkan). So this library depends on
// ZenithCore (mandatory) + ZenithRenderBackendVulkan (the header it
// implements, plus the Flux/Vulkan include-path chain).
//
// UNDECLARED SYMBOL, ACCEPTED, SAME PRECEDENT AS EVERY OTHER
// EXTERN-FUNCTION SEAM IN THIS REFACTOR: Zenith_Android_GetNativeWindow()
// is forward-declared `extern` in this TU and implemented in ZenithAndroid's
// own Android window source — but this library does NOT declare a
// ZenithAndroid dependency edge. A static lib never resolves an external
// symbol at its own link step, only the final game .so link does, and the
// composition root every agde game transitively links (the aggregate, via
// ZenithEngineLibs.AddAllToSolution / AddAllAsDependenciesOfAggregate)
// always carries ZenithAndroid too. Identical shape to
// Sharpmake_ZenithInput.cs's own undeclared glfwJoystickIsGamepad(...) call
// and Sharpmake_ZenithRenderBackendVulkan.cs's own "GLFW" paragraph.
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithRenderBackendVulkanPlatformAndroid".
[Sharpmake.Generate]
public class ZenithRenderBackendVulkanPlatformAndroidLibProject : ZenithBaseProject
{
	public ZenithRenderBackendVulkanPlatformAndroidLibProject()
	{
		Name = "ZenithRenderBackendVulkanPlatformAndroid";

		// AGDE ONLY — no win64 AddTargets call at all. See the file header
		// comment for why every call site that adds this project must be
		// guarded by a predicate rather than added unconditionally.
		AddTargets(new ZenithTarget
		{
			Platform = Platform.agde,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan,
			AndroidBuildTargets = ZenithAndroidAbi.All
		});

		SourceRootPath = RootPath + "/Zenith/RenderBackends/Vulkan/Platform/Android";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// No ConfigurePlatformExcludes() call — see the file header comment.
		// This directory holds only Android-named/pathed content, so neither
		// exclusion arm (this project only ever configures agde) would ever
		// find anything to strip.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithRenderBackendVulkanPlatformAndroid\[target.Platform]\[conf.Name]";

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);

		// The include chain this one TU needs (Zenith_Vulkan_Platform.h,
		// vulkan/vulkan_android.h) — mirrors ZenithRenderBackendVulkan's own
		// ConfigureAll exactly, since this file used to compile as part of
		// that same project before the Phase 4h.3 split.
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		conf.Output = Configuration.OutputType.Lib;

		// MANDATORY: ZenithCore — see Sharpmake_ZenithCore.cs's header comment.
		conf.AddPublicDependency<ZenithCoreLibProject>(target);

		// The interface this TU implements, plus its transitive Flux/Vulkan
		// chain — see the file header comment's DEPENDENCY paragraph.
		conf.AddPublicDependency<ZenithRenderBackendVulkanLibProject>(target);

		// No ZENITH_TOOLS branch: this project is ToolsEnabled.False only
		// (agde never carries tools — see AddTargets above).
	}
}
