#include "Zenith.h"

#include "Zenithmon/Source/Save/ZM_ResumePoint.h"

#include <cmath>
#include <cstdint>
#include <cstring>

// ============================================================================
// ZM_ResumePoint -- see the header for the contract. Two orderings in this file
// are load-bearing:
//
//   1. ZM_ValidateResume goes SCENE -> TAG -> TRANSFORM. The scene check must
//      come first because every later check needs a resolved ZM_WorldSpec row,
//      and because ZM_GetWorldSpec ASSERTS on an out-of-range id -- which is
//      fatal in every configuration -- so it may only ever be reached after
//      ZM_FindSceneByBuildIndex has returned something other than ZM_SCENE_NONE.
//   2. ZM_MakeWorldPosition validates EVERYTHING before it writes a single byte
//      of xOut, so "false means the destination is untouched" is true by
//      construction rather than by remembering to roll back.
//
// NOTHING HERE MAY Zenith_Assert (see the header). Every function is total.
// ============================================================================

namespace
{
	// A local mirror of the codec's IsPrintablePadded (ZM_SaveSchema.cpp:230-249)
	// for the OUTPUT side. It is duplicated rather than exported because the codec
	// is frozen by ZM-D-136 and must not grow a public surface; the two are kept
	// honest by the round-trip unit, which builds a tag here and feeds it to the
	// codec. Requires: at least one printable byte, a terminator strictly inside
	// the capacity, and every byte after the terminator zero.
	bool ZM_IsPrintablePaddedTag(const char* szTag, u_int uCapacity)
	{
		if (szTag == nullptr) { return false; }

		u_int uTerminator = uCapacity;
		for (u_int u = 0u; u < uCapacity; ++u)
		{
			const uint8_t uByte = (uint8_t)szTag[u];
			if (uByte == 0u)
			{
				uTerminator = u;
				break;
			}
			if (uByte < 0x20u || uByte > 0x7eu) { return false; }
		}
		if (uTerminator == uCapacity || uTerminator == 0u) { return false; }
		for (u_int u = uTerminator + 1u; u < uCapacity; ++u)
		{
			if (szTag[u] != '\0') { return false; }
		}
		return true;
	}

	// The INPUT-side grammar check, applied to a caller-supplied C string whose
	// tail is nobody's business. Mirrors ZM_SpawnPoint::IsTagValid exactly (1..31
	// printable bytes, NUL-terminated inside the capacity) without naming the
	// component, so this TU stays free of ECS.
	bool ZM_IsUsableSourceTag(const char* szTag, u_int uCapacity)
	{
		if (szTag == nullptr) { return false; }
		for (u_int u = 0u; u < uCapacity; ++u)
		{
			const uint8_t uByte = (uint8_t)szTag[u];
			if (uByte == 0u) { return u > 0u; }
			if (uByte < 0x20u || uByte > 0x7eu) { return false; }
		}
		return false;
	}
}

const char* ZM_ResumeValidityName(ZM_RESUME_VALIDITY eValidity)
{
	switch (eValidity)
	{
	case ZM_RESUME_VALID:              return "VALID";
	case ZM_RESUME_INVALID_TRANSFORM:  return "INVALID_TRANSFORM";
	case ZM_RESUME_INVALID_TAG:        return "INVALID_TAG";
	case ZM_RESUME_INVALID_SCENE:      return "INVALID_SCENE";
	// A switch, not a table lookup, precisely so COUNT and anything past it land
	// here instead of reading off the end of an array.
	default:                           return "UNKNOWN";
	}
}

bool ZM_IsResumeTransformUsable(const ZM_WorldPosition& xSaved)
{
	for (u_int u = 0u; u < 3u; ++u)
	{
		// isfinite, NOT isnan: an infinity is just as unusable as a NaN and the
		// classic partial guard (checking only component 0, or only for NaN) is what
		// lets an edited save through.
		if (!std::isfinite(xSaved.m_afPosition[u])) { return false; }
		if (std::fabs(xSaved.m_afPosition[u]) > fZM_RESUME_WORLD_EXTENT) { return false; }
	}
	return std::isfinite(xSaved.m_fYaw);
}

