using Sharpmake;
using System;
using System.IO;

// ZenithAndroid — leaf static library (Phase 4b — see Sharpmake_ZenithCore.cs
// for the pattern this file replicates). Holds Zenith/Android — 8 translation
// units (the 5 top-level Zenith_Android_*.cpp + Callstack/Zenith_Android_Callstack.cpp
// + FileAccess/Zenith_Android_FileAccess.cpp + Multithreading/Zenith_Android_Multithreading.cpp)
// + the NDK-vendored android_native_app_glue.c under NativeGlue/ (a plain C
// file with no Zenith.h include — excluded from the PCH create the same way
// the aggregate already excludes it) + the definition-free PCH create TU
// (Internal/Zenith_AndroidPCH.cpp). Previously globbed straight into the
// monolithic aggregate ZenithProject.
//
// AGDE ONLY — THE MIRROR IMAGE OF ZenithWindows' NEW HAZARD: this project
// declares ONLY an agde target set (no win64 AddTargets call at all).
// `conf.AddProject`/`conf.AddPublicDependency` THROW at Sharpmake-run time if
// the target being configured is not one the callee itself declares, so this
// project is wired into Build/Sharpmake_EngineLibs.cs with an explicit
// `t => t.Platform == Platform.agde` predicate rather than any consumer
// growing its own `if`. See Sharpmake_ZenithWindows.cs for the win64-only
// mirror image and the shared rationale.
//
// THE PCH: >= 4 compiled TUs (8, well past the threshold) justifies a binary
// PCH. ZenithAndroid CREATEs its own Zenith.pch from
// Internal/Zenith_AndroidPCH.cpp (a one-line `#include "Zenith.h"`, nothing
// else — must stay definition-free, see that file's own header comment,
// which is the exact reason Zenith_EnginePCH.cpp exists for the aggregate's
// own agde config). Every other TU /Yu's against it. Because this project
// only ever configures for agde, there is no platform branch needed on
// conf.PrecompSource the way the aggregate needs one (win64 keeps
// Zenith_Core.cpp, a definition-bearing TU that only MSVC's /Yc — which
// captures just the header prefix, not the rest of the file — can tolerate).
//
// DEPENDENCY: Zenith_Android_Main.cpp #includes FileAccess/Zenith_FileAccess.h
// and Profiling/Zenith_Profiling.h directly; Zenith_Android_FileAccess.cpp
// #includes FileAccess/Zenith_FileAccess.h; Zenith_Android_Window.cpp
// #includes Input/Zenith_Input.h; Multithreading/Zenith_Android_Multithreading.cpp
// #includes Profiling/Zenith_Profiling.h. So this lib depends on ZenithCore
// (mandatory) + ZenithFileAccess + ZenithProfiling + ZenithInput (all direct
// #include edges).
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithAndroid".
[Sharpmake.Generate]
public class ZenithAndroidLibProject : ZenithBaseProject
{
	public ZenithAndroidLibProject()
	{
		Name = "ZenithAndroid";

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

		// Whole-directory membership, additive: the 8 Zenith_Android_*.cpp
		// (top-level + Callstack/ + FileAccess/ + Multithreading/) plus
		// NativeGlue/android_native_app_glue.c, all under one recursive root.
		SourceRootPath = RootPath + "/Zenith/Android";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithAndroid\[target.Platform]\[conf.Name]";

		// Per-lib PCH: ZenithAndroid CREATEs Zenith.pch from Internal/Zenith_AndroidPCH.cpp.
		conf.PrecompHeader = "Zenith.h";
		conf.PrecompSource = "Zenith_AndroidPCH.cpp";
		// android_native_app_glue.c is a plain C file from the NDK, vendored
		// verbatim — no Zenith.h include, so it must compile WITHOUT the
		// engine PCH. Mirrors the aggregate's own identical exclude.
		conf.PrecompSourceExcludeFolders.Add(RootPath + "/Zenith/Android/NativeGlue");

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// every one of those helpers is win64-only and returns immediately on
		// agde anyway (see Sharpmake_Common.cs) — agde links its system
		// Vulkan loader via --sysroot at the game exe/so link, not through
		// any of these helpers. Omitted here for clarity, matching every
		// other leaf lib's convention of only calling the helpers it needs.

		// The PCH (Zenith.h) transitively pulls Zenith_DebugVariables.h ->
		// imgui.h AND Flux/Flux.h -> Vulkan/vma/Jolt headers.
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		conf.Output = Configuration.OutputType.Lib;

		// MANDATORY: ZenithCore — see Sharpmake_ZenithCore.cs's header comment.
		conf.AddPublicDependency<ZenithCoreLibProject>(target);

		// Direct edges (see the file header comment's DEPENDENCY paragraph).
		conf.AddPublicDependency<ZenithFileAccessLibProject>(target);
		conf.AddPublicDependency<ZenithProfilingLibProject>(target);
		conf.AddPublicDependency<ZenithInputLibProject>(target);

		// No ZENITH_TOOLS branch: this project is ToolsEnabled.False only
		// (agde never carries tools — see AddTargets above).
	}
}
