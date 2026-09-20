using Sharpmake;
using System;

// ZenithImGui — leaf static library (Phase 4d — see Sharpmake_ZenithCore.cs for
// the pattern this file replicates, though this library is the odd one out in
// two ways explained below). Holds exactly the FIVE core ImGui translation
// units at the top of Middleware/imgui-docking: imgui.cpp, imgui_demo.cpp,
// imgui_draw.cpp, imgui_tables.cpp, imgui_widgets.cpp (confirmed against what
// zenith_win64.vcxproj / zenith_agde.vcxproj compile today before this split —
// exactly those five, on both platforms). Previously globbed straight into the
// monolithic aggregate ZenithProject via its own
// AdditionalSourceRootPaths(/Middleware/imgui-docking) root.
//
// core ONLY — NONE of backends/ belongs to this project. The whole
// Middleware/imgui-docking/backends/ subtree is membership-excluded below
// (a single directory exclude, cleaner than the aggregate's historical
// per-backend-name regex list, because this project owns NO backend at all,
// not even one). Individual vendor backend TUs are members of whichever
// library owns that platform/graphics API instead:
//   imgui_impl_glfw.cpp / imgui_impl_win32.cpp -> ZenithWindows (win64-only;
//     see Sharpmake_ZenithWindows.cs's "THE TWO IMGUI BACKEND TUS").
//   imgui_impl_vulkan.cpp -> NOT part of this split. It belongs to the
//     Vulkan render-backend library, which is the NEXT phase's work, so it
//     stays owned by the aggregate exactly as before — this project must not
//     disturb its existing handling (it is NOT excluded from the aggregate's
//     own imgui-docking root, and the aggregate's existing agde-only
//     build-exclude for it is untouched).
//   imgui_impl_android.cpp -> compiled by NOTHING today (confirmed: no
//     project globs it after this split, same as before it — kept that way).
//   imgui_impl_dx9/dx10/dx12/allegro/glut/opengl/sdl*/wgpu.cpp -> never
//     compiled by any Zenith project (vendored examples for engines this
//     codebase does not use); excluded here as membership-irrelevant, same
//     "vendored-tree membership exclude" the aggregate already carried.
//   imgui_impl_dx11.cpp -> ZenithHub / TilePuzzleRegistryViewer's own
//     Win32+DX11 tool executables (unaffected by this project; each keeps its
//     own backends/ AdditionalSourceRootPaths + exclude list for that one
//     file — see Sharpmake_ZenithHub.cs / Sharpmake_TilePuzzleRegistryViewer.cs).
//
// examples/ and misc/ are membership-excluded too (carried over verbatim from
// the aggregate's own historical excludes for this vendored tree — 29 .cpp
// across the two directories that were never compiled by anything and must
// not become members of this project either).
//
// NO PRECOMPILED HEADER, EVER — the first way this library is the odd one
// out: imgui is vendored C++ that does NOT #include "Zenith.h" (confirmed:
// zero hits for "Zenith" across the five core TUs), so there is no Zenith
// master header for it to share, exactly the same reasoning
// Sharpmake_ZenithJolt.cs documents for the vendored Jolt tree. The aggregate
// already excludes this directory from ITS OWN PCH create scan
// (`conf.PrecompSourceExcludeFolders.Add(RootPath + "/Middleware/imgui-docking")`
// in Sharpmake_Zenith.cs) for the same reason. conf.PrecompHeader /
// conf.PrecompSource are deliberately left unset here.
//
// DEPENDENCY: NONE — the second way this library is the odd one out, and
// deliberately NOT given the "MANDATORY ZenithCore" edge every other project
// in this refactor carries. That mandatory edge exists because every TU
// compiling against the shared Zenith.h PCH references the inline
// Zenith_LogImpl -> Zenith_LogFileSinkWrite (Sharpmake_ZenithCore.cs's header
// comment) — imgui's five core TUs never see Zenith.h at all, so they
// reference no Zenith:: symbol, and adding the edge anyway would misstate
// what this library actually touches. ZenithJolt made exactly this same call
// for the vendored Jolt tree; ZenithImGui is the second (and, engine-wide, the
// last remaining vendored-source-with-no-PCH case this refactor's remaining
// phases are expected to hit — the Vulkan render-backend split is native
// Zenith code with a PCH like every other leaf).
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithImGui".
[Sharpmake.Generate]
public class ZenithImGuiLibProject : ZenithBaseProject
{
	// Leaf static lib — renderer-agnostic; must not link or propagate
	// glfw/vulkan/slang, so ConfigureAll below calls none of the
	// AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs helpers.
	public ZenithImGuiLibProject()
	{
		Name = "ZenithImGui";

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

		SourceRootPath = RootPath + "/Middleware/imgui-docking";

		// Own exactly the five core TUs at the top of imgui-docking — nothing
		// under backends/, examples/ or misc/. See the file header comment.
		SourceFilesExcludeRegex.Add(@".*imgui-docking\\backends\\.*");
		SourceFilesExcludeRegex.Add(@".*imgui-docking\\examples\\.*");
		SourceFilesExcludeRegex.Add(@".*imgui-docking\\misc\\.*");
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithImGui\[target.Platform]\[conf.Name]";

		// NO PCH — see the file header comment. conf.PrecompHeader /
		// conf.PrecompSource are deliberately left unset; verify NotUsing on
		// every TU here in the generated .vcxproj.

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf lib must not link or propagate the renderer backend.

		conf.Output = Configuration.OutputType.Lib;

		// No AddPublicDependency calls at all — see the file header comment
		// ("DEPENDENCY: NONE").

		// Tools define: lockstep across every consumer (ODR) — see
		// Sharpmake_ZenithCore.cs's header comment. ZenithImGui's own TUs
		// never read ZENITH_TOOLS (they never see Zenith.h), but this define
		// also gates the Tools middleware include paths below, which cost
		// nothing to add here and keep this project's True-config shape
		// identical to every other leaf lib for anyone diffing the generated
		// .vcxproj (mirrors ZenithJolt's identical reasoning).
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
