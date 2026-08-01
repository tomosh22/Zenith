#pragma once

#include "Maths/Zenith_Maths.h"   // Vector3 / Vector4

#include <cstring>                // strcmp -- the tint predicate is an EXACT name match

// ============================================================================
// ZM_PlayerHomePlacement (ZM-D-176) -- the authored PlayerHome interior
// coordinates, the seven-block name inventory, and the warm interior tint, in
// ONE place that BOTH the tools scene authoring and the tests can read.
//
// PURE. No ECS, no scene, no physics, no g_xEngine, no allocation, no I/O, and
// no ZENITH_TOOLS guard -- these constants have to be visible to boot units that
// run in a headless CI build where Project_RegisterEditorAutomationSteps is
// compiled out entirely, AND to ZM_GreyboxVisual, which compiles in EVERY
// configuration. HEADER-ONLY: every accessor is `inline`, so there is no paired
// .cpp to keep in step.
//
// WHY THIS FILE EXISTS. Source/World/ZM_ProfLabPlacement.h:15-25 records that
// the shipped PlayerHome interior had NO placement header: every one of its
// coordinates was a raw float literal typed inline in Zenithmon.cpp inside a
// ZENITH_TOOLS-only function, unnameable from any Tests/ TU. ZM-D-176 needs a
// tint constant and a block-name inventory that a test can NAME rather than
// re-spell -- a constant spelled twice cannot red a drift -- so exactly that
// much of the debt is closed here, and no more. An unread constant is not a
// check, it is a number that LOOKS like one.
//
// ★ NEVER RE-SPELL A LITERAL FROM THIS FILE AT A CALL SITE. If you need a
// number here, include this header and read it.
//
// ★ THE BUILD INDEX IS NOT HERE. PlayerHome's build index (and its scene kind,
// connections and offered spawn tags) live in the compiled world table --
// Source/Data/ZM_WorldSpec.cpp, row ZM_SCENE_PLAYERHOME. Read it with
// ZM_GetWorldSpec(ZM_SCENE_PLAYERHOME).m_uBuildIndex. Duplicating it here would
// create a second inventory nothing reconciles.
//
// ★ NO SPAWN / PLAYER / CAMERA / EXIT-TRIGGER CONSTANTS ARE HERE, DELIBERATELY.
// DoorSpawn, Player, PlayerHomeCamera and PlayerHomeExitTrigger keep their
// authored literals: nothing in this change reads them, and adding them would
// be adding unread numbers.
// ============================================================================

// ---- The room, as the seven numbers everything else is derived from ---------
//
// PlayerHome is a 16 x 12 m room with 3 m walls -- deliberately NOT ProfLab's
// 20 x 16 m hall, so a registration pointed at the wrong .zscen disagrees on
// geometry as well as on entity names.
//
// The floor's TOP FACE is exactly y = 0, so a spawn marker's FEET height is
// simply 0 and no consumer has to know the slab thickness to place a body.
//
// ★ EVERY DERIVED VALUE BELOW IS A DYADIC RATIONAL computed by exact halves and
// sums, so ZM_GetPlayerHomeBlock reproduces the seven literals this header
// replaced BIT FOR BIT. That is why ZM-D-176 moves no PlayerHome.zscen byte.
inline constexpr float fZM_PLAYERHOME_HALF_WIDTH          = 8.0f;   // +/- X inner extent
inline constexpr float fZM_PLAYERHOME_HALF_DEPTH          = 6.0f;   // +/- Z inner extent
inline constexpr float fZM_PLAYERHOME_WALL_THICKNESS      = 0.5f;
inline constexpr float fZM_PLAYERHOME_WALL_HEIGHT         = 3.0f;
inline constexpr float fZM_PLAYERHOME_FLOOR_THICKNESS     = 0.5f;

// The entrance is an ABSENCE of geometry, not a door panel: the +Z wall is
// authored as two side panels flanking a gap, with a lintel bridging the top.
inline constexpr float fZM_PLAYERHOME_APERTURE_HALF_WIDTH = 2.0f;
inline constexpr float fZM_PLAYERHOME_APERTURE_HEIGHT     = 2.5f;

// The floor's top face, and therefore every FEET height in this scene.
inline constexpr float fZM_PLAYERHOME_FLOOR_TOP_Y         = 0.0f;

