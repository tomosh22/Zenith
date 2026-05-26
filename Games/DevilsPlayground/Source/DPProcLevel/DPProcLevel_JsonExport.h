#pragma once
/**
 * DPProcLevel::JsonExport - writes a LevelLayout to a JSON file the
 * PowerShell visualiser can consume. P0 inspection tool only -- not
 * for runtime use. Format mirrors the telemetry JSON layout (header +
 * arrays) so future readers can follow the same parsing pattern.
 *
 * Schema (v2; bumped 2026-05-25 -- doorPoints gained `wallYaw`,
 * gameElements gained `yaw` (was previously only in the struct) and
 * `locked` (new)):
 *   {
 *     "header": {
 *       "version": 2,
 *       "seed": <uint64>,
 *       "bounds": { "minX": .., "minZ": .., "maxX": .., "maxZ": .. }
 *     },
 *     "rooms": [
 *       { "id": N, "cx": .., "cz": .., "hx": .., "hz": .., "yaw": .. },
 *       ...
 *     ],
 *     "doorPoints": [
 *       { "x": .., "z": .., "roomId": N, "wallYaw": .. },
 *       ...
 *     ],
 *     "corridors": [
 *       { "doorA": N, "doorB": N },
 *       ...
 *     ],
 *     "gameElements": [
 *       { "type": "Door", "x": .., "z": .., "roomId": N,
 *         "corridorId": N, "yaw": .., "locked": true|false },
 *       ...
 *     ]
 *   }
 */

#include "DPProcLevel_LevelLayout.h"

namespace DPProcLevel
{
	// Write the layout as JSON. Returns false on I/O error. The path is
	// taken verbatim (no temp-dir munging) so the caller controls
	// disposition -- unit tests typically write under
	// %TEMP%/dp_proclevel_seed_<N>.json.
	bool ExportLayoutJson(const LevelLayout& xLayout, const char* szPath);
}
