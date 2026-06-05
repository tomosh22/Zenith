#pragma once

#include "CityBuilder/Source/CB_RoadGraph.h"
#include "CityBuilder/Source/CB_Zoning.h"
#include "CityBuilder/Source/CB_BuildingPlacement.h"
#include "CityBuilder/Source/CB_Districts.h"
#include "CityBuilder/Source/CB_TransitLines.h"
#include "CityBuilder/Source/CB_Conduits.h"
#include "DataStream/Zenith_DataStream.h"
#include <cstdint>

// ============================================================================
// CB_SaveLoadFreeform — serialize/deserialize the free-form city: the road graph
// (nodes + spline segments), the zoning lots, and the buildings + services +
// treasury. Lots reference segments and buildings reference lots by stable slot
// index, so the whole cross-referenced state round-trips. Operates on the three
// systems directly, so it is headless-testable (no manager / ECS needed).
// ============================================================================
namespace CB_SaveLoadFreeform
{
	static constexpr uint32_t SAVE_VERSION = 4;   // v2 districts/policies; v3 transit lines; v4 conduits

	inline void Save(const CB_RoadGraph& xGraph, const CB_Zoning& xZoning,
	                 const CB_BuildingPlacement& xBuild, const CB_Districts& xDistricts,
	                 const CB_TransitLines& xTransit, const CB_Conduits& xConduits, Zenith_DataStream& xStream)
	{
		uint32_t uVersion = SAVE_VERSION;
		xStream << uVersion;
		xGraph.WriteToDataStream(xStream);
		xZoning.WriteToDataStream(xStream);
		xBuild.WriteToDataStream(xStream);
		xDistricts.WriteToDataStream(xStream);
		xTransit.WriteToDataStream(xStream);
		xConduits.WriteToDataStream(xStream);
	}

	inline bool Load(CB_RoadGraph& xGraph, CB_Zoning& xZoning,
	                 CB_BuildingPlacement& xBuild, CB_Districts& xDistricts,
	                 CB_TransitLines& xTransit, CB_Conduits& xConduits, Zenith_DataStream& xStream)
	{
		uint32_t uVersion = 0;
		xStream >> uVersion;
		if (uVersion != SAVE_VERSION)
		{
			return false;
		}
		xGraph.ReadFromDataStream(xStream);
		xZoning.ReadFromDataStream(xStream);
		xBuild.ReadFromDataStream(xStream);
		xDistricts.ReadFromDataStream(xStream);
		xTransit.ReadFromDataStream(xStream);
		xConduits.ReadFromDataStream(xStream);
		return true;
	}

	inline bool SaveToFile(const CB_RoadGraph& xGraph, const CB_Zoning& xZoning,
	                       const CB_BuildingPlacement& xBuild, const CB_Districts& xDistricts,
	                       const CB_TransitLines& xTransit, const CB_Conduits& xConduits, const char* szPath)
	{
		Zenith_DataStream xStream;
		Save(xGraph, xZoning, xBuild, xDistricts, xTransit, xConduits, xStream);
		xStream.WriteToFile(szPath);
		return true;
	}

	inline bool LoadFromFile(CB_RoadGraph& xGraph, CB_Zoning& xZoning,
	                         CB_BuildingPlacement& xBuild, CB_Districts& xDistricts,
	                         CB_TransitLines& xTransit, CB_Conduits& xConduits, const char* szPath)
	{
		Zenith_DataStream xStream;
		xStream.ReadFromFile(szPath);
		if (!xStream.IsValid())
		{
			return false;
		}
		xStream.SetCursor(0);
		return Load(xGraph, xZoning, xBuild, xDistricts, xTransit, xConduits, xStream);
	}
}
