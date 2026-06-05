#pragma once

#include <cstdint>

// ============================================================================
// CB_Events — CityBuilder gameplay events dispatched through the engine's
// Zenith_EventDispatcher. The game systems Dispatch these where the thing
// actually happens (a road is laid, a building grows, the player pauses); the
// telemetry layer (CB_Telemetry::Hooks) subscribes to them and forwards them
// into the Zenith_Telemetry recorder, so observing a play session never
// requires the gameplay systems to know the recorder exists.
//
// Plain payload structs — the dispatcher is fully generic (no base class, no
// registration). Modelled on DevilsPlayground's DPCommonTypes.h events.
//
// Dispatch is a cheap early-return when nobody is subscribed, so the systems
// dispatch unconditionally and only an active CB_Telemetry::Hooks records.
// ============================================================================

struct CB_OnToolSelected   { uint8_t  m_uTool; };                                        // CB_ETool
struct CB_OnRoadPlaced     { uint32_t m_uX; uint32_t m_uZ; };
struct CB_OnZonePainted    { uint32_t m_uX; uint32_t m_uZ; uint8_t m_uZone; };           // CB_EZoneType
struct CB_OnServicePlaced  { uint32_t m_uX; uint32_t m_uZ; uint8_t m_uBuildingType; };   // CB_EBuildingType
struct CB_OnBuildingGrew   { uint32_t m_uX; uint32_t m_uZ; uint8_t m_uBuildingType; uint32_t m_uOccupants; };
struct CB_OnBulldozed      { uint32_t m_uX; uint32_t m_uZ; };
struct CB_OnPauseToggled   { bool m_bPaused; };
struct CB_OnMilestone      { uint32_t m_uPopulation; int32_t m_iThreshold; };
struct CB_OnSaved          { uint32_t m_uRoads; uint32_t m_uBuildings; float m_fTreasury; };
struct CB_OnLoaded         { uint32_t m_uRoads; uint32_t m_uBuildings; float m_fTreasury; };
