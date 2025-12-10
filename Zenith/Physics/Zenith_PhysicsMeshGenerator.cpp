#include "Zenith.h"
#include "EntityComponent/Components/Zenith_PhysicsMeshGenerator.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_Scene.h"
#include "Flux/Primitives/Flux_Primitives.h"
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cmath>

// Global physics mesh configuration
PhysicsMeshConfig g_xPhysicsMeshConfig;

// Global debug flag for drawing all physics meshes
bool g_bDebugDrawAllPhysicsMeshes = false;

// Log tag for physics mesh generation
static constexpr const char* LOG_TAG_PHYSICS_MESH = "[PhysicsMeshGen]";

const char* Zenith_PhysicsMeshGenerator::GetQualityName(PhysicsMeshQuality eQuality)
{
	switch (eQuality)
	{
	case PHYSICS_MESH_QUALITY_LOW:    return "LOW (AABB)";
	case PHYSICS_MESH_QUALITY_MEDIUM: return "MEDIUM (ConvexHull)";
	case PHYSICS_MESH_QUALITY_HIGH:   return "HIGH (SimplifiedMesh)";
	default:                          return "UNKNOWN";
	}
}

Flux_MeshGeometry* Zenith_PhysicsMeshGenerator::GeneratePhysicsMesh(
	const Zenith_Vector<Flux_MeshGeometry*>& xMeshGeometries,
	PhysicsMeshQuality eQuality)
{
	PhysicsMeshConfig xConfig = g_xPhysicsMeshConfig;
	xConfig.m_eQuality = eQuality;
	return GeneratePhysicsMeshWithConfig(xMeshGeometries, xConfig);
}

Flux_MeshGeometry* Zenith_PhysicsMeshGenerator::GeneratePhysicsMeshWithConfig(
	const Zenith_Vector<Flux_MeshGeometry*>& xMeshGeometries,
	const PhysicsMeshConfig& xConfig)
{
	// Validate input
	if (xMeshGeometries.GetSize() == 0)
	{
		Zenith_Log("%s No meshes provided for physics mesh generation", LOG_TAG_PHYSICS_MESH);
		return nullptr;
	}

	// Count total triangles and vertices for logging
	uint32_t uTotalSourceVerts = 0;
	uint32_t uTotalSourceTris = 0;
	for (uint32_t i = 0; i < xMeshGeometries.GetSize(); i++)
	{
		const Flux_MeshGeometry* pxMesh = xMeshGeometries.Get(i);
		if (pxMesh && pxMesh->m_pxPositions)
		{
			uTotalSourceVerts += pxMesh->GetNumVerts();
			uTotalSourceTris += pxMesh->GetNumIndices() / 3;
		}
	}

	Zenith_Log("%s Generating physics mesh from %u submeshes (%u verts, %u tris), quality=%s",
		LOG_TAG_PHYSICS_MESH,
		xMeshGeometries.GetSize(),
		uTotalSourceVerts,
		uTotalSourceTris,
		GetQualityName(xConfig.m_eQuality));

	Flux_MeshGeometry* pxResult = nullptr;

	switch (xConfig.m_eQuality)
	{
	case PHYSICS_MESH_QUALITY_LOW:
		pxResult = GenerateAABBMesh(xMeshGeometries);
		break;

	case PHYSICS_MESH_QUALITY_MEDIUM:
		pxResult = GenerateConvexHullMesh(xMeshGeometries);
		break;

	case PHYSICS_MESH_QUALITY_HIGH:
		pxResult = GenerateSimplifiedMesh(xMeshGeometries, xConfig);
		break;

	default:
		Zenith_Log("%s Unknown quality level %d, falling back to AABB", LOG_TAG_PHYSICS_MESH, xConfig.m_eQuality);
		pxResult = GenerateAABBMesh(xMeshGeometries);
		break;
	}

	if (pxResult)
	{
		Zenith_Log("%s Generated physics mesh: %u verts, %u tris",
			LOG_TAG_PHYSICS_MESH,
			pxResult->GetNumVerts(),
			pxResult->GetNumIndices() / 3);
	}
	else
	{
		Zenith_Log("%s Failed to generate physics mesh, attempting AABB fallback", LOG_TAG_PHYSICS_MESH);
		pxResult = GenerateAABBMesh(xMeshGeometries);
		if (pxResult)
		{
			Zenith_Log("%s AABB fallback succeeded: %u verts, %u tris",
				LOG_TAG_PHYSICS_MESH,
				pxResult->GetNumVerts(),
				pxResult->GetNumIndices() / 3);
		}
	}

	return pxResult;
}