// ---- ZM-D-176: the warm interior tint ---------------------------------------
//
// The user ruling: the PlayerHome interior becomes SLIGHTLY YELLOW so the
// player's bedroom stops reading as the same greybox room as ProfLab, which
// ships an identical seven-block shell. Justified against the shipped blockout
// grey (0.52, 0.55, 0.60) in Source/Gen/ZM_HumanAppearance.h:61-63:
//
//   channel order  grey is B > G > R (cool); this is R > G > B (warm). The hue
//                  is INVERTED, which is what makes the two rooms read apart.
//   HSV hue        46.7 degrees -- yellow/amber, not orange (G sits near R) and
//                  not green.
//   HSV saturation 0.295 -- "slightly": a cream/beige wall, not a saturated
//                  primary. A boot unit caps it at 0.40.
//   Rec709 luma    0.5684 vs the grey's 0.5472 (+0.021), so this is a HUE move
//                  and cannot be mistaken for an exposure/lighting change.
//   separation     0.19339 from the grey, measured with the shipped
//                  ZM_HumanPaletteSeparation -- 1.29x the shipped
//                  fZM_HUMAN_PALETTE_MIN_SEPARATION floor of 0.15.
//
// ★ fZM_GREYBOX_FALLBACK_* MUST NOT MOVE. It is worn by every blockout body in
// every scene (ProfLab's seven blocks, Dawnmere's four, every prop) and is read
// by live boot units. This is an ADDED colour keyed to PlayerHome, never an
// edit to the shared fallback.
inline constexpr float fZM_PLAYERHOME_TINT_R = 0.61f;
inline constexpr float fZM_PLAYERHOME_TINT_G = 0.57f;
inline constexpr float fZM_PLAYERHOME_TINT_B = 0.43f;

inline Zenith_Maths::Vector4 ZM_GetPlayerHomeInteriorTintColour()
{
	return Zenith_Maths::Vector4(
		fZM_PLAYERHOME_TINT_R,
		fZM_PLAYERHOME_TINT_G,
		fZM_PLAYERHOME_TINT_B,
		1.0f);
}

// ---- The shell blockout -----------------------------------------------------

// The seven pieces of the shell, in AUTHORING ORDER. The authoring loop walks
// [0, ZM_PLAYERHOME_BLOCK_COUNT), so this order is part of the contract:
// appending a block is fine, REORDERING REWRITES THE SCENE BYTES (ZM-D-148
// dense authoring-order file indices).
enum ZM_PLAYERHOME_BLOCK : u_int
{
	ZM_PLAYERHOME_BLOCK_FLOOR,
	ZM_PLAYERHOME_BLOCK_BACK_WALL,
	ZM_PLAYERHOME_BLOCK_LEFT_WALL,
	ZM_PLAYERHOME_BLOCK_RIGHT_WALL,
	ZM_PLAYERHOME_BLOCK_FRONT_LEFT,
	ZM_PLAYERHOME_BLOCK_FRONT_RIGHT,
	ZM_PLAYERHOME_BLOCK_LINTEL,

	ZM_PLAYERHOME_BLOCK_COUNT
};

// ★ BOOKED DUPLICATION, not an oversight: this struct is line-for-line
// identical to ZM_ProfLabBlockout in the sibling Source/World/ZM_ProfLabPlacement.h
// (which itself duplicates ZM_DawnmereBlockout). Consolidating all three into a
// shared Source/World/ZM_Blockout.h would touch the Dawnmere and ProfLab
// authoring blocks and their test suites, which is out of ZM-D-176's scope. Do
// the merge as its own change, not as a drive-by.
struct ZM_PlayerHomeBlockout
{
	Zenith_Maths::Vector3 m_xCenter;
	Zenith_Maths::Vector3 m_xScale;

	// Half-extents, i.e. what the AABB arithmetic actually wants.
	Zenith_Maths::Vector3 HalfExtent() const { return m_xScale * 0.5f; }
	Zenith_Maths::Vector3 Min() const { return m_xCenter - HalfExtent(); }
	Zenith_Maths::Vector3 Max() const { return m_xCenter + HalfExtent(); }
};

