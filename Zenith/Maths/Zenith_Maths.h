#include "glm/glm.hpp"
namespace Zenith_Maths
{
	using Vector2 = glm::vec2;
	using Vector3 = glm::vec3;
	using Vector4 = glm::vec4;
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