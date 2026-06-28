#include "Zenith.h"
#include "Zenith_Core.h"
#include "Core/FrameContext.h"
#include "Core/Zenith_CommandLine.h"
#include "Core/Zenith_Engine.h"

// InputSimulator + AutomatedTest are both gated on ZENITH_INPUT_SIMULATOR. The
// nested #ifdef the previous version emitted around the AutomatedTest include
// was vacuously true inside the outer guard — collapsed to a single block.
#ifdef ZENITH_INPUT_SIMULATOR
#include "Input/Zenith_InputSimulator.h"
#include "Core/Zenith_AutomatedTest.h"
#endif
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "AI/Zenith_AI.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"
#include "EntityComponent/Zenith_PhysicsDebugDraw.h"
#include "EntityComponent/Zenith_PhysicsTransformSync.h"
#include "EntityComponent/Zenith_UISystem.h"
#include "Flux/Flux.h"
#include "Flux/Flux_RendererImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_PerFrame.h"
#include "Flux/Fog/Flux_FogImpl.h"
#include "Flux/IBL/Flux_IBLImpl.h"
#include "Flux/SSR/Flux_SSRImpl.h"
#include "Flux/SSGI/Flux_SSGIImpl.h"
#ifdef ZENITH_TOOLS
#include "AssetHandling/Zenith_PropertyTuning.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_SceneGraphDebug.h"
#include "EntityComponent/Zenith_GraphReload.h"
#endif
#include "Input/Zenith_Input.h"
#include "Input/Zenith_TouchInput.h"
#include "Physics/Zenith_Physics.h"
#include "Physics/Zenith_PhysicsMeshGenerator.h"


void Zenith_Core::UpdateTimers()
{
	FrameContext& xFrame = g_xEngine.Frame();
	const std::chrono::high_resolution_clock::time_point xCurrentTime = std::chrono::high_resolution_clock::now();

#ifdef ZENITH_INPUT_SIMULATOR
	if (Zenith_InputSimulator::HasFixedDtOverride())
	{
		xFrame.SetDt(Zenith_InputSimulator::GetFixedDt());
	}
	else
#endif
	{
		xFrame.SetDt(static_cast<float>(std::chrono::duration_cast<std::chrono::nanoseconds>(xCurrentTime - xFrame.GetLastFrameTime()).count() / 1.e9));
	}
	xFrame.SetLastFrameTime(xCurrentTime);

	xFrame.AddTimePassed(xFrame.GetDt());
}

static void ExecuteRenderGraph()
{
	// Check if any subsystem requested a full graph rebuild
	if (g_xEngine.FluxRenderer().ConsumeGraphRebuildRequest())
	{
		g_xEngine.FluxRenderer().SetupRenderGraph();
	}

	Flux_RenderGraph& xGraph = g_xEngine.FluxRenderer().GetRenderGraph();

	// Forward any debug-variable toggles that affect graph compilation (e.g.
	// transient aliasing) into the graph each frame. Cheap no-op when unchanged;
	// triggers MarkDirty on change so editor flips apply immediately instead of
	// waiting until the next SetupRenderGraph (which only runs on resize).
	g_xEngine.FluxRenderer().SyncRenderGraphDebugToggles();

	// Apply per-subsystem runtime selections (Fog technique, SSR blur,
	// SSGI denoise, IBL pass enable). Order/rationale lives in
	// Flux_RendererImpl::ApplySubsystemGraphSelections.
	g_xEngine.FluxRenderer().ApplySubsystemGraphSelections(xGraph);

	xGraph.Compile();
	xGraph.Execute();
}

// --- Per-frame phase helpers ----------------------------------------------
// Zenith_MainLoop below is just a linear sequence of these. Each phase owns its
// own platform/tools/sim #ifdef divergence so the top-level loop reads as a
// phase list. PROFILE index names are preserved verbatim for timeline
// comparability with pre-extraction runs.

