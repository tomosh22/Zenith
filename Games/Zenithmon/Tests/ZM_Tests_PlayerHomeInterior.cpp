#include "Zenith.h"

// ============================================================================
// ZM_Tests_PlayerHomeInterior (ZM-D-176) -- the boot units for the player's
// bedroom: the WARM INTERIOR TINT that stops it reading as the same greybox room
// as ProfLab, the seven-block NAME INVENTORY that tint is keyed on, and the
// GEOMETRY the new placement header derives.
//
// PURE, and it mirrors Tests/ZM_Tests_ProfLabPlacement.cpp deliberately: no
// scene, no entity, no physics, no assets, no graphics, no g_xEngine. Everything
// here reads COMPILED CONSTANTS -- Source/World/ZM_PlayerHomePlacement.h, its
// ProfLab sibling, and the shipped W4 palette seam in Source/Gen/ZM_HumanAppearance.h.
//
// ★ AND THESE UNITS CREATE NO ENTITY (ZM-D-148's precondition for two-boot scene
// byte stability). The tint is DERIVED at runtime from the entity name, not
// serialized, so there are no scene bytes for this change to move and nothing
// here needs a live scene to look at.
//
// ★ WHAT THESE UNITS CANNOT DO, STATED UP FRONT. They prove that the tint
// CONSTANT is warm, slight and far from the grey, and that the NAME PREDICATE
// selects exactly PlayerHome's seven shell blocks. They cannot prove the tint
// reached a material (that is ZM_InteriorTint_Test, headless) and they certainly
// cannot prove it reached a pixel (that is ZM_InteriorTintPixels_Test, which is
// graphics-required and therefore SKIPS -- i.e. passes -- in the headless gate).
// Do not read this file's greenness as "the bedroom is yellow".
//
// ★ AND NOTHING HERE RE-SPELLS THE TINT. The distinctness clause COMPUTES the
// separation between the two shipped colours with the shipped measuring function
// and asserts it against the shipped margin. A unit that restated the expected
// pairing would compare a literal against itself and could not red a drift.
// ============================================================================

#include <cmath>     // isfinite
#include <cstring>   // strcmp -- the name-identity claims

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Gen/ZM_HumanAppearance.h"       // the shipped grey, the margin, the metric
#include "Zenithmon/Source/World/ZM_PlayerHomePlacement.h"
#include "Zenithmon/Source/World/ZM_ProfLabPlacement.h"    // the OTHER room, for the cross-checks

namespace
{
	// "These two floats are the same authored number." Generous by float
	// standards and far below any placement quantity this room uses.
	constexpr float fPH_EXACT_EPSILON = 1.0e-4f;

	// "These two floats are the same authored PLANE." Looser than the equality
	// epsilon because a face coordinate is a sum of a centre and a half-extent,
	// so it carries one more rounding than a directly authored value.
	constexpr float fPH_PLANE_EPSILON = 1.0e-3f;

	// A closed room needs a floor plus four bounding sides at minimum. Asserted so
	// the [0, ZM_PLAYERHOME_BLOCK_COUNT) walks below cannot pass vacuously on an
	// empty or half-deleted table.
	constexpr u_int uPH_MIN_BLOCK_COUNT = 5u;

	// ---- The tint's three shape bounds --------------------------------------
	//
	// ★ THESE ARE PROPERTIES OF "SLIGHTLY YELLOW", NOT A RESTATEMENT OF THE
	// COLOUR. Each one is a band a whole family of colours satisfies, and each
	// rules out a specific wrong answer a separation-only clause would accept.

	// WARM: red must lead blue by a visible margin. A separation clause alone is
	// satisfied by a BLUE tint just as happily as by a yellow one.
	constexpr float fPH_MIN_RED_OVER_BLUE = 0.10f;

	// SLIGHT: HSV saturation ceiling. The ruling was "slightly yellow" -- a
	// cream/beige interior wall, not a saturated primary.
	constexpr float fPH_MAX_SATURATION = 0.40f;

	// NOT A BRIGHTNESS CHANGE: the tint must be a HUE move. If the two rooms
	// differed mainly in luma, the difference would be indistinguishable from an
	// exposure or lighting change and the ruling would not have been implemented.
	constexpr float fPH_MAX_LUMA_DELTA = 0.10f;

	// Rec709 relative luminance. Spelled once here rather than at each call.
	float PHLuma709(const Zenith_Maths::Vector4& xColour)
	{
		return 0.2126f * xColour.x + 0.7152f * xColour.y + 0.0722f * xColour.z;
	}

