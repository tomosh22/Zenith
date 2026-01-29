#include "Zenith.h"
#include "Zenith_Core.h"

#include "DebugVariables/Zenith_DebugVariables.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Skybox/Flux_Skybox.h"
#include "Flux/StaticMeshes/Flux_StaticMeshes.h"
#include "Flux/AnimatedMeshes/Flux_AnimatedMeshes.h"
#include "Flux/Terrain/Flux_Terrain.h"
#include "Flux/Primitives/Flux_Primitives.h"
#include "Flux/DeferredShading/Flux_DeferredShading.h"
#include "Flux/SSAO/Flux_SSAO.h"
#include "Flux/Fog/Flux_Fog.h"
#include "Flux/Quads/Flux_Quads.h"
#include "Flux/SDFs/Flux_SDFs.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "Flux/Particles/Flux_Particles.h"
#include "Flux/Text/Flux_Text.h"
#include "Flux/ComputeTest/Flux_ComputeTest.h"
#include "Flux/InstancedMeshes/Flux_InstancedMeshes.h"
#include "Flux/HDR/Flux_HDR.h"
#include "Flux/IBL/Flux_IBL.h"
#include "Flux/HiZ/Flux_HiZ.h"
#include "Flux/SSR/Flux_SSR.h"
#include "Flux/Vegetation/Flux_Grass.h"
#ifdef ZENITH_TOOLS
#include "Flux/Gizmos/Flux_Gizmos.h"
#include "Editor/Zenith_Editor.h"
#endif
#include "Input/Zenith_Input.h"
#include "Physics/Zenith_Physics.h"
#include "Physics/Zenith_PhysicsMeshGenerator.h"
#include "Profiling/Zenith_Profiling.h"
#include "AssetHandling/Zenith_AsyncAssetLoader.h"
#include "Zenith_OS_Include.h"

// Namespace variable definitions
float Zenith_Core::g_fDt = 0.f;
float Zenith_Core::g_fTimePassed = 0.f;
std::chrono::high_resolution_clock::time_point Zenith_Core::g_xLastFrameTime;



