#include "Zenith.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"

static Zenith_Entity s_xGameController;

void Zenith_Core::Project_Startup()
{
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	s_xGameController.Initialise(&xScene, "Game Controller");
	s_xGameController.AddComponent<Zenith_CameraComponent>();
	xScene.SetMainCameraEntity(s_xGameController);
}