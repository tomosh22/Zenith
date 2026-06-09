using Sharpmake;
using System;
using System.IO;

// SentinelECS -- leaf-proof executable for the ECS-leaf extraction (Phase 9b).
//
// This exe exists for ONE reason: to prove that ZenithECS is a self-contained
// leaf on top of ZenithBase, with NO undefined engine externals. It links
// EXACTLY zenithecs.lib + zenithbase.lib (the two AddPublicDependency calls
// below) and NOTHING else -- no ZenithProject (the aggregate engine lib), no
// Flux, no Physics, no game. Its single TU (Tests/SentinelECS/main.cpp) drives
// the ECS core end-to-end (construct the SceneSystem, register a stub component,
// create a scene + bare entity, AddComponent, Query, Destroy) without ever
// naming g_xEngine / Flux / Physics / a concrete component. If the ECS core
// secretly depended on an engine symbol, THIS link would fail with an
// unresolved external -- so a green SentinelECS build IS the leaf proof.
//
// SHAPE: modelled on the GameProject template (Output = Exe, win64 Debug/Release
// x True/False, no agde) for the executable boilerplate, and on ZenithECSLib /
// ZenithBaseLib for the include paths the PCH header needs to resolve.
//
// PCH: like the game EXEs (GameProject sets NO PrecompHeader/PrecompSource), the
// single main.cpp begins `#include "Zenith.h"` and compiles it as a plain
// textual include -- no binary /Yc/Yu PCH. A one-TU target gains nothing from a
// binary PCH, and skipping it avoids the per-target Zenith.pch obj-collision the
// multi-TU libs have to manage with a distinct IntermediatePath + Create TU.
//
// INCLUDE PATHS: the PCH master header Zenith.h transitively pulls
// Zenith_DebugVariables.h -> imgui.h AND Flux/Flux.h -> Vulkan/vma/Jolt headers,
// so the sentinel needs the SAME extra include paths ZenithECS/ZenithBase add
// for their own PCH compile. These are include paths ONLY -- the sentinel links
// none of those libs; the -I flags merely let the header resolve. In a True
// config the ZENITH_TOOLS branch of the PCH may also pull tools/editor headers,
// so the tools include paths are mirrored too (again: paths only, no tool libs).
[Sharpmake.Generate]
public class SentinelECSProject : ZenithBaseProject
{
	// Leaf-proof exe — must link ONLY the two leaf libs (ZenithBase + ZenithECS),
	// never the renderer backend (glfw/vulkan/slang). With this false, neither this
	// project's own ConfigureCommonLibraryPaths nor the propagated LibraryFiles from
	// the (also-LinksRendererBackend=false) leaf-lib dependencies add those libs, so
	// any accidental ECS->renderer edge surfaces as a hard undefined-symbol error.
	protected override bool LinksRendererBackend => false;

	public SentinelECSProject()
	{
		Name = "SentinelECS";
		SourceRootPath = RootPath + "/Tests/SentinelECS";

		// Win64, runtime-only (ToolsEnabled.False) -- the sentinel is a desktop
		// link-proof, no Android needed. It is deliberately a NON-tools build: the
		// ZENITH_TOOLS / ZENITH_DEBUG_VARIABLES branches of the shared PCH route
		// logging + memory-debug-UI through engine/editor symbols
		// (Zenith_EditorAddLogMessage, g_xEngine.DebugVariables()) that live in the
		// aggregate, NOT in either leaf lib -- those are editor instrumentation, not
		// the ECS's intrinsic dependency. A runtime config sheds them, so the leaf
		// proof reduces to the four pure-L0 platform symbols supplied by
		// sentinel_platform.cpp (Zenith_DebugBreak + three Zenith_FileAccess fns).
		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan | RenderBackend.D3D12
		});
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Drop Android platform files (there are none here, but keeps the
		// platform partition identical to every other project).
		ConfigurePlatformExcludes(conf, target);

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		// Distinct intermediate dir so the sentinel's obj never collides with the
		// aggregate / ZenithBase / ZenithECS obj trees (all rooted in Build\).
		conf.IntermediatePath = @"[conf.ProjectPath]\obj\SentinelECS\[target.Platform]\[conf.Name]";

		// Common configuration -- identical knobs to the libs it links, so the
		// single TU compiles the shared Zenith.h under the same flags/defines the
		// ECS core was compiled with (no ABI / macro skew across the link).
		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		ConfigureCommonLibraryPaths(conf, target);

		// LEAF-PROOF INTEGRITY: because LinksRendererBackend is overridden to false
		// (above), ConfigureCommonLibraryPaths did NOT add glfw3_mt/vulkan-1/slang for
		// this project, and the two leaf-lib dependencies below (also
		// LinksRendererBackend=false) do not propagate them either. So the sentinel
		// links EXACTLY zenithbase.lib + zenithecs.lib + the CRT/Win32 import libs the
		// toolchain adds + the four documented L0 platform shims in
		// Tests/SentinelECS/sentinel_platform.cpp. Any accidental ECS edge onto the
		// renderer (or anything beyond ZenithBase) now fails the link with a hard
		// undefined-symbol error -- which is the entire point of the sentinel.

		// PCH header transitively pulls imgui + Flux/Vulkan/vma/Jolt headers --
		// include paths only (no libs linked). Mirrors ZenithECS/ZenithBase.
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/Vulkan");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		// Executable (console subsystem by default for `int main()`).
		conf.Output = Configuration.OutputType.Exe;

		// THE WHOLE POINT: link EXACTLY the ECS leaf + the base leaf, nothing
		// more. No ZenithProject, no Flux, no Physics, no game deps. The ECS edge
		// (ZenithECS) re-exports ZenithBase publicly, so the second dependency is
		// technically transitive -- it is stated explicitly to make the
		// "links only these two" contract obvious and self-documenting.
		conf.AddPublicDependency<ZenithECSLibProject>(target);
		conf.AddPublicDependency<ZenithBaseLibProject>(target);

		// No ZENITH_TOOLS branch: this project is ToolsEnabled.False only (see
		// AddTargets above). The runtime PCH path needs no tools/editor include
		// paths, and compiling without ZENITH_TOOLS is exactly what keeps the leaf
		// proof honest -- the linked zenithbase.lib / zenithecs.lib are the _False
		// (non-tools) builds, so MemoryManagement does not reach g_xEngine and the
		// logger does not reach the editor.
	}
}
