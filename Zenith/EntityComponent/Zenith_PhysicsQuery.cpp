#include "Zenith.h"
#include "EntityComponent/Zenith_PhysicsQuery.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Input/Zenith_Input.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"

Zenith_Physics::RaycastResult Zenith_PhysicsQuery::RaycastIgnoring(
	const Zenith_Maths::Vector3& xOrigin,
	const Zenith_Maths::Vector3& xDirection,
	float fMaxDistance,
	Zenith_EntityID xIgnoreEntity)
{
	// Resolve the ignore-entity's collider body, then forward to the leaf's
	// body-id Raycast overload. Any miss (no scene / stale handle / no collider /
	// no body) falls back to the plain unfiltered raycast — byte-for-byte the
	// behaviour the leaf's former EntityID overload had.
	if (xIgnoreEntity != INVALID_ENTITY_ID)
	{
		Zenith_Entity xEntity = Zenith_SceneSystem::Get().ResolveEntity(xIgnoreEntity);
		if (xEntity.IsValid())
		{
			if (Zenith_ColliderComponent* pxCollider = xEntity.TryGetComponent<Zenith_ColliderComponent>())
			{
				const Zenith_PhysicsBodyID xIgnoreBody = pxCollider->GetBodyID();
				if (!xIgnoreBody.IsInvalid())
				{
					return g_xEngine.Physics().Raycast(xOrigin, xDirection, fMaxDistance, xIgnoreBody);
				}
			}
		}
	}
	return g_xEngine.Physics().Raycast(xOrigin, xDirection, fMaxDistance);
}

Zenith_PhysicsQuery::Ray Zenith_PhysicsQuery::BuildRayFromMouse(Zenith_CameraComponent& xCam)
{
	Zenith_Maths::Vector2_64 xMousePos;
	// Route through Zenith_Input rather than Zenith_Window so click-driven
	// raycasts respect Zenith_InputSimulator overrides.
	g_xEngine.Input().GetMousePosition(xMousePos);

	const double fX = xMousePos.x;
	const double fY = xMousePos.y;

	glm::vec3 xNearPos = { fX, fY, 0.0f };
	glm::vec3 xFarPos = { fX, fY, 1.0f };

	glm::vec3 xOrigin = xCam.ScreenSpaceToWorldSpace(xNearPos);
	glm::vec3 xDest = xCam.ScreenSpaceToWorldSpace(xFarPos);

	Zenith_Maths::Vector3 xRayDirection = glm::normalize(
		Zenith_Maths::Vector3(xDest.x - xOrigin.x, xDest.y - xOrigin.y, xDest.z - xOrigin.z));

	Ray xRay;
	xRay.m_xOrigin = xOrigin;
	xRay.m_xDirection = xRayDirection;
	return xRay;
}
