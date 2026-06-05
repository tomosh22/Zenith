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
		SourceFilesExtensions = new Strings(".cpp", ".c", ".h", ".inl", ".slang");
		SourceFilesCompileExtensions = new Strings(".cpp", ".c");

		// Strip generated-artifact directories from project membership.
		// `.slang` discovery used to pick up Slang's auto-extracted standard
		// modules under `output/.../slang-standard-module-*/*.slang` because
		// those files live below SourceRootPath; their presence varies per
		// configuration and disappears on a clean checkout, so they don't
		// belong in the project. Same applies to obj/, .vs/, .git/, and the
		// AGDE / Gradle build trees under Android/app/build/intermediates —
		// all pure generated state.
		SourceFilesExcludeRegex.Add(@"\\output\\");
		SourceFilesExcludeRegex.Add(@"\\obj\\");
		SourceFilesExcludeRegex.Add(@"\\\.vs\\");
		SourceFilesExcludeRegex.Add(@"\\\.git\\");
		SourceFilesExcludeRegex.Add(@"\\Android\\app\\build\\");
		SourceFilesExcludeRegex.Add(@"\\Android\\\.gradle\\");
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
		conf.Options.Add(Options.Vc.General.TreatWarningsAsErrors.Enable);
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
			// vulkan.hpp: exceptions are disabled on Android, use ResultValue returns
			conf.Defines.Add("VULKAN_HPP_NO_EXCEPTIONS");
			// Set C++20 for AGDE (Options.Vc.Compiler.CppLanguageStandard.CPP20 only works for Win64)
			conf.Options.Add(Options.Agde.Compiler.CppLanguageStandard.Cpp20);
			// Vulkan 1.1 functions require API 30+ (Android 11)
			conf.Options.Add(Options.Android.General.AndroidAPILevel.Android30);
			// Cap VMA at Vulkan 1.1 - Android NDK libvulkan.so doesn't export 1.2/1.3 symbols
			conf.Defines.Add("VMA_VULKAN_VERSION=1001000");
			// Windows-host cross-compile: the AGDE platform emits the include paths in
			// lowercase (e.g. ...\zenith\core, ...\middleware) while the on-disk dirs are
			// Zenith\Core, Middleware. clang's -Wnonportable-include-path then flags every
			// "Core/..."/"Middleware/..." #include as a case mismatch, and the AGDE default
			// -Werror turns it fatal -- a false alarm on the case-INSENSITIVE Windows host
			// (the lowercase paths resolve to the real files fine). Disable ONLY that one
			// warning; every other -Werror diagnostic stays fatal.
			conf.AdditionalCompilerOptions.Add("-Wno-nonportable-include-path");
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
			conf.IncludePaths.Add(RootPath + "/Middleware/slang/include");
			conf.IncludePaths.Add(RootPath + "/Zenith/Windows");
		}
		else if (target.Platform == Platform.agde)
		{
			conf.IncludePaths.Add(RootPath + "/Zenith/Android");
			// android_native_app_glue.h lives in NativeGlue subfolder
			conf.IncludePaths.Add(RootPath + "/Zenith/Android/NativeGlue");
			// Vulkan C++ headers (vulkan.hpp) from SDK - header-only, works with NDK's Vulkan
			conf.IncludePaths.Add(RootPath + "/Middleware/VulkanSDK/1.3.280.0/Include");
		}
	}

	// True for projects that actually link the renderer backend (the aggregate
	// engine lib + the games). FALSE for the L0/L1 leaf static libs (ZenithBase,
	// ZenithECS) and the SentinelECS leaf-proof exe: those are renderer-agnostic
	// and must NOT link OR PROPAGATE glfw/vulkan/slang. Propagating those libs out
	// of the leaf static libs is exactly what let the SentinelECS link silently
	// resolve an accidental ECS->renderer reference, defeating the leaf proof.
	// (win64-only knob; agde links its system libs via Sharpmake_Games.cs.)
	protected virtual bool LinksRendererBackend => true;

	protected void ConfigureCommonLibraryPaths(Configuration conf, ZenithTarget target)
	{
		if (target.Platform == Platform.win64 && LinksRendererBackend)
		{
			conf.LibraryPaths.Add(RootPath + "/Middleware/VulkanSDK/1.3.280.0/Lib");
			conf.LibraryPaths.Add(RootPath + "/Middleware/glfw-3.4.bin.WIN64/lib-vc2022");
			conf.LibraryPaths.Add(RootPath + "/Middleware/slang/lib");
			conf.LibraryFiles.Add("glfw3_mt.lib");
			conf.LibraryFiles.Add("vulkan-1.lib");
			conf.LibraryFiles.Add("slang.lib");

			// glfw3_mt.lib is compiled with /MT (release CRT), which pulls in LIBCMT.
			// In debug builds we use /MTd (LIBCMTD), causing a linker conflict.
			if (target.Optimization == Optimization.Debug)
			{
				conf.Options.Add(new Options.Vc.Linker.IgnoreSpecificLibraryNames("LIBCMT"));
			}
		}
		// Android links against system Vulkan loader via --sysroot (see Sharpmake_Games.cs)
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

		// ZenithBase: bottom-leaf static lib (L0 set), carved out of the
		// monolithic engine lib in Wave-20 stage 1. ZenithProject publicly
		// depends on it, so it would be pulled into the solution transitively;
		// adding it explicitly keeps it visible/buildable on its own.
		conf.AddProject<ZenithBaseLibProject>(target);
		// ZenithECS: L1 static lib (ECS-core set), carved out in the ECS-leaf
		// extraction. Depends on ZenithBase; ZenithProject publicly depends on
		// it, so it too would arrive transitively — added explicitly to keep it
		// visible/buildable on its own.
		conf.AddProject<ZenithECSLibProject>(target);
		conf.AddProject<ZenithProject>(target);
		// SentinelECS: ECS leaf-proof EXE (Phase 9b). Links ONLY zenithecs.lib +
		// zenithbase.lib; building it green proves the ECS core has no undefined
		// engine externals. Win64 + ToolsEnabled.False ONLY (the project declares
		// only those two configs -- a runtime build sheds the ZENITH_TOOLS /
		// ZENITH_DEBUG_VARIABLES editor instrumentation that would otherwise pull
		// engine symbols). The guard must match the project's target set, else
		// solution-link fails ("cannot find target ... in project SentinelECS").
		if (target.Platform == Platform.win64 && target.ToolsEnabled == ToolsEnabled.False)
		{
			conf.AddProject<SentinelECSProject>(target);
		}
		conf.AddProject<TestGameProject>(target);
		conf.AddProject<MarbleGameProject>(target);
		conf.AddProject<RunnerGameProject>(target);
		conf.AddProject<CombatGameProject>(target);
		conf.AddProject<ExplorationGameProject>(target);
		conf.AddProject<RenderTestGameProject>(target);
		conf.AddProject<SurvivalGameProject>(target);
		conf.AddProject<TilePuzzleGameProject>(target);
		conf.AddProject<AIShowcaseGameProject>(target);
		conf.AddProject<DevilsPlaygroundGameProject>(target);
		conf.AddProject<CityBuilderGameProject>(target);

		// Windows-only tools
		if (target.Platform == Platform.win64)
		{
			conf.AddProject<FluxCompilerProject>(target);
			conf.AddProject<TilePuzzleLevelGenProject>(target);
			conf.AddProject<TilePuzzleRegistryViewerProject>(target);

			// MSDF font deps — only present in tools-enabled builds (used by
			// Zenith_Tools_FontExport for the offline atlas bake at engine init).
			if (target.ToolsEnabled == ToolsEnabled.True)
			{
				conf.AddProject<FreeTypeProject>(target);
				conf.AddProject<MsdfgenProject>(target);
				conf.AddProject<MsdfAtlasGenProject>(target);
			}
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
