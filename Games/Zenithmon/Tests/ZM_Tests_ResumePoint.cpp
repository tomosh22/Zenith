#include "Zenith.h"

// ============================================================================
// ZM_Tests_ResumePoint -- S7 item 2 SC3 contract for the PURE half of world-
// position capture / resume placement / the milestone autosave policy.
//
// Everything in this file is headless and pure: no scene, no singleton, no ECS
// type, no disk. The ECS half (CaptureWorldPosition / RequestResume /
// RequestQuitToFrontEnd and the arrival-tag record) needs a live player, a live
// follow camera and a real scene load, so it is proven by the two windowed tests
// in Tests/ZM_AutoTests_SaveResume.cpp and NOT by anything here. That split is
// stated plainly so nobody reads the coverage below as covering the runtime.
//
// What this file pins, and why each part matters:
//
//   * ZM_ValidateResume's THREE-WAY answer, in the order SCENE -> TAG ->
//     TRANSFORM. The distinction between "cannot resume at all" (INVALID_SCENE /
//     INVALID_TAG) and "resume, but place at the spawn MARKER" (INVALID_TRANSFORM)
//     is the whole transform-first / spawn-tag-fallback rule (the "Load-time
//     placement" paragraph under save module 10, Docs/SaveFormat.md:279-283).
//     Collapsing the two either wedges the warp in
//     WAITING_FOR_SPAWN forever (a tag no scene offers) or silently degrades every
//     load to marker placement.
//
//   * ZM_MakeWorldPosition's REJECT-WITHOUT-MUTATION contract, byte-checked. The
//     frozen codec writes all 32 raw tag bytes verbatim and
//     ZM_SaveSchema.cpp:244-247 hard-rejects ANY non-NUL byte after the
//     terminator, so a memcpy that does not zero-fill the tail does not merely
//     perturb the wire image -- it makes the game UNSAVEABLE.
//
//   * The yaw convention. ZM_PlayerController.cpp:688 writes
//     atan2(facing.x, facing.z) and Zenith_Physics::EnforceUpright
//     (Zenith_Physics.cpp:703-708) rebuilds the same thing from
//     ATan2(forward.GetX(), forward.GetZ()). glm::eulerAngles(quat).y is a KNOWN
//     trap in this repo (it collapses past 90 degrees off +Z) and the cardinal
//     -Z case below is what catches it.
//
//   * The autosave policy: which triggers are LIVE today, and that each of the
//     FIVE blocking conditions is independently honoured. Five separate units,
//     one blocker each -- a single combined truth table still passes with any one
//     term missing from the predicate.
//
// Boot-unit legality: all pure. Nothing here touches Zenith_SaveData, disk, the
// scene system or the input simulator, so these run in any configuration at any
// point of the boot suite and leave no state behind.
//
// TOTALITY NOTE (read before adding a unit): several units below deliberately
// feed OUT-OF-RANGE ids and unresolvable build indices to the production
// functions. Every one of those functions is specified TOTAL -- it must RETURN
// its defined answer and diagnose bad data with a non-fatal
// Zenith_Error(LOG_CATEGORY_GAMEPLAY, ...). Zenith_Assert is FATAL in EVERY
// configuration (Zenith/Core/Zenith.h:138 defines ZENITH_ASSERT unconditionally
// above its own #ifdef), and the whole ZENITH_TEST suite runs at boot before the
// scene loads, so ONE assert firing on an input a unit here deliberately supplies
// kills the process and loses the ENTIRE unit gate -- not one red unit. In
// particular ZM_ValidateResume must check ZM_FindSceneByBuildIndex's
// ZM_SCENE_NONE return BEFORE calling ZM_GetWorldSpec, which opens with
// Zenith_Assert(eId < ZM_SCENE_COUNT, ...) (ZM_WorldSpec.cpp:65).
// ============================================================================

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

#include "Collections/Zenith_Vector.h"
#include "Core/Zenith_ErrorCode.h"
#include "Core/Zenith_TestFramework.h"
#include "DataStream/Zenith_DataStream.h"
#include "Maths/Zenith_Maths.h"
#include "Zenithmon/Source/Core/ZM_SaveSchema.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"
#include "Zenithmon/Source/Save/ZM_Autosave.h"
#include "Zenithmon/Source/Save/ZM_ResumePoint.h"
#include "Zenithmon/Source/Save/ZM_SaveSlots.h"

namespace
{
	// ---- literal expectations ------------------------------------------------
	// Spelled as LITERALS wherever a derived expectation would move with the code
	// under test and therefore never fail.

	// Dawnmere's build index and one spawn tag it genuinely offers
	// (ZM_WorldSpec.cpp:28,52). Both spelled out rather than looked up.
	constexpr u_int uDAWNMERE_BUILD_INDEX = 2u;
	constexpr const char* szDAWNMERE_TAG = "TownCenter";

	// A tag PlayerHome offers and Dawnmere does NOT (ZM_WorldSpec.cpp:31). This is
	// the pair that makes "the spawn-tag list is actually consulted" testable.
	constexpr const char* szPLAYERHOME_ONLY_TAG = "Door";

	// A build index no ZM_WorldSpec row carries (rows are 0/1/2/3/20/40/41/42).
	constexpr u_int uUNRESOLVABLE_BUILD_INDEX = 999u;

	// The authored Dawnmere player CENTRE (Zenithmon.cpp's TownCenter feet
	// (512, 25.98577, 480) + the 0.9 capsule half-extent).
	const Zenith_Maths::Vector3 xVALID_CENTRE(512.0f, 26.88577f, 480.0f);
	constexpr float fVALID_YAW = 0.7f;

	// Far outside ANY plausible world extent. Deliberately NOT written against a
	// production constant: naming ZM_ResumePoint's own bound here would make the
	// expectation move with the code under test. These magnitudes are finite (so
	// the finiteness guard alone cannot explain a rejection) and enormous (so any
	// extent bound at all rejects them).
	constexpr float fFAR_OUTSIDE_WORLD = 1.0e9f;

	// The SECOND rejection magnitude, and the one that actually brackets the bound.
	// 1e9 alone pins the guard only to (512.25, 1e9]: the production extent could be
	// loosened from 4096 to 1e8 and this unit, the whole boot suite and both windowed
	// tests would all stay green, i.e. the guard would be effectively disabled for
	// every realistic bad value an edited save can carry. 5000 is a HAND-WRITTEN
	// literal and is deliberately NOT spelled against fZM_RESUME_WORLD_EXTENT -- an
	// expectation derived from the code under test moves with it and can never fail.
	// Together with the authored Dawnmere control below, the two magnitudes bracket
	// the bound to roughly (540, 5000].
	constexpr float fJUST_OUTSIDE_WORLD = 5000.0f;

	const float fNAN_F = std::numeric_limits<float>::quiet_NaN();
	const float fINF_F = std::numeric_limits<float>::infinity();

	// ---- helpers -------------------------------------------------------------

	// Build a ZM_WorldPosition BY HAND, bypassing ZM_MakeWorldPosition entirely.
	// ZM_ValidateResume's units must not be fed through the very constructor whose
	// rejections they are meant to be independent of -- a hand-built value is the
	// only way to hand the validator a malformed tag or an unresolvable scene at
	// all.
	ZM_WorldPosition MakeRawWorldPosition(u_int uSceneBuildIndex, const char* szTag,
		const Zenith_Maths::Vector3& xCentre, float fYaw)
	{
		ZM_WorldPosition xPosition;
		xPosition.m_uSceneBuildIndex = uSceneBuildIndex;
		memset(xPosition.m_szSpawnTag, 0, uZM_WORLD_SPAWN_TAG_CAPACITY);
		if (szTag != nullptr)
		{
			const size_t uLength = strlen(szTag);
			const size_t uCopy = (uLength < (size_t)uZM_WORLD_SPAWN_TAG_CAPACITY - 1u)
				? uLength
				: (size_t)uZM_WORLD_SPAWN_TAG_CAPACITY - 1u;
			memcpy(xPosition.m_szSpawnTag, szTag, uCopy);
		}
		xPosition.m_afPosition[0] = xCentre.x;
		xPosition.m_afPosition[1] = xCentre.y;
		xPosition.m_afPosition[2] = xCentre.z;
		xPosition.m_fYaw = fYaw;
		return xPosition;
	}

