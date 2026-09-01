#include "Zenith.h"

// ============================================================================
// ZM_Tests_PropFit -- the boot units for ZM_ComputePropFit, the arithmetic that
// makes a prop MODEL the size the prop ROSTER says it is.
//
// PURE. No scene, no entity, no assets, no graphics, no g_xEngine -- an AABB and
// three roster numbers in, a scale and a ground height out. Runs in EVERY
// configuration, which is the point: the only other thing that exercises this
// path is ZM_ImportedPropShowcase_Test, and that is graphics-required and therefore SKIPS
// (i.e. passes) in the headless gate.
//
// ★★ THIS IS THE HALF THAT CANNOT ROT. Zenithmon's first imported asset arrives
// at 1.00 x 0.38 x 0.72 m against a 2.0 x 1.2 x 0.7 roster row, centred on its
// own origin rather than standing on it. If the fit silently became the identity
// -- a refactor, an early return, a sign flip on the lift -- the bed would be
// half size and sunk to its mattress in the floor, every unit would stay green,
// and the only thing that would notice is a human looking at a screenshot.
//
// ★ WHAT THESE UNITS CANNOT DO. They prove the arithmetic. They cannot prove the
// scene was AUTHORED with it (that is ZM_ImportedPropShowcase_Test's transform-vs-fit
// clause, which re-derives the fit from the live mesh) and they cannot prove the
// bed looks right. Do not read this file's greenness as "the bed is the right
// size in the game".
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Maths/Zenith_Maths.h"
#include "Zenithmon/Source/Data/ZM_PropData.h"
#include "Zenithmon/Source/World/ZM_PropFit.h"

#include <cmath>

namespace
{
	// A float round trip through a scene file, not a physical tolerance.
	constexpr float fPF_EPSILON = 1.0e-5f;

	Zenith_Maths::Vector3 PFVec(float x, float y, float z)
	{
		return Zenith_Maths::Vector3(x, y, z);
	}
}

// A model already at its roster size and grounded at its own origin -- the shape
// every PROCEDURAL prop would have if the generator did not jitter -- must come
// back as the identity, because that is what the authoring emitted before the
// fit existed and what it must keep emitting for such a model.
ZENITH_TEST(ZM_PropFit, ExactRosterModelIsTheIdentity)
{
	const ZM_PropFit xFit = ZM_ComputePropFit(
		PFVec(-1.0f, 0.0f, -0.6f), PFVec(1.0f, 0.7f, 0.6f),
		2.0f, 1.2f, 0.7f);

	ZENITH_ASSERT_LT(std::fabs(xFit.m_fScale - 1.0f), fPF_EPSILON,
		"a model already at its roster size must not be rescaled (got %.6f)",
		(double)xFit.m_fScale);
	ZENITH_ASSERT_LT(std::fabs(xFit.m_fGroundY), fPF_EPSILON,
		"a model grounded at its own origin must not be lifted (got %.6f)",
		(double)xFit.m_fGroundY);
}

