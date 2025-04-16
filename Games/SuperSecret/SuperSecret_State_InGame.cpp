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
#include "Components/PlayerController_Behaviour.h"

#define QUAD_SIZE 32
#define MAP_WIDTH 10
#define MAP_HEIGHT 10

Zenith_State* Zenith_StateMachine::s_pxCurrentState = new SuperSecret_State_InGame;

static Zenith_Entity s_xController;
static Zenith_Entity s_xPlayer0;

static Flux_Texture* g_pxTextures[SUPERSECRET_TEXTURE_INDEX__COUNT];

static SUPERSECRET_TEXTURE_INDICES s_aeMap[MAP_WIDTH * MAP_HEIGHT];

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
	Zenith_Assert(uX < MAP_WIDTH && uY < MAP_HEIGHT, "Won't fit on map");

	Flux_Quads::UploadQuad({
		{
			(QUAD_SIZE + uX * QUAD_SIZE * 2),
			(QUAD_SIZE + uY * QUAD_SIZE * 2),
			QUAD_SIZE * fSizeMult,
			QUAD_SIZE * fSizeMult
		},
		{1.,1.,1.,1.},
		static_cast<uint32_t>(eTexture) });
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
	const float fPitch = 0;
	const float fYaw = 0;
	const float fFOV = 45;
	const float fNear = 1;
	const float fFar = 5000;
	const float fAspectRatio = 16. / 9.;
	xCamera.InitialisePerspective(xPos, fPitch, fYaw, fFOV, fNear, fFar, fAspectRatio);
	xScene.SetMainCameraEntity(s_xController);

	s_xPlayer0.Initialise(&xScene, "Player0");
	Zenith_ScriptComponent& xScript = s_xPlayer0.AddComponent<Zenith_ScriptComponent>();
	xScript.SetBehaviour<PlayerController_Behaviour>();


	for (uint32_t u = 0; u < MAP_WIDTH * MAP_HEIGHT; u++)
	{
		s_aeMap[u] = SUPERSECRET_TEXTURE_INDEX__LONG_GRASS;
	}
}

void SuperSecret_State_InGame::OnUpdate()
{
	for (uint32_t u = 0; u < MAP_WIDTH * MAP_HEIGHT; u++)
	{
		UploadQuad(u % MAP_WIDTH, u / MAP_WIDTH, s_aeMap[u]);
	}
	const PlayerController_Behaviour& xPlayer = *(PlayerController_Behaviour*)s_xPlayer0.GetComponent<Zenith_ScriptComponent>().m_pxScriptBehaviour;
	UploadQuad(xPlayer.GetPosition().x, xPlayer.GetPosition().y, SUPERSECRET_TEXTURE_INDEX__PLAYER0, 2.0f);

	Zenith_Core::Zenith_MainLoop();
}

void SuperSecret_State_InGame::OnExit()
{
	Zenith_Scene::GetCurrentScene().Reset();
}