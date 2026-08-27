#include "Zenith.h"

// ============================================================================
// ZM_Tests_Route1Placement (S8 item 2, R1-1) -- the boot units that turn Route
// 1's authored PLACEMENT from a header full of plausible numbers into a checked
// property, BEFORE any Route1.zscen exists to check it against.
//
// PURE: no scene, no entity, no physics, no assets, no graphics, no g_xEngine,
// no file I/O. Everything below reads COMPILED CONSTANTS -- ZM_Route1Placement.h,
// the Route 1 TERRAIN RECIPE, the compiled world table, and the SHIPPED sight and
// facing predicates (which are free functions over plain maths types, so calling
// them constructs nothing).
//
// ★ AND THESE UNITS CREATE NO ENTITY AND TOUCH NO REGISTRY. Both are hard
// constraints rather than style notes:
//   * Scene authoring bakes in the entity indices allocated during the boot the
//     unit runs in, so one entity-creating boot unit re-authors different .zscen
//     bytes for every committed scene in the game.
//   * Three EXISTING boot units call RegisterSceneBuildIndex(2, "<fake path>")
//     on the LIVE registry (ZM_Tests_WorldTraversal.cpp), and
//     RegisterSceneBuildIndex ASSERTS on a re-registration with a different path.
//     Nothing in this file goes near g_xEngine.Scenes(); the registration claims
//     belong to the ZM_SceneRegistry suite, which reads the COMPILED TABLE.
//
// ★ WHAT THESE EIGHT UNITS CANNOT DO, STATED UP FRONT. They run BEFORE the
// initial scene loads, and no Route1.zscen exists yet in any case. They cannot
// prove the scene was authored, that the terrain baked, or that the ground under
// an anchor is really the height that anchor's row of axZM_ROUTE1_GROUND_SAMPLES
// claims -- every row of that table is still SEEDED from the recipe mirror
// fZM_ROUTE1_RECIPE_TARGET_GROUND_Y, so NO compiled-constant unit in this game can
// tell a measured table from an unfrozen one. That last claim belongs to the
// raycast oracle in Tests/ZM_AutoTests_RouteGround.cpp. What they CAN do is
// compare two independently authored tables (the placement header against the
// terrain recipe), and run the SHIPPED sight geometry over the SHIPPED lane, which
// is the only check in this game that can see a trainer placed outside his own
// cone.
//
// ★ NOTHING HERE RE-SPELLS A CONSTANT FROM THE HEADER IT TESTS. A clause of the
// form ZENITH_ASSERT_EQ(fZM_ROUTE1_GATE_SCALE_Y, 4.0f) compares a literal with
// itself and cannot red on any change to anything. Every claim below is measured
// against an INDEPENDENT source: the terrain recipe's landmarks / pads / paths,
// the human body contract, the sight cone's own constants, or an arithmetic
// invariant of the accessor. The only bare literals in this file are the ones
// transcribed from OTHER files (see U10's player-name clause), the two
// component-type-table COUNTS U10 pins against the shared table in
// Tests/ZM_Tests_ComponentTypeNames.h (which the sibling Thornacre suite pins
// the same way), and the sampling density -- and all of them say so where
// they are spelled.
// ============================================================================

#include <cmath>     // sqrt / fabs / cos / sin / ceil
#include <cstring>   // strcmp / strstr -- the byte-identity and needle claims

#include "Core/Zenith_TestFramework.h"
#include "Maths/Zenith_Maths.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/Interaction/ZM_InteractionLogic.h"     // ZM_ForwardFromRotation / ZM_FlattenXZ -- the SHIPPED facing
#include "Zenithmon/Source/Interaction/ZM_TrainerSightLogic.h"    // the SHIPPED sight cone, constants and predicate
#include "Zenithmon/Source/World/ZM_HumanBody.h"                  // THE body contract -- by name, never by literal
#include "Zenithmon/Components/ZM_GroundItemProp.h"            // fDEFAULT_RADIUS -- a PURE static; nothing is constructed
#include "Zenithmon/Source/World/ZM_GroundItem.h"              // the prop REGISTRY -- the other half of the placement claim
#include "Zenithmon/Source/World/ZM_Route1Placement.h"
#include "Zenithmon/Source/World/ZM_TerrainAuthoring.h"           // the recipe. The UNITS may include this; the HEADER may not.
#include "Zenithmon/Tests/ZM_Tests_ComponentTypeNames.h"          // the ONE shared component-type-name inventory (ZM-57)

namespace
{
	// "These two floats are the same authored number." Generous by float
	// standards and far below any placement quantity Route 1 uses.
	constexpr float fROUTE1_EXACT_EPSILON = 1.0e-4f;

	// "These two anchors are the same authored POINT." One decade looser, because
	// an anchor is reconciled against a landmark authored in a different table by
	// a different hand -- this is an identity claim, not a rounding claim.
	constexpr float fROUTE1_ANCHOR_EPSILON = 1.0e-3f;

	// "This quaternion is still unit length." TIGHTER than the two above: the
	// frozen facing is four exact IEEE-754 bit patterns, so its squared length is
	// exactly 1 and any slack here is pure headroom against a future non-exact
	// rotation rather than a tolerance anything needs today.
	constexpr float fROUTE1_UNIT_QUAT_EPSILON = 1.0e-6f;

	// "The authored spawn clearance is EXACTLY one body half-extent." Tighter than
	// the equality epsilon because the ZM-D-184 gap is one half-extent added to one
	// half-extent and then subtracted back off -- exact to well under this. The same
	// figure Tests/ZM_Tests_ThornacrePlacement.cpp pins its twin gap at.
	constexpr float fROUTE1_CLEARANCE_EPSILON = 1.0e-5f;

	// How deep inside a pad's flattened disc an anchor has to land before this
	// file will call the ground under it levelled. Three quarters of the radius,
	// not the whole radius: a dab's influence falls off toward its rim, and an
	// anchor sitting exactly on the edge is a placement with no room to move.
	constexpr float fROUTE1_PAD_CONTAINMENT_FRACTION = 0.75f;

	// The lane path the whole route is walked along, and the two pads the two
	// arrival markers sit in. Spelled ONCE here as lookup keys rather than at
	// eight call sites; the recipe owns the geometry behind each name.
	constexpr const char* szROUTE1_LANE_PATH_NAME   = "DirtLane";
	constexpr const char* szROUTE1_SOUTH_PAD_NAME   = "SouthGate";
	constexpr const char* szROUTE1_NORTH_PAD_NAME   = "NorthGate";
	constexpr const char* szROUTE1_SOUTH_LANDMARK   = "FromDawnmere";
	constexpr const char* szROUTE1_NORTH_LANDMARK   = "FromThornacre";

	// ★ NOT A SHIPPED ANCHOR. The recipe landmark U8 feeds through its OWN
	// containment predicate as the anti-vacuity arm: it names a point out in the
	// wild encounter fields, hundreds of metres off the lane, and it must FAIL the
	// clause the two real stations pass. Nothing outside this file may read it.
	constexpr const char* szROUTE1_OFF_LANE_LANDMARK = "TutorialFields";

	// ★ SHARED WITH Tests/ZM_Tests_ThornacrePlacement.cpp, in
	// Tests/ZM_Tests_ComponentTypeNames.h (landed by ZM-57 -- see that header for
	// the full rationale: why this is a deliberate COPY of the registries rather
	// than a live g_xEngine read, and why the anti-vacuity counts below stay
	// local to each file rather than moving into the header too). Aliased under
	// this file's existing names so every clause below is unchanged.
	constexpr auto& aszROUTE1_ENGINE_TYPE_NAMES = aszZM_ENGINE_COMPONENT_TYPE_NAMES;
	constexpr auto& aszROUTE1_GAME_TYPE_NAMES = aszZM_GAME_COMPONENT_TYPE_NAMES;

	// ★ THE ONE TYPE NAME THAT SWALLOWS AN ENTITY NAME, NAMED SO THE EXCLUSION IS
	// EXPLICIT. "Player" is a strict substring of this, and the player entity MUST
	// be called "Player" (see U10). Spelled separately rather than fished out of
	// the array above so the exclusion reads as a decision, not as an omission.
	constexpr const char* szROUTE1_TYPE_CONTAINING_THE_PLAYER_NAME =
		"ZM_PlayerController";

	// ★ WHAT THIS LITERAL IS FOR CHANGED AT Q-2026-08-15-002, AND THE OLD REASON IS
	// GONE. It used to be transcribed from ZM_FollowCamera.cpp, which acquired its
	// subject as FindEntityByName("Player") -- so this was an INDEPENDENT source and
	// a mismatch was a permanent black screen. The camera now acquires the unique
	// ZM_PlayerController in its own scene and never reads the name at all.
	//
	// The clause below is KEPT anyway, on a narrower and still-live claim: every
	// shipped scene spells its player the same way (szZM_PROFLAB_PLAYER_ENTITY_NAME
	// and the rest), and that shared spelling is what makes "Player" a strict
	// substring of the serialized type name "ZM_PlayerController" -- which is why
	// every committed-bytes needle on it must take the STRICTLY-MORE form rather
	// than an equality. A scene that spelled it differently would silently make
	// that reasoning wrong for one scene only. This pins a CONVENTION now, not a
	// contract -- do not re-describe it as one.
	constexpr const char* szROUTE1_PLAYER_NAME_CONVENTION = "Player";

	// ★ NOT SHIPPED NAMES. The pair U10 feeds through its OWN substring predicate
	// as an anti-vacuity arm: a marker named after the gate beside it is exactly
	// the FromLab / FromLabSpawn trap, where a byte needle on the shorter name
	// silently counts the longer one too.
	constexpr const char* szROUTE1_SWALLOWED_NEEDLE = "Route1South";
	constexpr const char* szROUTE1_SWALLOWING_NAME  = "Route1SouthArrival";

	// ---- Recipe lookups ------------------------------------------------------
	//
	// All four return nullptr rather than asserting: a missing table entry is a
	// failure this file must REPORT, and Zenith_Assert would end the whole boot
	// unit run instead.

	const ZM_TerrainLandmarkSpec* Route1FindLandmark(const char* szName)
	{
		const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetRoute1TerrainRecipe();
		if (xRecipe.m_pxLandmarks == nullptr || szName == nullptr)
		{
			return nullptr;
		}
		for (u_int uIndex = 0u; uIndex < xRecipe.m_uLandmarkCount; ++uIndex)
		{
			const ZM_TerrainLandmarkSpec& xLandmark = xRecipe.m_pxLandmarks[uIndex];
			if (xLandmark.m_szName != nullptr
				&& std::strcmp(xLandmark.m_szName, szName) == 0)
			{
				return &xLandmark;
			}
		}
		return nullptr;
	}

	const ZM_TerrainPadSpec* Route1FindPad(const char* szName)
	{
		const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetRoute1TerrainRecipe();
		if (xRecipe.m_pxPads == nullptr || szName == nullptr)
		{
			return nullptr;
		}
		for (u_int uIndex = 0u; uIndex < xRecipe.m_uPadCount; ++uIndex)
		{
			const ZM_TerrainPadSpec& xPad = xRecipe.m_pxPads[uIndex];
			if (xPad.m_szName != nullptr && std::strcmp(xPad.m_szName, szName) == 0)
			{
				return &xPad;
			}
		}
		return nullptr;
	}

	const ZM_TerrainPathSpec* Route1FindPath(const char* szName)
	{
		const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetRoute1TerrainRecipe();
		if (xRecipe.m_pxPaths == nullptr || szName == nullptr)
		{
			return nullptr;
		}
		for (u_int uIndex = 0u; uIndex < xRecipe.m_uPathCount; ++uIndex)
		{
			const ZM_TerrainPathSpec& xPath = xRecipe.m_pxPaths[uIndex];
			if (xPath.m_szName != nullptr && std::strcmp(xPath.m_szName, szName) == 0)
			{
				return &xPath;
			}
		}
		return nullptr;
	}

	// The DirtLane's FLATTEN radius -- the corridor whose ground the bake levels to
	// the recipe's target height, and (because the GRASS_ERASE phase runs the same
	// corridor at density 0) the corridor in which nothing can be standing in
	// encounter grass. Zero when the path is missing, which every caller's
	// containment clause then fails loudly on rather than passing vacuously.
	float Route1LaneFlattenRadius()
	{
		const ZM_TerrainPathSpec* pxLane = Route1FindPath(szROUTE1_LANE_PATH_NAME);
		return pxLane != nullptr ? pxLane->m_fFlattenRadius : 0.0f;
	}

	// ---- Lane geometry -------------------------------------------------------

	// The foot of the perpendicular from an XZ point onto the DirtLane polyline:
	// the closest point on it, and how far away it is. TOTAL -- an absent or
	// single-point path yields m_bValid false rather than a divide by zero.
	struct Route1LaneFoot
	{
		float m_fDistance = 0.0f;
		float m_fX = 0.0f;
		float m_fZ = 0.0f;
		bool m_bValid = false;
	};

	Route1LaneFoot Route1ClosestPointOnLane(float fX, float fZ)
	{
		Route1LaneFoot xFoot;
		const ZM_TerrainPathSpec* pxLane = Route1FindPath(szROUTE1_LANE_PATH_NAME);
		if (pxLane == nullptr || pxLane->m_pxPoints == nullptr
			|| pxLane->m_uPointCount < 2u)
		{
			return xFoot;
		}

		float fBestDistanceSquared = -1.0f;
		for (u_int uSegment = 0u; uSegment + 1u < pxLane->m_uPointCount; ++uSegment)
		{
			const ZM_TerrainPoint2& xA = pxLane->m_pxPoints[uSegment];
			const ZM_TerrainPoint2& xB = pxLane->m_pxPoints[uSegment + 1u];
			const float fDeltaX = xB.m_fX - xA.m_fX;
			const float fDeltaZ = xB.m_fZ - xA.m_fZ;
			const float fLengthSquared = fDeltaX * fDeltaX + fDeltaZ * fDeltaZ;
			if (!(fLengthSquared > 0.0f))
			{
				continue;   // a degenerate segment has no closest point of its own
			}

			// The projection parameter, CLAMPED to the segment: an unclamped
			// projection would measure against the infinite line and report a
			// station beside the route as being on it.
			float fT = ((fX - xA.m_fX) * fDeltaX + (fZ - xA.m_fZ) * fDeltaZ)
				/ fLengthSquared;
			if (fT < 0.0f) { fT = 0.0f; }
			if (fT > 1.0f) { fT = 1.0f; }

			const float fFootX = xA.m_fX + fDeltaX * fT;
			const float fFootZ = xA.m_fZ + fDeltaZ * fT;
			const float fToFootX = fX - fFootX;
			const float fToFootZ = fZ - fFootZ;
			const float fDistanceSquared = fToFootX * fToFootX + fToFootZ * fToFootZ;
			if (fBestDistanceSquared < 0.0f || fDistanceSquared < fBestDistanceSquared)
			{
				fBestDistanceSquared = fDistanceSquared;
				xFoot.m_fX = fFootX;
				xFoot.m_fZ = fFootZ;
				xFoot.m_bValid = true;
			}
		}

		if (xFoot.m_bValid)
		{
			xFoot.m_fDistance = std::sqrt(fBestDistanceSquared);
		}
		return xFoot;
	}

