#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/UnifiedMesh/Flux_Skinning.h"

// ============================================================================
// Flux compute-skinning Stage-5 unit tests (unified GPU-driven opaque-mesh pipeline).
//
// Pure CPU mirror of Flux_UnifiedMesh_Skinning.slang: bone-weighted accumulation to
// OBJECT space (sentinel-terminated, palette-base-indexed, defensively bounds-guarded),
// the bind-pose cull-bounds inflation, and a golden record hash. No GPU, no renderer
// boot. Hosted in an already-linked TU (Flux_MaterialTable.cpp) so the static-init
// test registrations are not dead-stripped before the path is wired (Stage 5d).
// ============================================================================

namespace
{
	Flux_SkinInputVertex Skin_MakeVertex(const Zenith_Maths::Vector3& xPos,
		const Zenith_Maths::Vector3& xNormal, u_int uB0, u_int uB1, u_int uB2, u_int uB3,
		float fW0, float fW1, float fW2, float fW3)
	{
		Flux_SkinInputVertex xV;
		xV.m_xPosition   = xPos;
		xV.m_xUV         = Zenith_Maths::Vector2(0.25f, 0.75f);
		xV.m_xNormal     = xNormal;
		xV.m_xTangent    = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
		xV.m_xBitangent  = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		xV.m_xColor      = Zenith_Maths::Vector4(0.1f, 0.2f, 0.3f, 1.0f);
		xV.m_auBoneIDs[0] = uB0; xV.m_auBoneIDs[1] = uB1; xV.m_auBoneIDs[2] = uB2; xV.m_auBoneIDs[3] = uB3;
		xV.m_xBoneWeights = Zenith_Maths::Vector4(fW0, fW1, fW2, fW3);
		return xV;
	}

	Zenith_Maths::Matrix4 Skin_Translate(float fX, float fY, float fZ)
	{
		Zenith_Maths::Matrix4 xM(1.0f);
		xM[3] = Zenith_Maths::Vector4(fX, fY, fZ, 1.0f);
		return xM;
	}

	// +90° rotation about Z (column-major): x' = -y, y' = x.
	Zenith_Maths::Matrix4 Skin_RotateZ90()
	{
		Zenith_Maths::Matrix4 xM(1.0f);
		xM[0] = Zenith_Maths::Vector4(0.0f, 1.0f, 0.0f, 0.0f);
		xM[1] = Zenith_Maths::Vector4(-1.0f, 0.0f, 0.0f, 0.0f);
		xM[2] = Zenith_Maths::Vector4(0.0f, 0.0f, 1.0f, 0.0f);
		xM[3] = Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 1.0f);
		return xM;
	}
}

// ---- identity pose == bind pose --------------------------------------------

ZENITH_TEST(Skinning, IdentityPosePreservesBindPose)
{
	Zenith_Maths::Matrix4 axPalette[1] = { Zenith_Maths::Matrix4(1.0f) };
	Flux_SkinInputVertex xIn = Skin_MakeVertex(
		Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f),
		0u, uFLUX_SKIN_BONE_SENTINEL, uFLUX_SKIN_BONE_SENTINEL, uFLUX_SKIN_BONE_SENTINEL,
		1.0f, 0.0f, 0.0f, 0.0f);

	Flux_SkinOutputVertex xOut = Flux_SkinVertexCPU(xIn, axPalette, 0u, 1u);

	ZENITH_ASSERT_EQ_FLOAT(xOut.m_xPosition.x, 1.0f, 0.0001f, "identity pose leaves position X");
	ZENITH_ASSERT_EQ_FLOAT(xOut.m_xPosition.y, 2.0f, 0.0001f, "identity pose leaves position Y");
	ZENITH_ASSERT_EQ_FLOAT(xOut.m_xPosition.z, 3.0f, 0.0001f, "identity pose leaves position Z");
	ZENITH_ASSERT_EQ_FLOAT(xOut.m_xNormal.y, 1.0f, 0.0001f, "identity pose leaves the normal");
	// UV / colour are passthrough.
	ZENITH_ASSERT_EQ_FLOAT(xOut.m_xUV.x, 0.25f, 0.0001f, "UV passthrough X");
	ZENITH_ASSERT_EQ_FLOAT(xOut.m_xColor.z, 0.3f, 0.0001f, "colour passthrough B");
}

// ---- single-bone transform: translation moves pos, not normal --------------

ZENITH_TEST(Skinning, SingleBoneTranslationMovesPositionNotNormal)
{
	Zenith_Maths::Matrix4 axPalette[1] = { Skin_Translate(10.0f, 0.0f, 0.0f) };
	Flux_SkinInputVertex xIn = Skin_MakeVertex(
		Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f),
		0u, uFLUX_SKIN_BONE_SENTINEL, uFLUX_SKIN_BONE_SENTINEL, uFLUX_SKIN_BONE_SENTINEL,
		1.0f, 0.0f, 0.0f, 0.0f);

	Flux_SkinOutputVertex xOut = Flux_SkinVertexCPU(xIn, axPalette, 0u, 1u);

	ZENITH_ASSERT_EQ_FLOAT(xOut.m_xPosition.x, 11.0f, 0.0001f, "translation adds to position X");
	ZENITH_ASSERT_EQ_FLOAT(xOut.m_xPosition.y, 2.0f, 0.0001f, "translation leaves position Y");
	ZENITH_ASSERT_EQ_FLOAT(xOut.m_xNormal.y, 1.0f, 0.0001f, "translation (mat3) leaves the normal");
}

// ---- single-bone rotation: rotates normal + tangent ------------------------

ZENITH_TEST(Skinning, SingleBoneRotationRotatesNormalAndTangent)
{
	Zenith_Maths::Matrix4 axPalette[1] = { Skin_RotateZ90() };
	// Normal +X, tangent +X: a +90° Z rotation sends +X -> +Y.
	Flux_SkinInputVertex xIn = Skin_MakeVertex(
		Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f),
		0u, uFLUX_SKIN_BONE_SENTINEL, uFLUX_SKIN_BONE_SENTINEL, uFLUX_SKIN_BONE_SENTINEL,
		1.0f, 0.0f, 0.0f, 0.0f);

	Flux_SkinOutputVertex xOut = Flux_SkinVertexCPU(xIn, axPalette, 0u, 1u);

	ZENITH_ASSERT_EQ_FLOAT(xOut.m_xNormal.x, 0.0f, 0.0001f, "rotated normal X -> 0");
	ZENITH_ASSERT_EQ_FLOAT(xOut.m_xNormal.y, 1.0f, 0.0001f, "rotated normal +X -> +Y");
	ZENITH_ASSERT_EQ_FLOAT(xOut.m_xTangent.y, 1.0f, 0.0001f, "rotated tangent +X -> +Y");
}

// ---- two-bone 50/50 blend --------------------------------------------------

