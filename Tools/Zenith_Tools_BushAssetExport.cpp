#include "Zenith.h"

//=============================================================================
// Zenith_Tools_BushAssetExport
//
// The SHARED wind-animated bush set — the fifth generated engine set, and the
// first one after the ProceduralTree to be ANIMATED. Generated at every tools
// boot from fixed seeds, exactly like the rock and deadwood sets beside it.
//
//   ENGINE_ASSETS_DIR/Meshes/Bushes/
//     Bush_Broad.{zasset,zmesh,zskel}  + Bush_Broad_Sway.zanmt    — wide dome shrub
//     Bush_Mound.{...}                 + Bush_Mound_Sway.zanmt    — low round mound
//     Bush_Spindly.{...}               + Bush_Spindly_Sway.zanmt  — tall sparse upright
//     Bush_Foliage_Albedo.ztxtr        — leaf-cluster albedo + ALPHA MASK (sRGB)
//     Bush_Foliage_Normal.ztxtr        — per-leaf dome + midrib crease (BC5 linear)
//     Bush_Foliage_AO.ztxtr            — stem-end + overlap occlusion (linear)
//     Bush_Foliage.zmtrl               — MASKED (cutoff 0.45), TWO-SIDED,
//                                        SUBSURFACE foliage material
//
// ★ ONE instance group per bush, not the tree's lockstep trunk+leaves pair.
// An instance group is single-material, and the tree splits because its trunk
// is OPAQUE while its leaves are MASKED — two materials, two groups, forced.
// A bush is foliage all the way through: the card shell is dense enough that
// interior stems would never be seen, so an opaque stem group would double the
// entity count and the serialized instance data for invisible geometry. If a
// game ever wants leggy, see-through bushes, that is a second lockstep group
// authored the tree's way — not a change to this set.
//
// ★ A VAT NEEDS A SKINNED MESH, so the chain is the tree's, not the rocks':
// branch graph -> skeleton (one bone per branch, AddBone takes PARENT-LOCAL
// positions) -> cards skinned to their branch bone -> a per-bone rotation clip
// (keyframe times in TICKS) -> Zenith_Tools_CreateFluxMeshGeometry (the
// SKINNED converter — CreateStaticFluxMeshGeometry drops the bone lanes and
// the bake would have nothing to deform) -> Flux_AnimationTexture::
// BakeFromAnimations. Per-instance phase is NOT serialized; the component
// re-derives it on load (instanceID * golden ratio), so a reloaded scene
// comes back de-synchronised for free.
//
// ★ THE FOLIAGE MATERIAL MUST BE MATERIAL_BLEND_MASKED WITH A NON-ZERO
// CUTOFF. BuildMaterialDrawConstants writes cutoff 0 for OPAQUE, the shader
// then never discards, and every card renders as the leaf texture on an
// opaque black square. This is the tree-leaves trap, inherited verbatim.
//
// ★ m_fHeightMetres MEANS metres, EXACTLY. After the cards are placed the
// whole cluster (branch graph included, since the skeleton is built from it
// afterwards) is uniformly rescaled so the highest card corner sits at
// exactly the authored height — so the scatter's bounds and sink values are
// derived from a number that is true, not from a band. The deadwood learned
// the band version of this lesson the hard way (its stump capsule was sized
// from the axis and left the real top uncovered).
//=============================================================================

#ifndef ZENITH_TOOLS

// Asset generation is a tools-build capability (the mesh/skeleton Export APIs
// only exist there); non-tools builds get a no-op so GenerateTestAssets links.
void GenerateBushAssets()
{
}

#else

#include "Zenith_Tools_TestAssetExport.h"

#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "Zenith_Tools_TextureExport.h"   // the ONE .ztxtr writer
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
// Export version. Every boot regenerates this set unconditionally, so this is
// not a staleness gate — it is the number a log line (and a bug report) can
// name when a capture and the current generator disagree. BUMP IT whenever the
// emitted bytes change.
//
// 2: foliage normal + AO maps, two-sided SUBSURFACE foliage material.
//=============================================================================
constexpr u_int uBUSH_ASSET_EXPORT_VERSION = 2u;

//=============================================================================
// Deterministic randomness — repeated boots must regenerate identical assets.
//=============================================================================
struct BushRng
{
	u_int m_uState;
	explicit BushRng(u_int uSeed) : m_uState(uSeed != 0 ? uSeed : 0x9E3779B9u) {}

	u_int NextUInt()
	{
		u_int uX = m_uState;
		uX ^= uX << 13;
		uX ^= uX >> 17;
		uX ^= uX << 5;
		m_uState = uX;
		return uX;
	}
	float NextFloat01() { return static_cast<float>(NextUInt() & 0xFFFFFFu) / 16777215.0f; }
	float NextRange(float fMin, float fMax) { return fMin + (fMax - fMin) * NextFloat01(); }
};

float SmoothStepB(float fEdge0, float fEdge1, float fX)
{
	const float fT = std::clamp((fX - fEdge0) / (fEdge1 - fEdge0), 0.0f, 1.0f);
	return fT * fT * (3.0f - 2.0f * fT);
}

//=============================================================================
// The variant table. ONE definition, read by the exporter AND the unit tests —
// same reasoning as RockVariantAt / FallenTreeVariantAt: a test that
// re-declares production parameters drifts (that has happened within the hour
// such a copy existed).
//=============================================================================
enum BushVariant
{
	BUSH_BROAD = 0,
	BUSH_MOUND,
	BUSH_SPINDLY,
	BUSH_VARIANT_COUNT
};

