#include "Zenith.h"

namespace Zenith_Maths
{
	Matrix4 PerspectiveProjection(const float fFOV, const float fAspect, const float fNear, const float fFar)
	{
		return glm::perspective(fFOV, fAspect, fNear, fFar);
	}

	Matrix4 OrthographicProjection(const float fLeft, const float fRight, const float fBottom, const float fTop, const float fNear, const float fFar)
	{
		return glm::ortho(fLeft, fRight, fBottom, fTop, fNear, fFar);
	}

	Matrix4 EulerRotationToMatrix4(float fDegrees, const Vector3& xAxis)
	{
		Matrix4 xRet = glm::identity<Zenith_Maths::Matrix4>();

		float fCos = cos(glm::radians(fDegrees));
		float fSin = sin(glm::radians(fDegrees));

		xRet[0][0] = (xAxis.x * xAxis.x) * (1.0f - fCos) + fCos;
		xRet[0][1] = (xAxis.y * xAxis.x) * (1.0f - fCos) + (xAxis.z * fSin);
		xRet[0][2] = (xAxis.z * xAxis.x) * (1.0f - fCos) - (xAxis.y * fSin);

		xRet[1][0] = (xAxis.x * xAxis.y) * (1.0f - fCos) - (xAxis.z * fSin);
		xRet[1][1] = (xAxis.y * xAxis.y) * (1.0f - fCos) + fCos;
		xRet[1][2] = (xAxis.z * xAxis.y) * (1.0f - fCos) + (xAxis.x * fSin);

		xRet[2][0] = (xAxis.x * xAxis.z) * (1.0f - fCos) + (xAxis.y * fSin);
		xRet[2][1] = (xAxis.y * xAxis.z) * (1.0f - fCos) - (xAxis.x * fSin);
		xRet[2][2] = (xAxis.z * xAxis.z) * (1.0f - fCos) + fCos;

		return xRet;
	}

	//--------------------------------------------------------------------------
	// Authoring math (see the header). Every glm formula below is transcribed
	// verbatim from Middleware/glm-master so these return the SAME values glm
	// does — the point is not different math, it is math that is compiled once,
	// here, under a pinned floating-point model instead of being re-derived in
	// every /fp:fast translation unit that happens to instantiate the template.
	//--------------------------------------------------------------------------
	ZENITH_AUTHORING_DETERMINISM_BEGIN

	// glm/ext/quaternion_trigonometric.inl: qua(cos(a * 0.5), v * sin(a * 0.5)).
	// v is a cardinal unit axis, so two of the three products are 0 * s == 0.
	// (glm::quat's constructor takes w first.)
	Quat AuthoringRotationX(float fRadians)
	{
		const float fHalf = fRadians * 0.5f;
		return Quat(cosf(fHalf), sinf(fHalf), 0.0f, 0.0f);
	}

	Quat AuthoringRotationY(float fRadians)
	{
		const float fHalf = fRadians * 0.5f;
		return Quat(cosf(fHalf), 0.0f, sinf(fHalf), 0.0f);
	}

	Quat AuthoringRotationZ(float fRadians)
	{
		const float fHalf = fRadians * 0.5f;
		return Quat(cosf(fHalf), 0.0f, 0.0f, sinf(fHalf));
	}

	// glm/detail/type_quat.inl, operator*= (which operator* forwards to).
	Quat AuthoringQuatMul(const Quat& xQ, const Quat& xP)
	{
		return Quat(
			xQ.w * xP.w - xQ.x * xP.x - xQ.y * xP.y - xQ.z * xP.z,
			xQ.w * xP.x + xQ.x * xP.w + xQ.y * xP.z - xQ.z * xP.y,
			xQ.w * xP.y + xQ.y * xP.w + xQ.z * xP.x - xQ.x * xP.z,
			xQ.w * xP.z + xQ.z * xP.w + xQ.x * xP.y - xQ.y * xP.x);
	}

