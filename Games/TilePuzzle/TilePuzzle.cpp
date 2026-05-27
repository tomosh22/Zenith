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
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
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
	static TilePuzzleResources g_xResources;
	TilePuzzleResources& Resources() { return g_xResources; }
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
// Depth bias pushes edge rounding strictly below the top face to prevent Z-fighting.
// All geometry is in one draw call, so triangle processing order is undefined.
// With LESS_OR_EQUAL depth test, equal-depth surfaces produce undefined results.
static constexpr float fEDGE_DEPTH_BIAS = 0.02f;

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
// Used by EmitEdgeRounding and EmitSideWalls (but NOT by EmitTopFace, which extends to the
// full perimeter so it always wins the depth test over edge rounding below).
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

	// Perimeter vertices at full perimeter (no inset). The top face must cover the
	// entire cell surface at fMaxY so it always wins the depth test over edge rounding
	// (which is biased below fMaxY by fEDGE_DEPTH_BIAS).
	Zenith_Vector<uint32_t> auPerimVerts;
	for (uint32_t u = 0; u < uNumPoints; ++u)
	{
		const PerimeterPoint& xPt = axPerimeter.Get(u);
		float fX = xPt.m_fX;
		float fZ = xPt.m_fZ;
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
			float fY = fMaxY - (fEDGE_RADIUS * (1.f - fCosAlpha) + fEDGE_DEPTH_BIAS) * fScaleA;

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
			float fY = fMaxY - (fEDGE_RADIUS * (1.f - fCosAlpha) + fEDGE_DEPTH_BIAS) * fScaleB;

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
			xBuilder.AddTriangle(uA0, uB0, uA1);
			xBuilder.AddTriangle(uA1, uB0, uB1);
		}
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

	// Perimeter vertices (no edge inset on bottom face)
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

	// Triangle fan (reversed winding for downward-facing face)
	for (uint32_t u = 0; u < uNumPoints; ++u)
	{
		uint32_t uNext = (u + 1) % uNumPoints;
		xBuilder.AddTriangle(uCenter, auPerimVerts.Get(uNext), auPerimVerts.Get(u));
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

		// Per-point side wall top: accounts for edge rounding + depth bias where present
		float fTopA = fMaxY - (fEDGE_RADIUS + fEDGE_DEPTH_BIAS) * GetEdgeScale(axPerimeter, u);
		float fTopB = fMaxY - (fEDGE_RADIUS + fEDGE_DEPTH_BIAS) * GetEdgeScale(axPerimeter, uNext);

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

		// Emit geometry layers
		EmitTopFace(xBuilder, axPerimeter, fMaxY, fCX, fCZ);
		EmitEdgeRounding(xBuilder, axPerimeter, fMaxY);
		EmitSideWalls(xBuilder, axPerimeter, fMinY, fMaxY);
		EmitBottomFace(xBuilder, axPerimeter, fMinY, fCX, fCZ);
	}

	xBuilder.CopyToGeometry(xGeometryOut);
	xGeometryOut.GenerateLayoutAndVertexData();
	g_xEngine.VulkanMemory().InitialiseVertexBuffer(
		xGeometryOut.GetVertexData(), xGeometryOut.GetVertexDataSize(), xGeometryOut.m_xVertexBuffer);
	g_xEngine.VulkanMemory().InitialiseIndexBuffer(
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
	g_xEngine.VulkanMemory().InitialiseVertexBuffer(
		xGeometryOut.GetVertexData(), xGeometryOut.GetVertexDataSize(), xGeometryOut.m_xVertexBuffer);
	g_xEngine.VulkanMemory().InitialiseIndexBuffer(
		xGeometryOut.GetIndexData(), xGeometryOut.GetIndexDataSize(), xGeometryOut.m_xIndexBuffer);
}

// ============================================================================
// Pinball Custom Mesh Generation
// ============================================================================