struct BushVariantSpec
{
	const char* m_szName            = "Bush_Broad";
	u_int       m_uSeed             = 0u;
	u_int       m_uBranches         = 8u;
	float       m_fBranchLenMin     = 0.9f;   // pre-normalisation metres (see m_fHeightMetres)
	float       m_fBranchLenMax     = 1.2f;
	float       m_fElevMinDeg       = 20.0f;  // branch pitch above horizontal
	float       m_fElevMaxDeg       = 45.0f;
	u_int       m_uCardsPerBranch   = 6u;
	float       m_fCardSizeMin      = 0.50f;
	float       m_fCardSizeMax      = 0.80f;
	float       m_fCardJitter       = 0.25f;  // outward scatter of card centres off the branch axis
	float       m_fHeightMetres     = 1.10f;  // EXACT final height of the highest card corner
	float       m_fSwayAmplitudeDeg = 6.0f;   // branch-bone peak rotation in the sway clip
	float       m_fInteriorTint     = 0.72f;  // vertex tint at the bush core; the rim is exactly 1.0
};

BushVariantSpec BushVariantAt(int iVariant)
{
	BushVariantSpec xSpec;
	switch (iVariant)
	{
	case BUSH_BROAD:
		// The hero piece: a waist-high dome wider than it is tall.
		xSpec.m_szName = "Bush_Broad";
		xSpec.m_uSeed = 15401u;
		xSpec.m_uBranches = 9u;
		xSpec.m_fBranchLenMin = 0.95f;
		xSpec.m_fBranchLenMax = 1.35f;
		xSpec.m_fElevMinDeg = 16.0f;
		xSpec.m_fElevMaxDeg = 40.0f;
		xSpec.m_uCardsPerBranch = 7u;
		xSpec.m_fCardSizeMin = 0.55f;
		xSpec.m_fCardSizeMax = 0.85f;
		xSpec.m_fCardJitter = 0.30f;
		xSpec.m_fHeightMetres = 1.10f;
		xSpec.m_fSwayAmplitudeDeg = 6.0f;
		xSpec.m_fInteriorTint = 0.72f;
		break;

	case BUSH_MOUND:
		// Knee-high and nearly hemispherical: many short branches at every
		// elevation, small cards, the density filler of the three.
		xSpec.m_szName = "Bush_Mound";
		xSpec.m_uSeed = 29863u;
		xSpec.m_uBranches = 11u;
		xSpec.m_fBranchLenMin = 0.55f;
		xSpec.m_fBranchLenMax = 0.80f;
		xSpec.m_fElevMinDeg = 10.0f;
		xSpec.m_fElevMaxDeg = 65.0f;
		xSpec.m_uCardsPerBranch = 6u;
		xSpec.m_fCardSizeMin = 0.45f;
		xSpec.m_fCardSizeMax = 0.70f;
		xSpec.m_fCardJitter = 0.22f;
		xSpec.m_fHeightMetres = 0.75f;
		xSpec.m_fSwayAmplitudeDeg = 4.5f;
		xSpec.m_fInteriorTint = 0.74f;
		break;

	default:
		// Tall and sparse: few long near-vertical branches, so it reads as a
		// different silhouette against the sky, and it sways the hardest —
		// long thin growth moves more than a dome.
		xSpec.m_szName = "Bush_Spindly";
		xSpec.m_uSeed = 47057u;
		xSpec.m_uBranches = 5u;
		xSpec.m_fBranchLenMin = 1.10f;
		xSpec.m_fBranchLenMax = 1.55f;
		xSpec.m_fElevMinDeg = 55.0f;
		xSpec.m_fElevMaxDeg = 80.0f;
		xSpec.m_uCardsPerBranch = 5u;
		xSpec.m_fCardSizeMin = 0.42f;
		xSpec.m_fCardSizeMax = 0.62f;
		xSpec.m_fCardJitter = 0.16f;
		xSpec.m_fHeightMetres = 1.60f;
		xSpec.m_fSwayAmplitudeDeg = 8.5f;
		xSpec.m_fInteriorTint = 0.76f;
		break;
	}
	return xSpec;
}

//=============================================================================
// Branch graph + card cloud — generated together, then uniformly rescaled so
// the highest card corner sits at exactly m_fHeightMetres. The skeleton is
// built from the graph AFTER the rescale, so bones and geometry stay
// consistent by construction.
//=============================================================================
struct BushBranch
{
	Zenith_Maths::Vector3 m_xBase;
	Zenith_Maths::Vector3 m_xTip;
	float                 m_fSwayPhase = 0.0f;
	float                 m_fSwayScale = 1.0f;   // per-branch amplitude variation
	u_int                 m_uBone = 0u;          // assigned by CreateBushSkeleton
	std::string           m_strBoneName;
};

struct BushCard
{
	Zenith_Maths::Vector3 m_xCentre;
	float                 m_fSize = 0.5f;
	float                 m_fYaw = 0.0f;
	float                 m_fPitch = 0.0f;
	u_int                 m_uBranch = 0u;        // index into the branch array
	float                 m_fTint = 1.0f;        // filled by the radial pass below
};