ZENITH_TEST(Skinning, TwoBoneBlendIsWeightedMidpoint)
{
	Zenith_Maths::Matrix4 axPalette[2] = { Zenith_Maths::Matrix4(1.0f), Skin_Translate(10.0f, 0.0f, 0.0f) };
	// Position at the origin, 50% identity + 50% translate(10,0,0) -> (5,0,0).
	Flux_SkinInputVertex xIn = Skin_MakeVertex(
		Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f),
		0u, 1u, uFLUX_SKIN_BONE_SENTINEL, uFLUX_SKIN_BONE_SENTINEL,
		0.5f, 0.5f, 0.0f, 0.0f);

	Flux_SkinOutputVertex xOut = Flux_SkinVertexCPU(xIn, axPalette, 0u, 2u);

	ZENITH_ASSERT_EQ_FLOAT(xOut.m_xPosition.x, 5.0f, 0.0001f, "50/50 blend -> midpoint X");
	ZENITH_ASSERT_EQ_FLOAT(xOut.m_xPosition.y, 0.0f, 0.0001f, "blend leaves Y");
}

// ---- sentinel terminates the influence list early --------------------------

ZENITH_TEST(Skinning, SentinelTerminatesInfluenceLoop)
{
	// Bone 0 (identity, weight 1), then a sentinel — the bone-1 translate at index 2 with a
	// non-zero weight must NOT be applied (the loop breaks at the sentinel).
	Zenith_Maths::Matrix4 axPalette[2] = { Zenith_Maths::Matrix4(1.0f), Skin_Translate(100.0f, 0.0f, 0.0f) };
	Flux_SkinInputVertex xIn = Skin_MakeVertex(
		Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f),
		0u, uFLUX_SKIN_BONE_SENTINEL, 1u, 1u,
		1.0f, 0.0f, 0.5f, 0.5f);

	Flux_SkinOutputVertex xOut = Flux_SkinVertexCPU(xIn, axPalette, 0u, 2u);

	ZENITH_ASSERT_EQ_FLOAT(xOut.m_xPosition.x, 1.0f, 0.0001f,
		"post-sentinel influences are ignored (X stays at the bone-0 result, no +100)");
}

// ---- palette base selects the instance's block -----------------------------

ZENITH_TEST(Skinning, PaletteBaseSelectsSkeletonBlock)
{
	// Two skeleton blocks: block 0 = identity, block 1 = translate(100,0,0). With base = 1,
	// bone id 0 indexes palette[1] -> the second skeleton's matrix.
	Zenith_Maths::Matrix4 axPalette[2] = { Zenith_Maths::Matrix4(1.0f), Skin_Translate(100.0f, 0.0f, 0.0f) };
	Flux_SkinInputVertex xIn = Skin_MakeVertex(
		Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f),
		0u, uFLUX_SKIN_BONE_SENTINEL, uFLUX_SKIN_BONE_SENTINEL, uFLUX_SKIN_BONE_SENTINEL,
		1.0f, 0.0f, 0.0f, 0.0f);

	Flux_SkinOutputVertex xOut = Flux_SkinVertexCPU(xIn, axPalette, /*base*/ 1u, /*count*/ 2u);

	ZENITH_ASSERT_EQ_FLOAT(xOut.m_xPosition.x, 100.0f, 0.0001f, "palette base selects the second skeleton block");
}

// ---- defensive out-of-range bounds guard -----------------------------------

ZENITH_TEST(Skinning, OutOfRangeInfluenceIsSkippedNotAppliedOrCrashing)
{
	// Bone 0 (identity, weight 1) then bone id 5 with palette count 1 (index 5 out of range):
	// the OOB influence is skipped, the valid one still applies (no read past the palette).
	Zenith_Maths::Matrix4 axPalette[1] = { Zenith_Maths::Matrix4(1.0f) };
	Flux_SkinInputVertex xIn = Skin_MakeVertex(
		Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f),
		0u, 5u, uFLUX_SKIN_BONE_SENTINEL, uFLUX_SKIN_BONE_SENTINEL,
		1.0f, 1.0f, 0.0f, 0.0f);

	Flux_SkinOutputVertex xOut = Flux_SkinVertexCPU(xIn, axPalette, 0u, /*count*/ 1u);

	ZENITH_ASSERT_EQ_FLOAT(xOut.m_xPosition.x, 2.0f, 0.0001f, "OOB influence skipped; valid influence applied");
}

// ---- raw-word skinning == typed skinning (pins the shader's byte offsets) ---

ZENITH_TEST(Skinning, RawWordSkinMatchesTypedSkin)
{
	// Flux_SkinVertexRaw reads/writes flat word arrays with MANUAL offsets (exactly what
	// the compute shader does to dodge the std430 vec3 trap). Feed it a buffer built by
	// memcpy from the typed struct (which uses the real C++ layout) and require the result
	// to equal the typed Flux_SkinVertexCPU output byte-for-byte — so any wrong word offset
	// in the raw path (and therefore the shader) fails here, headlessly.
	Zenith_Maths::Matrix4 axPalette[2] = { Zenith_Maths::Matrix4(1.0f), Skin_Translate(10.0f, -3.0f, 2.0f) };

	Flux_SkinInputVertex xIn = Skin_MakeVertex(
		Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f), Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f),
		0u, 1u, uFLUX_SKIN_BONE_SENTINEL, uFLUX_SKIN_BONE_SENTINEL,
		0.25f, 0.75f, 0.0f, 0.0f);

	// Build the raw input word buffer from the struct's exact bytes (104B == 26 words).
	static_assert(sizeof(Flux_SkinInputVertex) == uFLUX_SKIN_INPUT_WORDS * 4u, "input word count must match the struct size");
	static_assert(sizeof(Flux_SkinOutputVertex) == uFLUX_SKIN_OUTPUT_WORDS * 4u, "output word count must match the struct size");
	u_int auIn[uFLUX_SKIN_INPUT_WORDS];
	memcpy(auIn, &xIn, sizeof(xIn));

	u_int auOut[uFLUX_SKIN_OUTPUT_WORDS] = {};
	Flux_SkinVertexRaw(auIn, 0u, axPalette, 0u, 2u, auOut, 0u);

	// Reinterpret the raw output and compare to the typed path.
	Flux_SkinOutputVertex xRawOut;
	memcpy(&xRawOut, auOut, sizeof(xRawOut));
	const Flux_SkinOutputVertex xTypedOut = Flux_SkinVertexCPU(xIn, axPalette, 0u, 2u);

	ZENITH_ASSERT_EQ_FLOAT(xRawOut.m_xPosition.x, xTypedOut.m_xPosition.x, 0.0001f, "raw pos X == typed");
	ZENITH_ASSERT_EQ_FLOAT(xRawOut.m_xPosition.y, xTypedOut.m_xPosition.y, 0.0001f, "raw pos Y == typed");
	ZENITH_ASSERT_EQ_FLOAT(xRawOut.m_xPosition.z, xTypedOut.m_xPosition.z, 0.0001f, "raw pos Z == typed");
	ZENITH_ASSERT_EQ_FLOAT(xRawOut.m_xNormal.z,   xTypedOut.m_xNormal.z,   0.0001f, "raw normal Z == typed");
	ZENITH_ASSERT_EQ_FLOAT(xRawOut.m_xUV.x,       xTypedOut.m_xUV.x,       0.0001f, "raw uv X == typed (offset 3)");
	ZENITH_ASSERT_EQ_FLOAT(xRawOut.m_xColor.w,    xTypedOut.m_xColor.w,    0.0001f, "raw color W == typed (offset 17)");

	// Whole-struct byte equality: the strongest pin on every offset.
	ZENITH_ASSERT_EQ(memcmp(&xRawOut, &xTypedOut, sizeof(xRawOut)), 0, "raw word output is byte-identical to the typed output");
}

