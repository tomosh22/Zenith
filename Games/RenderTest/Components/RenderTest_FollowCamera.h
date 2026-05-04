#pragma once

#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Maths/Zenith_Maths.h"

class RenderTest_FollowCamera : public Zenith_ScriptBehaviour
{
public:
	RenderTest_FollowCamera(Zenith_Entity& xEntity)
		: Zenith_ScriptBehaviour()
	{
		m_xParentEntity = xEntity;
	}

	ZENITH_BEHAVIOUR_TYPE_NAME(RenderTest_FollowCamera)

	void OnStart() override
	{
		ResolvePlayer();
	}

	void OnLateUpdate(float fDt) override
	{
		if (!m_xParentEntity.HasComponent<Zenith_CameraComponent>())
			return;

		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneDataForEntity(m_xParentEntity.GetEntityID());
		if (!pxSceneData)
			return;

		if (m_uPlayerEntityID == INVALID_ENTITY_ID || !pxSceneData->EntityExists(m_uPlayerEntityID))
		{
			ResolvePlayer();
			if (m_uPlayerEntityID == INVALID_ENTITY_ID)
				return;
		}

		Zenith_Entity xPlayer = pxSceneData->GetEntity(m_uPlayerEntityID);
		if (!xPlayer.IsValid() || !xPlayer.HasComponent<Zenith_TransformComponent>())
			return;

		Zenith_Maths::Vector3 xPlayerPos;
		xPlayer.GetComponent<Zenith_TransformComponent>().GetPosition(xPlayerPos);

		const Zenith_Maths::Vector3 xOffset(0.0f, 12.0f, -15.0f);
		const Zenith_Maths::Vector3 xTargetPos = xPlayerPos + xOffset;

		Zenith_CameraComponent& xCamera = m_xParentEntity.GetComponent<Zenith_CameraComponent>();
		Zenith_Maths::Vector3 xCurrentPos;
		xCamera.GetPosition(xCurrentPos);

		const Zenith_Maths::Vector3 xNewPos = glm::mix(xCurrentPos, xTargetPos, fDt * 5.0f);
		xCamera.SetPosition(xNewPos);
		xCamera.SetPitch(-0.7f);
		xCamera.SetYaw(0.0f);
	}

private:
	void ResolvePlayer()
	{
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneDataForEntity(m_xParentEntity.GetEntityID());
		if (!pxSceneData)
			return;

		Zenith_Entity xPlayer = pxSceneData->FindEntityByName("Player");
		if (xPlayer.IsValid())
		{
			m_uPlayerEntityID = xPlayer.GetEntityID();
		}
	}

	Zenith_EntityID m_uPlayerEntityID = INVALID_ENTITY_ID;
};
