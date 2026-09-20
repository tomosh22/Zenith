using Sharpmake;
using System;
using System.IO;

// ZenithEntityComponent — leaf static library (Phase 4f — see
// Sharpmake_ZenithCore.cs for the pattern this file replicates, and
// Sharpmake_ZenithFlux.cs for the closest-shaped sibling: both are dense,
// high-fan-in leaves declaring a one-way edge into a genuine header cycle).
// Holds exactly Zenith/EntityComponent — 43 translation units across its one
// subdirectory (Components/) + the top-level ECS<->engine glue:
//   Components/: Zenith_AIAgentComponent.cpp, Zenith_AnimatorComponent.cpp,
//     Zenith_AtmosphereComponent.cpp, Zenith_AttachmentComponent.cpp,
//     Zenith_CameraComponent.cpp, Zenith_ColliderComponent.cpp,
//     Zenith_GraphComponent.cpp, Zenith_InstancedMeshComponent.cpp,
//     Zenith_LightComponent.cpp, Zenith_ModelComponent.cpp,
//     Zenith_ModelComponent_Editor.cpp, Zenith_NavMeshComponent.cpp,
//     Zenith_ParticleEmitterComponent.cpp, Zenith_SunComponent.cpp,
//     Zenith_TerrainComponent.cpp, Zenith_TerrainComponent_Editor.cpp,
//     Zenith_TransformComponent.cpp, Zenith_TweenComponent.cpp,
//     Zenith_UIComponent.cpp (19 files)
//   Top level: Zenith_AIDebugVarsRegistration.cpp, Zenith_AINavGeometry.cpp,
//     Zenith_AIWorldHooksInstall.cpp, Zenith_CameraResolve.cpp,
//     Zenith_ComponentEditorRegistry.cpp, Zenith_ComponentMeta_Registration.cpp,
//     Zenith_FallenBodyWatch.cpp, Zenith_GraphNode_Registration.cpp,
//     Zenith_GraphNode_Registration_AI.cpp, Zenith_GraphNode_Registration_Animation.cpp,
//     Zenith_GraphNode_Registration_Entity.cpp, Zenith_GraphNode_Registration_Flow.cpp,
//     Zenith_GraphNode_Registration_Input.cpp, Zenith_GraphNode_Registration_Math.cpp,
//     Zenith_GraphNode_Registration_Physics.cpp, Zenith_GraphNode_Registration_Scene.cpp,
//     Zenith_GraphNode_Registration_UI.cpp, Zenith_GraphReload.cpp,
//     Zenith_PhysicsDebugDraw.cpp, Zenith_PhysicsQuery.cpp,
//     Zenith_PhysicsTransformSync.cpp, Zenith_PhysicsWorldHooksInstall.cpp,
//     Zenith_TerrainPhysicsValidate.cpp, Zenith_UISystem.cpp (24 files)
// = 43, + the definition-free PCH create TU
// (Internal/Zenith_EntityComponentPCH.cpp). 43 + 1 = 44 compiled TUs.
// Previously globbed straight into the monolithic aggregate ZenithProject
// (its own header comment called this out by name: "The aggregate STILL
// compiles the engine-side EntityComponent/ files").
//
// ★ REGISTRAR-TU SURVIVAL (see Sharpmake_ZenithFlux.cs's own paragraph on the
// same hazard): this directory is the densest concentration of
// self-registering TUs in the engine — every Components/*.cpp carries a
// ZENITH_REGISTER_COMPONENT registrar and most of the top-level
// Zenith_GraphNode_Registration*.cpp carry RegisterNodeType<> registrars
// whose only reference is their own static initializer. /OPT:REF dead-strips
// an .obj with no OTHER inbound reference. This split changes which .lib the
// same 43 .obj land in (zenithentitycomponent.lib instead of zenith.lib) but
// not which .obj the final EXE link pulls in (every engine .lib is still an
// input via ZenithEngineLibs.All) — verification step 7 (the unit gate must
// report exactly 2706) is what would catch it if that reasoning is ever
// wrong for a future TU.
//
// LAYER 6 — EntityComponent sits at the TOP of the measured header DAG: it
// depends on nearly every leaf below it, including this same phase's other
// new library, ZenithUI (see Sharpmake_ZenithUI.cs's own header comment for
// the UI-side of that edge).
//
// ═══════════════════════════════════════════════════════════════════════
// SEAM 1 — EntityComponent <-> Editor: REAL, and DELIBERATELY UNDECLARED.
// ═══════════════════════════════════════════════════════════════════════
// Zenith/EntityComponent/Components/Zenith_ModelComponent_Editor.cpp and
// Zenith_TerrainComponent_Editor.cpp #include Editor/Zenith_Editor.h and
// Editor/Zenith_Editor_MaterialUI.h directly — three grandfathered
// `layer-up:` entries already record this in Tools/architecture_allowlist.txt
// (Zenith_ModelComponent_Editor.cpp / Zenith_TerrainComponent_Editor.cpp ->
// Editor/Zenith_Editor*.h, plus Components/Zenith_UIComponent.h -> UI/Zenith_UI.h,
// which THIS phase turns into a real declared dependency — see
// Sharpmake_ZenithUI.cs). This project must NOT add
// AddPublicDependency<ZenithEditorLibProject> (or any equivalent), and a
// future ZenithEditor library (a LATER phase — Editor/ is still aggregate-owned
// today, not yet extracted) must correspondingly NOT declare a dependency
// back on ZenithEntityComponent. Declaring either direction creates a
// Sharpmake dependency cycle the tool cannot represent.
// Why it compiles and links anyway: ConfigureCommonIncludePaths puts
// "/Zenith" on every project's include path (Sharpmake_Common.cs), so
// Editor/Zenith_Editor*.h resolve at COMPILE time off the plain include path
// with no declared edge at all — compiling a static lib never needs to
// resolve an external symbol (lib.exe does not link). Any Zenith_Editor::
// symbol referenced out-of-line resolves at the final game EXE link, where
// the aggregate ZenithProject (which owns Editor/ today) is always present.
// Do not "fix" this by adding a declared edge in either direction — it does
// not need fixing, and adding one breaks the graph.
//
// ═══════════════════════════════════════════════════════════════════════
// SEAM 2 — EntityComponent <-> Flux: the KNOWN, DELIBERATE module cycle.
// ═══════════════════════════════════════════════════════════════════════
// Tools/complexity_profiles.json already lists
// architecture.rules.ignore_module_cycles: [["EntityComponent", "Flux"]] —
// this directory reaches deep into Flux/** (InstancedMeshes, MeshAnimation,
// MeshGeometry, Particles, Primitives, RenderGraph, Skybox, Terrain, plus
// Flux_GraphicsImpl.h / Flux_ImGuiIntegration.h / Flux_ModelInstance.h /
// Flux_RendererImpl.h at the top level) for the same reason
// Sharpmake_ZenithFlux.cs's AssetHandling paragraph gives: components own
// Flux types/handles concretely, not through a forward-declarable pointer
// seam. This project declares AddPublicDependency<ZenithFluxLibProject> (the
// heavy direction) and does NOT expect Flux to depend back — Flux's own
// Sharpmake file has no EntityComponent edge and must not gain one; any
// stray reverse #include (there is none found by grep today) would resolve
// the same include-path-only way SEAM 1 does.
//
// ═══════════════════════════════════════════════════════════════════════
// SEAM 3 — AI -> EntityComponent: VERIFIED TEST-ONLY, NOTHING TO DROP.
// ═══════════════════════════════════════════════════════════════════════
// ZenithAI (Sharpmake_ZenithAI.cs) carries NO declared dependency on this
// project today — its AddPublicDependency list is exactly ZenithCore,
// ZenithMaths, ZenithDataStream, ZenithFileAccess, ZenithECS, ZenithPhysics.
// The only "EntityComponent" references under Zenith/AI are three
// `.Tests.inl` files (Zenith_NavMeshAgent.Tests.inl,
// Zenith_PerceptionSystem.Tests.inl, Zenith_TacticalPoint.Tests.inl), each
// #including EntityComponent/Components/Zenith_TransformComponent.h so its
// unit tests can construct a transform on a test entity. These compile
// today off the same "/Zenith" global include path SEAM 1 above documents —
// no declared Sharpmake dependency backs them, and none should be added:
// declaring ZenithAI -> ZenithEntityComponent here would invert this
// project's own ZenithEntityComponent -> ZenithAI edge (below) into a cycle.
// There was nothing to drop and no file to move; this is exactly the
// "if ZenithAI has no such declared dependency today, report that and move
// on" case.
//
// DEPENDENCY (direct #include edges beyond Flux/Editor/AI above, same
// "declare what is directly used" convention every phase-4 leaf follows —
// grep across every Zenith/EntityComponent/**/*.{h,cpp} for headers outside
// this directory): Maths/Zenith_Maths.h + Zenith_FrustumCulling.h directly;
// DataStream/Zenith_DataStream.h directly (every component's
// WriteToDataStream/ReadFromDataStream); FileAccess/Zenith_FileAccess.h
// directly; DebugVariables/Zenith_DebugVariables.h directly
// (Zenith_AIDebugVarsRegistration.cpp); Profiling/Zenith_Profiling.h
// directly; TaskSystem/Zenith_TaskSystem.h directly; Input/Zenith_Input.h +
// Zenith_InputActions.h + Zenith_InputSimulator.h + Zenith_KeyCodes.h +
// Zenith_Pointers.h directly; AssetHandling/Zenith_AssetHandle.h +
// Zenith_AssetRegistry.h + Zenith_AnimatorControllerAsset.h +
// Zenith_BehaviourGraphAsset.h + Zenith_Image.h + Zenith_MaterialAsset.h +
// Zenith_MeshAsset.h + Zenith_MeshGeometryAsset.h + Zenith_ModelAsset.h +
// Zenith_SkeletonAsset.h + Zenith_TextureAsset.h directly; ZenithECS/
// Zenith_ComponentMeta.h + Zenith_Entity.h + Zenith_Query.h + Zenith_Scene.h
// + Zenith_SceneData.h + Zenith_SceneSystem.h directly (this IS the
// ECS<->engine glue layer); Physics/Zenith_Physics.h +
// Zenith_PhysicsMeshGenerator.h + Zenith_PhysicsWorldHooks.h +
// Zenith_Physics_Fwd.h directly; AI/Navigation/** + AI/Perception/** +
// AI/Squad/** + AI/Zenith_AIDebugVariables.h + AI/Zenith_AIWorldHooks.h
// directly; Scripting/Zenith_BehaviourGraph.h + Zenith_GraphBlackboard.h +
// Zenith_GraphBuilder.h + Zenith_GraphChain.h + Zenith_GraphNode.h +
// Zenith_GraphNodeRegistry.h + Zenith_GraphPinTable.h directly (the whole
// Behaviour Graph node-library surface); Prefab/Zenith_Prefab.h directly;
// UI/Zenith_UI.h + Zenith_UIButton.h + Zenith_UIRect.h + Zenith_UIText.h
// directly (SEAM covered by Sharpmake_ZenithUI.cs). So this lib depends on
// ZenithCore (mandatory) + ZenithMaths + ZenithDataStream + ZenithFileAccess
// + ZenithProfiling + ZenithDebugVariables + ZenithTaskSystem + ZenithInput +
// ZenithAssetHandling + ZenithFlux + ZenithECS + ZenithPhysics + ZenithAI +
// ZenithScripting + ZenithPrefab + ZenithUI. No ZenithUnitTests edge: several
// TUs here #include UnitTests/Zenith_AssertCapture.h / Zenith_TempScene.h /
// Zenith_UnitTests.h through their sibling `.Tests.inl` (same as e.g.
// Flux_BufferReadback.Tests.inl does for ZenithFlux, which likewise declares
// no ZenithUnitTests edge) — those headers resolve off the plain include
// path with no link-time symbol needed from zenithunittests.lib itself.
//
// imgui.h — INCLUDE PATH ONLY, NOT A LINK EDGE, same reasoning as
// Sharpmake_ZenithUI.cs / Sharpmake_ZenithAssetHandling.cs / Sharpmake_ZenithFlux.cs.
//
// SHADER_SOURCE_ROOT — NEEDED DIRECTLY, NOT JUST VIA THE PCH: this project
// #includes Flux/Flux_GraphicsImpl.h directly (Zenith_TerrainComponent.cpp
// and others), which reaches the active backend's Pipeline header
// (Zenith_Null_Pipeline.h / Zenith_D3D12_Pipeline.h), and both reference
// SHADER_SOURCE_ROOT directly, unguarded — the exact defect class
// Sharpmake_ZenithAssetHandling.cs and Sharpmake_ZenithDebugVariables.cs
// already document for the same header. This define previously existed only
// on the aggregate ZenithProject and each GameProject, neither of which this
// standalone leaf lib is. Mirrors ZenithProject's own identical win64/agde
// branch exactly (same value, same "" on agde). No ENGINE_ASSETS_DIR: grep
// for that token across Zenith/EntityComponent finds zero unguarded hits in
// any header this project's TUs reach (verified against the Null_/D3D12_
// builds in this phase's verification steps 5/6 — see that file's own
// paragraph on how ZenithFlux/ZenithAssetHandling needed it and this one
// does not).
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithEntityComponent".
[Sharpmake.Generate]
public class ZenithEntityComponentLibProject : ZenithBaseProject
{
	public ZenithEntityComponentLibProject()
	{
		Name = "ZenithEntityComponent";

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

		SourceRootPath = RootPath + "/Zenith/EntityComponent";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithEntityComponent\[target.Platform]\[conf.Name]";

		// Per-lib PCH: ZenithEntityComponent CREATEs Zenith.pch from
		// Internal/Zenith_EntityComponentPCH.cpp.
		conf.PrecompHeader = "Zenith.h";
		conf.PrecompSource = "Zenith_EntityComponentPCH.cpp";

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf lib must not link or propagate the renderer backend —
		// it reaches Flux only through ZenithFlux's own backend-agnostic
		// surface (see the file header comment's SEAM 2 paragraph).

		// The PCH (Zenith.h) transitively pulls imgui + Flux/Vulkan/vma/Jolt
		// headers, and this project's own TUs #include imgui.h + Flux/**
		// headers + Editor/** headers directly (see the file header
		// comment's SEAM 1/2 paragraphs) — include paths only, no libs
		// linked beyond the declared dependencies below.
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
		conf.AddPublicDependency<ZenithFileAccessLibProject>(target);
		conf.AddPublicDependency<ZenithProfilingLibProject>(target);
		conf.AddPublicDependency<ZenithDebugVariablesLibProject>(target);
		conf.AddPublicDependency<ZenithTaskSystemLibProject>(target);
		conf.AddPublicDependency<ZenithInputLibProject>(target);
		conf.AddPublicDependency<ZenithAssetHandlingLibProject>(target);

		// SEAM 2 — the heavy direction of the declared EntityComponent<->Flux
		// cycle (Tools/complexity_profiles.json's ignore_module_cycles).
		// Flux does NOT depend back on this project. See the file header
		// comment.
		conf.AddPublicDependency<ZenithFluxLibProject>(target);

		conf.AddPublicDependency<ZenithECSLibProject>(target);
		conf.AddPublicDependency<ZenithPhysicsLibProject>(target);
		conf.AddPublicDependency<ZenithAILibProject>(target);
		conf.AddPublicDependency<ZenithScriptingLibProject>(target);
		conf.AddPublicDependency<ZenithPrefabLibProject>(target);

		// LAYER 5 -> LAYER 6 edge, this same phase's other new library. See
		// Sharpmake_ZenithUI.cs's header comment for the UI-side of this
		// one-way edge.
		conf.AddPublicDependency<ZenithUILibProject>(target);

		// Deliberately NO AddPublicDependency<ZenithEditorLibProject> (or
		// equivalent) and NO edge to/from ZenithAI the other way — see the
		// file header comment's SEAM 1 / SEAM 3 paragraphs.

		if (target.ToolsEnabled == ToolsEnabled.True && target.Platform == Platform.win64)
		{
			conf.Defines.Add("ZENITH_TOOLS");
		}
	}
}
