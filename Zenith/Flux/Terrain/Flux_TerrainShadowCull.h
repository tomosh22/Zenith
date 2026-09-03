#pragma once

#include <cstdint>

// =====================================================================
// Flux_TerrainShadowCull — the PURE half of terrain shadow casting.
//
// Terrain casts into the cascaded shadow maps the same way the unified mesh
// path does: the ONE terrain culling dispatch culls every chunk against the
// camera frustum AND against each active cascade's light-space ortho box, and
// appends the survivors into a PER-CASCADE slot of a second indirect-argument
// buffer (+ a per-cascade visible count). Each "Shadow Cascade N" pass then
// issues one DrawIndexedIndirectCount out of ITS slot. Nothing here is a new
// render-graph pass — the reset and cull dispatches simply cover more records.
//
// This header is the slot arithmetic + the caster LOD policy, and NOTHING
// else: <cstdint>-only, no Flux headers, so it is unit-testable in every
// configuration (same shape as Flux_TerrainPipelineSelect.h). The Slang side
// (Flux_TerrainCulling.slang / Flux_TerrainResetCounters.slang) spells the
// same arithmetic against uFLUX_TERRAIN_TOTAL_CHUNKS; the C++ static_asserts
// in Flux_Terrain.cpp pin the two halves to the same numbers.
//
// SLOT LAYOUT (records, not bytes):
//
//   shadow indirect buffer: [ cascade 0: TOTAL_CHUNKS ][ cascade 1: ... ][ 2 ][ 3 ]
//   shadow count buffer:    [ c0 ][ c1 ][ c2 ][ c3 ]   (one uint each)
//
// A cascade's slot is the SAME shape as the camera's whole buffer — a full
// chunk-capacity range whose live prefix culling compacts into and whose tail
// the reset pass keeps all-zero — so the cascade draw is the camera draw with
// two byte offsets, and the ZERO_PADDED_TO_MAX fallback contract carries over
// unchanged. Slots are view-major so a cascade's records are one contiguous
// range (one indirect draw per cascade, no per-record gather).
// =====================================================================

// One slot per shadow cascade. Pinned to ZENITH_FLUX_NUM_CSMS in Flux_Terrain.cpp
// (the number cannot be read here without a Flux include).
inline constexpr uint32_t uFLUX_TERRAIN_SHADOW_CULL_VIEWS = 4u;

// "Force LOW LOD from cascade N": a cascade index >= this value casts the
// always-resident LOW mesh regardless of camera distance. Equal to the view
// count means NEVER force (every cascade casts the camera-matched LOD).
inline constexpr uint32_t uFLUX_TERRAIN_SHADOW_FORCE_LOW_NEVER = uFLUX_TERRAIN_SHADOW_CULL_VIEWS;

// First record of cascade uCascade's slot, in RECORDS.
constexpr uint32_t Flux_TerrainShadowCullRecordBase(uint32_t uCascade, uint32_t uChunkCapacity)
{
	return uCascade * uChunkCapacity;
}

// Byte offset of cascade uCascade's slot in the shadow indirect buffer — the
// uIndirectOffset the cascade's DrawIndexedIndirectCount passes.
constexpr uint32_t Flux_TerrainShadowCullIndirectByteOffset(uint32_t uCascade, uint32_t uChunkCapacity, uint32_t uRecordStrideBytes)
{
	return Flux_TerrainShadowCullRecordBase(uCascade, uChunkCapacity) * uRecordStrideBytes;
}

// Byte offset of cascade uCascade's visible count — the uCountOffset the
// cascade's DrawIndexedIndirectCount passes. One uint32 per cascade.
constexpr uint32_t Flux_TerrainShadowCullCountByteOffset(uint32_t uCascade)
{
	return uCascade * static_cast<uint32_t>(sizeof(uint32_t));
}

// Allocation sizes. Every slot is a full chunk-capacity range (the zero-tail
// contract needs the whole range addressable, not just the live prefix).
constexpr uint64_t Flux_TerrainShadowCullIndirectBufferBytes(uint32_t uViews, uint32_t uChunkCapacity, uint32_t uRecordStrideBytes)
{
	return static_cast<uint64_t>(uViews) * static_cast<uint64_t>(uChunkCapacity) * static_cast<uint64_t>(uRecordStrideBytes);
}

constexpr uint64_t Flux_TerrainShadowCullCountBufferBytes(uint32_t uViews)
{
	return static_cast<uint64_t>(uViews) * sizeof(uint32_t);
}

// PURE "should terrain cast at all, and into how many cascades" policy.
//
// Returns the number of cascade slots the culling dispatch should FILL this
// frame. Zero means terrain casts nothing: the cull rows early-out on it and the
// reset pass leaves every slot's count at zero, so the per-cascade draws have
// nothing to issue. That is the whole cost of the feature being off — the switch
// removes the cull work as well as the draw, which an early-out in the draw alone
// would not (four cascades of chunk culling would still run every frame).
//
// It is a function rather than an inline `&&` at the call site precisely so the
// toggle's effect is pinnable by a unit test in every configuration; the same
// reason Flux_TerrainShadowCasterLOD below is one.
constexpr uint32_t Flux_TerrainShadowActiveCascades(bool bShadowsEnabled,
	bool bTerrainCastsShadows, uint32_t uActiveViewCascades)
{
	if (!bShadowsEnabled || !bTerrainCastsShadows)
	{
		return 0u;
	}
	return (uActiveViewCascades > uFLUX_TERRAIN_SHADOW_CULL_VIEWS)
		? uFLUX_TERRAIN_SHADOW_CULL_VIEWS : uActiveViewCascades;
}

// PURE caster LOD policy — the CPU twin of the branch in Flux_TerrainCulling.slang.
//
// uCameraLOD is the LOD the CAMERA view resolved for this chunk this frame (the
// hysteresis-stabilised, availability-checked one), uLowLOD the always-resident
// level. A cascade below the force threshold casts the camera-matched LOD; one at
// or above it casts LOW.
//
// Why camera-matched is the default and LOW is the exception: the G-buffer
// RECEIVER is the camera LOD. A caster drawn at a coarser LOD than its receiver
// is a different surface — up to decimetres away on rough ground — and wherever
// the receiver lies below the coarse caster it reads as in its own shadow: acne
// patches that no depth bias fixes because the geometry, not the sampling, is
// wrong. Casting the SAME vertex buffer at the SAME LOD makes terrain
// self-shadowing exact up to depth precision, which is what the slope bias then
// handles. The far cascade is where the mismatch is cheapest to hide (coarse
// world-per-texel, cheap far-cascade filtering) and HIGH is most expensive to
// draw, so that is where LOW pays; forcing it nearer is a tunable, not a default.
constexpr uint32_t Flux_TerrainShadowCasterLOD(uint32_t uCascade, uint32_t uCameraLOD, uint32_t uForceLowFromCascade, uint32_t uLowLOD)
{
	return (uCascade >= uForceLowFromCascade) ? uLowLOD : uCameraLOD;
}
