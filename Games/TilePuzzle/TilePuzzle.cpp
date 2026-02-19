#include "Zenith.h"

#include "Core/Zenith_GraphicsOptions.h"
#include "TilePuzzle/Components/TilePuzzle_Types.h"
#include "TilePuzzle/Components/TilePuzzle_Behaviour.h"
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

#include <unordered_set>

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_EditorAutomation.h"
#include "Editor/Zenith_Editor.h"
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
}

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
			// Single point at corner
			PerimeterPoint xPoint;
			xPoint.m_fX = xCorner.m_fCornerX;
			xPoint.m_fZ = xCorner.m_fCornerZ;

			// Use the next edge's outward normal as default
			xPoint.m_fOutX = afEdgeOutX[uCorner];
			xPoint.m_fOutZ = afEdgeOutZ[uCorner];
			xPoint.m_bExterior = xCorner.m_bNextEdgeExterior;
			axPerimeterOut.PushBack(xPoint);
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
	float fMaxY, float fCenterX, float fCenterZ)
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
		float fV = fZ - fCenterZ + 0.5f;

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

static void EmitBottomFace(
	MeshBuilder& xBuilder,
	const Zenith_Vector<PerimeterPoint>& axPerimeter,
	float fMinY, float fCenterX, float fCenterZ)
{
	uint32_t uNumPoints = axPerimeter.GetSize();

	// Center vertex
	uint32_t uCenter = xBuilder.AddVertex(
		{ fCenterX, fMinY, fCenterZ },
		{ 0.5f, 0.5f },
		{ 0.f, -1.f, 0.f },
		{ 1.f, 0.f, 0.f },
		{ 0.f, 0.f, 1.f });

	// Perimeter vertices at full perimeter position (no edge inset on bottom)
	Zenith_Vector<uint32_t> auPerimVerts;
	for (uint32_t u = 0; u < uNumPoints; ++u)
	{
		const PerimeterPoint& xPt = axPerimeter.Get(u);
		float fU = xPt.m_fX - fCenterX + 0.5f;
		float fV = xPt.m_fZ - fCenterZ + 0.5f;

		uint32_t uIdx = xBuilder.AddVertex(
			{ xPt.m_fX, fMinY, xPt.m_fZ },
			{ fU, fV },
			{ 0.f, -1.f, 0.f },
			{ 1.f, 0.f, 0.f },
			{ 0.f, 0.f, 1.f });
		auPerimVerts.PushBack(uIdx);
	}

	// Triangle fan (reversed winding for -Y normal)
	for (uint32_t u = 0; u < uNumPoints; ++u)
	{
		uint32_t uNext = (u + 1) % uNumPoints;
		xBuilder.AddTriangle(uCenter, auPerimVerts.Get(uNext), auPerimVerts.Get(u));
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

		// Emit geometry layers
		EmitTopFace(xBuilder, axPerimeter, fMaxY, fCX, fCZ);
		EmitEdgeRounding(xBuilder, axPerimeter, fMaxY);
		EmitSideWalls(xBuilder, axPerimeter, fMinY, fMaxY);
		EmitBottomFace(xBuilder, axPerimeter, fMinY, fCX, fCZ);
	}

	xBuilder.CopyToGeometry(xGeometryOut);
	xGeometryOut.GenerateLayoutAndVertexData();
	Flux_MemoryManager::InitialiseVertexBuffer(
		xGeometryOut.GetVertexData(), xGeometryOut.GetVertexDataSize(), xGeometryOut.m_xVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(
		xGeometryOut.GetIndexData(), xGeometryOut.GetIndexDataSize(), xGeometryOut.m_xIndexBuffer);
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

	// Generate merged polyomino meshes for each shape type
	for (uint32_t u = 0; u < TILEPUZZLE_SHAPE_COUNT; ++u)
	{
		TilePuzzleShapeDefinition xDef = TilePuzzleShapes::GetShape(
			static_cast<TilePuzzleShapeType>(u), true);
		g_apxShapeMeshes[u] = new Flux_MeshGeometry();
		GenerateShapeMesh(xDef, *g_apxShapeMeshes[u]);
	}

	// Use grid pattern texture with BaseColor for all materials
	Zenith_TextureAsset* pxGridTex = Flux_Graphics::s_pxGridTexture;

	// Create materials with grid texture and BaseColor
	auto& xRegistry = Zenith_AssetRegistry::Get();
	g_xFloorMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xFloorMaterial.Get()->SetName("TilePuzzleFloor");
	g_xFloorMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xFloorMaterial.Get()->SetBaseColor({ 77.f/255.f, 77.f/255.f, 89.f/255.f, 1.f });

	g_xBlockerMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xBlockerMaterial.Get()->SetName("TilePuzzleBlocker");
	g_xBlockerMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xBlockerMaterial.Get()->SetBaseColor({ 80.f/255.f, 50.f/255.f, 30.f/255.f, 1.f });

	// Shape materials with distinct colors
	const char* aszShapeColorNames[] = { "Red", "Green", "Blue", "Yellow" };
	const Zenith_Maths::Vector4 axShapeColors[] = {
		{ 230.f/255.f, 60.f/255.f, 60.f/255.f, 1.f },    // Red
		{ 60.f/255.f, 200.f/255.f, 60.f/255.f, 1.f },    // Green
		{ 60.f/255.f, 100.f/255.f, 230.f/255.f, 1.f },   // Blue
		{ 230.f/255.f, 230.f/255.f, 60.f/255.f, 1.f }    // Yellow
	};
	for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
	{
		char szName[64];
		snprintf(szName, sizeof(szName), "TilePuzzleShape%s", aszShapeColorNames[i]);
		g_axShapeMaterials[i].Set(xRegistry.Create<Zenith_MaterialAsset>());
		g_axShapeMaterials[i].Get()->SetName(szName);
		g_axShapeMaterials[i].Get()->SetDiffuseTextureDirectly(pxGridTex);
		g_axShapeMaterials[i].Get()->SetBaseColor(axShapeColors[i]);
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
	}

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

void Project_SetGraphicsOptions(Zenith_GraphicsOptions& xOptions)
{
	xOptions.m_uWindowWidth = 720;
	xOptions.m_uWindowHeight = 1280;
	xOptions.m_bFogEnabled = false;
	xOptions.m_bSSREnabled = false;
	xOptions.m_bSkyboxEnabled = false;
	xOptions.m_xSkyboxColour = Zenith_Maths::Vector3(0.1f, 0.1f, 0.15f);
}

void Project_RegisterScriptBehaviours()
{
	Zenith_SaveData::Initialise("TilePuzzle");
	InitializeTilePuzzleResources();
	TilePuzzle_Behaviour::RegisterBehaviour();
}

void Project_Shutdown()
{
	// TilePuzzle has no resources that need explicit cleanup
}

void Project_LoadInitialScene(); // Forward declaration for automation steps

#ifdef ZENITH_TOOLS
void Project_InitializeResources()
{
	// All TilePuzzle resources initialized in Project_RegisterScriptBehaviours
}

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
	Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuBackground", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	Zenith_EditorAutomation::AddStep_SetUIPosition("MenuBackground", 0.f, 0.f);
	Zenith_EditorAutomation::AddStep_SetUISize("MenuBackground", 4000.f, 4000.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("MenuBackground", 0.08f, 0.08f, 0.15f, 1.f);

	// Menu title
	Zenith_EditorAutomation::AddStep_CreateUIText("MenuTitle", "TILE PUZZLE");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("MenuTitle", 0.f, -180.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("MenuTitle", 72.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("MenuTitle", 1.f, 1.f, 1.f, 1.f);

	// Continue button
	Zenith_EditorAutomation::AddStep_CreateUIButton("ContinueButton", "Continue");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("ContinueButton", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("ContinueButton", 0.f, -20.f);
	Zenith_EditorAutomation::AddStep_SetUISize("ContinueButton", 300.f, 80.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("ContinueButton", 32.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("ContinueButton", 0.2f, 0.25f, 0.4f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("ContinueButton", 0.3f, 0.35f, 0.55f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor("ContinueButton", 0.12f, 0.15f, 0.25f, 1.f);

	// Level Select button
	Zenith_EditorAutomation::AddStep_CreateUIButton("LevelSelectButton", "Level Select");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("LevelSelectButton", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("LevelSelectButton", 0.f, 80.f);
	Zenith_EditorAutomation::AddStep_SetUISize("LevelSelectButton", 300.f, 80.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("LevelSelectButton", 32.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("LevelSelectButton", 0.2f, 0.25f, 0.4f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("LevelSelectButton", 0.3f, 0.35f, 0.55f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor("LevelSelectButton", 0.12f, 0.15f, 0.25f, 1.f);

	// New Game button
	Zenith_EditorAutomation::AddStep_CreateUIButton("NewGameButton", "New Game");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("NewGameButton", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("NewGameButton", 0.f, 180.f);
	Zenith_EditorAutomation::AddStep_SetUISize("NewGameButton", 300.f, 80.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("NewGameButton", 32.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("NewGameButton", 0.2f, 0.25f, 0.4f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("NewGameButton", 0.3f, 0.35f, 0.55f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor("NewGameButton", 0.12f, 0.15f, 0.25f, 1.f);

	// Level select background (starts hidden)
	Zenith_EditorAutomation::AddStep_CreateUIRect("LevelSelectBg");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("LevelSelectBg", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	Zenith_EditorAutomation::AddStep_SetUIPosition("LevelSelectBg", 0.f, 0.f);
	Zenith_EditorAutomation::AddStep_SetUISize("LevelSelectBg", 4000.f, 4000.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("LevelSelectBg", 0.08f, 0.08f, 0.15f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("LevelSelectBg", false);

	// Level select title (starts hidden)
	Zenith_EditorAutomation::AddStep_CreateUIText("LevelSelectTitle", "Select Level");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("LevelSelectTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("LevelSelectTitle", 0.f, -260.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("LevelSelectTitle", 48.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("LevelSelectTitle", 1.f, 1.f, 1.f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("LevelSelectTitle", false);

	// Page text (starts hidden)
	Zenith_EditorAutomation::AddStep_CreateUIText("PageText", "Page 1 / 5");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("PageText", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("PageText", 0.f, -200.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("PageText", 32.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("PageText", 0.7f, 0.7f, 0.8f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("PageText", false);

	// Level select grid (4x5)
	for (uint32_t u = 0; u < 20; ++u)
	{
		float fX = (static_cast<float>(u % 5) - 2.f) * 105.f;
		float fY = -50.f + (static_cast<float>(u / 5) - 1.5f) * 65.f;
		Zenith_EditorAutomation::AddStep_CreateUIButton(s_aszLevelBtnNames[u], s_aszLevelLabels[u]);
		Zenith_EditorAutomation::AddStep_SetUIAnchor(s_aszLevelBtnNames[u], static_cast<int>(Zenith_UI::AnchorPreset::Center));
		Zenith_EditorAutomation::AddStep_SetUIPosition(s_aszLevelBtnNames[u], fX, fY);
		Zenith_EditorAutomation::AddStep_SetUISize(s_aszLevelBtnNames[u], 90.f, 55.f);
		Zenith_EditorAutomation::AddStep_SetUIButtonFontSize(s_aszLevelBtnNames[u], 20.f);
		Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor(s_aszLevelBtnNames[u], 0.2f, 0.3f, 0.5f, 1.f);
		Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor(s_aszLevelBtnNames[u], 0.3f, 0.4f, 0.6f, 1.f);
		Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor(s_aszLevelBtnNames[u], 0.1f, 0.15f, 0.3f, 1.f);
		Zenith_EditorAutomation::AddStep_SetUIVisible(s_aszLevelBtnNames[u], false);
	}

	// PrevPage button
	Zenith_EditorAutomation::AddStep_CreateUIButton("PrevPageButton", "<");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("PrevPageButton", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("PrevPageButton", -160.f, 180.f);
	Zenith_EditorAutomation::AddStep_SetUISize("PrevPageButton", 100.f, 50.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("PrevPageButton", 28.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("PrevPageButton", 0.15f, 0.2f, 0.3f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("PrevPageButton", 0.25f, 0.3f, 0.45f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("PrevPageButton", false);

	// Back button
	Zenith_EditorAutomation::AddStep_CreateUIButton("BackButton", "Back");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("BackButton", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("BackButton", 0.f, 180.f);
	Zenith_EditorAutomation::AddStep_SetUISize("BackButton", 120.f, 50.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("BackButton", 24.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("BackButton", 0.15f, 0.2f, 0.3f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("BackButton", 0.25f, 0.3f, 0.45f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("BackButton", false);

	// NextPage button
	Zenith_EditorAutomation::AddStep_CreateUIButton("NextPageButton", ">");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("NextPageButton", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("NextPageButton", 160.f, 180.f);
	Zenith_EditorAutomation::AddStep_SetUISize("NextPageButton", 100.f, 50.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("NextPageButton", 28.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("NextPageButton", 0.15f, 0.2f, 0.3f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("NextPageButton", 0.25f, 0.3f, 0.45f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("NextPageButton", false);

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

	// UI layout constants: margin=30, marginTop=30, baseText=15, lineH=24
	// Title (y = 30 + 0 = 30)
	Zenith_EditorAutomation::AddStep_CreateUIText("Title", "TILE PUZZLE");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("Title", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	Zenith_EditorAutomation::AddStep_SetUIPosition("Title", -30.f, 30.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("Title", static_cast<int>(Zenith_UI::TextAlignment::Right));
	Zenith_EditorAutomation::AddStep_SetUIVisible("Title", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("Title", 72.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("Title", 1.f, 1.f, 1.f, 1.f);

	// ControlsHeader (y = 30 + lineH*2 = 78)
	Zenith_EditorAutomation::AddStep_CreateUIText("ControlsHeader", "How to Play:");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("ControlsHeader", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	Zenith_EditorAutomation::AddStep_SetUIPosition("ControlsHeader", -30.f, 78.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("ControlsHeader", static_cast<int>(Zenith_UI::TextAlignment::Right));
	Zenith_EditorAutomation::AddStep_SetUIVisible("ControlsHeader", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("ControlsHeader", 54.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("ControlsHeader", 0.9f, 0.9f, 0.2f, 1.f);

	// MoveInstr (y = 30 + lineH*3 = 102)
	Zenith_EditorAutomation::AddStep_CreateUIText("MoveInstr", "Click+Drag or Arrows: Move");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("MoveInstr", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	Zenith_EditorAutomation::AddStep_SetUIPosition("MoveInstr", -30.f, 102.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("MoveInstr", static_cast<int>(Zenith_UI::TextAlignment::Right));
	Zenith_EditorAutomation::AddStep_SetUIVisible("MoveInstr", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("MoveInstr", 45.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("MoveInstr", 0.8f, 0.8f, 0.8f, 1.f);

	// ResetInstr (y = 30 + lineH*4 = 126)
	Zenith_EditorAutomation::AddStep_CreateUIText("ResetInstr", "R: Reset  Esc: Menu");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("ResetInstr", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	Zenith_EditorAutomation::AddStep_SetUIPosition("ResetInstr", -30.f, 126.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("ResetInstr", static_cast<int>(Zenith_UI::TextAlignment::Right));
	Zenith_EditorAutomation::AddStep_SetUIVisible("ResetInstr", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("ResetInstr", 45.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("ResetInstr", 0.8f, 0.8f, 0.8f, 1.f);

	// GoalHeader (y = 30 + lineH*6 = 174)
	Zenith_EditorAutomation::AddStep_CreateUIText("GoalHeader", "Goal:");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("GoalHeader", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	Zenith_EditorAutomation::AddStep_SetUIPosition("GoalHeader", -30.f, 174.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("GoalHeader", static_cast<int>(Zenith_UI::TextAlignment::Right));
	Zenith_EditorAutomation::AddStep_SetUIVisible("GoalHeader", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("GoalHeader", 54.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("GoalHeader", 0.9f, 0.9f, 0.2f, 1.f);

	// GoalDesc (y = 30 + lineH*7 = 198)
	Zenith_EditorAutomation::AddStep_CreateUIText("GoalDesc", "Match shapes to cats");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("GoalDesc", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	Zenith_EditorAutomation::AddStep_SetUIPosition("GoalDesc", -30.f, 198.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("GoalDesc", static_cast<int>(Zenith_UI::TextAlignment::Right));
	Zenith_EditorAutomation::AddStep_SetUIVisible("GoalDesc", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("GoalDesc", 45.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("GoalDesc", 0.8f, 0.8f, 0.8f, 1.f);

	// Status (y = 30 + lineH*9 = 246)
	Zenith_EditorAutomation::AddStep_CreateUIText("Status", "Level: 1  Moves: 0");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("Status", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	Zenith_EditorAutomation::AddStep_SetUIPosition("Status", -30.f, 246.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("Status", static_cast<int>(Zenith_UI::TextAlignment::Right));
	Zenith_EditorAutomation::AddStep_SetUIVisible("Status", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("Status", 45.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("Status", 0.6f, 0.8f, 1.f, 1.f);

	// Progress (y = 30 + lineH*10 = 270)
	Zenith_EditorAutomation::AddStep_CreateUIText("Progress", "Cats: 0 / 3");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("Progress", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	Zenith_EditorAutomation::AddStep_SetUIPosition("Progress", -30.f, 270.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("Progress", static_cast<int>(Zenith_UI::TextAlignment::Right));
	Zenith_EditorAutomation::AddStep_SetUIVisible("Progress", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("Progress", 45.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("Progress", 0.6f, 0.8f, 1.f, 1.f);

	// WinText (y = 30 + lineH*12 = 318)
	Zenith_EditorAutomation::AddStep_CreateUIText("WinText", "");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("WinText", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	Zenith_EditorAutomation::AddStep_SetUIPosition("WinText", -30.f, 318.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("WinText", static_cast<int>(Zenith_UI::TextAlignment::Right));
	Zenith_EditorAutomation::AddStep_SetUIVisible("WinText", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("WinText", 63.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("WinText", 0.2f, 1.f, 0.2f, 1.f);

	// Reset button
	Zenith_EditorAutomation::AddStep_CreateUIButton("ResetBtn", "Reset");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("ResetBtn", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	Zenith_EditorAutomation::AddStep_SetUIPosition("ResetBtn", 20.f, 20.f);
	Zenith_EditorAutomation::AddStep_SetUISize("ResetBtn", 100.f, 50.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("ResetBtn", 20.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("ResetBtn", 0.2f, 0.25f, 0.35f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("ResetBtn", 0.3f, 0.35f, 0.5f, 1.f);

	// Menu button
	Zenith_EditorAutomation::AddStep_CreateUIButton("MenuBtn", "Menu");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuBtn", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	Zenith_EditorAutomation::AddStep_SetUIPosition("MenuBtn", 20.f, 80.f);
	Zenith_EditorAutomation::AddStep_SetUISize("MenuBtn", 100.f, 50.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("MenuBtn", 20.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("MenuBtn", 0.2f, 0.25f, 0.35f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("MenuBtn", 0.3f, 0.35f, 0.5f, 1.f);

	// Next Level button
	Zenith_EditorAutomation::AddStep_CreateUIButton("NextLevelBtn", "Next Level");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("NextLevelBtn", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("NextLevelBtn", 0.f, 80.f);
	Zenith_EditorAutomation::AddStep_SetUISize("NextLevelBtn", 200.f, 60.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("NextLevelBtn", 28.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("NextLevelBtn", 0.15f, 0.4f, 0.2f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("NextLevelBtn", 0.25f, 0.55f, 0.3f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("NextLevelBtn", false);

	// Script
	Zenith_EditorAutomation::AddStep_AddScript();
	Zenith_EditorAutomation::AddStep_SetBehaviourForSerialization("TilePuzzle_Behaviour");

	Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/TilePuzzle" ZENITH_SCENE_EXT);
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
	Zenith_SceneManager::LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}
