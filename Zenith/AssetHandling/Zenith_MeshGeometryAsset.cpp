#include "Zenith.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux.h"

#include <cmath>

//------------------------------------------------------------------------------
// Internal primitive generators
//------------------------------------------------------------------------------
namespace
{

void GenerateSphere(Flux_MeshGeometry& xGeometryOut, uint32_t uLatitudeSegments, uint32_t uLongitudeSegments)
{
	const float PI = 3.14159265359f;

	uint32_t uNumVerts = (uLatitudeSegments + 1) * (uLongitudeSegments + 1);
	uint32_t uNumIndices = uLatitudeSegments * uLongitudeSegments * 6;

	xGeometryOut.m_uNumVerts = uNumVerts;
	xGeometryOut.m_uNumIndices = uNumIndices;

	xGeometryOut.m_pxPositions = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxUVs = new Zenith_Maths::Vector2[uNumVerts];
	xGeometryOut.m_pxNormals = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxTangents = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxBitangents = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxColors = new Zenith_Maths::Vector4[uNumVerts];
	xGeometryOut.m_puIndices = new Flux_MeshGeometry::IndexType[uNumIndices];

	uint32_t uVertIdx = 0;
	for (uint32_t lat = 0; lat <= uLatitudeSegments; ++lat)
	{
		float fTheta = lat * PI / uLatitudeSegments;
		float fSinTheta = sinf(fTheta);
		float fCosTheta = cosf(fTheta);

		for (uint32_t lon = 0; lon <= uLongitudeSegments; ++lon)
		{
			float fPhi = lon * 2.0f * PI / uLongitudeSegments;
			float fSinPhi = sinf(fPhi);
			float fCosPhi = cosf(fPhi);

			Zenith_Maths::Vector3 xPos;
			xPos.x = fSinTheta * fCosPhi * 0.5f;
			xPos.y = fCosTheta * 0.5f;
			xPos.z = fSinTheta * fSinPhi * 0.5f;

			xGeometryOut.m_pxPositions[uVertIdx] = xPos;

			Zenith_Maths::Vector3 xNormal = { fSinTheta * fCosPhi, fCosTheta, fSinTheta * fSinPhi };
			xGeometryOut.m_pxNormals[uVertIdx] = xNormal;

			float fU = static_cast<float>(lon) / uLongitudeSegments;
			float fV = static_cast<float>(lat) / uLatitudeSegments;
			xGeometryOut.m_pxUVs[uVertIdx] = { fU, fV };

			xGeometryOut.m_pxTangents[uVertIdx] = { -fSinPhi, 0.0f, fCosPhi };
			xGeometryOut.m_pxBitangents[uVertIdx] = { fCosTheta * fCosPhi, -fSinTheta, fCosTheta * fSinPhi };
			xGeometryOut.m_pxColors[uVertIdx] = { 1.0f, 1.0f, 1.0f, 1.0f };

			uVertIdx++;
		}
	}

	uint32_t uIdxIdx = 0;
	for (uint32_t lat = 0; lat < uLatitudeSegments; ++lat)
	{
		for (uint32_t lon = 0; lon < uLongitudeSegments; ++lon)
		{
			uint32_t uCurrent = lat * (uLongitudeSegments + 1) + lon;
			uint32_t uNext = uCurrent + uLongitudeSegments + 1;

			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent + 1;

			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent + 1;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext + 1;
		}
	}

	xGeometryOut.GenerateLayoutAndVertexData();
	Flux_MemoryManager::InitialiseVertexBuffer(xGeometryOut.GetVertexData(), xGeometryOut.GetVertexDataSize(), xGeometryOut.m_xVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(xGeometryOut.GetIndexData(), xGeometryOut.GetIndexDataSize(), xGeometryOut.m_xIndexBuffer);
}

void GenerateCapsule(Flux_MeshGeometry& xGeometryOut, float fRadius, float fHeight, uint32_t uSlices, uint32_t uStacks)
{
	const float PI = 3.14159265359f;
	float fCylinderHalfHeight = fHeight * 0.5f;

	uint32_t uNumVerts = (uStacks + 1) * (uSlices + 1);
	uint32_t uNumIndices = uStacks * uSlices * 6;

	xGeometryOut.m_uNumVerts = uNumVerts;
	xGeometryOut.m_uNumIndices = uNumIndices;
	xGeometryOut.m_pxPositions = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxNormals = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxUVs = new Zenith_Maths::Vector2[uNumVerts];
	xGeometryOut.m_pxTangents = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxBitangents = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxColors = new Zenith_Maths::Vector4[uNumVerts];
	xGeometryOut.m_puIndices = new Flux_MeshGeometry::IndexType[uNumIndices];

	uint32_t uVertIdx = 0;
	for (uint32_t uStack = 0; uStack <= uStacks; uStack++)
	{
		float fPhi = static_cast<float>(uStack) / static_cast<float>(uStacks) * PI;
		float fSphereY = cos(fPhi);
		float fStackRadius = sin(fPhi) * fRadius;

		float fY;
		if (fSphereY > 0.0f)
		{
			fY = fSphereY * fRadius + fCylinderHalfHeight;
		}
		else
		{
			fY = fSphereY * fRadius - fCylinderHalfHeight;
		}

		for (uint32_t uSlice = 0; uSlice <= uSlices; uSlice++)
		{
			float fTheta = static_cast<float>(uSlice) / static_cast<float>(uSlices) * 2.0f * PI;
			float fX = cos(fTheta) * fStackRadius;
			float fZ = sin(fTheta) * fStackRadius;

			Zenith_Maths::Vector3 xPos(fX, fY, fZ);
			Zenith_Maths::Vector3 xNormal(fX, cos(fPhi) * fRadius, fZ);

			if (glm::length(xNormal) > 0.001f)
			{
				xNormal = glm::normalize(xNormal);
			}
			else
			{
				xNormal = Zenith_Maths::Vector3(0.0f, fSphereY > 0.0f ? 1.0f : -1.0f, 0.0f);
			}

			xGeometryOut.m_pxPositions[uVertIdx] = xPos;
			xGeometryOut.m_pxNormals[uVertIdx] = xNormal;
			xGeometryOut.m_pxUVs[uVertIdx] = Zenith_Maths::Vector2(
				static_cast<float>(uSlice) / static_cast<float>(uSlices),
				static_cast<float>(uStack) / static_cast<float>(uStacks)
			);

			Zenith_Maths::Vector3 xTangent(-sin(fTheta), 0.0f, cos(fTheta));
			xGeometryOut.m_pxTangents[uVertIdx] = xTangent;
			xGeometryOut.m_pxBitangents[uVertIdx] = glm::cross(xNormal, xTangent);
			xGeometryOut.m_pxColors[uVertIdx] = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);

			uVertIdx++;
		}
	}

	uint32_t uIdxIdx = 0;
	for (uint32_t uStack = 0; uStack < uStacks; uStack++)
	{
		for (uint32_t uSlice = 0; uSlice < uSlices; uSlice++)
		{
			uint32_t uCurrent = uStack * (uSlices + 1) + uSlice;
			uint32_t uNext = uCurrent + uSlices + 1;

			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent + 1;

			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent + 1;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext + 1;
		}
	}

	xGeometryOut.GenerateLayoutAndVertexData();
	Flux_MemoryManager::InitialiseVertexBuffer(xGeometryOut.GetVertexData(), xGeometryOut.GetVertexDataSize(), xGeometryOut.m_xVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(xGeometryOut.GetIndexData(), xGeometryOut.GetIndexDataSize(), xGeometryOut.m_xIndexBuffer);
}

void GenerateCylinder(Flux_MeshGeometry& xGeometryOut, float fRadius, float fHeight, uint32_t uSlices)
{
	const float PI = 3.14159265359f;
	float fHalfHeight = fHeight * 0.5f;

	// Cylinder body + top cap + bottom cap
	uint32_t uBodyVerts = (uSlices + 1) * 2;
	uint32_t uCapVerts = uSlices + 1;  // Ring + center for each cap
	uint32_t uNumVerts = uBodyVerts + uCapVerts * 2;
	uint32_t uNumIndices = uSlices * 6 + uSlices * 3 * 2;  // Body quads + cap triangles

	xGeometryOut.m_uNumVerts = uNumVerts;
	xGeometryOut.m_uNumIndices = uNumIndices;
	xGeometryOut.m_pxPositions = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxNormals = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxUVs = new Zenith_Maths::Vector2[uNumVerts];
	xGeometryOut.m_pxTangents = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxBitangents = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxColors = new Zenith_Maths::Vector4[uNumVerts];
	xGeometryOut.m_puIndices = new Flux_MeshGeometry::IndexType[uNumIndices];

	uint32_t uVertIdx = 0;

	// Generate body vertices (two rings)
	for (uint32_t ring = 0; ring < 2; ring++)
	{
		float fY = ring == 0 ? -fHalfHeight : fHalfHeight;
		for (uint32_t i = 0; i <= uSlices; i++)
		{
			float fTheta = static_cast<float>(i) / static_cast<float>(uSlices) * 2.0f * PI;
			float fX = cos(fTheta) * fRadius;
			float fZ = sin(fTheta) * fRadius;

			xGeometryOut.m_pxPositions[uVertIdx] = Zenith_Maths::Vector3(fX, fY, fZ);
			xGeometryOut.m_pxNormals[uVertIdx] = glm::normalize(Zenith_Maths::Vector3(fX, 0.0f, fZ));
			xGeometryOut.m_pxUVs[uVertIdx] = Zenith_Maths::Vector2(static_cast<float>(i) / uSlices, static_cast<float>(ring));
			xGeometryOut.m_pxTangents[uVertIdx] = Zenith_Maths::Vector3(-sin(fTheta), 0.0f, cos(fTheta));
			xGeometryOut.m_pxBitangents[uVertIdx] = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
			xGeometryOut.m_pxColors[uVertIdx] = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
			uVertIdx++;
		}
	}

	// Top cap vertices
	uint32_t uTopCapStart = uVertIdx;
	for (uint32_t i = 0; i <= uSlices; i++)
	{
		float fTheta = static_cast<float>(i) / static_cast<float>(uSlices) * 2.0f * PI;
		float fX = i < uSlices ? cos(fTheta) * fRadius : 0.0f;
		float fZ = i < uSlices ? sin(fTheta) * fRadius : 0.0f;
		float fY = i < uSlices ? fHalfHeight : fHalfHeight;

		xGeometryOut.m_pxPositions[uVertIdx] = Zenith_Maths::Vector3(fX, fY, fZ);
		xGeometryOut.m_pxNormals[uVertIdx] = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
		xGeometryOut.m_pxUVs[uVertIdx] = Zenith_Maths::Vector2(fX * 0.5f + 0.5f, fZ * 0.5f + 0.5f);
		xGeometryOut.m_pxTangents[uVertIdx] = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
		xGeometryOut.m_pxBitangents[uVertIdx] = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		xGeometryOut.m_pxColors[uVertIdx] = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		uVertIdx++;
	}

	// Bottom cap vertices
	uint32_t uBottomCapStart = uVertIdx;
	for (uint32_t i = 0; i <= uSlices; i++)
	{
		float fTheta = static_cast<float>(i) / static_cast<float>(uSlices) * 2.0f * PI;
		float fX = i < uSlices ? cos(fTheta) * fRadius : 0.0f;
		float fZ = i < uSlices ? sin(fTheta) * fRadius : 0.0f;
		float fY = i < uSlices ? -fHalfHeight : -fHalfHeight;

		xGeometryOut.m_pxPositions[uVertIdx] = Zenith_Maths::Vector3(fX, fY, fZ);
		xGeometryOut.m_pxNormals[uVertIdx] = Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f);
		xGeometryOut.m_pxUVs[uVertIdx] = Zenith_Maths::Vector2(fX * 0.5f + 0.5f, fZ * 0.5f + 0.5f);
		xGeometryOut.m_pxTangents[uVertIdx] = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
		xGeometryOut.m_pxBitangents[uVertIdx] = Zenith_Maths::Vector3(0.0f, 0.0f, -1.0f);
		xGeometryOut.m_pxColors[uVertIdx] = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		uVertIdx++;
	}

	// Generate indices
	uint32_t uIdxIdx = 0;

	// Body indices
	for (uint32_t i = 0; i < uSlices; i++)
	{
		uint32_t uBottom = i;
		uint32_t uTop = i + uSlices + 1;

		xGeometryOut.m_puIndices[uIdxIdx++] = uBottom;
		xGeometryOut.m_puIndices[uIdxIdx++] = uTop;
		xGeometryOut.m_puIndices[uIdxIdx++] = uBottom + 1;

		xGeometryOut.m_puIndices[uIdxIdx++] = uBottom + 1;
		xGeometryOut.m_puIndices[uIdxIdx++] = uTop;
		xGeometryOut.m_puIndices[uIdxIdx++] = uTop + 1;
	}

	// Top cap indices
	uint32_t uTopCenter = uTopCapStart + uSlices;
	for (uint32_t i = 0; i < uSlices; i++)
	{
		xGeometryOut.m_puIndices[uIdxIdx++] = uTopCapStart + i;
		xGeometryOut.m_puIndices[uIdxIdx++] = uTopCapStart + i + 1;
		xGeometryOut.m_puIndices[uIdxIdx++] = uTopCenter;
	}

	// Bottom cap indices (reversed winding)
	uint32_t uBottomCenter = uBottomCapStart + uSlices;
	for (uint32_t i = 0; i < uSlices; i++)
	{
		xGeometryOut.m_puIndices[uIdxIdx++] = uBottomCapStart + i + 1;
		xGeometryOut.m_puIndices[uIdxIdx++] = uBottomCapStart + i;
		xGeometryOut.m_puIndices[uIdxIdx++] = uBottomCenter;
	}

	xGeometryOut.GenerateLayoutAndVertexData();
	Flux_MemoryManager::InitialiseVertexBuffer(xGeometryOut.GetVertexData(), xGeometryOut.GetVertexDataSize(), xGeometryOut.m_xVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(xGeometryOut.GetIndexData(), xGeometryOut.GetIndexDataSize(), xGeometryOut.m_xIndexBuffer);
}

void GenerateCone(Flux_MeshGeometry& xGeometryOut, float fRadius, float fHeight, uint32_t uSlices)
{
	const float PI = 3.14159265359f;

	uint32_t uNumVerts = uSlices + 2;  // Base ring + apex + base center
	uint32_t uNumIndices = uSlices * 6;

	xGeometryOut.m_uNumVerts = uNumVerts;
	xGeometryOut.m_uNumIndices = uNumIndices;
	xGeometryOut.m_pxPositions = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxNormals = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxUVs = new Zenith_Maths::Vector2[uNumVerts];
	xGeometryOut.m_pxTangents = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxBitangents = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxColors = new Zenith_Maths::Vector4[uNumVerts];
	xGeometryOut.m_puIndices = new Flux_MeshGeometry::IndexType[uNumIndices];

	// Base ring vertices
	for (uint32_t i = 0; i < uSlices; i++)
	{
		float fTheta = static_cast<float>(i) / static_cast<float>(uSlices) * 2.0f * PI;
		float fX = cos(fTheta) * fRadius;
		float fZ = sin(fTheta) * fRadius;

		xGeometryOut.m_pxPositions[i] = Zenith_Maths::Vector3(fX, 0.0f, fZ);

		// Cone normal (pointing outward and up)
		float fSlope = fRadius / fHeight;
		Zenith_Maths::Vector3 xNormal = glm::normalize(Zenith_Maths::Vector3(cos(fTheta), fSlope, sin(fTheta)));
		xGeometryOut.m_pxNormals[i] = xNormal;

		xGeometryOut.m_pxUVs[i] = Zenith_Maths::Vector2(static_cast<float>(i) / uSlices, 0.0f);
		xGeometryOut.m_pxTangents[i] = Zenith_Maths::Vector3(-sin(fTheta), 0.0f, cos(fTheta));
		xGeometryOut.m_pxBitangents[i] = glm::cross(xNormal, xGeometryOut.m_pxTangents[i]);
		xGeometryOut.m_pxColors[i] = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	// Apex vertex
	uint32_t uApex = uSlices;
	xGeometryOut.m_pxPositions[uApex] = Zenith_Maths::Vector3(0.0f, fHeight, 0.0f);
	xGeometryOut.m_pxNormals[uApex] = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
	xGeometryOut.m_pxUVs[uApex] = Zenith_Maths::Vector2(0.5f, 1.0f);
	xGeometryOut.m_pxTangents[uApex] = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
	xGeometryOut.m_pxBitangents[uApex] = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
	xGeometryOut.m_pxColors[uApex] = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);

	// Base center vertex
	uint32_t uBaseCenter = uSlices + 1;
	xGeometryOut.m_pxPositions[uBaseCenter] = Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f);
	xGeometryOut.m_pxNormals[uBaseCenter] = Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f);
	xGeometryOut.m_pxUVs[uBaseCenter] = Zenith_Maths::Vector2(0.5f, 0.5f);
	xGeometryOut.m_pxTangents[uBaseCenter] = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
	xGeometryOut.m_pxBitangents[uBaseCenter] = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
	xGeometryOut.m_pxColors[uBaseCenter] = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);

	// Generate indices
	uint32_t uIdxIdx = 0;

	// Side triangles
	for (uint32_t i = 0; i < uSlices; i++)
	{
		uint32_t uNext = (i + 1) % uSlices;
		xGeometryOut.m_puIndices[uIdxIdx++] = i;
		xGeometryOut.m_puIndices[uIdxIdx++] = uApex;
		xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
	}

	// Base triangles (reversed winding)
	for (uint32_t i = 0; i < uSlices; i++)
	{
		uint32_t uNext = (i + 1) % uSlices;
		xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
		xGeometryOut.m_puIndices[uIdxIdx++] = uBaseCenter;
		xGeometryOut.m_puIndices[uIdxIdx++] = i;
	}

	xGeometryOut.GenerateLayoutAndVertexData();
	Flux_MemoryManager::InitialiseVertexBuffer(xGeometryOut.GetVertexData(), xGeometryOut.GetVertexDataSize(), xGeometryOut.m_xVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(xGeometryOut.GetIndexData(), xGeometryOut.GetIndexDataSize(), xGeometryOut.m_xIndexBuffer);
}

} // anonymous namespace

