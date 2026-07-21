#pragma once

#include "Maths/Zenith_Maths.h"

#include "Zenithmon/Source/Data/ZM_WorldSpec.h"     // ZM_SCENE_ID, ZM_FindSceneByBuildIndex
#include "Zenithmon/Source/Party/ZM_GameState.h"    // ZM_WorldPosition, uZM_WORLD_SCENE_UNSET

// ============================================================================
// ZM_ResumePoint (S7 item 2 SC3) -- the PURE half of "where does a loaded save
// put the player".
//
// Everything in this file is a free function over plain data plus the compiled
// ZM_WorldSpec table. It names NO ECS type, NO component, NO scene handle and no
// physics body, which is what lets the whole decision surface be pinned by
// headless boot units with no scene loaded. The impure half -- resolving the
// live player, reading its body pose, and applying a restored pose after the
// marker teleport -- lives on ZM_GameStateManager and calls down into here.
//
// SaveFormat.md's transform-vs-spawn-tag TBD is RESOLVED here as the doc's own
// intended rule: TRANSFORM-FIRST, SPAWN-TAG FALLBACK. Mechanically the resume
// rides the ordinary validated warp, whose marker placement already IS the
// fallback -- so a transform that fails validation simply never overrides it, at
// zero extra cost and with no second placement path to keep in sync.
//
// NOTHING IN THIS FILE MAY Zenith_Assert ON ITS ARGUMENTS. Zenith.h:138 defines
// ZENITH_ASSERT unconditionally, so Zenith_Assert breaks the process in EVERY
// configuration, and the whole ZENITH_TEST suite runs at boot before the scene
// loads. The boot units feed these functions NaNs, oversized tags, the UNSET
// sentinel and unresolvable build indices ON PURPOSE to pin their fail-closed
// answers; an assert on one of those would not report a bug, it would end the
// entire unit run. Every function below is TOTAL and diagnoses genuinely
// mis-authored data with a NON-FATAL Zenith_Error.
// ============================================================================

// How usable a saved ZM_WorldPosition is. Ordered from "best" to "worst" so a
// caller can reason about it monotonically, and split into TWO independent
// failures on purpose: a bad transform still leaves a perfectly good scene+tag
// to warp to, while a bad scene or tag leaves nothing at all.
enum ZM_RESUME_VALIDITY : u_int
{
	ZM_RESUME_VALID = 0u,            // scene + tag + transform all good -> place at the transform
	ZM_RESUME_INVALID_TRANSFORM,     // scene + tag good, transform unusable -> place at the marker
	ZM_RESUME_INVALID_TAG,           // tag malformed, or not offered by that scene -> cannot resume
	ZM_RESUME_INVALID_SCENE,         // UNSET or unresolvable build index -> cannot resume

	ZM_RESUME_VALIDITY_COUNT
};

// TOTAL: never returns nullptr; anything outside the enumerated range is
// "UNKNOWN". A switch rather than a table so COUNT and past it cannot index off
// the end.
const char* ZM_ResumeValidityName(ZM_RESUME_VALIDITY eValidity);

// The coordinate sanity bound a restored transform must satisfy on ALL THREE
// axes, in metres from the origin.
//
// This constant is invented HERE rather than imported because the engine has no
// world-extent value this layer may reach: the only world-size number in the
// game is ZM_GrassDensityMap::fWORLD_SIZE (4096), and that header drags in
// Flux/Flux_Enums.h plus <string>/<vector>, none of which may enter a pure
// Source/Save/ TU. ZM_WorldSpec carries no extent either. 4096 m matches the
// grass map's world size and is comfortably larger than anything authored today
// (Dawnmere sits around x/z 384..540), so the bound only ever fires on an edited
// or corrupted save -- which is exactly what it is for: a garbage transform must
// degrade to marker placement, never teleport the player into the void where
// nothing can reach them.
static constexpr float fZM_RESUME_WORLD_EXTENT = 4096.0f;

