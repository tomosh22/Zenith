#include "Zenith.h"

//=============================================================================
// Zenith_Tools_TestAssetExport
//
// StickFigure human test asset generation. Regenerated at every editor boot
// (called from GenerateTestAssets, same gate as the procedural tree):
//
//   ENGINE_ASSETS_DIR/Meshes/StickFigure/
//     StickFigure.zskel             — 16-bone humanoid rig (UNCHANGED layout:
//                                     bind positions are load-bearing for the
//                                     RenderTest foot-IK capsule sizing and
//                                     the engine skeleton unit tests)
//     StickFigure.zasset/.zmesh     — smooth lofted human body (~1.7k verts,
//                                     multi-bone blended skinning at joints)
//     StickFigure_Static.zmesh      — unskinned copy
//     StickFigure_Albedo.ztxtr      — 1024^2 atlas: painted face (eyes, brows,
//                                     lips, hair), T-shirt, trousers, boots
//     StickFigure_Normal.ztxtr      — normals from a procedural height field
//                                     (fabric weave, seams, pores, laces)
//     StickFigure_RM.ztxtr          — G=roughness / B=metallic (glTF packing,
//                                     matches SampleRoughnessMetallic)
//     StickFigure_Body.zmtrl        — material wiring the three textures
//     StickFigure.zmodel            — mesh + skeleton + material bundle (the
//                                     games' create-if-missing fallbacks are
//                                     superseded by this canonical export)
//     StickFigure_<Anim>.zanim      — 13 clips (Idle/Walk/Run/Attack1-3/Dodge/
//                                     Hit/Death/Aim/Fire/Reload/Jump), authored
//                                     from gait curves + eased key poses.
//                                     Names/durations/looping are pinned by
//                                     unit tests and the Combat hit windows.
//     StickFigure.gltf              — Blender round-trip export
//
// Everything is deterministic — repeated boots regenerate byte-identical
// assets. The Aim/Fire/Reload clips share StickFigureAimHoldPose so state
// transitions between them never snap (pinned by Zenith_Tools_TestAssetExport
// .Tests.inl).
//=============================================================================

#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "DataStream/Zenith_DataStream.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/InstancedMeshes/Flux_AnimationTexture.h"
#include "Zenith_Tools_GltfExport.h"
#include "Zenith_Tools_TestAssetExport.h"
#include "Zenith_Tools_TextureExport.h"   // v2 BC / offline-mip texture export
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <unordered_map>

//------------------------------------------------------------------------------
// Bone indices for stick figure skeleton
//------------------------------------------------------------------------------
static constexpr uint32_t STICK_BONE_ROOT = 0;
static constexpr uint32_t STICK_BONE_SPINE = 1;
static constexpr uint32_t STICK_BONE_NECK = 2;
static constexpr uint32_t STICK_BONE_HEAD = 3;
static constexpr uint32_t STICK_BONE_LEFT_UPPER_ARM = 4;
static constexpr uint32_t STICK_BONE_LEFT_LOWER_ARM = 5;
static constexpr uint32_t STICK_BONE_LEFT_HAND = 6;
static constexpr uint32_t STICK_BONE_RIGHT_UPPER_ARM = 7;
static constexpr uint32_t STICK_BONE_RIGHT_LOWER_ARM = 8;
static constexpr uint32_t STICK_BONE_RIGHT_HAND = 9;
static constexpr uint32_t STICK_BONE_LEFT_UPPER_LEG = 10;
static constexpr uint32_t STICK_BONE_LEFT_LOWER_LEG = 11;
static constexpr uint32_t STICK_BONE_LEFT_FOOT = 12;
static constexpr uint32_t STICK_BONE_RIGHT_UPPER_LEG = 13;
static constexpr uint32_t STICK_BONE_RIGHT_LOWER_LEG = 14;
static constexpr uint32_t STICK_BONE_RIGHT_FOOT = 15;
// UE5-class additions (appended so the original 16 keep their indices/names —
// the authored clips, foot-IK and unit tests reference those by name/index).
static constexpr uint32_t STICK_BONE_JAW = 16;
static constexpr uint32_t STICK_BONE_LEFT_EYE = 17;
static constexpr uint32_t STICK_BONE_RIGHT_EYE = 18;
static constexpr uint32_t STICK_BONE_LEFT_TOE = 19;
static constexpr uint32_t STICK_BONE_RIGHT_TOE = 20;
// Finger bones (3 per digit: proximal/middle/distal), thumb + 4 fingers per hand.
static constexpr uint32_t STICK_BONE_FINGERS_BEGIN = 21;
static constexpr uint32_t STICK_BONE_FINGERS_PER_HAND = 15;   // 5 digits x 3
static constexpr uint32_t STICK_BONE_LEFT_FINGERS_BEGIN = STICK_BONE_FINGERS_BEGIN;                              // 21..35
static constexpr uint32_t STICK_BONE_RIGHT_FINGERS_BEGIN = STICK_BONE_FINGERS_BEGIN + STICK_BONE_FINGERS_PER_HAND; // 36..50
static constexpr uint32_t STICK_BONE_COUNT = STICK_BONE_FINGERS_BEGIN + 2 * STICK_BONE_FINGERS_PER_HAND;          // 51

// Shared finger layout (world-space at bind), used by BOTH the skeleton bind
// pose and the hand mesh so the finger bones and the finger geometry stay in
// lockstep. 5 digits per hand: index, middle, ring, pinky, thumb.
struct HumanDigitDef { Zenith_Maths::Vector3 xRoot; Zenith_Maths::Vector3 xTip; float fRootR; };
static void GetHumanHandDigits(float fSide, HumanDigitDef axOut[5])
{
	const float fCx = fSide * 0.302f;
	const float fBaseZ = 0.006f;
	const float afZ[4]    = { fBaseZ - 0.031f, fBaseZ - 0.010f, fBaseZ + 0.011f, fBaseZ + 0.032f };
	const float afTipY[4] = { 0.226f, 0.205f, 0.210f, 0.230f };
	const float afR[4]    = { 0.0092f, 0.0100f, 0.0098f, 0.0084f };
	for (int i = 0; i < 4; i++)
	{
		axOut[i].xRoot  = Zenith_Maths::Vector3(fCx, 0.318f, afZ[i]);
		axOut[i].xTip   = Zenith_Maths::Vector3(fCx, afTipY[i], afZ[i]);
		axOut[i].fRootR = afR[i];
	}
	axOut[4].xRoot  = Zenith_Maths::Vector3(fCx - fSide * 0.006f, 0.356f, fBaseZ + 0.038f);
	axOut[4].xTip   = Zenith_Maths::Vector3(fCx - fSide * 0.020f, 0.322f, fBaseZ + 0.066f);
	axOut[4].fRootR = 0.0120f;
}

// Finger bone index for hand (0=left,1=right), digit (0..4), phalanx (0..2).
static u_int HumanFingerBone(int iHand, int iDigit, int iPhalanx)
{
	const u_int uBase = (iHand == 0) ? STICK_BONE_LEFT_FINGERS_BEGIN : STICK_BONE_RIGHT_FINGERS_BEGIN;
	return uBase + static_cast<u_int>(iDigit) * 3u + static_cast<u_int>(iPhalanx);
}