	ZM_WorldPosition MakeValidRawWorldPosition()
	{
		return MakeRawWorldPosition(uDAWNMERE_BUILD_INDEX, szDAWNMERE_TAG,
			xVALID_CENTRE, fVALID_YAW);
	}

	// A raw byte snapshot of a ZM_WorldPosition. Used by every
	// "rejects WITHOUT mutation" unit: snapshotting the same object's bytes (rather
	// than copy-constructing and comparing two objects) keeps any padding out of
	// the comparison's meaning.
	Zenith_Vector<u_int8> SnapshotWorldPosition(const ZM_WorldPosition& xPosition)
	{
		Zenith_Vector<u_int8> xBytes;
		xBytes.Resize((u_int)sizeof(ZM_WorldPosition));
		memcpy(xBytes.GetDataPointer(), &xPosition, sizeof(ZM_WorldPosition));
		return xBytes;
	}

	void AssertWorldPositionByteIdentical(const Zenith_Vector<u_int8>& xBefore,
		const ZM_WorldPosition& xAfter, const char* szContext)
	{
		ZENITH_ASSERT_EQ(xBefore.GetSize(), (u_int)sizeof(ZM_WorldPosition),
			"%s snapshot size", szContext);
		if (xBefore.GetSize() != (u_int)sizeof(ZM_WorldPosition)) { return; }
		ZENITH_ASSERT_TRUE(memcmp(xBefore.GetDataPointer(), &xAfter, sizeof(ZM_WorldPosition)) == 0,
			"%s mutated its output on a rejected call", szContext);
	}

	// A ZM_WorldPosition filled with a RECOGNISABLE non-zero byte pattern. Two
	// jobs: it is the "before" image for the reject-without-mutation units, and it
	// is what makes the zero-fill unit real -- a memcpy that does not clear the tag
	// tail leaves 0xab bytes behind where the codec demands NULs.
	ZM_WorldPosition MakePoisonedOutput()
	{
		ZM_WorldPosition xPosition;
		memset(&xPosition, 0xab, sizeof(ZM_WorldPosition));
		return xPosition;
	}

	// Wrap an angle difference into [-pi, pi] so a comparison across the +/-pi seam
	// is not off by a full turn. Without this, a correct round trip that answers
	// -pi where the input was +pi reads as a 6.28 rad error.
	float WrapAngleDifference(float fA, float fB)
	{
		float fDelta = fA - fB;
		while (fDelta > 3.14159265358979323846f)  { fDelta -= 6.28318530717958647692f; }
		while (fDelta < -3.14159265358979323846f) { fDelta += 6.28318530717958647692f; }
		return fDelta;
	}

	// The controller's OWN convention, recomputed here from first principles:
	// rotate +Z by the quaternion and take atan2(x, z). This is an INDEPENDENT
	// oracle -- it never calls ZM_YawFromRotation -- which is what lets the
	// cardinal unit below detect a glm::eulerAngles implementation.
	float ExpectedYawFromRotation(const Zenith_Maths::Quat& xRotation)
	{
		const Zenith_Maths::Vector3 xFacing =
			xRotation * Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		return std::atan2(xFacing.x, xFacing.z);
	}

	// Encode with the FROZEN codec. Independent of everything SC3 adds, so using it
	// as the round-trip oracle is not self-referential.
	Zenith_Vector<u_int8> EncodeState(const ZM_GameState& xState, const char* szContext)
	{
		Zenith_DataStream xStream;
		const Zenith_Status xStatus = ZM_SaveSchema::Write(xState, xStream);
		ZENITH_ASSERT_TRUE(xStatus.IsOk(), "%s fixture failed to encode (error %u)",
			szContext, (u_int)xStatus.Error());
		if (!xStatus.IsOk()) { return Zenith_Vector<u_int8>(); }
		Zenith_Vector<u_int8> xBytes;
		xBytes.Resize((u_int)xStream.GetCursor());
		if (xBytes.GetSize() != 0u)
		{
			memcpy(xBytes.GetDataPointer(), xStream.GetData(), (size_t)xBytes.GetSize());
		}
		return xBytes;
	}

	// Every registered validity arm, spelled out. Nothing here is derived by
	// calling the function under test.
	const ZM_RESUME_VALIDITY aeALL_VALIDITIES[] =
	{
		ZM_RESUME_VALID,
		ZM_RESUME_INVALID_TRANSFORM,
		ZM_RESUME_INVALID_TAG,
		ZM_RESUME_INVALID_SCENE,
	};
	constexpr u_int uALL_VALIDITY_COUNT =
		(u_int)(sizeof(aeALL_VALIDITIES) / sizeof(aeALL_VALIDITIES[0]));

	const ZM_AUTOSAVE_TRIGGER aeALL_TRIGGERS[] =
	{
		ZM_AUTOSAVE_TRIGGER_NONE,
		ZM_AUTOSAVE_TRIGGER_SCENE_ENTERED,
		ZM_AUTOSAVE_TRIGGER_BADGE_EARNED,
		ZM_AUTOSAVE_TRIGGER_STORY_FLAG_SET,
		ZM_AUTOSAVE_TRIGGER_LEAGUE_ENTERED,
		ZM_AUTOSAVE_TRIGGER_TOWER_STREAK_BANKED,
	};
	constexpr u_int uALL_TRIGGER_COUNT =
		(u_int)(sizeof(aeALL_TRIGGERS) / sizeof(aeALL_TRIGGERS[0]));
}

// ============================================================================
// ZM_ValidateResume -- SCENE -> TAG -> TRANSFORM
// ============================================================================

ZENITH_TEST(ZM_Save, Resume_ValidForAKnownSceneTagAndFiniteTransform)
{
	// The positive control for the whole validator. Without it, a validator that
	// rejects EVERYTHING passes every negative unit in this file and silently
	// degrades every load to spawn-marker placement.
	const ZM_WorldPosition xPosition = MakeValidRawWorldPosition();
	const ZM_RESUME_VALIDITY eValidity = ZM_ValidateResume(xPosition, true);

	ZENITH_ASSERT_EQ((u_int)eValidity, (u_int)ZM_RESUME_VALID,
		"a Dawnmere/TownCenter pose with a finite transform must validate as VALID "
		"(got '%s')", ZM_ResumeValidityName(eValidity));
	ZENITH_ASSERT_TRUE(ZM_CanResume(eValidity),
		"a fully valid saved pose must be resumable");
	ZENITH_ASSERT_TRUE(ZM_ShouldUseSavedTransform(eValidity),
		"a fully valid saved pose must place at the SAVED transform, not the marker");
	ZENITH_ASSERT_TRUE(ZM_IsResumeTransformUsable(xPosition),
		"a finite, in-world transform must be usable");
}

