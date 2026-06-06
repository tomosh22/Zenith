#pragma once

#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"
#include "CityBuilder/Source/CB_RoadGraph.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"

// ============================================================================
// CB_Traffic — SimCity / Cities: Skylines-style traffic. Traffic is NOT a fixed
// decorative pool; it is an EMERGENT product of the city's activity:
//
//   * Every vehicle is a TRIP: it spawns at a residence (origin node), routes via
//     A* (FindPath) over the road graph to a job/shop (destination node), drives
//     the path turn-by-turn, and DESPAWNS when it arrives — then a new trip takes
//     its slot. No origins+destinations (i.e. no built homes/jobs) ⇒ no trips, so
//     a bare road network carries NO traffic.
//   * The number of concurrent trips scales with POPULATION (the manager passes a
//     population-derived target), so traffic grows with the city.
//   * Roads have a CAPACITY by class (small < medium < large). When the vehicles on
//     a segment exceed it the segment is CONGESTED and the cars on it slow down —
//     busy corridors back up, exactly like the games.
//
// Telemetry (CB_TrafficStats) records trips started/completed, the live vehicle
// count, the busiest segment load, and the congested-segment count, so the human
// Zenith_InputSimulator test can PROVE the demand-driven behaviour. Pure logic +
// debug-primitive rendering.
// ============================================================================

struct CB_Vehicle
{
	static constexpr uint32_t MAX_TRIP_SEGS = 64; // segments per routed trip (truncated beyond; avoids the Win32 MAX_PATH macro)

	bool     m_bActive   = false;
	uint32_t m_uSeg      = 0xFFFFFFFFu;            // current segment
	uint32_t m_uFromNode = 0xFFFFFFFFu;           // node entered the current segment from
	uint32_t m_uGoalNode = 0xFFFFFFFFu;           // trip destination node
	float    m_fT        = 0.0f;                  // [0,1] along the current segment from m_uFromNode
	float    m_fSpeed    = 14.0f;                 // free-flow m/s (scaled down by congestion)
	float    m_fTripTime = 0.0f;                  // seconds since this trip started
	uint32_t m_uPathLen  = 0;                     // segments in m_auPath
	uint32_t m_uPathIdx  = 0;                     // index of the current segment in m_auPath
	uint32_t m_auPath[MAX_TRIP_SEGS] = {};        // routed segment sequence (origin→dest)
	Zenith_Maths::Vector3 m_xPos   = Zenith_Maths::Vector3(0.0f);     // world (updated each tick)
	Zenith_Maths::Vector2 m_xFwd   = Zenith_Maths::Vector2(0.0f, 1.0f);
	Zenith_Maths::Vector3 m_xColor = Zenith_Maths::Vector3(0.85f, 0.85f, 0.88f);
};

// Demand-driven traffic telemetry — the proof that traffic behaves like the games.
struct CB_TrafficStats
{
	uint32_t m_uActive         = 0;   // live vehicles (each is an in-progress trip)
	uint32_t m_uTripsStarted   = 0;   // cumulative trips dispatched
	uint32_t m_uTripsCompleted = 0;   // cumulative trips that reached their destination
	uint32_t m_uMaxSegmentLoad = 0;   // most vehicles on any one segment this frame
	uint32_t m_uCongestedSegs  = 0;   // segments whose load exceeded their capacity this frame
	float    m_fAvgTripTime    = 0.0f;// EMA of completed-trip durations (s)
};

class CB_Traffic
{
public:
	static constexpr uint32_t MAX_VEHICLES = 80;   // hard cap on concurrent trips

	void Reset();

	// Demand-driven update: advance live trips (congestion slows cars on full segments),
	// despawn arrivals, then dispatch new trips from random origin nodes (homes) to random
	// destination nodes (jobs/shops) up to uTargetVehicles. Empty origins/dests ⇒ no traffic.
	void Update(const CB_RoadGraph& xGraph, const CB_TerrainHeightfield& xField, float fDt,
	            const Zenith_Vector<uint32_t>& xOriginNodes, const Zenith_Vector<uint32_t>& xDestNodes,
	            uint32_t uTargetVehicles);

	// Draw each vehicle as a small oriented box (windowed).
	void Render() const;

	uint32_t               GetActiveVehicleCount() const { return m_xStats.m_uActive; }
	uint32_t               GetVehicleSlotCount()   const { return m_axVehicles.GetSize(); }
	const CB_Vehicle&      GetVehicle(uint32_t i)  const { return m_axVehicles.Get(i); }
	const CB_TrafficStats& GetStats()              const { return m_xStats; }

	// A* over the road graph: shortest segment path from node uStart to uGoal by summed
	// segment length. Fills outSegs (start→goal order); false if unreachable / invalid.
	static bool FindPath(const CB_RoadGraph& xGraph, uint32_t uStart, uint32_t uGoal, Zenith_Vector<uint32_t>& outSegs);

private:
	uint32_t NextRand();
	bool     SpawnTrip(const CB_RoadGraph& xGraph, const Zenith_Vector<uint32_t>& xOrigins,
	                   const Zenith_Vector<uint32_t>& xDests, CB_Vehicle& xV);
	void     AdvanceVehicle(const CB_RoadGraph& xGraph, const CB_TerrainHeightfield& xField, CB_Vehicle& xV, float fDt);
	static uint32_t SegCapacity(const CB_RoadSegment& xSeg);

	Zenith_Vector<CB_Vehicle> m_axVehicles;
	Zenith_Vector<uint32_t>   m_auSegLoad;     // per-segment live vehicle count (rebuilt each frame)
	Zenith_Vector<uint32_t>   m_xPathScratch;  // FindPath output buffer (reused)
	CB_TrafficStats           m_xStats;
	uint32_t                  m_uRng = 0x1234567u;
};