// ★★ EVERY SHIPPED IMPORT, WITH ITS REAL MEASUREMENTS. The bounds are the
// numbers each .glb's own POSITION accessor DECLARES -- which the importer
// reproduces byte-for-byte, so they are checkable against the file rather than
// copied out of a log -- against that prop's roster row, READ from the table.
//
// ★ IT IS A TABLE BECAUSE THE SECOND IMPORT BROKE THE FIRST ONE'S SHAPE. Written
// for the bed alone, this asserted "the fitted WIDTH equals the roster width",
// which is true of the bed and false of the table: the table's longest MODEL axis
// is Z while its longest ROSTER number is the 1.4 m width, so it fits on Z. The
// invariant that actually holds -- and that the four shipped imports now cover
// between them -- is LONGEST TO LONGEST, so that is what is asserted.
//
// ★★ AND THEY LAND ON ALL THREE AXES, WHICH IS WHY THE SET IS WORTH KEEPING
// RATHER THAN TRIMMING TO ONE. The bed fits on X, the table on Z, and the chair
// and shelf on Y. Any implementation that hard-codes an axis passes for one of
// them and fails the rest, and a single-asset version of this test would have let
// most of them through.
//
// ★ THE SPREAD OF SCALES MATTERS AS MUCH AS THE SPREAD OF AXES. The chair arrives
// at 1.0020 and the shelf at 2.0039, so the table also pins that a near-identity
// and a doubling come out of the same branch-free expression. An implementation
// that special-cased "already about right" would pass the chair and fail the shelf.
ZENITH_TEST(ZM_PropFit, ImportedAssetsAreScaledAndStoodOnTheFloor)
{
	struct ImportedAsset
	{
		ZM_PROP_ID m_eId;
		Zenith_Maths::Vector3 m_xMin;
		Zenith_Maths::Vector3 m_xMax;
	};

	const ImportedAsset axAssets[] =
	{
		{ ZM_PROP_BED,
			PFVec(-0.499755859f, -0.190185547f, -0.361572266f),
			PFVec( 0.499755859f,  0.190185547f,  0.361572266f) },
		{ ZM_PROP_TABLE,
			PFVec(-0.319091797f, -0.268798828f, -0.499755859f),
			PFVec( 0.319091797f,  0.268798828f,  0.499755859f) },
		{ ZM_PROP_CHAIR,
			PFVec(-0.307617188f, -0.499023438f, -0.268554688f),
			PFVec( 0.307617188f,  0.499023438f,  0.268554688f) },
		// The shelf fits on Y like the chair, but it is the FAR-FROM-IDENTITY case:
		// scale 2.0039 against the chair's 1.0020, from the same rule and the same
		// code path. It is also the delivery whose PROPORTIONS diverge most from its
		// roster row -- 1.2 x 0.4 x 2.0 asks for a footprint of 0.6 x 0.2 relative to
		// height and the model answers 0.26 x 0.49 -- so it comes out 0.528 x 0.975 in
		// plan rather than 1.2 x 0.4. That is left VISIBLE, per ZM_PropFit.h's own
		// ruling: a uniform scale honours the longest axis and reports the rest, and a
		// per-axis squash onto three roster numbers would shear the books modelled
		// into it. Nothing below asserts the short axes for exactly that reason.
		{ ZM_PROP_SHELF,
			PFVec(-0.131835938f, -0.499023438f, -0.243164063f),
			PFVec( 0.131835938f,  0.499023438f,  0.243164063f) },
		// ★ THE COUNTER IS THE SHELF'S MIRROR, and that is why it is worth a row
		// rather than being "another Z fit like the table". Both are far from the
		// identity -- 2.2011 against 2.0039 -- but they miss their roster
		// proportions in OPPOSITE directions. Fitted to its 2.0 m height the shelf
		// comes out 0.53 x 0.97 in plan where its row asks 1.2 x 0.4, so one short
		// axis lands well UNDER; fitted to its 2.2 m length the counter comes out
		// 0.951 deep and 1.338 tall where its row asks 0.7 and 1.0, so BOTH short
		// axes land OVER. An implementation that clamped a fitted axis to its
		// roster number, in either direction, would pass one of these and fail the
		// other. Nothing below asserts a short axis, for exactly that reason.
		{ ZM_PROP_COUNTER,
			PFVec(-0.216064453f, -0.303955078f, -0.499755859f),
			PFVec( 0.216064453f,  0.303955078f,  0.499755859f) },
		// ★★ THE BARREL IS THE CASE WHERE A SCALE OF ~1 MEANS NOTHING. It fits on
		// Y at 1.0005 -- nearer the identity than the chair's 1.0020, the closest
		// of the six -- and is still 27% OVER its roster row in plan: 0.892 x 0.893
		// against 0.7 x 0.7. "The scale came out about 1, so it was delivered to
		// size" is therefore false, and it is the reading a single-number summary
		// invites. The longest axis is the only one the fit promises; the other two
		// report whatever the model's proportions say, at any scale.
		{ ZM_PROP_BARREL,
			PFVec(-0.446044922f, -0.499755859f, -0.446533203f),
			PFVec( 0.446044922f,  0.499755859f,  0.446533203f) },
	};

	for (const ImportedAsset& xAsset : axAssets)
	{
		const ZM_PropData& xData = ZM_GetPropData(xAsset.m_eId);
		const ZM_PropFit xFit = ZM_ComputePropFit(
			xAsset.m_xMin, xAsset.m_xMax,
			xData.m_fWidth, xData.m_fDepth, xData.m_fHeight);
		const Zenith_Maths::Vector3 xFitted =
			ZM_FittedPropSize(xAsset.m_xMin, xAsset.m_xMax, xFit);

		// LONGEST TO LONGEST, whichever axes those are.
		float fFittedLongest = xFitted.x;
		fFittedLongest = (xFitted.y > fFittedLongest) ? xFitted.y : fFittedLongest;
		fFittedLongest = (xFitted.z > fFittedLongest) ? xFitted.z : fFittedLongest;

		float fRosterLongest = xData.m_fWidth;
		fRosterLongest = (xData.m_fDepth > fRosterLongest) ? xData.m_fDepth : fRosterLongest;
		fRosterLongest = (xData.m_fHeight > fRosterLongest) ? xData.m_fHeight : fRosterLongest;

		ZENITH_ASSERT_LT(std::fabs(fFittedLongest - fRosterLongest), 1.0e-3f,
			"'%s': the model's longest axis must end up at its roster's longest "
			"number %.3f (got %.4f)", xData.m_szName,
			(double)fRosterLongest, (double)fFittedLongest);

		// ★ AND IT STANDS ON THE FLOOR. Both meshes are origin-CENTRED, so the lift
		// is half the scaled height -- authored at 0 each would be buried to its
		// midline, which is the failure this clause exists for.
		ZENITH_ASSERT_GT(xFit.m_fGroundY, 0.0f,
			"'%s': an origin-centred model must be LIFTED, not left at y = 0",
			xData.m_szName);
		ZENITH_ASSERT_LT(std::fabs(xFit.m_fGroundY + xAsset.m_xMin.y * xFit.m_fScale),
			fPF_EPSILON,
			"'%s': scaled min.y + lift must be exactly 0", xData.m_szName);

		// The proportions survive: this is a UNIFORM scale, not a squash onto three
		// roster numbers. Every axis ratio in equals the same ratio out.
		const Zenith_Maths::Vector3 xSource = xAsset.m_xMax - xAsset.m_xMin;
		ZENITH_ASSERT_LT(
			std::fabs((xSource.z / xSource.x) - (xFitted.z / xFitted.x)), 1.0e-4f,
			"'%s': a uniform scale must preserve depth/width", xData.m_szName);
		ZENITH_ASSERT_LT(
			std::fabs((xSource.y / xSource.x) - (xFitted.y / xFitted.x)), 1.0e-4f,
			"'%s': a uniform scale must preserve height/width", xData.m_szName);
	}
}

