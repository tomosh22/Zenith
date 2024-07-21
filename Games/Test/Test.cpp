#include "Zenith.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "AssetHandling/Zenith_AssetHandler.h"

static Zenith_Entity s_xGameController;

void Zenith_Core::Project_Startup()
{
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	s_xGameController.Initialise(&xScene, "Game Controller");
	Zenith_ScriptComponent& xScript = s_xGameController.AddComponent<Zenith_ScriptComponent>();
	xScript.SetBehaviour<Zenith_CameraBehaviour>();
	Zenith_CameraBehaviour& xCamera = *(Zenith_CameraBehaviour*)xScript.m_pxScriptBehaviour;
	const Zenith_Maths::Vector3 xPos = { 0, 0, 0 };
	const float fPitch = 0;
	const float fYaw = 0;
	const float fFOV = 45;
	const float fNear = 1;
	const float fFar = 10000;
	const float fAspectRatio = 16./9.;
	xCamera.InitialisePerspective(xPos, fPitch, fYaw, fFOV, fNear, fFar, fAspectRatio);
	xScene.SetMainCameraEntity(s_xGameController);

	Zenith_AssetHandler::AddMesh(Zenith_GUID(), "Sphere_Smooth", "C:/dev/Zenith/Games/Test/Assets/Meshes/sphereSmooth.zmsh");
	Zenith_AssetHandler::AddTexture(Zenith_GUID(), "Crystal_Diffuse", "C:/dev/Zenith/Games/Test/Assets/Textures/crystal2k/diffuse.ztx");
}
