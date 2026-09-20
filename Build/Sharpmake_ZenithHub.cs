using Sharpmake;
using System;
using System.IO;

// ZenithHub project -- Unity-Hub-style launcher (ImGui + Win32 + D3D11, no Vulkan
// renderer dependency). Structural clone of TilePuzzleRegistryViewer. Windows-only.
// Engine-sln membership only (added by Sharpmake_Solutions.cs on Vulkan configs).
//
// Phase 4d — DUPLICATE-OWNED FIX: this project used to glob
// Middleware/imgui-docking itself (the core TUs) on top of AddPublicDependency<ZenithProject>,
// which already linked zenith.lib carrying its own copy of the SAME imgui core
// TUs — check_project_membership.ps1 reported this as genuine same-executable
// duplication (10 (source, exe) pairs across ZenithHub + TilePuzzleRegistryViewer,
// x4 configs = the 40 DUPLICATE findings this refactor removes). The five core
// TUs now belong to ZenithImGui (Sharpmake_ZenithImGui.cs) alone; this project's
// own imgui-docking (non-backends) source root is REMOVED. imgui_impl_win32.cpp
// is ALSO removed from this project's own backends glob for the same reason —
// it is owned by ZenithWindows (Sharpmake_ZenithWindows.cs), reached transitively
// via AddPublicDependency<ZenithProject> (ZenithEngineLibs.All adds ZenithWindows
// for every win64 target, and this project only ever builds win64). Only
// imgui_impl_dx11.cpp remains this project's OWN backend TU — nobody else
// compiles a DX11 backend. The imgui-docking *include* paths are kept (both
// core headers and backends/ headers still resolve from here, unaffected).
[Sharpmake.Generate]
public class ZenithHubProject : ZenithBaseProject
{
	public ZenithHubProject()
	{
		Name = "ZenithHub";
		SourceRootPath = RootPath + "/ZenithHub";

		// DX11 backend only — win32 now comes from ZenithWindows and the core
		// TUs from ZenithImGui, both transitively via AddPublicDependency<ZenithProject>
		// below (see the file header comment).
		AdditionalSourceRootPaths.Add(RootPath + "/Middleware/imgui-docking/backends");

		// Keep only the DX11 backend.
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

		// Windows-only tool (Vulkan configs; matches the engine it links).
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
		conf.ProjectPath = @"[project.SharpmakeCsPath]/../ZenithHub";

		conf.Options.Add(Options.Vc.Compiler.CppLanguageStandard.CPP20);

		string zenithRoot = Path.GetFullPath(Path.Combine(SharpmakeCsPath, "..")).Replace('\\', '/');

		// Baked repo root for descriptor scanning + CLI shelling.
		conf.Defines.Add($"ZENITH_ROOT=\"{zenithRoot}/\"");
		conf.Defines.Add("GLM_ENABLE_EXPERIMENTAL");
		conf.Defines.Add("NOMINMAX");
		conf.Defines.Add("_CRT_SECURE_NO_WARNINGS");

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
		conf.IncludePaths.Add(RootPath + "/ZenithHub");

		conf.LibraryPaths.Add(RootPath + "/Middleware/slang/lib");
		conf.LibraryFiles.Add("slang.lib");
		conf.LibraryFiles.Add("d3d11.lib");
		conf.LibraryFiles.Add("dxgi.lib");
		conf.LibraryFiles.Add("d3dcompiler.lib");
		conf.LibraryFiles.Add("bcrypt.lib");

		// Copy the Slang runtime DLL tree next to the exe (the linked engine loads
		// it at startup; same rationale as FluxCompiler / the games).
		string slangBinPath = Path.GetFullPath(Path.Combine(SharpmakeCsPath, "..", "Middleware", "slang", "bin"));
		conf.EventPostBuild.Add($"xcopy /Y /D \"{slangBinPath}\\*.dll\" \"$(OutDir)\"");

		conf.Output = Configuration.OutputType.Exe;

		if (target.Optimization == Optimization.Debug)
		{
			conf.Options.Add(new Options.Vc.Linker.IgnoreSpecificLibraryNames("LIBCMT"));
		}

		conf.AddPublicDependency<ZenithProject>(target);

		// Explicit edge for legibility, even though ZenithProject already
		// re-exports it (ZenithEngineLibs.All -> ZenithImGuiLibProject) — see
		// the file header comment's DUPLICATE-OWNED FIX paragraph. Supplies
		// the five core imgui TUs this project's own DX11 backend TU links
		// against (ImGui::* calls).
		conf.AddPublicDependency<ZenithImGuiLibProject>(target);
	}
}
