using Sharpmake;
using System;
using System.IO;

// ZenithRenderBackendVulkanPlatformWindows — leaf static library (Phase 4h.3
// — see Sharpmake_ZenithCore.cs for the pattern this file replicates).
// Holds exactly Zenith/RenderBackends/Vulkan/Platform/Windows — ONE
// translation unit (Zenith_Windows_Vulkan.cpp), implementing
// Zenith_Vulkan_Platform::GetRequiredInstanceExtensions() / CreateSurface()
// (declared in ZenithRenderBackendVulkan's own Vulkan/Zenith_Vulkan_Platform.h)
// via glfwGetRequiredInstanceExtensions() / glfwCreateWindowSurface() and
// Zenith_Window::GetInstance() (ZenithWindows).
//
// WHY THIS EXISTS: Phase 4h measured every generated .vcxproj against the
// four ConfigurePlatformExcludes patterns (\Android\, _Android, \Windows\,
// _Windows) and found exactly ONE genuinely multi-platform tree in the
// entire engine — Zenith/RenderBackends/Vulkan/, which held
// Platform/Windows/ and Platform/Android/ side by side under a single
// project (ZenithRenderBackendVulkan) that spans both win64 and agde. Every
// OTHER call site was dead (the project either never saw a matching file at
// all, or — for ZenithWindows/ZenithAndroid themselves — only ever
// configured the platform whose OWN files matched, so the opposite
// exclusion arm never had anything to strip). This library, plus its
// Android mirror (Sharpmake_ZenithRenderBackendVulkanPlatformAndroid.cs),
// applies the SAME rule this whole refactor has applied everywhere else —
// one library per directory, declaring only the targets it supports —
// instead of leaving a single per-configuration subtraction alive for one
// tree. ZenithRenderBackendVulkan's own constructor now
// MEMBERSHIP-excludes both Platform/ subdirectories (see that file), and
// neither ZenithRenderBackendVulkan nor this library nor its Android
// mirror needs ConfigurePlatformExcludes at all any more — see
// Sharpmake_Common.cs for that method's removal.
//
// WIN64 x RenderBackend.Vulkan ONLY — no agde AddTargets call, mirroring
// ZenithWindows' own win64-only shape (Sharpmake_ZenithWindows.cs). Every
// agde target already carries RenderBackend.Vulkan, so unlike
// ZenithRenderBackendVulkan itself this library does not need to span agde
// too — the Android implementation is a physically separate TU in a
// physically separate directory, owned by the sibling library instead.
//
// NO PCH: one compiled TU is well under this refactor's >= 4-TU threshold
// for a binary PCH (see Sharpmake_ZenithCore.cs's header comment for the
// rule). Zenith_Windows_Vulkan.cpp still begins with `#include "Zenith.h"`,
// so it resolves normally as a plain header include without /Yu.
//
// DEPENDENCY: Zenith_Windows_Vulkan.cpp #includes
// "Vulkan/Zenith_Vulkan_Platform.h" directly (the interface it implements,
// owned by ZenithRenderBackendVulkan) and calls Zenith_Window::GetInstance()
// (ZenithWindows, Zenith/Windows/Zenith_Windows_Window.h — resolved
// transitively through the PCH/include-path chain the same way the parent
// library's own Zenith_Vulkan.cpp/_Swapchain.cpp already read
// Zenith_Window::GetInstance() without a separate #include). So this
// library depends on ZenithCore (mandatory) + ZenithRenderBackendVulkan
// (the header it implements, plus the Flux/Vulkan/vma include paths that
// header's own transitive chain needs) + ZenithWindows (Zenith_Window).
//
// GLFW — UNDECLARED SYMBOLS, ACCEPTED, SAME PRECEDENT AS
// ZenithRenderBackendVulkan ITSELF: this TU calls glfwGetRequiredInstanceExtensions()
// / glfwCreateWindowSurface() directly but does not call AddGlfwLibs — the
// composition root every actual exe links (the aggregate) always carries
// ZenithWindows, which owns glfw3_mt.lib, so the symbol resolves at the
// final exe link. Identical reasoning to
// Sharpmake_ZenithRenderBackendVulkan.cs's own "GLFW" paragraph.
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithRenderBackendVulkanPlatformWindows".
[Sharpmake.Generate]
public class ZenithRenderBackendVulkanPlatformWindowsLibProject : ZenithBaseProject
{
	public ZenithRenderBackendVulkanPlatformWindowsLibProject()
	{
		Name = "ZenithRenderBackendVulkanPlatformWindows";

		// WIN64 x RenderBackend.Vulkan ONLY — see the file header comment.
		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan
		});

		SourceRootPath = RootPath + "/Zenith/RenderBackends/Vulkan/Platform/Windows";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// No ConfigurePlatformExcludes() call — see the file header comment.
		// This directory holds only Windows-named/pathed content, so neither
		// exclusion arm (this project only ever configures win64) would ever
		// find anything to strip.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithRenderBackendVulkanPlatformWindows\[target.Platform]\[conf.Name]";

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);

		// The include chain this one TU needs (Zenith_Vulkan_Platform.h,
		// vulkan.hpp via GLFW_INCLUDE_VULKAN, Zenith_Window) — mirrors
		// ZenithRenderBackendVulkan's own ConfigureAll exactly, since this
		// file used to compile as part of that same project before the
		// Phase 4h.3 split.
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

		// Zenith_Window::GetInstance() — see the file header comment's
		// DEPENDENCY paragraph.
		conf.AddPublicDependency<ZenithWindowsLibProject>(target);

		if (target.ToolsEnabled == ToolsEnabled.True && target.Platform == Platform.win64)
		{
			conf.Defines.Add("ZENITH_TOOLS");
		}
	}
}
