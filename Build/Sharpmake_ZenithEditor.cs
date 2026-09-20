using Sharpmake;
using System;
using System.IO;

// ZenithEditor — leaf static library (Phase 4g — see Sharpmake_ZenithCore.cs
// for the pattern this file replicates, and Sharpmake_ZenithEntityComponent.cs
// for the closest-shaped sibling: both are Tools-only leaves that reach deep
// into the header DAG). Holds exactly Zenith/Editor — 56 translation units
// across its three subdirectories (Animation/: 3, Panels/: 19, TerrainEditor/:
// 7) + 26 top-level Zenith_*.cpp (including Zenith_EditorTestFixture.cpp,
// moved into this directory in an earlier phase) + the definition-free PCH
// create TU (Internal/Zenith_EditorPCH.cpp). 56 + 1 = 57 compiled TUs.
// Previously globbed straight into the monolithic aggregate ZenithProject and
// per-configuration build-excluded everywhere it was NOT wanted (agde, and —
// see "TOOLS=FALSE NEEDS NO EXCLUDE AT ALL" below — nowhere else, because
// every .cpp here is self-gating).
//
// ═══════════════════════════════════════════════════════════════════════
// WIN64 x TOOLS=TRUE ONLY — THE HEADLINE OF THIS PHASE.
// ═══════════════════════════════════════════════════════════════════════
// This project declares exactly ONE target set: Platform.win64 x
// ToolsEnabled.True (all three RenderBackend fragments, both Optimizations).
// No agde AddTargets call, and no ToolsEnabled.False fragment either — unlike
// every win64-only leaf before it (ZenithWindows), this is the first leaf in
// the table to restrict on BOTH axes at once. `conf.AddProject` /
// `conf.AddPublicDependency` THROW at Sharpmake-run time for a target the
// callee itself never declares, so every call site that adds this project
// (via ZenithEngineLibs.All) must guard with a predicate matching this target
// set exactly: `t => t.Platform == Platform.win64 && t.ToolsEnabled ==
// ToolsEnabled.True`. A `_False` configuration or an agde configuration
// therefore has NO ZenithEditor project in its solution at all — not a
// project that is present but empty, an absent project. That absence is what
// retires the four per-configuration Editor/Tools build-excludes this phase
// deletes from ConfigurePlatformExcludes (Sharpmake_Common.cs) and
// Sharpmake_Zenith.cs's ConfigureAll (see those files' own comments at the
// deletion sites): a `_False`/agde config no longer needs to be told "build
// this project's source but skip the Editor directory", because the Editor
// directory was never a member of that config's project set to begin with.
//
// TOOLS=FALSE NEEDS NO EXCLUDE AT ALL — VERIFIED, NOT ASSUMED: all 56 of this
// directory's .cpp files are independently wrapped start-to-end in
// `#ifdef ZENITH_TOOLS` / `#endif` (verified by script: every file's first
// non-blank line is the `#ifdef` and its last non-blank line is the matching
// `#endif`). The brief for this phase cited 55; the measured count is 56 —
// every file in the directory, including Zenith_EditorTestFixture.cpp. This
// means a `_False` configuration was ALREADY emitting empty translation units
// for the whole directory before this phase — this library does not add a
// new restriction, it makes an existing content-level one structural (no
// project membership at all, instead of a project that compiles 56 TUs down
// to nothing).
//
// THE PCH: >= 4 compiled TUs justifies a binary PCH under this refactor's
// rule. ZenithEditor CREATEs its own Zenith.pch from
// Internal/Zenith_EditorPCH.cpp (a one-line `#include "Zenith.h"`, nothing
// else — mirrors Zenith/Flux/Internal/Zenith_FluxPCH.cpp; it MUST stay
// definition-free, see that file's own header comment for why). All 56 real
// TUs /Yu against it, even though each one's actual content is gated behind
// `#ifdef ZENITH_TOOLS` — ZENITH_TOOLS is unconditionally defined below
// (this project exists only when ToolsEnabled.True), so the gate is always
// open for every configuration this project actually has.
//
// ═══════════════════════════════════════════════════════════════════════
// SEAM — EntityComponent <-> Editor: REAL, and DELIBERATELY UNDECLARED.
// ═══════════════════════════════════════════════════════════════════════
// Zenith/EntityComponent/Components/Zenith_ModelComponent_Editor.cpp and
// Zenith_TerrainComponent_Editor.cpp #include Editor/Zenith_Editor.h and
// Editor/Zenith_Editor_MaterialUI.h directly — three grandfathered
// `layer-up:` entries already record this in Tools/architecture_allowlist.txt
// (see Sharpmake_ZenithEntityComponent.cs's own "SEAM 1" paragraph, which
// this project's existence was already written to not break). This project
// must NOT add AddPublicDependency<ZenithEntityComponentLibProject> (or any
// equivalent), matching ZenithEntityComponent's own promise not to depend
// back. Declaring either direction creates a Sharpmake dependency cycle the
// tool cannot represent. Why it compiles and links anyway: every project's
// include path carries "/Zenith" (ConfigureCommonIncludePaths), so
// EntityComponent/Components/*.h resolve at COMPILE time off the plain
// include path with no declared edge at all — a static lib never needs to
// resolve an external symbol at its own link step (lib.exe does not link);
// any Zenith_EntityComponent-side symbol referenced out-of-line resolves at
// the final game EXE link, where ZenithEntityComponent is always present
// (every game links the full ZenithEngineLibs.All set). If the executable
// link ever turns out to need a nudge for this specific seam, that is a
// finding to report, not a reason to add the edge here.
//
// DEPENDENCY (direct #include edges, same "declare what is directly used"
// convention every phase-4 leaf follows — grep across every
// Zenith/Editor/**/*.{h,cpp} for headers outside this directory): Maths/
// (27 files) directly; DataStream/Zenith_DataStream.h directly; FileAccess/
// (15 files) directly; AssetHandling/ (29 files: Zenith_AssetHandle.h,
// Zenith_AssetRegistry.h, Zenith_MaterialAsset.h, Zenith_MeshAsset.h,
// Zenith_TextureAsset.h, Zenith_AnimatorControllerAsset.h, ...) directly;
// Flux/ (36 files, including Flux_GraphicsImpl.h — see the SHADER_SOURCE_ROOT
// paragraph below) directly; ZenithECS/ (29 files) directly; Physics/
// Zenith_Physics.h directly (no direct Jolt/JPH:: use — grep confirms zero
// <Jolt/...> or JPH:: references anywhere in this directory, so no
// ZenithJolt edge, exactly as ZenithEntityComponent's own Physics edge needs
// none either); Scripting/ (3 files: the Behaviour Graph editor integration)
// directly; Input/ (5 files) directly; DebugVariables/ (2 files) directly;
// Profiling/ (2 files) directly; Prefab/ (2 files) directly; Windows/
// Zenith_Windows_Window.h directly (5 files: Zenith_Editor.cpp,
// Zenith_EditorActions.cpp, Zenith_EditorCamera.cpp, Zenith_EditorUI.cpp,
// Panels/Zenith_EditorPanel_Memory.cpp — window handle + native file-dialog
// interop) — the first leaf besides ZenithWindows' own consumers to need this
// edge explicitly; and EntityComponent/ (see the SEAM paragraph above — NOT a
// declared edge) + UI/ (Zenith_UIComponent's editor affordances reach through
// EntityComponent, not directly — grep finds a single UI/ hit and it is
// inside a comment, not a live #include, so no ZenithUI edge is declared
// either; Editor reaches UI-owned rendering only through Flux). So this lib
// depends on ZenithCore (mandatory) + ZenithMaths + ZenithDataStream +
// ZenithFileAccess + ZenithAssetHandling + ZenithFlux + ZenithECS +
// ZenithPhysics + ZenithScripting + ZenithInput + ZenithDebugVariables +
// ZenithProfiling + ZenithPrefab + ZenithWindows. No ZenithAI/ZenithTaskSystem/
// ZenithSaveData/ZenithTelemetry/ZenithUI edge: grep across every
// Zenith/Editor/**/*.{h,cpp} finds no direct #include of any of those five
// directories' headers. No ZenithUnitTests edge: several TUs here #include
// UnitTests/Zenith_AssertCapture.h / Zenith_UnitTests.h through their sibling
// `.Tests.inl` (same as e.g. Flux_BufferReadback.Tests.inl does for
// ZenithFlux, which likewise declares no ZenithUnitTests edge) — those
// headers resolve off the plain include path with no link-time symbol needed
// from zenithunittests.lib itself.
//
// imgui.h — INCLUDE PATH ONLY, NOT A LINK EDGE, same reasoning as
// Sharpmake_ZenithUI.cs / Sharpmake_ZenithAssetHandling.cs /
// Sharpmake_ZenithFlux.cs / Sharpmake_ZenithEntityComponent.cs.
//
// SHADER_SOURCE_ROOT / ENGINE_ASSETS_DIR — NEEDED DIRECTLY, NOT JUST VIA THE
// PCH: 8 files here (Zenith_Editor.cpp, Zenith_Editor_MaterialUI.cpp,
// Zenith_Editor_SceneOps.cpp, Zenith_AnimationPreviewSession.cpp,
// Panels/Zenith_EditorPanel_Animation_Render.cpp,
// Panels/Zenith_EditorPanel_ContentBrowser.cpp,
// Panels/Zenith_EditorPanel_MaterialEditor.cpp,
// Panels/Zenith_EditorPanel_Viewport.cpp) #include Flux/Flux_GraphicsImpl.h
// directly, which reaches the active backend's Pipeline header
// (Zenith_Null_Pipeline.h / Zenith_D3D12_Pipeline.h), and both reference
// SHADER_SOURCE_ROOT directly, unguarded — the exact defect class
// Sharpmake_ZenithEntityComponent.cs / Sharpmake_ZenithUI.cs already document
// for the same header (ZenithUI needed it with ZERO textual occurrences of
// the token anywhere in its own directory — grepping the token is not a
// reliable predictor either way, so this project's TerrainEditor files below
// were checked the same way). Separately, 3 TerrainEditor files
// (Zenith_TerrainEditor.cpp, Zenith_TerrainEditor_Bake.cpp,
// Zenith_TerrainEditor_Trees.cpp) reference ENGINE_ASSETS_DIR directly,
// unguarded, for brush-texture and procedural-tree-mesh asset paths. Both
// defines previously existed only on the aggregate ZenithProject; this
// standalone leaf lib's own compiled TUs need both explicitly. No
// ZENITH_ROOT: grep for that token across Zenith/Editor finds zero hits —
// unlike Tools/Zenith_Tools_HumanModelExport.cpp / …MeshExport.cpp /
// …TextureExport.cpp, which DO reference it directly (see
// Sharpmake_ZenithTools.cs) — so ZENITH_ROOT belongs to ZenithTools, not
// this project.
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithEditor".
[Sharpmake.Generate]
public class ZenithEditorLibProject : ZenithBaseProject
{
	public ZenithEditorLibProject()
	{
		Name = "ZenithEditor";

		// WIN64 x TOOLS=TRUE ONLY — see the file header comment. No agde
		// AddTargets call, and ToolsEnabled carries only True (not True |
		// False) — the generated zenitheditor_win64.vcxproj must carry ONLY
		// *_True configurations, and no zenitheditor_agde.vcxproj at all.
		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True,
			RenderBackend = RenderBackend.Vulkan | RenderBackend.D3D12 | RenderBackend.Null
		});

		SourceRootPath = RootPath + "/Zenith/Editor";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithEditor\[target.Platform]\[conf.Name]";

		// Per-lib PCH: ZenithEditor CREATEs Zenith.pch from
		// Internal/Zenith_EditorPCH.cpp.
		conf.PrecompHeader = "Zenith.h";
		conf.PrecompSource = "Zenith_EditorPCH.cpp";

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf lib must not link or propagate the renderer backend —
		// it reaches Flux only through ZenithFlux's own backend-agnostic
		// surface, and the window handle it needs comes from ZenithWindows
		// (declared dependency below), not from calling glfw directly.

		// The PCH (Zenith.h) transitively pulls imgui + Flux/Vulkan/vma/Jolt
		// headers, and this project's own TUs #include imgui.h + Flux/**
		// headers + Windows/** headers + EntityComponent/** headers directly
		// (see the file header comment's DEPENDENCY / SEAM paragraphs) —
		// include paths only, no libs linked beyond the declared dependencies
		// below.
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		// SHADER_SOURCE_ROOT / ENGINE_ASSETS_DIR — see the file header
		// comment. Win64-only project (ToolsEnabled.True is implied — this
		// project declares no ToolsEnabled.False target at all), so a single
		// unconditional branch, matching the pattern every other leaf's
		// win64 arm uses (same value, no agde arm needed).
		string zenithRoot = Path.GetFullPath(Path.Combine(SharpmakeCsPath, "..")).Replace('\\', '/');
		string shaderSourceRoot = zenithRoot + "/Zenith/Flux/Shaders/";
		conf.Defines.Add($"SHADER_SOURCE_ROOT=\"{shaderSourceRoot}\"");
		string engineAssetRoot = zenithRoot + "/Zenith/Assets/";
		conf.Defines.Add($"ENGINE_ASSETS_DIR=\"{engineAssetRoot}\"");

		conf.Output = Configuration.OutputType.Lib;

		// MANDATORY: ZenithCore — see Sharpmake_ZenithCore.cs's header comment.
		conf.AddPublicDependency<ZenithCoreLibProject>(target);

		// Direct edges — see the file header comment's DEPENDENCY paragraph.
		conf.AddPublicDependency<ZenithMathsLibProject>(target);
		conf.AddPublicDependency<ZenithDataStreamLibProject>(target);
		conf.AddPublicDependency<ZenithFileAccessLibProject>(target);
		conf.AddPublicDependency<ZenithProfilingLibProject>(target);
		conf.AddPublicDependency<ZenithDebugVariablesLibProject>(target);
		conf.AddPublicDependency<ZenithInputLibProject>(target);
		conf.AddPublicDependency<ZenithAssetHandlingLibProject>(target);
		conf.AddPublicDependency<ZenithFluxLibProject>(target);
		conf.AddPublicDependency<ZenithECSLibProject>(target);
		conf.AddPublicDependency<ZenithPhysicsLibProject>(target);
		conf.AddPublicDependency<ZenithScriptingLibProject>(target);
		conf.AddPublicDependency<ZenithPrefabLibProject>(target);
		conf.AddPublicDependency<ZenithWindowsLibProject>(target);

		// PSApi -- GetProcessMemoryInfo. Phase 5 moved this off the
		// aggregate ZenithProject (Sharpmake_Zenith.cs), which had carried
		// it unconditionally for every win64 config (including
		// ToolsEnabled.False, where nothing used it) since before this
		// library existed. The real (and only) caller is
		// Panels/Zenith_EditorPanel_Memory.cpp GetProcessMemoryInfo call
		// -- grep confirms it is the sole GetProcessMemoryInfo call site in
		// the engine, and it lives in THIS library, not in Zenith/Core/ as
		// Phase 5s own brief assumed (Zenith/Core/Zenith_Win32.h merely
		// #includes psapi.h for the PROCESS_MEMORY_COUNTERS type -- a header
		// include costs nothing at link time; only an actual out-of-line
		// call pulls in the import lib). This project is win64 x
		// ToolsEnabled.True only (see the file header comment), so this is
		// a tightening versus the aggregates old unconditional placement:
		// non-tools builds no longer link a .lib nothing in them calls.
		conf.LibraryFiles.Add("psapi.lib");

		// Deliberately NO AddPublicDependency<ZenithEntityComponentLibProject>
		// (or equivalent), and NO ZenithUI/ZenithAI/ZenithTaskSystem/
		// ZenithSaveData/ZenithTelemetry edge — see the file header comment's
		// SEAM and DEPENDENCY paragraphs.

		// ZENITH_TOOLS is unconditional here — this project only ever exists
		// for ToolsEnabled.True (see the file header comment); there is no
		// "else" branch the way the aggregate's ConfigureAll needs one.
		conf.Defines.Add("ZENITH_TOOLS");

		// Deliberately NO Tools/Middleware (assimp/freetype/msdf-atlas-gen)
		// include paths and NO MsdfAtlasGenProject dependency: grep across
		// every Zenith/Editor/**/*.{h,cpp} finds zero references to assimp,
		// freetype or msdf-atlas-gen headers, and Zenith.h's own
		// `#ifdef ZENITH_TOOLS` blocks (the PCH every TU here compiles
		// against) pull in none of them either — unlike ZenithTools, whose
		// FontExport.cpp uses msdf-atlas-gen directly (see
		// Sharpmake_ZenithTools.cs). Copying the aggregate's old
		// ToolsEnabled.True block wholesale here would have been the "do not
		// just relocate the whole block" mistake this phase's brief warns
		// against.
	}
}