ZENITH_TEST(Skinning, RawWordSkinHonoursVertexIndexStrides)
{
	// Two input vertices, write to two output slots: prove the per-vertex word strides
	// (26 in / 18 out) address the right slices (vertex 1 must not clobber vertex 0).
	Zenith_Maths::Matrix4 axPalette[1] = { Skin_Translate(5.0f, 0.0f, 0.0f) };

	Flux_SkinInputVertex xIn0 = Skin_MakeVertex(
		Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f),
		0u, uFLUX_SKIN_BONE_SENTINEL, uFLUX_SKIN_BONE_SENTINEL, uFLUX_SKIN_BONE_SENTINEL, 1.0f, 0.0f, 0.0f, 0.0f);
	Flux_SkinInputVertex xIn1 = Skin_MakeVertex(
		Zenith_Maths::Vector3(100.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f),
		0u, uFLUX_SKIN_BONE_SENTINEL, uFLUX_SKIN_BONE_SENTINEL, uFLUX_SKIN_BONE_SENTINEL, 1.0f, 0.0f, 0.0f, 0.0f);

	u_int auIn[2u * uFLUX_SKIN_INPUT_WORDS];
	memcpy(auIn + 0u * uFLUX_SKIN_INPUT_WORDS, &xIn0, sizeof(xIn0));
	memcpy(auIn + 1u * uFLUX_SKIN_INPUT_WORDS, &xIn1, sizeof(xIn1));

	u_int auOut[2u * uFLUX_SKIN_OUTPUT_WORDS] = {};
	Flux_SkinVertexRaw(auIn, 0u, axPalette, 0u, 1u, auOut, 0u);
	Flux_SkinVertexRaw(auIn, 1u, axPalette, 0u, 1u, auOut, 1u);

	Flux_SkinOutputVertex xOut0, xOut1;
	memcpy(&xOut0, auOut + 0u * uFLUX_SKIN_OUTPUT_WORDS, sizeof(xOut0));
	memcpy(&xOut1, auOut + 1u * uFLUX_SKIN_OUTPUT_WORDS, sizeof(xOut1));
	ZENITH_ASSERT_EQ_FLOAT(xOut0.m_xPosition.x, 5.0f,   0.0001f, "vertex 0 -> 0+5");
	ZENITH_ASSERT_EQ_FLOAT(xOut1.m_xPosition.x, 105.0f, 0.0001f, "vertex 1 -> 100+5 (stride addressed its own slot)");
}

// ---- previous-pose positions-only raw skinning (TAA velocity) --------------

ZENITH_TEST(Skinning, PrevPositionRawMatchesFullSkinPosition)
{
	// The prev arena is positions-only (3 words/vertex) written by a second skinning
	// dispatch that shares the current jobs but uses the PREVIOUS palette. Require the
	// prev-position raw write to equal Flux_SkinVertexCPU's POSITION for the same input +
	// palette — so the compact 3-word path never drifts from the pinned skinning math.
	Zenith_Maths::Matrix4 axPalette[2] = { Zenith_Maths::Matrix4(1.0f), Skin_Translate(10.0f, -3.0f, 2.0f) };

	Flux_SkinInputVertex xIn = Skin_MakeVertex(
		Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f), Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f),
		0u, 1u, uFLUX_SKIN_BONE_SENTINEL, uFLUX_SKIN_BONE_SENTINEL,
		0.25f, 0.75f, 0.0f, 0.0f);

	u_int auIn[uFLUX_SKIN_INPUT_WORDS];
	memcpy(auIn, &xIn, sizeof(xIn));

	u_int auPrev[uFLUX_SKIN_PREV_WORDS] = {};
	Flux_SkinPrevPositionRaw(auIn, 0u, axPalette, 0u, 2u, auPrev, 0u);

	float fPx, fPy, fPz;
	memcpy(&fPx, &auPrev[0], sizeof(fPx));
	memcpy(&fPy, &auPrev[1], sizeof(fPy));
	memcpy(&fPz, &auPrev[2], sizeof(fPz));

	const Flux_SkinOutputVertex xTyped = Flux_SkinVertexCPU(xIn, axPalette, 0u, 2u);
	ZENITH_ASSERT_EQ_FLOAT(fPx, xTyped.m_xPosition.x, 0.0001f, "prev raw pos X == full skinner position");
	ZENITH_ASSERT_EQ_FLOAT(fPy, xTyped.m_xPosition.y, 0.0001f, "prev raw pos Y == full skinner position");
	ZENITH_ASSERT_EQ_FLOAT(fPz, xTyped.m_xPosition.z, 0.0001f, "prev raw pos Z == full skinner position");
}

ZENITH_TEST(Skinning, PrevPositionRawHonoursThreeWordStride)
{
	// Two vertices into two prev-arena slots: the 3-word stride must address distinct
	// slots (vertex 1 must not clobber vertex 0). This pins the GLOBAL-index layout the
	// velocity VS relies on (prevArena[(outBase + SV_VertexID) * 3 + k]).
	Zenith_Maths::Matrix4 axPalette[1] = { Skin_Translate(5.0f, 0.0f, 0.0f) };

	Flux_SkinInputVertex xIn0 = Skin_MakeVertex(
		Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f),
		0u, uFLUX_SKIN_BONE_SENTINEL, uFLUX_SKIN_BONE_SENTINEL, uFLUX_SKIN_BONE_SENTINEL, 1.0f, 0.0f, 0.0f, 0.0f);
	Flux_SkinInputVertex xIn1 = Skin_MakeVertex(
		Zenith_Maths::Vector3(100.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f),
		0u, uFLUX_SKIN_BONE_SENTINEL, uFLUX_SKIN_BONE_SENTINEL, uFLUX_SKIN_BONE_SENTINEL, 1.0f, 0.0f, 0.0f, 0.0f);

	u_int auIn[2u * uFLUX_SKIN_INPUT_WORDS];
	memcpy(auIn + 0u * uFLUX_SKIN_INPUT_WORDS, &xIn0, sizeof(xIn0));
	memcpy(auIn + 1u * uFLUX_SKIN_INPUT_WORDS, &xIn1, sizeof(xIn1));

	u_int auPrev[2u * uFLUX_SKIN_PREV_WORDS] = {};
	Flux_SkinPrevPositionRaw(auIn, 0u, axPalette, 0u, 1u, auPrev, 0u);
	Flux_SkinPrevPositionRaw(auIn, 1u, axPalette, 0u, 1u, auPrev, 1u);

	float fX0, fX1;
	memcpy(&fX0, &auPrev[0u * uFLUX_SKIN_PREV_WORDS], sizeof(fX0));
	memcpy(&fX1, &auPrev[1u * uFLUX_SKIN_PREV_WORDS], sizeof(fX1));
	ZENITH_ASSERT_EQ_FLOAT(fX0, 5.0f,   0.0001f, "prev vertex 0 -> 0+5");
	ZENITH_ASSERT_EQ_FLOAT(fX1, 105.0f, 0.0001f, "prev vertex 1 -> 100+5 (3-word stride addressed its own slot)");
}