ZENITH_TEST(ZM_Save, Resume_UnsetSceneSentinelIsInvalidScene)
{
	// A fresh starter carries uZM_WORLD_SCENE_UNSET. Treated as resumable, it would
	// queue a warp at build index 0xffffffff, which IsWarpDestinationValid refuses
	// -- leaving Continue silently dead rather than falling back.
	// The tag is left ALL ZERO here on purpose: the frozen codec additionally
	// requires "UNSET implies an all-zero tag" (ZM_SaveSchema.cpp:381-388), so this
	// is the shape a real unset save actually has.
	const ZM_WorldPosition xPosition = MakeRawWorldPosition(
		uZM_WORLD_SCENE_UNSET, nullptr, xVALID_CENTRE, fVALID_YAW);
	const ZM_RESUME_VALIDITY eValidity = ZM_ValidateResume(xPosition, true);

	ZENITH_ASSERT_EQ((u_int)eValidity, (u_int)ZM_RESUME_INVALID_SCENE,
		"the UNSET scene sentinel must classify as INVALID_SCENE (got '%s')",
		ZM_ResumeValidityName(eValidity));
	ZENITH_ASSERT_FALSE(ZM_CanResume(eValidity),
		"an UNSET scene must not be resumable");

	// A default-constructed ZM_GameState is exactly this shape, and it is what
	// every brand-new game carries -- so the same answer must come back from the
	// model's own default, not merely from a hand-built copy of it.
	const ZM_GameState xStarter = ZM_MakeStarterGameState();
	const ZM_RESUME_VALIDITY eStarterValidity =
		ZM_ValidateResume(xStarter.m_xWorldPosition, true);
	ZENITH_ASSERT_EQ((u_int)eStarterValidity, (u_int)ZM_RESUME_INVALID_SCENE,
		"the fixed starter GameState must not look resumable (got '%s')",
		ZM_ResumeValidityName(eStarterValidity));
}

ZENITH_TEST(ZM_Save, Resume_UnresolvableBuildIndexIsInvalidScene)
{
	// ZM_FindSceneByBuildIndex returns ZM_SCENE_NONE (== ZM_SCENE_COUNT) for an
	// unknown index (ZM_WorldSpec.cpp:83-93), and ZM_GetWorldSpec opens with a
	// Zenith_Assert on exactly that value -- which is FATAL in every configuration
	// and would take the whole boot gate down with it. This unit is the tripwire
	// for a validator that forwards the sentinel into the table lookup.
	const ZM_WorldPosition xPosition = MakeRawWorldPosition(
		uUNRESOLVABLE_BUILD_INDEX, szDAWNMERE_TAG, xVALID_CENTRE, fVALID_YAW);
	const ZM_RESUME_VALIDITY eValidity = ZM_ValidateResume(xPosition, true);

	ZENITH_ASSERT_EQ((u_int)eValidity, (u_int)ZM_RESUME_INVALID_SCENE,
		"an unresolvable build index must classify as INVALID_SCENE (got '%s')",
		ZM_ResumeValidityName(eValidity));
	ZENITH_ASSERT_FALSE(ZM_CanResume(eValidity),
		"an unresolvable build index must not be resumable");
}

ZENITH_TEST(ZM_Save, Resume_MalformedTagIsInvalidTag)
{
	// The tag GRAMMAR answer is the caller's (ZM_SpawnPoint::IsTagValid) and is
	// passed IN so this TU names no ECS type. If the validator ignores the flag,
	// a tag the warp will later refuse sails through and the screen stays black.
	const ZM_WorldPosition xPosition = MakeValidRawWorldPosition();
	const ZM_RESUME_VALIDITY eValidity = ZM_ValidateResume(xPosition, false);

	ZENITH_ASSERT_EQ((u_int)eValidity, (u_int)ZM_RESUME_INVALID_TAG,
		"bTagGrammarValid == false must classify as INVALID_TAG (got '%s')",
		ZM_ResumeValidityName(eValidity));
	ZENITH_ASSERT_FALSE(ZM_CanResume(eValidity),
		"a malformed tag must not be resumable");

	// The control: the SAME position with the grammar flag set is VALID. Without
	// this pair, a validator hardwired to INVALID_TAG passes the assertion above.
	ZENITH_ASSERT_EQ((u_int)ZM_ValidateResume(xPosition, true), (u_int)ZM_RESUME_VALID,
		"the identical position with a valid grammar flag must be VALID");
}

ZENITH_TEST(ZM_Save, Resume_TagNotOfferedByThatSceneIsInvalidTag)
{
	// "Door" is real, printable and offered -- by PLAYERHOME (ZM_WorldSpec.cpp:31),
	// not by Dawnmere. The grammar flag is TRUE, so the ONLY thing that can reject
	// this is consulting that scene's own m_pszSpawnTags list. Without that check a
	// resume aims at a marker the destination does not contain and the warp machine
	// spins in WAITING_FOR_SPAWN forever behind an opaque screen.
	const ZM_WorldPosition xPosition = MakeRawWorldPosition(
		uDAWNMERE_BUILD_INDEX, szPLAYERHOME_ONLY_TAG, xVALID_CENTRE, fVALID_YAW);
	const ZM_RESUME_VALIDITY eValidity = ZM_ValidateResume(xPosition, true);

	ZENITH_ASSERT_EQ((u_int)eValidity, (u_int)ZM_RESUME_INVALID_TAG,
		"a tag the destination scene does not offer must classify as INVALID_TAG "
		"(got '%s')", ZM_ResumeValidityName(eValidity));
	ZENITH_ASSERT_FALSE(ZM_CanResume(eValidity),
		"a tag the destination does not offer must not be resumable");

	// The mirror: the SAME tag against the scene that DOES offer it is VALID. This
	// is what stops the assertion above being satisfied by a validator that simply
	// rejects "Door" everywhere.
	const ZM_WorldPosition xPlayerHome = MakeRawWorldPosition(
		40u, szPLAYERHOME_ONLY_TAG, Zenith_Maths::Vector3(0.0f, 0.9f, 3.5f), 0.0f);
	ZENITH_ASSERT_EQ((u_int)ZM_ValidateResume(xPlayerHome, true), (u_int)ZM_RESUME_VALID,
		"'Door' IS offered by PlayerHome (build index 40) and must validate there");
}

ZENITH_TEST(ZM_Save, Resume_EmptyTagIsInvalidTag)
{
	// The grammar flag is deliberately TRUE. An empty tag must still be rejected --
	// and the guard that rejects it is the PADDED-FIELD one, ZM_IsPrintablePaddedTag
	// (ZM_ResumePoint.cpp:33-54): a terminator at index 0 fails its
	// `uTerminator == 0u` test and returns INVALID_TAG BEFORE the ZM_WorldSpec
	// offered-tag loop is entered at all. It is NOT the offered-list check, and
	// naming that one here would send the next reader to the wrong function.
	// Passing false for the grammar flag would make this unit a duplicate of the
	// malformed-tag one and would prove nothing extra.
	const ZM_WorldPosition xPosition = MakeRawWorldPosition(
		uDAWNMERE_BUILD_INDEX, "", xVALID_CENTRE, fVALID_YAW);

	const ZM_RESUME_VALIDITY eGrammarOk = ZM_ValidateResume(xPosition, true);
	ZENITH_ASSERT_EQ((u_int)eGrammarOk, (u_int)ZM_RESUME_INVALID_TAG,
		"an EMPTY tag must classify as INVALID_TAG even when the caller reports the "
		"grammar as valid (got '%s')", ZM_ResumeValidityName(eGrammarOk));
	ZENITH_ASSERT_FALSE(ZM_CanResume(eGrammarOk), "an empty tag must not be resumable");

	// The neighbouring case that genuinely REACHES the offered-list loop, which the
	// empty tag above cannot. A single space is one printable byte (0x20 is inside
	// the 0x20..0x7e range ZM_IsPrintablePaddedTag accepts), terminated at index 1
	// and zero-padded to 32, so it clears the grammar flag AND the padded-field
	// guard -- and no ZM_WorldSpec row offers it (ZM_WorldSpec.cpp:27-33). This
	// replaces a second empty-tag sub-assertion that passed `false` for the grammar
	// flag: that one short-circuited on the flag and stayed green even if the flag
	// were ignored entirely, so it pinned nothing this file's malformed-tag control
	// does not already pin.
	const ZM_WorldPosition xSpaceTag = MakeRawWorldPosition(
		uDAWNMERE_BUILD_INDEX, " ", xVALID_CENTRE, fVALID_YAW);
	const ZM_RESUME_VALIDITY eSpaceTag = ZM_ValidateResume(xSpaceTag, true);
	ZENITH_ASSERT_EQ((u_int)eSpaceTag, (u_int)ZM_RESUME_INVALID_TAG,
		"a single-space tag is printable and correctly padded, so only the "
		"offered-tag list can reject it -- and no scene offers ' ' (got '%s')",
		ZM_ResumeValidityName(eSpaceTag));
	ZENITH_ASSERT_FALSE(ZM_CanResume(eSpaceTag),
		"a tag no scene offers must not be resumable");
}

