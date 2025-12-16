#include "Zenith.h"

#include "Test/Components/SphereMovement_Behaviour.h"
#include "Test/Components/PlayerController_Behaviour.h"

void Project_RegisterScriptBehaviours()
{
	PlayerController_Behaviour::RegisterBehaviour();
	HookesLaw_Behaviour::RegisterBehaviour();
	RotationBehaviour_Behaviour::RegisterBehaviour();
}

void Project_LoadInitialScene()
{
	Zenith_Scene::GetCurrentScene().LoadFromFile(ASSETS_ROOT"Scenes/scene.zscen");
}