// ---- cull-bounds inflation -------------------------------------------------

ZENITH_TEST(Skinning, InflateBoundsSphereScalesRadiusKeepsCentre)
{
	Zenith_Maths::Vector4 xSphere(3.0f, 4.0f, 5.0f, 2.0f);
	Zenith_Maths::Vector4 xInflated = Flux_InflateBoundsSphere(xSphere, fFLUX_SKIN_BOUNDS_INFLATION);

	ZENITH_ASSERT_EQ_FLOAT(xInflated.x, 3.0f, 0.0001f, "centre X unchanged");
	ZENITH_ASSERT_EQ_FLOAT(xInflated.y, 4.0f, 0.0001f, "centre Y unchanged");
	ZENITH_ASSERT_EQ_FLOAT(xInflated.z, 5.0f, 0.0001f, "centre Z unchanged");
	ZENITH_ASSERT_EQ_FLOAT(xInflated.w, 2.0f * fFLUX_SKIN_BOUNDS_INFLATION, 0.0001f, "radius scaled by the factor");
}

// ---- bone-palette builder: dedup + block bases -----------------------------

ZENITH_TEST(Skinning, BonePaletteBuilderDedupsSkeletonsAndAssignsBlockBases)
{
	Flux_BonePaletteBuilder xBuilder;
	xBuilder.Begin(/*bonesPerSkeleton*/ 100u);

	bool bNew = false;
	const u_int uBaseA = xBuilder.GetOrAddSkeleton(/*id*/ 0xAAAAull, bNew);
	ZENITH_ASSERT_EQ(uBaseA, 0u, "first skeleton -> base 0");
	ZENITH_ASSERT_TRUE(bNew, "first sight of a skeleton is newly added");
	ZENITH_ASSERT_EQ(xBuilder.GetSkeletonCount(), 1u, "one distinct skeleton");
	ZENITH_ASSERT_EQ(xBuilder.GetMatrixCount(), 100u, "one MAX_BONES block appended");

	// Same skeleton again -> same base, NOT newly added, no extra block.
	const u_int uBaseAagain = xBuilder.GetOrAddSkeleton(0xAAAAull, bNew);
	ZENITH_ASSERT_EQ(uBaseAagain, 0u, "repeat skeleton shares its block base");
	ZENITH_ASSERT_FALSE(bNew, "repeat skeleton is not newly added");
	ZENITH_ASSERT_EQ(xBuilder.GetMatrixCount(), 100u, "repeat skeleton appends no new block");

	// A second skeleton -> base at the next block.
	const u_int uBaseB = xBuilder.GetOrAddSkeleton(0xBBBBull, bNew);
	ZENITH_ASSERT_EQ(uBaseB, 100u, "second skeleton -> base after the first block");
	ZENITH_ASSERT_TRUE(bNew, "second skeleton is newly added");
	ZENITH_ASSERT_EQ(xBuilder.GetSkeletonCount(), 2u, "two distinct skeletons");
	ZENITH_ASSERT_EQ(xBuilder.GetMatrixCount(), 200u, "two MAX_BONES blocks");
}

ZENITH_TEST(Skinning, BonePaletteBuilderCallerFillsBlockAndSkinReadsIt)
{
	// End-to-end: the gather fills [base, base+count) of the palette, then Flux_SkinVertexCPU
	// reads through the base. Proves the base index + the shared palette wire together.
	Flux_BonePaletteBuilder xBuilder;
	xBuilder.Begin(2u);   // tiny 2-bone blocks for the test

	bool bNew = false;
	xBuilder.GetOrAddSkeleton(0x1ull, bNew);                       // skeleton 0 -> base 0
	const u_int uBaseB = xBuilder.GetOrAddSkeleton(0x2ull, bNew);  // skeleton 1 -> base 2
	ZENITH_ASSERT_EQ(uBaseB, 2u, "second 2-bone block starts at 2");

	// Fill skeleton 1's bone 0 with a translate(7,0,0).
	xBuilder.Matrices().Get(uBaseB + 0u) = Skin_Translate(7.0f, 0.0f, 0.0f);

	Flux_SkinInputVertex xIn = Skin_MakeVertex(
		Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f),
		0u, uFLUX_SKIN_BONE_SENTINEL, uFLUX_SKIN_BONE_SENTINEL, uFLUX_SKIN_BONE_SENTINEL,
		1.0f, 0.0f, 0.0f, 0.0f);

	Flux_SkinOutputVertex xOut = Flux_SkinVertexCPU(xIn,
		xBuilder.Matrices().GetDataPointer(), uBaseB, xBuilder.GetMatrixCount());
	ZENITH_ASSERT_EQ_FLOAT(xOut.m_xPosition.x, 7.0f, 0.0001f, "skin reads the skeleton's block via its base");
}

ZENITH_TEST(Skinning, BonePaletteBuilderBeginResetsStorage)
{
	Flux_BonePaletteBuilder xBuilder;
	xBuilder.Begin(50u);
	bool bNew = false;
	xBuilder.GetOrAddSkeleton(0x1ull, bNew);
	xBuilder.GetOrAddSkeleton(0x2ull, bNew);
	ZENITH_ASSERT_EQ(xBuilder.GetSkeletonCount(), 2u, "two before reset");

	xBuilder.Begin(50u);   // next frame
	ZENITH_ASSERT_EQ(xBuilder.GetSkeletonCount(), 0u, "Begin clears the skeleton set");
	ZENITH_ASSERT_EQ(xBuilder.GetMatrixCount(), 0u, "Begin clears the palette matrices");
}

// ---- golden hash: determinism + sensitivity --------------------------------

ZENITH_TEST(Skinning, SkinnedHashIsDeterministicAndSensitive)
{
	Zenith_Maths::Matrix4 axPalette[2] = { Zenith_Maths::Matrix4(1.0f), Skin_Translate(10.0f, 0.0f, 0.0f) };

	Flux_SkinInputVertex xIn = Skin_MakeVertex(
		Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f),
		0u, 1u, uFLUX_SKIN_BONE_SENTINEL, uFLUX_SKIN_BONE_SENTINEL,
		0.5f, 0.5f, 0.0f, 0.0f);

	Flux_SkinOutputVertex xA = Flux_SkinVertexCPU(xIn, axPalette, 0u, 2u);
	Flux_SkinOutputVertex xB = Flux_SkinVertexCPU(xIn, axPalette, 0u, 2u);
	ZENITH_ASSERT_EQ(Flux_HashSkinnedForTest(&xA, 1u), Flux_HashSkinnedForTest(&xB, 1u),
		"identical input + palette -> identical skinned hash");

	// A different pose (translate the second bone further) must change the hash.
	Zenith_Maths::Matrix4 axPalette2[2] = { Zenith_Maths::Matrix4(1.0f), Skin_Translate(20.0f, 0.0f, 0.0f) };
	Flux_SkinOutputVertex xC = Flux_SkinVertexCPU(xIn, axPalette2, 0u, 2u);
	ZENITH_ASSERT_NE(Flux_HashSkinnedForTest(&xA, 1u), Flux_HashSkinnedForTest(&xC, 1u),
		"a different bone pose must change the skinned hash");
}

