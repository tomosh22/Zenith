#pragma once

#include "Maths/Zenith_Maths.h"

/**
 * Axis-Aligned Bounding Box (AABB) structure
 * Represents a box aligned with the world axes, defined by min and max corners.
 */
struct Zenith_AABB
{
	Zenith_Maths::Vector3 m_xMin{ FLT_MAX, FLT_MAX, FLT_MAX };  // Minimum corner (bottom-left-back)
	Zenith_Maths::Vector3 m_xMax{ -FLT_MAX, -FLT_MAX, -FLT_MAX }; // Maximum corner (top-right-front)

	Zenith_AABB() = default;
	Zenith_AABB(const Zenith_Maths::Vector3& xMin, const Zenith_Maths::Vector3& xMax)
		: m_xMin(xMin), m_xMax(xMax) {}

	/**
	 * Expand this AABB to include a point
	 */
	void ExpandToInclude(const Zenith_Maths::Vector3& xPoint)
	{
		m_xMin.x = glm::min(m_xMin.x, xPoint.x);
		m_xMin.y = glm::min(m_xMin.y, xPoint.y);
		m_xMin.z = glm::min(m_xMin.z, xPoint.z);
		m_xMax.x = glm::max(m_xMax.x, xPoint.x);
		m_xMax.y = glm::max(m_xMax.y, xPoint.y);
		m_xMax.z = glm::max(m_xMax.z, xPoint.z);
	}

	/**
	 * Get the center point of the AABB
	 */
	Zenith_Maths::Vector3 GetCenter() const
	{
		return (m_xMin + m_xMax) * 0.5f;
	}

	/**
	 * Get the extents (half-dimensions) of the AABB
	 */
	Zenith_Maths::Vector3 GetExtents() const
	{
		return (m_xMax - m_xMin) * 0.5f;
	}

	/**
	 * Check if this AABB is valid (min <= max for all axes)
	 */
	bool IsValid() const
	{
		return m_xMin.x <= m_xMax.x && m_xMin.y <= m_xMax.y && m_xMin.z <= m_xMax.z;
	}

	/**
	 * Reset the AABB to an invalid state (for expansion)
	 */
	void Reset()
	{
		m_xMin = Zenith_Maths::Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
		m_xMax = Zenith_Maths::Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	}
};

/**
 * Plane in 3D space, defined by normal and distance from origin
 * The plane equation is: dot(normal, point) + distance = 0
 */
struct Zenith_Plane
{
	Zenith_Maths::Vector3 m_xNormal{ 0, 1, 0 }; // Unit normal vector
	float m_fDistance = 0.0f;                    // Distance from origin along normal

	Zenith_Plane() = default;
	Zenith_Plane(const Zenith_Maths::Vector3& xNormal, float fDistance)
		: m_xNormal(glm::normalize(xNormal)), m_fDistance(fDistance) {}
	Zenith_Plane(const Zenith_Maths::Vector3& xNormal, const Zenith_Maths::Vector3& xPoint)
		: m_xNormal(glm::normalize(xNormal)), m_fDistance(-glm::dot(xNormal, xPoint)) {}

	/**
	 * Calculate the signed distance from a point to this plane
	 * Positive = in front of plane (in direction of normal)
	 * Negative = behind plane
	 * Zero = on plane
	 */
	float GetSignedDistance(const Zenith_Maths::Vector3& xPoint) const
	{
		return glm::dot(m_xNormal, xPoint) + m_fDistance;
	}

	/**
	 * Normalize the plane (ensure normal is unit length)
	 */
	void Normalize()
	{
		const float fLength = glm::length(m_xNormal);
		if (fLength > 0.0f)
		{
			m_xNormal /= fLength;
			m_fDistance /= fLength;
		}
	}
};

/**
 * View Frustum represented by 6 planes
 * Used for culling objects outside the camera's view
 *
 * Plane order:
 * 0 = Left
 * 1 = Right
 * 2 = Bottom
 * 3 = Top
 * 4 = Near
 * 5 = Far
 */
