#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"

class SphereMovement_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	static constexpr double s_dMoveSpeed = 250;
	enum CameraType
	{
		CAMERA_TYPE_PERSPECTIVE,
		CAMERA_TYPE_ORTHOGRAPHIC,
		CAMERA_TYPE_MAX
	};
	SphereMovement_Behaviour() = delete;
	SphereMovement_Behaviour(Zenith_Entity& xParentEntity);
	~SphereMovement_Behaviour() = default;

	void OnUpdate(const float fDt) ZENITH_FINAL override;
	void OnCreate() ZENITH_FINAL override {}

	void SetInitialPosition(const Zenith_Maths::Vector3& xPos) { m_xInitialPosition = xPos; }
	void SetAmplitude(const float fAmp) { m_fAmplitude = fAmp; }

private:
	Zenith_Maths::Vector3 m_xInitialPosition = { 0,0,0 };
	float m_fAmplitude = 0.;

	Zenith_Entity m_xParentEntity;
};
