using Sharpmake;
using System;
using System.IO;

// ZenithRenderBackendVulkan — leaf static library (Phase 4e — see
// Sharpmake_ZenithCore.cs for the pattern this file replicates, and
// Sharpmake_ZenithWindows.cs for the win64-only-target pattern this file
// extends to a win64+agde TARGET-RESTRICTED library). Holds
// Zenith/RenderBackends/Vulkan MINUS its two nested Platform/ subdirectories
// (Phase 4h.3 split those out into their own leaf libraries — see
// Sharpmake_ZenithRenderBackendVulkanPlatformWindows.cs / ...Android.cs) —
// 15 top-level Zenith_Vulkan*.cpp + the one ImGui backend TU this library
// still OWNS (imgui_impl_vulkan.cpp — see "THE IMGUI BACKEND TU" below) +
// the definition-free PCH create TU (Internal/Zenith_RenderBackendVulkanPCH.cpp).
// 15 + 1 + 1 = 17 compiled TUs (down from 19 before the Phase 4h.3 Platform
// split moved 2 TUs to the two new single-file libraries).
// Previously globbed straight into the monolithic aggregate ZenithProject,
// with the OTHER two backends' trees (D3D12/Null) subtracted per-configuration
// via ConfigurePlatformExcludes' render-backend block.
//
// ★ THE HEADLINE OF THIS PHASE: Zenith/RenderBackends/ becomes THREE
// libraries, not one. A single library rooted at /Zenith/RenderBackends would
// glob all three backends and then need a build-exclude for the two inactive
// ones on every config — reintroducing exactly the per-configuration
// subtraction mechanism this whole refactor exists to remove. Instead, each
// backend is its own library that declares ONLY the RenderBackend fragment(s)
// it supports; conditionality becomes TARGET DECLARATION + SOLUTION
// MEMBERSHIP (via Build/Sharpmake_EngineLibs.cs's `Func<ZenithTarget,bool>`
// predicate table) rather than a source-file exclude. A `Null_*` or `D3D12_*`
// configuration simply never has this project in its solution at all — see
// Build/Sharpmake_Common.cs's ConfigurePlatformExcludes for exactly what got
// deleted as a result (the whole render-backend
// SourceFilesBuildExcludeRegex block, ~lines 130-156 before this phase).
//
// TARGET RESTRICTION — RenderBackend.Vulkan ONLY, BOTH PLATFORMS: unlike
// ZenithWindows (win64-only) or ZenithAndroid (agde-only), this library spans
// BOTH win64 and agde, because agde is always Vulkan (every agde target the
// engine declares carries RenderBackend.Vulkan — see ZenithFluxLibProject's
// own AddTargets for the identical shape). It declares NO D3D12/Null win64
// targets at all. `conf.AddProject`/`conf.AddPublicDependency` THROW at
// Sharpmake-run time if the target being configured is not one the callee
// itself declares, so every call site that adds this project (the three via
// ZenithEngineLibs.All) must guard with a predicate rather than adding it
// unconditionally — `t => t.RenderBackend == RenderBackend.Vulkan` (true for
// BOTH win64 Vulkan configs and every agde config, since agde targets always
// carry RenderBackend.Vulkan).
//
// THE PCH: >= 4 compiled TUs justifies a binary PCH under this refactor's
// rule (19 clears it easily). ZenithRenderBackendVulkan CREATEs its own
// Zenith.pch from Internal/Zenith_RenderBackendVulkanPCH.cpp (a one-line
// `#include "Zenith.h"`, nothing else — mirrors every other per-lib PCH
// create TU in this refactor; must stay definition-free, see that file's own
// header comment for why). The 18 real TUs /Yu against it.
//
// THE IMGUI BACKEND TU: Middleware/imgui-docking/backends/imgui_impl_vulkan.cpp
// becomes a member of ZenithRenderBackendVulkan — the library that owns that
// graphics API — instead of being globbed by the aggregate and then
// per-configuration build-excluded on the other two backends (the
// `.*imgui_impl_vulkan.*` lines Sharpmake_Common.cs's ConfigurePlatformExcludes
// carried in its D3D12/Null arms; both deleted this phase). It is added via a
// SECOND AdditionalSourceRootPaths root (the backends directory) with an
// exclude list that keeps only that one file — the same pattern
// Sharpmake_ZenithWindows.cs already uses for its glfw+win32 pair, and
// Sharpmake_ZenithHub.cs / Sharpmake_TilePuzzleRegistryViewer.cs use for their
// own dx11 backend TU. ZenithImGui already excludes the whole backends/
// subtree from its own membership (Sharpmake_ZenithImGui.cs), so no change is
// needed there — the two projects' ownership was already a clean complement.
// imgui_impl_glfw.cpp/imgui_impl_win32.cpp stay ZenithWindows's (Phase 4b);
// imgui_impl_android.cpp is compiled by nothing (unaffected).
//
// THE Flux <-> RenderBackends CYCLE — DECLARED backend -> Flux: the backend
// IMPLEMENTS the Flux concepts (~41 unique header edges that direction across
// the three backends combined — Zenith_Vulkan_*.cpp/.h alone #include 25+
// distinct Flux/** headers, listed below); Flux's only reverse edges are the
// 3 concrete-type-alias includes in Flux/Flux_BackendTypes.h (which pulls
// Zenith_PlatformGraphics_Include.h from HERE when ZENITH_VULKAN is defined) —
// those resolve purely by INCLUDE PATH (RootPath + "/Zenith/RenderBackends"
// added by ZenithFlux's own ConfigureAll), not by a Sharpmake dependency edge,
// so there is no cycle at the PROJECT level: ZenithRenderBackendVulkan depends
// on ZenithFlux, and ZenithFlux does not depend back. This mirrors exactly how
// Phase 4d declared the ZenithFlux <-> ZenithAssetHandling cycle one-way (see
// Sharpmake_ZenithFlux.cs's own header comment) — the same reasoning applies
// here: a static lib never resolves an external symbol at its own build step,
// only the final exe link does, and every consumer of this table always links
// every engine lib together.
//
// This direction relies on ZENITH_VULKAN coming from ConfigureCommonSettings
// (Sharpmake_Common.cs — the explicit three-way `if`/`else if`/`else throw`
// keyed on target.RenderBackend) rather than from the Sharpmake dependency
// edge. Verified still true: ConfigureCommonSettings is unchanged by this
// phase and remains the sole definer of ZENITH_VULKAN / ZENITH_D3D12 /
// ZENITH_NULL_RENDERER, one per config, with a `throw` on an unhandled
// fragment value.
//
// DEPENDENCY (direct #include edges beyond Flux, same "declare what is
// directly used" convention every Phase 4b/4c/4d leaf follows — grep across
// every Zenith_Vulkan*.{h,cpp} confirms these): Zenith_Vulkan.cpp,
// Zenith_Vulkan_MemoryManager*.cpp and Zenith_Vulkan_Swapchain.cpp #include
// DebugVariables/Zenith_DebugVariables.h directly; Zenith_Vulkan.cpp
// #includes TaskSystem/Zenith_TaskSystem.h directly. So this lib depends on
// ZenithCore (mandatory) + ZenithFlux (the primary edge — Vulkan.h/.cpp,
// MemoryManager, CommandBuffer, Pipeline, Swapchain and friends #include
// Flux/Flux.h, Flux/Backend/Flux_IndirectDraw.h, Flux/Flux_BindingValidation.h,
// Flux/Flux_Buffers.h, Flux/Flux_GraphicsImpl.h, Flux/Flux_ImGuiIntegration.h,
// Flux/Flux_PerFrame.h, Flux/Flux_PersistentSetLayouts.h,
// Flux/Flux_RecordValidation.h, Flux/Flux_RenderTargets.h,
// Flux/Flux_RendererImpl.h, Flux/Flux_Screenshot.h, Flux/Flux_SwapchainPolicy.h,
// Flux/Flux_Types.h, Flux/Flux_VertexLayoutValidation.h,
// Flux/Flux_ViewSetBinding.h, Flux/Present/Flux_PresentImpl.h,
// Flux/RenderGraph/Flux_RenderGraph.h, Flux/RenderViews/Flux_RenderViews.h,
// and the whole Flux/Slang/ shader-catalog/decl/hot-reload/compiler/spirv-usage
// set directly) + ZenithDebugVariables (direct #include) + ZenithTaskSystem
// (direct #include). ZenithMaths/ZenithDataStream/ZenithFileAccess/
// ZenithProfiling/ZenithECS/ZenithAssetHandling all arrive transitively
// through ZenithFlux's own PUBLIC dependency chain (Sharpmake_ZenithFlux.cs) —
// no separate edge declared for those, matching how ZenithFlux itself does
// not redeclare edges ZenithAssetHandling already re-exports.
//
// Collections (Zenith_Vector/Zenith_HashMap, used directly in
// Zenith_Vulkan.cpp/_CommandBuffer.cpp/_MemoryManager.cpp/_ImGuiIntegration.cpp)
// is header-only (zero .cpp of its own) and reached via the plain "/Zenith"
// include path every leaf already carries through ConfigureCommonIncludePaths
// — no separate library or dependency edge exists for it (see
// Sharpmake_ZenithCore.cs).
//
// GLFW — UNDECLARED SYMBOLS, ACCEPTED, SAME PRECEDENT AS ZenithInput: this
// library calls glfwXxx(...) directly in 3 files (Zenith_Vulkan_Platform.h,
// Zenith_Vulkan_Swapchain.cpp, Platform/Windows/Zenith_Windows_Vulkan.cpp) and
// reads Zenith_Window::GetInstance() (Zenith/Windows/Zenith_Windows_Window.h,
// owned by ZenithWindows) in Zenith_Vulkan.cpp/_Swapchain.cpp — but does NOT
// call AddGlfwLibs and does NOT declare a ZenithWindows dependency, exactly
// the choice Sharpmake_ZenithInput.cs already made for its own direct
// glfwJoystickIsGamepad(...) call: a static lib never resolves an external
// symbol at its own link step, and the composition root every actual exe
// links (the aggregate, via ZenithEngineLibs.AddAllToSolution /
// AddAllAsDependenciesOfAggregate) always carries ZenithWindows too, on win64.
// Sharpmake_ZenithWindows.cs's own header comment names this exact library as
// the future case that would need ZenithWindows added to its own link set if
// it ever became a standalone Sentinel exe target — not done this phase, no
// such Sentinel exists for a render backend.
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithRenderBackendVulkan".
[Sharpmake.Generate]
public class ZenithRenderBackendVulkanLibProject : ZenithBaseProject
{
	public ZenithRenderBackendVulkanLibProject()
	{
		Name = "ZenithRenderBackendVulkan";

		// RenderBackend.Vulkan ONLY — see the file header comment ("TARGET
		// RESTRICTION"). win64 spans every ToolsEnabled/Optimization combo;
		// agde is the full ABI set, ToolsEnabled.False only (matches every
		// other agde-capable leaf lib in this refactor).
		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan
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

		SourceRootPath = RootPath + "/Zenith/RenderBackends/Vulkan";

		// The one owned ImGui backend TU — see "THE IMGUI BACKEND TU" above.
		// Whole-directory glob + exclude-everything-else-including-the-two-
		// ZenithWindows-owns, same pattern Sharpmake_ZenithWindows.cs already
		// uses for its glfw+win32 pair.
		AdditionalSourceRootPaths.Add(RootPath + "/Middleware/imgui-docking/backends");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_allegro.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_android.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_dx.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_glfw.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_glut.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_opengl.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_sdl.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_wgpu.*");
		SourceFilesExcludeRegex.Add(@".*imgui_impl_win32.*");
		// imgui_impl_metal.mm / imgui_impl_osx.mm never match
		// SourceFilesExtensions (.cpp/.c/.h/.inl/.slang has no .mm) — no
		// exclude needed for those two.
		// Phase 4h.3 -- Zenith/RenderBackends/Vulkan/Platform/{Windows,Android}
		// split OUT into their own single-file, single-platform leaf libraries
		// (ZenithRenderBackendVulkanPlatformWindows / ...PlatformAndroid --
		// Sharpmake_ZenithRenderBackendVulkanPlatformWindows.cs / ...Android.cs).
		// This project's own recursive SourceRootPath (RenderBackends/Vulkan)
		// still SEES both nested Platform/ subdirectories, so they are
		// constructor MEMBERSHIP-excluded here, the same complement pattern
		// every other leaf split in this refactor uses. This was the ONE
		// genuinely multi-platform tree the Phase 4h measurement found:
		// Platform/Windows/Zenith_Windows_Vulkan.cpp and
		// Platform/Android/Zenith_Android_Vulkan.cpp were the sole reason
		// ConfigurePlatformExcludes' win64 \Android\ arm and agde \Windows\
		// arm were load-bearing anywhere in the whole codebase -- see those
		// two new files' own header comments for the split's dependency
		// reasoning, and Sharpmake_Common.cs for ConfigurePlatformExcludes'
		// removal now that every call site (including this one, below) is
		// dead.
		SourceFilesExcludeRegex.Add(@".*\\RenderBackends\\Vulkan\\Platform\\Windows\\.*");
		SourceFilesExcludeRegex.Add(@".*\\RenderBackends\\Vulkan\\Platform\\Android\\.*");
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED. Now that
		// Platform/Windows and Platform/Android are constructor MEMBERSHIP
		// excludes (see the constructor above), this project's own generated
		// .vcxproj carries zero members matching \Android\, _Android,
		// \Windows\ or _Windows. See Sharpmake_Common.cs for the method's
		// removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithRenderBackendVulkan\[target.Platform]\[conf.Name]";

		// Per-lib PCH: CREATEs Zenith.pch from
		// Internal/Zenith_RenderBackendVulkanPCH.cpp.
		conf.PrecompHeader = "Zenith.h";
		conf.PrecompSource = "Zenith_RenderBackendVulkanPCH.cpp";
		// imgui_impl_vulkan.cpp does not begin with `#include "Zenith.h"`, so
		// it must compile WITHOUT the engine PCH (mirrors how
		// Sharpmake_ZenithWindows.cs handles its own two ImGui backend TUs).
		conf.PrecompSourceExcludeFolders.Add(RootPath + "/Middleware/imgui-docking");

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);

