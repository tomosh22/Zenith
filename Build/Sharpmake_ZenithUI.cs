using Sharpmake;
using System;
using System.IO;

// ZenithUI — leaf static library (Phase 4f — see Sharpmake_ZenithCore.cs for
// the pattern this file replicates, and Sharpmake_ZenithFlux.cs /
// Sharpmake_ZenithAssetHandling.cs for the two closest-shaped siblings).
// Holds exactly Zenith/UI — 14 translation units (Zenith_UIButton.cpp,
// Zenith_UICanvas.cpp, Zenith_UIElement.cpp, Zenith_UIGridLayoutGroup.cpp,
// Zenith_UIImage.cpp, Zenith_UILayoutGroup.cpp, Zenith_UIOverlay.cpp,
// Zenith_UIRect.cpp, Zenith_UIScrollView.cpp, Zenith_UIStyleRenderer.cpp,
// Zenith_UIText.cpp, Zenith_UIToggle.cpp, Zenith_UIVirtualButton.cpp,
// Zenith_UIVirtualStick.cpp) + the definition-free PCH create TU
// (Internal/Zenith_UIPCH.cpp). 14 + 1 = 15 compiled TUs. Previously globbed
// straight into the monolithic aggregate ZenithProject. Single flat
// directory — no subdirectories, no AdditionalSourceRootPaths needed.
// Zenith/UI/Zenith_UI*.Tests.inl are #included by their sibling .cpp (e.g.
// Zenith_UIButton.Tests.inl by Zenith_UIButton.cpp) — not separate members.
//
// THE PCH: >= 4 compiled TUs justifies a binary PCH under this refactor's
// rule. ZenithUI CREATEs its own Zenith.pch from Internal/Zenith_UIPCH.cpp (a
// one-line `#include "Zenith.h"`, nothing else — mirrors
// Zenith/Scripting/Internal/Zenith_ScriptingPCH.cpp; it MUST stay
// definition-free, see that file's own header comment for why). The 14 real
// TUs /Yu against it.
//
// LAYER 5 — UI sits directly below EntityComponent (layer 6, this same
// phase's other new library) in the measured header DAG: EntityComponent's
// Zenith_UIComponent.h #includes UI/Zenith_UI.h (a pre-existing grandfathered
// `layer-up:` allowlist entry in Tools/architecture_allowlist.txt records the
// opposite direction is NOT a thing — UI never reaches EntityComponent), and
// EntityComponent's Zenith_UISystem.cpp drives every Zenith_UIComponent's
// canvas through this library's Zenith_UICanvas/Zenith_UIElement surface. So
// ZenithEntityComponent (Sharpmake_ZenithEntityComponent.cs) declares
// AddPublicDependency<ZenithUILibProject>; this project does NOT declare the
// reverse — a clean one-way edge, not a cycle.
//
// DEPENDENCY (direct #include edges, same "declare what is directly used"
// convention every phase-4 leaf follows — grep across every Zenith/UI/*.cpp
// and *.h): Zenith_UIStyleRenderer.h / Zenith_UIElement.h / several others
// #include Maths/Zenith_Maths.h directly; Zenith_UICanvas.cpp #includes
// DataStream/Zenith_DataStream.h directly (canvas serialization); several
// text-bearing elements (button/text/toggle/overlay/layout group) #include
// Flux/Text/Flux_TextImpl.h + Flux/Text/Flux_TextQueue.h directly, and
// Zenith_UIStyleRenderer.cpp #includes Flux/Flux_Buffers.h +
// Flux/Quads/Flux_QuadsImpl.h directly (the shared styled-rect renderer used
// by rect/button/overlay/scrollview/toggle); Zenith_UIImage.cpp #includes
// AssetHandling/Zenith_AssetHandle.h + Zenith_TextureAsset.h directly (sprite
// texture loading) and Zenith_UIText.cpp/Zenith_UICanvas.cpp #include
// AssetHandling/Zenith_FontAsset.h directly; the B9 virtual controls
// (Zenith_UIVirtualStick.cpp, Zenith_UIVirtualButton.cpp) and the focus/input
// walk (Zenith_UICanvas.cpp) #include Input/Zenith_InputActions.h +
// Zenith_InputSimulator.h + Zenith_Pointers.h + Zenith_Input.h directly; and
// Zenith_UICanvas.cpp #includes Profiling/Zenith_Profiling.h directly (the
// render/input phase profiling scopes). So this lib depends on ZenithCore
// (mandatory) + ZenithMaths + ZenithDataStream + ZenithAssetHandling +
// ZenithFlux + ZenithInput + ZenithProfiling. No ZenithECS/ZenithScripting
// edge: nothing under Zenith/UI #includes either (verified — grep for "ECS"
// or "Scripting/" across the directory finds none); the ECS-facing glue
// (Zenith_UIComponent, Zenith_UISystem) is engine-side, in EntityComponent.
//
// imgui.h — INCLUDE PATH ONLY, NOT A LINK EDGE: Zenith_UI.h transitively
// pulls it through the shared Zenith.h PCH chain and no .cpp here references
// an ImGui:: symbol out of line beyond what the PCH already provides, so
// (exactly as Sharpmake_ZenithAssetHandling.cs / Sharpmake_ZenithFlux.cs
// document for their own imgui.h reach) this project adds the
// Middleware/imgui-docking include path but does NOT
// AddPublicDependency<ZenithImGuiLibProject> — ZenithImGui carries no
// dependency edges at all (see its own header comment) and the symbols
// resolve at the final exe link where zenithimgui.lib is always present via
// its ZenithEngineLibs.All row.
//
// SHADER_SOURCE_ROOT — NEEDED, AND THE BRIEF'S OWN GREP-FOR-THE-TOKEN METHOD
// UNDERSTATES IT: nothing under Zenith/UI #includes Flux/Flux_GraphicsImpl.h
// or references SHADER_SOURCE_ROOT literally, but the shared Zenith.h PCH
// this project /Yu's against still transitively reaches the active backend's
// Pipeline header (Zenith_Null_Pipeline.h on a Null_ config), which
// references SHADER_SOURCE_ROOT unguarded — confirmed the hard way (C2065
// 'SHADER_SOURCE_ROOT': undeclared identifier, reported against every
// Zenith_UI*.cpp TU) on the Null_ build in this phase's verification step 5.
// Same defect class Sharpmake_ZenithAssetHandling.cs / Sharpmake_ZenithFlux.cs
// / Sharpmake_ZenithDebugVariables.cs already document; this project simply
// reaches it through the PCH chain rather than a direct #include. No
// ENGINE_ASSETS_DIR: unguarded only in Zenith/Core/Zenith_CommandLine.h
// (already covered by the mandatory ZenithCore edge) and
// Zenith/Flux/Particles/Flux_Particles.cpp (not reached from here) — grep
// finds no other unguarded site, and the Null_/D3D12_ builds in this phase's
// verification steps 5/6 confirm no second define is needed.
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithUI".
[Sharpmake.Generate]
public class ZenithUILibProject : ZenithBaseProject
{
	// Leaf static lib — renderer-agnostic in the same sense
	// Sharpmake_ZenithAssetHandling.cs documents (no direct vkXxx/glfw calls
	// anywhere under Zenith/UI); must not link or propagate
	// glfw/vulkan/slang, so ConfigureAll below calls none of the
	// AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs helpers.
	public ZenithUILibProject()
	{
		Name = "ZenithUI";

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

		SourceRootPath = RootPath + "/Zenith/UI";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithUI\[target.Platform]\[conf.Name]";

		// Per-lib PCH: ZenithUI CREATEs Zenith.pch from Internal/Zenith_UIPCH.cpp.
		conf.PrecompHeader = "Zenith.h";
		conf.PrecompSource = "Zenith_UIPCH.cpp";

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf lib must not link or propagate the renderer backend.

		// The PCH (Zenith.h) transitively pulls imgui + Flux/Vulkan/vma/Jolt
		// headers, and this project's own TUs #include imgui.h + Flux/**
		// headers directly (see the file header comment) — include paths
		// only, no libs linked.
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		// SHADER_SOURCE_ROOT — see the file header comment. Mirrors
		// ZenithProject's (Sharpmake_Zenith.cs) identical win64/agde branch
		// exactly (same value, same "" on agde).
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

		// MANDATORY: ZenithCore — see Sharpmake_ZenithCore.cs's header comment.
		conf.AddPublicDependency<ZenithCoreLibProject>(target);

		// Direct edges — see the file header comment's DEPENDENCY paragraph.
		conf.AddPublicDependency<ZenithMathsLibProject>(target);
		conf.AddPublicDependency<ZenithDataStreamLibProject>(target);
		conf.AddPublicDependency<ZenithAssetHandlingLibProject>(target);
		conf.AddPublicDependency<ZenithFluxLibProject>(target);
		conf.AddPublicDependency<ZenithInputLibProject>(target);
		conf.AddPublicDependency<ZenithProfilingLibProject>(target);
	}
}
