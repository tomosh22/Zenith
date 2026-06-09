using Sharpmake;
using System;
using System.IO;

// ZenithBase — bottom-leaf static library (Wave-20 stage 1 of the bounded
// lib-split). Carved out of the monolithic engine lib WITHOUT changing runtime
// behaviour. It holds the layering scout's L0 set only:
//   Maths/  Collections/  DataStream/  FileAccess/
//   Core/Memory/  Core/Multithreading/
//   Core/Zenith.cpp (the PCH-compile TU) + Core/Zenith_String.cpp
// plus the L0 headers consumed by everything (Core/Zenith.h — the PCH master
// header, Core/ZenithConfig.h, Core/Zenith_String.h, the root-level
// Zenith_DebugBreak.h, and the Maths/Collections headers).
//
// THE PCH: every engine .cpp begins `#include "Zenith.h"` and builds with
// MSVC /Yu"Zenith.h". Zenith.h is the bottom-most include, so its owning
// compile TU (Zenith.cpp) lives here. ZenithBase CREATEs Zenith.pch
// (PrecompiledHeader=Create on Zenith.cpp); the aggregate engine lib and every
// other consumer compile their OWN Zenith.pch from the same shared header
// (PrecompiledHeader=Use) — a per-lib PCH, because the binary .pch cannot be
// shared across projects that compile with different flags (e.g. ZENITH_TOOLS).
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithBase".
//
// NOTE ON SOURCE PARTITION (no TU may compile in BOTH libs — that would be an
// ODR / duplicate-symbol break): this project includes exactly the L0 .cpp
// set, and the aggregate ZenithProject build-EXCLUDES that same set below. The
// two must stay a clean complement. The Core directory is mixed (only 2 of its
// top-level .cpp are L0), so the Core glob here excludes every top-level
// Core *.cpp EXCEPT Zenith.cpp / Zenith_String.cpp via a negative-lookahead
// regex; the Core/Memory and Core/Multithreading subdirs are unaffected by
// that regex and are kept. A NEW top-level Core .cpp added later defaults to
// the aggregate (safe); it would only be wrong if it is genuinely L0, in which
// case both the regex here and the aggregate's build-exclude list must learn
// about it.
[Sharpmake.Generate]
public class ZenithBaseLibProject : ZenithBaseProject
{
	// L0 leaf static lib — renderer-agnostic; must not link or propagate glfw/vulkan/slang.
	protected override bool LinksRendererBackend => false;