struct Zenith_Frustum
{
	Zenith_Plane m_axPlanes[6];

	/**
	 * Extract frustum planes from a view-projection matrix
	 * This uses the Gribb-Hartmann method for extracting planes
	 * from the combined view-projection matrix.
	 */
	void ExtractFromViewProjection(const Zenith_Maths::Matrix4& xViewProj)
	{
		// Left plane: m[3] + m[0]
		m_axPlanes[0].m_xNormal.x = xViewProj[0][3] + xViewProj[0][0];
		m_axPlanes[0].m_xNormal.y = xViewProj[1][3] + xViewProj[1][0];
		m_axPlanes[0].m_xNormal.z = xViewProj[2][3] + xViewProj[2][0];
		m_axPlanes[0].m_fDistance = xViewProj[3][3] + xViewProj[3][0];

		// Right plane: m[3] - m[0]
		m_axPlanes[1].m_xNormal.x = xViewProj[0][3] - xViewProj[0][0];
		m_axPlanes[1].m_xNormal.y = xViewProj[1][3] - xViewProj[1][0];
		m_axPlanes[1].m_xNormal.z = xViewProj[2][3] - xViewProj[2][0];
		m_axPlanes[1].m_fDistance = xViewProj[3][3] - xViewProj[3][0];

		// Bottom plane: m[3] + m[1]
		m_axPlanes[2].m_xNormal.x = xViewProj[0][3] + xViewProj[0][1];
		m_axPlanes[2].m_xNormal.y = xViewProj[1][3] + xViewProj[1][1];
		m_axPlanes[2].m_xNormal.z = xViewProj[2][3] + xViewProj[2][1];
		m_axPlanes[2].m_fDistance = xViewProj[3][3] + xViewProj[3][1];

		// Top plane: m[3] - m[1]
		m_axPlanes[3].m_xNormal.x = xViewProj[0][3] - xViewProj[0][1];
		m_axPlanes[3].m_xNormal.y = xViewProj[1][3] - xViewProj[1][1];
		m_axPlanes[3].m_xNormal.z = xViewProj[2][3] - xViewProj[2][1];
		m_axPlanes[3].m_fDistance = xViewProj[3][3] - xViewProj[3][1];

		// Near plane: m[3] + m[2]
		m_axPlanes[4].m_xNormal.x = xViewProj[0][3] + xViewProj[0][2];
		m_axPlanes[4].m_xNormal.y = xViewProj[1][3] + xViewProj[1][2];
		m_axPlanes[4].m_xNormal.z = xViewProj[2][3] + xViewProj[2][2];
		m_axPlanes[4].m_fDistance = xViewProj[3][3] + xViewProj[3][2];

		// Far plane: m[3] - m[2]
		m_axPlanes[5].m_xNormal.x = xViewProj[0][3] - xViewProj[0][2];
		m_axPlanes[5].m_xNormal.y = xViewProj[1][3] - xViewProj[1][2];
		m_axPlanes[5].m_xNormal.z = xViewProj[2][3] - xViewProj[2][2];
		m_axPlanes[5].m_fDistance = xViewProj[3][3] - xViewProj[3][2];

		// Normalize all planes
		for (int i = 0; i < 6; ++i)
		{
			m_axPlanes[i].Normalize();
		}
	}
};

/**
 * Frustum Culling Utilities
 */
