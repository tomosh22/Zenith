#include "glm/glm.hpp"
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
namespace Zenith_Maths
{
	using Vector2 = glm::vec2;
	using Vector2_64 = glm::dvec2;
	using Vector3 = glm::vec3;
	using Vector3_64 = glm::dvec3;
	using Vector4 = glm::vec4;
	using Vector4_64 = glm::dvec4;
	using Matrix3 = glm::mat3;
	using Matrix3_64 = glm::dmat3;
	using Matrix4 = glm::mat4;
	using Matrix4_64 = glm::dmat4;
	using Quat = glm::quat;
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

	static Matrix4 PerspectiveProjection(const float fFOV, const float fAspect, const float fNear, const float fFar)
	{
		return glm::perspective(fFOV, fAspect, fNear, fFar);
	}

	static Matrix4 OrthographicProjection(const float fLeft, const float fRight, const float fBottom, const float fTop, const float fNear, const float fFar)
	{
		return glm::ortho(fLeft, fRight, fBottom, fTop, fNear, fFar);
	}
}