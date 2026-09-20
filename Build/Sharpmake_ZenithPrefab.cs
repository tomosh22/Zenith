using Sharpmake;
using System;
using System.IO;

// ZenithPrefab — leaf static library (Phase 4c — see Sharpmake_ZenithCore.cs
// for the pattern this file replicates). Holds exactly Zenith/Prefab — 1
// translation unit (Zenith_Prefab.cpp: prefab creation/instantiation +
// variant/override serialization). Previously globbed straight into the
// monolithic aggregate ZenithProject.
//
// NO PRECOMPILED HEADER: 1-TU library, see Sharpmake_ZenithDebugVariables.cs's
// header comment for the full rationale (identical here). conf.PrecompHeader /
// conf.PrecompSource are deliberately left UNSET — verify NotUsing in the
// generated .vcxproj.
//
// DEPENDENCY: Zenith_Prefab.h #includes DataStream/Zenith_DataStream.h
// (ZenithDataStream), Maths/Zenith_Maths.h (ZenithMaths), and
// ZenithECS/Zenith_Entity.h + ZenithECS/Zenith_SceneData.h (ZenithECS).
// Zenith_Prefab.cpp additionally #includes Profiling/Zenith_Profiling.h
// (ZenithProfiling — a genuine extracted-library header) and
// Core/Zenith_Engine.h (ZenithCore, already mandatory). Its
// AssetHandling/Zenith_AssetHandle.inl and
// EntityComponent/Components/Zenith_TransformComponent.h #includes resolve
// include-path-only: neither AssetHandling nor EntityComponent is its own
// library yet (both still compile into the aggregate ZenithProject), so —
// exactly like ZenithDebugVariables's Flux_GraphicsImpl.h reach — no
// AddPublicDependency exists for either; the plain "/Zenith" include path
// every leaf already adds (ConfigureCommonIncludePaths) resolves them, and
// any out-of-line symbol they use resolves at the final game-exe link where
// the aggregate is always present transitively. So this lib depends on
// ZenithCore (mandatory) + ZenithDataStream + ZenithMaths + ZenithECS +
// ZenithProfiling.
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithPrefab".
[Sharpmake.Generate]
public class ZenithPrefabLibProject : ZenithBaseProject
{
	// Leaf static lib — renderer-agnostic; must not link or propagate
	// glfw/vulkan/slang, so ConfigureAll below calls none of the
	// AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs helpers.
	public ZenithPrefabLibProject()
	{
		Name = "ZenithPrefab";

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

		SourceRootPath = RootPath + "/Zenith/Prefab";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithPrefab\[target.Platform]\[conf.Name]";

		// No conf.PrecompHeader / conf.PrecompSource: see the file header
		// comment. Zenith_Prefab.cpp includes "Zenith.h" as a plain
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

		// Direct edges: Zenith_Prefab.h #includes DataStream/Zenith_DataStream.h,
		// Maths/Zenith_Maths.h, ZenithECS/Zenith_Entity.h + Zenith_SceneData.h;
		// Zenith_Prefab.cpp #includes Profiling/Zenith_Profiling.h.
		conf.AddPublicDependency<ZenithDataStreamLibProject>(target);
		conf.AddPublicDependency<ZenithMathsLibProject>(target);
		conf.AddPublicDependency<ZenithECSLibProject>(target);
		conf.AddPublicDependency<ZenithProfilingLibProject>(target);

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