void Zenith_PhysicsMeshGenerator::DebugDrawPhysicsMesh(
	const Flux_MeshGeometry* pxPhysicsMesh,
	const Zenith_Maths::Matrix4& xTransform,
	const Zenith_Maths::Vector3& xColor)
{
	if (!pxPhysicsMesh || !pxPhysicsMesh->m_pxPositions || pxPhysicsMesh->GetNumIndices() < 3)
	{
		return;
	}

	// Draw wireframe triangles using lines
	const uint32_t uNumTris = pxPhysicsMesh->GetNumIndices() / 3;
	
	for (uint32_t t = 0; t < uNumTris; t++)
	{
		uint32_t uIdx0 = pxPhysicsMesh->m_puIndices[t * 3 + 0];
		uint32_t uIdx1 = pxPhysicsMesh->m_puIndices[t * 3 + 1];
		uint32_t uIdx2 = pxPhysicsMesh->m_puIndices[t * 3 + 2];

		// Get positions and transform to world space
		Zenith_Maths::Vector4 xPos0 = xTransform * Zenith_Maths::Vector4(pxPhysicsMesh->m_pxPositions[uIdx0], 1.0f);
		Zenith_Maths::Vector4 xPos1 = xTransform * Zenith_Maths::Vector4(pxPhysicsMesh->m_pxPositions[uIdx1], 1.0f);
		Zenith_Maths::Vector4 xPos2 = xTransform * Zenith_Maths::Vector4(pxPhysicsMesh->m_pxPositions[uIdx2], 1.0f);

		Zenith_Maths::Vector3 xV0(xPos0.x, xPos0.y, xPos0.z);
		Zenith_Maths::Vector3 xV1(xPos1.x, xPos1.y, xPos1.z);
		Zenith_Maths::Vector3 xV2(xPos2.x, xPos2.y, xPos2.z);

		// Draw the three edges of the triangle
		Flux_Primitives::AddLine(xV0, xV1, xColor, 0.05f);
		Flux_Primitives::AddLine(xV1, xV2, xColor, 0.05f);
		Flux_Primitives::AddLine(xV2, xV0, xColor, 0.05f);
	}
}

void Zenith_PhysicsMeshGenerator::ComputeAABB(
	const Zenith_Vector<Flux_MeshGeometry*>& xMeshGeometries,
	Zenith_Maths::Vector3& xMinOut,
	Zenith_Maths::Vector3& xMaxOut)
{
	xMinOut = Zenith_Maths::Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
	xMaxOut = Zenith_Maths::Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (uint32_t m = 0; m < xMeshGeometries.GetSize(); m++)
	{
		const Flux_MeshGeometry* pxMesh = xMeshGeometries.Get(m);
		if (!pxMesh || !pxMesh->m_pxPositions)
		{
			continue;
		}

		for (uint32_t v = 0; v < pxMesh->GetNumVerts(); v++)
		{
			const Zenith_Maths::Vector3& xPos = pxMesh->m_pxPositions[v];
			xMinOut.x = std::min(xMinOut.x, xPos.x);
			xMinOut.y = std::min(xMinOut.y, xPos.y);
			xMinOut.z = std::min(xMinOut.z, xPos.z);
			xMaxOut.x = std::max(xMaxOut.x, xPos.x);
			xMaxOut.y = std::max(xMaxOut.y, xPos.y);
			xMaxOut.z = std::max(xMaxOut.z, xPos.z);
		}
	}
}

void Zenith_PhysicsMeshGenerator::CollectAllPositions(
	const Zenith_Vector<Flux_MeshGeometry*>& xMeshGeometries,
	Zenith_Vector<Zenith_Maths::Vector3>& xPositionsOut)
{
	xPositionsOut.Clear();

	for (uint32_t m = 0; m < xMeshGeometries.GetSize(); m++)
	{
		const Flux_MeshGeometry* pxMesh = xMeshGeometries.Get(m);
		if (!pxMesh || !pxMesh->m_pxPositions)
		{
			continue;
		}

		for (uint32_t v = 0; v < pxMesh->GetNumVerts(); v++)
		{
			xPositionsOut.PushBack(pxMesh->m_pxPositions[v]);
		}
	}
}

