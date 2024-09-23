#include "Zenith.h"
#include "Test/Test_State_MainMenu.h"
#include "Test/Test_State_InGame.h"
#include "Input/Zenith_Input.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"

static Zenith_Entity s_xCamera;

void Test_State_MainMenu::OnEnter()
{
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	s_xCamera.Initialise(&xScene, "Game Controller");

	Zenith_CameraComponent& xCamera = s_xCamera.AddComponent<Zenith_CameraComponent>();
	const Zenith_Maths::Vector3 xPos = { 0, 0, 0 };
	const float fPitch = 0;
	const float fYaw = 0;
	const float fFOV = 45;
	const float fNear = 1;
	const float fFar = 5000;
	const float fAspectRatio = 16. / 9.;
	xCamera.InitialisePerspective(xPos, fPitch, fYaw, fFOV, fNear, fFar, fAspectRatio);
	xScene.SetMainCameraEntity(s_xCamera);
}

void Test_State_MainMenu::OnUpdate()
{
	Zenith_Core::Zenith_MainLoop();
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_O))
	{
		Zenith_StateMachine::RequestState(new Test_State_InGame);
	}
}

void Test_State_MainMenu::OnExit()
{
	Zenith_Scene::GetCurrentScene().Reset();
}