	public ZenithBaseLibProject()
	{
		Name = "ZenithBase";

		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan | RenderBackend.D3D12
		});

		AddTargets(new ZenithTarget
		{
			Platform = Platform.agde,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan,
			AndroidBuildTargets = Android.AndroidBuildTargets.arm64_v8a
		});

		// Use the engine root as SourceRootPath so the project's relative
		// layout — and crucially the "Zenith.h" PrecompHeader lookup — resolve
		// identically to the aggregate engine lib. Membership is narrowed to
		// the L0 set by the include-roots + excludes below rather than by a
		// tight root, because the L0 set spans several sibling dirs plus two
		// individual Core root files.
		SourceRootPath = RootPath + "/Zenith/Maths";

		// Remaining whole-directory L0 members. Core is added as a root so the
		// two L0 Core root files (Zenith.cpp / Zenith_String.cpp) AND the
		// Core/Memory + Core/Multithreading subdirs are discovered in one shot;
		// the non-L0 Core top-level files are stripped by the regex below.
		AdditionalSourceRootPaths.Add(RootPath + "/Zenith/Collections");
		AdditionalSourceRootPaths.Add(RootPath + "/Zenith/DataStream");
		AdditionalSourceRootPaths.Add(RootPath + "/Zenith/FileAccess");
		AdditionalSourceRootPaths.Add(RootPath + "/Zenith/Core");

		// Strip every top-level Core *.cpp that is NOT one of the two L0 files.
		// Anchored on "\Core\Zenith" so it never touches the Core\Memory\ or
		// Core\Multithreading\ subdir files (those are preceded by the subdir
		// name, not by "Zenith"). The negative lookahead keeps Zenith.cpp and
		// Zenith_String.cpp.
		SourceFilesExcludeRegex.Add(@".*\\Core\\Zenith(?!\.cpp$|_String\.cpp$)[^\\]*\.cpp$");

		// The Core root above also globs Core subdirs. Keep ONLY the two L0
		// subdirs (Core\Memory, Core\Multithreading); strip every other Core
		// subdir so it is not double-compiled here AND in the aggregate. Core
		// \Callstack is Core-level (not L0) and stays in the aggregate.
		SourceFilesExcludeRegex.Add(@".*\\Core\\Callstack\\.*");
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Same platform-file excludes as the engine lib (drop Android files on
		// Windows builds and vice-versa). ZenithBase has no Editor/Tools
		// sources, but ConfigurePlatformExcludes is harmless and keeps the
		// platform partition identical to the aggregate.
		ConfigurePlatformExcludes(conf, target);

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		// Distinct intermediate dir. ZenithBase and the aggregate Zenith both
		// live in the Build dir and otherwise default to the SAME obj tree
		// (obj\[platform]\[conf]) — now fatal because BOTH CREATE a Zenith.pch
		// there and would clobber each other (MSB8028 + PCH-collision). Give
		// ZenithBase its own obj subtree so its .obj + Zenith.pch are private.
		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithBase\[target.Platform]\[conf.Name]";

		// Per-lib PCH: ZenithBase CREATEs Zenith.pch from Zenith.cpp. Every
		// other ZenithBase TU compiles with /Yu"Zenith.h" against it.
		conf.PrecompHeader = "Zenith.h";
		conf.PrecompSource = "Zenith.cpp";

		// Common configuration — identical knobs to the engine lib so the L0
		// TUs compile under the exact same flags/defines/include paths they did
		// inside the monolith (no behavioural delta). ConfigureCommonIncludePaths
		// supplies /Zenith + /Zenith/Core + glm/stb/Middleware + the platform
		// include dir, so "Zenith.h", "Maths/Zenith_Maths.h", the platform
		// "Zenith_OS_Include.h" (pulled transitively by the PCH) etc. all
		// resolve unchanged.
		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		ConfigureCommonLibraryPaths(conf, target);

		// The PCH (Zenith.h) transitively pulls Zenith_DebugVariables.h →
		// imgui.h AND Flux/Flux.h → Vulkan/vma/Jolt headers, so ZenithBase
		// needs the SAME extra include paths the aggregate adds for its own PCH
		// compile. These are include paths only — ZenithBase links none of these
		// libs; the -I flags merely let the PCH headers resolve.
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/Vulkan");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		conf.Output = Configuration.OutputType.Lib;

		// Tools define: ZenithBase carries no tools-only code, but the PCH it
		// compiles (Zenith.h) branches on ZENITH_TOOLS, so the binary .pch must
		// be built with the same define as the consumers in a True config —
		// otherwise the per-lib PCH would diverge and consumers re-create their
		// own anyway. Keep it in lockstep with the engine lib.
		if (target.ToolsEnabled == ToolsEnabled.True && target.Platform == Platform.win64)
		{
			conf.Defines.Add("ZENITH_TOOLS");

			// The ZENITH_TOOLS PCH branch may pull tools/editor headers; mirror
			// the aggregate's tools include paths so the per-lib PCH resolves
			// identically in True. Include paths only (no tool libs linked).
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/assimp/include");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/opencv/build/include");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/opencv/build/include/opencv2");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/freetype/include");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen/msdfgen");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen/msdfgen/Config");

			// Exclude Tools from the PCH create (no tools sources here anyway).
			conf.PrecompSourceExcludeFolders.Add(RootPath + "/Tools");
		}
	}
}

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
			ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan | RenderBackend.D3D12
		});

		AddTargets(new ZenithTarget
		{
			Platform = Platform.agde,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan,
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
		SourceFilesExcludeRegex.Add(@".*stb_vorbis\.c.*");

		// MSDF font deps: source-vendored but built externally via build_msdf_deps.bat;
		// linked as prebuilt .libs (see ConfigureAll). Exclude their full source trees from
		// the /Tools auto-scan so they don't get compiled into Zenith — ODR violation otherwise.
		SourceFilesExcludeRegex.Add(@".*Tools\\Middleware\\freetype\\.*");
		SourceFilesExcludeRegex.Add(@".*Tools\\Middleware\\msdf-atlas-gen\\.*");
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Configure platform-specific excludes
		ConfigurePlatformExcludes(conf, target);

		// Android excludes Windows/GLFW-specific ImGui backends and editor
		if (target.Platform == Platform.agde)
		{
			conf.SourceFilesBuildExcludeRegex.Add(@".*imgui-docking\\backends\\imgui_impl_android.*");
			conf.SourceFilesBuildExcludeRegex.Add(@".*imgui-docking\\backends\\imgui_impl_glfw.*");
			conf.SourceFilesBuildExcludeRegex.Add(@".*imgui-docking\\backends\\imgui_impl_win32.*");
			conf.SourceFilesBuildExcludeRegex.Add(@".*imgui-docking\\backends\\imgui_impl_vulkan.*");
			// Exclude editor on Android
			conf.SourceFilesBuildExcludeRegex.Add(@".*Editor\\.*");
		}

		// L0 lib-split (Wave-20 stage 1): the engine root glob (SourceRootPath
		// = /Zenith) still SEES every L0 file, so build-EXCLUDE the exact set
		// now compiled by ZenithBaseLibProject. This is the complement of that
		// project's membership — the two must never overlap, or the L0 .obj
		// would be produced by both libs (duplicate-symbol link break, and a
		// duplicate Zenith.pch Create). Collections/ has no .cpp so it needs no
		// entry. Keep this list in lockstep with ZenithBaseLibProject above.
		conf.SourceFilesBuildExcludeRegex.Add(@".*\\Maths\\.*");
		conf.SourceFilesBuildExcludeRegex.Add(@".*\\DataStream\\.*");
		// Anchored on "\Zenith\FileAccess\" so it strips ONLY the top-level L0
		// FileAccess dir — NOT the platform impl in Windows\FileAccess\, which
		// is not in ZenithBase and must stay compiled here (else it is dropped
		// from both libs → unresolved external).
		conf.SourceFilesBuildExcludeRegex.Add(@".*\\Zenith\\FileAccess\\.*");
		conf.SourceFilesBuildExcludeRegex.Add(@".*\\Core\\Memory\\.*");
		conf.SourceFilesBuildExcludeRegex.Add(@".*\\Core\\Multithreading\\.*");
		conf.SourceFilesBuildExcludeRegex.Add(@".*\\Core\\Zenith\.cpp$");
		conf.SourceFilesBuildExcludeRegex.Add(@".*\\Core\\Zenith_String\.cpp$");

		// ECS-leaf partition: the engine root glob (SourceRootPath = /Zenith) still
		// SEES every ECS-core file under Zenith/ZenithECS/, so build-EXCLUDE the
		// WHOLE subtree's .cpp. Every TU there — public root + everything under
		// Internal/ — is a leaf TU compiled by ZenithECSLibProject, so the aggregate
		// must produce NONE of them (duplicate-symbol link break otherwise). One
		// anchored subtree pattern is the clean complement of that project's
		// membership and auto-covers any new ECS TU.
		//
		// The aggregate STILL compiles the engine-side EntityComponent/ files
		// (Zenith_ComponentMeta_Registration.cpp, Zenith_CameraResolve.cpp,
		// Zenith_ComponentRegistry.cpp, and the entire Components/ subtree) — they
		// live in a DIFFERENT directory and so are never matched here.
		conf.SourceFilesBuildExcludeRegex.Add(@".*\\ZenithECS\\.*\.cpp$");

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		// Precompiled header (per-lib). The PCH master header Zenith.h now
		// lives in the ZenithBase leaf lib, which OWNS the Create TU
		// (Zenith.cpp). Zenith.cpp moved to ZenithBase (see the L0 build-
		// excludes below), so the aggregate can no longer use it as its Create
		// source. The aggregate CREATEs its own Zenith.pch from Zenith_Core.cpp
		// — a Core TU that stays in this lib, compiles in every config/platform
		// (not platform- or tools-gated), and already begins with
		// `#include "Zenith.h"`. All other aggregate TUs compile /Yu against it.
		conf.PrecompHeader = "Zenith.h";
		// AGDE/clang precompiles the ENTIRE PrecompSource file via -xc++-header, so a
		// definition-bearing TU bakes its symbols into Zenith.h.pch and EVERY TU that
		// uses the PCH re-emits them -> hundreds of duplicate-symbol errors when a
		// game .so links the engine archive. Zenith_Core.cpp defines functions
		// (UpdateTimers / Zenith_MainLoop / ...) AND #includes Zenith_UnitTests.Tests.inl,
		// so on AGDE use the definition-free Zenith_EnginePCH.cpp (just #include
		// "Zenith.h") as the create TU instead. MSVC's /Yc captures only the header
		// prefix (not the rest of the .cpp), so win64 is unaffected and keeps
		// Zenith_Core.cpp as before.
		conf.PrecompSource = (target.Platform == Platform.agde) ? "Zenith_EnginePCH.cpp" : "Zenith_Core.cpp";
		conf.PrecompSourceExcludeFolders.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.PrecompSourceExcludeFolders.Add(RootPath + "/Middleware/imgui-docking");
		// android_native_app_glue is a plain C file from the NDK, no PCH
		conf.PrecompSourceExcludeFolders.Add(RootPath + "/Zenith/Android/NativeGlue");

		// Common configuration
		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		ConfigureCommonLibraryPaths(conf, target);

		// Depend on the ZenithBase leaf lib (the L0 set carved out above).
		// PUBLIC so the edge is re-exported to the 11 games + 4 tools that
		// AddPublicDependency<ZenithProject> — they transitively link ZenithBase
		// with no change to their own .cs files, and the aggregate keeps its
		// "Zenith" name + lib output (blast radius minimised). ZenithBase also
		// exports its include paths through this edge so dependents resolve
		// "Zenith.h" + the Maths/Collections/etc. headers; they already add the
		// same paths via their own ConfigureCommonIncludePaths, so resolution is
		// unchanged either way (no #include-path churn in any TU).
		conf.AddPublicDependency<ZenithBaseLibProject>(target);

		// Depend on the ZenithECS leaf lib (the ECS-core set). ECS-leaf
		// extraction Phase 8: the dependency edge is wired now, but the
		// aggregate does NOT yet build-exclude the ECS-core .cpp — they still
		// glob in via SourceRootPath = /Zenith and compile into BOTH libs. That
		// duplicate-symbol state is INTENTIONAL this phase (it validates that
		// ZenithECS compiles standalone against ZenithBase); Phase 9 adds the
		// matching SourceFilesBuildExcludeRegex here to make the two memberships
		// a clean complement. PUBLIC so the edge re-exports to dependents, like
		// the ZenithBase edge above.
		conf.AddPublicDependency<ZenithECSLibProject>(target);

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
		if (target.Platform == Platform.agde)
		{
			// On Android, assets are bundled into the APK by Gradle.
			// AAssetManager expects relative paths within the APK's assets directory.
			conf.Defines.Add("ENGINE_ASSETS_DIR=\"\"");
		}
		else
		{
			conf.Defines.Add($"ENGINE_ASSETS_DIR=\"{engineAssetRoot}\"");
		}

		// Shader source/asset path
		if (target.Platform == Platform.win64)
		{
			string shaderSourceRoot = zenithRoot + "/Zenith/Flux/Shaders/";
			conf.Defines.Add($"SHADER_SOURCE_ROOT=\"{shaderSourceRoot}\"");
		}
		else if (target.Platform == Platform.agde)
		{
			// On Android, pre-compiled shaders are bundled as APK assets.
			// Empty root so paths are relative for AAssetManager.
			conf.Defines.Add("SHADER_SOURCE_ROOT=\"\"");
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

			// MSDF font dep includes. Libs built as separate Sharpmake static-lib
			// projects (Sharpmake_FreeType.cs, Sharpmake_Msdfgen.cs, Sharpmake_MsdfAtlasGen.cs)
			// and added as deps below via AddPublicDependency.
			// "Config" subdir holds the hand-written msdfgen-config.h that
			// base.h references as <msdfgen/msdfgen-config.h>.
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/freetype/include");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen/msdfgen");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen/msdfgen/Config");

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

			// MSDF font deps as Sharpmake static-lib project deps (tools-only).
			// MsdfAtlasGenProject transitively brings in MsdfgenProject + FreeTypeProject.
			conf.AddPublicDependency<MsdfAtlasGenProject>(target);
		}
		else
		{
			// Exclude Tools source files when tools are disabled
			conf.SourceFilesBuildExcludeRegex.Add(@".*\\Tools\\.*");
		}
	}
}
