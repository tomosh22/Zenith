#pragma once

// ============================================================================
// CB_Config — CityBuilder compile-time switches.
//
// CB_USE_LEGACY_GRID: master switch for the original grid-based city (roads /
// zones / buildings on CB_CityGrid + CB_RoadNetwork). The Cities: Skylines
// rebuild (free-form spline roads + road-relative zoning + facing-the-road
// building growth) supersedes it. The legacy systems stay compiled but idle
// behind this flag through the multi-phase transition (G1..G4) so the exe always
// builds and runs. 0 = the new free-form systems are authoritative.
// ============================================================================

#define CB_USE_LEGACY_GRID 0
