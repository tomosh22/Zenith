#pragma once

#include "CityBuilder/Components/CB_CityManager_Behaviour.h"
#include "CityBuilder/Source/CB_Events.h"
#include "DataStream/Zenith_DataStream.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include <cstdint>

// ============================================================================
// CB_SaveLoad — serialize/deserialize a city to a Zenith_DataStream (and file).
// Saves the authoritative state: terrain heightfield, grid (zones/roads/building
// links/derived data), building records, and the treasury. Citizens are
// re-derived from buildings on the next sim tick, so they aren't persisted.
// ============================================================================
namespace CB_SaveLoad
{
	static constexpr uint32_t SAVE_VERSION = 1;

	inline void Save(CB_CityManager_Behaviour& xMgr, Zenith_DataStream& xStream)
	{
		xStream << SAVE_VERSION;
		xMgr.GetHeightfield().WriteToDataStream(xStream);
		xMgr.GetGrid().WriteToDataStream(xStream);
		xMgr.GetBuildings().WriteToDataStream(xStream);
		const float fTreasury = xMgr.GetStats().m_fTreasury;
		xStream << fTreasury;
		Zenith_EventDispatcher::Get().Dispatch(CB_OnSaved{
			xMgr.GetRoads().GetRoadCellCount(), xMgr.GetBuildings().GetActiveCount(), fTreasury });
	}

	inline bool Load(CB_CityManager_Behaviour& xMgr, Zenith_DataStream& xStream)
	{
		uint32_t uVersion = 0;
		xStream >> uVersion;
		if (uVersion != SAVE_VERSION)
		{
			return false;
		}
		xMgr.GetHeightfield().ReadFromDataStream(xStream);
		xMgr.GetGrid().ReadFromDataStream(xStream);
		xMgr.GetBuildings().ReadFromDataStream(xStream);
		float fTreasury = 0.0f;
		xStream >> fTreasury;
		xMgr.GetSim().SetStartingTreasury(fTreasury);
		Zenith_EventDispatcher::Get().Dispatch(CB_OnLoaded{
			xMgr.GetRoads().GetRoadCellCount(), xMgr.GetBuildings().GetActiveCount(), fTreasury });
		return true;
	}

	inline bool SaveToFile(CB_CityManager_Behaviour& xMgr, const char* szPath)
	{
		Zenith_DataStream xStream;
		Save(xMgr, xStream);
		xStream.WriteToFile(szPath);
		return true;
	}

	inline bool LoadFromFile(CB_CityManager_Behaviour& xMgr, const char* szPath)
	{
		Zenith_DataStream xStream;
		xStream.ReadFromFile(szPath);
		if (!xStream.IsValid())
		{
			return false;
		}
		xStream.SetCursor(0);
		return Load(xMgr, xStream);
	}
}