// A model whose longest axis is its HEIGHT (a shelf, a lamp post) must fit on
// that axis, not on width. The rule is "longest to longest", and a version that
// always matched X would silently leave every tall prop wrong.
ZENITH_TEST(ZM_PropFit, LongestAxisDrivesTheScaleWhicheverAxisItIs)
{
	// A 0.5 x 1.0 x 0.2 model against the Shelf row (1.2 wide, 0.4 deep, 2.0 tall).
	const ZM_PropData& xShelf = ZM_GetPropData(ZM_PROP_SHELF);
	const ZM_PropFit xFit = ZM_ComputePropFit(
		PFVec(-0.25f, 0.0f, -0.1f), PFVec(0.25f, 1.0f, 0.1f),
		xShelf.m_fWidth, xShelf.m_fDepth, xShelf.m_fHeight);

	// Longest model axis is Y = 1.0; longest roster number is the 2.0 height.
	ZENITH_ASSERT_LT(std::fabs(xFit.m_fScale - 2.0f), fPF_EPSILON,
		"height must drive the scale for a tall prop (got %.6f)", (double)xFit.m_fScale);
	ZENITH_ASSERT_LT(std::fabs(xFit.m_fGroundY), fPF_EPSILON,
		"a model already grounded stays grounded whatever the scale");
}

// A model sitting ENTIRELY above its origin is pushed DOWN onto the floor, not
// only ever lifted. A one-sided clamp would leave a floating prop.
ZENITH_TEST(ZM_PropFit, ModelAboveItsOriginIsPushedDown)
{
	const ZM_PropFit xFit = ZM_ComputePropFit(
		PFVec(-1.0f, 0.5f, -0.6f), PFVec(1.0f, 1.2f, 0.6f),
		2.0f, 1.2f, 0.7f);

	ZENITH_ASSERT_LT(std::fabs(xFit.m_fScale - 1.0f), fPF_EPSILON, "scale is unaffected by the offset");
	ZENITH_ASSERT_LT(xFit.m_fGroundY, 0.0f,
		"a model whose lowest point is above its origin must be pushed DOWN (got %.6f)",
		(double)xFit.m_fGroundY);
	ZENITH_ASSERT_LT(std::fabs(xFit.m_fGroundY + 0.5f), fPF_EPSILON,
		"the push-down is exactly -min.y * scale");
}

