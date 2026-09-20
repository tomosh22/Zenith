using Sharpmake;
using System;
using System.IO;

// ZenithRenderBackendNull — leaf static library (Phase 4e — see
// Sharpmake_ZenithRenderBackendVulkan.cs for the fuller pattern rationale and
// Sharpmake_ZenithRenderBackendD3D12.cs for its closest sibling — the two are
// deliberate twins, see Zenith/RenderBackends/Null/CLAUDE.md). Holds exactly
// Zenith/RenderBackends/Null — 3 translation units (Zenith_Null.cpp,
// Zenith_Null_ImGuiIntegration.cpp, Zenith_Null_MemoryManager.cpp) + the
// definition-free PCH create TU (Internal/Zenith_RenderBackendNullPCH.cpp).
// 3 + 1 = 4 compiled TUs — exactly at the >= 4 threshold, so this library
// DOES get its own PCH. Previously globbed straight into the monolithic
// aggregate ZenithProject, with the OTHER two backends' trees (Vulkan/D3D12)
// subtracted per-configuration via ConfigurePlatformExcludes' render-backend
// block (deleted this phase).
//
// WIN64 ONLY, RenderBackend.Null ONLY — no agde AddTargets call at all
// (headless/CI is a win64 concept in this engine; agde is always Vulkan).
// Every call site that adds this project (via ZenithEngineLibs.All) must
// guard with a predicate:
// `t => t.RenderBackend == RenderBackend.Null && t.Platform == Platform.win64`
// — `conf.AddProject`/`conf.AddPublicDependency` THROW at Sharpmake-run time
// for a target this project itself never declared. THIS is the library whose
// absence from a `Null_*` solution is what step 5 of this phase's
// verification (`zenith build Combat --headless`) proves: a config with no
// Vulkan project in it at all still links, because it has THIS project
// instead — no source-level subtraction anywhere.
//
// NO OWNED IMGUI BACKEND TU — same reasoning as ZenithRenderBackendD3D12:
// zero real rendering, so no vendored `imgui_impl_null.cpp` exists to own.
// Zenith_Null_ImGuiIntegration.cpp is this library's own native Zenith code.
//
// ★ STRUCTURAL PURITY, THE WHOLE POINT OF THIS LIBRARY: it calls NONE of
// AddVulkanBackendLibs / AddSlangLibs / AddD3D12Libs / AddGlfwLibs. A
// GPU-less backend that CANNOT acquire a graphics import library by
// accident — the same guarantee Sharpmake_Common.cs's header comment already
// promises for every leaf that simply never calls one of these helpers (a
// project that never calls AddVulkanBackendLibs cannot acquire vulkan-1.lib
// by accident). glfw3_mt.lib is NOT linked here either, even though this
// backend calls ImGui_ImplGlfw_InitForOther(...) and reads
// Zenith_Window::GetInstance() directly (Zenith_Null.cpp) — the same
// undeclared-symbol acceptance ZenithRenderBackendVulkan's header comment
// documents (see that file's "GLFW" paragraph): the composition root every
// actual exe links always carries ZenithWindows on win64 regardless of what
// this leaf itself declares.
//
// THE Flux <-> RenderBackends CYCLE — DECLARED backend -> Flux, same
// direction and reasoning as the other two backends (see
// Sharpmake_ZenithRenderBackendVulkan.cs's header comment's CYCLE paragraph
// in full). This backend #includes the same Flux/** header set as
// ZenithRenderBackendD3D12 (its deliberate twin — Flux_Buffers.h, Flux_Fwd.h,
// Flux_ImGuiIntegration.h, Flux_RecordValidation.h, Flux_RendererImpl.h,
// Flux_Types.h, Flux_VertexLayoutValidation.h, Flux_WorkDistribution.h,
// Backend/Flux_IndirectDraw.h, RenderGraph/Flux_RenderGraph.h, and the
// Slang/Flux_ShaderCatalog.h / Flux_ShaderDecl.h / Flux_SlangCompiler.h trio
// for its own reflection-only shader load). ZenithFlux does not depend
// back — its 3 reverse includes resolve purely by include path.
//
// This direction relies on ZENITH_NULL_RENDERER coming from
// ConfigureCommonSettings (Sharpmake_Common.cs) rather than from the
// Sharpmake dependency edge — unchanged by this phase, still the sole
// definer, one per config, with a `throw` on an unhandled RenderBackend
// fragment value.
//
// DEPENDENCY: no direct #include of DebugVariables/TaskSystem/ECS/
// AssetHandling anywhere under this tree. So this lib depends on ZenithCore
// (mandatory) + ZenithFlux (the sole direct edge). Zenith_Null_Pipeline.h
// directly #includes DataStream/Zenith_DataStream.h (reflection-load type),
// satisfied transitively through ZenithFlux's own public dependency chain —
// same as ZenithRenderBackendD3D12.
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithRenderBackendNull".
[Sharpmake.Generate]
public class ZenithRenderBackendNullLibProject : ZenithBaseProject
{
	public ZenithRenderBackendNullLibProject()
	{
		Name = "ZenithRenderBackendNull";

		// WIN64 ONLY, RenderBackend.Null ONLY — see the file header comment.
		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False,
			RenderBackend = RenderBackend.Null
		});

		SourceRootPath = RootPath + "/Zenith/RenderBackends/Null";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithRenderBackendNull\[target.Platform]\[conf.Name]";

		// Per-lib PCH: CREATEs Zenith.pch from
		// Internal/Zenith_RenderBackendNullPCH.cpp. Exactly at the >= 4-TU
		// threshold (3 real TUs + this create TU) — see the file header
		// comment.
		conf.PrecompHeader = "Zenith.h";
		conf.PrecompSource = "Zenith_RenderBackendNullPCH.cpp";

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);

		// Deliberately calls NONE of AddVulkanBackendLibs / AddSlangLibs /
		// AddD3D12Libs / AddGlfwLibs — see the file header comment's
		// "STRUCTURAL PURITY" paragraph.

		// The PCH (Zenith.h) transitively pulls imgui + Flux/Vulkan/vma/Jolt
		// headers, and this library's own headers (Zenith_Null.h,
		// Zenith_Null_ImGuiIntegration.cpp) reference imgui.h /
		// backends/imgui_impl_glfw.h directly.
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		conf.Output = Configuration.OutputType.Lib;

		// MANDATORY: ZenithCore — see Sharpmake_ZenithCore.cs's header comment.
		conf.AddPublicDependency<ZenithCoreLibProject>(target);

		// backend -> Flux: the sole direct edge — see the file header
		// comment's DEPENDENCY paragraph. ZenithFlux does NOT depend back —
		// see the CYCLE paragraph.
		conf.AddPublicDependency<ZenithFluxLibProject>(target);

		// SHADER_SOURCE_ROOT — Zenith_Null_Pipeline.h references it directly,
		// unguarded. Win64-only project, so a single unconditional define (no
		// agde branch — this project declares no agde target at all). Same
		// value as every other backend's win64 branch.
		string zenithRoot = Path.GetFullPath(Path.Combine(SharpmakeCsPath, "..")).Replace('\\', '/');
		string shaderSourceRoot = zenithRoot + "/Zenith/Flux/Shaders/";
		conf.Defines.Add($"SHADER_SOURCE_ROOT=\"{shaderSourceRoot}\"");

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
