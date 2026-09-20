using Sharpmake;
using System;
using System.IO;

// ZenithJolt — the vendored Jolt Physics backend, split into its own leaf
// static library (Phase 4c). Applying "one library per directory"
// consistently to the vendored Middleware/JoltPhysics-5.4.0 tree the same way
// every other subsystem directory was extracted: previously it compiled AS
// PART OF ZenithPhysicsLibProject via AdditionalSourceRootPaths (see the
// "SCAFFOLD PHASE" / "ZenithPhysics OWNS the Jolt backend" comments this file
// makes stale in Sharpmake_ZenithPhysics.cs). Jolt's own 133 in-scope .cpp
// (the runtime backend under Jolt/ — Build/Docs/UnitTests/Samples/
// TestFramework/PerformanceTest/JoltViewer/HelloWorld excluded, same nine
// membership regexes as before, just now living on the project that owns
// that tree) now compile here instead.
//
// NO PRECOMPILED HEADER, EVER: not a >= 4-TU judgement call — Jolt's sources
// do not, and must not, #include "Zenith.h" at all (confirmed: zero hits for
// "Zenith" across the whole Jolt/ tree). That is exactly why ZenithPhysics
// excluded the Jolt folder from ITS OWN PrecompSourceExcludeFolders before
// this split, and exactly why AGDE/clang's whole-file `-xc++-header`
// precompile of a binary .pch would be actively wrong here even at 133 TUs:
// there is no Zenith master header for Jolt to share. conf.PrecompHeader /
// conf.PrecompSource are deliberately left unset.
//
// DEPENDENCY: NONE. Every other library in this refactor carries a mandatory
// ZenithCore edge because every TU that compiles against the shared PCH
// references the inline Zenith_LogImpl -> Zenith_LogFileSinkWrite (see
// Sharpmake_ZenithCore.cs). That justification does not hold here: no Jolt TU
// ever includes Zenith.h, so none references ANY Zenith:: symbol, so there is
// no honest edge to declare — adding one anyway would misstate what this
// library actually touches. ZenithJolt is genuinely dependency-free vendored
// third-party code; ZenithPhysics is the one consumer that links it (see
// Sharpmake_ZenithPhysics.cs's own new AddPublicDependency<ZenithJoltLibProject>
// edge).
//
// ConfigureCommonSettings/ConfigureCommonIncludePaths ARE still called below,
// deliberately: they are what already governed these same TUs while they
// compiled inside ZenithPhysicsLibProject (TreatWarningsAsErrors, the C++20
// standard, the platform/render-backend defines, the glm/stb/vma include
// paths) — Jolt does not consume the extra paths or the Zenith-specific
// defines, but calling the same two helpers keeps every compiler flag
// byte-for-byte identical to before the split, which is the point (no
// behaviour change, only an ownership move).
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithJolt".
[Sharpmake.Generate]
public class ZenithJoltLibProject : ZenithBaseProject
{
	// Leaf static lib — renderer-agnostic; must not link or propagate
	// glfw/vulkan/slang, so ConfigureAll below calls none of the
	// AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs helpers.
	public ZenithJoltLibProject()
	{
		Name = "ZenithJolt";

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

		// The whole vendored Jolt tree. Moved here from
		// Sharpmake_ZenithPhysics.cs's AdditionalSourceRootPaths — this is now
		// the project's PRIMARY SourceRootPath (not an additional root), since
		// this project exists to own exactly this directory.
		SourceRootPath = RootPath + "/Middleware/JoltPhysics-5.4.0";

		// Jolt's own samples / tests / tools are not part of the runtime
		// backend — the same nine membership regexes ZenithPhysics carried
		// before the split, moved verbatim (vendored-TREE membership excludes,
		// legitimate on whichever project owns the tree).
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
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithJolt\[target.Platform]\[conf.Name]";

		// NO PCH — see the file header comment. conf.PrecompHeader /
		// conf.PrecompSource are deliberately left unset; verify NotUsing on
		// every Jolt TU in the generated .vcxproj.

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf lib must not link or propagate the renderer backend.

		// Jolt's own sources #include <Jolt/...> (angle brackets, rooted at
		// this directory) — e.g. Jolt/AABBTree/AABBTreeBuilder.cpp includes
		// <Jolt/Jolt.h>. This is the path that resolves it.
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");

		conf.Output = Configuration.OutputType.Lib;

		// No AddPublicDependency calls at all — see the file header comment
		// ("DEPENDENCY: NONE").

		// Tools define: lockstep across every consumer (ODR) — see
		// Sharpmake_ZenithCore.cs's header comment. ZenithJolt's own TUs never
		// read ZENITH_TOOLS (they never see Zenith.h), but this define also
		// gates the Tools middleware include paths below, which cost nothing
		// to add here and keep this project's True-config shape identical to
		// every other leaf lib for anyone diffing the generated .vcxproj.
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