// ============================================================================
// EVERY ACCESSOR BELOW IS TOTAL, in the ZM_GetProfLabBlock house style: no
// argument value, however degenerate, is UB, and NONE OF THEM CALLS
// Zenith_Assert. Zenith_Assert calls Zenith_DebugBreak() in EVERY configuration;
// ZM_IsPlayerHomeBlockName runs from ZM_GreyboxVisual::OnStart at serialization
// order 107 on every scene load, and the whole unit suite runs at boot, so an
// assert here would not fail one test -- it would end the run.
//
// The out-of-range answers are deliberately DEGENERATE rather than plausible: an
// all-zero blockout and a name no authored entity carries, so a caller that
// mistakenly indexes past the end fails loudly at its own assertion instead of
// silently matching a real block.
// ============================================================================

// The floor is the block the pixel probe looks up first, so its name is pinned
// separately for a caller that wants it without spelling an enumerator.
// ZM_GetPlayerHomeBlockName RETURNS THIS CONSTANT rather than a second copy of
// the literal -- one spelling, so the two can never disagree.
inline constexpr const char* szZM_PLAYERHOME_FLOOR_ENTITY_NAME = "PlayerHomeFloor";

inline const char* ZM_GetPlayerHomeBlockName(ZM_PLAYERHOME_BLOCK eBlock)
{
	switch (eBlock)
	{
	case ZM_PLAYERHOME_BLOCK_FLOOR:       return szZM_PLAYERHOME_FLOOR_ENTITY_NAME;
	case ZM_PLAYERHOME_BLOCK_BACK_WALL:   return "PlayerHomeBackWall";
	case ZM_PLAYERHOME_BLOCK_LEFT_WALL:   return "PlayerHomeLeftWall";
	case ZM_PLAYERHOME_BLOCK_RIGHT_WALL:  return "PlayerHomeRightWall";
	case ZM_PLAYERHOME_BLOCK_FRONT_LEFT:  return "PlayerHomeFrontLeft";
	case ZM_PLAYERHOME_BLOCK_FRONT_RIGHT: return "PlayerHomeFrontRight";
	case ZM_PLAYERHOME_BLOCK_LINTEL:      return "PlayerHomeLintel";
	default: break;
	}
	return "PlayerHomeInvalidBlock";
}