	float Route1DistanceToLane(float fX, float fZ)
	{
		const Route1LaneFoot xFoot = Route1ClosestPointOnLane(fX, fZ);
		// An invalid foot returns a HUGE distance rather than zero: every clause
		// that consumes this is an upper bound, so failing closed means failing.
		return xFoot.m_bValid ? xFoot.m_fDistance : 1.0e9f;
	}

	// ---- The sight geometry --------------------------------------------------

	// ★★ THE INDEPENDENT ORACLE FOR EVERY TRAINER ANCHOR IN THIS FILE, DERIVED
	// FROM THE SHIPPED SIGHT CONSTANTS AND NOTHING ELSE.
	//
	// A trainer staring straight down the route sees a player on the walked line at
	// lateral offset d only if the along-forward component is at least
	// fZM_SIGHT_MIN_FACING_DOT of the range, i.e. only if the range r satisfies
	// BOTH d <= r * sin(halfAngle) and r <= fZM_SIGHT_MAX_DISTANCE. Those two
	// windows overlap exactly while
	//
	//     d <= fZM_SIGHT_MAX_DISTANCE * sin(acos(fZM_SIGHT_MIN_FACING_DOT))
	//        = 8.0 * sqrt(1 - 0.75) = 4.0 m
	//
	// so a station further than this from the lane can NEVER be triggered, in any
	// playthrough, forever -- while the terrain, the scene bytes and every other
	// placement check stay perfectly green. This is computed from the constants
	// rather than written as 4.0f so that widening or narrowing the shipped cone
	// moves the ceiling and the anchors' headroom together.
	float Route1SightLateralCeiling()
	{
		// The cone's non-degeneracy is a COMPILE-TIME fact about the shipped
		// constants, so it is a static_assert and not a runtime branch: an `if`
		// over a constant expression is C4127, which this build treats as an
		// error. Asserting it at compile time is also strictly stronger than the
		// runtime fallback it replaces -- a degenerate cone now fails the BUILD
		// instead of quietly yielding a ceiling every anchor below is measured
		// against.
		static_assert(fZM_SIGHT_MIN_FACING_DOT > -1.0f && fZM_SIGHT_MIN_FACING_DOT < 1.0f,
			"The shipped sight cone must be non-degenerate (|cos(halfAngle)| < 1). "
			"At |cos| == 1 the cone has zero width, no lateral offset is ever "
			"visible, and every trainer-anchor check in this file is vacuous.");

		const float fSineSquared = 1.0f - fZM_SIGHT_MIN_FACING_DOT * fZM_SIGHT_MIN_FACING_DOT;
		return fZM_SIGHT_MAX_DISTANCE * std::sqrt(fSineSquared);
	}

	// A walking player's body CENTRE at a point on the lane, standing on the SAME
	// MEASURED GROUND as the station that is doing the observing.
	//
	// ★★ THE HEIGHT IS THE OBSERVER'S OWN MEASURED COLUMN, NOT THE RECIPE MIRROR,
	// AND THAT MAKES THE SHARED-PLANE PREMISE TRUE BY CONSTRUCTION RATHER THAN
	// ASSUMED. The shipped sight predicate measures its cone centre-to-centre, so any
	// height difference between the two bodies eats into the cone. Taking this height
	// from fZM_ROUTE1_RECIPE_TARGET_GROUND_Y while ZM_GetRoute1TrainerCentre takes
	// its own from axZM_ROUTE1_GROUND_SAMPLES would compute the vertical band from
	// TWO DIFFERENT GROUNDS the moment that table is frozen -- silently, because the
	// mirror and every seeded row are the same number until then.
	//
	// It is a claim about a walk beside ONE station, so it takes that station: the
	// lane corridor's flatten levels the walked line to the same ground the station
	// stands on, and reading the station's row is how that sentence is spelled once
	// instead of assumed twice.
	Zenith_Maths::Vector3 Route1LaneWalkerCentre(
		float fX, float fZ, ZM_ROUTE1_TRAINER_ID eObserver)
	{
		return Zenith_Maths::Vector3(
			fX,
			ZM_GetRoute1TrainerFeet(eObserver).y + fZM_HUMAN_BODY_HALF_HEIGHT,
			fZ);
	}

	struct Route1SightSampleCount
	{
		u_int m_uInCone = 0u;
		u_int m_uTotal = 0u;
	};

	// Walk the DirtLane at a fixed SPACING and count how many sampled body centres
	// the SHIPPED predicate says this observer can see.
	//
	// ★ THE DENSITY IS PER METRE, NOT PER SEGMENT, AND THAT IS LOAD-BEARING. The
	// lane's five segments run from 238 m to 393 m, so a fixed count PER SEGMENT
	// gives each a different spacing and makes an in-cone COUNT threshold mean a
	// different arc on every one of them. Pinning the spacing makes the count a
	// linear function of the walked arc, which is the quantity the claim is about.
	//
	// ★ AND IT RUNS ZM_IsTargetInTrainerSightFromRotation, THE SHIPPED PREDICATE,
	// FROM THE ROTATION. Re-deriving the cone here would let this file and the game
	// disagree about what "seen" means while both stayed green -- and taking the
	// rotation rather than a forward vector exercises ZM_ForwardFromRotation too,
	// which is the step a facing typo lands in.
	//
	// ★ THE OBSERVING STATION RIDES ALONGSIDE ITS CENTRE, rather than being derived
	// from it, because the anti-vacuity arm below DISPLACES that centre laterally
	// while keeping the station's ground. The walked line's height must follow the
	// station's measured column either way, so the station is named rather than
	// reverse-engineered from a position that may have been moved.
	Route1SightSampleCount Route1CountSightSamplesAlongLane(
		const Zenith_Maths::Vector3& xObserverCentre,
		const Zenith_Maths::Quat& xFacing,
		ZM_ROUTE1_TRAINER_ID eObserver,
		u_int uSamplesPerMetre)
	{
		Route1SightSampleCount xCount;
		const ZM_TerrainPathSpec* pxLane = Route1FindPath(szROUTE1_LANE_PATH_NAME);
		if (pxLane == nullptr || pxLane->m_pxPoints == nullptr
			|| pxLane->m_uPointCount < 2u || uSamplesPerMetre == 0u)
		{
			return xCount;
		}

		// Default-constructed IS the shipped tuning (ZM_TrainerSightLogic.h), so no
		// threshold is respelled here.
		const ZM_TrainerSightTuning xTuning;

		for (u_int uSegment = 0u; uSegment + 1u < pxLane->m_uPointCount; ++uSegment)
		{
			const ZM_TerrainPoint2& xA = pxLane->m_pxPoints[uSegment];
			const ZM_TerrainPoint2& xB = pxLane->m_pxPoints[uSegment + 1u];
			const float fDeltaX = xB.m_fX - xA.m_fX;
			const float fDeltaZ = xB.m_fZ - xA.m_fZ;
			const float fLength = std::sqrt(fDeltaX * fDeltaX + fDeltaZ * fDeltaZ);

			u_int uSteps = static_cast<u_int>(
				std::ceil(fLength * static_cast<float>(uSamplesPerMetre)));
			if (uSteps == 0u)
			{
				uSteps = 1u;
			}

			for (u_int uStep = 0u; uStep <= uSteps; ++uStep)
			{
				const float fT =
					static_cast<float>(uStep) / static_cast<float>(uSteps);
				const Zenith_Maths::Vector3 xWalker = Route1LaneWalkerCentre(
					xA.m_fX + fDeltaX * fT, xA.m_fZ + fDeltaZ * fT, eObserver);
				++xCount.m_uTotal;
				if (ZM_IsTargetInTrainerSightFromRotation(
					xObserverCentre, xFacing, xWalker, xTuning))
				{
					++xCount.m_uInCone;
				}
			}
		}
		return xCount;
	}

	// Push a station straight out along its own perpendicular until it stands
	// fOffset from the lane, keeping its height and its side. Used ONLY to build
	// U9b's anti-vacuity probe: a station displaced past the geometric ceiling
	// above must see NOTHING, or the counting clause cannot red.
	Zenith_Maths::Vector3 Route1LateralProbeCentre(
		const Zenith_Maths::Vector3& xStationCentre, float fOffset)
	{
		const Route1LaneFoot xFoot =
			Route1ClosestPointOnLane(xStationCentre.x, xStationCentre.z);
		if (!xFoot.m_bValid || !(xFoot.m_fDistance > 0.0f))
		{
			return xStationCentre;   // TOTAL: on the lane, there is no side to pick
		}
		const float fDirectionX = (xStationCentre.x - xFoot.m_fX) / xFoot.m_fDistance;
		const float fDirectionZ = (xStationCentre.z - xFoot.m_fZ) / xFoot.m_fDistance;
		return Zenith_Maths::Vector3(
			xFoot.m_fX + fDirectionX * fOffset,
			xStationCentre.y,
			xFoot.m_fZ + fDirectionZ * fOffset);
	}

	// ---- Volume predicates ---------------------------------------------------

	// The clear air a gate's NEAR FACE leaves the arriving body, along the walked
	// axis. Two spellings because the two gates face opposite ways down the route;
	// a single signed helper would hide which side each gate is on, which is the
	// half of the claim that matters.
	float Route1SouthGateClearanceToMarker(const ZM_Route1Volume& xGate, float fMarkerZ)
	{
		return fMarkerZ - xGate.Max().z;
	}

	float Route1NorthGateClearanceToMarker(const ZM_Route1Volume& xGate, float fMarkerZ)
	{
		return xGate.Min().z - fMarkerZ;
	}

	// "The box rests ON the ground under IT" -- its underside lands on that ground,
	// not its centre. A gate authored at world Y 0 is a tunnel 26 m below the player.
	//
	// ★★ THE EXPECTED GROUND IS A PARAMETER, AND A SINGLE SHARED EXPECTATION WOULD
	// NOW BE WRONG. The two sensors stand 1336 m apart on TWO DIFFERENT measured
	// columns of axZM_ROUTE1_GROUND_SAMPLES. One shared height would rest one gate on
	// the OTHER's ground the instant that table is frozen -- part-buried at one end
	// and hovering at the other, with every scale constant in the header still
	// reading exactly as intended, and a failure message that named a plane neither
	// gate stands on. Each caller passes its OWN column.
	bool Route1VolumeRestsOnGround(const ZM_Route1Volume& xVolume, float fGroundFeetY)
	{
		return std::fabs(xVolume.Min().y - fGroundFeetY) <= fROUTE1_EXACT_EPSILON;
	}

	// ---- Camera --------------------------------------------------------------

	// The settled camera pose recomputed for an ARBITRARY pitch, so U11 can drive a
	// sign-flipped pitch through the identical arithmetic. It is asserted to agree
	// with the SHIPPED accessor at the shipped pitch before the flipped value is
	// trusted -- otherwise this would be a second implementation checking itself.
	Zenith_Maths::Vector3 Route1CameraPositionForPitch(float fPitch)
	{
		Zenith_Maths::Vector3 xPivot = ZM_GetRoute1SouthArrivalBodyCentre();
		xPivot.y += fZM_ROUTE1_CAMERA_PIVOT_HEIGHT;

		const float fCosPitch = std::cos(fPitch);
		const Zenith_Maths::Vector3 xForward(
			std::sin(fZM_ROUTE1_CAMERA_YAW) * fCosPitch,
			std::sin(fPitch),
			std::cos(fZM_ROUTE1_CAMERA_YAW) * fCosPitch);

		return xPivot - xForward * fZM_ROUTE1_CAMERA_ARM;
	}

	// ---- Names ---------------------------------------------------------------

	// Every printable, space-free character is a legal scene entity name byte. The
	// names are the lookup keys a later slice's FindEntityByName walk and the
	// committed-bytes needles use, so a stray space or control byte is a silent
	// miss rather than a diagnosable failure.
	bool Route1NameIsALookupKey(const char* szName)
	{
		if (szName == nullptr || szName[0] == '\0')
		{
			return false;
		}
		for (u_int uIndex = 0u; szName[uIndex] != '\0'; ++uIndex)
		{
			const unsigned char uCharacter =
				static_cast<unsigned char>(szName[uIndex]);
			if (uCharacter <= 0x20u || uCharacter > 0x7eu)
			{
				return false;
			}
		}
		return true;
	}

	// "Needle A is swallowed by string B." The ONE spelling of the substring test,
	// so the shipped pairs and the anti-vacuity pair below cannot be checked by
	// two different predicates.
	bool Route1NeedleIsSwallowedBy(const char* szNeedle, const char* szHaystack)
	{
		if (szNeedle == nullptr || szHaystack == nullptr)
		{
			return false;
		}
		return std::strstr(szHaystack, szNeedle) != nullptr;
	}

	// Every entity name Route 1 authors, in one place.
	//
	// ★ THE SCENE NAME IS DELIBERATELY ABSENT. "Route1" is a strict substring of
	// every other name in this table, so including szZM_ROUTE1_SCENE_NAME would red
	// the pairwise clause on the first pair it reached -- correctly, and uselessly.
	// The scene name is not an entity and no needle counts it.
	//
	// ★ AND THE TWO TRAINER NAMES ARE ALREADY HERE. ZM_GetRoute1TrainerStation
	// hands back these same constants, so walking the stations as well would
	// double-count them rather than adding a check.
	const char* const aszROUTE1_ENTITY_NAMES[] =
	{
		szZM_ROUTE1_TERRAIN_ENTITY_NAME,
		szZM_ROUTE1_PLAYER_ENTITY_NAME,
		szZM_ROUTE1_CAMERA_ENTITY_NAME,
		szZM_ROUTE1_SOUTH_ARRIVAL_ENTITY_NAME,
		szZM_ROUTE1_NORTH_ARRIVAL_ENTITY_NAME,
		szZM_ROUTE1_SOUTH_GATE_ENTITY_NAME,
		szZM_ROUTE1_NORTH_GATE_ENTITY_NAME,
		szZM_ROUTE1_TRAINER_RAMBLER_ENTITY_NAME,
		szZM_ROUTE1_TRAINER_FORAGER_ENTITY_NAME,
	};

	// Matching the CountOf idiom ZM_TerrainAuthoring.cpp already uses: the array
	// bound deduces as size_t and is narrowed once, here, rather than at every
	// call site.
	template<typename T, size_t N>
	constexpr u_int Route1CountOf(const T (&)[N])
	{
		return static_cast<u_int>(N);
	}
}

