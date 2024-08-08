#include "Zenith.h"

#include "Zenith_OS_Include.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Skybox/Flux_Skybox.h"
#include "Flux/StaticMeshes/Flux_StaticMeshes.h"
#include "Flux/Terrain/Flux_Terrain.h"
#include "Flux/DeferredShading/Flux_DeferredShading.h"
#include "Flux/Water/Flux_Water.h"
#include "Flux/Fog/Flux_Fog.h"
#include "EntityComponent/Zenith_Scene.h"
#include "Physics/Zenith_Physics.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif
#include "EntityComponent/Components/Zenith_CameraComponent.h"

static std::chrono::high_resolution_clock::time_point s_xLastFrameTime;

static void UpdateTimers()
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
	ImGui::Indent(uCurrentDepth * 10);
	for (Zenith_DebugVariableTree::LeafNodeBase* pxLeaf : pxNode->m_xLeaves)
	{
		pxLeaf->ImGuiDisplay();
	}
	for (Zenith_DebugVariableTree::Node* pxChild : pxNode->m_xChildren)
	{
		TraverseTree(pxChild, uCurrentDepth + 1);
	}
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


	Zenith_DebugVariableTree& xTree = Zenith_DebugVariables::s_xTree;
	Zenith_DebugVariableTree::Node* pxRoot = xTree.m_pxRoot;
	TraverseTree(pxRoot, 0);

	ImGui::End();
}
#endif

void Zenith_MainLoop()
{
	UpdateTimers();
	Zenith_Window::GetInstance()->BeginFrame();
	Flux_MemoryManager::BeginFrame();
	if (!Flux_Swapchain::BeginFrame())
	{
		Flux_MemoryManager::EndFrame(false);
		return;
	}
	Flux_PlatformAPI::BeginFrame();

	Zenith_Physics::Update(Zenith_Core::GetDt());
	Zenith_Scene::GetCurrentScene().Update(Zenith_Core::GetDt());
	Flux_Graphics::UploadFrameConstants();
	Flux_Skybox::Render();
	Flux_StaticMeshes::Render();
	Flux_Terrain::Render();
	Flux_DeferredShading::Render();
	Flux_Water::Render();
	Flux_Fog::Render();

	Flux_MemoryManager::EndFrame();
#ifdef ZENITH_TOOLS
	RenderImGui();
#endif
	Flux_Swapchain::CopyToFramebuffer();
	Flux_PlatformAPI::EndFrame();
	Flux_Swapchain::EndFrame();
}

static bool s_bDVSTest0 = false;
static bool s_bDVSTest1 = false;
static bool s_bDVSTest2 = false;
static bool s_bDVSTest3 = false;
static Zenith_Maths::Vector3 s_xDVSTest4 = { 1,2,3 };

int main()
{
	s_xLastFrameTime = std::chrono::high_resolution_clock::now();
	Zenith_Window::Inititalise("Zenith", 1280, 720);
	Flux::EarlyInitialise();
	Zenith_Physics::Initialise();
	Zenith_Core::Project_Startup();
	Flux::LateInitialise();

#if defined ZENITH_TOOLS && defined ZENITH_DEBUG_VARIABLES

	extern void ExportAllMeshes();
	extern void ExportAllTextures();
	extern void ExportHeightmap();

	Zenith_DebugVariables::AddButton({ "Export", "Meshes", "Export All Meshes" }, ExportAllMeshes);
	Zenith_DebugVariables::AddButton({ "Export", "Textures", "Export All Textures" }, ExportAllTextures);
	Zenith_DebugVariables::AddButton({ "Export", "Terrain", "Export Heightmap" }, ExportHeightmap);
#endif
	
	while (true)
	{
		Zenith_MainLoop();
	}
	__debugbreak();
}