ZENITH_TEST(ZM_Save, Resume_NonFiniteTransformIsInvalidTransformNotInvalidTag)
{
	// The classic partial guard checks m_afPosition[0] and stops. Every component
	// AND the yaw get their own sub-case, against NaN, +inf and -inf. std::isnan
	// alone (rather than std::isfinite) passes the two infinities.
	//
	// The answer must be INVALID_TRANSFORM specifically: the scene and tag are
	// fine, so the resume still happens -- it just falls back to the spawn MARKER.
	// Collapsing this into INVALID_TAG would make a garbage transform kill Continue
	// outright.
	const float afBadValues[3] = { fNAN_F, fINF_F, -fINF_F };
	const char* const aszBadNames[3] = { "NaN", "+inf", "-inf" };

	for (u_int uBad = 0u; uBad < 3u; ++uBad)
	{
		for (u_int uComponent = 0u; uComponent < 4u; ++uComponent)
		{
			ZM_WorldPosition xPosition = MakeValidRawWorldPosition();
			if (uComponent < 3u)
			{
				xPosition.m_afPosition[uComponent] = afBadValues[uBad];
			}
			else
			{
				xPosition.m_fYaw = afBadValues[uBad];
			}

			const ZM_RESUME_VALIDITY eValidity = ZM_ValidateResume(xPosition, true);
			ZENITH_ASSERT_EQ((u_int)eValidity, (u_int)ZM_RESUME_INVALID_TRANSFORM,
				"%s in %s must classify as INVALID_TRANSFORM (got '%s')",
				aszBadNames[uBad],
				(uComponent < 3u) ? "a position component" : "the yaw",
				ZM_ResumeValidityName(eValidity));
			ZENITH_ASSERT_TRUE(ZM_CanResume(eValidity),
				"%s in component %u must still allow a MARKER resume",
				aszBadNames[uBad], uComponent);
			ZENITH_ASSERT_FALSE(ZM_ShouldUseSavedTransform(eValidity),
				"%s in component %u must not be used as a placement transform",
				aszBadNames[uBad], uComponent);
			ZENITH_ASSERT_FALSE(ZM_IsResumeTransformUsable(xPosition),
				"%s in component %u must make the transform unusable",
				aszBadNames[uBad], uComponent);
		}
	}

	// The control: the same fixture with every value finite is VALID and usable.
	const ZM_WorldPosition xGood = MakeValidRawWorldPosition();
	ZENITH_ASSERT_TRUE(ZM_IsResumeTransformUsable(xGood),
		"the finite control transform must be usable");
}

ZENITH_TEST(ZM_Save, Resume_PositionOutsideTheWorldExtentIsInvalidTransform)
{
	// Finiteness alone is not enough: an edited save carrying a finite but absurd
	// coordinate would teleport the player into the void, out of terrain range,
	// with no way back. Each axis gets its own sub-case in both directions.
	//
	// Neither magnitude is written against ZM_ResumePoint's own extent constant -- an
	// expectation derived from the code under test moves with it and can never fail.
	// BOTH are needed and they do different jobs:
	//   * 1e9 is finite (so the finiteness guard cannot explain the rejection) and
	//     beyond any world any version of this game will have; and
	//   * 5000 is only just outside the intended world, and it is the one that keeps
	//     the bound HONEST -- with 1e9 alone the production extent could be loosened
	//     to 1e8 and every assertion here would still pass.
	const float afSigns[2] = { 1.0f, -1.0f };
	const float afMagnitudes[2] = { fFAR_OUTSIDE_WORLD, fJUST_OUTSIDE_WORLD };

	for (u_int uMagnitude = 0u; uMagnitude < 2u; ++uMagnitude)
	{
		for (u_int uSign = 0u; uSign < 2u; ++uSign)
		{
			for (u_int uComponent = 0u; uComponent < 3u; ++uComponent)
			{
				const float fBadValue = afSigns[uSign] * afMagnitudes[uMagnitude];
				ZM_WorldPosition xPosition = MakeValidRawWorldPosition();
				xPosition.m_afPosition[uComponent] = fBadValue;

				const ZM_RESUME_VALIDITY eValidity = ZM_ValidateResume(xPosition, true);
				ZENITH_ASSERT_EQ((u_int)eValidity, (u_int)ZM_RESUME_INVALID_TRANSFORM,
					"a component %u of %.1f must classify as INVALID_TRANSFORM (got '%s')",
					uComponent, (double)fBadValue, ZM_ResumeValidityName(eValidity));
				ZENITH_ASSERT_FALSE(ZM_IsResumeTransformUsable(xPosition),
					"a component %u of %.1f must not be a usable transform",
					uComponent, (double)fBadValue);
				// It is still a RESUME -- just at the marker. Dropping to INVALID_TAG
				// here would kill Continue on a recoverable save.
				ZENITH_ASSERT_TRUE(ZM_CanResume(eValidity),
					"an out-of-extent transform must still allow a MARKER resume");
			}
		}
	}

	// The control: the authored Dawnmere player centre is INSIDE the extent. Reds
	// if the bound is tightened to something that excludes the shipped world.
	ZENITH_ASSERT_TRUE(ZM_IsResumeTransformUsable(MakeValidRawWorldPosition()),
		"the authored Dawnmere player centre must be inside the world extent");
}

ZENITH_TEST(ZM_Save, Resume_CanResumeMatrixIsTotal)
{
	// EXACTLY {VALID, INVALID_TRANSFORM} may resume, and EXACTLY {VALID} may use
	// the saved transform. A new arm added without deciding its resumability lands
	// here rather than in a black screen.
	ZENITH_ASSERT_EQ((u_int)ZM_RESUME_VALIDITY_COUNT, uALL_VALIDITY_COUNT,
		"a ZM_RESUME_VALIDITY arm was added or removed without updating this file");

	const u_int uWalkCount = ((u_int)ZM_RESUME_VALIDITY_COUNT < uALL_VALIDITY_COUNT)
		? (u_int)ZM_RESUME_VALIDITY_COUNT
		: uALL_VALIDITY_COUNT;
	for (u_int u = 0u; u < uWalkCount; ++u)
	{
		const ZM_RESUME_VALIDITY eValidity = aeALL_VALIDITIES[u];
		const bool bExpectedCanResume =
			(eValidity == ZM_RESUME_VALID) || (eValidity == ZM_RESUME_INVALID_TRANSFORM);
		const bool bExpectedUseTransform = (eValidity == ZM_RESUME_VALID);

		ZENITH_ASSERT_EQ(ZM_CanResume(eValidity), bExpectedCanResume,
			"ZM_CanResume('%s')", ZM_ResumeValidityName(eValidity));
		ZENITH_ASSERT_EQ(ZM_ShouldUseSavedTransform(eValidity), bExpectedUseTransform,
			"ZM_ShouldUseSavedTransform('%s')", ZM_ResumeValidityName(eValidity));
	}

	// TOTAL: out of range must fail CLOSED on both. A garbage validity opening the
	// resume would place the player from an unvalidated transform.
	const ZM_RESUME_VALIDITY eGarbage =
		(ZM_RESUME_VALIDITY)((u_int)ZM_RESUME_VALIDITY_COUNT + 3u);
	ZENITH_ASSERT_FALSE(ZM_CanResume(eGarbage),
		"an out-of-range validity must not be resumable");
	ZENITH_ASSERT_FALSE(ZM_ShouldUseSavedTransform(eGarbage),
		"an out-of-range validity must not authorise a saved transform");
}