Flux_MeshGeometry* Zenith_PhysicsMeshGenerator::CreateBoxMesh(
	const Zenith_Maths::Vector3& xMin,
	const Zenith_Maths::Vector3& xMax)
{
	Flux_MeshGeometry* pxMesh = new Flux_MeshGeometry();

	// 8 corners of the box
	pxMesh->m_uNumVerts = 8;
	pxMesh->m_pxPositions = new Zenith_Maths::Vector3[8];
	pxMesh->m_pxNormals = new Zenith_Maths::Vector3[8];

	// Define 8 corners (back-bottom-left to front-top-right)
	pxMesh->m_pxPositions[0] = Zenith_Maths::Vector3(xMin.x, xMin.y, xMin.z); // 0: BBL
	pxMesh->m_pxPositions[1] = Zenith_Maths::Vector3(xMax.x, xMin.y, xMin.z); // 1: BBR
	pxMesh->m_pxPositions[2] = Zenith_Maths::Vector3(xMax.x, xMax.y, xMin.z); // 2: BTR
	pxMesh->m_pxPositions[3] = Zenith_Maths::Vector3(xMin.x, xMax.y, xMin.z); // 3: BTL
	pxMesh->m_pxPositions[4] = Zenith_Maths::Vector3(xMin.x, xMin.y, xMax.z); // 4: FBL
	pxMesh->m_pxPositions[5] = Zenith_Maths::Vector3(xMax.x, xMin.y, xMax.z); // 5: FBR
	pxMesh->m_pxPositions[6] = Zenith_Maths::Vector3(xMax.x, xMax.y, xMax.z); // 6: FTR
	pxMesh->m_pxPositions[7] = Zenith_Maths::Vector3(xMin.x, xMax.y, xMax.z); // 7: FTL

	// Normals (pointing outward from center)
	Zenith_Maths::Vector3 xCenter = (xMin + xMax) * 0.5f;
	for (int i = 0; i < 8; i++)
	{
		pxMesh->m_pxNormals[i] = Zenith_Maths::Normalize(pxMesh->m_pxPositions[i] - xCenter);
	}

	// 12 triangles (2 per face, 6 faces)
	pxMesh->m_uNumIndices = 36;
	pxMesh->m_puIndices = new Flux_MeshGeometry::IndexType[36];

	// Back face (-Z)
	pxMesh->m_puIndices[0] = 0; pxMesh->m_puIndices[1] = 2; pxMesh->m_puIndices[2] = 1;
	pxMesh->m_puIndices[3] = 0; pxMesh->m_puIndices[4] = 3; pxMesh->m_puIndices[5] = 2;

	// Front face (+Z)
	pxMesh->m_puIndices[6] = 4; pxMesh->m_puIndices[7] = 5; pxMesh->m_puIndices[8] = 6;
	pxMesh->m_puIndices[9] = 4; pxMesh->m_puIndices[10] = 6; pxMesh->m_puIndices[11] = 7;

	// Left face (-X)
	pxMesh->m_puIndices[12] = 0; pxMesh->m_puIndices[13] = 4; pxMesh->m_puIndices[14] = 7;
	pxMesh->m_puIndices[15] = 0; pxMesh->m_puIndices[16] = 7; pxMesh->m_puIndices[17] = 3;

	// Right face (+X)
	pxMesh->m_puIndices[18] = 1; pxMesh->m_puIndices[19] = 2; pxMesh->m_puIndices[20] = 6;
	pxMesh->m_puIndices[21] = 1; pxMesh->m_puIndices[22] = 6; pxMesh->m_puIndices[23] = 5;

	// Bottom face (-Y)
	pxMesh->m_puIndices[24] = 0; pxMesh->m_puIndices[25] = 1; pxMesh->m_puIndices[26] = 5;
	pxMesh->m_puIndices[27] = 0; pxMesh->m_puIndices[28] = 5; pxMesh->m_puIndices[29] = 4;

	// Top face (+Y)
	pxMesh->m_puIndices[30] = 3; pxMesh->m_puIndices[31] = 7; pxMesh->m_puIndices[32] = 6;
	pxMesh->m_puIndices[33] = 3; pxMesh->m_puIndices[34] = 6; pxMesh->m_puIndices[35] = 2;

	return pxMesh;
}