//------------------------------------------------------------------------------
// Construction / Destruction
//------------------------------------------------------------------------------

Zenith_MeshGeometryAsset::Zenith_MeshGeometryAsset()
	: m_pxGeometry(nullptr)
	, m_bOwnsGeometry(true)
{
}

Zenith_MeshGeometryAsset::~Zenith_MeshGeometryAsset()
{
	if (m_bOwnsGeometry && m_pxGeometry)
	{
		delete m_pxGeometry;
		m_pxGeometry = nullptr;
	}
}

void Zenith_MeshGeometryAsset::SetGeometry(Flux_MeshGeometry* pxGeometry)
{
	if (m_bOwnsGeometry && m_pxGeometry)
	{
		delete m_pxGeometry;
	}
	m_pxGeometry = pxGeometry;
	m_bOwnsGeometry = true;
}

Flux_MeshGeometry* Zenith_MeshGeometryAsset::ReleaseGeometry()
{
	Flux_MeshGeometry* pxGeometry = m_pxGeometry;
	m_pxGeometry = nullptr;
	m_bOwnsGeometry = false;
	return pxGeometry;
}

bool Zenith_MeshGeometryAsset::LoadFromFile(const std::string& strPath, uint32_t uRetainAttributeBits, bool bUploadToGPU)
{
	if (strPath.empty())
	{
		return false;
	}

	m_pxGeometry = new Flux_MeshGeometry();
	Flux_MeshGeometry::LoadFromFile(strPath.c_str(), *m_pxGeometry, uRetainAttributeBits, bUploadToGPU);
	m_bOwnsGeometry = true;

	Zenith_Log(LOG_CATEGORY_MESH, "Loaded mesh geometry from: %s", strPath.c_str());
	return true;
}

