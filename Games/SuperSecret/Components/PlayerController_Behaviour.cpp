#include "Zenith.h"

#include "SuperSecret/Components/PlayerController_Behaviour.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Input/Zenith_Input.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "EntityComponent/Zenith_Entity.h"

static Zenith_Entity s_axBulletEntities[128];
static u_int s_uCurrentBulletIndex = 0;

PlayerController_Behaviour::PlayerController_Behaviour(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
	, m_xPosition({0,0})
{

}


void PlayerController_Behaviour::OnUpdate(const float fDt)
{
	if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_UP))
	{
		m_xPosition.y++;
	}
	if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_DOWN))
	{
		m_xPosition.y--;
	}
	if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_LEFT))
	{
		m_xPosition.x--;
	}
	if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_RIGHT))
	{
		m_xPosition.x++;
	}
}