// ============================================================================
// U5 -- THE ARRIVAL MARKERS SIT ON THE RECIPE LANDMARKS THE TERRAIN FLATTENED.
//
// Two independently authored tables are compared here: ZM_Route1Placement.h's
// four anchor floats against the Route 1 TERRAIN RECIPE's landmark and pad
// tables. Neither reads the other -- the placement header deliberately does not
// include ZM_TerrainAuthoring.h -- so this is a reconciliation, not a
// re-spelling.
//
// WHY IT MATTERS: fZM_ROUTE1_RECIPE_TARGET_GROUND_Y is NOT A MEASUREMENT and must
// never be given one. It MIRRORS the recipe's m_fTargetHeight -- the height the
// FLATTEN dabs drive their pads and paths TO -- and it is a true statement about
// the ground ONLY where a flatten dab actually reached. Move an anchor out of a
// flattened disc and the number becomes a lie no compiled-constant unit can see:
// the arriving body either floats above the hillside or spawns inside it. That is
// why this unit reconciles the MIRROR against the recipe and nothing here reads
// the measured table: every row of axZM_ROUTE1_GROUND_SAMPLES is still SEEDED
// from this same mirror, so pointing these clauses at it would swap a
// recipe-vs-recipe reconciliation for a recipe-vs-raycast comparison that reds for
// a reason its message does not name. The raycast oracle in
// Tests/ZM_AutoTests_RouteGround.cpp is what replaces the seeds with measurements.
//
// ★ AND THE ZM-D-184 SPAWN CLEARANCE RIDES HERE, in clause (3). It is the same
// ground plane the clauses above reconcile, read one step further up: how far
// ABOVE it the authoring lifts a dynamic body before physics takes over. It has no
// unit of its own because it has exactly one number in it and it belongs to the
// anchor, not to a second subject.
// ============================================================================

ZENITH_TEST(ZM_WorldTraversal, Route1_ArrivalMarkersSitOnTheRecipeLandmarksTheTerrainFlattened)
{
	const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetRoute1TerrainRecipe();

	// (0) ANTI-VACUITY. Every clause below walks one of these two tables; an empty
	//     one would make the whole unit pass while proving nothing.
	ZENITH_ASSERT_GT(xRecipe.m_uLandmarkCount, 0u,
		"the Route 1 recipe declares no landmarks at all, so every reconciliation "
		"below would pass by finding nothing to disagree with");
	ZENITH_ASSERT_GT(xRecipe.m_uPadCount, 0u,
		"the Route 1 recipe declares no pads at all -- there is no flattened ground "
		"for an arrival marker to stand on");

	// (1) THE RECIPE MIRROR IS STILL THE RECIPE'S TARGET HEIGHT. This is the entire
	//     licence for that constant: it is not a measurement, it is the value the
	//     bake drives flattened ground to, and if the recipe re-levels the route the
	//     mirror describes a plane that no longer exists.
	//
	//     ★ THE SUBJECT HERE IS THE MIRROR, NOT axZM_ROUTE1_GROUND_SAMPLES. The
	//     measured table is what "the real ground under this anchor" reads, and it is
	//     reconciled against the world by a raycast, never against the recipe.
	ZENITH_ASSERT_EQ_FLOAT(fZM_ROUTE1_RECIPE_TARGET_GROUND_Y, xRecipe.m_fTargetHeight,
		fROUTE1_EXACT_EPSILON,
		"the placement header's recipe-target ground MIRROR is %.4f while the Route 1 "
		"recipe flattens to %.4f. The mirror is what seeds every row of "
		"axZM_ROUTE1_GROUND_SAMPLES and what the landmark clause below is reconciled "
		"against, so the arriving player, both gates and both trainers would all "
		"stand at the wrong height. Fix the mirror to match the recipe -- never give "
		"it a measured value",
		(double)fZM_ROUTE1_RECIPE_TARGET_GROUND_Y, (double)xRecipe.m_fTargetHeight);

	// (2) Each marker sits on the landmark named after its inbound tag, and inside
	//     the pad that flattens the ground there.
	struct Route1ArrivalClaim
	{
		const char* m_szLandmark;
		const char* m_szPad;
		Zenith_Maths::Vector3 m_xFeet;
		const char* m_szWhich;
	};

	const Route1ArrivalClaim axClaims[] =
	{
		{ szROUTE1_SOUTH_LANDMARK, szROUTE1_SOUTH_PAD_NAME,
			ZM_GetRoute1SouthArrivalFeet(), "south" },
		{ szROUTE1_NORTH_LANDMARK, szROUTE1_NORTH_PAD_NAME,
			ZM_GetRoute1NorthArrivalFeet(), "north" },
	};

	for (u_int uClaim = 0u; uClaim < Route1CountOf(axClaims); ++uClaim)
	{
		const Route1ArrivalClaim& xClaim = axClaims[uClaim];

		const ZM_TerrainLandmarkSpec* pxLandmark =
			Route1FindLandmark(xClaim.m_szLandmark);
		ZENITH_ASSERT_NOT_NULL(pxLandmark,
			"the Route 1 recipe declares no landmark '%s', so the %s arrival marker "
			"stands on a point the terrain bake never levelled",
			xClaim.m_szLandmark, xClaim.m_szWhich);
		if (pxLandmark == nullptr)
		{
			continue;
		}

		ZENITH_ASSERT_EQ_FLOAT(xClaim.m_xFeet.x, pxLandmark->m_xPosition.m_fX,
			fROUTE1_ANCHOR_EPSILON,
			"the %s arrival marker's X is %.4f while recipe landmark '%s' is at "
			"%.4f -- the marker and the flattened ground have parted company",
			xClaim.m_szWhich, (double)xClaim.m_xFeet.x, xClaim.m_szLandmark,
			(double)pxLandmark->m_xPosition.m_fX);
		ZENITH_ASSERT_EQ_FLOAT(xClaim.m_xFeet.z, pxLandmark->m_xPosition.m_fZ,
			fROUTE1_ANCHOR_EPSILON,
			"the %s arrival marker's Z is %.4f while recipe landmark '%s' is at "
			"%.4f", xClaim.m_szWhich, (double)xClaim.m_xFeet.z, xClaim.m_szLandmark,
			(double)pxLandmark->m_xPosition.m_fZ);

		// The landmark's OWN Y is a third, independent statement of the same
		// FLATTEN TARGET -- the recipe author wrote it beside the XZ, and it agreeing
		// with the mirror is what makes clause (1) a claim about this route and not
		// just about two copies of one number.
		//
		// ★ THE MIRROR AGAIN, DELIBERATELY. A recipe landmark declares where the
		// bake is told to LEVEL to; it is not a measurement of where the bake left
		// the surface, so the measured table is not its counterpart.
		ZENITH_ASSERT_EQ_FLOAT(pxLandmark->m_xPosition.m_fY,
			fZM_ROUTE1_RECIPE_TARGET_GROUND_Y, fROUTE1_EXACT_EPSILON,
			"recipe landmark '%s' declares height %.4f, not the route's flatten target "
			"%.4f -- the recipe now disagrees with itself about where it is levelling "
			"the ground to",
			xClaim.m_szLandmark, (double)pxLandmark->m_xPosition.m_fY,
			(double)fZM_ROUTE1_RECIPE_TARGET_GROUND_Y);

		// ...and the marker is genuinely INSIDE the pad that flattens it, not
		// merely near a landmark that happens to carry the right numbers.
		const ZM_TerrainPadSpec* pxPad = Route1FindPad(xClaim.m_szPad);
		ZENITH_ASSERT_NOT_NULL(pxPad,
			"the Route 1 recipe declares no pad '%s', so nothing flattens the ground "
			"the %s arrival marker stands on", xClaim.m_szPad, xClaim.m_szWhich);
		if (pxPad == nullptr)
		{
			continue;
		}

		const float fToPadX = xClaim.m_xFeet.x - pxPad->m_xCentre.m_fX;
		const float fToPadZ = xClaim.m_xFeet.z - pxPad->m_xCentre.m_fZ;
		const float fToPad = std::sqrt(fToPadX * fToPadX + fToPadZ * fToPadZ);
		const float fLimit = pxPad->m_fFlattenRadius * fROUTE1_PAD_CONTAINMENT_FRACTION;
		ZENITH_ASSERT_LE(fToPad, fLimit,
			"the %s arrival marker stands %.4f m from pad '%s' (flatten radius "
			"%.4f), outside the %.4f m the ground is levelled well inside. The "
			"flatten target this marker's ground row is seeded from is only true "
			"where a FLATTEN dab reached, so an arriving player would float above the "
			"hillside or spawn inside it",
			xClaim.m_szWhich, (double)fToPad, xClaim.m_szPad,
			(double)pxPad->m_fFlattenRadius, (double)fLimit);
	}

	// (3) ★ ZM-D-184: THE AUTHORED PLAYER STARTS EXACTLY ONE HALF-EXTENT CLEAR.
	//     ZM_GetRoute1AuthoredPlayerCentre had NO READER AT ALL before this clause,
	//     so a Route-1-side spawn-clearance error would have shipped with every unit
	//     in this file green -- the very hole the sibling Thornacre suite already
	//     closes. Stated as a DIFFERENCE OF THE TWO ACCESSORS, exactly as Thornacre
	//     states its own, so it follows any change to the ground plane instead of
	//     re-spelling one.
	//
	//     ★ THE MUTATIONS IT CATCHES, BOTH REAL. A clearance of ZERO is the ZM-D-184
	//     substep-burst fall-through: a dynamic body authored at exact contact with
	//     the terrain bursts physics substeps on its first frame and drops straight
	//     THROUGH the ground -- that is how Vesper vanished, with every compiled
	//     constant reading correctly. TWO half-extents is the opposite mistake: the
	//     body is floated a full body height up and visibly falls the moment the
	//     route loads.
	const float fAuthoredPlayerClearance = ZM_GetRoute1AuthoredPlayerCentre().y
		- ZM_GetRoute1SouthArrivalBodyCentre().y;
	ZENITH_ASSERT_EQ_FLOAT(fAuthoredPlayerClearance, fZM_HUMAN_BODY_HALF_HEIGHT,
		fROUTE1_CLEARANCE_EPSILON,
		"the authored Route 1 player starts %.5f m above its resting body centre; "
		"the ZM-D-184 rule is exactly one body half-extent (%.5f). Zero is the "
		"substep-burst fall-through that lost Vesper; two is a visible drop the "
		"moment the route loads",
		(double)fAuthoredPlayerClearance, (double)fZM_HUMAN_BODY_HALF_HEIGHT);
}

// ============================================================================
// U6 -- EACH GATE STANDS BEYOND ITS OWN MARKER, WITH ARRIVAL CLEARANCE.
//
// ★ THE DEFECT THIS EXISTS FOR IS AN INFINITE PING-PONG, NOT A MISSING WARP. A
// player warps IN onto a marker; if the arriving body already overlapped the
// sensor beside it, ZM_WarpTrigger fires on the first contact tick and sends them
// straight back, and the gate at the other end sends them straight here again,
// forever, with no input accepted in between. Every other unit in this file --
// every scale, every height, every table lookup -- stays green throughout.
//
// ★ AND THE CLEARANCE IS STATED ON THE NEAR FACE, NOT ON THE CENTRE
// (Q-2026-08-15-001). An arriving player does not stand on the marker: they walk
// off it immediately, and the eight-way MOVE drive makes the natural walk a
// 45-degree diagonal, which spends depth as fast as it spends width. That is the
// defect that shipped in ProfLab with every centre-based arrival clause green.
//
// ★ AND CLAUSE (5) NAMES uZM_ROUTE1_GATE_TARGET_UNRESOLVED. That is a claim about
// the same two sensors -- where each one POINTS, rather than where it stands -- and
// it lives here because these two gates are this unit's subject.
// ============================================================================

