#include "Zenith.h"
#include "SuperSecret/SuperSecret_State_InGame.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_TextComponent.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Quads/Flux_Quads.h"
#include "Input/Zenith_Input.h"
#include "Components/CameraController_Behaviour.h"
#include "Components/PlayerController_Behaviour.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "DataStream/Zenith_DataStream.h"

#define QUAD_SIZE 256
static uint32_t g_uMapWidth = -1;
static uint32_t g_uMapHeight = -1;
static constexpr uint32_t g_uMaxMapWidth = 1024;
static constexpr uint32_t g_uMaxMapHeight = 1024;

Zenith_State* Zenith_StateMachine::s_pxCurrentState = new SuperSecret_State_InGame;

static Zenith_Entity s_xController;
static Zenith_Entity s_xPlayer0;

static Flux_Texture* g_pxTextures[SUPERSECRET_TEXTURE_INDEX__COUNT];

static Flux_Quads::Quad s_axMap[g_uMaxMapWidth * g_uMaxMapHeight];

static void LoadAssets()
{
	for (uint32_t u = 0; u < SUPERSECRET_TEXTURE_INDEX__COUNT; u++)
	{
		g_pxTextures[u] = &Zenith_AssetHandler::AddTexture2D(g_aszTextureNames[u], g_aszTextureFilenames[u]);
		Flux::RegisterBindlessTexture(g_pxTextures[u], u);
	}
}

void UploadQuad(const uint32_t uX, const uint32_t uY, const SUPERSECRET_TEXTURE_INDICES eTexture, const float fSizeMult = 1.0f)
{
	Zenith_Assert(uX < g_uMapWidth && uY < g_uMapHeight, "Won't fit on map");

	Flux_Quads::UploadQuad({
		{
			(QUAD_SIZE + uX * QUAD_SIZE * 2),
			(QUAD_SIZE + uY * QUAD_SIZE * 2) + (fSizeMult * QUAD_SIZE),
			QUAD_SIZE * fSizeMult,
			QUAD_SIZE * fSizeMult
		},
		{1.,1.,1.,1.},
		static_cast<uint32_t>(eTexture),
		{1,1}
		});
}

void SuperSecret_State_InGame::OnEnter()
{
	Flux_MemoryManager::BeginFrame();
	LoadAssets();
	Flux_MemoryManager::EndFrame(false);

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	s_xController.Initialise(&xScene, "Controller");
	Zenith_CameraComponent& xCamera = s_xController.AddComponent<Zenith_CameraComponent>();
	const Zenith_Maths::Vector3 xPos = { 0, 0, 0 };
	const float fPitch = 0.2f;
	const float fYaw = 0;
	const float fFOV = 45;
	const float fNear = 1;
	const float fFar = 5000;
	const float fAspectRatio = 256. / 192.;
	xCamera.InitialisePerspective(xPos, fPitch, fYaw, fFOV, fNear, fFar, fAspectRatio);
	xCamera.SetPosition({40, 30, -60});
	xScene.SetMainCameraEntity(s_xController);
	s_xController.AddComponent<Zenith_ScriptComponent>().SetBehaviour<CameraController_Behaviour>();


	s_xPlayer0.Initialise(&xScene, "Player0");
	s_xPlayer0.AddComponent<Zenith_ScriptComponent>().SetBehaviour<PlayerController_Behaviour>();

	uint64_t ulSize;
	char* pcData = Zenith_FileAccess::ReadFile(ASSETS_ROOT"Maps/map0.txt", ulSize);
	Zenith_DataStream xStream(pcData, ulSize);
	
	xStream >> g_uMapWidth;
	xStream >> g_uMapHeight;

	for (uint32_t u = 0; u < g_uMapWidth * g_uMapHeight; u++)
	{
		xStream >> s_axMap[u];

		Zenith_DebugVariables::AddUInt32({ "Map",std::to_string(u), "Texture"}, s_axMap[u].m_uTexture, 0, SUPERSECRET_TEXTURE_INDEX__COUNT - 1);
		Zenith_DebugVariables::AddUInt32({ "Map",std::to_string(u), "Pos X"}, s_axMap[u].m_xPosition_Size.x, 0, 8192);
		Zenith_DebugVariables::AddUInt32({ "Map",std::to_string(u), "Pos Y"}, s_axMap[u].m_xPosition_Size.y, 0, 8192);
		Zenith_DebugVariables::AddUInt32({ "Map",std::to_string(u), "Size X"}, s_axMap[u].m_xPosition_Size.z, 0, 512);
		Zenith_DebugVariables::AddUInt32({ "Map",std::to_string(u), "Size Y"}, s_axMap[u].m_xPosition_Size.w, 0, 512);
		Zenith_DebugVariables::AddFloat({ "Map",std::to_string(u), "UV Mult"}, s_axMap[u].m_xUVMult_UVAdd.x, 0, 2);
		Zenith_DebugVariables::AddFloat({ "Map",std::to_string(u), "UV Add" }, s_axMap[u].m_xUVMult_UVAdd.y, 0, 2);
	}
}

void SuperSecret_State_InGame::OnUpdate()
{
	for (uint32_t u = 0; u < g_uMapWidth * g_uMapHeight; u++)
	{
		Flux_Quads::UploadQuad(s_axMap[u]);
	}
	const PlayerController_Behaviour& xPlayer = *(PlayerController_Behaviour*)s_xPlayer0.GetComponent<Zenith_ScriptComponent>().m_pxScriptBehaviour;
	UploadQuad(xPlayer.GetPosition().x, xPlayer.GetPosition().y, SUPERSECRET_TEXTURE_INDEX__PLAYER0, 2.0f);

	if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_P))
	{
		Zenith_DataStream xStream(sizeof(s_axMap) + sizeof(uint32_t) * 2);

		xStream << g_uMapWidth;
		xStream << g_uMapHeight;

		for (uint32_t u = 0; u < g_uMapWidth * g_uMapHeight; u++)
		{
			xStream << s_axMap[u];
		}

		FILE* pxFile = fopen(ASSETS_ROOT"Maps/map0.txt", "wb");
		xStream.WriteToFile(pxFile);
		fclose(pxFile);
	}

	Zenith_Core::Zenith_MainLoop();
}

void SuperSecret_State_InGame::OnExit()
{
	Zenith_Scene::GetCurrentScene().Reset();
}