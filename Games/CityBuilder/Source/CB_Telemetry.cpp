#include "Zenith.h"

#include "CityBuilder/Source/CB_Telemetry.h"
#include "CityBuilder/Source/CB_Events.h"
#include "CityBuilder/Source/CB_ToolSystem.h"
#include "CityBuilder/Components/CB_CityManager_Behaviour.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"

namespace CB_Telemetry
{
	static constexpr float fCB_FIXED_DT = 1.0f / 60.0f;

	// Keep in sync with CB_EventType.
	const char* EventTypeToString(uint16_t uType)
	{
		switch (static_cast<CB_EventType>(uType))
		{
		case CB_EventType::None:          return "None";
		case CB_EventType::SessionStart:  return "SessionStart";
		case CB_EventType::SessionEnd:    return "SessionEnd";
		case CB_EventType::CitySnapshot:  return "CitySnapshot";
		case CB_EventType::ToolSelected:  return "ToolSelected";
		case CB_EventType::RoadPlaced:    return "RoadPlaced";
		case CB_EventType::ZonePainted:   return "ZonePainted";
		case CB_EventType::ServicePlaced: return "ServicePlaced";
		case CB_EventType::BuildingGrew:  return "BuildingGrew";
		case CB_EventType::Bulldozed:     return "Bulldozed";
		case CB_EventType::PauseToggled:  return "PauseToggled";
		case CB_EventType::Milestone:     return "Milestone";
		case CB_EventType::Saved:         return "Saved";
		case CB_EventType::Loaded:        return "Loaded";
		case CB_EventType::_Count:
		default:
			return nullptr;   // JSON/CSV exporter falls back to the numeric value
		}
	}

	// Byte-by-byte copy into a fixed buffer (avoids MSVC strncpy deprecation
	// under /W4-as-errors). Mirrors DPTelemetry::CopyToFixed.
	static void CopyToFixed(char* pszDst, size_t uDstSize, const char* pszSrc)
	{
		if (pszDst == nullptr || uDstSize == 0u) return;
		if (pszSrc == nullptr) { pszDst[0] = '\0'; return; }
		const size_t uMax = uDstSize - 1u;
		size_t uI = 0;
		for (; uI < uMax && pszSrc[uI] != '\0'; ++uI) { pszDst[uI] = pszSrc[uI]; }
		pszDst[uI] = '\0';
	}

	// Low-level emit. Every Hooks subscription + the meta/sample helpers route
	// through here into the engine recorder.
	static void EmitEvent(CB_EventType eType,
	                      int32_t i0 = 0, int32_t i1 = 0, int32_t i2 = 0, int32_t i3 = 0,
	                      float f0 = 0.0f, float f1 = 0.0f, float f2 = 0.0f, float f3 = 0.0f,
	                      const char* szLabel = nullptr)
	{
		Zenith_Telemetry::Event xE;
		xE.uEventType = static_cast<uint16_t>(eType);
		xE.xPayload.aiInts[0] = i0; xE.xPayload.aiInts[1] = i1;
		xE.xPayload.aiInts[2] = i2; xE.xPayload.aiInts[3] = i3;
		xE.xPayload.afFloats[0] = f0; xE.xPayload.afFloats[1] = f1;
		xE.xPayload.afFloats[2] = f2; xE.xPayload.afFloats[3] = f3;
		CopyToFixed(xE.xPayload.szLabel,  sizeof(xE.xPayload.szLabel),  szLabel);
		CopyToFixed(xE.xPayload.szSource, sizeof(xE.xPayload.szSource), "CityBuilder");
		Zenith_Telemetry::GetRecorder().RecordEvent(xE);
	}

	// ====================== Lifecycle ======================
	void Begin(const char* szScene)
	{
		Zenith_Telemetry::Header xHeader;
		xHeader.strSceneName        = (szScene != nullptr) ? szScene : "City";
		xHeader.fFixedDt            = fCB_FIXED_DT;
		xHeader.uSamplePeriodFrames = 6;    // 10 Hz at 60 fps
#ifdef ZENITH_DEBUG
		xHeader.strBuildConfig = "Debug";
#else
		xHeader.strBuildConfig = "Release";
#endif
		xHeader.strPersonalityName = "HumanSession";
		Zenith_Telemetry::GetRecorder().Begin(xHeader);
		EmitEvent(CB_EventType::SessionStart);
	}

	bool IsRecording()          { return Zenith_Telemetry::GetRecorder().IsRecording(); }
	void NextFrame()            { Zenith_Telemetry::GetRecorder().NextFrame(); }
	bool ShouldSampleThisFrame(){ return Zenith_Telemetry::GetRecorder().ShouldSampleThisFrame(); }