void BuildBushVariant(Zenith_Vector<BushBranch>& xBranchesOut, Zenith_Vector<BushCard>& xCardsOut,
	const BushVariantSpec& xSpec)
{
	BushRng xRng(xSpec.m_uSeed);

	// --- Branches: a whorl from a common root crown, golden-angle azimuths. ---
	for (u_int uBranch = 0; uBranch < xSpec.m_uBranches; uBranch++)
	{
		// Every draw hoisted into its own named const, in the order it happens —
		// same discipline as the scatter, and cheap insurance even here.
		const float fAzimuthJitter = xRng.NextRange(-0.35f, 0.35f);
		const float fElevDeg = xRng.NextRange(xSpec.m_fElevMinDeg, xSpec.m_fElevMaxDeg);
		const float fLength = xRng.NextRange(xSpec.m_fBranchLenMin, xSpec.m_fBranchLenMax);
		const float fBaseRadius = xRng.NextRange(0.04f, 0.10f);
		const float fBaseHeight = xRng.NextRange(0.04f, 0.09f);
		const float fSwayScale = xRng.NextRange(0.75f, 1.25f);
		const float fSwayPhase = xRng.NextRange(0.0f, 6.2831f);

		const float fAzimuth = static_cast<float>(uBranch) * 2.399963f + fAzimuthJitter; // golden angle
		const float fElev = fElevDeg * 0.01745329252f;
		const Zenith_Maths::Vector3 xDir(
			cosf(fAzimuth) * cosf(fElev), sinf(fElev), sinf(fAzimuth) * cosf(fElev));

		BushBranch xBranch;
		xBranch.m_xBase = Zenith_Maths::Vector3(
			cosf(fAzimuth) * fBaseRadius, fBaseHeight, sinf(fAzimuth) * fBaseRadius);
		xBranch.m_xTip = xBranch.m_xBase + xDir * fLength;
		xBranch.m_fSwayScale = fSwayScale;
		xBranch.m_fSwayPhase = fSwayPhase;
		char acName[32];
		snprintf(acName, sizeof(acName), "BushBone_%u", uBranch);
		xBranch.m_strBoneName = acName;
		xBranchesOut.PushBack(xBranch);
	}

	// --- Cards along the outer part of each branch, puffing outward. ---------
	for (u_int uBranch = 0; uBranch < xBranchesOut.GetSize(); uBranch++)
	{
		const BushBranch& xBranch = xBranchesOut.Get(uBranch);
		const Zenith_Maths::Vector3 xAxis = xBranch.m_xTip - xBranch.m_xBase;

		for (u_int uCard = 0; uCard < xSpec.m_uCardsPerBranch; uCard++)
		{
			const float fAlong = xRng.NextRange(0.30f, 1.05f);
			const float fJitterX = xRng.NextRange(-xSpec.m_fCardJitter, xSpec.m_fCardJitter);
			const float fJitterY = xRng.NextRange(-xSpec.m_fCardJitter * 0.7f, xSpec.m_fCardJitter * 0.9f);
			const float fJitterZ = xRng.NextRange(-xSpec.m_fCardJitter, xSpec.m_fCardJitter);
			const float fSize = xRng.NextRange(xSpec.m_fCardSizeMin, xSpec.m_fCardSizeMax);
			const float fYaw = xRng.NextRange(0.0f, 6.2831f);
			const float fPitch = xRng.NextRange(-0.50f, 0.50f);

			BushCard xCard;
			xCard.m_xCentre = xBranch.m_xBase + xAxis * fAlong
				+ Zenith_Maths::Vector3(fJitterX, fJitterY, fJitterZ);
			xCard.m_fSize = fSize;
			xCard.m_fYaw = fYaw;
			xCard.m_fPitch = fPitch;
			xCard.m_uBranch = uBranch;
			xCardsOut.PushBack(xCard);
		}
	}

	// --- Normalise: highest card corner to EXACTLY m_fHeightMetres. ----------
	// A card's vertical half-extent is half * cos(pitch) (its right vector is
	// horizontal), so the top corner is centreY + half*cosPitch.
	float fMaxCornerY = 0.0f;
	for (u_int uCard = 0; uCard < xCardsOut.GetSize(); uCard++)
	{
		const BushCard& xCard = xCardsOut.Get(uCard);
		fMaxCornerY = std::max(fMaxCornerY,
			xCard.m_xCentre.y + xCard.m_fSize * 0.5f * cosf(xCard.m_fPitch));
	}
	const float fRescale = xSpec.m_fHeightMetres / std::max(0.05f, fMaxCornerY);
	for (u_int uBranch = 0; uBranch < xBranchesOut.GetSize(); uBranch++)
	{
		xBranchesOut.Get(uBranch).m_xBase = xBranchesOut.Get(uBranch).m_xBase * fRescale;
		xBranchesOut.Get(uBranch).m_xTip = xBranchesOut.Get(uBranch).m_xTip * fRescale;
	}
	for (u_int uCard = 0; uCard < xCardsOut.GetSize(); uCard++)
	{
		BushCard& xCard = xCardsOut.Get(uCard);
		xCard.m_xCentre = xCard.m_xCentre * fRescale;
		xCard.m_fSize *= fRescale;
	}

	// --- Ground clamp AFTER the rescale: no card corner below y = -0.10. -----
	// The scatter places an instance AT the sampled terrain height and sinks it
	// a few centimetres; foliage kissing the ground is right, foliage buried to
	// its midline is not. Clamping only RAISES low cards, so the normalised top
	// corner is untouched.
	for (u_int uCard = 0; uCard < xCardsOut.GetSize(); uCard++)
	{
		BushCard& xCard = xCardsOut.Get(uCard);
		const float fHalfUp = xCard.m_fSize * 0.5f * cosf(xCard.m_fPitch);
		xCard.m_xCentre.y = std::max(xCard.m_xCentre.y, fHalfUp - 0.10f);
	}

	// --- Radial tint: darken toward the core from an UNMODULATED 1.0 rim. ----
	// The tint MULTIPLIES albedo, so the baseline must stay 1.0 (the rock set
	// once shipped a tint that dimmed the whole piece — an albedo cut with
	// extra steps). Interior cards read as self-shadowed depth for free.
	float fMaxRadial = 0.001f;
	for (u_int uCard = 0; uCard < xCardsOut.GetSize(); uCard++)
	{
		const BushCard& xCard = xCardsOut.Get(uCard);
		fMaxRadial = std::max(fMaxRadial,
			sqrtf(xCard.m_xCentre.x * xCard.m_xCentre.x + xCard.m_xCentre.z * xCard.m_xCentre.z));
	}
	for (u_int uCard = 0; uCard < xCardsOut.GetSize(); uCard++)
	{
		BushCard& xCard = xCardsOut.Get(uCard);
		const float fRadialT = std::clamp(sqrtf(
			xCard.m_xCentre.x * xCard.m_xCentre.x + xCard.m_xCentre.z * xCard.m_xCentre.z)
			/ fMaxRadial, 0.0f, 1.0f);
		xCard.m_fTint = xSpec.m_fInteriorTint + (1.0f - xSpec.m_fInteriorTint) * fRadialT;
	}
}