ZENITH_TEST(ZM_WorldTraversal, Route1_GatesStandBetweenTheirMarkerAndTheWorldEdgeWithArrivalClearance)
{
	const ZM_Route1Volume xSouthGate = ZM_GetRoute1SouthGate();
	const ZM_Route1Volume xNorthGate = ZM_GetRoute1NorthGate();

	// (1) SIDE. South gate at LOWER Z than the south marker, north gate at HIGHER
	//     Z than the north one -- each sensor is the last thing between the player
	//     and the edge of the world, never a wall in the middle of the route.
	ZENITH_ASSERT_LT(xSouthGate.m_xCenter.z, fZM_ROUTE1_SOUTH_ARRIVAL_Z,
		"the south gate's centre sits at Z %.4f, at or beyond the south arrival "
		"marker at %.4f. A gate on the wrong side of its own marker re-fires the "
		"instant the player arrives",
		(double)xSouthGate.m_xCenter.z, (double)fZM_ROUTE1_SOUTH_ARRIVAL_Z);
	ZENITH_ASSERT_GT(xNorthGate.m_xCenter.z, fZM_ROUTE1_NORTH_ARRIVAL_Z,
		"the north gate's centre sits at Z %.4f, at or before the north arrival "
		"marker at %.4f",
		(double)xNorthGate.m_xCenter.z, (double)fZM_ROUTE1_NORTH_ARRIVAL_Z);

	// (2) CLEARANCE, on the near FACE.
	const float fSouthClearance =
		Route1SouthGateClearanceToMarker(xSouthGate, fZM_ROUTE1_SOUTH_ARRIVAL_Z);
	ZENITH_ASSERT_GE(fSouthClearance, fZM_ROUTE1_GATE_ARRIVAL_CLEARANCE_MIN,
		"the south gate's near face stops only %.4f m short of the south arrival "
		"marker, inside the %.4f m floor. A player warping in from Dawnmere would "
		"be sent straight back, and Dawnmere's gate would send them here again",
		(double)fSouthClearance, (double)fZM_ROUTE1_GATE_ARRIVAL_CLEARANCE_MIN);

	const float fNorthClearance =
		Route1NorthGateClearanceToMarker(xNorthGate, fZM_ROUTE1_NORTH_ARRIVAL_Z);
	ZENITH_ASSERT_GE(fNorthClearance, fZM_ROUTE1_GATE_ARRIVAL_CLEARANCE_MIN,
		"the north gate's near face stops only %.4f m short of the north arrival "
		"marker, inside the %.4f m floor",
		(double)fNorthClearance, (double)fZM_ROUTE1_GATE_ARRIVAL_CLEARANCE_MIN);

	// (3) Both gates stand on ground the recipe actually flattens -- a sensor on
	//     an unlevelled slope sits partly buried and partly in the air.
	struct Route1GatePadClaim
	{
		const char* m_szPad;
		Zenith_Maths::Vector3 m_xCentre;
		const char* m_szWhich;
	};
	const Route1GatePadClaim axPadClaims[] =
	{
		{ szROUTE1_SOUTH_PAD_NAME, xSouthGate.m_xCenter, "south" },
		{ szROUTE1_NORTH_PAD_NAME, xNorthGate.m_xCenter, "north" },
	};

	for (u_int uClaim = 0u; uClaim < Route1CountOf(axPadClaims); ++uClaim)
	{
		const Route1GatePadClaim& xClaim = axPadClaims[uClaim];
		const ZM_TerrainPadSpec* pxPad = Route1FindPad(xClaim.m_szPad);
		ZENITH_ASSERT_NOT_NULL(pxPad,
			"the Route 1 recipe declares no pad '%s' for the %s gate to stand on",
			xClaim.m_szPad, xClaim.m_szWhich);
		if (pxPad == nullptr)
		{
			continue;
		}
		const float fToPadX = xClaim.m_xCentre.x - pxPad->m_xCentre.m_fX;
		const float fToPadZ = xClaim.m_xCentre.z - pxPad->m_xCentre.m_fZ;
		const float fToPad = std::sqrt(fToPadX * fToPadX + fToPadZ * fToPadZ);
		const float fLimit = pxPad->m_fFlattenRadius * fROUTE1_PAD_CONTAINMENT_FRACTION;
		ZENITH_ASSERT_LE(fToPad, fLimit,
			"the %s gate's centre stands %.4f m from pad '%s', outside the %.4f m "
			"of well-flattened ground -- the sensor box would be part-buried in an "
			"unlevelled slope", xClaim.m_szWhich, (double)fToPad, xClaim.m_szPad,
			(double)fLimit);
	}

	// (4) ★ ANTI-VACUITY. A hypothetical gate centred ON its marker -- the exact
	//     mistake clause (2) exists to catch -- must FAIL through the identical
	//     helper. Without this, a clearance predicate that always returned a large
	//     number would look green.
	//
	//     ★ IT IS BUILT FROM THE SOUTH GATE'S OWN CENTRE HEIGHT. There is no longer
	//     a shared gate centre to reach for: each sensor's Y derives from its own
	//     measured column, and this probe is the SOUTH gate slid onto the SOUTH
	//     marker, so it takes the south gate's own constant.
	const ZM_Route1Volume xGateOnTheMarker =
	{
		Zenith_Maths::Vector3(
			fZM_ROUTE1_SOUTH_GATE_X,
			fZM_ROUTE1_SOUTH_GATE_CENTRE_Y,
			fZM_ROUTE1_SOUTH_ARRIVAL_Z),
		xSouthGate.m_xScale
	};
	const float fOnMarkerClearance =
		Route1SouthGateClearanceToMarker(xGateOnTheMarker, fZM_ROUTE1_SOUTH_ARRIVAL_Z);
	ZENITH_ASSERT_LT(fOnMarkerClearance, fZM_ROUTE1_GATE_ARRIVAL_CLEARANCE_MIN,
		"the ANTI-VACUITY arm failed: a gate centred exactly ON the south arrival "
		"marker reports %.4f m of near-face clearance, which CLEARS the %.4f m "
		"floor clause (2) uses. Clause (2) therefore cannot red on the defect it "
		"was written for",
		(double)fOnMarkerClearance, (double)fZM_ROUTE1_GATE_ARRIVAL_CLEARANCE_MIN);

	// (5) ★ NEITHER GATE EVER RESOLVES THE UNRESOLVED SENTINEL, ASSERTED BY NAME.
	//     ZM_Route1Placement.h says of uZM_ROUTE1_GATE_TARGET_UNRESOLVED that "the
	//     boot units assert this value is never actually produced" -- and until this
	//     clause NOTHING IN THE TREE NAMED THE CONSTANT, so the header was
	//     describing a check that did not exist. (Thornacre's twin,
	//     uZM_THORNACRE_RETURN_TARGET_UNRESOLVED, has always been asserted by name;
	//     this is the Route 1 side of the same claim.)
	//
	//     ★ THE MUTATION IT CATCHES: drop or re-point either Route 1 edge in
	//     Source/Data/ZM_WorldSpec.cpp and the resolver hands back 0xFFFFFFFF. R1-2
	//     would then author a ZM_WarpTrigger at a build index no scene can hold --
	//     and that warp is still ACCEPTED by
	//     ZM_GameStateManager::IsWarpDestinationValid, which consults only the
	//     compiled table, so the failure is not a rejection but
	//     ZM_WARP_TRANSITION_WAITING_FOR_SCENE: an opaque fade over a frozen player
	//     until that barrier's frame budget expires (ZM-D-200), then a Zenith_Error
	//     naming the build index it waited on. Not a crash -- and until that budget
	//     runs out, not a visibly broken door either.
	ZENITH_ASSERT_NE(ZM_GetRoute1SouthGateTargetBuildIndex(),
		uZM_ROUTE1_GATE_TARGET_UNRESOLVED,
		"the Route 1 SOUTH gate resolves the unresolved-target sentinel, so the "
		"compiled ZM_SCENE_ROUTE1 row carries no edge back to Dawnmere. The "
		"authored trigger would take a build index no scene can hold -- fix "
		"Source/Data/ZM_WorldSpec.cpp, not this test");
	ZENITH_ASSERT_NE(ZM_GetRoute1NorthGateTargetBuildIndex(),
		uZM_ROUTE1_GATE_TARGET_UNRESOLVED,
		"the Route 1 NORTH gate resolves the unresolved-target sentinel, so the "
		"compiled ZM_SCENE_ROUTE1 row carries no edge on to Thornacre");
}

// ============================================================================
// U7 -- THE GATE VOLUMES ADMIT NO STEP-OVER, NO WALK-AROUND, AND SIT ON THE
// GROUND.
//
// Three separate ways a sensor box ships looking perfectly correct and never
// fires: shorter than a person (stepped over), narrower than the walked corridor
// (walked around), or centred at world Y 0 -- a tunnel 26 m below the player,
// with every scale constant in the header still reading exactly as intended.
//
// Clauses (1) and (2) measure the header against an INDEPENDENT source: the human
// body contract for the height, the recipe's own dirt radius for the width.
//
// ★ CLAUSE (4) IS NOT ONE OF THOSE, AND SAYING SO IS THE POINT. Since the R1-2
// split, each gate's centre is DEFINED as its own measured column plus half the
// box height, so Min().y == that column identically: the clause is arithmetically
// forced and can only red on the volume arithmetic itself (a HalfExtent()/Min()
// that lost its term, a re-defined centre constant), never on a drifted ground.
// It reads THAT GATE'S OWN ROW of axZM_ROUTE1_GROUND_SAMPLES because that is the
// quantity the identity is over -- the two gates are 1336 m apart on two different
// columns, so one shared plane would red the north gate for no defect at all the
// day the table is frozen. Per-gate keeps the clause CORRECT; it does not make it
// stronger.
// ============================================================================

ZENITH_TEST(ZM_WorldTraversal, Route1_GateVolumesAdmitNoStepOverAndSitOnTheGround)
{
	// (1) TALLER THAN A PERSON. Against the shipped body contract, not a literal.
	ZENITH_ASSERT_GT(fZM_ROUTE1_GATE_SCALE_Y, fZM_HUMAN_BODY_HEIGHT,
		"the gate boxes are %.4f m tall against a %.4f m person -- a sensor no "
		"taller than its subject can be stepped or jumped clean over",
		(double)fZM_ROUTE1_GATE_SCALE_Y, (double)fZM_HUMAN_BODY_HEIGHT);

	// (2) WIDER THAN THE VISIBLE PATH. The DirtLane's dirt radius is the recipe's
	//     own statement of how wide the walked path reads, so a gate that does not
	//     span twice it can be walked around without leaving the path at all.
	const ZM_TerrainPathSpec* pxLane = Route1FindPath(szROUTE1_LANE_PATH_NAME);
	ZENITH_ASSERT_NOT_NULL(pxLane,
		"the Route 1 recipe declares no '%s' path, so there is no authored corridor "
		"width for the gates to span", szROUTE1_LANE_PATH_NAME);
	if (pxLane != nullptr)
	{
		const float fVisiblePathWidth = pxLane->m_fDirtRadius * 2.0f;
		ZENITH_ASSERT_GE(fZM_ROUTE1_GATE_SCALE_X, fVisiblePathWidth,
			"the gate boxes span %.4f m against a %.4f m visible dirt path -- a "
			"player can walk around the sensor without ever leaving the lane, and "
			"side blockers are not this slice's job",
			(double)fZM_ROUTE1_GATE_SCALE_X, (double)fVisiblePathWidth);
	}

	// (3) A real depth at all. A zero or negative Z scale inverts the AABB, after
	//     which every Min()/Max() claim in this file evaluates nonsense and still
	//     returns a bool.
	ZENITH_ASSERT_GT(fZM_ROUTE1_GATE_SCALE_Z, 0.0f,
		"the gate boxes have depth %.4f -- a non-positive extent inverts the AABB "
		"and every containment and clearance clause silently changes meaning",
		(double)fZM_ROUTE1_GATE_SCALE_Z);

	// (4) EACH BOX RESTS ON THE MEASURED GROUND OF ITS OWN COLUMN, underside down.
	//
	//     ★ HONEST ABOUT WHAT THIS CAN CATCH: the identity is arithmetically forced
	//     (a gate's centre IS its column plus half the box height), so it guards the
	//     'centre = ground + half height' rule and the Min()/HalfExtent() arithmetic,
	//     not the ground. It is kept because that rule is what stops a sensor being
	//     authored at world Y 0.
	//
	//     ★★ ONE EXPECTATION PER GATE -- NOT A STRENGTHENING, A CORRECTION. The two
	//     sensors stand 1336 m apart and read two different rows of
	//     axZM_ROUTE1_GROUND_SAMPLES. A single shared expectation would compare the
	//     north gate against the SOUTH column and red on a defect that does not
	//     exist the day that table is frozen -- and passes today only because every
	//     row is still seeded from the recipe mirror.
	const float fSouthGateGroundY =
		ZM_Route1GroundFeetY(ZM_ROUTE1_GROUND_SAMPLE_SOUTH_GATE);
	const float fNorthGateGroundY =
		ZM_Route1GroundFeetY(ZM_ROUTE1_GROUND_SAMPLE_NORTH_GATE);

	ZENITH_ASSERT_TRUE(
		Route1VolumeRestsOnGround(ZM_GetRoute1SouthGate(), fSouthGateGroundY),
		"the south gate's underside lands at Y %.4f, not on the %.4f m MEASURED "
		"ground of its own column (ZM_ROUTE1_GROUND_SAMPLE_SOUTH_GATE). A gate "
		"authored at world Y 0 is a tunnel 26 m below the player: a sensor that never "
		"fires, with every compiled scale constant correct",
		(double)ZM_GetRoute1SouthGate().Min().y, (double)fSouthGateGroundY);
	ZENITH_ASSERT_TRUE(
		Route1VolumeRestsOnGround(ZM_GetRoute1NorthGate(), fNorthGateGroundY),
		"the north gate's underside lands at Y %.4f, not on the %.4f m MEASURED "
		"ground of its own column (ZM_ROUTE1_GROUND_SAMPLE_NORTH_GATE, which is NOT "
		"the south gate's). The 'centre = its own ground + half the box height' rule "
		"has been broken, and a sensor resting on its neighbour's height is "
		"part-buried or hovering with every scale constant still correct",
		(double)ZM_GetRoute1NorthGate().Min().y, (double)fNorthGateGroundY);

	// (5) ★ ANTI-VACUITY. The world-Y-0 volume clause (4) exists to reject must
	//     FAIL the identical predicate, against the SAME south-column expectation
	//     clause (4) used.
	const ZM_Route1Volume xSunkenGate =
	{
		Zenith_Maths::Vector3(fZM_ROUTE1_SOUTH_GATE_X, 0.0f, fZM_ROUTE1_SOUTH_GATE_Z),
		ZM_GetRoute1SouthGate().m_xScale
	};
	ZENITH_ASSERT_FALSE(Route1VolumeRestsOnGround(xSunkenGate, fSouthGateGroundY),
		"the ANTI-VACUITY arm failed: a gate centred at world Y 0 is reported as "
		"RESTING ON the south column's %.4f m measured ground, so clause (4) cannot "
		"red on the defect it was written for", (double)fSouthGateGroundY);
}

// ============================================================================
// U8 -- THE TRAINER STATIONS STAND ON FLATTENED, GRASS-FREE GROUND BESIDE THE
// LANE, INSIDE THE CONE'S OWN GEOMETRIC CEILING.
//
// ★★ THE CEILING IS DERIVED FROM THE SHIPPED SIGHT CONSTANTS, NOT WRITTEN DOWN.
// See Route1SightLateralCeiling() above: fZM_SIGHT_MAX_DISTANCE = 8.0 with a
// 30-degree half angle admits a player on the walked line only while the station
// is within 4.0 m of it. That number is the INDEPENDENT ORACLE this unit checks
// the anchors against -- a clause that re-spelled the anchor coordinates would
// catch nothing at all, and a station 6 m off the lane would be a trainer nobody
// can ever meet, with the terrain, the scene bytes and every other placement
// check perfectly green.
//
// The lane's FLATTEN radius does double duty and is stated once: the same
// corridor that levels the ground to the route's target height is run again by
// the GRASS_ERASE phase at density 0, so an anchor inside it is simultaneously
// on measured-height ground AND out of encounter grass. A trainer standing in
// tall grass would roll a wild encounter mid-challenge.
// ============================================================================

