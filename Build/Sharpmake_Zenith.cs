using Sharpmake;
using System;
using System.IO;

// ZenithBase DISSOLVED (Phase 4a). It used to be one bottom-leaf static
// library spanning FIVE directories (Maths/, Collections/, DataStream/,
// FileAccess/, and a subtractive-regex slice of Core/) held together only by
// a negative-lookahead regex over Core's top-level .cpp files — membership
// that was additive-glob-minus-subtractive-regex, global and easy to get
// wrong (a new file under an already-extracted directory silently compiled
// into two libraries unless someone remembered to widen the regex). It is
// replaced by FOUR single-directory leaf libraries, each globbed additively
// by exactly one project:
//   Sharpmake_ZenithCore.cs        (Zenith/Core + Zenith/Collections; PCH)
//   Sharpmake_ZenithMaths.cs       (Zenith/Maths; no PCH, 1 TU)
//   Sharpmake_ZenithDataStream.cs  (Zenith/DataStream; no PCH, 2 TUs)
//   Sharpmake_ZenithFileAccess.cs  (Zenith/FileAccess; no PCH, 1 TU)
// See Sharpmake_ZenithCore.cs's header comment for the full rationale
// (including why Core/Callstack + Core/Memory + Core/Multithreading, split
// apart under the old regime, are ALL simply "Core" now). Every consumer that
// used to say `AddPublicDependency<ZenithBaseLibProject>` now depends on
// ZenithCoreLibProject unconditionally (mandatory — see that file), plus
// ZenithMathsLibProject / ZenithDataStreamLibProject / ZenithFileAccessLibProject
// where it genuinely uses them directly.

