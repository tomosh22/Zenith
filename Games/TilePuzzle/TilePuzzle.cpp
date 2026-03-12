#include "Zenith.h"

#include "Core/Zenith_GraphicsOptions.h"
#include "TilePuzzle/Components/TilePuzzle_Types.h"
#include "TilePuzzle/Components/TilePuzzle_Behaviour.h"
#include "TilePuzzle/Components/Pinball_Behaviour.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "Flux/Flux_Graphics.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "Prefab/Zenith_Prefab.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UIRect.h"
#include "SaveData/Zenith_SaveData.h"
#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"

#include <unordered_set>

#ifdef ZENITH_INPUT_SIMULATOR
#include "TilePuzzle/Tests/TilePuzzle_AutoTest.h"
#include "Input/Zenith_InputSimulator.h"
#endif

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_EditorAutomation.h"
#include "Editor/Zenith_Editor.h"
#endif

#ifdef ZENITH_WINDOWS
#include <cstdlib> // __argc, __argv
#include <cstring> // strcmp
#endif

// ============================================================================
// TilePuzzle Resources - Global access for behaviours
// ============================================================================
namespace TilePuzzle
{
	// Shared geometry assets (registry-managed)
	Zenith_MeshGeometryAsset* g_pxCubeAsset = nullptr;
	Zenith_MeshGeometryAsset* g_pxSphereAsset = nullptr;

	// Convenience pointers to underlying geometry (do not delete - managed by assets)
	Flux_MeshGeometry* g_pxCubeGeometry = nullptr;
	Flux_MeshGeometry* g_pxSphereGeometry = nullptr;

	// Programmatically generated cat head mesh
	Flux_MeshGeometry* g_pxCatMeshGeometry = nullptr;

	// Floor material
	MaterialHandle g_xFloorMaterial;

	// Blocker material (static shapes)
	MaterialHandle g_xBlockerMaterial;

	// Colored shape materials (draggable)
	MaterialHandle g_axShapeMaterials[TILEPUZZLE_COLOR_COUNT];

	// Colored cat materials
	MaterialHandle g_axCatMaterials[TILEPUZZLE_COLOR_COUNT];

	// Prefabs for runtime instantiation
	Zenith_Prefab* g_pxCellPrefab = nullptr;
	Zenith_Prefab* g_pxShapeCubePrefab = nullptr;
	Zenith_Prefab* g_pxCatPrefab = nullptr;

	// Pre-generated merged meshes for each shape type
	Flux_MeshGeometry* g_apxShapeMeshes[TILEPUZZLE_SHAPE_COUNT] = {};

	// Highlight emissive intensity (loaded from materials.bin)
	float g_fHighlightEmissiveIntensity = 0.5f;

	// UI Icon textures (loaded via AssetRegistry from .ztxtr files)
	Zenith_TextureAsset* g_pxIconStarFilled = nullptr;
	Zenith_TextureAsset* g_pxIconStarEmpty = nullptr;
	Zenith_TextureAsset* g_pxIconCoin = nullptr;
	Zenith_TextureAsset* g_pxIconHeart = nullptr;
	Zenith_TextureAsset* g_pxIconUndo = nullptr;
	Zenith_TextureAsset* g_pxIconSkip = nullptr;
	Zenith_TextureAsset* g_pxIconLock = nullptr;
	Zenith_TextureAsset* g_pxIconMenu = nullptr;
	Zenith_TextureAsset* g_pxIconBack = nullptr;
	Zenith_TextureAsset* g_pxIconSoundOn = nullptr;
	Zenith_TextureAsset* g_pxIconSoundOff = nullptr;
	Zenith_TextureAsset* g_pxIconReset = nullptr;
	Zenith_TextureAsset* g_pxIconGear = nullptr;
	Zenith_TextureAsset* g_pxIconCatSilhouette = nullptr;
	Zenith_TextureAsset* g_pxIconHint = nullptr;
	Zenith_TextureAsset* g_pxIconHintToken = nullptr;

	// Cat face textures (one per color)
	Zenith_TextureAsset* g_apxCatFaceTextures[TILEPUZZLE_COLOR_COUNT] = {};

	// Gameplay textures
	Zenith_TextureAsset* g_pxFloorTileTexture = nullptr;
	Zenith_TextureAsset* g_pxBlockerTexture = nullptr;

	// Pinball materials (loaded from .zmtrl files)
	Zenith_MaterialAsset* g_pxPinballBallMaterial = nullptr;
	Zenith_MaterialAsset* g_pxPinballPegMaterial = nullptr;
	Zenith_MaterialAsset* g_pxPinballPegHitMaterial = nullptr;

	// Pinball PBR textures
	Zenith_TextureAsset* g_pxPinballBumperDiffuseTex = nullptr;
	Zenith_TextureAsset* g_pxPinballBumperRMTex = nullptr;
	Zenith_TextureAsset* g_pxPinballWallDiffuseTex = nullptr;
	Zenith_TextureAsset* g_pxPinballWallRMTex = nullptr;
	Zenith_TextureAsset* g_pxPinballFloorDiffuseTex = nullptr;
	Zenith_TextureAsset* g_pxPinballFloorRMTex = nullptr;
	Zenith_TextureAsset* g_pxPinballPlungerRMTex = nullptr;
	Zenith_TextureAsset* g_pxPinballTargetDiffuseTex = nullptr;

	// Pinball custom meshes
	Flux_MeshGeometry* g_pxBumperGeometry = nullptr;
	Flux_MeshGeometry* g_pxBeveledCubeGeometry = nullptr;
	Flux_MeshGeometry* g_pxPlungerGeometry = nullptr;
	Flux_MeshGeometry* g_pxTargetRampGeometry = nullptr;

	// Particle configs
	Flux_ParticleEmitterConfig* g_pxEliminationParticleConfig = nullptr;
	Flux_ParticleEmitterConfig* g_pxVictoryConfettiConfig = nullptr;
}

#include "TilePuzzle/Components/TilePuzzle_AssetGen.h"

static bool s_bResourcesInitialized = false;

// ============================================================================
// Rounded Polyomino Mesh Generation
// ============================================================================
static constexpr float fPI = 3.14159265358979f;
static constexpr float fBORDER = 0.05f;
static constexpr float fHALF = 0.5f;
static constexpr float fHALF_HEIGHT = 0.5f;
static constexpr float fCORNER_RADIUS = 0.10f;
static constexpr uint32_t uCORNER_SEGMENTS = 4;
static constexpr float fEDGE_RADIUS = 0.04f;
static constexpr uint32_t uEDGE_SEGMENTS = 3;

struct PerimeterPoint
{
	float m_fX;
	float m_fZ;
	float m_fOutX;
	float m_fOutZ;
	bool m_bExterior; // segment from this point to next is exterior
};

struct MeshBuilder
{
	Zenith_Vector<Zenith_Maths::Vector3> m_axPositions;
	Zenith_Vector<Zenith_Maths::Vector2> m_axUVs;
	Zenith_Vector<Zenith_Maths::Vector3> m_axNormals;
	Zenith_Vector<Zenith_Maths::Vector3> m_axTangents;
	Zenith_Vector<Zenith_Maths::Vector3> m_axBitangents;
	Zenith_Vector<Zenith_Maths::Vector4> m_axColors;
	Zenith_Vector<uint32_t> m_auIndices;

	uint32_t AddVertex(
		const Zenith_Maths::Vector3& xPos,
		const Zenith_Maths::Vector2& xUV,
		const Zenith_Maths::Vector3& xNormal,
		const Zenith_Maths::Vector3& xTangent,
		const Zenith_Maths::Vector3& xBitangent)
	{
		uint32_t uIndex = m_axPositions.GetSize();
		m_axPositions.PushBack(xPos);
		m_axUVs.PushBack(xUV);
		m_axNormals.PushBack(xNormal);
		m_axTangents.PushBack(xTangent);
		m_axBitangents.PushBack(xBitangent);
		m_axColors.PushBack({ 1.f, 1.f, 1.f, 1.f });
		return uIndex;
	}

	void AddTriangle(uint32_t uA, uint32_t uB, uint32_t uC)
	{
		m_auIndices.PushBack(uA);
		m_auIndices.PushBack(uB);
		m_auIndices.PushBack(uC);
	}

	void CopyToGeometry(Flux_MeshGeometry& xGeometryOut)
	{
		uint32_t uNumVerts = m_axPositions.GetSize();
		uint32_t uNumIndices = m_auIndices.GetSize();

		xGeometryOut.m_uNumVerts = uNumVerts;
		xGeometryOut.m_uNumIndices = uNumIndices;

		xGeometryOut.m_pxPositions = static_cast<Zenith_Maths::Vector3*>(
			Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
		xGeometryOut.m_pxUVs = static_cast<Zenith_Maths::Vector2*>(
			Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector2)));
		xGeometryOut.m_pxNormals = static_cast<Zenith_Maths::Vector3*>(
			Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
		xGeometryOut.m_pxTangents = static_cast<Zenith_Maths::Vector3*>(
			Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
		xGeometryOut.m_pxBitangents = static_cast<Zenith_Maths::Vector3*>(
			Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
		xGeometryOut.m_pxColors = static_cast<Zenith_Maths::Vector4*>(
			Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector4)));
		xGeometryOut.m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(
			Zenith_MemoryManagement::Allocate(uNumIndices * sizeof(Flux_MeshGeometry::IndexType)));

		memcpy(xGeometryOut.m_pxPositions, m_axPositions.GetDataPointer(), uNumVerts * sizeof(Zenith_Maths::Vector3));
		memcpy(xGeometryOut.m_pxUVs, m_axUVs.GetDataPointer(), uNumVerts * sizeof(Zenith_Maths::Vector2));
		memcpy(xGeometryOut.m_pxNormals, m_axNormals.GetDataPointer(), uNumVerts * sizeof(Zenith_Maths::Vector3));
		memcpy(xGeometryOut.m_pxTangents, m_axTangents.GetDataPointer(), uNumVerts * sizeof(Zenith_Maths::Vector3));
		memcpy(xGeometryOut.m_pxBitangents, m_axBitangents.GetDataPointer(), uNumVerts * sizeof(Zenith_Maths::Vector3));
		memcpy(xGeometryOut.m_pxColors, m_axColors.GetDataPointer(), uNumVerts * sizeof(Zenith_Maths::Vector4));
		memcpy(xGeometryOut.m_puIndices, m_auIndices.GetDataPointer(), uNumIndices * sizeof(uint32_t));
	}
};

// Build CW perimeter (viewed from +Y) for a single cell
static void BuildCellPerimeter(
	float fMinX, float fMaxX, float fMinZ, float fMaxZ,
	bool bHasRight, bool bHasLeft, bool bHasFront, bool bHasBack,
	Zenith_Vector<PerimeterPoint>& axPerimeterOut)
{
	// Corner positions: BR(+X,-Z), BL(-X,-Z), TL(-X,+Z), TR(+X,+Z)
	// CW walk from above: BR -> BL -> TL -> TR
	// Edges between: -Z (BR->BL), -X (BL->TL), +Z (TL->TR), +X (TR->BR)

	struct CornerInfo
	{
		float m_fCornerX, m_fCornerZ;
		float m_fArcCenterX, m_fArcCenterZ;
		float m_fStartAngle;
		bool m_bConvex;
		bool m_bNextEdgeExterior;
	};

	// Determine convexity: convex if neither adjacent cardinal neighbor exists
	bool bConvexBR = !bHasRight && !bHasBack;
	bool bConvexBL = !bHasLeft && !bHasBack;
	bool bConvexTL = !bHasLeft && !bHasFront;
	bool bConvexTR = !bHasRight && !bHasFront;

	CornerInfo axCorners[4];

	// Corner 0: BR (+X,-Z), next edge is -Z (BR->BL)
	axCorners[0].m_fCornerX = fMaxX;
	axCorners[0].m_fCornerZ = fMinZ;
	axCorners[0].m_fArcCenterX = fMaxX - fCORNER_RADIUS;
	axCorners[0].m_fArcCenterZ = fMinZ + fCORNER_RADIUS;
	axCorners[0].m_fStartAngle = 0.f;
	axCorners[0].m_bConvex = bConvexBR;
	axCorners[0].m_bNextEdgeExterior = !bHasBack;

	// Corner 1: BL (-X,-Z), next edge is -X (BL->TL)
	axCorners[1].m_fCornerX = fMinX;
	axCorners[1].m_fCornerZ = fMinZ;
	axCorners[1].m_fArcCenterX = fMinX + fCORNER_RADIUS;
	axCorners[1].m_fArcCenterZ = fMinZ + fCORNER_RADIUS;
	axCorners[1].m_fStartAngle = -fPI / 2.f;
	axCorners[1].m_bConvex = bConvexBL;
	axCorners[1].m_bNextEdgeExterior = !bHasLeft;

	// Corner 2: TL (-X,+Z), next edge is +Z (TL->TR)
	axCorners[2].m_fCornerX = fMinX;
	axCorners[2].m_fCornerZ = fMaxZ;
	axCorners[2].m_fArcCenterX = fMinX + fCORNER_RADIUS;
	axCorners[2].m_fArcCenterZ = fMaxZ - fCORNER_RADIUS;
	axCorners[2].m_fStartAngle = fPI;
	axCorners[2].m_bConvex = bConvexTL;
	axCorners[2].m_bNextEdgeExterior = !bHasFront;

	// Corner 3: TR (+X,+Z), next edge is +X (TR->BR)
	axCorners[3].m_fCornerX = fMaxX;
	axCorners[3].m_fCornerZ = fMaxZ;
	axCorners[3].m_fArcCenterX = fMaxX - fCORNER_RADIUS;
	axCorners[3].m_fArcCenterZ = fMaxZ - fCORNER_RADIUS;
	axCorners[3].m_fStartAngle = fPI / 2.f;
	axCorners[3].m_bConvex = bConvexTR;
	axCorners[3].m_bNextEdgeExterior = !bHasRight;

	// Edge outward normals: -Z->(0,-1), -X->(-1,0), +Z->(0,1), +X->(1,0)
	float afEdgeOutX[4] = { 0.f, -1.f, 0.f, 1.f };
	float afEdgeOutZ[4] = { -1.f, 0.f, 1.f, 0.f };

	for (uint32_t uCorner = 0; uCorner < 4; ++uCorner)
	{
		const CornerInfo& xCorner = axCorners[uCorner];

		if (xCorner.m_bConvex)
		{
			// Emit arc: CW sweep of -PI/2 from start angle
			for (uint32_t uSeg = 0; uSeg <= uCORNER_SEGMENTS; ++uSeg)
			{
				float fTheta = xCorner.m_fStartAngle
					- static_cast<float>(uSeg) * (fPI / 2.f) / static_cast<float>(uCORNER_SEGMENTS);
				float fCosTheta = cosf(fTheta);
				float fSinTheta = sinf(fTheta);

				PerimeterPoint xPoint;
				xPoint.m_fX = xCorner.m_fArcCenterX + fCORNER_RADIUS * fCosTheta;
				xPoint.m_fZ = xCorner.m_fArcCenterZ + fCORNER_RADIUS * fSinTheta;
				xPoint.m_fOutX = fCosTheta;
				xPoint.m_fOutZ = fSinTheta;
				// All arc segments and following edge are exterior for convex corners
				xPoint.m_bExterior = true;
				axPerimeterOut.PushBack(xPoint);
			}
		}
		else
		{
			// Emit two points at the corner: one with the previous edge's outward
			// normal (terminates the previous edge's side wall correctly) and one
			// with the next edge's outward normal (starts the next edge correctly).
			// The degenerate zero-length segment between them is marked non-exterior
			// so side walls and edge rounding skip it.
			uint32_t uPrevCorner = (uCorner + 3) % 4;

			PerimeterPoint xPt1;
			xPt1.m_fX = xCorner.m_fCornerX;
			xPt1.m_fZ = xCorner.m_fCornerZ;
			xPt1.m_fOutX = afEdgeOutX[uPrevCorner];
			xPt1.m_fOutZ = afEdgeOutZ[uPrevCorner];
			xPt1.m_bExterior = false;
			axPerimeterOut.PushBack(xPt1);

			PerimeterPoint xPt2;
			xPt2.m_fX = xCorner.m_fCornerX;
			xPt2.m_fZ = xCorner.m_fCornerZ;
			xPt2.m_fOutX = afEdgeOutX[uCorner];
			xPt2.m_fOutZ = afEdgeOutZ[uCorner];
			xPt2.m_bExterior = xCorner.m_bNextEdgeExterior;
			axPerimeterOut.PushBack(xPt2);
		}
	}
}

// Per-point edge rounding scale: 1.0 if both adjacent segments are exterior, 0.0 otherwise.
// This prevents edge rounding inset on interior boundaries between cells.
static float GetEdgeScale(const Zenith_Vector<PerimeterPoint>& axPerimeter, uint32_t uIndex)
{
	uint32_t uNumPoints = axPerimeter.GetSize();
	uint32_t uPrev = (uIndex + uNumPoints - 1) % uNumPoints;
	bool bPrevExterior = axPerimeter.Get(uPrev).m_bExterior;
	bool bThisExterior = axPerimeter.Get(uIndex).m_bExterior;
	return (bPrevExterior && bThisExterior) ? 1.f : 0.f;
}

static void EmitTopFace(
	MeshBuilder& xBuilder,
	const Zenith_Vector<PerimeterPoint>& axPerimeter,
	float fMaxY, float fCenterX, float fCenterZ, bool bFlipV = false)
{
	uint32_t uNumPoints = axPerimeter.GetSize();

	// Center vertex
	uint32_t uCenter = xBuilder.AddVertex(
		{ fCenterX, fMaxY, fCenterZ },
		{ 0.5f, 0.5f },
		{ 0.f, 1.f, 0.f },
		{ 1.f, 0.f, 0.f },
		{ 0.f, 0.f, -1.f });

	// Perimeter vertices (only inset on fully-exterior points)
	Zenith_Vector<uint32_t> auPerimVerts;
	for (uint32_t u = 0; u < uNumPoints; ++u)
	{
		const PerimeterPoint& xPt = axPerimeter.Get(u);
		float fScale = GetEdgeScale(axPerimeter, u);
		float fInset = fEDGE_RADIUS * fScale;
		float fX = xPt.m_fX - xPt.m_fOutX * fInset;
		float fZ = xPt.m_fZ - xPt.m_fOutZ * fInset;
		float fU = fX - fCenterX + 0.5f;
		float fV = bFlipV ? (fCenterZ - fZ + 0.5f) : (fZ - fCenterZ + 0.5f);

		uint32_t uIdx = xBuilder.AddVertex(
			{ fX, fMaxY, fZ },
			{ fU, fV },
			{ 0.f, 1.f, 0.f },
			{ 1.f, 0.f, 0.f },
			{ 0.f, 0.f, -1.f });
		auPerimVerts.PushBack(uIdx);
	}

	// Triangle fan (winding matches existing top face convention)
	for (uint32_t u = 0; u < uNumPoints; ++u)
	{
		uint32_t uNext = (u + 1) % uNumPoints;
		xBuilder.AddTriangle(uCenter, auPerimVerts.Get(u), auPerimVerts.Get(uNext));
	}
}

