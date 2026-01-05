using Sharpmake;
using System;
using System.IO;

// Tools enabled fragment for editor/tools builds
[Fragment, Flags]
public enum ToolsEnabled
{
	True = 1,
	False = 2
}

// Custom target supporting both Windows and Android platforms
public class ZenithTarget : ITarget
{
	public Platform Platform;
	public DevEnv DevEnv;
	public Optimization Optimization;
	public ToolsEnabled ToolsEnabled;

	// Android-specific fragments (required for AGDE platform)
	public Android.AndroidBuildTargets AndroidBuildTargets;
}

// Base project class with common configuration for all Zenith projects
public abstract class ZenithBaseProject : Project
{
	protected new static string RootPath = @"[project.SharpmakeCsPath]/..";

	public ZenithBaseProject() : base(typeof(ZenithTarget))
	{
		SourceFilesExtensions = new Strings(".cpp", ".c", ".h", ".vert", ".frag", ".comp", ".tese", ".tesc", ".geom", ".fxh");
		SourceFilesCompileExtensions = new Strings(".cpp", ".c");
	}

	// Configure platform-specific file excludes (called during configuration)
	// Uses conf.SourceFilesBuildExcludeRegex for configuration-level excludes
	protected void ConfigurePlatformExcludes(Configuration conf, ZenithTarget target)
	{
		if (target.Platform == Platform.win64)
		{
			// Windows build: exclude Android platform files from compilation
			conf.SourceFilesBuildExcludeRegex.Add(@".*\\Android\\.*");
			conf.SourceFilesBuildExcludeRegex.Add(@".*_Android.*");
		}
		else if (target.Platform == Platform.agde)
		{
			// Android build: exclude Windows platform files and editor/tools from compilation
			conf.SourceFilesBuildExcludeRegex.Add(@".*\\Windows\\.*");
			conf.SourceFilesBuildExcludeRegex.Add(@".*_Windows.*");
			conf.SourceFilesBuildExcludeRegex.Add(@".*\\Editor\\.*");
			conf.SourceFilesBuildExcludeRegex.Add(@".*\\Tools\\.*");
		}
	}

	protected void ConfigureCommonSettings(Configuration conf, ZenithTarget target)
	{
		conf.Options.Add(Options.Vc.Compiler.CppLanguageStandard.CPP20);
		conf.Defines.Add("GLM_ENABLE_EXPERIMENTAL");
		conf.Defines.Add("NOMINMAX");

		// Platform defines
		if (target.Platform == Platform.win64)
		{
			conf.Defines.Add("ZENITH_WINDOWS");
		}
		else if (target.Platform == Platform.agde)
		{
			conf.Defines.Add("ZENITH_ANDROID");
		}

		conf.Defines.Add("ZENITH_VULKAN");

		if (target.Optimization == Optimization.Debug)
		{
			conf.Defines.Add("ZENITH_DEBUG");
		}
	}

	protected void ConfigureCommonIncludePaths(Configuration conf, ZenithTarget target)
	{
		conf.IncludePaths.Add(RootPath + "/Zenith");
		conf.IncludePaths.Add(RootPath + "/Zenith/Core");
		conf.IncludePaths.Add(RootPath + "/Middleware/glm-master");
		conf.IncludePaths.Add(RootPath + "/Middleware/stb");
		conf.IncludePaths.Add(RootPath + "/Middleware");

		if (target.Platform == Platform.win64)
		{
			conf.IncludePaths.Add(RootPath + "/Middleware/glfw-3.4.bin.WIN64/include");
			conf.IncludePaths.Add(RootPath + "/Middleware/VulkanSDK/1.3.280.0/Include");
			conf.IncludePaths.Add(RootPath + "/Zenith/Windows");
		}
		else if (target.Platform == Platform.agde)
		{
			conf.IncludePaths.Add(RootPath + "/Zenith/Android");
			// Android NDK Vulkan headers are provided by the toolchain
		}
	}

	protected void ConfigureCommonLibraryPaths(Configuration conf, ZenithTarget target)
	{
		if (target.Platform == Platform.win64)
		{
			conf.LibraryPaths.Add(RootPath + "/Middleware/VulkanSDK/1.3.280.0/Lib");
			conf.LibraryPaths.Add(RootPath + "/Middleware/glfw-3.4.bin.WIN64/lib-vc2022");
			conf.LibraryFiles.Add("glfw3_mt.lib");
			conf.LibraryFiles.Add("vulkan-1.lib");
		}
		// Android links against system Vulkan loader, no static libs needed
	}
}

// Main Zenith solution containing all projects
[Sharpmake.Generate]
public class ZenithSolution : Solution
{
	public ZenithSolution() : base(typeof(ZenithTarget))
	{
		Name = "Zenith";

		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False
		});

		// Add Android target separately (no tools)
		AddTargets(new ZenithTarget
		{
			Platform = Platform.agde,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.False,
			AndroidBuildTargets = Android.AndroidBuildTargets.arm64_v8a
		});
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		conf.SolutionFileName = "[solution.Name]_[target.Platform]";
		conf.SolutionPath = @"[solution.SharpmakeCsPath]";

		conf.AddProject<ZenithProject>(target);
		conf.AddProject<TestGameProject>(target);
		conf.AddProject<MarbleGameProject>(target);

		// FluxCompiler is Windows-only
		if (target.Platform == Platform.win64)
		{
			conf.AddProject<FluxCompilerProject>(target);
		}
	}
}

// Entry point for Sharpmake
public static class Main
{
	[Sharpmake.Main]
	public static void SharpmakeMain(Sharpmake.Arguments arguments)
	{
		arguments.Generate<ZenithSolution>();
	}
}
