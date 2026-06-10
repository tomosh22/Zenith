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
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"
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
#include "Editor/Zenith_Editor.h"
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

void Zenith_Core::Zenith_MainLoop()
{
	// BeginFrame issues the Vulkan backend's wait-fence + reset-pools per-frame
	// begin work (PerFrameBegin), called directly through the neutral
	// Flux_PlatformAPI alias. The PROFILE index name is kept the same so the
	// profiler timeline is comparable to pre-extraction runs. BeginFrame is a
	// NOP in --headless (the backend is never initialised). Manual scope
	// (rather than FUNCTION_WRAPPER macro) because BeginFrame is a member
	// function and can't be passed as a free-function-style callable.
	{
		Zenith_Profiling::Scope xBeginFrameProfile(ZENITH_PROFILE_INDEX__FLUX_PLATFORMAPI_BEGIN_FRAME);
		g_xEngine.FluxRenderer().BeginFrame();
	}

	UpdateTimers();
	g_xEngine.Input().BeginFrame();
	Zenith_Window::GetInstance()->BeginFrame();
	g_xEngine.Touch().Update();

	if (!Zenith_CommandLine::IsHeadless())
	{
		if (!g_xEngine.FluxSwapchain().BeginFrame())
		{
			// Drain any memory work staged before the failed acquire.
			g_xEngine.FluxMemory().Flush();
			// Skipped frame still runs end-of-frame work so the deferred VRAM
			// deletion clock ticks, but we deliberately DON'T advance the frame
			// index (early return skips the AdvanceFrameIndex at the bottom of
			// the loop) — a rapid-resize sequence of consecutive skips would
			// otherwise wrap the ring index past valid fences and shorten the
			// effective MAX_FRAMES_IN_FLIGHT+1 deferred-deletion grace period.
			g_xEngine.FluxRenderer().ProcessFrameEnd();
			return;
		}
	}

	bool bSubmitRenderWork = !Zenith_CommandLine::IsHeadless();
#ifdef ZENITH_TOOLS
	// CRITICAL: Update editor BEFORE any game logic or rendering
	// This is where deferred scene loads happen (from "Open Scene" menu)
	// Must occur when no render tasks are active to avoid concurrent access to scene data
	bool bEditorWantsRender = g_xEngine.Editor().Update();
	if (!Zenith_CommandLine::IsHeadless())
	{
		bSubmitRenderWork = bEditorWantsRender;
	}

	// Skip physics and scene updates when editor is paused or stopped
	// Only run game simulation when in Playing mode
	bool bShouldUpdateGameLogic = (g_xEngine.Editor().GetEditorMode() == EditorMode::Playing);
#else
	bool bShouldUpdateGameLogic = true;
#endif

#ifdef ZENITH_INPUT_SIMULATOR
	// EXT-3a: pump the automated-test state machine. Runs AFTER editor update
	// so HarnessPhase::WaitForAutomationComplete observes the automation queue
	// drain in the same frame, and BEFORE physics/scene update so a phase
	// transition into Playing takes effect on the *next* frame's update — that
	// gives OnAwake time to fire and lets us observe a clean OnStart on the
	// flush-first-frame iteration.
	Zenith_AutomatedTestRunner::Tick();
	// Re-read after Tick() in case it switched the editor into Playing mode.
	#ifdef ZENITH_TOOLS
	bShouldUpdateGameLogic = (g_xEngine.Editor().GetEditorMode() == EditorMode::Playing);
	#endif
#endif

	if (bShouldUpdateGameLogic)
	{
		ZENITH_PROFILING_FUNCTION_WRAPPER(g_xEngine.Physics().Update, ZENITH_PROFILE_INDEX__PHYSICS, g_xEngine.Frame().GetDt());
		ZENITH_PROFILING_FUNCTION_WRAPPER(g_xEngine.Scenes().Update, ZENITH_PROFILE_INDEX__SCENE_UPDATE, g_xEngine.Frame().GetDt());
	}

#ifdef ZENITH_INPUT_SIMULATOR
	// Tear down per-frame simulated input AFTER the scene/script update
	// has consumed it. Specifically clears the mouse-wheel delta — see
	// Zenith_InputSimulator::EndOfFrameTickComplete for the lifecycle.
	Zenith_InputSimulator::EndOfFrameTickComplete();
#endif

	if (!Zenith_CommandLine::IsHeadless())
	{
		g_xEngine.FluxGraphics().UploadFrameConstants();
	}

	// Only submit render tasks if we're going to process them
	// During scene transitions, bSubmitRenderWork is false and we skip rendering entirely
	// to avoid building command lists with potentially incomplete scene state
	// In --headless, bSubmitRenderWork is false (set initially on line ~186)
	// regardless of editor state, so this whole block is skipped.
	if (bSubmitRenderWork)
	{
		// Queue physics debug visualization only while the editor is stopped,
		// so play mode doesn't flood the primitives pass.
		#ifdef ZENITH_TOOLS
		if (g_xEngine.Editor().GetEditorMode() == EditorMode::Stopped)
		{
			Zenith_PhysicsMeshGenerator::QueuePhysicsDebugDraws();
		}
		#endif

		// UI component frame (update + quad/text submission to Flux_Quads /
		// Flux_Text). Must happen before ExecuteRenderGraph() below, which
		// consumes those submissions. The two-pass structure (and the
		// deferred-LoadScene drain between the passes) lives inside
		// Zenith_UISystem::Update.
		ZENITH_PROFILING_FUNCTION_WRAPPER(g_xEngine.UI().Update, ZENITH_PROFILE_INDEX__UI_UPDATE, g_xEngine.Frame().GetDt());

		// W22: ordering constraint documented on Flux_RenderGraph::Execute.
		#ifdef ZENITH_TOOLS
		ZENITH_PROFILING_FUNCTION_WRAPPER(g_xEngine.Editor().RenderImGuiFrame, ZENITH_PROFILE_INDEX__RENDER_IMGUI);
		#endif

		// Render-phase boundary. Compiled in ALL configs (the signal is a real
		// atomic now, not assert-only): scene reads that happen on render
		// worker threads check AreRenderTasksActive() to know this window is open.
		g_xEngine.Scenes().SetRenderTasksActive(true);
		ExecuteRenderGraph();
		g_xEngine.Scenes().SetRenderTasksActive(false);
	}

	// Hand this frame's lazily-recorded memory work to the backend; it is
	// submitted ahead of the render command buffers against the memory
	// semaphore in Zenith_Vulkan::EndFrame. No memory operation may run
	// between here and that submit.
	if (!Zenith_CommandLine::IsHeadless())
	{
		Zenith_Profiling::Scope xMemMgrProfile(ZENITH_PROFILE_INDEX__FLUX_MEMORY_MANAGER);
		g_xEngine.FluxMemory().SubmitFrameMemoryWork();
	}

	Zenith_MemoryManagement::EndFrame();

	// Vulkan EndFrame records render command buffers. Manual scope (rather
	// than FUNCTION_WRAPPER macro) because EndFrame is now an instance
	// method on Zenith_Vulkan and can't be passed as a callable.
	if (!Zenith_CommandLine::IsHeadless())
	{
		{
			Zenith_Profiling::Scope xEndFrameProfile(ZENITH_PROFILE_INDEX__FLUX_PLATFORMAPI_END_FRAME);
			g_xEngine.FluxBackend().EndFrame(bSubmitRenderWork);
		}

		{
			// Manual scope (rather than FUNCTION_WRAPPER macro) because EndFrame
			// is now an instance method on the swapchain and can't be passed as a
			// callable -- mirrors the Zenith_Vulkan::EndFrame conversion above.
			Zenith_Profiling::Scope xSwapchainEndFrameProfile(ZENITH_PROFILE_INDEX__FLUX_SWAPCHAIN_END_FRAME);
			g_xEngine.FluxSwapchain().EndFrame();
		}
	}

	// Final action of the main loop: run end-of-frame work (the deferred-
	// deletion countdown, ProcessDeferredDeletions), then advance the engine
	// frame index. Core owns the frame clock — FrameContext holds the single
	// frame-index variable engine-wide. The advance happens AFTER
	// Swapchain::EndFrame so the present uses the slot for frame N before the
	// ring index moves to N+1 for the next iteration. The deferred-deletion
	// call is a NOP in headless (no memory manager); the advance is harmless in
	// headless and keeps frame-counting consistent for any downstream code
	// that reads g_xEngine.Frame().GetFrameIndex().
	g_xEngine.FluxRenderer().ProcessFrameEnd();
	g_xEngine.Frame().AdvanceFrameIndex();
}

#ifdef ZENITH_TESTING
#include "Core/Zenith_UnitTests.Tests.inl"
#endif
