#include "Zenith.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Profiling/Zenith_Profiling.h"

const bool Zenith_TerrainComponent::IsVisible(const float fVisibilityMultiplier, const Zenith_CameraComponent& xCam) const
{
	Zenith_Profiling::BeginProfile(ZENITH_PROFILE_INDEX__VISIBILITY_CHECK);
	//#TO_TODO: this should be a camera frustum check against the terrain's encapsulating AABB
	Zenith_Maths::Vector3 xCamPos;
	xCam.GetPosition(xCamPos);
	const Zenith_Maths::Vector2 xCamPos_2D(xCamPos.x, xCamPos.z);

	bool bRet = (glm::length(xCamPos_2D - GetPosition_2D()) < xCam.GetFarPlane() * 2 * fVisibilityMultiplier);
	Zenith_Profiling::EndProfile(ZENITH_PROFILE_INDEX__VISIBILITY_CHECK);
	return bRet;
}
