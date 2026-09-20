using Sharpmake;
using System;
using System.IO;

// ZenithDebugVariables — leaf static library (Phase 4b: the seven-library round
// that follows Phase 4a's ZenithBase dissolution — see Sharpmake_ZenithCore.cs
// for the pattern this file replicates). Holds exactly Zenith/DebugVariables —
// 1 translation unit (Zenith_DebugVariables.cpp: the runtime debug-variable
// tree). Previously globbed straight into the monolithic aggregate ZenithProject.
//
// NO PRECOMPILED HEADER: the >= 4-TU rule this refactor applies means a 1-TU
// library gains nothing from a binary PCH — it would pay a full glm parse
// building the PCH for a single consumer, worse than textually including
// Zenith.h once. conf.PrecompHeader / conf.PrecompSource are deliberately left
// UNSET; Zenith_DebugVariables.cpp compiles `#include "Zenith.h"` textually,
// exactly like the Sentinels and the game executables already do. (Verify in
// the generated .vcxproj: no PrecompiledHeader Create/Use here — it must say
// NotUsing.)
//
// DEPENDENCY: Zenith_DebugVariables.cpp's own #includes reach only
// Core/Zenith_Engine.h (ZenithCore) and Flux/Flux.h + Flux/Flux_GraphicsImpl.h
// for its debug-widget rendering hooks. Flux is NOT its own library yet (still
// part of the aggregate ZenithProject) — those two #includes resolve via the
// plain "/Zenith" include path every leaf already adds (ConfigureCommonIncludePaths),
// same as the vma/Jolt/RenderBackends/imgui include-path-only pattern the other
// leaves use for their own PCH resolution. No AddPublicDependency is needed for
// Flux because compiling a static lib never resolves external symbols (lib.exe
// does not link) — any Flux_:: symbol Zenith_DebugVariables.cpp calls out-of-line
// resolves at the final game exe link, where the aggregate (which still owns
// Flux) is always present transitively.
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithDebugVariables".
[Sharpmake.Generate]
public class ZenithDebugVariablesLibProject : ZenithBaseProject
{
	// Leaf static lib — renderer-agnostic; must not link or propagate
	// glfw/vulkan/slang, so ConfigureAll below calls none of the
	// AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs helpers.
	public ZenithDebugVariablesLibProject()
	{
		Name = "ZenithDebugVariables";

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

		SourceRootPath = RootPath + "/Zenith/DebugVariables";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		// Distinct intermediate dir — never share obj\[platform]\[conf] with
		// another lib.
		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithDebugVariables\[target.Platform]\[conf.Name]";

		// No conf.PrecompHeader / conf.PrecompSource: see the file header
		// comment. Zenith_DebugVariables.cpp includes "Zenith.h" as a plain
		// textual include.

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf lib must not link or propagate the renderer backend.

		// Zenith.h (textual include) transitively reaches Flux/Flux.h ->
		// Vulkan/vma/Jolt headers, and this TU directly includes
		// Flux/Flux_GraphicsImpl.h — include paths only, no libs linked.
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		// SHADER_SOURCE_ROOT: Flux_GraphicsImpl.h selects the active render
		// backend's Pipeline header (Zenith_Null_Pipeline.h /
		// Zenith_D3D12_Pipeline.h both reference SHADER_SOURCE_ROOT directly,
		// unguarded, at line 59 of each), and this TU is the only leaf lib
		// that #includes Flux_GraphicsImpl.h itself rather than reaching it
		// only through the PCH's Zenith.h -> Flux.h chain (which never reaches
		// the per-backend Pipeline headers). Found via a Null_ config build
		// (C2065 'SHADER_SOURCE_ROOT': undeclared identifier) — this define
		// previously existed only on the aggregate ZenithProject and each
		// GameProject, neither of which this standalone leaf lib is. Mirrors
		// ZenithProject's own identical win64/agde branch exactly (same
		// value, same "" on agde) so every other build.
		string zenithRoot = Path.GetFullPath(Path.Combine(SharpmakeCsPath, "..")).Replace('\\', '/');
		if (target.Platform == Platform.win64)
		{
			string shaderSourceRoot = zenithRoot + "/Zenith/Flux/Shaders/";
			conf.Defines.Add($"SHADER_SOURCE_ROOT=\"{shaderSourceRoot}\"");
		}
		else if (target.Platform == Platform.agde)
		{
			conf.Defines.Add("SHADER_SOURCE_ROOT=\"\"");
		}

		conf.Output = Configuration.OutputType.Lib;

		// MANDATORY: ZenithCore is an unconditional dependency of every
		// engine library (see Sharpmake_ZenithCore.cs's header comment) —
		// Zenith_LogFileSinkWrite, referenced from the inline Zenith_LogImpl
		// in the shared Zenith.h, must resolve for every TU that includes it.
		conf.AddPublicDependency<ZenithCoreLibProject>(target);

		// Tools define: lockstep across every consumer (ODR) — see
		// Sharpmake_ZenithCore.cs's header comment.
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
