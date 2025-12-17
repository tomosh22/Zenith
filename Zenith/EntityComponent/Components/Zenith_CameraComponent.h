#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Zenith_Scene.h"

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
	//--------------------------------------------------------------------------
	// Editor UI - Renders component properties in the Properties panel
	//--------------------------------------------------------------------------
	void RenderPropertiesPanel()
	{
		if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// Camera type selection
			const char* szCameraTypes[] = { "Perspective", "Orthographic" };
			int iCameraType = static_cast<int>(m_eType);
			if (ImGui::Combo("Camera Type", &iCameraType, szCameraTypes, 2))
			{
				m_eType = static_cast<CameraType>(iCameraType);
			}

			ImGui::Separator();

			// Set as Main Camera button
			Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
			bool bIsMainCamera = false;
			if (xScene.GetMainCameraEntity() != nullptr)
			{
				bIsMainCamera = (xScene.GetMainCameraEntity()->GetEntityID() == m_xParentEntity.GetEntityID());
			}

			if (bIsMainCamera)
			{
				ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "This is the Main Camera");
			}
			else
			{
				if (ImGui::Button("Set as Main Camera"))
				{
					xScene.SetMainCameraEntity(m_xParentEntity);
					Zenith_Log("Set entity '%s' as main camera", m_xParentEntity.m_strName.c_str());
				}
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

				// Aspect ratio editing
				float fAspect = GetAspectRatio();
				if (ImGui::DragFloat("Aspect Ratio", &fAspect, 0.01f, 0.1f, 4.0f, "%.3f"))
				{
					m_fAspect = fAspect;
				}

				ImGui::Separator();

				// Pitch editing
				float fPitch = static_cast<float>(GetPitch());
				if (ImGui::DragFloat("Pitch", &fPitch, 0.01f, -1.5f, 1.5f, "%.3f rad"))
				{
					m_fPitch = fPitch;
				}

				// Yaw editing
				float fYaw = static_cast<float>(GetYaw());
				if (ImGui::DragFloat("Yaw", &fYaw, 0.01f, 0.0f, 6.28318f, "%.3f rad"))
				{
					m_fYaw = fYaw;
				}
			}
			else if (m_eType == CAMERA_TYPE_ORTHOGRAPHIC)
			{
				// Orthographic camera properties
				ImGui::DragFloat("Left", &m_fLeft, 1.0f);
				ImGui::DragFloat("Right", &m_fRight, 1.0f);
				ImGui::DragFloat("Top", &m_fTop, 1.0f);
				ImGui::DragFloat("Bottom", &m_fBottom, 1.0f);
				ImGui::DragFloat("Near", &m_fNear, 0.1f);
				ImGui::DragFloat("Far", &m_fFar, 1.0f);
			}

			ImGui::Separator();

			// Camera position editing
			// Use PushID to avoid ImGui ID collision with TransformComponent's Position field
			ImGui::PushID("CameraPosition");
			Zenith_Maths::Vector3 xPos;
			GetPosition(xPos);
			float afPos[3] = { static_cast<float>(xPos.x), static_cast<float>(xPos.y), static_cast<float>(xPos.z) };
			if (ImGui::DragFloat3("Position", afPos, 1.0f))
			{
				SetPosition({ afPos[0], afPos[1], afPos[2] });
			}
			ImGui::PopID();
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
