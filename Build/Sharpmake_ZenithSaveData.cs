using Sharpmake;
using System;
using System.IO;

// ZenithSaveData — leaf static library (Phase 4c — see Sharpmake_ZenithCore.cs
// for the pattern this file replicates). Holds exactly Zenith/SaveData — 1
// translation unit (Zenith_SaveData.cpp: the save-slot read/write/CRC system).
// Previously globbed straight into the monolithic aggregate ZenithProject.
//
// NO PRECOMPILED HEADER: 1-TU library, see Sharpmake_ZenithDebugVariables.cs's
// header comment for the full rationale (identical here). conf.PrecompHeader /
// conf.PrecompSource are deliberately left UNSET — verify NotUsing in the
// generated .vcxproj.
//
// DEPENDENCY: Zenith_SaveData.h #includes DataStream/Zenith_DataStream.h
// (ZenithDataStream) and FileAccess/Zenith_FileAccess.h (ZenithFileAccess —
// ZENITH_MAX_PATH_LENGTH / ZENITH_SAVE_EXT), plus Core/Zenith_Result.h and
// Collections/Zenith_Vector.h (both ZenithCore, already mandatory). The .cpp's
// own #includes (Core/Zenith_CommandLine.h, Core/Zenith_PlatformStdio.h,
// Core/Zenith_Win32.h under ZENITH_WINDOWS, <android_native_app_glue.h> under
// ZENITH_ANDROID) resolve to ZenithCore or system/NDK headers — no new edges.
// So this lib depends on ZenithCore (mandatory) + ZenithDataStream +
// ZenithFileAccess.
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithSaveData".
[Sharpmake.Generate]
public class ZenithSaveDataLibProject : ZenithBaseProject
{
	// Leaf static lib — renderer-agnostic; must not link or propagate
	// glfw/vulkan/slang, so ConfigureAll below calls none of the
	// AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs helpers.
	public ZenithSaveDataLibProject()
	{
		Name = "ZenithSaveData";

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

		SourceRootPath = RootPath + "/Zenith/SaveData";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithSaveData\[target.Platform]\[conf.Name]";

		// No conf.PrecompHeader / conf.PrecompSource: see the file header
		// comment. Zenith_SaveData.cpp includes "Zenith.h" as a plain
		// textual include.

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf lib must not link or propagate the renderer backend.

		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		conf.Output = Configuration.OutputType.Lib;

		// MANDATORY: ZenithCore — see Sharpmake_ZenithCore.cs's header comment.
		conf.AddPublicDependency<ZenithCoreLibProject>(target);

		// Direct edges: Zenith_SaveData.h #includes DataStream/Zenith_DataStream.h
		// and FileAccess/Zenith_FileAccess.h.
		conf.AddPublicDependency<ZenithDataStreamLibProject>(target);
		conf.AddPublicDependency<ZenithFileAccessLibProject>(target);

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
