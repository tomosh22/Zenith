#pragma once

#include "ZenithECS/Zenith_Entity.h"
#include "Maths/Zenith_Maths.h"
#include "Core/Zenith_EnvironmentAuthority.h"

// A scene Sun owns only solar geometry. It deliberately has no colour,
// intensity, radiance, exposure or any equivalent artistic brightness
// multiplier field: Flux derives the direct solar key from the engine's one
// radiometric anchor (policy, never scene-authored) and per-channel
// atmospheric transmittance. It is resolved together with a co-located
// Zenith_AtmosphereComponent via Zenith_EnvironmentAuthorityData.
//
// TODO(second-atmosphere-light): there is exactly ONE Sun. A moon -- Unreal
// supports two atmosphere lights precisely for this -- is not a second
// component, it is a spine change: g_xSunDir_Pad / g_xSunColour_Pad are single
// float4s in the VIEW descriptor set (Flux/Flux_PersistentSetLayouts.h), the sky
// solvers take one xSunDir and render one disk, CSM fits one direction, and the
// IBL capture convolves one sun. Until then a night scene gets its visibility
// from ordinary authored lights, which is what DevilsPlayground does.
//
// TODO(look-override): there is deliberately NO artistic sun colour/intensity
// override, and adding one is NOT a small change of mind -- ZM-D-171 removed
// exactly those knobs (dbg_SunColour, the IBL 0.5, the key 0.14) because they
// let the sky and the key light disagree, which is the single most common
// lighting bug in Unity/Unreal projects. If a project needs a wider look space,
// widen what is PHYSICALLY authorable (ozone, turbidity, ground albedo, planet
// radius -- see Shaders/Common/Atmosphere.slang) rather than reintroducing a
// channel in which sun, sky and ambient can drift apart.
enum SUN_DIRECTION_MODE : u_int
{
	SUN_DIRECTION_MODE_VECTOR = 0,
	SUN_DIRECTION_MODE_TIME_OF_DAY = 1,
	SUN_DIRECTION_MODE_COUNT
};

class Zenith_SunComponent
{
public:
	explicit Zenith_SunComponent(Zenith_Entity& xEntity);
	~Zenith_SunComponent() = default;

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	SUN_DIRECTION_MODE GetDirectionMode() const { return m_eDirectionMode; }
	void SetDirectionMode(SUN_DIRECTION_MODE eMode);

	const Zenith_Maths::Vector3& GetAuthoredDirection() const { return m_xDirection; }
	void SetDirection(const Zenith_Maths::Vector3& xDirection);

	// Daily-orbit convention: 0 degrees = sunrise horizon, 90 = noon,
	// 180 = sunset horizon, 270 = midnight (sun below the world).
	float GetTimeOfDayAngleDegrees() const { return m_fTimeOfDayAngleDegrees; }
	void SetTimeOfDayAngleDegrees(float fDegrees);

	// World-space bearing to the sunrise point, measured around +Y.
	float GetOrbitAzimuthDegrees() const { return m_fOrbitAzimuthDegrees; }
	void SetOrbitAzimuthDegrees(float fDegrees);

	Zenith_Maths::Vector3 GetWorldDirection() const;

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

private:
	static float WrapDegrees(float fDegrees);

	// Not serialized: the owning entity, kept so the TOOLS panel can report
	// whether THIS entity won the environment authority.
	Zenith_Entity         m_xParentEntity;
	SUN_DIRECTION_MODE    m_eDirectionMode = SUN_DIRECTION_MODE_VECTOR;
	Zenith_Maths::Vector3 m_xDirection = Zenith_GetDefaultSunDirection();
	float                 m_fTimeOfDayAngleDegrees = 90.0f;
	float                 m_fOrbitAzimuthDegrees = 0.0f;
};
