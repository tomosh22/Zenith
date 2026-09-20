using Sharpmake;
using System;
using System.IO;

// ZenithPhysics — the Physics leaf static library. Carved out of the monolithic
// engine lib the same way ZenithECS (L1) and ZenithBase (L0) were, WITHOUT
// changing runtime behaviour. It holds the Physics CORE only: the contents of
// Zenith/Physics (the Jolt-backed simulation manager + the renderer-neutral
// collision-mesh generator) — 4 translation units total (the 3 real Physics
// .cpp + its own PCH create TU). The engine-side glue that needs concrete
// components (Zenith_PhysicsQuery, Zenith_PhysicsDebugDraw) stays behind in
// Zenith/EntityComponent and compiles into the aggregate engine lib.
//
// LAYERING: ZenithPhysics is a strict leaf over the L0 leaves (ZenithCore +
// ZenithMaths directly; ZenithDataStream/ZenithFileAccess transitively via
// ZenithECS — see the dependency comment below) + ZenithECS (L1) + ZenithJolt
// (Phase 4c: the vendored Jolt backend split OUT into its own leaf library —
// see Sharpmake_ZenithJolt.cs — this project no longer owns those sources,
// it depends on the library that does). It must NOT name g_xEngine, Flux, AI,
// UI, Editor, AssetHandling, or any concrete component — that is the whole
// point of the leaf boundary, proven by the SentinelPhysics link-proof exe +
// the dumpbin FORBIDDEN_EXTERNALS scan.
//
// THE PCH: like ZenithBase/ZenithECS, this lib compiles its OWN per-lib Zenith.pch
// from a trivial Create TU (Internal/Zenith_PhysicsPCH.cpp = `#include "Zenith.h"`).
// It sits exactly AT the >= 4-TU PCH threshold (3 Physics TUs + the PCH create
// TU itself) — do not let this shrink further without revisiting the PCH.
[Sharpmake.Generate]
public class ZenithPhysicsLibProject : ZenithBaseProject
{
	// Renderer-agnostic leaf static lib — must not link or propagate
	// glfw/vulkan/slang, so ConfigureAll below calls none of the
	// renderer-backend library helpers.
	public ZenithPhysicsLibProject()
	{
		Name = "ZenithPhysics";

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

		// The whole Physics directory. The "Zenith.h" PrecompHeader lookup resolves
		// through ConfigureCommonIncludePaths (which adds /Zenith + /Zenith/Core).
		// Phase 4c: the Jolt AdditionalSourceRootPaths + its nine membership
		// excludes moved to Sharpmake_ZenithJolt.cs, which now owns that tree —
		// this project depends on it instead (see the AddPublicDependency below).
		SourceRootPath = RootPath + "/Zenith/Physics";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		// Distinct intermediate dir so this lib's .obj + private Zenith.pch never
		// collide with ZenithBase / ZenithECS / the aggregate (all rooted in Build\).
		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithPhysics\[target.Platform]\[conf.Name]";

		// Per-lib PCH: ZenithPhysics CREATEs Zenith.pch from Zenith_PhysicsPCH.cpp.
		conf.PrecompHeader = "Zenith.h";
		conf.PrecompSource = "Zenith_PhysicsPCH.cpp";
		// Phase 4c: the Jolt PrecompSourceExcludeFolders entry that used to sit here
		// is gone — the Jolt tree is no longer part of THIS project's source set at
		// all (ZenithJoltLibProject owns it now, with no PCH of its own), so there
		// is nothing in this lib's own glob left to exclude from its PCH.

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf lib must not link or propagate the renderer backend.

		// Phase 5.3 CORRECTION: the comment this replaced claimed "the PCH
		// (Zenith.h) transitively pulls imgui + Flux/Vulkan/vma/Jolt headers".
		// Zenith.h's own closure carries none of the four (grep: 17 project
		// headers + ~202 glm headers). The Jolt path below IS still needed,
		// but for the reason this comment already named correctly — the
		// Physics .cpp themselves #include the Jolt headers directly — not
		// because of any PCH reach. vma/imgui-docking/RenderBackends had no
		// such justification (grep finds zero imgui/vulkan/vma references
		// anywhere under Zenith/Physics) and are removed, verified by a green
		// SentinelPhysics build in every RenderBackend x win64 plus a green
		// `zenith build engine`.
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");

		conf.Output = Configuration.OutputType.Lib;

		// L0 + L1 edges. ZenithCore is MANDATORY for every engine lib (see
		// Sharpmake_ZenithCore.cs — Zenith_LogFileSinkWrite must resolve for
		// every TU). ZenithMaths is a direct dependency: the Physics core's
		// own TUs call glm:: math through Zenith_Maths types (collision
		// shapes, transforms). ZenithDataStream/ZenithFileAccess are NOT
		// listed explicitly — Physics's own sources reference neither
		// directly, and ZenithECS (below) already re-exports them publicly,
		// so they still reach any final link that includes this lib.
		// ZenithECS publicly re-exports ZenithCore/ZenithMaths/ZenithDataStream/
		// ZenithFileAccess too, so the ZenithCore/ZenithMaths edges here are
		// technically transitive; stated explicitly to mirror ZenithProject
		// and to document Physics's actual direct usage.
		conf.AddPublicDependency<ZenithCoreLibProject>(target);
		conf.AddPublicDependency<ZenithMathsLibProject>(target);
		conf.AddPublicDependency<ZenithECSLibProject>(target);

		// Phase 4c: ZenithJolt — the Jolt backend now lives in its own leaf
		// library (Sharpmake_ZenithJolt.cs) instead of compiling as part of
		// this project. PUBLIC so every JPH:: symbol Physics's own TUs call
		// out-of-line resolves at the consuming link (SentinelPhysics, every
		// game exe) without those consumers needing their own Jolt edge.
		conf.AddPublicDependency<ZenithJoltLibProject>(target);

		// Tools define: ZenithPhysics carries no tools-only code, but the shared PCH
		// (Zenith.h) branches on ZENITH_TOOLS, so the binary .pch must be built with
		// the same define as the consumers in a True config. Mirrors ZenithECS.
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
