#pragma once
/**
 * DPProcLevel::LevelLayout - data carrier for procedurally-generated levels.
 *
 * Pure-data output of DPProcLevel::Generator::Generate(). Decoupled from the
 * scene-instantiation step (Phase 1 will translate this struct into wall +
 * door entity placements) so the algorithm can be iterated on standalone:
 * unit tests + a PowerShell visualiser both consume LevelLayout via the
 * JSON export with no engine boot.
 *
 * Convention reminder (mirrors DPTelemetry::SceneObstacle):
 *   * XZ plane only (Y is discarded for top-down)
 *   * Room is an oriented bounding box: centre + half-extents (in local
 *     room frame) + yawRadians (rotation around +Y, right-hand rule)
 *   * Corridors connect ROOMS (by index) via a single straight line
 *     between two DOOR POINTS (one anchored on each room's edge)
 *
 * Format-version 1. Bump if the struct layout changes incompatibly --
 * the JSON exporter mirrors the same version field so old PNGs can be
 * regenerated from old JSONs without lying about which generator built them.
 */

#include "Collections/Zenith_Vector.h"
#include <cstdint>

namespace DPProcLevel
{
	// Inclusive integer ID for a room. Used as the index into
	// LevelLayout::axRooms; corridor + door references chain by ID.
	using RoomId = int32_t;
	constexpr RoomId kInvalidRoomId = -1;

	struct Room
	{
		RoomId id          = kInvalidRoomId;
		// World-space OBB. centre + half-extents + yaw. Half-extents are
		// in the room's LOCAL frame so a yaw rotation just orients the
		// rectangle without changing its size.
		float   fCentreX    = 0.0f;
		float   fCentreZ    = 0.0f;
		float   fHalfExtentX = 0.0f;
		float   fHalfExtentZ = 0.0f;
		float   fYawRadians = 0.0f;
	};

	struct DoorPoint
	{
		// World-space XZ position of the door (on the shared edge of two
		// rooms or on the corridor entrance to a room).
		float fX = 0.0f;
		float fZ = 0.0f;
		// Which room this door anchors against. Each corridor has two
		// door points, one per endpoint room.
		RoomId xRoomId = kInvalidRoomId;
	};

	struct Corridor
	{
		// Indices into LevelLayout::axDoorPoints. A corridor connects
		// exactly two door points; the corresponding rooms come from
		// the door points' xRoomId fields.
		int32_t iDoorA = -1;
		int32_t iDoorB = -1;
	};

	struct LevelLayout
	{
		// Bounds the entire level occupies in world XZ. Set by the
		// generator from its config; the visualiser uses it to size
		// the rendering window so all rooms fit.
		float fBoundsMinX = 0.0f;
		float fBoundsMinZ = 0.0f;
		float fBoundsMaxX = 0.0f;
		float fBoundsMaxZ = 0.0f;

		uint64_t uSeed = 0;     // generator seed -- echoed back so the
		                        //   visualiser caption can show it

		Zenith_Vector<Room>      axRooms;
		Zenith_Vector<DoorPoint> axDoorPoints;
		Zenith_Vector<Corridor>  axCorridors;
	};

	// Generator configuration. Defaults match the v1 design (100x100 m
	// centred at (50,50), 4..15 m rooms, ~8 leaves, full-yaw rooms).
	struct GenConfig
	{
		float    fBoundsMinX     = 0.0f;
		float    fBoundsMinZ     = 0.0f;
		float    fBoundsMaxX     = 100.0f;
		float    fBoundsMaxZ     = 100.0f;
		// Minimum room dimension after subdivision -- the BSP stops
		// splitting a partition once any further split would produce a
		// child smaller than this.
		float    fMinRoomSize    = 4.0f;
		// Hard cap on a single room's half-extent. The BSP partition
		// may produce larger cells but the room placed inside is
		// shrunk to fit so the visual variety stays consistent.
		float    fMaxRoomSize    = 15.0f;
		// BSP depth. Each leaf becomes one room, so the max room count
		// is 2^depth.
		uint32_t uBspDepth       = 3;
		// Number of attempts the per-room yaw rotation gets to find an
		// orientation that doesn't overlap previously-placed rooms.
		// After this many failures the room falls back to yaw=0.
		uint32_t uMaxYawRetries  = 16;
	};
}