ZENITH_TEST(ZM_WorldTraversal, Route1_TrainerStationsStandOnFlattenedGrassFreeGroundBesideTheLane)
{
	// (0) ANTI-VACUITY on the walk. Spelled as a literal deliberately: this is the
	//     tripwire that makes appending a third station a conscious act, since a
	//     new station's anchor gets none of the clauses below for free.
	ZENITH_ASSERT_EQ((u_int)ZM_ROUTE1_TRAINER_COUNT, 2u,
		"Route 1 declares %u trainer stations, not the two this unit's derivation "
		"covers. A new station needs its own lane-offset and sight-window argument "
		"before it can ship",
		(u_int)ZM_ROUTE1_TRAINER_COUNT);

	const float fLaneFlattenRadius = Route1LaneFlattenRadius();
	ZENITH_ASSERT_GT(fLaneFlattenRadius, 0.0f,
		"the Route 1 recipe's '%s' path declares no flatten radius, so the "
		"'his ground is levelled and his grass is erased' argument below has "
		"nothing behind it", szROUTE1_LANE_PATH_NAME);

	// The cone's own geometric limit. Everything below is measured against this.
	const float fSightCeiling = Route1SightLateralCeiling();
	ZENITH_ASSERT_GT(fSightCeiling, 0.0f,
		"the shipped sight cone admits no lateral offset at all (max distance "
		"%.4f, min facing dot %.7f), so no beside-the-lane station could ever work",
		(double)fZM_SIGHT_MAX_DISTANCE, (double)fZM_SIGHT_MIN_FACING_DOT);

	// (1) The authored CEILING is inside the geometric one, with headroom. This is
	//     the clause that stops a later hand widening the band until an unreachable
	//     anchor fits inside it.
	ZENITH_ASSERT_LT(fZM_ROUTE1_TRAINER_MAX_LANE_OFFSET, fSightCeiling,
		"the placement header admits stations up to %.4f m off the lane while the "
		"SHIPPED sight cone can only reach %.4f m. Anything in that gap is a "
		"trainer who can never be triggered -- move the anchor, never the cone",
		(double)fZM_ROUTE1_TRAINER_MAX_LANE_OFFSET, (double)fSightCeiling);

	// (2) THE FLOOR STANDS CLEAR OF THE BODY CONTRACT -- AN INDEPENDENT STATEMENT
	//     ABOUT IT, NOT A RE-EVALUATION OF THE HEADER'S OWN DEFINITION.
	//
	//     ★ WHAT USED TO BE HERE, AND WHY IT IS GONE. DO NOT RESTORE IT. This
	//     clause used to rebuild fZM_ROUTE1_TRAINER_MIN_LANE_OFFSET's defining
	//     expression term for term -- half a footprint, plus a capsule radius, plus
	//     the clear walking gap -- and assert equality against the constant that IS
	//     that expression. Both sides moved together on any edit to any input, so it
	//     could not red on anything at all: a value compared against a
	//     re-computation of ITSELF -- exactly the species this file's own opening
	//     star forbids, and the one that has already cost this repo a debugging
	//     cycle on Vesper's facing (ZM-D-179 / ZM-D-183).
	//
	//     ★ WHAT REPLACES IT, AND THE MUTATION IT CATCHES. Two human capsules
	//     cannot occupy the same metre. A trainer standing off the walked
	//     centreline by less than two capsule radii is in bodily contact with the
	//     passing player HOWEVER the header chooses to derive its floor -- so this
	//     is a claim about the body contract that the floor has to satisfy, not a
	//     copy of the floor. It reds on a re-tune of fZM_HUMAN_BODY_CAPSULE_RADIUS
	//     past half the floor, and on a floor frozen down to a small literal to make
	//     room for an anchor that crowds the lane. Neither was visible to the
	//     equality it replaces.
	ZENITH_ASSERT_GE(fZM_ROUTE1_TRAINER_MIN_LANE_OFFSET,
		fZM_HUMAN_BODY_CAPSULE_RADIUS * 2.0f,
		"the trainer lane-offset floor is %.4f m while two %.4f m capsule radii "
		"need %.4f m. A trainer held no further off the centreline than two bodies "
		"are wide is standing in contact with every player who walks past him, and "
		"the walked lane is where this game spends its whole first hour",
		(double)fZM_ROUTE1_TRAINER_MIN_LANE_OFFSET,
		(double)fZM_HUMAN_BODY_CAPSULE_RADIUS,
		(double)(fZM_HUMAN_BODY_CAPSULE_RADIUS * 2.0f));
	ZENITH_ASSERT_LT(fZM_ROUTE1_TRAINER_MIN_LANE_OFFSET,
		fZM_ROUTE1_TRAINER_MAX_LANE_OFFSET,
		"the trainer lane-offset band is empty or inverted: floor %.4f, ceiling "
		"%.4f", (double)fZM_ROUTE1_TRAINER_MIN_LANE_OFFSET,
		(double)fZM_ROUTE1_TRAINER_MAX_LANE_OFFSET);

	// (3) Each shipped station lands inside that band, inside the geometric
	//     ceiling, and well inside the flatten/grass-erase corridor.
	const float fCorridorLimit =
		fLaneFlattenRadius * fROUTE1_PAD_CONTAINMENT_FRACTION;
	for (u_int uStation = 0u; uStation < (u_int)ZM_ROUTE1_TRAINER_COUNT; ++uStation)
	{
		const ZM_Route1TrainerStation xStation =
			ZM_GetRoute1TrainerStation((ZM_ROUTE1_TRAINER_ID)uStation);
		const float fOffset = Route1DistanceToLane(xStation.m_fX, xStation.m_fZ);

		// ★ THE RAMBLER'S MARGIN IS TIGHT, AND THE NUMBER IS RECORDED HERE SO THE
		//   NEXT BODY RE-TUNE IS NOT A SURPRISE. He measures 2.1288 m off the lane
		//   centreline against today's 2.0 m derived floor -- 0.129 m of room, and
		//   the forager's 2.4551 m is not much better. Grow fZM_HUMAN_BODY_FOOTPRINT
		//   or fZM_HUMAN_BODY_CAPSULE_RADIUS by more than that and this clause reds
		//   on an anchor nobody touched: move the ANCHOR, and re-derive the sight
		//   window in U9b before believing the new one.
		ZENITH_ASSERT_GE(fOffset, fZM_ROUTE1_TRAINER_MIN_LANE_OFFSET,
			"trainer station '%s' stands %.4f m from the lane centreline, inside "
			"the %.4f m floor -- his body crowds the walking line itself",
			xStation.m_szEntityName, (double)fOffset,
			(double)fZM_ROUTE1_TRAINER_MIN_LANE_OFFSET);
		ZENITH_ASSERT_LE(fOffset, fZM_ROUTE1_TRAINER_MAX_LANE_OFFSET,
			"trainer station '%s' stands %.4f m from the lane centreline, past the "
			"%.4f m ceiling", xStation.m_szEntityName, (double)fOffset,
			(double)fZM_ROUTE1_TRAINER_MAX_LANE_OFFSET);
		ZENITH_ASSERT_LT(fOffset, fSightCeiling,
			"trainer station '%s' stands %.4f m from the walked line while the "
			"SHIPPED sight cone reaches only %.4f m laterally. He can NEVER be "
			"triggered, in any playthrough, and nothing else in this game would "
			"notice", xStation.m_szEntityName, (double)fOffset,
			(double)fSightCeiling);

		// The flatten/grass-erase corridor. Strictly weaker than the ceiling above
		// at today's numbers -- it is kept because it is a DIFFERENT claim (levelled
		// ground and erased grass, not sightability) and it would survive a change
		// to either the cone or the path.
		ZENITH_ASSERT_LE(fOffset, fCorridorLimit,
			"trainer station '%s' stands %.4f m from the lane, outside the %.4f m "
			"of the '%s' flatten corridor. Out there his ground row's seed describes "
			"a surface no dab levelled AND the GRASS_ERASE phase never reached, so "
			"the trainer stands in encounter grass",
			xStation.m_szEntityName, (double)fOffset,
			(double)fCorridorLimit, szROUTE1_LANE_PATH_NAME);
	}

	// (4) ★ ANTI-VACUITY. A recipe landmark out in the encounter fields, fed
	//     through the identical helper, must FAIL the corridor clause -- otherwise
	//     a distance helper that always returned zero would look green.
	const ZM_TerrainLandmarkSpec* pxOffLane =
		Route1FindLandmark(szROUTE1_OFF_LANE_LANDMARK);
	ZENITH_ASSERT_NOT_NULL(pxOffLane,
		"the Route 1 recipe no longer declares landmark '%s', so the anti-vacuity "
		"arm below has no known-bad point to feed through the offset helper",
		szROUTE1_OFF_LANE_LANDMARK);
	if (pxOffLane != nullptr)
	{
		const float fOffLaneOffset = Route1DistanceToLane(
			pxOffLane->m_xPosition.m_fX, pxOffLane->m_xPosition.m_fZ);
		ZENITH_ASSERT_GT(fOffLaneOffset, fCorridorLimit,
			"the ANTI-VACUITY arm failed: recipe landmark '%s', out in the wild "
			"encounter fields, measures only %.4f m from the lane and so CLEARS the "
			"%.4f m corridor clause (3) uses. The lane-distance helper cannot "
			"distinguish beside-the-path from out-in-the-grass",
			szROUTE1_OFF_LANE_LANDMARK, (double)fOffLaneOffset,
			(double)fCorridorLimit);
	}
}

// ============================================================================
// U9 -- BOTH TRAINERS ARE TURNED BACK DOWN THE ROUTE, AT THE EXACT HALF TURN.
//
// ★ THIS UNIT MUST NOT RE-SPELL THE QUATERNION THE AUTHORING SPELLS. A guard that
// compared ZM_Route1TrainerFacing() against a locally written (0, 1, 0, 0) could
// not see the authored value move -- both sides are the same constant and they
// would move together. So the property under test is not "these 16 bytes"; it is
// HE IS LOOKING BACK DOWN THE ROUTE, expressed as a dot product against a
// direction derived from the arrival placement.
//
// ★ AND IT RUNS THE SHIPPED FORWARD FUNCTION. ZM_ForwardFromRotation rotates the
// +Z basis and is what the interact picker and the trainer cone use; it must never
// be reimplemented here, and in particular never in terms of glm::eulerAngles,
// which collapses past 90 degrees off +Z and has already cost this repo a full
// debugging cycle.
// ============================================================================

namespace
{
	// The XZ dot between "the way this rotation faces" and "the way from here to
	// there". Both halves go through the shipped helpers -- the facing through
	// ZM_ForwardFromRotation, the bearing through ZM_FlattenXZ -- so a degenerate
	// input yields a zero dot rather than a NaN.
	float Route1FacingDotToward(
		const Zenith_Maths::Quat& xRotation,
		const Zenith_Maths::Vector3& xFrom,
		const Zenith_Maths::Vector3& xTo)
	{
		const Zenith_Maths::Vector3 xForward = ZM_ForwardFromRotation(xRotation);
		const Zenith_Maths::Vector3 xBearing = ZM_FlattenXZ(xTo - xFrom);
		return xForward.x * xBearing.x + xForward.z * xBearing.z;
	}
}

ZENITH_TEST(ZM_WorldTraversal, Route1_TrainerFacingsAreTheExactHalfTurnAndLookBackDownTheRoute)
{
	// The floor. Both stations sit within a couple of metres of the lane and
	// several hundred metres up it from the south arrival, so the true dot is
	// ~0.9998 either way; 0.5 is "turned down the route at all" rather than a
	// tuning value, and it is what a facing typo'd onto the wrong axis fails.
	constexpr float fLOOKS_BACK_MIN_DOT = 0.5f;

	const Zenith_Maths::Vector3 xArrival = ZM_GetRoute1SouthArrivalBodyCentre();

	for (u_int uStation = 0u; uStation < (u_int)ZM_ROUTE1_TRAINER_COUNT; ++uStation)
	{
		const ZM_ROUTE1_TRAINER_ID eStation = (ZM_ROUTE1_TRAINER_ID)uStation;
		const ZM_Route1TrainerStation xStation = ZM_GetRoute1TrainerStation(eStation);
		const Zenith_Maths::Quat xFacing = ZM_Route1TrainerFacing(eStation);
		const Zenith_Maths::Vector3 xCentre = ZM_GetRoute1TrainerCentre(eStation);

		// (1) It is a UNIT quaternion. A non-unit one still rotates, but Jolt
		//     normalises the body's copy and the serialized transform then disagrees
		//     with the constant every other test compares against -- a ZM-D-179
		//     shaped drift with a different cause.
		const float fLengthSquared = xFacing.x * xFacing.x + xFacing.y * xFacing.y
			+ xFacing.z * xFacing.z + xFacing.w * xFacing.w;
		ZENITH_ASSERT_EQ_FLOAT(fLengthSquared, 1.0f, fROUTE1_UNIT_QUAT_EPSILON,
			"trainer '%s' carries a frozen facing of squared length %.7f, not 1. A "
			"non-unit authored quaternion is normalised by the physics body and the "
			"saved bytes then differ from the constant",
			xStation.m_szEntityName, (double)fLengthSquared);

		// (2) It gives a real XZ heading at all. A facing whose XZ projection has
		//     collapsed (a pure pitch, say) makes every dot below zero and would
		//     fail clause (3) for a reason unrelated to where he is looking.
		const Zenith_Maths::Vector3 xForward = ZM_ForwardFromRotation(xFacing);
		const float fForwardLengthSquared =
			xForward.x * xForward.x + xForward.z * xForward.z;
		ZENITH_ASSERT_GT(fForwardLengthSquared, 0.5f,
			"trainer '%s' has no XZ heading (flattened forward (%.5f, %.5f)) -- his "
			"facing is a pitch or a roll, not a turn, and the sight cone rejects "
			"him outright", xStation.m_szEntityName, (double)xForward.x,
			(double)xForward.z);

		// (3) He is turned back DOWN the route, toward the door every player walks
		//     in through, not up it with his back turned.
		const float fDot = Route1FacingDotToward(xFacing, xCentre, xArrival);
		ZENITH_ASSERT_GT(fDot, fLOOKS_BACK_MIN_DOT,
			"trainer '%s' dots %.5f with the direction from his anchor back to the "
			"south arrival, against a floor of %.2f. A NEGATIVE value means his back "
			"is to every player who will ever walk up this route -- a trainer nobody "
			"can trigger, invisible to every geometry check in this file",
			xStation.m_szEntityName, (double)fDot, (double)fLOOKS_BACK_MIN_DOT);
	}

	// (4) ★ TOTALITY. An out-of-range station returns the IDENTITY quaternion, and
	//     that choice is deliberate: identity is forward +Z, i.e. back turned,
	//     which is exactly the value clause (5) proves must FAIL. A degenerate read
	//     therefore cannot silently ship a plausible-looking trainer.
	const Zenith_Maths::Quat xOutOfRange =
		ZM_Route1TrainerFacing(ZM_ROUTE1_TRAINER_COUNT);
	ZENITH_ASSERT_EQ_FLOAT(xOutOfRange.w, 1.0f, fROUTE1_EXACT_EPSILON,
		"ZM_Route1TrainerFacing(ZM_ROUTE1_TRAINER_COUNT) returned w %.7f, not the "
		"documented identity. The degenerate answer must stay the one the "
		"anti-vacuity arm proves is rejected, never a plausible-looking facing",
		(double)xOutOfRange.w);
	ZENITH_ASSERT_EQ_FLOAT(xOutOfRange.x, 0.0f, fROUTE1_EXACT_EPSILON,
		"the out-of-range facing has x %.7f, not 0", (double)xOutOfRange.x);
	ZENITH_ASSERT_EQ_FLOAT(xOutOfRange.y, 0.0f, fROUTE1_EXACT_EPSILON,
		"the out-of-range facing has y %.7f, not 0", (double)xOutOfRange.y);
	ZENITH_ASSERT_EQ_FLOAT(xOutOfRange.z, 0.0f, fROUTE1_EXACT_EPSILON,
		"the out-of-range facing has z %.7f, not 0", (double)xOutOfRange.z);

	// (5) ★ ANTI-VACUITY. That same identity rotation -- back turned, forward +Z --
	//     must FAIL clause (3)'s floor through the identical predicate, at both
	//     anchors, or clause (3) is not known to be able to red at all.
	for (u_int uStation = 0u; uStation < (u_int)ZM_ROUTE1_TRAINER_COUNT; ++uStation)
	{
		const ZM_ROUTE1_TRAINER_ID eStation = (ZM_ROUTE1_TRAINER_ID)uStation;
		const float fIdentityDot = Route1FacingDotToward(
			xOutOfRange, ZM_GetRoute1TrainerCentre(eStation), xArrival);
		ZENITH_ASSERT_LT(fIdentityDot, fLOOKS_BACK_MIN_DOT,
			"the ANTI-VACUITY arm failed at station %u: the identity rotation (back "
			"turned, forward +Z) dots %.5f toward the south arrival, which CLEARS "
			"the %.2f floor clause (3) uses",
			uStation, (double)fIdentityDot, (double)fLOOKS_BACK_MIN_DOT);
	}
}

