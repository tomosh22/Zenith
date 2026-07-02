#pragma once

// ============================================================================
// DP_MetaSave (2026-07-01, metagame v1) — cross-run meta-progression state.
//
// Implements GDD §5.4's persistence layer: the Knot balance and the three
// hermit unlock tracks (Wynstan's Forge / Mereworth's Eye / Old Bett's
// Breath), each a 12-node sequential track stored as a bitmask whose set
// bits are always a prefix (node N requires node N-1; enforced at spend
// time, so the "unlock level" is simply popcount of the mask).
//
// Unlike DP_Save (run state; serialisation-only until the save/load UI
// lands), the meta state persists to DISK from day one via Zenith_SaveData
// under its own slot (kSLOT_NAME), with fail-soft default-on-corruption.
// Schema mirrors DP_Save's pattern: own magic ('DPMS'), own version
// constants, HasBytes-guarded fail-soft TryLoad. New nodes are forward-
// compatible by construction — they claim higher bitmask bits, so an old
// save loaded into a build with more nodes just shows them locked.
//
// Blob layout (see Docs/MetaSaveFormat.md):
//   [u32 magic][u32 schema_version][u32 knotBalance]
//   [u32 trackCount]<u32 unlockMask per track>[u32 earnedUnspentLastRun]
// ============================================================================

#include "DataStream/Zenith_DataStream.h"

#include <cstdint>

namespace DP_MetaSave
{
	constexpr uint32_t kCURRENT_SCHEMA_VERSION       = 1;
	constexpr uint32_t kMIN_SUPPORTED_SCHEMA_VERSION = 1;
	constexpr uint32_t kMAGIC = 0x44504D53u; // 'DPMS' little-endian ("SMPD" on disk)

	// Zenith_SaveData slot (production). Distinct from any future DP_Save
	// run-state slot. All read/write/delete paths go through SlotName(),
	// which redirects automated-test runs to a separate throwaway slot so
	// a suite/matrix run can never overwrite or delete a player's real
	// profile (the read side is hermetic via Cached(); this is the
	// matching write-side guard).
	constexpr const char* kSLOT_NAME      = "meta";
	constexpr const char* kSLOT_NAME_TEST = "meta_test";
	const char* SlotName();

	constexpr uint32_t kTRACK_COUNT     = 3;
	constexpr uint32_t kNODES_PER_TRACK = 12; // per AssetManifest §7.2 (~12 nodes/hermit)

	enum class HermitTrack : uint32_t
	{
		Forge  = 0, // Wynstan's Forge  (crafting)      — gameplay effects post-v1 (crafting is instant today)
		Eye    = 1, // Mereworth's Eye  (perception)    — longer fog memory
		Breath = 2, // Old Bett's Breath (movement)     — cheaper sprint, shorter possession cooldown
	};
}

struct DP_MetaState
{
	uint32_t m_uSchemaVersion = DP_MetaSave::kCURRENT_SCHEMA_VERSION;

	// Spendable Knot balance (earned 1 per reagent inscribed + bonuses).
	uint32_t m_uKnotBalance = 0;

	// Per-track unlock bitmasks. Prefix invariant: bit N set implies bits
	// 0..N-1 set (TrySpendUnlock enforces it; loading tolerates any mask).
	uint32_t m_auTrackUnlockMasks[DP_MetaSave::kTRACK_COUNT] = { 0, 0, 0 };

	// What the most recent run banked — the Liminal's run-results readout.
	uint32_t m_uEarnedUnspentKnotsLastRun = 0;
};

namespace DP_MetaSave
{
	// Raw stream contract (mirrors DP_Save::Save/TryLoad). TryLoad fail-
	// softs to a default-constructed state on magic/version/truncation
	// problems and returns false.
	void Save(const DP_MetaState& xState, Zenith_DataStream& xOutStream);
	bool TryLoad(Zenith_DataStream& xInStream, DP_MetaState& xOutState);

	// Disk persistence via Zenith_SaveData under kSLOT_NAME. SaveToDisk
	// also refreshes the process-wide cache below.
	bool SaveToDisk(const DP_MetaState& xState);
	// Missing / corrupt / version-mismatched slot yields a default state.
	DP_MetaState LoadOrDefault();

	// Process-wide cached copy: loaded from disk on first access, refreshed
	// by SaveToDisk. Gameplay effect reads route through this so per-frame
	// consumers never touch the file system.
	const DP_MetaState& Cached();

	// ---- Node helpers (prefix invariant) ----
	uint32_t GetUnlockedCount(const DP_MetaState& xState, HermitTrack eTrack);
	bool IsNodeUnlocked(const DP_MetaState& xState, HermitTrack eTrack, uint32_t uNode);
	// True iff uNode is the next locked node in the track AND the balance
	// covers GetNodeCost(uNode).
	bool CanUnlockNode(const DP_MetaState& xState, HermitTrack eTrack, uint32_t uNode);
	// Knot cost for a node (linear curve from Tuning.json's metagame keys).
	uint32_t GetNodeCost(uint32_t uNode);
	// Deducts + sets the bit iff CanUnlockNode. Returns success. Does NOT
	// persist — callers (the Liminal hub) SaveToDisk after a batch.
	bool TrySpendUnlock(DP_MetaState& xState, HermitTrack eTrack, uint32_t uNode);

	// ---- Gameplay effect scales (read Cached() + Tuning.json) ----
	// All return 1.0 with zero unlocks and lerp linearly to their
	// Tuning.json "*_at_full" target at a fully-unlocked track, so bots /
	// tests / the balance matrix are untouched until nodes are bought.
	float GetSprintDrainScale();        // Breath: < 1 = cheaper sprint
	float GetPossessionCooldownScale(); // Breath: < 1 = shorter cooldown (GDD 1.5 s -> 1.0 s)
	float GetFogMemoryScale();          // Eye:    > 1 = memories last longer

#ifdef ZENITH_INPUT_SIMULATOR
	// Test hooks: stage a cache state without touching disk / drop the
	// cache so the next access reloads.
	void SetCachedForTest(const DP_MetaState& xState);
	void InvalidateCacheForTest();
#endif
}
