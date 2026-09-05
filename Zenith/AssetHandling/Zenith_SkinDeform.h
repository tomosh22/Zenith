#pragma once
#include "Maths/Zenith_Maths.h"
#include "AssetHandling/Zenith_HumanProportions.h"

class Zenith_MeshAsset;
class Zenith_SkeletonAsset;

// ============================================================================
// Zenith_SkinDeform -- the two operations that move a humanoid mesh, and the
// measurement both of them are driven from.
//
// ★★ THEY ARE NOT THE SAME OPERATION, and the whole file exists to keep them
// apart.
//
//   RE-PROPORTION  a mesh authored against the old rig, so its knee is where the
//                  new rig's knee bone is. Regions must STRETCH -- a leg gets
//                  longer while the body stays the same height. That is a WARP.
//
//   RE-BIND        a mesh authored against one rig onto another, so a T-posed
//                  import ends up in the arms-down bind pose. Nothing may change
//                  SIZE; joints rotate. That is linear-blend skinning.
//
// Reaching for the wrong one is the easy mistake and it is invisible in a test
// that only checks "did the vertices move". Linear-blend re-binding TRANSLATES a
// vertex weighted 1.0 to a single bone, so re-proportioning with it shrinks a
// body by the distance the end bones moved: Zenithmon's crown is fully weighted
// to Head and its lowest foot region fully to Foot, so an LBS "re-proportion"
// would drag a 2.60-unit body down to about 2.36 and every downstream constant
// that divides by a canonical height would then render one human 10% taller than
// the next. WarpPreservesHeightAndPinnedLandmarks is the test for it.
//
// ★ NORMALS. The vertex warp is non-uniform -- it stretches one region against
// another -- so it INVALIDATES normals and may only be called where they are
// rebuilt afterwards. StickFigure rebuilds them (ComputeHumanSmoothNormals);
// Zenithmon deliberately does not (ZM_HumanMesh.cpp:286 -- "EmitRing already
// wrote analytic loft normals; never regenerate them"), so it warps its loft
// RINGS instead and lets the analytic generator run on already-warped geometry.
// Two forms, one map, WarpRingAndVertexFormsAgree pins that they agree.
//
// The rebind carries normals correctly on its own, because a bind-pose change is
// a rotation and it has one to give them.
// ============================================================================

//-----------------------------------------------------------------------------
// A mesh, as this file needs to see it. Zenith_MeshAsset and Zenithmon's
// ZM_GenMesh have the same field shapes and different types (ZM_GenCommon.h
// :180-196 says so on purpose), so a view is what lets one implementation serve
// both without either of them learning about the other.
//-----------------------------------------------------------------------------
struct Zenith_SkinDeformView
{
	Zenith_Maths::Vector3* m_pxPositions   = nullptr;
	Zenith_Maths::Vector3* m_pxNormals     = nullptr;   // optional; the rebind rotates them
	const glm::uvec4*      m_pxBoneIndices = nullptr;   // optional; without it every weight reads as zero
	const glm::vec4*       m_pxBoneWeights = nullptr;
	u_int                  m_uNumVerts     = 0u;

	bool IsValid() const { return m_pxPositions != nullptr && m_uNumVerts > 0u; }
	bool HasSkinning() const { return m_pxBoneIndices != nullptr && m_pxBoneWeights != nullptr; }
};

Zenith_SkinDeformView Zenith_MakeSkinDeformView(Zenith_MeshAsset& xMesh);

//-----------------------------------------------------------------------------
// Which way the arms are held. It decides which axis the arm chain is measured
// along, and nothing else -- a T-posed arm runs sideways through one narrow band
// of Y, an arms-down arm runs vertically through most of the body's Y range, and
// a scan that assumes the wrong one finds the ribcage.
//-----------------------------------------------------------------------------
enum ZENITH_HUMAN_POSE
{
	ZENITH_HUMAN_POSE_ARMS_DOWN,
	ZENITH_HUMAN_POSE_T_POSE
};

//-----------------------------------------------------------------------------
// The warp's anchor chains. Ascending, always -- PiecewiseLinear needs it and
// IsMonotonic asserts it on the REAL arrays rather than on the idea of them.
//-----------------------------------------------------------------------------
enum ZENITH_HUMAN_BODY_ANCHOR
{
	ZENITH_HUMAN_BODY_SOLE = 0,
	ZENITH_HUMAN_BODY_ANKLE,
	ZENITH_HUMAN_BODY_KNEE,
	ZENITH_HUMAN_BODY_HIP,
	ZENITH_HUMAN_BODY_SHOULDER,
	ZENITH_HUMAN_BODY_NECK,
	ZENITH_HUMAN_BODY_HEAD,
	ZENITH_HUMAN_BODY_CROWN,
	ZENITH_HUMAN_BODY_ANCHOR_COUNT
};