Flux_MeshGeometry* Zenith_PhysicsMeshGenerator::CreateMeshFromData(
	const Zenith_Vector<Zenith_Maths::Vector3>& xPositions,
	const Zenith_Vector<uint32_t>& xIndices)
{
	if (xPositions.GetSize() == 0 || xIndices.GetSize() < 3)
	{
		return nullptr;
	}

	Flux_MeshGeometry* pxMesh = new Flux_MeshGeometry();

	pxMesh->m_uNumVerts = xPositions.GetSize();
	pxMesh->m_uNumIndices = xIndices.GetSize();

	pxMesh->m_pxPositions = new Zenith_Maths::Vector3[pxMesh->m_uNumVerts];
	pxMesh->m_pxNormals = new Zenith_Maths::Vector3[pxMesh->m_uNumVerts];
	pxMesh->m_puIndices = new Flux_MeshGeometry::IndexType[pxMesh->m_uNumIndices];

	// Copy positions
	for (uint32_t i = 0; i < pxMesh->m_uNumVerts; i++)
	{
		pxMesh->m_pxPositions[i] = xPositions.Get(i);
		pxMesh->m_pxNormals[i] = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f); // Initialize normals
	}

	// Copy indices
	for (uint32_t i = 0; i < pxMesh->m_uNumIndices; i++)
	{
		pxMesh->m_puIndices[i] = xIndices.Get(i);
	}

	// Generate proper normals from triangles
	// First zero out all normals
	for (uint32_t i = 0; i < pxMesh->m_uNumVerts; i++)
	{
		pxMesh->m_pxNormals[i] = Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f);
	}

	// Accumulate face normals to vertices
	for (uint32_t t = 0; t < pxMesh->m_uNumIndices; t += 3)
	{
		uint32_t i0 = pxMesh->m_puIndices[t + 0];
		uint32_t i1 = pxMesh->m_puIndices[t + 1];
		uint32_t i2 = pxMesh->m_puIndices[t + 2];

		Zenith_Maths::Vector3 xV0 = pxMesh->m_pxPositions[i0];
		Zenith_Maths::Vector3 xV1 = pxMesh->m_pxPositions[i1];
		Zenith_Maths::Vector3 xV2 = pxMesh->m_pxPositions[i2];

		Zenith_Maths::Vector3 xEdge1 = xV1 - xV0;
		Zenith_Maths::Vector3 xEdge2 = xV2 - xV0;
		Zenith_Maths::Vector3 xFaceNormal = Zenith_Maths::Cross(xEdge1, xEdge2);

		pxMesh->m_pxNormals[i0] += xFaceNormal;
		pxMesh->m_pxNormals[i1] += xFaceNormal;
		pxMesh->m_pxNormals[i2] += xFaceNormal;
	}

	// Normalize all vertex normals
	for (uint32_t i = 0; i < pxMesh->m_uNumVerts; i++)
	{
		float fLen = Zenith_Maths::Length(pxMesh->m_pxNormals[i]);
		if (fLen > 0.0001f)
		{
			pxMesh->m_pxNormals[i] = pxMesh->m_pxNormals[i] / fLen;
		}
		else
		{
			pxMesh->m_pxNormals[i] = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
		}
	}

	return pxMesh;
}

Flux_MeshGeometry* Zenith_PhysicsMeshGenerator::GenerateAABBMesh(
	const Zenith_Vector<Flux_MeshGeometry*>& xMeshGeometries)
{
	Zenith_Maths::Vector3 xMin, xMax;
	ComputeAABB(xMeshGeometries, xMin, xMax);

	// Validate AABB
	if (xMin.x > xMax.x || xMin.y > xMax.y || xMin.z > xMax.z)
	{
		Zenith_Log("%s Invalid AABB computed, using unit box", LOG_TAG_PHYSICS_MESH);
		xMin = Zenith_Maths::Vector3(-0.5f, -0.5f, -0.5f);
		xMax = Zenith_Maths::Vector3(0.5f, 0.5f, 0.5f);
	}

	Zenith_Log("%s AABB bounds: (%.2f, %.2f, %.2f) to (%.2f, %.2f, %.2f)",
		LOG_TAG_PHYSICS_MESH,
		xMin.x, xMin.y, xMin.z,
		xMax.x, xMax.y, xMax.z);

	return CreateBoxMesh(xMin, xMax);
}