// ============================================================================
// U9b -- THE STATIONS CAN ACTUALLY SEE A PLAYER WALKING THE LANE.
//
// ★★ THIS IS THE ONLY CHECK IN THE GAME THAT WOULD RED ON A TRAINER PLACED
// OUTSIDE HIS OWN SIGHT GEOMETRY, AND ONE OF THESE TWO ANCHORS WAS ALREADY DEAD
// ONCE. A draft forager at (528, 950) stood EAST of the lane while the foot of the
// perpendicular fell ~0.56 m north of him; facing -Z, his cone window and his
// range window were disjoint and he produced ZERO in-cone samples at every
// sampling density tried. Nothing else in the tree -- not the terrain, not the
// scene bytes, not U8's offset band, not U9's facing dot -- can tell that state
// apart from a working one.
//
// THE METHOD: walk the recipe's OWN DirtLane polyline at a pinned SPACING, put a
// walking player's body centre at each sample, and ask the SHIPPED predicate
// (ZM_IsTargetInTrainerSightFromRotation, default tuning = the shipped cone)
// whether the trainer sees him. Count the samples that pass.
//
// ★ THE THRESHOLD IS ON THE COUNT AT A PINNED SPACING, WHICH IS THE ONLY FORM
// THAT MEANS ANYTHING. An earlier draft asked for "at least 8 samples out of 200
// per segment"; the lane's segments run 238 m to 393 m, so 200 samples is 1.3 m
// spacing and 8 of them is 10.5 m of walked arc -- more approach than the 8 m cone
// can physically contain, i.e. unsatisfiable for ANY station anywhere. At 20
// samples per metre, 8 samples is 0.4 m of approach, which both anchors clear by
// a wide margin (they hold roughly 2.2 m and 4.7 m of in-cone arc respectively).
// ============================================================================

ZENITH_TEST(ZM_WorldTraversal, Route1_TrainerStationsCanActuallySeeAPlayerWalkingTheLane)
{
	// The pinned SPACING, and the floor on the in-cone count it makes meaningful.
	// 20 per metre => one sample every 5 cm, so 8 samples is 0.4 m of approach.
	constexpr u_int uSAMPLES_PER_METRE = 20u;
	constexpr u_int uMIN_IN_CONE_SAMPLES = 8u;

	// ★ NOT A SHIPPED ANCHOR. The lateral offset the anti-vacuity probe is pushed
	// out to -- comfortably past the cone's geometric ceiling, and the exact figure
	// the earlier dead-anchor investigation used.
	constexpr float fPROBE_LATERAL_OFFSET = 6.0f;

	const float fSightCeiling = Route1SightLateralCeiling();
	ZENITH_ASSERT_GT(fPROBE_LATERAL_OFFSET, fSightCeiling,
		"the anti-vacuity probe is displaced only %.4f m laterally while the cone "
		"reaches %.4f m -- the probe is no longer a known-bad placement, so the "
		"zero-samples arm below would be proving nothing",
		(double)fPROBE_LATERAL_OFFSET, (double)fSightCeiling);

	for (u_int uStation = 0u; uStation < (u_int)ZM_ROUTE1_TRAINER_COUNT; ++uStation)
	{
		const ZM_ROUTE1_TRAINER_ID eStation = (ZM_ROUTE1_TRAINER_ID)uStation;
		const ZM_Route1TrainerStation xStation = ZM_GetRoute1TrainerStation(eStation);

		// ★ THE SIGHT ORIGIN IS THE RESTING CENTRE, NOT THE AUTHORED ONE.
		// ZM_GetRoute1TrainerAuthoredCentre carries the ZM-D-184 spawn clearance --
		// a dynamic body authored at exact contact bursts physics substeps and falls
		// through the terrain -- and the body settles out of that within a frame.
		// The cone is a claim about where he STANDS.
		const Zenith_Maths::Vector3 xCentre = ZM_GetRoute1TrainerCentre(eStation);
		const Zenith_Maths::Quat xFacing = ZM_Route1TrainerFacing(eStation);

		const Route1SightSampleCount xCount = Route1CountSightSamplesAlongLane(
			xCentre, xFacing, eStation, uSAMPLES_PER_METRE);

		// (0) ANTI-VACUITY on the walk itself: a helper that sampled nothing would
		//     report zero in-cone samples and look like a placement defect, or -- if
		//     the threshold were ever inverted -- like a pass.
		ZENITH_ASSERT_GT(xCount.m_uTotal, 0u,
			"the lane sampler produced no samples at all for '%s', so the count "
			"below measures nothing. The recipe's '%s' path is missing or has fewer "
			"than two points", xStation.m_szEntityName, szROUTE1_LANE_PATH_NAME);

		// (1) THE CLAIM. He can see a real stretch of the walked lane.
		ZENITH_ASSERT_GE(xCount.m_uInCone, uMIN_IN_CONE_SAMPLES,
			"trainer '%s' sees only %u of %u sampled points on the walked lane (at "
			"%u samples/metre), against a floor of %u. With fZM_SIGHT_MAX_DISTANCE "
			"= %.2f and a 30-degree half angle, a station more than %.4f m from the "
			"walked line is unreachable FOREVER -- and the terrain, the scene bytes "
			"and every other placement check would all stay green. Move the ANCHOR; "
			"the sight constants are out of scope",
			xStation.m_szEntityName, xCount.m_uInCone, xCount.m_uTotal,
			uSAMPLES_PER_METRE, uMIN_IN_CONE_SAMPLES,
			(double)fZM_SIGHT_MAX_DISTANCE, (double)fSightCeiling);

		// (2) ★ ANTI-VACUITY. The same station pushed straight out along its own
		//     perpendicular to past the geometric ceiling, with the SAME facing,
		//     through the IDENTICAL sampler, must see NOTHING. Without this arm a
		//     predicate that always answered "seen" would satisfy clause (1).
		const Zenith_Maths::Vector3 xProbe =
			Route1LateralProbeCentre(xCentre, fPROBE_LATERAL_OFFSET);
		const float fProbeOffset = Route1DistanceToLane(xProbe.x, xProbe.z);
		ZENITH_ASSERT_EQ_FLOAT(fProbeOffset, fPROBE_LATERAL_OFFSET,
			fROUTE1_ANCHOR_EPSILON,
			"the anti-vacuity probe built from '%s' landed %.4f m from the lane "
			"rather than the intended %.4f m, so the zero-samples arm below is not "
			"testing the placement it claims to", xStation.m_szEntityName,
			(double)fProbeOffset, (double)fPROBE_LATERAL_OFFSET);

		// ★ THE PROBE KEEPS THE STATION'S OWN MEASURED GROUND: it is the SAME
		//   observer displaced laterally, so the walked line it is measured against
		//   must not silently move to a different height as well.
		const Route1SightSampleCount xProbeCount = Route1CountSightSamplesAlongLane(
			xProbe, xFacing, eStation, uSAMPLES_PER_METRE);
		ZENITH_ASSERT_EQ(xProbeCount.m_uInCone, 0u,
			"the ANTI-VACUITY arm failed: a probe station displaced to %.4f m off "
			"the lane -- past the cone's %.4f m geometric ceiling -- still reports "
			"%u in-cone samples out of %u. Clause (1) therefore cannot red on the "
			"dead-anchor defect it was written for",
			(double)fPROBE_LATERAL_OFFSET, (double)fSightCeiling,
			xProbeCount.m_uInCone, xProbeCount.m_uTotal);
	}
}

// ============================================================================
// U10 -- THE ENTITY NAMES ARE UNIQUE, PRINTABLE LOOKUP KEYS AND NO FUTURE NEEDLE
// IS SWALLOWED.
//
// The committed-bytes needles (Tests/ZM_Tests_CommittedSceneBytes.cpp) search
// BARE NAME BYTES with no NUL over the whole .zscen blob. A needle N is SWALLOWED
// exactly when N is a substring of some OTHER string that also lands in those
// bytes -- and the strings that land there are entity names, serialized component
// TYPE names, and spawn tags. This unit mechanises that whole argument.
//
// ★★ TRAP 1 -- THE SCENE NAME IS NOT AN ENTITY NAME AND IS NOT WALKED HERE.
// "Route1" is a strict substring of every other Route 1 name, so adding
// szZM_ROUTE1_SCENE_NAME to the table would red the pairwise clause on the first
// pair it reached. See the table's own comment above.
//
// ★★ TRAP 2 -- THE TYPE-NAME CLAUSE IS ONE-DIRECTIONAL, AND MUST STAY THAT WAY.
// It asserts strstr(typeName, entityName) == nullptr and NEVER the converse.
// "Camera" IS a substring of "Route1Camera", "Terrain" of "Route1Terrain",
// "Player" of "ZM_PlayerController" -- symmetrising this clause reds it
// immediately and would say nothing about needles. DO NOT "fix" it into a
// symmetric form.
//
// ★★ AND THE PLAYER NAME IS EXCLUDED FROM THE TYPE-NAME CLAUSE ENTIRELY, WHICH IS
// A COST THIS SLICE DELIBERATELY BOOKED. The player entity MUST be named "Player"
// (clause 1 below), and "Player" is a strict substring of the serialized type name
// "ZM_PlayerController". A later committed-bytes needle on the player must
// therefore use the STRICTLY-MORE clause the shipped tests already carry, never a
// `== 1` equality.
// ============================================================================

