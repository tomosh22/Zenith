#include "Zenith.h"

// ============================================================================
// ZM_Tests_DawnmerePlacement (S7 item 3 SC8) -- the boot units that turn the
// authored rival's PLACEMENT from a comment into a checked property.
//
// PURE: no scene, no entity, no physics, no assets, no graphics. These run in the
// headless CI unit gate, which is precisely where the argument is needed -- the
// automated test that walks up to him needs baked terrain and can RequestSkip, so
// it cannot be the only place the whiteout-softlock claim lives.
// ============================================================================

#include <cmath>

#include "Core/Zenith_TestFramework.h"
#include "Maths/Zenith_Maths.h"
#include "Zenithmon/Source/Interaction/ZM_InteractionLogic.h"   // ZM_ForwardFromRotation
#include "Zenithmon/Source/Interaction/ZM_TrainerSightLogic.h"
#include "Zenithmon/Source/World/ZM_DawnmerePlacement.h"

namespace
{
	// The two XZ points the units reason about, built at the SAME Y so the vertical
	// band can never be what quietly saves a bad placement.
	constexpr float fPLACEMENT_TEST_Y = 26.0f;

	Zenith_Maths::Vector3 VesperPoint()
	{
		return Zenith_Maths::Vector3(
			fZM_DAWNMERE_VESPER_X, fPLACEMENT_TEST_Y, fZM_DAWNMERE_VESPER_Z);
	}

	Zenith_Maths::Vector3 SpawnPoint()
	{
		return Zenith_Maths::Vector3(
			fZM_DAWNMERE_TOWN_CENTER_X, fPLACEMENT_TEST_Y, fZM_DAWNMERE_TOWN_CENTER_Z);
	}

	float PlanarDistance(const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB)
	{
		const float fDeltaX = xA.x - xB.x;
		const float fDeltaZ = xA.z - xB.z;
		return std::sqrt(fDeltaX * fDeltaX + fDeltaZ * fDeltaZ);
	}
}

// THE WHITEOUT-SOFTLOCK GUARD, and it is not hypothetical. Losing a trainer battle
// routes through the WILD write-back (m_bPendingWhiteout), heals, and warps the
// player to the Dawnmere TownCenter (ZM_GameStateManager.cpp:177-186) WITHOUT
// setting RIVAL1_DEFEATED -- and ZM_MayTrainerEngage's flagged arm ignores the
// session latch (ZM_TrainerSightFsm.cpp:83-86). So a rival whose cone covered the
// respawn would re-challenge forever and the player could never reach the Care
// Center. RANGE is what prevents that, which is why clause (b) exists.
ZENITH_TEST(ZM_Interaction, Vesper_PlacementCannotSpawnCampOnTheWhiteoutTarget)
{
	const ZM_TrainerSightTuning xTuning;   // default-constructed IS the shipped tuning

	// (a) THE PROPERTY. A player who just whited out is NOT inside his cone.
	ZENITH_ASSERT_FALSE(
		ZM_IsTargetInTrainerSightFromRotation(
			VesperPoint(), ZM_DawnmereVesperFacing(), SpawnPoint(), xTuning),
		"the authored rival can see the whiteout respawn point -- an honest loss "
		"would softlock the game in a forced-battle loop");

	// (b) ANTI-VACUITY, and the whole point of the unit. He IS facing the spawn, so
	//     clause (a) is carried by RANGE alone rather than by an accidental facing
	//     that a future re-derivation could silently flip.
	const Zenith_Maths::Vector3 xForward =
		ZM_ForwardFromRotation(ZM_DawnmereVesperFacing());
	Zenith_Maths::Vector3 xToSpawn(
		SpawnPoint().x - VesperPoint().x, 0.0f, SpawnPoint().z - VesperPoint().z);
	const float fLength = std::sqrt(xToSpawn.x * xToSpawn.x + xToSpawn.z * xToSpawn.z);
	ZENITH_ASSERT_GT(fLength, 0.0f, "the rival is standing on the spawn");
	xToSpawn /= fLength;
	const float fDot = xForward.x * xToSpawn.x + xForward.z * xToSpawn.z;
	ZENITH_ASSERT_GT(fDot, fZM_SIGHT_MIN_FACING_DOT,
		"the rival is NOT facing the spawn (dot %.4f), so clause (a) above proves "
		"nothing about range -- re-derive the placement", fDot);

	// (c) the margin, spelled against the SHIPPED sight range rather than a literal.
	ZENITH_ASSERT_GT(PlanarDistance(VesperPoint(), SpawnPoint()),
		fZM_SIGHT_MAX_DISTANCE * 4.0f,
		"the rival must clear the whiteout respawn by a real margin");

	// (d) the z = 480 Home corridor ZM_PlayerHomeRoundTrip_Test drives BLIND.
	ZENITH_ASSERT_GT(
		std::fabs(fZM_DAWNMERE_VESPER_Z - fZM_DAWNMERE_TOWN_CENTER_Z),
		fZM_SIGHT_MAX_DISTANCE * 4.0f,
		"the rival is close enough to the z=480 traversal corridor to hijack a "
		"suite that never mentions him");
}

// The facing is DERIVED from the two authored anchors, and the argument order of
// that atan2 is load-bearing: atan2(x, z), matching ZM_ForwardFromRotation's +Z
// convention. Transposing it turns him 90 degrees, the approach never enters his
// cone, and the only symptom would be an automated test that times out naming a
// distance.
ZENITH_TEST(ZM_Interaction, Vesper_FacingIsDerivedFromTheTownCentreBearing)
{
	const float fExpectedYaw = std::atan2(
		fZM_DAWNMERE_TOWN_CENTER_X - fZM_DAWNMERE_VESPER_X,
		fZM_DAWNMERE_TOWN_CENTER_Z - fZM_DAWNMERE_VESPER_Z);
	ZENITH_ASSERT_EQ_FLOAT(ZM_DawnmereVesperYaw(), fExpectedYaw, 0.0001f,
		"the rival's yaw is no longer the town-centre bearing");

	// The quaternion really is that yaw about +Y (never eulerAngles).
	const Zenith_Maths::Vector3 xForward =
		ZM_ForwardFromRotation(ZM_DawnmereVesperFacing());
	ZENITH_ASSERT_EQ_FLOAT(xForward.x, std::sin(fExpectedYaw), 0.0005f,
		"the facing quaternion does not match the derived yaw");
	ZENITH_ASSERT_EQ_FLOAT(xForward.z, std::cos(fExpectedYaw), 0.0005f,
		"the facing quaternion does not match the derived yaw");

	// A point one metre in FRONT of him is in sight; the same metre BEHIND is not.
	// Without this the two clauses above would agree with any self-consistent
	// convention, including a transposed one.
	const Zenith_Maths::Vector3 xFront(
		VesperPoint().x + xForward.x, fPLACEMENT_TEST_Y, VesperPoint().z + xForward.z);
	const Zenith_Maths::Vector3 xBehind(
		VesperPoint().x - xForward.x, fPLACEMENT_TEST_Y, VesperPoint().z - xForward.z);
	const ZM_TrainerSightTuning xTuning;
	ZENITH_ASSERT_TRUE(ZM_IsTargetInTrainerSightFromRotation(
		VesperPoint(), ZM_DawnmereVesperFacing(), xFront, xTuning));
	ZENITH_ASSERT_FALSE(ZM_IsTargetInTrainerSightFromRotation(
		VesperPoint(), ZM_DawnmereVesperFacing(), xBehind, xTuning));
}