static void EmitEdgeRounding(
	MeshBuilder& xBuilder,
	const Zenith_Vector<PerimeterPoint>& axPerimeter,
	float fMaxY)
{
	uint32_t uNumPoints = axPerimeter.GetSize();
	uint32_t uNumRings = uEDGE_SEGMENTS + 1;

	for (uint32_t u = 0; u < uNumPoints; ++u)
	{
		if (!axPerimeter.Get(u).m_bExterior)
			continue;

		uint32_t uNext = (u + 1) % uNumPoints;
		const PerimeterPoint& xPtA = axPerimeter.Get(u);
		const PerimeterPoint& xPtB = axPerimeter.Get(uNext);
		float fScaleA = GetEdgeScale(axPerimeter, u);
		float fScaleB = GetEdgeScale(axPerimeter, uNext);

		uint32_t uBaseA = xBuilder.m_axPositions.GetSize();
		for (uint32_t uRing = 0; uRing < uNumRings; ++uRing)
		{
			float fAlpha = static_cast<float>(uRing) * (fPI / 2.f) / static_cast<float>(uEDGE_SEGMENTS);
			float fSinAlpha = sinf(fAlpha);
			float fCosAlpha = cosf(fAlpha);

			float fInset = fEDGE_RADIUS * (1.f - fSinAlpha) * fScaleA;
			float fY = fMaxY - fEDGE_RADIUS * (1.f - fCosAlpha) * fScaleA;

			float fX = xPtA.m_fX - xPtA.m_fOutX * fInset;
			float fZ = xPtA.m_fZ - xPtA.m_fOutZ * fInset;

			Zenith_Maths::Vector3 xNormal = {
				xPtA.m_fOutX * fSinAlpha,
				fCosAlpha,
				xPtA.m_fOutZ * fSinAlpha };
			Zenith_Maths::Vector3 xTangent = { xPtA.m_fOutZ, 0.f, -xPtA.m_fOutX };
			Zenith_Maths::Vector3 xBitangent = {
				-fCosAlpha * xPtA.m_fOutX,
				fSinAlpha,
				-fCosAlpha * xPtA.m_fOutZ };

			xBuilder.AddVertex({ fX, fY, fZ }, { 0.f, fAlpha / (fPI / 2.f) }, xNormal, xTangent, xBitangent);
		}

		uint32_t uBaseB = xBuilder.m_axPositions.GetSize();
		for (uint32_t uRing = 0; uRing < uNumRings; ++uRing)
		{
			float fAlpha = static_cast<float>(uRing) * (fPI / 2.f) / static_cast<float>(uEDGE_SEGMENTS);
			float fSinAlpha = sinf(fAlpha);
			float fCosAlpha = cosf(fAlpha);

			float fInset = fEDGE_RADIUS * (1.f - fSinAlpha) * fScaleB;
			float fY = fMaxY - fEDGE_RADIUS * (1.f - fCosAlpha) * fScaleB;

			float fX = xPtB.m_fX - xPtB.m_fOutX * fInset;
			float fZ = xPtB.m_fZ - xPtB.m_fOutZ * fInset;

			Zenith_Maths::Vector3 xNormal = {
				xPtB.m_fOutX * fSinAlpha,
				fCosAlpha,
				xPtB.m_fOutZ * fSinAlpha };
			Zenith_Maths::Vector3 xTangent = { xPtB.m_fOutZ, 0.f, -xPtB.m_fOutX };
			Zenith_Maths::Vector3 xBitangent = {
				-fCosAlpha * xPtB.m_fOutX,
				fSinAlpha,
				-fCosAlpha * xPtB.m_fOutZ };

			xBuilder.AddVertex({ fX, fY, fZ }, { 1.f, fAlpha / (fPI / 2.f) }, xNormal, xTangent, xBitangent);
		}

		for (uint32_t uRing = 0; uRing < uEDGE_SEGMENTS; ++uRing)
		{
			uint32_t uA0 = uBaseA + uRing;
			uint32_t uA1 = uBaseA + uRing + 1;
			uint32_t uB0 = uBaseB + uRing;
			uint32_t uB1 = uBaseB + uRing + 1;
			xBuilder.AddTriangle(uA0, uA1, uB0);
			xBuilder.AddTriangle(uB0, uA1, uB1);
		}
	}
}

static void EmitSideWalls(
	MeshBuilder& xBuilder,
	const Zenith_Vector<PerimeterPoint>& axPerimeter,
	float fMinY, float fMaxY)
{
	uint32_t uNumPoints = axPerimeter.GetSize();

	for (uint32_t u = 0; u < uNumPoints; ++u)
	{
		if (!axPerimeter.Get(u).m_bExterior)
			continue;

		uint32_t uNext = (u + 1) % uNumPoints;
		const PerimeterPoint& xPtA = axPerimeter.Get(u);
		const PerimeterPoint& xPtB = axPerimeter.Get(uNext);

		// Per-point side wall top: accounts for edge rounding where present
		float fTopA = fMaxY - fEDGE_RADIUS * GetEdgeScale(axPerimeter, u);
		float fTopB = fMaxY - fEDGE_RADIUS * GetEdgeScale(axPerimeter, uNext);

		Zenith_Maths::Vector3 xTangentA = { xPtA.m_fOutZ, 0.f, -xPtA.m_fOutX };
		Zenith_Maths::Vector3 xTangentB = { xPtB.m_fOutZ, 0.f, -xPtB.m_fOutX };

		uint32_t uV0 = xBuilder.AddVertex(
			{ xPtA.m_fX, fMinY, xPtA.m_fZ }, { 0.f, 0.f },
			{ xPtA.m_fOutX, 0.f, xPtA.m_fOutZ }, xTangentA, { 0.f, 1.f, 0.f });
		uint32_t uV1 = xBuilder.AddVertex(
			{ xPtB.m_fX, fMinY, xPtB.m_fZ }, { 1.f, 0.f },
			{ xPtB.m_fOutX, 0.f, xPtB.m_fOutZ }, xTangentB, { 0.f, 1.f, 0.f });
		uint32_t uV2 = xBuilder.AddVertex(
			{ xPtA.m_fX, fTopA, xPtA.m_fZ }, { 0.f, 1.f },
			{ xPtA.m_fOutX, 0.f, xPtA.m_fOutZ }, xTangentA, { 0.f, 1.f, 0.f });
		uint32_t uV3 = xBuilder.AddVertex(
			{ xPtB.m_fX, fTopB, xPtB.m_fZ }, { 1.f, 1.f },
			{ xPtB.m_fOutX, 0.f, xPtB.m_fOutZ }, xTangentB, { 0.f, 1.f, 0.f });

		xBuilder.AddTriangle(uV0, uV2, uV1);
		xBuilder.AddTriangle(uV1, uV2, uV3);
	}
}

