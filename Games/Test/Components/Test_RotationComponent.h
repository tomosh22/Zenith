#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

class Test_RotationComponent
{
public:
	Test_RotationComponent() = delete;
	Test_RotationComponent(Zenith_Entity& xParentEntity);

	void OnUpdate(const float fDt);
	void OnAwake() {}

	void SetAngularVel(const Zenith_Maths::Vector3& xVel) { m_xAngularVel = xVel; }
	const Zenith_Maths::Vector3& GetAngularVel() const { return m_xAngularVel; }

	// Editor UI for component-specific properties
	void RenderPropertiesPanel()
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

	// Serialize component parameters
	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		const uint32_t uVersion = 1;
		xStream << uVersion;
		xStream << m_xAngularVel;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		uint32_t uVersion = 0;
		xStream >> uVersion;
		xStream >> m_xAngularVel;
	}

private:
	Zenith_Maths::Vector3 m_xAngularVel = { 0,0,0 };

	Zenith_Entity m_xParentEntity;
};