	// HSV saturation, (max - min) / max. FAILS CLOSED: a non-positive or
	// non-finite max yields 1.0, which is above every ceiling, so garbage cannot
	// satisfy a "saturation <= margin" clause.
	float PHSaturation(const Zenith_Maths::Vector4& xColour)
	{
		float fMax = xColour.x;
		float fMin = xColour.x;
		if (xColour.y > fMax) { fMax = xColour.y; }
		if (xColour.z > fMax) { fMax = xColour.z; }
		if (xColour.y < fMin) { fMin = xColour.y; }
		if (xColour.z < fMin) { fMin = xColour.z; }
		if (!std::isfinite(fMax) || !std::isfinite(fMin) || fMax <= 0.0f)
		{
			return 1.0f;
		}
		return (fMax - fMin) / fMax;
	}

	bool PHChannelIsUnitRange(float fChannel)
	{
		return std::isfinite(fChannel) && fChannel >= 0.0f && fChannel <= 1.0f;
	}

	bool PHBlockoutIsFinite(const ZM_PlayerHomeBlockout& xBlock)
	{
		return std::isfinite(xBlock.m_xCenter.x)
			&& std::isfinite(xBlock.m_xCenter.y)
			&& std::isfinite(xBlock.m_xCenter.z)
			&& std::isfinite(xBlock.m_xScale.x)
			&& std::isfinite(xBlock.m_xScale.y)
			&& std::isfinite(xBlock.m_xScale.z);
	}

	// Every printable, space-free character is a legal scene entity name byte.
	// These names are the KEYS the tint predicate matches on, so a stray space or
	// control byte is a silent miss rather than a diagnosable failure.
	bool PHNameIsALookupKey(const char* szName)
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

	// The doorway is an ABSENCE of geometry: the clear gap between the two front
	// stubs. DERIVED from the blockout table rather than read off the aperture
	// constant, so the two can be compared against each other.
	float PHApertureClearWidth()
	{
		return ZM_GetPlayerHomeBlock(ZM_PLAYERHOME_BLOCK_FRONT_RIGHT).Min().x
			- ZM_GetPlayerHomeBlock(ZM_PLAYERHOME_BLOCK_FRONT_LEFT).Max().x;
	}
}

// ============================================================================
// U1 -- the tint is a WARM, SLIGHT, DISTINCT colour.
// ============================================================================

