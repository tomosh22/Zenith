#pragma once

// ============================================================================
// Terrain physics-body validation -- the instrumentation for "characters fall
// through the world".
//
// WHY THIS EXISTS. A terrain with no physics body is NOT a loud failure. Every
// step of the way is a deliberate quiet no-op:
//   * a stale/partial bake makes TryReadTerrainChunkSnapshot REJECT chunk (0,0),
//     so LoadCombinedPhysicsGeometry returns with m_pxPhysicsGeometry null;
//   * Zenith_ColliderComponent::CreateTerrainShape then returns nullptr with a
//     WARNING;
//   * AddCollider treats a null TERRAIN shape as an advertised skip and returns.
// The game keeps running with the terrain rendering (or not) and NO COLLISION,
// which presents as a gameplay bug -- dynamic bodies falling through the floor --
// rather than as the asset problem it is. That is the exact trap ZM-D-182
// describes, and it is invisible in a log unless you already know to grep for it.
//
// This sweep makes the answer UNCONDITIONAL: after every runtime scene load (and
// on entering editor Play mode) it states, at INFO, whether each loaded terrain
// has physics geometry and a live body. A fall-through bug report is then one
// grep away from its cause instead of a debugging session.
//
// ★ IT IS DELIBERATELY NOT AN ASSERT ON THE COMMON FAILURE. A cold or
// deliberately-unbaked terrain tree is a LEGITIMATE developer state, and
// Zenith_Assert calls Zenith_DebugBreak() in EVERY configuration -- asserting on
// "no physics geometry" would break every dev box with a cold bake and take the
// unit gate down with it. That case is a non-fatal Zenith_Error: loud, greppable,
// actionable, harmless.
//
// The one state that IS asserted is the genuinely impossible one: physics
// geometry loaded successfully AND the collider still has no body. That cannot be
// caused by an asset, so it is an engine defect and wants a break.
// ============================================================================

// Walk every loaded scene's Zenith_TerrainComponent and report its physics state.
// szContext names what triggered the sweep (a scene path, "EnterPlayMode", ...)
// and is echoed in every line so interleaved loads stay attributable.
// Safe to call with no scenes loaded (does nothing) and cheap enough to call per
// scene load -- it touches only terrain entities, of which a scene has one or two.
void Zenith_ValidateTerrainPhysicsBodies(const char* szContext);