	// glm/gtc/quaternion.inl mat3_cast, widened to 4x4, with the translation in
	// column 3 and each rotation column scaled. Composing T * R * S longhand is
	// EXACT: T's contribution to columns 0-2 is 1*c + 0 + 0 + t*0, and R's fourth
	// column is (0,0,0,1), so the full 4x4 product reduces to precisely this.
	Matrix4 AuthoringTRS(const Vector3& xPosition, const Quat& xRotation, const Vector3& xScale)
	{
		const float qxx = xRotation.x * xRotation.x;
		const float qyy = xRotation.y * xRotation.y;
		const float qzz = xRotation.z * xRotation.z;
		const float qxz = xRotation.x * xRotation.z;
		const float qxy = xRotation.x * xRotation.y;
		const float qyz = xRotation.y * xRotation.z;
		const float qwx = xRotation.w * xRotation.x;
		const float qwy = xRotation.w * xRotation.y;
		const float qwz = xRotation.w * xRotation.z;

		Matrix4 xRet(1.0f);
		xRet[0][0] = (1.0f - 2.0f * (qyy + qzz)) * xScale.x;
		xRet[0][1] = (2.0f * (qxy + qwz)) * xScale.x;
		xRet[0][2] = (2.0f * (qxz - qwy)) * xScale.x;
		xRet[0][3] = 0.0f;

		xRet[1][0] = (2.0f * (qxy - qwz)) * xScale.y;
		xRet[1][1] = (1.0f - 2.0f * (qxx + qzz)) * xScale.y;
		xRet[1][2] = (2.0f * (qyz + qwx)) * xScale.y;
		xRet[1][3] = 0.0f;

		xRet[2][0] = (2.0f * (qxz + qwy)) * xScale.z;
		xRet[2][1] = (2.0f * (qyz - qwx)) * xScale.z;
		xRet[2][2] = (1.0f - 2.0f * (qxx + qyy)) * xScale.z;
		xRet[2][3] = 0.0f;

		xRet[3][0] = xPosition.x;
		xRet[3][1] = xPosition.y;
		xRet[3][2] = xPosition.z;
		xRet[3][3] = 1.0f;
		return xRet;
	}

	// glm/trigonometric.inl: degrees * 0.01745329251994329576923690768489.
	float AuthoringRadians(float fDegrees)
	{
		return fDegrees * static_cast<float>(0.01745329251994329576923690768489);
	}

	ZENITH_AUTHORING_DETERMINISM_END

	// Deliberately OUTSIDE the authoring-determinism pin above -- see the header.
	// A decomposed pose is consumed by Jolt at runtime and never serialized, so it
	// has nothing to be bit-identical WITH.
	void DecomposeTRS(const Matrix4& xMatrix, Vector3& xPositionOut,
		Quat& xRotationOut, Vector3& xScaleOut)
	{
		xPositionOut = Vector3(xMatrix[3]);

		// Column lengths are the scale. A degenerate column is floored so the
		// division below cannot produce inf/NaN, and its basis direction falls back
		// to the matching axis unit vector.
		constexpr float fMIN_AXIS_LENGTH = 1e-6f;
		Vector3 axBasis[3];
		for (int iCol = 0; iCol < 3; ++iCol)
		{
			const Vector3 xColumn(xMatrix[iCol]);
			const float fLength = glm::length(xColumn);
			if (fLength < fMIN_AXIS_LENGTH)
			{
				xScaleOut[iCol] = fMIN_AXIS_LENGTH;
				axBasis[iCol] = Vector3(0.0f);
				axBasis[iCol][iCol] = 1.0f;
			}
			else
			{
				xScaleOut[iCol] = fLength;
				axBasis[iCol] = xColumn / fLength;
			}
		}

		const Matrix3 xRotationBasis(axBasis[0], axBasis[1], axBasis[2]);
		// normalize is MANDATORY, not defensive: quat_cast of an almost-orthonormal
		// basis returns an almost-unit quat, and Jolt asserts IsNormalized().
		xRotationOut = glm::normalize(glm::quat_cast(xRotationBasis));
	}
}