// Simple quickhull-style convex hull implementation
Flux_MeshGeometry* Zenith_PhysicsMeshGenerator::GenerateConvexHullMesh(
	const Zenith_Vector<Flux_MeshGeometry*>& xMeshGeometries)
{
	Zenith_Vector<Zenith_Maths::Vector3> xAllPositions;
	CollectAllPositions(xMeshGeometries, xAllPositions);

	if (xAllPositions.GetSize() < 4)
	{
		Zenith_Log("%s Not enough vertices for convex hull (%u), using AABB fallback",
			LOG_TAG_PHYSICS_MESH, xAllPositions.GetSize());
		return GenerateAABBMesh(xMeshGeometries);
	}

	// For simplicity and robustness, we'll use a simplified approach:
	// 1. Find extreme points in 6 directions (±X, ±Y, ±Z)
	// 2. Build a simple convex polyhedron from these points
	
	// Find extreme points
	Zenith_Maths::Vector3 xMinPt[3], xMaxPt[3];
	int32_t iMinIdx[3] = { 0, 0, 0 };
	int32_t iMaxIdx[3] = { 0, 0, 0 };

	for (int axis = 0; axis < 3; axis++)
	{
		float fMin = FLT_MAX, fMax = -FLT_MAX;
		for (uint32_t i = 0; i < xAllPositions.GetSize(); i++)
		{
			float fVal = (axis == 0) ? xAllPositions.Get(i).x :
			             (axis == 1) ? xAllPositions.Get(i).y :
			                           xAllPositions.Get(i).z;
			if (fVal < fMin) { fMin = fVal; iMinIdx[axis] = i; }
			if (fVal > fMax) { fMax = fVal; iMaxIdx[axis] = i; }
		}
		xMinPt[axis] = xAllPositions.Get(iMinIdx[axis]);
		xMaxPt[axis] = xAllPositions.Get(iMaxIdx[axis]);
	}

	// Collect unique extreme points
	Zenith_Vector<Zenith_Maths::Vector3> xHullPoints;
	auto AddUniquePoint = [&xHullPoints](const Zenith_Maths::Vector3& xPt) {
		for (uint32_t i = 0; i < xHullPoints.GetSize(); i++)
		{
			if (Zenith_Maths::Length(xHullPoints.Get(i) - xPt) < 0.001f)
				return;
		}
		xHullPoints.PushBack(xPt);
	};

	for (int axis = 0; axis < 3; axis++)
	{
		AddUniquePoint(xMinPt[axis]);
		AddUniquePoint(xMaxPt[axis]);
	}

	// If we have fewer than 4 unique points, add some intermediate points
	if (xHullPoints.GetSize() < 4)
	{
		Zenith_Log("%s Only %u unique extreme points, using AABB fallback",
			LOG_TAG_PHYSICS_MESH, xHullPoints.GetSize());
		return GenerateAABBMesh(xMeshGeometries);
	}

	// Build triangles using the extreme points
	// For a robust convex hull, we need to triangulate properly
	// Simple approach: Use AABB-like structure with the extreme points

	Zenith_Maths::Vector3 xMin, xMax;
	ComputeAABB(xMeshGeometries, xMin, xMax);

	// Create a convex hull approximation by using the actual extreme points
	// We'll create an 8-point bounding shape using the extremes
	Zenith_Vector<Zenith_Maths::Vector3> xFinalPositions;
	xFinalPositions.PushBack(Zenith_Maths::Vector3(xMinPt[0].x, xMinPt[1].y, xMinPt[2].z));
	xFinalPositions.PushBack(Zenith_Maths::Vector3(xMaxPt[0].x, xMinPt[1].y, xMinPt[2].z));
	xFinalPositions.PushBack(Zenith_Maths::Vector3(xMaxPt[0].x, xMaxPt[1].y, xMinPt[2].z));
	xFinalPositions.PushBack(Zenith_Maths::Vector3(xMinPt[0].x, xMaxPt[1].y, xMinPt[2].z));
	xFinalPositions.PushBack(Zenith_Maths::Vector3(xMinPt[0].x, xMinPt[1].y, xMaxPt[2].z));
	xFinalPositions.PushBack(Zenith_Maths::Vector3(xMaxPt[0].x, xMinPt[1].y, xMaxPt[2].z));
	xFinalPositions.PushBack(Zenith_Maths::Vector3(xMaxPt[0].x, xMaxPt[1].y, xMaxPt[2].z));
	xFinalPositions.PushBack(Zenith_Maths::Vector3(xMinPt[0].x, xMaxPt[1].y, xMaxPt[2].z));

	// Create indices for a box (same as AABB)
	Zenith_Vector<uint32_t> xIndices;
	// Back face (-Z)
	xIndices.PushBack(0); xIndices.PushBack(2); xIndices.PushBack(1);
	xIndices.PushBack(0); xIndices.PushBack(3); xIndices.PushBack(2);
	// Front face (+Z)
	xIndices.PushBack(4); xIndices.PushBack(5); xIndices.PushBack(6);
	xIndices.PushBack(4); xIndices.PushBack(6); xIndices.PushBack(7);
	// Left face (-X)
	xIndices.PushBack(0); xIndices.PushBack(4); xIndices.PushBack(7);
	xIndices.PushBack(0); xIndices.PushBack(7); xIndices.PushBack(3);
	// Right face (+X)
	xIndices.PushBack(1); xIndices.PushBack(2); xIndices.PushBack(6);
	xIndices.PushBack(1); xIndices.PushBack(6); xIndices.PushBack(5);
	// Bottom face (-Y)
	xIndices.PushBack(0); xIndices.PushBack(1); xIndices.PushBack(5);
	xIndices.PushBack(0); xIndices.PushBack(5); xIndices.PushBack(4);
	// Top face (+Y)
	xIndices.PushBack(3); xIndices.PushBack(7); xIndices.PushBack(6);
	xIndices.PushBack(3); xIndices.PushBack(6); xIndices.PushBack(2);

	Zenith_Log("%s Convex hull approximation: %u vertices, %u triangles",
		LOG_TAG_PHYSICS_MESH, xFinalPositions.GetSize(), xIndices.GetSize() / 3);

	return CreateMeshFromData(xFinalPositions, xIndices);
}