//------------------------------------------------------------------------------
// Skeleton. The first 16 bones (indices 0-15, names unchanged) are a frozen
// contract: RenderTest's capsule/foot-IK assume feet at Y=-1.0, the engine unit
// tests pin head Y=1.4 and the parent hierarchy, and the 13 authored clips bind
// to these names. UE5-class bones (jaw, eyes, toes, articulated fingers) are
// APPENDED after them so all of that keeps working.
//------------------------------------------------------------------------------
static Zenith_SkeletonAsset* CreateStickFigureSkeleton()
{
	Zenith_SkeletonAsset* pxSkel = new Zenith_SkeletonAsset();
	const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
	const Zenith_Maths::Vector3 xUnitScale(1.0f);

	// Root (at origin)
	pxSkel->AddBone("Root", -1, Zenith_Maths::Vector3(0, 0, 0), xIdentity, xUnitScale);

	// Spine (up from root)
	pxSkel->AddBone("Spine", STICK_BONE_ROOT, Zenith_Maths::Vector3(0, 0.5f, 0), xIdentity, xUnitScale);

	// Neck (up from spine)
	pxSkel->AddBone("Neck", STICK_BONE_SPINE, Zenith_Maths::Vector3(0, 0.7f, 0), xIdentity, xUnitScale);

	// Head (up from neck)
	pxSkel->AddBone("Head", STICK_BONE_NECK, Zenith_Maths::Vector3(0, 0.2f, 0), xIdentity, xUnitScale);

	// Left arm chain
	pxSkel->AddBone("LeftUpperArm", STICK_BONE_SPINE, Zenith_Maths::Vector3(-0.3f, 0.6f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("LeftLowerArm", STICK_BONE_LEFT_UPPER_ARM, Zenith_Maths::Vector3(0, -0.4f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("LeftHand", STICK_BONE_LEFT_LOWER_ARM, Zenith_Maths::Vector3(0, -0.3f, 0), xIdentity, xUnitScale);

	// Right arm chain
	pxSkel->AddBone("RightUpperArm", STICK_BONE_SPINE, Zenith_Maths::Vector3(0.3f, 0.6f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("RightLowerArm", STICK_BONE_RIGHT_UPPER_ARM, Zenith_Maths::Vector3(0, -0.4f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("RightHand", STICK_BONE_RIGHT_LOWER_ARM, Zenith_Maths::Vector3(0, -0.3f, 0), xIdentity, xUnitScale);

	// Left leg chain
	pxSkel->AddBone("LeftUpperLeg", STICK_BONE_ROOT, Zenith_Maths::Vector3(-0.15f, 0, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("LeftLowerLeg", STICK_BONE_LEFT_UPPER_LEG, Zenith_Maths::Vector3(0, -0.5f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("LeftFoot", STICK_BONE_LEFT_LOWER_LEG, Zenith_Maths::Vector3(0, -0.5f, 0), xIdentity, xUnitScale);

	// Right leg chain
	pxSkel->AddBone("RightUpperLeg", STICK_BONE_ROOT, Zenith_Maths::Vector3(0.15f, 0, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("RightLowerLeg", STICK_BONE_RIGHT_UPPER_LEG, Zenith_Maths::Vector3(0, -0.5f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("RightFoot", STICK_BONE_RIGHT_LOWER_LEG, Zenith_Maths::Vector3(0, -0.5f, 0), xIdentity, xUnitScale);

	// --- UE5-class additions (indices 16+; the 16 core bones above are unchanged) ---
	// Jaw + eyes hang off the head (world: head at (0,1.4,0)); toes off the feet.
	pxSkel->AddBone("Jaw",      STICK_BONE_HEAD,       Zenith_Maths::Vector3(0.0f, -0.050f, -0.020f), xIdentity, xUnitScale);
	pxSkel->AddBone("LeftEye",  STICK_BONE_HEAD,       Zenith_Maths::Vector3(-0.034f, 0.029f, 0.060f), xIdentity, xUnitScale);
	pxSkel->AddBone("RightEye", STICK_BONE_HEAD,       Zenith_Maths::Vector3(0.034f, 0.029f, 0.060f), xIdentity, xUnitScale);
	pxSkel->AddBone("LeftToe",  STICK_BONE_LEFT_FOOT,  Zenith_Maths::Vector3(0.0f, -0.010f, 0.100f), xIdentity, xUnitScale);
	pxSkel->AddBone("RightToe", STICK_BONE_RIGHT_FOOT, Zenith_Maths::Vector3(0.0f, -0.010f, 0.100f), xIdentity, xUnitScale);

	// Articulated fingers: 5 digits x 3 phalanges per hand, chained off the hand.
	const char* aszDigit[5] = { "Index", "Middle", "Ring", "Pinky", "Thumb" };
	for (int iHand = 0; iHand < 2; iHand++)
	{
		const float fSide = (iHand == 0) ? -1.0f : 1.0f;
		const u_int uHandBone = (iHand == 0) ? STICK_BONE_LEFT_HAND : STICK_BONE_RIGHT_HAND;
		const Zenith_Maths::Vector3 xHandWorld(fSide * 0.3f, 0.4f, 0.0f);   // hand bone bind-pose world position
		const std::string strPrefix = (iHand == 0) ? "L_" : "R_";
		HumanDigitDef axDigits[5];
		GetHumanHandDigits(fSide, axDigits);
		for (int d = 0; d < 5; d++)
		{
			const Zenith_Maths::Vector3& xR = axDigits[d].xRoot;
			const Zenith_Maths::Vector3& xT = axDigits[d].xTip;
			const Zenith_Maths::Vector3 xJ0 = xR;
			const Zenith_Maths::Vector3 xJ1 = xR + (xT - xR) * 0.40f;
			const Zenith_Maths::Vector3 xJ2 = xR + (xT - xR) * 0.70f;
			const std::string strBase = strPrefix + aszDigit[d];
			const uint32_t uProx = pxSkel->AddBone((strBase + "_01").c_str(), uHandBone, xJ0 - xHandWorld, xIdentity, xUnitScale);
			const uint32_t uMid  = pxSkel->AddBone((strBase + "_02").c_str(), uProx, xJ1 - xJ0, xIdentity, xUnitScale);
			pxSkel->AddBone((strBase + "_03").c_str(), uMid, xJ2 - xJ1, xIdentity, xUnitScale);
		}
	}

	pxSkel->ComputeBindPoseMatrices();
	return pxSkel;
}

//==============================================================================
// Human body construction
//
// The body is built from vertical lofts (stacked elliptical rings) for the
// torso, head+neck, arms, hands and legs, plus a Z-aligned superellipse loft
// per shoe. Rings carry up-to-two-bone blended skin weights so the mesh bends
// smoothly at every joint instead of the old rigid cube-per-bone look.
// Normals are recomputed smooth (position-welded) after assembly and the UV
// atlas drives GenerateTangents for normal mapping.
//==============================================================================
namespace
{

constexpr float fHUMAN_PI = 3.14159265358979f;
constexpr float fHUMAN_TWO_PI = 6.28318530717959f;

float HumanSmoothStep(float fEdge0, float fEdge1, float fX)
{
	const float fT = std::clamp((fX - fEdge0) / (fEdge1 - fEdge0), 0.0f, 1.0f);
	return fT * fT * (3.0f - 2.0f * fT);
}

// Deterministic 2D value noise (integer-hash based) for the texture painters.
float HumanHashToFloat(u_int uX, u_int uY, u_int uSeed)
{
	u_int h = uX * 374761393u + uY * 668265263u + uSeed * 2246822519u;
	h = (h ^ (h >> 13)) * 1274126177u;
	return static_cast<float>((h ^ (h >> 16)) & 0xFFFFFFu) / 16777215.0f;
}

float HumanValueNoise2D(float fX, float fY, u_int uSeed)
{
	const float fXF = std::floor(fX);
	const float fYF = std::floor(fY);
	const u_int uX0 = static_cast<u_int>(static_cast<int>(fXF) & 0xFFFF);
	const u_int uY0 = static_cast<u_int>(static_cast<int>(fYF) & 0xFFFF);
	const float fTX = HumanSmoothStep(0.0f, 1.0f, fX - fXF);
	const float fTY = HumanSmoothStep(0.0f, 1.0f, fY - fYF);
	const float fA = HumanHashToFloat(uX0, uY0, uSeed);
	const float fB = HumanHashToFloat(uX0 + 1, uY0, uSeed);
	const float fC = HumanHashToFloat(uX0, uY0 + 1, uSeed);
	const float fD = HumanHashToFloat(uX0 + 1, uY0 + 1, uSeed);
	return (fA * (1 - fTX) + fB * fTX) * (1 - fTY) + (fC * (1 - fTX) + fD * fTX) * fTY;
}

float HumanFBM2D(float fX, float fY, u_int uOctaves, u_int uSeed)
{
	float fSum = 0.0f;
	float fAmp = 0.5f;
	float fFreq = 1.0f;
	for (u_int u = 0; u < uOctaves; u++)
	{
		fSum += HumanValueNoise2D(fX * fFreq, fY * fFreq, uSeed + u * 101u) * fAmp;
		fFreq *= 2.0f;
		fAmp *= 0.5f;
	}
	return fSum;
}

// Gaussian-ish radial falloff used for painted face features.
float HumanGauss(float fDX, float fDY, float fRX, float fRY)
{
	const float fX = fDX / fRX;
	const float fY = fDY / fRY;
	return expf(-(fX * fX + fY * fY));
}

//------------------------------------------------------------------------------
// UV atlas islands. The head gets a disproportionately large island so the
// painted face carries real detail; cloth needs far less texel density.
// Coordinates are normalized [0,1] across the 1024^2 atlas, with >=8px gutters
// (the post-paint dilation fills gutters so bilinear sampling never bleeds
// neutral pixels into an island edge).
//------------------------------------------------------------------------------
struct HumanUVIsland
{
	float fU0, fV0, fU1, fV1;

	float U(float fUN) const { return fU0 + (fU1 - fU0) * fUN; }
	float V(float fVN) const { return fV0 + (fV1 - fV0) * fVN; }
};

constexpr HumanUVIsland xUV_HEAD   { 0.005f, 0.005f, 0.325f, 0.420f };
constexpr HumanUVIsland xUV_TORSO  { 0.335f, 0.005f, 0.660f, 0.420f };
constexpr HumanUVIsland xUV_ARM_L  { 0.670f, 0.005f, 0.825f, 0.420f };
constexpr HumanUVIsland xUV_ARM_R  { 0.835f, 0.005f, 0.990f, 0.420f };
constexpr HumanUVIsland xUV_LEG_L  { 0.005f, 0.430f, 0.230f, 0.900f };
constexpr HumanUVIsland xUV_LEG_R  { 0.240f, 0.430f, 0.465f, 0.900f };
constexpr HumanUVIsland xUV_FOOT_L { 0.475f, 0.430f, 0.700f, 0.620f };
constexpr HumanUVIsland xUV_FOOT_R { 0.475f, 0.630f, 0.700f, 0.820f };
constexpr HumanUVIsland xUV_HAND_L { 0.710f, 0.430f, 0.840f, 0.560f };
constexpr HumanUVIsland xUV_HAND_R { 0.850f, 0.430f, 0.980f, 0.560f };

//------------------------------------------------------------------------------
// Vertical loft machinery. A ring is a horizontal ellipse; consecutive rings
// are stitched into quads. Ring vertices duplicate the first column for the
// UV seam (placed at the BACK of each part, so the seam hides from the front).
// Each ring is skinned to at most two bones with a blend weight — that is what
// makes elbows/knees/hips/shoulders deform smoothly.
//------------------------------------------------------------------------------
struct HumanRing
{
	float fY;            // ring plane height (model space)
	float fCx, fCz;      // ring centre
	float fRx, fRz;      // ellipse half-extents
	u_int uBoneA;
	u_int uBoneB;        // == uBoneA when rigid
	float fBlendB;       // weight of bone B (0..1)
};

// Vertex angle convention: ang=0 faces BACK (-Z), ang=pi faces FRONT (+Z).
// uNorm runs 0..1 around the ring, so the painted island's centre column
// (uNorm 0.5) is the front of the body part.
uint32_t AddHumanRing(Zenith_MeshAsset* pxMesh, const HumanRing& xRing, u_int uSegs,
                      const HumanUVIsland& xIsland, float fVNorm)
{
	const uint32_t uFirst = pxMesh->GetNumVerts();
	for (u_int uSeg = 0; uSeg <= uSegs; uSeg++)
	{
		const float fAng = fHUMAN_TWO_PI * static_cast<float>(uSeg) / static_cast<float>(uSegs);
		const float fSin = sinf(fAng);
		const float fCos = cosf(fAng);
		const Zenith_Maths::Vector3 xPos(
			xRing.fCx + xRing.fRx * fSin,
			xRing.fY,
			xRing.fCz - xRing.fRz * fCos);
		// Placeholder normal — recomputed smooth (position-welded) after assembly.
		const Zenith_Maths::Vector3 xNormal = glm::normalize(Zenith_Maths::Vector3(fSin, 0.0f, -fCos));
		const Zenith_Maths::Vector2 xUV(
			xIsland.U(static_cast<float>(uSeg) / static_cast<float>(uSegs)),
			xIsland.V(fVNorm));

		pxMesh->AddVertex(xPos, xNormal, xUV);
		const float fB = std::clamp(xRing.fBlendB, 0.0f, 1.0f);
		pxMesh->SetVertexSkinning(pxMesh->GetNumVerts() - 1,
			glm::uvec4(xRing.uBoneA, xRing.uBoneB, 0, 0),
			glm::vec4(1.0f - fB, fB, 0.0f, 0.0f));
	}
	return uFirst;
}

// Stitch two rings of (uSegs+1) verts. Winding follows the engine convention
// proven by the original cube figure and the tree trunk: the geometric normal
// cross(v1-v0, v2-v0) points OUTWARD. bFlip reverses it (used by the Z-aligned
// shoe lofts whose advance axis flips the orientation).
void StitchHumanRings(Zenith_MeshAsset* pxMesh, uint32_t uRingA, uint32_t uRingB, u_int uSegs, bool bFlip = false)
{
	for (u_int uSeg = 0; uSeg < uSegs; uSeg++)
	{
		const uint32_t uA = uRingA + uSeg;
		const uint32_t uA1 = uA + 1;
		const uint32_t uB = uRingB + uSeg;
		const uint32_t uB1 = uB + 1;
		if (!bFlip)
		{
			pxMesh->AddTriangle(uA, uB, uA1);
			pxMesh->AddTriangle(uA1, uB, uB1);
		}
		else
		{
			pxMesh->AddTriangle(uA, uA1, uB);
			pxMesh->AddTriangle(uA1, uB1, uB);
		}
	}
}

// Cap a ring with a triangle fan to a centre vertex. bUpward selects the
// winding for a cap whose outward direction is +Y (top) vs -Y (bottom);
// Z-loft callers pass the flag for +Z (toe) vs -Z (heel) analogously.
void CapHumanRing(Zenith_MeshAsset* pxMesh, uint32_t uRing, u_int uSegs,
                  const Zenith_Maths::Vector3& xCentre, const Zenith_Maths::Vector3& xCentreNormal,
                  const HumanUVIsland& xIsland, float fUNorm, float fVNorm,
                  u_int uBoneA, u_int uBoneB, float fBlendB, bool bUpward)
{
	const uint32_t uCentre = pxMesh->GetNumVerts();
	pxMesh->AddVertex(xCentre, xCentreNormal, Zenith_Maths::Vector2(xIsland.U(fUNorm), xIsland.V(fVNorm)));
	const float fB = std::clamp(fBlendB, 0.0f, 1.0f);
	pxMesh->SetVertexSkinning(uCentre,
		glm::uvec4(uBoneA, uBoneB, 0, 0),
		glm::vec4(1.0f - fB, fB, 0.0f, 0.0f));

	for (u_int uSeg = 0; uSeg < uSegs; uSeg++)
	{
		if (bUpward)
		{
			pxMesh->AddTriangle(uRing + uSeg, uCentre, uRing + uSeg + 1);
		}
		else
		{
			pxMesh->AddTriangle(uRing + uSeg, uRing + uSeg + 1, uCentre);
		}
	}
}

// High-poly knobs. uHUMAN_RING_SUBDIV inserts (N-1) Catmull-Rom interpolated
// rings between each authored ring pair (1 = authored rings only); the per-part
// uSEGS constants set the around-resolution. Together they take the body from
// the original ~1.7k verts to a smooth, AAA-grade ~10k. The 16-bone rig, bind
// pose and UV islands are untouched — only tessellation changes.
constexpr u_int uHUMAN_RING_SUBDIV = 4;

// Uniform Catmull-Rom through p1..p2 (neighbours p0, p3), fT in [0,1]. fT==0
// returns p1 exactly, so authored rings survive subdivision byte-for-byte.
float HumanCatmullRom(float fP0, float fP1, float fP2, float fP3, float fT)
{
	const float fT2 = fT * fT;
	const float fT3 = fT2 * fT;
	return 0.5f * ((2.0f * fP1)
		+ (-fP0 + fP2) * fT
		+ (2.0f * fP0 - 5.0f * fP1 + 4.0f * fP2 - fP3) * fT2
		+ (-fP0 + 3.0f * fP1 - 3.0f * fP2 + fP3) * fT3);
}

// Skin weight for a ring interpolated between rings A and B at parameter fT.
// Each authored ring carries at most two bone influences; we lerp in per-bone
// weight space and keep the two heaviest, renormalized to sum to 1 (matching the
// <=2-influence authored data the rig + unit tests expect). For this rig no A/B
// pair ever spans more than two distinct bones, so this is exact, never lossy.
// At fT==0 it reproduces ring A's skinning exactly (bones may be reordered, which
// is deformation-identical — skinning is a weighted sum, order-independent).
void LerpHumanRingSkin(const HumanRing& xA, const HumanRing& xB, float fT,
                       u_int& uOutBoneA, u_int& uOutBoneB, float& fOutBlendB)
{
	u_int auBone[4];
	float afWeight[4];
	int iCount = 0;
	auto Accumulate = [&](u_int uBone, float fW)
	{
		if (fW <= 0.0f) { return; }
		for (int i = 0; i < iCount; i++)
		{
			if (auBone[i] == uBone) { afWeight[i] += fW; return; }
		}
		auBone[iCount] = uBone;
		afWeight[iCount] = fW;
		iCount++;
	};
	Accumulate(xA.uBoneA, (1.0f - xA.fBlendB) * (1.0f - fT));
	Accumulate(xA.uBoneB, xA.fBlendB * (1.0f - fT));
	Accumulate(xB.uBoneA, (1.0f - xB.fBlendB) * fT);
	Accumulate(xB.uBoneB, xB.fBlendB * fT);

	if (iCount == 0)
	{
		uOutBoneA = uOutBoneB = xA.uBoneA;
		fOutBlendB = 0.0f;
		return;
	}

	int iTop0 = 0;
	for (int i = 1; i < iCount; i++) { if (afWeight[i] > afWeight[iTop0]) { iTop0 = i; } }
	int iTop1 = -1;
	for (int i = 0; i < iCount; i++)
	{
		if (i == iTop0) { continue; }
		if (iTop1 < 0 || afWeight[i] > afWeight[iTop1]) { iTop1 = i; }
	}

	const float fW0 = afWeight[iTop0];
	const float fW1 = (iTop1 >= 0) ? afWeight[iTop1] : 0.0f;
	const float fSum = fW0 + fW1;
	uOutBoneA = auBone[iTop0];
	uOutBoneB = (iTop1 >= 0) ? auBone[iTop1] : auBone[iTop0];
	fOutBlendB = (fSum > 1e-6f) ? (fW1 / fSum) : 0.0f;
}

// Expand authored rings into a denser list: for each gap emit the authored ring
// plus (uSub-1) Catmull-Rom interpolated rings (geometry smooth, skinning lerped
// per-bone). uSub<=1 returns a verbatim copy. Result count = (N-1)*uSub + 1.
Zenith_Vector<HumanRing> SubdivideHumanRings(const HumanRing* pxRings, u_int uNumRings, u_int uSub)
{
	Zenith_Vector<HumanRing> xOut;
	if (uNumRings == 0) { return xOut; }
	if (uSub <= 1 || uNumRings < 2)
	{
		for (u_int u = 0; u < uNumRings; u++) { xOut.PushBack(pxRings[u]); }
		return xOut;
	}
	auto At = [&](int i) -> const HumanRing& { return pxRings[std::clamp(i, 0, static_cast<int>(uNumRings) - 1)]; };
	for (u_int u = 0; u + 1 < uNumRings; u++)
	{
		const HumanRing& xP0 = At(static_cast<int>(u) - 1);
		const HumanRing& xP1 = pxRings[u];
		const HumanRing& xP2 = pxRings[u + 1];
		const HumanRing& xP3 = At(static_cast<int>(u) + 2);
		for (u_int s = 0; s < uSub; s++)
		{
			const float fT = static_cast<float>(s) / static_cast<float>(uSub);
			HumanRing xR;
			xR.fY  = HumanCatmullRom(xP0.fY,  xP1.fY,  xP2.fY,  xP3.fY,  fT);
			xR.fCx = HumanCatmullRom(xP0.fCx, xP1.fCx, xP2.fCx, xP3.fCx, fT);
			xR.fCz = HumanCatmullRom(xP0.fCz, xP1.fCz, xP2.fCz, xP3.fCz, fT);
			xR.fRx = std::max(0.0f, HumanCatmullRom(xP0.fRx, xP1.fRx, xP2.fRx, xP3.fRx, fT));
			xR.fRz = std::max(0.0f, HumanCatmullRom(xP0.fRz, xP1.fRz, xP2.fRz, xP3.fRz, fT));
			LerpHumanRingSkin(xP1, xP2, fT, xR.uBoneA, xR.uBoneB, xR.fBlendB);
			xOut.PushBack(xR);
		}
	}
	xOut.PushBack(pxRings[uNumRings - 1]);
	return xOut;
}

// Loft a full part: rings stitched in order, with V coordinates spread by
// cumulative vertical distance so the painted texture doesn't stretch.
// Returns the first ring's first vertex index.
uint32_t AddHumanLoft(Zenith_MeshAsset* pxMesh, const HumanRing* pxRings, u_int uNumRings,
                      u_int uSegs, const HumanUVIsland& xIsland)
{
	// Densify with smooth interpolated rings so limbs read high-poly, not faceted.
	const Zenith_Vector<HumanRing> xRings = SubdivideHumanRings(pxRings, uNumRings, uHUMAN_RING_SUBDIV);
	const u_int uDense = xRings.GetSize();

	float fTotal = 0.0f;
	for (u_int u = 1; u < uDense; u++)
	{
		fTotal += fabsf(xRings.Get(u).fY - xRings.Get(u - 1).fY);
	}
	fTotal = std::max(fTotal, 0.0001f);

	uint32_t uFirst = 0;
	uint32_t uPrev = 0;
	float fAccum = 0.0f;
	for (u_int u = 0; u < uDense; u++)
	{
		if (u > 0)
		{
			fAccum += fabsf(xRings.Get(u).fY - xRings.Get(u - 1).fY);
		}
		const uint32_t uRing = AddHumanRing(pxMesh, xRings.Get(u), uSegs, xIsland, fAccum / fTotal);
		if (u == 0)
		{
			uFirst = uRing;
		}
		else
		{
			StitchHumanRings(pxMesh, uPrev, uRing, uSegs);
		}
		uPrev = uRing;
	}
	return uFirst;
}

//------------------------------------------------------------------------------
// Shoe loft — rings perpendicular to Z (heel to toe), superellipse profile for
// a boxier silhouette. uNorm 0 is the SOLE centre, 0.5 the top of the foot.
//------------------------------------------------------------------------------
struct HumanShoeRing
{
	float fZ;            // ring plane depth (relative to the foot pivot)
	float fCy;           // ring centre height (model space)
	float fWx, fHy;      // half width / half height
};

uint32_t AddHumanShoeRing(Zenith_MeshAsset* pxMesh, float fSide, const HumanShoeRing& xRing,
                          u_int uSegs, const HumanUVIsland& xIsland, float fVNorm, u_int uBone)
{
	const uint32_t uFirst = pxMesh->GetNumVerts();
	const float fCx = fSide * 0.15f;
	for (u_int uSeg = 0; uSeg <= uSegs; uSeg++)
	{
		const float fAng = fHUMAN_TWO_PI * static_cast<float>(uSeg) / static_cast<float>(uSegs);
		// Superellipse: pulls the profile toward a rounded box (shoe-like).
		const float fS = sinf(fAng);
		const float fC = cosf(fAng);
		const float fSx = (fS >= 0.0f ? 1.0f : -1.0f) * powf(fabsf(fS), 0.72f);
		const float fSy = (fC >= 0.0f ? 1.0f : -1.0f) * powf(fabsf(fC), 0.72f);
		const Zenith_Maths::Vector3 xPos(
			fCx + xRing.fWx * fSx,
			xRing.fCy - xRing.fHy * fSy,   // ang=0 -> sole
			xRing.fZ);
		const Zenith_Maths::Vector3 xNormal = glm::normalize(Zenith_Maths::Vector3(fSx, -fSy, 0.0f));
		const Zenith_Maths::Vector2 xUV(
			xIsland.U(static_cast<float>(uSeg) / static_cast<float>(uSegs)),
			xIsland.V(fVNorm));

		pxMesh->AddVertex(xPos, xNormal, xUV);
		pxMesh->SetVertexSkinning(pxMesh->GetNumVerts() - 1,
			glm::uvec4(uBone, uBone, 0, 0), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
	}
	return uFirst;
}

// Shoe-ring densifier (geometry-only; the shoe is rigidly skinned to one foot
// bone). Same Catmull-Rom scheme as SubdivideHumanRings.
Zenith_Vector<HumanShoeRing> SubdivideShoeRings(const HumanShoeRing* pxRings, u_int uNumRings, u_int uSub)
{
	Zenith_Vector<HumanShoeRing> xOut;
	if (uNumRings == 0) { return xOut; }
	if (uSub <= 1 || uNumRings < 2)
	{
		for (u_int u = 0; u < uNumRings; u++) { xOut.PushBack(pxRings[u]); }
		return xOut;
	}
	auto At = [&](int i) -> const HumanShoeRing& { return pxRings[std::clamp(i, 0, static_cast<int>(uNumRings) - 1)]; };
	for (u_int u = 0; u + 1 < uNumRings; u++)
	{
		const HumanShoeRing& xP0 = At(static_cast<int>(u) - 1);
		const HumanShoeRing& xP1 = pxRings[u];
		const HumanShoeRing& xP2 = pxRings[u + 1];
		const HumanShoeRing& xP3 = At(static_cast<int>(u) + 2);
		for (u_int s = 0; s < uSub; s++)
		{
			const float fT = static_cast<float>(s) / static_cast<float>(uSub);
			HumanShoeRing xR;
			xR.fZ  = HumanCatmullRom(xP0.fZ,  xP1.fZ,  xP2.fZ,  xP3.fZ,  fT);
			xR.fCy = HumanCatmullRom(xP0.fCy, xP1.fCy, xP2.fCy, xP3.fCy, fT);
			xR.fWx = std::max(0.0f, HumanCatmullRom(xP0.fWx, xP1.fWx, xP2.fWx, xP3.fWx, fT));
			xR.fHy = std::max(0.0f, HumanCatmullRom(xP0.fHy, xP1.fHy, xP2.fHy, xP3.fHy, fT));
			xOut.PushBack(xR);
		}
	}
	xOut.PushBack(pxRings[uNumRings - 1]);
	return xOut;
}

void BuildHumanShoe(Zenith_MeshAsset* pxMesh, float fSide, u_int uFootBone, const HumanUVIsland& xIsland)
{
	// Sole bottoms sit at ~-1.04, comfortably inside the RenderTest capsule's
	// 1.05 total half-extent; the foot pivot is at Y=-1.0.
	const HumanShoeRing axAuthored[] = {
		{ -0.075f, -0.984f, 0.040f, 0.050f },   // heel (rounded)
		{ -0.030f, -0.984f, 0.047f, 0.056f },
		{  0.030f, -0.990f, 0.051f, 0.050f },   // instep
		{  0.090f, -1.000f, 0.051f, 0.040f },
		{  0.150f, -1.013f, 0.044f, 0.027f },   // toe
	};
	constexpr u_int uNUM = sizeof(axAuthored) / sizeof(axAuthored[0]);
	constexpr u_int uSEGS = 28;

	const Zenith_Vector<HumanShoeRing> xRings = SubdivideShoeRings(axAuthored, uNUM, uHUMAN_RING_SUBDIV);
	const u_int uDense = xRings.GetSize();

	float fTotal = 0.0f;
	for (u_int u = 1; u < uDense; u++) { fTotal += fabsf(xRings.Get(u).fZ - xRings.Get(u - 1).fZ); }
	fTotal = std::max(fTotal, 0.0001f);

	uint32_t uPrev = 0;
	uint32_t uFirstRing = 0;
	float fAccum = 0.0f;
	for (u_int u = 0; u < uDense; u++)
	{
		if (u > 0) { fAccum += fabsf(xRings.Get(u).fZ - xRings.Get(u - 1).fZ); }
		const uint32_t uRing = AddHumanShoeRing(pxMesh, fSide, xRings.Get(u), uSEGS, xIsland, fAccum / fTotal, uFootBone);
		if (u == 0)
		{
			uFirstRing = uRing;
		}
		else
		{
			// Z-advance flips orientation relative to the vertical lofts.
			StitchHumanRings(pxMesh, uPrev, uRing, uSEGS, true);
		}
		uPrev = uRing;
	}

	const HumanShoeRing& xHeel = xRings.Get(0);
	const HumanShoeRing& xToe = xRings.Get(uDense - 1);
	// Heel cap (faces -Z) and toe cap (faces +Z).
	CapHumanRing(pxMesh, uFirstRing, uSEGS,
		Zenith_Maths::Vector3(fSide * 0.15f, xHeel.fCy, xHeel.fZ - 0.012f),
		Zenith_Maths::Vector3(0, 0, -1), xIsland, 0.5f, 0.0f, uFootBone, uFootBone, 0.0f, true);
	CapHumanRing(pxMesh, uPrev, uSEGS,
		Zenith_Maths::Vector3(fSide * 0.15f, xToe.fCy, xToe.fZ + 0.012f),
		Zenith_Maths::Vector3(0, 0, 1), xIsland, 0.5f, 1.0f, uFootBone, uFootBone, 0.0f, false);
}

//------------------------------------------------------------------------------
// Body part ring tables. All ordered TOP -> BOTTOM so vNorm 0 is the top of
// each painted island. Blend weights at joints are what the smooth-skinning
// upgrade is about: the elbow ring is 50/50 upper/lower arm, etc.
//------------------------------------------------------------------------------
void BuildHumanTorso(Zenith_MeshAsset* pxMesh)
{
	const u_int R = STICK_BONE_ROOT;
	const u_int S = STICK_BONE_SPINE;
	const HumanRing axRings[] = {
		//   y      cx     cz      rx      rz      boneA boneB blend
		{ 1.140f, 0.0f, -0.004f, 0.140f, 0.088f,  S, S, 0.0f },   // trap (wide enough for the deltoid to overlap)
		{ 1.060f, 0.0f, -0.006f, 0.235f, 0.112f,  S, S, 0.0f },   // shoulder line (broad)
		{ 0.950f, 0.0f, -0.005f, 0.212f, 0.124f,  S, S, 0.0f },   // upper chest (pecs)
		{ 0.800f, 0.0f,  0.002f, 0.188f, 0.124f,  S, S, 0.0f },   // chest
		{ 0.620f, 0.0f,  0.004f, 0.164f, 0.111f,  S, S, 0.0f },   // lower ribs
		{ 0.450f, 0.0f,  0.002f, 0.150f, 0.101f,  R, S, 0.80f },  // upper waist
		{ 0.300f, 0.0f,  0.000f, 0.144f, 0.097f,  R, S, 0.50f },  // waist (narrowest)
		{ 0.180f, 0.0f,  0.000f, 0.160f, 0.108f,  R, S, 0.22f },  // belt line
		{ 0.060f, 0.0f,  0.000f, 0.190f, 0.118f,  R, R, 0.0f },   // hips (wide to cover the thigh tops)
		{ -0.040f, 0.0f, 0.000f, 0.196f, 0.120f,  R, R, 0.0f },   // widest (seat)
		{ -0.120f, 0.0f, 0.000f, 0.172f, 0.110f,  R, R, 0.0f },   // pelvis bottom
	};
	constexpr u_int uSEGS = 48;
	const uint32_t uFirst = AddHumanLoft(pxMesh, axRings, sizeof(axRings) / sizeof(axRings[0]), uSEGS, xUV_TORSO);

	// Close the trunk: shoulder cap up top (the neck loft overlaps it), crotch
	// cap underneath (the thigh lofts overlap it).
	CapHumanRing(pxMesh, uFirst, uSEGS,
		Zenith_Maths::Vector3(0.0f, 1.155f, -0.004f), Zenith_Maths::Vector3(0, 1, 0),
		xUV_TORSO, 0.5f, 0.0f, S, S, 0.0f, true);
	const uint32_t uLastRing = pxMesh->GetNumVerts() - (uSEGS + 1);
	CapHumanRing(pxMesh, uLastRing, uSEGS,
		Zenith_Maths::Vector3(0.0f, -0.140f, 0.0f), Zenith_Maths::Vector3(0, -1, 0),
		xUV_TORSO, 0.5f, 1.0f, R, R, 0.0f, false);
}

void BuildHumanHeadNeck(Zenith_MeshAsset* pxMesh)
{
	const u_int S = STICK_BONE_SPINE;
	const u_int N = STICK_BONE_NECK;
	const u_int H = STICK_BONE_HEAD;
	const HumanRing axRings[] = {
		//   y      cx     cz      rx      rz      boneA boneB blend
		{ 1.575f, 0.0f, -0.008f, 0.058f, 0.064f,  H, H, 0.0f },   // crown taper
		{ 1.525f, 0.0f, -0.004f, 0.084f, 0.092f,  H, H, 0.0f },   // cranium
		{ 1.465f, 0.0f,  0.000f, 0.086f, 0.096f,  H, H, 0.0f },   // brow/temples
		{ 1.400f, 0.0f,  0.006f, 0.080f, 0.092f,  H, H, 0.0f },   // cheeks/nose
		{ 1.340f, 0.0f,  0.008f, 0.072f, 0.082f,  H, H, 0.0f },   // mouth
		{ 1.300f, 0.0f,  0.010f, 0.063f, 0.070f,  H, H, 0.0f },   // jaw/chin
		{ 1.270f, 0.0f,  0.002f, 0.056f, 0.057f,  N, H, 0.60f },  // under-jaw
		{ 1.200f, 0.0f,  0.000f, 0.057f, 0.058f,  N, N, 0.0f },   // neck
		{ 1.130f, 0.0f, -0.002f, 0.062f, 0.062f,  S, N, 0.60f },  // neck base (into torso)
	};
	constexpr u_int uSEGS = 64;   // dense enough for sculpted facial features
	const uint32_t uFirst = AddHumanLoft(pxMesh, axRings, sizeof(axRings) / sizeof(axRings[0]), uSEGS, xUV_HEAD);

	CapHumanRing(pxMesh, uFirst, uSEGS,
		Zenith_Maths::Vector3(0.0f, 1.596f, -0.010f), Zenith_Maths::Vector3(0, 1, 0),
		xUV_HEAD, 0.5f, 0.0f, H, H, 0.0f, true);
	// The neck base ends inside the torso — no bottom cap needed (never visible).
}

void BuildHumanArm(Zenith_MeshAsset* pxMesh, float fSide, u_int uUpper, u_int uLower, u_int uHand,
                   const HumanUVIsland& xIsland)
{
	const u_int S = STICK_BONE_SPINE;
	// Shoulder pivot is at (side*0.3, 1.1); elbow at y 0.7; wrist at y 0.4.
	const HumanRing axRings[] = {
		//   y       cx            cz      rx      rz      boneA  boneB  blend
		{ 1.150f, fSide * 0.205f, -0.004f, 0.102f, 0.094f,  S,     uUpper, 0.15f },  // deltoid cap — sits over the shoulder, well inside the torso
		{ 1.095f, fSide * 0.248f, 0.000f, 0.096f, 0.086f,  S,     uUpper, 0.50f },  // deltoid — bridges torso to arm (no gap)
		{ 1.020f, fSide * 0.290f, 0.000f, 0.080f, 0.072f,  uUpper, uUpper, 0.0f },  // biceps peak
		{ 0.920f, fSide * 0.300f, 0.000f, 0.066f, 0.061f,  uUpper, uUpper, 0.0f },  // mid upper arm
		{ 0.790f, fSide * 0.300f, 0.000f, 0.053f, 0.050f,  uUpper, uUpper, 0.0f },  // above elbow
		{ 0.748f, fSide * 0.300f, 0.000f, 0.049f, 0.047f,  uUpper, uLower, 0.20f }, // elbow upper loop
		{ 0.715f, fSide * 0.300f, 0.000f, 0.046f, 0.045f,  uUpper, uLower, 0.50f }, // elbow (graduated loops bend cleanly)
		{ 0.682f, fSide * 0.300f, 0.000f, 0.049f, 0.047f,  uUpper, uLower, 0.80f }, // elbow lower loop
		{ 0.640f, fSide * 0.300f, 0.000f, 0.055f, 0.053f,  uLower, uLower, 0.0f },  // forearm bulge
		{ 0.520f, fSide * 0.300f, 0.000f, 0.044f, 0.043f,  uLower, uLower, 0.0f },  // forearm
		{ 0.435f, fSide * 0.300f, 0.000f, 0.031f, 0.033f,  uLower, uHand, 0.45f },  // wrist
	};
	constexpr u_int uSEGS = 28;
	const uint32_t uFirst = AddHumanLoft(pxMesh, axRings, sizeof(axRings) / sizeof(axRings[0]), uSEGS, xIsland);

	// Shoulder cap — closes the top of the deltoid (its inner half is inside the torso).
	CapHumanRing(pxMesh, uFirst, uSEGS,
		Zenith_Maths::Vector3(fSide * 0.205f, 1.166f, -0.004f), Zenith_Maths::Vector3(0, 1, 0),
		xIsland, 0.5f, 0.0f, S, uUpper, 0.15f, true);
}

// Loft a tapered digit articulated over THREE phalanx bones (proximal/middle/
// distal) so it bends like a real finger. Rings stay horizontal (the digit is
// mostly vertical) and the per-ring skin transitions across the two knuckles.
void BuildHumanDigit(Zenith_MeshAsset* pxMesh, const Zenith_Maths::Vector3& xRoot,
                     const Zenith_Maths::Vector3& xTip, float fRootR, float fTipR,
                     u_int uProx, u_int uMid, u_int uDist, const HumanUVIsland& xIsland)
{
	constexpr u_int uNUM = 4;
	constexpr u_int uSEGS = 10;
	// ring0 proximal, ring1 prox/mid knuckle, ring2 mid/distal knuckle, ring3 distal.
	const u_int auBoneA[uNUM] = { uProx, uProx, uMid,  uDist };
	const u_int auBoneB[uNUM] = { uProx, uMid,  uDist, uDist };
	const float afBlend[uNUM] = { 0.0f,  0.5f,  0.5f,  0.0f  };
	HumanRing axRings[uNUM];
	for (u_int i = 0; i < uNUM; i++)
	{
		const float fT = static_cast<float>(i) / static_cast<float>(uNUM - 1);
		const Zenith_Maths::Vector3 xC = xRoot + (xTip - xRoot) * fT;
		const float fR = (fRootR + (fTipR - fRootR) * fT) * (1.0f + 0.18f * sinf(fT * fHUMAN_PI));
		axRings[i] = { xC.y, xC.x, xC.z, fR, fR, auBoneA[i], auBoneB[i], afBlend[i] };
	}
	AddHumanLoft(pxMesh, axRings, uNUM, uSEGS, xIsland);
	const uint32_t uLast = pxMesh->GetNumVerts() - (uSEGS + 1);
	const Zenith_Maths::Vector3 xCap = xTip + (xTip - xRoot) * 0.12f;
	CapHumanRing(pxMesh, uLast, uSEGS, xCap, Zenith_Maths::Vector3(0, -1, 0),
		xIsland, 0.5f, 1.0f, uDist, uDist, 0.0f, false);
}

void BuildHumanHand(Zenith_MeshAsset* pxMesh, float fSide, u_int uLower, u_int uHand,
                    const HumanUVIsland& xIsland)
{
	const float fCx = fSide * 0.302f;
	// Palm: flattened (thin across X, broad front-to-back in Z), wrist -> knuckles.
	const HumanRing axPalm[] = {
		//   y       cx     cz      rx      rz      boneA  boneB  blend
		{ 0.425f, fSide * 0.300f, 0.000f, 0.026f, 0.030f,  uLower, uHand, 0.65f },  // wrist
		{ 0.380f, fCx,            0.004f, 0.024f, 0.044f,  uHand, uHand, 0.0f },     // mid-palm
		{ 0.330f, fCx,            0.006f, 0.022f, 0.050f,  uHand, uHand, 0.0f },     // knuckle line
		{ 0.305f, fCx,            0.006f, 0.020f, 0.048f,  uHand, uHand, 0.0f },     // knuckle base
	};
	constexpr u_int uSEGS = 24;
	AddHumanLoft(pxMesh, axPalm, sizeof(axPalm) / sizeof(axPalm[0]), uSEGS, xIsland);
	const uint32_t uLastRing = pxMesh->GetNumVerts() - (uSEGS + 1);
	CapHumanRing(pxMesh, uLastRing, uSEGS,
		Zenith_Maths::Vector3(fCx, 0.300f, 0.006f), Zenith_Maths::Vector3(0, -1, 0),
		xIsland, 0.5f, 1.0f, uHand, uHand, 0.0f, false);

	// Five articulated digits (index, middle, ring, pinky, thumb), each skinned to
	// its three phalanx bones. The shared layout (GetHumanHandDigits) matches the
	// finger bones added in CreateStickFigureSkeleton, so the bind pose lines up.
	const int iHand = (fSide < 0.0f) ? 0 : 1;
	HumanDigitDef axDigits[5];
	GetHumanHandDigits(fSide, axDigits);
	for (int d = 0; d < 5; d++)
	{
		BuildHumanDigit(pxMesh, axDigits[d].xRoot, axDigits[d].xTip,
			axDigits[d].fRootR, axDigits[d].fRootR * 0.72f,
			HumanFingerBone(iHand, d, 0), HumanFingerBone(iHand, d, 1), HumanFingerBone(iHand, d, 2),
			xIsland);
	}
}

void BuildHumanLeg(Zenith_MeshAsset* pxMesh, float fSide, u_int uUpper, u_int uLower, u_int uFoot,
                   const HumanUVIsland& xIsland)
{
	const u_int R = STICK_BONE_ROOT;
	// Hip pivot at (side*0.15, 0); knee at y -0.5; ankle at y -1.0.
	const HumanRing axRings[] = {
		//   y       cx            cz      rx      rz      boneA  boneB  blend
		{  0.075f, fSide * 0.130f, 0.004f, 0.060f, 0.068f,  R,     uUpper, 0.18f }, // thigh root — tucked inside the pelvis so the seam is hidden
		{ -0.020f, fSide * 0.143f, 0.004f, 0.101f, 0.109f,  R,     uUpper, 0.50f }, // hip
		{ -0.120f, fSide * 0.149f, 0.004f, 0.099f, 0.107f,  R,     uUpper, 0.88f }, // quad
		{ -0.250f, fSide * 0.150f, 0.002f, 0.089f, 0.096f,  uUpper, uUpper, 0.0f }, // mid thigh
		{ -0.400f, fSide * 0.150f, 0.000f, 0.073f, 0.080f,  uUpper, uUpper, 0.0f }, // above knee
		{ -0.445f, fSide * 0.150f, 0.000f, 0.066f, 0.071f,  uUpper, uLower, 0.20f }, // knee upper loop
		{ -0.480f, fSide * 0.150f, 0.000f, 0.063f, 0.068f,  uUpper, uLower, 0.50f }, // knee (graduated loops bend cleanly)
		{ -0.515f, fSide * 0.150f, -0.001f, 0.064f, 0.070f, uUpper, uLower, 0.80f }, // knee lower loop
		{ -0.560f, fSide * 0.150f, -0.002f, 0.064f, 0.070f, uLower, uLower, 0.0f },  // upper calf
		{ -0.660f, fSide * 0.150f, -0.008f, 0.071f, 0.079f, uLower, uLower, 0.0f },  // calf belly
		{ -0.800f, fSide * 0.150f, -0.004f, 0.052f, 0.058f, uLower, uLower, 0.0f },  // lower calf
		{ -0.920f, fSide * 0.150f, 0.000f, 0.040f, 0.044f,  uLower, uFoot, 0.40f },  // ankle
		{ -0.975f, fSide * 0.150f, 0.000f, 0.037f, 0.041f,  uFoot, uFoot, 0.0f },    // into the shoe
	};
	constexpr u_int uSEGS = 36;
	AddHumanLoft(pxMesh, axRings, sizeof(axRings) / sizeof(axRings[0]), uSEGS, xIsland);
	// No caps: the top ends inside the pelvis, the bottom inside the shoe.
}

//------------------------------------------------------------------------------
// Post-assembly passes.
//------------------------------------------------------------------------------

// Sculpt real facial anatomy the ring loft can't express. Feature centres are
// aligned to the painted atlas features (PaintHumanHead) — model Y derived from
// the head loft's cumulative-distance V-spread — so the 3D relief reinforces the
// painted eyes/nose/lips instead of fighting them. Front features are gated to
// the front hemisphere; ears are separate lateral protrusions near z~0.
void SculptHumanFace(Zenith_MeshAsset* pxMesh)
{
	for (uint32_t u = 0; u < pxMesh->GetNumVerts(); u++)
	{
		Zenith_Maths::Vector3& xPos = pxMesh->m_xPositions.Get(u);
		const float x = xPos.x;
		const float y = xPos.y;
		const float z = xPos.z;
		if (y < 1.255f || y > 1.605f)
		{
			continue;   // head region only
		}

		// Front-hemisphere weight so the sides/back of the skull stay untouched.
		const float fFront = std::clamp(z / 0.04f, 0.0f, 1.0f);

		float fDZ = 0.0f;   // forward (+z) relief, applied with fFront

		// Brow ridge — protrudes above the eyes, widest at centre.
		fDZ += 0.013f * HumanGauss(x * 0.7f, y - 1.454f, 0.056f, 0.013f);

		// Eye sockets (recessed) each with an eyeball dome and an upper-lid fold.
		for (int s = -1; s <= 1; s += 2)
		{
			const float fEX = x - static_cast<float>(s) * 0.034f;
			fDZ -= 0.019f * HumanGauss(fEX, y - 1.429f, 0.026f, 0.020f);   // socket recess (deeper)
			fDZ += 0.010f * HumanGauss(fEX, y - 1.426f, 0.014f, 0.012f);   // eyeball dome
			fDZ += 0.006f * HumanGauss(fEX, y - 1.438f, 0.020f, 0.006f);   // upper-lid fold
		}

		// Nose — narrow bridge ridge, rounded tip, nostril wings, sub-nose scoop.
		fDZ += 0.022f * HumanGauss(x, y - 1.418f, 0.012f, 0.040f);        // bridge
		fDZ += 0.017f * HumanGauss(x, y - 1.388f, 0.015f, 0.014f);        // tip ball
		for (int s = -1; s <= 1; s += 2)
		{
			fDZ += 0.009f * HumanGauss(x - static_cast<float>(s) * 0.012f, y - 1.382f, 0.010f, 0.012f);  // wings
		}
		fDZ -= 0.006f * HumanGauss(x, y - 1.360f, 0.020f, 0.009f);        // under the nose

		// Cheekbones (zygomatic).
		for (int s = -1; s <= 1; s += 2)
		{
			fDZ += 0.009f * HumanGauss(x - static_cast<float>(s) * 0.046f, y - 1.396f, 0.028f, 0.024f);
		}

		// Lips: fuller and closed (the gap/seam used to read as an open mouth).
		fDZ += 0.013f * HumanGauss(x, y - 1.338f, 0.030f, 0.011f);        // upper lip
		fDZ += 0.015f * HumanGauss(x, y - 1.325f, 0.029f, 0.012f);        // lower lip (fuller)
		fDZ -= 0.0025f * HumanGauss(x, y - 1.332f, 0.032f, 0.0035f);     // mouth seam (subtle)
		fDZ -= 0.004f * HumanGauss(x, y - 1.351f, 0.006f, 0.010f);        // philtrum

		// Chin (mental protrusion) + mentolabial crease above it.
		fDZ += 0.013f * HumanGauss(x, y - 1.300f, 0.026f, 0.018f);
		fDZ -= 0.005f * HumanGauss(x, y - 1.316f, 0.024f, 0.008f);

		// Jawline — define the mandible corners for a sharper jaw.
		for (int s = -1; s <= 1; s += 2)
		{
			fDZ += 0.007f * HumanGauss(x - static_cast<float>(s) * 0.058f, y - 1.306f, 0.022f, 0.026f);
		}

		xPos.z += fDZ * fFront;
		// The nose tip droops a touch.
		xPos.y -= 0.004f * HumanGauss(x, y - 1.388f, 0.015f, 0.014f) * fFront;

		// Ears — lateral protrusions at the temple line with a helix rim and a
		// recessed concha bowl, near z~0 on both sides.
		const float fEarZ = std::clamp(1.0f - std::abs(z + 0.004f) / 0.042f, 0.0f, 1.0f);
		for (int s = -1; s <= 1; s += 2)
		{
			const float fEar = HumanGauss(x - static_cast<float>(s) * 0.085f, y - 1.418f, 0.019f, 0.034f) * fEarZ;
			xPos.x += static_cast<float>(s) * 0.024f * fEar;   // push outward (helix rim)
			xPos.z -= 0.006f * fEar;                           // and slightly back
			const float fConcha = HumanGauss(x - static_cast<float>(s) * 0.088f, y - 1.416f, 0.008f, 0.014f) * fEarZ;
			xPos.x -= static_cast<float>(s) * 0.012f * fConcha;  // concha bowl indent
		}
		// (Hair is now real geometry — see BuildHumanHair — so the scalp is no
		// longer inflated here.)
	}
}

// Subtle muscular / skeletal surface definition on the trunk and bare arms.
// Gentle on the clothed torso/legs (cloth drapes over muscle) and a touch
// stronger on the bare upper arms. Runs before normal computation so shading
// follows the relief.
void SculptHumanBody(Zenith_MeshAsset* pxMesh)
{
	for (uint32_t u = 0; u < pxMesh->GetNumVerts(); u++)
	{
		Zenith_Maths::Vector3& xPos = pxMesh->m_xPositions.Get(u);
		const float x = xPos.x;
		const float y = xPos.y;
		const float z = xPos.z;
		if (y > 1.16f)
		{
			continue;   // head/neck handled by SculptHumanFace
		}

		const float fFront = std::clamp(z / 0.06f, 0.0f, 1.0f);
		const float fBack = std::clamp(-z / 0.06f, 0.0f, 1.0f);

		// Trunk (clothed -> subtle): pecs, sternum groove, clavicles on the front;
		// spine groove + shoulder blades on the back.
		if (std::abs(x) < 0.24f && y > -0.10f)
		{
			float fF = 0.0f;
			for (int s = -1; s <= 1; s += 2) { fF += 0.011f * HumanGauss(x - static_cast<float>(s) * 0.078f, y - 0.93f, 0.058f, 0.075f); }   // pecs
			fF -= 0.006f * HumanGauss(x, y - 0.88f, 0.012f, 0.100f);                                                                          // sternum groove
			for (int s = -1; s <= 1; s += 2) { fF += 0.007f * HumanGauss(x - static_cast<float>(s) * 0.085f, y - 1.045f, 0.058f, 0.015f); }  // clavicles
			xPos.z += fF * fFront;

			float fScap = 0.0f;
			for (int s = -1; s <= 1; s += 2) { fScap += 0.008f * HumanGauss(x - static_cast<float>(s) * 0.100f, y - 0.98f, 0.060f, 0.055f); }
			xPos.z += 0.009f * HumanGauss(x, y - 0.62f, 0.020f, 0.420f) * fBack;   // spine groove (inward)
			xPos.z -= fScap * fBack;                                              // scapulae (outward)
		}

		// Bare upper-arm biceps (front) / triceps (back).
		for (int s = -1; s <= 1; s += 2)
		{
			const float fArm = HumanGauss(x - static_cast<float>(s) * 0.300f, y - 0.95f, 0.045f, 0.085f);
			xPos.z += 0.011f * fArm * fFront;
			xPos.z -= 0.006f * fArm * fBack;
		}

		// Kneecaps (front of the knee).
		for (int s = -1; s <= 1; s += 2)
		{
			xPos.z += 0.008f * HumanGauss(x - static_cast<float>(s) * 0.150f, y + 0.480f, 0.045f, 0.045f) * fFront;
		}
	}
}

// Real hair geometry: a layered shell of stacked rings over the crown/back/sides
// that snaps to an around-the-head hairline (high at the front so the face stays
// open, lower over the sides/back), with a scalloped/strandy outer surface for a
// hair-like silhouette rather than a smooth helmet. Rigidly skinned to the head
// bone and UV-mapped into the painted hair region so it reads dark. Built AFTER
// the face sculpt so the sculpt never deforms it.
void BuildHumanHair(Zenith_MeshAsset* pxMesh)
{
	const u_int H = STICK_BONE_HEAD;
	constexpr u_int uSEGS = 56;
	constexpr u_int uRINGS = 16;

	// Head surface radius/centre over the hair band (piecewise-linear, y 1.40..1.60).
	auto HeadProfile = [](float fY, float& fRx, float& fRz, float& fCz)
	{
		struct P { float fY, fRx, fRz, fCz; };
		static const P aP[] = {
			{ 1.600f, 0.050f, 0.058f, -0.008f },
			{ 1.520f, 0.085f, 0.093f, -0.004f },
			{ 1.460f, 0.087f, 0.097f,  0.000f },
			{ 1.400f, 0.081f, 0.093f,  0.006f },
		};
		const float fC = std::clamp(fY, aP[3].fY, aP[0].fY);
		for (int i = 0; i < 3; i++)
		{
			if (fC <= aP[i].fY && fC >= aP[i + 1].fY)
			{
				const float fF = (aP[i].fY - fC) / (aP[i].fY - aP[i + 1].fY);
				fRx = aP[i].fRx + (aP[i + 1].fRx - aP[i].fRx) * fF;
				fRz = aP[i].fRz + (aP[i + 1].fRz - aP[i].fRz) * fF;
				fCz = aP[i].fCz + (aP[i + 1].fCz - aP[i].fCz) * fF;
				return;
			}
		}
		fRx = aP[0].fRx; fRz = aP[0].fRz; fCz = aP[0].fCz;
	};

	uint32_t uPrevRow = 0;
	uint32_t uFirstRow = 0;
	for (u_int r = 0; r < uRINGS; r++)
	{
		const float fT = static_cast<float>(r) / static_cast<float>(uRINGS - 1);
		const float fY = 1.605f - fT * 0.215f;   // ~1.605 (crown) -> 1.39
		const uint32_t uRow = pxMesh->GetNumVerts();
		for (u_int s = 0; s <= uSEGS; s++)
		{
			const float fAng = fHUMAN_TWO_PI * static_cast<float>(s) / static_cast<float>(uSEGS);
			const float fSin = sinf(fAng);
			const float fCos = cosf(fAng);
			const float fFrontness = (-fCos) * 0.5f + 0.5f;             // 1 at front (ang=pi), 0 at back
			const float fHairline = 1.392f + 0.094f * powf(fFrontness, 1.7f);   // high front, low back/sides
			const float fVY = std::max(fY, fHairline);
			float fRx, fRz, fCz;
			HeadProfile(fVY, fRx, fRz, fCz);
			// Hair thickness with a strandy/scalloped outer surface.
			const float fStrand = HumanValueNoise2D(fAng * 4.0f, fT * 7.0f, 71u) - 0.5f;
			const float fOff = 0.022f + 0.011f * fStrand + 0.006f * sinf(fAng * 9.0f);
			const Zenith_Maths::Vector3 xPos((fRx + fOff) * fSin, fVY, fCz - (fRz + fOff) * fCos);
			const Zenith_Maths::Vector3 xNrm = glm::normalize(Zenith_Maths::Vector3(fSin, 0.25f, -fCos));
			const float fVN = 0.03f + fT * 0.16f;   // map into the painted hair region (dark)
			pxMesh->AddVertex(xPos, xNrm, Zenith_Maths::Vector2(xUV_HEAD.U(static_cast<float>(s) / static_cast<float>(uSEGS)), xUV_HEAD.V(fVN)));
			pxMesh->SetVertexSkinning(pxMesh->GetNumVerts() - 1, glm::uvec4(H, H, 0, 0), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
		}
		if (r == 0) { uFirstRow = uRow; }
		else { StitchHumanRings(pxMesh, uPrevRow, uRow, uSEGS); }
		uPrevRow = uRow;
	}

	// Crown cap closes the top of the hair.
	CapHumanRing(pxMesh, uFirstRow, uSEGS,
		Zenith_Maths::Vector3(0.0f, 1.628f, -0.010f), Zenith_Maths::Vector3(0, 1, 0),
		xUV_HEAD, 0.5f, 0.0f, H, H, 0.0f, true);
}

// Separate eyeball geometry (like the UE5 mannequin): a small sphere per eye,
// skinned rigidly to its eye bone so it can later be aimed, and UV-projected onto
// the painted eye in the atlas (sclera/iris/pupil/catchlight) — the sphere's front
// pole lands on the iris and the surface fans out to the sclera. Sits ~3mm proud
// of the sculpted socket so it reads as a real eyeball. Built after the sculpt.
void BuildHumanEyes(Zenith_MeshAsset* pxMesh)
{
	struct EyeDef { float fCx; u_int uBone; float fUNorm; };
	const EyeDef axEyes[2] = {
		{ -0.034f, STICK_BONE_LEFT_EYE,  0.563f },
		{  0.034f, STICK_BONE_RIGHT_EYE, 0.437f },
	};
	const float fCy = 1.429f;
	const float fCz = 0.062f;
	const float fR = 0.018f;
	const float fEyeVN = 0.328f;
	constexpr u_int uLAT = 8;
	constexpr u_int uLON = 12;

	for (const EyeDef& xE : axEyes)
	{
		const float fU0 = xUV_HEAD.U(xE.fUNorm);
		const float fV0 = xUV_HEAD.V(fEyeVN);
		const float fHalfU = xUV_HEAD.U(xE.fUNorm + 0.027f) - fU0;   // almond half-extents in atlas UV
		const float fHalfV = xUV_HEAD.V(fEyeVN + 0.016f) - fV0;
		uint32_t uPrevRow = 0;
		for (u_int la = 0; la <= uLAT; la++)
		{
			const float fTheta = fHUMAN_PI * static_cast<float>(la) / static_cast<float>(uLAT);   // 0 = front pole (+z, iris)
			const float fSinT = sinf(fTheta);
			const float fCosT = cosf(fTheta);
			const uint32_t uRow = pxMesh->GetNumVerts();
			for (u_int lo = 0; lo <= uLON; lo++)
			{
				const float fPhi = fHUMAN_TWO_PI * static_cast<float>(lo) / static_cast<float>(uLON);
				const float fDx = fR * fSinT * cosf(fPhi);
				const float fDy = fR * fSinT * sinf(fPhi);
				const float fDz = fR * fCosT;
				const Zenith_Maths::Vector3 xPos(xE.fCx + fDx, fCy + fDy, fCz + fDz);
				const Zenith_Maths::Vector3 xNrm = glm::normalize(Zenith_Maths::Vector3(fDx, fDy, fDz) + Zenith_Maths::Vector3(0, 0, 1e-4f));
				const Zenith_Maths::Vector2 xUV(fU0 + (fDx / fR) * fHalfU, fV0 - (fDy / fR) * fHalfV);
				pxMesh->AddVertex(xPos, xNrm, xUV);
				pxMesh->SetVertexSkinning(pxMesh->GetNumVerts() - 1, glm::uvec4(xE.uBone, xE.uBone, 0, 0), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
			}
			if (la > 0) { StitchHumanRings(pxMesh, uPrevRow, uRow, uLON); }
			uPrevRow = uRow;
		}
	}
}

// Smooth normals, welded by position so UV-seam duplicate columns and part
// overlaps shade continuously. Face normals are accumulated area-weighted.
void ComputeHumanSmoothNormals(Zenith_MeshAsset* pxMesh)
{
	const uint32_t uNumVerts = pxMesh->GetNumVerts();

	// Weld map: weld[i] = lowest vertex index sharing (quantized) position. Built
	// via a hash on the packed quantized position so the pass is O(N), not the
	// original O(N^2) nested scan — the body is now ~10k verts and this runs on
	// every tools boot. Iterating i ascending means the first index stored for a
	// key is its lowest index (== the old "lowest matching j" root), so the result
	// is identical to the nested-scan version.
	Zenith_Vector<uint32_t> xWeld(uNumVerts);
	auto Quantize = [](float f) { return static_cast<int>(std::floor(f * 4096.0f + 0.5f)); };
	auto Pack = [](int iX, int iY, int iZ) -> int64_t
	{
		// Quantized body coords sit well inside +/-2^20; offset to non-negative and
		// pack three 21-bit fields into one 63-bit key (collision-free for the rig).
		constexpr int64_t kOffset = 1ll << 20;
		return (static_cast<int64_t>(iX) + kOffset)
			| ((static_cast<int64_t>(iY) + kOffset) << 21)
			| ((static_cast<int64_t>(iZ) + kOffset) << 42);
	};
	std::unordered_map<int64_t, uint32_t> xPosToRoot;
	xPosToRoot.reserve(uNumVerts);
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		const Zenith_Maths::Vector3& xPi = pxMesh->m_xPositions.Get(i);
		const int64_t iKey = Pack(Quantize(xPi.x), Quantize(xPi.y), Quantize(xPi.z));
		const auto xIt = xPosToRoot.find(iKey);
		if (xIt != xPosToRoot.end())
		{
			xWeld.PushBack(xIt->second);
		}
		else
		{
			xPosToRoot.emplace(iKey, i);
			xWeld.PushBack(i);
		}
	}

	Zenith_Vector<Zenith_Maths::Vector3> xAccum(uNumVerts);
	for (uint32_t i = 0; i < uNumVerts; i++) { xAccum.PushBack(Zenith_Maths::Vector3(0.0f)); }

	for (uint32_t u = 0; u < pxMesh->GetNumIndices(); u += 3)
	{
		const uint32_t uA = pxMesh->m_xIndices.Get(u);
		const uint32_t uB = pxMesh->m_xIndices.Get(u + 1);
		const uint32_t uC = pxMesh->m_xIndices.Get(u + 2);
		const Zenith_Maths::Vector3& xA = pxMesh->m_xPositions.Get(uA);
		const Zenith_Maths::Vector3& xB = pxMesh->m_xPositions.Get(uB);
		const Zenith_Maths::Vector3& xC = pxMesh->m_xPositions.Get(uC);
		// Length = 2*area, so this is the area-weighted face normal.
		const Zenith_Maths::Vector3 xFace = glm::cross(xB - xA, xC - xA);
		xAccum.Get(xWeld.Get(uA)) += xFace;
		xAccum.Get(xWeld.Get(uB)) += xFace;
		xAccum.Get(xWeld.Get(uC)) += xFace;
	}

	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		const Zenith_Maths::Vector3& xSum = xAccum.Get(xWeld.Get(i));
		const float fLen = glm::length(xSum);
		pxMesh->m_xNormals.Get(i) = (fLen > 1e-8f)
			? xSum / fLen
			: Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
	}
}

// Bake soft cavity occlusion into vertex colors (the animated G-buffer shader
// multiplies diffuse by vertex color). Kept subtle — armpits, crotch, under
// the chin and behind the knees.
void BakeHumanVertexAO(Zenith_MeshAsset* pxMesh)
{
	struct AOSphere { Zenith_Maths::Vector3 xPos; float fRadius; float fStrength; };
	const AOSphere axSpheres[] = {
		{ {  0.21f, 1.06f,  0.00f }, 0.10f, 0.30f },   // right armpit
		{ { -0.21f, 1.06f,  0.00f }, 0.10f, 0.30f },   // left armpit
		{ {  0.00f, -0.10f, 0.00f }, 0.13f, 0.35f },   // crotch
		{ {  0.00f, 1.265f, 0.035f }, 0.07f, 0.25f },  // under chin
		{ {  0.15f, -0.50f, -0.05f }, 0.09f, 0.20f },  // right knee back
		{ { -0.15f, -0.50f, -0.05f }, 0.09f, 0.20f },  // left knee back
	};

	for (uint32_t u = 0; u < pxMesh->GetNumVerts(); u++)
	{
		const Zenith_Maths::Vector3& xPos = pxMesh->m_xPositions.Get(u);
		float fAO = 1.0f;
		for (const AOSphere& xS : axSpheres)
		{
			const float fD = glm::length(xPos - xS.xPos) / xS.fRadius;
			fAO -= xS.fStrength * expf(-fD * fD);
		}
		fAO = std::clamp(fAO, 0.55f, 1.0f);
		pxMesh->m_xColors.Get(u) = Zenith_Maths::Vector4(fAO, fAO, fAO, 1.0f);
	}
}

// GenerateTangents skips UV-degenerate triangles; any vertex left with a zero
// accumulation would normalize to NaN. Replace non-finite results with a frame
// built from the normal.
void SanitizeHumanTangents(Zenith_MeshAsset* pxMesh)
{
	for (uint32_t u = 0; u < pxMesh->GetNumVerts(); u++)
	{
		Zenith_Maths::Vector3& xT = pxMesh->m_xTangents.Get(u);
		Zenith_Maths::Vector3& xB = pxMesh->m_xBitangents.Get(u);
		if (std::isfinite(xT.x) && std::isfinite(xT.y) && std::isfinite(xT.z) && glm::length(xT) > 0.5f)
		{
			continue;
		}
		const Zenith_Maths::Vector3& xN = pxMesh->m_xNormals.Get(u);
		const Zenith_Maths::Vector3 xRef = (fabsf(xN.y) < 0.95f)
			? Zenith_Maths::Vector3(0, 1, 0) : Zenith_Maths::Vector3(1, 0, 0);
		xT = glm::normalize(glm::cross(xRef, xN));
		xB = glm::cross(xN, xT);
	}
}

} // namespace

//------------------------------------------------------------------------------
// The human body mesh. Same signature as the old cube builder — the skeleton
// parameter documents the rig the proportions are authored against.
//------------------------------------------------------------------------------
static Zenith_MeshAsset* CreateStickFigureMesh(const Zenith_SkeletonAsset* pxSkeleton)
{
	Zenith_Assert(pxSkeleton->GetNumBones() == STICK_BONE_COUNT, "StickFigure rig changed — body proportions are authored against the 16-core + UE5-additions layout");

	Zenith_MeshAsset* pxMesh = new Zenith_MeshAsset();
	// High-poly: ~10k verts after segment + Catmull-Rom ring subdivision. Reserve
	// generously so the per-part lofts don't trigger repeated vector reallocations.
	pxMesh->Reserve(16384, 98304);

	BuildHumanTorso(pxMesh);
	BuildHumanHeadNeck(pxMesh);
	BuildHumanArm(pxMesh, -1.0f, STICK_BONE_LEFT_UPPER_ARM, STICK_BONE_LEFT_LOWER_ARM, STICK_BONE_LEFT_HAND, xUV_ARM_L);
	BuildHumanArm(pxMesh, 1.0f, STICK_BONE_RIGHT_UPPER_ARM, STICK_BONE_RIGHT_LOWER_ARM, STICK_BONE_RIGHT_HAND, xUV_ARM_R);
	BuildHumanHand(pxMesh, -1.0f, STICK_BONE_LEFT_LOWER_ARM, STICK_BONE_LEFT_HAND, xUV_HAND_L);
	BuildHumanHand(pxMesh, 1.0f, STICK_BONE_RIGHT_LOWER_ARM, STICK_BONE_RIGHT_HAND, xUV_HAND_R);
	BuildHumanLeg(pxMesh, -1.0f, STICK_BONE_LEFT_UPPER_LEG, STICK_BONE_LEFT_LOWER_LEG, STICK_BONE_LEFT_FOOT, xUV_LEG_L);
	BuildHumanLeg(pxMesh, 1.0f, STICK_BONE_RIGHT_UPPER_LEG, STICK_BONE_RIGHT_LOWER_LEG, STICK_BONE_RIGHT_FOOT, xUV_LEG_R);
	BuildHumanShoe(pxMesh, -1.0f, STICK_BONE_LEFT_FOOT, xUV_FOOT_L);
	BuildHumanShoe(pxMesh, 1.0f, STICK_BONE_RIGHT_FOOT, xUV_FOOT_R);

	SculptHumanFace(pxMesh);
	SculptHumanBody(pxMesh);
	BuildHumanHair(pxMesh);   // real hair geometry, after sculpt so it isn't deformed
	const uint32_t uEyeIndexStart = pxMesh->GetNumIndices();
	BuildHumanEyes(pxMesh);   // separate eyeball geometry skinned to the eye bones
	ComputeHumanSmoothNormals(pxMesh);
	pxMesh->GenerateTangents();
	SanitizeHumanTangents(pxMesh);
	BakeHumanVertexAO(pxMesh);

	// Two submeshes: body (material 0 — skin/cloth, subsurface) and the eyeballs
	// (material 1 — a glossy clear-coat cornea). The eye verts are the contiguous
	// tail added by BuildHumanEyes; the post-passes don't change topology so the
	// index range is stable.
	pxMesh->AddSubmesh(0, uEyeIndexStart, 0);
	pxMesh->AddSubmesh(uEyeIndexStart, pxMesh->GetNumIndices() - uEyeIndexStart, 1);
	pxMesh->ComputeBounds();
	return pxMesh;
}

// Non-static: shared with Zenith_Tools_TreeAssetExport.cpp (declared in the header).
Flux_MeshGeometry* Zenith_Tools_CreateFluxMeshGeometry(const Zenith_MeshAsset* pxMeshAsset, const Zenith_SkeletonAsset* pxSkeleton)
{
	Flux_MeshGeometry* pxGeometry = new Flux_MeshGeometry();

	const uint32_t uNumVerts = pxMeshAsset->GetNumVerts();
	const uint32_t uNumIndices = pxMeshAsset->GetNumIndices();
	const uint32_t uNumBones = pxSkeleton->GetNumBones();

	pxGeometry->m_uNumVerts = uNumVerts;
	pxGeometry->m_uNumIndices = uNumIndices;
	pxGeometry->m_uNumBones = uNumBones;
	pxGeometry->m_xMaterialColor = pxMeshAsset->m_xMaterialColor;

	// Copy positions
	pxGeometry->m_pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		pxGeometry->m_pxPositions[i] = pxMeshAsset->m_xPositions.Get(i);
	}

	// Copy normals
	if (pxMeshAsset->m_xNormals.GetSize() > 0)
	{
		pxGeometry->m_pxNormals = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
		for (uint32_t i = 0; i < uNumVerts; i++)
		{
			pxGeometry->m_pxNormals[i] = pxMeshAsset->m_xNormals.Get(i);
		}
	}

	// Copy UVs
	if (pxMeshAsset->m_xUVs.GetSize() > 0)
	{
		pxGeometry->m_pxUVs = static_cast<Zenith_Maths::Vector2*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector2)));
		for (uint32_t i = 0; i < uNumVerts; i++)
		{
			pxGeometry->m_pxUVs[i] = pxMeshAsset->m_xUVs.Get(i);
		}
	}

	// Copy tangents
	if (pxMeshAsset->m_xTangents.GetSize() > 0)
	{
		pxGeometry->m_pxTangents = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
		for (uint32_t i = 0; i < uNumVerts; i++)
		{
			pxGeometry->m_pxTangents[i] = pxMeshAsset->m_xTangents.Get(i);
		}
	}

	// Copy bitangents (the human body authors them via GenerateTangents; the
	// animated-mesh vertex layout reconstructs its TBN from them)
	if (pxMeshAsset->m_xBitangents.GetSize() > 0)
	{
		pxGeometry->m_pxBitangents = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
		for (uint32_t i = 0; i < uNumVerts; i++)
		{
			pxGeometry->m_pxBitangents[i] = pxMeshAsset->m_xBitangents.Get(i);
		}
	}

	// Copy colors
	if (pxMeshAsset->m_xColors.GetSize() > 0)
	{
		pxGeometry->m_pxColors = static_cast<Zenith_Maths::Vector4*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector4)));
		for (uint32_t i = 0; i < uNumVerts; i++)
		{
			pxGeometry->m_pxColors[i] = pxMeshAsset->m_xColors.Get(i);
		}
	}

	// Copy indices
	pxGeometry->m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Allocate(uNumIndices * sizeof(Flux_MeshGeometry::IndexType)));
	for (uint32_t i = 0; i < uNumIndices; i++)
	{
		pxGeometry->m_puIndices[i] = pxMeshAsset->m_xIndices.Get(i);
	}

	// Copy bone IDs (flatten uvec4 to uint32_t array)
	if (pxMeshAsset->m_xBoneIndices.GetSize() > 0)
	{
		pxGeometry->m_puBoneIDs = static_cast<uint32_t*>(Zenith_MemoryManagement::Allocate(uNumVerts * MAX_BONES_PER_VERTEX * sizeof(uint32_t)));
		for (uint32_t v = 0; v < uNumVerts; v++)
		{
			const glm::uvec4& xIndices = pxMeshAsset->m_xBoneIndices.Get(v);
			pxGeometry->m_puBoneIDs[v * MAX_BONES_PER_VERTEX + 0] = xIndices.x;
			pxGeometry->m_puBoneIDs[v * MAX_BONES_PER_VERTEX + 1] = xIndices.y;
			pxGeometry->m_puBoneIDs[v * MAX_BONES_PER_VERTEX + 2] = xIndices.z;
			pxGeometry->m_puBoneIDs[v * MAX_BONES_PER_VERTEX + 3] = xIndices.w;
		}
	}

	// Copy bone weights (flatten vec4 to float array)
	if (pxMeshAsset->m_xBoneWeights.GetSize() > 0)
	{
		pxGeometry->m_pfBoneWeights = static_cast<float*>(Zenith_MemoryManagement::Allocate(uNumVerts * MAX_BONES_PER_VERTEX * sizeof(float)));
		for (uint32_t v = 0; v < uNumVerts; v++)
		{
			const glm::vec4& xWeights = pxMeshAsset->m_xBoneWeights.Get(v);
			pxGeometry->m_pfBoneWeights[v * MAX_BONES_PER_VERTEX + 0] = xWeights.x;
			pxGeometry->m_pfBoneWeights[v * MAX_BONES_PER_VERTEX + 1] = xWeights.y;
			pxGeometry->m_pfBoneWeights[v * MAX_BONES_PER_VERTEX + 2] = xWeights.z;
			pxGeometry->m_pfBoneWeights[v * MAX_BONES_PER_VERTEX + 3] = xWeights.w;
		}
	}

	// Build bone name to ID and offset matrix map from skeleton
	for (uint32_t b = 0; b < uNumBones; b++)
	{
		const Zenith_SkeletonAsset::Bone& xBone = pxSkeleton->GetBone(b);
		Zenith_Maths::Matrix4 xOffsetMat = glm::inverse(xBone.m_xBindPoseModel);
		pxGeometry->m_xBoneNameToIdAndOffset[xBone.m_strName] = std::make_pair(b, xOffsetMat);
	}

	// Generate buffer layout and interleaved vertex data
	pxGeometry->GenerateLayoutAndVertexData();

	return pxGeometry;
}

// Non-static: shared with Zenith_Tools_TreeAssetExport.cpp (declared in the header).
Flux_MeshGeometry* Zenith_Tools_CreateStaticFluxMeshGeometry(const Zenith_MeshAsset* pxMeshAsset)
{
	Flux_MeshGeometry* pxGeometry = new Flux_MeshGeometry();

	const uint32_t uNumVerts = pxMeshAsset->GetNumVerts();
	const uint32_t uNumIndices = pxMeshAsset->GetNumIndices();

	pxGeometry->m_uNumVerts = uNumVerts;
	pxGeometry->m_uNumIndices = uNumIndices;
	pxGeometry->m_uNumBones = 0;  // No bones for static mesh
	pxGeometry->m_xMaterialColor = pxMeshAsset->m_xMaterialColor;

	// Copy positions
	pxGeometry->m_pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		pxGeometry->m_pxPositions[i] = pxMeshAsset->m_xPositions.Get(i);
	}

	// Copy normals (or generate default up vector)
	pxGeometry->m_pxNormals = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		if (pxMeshAsset->m_xNormals.GetSize() > 0)
			pxGeometry->m_pxNormals[i] = pxMeshAsset->m_xNormals.Get(i);
		else
			pxGeometry->m_pxNormals[i] = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
	}

	// Copy UVs (or generate default zero)
	pxGeometry->m_pxUVs = static_cast<Zenith_Maths::Vector2*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector2)));
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		if (pxMeshAsset->m_xUVs.GetSize() > 0)
			pxGeometry->m_pxUVs[i] = pxMeshAsset->m_xUVs.Get(i);
		else
			pxGeometry->m_pxUVs[i] = Zenith_Maths::Vector2(0.0f, 0.0f);
	}

	// Copy tangents (or generate default)
	pxGeometry->m_pxTangents = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		if (pxMeshAsset->m_xTangents.GetSize() > 0)
			pxGeometry->m_pxTangents[i] = pxMeshAsset->m_xTangents.Get(i);
		else
			pxGeometry->m_pxTangents[i] = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
	}

	// Copy bitangents (or generate default)
	pxGeometry->m_pxBitangents = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		if (pxMeshAsset->m_xBitangents.GetSize() > 0)
			pxGeometry->m_pxBitangents[i] = pxMeshAsset->m_xBitangents.Get(i);
		else
			pxGeometry->m_pxBitangents[i] = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
	}

	// Copy colors (or generate default white)
	pxGeometry->m_pxColors = static_cast<Zenith_Maths::Vector4*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector4)));
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		if (pxMeshAsset->m_xColors.GetSize() > 0)
			pxGeometry->m_pxColors[i] = pxMeshAsset->m_xColors.Get(i);
		else
			pxGeometry->m_pxColors[i] = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	// Copy indices
	pxGeometry->m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Allocate(uNumIndices * sizeof(Flux_MeshGeometry::IndexType)));
	for (uint32_t i = 0; i < uNumIndices; i++)
	{
		pxGeometry->m_puIndices[i] = pxMeshAsset->m_xIndices.Get(i);
	}

	// NO bone IDs or weights - this is a static mesh

	// Generate buffer layout and interleaved vertex data
	pxGeometry->GenerateLayoutAndVertexData();

	return pxGeometry;
}

