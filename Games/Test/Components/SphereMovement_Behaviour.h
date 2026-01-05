#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

class HookesLaw_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	// Serialization support - type name and factory registration
	ZENITH_BEHAVIOUR_TYPE_NAME(HookesLaw_Behaviour)

	HookesLaw_Behaviour() = delete;
	HookesLaw_Behaviour(Zenith_Entity& xParentEntity);
	~HookesLaw_Behaviour() = default;

	void OnUpdate(const float fDt) ZENITH_FINAL override;
	void OnAwake() ZENITH_FINAL override {}

	void SetDesiredPosition(const Zenith_Maths::Vector3& xPos) { m_xDesiredPosition = xPos; }
	const Zenith_Maths::Vector3& GetDesiredPosition() const { return m_xDesiredPosition; }

	// Editor UI for behavior-specific properties
	void RenderPropertiesPanel() override
	{
#ifdef ZENITH_TOOLS
		float afDesiredPos[3] = { 
			static_cast<float>(m_xDesiredPosition.x), 
			static_cast<float>(m_xDesiredPosition.y), 
			static_cast<float>(m_xDesiredPosition.z) 
		};
		if (ImGui::DragFloat3("Desired Position", afDesiredPos, 0.1f))
		{
			m_xDesiredPosition = { afDesiredPos[0], afDesiredPos[1], afDesiredPos[2] };
		}
#endif
	}

	// Serialize behavior parameters
	void WriteParametersToDataStream(Zenith_DataStream& xStream) const override
	{
		xStream << m_xDesiredPosition;
	}

	void ReadParametersFromDataStream(Zenith_DataStream& xStream) override
	{
		xStream >> m_xDesiredPosition;
	}

private:
	Zenith_Maths::Vector3 m_xDesiredPosition = { 0,0,0 };

	Zenith_Entity m_xParentEntity;
};

class RotationBehaviour_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	// Serialization support - type name and factory registration
	ZENITH_BEHAVIOUR_TYPE_NAME(RotationBehaviour_Behaviour)

	RotationBehaviour_Behaviour() = delete;
	RotationBehaviour_Behaviour(Zenith_Entity& xParentEntity);
	~RotationBehaviour_Behaviour() = default;

	void OnUpdate(const float fDt) ZENITH_FINAL override;
	void OnAwake() ZENITH_FINAL override {}

	void SetAngularVel(const Zenith_Maths::Vector3& xVel) { m_xAngularVel = xVel; }
	const Zenith_Maths::Vector3& GetAngularVel() const { return m_xAngularVel; }

	// Editor UI for behavior-specific properties
	void RenderPropertiesPanel() override
	{
#ifdef ZENITH_TOOLS
		float afAngularVel[3] = { 
			static_cast<float>(m_xAngularVel.x), 
			static_cast<float>(m_xAngularVel.y), 
			static_cast<float>(m_xAngularVel.z) 
		};
		if (ImGui::DragFloat3("Angular Velocity", afAngularVel, 0.01f))
		{
			m_xAngularVel = { afAngularVel[0], afAngularVel[1], afAngularVel[2] };
		}
#endif
	}

	// Serialize behavior parameters
	void WriteParametersToDataStream(Zenith_DataStream& xStream) const override
	{
		xStream << m_xAngularVel;
	}

	void ReadParametersFromDataStream(Zenith_DataStream& xStream) override
	{
		xStream >> m_xAngularVel;
	}

private:
	Zenith_Maths::Vector3 m_xAngularVel = { 0,0,0 };

	Zenith_Entity m_xParentEntity;
};
