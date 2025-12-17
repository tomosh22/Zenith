#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

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

	// Editor UI for behavior-specific properties
	void RenderPropertiesPanel() override
	{
#ifdef ZENITH_TOOLS
		ImGui::Checkbox("Fly Camera Enabled", &m_bFlyCamEnabled);
		ImGui::Text("Move Speed: %.1f", s_dMoveSpeed);
#endif
	}

private:
	bool m_bFlyCamEnabled = false;
	Zenith_Entity m_xParentEntity;
};