ZENITH_TEST(ZM_WorldTraversal, Route1_EntityNamesAreUniquePrintableLookupKeysNoNeedleIsSwallowed)
{
	const u_int uNameCount = Route1CountOf(aszROUTE1_ENTITY_NAMES);

	// (0) ANTI-VACUITY on every walk below.
	ZENITH_ASSERT_GT(uNameCount, 0u,
		"Route 1 declares no entity names, so every clause in this unit passes by "
		"walking nothing");

	// (1) ★ THE PLAYER IS SPELLED "Player" IN EVERY SCENE, AND THIS IS THE ONE
	//     CLAUSE IN THIS FILE THAT SPELLS A NAME AS A LITERAL. It is a CONVENTION
	//     clause since Q-2026-08-15-002 -- ZM_FollowCamera used to acquire by this
	//     exact name, which made a mismatch a permanent black screen; it now
	//     acquires the unique ZM_PlayerController in its own scene and never reads
	//     the name. What survives is the byte-needle consequence: the shared
	//     spelling is what makes "Player" a strict substring of the type name
	//     "ZM_PlayerController", and every committed-bytes needle on it depends on
	//     that being true for EVERY scene, not most of them.
	ZENITH_ASSERT_STREQ(szZM_ROUTE1_PLAYER_ENTITY_NAME,
		szROUTE1_PLAYER_NAME_CONVENTION,
		"Route 1's player entity is named '%s' while every other shipped scene "
		"spells it '%s'. The camera no longer cares (it acquires by "
		"ZM_PlayerController), so this is not a black screen -- but the "
		"committed-bytes needles rely on the player name being a strict substring "
		"of \"ZM_PlayerController\" in EVERY scene, and a one-off spelling makes "
		"that reasoning silently wrong for this one",
		szZM_ROUTE1_PLAYER_ENTITY_NAME, szROUTE1_PLAYER_NAME_CONVENTION);

	// (2) Every name is a usable lookup key.
	for (u_int uIndex = 0u; uIndex < uNameCount; ++uIndex)
	{
		ZENITH_ASSERT_TRUE(Route1NameIsALookupKey(aszROUTE1_ENTITY_NAMES[uIndex]),
			"Route 1 entity name %u is null, empty, or carries a space or control "
			"byte -- FindEntityByName would silently miss it rather than fail "
			"diagnosably", uIndex);
	}

	// (3) Pairwise DISTINCT and pairwise NON-SUBSTRING. The second half is the
	//     FromLab / FromLabSpawn trap: a needle on the shorter name silently counts
	//     the longer one too, and the shipped needles have exactly one
	//     strictly-more clause to work around it with.
	for (u_int uA = 0u; uA < uNameCount; ++uA)
	{
		for (u_int uB = uA + 1u; uB < uNameCount; ++uB)
		{
			const char* szA = aszROUTE1_ENTITY_NAMES[uA];
			const char* szB = aszROUTE1_ENTITY_NAMES[uB];
			ZENITH_ASSERT_NE(std::strcmp(szA, szB), 0,
				"Route 1 entity names %u and %u are both '%s' -- two authored "
				"entities sharing a name make every FindEntityByName on it "
				"order-dependent", uA, uB, szA);
			ZENITH_ASSERT_FALSE(Route1NeedleIsSwallowedBy(szA, szB),
				"Route 1 entity name '%s' is a substring of '%s', so a "
				"committed-bytes needle on the first silently counts the second "
				"too -- the FromLab/FromLabSpawn trap", szA, szB);
			ZENITH_ASSERT_FALSE(Route1NeedleIsSwallowedBy(szB, szA),
				"Route 1 entity name '%s' is a substring of '%s'", szB, szA);
		}
	}

	// (4) No name is swallowed by a serialized component TYPE name. ONE DIRECTION
	//     ONLY -- see trap 2 in the block above -- and with the player name
	//     excluded, for the reason stated there and re-proved in clause (6).
	const u_int uEngineTypeCount = Route1CountOf(aszROUTE1_ENGINE_TYPE_NAMES);
	const u_int uGameTypeCount = Route1CountOf(aszROUTE1_GAME_TYPE_NAMES);
	ZENITH_ASSERT_GT(uEngineTypeCount, 0u,
		"the transcribed engine component type table is empty");
	ZENITH_ASSERT_GT(uGameTypeCount, 0u,
		"the transcribed game component type table is empty");

	// ★ THE TABLE ABOVE IS SHARED NOW (ZM-57): this file and
	// Tests/ZM_Tests_ThornacrePlacement.cpp both include
	// Tests/ZM_Tests_ComponentTypeNames.h and alias its two arrays under their
	// own local names, so there is exactly one aszROUTE1_ENGINE_TYPE_NAMES /
	// aszROUTE1_GAME_TYPE_NAMES table in the tree, not two copies that could
	// drift from EACH OTHER. What the literal counts below still catch is
	// drift from the REAL registries: an edit to the shared header that is not
	// reconciled against Zenith_ComponentMeta_Registration.cpp and
	// Zenithmon.cpp's ZENITH_REGISTER_COMPONENT block reds BOTH suites (this
	// file's and Thornacre's) rather than neither, because both pin the same
	// counts against the same shared table.
	//
	// EDIT THE SHARED HEADER AND BOTH FILES' LITERALS IN ONE CHANGE.
	ZENITH_ASSERT_EQ(uGameTypeCount, 16u,
		"the shared GAME component type table (Tests/ZM_Tests_ComponentTypeNames.h) "
		"holds %u rows, not the 16 that Games/Zenithmon/Zenithmon.cpp's "
		"ZENITH_REGISTER_COMPONENT block registers. "
		"Tests/ZM_Tests_ThornacrePlacement.cpp pins the SAME shared table at the "
		"same literal, so this reds if the header is grown and that file's literal "
		"is not -- update the header and BOTH files' literals", uGameTypeCount);
	ZENITH_ASSERT_EQ(uEngineTypeCount, 17u,
		"the shared ENGINE component type table (Tests/ZM_Tests_ComponentTypeNames.h) "
		"holds %u rows, not the 17 the engine serializes (the 16 in "
		"Zenith_ComponentMeta_Registration.cpp plus \"AIAgent\", registered at "
		"order 90 through the AI forwarder). "
		"Tests/ZM_Tests_ThornacrePlacement.cpp pins the SAME shared table at the "
		"same literal", uEngineTypeCount);

	for (u_int uIndex = 0u; uIndex < uNameCount; ++uIndex)
	{
		const char* szName = aszROUTE1_ENTITY_NAMES[uIndex];
		if (std::strcmp(szName, szZM_ROUTE1_PLAYER_ENTITY_NAME) == 0)
		{
			continue;   // see clause (6) -- the booked exclusion
		}
		for (u_int uType = 0u; uType < uEngineTypeCount; ++uType)
		{
			ZENITH_ASSERT_FALSE(
				Route1NeedleIsSwallowedBy(szName, aszROUTE1_ENGINE_TYPE_NAMES[uType]),
				"Route 1 entity name '%s' is a substring of the serialized engine "
				"component type name '%s', so a byte needle on it counts every "
				"component of that type as well", szName,
				aszROUTE1_ENGINE_TYPE_NAMES[uType]);
		}
		for (u_int uType = 0u; uType < uGameTypeCount; ++uType)
		{
			ZENITH_ASSERT_FALSE(
				Route1NeedleIsSwallowedBy(szName, aszROUTE1_GAME_TYPE_NAMES[uType]),
				"Route 1 entity name '%s' is a substring of the serialized game "
				"component type name '%s'", szName, aszROUTE1_GAME_TYPE_NAMES[uType]);
		}
	}

	// (5) No name CONTAINS a spawn tag, so no future TAG needle is inflated by an
	//     entity name. This is why the markers are called ...Arrival rather than
	//     Route1FromDawnmereSpawn, and it is what lets a later slice assert
	//     tagHits == 1 as a plain equality.
	//
	//     The tags are RESOLVED, never spelled: the two Route 1 OFFERS come off the
	//     compiled world row, and the two its gates ASK FOR come off the header's
	//     resolvers, which walk that row's connection list.
	const ZM_WorldSpec& xRoute1Row = ZM_GetWorldSpec(ZM_SCENE_ROUTE1);
	ZENITH_ASSERT_GT(xRoute1Row.m_uSpawnTagCount, 0u,
		"the Route 1 world row offers no spawn tags, so the tag clause below walks "
		"nothing");

	const char* aszTags[] =
	{
		ZM_GetRoute1SouthGateSpawnTag(),
		ZM_GetRoute1NorthGateSpawnTag(),
	};
	for (u_int uIndex = 0u; uIndex < uNameCount; ++uIndex)
	{
		const char* szName = aszROUTE1_ENTITY_NAMES[uIndex];
		for (u_int uTag = 0u; uTag < xRoute1Row.m_uSpawnTagCount; ++uTag)
		{
			const char* szTag = xRoute1Row.m_pszSpawnTags != nullptr
				? xRoute1Row.m_pszSpawnTags[uTag] : nullptr;
			if (szTag == nullptr || szTag[0] == '\0')
			{
				continue;
			}
			ZENITH_ASSERT_FALSE(Route1NeedleIsSwallowedBy(szTag, szName),
				"Route 1 entity name '%s' CONTAINS the spawn tag '%s' that its own "
				"world row offers, so every future needle on that tag counts this "
				"entity too and the claim stops being a plain equality",
				szName, szTag);
		}
		for (u_int uTag = 0u; uTag < Route1CountOf(aszTags); ++uTag)
		{
			if (aszTags[uTag] == nullptr || aszTags[uTag][0] == '\0')
			{
				continue;   // an unresolved gate is U4's failure to report, not this one's
			}
			ZENITH_ASSERT_FALSE(Route1NeedleIsSwallowedBy(aszTags[uTag], szName),
				"Route 1 entity name '%s' CONTAINS the spawn tag '%s' its own gate "
				"asks its destination for", szName, aszTags[uTag]);
		}
	}

	// (6) ★ ANTI-VACUITY, and the standing proof that clause (4)'s exclusion is
	//     still NECESSARY. The one pair everybody knows collides must be reported as
	//     colliding by the identical predicate; if it ever stops, either the
	//     predicate is broken or the player name moved -- and the player name moving
	//     is the black-screen defect clause (1) polices.
	ZENITH_ASSERT_TRUE(
		Route1NeedleIsSwallowedBy(szZM_ROUTE1_PLAYER_ENTITY_NAME,
			szROUTE1_TYPE_CONTAINING_THE_PLAYER_NAME),
		"the ANTI-VACUITY arm failed: '%s' is not reported as a substring of the "
		"serialized type name '%s'. Either the substring predicate is broken -- in "
		"which case clauses (3) to (5) are all vacuous -- or the player entity has "
		"been renamed, which breaks ZM_FollowCamera's lookup",
		szZM_ROUTE1_PLAYER_ENTITY_NAME, szROUTE1_TYPE_CONTAINING_THE_PLAYER_NAME);

	// ...and the same for the pairwise clause, using a name pair that is NOT
	// shipped: a marker named after the gate beside it would be swallowed exactly
	// this way.
	ZENITH_ASSERT_TRUE(
		Route1NeedleIsSwallowedBy(szROUTE1_SWALLOWED_NEEDLE, szROUTE1_SWALLOWING_NAME),
		"the ANTI-VACUITY arm failed: the known-bad pair ('%s' inside '%s') is not "
		"reported as a collision, so clause (3) cannot red on the name trap it was "
		"written for", szROUTE1_SWALLOWED_NEEDLE, szROUTE1_SWALLOWING_NAME);

	// (7) THE STATION TABLE HANDS BACK THE DECLARED CONSTANTS, and its degenerate
	//     answer is the documented sentinel.
	//
	//     ★ WITHOUT THIS CLAUSE A SWITCH THAT RETURNED ONE STATION'S NAME TWICE
	//     SATISFIES EVERYTHING ABOVE: the names walked above are the CONSTANTS, and
	//     nothing else ties the accessor's answers to them.
	const char* const aszDeclaredStationNames[] =
	{
		szZM_ROUTE1_TRAINER_RAMBLER_ENTITY_NAME,
		szZM_ROUTE1_TRAINER_FORAGER_ENTITY_NAME,
	};
	ZENITH_ASSERT_EQ(Route1CountOf(aszDeclaredStationNames),
		(u_int)ZM_ROUTE1_TRAINER_COUNT,
		"this unit knows %u declared station names but the enum declares %u -- a "
		"station was added without a name constant, or the reverse",
		Route1CountOf(aszDeclaredStationNames), (u_int)ZM_ROUTE1_TRAINER_COUNT);

	for (u_int uStation = 0u;
		uStation < (u_int)ZM_ROUTE1_TRAINER_COUNT
			&& uStation < Route1CountOf(aszDeclaredStationNames);
		++uStation)
	{
		ZENITH_ASSERT_STREQ(
			ZM_GetRoute1TrainerStation((ZM_ROUTE1_TRAINER_ID)uStation).m_szEntityName,
			aszDeclaredStationNames[uStation],
			"ZM_GetRoute1TrainerStation(%u) hands back a name that is not the %uth "
			"declared station constant -- the switch and the constants have parted "
			"company, and the authoring would emit one trainer's name twice",
			uStation, uStation);
	}

	// TOTALITY: an out-of-range station yields the documented degenerate row, whose
	// name is deliberately one no authored entity carries. A caller that mistakenly
	// indexes past the end must fail loudly at its own lookup instead of silently
	// matching a real station.
	const ZM_Route1TrainerStation xOutOfRange =
		ZM_GetRoute1TrainerStation(ZM_ROUTE1_TRAINER_COUNT);
	ZENITH_ASSERT_TRUE(Route1NameIsALookupKey(xOutOfRange.m_szEntityName),
		"the out-of-range trainer station's name is null or empty rather than the "
		"documented sentinel -- a caller that indexed past the end would crash "
		"rather than miss");
	for (u_int uStation = 0u;
		uStation < Route1CountOf(aszDeclaredStationNames); ++uStation)
	{
		ZENITH_ASSERT_NE(
			std::strcmp(xOutOfRange.m_szEntityName,
				aszDeclaredStationNames[uStation]), 0,
			"the out-of-range trainer station hands back the REAL name '%s'. A "
			"degenerate read would then author a plausible-looking duplicate trainer "
			"instead of failing", aszDeclaredStationNames[uStation]);
	}
}

// ============================================================================
// U11 -- THE SETTLED CAMERA STANDS ABOVE THE GROUND, BEHIND THE ARRIVAL, AND
// SEES THE ROUTE.
//
// Two silent shipping defects live here. A SIGN ERROR ON THE PITCH swings the eye
// BELOW the pivot instead of above it and buries the camera in the terrain -- and
// every constant in the header still reads exactly as intended. A DEFAULT FAR
// PLANE (the 100 m the two interiors ship with) clips a 1536 m corridor a few
// strides ahead of the player.
//
// ★ THE FAR-PLANE CLAUSE IS MEASURED AGAINST THE RECIPE'S OWN Z EXTENT, not
// against a number in the header, so re-cutting the route's bounds moves the
// requirement with them.
// ============================================================================

ZENITH_TEST(ZM_WorldTraversal, Route1_SettledCameraStandsAboveGroundBehindTheArrival)
{
	// (1) The projection parameters are a usable frustum at all. A far plane at or
	//     behind the near one, a non-positive aspect or an out-of-domain FOV all
	//     produce a projection matrix that renders nothing, which reads as a black
	//     screen rather than as a camera bug.
	ZENITH_ASSERT_LT(fZM_ROUTE1_CAMERA_NEAR, fZM_ROUTE1_CAMERA_FAR,
		"the Route 1 camera's near plane %.4f is at or beyond its far plane %.4f",
		(double)fZM_ROUTE1_CAMERA_NEAR, (double)fZM_ROUTE1_CAMERA_FAR);
	ZENITH_ASSERT_GT(fZM_ROUTE1_CAMERA_NEAR, 0.0f,
		"the Route 1 camera's near plane is %.4f; a non-positive near plane is not "
		"a perspective projection", (double)fZM_ROUTE1_CAMERA_NEAR);
	ZENITH_ASSERT_GT(fZM_ROUTE1_CAMERA_FOV_DEGREES, 0.0f,
		"the Route 1 camera's field of view is %.4f degrees",
		(double)fZM_ROUTE1_CAMERA_FOV_DEGREES);
	ZENITH_ASSERT_LT(fZM_ROUTE1_CAMERA_FOV_DEGREES, 180.0f,
		"the Route 1 camera's field of view is %.4f degrees, at or past the "
		"degenerate half turn", (double)fZM_ROUTE1_CAMERA_FOV_DEGREES);
	ZENITH_ASSERT_GT(fZM_ROUTE1_CAMERA_ASPECT, 0.0f,
		"the Route 1 camera's aspect ratio is %.4f",
		(double)fZM_ROUTE1_CAMERA_ASPECT);
	ZENITH_ASSERT_GT(fZM_ROUTE1_CAMERA_ARM, 0.0f,
		"the Route 1 camera's arm is %.4f -- a non-positive arm puts the eye at or "
		"in front of its own pivot", (double)fZM_ROUTE1_CAMERA_ARM);

	// (2) The far plane covers the route the camera is looking down. Measured
	//     against the RECIPE's authored Z bounds, not against a comment.
	const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetRoute1TerrainRecipe();
	const float fRouteDepth = xRecipe.WorldMaxZ() - xRecipe.WorldMinZ();
	ZENITH_ASSERT_GT(fRouteDepth, 0.0f,
		"the Route 1 recipe declares a non-positive Z extent (%.4f to %.4f), so the "
		"far-plane clause below has nothing to measure against",
		(double)xRecipe.WorldMinZ(), (double)xRecipe.WorldMaxZ());
	ZENITH_ASSERT_GE(fZM_ROUTE1_CAMERA_FAR, fRouteDepth,
		"the Route 1 camera's far plane is %.4f against a %.4f m route. The default "
		"100 m plane the two interiors ship with would clip the world away a few "
		"strides ahead of the player",
		(double)fZM_ROUTE1_CAMERA_FAR, (double)fRouteDepth);

	// (3) The settled eye stands clear of the ground.
	//
	//     ★ THE GROUND HERE IS THE SOUTH ARRIVAL'S MEASURED COLUMN, because the
	//     settled pose is DERIVED from the south arrival: the pivot is
	//     ZM_GetRoute1SouthArrivalBodyCentre lifted by the follow camera's pivot
	//     height, so the surface the eye must clear is the one that body stands on.
	//     The recipe mirror would be a different quantity wearing the same number
	//     until axZM_ROUTE1_GROUND_SAMPLES is frozen.
	const Zenith_Maths::Vector3 xCamera = ZM_GetRoute1SettledCameraPosition();
	const float fArrivalGroundY =
		ZM_Route1GroundFeetY(ZM_ROUTE1_GROUND_SAMPLE_SOUTH_ARRIVAL);
	const float fClearanceFloor =
		fArrivalGroundY + fZM_ROUTE1_CAMERA_MIN_GROUND_CLEARANCE;
	ZENITH_ASSERT_GE(xCamera.y, fClearanceFloor,
		"the settled Route 1 camera sits at Y %.4f, below the %.4f m floor (the south "
		"arrival's measured ground %.4f + %.4f clearance). A pitch sign error swings "
		"the eye BELOW its pivot and buries it in the terrain, with every camera "
		"constant still reading exactly as intended", (double)xCamera.y,
		(double)fClearanceFloor, (double)fArrivalGroundY,
		(double)fZM_ROUTE1_CAMERA_MIN_GROUND_CLEARANCE);

	// (4) It stands BEHIND the arriving body -- far enough back not to be inside
	//     the capsule, never further than its own arm.
	const Zenith_Maths::Vector3 xArrival = ZM_GetRoute1SouthArrivalBodyCentre();
	const float fToArrivalX = xCamera.x - xArrival.x;
	const float fToArrivalZ = xCamera.z - xArrival.z;
	const float fPlanarDistance =
		std::sqrt(fToArrivalX * fToArrivalX + fToArrivalZ * fToArrivalZ);
	ZENITH_ASSERT_GE(fPlanarDistance, fZM_HUMAN_BODY_CAPSULE_RADIUS,
		"the settled Route 1 camera stands only %.4f m from the arriving body in "
		"plan, inside its own %.4f m capsule radius -- the eye is inside the player",
		(double)fPlanarDistance, (double)fZM_HUMAN_BODY_CAPSULE_RADIUS);
	ZENITH_ASSERT_LE(fPlanarDistance, fZM_ROUTE1_CAMERA_ARM,
		"the settled Route 1 camera stands %.4f m from the arriving body in plan, "
		"further than its own %.4f m arm -- the pose accessor is no longer swinging "
		"the arm back from the pivot", (double)fPlanarDistance,
		(double)fZM_ROUTE1_CAMERA_ARM);

	// (5) ★ ANTI-VACUITY, in two steps because the arm needs a second
	//     implementation and a second implementation checking only itself would
	//     prove nothing.
	//     (5a) the local pose arithmetic AGREES with the shipped accessor at the
	//          shipped pitch -- which is what makes it a stand-in for it;
	//     (5b) the SIGN-FLIPPED pitch, through that same arithmetic, must FAIL
	//          clause (3)'s floor.
	const Zenith_Maths::Vector3 xLocalAtShippedPitch =
		Route1CameraPositionForPitch(fZM_ROUTE1_CAMERA_PITCH);
	ZENITH_ASSERT_NEAR_VEC3(xLocalAtShippedPitch, xCamera, fROUTE1_EXACT_EPSILON,
		"this unit's pose arithmetic disagrees with ZM_GetRoute1SettledCameraPosition "
		"at the shipped pitch, so the sign-flipped probe below is not exercising the "
		"accessor's geometry");

	const Zenith_Maths::Vector3 xFlippedPitchCamera =
		Route1CameraPositionForPitch(-fZM_ROUTE1_CAMERA_PITCH);
	ZENITH_ASSERT_LT(xFlippedPitchCamera.y, fClearanceFloor,
		"the ANTI-VACUITY arm failed: a sign-flipped pitch puts the settled eye at "
		"Y %.4f, which still CLEARS the %.4f m floor clause (3) uses. Clause (3) "
		"therefore cannot red on the pitch sign error it was written for",
		(double)xFlippedPitchCamera.y, (double)fClearanceFloor);
}

