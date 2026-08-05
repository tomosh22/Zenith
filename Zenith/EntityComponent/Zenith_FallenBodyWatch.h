#pragma once

// ============================================================================
// Fallen-body watch -- names the entity that left the world, and when.
//
// WHY THIS EXISTS, AND WHY IT IS NOT THE TERRAIN CHECK. Zenith_TerrainPhysicsValidate
// answers "does the terrain have collision AT ALL" -- a whole-world question whose
// failure drops EVERY dynamic body. It does not help when ONE body falls through a
// floor that everything else is standing on, which is a completely different defect
// (a per-body one: tunnelling, a rebuilt/absent collider, a body spawned inside or
// beneath geometry, a velocity write that defeats contact resolution).
//
// That case is otherwise UNDIAGNOSABLE from a log: the game reports nothing, and by
// the time a human notices the character is gone, the interesting moment -- WHICH
// body, at WHAT position, moving HOW FAST, and how long after the scene loaded --
// is thousands of frames in the past.
//
// This watch runs inside the existing per-frame physics->transform sweep (so it adds
// one float compare per dynamic body, no new traversal) and reports each entity
// EXACTLY ONCE per fall, at Zenith_Error, with everything needed to bisect:
// name, position, velocity, and the frame + seconds since the watch last saw a scene
// load. An entity that returns above the threshold is re-armed, so a deliberate
// respawn does not permanently silence it.
//
// ★ NON-FATAL, DELIBERATELY. A body leaving the world is a GAMEPLAY defect, not an
// engine invariant -- some games drop props off ledges on purpose -- and
// Zenith_Assert breaks in EVERY configuration. This reports; it never halts.
// ============================================================================

// Called once per frame from Zenith_SyncPhysicsTransforms, which already walks every
// Transform+Collider pair across loaded scenes. Cheap: one comparison per dynamic
// body, and it only allocates when something has actually fallen.
void Zenith_TickFallenBodyWatch(float fDeltaSeconds);

// Re-arms every previously-reported entity and resets the frame/time origin. Called
// on scene load so the "N frames after load" figure in the report is meaningful and
// so a reloaded scene reports afresh.
void Zenith_ResetFallenBodyWatch();