//------------------------------------------------------------------------------
// Static Primitive Creators
//------------------------------------------------------------------------------

Zenith_MeshGeometryAsset* Zenith_MeshGeometryAsset::CreateFullscreenQuad()
{
	static const std::string strPath = "procedural://fullscreen_quad";

	// Check if already cached
	Zenith_MeshGeometryAsset* pxExisting = Zenith_AssetRegistry::Get().Get<Zenith_MeshGeometryAsset>(strPath);
	if (pxExisting)
	{
		return pxExisting;
	}

	// Create new
	Zenith_MeshGeometryAsset* pxAsset = Zenith_AssetRegistry::Get().Create<Zenith_MeshGeometryAsset>(strPath);
	pxAsset->m_pxGeometry = new Flux_MeshGeometry();
	Flux_MeshGeometry::GenerateFullscreenQuad(*pxAsset->m_pxGeometry);
	return pxAsset;
}

Zenith_MeshGeometryAsset* Zenith_MeshGeometryAsset::CreateUnitCube()
{
	static const std::string strPath = "procedural://unit_cube";

	Zenith_MeshGeometryAsset* pxExisting = Zenith_AssetRegistry::Get().Get<Zenith_MeshGeometryAsset>(strPath);
	if (pxExisting)
	{
		return pxExisting;
	}

	Zenith_MeshGeometryAsset* pxAsset = Zenith_AssetRegistry::Get().Create<Zenith_MeshGeometryAsset>(strPath);
	pxAsset->m_pxGeometry = new Flux_MeshGeometry();
	Flux_MeshGeometry::GenerateUnitCube(*pxAsset->m_pxGeometry);
	return pxAsset;
}

