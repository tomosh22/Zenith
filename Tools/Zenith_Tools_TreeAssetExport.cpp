#include "Zenith.h"

//=============================================================================
// Zenith_Tools_TreeAssetExport
//
// Procedural tree asset generation — the high-quality replacement for the
// original cube-per-bone test tree. Generates EVERYTHING the terrain editor's
// tree brush needs, at every editor boot (called from GenerateTestAssets):
//
//   ENGINE_ASSETS_DIR/Meshes/ProceduralTree/
//     Tree.zskel                 — branching skeleton (sway rig)
//     Tree_Trunk.zasset/.zmesh   — opaque trunk+branches (tapered tubes)
//     Tree_Leaves.zasset/.zmesh  — alpha-tested leaf cards (double-sided)
//     Tree_Trunk_Sway.zanmt      — VAT for the trunk mesh
//     Tree_Leaves_Sway.zanmt     — VAT for the leaves mesh
//     Tree_Bark_Albedo.ztxtr     — procedural bark albedo  (RGBA8 sRGB)
//     Tree_Bark_Normal.ztxtr     — procedural bark normals (RGBA8 linear)
//     Tree_Leaves_Albedo.ztxtr   — leaf-cluster albedo + ALPHA MAP (RGBA8 sRGB)
//     Tree_Bark.zmtrl            — opaque bark material
//     Tree_Leaves.zmtrl          — alpha-tested leaf material (cutoff 0.45)
//     Tree.zasset/.zmesh,
//     Tree_Static.zmesh,
//     Tree_Sway.zanmt, Tree_Sway.zanim, Tree.gltf
//                                — combined-mesh outputs kept for existing
//                                  consumers (Games/Exploration) + Blender.
//
// Everything is seeded — repeated boots regenerate byte-identical assets.
// The mesh/skeleton/VAT writers are shared with the StickFigure generator
// (Zenith_Tools_TestAssetExport.cpp).
//=============================================================================

#ifndef ZENITH_TOOLS

// Asset generation is a tools-build capability (the mesh/skeleton Export APIs
// only exist there); non-tools builds get a no-op so GenerateTestAssets links.
void GenerateProceduralTreeAssets()
{
}

#else

#include "Zenith_Tools_TestAssetExport.h"
#include "Zenith_Tools_GltfExport.h"

#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Collections/Zenith_Vector.h"
#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Flux/InstancedMeshes/Flux_AnimationTexture.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

namespace
{

//=============================================================================
// Deterministic randomness — repeated boots must regenerate identical assets.
//=============================================================================
struct TreeRng
{
	u_int m_uState;
	explicit TreeRng(u_int uSeed) : m_uState(uSeed != 0 ? uSeed : 0x9E3779B9u) {}

