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
#include "DebugVariables/Zenith_DebugVariables.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Flux/Flux.h"
#include "Flux/Flux_RendererImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
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

#ifdef ZENITH_TOOLS

void TraverseTree(Zenith_DebugVariableTree::Node* pxNode, uint32_t uCurrentDepth)
{
	ImGui::PushID(pxNode);
	
	if (!ImGui::CollapsingHeader(pxNode->m_xName[uCurrentDepth].c_str()))
	{
		ImGui::PopID();
		return;
	}
	
	ImGui::Indent();
	
	for (Zenith_DebugVariableTree::LeafNodeBase* pxLeaf : pxNode->m_xLeaves)
	{
		pxLeaf->ImGuiDisplay();
	}
	for (Zenith_DebugVariableTree::Node* pxChild : pxNode->m_xChildren)
	{
		TraverseTree(pxChild, uCurrentDepth + 1);
	}
	
	ImGui::Unindent();
	ImGui::PopID();
}

void RenderImGui()
{
	g_xEngine.Vulkan().ImGuiBeginFrame();
	
	// Render the editor UI (includes docking, viewport, hierarchy, etc.)
	g_xEngine.Editor().Render();
	
	// Also render the old debug tools window for backwards compatibility
	ImGui::Begin("Zenith Tools");

	std::string strCamPosText = "Camera Position: " + std::to_string(static_cast<int32_t>(g_xEngine.FluxGraphics().m_xFrameConstants.m_xCamPos_Pad.x)) + " " + std::to_string(static_cast<int32_t>(g_xEngine.FluxGraphics().m_xFrameConstants.m_xCamPos_Pad.y)) + " " + std::to_string(static_cast<int32_t>(g_xEngine.FluxGraphics().m_xFrameConstants.m_xCamPos_Pad.z));
	ImGui::Text(strCamPosText.c_str());

	std::string strFpsText = "FPS: " + std::to_string(1.f / g_xEngine.Frame().GetDt());
	ImGui::Text(strFpsText.c_str());

	Zenith_DebugVariableTree& xTree = g_xEngine.DebugVariables().m_xTree;
	Zenith_DebugVariableTree::Node* pxRoot = xTree.m_pxRoot;
	TraverseTree(pxRoot, 0);

	ImGui::End();
	
	// Render profiling window. Manual begin/end (rather than the
	// FUNCTION_WRAPPER macro) because RenderToImGui is now a member
	// function and can't be passed as a free-function-style callable.
	{
		Zenith_Profiling::Scope xRenderProfileScope(ZENITH_PROFILE_INDEX__RENDER_IMGUI_PROFILING);
		g_xEngine.Profiling().RenderToImGui();
	}
	
	// Finalize ImGui rendering data - this MUST be called before submitting the render task
	ImGui::Render();
}

