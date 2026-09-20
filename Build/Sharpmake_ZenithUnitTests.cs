using Sharpmake;
using System;
using System.IO;

// ZenithUnitTests — leaf static library (Phase 4b — see Sharpmake_ZenithCore.cs
// for the pattern this file replicates). Holds exactly Zenith/UnitTests — 1
// translation unit (Zenith_AssertCapture.cpp: the assert-capture atomics used by
// negative-path unit tests) — AFTER Zenith_EditorTestFixture.cpp/.h moved OUT to
// Zenith/Editor/ (see that move's note below). Previously globbed straight into
// the monolithic aggregate ZenithProject.
//
// THE MOVE THAT MADE THIS A CLEAN LEAF: Zenith/UnitTests/Zenith_EditorTestFixture.*
// was the SOLE source of a UnitTests -> Editor / -> EntityComponent / -> Input
// header edge (it #included Editor/Zenith_Editor.h, Editor/Zenith_UndoSystem.h,
// Editor/Zenith_EditorSceneAccess.h, EntityComponent/Components/Zenith_TransformComponent.h,
// Input/Zenith_InputSimulator.h) and is consumed only by three Editor test .inl
// files (Zenith_EditorAutomation.Tests.inl, Zenith_Editor.Tests.inl,
// Zenith_EditorCommands.Tests.inl). It has been `git mv`-ed to
// Zenith/Editor/Zenith_EditorTestFixture.{h,cpp} (see that directory's own
// project) and the three consumers' #include updated from
// "UnitTests/Zenith_EditorTestFixture.h" to "Editor/Zenith_EditorTestFixture.h".
// Re-measured: Zenith/UnitTests/** now includes nothing from Editor/,
// EntityComponent/, or Input/ — verified by grep across the remaining four
// files (Zenith_AssertCapture.cpp/.h, Zenith_AITests.h, Zenith_TempScene.h,
// Zenith_UnitTests.h). The two remaining headers that DO reach outward
// (Zenith_AITests.h -> AI/Navigation/Zenith_NavMeshGenerator.h; Zenith_TempScene.h
// -> ZenithECS/*) are consumed by OTHER projects that already declare those
// dependencies themselves (this library's own single TU never includes either) —
// headers-only inclusion needs no AddPublicDependency edge from a static lib
// that never compiles them into its own .obj.
//
// NO PRECOMPILED HEADER: 1-TU library, see Sharpmake_ZenithDebugVariables.cs's
// header comment for the full rationale (identical here). conf.PrecompHeader /
// conf.PrecompSource are deliberately left UNSET — verify NotUsing in the
// generated .vcxproj.
//
// DEPENDENCY: Zenith_AssertCapture.cpp #includes only its own header (atomics,
// <cstdint>) plus Zenith.h — no engine dependency beyond the mandatory
// ZenithCore edge.
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithUnitTests".
[Sharpmake.Generate]
public class ZenithUnitTestsLibProject : ZenithBaseProject
{
	// Leaf static lib — renderer-agnostic; must not link or propagate
	// glfw/vulkan/slang, so ConfigureAll below calls none of the
	// AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs helpers.
	public ZenithUnitTestsLibProject()
	{
		Name = "ZenithUnitTests";

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

		SourceRootPath = RootPath + "/Zenith/UnitTests";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithUnitTests\[target.Platform]\[conf.Name]";

		// No conf.PrecompHeader / conf.PrecompSource: see the file header
		// comment. Zenith_AssertCapture.cpp includes "Zenith.h" as a plain
		// textual include.

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf lib must not link or propagate the renderer backend.

		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		conf.Output = Configuration.OutputType.Lib;

		// MANDATORY: ZenithCore — see Sharpmake_ZenithCore.cs's header comment.
		conf.AddPublicDependency<ZenithCoreLibProject>(target);

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
