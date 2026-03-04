using Sharpmake;
using System;
using System.IO;

// TilePuzzleRegistryViewer project - ImGui-based registry browser (Windows only)
[Sharpmake.Generate]
public class TilePuzzleRegistryViewerProject : ZenithBaseProject
{
	public TilePuzzleRegistryViewerProject()
	{
		Name = "TilePuzzleRegistryViewer";
		SourceRootPath = RootPath + "/TilePuzzleRegistryViewer";

		// Include ImGui core sources
		AdditionalSourceRootPaths.Add(RootPath + "/Middleware/imgui-docking");

		// Include only DX11 and Win32 backends
		AdditionalSourceRootPaths.Add(RootPath + "/Middleware/imgui-docking/backends");

		// Exclude all backends except Win32 and DX11
		SourceFilesExcludeRegex.Add(@".*imgui_impl_allegro.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_android.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_dx9.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_dx10.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_dx12.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_glfw.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_glut.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_metal.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_opengl.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_osx.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_sdl.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_vulkan.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_wgpu.*");
		// Exclude ImGui demo (not needed)
		SourceFilesExcludeRegex.Add(@".*imgui_demo\.cpp");
		// Exclude ImGui examples directory
		SourceFilesExcludeRegex.Add(@".*imgui-docking\\examples\\.*");
		SourceFilesExcludeRegex.Add(@".*imgui-docking\\misc\\.*");

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
		conf.ProjectPath = @"[project.SharpmakeCsPath]/../TilePuzzleRegistryViewer";

		conf.Options.Add(Options.Vc.Compiler.CppLanguageStandard.CPP20);

		// Zenith root path
		string zenithRoot = Path.GetFullPath(Path.Combine(SharpmakeCsPath, "..")).Replace('\\', '/');

		// Registry directory define
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
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking/backends");
		conf.IncludePaths.Add(RootPath + "/Games/TilePuzzle");
		conf.IncludePaths.Add(RootPath + "/Games");
		conf.IncludePaths.Add(RootPath + "/TilePuzzleLevelGen");

		// Library paths
		conf.LibraryPaths.Add(RootPath + "/Middleware/slang/lib");
		conf.LibraryFiles.Add("slang.lib");
		conf.LibraryFiles.Add("d3d11.lib");
		conf.LibraryFiles.Add("dxgi.lib");
		conf.LibraryFiles.Add("d3dcompiler.lib");

		// Copy Slang DLLs to output directory
		string slangBinPath = Path.GetFullPath(Path.Combine(SharpmakeCsPath, "..", "Middleware", "slang", "bin"));
		conf.EventPostBuild.Add($"xcopy /Y /D \"{slangBinPath}\\slang.dll\" \"$(OutDir)\"");
		conf.EventPostBuild.Add($"xcopy /Y /D \"{slangBinPath}\\slang-glslang.dll\" \"$(OutDir)\"");

		// Output executable
		conf.Output = Configuration.OutputType.Exe;

		// glfw3_mt.lib (from Zenith dependency) is compiled with /MT (release CRT).
		if (target.Optimization == Optimization.Debug)
		{
			conf.Options.Add(new Options.Vc.Linker.IgnoreSpecificLibraryNames("LIBCMT"));
		}

		// Add dependency on Zenith library
		conf.AddPublicDependency<ZenithProject>(target);
	}
}