// ★ THE CLAUSE THAT ANSWERS THE RULING, AT CONSTANT LEVEL. The user's complaint
// was that PlayerHome and ProfLab read as the SAME greybox room; the fix is a
// warm interior tint on PlayerHome's seven shell blocks only.
//
// The separation is MEASURED with the shipped ZM_HumanPaletteSeparation and
// compared against the shipped fZM_HUMAN_PALETTE_MIN_SEPARATION -- there is
// deliberately no second 0.15 in this file, and the expected pairing is never
// restated. A unit that spelled "the tint is (0.61, 0.57, 0.43)" would move in
// lockstep with the header and could not red anything.
//
// ★ AND THERE IS DELIBERATELY NO TINT-vs-NPC-PALETTE SEPARATION CLAUSE, BUT THE
// REASON IS NARROWER THAN IT USED TO BE. This comment used to rest on two legs:
// that PlayerHome authors no NPCs, and that ZM_HUMAN_PROF_ASTER sat only 0.0677
// from the greybox grey (a booked known limit) so a low Aster-vs-tint distance
// could fire for reasons unrelated to this file. THE SECOND LEG IS DEAD --
// Aster's hair moved GREY -> WHITE and he now sits 0.21547 from the grey (see
// ZM_HumanData.cpp). The FIRST leg still stands on its own and is the whole
// justification now: PlayerHome authors no NPCs at all, so the comparison would
// measure two things that never share a frame. If PlayerHome ever gains an
// occupant, this clause becomes worth adding rather than worth avoiding.
ZENITH_TEST(ZM_WorldTraversal, PlayerHome_TintIsDistinctFromTheBlockoutGrey)
{
	const Zenith_Maths::Vector4 xTint = ZM_GetPlayerHomeInteriorTintColour();
	const Zenith_Maths::Vector4 xGrey = ZM_GetHumanPaletteFallbackColour();

	// (0) WELL-FORMED. Every clause below reasons about magnitudes, so a
	//     non-finite or out-of-gamut channel has to be caught first -- and the
	//     alpha has to be the opaque 1 every other greybox material carries.
	ZENITH_ASSERT_TRUE(PHChannelIsUnitRange(xTint.x)
		&& PHChannelIsUnitRange(xTint.y) && PHChannelIsUnitRange(xTint.z),
		"the PlayerHome interior tint is (%.4f, %.4f, %.4f) -- a channel is "
		"non-finite or outside [0,1], so every measurement below is noise",
		(double)xTint.x, (double)xTint.y, (double)xTint.z);
	ZENITH_ASSERT_EQ_FLOAT(xTint.w, 1.0f, fPH_EXACT_EPSILON,
		"the PlayerHome interior tint carries alpha %.4f; the blockout material "
		"is opaque and every other greybox colour ships alpha 1",
		(double)xTint.w);

	// (1) ★ DISTINCT -- COMPUTED, NEVER RE-SPELLED. The measured value is LOGGED
	//     as well as asserted, so a future re-tune has a real number to work from
	//     rather than a pass/fail bit.
	const float fSeparation = ZM_HumanPaletteSeparation(xTint, xGrey);
	Zenith_Log(LOG_CATEGORY_UNITTEST,
		"[ZM_PlayerHomeInterior] tint (%.4f, %.4f, %.4f) vs blockout grey "
		"(%.4f, %.4f, %.4f): separation %.5f against a %.5f floor; "
		"luma709 %.5f vs %.5f; saturation %.5f",
		(double)xTint.x, (double)xTint.y, (double)xTint.z,
		(double)xGrey.x, (double)xGrey.y, (double)xGrey.z,
		(double)fSeparation, (double)fZM_HUMAN_PALETTE_MIN_SEPARATION,
		(double)PHLuma709(xTint), (double)PHLuma709(xGrey),
		(double)PHSaturation(xTint));
	ZENITH_ASSERT_GE(fSeparation, fZM_HUMAN_PALETTE_MIN_SEPARATION,
		"the PlayerHome interior tint sits only %.5f from the shipped blockout "
		"grey, under the %.5f margin every authored appearance owes it -- the "
		"bedroom would still read as the same room as ProfLab, which is exactly "
		"the complaint ZM-D-176 exists to answer",
		(double)fSeparation, (double)fZM_HUMAN_PALETTE_MIN_SEPARATION);

	// (2) WARM. Red leads blue, and the channels descend R > G > B. This is what
	//     makes "yellow" a property rather than a hope: the shipped grey is COOL
	//     (B > G > R), so the ordering is INVERTED, and that inversion is what
	//     makes the two rooms read as different rooms rather than as the same
	//     room under a different exposure.
	ZENITH_ASSERT_GE(xTint.x - xTint.z, fPH_MIN_RED_OVER_BLUE,
		"the tint's red leads its blue by only %.4f (need %.2f) -- the "
		"separation clause above is satisfied just as well by a BLUE tint, so "
		"this is the clause that says WARM",
		(double)(xTint.x - xTint.z), (double)fPH_MIN_RED_OVER_BLUE);
	ZENITH_ASSERT_GT(xTint.x, xTint.y,
		"the tint's red (%.4f) does not lead its green (%.4f); the descending "
		"R > G > B ordering is what makes it read yellow/amber rather than green",
		(double)xTint.x, (double)xTint.y);
	ZENITH_ASSERT_GT(xTint.y, xTint.z,
		"the tint's green (%.4f) does not lead its blue (%.4f); with G at or "
		"below B the hue swings toward red/magenta rather than yellow",
		(double)xTint.y, (double)xTint.z);

	// (3) SLIGHT. The ruling said "slightly yellow" -- a warm interior wall, not
	//     a saturated primary. Without this the WARM clauses above are satisfied
	//     by pure yellow.
	const float fSaturation = PHSaturation(xTint);
	ZENITH_ASSERT_LE(fSaturation, fPH_MAX_SATURATION,
		"the tint's HSV saturation is %.4f, above the %.2f ceiling -- "
		"'slightly yellow' is a cream/beige wall, and past this the room reads "
		"as a painted set rather than as a neutral interior",
		(double)fSaturation, (double)fPH_MAX_SATURATION);

	// (4) A HUE MOVE, NOT A BRIGHTNESS MOVE. If the tint mainly changed luma,
	//     the two rooms would differ the way two exposures differ, and a viewer
	//     would read it as a lighting bug rather than as a different room.
	const float fLumaDelta = PHLuma709(xTint) - PHLuma709(xGrey);
	const float fAbsLumaDelta = fLumaDelta < 0.0f ? -fLumaDelta : fLumaDelta;
	ZENITH_ASSERT_LE(fAbsLumaDelta, fPH_MAX_LUMA_DELTA,
		"the tint shifts Rec709 luminance by %.4f (limit %.2f) -- at that size "
		"the change reads as an exposure/lighting move rather than as a hue, and "
		"ZM-D-176 asked for a room that looks DIFFERENT, not one that looks "
		"brighter", (double)fAbsLumaDelta, (double)fPH_MAX_LUMA_DELTA);
}

