#include "Zenith.h"

#include "SuperSecret/Components/PlayerController_Behaviour.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Input/Zenith_Input.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "EntityComponent/Zenith_Entity.h"

DEBUGVAR float dbg_fCamDistance = 25.f;

static Zenith_Entity s_axBulletEntities[128];
static u_int s_uCurrentBulletIndex = 0;

PlayerController_Behaviour::PlayerController_Behaviour(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
	, m_xPosition({0,0})
{
#ifdef ZENITH_DEBUG_VARIABLES
	static bool ls_bInit = false;
	if (!ls_bInit)
	{
		ls_bInit = true;
		Zenith_DebugVariables::AddFloat({ "PlayerController", "Camera Distance" }, dbg_fCamDistance, 0, 50);
	}
#endif
}


void PlayerController_Behaviour::OnUpdate(const float fDt)
{
	if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_W))
	{
		m_xPosition.y++;
	}
	if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_S))
	{
		m_xPosition.y--;
	}
	if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_A))
	{
		m_xPosition.x--;
	}
	if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_D))
	{
		m_xPosition.x++;
	}
}