// PURE. bTagGrammarValid is the caller's ALREADY-COMPUTED
// ZM_SpawnPoint::IsTagValid answer, passed IN so this TU names no ECS type and
// stays headlessly unit-testable. The other two halves of tag validity -- the
// scene resolves, and the scene actually OFFERS that tag -- are resolved here
// against ZM_WorldSpec, which is pure compiled data.
//
// Evaluation order is SCENE -> TAG -> TRANSFORM and that order is the contract:
// a save whose scene does not resolve has no meaningful tag to report on, and a
// save with no usable place to stand must be reported as INVALID_TRANSFORM (a
// recoverable, marker-placed resume) rather than as a hard tag/scene failure.
ZM_RESUME_VALIDITY ZM_ValidateResume(const ZM_WorldPosition& xPosition, bool bTagGrammarValid);

// Can a resume happen at all? VALID and INVALID_TRANSFORM only -- the latter
// still has a scene and a spawn tag, which is a complete warp destination.
bool ZM_CanResume(ZM_RESUME_VALIDITY eValidity);
// Should the SAVED TRANSFORM override the marker? VALID only.
bool ZM_ShouldUseSavedTransform(ZM_RESUME_VALIDITY eValidity);

// PURE. All three coordinates and the yaw finite, and every coordinate inside
// +/- fZM_RESUME_WORLD_EXTENT. Deliberately independent of the scene/tag halves
// so ZM_ValidateResume can report the two failures separately.
bool ZM_IsResumeTransformUsable(const ZM_WorldPosition& xSaved);

// PURE. Build a ZM_WorldPosition from live values. Returns false with NO
// mutation of xOut when the tag is null / empty / non-printable / not
// NUL-terminated inside uZM_WORLD_SPAWN_TAG_CAPACITY, or when a coordinate or
// the yaw is non-finite.
//
// ZERO-FILLS the whole 32-byte tag field after the terminator. That is not
// tidiness: save module 10 writes all 32 raw bytes verbatim and the codec's
// IsPrintablePadded hard-REJECTS any non-NUL byte after the terminator
// (ZM_SaveSchema.cpp:244-247), so a memcpy that leaves stack garbage in the tail
// does not merely make the wire image non-deterministic -- it makes the game
// UNSAVEABLE.
//
// m_afPosition stores the capsule/body CENTRE (what
// Zenith_TransformComponent::GetPosition and Zenith_Physics::GetBodyPosition
// both return), NOT feet. Spawn MARKERS store feet and
// ZM_GameStateManager::CalculateSpawnCenter adds the capsule half-extent (0.9 m
// for the authored 1.8 m player). Mixing the two conventions is a silent 0.9 m
// error that sinks or floats the player.
//
// The scene build index is NOT validated here: the caller owns which scene it is
// standing in, and ZM_ValidateResume is the one place that decides whether an
// index resolves.
bool ZM_MakeWorldPosition(u_int uSceneBuildIndex, const char* szSpawnTag,
	const Zenith_Maths::Vector3& xCentrePosition, float fYaw, ZM_WorldPosition& xOut);

// PURE. Yaw the SAME way ZM_PlayerController writes it and the SAME way Jolt's
// EnforceUpright reads it back: atan2 of the quaternion-rotated +Z
// (ZM_PlayerController.cpp:688, Zenith_Physics.cpp:703-708).
//
// NEVER glm::eulerAngles(quat).y. That is a documented trap in this repo -- it
// collapses once the facing is more than 90 degrees off +Z, so a player facing
// backwards would be restored facing forwards and only SOME headings would look
// wrong.
//
// TOTAL: a non-finite or degenerate rotation yields 0 rather than a NaN that
// would later fail codec validation.
float ZM_YawFromRotation(const Zenith_Maths::Quat& xRotation);
// The exact inverse convention: a Y-axis-only rotation, which is also the shape
// Zenith_Physics::EnforceUpright rebuilds every frame -- so a restored yaw
// SURVIVES ZM_PlayerController's per-frame upright enforcement instead of being
// quietly flattened.
Zenith_Maths::Quat ZM_RotationFromYaw(float fYaw);
