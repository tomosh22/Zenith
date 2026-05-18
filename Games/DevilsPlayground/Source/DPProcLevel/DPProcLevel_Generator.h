#pragma once
/**
 * DPProcLevel::Generator - seed-deterministic BSP layout generator.
 *
 * P0 of the procgen feature. Given a seed + config, produces a LevelLayout
 * with rooms + corridors + door points. Decoupled from the engine: pure
 * data in, pure data out, no scene mutation. Phase 1 will translate
 * LevelLayout into wall + door entity placements; Phase 2 will place
 * game elements; Phase 3 villagers + priest.
 *
 * Algorithm (v1):
 *   1. Recursively partition the world bounds via 2D BSP. Splits
 *      alternate between X and Z axes; split position is randomised
 *      within the middle 50%% of the current partition to give variety.
 *      Stops when either axis is below 2 * GenConfig::fMinRoomSize.
 *   2. Each leaf partition becomes a Room. The room shrinks to fit
 *      inside the partition with margin, then receives a random yaw
 *      rotation. If the rotated OBB overlaps a previously-placed room,
 *      the yaw is retried up to GenConfig::uMaxYawRetries times before
 *      falling back to 0.
 *   3. Adjacent leaves in the BSP tree (siblings + ancestors) get a
 *      corridor: pick the midpoint of their shared edge, project it
 *      onto each room's nearest edge to get DoorPoints, emit a
 *      Corridor referencing both DoorPoint indices.
 *
 * Determinism:
 *   * Single uint64_t seed drives an std::mt19937_64 PRNG.
 *   * Every random draw goes through that PRNG; no external clock or
 *     std::random_device. Same seed -> identical LevelLayout across
 *     runs, platforms, and processes.
 *
 * Failure:
 *   * If the post-rotation overlap check fails for ALL retries, the
 *     room falls back to yaw=0 (which is guaranteed not to overlap
 *     since BSP partitions are non-overlapping by construction). The
 *     test surface should pin a "no fall-back required" invariant
 *     when uMaxYawRetries is generous.
 */

#include "DPProcLevel_LevelLayout.h"

namespace DPProcLevel
{
	// Deterministic generation. Caller owns the LevelLayout; the
	// function clears it before populating, so passing a recycled
	// instance is fine. Returns true on success; only fails today
	// for pathological GenConfigs (e.g. bounds smaller than
	// fMinRoomSize). Failures are logged via Zenith_Warning.
	bool Generate(uint64_t uSeed, const GenConfig& xConfig, LevelLayout& xOut);
}
