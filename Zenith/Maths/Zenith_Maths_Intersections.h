#pragma once

#include "Maths/Zenith_Maths.h"

namespace Zenith_Maths::Intersections
{
	inline bool RayIntersectsCircle(const Zenith_Maths::Vector3& xRayOrigin, const Zenith_Maths::Vector3& xRayDir,
		const Zenith_Maths::Vector3& xNormal, const float fRadius, const float fThreshold, float& fOutDistance)
	{
		// Ray-plane intersection
		const float fDenom = glm::dot(xNormal, xRayDir);
		if (fabsf(fDenom) < 0.0001f)
			return false;  // Parallel

		const float fT = -glm::dot(xNormal, xRayOrigin) / fDenom;
		if (fT < 0.0f)
			return false;

		// Check if hit point is near the circle (torus-like region)
		const Zenith_Maths::Vector3 xHitPoint = xRayOrigin + xRayDir * fT;
		const float fDistFromCenter = glm::length(xHitPoint);

		if (fabsf(fDistFromCenter - fRadius) < fThreshold)
		{
			fOutDistance = fT;
			return true;
		}

		return false;
	}

	inline bool RayIntersectsAABB(const Zenith_Maths::Vector3& xRayOrigin, const Zenith_Maths::Vector3& xRayDir,
		const Zenith_Maths::Vector3& xAABBCenter, float fAABBSize, float& fOutDistance)
	{
		const float fHalf = fAABBSize * 0.5f;
		const Zenith_Maths::Vector3 xBoxMin = xAABBCenter - Zenith_Maths::Vector3(fHalf);
		const Zenith_Maths::Vector3 xBoxMax = xAABBCenter + Zenith_Maths::Vector3(fHalf);

		// Safe inverse direction - avoid infinity by using large value for near-zero components
		constexpr float fMinDir = 1e-6f;
		constexpr float fMaxInv = 1e6f;
		const Zenith_Maths::Vector3 xInvDir = Zenith_Maths::Vector3(
			(fabsf(xRayDir.x) > fMinDir) ? 1.0f / xRayDir.x : ((xRayDir.x >= 0.0f) ? fMaxInv : -fMaxInv),
			(fabsf(xRayDir.y) > fMinDir) ? 1.0f / xRayDir.y : ((xRayDir.y >= 0.0f) ? fMaxInv : -fMaxInv),
			(fabsf(xRayDir.z) > fMinDir) ? 1.0f / xRayDir.z : ((xRayDir.z >= 0.0f) ? fMaxInv : -fMaxInv)
		);
		const Zenith_Maths::Vector3 xT0 = (xBoxMin - xRayOrigin) * xInvDir;
		const Zenith_Maths::Vector3 xT1 = (xBoxMax - xRayOrigin) * xInvDir;

		const Zenith_Maths::Vector3 xTmin = glm::min(xT0, xT1);
		const Zenith_Maths::Vector3 xTmax = glm::max(xT0, xT1);

		const float fTNear = glm::max(glm::max(xTmin.x, xTmin.y), xTmin.z);
		const float fTFar = glm::min(glm::min(xTmax.x, xTmax.y), xTmax.z);

		if (fTNear > fTFar || fTFar < 0.0f)
			return false;

		fOutDistance = fTNear > 0.0f ? fTNear : fTFar;
		return true;
	}

