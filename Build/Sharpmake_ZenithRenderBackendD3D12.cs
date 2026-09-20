using Sharpmake;
using System;
using System.IO;

// ZenithRenderBackendD3D12 — leaf static library (Phase 4e — see
// Sharpmake_ZenithRenderBackendVulkan.cs for the fuller pattern rationale;
// this file and Sharpmake_ZenithRenderBackendNull.cs are its two win64-only
// siblings). Holds exactly Zenith/RenderBackends/D3D12 — 3 translation units
// (Zenith_D3D12.cpp, Zenith_D3D12_ImGuiIntegration.cpp,
// Zenith_D3D12_MemoryManager.cpp) + the definition-free PCH create TU
// (Internal/Zenith_RenderBackendD3D12PCH.cpp). 3 + 1 = 4 compiled TUs —
// exactly at the >= 4 threshold, so this library DOES get its own PCH (see
// the ≥4-TU rule in every other leaf's header comment). Previously globbed
// straight into the monolithic aggregate ZenithProject, with the OTHER two
// backends' trees (Vulkan/Null) subtracted per-configuration via
// ConfigurePlatformExcludes' render-backend block (deleted this phase).
//
// WIN64 ONLY, RenderBackend.D3D12 ONLY — no agde AddTargets call at all
// (D3D12 has no Android meaning; agde is always Vulkan). Every call site that
// adds this project (via ZenithEngineLibs.All) must guard with a predicate:
// `t => t.RenderBackend == RenderBackend.D3D12 && t.Platform == Platform.win64`
// — `conf.AddProject`/`conf.AddPublicDependency` THROW at Sharpmake-run time
// for a target this project itself never declared.
//
// NO OWNED IMGUI BACKEND TU — unlike Vulkan, this backend performs zero real
// rendering (see Zenith/RenderBackends/D3D12/CLAUDE.md: "no <d3d12.h>, no
// device, no swapchain") and has no `imgui_impl_dx12.cpp` counterpart in this
// codebase's vendored imgui tree to own; Zenith_D3D12_ImGuiIntegration.cpp is
// this library's OWN native Zenith code (editor texture registration stubs),
// not a vendored backend TU, so it needs no AdditionalSourceRootPaths.
//
// THE Flux <-> RenderBackends CYCLE — DECLARED backend -> Flux, same
// direction and same reasoning as ZenithRenderBackendVulkan (see that file's
// header comment's CYCLE paragraph in full). This backend #includes 9+
// distinct Flux/** headers directly (Flux_Buffers.h, Flux_Fwd.h,
// Flux_ImGuiIntegration.h, Flux_RecordValidation.h, Flux_RendererImpl.h,
// Flux_Types.h, Flux_VertexLayoutValidation.h, Flux_WorkDistribution.h,
// Backend/Flux_IndirectDraw.h, RenderGraph/Flux_RenderGraph.h, and the
// Slang/Flux_ShaderCatalog.h / Flux_ShaderDecl.h / Flux_SlangCompiler.h trio
// for its reflection-only shader load — see
// Zenith/RenderBackends/D3D12/CLAUDE.md's "Shaders load reflection only").
// ZenithFlux does not depend back — its 3 reverse includes resolve purely by
// include path (RootPath + "/Zenith/RenderBackends", added by ZenithFlux's
// own ConfigureAll).
//
// This direction relies on ZENITH_D3D12 coming from ConfigureCommonSettings
// (Sharpmake_Common.cs) rather than from the Sharpmake dependency edge —
// unchanged by this phase, still the sole definer, one per config, with a
// `throw` on an unhandled RenderBackend fragment value.
//
// DEPENDENCY: no direct #include of DebugVariables/TaskSystem/ECS/
// AssetHandling anywhere under this tree (grep confirms — unlike the Vulkan
// backend, which directly uses both). So this lib depends on ZenithCore
// (mandatory) + ZenithFlux (the sole direct edge — every Flux/** header this
// library #includes). ZenithMaths/ZenithDataStream/ZenithFileAccess/
// ZenithProfiling/ZenithDebugVariables/ZenithTaskSystem/ZenithECS/
// ZenithAssetHandling all arrive transitively through ZenithFlux's own
// PUBLIC dependency chain — this library's Zenith_D3D12_Pipeline.h directly
// #includes DataStream/Zenith_DataStream.h (for the reflection-load type),
// but that is satisfied by the same transitive edge, not a separate
// declaration (matches how ZenithRenderBackendVulkan does not redeclare
// edges ZenithFlux already re-exports).
//
// LIBRARY: this library calls AddD3D12Libs (d3d12.lib, dxgi.lib, dxguid.lib —
// Windows SDK import libs, no LibraryPaths entry needed, see
// Sharpmake_Common.cs). Does NOT call AddVulkanBackendLibs/AddSlangLibs (this
// is not the Vulkan backend) and does NOT call AddGlfwLibs — this backend
// calls ImGui_ImplGlfw_InitForOther(...) and reads
// Zenith_Window::GetInstance() directly (Zenith_D3D12.cpp), exactly the
// undeclared-symbol acceptance ZenithRenderBackendVulkan's header comment
// documents for the identical pattern (see that file's "GLFW" paragraph) —
// the composition root every actual exe links always carries ZenithWindows
// on win64 regardless.
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithRenderBackendD3D12".
[Sharpmake.Generate]
public class ZenithRenderBackendD3D12LibProject : ZenithBaseProject
{
	public ZenithRenderBackendD3D12LibProject()
	{
		Name = "ZenithRenderBackendD3D12";

		// WIN64 ONLY, RenderBackend.D3D12 ONLY — see the file header comment.
		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False,
			RenderBackend = RenderBackend.D3D12
		});

		SourceRootPath = RootPath + "/Zenith/RenderBackends/D3D12";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithRenderBackendD3D12\[target.Platform]\[conf.Name]";

		// Per-lib PCH: CREATEs Zenith.pch from
		// Internal/Zenith_RenderBackendD3D12PCH.cpp. Exactly at the >= 4-TU
		// threshold (3 real TUs + this create TU) — see the file header
		// comment.
		conf.PrecompHeader = "Zenith.h";
		conf.PrecompSource = "Zenith_RenderBackendD3D12PCH.cpp";

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);

		// This library IS the D3D12 system import libs' owner. No
		// AddVulkanBackendLibs/AddSlangLibs/AddGlfwLibs call — see the file
		// header comment's "LIBRARY" paragraph.
		AddD3D12Libs(conf, target);

		// The PCH (Zenith.h) transitively pulls imgui + Flux/Vulkan/vma/Jolt
		// headers, and this library's own headers (Zenith_D3D12.h,
		// Zenith_D3D12_ImGuiIntegration.cpp) reference imgui.h /
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

		// SHADER_SOURCE_ROOT — Zenith_D3D12_Pipeline.h references it
		// directly, unguarded. Win64-only project, so a single unconditional
		// define (no agde branch — this project declares no agde target at
		// all). Same value as every other backend's win64 branch.
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