#ifdef ZENITH_TOOLS

//==============================================================================
// Texture atlas painting (tools builds only — runs at asset-generation time).
//
// One 1024^2 atlas drives albedo, a height field (turned into the normal map)
// and roughness/metallic. Painters work in island-normalized (uN, vN) space:
// uN runs AROUND each body part (0 = back seam, 0.5 = front), vN runs DOWN it
// (0 = top). The body-landmark math mirrors the ring tables above.
//==============================================================================
namespace
{

constexpr int32_t iHUMAN_ATLAS_SIZE = 2048;

struct HumanAtlas
{
	// Planar float buffers, 1024^2 each.
	Zenith_Vector<float> xR, xG, xB;       // albedo (linear 0..1, written as sRGB bytes)
	Zenith_Vector<float> xHeight;          // relative height for the normal map
	Zenith_Vector<float> xRough, xMetal;
	Zenith_Vector<u_int8> xPainted;        // dilation mask

	HumanAtlas()
		: xR(iHUMAN_ATLAS_SIZE * iHUMAN_ATLAS_SIZE), xG(iHUMAN_ATLAS_SIZE * iHUMAN_ATLAS_SIZE)
		, xB(iHUMAN_ATLAS_SIZE * iHUMAN_ATLAS_SIZE), xHeight(iHUMAN_ATLAS_SIZE * iHUMAN_ATLAS_SIZE)
		, xRough(iHUMAN_ATLAS_SIZE * iHUMAN_ATLAS_SIZE), xMetal(iHUMAN_ATLAS_SIZE * iHUMAN_ATLAS_SIZE)
		, xPainted(iHUMAN_ATLAS_SIZE * iHUMAN_ATLAS_SIZE)
	{
		for (int32_t i = 0; i < iHUMAN_ATLAS_SIZE * iHUMAN_ATLAS_SIZE; i++)
		{
			xR.PushBack(0.5f); xG.PushBack(0.5f); xB.PushBack(0.5f);
			xHeight.PushBack(0.5f);
			xRough.PushBack(0.8f); xMetal.PushBack(0.0f);
			xPainted.PushBack(0);
		}
	}