//=============================================================================
// Skeleton — a root anchor plus one bone per branch at the branch base.
// AddBone takes the PARENT-LOCAL position; every branch parents to the root,
// which sits at the origin, so parent-local == the branch base itself.
//=============================================================================
Zenith_SkeletonAsset* CreateBushSkeleton(Zenith_Vector<BushBranch>& xBranches)
{
	Zenith_SkeletonAsset* pxSkel = new Zenith_SkeletonAsset();
	const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
	const Zenith_Maths::Vector3 xUnitScale(1.0f, 1.0f, 1.0f);

	pxSkel->AddBone("BushRoot", -1, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), xIdentity, xUnitScale);
	for (u_int uBranch = 0; uBranch < xBranches.GetSize(); uBranch++)
	{
		BushBranch& xBranch = xBranches.Get(uBranch);
		pxSkel->AddBone(xBranch.m_strBoneName.c_str(), 0, xBranch.m_xBase, xIdentity, xUnitScale);
		xBranch.m_uBone = pxSkel->GetNumBones() - 1;
	}
	pxSkel->ComputeBindPoseMatrices();
	return pxSkel;
}

//=============================================================================
// Foliage mesh — crossed leaf cards, every one skinned WHOLLY to its branch
// bone (weight exactly 1; an unskinned vertex is frozen while its neighbours
// sway, which is very visible and very easy to ship). Both windings are
// emitted per card — the instanced pipeline backface-culls and foliage must
// read from both sides. Normals are up-biased for soft foliage lighting;
// because the card's right vector is horizontal and the bias is vertical, the
// tangent stays exactly perpendicular to the shading normal.
//=============================================================================
Zenith_MeshAsset* CreateBushFoliageMesh(const Zenith_Vector<BushBranch>& xBranches,
	const Zenith_Vector<BushCard>& xCards)
{
	Zenith_MeshAsset* pxMesh = new Zenith_MeshAsset();
	pxMesh->Reserve(1024, 4096);

	for (u_int uCard = 0; uCard < xCards.GetSize(); uCard++)
	{
		const BushCard& xCard = xCards.Get(uCard);
		const u_int uBone = xBranches.Get(xCard.m_uBranch).m_uBone;

		const float fCY = cosf(xCard.m_fYaw), fSY = sinf(xCard.m_fYaw);
		const float fCP = cosf(xCard.m_fPitch), fSP = sinf(xCard.m_fPitch);
		const Zenith_Maths::Vector3 xRight(fCY, 0.0f, fSY);
		const Zenith_Maths::Vector3 xUp = glm::normalize(Zenith_Maths::Vector3(-fSY * fSP, fCP, fCY * fSP));
		Zenith_Maths::Vector3 xNormal = glm::cross(xRight, xUp);
		xNormal = glm::normalize(xNormal * 0.5f + Zenith_Maths::Vector3(0.0f, 0.85f, 0.0f));

		const float fHalf = xCard.m_fSize * 0.5f;
		const Zenith_Maths::Vector3 axCorners[4] = {
			xCard.m_xCentre - xRight * fHalf - xUp * fHalf,
			xCard.m_xCentre + xRight * fHalf - xUp * fHalf,
			xCard.m_xCentre + xRight * fHalf + xUp * fHalf,
			xCard.m_xCentre - xRight * fHalf + xUp * fHalf,
		};
		const Zenith_Maths::Vector2 axUVs[4] = {
			{ 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f } };

		const uint32_t uFirst = pxMesh->GetNumVerts();
		for (u_int u = 0; u < 4; u++)
		{
			pxMesh->AddVertex(axCorners[u], xNormal, axUVs[u], xRight,
				Zenith_Maths::Vector4(xCard.m_fTint, xCard.m_fTint, xCard.m_fTint, 1.0f));
			pxMesh->SetVertexSkinning(pxMesh->GetNumVerts() - 1,
				glm::uvec4(uBone, 0, 0, 0), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
		}
		// Both windings, in a FIXED pair order the unit tests pin: triangle k
		// and triangle k+2 of a card are the same corners wound oppositely.
		pxMesh->AddTriangle(uFirst + 0, uFirst + 1, uFirst + 2);
		pxMesh->AddTriangle(uFirst + 0, uFirst + 2, uFirst + 3);
		pxMesh->AddTriangle(uFirst + 0, uFirst + 2, uFirst + 1);
		pxMesh->AddTriangle(uFirst + 0, uFirst + 3, uFirst + 2);
	}

	pxMesh->AddSubmesh(0, pxMesh->GetNumIndices(), 0);
	pxMesh->ComputeBounds();
	return pxMesh;
}

//=============================================================================
// Sway clip — the tree's layered-sine recipe: a primary cycle plus a faster
// ripple, per-branch phase and amplitude, about wind-perpendicular axes (the
// same axes the tree uses, so one wind direction reads across the whole map).
// 4-second loop; the first and last keys land on the same value because both
// sines complete whole cycles. Keyframe times are in TICKS (30/s), not
// seconds — 0..120, not 0..4.
//=============================================================================
Flux_AnimationClip* CreateBushSwayClip(const Zenith_Vector<BushBranch>& xBranches,
	const BushVariantSpec& xSpec)
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Sway");
	pxClip->SetDuration(4.0f);
	pxClip->SetTicksPerSecond(30);
	pxClip->SetLooping(true);

	constexpr u_int uKEYS = 17;
	constexpr float fTOTAL_TICKS = 120.0f;

	for (u_int uBranch = 0; uBranch < xBranches.GetSize(); uBranch++)
	{
		const BushBranch& xBranch = xBranches.Get(uBranch);
		const float fAmplitude = glm::radians(xSpec.m_fSwayAmplitudeDeg * xBranch.m_fSwayScale);
		const float fPhase = xBranch.m_fSwayPhase;

		Flux_BoneChannel xChannel;
		for (u_int uKey = 0; uKey < uKEYS; uKey++)
		{
			const float fT = static_cast<float>(uKey) / (uKEYS - 1);
			const float fW = fT * 6.2831853f;
			const float fMain = sinf(fW + fPhase);
			const float fRipple = 0.35f * sinf(2.0f * fW + fPhase * 1.7f + 1.3f);
			const float fAngle = fAmplitude * (fMain + fRipple) / 1.35f;
			const Zenith_Maths::Quat xRot =
				glm::angleAxis(fAngle, Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f)) *
				glm::angleAxis(fAngle * 0.4f, Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
			xChannel.AddRotationKeyframe(fT * fTOTAL_TICKS, xRot);
		}
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel(xBranch.m_strBoneName, std::move(xChannel));
	}
	return pxClip;
}

