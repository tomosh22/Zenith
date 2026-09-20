using Sharpmake;
using System;
using System.IO;

// ZenithWindows — leaf static library (Phase 4b — see Sharpmake_ZenithCore.cs
// for the pattern this file replicates). Holds Zenith/Windows — 9 translation
// units (the 6 top-level Zenith_Windows_*.cpp + Callstack/Zenith_Windows_Callstack.cpp
// + FileAccess/Zenith_Windows_FileAccess.cpp + Multithreading/Zenith_Windows_Multithreading.cpp)
// + the two ImGui platform-backend TUs it now OWNS (imgui_impl_glfw.cpp,
// imgui_impl_win32.cpp — see "THE TWO IMGUI BACKEND TUS" below) + the
// definition-free PCH create TU (Internal/Zenith_WindowsPCH.cpp). 9 + 2 = 11
// compiled TUs. Previously globbed straight into the monolithic aggregate
// ZenithProject.
//
// WIN64 ONLY — THE NEW HAZARD THIS LIB INTRODUCES: unlike every leaf lib
// before it, ZenithWindows declares ONLY a win64 target set (no agde
// AddTargets call at all). `conf.AddProject`/`conf.AddPublicDependency` THROW
// at Sharpmake-run time if the target being configured is not one the callee
// itself declares — so this project can never be unconditionally added the
// way ZenithCore/ZenithMaths/etc. are. It is wired into
// Build/Sharpmake_EngineLibs.cs with an explicit
// `t => t.Platform == Platform.win64` predicate instead of any consumer
// growing its own `if`. ZenithAndroidLibProject is the agde-only mirror image
// (see Sharpmake_ZenithAndroid.cs).
//
// THE PCH: >= 4 compiled TUs justifies a binary PCH. ZenithWindows CREATEs its
// own Zenith.pch from Internal/Zenith_WindowsPCH.cpp (a one-line
// `#include "Zenith.h"`, nothing else — must stay definition-free, see that
// file's own header comment). Every other TU /Yu's against it.
//
// THE TWO IMGUI BACKEND TUS: Middleware/imgui-docking/backends/imgui_impl_glfw.cpp
// and imgui_impl_win32.cpp become members of ZenithWindows — the library that
// owns the GLFW + Win32 platform APIs those backends wrap — instead of being
// globbed by the aggregate and then per-configuration build-excluded on agde.
// They are added via a SECOND AdditionalSourceRootPaths root (the backends
// directory) with an exclude list that keeps only those two files — the same
// "glob the whole dir, exclude everything but the two wanted names" pattern
// Sharpmake_ZenithHub.cs already uses for its own Win32+DX11 backend pair.
// imgui_impl_vulkan.cpp is NOT here — it belongs to the (not-yet-extracted)
// Vulkan backend library and stays wherever the aggregate/Vulkan-backend
// partition currently owns it. imgui_impl_android.cpp is compiled by NOTHING
// today (verified: the aggregate's own agde-only build-exclude block dropped
// it even on agde, and no win64 config would ever compile an Android backend
// file) — kept that way; it is added to neither library.
//
// CONSEQUENCE — TWO OF THE AGGREGATE'S FOUR AGDE-ONLY IMGUI BUILD-EXCLUDES ARE
// NOW DEAD: Sharpmake_Zenith.cs's `ConfigureAll` has an agde-only block adding
// per-configuration build-exclude entries for imgui_impl_android /
// imgui_impl_glfw / imgui_impl_win32 / imgui_impl_vulkan. Now that glfw+win32 are owned WHOLE by
// this win64-only library, the aggregate's own recursive /Zenith... no, its
// AdditionalSourceRootPaths(/Middleware/imgui-docking) root never sees glfw/win32
// AT ALL on agde once ownership moves here (whole-directory ownership exclude,
// not a per-config build exclude) — so the aggregate's agde-only excludes for
// those two names are now unreachable dead code and have been deleted. The
// android and vulkan excludes stay: imgui_impl_android.cpp and
// imgui_impl_vulkan.cpp are still globbed by the aggregate's own
// AdditionalSourceRootPaths(/Middleware/imgui-docking) root (their ownership did
// not move — android compiles nowhere, vulkan is aggregate/Vulkan-backend's),
// so their agde build-excludes remain load-bearing.
//
// GLFW OWNERSHIP — WHY IT LIVES HERE AND NOT ANYWHERE ELSE: GLFW is called
// directly from FOUR directories, not one: Zenith/Windows/ (2 files:
// Zenith_Windows_Window.cpp + the two imgui backend TUs now owned here — so
// really 3 call sites live in this one library), Zenith/Input/ (2 files:
// see Sharpmake_ZenithInput.cs), the Vulkan render backend (3 files, still
// aggregate-owned), and formerly Zenith_Core.cpp (moved to
// Zenith/EngineComposition/ in an earlier phase, so ZenithCore itself is
// clean of GLFW calls). `AddGlfwLibs` is called from HERE and from NOWHERE
// ELSE new this phase. Owning glfw3_mt.lib on ZenithWindows links correctly
// at every game exe, because the composition root every game transitively
// links (the aggregate) always carries ZenithWindows too — but it leaves
// ZenithInput and the Vulkan backend as libraries with UNDECLARED win64 glfw
// symbols of their own. That is accepted, not a bug, for exactly this phase:
// a static lib never resolves external symbols at its own link step, only the
// final exe link does, and the aggregate always supplies ZenithWindows there.
// Write this down for whoever adds it: A FUTURE ZenithInput OR VULKAN-BACKEND
// SENTINEL (a leaf-proof exe in the SentinelECS/SentinelPhysics/SentinelAI
// mould) WOULD NEED ZenithWindows ADDED TO ITS OWN LINK SET, because neither
// of those two libraries declares the GLFW edge itself.
//
// DEPENDENCY: Zenith_Windows_Window.cpp #includes Input/Zenith_Input.h
// (direct use — window creation feeds the input layer) and
// Multithreading/Zenith_Windows_Multithreading.cpp #includes
// Profiling/Zenith_Profiling.h (direct use — thread-name/priority calls are
// profiler-instrumented). So this lib depends on ZenithCore (mandatory) +
// ZenithInput (direct #include) + ZenithProfiling (direct #include).
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithWindows".
[Sharpmake.Generate]
public class ZenithWindowsLibProject : ZenithBaseProject
{
	public ZenithWindowsLibProject()
	{
		Name = "ZenithWindows";

		// WIN64 ONLY — no agde AddTargets call at all. See the file header
		// comment ("WIN64 ONLY") for why every call site that adds this
		// project must be guarded by a predicate rather than added
		// unconditionally.
		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan | RenderBackend.D3D12 | RenderBackend.Null
		});

		SourceRootPath = RootPath + "/Zenith/Windows";

		// The two owned ImGui platform-backend TUs — see "THE TWO IMGUI
		// BACKEND TUS" above. Whole-directory glob + exclude-everything-else,
		// same pattern Sharpmake_ZenithHub.cs already uses for its own
		// Win32+DX11 backend pair.
		AdditionalSourceRootPaths.Add(RootPath + "/Middleware/imgui-docking/backends");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_allegro.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_android.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_dx.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_glut.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_opengl.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_sdl.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_vulkan.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_wgpu.*");
		// imgui_impl_metal.mm / imgui_impl_osx.mm never match
		// SourceFilesExtensions (.cpp/.c/.h/.inl/.slang has no .mm) — no
		// exclude needed for those two.
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithWindows\[target.Platform]\[conf.Name]";

		// Per-lib PCH: ZenithWindows CREATEs Zenith.pch from Internal/Zenith_WindowsPCH.cpp.
		conf.PrecompHeader = "Zenith.h";
		conf.PrecompSource = "Zenith_WindowsPCH.cpp";
		// The two ImGui backend TUs do not begin with `#include "Zenith.h"`,
		// so they must compile WITHOUT the engine PCH (mirrors how the
		// aggregate handles Middleware/imgui-docking and JoltPhysics-5.4.0
		// already).
		conf.PrecompSourceExcludeFolders.Add(RootPath + "/Middleware/imgui-docking");

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);

		// ZenithWindows OWNS GLFW — see the file header comment. Called from
		// HERE and from nowhere else new this phase.
		AddGlfwLibs(conf, target);
		// No AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call: this lib is
		// renderer-agnostic (the platform/windowing layer, not a render
		// backend) and must not link or propagate vulkan-1/slang/d3d12.

		// The PCH (Zenith.h) transitively pulls Zenith_DebugVariables.h ->
		// imgui.h AND Flux/Flux.h -> Vulkan/vma/Jolt headers; the two owned
		// ImGui backend TUs also directly include imgui.h.
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		conf.Output = Configuration.OutputType.Lib;

		// MANDATORY: ZenithCore — see Sharpmake_ZenithCore.cs's header comment.
		conf.AddPublicDependency<ZenithCoreLibProject>(target);

		// Direct edges (see the file header comment's DEPENDENCY paragraph).
		conf.AddPublicDependency<ZenithInputLibProject>(target);
		conf.AddPublicDependency<ZenithProfilingLibProject>(target);

		// DbgHelp -- callstack symbolication. Phase 5 moved this off the
		// aggregate ZenithProject (Sharpmake_Zenith.cs), which had carried
		// it unconditionally for every win64 config since before this
		// library existed. The real (and only) caller is
		// Zenith/Windows/Callstack/Zenith_Windows_Callstack.cpp (SymInitialize
		// / SymFromAddr / StackWalk64 -- grep confirms dbghelp.h is included
		// nowhere else in the engine), which lives in THIS library, not in
		// Zenith/Core/ as Phase 5s own brief assumed -- a corrected finding,
		// not a guess (verified by grep before moving, then by a green
		// SentinelECS/Physics/AI + engine build, none of which link
		// ZenithWindows and none of which need dbghelp.lib). Unconditional
		// within this projects own win64 target set (both ToolsEnabled
		// values -- callstack capture is not tools-only), matching the
		// aggregates old unconditional-for-win64 placement exactly.
		conf.LibraryFiles.Add("dbghelp.lib");

		if (target.ToolsEnabled == ToolsEnabled.True && target.Platform == Platform.win64)
		{
			conf.Defines.Add("ZENITH_TOOLS");

			conf.IncludePaths.Add(RootPath + "/Tools/Middleware");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/assimp/include");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/freetype/include");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen/msdfgen");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen/msdfgen/Config");

			conf.PrecompSourceExcludeFolders.Add(RootPath + "/Tools");
		}
	}
}
