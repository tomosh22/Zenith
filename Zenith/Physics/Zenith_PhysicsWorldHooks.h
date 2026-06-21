#pragma once

#include "ZenithECS/Zenith_Entity.h"   // Zenith_EntityID

// =============================================================================
// Zenith_PhysicsWorldHooks -- leaf-safe runtime hook for the Physics core
// (mirrors Zenith_AIWorldHooks / Zenith_ECSRuntimeHooks).
//
// Zenith_Physics is a strict leaf: it names no concrete component, so it cannot
// invalidate a Zenith_TransformComponent's cached world matrix directly. When a
// body's pose is changed out-of-band (a teleport), the engine wants to invalidate
// the owning entity's cached world transform *immediately* (the per-frame
// post-physics sweep would otherwise only catch it next frame). The engine installs
// a captureless thunk (via Zenith_Physics_SetWorldHooks) that resolves the entity's
// Transform and syncs/invalidates it; the Physics core only fires the hook by
// EntityID.
//
// The pointer defaults to nullptr and a null hook is a safe no-op, so a
// physics-only / headless build (and the SentinelPhysics link proof) behaves
// identically whether or not the engine has wired the hook.
// =============================================================================

struct Zenith_PhysicsWorldHooks
{
	// Fired when a body's pose is changed OUT-OF-BAND — i.e. directly on the Jolt body,
	// bypassing both the per-step simulation integration AND the Zenith_TransformComponent
	// setters — so the engine can immediately commit the new pose into the owning
	// Transform's cache and bump its hierarchy revision (this frame, not next). Fired from
	// Zenith_Physics::TeleportBody, EnforceUpright, and LockRotation (step 3). Deliberately
	// NOT fired from the per-step simulation integration (the main loop's post-physics sweep
	// catches those next frame) nor from SetBodyPosition/Rotation (which sit downstream of a
	// Transform setter that already invalidates the cache).
	void (*m_pfnOnBodyPoseChanged)(Zenith_EntityID) = nullptr;
};

// Composition-root install (Zenith_Engine::Initialise). Copies the struct.
void Zenith_Physics_SetWorldHooks(const Zenith_PhysicsWorldHooks& xHooks);

// Null-safe fire helper the Physics core calls.
void Zenith_Physics_FireBodyPoseChanged(Zenith_EntityID xEntity);