//=============================================================================
// Foliage texture — layered rounded leaves, painter's order, alpha as a REAL
// leaf-shape mask (the MASKED material is pointless against an all-255 alpha,
// and the unit tests pin that it never regresses to one). The pixel function
// is split from the file writer so the tests can read the same pixels the
// asset gets.
//=============================================================================
constexpr int32_t iBUSH_FOLIAGE_SIZE = 1024;

//=============================================================================
// The foliage card, painted ONCE into every map it feeds.
//
// The albedo half is byte-for-byte what it always was (pinned by
// BushAssets.FoliageAlbedoAlphaIsARealMask); the two new outputs are what stop
// the card shading as a flat decal:
//
//   HEIGHT — each leaf a gentle DOME with a midrib CREASE, raised by its
//   painter's-order layer so an overlapping leaf steps above the one behind.
//   The normal map derived from it is what makes ONE card read as many
//   separately-curved leaves under a moving light.
//
//   AO — occlusion toward each leaf's stem end and under overlaps, counted
//   from how many leaves have already painted a texel.
//
// One pass, not two: a second pass with its own RNG draws would place
// DIFFERENT leaves, and the normal map would describe foliage the albedo does
// not have. GenerateBushFoliagePixels below is the albedo-only wrapper the
// existing units call.
//=============================================================================
void GenerateBushFoliageMaps(Zenith_Vector<u_int8>& xPixels, Zenith_Vector<float>& xHeight,
	Zenith_Vector<float>& xAO)
{
	xPixels.Clear();
	xPixels.Resize(iBUSH_FOLIAGE_SIZE * iBUSH_FOLIAGE_SIZE * 4, 0);
	xHeight.Clear();
	xHeight.Resize(iBUSH_FOLIAGE_SIZE * iBUSH_FOLIAGE_SIZE, 0.0f);
	// Unpainted texels are fully unoccluded: they are discarded by the alpha
	// test, and a 0 there would drag the AO mip chain toward black.
	xAO.Clear();
	xAO.Resize(iBUSH_FOLIAGE_SIZE * iBUSH_FOLIAGE_SIZE, 1.0f);

	// How many leaves have painted each texel — one is the top of the pile,
	// more is a texel buried in foliage, which is where a shrub goes dark.
	Zenith_Vector<u_int8> xCoverage;
	xCoverage.Resize(iBUSH_FOLIAGE_SIZE * iBUSH_FOLIAGE_SIZE, 0u);

	BushRng xRng(52201u);

	// Back layers darker (depth cue), front brighter. Rounder, blunter leaves
	// than the tree's — a shrub leaf, not a serrated canopy leaf — and tuned a
	// shade brighter: this scene's ground set is a near-white limestone, and
	// both the stone and bark sets needed a lift before they stopped reading
	// as black smears against it.
	constexpr u_int uLEAVES = 34;
	for (u_int uLeaf = 0; uLeaf < uLEAVES; uLeaf++)
	{
		const float fLayerT = static_cast<float>(uLeaf) / (uLEAVES - 1);
		const float fCX = iBUSH_FOLIAGE_SIZE * (0.5f + (xRng.NextFloat01() - 0.5f) * 0.70f);
		const float fCY = iBUSH_FOLIAGE_SIZE * (0.5f + (xRng.NextFloat01() - 0.5f) * 0.70f);
		const float fAngle = xRng.NextRange(0.0f, 6.2831f);
		const float fLen = xRng.NextRange(0.15f, 0.24f) * iBUSH_FOLIAGE_SIZE;
		const float fWidth = fLen * xRng.NextRange(0.55f, 0.75f);

		const float fBright = 0.58f + 0.47f * fLayerT;
		const float fHueJit = xRng.NextRange(-0.04f, 0.07f);
		const float fLR = (0.15f + fHueJit * 0.5f) * fBright;
		const float fLG = (0.37f + fHueJit) * fBright;
		const float fLB = (0.085f + fHueJit * 0.2f) * fBright;

		const float fCA = cosf(fAngle), fSA = sinf(fAngle);
		// Where this leaf's surface sits in the card's depth range. Front leaves
		// build their dome on a higher base, so the step down to the leaf behind
		// reads as a real edge in the normal map rather than a colour change.
		const float fLeafBase = 0.16f + 0.60f * fLayerT;
		const int32_t iRadius = static_cast<int32_t>(fLen * 0.62f) + 2;
		const int32_t iCXi = static_cast<int32_t>(fCX);
		const int32_t iCYi = static_cast<int32_t>(fCY);
		for (int32_t iY = std::max(0, iCYi - iRadius); iY < std::min(iBUSH_FOLIAGE_SIZE, iCYi + iRadius); iY++)
		{
			for (int32_t iX = std::max(0, iCXi - iRadius); iX < std::min(iBUSH_FOLIAGE_SIZE, iCXi + iRadius); iX++)
			{
				const float fDXp = static_cast<float>(iX) - fCX;
				const float fDYp = static_cast<float>(iY) - fCY;
				const float fAlong = (fDXp * fCA + fDYp * fSA) / fLen + 0.5f;
				const float fAcross = (-fDXp * fSA + fDYp * fCA) / fWidth;
				if (fAlong < 0.0f || fAlong > 1.0f)
				{
					continue;
				}
				// Blunt ovate outline with a gentle lobe wobble, no serration.
				const float fProfile = powf(sinf(fAlong * 3.14159f), 0.5f) * 0.5f;
				const float fEdge = fProfile * (1.0f + 0.05f * sinf(fAlong * 7.0f + uLeaf * 2.1f));
				const float fDist = fabsf(fAcross);
				if (fDist > fEdge)
				{
					continue;
				}

				float fShade = 0.80f + 0.20f * (1.0f - fDist / std::max(fEdge, 0.001f));
				fShade *= 0.88f + 0.12f * fAlong;
				if (fDist < 0.015f) { fShade *= 0.70f; }                       // midrib
				const float fVein = fabsf(sinf(fAlong * 14.0f + fDist * 7.0f));
				if (fVein > 0.94f) { fShade *= 0.88f; }                        // side veins

				const int32_t iIdx = iY * iBUSH_FOLIAGE_SIZE + iX;
				u_int8* pucP = &xPixels.Get(iIdx * 4);
				pucP[0] = static_cast<u_int8>(std::clamp(fLR * fShade, 0.0f, 1.0f) * 255.0f);
				pucP[1] = static_cast<u_int8>(std::clamp(fLG * fShade, 0.0f, 1.0f) * 255.0f);
				pucP[2] = static_cast<u_int8>(std::clamp(fLB * fShade, 0.0f, 1.0f) * 255.0f);
				const float fAlpha = SmoothStepB(1.0f, 0.90f, fDist / std::max(fEdge, 0.001f));
				pucP[3] = std::max(pucP[3], static_cast<u_int8>(fAlpha * 255.0f));

				// --- Height: dome across the leaf, creased along the midrib ---
				const float fRel = fDist / std::max(fEdge, 0.001f);            // 0 spine .. 1 rim
				const float fDome = sqrtf(std::max(0.0f, 1.0f - fRel * fRel)); // circular cross-section
				const float fTaper = powf(sinf(fAlong * 3.14159f), 0.35f);     // flatter at stem and tip
				// Without the crease a leaf reads as a pillow: a real one folds
				// along its midrib, and the fold is what catches the sun.
				const float fCrease = SmoothStepB(0.10f, 0.0f, fRel) * 0.28f;
				xHeight.Get(iIdx) = std::clamp(
					fLeafBase + (fDome * fTaper * 0.34f) - fCrease * fTaper, 0.0f, 1.0f);

				// --- AO: stem-end crowding + how deep in the pile this texel is ---
				const float fStemDark = (1.0f - SmoothStepB(0.0f, 0.32f, fAlong)) * 0.42f;
				const u_int8 ucUnder = xCoverage.Get(iIdx);
				const float fPileDark = std::min(static_cast<float>(ucUnder), 3.0f) * 0.09f;
				const float fRimLift = SmoothStepB(0.70f, 1.0f, fRel) * 0.08f;
				xAO.Get(iIdx) = std::clamp(1.0f - fStemDark - fPileDark + fRimLift, 0.05f, 1.0f);
				if (ucUnder < 255u)
				{
					xCoverage.Get(iIdx) = static_cast<u_int8>(ucUnder + 1u);
				}
			}
		}
	}
}

