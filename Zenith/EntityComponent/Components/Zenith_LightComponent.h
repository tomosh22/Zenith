#pragma once

#include "EntityComponent/Zenith_Entity.h"
#include "Maths/Zenith_Maths.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

// Light type enumeration
// Note: Values must not change for serialization compatibility
enum LIGHT_TYPE : u_int
{
	LIGHT_TYPE_POINT = 0,
	LIGHT_TYPE_SPOT = 1,
	LIGHT_TYPE_DIRECTIONAL = 2,
	LIGHT_TYPE_COUNT
};

// ============================================================================
// LIGHT INTENSITY UNITS (Physical)
// ============================================================================
//
// Point/Spot lights: Luminous power in lumens (lm)
//   Candle flame:       ~12 lm
//   40W incandescent:   ~450 lm
//   60W incandescent:   ~800 lm
//   100W incandescent:  ~1600 lm
//   Bright LED bulb:    ~1500 lm
//   Studio light:       ~5000 lm
//   Car headlight:      ~3000 lm
//
// Directional lights: Illuminance in lux (lm/m²)
//   Full moon:          ~0.25 lux
//   Street lighting:    ~10-50 lux
//   Office lighting:    ~300-500 lux
//   Overcast day:       ~1000 lux
//   Cloudy day:         ~10000 lux
//   Direct sunlight:    ~100000 lux
//
// COLOR SPACE:
//   Light color (m_xColor) must be in LINEAR RGB space.
//   sRGB values will appear incorrectly bright after gamma correction.
//
// DIRECTION CONVENTION:
//   Light direction points FROM the light source INTO the scene.
//   For spot/directional lights, this is the direction the light is "shining".
//   - GetWorldDirection() returns a normalized vector in this convention.
//   - The BRDF computation in the shader negates this to get the light-to-fragment
//     direction required for the standard L vector in lighting equations.
//   - All direction vectors are normalized on CPU before upload to GPU.
// ============================================================================

class Zenith_LightComponent
{
public:
	Zenith_LightComponent(Zenith_Entity& xEntity);
	~Zenith_LightComponent() = default;

	// Serialization
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	// Accessors
	LIGHT_TYPE GetLightType() const { return m_eLightType; }
	void SetLightType(LIGHT_TYPE eType) { m_eLightType = eType; }

	const Zenith_Maths::Vector3& GetColor() const { return m_xColor; }
	void SetColor(const Zenith_Maths::Vector3& xColor)
	{
		// Clamp to valid linear RGB range (0 to 10.0 for HDR support)
		m_xColor = Zenith_Maths::Vector3(
			std::clamp(xColor.x, 0.0f, 10.0f),
			std::clamp(xColor.y, 0.0f, 10.0f),
			std::clamp(xColor.z, 0.0f, 10.0f));
	}

	float GetIntensity() const { return m_fIntensity; }
	void SetIntensity(float fIntensity) { m_fIntensity = std::clamp(fIntensity, 0.0f, 10000000.0f); }  // 0 to 10M lumens

	float GetRange() const { return m_fRange; }
	void SetRange(float fRange) { m_fRange = std::clamp(fRange, 0.1f, 10000.0f); }  // 10cm to 10km

	float GetSpotInnerAngle() const { return m_fSpotInnerAngle; }
	void SetSpotInnerAngle(float fAngle);

	float GetSpotOuterAngle() const { return m_fSpotOuterAngle; }
	void SetSpotOuterAngle(float fAngle);

	bool GetCastShadows() const { return m_bCastShadows; }
	void SetCastShadows(bool bCast) { m_bCastShadows = bCast; }

	// Position offset accessors (adds to transform position)
	bool GetUsePositionOffset() const { return m_bUsePositionOffset; }
	void SetUsePositionOffset(bool bUse) { m_bUsePositionOffset = bUse; }
	const Zenith_Maths::Vector3& GetPositionOffset() const { return m_xPositionOffset; }
	void SetPositionOffset(const Zenith_Maths::Vector3& xOffset) { m_xPositionOffset = xOffset; }

	// Direction override accessors (when enabled, stores absolute world direction)
	bool GetUseDirectionOffset() const { return m_bUseDirectionOffset; }
	void SetUseDirectionOffset(bool bUse) { m_bUseDirectionOffset = bUse; }
	const Zenith_Maths::Vector3& GetDirectionOffset() const { return m_xDirectionOffset; }
	void SetDirectionOffset(const Zenith_Maths::Vector3& xOffset) { m_xDirectionOffset = xOffset; }

	// Transform helpers - get world position/direction from parent entity
	Zenith_Maths::Vector3 GetWorldPosition() const;
	Zenith_Maths::Vector3 GetWorldDirection() const;

	// Set absolute world direction (enables direction override mode)
	void SetWorldDirection(const Zenith_Maths::Vector3& xWorldDir);

	Zenith_Entity& GetParentEntity() { return m_xParentEntity; }
	const Zenith_Entity& GetParentEntity() const { return m_xParentEntity; }

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

private:
	// Unified validation for spot light angles - ensures constraints are met
	void ValidateSpotAngles();

	Zenith_Entity m_xParentEntity;

	LIGHT_TYPE m_eLightType = LIGHT_TYPE_POINT;
	Zenith_Maths::Vector3 m_xColor = { 1.0f, 1.0f, 1.0f };  // Linear RGB (not sRGB)
	// Intensity in lumens (point/spot) or lux (directional). Typical values:
	// - Candle: ~12 lm, 60W bulb: ~800 lm, Studio light: ~5000 lm
	// - Direct sunlight: ~100000 lux, Overcast: ~1000 lux
	float m_fIntensity = 800.0f;  // 800 lumens (60W incandescent equivalent)
	float m_fRange = 10.0f;

	// Spot light specific (angles in radians)
	float m_fSpotInnerAngle = 0.349066f;  // 20 degrees
	float m_fSpotOuterAngle = 0.523599f;  // 30 degrees

	bool m_bCastShadows = false;  // Reserved for future shadow mapping

	// Position/direction offsets (added to transform component values)
	bool m_bUsePositionOffset = false;
	Zenith_Maths::Vector3 m_xPositionOffset = { 0.0f, 0.0f, 0.0f };
	bool m_bUseDirectionOffset = false;
	Zenith_Maths::Vector3 m_xDirectionOffset = { 0.0f, 0.0f, 0.0f };
};
