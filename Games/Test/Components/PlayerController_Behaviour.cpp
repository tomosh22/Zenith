#include "Zenith.h"

#include "Test/Components/PlayerController_Behaviour.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UI.h"
#include "Input/Zenith_Input.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "EntityComponent/Zenith_Entity.h"
#include "Flux/Flux_Graphics.h"
#include "Prefab/Zenith_Prefab.h"

DEBUGVAR float dbg_fCamDistance = 25.f;

static Zenith_Entity s_axBulletEntities[128];
static u_int s_uCurrentBulletIndex = 0;

PlayerController_Behaviour::PlayerController_Behaviour(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
	Zenith_Assert(m_xParentEntity.HasComponent<Zenith_ColliderComponent>(), "");

#ifdef ZENITH_DEBUG_VARIABLES
	static bool ls_bInit = false;
	if (!ls_bInit)
	{
		ls_bInit = true;
		Zenith_DebugVariables::AddFloat({ "PlayerController", "Camera Distance" }, dbg_fCamDistance, 0, 50);
	}
#endif
}

static void UpdateCameraRotation(Zenith_CameraComponent& xCamera)
{
	static Zenith_Maths::Vector2_64 s_xPreviousMousePos = { FLT_MAX,FLT_MAX };
	Zenith_Maths::Vector2_64 xCurrentMousePos;
	Zenith_Input::GetMousePosition(xCurrentMousePos);
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_Q))
	{
		if (s_xPreviousMousePos.x == FLT_MAX)
		{
			s_xPreviousMousePos = xCurrentMousePos;
			return;
		}

		//#TO_TODO: if cursor was not released this frame
		if (true)
		{
			double dDeltaPitch = (xCurrentMousePos.y - s_xPreviousMousePos.y) / 1000.;
			xCamera.SetPitch(xCamera.GetPitch() - dDeltaPitch);
			double dDeltaYaw = (xCurrentMousePos.x - s_xPreviousMousePos.x) / 1000.;
			xCamera.SetYaw(xCamera.GetYaw() - dDeltaYaw);

			xCamera.SetPitch(std::min(xCamera.GetPitch(), glm::pi<double>() / 2));
			xCamera.SetPitch(std::max(xCamera.GetPitch(), -glm::pi<double>() / 2));

			if (xCamera.GetYaw() < 0)
			{
				xCamera.SetYaw(xCamera.GetYaw() + Zenith_Maths::Pi * 2.0);
			}
			if (xCamera.GetYaw() > Zenith_Maths::Pi * 2.0)
			{
				xCamera.SetYaw(xCamera.GetYaw() - Zenith_Maths::Pi * 2.0);
			}
		}
	}
	s_xPreviousMousePos = xCurrentMousePos;
}

void PlayerController_Behaviour::Shoot()
{
	if (!m_pxBulletPrefab)
	{
		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[PlayerController] Bullet prefab not loaded!");
		return;
	}

	Zenith_Entity& xBulletEntity = s_axBulletEntities[s_uCurrentBulletIndex];
	xBulletEntity.Initialise(&Zenith_Scene::GetCurrentScene(), "Bullet" + std::to_string(s_uCurrentBulletIndex));
	s_uCurrentBulletIndex = (s_uCurrentBulletIndex + 1) % 128;

	m_pxBulletPrefab->ApplyToEntity(xBulletEntity);

	Zenith_CameraComponent& xCamera = m_xParentEntity.GetComponent<Zenith_CameraComponent>();
	Zenith_Maths::Vector3 xFacingDir;
	xCamera.GetFacingDir(xFacingDir);

	Zenith_TransformComponent& xTrans = xBulletEntity.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xPlayerPos;
	m_xParentEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xPlayerPos);

	xTrans.SetPosition(xPlayerPos + Zenith_Maths::Vector3(0, 7, 0) + xFacingDir * 10.0f);
	xTrans.SetScale({ 1, 1, 1 });

	Zenith_ColliderComponent& xCollider = xBulletEntity.GetComponent<Zenith_ColliderComponent>();
	Zenith_Physics::SetLinearVelocity(xCollider.GetBodyID(), xFacingDir * 50.0f);
}

