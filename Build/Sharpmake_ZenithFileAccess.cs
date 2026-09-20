using Sharpmake;
using System;
using System.IO;

// ZenithFileAccess — bottom-leaf static library (Phase 4a: ZenithBase
// dissolution). Holds exactly the top-level Zenith/FileAccess directory — 1
// translation unit (Zenith_FileAccess.cpp). Previously part of the monolithic
// ZenithBase; see Sharpmake_ZenithCore.cs's header comment for why the
// dissolution happened.
//
// NOT the platform implementation: Zenith/Windows/FileAccess/
// (Zenith_Windows_FileAccess.cpp) is a SIBLING directory under Zenith/Windows,
// not under Zenith/FileAccess, so SourceRootPath below never sees it — it
// stays compiled into the aggregate ZenithProject, unchanged. Do not add
// Zenith/Windows as a root here.
//
// NO PRECOMPILED HEADER: the >= 4-TU rule this refactor applies means a
// 1-TU library gains nothing from a binary PCH. conf.PrecompHeader /
// conf.PrecompSource are deliberately left UNSET; the TU compiles
// `#include "Zenith.h"` textually, exactly like the Sentinels and the game
// executables already do. (Verify in the generated .vcxproj: no
// PrecompiledHeader Create/Use here.)
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithFileAccess".
[Sharpmake.Generate]
public class ZenithFileAccessLibProject : ZenithBaseProject
{
	// L0 leaf static lib — renderer-agnostic; must not link or propagate
	// glfw/vulkan/slang, so ConfigureAll below calls none of the
	// AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs helpers.
	public ZenithFileAccessLibProject()
	{
		Name = "ZenithFileAccess";

		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan | RenderBackend.D3D12 | RenderBackend.Null
		});

		AddTargets(new ZenithTarget
		{
			Platform = Platform.agde,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan,
			AndroidBuildTargets = ZenithAndroidAbi.All
		});

		SourceRootPath = RootPath + "/Zenith/FileAccess";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithFileAccess\[target.Platform]\[conf.Name]";

		// No conf.PrecompHeader / conf.PrecompSource: see the file header
		// comment. The TU includes "Zenith.h" as a plain textual include.

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf lib must not link or propagate the renderer backend.

		// Zenith.h (textual include; same header graph as the PCH path)
		// transitively reaches Zenith_DebugVariables.h → imgui.h and
		// Flux/Flux.h → Vulkan/vma/Jolt headers, so this TU needs the same
		// extra include paths ZenithCore needs for its PCH compile. Include
		// paths only — ZenithFileAccess links none of these libs. Carried
		// over unconditionally per the Phase 4a plan (start conservative,
		// trim only once verified unnecessary); not yet empirically trimmed.
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		conf.Output = Configuration.OutputType.Lib;

		// MANDATORY: ZenithCore is an unconditional dependency of every
		// engine library (see Sharpmake_ZenithCore.cs's header comment) —
		// Zenith_LogFileSinkWrite, referenced from the inline Zenith_LogImpl
		// in the shared Zenith.h, must resolve for every TU that includes it,
		// this one included.
		conf.AddPublicDependency<ZenithCoreLibProject>(target);

		// Tools define: ZenithFileAccess carries no tools-only code, but
		// Zenith.h branches on ZENITH_TOOLS and the three lockstep defines
		// (ZENITH_TOOLS / ZENITH_PROFILING_ENABLED / ZENITH_MEMORY_TRACKING_LEVEL)
		// must be identical across every consumer (ODR) — so this define is
		// set even though nothing in this lib's own source reads it.
		if (target.ToolsEnabled == ToolsEnabled.True && target.Platform == Platform.win64)
		{
			conf.Defines.Add("ZENITH_TOOLS");

			conf.IncludePaths.Add(RootPath + "/Tools/Middleware");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/assimp/include");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/freetype/include");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen/msdfgen");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen/msdfgen/Config");
		}
	}
}
