#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"

class Zenith_CameraBehaviour : Zenith_ScriptBehaviour
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
	Zenith_CameraBehaviour() = delete;
	Zenith_CameraBehaviour(Zenith_ScriptComponent& xScriptComponent);
	~Zenith_CameraBehaviour() {}
	void InitialisePerspective(const Zenith_Maths::Vector3& xPos, const float fPitch, const float fYaw, const float fFOV, const float fNear, const float fFar, const float fAspectRatio);
	void BuildViewMatrix(Zenith_Maths::Matrix4& xOut) const;
	void BuildProjectionMatrix(Zenith_Maths::Matrix4& xOut) const;

	Zenith_Maths::Vector3 ScreenSpaceToWorldSpace(Zenith_Maths::Vector3 xScreenSpace);

	void UpdateRotation(const float fDt);
	void OnUpdate(const float fDt) override;
	void OnCreate() override {}

	void GetPosition(Zenith_Maths::Vector3& xOut);

	//#TO w = 0 for padding
	void GetPosition(Zenith_Maths::Vector4& xOut);

private:
	float m_fNear = 0;
	float m_fFar = 0;
	float m_fLeft = 0;
	float m_fRight = 0;
	float m_fTop = 0;
	float m_fBottom = 0;
	float m_fFOV = 0;
	double m_fYaw = 0;
	double m_fPitch = 0;
	float m_fAspect = 0;
	Zenith_Maths::Vector3 m_xPosition = {0,0,0};
	CameraType m_eType = CAMERA_TYPE_MAX;

	Zenith_ScriptComponent& m_xScriptComponent;
};
