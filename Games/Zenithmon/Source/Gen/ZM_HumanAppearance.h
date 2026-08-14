#pragma once

// ============================================================================
// ZM_HumanAppearance -- internal SC3 appearance seam shared by the human mesh,
// albedo painter and pure headless tests. This is deliberately not part of the
// frozen public ZM_HumanGen contract.
// ============================================================================

#include "Maths/Zenith_Maths.h"                    // Zenith_Maths::Vector4 (the W4 palette's currency)
#include "Zenithmon/Source/Gen/ZM_HumanGen.h"

inline constexpr u_int uZM_HUMAN_HAIR_STYLE_COUNT = 6u;

// Normalized islands over the retained 256x256 human atlas. One clamp-to-edge
// dilation texel surrounds each painted core; the remaining gutter is untouched.
inline constexpr ZM_GenUVIsland xZM_HUMAN_UV_HEAD
	{ 0.005f, 0.005f, 0.325f, 0.420f };
inline constexpr ZM_GenUVIsland xZM_HUMAN_UV_TORSO
	{ 0.335f, 0.005f, 0.660f, 0.420f };
inline constexpr ZM_GenUVIsland xZM_HUMAN_UV_ARM_L
	{ 0.670f, 0.005f, 0.825f, 0.420f };
inline constexpr ZM_GenUVIsland xZM_HUMAN_UV_ARM_R
	{ 0.835f, 0.005f, 0.990f, 0.420f };
inline constexpr ZM_GenUVIsland xZM_HUMAN_UV_LEG_L
	{ 0.005f, 0.430f, 0.230f, 0.900f };
inline constexpr ZM_GenUVIsland xZM_HUMAN_UV_LEG_R
	{ 0.240f, 0.430f, 0.465f, 0.900f };
inline constexpr ZM_GenUVIsland xZM_HUMAN_UV_HAIR
	{ 0.475f, 0.430f, 0.700f, 0.900f };
inline constexpr ZM_GenUVIsland xZM_HUMAN_UV_ATTACHMENT
	{ 0.710f, 0.430f, 0.990f, 0.900f };

// Pure per-recipe appearance outputs. Mesh appenders add geometry only: they do
// not reset, add bones, consume RNG or run the final tangent/weight passes.
ZM_GenImage ZM_BuildHumanAlbedo(const ZM_HumanRecipe& xRecipe);
void ZM_AppendHumanHair(const ZM_HumanRecipe& xRecipe, ZM_GenMesh& xMesh);
void ZM_AppendHumanAttachment(const ZM_HumanRecipe& xRecipe, ZM_GenMesh& xMesh);

// Frozen append order for the complete human mesh: hair, then attachment.
void ZM_AppendHumanAppearanceMesh(const ZM_HumanRecipe& xRecipe, ZM_GenMesh& xMesh);

// ============================================================================
// Known-limit W4 -- THE GREYBOX APPEARANCE PALETTE.
//
// One flat colour per ZM_HUMAN_ID, derived from the SAME outfit/hair tables the
// full SC3 albedo painter above already uses, so a blockout body reads as a
// crude preview of the human that will eventually replace it rather than as an
// unrelated second opinion. It is deliberately a SINGLE colour: its consumer is
// ZM_GreyboxVisual's one-material unit cube (Zenithmon.cpp), which has no UVs
// worth painting.
//
// NOTHING HERE IS SERIALIZED. The colour is re-derived at runtime from the
// entity's already-serialized ZM_NpcData row, exactly the way SC8's trainer id
// is -- so ZM_GreyboxVisual keeps writing its single version u_int and the
// committed .zscen bytes cannot move.
// ============================================================================

// The SHIPPED blockout grey, spelled ONCE. This is what every non-NPC greybox
// entity (walls, floors, doors, props) wears and must keep wearing byte for
// byte, and it doubles as the palette's inert answer for an id it cannot serve.
//
// ★ ONE CARVE-OUT, ZM-D-176: the SEVEN PlayerHome shell blocks named by
// Source/World/ZM_PlayerHomePlacement.h wear ZM_GetPlayerHomeInteriorTintColour()
// instead, so the player's bedroom stops reading as the same greybox room as
// ProfLab. That is an ADDED branch in ZM_GreyboxVisual::ResolveBlockoutColour
// keyed to those seven names -- NOT an edit to the three values below, which
// still paint ProfLab's seven blocks, Dawnmere's four, and every other prop.
// ★ THESE THREE VALUES MUST NOT MOVE. Live boot units measure every palette
// entry's separation from them (ZM_Tests_HumanGen.cpp, ZM_Tests_NpcData.cpp).
// This note used to add "and ZM_HUMAN_PROF_ASTER already sits only 0.0677 away"
// as the sharpest illustration; that gap is CLOSED (his hair moved GREY -> WHITE
// and he now sits 0.21547 away -- see ZM_HumanData.cpp for why WHITE and not
// another slot). The MUST-NOT-MOVE rule is unaffected and does not weaken: these
// three floats are additionally pixel-asserted by ZM_AutoTests_InteriorTint and
// ZM_AutoTests_PlayerHomeTintPixels, so moving them to buy a future row headroom
// would red those instead. Move the HUMAN, never the grey.
inline constexpr float fZM_GREYBOX_FALLBACK_R = 0.52f;
inline constexpr float fZM_GREYBOX_FALLBACK_G = 0.55f;
inline constexpr float fZM_GREYBOX_FALLBACK_B = 0.60f;

// The minimum RGB separation two authored appearances owe each other, and that
// each owes the fallback grey. Spelled HERE rather than in the tests so the
// promise lives with the palette: a future roster row that lands on top of an
// existing one reds a boot unit instead of silently shipping two identical NPCs.
// The tightest shipped pair sits at ~0.20, so this carries real headroom.
inline constexpr float fZM_HUMAN_PALETTE_MIN_SEPARATION = 0.15f;

// The inert row, in the ZM_GetTrainerData / ZM_GetHumanName house style: an id
// the palette cannot serve gets a DEFINED answer, never an assert and never a
// read past the roster.
Zenith_Maths::Vector4 ZM_GetHumanPaletteFallbackColour();

// TOTAL by contract. Every ZM_HUMAN_ID below ZM_HUMAN_COUNT yields a finite
// colour whose RGB all sit in [0,1]; ZM_HUMAN_NONE (which aliases
// ZM_HUMAN_COUNT) and every garbage value yield ZM_GetHumanPaletteFallbackColour().
//
// IT MUST NEVER Zenith_Assert. Zenith_Assert calls Zenith_DebugBreak() in EVERY
// configuration, and this runs from ZM_GreyboxVisual::OnStart -- which starts at
// serialization order 107, BEFORE ZM_Interactable (113) has clamped a stale
// serialized row id. A defensive assert here would turn a mis-authored scene into
// a process kill rather than a grey cube.
Zenith_Maths::Vector4 ZM_GetHumanPaletteColour(ZM_HUMAN_ID eId);

// RGB Euclidean distance between two palette colours (alpha is ignored: it is
// always 1 and carries no appearance information). FAILS CLOSED -- a non-finite
// operand yields 0, so a "separation >= margin" clause can never be satisfied by
// garbage.
float ZM_HumanPaletteSeparation(
	const Zenith_Maths::Vector4& xA, const Zenith_Maths::Vector4& xB);
