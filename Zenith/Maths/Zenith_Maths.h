#include "glm/glm.hpp"
#include <glm/gtx/quaternion.hpp>
namespace Zenith_Maths
{
	using Vector2 = glm::vec2;
	using Vector3 = glm::vec3;
	using Vector4 = glm::vec4;
	using Matrix3 = glm::mat3;
	using Matrix4 = glm::mat4;
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
}