// Backend per-frame begin (wait-fence + reset-pools, NOP in --headless) then
// input/window/touch begin-frame. Manual profile scope because BeginFrame is a
// member function, not a free callable.
static void BeginFrame_Platform()
{
	{
		Zenith_Profiling::ScopeZone xBeginFrameProfile(ZENITH_PROFILE_ZONE("Flux PlatformAPI Begin Frame"));
		g_xEngine.FluxRenderer().BeginFrame();
	}
	Zenith_Core::UpdateTimers();
	g_xEngine.Input().BeginFrame();
	Zenith_Window::GetInstance()->BeginFrame();
	ZENITH_PROFILING_FUNCTION_WRAPPER(g_xEngine.Touch().Update, ZENITH_PROFILE_ZONE("Touch Update"));
}

// Acquire the swapchain image (windowed only). Returns true to proceed (always
// in --headless). Returns false on a failed acquire (resize): it runs the
// skipped-frame end-of-frame work and the caller must return WITHOUT advancing
// the frame index — a rapid-resize run of consecutive skips would otherwise wrap
// the ring index past valid fences and shorten the deferred-deletion grace period.
static bool AcquireSwapchainOrSkip()
{
	if (Zenith_CommandLine::IsHeadless()) return true;
	if (g_xEngine.FluxSwapchain().BeginFrame()) return true;

	g_xEngine.FluxMemory().Flush();
	g_xEngine.FluxRenderer().ProcessFrameEnd();
	return false;
}

// Tools-only: editor update (where deferred scene loads happen — MUST run before
// any game logic / render, with no render tasks active), the live property +
// behaviour-graph hot-reload safe sync point, and the render-submit / game-logic
// gates derived from editor mode. In non-tools builds the gates keep the
// caller-set defaults (submit = !headless, game logic = on).
static void UpdateEditorAndTuning(bool& bSubmitRenderWork, bool& bShouldUpdateGameLogic)
{
#ifdef ZENITH_TOOLS
	bool bEditorWantsRender;
	{
		// Editor update (deferred scene loads, editor state, gizmo/selection logic).
		Zenith_Profiling::ScopeZone xEditorUpdateProfile(ZENITH_PROFILE_ZONE("Editor Update"));
		bEditorWantsRender = g_xEngine.Editor().Update();
	}
	if (!Zenith_CommandLine::IsHeadless())
	{
		bSubmitRenderWork = bEditorWantsRender;
	}
	bShouldUpdateGameLogic = (g_xEngine.Editor().GetEditorMode() == EditorMode::Playing);

	ZENITH_PROFILING_FUNCTION_WRAPPER(Zenith_PropertyTuning::Update, ZENITH_PROFILE_ZONE("Property Tuning"));
	ZENITH_PROFILING_FUNCTION_WRAPPER(Zenith_GraphReload::Update, ZENITH_PROFILE_ZONE("Graph Reload"));
#else
	(void)bSubmitRenderWork;
	(void)bShouldUpdateGameLogic;
#endif
}

// Sim-only: pump the automated-test state machine AFTER the editor update (so a
// transition into Playing takes effect next frame) and re-read the game-logic
// gate in case Tick() switched into Playing.
static void PumpAutomatedTest(bool& bShouldUpdateGameLogic)
{
#ifdef ZENITH_INPUT_SIMULATOR
	Zenith_AutomatedTestRunner::Tick();
	#ifdef ZENITH_TOOLS
	bShouldUpdateGameLogic = (g_xEngine.Editor().GetEditorMode() == EditorMode::Playing);
	#else
	// Non-tools (_False): no editor mode to re-read, so the gate is unchanged.
	// Mark used to silence C4100 (ZENITH_INPUT_SIMULATOR is defined unconditionally,
	// so this branch compiles in every _False config). Pre-existing warning.
	(void)bShouldUpdateGameLogic;
	#endif
#else
	(void)bShouldUpdateGameLogic;
#endif
}

