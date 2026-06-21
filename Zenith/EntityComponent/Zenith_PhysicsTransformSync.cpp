#include "Zenith.h"
#include "EntityComponent/Zenith_PhysicsTransformSync.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Profiling/Zenith_Profiling.h"

void Zenith_SyncPhysicsTransforms()
{
	ZENITH_PROFILE_SCOPE("Physics::SyncTransforms");

	// Engine-free scene access (Zenith_SceneSystem::Get(), not the engine singleton)
	// keeps this TU off the singleton ratchet. The cross-scene QueryAllScenes form covers
	// additive + persistent scenes. SyncPhysicsPoseAndInvalidate is a no-op for entities
	// whose body has not moved (or has none), so the sweep only invalidates real changes.
	Zenith_SceneSystem::Get().QueryAllScenes<Zenith_TransformComponent, Zenith_ColliderComponent>()
		.ForEach([](Zenith_EntityID, Zenith_TransformComponent& xTransform, Zenith_ColliderComponent&)
	{
		xTransform.SyncPhysicsPoseAndInvalidate();
	});
}
