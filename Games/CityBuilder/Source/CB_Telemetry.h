#pragma once

#include "Telemetry/Zenith_Telemetry.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include <cstdint>

// ============================================================================
// CB_Telemetry — CityBuilder's telemetry layer on top of the engine's
// game-agnostic Zenith_Telemetry recorder, mirroring DevilsPlayground's
// DPTelemetry:
//
//   * CB_EventType         — the CityBuilder event schema (stable uint16 IDs).
//   * Hooks (RAII)         — subscribes to the CB_On* dispatcher events and
//                            routes each into Zenith_Telemetry::RecordEvent.
//   * Begin / End          — bracket a recording; End writes .ztlm + .json
//                            (+ optional CSV) for offline / Claude inspection.
//   * SampleCity           — polled per sample-frame: a CitySnapshot event
//                            (population / treasury / demand …) + a camera
//                            FrameSample, the way DP polls villager positions.
//   * Summarize / LogSummary — fold the recorded events into a verdict digest
//                            so a play session can be evaluated in-process.
//
// All calls are main-thread only (same contract as the recorder + dispatcher).
// ============================================================================

namespace CB_Telemetry
{
	enum class CB_EventType : uint16_t
	{
		None = 0,
		SessionStart,
		SessionEnd,
		CitySnapshot,    // ints=[pop, buildings, roads, jobs]  floats=[treasury, resD, comD, indD]
		ToolSelected,    // ints=[tool]                          label=tool name
		RoadPlaced,      // ints=[x, z]
		ZonePainted,     // ints=[x, z, zone]
		ServicePlaced,   // ints=[x, z, buildingType]
		BuildingGrew,    // ints=[x, z, buildingType, occupants]
		Bulldozed,       // ints=[x, z]
		PauseToggled,    // ints=[isPaused]
		Milestone,       // ints=[population, threshold]
		Saved,           // ints=[roads, buildings]  floats=[treasury]
		Loaded,          // ints=[roads, buildings]  floats=[treasury]
		_Count
	};

	const char* EventTypeToString(uint16_t uType);

	// ---- Recording lifecycle ----
	void Begin(const char* szScene);
	bool End(const char* szBinPath,
	         const char* szJsonPath,
	         const char* szFramesCsvPath = nullptr,
	         const char* szEventsCsvPath = nullptr);
	bool IsRecording();

	// ---- Per-frame driving (call once per game frame from the loop/test) ----
	void NextFrame();
	bool ShouldSampleThisFrame();
	void SampleCity();      // CitySnapshot event + camera FrameSample (polls the active CityManager)
	void EmitSessionEnd();  // closing bookend with the final city stats

	// ---- RAII subscription holder. Construct = start observing; destruct =
	//      stop. Subscribes to every CB_On* event and records it. ----
	class Hooks
	{
	public:
		Hooks();
		~Hooks();
		Hooks(const Hooks&)            = delete;
		Hooks& operator=(const Hooks&) = delete;

	private:
		Zenith_EventHandle m_xToolSelected  = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xRoadPlaced    = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xZonePainted   = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xServicePlaced = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xBuildingGrew  = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xBulldozed     = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xPauseToggled  = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xMilestone     = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xSaved         = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xLoaded        = INVALID_EVENT_HANDLE;
	};

	// ---- Evaluation: a digest folded from the recorded event stream. ----
	struct Summary
	{
		uint32_t uTotalEvents    = 0;
		uint32_t uSnapshots      = 0;
		uint32_t uToolSelections = 0;
		uint32_t uRoadsPlaced    = 0;
		uint32_t uZonesPainted   = 0;
		uint32_t uServicesPlaced = 0;
		uint32_t uBuildingsGrew  = 0;
		uint32_t uBulldozed      = 0;
		uint32_t uPauseToggles   = 0;
		uint32_t uMilestones     = 0;
		bool     bSaved          = false;
		bool     bLoaded         = false;
		uint32_t uPeakPopulation = 0;
		uint32_t uPeakBuildings  = 0;
		float    fFinalTreasury  = 0.0f;
	};
	Summary Summarize();                       // reads GetRecorder().GetEvents()
	void    LogSummary(const Summary& xSummary);
}