enum ZENITH_HUMAN_ARM_ANCHOR
{
	ZENITH_HUMAN_ARM_FINGERTIP = 0,
	ZENITH_HUMAN_ARM_WRIST,
	ZENITH_HUMAN_ARM_ELBOW,
	ZENITH_HUMAN_ARM_SHOULDER,
	ZENITH_HUMAN_ARM_ANCHOR_COUNT
};

//-----------------------------------------------------------------------------
// What a bounded scan of one humanoid mesh found.
//
// ★ MEASURED MESH LANDMARKS, NEVER BONE POSITIONS. This is what keeps the anchor
// arrays monotonic. Zenithmon's lowest vertex is -0.995145 while the legacy Foot
// PIVOT is -1.0 -- the sole sits ABOVE the bone -- so an anchor array taken from
// the skeleton is already out of order before the rig is even touched, and the
// order flips again once Foot moves up. Measuring each mesh's own geometry and
// mapping onto the rig's planes makes the ordering a property of the data.
//
// ★ A LANDMARK A MESH DOES NOT HAVE IS NOT AN ERROR. Zenithmon's legs taper to a
// point: it has no shoes, so it has no ankle seam, so its lower leg has no
// interior radius minimum to find. That anchor is dropped and the neighbouring
// segment spans it, which is the correct answer -- a foot that is not modelled
// cannot be stretched into a boot.
//-----------------------------------------------------------------------------
struct Zenith_HumanLandmarks
{
	bool m_bValid = false;

	ZENITH_HUMAN_POSE m_ePose = ZENITH_HUMAN_POSE_ARMS_DOWN;

	float m_afBodyY[ZENITH_HUMAN_BODY_ANCHOR_COUNT] = {};
	bool  m_abBodyFound[ZENITH_HUMAN_BODY_ANCHOR_COUNT] = {};

	// Ascending: fingertip, wrist, elbow, shoulder. In ARMS_DOWN these are mesh Y;
	// in T_POSE they are distances OUT along the lateral axis from the body centre.
	float m_afArmChain[ZENITH_HUMAN_ARM_ANCHOR_COUNT] = {};
	bool  m_bArmChainFound = false;

	// ★ SOME ARMS HAVE NO HAND, and saying so matters more than it sounds.
	// Zenithmon lofts no hand at all -- its arm ends in a cap 3 mm below its
	// wrist -- so a fingertip anchor there would map a 3 mm sliver of end-cap
	// onto the rig's whole 26 cm hand and draw it out into a spike. When this is
	// false the fingertip anchor is dropped and the chain simply ends at the
	// wrist, which is what that mesh actually has.
	bool  m_bArmHasHand = false;

	float m_fShoulderHalfX = 0.0f;
	float m_fHipHalfX      = 0.0f;

	// ★★ WHICH WAY THE BODY FACES IS MEASURABLE, and it was treated as if it were
	// not. A foot settles it: the ANKLE SITS AT THE BACK, so from the shin's own
	// axis the foot reaches about three times further forward than back. That is
	// anatomy -- true of everybody, in every shoe -- and no human has to be asked.
	//
	// ★ IT IS NOT "the heel is the taller end". That is true of a bare foot and
	// false of a trainer with a built-up toe box, and it gave OPPOSITE answers on
	// the two meshes here while looking equally confident on both.
	//
	// Facing was not measured at all at first, and a 180-degree-wrong character
	// shipped past a full screenshot pass: the check performed was "do both humans
	// show a face at this camera", and at head-thumbnail size the back of a head
	// reads as a face. m_fToeZ used to be simply max-Z below the ankle -- it
	// ASSUMED the answer and so could never contradict it.
	float m_fToeZ          = 0.0f;   // the THIN, wide-ball end
	float m_fHeelZ         = 0.0f;   // the TALL, narrow end
	bool  m_bFootFacingMeasured = false;
	// +1 when the toes point +Z (the engine's forward, and StickFigure's own shoe
	// convention: its heel ring is at z -0.075 and its toe ring at +0.150).
	float m_fFacingSign = 0.0f;

	float SoleY()  const { return m_afBodyY[ZENITH_HUMAN_BODY_SOLE]; }
	float CrownY() const { return m_afBodyY[ZENITH_HUMAN_BODY_CROWN]; }
	float Height() const { return CrownY() - SoleY(); }

	// As a fraction of height, 0 at the sole. The form every log line and every
	// pinned test uses, because it is the only form comparable across meshes.
	float Frac(float fY) const { const float fH = Height(); return (fH > 1.0e-6f) ? ((fY - SoleY()) / fH) : 0.0f; }
};

// Scan xView for the landmarks above. Bounded searches only: every scan states
// the band it looks in, so an unbounded "minimum in the outer 40%" cannot come
// back with a fingertip when it was asked for a wrist. Any degenerate scan
// leaves that anchor un-found; a mesh with no height at all comes back invalid.
bool Zenith_MeasureHumanLandmarks(const Zenith_SkinDeformView& xView, ZENITH_HUMAN_POSE ePose,
	Zenith_HumanLandmarks& xOut);