ZENITH_TEST(ZM_Save, Resume_ValidityNameIsTotalAndNeverNull)
{
	for (u_int u = 0u; u < uALL_VALIDITY_COUNT; ++u)
	{
		const char* szName = ZM_ResumeValidityName(aeALL_VALIDITIES[u]);
		ZENITH_ASSERT_NOT_NULL(szName, "ZM_ResumeValidityName(%u) is null", u);
		if (szName == nullptr) { continue; }
		ZENITH_ASSERT_TRUE(szName[0] != '\0', "ZM_ResumeValidityName(%u) is empty", u);
	}

	// Distinctness: a copy-pasted arm makes two different verdicts read identically
	// in every log this project will ever produce.
	for (u_int uA = 0u; uA < uALL_VALIDITY_COUNT; ++uA)
	{
		for (u_int uB = uA + 1u; uB < uALL_VALIDITY_COUNT; ++uB)
		{
			const char* szA = ZM_ResumeValidityName(aeALL_VALIDITIES[uA]);
			const char* szB = ZM_ResumeValidityName(aeALL_VALIDITIES[uB]);
			if (szA == nullptr || szB == nullptr) { continue; }
			ZENITH_ASSERT_TRUE(strcmp(szA, szB) != 0,
				"validities %u and %u share the name '%s'", uA, uB, szA);
		}
	}

	const char* szOutOfRange =
		ZM_ResumeValidityName((ZM_RESUME_VALIDITY)((u_int)ZM_RESUME_VALIDITY_COUNT + 9u));
	ZENITH_ASSERT_NOT_NULL(szOutOfRange,
		"ZM_ResumeValidityName must never return null, even out of range");
	const char* szCountSentinel = ZM_ResumeValidityName(ZM_RESUME_VALIDITY_COUNT);
	ZENITH_ASSERT_NOT_NULL(szCountSentinel,
		"ZM_ResumeValidityName(COUNT) must never return null");
}

// ============================================================================
// ZM_MakeWorldPosition -- the only sanctioned way to build module 10's payload
// ============================================================================

ZENITH_TEST(ZM_Save, WorldPosition_RoundTripsSceneTagCentreAndYaw)
{
	ZM_WorldPosition xOut = MakePoisonedOutput();
	const Zenith_Maths::Vector3 xCentre(512.25f, 26.5f, 480.75f);
	const bool bMade = ZM_MakeWorldPosition(uDAWNMERE_BUILD_INDEX, szDAWNMERE_TAG,
		xCentre, -1.25f, xOut);

	ZENITH_ASSERT_TRUE(bMade, "a well-formed capture must be accepted");
	if (!bMade) { return; }

	ZENITH_ASSERT_EQ(xOut.m_uSceneBuildIndex, uDAWNMERE_BUILD_INDEX,
		"the scene build index was not carried through");
	ZENITH_ASSERT_STREQ(xOut.m_szSpawnTag, szDAWNMERE_TAG,
		"the spawn tag was not carried through");
	// Exact float equality on purpose: this is a byte-for-byte transport, not a
	// computation, so any tolerance here would hide a lossy conversion.
	ZENITH_ASSERT_TRUE(xOut.m_afPosition[0] == xCentre.x, "position x was altered");
	ZENITH_ASSERT_TRUE(xOut.m_afPosition[1] == xCentre.y, "position y was altered");
	ZENITH_ASSERT_TRUE(xOut.m_afPosition[2] == xCentre.z, "position z was altered");
	ZENITH_ASSERT_TRUE(xOut.m_fYaw == -1.25f, "yaw was altered");
}

ZENITH_TEST(ZM_Save, WorldPosition_RejectsAnOversizedTagWithoutMutation)
{
	// 32-byte field: the longest legal tag body is 31 characters plus the NUL. A
	// strncpy-style implementation happily fills all 32 bytes and leaves the field
	// UNTERMINATED, which the codec's IsPrintablePadded then rejects -- making the
	// game unsaveable at the exact moment the player asks to save.
	char szOversized[41];
	memset(szOversized, 'a', 40);
	szOversized[40] = '\0';

	char szExactlyCapacity[uZM_WORLD_SPAWN_TAG_CAPACITY + 1u];
	memset(szExactlyCapacity, 'b', uZM_WORLD_SPAWN_TAG_CAPACITY);
	szExactlyCapacity[uZM_WORLD_SPAWN_TAG_CAPACITY] = '\0';

	const char* const aszRejected[2] = { szOversized, szExactlyCapacity };
	for (u_int u = 0u; u < 2u; ++u)
	{
		ZM_WorldPosition xOut = MakePoisonedOutput();
		const Zenith_Vector<u_int8> xBefore = SnapshotWorldPosition(xOut);
		const bool bMade = ZM_MakeWorldPosition(uDAWNMERE_BUILD_INDEX, aszRejected[u],
			xVALID_CENTRE, fVALID_YAW, xOut);
		ZENITH_ASSERT_FALSE(bMade,
			"a %u-character tag must be rejected (the field holds 31 + NUL)",
			(u_int)strlen(aszRejected[u]));
		AssertWorldPositionByteIdentical(xBefore, xOut,
			"the oversized-tag rejection");
	}

	// The BOUNDARY that must still be accepted: exactly 31 characters plus the NUL.
	// Reds if the length guard is written one off in the safe direction, which
	// would quietly make long-but-legal tags unsaveable.
	char szMaximal[uZM_WORLD_SPAWN_TAG_CAPACITY];
	memset(szMaximal, 'c', uZM_WORLD_SPAWN_TAG_CAPACITY - 1u);
	szMaximal[uZM_WORLD_SPAWN_TAG_CAPACITY - 1u] = '\0';
	ZM_WorldPosition xMaximalOut = MakePoisonedOutput();
	const bool bMaximal = ZM_MakeWorldPosition(uDAWNMERE_BUILD_INDEX, szMaximal,
		xVALID_CENTRE, fVALID_YAW, xMaximalOut);
	ZENITH_ASSERT_TRUE(bMaximal,
		"a %u-character tag exactly fills the field and must be accepted",
		(u_int)(uZM_WORLD_SPAWN_TAG_CAPACITY - 1u));
	if (bMaximal)
	{
		ZENITH_ASSERT_STREQ(xMaximalOut.m_szSpawnTag, szMaximal,
			"the maximal tag was truncated");
		ZENITH_ASSERT_TRUE(
			xMaximalOut.m_szSpawnTag[uZM_WORLD_SPAWN_TAG_CAPACITY - 1u] == '\0',
			"the maximal tag left the field unterminated");
	}
}

ZENITH_TEST(ZM_Save, WorldPosition_RejectsEmptyAndNonPrintableTags)
{
	// The tag reaches the wire verbatim and the codec re-checks it. Anything that
	// slips through here produces a save the game cannot write.
	const char szTab[]      = { 'A', '\t', 'B', '\0' };
	const char szNewline[]  = { 'A', '\n', 'B', '\0' };
	const char szControl[]  = { 'A', (char)0x01, 'B', '\0' };
	const char szDelete[]   = { 'A', (char)0x7f, 'B', '\0' };
	// Via unsigned char: a direct (char)0x80 is a truncating constant cast (C4310,
	// warnings-as-errors here). The byte VALUE is what this test is about.
	const char szHighByte[] = { 'A', (char)(unsigned char)0x80u, 'B', '\0' };
	const char szFullByte[] = { 'A', (char)(unsigned char)0xffu, 'B', '\0' };

	const char* const aszRejected[] =
	{
		"", szTab, szNewline, szControl, szDelete, szHighByte, szFullByte,
	};
	constexpr u_int uRejectedCount =
		(u_int)(sizeof(aszRejected) / sizeof(aszRejected[0]));

	for (u_int u = 0u; u < uRejectedCount; ++u)
	{
		ZM_WorldPosition xOut = MakePoisonedOutput();
		const Zenith_Vector<u_int8> xBefore = SnapshotWorldPosition(xOut);
		const bool bMade = ZM_MakeWorldPosition(uDAWNMERE_BUILD_INDEX, aszRejected[u],
			xVALID_CENTRE, fVALID_YAW, xOut);
		ZENITH_ASSERT_FALSE(bMade, "tag case %u must be rejected", u);
		AssertWorldPositionByteIdentical(xBefore, xOut, "a non-printable tag rejection");
	}

	// A null tag pointer must be refused, not dereferenced.
	{
		ZM_WorldPosition xOut = MakePoisonedOutput();
		const Zenith_Vector<u_int8> xBefore = SnapshotWorldPosition(xOut);
		const bool bMade = ZM_MakeWorldPosition(uDAWNMERE_BUILD_INDEX, nullptr,
			xVALID_CENTRE, fVALID_YAW, xOut);
		ZENITH_ASSERT_FALSE(bMade, "a null tag pointer must be rejected");
		AssertWorldPositionByteIdentical(xBefore, xOut, "the null-tag rejection");
	}

	// The control: an ordinary printable tag IS accepted, so the assertions above
	// cannot be satisfied by a function that rejects everything.
	{
		ZM_WorldPosition xOut = MakePoisonedOutput();
		ZENITH_ASSERT_TRUE(ZM_MakeWorldPosition(uDAWNMERE_BUILD_INDEX, szDAWNMERE_TAG,
			xVALID_CENTRE, fVALID_YAW, xOut),
			"an ordinary printable tag must be accepted");
	}
}

