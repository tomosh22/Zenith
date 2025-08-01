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
#include "Flux/Water/Flux_Water.h"
#include "Flux/Fog/Flux_Fog.h"
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

	ZENITH_PROFILING_FUNCTION_WRAPPER(Flux_Shadows::Render, ZENITH_PROFILE_INDEX__FLUX_SHADOWS);
	ZENITH_PROFILING_FUNCTION_WRAPPER(Flux_DeferredShading::BeginFrame, ZENITH_PROFILE_INDEX__FLUX_DEFERRED_SHADING);
	ZENITH_PROFILING_FUNCTION_WRAPPER(Flux_Skybox::Render, ZENITH_PROFILE_INDEX__FLUX_SKYBOX);
	ZENITH_PROFILING_FUNCTION_WRAPPER(Flux_StaticMeshes::RenderToGBuffer, ZENITH_PROFILE_INDEX__FLUX_STATIC_MESHES);
	ZENITH_PROFILING_FUNCTION_WRAPPER(Flux_AnimatedMeshes::Render, ZENITH_PROFILE_INDEX__FLUX_ANIMATED_MESHES);
	ZENITH_PROFILING_FUNCTION_WRAPPER(Flux_Terrain::RenderToGBuffer, ZENITH_PROFILE_INDEX__FLUX_TERRAIN);
	ZENITH_PROFILING_FUNCTION_WRAPPER(Flux_DeferredShading::Render, ZENITH_PROFILE_INDEX__FLUX_DEFERRED_SHADING);
	ZENITH_PROFILING_FUNCTION_WRAPPER(Flux_Water::Render, ZENITH_PROFILE_INDEX__FLUX_WATER);
	ZENITH_PROFILING_FUNCTION_WRAPPER(Flux_Fog::Render, ZENITH_PROFILE_INDEX__FLUX_FOG);
	ZENITH_PROFILING_FUNCTION_WRAPPER(Flux_SDFs::Render, ZENITH_PROFILE_INDEX__FLUX_SDFS);
	ZENITH_PROFILING_FUNCTION_WRAPPER(Flux_Particles::Render, ZENITH_PROFILE_INDEX__FLUX_PFX);
	ZENITH_PROFILING_FUNCTION_WRAPPER(Flux_Text::Render, ZENITH_PROFILE_INDEX__FLUX_TEXT);

	Flux_MemoryManager::EndFrame();

	Zenith_MemoryManagement::EndFrame();

#ifdef ZENITH_TOOLS
	Zenith_Profiling::EndFrame();
	RenderImGui();
	Zenith_Profiling::RenderToImGui();
#endif
	Flux_Swapchain::CopyToFramebuffer();
	Zenith_Scene::WaitForUpdateComplete();
	Flux_PlatformAPI::EndFrame();
	Flux_Swapchain::EndFrame();
}
