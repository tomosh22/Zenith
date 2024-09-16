#include "Zenith.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"

const bool Zenith_TerrainComponent::IsVisible() const
{
	//#TO_TODO: this should be a camera frustum check against the terrain's encapsulating AABB
	const Zenith_CameraComponent& xCam = Zenith_Scene::GetCurrentScene().GetMainCamera();
	Zenith_Maths::Vector3 xCamPos;
	xCam.GetPosition(xCamPos);
	const Zenith_Maths::Vector2 xCamPos_2D(xCamPos.x, xCamPos.z);

	return (glm::length(xCamPos_2D - GetPosition_2D()) < xCam.GetFarPlane() * 2);
}