static void GenerateShapeMesh(const TilePuzzleShapeDefinition& xDef, Flux_MeshGeometry& xGeometryOut)
{
	const std::vector<TilePuzzleCellOffset>& axCells = xDef.axCells;
	uint32_t uNumCells = static_cast<uint32_t>(axCells.size());

	// Build occupancy set for O(1) neighbor lookup
	std::unordered_set<uint32_t> xOccupied;
	for (uint32_t c = 0; c < uNumCells; ++c)
	{
		uint32_t uKey = (static_cast<uint32_t>(axCells[c].iY + 128)) * 256
			+ static_cast<uint32_t>(axCells[c].iX + 128);
		xOccupied.insert(uKey);
	}

	auto IsOccupied = [&](int32_t iX, int32_t iY) -> bool
	{
		uint32_t uKey = (static_cast<uint32_t>(iY + 128)) * 256
			+ static_cast<uint32_t>(iX + 128);
		return xOccupied.count(uKey) > 0;
	};

	MeshBuilder xBuilder;
	float fMinY = -fHALF_HEIGHT;
	float fMaxY = fHALF_HEIGHT;

	for (uint32_t c = 0; c < uNumCells; ++c)
	{
		float fCX = static_cast<float>(axCells[c].iX);
		float fCZ = static_cast<float>(axCells[c].iY);
		int32_t iCX = axCells[c].iX;
		int32_t iCY = axCells[c].iY;

		bool bHasRight = IsOccupied(iCX + 1, iCY);
		bool bHasLeft = IsOccupied(iCX - 1, iCY);
		bool bHasFront = IsOccupied(iCX, iCY + 1);
		bool bHasBack = IsOccupied(iCX, iCY - 1);

		// Border-adjusted extents
		float fMinX = fCX - fHALF + (bHasLeft ? 0.f : fBORDER);
		float fMaxX = fCX + fHALF - (bHasRight ? 0.f : fBORDER);
		float fMinZ = fCZ - fHALF + (bHasBack ? 0.f : fBORDER);
		float fMaxZ = fCZ + fHALF - (bHasFront ? 0.f : fBORDER);

		// Build CW perimeter for this cell
		Zenith_Vector<PerimeterPoint> axPerimeter;
		BuildCellPerimeter(
			fMinX, fMaxX, fMinZ, fMaxZ,
			bHasRight, bHasLeft, bHasFront, bHasBack,
			axPerimeter);

		// Emit geometry layers (no bottom face — always occluded from the top-down camera)
		EmitTopFace(xBuilder, axPerimeter, fMaxY, fCX, fCZ);
		EmitEdgeRounding(xBuilder, axPerimeter, fMaxY);
		EmitSideWalls(xBuilder, axPerimeter, fMinY, fMaxY);
	}

	xBuilder.CopyToGeometry(xGeometryOut);
	xGeometryOut.GenerateLayoutAndVertexData();
	Flux_MemoryManager::InitialiseVertexBuffer(
		xGeometryOut.GetVertexData(), xGeometryOut.GetVertexDataSize(), xGeometryOut.m_xVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(
		xGeometryOut.GetIndexData(), xGeometryOut.GetIndexDataSize(), xGeometryOut.m_xIndexBuffer);
}

void TilePuzzle::GenerateShapeMeshFromDefinition(const TilePuzzleShapeDefinition& xDef, Flux_MeshGeometry& xGeometryOut)
{
	GenerateShapeMesh(xDef, xGeometryOut);
}

// ============================================================================
// Cat Head Mesh Generation
// ============================================================================
static void GenerateCatMesh(Flux_MeshGeometry& xGeometryOut)
{
	MeshBuilder xBuilder;
	Zenith_Vector<PerimeterPoint> axPerimeter;

	// Cat head geometry parameters (10% larger than base 0.35/0.48)
	static constexpr float fHEAD_RADIUS = 0.385f;
	static constexpr float fEAR_TIP_RADIUS = 0.528f;
	static constexpr float fEAR_ANGLE_OFFSET = 0.45f;
	static constexpr float fEAR_HALF_WIDTH = 0.18f;
	static constexpr uint32_t uCIRCLE_RESOLUTION = 24;

	// CW perimeter from +Y: x = R*cos(t), z = -R*sin(t), t in [0, 2pi]
	// Top (+Z) is at t = 3pi/2
	// Left ear (negative X) at t = 3pi/2 - offset, right ear (positive X) at t = 3pi/2 + offset
	float fTopT = 1.5f * fPI;
	float fLEStart = fTopT - fEAR_ANGLE_OFFSET - fEAR_HALF_WIDTH;
	float fLETip = fTopT - fEAR_ANGLE_OFFSET;
	float fLEEnd = fTopT - fEAR_ANGLE_OFFSET + fEAR_HALF_WIDTH;
	float fREStart = fTopT + fEAR_ANGLE_OFFSET - fEAR_HALF_WIDTH;
	float fRETip = fTopT + fEAR_ANGLE_OFFSET;
	float fREEnd = fTopT + fEAR_ANGLE_OFFSET + fEAR_HALF_WIDTH;

	// Helper: add a circle perimeter point at parameter t
	auto AddCirclePt = [&](float fT)
	{
		PerimeterPoint xPt;
		xPt.m_fX = fHEAD_RADIUS * cosf(fT);
		xPt.m_fZ = -fHEAD_RADIUS * sinf(fT);
		xPt.m_fOutX = cosf(fT);
		xPt.m_fOutZ = -sinf(fT);
		xPt.m_bExterior = true;
		axPerimeter.PushBack(xPt);
	};

	// Helper: add ear tip point with averaged edge normals
	auto AddEarTip = [&](float fTBase1, float fTTip, float fTBase2)
	{
		float fBase1X = fHEAD_RADIUS * cosf(fTBase1);
		float fBase1Z = -fHEAD_RADIUS * sinf(fTBase1);
		float fTipX = fEAR_TIP_RADIUS * cosf(fTTip);
		float fTipZ = -fEAR_TIP_RADIUS * sinf(fTTip);
		float fBase2X = fHEAD_RADIUS * cosf(fTBase2);
		float fBase2Z = -fHEAD_RADIUS * sinf(fTBase2);

		// Edge 1 (base1 -> tip): outward normal = CCW rotation of edge direction
		float fDx1 = fTipX - fBase1X;
		float fDz1 = fTipZ - fBase1Z;
		float fLen1 = sqrtf(fDx1 * fDx1 + fDz1 * fDz1);
		float fN1X = -fDz1 / fLen1;
		float fN1Z = fDx1 / fLen1;

		// Edge 2 (tip -> base2): outward normal = CCW rotation of edge direction
		float fDx2 = fBase2X - fTipX;
		float fDz2 = fBase2Z - fTipZ;
		float fLen2 = sqrtf(fDx2 * fDx2 + fDz2 * fDz2);
		float fN2X = -fDz2 / fLen2;
		float fN2Z = fDx2 / fLen2;

		// Average and normalize
		float fAvgX = fN1X + fN2X;
		float fAvgZ = fN1Z + fN2Z;
		float fAvgLen = sqrtf(fAvgX * fAvgX + fAvgZ * fAvgZ);
		fAvgX /= fAvgLen;
		fAvgZ /= fAvgLen;

		PerimeterPoint xPt;
		xPt.m_fX = fTipX;
		xPt.m_fZ = fTipZ;
		xPt.m_fOutX = fAvgX;
		xPt.m_fOutZ = fAvgZ;
		xPt.m_bExterior = true;
		axPerimeter.PushBack(xPt);
	};

	// Helper: emit evenly-spaced circle points for an arc
	auto EmitArc = [&](float fTStart, float fTEnd, bool bIncludeStart, bool bIncludeEnd)
	{
		float fSpan = fTEnd - fTStart;
		uint32_t uSegs = static_cast<uint32_t>(fSpan / (2.f * fPI) * static_cast<float>(uCIRCLE_RESOLUTION));
		if (uSegs < 2)
			uSegs = 2;
		uint32_t uStart = bIncludeStart ? 0 : 1;
		uint32_t uEnd = bIncludeEnd ? uSegs : uSegs - 1;
		for (uint32_t i = uStart; i <= uEnd; ++i)
		{
			float fT = fTStart + fSpan * static_cast<float>(i) / static_cast<float>(uSegs);
			AddCirclePt(fT);
		}
	};

	// Build CW perimeter:
	// Section 1: circle from 0 to left ear start
	EmitArc(0.f, fLEStart, true, false);

	// Left ear
	AddCirclePt(fLEStart);
	AddEarTip(fLEStart, fLETip, fLEEnd);
	AddCirclePt(fLEEnd);

	// Section 3: circle between ears
	EmitArc(fLEEnd, fREStart, false, false);

	// Right ear
	AddCirclePt(fREStart);
	AddEarTip(fREStart, fRETip, fREEnd);
	AddCirclePt(fREEnd);

	// Section 5: circle from right ear end back to start
	EmitArc(fREEnd, 2.f * fPI, false, false);

	// Emit geometry using same pipeline as shapes
	EmitTopFace(xBuilder, axPerimeter, fHALF_HEIGHT, 0.f, 0.f, true);
	EmitEdgeRounding(xBuilder, axPerimeter, fHALF_HEIGHT);
	EmitSideWalls(xBuilder, axPerimeter, -fHALF_HEIGHT, fHALF_HEIGHT);

	// Finalize
	xBuilder.CopyToGeometry(xGeometryOut);
	xGeometryOut.GenerateLayoutAndVertexData();
	Flux_MemoryManager::InitialiseVertexBuffer(
		xGeometryOut.GetVertexData(), xGeometryOut.GetVertexDataSize(), xGeometryOut.m_xVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(
		xGeometryOut.GetIndexData(), xGeometryOut.GetIndexDataSize(), xGeometryOut.m_xIndexBuffer);
}

// ============================================================================
// Pinball Custom Mesh Generation
// ============================================================================

static void FinalizeMesh(MeshBuilder& xBuilder, Flux_MeshGeometry& xGeometry)
{
	xBuilder.CopyToGeometry(xGeometry);
	xGeometry.GenerateLayoutAndVertexData();
	Flux_MemoryManager::InitialiseVertexBuffer(
		xGeometry.GetVertexData(), xGeometry.GetVertexDataSize(), xGeometry.m_xVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(
		xGeometry.GetIndexData(), xGeometry.GetIndexDataSize(), xGeometry.m_xIndexBuffer);
}

static void GenerateBumperMesh(Flux_MeshGeometry& xGeometry)
{
	MeshBuilder xBuilder;
	static constexpr uint32_t uSegments = 16;
	static constexpr uint32_t uDomeRings = 8;
	static constexpr float fRadius = 0.5f;
	static constexpr float fBaseHeight = 0.3f;

	Zenith_Maths::Vector3 xTangent(1.f, 0.f, 0.f);
	Zenith_Maths::Vector3 xBitangent(0.f, 0.f, 1.f);

	// Bottom cap: triangle fan
	uint32_t uCenterBot = xBuilder.AddVertex(
		{0.f, 0.f, 0.f}, {0.5f, 0.5f}, {0.f, -1.f, 0.f}, xTangent, xBitangent);
	uint32_t uFirstBot = 0;
	for (uint32_t i = 0; i <= uSegments; ++i)
	{
		float fAngle = static_cast<float>(i) / static_cast<float>(uSegments) * 2.f * fPI;
		float fX = cosf(fAngle) * fRadius;
		float fZ = sinf(fAngle) * fRadius;
		uint32_t uIdx = xBuilder.AddVertex(
			{fX, 0.f, fZ},
			{fX * 0.5f + 0.5f, fZ * 0.5f + 0.5f},
			{0.f, -1.f, 0.f}, xTangent, xBitangent);
		if (i == 0) uFirstBot = uIdx;
	}
	for (uint32_t i = 0; i < uSegments; ++i)
		xBuilder.AddTriangle(uCenterBot, uFirstBot + i + 1, uFirstBot + i);

	// Cylinder sides
	for (uint32_t i = 0; i <= uSegments; ++i)
	{
		float fAngle = static_cast<float>(i) / static_cast<float>(uSegments) * 2.f * fPI;
		float fCos = cosf(fAngle);
		float fSin = sinf(fAngle);
		float fU = static_cast<float>(i) / static_cast<float>(uSegments);
		Zenith_Maths::Vector3 xNorm(fCos, 0.f, fSin);

		xBuilder.AddVertex({fCos * fRadius, 0.f, fSin * fRadius}, {fU, 0.f}, xNorm, xTangent, xBitangent);
		xBuilder.AddVertex({fCos * fRadius, fBaseHeight, fSin * fRadius}, {fU, 0.5f}, xNorm, xTangent, xBitangent);
	}
	uint32_t uCylStart = uFirstBot + uSegments + 1;
	for (uint32_t i = 0; i < uSegments; ++i)
	{
		uint32_t uBL = uCylStart + i * 2;
		uint32_t uBR = uCylStart + (i + 1) * 2;
		uint32_t uTL = uBL + 1;
		uint32_t uTR = uBR + 1;
		xBuilder.AddTriangle(uBL, uBR, uTR);
		xBuilder.AddTriangle(uBL, uTR, uTL);
	}

	// Dome (hemisphere)
	uint32_t uDomeStart = xBuilder.m_axPositions.GetSize();
	for (uint32_t iRing = 0; iRing <= uDomeRings; ++iRing)
	{
		float fPhi = static_cast<float>(iRing) / static_cast<float>(uDomeRings) * fPI * 0.5f;
		float fY = fBaseHeight + sinf(fPhi) * fRadius;
		float fRingRadius = cosf(fPhi) * fRadius;

		for (uint32_t iSeg = 0; iSeg <= uSegments; ++iSeg)
		{
			float fTheta = static_cast<float>(iSeg) / static_cast<float>(uSegments) * 2.f * fPI;
			float fX = cosf(fTheta) * fRingRadius;
			float fZ = sinf(fTheta) * fRingRadius;
			Zenith_Maths::Vector3 xNorm = glm::normalize(Zenith_Maths::Vector3(
				cosf(fTheta) * cosf(fPhi), sinf(fPhi), sinf(fTheta) * cosf(fPhi)));
			float fU = static_cast<float>(iSeg) / static_cast<float>(uSegments);
			float fV = 0.5f + static_cast<float>(iRing) / static_cast<float>(uDomeRings) * 0.5f;
			xBuilder.AddVertex({fX, fY, fZ}, {fU, fV}, xNorm, xTangent, xBitangent);
		}
	}
	for (uint32_t iRing = 0; iRing < uDomeRings; ++iRing)
	{
		for (uint32_t iSeg = 0; iSeg < uSegments; ++iSeg)
		{
			uint32_t uCur = uDomeStart + iRing * (uSegments + 1) + iSeg;
			uint32_t uNext = uCur + uSegments + 1;
			xBuilder.AddTriangle(uCur, uCur + 1, uNext + 1);
			xBuilder.AddTriangle(uCur, uNext + 1, uNext);
		}
	}

	FinalizeMesh(xBuilder, xGeometry);
}

static void GenerateBeveledCubeMesh(Flux_MeshGeometry& xGeometry)
{
	MeshBuilder xBuilder;
	static constexpr float fHalf = 0.5f;
	static constexpr float fBevel = 0.06f;
	static constexpr float fInner = fHalf - fBevel;
	static constexpr uint32_t uBevelSegs = 3;

	Zenith_Maths::Vector3 xTangent(1.f, 0.f, 0.f);
	Zenith_Maths::Vector3 xBitangent(0.f, 0.f, 1.f);

	// 6 flat faces (inset by bevel radius)
	auto AddQuad = [&](Zenith_Maths::Vector3 a, Zenith_Maths::Vector3 b,
		Zenith_Maths::Vector3 c, Zenith_Maths::Vector3 d, Zenith_Maths::Vector3 n) {
		Zenith_Maths::Vector3 t = glm::normalize(b - a);
		Zenith_Maths::Vector3 bt = glm::normalize(glm::cross(n, t));
		uint32_t u0 = xBuilder.AddVertex(a, {0.f, 0.f}, n, t, bt);
		uint32_t u1 = xBuilder.AddVertex(b, {1.f, 0.f}, n, t, bt);
		uint32_t u2 = xBuilder.AddVertex(c, {1.f, 1.f}, n, t, bt);
		uint32_t u3 = xBuilder.AddVertex(d, {0.f, 1.f}, n, t, bt);
		xBuilder.AddTriangle(u0, u1, u2);
		xBuilder.AddTriangle(u0, u2, u3);
	};

	// Top (+Y)
	AddQuad({-fInner, fHalf, -fInner}, {fInner, fHalf, -fInner},
		{fInner, fHalf, fInner}, {-fInner, fHalf, fInner}, {0,1,0});
	// Bottom (-Y)
	AddQuad({-fInner, -fHalf, fInner}, {fInner, -fHalf, fInner},
		{fInner, -fHalf, -fInner}, {-fInner, -fHalf, -fInner}, {0,-1,0});
	// Front (+Z)
	AddQuad({-fInner, -fInner, fHalf}, {fInner, -fInner, fHalf},
		{fInner, fInner, fHalf}, {-fInner, fInner, fHalf}, {0,0,1});
	// Back (-Z)
	AddQuad({fInner, -fInner, -fHalf}, {-fInner, -fInner, -fHalf},
		{-fInner, fInner, -fHalf}, {fInner, fInner, -fHalf}, {0,0,-1});
	// Right (+X)
	AddQuad({fHalf, -fInner, fInner}, {fHalf, -fInner, -fInner},
		{fHalf, fInner, -fInner}, {fHalf, fInner, fInner}, {1,0,0});
	// Left (-X)
	AddQuad({-fHalf, -fInner, -fInner}, {-fHalf, -fInner, fInner},
		{-fHalf, fInner, fInner}, {-fHalf, fInner, -fInner}, {-1,0,0});

	// 12 edge bevels (quarter-cylinder strips)
	auto AddEdgeBevel = [&](Zenith_Maths::Vector3 xEdgeStart, Zenith_Maths::Vector3 xEdgeEnd,
		Zenith_Maths::Vector3 xN1, Zenith_Maths::Vector3 xN2) {
		Zenith_Maths::Vector3 xEdgeDir = glm::normalize(xEdgeEnd - xEdgeStart);
		for (uint32_t i = 0; i <= uBevelSegs; ++i)
		{
			float fT = static_cast<float>(i) / static_cast<float>(uBevelSegs);
			float fAngle = fT * fPI * 0.5f;
			Zenith_Maths::Vector3 xNorm = glm::normalize(xN1 * cosf(fAngle) + xN2 * sinf(fAngle));
			Zenith_Maths::Vector3 xOffset = xN1 * (cosf(fAngle) * fBevel) + xN2 * (sinf(fAngle) * fBevel);

			uint32_t uA = xBuilder.AddVertex(xEdgeStart + xOffset, {fT, 0.f}, xNorm, xEdgeDir, glm::cross(xNorm, xEdgeDir));
			uint32_t uB = xBuilder.AddVertex(xEdgeEnd + xOffset, {fT, 1.f}, xNorm, xEdgeDir, glm::cross(xNorm, xEdgeDir));

			if (i > 0)
			{
				xBuilder.AddTriangle(uA - 2, uB - 2, uB);
				xBuilder.AddTriangle(uA - 2, uB, uA);
			}
		}
	};

	// Top 4 edges
	AddEdgeBevel({-fInner, fInner, -fInner}, {fInner, fInner, -fInner}, {0,0,-1}, {0,1,0});
	AddEdgeBevel({fInner, fInner, -fInner}, {fInner, fInner, fInner}, {1,0,0}, {0,1,0});
	AddEdgeBevel({fInner, fInner, fInner}, {-fInner, fInner, fInner}, {0,0,1}, {0,1,0});
	AddEdgeBevel({-fInner, fInner, fInner}, {-fInner, fInner, -fInner}, {-1,0,0}, {0,1,0});

	// Bottom 4 edges
	AddEdgeBevel({-fInner, -fInner, fInner}, {fInner, -fInner, fInner}, {0,0,1}, {0,-1,0});
	AddEdgeBevel({fInner, -fInner, fInner}, {fInner, -fInner, -fInner}, {1,0,0}, {0,-1,0});
	AddEdgeBevel({fInner, -fInner, -fInner}, {-fInner, -fInner, -fInner}, {0,0,-1}, {0,-1,0});
	AddEdgeBevel({-fInner, -fInner, -fInner}, {-fInner, -fInner, fInner}, {-1,0,0}, {0,-1,0});

	// Vertical 4 edges
	AddEdgeBevel({fInner, -fInner, fInner}, {fInner, fInner, fInner}, {0,0,1}, {1,0,0});
	AddEdgeBevel({fInner, -fInner, -fInner}, {fInner, fInner, -fInner}, {1,0,0}, {0,0,-1});
	AddEdgeBevel({-fInner, -fInner, -fInner}, {-fInner, fInner, -fInner}, {0,0,-1}, {-1,0,0});
	AddEdgeBevel({-fInner, -fInner, fInner}, {-fInner, fInner, fInner}, {-1,0,0}, {0,0,1});

	FinalizeMesh(xBuilder, xGeometry);
}

static void GeneratePlungerMesh(Flux_MeshGeometry& xGeometry)
{
	MeshBuilder xBuilder;
	static constexpr uint32_t uSegments = 12;
	static constexpr float fShaftRadius = 0.3f;
	static constexpr float fShaftHeight = 0.7f;
	static constexpr float fHandleRadius = 0.35f;
	static constexpr uint32_t uHandleRings = 6;

	Zenith_Maths::Vector3 xTangent(1.f, 0.f, 0.f);
	Zenith_Maths::Vector3 xBitangent(0.f, 0.f, 1.f);

	// Bottom cap
	uint32_t uCenter = xBuilder.AddVertex({0,0,0}, {0.5f,0.5f}, {0,-1,0}, xTangent, xBitangent);
	uint32_t uFirstBot = 0;
	for (uint32_t i = 0; i <= uSegments; ++i)
	{
		float fAngle = static_cast<float>(i) / static_cast<float>(uSegments) * 2.f * fPI;
		uint32_t uIdx = xBuilder.AddVertex(
			{cosf(fAngle) * fShaftRadius, 0.f, sinf(fAngle) * fShaftRadius},
			{cosf(fAngle) * 0.5f + 0.5f, sinf(fAngle) * 0.5f + 0.5f},
			{0,-1,0}, xTangent, xBitangent);
		if (i == 0) uFirstBot = uIdx;
	}
	for (uint32_t i = 0; i < uSegments; ++i)
		xBuilder.AddTriangle(uCenter, uFirstBot + i + 1, uFirstBot + i);

	// Cylinder shaft
	uint32_t uCylStart = xBuilder.m_axPositions.GetSize();
	for (uint32_t i = 0; i <= uSegments; ++i)
	{
		float fAngle = static_cast<float>(i) / static_cast<float>(uSegments) * 2.f * fPI;
		float fCos = cosf(fAngle), fSin = sinf(fAngle);
		Zenith_Maths::Vector3 xN(fCos, 0.f, fSin);
		float fU = static_cast<float>(i) / static_cast<float>(uSegments);
		xBuilder.AddVertex({fCos * fShaftRadius, 0.f, fSin * fShaftRadius}, {fU, 0.f}, xN, xTangent, xBitangent);
		xBuilder.AddVertex({fCos * fShaftRadius, fShaftHeight, fSin * fShaftRadius}, {fU, 0.7f}, xN, xTangent, xBitangent);
	}
	for (uint32_t i = 0; i < uSegments; ++i)
	{
		uint32_t uBL = uCylStart + i * 2;
		xBuilder.AddTriangle(uBL, uBL + 2, uBL + 3);
		xBuilder.AddTriangle(uBL, uBL + 3, uBL + 1);
	}

	// Handle dome (hemisphere)
	uint32_t uDomeStart = xBuilder.m_axPositions.GetSize();
	for (uint32_t iRing = 0; iRing <= uHandleRings; ++iRing)
	{
		float fPhi = static_cast<float>(iRing) / static_cast<float>(uHandleRings) * fPI * 0.5f;
		float fY = fShaftHeight + sinf(fPhi) * fHandleRadius;
		float fRR = cosf(fPhi) * fHandleRadius;
		for (uint32_t iSeg = 0; iSeg <= uSegments; ++iSeg)
		{
			float fTheta = static_cast<float>(iSeg) / static_cast<float>(uSegments) * 2.f * fPI;
			Zenith_Maths::Vector3 xN = glm::normalize(Zenith_Maths::Vector3(
				cosf(fTheta) * cosf(fPhi), sinf(fPhi), sinf(fTheta) * cosf(fPhi)));
			xBuilder.AddVertex(
				{cosf(fTheta) * fRR, fY, sinf(fTheta) * fRR},
				{static_cast<float>(iSeg) / uSegments, 0.7f + static_cast<float>(iRing) / uHandleRings * 0.3f},
				xN, xTangent, xBitangent);
		}
	}
	for (uint32_t iRing = 0; iRing < uHandleRings; ++iRing)
	{
		for (uint32_t iSeg = 0; iSeg < uSegments; ++iSeg)
		{
			uint32_t uCur = uDomeStart + iRing * (uSegments + 1) + iSeg;
			uint32_t uNext = uCur + uSegments + 1;
			xBuilder.AddTriangle(uCur, uCur + 1, uNext + 1);
			xBuilder.AddTriangle(uCur, uNext + 1, uNext);
		}
	}

	FinalizeMesh(xBuilder, xGeometry);
}

static void GenerateTargetRampMesh(Flux_MeshGeometry& xGeometry)
{
	MeshBuilder xBuilder;

	// Tapered wedge: wider front, narrower back, sloping down
	float fFrontW = 0.5f, fBackW = 0.3f;
	float fDepth = 0.5f;
	float fFrontH = 0.3f, fBackH = 0.1f;

	// 8 corner vertices (top trapezoid + bottom rectangle)
	Zenith_Maths::Vector3 axTop[4] = {
		{-fFrontW, fFrontH, -fDepth * 0.5f},  // front-left
		{ fFrontW, fFrontH, -fDepth * 0.5f},  // front-right
		{ fBackW,  fBackH,   fDepth * 0.5f},   // back-right
		{-fBackW,  fBackH,   fDepth * 0.5f},   // back-left
	};
	Zenith_Maths::Vector3 axBot[4] = {
		{-fFrontW, 0.f, -fDepth * 0.5f},
		{ fFrontW, 0.f, -fDepth * 0.5f},
		{ fBackW,  0.f,  fDepth * 0.5f},
		{-fBackW,  0.f,  fDepth * 0.5f},
	};

	Zenith_Maths::Vector3 xTangent(1.f, 0.f, 0.f);
	Zenith_Maths::Vector3 xBitangent(0.f, 0.f, 1.f);

	// Helper to add a flat quad with computed face normal
	auto AddFace = [&](Zenith_Maths::Vector3 a, Zenith_Maths::Vector3 b,
		Zenith_Maths::Vector3 c, Zenith_Maths::Vector3 d) {
		Zenith_Maths::Vector3 xN = glm::normalize(glm::cross(b - a, d - a));
		Zenith_Maths::Vector3 t = glm::normalize(b - a);
		Zenith_Maths::Vector3 bt = glm::normalize(glm::cross(xN, t));
		uint32_t u0 = xBuilder.AddVertex(a, {0,0}, xN, t, bt);
		uint32_t u1 = xBuilder.AddVertex(b, {1,0}, xN, t, bt);
		uint32_t u2 = xBuilder.AddVertex(c, {1,1}, xN, t, bt);
		uint32_t u3 = xBuilder.AddVertex(d, {0,1}, xN, t, bt);
		xBuilder.AddTriangle(u0, u1, u2);
		xBuilder.AddTriangle(u0, u2, u3);
	};

	// Top (ramp surface)
	AddFace(axTop[0], axTop[1], axTop[2], axTop[3]);
	// Bottom
	AddFace(axBot[3], axBot[2], axBot[1], axBot[0]);
	// Front
	AddFace(axBot[0], axBot[1], axTop[1], axTop[0]);
	// Back
	AddFace(axBot[2], axBot[3], axTop[3], axTop[2]);
	// Left side
	AddFace(axBot[3], axBot[0], axTop[0], axTop[3]);
	// Right side
	AddFace(axBot[1], axBot[2], axTop[2], axTop[1]);

	FinalizeMesh(xBuilder, xGeometry);
}

// ============================================================================
// Shape Mesh Deserialization (runtime)
// ============================================================================
static bool ReadShapeMeshFromStream(Zenith_DataStream& xStream, Flux_MeshGeometry& xGeometry)
{
	uint32_t uNumVerts = 0;
	uint32_t uNumIndices = 0;
	xStream >> uNumVerts;
	xStream >> uNumIndices;

	if (uNumVerts == 0 || uNumIndices == 0)
		return false;

	xGeometry.m_uNumVerts = uNumVerts;
	xGeometry.m_uNumIndices = uNumIndices;

	// Allocate and read positions
	xGeometry.m_pxPositions = static_cast<Zenith_Maths::Vector3*>(
		Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	xStream.ReadData(xGeometry.m_pxPositions, uNumVerts * sizeof(Zenith_Maths::Vector3));

	// Allocate and read UVs
	xGeometry.m_pxUVs = static_cast<Zenith_Maths::Vector2*>(
		Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector2)));
	xStream.ReadData(xGeometry.m_pxUVs, uNumVerts * sizeof(Zenith_Maths::Vector2));

	// Allocate and read normals
	xGeometry.m_pxNormals = static_cast<Zenith_Maths::Vector3*>(
		Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	xStream.ReadData(xGeometry.m_pxNormals, uNumVerts * sizeof(Zenith_Maths::Vector3));

	// Allocate and read tangents
	xGeometry.m_pxTangents = static_cast<Zenith_Maths::Vector3*>(
		Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	xStream.ReadData(xGeometry.m_pxTangents, uNumVerts * sizeof(Zenith_Maths::Vector3));

	// Allocate and read bitangents
	xGeometry.m_pxBitangents = static_cast<Zenith_Maths::Vector3*>(
		Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	xStream.ReadData(xGeometry.m_pxBitangents, uNumVerts * sizeof(Zenith_Maths::Vector3));

	// Allocate and read colors
	xGeometry.m_pxColors = static_cast<Zenith_Maths::Vector4*>(
		Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector4)));
	xStream.ReadData(xGeometry.m_pxColors, uNumVerts * sizeof(Zenith_Maths::Vector4));

	// Allocate and read indices
	xGeometry.m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(
		Zenith_MemoryManagement::Allocate(uNumIndices * sizeof(Flux_MeshGeometry::IndexType)));
	xStream.ReadData(xGeometry.m_puIndices, uNumIndices * sizeof(Flux_MeshGeometry::IndexType));

	// Generate interleaved vertex data and upload to GPU
	xGeometry.GenerateLayoutAndVertexData();
	Flux_MemoryManager::InitialiseVertexBuffer(
		xGeometry.GetVertexData(), xGeometry.GetVertexDataSize(), xGeometry.m_xVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(
		xGeometry.GetIndexData(), xGeometry.GetIndexDataSize(), xGeometry.m_xIndexBuffer);

	return true;
}

static void LoadProceduralAssets(Zenith_AssetRegistry& xRegistry)
{
	using namespace TilePuzzle;

	// Load procedural textures from .ztxtr files via AssetRegistry
	g_pxIconStarFilled = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Icons/star_filled" ZENITH_TEXTURE_EXT);
	g_pxIconStarEmpty = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Icons/star_empty" ZENITH_TEXTURE_EXT);
	g_pxIconCoin = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Icons/coin" ZENITH_TEXTURE_EXT);
	g_pxIconHeart = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Icons/heart" ZENITH_TEXTURE_EXT);
	g_pxIconUndo = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Icons/undo" ZENITH_TEXTURE_EXT);
	g_pxIconSkip = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Icons/skip" ZENITH_TEXTURE_EXT);
	g_pxIconLock = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Icons/lock" ZENITH_TEXTURE_EXT);
	g_pxIconMenu = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Icons/menu" ZENITH_TEXTURE_EXT);
	g_pxIconBack = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Icons/back" ZENITH_TEXTURE_EXT);
	g_pxIconSoundOn = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Icons/sound_on" ZENITH_TEXTURE_EXT);
	g_pxIconSoundOff = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Icons/sound_off" ZENITH_TEXTURE_EXT);
	g_pxIconReset = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Icons/reset" ZENITH_TEXTURE_EXT);
	g_pxIconGear = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Icons/gear" ZENITH_TEXTURE_EXT);
	g_pxIconCatSilhouette = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Icons/cat_silhouette" ZENITH_TEXTURE_EXT);
	g_pxIconHint = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Icons/hint" ZENITH_TEXTURE_EXT);
	g_pxIconHintToken = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Icons/hint_token" ZENITH_TEXTURE_EXT);

	// Load cat face textures
	for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
	{
		char szPath[ZENITH_MAX_PATH_LENGTH];
		snprintf(szPath, sizeof(szPath), GAME_ASSETS_DIR "Textures/CatFaces/cat_face_%u" ZENITH_TEXTURE_EXT, i);
		g_apxCatFaceTextures[i] = xRegistry.Get<Zenith_TextureAsset>(szPath);
	}

	// Load gameplay textures and apply to materials
	g_pxFloorTileTexture = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Gameplay/floor_tile" ZENITH_TEXTURE_EXT);
	g_pxBlockerTexture = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Gameplay/blocker" ZENITH_TEXTURE_EXT);

	if (g_pxFloorTileTexture)
		g_xFloorMaterial.Get()->SetDiffuseTextureDirectly(g_pxFloorTileTexture);
	if (g_pxBlockerTexture)
		g_xBlockerMaterial.Get()->SetDiffuseTextureDirectly(g_pxBlockerTexture);
	for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
	{
		if (g_apxCatFaceTextures[i])
			g_axCatMaterials[i].Get()->SetDiffuseTextureDirectly(g_apxCatFaceTextures[i]);
	}

	// Load pinball materials from .zmtrl files
	g_pxPinballBallMaterial = xRegistry.Get<Zenith_MaterialAsset>(GAME_ASSETS_DIR "Materials/pinball_ball" ZENITH_MATERIAL_EXT);
	g_pxPinballPegMaterial = xRegistry.Get<Zenith_MaterialAsset>(GAME_ASSETS_DIR "Materials/pinball_peg" ZENITH_MATERIAL_EXT);
	g_pxPinballPegHitMaterial = xRegistry.Get<Zenith_MaterialAsset>(GAME_ASSETS_DIR "Materials/pinball_peg_hit" ZENITH_MATERIAL_EXT);

	// Load pinball PBR textures
	g_pxPinballBumperDiffuseTex = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Pinball/bumper_diffuse" ZENITH_TEXTURE_EXT);
	g_pxPinballBumperRMTex = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Pinball/bumper_rm" ZENITH_TEXTURE_EXT);
	g_pxPinballWallDiffuseTex = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Pinball/wall_diffuse" ZENITH_TEXTURE_EXT);
	g_pxPinballWallRMTex = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Pinball/wall_rm" ZENITH_TEXTURE_EXT);
	g_pxPinballFloorDiffuseTex = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Pinball/floor_diffuse" ZENITH_TEXTURE_EXT);
	g_pxPinballFloorRMTex = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Pinball/floor_rm" ZENITH_TEXTURE_EXT);
	g_pxPinballPlungerRMTex = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Pinball/plunger_rm" ZENITH_TEXTURE_EXT);
	g_pxPinballTargetDiffuseTex = xRegistry.Get<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Pinball/target_diffuse" ZENITH_TEXTURE_EXT);

	// Load particle configs
	TilePuzzle_AssetGen::LoadParticleConfigs();
}

static void InitializeTilePuzzleResources()
{
	if (s_bResourcesInitialized)
		return;

	using namespace TilePuzzle;

	// Create geometry using registry's cached primitives
	g_pxCubeAsset = Zenith_MeshGeometryAsset::CreateUnitCube();
	g_pxCubeGeometry = g_pxCubeAsset->GetGeometry();

	g_pxSphereAsset = Zenith_MeshGeometryAsset::CreateUnitSphere(16);
	g_pxSphereGeometry = g_pxSphereAsset->GetGeometry();

	// Generate cat head mesh
	g_pxCatMeshGeometry = new Flux_MeshGeometry();
	GenerateCatMesh(*g_pxCatMeshGeometry);

	// Generate pinball custom meshes
	g_pxBumperGeometry = new Flux_MeshGeometry();
	GenerateBumperMesh(*g_pxBumperGeometry);
	g_pxBeveledCubeGeometry = new Flux_MeshGeometry();
	GenerateBeveledCubeMesh(*g_pxBeveledCubeGeometry);
	g_pxPlungerGeometry = new Flux_MeshGeometry();
	GeneratePlungerMesh(*g_pxPlungerGeometry);
	g_pxTargetRampGeometry = new Flux_MeshGeometry();
	GenerateTargetRampMesh(*g_pxTargetRampGeometry);

	// Load pre-generated merged polyomino meshes from disk
	for (uint32_t u = 0; u < TILEPUZZLE_SHAPE_COUNT; ++u)
	{
		char szPath[ZENITH_MAX_PATH_LENGTH];
		snprintf(szPath, sizeof(szPath), GAME_ASSETS_DIR "Meshes/shape_%u.bin", u);

		Zenith_DataStream xStream;
		xStream.ReadFromFile(szPath);

		g_apxShapeMeshes[u] = new Flux_MeshGeometry();
		ReadShapeMeshFromStream(xStream, *g_apxShapeMeshes[u]);
	}

	// Load material color definitions from disk
	Zenith_Maths::Vector4 axShapeColors[TILEPUZZLE_COLOR_COUNT];
	Zenith_Maths::Vector4 xFloorColor = { 77.f/255.f, 77.f/255.f, 89.f/255.f, 1.f };
	Zenith_Maths::Vector4 xBlockerColor = { 80.f/255.f, 50.f/255.f, 30.f/255.f, 1.f };
	float fHighlightEmissive = 0.5f;

	// Default fallback colors
	axShapeColors[0] = { 230.f/255.f, 60.f/255.f, 60.f/255.f, 1.f };    // Red
	axShapeColors[1] = { 60.f/255.f, 200.f/255.f, 60.f/255.f, 1.f };    // Green
	axShapeColors[2] = { 60.f/255.f, 100.f/255.f, 230.f/255.f, 1.f };   // Blue
	axShapeColors[3] = { 230.f/255.f, 230.f/255.f, 60.f/255.f, 1.f };   // Yellow
	axShapeColors[4] = { 180.f/255.f, 60.f/255.f, 220.f/255.f, 1.f };   // Purple

	{
		Zenith_DataStream xStream;
		xStream.ReadFromFile(GAME_ASSETS_DIR "Materials/materials.bin");

		uint32_t uVersion;
		xStream >> uVersion;

		if (uVersion == 1)
		{
			uint32_t uColorCount;
			xStream >> uColorCount;

			for (uint32_t i = 0; i < uColorCount && i < TILEPUZZLE_COLOR_COUNT; ++i)
			{
				uint8_t uR, uG, uB;
				xStream >> uR;
				xStream >> uG;
				xStream >> uB;
				axShapeColors[i] = {
					static_cast<float>(uR) / 255.f,
					static_cast<float>(uG) / 255.f,
					static_cast<float>(uB) / 255.f,
					1.f
				};
			}

			// Floor color
			{
				uint8_t uR, uG, uB;
				xStream >> uR; xStream >> uG; xStream >> uB;
				xFloorColor = {
					static_cast<float>(uR) / 255.f,
					static_cast<float>(uG) / 255.f,
					static_cast<float>(uB) / 255.f,
					1.f
				};
			}

			// Blocker color
			{
				uint8_t uR, uG, uB;
				xStream >> uR; xStream >> uG; xStream >> uB;
				xBlockerColor = {
					static_cast<float>(uR) / 255.f,
					static_cast<float>(uG) / 255.f,
					static_cast<float>(uB) / 255.f,
					1.f
				};
			}

			// Highlight emissive intensity
			xStream >> fHighlightEmissive;
		}
	}

	// Store loaded highlight emissive intensity globally for behaviours
	g_fHighlightEmissiveIntensity = fHighlightEmissive;

	// Use grid pattern texture with BaseColor for all materials
	Zenith_TextureAsset* pxGridTex = Flux_Graphics::s_pxGridTexture;

	// Create materials with loaded colors
	auto& xRegistry = Zenith_AssetRegistry::Get();
	g_xFloorMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xFloorMaterial.Get()->SetName("TilePuzzleFloor");
	g_xFloorMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xFloorMaterial.Get()->SetBaseColor(xFloorColor);
	g_xFloorMaterial.Get()->SetRoughness(0.8f);
	g_xFloorMaterial.Get()->SetMetallic(0.0f);

	g_xBlockerMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xBlockerMaterial.Get()->SetName("TilePuzzleBlocker");
	g_xBlockerMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xBlockerMaterial.Get()->SetBaseColor(xBlockerColor);
	g_xBlockerMaterial.Get()->SetRoughness(0.9f);
	g_xBlockerMaterial.Get()->SetMetallic(0.0f);

	// Shape materials with loaded colors + per-color PBR variation
	const char* aszShapeColorNames[] = { "Red", "Green", "Blue", "Yellow", "Purple" };
	static constexpr float s_afShapeRoughness[TILEPUZZLE_COLOR_COUNT] = { 0.5f, 0.5f, 0.3f, 0.2f, 0.4f };
	static constexpr float s_afShapeMetallic[TILEPUZZLE_COLOR_COUNT]  = { 0.0f, 0.0f, 0.1f, 0.2f, 0.1f };
	for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
	{
		char szName[64];
		snprintf(szName, sizeof(szName), "TilePuzzleShape%s", aszShapeColorNames[i]);
		g_axShapeMaterials[i].Set(xRegistry.Create<Zenith_MaterialAsset>());
		g_axShapeMaterials[i].Get()->SetName(szName);
		g_axShapeMaterials[i].Get()->SetDiffuseTextureDirectly(pxGridTex);
		g_axShapeMaterials[i].Get()->SetBaseColor(axShapeColors[i]);
		g_axShapeMaterials[i].Get()->SetRoughness(s_afShapeRoughness[i]);
		g_axShapeMaterials[i].Get()->SetMetallic(s_afShapeMetallic[i]);
	}

	// Cat materials (same colors as shapes)
	for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
	{
		char szName[64];
		snprintf(szName, sizeof(szName), "TilePuzzleCat%s", aszShapeColorNames[i]);
		g_axCatMaterials[i].Set(xRegistry.Create<Zenith_MaterialAsset>());
		g_axCatMaterials[i].Get()->SetName(szName);
		g_axCatMaterials[i].Get()->SetDiffuseTextureDirectly(pxGridTex);
		g_axCatMaterials[i].Get()->SetBaseColor(axShapeColors[i]);
		g_axCatMaterials[i].Get()->SetRoughness(0.6f);
		g_axCatMaterials[i].Get()->SetMetallic(0.05f);
	}

#ifndef ZENITH_TOOLS
	// Non-tools: load procedural assets from disk (generated by a prior ZENITH_TOOLS run)
	// In ZENITH_TOOLS builds, these are generated and loaded in Project_InitializeResources()
	LoadProceduralAssets(xRegistry);
#endif

	// Create prefabs for runtime instantiation
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	// Cell prefab (floor tiles)
	{
		Zenith_Entity xCellTemplate(pxSceneData, "CellTemplate");
		g_pxCellPrefab = new Zenith_Prefab();
		g_pxCellPrefab->CreateFromEntity(xCellTemplate, "Cell");
		Zenith_SceneManager::Destroy(xCellTemplate);
	}

	// Shape cube prefab (for multi-cube shapes)
	{
		Zenith_Entity xShapeCubeTemplate(pxSceneData, "ShapeCubeTemplate");
		g_pxShapeCubePrefab = new Zenith_Prefab();
		g_pxShapeCubePrefab->CreateFromEntity(xShapeCubeTemplate, "ShapeCube");
		Zenith_SceneManager::Destroy(xShapeCubeTemplate);
	}

	// Cat prefab (spheres)
	{
		Zenith_Entity xCatTemplate(pxSceneData, "CatTemplate");
		g_pxCatPrefab = new Zenith_Prefab();
		g_pxCatPrefab->CreateFromEntity(xCatTemplate, "Cat");
		Zenith_SceneManager::Destroy(xCatTemplate);
	}

	s_bResourcesInitialized = true;
}

// ============================================================================
// Required Entry Point Functions
// ============================================================================

const char* Project_GetName()
{
	return "TilePuzzle";
}

const char* Project_GetGameAssetsDirectory()
{
	return GAME_ASSETS_DIR;
}

const char* Project_GetGameAssetsDir() { return GAME_ASSETS_DIR; }

void Project_SetGraphicsOptions(Zenith_GraphicsOptions& xOptions)
{
#ifdef ZENITH_WINDOWS
	xOptions.m_uWindowWidth = 720;
	xOptions.m_uWindowHeight = 1280;
#endif
	// On Android, window dimensions come from the native window (SetNativeWindow)
	xOptions.m_bFogEnabled = false;
	xOptions.m_bSSREnabled = false;
	xOptions.m_bSkyboxEnabled = false;
	xOptions.m_xSkyboxColour = Zenith_Maths::Vector3(0.1f, 0.1f, 0.15f);
}

#ifdef ZENITH_INPUT_SIMULATOR
static bool TilePuzzle_HasAutoTestFlag();
#endif

void Project_RegisterScriptBehaviours()
{
	Zenith_SaveData::Initialise("TilePuzzle");
	InitializeTilePuzzleResources();
	TilePuzzle_Behaviour::RegisterBehaviour();
	Pinball_Behaviour::RegisterBehaviour();

#ifdef ZENITH_INPUT_SIMULATOR
	TilePuzzle_AutoTest::RegisterBehaviour();
#endif
}

void Project_Shutdown()
{
	// TilePuzzle has no resources that need explicit cleanup
}

void Project_LoadInitialScene(); // Forward declaration for automation steps

#ifdef ZENITH_TOOLS

// ============================================================================
// Shape Mesh Serialization (tools only)
// ============================================================================
static void WriteShapeMeshToStream(Zenith_DataStream& xStream, const Flux_MeshGeometry& xGeometry)
{
	xStream << xGeometry.m_uNumVerts;
	xStream << xGeometry.m_uNumIndices;

	// Write positions
	xStream.WriteData(xGeometry.m_pxPositions,
		xGeometry.m_uNumVerts * sizeof(Zenith_Maths::Vector3));

	// Write UVs
	xStream.WriteData(xGeometry.m_pxUVs,
		xGeometry.m_uNumVerts * sizeof(Zenith_Maths::Vector2));

	// Write normals
	xStream.WriteData(xGeometry.m_pxNormals,
		xGeometry.m_uNumVerts * sizeof(Zenith_Maths::Vector3));

	// Write tangents
	xStream.WriteData(xGeometry.m_pxTangents,
		xGeometry.m_uNumVerts * sizeof(Zenith_Maths::Vector3));

	// Write bitangents
	xStream.WriteData(xGeometry.m_pxBitangents,
		xGeometry.m_uNumVerts * sizeof(Zenith_Maths::Vector3));

	// Write colors
	xStream.WriteData(xGeometry.m_pxColors,
		xGeometry.m_uNumVerts * sizeof(Zenith_Maths::Vector4));

	// Write indices
	xStream.WriteData(xGeometry.m_puIndices,
		xGeometry.m_uNumIndices * sizeof(Flux_MeshGeometry::IndexType));
}

void Project_InitializeResources()
{
	// ================================================================
	// 1. Validate level files exist
	// ================================================================
	{
		uint32_t uFoundCount = 0;
		for (uint32_t u = 1; u <= 100; ++u)
		{
			char szPath[ZENITH_MAX_PATH_LENGTH];
			snprintf(szPath, sizeof(szPath), GAME_ASSETS_DIR "Levels/level_%04u.tlvl", u);
			if (Zenith_FileAccess::FileExists(szPath))
			{
				uFoundCount++;
			}
			else
			{
				Zenith_Warning(LOG_CATEGORY_GENERAL,
					"Level file missing: %s (generate with TilePuzzleLevelGen tool)", szPath);
			}
		}
		Zenith_Log(LOG_CATEGORY_GENERAL,
			"Level validation: %u/100 level files found", uFoundCount);
	}

	// ================================================================
	// 2. Generate and write shape meshes to disk
	// ================================================================
	{
		std::filesystem::create_directories(GAME_ASSETS_DIR "Meshes");

		for (uint32_t u = 0; u < TILEPUZZLE_SHAPE_COUNT; ++u)
		{
			TilePuzzleShapeDefinition xDef = TilePuzzleShapes::GetShape(
				static_cast<TilePuzzleShapeType>(u), true);

			Flux_MeshGeometry xMesh;
			GenerateShapeMesh(xDef, xMesh);

			Zenith_DataStream xStream;
			WriteShapeMeshToStream(xStream, xMesh);

			char szPath[ZENITH_MAX_PATH_LENGTH];
			snprintf(szPath, sizeof(szPath), GAME_ASSETS_DIR "Meshes/shape_%u.bin", u);
			xStream.WriteToFile(szPath);
		}

		Zenith_Log(LOG_CATEGORY_GENERAL,
			"Wrote %u shape meshes to " GAME_ASSETS_DIR "Meshes/", TILEPUZZLE_SHAPE_COUNT);
	}

	// ================================================================
	// 3. Generate and write material color definitions
	// ================================================================
	{
		std::filesystem::create_directories(GAME_ASSETS_DIR "Materials");

		Zenith_DataStream xStream;

		// Version header
		uint32_t uVersion = 1;
		xStream << uVersion;

		// Shape colors (5 colors, each as 3 uint8_t RGB values)
		uint32_t uColorCount = TILEPUZZLE_COLOR_COUNT;
		xStream << uColorCount;

		// Red
		uint8_t uR = 230; uint8_t uG = 60; uint8_t uB = 60;
		xStream << uR; xStream << uG; xStream << uB;
		// Green
		uR = 60; uG = 200; uB = 60;
		xStream << uR; xStream << uG; xStream << uB;
		// Blue
		uR = 60; uG = 100; uB = 230;
		xStream << uR; xStream << uG; xStream << uB;
		// Yellow
		uR = 230; uG = 230; uB = 60;
		xStream << uR; xStream << uG; xStream << uB;
		// Purple
		uR = 180; uG = 60; uB = 220;
		xStream << uR; xStream << uG; xStream << uB;

		// Floor color
		uR = 77; uG = 77; uB = 89;
		xStream << uR; xStream << uG; xStream << uB;

		// Blocker color
		uR = 80; uG = 50; uB = 30;
		xStream << uR; xStream << uG; xStream << uB;

		// Highlight emissive intensity
		float fHighlightEmissive = 0.5f;
		xStream << fHighlightEmissive;

		xStream.WriteToFile(GAME_ASSETS_DIR "Materials/materials.bin");

		Zenith_Log(LOG_CATEGORY_GENERAL,
			"Wrote material definitions to " GAME_ASSETS_DIR "Materials/materials.bin");
	}

	// ================================================================
	// 4. Generate and write pinball data
	// ================================================================
	Pinball_Behaviour::GenerateAndWriteLayouts();
	Pinball_Behaviour::GenerateAndWriteGateData();

	Zenith_Log(LOG_CATEGORY_GENERAL,
		"Wrote pinball peg layouts and gate data to " GAME_ASSETS_DIR "Pinball/");

	// ================================================================
	// 5. Generate procedural textures (icons, cat faces, gameplay)
	// ================================================================
	TilePuzzle_AssetGen::GenerateAllTextures();

	// ================================================================
	// 6. Generate pinball materials (.zmtrl files)
	// ================================================================
	TilePuzzle_AssetGen::GeneratePinballMaterials();

	// ================================================================
	// 7. Generate particle configs (.zptcl files)
	// ================================================================
	TilePuzzle_AssetGen::GenerateParticleConfigs();

	// ================================================================
	// 8. Load generated procedural assets
	// ================================================================
	LoadProceduralAssets(Zenith_AssetRegistry::Get());
}

// Static string arrays for cat cafe cards (safe for deferred const char* in automation actions)
static const char* s_aszCatCardBgNames[8] = {
	"CatCardBg_0", "CatCardBg_1", "CatCardBg_2", "CatCardBg_3",
	"CatCardBg_4", "CatCardBg_5", "CatCardBg_6", "CatCardBg_7"
};
static const char* s_aszCatCardNames[8] = {
	"CatCard_0", "CatCard_1", "CatCard_2", "CatCard_3",
	"CatCard_4", "CatCard_5", "CatCard_6", "CatCard_7"
};

// Static string arrays for level select grid (safe for deferred const char* in automation actions)
static const char* s_aszLevelBtnNames[20] = {
	"LevelBtn_0", "LevelBtn_1", "LevelBtn_2", "LevelBtn_3", "LevelBtn_4",
	"LevelBtn_5", "LevelBtn_6", "LevelBtn_7", "LevelBtn_8", "LevelBtn_9",
	"LevelBtn_10", "LevelBtn_11", "LevelBtn_12", "LevelBtn_13", "LevelBtn_14",
	"LevelBtn_15", "LevelBtn_16", "LevelBtn_17", "LevelBtn_18", "LevelBtn_19"
};
static const char* s_aszLevelLabels[20] = {
	"1", "2", "3", "4", "5", "6", "7", "8", "9", "10",
	"11", "12", "13", "14", "15", "16", "17", "18", "19", "20"
};

void Project_RegisterEditorAutomationSteps()
{
	// ---- MainMenu scene (build index 0) ----
	Zenith_EditorAutomation::AddStep_CreateScene("MainMenu");
	Zenith_EditorAutomation::AddStep_CreateEntity("GameManager");
	Zenith_EditorAutomation::AddStep_AddCamera();
	Zenith_EditorAutomation::AddStep_SetCameraPosition(0.f, 12.f, 0.f);
	Zenith_EditorAutomation::AddStep_SetCameraPitch(-1.5f);
	Zenith_EditorAutomation::AddStep_SetCameraFOV(glm::radians(45.f));
	Zenith_EditorAutomation::AddStep_SetCameraAspect(9.f / 16.f);
	Zenith_EditorAutomation::AddStep_SetAsMainCamera();
	Zenith_EditorAutomation::AddStep_AddUI();

	// Main menu background
	Zenith_EditorAutomation::AddStep_CreateUIRect("MenuBackground");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuBackground", static_cast<int>(Zenith_UI::AnchorPreset::StretchAll));
	Zenith_EditorAutomation::AddStep_SetUIColor("MenuBackground", 0.06f, 0.06f, 0.12f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIGradientColor("MenuBackground", 0.10f, 0.06f, 0.18f, 1.f);

	// Menu title
	Zenith_EditorAutomation::AddStep_CreateUIText("MenuTitle", "Paws & Pins");
	Zenith_EditorAutomation::AddStep_SetUIFontSize("MenuTitle", 84.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("MenuTitle", 1.f, 1.f, 1.f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("MenuTitle", static_cast<int>(Zenith_UI::TextAlignment::Center));
	Zenith_EditorAutomation::AddStep_SetUITextShadow("MenuTitle", 2.f, 2.f, true);
	Zenith_EditorAutomation::AddStep_SetUITextShadowColor("MenuTitle", 0.f, 0.f, 0.f, 0.5f);

	// Menu subtitle
	Zenith_EditorAutomation::AddStep_CreateUIText("MenuSubtitle", "A Cat Puzzle Game");
	Zenith_EditorAutomation::AddStep_SetUIFontSize("MenuSubtitle", 24.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("MenuSubtitle", 0.6f, 0.6f, 0.8f, 0.7f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("MenuSubtitle", static_cast<int>(Zenith_UI::TextAlignment::Center));
	Zenith_EditorAutomation::AddStep_SetUITextShadow("MenuSubtitle", 1.f, 1.f, true);
	Zenith_EditorAutomation::AddStep_SetUITextShadowColor("MenuSubtitle", 0.f, 0.f, 0.f, 0.3f);

	// Menu layout group (vertical stack: title, subtitle, then buttons)
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("MenuButtonGroup");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuButtonGroup", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("MenuButtonGroup", 0.f, 60.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("MenuButtonGroup", static_cast<int>(Zenith_UI::LayoutDirection::Vertical));
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("MenuButtonGroup", 14.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("MenuButtonGroup", static_cast<int>(Zenith_UI::ChildAlignment::UpperCenter));
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("MenuButtonGroup", true);

	// Continue button
	Zenith_EditorAutomation::AddStep_CreateUIButton("ContinueButton", "Continue");
	Zenith_EditorAutomation::AddStep_SetUISize("ContinueButton", 300.f, 66.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("ContinueButton", 32.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("ContinueButton", 0.18f, 0.30f, 0.55f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("ContinueButton", 0.22f, 0.36f, 0.65f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor("ContinueButton", 0.12f, 0.22f, 0.42f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonCornerRadius("ContinueButton", 12.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonShadow("ContinueButton", 3.f, 3.f, 2.f, true);
	Zenith_EditorAutomation::AddStep_SetUIButtonShadowColor("ContinueButton", 0.f, 0.f, 0.f, 0.3f);
	Zenith_EditorAutomation::AddStep_SetUIButtonBorderColor("ContinueButton", 0.30f, 0.45f, 0.70f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonBorderThickness("ContinueButton", 2.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonTransitionDuration("ContinueButton", 0.12f);
	Zenith_EditorAutomation::AddStep_SetUIButtonTextShadow("ContinueButton", 1.f, 1.f, true);
	Zenith_EditorAutomation::AddStep_SetUIButtonTextShadowColor("ContinueButton", 0.f, 0.f, 0.f, 0.4f);

	// Level Select button
	Zenith_EditorAutomation::AddStep_CreateUIButton("LevelSelectButton", "Level Select");
	Zenith_EditorAutomation::AddStep_SetUISize("LevelSelectButton", 300.f, 66.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("LevelSelectButton", 32.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("LevelSelectButton", 0.18f, 0.30f, 0.55f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("LevelSelectButton", 0.22f, 0.36f, 0.65f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor("LevelSelectButton", 0.12f, 0.22f, 0.42f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonCornerRadius("LevelSelectButton", 12.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonShadow("LevelSelectButton", 3.f, 3.f, 2.f, true);
	Zenith_EditorAutomation::AddStep_SetUIButtonShadowColor("LevelSelectButton", 0.f, 0.f, 0.f, 0.3f);
	Zenith_EditorAutomation::AddStep_SetUIButtonBorderColor("LevelSelectButton", 0.30f, 0.45f, 0.70f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonBorderThickness("LevelSelectButton", 2.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonTransitionDuration("LevelSelectButton", 0.12f);
	Zenith_EditorAutomation::AddStep_SetUIButtonTextShadow("LevelSelectButton", 1.f, 1.f, true);
	Zenith_EditorAutomation::AddStep_SetUIButtonTextShadowColor("LevelSelectButton", 0.f, 0.f, 0.f, 0.4f);

	// New Game button
	Zenith_EditorAutomation::AddStep_CreateUIButton("NewGameButton", "New Game");
	Zenith_EditorAutomation::AddStep_SetUISize("NewGameButton", 300.f, 66.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("NewGameButton", 32.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("NewGameButton", 0.20f, 0.25f, 0.40f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("NewGameButton", 0.28f, 0.33f, 0.50f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor("NewGameButton", 0.14f, 0.18f, 0.30f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonCornerRadius("NewGameButton", 12.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonShadow("NewGameButton", 3.f, 3.f, 2.f, true);
	Zenith_EditorAutomation::AddStep_SetUIButtonShadowColor("NewGameButton", 0.f, 0.f, 0.f, 0.3f);
	Zenith_EditorAutomation::AddStep_SetUIButtonBorderColor("NewGameButton", 0.32f, 0.38f, 0.55f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonBorderThickness("NewGameButton", 2.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonTransitionDuration("NewGameButton", 0.12f);
	Zenith_EditorAutomation::AddStep_SetUIButtonTextShadow("NewGameButton", 1.f, 1.f, true);
	Zenith_EditorAutomation::AddStep_SetUIButtonTextShadowColor("NewGameButton", 0.f, 0.f, 0.f, 0.4f);

	// Pinball button
	Zenith_EditorAutomation::AddStep_CreateUIButton("PinballButton", "Pinball");
	Zenith_EditorAutomation::AddStep_SetUISize("PinballButton", 300.f, 66.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("PinballButton", 32.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("PinballButton", 0.18f, 0.35f, 0.40f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("PinballButton", 0.22f, 0.42f, 0.48f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor("PinballButton", 0.12f, 0.26f, 0.30f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonCornerRadius("PinballButton", 12.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonShadow("PinballButton", 3.f, 3.f, 2.f, true);
	Zenith_EditorAutomation::AddStep_SetUIButtonShadowColor("PinballButton", 0.f, 0.f, 0.f, 0.3f);
	Zenith_EditorAutomation::AddStep_SetUIButtonBorderColor("PinballButton", 0.30f, 0.50f, 0.55f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonBorderThickness("PinballButton", 2.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonTransitionDuration("PinballButton", 0.12f);
	Zenith_EditorAutomation::AddStep_SetUIButtonTextShadow("PinballButton", 1.f, 1.f, true);
	Zenith_EditorAutomation::AddStep_SetUIButtonTextShadowColor("PinballButton", 0.f, 0.f, 0.f, 0.4f);

	// Reset Save button
	Zenith_EditorAutomation::AddStep_CreateUIButton("ResetSaveButton", "Reset Save");
	Zenith_EditorAutomation::AddStep_SetUISize("ResetSaveButton", 300.f, 66.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("ResetSaveButton", 32.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("ResetSaveButton", 0.45f, 0.15f, 0.15f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("ResetSaveButton", 0.55f, 0.20f, 0.20f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor("ResetSaveButton", 0.35f, 0.10f, 0.10f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonCornerRadius("ResetSaveButton", 12.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonShadow("ResetSaveButton", 3.f, 3.f, 2.f, true);
	Zenith_EditorAutomation::AddStep_SetUIButtonShadowColor("ResetSaveButton", 0.f, 0.f, 0.f, 0.3f);
	Zenith_EditorAutomation::AddStep_SetUIButtonBorderColor("ResetSaveButton", 0.60f, 0.25f, 0.25f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonBorderThickness("ResetSaveButton", 2.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonTransitionDuration("ResetSaveButton", 0.12f);
	Zenith_EditorAutomation::AddStep_SetUIButtonTextShadow("ResetSaveButton", 1.f, 1.f, true);
	Zenith_EditorAutomation::AddStep_SetUIButtonTextShadowColor("ResetSaveButton", 0.f, 0.f, 0.f, 0.4f);

	// Cat Cafe button
	Zenith_EditorAutomation::AddStep_CreateUIButton("CatCafeButton", "Cat Cafe");
	Zenith_EditorAutomation::AddStep_SetUISize("CatCafeButton", 300.f, 66.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("CatCafeButton", 32.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("CatCafeButton", 0.45f, 0.22f, 0.35f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("CatCafeButton", 0.55f, 0.28f, 0.42f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor("CatCafeButton", 0.35f, 0.16f, 0.28f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonCornerRadius("CatCafeButton", 12.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonShadow("CatCafeButton", 3.f, 3.f, 2.f, true);
	Zenith_EditorAutomation::AddStep_SetUIButtonShadowColor("CatCafeButton", 0.f, 0.f, 0.f, 0.3f);
	Zenith_EditorAutomation::AddStep_SetUIButtonBorderColor("CatCafeButton", 0.60f, 0.35f, 0.50f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonBorderThickness("CatCafeButton", 2.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonTransitionDuration("CatCafeButton", 0.12f);
	Zenith_EditorAutomation::AddStep_SetUIButtonTextShadow("CatCafeButton", 1.f, 1.f, true);
	Zenith_EditorAutomation::AddStep_SetUIButtonTextShadowColor("CatCafeButton", 0.f, 0.f, 0.f, 0.4f);

	// Daily Puzzle button
	Zenith_EditorAutomation::AddStep_CreateUIButton("DailyPuzzleButton", "Daily Puzzle");
	Zenith_EditorAutomation::AddStep_SetUISize("DailyPuzzleButton", 300.f, 66.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("DailyPuzzleButton", 32.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("DailyPuzzleButton", 0.22f, 0.38f, 0.22f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("DailyPuzzleButton", 0.28f, 0.48f, 0.28f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor("DailyPuzzleButton", 0.16f, 0.28f, 0.16f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonCornerRadius("DailyPuzzleButton", 12.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonShadow("DailyPuzzleButton", 3.f, 3.f, 2.f, true);
	Zenith_EditorAutomation::AddStep_SetUIButtonShadowColor("DailyPuzzleButton", 0.f, 0.f, 0.f, 0.3f);
	Zenith_EditorAutomation::AddStep_SetUIButtonBorderColor("DailyPuzzleButton", 0.35f, 0.55f, 0.35f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonBorderThickness("DailyPuzzleButton", 2.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonTransitionDuration("DailyPuzzleButton", 0.12f);
	Zenith_EditorAutomation::AddStep_SetUIButtonTextShadow("DailyPuzzleButton", 1.f, 1.f, true);
	Zenith_EditorAutomation::AddStep_SetUIButtonTextShadowColor("DailyPuzzleButton", 0.f, 0.f, 0.f, 0.4f);

	Zenith_EditorAutomation::AddStep_AddUIChild("MenuButtonGroup", "MenuTitle");
	Zenith_EditorAutomation::AddStep_AddUIChild("MenuButtonGroup", "MenuSubtitle");
	Zenith_EditorAutomation::AddStep_AddUIChild("MenuButtonGroup", "ContinueButton");
	Zenith_EditorAutomation::AddStep_AddUIChild("MenuButtonGroup", "LevelSelectButton");
	Zenith_EditorAutomation::AddStep_AddUIChild("MenuButtonGroup", "NewGameButton");
	Zenith_EditorAutomation::AddStep_AddUIChild("MenuButtonGroup", "PinballButton");
	Zenith_EditorAutomation::AddStep_AddUIChild("MenuButtonGroup", "ResetSaveButton");
	Zenith_EditorAutomation::AddStep_AddUIChild("MenuButtonGroup", "CatCafeButton");
	Zenith_EditorAutomation::AddStep_AddUIChild("MenuButtonGroup", "DailyPuzzleButton");

	// Top-right counters area (vertical stack: coins pill, stars pill)
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("TopRightCounters");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("TopRightCounters", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	Zenith_EditorAutomation::AddStep_SetUIPosition("TopRightCounters", -14.f, 14.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("TopRightCounters", static_cast<int>(Zenith_UI::LayoutDirection::Vertical));
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("TopRightCounters", 8.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("TopRightCounters", static_cast<int>(Zenith_UI::ChildAlignment::UpperRight));
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("TopRightCounters", true);

	// Coin pill (icon + text with pill background)
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("CoinGroup");
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("CoinGroup", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("CoinGroup", 8.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("CoinGroup", static_cast<int>(Zenith_UI::ChildAlignment::MiddleCenter));
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("CoinGroup", true);
	Zenith_EditorAutomation::AddStep_SetUILayoutPadding("CoinGroup", 10.f, 10.f, 10.f, 2.f);
	Zenith_EditorAutomation::AddStep_SetUIBackgroundColor("CoinGroup", 0.05f, 0.05f, 0.10f, 0.6f);
	Zenith_EditorAutomation::AddStep_SetUIBackgroundCornerRadius("CoinGroup", 16.f);
	Zenith_EditorAutomation::AddStep_SetUIBackgroundBorder("CoinGroup", 0.2f, 0.2f, 0.3f, 1.f);

	Zenith_EditorAutomation::AddStep_CreateUIImage("CoinIcon");
	Zenith_EditorAutomation::AddStep_SetUISize("CoinIcon", 36.f, 36.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("CoinIcon", 1.f, 0.85f, 0.2f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIImageTexturePath("CoinIcon",
		GAME_ASSETS_DIR "Textures/Icons/coin" ZENITH_TEXTURE_EXT);

	Zenith_EditorAutomation::AddStep_CreateUIText("CoinText", "0");
	Zenith_EditorAutomation::AddStep_SetUIFontSize("CoinText", 36.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("CoinText", 1.f, 0.85f, 0.2f, 1.f);

	Zenith_EditorAutomation::AddStep_AddUIChild("CoinGroup", "CoinIcon");
	Zenith_EditorAutomation::AddStep_AddUIChild("CoinGroup", "CoinText");

	// Star pill (icon + text with pill background)
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("StarGroup");
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("StarGroup", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("StarGroup", 8.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("StarGroup", static_cast<int>(Zenith_UI::ChildAlignment::MiddleCenter));
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("StarGroup", true);
	Zenith_EditorAutomation::AddStep_SetUILayoutPadding("StarGroup", 10.f, 10.f, 10.f, 2.f);
	Zenith_EditorAutomation::AddStep_SetUIBackgroundColor("StarGroup", 0.05f, 0.05f, 0.10f, 0.6f);
	Zenith_EditorAutomation::AddStep_SetUIBackgroundCornerRadius("StarGroup", 16.f);
	Zenith_EditorAutomation::AddStep_SetUIBackgroundBorder("StarGroup", 0.2f, 0.2f, 0.3f, 1.f);

	Zenith_EditorAutomation::AddStep_CreateUIImage("StarIcon");
	Zenith_EditorAutomation::AddStep_SetUISize("StarIcon", 36.f, 36.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("StarIcon", 1.f, 0.85f, 0.2f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIImageTexturePath("StarIcon",
		GAME_ASSETS_DIR "Textures/Icons/star_filled" ZENITH_TEXTURE_EXT);

	Zenith_EditorAutomation::AddStep_CreateUIText("TotalStarsText", "0 / 300");
	Zenith_EditorAutomation::AddStep_SetUIFontSize("TotalStarsText", 36.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("TotalStarsText", 1.f, 0.85f, 0.2f, 1.f);

	Zenith_EditorAutomation::AddStep_AddUIChild("StarGroup", "StarIcon");
	Zenith_EditorAutomation::AddStep_AddUIChild("StarGroup", "TotalStarsText");

	Zenith_EditorAutomation::AddStep_AddUIChild("TopRightCounters", "CoinGroup");
	Zenith_EditorAutomation::AddStep_AddUIChild("TopRightCounters", "StarGroup");

	// Lives layout group (top-left of menu): icon + text with pill background
	// Lives area — vertical stack: pill (icon+text), timer, refill button
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("LivesArea");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("LivesArea", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	Zenith_EditorAutomation::AddStep_SetUIPosition("LivesArea", 14.f, 14.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("LivesArea", static_cast<int>(Zenith_UI::LayoutDirection::Vertical));
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("LivesArea", 6.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("LivesArea", true);

	// Lives pill — horizontal icon + text with background
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("LivesGroup");
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("LivesGroup", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("LivesGroup", 8.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("LivesGroup", static_cast<int>(Zenith_UI::ChildAlignment::MiddleCenter));
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("LivesGroup", true);
	Zenith_EditorAutomation::AddStep_SetUILayoutPadding("LivesGroup", 10.f, 10.f, 10.f, 2.f);
	Zenith_EditorAutomation::AddStep_SetUIBackgroundColor("LivesGroup", 0.05f, 0.05f, 0.10f, 0.6f);
	Zenith_EditorAutomation::AddStep_SetUIBackgroundCornerRadius("LivesGroup", 16.f);
	Zenith_EditorAutomation::AddStep_SetUIBackgroundBorder("LivesGroup", 0.2f, 0.2f, 0.3f, 1.f);

	Zenith_EditorAutomation::AddStep_CreateUIImage("HeartIcon");
	Zenith_EditorAutomation::AddStep_SetUISize("HeartIcon", 36.f, 36.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("HeartIcon", 1.f, 0.3f, 0.3f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIImageTexturePath("HeartIcon",
		GAME_ASSETS_DIR "Textures/Icons/heart" ZENITH_TEXTURE_EXT);

	Zenith_EditorAutomation::AddStep_CreateUIText("LivesText", "5/5");
	Zenith_EditorAutomation::AddStep_SetUIFontSize("LivesText", 36.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("LivesText", 1.f, 0.3f, 0.3f, 1.f);

	Zenith_EditorAutomation::AddStep_AddUIChild("LivesGroup", "HeartIcon");
	Zenith_EditorAutomation::AddStep_AddUIChild("LivesGroup", "LivesText");

	// Lives timer text (shown when lives are regenerating)
	Zenith_EditorAutomation::AddStep_CreateUIText("LivesTimerText", "");
	Zenith_EditorAutomation::AddStep_SetUIFontSize("LivesTimerText", 24.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("LivesTimerText", 0.8f, 0.5f, 0.5f, 0.8f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("LivesTimerText", false);

	// Lives Refill button (hidden by default)
	Zenith_EditorAutomation::AddStep_CreateUIButton("RefillLivesButton", "Refill (50)");
	Zenith_EditorAutomation::AddStep_SetUISize("RefillLivesButton", 170.f, 60.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("RefillLivesButton", 22.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("RefillLivesButton", 0.50f, 0.20f, 0.20f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("RefillLivesButton", 0.60f, 0.30f, 0.30f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor("RefillLivesButton", 0.35f, 0.12f, 0.12f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonCornerRadius("RefillLivesButton", 8.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonShadow("RefillLivesButton", 3.f, 3.f, 2.f, true);
	Zenith_EditorAutomation::AddStep_SetUIButtonShadowColor("RefillLivesButton", 0.f, 0.f, 0.f, 0.3f);
	Zenith_EditorAutomation::AddStep_SetUIButtonBorderColor("RefillLivesButton", 0.65f, 0.32f, 0.32f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonBorderThickness("RefillLivesButton", 2.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonTransitionDuration("RefillLivesButton", 0.12f);
	Zenith_EditorAutomation::AddStep_SetUIButtonTextShadow("RefillLivesButton", 1.f, 1.f, true);
	Zenith_EditorAutomation::AddStep_SetUIButtonTextShadowColor("RefillLivesButton", 0.f, 0.f, 0.f, 0.4f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("RefillLivesButton", false);

	// Add all three as children of the vertical LivesArea
	Zenith_EditorAutomation::AddStep_AddUIChild("LivesArea", "LivesGroup");
	Zenith_EditorAutomation::AddStep_AddUIChild("LivesArea", "LivesTimerText");
	Zenith_EditorAutomation::AddStep_AddUIChild("LivesArea", "RefillLivesButton");

	// Hint token pill (icon + text with pill background) — under lives
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("HintTokenGroup");
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("HintTokenGroup", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("HintTokenGroup", 8.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("HintTokenGroup", static_cast<int>(Zenith_UI::ChildAlignment::MiddleCenter));
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("HintTokenGroup", true);
	Zenith_EditorAutomation::AddStep_SetUILayoutPadding("HintTokenGroup", 10.f, 10.f, 10.f, 2.f);
	Zenith_EditorAutomation::AddStep_SetUIBackgroundColor("HintTokenGroup", 0.05f, 0.05f, 0.10f, 0.6f);
	Zenith_EditorAutomation::AddStep_SetUIBackgroundCornerRadius("HintTokenGroup", 16.f);
	Zenith_EditorAutomation::AddStep_SetUIBackgroundBorder("HintTokenGroup", 0.2f, 0.2f, 0.3f, 1.f);

	Zenith_EditorAutomation::AddStep_CreateUIImage("HintTokenIcon");
	Zenith_EditorAutomation::AddStep_SetUISize("HintTokenIcon", 36.f, 36.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("HintTokenIcon", 0.4f, 0.85f, 1.f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIImageTexturePath("HintTokenIcon",
		GAME_ASSETS_DIR "Textures/Icons/hint_token" ZENITH_TEXTURE_EXT);

	Zenith_EditorAutomation::AddStep_CreateUIText("HintTokenText", "0");
	Zenith_EditorAutomation::AddStep_SetUIFontSize("HintTokenText", 36.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("HintTokenText", 0.4f, 0.85f, 1.f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUITextShadow("HintTokenText", 1.f, 1.f, true);

	Zenith_EditorAutomation::AddStep_AddUIChild("HintTokenGroup", "HintTokenIcon");
	Zenith_EditorAutomation::AddStep_AddUIChild("HintTokenGroup", "HintTokenText");
	Zenith_EditorAutomation::AddStep_AddUIChild("LivesArea", "HintTokenGroup");

	// Daily streak (bottom-left) — vertical layout with pill background
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("StreakGroup");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("StreakGroup", static_cast<int>(Zenith_UI::AnchorPreset::BottomLeft));
	Zenith_EditorAutomation::AddStep_SetUIPosition("StreakGroup", 14.f, -14.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("StreakGroup", static_cast<int>(Zenith_UI::LayoutDirection::Vertical));
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("StreakGroup", 2.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("StreakGroup", static_cast<int>(Zenith_UI::ChildAlignment::UpperLeft));
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("StreakGroup", true);
	Zenith_EditorAutomation::AddStep_SetUILayoutPadding("StreakGroup", 10.f, 6.f, 10.f, 6.f);
	Zenith_EditorAutomation::AddStep_SetUIBackgroundColor("StreakGroup", 0.05f, 0.05f, 0.10f, 0.6f);
	Zenith_EditorAutomation::AddStep_SetUIBackgroundCornerRadius("StreakGroup", 14.f);
	Zenith_EditorAutomation::AddStep_SetUIBackgroundBorder("StreakGroup", 0.2f, 0.2f, 0.3f, 1.f);

	Zenith_EditorAutomation::AddStep_CreateUIText("DailyStreakLabel", "Streak");
	Zenith_EditorAutomation::AddStep_SetUIFontSize("DailyStreakLabel", 22.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("DailyStreakLabel", 0.5f, 0.7f, 0.5f, 0.7f);

	Zenith_EditorAutomation::AddStep_CreateUIText("DailyStreakText", "0 days");
	Zenith_EditorAutomation::AddStep_SetUIFontSize("DailyStreakText", 26.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("DailyStreakText", 0.6f, 0.8f, 0.6f, 1.f);

	Zenith_EditorAutomation::AddStep_AddUIChild("StreakGroup", "DailyStreakLabel");
	Zenith_EditorAutomation::AddStep_AddUIChild("StreakGroup", "DailyStreakText");

	// Text shadows on HUD texts
	Zenith_EditorAutomation::AddStep_SetUITextShadow("CoinText", 1.f, 1.f, true);
	Zenith_EditorAutomation::AddStep_SetUITextShadow("LivesText", 1.f, 1.f, true);
	Zenith_EditorAutomation::AddStep_SetUITextShadow("DailyStreakLabel", 1.f, 1.f, true);
	Zenith_EditorAutomation::AddStep_SetUITextShadow("DailyStreakText", 1.f, 1.f, true);

	// Version text
	Zenith_EditorAutomation::AddStep_CreateUIText("VersionText", "v1.0");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("VersionText", static_cast<int>(Zenith_UI::AnchorPreset::BottomCenter));
	Zenith_EditorAutomation::AddStep_SetUIPosition("VersionText", 0.f, -8.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("VersionText", 22.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("VersionText", 0.4f, 0.4f, 0.5f, 0.4f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("VersionText", static_cast<int>(Zenith_UI::TextAlignment::Center));

	// ---- Cat Cafe UI elements (starts hidden) ----

	// Cat Cafe background
	Zenith_EditorAutomation::AddStep_CreateUIRect("CatCafeBg");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("CatCafeBg", static_cast<int>(Zenith_UI::AnchorPreset::StretchAll));
	Zenith_EditorAutomation::AddStep_SetUIColor("CatCafeBg", 0.06f, 0.06f, 0.12f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIGradientColor("CatCafeBg", 0.10f, 0.06f, 0.18f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("CatCafeBg", false);

	// Cat Cafe title
	Zenith_EditorAutomation::AddStep_CreateUIText("CatCafeTitle", "Cat Cafe");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("CatCafeTitle", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	Zenith_EditorAutomation::AddStep_SetUIPosition("CatCafeTitle", 0.f, 30.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("CatCafeTitle", 56.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("CatCafeTitle", static_cast<int>(Zenith_UI::TextAlignment::Center));
	Zenith_EditorAutomation::AddStep_SetUIColor("CatCafeTitle", 1.f, 0.8f, 0.6f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUITextShadow("CatCafeTitle", 2.f, 2.f, true);
	Zenith_EditorAutomation::AddStep_SetUIVisible("CatCafeTitle", false);

	// Cat Cafe count
	Zenith_EditorAutomation::AddStep_CreateUIText("CatCafeCount", "0 / 100 cats rescued");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("CatCafeCount", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	Zenith_EditorAutomation::AddStep_SetUIPosition("CatCafeCount", 0.f, 80.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("CatCafeCount", 32.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("CatCafeCount", static_cast<int>(Zenith_UI::TextAlignment::Center));
	Zenith_EditorAutomation::AddStep_SetUIColor("CatCafeCount", 0.8f, 0.8f, 0.8f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("CatCafeCount", false);

	// Cat collection progress bar (background + fill)
	Zenith_EditorAutomation::AddStep_CreateUIRect("CatProgressBg");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("CatProgressBg", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	Zenith_EditorAutomation::AddStep_SetUIPosition("CatProgressBg", 0.f, 108.f);
	Zenith_EditorAutomation::AddStep_SetUISize("CatProgressBg", 400.f, 16.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("CatProgressBg", 0.15f, 0.15f, 0.2f, 0.8f);
	Zenith_EditorAutomation::AddStep_SetUICornerRadius("CatProgressBg", 6.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("CatProgressBg", false);

	Zenith_EditorAutomation::AddStep_CreateUIRect("CatProgressFill");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("CatProgressFill", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	Zenith_EditorAutomation::AddStep_SetUIPosition("CatProgressFill", 0.f, 108.f);
	Zenith_EditorAutomation::AddStep_SetUISize("CatProgressFill", 400.f, 16.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("CatProgressFill", 1.f, 0.7f, 0.2f, 0.9f);
	Zenith_EditorAutomation::AddStep_SetUICornerRadius("CatProgressFill", 6.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("CatProgressFill", false);

	// Cat cards (8 per page, arranged in 2x4 grid)
	for (uint32_t u = 0; u < 8; ++u)
	{
		float fX = (static_cast<float>(u % 2) - 0.5f) * 280.f;
		float fY = 140.f + static_cast<float>(u / 2) * 120.f;

		// Card background rect
		Zenith_EditorAutomation::AddStep_CreateUIRect(s_aszCatCardBgNames[u]);
		Zenith_EditorAutomation::AddStep_SetUIAnchor(s_aszCatCardBgNames[u], static_cast<int>(Zenith_UI::AnchorPreset::Center));
		Zenith_EditorAutomation::AddStep_SetUIPosition(s_aszCatCardBgNames[u], fX, fY);
		Zenith_EditorAutomation::AddStep_SetUISize(s_aszCatCardBgNames[u], 250.f, 100.f);
		Zenith_EditorAutomation::AddStep_SetUIColor(s_aszCatCardBgNames[u], 0.15f, 0.12f, 0.14f, 1.f);
		Zenith_EditorAutomation::AddStep_SetUICornerRadius(s_aszCatCardBgNames[u], 10.f);
		Zenith_EditorAutomation::AddStep_SetUIShadow(s_aszCatCardBgNames[u], 2.f, 2.f, 2.f, true);
		Zenith_EditorAutomation::AddStep_SetUIRectBorder(s_aszCatCardBgNames[u], 0.25f, 0.20f, 0.24f, 1.f);
		Zenith_EditorAutomation::AddStep_SetUIVisible(s_aszCatCardBgNames[u], false);

		// Card text
		Zenith_EditorAutomation::AddStep_CreateUIText(s_aszCatCardNames[u], "???");
		Zenith_EditorAutomation::AddStep_SetUIAnchor(s_aszCatCardNames[u], static_cast<int>(Zenith_UI::AnchorPreset::Center));
		Zenith_EditorAutomation::AddStep_SetUIPosition(s_aszCatCardNames[u], fX, fY);
		Zenith_EditorAutomation::AddStep_SetUIFontSize(s_aszCatCardNames[u], 22.f);
		Zenith_EditorAutomation::AddStep_SetUIAlignment(s_aszCatCardNames[u], static_cast<int>(Zenith_UI::TextAlignment::Center));
		Zenith_EditorAutomation::AddStep_SetUIColor(s_aszCatCardNames[u], 0.9f, 0.9f, 0.9f, 1.f);
		Zenith_EditorAutomation::AddStep_SetUIVisible(s_aszCatCardNames[u], false);
	}

	// Cat Cafe navigation layout group (< Back >)
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("CatCafeNavGroup");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("CatCafeNavGroup", static_cast<int>(Zenith_UI::AnchorPreset::BottomCenter));
	Zenith_EditorAutomation::AddStep_SetUIPosition("CatCafeNavGroup", 0.f, -40.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("CatCafeNavGroup", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("CatCafeNavGroup", 10.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("CatCafeNavGroup", static_cast<int>(Zenith_UI::ChildAlignment::MiddleCenter));
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("CatCafeNavGroup", true);
	Zenith_EditorAutomation::AddStep_SetUIVisible("CatCafeNavGroup", false);

	Zenith_EditorAutomation::AddStep_CreateUIButton("CatCafePrevPage", "<");
	Zenith_EditorAutomation::AddStep_SetUISize("CatCafePrevPage", 70.f, 70.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("CatCafePrevPage", 28.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("CatCafePrevPage", 0.15f, 0.2f, 0.3f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("CatCafePrevPage", 0.25f, 0.3f, 0.45f, 1.f);

	Zenith_EditorAutomation::AddStep_CreateUIButton("CatCafeBackButton", "Back");
	Zenith_EditorAutomation::AddStep_SetUISize("CatCafeBackButton", 140.f, 70.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("CatCafeBackButton", 24.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("CatCafeBackButton", 0.15f, 0.2f, 0.3f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("CatCafeBackButton", 0.25f, 0.3f, 0.45f, 1.f);

	Zenith_EditorAutomation::AddStep_CreateUIButton("CatCafeNextPage", ">");
	Zenith_EditorAutomation::AddStep_SetUISize("CatCafeNextPage", 70.f, 70.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("CatCafeNextPage", 28.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("CatCafeNextPage", 0.15f, 0.2f, 0.3f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("CatCafeNextPage", 0.25f, 0.3f, 0.45f, 1.f);

	Zenith_EditorAutomation::AddStep_AddUIChild("CatCafeNavGroup", "CatCafePrevPage");
	Zenith_EditorAutomation::AddStep_AddUIChild("CatCafeNavGroup", "CatCafeBackButton");
	Zenith_EditorAutomation::AddStep_AddUIChild("CatCafeNavGroup", "CatCafeNextPage");

	// Level select background (starts hidden)
	Zenith_EditorAutomation::AddStep_CreateUIRect("LevelSelectBg");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("LevelSelectBg", static_cast<int>(Zenith_UI::AnchorPreset::StretchAll));
	Zenith_EditorAutomation::AddStep_SetUIColor("LevelSelectBg", 0.06f, 0.06f, 0.12f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIGradientColor("LevelSelectBg", 0.10f, 0.06f, 0.18f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("LevelSelectBg", false);

	// Level select title (starts hidden)
	Zenith_EditorAutomation::AddStep_CreateUIText("LevelSelectTitle", "Select Level");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("LevelSelectTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("LevelSelectTitle", 0.f, -260.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("LevelSelectTitle", 48.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("LevelSelectTitle", 1.f, 1.f, 1.f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUITextShadow("LevelSelectTitle", 2.f, 2.f, true);
	Zenith_EditorAutomation::AddStep_SetUIVisible("LevelSelectTitle", false);

	// Page text (starts hidden)
	Zenith_EditorAutomation::AddStep_CreateUIText("PageText", "Page 1 / 5");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("PageText", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("PageText", 0.f, -200.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("PageText", 32.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("PageText", 0.7f, 0.7f, 0.8f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("PageText", false);

	// Star progress text (starts hidden)
	Zenith_EditorAutomation::AddStep_CreateUIText("LevelSelectStarProgress", "Stars: 0 / 300");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("LevelSelectStarProgress", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("LevelSelectStarProgress", 0.f, -240.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("LevelSelectStarProgress", 24.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("LevelSelectStarProgress", 1.0f, 0.85f, 0.2f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("LevelSelectStarProgress", false);

	// Level select grid (4x5)
	for (uint32_t u = 0; u < 20; ++u)
	{
		float fX = (static_cast<float>(u % 5) - 2.f) * 105.f;
		float fY = -50.f + (static_cast<float>(u / 5) - 1.5f) * 80.f;
		Zenith_EditorAutomation::AddStep_CreateUIButton(s_aszLevelBtnNames[u], s_aszLevelLabels[u]);
		Zenith_EditorAutomation::AddStep_SetUIAnchor(s_aszLevelBtnNames[u], static_cast<int>(Zenith_UI::AnchorPreset::Center));
		Zenith_EditorAutomation::AddStep_SetUIPosition(s_aszLevelBtnNames[u], fX, fY);
		Zenith_EditorAutomation::AddStep_SetUISize(s_aszLevelBtnNames[u], 100.f, 70.f);
		Zenith_EditorAutomation::AddStep_SetUIButtonFontSize(s_aszLevelBtnNames[u], 24.f);
		Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor(s_aszLevelBtnNames[u], 0.2f, 0.3f, 0.5f, 1.f);
		Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor(s_aszLevelBtnNames[u], 0.3f, 0.4f, 0.6f, 1.f);
		Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor(s_aszLevelBtnNames[u], 0.1f, 0.15f, 0.3f, 1.f);
		Zenith_EditorAutomation::AddStep_SetUIButtonCornerRadius(s_aszLevelBtnNames[u], 8.f);
		Zenith_EditorAutomation::AddStep_SetUIButtonShadow(s_aszLevelBtnNames[u], 2.f, 2.f, 1.f, true);
		Zenith_EditorAutomation::AddStep_SetUIButtonTransitionDuration(s_aszLevelBtnNames[u], 0.10f);
		Zenith_EditorAutomation::AddStep_SetUIVisible(s_aszLevelBtnNames[u], false);
	}

	// Level select navigation layout group (< Back >)
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("LevelSelectNavGroup");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("LevelSelectNavGroup", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("LevelSelectNavGroup", 0.f, 180.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("LevelSelectNavGroup", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("LevelSelectNavGroup", 50.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("LevelSelectNavGroup", static_cast<int>(Zenith_UI::ChildAlignment::MiddleCenter));
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("LevelSelectNavGroup", true);
	Zenith_EditorAutomation::AddStep_SetUIVisible("LevelSelectNavGroup", false);

	// PrevPage button
	Zenith_EditorAutomation::AddStep_CreateUIButton("PrevPageButton", "<");
	Zenith_EditorAutomation::AddStep_SetUISize("PrevPageButton", 100.f, 70.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("PrevPageButton", 28.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("PrevPageButton", 0.15f, 0.2f, 0.3f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("PrevPageButton", 0.25f, 0.3f, 0.45f, 1.f);

	// Back button
	Zenith_EditorAutomation::AddStep_CreateUIButton("BackButton", "Back");
	Zenith_EditorAutomation::AddStep_SetUISize("BackButton", 140.f, 70.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("BackButton", 24.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("BackButton", 0.15f, 0.2f, 0.3f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("BackButton", 0.25f, 0.3f, 0.45f, 1.f);

	// NextPage button
	Zenith_EditorAutomation::AddStep_CreateUIButton("NextPageButton", ">");
	Zenith_EditorAutomation::AddStep_SetUISize("NextPageButton", 100.f, 70.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("NextPageButton", 28.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("NextPageButton", 0.15f, 0.2f, 0.3f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("NextPageButton", 0.25f, 0.3f, 0.45f, 1.f);

	Zenith_EditorAutomation::AddStep_AddUIChild("LevelSelectNavGroup", "PrevPageButton");
	Zenith_EditorAutomation::AddStep_AddUIChild("LevelSelectNavGroup", "BackButton");
	Zenith_EditorAutomation::AddStep_AddUIChild("LevelSelectNavGroup", "NextPageButton");

	// Level select nav buttons focus navigation (horizontal)
	Zenith_EditorAutomation::AddStep_SetUINavigation("PrevPageButton", nullptr, nullptr, nullptr, "BackButton");
	Zenith_EditorAutomation::AddStep_SetUINavigation("BackButton", nullptr, nullptr, "PrevPageButton", "NextPageButton");
	Zenith_EditorAutomation::AddStep_SetUINavigation("NextPageButton", nullptr, nullptr, "BackButton", nullptr);

	// ---- Settings Button (main menu, gear icon) ----
	Zenith_EditorAutomation::AddStep_CreateUIButton("SettingsButton", "Settings");
	Zenith_EditorAutomation::AddStep_SetUISize("SettingsButton", 300.f, 66.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("SettingsButton", 32.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("SettingsButton", 0.25f, 0.25f, 0.3f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("SettingsButton", 0.35f, 0.35f, 0.4f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor("SettingsButton", 0.15f, 0.15f, 0.2f, 1.f);
	Zenith_EditorAutomation::AddStep_AddUIChild("MenuButtonGroup", "SettingsButton");

	// ---- Achievements Button (main menu) ----
	Zenith_EditorAutomation::AddStep_CreateUIButton("AchievementsButton", "Achievements");
	Zenith_EditorAutomation::AddStep_SetUISize("AchievementsButton", 300.f, 66.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("AchievementsButton", 32.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("AchievementsButton", 0.4f, 0.35f, 0.1f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("AchievementsButton", 0.5f, 0.45f, 0.15f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor("AchievementsButton", 0.3f, 0.25f, 0.08f, 1.f);
	Zenith_EditorAutomation::AddStep_AddUIChild("MenuButtonGroup", "AchievementsButton");

	// Main menu focus navigation (vertical)
	Zenith_EditorAutomation::AddStep_SetUINavigation("ContinueButton", nullptr, "LevelSelectButton", nullptr, nullptr);
	Zenith_EditorAutomation::AddStep_SetUINavigation("LevelSelectButton", "ContinueButton", "NewGameButton", nullptr, nullptr);
	Zenith_EditorAutomation::AddStep_SetUINavigation("NewGameButton", "LevelSelectButton", "PinballButton", nullptr, nullptr);
	Zenith_EditorAutomation::AddStep_SetUINavigation("PinballButton", "NewGameButton", "ResetSaveButton", nullptr, nullptr);
	Zenith_EditorAutomation::AddStep_SetUINavigation("ResetSaveButton", "PinballButton", "CatCafeButton", nullptr, nullptr);
	Zenith_EditorAutomation::AddStep_SetUINavigation("CatCafeButton", "ResetSaveButton", "DailyPuzzleButton", nullptr, nullptr);
	Zenith_EditorAutomation::AddStep_SetUINavigation("DailyPuzzleButton", "CatCafeButton", "SettingsButton", nullptr, nullptr);
	Zenith_EditorAutomation::AddStep_SetUINavigation("SettingsButton", "DailyPuzzleButton", "AchievementsButton", nullptr, nullptr);
	Zenith_EditorAutomation::AddStep_SetUINavigation("AchievementsButton", "SettingsButton", nullptr, nullptr, nullptr);

	// ---- Settings Screen UI elements (starts hidden) ----

	// Settings background
	Zenith_EditorAutomation::AddStep_CreateUIRect("SettingsBg");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("SettingsBg", static_cast<int>(Zenith_UI::AnchorPreset::StretchAll));
	Zenith_EditorAutomation::AddStep_SetUIColor("SettingsBg", 0.06f, 0.06f, 0.12f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIGradientColor("SettingsBg", 0.10f, 0.06f, 0.18f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("SettingsBg", false);

	// Settings title
	Zenith_EditorAutomation::AddStep_CreateUIText("SettingsTitle", "Settings");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("SettingsTitle", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	Zenith_EditorAutomation::AddStep_SetUIPosition("SettingsTitle", 0.f, 40.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("SettingsTitle", 56.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("SettingsTitle", static_cast<int>(Zenith_UI::TextAlignment::Center));
	Zenith_EditorAutomation::AddStep_SetUIColor("SettingsTitle", 1.f, 1.f, 1.f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUITextShadow("SettingsTitle", 2.f, 2.f, true);
	Zenith_EditorAutomation::AddStep_SetUIVisible("SettingsTitle", false);

	// Settings toggles
	Zenith_EditorAutomation::AddStep_CreateUIToggle("SettingsSoundBtn", "Sound");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("SettingsSoundBtn", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("SettingsSoundBtn", 0.f, -60.f);
	Zenith_EditorAutomation::AddStep_SetUISize("SettingsSoundBtn", 300.f, 80.f);
	Zenith_EditorAutomation::AddStep_SetUIToggleOnColor("SettingsSoundBtn", 0.2f, 0.4f, 0.3f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIToggleOffColor("SettingsSoundBtn", 0.3f, 0.15f, 0.15f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("SettingsSoundBtn", false);

	Zenith_EditorAutomation::AddStep_CreateUIToggle("SettingsMusicBtn", "Music");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("SettingsMusicBtn", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("SettingsMusicBtn", 0.f, 20.f);
	Zenith_EditorAutomation::AddStep_SetUISize("SettingsMusicBtn", 300.f, 80.f);
	Zenith_EditorAutomation::AddStep_SetUIToggleOnColor("SettingsMusicBtn", 0.2f, 0.4f, 0.3f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIToggleOffColor("SettingsMusicBtn", 0.3f, 0.15f, 0.15f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("SettingsMusicBtn", false);

	Zenith_EditorAutomation::AddStep_CreateUIToggle("SettingsHapticsBtn", "Haptics");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("SettingsHapticsBtn", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("SettingsHapticsBtn", 0.f, 100.f);
	Zenith_EditorAutomation::AddStep_SetUISize("SettingsHapticsBtn", 300.f, 80.f);
	Zenith_EditorAutomation::AddStep_SetUIToggleOnColor("SettingsHapticsBtn", 0.2f, 0.4f, 0.3f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIToggleOffColor("SettingsHapticsBtn", 0.3f, 0.15f, 0.15f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("SettingsHapticsBtn", false);

	// Credits button
	Zenith_EditorAutomation::AddStep_CreateUIButton("SettingsCreditsBtn", "Credits");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("SettingsCreditsBtn", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("SettingsCreditsBtn", 0.f, 170.f);
	Zenith_EditorAutomation::AddStep_SetUISize("SettingsCreditsBtn", 200.f, 65.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("SettingsCreditsBtn", 26.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("SettingsCreditsBtn", 0.2f, 0.2f, 0.3f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("SettingsCreditsBtn", 0.3f, 0.3f, 0.42f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor("SettingsCreditsBtn", 0.14f, 0.14f, 0.22f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("SettingsCreditsBtn", false);

	// Settings back button
	Zenith_EditorAutomation::AddStep_CreateUIButton("SettingsBackBtn", "Back");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("SettingsBackBtn", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("SettingsBackBtn", 0.f, 240.f);
	Zenith_EditorAutomation::AddStep_SetUISize("SettingsBackBtn", 200.f, 70.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("SettingsBackBtn", 28.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("SettingsBackBtn", 0.15f, 0.2f, 0.3f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("SettingsBackBtn", 0.25f, 0.3f, 0.45f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor("SettingsBackBtn", 0.12f, 0.15f, 0.25f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("SettingsBackBtn", false);

	// Settings focus navigation (vertical)
	Zenith_EditorAutomation::AddStep_SetUINavigation("SettingsSoundBtn", nullptr, "SettingsMusicBtn", nullptr, nullptr);
	Zenith_EditorAutomation::AddStep_SetUINavigation("SettingsMusicBtn", "SettingsSoundBtn", "SettingsHapticsBtn", nullptr, nullptr);
	Zenith_EditorAutomation::AddStep_SetUINavigation("SettingsHapticsBtn", "SettingsMusicBtn", "SettingsCreditsBtn", nullptr, nullptr);
	Zenith_EditorAutomation::AddStep_SetUINavigation("SettingsCreditsBtn", "SettingsHapticsBtn", "SettingsBackBtn", nullptr, nullptr);
	Zenith_EditorAutomation::AddStep_SetUINavigation("SettingsBackBtn", "SettingsCreditsBtn", nullptr, nullptr, nullptr);

	// ---- Confirm Dialog Overlay ----
	Zenith_EditorAutomation::AddStep_CreateUIOverlay("ConfirmOverlay");
	Zenith_EditorAutomation::AddStep_SetUIOverlayDimColor("ConfirmOverlay", 0.f, 0.f, 0.f, 0.7f);
	Zenith_EditorAutomation::AddStep_SetUIOverlayContentSize("ConfirmOverlay", 400.f, 220.f);

	Zenith_EditorAutomation::AddStep_CreateUIText("ConfirmText", "Are you sure?");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("ConfirmText", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	Zenith_EditorAutomation::AddStep_SetUIPosition("ConfirmText", 0.f, 30.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("ConfirmText", 28.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("ConfirmText", 1.f, 1.f, 0.9f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("ConfirmText", static_cast<int>(Zenith_UI::TextAlignment::Center));
	Zenith_EditorAutomation::AddStep_AddUIChild("ConfirmOverlay", "ConfirmText");

	Zenith_EditorAutomation::AddStep_CreateUIButton("ConfirmCancelBtn", "Cancel");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("ConfirmCancelBtn", static_cast<int>(Zenith_UI::AnchorPreset::BottomLeft));
	Zenith_EditorAutomation::AddStep_SetUIPosition("ConfirmCancelBtn", 30.f, -20.f);
	Zenith_EditorAutomation::AddStep_SetUISize("ConfirmCancelBtn", 155.f, 65.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("ConfirmCancelBtn", 26.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("ConfirmCancelBtn", 0.25f, 0.25f, 0.3f, 0.9f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("ConfirmCancelBtn", 0.35f, 0.35f, 0.42f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor("ConfirmCancelBtn", 0.18f, 0.18f, 0.22f, 1.f);
	Zenith_EditorAutomation::AddStep_AddUIChild("ConfirmOverlay", "ConfirmCancelBtn");

	Zenith_EditorAutomation::AddStep_CreateUIButton("ConfirmAcceptBtn", "Accept");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("ConfirmAcceptBtn", static_cast<int>(Zenith_UI::AnchorPreset::BottomRight));
	Zenith_EditorAutomation::AddStep_SetUIPosition("ConfirmAcceptBtn", -30.f, -20.f);
	Zenith_EditorAutomation::AddStep_SetUISize("ConfirmAcceptBtn", 155.f, 65.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("ConfirmAcceptBtn", 26.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("ConfirmAcceptBtn", 0.5f, 0.15f, 0.15f, 0.9f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("ConfirmAcceptBtn", 0.65f, 0.25f, 0.25f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor("ConfirmAcceptBtn", 0.35f, 0.1f, 0.1f, 1.f);
	Zenith_EditorAutomation::AddStep_AddUIChild("ConfirmOverlay", "ConfirmAcceptBtn");

	// ---- Credits Overlay ----
	Zenith_EditorAutomation::AddStep_CreateUIOverlay("CreditsOverlay");
	Zenith_EditorAutomation::AddStep_SetUIOverlayDimColor("CreditsOverlay", 0.f, 0.f, 0.f, 0.8f);
	Zenith_EditorAutomation::AddStep_SetUIOverlayContentSize("CreditsOverlay", 350.f, 250.f);

	Zenith_EditorAutomation::AddStep_CreateUIText("CreditsTitleText", "Paws & Pins v1.0");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("CreditsTitleText", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	Zenith_EditorAutomation::AddStep_SetUIPosition("CreditsTitleText", 0.f, 30.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("CreditsTitleText", 32.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("CreditsTitleText", 1.f, 0.9f, 0.3f, 1.f);
	Zenith_EditorAutomation::AddStep_AddUIChild("CreditsOverlay", "CreditsTitleText");

	Zenith_EditorAutomation::AddStep_CreateUIText("CreditsLine1", "Built with Zenith Engine");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("CreditsLine1", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	Zenith_EditorAutomation::AddStep_SetUIPosition("CreditsLine1", 0.f, 80.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("CreditsLine1", 22.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("CreditsLine1", 0.8f, 0.8f, 0.9f, 1.f);
	Zenith_EditorAutomation::AddStep_AddUIChild("CreditsOverlay", "CreditsLine1");

	Zenith_EditorAutomation::AddStep_CreateUIText("CreditsLine2", "A Cat Puzzle Game");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("CreditsLine2", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	Zenith_EditorAutomation::AddStep_SetUIPosition("CreditsLine2", 0.f, 120.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("CreditsLine2", 22.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("CreditsLine2", 0.8f, 0.8f, 0.9f, 1.f);
	Zenith_EditorAutomation::AddStep_AddUIChild("CreditsOverlay", "CreditsLine2");

	Zenith_EditorAutomation::AddStep_CreateUIText("CreditsDismissText", "Tap anywhere to dismiss");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("CreditsDismissText", static_cast<int>(Zenith_UI::AnchorPreset::BottomCenter));
	Zenith_EditorAutomation::AddStep_SetUIPosition("CreditsDismissText", 0.f, -20.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("CreditsDismissText", 22.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("CreditsDismissText", 0.6f, 0.6f, 0.6f, 0.7f);
	Zenith_EditorAutomation::AddStep_AddUIChild("CreditsOverlay", "CreditsDismissText");

	// Script
	Zenith_EditorAutomation::AddStep_AddScript();
	Zenith_EditorAutomation::AddStep_SetBehaviourForSerialization("TilePuzzle_Behaviour");

	Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	Zenith_EditorAutomation::AddStep_UnloadScene();

	// ---- TilePuzzle gameplay scene (build index 1) ----
	Zenith_EditorAutomation::AddStep_CreateScene("TilePuzzle");
	Zenith_EditorAutomation::AddStep_CreateEntity("GameManager");
	Zenith_EditorAutomation::AddStep_AddCamera();
	Zenith_EditorAutomation::AddStep_SetCameraPosition(0.f, 12.f, 0.f);
	Zenith_EditorAutomation::AddStep_SetCameraPitch(-1.5f);
	Zenith_EditorAutomation::AddStep_SetCameraFOV(glm::radians(45.f));
	Zenith_EditorAutomation::AddStep_SetCameraAspect(9.f / 16.f);
	Zenith_EditorAutomation::AddStep_SetAsMainCamera();
	Zenith_EditorAutomation::AddStep_AddUI();

	// ---- Gameplay HUD (GDD section 7.4) ----

	// Top info group: level number, move counter, cats remaining (centered)
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("HUDInfoGroup");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("HUDInfoGroup", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	Zenith_EditorAutomation::AddStep_SetUIPosition("HUDInfoGroup", 0.f, 15.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("HUDInfoGroup", static_cast<int>(Zenith_UI::LayoutDirection::Vertical));
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("HUDInfoGroup", 4.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("HUDInfoGroup", static_cast<int>(Zenith_UI::ChildAlignment::UpperCenter));
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("HUDInfoGroup", true);
	Zenith_EditorAutomation::AddStep_SetUIVisible("HUDInfoGroup", false);

	Zenith_EditorAutomation::AddStep_CreateUIText("LevelText", "Level 1");
	Zenith_EditorAutomation::AddStep_SetUIFontSize("LevelText", 40.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("LevelText", 1.f, 1.f, 1.f, 1.f);

	Zenith_EditorAutomation::AddStep_CreateUIText("MovesText", "Moves: 0");
	Zenith_EditorAutomation::AddStep_SetUIFontSize("MovesText", 28.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("MovesText", 0.6f, 0.8f, 1.f, 1.f);

	Zenith_EditorAutomation::AddStep_CreateUIText("CatsText", "Cats: 0 / 3");
	Zenith_EditorAutomation::AddStep_SetUIFontSize("CatsText", 28.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("CatsText", 0.6f, 0.8f, 1.f, 1.f);

	Zenith_EditorAutomation::AddStep_AddUIChild("HUDInfoGroup", "LevelText");
	Zenith_EditorAutomation::AddStep_AddUIChild("HUDInfoGroup", "MovesText");
	Zenith_EditorAutomation::AddStep_AddUIChild("HUDInfoGroup", "CatsText");

	// Coin display (top-right): icon + coin count
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("HUDCoinGroup");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("HUDCoinGroup", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	Zenith_EditorAutomation::AddStep_SetUIPosition("HUDCoinGroup", -15.f, 15.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("HUDCoinGroup", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("HUDCoinGroup", 6.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("HUDCoinGroup", static_cast<int>(Zenith_UI::ChildAlignment::MiddleCenter));
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("HUDCoinGroup", true);
	Zenith_EditorAutomation::AddStep_SetUIVisible("HUDCoinGroup", false);

	Zenith_EditorAutomation::AddStep_CreateUIImage("HUDCoinIcon");
	Zenith_EditorAutomation::AddStep_SetUISize("HUDCoinIcon", 36.f, 36.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("HUDCoinIcon", 1.f, 0.85f, 0.2f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIImageTexturePath("HUDCoinIcon",
		GAME_ASSETS_DIR "Textures/Icons/coin" ZENITH_TEXTURE_EXT);

	Zenith_EditorAutomation::AddStep_CreateUIText("HUDCoinsText", "0");
	Zenith_EditorAutomation::AddStep_SetUIFontSize("HUDCoinsText", 28.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("HUDCoinsText", 1.f, 0.85f, 0.2f, 1.f);

	Zenith_EditorAutomation::AddStep_AddUIChild("HUDCoinGroup", "HUDCoinIcon");
	Zenith_EditorAutomation::AddStep_AddUIChild("HUDCoinGroup", "HUDCoinsText");

	// Bottom action buttons (GDD: Reset, Undo, Hint, Skip, Menu)
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("HUDButtonGroup");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("HUDButtonGroup", static_cast<int>(Zenith_UI::AnchorPreset::BottomCenter));
	Zenith_EditorAutomation::AddStep_SetUIPosition("HUDButtonGroup", 0.f, -15.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("HUDButtonGroup", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("HUDButtonGroup", 8.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("HUDButtonGroup", static_cast<int>(Zenith_UI::ChildAlignment::MiddleCenter));
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("HUDButtonGroup", true);
	Zenith_EditorAutomation::AddStep_SetUIVisible("HUDButtonGroup", false);

	Zenith_EditorAutomation::AddStep_CreateUIButton("MenuBtn", "Menu");
	Zenith_EditorAutomation::AddStep_SetUISize("MenuBtn", 120.f, 70.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("MenuBtn", 24.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("MenuBtn", 0.2f, 0.25f, 0.35f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("MenuBtn", 0.3f, 0.35f, 0.5f, 1.f);

	Zenith_EditorAutomation::AddStep_CreateUIButton("ResetBtn", "Reset");
	Zenith_EditorAutomation::AddStep_SetUISize("ResetBtn", 120.f, 70.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("ResetBtn", 24.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("ResetBtn", 0.2f, 0.25f, 0.35f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("ResetBtn", 0.3f, 0.35f, 0.5f, 1.f);

	Zenith_EditorAutomation::AddStep_CreateUIButton("UndoBtn", "Undo");
	Zenith_EditorAutomation::AddStep_SetUISize("UndoBtn", 120.f, 70.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("UndoBtn", 24.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("UndoBtn", 0.2f, 0.25f, 0.35f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("UndoBtn", 0.3f, 0.35f, 0.5f, 1.f);

	Zenith_EditorAutomation::AddStep_CreateUIButton("HintBtn", "Hint");
	Zenith_EditorAutomation::AddStep_SetUISize("HintBtn", 120.f, 70.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("HintBtn", 24.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("HintBtn", 0.2f, 0.25f, 0.35f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("HintBtn", 0.3f, 0.35f, 0.5f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonIcon("HintBtn",
		GAME_ASSETS_DIR "Textures/Icons/hint_token" ZENITH_TEXTURE_EXT);
	Zenith_EditorAutomation::AddStep_SetUIButtonIconSize("HintBtn", 28.f, 28.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonIconPlacement("HintBtn",
		static_cast<int>(Zenith_UI::Zenith_UIButton::IconPlacement::LEFT));

	Zenith_EditorAutomation::AddStep_CreateUIButton("SkipBtn", "Skip");
	Zenith_EditorAutomation::AddStep_SetUISize("SkipBtn", 120.f, 70.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("SkipBtn", 24.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("SkipBtn", 0.5f, 0.15f, 0.15f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("SkipBtn", 0.65f, 0.2f, 0.2f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("SkipBtn", false);

	Zenith_EditorAutomation::AddStep_AddUIChild("HUDButtonGroup", "MenuBtn");
	Zenith_EditorAutomation::AddStep_AddUIChild("HUDButtonGroup", "ResetBtn");
	Zenith_EditorAutomation::AddStep_AddUIChild("HUDButtonGroup", "UndoBtn");
	Zenith_EditorAutomation::AddStep_AddUIChild("HUDButtonGroup", "HintBtn");
	Zenith_EditorAutomation::AddStep_AddUIChild("HUDButtonGroup", "SkipBtn");

	// HUD buttons focus navigation (horizontal)
	Zenith_EditorAutomation::AddStep_SetUINavigation("MenuBtn", nullptr, nullptr, nullptr, "ResetBtn");
	Zenith_EditorAutomation::AddStep_SetUINavigation("ResetBtn", nullptr, nullptr, "MenuBtn", "UndoBtn");
	Zenith_EditorAutomation::AddStep_SetUINavigation("UndoBtn", nullptr, nullptr, "ResetBtn", "HintBtn");
	Zenith_EditorAutomation::AddStep_SetUINavigation("HintBtn", nullptr, nullptr, "UndoBtn", "SkipBtn");
	Zenith_EditorAutomation::AddStep_SetUINavigation("SkipBtn", nullptr, nullptr, "HintBtn", nullptr);

	// ---- Confirm Dialog Overlay (gameplay scene) ----
	Zenith_EditorAutomation::AddStep_CreateUIOverlay("ConfirmOverlay");
	Zenith_EditorAutomation::AddStep_SetUIOverlayDimColor("ConfirmOverlay", 0.f, 0.f, 0.f, 0.7f);
	Zenith_EditorAutomation::AddStep_SetUIOverlayContentSize("ConfirmOverlay", 400.f, 220.f);

	Zenith_EditorAutomation::AddStep_CreateUIText("ConfirmText", "Are you sure?");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("ConfirmText", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	Zenith_EditorAutomation::AddStep_SetUIPosition("ConfirmText", 0.f, 30.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("ConfirmText", 28.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("ConfirmText", 1.f, 1.f, 0.9f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("ConfirmText", static_cast<int>(Zenith_UI::TextAlignment::Center));
	Zenith_EditorAutomation::AddStep_AddUIChild("ConfirmOverlay", "ConfirmText");

	Zenith_EditorAutomation::AddStep_CreateUIButton("ConfirmCancelBtn", "Cancel");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("ConfirmCancelBtn", static_cast<int>(Zenith_UI::AnchorPreset::BottomLeft));
	Zenith_EditorAutomation::AddStep_SetUIPosition("ConfirmCancelBtn", 30.f, -20.f);
	Zenith_EditorAutomation::AddStep_SetUISize("ConfirmCancelBtn", 155.f, 65.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("ConfirmCancelBtn", 26.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("ConfirmCancelBtn", 0.25f, 0.25f, 0.3f, 0.9f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("ConfirmCancelBtn", 0.35f, 0.35f, 0.42f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor("ConfirmCancelBtn", 0.18f, 0.18f, 0.22f, 1.f);
	Zenith_EditorAutomation::AddStep_AddUIChild("ConfirmOverlay", "ConfirmCancelBtn");

	Zenith_EditorAutomation::AddStep_CreateUIButton("ConfirmAcceptBtn", "Accept");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("ConfirmAcceptBtn", static_cast<int>(Zenith_UI::AnchorPreset::BottomRight));
	Zenith_EditorAutomation::AddStep_SetUIPosition("ConfirmAcceptBtn", -30.f, -20.f);
	Zenith_EditorAutomation::AddStep_SetUISize("ConfirmAcceptBtn", 155.f, 65.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("ConfirmAcceptBtn", 26.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("ConfirmAcceptBtn", 0.5f, 0.15f, 0.15f, 0.9f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("ConfirmAcceptBtn", 0.65f, 0.25f, 0.25f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor("ConfirmAcceptBtn", 0.35f, 0.1f, 0.1f, 1.f);
	Zenith_EditorAutomation::AddStep_AddUIChild("ConfirmOverlay", "ConfirmAcceptBtn");

	// ---- Victory Overlay UI elements (starts hidden) ----

	// Victory background
	Zenith_EditorAutomation::AddStep_CreateUIRect("VictoryBg");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("VictoryBg", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("VictoryBg", 0.f, 0.f);
	Zenith_EditorAutomation::AddStep_SetUISize("VictoryBg", 500.f, 400.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("VictoryBg", 0.05f, 0.05f, 0.15f, 0.9f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("VictoryBg", false);

	// Victory title
	Zenith_EditorAutomation::AddStep_CreateUIText("VictoryTitle", "Level Complete!");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("VictoryTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("VictoryTitle", 0.f, -120.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("VictoryTitle", 56.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("VictoryTitle", static_cast<int>(Zenith_UI::TextAlignment::Center));
	Zenith_EditorAutomation::AddStep_SetUIColor("VictoryTitle", 1.f, 1.f, 0.5f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("VictoryTitle", false);

	// Victory stars
	Zenith_EditorAutomation::AddStep_CreateUIText("VictoryStars", "- - -");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("VictoryStars", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("VictoryStars", 0.f, -50.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("VictoryStars", 72.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("VictoryStars", static_cast<int>(Zenith_UI::TextAlignment::Center));
	Zenith_EditorAutomation::AddStep_SetUIColor("VictoryStars", 1.f, 0.85f, 0.1f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("VictoryStars", false);

	// Victory content vertical layout group (holds stars, text, coins)
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("VictoryContentGroup");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("VictoryContentGroup", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("VictoryContentGroup", 0.f, 20.f);
	Zenith_EditorAutomation::AddStep_SetUISize("VictoryContentGroup", 460.f, 250.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("VictoryContentGroup", static_cast<int>(Zenith_UI::LayoutDirection::Vertical));
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("VictoryContentGroup", 20.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("VictoryContentGroup", static_cast<int>(Zenith_UI::ChildAlignment::UpperCenter));
	Zenith_EditorAutomation::AddStep_SetUILayoutChildForceExpand("VictoryContentGroup", true, false);
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("VictoryContentGroup", false);
	Zenith_EditorAutomation::AddStep_SetUIVisible("VictoryContentGroup", false);

	// Victory star images layout group (3 stars for rating display)
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("VictoryStarGroup");
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("VictoryStarGroup", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("VictoryStarGroup", 12.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("VictoryStarGroup", static_cast<int>(Zenith_UI::ChildAlignment::MiddleCenter));
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("VictoryStarGroup", true);

	{
		static const char* s_aszVictoryStarNames[] = { "VictoryStar0", "VictoryStar1", "VictoryStar2" };
		for (uint32_t u = 0; u < 3; ++u)
		{
			Zenith_EditorAutomation::AddStep_CreateUIImage(s_aszVictoryStarNames[u]);
			Zenith_EditorAutomation::AddStep_SetUISize(s_aszVictoryStarNames[u], 48.f, 48.f);
			Zenith_EditorAutomation::AddStep_SetUIColor(s_aszVictoryStarNames[u], 1.f, 0.85f, 0.1f, 1.f);
			Zenith_EditorAutomation::AddStep_SetUIImageTexturePath(s_aszVictoryStarNames[u],
				GAME_ASSETS_DIR "Textures/Icons/star_empty" ZENITH_TEXTURE_EXT);
			Zenith_EditorAutomation::AddStep_AddUIChild("VictoryStarGroup", s_aszVictoryStarNames[u]);
		}
	}
	Zenith_EditorAutomation::AddStep_AddUIChild("VictoryContentGroup", "VictoryStarGroup");

	// Victory cat text
	Zenith_EditorAutomation::AddStep_CreateUIText("VictoryCatText", "Cat rescued!");
	Zenith_EditorAutomation::AddStep_SetUIFontSize("VictoryCatText", 36.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("VictoryCatText", static_cast<int>(Zenith_UI::TextAlignment::Center));
	Zenith_EditorAutomation::AddStep_SetUIColor("VictoryCatText", 0.9f, 0.7f, 0.5f, 1.f);
	Zenith_EditorAutomation::AddStep_AddUIChild("VictoryContentGroup", "VictoryCatText");

	// Victory coins text
	Zenith_EditorAutomation::AddStep_CreateUIText("VictoryCoinsText", "+10 coins");
	Zenith_EditorAutomation::AddStep_SetUIFontSize("VictoryCoinsText", 32.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("VictoryCoinsText", static_cast<int>(Zenith_UI::TextAlignment::Center));
	Zenith_EditorAutomation::AddStep_SetUIColor("VictoryCoinsText", 1.f, 0.85f, 0.2f, 1.f);
	Zenith_EditorAutomation::AddStep_AddUIChild("VictoryContentGroup", "VictoryCoinsText");

	// Next Level button (created last so it renders on top of VictoryBg overlay)
	Zenith_EditorAutomation::AddStep_CreateUIButton("NextLevelBtn", "Next Level");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("NextLevelBtn", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("NextLevelBtn", 0.f, 145.f);
	Zenith_EditorAutomation::AddStep_SetUISize("NextLevelBtn", 200.f, 70.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("NextLevelBtn", 28.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("NextLevelBtn", 0.15f, 0.4f, 0.2f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("NextLevelBtn", 0.25f, 0.55f, 0.3f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor("NextLevelBtn", 0.1f, 0.3f, 0.15f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("NextLevelBtn", false);

	// Elimination particle emitter
	Zenith_EditorAutomation::AddStep_CreateEntity("EliminationEmitter");
	Zenith_EditorAutomation::AddStep_AddParticleEmitter();
	Zenith_EditorAutomation::AddStep_SetParticleConfigByName("Elimination");
	Zenith_EditorAutomation::AddStep_SetParticleEmitting(false);

	// Victory confetti particle emitter
	Zenith_EditorAutomation::AddStep_CreateEntity("VictoryConfettiEmitter");
	Zenith_EditorAutomation::AddStep_SetTransformPosition(0.f, 8.f, 0.f);
	Zenith_EditorAutomation::AddStep_AddParticleEmitter();
	Zenith_EditorAutomation::AddStep_SetParticleConfigByName("VictoryConfetti");
	Zenith_EditorAutomation::AddStep_SetParticleEmitting(false);

	// Re-select GameManager for the script step
	Zenith_EditorAutomation::AddStep_SelectEntity("GameManager");

	// Script
	Zenith_EditorAutomation::AddStep_AddScript();
	Zenith_EditorAutomation::AddStep_SetBehaviourForSerialization("TilePuzzle_Behaviour");

	Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/TilePuzzle" ZENITH_SCENE_EXT);
	Zenith_EditorAutomation::AddStep_UnloadScene();

	// ---- Pinball scene (build index 2) ----
	Zenith_EditorAutomation::AddStep_CreateScene("Pinball");
	Zenith_EditorAutomation::AddStep_CreateEntity("PinballManager");
	Zenith_EditorAutomation::AddStep_AddCamera();
	Zenith_EditorAutomation::AddStep_SetCameraPosition(0.f, 4.f, -12.f);
	Zenith_EditorAutomation::AddStep_SetCameraPitch(0.f);
	Zenith_EditorAutomation::AddStep_SetCameraFOV(glm::radians(45.f));
	Zenith_EditorAutomation::AddStep_SetCameraAspect(9.f / 16.f);
	Zenith_EditorAutomation::AddStep_SetAsMainCamera();
	Zenith_EditorAutomation::AddStep_AddUI();

	// Pinball score layout group (score + high score, vertical stack)
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("PinballScoreGroup");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("PinballScoreGroup", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	Zenith_EditorAutomation::AddStep_SetUIPosition("PinballScoreGroup", -30.f, 30.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("PinballScoreGroup", static_cast<int>(Zenith_UI::LayoutDirection::Vertical));
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("PinballScoreGroup", 0.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("PinballScoreGroup", static_cast<int>(Zenith_UI::ChildAlignment::UpperRight));
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("PinballScoreGroup", true);

	Zenith_EditorAutomation::AddStep_CreateUIText("PinballScore", "Score: 0");
	Zenith_EditorAutomation::AddStep_SetUIFontSize("PinballScore", 54.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("PinballScore", 1.f, 1.f, 1.f, 1.f);

	Zenith_EditorAutomation::AddStep_CreateUIText("PinballHighScore", "Total: 0");
	Zenith_EditorAutomation::AddStep_SetUIFontSize("PinballHighScore", 36.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("PinballHighScore", 0.7f, 0.7f, 0.8f, 1.f);

	Zenith_EditorAutomation::AddStep_AddUIChild("PinballScoreGroup", "PinballScore");
	Zenith_EditorAutomation::AddStep_AddUIChild("PinballScoreGroup", "PinballHighScore");

	// Back button
	Zenith_EditorAutomation::AddStep_CreateUIButton("PinballBackBtn", "Menu");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("PinballBackBtn", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	Zenith_EditorAutomation::AddStep_SetUIPosition("PinballBackBtn", 20.f, 20.f);
	Zenith_EditorAutomation::AddStep_SetUISize("PinballBackBtn", 100.f, 70.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("PinballBackBtn", 24.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("PinballBackBtn", 0.2f, 0.25f, 0.35f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("PinballBackBtn", 0.3f, 0.35f, 0.5f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor("PinballBackBtn", 0.12f, 0.15f, 0.25f, 1.f);

	// Launch hint
	Zenith_EditorAutomation::AddStep_CreateUIText("PinballLaunchHint", "Drag plunger to launch");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("PinballLaunchHint", static_cast<int>(Zenith_UI::AnchorPreset::BottomCenter));
	Zenith_EditorAutomation::AddStep_SetUIPosition("PinballLaunchHint", 0.f, -30.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("PinballLaunchHint", static_cast<int>(Zenith_UI::TextAlignment::Center));
	Zenith_EditorAutomation::AddStep_SetUIFontSize("PinballLaunchHint", 36.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("PinballLaunchHint", 0.6f, 0.6f, 0.7f, 1.f);

	// ---- Gate Select Screen (replaces raw SubmitQuad/SubmitText rendering) ----
	Zenith_EditorAutomation::AddStep_CreateUIRect("GateSelectBg");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("GateSelectBg", static_cast<int>(Zenith_UI::AnchorPreset::StretchAll));
	Zenith_EditorAutomation::AddStep_SetUIColor("GateSelectBg", 0.08f, 0.08f, 0.15f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("GateSelectBg", false);

	Zenith_EditorAutomation::AddStep_CreateUIText("GateSelectTitle", "Select Gate");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("GateSelectTitle", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	Zenith_EditorAutomation::AddStep_SetUIPosition("GateSelectTitle", 0.f, 40.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("GateSelectTitle", 28.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("GateSelectTitle", 1.f, 0.9f, 0.5f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("GateSelectTitle", false);

	// Static string arrays for gate buttons (safe for deferred const char* in automation actions)
	static const char* s_aszGateBtnNames[10] = {
		"GateBtn_0", "GateBtn_1", "GateBtn_2", "GateBtn_3", "GateBtn_4",
		"GateBtn_5", "GateBtn_6", "GateBtn_7", "GateBtn_8", "GateBtn_9"
	};
	static const char* s_aszGateBtnLabels[10] = {
		"1", "2", "3", "4", "5", "6", "7", "8", "9", "10"
	};

	// 10 gate buttons (5x2 grid)
	for (int i = 0; i < 10; ++i)
	{
		Zenith_EditorAutomation::AddStep_CreateUIButton(s_aszGateBtnNames[i], s_aszGateBtnLabels[i]);
		Zenith_EditorAutomation::AddStep_SetUIAnchor(s_aszGateBtnNames[i], static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));

		int iCol = i % 5;
		int iRow = i / 5;
		float fBtnW = 80.f;
		float fBtnH = 80.f;
		float fGap = 15.f;
		float fGridW = 5.f * fBtnW + 4.f * fGap;
		float fOffsetX = -fGridW * 0.5f + static_cast<float>(iCol) * (fBtnW + fGap) + fBtnW * 0.5f;
		float fOffsetY = 100.f + static_cast<float>(iRow) * (fBtnH + fGap);

		Zenith_EditorAutomation::AddStep_SetUIPosition(s_aszGateBtnNames[i], fOffsetX, fOffsetY);
		Zenith_EditorAutomation::AddStep_SetUISize(s_aszGateBtnNames[i], fBtnW, fBtnH);
		Zenith_EditorAutomation::AddStep_SetUIButtonFontSize(s_aszGateBtnNames[i], 24.f);
		Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor(s_aszGateBtnNames[i], 0.3f, 0.3f, 0.3f, 1.f);
		Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor(s_aszGateBtnNames[i], 0.4f, 0.4f, 0.5f, 1.f);
		Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor(s_aszGateBtnNames[i], 0.2f, 0.2f, 0.25f, 1.f);
		Zenith_EditorAutomation::AddStep_SetUIVisible(s_aszGateBtnNames[i], false);
	}

	// Freeplay button
	Zenith_EditorAutomation::AddStep_CreateUIButton("GateFreeplayBtn", "Freeplay");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("GateFreeplayBtn", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	Zenith_EditorAutomation::AddStep_SetUIPosition("GateFreeplayBtn", 0.f, 310.f);
	Zenith_EditorAutomation::AddStep_SetUISize("GateFreeplayBtn", 200.f, 65.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("GateFreeplayBtn", 24.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("GateFreeplayBtn", 0.5f, 0.3f, 0.6f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("GateFreeplayBtn", 0.6f, 0.4f, 0.7f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor("GateFreeplayBtn", 0.35f, 0.2f, 0.45f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("GateFreeplayBtn", false);

	// Gate select back button
	Zenith_EditorAutomation::AddStep_CreateUIButton("GateBackBtn", "Back");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("GateBackBtn", static_cast<int>(Zenith_UI::AnchorPreset::BottomCenter));
	Zenith_EditorAutomation::AddStep_SetUIPosition("GateBackBtn", 0.f, -35.f);
	Zenith_EditorAutomation::AddStep_SetUISize("GateBackBtn", 150.f, 65.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("GateBackBtn", 24.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("GateBackBtn", 0.4f, 0.2f, 0.2f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("GateBackBtn", 0.55f, 0.3f, 0.3f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor("GateBackBtn", 0.3f, 0.15f, 0.15f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("GateBackBtn", false);

	// Script
	Zenith_EditorAutomation::AddStep_AddScript();
	Zenith_EditorAutomation::AddStep_SetBehaviourForSerialization("Pinball_Behaviour");

	Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Pinball" ZENITH_SCENE_EXT);
	Zenith_EditorAutomation::AddStep_UnloadScene();

	// ---- Final scene loading ----
	Zenith_EditorAutomation::AddStep_SetInitialSceneLoadCallback(&Project_LoadInitialScene);
	Zenith_EditorAutomation::AddStep_SetLoadingScene(true);
	Zenith_EditorAutomation::AddStep_Custom(&Project_LoadInitialScene);
	Zenith_EditorAutomation::AddStep_SetLoadingScene(false);
}
#endif

void Project_LoadInitialScene()
{
	Zenith_SceneManager::RegisterSceneBuildIndex(0, GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	Zenith_SceneManager::RegisterSceneBuildIndex(1, GAME_ASSETS_DIR "Scenes/TilePuzzle" ZENITH_SCENE_EXT);
	Zenith_SceneManager::RegisterSceneBuildIndex(2, GAME_ASSETS_DIR "Scenes/Pinball" ZENITH_SCENE_EXT);
	Zenith_SceneManager::LoadSceneByIndex(0, SCENE_LOAD_SINGLE);

#ifdef ZENITH_INPUT_SIMULATOR
	if (TilePuzzle_HasAutoTestFlag())
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "====================================");
		Zenith_Log(LOG_CATEGORY_UNITTEST, "TilePuzzle --autotest mode enabled");
		Zenith_Log(LOG_CATEGORY_UNITTEST, "====================================");

		// Create an entity in the active scene with the autotest behaviour
		Zenith_Scene xScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xScene);

		Zenith_Entity xTestEntity(pxSceneData, "AutoTestRunner");
		Zenith_SceneManager::MarkEntityPersistent(xTestEntity);
		Zenith_ScriptComponent& xScript = xTestEntity.AddComponent<Zenith_ScriptComponent>();
		xScript.SetBehaviour<TilePuzzle_AutoTest>();

#ifdef ZENITH_TOOLS
		// Switch editor to Playing mode so SceneManager::Update runs
		// (Stopped mode skips scene updates, preventing OnStart/OnUpdate)
		Zenith_Editor::SetEditorMode(EditorMode::Playing);
#endif
	}
#endif
}

// ============================================================================
// AutoTest Support
// ============================================================================

#ifdef ZENITH_INPUT_SIMULATOR

static bool TilePuzzle_HasAutoTestFlag()
{
#ifdef ZENITH_WINDOWS
	for (int i = 1; i < __argc; ++i)
	{
		if (strcmp(__argv[i], "--autotest") == 0)
			return true;
	}
#endif
	return false;
}

#endif // ZENITH_INPUT_SIMULATOR
