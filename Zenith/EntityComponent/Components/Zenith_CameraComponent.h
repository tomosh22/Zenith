#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "EntityComponent/Zenith_ComponentRegistry.h"
#endif

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
	struct PerspectiveParams
	{
		Zenith_Maths::Vector3 m_xPosition = {0.f, 0.f, 0.f};
		float m_fPitch = 0.f;
		float m_fYaw = 0.f;
		float m_fFOV = 60.f;
		float m_fNear = 0.1f;
		float m_fFar = 1000.f;
		float m_fAspectRatio = 16.f / 9.f;
	};

	Zenith_CameraComponent() = default;
	Zenith_CameraComponent(Zenith_Entity& xParentEntity);
	~Zenith_CameraComponent() = default;
	void InitialisePerspective(const PerspectiveParams& xParams);

	// Serialization methods for Zenith_DataStream
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);
	void BuildViewMatrix(Zenith_Maths::Matrix4& xOut) const;
	void BuildProjectionMatrix(Zenith_Maths::Matrix4& xOut) const;

	Zenith_Maths::Vector3 ScreenSpaceToWorldSpace(Zenith_Maths::Vector3 xScreenSpace);

	void SetPosition(const Zenith_Maths::Vector3 xPos) { m_xPosition = xPos; }
	void GetPosition(Zenith_Maths::Vector3& xOut) const;
	//#TO w = 0 for padding
	void GetPosition(Zenith_Maths::Vector4& xOut);
	void GetPosition(Zenith_Maths::Vector3& xOut);

	void GetFacingDir(Zenith_Maths::Vector3& xOut) const;

	const double GetPitch() const { return m_fPitch; }
	void SetPitch(const double fPitch) { m_fPitch = fPitch; }
	const double GetYaw() const { return m_fYaw; }
	void SetYaw(const double fYaw) { m_fYaw = fYaw; }

	Zenith_Maths::Vector3 GetUpDir();

	const float GetNearPlane() const { return m_fNear; }
	void SetNearPlane(float fNear) { m_fNear = fNear; }
	const float GetFarPlane() const { return m_fFar; }
	void SetFarPlane(float fFar) { m_fFar = fFar; }

	const float GetFOV() const { return m_fFOV; }
	void SetFOV(float fFOV) { m_fFOV = fFOV; }
	const float GetAspectRatio() const { return m_fAspect; }
	void SetAspectRatio(float fAspect) { m_fAspect = fAspect; }

	// Get parent entity
	Zenith_Entity& GetParentEntity() { return m_xParentEntity; }
	const Zenith_Entity& GetParentEntity() const { return m_xParentEntity; }

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

private:
	// Safe defaults to prevent division by zero / NaN in BuildProjectionMatrix
	float m_fNear = 0.1f;           // Near plane must be > 0
	float m_fFar = 1000.0f;         // Far plane must be > near
	float m_fLeft = -1.0f;          // Orthographic defaults
	float m_fRight = 1.0f;
	float m_fTop = 1.0f;
	float m_fBottom = -1.0f;
	float m_fFOV = 60.0f;           // Field of view in degrees
	double m_fYaw = 0;
	double m_fPitch = 0;
	float m_fAspect = 16.0f / 9.0f; // Common aspect ratio
	Zenith_Maths::Vector3 m_xPosition = { 0,0,0 };
	CameraType m_eType = CAMERA_TYPE_PERSPECTIVE;

	Zenith_Entity m_xParentEntity;

};
