#pragma once

#include "Maths/Zenith_Maths.h"
#include "Physics/Zenith_Physics.h"   // Zenith_Physics::RaycastResult, Zenith_PhysicsBodyID
#include "ZenithECS/Zenith_Entity.h"  // Zenith_EntityID

class Zenith_CameraComponent;

// Engine-side physics-query glue. These helpers need CONCRETE engine types
// (Zenith_ColliderComponent for entity->body resolution; Zenith_CameraComponent +
// Zenith_Input for mouse picking), so they live in EntityComponent (the aggregate),
// keeping the Zenith_Physics leaf free of any concrete-component / engine include.
// Relocated out of Zenith_Physics during the Physics-leaf extraction.
class Zenith_PhysicsQuery
{
public:
	// Raycast that ignores the collider body owned by xIgnoreEntity. Resolves the
	// entity's Zenith_ColliderComponent body id, then forwards to the leaf's
	// body-id Raycast overload (the leaf cannot name Zenith_ColliderComponent).
	static Zenith_Physics::RaycastResult RaycastIgnoring(
		const Zenith_Maths::Vector3& xOrigin,
		const Zenith_Maths::Vector3& xDirection,
		float fMaxDistance,
		Zenith_EntityID xIgnoreEntity);

	// A world-space ray (origin + normalized direction).
	struct Ray
	{
		Zenith_Maths::Vector3 m_xOrigin;
		Zenith_Maths::Vector3 m_xDirection;
	};

	// Build a world-space pick ray from the current mouse position through xCam.
	// Routes mouse reads through Zenith_Input so simulated input is respected.
	static Ray BuildRayFromMouse(Zenith_CameraComponent& xCam);
};