void Zenith_PhysicsMeshGenerator::DecimateVertices(
	const Zenith_Vector<Zenith_Maths::Vector3>& xPositions,
	const Zenith_Vector<uint32_t>& xIndices,
	Zenith_Vector<Zenith_Maths::Vector3>& xPositionsOut,
	Zenith_Vector<uint32_t>& xIndicesOut,
	float fCellSize)
{
	xPositionsOut.Clear();
	xIndicesOut.Clear();

	if (xPositions.GetSize() == 0 || xIndices.GetSize() < 3 || fCellSize <= 0.0f)
	{
		return;
	}

	// CRITICAL: First identify extreme vertices (min/max in each axis)
	// These must be preserved to maintain correct bounding volume
	uint32_t uExtremeIndices[6] = { 0, 0, 0, 0, 0, 0 }; // minX, maxX, minY, maxY, minZ, maxZ
	float fExtremeValues[6] = { FLT_MAX, -FLT_MAX, FLT_MAX, -FLT_MAX, FLT_MAX, -FLT_MAX };

	for (uint32_t i = 0; i < xPositions.GetSize(); i++)
	{
		const Zenith_Maths::Vector3& xPos = xPositions.Get(i);

		// Check X extremes
		if (xPos.x < fExtremeValues[0]) { fExtremeValues[0] = xPos.x; uExtremeIndices[0] = i; }
		if (xPos.x > fExtremeValues[1]) { fExtremeValues[1] = xPos.x; uExtremeIndices[1] = i; }
		// Check Y extremes
		if (xPos.y < fExtremeValues[2]) { fExtremeValues[2] = xPos.y; uExtremeIndices[2] = i; }
		if (xPos.y > fExtremeValues[3]) { fExtremeValues[3] = xPos.y; uExtremeIndices[3] = i; }
		// Check Z extremes
		if (xPos.z < fExtremeValues[4]) { fExtremeValues[4] = xPos.z; uExtremeIndices[4] = i; }
		if (xPos.z > fExtremeValues[5]) { fExtremeValues[5] = xPos.z; uExtremeIndices[5] = i; }
	}

	// Create a set of extreme vertex indices for quick lookup
	std::unordered_set<uint32_t> xExtremeVertexSet;
	for (int i = 0; i < 6; i++)
	{
		xExtremeVertexSet.insert(uExtremeIndices[i]);
	}

	// Spatial hash for vertex merging
	struct CellKey
	{
		int32_t x, y, z;
		bool operator==(const CellKey& other) const
		{
			return x == other.x && y == other.y && z == other.z;
		}
	};

	struct CellKeyHash
	{
		size_t operator()(const CellKey& k) const
		{
			return std::hash<int32_t>()(k.x) ^
			       (std::hash<int32_t>()(k.y) << 1) ^
			       (std::hash<int32_t>()(k.z) << 2);
		}
	};

	std::unordered_map<CellKey, uint32_t, CellKeyHash> xCellToVertex;
	std::unordered_map<uint32_t, uint32_t> xOldToNewIndex;

	float fInvCellSize = 1.0f / fCellSize;

	// First pass: Add all extreme vertices to ensure they're preserved
	// This guarantees the bounding volume is maintained
	for (uint32_t i = 0; i < xPositions.GetSize(); i++)
	{
		if (xExtremeVertexSet.find(i) == xExtremeVertexSet.end())
		{
			continue; // Not an extreme vertex, skip in first pass
		}

		const Zenith_Maths::Vector3& xPos = xPositions.Get(i);
		CellKey xKey;
		xKey.x = static_cast<int32_t>(std::floor(xPos.x * fInvCellSize));
		xKey.y = static_cast<int32_t>(std::floor(xPos.y * fInvCellSize));
		xKey.z = static_cast<int32_t>(std::floor(xPos.z * fInvCellSize));

		// Extreme vertices always get their own slot, even if cell already exists
		// Use a unique key by adding a large offset to prevent collision
		CellKey xUniqueKey = xKey;
		while (xCellToVertex.find(xUniqueKey) != xCellToVertex.end())
		{
			// If the cell already has an extreme vertex with the same position, merge
			uint32_t uExistingIdx = xCellToVertex[xUniqueKey];
			if (Zenith_Maths::Length(xPositionsOut.Get(uExistingIdx) - xPos) < 0.0001f)
			{
				xOldToNewIndex[i] = uExistingIdx;
				goto next_extreme;
			}
			// Otherwise, find a unique key
			xUniqueKey.x += 1000000;
		}

		{
			uint32_t uNewIdx = xPositionsOut.GetSize();
			xPositionsOut.PushBack(xPos);
			xCellToVertex[xKey] = uNewIdx; // Map the original key to this extreme vertex
			xOldToNewIndex[i] = uNewIdx;
		}
		next_extreme:;
	}

	// Second pass: Merge non-extreme vertices that fall into the same cell
	for (uint32_t i = 0; i < xPositions.GetSize(); i++)
	{
		if (xExtremeVertexSet.find(i) != xExtremeVertexSet.end())
		{
			continue; // Already processed extreme vertex
		}

		const Zenith_Maths::Vector3& xPos = xPositions.Get(i);
		CellKey xKey;
		xKey.x = static_cast<int32_t>(std::floor(xPos.x * fInvCellSize));
		xKey.y = static_cast<int32_t>(std::floor(xPos.y * fInvCellSize));
		xKey.z = static_cast<int32_t>(std::floor(xPos.z * fInvCellSize));

		auto xIt = xCellToVertex.find(xKey);
		if (xIt == xCellToVertex.end())
		{
			uint32_t uNewIdx = xPositionsOut.GetSize();
			xPositionsOut.PushBack(xPos);
			xCellToVertex[xKey] = uNewIdx;
			xOldToNewIndex[i] = uNewIdx;
		}
		else
		{
			xOldToNewIndex[i] = xIt->second;
		}
	}

	// Remap indices and filter degenerate triangles
	for (uint32_t t = 0; t < xIndices.GetSize(); t += 3)
	{
		uint32_t i0 = xOldToNewIndex[xIndices.Get(t + 0)];
		uint32_t i1 = xOldToNewIndex[xIndices.Get(t + 1)];
		uint32_t i2 = xOldToNewIndex[xIndices.Get(t + 2)];

		// Skip degenerate triangles
		if (i0 != i1 && i1 != i2 && i2 != i0)
		{
			xIndicesOut.PushBack(i0);
			xIndicesOut.PushBack(i1);
			xIndicesOut.PushBack(i2);
		}
	}
}