ZENITH_TEST(ZM_Save, WorldPosition_ZeroFillsTheTagTailAfterTheTerminator)
{
	// Module 10 writes all 32 raw tag bytes verbatim and ZM_SaveSchema's
	// IsPrintablePadded (ZM_SaveSchema.cpp:244-247) rejects ANY non-NUL byte after
	// the terminator. A memcpy that only writes strlen bytes leaves whatever was in
	// the destination -- here the 0xab poison, in the wild uninitialised stack --
	// and makes the save fail validation.
	ZM_WorldPosition xOut = MakePoisonedOutput();
	const char* szTag = "FromHome";
	const size_t uTagLength = strlen(szTag);

	const bool bMade = ZM_MakeWorldPosition(uDAWNMERE_BUILD_INDEX, szTag,
		xVALID_CENTRE, fVALID_YAW, xOut);
	ZENITH_ASSERT_TRUE(bMade, "'FromHome' is a printable tag Dawnmere offers");
	if (!bMade) { return; }

	ZENITH_ASSERT_STREQ(xOut.m_szSpawnTag, szTag, "the tag body was not written");
	ZENITH_ASSERT_TRUE(xOut.m_szSpawnTag[uTagLength] == '\0',
		"the tag was not NUL-terminated");
	for (size_t u = uTagLength + 1u; u < (size_t)uZM_WORLD_SPAWN_TAG_CAPACITY; ++u)
	{
		ZENITH_ASSERT_TRUE(xOut.m_szSpawnTag[u] == '\0',
			"tag tail byte %u is 0x%02x, not NUL -- the codec rejects this and the "
			"game becomes unsaveable", (u_int)u, (u_int)(unsigned char)xOut.m_szSpawnTag[u]);
	}
}

ZENITH_TEST(ZM_Save, WorldPosition_RejectsNonFiniteFloats)
{
	// A NaN or infinity reaching m_afPosition is not a cosmetic problem: the frozen
	// codec's ValidateWorldPosition rejects it, so ZM_SaveSlots::WriteState returns
	// INVALID_ARGUMENT and the player simply cannot save any more.
	const float afBadValues[3] = { fNAN_F, fINF_F, -fINF_F };
	const char* const aszBadNames[3] = { "NaN", "+inf", "-inf" };

	for (u_int uBad = 0u; uBad < 3u; ++uBad)
	{
		for (u_int uComponent = 0u; uComponent < 4u; ++uComponent)
		{
			Zenith_Maths::Vector3 xCentre = xVALID_CENTRE;
			float fYaw = fVALID_YAW;
			if (uComponent == 0u)      { xCentre.x = afBadValues[uBad]; }
			else if (uComponent == 1u) { xCentre.y = afBadValues[uBad]; }
			else if (uComponent == 2u) { xCentre.z = afBadValues[uBad]; }
			else                       { fYaw = afBadValues[uBad]; }

			ZM_WorldPosition xOut = MakePoisonedOutput();
			const Zenith_Vector<u_int8> xBefore = SnapshotWorldPosition(xOut);
			const bool bMade = ZM_MakeWorldPosition(uDAWNMERE_BUILD_INDEX, szDAWNMERE_TAG,
				xCentre, fYaw, xOut);
			ZENITH_ASSERT_FALSE(bMade, "%s in %s must be rejected",
				aszBadNames[uBad],
				(uComponent < 3u) ? "a position component" : "the yaw");
			AssertWorldPositionByteIdentical(xBefore, xOut,
				"the non-finite capture rejection");
		}
	}
}

// ============================================================================
// Yaw -- the ONE convention shared by the controller, Jolt's EnforceUpright and
// the save payload
// ============================================================================

