#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"

class PlayerController_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	// Serialization support - type name and factory registration
	ZENITH_BEHAVIOUR_TYPE_NAME(PlayerController_Behaviour)

	static constexpr double s_dMoveSpeed = 20;
	enum CameraType
	{
		CAMERA_TYPE_PERSPECTIVE,
		CAMERA_TYPE_ORTHOGRAPHIC,
		CAMERA_TYPE_MAX
	};
	PlayerController_Behaviour() = delete;
	PlayerController_Behaviour(Zenith_Entity& xParentEntity);
	~PlayerController_Behaviour() = default;

	void OnUpdate(const float fDt) ZENITH_FINAL override;
	void OnCreate() ZENITH_FINAL override {}

	void Shoot();

private:
	bool m_bFlyCamEnabled = false;
	Zenith_Entity m_xParentEntity;
};