	void Set(int32_t iX, int32_t iY, float fR, float fG, float fB_, float fH, float fRgh, float fMtl)
	{
		const int32_t i = iY * iHUMAN_ATLAS_SIZE + iX;
		xR.Get(i) = fR; xG.Get(i) = fG; xB.Get(i) = fB_;
		xHeight.Get(i) = fH;
		xRough.Get(i) = fRgh; xMetal.Get(i) = fMtl;
		xPainted.Get(i) = 1;
	}
};

struct HumanPixel
{
	float fR, fG, fB;
	float fHeight;     // 0..1, 0.5 = neutral
	float fRough;
	float fMetal;
};

// Iterate an island's pixel rect, calling the painter with island-normalized
// coordinates. TFn: void(float fUN, float fVN, HumanPixel&).
template <typename TFn>
void PaintHumanIsland(HumanAtlas& xAtlas, const HumanUVIsland& xIsland, TFn&& xFn)
{
	const int32_t iX0 = static_cast<int32_t>(xIsland.fU0 * iHUMAN_ATLAS_SIZE);
	const int32_t iX1 = static_cast<int32_t>(xIsland.fU1 * iHUMAN_ATLAS_SIZE);
	const int32_t iY0 = static_cast<int32_t>(xIsland.fV0 * iHUMAN_ATLAS_SIZE);
	const int32_t iY1 = static_cast<int32_t>(xIsland.fV1 * iHUMAN_ATLAS_SIZE);
	for (int32_t iY = iY0; iY <= iY1; iY++)
	{
		const float fVN = (static_cast<float>(iY) / iHUMAN_ATLAS_SIZE - xIsland.fV0) / (xIsland.fV1 - xIsland.fV0);
		for (int32_t iX = iX0; iX <= iX1; iX++)
		{
			const float fUN = (static_cast<float>(iX) / iHUMAN_ATLAS_SIZE - xIsland.fU0) / (xIsland.fU1 - xIsland.fU0);
			HumanPixel xP;
			xFn(fUN, fVN, xP);
			xAtlas.Set(iX, iY,
				std::clamp(xP.fR, 0.0f, 1.0f), std::clamp(xP.fG, 0.0f, 1.0f), std::clamp(xP.fB, 0.0f, 1.0f),
				std::clamp(xP.fHeight, 0.0f, 1.0f),
				std::clamp(xP.fRough, 0.02f, 1.0f), std::clamp(xP.fMetal, 0.0f, 1.0f));
		}
	}
}

//------------------------------------------------------------------------------
// Shared surface treatments.
//------------------------------------------------------------------------------
void HumanSkinBase(float fUN, float fVN, u_int uSeed, HumanPixel& xP)
{
	// Warm skin: low-frequency tonal mottle + a subtle subsurface-red undertone +
	// faint pores. Slightly brighter, warmer and more saturated than before so it
	// reads as living skin rather than grey clay, with a touch of sheen.
	const float fMottle = HumanFBM2D(fUN * 18.0f, fVN * 18.0f, 3, uSeed) - 0.5f;
	const float fUnder = HumanFBM2D(fUN * 7.0f, fVN * 7.0f, 3, uSeed + 3u) - 0.5f;        // blotchy undertone
	const float fPores = HumanValueNoise2D(fUN * 160.0f, fVN * 160.0f, uSeed + 7u) - 0.5f;
	const float fMicro = HumanValueNoise2D(fUN * 380.0f, fVN * 380.0f, uSeed + 9u) - 0.5f; // fine pore grain
	const float fFreckle = HumanSmoothStep(0.80f, 0.93f, HumanValueNoise2D(fUN * 300.0f, fVN * 300.0f, uSeed + 13u));
	xP.fR = 0.870f + fMottle * 0.075f + fUnder * 0.045f - fFreckle * 0.10f;
	xP.fG = 0.620f + fMottle * 0.060f - fUnder * 0.010f - fFreckle * 0.085f;
	xP.fB = 0.500f + fMottle * 0.040f - fUnder * 0.020f - fFreckle * 0.060f;
	xP.fHeight = 0.5f + fPores * 0.06f + fMicro * 0.03f;
	xP.fRough = 0.58f + fMottle * 0.06f + fMicro * 0.05f;   // matte skin (avoids a blue sky-specular sheen)
	xP.fMetal = 0.0f;
}

void HumanShirtCloth(float fU, float fV, HumanPixel& xP)
{
	// Heathered slate-blue jersey: fbm tone + fine knit rows.
	const float fHeather = HumanFBM2D(fU * 34.0f, fV * 34.0f, 3, 901u) - 0.5f;
	const float fKnit = sinf(fV * fHUMAN_TWO_PI * 180.0f) * 0.5f + sinf(fU * fHUMAN_TWO_PI * 90.0f) * 0.25f;
	const float fThread = (sinf(fU * fHUMAN_TWO_PI * 300.0f) + sinf(fV * fHUMAN_TWO_PI * 320.0f)) * 0.5f;   // fine weave
	const float fWrinkle = HumanFBM2D(fU * 6.0f, fV * 6.0f, 3, 911u) - 0.5f;
	const float fShade = 1.0f + fHeather * 0.24f + fWrinkle * 0.12f;
	xP.fR = 0.215f * fShade;
	xP.fG = 0.310f * fShade;
	xP.fB = 0.480f * fShade;   // richer, more saturated denim-blue jersey
	xP.fHeight = 0.5f + fKnit * 0.05f + fThread * 0.02f + fWrinkle * 0.24f;
	xP.fRough = 0.90f;
	xP.fMetal = 0.0f;
}

void HumanTrouserCloth(float fU, float fV, HumanPixel& xP)
{
	// Charcoal twill: diagonal weave + wear-driven tone variance.
	const float fTwill = sinf((fU * 260.0f + fV * 420.0f) * fHUMAN_TWO_PI * 0.5f);
	const float fTone = HumanFBM2D(fU * 12.0f, fV * 12.0f, 3, 921u) - 0.5f;
	const float fWrinkle = HumanFBM2D(fU * 5.0f, fV * 7.0f, 3, 931u) - 0.5f;
	const float fShade = 1.0f + fTone * 0.18f + fWrinkle * 0.10f;
	xP.fR = 0.235f * fShade;
	xP.fG = 0.225f * fShade;
	xP.fB = 0.210f * fShade;
	xP.fHeight = 0.5f + fTwill * 0.035f + fWrinkle * 0.20f;
	xP.fRough = 0.88f;
	xP.fMetal = 0.0f;
}

// Dark groove helper (seams, creases): returns 0..1 line mask for |fD| < fW.
float HumanLine(float fD, float fW)
{
	return 1.0f - HumanSmoothStep(fW * 0.5f, fW, fabsf(fD));
}

void HumanApplySeam(HumanPixel& xP, float fMask)
{
	xP.fR *= 1.0f - 0.35f * fMask;
	xP.fG *= 1.0f - 0.35f * fMask;
	xP.fB *= 1.0f - 0.35f * fMask;
	xP.fHeight -= 0.18f * fMask;
}

//------------------------------------------------------------------------------
// Head island. Landmark vN values follow the head loft (y 1.596 at vN 0 down
// to the neck base y 1.13 at vN 1): brows .275, eyes .328, nose tip .41,
// mouth .54, chin .635, neck .70+. Eye centres sit at uN 0.5 +- 0.063.
//------------------------------------------------------------------------------
void PaintHumanHead(HumanAtlas& xAtlas)
{
	PaintHumanIsland(xAtlas, xUV_HEAD, [](float fUN, float fVN, HumanPixel& xP)
	{
		HumanSkinBase(fUN, fVN, 11u, xP);

		const float fSide = fUN - 0.5f;             // signed distance from the face centre column
		const float fFaceness = expf(-(fSide * fSide) / (0.20f * 0.20f));   // 1 at front, ~0 at back

		// Gentle side shading so the face reads as a form even in flat light.
		const float fShape = 0.94f + 0.06f * fFaceness;
		xP.fR *= fShape; xP.fG *= fShape; xP.fB *= fShape;

		// Warmth: cheeks, nose tip, ears.
		float fRed = 0.0f;
		fRed += 0.55f * (HumanGauss(fSide - 0.085f, fVN - 0.42f, 0.045f, 0.055f)
		               + HumanGauss(fSide + 0.085f, fVN - 0.42f, 0.045f, 0.055f));   // cheeks
		fRed += 0.50f * HumanGauss(fSide, fVN - 0.42f, 0.020f, 0.030f);              // nose tip
		fRed += 0.45f * (HumanGauss(fSide - 0.24f, fVN - 0.36f, 0.030f, 0.045f)
		               + HumanGauss(fSide + 0.24f, fVN - 0.36f, 0.030f, 0.045f));    // ears
		xP.fR += 0.060f * fRed;
		xP.fG -= 0.012f * fRed;
		xP.fB -= 0.018f * fRed;

		// Eye sockets: soft shading above/below the eyes.
		const float fSocket = HumanGauss(fSide - 0.063f, fVN - 0.328f, 0.040f, 0.030f)
		                    + HumanGauss(fSide + 0.063f, fVN - 0.328f, 0.040f, 0.030f);
		xP.fR *= 1.0f - 0.10f * fSocket;
		xP.fG *= 1.0f - 0.12f * fSocket;
		xP.fB *= 1.0f - 0.10f * fSocket;

		// --- Eyes (painted almond + iris + pupil + catchlight) ---
		for (float fEyeSign = -1.0f; fEyeSign <= 1.5f; fEyeSign += 2.0f)
		{
			const float fEX = fSide - fEyeSign * 0.063f;
			const float fEY = fVN - 0.328f;
			// Almond mask: ellipse, slightly squashed vertically.
			const float fAlmond = (fEX * fEX) / (0.024f * 0.024f) + (fEY * fEY) / (0.0135f * 0.0135f);
			if (fAlmond < 1.0f)
			{
				const float fEdge = HumanSmoothStep(1.0f, 0.78f, fAlmond);
				// Sclera, shadowed under the upper lid.
				float fScl = 0.90f - 0.18f * HumanSmoothStep(0.0f, -0.011f, fEY);
				float fER = fScl, fEG = fScl * 0.985f, fEB = fScl * 0.96f;

				// Iris.
				const float fIrisD = sqrtf(fEX * fEX + fEY * fEY) / 0.0115f;
				if (fIrisD < 1.0f)
				{
					const float fSpoke = 0.85f + 0.3f * HumanValueNoise2D(atan2f(fEY, fEX) * 6.0f, fIrisD * 3.0f, 77u);
					const float fRim = HumanSmoothStep(0.65f, 1.0f, fIrisD);
					fER = (0.24f + 0.10f * (1.0f - fIrisD)) * fSpoke * (1.0f - 0.55f * fRim);
					fEG = (0.35f + 0.12f * (1.0f - fIrisD)) * fSpoke * (1.0f - 0.55f * fRim);
					fEB = (0.42f + 0.12f * (1.0f - fIrisD)) * fSpoke * (1.0f - 0.50f * fRim);
					// Pupil.
					if (fIrisD < 0.38f)
					{
						const float fPup = HumanSmoothStep(0.38f, 0.30f, fIrisD);
						fER *= 1.0f - 0.95f * fPup; fEG *= 1.0f - 0.95f * fPup; fEB *= 1.0f - 0.95f * fPup;
					}
				}
				// Catchlight.
				// Faint painted catchlight only — the live clear-coat cornea highlight
				// (StickFigure_Eye material) now provides the view-dependent glint.
				const float fCatch = HumanGauss(fEX - 0.004f, fEY + 0.005f, 0.0035f, 0.0035f);
				fER += 0.25f * fCatch; fEG += 0.25f * fCatch; fEB += 0.25f * fCatch;

				xP.fR = glm::mix(xP.fR, fER, fEdge);
				xP.fG = glm::mix(xP.fG, fEG, fEdge);
				xP.fB = glm::mix(xP.fB, fEB, fEdge);
				xP.fRough = glm::mix(xP.fRough, 0.12f, fEdge);    // eyes are wet
				xP.fHeight += 0.05f * fEdge;                      // eyeball dome
			}
			// Lash/lid line along the top of the almond.
			const float fLash = HumanLine(fEY + 0.013f, 0.006f) * HumanSmoothStep(0.034f, 0.020f, fabsf(fEX));
			xP.fR *= 1.0f - 0.55f * fLash; xP.fG *= 1.0f - 0.55f * fLash; xP.fB *= 1.0f - 0.55f * fLash;
			// Lid crease above.
			const float fCrease = HumanLine(fEY + 0.024f, 0.005f) * HumanSmoothStep(0.034f, 0.022f, fabsf(fEX));
			xP.fR *= 1.0f - 0.18f * fCrease; xP.fG *= 1.0f - 0.18f * fCrease; xP.fB *= 1.0f - 0.18f * fCrease;
			xP.fHeight -= 0.04f * fCrease;
		}

		// --- Eyebrows: arcs above each eye with strand jitter ---
		for (float fEyeSign = -1.0f; fEyeSign <= 1.5f; fEyeSign += 2.0f)
		{
			const float fBX = fSide - fEyeSign * 0.066f;
			// Arc: brow rises slightly toward the outer end.
			const float fArcY = fVN - (0.272f - 0.012f * (fEyeSign * fBX) / 0.04f);
			const float fJitter = (HumanValueNoise2D(fBX * 220.0f, fArcY * 60.0f, 31u) - 0.5f) * 0.006f;
			const float fBrow = HumanSmoothStep(0.040f, 0.030f, fabsf(fBX))
			                  * HumanLine(fArcY + fJitter, 0.013f);
			if (fBrow > 0.0f)
			{
				const float fStrand = 0.6f + 0.4f * HumanValueNoise2D(fBX * 300.0f, fArcY * 40.0f, 37u);
				xP.fR = glm::mix(xP.fR, 0.16f * fStrand, fBrow * 0.9f);
				xP.fG = glm::mix(xP.fG, 0.115f * fStrand, fBrow * 0.9f);
				xP.fB = glm::mix(xP.fB, 0.085f * fStrand, fBrow * 0.9f);
				xP.fHeight += 0.03f * fBrow;
			}
		}

		// --- Nose: bridge highlight, side shading, nostrils ---
		const float fBridge = HumanGauss(fSide, fVN - 0.36f, 0.014f, 0.055f);
		xP.fR += 0.035f * fBridge; xP.fG += 0.030f * fBridge; xP.fB += 0.025f * fBridge;
		const float fNoseShade = HumanGauss(fabsf(fSide) - 0.026f, fVN - 0.40f, 0.012f, 0.040f);
		xP.fR *= 1.0f - 0.10f * fNoseShade; xP.fG *= 1.0f - 0.11f * fNoseShade; xP.fB *= 1.0f - 0.10f * fNoseShade;
		const float fNostril = HumanGauss(fSide - 0.020f, fVN - 0.455f, 0.008f, 0.007f)
		                     + HumanGauss(fSide + 0.020f, fVN - 0.455f, 0.008f, 0.007f);
		xP.fR *= 1.0f - 0.60f * fNostril; xP.fG *= 1.0f - 0.62f * fNostril; xP.fB *= 1.0f - 0.60f * fNostril;
		xP.fHeight -= 0.12f * fNostril;

		// --- Mouth ---
		const float fMouthMask = HumanSmoothStep(0.052f, 0.030f, fabsf(fSide));
		const float fLipUp = fMouthMask * HumanLine(fVN - 0.536f, 0.018f);
		const float fLipLo = fMouthMask * HumanLine(fVN - 0.551f, 0.020f);   // lips meet (closed mouth)
		xP.fR = glm::mix(xP.fR, 0.585f, fLipUp * 0.85f);
		xP.fG = glm::mix(xP.fG, 0.345f, fLipUp * 0.85f);
		xP.fB = glm::mix(xP.fB, 0.330f, fLipUp * 0.85f);
		xP.fR = glm::mix(xP.fR, 0.700f, fLipLo * 0.80f);
		xP.fG = glm::mix(xP.fG, 0.430f, fLipLo * 0.80f);
		xP.fB = glm::mix(xP.fB, 0.405f, fLipLo * 0.80f);
		const float fLipLine = fMouthMask * HumanLine(fVN - 0.543f, 0.004f);
		xP.fR *= 1.0f - 0.28f * fLipLine; xP.fG *= 1.0f - 0.30f * fLipLine; xP.fB *= 1.0f - 0.28f * fLipLine;
		xP.fRough -= 0.10f * (fLipUp + fLipLo);   // lips a touch glossier than skin, not mirror-wet
		xP.fHeight += 0.05f * fLipLo - 0.03f * fLipLine;
		// Philtrum groove.
		const float fPhiltrum = HumanLine(fSide, 0.006f) * HumanSmoothStep(0.535f, 0.50f, fVN) * HumanSmoothStep(0.455f, 0.49f, fVN);
		xP.fHeight -= 0.05f * fPhiltrum;

		// --- Stubble on the jaw ---
		const float fJawZone = HumanSmoothStep(0.56f, 0.62f, fVN) * HumanSmoothStep(0.72f, 0.66f, fVN)
		                     + HumanSmoothStep(0.10f, 0.16f, fabsf(fSide)) * HumanSmoothStep(0.46f, 0.55f, fVN) * HumanSmoothStep(0.70f, 0.62f, fVN);
		const float fStubble = std::clamp(fJawZone, 0.0f, 1.0f)
		                     * HumanSmoothStep(0.45f, 0.75f, HumanValueNoise2D(fUN * 420.0f, fVN * 420.0f, 41u)) * 0.5f;
		xP.fR *= 1.0f - 0.22f * fStubble; xP.fG *= 1.0f - 0.24f * fStubble; xP.fB *= 1.0f - 0.24f * fStubble;

		// --- Under-jaw / neck shading ---
		const float fNeckAO = HumanSmoothStep(0.64f, 0.72f, fVN) * HumanSmoothStep(0.86f, 0.74f, fVN);
		xP.fR *= 1.0f - 0.10f * fNeckAO; xP.fG *= 1.0f - 0.11f * fNeckAO; xP.fB *= 1.0f - 0.11f * fNeckAO;

		// --- Hair: short dark crop. Covers the crown, dips to a noisy front
		//     hairline, sweeps down the sides (over the ear tops) and tapers
		//     into a nape line at the back. ---
		const float fEdgeNoise = (HumanValueNoise2D(fUN * 70.0f, fVN * 70.0f, 51u) - 0.5f) * 0.030f;
		// Hairline vN limit as a function of around-the-head position.
		const float fHairLimit = 0.205f                          // front fringe
		                       + 0.28f * powf(HumanSmoothStep(0.06f, 0.30f, fabsf(fSide)), 1.4f)   // sides drop over the ears
		                       + 0.17f * HumanSmoothStep(0.30f, 0.50f, fabsf(fSide));              // nape at the back
		const float fHair = HumanSmoothStep(0.012f, -0.012f, fVN - (fHairLimit + fEdgeNoise));
		if (fHair > 0.0f)
		{
			// Directional strands: high frequency around, stretched down.
			const float fStrand = 0.55f + 0.45f * HumanValueNoise2D(fUN * 480.0f, fVN * 26.0f, 61u);
			const float fHR = 0.150f * fStrand;
			const float fHG = 0.105f * fStrand;
			const float fHB = 0.075f * fStrand;
			xP.fR = glm::mix(xP.fR, fHR, fHair);
			xP.fG = glm::mix(xP.fG, fHG, fHair);
			xP.fB = glm::mix(xP.fB, fHB, fHair);
			xP.fHeight = glm::mix(xP.fHeight, 0.5f + (fStrand - 0.78f) * 0.45f, fHair);
			xP.fRough = glm::mix(xP.fRough, 0.60f, fHair);
		}

		// Ears: painted shading where the ear geometry would be.
		const float fEar = HumanGauss(fabsf(fSide) - 0.245f, fVN - 0.355f, 0.022f, 0.038f) * (1.0f - fHair);
		xP.fR *= 1.0f - 0.16f * fEar; xP.fG *= 1.0f - 0.18f * fEar; xP.fB *= 1.0f - 0.16f * fEar;
		xP.fHeight += 0.10f * fEar;
	});
}

//------------------------------------------------------------------------------
// Torso island: crew-neck T-shirt over the chest, belt + trouser top below.
// vN(y) = (1.14 - y) / 1.28: collar ~.03, belt line .773, pelvis bottom 1.
//------------------------------------------------------------------------------
void PaintHumanTorso(HumanAtlas& xAtlas)
{
	PaintHumanIsland(xAtlas, xUV_TORSO, [](float fUN, float fVN, HumanPixel& xP)
	{
		const float fSide = fUN - 0.5f;

		// Crew collar dips at the front.
		const float fCollarV = 0.018f + 0.040f * expf(-(fSide * fSide) / (0.13f * 0.13f));

		if (fVN < fCollarV)
		{
			HumanSkinBase(fUN, fVN, 13u, xP);   // bare neck/upper chest
			return;
		}

		if (fVN < 0.773f)
		{
			HumanShirtCloth(fUN, fVN, xP);
			// Collar rib band.
			const float fRib = HumanLine(fVN - fCollarV - 0.012f, 0.014f);
			xP.fR *= 1.0f - 0.12f * fRib; xP.fG *= 1.0f - 0.12f * fRib; xP.fB *= 1.0f - 0.12f * fRib;
			xP.fHeight += 0.10f * fRib;
			// Side seams + hem stitching.
			HumanApplySeam(xP, HumanLine(fabsf(fSide) - 0.25f, 0.006f));
			HumanApplySeam(xP, HumanLine(fVN - 0.760f, 0.005f));
			// Soft chest shading.
			const float fPecs = HumanGauss(fabsf(fSide) - 0.075f, fVN - 0.16f, 0.06f, 0.10f);
			xP.fR += 0.02f * fPecs; xP.fG += 0.02f * fPecs; xP.fB += 0.02f * fPecs;
			return;
		}

		if (fVN < 0.815f)
		{
			// Leather belt with a metal buckle at the front.
			const float fGrain = HumanFBM2D(fUN * 60.0f, fVN * 60.0f, 2, 951u) - 0.5f;
			xP.fR = 0.16f + fGrain * 0.04f;
			xP.fG = 0.12f + fGrain * 0.03f;
			xP.fB = 0.09f + fGrain * 0.02f;
			xP.fHeight = 0.62f + fGrain * 0.05f;
			xP.fRough = 0.50f;
			xP.fMetal = 0.0f;
			const float fBuckle = HumanSmoothStep(0.026f, 0.020f, fabsf(fSide)) * HumanSmoothStep(0.815f, 0.795f, fVN) * HumanSmoothStep(0.773f, 0.788f, fVN);
			if (fBuckle > 0.0f)
			{
				xP.fR = glm::mix(xP.fR, 0.62f, fBuckle);
				xP.fG = glm::mix(xP.fG, 0.62f, fBuckle);
				xP.fB = glm::mix(xP.fB, 0.65f, fBuckle);
				xP.fRough = glm::mix(xP.fRough, 0.30f, fBuckle);
				xP.fMetal = glm::mix(xP.fMetal, 0.85f, fBuckle);
				xP.fHeight += 0.10f * fBuckle;
			}
			return;
		}

		// Trouser yoke below the belt: fly at the front, pocket arcs at the hips.
		HumanTrouserCloth(fUN, fVN, xP);
		HumanApplySeam(xP, HumanLine(fSide, 0.005f) * HumanSmoothStep(0.99f, 0.84f, fVN));   // fly
		const float fPocket = HumanLine(fabsf(fSide) - (0.13f + 0.35f * (fVN - 0.83f)), 0.006f)
		                    * HumanSmoothStep(0.82f, 0.86f, fVN) * HumanSmoothStep(0.97f, 0.93f, fVN);
		HumanApplySeam(xP, fPocket);
	});
}

//------------------------------------------------------------------------------
// Arm islands: shirt sleeve to mid-biceps (vN .285), bare skin below, with an
// elbow crease and a wristwatch on the left arm.
//------------------------------------------------------------------------------
void PaintHumanArm(HumanAtlas& xAtlas, const HumanUVIsland& xIsland, bool bLeft)
{
	PaintHumanIsland(xAtlas, xIsland, [bLeft](float fUN, float fVN, HumanPixel& xP)
	{
		if (fVN < 0.285f)
		{
			HumanShirtCloth(fUN + (bLeft ? 3.0f : 5.0f), fVN, xP);
			// Sleeve hem double-stitch.
			HumanApplySeam(xP, HumanLine(fVN - 0.270f, 0.006f));
			HumanApplySeam(xP, HumanLine(fVN - 0.282f, 0.004f));
			// Shoulder seam.
			HumanApplySeam(xP, HumanLine(fVN - 0.030f, 0.006f));
			return;
		}

		HumanSkinBase(fUN + (bLeft ? 7.0f : 9.0f), fVN, bLeft ? 17u : 19u, xP);

		// Elbow crease (front of the arm, around vN .53).
		const float fSide = fUN - 0.5f;
		const float fCrease = HumanLine(fVN - 0.535f, 0.008f) * HumanSmoothStep(0.30f, 0.12f, fabsf(fSide));
		xP.fR *= 1.0f - 0.12f * fCrease; xP.fG *= 1.0f - 0.13f * fCrease; xP.fB *= 1.0f - 0.13f * fCrease;
		xP.fHeight -= 0.08f * fCrease;

		// Fine arm hair on the forearm.
		const float fHairNoise = HumanSmoothStep(0.55f, 0.85f, HumanValueNoise2D(fUN * 240.0f, fVN * 240.0f, 23u));
		const float fForearm = HumanSmoothStep(0.58f, 0.68f, fVN) * 0.35f;
		xP.fR *= 1.0f - 0.10f * fHairNoise * fForearm;
		xP.fG *= 1.0f - 0.11f * fHairNoise * fForearm;
		xP.fB *= 1.0f - 0.12f * fHairNoise * fForearm;

		if (bLeft)
		{
			// Wristwatch: dark strap band + a glinting face on the outside.
			const float fStrap = HumanSmoothStep(0.935f, 0.945f, fVN) * HumanSmoothStep(0.985f, 0.975f, fVN);
			if (fStrap > 0.0f)
			{
				xP.fR = glm::mix(xP.fR, 0.10f, fStrap);
				xP.fG = glm::mix(xP.fG, 0.09f, fStrap);
				xP.fB = glm::mix(xP.fB, 0.085f, fStrap);
				xP.fRough = glm::mix(xP.fRough, 0.45f, fStrap);
				xP.fHeight += 0.10f * fStrap;
				const float fFace = HumanGauss(fUN - 0.25f, fVN - 0.96f, 0.030f, 0.012f);
				xP.fR = glm::mix(xP.fR, 0.70f, fFace);
				xP.fG = glm::mix(xP.fG, 0.71f, fFace);
				xP.fB = glm::mix(xP.fB, 0.73f, fFace);
				xP.fMetal = glm::mix(xP.fMetal, 0.9f, fFace);
				xP.fRough = glm::mix(xP.fRough, 0.15f, fFace);
			}
		}
	});
}

//------------------------------------------------------------------------------
// Leg islands: trousers with outer seams, knee wear, hem, and a sock band.
//------------------------------------------------------------------------------
void PaintHumanLeg(HumanAtlas& xAtlas, const HumanUVIsland& xIsland, bool bLeft)
{
	PaintHumanIsland(xAtlas, xIsland, [bLeft](float fUN, float fVN, HumanPixel& xP)
	{
		if (fVN > 0.930f)
		{
			// Sock between trouser hem and shoe.
			const float fRib = sinf(fVN * fHUMAN_TWO_PI * 140.0f) * 0.5f;
			xP.fR = 0.13f; xP.fG = 0.13f; xP.fB = 0.145f;
			xP.fHeight = 0.5f + fRib * 0.06f;
			xP.fRough = 0.85f;
			xP.fMetal = 0.0f;
			return;
		}

		HumanTrouserCloth(fUN + (bLeft ? 11.0f : 13.0f), fVN, xP);

		const float fSide = fUN - 0.5f;
		// Outer/inner seams run the length of the leg.
		HumanApplySeam(xP, HumanLine(fabsf(fSide) - 0.25f, 0.006f));
		// Hem stitch.
		HumanApplySeam(xP, HumanLine(fVN - 0.918f, 0.005f));
		// Knee wear: slightly lighter, smoother patch on the front.
		const float fKnee = HumanGauss(fSide, fVN - 0.46f, 0.16f, 0.060f);
		xP.fR += 0.030f * fKnee; xP.fG += 0.030f * fKnee; xP.fB += 0.030f * fKnee;
		xP.fRough -= 0.06f * fKnee;
		// Thigh/shin wrinkle accents.
		const float fWrk = HumanLine(fVN - 0.50f - 0.02f * sinf(fUN * fHUMAN_TWO_PI * 2.0f), 0.012f) * 0.5f;
		xP.fHeight -= 0.10f * fWrk;
	});
}

//------------------------------------------------------------------------------
// Hand islands: skin with finger grooves, knuckle creases and nails painted
// onto the mitt geometry.
//------------------------------------------------------------------------------
void PaintHumanHand(HumanAtlas& xAtlas, const HumanUVIsland& xIsland, bool bLeft)
{
	PaintHumanIsland(xAtlas, xIsland, [bLeft](float fUN, float fVN, HumanPixel& xP)
	{
		HumanSkinBase(fUN + (bLeft ? 15.0f : 17.0f), fVN, bLeft ? 29u : 31u, xP);

		// Finger separation grooves: four fingers across each broad face.
		// uN 0/.5 are the broad faces (front/back of the mitt).
		if (fVN > 0.42f)
		{
			float fGroove = 0.0f;
			for (int i = 1; i <= 3; i++)
			{
				const float fOff = static_cast<float>(i) * 0.125f;
				fGroove = std::max(fGroove, HumanLine(fUN - fOff, 0.012f));
				fGroove = std::max(fGroove, HumanLine(fUN - 0.5f - fOff, 0.012f));
			}
			fGroove *= HumanSmoothStep(0.42f, 0.50f, fVN);
			xP.fR *= 1.0f - 0.22f * fGroove; xP.fG *= 1.0f - 0.24f * fGroove; xP.fB *= 1.0f - 0.24f * fGroove;
			xP.fHeight -= 0.16f * fGroove;
		}

		// Knuckle bumps + creases at the finger base.
		const float fKnuckleRow = HumanLine(fVN - 0.46f, 0.018f);
		const float fBump = fKnuckleRow * (0.5f + 0.5f * sinf(fUN * fHUMAN_TWO_PI * 8.0f));
		xP.fHeight += 0.10f * fBump;
		const float fCrease = HumanLine(fVN - 0.58f, 0.006f) + HumanLine(fVN - 0.72f, 0.006f);
		xP.fHeight -= 0.07f * std::clamp(fCrease, 0.0f, 1.0f);

		// Nails near the fingertips on the back face (the uN seam side).
		const float fBackFace = HumanSmoothStep(0.25f, 0.10f, std::min(fUN, 1.0f - fUN));
		const float fNail = HumanSmoothStep(0.86f, 0.90f, fVN) * HumanSmoothStep(0.97f, 0.93f, fVN)
		                  * (0.5f + 0.5f * sinf(fUN * fHUMAN_TWO_PI * 8.0f)) * fBackFace;
		xP.fR = glm::mix(xP.fR, 0.85f, fNail * 0.6f);
		xP.fG = glm::mix(xP.fG, 0.72f, fNail * 0.6f);
		xP.fB = glm::mix(xP.fB, 0.62f, fNail * 0.6f);
		xP.fRough -= 0.15f * fNail;
	});
}

//------------------------------------------------------------------------------
// Foot islands: leather boots. uN 0 = sole centre, .5 = top of the foot;
// vN 0 = heel, 1 = toe.
//------------------------------------------------------------------------------
void PaintHumanFoot(HumanAtlas& xAtlas, const HumanUVIsland& xIsland, bool bLeft)
{
	PaintHumanIsland(xAtlas, xIsland, [bLeft](float fUN, float fVN, HumanPixel& xP)
	{
		const float fGrain = HumanFBM2D(fUN * 50.0f + (bLeft ? 0.0f : 31.0f), fVN * 50.0f, 3, 971u) - 0.5f;

		// Default: dark worn leather.
		xP.fR = 0.140f + fGrain * 0.035f;
		xP.fG = 0.118f + fGrain * 0.028f;
		xP.fB = 0.103f + fGrain * 0.024f;
		xP.fHeight = 0.5f + fGrain * 0.10f;
		xP.fRough = 0.42f;
		xP.fMetal = 0.0f;

		const float fSole = std::min(fUN, 1.0f - fUN);   // 0 at the sole seam column

		// Rubber sole band around the bottom edge.
		if (fSole < 0.115f)
		{
			const float fTread = (0.5f + 0.5f * sinf(fVN * fHUMAN_TWO_PI * 24.0f)) * HumanSmoothStep(0.06f, 0.02f, fSole);
			xP.fR = 0.205f + fGrain * 0.02f;
			xP.fG = 0.195f + fGrain * 0.02f;
			xP.fB = 0.185f + fGrain * 0.02f;
			xP.fHeight = 0.5f + fTread * 0.18f;
			xP.fRough = 0.88f;
			// Welt stripe where sole meets leather.
			const float fWelt = HumanLine(fSole - 0.115f, 0.012f);
			xP.fR = glm::mix(xP.fR, 0.62f, fWelt);
			xP.fG = glm::mix(xP.fG, 0.58f, fWelt);
			xP.fB = glm::mix(xP.fB, 0.52f, fWelt);
			xP.fHeight += 0.10f * fWelt;
			return;
		}

		// Toe cap: slightly lighter, glossier, with a curved seam.
		if (fVN > 0.72f)
		{
			xP.fR += 0.025f; xP.fG += 0.020f; xP.fB += 0.018f;
			xP.fRough = 0.36f;
		}
		HumanApplySeam(xP, HumanLine(fVN - 0.72f, 0.008f));
		// Heel counter seam.
		HumanApplySeam(xP, HumanLine(fVN - 0.13f, 0.008f));

		// Lacing panel on top of the foot: crossed laces + metal eyelets.
		const float fTop = HumanSmoothStep(0.16f, 0.08f, fabsf(fUN - 0.5f));
		if (fTop > 0.0f && fVN > 0.22f && fVN < 0.62f)
		{
			const float fLaceCoord = (fVN - 0.22f) / 0.40f;
			const float fZig = fabsf(std::fmod(fLaceCoord * 5.0f + (fUN - 0.5f) * 4.0f, 1.0f) - 0.5f);
			const float fZag = fabsf(std::fmod(fLaceCoord * 5.0f - (fUN - 0.5f) * 4.0f, 1.0f) - 0.5f);
			const float fLace = std::max(HumanSmoothStep(0.20f, 0.10f, fZig), HumanSmoothStep(0.20f, 0.10f, fZag)) * fTop;
			xP.fR = glm::mix(xP.fR, 0.78f, fLace * 0.85f);
			xP.fG = glm::mix(xP.fG, 0.75f, fLace * 0.85f);
			xP.fB = glm::mix(xP.fB, 0.70f, fLace * 0.85f);
			xP.fRough = glm::mix(xP.fRough, 0.78f, fLace);
			xP.fHeight += 0.14f * fLace;
			// Eyelets along the panel edges.
			const float fEyeletRow = 0.5f + 0.5f * cosf(fLaceCoord * fHUMAN_TWO_PI * 5.0f);
			const float fEyelet = HumanSmoothStep(0.92f, 0.99f, fEyeletRow) * HumanLine(fabsf(fUN - 0.5f) - 0.075f, 0.012f);
			xP.fR = glm::mix(xP.fR, 0.55f, fEyelet);
			xP.fG = glm::mix(xP.fG, 0.55f, fEyelet);
			xP.fB = glm::mix(xP.fB, 0.58f, fEyelet);
			xP.fMetal = glm::mix(xP.fMetal, 0.8f, fEyelet);
			xP.fRough = glm::mix(xP.fRough, 0.30f, fEyelet);
		}
	});
}

//------------------------------------------------------------------------------
// Post-paint passes + file export.
//------------------------------------------------------------------------------

// Flood island borders outward so bilinear sampling at island edges (and mip
// generation) never pulls in neutral background pixels.
void DilateHumanAtlas(HumanAtlas& xAtlas)
{
	constexpr int32_t iS = iHUMAN_ATLAS_SIZE;
	// Bleed is measured in texels, so doubling the atlas resolution needs ~2x the
	// passes to fill the same physical gutter between islands.
	for (u_int uPass = 0; uPass < 16; uPass++)
	{
		Zenith_Vector<u_int8> xNewMask(iS * iS);
		for (int32_t i = 0; i < iS * iS; i++) { xNewMask.PushBack(xAtlas.xPainted.Get(i)); }

		for (int32_t iY = 0; iY < iS; iY++)
		{
			for (int32_t iX = 0; iX < iS; iX++)
			{
				const int32_t i = iY * iS + iX;
				if (xAtlas.xPainted.Get(i))
				{
					continue;
				}
				const int32_t aiNeighbours[4] = {
					iY * iS + std::max(iX - 1, 0),
					iY * iS + std::min(iX + 1, iS - 1),
					std::max(iY - 1, 0) * iS + iX,
					std::min(iY + 1, iS - 1) * iS + iX,
				};
				for (int32_t iN : aiNeighbours)
				{
					if (xAtlas.xPainted.Get(iN))
					{
						xAtlas.xR.Get(i) = xAtlas.xR.Get(iN);
						xAtlas.xG.Get(i) = xAtlas.xG.Get(iN);
						xAtlas.xB.Get(i) = xAtlas.xB.Get(iN);
						xAtlas.xHeight.Get(i) = xAtlas.xHeight.Get(iN);
						xAtlas.xRough.Get(i) = xAtlas.xRough.Get(iN);
						xAtlas.xMetal.Get(i) = xAtlas.xMetal.Get(iN);
						xNewMask.Get(i) = 1;
						break;
					}
				}
			}
		}
		for (int32_t i = 0; i < iS * iS; i++) { xAtlas.xPainted.Get(i) = xNewMask.Get(i); }
	}
}

// Tiling micro-detail maps (skin pores / fabric weave): a BC5 detail normal plus
// a neutral detail albedo. Integer-frequency sine products tile seamlessly over
// [0,1) and the height->normal step wraps, so the material repeats them across
// the body via detail-UV tiling with no visible seams — the close-up surface
// "AAA" detail the macro atlas can't carry at body scale.
//
// The detail albedo is stored LINEAR (UNORM) centred on 0.5: the shader applies
// detail albedo as "x2 around mid-grey" (xAlbedo * detail * 2), so a 0.5 sample
// is an identity. The slot's default is white (1.0), which would DOUBLE albedo;
// supplying a neutral map (with only a faint tonal ripple) keeps base colour intact.
void GenerateStickFigureDetailMaps(const std::string& strDir)
{
	constexpr int32_t iN = 512;
	std::vector<float> xHeight(static_cast<size_t>(iN) * iN, 0.0f);
	for (int32_t iY = 0; iY < iN; iY++)
	{
		for (int32_t iX = 0; iX < iN; iX++)
		{
			const float fU = static_cast<float>(iX) / static_cast<float>(iN);
			const float fV = static_cast<float>(iY) / static_cast<float>(iN);
			float fH = 0.0f;
			fH += sinf(fU * fHUMAN_TWO_PI * 17.0f) * sinf(fV * fHUMAN_TWO_PI * 13.0f) * 0.50f;
			fH += sinf(fU * fHUMAN_TWO_PI * 31.0f + 1.7f) * sinf(fV * fHUMAN_TWO_PI * 29.0f) * 0.30f;
			fH += sinf(fU * fHUMAN_TWO_PI * 53.0f) * cosf(fV * fHUMAN_TWO_PI * 61.0f + 0.4f) * 0.20f;
			xHeight[static_cast<size_t>(iY) * iN + iX] = fH;
		}
	}

	Zenith_Vector<u_int8> xNormal(iN * iN * 4);
	Zenith_Vector<u_int8> xAlbedo(iN * iN * 4);
	for (int32_t iY = 0; iY < iN; iY++)
	{
		for (int32_t iX = 0; iX < iN; iX++)
		{
			const int32_t iXP = (iX + 1) % iN, iXM = (iX + iN - 1) % iN;   // wrap for tiling
			const int32_t iYP = (iY + 1) % iN, iYM = (iY + iN - 1) % iN;
			// Keep the slope small: at full strength the regular sine lattice reads
			// as an artificial cross-hatch on skin. This is a faint micro-break-up.
			const float fDX = (xHeight[static_cast<size_t>(iY) * iN + iXP] - xHeight[static_cast<size_t>(iY) * iN + iXM]) * 0.35f;
			const float fDY = (xHeight[static_cast<size_t>(iYP) * iN + iX] - xHeight[static_cast<size_t>(iYM) * iN + iX]) * 0.35f;
			const Zenith_Maths::Vector3 xNN = glm::normalize(Zenith_Maths::Vector3(-fDX, -fDY, 1.0f));
			xNormal.PushBack(static_cast<u_int8>((xNN.x * 0.5f + 0.5f) * 255.0f));
			xNormal.PushBack(static_cast<u_int8>((xNN.y * 0.5f + 0.5f) * 255.0f));
			xNormal.PushBack(static_cast<u_int8>((xNN.z * 0.5f + 0.5f) * 255.0f));
			xNormal.PushBack(255);

			// Neutral 0.5 (linear) +/- a faint tonal ripple — mean-neutral so the
			// x2 overlay is ~identity, with a touch of micro tonal variation.
			const float fTone = std::clamp(0.5f + xHeight[static_cast<size_t>(iY) * iN + iX] * 0.02f, 0.0f, 1.0f);
			const u_int8 uTone = static_cast<u_int8>(fTone * 255.0f);
			xAlbedo.PushBack(uTone); xAlbedo.PushBack(uTone); xAlbedo.PushBack(uTone); xAlbedo.PushBack(255);
		}
	}
	Zenith_Tools_TextureExport::ExportFromDataCompressed(
		xNormal.GetDataPointer(), strDir + "StickFigure_DetailNormal" ZENITH_TEXTURE_EXT, iN, iN, TextureCompressionMode::BC5);
	// Linear (UNORM), NOT sRGB — this is an overlay multiplier, not a displayed colour.
	Zenith_Tools_TextureExport::ExportFromDataV2Uncompressed(
		xAlbedo.GetDataPointer(), strDir + "StickFigure_DetailAlbedo" ZENITH_TEXTURE_EXT, iN, iN, TEXTURE_FORMAT_RGBA8_UNORM);
}

void GenerateStickFigureTextures(const std::string& strDir)
{
	HumanAtlas xAtlas;
	PaintHumanHead(xAtlas);
	PaintHumanTorso(xAtlas);
	PaintHumanArm(xAtlas, xUV_ARM_L, true);
	PaintHumanArm(xAtlas, xUV_ARM_R, false);
	PaintHumanLeg(xAtlas, xUV_LEG_L, true);
	PaintHumanLeg(xAtlas, xUV_LEG_R, false);
	PaintHumanHand(xAtlas, xUV_HAND_L, true);
	PaintHumanHand(xAtlas, xUV_HAND_R, false);
	PaintHumanFoot(xAtlas, xUV_FOOT_L, true);
	PaintHumanFoot(xAtlas, xUV_FOOT_R, false);
	DilateHumanAtlas(xAtlas);

	constexpr int32_t iS = iHUMAN_ATLAS_SIZE;

	// Albedo (sRGB). The cavity term now lives in the dedicated AO map (slot 3)
	// rather than being pre-multiplied into albedo — physically cleaner, lets the
	// lighting carry occlusion. Uncompressed sRGB with an offline mip chain (there
	// is no BC sRGB format in the pipeline, and 2048^2 RGBA8 is fine for one hero).
	Zenith_Vector<u_int8> xAlbedo(iS * iS * 4);
	for (int32_t i = 0; i < iS * iS; i++)
	{
		xAlbedo.PushBack(static_cast<u_int8>(std::clamp(xAtlas.xR.Get(i), 0.0f, 1.0f) * 255.0f));
		xAlbedo.PushBack(static_cast<u_int8>(std::clamp(xAtlas.xG.Get(i), 0.0f, 1.0f) * 255.0f));
		xAlbedo.PushBack(static_cast<u_int8>(std::clamp(xAtlas.xB.Get(i), 0.0f, 1.0f) * 255.0f));
		xAlbedo.PushBack(255);
	}
	Zenith_Tools_TextureExport::ExportFromDataV2Uncompressed(
		xAlbedo.GetDataPointer(), strDir + "StickFigure_Albedo" ZENITH_TEXTURE_EXT, iS, iS, TEXTURE_FORMAT_RGBA8_SRGB);

	// Normal map from the height field (central differences). The gradient scale
	// tracks the atlas resolution so apparent bump strength is independent of texel
	// size (smaller texels => smaller per-texel delta). Exported as BC5: only R,G
	// survive and the shader reconstructs Z (Common/Material.slang SampleNormalMap).
	const float fNormalScale = 2.2f * (static_cast<float>(iS) / 1024.0f);
	Zenith_Vector<u_int8> xNormal(iS * iS * 4);
	for (int32_t iY = 0; iY < iS; iY++)
	{
		for (int32_t iX = 0; iX < iS; iX++)
		{
			const int32_t iXP = std::min(iX + 1, iS - 1), iXM = std::max(iX - 1, 0);
			const int32_t iYP = std::min(iY + 1, iS - 1), iYM = std::max(iY - 1, 0);
			const float fDX = (xAtlas.xHeight.Get(iY * iS + iXP) - xAtlas.xHeight.Get(iY * iS + iXM)) * fNormalScale;
			const float fDY = (xAtlas.xHeight.Get(iYP * iS + iX) - xAtlas.xHeight.Get(iYM * iS + iX)) * fNormalScale;
			const Zenith_Maths::Vector3 xN = glm::normalize(Zenith_Maths::Vector3(-fDX, -fDY, 1.0f));
			xNormal.PushBack(static_cast<u_int8>((xN.x * 0.5f + 0.5f) * 255.0f));
			xNormal.PushBack(static_cast<u_int8>((xN.y * 0.5f + 0.5f) * 255.0f));
			xNormal.PushBack(static_cast<u_int8>((xN.z * 0.5f + 0.5f) * 255.0f));
			xNormal.PushBack(255);
		}
	}
	Zenith_Tools_TextureExport::ExportFromDataCompressed(
		xNormal.GetDataPointer(), strDir + "StickFigure_Normal" ZENITH_TEXTURE_EXT, iS, iS, TextureCompressionMode::BC5);

	// Roughness/metallic — glTF packing read by SampleRoughnessMetallic: G=rough,
	// B=metal. BC3 (BC1 colour block + BC4 alpha); G/B ride the colour block.
	Zenith_Vector<u_int8> xRM(iS * iS * 4);
	for (int32_t i = 0; i < iS * iS; i++)
	{
		const float fRough = std::clamp(xAtlas.xRough.Get(i), 0.0f, 1.0f);
		// R = per-texel specular (read by the skin shader path): glossy regions
		// (wet lips/eyes, low roughness) get higher F0, matte cheeks/cloth lower.
		const float fSpec = std::clamp(0.32f + (1.0f - fRough) * 0.58f, 0.0f, 1.0f);
		xRM.PushBack(static_cast<u_int8>(fSpec * 255.0f));
		xRM.PushBack(static_cast<u_int8>(fRough * 255.0f));
		xRM.PushBack(static_cast<u_int8>(std::clamp(xAtlas.xMetal.Get(i), 0.0f, 1.0f) * 255.0f));
		xRM.PushBack(255);
	}
	Zenith_Tools_TextureExport::ExportFromDataCompressed(
		xRM.GetDataPointer(), strDir + "StickFigure_RM" ZENITH_TEXTURE_EXT, iS, iS, TextureCompressionMode::BC3);

	// Ambient occlusion (slot 3, shader reads .r) — the soft cavity term albedo
	// used to bake in, now a proper map so lighting carries it. BC1 grayscale.
	Zenith_Vector<u_int8> xAO(iS * iS * 4);
	for (int32_t i = 0; i < iS * iS; i++)
	{
		const float fCavity = 1.0f - 0.25f * std::clamp(0.5f - xAtlas.xHeight.Get(i), 0.0f, 0.5f);
		const u_int8 uAO = static_cast<u_int8>(std::clamp(fCavity, 0.0f, 1.0f) * 255.0f);
		xAO.PushBack(uAO); xAO.PushBack(uAO); xAO.PushBack(uAO); xAO.PushBack(255);
	}
	Zenith_Tools_TextureExport::ExportFromDataCompressed(
		xAO.GetDataPointer(), strDir + "StickFigure_AO" ZENITH_TEXTURE_EXT, iS, iS, TextureCompressionMode::BC1);

	// Height (slot 5, shader reads .r) — drives subtle parallax-occlusion mapping
	// on cloth/leather. Same field the normal map derives from. BC1 grayscale.
	Zenith_Vector<u_int8> xHeightTex(iS * iS * 4);
	for (int32_t i = 0; i < iS * iS; i++)
	{
		const u_int8 uH = static_cast<u_int8>(std::clamp(xAtlas.xHeight.Get(i), 0.0f, 1.0f) * 255.0f);
		xHeightTex.PushBack(uH); xHeightTex.PushBack(uH); xHeightTex.PushBack(uH); xHeightTex.PushBack(255);
	}
	Zenith_Tools_TextureExport::ExportFromDataCompressed(
		xHeightTex.GetDataPointer(), strDir + "StickFigure_Height" ZENITH_TEXTURE_EXT, iS, iS, TextureCompressionMode::BC1);

	// Tiling micro-detail maps (slots 6/7) for close-up pore/weave surface.
	GenerateStickFigureDetailMaps(strDir);
}

void GenerateStickFigureMaterial(const std::string& strDir)
{
	Zenith_MaterialAsset* pxBody = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	pxBody->SetName("StickFigureBody");
	const std::string strBase = "engine:Meshes/StickFigure/";
	pxBody->SetDiffuseTexture(TextureHandle(strBase + "StickFigure_Albedo" ZENITH_TEXTURE_EXT));
	pxBody->SetNormalTexture(TextureHandle(strBase + "StickFigure_Normal" ZENITH_TEXTURE_EXT));
	pxBody->SetRoughnessMetallicTexture(TextureHandle(strBase + "StickFigure_RM" ZENITH_TEXTURE_EXT));
	pxBody->SetTexture(MATERIAL_TEXTURE_OCCLUSION, TextureHandle(strBase + "StickFigure_AO" ZENITH_TEXTURE_EXT));
	pxBody->SetTexture(MATERIAL_TEXTURE_HEIGHT, TextureHandle(strBase + "StickFigure_Height" ZENITH_TEXTURE_EXT));
	pxBody->SetTexture(MATERIAL_TEXTURE_DETAIL_NORMAL, TextureHandle(strBase + "StickFigure_DetailNormal" ZENITH_TEXTURE_EXT));
	pxBody->SetTexture(MATERIAL_TEXTURE_DETAIL_ALBEDO, TextureHandle(strBase + "StickFigure_DetailAlbedo" ZENITH_TEXTURE_EXT));

	// RM multipliers stay 1.0 so the texture's per-region values pass through.
	pxBody->SetRoughness(1.0f);
	pxBody->SetMetallic(1.0f);
	pxBody->SetShadingModel(MATERIAL_SHADING_SUBSURFACE);   // skin: wrap diffuse + warm scatter terminator
	pxBody->SetSpecular(1.0f);            // multiplier=1: the per-texel specular in RM.R passes through
	pxBody->SetNormalStrength(1.0f);
	pxBody->SetOcclusionStrength(1.0f);
	// Parallax-occlusion mapping, kept SUBTLE on a skinned body to avoid limb-bend
	// / seam artefacts. POM auto-enables once a height texture + non-zero height
	// scale are set (Flux_MaterialBinding::BuildMaterialDrawFlags). POM step counts
	// keep their defaults (8/32).
	pxBody->SetHeightScale(0.02f);
	// Detail maps auto-enable on detail-texture presence; repeat the micro-surface
	// several times across each UV island.
	pxBody->SetDetailTiling(Zenith_Maths::Vector2(10.0f, 10.0f));

	pxBody->SaveToFile(strDir + "StickFigure_Body.zmtrl");

	// --- Eye material: a glossy clear-coat cornea (default-lit, NOT subsurface).
	// The eyeball geometry's UVs sample the painted iris/sclera from the shared
	// albedo atlas; a low base roughness + a sharp clear-coat lobe give a live,
	// view-dependent cornea highlight from the sun + IBL instead of a flat painted
	// catchlight. ---
	Zenith_MaterialAsset* pxEye = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	pxEye->SetName("StickFigureEye");
	pxEye->SetDiffuseTexture(TextureHandle(strBase + "StickFigure_Albedo" ZENITH_TEXTURE_EXT));
	pxEye->SetRoughness(0.10f);             // wet glossy sphere (default-white RM * 0.10)
	pxEye->SetMetallic(0.0f);
	pxEye->SetSpecular(0.55f);
	pxEye->SetClearCoatStrength(0.9f);      // sharp cornea highlight
	pxEye->SetClearCoatRoughness(0.04f);
	pxEye->SaveToFile(strDir + "StickFigure_Eye.zmtrl");
}

// The canonical model bundle: mesh + skeleton + body material. Exported every
// boot (overwriting any stale material-less .zmodel the games' create-if-
// missing fallbacks produced in the past).
void ExportStickFigureModel(const std::string& strDir, const std::string& strSkelPath,
                            const std::string& strMeshAssetPath)
{
	Zenith_ModelAsset* pxModel = Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
	pxModel->SetName("StickFigure");
	pxModel->SetSkeletonPath(strSkelPath);

	Zenith_Vector<std::string> xMaterials;
	xMaterials.PushBack("engine:Meshes/StickFigure/StickFigure_Body.zmtrl");   // submesh 0: body
	xMaterials.PushBack("engine:Meshes/StickFigure/StickFigure_Eye.zmtrl");    // submesh 1: eyeballs
	pxModel->AddMeshByPath(strMeshAssetPath, xMaterials);
	pxModel->Export((strDir + "StickFigure" ZENITH_MODEL_EXT).c_str());
}

} // namespace

