#pragma once

#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"
#include "DataStream/Zenith_DataStream.h"
#include <cstdint>

// ============================================================================
// CB_TransitLines — Cities: Skylines-style public-transport LINES: each line is an
// ordered set of stops the player places along the roads. Unlike bus depots (which
// supply ridership CAPACITY), lines supply ridership REACH — only residents/jobs
// near a stop actually ride, so where you route the line matters. Pure data/logic
// (header-only) so it unit-tests headlessly + serializes; the CityManager renders
// the stop/line overlay and the buses drive the routes (CB_Traffic).
//
// The sim (CB_BuildingPlacement) asks IsNearAnyStop per building: with lines present,
// transit ridership is gated by the population those stops reach; with no lines, the
// depot-capacity model applies unchanged (backward-compatible).
// ============================================================================

struct CB_TransitStop
{
	float    m_fX    = 0.0f;
	float    m_fZ    = 0.0f;
	uint32_t m_uLine = 0u;   // which line this stop belongs to
};

class CB_TransitLines
{
public:
	static constexpr float    STOP_REACH = 150.0f;       // people within this of a stop can ride
	static constexpr uint32_t INVALID    = 0xFFFFFFFFu;

	void Reset()
	{
		m_axStops.Clear();
		m_uLineCount   = 0u;
		m_uCurrentLine = INVALID;
	}

	// Begin a new line; subsequent AddStop calls append to it. Returns the line id.
	uint32_t StartLine() { m_uCurrentLine = m_uLineCount; ++m_uLineCount; return m_uCurrentLine; }

	// Add a stop at a world point to the current line (auto-starts a line if none).
	void AddStop(float fX, float fZ)
	{
		if (m_uCurrentLine == INVALID) { StartLine(); }
		CB_TransitStop xS; xS.m_fX = fX; xS.m_fZ = fZ; xS.m_uLine = m_uCurrentLine;
		m_axStops.PushBack(xS);
	}

	uint32_t GetStopCount() const { return m_axStops.GetSize(); }
	uint32_t GetLineCount() const { return m_uLineCount; }
	uint32_t GetCurrentLine() const { return m_uCurrentLine; }
	const CB_TransitStop& GetStop(uint32_t i) const { return m_axStops.Get(i); }

	// Is a world point within STOP_REACH of any stop? (the sim's transit-access query)
	bool IsNearAnyStop(float fX, float fZ) const
	{
		const float fR2 = STOP_REACH * STOP_REACH;
		for (uint32_t i = 0; i < m_axStops.GetSize(); ++i)
		{
			const CB_TransitStop& xS = m_axStops.Get(i);
			const float dx = fX - xS.m_fX, dz = fZ - xS.m_fZ;
			if (dx * dx + dz * dz <= fR2) { return true; }
		}
		return false;
	}

	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		xStream << m_uLineCount;
		const uint32_t uN = m_axStops.GetSize();
		xStream << uN;
		for (uint32_t i = 0; i < uN; ++i)
		{
			const CB_TransitStop& xS = m_axStops.Get(i);
			xStream << xS.m_fX; xStream << xS.m_fZ; xStream << xS.m_uLine;
		}
	}
	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		Reset();
		xStream >> m_uLineCount;
		uint32_t uN = 0u;
		xStream >> uN;
		for (uint32_t i = 0; i < uN; ++i)
		{
			CB_TransitStop xS;
			xStream >> xS.m_fX; xStream >> xS.m_fZ; xStream >> xS.m_uLine;
			m_axStops.PushBack(xS);
		}
		m_uCurrentLine = (m_uLineCount > 0u) ? (m_uLineCount - 1u) : INVALID;
	}

private:
	Zenith_Vector<CB_TransitStop> m_axStops;
	uint32_t                      m_uLineCount   = 0u;
	uint32_t                      m_uCurrentLine = INVALID;
};
