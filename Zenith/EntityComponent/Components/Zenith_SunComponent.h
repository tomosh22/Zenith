#pragma once

#include "ZenithECS/Zenith_Entity.h"
#include "Maths/Zenith_Maths.h"
#include "Core/Zenith_SunAuthority.h"

// A scene Sun owns only solar geometry. It deliberately has no colour,
// intensity, or radiance field: Flux derives the direct key from the one
// atmosphere anchor and per-channel transmittance.
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

	SUN_DIRECTION_MODE    m_eDirectionMode = SUN_DIRECTION_MODE_VECTOR;
	Zenith_Maths::Vector3 m_xDirection = Zenith_GetDefaultSunDirection();
	float                 m_fTimeOfDayAngleDegrees = 90.0f;
	float                 m_fOrbitAzimuthDegrees = 0.0f;
};