ZENITH_TEST(ZM_Save, Yaw_MatchesTheControllerConventionForFourCardinalHeadings)
{
	// The oracle is recomputed from the rotated +Z vector, never from
	// ZM_YawFromRotation, so this compares two INDEPENDENT answers.
	//
	// The -Z (pi) heading is the one that matters most: glm::eulerAngles(quat).y
	// COLLAPSES there (it reports the rotation as pitch/roll instead), which is the
	// documented engine trap this project has already been bitten by. A test that
	// only exercised 0 and +/-pi/2 would pass with eulerAngles in place.
	struct YawCase
	{
		float m_fAngle;
		const char* m_szName;
	};
	const YawCase axCases[] =
	{
		{  0.0f,                 "+Z (north)"     },
		{  1.57079632679489662f, "+X (east)"      },
		{  3.14159265358979324f, "-Z (south)"     },
		{ -1.57079632679489662f, "-X (west)"      },
		{  2.35619449019234493f, "-Z/+X diagonal" },
		{ -2.35619449019234493f, "-Z/-X diagonal" },
	};
	constexpr u_int uCaseCount = (u_int)(sizeof(axCases) / sizeof(axCases[0]));

	for (u_int u = 0u; u < uCaseCount; ++u)
	{
		const Zenith_Maths::Quat xRotation = glm::angleAxis(axCases[u].m_fAngle,
			Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		const float fExpected = ExpectedYawFromRotation(xRotation);
		const float fActual = ZM_YawFromRotation(xRotation);

		ZENITH_ASSERT_TRUE(std::fabs(WrapAngleDifference(fActual, fExpected)) < 0.0005f,
			"heading %s: ZM_YawFromRotation returned %.5f but atan2(facing.x, facing.z) "
			"is %.5f -- the yaw convention does not match ZM_PlayerController.cpp:688",
			axCases[u].m_szName, (double)fActual, (double)fExpected);
		// ...and against the angle the quaternion was BUILT from, which is what
		// pins the -Z case specifically.
		ZENITH_ASSERT_TRUE(
			std::fabs(WrapAngleDifference(fActual, axCases[u].m_fAngle)) < 0.0005f,
			"heading %s: ZM_YawFromRotation returned %.5f for a Y rotation of %.5f",
			axCases[u].m_szName, (double)fActual, (double)axCases[u].m_fAngle);
	}
}

ZENITH_TEST(ZM_Save, Yaw_RotationFromYawRoundTripsThroughYawFromRotation)
{
	// The two helpers are used at OPPOSITE ends of the save: capture takes the live
	// body rotation apart, placement puts it back together. If they disagree the
	// restored facing is mirrored -- geometrically plausible and completely wrong.
	const float afHeadings[] =
	{
		0.0f,
		0.78539816339744831f,
		1.57079632679489662f,
		2.35619449019234493f,
		3.14159265358979324f,
		-0.78539816339744831f,
		-1.57079632679489662f,
		-2.35619449019234493f,
		-3.14159265358979324f,
	};
	constexpr u_int uHeadingCount = (u_int)(sizeof(afHeadings) / sizeof(afHeadings[0]));

	for (u_int u = 0u; u < uHeadingCount; ++u)
	{
		const Zenith_Maths::Quat xRotation = ZM_RotationFromYaw(afHeadings[u]);
		const float fRoundTripped = ZM_YawFromRotation(xRotation);
		ZENITH_ASSERT_TRUE(
			std::fabs(WrapAngleDifference(fRoundTripped, afHeadings[u])) < 0.0005f,
			"yaw %.5f round-tripped to %.5f",
			(double)afHeadings[u], (double)fRoundTripped);

		// The rebuilt quaternion must also FACE the right way, checked from the
		// rotated +Z rather than from the helper being tested.
		const float fFacingYaw = ExpectedYawFromRotation(xRotation);
		ZENITH_ASSERT_TRUE(
			std::fabs(WrapAngleDifference(fFacingYaw, afHeadings[u])) < 0.0005f,
			"ZM_RotationFromYaw(%.5f) produced a quaternion facing %.5f",
			(double)afHeadings[u], (double)fFacingYaw);

		// Jolt asserts on denormalised quaternions in SetRotation, so a helper that
		// hands one over takes the process down rather than failing a test.
		ZENITH_ASSERT_TRUE(std::fabs(glm::length(xRotation) - 1.0f) < 0.001f,
			"ZM_RotationFromYaw(%.5f) returned a denormalised quaternion (length %.6f)",
			(double)afHeadings[u], (double)glm::length(xRotation));
	}
}

// ============================================================================
// The resume decision survives the frozen wire format
// ============================================================================

ZENITH_TEST(ZM_Save, Codec_ResumeDecisionSurvivesASchemaRoundTrip)
{
	// A capture that validates in RAM but decodes differently off disk would give
	// the player a different world on every load. This is the only unit that puts
	// the two halves together: build -> encode -> decode -> re-validate.
	//
	// NOTE, stated honestly: the codec ALREADY refuses to write a world position
	// whose scene or tag is bad (ZM_SaveSchema.cpp:366-408), so the INVALID_SCENE
	// and INVALID_TAG arms are unreachable through a round trip and are covered by
	// the pure units above instead. What this unit covers is the VALID and
	// INVALID_TRANSFORM pair -- the two the codec will actually carry.
	ZM_WorldPosition xBuilt = MakePoisonedOutput();
	const Zenith_Maths::Vector3 xCentre(512.25f, 26.5f, 480.75f);
	const bool bMade = ZM_MakeWorldPosition(uDAWNMERE_BUILD_INDEX, szDAWNMERE_TAG,
		xCentre, -1.25f, xBuilt);
	ZENITH_ASSERT_TRUE(bMade, "the round-trip fixture failed to build");
	if (!bMade) { return; }

	ZM_GameState xState = ZM_MakeStarterGameState();
	xState.m_xWorldPosition = xBuilt;

	const Zenith_Vector<u_int8> xBytes = EncodeState(xState, "the resume round trip");
	if (xBytes.GetSize() == 0u) { return; }

	// Decode into a SEPARATE, default-constructed state. Reading back into the
	// object that was written from proves only that an object equals itself.
	ZM_GameState xDecoded;
	Zenith_DataStream xStream((void*)xBytes.GetDataPointer(), xBytes.GetSize());
	const Zenith_Status xStatus = ZM_SaveSchema::Read(xStream, xBytes.GetSize(), xDecoded);
	ZENITH_ASSERT_TRUE(xStatus.IsOk(), "the round-trip payload did not decode (error %u)",
		(u_int)xStatus.Error());
	if (!xStatus.IsOk()) { return; }

	ZENITH_ASSERT_EQ(xDecoded.m_xWorldPosition.m_uSceneBuildIndex,
		xBuilt.m_uSceneBuildIndex, "the scene build index did not survive the wire");
	ZENITH_ASSERT_TRUE(memcmp(xDecoded.m_xWorldPosition.m_szSpawnTag,
		xBuilt.m_szSpawnTag, uZM_WORLD_SPAWN_TAG_CAPACITY) == 0,
		"all 32 spawn-tag bytes must survive the wire verbatim, tail included");
	for (u_int u = 0u; u < 3u; ++u)
	{
		ZENITH_ASSERT_TRUE(
			xDecoded.m_xWorldPosition.m_afPosition[u] == xBuilt.m_afPosition[u],
			"position component %u did not survive the wire", u);
	}
	ZENITH_ASSERT_TRUE(xDecoded.m_xWorldPosition.m_fYaw == xBuilt.m_fYaw,
		"the yaw did not survive the wire");

	const ZM_RESUME_VALIDITY eBefore = ZM_ValidateResume(xBuilt, true);
	const ZM_RESUME_VALIDITY eAfter = ZM_ValidateResume(xDecoded.m_xWorldPosition, true);
	ZENITH_ASSERT_EQ((u_int)eAfter, (u_int)eBefore,
		"the resume verdict changed across the wire ('%s' -> '%s')",
		ZM_ResumeValidityName(eBefore), ZM_ResumeValidityName(eAfter));
	ZENITH_ASSERT_EQ((u_int)eAfter, (u_int)ZM_RESUME_VALID,
		"a captured Dawnmere pose must still be VALID after a save/load cycle");
}

// ============================================================================
// The milestone autosave policy (PURE)
// ============================================================================

ZENITH_TEST(ZM_Save, Autosave_TriggerNameIsTotalAndDistinct)
{
	ZENITH_ASSERT_EQ((u_int)ZM_AUTOSAVE_TRIGGER_COUNT, uALL_TRIGGER_COUNT,
		"a ZM_AUTOSAVE_TRIGGER arm was added or removed without updating this file");

	for (u_int u = 0u; u < uALL_TRIGGER_COUNT; ++u)
	{
		const char* szName = ZM_AutosaveTriggerName(aeALL_TRIGGERS[u]);
		ZENITH_ASSERT_NOT_NULL(szName, "ZM_AutosaveTriggerName(%u) is null", u);
		if (szName == nullptr) { continue; }
		ZENITH_ASSERT_TRUE(szName[0] != '\0', "ZM_AutosaveTriggerName(%u) is empty", u);
	}

	for (u_int uA = 0u; uA < uALL_TRIGGER_COUNT; ++uA)
	{
		for (u_int uB = uA + 1u; uB < uALL_TRIGGER_COUNT; ++uB)
		{
			const char* szA = ZM_AutosaveTriggerName(aeALL_TRIGGERS[uA]);
			const char* szB = ZM_AutosaveTriggerName(aeALL_TRIGGERS[uB]);
			if (szA == nullptr || szB == nullptr) { continue; }
			ZENITH_ASSERT_TRUE(strcmp(szA, szB) != 0,
				"triggers %u and %u share the name '%s' -- a copy-pasted switch arm",
				uA, uB, szA);
		}
	}

	const char* szOutOfRange =
		ZM_AutosaveTriggerName((ZM_AUTOSAVE_TRIGGER)((u_int)ZM_AUTOSAVE_TRIGGER_COUNT + 5u));
	ZENITH_ASSERT_NOT_NULL(szOutOfRange,
		"ZM_AutosaveTriggerName must never return null, even out of range");
	const char* szCountSentinel = ZM_AutosaveTriggerName(ZM_AUTOSAVE_TRIGGER_COUNT);
	ZENITH_ASSERT_NOT_NULL(szCountSentinel,
		"ZM_AutosaveTriggerName(COUNT) must never return null");
}

ZENITH_TEST(ZM_Save, Autosave_OnlySceneEnteredIsLiveToday)
{
	// The enum declares all five milestones NOW so it never has to be reordered,
	// but only SCENE_ENTERED has a producer in this sub-commit. A trigger switched
	// live before its system exists autosaves from code nothing can test.
	for (u_int u = 0u; u < uALL_TRIGGER_COUNT; ++u)
	{
		const ZM_AUTOSAVE_TRIGGER eTrigger = aeALL_TRIGGERS[u];
		const bool bExpectedLive = (eTrigger == ZM_AUTOSAVE_TRIGGER_SCENE_ENTERED);
		ZENITH_ASSERT_EQ(ZM_IsAutosaveTriggerLive(eTrigger), bExpectedLive,
			"ZM_IsAutosaveTriggerLive('%s')", ZM_AutosaveTriggerName(eTrigger));
	}

	// TOTAL, and it must fail CLOSED: a garbage trigger must not autosave.
	ZENITH_ASSERT_FALSE(
		ZM_IsAutosaveTriggerLive((ZM_AUTOSAVE_TRIGGER)((u_int)ZM_AUTOSAVE_TRIGGER_COUNT + 4u)),
		"an out-of-range trigger must not be live");
	ZENITH_ASSERT_FALSE(ZM_IsAutosaveTriggerLive(ZM_AUTOSAVE_TRIGGER_COUNT),
		"the COUNT sentinel must not be live");
}

ZENITH_TEST(ZM_Save, Autosave_NoneTriggerNeverSaves)
{
	// NONE is the "nothing happened" value. A default switch arm returning true
	// would make every frame that consults the policy write a save file.
	ZENITH_ASSERT_FALSE(
		ZM_ShouldAutosave(ZM_AUTOSAVE_TRIGGER_NONE,
			ZM_SaveSlots::ZM_SAVE_BLOCKER_NONE, false),
		"the NONE trigger must never autosave, even with nothing blocking");

	// Every declared-but-unshipped trigger is equally refused, with everything
	// clear -- so the refusal is coming from the trigger, not from a blocker.
	const ZM_AUTOSAVE_TRIGGER aeNotLive[] =
	{
		ZM_AUTOSAVE_TRIGGER_BADGE_EARNED,
		ZM_AUTOSAVE_TRIGGER_STORY_FLAG_SET,
		ZM_AUTOSAVE_TRIGGER_LEAGUE_ENTERED,
		ZM_AUTOSAVE_TRIGGER_TOWER_STREAK_BANKED,
	};
	for (u_int u = 0u; u < (u_int)(sizeof(aeNotLive) / sizeof(aeNotLive[0])); ++u)
	{
		ZENITH_ASSERT_FALSE(
			ZM_ShouldAutosave(aeNotLive[u], ZM_SaveSlots::ZM_SAVE_BLOCKER_NONE, false),
			"'%s' has no producer yet and must not autosave",
			ZM_AutosaveTriggerName(aeNotLive[u]));
	}

	// The control, so a hardcoded-false predicate cannot satisfy the above.
	ZENITH_ASSERT_TRUE(
		ZM_ShouldAutosave(ZM_AUTOSAVE_TRIGGER_SCENE_ENTERED,
			ZM_SaveSlots::ZM_SAVE_BLOCKER_NONE, false),
		"the LIVE trigger with nothing blocking must autosave");
}

// ---- One blocker per unit. A single combined truth table still passes with any
// one term missing from the predicate; this shape is the only one that reds an
// omission, and each unit carries its own all-clear control so it cannot be
// satisfied by a predicate that is simply always false. ----

ZENITH_TEST(ZM_Save, Autosave_BlockedByNotOverworld)
{
	ZENITH_ASSERT_FALSE(
		ZM_ShouldAutosave(ZM_AUTOSAVE_TRIGGER_SCENE_ENTERED,
			ZM_SaveSlots::ZM_SAVE_BLOCKER_NOT_OVERWORLD, false),
		"a non-overworld scene (FrontEnd / the additive battle scene) must block the "
		"autosave -- this is what stops quit-to-title writing a title-screen save");
	ZENITH_ASSERT_TRUE(
		ZM_ShouldAutosave(ZM_AUTOSAVE_TRIGGER_SCENE_ENTERED,
			ZM_SaveSlots::ZM_SAVE_BLOCKER_NONE, false),
		"control: the same trigger with nothing blocking must autosave");
}

ZENITH_TEST(ZM_Save, Autosave_BlockedByBattle)
{
	// The "When saves happen" policy, menu-save bullet (Docs/SaveFormat.md:421-425):
	// never mid-battle. A save taken while a battle
	// transition owns the screen restores into a half-torn-down battle.
	ZENITH_ASSERT_FALSE(
		ZM_ShouldAutosave(ZM_AUTOSAVE_TRIGGER_SCENE_ENTERED,
			ZM_SaveSlots::ZM_SAVE_BLOCKER_BATTLE, false),
		"an active battle transition must block the autosave");
	ZENITH_ASSERT_TRUE(
		ZM_ShouldAutosave(ZM_AUTOSAVE_TRIGGER_SCENE_ENTERED,
			ZM_SaveSlots::ZM_SAVE_BLOCKER_NONE, false),
		"control: the same trigger with nothing blocking must autosave");
}

ZENITH_TEST(ZM_Save, Autosave_BlockedByWarp)
{
	// ZM_GameStateManager::IsWarpInProgress is true for EVERY transition state
	// except IDLE, so a milestone autosave fired from inside the fade-in would
	// always resolve WARP and silently never save. This is the term that forces the
	// one-shot next-frame latch.
	ZENITH_ASSERT_FALSE(
		ZM_ShouldAutosave(ZM_AUTOSAVE_TRIGGER_SCENE_ENTERED,
			ZM_SaveSlots::ZM_SAVE_BLOCKER_WARP, false),
		"a warp in progress must block the autosave");
	ZENITH_ASSERT_TRUE(
		ZM_ShouldAutosave(ZM_AUTOSAVE_TRIGGER_SCENE_ENTERED,
			ZM_SaveSlots::ZM_SAVE_BLOCKER_NONE, false),
		"control: the same trigger with nothing blocking must autosave");
}

ZENITH_TEST(ZM_Save, Autosave_BlockedByPendingWhiteout)
{
	// The loss latch has not resolved yet: the party is still fainted and the
	// whiteout warp has not run. Saving here banks a state the game is about to
	// change out from under the player.
	ZENITH_ASSERT_FALSE(
		ZM_ShouldAutosave(ZM_AUTOSAVE_TRIGGER_SCENE_ENTERED,
			ZM_SaveSlots::ZM_SAVE_BLOCKER_PENDING_WHITEOUT, false),
		"an unresolved whiteout latch must block the autosave");
	ZENITH_ASSERT_TRUE(
		ZM_ShouldAutosave(ZM_AUTOSAVE_TRIGGER_SCENE_ENTERED,
			ZM_SaveSlots::ZM_SAVE_BLOCKER_NONE, false),
		"control: the same trigger with nothing blocking must autosave");
}

ZENITH_TEST(ZM_Save, Autosave_BlockedByMenuOpen)
{
	// The menu-open term is the ONE input that is not carried by ZM_SAVE_BLOCKER,
	// so it is the one most easily dropped when the predicate is refactored to
	// "just forward the blocker".
	ZENITH_ASSERT_FALSE(
		ZM_ShouldAutosave(ZM_AUTOSAVE_TRIGGER_SCENE_ENTERED,
			ZM_SaveSlots::ZM_SAVE_BLOCKER_NONE, true),
		"an open menu must block the autosave");
	ZENITH_ASSERT_TRUE(
		ZM_ShouldAutosave(ZM_AUTOSAVE_TRIGGER_SCENE_ENTERED,
			ZM_SaveSlots::ZM_SAVE_BLOCKER_NONE, false),
		"control: the same trigger with the menu closed must autosave");
}

ZENITH_TEST(ZM_Save, Autosave_OpensOnALiveTriggerWithEveryBlockerClear)
{
	// The single positive. Without it the whole policy could be `return false;` and
	// every blocker unit above would still be green.
	ZENITH_ASSERT_TRUE(
		ZM_ShouldAutosave(ZM_AUTOSAVE_TRIGGER_SCENE_ENTERED,
			ZM_SaveSlots::ZM_SAVE_BLOCKER_NONE, false),
		"a live trigger with every blocker clear and the menu closed MUST autosave -- "
		"otherwise the milestone autosave never fires at all");
}