	u_int NextUInt()
	{
		u_int x = m_uState;
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		m_uState = x;
		return x;
	}
	float NextFloat01() { return static_cast<float>(NextUInt() & 0xFFFFFFu) / 16777215.0f; }
	float NextRange(float fMin, float fMax) { return fMin + (fMax - fMin) * NextFloat01(); }
};

float SmoothStepF(float fEdge0, float fEdge1, float fX)
{
	const float fT = std::clamp((fX - fEdge0) / (fEdge1 - fEdge0), 0.0f, 1.0f);
	return fT * fT * (3.0f - 2.0f * fT);
}

// 2D value noise (integer-hash based, deterministic) for the textures.
float HashToFloat(u_int uX, u_int uY, u_int uSeed)
{
	u_int h = uX * 374761393u + uY * 668265263u + uSeed * 2246822519u;
	h = (h ^ (h >> 13)) * 1274126177u;
	return static_cast<float>((h ^ (h >> 16)) & 0xFFFFFFu) / 16777215.0f;
}

float ValueNoise2D(float fX, float fY, u_int uSeed)
{
	const float fXF = std::floor(fX);
	const float fYF = std::floor(fY);
	const u_int uX0 = static_cast<u_int>(static_cast<int>(fXF) & 0xFFFF);
	const u_int uY0 = static_cast<u_int>(static_cast<int>(fYF) & 0xFFFF);
	const float fTX = SmoothStepF(0.0f, 1.0f, fX - fXF);
	const float fTY = SmoothStepF(0.0f, 1.0f, fY - fYF);
	const float fA = HashToFloat(uX0, uY0, uSeed);
	const float fB = HashToFloat(uX0 + 1, uY0, uSeed);
	const float fC = HashToFloat(uX0, uY0 + 1, uSeed);
	const float fD = HashToFloat(uX0 + 1, uY0 + 1, uSeed);
	return (fA * (1 - fTX) + fB * fTX) * (1 - fTY) + (fC * (1 - fTX) + fD * fTX) * fTY;
}

float FBM2D(float fX, float fY, u_int uOctaves, u_int uSeed)
{
	float fSum = 0.0f;
	float fAmp = 0.5f;
	float fFreq = 1.0f;
	for (u_int u = 0; u < uOctaves; u++)
	{
		fSum += ValueNoise2D(fX * fFreq, fY * fFreq, uSeed + u * 101u) * fAmp;
		fFreq *= 2.0f;
		fAmp *= 0.5f;
	}
	return fSum;
}

//=============================================================================
// Branch graph — generated first, then skeleton/meshes/sway derive from it.
//=============================================================================
struct TreeBranch
{
	int                   m_iParent = -1;       // index into the branch array
	Zenith_Maths::Vector3 m_xBase;              // world (bind) space
	Zenith_Maths::Vector3 m_xTip;
	float                 m_fBaseRadius = 0.1f;
	float                 m_fTipRadius = 0.05f;
	u_int                 m_uDepth = 0;         // 0 = trunk segment
	bool                  m_bLeafy = false;     // terminal — carries leaf cards
	float                 m_fSwayPhase = 0.0f;
	std::string           m_strBoneName;
	u_int                 m_uBone = 0;
};

void BuildTreeGraph(Zenith_Vector<TreeBranch>& xOut, TreeRng& xRng)
{
	// --- Trunk: 3 stacked segments with a slight organic lean drift. ---
	constexpr u_int uTRUNK_SEGMENTS = 3;
	const float fTrunkHeight = xRng.NextRange(6.6f, 7.4f);
	const float fSegLen = fTrunkHeight / uTRUNK_SEGMENTS;
	const float fTrunkBaseRadius = xRng.NextRange(0.30f, 0.34f);

	Zenith_Maths::Vector3 xCursor(0.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xDir(0.0f, 1.0f, 0.0f);
	for (u_int u = 0; u < uTRUNK_SEGMENTS; u++)
	{
		TreeBranch xSeg;
		xSeg.m_iParent = (u == 0) ? -1 : static_cast<int>(u - 1);
		xSeg.m_xBase = xCursor;
		// Lean drifts a little per segment.
		xDir = glm::normalize(xDir + Zenith_Maths::Vector3(
			xRng.NextRange(-0.06f, 0.06f), 0.0f, xRng.NextRange(-0.06f, 0.06f)));
		xCursor = xCursor + xDir * fSegLen;
		xSeg.m_xTip = xCursor;
		const float fT0 = static_cast<float>(u) / uTRUNK_SEGMENTS;
		const float fT1 = static_cast<float>(u + 1) / uTRUNK_SEGMENTS;
		xSeg.m_fBaseRadius = fTrunkBaseRadius * (1.0f - 0.62f * fT0);
		xSeg.m_fTipRadius  = fTrunkBaseRadius * (1.0f - 0.62f * fT1);
		xSeg.m_uDepth = 0;
		xSeg.m_fSwayPhase = 0.0f;
		xOut.PushBack(xSeg);
	}
	// Crown continuation: the trunk tip carries leaves too.
	xOut.Get(uTRUNK_SEGMENTS - 1).m_bLeafy = true;

	// --- Level-1 branches off the trunk. ---
	constexpr u_int uL1_COUNT = 6;
	for (u_int u = 0; u < uL1_COUNT; u++)
	{
		// Attach height climbs the trunk; spiral azimuth with jitter.
		const float fHeightT = 0.38f + 0.52f * (static_cast<float>(u) / (uL1_COUNT - 1));
		const float fAzimuth = static_cast<float>(u) * 2.399963f + xRng.NextRange(-0.35f, 0.35f); // golden angle
		const u_int uTrunkSeg = std::min(static_cast<u_int>(fHeightT * uTRUNK_SEGMENTS), uTRUNK_SEGMENTS - 1);
		const TreeBranch& xTrunk = xOut.Get(uTrunkSeg);
		const float fSegT = fHeightT * uTRUNK_SEGMENTS - uTrunkSeg;
		const Zenith_Maths::Vector3 xBase = xTrunk.m_xBase + (xTrunk.m_xTip - xTrunk.m_xBase) * fSegT;

		// Higher branches pitch up more and shorten (conical silhouette).
		const float fPitch = glm::radians(xRng.NextRange(28.0f, 44.0f) + fHeightT * 18.0f);
		const float fLength = xRng.NextRange(2.2f, 2.9f) * (1.0f - 0.38f * fHeightT);
		Zenith_Maths::Vector3 xBranchDir(
			cosf(fAzimuth) * cosf(fPitch), sinf(fPitch), sinf(fAzimuth) * cosf(fPitch));

		TreeBranch xB;
		xB.m_iParent = static_cast<int>(uTrunkSeg);
		xB.m_xBase = xBase;
		xB.m_xTip = xBase + xBranchDir * fLength;
		xB.m_fBaseRadius = xTrunk.m_fTipRadius * xRng.NextRange(0.42f, 0.52f);
		xB.m_fTipRadius = xB.m_fBaseRadius * 0.42f;
		xB.m_uDepth = 1;
		xB.m_bLeafy = true;     // L1 tips carry foliage as well
		xB.m_fSwayPhase = xRng.NextRange(0.0f, 6.2831f);
		xOut.PushBack(xB);
		const int iL1 = static_cast<int>(xOut.GetSize() - 1);

		// --- Level-2 children: two per L1 branch. ---
		for (u_int uChild = 0; uChild < 2; uChild++)
		{
			const float fAlongT = (uChild == 0) ? xRng.NextRange(0.45f, 0.6f) : xRng.NextRange(0.78f, 0.92f);
			const Zenith_Maths::Vector3 xChildBase = xB.m_xBase + (xB.m_xTip - xB.m_xBase) * fAlongT;
			// Deviate from the parent direction, biased upward.
			Zenith_Maths::Vector3 xChildDir = glm::normalize(xBranchDir +
				Zenith_Maths::Vector3(xRng.NextRange(-0.55f, 0.55f),
				                       xRng.NextRange(0.25f, 0.65f),
				                       xRng.NextRange(-0.55f, 0.55f)));
			const float fChildLen = fLength * xRng.NextRange(0.42f, 0.58f);

			TreeBranch xC;
			xC.m_iParent = iL1;
			xC.m_xBase = xChildBase;
			xC.m_xTip = xChildBase + xChildDir * fChildLen;
			xC.m_fBaseRadius = xB.m_fTipRadius * xRng.NextRange(0.7f, 0.9f);
			xC.m_fTipRadius = xC.m_fBaseRadius * 0.35f;
			xC.m_uDepth = 2;
			xC.m_bLeafy = true;
			xC.m_fSwayPhase = xRng.NextRange(0.0f, 6.2831f);
			xOut.PushBack(xC);
		}
	}

	// Bone names — stable, index-derived (the sway clip references them).
	for (u_int u = 0; u < xOut.GetSize(); u++)
	{
		char acName[32];
		snprintf(acName, sizeof(acName), "TreeBone_%u", u);
		xOut.Get(u).m_strBoneName = acName;
	}
}

//=============================================================================
// Skeleton — one bone per branch, positioned at the branch base. Geometry is
// authored in world (bind) space; the sway clip rotates about these origins.
//=============================================================================
Zenith_SkeletonAsset* CreateTreeSkeletonFromGraph(Zenith_Vector<TreeBranch>& xGraph)
{
	Zenith_SkeletonAsset* pxSkel = new Zenith_SkeletonAsset();
	const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
	const Zenith_Maths::Vector3 xUnitScale(1.0f, 1.0f, 1.0f);

	// Root at the ground.
	pxSkel->AddBone("TreeRoot", -1, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), xIdentity, xUnitScale);

	for (u_int u = 0; u < xGraph.GetSize(); u++)
	{
		TreeBranch& xB = xGraph.Get(u);
		const int iParentBone = (xB.m_iParent < 0)
			? 0
			: static_cast<int>(xGraph.Get(static_cast<u_int>(xB.m_iParent)).m_uBone);
		const Zenith_Maths::Vector3 xParentWorld = (xB.m_iParent < 0)
			? Zenith_Maths::Vector3(0.0f)
			: xGraph.Get(static_cast<u_int>(xB.m_iParent)).m_xBase;
		// AddBone takes the PARENT-LOCAL position.
		pxSkel->AddBone(xB.m_strBoneName.c_str(), iParentBone, xB.m_xBase - xParentWorld, xIdentity, xUnitScale);
		xB.m_uBone = pxSkel->GetNumBones() - 1;
	}

	pxSkel->ComputeBindPoseMatrices();
	return pxSkel;
}

//=============================================================================
// Trunk + branch mesh — tapered tubes. Each tube's BASE ring is skinned to
// the PARENT bone and the upper rings to the branch's own bone, so the sway
// clip bends each branch at its base without tearing the join.
//=============================================================================
void BuildOrthonormalBasis(const Zenith_Maths::Vector3& xDir,
                           Zenith_Maths::Vector3& xOutU, Zenith_Maths::Vector3& xOutV)
{
	const Zenith_Maths::Vector3 xRef = (fabsf(xDir.y) < 0.95f)
		? Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)
		: Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
	xOutU = glm::normalize(glm::cross(xRef, xDir));
	xOutV = glm::cross(xDir, xOutU);
}