void PlayerController_Behaviour::OnUpdate(const float fDt)
{
	Zenith_TransformComponent& xTrans = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
	Zenith_CameraComponent& xCamera = m_xParentEntity.GetComponent<Zenith_CameraComponent>();

	UpdateCameraRotation(xCamera);

	//#TO i don't think i need to multiply by fDt? physics update should handle frame rate inconsistencies right?
	const double dMoveSpeed = s_dMoveSpeed;

	if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_C))
	{
		m_bFlyCamEnabled = !m_bFlyCamEnabled;
	}

	if (m_bFlyCamEnabled)
	{
		Zenith_Maths::Vector3 xFinalVelocity(0,0,0);

		if (Zenith_Input::IsKeyDown(ZENITH_KEY_W))
		{
			Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));
			Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(0, 0, 1, 1) * dMoveSpeed;
			xFinalVelocity += Zenith_Maths::Vector3(xResult);
		}
		if (Zenith_Input::IsKeyDown(ZENITH_KEY_S))
		{
			Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));
			Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(0, 0, 1, 1) * dMoveSpeed;
			xFinalVelocity -= Zenith_Maths::Vector3(xResult);
		}
		if (Zenith_Input::IsKeyDown(ZENITH_KEY_A))
		{
			Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));
			Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(-1, 0, 0, 1) * dMoveSpeed;
			xFinalVelocity += Zenith_Maths::Vector3(xResult);
		}
		if (Zenith_Input::IsKeyDown(ZENITH_KEY_D))
		{
			Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));
			Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(-1, 0, 0, 1) * dMoveSpeed;
			xFinalVelocity -= Zenith_Maths::Vector3(xResult);
		}
		if (Zenith_Input::IsKeyDown(ZENITH_KEY_LEFT_SHIFT))
		{
			Zenith_Maths::Vector3_64 xUp = xCamera.GetUpDir();

			xFinalVelocity -= xUp * dMoveSpeed;
		}
		if (Zenith_Input::IsKeyDown(ZENITH_KEY_SPACE))
		{
			Zenith_Maths::Vector3_64 xUp = xCamera.GetUpDir();

			xFinalVelocity += xUp * dMoveSpeed;
		}

		Zenith_Maths::Vector3 xPos;
		xCamera.GetPosition(xPos);
		xFinalVelocity *= Zenith_Maths::Vector3(Zenith_Core::GetDt());
		xPos += xFinalVelocity;
		xCamera.SetPosition(xPos);

		return;
	}

	

	Zenith_ColliderComponent& xCollider = m_xParentEntity.GetComponent<Zenith_ColliderComponent>();
	Zenith_Maths::Vector3 xFinalVelocity(0, Zenith_Physics::GetLinearVelocity(xCollider.GetBodyID()).y, 0);

	if (Zenith_Input::IsKeyDown(ZENITH_KEY_W))
	{
		Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));
		Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(0, 0, 1, 1) * dMoveSpeed;
		xFinalVelocity += Zenith_Maths::Vector3(xResult);
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_S))
	{
		Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));
		Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(0, 0, 1, 1) * dMoveSpeed;
		xFinalVelocity -= Zenith_Maths::Vector3(xResult);
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_A))
	{
		Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));
		Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(-1, 0, 0, 1) * dMoveSpeed;
		xFinalVelocity += Zenith_Maths::Vector3(xResult);
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_D))
	{
		Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));
		Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(-1, 0, 0, 1) * dMoveSpeed;
		xFinalVelocity -= Zenith_Maths::Vector3(xResult);
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_LEFT_SHIFT))
	{
		xFinalVelocity.y = -dMoveSpeed / 10;
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_SPACE))
	{
		xFinalVelocity.y = dMoveSpeed / 10;
	}

	Zenith_Physics::SetLinearVelocity(xCollider.GetBodyID(), xFinalVelocity);
	xTrans.SetRotation(Zenith_Maths::EulerRotationToMatrix4(-xCamera.GetYaw() * Zenith_Maths::RadToDeg, { 0,1,0 }));

	Zenith_Maths::Vector3 xOffsetXZ = Zenith_Maths::Vector3(sinf(xCamera.GetYaw()), 0, -cosf(xCamera.GetYaw())) * dbg_fCamDistance;
	Zenith_Maths::Vector3 xPos;
	xTrans.GetPosition(xPos);
	xCamera.SetPosition(xPos + Zenith_Maths::Vector3(0, 20, 0) + xOffsetXZ);

	if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_E))
	{
		Shoot();
	}

	// Inventory slot selection (keys 1-6)
	for (int i = 0; i < s_iInventorySlots; i++)
	{
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_1 + i))
		{
			SetSelectedSlot(i);
		}
	}

	// Update compass based on camera yaw
	UpdateCompassUI();

	// Demo: take damage when pressing T, heal when pressing H
	if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_T))
	{
		TakeDamage(10.0f);
	}
	if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_H))
	{
		Heal(15.0f);
	}
}

