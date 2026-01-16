#pragma once

#include "Maths/Zenith_Maths.h"
#include "Maths/Zenith_FrustumCulling.h"

//=============================================================================
// GPU Culling Constants (matches layout in Flux_InstanceCulling.comp)
//=============================================================================

// Frustum plane in GPU format (16 bytes)
struct Flux_FrustumPlaneGPU
{
	Zenith_Maths::Vector4 m_xNormalAndDistance;  // xyz = normal, w = distance
};
static_assert(sizeof(Flux_FrustumPlaneGPU) == 16, "Flux_FrustumPlaneGPU must be 16 bytes");

// Culling constants uniform buffer (must match CullingConstants in compute shader)
struct Flux_CullingConstants
{
	Flux_FrustumPlaneGPU m_axFrustumPlanes[6];  // 96 bytes: Left, Right, Bottom, Top, Near, Far
	Zenith_Maths::Vector4 m_xCameraPosition;    // 16 bytes: xyz = camera position, w = padding
	uint32_t m_uTotalInstanceCount;             // 4 bytes: Total instance count (matches totalInstanceCount in shader)
	uint32_t m_uMeshIndexCount;                 // 4 bytes: Index count for indirect draw
	float m_fBoundingSphereRadius;              // 4 bytes: Local-space bounding sphere radius
	float m_fPadding;                           // 4 bytes: Padding for alignment
};
static_assert(sizeof(Flux_CullingConstants) == 128, "Flux_CullingConstants must be 128 bytes");

//=============================================================================
// Utility Functions
//=============================================================================

namespace Flux_InstanceCullingUtil
{
	// Extract frustum planes from view-projection matrix into GPU format
	inline void ExtractFrustumPlanes(const Zenith_Maths::Matrix4& xViewProjMatrix, Flux_FrustumPlaneGPU* pxOutPlanes)
	{
		Zenith_Frustum xFrustum;
		xFrustum.ExtractFromViewProjection(xViewProjMatrix);

		// Convert to GPU format
		for (int i = 0; i < 6; ++i)
		{
			pxOutPlanes[i].m_xNormalAndDistance = Zenith_Maths::Vector4(
				xFrustum.m_axPlanes[i].m_xNormal,
				xFrustum.m_axPlanes[i].m_fDistance
			);
		}
	}

	// Calculate bounding sphere radius from mesh vertices
	// Returns maximum distance from origin to any vertex
	inline float CalculateBoundingSphereRadius(const Zenith_Maths::Vector3* pxPositions, uint32_t uNumVertices)
	{
		float fMaxDistSq = 0.0f;
		for (uint32_t i = 0; i < uNumVertices; ++i)
		{
			float fDistSq = glm::dot(pxPositions[i], pxPositions[i]);
			if (fDistSq > fMaxDistSq)
			{
				fMaxDistSq = fDistSq;
			}
		}
		return glm::sqrt(fMaxDistSq);
	}
}
