#include "Zenith.h"

#include "EntityComponent/Components/Zenith_LightComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "DataStream/Zenith_DataStream.h"

ZENITH_REGISTER_COMPONENT(Zenith_LightComponent, "Light")

// Serialization version history:
// Version 1: Initial implementation
static constexpr u_int uLIGHT_COMPONENT_VERSION = 1;

// Magic number constants for direction normalization
static constexpr float fDIRECTION_NORMALIZE_EPSILON = 0.0001f;

Zenith_LightComponent::Zenith_LightComponent(Zenith_Entity& xEntity)
	: m_xParentEntity(xEntity)
{
}

Zenith_Maths::Vector3 Zenith_LightComponent::GetWorldPosition() const
{
	Zenith_Maths::Vector3 xPos(0.0f);
	if (m_xParentEntity.IsValid() && m_xParentEntity.HasComponent<Zenith_TransformComponent>())
	{
		Zenith_TransformComponent& xTransform = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
		xTransform.GetPosition(xPos);
	}
	if (m_bUsePositionOffset)
	{
		xPos += m_xPositionOffset;
	}
	return xPos;
}

Zenith_Maths::Vector3 Zenith_LightComponent::GetWorldDirection() const
{
	// When direction offset is enabled, treat m_xDirectionOffset as an absolute world direction
	// This is set via SetWorldDirection() for convenience
	if (m_bUseDirectionOffset)
	{
		// Safely normalize - return default direction if zero-length to prevent NaN
		float fLength = Zenith_Maths::Length(m_xDirectionOffset);
		if (fLength < fDIRECTION_NORMALIZE_EPSILON)
		{
			return Zenith_Maths::Vector3(0.0f, 0.0f, -1.0f);  // Default forward
		}
		return m_xDirectionOffset / fLength;
	}

	// Default: use transform rotation applied to forward vector (-Z)
	Zenith_Maths::Vector3 xDir(0.0f, 0.0f, -1.0f);
	if (m_xParentEntity.IsValid() && m_xParentEntity.HasComponent<Zenith_TransformComponent>())
	{
		Zenith_TransformComponent& xTransform = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
		Zenith_Maths::Quat xRot;
		xTransform.GetRotation(xRot);
		xDir = xRot * Zenith_Maths::Vector3(0.0f, 0.0f, -1.0f);
	}
	return xDir;
}

void Zenith_LightComponent::SetWorldDirection(const Zenith_Maths::Vector3& xWorldDir)
{
	m_bUseDirectionOffset = true;
	// Safely normalize - use default direction if zero-length to prevent NaN
	float fLength = Zenith_Maths::Length(xWorldDir);
	if (fLength < fDIRECTION_NORMALIZE_EPSILON)
	{
		m_xDirectionOffset = Zenith_Maths::Vector3(0.0f, 0.0f, -1.0f);  // Default forward
		return;
	}
	m_xDirectionOffset = xWorldDir / fLength;
}

// Spot light angle constants (in radians)
static constexpr float fSPOT_MIN_INNER_ANGLE = 0.0f;          // 0 degrees
static constexpr float fSPOT_MAX_INNER_ANGLE = 1.5533f;       // ~89 degrees
static constexpr float fSPOT_MIN_OUTER_ANGLE = 0.01f;         // ~0.5 degrees
static constexpr float fSPOT_MAX_OUTER_ANGLE = 1.5708f;       // PI/2 (90 degrees)
static constexpr float fSPOT_MIN_ANGLE_MARGIN = 0.01f;        // Minimum gap between inner and outer

// UI editor constants
static constexpr float fUI_MAX_INTENSITY_LUX = 150000.0f;     // Max lux for directional lights (direct sunlight)
static constexpr float fUI_MAX_INTENSITY_LUMENS = 10000000.0f; // Max lumens for point/spot lights (stadium lights)
static constexpr float fUI_INTENSITY_DRAG_SPEED_LUX = 100.0f;  // Drag speed for directional light intensity
static constexpr float fUI_INTENSITY_DRAG_SPEED_LUMENS = 10.0f; // Drag speed for point/spot light intensity