// ═════════════════════════════════════════════════════════════════════════
// PHASE 5.1 — THE AGGREGATE BECOMES THE COMPOSITION ROOT.
// ═════════════════════════════════════════════════════════════════════════
// Every engine subsystem is now its own static library (32 entries — see
// Sharpmake_EngineLibs.cs). What used to be the ONE structurally-wrong thing
// left was this project's own SourceRootPath: it still globbed the whole
// "/Zenith" tree recursively (330 TUs) and subtracted every extracted
// library with a long list of constructor SourceFilesExcludeRegex lines —
// the exact greedy-glob-minus-subtraction shape the rest of this refactor
// exists to remove. Phase 5.1 narrows it:
//
//   SourceRootPath = RootPath + "/Zenith/EngineComposition"
//
// EngineComposition holds exactly the 7 .cpp this project still needs to
// compile (Zenith_Engine.cpp, Zenith_Core.cpp, Zenith_Main.cpp,
// Zenith_BenchECS.cpp, Zenith_AutomatedTest.cpp, Zenith_UserSettings.cpp,
// Zenith_EnginePCH.cpp) + their 5 .Tests.inl companions — moved there in an
// earlier phase (see Zenith/EngineComposition/CLAUDE.md) specifically so a
// later phase could re-root this project at it. That later phase is this
// one. EVERY SourceFilesExcludeRegex line that existed only to subtract an
// extracted library (Windows, Android, Input, Profiling, DebugVariables,
// TaskSystem, UnitTests, SaveData, Telemetry, Prefab, Scripting,
// AssetHandling, Flux, the three RenderBackends, UI, EntityComponent,
// Editor, ZenithECS, Physics, AI, plus the imgui-docking backend/core-file
// subtraction list) is DEAD the moment the root no longer globs those
// directories at all, and is deleted outright below — narrower ownership,
// not more exclusion rules, is the fix. AdditionalSourceRootPaths for
// Middleware/imgui-docking and repo-root /Tools are deleted for the same
// reason: ZenithImGui (Sharpmake_ZenithImGui.cs) and ZenithTools
// (Sharpmake_ZenithTools.cs) own those trees now, and this project has no
// TU under either directory to compile. (The one file that loses a "member"
// as a result — Middleware/imgui-docking/backends/imgui_impl_android.cpp,
// which compiled nowhere before this change either — simply stops being
// globbed by anything at all; Tools/check_project_membership.ps1's UNOWNED
// check only scans Zenith/**, so this is invisible to that gate, exactly as
// it was invisible before when the aggregate carried the file as an
// excluded, uncompiled member.)
//
// Games keep their single `AddPublicDependency<ZenithProject>` edge
// (Sharpmake_Games.cs) unchanged — zero edits to any game file — and the
// same holds for FluxCompiler, ZenithHub and both TilePuzzle projects:
// `ZenithEngineLibs.AddAllAsDependenciesOfAggregate` (below) still names
// every other leaf publicly, so every one of those edges is still
// transitively present at the final exe link. `zenith build engine`
// (`/t:Zenith`) still names everything through that same edge.
//
// dbghelp.lib / psapi.lib MOVED OFF THIS PROJECT — see Sharpmake_ZenithWindows.cs
// / Sharpmake_ZenithEditor.cs. Both used to sit here unconditionally for
// every win64 config; grep found their real (and only) callers are
// Zenith/Windows/Callstack/Zenith_Windows_Callstack.cpp (dbghelp) and
// Zenith/Editor/Panels/Zenith_EditorPanel_Memory.cpp (psapi) — NEITHER of
// which is Zenith/Core/, contrary to this phase's own brief. Each now sits
// on the library that actually calls it.
[Sharpmake.Generate]
public class ZenithProject : ZenithBaseProject
{
	public ZenithProject()
	{
		Name = "Zenith";

		// Phase 5.1 — narrowed from RootPath + "/Zenith" (330 TUs, recursive)
		// to exactly the composition root: 7 .cpp + 5 .Tests.inl. See the
		// file header comment above.
		SourceRootPath = RootPath + "/Zenith/EngineComposition";

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

		// No SourceFilesExcludeRegex of any kind. Every Phase 4a-4h.1 line
		// that used to live here (Core/Maths/DataStream/FileAccess/Windows/
		// Android/Input/Profiling/DebugVariables/TaskSystem/UnitTests/
		// SaveData/Telemetry/Prefab/Scripting/AssetHandling/Flux/the three
		// RenderBackends/UI/EntityComponent/Editor/ZenithECS/Physics/AI/the
		// eleven imgui-docking subtraction patterns/the VulkanSDK,
		// FluxCompiler, glm-master, cmake vendored-tree excludes) subtracted
		// a directory or file the OLD recursive "/Zenith" glob could still
		// see. The narrowed SourceRootPath above sees none of them any more
		// — EngineComposition contains nothing that matches any of those
		// patterns — so every one of those lines is verified-dead, not
		// merely "probably no longer needed", and is removed rather than
		// carried forward as inert weight. Nothing under EngineComposition
		// itself needs its own exclude (all 12 files there are real members).
	}
	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// ConfigurePlatformExcludes was deleted engine-wide at Phase 4h.3;
		// this project never re-adds an equivalent call.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		// Precompiled header. The PCH master header Zenith.h is CREATEd by
		// ZenithCore (Zenith.cpp), which every other leaf lib and this
		// aggregate compile /Yu against. This aggregate CREATEs its OWN
		// Zenith.pch (a separate binary .pch — per-project, like every other
		// leaf) from Zenith_Core.cpp, an EngineComposition TU that stays in
		// this project's now-narrow SourceRootPath and already begins with
		// `#include "Zenith.h"`.
		conf.PrecompHeader = "Zenith.h";
		// AGDE/clang precompiles the ENTIRE PrecompSource file via -xc++-header, so a
		// definition-bearing TU bakes its symbols into Zenith.h.pch and EVERY TU that
		// uses the PCH re-emits them -> hundreds of duplicate-symbol errors when a
		// game .so links the engine archive. Zenith_Core.cpp defines functions
		// (UpdateTimers / Zenith_MainLoop / ...) AND #includes
		// EngineComposition/Zenith_UnitTests.Tests.inl, so on AGDE use the
		// definition-free Zenith_EnginePCH.cpp (just #include "Zenith.h") as the
		// create TU instead. MSVC's /Yc captures only the header prefix (not the
		// rest of the .cpp), so win64 is unaffected and keeps Zenith_Core.cpp as
		// before. Both TUs live in Zenith/EngineComposition/, now this project's
		// own SourceRootPath (no longer reached via a wider recursive glob).
		conf.PrecompSource = (target.Platform == Platform.agde) ? "Zenith_EnginePCH.cpp" : "Zenith_Core.cpp";
		// No PrecompSourceExcludeFolders: Phase 5.1's narrowed SourceRootPath
		// no longer sees Middleware/JoltPhysics-5.4.0, Middleware/imgui-docking
		// or Zenith/Android/NativeGlue at all (none is a descendant of
		// Zenith/EngineComposition), so the three exclude-folder entries that
		// used to guard the PCH-source scan against them are verified-dead
		// and removed rather than kept as belt-and-braces.