Flux_MeshGeometry* Zenith_PhysicsMeshGenerator::GenerateSimplifiedMesh(
	const Zenith_Vector<Flux_MeshGeometry*>& xMeshGeometries,
	const PhysicsMeshConfig& xConfig)
{
	// Collect all positions and indices from all meshes
	Zenith_Vector<Zenith_Maths::Vector3> xAllPositions;
	Zenith_Vector<uint32_t> xAllIndices;

	uint32_t uVertexOffset = 0;
	for (uint32_t m = 0; m < xMeshGeometries.GetSize(); m++)
	{
		const Flux_MeshGeometry* pxMesh = xMeshGeometries.Get(m);
		if (!pxMesh || !pxMesh->m_pxPositions || !pxMesh->m_puIndices)
		{
			continue;
		}

		for (uint32_t v = 0; v < pxMesh->GetNumVerts(); v++)
		{
			xAllPositions.PushBack(pxMesh->m_pxPositions[v]);
		}

		for (uint32_t i = 0; i < pxMesh->GetNumIndices(); i++)
		{
			xAllIndices.PushBack(pxMesh->m_puIndices[i] + uVertexOffset);
		}

		uVertexOffset += pxMesh->GetNumVerts();
	}

	if (xAllPositions.GetSize() == 0 || xAllIndices.GetSize() < 3)
	{
		Zenith_Log("%s No valid geometry for simplified mesh, using AABB fallback", LOG_TAG_PHYSICS_MESH);
		return GenerateAABBMesh(xMeshGeometries);
	}

	// Compute AABB for cell size calculation
	Zenith_Maths::Vector3 xMin(FLT_MAX, FLT_MAX, FLT_MAX);
	Zenith_Maths::Vector3 xMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	for (uint32_t i = 0; i < xAllPositions.GetSize(); i++)
	{
		const Zenith_Maths::Vector3& xPos = xAllPositions.Get(i);
		xMin.x = std::min(xMin.x, xPos.x);
		xMin.y = std::min(xMin.y, xPos.y);
		xMin.z = std::min(xMin.z, xPos.z);
		xMax.x = std::max(xMax.x, xPos.x);
		xMax.y = std::max(xMax.y, xPos.y);
		xMax.z = std::max(xMax.z, xPos.z);
	}

	Zenith_Maths::Vector3 xExtent = xMax - xMin;
	float fMaxExtent = std::max(std::max(xExtent.x, xExtent.y), xExtent.z);

	// Calculate target vertex count based on simplification ratio
	uint32_t uSourceTriCount = xAllIndices.GetSize() / 3;
	uint32_t uTargetTriCount = static_cast<uint32_t>(uSourceTriCount * xConfig.m_fSimplificationRatio);
	uTargetTriCount = std::max(uTargetTriCount, xConfig.m_uMinTriangles);
	uTargetTriCount = std::min(uTargetTriCount, xConfig.m_uMaxTriangles);

	// Iteratively decimate until we reach target triangle count
	Zenith_Vector<Zenith_Maths::Vector3> xCurrentPositions = xAllPositions;
	Zenith_Vector<uint32_t> xCurrentIndices = xAllIndices;

	float fCellSize = fMaxExtent * 0.01f; // Start with small cell size
	const float fCellSizeMultiplier = 1.5f;
	const int iMaxIterations = 10;

	for (int iter = 0; iter < iMaxIterations; iter++)
	{
		uint32_t uCurrentTriCount = xCurrentIndices.GetSize() / 3;
		if (uCurrentTriCount <= uTargetTriCount)
		{
			break;
		}

		Zenith_Vector<Zenith_Maths::Vector3> xDecimatedPositions;
		Zenith_Vector<uint32_t> xDecimatedIndices;

		DecimateVertices(xCurrentPositions, xCurrentIndices, xDecimatedPositions, xDecimatedIndices, fCellSize);


		if (xDecimatedIndices.GetSize() >= 3)
		{
			xCurrentPositions = xDecimatedPositions;
			xCurrentIndices = xDecimatedIndices;
		}

		fCellSize *= fCellSizeMultiplier;
	}

	// Ensure we have valid geometry
	if (xCurrentPositions.GetSize() < 3 || xCurrentIndices.GetSize() < 3)
	{
		Zenith_Log("%s Decimation produced invalid geometry, using convex hull fallback", LOG_TAG_PHYSICS_MESH);
		return GenerateConvexHullMesh(xMeshGeometries);
	}

	Zenith_Log("%s Simplified mesh: %u -> %u vertices, %u -> %u triangles",
		LOG_TAG_PHYSICS_MESH,
		xAllPositions.GetSize(), xCurrentPositions.GetSize(),
		xAllIndices.GetSize() / 3, xCurrentIndices.GetSize() / 3);

	return CreateMeshFromData(xCurrentPositions, xCurrentIndices);
}

