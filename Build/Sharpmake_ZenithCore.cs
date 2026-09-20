using Sharpmake;
using System;
using System.IO;

// ZenithCore — bottom-leaf static library (Phase 4a of the bounded lib-split:
// ZenithBase dissolution). ZenithBase used to span FIVE directories (Maths,
// Collections, DataStream, FileAccess, and a subtractive-regex SLICE of Core)
// held together by nothing but a negative-lookahead regex over Core's
// top-level .cpp files. That split-directory, subtractive membership is
// exactly what this refactor removes: ZenithCore now owns Zenith/Core WHOLE
// (every top-level .cpp and every subdirectory — Callstack/, Memory/,
// Multithreading/, all of it, no exclusion regex) plus Zenith/Collections'
// headers (zero .cpp there; it joins purely so its headers belong to exactly
// one project for IDE browsing). Maths, DataStream and FileAccess are now
// their OWN leaf libs (Sharpmake_ZenithMaths.cs / _ZenithDataStream.cs /
// _ZenithFileAccess.cs) — see those files' header comments for why they carry
// no PCH.
//
// THE PCH: every engine .cpp begins `#include "Zenith.h"` and builds with
// MSVC /Yu"Zenith.h". Zenith.h is the bottom-most include, so its owning
// compile TU (Zenith.cpp) lives here, unchanged from ZenithBase. ZenithCore
// CREATEs Zenith.pch (PrecompiledHeader=Create on Zenith.cpp); the aggregate
// engine lib and every other consumer compile their OWN Zenith.pch from the
// same shared header (PrecompiledHeader=Use) — a per-lib PCH, because the
// binary .pch cannot be shared across projects that compile with different
// flags (e.g. ZENITH_TOOLS).
//
// WHY A PCH HERE AND NOT ON THE OTHER THREE NEW LIBS: the rule this refactor
// applies is >= 4 compiled TUs justifies a binary PCH. ZenithCore compiles
// ~15 TUs (see the .cpp list under Zenith/Core below); ZenithMaths/
// ZenithDataStream/ZenithFileAccess compile 1/2/1 TU respectively — a binary
// PCH for those would pay a full glm parse TWICE (once building the PCH,
// once — nothing — using it, since there is only the one TU) for zero reuse.
// Those three compile Zenith.h textually instead, exactly like the Sentinels
// and the game executables already do.
//
// MANDATORY DEPENDENCY (see Sharpmake_Zenith.cs / Sharpmake_EngineLibs.cs):
// every OTHER engine lib and every Sentinel depends on ZenithCoreProject
// unconditionally, even where the edge would be transitive anyway, because
// Zenith_LogFileSinkWrite (defined in Zenith.cpp, which lives HERE) is called
// from the *inline* Zenith_LogImpl in the shared Zenith.h PCH header — so
// every translation unit in every library emits a reference to it. Do not
// prune this edge from any consumer "because it looks redundant".
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithCore".
[Sharpmake.Generate]
public class ZenithCoreLibProject : ZenithBaseProject
{
	// L0 leaf static lib — renderer-agnostic; must not link or propagate
	// glfw/vulkan/slang, so ConfigureAll below calls none of the
	// AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs helpers.
	public ZenithCoreLibProject()
	{
		Name = "ZenithCore";

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

		// Whole-directory membership, additive and local — no subtractive
		// regex needed (that mechanism is what this refactor removes). Core
		// currently holds 15 .cpp: the 9 top-level Core files (Zenith.cpp,
		// Zenith_String.cpp, Zenith_AudioBus.cpp, Zenith_CommandLine.cpp,
		// Zenith_GraphicsOptions.cpp, Zenith_ImGuiWidgets.cpp,
		// Zenith_PropertySystem.cpp, Zenith_TestFramework.cpp, Zenith_Tween.cpp)
		// + Callstack/Zenith_Callstack.cpp + Memory/'s 4 .cpp +
		// Multithreading/Zenith_Multithreading.cpp.
		SourceRootPath = RootPath + "/Zenith/Core";

		// Collections is headers-only (zero .cpp) — added purely so its
		// headers are owned by exactly one project for IDE browsing, not
		// because it contributes any TU to this lib's compile.
		AdditionalSourceRootPaths.Add(RootPath + "/Zenith/Collections");
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		// Distinct intermediate dir. ZenithCore and every other lib living in
		// the Build dir otherwise default to the SAME obj tree
		// (obj\[platform]\[conf]) — fatal because each CREATEs a Zenith.pch
		// there and they would clobber each other (MSB8028 + PCH-collision).
		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithCore\[target.Platform]\[conf.Name]";

		// Per-lib PCH: ZenithCore CREATEs Zenith.pch from Zenith.cpp. Every
		// other ZenithCore TU compiles with /Yu"Zenith.h" against it.
		conf.PrecompHeader = "Zenith.h";
		conf.PrecompSource = "Zenith.cpp";

		// Common configuration — identical knobs to the engine lib so the
		// Core TUs compile under the exact same flags/defines/include paths
		// they did inside the monolith (no behavioural delta).
		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf lib must not link or propagate the renderer backend.

		// The PCH (Zenith.h) transitively pulls Zenith_DebugVariables.h →
		// imgui.h AND Flux/Flux.h → Vulkan/vma/Jolt headers, so ZenithCore
		// needs the SAME extra include paths the aggregate adds for its own
		// PCH compile. These are include paths only — ZenithCore links none
		// of these libs; the -I flags merely let the PCH headers resolve.
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		conf.Output = Configuration.OutputType.Lib;

		// No AddPublicDependency<ZenithCoreProject> here, obviously — this
		// IS ZenithCore, the bottom of the leaf DAG. Nothing in Zenith/Core
		// or Zenith/Collections references another leaf's out-of-line symbol
		// (verified: the only g_xEngine references left under Zenith/Core are
		// in Zenith_Multithreading.cpp / Zenith_GraphicsOptions.cpp, and
		// neither .obj is pulled into a Sentinel link because nothing the
		// Sentinels transitively call references them — static libs only
		// pull in the .obj members needed to resolve a currently-unresolved
		// symbol).

		// Tools define: the PCH it compiles (Zenith.h) branches on
		// ZENITH_TOOLS, so the binary .pch must be built with the same define
		// as the consumers in a True config — otherwise the per-lib PCH would
		// diverge. Keep it in lockstep with the engine lib (mirrors the old
		// ZenithBase).
		if (target.ToolsEnabled == ToolsEnabled.True && target.Platform == Platform.win64)
		{
			conf.Defines.Add("ZENITH_TOOLS");

			// The ZENITH_TOOLS PCH branch may pull tools/editor headers; mirror
			// the aggregate's tools include paths so the per-lib PCH resolves
			// identically in True. Include paths only (no tool libs linked).
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/assimp/include");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/freetype/include");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen/msdfgen");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen/msdfgen/Config");

			// Exclude Tools from the PCH create (no tools sources here anyway).
			conf.PrecompSourceExcludeFolders.Add(RootPath + "/Tools");
		}
	}
}