		// Common configuration
		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);

		// glfw is the windowing backend for every render backend.
		AddGlfwLibs(conf, target);
		if (target.RenderBackend == RenderBackend.Vulkan)
		{
			AddVulkanBackendLibs(conf, target);
			AddSlangLibs(conf, target);
		}
		else if (target.RenderBackend == RenderBackend.D3D12)
		{
			AddD3D12Libs(conf, target);
		}
		// RenderBackend.Null deliberately calls neither: no vulkan-1/slang, no
		// d3d12/dxgi/dxguid. glfw above is the only graphics-adjacent lib it
		// links (the window/input layer is shared by every backend).

		// Depend on every other engine leaf lib (all 31 entries in
		// ZenithEngineLibs.All except this aggregate itself) as a public
		// dependency — PUBLIC so each edge is re-exported to the games +
		// tools that AddPublicDependency<ZenithProject>() — they transitively
		// link every leaf lib with no change to their own .cs files, and the
		// aggregate keeps its "Zenith" name + lib output (blast radius
		// minimised — see the file header comment).
		ZenithEngineLibs.AddAllAsDependenciesOfAggregate(conf, target);

		// Additional include paths — needed directly, not merely "the PCH
		// might touch them": EngineComposition/Zenith_Core.cpp,
		// Zenith_Engine.cpp and Zenith_UnitTests.Tests.inl (#included from
		// Zenith_Core.cpp) #include Flux/** (Flux.h, Flux_RendererImpl.h,
		// two dozen Flux/<Subsystem>/Flux_*Impl.h headers), Physics/Zenith_Physics.h
		// + Physics/Zenith_PhysicsMeshGenerator.h (Jolt-backed — needs the
		// Jolt header path directly, not just via a transitive PCH pull) and
		// Editor/** directly — this is the composition root that wires every
		// subsystem together, so unlike the narrow ECS/Physics/AI leaves (see
		// those files' own corrected header comments) it genuinely reaches
		// all four trees from its own compiled TUs. Verified by grep, not
		// merely asserted, and re-verified by a green Vulkan_/Null_/D3D12_
		// engine build (steps 3 and the Combat matrix in this phase's
		// verification).
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends");
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

			// Phase 5.1 — dbghelp.lib / psapi.lib MOVED OFF this project onto
			// ZenithWindows / ZenithEditor respectively (see this file's own
			// header comment and each of those two files' own comment at the
			// new conf.LibraryFiles.Add call). Both are still present
			// transitively at every game exe link (ZenithWindows is a
			// mandatory win64 row in ZenithEngineLibs.All; ZenithEditor is
			// mandatory for win64 x ToolsEnabled.True) — do not re-add either
			// line here.
		}
		else if (target.Platform == Platform.agde)
		{
			conf.Output = Configuration.OutputType.Lib;
		}

		// ZENITH_TOOLS / ZENITH_ROOT — still needed here, even though this
		// aggregate no longer owns either Editor/ or Tools/ (Phase 4g moved
		// both to their own leaf libs — see Sharpmake_ZenithEditor.cs /
		// Sharpmake_ZenithTools.cs). ZENITH_TOOLS: EngineComposition/
		// Zenith_Engine.cpp's own `#ifdef ZENITH_TOOLS` block (InitialiseAssets'
		// tool-export phase, forward-declaring ExportAllMeshes/ExportAllTextures/
		// ExportDefaultFontAtlas/GenerateTestAssets as `extern` and calling
		// them — resolved at the final game EXE link, not here) still lives in
		// THIS project. ZENITH_ROOT: EngineComposition/Zenith_UnitTests.Tests.inl
		// (#included from this project's own Zenith_Core.cpp, unconditionally
		// on every ToolsEnabled.True win64 config) references ZENITH_ROOT
		// directly, unguarded — a `Vulkan_..._True` engine build fails C2065
		// without it. So ZENITH_ROOT is needed on BOTH this aggregate AND
		// ZenithToolsProject (which has its own genuine, separate need — see
		// that file's header comment).
		if (target.ToolsEnabled == ToolsEnabled.True && target.Platform == Platform.win64)
		{
			conf.Defines.Add("ZENITH_TOOLS");
			conf.Defines.Add($"ZENITH_ROOT=\"{zenithRoot}/\"");
		}
	}
}