// Log every landmark as a fraction of height plus the raw Y. This is the line the
// verification pass reads to set RenderTest's k_fAnkleHeight -- read it, never
// guess it.
void Zenith_LogHumanLandmarks(const char* szWho, const Zenith_HumanLandmarks& xLandmarks);

//-----------------------------------------------------------------------------
// The proportion warp: a piecewise-linear landmark map plus a lateral arm shift.
//
// No skeleton is needed to build one, so the whole thing is unit-testable with no
// asset on disk.
//-----------------------------------------------------------------------------
struct Zenith_HumanWarp
{
	// Body chain, MESH space, ascending. Compacted -- a landmark the mesh does not
	// have is absent from BOTH arrays rather than being invented.
	float m_afBodySrcY[ZENITH_HUMAN_BODY_ANCHOR_COUNT] = {};
	float m_afBodyDstY[ZENITH_HUMAN_BODY_ANCHOR_COUNT] = {};
	u_int m_uNumBodyAnchors = 0u;

	// Arm chain, MESH space, ascending. The arm needs its OWN chain because in the
	// arms-down bind pose it runs vertically through the body's Y range, so a
	// single body map would drag the elbow and wrist along the chest and waist
	// mapping instead of along the arm's.
	float m_afArmSrcY[ZENITH_HUMAN_ARM_ANCHOR_COUNT] = {};
	float m_afArmDstY[ZENITH_HUMAN_ARM_ANCHOR_COUNT] = {};
	u_int m_uNumArmAnchors = 0u;

	float m_fLateralShiftX = 0.0f;   // added to |x| at arm weight 1

	// Which bones count as "arm". The two chains meet at the shoulder and blend by
	// the vertex's own weight on these, so the blend rides the loft's existing
	// deltoid weighting and needs no new tuning.
	u_int64 m_ulArmBoneMask = 0ull;

	bool IsMonotonic() const;
	bool IsValid() const { return m_uNumBodyAnchors >= 2u && IsMonotonic(); }

	// Where one Y goes, at a given arm weight. The scalar core of both forms.
	float MapY(float fY, float fArmWeight) const;
	// How far this vertex's weights say it is "arm".
	float ArmWeight(const glm::uvec4& xIndices, const glm::vec4& xWeights) const;
};

// StickFigure's arm chain (LeftUpperArm..RightHand = 4..9) plus its thirty finger
// bones (21..50). The fingers are IN because hands and digits have to travel with
// the wrist -- the arm chain is what re-proportions them.
inline constexpr u_int64 ulZENITH_STICKFIGURE_FINGER_BONE_MASK = ((1ull << 30) - 1ull) << 21;
inline constexpr u_int64 ulZENITH_STICKFIGURE_ARM_BONE_MASK =
	(0x3Full << 4) | ulZENITH_STICKFIGURE_FINGER_BONE_MASK;

// ★ EVERYTHING DISTAL OF ONE JOINT, which is how an arms-down mesh's joints are
// LOCATED rather than guessed at. A loft already declares where its elbow is --
// it is the ring whose weights are 50/50 across the elbow -- so the height at
// which the distal weight crosses 0.5 IS the joint, exactly, with no band to
// tune and no radius feature to hope for. On StickFigure it recovers the
// authored shoulder to 0.5 cm and the authored elbow to 1.5 cm, which is the
// independent check that the reading means what it claims.
inline constexpr u_int64 ulZENITH_STICKFIGURE_BELOW_ELBOW_MASK =
	((1ull << 5) | (1ull << 6) | (1ull << 8) | (1ull << 9)) | ulZENITH_STICKFIGURE_FINGER_BONE_MASK;
inline constexpr u_int64 ulZENITH_STICKFIGURE_BELOW_WRIST_MASK =
	((1ull << 6) | (1ull << 9)) | ulZENITH_STICKFIGURE_FINGER_BONE_MASK;

// Build the map that takes xMeasured's landmarks onto xTo's rig planes.
//
// ★ THE TARGETS ARE THE RIG'S PLANES, NOT THIS MESH'S FRACTIONS. A mesh's joints
// have to land where the SHARED skeleton's bones are, or it bends in the wrong
// place; two meshes with slightly different heights re-proportioned to their own
// fractions would put their knees 4.6 cm apart on one rig. The two ENDPOINTS are
// the exception and are pinned to this mesh's own sole and crown, so height,
// bounds and every constant tuned against them survive untouched.
bool Zenith_MakeHumanWarp(const Zenith_HumanLandmarks& xMeasured,
	const Zenith_HumanProportions& xTo, Zenith_HumanWarp& xOut);

// Vertex form -- ONLY for meshes that rebuild normals afterwards.
bool Zenith_SkinWarpVertices(const Zenith_SkinDeformView& xView, const Zenith_HumanWarp& xWarp);

// Ring form -- for lofts whose normals are emitted analytically and never
// regenerated. Warp the rings, and the analytic normals come out right for free.
bool Zenith_SkinWarpRing(float& fY, float& fCx, float fArmWeight, const Zenith_HumanWarp& xWarp);

