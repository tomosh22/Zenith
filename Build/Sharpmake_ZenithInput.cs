using Sharpmake;
using System;
using System.IO;

// ZenithInput — leaf static library (Phase 4b — see Sharpmake_ZenithCore.cs for
// the pattern this file replicates). Holds exactly Zenith/Input — 4 translation
// units (Zenith_Input.cpp, Zenith_InputActions.cpp, Zenith_InputSimulator.cpp,
// Zenith_Pointers.cpp) + the definition-free PCH create TU
// (Internal/Zenith_InputPCH.cpp). Previously globbed straight into the
// monolithic aggregate ZenithProject.
//
// THE PCH: >= 4 compiled TUs justifies a binary PCH under this refactor's rule.
// ZenithInput CREATEs its own Zenith.pch from Internal/Zenith_InputPCH.cpp (a
// one-line `#include "Zenith.h"`, nothing else — see that file's own header
// comment for why it must stay definition-free). The four real TUs /Yu against
// it.
//
// DEPENDENCY: none of Zenith_Input.cpp / Zenith_InputActions.cpp /
// Zenith_InputSimulator.cpp / Zenith_Pointers.cpp #include anything outside
// Input/ and Zenith_Core.h (ZenithCore) — no Windows/, Android/, Profiling/,
// DebugVariables/, or TaskSystem/ headers. So this lib depends on ZenithCore
// only.
//
// GLFW IS DELIBERATELY NOT LINKED HERE, EVEN THOUGH IT IS CALLED DIRECTLY: see
// Sharpmake_ZenithWindows.cs's header comment ("ZenithWindows owns GLFW") for
// the full account. Short version — glfw3_mt.lib is called directly from FOUR
// directories (Zenith/Windows/, Zenith/Input/, the Vulkan backend, and formerly
// Zenith_Core.cpp), but only ZenithWindows calls AddGlfwLibs this phase. That
// leaves ZenithInput (and the Vulkan backend) as libraries with undeclared
// win64 glfw symbols — accepted, not a bug, because the composition root that
// every game links (the aggregate) always carries ZenithWindows transitively
// too, so the symbol resolves at the final exe link. A future ZenithInput
// Sentinel would need ZenithWindows added to its own link set to prove this
// leaf standalone; today no such Sentinel exists.
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithInput".
[Sharpmake.Generate]
public class ZenithInputLibProject : ZenithBaseProject
{
	// Leaf static lib — must not link or propagate glfw/vulkan/slang (see the
	// header comment above for why GLFW specifically is not linked here even
	// though it is called directly), so ConfigureAll below calls none of the
	// AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs helpers.
	public ZenithInputLibProject()
	{
		Name = "ZenithInput";

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

		SourceRootPath = RootPath + "/Zenith/Input";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithInput\[target.Platform]\[conf.Name]";

		// Per-lib PCH: ZenithInput CREATEs Zenith.pch from Internal/Zenith_InputPCH.cpp.
		conf.PrecompHeader = "Zenith.h";
		conf.PrecompSource = "Zenith_InputPCH.cpp";

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf lib must not link or propagate the renderer backend (see
		// the header comment on why GLFW itself is excluded despite being
		// called directly from this directory).

		// The PCH (Zenith.h) transitively pulls Zenith_DebugVariables.h ->
		// imgui.h AND Flux/Flux.h -> Vulkan/vma/Jolt headers.
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

			conf.PrecompSourceExcludeFolders.Add(RootPath + "/Tools");
		}
	}
}