void Zenith_PhysicsMeshGenerator::DebugDrawAllPhysicsMeshes()
{
	// Get all model components in the current scene
	Zenith_Vector<Zenith_ModelComponent*> xModels;
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_ModelComponent>(xModels);

	static bool ls_bLoggedOnce = false;

	uint32_t uDrawnCount = 0;
	for (uint32_t i = 0; i < xModels.GetSize(); i++)
	{
		Zenith_ModelComponent* pxModel = xModels.Get(i);
		if (!pxModel)
		{
			continue;
		}

		// Check if we should draw this model's physics mesh
		bool bShouldDraw = g_bDebugDrawAllPhysicsMeshes || pxModel->GetDebugDrawPhysicsMesh();
		if (!bShouldDraw)
		{
			continue;
		}

		// Check if the model has a physics mesh
		const Flux_MeshGeometry* pxPhysicsMesh = pxModel->GetPhysicsMesh();
		if (!pxPhysicsMesh)
		{
			continue;
		}

		// Get the transform matrix
		Zenith_Entity xEntity = pxModel->GetParentEntity();
		if (!xEntity.HasComponent<Zenith_TransformComponent>())
		{
			continue;
		}

		Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
		Zenith_Maths::Matrix4 xModelMatrix;
		xTransform.BuildModelMatrix(xModelMatrix);

		// Determine color
		Zenith_Maths::Vector3 xColor = pxModel->GetDebugDrawColor();
		if (g_bDebugDrawAllPhysicsMeshes && !pxModel->GetDebugDrawPhysicsMesh())
		{
			// Use global config color if globally enabled but not per-component
			xColor = g_xPhysicsMeshConfig.m_xDebugColor;
		}

		// Draw the physics mesh
		DebugDrawPhysicsMesh(pxPhysicsMesh, xModelMatrix, xColor);
		uDrawnCount++;
	}

	// Log once when debug draw is first enabled
	if ((g_bDebugDrawAllPhysicsMeshes || g_xPhysicsMeshConfig.m_bDebugDraw) && !ls_bLoggedOnce)
	{
		Zenith_Log("[PhysicsDebugDraw] Debug drawing physics meshes for %u/%u model components",
			uDrawnCount, xModels.GetSize());
		ls_bLoggedOnce = true;
	}
	else if (!g_bDebugDrawAllPhysicsMeshes && !g_xPhysicsMeshConfig.m_bDebugDraw)
	{
		ls_bLoggedOnce = false;
	}
}