	// Ray against a sphere CENTRED ON THE ORIGIN, matching every other helper in
	// this header: the shape sits at the origin and the caller pre-translates the
	// ray (pass xRayOrigin - centre). Returns the NEAREST non-negative hit, so a
	// ray whose origin is INSIDE the sphere reports the exit point rather than
	// missing — which is what a picking ray that starts inside a joint's ball
	// has to do if the joint is to stay selectable from close up.
	inline bool RayIntersectsSphere(const Zenith_Maths::Vector3& xRayOrigin, const Zenith_Maths::Vector3& xRayDir,
		float fRadius, float& fOutDistance)
	{
		// |o + t*d|^2 = r^2  ->  (d.d) t^2 + 2(o.d) t + (o.o - r^2) = 0
		const float fA = glm::dot(xRayDir, xRayDir);
		if (fA < 1.0e-12f)
		{
			// A zero-length direction is not a ray; it has no distance to report.
			return false;
		}

		const float fB = 2.0f * glm::dot(xRayOrigin, xRayDir);
		const float fC = glm::dot(xRayOrigin, xRayOrigin) - fRadius * fRadius;

		const float fDiscriminant = fB * fB - 4.0f * fA * fC;
		if (fDiscriminant < 0.0f)
		{
			return false;
		}

		const float fSqrtDisc = sqrtf(fDiscriminant);
		const float fInv2A = 1.0f / (2.0f * fA);

		float fT = (-fB - fSqrtDisc) * fInv2A;
		if (fT < 0.0f)
		{
			fT = (-fB + fSqrtDisc) * fInv2A;
		}
		if (fT < 0.0f)
		{
			// Both roots are behind the ray.
			return false;
		}

		fOutDistance = fT;
		return true;
	}

	inline bool RayIntersectsCylinder(const Zenith_Maths::Vector3& xRayOrigin, const Zenith_Maths::Vector3& xRayDir,
		const Zenith_Maths::Vector3& xAxis, float fCylinderRadius, float fCylinderLength, float& fOutDistance)
	{
		// Ray-cylinder intersection (finite cylinder along axis from origin)
		// Ray: P = rayOrigin + t * rayDir
		// Cylinder: |P - (P�axis)*axis|� = radius�, 0 <= P�axis <= arrowLength

		const float fDotAxisDir = glm::dot(xAxis, xRayDir);
		const float fDotAxisOrigin = glm::dot(xAxis, xRayOrigin);

		// Quadratic coefficients for ray-cylinder intersection
		// We're solving for t where the ray hits the infinite cylinder
		const float fA = glm::dot(xRayDir, xRayDir) - fDotAxisDir * fDotAxisDir;
		const float fB = 2.0f * (glm::dot(xRayDir, xRayOrigin) - fDotAxisDir * fDotAxisOrigin);
		const float fC = glm::dot(xRayOrigin, xRayOrigin) - fDotAxisOrigin * fDotAxisOrigin - fCylinderRadius * fCylinderRadius;

		// Handle degenerate case where ray is parallel to axis
		if (glm::abs(fA) < 0.0001f)
		{
			// Ray is parallel to cylinder axis
			// Check if ray origin is inside cylinder radius
			float distFromAxis = sqrtf(glm::max(0.0f, glm::dot(xRayOrigin, xRayOrigin) - fDotAxisOrigin * fDotAxisOrigin));
			if (distFromAxis <= fCylinderRadius)
			{
				// Find where along axis the ray is closest
				fOutDistance = 0.0f;
				return true;
			}
			return false;
		}

		const float fDiscriminant = fB * fB - 4.0f * fA * fC;

		if (fDiscriminant < 0.0f)
			return false;

		// Get both intersection points
		const float sqrtDisc = sqrtf(fDiscriminant);
		const float t1 = (-fB - sqrtDisc) / (2.0f * fA);
		const float t2 = (-fB + sqrtDisc) / (2.0f * fA);

		// Try the closer intersection first
		float fT = t1;
		if (fT < 0.0f)
			fT = t2;
		if (fT < 0.0f)
			return false;

		// Check if hit point is within arrow length bounds
		Zenith_Maths::Vector3 xHitPoint = xRayOrigin + xRayDir * fT;
		float fAlongAxis = glm::dot(xHitPoint, xAxis);

		// If first hit is outside bounds, try the second hit
		if (fAlongAxis < 0.0f || fAlongAxis > fCylinderLength)
		{
			fT = t2;
			if (fT < 0.0f)
				return false;
			xHitPoint = xRayOrigin + xRayDir * fT;
			fAlongAxis = glm::dot(xHitPoint, xAxis);
		}

		if (fAlongAxis >= 0.0f && fAlongAxis <= fCylinderLength)
		{
			fOutDistance = fT;
			return true;
		}

		return false;
	}
}