// Centre + scale for one shell piece. Every value is DERIVED from the room
// numbers above by the SAME formulas ZM_GetProfLabBlock spells, verbatim:
//   floor       centre y = -floorThickness/2          (top face lands on y = 0)
//   walls       centre y =  wallHeight/2              (foot on the floor's top)
//   side walls  centre x = +/- halfWidth, depth = 2*halfDepth
//   end walls   centre z = +/- halfDepth, width = 2*halfWidth
//   front pair  width    =  halfWidth - apertureHalfWidth, centred on the
//                           remaining span either side of the opening
//   lintel      spans the aperture and fills wallHeight - apertureHeight above it
inline ZM_PlayerHomeBlockout ZM_GetPlayerHomeBlock(ZM_PLAYERHOME_BLOCK eBlock)
{
	constexpr float fFULL_WIDTH = fZM_PLAYERHOME_HALF_WIDTH * 2.0f;
	constexpr float fFULL_DEPTH = fZM_PLAYERHOME_HALF_DEPTH * 2.0f;
	constexpr float fWALL_CENTRE_Y = fZM_PLAYERHOME_WALL_HEIGHT * 0.5f;
	constexpr float fFRONT_PANEL_WIDTH =
		fZM_PLAYERHOME_HALF_WIDTH - fZM_PLAYERHOME_APERTURE_HALF_WIDTH;
	constexpr float fFRONT_PANEL_X =
		fZM_PLAYERHOME_APERTURE_HALF_WIDTH + fFRONT_PANEL_WIDTH * 0.5f;
	constexpr float fLINTEL_HEIGHT =
		fZM_PLAYERHOME_WALL_HEIGHT - fZM_PLAYERHOME_APERTURE_HEIGHT;
	constexpr float fLINTEL_CENTRE_Y =
		fZM_PLAYERHOME_APERTURE_HEIGHT + fLINTEL_HEIGHT * 0.5f;

	switch (eBlock)
	{
	case ZM_PLAYERHOME_BLOCK_FLOOR:
		return {
			Zenith_Maths::Vector3(
				0.0f, -fZM_PLAYERHOME_FLOOR_THICKNESS * 0.5f, 0.0f),
			Zenith_Maths::Vector3(
				fFULL_WIDTH, fZM_PLAYERHOME_FLOOR_THICKNESS, fFULL_DEPTH) };

	case ZM_PLAYERHOME_BLOCK_BACK_WALL:
		return {
			Zenith_Maths::Vector3(
				0.0f, fWALL_CENTRE_Y, -fZM_PLAYERHOME_HALF_DEPTH),
			Zenith_Maths::Vector3(
				fFULL_WIDTH, fZM_PLAYERHOME_WALL_HEIGHT,
				fZM_PLAYERHOME_WALL_THICKNESS) };

	case ZM_PLAYERHOME_BLOCK_LEFT_WALL:
		return {
			Zenith_Maths::Vector3(
				-fZM_PLAYERHOME_HALF_WIDTH, fWALL_CENTRE_Y, 0.0f),
			Zenith_Maths::Vector3(
				fZM_PLAYERHOME_WALL_THICKNESS, fZM_PLAYERHOME_WALL_HEIGHT,
				fFULL_DEPTH) };

	case ZM_PLAYERHOME_BLOCK_RIGHT_WALL:
		return {
			Zenith_Maths::Vector3(
				fZM_PLAYERHOME_HALF_WIDTH, fWALL_CENTRE_Y, 0.0f),
			Zenith_Maths::Vector3(
				fZM_PLAYERHOME_WALL_THICKNESS, fZM_PLAYERHOME_WALL_HEIGHT,
				fFULL_DEPTH) };

	case ZM_PLAYERHOME_BLOCK_FRONT_LEFT:
		return {
			Zenith_Maths::Vector3(
				-fFRONT_PANEL_X, fWALL_CENTRE_Y, fZM_PLAYERHOME_HALF_DEPTH),
			Zenith_Maths::Vector3(
				fFRONT_PANEL_WIDTH, fZM_PLAYERHOME_WALL_HEIGHT,
				fZM_PLAYERHOME_WALL_THICKNESS) };

	case ZM_PLAYERHOME_BLOCK_FRONT_RIGHT:
		return {
			Zenith_Maths::Vector3(
				fFRONT_PANEL_X, fWALL_CENTRE_Y, fZM_PLAYERHOME_HALF_DEPTH),
			Zenith_Maths::Vector3(
				fFRONT_PANEL_WIDTH, fZM_PLAYERHOME_WALL_HEIGHT,
				fZM_PLAYERHOME_WALL_THICKNESS) };

	case ZM_PLAYERHOME_BLOCK_LINTEL:
		return {
			Zenith_Maths::Vector3(
				0.0f, fLINTEL_CENTRE_Y, fZM_PLAYERHOME_HALF_DEPTH),
			Zenith_Maths::Vector3(
				fZM_PLAYERHOME_APERTURE_HALF_WIDTH * 2.0f, fLINTEL_HEIGHT,
				fZM_PLAYERHOME_WALL_THICKNESS) };

	default: break;
	}
	return { Zenith_Maths::Vector3(0.0f), Zenith_Maths::Vector3(0.0f) };
}

// ---- The tint membership predicate ------------------------------------------
//
// ZM-D-176. TRUE for exactly the seven names ZM_GetPlayerHomeBlockName offers,
// matched EXACTLY -- the same inventory the authoring loop walks, one spelling,
// so a rename moves both sides together.
//
// ★ EXACT, NEVER A PREFIX. A "starts with PlayerHome" test would also catch
// PlayerHomeCamera and PlayerHomeExitTrigger (which carry no ZM_GreyboxVisual
// today, so it would be a latent trap rather than a visible bug) and could not
// be cross-checked against ProfLab's names -- which is precisely the clause
// PlayerHome_BlockNamesAreTheTintedInventory runs.
//
// TOTAL: nullptr and "" are false, and so is the degenerate out-of-range name.
inline bool ZM_IsPlayerHomeBlockName(const char* szEntityName)
{
	if (szEntityName == nullptr || szEntityName[0] == '\0')
	{
		return false;
	}
	for (u_int uBlock = 0u; uBlock < (u_int)ZM_PLAYERHOME_BLOCK_COUNT; ++uBlock)
	{
		const char* szBlockName =
			ZM_GetPlayerHomeBlockName((ZM_PLAYERHOME_BLOCK)uBlock);
		if (std::strcmp(szEntityName, szBlockName) == 0)
		{
			return true;
		}
	}
	return false;
}