// ========== UI Integration ==========

void PlayerController_Behaviour::OnAwake()
{
	FindHUDElements();
	return;
	if (!m_pxBulletPrefab && !m_strBulletPrefabPath.empty())
	{
		m_pxBulletPrefab = new Zenith_Prefab();
		if (!m_pxBulletPrefab->LoadFromFile(m_strBulletPrefabPath))
		{
			Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[PlayerController] Failed to load bullet prefab: %s", m_strBulletPrefabPath.c_str());
			delete m_pxBulletPrefab;
			m_pxBulletPrefab = nullptr;
		}
	}
}

void PlayerController_Behaviour::SetBulletPrefabPath(const std::string& strPath)
{
	m_strBulletPrefabPath = strPath;

	// Reload the prefab with the new path
	if (m_pxBulletPrefab)
	{
		delete m_pxBulletPrefab;
		m_pxBulletPrefab = nullptr;
	}

	if (!strPath.empty())
	{
		m_pxBulletPrefab = new Zenith_Prefab();
		if (!m_pxBulletPrefab->LoadFromFile(strPath))
		{
			Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[PlayerController] Failed to load bullet prefab: %s", strPath.c_str());
			delete m_pxBulletPrefab;
			m_pxBulletPrefab = nullptr;
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[PlayerController] Loaded bullet prefab: %s", strPath.c_str());
		}
	}
}

void PlayerController_Behaviour::FindHUDElements()
{
	if (m_bUIInitialized)
		return;

	// Find the HUD entity by name
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	Zenith_Entity xHUDEntity = xScene.FindEntityByName("HUD");
	if (!xHUDEntity.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[PlayerController] Could not find HUD entity");
		return;
	}

	if (!xHUDEntity.HasComponent<Zenith_UIComponent>())
	{
		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[PlayerController] HUD entity has no UIComponent");
		return;
	}

	Zenith_UIComponent& xUI = xHUDEntity.GetComponent<Zenith_UIComponent>();

	// Find health bar fill
	m_pxHealthFill = xUI.FindElement<Zenith_UI::Zenith_UIRect>("HealthBar_Fill");

	// Find compass text
	m_pxCompassText = xUI.FindElement<Zenith_UI::Zenith_UIText>("CompassText");

	// Find inventory slots
	for (int i = 0; i < s_iInventorySlots; i++)
	{
		std::string strSlotName = "InventorySlot_" + std::to_string(i) + "_BG";
		m_apxInventorySlots[i] = xUI.FindElement<Zenith_UI::Zenith_UIRect>(strSlotName);
	}

	m_bUIInitialized = true;
	Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[PlayerController] UI elements initialized successfully");

	// Initial UI update
	UpdateHealthUI();
	UpdateInventoryUI();
}