// ★ TOTAL ON GARBAGE. A degenerate mesh or roster row answers the IDENTITY, so a
// prop with no bake is authored exactly as it was before this function existed.
// Refusing would mean an entity with no transform at all.
ZENITH_TEST(ZM_PropFit, DegenerateInputsAnswerTheIdentity)
{
	const ZM_PropFit xEmptyMesh = ZM_ComputePropFit(
		PFVec(0.0f, 0.0f, 0.0f), PFVec(0.0f, 0.0f, 0.0f), 2.0f, 1.2f, 0.7f);
	ZENITH_ASSERT_LT(std::fabs(xEmptyMesh.m_fScale - 1.0f), fPF_EPSILON, "zero-extent mesh -> scale 1");
	ZENITH_ASSERT_LT(std::fabs(xEmptyMesh.m_fGroundY), fPF_EPSILON, "zero-extent mesh -> no lift");

	const ZM_PropFit xEmptyRoster = ZM_ComputePropFit(
		PFVec(-1.0f, 0.0f, -1.0f), PFVec(1.0f, 1.0f, 1.0f), 0.0f, 0.0f, 0.0f);
	ZENITH_ASSERT_LT(std::fabs(xEmptyRoster.m_fScale - 1.0f), fPF_EPSILON, "zero roster row -> scale 1");

	// An inverted AABB -- what an uninitialised Zenith_AABB looks like -- must not
	// produce a negative or absurd scale.
	const ZM_PropFit xInverted = ZM_ComputePropFit(
		PFVec(1.0f, 1.0f, 1.0f), PFVec(-1.0f, -1.0f, -1.0f), 2.0f, 1.2f, 0.7f);
	ZENITH_ASSERT_LT(std::fabs(xInverted.m_fScale - 1.0f), fPF_EPSILON,
		"an inverted AABB -> scale 1, never a negative scale (got %.6f)",
		(double)xInverted.m_fScale);
}

// ★ EVERY ROSTER ROW SURVIVES ITS OWN NOMINAL MODEL. Sweeping the table rather
// than testing one prop is what catches a row added later with a zero or
// negative dimension: such a row would author its prop at some arbitrary scale
// and nothing else in the game would notice.
ZENITH_TEST(ZM_PropFit, EveryRosterRowFitsItsOwnNominalBox)
{
	for (u_int u = 0u; u < (u_int)ZM_PROP_COUNT; ++u)
	{
		const ZM_PROP_ID eId = (ZM_PROP_ID)u;
		const ZM_PropData& xData = ZM_GetPropData(eId);

		ZENITH_ASSERT_GT(xData.m_fWidth, 0.0f, "prop '%s' has a non-positive width", xData.m_szName);
		ZENITH_ASSERT_GT(xData.m_fDepth, 0.0f, "prop '%s' has a non-positive depth", xData.m_szName);
		ZENITH_ASSERT_GT(xData.m_fHeight, 0.0f, "prop '%s' has a non-positive height", xData.m_szName);

		// The nominal box: exactly the roster size, grounded. Must be the identity.
		const ZM_PropFit xFit = ZM_ComputePropFit(
			PFVec(-xData.m_fWidth * 0.5f, 0.0f, -xData.m_fDepth * 0.5f),
			PFVec(xData.m_fWidth * 0.5f, xData.m_fHeight, xData.m_fDepth * 0.5f),
			xData.m_fWidth, xData.m_fDepth, xData.m_fHeight);

		ZENITH_ASSERT_LT(std::fabs(xFit.m_fScale - 1.0f), fPF_EPSILON,
			"prop '%s' nominal box must fit at scale 1 (got %.6f)",
			xData.m_szName, (double)xFit.m_fScale);
		ZENITH_ASSERT_LT(std::fabs(xFit.m_fGroundY), fPF_EPSILON,
			"prop '%s' nominal box must need no lift", xData.m_szName);
	}
}