// ============================================================================
// U2 -- the name inventory the tint is keyed on.
// ============================================================================

// The tint has no serialized carrier: ZM_GreyboxVisual::ResolveBaseColour asks
// ZM_IsPlayerHomeBlockName whether the entity it is dressing is one of the seven
// shell blocks. That predicate is therefore the entire wiring, and its two ways
// of being wrong are opposites:
//
//   * too NARROW (a renamed block, a typo) -- some of the room stays grey;
//   * too WIDE (a prefix or substring match) -- it would also catch
//     PlayerHomeCamera / PlayerHomeExitTrigger, and a careless generalisation
//     could reach ProfLab's blocks and repaint the room that must not move.
//
// The ProfLab cross-check below is what catches the WIDE failure, and it is a
// cross-check between two REAL headers rather than against typed-out strings --
// so it follows a rename on either side.
ZENITH_TEST(ZM_WorldTraversal, PlayerHome_BlockNamesAreTheTintedInventory)
{
	// ANTI-VACUITY. Delete the table and every walk below passes by never
	// iterating.
	ZENITH_ASSERT_GE((u_int)ZM_PLAYERHOME_BLOCK_COUNT, uPH_MIN_BLOCK_COUNT,
		"the PlayerHome blockout table has %u entries; a closed room needs at "
		"least a floor and four bounding sides, and every walk in this file "
		"passes vacuously below that", (u_int)ZM_PLAYERHOME_BLOCK_COUNT);

	for (u_int u = 0u; u < (u_int)ZM_PLAYERHOME_BLOCK_COUNT; ++u)
	{
		const ZM_PLAYERHOME_BLOCK eBlock = (ZM_PLAYERHOME_BLOCK)u;
		const char* szName = ZM_GetPlayerHomeBlockName(eBlock);

		ZENITH_ASSERT_TRUE(PHNameIsALookupKey(szName),
			"block %u's name is null, empty, or carries a non-printable byte -- "
			"the authoring would create an entity the tint predicate can never "
			"match, so that block would silently stay grey", u);

		// THE POSITIVE HALF: every name the AUTHORING writes is a name the TINT
		// matches. Both sides call the header, so a rename moves them together --
		// which is the point: one spelling, no drift.
		ZENITH_ASSERT_TRUE(ZM_IsPlayerHomeBlockName(szName),
			"ZM_GetPlayerHomeBlockName(%u) returns '%s' but "
			"ZM_IsPlayerHomeBlockName rejects it -- the authoring and the tint "
			"resolver disagree, and that block would be authored grey",
			u, szName != nullptr ? szName : "(null)");
	}

	// Unique. A duplicated name would not break the tint (both would match) but
	// it WOULD break the automated tint scan's count, which expects exactly
	// ZM_PLAYERHOME_BLOCK_COUNT distinct entities.
	for (u_int uA = 0u; uA < (u_int)ZM_PLAYERHOME_BLOCK_COUNT; ++uA)
	{
		for (u_int uB = uA + 1u; uB < (u_int)ZM_PLAYERHOME_BLOCK_COUNT; ++uB)
		{
			const char* szA = ZM_GetPlayerHomeBlockName((ZM_PLAYERHOME_BLOCK)uA);
			const char* szB = ZM_GetPlayerHomeBlockName((ZM_PLAYERHOME_BLOCK)uB);
			if (szA == nullptr || szB == nullptr)
			{
				continue;   // already reported above
			}
			ZENITH_ASSERT_NE(std::strcmp(szA, szB), 0,
				"blocks %u and %u share the entity name '%s' -- two authored "
				"entities would collide and the tint scan's seven-block count "
				"would be measuring six", uA, uB, szA);
		}
	}

	// ★ THE CROSS-CHECK THAT CATCHES A DEGENERATED PREDICATE. ProfLab's seven
	// blocks must be REJECTED. This is asserted between two real headers, not
	// against typed-out strings, so it survives a rename on either side -- and a
	// predicate that had become a prefix/substring/always-true match reds here
	// rather than quietly repainting the room the ruling said must not change.
	for (u_int u = 0u; u < (u_int)ZM_PROFLAB_BLOCK_COUNT; ++u)
	{
		const char* szProfLab = ZM_GetProfLabBlockName((ZM_PROFLAB_BLOCK)u);
		ZENITH_ASSERT_FALSE(ZM_IsPlayerHomeBlockName(szProfLab),
			"ZM_IsPlayerHomeBlockName accepts ProfLab's '%s' -- the tint "
			"predicate has widened past exact matching, and ProfLab (which "
			"ZM-D-176 says must be left exactly as it is) would be repainted",
			szProfLab != nullptr ? szProfLab : "(null)");
	}

	// ★ THE NEGATIVE SPELLINGS BELOW ARE DELIBERATE, AND THEY ARE SAFE. Every
	// other clause in this file refuses to re-spell a constant, because a
	// re-spelled POSITIVE cannot red a drift. A re-spelled NEGATIVE cannot go
	// silently green: if one of these entities were ever renamed, the predicate
	// would still reject the old string, so the clause keeps holding for the
	// right reason. What it pins is the shape of the match -- these four names
	// share PlayerHome's prefix or Dawnmere's blockout family, and a predicate
	// that matched on anything less than the whole string would take them.
	const char* aszMustReject[] =
	{
		"PlayerHomeExitTrigger",   // shares the prefix, carries no ZM_GreyboxVisual
		"PlayerHomeCamera",        // ditto
		"Player",                  // the capsule, in this very scene
		"DawnmereHomeShell",       // another scene's blockout body
		"DawnmereHomeDoorLeft",    // ...and another
		"",                        // empty
	};
	for (u_int u = 0u;
		u < (u_int)(sizeof(aszMustReject) / sizeof(aszMustReject[0])); ++u)
	{
		ZENITH_ASSERT_FALSE(ZM_IsPlayerHomeBlockName(aszMustReject[u]),
			"ZM_IsPlayerHomeBlockName accepts '%s', which is not one of the seven "
			"shell blocks -- the match is looser than the exact-name contract",
			aszMustReject[u]);
	}

	// TOTAL, and DEGENERATE out of range. The predicate runs from OnStart at
	// serialization order 107, so it may never assert; the header's contract is a
	// defined answer for every input, including a null pointer and the COUNT
	// sentinel, and a sentinel name no authored entity carries.
	ZENITH_ASSERT_FALSE(ZM_IsPlayerHomeBlockName(nullptr),
		"ZM_IsPlayerHomeBlockName(nullptr) must be false -- it runs from OnStart "
		"in every configuration and may neither assert nor read through null");
	const char* szSentinel = ZM_GetPlayerHomeBlockName(ZM_PLAYERHOME_BLOCK_COUNT);
	ZENITH_ASSERT_NOT_NULL(szSentinel,
		"ZM_GetPlayerHomeBlockName is not total: the out-of-range answer is null "
		"rather than a degenerate sentinel");
	ZENITH_ASSERT_FALSE(ZM_IsPlayerHomeBlockName(szSentinel),
		"the out-of-range block name '%s' is accepted by the tint predicate -- a "
		"caller that indexed past the end would silently match a real block "
		"instead of failing at its own assertion",
		szSentinel != nullptr ? szSentinel : "(null)");
}

