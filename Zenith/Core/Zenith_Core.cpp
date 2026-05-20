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
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_PerFrame.h"
#include "Flux/Fog/Flux_Fog.h"
#include "Flux/IBL/Flux_IBL.h"
#include "Flux/SSR/Flux_SSR.h"
#include "Flux/SSGI/Flux_SSGI.h"
#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"
#endif
#include "Input/Zenith_Input.h"
#include "Input/Zenith_TouchInput.h"
#include "Physics/Zenith_Physics.h"
#include "Physics/Zenith_PhysicsMeshGenerator.h"
#include "AssetHandling/Zenith_AsyncAssetLoader.h"


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
	Flux_PlatformAPI::ImGuiBeginFrame();
	
	// Render the editor UI (includes docking, viewport, hierarchy, etc.)
	Zenith_Editor::Render();
	
	// Also render the old debug tools window for backwards compatibility
	ImGui::Begin("Zenith Tools");

	std::string strCamPosText = "Camera Position: " + std::to_string(static_cast<int32_t>(Flux_Graphics::s_xFrameConstants.m_xCamPos_Pad.x)) + " " + std::to_string(static_cast<int32_t>(Flux_Graphics::s_xFrameConstants.m_xCamPos_Pad.y)) + " " + std::to_string(static_cast<int32_t>(Flux_Graphics::s_xFrameConstants.m_xCamPos_Pad.z));
	ImGui::Text(strCamPosText.c_str());

	std::string strFpsText = "FPS: " + std::to_string(1.f / g_xEngine.Frame().GetDt());
	ImGui::Text(strFpsText.c_str());

	Zenith_DebugVariableTree& xTree = Zenith_DebugVariables::s_xTree;
	Zenith_DebugVariableTree::Node* pxRoot = xTree.m_pxRoot;
	TraverseTree(pxRoot, 0);

	ImGui::End();
	
	// Render profiling window
	ZENITH_PROFILING_FUNCTION_WRAPPER(Zenith_Profiling::RenderToImGui, ZENITH_PROFILE_INDEX__RENDER_IMGUI_PROFILING);
	
	// Finalize ImGui rendering data - this MUST be called before submitting the render task
	ImGui::Render();
}

#endif

static void ExecuteRenderGraph()
{
	// Check if any subsystem requested a full graph rebuild
	if (Flux::ConsumeGraphRebuildRequest())
	{
		Flux::SetupRenderGraph();
	}

	Flux_RenderGraph& xGraph = Flux::GetRenderGraph();

	// Forward any debug-variable toggles that affect graph compilation (e.g.
	// transient aliasing) into the graph each frame. Cheap no-op when unchanged;
	// triggers MarkDirty on change so editor flips apply immediately instead of
	// waiting until the next SetupRenderGraph (which only runs on resize).
	Flux::SyncRenderGraphDebugToggles();

	// Order matters here. Both subsystems below run BEFORE Compile() so any
	// SetPassEnabled / MarkDirty mutations they perform take effect on the
	// same frame. Neither can live as a pass OnPrepare callback because
	// Phase 0 only fires OnPrepare for *enabled* passes — once a system has
	// disabled its previously-active pass, an OnPrepare-based switcher would
	// never run again.
	//
	// Fog must run BEFORE IBL so that if the user changes fog technique this
	// frame, ApplyTechniqueSelectionToGraph calls MarkDirty() *before* IBL's
	// UpdateGraphPassEnables checks IsDirty() — which lets IBL force-enable
	// all 49 of its passes for the upcoming full Compile() so the validator
	// sees a writer for every IBL texture that DeferredShading reads.
	Flux_Fog::ApplyTechniqueSelectionToGraph(xGraph);
	// SSR / SSGI runtime output toggles: when blur or denoise flip, these
	// enable/disable their post-pass and MarkDirty so the deferred-lighting
	// pass re-reads the correct handle (see Flux_SSR::GetReflectionHandle).
	// Must run BEFORE IBL's UpdateGraphPassEnables for the same MarkDirty
	// propagation reason described above.
	Flux_SSR::ApplyBlurSelectionToGraph(xGraph);
	Flux_SSGI::ApplyDenoiseSelectionToGraph(xGraph);
	Flux_IBL::UpdateGraphPassEnables(xGraph);

	xGraph.Compile();
	xGraph.Execute();
}

void Zenith_Core::Zenith_MainLoop()
{
	// Flux_PerFrame::BeginFrame fires registered begin-frame callbacks (which
	// includes the Vulkan backend's wait-fence + reset-pools logic that used
	// to live behind Flux_PlatformAPI::BeginFrame). The PROFILE index name is
	// kept the same so the profiler timeline is comparable to pre-extraction
	// runs.
	// Flux_PerFrame::BeginFrame is a NOP when no backend callbacks are
	// registered (i.e. in --headless where Flux::EarlyInitialise was skipped).
	ZENITH_PROFILING_FUNCTION_WRAPPER(Flux_PerFrame::BeginFrame, ZENITH_PROFILE_INDEX__FLUX_PLATFORMAPI_BEGIN_FRAME);

	UpdateTimers();
	Zenith_Input::BeginFrame();
	Zenith_Window::GetInstance()->BeginFrame();
	Zenith_TouchInput::Update();

	// Process async asset load callbacks on main thread
	Zenith_AsyncAssetLoader::ProcessCompletedLoads();

	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux_MemoryManager::BeginFrame();
		if (!Flux_Swapchain::BeginFrame())
		{
			Flux_MemoryManager::EndFrame(false);
			// Skipped frame still fires end-frame callbacks so the deferred VRAM
			// deletion clock ticks, but we deliberately DON'T advance the ring
			// counter — a rapid-resize sequence of consecutive skips would
			// otherwise wrap the counter past valid fences and shorten the
			// effective MAX_FRAMES_IN_FLIGHT+1 deferred-deletion grace period.
			Flux_PerFrame::FireEndCallbacks();
			return;
		}
	}

	bool bSubmitRenderWork = !Zenith_CommandLine::IsHeadless();