Zenith_MeshAsset* CreateTreeTrunkMesh(const Zenith_Vector<TreeBranch>& xGraph)
{
	Zenith_MeshAsset* pxMesh = new Zenith_MeshAsset();
	pxMesh->Reserve(4096, 16384);

	constexpr u_int uRINGS = 4;            // rings along each branch (incl. base+tip)
	constexpr float fBARK_V_TILE = 0.45f;  // bark texture metres-per-tile along the branch

	for (u_int uB = 0; uB < xGraph.GetSize(); uB++)
	{
		const TreeBranch& xB = xGraph.Get(uB);
		const u_int uRadial = (xB.m_uDepth == 0) ? 12u : (xB.m_uDepth == 1 ? 9u : 7u);

		const Zenith_Maths::Vector3 xAxis = glm::normalize(xB.m_xTip - xB.m_xBase);
		const float fLen = glm::length(xB.m_xTip - xB.m_xBase);
		Zenith_Maths::Vector3 xU, xV;
		BuildOrthonormalBasis(xAxis, xU, xV);

		const u_int uParentBone = (xB.m_iParent < 0)
			? 0u
			: xGraph.Get(static_cast<u_int>(xB.m_iParent)).m_uBone;

		const uint32_t uFirstVert = pxMesh->GetNumVerts();

		for (u_int uRing = 0; uRing < uRINGS; uRing++)
		{
			const float fT = static_cast<float>(uRing) / (uRINGS - 1);
			const float fRadius = xB.m_fBaseRadius + (xB.m_fTipRadius - xB.m_fBaseRadius) * fT;
			const Zenith_Maths::Vector3 xCentre = xB.m_xBase + xAxis * (fLen * fT);
			// Base ring welds to the parent; upper rings ride this branch's bone.
			const u_int uRingBone = (uRing == 0) ? uParentBone : xB.m_uBone;

			for (u_int uSeg = 0; uSeg <= uRadial; uSeg++)   // +1 duplicates the UV seam
			{
				const float fAngle = (static_cast<float>(uSeg) / uRadial) * 6.2831853f;
				const Zenith_Maths::Vector3 xRadial = xU * cosf(fAngle) + xV * sinf(fAngle);
				const Zenith_Maths::Vector3 xPos = xCentre + xRadial * fRadius;
				const Zenith_Maths::Vector3 xNormal = xRadial;
				const Zenith_Maths::Vector3 xTangent = glm::normalize(glm::cross(xAxis, xRadial));
				const Zenith_Maths::Vector2 xUV(
					static_cast<float>(uSeg) / uRadial * (xB.m_uDepth == 0 ? 2.0f : 1.0f),
					(fLen * fT) / fBARK_V_TILE);

				pxMesh->AddVertex(xPos, xNormal, xUV, xTangent);
				pxMesh->SetVertexSkinning(pxMesh->GetNumVerts() - 1,
					glm::uvec4(uRingBone, 0, 0, 0), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
			}
		}

		// Stitch the rings.
		const u_int uRowStride = uRadial + 1;
		for (u_int uRing = 0; uRing + 1 < uRINGS; uRing++)
		{
			for (u_int uSeg = 0; uSeg < uRadial; uSeg++)
			{
				const uint32_t uA = uFirstVert + uRing * uRowStride + uSeg;
				const uint32_t uBv = uA + 1;
				const uint32_t uC = uA + uRowStride;
				const uint32_t uD = uC + 1;
				pxMesh->AddTriangle(uA, uC, uBv);
				pxMesh->AddTriangle(uBv, uC, uD);
			}
		}

		// Tip cap (small fan) so trunk/branch ends aren't open.
		const uint32_t uTipCentre = pxMesh->GetNumVerts();
		pxMesh->AddVertex(xB.m_xTip, xAxis, Zenith_Maths::Vector2(0.5f, 0.99f), xU);
		pxMesh->SetVertexSkinning(uTipCentre, glm::uvec4(xB.m_uBone, 0, 0, 0), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
		const uint32_t uTipRing = uFirstVert + (uRINGS - 1) * uRowStride;
		for (u_int uSeg = 0; uSeg < uRadial; uSeg++)
		{
			pxMesh->AddTriangle(uTipRing + uSeg, uTipCentre, uTipRing + uSeg + 1);
		}
	}

	pxMesh->AddSubmesh(0, pxMesh->GetNumIndices(), 0);
	pxMesh->ComputeBounds();
	return pxMesh;
}

//=============================================================================
// Leaf-card mesh — clusters of crossed quads at leafy branch tips, skinned
// to the branch bone so they sway with it. Both windings are emitted (the
// instanced pipeline backface-culls; leaves must read from both sides).
// Normals are biased upward for soft foliage lighting.
//=============================================================================
Zenith_MeshAsset* CreateTreeLeavesMesh(const Zenith_Vector<TreeBranch>& xGraph, TreeRng& xRng)
{
	Zenith_MeshAsset* pxMesh = new Zenith_MeshAsset();
	pxMesh->Reserve(4096, 8192);

	auto AddLeafCard = [&](const Zenith_Maths::Vector3& xCentre, float fSize,
	                       float fYaw, float fPitch, u_int uBone)
	{
		const float fCY = cosf(fYaw), fSY = sinf(fYaw);
		const float fCP = cosf(fPitch), fSP = sinf(fPitch);
		// Card basis: right (yaw), up (pitched).
		const Zenith_Maths::Vector3 xRight(fCY, 0.0f, fSY);
		const Zenith_Maths::Vector3 xUp = glm::normalize(Zenith_Maths::Vector3(-fSY * fSP, fCP, fCY * fSP));
		Zenith_Maths::Vector3 xNormal = glm::cross(xRight, xUp);
		// Up-biased normal: softer, less harsh side lighting on foliage.
		xNormal = glm::normalize(xNormal * 0.5f + Zenith_Maths::Vector3(0.0f, 0.85f, 0.0f));

		const float fHalf = fSize * 0.5f;
		const Zenith_Maths::Vector3 axCorners[4] = {
			xCentre - xRight * fHalf - xUp * fHalf,
			xCentre + xRight * fHalf - xUp * fHalf,
			xCentre + xRight * fHalf + xUp * fHalf,
			xCentre - xRight * fHalf + xUp * fHalf,
		};
		const Zenith_Maths::Vector2 axUVs[4] = {
			{ 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f } };

		const uint32_t uFirst = pxMesh->GetNumVerts();
		for (u_int u = 0; u < 4; u++)
		{
			pxMesh->AddVertex(axCorners[u], xNormal, axUVs[u], xRight);
			pxMesh->SetVertexSkinning(pxMesh->GetNumVerts() - 1,
				glm::uvec4(uBone, 0, 0, 0), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
		}
		// Both windings — double-sided under backface culling.
		pxMesh->AddTriangle(uFirst + 0, uFirst + 1, uFirst + 2);
		pxMesh->AddTriangle(uFirst + 0, uFirst + 2, uFirst + 3);
		pxMesh->AddTriangle(uFirst + 0, uFirst + 2, uFirst + 1);
		pxMesh->AddTriangle(uFirst + 0, uFirst + 3, uFirst + 2);
	};

	for (u_int uB = 0; uB < xGraph.GetSize(); uB++)
	{
		const TreeBranch& xB = xGraph.Get(uB);
		if (!xB.m_bLeafy)
		{
			continue;
		}

		const Zenith_Maths::Vector3 xAxis = glm::normalize(xB.m_xTip - xB.m_xBase);
		const u_int uCards = (xB.m_uDepth == 0) ? 9u : (xB.m_uDepth == 1 ? 6u : 5u);
		const float fBaseSize = (xB.m_uDepth == 0) ? 1.7f : (xB.m_uDepth == 1 ? 1.45f : 1.2f);

		for (u_int uCard = 0; uCard < uCards; uCard++)
		{
			// Cluster along the outer half of the branch, puffing outward.
			const float fAlong = xRng.NextRange(0.55f, 1.05f);
			const Zenith_Maths::Vector3 xJitter(
				xRng.NextRange(-0.55f, 0.55f), xRng.NextRange(-0.25f, 0.5f), xRng.NextRange(-0.55f, 0.55f));
			const Zenith_Maths::Vector3 xCentre =
				xB.m_xBase + xAxis * (glm::length(xB.m_xTip - xB.m_xBase) * fAlong) + xJitter;

			AddLeafCard(xCentre, fBaseSize * xRng.NextRange(0.8f, 1.25f),
				xRng.NextRange(0.0f, 6.2831f), xRng.NextRange(-0.5f, 0.5f), xB.m_uBone);
		}
	}

	pxMesh->AddSubmesh(0, pxMesh->GetNumIndices(), 0);
	pxMesh->ComputeBounds();
	return pxMesh;
}

//=============================================================================
// Sway animation — layered-sine rotations about wind-perpendicular axes,
// amplitude growing with branch depth, per-branch phase offsets. 4-second
// loop with matching first/last keys.
//=============================================================================
Flux_AnimationClip* CreateTreeSwayClipFromGraph(const Zenith_Vector<TreeBranch>& xGraph)
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Sway");
	pxClip->SetDuration(4.0f);
	pxClip->SetTicksPerSecond(30);
	pxClip->SetLooping(true);

	constexpr u_int uKEYS = 17;            // every 7.5 ticks across 120 ticks
	constexpr float fTOTAL_TICKS = 120.0f;

	for (u_int uB = 0; uB < xGraph.GetSize(); uB++)
	{
		const TreeBranch& xB = xGraph.Get(uB);
		const float fAmplitudeDeg = (xB.m_uDepth == 0) ? 1.4f : (xB.m_uDepth == 1 ? 4.5f : 7.5f);
		const float fPhase = xB.m_fSwayPhase;

		Flux_BoneChannel xChannel;
		for (u_int uKey = 0; uKey < uKEYS; uKey++)
		{
			const float fT = static_cast<float>(uKey) / (uKEYS - 1);   // 0..1, loops
			const float fW = fT * 6.2831853f;                          // one full cycle
			// Primary sway + a faster secondary ripple (phase-shifted per bone).
			const float fMain  = sinf(fW + fPhase);
			const float fRipple = 0.35f * sinf(2.0f * fW + fPhase * 1.7f + 1.3f);
			const float fAngle = glm::radians(fAmplitudeDeg) * (fMain + fRipple) / 1.35f;
			// Sway about Z (wind along X) with a slight X-axis wobble.
			const Zenith_Maths::Quat xRot =
				glm::angleAxis(fAngle, Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f)) *
				glm::angleAxis(fAngle * 0.4f, Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
			xChannel.AddRotationKeyframe(fT * fTOTAL_TICKS, xRot);
		}
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel(xB.m_strBoneName, std::move(xChannel));
	}

	return pxClip;
}

//=============================================================================
// Procedural textures.
//=============================================================================
void WriteTreeZtxtr(const std::string& strPath, int32_t iSize, TextureFormat eFormat,
                    const Zenith_Vector<u_int8>& xPixels)
{
	Zenith_DataStream xStream;
	xStream << iSize;
	xStream << iSize;
	xStream << static_cast<int32_t>(1);
	xStream << eFormat;
	xStream << static_cast<u_int64>(xPixels.GetSize());
	xStream.WriteData(xPixels.GetDataPointer(), xPixels.GetSize());
	xStream.WriteToFile(strPath.c_str());
}

void GenerateBarkTextures(const std::string& strDir)
{
	constexpr int32_t iSIZE = 512;
	Zenith_Vector<u_int8> xAlbedo(iSIZE * iSIZE * 4);
	Zenith_Vector<u_int8> xNormal(iSIZE * iSIZE * 4);
	Zenith_Vector<float> xHeight(iSIZE * iSIZE);
	for (int32_t i = 0; i < iSIZE * iSIZE * 4; i++) { xAlbedo.PushBack(0); xNormal.PushBack(0); }
	for (int32_t i = 0; i < iSIZE * iSIZE; i++) { xHeight.PushBack(0.0f); }

	// Height field: vertical bark ridges (high-frequency in X, stretched in Y)
	// + large furrows + fine grain.
	for (int32_t iY = 0; iY < iSIZE; iY++)
	{
		for (int32_t iX = 0; iX < iSIZE; iX++)
		{
			const float fU = static_cast<float>(iX) / iSIZE;
			const float fV = static_cast<float>(iY) / iSIZE;
			// Tileable-ish: sample noise on a wrapped domain.
			const float fRidges = FBM2D(fU * 24.0f, fV * 4.0f, 4, 71u);
			const float fFurrow = FBM2D(fU * 6.0f, fV * 1.5f, 3, 137u);
			const float fGrain = ValueNoise2D(fU * 90.0f, fV * 90.0f, 211u);
			float fH = fRidges * 0.62f + fFurrow * 0.3f + fGrain * 0.08f;
			// Crack accents: deep cuts where the ridge noise dips.
			const float fCrack = SmoothStepF(0.34f, 0.26f, fRidges);
			fH -= fCrack * 0.35f;
			xHeight.Get(iY * iSIZE + iX) = fH;
		}
	}

	for (int32_t iY = 0; iY < iSIZE; iY++)
	{
		for (int32_t iX = 0; iX < iSIZE; iX++)
		{
			const float fH = std::clamp(xHeight.Get(iY * iSIZE + iX), 0.0f, 1.0f);
			const float fU = static_cast<float>(iX) / iSIZE;
			const float fV = static_cast<float>(iY) / iSIZE;

			// Albedo: dark crevices -> grey-brown ridge plates, subtle hue drift.
			const float fHue = ValueNoise2D(fU * 3.0f, fV * 2.0f, 307u);
			const float fR = 0.16f + fH * 0.30f + fHue * 0.05f;
			const float fG = 0.115f + fH * 0.235f + fHue * 0.035f;
			const float fB = 0.085f + fH * 0.16f;

			u_int8* pucA = &xAlbedo.Get((iY * iSIZE + iX) * 4);
			pucA[0] = static_cast<u_int8>(std::clamp(fR, 0.0f, 1.0f) * 255.0f);
			pucA[1] = static_cast<u_int8>(std::clamp(fG, 0.0f, 1.0f) * 255.0f);
			pucA[2] = static_cast<u_int8>(std::clamp(fB, 0.0f, 1.0f) * 255.0f);
			pucA[3] = 255;

			// Normal from height (central differences, wrapped).
			const int32_t iXP = (iX + 1) % iSIZE, iXM = (iX + iSIZE - 1) % iSIZE;
			const int32_t iYP = (iY + 1) % iSIZE, iYM = (iY + iSIZE - 1) % iSIZE;
			const float fDX = (xHeight.Get(iY * iSIZE + iXP) - xHeight.Get(iY * iSIZE + iXM)) * 2.4f;
			const float fDY = (xHeight.Get(iYP * iSIZE + iX) - xHeight.Get(iYM * iSIZE + iX)) * 2.4f;
			Zenith_Maths::Vector3 xN = glm::normalize(Zenith_Maths::Vector3(-fDX, -fDY, 1.0f));
			u_int8* pucN = &xNormal.Get((iY * iSIZE + iX) * 4);
			pucN[0] = static_cast<u_int8>((xN.x * 0.5f + 0.5f) * 255.0f);
			pucN[1] = static_cast<u_int8>((xN.y * 0.5f + 0.5f) * 255.0f);
			pucN[2] = static_cast<u_int8>((xN.z * 0.5f + 0.5f) * 255.0f);
			pucN[3] = 255;
		}
	}

	WriteTreeZtxtr(strDir + "Tree_Bark_Albedo" ZENITH_TEXTURE_EXT, iSIZE, TEXTURE_FORMAT_RGBA8_SRGB, xAlbedo);
	WriteTreeZtxtr(strDir + "Tree_Bark_Normal" ZENITH_TEXTURE_EXT, iSIZE, TEXTURE_FORMAT_RGBA8_UNORM, xNormal);
}

void GenerateLeafClusterTexture(const std::string& strDir)
{
	constexpr int32_t iSIZE = 1024;
	Zenith_Vector<u_int8> xPixels(iSIZE * iSIZE * 4);
	for (int32_t i = 0; i < iSIZE * iSIZE * 4; i++) { xPixels.PushBack(0); }

	TreeRng xRng(40411u);

	// Layered individual leaves: back layer darker (depth cue), front brighter.
	constexpr u_int uLEAVES = 30;
	for (u_int uLeaf = 0; uLeaf < uLEAVES; uLeaf++)
	{
		const float fLayerT = static_cast<float>(uLeaf) / (uLEAVES - 1);   // 0 back .. 1 front
		// Cluster centres bias toward the middle of the card.
		const float fCX = iSIZE * (0.5f + (xRng.NextFloat01() - 0.5f) * 0.72f);
		const float fCY = iSIZE * (0.5f + (xRng.NextFloat01() - 0.5f) * 0.72f);
		const float fAngle = xRng.NextRange(0.0f, 6.2831f);
		const float fLen = xRng.NextRange(0.17f, 0.26f) * iSIZE;
		const float fWidth = fLen * xRng.NextRange(0.40f, 0.5f);

		// Per-leaf colour: green base with brightness/hue variation by layer.
		const float fBright = 0.55f + 0.45f * fLayerT;
		const float fHueJit = xRng.NextRange(-0.05f, 0.08f);
		const float fLR = (0.13f + fHueJit * 0.6f) * fBright;
		const float fLG = (0.34f + fHueJit) * fBright;
		const float fLB = 0.07f * fBright;

		const float fCA = cosf(fAngle), fSA = sinf(fAngle);

		// Rasterize the leaf's bounding area.
		const int32_t iRadius = static_cast<int32_t>(fLen * 0.62f) + 2;
		const int32_t iCXi = static_cast<int32_t>(fCX);
		const int32_t iCYi = static_cast<int32_t>(fCY);
		for (int32_t iY = std::max(0, iCYi - iRadius); iY < std::min(iSIZE, iCYi + iRadius); iY++)
		{
			for (int32_t iX = std::max(0, iCXi - iRadius); iX < std::min(iSIZE, iCXi + iRadius); iX++)
			{
				// Leaf-local coords: s along the midrib (0 stem .. 1 tip), t across.
				const float fDXp = static_cast<float>(iX) - fCX;
				const float fDYp = static_cast<float>(iY) - fCY;
				const float fAlong = (fDXp * fCA + fDYp * fSA) / fLen + 0.5f;
				const float fAcross = (-fDXp * fSA + fDYp * fCA) / fWidth;
				if (fAlong < 0.0f || fAlong > 1.0f)
				{
					continue;
				}
				// Leaf outline: pointed ellipse with serrated edge.
				const float fProfile = powf(sinf(fAlong * 3.14159f), 0.62f) * 0.5f;
				const float fSerration = 0.035f * sinf(fAlong * 60.0f + uLeaf * 1.7f);
				const float fEdge = fProfile + fSerration;
				const float fDist = fabsf(fAcross);
				if (fDist > fEdge)
				{
					continue;
				}

				// Shading: vein structure + edge darkening + base-to-tip gradient.
				float fShade = 0.82f + 0.18f * (1.0f - fDist / std::max(fEdge, 0.001f));
				fShade *= 0.9f + 0.1f * fAlong;
				// Midrib (dark line) + side veins.
				if (fDist < 0.018f) { fShade *= 0.66f; }
				const float fVein = fabsf(sinf(fAlong * 22.0f + fDist * 9.0f));
				if (fVein > 0.93f) { fShade *= 0.84f; }

				u_int8* pucP = &xPixels.Get((iY * iSIZE + iX) * 4);
				// Front leaves overwrite back leaves (painter's order).
				pucP[0] = static_cast<u_int8>(std::clamp(fLR * fShade, 0.0f, 1.0f) * 255.0f);
				pucP[1] = static_cast<u_int8>(std::clamp(fLG * fShade, 0.0f, 1.0f) * 255.0f);
				pucP[2] = static_cast<u_int8>(std::clamp(fLB * fShade, 0.0f, 1.0f) * 255.0f);
				// Alpha map: soft edge over the outer 8% of the leaf profile.
				const float fAlpha = SmoothStepF(1.0f, 0.92f, fDist / std::max(fEdge, 0.001f));
				pucP[3] = std::max(pucP[3], static_cast<u_int8>(fAlpha * 255.0f));
			}
		}
	}

	WriteTreeZtxtr(strDir + "Tree_Leaves_Albedo" ZENITH_TEXTURE_EXT, iSIZE, TEXTURE_FORMAT_RGBA8_SRGB, xPixels);
}

//=============================================================================
// Materials.
//=============================================================================
void GenerateTreeMaterials(const std::string& strDir)
{
	{
		auto xhBark = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		Zenith_MaterialAsset* pxBark = xhBark.GetDirect();
		pxBark->SetName("TreeBark");
		pxBark->SetDiffuseTexture(TextureHandle("engine:Meshes/ProceduralTree/Tree_Bark_Albedo" ZENITH_TEXTURE_EXT));
		pxBark->SetNormalTexture(TextureHandle("engine:Meshes/ProceduralTree/Tree_Bark_Normal" ZENITH_TEXTURE_EXT));
		pxBark->SetRoughness(0.92f);
		pxBark->SetMetallic(0.0f);
		pxBark->SaveToFile(strDir + "Tree_Bark.zmtrl");
	}
	{
		auto xhLeaves = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		Zenith_MaterialAsset* pxLeaves = xhLeaves.GetDirect();
		pxLeaves->SetName("TreeLeaves");
		pxLeaves->SetDiffuseTexture(TextureHandle("engine:Meshes/ProceduralTree/Tree_Leaves_Albedo" ZENITH_TEXTURE_EXT));
		pxLeaves->SetRoughness(0.78f);
		pxLeaves->SetMetallic(0.0f);
		// Alpha-tested foliage: the leaf albedo's alpha channel is a real leaf-shape mask,
		// so the material MUST be MASKED — only then does BuildMaterialDrawConstants feed a
		// non-zero cutoff to the shader's discard (OPAQUE writes cutoff 0). Without this the
		// leaf cards render as opaque quads (the leaf texture on a black square).
		pxLeaves->SetBlendMode(MATERIAL_BLEND_MASKED);
		pxLeaves->SetAlphaCutoff(0.45f);
		pxLeaves->SaveToFile(strDir + "Tree_Leaves.zmtrl");
	}
}

//=============================================================================
// Combined mesh (compat for Games/Exploration + the Blender glTF export).
//=============================================================================
Zenith_MeshAsset* CreateCombinedTreeMesh(const Zenith_MeshAsset* pxTrunk, const Zenith_MeshAsset* pxLeaves)
{
	Zenith_MeshAsset* pxMesh = new Zenith_MeshAsset();
	pxMesh->Reserve(pxTrunk->GetNumVerts() + pxLeaves->GetNumVerts(),
	                pxTrunk->GetNumIndices() + pxLeaves->GetNumIndices());

	auto Append = [&](const Zenith_MeshAsset* pxSrc)
	{
		const uint32_t uBase = pxMesh->GetNumVerts();
		for (uint32_t u = 0; u < pxSrc->GetNumVerts(); u++)
		{
			pxMesh->AddVertex(pxSrc->m_xPositions.Get(u), pxSrc->m_xNormals.Get(u),
				pxSrc->m_xUVs.Get(u), pxSrc->m_xTangents.Get(u));
			pxMesh->SetVertexSkinning(pxMesh->GetNumVerts() - 1,
				pxSrc->m_xBoneIndices.Get(u), pxSrc->m_xBoneWeights.Get(u));
		}
		for (uint32_t u = 0; u < pxSrc->GetNumIndices(); u += 3)
		{
			pxMesh->AddTriangle(uBase + pxSrc->m_xIndices.Get(u),
				uBase + pxSrc->m_xIndices.Get(u + 1), uBase + pxSrc->m_xIndices.Get(u + 2));
		}
	};
	Append(pxTrunk);
	Append(pxLeaves);

	pxMesh->AddSubmesh(0, pxMesh->GetNumIndices(), 0);
	pxMesh->ComputeBounds();
	return pxMesh;
}

void ExportTreeMeshSet(const std::string& strDir, const char* szBaseName,
                       Zenith_MeshAsset* pxMesh, Zenith_SkeletonAsset* pxSkel,
                       Flux_AnimationClip* pxSwayClip)
{
	pxMesh->SetSkeletonPath("Meshes/ProceduralTree/Tree" ZENITH_SKELETON_EXT);

	const std::string strAssetPath = strDir + szBaseName + ZENITH_MESH_ASSET_EXT;
	pxMesh->Export(strAssetPath.c_str());

	Flux_MeshGeometry* pxGeometry = Zenith_Tools_CreateFluxMeshGeometry(pxMesh, pxSkel);
	const std::string strMeshPath = strDir + szBaseName + ZENITH_MESH_EXT;
	pxGeometry->Export(strMeshPath.c_str());

	Flux_AnimationTexture* pxVAT = new Flux_AnimationTexture();
	Zenith_Vector<Flux_AnimationClip*> axAnimations;
	axAnimations.PushBack(pxSwayClip);
	if (pxVAT->BakeFromAnimations(pxGeometry, pxSkel, axAnimations, 30))
	{
		pxVAT->Export(strDir + szBaseName + std::string("_Sway.zanmt"));
		Zenith_Log(LOG_CATEGORY_ASSET, "  %s VAT: %u x %u (verts x frames)",
			szBaseName, pxVAT->GetTextureWidth(), pxVAT->GetTextureHeight());
	}
	else
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "  %s VAT bake FAILED", szBaseName);
	}
	delete pxVAT;
	delete pxGeometry;
}

} // namespace