ZM_RESUME_VALIDITY ZM_ValidateResume(const ZM_WorldPosition& xPosition, bool bTagGrammarValid)
{
	// 1. SCENE. The UNSET sentinel is the ordinary state of a never-saved game and
	// is not an error worth logging; an index that simply does not resolve is the
	// same answer by a different route. Both must be caught BEFORE ZM_GetWorldSpec
	// is reached -- it asserts (fatally, in every configuration) on ZM_SCENE_NONE.
	if (xPosition.m_uSceneBuildIndex == uZM_WORLD_SCENE_UNSET)
	{
		return ZM_RESUME_INVALID_SCENE;
	}
	const ZM_SCENE_ID eScene = ZM_FindSceneByBuildIndex(xPosition.m_uSceneBuildIndex);
	if (eScene == ZM_SCENE_NONE)
	{
		return ZM_RESUME_INVALID_SCENE;
	}

	// 2. TAG. Three independent conditions, all required: the caller's grammar
	// answer, a well-formed padded field (so the strcmp below cannot run off the
	// end of an unterminated 32-byte array), and the scene actually offering it.
	// The last one is the interesting failure -- a tag another scene offers would
	// pass grammar, be accepted by the warp validator's own tag test, and then
	// WEDGE the transition in WAITING_FOR_SPAWN forever, because no marker with
	// that tag exists in the destination.
	if (!bTagGrammarValid
		|| !ZM_IsPrintablePaddedTag(xPosition.m_szSpawnTag, uZM_WORLD_SPAWN_TAG_CAPACITY))
	{
		return ZM_RESUME_INVALID_TAG;
	}

	const ZM_WorldSpec& xSpec = ZM_GetWorldSpec(eScene);
	bool bTagOffered = false;
	for (u_int u = 0u; u < xSpec.m_uSpawnTagCount; ++u)
	{
		if (xSpec.m_pszSpawnTags[u] != nullptr
			&& std::strcmp(xSpec.m_pszSpawnTags[u], xPosition.m_szSpawnTag) == 0)
		{
			bTagOffered = true;
			break;
		}
	}
	if (!bTagOffered)
	{
		return ZM_RESUME_INVALID_TAG;
	}

	// 3. TRANSFORM. A recoverable failure: the scene and tag above are a complete
	// warp destination, so the resume still happens and the marker placement
	// stands. This is the transform-first / spawn-tag-fallback rule in one line.
	if (!ZM_IsResumeTransformUsable(xPosition))
	{
		return ZM_RESUME_INVALID_TRANSFORM;
	}
	return ZM_RESUME_VALID;
}

bool ZM_CanResume(ZM_RESUME_VALIDITY eValidity)
{
	return eValidity == ZM_RESUME_VALID || eValidity == ZM_RESUME_INVALID_TRANSFORM;
}

bool ZM_ShouldUseSavedTransform(ZM_RESUME_VALIDITY eValidity)
{
	return eValidity == ZM_RESUME_VALID;
}

bool ZM_MakeWorldPosition(u_int uSceneBuildIndex, const char* szSpawnTag,
	const Zenith_Maths::Vector3& xCentrePosition, float fYaw, ZM_WorldPosition& xOut)
{
	// VALIDATE EVERYTHING FIRST. Nothing below may touch xOut until every input is
	// known good -- the "false leaves the destination byte-identical" contract is
	// what stops a half-built world position from reaching the codec.
	if (!ZM_IsUsableSourceTag(szSpawnTag, uZM_WORLD_SPAWN_TAG_CAPACITY))
	{
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM ResumePoint] ZM_MakeWorldPosition: spawn tag is empty, non-printable or "
			"longer than %u bytes -- refusing to build a world position",
			uZM_WORLD_SPAWN_TAG_CAPACITY - 1u);
		return false;
	}
	if (!std::isfinite(xCentrePosition.x) || !std::isfinite(xCentrePosition.y)
		|| !std::isfinite(xCentrePosition.z) || !std::isfinite(fYaw))
	{
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM ResumePoint] ZM_MakeWorldPosition: non-finite pose -- refusing to build a "
			"world position the save codec would reject");
		return false;
	}

	// Build into a LOCAL and publish in one assignment. A default-constructed
	// ZM_WorldPosition already zeroes all 32 tag bytes, so copying strlen() bytes
	// into it leaves the whole tail NUL -- the zero-fill the codec demands, with no
	// separate memset to forget.
	ZM_WorldPosition xBuilt;
	xBuilt.m_uSceneBuildIndex = uSceneBuildIndex;
	const size_t ulTagLength = std::strlen(szSpawnTag);
	std::memcpy(xBuilt.m_szSpawnTag, szSpawnTag, ulTagLength);
	xBuilt.m_afPosition[0] = xCentrePosition.x;
	xBuilt.m_afPosition[1] = xCentrePosition.y;
	xBuilt.m_afPosition[2] = xCentrePosition.z;
	xBuilt.m_fYaw = fYaw;

	xOut = xBuilt;
	return true;
}

float ZM_YawFromRotation(const Zenith_Maths::Quat& xRotation)
{
	// Rotate the forward basis and take its heading. Deliberately NOT
	// glm::eulerAngles(q).y -- see the header.
	const Zenith_Maths::Vector3 xForward =
		xRotation * Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
	if (!std::isfinite(xForward.x) || !std::isfinite(xForward.z))
	{
		return 0.0f;
	}
	if (xForward.x == 0.0f && xForward.z == 0.0f)
	{
		// A perfectly vertical forward vector has no heading. atan2(0,0) is 0 on
		// every implementation we ship on, but saying so explicitly keeps a
		// degenerate rotation from looking like a meaningful "facing +Z".
		return 0.0f;
	}
	return std::atan2(xForward.x, xForward.z);
}

Zenith_Maths::Quat ZM_RotationFromYaw(float fYaw)
{
	if (!std::isfinite(fYaw))
	{
		fYaw = 0.0f;
	}
	return glm::angleAxis(fYaw, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
}