void Zenith_LightComponent::ValidateSpotAngles()
{
	// Clamp outer first (it's the primary constraint)
	m_fSpotOuterAngle = Zenith_Maths::Clamp(m_fSpotOuterAngle, fSPOT_MIN_OUTER_ANGLE, fSPOT_MAX_OUTER_ANGLE);

	// Then constrain inner to be less than outer with minimum margin
	float fMaxInner = m_fSpotOuterAngle - fSPOT_MIN_ANGLE_MARGIN;
	m_fSpotInnerAngle = Zenith_Maths::Clamp(m_fSpotInnerAngle, fSPOT_MIN_INNER_ANGLE, fMaxInner);
}

void Zenith_LightComponent::SetSpotInnerAngle(float fAngle)
{
	m_fSpotInnerAngle = fAngle;
	ValidateSpotAngles();
}

void Zenith_LightComponent::SetSpotOuterAngle(float fAngle)
{
	m_fSpotOuterAngle = fAngle;
	ValidateSpotAngles();
}

void Zenith_LightComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write version first for future compatibility
	xStream << uLIGHT_COMPONENT_VERSION;

	xStream << static_cast<u_int>(m_eLightType);
	xStream << m_xColor;
	xStream << m_fIntensity;
	xStream << m_fRange;
	xStream << m_fSpotInnerAngle;
	xStream << m_fSpotOuterAngle;
	xStream << m_bCastShadows;
	xStream << m_bUsePositionOffset;
	xStream << m_xPositionOffset;
	xStream << m_bUseDirectionOffset;
	xStream << m_xDirectionOffset;
}

void Zenith_LightComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Read version for compatibility handling
	u_int uVersion;
	xStream >> uVersion;

	// Version 1 data (current)
	if (uVersion >= 1)
	{
		u_int uType;
		xStream >> uType;
		m_eLightType = static_cast<LIGHT_TYPE>(uType);
		xStream >> m_xColor;
		xStream >> m_fIntensity;
		xStream >> m_fRange;
		m_fRange = std::max(m_fRange, 0.1f);  // Clamp legacy data to minimum
		xStream >> m_fSpotInnerAngle;
		xStream >> m_fSpotOuterAngle;
		xStream >> m_bCastShadows;
		xStream >> m_bUsePositionOffset;
		xStream >> m_xPositionOffset;
		xStream >> m_bUseDirectionOffset;
		xStream >> m_xDirectionOffset;
	}

	// Future versions can add new data here:
	// if (uVersion >= 2) { xStream >> m_fNewField; }

	// Warn about unknown future versions (data may be ignored)
	if (uVersion > uLIGHT_COMPONENT_VERSION)
	{
		Zenith_Log(LOG_CATEGORY_ECS, "Warning: LightComponent version %u is newer than supported (%u), some data may be ignored",
			uVersion, uLIGHT_COMPONENT_VERSION);
	}
}