// Albedo only — the shape the existing units drive. One body, so the pixels a
// test reads are the pixels the .ztxtr gets.
void GenerateBushFoliagePixels(Zenith_Vector<u_int8>& xPixels)
{
	Zenith_Vector<float> xHeight;
	Zenith_Vector<float> xAO;
	GenerateBushFoliageMaps(xPixels, xHeight, xAO);
}

//=============================================================================
// Tangent-space normal map from the foliage height field, RGBA8 for the BC5
// writer (which keeps R+G; the shader rebuilds Z).
//
// CLAMPED, not wrapped: the foliage card does NOT tile — its border is
// transparent — so a wrapped central difference would invent a slope joining
// two unrelated leaves across the seam.
//
// Shared with the units deliberately: a test that re-derived the encoding
// would be checking its own arithmetic, not the shipped texels.
//=============================================================================
void EncodeBushNormalMap(const Zenith_Vector<float>& xHeight, Zenith_Vector<u_int8>& xOut)
{
	constexpr float fSLOPE_GAIN = 3.0f;
	constexpr int32_t iSIZE = iBUSH_FOLIAGE_SIZE;
	xOut.Clear();
	xOut.Resize(iSIZE * iSIZE * 4, 0);
	for (int32_t iY = 0; iY < iSIZE; iY++)
	{
		for (int32_t iX = 0; iX < iSIZE; iX++)
		{
			const int32_t iXP = std::min(iX + 1, iSIZE - 1);
			const int32_t iXM = std::max(iX - 1, 0);
			const int32_t iYP = std::min(iY + 1, iSIZE - 1);
			const int32_t iYM = std::max(iY - 1, 0);
			const float fDX = (xHeight.Get(iY * iSIZE + iXP) - xHeight.Get(iY * iSIZE + iXM)) * fSLOPE_GAIN;
			const float fDY = (xHeight.Get(iYP * iSIZE + iX) - xHeight.Get(iYM * iSIZE + iX)) * fSLOPE_GAIN;
			const Zenith_Maths::Vector3 xN =
				glm::normalize(Zenith_Maths::Vector3(-fDX, -fDY, 1.0f));
			u_int8* pucN = &xOut.Get((iY * iSIZE + iX) * 4);
			pucN[0] = static_cast<u_int8>((xN.x * 0.5f + 0.5f) * 255.0f);
			pucN[1] = static_cast<u_int8>((xN.y * 0.5f + 0.5f) * 255.0f);
			pucN[2] = static_cast<u_int8>((xN.z * 0.5f + 0.5f) * 255.0f);
			pucN[3] = 255;
		}
	}
}

