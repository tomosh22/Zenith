#include "Zenith.h"

#include "Zenith_OS_Include.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Skybox/Flux_Skybox.h"
#include "Flux/StaticMeshes/Flux_StaticMeshes.h"
#include "Flux/Terrain/Flux_Terrain.h"
#include "Flux/DeferredShading/Flux_DeferredShading.h"
#include "EntityComponent/Zenith_Scene.h"
#include "Physics/Zenith_Physics.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

static std::chrono::high_resolution_clock::time_point s_xLastFrameTime;

static void UpdateTimers()
{
	std::chrono::high_resolution_clock::time_point xCurrentTime = std::chrono::high_resolution_clock::now();

	Zenith_Core::SetDt(std::chrono::duration_cast<std::chrono::nanoseconds>(xCurrentTime - s_xLastFrameTime).count() / 1.e9);
	s_xLastFrameTime = xCurrentTime;

	Zenith_Core::AddTimePassed(Zenith_Core::GetDt());
}

#ifdef ZENITH_TOOLS

void TraverseTree(Zenith_DebugVariableTree::Node* pxNode)
{
	std::string strName;
	for (const std::string& strHeader : pxNode->m_xName)
	{
		strName += strHeader;
	}
	if (!ImGui::CollapsingHeader(strName.c_str()))
	{
		return;
	}
	for (Zenith_DebugVariableTree::LeafNodeBase* pxLeaf : pxNode->m_xLeaves)
	{
		pxLeaf->ImGuiDisplay();
	}
	for (Zenith_DebugVariableTree::Node* pxChild : pxNode->m_xChildren)
	{
		TraverseTree(pxChild);
	}
}

void RenderImGui()
{
	Flux_PlatformAPI::ImGuiBeginFrame();
	ImGui::Begin("Zenith Tools");

	bool bTest0 = false;
	bool bTest1 = false;

	Zenith_DebugVariableTree& xTree = Zenith_DebugVariables::s_xTree;
	Zenith_DebugVariableTree::Node* pxRoot = xTree.m_pxRoot;
	TraverseTree(pxRoot);

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

int main()
{
#if 0//def ZENITH_TOOLS
	extern void ExportAllMeshes();
	extern void ExportAllTextures();
	extern void ExportHeightmap();
	ExportAllMeshes();
	ExportAllTextures();
	ExportHeightmap();
#else
	s_xLastFrameTime = std::chrono::high_resolution_clock::now();
	Zenith_Window::Inititalise("Zenith", 1280, 720);
	Flux::EarlyInitialise();
	Zenith_Physics::Initialise();
	Zenith_Core::Project_Startup();
	Flux::LateInitialise();

	Zenith_DebugVariables::DebugBoolean({ "Root", "AAA", "BBB", "Test0" }, s_bDVSTest0);
	Zenith_DebugVariables::DebugBoolean({ "Root", "AAA", "BBB", "Test1" }, s_bDVSTest1);
	Zenith_DebugVariables::DebugBoolean({ "Root", "AAA", "CCC", "Test2" }, s_bDVSTest2);
	Zenith_DebugVariables::DebugBoolean({ "Root", "Test3" }, s_bDVSTest3);
	
	while (true)
	{
		Zenith_MainLoop();
	}
#endif //ZENITH_TOOLS
	__debugbreak();
}