//=============================================================================
// Entry point — called from GenerateTestAssets() at every editor boot.
//=============================================================================
void GenerateProceduralTreeAssets()
{
	Zenith_Log(LOG_CATEGORY_ASSET, "Generating ProceduralTree assets (branching tree, bark+leaf textures, sway VATs)...");

	const std::string strOutputDir = std::string(ENGINE_ASSETS_DIR) + "Meshes/ProceduralTree/";
	std::filesystem::create_directories(strOutputDir);

	// Branch graph + skeleton + meshes + sway — all from one fixed seed so
	// every boot regenerates byte-identical assets.
	TreeRng xRng(7331u);
	Zenith_Vector<TreeBranch> xGraph;
	BuildTreeGraph(xGraph, xRng);

	Zenith_SkeletonAsset* pxSkel = CreateTreeSkeletonFromGraph(xGraph);
	pxSkel->Export((strOutputDir + "Tree" ZENITH_SKELETON_EXT).c_str());

	Zenith_MeshAsset* pxTrunk = CreateTreeTrunkMesh(xGraph);
	Zenith_MeshAsset* pxLeaves = CreateTreeLeavesMesh(xGraph, xRng);
	Flux_AnimationClip* pxSwayClip = CreateTreeSwayClipFromGraph(xGraph);

	Zenith_Log(LOG_CATEGORY_ASSET, "  Tree graph: %u branches; trunk %u verts, leaves %u verts",
		xGraph.GetSize(), pxTrunk->GetNumVerts(), pxLeaves->GetNumVerts());

	// Textures + materials.
	GenerateBarkTextures(strOutputDir);
	GenerateLeafClusterTexture(strOutputDir);
	GenerateTreeMaterials(strOutputDir);

	// Split meshes + VATs (the terrain editor's tree brush consumes these).
	ExportTreeMeshSet(strOutputDir, "Tree_Trunk", pxTrunk, pxSkel, pxSwayClip);
	ExportTreeMeshSet(strOutputDir, "Tree_Leaves", pxLeaves, pxSkel, pxSwayClip);

	// Combined-mesh outputs kept for existing consumers (Exploration, Blender).
	Zenith_MeshAsset* pxCombined = CreateCombinedTreeMesh(pxTrunk, pxLeaves);
	ExportTreeMeshSet(strOutputDir, "Tree", pxCombined, pxSkel, pxSwayClip);
	{
		Flux_MeshGeometry* pxStatic = Zenith_Tools_CreateStaticFluxMeshGeometry(pxCombined);
		pxStatic->Export((strOutputDir + "Tree_Static" ZENITH_MESH_EXT).c_str());
		delete pxStatic;
	}
	pxSwayClip->Export(strOutputDir + "Tree_Sway" ZENITH_ANIMATION_EXT);
	{
		std::vector<const Flux_AnimationClip*> axClips = { pxSwayClip };
		Zenith_Tools_GltfExport::ExportToGltf((strOutputDir + "Tree.gltf").c_str(), pxCombined, pxSkel, axClips);
	}

	delete pxCombined;
	delete pxSwayClip;
	delete pxLeaves;
	delete pxTrunk;
	delete pxSkel;

	Zenith_Log(LOG_CATEGORY_ASSET, "ProceduralTree assets generated at: %s", strOutputDir.c_str());
}

#endif // ZENITH_TOOLS
