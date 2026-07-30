#include "Zenith.h"

#include "EntityComponent/Components/Zenith_SunComponent.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

namespace
{
	constexpr u_int uSUN_COMPONENT_VERSION = 1u;
	constexpr float fDIRECTION_EPSILON = 0.0001f;
}

Zenith_SunComponent::Zenith_SunComponent(Zenith_Entity&)
{
}

float Zenith_SunComponent::WrapDegrees(float fDegrees)
{
	float fWrapped = fmodf(fDegrees, 360.0f);
	if (fWrapped < 0.0f)
	{
		fWrapped += 360.0f;
	}
	return fWrapped;
}

void Zenith_SunComponent::SetDirectionMode(SUN_DIRECTION_MODE eMode)
{
	m_eDirectionMode = eMode < SUN_DIRECTION_MODE_COUNT ? eMode : SUN_DIRECTION_MODE_VECTOR;
}

void Zenith_SunComponent::SetDirection(const Zenith_Maths::Vector3& xDirection)
{
	const float fLength = Zenith_Maths::Length(xDirection);
	m_xDirection = fLength >= fDIRECTION_EPSILON
		? xDirection / fLength
		: Zenith_GetDefaultSunDirection();
	m_eDirectionMode = SUN_DIRECTION_MODE_VECTOR;
}

void Zenith_SunComponent::SetTimeOfDayAngleDegrees(float fDegrees)
{
	m_fTimeOfDayAngleDegrees = WrapDegrees(fDegrees);
	m_eDirectionMode = SUN_DIRECTION_MODE_TIME_OF_DAY;
}

void Zenith_SunComponent::SetOrbitAzimuthDegrees(float fDegrees)
{
	m_fOrbitAzimuthDegrees = WrapDegrees(fDegrees);
}

Zenith_Maths::Vector3 Zenith_SunComponent::GetWorldDirection() const
{
	if (m_eDirectionMode == SUN_DIRECTION_MODE_TIME_OF_DAY)
	{
		const float fOrbit = glm::radians(m_fTimeOfDayAngleDegrees);
		const float fAzimuth = glm::radians(m_fOrbitAzimuthDegrees);
		return Zenith_Maths::Vector3(
			-cosf(fOrbit) * cosf(fAzimuth),
			-sinf(fOrbit),
			-cosf(fOrbit) * sinf(fAzimuth));
	}
	return m_xDirection;
}

void Zenith_SunComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << uSUN_COMPONENT_VERSION;
	xStream << static_cast<u_int>(m_eDirectionMode);
	xStream << m_xDirection;
	xStream << m_fTimeOfDayAngleDegrees;
	xStream << m_fOrbitAzimuthDegrees;
}

void Zenith_SunComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	u_int uVersion = 0u;
	xStream >> uVersion;
	Zenith_Assert(uVersion == uSUN_COMPONENT_VERSION,
		"Unsupported SunComponent version %u (expected %u)", uVersion, uSUN_COMPONENT_VERSION);

	u_int uMode = 0u;
	xStream >> uMode;
	xStream >> m_xDirection;
	xStream >> m_fTimeOfDayAngleDegrees;
	xStream >> m_fOrbitAzimuthDegrees;

	SetDirection(m_xDirection);
	SetTimeOfDayAngleDegrees(m_fTimeOfDayAngleDegrees);
	SetOrbitAzimuthDegrees(m_fOrbitAzimuthDegrees);
	SetDirectionMode(static_cast<SUN_DIRECTION_MODE>(uMode));
}

#ifdef ZENITH_TOOLS
void Zenith_SunComponent::RenderPropertiesPanel()
{
	if (!ImGui::CollapsingHeader("Sun", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}

	const char* aszModes[] = { "Direction Vector", "Time of Day" };
	int iMode = static_cast<int>(m_eDirectionMode);
	if (ImGui::Combo("Direction Mode", &iMode, aszModes, SUN_DIRECTION_MODE_COUNT))
	{
		SetDirectionMode(static_cast<SUN_DIRECTION_MODE>(iMode));
	}

	if (m_eDirectionMode == SUN_DIRECTION_MODE_VECTOR)
	{
		float afDirection[3] = { m_xDirection.x, m_xDirection.y, m_xDirection.z };
		if (ImGui::DragFloat3("Travel Direction", afDirection, 0.01f, -1.0f, 1.0f))
		{
			SetDirection(Zenith_Maths::Vector3(afDirection[0], afDirection[1], afDirection[2]));
		}
	}
	else
	{
		float fTime = m_fTimeOfDayAngleDegrees;
		if (ImGui::SliderFloat("Time-of-Day Angle", &fTime, 0.0f, 360.0f, "%.1f deg"))
		{
			SetTimeOfDayAngleDegrees(fTime);
		}
		float fAzimuth = m_fOrbitAzimuthDegrees;
		if (ImGui::SliderFloat("Orbit Azimuth", &fAzimuth, 0.0f, 360.0f, "%.1f deg"))
		{
			SetOrbitAzimuthDegrees(fAzimuth);
		}
		ImGui::TextDisabled("0 sunrise, 90 noon, 180 sunset, 270 midnight");
	}

	const Zenith_Maths::Vector3 xDirection = GetWorldDirection();
	ImGui::Text("Resolved travel direction: %.3f, %.3f, %.3f",
		xDirection.x, xDirection.y, xDirection.z);
	ImGui::Separator();
	ImGui::TextWrapped("Sun colour and radiance are not authored here. They derive from atmospheric transmittance and the engine's single radiometric anchor.");
}
#endif

#include "EntityComponent/Components/Zenith_SunComponent.Tests.inl"
