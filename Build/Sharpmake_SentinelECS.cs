using Sharpmake;
using System;
using System.IO;

// SentinelECS -- leaf-proof executable for the ECS-leaf extraction (Phase 9b).
//
// This exe exists for ONE reason: to prove that ZenithECS is a self-contained
// leaf on top of ZenithCore (formerly ZenithBase, dissolved Phase 4a), with
// NO undefined engine externals. It links EXACTLY zenithecs.lib +
// zenithcore.lib (the two AddPublicDependency calls below; ZenithECS also
// re-exports zenithmaths.lib/zenithdatastream.lib/zenithfileaccess.lib
// transitively) and NOTHING else -- no ZenithProject (the aggregate engine lib), no
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
	// never the renderer backend (glfw/vulkan/slang). ConfigureAll below calls
	// none of the renderer-backend library helpers, and the two leaf-lib
	// dependencies (which also call none of them) do not propagate those libs
	// either, so any accidental ECS->renderer edge surfaces as a hard
	// undefined-symbol error.
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
			RenderBackend = RenderBackend.Vulkan | RenderBackend.D3D12 | RenderBackend.Null
		});
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

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
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf-proof exe must not link or propagate the renderer backend.

		// LEAF-PROOF INTEGRITY: because ConfigureAll calls none of the
		// renderer-backend library helpers, this project did NOT add
		// glfw3_mt/vulkan-1/slang, and the two leaf-lib dependencies below
		// (which also call none of them) do not propagate them either. So the
		// sentinel links EXACTLY zenithbase.lib + zenithecs.lib + the CRT/Win32
		// import libs the
		// toolchain adds + the four documented L0 platform shims in
		// Tests/SentinelECS/sentinel_platform.cpp. Any accidental ECS edge onto the
		// renderer (or anything beyond ZenithBase) now fails the link with a hard
		// undefined-symbol error -- which is the entire point of the sentinel.

		// Phase 5.3 CORRECTION: this comment (and Sharpmake_ZenithECS.cs's
		// identical one) claimed "the PCH transitively pulls imgui + Flux/
		// Vulkan/vma/Jolt headers". Zenith.h's own closure is 17 project
		// headers + ~202 glm headers -- zero imgui/Flux/Vulkan/vma/Jolt -- and
		// this sentinel's own main.cpp reaches only ZenithECS + DataStream
		// headers, none of which touch any of the four vendored trees either.
		// All four IncludePaths.Add calls were dead weight; removed, verified
		// by a green build in every RenderBackend (Vulkan_/Null_/D3D12_) x
		// win64.

		// Executable (console subsystem by default for `int main()`).
		conf.Output = Configuration.OutputType.Exe;

		// THE WHOLE POINT: link EXACTLY the ECS leaf + ZenithCore, nothing
		// more. No ZenithProject, no Flux, no Physics, no game deps. The ECS
		// edge (ZenithECS) re-exports ZenithCore/ZenithMaths/ZenithDataStream/
		// ZenithFileAccess publicly, so the ZenithCore dependency here is
		// technically transitive -- it is stated explicitly per the mandatory
		// rule (Sharpmake_ZenithCore.cs: Zenith_LogFileSinkWrite must resolve
		// for every TU) and to make the "links only these" contract obvious.
		conf.AddPublicDependency<ZenithECSLibProject>(target);
		conf.AddPublicDependency<ZenithCoreLibProject>(target);

		// No ZENITH_TOOLS branch: this project is ToolsEnabled.False only (see
		// AddTargets above). The runtime PCH path needs no tools/editor include
		// paths, and compiling without ZENITH_TOOLS is exactly what keeps the leaf
		// proof honest -- the linked zenithbase.lib / zenithecs.lib are the _False
		// (non-tools) builds, so MemoryManagement does not reach g_xEngine and the
		// logger does not reach the editor.
	}
}
