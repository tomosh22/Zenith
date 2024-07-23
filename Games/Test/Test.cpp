#include "Zenith.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_CameraBehaviour.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "Flux/Flux_Material.h"
#include "Flux/Flux_Graphics.h"

#include "Test/Components/SphereMovement_Behaviour.h"

static Zenith_Entity s_xGameController;
static Zenith_Entity s_xSphere0;
static Zenith_Entity s_xSphere1;

static Flux_Material s_xCrystalMaterial;
static Flux_Material s_xRockMaterial;

void LoadAssets()
{
	Zenith_AssetHandler::AddMesh(Zenith_GUID(), "Sphere_Smooth", "C:/dev/Zenith/Games/Test/Assets/Meshes/sphereSmooth.zmsh");

	{
		Zenith_AssetHandler::AddTexture(Zenith_GUID(), "Crystal_Diffuse", "C:/dev/Zenith/Games/Test/Assets/Textures/crystal2k/diffuse.ztx");
		Zenith_AssetHandler::AddTexture(Zenith_GUID(), "Crystal_Normal", "C:/dev/Zenith/Games/Test/Assets/Textures/crystal2k/normal.ztx");
		Zenith_AssetHandler::AddTexture(Zenith_GUID(), "Crystal_Roughness", "C:/dev/Zenith/Games/Test/Assets/Textures/crystal2k/roughness.ztx");
		Zenith_AssetHandler::AddTexture(Zenith_GUID(), "Crystal_Metallic", "C:/dev/Zenith/Games/Test/Assets/Textures/crystal2k/metallic.ztx");

		Flux_Texture& xDiffuse = Zenith_AssetHandler::GetTexture("Crystal_Diffuse");
		Flux_Texture& xNormal = Zenith_AssetHandler::GetTexture("Crystal_Normal");
		Flux_Texture& xRoughness = Zenith_AssetHandler::GetTexture("Crystal_Roughness");
		Flux_Texture& xMetallic = Zenith_AssetHandler::GetTexture("Crystal_Metallic");

		s_xCrystalMaterial.SetDiffuse(&xDiffuse);
		s_xCrystalMaterial.SetNormal(&xNormal);
		s_xCrystalMaterial.SetRoughness(&xRoughness);
		s_xCrystalMaterial.SetMetallic(&xMetallic);
	}

	{
		Zenith_AssetHandler::AddTexture(Zenith_GUID(), "Rock_Diffuse", "C:/dev/Zenith/Games/Test/Assets/Textures/rock2k/diffuse.ztx");
		Zenith_AssetHandler::AddTexture(Zenith_GUID(), "Rock_Normal", "C:/dev/Zenith/Games/Test/Assets/Textures/rock2k/normal.ztx");
		Zenith_AssetHandler::AddTexture(Zenith_GUID(), "Rock_Roughness", "C:/dev/Zenith/Games/Test/Assets/Textures/rock2k/roughness.ztx");
		Zenith_AssetHandler::AddTexture(Zenith_GUID(), "Rock_Metallic", "C:/dev/Zenith/Games/Test/Assets/Textures/rock2k/metallic.ztx");

		Flux_Texture& xDiffuse = Zenith_AssetHandler::GetTexture("Rock_Diffuse");
		Flux_Texture& xNormal = Zenith_AssetHandler::GetTexture("Rock_Normal");
		Flux_Texture& xRoughness = Zenith_AssetHandler::GetTexture("Rock_Roughness");
		Flux_Texture& xMetallic = Zenith_AssetHandler::GetTexture("Rock_Metallic");

		s_xRockMaterial.SetDiffuse(&xDiffuse);
		s_xRockMaterial.SetNormal(&xNormal);
		s_xRockMaterial.SetRoughness(&xRoughness);
		s_xRockMaterial.SetMetallic(&xMetallic);
	}

}

void Zenith_Core::Project_Startup()
{
	LoadAssets();

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

	Flux_MeshGeometry& xSphereMesh = Zenith_AssetHandler::GetMesh("Sphere_Smooth");

	{
		s_xSphere0.Initialise(&xScene, "Sphere0");
		s_xSphere0.AddComponent<Zenith_ModelComponent>(xSphereMesh, s_xCrystalMaterial);
		Zenith_TransformComponent& xTrans = s_xSphere0.GetComponent<Zenith_TransformComponent>();
		xTrans.SetScale({ 10,10,10 });

		Zenith_ScriptComponent& xScript = s_xSphere0.AddComponent<Zenith_ScriptComponent>();
		xScript.SetBehaviour<SphereMovement_Behaviour>();
		SphereMovement_Behaviour& xBehaviour = *(SphereMovement_Behaviour*)xScript.m_pxScriptBehaviour;
		xBehaviour.SetInitialPosition({ 10.,0,10. });
		xBehaviour.SetAmplitude(10.);
	}
	{
		s_xSphere1.Initialise(&xScene, "Sphere1");
		s_xSphere1.AddComponent<Zenith_ModelComponent>(xSphereMesh, s_xRockMaterial);
		Zenith_TransformComponent& xTrans = s_xSphere1.GetComponent<Zenith_TransformComponent>();
		xTrans.SetScale({ 10,10,10 });

		Zenith_ScriptComponent& xScript = s_xSphere1.AddComponent<Zenith_ScriptComponent>();
		xScript.SetBehaviour<SphereMovement_Behaviour>();
		SphereMovement_Behaviour& xBehaviour = *(SphereMovement_Behaviour*)xScript.m_pxScriptBehaviour;
		xBehaviour.SetInitialPosition({-10.,0,-10.});
		xBehaviour.SetAmplitude(-10.);
	}
}