Zenith_MeshGeometryAsset* Zenith_MeshGeometryAsset::CreateUnitSphere(uint32_t uSegments)
{
	std::string strPath = "procedural://unit_sphere_" + std::to_string(uSegments);

	Zenith_MeshGeometryAsset* pxExisting = Zenith_AssetRegistry::Get().Get<Zenith_MeshGeometryAsset>(strPath);
	if (pxExisting)
	{
		return pxExisting;
	}

	Zenith_MeshGeometryAsset* pxAsset = Zenith_AssetRegistry::Get().Create<Zenith_MeshGeometryAsset>(strPath);
	pxAsset->m_pxGeometry = new Flux_MeshGeometry();
	GenerateSphere(*pxAsset->m_pxGeometry, uSegments, uSegments * 2);
	return pxAsset;
}

Zenith_MeshGeometryAsset* Zenith_MeshGeometryAsset::CreateUnitCapsule(uint32_t uSegments)
{
	std::string strPath = "procedural://unit_capsule_" + std::to_string(uSegments);

	Zenith_MeshGeometryAsset* pxExisting = Zenith_AssetRegistry::Get().Get<Zenith_MeshGeometryAsset>(strPath);
	if (pxExisting)
	{
		return pxExisting;
	}

	Zenith_MeshGeometryAsset* pxAsset = Zenith_AssetRegistry::Get().Create<Zenith_MeshGeometryAsset>(strPath);
	pxAsset->m_pxGeometry = new Flux_MeshGeometry();
	// Unit capsule: radius 0.25, height 0.5 (total height 1.0 including caps)
	GenerateCapsule(*pxAsset->m_pxGeometry, 0.25f, 0.5f, uSegments, uSegments);
	return pxAsset;
}

