#pragma once

#include "Collections/Zenith_Vector.h"
#include "DataStream/Zenith_DataStream.h"
#include "CityBuilder/Source/CB_Policy.h"
#include <cmath>
#include <cstdint>

// ============================================================================
// CB_Districts — named regions the player paints over the city, each carrying its
// own set of CB_Policy ordinances, plus a city-wide policy set. Pure data/logic
// (header-only, no engine deps) so it unit-tests headlessly and serializes for
// save/load; the CityManager renders the overlay + drives the paint tool.
//
// The simulation (CB_BuildingPlacement) asks GetPolicyMaskAt(x,z) per building and
// applies each policy's effect scaled by the fraction of buildings it covers — so a
// city-wide policy hits everything while a district policy only touches its area.
// ============================================================================

struct CB_District
{
	float    m_fCentreX   = 0.0f;
	float    m_fCentreZ   = 0.0f;
	float    m_fRadius    = 0.0f;
	uint32_t m_uPolicyMask = 0u;
	bool     m_bActive    = false;
};

class CB_Districts
{
public:
	static constexpr float DEFAULT_RADIUS = 120.0f;   // a freshly-painted district's radius
	static constexpr float MERGE_RADIUS   = 160.0f;   // paint within this of a district's centre grows it

	void Reset()
	{
		m_axDistricts.Clear();
		m_uCityPolicyMask = 0u;
		m_uCurrent        = INVALID;
	}

	static constexpr uint32_t INVALID = 0xFFFFFFFFu;

	// --- city-wide policies (apply everywhere) ---
	void SetCityPolicy(CB_EPolicy ePolicy, bool bOn)
	{
		if (bOn) { m_uCityPolicyMask |=  CB_PolicyBit(ePolicy); }
		else     { m_uCityPolicyMask &= ~CB_PolicyBit(ePolicy); }
	}
	void ToggleCityPolicy(CB_EPolicy ePolicy) { m_uCityPolicyMask ^= CB_PolicyBit(ePolicy); }
	bool IsCityPolicy(CB_EPolicy ePolicy) const { return (m_uCityPolicyMask & CB_PolicyBit(ePolicy)) != 0u; }
	uint32_t GetCityPolicyMask() const { return m_uCityPolicyMask; }

	// --- districts ---
	// Paint at (x,z): grows the nearest district whose centre is within MERGE_RADIUS to
	// include the point, else creates a new district there. Returns its index + makes it
	// current. (Circular districts — simple + enough for policy scoping.)
	uint32_t PaintDistrict(float fX, float fZ)
	{
		uint32_t uBest = INVALID;
		float    fBestD2 = MERGE_RADIUS * MERGE_RADIUS;
		for (uint32_t i = 0; i < m_axDistricts.GetSize(); ++i)
		{
			const CB_District& xD = m_axDistricts.Get(i);
			if (!xD.m_bActive) { continue; }
			const float dx = fX - xD.m_fCentreX, dz = fZ - xD.m_fCentreZ;
			const float d2 = dx * dx + dz * dz;
			if (d2 < fBestD2) { fBestD2 = d2; uBest = i; }
		}
		if (uBest != INVALID)
		{
			CB_District& xD = m_axDistricts.Get(uBest);
			const float dx = fX - xD.m_fCentreX, dz = fZ - xD.m_fCentreZ;
			const float fD = std::sqrt(dx * dx + dz * dz);
			if (fD + 1.0f > xD.m_fRadius) { xD.m_fRadius = fD + 30.0f; }   // grow to include the point
			m_uCurrent = uBest;
			return uBest;
		}
		CB_District xNew;
		xNew.m_fCentreX = fX; xNew.m_fCentreZ = fZ; xNew.m_fRadius = DEFAULT_RADIUS; xNew.m_bActive = true;
		m_axDistricts.PushBack(xNew);
		m_uCurrent = m_axDistricts.GetSize() - 1u;
		return m_uCurrent;
	}

	uint32_t GetCurrent() const { return m_uCurrent; }

	void SetDistrictPolicy(uint32_t uIdx, CB_EPolicy ePolicy, bool bOn)
	{
		if (uIdx >= m_axDistricts.GetSize() || !m_axDistricts.Get(uIdx).m_bActive) { return; }
		uint32_t& uMask = m_axDistricts.Get(uIdx).m_uPolicyMask;
		if (bOn) { uMask |=  CB_PolicyBit(ePolicy); }
		else     { uMask &= ~CB_PolicyBit(ePolicy); }
	}
	void ToggleDistrictPolicy(uint32_t uIdx, CB_EPolicy ePolicy)
	{
		if (uIdx >= m_axDistricts.GetSize() || !m_axDistricts.Get(uIdx).m_bActive) { return; }
		m_axDistricts.Get(uIdx).m_uPolicyMask ^= CB_PolicyBit(ePolicy);
	}

	uint32_t           GetSlotCount() const     { return m_axDistricts.GetSize(); }
	const CB_District& Get(uint32_t i) const    { return m_axDistricts.Get(i); }
	uint32_t GetActiveCount() const
	{
		uint32_t u = 0u;
		for (uint32_t i = 0; i < m_axDistricts.GetSize(); ++i) { if (m_axDistricts.Get(i).m_bActive) { ++u; } }
		return u;
	}

	// The effective policy mask at a world point: city-wide OR'd with every district
	// whose disc contains the point.
	uint32_t GetPolicyMaskAt(float fX, float fZ) const
	{
		uint32_t uMask = m_uCityPolicyMask;
		for (uint32_t i = 0; i < m_axDistricts.GetSize(); ++i)
		{
			const CB_District& xD = m_axDistricts.Get(i);
			if (!xD.m_bActive) { continue; }
			const float dx = fX - xD.m_fCentreX, dz = fZ - xD.m_fCentreZ;
			if (dx * dx + dz * dz <= xD.m_fRadius * xD.m_fRadius) { uMask |= xD.m_uPolicyMask; }
		}
		return uMask;
	}

	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		xStream << m_uCityPolicyMask;
		const uint32_t uCount = m_axDistricts.GetSize();
		xStream << uCount;
		for (uint32_t i = 0; i < uCount; ++i)
		{
			const CB_District& xD = m_axDistricts.Get(i);
			xStream << xD.m_fCentreX; xStream << xD.m_fCentreZ; xStream << xD.m_fRadius;
			xStream << xD.m_uPolicyMask; xStream << xD.m_bActive;
		}
	}
	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		Reset();
		xStream >> m_uCityPolicyMask;
		uint32_t uCount = 0u;
		xStream >> uCount;
		for (uint32_t i = 0; i < uCount; ++i)
		{
			CB_District xD;
			xStream >> xD.m_fCentreX; xStream >> xD.m_fCentreZ; xStream >> xD.m_fRadius;
			xStream >> xD.m_uPolicyMask; xStream >> xD.m_bActive;
			m_axDistricts.PushBack(xD);
		}
	}

private:
	Zenith_Vector<CB_District> m_axDistricts;
	uint32_t                   m_uCityPolicyMask = 0u;
	uint32_t                   m_uCurrent        = INVALID;
};
