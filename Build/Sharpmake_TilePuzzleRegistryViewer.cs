using Sharpmake;
using System;
using System.IO;

// TilePuzzleRegistryViewer project - ImGui-based registry browser (Windows only)
//
// Phase 4d — DUPLICATE-OWNED FIX: this project used to glob
// Middleware/imgui-docking itself (the core TUs) on top of AddPublicDependency<ZenithProject>,
// which already linked zenith.lib carrying its own copy of the SAME imgui core
// TUs — check_project_membership.ps1 reported this as genuine same-executable
// duplication (see Sharpmake_ZenithHub.cs's identical header comment for the
// full count). The five core TUs now belong to ZenithImGui
// (Sharpmake_ZenithImGui.cs) alone; this project's own imgui-docking
// (non-backends) source root is REMOVED. imgui_impl_win32.cpp is ALSO removed
// from this project's own backends glob — it is owned by ZenithWindows,
// reached transitively via AddPublicDependency<ZenithProject>. Only
// imgui_impl_dx11.cpp remains this project's OWN backend TU. The imgui-docking
// *include* paths are kept (unaffected).
[Sharpmake.Generate]
public class TilePuzzleRegistryViewerProject : ZenithBaseProject
{
	public TilePuzzleRegistryViewerProject()
	{
		Name = "TilePuzzleRegistryViewer";
		SourceRootPath = RootPath + "/TilePuzzleRegistryViewer";

		// DX11 backend only — win32 now comes from ZenithWindows and the core
		// TUs from ZenithImGui, both transitively via AddPublicDependency<ZenithProject>
		// below (see the file header comment).
		AdditionalSourceRootPaths.Add(RootPath + "/Middleware/imgui-docking/backends");

		// Exclude every backend except DX11.
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
		SourceFilesExcludeRegex.Add(@".*imgui_impl_win32.*");

		// Windows-only tool
		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan
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
		if (target.ToolsEnabled == ToolsEnabled.True)
		{
			conf.Defines.Add("ZENITH_TOOLS");
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

		// Copy ALL Slang runtime DLLs to output directory. MVP-0.0.5 changed
		// from `slang.dll` to `*.dll` -- slang.dll loads its own dependency
		// tree (slang-rt, slang-glslang, slang-glsl-module, slang-llvm,
		// slang-compiler, gfx) at startup and STATUS_DLL_NOT_FOUND fires
		// if those aren't beside the exe.
		string slangBinPath = Path.GetFullPath(Path.Combine(SharpmakeCsPath, "..", "Middleware", "slang", "bin"));
		conf.EventPostBuild.Add($"xcopy /Y /D \"{slangBinPath}\\*.dll\" \"$(OutDir)\"");

		// Output executable
		conf.Output = Configuration.OutputType.Exe;

		// glfw3_mt.lib (from Zenith dependency) is compiled with /MT (release CRT).
		if (target.Optimization == Optimization.Debug)
		{
			conf.Options.Add(new Options.Vc.Linker.IgnoreSpecificLibraryNames("LIBCMT"));
		}

		// Add dependency on Zenith library
		conf.AddPublicDependency<ZenithProject>(target);

		// Explicit edge for legibility, even though ZenithProject already
		// re-exports it (ZenithEngineLibs.All -> ZenithImGuiLibProject) — see
		// the file header comment's DUPLICATE-OWNED FIX paragraph. Supplies
		// the five core imgui TUs this project's own DX11 backend TU links
		// against (ImGui::* calls).
		conf.AddPublicDependency<ZenithImGuiLibProject>(target);
	}
}
