using Sharpmake;
using System;
using System.IO;

// Zenith Engine core project
[Sharpmake.Generate]
public class ZenithProject : ZenithBaseProject
{
	public ZenithProject()
	{
		Name = "Zenith";
		SourceRootPath = RootPath + "/Zenith";

		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False
		});

		AddTargets(new ZenithTarget
		{
			Platform = Platform.agde,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.False,
			AndroidBuildTargets = Android.AndroidBuildTargets.arm64_v8a
		});

		// Common excludes for all platforms
		SourceFilesExcludeRegex.Add(@".*VulkanSDK.*");
		SourceFilesExcludeRegex.Add(@".*FluxCompiler.*");
		SourceFilesExcludeRegex.Add(@".*glm-master.*");

		// Jolt Physics excludes
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\Build.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\Docs.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\UnitTests.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\Samples.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\TestFramework.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\PerformanceTest.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\JoltViewer.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\HelloWorld.*");

		// ImGui excludes (non-Vulkan backends, examples)
		SourceFilesExcludeRegex.Add(@".*imgui-docking\\examples.*");
		SourceFilesExcludeRegex.Add(@".*imgui-docking\\backends\\imgui_impl_sdl.*");
		SourceFilesExcludeRegex.Add(@".*imgui-docking\\backends\\imgui_impl_opengl.*");
		SourceFilesExcludeRegex.Add(@".*imgui-docking\\backends\\imgui_impl_dx.*");
		SourceFilesExcludeRegex.Add(@".*imgui-docking\\backends\\imgui_impl_glut.*");
		SourceFilesExcludeRegex.Add(@".*imgui-docking\\backends\\imgui_impl_wgpu.*");
		SourceFilesExcludeRegex.Add(@".*imgui-docking\\backends\\imgui_impl_allegro.*");
		SourceFilesExcludeRegex.Add(@".*imgui-docking\\misc.*");

		SourceFilesExcludeRegex.Add(@".*cmake.*");

		// Include Jolt Physics and ImGui source directories
		AdditionalSourceRootPaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		AdditionalSourceRootPaths.Add(RootPath + "/Middleware/imgui-docking");

		// Include Tools source directory (compiled as part of engine, not separate library)
		AdditionalSourceRootPaths.Add(RootPath + "/Tools");

		// Tools middleware excludes
		SourceFilesExcludeRegex.Add(@".*assimp-5.4.2\\test.*");
		SourceFilesExcludeRegex.Add(@".*assimp-5.4.2\\tools.*");
		SourceFilesExcludeRegex.Add(@".*assimp-5.4.2\\port.*");
		SourceFilesExcludeRegex.Add(@".*zstream_test.*");
		SourceFilesExcludeRegex.Add(@".*zfstream.cpp.*");
		SourceFilesExcludeRegex.Add(@".*zip\\test.*");
		SourceFilesExcludeRegex.Add(@".*inflate86.*");
		SourceFilesExcludeRegex.Add(@".*minizip.*");
		SourceFilesExcludeRegex.Add(@".*iostream\\test.*");
		SourceFilesExcludeRegex.Add(@".*blast\\.*");
		SourceFilesExcludeRegex.Add(@".*puff\\.*");
		SourceFilesExcludeRegex.Add(@".*testzlib\\.*");
		SourceFilesExcludeRegex.Add(@".*untgz\\.*");
		SourceFilesExcludeRegex.Add(@".*opencv.*");
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Configure platform-specific excludes
		ConfigurePlatformExcludes(conf, target);

		// Android also excludes ImGui Android backend (we use custom implementation)
		if (target.Platform == Platform.agde)
		{
			conf.SourceFilesBuildExcludeRegex.Add(@".*imgui-docking\\backends\\imgui_impl_android.*");
			// Exclude editor on Android
			conf.SourceFilesBuildExcludeRegex.Add(@".*Editor\\.*");
		}

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		// Precompiled header
		conf.PrecompHeader = "Zenith.h";
		conf.PrecompSource = "Zenith.cpp";
		conf.PrecompSourceExcludeFolders.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.PrecompSourceExcludeFolders.Add(RootPath + "/Middleware/imgui-docking");

		// Common configuration
		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		ConfigureCommonLibraryPaths(conf, target);

		// Additional include paths
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/Vulkan");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		// Zenith root path - absolute path computed from Sharpmake location
		// Use Path.GetFullPath to canonicalize and remove ".." components
		string zenithRoot = Path.GetFullPath(Path.Combine(SharpmakeCsPath, "..")).Replace('\\', '/');

		// Asset paths use absolute paths so they work regardless of working directory
		string engineAssetRoot = zenithRoot + "/Zenith/Assets/";
		conf.Defines.Add($"ENGINE_ASSETS_DIR=\"{engineAssetRoot}\"");

		// Shader source path for runtime compilation (Windows only)
		if (target.Platform == Platform.win64)
		{
			string shaderSourceRoot = zenithRoot + "/Zenith/Flux/Shaders/";
			conf.Defines.Add($"SHADER_SOURCE_ROOT=\"{shaderSourceRoot}\"");
		}

		// Output type
		if (target.Platform == Platform.win64)
		{
			conf.Output = Configuration.OutputType.Lib;

			// DbgHelp library for callstack capture (used by memory tracking)
			conf.LibraryFiles.Add("dbghelp.lib");
			// PSApi library for process memory info (GetProcessMemoryInfo)
			conf.LibraryFiles.Add("psapi.lib");
		}
		else if (target.Platform == Platform.agde)
		{
			conf.Output = Configuration.OutputType.Lib;
		}

		// Tools configuration (compiled as part of engine when enabled)
		if (target.ToolsEnabled == ToolsEnabled.True && target.Platform == Platform.win64)
		{
			conf.Defines.Add("ZENITH_TOOLS");
			conf.Defines.Add("OPENDDLPARSER_BUILD");

			// ZENITH_ROOT define for tools code to construct paths at runtime
			conf.Defines.Add($"ZENITH_ROOT=\"{zenithRoot}/\"");

			// Tools include paths
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/assimp/include");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/opencv/build/include");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/opencv/build/include/opencv2");

			// Tools library paths and dependencies
			conf.LibraryPaths.Add(RootPath + "/Tools/Middleware/opencv/build/x64/vc16/lib");
			conf.LibraryPaths.Add(RootPath + "/Tools/Middleware/assimp/lib");

			// Exclude Tools from precompiled header
			conf.PrecompSourceExcludeFolders.Add(RootPath + "/Tools");

			if (target.Optimization == Optimization.Debug)
			{
				conf.LibraryFiles.Add("opencv_world4100d.lib");
				conf.LibraryFiles.Add("assimp-vc143-mtd.lib");
			}
			else
			{
				conf.LibraryFiles.Add("opencv_world4100.lib");
				conf.LibraryFiles.Add("assimp-vc143-mt.lib");
			}
		}
		else
		{
			// Exclude Tools source files when tools are disabled
			conf.SourceFilesBuildExcludeRegex.Add(@".*\\Tools\\.*");
		}
	}
}
