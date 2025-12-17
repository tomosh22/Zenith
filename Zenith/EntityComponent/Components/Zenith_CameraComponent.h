#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
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
	Zenith_CameraComponent() = default;
	Zenith_CameraComponent(Zenith_Entity& xParentEntity);
	~Zenith_CameraComponent() = default;
	void InitialisePerspective(const Zenith_Maths::Vector3& xPos, const float fPitch, const float fYaw, const float fFOV, const float fNear, const float fFar, const float fAspectRatio);

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
	const float GetFarPlane() const { return m_fFar; }

	const float GetFOV() const { return m_fFOV; }
	const float GetAspectRatio() const { return m_fAspect; }

	// Get parent entity
	Zenith_Entity& GetParentEntity() { return m_xParentEntity; }
	const Zenith_Entity& GetParentEntity() const { return m_xParentEntity; }

#ifdef ZENITH_TOOLS
	//--------------------------------------------------------------------------
	// Editor UI - Renders component properties in the Properties panel
	//--------------------------------------------------------------------------
	void RenderPropertiesPanel()
	{
		if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// Camera type
			const char* szCameraTypes[] = { "Perspective", "Orthographic" };
			if (m_eType < CAMERA_TYPE_MAX)
			{
				ImGui::Text("Type: %s", szCameraTypes[m_eType]);
			}

			ImGui::Separator();

			// Editable camera properties for perspective camera
			if (m_eType == CAMERA_TYPE_PERSPECTIVE)
			{
				// FOV editing
				float fFOV = GetFOV();
				if (ImGui::SliderFloat("FOV", &fFOV, 30.0f, 120.0f, "%.1f"))
				{
					m_fFOV = fFOV;
				}

				// Near plane editing
				float fNear = GetNearPlane();
				if (ImGui::DragFloat("Near Plane", &fNear, 0.01f, 0.001f, 10.0f, "%.3f"))
				{
					m_fNear = fNear;
				}

				// Far plane editing
				float fFar = GetFarPlane();
				if (ImGui::DragFloat("Far Plane", &fFar, 10.0f, 10.0f, 10000.0f, "%.1f"))
				{
					m_fFar = fFar;
				}

				ImGui::Separator();

				// Pitch and yaw (read-only display, as they're controlled by scripts/input)
				ImGui::Text("Pitch: %.2f degrees", GetPitch());
				ImGui::Text("Yaw: %.2f degrees", GetYaw());

				// Aspect ratio (read-only)
				ImGui::Text("Aspect Ratio: %.3f", GetAspectRatio());
			}

			ImGui::Separator();

			// Camera position (from position, read-only as it's usually controlled by scripts)
			Zenith_Maths::Vector3 xPos;
			GetPosition(xPos);
			ImGui::Text("Position: (%.2f, %.2f, %.2f)", xPos.x, xPos.y, xPos.z);
		}
	}
#endif

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
	Zenith_Maths::Vector3 m_xPosition = { 0,0,0 };
	CameraType m_eType = CAMERA_TYPE_PERSPECTIVE;

	Zenith_Entity m_xParentEntity;

public:
#ifdef ZENITH_TOOLS
	// Static registration function called by ComponentRegistry::Initialise()
	static void RegisterWithEditor()
	{
		Zenith_ComponentRegistry::Get().RegisterComponent<Zenith_CameraComponent>("Camera");
	}
#endif
};
