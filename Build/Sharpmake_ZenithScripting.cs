using Sharpmake;
using System;
using System.IO;

// ZenithScripting — leaf static library (Phase 4c — see Sharpmake_ZenithCore.cs
// for the pattern this file replicates). Holds exactly Zenith/Scripting — the
// Behaviour Graph interpreter: 5 translation units (Zenith_BehaviourGraph.cpp,
// Zenith_GraphBlackboard.cpp, Zenith_GraphBuilder.cpp,
// Zenith_GraphDefinitionValidator.cpp, Zenith_GraphNodeRegistry.cpp) + the
// definition-free PCH create TU (Internal/Zenith_ScriptingPCH.cpp). Previously
// globbed straight into the monolithic aggregate ZenithProject.
//
// THE PCH: >= 4 compiled TUs justifies a binary PCH under this refactor's
// rule. ZenithScripting CREATEs its own Zenith.pch from
// Internal/Zenith_ScriptingPCH.cpp (a one-line `#include "Zenith.h"`, nothing
// else — mirrors Zenith/Physics/Internal/Zenith_PhysicsPCH.cpp and
// Zenith/Input/Internal/Zenith_InputPCH.cpp; it MUST stay definition-free,
// see that file's own header comment for why). The five real TUs /Yu against
// it.
//
// DEPENDENCY: Zenith_BehaviourGraph.cpp #includes ZenithECS/Zenith_SceneSystem.h
// directly (ResolveTargetEntity); Zenith_GraphNode.h #includes
// ZenithECS/Zenith_Entity.h. Zenith_GraphBlackboard.cpp #includes
// DataStream/Zenith_DataStream.h. Zenith_BehaviourGraph.h #includes
// Maths/Zenith_Maths.h. Collections/Zenith_HashMap.h + Zenith_Vector.h and
// Core/Zenith_PropertySystem.h are all ZenithCore (already mandatory). So
// this lib depends on ZenithCore (mandatory) + ZenithDataStream + ZenithMaths
// + ZenithECS — the same "ZenithCore-family leaves + ZenithECS only" layering
// Zenith/Scripting/CLAUDE.md already documents (it never names Flux, Physics,
// AssetHandling, or any concrete component — enforced by the architecture
// gate).
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithScripting".
[Sharpmake.Generate]
public class ZenithScriptingLibProject : ZenithBaseProject
{
	// Leaf static lib — renderer-agnostic; must not link or propagate
	// glfw/vulkan/slang, so ConfigureAll below calls none of the
	// AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs helpers.
	public ZenithScriptingLibProject()
	{
		Name = "ZenithScripting";

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

		SourceRootPath = RootPath + "/Zenith/Scripting";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithScripting\[target.Platform]\[conf.Name]";

		// Per-lib PCH: ZenithScripting CREATEs Zenith.pch from
		// Internal/Zenith_ScriptingPCH.cpp.
		conf.PrecompHeader = "Zenith.h";
		conf.PrecompSource = "Zenith_ScriptingPCH.cpp";

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf lib must not link or propagate the renderer backend.

		// The PCH (Zenith.h) transitively pulls imgui + Flux/Vulkan/vma/Jolt headers.
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		conf.Output = Configuration.OutputType.Lib;

		// MANDATORY: ZenithCore — see Sharpmake_ZenithCore.cs's header comment.
		conf.AddPublicDependency<ZenithCoreLibProject>(target);

		// Direct edges: Zenith_GraphBlackboard.cpp #includes
		// DataStream/Zenith_DataStream.h; Zenith_BehaviourGraph.h #includes
		// Maths/Zenith_Maths.h; Zenith_BehaviourGraph.cpp #includes
		// ZenithECS/Zenith_SceneSystem.h and Zenith_GraphNode.h #includes
		// ZenithECS/Zenith_Entity.h.
		conf.AddPublicDependency<ZenithDataStreamLibProject>(target);
		conf.AddPublicDependency<ZenithMathsLibProject>(target);
		conf.AddPublicDependency<ZenithECSLibProject>(target);

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
