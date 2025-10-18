#include "Zenith.h"
#include "Zenith_Core.h"

#include "DebugVariables/Zenith_DebugVariables.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Skybox/Flux_Skybox.h"
#include "Flux/StaticMeshes/Flux_StaticMeshes.h"
#include "Flux/AnimatedMeshes/Flux_AnimatedMeshes.h"
#include "Flux/Terrain/Flux_Terrain.h"
#include "Flux/DeferredShading/Flux_DeferredShading.h"
#include "Flux/SSAO/Flux_SSAO.h"
#include "Flux/Fog/Flux_Fog.h"
#include "Flux/Quads/Flux_Quads.h"
#include "Flux/SDFs/Flux_SDFs.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "Flux/Particles/Flux_Particles.h"
#include "Flux/Text/Flux_Text.h"
#include "Input/Zenith_Input.h"
#include "Physics/Zenith_Physics.h"
#include "Profiling/Zenith_Profiling.h"
#include "Zenith_OS_Include.h"

float Zenith_Core::s_fDt = 0.f;
float Zenith_Core::s_fTimePassed = 0.f;
std::chrono::high_resolution_clock::time_point Zenith_Core::s_xLastFrameTime;



void Zenith_Core::UpdateTimers()
{
	std::chrono::high_resolution_clock::time_point xCurrentTime = std::chrono::high_resolution_clock::now();

	Zenith_Core::SetDt(std::chrono::duration_cast<std::chrono::nanoseconds>(xCurrentTime - s_xLastFrameTime).count() / 1.e9);
	s_xLastFrameTime = xCurrentTime;

	Zenith_Core::AddTimePassed(Zenith_Core::GetDt());
}

#ifdef ZENITH_TOOLS

void TraverseTree(Zenith_DebugVariableTree::Node* pxNode, uint32_t uCurrentDepth)
{
	if (!ImGui::CollapsingHeader(pxNode->m_xName[uCurrentDepth].c_str()))
	{
		return;
	}
	ImGui::Indent(uCurrentDepth * 20);
	for (Zenith_DebugVariableTree::LeafNodeBase* pxLeaf : pxNode->m_xLeaves)
	{
		pxLeaf->ImGuiDisplay();
	}
	for (Zenith_DebugVariableTree::Node* pxChild : pxNode->m_xChildren)
	{
		TraverseTree(pxChild, uCurrentDepth + 1);
	}
	ImGui::Unindent(uCurrentDepth * 20);
}

void RenderImGui()
{
	Flux_PlatformAPI::ImGuiBeginFrame();
	ImGui::Begin("Zenith Tools");

	const Zenith_CameraComponent& xCamera = Zenith_Scene::GetCurrentScene().GetMainCamera();

	Zenith_Maths::Vector3 xCamPos;
	xCamera.GetPosition(xCamPos);
	std::string strCamPosText = "Camera Position: " + std::to_string(static_cast<int32_t>(xCamPos.x)) + " " + std::to_string(static_cast<int32_t>(xCamPos.y)) + " " + std::to_string(static_cast<int32_t>(xCamPos.z));

	Zenith_Maths::Vector3 xFacingDir;
	xCamera.GetFacingDir(xFacingDir);
	std::string strCamDirText = "Camera Facing Dir: " + std::to_string(xFacingDir.x) + " " + std::to_string(xFacingDir.y) + " " + std::to_string(xFacingDir.z);

	ImGui::Text(strCamPosText.c_str());
	ImGui::Text(strCamDirText.c_str());

	std::string strFpsText = "FPS: " + std::to_string(1.f / Zenith_Core::GetDt());
	ImGui::Text(strFpsText.c_str());

	Zenith_DebugVariableTree& xTree = Zenith_DebugVariables::s_xTree;
	Zenith_DebugVariableTree::Node* pxRoot = xTree.m_pxRoot;
	TraverseTree(pxRoot, 0);

	ImGui::End();
}
#endif

static void SubmitRenderTasks()
{
	Flux_Shadows::SubmitRenderTask();
	Flux_Skybox::SubmitRenderTask();
	Flux_StaticMeshes::SubmitRenderToGBufferTask();
	Flux_AnimatedMeshes::SubmitRenderTask();
	Flux_Terrain::SubmitRenderToGBufferTask();
	Flux_DeferredShading::SubmitRenderTask();
	Flux_SSAO::SubmitRenderTask();
	Flux_Fog::SubmitRenderTask();
	Flux_SDFs::SubmitRenderTask();
	Flux_Particles::SubmitRenderTask();
	Flux_Text::SubmitRenderTask();
	Flux_Quads::SubmitRenderTask();
}

static void WaitForRenderTasks()
{
	Flux_Shadows::WaitForRenderTask();
	Flux_Skybox::WaitForRenderTask();
	Flux_StaticMeshes::WaitForRenderToGBufferTask();
	Flux_AnimatedMeshes::WaitForRenderTask();
	Flux_Terrain::WaitForRenderToGBufferTask();
	Flux_DeferredShading::WaitForRenderTask();
	Flux_SSAO::WaitForRenderTask();
	Flux_Fog::WaitForRenderTask();
	Flux_SDFs::WaitForRenderTask();
	Flux_Particles::WaitForRenderTask();
	Flux_Text::WaitForRenderTask();
	Flux_Quads::WaitForRenderTask();
}

void Zenith_Core::Zenith_MainLoop()
{
	Flux_PlatformAPI::BeginFrame();
	Zenith_Profiling::BeginFrame();
	UpdateTimers();
	Zenith_Input::BeginFrame();
	Zenith_Window::GetInstance()->BeginFrame();

	Flux_MemoryManager::BeginFrame();
	if (!Flux_Swapchain::BeginFrame())
	{
		Flux_MemoryManager::EndFrame(false);
		return;
	}
	
	ZENITH_PROFILING_FUNCTION_WRAPPER(Zenith_Physics::Update, ZENITH_PROFILE_INDEX__PHYSICS, Zenith_Core::GetDt());
	ZENITH_PROFILING_FUNCTION_WRAPPER(Zenith_Scene::Update, ZENITH_PROFILE_INDEX__SCENE_UPDATE, Zenith_Core::GetDt());
	Flux_Graphics::UploadFrameConstants();

	SubmitRenderTasks();

	WaitForRenderTasks();
	Zenith_Scene::WaitForUpdateComplete();

	Flux_MemoryManager::EndFrame();

	Zenith_MemoryManagement::EndFrame();

	//#TO_TODO: profiling currently doesn't include Flux_PlatformAPI/Flux_Swapchain EndFrame
	Zenith_Profiling::EndFrame();
	#ifdef ZENITH_TOOLS
	RenderImGui();
	Zenith_Profiling::RenderToImGui();
	#endif

	Flux_PlatformAPI::EndFrame();
	Flux_Swapchain::EndFrame();
}
