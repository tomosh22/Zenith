#pragma once
/**
 * DPProcLevel::JsonExport - writes a LevelLayout to a JSON file the
 * PowerShell visualiser can consume. P0 inspection tool only -- not
 * for runtime use. Format mirrors the telemetry JSON layout (header +
 * arrays) so future readers can follow the same parsing pattern.
 *
 * Schema (v1):
 *   {
 *     "header": {
 *       "version": 1,
 *       "seed": <uint64>,
 *       "bounds": { "minX": .., "minZ": .., "maxX": .., "maxZ": .. }
 *     },
 *     "rooms": [
 *       { "id": N, "cx": .., "cz": .., "hx": .., "hz": .., "yaw": .. },
 *       ...
 *     ],
 *     "doorPoints": [
 *       { "x": .., "z": .., "roomId": N },
 *       ...
 *     ],
 *     "corridors": [
 *       { "doorA": N, "doorB": N },
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