static void FinalizeMesh(MeshBuilder& xBuilder, Flux_MeshGeometry& xGeometry)
{
	xBuilder.CopyToGeometry(xGeometry);
	xGeometry.GenerateLayoutAndVertexData();
	g_xEngine.VulkanMemory().InitialiseVertexBuffer(
		xGeometry.GetVertexData(), xGeometry.GetVertexDataSize(), xGeometry.m_xVertexBuffer);
	g_xEngine.VulkanMemory().InitialiseIndexBuffer(
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
	g_xEngine.VulkanMemory().InitialiseVertexBuffer(
		xGeometry.GetVertexData(), xGeometry.GetVertexDataSize(), xGeometry.m_xVertexBuffer);
	g_xEngine.VulkanMemory().InitialiseIndexBuffer(
		xGeometry.GetIndexData(), xGeometry.GetIndexDataSize(), xGeometry.m_xIndexBuffer);

	return true;
}

// ============================================================================
// Cat Cafe Procedural Face Texture Generation
// ============================================================================
static void GenerateCatCafeFaceTextures(
	const Zenith_Maths::Vector4 axColors[TILEPUZZLE_COLOR_COUNT])
{
	using namespace TilePuzzle;

	static constexpr uint32_t uTEX_SIZE = 256;

	uint8_t* pPixels = new uint8_t[uTEX_SIZE * uTEX_SIZE * 4];

	auto SetPixel = [&](int32_t iX, int32_t iY, uint8_t uR, uint8_t uG, uint8_t uB)
	{
		if (iX < 0 || iX >= static_cast<int32_t>(uTEX_SIZE) ||
			iY < 0 || iY >= static_cast<int32_t>(uTEX_SIZE)) return;
		uint8_t* pPx = pPixels + (iY * static_cast<int32_t>(uTEX_SIZE) + iX) * 4;
		pPx[0] = uR; pPx[1] = uG; pPx[2] = uB; pPx[3] = 255;
	};

	auto DrawFilledCircle = [&](int32_t iCX, int32_t iCY, int32_t iRadius,
		uint8_t uR, uint8_t uG, uint8_t uB)
	{
		for (int32_t iY = iCY - iRadius; iY <= iCY + iRadius; ++iY)
			for (int32_t iX = iCX - iRadius; iX <= iCX + iRadius; ++iX)
				if ((iX - iCX) * (iX - iCX) + (iY - iCY) * (iY - iCY) <= iRadius * iRadius)
					SetPixel(iX, iY, uR, uG, uB);
	};

	auto DrawLine = [&](int32_t iX0, int32_t iY0, int32_t iX1, int32_t iY1,
		uint8_t uR, uint8_t uG, uint8_t uB)
	{
		int32_t iDX = abs(iX1 - iX0), iDY = abs(iY1 - iY0);
		int32_t iSX = iX0 < iX1 ? 1 : -1, iSY = iY0 < iY1 ? 1 : -1;
		int32_t iErr = iDX - iDY;
		while (true)
		{
			SetPixel(iX0, iY0, uR, uG, uB);
			if (iX0 == iX1 && iY0 == iY1) break;
			int32_t i2Err = iErr * 2;
			if (i2Err > -iDY) { iErr -= iDY; iX0 += iSX; }
			if (i2Err <  iDX) { iErr += iDX; iY0 += iSY; }
		}
	};

	Flux_SurfaceInfo xSurfaceInfo;
	xSurfaceInfo.m_eFormat = TEXTURE_FORMAT_RGBA8_UNORM;
	xSurfaceInfo.m_uWidth = uTEX_SIZE;
	xSurfaceInfo.m_uHeight = uTEX_SIZE;
	xSurfaceInfo.m_uDepth = 1;
	xSurfaceInfo.m_uNumMips = 1;
	xSurfaceInfo.m_uNumLayers = 1;
	xSurfaceInfo.m_uMemoryFlags = 1 << MEMORY_FLAGS__SHADER_READ;

	for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
	{
		// Fill background with cat's base color
		uint8_t uBR = static_cast<uint8_t>(axColors[i].x * 255.f);
		uint8_t uBG = static_cast<uint8_t>(axColors[i].y * 255.f);
		uint8_t uBB = static_cast<uint8_t>(axColors[i].z * 255.f);
		for (uint32_t uPx = 0; uPx < uTEX_SIZE * uTEX_SIZE; ++uPx)
		{
			pPixels[uPx * 4 + 0] = uBR;
			pPixels[uPx * 4 + 1] = uBG;
			pPixels[uPx * 4 + 2] = uBB;
			pPixels[uPx * 4 + 3] = 255;
		}

		// Eyes: white circles at (95,115) and (161,115), radius 10
		DrawFilledCircle(95, 115, 10, 255, 255, 255);
		DrawFilledCircle(161, 115, 10, 255, 255, 255);
		// Pupils: dark circles slightly up-right inside each eye, radius 5
		DrawFilledCircle(97, 113, 5, 30, 20, 20);
		DrawFilledCircle(163, 113, 5, 30, 20, 20);

		// Nose: pink triangle centered at (128,148)
		for (int32_t iY = 142; iY <= 153; ++iY)
		{
			int32_t iHalfWidth = ((iY - 142) * 6) / 11;
			for (int32_t iX = 128 - iHalfWidth; iX <= 128 + iHalfWidth; ++iX)
				SetPixel(iX, iY, 220, 140, 140);
		}

		// Mouth: two short lines angling down-out from below the nose
		DrawLine(128, 158, 120, 165, 80, 40, 40);
		DrawLine(128, 158, 136, 165, 80, 40, 40);

		// Whiskers: 3 lines per side
		DrawLine(120, 147, 58, 139, 255, 255, 255);
		DrawLine(120, 150, 58, 150, 255, 255, 255);
		DrawLine(120, 153, 58, 161, 255, 255, 255);
		DrawLine(136, 147, 198, 139, 255, 255, 255);
		DrawLine(136, 150, 198, 150, 255, 255, 255);
		DrawLine(136, 153, 198, 161, 255, 255, 255);

		Zenith_TextureAsset* pxFaceTex = Zenith_AssetRegistry::Create<Zenith_TextureAsset>();
		pxFaceTex->CreateFromData(pPixels, xSurfaceInfo);
		pxFaceTex->MarkAsBindless();
		Resources().m_axCatCafeFaceTextures[i].Set(pxFaceTex);
	}

	delete[] pPixels;
}

static void LoadProceduralAssets()
{
	using namespace TilePuzzle;

	// Load procedural textures from .ztxtr files via AssetRegistry (pinned via handles)
	Resources().m_xIconStarFilled.SetPath(GAME_ASSETS_DIR "Textures/Icons/star_filled" ZENITH_TEXTURE_EXT);     Resources().m_xIconStarFilled.Resolve();
	Resources().m_xIconStarEmpty.SetPath(GAME_ASSETS_DIR "Textures/Icons/star_empty" ZENITH_TEXTURE_EXT);       Resources().m_xIconStarEmpty.Resolve();
	Resources().m_xIconCoin.SetPath(GAME_ASSETS_DIR "Textures/Icons/coin" ZENITH_TEXTURE_EXT);                  Resources().m_xIconCoin.Resolve();
	Resources().m_xIconHeart.SetPath(GAME_ASSETS_DIR "Textures/Icons/heart" ZENITH_TEXTURE_EXT);                Resources().m_xIconHeart.Resolve();
	Resources().m_xIconUndo.SetPath(GAME_ASSETS_DIR "Textures/Icons/undo" ZENITH_TEXTURE_EXT);                  Resources().m_xIconUndo.Resolve();
	Resources().m_xIconSkip.SetPath(GAME_ASSETS_DIR "Textures/Icons/skip" ZENITH_TEXTURE_EXT);                  Resources().m_xIconSkip.Resolve();
	Resources().m_xIconLock.SetPath(GAME_ASSETS_DIR "Textures/Icons/lock" ZENITH_TEXTURE_EXT);                  Resources().m_xIconLock.Resolve();
	Resources().m_xIconMenu.SetPath(GAME_ASSETS_DIR "Textures/Icons/menu" ZENITH_TEXTURE_EXT);                  Resources().m_xIconMenu.Resolve();
	Resources().m_xIconBack.SetPath(GAME_ASSETS_DIR "Textures/Icons/back" ZENITH_TEXTURE_EXT);                  Resources().m_xIconBack.Resolve();
	Resources().m_xIconSoundOn.SetPath(GAME_ASSETS_DIR "Textures/Icons/sound_on" ZENITH_TEXTURE_EXT);           Resources().m_xIconSoundOn.Resolve();
	Resources().m_xIconSoundOff.SetPath(GAME_ASSETS_DIR "Textures/Icons/sound_off" ZENITH_TEXTURE_EXT);         Resources().m_xIconSoundOff.Resolve();
	Resources().m_xIconReset.SetPath(GAME_ASSETS_DIR "Textures/Icons/reset" ZENITH_TEXTURE_EXT);                Resources().m_xIconReset.Resolve();
	Resources().m_xIconGear.SetPath(GAME_ASSETS_DIR "Textures/Icons/gear" ZENITH_TEXTURE_EXT);                  Resources().m_xIconGear.Resolve();
	Resources().m_xIconCatSilhouette.SetPath(GAME_ASSETS_DIR "Textures/Icons/cat_silhouette" ZENITH_TEXTURE_EXT); Resources().m_xIconCatSilhouette.Resolve();
	Resources().m_xIconHint.SetPath(GAME_ASSETS_DIR "Textures/Icons/hint" ZENITH_TEXTURE_EXT);                  Resources().m_xIconHint.Resolve();
	Resources().m_xIconHintToken.SetPath(GAME_ASSETS_DIR "Textures/Icons/hint_token" ZENITH_TEXTURE_EXT);       Resources().m_xIconHintToken.Resolve();

	// Load cat face textures
	for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
	{
		char szPath[ZENITH_MAX_PATH_LENGTH];
		snprintf(szPath, sizeof(szPath), GAME_ASSETS_DIR "Textures/CatFaces/cat_face_%u" ZENITH_TEXTURE_EXT, i);
		Resources().m_axCatFaceTextures[i].SetPath(szPath);
	}

	// Load gameplay textures and apply to materials
	Resources().m_xFloorTileTexture.SetPath(GAME_ASSETS_DIR "Textures/Gameplay/floor_tile" ZENITH_TEXTURE_EXT);
	Resources().m_xBlockerTexture.SetPath(GAME_ASSETS_DIR "Textures/Gameplay/blocker" ZENITH_TEXTURE_EXT);

	Resources().m_xFloorMaterial.GetDirect()->SetDiffuseTexture(Resources().m_xFloorTileTexture);
	Resources().m_xBlockerMaterial.GetDirect()->SetDiffuseTexture(Resources().m_xBlockerTexture);
	for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
	{
		Resources().m_axCatMaterials[i].GetDirect()->SetDiffuseTexture(Resources().m_axCatFaceTextures[i]);
	}

	// Load pinball materials from .zmtrl files into MaterialHandles -- the
	// handle's Set() AddRefs the asset so UnloadUnusedAssets (fired on every
	// SCENE_LOAD_SINGLE, e.g. transitioning into the pinball scene at level 10)
	// leaves them alone. Storing the raw pointer would leave the asset at
	// refcount 0 and crash later inside Pinball_Behaviour::CreateMaterials.
	Resources().m_xPinballBallMaterial.Set(
		Zenith_AssetRegistry::Get<Zenith_MaterialAsset>(GAME_ASSETS_DIR "Materials/pinball_ball" ZENITH_MATERIAL_EXT));
	Resources().m_xPinballPegMaterial.Set(
		Zenith_AssetRegistry::Get<Zenith_MaterialAsset>(GAME_ASSETS_DIR "Materials/pinball_peg" ZENITH_MATERIAL_EXT));
	Resources().m_xPinballPegHitMaterial.Set(
		Zenith_AssetRegistry::Get<Zenith_MaterialAsset>(GAME_ASSETS_DIR "Materials/pinball_peg_hit" ZENITH_MATERIAL_EXT));

	// Load pinball PBR textures
	Resources().m_xPinballBumperDiffuseTex.SetPath(GAME_ASSETS_DIR "Textures/Pinball/bumper_diffuse" ZENITH_TEXTURE_EXT);
	Resources().m_xPinballBumperRMTex     .SetPath(GAME_ASSETS_DIR "Textures/Pinball/bumper_rm" ZENITH_TEXTURE_EXT);
	Resources().m_xPinballWallDiffuseTex  .SetPath(GAME_ASSETS_DIR "Textures/Pinball/wall_diffuse" ZENITH_TEXTURE_EXT);
	Resources().m_xPinballWallRMTex       .SetPath(GAME_ASSETS_DIR "Textures/Pinball/wall_rm" ZENITH_TEXTURE_EXT);
	Resources().m_xPinballFloorDiffuseTex .SetPath(GAME_ASSETS_DIR "Textures/Pinball/floor_diffuse" ZENITH_TEXTURE_EXT);
	Resources().m_xPinballFloorRMTex      .SetPath(GAME_ASSETS_DIR "Textures/Pinball/floor_rm" ZENITH_TEXTURE_EXT);
	Resources().m_xPinballPlungerRMTex    .SetPath(GAME_ASSETS_DIR "Textures/Pinball/plunger_rm" ZENITH_TEXTURE_EXT);
	Resources().m_xPinballTargetDiffuseTex.SetPath(GAME_ASSETS_DIR "Textures/Pinball/target_diffuse" ZENITH_TEXTURE_EXT);

	// Load particle configs
	TilePuzzle_AssetGen::LoadParticleConfigs();
}

static void InitializeTilePuzzleResources()
{
	if (s_bResourcesInitialized)
		return;

	using namespace TilePuzzle;

	// Create geometry using registry's cached primitives
	Resources().m_xCubeAsset.Set(Zenith_MeshGeometryAsset::CreateUnitCube());
	Resources().m_pxCubeGeometry = Resources().m_xCubeAsset.GetDirect()->GetGeometry();

	Resources().m_xSphereAsset.Set(Zenith_MeshGeometryAsset::CreateUnitSphere(16));
	Resources().m_pxSphereGeometry = Resources().m_xSphereAsset.GetDirect()->GetGeometry();

	// Generate cat head mesh
	Resources().m_pxCatMeshGeometry = new Flux_MeshGeometry();
	GenerateCatMesh(*Resources().m_pxCatMeshGeometry);

	// Generate pinball custom meshes
	Resources().m_pxBumperGeometry = new Flux_MeshGeometry();
	GenerateBumperMesh(*Resources().m_pxBumperGeometry);
	Resources().m_pxBeveledCubeGeometry = new Flux_MeshGeometry();
	GenerateBeveledCubeMesh(*Resources().m_pxBeveledCubeGeometry);
	Resources().m_pxPlungerGeometry = new Flux_MeshGeometry();
	GeneratePlungerMesh(*Resources().m_pxPlungerGeometry);
	Resources().m_pxTargetRampGeometry = new Flux_MeshGeometry();
	GenerateTargetRampMesh(*Resources().m_pxTargetRampGeometry);

	// Load pre-generated merged polyomino meshes from disk
	for (uint32_t u = 0; u < TILEPUZZLE_SHAPE_COUNT; ++u)
	{
		char szPath[ZENITH_MAX_PATH_LENGTH];
		snprintf(szPath, sizeof(szPath), GAME_ASSETS_DIR "Meshes/shape_%u.bin", u);

		Zenith_DataStream xStream;
		xStream.ReadFromFile(szPath);

		Resources().m_apxShapeMeshes[u] = new Flux_MeshGeometry();
		ReadShapeMeshFromStream(xStream, *Resources().m_apxShapeMeshes[u]);
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
	Resources().m_fHighlightEmissiveIntensity = fHighlightEmissive;

	// Use grid pattern texture with BaseColor for all materials.
	const TextureHandle& xGridTex = g_xEngine.FluxGraphics().m_xGridTexture;

	// Create materials with loaded colors
	Resources().m_xFloorMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xFloorMaterial.GetDirect()->SetName("TilePuzzleFloor");
	Resources().m_xFloorMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xFloorMaterial.GetDirect()->SetBaseColor(xFloorColor);
	Resources().m_xFloorMaterial.GetDirect()->SetRoughness(0.8f);
	Resources().m_xFloorMaterial.GetDirect()->SetMetallic(0.0f);

	Resources().m_xBlockerMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xBlockerMaterial.GetDirect()->SetName("TilePuzzleBlocker");
	Resources().m_xBlockerMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xBlockerMaterial.GetDirect()->SetBaseColor(xBlockerColor);
	Resources().m_xBlockerMaterial.GetDirect()->SetRoughness(0.9f);
	Resources().m_xBlockerMaterial.GetDirect()->SetMetallic(0.0f);

	// Shape materials with loaded colors + per-color PBR variation
	const char* aszShapeColorNames[] = { "Red", "Green", "Blue", "Yellow", "Purple" };
	static constexpr float s_afShapeRoughness[TILEPUZZLE_COLOR_COUNT] = { 0.5f, 0.5f, 0.3f, 0.2f, 0.4f };
	static constexpr float s_afShapeMetallic[TILEPUZZLE_COLOR_COUNT]  = { 0.0f, 0.0f, 0.1f, 0.2f, 0.1f };
	for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
	{
		char szName[64];
		snprintf(szName, sizeof(szName), "TilePuzzleShape%s", aszShapeColorNames[i]);
		Resources().m_axShapeMaterials[i].Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
		Resources().m_axShapeMaterials[i].GetDirect()->SetName(szName);
		Resources().m_axShapeMaterials[i].GetDirect()->SetDiffuseTexture(xGridTex);
		Resources().m_axShapeMaterials[i].GetDirect()->SetBaseColor(axShapeColors[i]);
		Resources().m_axShapeMaterials[i].GetDirect()->SetRoughness(s_afShapeRoughness[i]);
		Resources().m_axShapeMaterials[i].GetDirect()->SetMetallic(s_afShapeMetallic[i]);
	}

	// Cat materials (same colors as shapes)
	for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
	{
		char szName[64];
		snprintf(szName, sizeof(szName), "TilePuzzleCat%s", aszShapeColorNames[i]);
		Resources().m_axCatMaterials[i].Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
		Resources().m_axCatMaterials[i].GetDirect()->SetName(szName);
		Resources().m_axCatMaterials[i].GetDirect()->SetDiffuseTexture(xGridTex);
		Resources().m_axCatMaterials[i].GetDirect()->SetBaseColor(axShapeColors[i]);
		Resources().m_axCatMaterials[i].GetDirect()->SetRoughness(0.6f);
		Resources().m_axCatMaterials[i].GetDirect()->SetMetallic(0.05f);
	}

	// Cat cafe display materials (programmatic face textures on cat head mesh)
	GenerateCatCafeFaceTextures(axShapeColors);
	for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
	{
		char szName[64];
		snprintf(szName, sizeof(szName), "TilePuzzleCatCafeDisplay%s", aszShapeColorNames[i]);
		Resources().m_axCatCafeDisplayMaterials[i].Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
		Resources().m_axCatCafeDisplayMaterials[i].GetDirect()->SetName(szName);
		Resources().m_axCatCafeDisplayMaterials[i].GetDirect()->SetDiffuseTexture(Resources().m_axCatCafeFaceTextures[i]);
		Resources().m_axCatCafeDisplayMaterials[i].GetDirect()->SetBaseColor(Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f));
		Resources().m_axCatCafeDisplayMaterials[i].GetDirect()->SetRoughness(0.6f);
		Resources().m_axCatCafeDisplayMaterials[i].GetDirect()->SetMetallic(0.05f);
	}

#ifndef ZENITH_TOOLS
	// Non-tools: load procedural assets from disk (generated by a prior ZENITH_TOOLS run)
	// In ZENITH_TOOLS builds, these are generated and loaded in Project_InitializeResources()
	LoadProceduralAssets();
#endif

	// Create prefabs for runtime instantiation.
	// Use the persistent scene here: InitializeResources runs before the initial scene
	// loads, and (post-A6) GetActiveScene returns INVALID until that happens. These
	// template entities are destroyed immediately after capture into the Zenith_Prefab.
	Zenith_Scene xActiveScene = g_xEngine.SceneRegistry().GetPersistentScene();
	Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xActiveScene);

	// Cell prefab (floor tiles)
	{
		Zenith_Entity xCellTemplate(pxSceneData, "CellTemplate");
		Zenith_Prefab* pxCell = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxCell->CreateFromEntity(xCellTemplate, "Cell");
		Resources().m_xCellPrefab.Set(pxCell);
		Zenith_SceneEntityOwnership::Destroy(xCellTemplate);
	}

	// Shape cube prefab (for multi-cube shapes)
	{
		Zenith_Entity xShapeCubeTemplate(pxSceneData, "ShapeCubeTemplate");
		Zenith_Prefab* pxShapeCube = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxShapeCube->CreateFromEntity(xShapeCubeTemplate, "ShapeCube");
		Resources().m_xShapeCubePrefab.Set(pxShapeCube);
		Zenith_SceneEntityOwnership::Destroy(xShapeCubeTemplate);
	}

	// Cat prefab (spheres)
	{
		Zenith_Entity xCatTemplate(pxSceneData, "CatTemplate");
		Zenith_Prefab* pxCat = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxCat->CreateFromEntity(xCatTemplate, "Cat");
		Resources().m_xCatPrefab.Set(pxCat);
		Zenith_SceneEntityOwnership::Destroy(xCatTemplate);
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
	// TilePuzzle_Behaviour, Pinball_Behaviour, and TilePuzzle_AutoTest auto-register
	// via ZENITH_BEHAVIOUR_TYPE_NAME (no explicit calls needed).
}

void Project_Shutdown()
{
	using namespace TilePuzzle;
	// Drop asset handle refs before Zenith_AssetRegistry::Shutdown teardown.
	Resources().m_xCubeAsset.Clear();
	Resources().m_xSphereAsset.Clear();
	Resources().m_xFloorMaterial.Clear();
	Resources().m_xBlockerMaterial.Clear();
	for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
	{
		Resources().m_axShapeMaterials[i].Clear();
		Resources().m_axCatMaterials[i].Clear();
		Resources().m_axCatCafeDisplayMaterials[i].Clear();
		Resources().m_axCatCafeFaceTextures[i].Clear();
		Resources().m_axCatFaceTextures[i].Clear();
	}
	Resources().m_xCellPrefab.Clear();
	Resources().m_xShapeCubePrefab.Clear();
	Resources().m_xCatPrefab.Clear();
	Resources().m_xIconStarFilled.Clear();
	Resources().m_xIconStarEmpty.Clear();
	Resources().m_xIconCoin.Clear();
	Resources().m_xIconHeart.Clear();
	Resources().m_xIconUndo.Clear();
	Resources().m_xIconSkip.Clear();
	Resources().m_xIconLock.Clear();
	Resources().m_xIconMenu.Clear();
	Resources().m_xIconBack.Clear();
	Resources().m_xIconSoundOn.Clear();
	Resources().m_xIconSoundOff.Clear();
	Resources().m_xIconReset.Clear();
	Resources().m_xIconGear.Clear();
	Resources().m_xIconCatSilhouette.Clear();
	Resources().m_xIconHint.Clear();
	Resources().m_xIconHintToken.Clear();
	Resources().m_xFloorTileTexture.Clear();
	Resources().m_xBlockerTexture.Clear();
	Resources().m_xPinballBumperDiffuseTex.Clear();
	Resources().m_xPinballBumperRMTex.Clear();
	Resources().m_xPinballWallDiffuseTex.Clear();
	Resources().m_xPinballWallRMTex.Clear();
	Resources().m_xPinballFloorDiffuseTex.Clear();
	Resources().m_xPinballFloorRMTex.Clear();
	Resources().m_xPinballPlungerRMTex.Clear();
	Resources().m_xPinballTargetDiffuseTex.Clear();
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
	LoadProceduralAssets();
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

namespace TilePuzzleUI
{
	// Main menu buttons (Continue, Level Select, Pinball, etc.)
	static constexpr float fMENU_BTN_W = 360.f;
	static constexpr float fMENU_BTN_H = 88.f;
	static constexpr float fMENU_BTN_FONT = 40.f;
	static constexpr float fMENU_BTN_SPACING = 18.f;
	static constexpr float fMENU_TITLE_FONT = 96.f;
	static constexpr float fMENU_SUBTITLE_FONT = 34.f;

	// Navigation/action buttons (Back, PrevPage, NextPage, etc.)
	static constexpr float fNAV_BTN_W = 120.f;
	static constexpr float fNAV_BTN_H = 88.f;
	static constexpr float fNAV_BTN_FONT = 36.f;
	static constexpr float fNAV_BACK_BTN_W = 170.f;
	static constexpr float fNAV_BACK_BTN_FONT = 34.f;

	// HUD action buttons (Menu, Reset, Undo, Hint, Skip)
	static constexpr float fHUD_BTN_W = 150.f;
	static constexpr float fHUD_BTN_H = 88.f;
	static constexpr float fHUD_BTN_FONT = 32.f;
	static constexpr float fHUD_BTN_ICON = 36.f;

	// Level select grid buttons
	static constexpr float fLEVEL_BTN_W = 120.f;
	static constexpr float fLEVEL_BTN_H = 88.f;
	static constexpr float fLEVEL_BTN_FONT = 32.f;
	static constexpr float fLEVEL_GRID_X_SPACING = 130.f;
	static constexpr float fLEVEL_GRID_Y_SPACING = 98.f;
	static constexpr float fLEVEL_NAV_Y = 240.f;

	// Icon sizes (coins, stars, hearts, hint tokens)
	static constexpr float fICON_SIZE = 48.f;

	// HUD info text
	static constexpr float fHUD_TITLE_FONT = 52.f;
	static constexpr float fHUD_BODY_FONT = 38.f;

	// Screen titles
	static constexpr float fSCREEN_TITLE_FONT = 68.f;
	static constexpr float fSCREEN_BODY_FONT = 42.f;

	// Pill/counter text
	static constexpr float fPILL_TEXT_FONT = 44.f;

	// Small labels and timer text
	static constexpr float fSMALL_LABEL_FONT = 32.f;
	static constexpr float fSTREAK_LABEL_FONT = 32.f;
	static constexpr float fSTREAK_VALUE_FONT = 36.f;
	static constexpr float fVERSION_FONT = 30.f;

	// Refill button
	static constexpr float fREFILL_BTN_W = 220.f;
	static constexpr float fREFILL_BTN_H = 80.f;
	static constexpr float fREFILL_BTN_FONT = 32.f;

	// Settings toggles
	static constexpr float fTOGGLE_W = 380.f;
	static constexpr float fTOGGLE_H = 100.f;

	// Settings buttons
	static constexpr float fSETTINGS_BTN_W = 260.f;
	static constexpr float fSETTINGS_BTN_H = 85.f;
	static constexpr float fSETTINGS_BTN_FONT = 36.f;

	// Confirm/Credits overlay
	static constexpr float fCONFIRM_OVERLAY_W = 500.f;
	static constexpr float fCONFIRM_OVERLAY_H = 280.f;
	static constexpr float fCONFIRM_TEXT_FONT = 38.f;
	static constexpr float fOVERLAY_BTN_W = 200.f;
	static constexpr float fOVERLAY_BTN_H = 85.f;
	static constexpr float fOVERLAY_BTN_FONT = 34.f;
	static constexpr float fCREDITS_OVERLAY_W = 450.f;
	static constexpr float fCREDITS_OVERLAY_H = 320.f;
	static constexpr float fCREDITS_TITLE_FONT = 42.f;
	static constexpr float fCREDITS_LINE_FONT = 32.f;

	// Level select title
	static constexpr float fLEVEL_SELECT_TITLE_FONT = 60.f;
	static constexpr float fPAGE_TEXT_FONT = 42.f;
	static constexpr float fSTAR_PROGRESS_FONT = 34.f;

	// Cat cafe
	static constexpr float fCAT_CARD_W = 320.f;
	static constexpr float fCAT_CARD_H = 130.f;
	static constexpr float fCAT_CARD_FONT = 32.f;
	static constexpr float fCAT_CARD_X_SPACING = 360.f;
	static constexpr float fCAT_CARD_Y_START = 175.f;
	static constexpr float fCAT_CARD_Y_SPACING = 150.f;
	static constexpr float fCAT_PROGRESS_W = 500.f;
	static constexpr float fCAT_PROGRESS_H = 24.f;
	static constexpr float fCAT_PROGRESS_Y = 135.f;
	static constexpr float fCAFE_COUNT_FONT = 42.f;
	static constexpr float fCAFE_COUNT_Y = 95.f;
	static constexpr float fCAFE_NAV_BTN_SIZE = 90.f;
	static constexpr float fCAFE_NAV_BTN_FONT = 36.f;
	static constexpr float fCAFE_BACK_BTN_W = 180.f;
	static constexpr float fCAFE_BACK_BTN_FONT = 34.f;
	// Victory overlay
	static constexpr float fVICTORY_BG_W = 640.f;
	static constexpr float fVICTORY_BG_H = 520.f;
	static constexpr float fVICTORY_TITLE_FONT = 72.f;
	static constexpr float fVICTORY_TITLE_Y = -160.f;
	static constexpr float fVICTORY_STARS_FONT = 88.f;
	static constexpr float fVICTORY_STARS_Y = -70.f;
	static constexpr float fVICTORY_STAR_SIZE = 64.f;
	static constexpr float fVICTORY_STAR_SPACING = 16.f;
	static constexpr float fVICTORY_CAT_FONT = 46.f;
	static constexpr float fVICTORY_COINS_FONT = 42.f;
	static constexpr float fNEXT_LEVEL_BTN_W = 260.f;
	static constexpr float fNEXT_LEVEL_BTN_H = 88.f;
	static constexpr float fNEXT_LEVEL_BTN_FONT = 38.f;

	// Pinball scene
	static constexpr float fPB_SCORE_FONT = 66.f;
	static constexpr float fPB_HIGH_SCORE_FONT = 44.f;
	static constexpr float fPB_BACK_BTN_W = 130.f;
	static constexpr float fPB_BACK_BTN_H = 88.f;
	static constexpr float fPB_BACK_BTN_FONT = 34.f;
	static constexpr float fPB_LAUNCH_HINT_FONT = 46.f;
	static constexpr float fGATE_SELECT_TITLE_FONT = 40.f;
	static constexpr float fGATE_BTN_SIZE = 100.f;
	static constexpr float fGATE_BTN_FONT = 34.f;
	static constexpr float fGATE_BTN_GAP = 18.f;
	static constexpr float fGATE_ACTION_BTN_W = 260.f;
	static constexpr float fGATE_ACTION_BTN_H = 85.f;
	static constexpr float fGATE_ACTION_BTN_FONT = 34.f;
	static constexpr float fGATE_BACK_BTN_W = 200.f;
	static constexpr float fGATE_BACK_BTN_H = 85.f;
}

void Project_RegisterEditorAutomationSteps()
{
	// ---- MainMenu scene (build index 0) ----
	g_xEngine.EditorAutomation().AddStep_CreateScene("MainMenu");
	g_xEngine.EditorAutomation().AddStep_CreateEntity("GameManager");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraPosition(0.f, 12.f, 0.f);
	g_xEngine.EditorAutomation().AddStep_SetCameraPitch(-1.5f);
	g_xEngine.EditorAutomation().AddStep_SetCameraFOV(glm::radians(45.f));
	g_xEngine.EditorAutomation().AddStep_SetCameraAspect(9.f / 16.f);
	g_xEngine.EditorAutomation().AddStep_SetAsMainCamera();
	g_xEngine.EditorAutomation().AddStep_AddUI();

	// Main menu background
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("MenuBackground");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuBackground", static_cast<int>(Zenith_UI::AnchorPreset::StretchAll));
	g_xEngine.EditorAutomation().AddStep_SetUIColor("MenuBackground", 0.06f, 0.06f, 0.12f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIGradientColor("MenuBackground", 0.10f, 0.06f, 0.18f, 1.f);

	// Menu title (standalone, not in button group to avoid glyph correction offset)
	g_xEngine.EditorAutomation().AddStep_CreateUIText("MenuTitle", "Paws & Pins");
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("MenuTitle", TilePuzzleUI::fMENU_TITLE_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("MenuTitle", 1.f, 1.f, 1.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("MenuTitle", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUITextShadow("MenuTitle", 2.f, 2.f, true);
	g_xEngine.EditorAutomation().AddStep_SetUITextShadowColor("MenuTitle", 0.f, 0.f, 0.f, 0.5f);
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuTitle", 0.f, -419.f);

	// Menu subtitle (standalone, positioned below title)
	g_xEngine.EditorAutomation().AddStep_CreateUIText("MenuSubtitle", "A Cat Puzzle Game");
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("MenuSubtitle", TilePuzzleUI::fMENU_SUBTITLE_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("MenuSubtitle", 0.6f, 0.6f, 0.8f, 0.7f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("MenuSubtitle", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUITextShadow("MenuSubtitle", 1.f, 1.f, true);
	g_xEngine.EditorAutomation().AddStep_SetUITextShadowColor("MenuSubtitle", 0.f, 0.f, 0.f, 0.3f);
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuSubtitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuSubtitle", 0.f, -375.f);

	// Menu layout group (vertical stack of buttons only)
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("MenuButtonGroup");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuButtonGroup", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuButtonGroup", 0.f, 60.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("MenuButtonGroup", static_cast<int>(Zenith_UI::LayoutDirection::Vertical));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("MenuButtonGroup", TilePuzzleUI::fMENU_BTN_SPACING);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("MenuButtonGroup", static_cast<int>(Zenith_UI::ChildAlignment::UpperCenter));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("MenuButtonGroup", true);

	// Continue button
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("ContinueButton", "Continue");
	g_xEngine.EditorAutomation().AddStep_SetUISize("ContinueButton", TilePuzzleUI::fMENU_BTN_W, TilePuzzleUI::fMENU_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("ContinueButton", TilePuzzleUI::fMENU_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("ContinueButton", 0.18f, 0.30f, 0.55f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("ContinueButton", 0.22f, 0.36f, 0.65f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonPressedColor("ContinueButton", 0.12f, 0.22f, 0.42f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonCornerRadius("ContinueButton", 12.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonShadow("ContinueButton", 3.f, 3.f, 2.f, true);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonShadowColor("ContinueButton", 0.f, 0.f, 0.f, 0.3f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonBorderColor("ContinueButton", 0.30f, 0.45f, 0.70f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonBorderThickness("ContinueButton", 2.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonTransitionDuration("ContinueButton", 0.12f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonTextShadow("ContinueButton", 1.f, 1.f, true);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonTextShadowColor("ContinueButton", 0.f, 0.f, 0.f, 0.4f);

	// Level Select button
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("LevelSelectButton", "Level Select");
	g_xEngine.EditorAutomation().AddStep_SetUISize("LevelSelectButton", TilePuzzleUI::fMENU_BTN_W, TilePuzzleUI::fMENU_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("LevelSelectButton", TilePuzzleUI::fMENU_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("LevelSelectButton", 0.18f, 0.30f, 0.55f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("LevelSelectButton", 0.22f, 0.36f, 0.65f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonPressedColor("LevelSelectButton", 0.12f, 0.22f, 0.42f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonCornerRadius("LevelSelectButton", 12.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonShadow("LevelSelectButton", 3.f, 3.f, 2.f, true);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonShadowColor("LevelSelectButton", 0.f, 0.f, 0.f, 0.3f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonBorderColor("LevelSelectButton", 0.30f, 0.45f, 0.70f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonBorderThickness("LevelSelectButton", 2.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonTransitionDuration("LevelSelectButton", 0.12f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonTextShadow("LevelSelectButton", 1.f, 1.f, true);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonTextShadowColor("LevelSelectButton", 0.f, 0.f, 0.f, 0.4f);

	// Pinball button
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("PinballButton", "Pinball");
	g_xEngine.EditorAutomation().AddStep_SetUISize("PinballButton", TilePuzzleUI::fMENU_BTN_W, TilePuzzleUI::fMENU_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("PinballButton", TilePuzzleUI::fMENU_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("PinballButton", 0.18f, 0.35f, 0.40f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("PinballButton", 0.22f, 0.42f, 0.48f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonPressedColor("PinballButton", 0.12f, 0.26f, 0.30f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonCornerRadius("PinballButton", 12.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonShadow("PinballButton", 3.f, 3.f, 2.f, true);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonShadowColor("PinballButton", 0.f, 0.f, 0.f, 0.3f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonBorderColor("PinballButton", 0.30f, 0.50f, 0.55f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonBorderThickness("PinballButton", 2.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonTransitionDuration("PinballButton", 0.12f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonTextShadow("PinballButton", 1.f, 1.f, true);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonTextShadowColor("PinballButton", 0.f, 0.f, 0.f, 0.4f);

	// Reset Save button
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("ResetSaveButton", "Reset Save");
	g_xEngine.EditorAutomation().AddStep_SetUISize("ResetSaveButton", TilePuzzleUI::fMENU_BTN_W, TilePuzzleUI::fMENU_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("ResetSaveButton", TilePuzzleUI::fMENU_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("ResetSaveButton", 0.45f, 0.15f, 0.15f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("ResetSaveButton", 0.55f, 0.20f, 0.20f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonPressedColor("ResetSaveButton", 0.35f, 0.10f, 0.10f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonCornerRadius("ResetSaveButton", 12.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonShadow("ResetSaveButton", 3.f, 3.f, 2.f, true);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonShadowColor("ResetSaveButton", 0.f, 0.f, 0.f, 0.3f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonBorderColor("ResetSaveButton", 0.60f, 0.25f, 0.25f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonBorderThickness("ResetSaveButton", 2.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonTransitionDuration("ResetSaveButton", 0.12f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonTextShadow("ResetSaveButton", 1.f, 1.f, true);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonTextShadowColor("ResetSaveButton", 0.f, 0.f, 0.f, 0.4f);

	// Cat Cafe button
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("CatCafeButton", "Cat Cafe");
	g_xEngine.EditorAutomation().AddStep_SetUISize("CatCafeButton", TilePuzzleUI::fMENU_BTN_W, TilePuzzleUI::fMENU_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("CatCafeButton", TilePuzzleUI::fMENU_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("CatCafeButton", 0.45f, 0.22f, 0.35f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("CatCafeButton", 0.55f, 0.28f, 0.42f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonPressedColor("CatCafeButton", 0.35f, 0.16f, 0.28f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonCornerRadius("CatCafeButton", 12.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonShadow("CatCafeButton", 3.f, 3.f, 2.f, true);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonShadowColor("CatCafeButton", 0.f, 0.f, 0.f, 0.3f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonBorderColor("CatCafeButton", 0.60f, 0.35f, 0.50f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonBorderThickness("CatCafeButton", 2.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonTransitionDuration("CatCafeButton", 0.12f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonTextShadow("CatCafeButton", 1.f, 1.f, true);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonTextShadowColor("CatCafeButton", 0.f, 0.f, 0.f, 0.4f);

	// Daily Puzzle button
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("DailyPuzzleButton", "Daily Puzzle");
	g_xEngine.EditorAutomation().AddStep_SetUISize("DailyPuzzleButton", TilePuzzleUI::fMENU_BTN_W, TilePuzzleUI::fMENU_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("DailyPuzzleButton", TilePuzzleUI::fMENU_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("DailyPuzzleButton", 0.22f, 0.38f, 0.22f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("DailyPuzzleButton", 0.28f, 0.48f, 0.28f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonPressedColor("DailyPuzzleButton", 0.16f, 0.28f, 0.16f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonCornerRadius("DailyPuzzleButton", 12.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonShadow("DailyPuzzleButton", 3.f, 3.f, 2.f, true);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonShadowColor("DailyPuzzleButton", 0.f, 0.f, 0.f, 0.3f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonBorderColor("DailyPuzzleButton", 0.35f, 0.55f, 0.35f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonBorderThickness("DailyPuzzleButton", 2.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonTransitionDuration("DailyPuzzleButton", 0.12f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonTextShadow("DailyPuzzleButton", 1.f, 1.f, true);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonTextShadowColor("DailyPuzzleButton", 0.f, 0.f, 0.f, 0.4f);

	g_xEngine.EditorAutomation().AddStep_AddUIChild("MenuButtonGroup", "ContinueButton");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("MenuButtonGroup", "LevelSelectButton");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("MenuButtonGroup", "PinballButton");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("MenuButtonGroup", "ResetSaveButton");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("MenuButtonGroup", "CatCafeButton");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("MenuButtonGroup", "DailyPuzzleButton");

	// Top-right counters area (vertical stack: coins pill, stars pill)
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("TopRightCounters");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("TopRightCounters", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("TopRightCounters", -14.f, 14.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("TopRightCounters", static_cast<int>(Zenith_UI::LayoutDirection::Vertical));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("TopRightCounters", 8.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("TopRightCounters", static_cast<int>(Zenith_UI::ChildAlignment::UpperRight));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("TopRightCounters", true);

	// Coin pill (icon + text with pill background)
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("CoinGroup");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("CoinGroup", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("CoinGroup", 8.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("CoinGroup", static_cast<int>(Zenith_UI::ChildAlignment::MiddleCenter));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("CoinGroup", true);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutPadding("CoinGroup", 10.f, 10.f, 10.f, 2.f);
	g_xEngine.EditorAutomation().AddStep_SetUIBackgroundColor("CoinGroup", 0.05f, 0.05f, 0.10f, 0.6f);
	g_xEngine.EditorAutomation().AddStep_SetUIBackgroundCornerRadius("CoinGroup", 16.f);
	g_xEngine.EditorAutomation().AddStep_SetUIBackgroundBorder("CoinGroup", 0.2f, 0.2f, 0.3f, 1.f);

	g_xEngine.EditorAutomation().AddStep_CreateUIImage("CoinIcon");
	g_xEngine.EditorAutomation().AddStep_SetUISize("CoinIcon", TilePuzzleUI::fICON_SIZE, TilePuzzleUI::fICON_SIZE);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("CoinIcon", 1.f, 0.85f, 0.2f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIImageTexturePath("CoinIcon",
		GAME_ASSETS_DIR "Textures/Icons/coin" ZENITH_TEXTURE_EXT);

	g_xEngine.EditorAutomation().AddStep_CreateUIText("CoinText", "0");
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("CoinText", TilePuzzleUI::fPILL_TEXT_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("CoinText", 1.f, 0.85f, 0.2f, 1.f);

	g_xEngine.EditorAutomation().AddStep_AddUIChild("CoinGroup", "CoinIcon");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("CoinGroup", "CoinText");

	// Star pill (icon + text with pill background)
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("StarGroup");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("StarGroup", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("StarGroup", 8.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("StarGroup", static_cast<int>(Zenith_UI::ChildAlignment::MiddleCenter));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("StarGroup", true);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutPadding("StarGroup", 10.f, 10.f, 10.f, 2.f);
	g_xEngine.EditorAutomation().AddStep_SetUIBackgroundColor("StarGroup", 0.05f, 0.05f, 0.10f, 0.6f);
	g_xEngine.EditorAutomation().AddStep_SetUIBackgroundCornerRadius("StarGroup", 16.f);
	g_xEngine.EditorAutomation().AddStep_SetUIBackgroundBorder("StarGroup", 0.2f, 0.2f, 0.3f, 1.f);

	g_xEngine.EditorAutomation().AddStep_CreateUIImage("StarIcon");
	g_xEngine.EditorAutomation().AddStep_SetUISize("StarIcon", TilePuzzleUI::fICON_SIZE, TilePuzzleUI::fICON_SIZE);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("StarIcon", 1.f, 0.85f, 0.2f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIImageTexturePath("StarIcon",
		GAME_ASSETS_DIR "Textures/Icons/star_filled" ZENITH_TEXTURE_EXT);

	g_xEngine.EditorAutomation().AddStep_CreateUIText("TotalStarsText", "0 / 300");
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("TotalStarsText", TilePuzzleUI::fPILL_TEXT_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("TotalStarsText", 1.f, 0.85f, 0.2f, 1.f);

	g_xEngine.EditorAutomation().AddStep_AddUIChild("StarGroup", "StarIcon");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("StarGroup", "TotalStarsText");

	g_xEngine.EditorAutomation().AddStep_AddUIChild("TopRightCounters", "CoinGroup");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("TopRightCounters", "StarGroup");

	// Lives layout group (top-left of menu): icon + text with pill background
	// Lives area — vertical stack: pill (icon+text), timer, refill button
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("LivesArea");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("LivesArea", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("LivesArea", 14.f, 14.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("LivesArea", static_cast<int>(Zenith_UI::LayoutDirection::Vertical));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("LivesArea", 6.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("LivesArea", static_cast<int>(Zenith_UI::ChildAlignment::UpperLeft));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("LivesArea", true);

	// Lives pill — horizontal icon + text with background
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("LivesGroup");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("LivesGroup", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("LivesGroup", 8.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("LivesGroup", static_cast<int>(Zenith_UI::ChildAlignment::MiddleCenter));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("LivesGroup", true);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutPadding("LivesGroup", 10.f, 10.f, 10.f, 2.f);
	g_xEngine.EditorAutomation().AddStep_SetUIBackgroundColor("LivesGroup", 0.05f, 0.05f, 0.10f, 0.6f);
	g_xEngine.EditorAutomation().AddStep_SetUIBackgroundCornerRadius("LivesGroup", 16.f);
	g_xEngine.EditorAutomation().AddStep_SetUIBackgroundBorder("LivesGroup", 0.2f, 0.2f, 0.3f, 1.f);

	g_xEngine.EditorAutomation().AddStep_CreateUIImage("HeartIcon");
	g_xEngine.EditorAutomation().AddStep_SetUISize("HeartIcon", TilePuzzleUI::fICON_SIZE, TilePuzzleUI::fICON_SIZE);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("HeartIcon", 1.f, 0.3f, 0.3f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIImageTexturePath("HeartIcon",
		GAME_ASSETS_DIR "Textures/Icons/heart" ZENITH_TEXTURE_EXT);

	g_xEngine.EditorAutomation().AddStep_CreateUIText("LivesText", "5/5");
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("LivesText", TilePuzzleUI::fPILL_TEXT_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("LivesText", 1.f, 0.3f, 0.3f, 1.f);

	g_xEngine.EditorAutomation().AddStep_AddUIChild("LivesGroup", "HeartIcon");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("LivesGroup", "LivesText");

	// Lives timer text (shown when lives are regenerating)
	g_xEngine.EditorAutomation().AddStep_CreateUIText("LivesTimerText", "");
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("LivesTimerText", TilePuzzleUI::fSMALL_LABEL_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("LivesTimerText", 0.8f, 0.5f, 0.5f, 0.8f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("LivesTimerText", false);

	// Lives Refill button (hidden by default)
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("RefillLivesButton", "Refill (50)");
	g_xEngine.EditorAutomation().AddStep_SetUISize("RefillLivesButton", TilePuzzleUI::fREFILL_BTN_W, TilePuzzleUI::fREFILL_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("RefillLivesButton", TilePuzzleUI::fREFILL_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("RefillLivesButton", 0.50f, 0.20f, 0.20f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("RefillLivesButton", 0.60f, 0.30f, 0.30f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonPressedColor("RefillLivesButton", 0.35f, 0.12f, 0.12f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonCornerRadius("RefillLivesButton", 8.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonShadow("RefillLivesButton", 3.f, 3.f, 2.f, true);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonShadowColor("RefillLivesButton", 0.f, 0.f, 0.f, 0.3f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonBorderColor("RefillLivesButton", 0.65f, 0.32f, 0.32f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonBorderThickness("RefillLivesButton", 2.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonTransitionDuration("RefillLivesButton", 0.12f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonTextShadow("RefillLivesButton", 1.f, 1.f, true);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonTextShadowColor("RefillLivesButton", 0.f, 0.f, 0.f, 0.4f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("RefillLivesButton", false);

	// Hint token pill (icon + text with pill background) — above lives
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("HintTokenGroup");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("HintTokenGroup", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("HintTokenGroup", 8.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("HintTokenGroup", static_cast<int>(Zenith_UI::ChildAlignment::MiddleCenter));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("HintTokenGroup", true);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutPadding("HintTokenGroup", 10.f, 10.f, 10.f, 2.f);
	g_xEngine.EditorAutomation().AddStep_SetUIBackgroundColor("HintTokenGroup", 0.05f, 0.05f, 0.10f, 0.6f);
	g_xEngine.EditorAutomation().AddStep_SetUIBackgroundCornerRadius("HintTokenGroup", 16.f);
	g_xEngine.EditorAutomation().AddStep_SetUIBackgroundBorder("HintTokenGroup", 0.2f, 0.2f, 0.3f, 1.f);

	g_xEngine.EditorAutomation().AddStep_CreateUIImage("HintTokenIcon");
	g_xEngine.EditorAutomation().AddStep_SetUISize("HintTokenIcon", TilePuzzleUI::fICON_SIZE, TilePuzzleUI::fICON_SIZE);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("HintTokenIcon", 0.4f, 0.85f, 1.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIImageTexturePath("HintTokenIcon",
		GAME_ASSETS_DIR "Textures/Icons/hint_token" ZENITH_TEXTURE_EXT);

	g_xEngine.EditorAutomation().AddStep_CreateUIText("HintTokenText", "0");
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("HintTokenText", TilePuzzleUI::fPILL_TEXT_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("HintTokenText", 0.4f, 0.85f, 1.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUITextShadow("HintTokenText", 1.f, 1.f, true);

	g_xEngine.EditorAutomation().AddStep_AddUIChild("HintTokenGroup", "HintTokenIcon");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("HintTokenGroup", "HintTokenText");

	// Add children of the vertical LivesArea (hint tokens on top, then lives)
	g_xEngine.EditorAutomation().AddStep_AddUIChild("LivesArea", "HintTokenGroup");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("LivesArea", "LivesGroup");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("LivesArea", "LivesTimerText");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("LivesArea", "RefillLivesButton");

	// Daily streak (bottom-left) — vertical layout with pill background
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("StreakGroup");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("StreakGroup", static_cast<int>(Zenith_UI::AnchorPreset::BottomLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("StreakGroup", 14.f, -14.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("StreakGroup", static_cast<int>(Zenith_UI::LayoutDirection::Vertical));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("StreakGroup", 2.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("StreakGroup", static_cast<int>(Zenith_UI::ChildAlignment::UpperLeft));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("StreakGroup", true);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutPadding("StreakGroup", 10.f, 6.f, 10.f, 6.f);
	g_xEngine.EditorAutomation().AddStep_SetUIBackgroundColor("StreakGroup", 0.05f, 0.05f, 0.10f, 0.6f);
	g_xEngine.EditorAutomation().AddStep_SetUIBackgroundCornerRadius("StreakGroup", 14.f);
	g_xEngine.EditorAutomation().AddStep_SetUIBackgroundBorder("StreakGroup", 0.2f, 0.2f, 0.3f, 1.f);

	g_xEngine.EditorAutomation().AddStep_CreateUIText("DailyStreakLabel", "Streak");
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("DailyStreakLabel", TilePuzzleUI::fSTREAK_LABEL_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("DailyStreakLabel", 0.5f, 0.7f, 0.5f, 0.7f);

	g_xEngine.EditorAutomation().AddStep_CreateUIText("DailyStreakText", "0 days");
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("DailyStreakText", TilePuzzleUI::fSTREAK_VALUE_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("DailyStreakText", 0.6f, 0.8f, 0.6f, 1.f);

	g_xEngine.EditorAutomation().AddStep_AddUIChild("StreakGroup", "DailyStreakLabel");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("StreakGroup", "DailyStreakText");

	// Text shadows on HUD texts
	g_xEngine.EditorAutomation().AddStep_SetUITextShadow("CoinText", 1.f, 1.f, true);
	g_xEngine.EditorAutomation().AddStep_SetUITextShadow("LivesText", 1.f, 1.f, true);
	g_xEngine.EditorAutomation().AddStep_SetUITextShadow("DailyStreakLabel", 1.f, 1.f, true);
	g_xEngine.EditorAutomation().AddStep_SetUITextShadow("DailyStreakText", 1.f, 1.f, true);

	// Version text
	g_xEngine.EditorAutomation().AddStep_CreateUIText("VersionText", "v1.0");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("VersionText", static_cast<int>(Zenith_UI::AnchorPreset::BottomCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("VersionText", 0.f, -8.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("VersionText", TilePuzzleUI::fVERSION_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("VersionText", 0.4f, 0.4f, 0.5f, 0.4f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("VersionText", static_cast<int>(Zenith_UI::TextAlignment::Center));

	// ---- Cat Cafe UI elements (starts hidden) ----

	// Cat Cafe title
	g_xEngine.EditorAutomation().AddStep_CreateUIText("CatCafeTitle", "Cat Cafe");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("CatCafeTitle", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("CatCafeTitle", 0.f, 30.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("CatCafeTitle", TilePuzzleUI::fSCREEN_TITLE_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("CatCafeTitle", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIColor("CatCafeTitle", 1.f, 0.8f, 0.6f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUITextShadow("CatCafeTitle", 2.f, 2.f, true);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("CatCafeTitle", false);

	// Cat Cafe count
	g_xEngine.EditorAutomation().AddStep_CreateUIText("CatCafeCount", "0 / 100 cats rescued");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("CatCafeCount", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("CatCafeCount", 0.f, TilePuzzleUI::fCAFE_COUNT_Y);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("CatCafeCount", TilePuzzleUI::fCAFE_COUNT_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("CatCafeCount", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIColor("CatCafeCount", 0.8f, 0.8f, 0.8f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("CatCafeCount", false);

	// Cat collection progress bar (background + fill)
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("CatProgressBg");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("CatProgressBg", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("CatProgressBg", 0.f, TilePuzzleUI::fCAT_PROGRESS_Y);
	g_xEngine.EditorAutomation().AddStep_SetUISize("CatProgressBg", TilePuzzleUI::fCAT_PROGRESS_W, TilePuzzleUI::fCAT_PROGRESS_H);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("CatProgressBg", 0.15f, 0.15f, 0.2f, 0.8f);
	g_xEngine.EditorAutomation().AddStep_SetUICornerRadius("CatProgressBg", 6.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("CatProgressBg", false);

	g_xEngine.EditorAutomation().AddStep_CreateUIRect("CatProgressFill");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("CatProgressFill", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("CatProgressFill", 0.f, TilePuzzleUI::fCAT_PROGRESS_Y);
	g_xEngine.EditorAutomation().AddStep_SetUISize("CatProgressFill", TilePuzzleUI::fCAT_PROGRESS_W, TilePuzzleUI::fCAT_PROGRESS_H);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("CatProgressFill", 1.f, 0.7f, 0.2f, 0.9f);
	g_xEngine.EditorAutomation().AddStep_SetUICornerRadius("CatProgressFill", 6.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("CatProgressFill", false);

	// Cat cafe single-cat info display
	g_xEngine.EditorAutomation().AddStep_CreateUIText("CatCafeInfoName", "");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("CatCafeInfoName", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("CatCafeInfoName", 0.f, 340.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("CatCafeInfoName", 32.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("CatCafeInfoName", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIColor("CatCafeInfoName", 1.f, 1.f, 1.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUITextShadow("CatCafeInfoName", 2.f, 2.f, true);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("CatCafeInfoName", false);

	g_xEngine.EditorAutomation().AddStep_CreateUIText("CatCafeInfoBreed", "");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("CatCafeInfoBreed", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("CatCafeInfoBreed", 0.f, 385.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("CatCafeInfoBreed", 22.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("CatCafeInfoBreed", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIColor("CatCafeInfoBreed", 0.75f, 0.75f, 0.8f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUITextShadow("CatCafeInfoBreed", 1.f, 1.f, true);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("CatCafeInfoBreed", false);

	g_xEngine.EditorAutomation().AddStep_CreateUIText("CatCafeInfoLevel", "");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("CatCafeInfoLevel", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("CatCafeInfoLevel", 0.f, 415.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("CatCafeInfoLevel", 20.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("CatCafeInfoLevel", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIColor("CatCafeInfoLevel", 1.f, 0.85f, 0.2f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("CatCafeInfoLevel", false);

	g_xEngine.EditorAutomation().AddStep_CreateUIText("CatCafeEmpty", "No cats rescued yet!");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("CatCafeEmpty", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("CatCafeEmpty", 0.f, 0.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("CatCafeEmpty", 28.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("CatCafeEmpty", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIColor("CatCafeEmpty", 0.7f, 0.7f, 0.75f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("CatCafeEmpty", false);

	g_xEngine.EditorAutomation().AddStep_CreateUIText("CatCafeSwipeHint", "< Swipe to browse >");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("CatCafeSwipeHint", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("CatCafeSwipeHint", 0.f, 460.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("CatCafeSwipeHint", 18.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("CatCafeSwipeHint", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIColor("CatCafeSwipeHint", 0.5f, 0.5f, 0.55f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("CatCafeSwipeHint", false);

	// Cat Cafe navigation layout group (< Back >)
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("CatCafeNavGroup");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("CatCafeNavGroup", static_cast<int>(Zenith_UI::AnchorPreset::BottomCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("CatCafeNavGroup", 0.f, -40.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("CatCafeNavGroup", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("CatCafeNavGroup", 10.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("CatCafeNavGroup", static_cast<int>(Zenith_UI::ChildAlignment::MiddleCenter));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("CatCafeNavGroup", true);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("CatCafeNavGroup", false);

	g_xEngine.EditorAutomation().AddStep_CreateUIButton("CatCafePrevPage", "<");
	g_xEngine.EditorAutomation().AddStep_SetUISize("CatCafePrevPage", TilePuzzleUI::fCAFE_NAV_BTN_SIZE, TilePuzzleUI::fCAFE_NAV_BTN_SIZE);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("CatCafePrevPage", TilePuzzleUI::fCAFE_NAV_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("CatCafePrevPage", 0.15f, 0.2f, 0.3f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("CatCafePrevPage", 0.25f, 0.3f, 0.45f, 1.f);

	g_xEngine.EditorAutomation().AddStep_CreateUIButton("CatCafeBackButton", "Back");
	g_xEngine.EditorAutomation().AddStep_SetUISize("CatCafeBackButton", TilePuzzleUI::fCAFE_BACK_BTN_W, TilePuzzleUI::fCAFE_NAV_BTN_SIZE);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("CatCafeBackButton", TilePuzzleUI::fCAFE_BACK_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("CatCafeBackButton", 0.15f, 0.2f, 0.3f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("CatCafeBackButton", 0.25f, 0.3f, 0.45f, 1.f);

	g_xEngine.EditorAutomation().AddStep_CreateUIButton("CatCafeNextPage", ">");
	g_xEngine.EditorAutomation().AddStep_SetUISize("CatCafeNextPage", TilePuzzleUI::fCAFE_NAV_BTN_SIZE, TilePuzzleUI::fCAFE_NAV_BTN_SIZE);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("CatCafeNextPage", TilePuzzleUI::fCAFE_NAV_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("CatCafeNextPage", 0.15f, 0.2f, 0.3f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("CatCafeNextPage", 0.25f, 0.3f, 0.45f, 1.f);

	g_xEngine.EditorAutomation().AddStep_AddUIChild("CatCafeNavGroup", "CatCafePrevPage");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("CatCafeNavGroup", "CatCafeBackButton");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("CatCafeNavGroup", "CatCafeNextPage");

	// Level select background (starts hidden)
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("LevelSelectBg");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("LevelSelectBg", static_cast<int>(Zenith_UI::AnchorPreset::StretchAll));
	g_xEngine.EditorAutomation().AddStep_SetUIColor("LevelSelectBg", 0.06f, 0.06f, 0.12f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIGradientColor("LevelSelectBg", 0.10f, 0.06f, 0.18f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("LevelSelectBg", false);

	// Level select title (starts hidden)
	g_xEngine.EditorAutomation().AddStep_CreateUIText("LevelSelectTitle", "Select Level");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("LevelSelectTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("LevelSelectTitle", 0.f, -260.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("LevelSelectTitle", TilePuzzleUI::fLEVEL_SELECT_TITLE_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("LevelSelectTitle", 1.f, 1.f, 1.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUITextShadow("LevelSelectTitle", 2.f, 2.f, true);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("LevelSelectTitle", false);

	// Page text (starts hidden)
	g_xEngine.EditorAutomation().AddStep_CreateUIText("PageText", "Page 1 / 5");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("PageText", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("PageText", 0.f, -200.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("PageText", TilePuzzleUI::fPAGE_TEXT_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("PageText", 0.7f, 0.7f, 0.8f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("PageText", false);

	// Star progress text (starts hidden)
	g_xEngine.EditorAutomation().AddStep_CreateUIText("LevelSelectStarProgress", "Stars: 0 / 300");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("LevelSelectStarProgress", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("LevelSelectStarProgress", 0.f, -240.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("LevelSelectStarProgress", TilePuzzleUI::fSTAR_PROGRESS_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("LevelSelectStarProgress", 1.0f, 0.85f, 0.2f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("LevelSelectStarProgress", false);

	// Level select grid (4x5)
	for (uint32_t u = 0; u < 20; ++u)
	{
		float fX = (static_cast<float>(u % 5) - 2.f) * TilePuzzleUI::fLEVEL_GRID_X_SPACING;
		float fY = -50.f + (static_cast<float>(u / 5) - 1.5f) * TilePuzzleUI::fLEVEL_GRID_Y_SPACING;
		g_xEngine.EditorAutomation().AddStep_CreateUIButton(s_aszLevelBtnNames[u], s_aszLevelLabels[u]);
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor(s_aszLevelBtnNames[u], static_cast<int>(Zenith_UI::AnchorPreset::Center));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition(s_aszLevelBtnNames[u], fX, fY);
		g_xEngine.EditorAutomation().AddStep_SetUISize(s_aszLevelBtnNames[u], TilePuzzleUI::fLEVEL_BTN_W, TilePuzzleUI::fLEVEL_BTN_H);
		g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize(s_aszLevelBtnNames[u], TilePuzzleUI::fLEVEL_BTN_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor(s_aszLevelBtnNames[u], 0.2f, 0.3f, 0.5f, 1.f);
		g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor(s_aszLevelBtnNames[u], 0.3f, 0.4f, 0.6f, 1.f);
		g_xEngine.EditorAutomation().AddStep_SetUIButtonPressedColor(s_aszLevelBtnNames[u], 0.1f, 0.15f, 0.3f, 1.f);
		g_xEngine.EditorAutomation().AddStep_SetUIButtonCornerRadius(s_aszLevelBtnNames[u], 8.f);
		g_xEngine.EditorAutomation().AddStep_SetUIButtonShadow(s_aszLevelBtnNames[u], 2.f, 2.f, 1.f, true);
		g_xEngine.EditorAutomation().AddStep_SetUIButtonTransitionDuration(s_aszLevelBtnNames[u], 0.10f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible(s_aszLevelBtnNames[u], false);
	}

	// Level select navigation layout group (< Back >)
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("LevelSelectNavGroup");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("LevelSelectNavGroup", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("LevelSelectNavGroup", 0.f, TilePuzzleUI::fLEVEL_NAV_Y);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("LevelSelectNavGroup", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("LevelSelectNavGroup", 50.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("LevelSelectNavGroup", static_cast<int>(Zenith_UI::ChildAlignment::MiddleCenter));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("LevelSelectNavGroup", true);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("LevelSelectNavGroup", false);

	// PrevPage button
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("PrevPageButton", "<");
	g_xEngine.EditorAutomation().AddStep_SetUISize("PrevPageButton", TilePuzzleUI::fNAV_BTN_W, TilePuzzleUI::fNAV_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("PrevPageButton", TilePuzzleUI::fNAV_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("PrevPageButton", 0.15f, 0.2f, 0.3f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("PrevPageButton", 0.25f, 0.3f, 0.45f, 1.f);

	// Back button
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("BackButton", "Back");
	g_xEngine.EditorAutomation().AddStep_SetUISize("BackButton", TilePuzzleUI::fNAV_BACK_BTN_W, TilePuzzleUI::fNAV_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("BackButton", TilePuzzleUI::fNAV_BACK_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("BackButton", 0.15f, 0.2f, 0.3f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("BackButton", 0.25f, 0.3f, 0.45f, 1.f);

	// NextPage button
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("NextPageButton", ">");
	g_xEngine.EditorAutomation().AddStep_SetUISize("NextPageButton", TilePuzzleUI::fNAV_BTN_W, TilePuzzleUI::fNAV_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("NextPageButton", TilePuzzleUI::fNAV_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("NextPageButton", 0.15f, 0.2f, 0.3f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("NextPageButton", 0.25f, 0.3f, 0.45f, 1.f);

	g_xEngine.EditorAutomation().AddStep_AddUIChild("LevelSelectNavGroup", "PrevPageButton");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("LevelSelectNavGroup", "BackButton");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("LevelSelectNavGroup", "NextPageButton");

	// Level select nav buttons focus navigation (horizontal)
	g_xEngine.EditorAutomation().AddStep_SetUINavigation("PrevPageButton", nullptr, nullptr, nullptr, "BackButton");
	g_xEngine.EditorAutomation().AddStep_SetUINavigation("BackButton", nullptr, nullptr, "PrevPageButton", "NextPageButton");
	g_xEngine.EditorAutomation().AddStep_SetUINavigation("NextPageButton", nullptr, nullptr, "BackButton", nullptr);

	// ---- Settings Button (main menu, gear icon) ----
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("SettingsButton", "Settings");
	g_xEngine.EditorAutomation().AddStep_SetUISize("SettingsButton", TilePuzzleUI::fMENU_BTN_W, TilePuzzleUI::fMENU_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("SettingsButton", TilePuzzleUI::fMENU_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("SettingsButton", 0.25f, 0.25f, 0.3f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("SettingsButton", 0.35f, 0.35f, 0.4f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonPressedColor("SettingsButton", 0.15f, 0.15f, 0.2f, 1.f);
	g_xEngine.EditorAutomation().AddStep_AddUIChild("MenuButtonGroup", "SettingsButton");

	// ---- Achievements Button (main menu) ----
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("AchievementsButton", "Achievements");
	g_xEngine.EditorAutomation().AddStep_SetUISize("AchievementsButton", TilePuzzleUI::fMENU_BTN_W, TilePuzzleUI::fMENU_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("AchievementsButton", TilePuzzleUI::fMENU_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("AchievementsButton", 0.4f, 0.35f, 0.1f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("AchievementsButton", 0.5f, 0.45f, 0.15f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonPressedColor("AchievementsButton", 0.3f, 0.25f, 0.08f, 1.f);
	g_xEngine.EditorAutomation().AddStep_AddUIChild("MenuButtonGroup", "AchievementsButton");

	// Main menu focus navigation (vertical)
	g_xEngine.EditorAutomation().AddStep_SetUINavigation("ContinueButton", nullptr, "LevelSelectButton", nullptr, nullptr);
	g_xEngine.EditorAutomation().AddStep_SetUINavigation("LevelSelectButton", "ContinueButton", "PinballButton", nullptr, nullptr);
	g_xEngine.EditorAutomation().AddStep_SetUINavigation("PinballButton", "LevelSelectButton", "ResetSaveButton", nullptr, nullptr);
	g_xEngine.EditorAutomation().AddStep_SetUINavigation("ResetSaveButton", "PinballButton", "CatCafeButton", nullptr, nullptr);
	g_xEngine.EditorAutomation().AddStep_SetUINavigation("CatCafeButton", "ResetSaveButton", "DailyPuzzleButton", nullptr, nullptr);
	g_xEngine.EditorAutomation().AddStep_SetUINavigation("DailyPuzzleButton", "CatCafeButton", "SettingsButton", nullptr, nullptr);
	g_xEngine.EditorAutomation().AddStep_SetUINavigation("SettingsButton", "DailyPuzzleButton", "AchievementsButton", nullptr, nullptr);
	g_xEngine.EditorAutomation().AddStep_SetUINavigation("AchievementsButton", "SettingsButton", nullptr, nullptr, nullptr);

	// ---- Settings Screen UI elements (starts hidden) ----

	// Settings background
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("SettingsBg");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("SettingsBg", static_cast<int>(Zenith_UI::AnchorPreset::StretchAll));
	g_xEngine.EditorAutomation().AddStep_SetUIColor("SettingsBg", 0.06f, 0.06f, 0.12f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIGradientColor("SettingsBg", 0.10f, 0.06f, 0.18f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("SettingsBg", false);

	// Settings title
	g_xEngine.EditorAutomation().AddStep_CreateUIText("SettingsTitle", "Settings");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("SettingsTitle", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("SettingsTitle", 0.f, 40.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("SettingsTitle", TilePuzzleUI::fSCREEN_TITLE_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("SettingsTitle", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIColor("SettingsTitle", 1.f, 1.f, 1.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUITextShadow("SettingsTitle", 2.f, 2.f, true);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("SettingsTitle", false);

	// Settings toggles
	g_xEngine.EditorAutomation().AddStep_CreateUIToggle("SettingsSoundBtn", "Sound");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("SettingsSoundBtn", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("SettingsSoundBtn", 0.f, -60.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("SettingsSoundBtn", TilePuzzleUI::fTOGGLE_W, TilePuzzleUI::fTOGGLE_H);
	g_xEngine.EditorAutomation().AddStep_SetUIToggleOnColor("SettingsSoundBtn", 0.2f, 0.4f, 0.3f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIToggleOffColor("SettingsSoundBtn", 0.3f, 0.15f, 0.15f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("SettingsSoundBtn", false);

	g_xEngine.EditorAutomation().AddStep_CreateUIToggle("SettingsMusicBtn", "Music");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("SettingsMusicBtn", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("SettingsMusicBtn", 0.f, 20.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("SettingsMusicBtn", TilePuzzleUI::fTOGGLE_W, TilePuzzleUI::fTOGGLE_H);
	g_xEngine.EditorAutomation().AddStep_SetUIToggleOnColor("SettingsMusicBtn", 0.2f, 0.4f, 0.3f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIToggleOffColor("SettingsMusicBtn", 0.3f, 0.15f, 0.15f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("SettingsMusicBtn", false);

	g_xEngine.EditorAutomation().AddStep_CreateUIToggle("SettingsHapticsBtn", "Haptics");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("SettingsHapticsBtn", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("SettingsHapticsBtn", 0.f, 100.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("SettingsHapticsBtn", TilePuzzleUI::fTOGGLE_W, TilePuzzleUI::fTOGGLE_H);
	g_xEngine.EditorAutomation().AddStep_SetUIToggleOnColor("SettingsHapticsBtn", 0.2f, 0.4f, 0.3f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIToggleOffColor("SettingsHapticsBtn", 0.3f, 0.15f, 0.15f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("SettingsHapticsBtn", false);

	// Credits button
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("SettingsCreditsBtn", "Credits");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("SettingsCreditsBtn", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("SettingsCreditsBtn", 0.f, 170.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("SettingsCreditsBtn", TilePuzzleUI::fSETTINGS_BTN_W, TilePuzzleUI::fSETTINGS_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("SettingsCreditsBtn", TilePuzzleUI::fSETTINGS_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("SettingsCreditsBtn", 0.2f, 0.2f, 0.3f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("SettingsCreditsBtn", 0.3f, 0.3f, 0.42f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonPressedColor("SettingsCreditsBtn", 0.14f, 0.14f, 0.22f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("SettingsCreditsBtn", false);

	// Settings back button
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("SettingsBackBtn", "Back");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("SettingsBackBtn", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("SettingsBackBtn", 0.f, 240.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("SettingsBackBtn", TilePuzzleUI::fSETTINGS_BTN_W, TilePuzzleUI::fSETTINGS_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("SettingsBackBtn", TilePuzzleUI::fSETTINGS_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("SettingsBackBtn", 0.15f, 0.2f, 0.3f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("SettingsBackBtn", 0.25f, 0.3f, 0.45f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonPressedColor("SettingsBackBtn", 0.12f, 0.15f, 0.25f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("SettingsBackBtn", false);

	// Settings focus navigation (vertical)
	g_xEngine.EditorAutomation().AddStep_SetUINavigation("SettingsSoundBtn", nullptr, "SettingsMusicBtn", nullptr, nullptr);
	g_xEngine.EditorAutomation().AddStep_SetUINavigation("SettingsMusicBtn", "SettingsSoundBtn", "SettingsHapticsBtn", nullptr, nullptr);
	g_xEngine.EditorAutomation().AddStep_SetUINavigation("SettingsHapticsBtn", "SettingsMusicBtn", "SettingsCreditsBtn", nullptr, nullptr);
	g_xEngine.EditorAutomation().AddStep_SetUINavigation("SettingsCreditsBtn", "SettingsHapticsBtn", "SettingsBackBtn", nullptr, nullptr);
	g_xEngine.EditorAutomation().AddStep_SetUINavigation("SettingsBackBtn", "SettingsCreditsBtn", nullptr, nullptr, nullptr);

	// ---- Confirm Dialog Overlay ----
	g_xEngine.EditorAutomation().AddStep_CreateUIOverlay("ConfirmOverlay");
	g_xEngine.EditorAutomation().AddStep_SetUIOverlayDimColor("ConfirmOverlay", 0.f, 0.f, 0.f, 0.7f);
	g_xEngine.EditorAutomation().AddStep_SetUIOverlayContentSize("ConfirmOverlay", TilePuzzleUI::fCONFIRM_OVERLAY_W, TilePuzzleUI::fCONFIRM_OVERLAY_H);

	g_xEngine.EditorAutomation().AddStep_CreateUIText("ConfirmText", "Are you sure?");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("ConfirmText", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("ConfirmText", 0.f, 30.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("ConfirmText", TilePuzzleUI::fCONFIRM_TEXT_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("ConfirmText", 1.f, 1.f, 0.9f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("ConfirmText", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_AddUIChild("ConfirmOverlay", "ConfirmText");

	g_xEngine.EditorAutomation().AddStep_CreateUIButton("ConfirmCancelBtn", "Cancel");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("ConfirmCancelBtn", static_cast<int>(Zenith_UI::AnchorPreset::BottomLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("ConfirmCancelBtn", 30.f, -20.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("ConfirmCancelBtn", TilePuzzleUI::fOVERLAY_BTN_W, TilePuzzleUI::fOVERLAY_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("ConfirmCancelBtn", TilePuzzleUI::fOVERLAY_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("ConfirmCancelBtn", 0.25f, 0.25f, 0.3f, 0.9f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("ConfirmCancelBtn", 0.35f, 0.35f, 0.42f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonPressedColor("ConfirmCancelBtn", 0.18f, 0.18f, 0.22f, 1.f);
	g_xEngine.EditorAutomation().AddStep_AddUIChild("ConfirmOverlay", "ConfirmCancelBtn");

	g_xEngine.EditorAutomation().AddStep_CreateUIButton("ConfirmAcceptBtn", "Accept");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("ConfirmAcceptBtn", static_cast<int>(Zenith_UI::AnchorPreset::BottomRight));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("ConfirmAcceptBtn", -30.f, -20.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("ConfirmAcceptBtn", TilePuzzleUI::fOVERLAY_BTN_W, TilePuzzleUI::fOVERLAY_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("ConfirmAcceptBtn", TilePuzzleUI::fOVERLAY_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("ConfirmAcceptBtn", 0.5f, 0.15f, 0.15f, 0.9f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("ConfirmAcceptBtn", 0.65f, 0.25f, 0.25f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonPressedColor("ConfirmAcceptBtn", 0.35f, 0.1f, 0.1f, 1.f);
	g_xEngine.EditorAutomation().AddStep_AddUIChild("ConfirmOverlay", "ConfirmAcceptBtn");

	// ---- Tutorial Overlay ----
	g_xEngine.EditorAutomation().AddStep_CreateUIOverlay("TutorialOverlay");
	g_xEngine.EditorAutomation().AddStep_SetUIOverlayDimColor("TutorialOverlay", 0.f, 0.f, 0.f, 0.7f);
	g_xEngine.EditorAutomation().AddStep_SetUIOverlayContentSize("TutorialOverlay", TilePuzzleUI::fTUTORIAL_OVERLAY_W, TilePuzzleUI::fTUTORIAL_OVERLAY_H);

	g_xEngine.EditorAutomation().AddStep_CreateUIText("TutorialText", " ");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("TutorialText", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("TutorialText", 0.f, 20.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("TutorialText", 750.f, 100.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("TutorialText", TilePuzzleUI::fTUTORIAL_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("TutorialText", 1.f, 1.f, 0.8f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("TutorialText", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_AddUIChild("TutorialOverlay", "TutorialText");

	g_xEngine.EditorAutomation().AddStep_CreateUIText("TutorialHintText", "Tap to continue");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("TutorialHintText", static_cast<int>(Zenith_UI::AnchorPreset::BottomCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("TutorialHintText", 0.f, -15.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("TutorialHintText", 400.f, 40.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("TutorialHintText", TilePuzzleUI::fTUTORIAL_HINT_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("TutorialHintText", 0.7f, 0.7f, 0.7f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("TutorialHintText", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_AddUIChild("TutorialOverlay", "TutorialHintText");

	// ---- Credits Overlay ----
	g_xEngine.EditorAutomation().AddStep_CreateUIOverlay("CreditsOverlay");
	g_xEngine.EditorAutomation().AddStep_SetUIOverlayDimColor("CreditsOverlay", 0.f, 0.f, 0.f, 0.8f);
	g_xEngine.EditorAutomation().AddStep_SetUIOverlayContentSize("CreditsOverlay", TilePuzzleUI::fCREDITS_OVERLAY_W, TilePuzzleUI::fCREDITS_OVERLAY_H);

	g_xEngine.EditorAutomation().AddStep_CreateUIText("CreditsTitleText", "Paws & Pins v1.0");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("CreditsTitleText", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("CreditsTitleText", 0.f, 30.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("CreditsTitleText", TilePuzzleUI::fCREDITS_TITLE_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("CreditsTitleText", 1.f, 0.9f, 0.3f, 1.f);
	g_xEngine.EditorAutomation().AddStep_AddUIChild("CreditsOverlay", "CreditsTitleText");

	g_xEngine.EditorAutomation().AddStep_CreateUIText("CreditsLine1", "Built with Zenith Engine");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("CreditsLine1", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("CreditsLine1", 0.f, 80.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("CreditsLine1", TilePuzzleUI::fCREDITS_LINE_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("CreditsLine1", 0.8f, 0.8f, 0.9f, 1.f);
	g_xEngine.EditorAutomation().AddStep_AddUIChild("CreditsOverlay", "CreditsLine1");

	g_xEngine.EditorAutomation().AddStep_CreateUIText("CreditsLine2", "A Cat Puzzle Game");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("CreditsLine2", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("CreditsLine2", 0.f, 120.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("CreditsLine2", TilePuzzleUI::fCREDITS_LINE_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("CreditsLine2", 0.8f, 0.8f, 0.9f, 1.f);
	g_xEngine.EditorAutomation().AddStep_AddUIChild("CreditsOverlay", "CreditsLine2");

	g_xEngine.EditorAutomation().AddStep_CreateUIText("CreditsDismissText", "Tap anywhere to dismiss");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("CreditsDismissText", static_cast<int>(Zenith_UI::AnchorPreset::BottomCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("CreditsDismissText", 0.f, -20.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("CreditsDismissText", TilePuzzleUI::fCREDITS_LINE_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("CreditsDismissText", 0.6f, 0.6f, 0.6f, 0.7f);
	g_xEngine.EditorAutomation().AddStep_AddUIChild("CreditsOverlay", "CreditsDismissText");

	// Script
	g_xEngine.EditorAutomation().AddStep_AttachScript("TilePuzzle_Behaviour");

	g_xEngine.EditorAutomation().AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	g_xEngine.EditorAutomation().AddStep_UnloadScene();

	// ---- TilePuzzle gameplay scene (build index 1) ----
	g_xEngine.EditorAutomation().AddStep_CreateScene("TilePuzzle");
	g_xEngine.EditorAutomation().AddStep_CreateEntity("GameManager");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraPosition(0.f, 12.f, 0.f);
	g_xEngine.EditorAutomation().AddStep_SetCameraPitch(-1.5f);
	g_xEngine.EditorAutomation().AddStep_SetCameraFOV(glm::radians(45.f));
	g_xEngine.EditorAutomation().AddStep_SetCameraAspect(9.f / 16.f);
	g_xEngine.EditorAutomation().AddStep_SetAsMainCamera();
	g_xEngine.EditorAutomation().AddStep_AddUI();

	// ---- Gameplay HUD (GDD section 7.4) ----

	// Top info group: level number, move counter, cats remaining (centered)
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("HUDInfoGroup");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("HUDInfoGroup", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("HUDInfoGroup", 0.f, 15.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("HUDInfoGroup", static_cast<int>(Zenith_UI::LayoutDirection::Vertical));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("HUDInfoGroup", 4.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("HUDInfoGroup", static_cast<int>(Zenith_UI::ChildAlignment::UpperCenter));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("HUDInfoGroup", true);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("HUDInfoGroup", false);

	g_xEngine.EditorAutomation().AddStep_CreateUIText("LevelText", "Level 1");
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("LevelText", TilePuzzleUI::fHUD_TITLE_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("LevelText", 1.f, 1.f, 1.f, 1.f);

	g_xEngine.EditorAutomation().AddStep_CreateUIText("MovesText", "Moves: 0");
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("MovesText", TilePuzzleUI::fHUD_BODY_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("MovesText", 0.6f, 0.8f, 1.f, 1.f);

	g_xEngine.EditorAutomation().AddStep_CreateUIText("CatsText", "Cats: 0 / 3");
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("CatsText", TilePuzzleUI::fHUD_BODY_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("CatsText", 0.6f, 0.8f, 1.f, 1.f);

	g_xEngine.EditorAutomation().AddStep_AddUIChild("HUDInfoGroup", "LevelText");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("HUDInfoGroup", "MovesText");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("HUDInfoGroup", "CatsText");

	// Coin display (top-right): icon + coin count
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("HUDCoinGroup");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("HUDCoinGroup", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("HUDCoinGroup", -15.f, 15.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("HUDCoinGroup", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("HUDCoinGroup", 6.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("HUDCoinGroup", static_cast<int>(Zenith_UI::ChildAlignment::MiddleCenter));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("HUDCoinGroup", true);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("HUDCoinGroup", false);

	g_xEngine.EditorAutomation().AddStep_CreateUIImage("HUDCoinIcon");
	g_xEngine.EditorAutomation().AddStep_SetUISize("HUDCoinIcon", TilePuzzleUI::fICON_SIZE, TilePuzzleUI::fICON_SIZE);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("HUDCoinIcon", 1.f, 0.85f, 0.2f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIImageTexturePath("HUDCoinIcon",
		GAME_ASSETS_DIR "Textures/Icons/coin" ZENITH_TEXTURE_EXT);

	g_xEngine.EditorAutomation().AddStep_CreateUIText("HUDCoinsText", "0");
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("HUDCoinsText", TilePuzzleUI::fHUD_BODY_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("HUDCoinsText", 1.f, 0.85f, 0.2f, 1.f);

	g_xEngine.EditorAutomation().AddStep_AddUIChild("HUDCoinGroup", "HUDCoinIcon");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("HUDCoinGroup", "HUDCoinsText");

	// Bottom action buttons (GDD: Reset, Undo, Hint, Skip, Menu)
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("HUDButtonGroup");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("HUDButtonGroup", static_cast<int>(Zenith_UI::AnchorPreset::BottomCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("HUDButtonGroup", 0.f, -15.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("HUDButtonGroup", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("HUDButtonGroup", 8.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("HUDButtonGroup", static_cast<int>(Zenith_UI::ChildAlignment::MiddleCenter));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("HUDButtonGroup", true);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("HUDButtonGroup", false);

	g_xEngine.EditorAutomation().AddStep_CreateUIButton("MenuBtn", "Menu");
	g_xEngine.EditorAutomation().AddStep_SetUISize("MenuBtn", TilePuzzleUI::fHUD_BTN_W, TilePuzzleUI::fHUD_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("MenuBtn", TilePuzzleUI::fHUD_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("MenuBtn", 0.2f, 0.25f, 0.35f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("MenuBtn", 0.3f, 0.35f, 0.5f, 1.f);

	g_xEngine.EditorAutomation().AddStep_CreateUIButton("ResetBtn", "Reset");
	g_xEngine.EditorAutomation().AddStep_SetUISize("ResetBtn", TilePuzzleUI::fHUD_BTN_W, TilePuzzleUI::fHUD_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("ResetBtn", TilePuzzleUI::fHUD_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("ResetBtn", 0.2f, 0.25f, 0.35f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("ResetBtn", 0.3f, 0.35f, 0.5f, 1.f);

	g_xEngine.EditorAutomation().AddStep_CreateUIButton("UndoBtn", "Undo");
	g_xEngine.EditorAutomation().AddStep_SetUISize("UndoBtn", TilePuzzleUI::fHUD_BTN_W, TilePuzzleUI::fHUD_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("UndoBtn", TilePuzzleUI::fHUD_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("UndoBtn", 0.2f, 0.25f, 0.35f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("UndoBtn", 0.3f, 0.35f, 0.5f, 1.f);

	g_xEngine.EditorAutomation().AddStep_CreateUIButton("HintBtn", "Hint");
	g_xEngine.EditorAutomation().AddStep_SetUISize("HintBtn", TilePuzzleUI::fHUD_BTN_W, TilePuzzleUI::fHUD_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("HintBtn", TilePuzzleUI::fHUD_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("HintBtn", 0.2f, 0.25f, 0.35f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("HintBtn", 0.3f, 0.35f, 0.5f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonIcon("HintBtn",
		GAME_ASSETS_DIR "Textures/Icons/hint_token" ZENITH_TEXTURE_EXT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonIconSize("HintBtn", TilePuzzleUI::fHUD_BTN_ICON, TilePuzzleUI::fHUD_BTN_ICON);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonIconPlacement("HintBtn",
		static_cast<int>(Zenith_UI::Zenith_UIButton::IconPlacement::LEFT));

	g_xEngine.EditorAutomation().AddStep_CreateUIButton("SkipBtn", "Skip");
	g_xEngine.EditorAutomation().AddStep_SetUISize("SkipBtn", TilePuzzleUI::fHUD_BTN_W, TilePuzzleUI::fHUD_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("SkipBtn", TilePuzzleUI::fHUD_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("SkipBtn", 0.5f, 0.15f, 0.15f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("SkipBtn", 0.65f, 0.2f, 0.2f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("SkipBtn", false);

	g_xEngine.EditorAutomation().AddStep_AddUIChild("HUDButtonGroup", "MenuBtn");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("HUDButtonGroup", "ResetBtn");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("HUDButtonGroup", "UndoBtn");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("HUDButtonGroup", "HintBtn");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("HUDButtonGroup", "SkipBtn");

	// HUD buttons focus navigation (horizontal)
	g_xEngine.EditorAutomation().AddStep_SetUINavigation("MenuBtn", nullptr, nullptr, nullptr, "ResetBtn");
	g_xEngine.EditorAutomation().AddStep_SetUINavigation("ResetBtn", nullptr, nullptr, "MenuBtn", "UndoBtn");
	g_xEngine.EditorAutomation().AddStep_SetUINavigation("UndoBtn", nullptr, nullptr, "ResetBtn", "HintBtn");
	g_xEngine.EditorAutomation().AddStep_SetUINavigation("HintBtn", nullptr, nullptr, "UndoBtn", "SkipBtn");
	g_xEngine.EditorAutomation().AddStep_SetUINavigation("SkipBtn", nullptr, nullptr, "HintBtn", nullptr);

	// ---- Confirm Dialog Overlay (gameplay scene) ----
	g_xEngine.EditorAutomation().AddStep_CreateUIOverlay("ConfirmOverlay");
	g_xEngine.EditorAutomation().AddStep_SetUIOverlayDimColor("ConfirmOverlay", 0.f, 0.f, 0.f, 0.7f);
	g_xEngine.EditorAutomation().AddStep_SetUIOverlayContentSize("ConfirmOverlay", TilePuzzleUI::fCONFIRM_OVERLAY_W, TilePuzzleUI::fCONFIRM_OVERLAY_H);

	g_xEngine.EditorAutomation().AddStep_CreateUIText("ConfirmText", "Are you sure?");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("ConfirmText", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("ConfirmText", 0.f, 30.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("ConfirmText", TilePuzzleUI::fCONFIRM_TEXT_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("ConfirmText", 1.f, 1.f, 0.9f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("ConfirmText", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_AddUIChild("ConfirmOverlay", "ConfirmText");

	g_xEngine.EditorAutomation().AddStep_CreateUIButton("ConfirmCancelBtn", "Cancel");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("ConfirmCancelBtn", static_cast<int>(Zenith_UI::AnchorPreset::BottomLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("ConfirmCancelBtn", 30.f, -20.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("ConfirmCancelBtn", TilePuzzleUI::fOVERLAY_BTN_W, TilePuzzleUI::fOVERLAY_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("ConfirmCancelBtn", TilePuzzleUI::fOVERLAY_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("ConfirmCancelBtn", 0.25f, 0.25f, 0.3f, 0.9f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("ConfirmCancelBtn", 0.35f, 0.35f, 0.42f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonPressedColor("ConfirmCancelBtn", 0.18f, 0.18f, 0.22f, 1.f);
	g_xEngine.EditorAutomation().AddStep_AddUIChild("ConfirmOverlay", "ConfirmCancelBtn");

	g_xEngine.EditorAutomation().AddStep_CreateUIButton("ConfirmAcceptBtn", "Accept");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("ConfirmAcceptBtn", static_cast<int>(Zenith_UI::AnchorPreset::BottomRight));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("ConfirmAcceptBtn", -30.f, -20.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("ConfirmAcceptBtn", TilePuzzleUI::fOVERLAY_BTN_W, TilePuzzleUI::fOVERLAY_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("ConfirmAcceptBtn", TilePuzzleUI::fOVERLAY_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("ConfirmAcceptBtn", 0.5f, 0.15f, 0.15f, 0.9f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("ConfirmAcceptBtn", 0.65f, 0.25f, 0.25f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonPressedColor("ConfirmAcceptBtn", 0.35f, 0.1f, 0.1f, 1.f);
	g_xEngine.EditorAutomation().AddStep_AddUIChild("ConfirmOverlay", "ConfirmAcceptBtn");

	// ---- Tutorial Overlay (gameplay scene) ----
	g_xEngine.EditorAutomation().AddStep_CreateUIOverlay("TutorialOverlay");
	g_xEngine.EditorAutomation().AddStep_SetUIOverlayDimColor("TutorialOverlay", 0.f, 0.f, 0.f, 0.7f);
	g_xEngine.EditorAutomation().AddStep_SetUIOverlayContentSize("TutorialOverlay", TilePuzzleUI::fTUTORIAL_OVERLAY_W, TilePuzzleUI::fTUTORIAL_OVERLAY_H);

	g_xEngine.EditorAutomation().AddStep_CreateUIText("TutorialText", " ");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("TutorialText", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("TutorialText", 0.f, 20.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("TutorialText", 750.f, 100.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("TutorialText", TilePuzzleUI::fTUTORIAL_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("TutorialText", 1.f, 1.f, 0.8f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("TutorialText", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_AddUIChild("TutorialOverlay", "TutorialText");

	g_xEngine.EditorAutomation().AddStep_CreateUIText("TutorialHintText", "Tap to continue");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("TutorialHintText", static_cast<int>(Zenith_UI::AnchorPreset::BottomCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("TutorialHintText", 0.f, -15.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("TutorialHintText", 400.f, 40.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("TutorialHintText", TilePuzzleUI::fTUTORIAL_HINT_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("TutorialHintText", 0.7f, 0.7f, 0.7f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("TutorialHintText", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_AddUIChild("TutorialOverlay", "TutorialHintText");

	// ---- Victory Overlay UI elements (starts hidden) ----

	// Victory background
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("VictoryBg");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("VictoryBg", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("VictoryBg", 0.f, 0.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("VictoryBg", TilePuzzleUI::fVICTORY_BG_W, TilePuzzleUI::fVICTORY_BG_H);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("VictoryBg", 0.05f, 0.05f, 0.15f, 0.9f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("VictoryBg", false);

	// Victory title
	g_xEngine.EditorAutomation().AddStep_CreateUIText("VictoryTitle", "Level Complete!");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("VictoryTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("VictoryTitle", 0.f, TilePuzzleUI::fVICTORY_TITLE_Y);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("VictoryTitle", TilePuzzleUI::fVICTORY_TITLE_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("VictoryTitle", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIColor("VictoryTitle", 1.f, 1.f, 0.5f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("VictoryTitle", false);

	// Victory stars
	g_xEngine.EditorAutomation().AddStep_CreateUIText("VictoryStars", "- - -");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("VictoryStars", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("VictoryStars", 0.f, TilePuzzleUI::fVICTORY_STARS_Y);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("VictoryStars", TilePuzzleUI::fVICTORY_STARS_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("VictoryStars", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIColor("VictoryStars", 1.f, 0.85f, 0.1f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("VictoryStars", false);

	// Victory content vertical layout group (holds stars, text, coins)
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("VictoryContentGroup");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("VictoryContentGroup", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("VictoryContentGroup", 0.f, 20.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("VictoryContentGroup", TilePuzzleUI::fVICTORY_CONTENT_W, TilePuzzleUI::fVICTORY_CONTENT_H);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("VictoryContentGroup", static_cast<int>(Zenith_UI::LayoutDirection::Vertical));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("VictoryContentGroup", TilePuzzleUI::fVICTORY_CONTENT_SPACING);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("VictoryContentGroup", static_cast<int>(Zenith_UI::ChildAlignment::UpperCenter));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildForceExpand("VictoryContentGroup", true, false);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("VictoryContentGroup", false);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("VictoryContentGroup", false);

	// Victory star images layout group (3 stars for rating display)
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("VictoryStarGroup");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("VictoryStarGroup", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("VictoryStarGroup", TilePuzzleUI::fVICTORY_STAR_SPACING);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("VictoryStarGroup", static_cast<int>(Zenith_UI::ChildAlignment::MiddleCenter));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("VictoryStarGroup", true);

	{
		static const char* s_aszVictoryStarNames[] = { "VictoryStar0", "VictoryStar1", "VictoryStar2" };
		for (uint32_t u = 0; u < 3; ++u)
		{
			g_xEngine.EditorAutomation().AddStep_CreateUIImage(s_aszVictoryStarNames[u]);
			g_xEngine.EditorAutomation().AddStep_SetUISize(s_aszVictoryStarNames[u], TilePuzzleUI::fVICTORY_STAR_SIZE, TilePuzzleUI::fVICTORY_STAR_SIZE);
			g_xEngine.EditorAutomation().AddStep_SetUIColor(s_aszVictoryStarNames[u], 1.f, 0.85f, 0.1f, 1.f);
			g_xEngine.EditorAutomation().AddStep_SetUIImageTexturePath(s_aszVictoryStarNames[u],
				GAME_ASSETS_DIR "Textures/Icons/star_empty" ZENITH_TEXTURE_EXT);
			g_xEngine.EditorAutomation().AddStep_AddUIChild("VictoryStarGroup", s_aszVictoryStarNames[u]);
		}
	}
	g_xEngine.EditorAutomation().AddStep_AddUIChild("VictoryContentGroup", "VictoryStarGroup");

	// Victory cat text
	g_xEngine.EditorAutomation().AddStep_CreateUIText("VictoryCatText", "Cat rescued!");
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("VictoryCatText", TilePuzzleUI::fVICTORY_CAT_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("VictoryCatText", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIColor("VictoryCatText", 0.9f, 0.7f, 0.5f, 1.f);
	g_xEngine.EditorAutomation().AddStep_AddUIChild("VictoryContentGroup", "VictoryCatText");

	// Victory coins text
	g_xEngine.EditorAutomation().AddStep_CreateUIText("VictoryCoinsText", "+10 coins");
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("VictoryCoinsText", TilePuzzleUI::fVICTORY_COINS_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("VictoryCoinsText", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIColor("VictoryCoinsText", 1.f, 0.85f, 0.2f, 1.f);
	g_xEngine.EditorAutomation().AddStep_AddUIChild("VictoryContentGroup", "VictoryCoinsText");

	// Next Level button (created last so it renders on top of VictoryBg overlay)
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("NextLevelBtn", "Next Level");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("NextLevelBtn", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("NextLevelBtn", 0.f, TilePuzzleUI::fNEXT_LEVEL_BTN_Y);
	g_xEngine.EditorAutomation().AddStep_SetUISize("NextLevelBtn", TilePuzzleUI::fNEXT_LEVEL_BTN_W, TilePuzzleUI::fNEXT_LEVEL_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("NextLevelBtn", TilePuzzleUI::fNEXT_LEVEL_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("NextLevelBtn", 0.15f, 0.4f, 0.2f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("NextLevelBtn", 0.25f, 0.55f, 0.3f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonPressedColor("NextLevelBtn", 0.1f, 0.3f, 0.15f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("NextLevelBtn", false);

	// Elimination particle emitter
	g_xEngine.EditorAutomation().AddStep_CreateEntity("EliminationEmitter");
	g_xEngine.EditorAutomation().AddStep_AddParticleEmitter();
	g_xEngine.EditorAutomation().AddStep_SetParticleConfigByName("Elimination");
	g_xEngine.EditorAutomation().AddStep_SetParticleEmitting(false);

	// Victory confetti particle emitter
	g_xEngine.EditorAutomation().AddStep_CreateEntity("VictoryConfettiEmitter");
	g_xEngine.EditorAutomation().AddStep_SetTransformPosition(0.f, 8.f, 0.f);
	g_xEngine.EditorAutomation().AddStep_AddParticleEmitter();
	g_xEngine.EditorAutomation().AddStep_SetParticleConfigByName("VictoryConfetti");
	g_xEngine.EditorAutomation().AddStep_SetParticleEmitting(false);

	// Re-select GameManager for the script step
	g_xEngine.EditorAutomation().AddStep_SelectEntity("GameManager");

	// Script
	g_xEngine.EditorAutomation().AddStep_AttachScript("TilePuzzle_Behaviour");

	g_xEngine.EditorAutomation().AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/TilePuzzle" ZENITH_SCENE_EXT);
	g_xEngine.EditorAutomation().AddStep_UnloadScene();

	// ---- Pinball scene (build index 2) ----
	g_xEngine.EditorAutomation().AddStep_CreateScene("Pinball");
	g_xEngine.EditorAutomation().AddStep_CreateEntity("PinballManager");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraPosition(0.f, 4.f, -12.f);
	g_xEngine.EditorAutomation().AddStep_SetCameraPitch(0.f);
	g_xEngine.EditorAutomation().AddStep_SetCameraFOV(glm::radians(45.f));
	g_xEngine.EditorAutomation().AddStep_SetCameraAspect(9.f / 16.f);
	g_xEngine.EditorAutomation().AddStep_SetAsMainCamera();
	g_xEngine.EditorAutomation().AddStep_AddUI();

	// Pinball score layout group (score + high score, vertical stack)
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("PinballScoreGroup");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("PinballScoreGroup", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("PinballScoreGroup", -30.f, TilePuzzleUI::fPB_TOP_PADDING + 30.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("PinballScoreGroup", static_cast<int>(Zenith_UI::LayoutDirection::Vertical));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("PinballScoreGroup", 0.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("PinballScoreGroup", static_cast<int>(Zenith_UI::ChildAlignment::UpperRight));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("PinballScoreGroup", true);

	g_xEngine.EditorAutomation().AddStep_CreateUIText("PinballScore", "Score: 0");
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("PinballScore", TilePuzzleUI::fPB_SCORE_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("PinballScore", 1.f, 1.f, 1.f, 1.f);

	g_xEngine.EditorAutomation().AddStep_CreateUIText("PinballHighScore", "Total: 0");
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("PinballHighScore", TilePuzzleUI::fPB_HIGH_SCORE_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("PinballHighScore", 0.7f, 0.7f, 0.8f, 1.f);

	g_xEngine.EditorAutomation().AddStep_AddUIChild("PinballScoreGroup", "PinballScore");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("PinballScoreGroup", "PinballHighScore");

	// Back button
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("PinballBackBtn", "Menu");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("PinballBackBtn", static_cast<int>(Zenith_UI::AnchorPreset::BottomLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("PinballBackBtn", 20.f, -20.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("PinballBackBtn", TilePuzzleUI::fPB_BACK_BTN_W, TilePuzzleUI::fPB_BACK_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("PinballBackBtn", TilePuzzleUI::fPB_BACK_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("PinballBackBtn", 0.2f, 0.25f, 0.35f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("PinballBackBtn", 0.3f, 0.35f, 0.5f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonPressedColor("PinballBackBtn", 0.12f, 0.15f, 0.25f, 1.f);

	// Launch hint
	g_xEngine.EditorAutomation().AddStep_CreateUIText("PinballLaunchHint", "Drag plunger to launch");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("PinballLaunchHint", static_cast<int>(Zenith_UI::AnchorPreset::BottomCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("PinballLaunchHint", 0.f, -30.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("PinballLaunchHint", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("PinballLaunchHint", TilePuzzleUI::fPB_LAUNCH_HINT_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("PinballLaunchHint", 0.6f, 0.6f, 0.7f, 1.f);

	// ---- Gate Select Screen (replaces raw SubmitQuad/SubmitText rendering) ----
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("GateSelectBg");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("GateSelectBg", static_cast<int>(Zenith_UI::AnchorPreset::StretchAll));
	g_xEngine.EditorAutomation().AddStep_SetUIColor("GateSelectBg", 0.08f, 0.08f, 0.15f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("GateSelectBg", false);

	g_xEngine.EditorAutomation().AddStep_CreateUIText("GateSelectTitle", "Select Gate");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("GateSelectTitle", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("GateSelectTitle", 0.f, 40.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("GateSelectTitle", TilePuzzleUI::fGATE_SELECT_TITLE_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("GateSelectTitle", 1.f, 0.9f, 0.5f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("GateSelectTitle", false);

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
		g_xEngine.EditorAutomation().AddStep_CreateUIButton(s_aszGateBtnNames[i], s_aszGateBtnLabels[i]);
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor(s_aszGateBtnNames[i], static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));

		int iCol = i % 5;
		int iRow = i / 5;
		float fBtnW = TilePuzzleUI::fGATE_BTN_SIZE;
		float fBtnH = TilePuzzleUI::fGATE_BTN_SIZE;
		float fGap = TilePuzzleUI::fGATE_BTN_GAP;
		float fGridW = 5.f * fBtnW + 4.f * fGap;
		float fOffsetX = -fGridW * 0.5f + static_cast<float>(iCol) * (fBtnW + fGap) + fBtnW * 0.5f;
		float fOffsetY = 100.f + static_cast<float>(iRow) * (fBtnH + fGap);

		g_xEngine.EditorAutomation().AddStep_SetUIPosition(s_aszGateBtnNames[i], fOffsetX, fOffsetY);
		g_xEngine.EditorAutomation().AddStep_SetUISize(s_aszGateBtnNames[i], fBtnW, fBtnH);
		g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize(s_aszGateBtnNames[i], TilePuzzleUI::fGATE_BTN_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor(s_aszGateBtnNames[i], 0.3f, 0.3f, 0.3f, 1.f);
		g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor(s_aszGateBtnNames[i], 0.4f, 0.4f, 0.5f, 1.f);
		g_xEngine.EditorAutomation().AddStep_SetUIButtonPressedColor(s_aszGateBtnNames[i], 0.2f, 0.2f, 0.25f, 1.f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible(s_aszGateBtnNames[i], false);
	}

	// Freeplay button
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("GateFreeplayBtn", "Freeplay");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("GateFreeplayBtn", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("GateFreeplayBtn", 0.f, 310.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("GateFreeplayBtn", TilePuzzleUI::fGATE_ACTION_BTN_W, TilePuzzleUI::fGATE_ACTION_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("GateFreeplayBtn", TilePuzzleUI::fGATE_ACTION_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("GateFreeplayBtn", 0.5f, 0.3f, 0.6f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("GateFreeplayBtn", 0.6f, 0.4f, 0.7f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonPressedColor("GateFreeplayBtn", 0.35f, 0.2f, 0.45f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("GateFreeplayBtn", false);

	// Gate select back button
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("GateBackBtn", "Back");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("GateBackBtn", static_cast<int>(Zenith_UI::AnchorPreset::BottomCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("GateBackBtn", 0.f, -35.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("GateBackBtn", TilePuzzleUI::fGATE_BACK_BTN_W, TilePuzzleUI::fGATE_BACK_BTN_H);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("GateBackBtn", TilePuzzleUI::fGATE_ACTION_BTN_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("GateBackBtn", 0.4f, 0.2f, 0.2f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("GateBackBtn", 0.55f, 0.3f, 0.3f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonPressedColor("GateBackBtn", 0.3f, 0.15f, 0.15f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("GateBackBtn", false);

	// ---- Tutorial Overlay (pinball scene) ----
	g_xEngine.EditorAutomation().AddStep_CreateUIOverlay("TutorialOverlay");
	g_xEngine.EditorAutomation().AddStep_SetUIOverlayDimColor("TutorialOverlay", 0.f, 0.f, 0.f, 0.7f);
	g_xEngine.EditorAutomation().AddStep_SetUIOverlayContentSize("TutorialOverlay", TilePuzzleUI::fTUTORIAL_OVERLAY_W, TilePuzzleUI::fTUTORIAL_OVERLAY_H);

	g_xEngine.EditorAutomation().AddStep_CreateUIText("TutorialText", " ");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("TutorialText", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("TutorialText", 0.f, 20.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("TutorialText", 750.f, 100.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("TutorialText", TilePuzzleUI::fTUTORIAL_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("TutorialText", 1.f, 1.f, 0.8f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("TutorialText", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_AddUIChild("TutorialOverlay", "TutorialText");

	g_xEngine.EditorAutomation().AddStep_CreateUIText("TutorialHintText", "Tap to continue");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("TutorialHintText", static_cast<int>(Zenith_UI::AnchorPreset::BottomCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("TutorialHintText", 0.f, -15.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("TutorialHintText", 400.f, 40.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("TutorialHintText", TilePuzzleUI::fTUTORIAL_HINT_FONT);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("TutorialHintText", 0.7f, 0.7f, 0.7f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("TutorialHintText", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_AddUIChild("TutorialOverlay", "TutorialHintText");

	// Script
	g_xEngine.EditorAutomation().AddStep_AttachScript("Pinball_Behaviour");

	g_xEngine.EditorAutomation().AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Pinball" ZENITH_SCENE_EXT);
	g_xEngine.EditorAutomation().AddStep_UnloadScene();

	// ---- Final scene loading ----
	g_xEngine.EditorAutomation().AddStep_LoadInitialScene(&Project_LoadInitialScene);
}
#endif

void Project_LoadInitialScene()
{
	g_xEngine.SceneRegistry().RegisterSceneBuildIndex(0, GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	g_xEngine.SceneRegistry().RegisterSceneBuildIndex(1, GAME_ASSETS_DIR "Scenes/TilePuzzle" ZENITH_SCENE_EXT);
	g_xEngine.SceneRegistry().RegisterSceneBuildIndex(2, GAME_ASSETS_DIR "Scenes/Pinball" ZENITH_SCENE_EXT);
	g_xEngine.SceneOperations().LoadSceneByIndexBlockingForBootstrap(0, SCENE_LOAD_SINGLE);

#ifdef ZENITH_INPUT_SIMULATOR
	if (TilePuzzle_HasAutoTestFlag())
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "====================================");
		Zenith_Log(LOG_CATEGORY_UNITTEST, "TilePuzzle --autotest mode enabled");
		Zenith_Log(LOG_CATEGORY_UNITTEST, "====================================");

		// Create an entity in the active scene with the autotest behaviour
		Zenith_Scene xScene = g_xEngine.SceneRegistry().GetActiveScene();
		Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xScene);

		Zenith_Entity xTestEntity(pxSceneData, "AutoTestRunner");
		Zenith_SceneEntityOwnership::MarkEntityPersistent(xTestEntity);
		Zenith_ScriptComponent& xScript = xTestEntity.AddComponent<Zenith_ScriptComponent>();
		xScript.AddScript<TilePuzzle_AutoTest>();

#ifdef ZENITH_TOOLS
		// Switch editor to Playing mode so SceneManager::Update runs
		// (Stopped mode skips scene updates, preventing OnStart/OnUpdate)
		g_xEngine.Editor().SetEditorMode(EditorMode::Playing);
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