	void SampleCity()
	{
		Zenith_Telemetry::Recorder& xRec = Zenith_Telemetry::GetRecorder();
		if (!xRec.IsRecording()) return;
		CB_CityManager_Behaviour* pxMgr = CB_CityManager_Behaviour::GetActive();
		if (pxMgr == nullptr) return;

		const CB_CityStats& xS = pxMgr->GetStats();
		EmitEvent(CB_EventType::CitySnapshot,
			static_cast<int32_t>(xS.m_uPopulation),
			static_cast<int32_t>(pxMgr->GetBuildings().GetActiveCount()),
			static_cast<int32_t>(pxMgr->GetRoads().GetRoadCellCount()),
			static_cast<int32_t>(xS.m_uJobs),
			xS.m_fTreasury, xS.m_fResDemand, xS.m_fComDemand, xS.m_fIndDemand);

		// Camera FrameSample — "what was the player looking at" replay aid.
		Zenith_Telemetry::FrameSample xF;
		xF.uFrameIdx = xRec.GetFrameIdx();
		xF.fTimeS    = static_cast<float>(xF.uFrameIdx) * fCB_FIXED_DT;
		if (Zenith_CameraComponent* pxCam = Zenith_GetMainCameraAcrossScenes())
		{
			Zenith_Maths::Vector3 xPos(0.0f);
			pxCam->GetPosition(xPos);
			xF.xCamera.xPos         = xPos;
			xF.xCamera.fOrbitYawRad = static_cast<float>(pxCam->GetYaw());
			xF.xCamera.bValid       = 1;
		}
		xRec.RecordFrame(xF);
	}

	void EmitSessionEnd()
	{
		if (!Zenith_Telemetry::GetRecorder().IsRecording()) return;
		if (CB_CityManager_Behaviour* pxMgr = CB_CityManager_Behaviour::GetActive())
		{
			const CB_CityStats& xS = pxMgr->GetStats();
			EmitEvent(CB_EventType::SessionEnd,
				static_cast<int32_t>(xS.m_uPopulation),
				static_cast<int32_t>(pxMgr->GetBuildings().GetActiveCount()),
				static_cast<int32_t>(pxMgr->GetRoads().GetRoadCellCount()),
				static_cast<int32_t>(xS.m_uJobs),
				xS.m_fTreasury);
		}
		else
		{
			EmitEvent(CB_EventType::SessionEnd);
		}
	}

	bool End(const char* szBinPath, const char* szJsonPath,
	         const char* szFramesCsvPath, const char* szEventsCsvPath)
	{
		const bool bOk = Zenith_Telemetry::GetRecorder().End(szBinPath, szJsonPath, &EventTypeToString);
		if (bOk && (szFramesCsvPath != nullptr || szEventsCsvPath != nullptr))
		{
			Zenith_Telemetry::Reader xReader;
			if (xReader.LoadFromFile(szBinPath))
			{
				xReader.ExportCsv(szFramesCsvPath, szEventsCsvPath, &EventTypeToString);
			}
		}
		return bOk;
	}

	// ====================== Hooks ======================
	Hooks::Hooks()
	{
		auto& xDisp = Zenith_EventDispatcher::Get();

		m_xToolSelected = xDisp.Subscribe<CB_OnToolSelected>(
			[](const CB_OnToolSelected& xEvt)
			{
				EmitEvent(CB_EventType::ToolSelected, static_cast<int32_t>(xEvt.m_uTool),
					0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f,
					CB_ToolSystem::ToolName(static_cast<CB_ETool>(xEvt.m_uTool)));
			});

		m_xRoadPlaced = xDisp.Subscribe<CB_OnRoadPlaced>(
			[](const CB_OnRoadPlaced& xEvt)
			{
				EmitEvent(CB_EventType::RoadPlaced, static_cast<int32_t>(xEvt.m_uX), static_cast<int32_t>(xEvt.m_uZ));
			});

		m_xZonePainted = xDisp.Subscribe<CB_OnZonePainted>(
			[](const CB_OnZonePainted& xEvt)
			{
				EmitEvent(CB_EventType::ZonePainted, static_cast<int32_t>(xEvt.m_uX),
					static_cast<int32_t>(xEvt.m_uZ), static_cast<int32_t>(xEvt.m_uZone));
			});

		m_xServicePlaced = xDisp.Subscribe<CB_OnServicePlaced>(
			[](const CB_OnServicePlaced& xEvt)
			{
				EmitEvent(CB_EventType::ServicePlaced, static_cast<int32_t>(xEvt.m_uX),
					static_cast<int32_t>(xEvt.m_uZ), static_cast<int32_t>(xEvt.m_uBuildingType));
			});

		m_xBuildingGrew = xDisp.Subscribe<CB_OnBuildingGrew>(
			[](const CB_OnBuildingGrew& xEvt)
			{
				EmitEvent(CB_EventType::BuildingGrew, static_cast<int32_t>(xEvt.m_uX),
					static_cast<int32_t>(xEvt.m_uZ), static_cast<int32_t>(xEvt.m_uBuildingType),
					static_cast<int32_t>(xEvt.m_uOccupants));
			});

		m_xBulldozed = xDisp.Subscribe<CB_OnBulldozed>(
			[](const CB_OnBulldozed& xEvt)
			{
				EmitEvent(CB_EventType::Bulldozed, static_cast<int32_t>(xEvt.m_uX), static_cast<int32_t>(xEvt.m_uZ));
			});

		m_xPauseToggled = xDisp.Subscribe<CB_OnPauseToggled>(
			[](const CB_OnPauseToggled& xEvt)
			{
				EmitEvent(CB_EventType::PauseToggled, xEvt.m_bPaused ? 1 : 0);
			});

		m_xMilestone = xDisp.Subscribe<CB_OnMilestone>(
			[](const CB_OnMilestone& xEvt)
			{
				EmitEvent(CB_EventType::Milestone, static_cast<int32_t>(xEvt.m_uPopulation), xEvt.m_iThreshold);
			});

		m_xSaved = xDisp.Subscribe<CB_OnSaved>(
			[](const CB_OnSaved& xEvt)
			{
				EmitEvent(CB_EventType::Saved, static_cast<int32_t>(xEvt.m_uRoads),
					static_cast<int32_t>(xEvt.m_uBuildings), 0, 0, xEvt.m_fTreasury);
			});

		m_xLoaded = xDisp.Subscribe<CB_OnLoaded>(
			[](const CB_OnLoaded& xEvt)
			{
				EmitEvent(CB_EventType::Loaded, static_cast<int32_t>(xEvt.m_uRoads),
					static_cast<int32_t>(xEvt.m_uBuildings), 0, 0, xEvt.m_fTreasury);
			});
	}