#endif // ZENITH_TOOLS

//==============================================================================
// Animation authoring
//
// Conventions for this rig (forward = +Z, bind pose = arms at the sides):
//   UpperLeg  -X swings the leg forward, +X back
//   LowerLeg  +X is knee flexion (heel toward the seat)
//   Foot      +X is plantarflexion (toes down), -X dorsiflexion
//   UpperArm  -X swings the arm forward; +-Z abducts away from the body
//   LowerArm  -X is elbow flexion (forearm raises forward)
//   Spine     +X leans forward, +-Y twists, +Z tips the shoulders left
//   Root      position channel translates the pelvis (model space)
//
// Looping clips (Idle/Walk/Run/Aim) are SAMPLED from continuous gait curves at
// 25 keys so the slerp playback follows the curve closely; the curves are
// built from sin/cos of the cycle phase so key 0 == key N and the loop never
// pops. Action clips are key-posed with anticipation/strike/recovery timing.
// Channels REPLACE the bind-pose local TRS (engine sampler contract), so
// rotation keys are absolute local rotations and Root/Spine position keys are
// based on their bind-local positions ((0,0,0) and (0,0.5,0)).
//==============================================================================
namespace
{

Zenith_Maths::Quat HumanRotX(float fDeg) { return glm::angleAxis(glm::radians(fDeg), Zenith_Maths::Vector3(1, 0, 0)); }
Zenith_Maths::Quat HumanRotY(float fDeg) { return glm::angleAxis(glm::radians(fDeg), Zenith_Maths::Vector3(0, 1, 0)); }
Zenith_Maths::Quat HumanRotZ(float fDeg) { return glm::angleAxis(glm::radians(fDeg), Zenith_Maths::Vector3(0, 0, 1)); }

// Sample a continuous curve into a bone rotation channel.
// TFn: Zenith_Maths::Quat(float fT01).
template <typename TFn>
void HumanAddRotCurve(Flux_AnimationClip* pxClip, const char* szBone, float fTotalTicks, u_int uKeys, TFn&& xFn)
{
	Flux_BoneChannel xChannel;
	for (u_int u = 0; u < uKeys; u++)
	{
		const float fT = static_cast<float>(u) / static_cast<float>(uKeys - 1);
		xChannel.AddRotationKeyframe(fT * fTotalTicks, xFn(fT));
	}
	xChannel.SortKeyframes();
	pxClip->AddBoneChannel(szBone, std::move(xChannel));
}

// TFn: Zenith_Maths::Vector3(float fT01).
template <typename TFn>
void HumanAddPosCurve(Flux_AnimationClip* pxClip, const char* szBone, float fTotalTicks, u_int uKeys, TFn&& xFn)
{
	Flux_BoneChannel xChannel;
	for (u_int u = 0; u < uKeys; u++)
	{
		const float fT = static_cast<float>(u) / static_cast<float>(uKeys - 1);
		xChannel.AddPositionKeyframe(fT * fTotalTicks, xFn(fT));
	}
	xChannel.SortKeyframes();
	pxClip->AddBoneChannel(szBone, std::move(xChannel));
}

// Explicit key-pose channels for the action clips.
struct HumanRotKey { float fTick; Zenith_Maths::Quat xRot; };
struct HumanPosKey { float fTick; Zenith_Maths::Vector3 xPos; };

void HumanAddRotKeys(Flux_AnimationClip* pxClip, const char* szBone, const HumanRotKey* pxKeys, u_int uCount)
{
	Flux_BoneChannel xChannel;
	for (u_int u = 0; u < uCount; u++)
	{
		xChannel.AddRotationKeyframe(pxKeys[u].fTick, pxKeys[u].xRot);
	}
	xChannel.SortKeyframes();
	pxClip->AddBoneChannel(szBone, std::move(xChannel));
}

void HumanAddPosKeys(Flux_AnimationClip* pxClip, const char* szBone, const HumanPosKey* pxKeys, u_int uCount)
{
	Flux_BoneChannel xChannel;
	for (u_int u = 0; u < uCount; u++)
	{
		xChannel.AddPositionKeyframe(pxKeys[u].fTick, pxKeys[u].xPos);
	}
	xChannel.SortKeyframes();
	pxClip->AddBoneChannel(szBone, std::move(xChannel));
}

// Periodic raised-cosine bump centred at fCentre01 with half-width fWidth01
// (both in cycle fractions). Used for the knee/ankle gait events.
float HumanGaitBump(float fPhase01, float fCentre01, float fWidth01)
{
	float fD = fPhase01 - fCentre01;
	fD -= std::floor(fD + 0.5f);   // wrap to [-0.5, 0.5)
	const float fX = fD / fWidth01;
	if (fabsf(fX) >= 1.0f)
	{
		return 0.0f;
	}
	return 0.5f + 0.5f * cosf(fX * fHUMAN_PI);
}

// Shared full-body walk/run cycle. fStride scales the leg/arm amplitudes,
// fLean the forward lean, fBob the pelvis bob.
void HumanBuildGaitClip(Flux_AnimationClip* pxClip, float fTicks,
                        float fHipFwd, float fHipBack, float fKneeStance, float fKneeSwing,
                        float fArmSwing, float fElbowBase, float fElbowPump,
                        float fLean, float fBob, float fYaw)
{
	constexpr u_int uKEYS = 25;

	// Legs: left leg phase 0 (heel strike when the leg is forward), right +0.5.
	for (u_int uSide = 0; uSide < 2; uSide++)
	{
		const float fPhase = (uSide == 0) ? 0.0f : 0.5f;
		const char* szUpper = (uSide == 0) ? "LeftUpperLeg" : "RightUpperLeg";
		const char* szLower = (uSide == 0) ? "LeftLowerLeg" : "RightLowerLeg";
		const char* szFoot = (uSide == 0) ? "LeftFoot" : "RightFoot";

		HumanAddRotCurve(pxClip, szUpper, fTicks, uKEYS, [=](float fT)
		{
			const float fP = fT + fPhase;
			// cos = +1 at heel strike: swing forward (-X) at phase 0.
			const float fMid = 0.5f * (fHipBack - fHipFwd);
			const float fAmp = 0.5f * (fHipBack + fHipFwd);
			return HumanRotX(fMid - fAmp * cosf(fP * fHUMAN_TWO_PI));
		});
		HumanAddRotCurve(pxClip, szLower, fTicks, uKEYS, [=](float fT)
		{
			const float fP = fT + fPhase;
			const float fFlex = 4.0f
				+ fKneeStance * HumanGaitBump(fP, 0.16f, 0.14f)
				+ fKneeSwing * HumanGaitBump(fP, 0.72f, 0.20f);
			return HumanRotX(fFlex);
		});
		HumanAddRotCurve(pxClip, szFoot, fTicks, uKEYS, [=](float fT)
		{
			const float fP = fT + fPhase;
			const float fAngle = -7.0f * HumanGaitBump(fP, 0.02f, 0.10f)     // heel-strike dorsiflex
				+ 14.0f * HumanGaitBump(fP, 0.52f, 0.14f)                    // toe-off push
				- 6.0f * HumanGaitBump(fP, 0.80f, 0.14f);                    // swing clearance
			return HumanRotX(fAngle);
		});
	}

	// Arms: contralateral (left arm forward with the right leg).
	for (u_int uSide = 0; uSide < 2; uSide++)
	{
		const float fSign = (uSide == 0) ? 1.0f : -1.0f;   // phase flip via cos sign
		const char* szUpper = (uSide == 0) ? "LeftUpperArm" : "RightUpperArm";
		const char* szLower = (uSide == 0) ? "LeftLowerArm" : "RightLowerArm";
		const float fOut = (uSide == 0) ? -3.5f : 3.5f;    // arms hang slightly out

		HumanAddRotCurve(pxClip, szUpper, fTicks, uKEYS, [=](float fT)
		{
			const float fSwing = -2.0f + fSign * fArmSwing * cosf(fT * fHUMAN_TWO_PI);
			return HumanRotX(fSwing) * HumanRotZ(fOut);
		});
		HumanAddRotCurve(pxClip, szLower, fTicks, uKEYS, [=](float fT)
		{
			// More elbow bend while the arm swings forward.
			const float fFwd = 0.5f * (1.0f - fSign * cosf(fT * fHUMAN_TWO_PI));
			return HumanRotX(-fElbowBase - fElbowPump * fFwd);
		});
	}

	// Pelvis: vertical bob (2 per cycle), lateral sway toward the stance leg,
	// counter-rotating yaw.
	HumanAddPosCurve(pxClip, "Root", fTicks, uKEYS, [=](float fT)
	{
		const float fP = fT * fHUMAN_TWO_PI;
		return Zenith_Maths::Vector3(
			-0.012f * sinf(fP),
			-0.4f * fBob + fBob * cosf(2.0f * fP + 0.35f),
			0.0f);
	});
	HumanAddRotCurve(pxClip, "Root", fTicks, uKEYS, [=](float fT)
	{
		const float fP = fT * fHUMAN_TWO_PI;
		return HumanRotY(fYaw * cosf(fP)) * HumanRotZ(1.6f * sinf(fP));
	});

	// Spine counters the pelvis and leans into the motion; the head stabilizes.
	HumanAddRotCurve(pxClip, "Spine", fTicks, uKEYS, [=](float fT)
	{
		const float fP = fT * fHUMAN_TWO_PI;
		return HumanRotX(fLean) * HumanRotY(-1.4f * fYaw * cosf(fP)) * HumanRotZ(-1.2f * sinf(fP));
	});
	HumanAddRotCurve(pxClip, "Head", fTicks, uKEYS, [=](float fT)
	{
		const float fP = fT * fHUMAN_TWO_PI;
		return HumanRotX(-fLean * 0.55f + 0.8f * cosf(2.0f * fP)) * HumanRotY(0.45f * fYaw * cosf(fP));
	});
	HumanAddRotCurve(pxClip, "Neck", fTicks, uKEYS, [=](float fT)
	{
		const float fP = fT * fHUMAN_TWO_PI;
		return HumanRotX(-fLean * 0.25f + 0.4f * cosf(2.0f * fP + 0.5f));
	});
}

} // namespace

