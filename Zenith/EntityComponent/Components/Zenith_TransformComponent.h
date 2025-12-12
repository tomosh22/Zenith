#pragma once
#include "EntityComponent/Zenith_Entity.h"
#include "Maths/Zenith_Maths.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#include "EntityComponent/Zenith_ComponentRegistry.h"
#endif

namespace JPH
{
	class Body;
}

class Zenith_TransformComponent
{
public:
	Zenith_TransformComponent(Zenith_Entity& xEntity);
	~Zenith_TransformComponent();

	// Serialization methods for Zenith_DataStream
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	void SetPosition(const Zenith_Maths::Vector3 xPos);
	void SetRotation(const Zenith_Maths::Quat xRot);
	void SetScale(const Zenith_Maths::Vector3 xScale);

	void GetPosition(Zenith_Maths::Vector3& xPos);
	void GetRotation(Zenith_Maths::Quat& xRot);
	void GetScale(Zenith_Maths::Vector3& xScale);

	Zenith_Maths::Vector3 m_xScale = { 1.,1.,1. };
	JPH::Body* m_pxRigidBody = nullptr;

	void BuildModelMatrix(Zenith_Maths::Matrix4& xMatOut);


#ifdef ZENITH_TOOLS
	//--------------------------------------------------------------------------
	// Editor UI - Renders component properties in the Properties panel
	//--------------------------------------------------------------------------
	void RenderPropertiesPanel()
	{
		if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
		{
			Zenith_Maths::Vector3 pos, scale;
			Zenith_Maths::Quat rot;
			GetPosition(pos);
			GetRotation(rot);
			GetScale(scale);
			
			// Position editing
			float position[3] = { pos.x, pos.y, pos.z };
			if (ImGui::DragFloat3("Position", position, 0.1f))
			{
				SetPosition({ position[0], position[1], position[2] });
			}
			
			// Rotation editing - convert quaternion to Euler angles for UI
			Zenith_Maths::Vector3 euler = glm::degrees(glm::eulerAngles(rot));
			float rotation[3] = { euler.x, euler.y, euler.z };
			if (ImGui::DragFloat3("Rotation", rotation, 1.0f))
			{
				Zenith_Maths::Vector3 newEuler = glm::radians(Zenith_Maths::Vector3(rotation[0], rotation[1], rotation[2]));
				SetRotation(Zenith_Maths::Quat(newEuler));
			}
			
			// Scale editing
			float scaleValues[3] = { scale.x, scale.y, scale.z };
			if (ImGui::DragFloat3("Scale", scaleValues, 0.1f))
			{
				SetScale({ scaleValues[0], scaleValues[1], scaleValues[2] });
			}
		}
	}

	static void RegisterWithEditor()
	{
		Zenith_ComponentRegistry::Get().RegisterComponent<Zenith_TransformComponent>("Transform");
	}
#endif

private:
	friend class Zenith_ColliderComponent;
	
	Zenith_Maths::Vector3 m_xPosition = { 0.0, 0.0, 0.0 };
	Zenith_Maths::Quat m_xRotation = { 1.0, 0.0, 0.0, 0.0 };

	Zenith_Entity m_xParentEntity;

};
