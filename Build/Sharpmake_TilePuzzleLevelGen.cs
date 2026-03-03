using Sharpmake;
using System;
using System.IO;

// TilePuzzleLevelGen project - Offline level generator tool (Windows only)
[Sharpmake.Generate]
public class TilePuzzleLevelGenProject : ZenithBaseProject
{
	public TilePuzzleLevelGenProject()
	{
		Name = "TilePuzzleLevelGen";
		SourceRootPath = RootPath + "/TilePuzzleLevelGen";

		// Windows-only tool
		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False
		});
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]/../TilePuzzleLevelGen";

		conf.Options.Add(Options.Vc.Compiler.CppLanguageStandard.CPP20);

		// Zenith root path
		string zenithRoot = Path.GetFullPath(Path.Combine(SharpmakeCsPath, "..")).Replace('\\', '/');

		// Output directory define
		string outputDir = zenithRoot + "/TilePuzzleLevelGen/Output/";
		conf.Defines.Add($"LEVELGEN_OUTPUT_DIR=\"{outputDir}\"");
		string registryDir = zenithRoot + "/TilePuzzleLevelGen/LevelRegistry/";
		conf.Defines.Add($"LEVELGEN_REGISTRY_DIR=\"{registryDir}\"");
		conf.Defines.Add("GLM_ENABLE_EXPERIMENTAL");
		conf.Defines.Add("NOMINMAX");
		conf.Defines.Add("_CRT_SECURE_NO_WARNINGS");

		// Platform defines
		conf.Defines.Add("ZENITH_WINDOWS");
		conf.Defines.Add("ZENITH_VULKAN");
		if (target.Optimization == Optimization.Debug)
		{
			conf.Defines.Add("ZENITH_DEBUG");
		}

		// Include paths
		conf.IncludePaths.Add(RootPath + "/Zenith");
		conf.IncludePaths.Add(RootPath + "/Zenith/Core");
		conf.IncludePaths.Add(RootPath + "/Middleware/glm-master");
		conf.IncludePaths.Add(RootPath + "/Middleware/stb");
		conf.IncludePaths.Add(RootPath + "/Middleware");
		conf.IncludePaths.Add(RootPath + "/Zenith/Windows");
		conf.IncludePaths.Add(RootPath + "/Middleware/glfw-3.4.bin.WIN64/include");
		conf.IncludePaths.Add(RootPath + "/Middleware/VulkanSDK/1.3.280.0/Include");
		conf.IncludePaths.Add(RootPath + "/Middleware/slang/include");
		conf.IncludePaths.Add(RootPath + "/Games/TilePuzzle");
		conf.IncludePaths.Add(RootPath + "/Games");

		// Library paths
		conf.LibraryPaths.Add(RootPath + "/Middleware/slang/lib");
		conf.LibraryFiles.Add("slang.lib");

		// Copy Slang DLLs to output directory
		string slangBinPath = Path.GetFullPath(Path.Combine(SharpmakeCsPath, "..", "Middleware", "slang", "bin"));
		conf.EventPostBuild.Add($"xcopy /Y /D \"{slangBinPath}\\slang.dll\" \"$(OutDir)\"");
		conf.EventPostBuild.Add($"xcopy /Y /D \"{slangBinPath}\\slang-glslang.dll\" \"$(OutDir)\"");

		// Output executable
		conf.Output = Configuration.OutputType.Exe;

		// glfw3_mt.lib (from Zenith dependency) is compiled with /MT (release CRT).
		// In debug builds we use /MTd (LIBCMTD), causing a linker conflict.
		if (target.Optimization == Optimization.Debug)
		{
			conf.Options.Add(new Options.Vc.Linker.IgnoreSpecificLibraryNames("LIBCMT"));
		}

		// Add dependency on Zenith library
		conf.AddPublicDependency<ZenithProject>(target);
	}
}