void Zenith_Core::UpdateTimers()
{
	std::chrono::high_resolution_clock::time_point xCurrentTime = std::chrono::high_resolution_clock::now();

	Zenith_Core::SetDt(std::chrono::duration_cast<std::chrono::nanoseconds>(xCurrentTime - g_xLastFrameTime).count() / 1.e9);
	g_xLastFrameTime = xCurrentTime;

	Zenith_Core::AddTimePassed(Zenith_Core::GetDt());
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

	std::string strFpsText = "FPS: " + std::to_string(1.f / Zenith_Core::GetDt());
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

static void SubmitRenderTasks()
{
	Flux_ComputeTest::Run();
	Flux_IBL::SubmitRenderTask();         // IBL BRDF LUT generation (early, used by deferred shading)
	Flux_Shadows::SubmitRenderTask();
	Flux_Skybox::SubmitRenderTask();      // Cubemap skybox + procedural atmosphere
	Flux_Skybox::SubmitAerialPerspectiveTask();  // Aerial perspective (if atmosphere enabled)
	Flux_StaticMeshes::SubmitRenderToGBufferTask();
	Flux_AnimatedMeshes::SubmitRenderTask();
	Flux_InstancedMeshes::SubmitCullingTask();
	Flux_InstancedMeshes::SubmitRenderTask();
	Flux_Terrain::SubmitRenderToGBufferTask();
	Flux_Grass::SubmitRenderTask();       // Grass/vegetation (after terrain)
	Flux_Primitives::SubmitRenderTask();
	Flux_HiZ::SubmitRenderTask();         // Hi-Z depth pyramid (after G-Buffer, needed by SSR)
	Flux_SSR::SubmitRenderTask();         // Screen-space reflections (uses Hi-Z, needed by deferred shading)
	Flux_DeferredShading::SubmitRenderTask();
	Flux_SSAO::SubmitRenderTask();
	Flux_Fog::SubmitRenderTask();
	Flux_SDFs::SubmitRenderTask();
	Flux_Particles::SubmitRenderTask();
	Flux_HDR::SubmitRenderTask();         // Tone mapping (must be after all HDR scene passes)
	Flux_Text::SubmitRenderTask();
	Flux_Quads::SubmitRenderTask();

	#ifdef ZENITH_TOOLS
	//#TO calls Flux_Gizmos::SubmitRenderTask()
	ZENITH_PROFILING_FUNCTION_WRAPPER(RenderImGui, ZENITH_PROFILE_INDEX__RENDER_IMGUI);
	#endif
}

// Public wrapper for WaitForRenderTasks
// Used by editor to synchronize before scene transitions
// bIncludeGizmos: If false, skips waiting for gizmo task (useful when called mid-frame before gizmo submission)
void Zenith_Core::WaitForAllRenderTasks()
{
	Flux_IBL::WaitForRenderTask();
	Flux_Shadows::WaitForRenderTask();
	Flux_Skybox::WaitForRenderTask();
	Flux_Skybox::WaitForAerialPerspectiveTask();
	Flux_StaticMeshes::WaitForRenderToGBufferTask();
	Flux_AnimatedMeshes::WaitForRenderTask();
	Flux_InstancedMeshes::WaitForCullingTask();
	Flux_InstancedMeshes::WaitForRenderTask();
	Flux_Terrain::WaitForRenderToGBufferTask();
	Flux_Grass::WaitForRenderTask();
	Flux_Primitives::WaitForRenderTask();
	Flux_HiZ::WaitForRenderTask();
	Flux_SSR::WaitForRenderTask();
	Flux_DeferredShading::WaitForRenderTask();
	Flux_SSAO::WaitForRenderTask();
	Flux_Fog::WaitForRenderTask();
	Flux_SDFs::WaitForRenderTask();
	Flux_Particles::WaitForRenderTask();
	Flux_HDR::WaitForRenderTask();
	Flux_Text::WaitForRenderTask();
	Flux_Quads::WaitForRenderTask();
	#ifdef ZENITH_TOOLS
	Flux_Gizmos::WaitForRenderTask();
	#endif
}

void Zenith_Core::Zenith_MainLoop()
{
	ZENITH_PROFILING_FUNCTION_WRAPPER(Flux_PlatformAPI::BeginFrame, ZENITH_PROFILE_INDEX__FLUX_PLATFORMAPI_BEGIN_FRAME);

	UpdateTimers();
	Zenith_Input::BeginFrame();
	Zenith_Window::GetInstance()->BeginFrame();

	// Process async asset load callbacks on main thread
	Zenith_AsyncAssetLoader::ProcessCompletedLoads();

	Flux_MemoryManager::BeginFrame();
	if (!Flux_Swapchain::BeginFrame())
	{
		Flux_MemoryManager::EndFrame(false);
		return;
	}

	bool bSubmitRenderWork = true;
#ifdef ZENITH_TOOLS
	// CRITICAL: Update editor BEFORE any game logic or rendering
	// This is where deferred scene loads happen (from "Open Scene" menu)
	// Must occur when no render tasks are active to avoid concurrent access to scene data
	bSubmitRenderWork = Zenith_Editor::Update();

	// Skip physics and scene updates when editor is paused or stopped
	// Only run game simulation when in Playing mode
	bool bShouldUpdateGameLogic = (Zenith_Editor::GetEditorMode() == EditorMode::Playing);
#else
	bool bShouldUpdateGameLogic = true;
#endif

	if (bShouldUpdateGameLogic)
	{
		ZENITH_PROFILING_FUNCTION_WRAPPER(Zenith_Physics::Update, ZENITH_PROFILE_INDEX__PHYSICS, Zenith_Core::GetDt());
		ZENITH_PROFILING_FUNCTION_WRAPPER(Zenith_Scene::Update, ZENITH_PROFILE_INDEX__SCENE_UPDATE, Zenith_Core::GetDt());
	}
	Flux_Graphics::UploadFrameConstants();

	// Only submit render tasks if we're going to process them
	// During scene transitions, bSubmitRenderWork is false and we skip rendering entirely
	// to avoid building command lists with potentially incomplete scene state
	if (bSubmitRenderWork)
	{
		// Queue physics mesh debug visualization for rendering (independent of game logic)
		// This allows viewing physics meshes in editor even when paused/stopped
		#ifdef ZENITH_TOOLS
		Zenith_PhysicsMeshGenerator::DebugDrawAllPhysicsMeshes();
		#endif

		// Render UI components - submits to Flux_Quads and Flux_Text
		// Must happen before SubmitRenderTasks() which submits those systems
		Zenith_Vector<Zenith_UIComponent*> xUIComponents;
		Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_UIComponent>(xUIComponents);
		for (Zenith_Vector<Zenith_UIComponent*>::Iterator xIt(xUIComponents); !xIt.Done(); xIt.Next())
		{
			Zenith_UIComponent* const pxUI = xIt.GetData();
			pxUI->Render();
		}

		SubmitRenderTasks();
		WaitForAllRenderTasks();
	}

	// Only wait for scene update if we actually ran it
	if (bShouldUpdateGameLogic)
	{
		Zenith_Scene::WaitForUpdateComplete();
	}

	// EndFrame prepares memory command buffer for submission and processes deferred deletions
	// Deferred deletions use a frame counter (MAX_FRAMES_IN_FLIGHT) to ensure GPU has finished
	// using resources before they are deleted
	ZENITH_PROFILING_FUNCTION_WRAPPER(Flux_MemoryManager::EndFrame, ZENITH_PROFILE_INDEX__FLUX_MEMORY_MANAGER);

	Zenith_MemoryManagement::EndFrame();

	// Flux_PlatformAPI::EndFrame records render command buffers
	ZENITH_PROFILING_FUNCTION_WRAPPER(Flux_PlatformAPI::EndFrame, ZENITH_PROFILE_INDEX__FLUX_PLATFORMAPI_END_FRAME, bSubmitRenderWork);

	ZENITH_PROFILING_FUNCTION_WRAPPER(Flux_Swapchain::EndFrame, ZENITH_PROFILE_INDEX__FLUX_SWAPCHAIN_END_FRAME);
}
