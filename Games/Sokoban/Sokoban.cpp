#include "Zenith.h"

#include "Sokoban/Components/Sokoban_Behaviour.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"

const char* Project_GetName()
{
	return "Sokoban";
}

const char* Project_GetGameAssetsDirectory()
{
	return GAME_ASSETS_DIR;
}

void Project_RegisterScriptBehaviours()
{
	Sokoban_Behaviour::RegisterBehaviour();
}

void Project_LoadInitialScene()
{
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	xScene.Reset();

	Zenith_Entity xCameraEntity(&xScene, "MainCamera");
	Zenith_CameraComponent& xCamera = xCameraEntity.AddComponent<Zenith_CameraComponent>();
	xCamera.InitialisePerspective(
		Zenith_Maths::Vector3(0.f, 0.f, -10.f),
		0.f,
		0.f,
		45.f,
		0.1f,
		1000.f,
		16.f / 9.f
	);
	xScene.SetMainCameraEntity(xCameraEntity.GetEntityID());

	Zenith_Entity xSokobanEntity(&xScene, "SokobanGame");
	xSokobanEntity.AddComponent<Zenith_ScriptComponent>().SetBehaviour<Sokoban_Behaviour>();
}
