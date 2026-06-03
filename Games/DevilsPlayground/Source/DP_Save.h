#pragma once

// ============================================================================
// DP_Save (MVP-1.10) -- DevilsPlayground run-state save/load.
//
// Scope of this header:
//   * `DP_RunState` -- POD struct describing a single run's full state.
//   * `DP_Save::Save(state, stream)` / `DP_Save::TryLoad(stream, &state)`
//     -- raw serialisation contract. Operate on `Zenith_DataStream`
//     buffers; do NOT touch the file system. Real on-disk persistence
//     is post-MVP (when the UI flow for Save / Load Game lands).
//
// Out of scope:
//   * Live `DP_Player`/`DP_Win` capture into `DP_RunState`. Tests
//     construct `DP_RunState` manually with known values, round-trip
//     it, and verify equality. The "apply a loaded state back to the
//     live game" path is gated on an explicit UI/menu flow and a
//     handful of new setters in `DP_Player` (e.g.
//     SetDemonScentForTest -> a non-test variant). When that lands
//     it lives in this same header.
//
// Schema versioning:
//   * Every blob begins with `uint32_t uSchemaVersion`. Version 1 is
//     the initial format. Future versions can add fields; the loader
//     compares the version against `kCURRENT_SCHEMA_VERSION` and
//     fails-soft (TryLoad returns false) if it doesn't match.
//
//   * Migration policy (per MVP-1.10.4 SaveFormat.md): each future
//     version SHOULD implement a migration function from N-1 to N;
//     failed migration falls back to default. Versions lower than
//     `kMIN_SUPPORTED_SCHEMA_VERSION` are rejected outright with a
//     one-line log. For MVP both `kCURRENT` and `kMIN_SUPPORTED` are
//     1; the policy lives here ready to flex when V2 lands.
// ============================================================================

#include "Collections/Zenith_Vector.h"
#include "DataStream/Zenith_DataStream.h"
#include "ZenithECS/Zenith_Entity.h"
#include "PublicInterfaces.h"

#include <cstdint>

namespace DP_Save
{
	// Current on-disk format. Bump when DP_RunState gains or loses a
	// field, or when the in-stream byte layout changes for any reason.
	constexpr uint32_t kCURRENT_SCHEMA_VERSION   = 1;

	// Oldest version this build can load. Set equal to kCURRENT until
	// a migration path is implemented for older blobs.
	constexpr uint32_t kMIN_SUPPORTED_SCHEMA_VERSION = 1;

	// Magic number prefix. Catches "this isn't a DP save at all" cases
	// (truncated to a few bytes, random binary, accidentally pointed at
	// a non-save asset) before the version comparison runs and avoids
	// misleading "unsupported version" diagnostics.
	constexpr uint32_t kMAGIC = 0x44505352u; // 'DPSR' little-endian
}

struct DP_HeldItemEntry
{
	Zenith_EntityID m_xVillager;
	Zenith_EntityID m_xItem;
	DP_ItemTag      m_eTag = DP_ItemTag::None;

	// Trivially copyable; Zenith_DataStream serialises via memcpy.
};

struct DP_ScentEntry
{
	Zenith_EntityID m_xVillager;
	float           m_fScent = 0.0f;

	// Trivially copyable.
};

struct DP_RunState
{
	// First 4 bytes of the blob: schema version + magic. The loader
	// validates both before reading any other field.
	uint32_t            m_uSchemaVersion = DP_Save::kCURRENT_SCHEMA_VERSION;

	// Possession state.
	Zenith_EntityID     m_xPossessedVillager;    // INVALID if not possessed
	float               m_fPossessedLife = 0.0f; // Last known life of the possessed villager

	// Held items per villager. Variable-length.
	Zenith_Vector<DP_HeldItemEntry> m_axHeldItems;

	// Per-villager scent table. Variable-length.
	Zenith_Vector<DP_ScentEntry>    m_axScent;

	// Objectives bitmask (matches DP_Win::GetCollectedObjectivesMask).
	uint32_t            m_uObjectivesMask = 0;

	// Dawn-timer remaining seconds. Phase 4 placeholder; serialised
	// now so V1 blobs persist forward when the Night timer lands.
	float               m_fDawnTimerRemaining = 30.0f;
};

namespace DP_Save
{
	// Serialise `xState` to `xOutStream`. The stream's cursor advances
	// to the end of the written data; callers may call
	// `xOutStream.GetCursor()` to know the byte size.
	//
	// The blob layout is:
	//   [u32 magic][u32 schema_version][u32 possessed.index][u32 possessed.generation]
	//   [f32 possessedLife][u32 heldCount]<held entries>
	//   [u32 scentCount]<scent entries>[u32 objectives][f32 dawnTimer]
	void Save(const DP_RunState& xState, Zenith_DataStream& xOutStream);

	// Deserialise from `xInStream` into `xOutState`. Returns false on
	// any of:
	//   * Magic mismatch
	//   * Version outside [kMIN_SUPPORTED_SCHEMA_VERSION, kCURRENT_SCHEMA_VERSION]
	//   * Truncated stream (insufficient bytes for a declared field
	//     or a declared collection size)
	//   * Sanity-bound exceedance (held / scent count > kMAX_ENTRIES)
	// On failure, `xOutState` is left default-constructed (all zero/
	// empty). Callers should treat false as "use a fresh run state".
	bool TryLoad(Zenith_DataStream& xInStream, DP_RunState& xOutState);

	// Sanity ceiling on collection sizes inside a blob. 1024 covers
	// well-formed runs (17 villagers max in MVP); a malformed blob
	// claiming millions of entries is rejected before any allocation.
	constexpr uint32_t kMAX_ENTRIES = 1024;
}