// ---- persistent bind-pose pool: upload gating ------------------------------
// The pool is persistent; its GPU copy is frame-indexed, so ANY change (a growth OR an
// eviction re-pack, both bump the generation) must refresh every physical copy (upload for
// MAX_FRAMES_IN_FLIGHT frames) then skip. Gated on the generation, not the word count, so a
// re-pack that SHRINKS the pool still re-uploads.

ZENITH_TEST(Skinning, BindPosePoolUploadsForMaxFramesAfterChangeThenSkips)
{
	const u_int uN = 3u;   // pretend MAX_FRAMES_IN_FLIGHT (test-local so it's platform-independent)
	u_int uUploadedGen = 0u, uDirty = 0u;

	// Generation bumps 0 -> 1 (a mesh was appended). Upload + arm the remaining copies.
	ZENITH_ASSERT_TRUE(Flux_BindPosePoolShouldUpload(1u, uN, uUploadedGen, uDirty), "change frame uploads");
	ZENITH_ASSERT_EQ(uUploadedGen, 1u, "uploaded-generation tracks the pool generation");
	// Next frames: generation unchanged, still refreshing the remaining frame-indexed copies.
	ZENITH_ASSERT_TRUE(Flux_BindPosePoolShouldUpload(1u, uN, uUploadedGen, uDirty), "copy 2 refreshed");
	ZENITH_ASSERT_TRUE(Flux_BindPosePoolShouldUpload(1u, uN, uUploadedGen, uDirty), "copy 3 refreshed");
	// Steady state: every copy current -> skip (the whole point of the optimization).
	ZENITH_ASSERT_FALSE(Flux_BindPosePoolShouldUpload(1u, uN, uUploadedGen, uDirty), "steady state skips");
	ZENITH_ASSERT_FALSE(Flux_BindPosePoolShouldUpload(1u, uN, uUploadedGen, uDirty), "steady state stays skipped");
}

ZENITH_TEST(Skinning, BindPosePoolReArmsOnEachChangeIncludingShrink)
{
	const u_int uN = 2u;
	u_int uUploadedGen = 0u, uDirty = 0u;
	// First change (append): gen 1 -> upload + settle.
	ZENITH_ASSERT_TRUE (Flux_BindPosePoolShouldUpload(1u, uN, uUploadedGen, uDirty), "first change");
	ZENITH_ASSERT_TRUE (Flux_BindPosePoolShouldUpload(1u, uN, uUploadedGen, uDirty), "first change copy 2");
	ZENITH_ASSERT_FALSE(Flux_BindPosePoolShouldUpload(1u, uN, uUploadedGen, uDirty), "settled");
	// Second change — e.g. an eviction RE-PACK that shrinks the pool. Generation bumps to 2;
	// the word count went DOWN, but generation-gating still re-arms (the load-bearing property).
	ZENITH_ASSERT_TRUE (Flux_BindPosePoolShouldUpload(2u, uN, uUploadedGen, uDirty), "shrink/re-pack re-arms");
	ZENITH_ASSERT_EQ(uUploadedGen, 2u, "uploaded-generation advances on re-pack");
	ZENITH_ASSERT_TRUE (Flux_BindPosePoolShouldUpload(2u, uN, uUploadedGen, uDirty), "re-pack copy 2");
	ZENITH_ASSERT_FALSE(Flux_BindPosePoolShouldUpload(2u, uN, uUploadedGen, uDirty), "settled again");
}

ZENITH_TEST(Skinning, BindPosePoolNeverUploadsWhenUnchanged)
{
	const u_int uN = 4u;
	u_int uUploadedGen = 0u, uDirty = 0u;
	// No skinned content / no change: generation stays 0 -> never upload (no traffic, no stale read).
	ZENITH_ASSERT_FALSE(Flux_BindPosePoolShouldUpload(0u, uN, uUploadedGen, uDirty), "unchanged pool never uploads");
	ZENITH_ASSERT_FALSE(Flux_BindPosePoolShouldUpload(0u, uN, uUploadedGen, uDirty), "still unchanged -> still skip");
	ZENITH_ASSERT_EQ(uDirty, 0u, "no dirty frames armed for an unchanged pool");
}

// ---- stable skinned-instance id allocator (optimization (iii)) -------------

ZENITH_TEST(Skinning, SkinnedIdIsStableAcrossSyncsAndDistinctPerIdentity)
{
	Flux_SkinnedInstanceIdRegistry xReg;
	int iSkelA = 0, iSkelB = 0, iMesh = 0;   // distinct addresses = opaque identities (never deref'd)
	Flux_SkinnedInstanceKey xA{ &iSkelA, &iMesh, 0u };
	Flux_SkinnedInstanceKey xB{ &iSkelB, &iMesh, 0u };   // different skeleton instance
	Flux_SkinnedInstanceKey xA1{ &iSkelA, &iMesh, 1u };  // same skeleton+mesh, different submesh slot

	xReg.BeginSync();
	const u_int uA  = xReg.Reference(xA);
	const u_int uB  = xReg.Reference(xB);
	const u_int uA1 = xReg.Reference(xA1);
	xReg.EndSync();
	ZENITH_ASSERT_NE(uA, uB,  "distinct skeleton instances -> distinct ids");
	ZENITH_ASSERT_NE(uA, uA1, "distinct submesh slots -> distinct ids");
	ZENITH_ASSERT_EQ(xReg.GetLiveCount(), 3u, "three live skinned instances");

	// Next frame: the SAME identities -> the SAME ids (the whole point of the optimization).
	xReg.BeginSync();
	ZENITH_ASSERT_EQ(xReg.Reference(xA),  uA,  "id stable across syncs (A)");
	ZENITH_ASSERT_EQ(xReg.Reference(xB),  uB,  "id stable across syncs (B)");
	ZENITH_ASSERT_EQ(xReg.Reference(xA1), uA1, "id stable across syncs (A submesh 1)");
	xReg.EndSync();
}

ZENITH_TEST(Skinning, SkinnedIdRecyclesWhenInstanceStopsBeingDrawn)
{
	Flux_SkinnedInstanceIdRegistry xReg;
	int iSkelA = 0, iSkelB = 0, iMesh = 0;
	Flux_SkinnedInstanceKey xA{ &iSkelA, &iMesh, 0u };
	Flux_SkinnedInstanceKey xB{ &iSkelB, &iMesh, 0u };

	xReg.BeginSync();
	const u_int uA = xReg.Reference(xA);
	xReg.EndSync();
	ZENITH_ASSERT_EQ(xReg.GetLiveCount(), 1u, "A live");

	// A despawns (not referenced this sync) -> its id is recycled, live count drops.
	xReg.BeginSync();
	xReg.EndSync();
	ZENITH_ASSERT_EQ(xReg.GetLiveCount(), 0u, "A retired when no longer referenced");

	// A brand-new instance gets the recycled slot (id space stays bounded over a session).
	xReg.BeginSync();
	const u_int uB = xReg.Reference(xB);
	xReg.EndSync();
	ZENITH_ASSERT_EQ(uB, uA, "freed id is recycled for the next new instance");
}