	Hooks::~Hooks()
	{
		auto& xDisp = Zenith_EventDispatcher::Get();
		if (m_xToolSelected  != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xToolSelected);
		if (m_xRoadPlaced    != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xRoadPlaced);
		if (m_xZonePainted   != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xZonePainted);
		if (m_xServicePlaced != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xServicePlaced);
		if (m_xBuildingGrew  != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xBuildingGrew);
		if (m_xBulldozed     != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xBulldozed);
		if (m_xPauseToggled  != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xPauseToggled);
		if (m_xMilestone     != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xMilestone);
		if (m_xSaved         != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xSaved);
		if (m_xLoaded        != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xLoaded);
	}

	// ====================== Evaluation ======================
	Summary Summarize()
	{
		Summary xOut;
		const Zenith_Vector<Zenith_Telemetry::Event>& xEvents = Zenith_Telemetry::GetRecorder().GetEvents();
		xOut.uTotalEvents = xEvents.GetSize();
		for (u_int u = 0; u < xEvents.GetSize(); ++u)
		{
			const Zenith_Telemetry::Event& xE = xEvents.Get(u);
			switch (static_cast<CB_EventType>(xE.uEventType))
			{
			case CB_EventType::CitySnapshot:
			case CB_EventType::SessionEnd:
			{
				if (static_cast<CB_EventType>(xE.uEventType) == CB_EventType::CitySnapshot) { ++xOut.uSnapshots; }
				const uint32_t uPop = static_cast<uint32_t>(xE.xPayload.aiInts[0]);
				const uint32_t uBld = static_cast<uint32_t>(xE.xPayload.aiInts[1]);
				if (uPop > xOut.uPeakPopulation) { xOut.uPeakPopulation = uPop; }
				if (uBld > xOut.uPeakBuildings)  { xOut.uPeakBuildings  = uBld; }
				xOut.fFinalTreasury = xE.xPayload.afFloats[0];
				break;
			}
			case CB_EventType::ToolSelected:  ++xOut.uToolSelections; break;
			case CB_EventType::RoadPlaced:    ++xOut.uRoadsPlaced;    break;
			case CB_EventType::ZonePainted:   ++xOut.uZonesPainted;   break;
			case CB_EventType::ServicePlaced: ++xOut.uServicesPlaced; break;
			case CB_EventType::BuildingGrew:  ++xOut.uBuildingsGrew;  break;
			case CB_EventType::Bulldozed:     ++xOut.uBulldozed;      break;
			case CB_EventType::PauseToggled:  ++xOut.uPauseToggles;   break;
			case CB_EventType::Milestone:     ++xOut.uMilestones;     break;
			case CB_EventType::Saved:         xOut.bSaved  = true;    break;
			case CB_EventType::Loaded:        xOut.bLoaded = true;    break;
			default: break;
			}
		}
		return xOut;
	}

	void LogSummary(const Summary& x)
	{
		Zenith_Log(LOG_CATEGORY_GAMEPLAY,
			"[CB_Telemetry] %u events | tools=%u roads=%u zones=%u services=%u grew=%u bulldoze=%u "
			"pause=%u milestones=%u snapshots=%u saved=%d loaded=%d | peakPop=%u peakBuildings=%u finalTreasury=%.0f",
			x.uTotalEvents, x.uToolSelections, x.uRoadsPlaced, x.uZonesPainted, x.uServicesPlaced,
			x.uBuildingsGrew, x.uBulldozed, x.uPauseToggles, x.uMilestones, x.uSnapshots,
			x.bSaved ? 1 : 0, x.bLoaded ? 1 : 0, x.uPeakPopulation, x.uPeakBuildings, x.fFinalTreasury);
	}
}
