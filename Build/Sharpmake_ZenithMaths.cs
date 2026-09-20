using Sharpmake;
using System;
using System.IO;

// ZenithMaths — bottom-leaf static library (Phase 4a: ZenithBase dissolution).
// Holds exactly Zenith/Maths — 1 translation unit (Zenith_Maths.cpp: the free
// projection/rotation-matrix helpers). Previously part of the monolithic
// ZenithBase; see Sharpmake_ZenithCore.cs's header comment for why the
// dissolution happened.
//
// NO PRECOMPILED HEADER: the >= 4-TU rule this refactor applies means a
// 1-TU library gains nothing from a binary PCH — it would pay a full glm
// parse building the PCH for a single consumer of it, worse than just
// textually including Zenith.h once. conf.PrecompHeader / conf.PrecompSource
// are deliberately left UNSET; Zenith_Maths.cpp compiles `#include "Zenith.h"`
// textually, exactly like the Sentinels and the game executables already do.
// (Verify in the generated .vcxproj: no PrecompiledHeader Create/Use here.)
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithMaths".
[Sharpmake.Generate]
public class ZenithMathsLibProject : ZenithBaseProject
{
	// L0 leaf static lib — renderer-agnostic; must not link or propagate
	// glfw/vulkan/slang, so ConfigureAll below calls none of the
	// AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs helpers.
	public ZenithMathsLibProject()
	{
		Name = "ZenithMaths";

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

		SourceRootPath = RootPath + "/Zenith/Maths";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		// Distinct intermediate dir — never share obj\[platform]\[conf] with
		// another lib (each of those CREATEs or otherwise touches its own
		// PCH/obj set).
		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithMaths\[target.Platform]\[conf.Name]";

		// No conf.PrecompHeader / conf.PrecompSource: see the file header
		// comment. Zenith_Maths.cpp includes "Zenith.h" as a plain textual
		// include.

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf lib must not link or propagate the renderer backend.

		// Zenith.h (included textually, not via PCH — the header graph it
		// pulls in is identical either way) transitively reaches
		// Zenith_DebugVariables.h → imgui.h and Flux/Flux.h → Vulkan/vma/Jolt
		// headers, so this TU needs the same extra include paths ZenithCore
		// needs for its PCH compile. Include paths only — ZenithMaths links
		// none of these libs. Carried over unconditionally per the Phase 4a
        // plan (start conservative, trim only once verified unnecessary);
		// not yet empirically trimmed.
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

		// Tools define: ZenithMaths carries no tools-only code, but Zenith.h
		// branches on ZENITH_TOOLS and the three lockstep defines
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
