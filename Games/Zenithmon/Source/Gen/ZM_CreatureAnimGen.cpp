#include "Zenith.h"

// ============================================================================
// ZM_CreatureAnimGen -- the creature-animation generator driver. See the header
// for the pure-function + rotation-only + byte-identity contract. This TU owns:
// the golden clip metadata table, the archetype builder dispatch, the pure
// driver, the byte-identity + content-hash machinery, the clip validation, and
// (tools only) the disk-bake stub (a later sub-commit wires the real bundle bake).
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_CreatureAnimGen.h"

#include <cstring>   // memcmp
#include <cstdio>    // snprintf
#include <cmath>     // std::isfinite, fabsf

namespace
{
	// FNV-1a constants (byte-identical to ZM_GenHashName / ZM_CreatureContentHash).
	constexpr u_int uZM_ANIM_FNV_OFFSET = 2166136261u;
	constexpr u_int uZM_ANIM_FNV_PRIME  = 16777619u;

	// One golden clip-metadata row. The Name single-sources the Flux clip name AND
	// the future on-disk file suffix.
	struct ZM_ClipMeta
	{
		const char* m_szName;
		float       m_fDurationSeconds;
		bool        m_bLooping;
	};

	// LOCKED 6-clip table -- indexed by ZM_ANIM_CLIP, order pinned by the enum.
	constexpr ZM_ClipMeta axZM_CLIP_META[ZM_ANIM_CLIP_COUNT] =
	{
		{ "Idle",    2.0f, true  },   // ZM_ANIM_CLIP_IDLE
		{ "Walk",    1.0f, true  },   // ZM_ANIM_CLIP_WALK
		{ "Attack",  0.7f, false },   // ZM_ANIM_CLIP_ATTACK
		{ "Special", 0.9f, false },   // ZM_ANIM_CLIP_SPECIAL
		{ "Hit",     0.4f, false },   // ZM_ANIM_CLIP_HIT
		{ "Faint",   1.2f, false },   // ZM_ANIM_CLIP_FAINT
	};

	// A quat is "same rotation" as another within eps (double-cover aware: q and
	// -q are the same rotation, so compare |dot|).
	bool ZM_QuatApproxSameRotation(const Zenith_Maths::Quat& xA, const Zenith_Maths::Quat& xB, float fEps)
	{
		const float fDot = xA.w * xB.w + xA.x * xB.x + xA.y * xB.y + xA.z * xB.z;
		return fabsf(fDot) >= 1.0f - fEps;
	}

	// A quat is finite on all four components AND ~unit-length.
	bool ZM_QuatFiniteNormalized(const Zenith_Maths::Quat& xQ, float fLenEps)
	{
		if (!std::isfinite(xQ.w) || !std::isfinite(xQ.x)
			|| !std::isfinite(xQ.y) || !std::isfinite(xQ.z))
		{
			return false;
		}
		const float fLen2 = xQ.w * xQ.w + xQ.x * xQ.x + xQ.y * xQ.y + xQ.z * xQ.z;
		return fabsf(fLen2 - 1.0f) <= fLenEps;
	}
}

// ============================================================================
// Golden metadata accessors.
// ============================================================================
const char* ZM_CreatureClipName(ZM_ANIM_CLIP eClip)
{
	Zenith_Assert(eClip < ZM_ANIM_CLIP_COUNT, "ZM_CreatureClipName: bad clip %u", (u_int)eClip);
	if (eClip >= ZM_ANIM_CLIP_COUNT) { return ""; }
	return axZM_CLIP_META[eClip].m_szName;
}

float ZM_CreatureClipDurationSeconds(ZM_ANIM_CLIP eClip)
{
	Zenith_Assert(eClip < ZM_ANIM_CLIP_COUNT, "ZM_CreatureClipDurationSeconds: bad clip %u", (u_int)eClip);
	if (eClip >= ZM_ANIM_CLIP_COUNT) { return 0.0f; }
	return axZM_CLIP_META[eClip].m_fDurationSeconds;
}

bool ZM_CreatureClipLooping(ZM_ANIM_CLIP eClip)
{
	Zenith_Assert(eClip < ZM_ANIM_CLIP_COUNT, "ZM_CreatureClipLooping: bad clip %u", (u_int)eClip);
	if (eClip >= ZM_ANIM_CLIP_COUNT) { return false; }
	return axZM_CLIP_META[eClip].m_bLooping;
}