//------------------------------------------------------------------------------
// Looping locomotion clips.
//------------------------------------------------------------------------------

static Flux_AnimationClip* CreateIdleAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Idle");
	pxClip->SetDuration(2.0f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(true);

	constexpr float fTICKS = 48.0f;
	constexpr u_int uKEYS = 25;

	// Breathing: the whole upper body rides the Spine bone, so a small lift +
	// pitch reads as a chest rise. Bind-local Spine position is (0, 0.5, 0).
	HumanAddPosCurve(pxClip, "Spine", fTICKS, uKEYS, [](float fT)
	{
		return Zenith_Maths::Vector3(0.0f, 0.5f + 0.007f * sinf(fT * fHUMAN_TWO_PI), 0.0f);
	});
	HumanAddRotCurve(pxClip, "Spine", fTICKS, uKEYS, [](float fT)
	{
		const float fP = fT * fHUMAN_TWO_PI;
		return HumanRotX(1.1f * sinf(fP + 0.3f)) * HumanRotZ(0.5f * sinf(fP));
	});

	// Slow weight shift on the pelvis.
	HumanAddPosCurve(pxClip, "Root", fTICKS, uKEYS, [](float fT)
	{
		const float fP = fT * fHUMAN_TWO_PI;
		return Zenith_Maths::Vector3(0.009f * sinf(fP), -0.004f + 0.004f * cosf(fP), 0.0f);
	});
	HumanAddRotCurve(pxClip, "Root", fTICKS, uKEYS, [](float fT)
	{
		return HumanRotZ(0.9f * sinf(fT * fHUMAN_TWO_PI));
	});

	// Relaxed arms: slight outward hang, elbows soft, gentle drift.
	for (u_int uSide = 0; uSide < 2; uSide++)
	{
		const float fOut = (uSide == 0) ? -4.0f : 4.0f;
		const float fPhase = (uSide == 0) ? 0.9f : 2.1f;
		const char* szUpper = (uSide == 0) ? "LeftUpperArm" : "RightUpperArm";
		const char* szLower = (uSide == 0) ? "LeftLowerArm" : "RightLowerArm";
		HumanAddRotCurve(pxClip, szUpper, fTICKS, uKEYS, [=](float fT)
		{
			return HumanRotX(1.6f * sinf(fT * fHUMAN_TWO_PI + fPhase)) * HumanRotZ(fOut);
		});
		HumanAddRotCurve(pxClip, szLower, fTICKS, uKEYS, [=](float fT)
		{
			return HumanRotX(-8.0f - 1.5f * sinf(fT * fHUMAN_TWO_PI + fPhase));
		});
	}

	// Head: slow attentive drift.
	HumanAddRotCurve(pxClip, "Head", fTICKS, uKEYS, [](float fT)
	{
		const float fP = fT * fHUMAN_TWO_PI;
		return HumanRotY(2.4f * sinf(fP)) * HumanRotX(-1.0f + 0.9f * sinf(fP + 0.7f));
	});

	return pxClip;
}

static Flux_AnimationClip* CreateWalkAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Walk");
	pxClip->SetDuration(1.0f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(true);

	HumanBuildGaitClip(pxClip, 24.0f,
		/*hipFwd*/ 28.0f, /*hipBack*/ 18.0f,
		/*kneeStance*/ 13.0f, /*kneeSwing*/ 52.0f,
		/*armSwing*/ 21.0f, /*elbowBase*/ 16.0f, /*elbowPump*/ 13.0f,
		/*lean*/ 3.5f, /*bob*/ 0.018f, /*yaw*/ 4.5f);

	return pxClip;
}

static Flux_AnimationClip* CreateRunAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Run");
	pxClip->SetDuration(0.5f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(true);

	HumanBuildGaitClip(pxClip, 12.0f,
		/*hipFwd*/ 50.0f, /*hipBack*/ 26.0f,
		/*kneeStance*/ 22.0f, /*kneeSwing*/ 82.0f,
		/*armSwing*/ 34.0f, /*elbowBase*/ 62.0f, /*elbowPump*/ 18.0f,
		/*lean*/ 11.0f, /*bob*/ 0.034f, /*yaw*/ 7.5f);

	return pxClip;
}

//------------------------------------------------------------------------------
// Combat action clips. Authored with anticipation -> strike -> follow-through
// -> recovery key timing; all channels return to identity so the Combat state
// machine blends cleanly back to Idle/Walk. Durations are pinned by the
// Combat hit windows (30-70% normalized time).
//------------------------------------------------------------------------------