void GenerateBushFoliageTexture(const std::string& strDir)
{
	Zenith_Vector<u_int8> xPixels;
	Zenith_Vector<float> xHeight;
	Zenith_Vector<float> xAOField;
	GenerateBushFoliageMaps(xPixels, xHeight, xAOField);

	// --- Normal (BC5 linear) from the per-leaf domes -------------------------
	Zenith_Vector<u_int8> xNormal;
	EncodeBushNormalMap(xHeight, xNormal);
	Zenith_Tools_TextureExport::ExportFromDataCompressed(
		xNormal.GetDataPointer(), strDir + "Bush_Foliage_Normal" ZENITH_TEXTURE_EXT,
		iBUSH_FOLIAGE_SIZE, iBUSH_FOLIAGE_SIZE,
		TextureCompressionMode::BC5, TextureColourSpace::Linear);

	// --- AO (linear grey, uncompressed so the values survive exactly) --------
	Zenith_Vector<u_int8> xAO;
	xAO.Resize(iBUSH_FOLIAGE_SIZE * iBUSH_FOLIAGE_SIZE * 4, 0);
	for (int32_t i = 0; i < iBUSH_FOLIAGE_SIZE * iBUSH_FOLIAGE_SIZE; i++)
	{
		const u_int8 ucAO = static_cast<u_int8>(std::clamp(xAOField.Get(i), 0.0f, 1.0f) * 255.0f);
		u_int8* pucAO = &xAO.Get(i * 4);
		pucAO[0] = ucAO;
		pucAO[1] = ucAO;
		pucAO[2] = ucAO;
		pucAO[3] = 255;
	}
	Zenith_Tools_TextureExport::ExportFromDataV2Uncompressed(
		xAO.GetDataPointer(), strDir + "Bush_Foliage_AO" ZENITH_TEXTURE_EXT,
		iBUSH_FOLIAGE_SIZE, iBUSH_FOLIAGE_SIZE, TEXTURE_FORMAT_RGBA8_UNORM);

	// Full mip chain with COVERAGE-PRESERVING alpha at the foliage material's
	// cutoff (SetAlphaCutoff(0.45f) below): a plain box filter averages the soft
	// leaf edge below the cutoff every level and a distant bush evaporates.
	// Uncompressed keeps the 8-bit mask edge exact.
	Zenith_Tools_TextureExport::ExportFromDataV2Uncompressed(
		xPixels.GetDataPointer(), strDir + "Bush_Foliage_Albedo" ZENITH_TEXTURE_EXT, iBUSH_FOLIAGE_SIZE, iBUSH_FOLIAGE_SIZE, TEXTURE_FORMAT_RGBA8_SRGB,
		/*fAlphaCoverageCutoff*/ 0.45f);
}