// ============================================================================
// Archetype builder dispatch. SC1: Quadruped; SC2 adds Biped + Avian; SC3 adds
// Serpent + Aquatic; SC4 adds Insectoid + Blob; SC5 adds FloaterPlantoid -- ALL 8
// archetypes are now wired, so the dispatch is TOTAL: every archetype in
// [0, ZM_ARCHETYPE_COUNT) returns a non-null builder. Only the out-of-range
// ZM_ARCHETYPE_COUNT sentinel (and any future archetype) falls through to nullptr.
// ============================================================================
ZM_ArchetypeAnimFn ZM_GetArchetypeAnimBuilder(ZM_ARCHETYPE eArchetype)
{
	switch (eArchetype)
	{
	case ZM_ARCHETYPE_QUADRUPED:        return &ZM_BuildAnim_Quadruped;       // SC1
	case ZM_ARCHETYPE_BIPED:            return &ZM_BuildAnim_Biped;           // SC2
	case ZM_ARCHETYPE_AVIAN:            return &ZM_BuildAnim_Avian;           // SC2
	case ZM_ARCHETYPE_SERPENT:          return &ZM_BuildAnim_Serpent;         // SC3
	case ZM_ARCHETYPE_AQUATIC:          return &ZM_BuildAnim_Aquatic;         // SC3
	case ZM_ARCHETYPE_INSECTOID:        return &ZM_BuildAnim_Insectoid;       // SC4
	case ZM_ARCHETYPE_BLOB:             return &ZM_BuildAnim_Blob;            // SC4
	case ZM_ARCHETYPE_FLOATER_PLANTOID: return &ZM_BuildAnim_FloaterPlantoid; // SC5 -- dispatch now TOTAL
	default:                            return nullptr;                       // out-of-range sentinel only
	}
}

// ============================================================================
// Pure driver.
// ============================================================================
void ZM_BuildCreatureClip(ZM_ARCHETYPE eArchetype, ZM_ANIM_CLIP eClip, Flux_AnimationClip& xOut)
{
	Zenith_Assert(eClip < ZM_ANIM_CLIP_COUNT, "ZM_BuildCreatureClip: bad clip %u", (u_int)eClip);

	const ZM_ArchetypeAnimFn pxFn = ZM_GetArchetypeAnimBuilder(eArchetype);
	Zenith_Assert(pxFn != nullptr,
		"ZM_BuildCreatureClip: no anim builder wired for archetype %u yet", (u_int)eArchetype);

	// Golden metadata. Source path left empty; root motion left disabled (defaults).
	xOut.SetName(ZM_CreatureClipName(eClip));
	xOut.SetTicksPerSecond(uZM_CREATURE_ANIM_TICKS_PER_SECOND);
	xOut.SetDuration(ZM_CreatureClipDurationSeconds(eClip));
	xOut.SetLooping(ZM_CreatureClipLooping(eClip));

	if (pxFn != nullptr)
	{
		pxFn(eClip, xOut);
	}
}

// ============================================================================
// Determinism helpers. Both fold exactly the bytes Export() writes.
// ============================================================================
bool ZM_CreatureClipBytesEqual(const Flux_AnimationClip& xA, const Flux_AnimationClip& xB)
{
	Zenith_DataStream xStreamA;
	Zenith_DataStream xStreamB;
	xA.WriteToDataStream(xStreamA);
	xB.WriteToDataStream(xStreamB);

	const uint64_t ulLenA = xStreamA.GetCursor();   // bytes WRITTEN (not capacity)
	const uint64_t ulLenB = xStreamB.GetCursor();
	if (ulLenA != ulLenB) { return false; }
	if (ulLenA == 0u)     { return true;  }
	return memcmp(xStreamA.GetData(), xStreamB.GetData(), static_cast<size_t>(ulLenA)) == 0;
}

u_int ZM_CreatureClipContentHash(const Flux_AnimationClip& xClip)
{
	Zenith_DataStream xStream;
	xClip.WriteToDataStream(xStream);

	const uint64_t ulLen = xStream.GetCursor();
	u_int uHash = uZM_ANIM_FNV_OFFSET;
	if (ulLen == 0u) { return uHash; }

	const u_int8* pByte = static_cast<const u_int8*>(xStream.GetData());
	for (uint64_t i = 0; i < ulLen; ++i)
	{
		uHash ^= pByte[i];
		uHash *= uZM_ANIM_FNV_PRIME;
	}
	return uHash;
}