// ============================================================================
// U3 -- the room the placement header derives.
// ============================================================================

// ★ THE TRANSCRIPTION GUARD. This header did not exist before ZM-D-176: every one
// of PlayerHome's coordinates was a raw float literal typed inline in
// Zenithmon.cpp's authoring block, which no Tests/ TU could name (the shipped
// ZM_ProfLabPlacement.h records that gap in as many words). The new header
// re-derives those literals from seven room numbers using ProfLab's formulas, and
// the acceptance gate for that derivation is that PlayerHome.zscen does not move
// a single byte.
//
// These clauses are the CHEAP half of that gate -- the properties that say "this
// is still a closed room with a doorway in it" without needing a boot. They are
// modelled on ProfLab's BlockoutExtentsArePositiveAndFinite +
// ShellEnclosesTheFloorOnEverySide + DoorApertureAdmitsTheAuthoredPlayer.
ZENITH_TEST(ZM_WorldTraversal, PlayerHome_BlockoutGeometryIsTheAuthoredRoom)
{
	// (0) Every piece is a real box.
	for (u_int u = 0u; u < (u_int)ZM_PLAYERHOME_BLOCK_COUNT; ++u)
	{
		const ZM_PLAYERHOME_BLOCK eBlock = (ZM_PLAYERHOME_BLOCK)u;
		const ZM_PlayerHomeBlockout xBlock = ZM_GetPlayerHomeBlock(eBlock);
		const char* szName = ZM_GetPlayerHomeBlockName(eBlock);

		ZENITH_ASSERT_TRUE(PHBlockoutIsFinite(xBlock),
			"block '%s' carries a non-finite centre or scale",
			szName != nullptr ? szName : "(null)");
		ZENITH_ASSERT_GT(xBlock.m_xScale.x, 0.0f,
			"block '%s' has X extent %.5f -- a non-positive extent inverts its "
			"AABB and every containment claim about it becomes noise",
			szName != nullptr ? szName : "(null)", (double)xBlock.m_xScale.x);
		ZENITH_ASSERT_GT(xBlock.m_xScale.y, 0.0f,
			"block '%s' has Y extent %.5f -- a non-positive extent inverts its AABB",
			szName != nullptr ? szName : "(null)", (double)xBlock.m_xScale.y);
		ZENITH_ASSERT_GT(xBlock.m_xScale.z, 0.0f,
			"block '%s' has Z extent %.5f -- a non-positive extent inverts its AABB",
			szName != nullptr ? szName : "(null)", (double)xBlock.m_xScale.z);
	}

	const ZM_PlayerHomeBlockout xFloor =
		ZM_GetPlayerHomeBlock(ZM_PLAYERHOME_BLOCK_FLOOR);
	const ZM_PlayerHomeBlockout xBack =
		ZM_GetPlayerHomeBlock(ZM_PLAYERHOME_BLOCK_BACK_WALL);
	const ZM_PlayerHomeBlockout xLeftWall =
		ZM_GetPlayerHomeBlock(ZM_PLAYERHOME_BLOCK_LEFT_WALL);
	const ZM_PlayerHomeBlockout xRightWall =
		ZM_GetPlayerHomeBlock(ZM_PLAYERHOME_BLOCK_RIGHT_WALL);
	const ZM_PlayerHomeBlockout xFrontLeft =
		ZM_GetPlayerHomeBlock(ZM_PLAYERHOME_BLOCK_FRONT_LEFT);
	const ZM_PlayerHomeBlockout xFrontRight =
		ZM_GetPlayerHomeBlock(ZM_PLAYERHOME_BLOCK_FRONT_RIGHT);
	const ZM_PlayerHomeBlockout xLintel =
		ZM_GetPlayerHomeBlock(ZM_PLAYERHOME_BLOCK_LINTEL);

	// (1) THE FLOOR'S TOP FACE IS THE FEET PLANE. Every spawn marker and every
	//     wall foot in this room is stated against it, so the slab thickness must
	//     stay entirely BELOW y = fZM_PLAYERHOME_FLOOR_TOP_Y.
	ZENITH_ASSERT_EQ_FLOAT(xFloor.Max().y, fZM_PLAYERHOME_FLOOR_TOP_Y,
		fPH_PLANE_EPSILON,
		"the floor's top face is at y=%.5f but the header declares the feet "
		"plane at y=%.5f -- the derived floor centre no longer sinks the slab by "
		"half its thickness", (double)xFloor.Max().y,
		(double)fZM_PLAYERHOME_FLOOR_TOP_Y);
	ZENITH_ASSERT_EQ_FLOAT(xFloor.m_xScale.y, fZM_PLAYERHOME_FLOOR_THICKNESS,
		fPH_EXACT_EPSILON,
		"the floor slab is %.5f m thick but the header declares %.5f",
		(double)xFloor.m_xScale.y, (double)fZM_PLAYERHOME_FLOOR_THICKNESS);

	// (2) THE FIVE BOUNDING SIDES STAND ON THAT PLANE AND RISE TO THE WALL
	//     HEIGHT. The LINTEL is deliberately excluded: it bridges the doorway
	//     from the aperture height up, so its foot is NOT on the floor, and a
	//     clause that included it would have to be weakened until it proved
	//     nothing.
	const ZM_PLAYERHOME_BLOCK aeSides[] =
	{
		ZM_PLAYERHOME_BLOCK_BACK_WALL,
		ZM_PLAYERHOME_BLOCK_LEFT_WALL,
		ZM_PLAYERHOME_BLOCK_RIGHT_WALL,
		ZM_PLAYERHOME_BLOCK_FRONT_LEFT,
		ZM_PLAYERHOME_BLOCK_FRONT_RIGHT,
	};
	for (u_int uSide = 0u;
		uSide < (u_int)(sizeof(aeSides) / sizeof(aeSides[0])); ++uSide)
	{
		const ZM_PlayerHomeBlockout xSide = ZM_GetPlayerHomeBlock(aeSides[uSide]);
		const char* szName = ZM_GetPlayerHomeBlockName(aeSides[uSide]);
		ZENITH_ASSERT_EQ_FLOAT(xSide.Min().y, fZM_PLAYERHOME_FLOOR_TOP_Y,
			fPH_PLANE_EPSILON,
			"'%s' has its foot at y=%.5f rather than on the floor's top face "
			"(%.5f) -- it either floats or sinks into the slab",
			szName != nullptr ? szName : "(null)", (double)xSide.Min().y,
			(double)fZM_PLAYERHOME_FLOOR_TOP_Y);
		ZENITH_ASSERT_EQ_FLOAT(xSide.Max().y,
			fZM_PLAYERHOME_FLOOR_TOP_Y + fZM_PLAYERHOME_WALL_HEIGHT,
			fPH_PLANE_EPSILON,
			"'%s' tops out at y=%.5f rather than at the authored wall height "
			"(%.5f above the floor)", szName != nullptr ? szName : "(null)",
			(double)xSide.Max().y, (double)fZM_PLAYERHOME_WALL_HEIGHT);
	}

	// (3) THE SHELL CLOSES ON THE FLOOR FOOTPRINT. A wall authored one scale
	//     component short leaves a corner gap that no doorway clause would ever
	//     notice.
	ZENITH_ASSERT_LT(xBack.m_xCenter.z, xFloor.m_xCenter.z,
		"the back wall (z=%.4f) is not on the -Z side of the floor (z=%.4f)",
		(double)xBack.m_xCenter.z, (double)xFloor.m_xCenter.z);
	ZENITH_ASSERT_GT(xFrontLeft.m_xCenter.z, xFloor.m_xCenter.z,
		"the doorway wall (z=%.4f) is not on the +Z side of the floor (z=%.4f) "
		"-- the shipped PlayerHomeCamera trails toward -Z, so the entrance has "
		"to be the +Z face",
		(double)xFrontLeft.m_xCenter.z, (double)xFloor.m_xCenter.z);
	ZENITH_ASSERT_LE(xBack.Min().x, xFloor.Min().x + fPH_PLANE_EPSILON,
		"the back wall starts at x=%.4f, inside the floor's -X edge (%.4f)",
		(double)xBack.Min().x, (double)xFloor.Min().x);
	ZENITH_ASSERT_GE(xBack.Max().x, xFloor.Max().x - fPH_PLANE_EPSILON,
		"the back wall ends at x=%.4f, short of the floor's +X edge (%.4f)",
		(double)xBack.Max().x, (double)xFloor.Max().x);
	ZENITH_ASSERT_LE(xLeftWall.Min().z, xFloor.Min().z + fPH_PLANE_EPSILON,
		"the -X wall starts at z=%.4f, inside the floor's -Z edge (%.4f)",
		(double)xLeftWall.Min().z, (double)xFloor.Min().z);
	ZENITH_ASSERT_GE(xLeftWall.Max().z, xFloor.Max().z - fPH_PLANE_EPSILON,
		"the -X wall ends at z=%.4f, short of the floor's +Z edge (%.4f)",
		(double)xLeftWall.Max().z, (double)xFloor.Max().z);
	ZENITH_ASSERT_LE(xRightWall.Min().z, xFloor.Min().z + fPH_PLANE_EPSILON,
		"the +X wall starts at z=%.4f, inside the floor's -Z edge (%.4f)",
		(double)xRightWall.Min().z, (double)xFloor.Min().z);
	ZENITH_ASSERT_GE(xRightWall.Max().z, xFloor.Max().z - fPH_PLANE_EPSILON,
		"the +X wall ends at z=%.4f, short of the floor's +Z edge (%.4f)",
		(double)xRightWall.Max().z, (double)xFloor.Max().z);
	ZENITH_ASSERT_LE(xFrontLeft.Min().x, xFloor.Min().x + fPH_PLANE_EPSILON,
		"the doorway's -X stub starts at x=%.4f, inside the floor's -X edge "
		"(%.4f) -- a gap opens beside the jamb",
		(double)xFrontLeft.Min().x, (double)xFloor.Min().x);
	ZENITH_ASSERT_GE(xFrontRight.Max().x, xFloor.Max().x - fPH_PLANE_EPSILON,
		"the doorway's +X stub ends at x=%.4f, short of the floor's +X edge "
		"(%.4f) -- a gap opens beside the jamb",
		(double)xFrontRight.Max().x, (double)xFloor.Max().x);

	// (4) THE DOORWAY IS THE APERTURE THE HEADER DECLARES. Measured from the two
	//     stubs that BOUND the gap and compared against the aperture constant, so
	//     the geometry and the number it is derived from are reconciled rather
	//     than merely both present.
	ZENITH_ASSERT_EQ_FLOAT(xFrontLeft.m_xCenter.z, xFrontRight.m_xCenter.z,
		fPH_EXACT_EPSILON,
		"the doorway's two front stubs sit on different Z planes (%.4f vs %.4f)",
		(double)xFrontLeft.m_xCenter.z, (double)xFrontRight.m_xCenter.z);
	ZENITH_ASSERT_LT(xFrontLeft.m_xCenter.x, xFrontRight.m_xCenter.x,
		"the front-left stub (x=%.4f) is not on the -X side of the front-right "
		"stub (x=%.4f) -- the aperture width would be measured backwards",
		(double)xFrontLeft.m_xCenter.x, (double)xFrontRight.m_xCenter.x);
	ZENITH_ASSERT_EQ_FLOAT(PHApertureClearWidth(),
		2.0f * fZM_PLAYERHOME_APERTURE_HALF_WIDTH, fPH_PLANE_EPSILON,
		"the clear gap between the doorway stubs is %.4f m but the header "
		"declares a %.4f m aperture -- widen a stub any further and the bedroom "
		"has no usable exit, which is the ONLY way out of the scene a new run "
		"now starts in", (double)PHApertureClearWidth(),
		(double)(2.0f * fZM_PLAYERHOME_APERTURE_HALF_WIDTH));

	// ...and the lintel caps that gap, starting at the authored aperture height.
	ZENITH_ASSERT_EQ_FLOAT(xLintel.Min().y,
		fZM_PLAYERHOME_FLOOR_TOP_Y + fZM_PLAYERHOME_APERTURE_HEIGHT,
		fPH_PLANE_EPSILON,
		"the lintel's underside is at y=%.5f but the header declares a %.4f m "
		"aperture height above the floor -- the headroom the doorway offers is "
		"not the headroom the header claims",
		(double)xLintel.Min().y, (double)fZM_PLAYERHOME_APERTURE_HEIGHT);
	ZENITH_ASSERT_EQ_FLOAT(xLintel.Max().y,
		fZM_PLAYERHOME_FLOOR_TOP_Y + fZM_PLAYERHOME_WALL_HEIGHT,
		fPH_PLANE_EPSILON,
		"the lintel tops out at y=%.5f rather than flush with the wall height "
		"(%.4f above the floor) -- a slot opens above the door",
		(double)xLintel.Max().y, (double)fZM_PLAYERHOME_WALL_HEIGHT);
	ZENITH_ASSERT_LE(xLintel.Min().x, xFrontLeft.Max().x + fPH_PLANE_EPSILON,
		"the lintel starts at x=%.4f, inside the opening whose -X jamb ends at "
		"%.4f -- it does not bridge the doorway",
		(double)xLintel.Min().x, (double)xFrontLeft.Max().x);
	ZENITH_ASSERT_GE(xLintel.Max().x, xFrontRight.Min().x - fPH_PLANE_EPSILON,
		"the lintel ends at x=%.4f, short of the +X jamb at %.4f",
		(double)xLintel.Max().x, (double)xFrontRight.Min().x);

	// (5) ★ AND THIS ROOM IS NOT PROFLAB'S. The two interiors share a formula
	//     family, a block enum and a seven-piece shell -- which is exactly why a
	//     header transcribed from the wrong sibling would look right. The same
	//     property ZM_ProfLabPlacement.h claims for itself, asserted from this
	//     side: differing dimensions mean a scene registration pointed at the
	//     wrong .zscen disagrees on GEOMETRY as well as on entity names.
	ZENITH_ASSERT_NE(fZM_PLAYERHOME_HALF_WIDTH, fZM_PROFLAB_HALF_WIDTH,
		"PlayerHome and ProfLab now share half-width %.4f -- the two greybox "
		"interiors are dimensionally identical, so nothing but the entity names "
		"distinguishes them and the ZM-D-176 tint is carrying the whole "
		"'these are different rooms' claim on its own",
		(double)fZM_PLAYERHOME_HALF_WIDTH);
	ZENITH_ASSERT_NE(fZM_PLAYERHOME_HALF_DEPTH, fZM_PROFLAB_HALF_DEPTH,
		"PlayerHome and ProfLab now share half-depth %.4f",
		(double)fZM_PLAYERHOME_HALF_DEPTH);
}