namespace Zenith_FrustumCulling
{
	/**
	 * Test if an AABB intersects with a frustum
	 * Uses the "p-vertex / n-vertex" method for efficient testing
	 *
	 * @param xFrustum The frustum to test against
	 * @param xAABB The axis-aligned bounding box to test
	 * @return true if the AABB is at least partially inside the frustum
	 *
	 * This test is conservative: it may return false positives (saying an object
	 * is visible when it's not), but never false negatives (correctly identifies
	 * all truly visible objects).
	 */
	inline bool TestAABBFrustum(const Zenith_Frustum& xFrustum, const Zenith_AABB& xAABB)
	{
		// Get AABB center and extents for efficient testing
		const Zenith_Maths::Vector3 xCenter = xAABB.GetCenter();
		const Zenith_Maths::Vector3 xExtents = xAABB.GetExtents();

		// Test against each frustum plane
		for (int i = 0; i < 6; ++i)
		{
			const Zenith_Plane& xPlane = xFrustum.m_axPlanes[i];

			// Calculate the "positive vertex" - the corner of the AABB
			// that is furthest in the direction of the plane normal
			// This is the point most likely to be in front of the plane
			const float fRadius =
				xExtents.x * glm::abs(xPlane.m_xNormal.x) +
				xExtents.y * glm::abs(xPlane.m_xNormal.y) +
				xExtents.z * glm::abs(xPlane.m_xNormal.z);

			// Test center distance against radius
			// If center + radius is behind the plane, the entire AABB is outside
			const float fDistance = xPlane.GetSignedDistance(xCenter);
			if (fDistance < -fRadius)
			{
				return false; // AABB is completely outside this plane
			}
		}

		// AABB passed all plane tests - it's at least partially visible
		return true;
	}

	/**
	 * Generate an AABB from mesh geometry vertex positions
	 *
	 * @param pxPositions Array of vertex positions
	 * @param uNumVertices Number of vertices
	 * @return AABB encompassing all vertices
	 */
	inline Zenith_AABB GenerateAABBFromVertices(const Zenith_Maths::Vector3* pxPositions, uint32_t uNumVertices)
	{
		Zenith_AABB xResult;
		xResult.Reset();

		for (uint32_t i = 0; i < uNumVertices; ++i)
		{
			xResult.ExpandToInclude(pxPositions[i]);
		}

		return xResult;
	}

	/**
	 * Transform an AABB by a transformation matrix
	 * Since AABBs are axis-aligned, we need to recalculate the bounds
	 * after transformation (not just transform min/max corners)
	 *
	 * @param xAABB The original AABB
	 * @param xTransform The transformation matrix
	 * @return Transformed AABB (still axis-aligned)
	 */
	inline Zenith_AABB TransformAABB(const Zenith_AABB& xAABB, const Zenith_Maths::Matrix4& xTransform)
	{
		// Get the 8 corners of the original AABB
		Zenith_Maths::Vector3 axCorners[8];
		axCorners[0] = Zenith_Maths::Vector3(xAABB.m_xMin.x, xAABB.m_xMin.y, xAABB.m_xMin.z);
		axCorners[1] = Zenith_Maths::Vector3(xAABB.m_xMax.x, xAABB.m_xMin.y, xAABB.m_xMin.z);
		axCorners[2] = Zenith_Maths::Vector3(xAABB.m_xMin.x, xAABB.m_xMax.y, xAABB.m_xMin.z);
		axCorners[3] = Zenith_Maths::Vector3(xAABB.m_xMax.x, xAABB.m_xMax.y, xAABB.m_xMin.z);
		axCorners[4] = Zenith_Maths::Vector3(xAABB.m_xMin.x, xAABB.m_xMin.y, xAABB.m_xMax.z);
		axCorners[5] = Zenith_Maths::Vector3(xAABB.m_xMax.x, xAABB.m_xMin.y, xAABB.m_xMax.z);
		axCorners[6] = Zenith_Maths::Vector3(xAABB.m_xMin.x, xAABB.m_xMax.y, xAABB.m_xMax.z);
		axCorners[7] = Zenith_Maths::Vector3(xAABB.m_xMax.x, xAABB.m_xMax.y, xAABB.m_xMax.z);

		// Transform all corners and build new AABB
		Zenith_AABB xResult;
		xResult.Reset();

		for (int i = 0; i < 8; ++i)
		{
			Zenith_Maths::Vector4 xTransformedCorner = xTransform * Zenith_Maths::Vector4(axCorners[i], 1.0f);
			Zenith_Maths::Vector3 xCorner3D(xTransformedCorner.x, xTransformedCorner.y, xTransformedCorner.z);
			xResult.ExpandToInclude(xCorner3D);
		}

		return xResult;
	}
}
