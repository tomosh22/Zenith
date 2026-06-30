#include "Zenith.h"
#include "Physics/Zenith_PhysicsWorldHooks.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"

// Engine-side wiring of Zenith_PhysicsWorldHooks. The captureless thunk forwards the
// Physics leaf's "a body teleported" notification to the concrete
// Zenith_TransformComponent — so the Physics leaf names no concrete component.
// Installed once by Zenith_Engine::Initialise via Zenith_Physics_InstallWorldHooks().
// Mirrors Zenith_AIWorldHooksInstall.cpp (the same inversion-of-control that keeps the
// Physics leaf clean).

namespace
{
	void OnBodyPoseChanged(Zenith_EntityID xEntityID)
	{
		// Resolve the entity across all loaded scenes (engine-free, via Zenith_SceneSystem::Get()).
		Zenith_Entity xEntity = Zenith_SceneSystem::Get().ResolveEntity(xEntityID);
		if (!xEntity.IsValid())
		{
			return;
		}
		Zenith_TransformComponent* pxTransform = xEntity.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr)
		{
			return;
		}
		// Commits the new body pose into the cache and bumps the entity's subtree
		// revision (immediate invalidation, not next frame). No-op if the body pose
		// happens to match the cached pose.
		pxTransform->SyncPhysicsPoseAndInvalidate();
	}
}

// Called from Zenith_Engine::Initialise (forward-declared there, like
// Zenith_AI_InstallWorldHooks). Builds the hook table and installs it on the leaf.
void Zenith_Physics_InstallWorldHooks()
{
	Zenith_PhysicsWorldHooks xHooks;
	xHooks.m_pfnOnBodyPoseChanged = &OnBodyPoseChanged;
	Zenith_Physics_SetWorldHooks(xHooks);
}