ZENITH_TEST(Skinning, SkinnedIdNoCollisionAcrossManyInstances)
{
	// 5000 distinct identities -> 5000 distinct ids, all in the 31-bit space. This guards the
	// rejected 31-bit-HASH design: a hash collision would merge two distinct poses into one
	// bucket -> the wrong arena slice -> visible corruption. The allocator is collision-free.
	Flux_SkinnedInstanceIdRegistry xReg;
	int iMesh = 0;
	Zenith_HashMap<u_int, u_int> xSeen;
	xReg.BeginSync();
	for (u_int u = 0; u < 5000u; ++u)
	{
		// distinct fake skeleton pointers (the allocator only hashes/compares the bits)
		Flux_SkinnedInstanceKey xK{ reinterpret_cast<const void*>((size_t)(u + 1u)), &iMesh, 0u };
		const u_int uId = xReg.Reference(xK);
		ZENITH_ASSERT_TRUE(uId < 0x80000000u, "id stays in the 31-bit space (high bit reserved for the skinned tag)");
		ZENITH_ASSERT_FALSE(xSeen.Contains(uId), "no two distinct identities share an id");
		xSeen.Insert(uId, u);
	}
	xReg.EndSync();
	ZENITH_ASSERT_EQ(xReg.GetLiveCount(), 5000u, "all 5000 live + distinct");
}

// ---- skinned-pose store (folded onto the refcount-diff base) ----------------
// Headless via a MOCK provider (no GPU): each build mints a heap entry with fake bind-pose
// words so the registry's create/evict orchestration + the OWNED bind-pose pool (append on
// create, wholesale re-pack on retire, generation gating) are exercised without a renderer.

namespace
{
	int         g_iPoseBuilds        = 0;
	int         g_iPoseDestroys      = 0;
	const void* g_pvPoseLastDestroyed = nullptr;
	constexpr u_int kuPOSE_TEST_VERTS = 2u;   // each mock pose is 2 verts

	void Pose_ResetCounters()
	{
		g_iPoseBuilds         = 0;
		g_iPoseDestroys       = 0;
		g_pvPoseLastDestroyed = nullptr;
	}

	bool Pose_MockBuild(const Flux_SkinnedPoseKey& xKey, Flux_SkinnedPoseEntry*& pxOut)
	{
		++g_iPoseBuilds;
		Flux_SkinnedPoseEntry* pxEntry = new Flux_SkinnedPoseEntry();
		pxEntry->m_pxMesh        = nullptr;   // mock destroy never dereferences it
		pxEntry->m_pvSourceAsset = xKey.m_pvAsset;
		pxEntry->m_uNumVerts     = kuPOSE_TEST_VERTS;
		for (u_int i = 0; i < kuPOSE_TEST_VERTS * uFLUX_SKIN_INPUT_WORDS; ++i)
		{
			pxEntry->m_auBindPoseWords.PushBack(i);
		}
		pxOut = pxEntry;
		return true;
	}
	void Pose_MockDestroy(Flux_SkinnedPoseEntry*& pxEntry)
	{
		++g_iPoseDestroys;
		if (pxEntry != nullptr)
		{
			g_pvPoseLastDestroyed = pxEntry->m_pvSourceAsset;
			delete pxEntry;
			pxEntry = nullptr;
		}
	}
	Flux_SkinnedPoseRegistry::Provider Pose_MockProvider()
	{
		Flux_SkinnedPoseRegistry::Provider x;
		x.m_pfnBuild   = &Pose_MockBuild;
		x.m_pfnDestroy = &Pose_MockDestroy;
		return x;
	}
}

ZENITH_TEST(SkinnedPose, BuildsOncePerAssetAndAppendsToPoolOnce)
{
	Flux_SkinnedPoseRegistry xReg;
	xReg.SetProvider(Pose_MockProvider());
	Pose_ResetCounters();
	int iAssetA = 0;

	// Two instances of the SAME skinned mesh in one sync -> one build, one pool slice.
	xReg.BeginFrameEvictingPrevious();
	Flux_SkinnedPoseEntry* pxA  = xReg.Reference(&iAssetA);
	Flux_SkinnedPoseEntry* pxA2 = xReg.Reference(&iAssetA);

	ZENITH_ASSERT_NOT_NULL(pxA, "first reference builds the pose");
	ZENITH_ASSERT_EQ(pxA, pxA2, "same asset -> same cached pose entry");
	ZENITH_ASSERT_EQ(g_iPoseBuilds, 1, "the pose is built exactly once for repeated references");
	ZENITH_ASSERT_EQ(xReg.GetLiveCount(), 1u, "one distinct skinned mesh -> one live entry");
	ZENITH_ASSERT_EQ(pxA->m_uPoolVertBase, 0u, "first pose sits at pool vertex base 0");
	ZENITH_ASSERT_EQ(xReg.GetPoolWords().GetSize(), kuPOSE_TEST_VERTS * uFLUX_SKIN_INPUT_WORDS,
		"the pool holds exactly one mesh's bind-pose words");

	xReg.Shutdown();   // free the heap entry (no GPU in the mock)
}

ZENITH_TEST(SkinnedPose, EvictsUndrawnAssetAndRepacksPool)
{
	Flux_SkinnedPoseRegistry xReg;
	xReg.SetProvider(Pose_MockProvider());
	Pose_ResetCounters();
	int iAssetA = 0, iAssetB = 0;

	// Frame 1: draw A + B.
	xReg.BeginFrameEvictingPrevious();
	xReg.Reference(&iAssetA);
	xReg.Reference(&iAssetB);
	ZENITH_ASSERT_EQ(g_iPoseBuilds, 2, "two distinct meshes built");
	ZENITH_ASSERT_EQ(xReg.GetPoolWords().GetSize(), 2u * kuPOSE_TEST_VERTS * uFLUX_SKIN_INPUT_WORDS, "pool holds both meshes");
	const u_int uGenAfterF1 = xReg.GetPoolGeneration();

	// Frame 2: draw only A. B is unreferenced THIS sync but not yet evicted (eviction is deferred
	// to the next frame's BeginFrameEvictingPrevious, so pool bases stay stable through this walk).
	xReg.BeginFrameEvictingPrevious();
	xReg.Reference(&iAssetA);
	ZENITH_ASSERT_EQ(g_iPoseDestroys, 0, "nothing evicted yet (B was referenced in frame 1)");
	ZENITH_ASSERT_EQ(xReg.GetPoolGeneration(), uGenAfterF1, "steady frame does not bump the pool generation");

	// Frame 3: the deferred evict of B fires -> its entry destroyed, pool re-packed to just A.
	xReg.BeginFrameEvictingPrevious();
	ZENITH_ASSERT_EQ(g_iPoseDestroys, 1, "B is evicted when it stops being drawn");
	ZENITH_ASSERT_EQ(g_pvPoseLastDestroyed, (const void*)&iAssetB, "the correct (B's) pose is destroyed");
	ZENITH_ASSERT_EQ(xReg.GetLiveCount(), 1u, "only A remains live");
	ZENITH_ASSERT_EQ(xReg.GetPoolWords().GetSize(), kuPOSE_TEST_VERTS * uFLUX_SKIN_INPUT_WORDS, "pool re-packed to one mesh");
	ZENITH_ASSERT_TRUE(xReg.GetPoolGeneration() != uGenAfterF1, "an eviction re-pack bumps the pool generation");

	// A survivor's base is re-assigned by the re-pack (here back to 0).
	u_int uIdA = 0u;
	Flux_SkinnedPoseKey xKeyA; xKeyA.m_pvAsset = &iAssetA;
	ZENITH_ASSERT_TRUE(xReg.TryGetId(xKeyA, uIdA), "A still resolves after the re-pack");
	ZENITH_ASSERT_EQ((*xReg.TryGetPayload(uIdA))->m_uPoolVertBase, 0u, "the survivor's pool base is re-packed to 0");

	xReg.Shutdown();
}

