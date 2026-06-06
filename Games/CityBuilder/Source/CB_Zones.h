#pragma once

#include <cstdint>

// ============================================================================
// CB_EZoneType — the land-use zone types painted onto road frontage (Residential
// / Commercial / Industrial / Park). Used by zoning, building placement, the
// building-def table, the tool system, events, and the HUD. A tiny shared enum so
// no system has to pull a heavier header just for it.
// ============================================================================

enum CB_EZoneType : uint8_t
{
	CB_ZONE_NONE        = 0,
	CB_ZONE_RESIDENTIAL = 1,
	CB_ZONE_COMMERCIAL  = 2,
	CB_ZONE_INDUSTRIAL  = 3,
	CB_ZONE_PARK        = 4,
	CB_ZONE_COUNT
};