// Physics + scene simulation (only in Playing mode / non-tools), then tear down
// per-frame simulated input AFTER the scene/script update has consumed it
// (clears the mouse-wheel delta — see Zenith_InputSimulator::EndOfFrameTickComplete).
static void UpdateGameLogic(bool bShouldUpdateGameLogic)
{
	if (bShouldUpdateGameLogic)
	{
		ZENITH_PROFILING_FUNCTION_WRAPPER(g_xEngine.Physics().Update, ZENITH_PROFILE_ZONE("Physics"), g_xEngine.Frame().GetDt());

		// Scene-graph transform cache (Phase 1): sync any body that the simulation just
		// moved into the owning Transform's cache + invalidate its subtree, BEFORE Scene
		// Update runs animation/game logic that reads BuildModelMatrix. Must sit between
		// Physics().Update() and Scenes().Update().
		Zenith_SyncPhysicsTransforms();

		ZENITH_PROFILING_FUNCTION_WRAPPER(g_xEngine.Scenes().Update, ZENITH_PROFILE_ZONE("Scene Update"), g_xEngine.Frame().GetDt());

		// Optional engine-driven AI manager tick (opt-in, default off). Most games
		// drive the AI managers from their own components in a game-specific order;
		// a game with no such constraint opts in via Zenith_AI::SetEngineTickEnabled.
		if (Zenith_AI::IsEngineTickEnabled())
		{
			ZENITH_PROFILING_FUNCTION_WRAPPER(Zenith_AI::Update, ZENITH_PROFILE_ZONE("AI Update"), g_xEngine.Frame().GetDt());
		}
	}
#ifdef ZENITH_INPUT_SIMULATOR
	Zenith_InputSimulator::EndOfFrameTickComplete();
#endif
}

// Upload frame constants (windowed), then — only when submitting render work
// (skipped in --headless and during scene transitions, to avoid recording
// against incomplete scene state) — the UI frame, the ImGui frame, and the
// render-graph execute bracketed by the SetRenderTasksActive window (so scene
// reads on render-worker threads know the window is open).
static void SubmitRenderWork(bool bSubmitRenderWork)
{
	if (!Zenith_CommandLine::IsHeadless())
	{
		g_xEngine.FluxGraphics().UploadFrameConstants();
	}

	if (!bSubmitRenderWork) return;

	// Physics debug primitives only while stopped, so play mode doesn't flood them.
	#ifdef ZENITH_TOOLS
	if (g_xEngine.Editor().GetEditorMode() == EditorMode::Stopped)
	{
		Zenith_PhysicsDebugDraw::QueueAll();
	}
	#endif

	// UI frame (quad/text submission) must precede ExecuteRenderGraph, which
	// consumes the submissions. The two-pass structure + deferred-LoadScene drain
	// lives inside Zenith_UISystem::Update.
	ZENITH_PROFILING_FUNCTION_WRAPPER(g_xEngine.UI().Update, ZENITH_PROFILE_ZONE("UI Update"), g_xEngine.Frame().GetDt());

	// W22: ordering constraint documented on Flux_RenderGraph::Execute.
	#ifdef ZENITH_TOOLS
	ZENITH_PROFILING_FUNCTION_WRAPPER(g_xEngine.Editor().RenderImGuiFrame, ZENITH_PROFILE_ZONE("ImGUI"));
	#endif

	// Scene-graph snapshot (Phase 2): the renderer owns the uncullled master list and
	// rebuilds it EXACTLY ONCE here — after UI().Update() drained deferred scene loads
	// and after ImGui transform edits, immediately before the render-task window opens.
	// Rebuilding here (not in a pass Prepare) means every consumer derives from the same
	// fresh list regardless of which passes are enabled, and no entry can dangle from an
	// entity a late scene-load destroyed. The epoch is passed explicitly.
	ZENITH_PROFILING_FUNCTION_WRAPPER(g_xEngine.FluxRenderer().RebuildSceneSnapshot,
		ZENITH_PROFILE_ZONE("Snapshot::Build"), g_xEngine.Scenes().GetRenderMutationEpoch(),
		g_xEngine.FluxGraphics().GetViewProjMatrix(), g_xEngine.FluxGraphics().IsCameraValid());

#ifdef ZENITH_TOOLS
	// Phase 3: queue the scene-graph debug overlays (world-AABB wireframes + cull stats)
	// from the just-rebuilt snapshot, before the render graph records the Primitives pass.
	Zenith_SceneGraphDebug::QueueOverlays(g_xEngine.FluxRenderer().GetSceneSnapshot(),
		g_xEngine.Scenes().GetRenderMutationEpoch());
#endif

	// Stage 0 (inert): build the unified GPU-driven mesh scene (bucket topology + GPU-scene
	// records) from the just-rebuilt snapshot, on the main thread before the render-task
	// window opens — the same single-writer placement as the snapshot rebuild. Its
	// RequestGraphRebuild (Stage 1+) would land before ConsumeGraphRebuildRequest at the
	// top of ExecuteRenderGraph, giving a same-frame rebuild. Sampled by nothing until
	// Stage 1; gated on the Render/UnifiedMesh/Enabled toggle.
	{
		Zenith_Profiling::ScopeZone xUnifiedSyncProfile(ZENITH_PROFILE_ZONE("UnifiedMesh::Sync"));
		g_xEngine.FluxRenderer().SyncUnifiedBucketsFromSnapshot();
	}

	g_xEngine.Scenes().SetRenderTasksActive(true);
	ExecuteRenderGraph();
	g_xEngine.Scenes().SetRenderTasksActive(false);
}

