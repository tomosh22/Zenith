#include "Zenith.h"
#include "Zenith_Core.h"
#include "Core/FrameContext.h"
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
#include "EntityComponent/Zenith_FallenBodyWatch.h"
#include "EntityComponent/Zenith_UISystem.h"
#include "Flux/Flux.h"
#include "Flux/Flux_RendererImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_PerFrame.h"
#include "Flux/Fog/Flux_FogImpl.h"
#include "Flux/IBL/Flux_IBLImpl.h"
#include "Flux/SSR/Flux_SSRImpl.h"
#include "Flux/SSGI/Flux_SSGIImpl.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"   // Stage 2: hoisted UpdateShadowMatrices (pre-gather)
#include "Flux/Slang/Flux_SpirvUsage.h"       // hosted unit: the BINDLESS static-use scan
#include "Core/Zenith_GraphicsOptions.h"      // m_bShadowsEnabled gate for the hoisted update
#ifdef ZENITH_TOOLS
#include "AssetHandling/Zenith_PropertyTuning.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_SceneGraphDebug.h"
#include "EntityComponent/Zenith_GraphReload.h"
#endif
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputActions.h"
#include "Input/Zenith_Pointers.h"
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

// Backend per-frame begin (wait-fence + reset-pools, a no-op on the Null backend),
// timers, then the platform PUMP. Manual profile scope because BeginFrame is a
// member function, not a free callable.
//
// Frame contract step 1. Window::BeginFrame is glfwPollEvents on Windows (Android
// is pumped by the ALooper loop before Zenith_MainLoop is entered); its callbacks
// ONLY enqueue into the input FIFO and change no state. The gamepad poll-diff
// joins it here, so a pad tap that lands on a frame the swapchain later skips is
// still enqueued and survives to the next drain. The DRAIN itself deliberately
// does NOT happen here — see below.
static void BeginFrame_Platform()
{
	{
		Zenith_Profiling::ScopeZone xBeginFrameProfile(ZENITH_PROFILE_ZONE("Flux PlatformAPI Begin Frame"));
		g_xEngine.FluxRenderer().BeginFrame();
	}
	Zenith_Core::UpdateTimers();
	Zenith_Window::GetInstance()->BeginFrame();
	g_xEngine.Input().PollGamepads();
}

// Frame contract step 3: the input DRAIN, plus step 4, the pointer table that
// consumes what it produced. Sits AFTER the acquire gate on purpose — a frame
// the swapchain skips runs no game logic, so draining there would consume
// presses, releases and wheel ticks that nothing would ever see. Skipping
// instead leaves the FIFO and the retained pad snapshot intact for the next
// real frame.
static void DrainInputAndPointers()
{
	g_xEngine.Input().BeginFrame();
	{
		Zenith_Profiling::ScopeZone xPointerProfile(ZENITH_PROFILE_ZONE("Pointers Update"));
		g_xEngine.Pointers().ApplyPlatform(g_xEngine.Input(),
			Zenith_Window::GetInstance()->GetDisplayScale());
	}
}

// Acquire the swapchain image. Returns false on a failed acquire (resize): it
// runs the skipped-frame end-of-frame work and the caller must return WITHOUT
// advancing the frame index — a rapid-resize run of consecutive skips would
// otherwise wrap the ring index past valid fences and shorten the
// deferred-deletion grace period. The Null backend's swapchain always succeeds.
static bool AcquireSwapchainOrSkip()
{
	if (g_xEngine.FluxSwapchain().BeginFrame()) return true;

	g_xEngine.FluxMemory().Flush();
	g_xEngine.FluxRenderer().ProcessFrameEnd();
	return false;
}

// Tools-only: editor update (where deferred scene loads happen — MUST run before
// any game logic / render, with no render tasks active), the live property +
// behaviour-graph hot-reload safe sync point, and the render-submit / game-logic
// gates derived from editor mode. In non-tools builds the gates keep the
// caller-set defaults (submit = on, game logic = on).
static void UpdateEditorAndTuning(bool& bSubmitRenderWork, bool& bShouldUpdateGameLogic)
{
#ifdef ZENITH_TOOLS
	bool bEditorWantsRender;
	{
		// Editor update (deferred scene loads, editor state, gizmo/selection logic).
		Zenith_Profiling::ScopeZone xEditorUpdateProfile(ZENITH_PROFILE_ZONE("Editor Update"));
		bEditorWantsRender = g_xEngine.Editor().Update();
	}
	bSubmitRenderWork = bEditorWantsRender;
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

		// Immediately after the sweep, so the poses it just committed are the ones
		// inspected: report (once per fall) any DYNAMIC body that has left the world.
		// This is the per-body counterpart to Zenith_ValidateTerrainPhysicsBodies'
		// whole-world check — see Zenith_FallenBodyWatch.h for why one does not
		// substitute for the other.
		Zenith_TickFallenBodyWatch(g_xEngine.Frame().GetDt());

		ZENITH_PROFILING_FUNCTION_WRAPPER(g_xEngine.Scenes().Update, ZENITH_PROFILE_ZONE("Scene Update"), g_xEngine.Frame().GetDt());

		// Optional engine-driven AI manager tick (opt-in, default off). Most games
		// drive the AI managers from their own components in a game-specific order;
		// a game with no such constraint opts in via Zenith_AI::SetEngineTickEnabled.
		if (Zenith_AI::IsEngineTickEnabled())
		{
			ZENITH_PROFILING_FUNCTION_WRAPPER(Zenith_AI::Update, ZENITH_PROFILE_ZONE("AI Update"), g_xEngine.Frame().GetDt());
		}

#ifdef ZENITH_TOOLS
		// AI debug visualisation, driven by the AI/* debug variables. NOT inside
		// the IsEngineTickEnabled() branch above: most games tick the AI managers
		// from their own components, and the panel toggles have to work for them
		// too. Self-gating and read-only — see Zenith_AI::DebugDraw.
		Zenith_AI::DebugDraw();
#endif
	}