ZENITH_TEST(SkinnedPose, NewAssetAfterRepackAppendsAtPostRepackTailAndPoolContentIntact)
{
	// After an eviction re-pack shrinks the pool, a brand-new asset must append at the POST-repack tail
	// (not a stale pre-repack offset), and the survivor's bind-pose words must be intact in the pool.
	// The mock writes sequential words 0,1,2,... per mesh, so the pool slice content is verifiable.
	Flux_SkinnedPoseRegistry xReg;
	xReg.SetProvider(Pose_MockProvider());
	Pose_ResetCounters();
	int iAssetA = 0, iAssetB = 0, iAssetC = 0;
	const u_int uWordsPerMesh = kuPOSE_TEST_VERTS * uFLUX_SKIN_INPUT_WORDS;
	const u_int uLastWord = uWordsPerMesh - 1u;   // mock's highest word value in a slice

	// F1: A + B.
	xReg.BeginFrameEvictingPrevious();
	Flux_SkinnedPoseEntry* pxA = xReg.Reference(&iAssetA);
	xReg.Reference(&iAssetB);
	ZENITH_ASSERT_EQ(pxA->m_uPoolVertBase, 0u, "A at base 0");
	ZENITH_ASSERT_EQ(xReg.GetPoolWords().Get(0u), 0u, "pool holds A's first bind-pose word");
	ZENITH_ASSERT_EQ(xReg.GetPoolWords().Get(uLastWord), uLastWord, "pool holds A's last bind-pose word (full slice copied)");

	// F2: only A (B unreferenced; eviction deferred to F3).
	xReg.BeginFrameEvictingPrevious();
	xReg.Reference(&iAssetA);

	// F3: deferred evict of B + re-pack (A -> base 0). A brand-new asset C must append at the
	// post-repack tail = A's word count, NOT B's old base.
	xReg.BeginFrameEvictingPrevious();
	xReg.Reference(&iAssetA);
	Flux_SkinnedPoseEntry* pxC = xReg.Reference(&iAssetC);
	ZENITH_ASSERT_NOT_NULL(pxC, "C built after the re-pack");
	ZENITH_ASSERT_EQ(pxC->m_uPoolVertBase, kuPOSE_TEST_VERTS, "C appends at the post-repack tail (after A's one mesh)");
	ZENITH_ASSERT_EQ(xReg.GetPoolWords().GetSize(), 2u * uWordsPerMesh, "pool holds A + C, B's slice reclaimed");
	ZENITH_ASSERT_EQ(xReg.GetPoolWords().Get(0u), 0u, "A's words survive the re-pack intact (first word)");
	ZENITH_ASSERT_EQ(xReg.GetPoolWords().Get(uWordsPerMesh), 0u, "C's slice begins at the post-repack tail (first word)");

	xReg.Shutdown();
}

ZENITH_TEST(SkinnedPose, NullAssetIsRejected)
{
	Flux_SkinnedPoseRegistry xReg;
	xReg.SetProvider(Pose_MockProvider());
	Pose_ResetCounters();

	xReg.BeginFrameEvictingPrevious();
	ZENITH_ASSERT_NULL(xReg.Reference(nullptr), "a null asset builds no pose");
	ZENITH_ASSERT_EQ(g_iPoseBuilds, 0, "a null asset never invokes the provider");
	ZENITH_ASSERT_EQ(xReg.GetLiveCount(), 0u, "a null asset registers no entry");
}

// ---- previous-frame bone-palette history (skinned motion vectors) ----------
// Flux_BonePaletteHistory rebuilds a prev-frame palette laid out at the SAME bases as the
// current palette, keyed by OPAQUE skeleton id (not base). Pure CPU; it drives the second
// positions-only skinning dispatch that produces skinned uvPrev for TAA velocity.

namespace
{
	Zenith_Maths::Matrix4 Hist_Translate(float fX)   // distinct, easily-probed matrix
	{
		Zenith_Maths::Matrix4 xM(1.0f);
		xM[3] = Zenith_Maths::Vector4(fX, 0.0f, 0.0f, 1.0f);
		return xM;
	}
	float Hist_TX(const Zenith_Maths::Matrix4& xM) { return xM[3].x; }   // translation.x probe
}

ZENITH_TEST(BonePaletteHistory, FirstSightPrevEqualsCurrent)
{
	Flux_BonePaletteHistory xHist;
	xHist.BeginFrame(2u);
	const Zenith_Maths::Matrix4 axCur[2] = { Hist_Translate(3.0f), Hist_Translate(4.0f) };
	xHist.SubmitSkeleton(0xA1ull, /*base*/ 0u, axCur, 2u);
	const Zenith_Vector<Zenith_Maths::Matrix4>& xPrev = xHist.PrevPalette();
	ZENITH_ASSERT_EQ(xPrev.GetSize(), 2u, "prev palette covers the one 2-bone block");
	ZENITH_ASSERT_EQ_FLOAT(Hist_TX(xPrev.Get(0u)), 3.0f, 1e-6f, "first sight: prev == current (bone 0)");
	ZENITH_ASSERT_EQ_FLOAT(Hist_TX(xPrev.Get(1u)), 4.0f, 1e-6f, "first sight: prev == current (bone 1)");
}

ZENITH_TEST(BonePaletteHistory, SecondFramePrevEqualsPriorFrame)
{
	Flux_BonePaletteHistory xHist;
	xHist.BeginFrame(2u);
	const Zenith_Maths::Matrix4 axF1[2] = { Hist_Translate(1.0f), Hist_Translate(2.0f) };
	xHist.SubmitSkeleton(0xA1ull, 0u, axF1, 2u);
	xHist.EndFrame();

	xHist.BeginFrame(2u);   // frame 2: skeleton moved
	const Zenith_Maths::Matrix4 axF2[2] = { Hist_Translate(10.0f), Hist_Translate(20.0f) };
	xHist.SubmitSkeleton(0xA1ull, 0u, axF2, 2u);
	const Zenith_Vector<Zenith_Maths::Matrix4>& xPrev = xHist.PrevPalette();
	ZENITH_ASSERT_EQ_FLOAT(Hist_TX(xPrev.Get(0u)), 1.0f, 1e-6f, "prev is last frame's matrices, not this frame's");
	ZENITH_ASSERT_EQ_FLOAT(Hist_TX(xPrev.Get(1u)), 2.0f, 1e-6f, "prev is last frame's matrices (bone 1)");
}