#ifdef ZENITH_TOOLS
	// CRITICAL: Update editor BEFORE any game logic or rendering
	// This is where deferred scene loads happen (from "Open Scene" menu)
	// Must occur when no render tasks are active to avoid concurrent access to scene data
	bool bEditorWantsRender = Zenith_Editor::Update();
	if (!Zenith_CommandLine::IsHeadless())
	{
		bSubmitRenderWork = bEditorWantsRender;
	}

	// Skip physics and scene updates when editor is paused or stopped
	// Only run game simulation when in Playing mode
	bool bShouldUpdateGameLogic = (Zenith_Editor::GetEditorMode() == EditorMode::Playing);
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
	bShouldUpdateGameLogic = (Zenith_Editor::GetEditorMode() == EditorMode::Playing);
	#endif
#endif

	if (bShouldUpdateGameLogic)
	{
		ZENITH_PROFILING_FUNCTION_WRAPPER(Zenith_Physics::Update, ZENITH_PROFILE_INDEX__PHYSICS, g_xEngine.Frame().GetDt());
		ZENITH_PROFILING_FUNCTION_WRAPPER(Zenith_SceneManager::Update, ZENITH_PROFILE_INDEX__SCENE_UPDATE, g_xEngine.Frame().GetDt());
	}

#ifdef ZENITH_INPUT_SIMULATOR
	// Tear down per-frame simulated input AFTER the scene/script update
	// has consumed it. Specifically clears the mouse-wheel delta — see
	// Zenith_InputSimulator::EndOfFrameTickComplete for the lifecycle.
	Zenith_InputSimulator::EndOfFrameTickComplete();
#endif

	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux_Graphics::UploadFrameConstants();
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
		if (Zenith_Editor::GetEditorMode() == EditorMode::Stopped)
		{
			Zenith_PhysicsMeshGenerator::QueuePhysicsDebugDraws();
		}
		#endif

		// Render UI components - submits to Flux_Quads and Flux_Text
		// Must happen before SubmitRenderTasks() which submits those systems
		// Collects from ALL loaded scenes (persistent entity UI + game scene UI)
		// Mark as updating so UI callbacks (e.g. button click -> LoadSceneByIndex)
		// defer scene loads instead of destroying scenes mid-iteration
		{
			Zenith_SceneManager::SceneUpdateDeferralGuard xUpdateGuard;
			Zenith_Vector<Zenith_UIComponent*> xUIComponents;
			Zenith_SceneManager::GetAllOfComponentTypeFromAllScenes<Zenith_UIComponent>(xUIComponents);
			for (Zenith_Vector<Zenith_UIComponent*>::Iterator xIt(xUIComponents); !xIt.Done(); xIt.Next())
			{
				Zenith_UIComponent* const pxUI = xIt.GetData();
				pxUI->Update(g_xEngine.Frame().GetDt());
				pxUI->Render();
			}
		}

		// W22: ordering constraint documented on Flux_RenderGraph::Execute.
		#ifdef ZENITH_TOOLS
		ZENITH_PROFILING_FUNCTION_WRAPPER(RenderImGui, ZENITH_PROFILE_INDEX__RENDER_IMGUI);
		#endif

		#ifdef ZENITH_ASSERT
		Zenith_SceneManager::SetRenderTasksActive(true);
		#endif
		ExecuteRenderGraph();
		#ifdef ZENITH_ASSERT
		Zenith_SceneManager::SetRenderTasksActive(false);
		#endif
	}

	// Only wait for scene update if we actually ran it
	if (bShouldUpdateGameLogic)
	{
		Zenith_SceneManager::WaitForUpdateComplete();
	}

	// EndFrame prepares memory command buffer for submission and processes deferred deletions
	// Deferred deletions use a frame counter (MAX_FRAMES_IN_FLIGHT) to ensure GPU has finished
	// using resources before they are deleted
	if (!Zenith_CommandLine::IsHeadless())
	{
		ZENITH_PROFILING_FUNCTION_WRAPPER(Flux_MemoryManager::EndFrame, ZENITH_PROFILE_INDEX__FLUX_MEMORY_MANAGER);
	}

	Zenith_MemoryManagement::EndFrame();

	// Flux_PlatformAPI::EndFrame records render command buffers
	if (!Zenith_CommandLine::IsHeadless())
	{
		ZENITH_PROFILING_FUNCTION_WRAPPER(Flux_PlatformAPI::EndFrame, ZENITH_PROFILE_INDEX__FLUX_PLATFORMAPI_END_FRAME, bSubmitRenderWork);

		ZENITH_PROFILING_FUNCTION_WRAPPER(Flux_Swapchain::EndFrame, ZENITH_PROFILE_INDEX__FLUX_SWAPCHAIN_END_FRAME);
	}

	// Final action of the main loop: fires registered end-frame callbacks
	// (deferred-deletion countdown lives here now) and advances the
	// Flux_PerFrame counter. Counter advance happens AFTER Swapchain::EndFrame
	// so the present uses the slot for frame N before the ring index moves to
	// N+1 for the next iteration — matches the old in-swapchain bump.
	// Flux_PerFrame::EndFrame is a NOP when no backend callbacks are registered.
	// The ring counter advance is harmless in headless and keeps frame-counting
	// consistent for any downstream code that reads Flux_PerFrame::GetFrameIndex().
	Flux_PerFrame::EndFrame();
}

#ifdef ZENITH_TESTING
#include "Core/Zenith_UnitTests.Tests.inl"
#endif
