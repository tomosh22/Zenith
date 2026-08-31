#pragma once

#include "Maths/Zenith_Maths.h"

// ============================================================================
// ZM_PropFit -- the transform that makes a prop MODEL the size the prop ROSTER
// says it is, whoever authored the model.
//
// PURE. No ECS, no scene, no I/O, no g_xEngine, no ZENITH_TOOLS guard --
// arithmetic over an AABB and three roster numbers, so the boot units can drive
// it headless and the tools authoring can call it while building a step list.
// HEADER-ONLY.
//
// ★★ WHY THIS EXISTS. Every generated prop is emitted at its roster size and
// grounded at y = 0, so the scene authoring could hand each one an identity
// transform and be right. A HAND-MADE model has neither property and cannot be
// made to: it arrives at whatever scale and origin its authoring tool chose --
// Zenithmon's first one is a bed that measures 1.00 x 0.38 x 0.72 m against a
// roster row of 2.0 x 1.2 x 0.7, centred on its own origin rather than standing
// on it. Authored at identity it is a half-size bed sunk to its mattress in the
// floor.
//
// The fix belongs HERE and not in the asset, because the asset is the thing that
// is allowed to change. Someone re-exports the bed at a different scale and
// nothing in the game needs editing; the numbers are read back off the mesh.
//
// ★ IT IS APPLIED TO EVERY PROP, GENERATED OR NOT, and there is no "is this
// model imported?" branch anywhere. Such a branch would have to know where each
// model came from, which is exactly the coupling this file exists to avoid: the
// whole point is that the authoring measures whatever is on disk.
//
// ★★ AND FOR A GENERATED PROP THAT IS NOT QUITE THE IDENTITY -- MEASURED, NOT
// ASSUMED. An earlier draft of this comment claimed a generated prop "measures
// exactly its roster size, so the scale comes out 1". It does not.
// ZM_PropGen jitters its box composition by a documented +/-4% (the sweep in
// ZM_Tests_PropGen prints `size factor [0.96012, 1.04000]`), so the shipped
// models measure e.g. Shelf 1.951 m against a 2.0 m roster row -- a scale of
// 1.0251, not 1. The observed set is 0.9912 to 1.0352.
//
// That is a real change to those props and it is the right one: the roster size
// is what the corridor clauses, the placement anchors and the colliders are all
// reasoning about, so a prop that is 2.5% under it was quietly wrong. The
// generator's variety is in the SHAPE -- which box goes where, in what
// proportion -- and a uniform scale preserves every bit of it. Only the overall
// size moves, onto the number the roster already claimed.
//
// The ground lift IS the identity for them: they are composed from y = 0 up, so
// min.y is 0 and the lift is 0.
//
// ★ THE SCALE IS UNIFORM, AND THAT IS A CHOICE WITH A COST. Matching all three
// roster numbers exactly needs a per-axis scale, and a per-axis scale on a
// photographed-then-reconstructed mesh visibly shears it -- a mattress stops
// being a mattress. So the LONGEST axis is matched and the other two follow the
// model's own proportions: the footprint the roster describes is honoured on the
// axis that matters and approximated on the others. A model whose proportions
// are wildly wrong for its roster row is an ASSET problem, and one this rule
// leaves visible rather than hiding under a squash.
// ============================================================================

// The transform to author for one prop, in the scene's own units.
struct ZM_PropFit
{
	// Uniform scale for all three axes. 1.0 when the model already measures its
	// roster size, or when the inputs are degenerate.
	float m_fScale = 1.0f;

	// World Y for the entity so the scaled model STANDS on the floor plane. A
	// model already authored with its base at the origin gets 0.
	float m_fGroundY = 0.0f;
};

// The transform that makes a model whose local AABB is [xMeshMin, xMeshMax]
// stand on y = 0 at its roster size.
//
// fRosterWidth / fRosterDepth / fRosterHeight are the roster row's three numbers
// in ZM_PropData's own order (X extent, Z extent, Y extent) -- read them from
// ZM_GetPropData rather than re-spelling them.
//
// TOTAL: a degenerate mesh (zero or non-finite extent on every axis) or a
// degenerate roster row answers the identity transform, which authors the model
// exactly as it would have been authored before this function existed. Refusing
// would mean an entity with no transform at all.
inline ZM_PropFit ZM_ComputePropFit(
	const Zenith_Maths::Vector3& xMeshMin,
	const Zenith_Maths::Vector3& xMeshMax,
	float fRosterWidth,
	float fRosterDepth,
	float fRosterHeight)
{
	ZM_PropFit xFit;

	const float fMeshX = xMeshMax.x - xMeshMin.x;
	const float fMeshY = xMeshMax.y - xMeshMin.y;
	const float fMeshZ = xMeshMax.z - xMeshMin.z;

	// A NaN fails every comparison, so this rejects non-finite extents too.
	const bool bMeshUsable =
		(fMeshX > 0.0f && fMeshX < 1.0e6f) ||
		(fMeshY > 0.0f && fMeshY < 1.0e6f) ||
		(fMeshZ > 0.0f && fMeshZ < 1.0e6f);
	const bool bRosterUsable =
		(fRosterWidth > 0.0f) || (fRosterDepth > 0.0f) || (fRosterHeight > 0.0f);
	if (!bMeshUsable || !bRosterUsable)
	{
		return xFit;
	}

	float fMeshLongest = fMeshX;
	fMeshLongest = (fMeshY > fMeshLongest) ? fMeshY : fMeshLongest;
	fMeshLongest = (fMeshZ > fMeshLongest) ? fMeshZ : fMeshLongest;

	float fRosterLongest = fRosterWidth;
	fRosterLongest = (fRosterDepth > fRosterLongest) ? fRosterDepth : fRosterLongest;
	fRosterLongest = (fRosterHeight > fRosterLongest) ? fRosterHeight : fRosterLongest;

	if (!(fMeshLongest > 0.0f) || !(fRosterLongest > 0.0f))
	{
		return xFit;
	}

	// ONE divide, no library call. glm::angleAxis-style transcendentals differ by
	// 1-2 ULP between MSVC Debug and Release codegen and that difference reaches
	// committed scene bytes (ZM-D-183); an IEEE divide is correctly rounded and
	// identical in every configuration, which is what lets this value be authored.
	xFit.m_fScale = fRosterLongest / fMeshLongest;

	// Stand it on the floor. A model grounded at its own origin has min.y == 0 and
	// gets 0 back; one centred on its origin gets lifted by half its scaled height.
	xFit.m_fGroundY = -xMeshMin.y * xFit.m_fScale;

	return xFit;
}

// The model's size after ZM_ComputePropFit, for diagnostics and for tests that
// want to state the outcome rather than recompute it.
inline Zenith_Maths::Vector3 ZM_FittedPropSize(
	const Zenith_Maths::Vector3& xMeshMin,
	const Zenith_Maths::Vector3& xMeshMax,
	const ZM_PropFit& xFit)
{
	return (xMeshMax - xMeshMin) * xFit.m_fScale;
}
