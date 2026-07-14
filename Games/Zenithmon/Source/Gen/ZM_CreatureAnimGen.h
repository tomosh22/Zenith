#pragma once

// ============================================================================
// ZM_CreatureAnimGen -- the S4 procedural creature-ANIMATION generator: it
// authors the 6 archetype clip templates (Idle / Walk / Attack / Special / Hit
// / Faint) ONCE per body plan against the FROZEN per-archetype creature skeleton
// that ZM_CreatureGen already emits, and (in tools builds) bakes them to disk
// under the per-species asset scheme.
//
// ARCHITECTURE (the leverage): a clip is a PURE closed-form function of
// (archetype, clip-id) ONLY -- NO RNG, NO species / recipe / seed input. Every
// creature bone's bind-local rotation is identity and the bone-NAME set for an
// archetype is IDENTICAL across all evolution stages and every species, so a
// clip authored against those bone NAMES transfers by name to every species of
// the archetype. ZM_BuildCreatureClip therefore yields BYTE-IDENTICAL clip bytes
// for every species of an archetype -- "author once per archetype".
//
// ROTATION-ONLY (v1, load-bearing): the sampler REPLACES a bone's bind-local TRS
// with a channel (it is NOT a delta). Bind-local ROTATION is identity for every
// creature bone regardless of size class, so an absolute local rotation is
// meaningful and identical for every species; bind-local POSITION varies per
// species (size scaling), so a position channel would teleport the bone or break
// cross-species purity. All clips author ROTATION keyframes ONLY -- no position,
// no scale channels. Vertical bob / hop / collapse is expressed via spine-flexion
// ROTATION, never root translation; root motion stays disabled.
//
// GUARD MODEL (mirrors ZM_CreatureGen / ZM_GenCommon): the pure authoring API
// below compiles in ALL configs so the in-memory ZM_Gen unit gate exercises it
// headless (Flux_AnimationClip's authoring + serialize surface is engine-side and
// all-config). Only the disk bake at the very end is #ifdef ZENITH_TOOLS, with a
// non-tools no-op so _False builds link.
//
// The reserved ZM_GEN_DOMAIN_ANIM stream stays UNUSED in v1 (clips draw from no
// RNG at all); it is kept reserved so a future stochastic variant can claim it
// without perturbing any other domain's derived seeds.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_GenCommon.h"       // ZM_GenMesh, ZM_GenMeshFindBone, uZM_GEN_BONE_NAME_MAX
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"     // ZM_ARCHETYPE, ZM_SPECIES_ID
#include "Flux/MeshAnimation/Flux_AnimationClip.h"    // Flux_AnimationClip authoring + serialize surface

// Clip-curve algorithm version. Bump on ANY curve / timing / looping change so a
// future disk-bake manifest self-invalidates stale clips.
constexpr u_int uZM_CREATUREANIMGEN_VERSION = 1u;

// Authored keyframe sample rate. Duration is stored in SECONDS but keyframe
// times are in TICKS; tick = t01 * durationSeconds * ticksPerSecond. GOLDEN.
constexpr u_int uZM_CREATURE_ANIM_TICKS_PER_SECOND = 24u;

// ---------------------------------------------------------------------------
// The 6-clip set (LOCKED -- names / durations / looping are golden literals).
//   IDLE    "Idle"    2.0s  loop
//   WALK    "Walk"    1.0s  loop
//   ATTACK  "Attack"  0.7s  one-shot
//   SPECIAL "Special" 0.9s  one-shot
//   HIT     "Hit"     0.4s  one-shot
//   FAINT   "Faint"   1.2s  one-shot (holds the downed pose; does NOT return to bind)
// ---------------------------------------------------------------------------
enum ZM_ANIM_CLIP : u_int
{
	ZM_ANIM_CLIP_IDLE,
	ZM_ANIM_CLIP_WALK,
	ZM_ANIM_CLIP_ATTACK,
	ZM_ANIM_CLIP_SPECIAL,
	ZM_ANIM_CLIP_HIT,
	ZM_ANIM_CLIP_FAINT,

	ZM_ANIM_CLIP_COUNT
};

// Golden metadata accessors (literal-pinned; no version read). The clip Name
// doubles as the Flux clip name AND the future on-disk file suffix.
const char* ZM_CreatureClipName(ZM_ANIM_CLIP eClip);
float       ZM_CreatureClipDurationSeconds(ZM_ANIM_CLIP eClip);
bool        ZM_CreatureClipLooping(ZM_ANIM_CLIP eClip);

// ---------------------------------------------------------------------------
// Per-archetype clip-authoring dispatch (the twin of ZM_CreatureGen's builder
// dispatch). A builder APPENDS the rotation channels for one (archetype, clip)
// into xOut (whose metadata the driver has already set). The 8 builders each
// live ALONE in ZM_CreatureAnimArchetype_<Name>.cpp as they land; declared
// together so the dispatch switch can take a wired one's address without a link
// error on the un-authored ones (their addresses are never taken until their
// switch case + .cpp arrive).
// ---------------------------------------------------------------------------
typedef void (*ZM_ArchetypeAnimFn)(ZM_ANIM_CLIP eClip, Flux_AnimationClip& xOut);