void PlayerController_Behaviour::UpdateHealthUI()
{
	if (!m_bUIInitialized)
		FindHUDElements();

	if (m_pxHealthFill)
	{
		float fHealthPercent = m_fHealth / s_fMaxHealth;
		m_pxHealthFill->SetFillAmount(fHealthPercent);

		// Change color based on health level
		if (fHealthPercent > 0.6f)
		{
			m_pxHealthFill->SetColor({ 0.2f, 0.8f, 0.2f, 1.0f }); // Green
		}
		else if (fHealthPercent > 0.3f)
		{
			m_pxHealthFill->SetColor({ 0.9f, 0.7f, 0.1f, 1.0f }); // Yellow
		}
		else
		{
			m_pxHealthFill->SetColor({ 0.9f, 0.1f, 0.1f, 1.0f }); // Red
		}
	}
}

void PlayerController_Behaviour::UpdateCompassUI()
{
	if (!m_bUIInitialized)
		FindHUDElements();

	if (!m_pxCompassText)
		return;

	// Get camera yaw to determine facing direction
	if (!m_xParentEntity.HasComponent<Zenith_CameraComponent>())
		return;

	Zenith_CameraComponent& xCamera = m_xParentEntity.GetComponent<Zenith_CameraComponent>();
	double dYaw = xCamera.GetYaw();

	// Normalize yaw to [0, 2*PI)
	while (dYaw < 0) dYaw += Zenith_Maths::Pi * 2.0;
	while (dYaw >= Zenith_Maths::Pi * 2.0) dYaw -= Zenith_Maths::Pi * 2.0;

	// Convert to degrees
	double dDegrees = dYaw * Zenith_Maths::RadToDeg;

	// Determine cardinal direction
	std::string strDirection;
	if (dDegrees >= 337.5 || dDegrees < 22.5)
		strDirection = "N";
	else if (dDegrees >= 22.5 && dDegrees < 67.5)
		strDirection = "NE";
	else if (dDegrees >= 67.5 && dDegrees < 112.5)
		strDirection = "E";
	else if (dDegrees >= 112.5 && dDegrees < 157.5)
		strDirection = "SE";
	else if (dDegrees >= 157.5 && dDegrees < 202.5)
		strDirection = "S";
	else if (dDegrees >= 202.5 && dDegrees < 247.5)
		strDirection = "SW";
	else if (dDegrees >= 247.5 && dDegrees < 292.5)
		strDirection = "W";
	else
		strDirection = "NW";

	m_pxCompassText->SetText(strDirection);
}

void PlayerController_Behaviour::UpdateInventoryUI()
{
	if (!m_bUIInitialized)
		FindHUDElements();

	for (int i = 0; i < s_iInventorySlots; i++)
	{
		if (m_apxInventorySlots[i])
		{
			if (i == m_iSelectedSlot)
			{
				m_apxInventorySlots[i]->SetGlowEnabled(true);
				m_apxInventorySlots[i]->SetGlowColor({ 1.0f, 0.8f, 0.2f, 0.6f });
			}
			else
			{
				m_apxInventorySlots[i]->SetGlowEnabled(false);
			}
		}
	}
}

void PlayerController_Behaviour::SetHealth(float fHealth)
{
	m_fHealth = std::max(0.0f, std::min(fHealth, s_fMaxHealth));
	UpdateHealthUI();
}

void PlayerController_Behaviour::TakeDamage(float fDamage)
{
	SetHealth(m_fHealth - fDamage);
}

void PlayerController_Behaviour::Heal(float fAmount)
{
	SetHealth(m_fHealth + fAmount);
}

void PlayerController_Behaviour::SetSelectedSlot(int iSlot)
{
	if (iSlot >= 0 && iSlot < s_iInventorySlots)
	{
		m_iSelectedSlot = iSlot;
		UpdateInventoryUI();
	}
}