// ============================================================================
// Validation.
// ============================================================================
ZM_CreatureClipValidation ZM_ValidateCreatureClip(const Flux_AnimationClip& xClip,
	const ZM_GenMesh& xSkeletonMesh, bool bLooping)
{
	constexpr float fZM_ANIM_LEN_EPS  = 1.0e-3f;   // quat unit-length tolerance
	constexpr float fZM_ANIM_LOOP_EPS = 1.0e-4f;   // loop-close |dot| tolerance

	ZM_CreatureClipValidation xV;

	const Zenith_HashMap<std::string, Flux_BoneChannel>& xChannels = xClip.GetBoneChannels();
	xV.m_bHasChannels = (xChannels.GetSize() > 0u);

	// Assume-true, clear on the first offending channel (so an empty clip trivially
	// passes the per-channel flags but fails m_bHasChannels -> m_bAllValid).
	bool bAllBind    = true;
	bool bAllRotKeys = true;
	bool bFinite     = true;
	bool bLoopCloses = true;   // only computed when bLooping

	Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(xChannels);
	for (; !xIt.Done(); xIt.Next())
	{
		const Flux_BoneChannel& xChannel = xIt.GetValue();
		const std::string& strName = xChannel.GetBoneName();

		// Bone name must resolve in the archetype skeleton (else a dead channel).
		if (ZM_GenMeshFindBone(xSkeletonMesh, strName.c_str()) < 0)
		{
			if (bAllBind)   // capture the FIRST bad bone name only (always NUL-terminates)
			{
				snprintf(xV.m_szFirstBadBone, uZM_GEN_BONE_NAME_MAX, "%s", strName.c_str());
			}
			bAllBind = false;
		}

		// Every channel must carry rotation keyframes.
		if (!xChannel.HasRotationKeyframes())
		{
			bAllRotKeys = false;
			continue;   // no rotations to finiteness/loop-check
		}

		const Zenith_Vector<std::pair<Zenith_Maths::Quat, float>>& xKeys = xChannel.GetRotationKeyframes();

		// Every authored quat finite + ~unit-length.
		for (u_int u = 0; u < xKeys.GetSize(); ++u)
		{
			if (!ZM_QuatFiniteNormalized(xKeys.Get(u).first, fZM_ANIM_LEN_EPS))
			{
				bFinite = false;
			}
		}

		// Loop closure (looping clips only): first-key rotation ~= last-key rotation.
		// Keys are sorted, so front is t=0 and back is t=durationTicks.
		if (bLooping && xKeys.GetSize() > 0u)
		{
			if (!ZM_QuatApproxSameRotation(xKeys.GetFront().first, xKeys.GetBack().first, fZM_ANIM_LOOP_EPS))
			{
				bLoopCloses = false;
			}
		}
	}

	xV.m_bAllChannelsBindToBone  = bAllBind;
	xV.m_bAllChannelsHaveRotKeys = bAllRotKeys;
	xV.m_bRotationsFinite        = bFinite;
	xV.m_bDurationPositive       = (xClip.GetDuration() > 0.0f);
	xV.m_bTicksPerSecondPinned   = (xClip.GetTicksPerSecond() == uZM_CREATURE_ANIM_TICKS_PER_SECOND);
	xV.m_bLoopClosesIfLooping    = bLooping ? bLoopCloses : true;   // meaningful only when looping

	xV.m_bAllValid = xV.m_bHasChannels
		&& xV.m_bAllChannelsBindToBone
		&& xV.m_bAllChannelsHaveRotKeys
		&& xV.m_bRotationsFinite
		&& xV.m_bDurationPositive
		&& xV.m_bTicksPerSecondPinned
		&& xV.m_bLoopClosesIfLooping;
	return xV;
}

// ============================================================================
// Disk bake (TOOLS ONLY) -- SC1 stub. The real .zanim bundle bake (asset-kind
// enum + per-species file paths) lands in a later sub-commit.
// ============================================================================
#ifdef ZENITH_TOOLS
bool ZM_BakeCreatureClips(ZM_SPECIES_ID /*eId*/)
{
	// SC6: real bundle bake wires here (author each of the 6 clips for the species'
	// archetype and Export() them under the per-species asset path scheme).
	return false;
}

bool ZM_BakeAllCreatureClips()
{
	// SC6: real bundle bake wires here.
	return false;
}
#endif   // ZENITH_TOOLS