#endif

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
	// BeginFrame fires registered begin-frame callbacks (which includes the
	// Vulkan backend's wait-fence + reset-pools logic that used to live
	// behind Flux_PlatformAPI::BeginFrame). The PROFILE index name is kept
	// the same so the profiler timeline is comparable to pre-extraction
	// runs. BeginFrame is a NOP when no backend callbacks are registered
	// (i.e. in --headless where g_xEngine.FluxRenderer().EarlyInitialise
	// was skipped). Manual begin/end (rather than FUNCTION_WRAPPER macro)
	// because BeginFrame is now a member function and can't be passed as a
	// free-function-style callable to the macro.
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
		g_xEngine.VulkanMemory().BeginFrame();
		if (!Flux_Swapchain::BeginFrame())
		{
			g_xEngine.VulkanMemory().EndFrame(false);
			// Skipped frame still fires end-frame callbacks so the deferred VRAM
			// deletion clock ticks, but we deliberately DON'T advance the ring
			// counter — a rapid-resize sequence of consecutive skips would
			// otherwise wrap the counter past valid fences and shorten the
			// effective MAX_FRAMES_IN_FLIGHT+1 deferred-deletion grace period.
			g_xEngine.FluxRenderer().FireEndCallbacks();
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

		// Render UI components - submits to Flux_Quads and Flux_Text
		// Must happen before SubmitRenderTasks() which submits those systems
		// Collects from ALL loaded scenes (persistent entity UI + game scene UI).
		//
		// Two-pass: all Updates first (button clicks queue scene loads), then
		// the guard scope closes which drains any pending load, then we
		// re-collect components and Render. Without the split the click's
		// canvas would paint once more before the deferred load tears it
		// down — the "buttons persist for a frame" symptom.
		{
			Zenith_SceneUpdateDeferralGuard xUpdateGuard;
			Zenith_Vector<Zenith_UIComponent*> xUIComponents;
			g_xEngine.Scenes().GetAllOfComponentTypeFromAllScenes<Zenith_UIComponent>(xUIComponents);
			for (Zenith_Vector<Zenith_UIComponent*>::Iterator xIt(xUIComponents); !xIt.Done(); xIt.Next())
			{
				xIt.GetData()->Update(g_xEngine.Frame().GetDt());
			}
		}
		// Guard destructor above drained any deferred LoadScene — scene may
		// have swapped here. Re-collect post-drain so we render the new
		// scene's UI, not the destroyed one.
		{
			Zenith_Vector<Zenith_UIComponent*> xUIComponents;
			g_xEngine.Scenes().GetAllOfComponentTypeFromAllScenes<Zenith_UIComponent>(xUIComponents);
			for (Zenith_Vector<Zenith_UIComponent*>::Iterator xIt(xUIComponents); !xIt.Done(); xIt.Next())
			{
				xIt.GetData()->Render();
			}
		}

		// W22: ordering constraint documented on Flux_RenderGraph::Execute.
		#ifdef ZENITH_TOOLS
		ZENITH_PROFILING_FUNCTION_WRAPPER(RenderImGui, ZENITH_PROFILE_INDEX__RENDER_IMGUI);
		#endif

		// Render-phase boundary. Compiled in ALL configs (the signal is a real
		// atomic now, not assert-only): scene reads that happen on render
		// worker threads check AreRenderTasksActive() to know this window is open.
		g_xEngine.Scenes().SetRenderTasksActive(true);
		ExecuteRenderGraph();
		g_xEngine.Scenes().SetRenderTasksActive(false);
	}

	// EndFrame prepares memory command buffer for submission and processes deferred deletions.
	// Deferred deletions use a frame counter (MAX_FRAMES_IN_FLIGHT) to ensure GPU has finished
	// using resources before they are deleted. Manual scope (rather than
	// FUNCTION_WRAPPER macro) because EndFrame is now an instance method.
	if (!Zenith_CommandLine::IsHeadless())
	{
		Zenith_Profiling::Scope xMemMgrProfile(ZENITH_PROFILE_INDEX__FLUX_MEMORY_MANAGER);
		g_xEngine.VulkanMemory().EndFrame();
	}

	Zenith_MemoryManagement::EndFrame();

	// Vulkan EndFrame records render command buffers. Manual scope (rather
	// than FUNCTION_WRAPPER macro) because EndFrame is now an instance
	// method on Zenith_Vulkan and can't be passed as a callable.
	if (!Zenith_CommandLine::IsHeadless())
	{
		{
			Zenith_Profiling::Scope xEndFrameProfile(ZENITH_PROFILE_INDEX__FLUX_PLATFORMAPI_END_FRAME);
			g_xEngine.Vulkan().EndFrame(bSubmitRenderWork);
		}

		ZENITH_PROFILING_FUNCTION_WRAPPER(Flux_Swapchain::EndFrame, ZENITH_PROFILE_INDEX__FLUX_SWAPCHAIN_END_FRAME);
	}

	// Final action of the main loop: fires registered end-frame callbacks
	// (deferred-deletion countdown lives here now) and advances the
	// Flux_PerFrame counter. Counter advance happens AFTER Swapchain::EndFrame
	// so the present uses the slot for frame N before the ring index moves to
	// N+1 for the next iteration — matches the old in-swapchain bump.
	// Flux_PerFrame::EndFrame is a NOP when no backend callbacks are registered.
	// The ring counter advance is harmless in headless and keeps frame-counting
	// consistent for any downstream code that reads g_xEngine.FluxRenderer().GetFrameIndex().
	g_xEngine.FluxRenderer().EndFrame();
}

#ifdef ZENITH_TESTING
#include "Core/Zenith_UnitTests.Tests.inl"
#endif
