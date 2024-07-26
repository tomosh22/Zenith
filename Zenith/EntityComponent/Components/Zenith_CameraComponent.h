#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"

class Zenith_CameraComponent
{
	friend class Zenith_ScriptComponent;
public:
	enum CameraType
	{
		CAMERA_TYPE_PERSPECTIVE,
		CAMERA_TYPE_ORTHOGRAPHIC,
		CAMERA_TYPE_MAX
	};
	Zenith_CameraComponent() = delete;
	Zenith_CameraComponent(Zenith_Entity& xParentEntity);
	~Zenith_CameraComponent() = default;
	void InitialisePerspective(const Zenith_Maths::Vector3& xPos, const float fPitch, const float fYaw, const float fFOV, const float fNear, const float fFar, const float fAspectRatio);
	void BuildViewMatrix(Zenith_Maths::Matrix4& xOut) const;
	void BuildProjectionMatrix(Zenith_Maths::Matrix4& xOut) const;

	Zenith_Maths::Vector3 ScreenSpaceToWorldSpace(Zenith_Maths::Vector3 xScreenSpace);

	void SetPosition(const Zenith_Maths::Vector3 xPos) { m_xPosition = xPos; }
	void GetPosition(Zenith_Maths::Vector3& xOut);
	//#TO w = 0 for padding
	void GetPosition(Zenith_Maths::Vector4& xOut);

	double GetPitch() { return m_fPitch; }
	void SetPitch(const double fPitch) { m_fPitch = fPitch; }
	double GetYaw() { return m_fYaw; }
	void SetYaw(const double fYaw) { m_fYaw = fYaw; }

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

	Zenith_Entity m_xParentEntity;
};
