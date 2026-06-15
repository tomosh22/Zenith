using Sharpmake;
using System;
using System.IO;

// ZenithPhysics — the Physics leaf static library. Carved out of the monolithic
// engine lib the same way ZenithECS (L1) and ZenithBase (L0) were, WITHOUT
// changing runtime behaviour. It holds the Physics CORE only: the contents of
// Zenith/Physics (the Jolt-backed simulation manager + the renderer-neutral
// collision-mesh generator). The engine-side glue that needs concrete components
// (Zenith_PhysicsQuery, Zenith_PhysicsDebugDraw) stays behind in
// Zenith/EntityComponent and compiles into the aggregate engine lib.
//
// LAYERING: ZenithPhysics is a strict leaf over ZenithBase (L0) + ZenithECS (L1)
// (+ Jolt, which it OWNS once the Phase-3 partition moves the Jolt sources here).
// It must NOT name g_xEngine, Flux, AI, UI, Editor, AssetHandling, or any concrete
// component — that is the whole point of the leaf boundary, proven by the
// SentinelPhysics link-proof exe + the dumpbin FORBIDDEN_EXTERNALS scan.
//
// SCAFFOLD PHASE (P2): this project compiles ONLY the Physics .cpp set. The Jolt
// sources still live in the aggregate; the Physics TUs therefore have undefined
// JPH:: externals here — harmless for a static lib (lib.exe does not resolve
// externals). The Phase-3 partition moves Jolt ownership into this lib and adds
// the matching aggregate build-excludes, making the two memberships a clean
// complement. Until then, Physics .cpp compile into BOTH this lib AND the
// aggregate (a deliberate, temporary duplicate-symbol state that is never
// co-linked — games depend only on the aggregate this phase).
//
// THE PCH: like ZenithBase/ZenithECS, this lib compiles its OWN per-lib Zenith.pch
// from a trivial Create TU (Internal/Zenith_PhysicsPCH.cpp = `#include "Zenith.h"`).
[Sharpmake.Generate]
public class ZenithPhysicsLibProject : ZenithBaseProject
{
	// Renderer-agnostic leaf static lib — must not link or propagate glfw/vulkan/slang.
	protected override bool LinksRendererBackend => false;

	public ZenithPhysicsLibProject()
	{
		Name = "ZenithPhysics";

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

		// The whole Physics directory. The "Zenith.h" PrecompHeader lookup resolves
		// through ConfigureCommonIncludePaths (which adds /Zenith + /Zenith/Core).
		SourceRootPath = RootPath + "/Zenith/Physics";

		// ZenithPhysics OWNS the Jolt backend (moved out of the aggregate at the
		// Phase-3 partition): the simulation core that names JPH:: lives here, so the
		// lib is self-contained over ZenithBase + ZenithECS + Jolt and the
		// SentinelPhysics link-proof resolves every JPH:: symbol without the engine.
		AdditionalSourceRootPaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");

		// Jolt's own samples / tests / tools are not part of the runtime backend.
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\Build.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\Docs.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\UnitTests.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\Samples.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\TestFramework.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\PerformanceTest.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\JoltViewer.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\HelloWorld.*");
		SourceFilesExcludeRegex.Add(@".*cmake.*");
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		ConfigurePlatformExcludes(conf, target);

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		// Distinct intermediate dir so this lib's .obj + private Zenith.pch never
		// collide with ZenithBase / ZenithECS / the aggregate (all rooted in Build\).
		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithPhysics\[target.Platform]\[conf.Name]";

		// Per-lib PCH: ZenithPhysics CREATEs Zenith.pch from Zenith_PhysicsPCH.cpp.
		conf.PrecompHeader = "Zenith.h";
		conf.PrecompSource = "Zenith_PhysicsPCH.cpp";
		// Jolt's third-party .cpp do not begin with `#include "Zenith.h"`, so they
		// must compile WITHOUT the engine PCH (mirrors the aggregate's handling).
		conf.PrecompSourceExcludeFolders.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		ConfigureCommonLibraryPaths(conf, target);

		// The PCH (Zenith.h) transitively pulls imgui + Flux/Vulkan/vma/Jolt headers,
		// and the Physics .cpp themselves include the Jolt headers directly. Include
		// paths only — no libs linked (undefined JPH:: resolve at the consuming link).
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/Vulkan");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		conf.Output = Configuration.OutputType.Lib;

		// L0 + L1 edges. ZenithECS publicly re-exports ZenithBase, so the base edge
		// is technically transitive; stated explicitly to mirror ZenithProject.
		conf.AddPublicDependency<ZenithBaseLibProject>(target);
		conf.AddPublicDependency<ZenithECSLibProject>(target);

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