ZENITH_TEST(BonePaletteHistory, BlockTailStaysIdentity)
{
	Flux_BonePaletteHistory xHist;
	xHist.BeginFrame(4u);   // 4-bone blocks
	const Zenith_Maths::Matrix4 axCur[2] = { Hist_Translate(5.0f), Hist_Translate(6.0f) };
	xHist.SubmitSkeleton(0xA1ull, 0u, axCur, 2u);   // only 2 bones used
	const Zenith_Vector<Zenith_Maths::Matrix4>& xPrev = xHist.PrevPalette();
	ZENITH_ASSERT_EQ(xPrev.GetSize(), 4u, "block covers bonesPerSkeleton (4) even with 2 used bones");
	ZENITH_ASSERT_EQ_FLOAT(Hist_TX(xPrev.Get(2u)), 0.0f, 1e-6f, "unused tail bone 2 is identity");
	ZENITH_ASSERT_EQ_FLOAT(Hist_TX(xPrev.Get(3u)), 0.0f, 1e-6f, "unused tail bone 3 is identity");
}

ZENITH_TEST(BonePaletteHistory, TwoSkeletonsAtDistinctBases)
{
	Flux_BonePaletteHistory xHist;
	xHist.BeginFrame(2u);
	const Zenith_Maths::Matrix4 axA[2] = { Hist_Translate(1.0f), Hist_Translate(1.5f) };
	const Zenith_Maths::Matrix4 axB[2] = { Hist_Translate(2.0f), Hist_Translate(2.5f) };
	xHist.SubmitSkeleton(0xAAull, 0u, axA, 2u);
	xHist.SubmitSkeleton(0xBBull, 2u, axB, 2u);
	xHist.EndFrame();

	xHist.BeginFrame(2u);   // frame 2: both moved
	const Zenith_Maths::Matrix4 ax9[2] = { Hist_Translate(9.0f), Hist_Translate(9.0f) };
	xHist.SubmitSkeleton(0xAAull, 0u, ax9, 2u);
	xHist.SubmitSkeleton(0xBBull, 2u, ax9, 2u);
	const Zenith_Vector<Zenith_Maths::Matrix4>& xPrev = xHist.PrevPalette();
	ZENITH_ASSERT_EQ(xPrev.GetSize(), 4u, "palette covers both blocks");
	ZENITH_ASSERT_EQ_FLOAT(Hist_TX(xPrev.Get(0u)), 1.0f, 1e-6f, "A's prev at base 0");
	ZENITH_ASSERT_EQ_FLOAT(Hist_TX(xPrev.Get(2u)), 2.0f, 1e-6f, "B's prev at base 2");
}

ZENITH_TEST(BonePaletteHistory, HistoryKeyedByIdNotBaseAcrossRepack)
{
	// Skeleton B moves to a DIFFERENT base after A is evicted (a re-pack). Its prev must still be
	// B's own last-frame matrices — proving history is keyed by id, not by base.
	Flux_BonePaletteHistory xHist;
	xHist.BeginFrame(2u);
	const Zenith_Maths::Matrix4 axA[2] = { Hist_Translate(1.0f), Hist_Translate(1.0f) };
	const Zenith_Maths::Matrix4 axB[2] = { Hist_Translate(7.0f), Hist_Translate(8.0f) };
	xHist.SubmitSkeleton(0xAAull, 0u, axA, 2u);
	xHist.SubmitSkeleton(0xBBull, 2u, axB, 2u);
	xHist.EndFrame();

	xHist.BeginFrame(2u);   // frame 2: A gone; B re-packed to base 0
	const Zenith_Maths::Matrix4 axB2[2] = { Hist_Translate(70.0f), Hist_Translate(80.0f) };
	xHist.SubmitSkeleton(0xBBull, /*new base*/ 0u, axB2, 2u);
	const Zenith_Vector<Zenith_Maths::Matrix4>& xPrev = xHist.PrevPalette();
	ZENITH_ASSERT_EQ_FLOAT(Hist_TX(xPrev.Get(0u)), 7.0f, 1e-6f, "B's prev follows B's id to its new base (bone 0)");
	ZENITH_ASSERT_EQ_FLOAT(Hist_TX(xPrev.Get(1u)), 8.0f, 1e-6f, "B's prev follows B's id to its new base (bone 1)");
}

ZENITH_TEST(BonePaletteHistory, MidFrameWriteDoesNotCorruptEarlierBlock)
{
	Flux_BonePaletteHistory xHist;
	xHist.BeginFrame(2u);
	const Zenith_Maths::Matrix4 axA[2] = { Hist_Translate(11.0f), Hist_Translate(12.0f) };
	xHist.SubmitSkeleton(0xAAull, 0u, axA, 2u);   // first block written
	const Zenith_Maths::Matrix4 axB[2] = { Hist_Translate(21.0f), Hist_Translate(22.0f) };
	xHist.SubmitSkeleton(0xBBull, 2u, axB, 2u);   // second block appended (grows palette)
	const Zenith_Vector<Zenith_Maths::Matrix4>& xPrev = xHist.PrevPalette();
	ZENITH_ASSERT_EQ_FLOAT(Hist_TX(xPrev.Get(0u)), 11.0f, 1e-6f, "A block intact after B appended (bone 0)");
	ZENITH_ASSERT_EQ_FLOAT(Hist_TX(xPrev.Get(1u)), 12.0f, 1e-6f, "A block intact after B appended (bone 1)");
}

ZENITH_TEST(BonePaletteHistory, DroppedSkeletonRestartsAtPrevEqualsCurrent)
{
	Flux_BonePaletteHistory xHist;
	xHist.BeginFrame(2u);
	const Zenith_Maths::Matrix4 axF1[2] = { Hist_Translate(4.0f), Hist_Translate(4.0f) };
	xHist.SubmitSkeleton(0xCCull, 0u, axF1, 2u);
	xHist.EndFrame();

	xHist.BeginFrame(2u);   // a frame WITHOUT skeleton CC
	xHist.EndFrame();
	ZENITH_ASSERT_EQ(xHist.GetHistoryCount(), 0u, "history empty after a frame with no skeletons");

	xHist.BeginFrame(2u);   // CC reappears — no history => prev == current
	const Zenith_Maths::Matrix4 axF3[2] = { Hist_Translate(30.0f), Hist_Translate(31.0f) };
	xHist.SubmitSkeleton(0xCCull, 0u, axF3, 2u);
	const Zenith_Vector<Zenith_Maths::Matrix4>& xPrev = xHist.PrevPalette();
	ZENITH_ASSERT_EQ_FLOAT(Hist_TX(xPrev.Get(0u)), 30.0f, 1e-6f, "dropped skeleton restarts at prev == current");
}
