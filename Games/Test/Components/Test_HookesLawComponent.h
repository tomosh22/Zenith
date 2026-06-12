#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

class Test_HookesLawComponent
{
public:
	Test_HookesLawComponent() = delete;
	Test_HookesLawComponent(Zenith_Entity& xParentEntity);

	void OnUpdate(const float fDt);
	void OnAwake() {}

	void SetDesiredPosition(const Zenith_Maths::Vector3& xPos) { m_xDesiredPosition = xPos; }
	const Zenith_Maths::Vector3& GetDesiredPosition() const { return m_xDesiredPosition; }

	// Editor UI for component-specific properties
	void RenderPropertiesPanel()
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

	// Serialize component parameters
	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		const uint32_t uVersion = 1;
		xStream << uVersion;
		xStream << m_xDesiredPosition;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		uint32_t uVersion = 0;
		xStream >> uVersion;
		xStream >> m_xDesiredPosition;
	}

private:
	Zenith_Maths::Vector3 m_xDesiredPosition = { 0,0,0 };

	Zenith_Entity m_xParentEntity;
};