// Hand this frame's lazily-recorded memory work to the backend (submitted ahead
// of the render command buffers against the memory semaphore in EndFrame — no
// memory op may run between here and that submit), then record + submit the
// render command buffers (windowed only). Manual profile scopes because these
// are instance methods, not free callables.
static void EndFrameSubmitAndPresent(bool bSubmitRenderWork)
{
	if (!Zenith_CommandLine::IsHeadless())
	{
		Zenith_Profiling::ScopeZone xMemMgrProfile(ZENITH_PROFILE_ZONE("Flux Memory Manager"));
		g_xEngine.FluxMemory().SubmitFrameMemoryWork();
	}

	Zenith_MemoryManagement::EndFrame();

	if (!Zenith_CommandLine::IsHeadless())
	{
		{
			Zenith_Profiling::ScopeZone xEndFrameProfile(ZENITH_PROFILE_ZONE("Flux PlatformAPI End Frame"));
			g_xEngine.FluxBackend().EndFrame(bSubmitRenderWork);
		}
		{
			Zenith_Profiling::ScopeZone xSwapchainEndFrameProfile(ZENITH_PROFILE_ZONE("Flux Swapchain End Frame"));
			g_xEngine.FluxSwapchain().EndFrame();
		}
	}
}

void Zenith_Core::Zenith_MainLoop()
{
	BeginFrame_Platform();

	if (!AcquireSwapchainOrSkip())
	{
		// Resize-skip: end-of-frame cleanup ran inside the helper; the frame index
		// is deliberately NOT advanced (see AcquireSwapchainOrSkip).
		return;
	}

	bool bSubmitRenderWork      = !Zenith_CommandLine::IsHeadless();
	bool bShouldUpdateGameLogic = true;
	UpdateEditorAndTuning(bSubmitRenderWork, bShouldUpdateGameLogic);
	PumpAutomatedTest(bShouldUpdateGameLogic);

	UpdateGameLogic(bShouldUpdateGameLogic);
	SubmitRenderWork(bSubmitRenderWork);
	EndFrameSubmitAndPresent(bSubmitRenderWork);

	// End of frame: deferred-deletion countdown, then advance the engine frame
	// index. The advance happens AFTER Swapchain::EndFrame so the present uses the
	// slot for frame N before the ring index moves to N+1. NOP-safe in headless.
	g_xEngine.FluxRenderer().ProcessFrameEnd();
	g_xEngine.Frame().AdvanceFrameIndex();
}

#ifdef ZENITH_TESTING
#include "Core/Zenith_UnitTests.Tests.inl"
#endif