void ZM_BuildAnim_Quadruped      (ZM_ANIM_CLIP eClip, Flux_AnimationClip& xOut);   // SC1
void ZM_BuildAnim_Biped          (ZM_ANIM_CLIP eClip, Flux_AnimationClip& xOut);   // later
void ZM_BuildAnim_Avian          (ZM_ANIM_CLIP eClip, Flux_AnimationClip& xOut);   // later
void ZM_BuildAnim_Serpent        (ZM_ANIM_CLIP eClip, Flux_AnimationClip& xOut);   // later
void ZM_BuildAnim_Aquatic        (ZM_ANIM_CLIP eClip, Flux_AnimationClip& xOut);   // later
void ZM_BuildAnim_Insectoid      (ZM_ANIM_CLIP eClip, Flux_AnimationClip& xOut);   // later
void ZM_BuildAnim_Blob           (ZM_ANIM_CLIP eClip, Flux_AnimationClip& xOut);   // later
void ZM_BuildAnim_FloaterPlantoid(ZM_ANIM_CLIP eClip, Flux_AnimationClip& xOut);   // later

// Explicit archetype -> builder mapping. SC1 returns the Quadruped builder for
// ZM_ARCHETYPE_QUADRUPED and nullptr for every other archetype (and the
// out-of-range ZM_ARCHETYPE_COUNT sentinel), so downstream harness coverage
// auto-grows as each later builder lands and claims its case.
ZM_ArchetypeAnimFn ZM_GetArchetypeAnimBuilder(ZM_ARCHETYPE eArchetype);

// ---------------------------------------------------------------------------
// Pure driver: set the golden metadata on xOut then dispatch the archetype
// builder to append the rotation channels. Byte-identical for every species of
// an archetype. Asserts eClip in range and a non-null builder.
// ---------------------------------------------------------------------------
void ZM_BuildCreatureClip(ZM_ARCHETYPE eArchetype, ZM_ANIM_CLIP eClip, Flux_AnimationClip& xOut);

// ---------------------------------------------------------------------------
// Determinism helpers (the same-inputs byte-identity gate machinery). Both fold
// exactly the bytes Flux_AnimationClip::Export() writes (via WriteToDataStream).
// ---------------------------------------------------------------------------
bool  ZM_CreatureClipBytesEqual(const Flux_AnimationClip& xA, const Flux_AnimationClip& xB);
u_int ZM_CreatureClipContentHash(const Flux_AnimationClip& xClip);

// ---------------------------------------------------------------------------
// ZM_ValidateCreatureClip -- the S4 clip test contract in one pure call. Cross-
// checks every channel's bone name against a real archetype skeleton mesh via
// ZM_GenMeshFindBone (>= 0), so a name that is NOT in the skeleton (a silently
// dead channel) is caught. m_bLoopClosesIfLooping is only meaningful (and only
// computed) when bLooping; it is left true otherwise so m_bAllValid stays a plain
// AND of the rest.
// ---------------------------------------------------------------------------
struct ZM_CreatureClipValidation
{
	bool m_bHasChannels          = false;   // clip carries >= 1 bone channel
	bool m_bAllChannelsBindToBone = false;  // every channel name resolves in the skeleton
	bool m_bAllChannelsHaveRotKeys = false; // every channel has rotation keyframes
	bool m_bRotationsFinite       = false;  // every authored quat is finite AND ~unit-length
	bool m_bDurationPositive      = false;  // duration (seconds) > 0
	bool m_bTicksPerSecondPinned  = false;  // ticks-per-second == 24
	bool m_bLoopClosesIfLooping   = false;  // (looping only) per channel rot(t=0) ~= rot(t=durationTicks)
	bool m_bAllValid              = false;   // AND of the above
	char m_szFirstBadBone[uZM_GEN_BONE_NAME_MAX] = {};   // first channel name not in the skeleton
};
ZM_CreatureClipValidation ZM_ValidateCreatureClip(const Flux_AnimationClip& xClip,
	const ZM_GenMesh& xSkeletonMesh, bool bLooping);

// ---------------------------------------------------------------------------
// Disk bake (TOOLS ONLY) -- FROZEN decl now; the real .zanim bundle bake (the
// asset-kind enum + per-species file paths) lands in a later sub-commit. The
// SC1 tools body is a temporary stub that returns false; non-tools no-ops keep
// _False configs linking.
// ---------------------------------------------------------------------------
#ifdef ZENITH_TOOLS
bool ZM_BakeCreatureClips(ZM_SPECIES_ID eId);
bool ZM_BakeAllCreatureClips();
#else
inline bool ZM_BakeCreatureClips(ZM_SPECIES_ID) { return false; }
inline bool ZM_BakeAllCreatureClips()           { return false; }
#endif
