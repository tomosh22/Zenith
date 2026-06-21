#pragma once

// Post-physics scene-graph transform sync (Phase 1). Iterates every collider entity
// across all loaded scenes and, for any whose Jolt body has moved since it was last
// synced, commits the new pose into the owning Zenith_TransformComponent's cache and
// bumps its hierarchy revision (invalidating that entity's subtree's cached world
// matrices).
//
// MUST run once per frame BETWEEN Zenith_Physics::Update and Zenith_SceneSystem::Update
// (the main loop), because Scene Update runs animation/game logic that reads
// BuildModelMatrix and must observe the synced cache. This catches Jolt *simulation*
// integration moves, which go through no setter; direct teleports are invalidated
// immediately via the physics pose-change hook (Zenith_PhysicsWorldHooks). Bodyless and
// unmoved entities are a no-op.
void Zenith_SyncPhysicsTransforms();
