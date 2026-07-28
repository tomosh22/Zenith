#pragma once

#include "Maths/Zenith_Maths.h"   // Vector3 / Quat / AngleAxis

// ============================================================================
// ZM_DawnmerePlacement (S7 item 3 SC8) -- the authored world coordinates that
// BOTH the tools scene authoring and the tests must agree on, in ONE place.
//
// PURE. No ECS, no scene, no physics, no g_xEngine, no allocation, no I/O, and
// no ZENITH_TOOLS guard -- these constants have to be visible to boot units that
// run in a headless CI build where Project_RegisterEditorAutomationSteps is
// compiled out entirely.
//
// WHY THIS FILE EXISTS. Dawnmere is re-authored on every warm windowed tools
// boot, so the committed .zscen bytes must be reproducible from compiled
// constants rather than from anything measured. Before SC8 those constants lived
// only inside a #ifdef ZENITH_TOOLS block in Zenithmon.cpp, which meant no unit
// could assert anything about them: "the rival cannot spot the player standing on
// the whiteout respawn" was an argument in a comment. Here it is a boot unit.
//
// The town-centre anchor is MIGRATED VERBATIM from Zenithmon.cpp's local
// xTownCenterFeet -- identical literals, so no existing authored entity moves by
// one bit.
//
// This file deliberately owns ONLY the anchor and the rival. Moving the
// villager / clerk / caretaker / warden / wanderer coordinates here as well would
// let their corridor clearances become units too, but it is a large mechanical
// change with its own scene-byte exposure; it is S9 work, together with sampled
// per-NPC feet heights.
// ============================================================================

// ---- The town-centre anchor (the TownCenterSpawn marker's FEET) -------------
// The one sampled terrain surface every Dawnmere placement is derived from, and
// the warp target ZM_GameStateManager uses after a whiteout.
inline constexpr float fZM_DAWNMERE_TOWN_CENTER_X      = 512.0f;
inline constexpr float fZM_DAWNMERE_TOWN_CENTER_FEET_Y = 25.98577f;
inline constexpr float fZM_DAWNMERE_TOWN_CENTER_Z      = 480.0f;

// ---- Rival Vesper (S7 item 4) ----------------------------------------------
//
// DERIVED, NOT EYEBALLED. The two flank NPCs nearest the spawn are the villager
// (512, 490) and the caretaker (498, 498); their midpoint (505, 494) is the
// widest gate out of the plaza core. Take the ray from the TownCenter spawn
// (512, 480) through that midpoint -- direction (-7, +14) -- and extend it by
// exactly 22/7:  (512 - 22, 480 + 44) = (490, 524). That centres the approach
// lane between the two solid static AABBs (~5.0 m clearance each), which matters
// because the tests' DriveTowardXZ has NO obstacle avoidance and a 1.8 m body
// stops the player capsule dead (the step assist is 0.40 m).
//
// Separations, against an 8 m sight range and a 2.9 m interact reach:
//   caretaker (498,498)  27.20 m      warden   (478,498)  28.64 m
//   villager  (512,490)  40.50 m      clerk    (526,498)  44.41 m
//   wanderer patrol nearest endpoint (540,484)  64.03 m
//   TownCenter spawn (512,480)        49.19 m   <-- the whiteout clearance
//   z=480 Home corridor               44.00 m   <-- driven BLIND by
//                                                   ZM_PlayerHomeRoundTrip_Test
//   x=512 spawn-to-villager corridor  40.50 m
// Every one is more than 3x the sight range.
//
// GROUND. Both the spawn (32.0 m from the Plaza pad centre (512,512)) and Vesper
// (25.06 m) lie inside that pad's 45 m dirt radius and 60 m flatten radius
// (ZM_TerrainAuthoring.cpp:67), so both are flattened toward the same target
// height; neither sits inside any path's flatten band (Home 19.96 m vs radius 13,
// Lab 25.06 vs 13, Route 23.19 vs 18). Height therefore reuses the one authored
// xPlayerCenter.y like every other NPC -- and ZM_RivalVesperAuthored_Test MEASURES
// the resulting |dy| against fZM_SIGHT_MAX_VERTICAL rather than trusting it.
//
// ★ GDD DEVIATION (Q-2026-07-24-002 Q-D). GameDesignDocument.md places rival
// battle 1 on "Route 1 (L5, scripted first battle)". Route 1 does not exist in S7;
// Dawnmere is the only authored scene. When a real Route 1 is authored, MOVE HIM
// THERE and re-derive every figure above from scratch -- none of them carries over
// -- exactly as the warden's block in Zenithmon.cpp instructs for the same reason.
inline constexpr float fZM_DAWNMERE_VESPER_X = 490.0f;
inline constexpr float fZM_DAWNMERE_VESPER_Z = 524.0f;

// The yaw that points Vesper back down the approach bearing, at the town centre.
//
// atan2(dx, dz) -- X FIRST, Z SECOND. That argument order is the +Z-forward
// convention ZM_ForwardFromRotation uses (it rotates the +Z basis vector), and
// transposing it silently turns him 90 degrees. Never derived via
// glm::eulerAngles(quat).y, which collapses past 90 degrees off +Z and has already
// cost this repo a full debugging cycle.
float ZM_DawnmereVesperYaw();

// The same facing as a quaternion, built with AngleAxis about +Y. This is the
// EXACT rotation the authoring writes into the scene, so a test may compare an
// authored transform against it directly.
Zenith_Maths::Quat ZM_DawnmereVesperFacing();