#ifdef ZENITH_INPUT_SIMULATOR
	Zenith_InputSimulator::EndOfFrameTickComplete();
#endif
}

// Upload frame constants (windowed), then — only when submitting render work
// (skipped during scene transitions, to avoid recording
// against incomplete scene state) — the UI frame, the ImGui frame, and the
// render-graph execute bracketed by the SetRenderTasksActive window (so scene
// reads on render-worker threads know the window is open).
static void SubmitRenderWork(bool bSubmitRenderWork)
{
	g_xEngine.FluxGraphics().UploadFrameConstants();

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

	// Compute the sun cascade view×proj matrices here (main thread, before the render-task
	// window) rather than in the shadow cascade-0 Prepare. Camera-derived like the snapshot
	// frustum above, and hoisting it ahead of the render graph's Prepare phase means the unified
	// mesh cull's Prepare — which runs earlier in topological order once the cascade passes read
	// its cull-output buffers (Stage 2) — sees up-to-date cascade frustums. Behaviour-preserving
	// for the non-unified shadow path: nothing consumes the matrices before the graph executes.
	// (UpdateShadowMatrices profiles itself internally.)
	if (Zenith_GraphicsOptions::Get().m_bShadowsEnabled)
	{
		g_xEngine.Shadows().UpdateShadowMatrices();
	}

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
// Pure decision half of the FirstPresentSubmitted latch. Split out so the contract —
// the predicate is SNAPSHOTTED before EndFrame consumes the acquired image, and the
// latch is taken after it returns — is testable without a swapchain or a device.
static bool ShouldLatchFirstPresent(const bool bPresentWillSubmitSnapshot, const bool bAlreadyLatched)
{
	return bPresentWillSubmitSnapshot && !bAlreadyLatched;
}

static void EndFrameSubmitAndPresent(bool bSubmitRenderWork)
{
	Zenith_Engine& xEngine = g_xEngine;

	{
		Zenith_Profiling::ScopeZone xMemMgrProfile(ZENITH_PROFILE_ZONE("Flux Memory Manager"));
		xEngine.FluxMemory().SubmitFrameMemoryWork();
	}

	Zenith_MemoryManagement::EndFrame();

#if ZENITH_MEMORY_TRACKING_ANY
	// Feed the once-per-frame memory snapshot into the profiler's Memory tab/HUD/report.
	// SampleFrame() is a pure counter read; PushMemorySample skips itself while paused.
	xEngine.Profiling().PushMemorySample(Zenith_MemoryManagement::SampleFrame());
#endif

	// Boot milestone "FirstPresentSubmitted", latched below. The predicate is snapshotted
	// BEFORE EndFrame because EndFrame is what consumes the acquired image — asking
	// afterwards answers a different question. Backend variance goes entirely through
	// this facade call: the Null and D3D12 swapchains are no-op facades that return
	// false, and a Vulkan acquire-skip frame returns false too, so the milestone stays
	// N/A exactly when nothing was actually submitted for presentation. No backend TU
	// is touched. The name stays honest — submitted, not confirmed on screen.
	const bool bPresentWillSubmit = xEngine.FluxSwapchain().ShouldWaitOnImageAvailableSemaphore();

	{
		Zenith_Profiling::ScopeZone xEndFrameProfile(ZENITH_PROFILE_ZONE("Flux PlatformAPI End Frame"));
		xEngine.FluxBackend().EndFrame(bSubmitRenderWork);
	}

	// Once-latched at the CALL SITE, not inside the profiler: RecordBootMilestone is
	// first-wins but takes the control mutex to find that out, and this is the steady
	// -state frame path. Main-thread only, so a plain local static is sufficient.
	static bool ls_bFirstPresentLatched = false;
	if (ShouldLatchFirstPresent(bPresentWillSubmit, ls_bFirstPresentLatched))
	{
		ls_bFirstPresentLatched = true;
		xEngine.Profiling().RecordBootMilestone("FirstPresentSubmitted");
	}

	{
		Zenith_Profiling::ScopeZone xSwapchainEndFrameProfile(ZENITH_PROFILE_ZONE("Flux Swapchain End Frame"));
		xEngine.FluxSwapchain().EndFrame();
	}
}

void Zenith_Core::Zenith_MainLoop()
{
	// Reset the per-frame memory delta / alloc / free counters at frame start,
	// symmetric with Zenith_MemoryManagement::EndFrame() in EndFrameSubmitAndPresent.
	// (Was never wired before, so frame-delta stats grew unbounded.)
	Zenith_MemoryManagement::BeginFrame();

	BeginFrame_Platform();

	if (!AcquireSwapchainOrSkip())
	{
		// Resize-skip: end-of-frame cleanup ran inside the helper; the frame index
		// is deliberately NOT advanced (see AcquireSwapchainOrSkip). Input is NOT
		// drained either — the FIFO persists to the next real frame.
		return;
	}

	DrainInputAndPointers();

	bool bSubmitRenderWork      = true;
	bool bShouldUpdateGameLogic = true;
	UpdateEditorAndTuning(bSubmitRenderWork, bShouldUpdateGameLogic);
	PumpAutomatedTest(bShouldUpdateGameLogic);

	// Frame contract step 7: everything this frame's automated-test Steps
	// injected becomes ordered transitions on Zenith_Input before any game logic
	// runs, so simulated and real input reach the action layer through exactly
	// one path. Input PULLS from the simulator (rather than the simulator
	// pushing) so the test harness never reaches the engine singleton. No-op
	// while the simulator is disabled.
	g_xEngine.Input().ApplySimulatorInjection();
	// ...and into the pointer table, which already ran its step-4 pass before the
	// Steps existed. Injected touches append to the SAME staged stream, so this
	// consumes only the new tail; the mouse edges a Step raised reach pointer 0
	// through the same B3 projection real ones do. Driven from here rather than
	// from inside Zenith_Input because the device layer must not reach the engine
	// singleton — this is the composition root, and it already owns the ordering.
	g_xEngine.Pointers().ApplyInjection(g_xEngine.Input());

	// Frame contract step 8: the action layer's frame opener. Activity detection
	// over this frame's transitions decides the active profile (auto switch or a
	// forced override), and ANY mask change rebases every action from its
	// current source states before a single transition is replayed. It runs here
	// — after the last thing that can add a transition, before anything that
	// reads an action.
	//
	// Step 9 (per-context binding sync) is WP3b and belongs in this gap: after
	// the profile is settled, before the UI input phase closes any action.
	g_xEngine.Actions().UpdateProfile();

	// Frame contract step 10: the UI input phase. Runs BEFORE game logic (so a
	// widget consumes a press the same frame gameplay would otherwise see it) and
	// independently of the render/UI VISUAL pass below, which a scene transition
	// can skip — a skipped render frame must never change input state. It owns
	// steps 10b (close the reserved UI actions) and 10e (close everything else)
	// around its own capture walk, which is why the action layer is handed in.
	ZENITH_PROFILING_FUNCTION_WRAPPER(g_xEngine.UI().UpdateInput,
		ZENITH_PROFILE_ZONE("UI Input"), g_xEngine.Actions(), g_xEngine.Pointers(),
		g_xEngine.Frame().GetDt());

	UpdateGameLogic(bShouldUpdateGameLogic);
	SubmitRenderWork(bSubmitRenderWork);
	EndFrameSubmitAndPresent(bSubmitRenderWork);

	// End of frame: deferred-deletion countdown, then advance the engine frame
	// index. The advance happens AFTER Swapchain::EndFrame so the present uses the
	// slot for frame N before the ring index moves to N+1.
	Zenith_Engine& xEngine = g_xEngine;
	xEngine.FluxRenderer().ProcessFrameEnd();
	xEngine.Frame().AdvanceFrameIndex();

	// Boot milestone "FirstFrameCompleted". Placed HERE, in the shared main loop, so
	// Android is covered too — and after AdvanceFrameIndex so the resize-skip early
	// return above (which deliberately does NOT advance) cannot count as a completed
	// frame. Once-latched at the call site so the steady state costs one bool test.
	static bool ls_bFirstFrameLatched = false;
	if (!ls_bFirstFrameLatched)
	{
		ls_bFirstFrameLatched = true;
		xEngine.Profiling().RecordBootMilestone("FirstFrameCompleted");
	}
}

#ifdef ZENITH_TESTING
#include "Core/Zenith_UnitTests.Tests.inl"
#include "Core/Zenith_TerrainDimensions.Tests.inl"
#endif
