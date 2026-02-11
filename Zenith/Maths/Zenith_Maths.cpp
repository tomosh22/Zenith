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
}