Zenith_MeshGeometryAsset* Zenith_MeshGeometryAsset::CreateUnitCylinder(uint32_t uSegments)
{
	std::string strPath = "procedural://unit_cylinder_" + std::to_string(uSegments);

	Zenith_MeshGeometryAsset* pxExisting = Zenith_AssetRegistry::Get().Get<Zenith_MeshGeometryAsset>(strPath);
	if (pxExisting)
	{
		return pxExisting;
	}

	Zenith_MeshGeometryAsset* pxAsset = Zenith_AssetRegistry::Get().Create<Zenith_MeshGeometryAsset>(strPath);
	pxAsset->m_pxGeometry = new Flux_MeshGeometry();
	// Unit cylinder: radius 0.5, height 1.0
	GenerateCylinder(*pxAsset->m_pxGeometry, 0.5f, 1.0f, uSegments);
	return pxAsset;
}

Zenith_MeshGeometryAsset* Zenith_MeshGeometryAsset::CreateUnitCone(uint32_t uSegments)
{
	std::string strPath = "procedural://unit_cone_" + std::to_string(uSegments);

	Zenith_MeshGeometryAsset* pxExisting = Zenith_AssetRegistry::Get().Get<Zenith_MeshGeometryAsset>(strPath);
	if (pxExisting)
	{
		return pxExisting;
	}

	Zenith_MeshGeometryAsset* pxAsset = Zenith_AssetRegistry::Get().Create<Zenith_MeshGeometryAsset>(strPath);
	pxAsset->m_pxGeometry = new Flux_MeshGeometry();
	// Unit cone: radius 0.5, height 1.0
	GenerateCone(*pxAsset->m_pxGeometry, 0.5f, 1.0f, uSegments);
	return pxAsset;
}