static Flux_AnimationClip* CreateAttack1Animation()
{
	// Right straight jab: chamber, drive off the hips, snap back.
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Attack1");
	pxClip->SetDuration(0.4f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Quat xId = glm::identity<Zenith_Maths::Quat>();

	{
		const HumanRotKey ax[] = {
			{ 0.0f, xId },
			{ 1.5f, HumanRotX(16.0f) * HumanRotY(8.0f) },               // chamber back
			{ 3.8f, HumanRotX(-86.0f) * HumanRotY(-10.0f) },            // full extension
			{ 5.2f, HumanRotX(-70.0f) },                                // follow-through
			{ 7.5f, HumanRotX(-18.0f) },
			{ 9.5f, xId },
		};
		HumanAddRotKeys(pxClip, "RightUpperArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f, HumanRotX(-12.0f) },
			{ 1.5f, HumanRotX(-78.0f) },                                // cocked
			{ 3.8f, HumanRotX(-4.0f) },                                 // arm straight at impact
			{ 5.2f, HumanRotX(-14.0f) },
			{ 7.5f, HumanRotX(-32.0f) },
			{ 9.5f, xId },
		};
		HumanAddRotKeys(pxClip, "RightLowerArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		// Left arm stays up in guard through the punch.
		const HumanRotKey ax[] = {
			{ 0.0f, xId },
			{ 1.5f, HumanRotX(-36.0f) },
			{ 6.0f, HumanRotX(-32.0f) },
			{ 9.5f, xId },
		};
		HumanAddRotKeys(pxClip, "LeftUpperArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f, HumanRotX(-10.0f) },
			{ 1.5f, HumanRotX(-85.0f) },
			{ 6.0f, HumanRotX(-80.0f) },
			{ 9.5f, xId },
		};
		HumanAddRotKeys(pxClip, "LeftLowerArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		// Hips/shoulders drive the punch: wind up, rotate through, recover.
		const HumanRotKey ax[] = {
			{ 0.0f, xId },
			{ 1.5f, HumanRotY(10.0f) * HumanRotX(2.0f) },               // wind up (right shoulder back)
			{ 3.8f, HumanRotY(-16.0f) * HumanRotX(11.0f) },             // rotate into the punch
			{ 6.5f, HumanRotY(-8.0f) * HumanRotX(6.0f) },
			{ 9.5f, xId },
		};
		HumanAddRotKeys(pxClip, "Spine", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f, xId },
			{ 3.8f, HumanRotX(-4.0f) * HumanRotY(4.0f) },               // chin tucked, eyes on target
			{ 9.5f, xId },
		};
		HumanAddRotKeys(pxClip, "Head", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		// Small forward lunge + dip.
		const HumanPosKey ax[] = {
			{ 0.0f, { 0.0f, 0.0f, 0.0f } },
			{ 3.8f, { 0.0f, -0.035f, 0.05f } },
			{ 7.0f, { 0.0f, -0.015f, 0.02f } },
			{ 9.5f, { 0.0f, 0.0f, 0.0f } },
		};
		HumanAddPosKeys(pxClip, "Root", ax, sizeof(ax) / sizeof(ax[0]));
	}

	return pxClip;
}

static Flux_AnimationClip* CreateAttack2Animation()
{
	// Left hook: weight transfer, arcing swing across the body.
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Attack2");
	pxClip->SetDuration(0.4f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Quat xId = glm::identity<Zenith_Maths::Quat>();

	{
		const HumanRotKey ax[] = {
			{ 0.0f, xId },
			{ 1.8f, HumanRotX(14.0f) * HumanRotY(-14.0f) * HumanRotZ(-12.0f) },   // wind back + out
			{ 4.2f, HumanRotX(-72.0f) * HumanRotY(38.0f) },                       // hook arcs across
			{ 5.6f, HumanRotX(-58.0f) * HumanRotY(46.0f) },                       // follow-through
			{ 7.8f, HumanRotX(-16.0f) * HumanRotY(12.0f) },
			{ 9.5f, xId },
		};
		HumanAddRotKeys(pxClip, "LeftUpperArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f, HumanRotX(-10.0f) },
			{ 1.8f, HumanRotX(-64.0f) },
			{ 4.2f, HumanRotX(-78.0f) },                                          // elbow stays bent through a hook
			{ 7.8f, HumanRotX(-26.0f) },
			{ 9.5f, xId },
		};
		HumanAddRotKeys(pxClip, "LeftLowerArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		// Right guard.
		const HumanRotKey ax[] = {
			{ 0.0f, xId },
			{ 1.8f, HumanRotX(-34.0f) },
			{ 6.0f, HumanRotX(-30.0f) },
			{ 9.5f, xId },
		};
		HumanAddRotKeys(pxClip, "RightUpperArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f, HumanRotX(-10.0f) },
			{ 1.8f, HumanRotX(-82.0f) },
			{ 6.0f, HumanRotX(-76.0f) },
			{ 9.5f, xId },
		};
		HumanAddRotKeys(pxClip, "RightLowerArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f, xId },
			{ 1.8f, HumanRotY(-12.0f) * HumanRotZ(-3.0f) },                       // coil away
			{ 4.2f, HumanRotY(20.0f) * HumanRotX(7.0f) * HumanRotZ(4.0f) },       // whip through
			{ 6.5f, HumanRotY(11.0f) * HumanRotX(4.0f) },
			{ 9.5f, xId },
		};
		HumanAddRotKeys(pxClip, "Spine", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f, xId },
			{ 4.2f, HumanRotY(-9.0f) * HumanRotX(-3.0f) },
			{ 9.5f, xId },
		};
		HumanAddRotKeys(pxClip, "Head", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		// Weight transfers onto the lead (right) foot through the hook.
		const HumanPosKey ax[] = {
			{ 0.0f, { 0.0f, 0.0f, 0.0f } },
			{ 1.8f, { -0.025f, -0.02f, 0.0f } },
			{ 4.2f, { 0.035f, -0.04f, 0.03f } },
			{ 7.5f, { 0.015f, -0.015f, 0.01f } },
			{ 9.5f, { 0.0f, 0.0f, 0.0f } },
		};
		HumanAddPosKeys(pxClip, "Root", ax, sizeof(ax) / sizeof(ax[0]));
	}

	return pxClip;
}

static Flux_AnimationClip* CreateAttack3Animation()
{
	// Two-handed overhead smash with a forward hop. Ends with the same small
	// forward displacement the old clip established (Root z 0.1).
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Attack3");
	pxClip->SetDuration(0.5f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Quat xId = glm::identity<Zenith_Maths::Quat>();

	for (u_int uSide = 0; uSide < 2; uSide++)
	{
		const char* szUpper = (uSide == 0) ? "LeftUpperArm" : "RightUpperArm";
		const char* szLower = (uSide == 0) ? "LeftLowerArm" : "RightLowerArm";
		const float fOut = (uSide == 0) ? -10.0f : 10.0f;
		{
			const HumanRotKey ax[] = {
				{ 0.0f, xId },
				{ 2.0f, HumanRotX(-60.0f) * HumanRotZ(fOut * 0.5f) },
				{ 4.5f, HumanRotX(-152.0f) * HumanRotZ(fOut) },        // arms overhead
				{ 6.0f, HumanRotX(-148.0f) * HumanRotZ(fOut) },        // hang at the top
				{ 7.8f, HumanRotX(38.0f) },                            // slammed down past the hips
				{ 9.5f, HumanRotX(22.0f) },
				{ 12.0f, xId },
			};
			HumanAddRotKeys(pxClip, szUpper, ax, sizeof(ax) / sizeof(ax[0]));
		}
		{
			const HumanRotKey ax[] = {
				{ 0.0f, HumanRotX(-8.0f) },
				{ 4.5f, HumanRotX(-42.0f) },                           // elbows soften overhead
				{ 7.8f, HumanRotX(-6.0f) },                            // straight through the strike
				{ 12.0f, xId },
			};
			HumanAddRotKeys(pxClip, szLower, ax, sizeof(ax) / sizeof(ax[0]));
		}
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f, xId },
			{ 4.5f, HumanRotX(-20.0f) },                               // arch back under the raise
			{ 7.8f, HumanRotX(34.0f) },                                // crunch into the slam
			{ 10.0f, HumanRotX(12.0f) },
			{ 12.0f, xId },
		};
		HumanAddRotKeys(pxClip, "Spine", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f, xId },
			{ 4.5f, HumanRotX(-14.0f) },
			{ 7.8f, HumanRotX(10.0f) },
			{ 12.0f, xId },
		};
		HumanAddRotKeys(pxClip, "Head", ax, sizeof(ax) / sizeof(ax[0]));
	}
	// Legs load the hop and absorb the landing.
	for (u_int uSide = 0; uSide < 2; uSide++)
	{
		const char* szUpper = (uSide == 0) ? "LeftUpperLeg" : "RightUpperLeg";
		const char* szLower = (uSide == 0) ? "LeftLowerLeg" : "RightLowerLeg";
		{
			const HumanRotKey ax[] = {
				{ 0.0f, xId },
				{ 3.0f, HumanRotX(14.0f) },
				{ 7.8f, HumanRotX(-22.0f) },
				{ 9.5f, HumanRotX(20.0f) },
				{ 12.0f, xId },
			};
			HumanAddRotKeys(pxClip, szUpper, ax, sizeof(ax) / sizeof(ax[0]));
		}
		{
			const HumanRotKey ax[] = {
				{ 0.0f, xId },
				{ 3.0f, HumanRotX(24.0f) },
				{ 7.8f, HumanRotX(8.0f) },
				{ 9.5f, HumanRotX(34.0f) },
				{ 12.0f, xId },
			};
			HumanAddRotKeys(pxClip, szLower, ax, sizeof(ax) / sizeof(ax[0]));
		}
	}
	{
		const HumanPosKey ax[] = {
			{ 0.0f, { 0.0f, 0.0f, 0.0f } },
			{ 3.0f, { 0.0f, -0.05f, 0.0f } },                          // load
			{ 6.0f, { 0.0f, 0.09f, 0.10f } },                          // hop up + forward
			{ 7.8f, { 0.0f, -0.09f, 0.14f } },                         // drive down with the strike
			{ 10.0f, { 0.0f, -0.03f, 0.11f } },
			{ 12.0f, { 0.0f, 0.0f, 0.10f } },                          // ends a step forward (legacy contract)
		};
		HumanAddPosKeys(pxClip, "Root", ax, sizeof(ax) / sizeof(ax[0]));
	}

	return pxClip;
}

static Flux_AnimationClip* CreateDodgeAnimation()
{
	// Side-step dodge to the right: crouching push, lean into the motion,
	// trailing leg crossing behind. Ends displaced (Root x 0.8, legacy contract).
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Dodge");
	pxClip->SetDuration(0.5f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Quat xId = glm::identity<Zenith_Maths::Quat>();

	{
		const HumanPosKey ax[] = {
			{ 0.0f, { 0.0f, 0.0f, 0.0f } },
			{ 2.5f, { 0.12f, -0.10f, 0.0f } },                         // load sideways
			{ 5.5f, { 0.52f, -0.20f, 0.0f } },                         // push
			{ 9.0f, { 0.78f, -0.06f, 0.0f } },
			{ 12.0f, { 0.80f, 0.0f, 0.0f } },
		};
		HumanAddPosKeys(pxClip, "Root", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		// Lean INTO the dodge (top of the spine toward +X).
		const HumanRotKey ax[] = {
			{ 0.0f, xId },
			{ 3.5f, HumanRotZ(-16.0f) * HumanRotX(8.0f) },
			{ 7.0f, HumanRotZ(-10.0f) },
			{ 12.0f, xId },
		};
		HumanAddRotKeys(pxClip, "Spine", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		// Head stays level on the horizon.
		const HumanRotKey ax[] = {
			{ 0.0f, xId },
			{ 3.5f, HumanRotZ(10.0f) },
			{ 7.0f, HumanRotZ(6.0f) },
			{ 12.0f, xId },
		};
		HumanAddRotKeys(pxClip, "Head", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		// Right leg steps out toward the dodge.
		const HumanRotKey ax[] = {
			{ 0.0f, xId },
			{ 3.0f, HumanRotZ(20.0f) * HumanRotX(-10.0f) },
			{ 6.5f, HumanRotZ(12.0f) },
			{ 12.0f, xId },
		};
		HumanAddRotKeys(pxClip, "RightUpperLeg", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f, xId },
			{ 3.0f, HumanRotX(28.0f) },
			{ 6.5f, HumanRotX(12.0f) },
			{ 12.0f, xId },
		};
		HumanAddRotKeys(pxClip, "RightLowerLeg", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		// Left leg pushes off then trails behind.
		const HumanRotKey ax[] = {
			{ 0.0f, xId },
			{ 3.0f, HumanRotZ(14.0f) },
			{ 6.5f, HumanRotZ(24.0f) * HumanRotX(6.0f) },
			{ 12.0f, xId },
		};
		HumanAddRotKeys(pxClip, "LeftUpperLeg", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f, xId },
			{ 6.5f, HumanRotX(36.0f) },
			{ 12.0f, xId },
		};
		HumanAddRotKeys(pxClip, "LeftLowerLeg", ax, sizeof(ax) / sizeof(ax[0]));
	}
	// Arms counterbalance away from the motion.
	{
		const HumanRotKey ax[] = {
			{ 0.0f, xId },
			{ 4.0f, HumanRotX(-26.0f) * HumanRotZ(-22.0f) },
			{ 8.0f, HumanRotX(-12.0f) * HumanRotZ(-8.0f) },
			{ 12.0f, xId },
		};
		HumanAddRotKeys(pxClip, "LeftUpperArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f, xId },
			{ 4.0f, HumanRotX(-20.0f) * HumanRotZ(14.0f) },
			{ 8.0f, HumanRotX(-8.0f) },
			{ 12.0f, xId },
		};
		HumanAddRotKeys(pxClip, "RightUpperArm", ax, sizeof(ax) / sizeof(ax[0]));
	}

	return pxClip;
}

static Flux_AnimationClip* CreateHitAnimation()
{
	// Impact from the front: head snaps, shoulders twist, a stagger step back
	// with a partial recovery (the old clip's Root ends at -0.2; kept).
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Hit");
	pxClip->SetDuration(0.3f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Quat xId = glm::identity<Zenith_Maths::Quat>();

	{
		const HumanPosKey ax[] = {
			{ 0.0f, { 0.0f, 0.0f, 0.0f } },
			{ 2.5f, { 0.015f, -0.03f, -0.16f } },
			{ 5.0f, { 0.0f, -0.05f, -0.22f } },
			{ 7.0f, { 0.0f, -0.03f, -0.20f } },
		};
		HumanAddPosKeys(pxClip, "Root", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f, xId },
			{ 1.2f, HumanRotX(-26.0f) * HumanRotY(7.0f) },             // whiplash back
			{ 4.0f, HumanRotX(-10.0f) },
			{ 7.0f, HumanRotX(-4.0f) },
		};
		HumanAddRotKeys(pxClip, "Head", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f, xId },
			{ 2.0f, HumanRotX(-18.0f) * HumanRotY(9.0f) },             // shoulders ride the impact
			{ 5.0f, HumanRotX(-8.0f) },
			{ 7.0f, HumanRotX(-3.0f) },
		};
		HumanAddRotKeys(pxClip, "Spine", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		// Right leg staggers back to catch the weight.
		const HumanRotKey ax[] = {
			{ 0.0f, xId },
			{ 3.0f, HumanRotX(16.0f) },
			{ 7.0f, HumanRotX(8.0f) },
		};
		HumanAddRotKeys(pxClip, "RightUpperLeg", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f, xId },
			{ 3.0f, HumanRotX(20.0f) },
			{ 7.0f, HumanRotX(10.0f) },
		};
		HumanAddRotKeys(pxClip, "RightLowerLeg", ax, sizeof(ax) / sizeof(ax[0]));
	}
	// Arms flinch up.
	for (u_int uSide = 0; uSide < 2; uSide++)
	{
		const char* szUpper = (uSide == 0) ? "LeftUpperArm" : "RightUpperArm";
		const char* szLower = (uSide == 0) ? "LeftLowerArm" : "RightLowerArm";
		{
			const HumanRotKey ax[] = {
				{ 0.0f, xId },
				{ 2.0f, HumanRotX(-30.0f) },
				{ 7.0f, HumanRotX(-8.0f) },
			};
			HumanAddRotKeys(pxClip, szUpper, ax, sizeof(ax) / sizeof(ax[0]));
		}
		{
			const HumanRotKey ax[] = {
				{ 0.0f, xId },
				{ 2.0f, HumanRotX(-55.0f) },
				{ 7.0f, HumanRotX(-14.0f) },
			};
			HumanAddRotKeys(pxClip, szLower, ax, sizeof(ax) / sizeof(ax[0]));
		}
	}

	return pxClip;
}

static Flux_AnimationClip* CreateDeathAnimation()
{
	// Backward collapse in stages: impact recoil, knees buckle, fold to the
	// ground, settle limp. Slight left/right asymmetry keeps it organic.
	// Root descends to -1.0 (pelvis at ground level — legacy contract).
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Death");
	pxClip->SetDuration(1.0f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Quat xId = glm::identity<Zenith_Maths::Quat>();

	{
		const HumanPosKey ax[] = {
			{ 0.0f, { 0.0f, 0.0f, 0.0f } },
			{ 4.0f, { 0.0f, -0.06f, -0.06f } },                        // recoil
			{ 9.0f, { 0.02f, -0.34f, -0.16f } },                       // knees give way
			{ 15.0f, { 0.03f, -0.74f, -0.30f } },                      // falling
			{ 20.0f, { 0.03f, -1.0f, -0.40f } },                       // down
			{ 24.0f, { 0.03f, -1.0f, -0.40f } },                       // still
		};
		HumanAddPosKeys(pxClip, "Root", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f, xId },
			{ 4.0f, HumanRotX(-16.0f) },
			{ 9.0f, HumanRotX(-44.0f) * HumanRotY(6.0f) },
			{ 15.0f, HumanRotX(-72.0f) * HumanRotY(8.0f) },
			{ 20.0f, HumanRotX(-88.0f) * HumanRotY(8.0f) },            // flat on the back
			{ 24.0f, HumanRotX(-88.0f) * HumanRotY(8.0f) },
		};
		HumanAddRotKeys(pxClip, "Spine", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f, xId },
			{ 3.0f, HumanRotX(-30.0f) },                               // head whips back first
			{ 10.0f, HumanRotX(-12.0f) },
			{ 16.0f, HumanRotX(14.0f) },                               // lolls forward as the back hits
			{ 21.0f, HumanRotX(6.0f) * HumanRotY(10.0f) },             // settles to the side
			{ 24.0f, HumanRotX(6.0f) * HumanRotY(10.0f) },
		};
		HumanAddRotKeys(pxClip, "Head", ax, sizeof(ax) / sizeof(ax[0]));
	}
	// Legs: knees buckle, then slide out as the body lands.
	for (u_int uSide = 0; uSide < 2; uSide++)
	{
		const char* szUpper = (uSide == 0) ? "LeftUpperLeg" : "RightUpperLeg";
		const char* szLower = (uSide == 0) ? "LeftLowerLeg" : "RightLowerLeg";
		const float fAsym = (uSide == 0) ? 1.0f : 1.18f;
		{
			const HumanRotKey ax[] = {
				{ 0.0f, xId },
				{ 9.0f, HumanRotX(28.0f * fAsym) },                    // thighs fold under
				{ 15.0f, HumanRotX(2.0f) },
				{ 20.0f, HumanRotX(-12.0f * fAsym) * HumanRotZ((uSide == 0) ? -7.0f : 9.0f) },   // sprawled
				{ 24.0f, HumanRotX(-12.0f * fAsym) * HumanRotZ((uSide == 0) ? -7.0f : 9.0f) },
			};
			HumanAddRotKeys(pxClip, szUpper, ax, sizeof(ax) / sizeof(ax[0]));
		}
		{
			const HumanRotKey ax[] = {
				{ 0.0f, xId },
				{ 9.0f, HumanRotX(60.0f * fAsym) },
				{ 15.0f, HumanRotX(46.0f) },
				{ 20.0f, HumanRotX(24.0f * fAsym) },
				{ 24.0f, HumanRotX(24.0f * fAsym) },
			};
			HumanAddRotKeys(pxClip, szLower, ax, sizeof(ax) / sizeof(ax[0]));
		}
	}
	// Arms: flail on the way down, land spread, settle.
	for (u_int uSide = 0; uSide < 2; uSide++)
	{
		const char* szUpper = (uSide == 0) ? "LeftUpperArm" : "RightUpperArm";
		const char* szLower = (uSide == 0) ? "LeftLowerArm" : "RightLowerArm";
		const float fOut = (uSide == 0) ? -1.0f : 1.0f;
		const float fAsym = (uSide == 0) ? 1.0f : 1.25f;
		{
			const HumanRotKey ax[] = {
				{ 0.0f, xId },
				{ 5.0f, HumanRotX(-34.0f * fAsym) * HumanRotZ(fOut * 14.0f) },    // thrown up by the impact
				{ 12.0f, HumanRotX(-12.0f) * HumanRotZ(fOut * 38.0f) },
				{ 19.0f, HumanRotZ(fOut * 56.0f * fAsym) },                       // spread on the ground
				{ 24.0f, HumanRotZ(fOut * 56.0f * fAsym) },
			};
			HumanAddRotKeys(pxClip, szUpper, ax, sizeof(ax) / sizeof(ax[0]));
		}
		{
			const HumanRotKey ax[] = {
				{ 0.0f, xId },
				{ 5.0f, HumanRotX(-48.0f) },
				{ 12.0f, HumanRotX(-20.0f) },
				{ 19.0f, HumanRotX(-8.0f * fAsym) },
				{ 24.0f, HumanRotX(-8.0f * fAsym) },
			};
			HumanAddRotKeys(pxClip, szLower, ax, sizeof(ax) / sizeof(ax[0]));
		}
	}

	return pxClip;
}

//------------------------------------------------------------------------------
// Aim Hold Pose
//
// Shared upper-body rest pose used by the Aim, Fire, and Reload clips. Each
// clip's start/end keyframes match this pose so transitions between them
// don't snap arms back to identity. (Values pinned by the .Tests.inl suite.)
//------------------------------------------------------------------------------
namespace StickFigureAimHoldPose
{
	inline Zenith_Maths::Quat RightUpperArm()
	{
		return glm::angleAxis(glm::radians(-75.0f), Zenith_Maths::Vector3(1, 0, 0))
		     * glm::angleAxis(glm::radians( 20.0f), Zenith_Maths::Vector3(0, 1, 0));
	}
	inline Zenith_Maths::Quat RightLowerArm()
	{
		return glm::angleAxis(glm::radians(-45.0f), Zenith_Maths::Vector3(1, 0, 0));
	}
	inline Zenith_Maths::Quat LeftUpperArm()
	{
		return glm::angleAxis(glm::radians(-65.0f), Zenith_Maths::Vector3(1, 0, 0))
		     * glm::angleAxis(glm::radians(-25.0f), Zenith_Maths::Vector3(0, 1, 0));
	}
	inline Zenith_Maths::Quat LeftLowerArm()
	{
		return glm::angleAxis(glm::radians(-50.0f), Zenith_Maths::Vector3(1, 0, 0));
	}
	inline Zenith_Maths::Quat Spine()
	{
		return glm::angleAxis(glm::radians(-10.0f), Zenith_Maths::Vector3(1, 0, 0));
	}
	inline Zenith_Maths::Quat Head()
	{
		return glm::angleAxis(glm::radians(-5.0f), Zenith_Maths::Vector3(1, 0, 0));
	}
}

static Flux_AnimationClip* CreateAimAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Aim");
	pxClip->SetDuration(0.5f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(true);

	// Stable hold with a breathing waver on the mid keys. Keys 0, 3 and 12 are
	// the EXACT hold pose: 0/12 pin the loop + the Fire/Reload transitions
	// (AimClipRightArmRotation samples both), and 3 must match 0 because
	// SampleRotation's end-of-clip fallback EXTRAPOLATES the first segment —
	// slerp(key0, key1, (12-0)/(3-0)) — so any 0->3 delta would be amplified
	// 4x at the t=12 boundary sample. The +-sway lives at ticks 6 and 9.
	auto AddHold = [&](const char* szBone, const Zenith_Maths::Quat& xPose, float fSwayDeg)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, xPose);
		xChannel.AddRotationKeyframe(3.0f, xPose);
		xChannel.AddRotationKeyframe(6.0f, HumanRotX(fSwayDeg) * xPose);
		xChannel.AddRotationKeyframe(9.0f, HumanRotX(-fSwayDeg) * xPose);
		xChannel.AddRotationKeyframe(12.0f, xPose);
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel(szBone, std::move(xChannel));
	};

	AddHold("RightUpperArm", StickFigureAimHoldPose::RightUpperArm(), 0.7f);
	AddHold("RightLowerArm", StickFigureAimHoldPose::RightLowerArm(), 0.5f);
	AddHold("LeftUpperArm",  StickFigureAimHoldPose::LeftUpperArm(),  0.7f);
	AddHold("LeftLowerArm",  StickFigureAimHoldPose::LeftLowerArm(),  0.5f);
	AddHold("Spine",         StickFigureAimHoldPose::Spine(),         0.4f);
	AddHold("Head",          StickFigureAimHoldPose::Head(),          0.3f);

	return pxClip;
}

static Flux_AnimationClip* CreateFireAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Fire");
	pxClip->SetDuration(0.20f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Vector3 xXAxis(1, 0, 0);

	// Recoil deltas applied on top of the aim hold pose, so arms stay raised.
	// The t=2 peak (RightUpperArm = +15 deg X on the hold pose) is pinned by
	// FireClipPeakRecoil; the t=3.5 key adds a small settle overshoot.
	auto AddRecoil = [&](const char* szBone, const Zenith_Maths::Quat& xRest, float fKickDeg)
	{
		const Zenith_Maths::Quat xKick = glm::angleAxis(glm::radians(fKickDeg), xXAxis) * xRest;
		const Zenith_Maths::Quat xSettle = glm::angleAxis(glm::radians(-fKickDeg * 0.18f), xXAxis) * xRest;
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, xRest);
		xChannel.AddRotationKeyframe(2.0f, xKick);
		xChannel.AddRotationKeyframe(3.5f, xSettle);
		xChannel.AddRotationKeyframe(5.0f, xRest);
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel(szBone, std::move(xChannel));
	};

	AddRecoil("RightUpperArm", StickFigureAimHoldPose::RightUpperArm(), 15.0f);
	AddRecoil("RightLowerArm", StickFigureAimHoldPose::RightLowerArm(), -10.0f);
	AddRecoil("LeftUpperArm",  StickFigureAimHoldPose::LeftUpperArm(),  6.0f);
	AddRecoil("LeftLowerArm",  StickFigureAimHoldPose::LeftLowerArm(),  -4.0f);
	AddRecoil("Spine",         StickFigureAimHoldPose::Spine(),         3.0f);
	AddRecoil("Head",          StickFigureAimHoldPose::Head(),          2.0f);

	return pxClip;
}

static Flux_AnimationClip* CreateReloadAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Reload");
	pxClip->SetDuration(1.5f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Quat xLeftUpperRest  = StickFigureAimHoldPose::LeftUpperArm();
	const Zenith_Maths::Quat xLeftLowerRest  = StickFigureAimHoldPose::LeftLowerArm();
	const Zenith_Maths::Quat xRightUpperRest = StickFigureAimHoldPose::RightUpperArm();
	const Zenith_Maths::Quat xRightLowerRest = StickFigureAimHoldPose::RightLowerArm();
	const Zenith_Maths::Quat xSpineRest      = StickFigureAimHoldPose::Spine();
	const Zenith_Maths::Quat xHeadRest       = StickFigureAimHoldPose::Head();

	// Left hand: drop off the foregrip toward the belt (POSITIVE X deltas — the
	// rest pose is already raised at -65X, so dropping means rotating back),
	// grab the magazine, bring it up to the mag-well, seat it with a push,
	// slap the release, return. 8 keyframes (pinned by
	// ReloadClipKeyframesOnLeftArm).
	{
		const HumanRotKey ax[] = {
			{ 0.0f,  xLeftUpperRest },
			{ 5.0f,  HumanRotX(52.0f) * xLeftUpperRest },                        // drop off the grip
			{ 12.0f, HumanRotX(44.0f) * HumanRotY(34.0f) * xLeftUpperRest },     // reach to the belt
			{ 17.0f, HumanRotX(40.0f) * HumanRotY(22.0f) * xLeftUpperRest },     // mag in hand
			{ 23.0f, HumanRotX(6.0f) * xLeftUpperRest },                         // up to the mag-well
			{ 26.0f, HumanRotX(14.0f) * xLeftUpperRest },                        // seat it
			{ 29.0f, HumanRotX(2.0f) * xLeftUpperRest },                         // slap the release
			{ 36.0f, xLeftUpperRest },                                           // back on the grip
		};
		HumanAddRotKeys(pxClip, "LeftUpperArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f,  xLeftLowerRest },
			{ 5.0f,  HumanRotX(28.0f) * xLeftLowerRest },                        // forearm extends as the arm drops
			{ 12.0f, HumanRotX(14.0f) * xLeftLowerRest },
			{ 17.0f, HumanRotX(-6.0f) * xLeftLowerRest },
			{ 23.0f, HumanRotX(-22.0f) * xLeftLowerRest },                       // curls up with the magazine
			{ 26.0f, HumanRotX(-14.0f) * xLeftLowerRest },
			{ 29.0f, HumanRotX(-26.0f) * xLeftLowerRest },
			{ 36.0f, xLeftLowerRest },
		};
		HumanAddRotKeys(pxClip, "LeftLowerArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	// Right arm tips the weapon up and in toward the body for the mag change.
	{
		const HumanRotKey ax[] = {
			{ 0.0f,  xRightUpperRest },
			{ 6.0f,  HumanRotX(-12.0f) * HumanRotY(-8.0f) * xRightUpperRest },
			{ 20.0f, HumanRotX(-16.0f) * HumanRotY(-10.0f) * xRightUpperRest },
			{ 28.0f, HumanRotX(-10.0f) * xRightUpperRest },
			{ 36.0f, xRightUpperRest },
		};
		HumanAddRotKeys(pxClip, "RightUpperArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f,  xRightLowerRest },
			{ 6.0f,  HumanRotX(-14.0f) * xRightLowerRest },
			{ 20.0f, HumanRotX(-18.0f) * xRightLowerRest },
			{ 36.0f, xRightLowerRest },
		};
		HumanAddRotKeys(pxClip, "RightLowerArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	// Eyes drop to the weapon; shoulders hunch slightly over the work.
	{
		const HumanRotKey ax[] = {
			{ 0.0f,  xHeadRest },
			{ 8.0f,  HumanRotX(-11.0f) * xHeadRest },
			{ 27.0f, HumanRotX(-11.0f) * xHeadRest },
			{ 36.0f, xHeadRest },
		};
		HumanAddRotKeys(pxClip, "Head", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f,  xSpineRest },
			{ 10.0f, HumanRotX(-4.0f) * xSpineRest },
			{ 26.0f, HumanRotX(-4.0f) * xSpineRest },
			{ 36.0f, xSpineRest },
		};
		HumanAddRotKeys(pxClip, "Spine", ax, sizeof(ax) / sizeof(ax[0]));
	}

	return pxClip;
}

static Flux_AnimationClip* CreateJumpAnimation()
{
	// Crouch -> explosive extension -> mid-air tuck -> landing absorb ->
	// recover. Spine's final key is identity (pinned by
	// JumpClipReturnsToIdentityAtEnd).
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Jump");
	pxClip->SetDuration(0.8f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Quat xId = glm::identity<Zenith_Maths::Quat>();

	{
		const HumanRotKey ax[] = {
			{ 0.0f,  xId },
			{ 4.0f,  HumanRotX(18.0f) },                               // crouch hunch
			{ 8.0f,  HumanRotX(-7.0f) },                               // extended, chest open
			{ 12.0f, HumanRotX(6.0f) },                                // tucked
			{ 16.0f, HumanRotX(9.0f) },                                // landing absorb
			{ 19.0f, xId },
		};
		HumanAddRotKeys(pxClip, "Spine", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f,  xId },
			{ 4.0f,  HumanRotX(-9.0f) },                               // eyes stay up in the crouch
			{ 12.0f, HumanRotX(4.0f) },
			{ 19.0f, xId },
		};
		HumanAddRotKeys(pxClip, "Head", ax, sizeof(ax) / sizeof(ax[0]));
	}
	for (u_int uSide = 0; uSide < 2; uSide++)
	{
		const char* szUpperLeg = (uSide == 0) ? "LeftUpperLeg" : "RightUpperLeg";
		const char* szLowerLeg = (uSide == 0) ? "LeftLowerLeg" : "RightLowerLeg";
		const char* szFoot = (uSide == 0) ? "LeftFoot" : "RightFoot";
		const char* szUpperArm = (uSide == 0) ? "LeftUpperArm" : "RightUpperArm";
		const char* szLowerArm = (uSide == 0) ? "LeftLowerArm" : "RightLowerArm";
		{
			const HumanRotKey ax[] = {
				{ 0.0f,  xId },
				{ 4.0f,  HumanRotX(34.0f) },                           // crouch load
				{ 8.0f,  HumanRotX(-12.0f) },                          // full hip extension at takeoff
				{ 12.0f, HumanRotX(-44.0f) },                          // knees driven up in the tuck
				{ 16.0f, HumanRotX(26.0f) },                           // landing absorb
				{ 19.0f, xId },
			};
			HumanAddRotKeys(pxClip, szUpperLeg, ax, sizeof(ax) / sizeof(ax[0]));
		}
		{
			const HumanRotKey ax[] = {
				{ 0.0f,  xId },
				{ 4.0f,  HumanRotX(50.0f) },
				{ 8.0f,  HumanRotX(4.0f) },
				{ 12.0f, HumanRotX(86.0f) },                           // heels tucked
				{ 16.0f, HumanRotX(42.0f) },
				{ 19.0f, xId },
			};
			HumanAddRotKeys(pxClip, szLowerLeg, ax, sizeof(ax) / sizeof(ax[0]));
		}
		{
			const HumanRotKey ax[] = {
				{ 0.0f,  xId },
				{ 4.0f,  HumanRotX(-8.0f) },                           // dorsiflex in the crouch
				{ 8.0f,  HumanRotX(22.0f) },                           // toes point at push-off
				{ 12.0f, HumanRotX(-10.0f) },                          // toes up for the landing
				{ 16.0f, HumanRotX(6.0f) },
				{ 19.0f, xId },
			};
			HumanAddRotKeys(pxClip, szFoot, ax, sizeof(ax) / sizeof(ax[0]));
		}
		{
			const float fOut = (uSide == 0) ? -8.0f : 8.0f;
			const HumanRotKey ax[] = {
				{ 0.0f,  xId },
				{ 4.0f,  HumanRotX(24.0f) },                           // arms swing back in the crouch
				{ 8.0f,  HumanRotX(-58.0f) * HumanRotZ(fOut) },        // thrown up for momentum
				{ 12.0f, HumanRotX(-30.0f) * HumanRotZ(fOut) },
				{ 16.0f, HumanRotX(14.0f) },                           // forward for balance on landing
				{ 19.0f, xId },
			};
			HumanAddRotKeys(pxClip, szUpperArm, ax, sizeof(ax) / sizeof(ax[0]));
		}
		{
			const HumanRotKey ax[] = {
				{ 0.0f,  xId },
				{ 4.0f,  HumanRotX(-18.0f) },
				{ 8.0f,  HumanRotX(-34.0f) },
				{ 12.0f, HumanRotX(-16.0f) },
				{ 19.0f, xId },
			};
			HumanAddRotKeys(pxClip, szLowerArm, ax, sizeof(ax) / sizeof(ax[0]));
		}
	}

	return pxClip;
}

//------------------------------------------------------------------------------
// Tennis stroke clips (RenderTest tennis testbed).
//
// Right-handed player; the racket is attached to RightHand by the engine
// attachment system. Authored anticipation -> contact -> follow-through; every
// channel returns to identity so the tennis state machine blends cleanly back to
// ReadyStance. The arm-IK pass in the tennis player component refines the racket
// hand onto the ball during the contact window, so these clips supply the
// readable swing shape rather than a frame-perfect contact point.
//------------------------------------------------------------------------------

static Flux_AnimationClip* CreateServeAnimation()
{
	// Overhead serve: knees load, left arm tosses, right arm cocks into the
	// "trophy" behind the head, then drives up through an overhead contact and
	// pronates down across the body. Modelled on the Attack3 smash, one-handed.
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Serve");
	pxClip->SetDuration(1.25f);          // 30 ticks @ 24 TPS
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Quat xId = glm::identity<Zenith_Maths::Quat>();

	{
		// Racket arm: down -> trophy behind head -> reach to contact -> pronate down.
		const HumanRotKey ax[] = {
			{ 0.0f,  xId },
			{ 6.0f,  HumanRotX(-45.0f) },                          // start the take-back up
			{ 12.0f, HumanRotX(-140.0f) * HumanRotZ(18.0f) },      // trophy (elbow cocked behind head)
			{ 16.0f, HumanRotX(-165.0f) },                         // reach up to contact
			{ 19.0f, HumanRotX(-95.0f) },                          // pronate forward through the ball
			{ 23.0f, HumanRotX(40.0f) },                           // follow-through down across the body
			{ 30.0f, xId },
		};
		HumanAddRotKeys(pxClip, "RightUpperArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f,  HumanRotX(-10.0f) },
			{ 12.0f, HumanRotX(-90.0f) },                          // elbow cocked in the trophy
			{ 16.0f, HumanRotX(-15.0f) },                          // snaps straight at contact
			{ 19.0f, HumanRotX(-5.0f) },
			{ 23.0f, HumanRotX(-35.0f) },
			{ 30.0f, xId },
		};
		HumanAddRotKeys(pxClip, "RightLowerArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		// Toss arm: raises straight up to the toss apex, then lowers.
		const HumanRotKey ax[] = {
			{ 0.0f,  xId },
			{ 8.0f,  HumanRotX(-120.0f) * HumanRotZ(-10.0f) },     // raise to release the toss
			{ 14.0f, HumanRotX(-150.0f) },                         // toss apex, arm pointing up
			{ 20.0f, HumanRotX(-40.0f) },                          // lower out of the way
			{ 30.0f, xId },
		};
		HumanAddRotKeys(pxClip, "LeftUpperArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f,  HumanRotX(-10.0f) },
			{ 8.0f,  HumanRotX(-18.0f) },                          // near-straight for the toss
			{ 14.0f, HumanRotX(-8.0f) },
			{ 30.0f, xId },
		};
		HumanAddRotKeys(pxClip, "LeftLowerArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		// Spine arches back into the trophy, crunches forward through contact.
		const HumanRotKey ax[] = {
			{ 0.0f,  xId },
			{ 10.0f, HumanRotX(-12.0f) },                          // arch back under the toss
			{ 16.0f, HumanRotX(14.0f) },                           // crunch forward at contact
			{ 22.0f, HumanRotX(6.0f) },
			{ 30.0f, xId },
		};
		HumanAddRotKeys(pxClip, "Spine", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f,  xId },
			{ 12.0f, HumanRotX(-16.0f) },                          // eyes up on the toss
			{ 16.0f, HumanRotX(-8.0f) },
			{ 30.0f, xId },
		};
		HumanAddRotKeys(pxClip, "Head", ax, sizeof(ax) / sizeof(ax[0]));
	}
	// Legs load the crouch then drive up at contact.
	for (u_int uSide = 0; uSide < 2; uSide++)
	{
		const char* szUpper = (uSide == 0) ? "LeftUpperLeg" : "RightUpperLeg";
		const char* szLower = (uSide == 0) ? "LeftLowerLeg" : "RightLowerLeg";
		{
			const HumanRotKey ax[] = {
				{ 0.0f,  xId },
				{ 10.0f, HumanRotX(10.0f) },
				{ 16.0f, HumanRotX(-8.0f) },                       // hip extension drive
				{ 30.0f, xId },
			};
			HumanAddRotKeys(pxClip, szUpper, ax, sizeof(ax) / sizeof(ax[0]));
		}
		{
			const HumanRotKey ax[] = {
				{ 0.0f,  xId },
				{ 10.0f, HumanRotX(30.0f) },                       // load crouch
				{ 16.0f, HumanRotX(4.0f) },                        // extend up into contact
				{ 20.0f, HumanRotX(20.0f) },                       // land
				{ 30.0f, xId },
			};
			HumanAddRotKeys(pxClip, szLower, ax, sizeof(ax) / sizeof(ax[0]));
		}
	}
	{
		const HumanPosKey ax[] = {
			{ 0.0f,  { 0.0f, 0.0f, 0.0f } },
			{ 10.0f, { 0.0f, -0.06f, -0.02f } },                  // load down/back
			{ 16.0f, { 0.0f, 0.05f, 0.04f } },                    // rise onto toes into contact
			{ 22.0f, { 0.0f, -0.02f, 0.05f } },
			{ 30.0f, { 0.0f, 0.0f, 0.0f } },
		};
		HumanAddPosKeys(pxClip, "Root", ax, sizeof(ax) / sizeof(ax[0]));
	}

	return pxClip;
}

static Flux_AnimationClip* CreateForehandAnimation()
{
	// Forehand drive: torso coils away (right shoulder back), then uncoils
	// through a contact in front, racket arm sweeping right-to-left up over the
	// left shoulder. The spine rotation carries the whole arm horizontally.
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Forehand");
	pxClip->SetDuration(0.75f);          // 18 ticks @ 24 TPS
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Quat xId = glm::identity<Zenith_Maths::Quat>();

	{
		const HumanRotKey ax[] = {
			{ 0.0f,  xId },
			{ 4.0f,  HumanRotY(28.0f) },                          // coil: right shoulder back
			{ 10.0f, HumanRotY(-30.0f) * HumanRotX(6.0f) },       // uncoil through contact, lean in
			{ 14.0f, HumanRotY(-18.0f) },
			{ 18.0f, xId },
		};
		HumanAddRotKeys(pxClip, "Spine", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		// Racket arm: take-back out to the right, sweep across to a high finish.
		const HumanRotKey ax[] = {
			{ 0.0f,  xId },
			{ 4.0f,  HumanRotX(-50.0f) * HumanRotZ(20.0f) },      // take-back (arm out/back to the right)
			{ 10.0f, HumanRotX(-85.0f) },                         // contact in front
			{ 13.0f, HumanRotX(-100.0f) * HumanRotZ(-12.0f) },    // follow-through up over the left shoulder
			{ 18.0f, xId },
		};
		HumanAddRotKeys(pxClip, "RightUpperArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f,  HumanRotX(-12.0f) },
			{ 4.0f,  HumanRotX(-55.0f) },                         // cocked on the take-back
			{ 10.0f, HumanRotX(-12.0f) },                         // extends at contact
			{ 13.0f, HumanRotX(-30.0f) },
			{ 18.0f, xId },
		};
		HumanAddRotKeys(pxClip, "RightLowerArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		// Off arm balances out to the left.
		const HumanRotKey ax[] = {
			{ 0.0f,  xId },
			{ 4.0f,  HumanRotX(-30.0f) * HumanRotZ(-20.0f) },
			{ 10.0f, HumanRotX(-10.0f) * HumanRotZ(-25.0f) },
			{ 18.0f, xId },
		};
		HumanAddRotKeys(pxClip, "LeftUpperArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f,  HumanRotX(-10.0f) },
			{ 4.0f,  HumanRotX(-40.0f) },
			{ 18.0f, xId },
		};
		HumanAddRotKeys(pxClip, "LeftLowerArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f,  xId },
			{ 10.0f, HumanRotY(-8.0f) },                          // eyes follow across
			{ 18.0f, xId },
		};
		HumanAddRotKeys(pxClip, "Head", ax, sizeof(ax) / sizeof(ax[0]));
	}
	// Light knee load through the stroke.
	for (u_int uSide = 0; uSide < 2; uSide++)
	{
		const char* szLower = (uSide == 0) ? "LeftLowerLeg" : "RightLowerLeg";
		const HumanRotKey ax[] = {
			{ 0.0f,  xId },
			{ 4.0f,  HumanRotX(18.0f) },
			{ 10.0f, HumanRotX(8.0f) },
			{ 18.0f, xId },
		};
		HumanAddRotKeys(pxClip, szLower, ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanPosKey ax[] = {
			{ 0.0f,  { 0.0f, 0.0f, 0.0f } },
			{ 4.0f,  { 0.03f, -0.02f, -0.02f } },                 // load onto the right foot
			{ 10.0f, { -0.02f, -0.01f, 0.04f } },                 // drive forward/through
			{ 18.0f, { 0.0f, 0.0f, 0.0f } },
		};
		HumanAddPosKeys(pxClip, "Root", ax, sizeof(ax) / sizeof(ax[0]));
	}

	return pxClip;
}

static Flux_AnimationClip* CreateBackhandAnimation()
{
	// Two-handed backhand: torso coils the other way (right shoulder forward),
	// both arms take the racket back across to the left, then sweep out to the
	// right through contact.
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Backhand");
	pxClip->SetDuration(0.75f);          // 18 ticks @ 24 TPS
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Quat xId = glm::identity<Zenith_Maths::Quat>();

	{
		const HumanRotKey ax[] = {
			{ 0.0f,  xId },
			{ 4.0f,  HumanRotY(-26.0f) },                         // coil: right shoulder forward
			{ 10.0f, HumanRotY(26.0f) * HumanRotX(6.0f) },        // uncoil out to the right
			{ 14.0f, HumanRotY(16.0f) },
			{ 18.0f, xId },
		};
		HumanAddRotKeys(pxClip, "Spine", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		// Racket arm: taken back across to the left, sweeps out to the right.
		const HumanRotKey ax[] = {
			{ 0.0f,  xId },
			{ 4.0f,  HumanRotX(-80.0f) * HumanRotZ(-18.0f) },     // take-back across to the left
			{ 10.0f, HumanRotX(-70.0f) * HumanRotZ(15.0f) },      // contact out in front-right
			{ 13.0f, HumanRotX(-75.0f) * HumanRotZ(25.0f) },      // follow-through to the right
			{ 18.0f, xId },
		};
		HumanAddRotKeys(pxClip, "RightUpperArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f,  HumanRotX(-12.0f) },
			{ 4.0f,  HumanRotX(-45.0f) },
			{ 10.0f, HumanRotX(-15.0f) },                         // extends through contact
			{ 18.0f, xId },
		};
		HumanAddRotKeys(pxClip, "RightLowerArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		// Off hand stays on the racket and mirrors the swing.
		const HumanRotKey ax[] = {
			{ 0.0f,  xId },
			{ 4.0f,  HumanRotX(-70.0f) * HumanRotZ(-22.0f) },
			{ 10.0f, HumanRotX(-60.0f) * HumanRotZ(10.0f) },
			{ 18.0f, xId },
		};
		HumanAddRotKeys(pxClip, "LeftUpperArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f,  HumanRotX(-10.0f) },
			{ 4.0f,  HumanRotX(-50.0f) },
			{ 10.0f, HumanRotX(-20.0f) },
			{ 18.0f, xId },
		};
		HumanAddRotKeys(pxClip, "LeftLowerArm", ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanRotKey ax[] = {
			{ 0.0f,  xId },
			{ 10.0f, HumanRotY(8.0f) },
			{ 18.0f, xId },
		};
		HumanAddRotKeys(pxClip, "Head", ax, sizeof(ax) / sizeof(ax[0]));
	}
	for (u_int uSide = 0; uSide < 2; uSide++)
	{
		const char* szLower = (uSide == 0) ? "LeftLowerLeg" : "RightLowerLeg";
		const HumanRotKey ax[] = {
			{ 0.0f,  xId },
			{ 4.0f,  HumanRotX(18.0f) },
			{ 10.0f, HumanRotX(8.0f) },
			{ 18.0f, xId },
		};
		HumanAddRotKeys(pxClip, szLower, ax, sizeof(ax) / sizeof(ax[0]));
	}
	{
		const HumanPosKey ax[] = {
			{ 0.0f,  { 0.0f, 0.0f, 0.0f } },
			{ 4.0f,  { -0.03f, -0.02f, -0.02f } },                // load onto the left foot
			{ 10.0f, { 0.03f, -0.01f, 0.04f } },                  // drive forward/through to the right
			{ 18.0f, { 0.0f, 0.0f, 0.0f } },
		};
		HumanAddPosKeys(pxClip, "Root", ax, sizeof(ax) / sizeof(ax[0]));
	}

	return pxClip;
}

static Flux_AnimationClip* CreateReadyStanceAnimation()
{
	// Looping tennis ready stance: knees bent, slight forward lean, both arms
	// held forward and inward (racket out front in both hands), with a gentle
	// split-step bounce. Continuous sin/cos curves so key 0 == key N.
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("ReadyStance");
	pxClip->SetDuration(1.5f);           // 36 ticks @ 24 TPS
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(true);

	constexpr float fTICKS = 36.0f;
	constexpr u_int uKEYS = 25;

	// Pelvis: low crouch with a two-per-cycle split-step bounce + small sway.
	HumanAddPosCurve(pxClip, "Root", fTICKS, uKEYS, [](float fT)
	{
		const float fP = fT * fHUMAN_TWO_PI;
		return Zenith_Maths::Vector3(0.012f * sinf(fP), -0.05f + 0.025f * cosf(2.0f * fP), 0.0f);
	});

	// Bent knees with a small bob; balls-of-feet stance.
	for (u_int uSide = 0; uSide < 2; uSide++)
	{
		const char* szUpper = (uSide == 0) ? "LeftUpperLeg" : "RightUpperLeg";
		const char* szLower = (uSide == 0) ? "LeftLowerLeg" : "RightLowerLeg";
		const char* szFoot  = (uSide == 0) ? "LeftFoot" : "RightFoot";
		HumanAddRotCurve(pxClip, szUpper, fTICKS, uKEYS, [](float fT)
		{
			return HumanRotX(-6.0f + 2.0f * cosf(2.0f * fT * fHUMAN_TWO_PI));
		});
		HumanAddRotCurve(pxClip, szLower, fTICKS, uKEYS, [](float fT)
		{
			return HumanRotX(30.0f + 5.0f * cosf(2.0f * fT * fHUMAN_TWO_PI));
		});
		HumanAddRotCurve(pxClip, szFoot, fTICKS, uKEYS, [](float fT)
		{
			return HumanRotX(8.0f + 2.0f * cosf(2.0f * fT * fHUMAN_TWO_PI));
		});
	}

	// Forward lean with a small lateral rock.
	HumanAddRotCurve(pxClip, "Spine", fTICKS, uKEYS, [](float fT)
	{
		return HumanRotX(12.0f) * HumanRotZ(1.5f * sinf(fT * fHUMAN_TWO_PI));
	});

	// Both arms forward and inward so the hands meet on the racket out front,
	// with a light ready-bounce on the elbows.
	for (u_int uSide = 0; uSide < 2; uSide++)
	{
		const float fIn = (uSide == 0) ? 12.0f : -12.0f;   // bring both arms toward centre
		const char* szUpper = (uSide == 0) ? "LeftUpperArm" : "RightUpperArm";
		const char* szLower = (uSide == 0) ? "LeftLowerArm" : "RightLowerArm";
		HumanAddRotCurve(pxClip, szUpper, fTICKS, uKEYS, [=](float fT)
		{
			return HumanRotX(-40.0f + 2.5f * cosf(2.0f * fT * fHUMAN_TWO_PI)) * HumanRotZ(fIn);
		});
		HumanAddRotCurve(pxClip, szLower, fTICKS, uKEYS, [](float fT)
		{
			return HumanRotX(-55.0f + 3.0f * cosf(2.0f * fT * fHUMAN_TWO_PI));
		});
	}

	// Head: attentive, watching the ball.
	HumanAddRotCurve(pxClip, "Head", fTICKS, uKEYS, [](float fT)
	{
		return HumanRotX(-4.0f) * HumanRotY(1.5f * sinf(fT * fHUMAN_TWO_PI));
	});

	return pxClip;
}

//------------------------------------------------------------------------------
// Bullet sphere mesh
//
// Generates a unit sphere into a Zenith_MeshAsset. Used by GenerateRenderTestAssets
// to produce the bullet projectile asset on disk.
//------------------------------------------------------------------------------
static void GenerateUnitSphereMeshAsset(Zenith_MeshAsset& xMeshOut, uint32_t uSegments, uint32_t uRings)
{
	xMeshOut.Reset();

	const uint32_t uVertCount = (uRings + 1) * (uSegments + 1);
	const uint32_t uIdxCount  = uRings * uSegments * 6;
	xMeshOut.Reserve(uVertCount, uIdxCount);

	for (uint32_t uRing = 0; uRing <= uRings; ++uRing)
	{
		const float fV = static_cast<float>(uRing) / static_cast<float>(uRings);
		const float fPhi = fV * glm::pi<float>();
		const float fY = cosf(fPhi);
		const float fSinPhi = sinf(fPhi);

		for (uint32_t uSeg = 0; uSeg <= uSegments; ++uSeg)
		{
			const float fU = static_cast<float>(uSeg) / static_cast<float>(uSegments);
			const float fTheta = fU * glm::pi<float>() * 2.0f;
			const Zenith_Maths::Vector3 xPos(fSinPhi * cosf(fTheta), fY, fSinPhi * sinf(fTheta));
			xMeshOut.AddVertex(xPos * 0.5f, xPos /* normal */, Zenith_Maths::Vector2(fU, fV));
		}
	}

	for (uint32_t uRing = 0; uRing < uRings; ++uRing)
	{
		for (uint32_t uSeg = 0; uSeg < uSegments; ++uSeg)
		{
			const uint32_t uA = uRing * (uSegments + 1) + uSeg;
			const uint32_t uB = uA + (uSegments + 1);
			xMeshOut.AddTriangle(uA, uB, uA + 1);
			xMeshOut.AddTriangle(uA + 1, uB, uB + 1);
		}
	}

	xMeshOut.AddSubmesh(0, uIdxCount, 0);
	xMeshOut.ComputeBounds();
}

//------------------------------------------------------------------------------
// Public Asset Generation Functions
//------------------------------------------------------------------------------

void GenerateStickFigureAssets()
{
	Zenith_Log(LOG_CATEGORY_ASSET, "Generating StickFigure human assets (lofted body, painted atlas, gait clips)...");

	// Create all assets
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Zenith_MeshAsset* pxMesh = CreateStickFigureMesh(pxSkel);
	Flux_AnimationClip* pxIdleClip = CreateIdleAnimation();
	Flux_AnimationClip* pxWalkClip = CreateWalkAnimation();
	Flux_AnimationClip* pxRunClip = CreateRunAnimation();
	Flux_AnimationClip* pxAttack1Clip = CreateAttack1Animation();
	Flux_AnimationClip* pxAttack2Clip = CreateAttack2Animation();
	Flux_AnimationClip* pxAttack3Clip = CreateAttack3Animation();
	Flux_AnimationClip* pxDodgeClip = CreateDodgeAnimation();
	Flux_AnimationClip* pxHitClip = CreateHitAnimation();
	Flux_AnimationClip* pxDeathClip = CreateDeathAnimation();
	Flux_AnimationClip* pxAimClip = CreateAimAnimation();
	Flux_AnimationClip* pxFireClip = CreateFireAnimation();
	Flux_AnimationClip* pxReloadClip = CreateReloadAnimation();
	Flux_AnimationClip* pxJumpClip = CreateJumpAnimation();
	Flux_AnimationClip* pxServeClip = CreateServeAnimation();
	Flux_AnimationClip* pxForehandClip = CreateForehandAnimation();
	Flux_AnimationClip* pxBackhandClip = CreateBackhandAnimation();
	Flux_AnimationClip* pxReadyStanceClip = CreateReadyStanceAnimation();

	// Create output directory
	std::string strOutputDir = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/";
	std::filesystem::create_directories(strOutputDir);

	// Export skeleton
	std::string strSkelPath = strOutputDir + "StickFigure" ZENITH_SKELETON_EXT;
	pxSkel->Export(strSkelPath.c_str());
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported skeleton to: %s", strSkelPath.c_str());

	// Set skeleton path on mesh before export
	pxMesh->SetSkeletonPath("Meshes/StickFigure/StickFigure" ZENITH_SKELETON_EXT);

	// Export mesh in Zenith_MeshAsset format
	std::string strMeshAssetPath = strOutputDir + "StickFigure" ZENITH_MESH_ASSET_EXT;
	pxMesh->Export(strMeshAssetPath.c_str());
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported mesh asset to: %s (%u verts, %u indices)",
		strMeshAssetPath.c_str(), pxMesh->GetNumVerts(), pxMesh->GetNumIndices());

#ifdef ZENITH_TOOLS
	// Export mesh in Flux_MeshGeometry format
	Flux_MeshGeometry* pxFluxGeometry = Zenith_Tools_CreateFluxMeshGeometry(pxMesh, pxSkel);
	std::string strMeshPath = strOutputDir + "StickFigure" ZENITH_MESH_EXT;
	pxFluxGeometry->Export(strMeshPath.c_str());
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported mesh geometry to: %s", strMeshPath.c_str());
	delete pxFluxGeometry;

	// Export static mesh
	Flux_MeshGeometry* pxStaticGeometry = Zenith_Tools_CreateStaticFluxMeshGeometry(pxMesh);
	std::string strStaticMeshPath = strOutputDir + "StickFigure_Static" ZENITH_MESH_EXT;
	pxStaticGeometry->Export(strStaticMeshPath.c_str());
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported static mesh geometry to: %s", strStaticMeshPath.c_str());
	delete pxStaticGeometry;

	// Painted texture atlas + body material + the canonical model bundle.
	GenerateStickFigureTextures(strOutputDir);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported texture atlas (Albedo/Normal/RM) to: %s", strOutputDir.c_str());
	GenerateStickFigureMaterial(strOutputDir);
	ExportStickFigureModel(strOutputDir, strSkelPath, strMeshAssetPath);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Exported body material + model to: %s", strOutputDir.c_str());
#endif

	// Export animations
	struct ClipExport { Flux_AnimationClip* pxClip; const char* szSuffix; };
	const ClipExport axClips[] = {
		{ pxIdleClip, "Idle" }, { pxWalkClip, "Walk" }, { pxRunClip, "Run" },
		{ pxAttack1Clip, "Attack1" }, { pxAttack2Clip, "Attack2" }, { pxAttack3Clip, "Attack3" },
		{ pxDodgeClip, "Dodge" }, { pxHitClip, "Hit" }, { pxDeathClip, "Death" },
		{ pxAimClip, "Aim" }, { pxFireClip, "Fire" }, { pxReloadClip, "Reload" },
		{ pxJumpClip, "Jump" },
		{ pxServeClip, "Serve" }, { pxForehandClip, "Forehand" },
		{ pxBackhandClip, "Backhand" }, { pxReadyStanceClip, "ReadyStance" },
	};
	for (const ClipExport& xExport : axClips)
	{
		const std::string strPath = strOutputDir + "StickFigure_" + xExport.szSuffix + ZENITH_ANIMATION_EXT;
		xExport.pxClip->Export(strPath);
		Zenith_Log(LOG_CATEGORY_ASSET, "  Exported %s animation to: %s", xExport.szSuffix, strPath.c_str());
	}

	// Export to glTF format for editing in Blender
	{
		std::vector<const Flux_AnimationClip*> axGltfClips = {
			pxIdleClip, pxWalkClip, pxRunClip, pxAttack1Clip, pxAttack2Clip,
			pxAttack3Clip, pxDodgeClip, pxHitClip, pxDeathClip,
			pxAimClip, pxFireClip, pxReloadClip, pxJumpClip,
			pxServeClip, pxForehandClip, pxBackhandClip, pxReadyStanceClip
		};
		std::string strGltfPath = strOutputDir + "StickFigure.gltf";
		if (Zenith_Tools_GltfExport::ExportToGltf(strGltfPath.c_str(), pxMesh, pxSkel, axGltfClips))
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "  Exported glTF to: %s", strGltfPath.c_str());
		}
	}

	// Cleanup
	delete pxReadyStanceClip;
	delete pxBackhandClip;
	delete pxForehandClip;
	delete pxServeClip;
	delete pxJumpClip;
	delete pxReloadClip;
	delete pxFireClip;
	delete pxAimClip;
	delete pxDeathClip;
	delete pxHitClip;
	delete pxDodgeClip;
	delete pxAttack3Clip;
	delete pxAttack2Clip;
	delete pxAttack1Clip;
	delete pxRunClip;
	delete pxWalkClip;
	delete pxIdleClip;
	delete pxMesh;
	delete pxSkel;

	Zenith_Log(LOG_CATEGORY_ASSET, "StickFigure assets generated at: %s", strOutputDir.c_str());
}

void GenerateRenderTestAssets()
{
	Zenith_Log(LOG_CATEGORY_ASSET, "Generating RenderTest game assets...");

	// Shared engine asset (any game can reference it). Mirrors how StickFigure
	// and ProceduralTree live under ENGINE_ASSETS_DIR/Meshes/. GAME_ASSETS_DIR
	// is per-game and isn't defined in the engine-library translation unit.
	const std::string strOutputDir = std::string(ENGINE_ASSETS_DIR) + "Meshes/Bullet_Sphere/";
	std::filesystem::create_directories(strOutputDir);

	const std::string strMeshAssetPath = strOutputDir + "Bullet_Sphere" ZENITH_MESH_ASSET_EXT;
	const std::string strModelPath     = strOutputDir + "Bullet_Sphere" ZENITH_MODEL_EXT;

	if (!std::filesystem::exists(strMeshAssetPath))
	{
		Zenith_MeshAsset xSphere;
		GenerateUnitSphereMeshAsset(xSphere, /*uSegments=*/16, /*uRings=*/8);
		xSphere.Export(strMeshAssetPath.c_str());
		Zenith_Log(LOG_CATEGORY_ASSET, "  Exported bullet sphere mesh to: %s", strMeshAssetPath.c_str());
	}

	if (!std::filesystem::exists(strModelPath))
	{
		Zenith_ModelAsset* pxModel = Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
		pxModel->SetName("BulletSphere");
		Zenith_Vector<std::string> xEmptyMaterials;
		pxModel->AddMeshByPath(strMeshAssetPath, xEmptyMaterials);
		pxModel->Export(strModelPath.c_str());
		Zenith_Log(LOG_CATEGORY_ASSET, "  Exported bullet sphere model to: %s", strModelPath.c_str());
	}

	Zenith_Log(LOG_CATEGORY_ASSET, "RenderTest assets generated at: %s", strOutputDir.c_str());
}

void GenerateTestAssets()
{
	Zenith_Log(LOG_CATEGORY_ASSET, "=== Generating Test Assets ===");
	GenerateStickFigureAssets();
	GenerateProceduralTreeAssets();
	GenerateRenderTestAssets();
	Zenith_Log(LOG_CATEGORY_ASSET, "=== Test Asset Generation Complete ===");
}

#include "Zenith_Tools_TestAssetExport.Tests.inl"
