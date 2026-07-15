#pragma once

// ============================================================================
// ZM_HumanAppearance -- internal SC3 appearance seam shared by the human mesh,
// albedo painter and pure headless tests. This is deliberately not part of the
// frozen public ZM_HumanGen contract.
// ============================================================================

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
