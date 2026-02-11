#pragma once

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#pragma warning(push, 0)
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#pragma warning(pop)
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

	Matrix4 PerspectiveProjection(const float fFOV, const float fAspect, const float fNear, const float fFar);
	Matrix4 OrthographicProjection(const float fLeft, const float fRight, const float fBottom, const float fTop, const float fNear, const float fFar);
	Matrix4 EulerRotationToMatrix4(float fDegrees, const Vector3& xAxis);

	// ========== GLM Wrapper Functions for Flux_Primitives ==========

	// Vector operations
	static inline Vector3 Normalize(const Vector3& v)
	{
		return glm::normalize(v);
	}

	static inline float Length(const Vector3& v)
	{
		return glm::length(v);
	}

	static inline float LengthSq(const Vector3& v)
	{
		return glm::dot(v, v);
	}

	static inline float Dot(const Vector3& a, const Vector3& b)
	{
		return glm::dot(a, b);
	}

	static inline Vector3 Cross(const Vector3& a, const Vector3& b)
	{
		return glm::cross(a, b);
	}

	// Matrix transformation operations
	static inline Matrix4 Translate(const Matrix4& m, const Vector3& v)
	{
		return glm::translate(m, v);
	}

	static inline Matrix4 Scale(const Matrix4& m, const Vector3& v)
	{
		return glm::scale(m, v);
	}

	static inline Matrix4 Rotate(const Matrix4& m, float angle, const Vector3& axis)
	{
		return glm::rotate(m, angle, axis);
	}

	// Quaternion operations
	static inline Quaternion AngleAxis(float angle, const Vector3& axis)
	{
		return glm::angleAxis(angle, axis);
	}

	static inline Matrix4 Mat4Cast(const Quaternion& q)
	{
		return glm::mat4_cast(q);
	}

	static inline Quaternion QuatCast(const Matrix4& m)
	{
		return glm::quat_cast(m);
	}

	static inline Quaternion QuatLookAt(const Vector3& direction, const Vector3& up)
	{
		return glm::quatLookAt(direction, up);
	}

	// Rotate a vector by a quaternion
	static inline Vector3 RotateVector(const Vector3& v, const Quaternion& q)
	{
		return q * v;
	}

	// Create quaternion from euler angles (pitch, yaw, roll in radians)
	static inline Quaternion QuatFromEuler(float fPitch, float fYaw, float fRoll)
	{
		return glm::quat(Vector3(fPitch, fYaw, fRoll));
	}
}