void GenerateBushFoliageMaterial(const std::string& strDir)
{
	auto xhFoliage = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Zenith_MaterialAsset* pxFoliage = xhFoliage.GetDirect();
	pxFoliage->SetName("BushFoliage");
	pxFoliage->SetDiffuseTexture(TextureHandle("engine:Meshes/Bushes/Bush_Foliage_Albedo" ZENITH_TEXTURE_EXT));
	pxFoliage->SetNormalTexture(TextureHandle("engine:Meshes/Bushes/Bush_Foliage_Normal" ZENITH_TEXTURE_EXT));
	pxFoliage->SetOcclusionTexture(TextureHandle("engine:Meshes/Bushes/Bush_Foliage_AO" ZENITH_TEXTURE_EXT));
	pxFoliage->SetRoughness(0.80f);
	pxFoliage->SetMetallic(0.0f);
	pxFoliage->SetNormalStrength(1.0f);
	pxFoliage->SetOcclusionStrength(1.0f);
	// ★ TWO-SIDED at the MATERIAL, which is a different thing from the doubled
	// windings the mesh emits. The geometry duplication makes a card VISIBLE
	// from behind; TWO_SIDED_NORMAL_FLIP is what makes the back face SHADE as a
	// leaf instead of as a surface lit from the wrong side. (The duplicated
	// windings stay — BushAssets.EveryCardIsEmittedDoubleSided pins them —
	// but they are now redundant geometry; see Flux/Vegetation/CLAUDE.md.)
	pxFoliage->SetTwoSided(true);
	// ★ Foliage TRANSMITS: the deferred pass's subsurface branch wraps the
	// diffuse past the terminator and back-lights the graze, which is what a
	// sunlit shrub actually does. Its constants are tuned for skin — the
	// documented approximation until a FOLIAGE shading model exists.
	pxFoliage->SetShadingModel(MATERIAL_SHADING_SUBSURFACE);
	// ★ MASKED with a non-zero cutoff, or the alpha mask above is dead weight:
	// BuildMaterialDrawConstants writes cutoff 0 for OPAQUE, the shader never
	// discards, and every card renders as an opaque black square with a leaf
	// painted on it. Same trap, same fix, as the tree's leaf material.
	pxFoliage->SetBlendMode(MATERIAL_BLEND_MASKED);
	pxFoliage->SetAlphaCutoff(0.45f);
	pxFoliage->SaveToFile(strDir + "Bush_Foliage" ZENITH_MATERIAL_EXT);
}

//=============================================================================
// Export one variant: skeleton + skinned mesh (.zasset / .zgeom) + sway VAT.
// No .zmodel on purpose — the instanced component is this set's consumer, and
// a ModelComponent would render the bush frozen at bind pose; a game that
// wants that can still LoadMesh the .zasset directly.
//=============================================================================
void ExportBushVariant(const std::string& strDir, const BushVariantSpec& xSpec)
{
	Zenith_Vector<BushBranch> xBranches;
	Zenith_Vector<BushCard> xCards;
	BuildBushVariant(xBranches, xCards, xSpec);

	Zenith_SkeletonAsset* pxSkel = CreateBushSkeleton(xBranches);
	pxSkel->Export((strDir + xSpec.m_szName + ZENITH_SKELETON_EXT).c_str());

	Zenith_MeshAsset* pxMesh = CreateBushFoliageMesh(xBranches, xCards);
	pxMesh->SetSkeletonPath(std::string("Meshes/Bushes/") + xSpec.m_szName + ZENITH_SKELETON_EXT);
	pxMesh->Export((strDir + xSpec.m_szName + ZENITH_MESH_ASSET_EXT).c_str());

	// The SKINNED converter, not CreateStaticFluxMeshGeometry — the static one
	// drops the bone lanes and the VAT bake would have nothing to deform.
	Flux_MeshGeometry* pxGeometry = Zenith_Tools_CreateFluxMeshGeometry(pxMesh, pxSkel);
	pxGeometry->Export((strDir + xSpec.m_szName + ZENITH_GEOMETRY_EXT).c_str());

	Flux_AnimationClip* pxSwayClip = CreateBushSwayClip(xBranches, xSpec);
	Flux_AnimationTexture* pxVAT = new Flux_AnimationTexture();
	Zenith_Vector<Flux_AnimationClip*> axAnimations;
	axAnimations.PushBack(pxSwayClip);
	if (pxVAT->BakeFromAnimations(pxGeometry, pxSkel, axAnimations, 30))
	{
		pxVAT->Export(strDir + xSpec.m_szName + std::string("_Sway.zanmt"));
		Zenith_Log(LOG_CATEGORY_ASSET, "  %s: %u cards, %u verts; VAT %u x %u (verts x frames)",
			xSpec.m_szName, xCards.GetSize(), pxMesh->GetNumVerts(),
			pxVAT->GetTextureWidth(), pxVAT->GetTextureHeight());
	}
	else
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "  %s VAT bake FAILED", xSpec.m_szName);
	}

	delete pxVAT;
	delete pxSwayClip;
	delete pxGeometry;
	delete pxMesh;
	delete pxSkel;
}

} // namespace

//=============================================================================
// Entry point — called from GenerateTestAssets() at every editor boot.
//=============================================================================
void GenerateBushAssets()
{
	Zenith_Log(LOG_CATEGORY_ASSET,
		"Generating shared Bush assets v%u (3 wind-animated foliage bushes, masked two-sided subsurface leaf material)...",
		uBUSH_ASSET_EXPORT_VERSION);

	const std::string strOutputDir = std::string(ENGINE_ASSETS_DIR) + "Meshes/Bushes/";
	std::filesystem::create_directories(strOutputDir);

	// Texture + material first: the meshes' consumers reference them by path.
	GenerateBushFoliageTexture(strOutputDir);
	GenerateBushFoliageMaterial(strOutputDir);

	for (int iVariant = 0; iVariant < BUSH_VARIANT_COUNT; iVariant++)
	{
		ExportBushVariant(strOutputDir, BushVariantAt(iVariant));
	}

	Zenith_Log(LOG_CATEGORY_ASSET, "Bush assets generated at: %s", strOutputDir.c_str());
}

#include "Zenith_Tools_BushAssetExport.Tests.inl"

#endif // ZENITH_TOOLS