// ============================================================================
// ---- ZM-27: THE THREE GROUND-ITEM PROPS ------------------------------------
//
// ★★ THE CLAIM THESE EXIST FOR IS **REACHABILITY**, and nothing else in this game
// can make it. A prop authored 3 m off the walked line is a perfectly valid entity
// standing on perfectly measured ground carrying a perfectly configured component,
// and it can never be picked up in any playthrough, forever -- because
// ZM_PickInteractTarget refuses a candidate beyond fZM_INTERACT_MAX_DISTANCE and
// these props are authored at ZM_GroundItemProp::fDEFAULT_RADIUS (zero bonus).
// Every other check in the suite would stay green: the ground oracle would resolve
// its column, a scene-byte needle would find the entity, and the component's own
// units would pass on compiled constants. This is the same failure shape as the
// dead forager anchor the sight units above exist for, one layer up.
// ============================================================================

namespace
{
	// The reach a prop authored at the DEFAULT radius actually gets. Read from the
	// two shipped constants rather than spelled -- widening either must move this
	// oracle with it, or the unit stops testing the shipped rule.
	constexpr float Route1PropInteractReach()
	{
		return fZM_INTERACT_MAX_DISTANCE + ZM_GroundItemProp::fDEFAULT_RADIUS;
	}

	// Every prop anchor, paired with the ground row it stands on, IN AUTHORING
	// ORDER. A table rather than three copies of the same clause, so appending a
	// fourth prop is one row here and the units below cover it automatically.
	struct Route1PropAnchor
	{
		const char*             m_szEntityName;
		float                   m_fX;
		float                   m_fZ;
		ZM_ROUTE1_GROUND_SAMPLE m_eGroundSample;
		ZM_GROUND_ITEM_ID       m_eGroundItemId;
	};

	constexpr Route1PropAnchor axROUTE1_PROP_ANCHORS[] =
	{
		{ szZM_ROUTE1_PROP_SOUTH_SALVE_ENTITY_NAME,
		  fZM_ROUTE1_PROP_SOUTH_SALVE_X, fZM_ROUTE1_PROP_SOUTH_SALVE_Z,
		  ZM_ROUTE1_GROUND_SAMPLE_PROP_SOUTH_SALVE,
		  ZM_GROUND_ITEM_ROUTE1_SOUTH_SALVE },
		{ szZM_ROUTE1_PROP_LANE_CATCHORB_ENTITY_NAME,
		  fZM_ROUTE1_PROP_LANE_CATCHORB_X, fZM_ROUTE1_PROP_LANE_CATCHORB_Z,
		  ZM_ROUTE1_GROUND_SAMPLE_PROP_LANE_CATCHORB,
		  ZM_GROUND_ITEM_ROUTE1_LANE_CATCHORB },
		{ szZM_ROUTE1_PROP_NORTH_SALVE_ENTITY_NAME,
		  fZM_ROUTE1_PROP_NORTH_SALVE_X, fZM_ROUTE1_PROP_NORTH_SALVE_Z,
		  ZM_ROUTE1_GROUND_SAMPLE_PROP_NORTH_SALVE,
		  ZM_GROUND_ITEM_ROUTE1_NORTH_SALVE },
	};

	// DEDUCED, never spelled -- the ZM_ROUTE1_GROUND_SAMPLE_COUNT idiom. An
	// explicit bound would make the count clause below a tautology.
	constexpr u_int uROUTE1_PROP_ANCHOR_COUNT =
		sizeof(axROUTE1_PROP_ANCHORS) / sizeof(axROUTE1_PROP_ANCHORS[0]);
}

// EVERY registered prop is authored somewhere, and every authored prop is
// registered. Without this the two tables drift silently: a fourth registry row
// with no anchor is an item no scene yields, and an anchor naming an id the
// registry lost is a prop that fails closed and is never takeable.
ZENITH_TEST(ZM_WorldTraversal, Route1_GroundItemPropAnchorsCoverTheWholeRegistry)
{
	ZENITH_ASSERT_EQ(uROUTE1_PROP_ANCHOR_COUNT, (u_int)ZM_GROUND_ITEM_COUNT,
		"the Route 1 prop anchor table holds %u rows while the ground-item registry "
		"holds %u. Every registered prop must be authored somewhere and every "
		"authored prop must be registered -- a registry row with no anchor is an "
		"item no scene ever yields",
		uROUTE1_PROP_ANCHOR_COUNT, (u_int)ZM_GROUND_ITEM_COUNT);

	// And the ids are DISTINCT. Two anchors sharing an id would author two props
	// that collapse into one collected bit -- taking either would silently retire
	// the other, with every count clause above still green.
	for (u_int uA = 0u; uA < uROUTE1_PROP_ANCHOR_COUNT; ++uA)
	{
		ZENITH_ASSERT_LT((u_int)axROUTE1_PROP_ANCHORS[uA].m_eGroundItemId,
			(u_int)ZM_GROUND_ITEM_COUNT,
			"prop anchor '%s' names ground-item id %u, which is outside the registry",
			axROUTE1_PROP_ANCHORS[uA].m_szEntityName,
			(u_int)axROUTE1_PROP_ANCHORS[uA].m_eGroundItemId);

		for (u_int uB = uA + 1u; uB < uROUTE1_PROP_ANCHOR_COUNT; ++uB)
		{
			ZENITH_ASSERT_NE(
				(u_int)axROUTE1_PROP_ANCHORS[uA].m_eGroundItemId,
				(u_int)axROUTE1_PROP_ANCHORS[uB].m_eGroundItemId,
				"prop anchors '%s' and '%s' both carry ground-item id %u -- taking "
				"either would mark the other collected too",
				axROUTE1_PROP_ANCHORS[uA].m_szEntityName,
				axROUTE1_PROP_ANCHORS[uB].m_szEntityName,
				(u_int)axROUTE1_PROP_ANCHORS[uA].m_eGroundItemId);
		}
	}
}

// ★★ THE ONE THAT CAN SEE AN UNPICKABLE PROP. Measured against the recipe's OWN
// DirtLane polyline through Route1DistanceToLane -- the same independent oracle the
// trainer units use -- so it re-derives the perpendicular offsets the placement
// header states rather than trusting them.
ZENITH_TEST(ZM_WorldTraversal, Route1_GroundItemPropsAreReachableFromTheWalkedLane)
{
	const float fReach = Route1PropInteractReach();
	ZENITH_ASSERT_GT(fReach, 0.0f,
		"the shipped interact reach resolved to %.4f m, so every clause below would "
		"be vacuous", (double)fReach);

	const float fFlattenRadius = Route1LaneFlattenRadius();

	for (u_int uProp = 0u; uProp < uROUTE1_PROP_ANCHOR_COUNT; ++uProp)
	{
		const Route1PropAnchor& xAnchor = axROUTE1_PROP_ANCHORS[uProp];
		const float fToLane = Route1DistanceToLane(xAnchor.m_fX, xAnchor.m_fZ);

		ZENITH_ASSERT_LT(fToLane, fReach,
			"prop '%s' stands %.4f m from the DirtLane while a player walking it "
			"reaches only %.4f m. It can NEVER be picked up, in any playthrough, and "
			"nothing else in this suite can see that -- its ground column resolves, "
			"its scene bytes needle, and its component units pass on compiled "
			"constants. MOVE THE ANCHOR; do not widen fZM_INTERACT_MAX_DISTANCE",
			xAnchor.m_szEntityName, (double)fToLane, (double)fReach);

		// ...AND INSIDE THE FLATTEN CORRIDOR, which is a different claim: reach says
		// the player can address it, the corridor says the ground under it was
		// levelled and its grass erased, so the prop is neither buried nor hidden in
		// encounter grass.
		ZENITH_ASSERT_LT(fToLane, fFlattenRadius,
			"prop '%s' stands %.4f m from the DirtLane, outside the %.4f m FLATTEN "
			"corridor. Its measured column is then unflattened ground carrying "
			"erosion deposit, and it may be standing in encounter grass",
			xAnchor.m_szEntityName, (double)fToLane, (double)fFlattenRadius);
	}

	// ★ THE ANTI-VACUITY ARM. A point pushed well past the reach must FAIL the same
	// predicate through the same helper -- otherwise the clause above could be
	// passing because Route1DistanceToLane returns something degenerate rather than
	// because the anchors are close to the lane.
	//
	// It is displaced from a REAL anchor along the perpendicular that anchor's own
	// foot already defines, so the failure it models is the actual mistake -- an
	// author nudging a prop off the path -- and it introduces no second geometry
	// that could disagree with the measurement it is testing.
	constexpr float fPROBE_LATERAL_OFFSET = 12.0f;
	const Route1PropAnchor& xFirst = axROUTE1_PROP_ANCHORS[0];
	const Route1LaneFoot xFoot = Route1ClosestPointOnLane(xFirst.m_fX, xFirst.m_fZ);
	ZENITH_ASSERT_TRUE(xFoot.m_bValid,
		"the DirtLane polyline did not resolve, so the anti-vacuity probe below "
		"cannot be constructed and the clauses above are unproven");

	const float fAwayX = xFirst.m_fX - xFoot.m_fX;
	const float fAwayZ = xFirst.m_fZ - xFoot.m_fZ;
	const float fAwayLength = std::sqrt(fAwayX * fAwayX + fAwayZ * fAwayZ);
	ZENITH_ASSERT_GT(fAwayLength, 0.0f,
		"prop '%s' sits exactly ON the lane centreline, so there is no perpendicular "
		"direction to push the anti-vacuity probe along", xFirst.m_szEntityName);

	const float fProbeX = xFoot.m_fX + (fAwayX / fAwayLength) * fPROBE_LATERAL_OFFSET;
	const float fProbeZ = xFoot.m_fZ + (fAwayZ / fAwayLength) * fPROBE_LATERAL_OFFSET;
	const float fProbeToLane = Route1DistanceToLane(fProbeX, fProbeZ);
	ZENITH_ASSERT_GT(fProbeToLane, fReach,
		"the ANTI-VACUITY arm failed: a probe displaced %.4f m off the lane measures "
		"only %.4f m from it and would still pass the %.4f m reach clause above, so "
		"that clause cannot red on the misplacement it was written for",
		(double)fPROBE_LATERAL_OFFSET, (double)fProbeToLane, (double)fReach);
}

// Each prop stands ON its own measured column, and the three columns are genuinely
// PER-PROP rather than one number copied three times.
ZENITH_TEST(ZM_WorldTraversal, Route1_GroundItemPropsStandOnTheirOwnMeasuredColumn)
{
	for (u_int uProp = 0u; uProp < uROUTE1_PROP_ANCHOR_COUNT; ++uProp)
	{
		const Route1PropAnchor& xAnchor = axROUTE1_PROP_ANCHORS[uProp];
		const ZM_Route1GroundSample& xRow =
			ZM_GetRoute1GroundSample(xAnchor.m_eGroundSample);

		// The row this prop reads is the row NAMED for it. A mis-paired enum value
		// would put a prop on somebody else's column with every height still finite
		// and every other clause still green.
		ZENITH_ASSERT_TRUE(strcmp(xRow.m_szEntityName, xAnchor.m_szEntityName) == 0,
			"prop '%s' reads ground row '%s' -- the anchor table and the ground "
			"table have been paired wrongly",
			xAnchor.m_szEntityName, xRow.m_szEntityName);

		// The column is the prop's own XZ, not merely a plausible height.
		ZENITH_ASSERT_EQ_FLOAT(xRow.m_fX, xAnchor.m_fX, fROUTE1_EXACT_EPSILON,
			"ground row '%s' was measured at X %.4f while the prop stands at X %.4f",
			xRow.m_szEntityName, (double)xRow.m_fX, (double)xAnchor.m_fX);
		ZENITH_ASSERT_EQ_FLOAT(xRow.m_fZ, xAnchor.m_fZ, fROUTE1_EXACT_EPSILON,
			"ground row '%s' was measured at Z %.4f while the prop stands at Z %.4f",
			xRow.m_szEntityName, (double)xRow.m_fZ, (double)xAnchor.m_fZ);

		// The authored centre is feet + HALF the cube: the prop rests on the surface
		// rather than being half-buried in it or hovering a whole edge above it.
		const float fCentreY = ZM_Route1PropCentreY(xAnchor.m_eGroundSample);
		ZENITH_ASSERT_EQ_FLOAT(fCentreY - xRow.m_fFeetY,
			0.5f * fZM_ROUTE1_PROP_CUBE_EDGE, fROUTE1_EXACT_EPSILON,
			"prop '%s' has its centre %.4f m above its measured surface, which is not "
			"half its %.4f m cube -- it is buried or floating",
			xAnchor.m_szEntityName, (double)(fCentreY - xRow.m_fFeetY),
			(double)fZM_ROUTE1_PROP_CUBE_EDGE);
	}

	// ★ THE THREE COLUMNS ARE DISTINCT. All three rows carrying one height is the
	// signature of a table that was seeded and never frozen -- which is precisely
	// the state this table's own banner says no compiled-constant unit can detect.
	// It cannot detect an unfrozen table in general; it CAN detect this one.
	bool bAnyDistinct = false;
	for (u_int uProp = 1u; uProp < uROUTE1_PROP_ANCHOR_COUNT; ++uProp)
	{
		if (ZM_Route1GroundFeetY(axROUTE1_PROP_ANCHORS[uProp].m_eGroundSample)
			!= ZM_Route1GroundFeetY(axROUTE1_PROP_ANCHORS[0].m_eGroundSample))
		{
			bAnyDistinct = true;
			break;
		}
	}
	ZENITH_ASSERT_TRUE(bAnyDistinct,
		"all %u prop ground columns carry the SAME height. Over a kilometre of "
		"eroded route that is not a measurement -- the rows are still seeded, and "
		"ZM_Route1GroundTruth_Test is what freezes them",
		uROUTE1_PROP_ANCHOR_COUNT);
}
