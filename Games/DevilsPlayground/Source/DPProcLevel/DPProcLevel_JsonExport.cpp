#include "Zenith.h"
#include "Source/DPProcLevel/DPProcLevel_JsonExport.h"

#include <cstdio>
#include <fstream>

namespace DPProcLevel
{
	bool ExportLayoutJson(const LevelLayout& xLayout, const char* szPath)
	{
		std::ofstream xOut(szPath);
		if (!xOut.is_open()) return false;

		char buf[256];

		xOut << "{\n";

		// Header
		xOut << "  \"header\": {\n";
		xOut << "    \"version\": 1,\n";
		xOut << "    \"seed\": " << xLayout.uSeed << ",\n";
		std::snprintf(buf, sizeof(buf),
			"    \"bounds\": { \"minX\": %.3f, \"minZ\": %.3f, \"maxX\": %.3f, \"maxZ\": %.3f }\n",
			xLayout.fBoundsMinX, xLayout.fBoundsMinZ,
			xLayout.fBoundsMaxX, xLayout.fBoundsMaxZ);
		xOut << buf;
		xOut << "  },\n";

		// Rooms
		xOut << "  \"rooms\": [";
		const uint32_t uNR = xLayout.axRooms.GetSize();
		for (uint32_t i = 0; i < uNR; ++i)
		{
			const Room& xR = xLayout.axRooms.Get(i);
			std::snprintf(buf, sizeof(buf),
				"{\"id\":%d,\"cx\":%.3f,\"cz\":%.3f,\"hx\":%.3f,\"hz\":%.3f,\"yaw\":%.5f}",
				xR.id, xR.fCentreX, xR.fCentreZ, xR.fHalfExtentX, xR.fHalfExtentZ, xR.fYawRadians);
			xOut << buf;
			if (i + 1 < uNR) xOut << ",";
		}
		xOut << "],\n";

		// Door points
		xOut << "  \"doorPoints\": [";
		const uint32_t uND = xLayout.axDoorPoints.GetSize();
		for (uint32_t i = 0; i < uND; ++i)
		{
			const DoorPoint& xD = xLayout.axDoorPoints.Get(i);
			std::snprintf(buf, sizeof(buf),
				"{\"x\":%.3f,\"z\":%.3f,\"roomId\":%d}",
				xD.fX, xD.fZ, xD.xRoomId);
			xOut << buf;
			if (i + 1 < uND) xOut << ",";
		}
		xOut << "],\n";

		// Corridors
		xOut << "  \"corridors\": [";
		const uint32_t uNC = xLayout.axCorridors.GetSize();
		for (uint32_t i = 0; i < uNC; ++i)
		{
			const Corridor& xC = xLayout.axCorridors.Get(i);
			std::snprintf(buf, sizeof(buf),
				"{\"doorA\":%d,\"doorB\":%d}",
				xC.iDoorA, xC.iDoorB);
			xOut << buf;
			if (i + 1 < uNC) xOut << ",";
		}
		xOut << "],\n";

		// Wall segments (P1 geometry emission). Same OBB schema as
		// rooms (cx, cz, hx, hz, yaw) so the visualiser can reuse its
		// existing rotated-rectangle rendering.
		xOut << "  \"walls\": [";
		const uint32_t uNW = xLayout.axWallSegments.GetSize();
		for (uint32_t i = 0; i < uNW; ++i)
		{
			const WallSegment& xW = xLayout.axWallSegments.Get(i);
			std::snprintf(buf, sizeof(buf),
				"{\"cx\":%.3f,\"cz\":%.3f,\"hx\":%.3f,\"hz\":%.3f,\"yaw\":%.5f}",
				xW.fCentreX, xW.fCentreZ, xW.fHalfExtentX, xW.fHalfExtentZ, xW.fYawRadians);
			xOut << buf;
			if (i + 1 < uNW) xOut << ",";
		}
		xOut << "],\n";

		// Game elements (P2 placement). Type stored as a string for
		// the visualiser to pick distinct icons; corridor id only
		// populated for the Door element.
		auto TypeName = [](GameElementType e) -> const char*
		{
			switch (e)
			{
				case GameElementType::SpawnPoint:   return "SpawnPoint";
				case GameElementType::Pentagram:    return "Pentagram";
				case GameElementType::Forge:        return "Forge";
				case GameElementType::Door:         return "Door";
				case GameElementType::Chest:        return "Chest";
				case GameElementType::NoiseMachine: return "NoiseMachine";
				case GameElementType::Iron:         return "Iron";
				case GameElementType::Objective1:   return "Objective1";
				case GameElementType::Objective2:   return "Objective2";
				case GameElementType::Objective3:   return "Objective3";
				case GameElementType::Objective4:   return "Objective4";
				case GameElementType::Objective5:   return "Objective5";
			}
			return "Unknown";
		};
		xOut << "  \"gameElements\": [";
		const uint32_t uNG = xLayout.axGameElements.GetSize();
		for (uint32_t i = 0; i < uNG; ++i)
		{
			const GameElement& xE = xLayout.axGameElements.Get(i);
			std::snprintf(buf, sizeof(buf),
				"{\"type\":\"%s\",\"x\":%.3f,\"z\":%.3f,\"roomId\":%d,\"corridorId\":%d}",
				TypeName(xE.eType), xE.fX, xE.fZ, xE.xRoomId, xE.iCorridorId);
			xOut << buf;
			if (i + 1 < uNG) xOut << ",";
		}
		xOut << "]\n";

		xOut << "}\n";
		return true;
	}
}