#ifdef ZENITH_TOOLS
void Zenith_LightComponent::RenderPropertiesPanel()
{
	if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// Light type selection
		const char* aszLightTypes[] = { "Point", "Spot", "Directional" };
		int iLightType = static_cast<int>(m_eLightType);
		if (ImGui::Combo("Light Type", &iLightType, aszLightTypes, LIGHT_TYPE_COUNT))
		{
			m_eLightType = static_cast<LIGHT_TYPE>(iLightType);
		}

		ImGui::Separator();

		// Color picker (linear RGB space)
		float afColor[3] = { m_xColor.x, m_xColor.y, m_xColor.z };
		if (ImGui::ColorEdit3("Color (Linear)", afColor))
		{
			m_xColor = { afColor[0], afColor[1], afColor[2] };
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Color must be in LINEAR RGB space (not sRGB)");
		}

		// Intensity with physical units
		// Minimum 0 to prevent subtractive lighting, maximum to prevent overflow
		if (m_eLightType == LIGHT_TYPE_DIRECTIONAL)
		{
			// Directional lights: lux (lm/m²)
			if (ImGui::DragFloat("Intensity (lux)", &m_fIntensity, fUI_INTENSITY_DRAG_SPEED_LUX, 0.0f, fUI_MAX_INTENSITY_LUX, "%.0f"))
			{
				m_fIntensity = Zenith_Maths::Clamp(m_fIntensity, 0.0f, fUI_MAX_INTENSITY_LUX);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Illuminance in lux (lm/m^2)\nOvercast: ~1000, Cloudy: ~10000, Sun: ~100000");
			}
		}
		else
		{
			// Point/Spot lights: lumens
			if (ImGui::DragFloat("Intensity (lm)", &m_fIntensity, fUI_INTENSITY_DRAG_SPEED_LUMENS, 0.0f, fUI_MAX_INTENSITY_LUMENS, "%.0f"))
			{
				m_fIntensity = Zenith_Maths::Clamp(m_fIntensity, 0.0f, fUI_MAX_INTENSITY_LUMENS);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Luminous power in lumens\n60W bulb: ~800, Studio light: ~5000");
			}
		}

		// Range (for point/spot)
		// Max 10km to prevent extreme light volumes
		if (m_eLightType != LIGHT_TYPE_DIRECTIONAL)
		{
			float fRange = m_fRange;
			if (ImGui::DragFloat("Range", &fRange, 0.5f, 0.1f, 10000.0f, "%.1f m"))
			{
				SetRange(fRange);  // Use setter for validation
			}
		}

		// Spot light specific
		if (m_eLightType == LIGHT_TYPE_SPOT)
		{
			ImGui::Separator();
			ImGui::Text("Spot Parameters");

			float fInnerDeg = glm::degrees(m_fSpotInnerAngle);
			float fOuterDeg = glm::degrees(m_fSpotOuterAngle);

			// Min 0 degrees matches SetSpotInnerAngle clamping behavior
			if (ImGui::SliderFloat("Inner Angle", &fInnerDeg, 0.0f, 89.0f, "%.1f deg"))
			{
				SetSpotInnerAngle(glm::radians(fInnerDeg));
			}
			// Min 1 degree to prevent degenerate cone
			if (ImGui::SliderFloat("Outer Angle", &fOuterDeg, 1.0f, 90.0f, "%.1f deg"))
			{
				SetSpotOuterAngle(glm::radians(fOuterDeg));
			}
		}

		ImGui::Separator();

		// Shadow toggle (reserved for future)
		ImGui::BeginDisabled();
		ImGui::Checkbox("Cast Shadows", &m_bCastShadows);
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::TextDisabled("(Not implemented)");

		ImGui::Separator();
		ImGui::Text("Transform Offsets");

		// Position offset
		ImGui::Checkbox("Use Position Offset", &m_bUsePositionOffset);
		if (m_bUsePositionOffset)
		{
			float afPos[3] = { m_xPositionOffset.x, m_xPositionOffset.y, m_xPositionOffset.z };
			if (ImGui::DragFloat3("Position Offset", afPos, 0.1f))
			{
				m_xPositionOffset = { afPos[0], afPos[1], afPos[2] };
			}
		}

		// Direction offset (for spot/directional)
		if (m_eLightType != LIGHT_TYPE_POINT)
		{
			ImGui::Checkbox("Use Direction Offset", &m_bUseDirectionOffset);
			if (m_bUseDirectionOffset)
			{
				// Display the normalized direction from GetWorldDirection()
				Zenith_Maths::Vector3 xNormalized = GetWorldDirection();
				float afDir[3] = { xNormalized.x, xNormalized.y, xNormalized.z };
				if (ImGui::DragFloat3("Direction Offset", afDir, 0.01f))
				{
					// Use setter to normalize and store
					SetWorldDirection({ afDir[0], afDir[1], afDir[2] });
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Direction is automatically normalized");
				}
			}
		}
	}
}
#endif