		// This library IS the Vulkan loader import lib's owner. Guarded
		// win64-only, as the helper itself already does — agde links its
		// system Vulkan loader via --sysroot (see Sharpmake_Games.cs), never
		// this lib path. No AddSlangLibs (ZenithFlux owns slang.lib) and no
		// AddGlfwLibs (see the file header comment's "GLFW" paragraph).
		AddVulkanBackendLibs(conf, target);

		// The PCH (Zenith.h) transitively pulls imgui + Flux/Vulkan/vma/Jolt
		// headers, and this library's own headers (Zenith_Vulkan_Platform.h,
		// Zenith_PlatformGraphics_Include.h) reference vulkan.hpp/vk_mem_alloc.h
		// directly, plus imgui_impl_vulkan.cpp/.h and this library's own
		// Zenith_Vulkan_ImGuiIntegration.cpp reference imgui.h directly.
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		conf.Output = Configuration.OutputType.Lib;

		// MANDATORY: ZenithCore — see Sharpmake_ZenithCore.cs's header comment.
		conf.AddPublicDependency<ZenithCoreLibProject>(target);

		// Direct edges — see the file header comment's DEPENDENCY paragraph.
		conf.AddPublicDependency<ZenithDebugVariablesLibProject>(target);
		conf.AddPublicDependency<ZenithTaskSystemLibProject>(target);

		// backend -> Flux: the primary, heaviest edge. ZenithFlux does NOT
		// depend back — see the file header comment's CYCLE paragraph.
		conf.AddPublicDependency<ZenithFluxLibProject>(target);

		// SHADER_SOURCE_ROOT — Zenith_Vulkan_Shader.cpp references it
		// directly, unguarded (found the same way Sharpmake_ZenithFlux.cs /
		// Sharpmake_ZenithAssetHandling.cs / Sharpmake_ZenithDebugVariables.cs
		// each already did for their own directly-referencing TUs). No
		// ENGINE_ASSETS_DIR reference exists anywhere under this tree, so
		// unlike ZenithFlux this library does not need that second define.
		// Mirrors ZenithFlux's identical win64/agde branch exactly (same
		// value, same "" on agde).
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
