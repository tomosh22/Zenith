#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
namespace Zenith_Maths
{
	using Vector2 = glm::vec2;
	using UVector2 = glm::uvec2;
	using Vector2_64 = glm::dvec2;
	using Vector3 = glm::vec3;
	using Vector3_64 = glm::dvec3;
	using UVector4 = glm::uvec4;
	using Vector4 = glm::vec4;
	using Vector4_64 = glm::dvec4;
	using Matrix2 = glm::mat2;
	using Matrix2_64 = glm::dmat2;
	using Matrix3 = glm::mat3;
	using Matrix3_64 = glm::dmat3;
	using Matrix4 = glm::mat4;
	using Matrix4_64 = glm::dmat4;
	using Quat = glm::quat;
	using Quaternion = glm::quat;  // Alias for Quat for consistency
	template<typename T>
	static T Clamp(T xArg, T xMin, T xMax)
	{
		if (xArg > xMax)
		{
			return xMax;
		}
		if (xArg < xMin)
		{
			return xMin;
		}
		return xArg;
	}

	static constexpr double Pi = 3.14159265358979323846264338327950288;
	static constexpr double RadToDeg = 180/Pi;

	static Matrix4 PerspectiveProjection(const float fFOV, const float fAspect, const float fNear, const float fFar)
	{
		return glm::perspective(fFOV, fAspect, fNear, fFar);
	}

	static Matrix4 OrthographicProjection(const float fLeft, const float fRight, const float fBottom, const float fTop, const float fNear, const float fFar)
	{
		return glm::ortho(fLeft, fRight, fBottom, fTop, fNear, fFar);
	}

	static Matrix4 EulerRotationToMatrix4(float fDegrees, const Vector3& xAxis)
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