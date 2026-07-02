#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Core/Multithreading/Zenith_Multithreading.h"
#include "Core/Zenith_CommandLine.h"

#include "DP_MetaSave.h"
#include "DP_Tuning.h"

#include "SaveData/Zenith_SaveData.h"

#include <algorithm>
#include <bit>

namespace DP_MetaSave
{
	void Save(const DP_MetaState& xState, Zenith_DataStream& xOutStream)
	{
		xOutStream << kMAGIC;
		xOutStream << xState.m_uSchemaVersion;
		xOutStream << xState.m_uKnotBalance;
		// Track count written explicitly so a future build with more tracks
		// can still read old blobs (missing tracks default to 0 mask).
		xOutStream << kTRACK_COUNT;
		for (uint32_t u = 0; u < kTRACK_COUNT; ++u)
		{
			xOutStream << xState.m_auTrackUnlockMasks[u];
		}
		xOutStream << xState.m_uEarnedUnspentKnotsLastRun;
	}

	namespace
	{
		bool HasBytes(const Zenith_DataStream& xStream, uint64_t ulBytesNeeded)
		{
			return xStream.GetCursor() + ulBytesNeeded <= xStream.GetCapacity();
		}

		// Sanity ceiling on the track count declared in a blob — a well-
		// formed file says 3; a corrupt one claiming millions is rejected
		// before any read loop spins.
		constexpr uint32_t kMAX_TRACKS_IN_BLOB = 16;
	}

	bool TryLoad(Zenith_DataStream& xInStream, DP_MetaState& xOutState)
	{
		xOutState = DP_MetaState{};

		if (!HasBytes(xInStream, sizeof(uint32_t) * 2))
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"DP_MetaSave::TryLoad: stream too small for magic + version");
			return false;
		}
		uint32_t uMagic = 0;
		xInStream >> uMagic;
		if (uMagic != kMAGIC)
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"DP_MetaSave::TryLoad: magic mismatch (got 0x%08X, expected 0x%08X)",
				uMagic, kMAGIC);
			return false;
		}
		uint32_t uVersion = 0;
		xInStream >> uVersion;
		if (uVersion < kMIN_SUPPORTED_SCHEMA_VERSION || uVersion > kCURRENT_SCHEMA_VERSION)
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"DP_MetaSave::TryLoad: unsupported schema version %u (supported [%u, %u]) -- falling back to default",
				uVersion, kMIN_SUPPORTED_SCHEMA_VERSION, kCURRENT_SCHEMA_VERSION);
			return false;
		}
		xOutState.m_uSchemaVersion = uVersion;

		if (!HasBytes(xInStream, sizeof(uint32_t) * 2))
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"DP_MetaSave::TryLoad: stream truncated before balance/track count");
			xOutState = DP_MetaState{};
			return false;
		}
		xInStream >> xOutState.m_uKnotBalance;
		uint32_t uTracksInBlob = 0;
		xInStream >> uTracksInBlob;
		if (uTracksInBlob > kMAX_TRACKS_IN_BLOB)
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"DP_MetaSave::TryLoad: track count %u exceeds sanity cap %u",
				uTracksInBlob, kMAX_TRACKS_IN_BLOB);
			xOutState = DP_MetaState{};
			return false;
		}
		if (!HasBytes(xInStream, static_cast<uint64_t>(uTracksInBlob) * sizeof(uint32_t) + sizeof(uint32_t)))
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"DP_MetaSave::TryLoad: stream truncated reading %u track masks", uTracksInBlob);
			xOutState = DP_MetaState{};
			return false;
		}
		for (uint32_t u = 0; u < uTracksInBlob; ++u)
		{
			uint32_t uMask = 0;
			xInStream >> uMask;
			// Extra tracks from a future build are dropped; missing tracks
			// stay at their default 0 mask.
			if (u < kTRACK_COUNT)
			{
				xOutState.m_auTrackUnlockMasks[u] = uMask;
			}
		}
		xInStream >> xOutState.m_uEarnedUnspentKnotsLastRun;
		return true;
	}

	// ========================================================================
	// Disk persistence + process-wide cache.
	// ========================================================================
	namespace
	{
		DP_MetaState g_xCached;
		bool         g_bCacheLoaded = false;

		void WriteMetaPayload(Zenith_DataStream& xStream, void* pxUserData)
		{
			const DP_MetaState* pxState = static_cast<const DP_MetaState*>(pxUserData);
			Save(*pxState, xStream);
		}

		void ReadMetaPayload(Zenith_DataStream& xStream, uint32_t /*uGameVersion*/, void* pxUserData)
		{
			DP_MetaState* pxState = static_cast<DP_MetaState*>(pxUserData);
			// TryLoad fail-softs to default; the /*uGameVersion*/ in the
			// Zenith_SaveData header is redundant with the blob's own
			// schema version, which is the one the migration policy keys on.
			TryLoad(xStream, *pxState);
		}
	}

	const char* SlotName()
	{
		// Write-side hermeticity: automated-test / matrix processes bank
		// and spend against a throwaway slot. Without this, every batched
		// bot playthrough that reaches a run end would overwrite (and the
		// meta tests' DeleteSlot hygiene would delete) the PLAYER's real
		// %APPDATA% profile.
		return Zenith_CommandLine::IsAutomatedTestRun() ? kSLOT_NAME_TEST : kSLOT_NAME;
	}

	bool SaveToDisk(const DP_MetaState& xState)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_MetaSave::SaveToDisk must be called from main thread");
		const bool bOk = Zenith_SaveData::Save(SlotName(), kCURRENT_SCHEMA_VERSION,
			&WriteMetaPayload, const_cast<DP_MetaState*>(&xState));
		// Keep the cache coherent with what we just persisted (even on a
		// failed disk write the in-memory truth is the newest state).
		g_xCached = xState;
		g_bCacheLoaded = true;
		return bOk;
	}

	DP_MetaState LoadOrDefault()
	{
		DP_MetaState xState;
		// Missing file / bad magic / version mismatch / corrupt payload all
		// leave xState default via ReadMetaPayload's fail-soft TryLoad (or
		// never invoke it at all) — exactly the "fresh profile" contract.
		Zenith_SaveData::Load(SlotName(), &ReadMetaPayload, &xState);
		return xState;
	}

	const DP_MetaState& Cached()
	{
		if (!g_bCacheLoaded)
		{
			// Hermetic tests: automated-test runs start from a DEFAULT
			// profile instead of whatever meta save happens to sit in
			// %APPDATA% on this machine — otherwise a real profile's Breath
			// unlocks would silently rescale sprint drain / possession
			// cooldown under pinned physics tests. Tests that exercise the
			// disk path do so explicitly (LoadOrDefault + the
			// Zenith_SaveData readback/recording hooks) or stage state via
			// SetCachedForTest.
			g_xCached = Zenith_CommandLine::IsAutomatedTestRun()
				? DP_MetaState{}
				: LoadOrDefault();
			g_bCacheLoaded = true;
		}
		return g_xCached;
	}

#ifdef ZENITH_INPUT_SIMULATOR
	void SetCachedForTest(const DP_MetaState& xState)
	{
		g_xCached = xState;
		g_bCacheLoaded = true;
	}

	void InvalidateCacheForTest()
	{
		g_xCached = DP_MetaState{};
		g_bCacheLoaded = false;
	}
#endif

	// ========================================================================
	// Node helpers.
	// ========================================================================
	uint32_t GetUnlockedCount(const DP_MetaState& xState, HermitTrack eTrack)
	{
		const uint32_t uTrack = static_cast<uint32_t>(eTrack);
		if (uTrack >= kTRACK_COUNT) return 0;
		return static_cast<uint32_t>(std::popcount(xState.m_auTrackUnlockMasks[uTrack]));
	}

	bool IsNodeUnlocked(const DP_MetaState& xState, HermitTrack eTrack, uint32_t uNode)
	{
		const uint32_t uTrack = static_cast<uint32_t>(eTrack);
		if (uTrack >= kTRACK_COUNT || uNode >= kNODES_PER_TRACK) return false;
		return (xState.m_auTrackUnlockMasks[uTrack] & (1u << uNode)) != 0;
	}

	uint32_t GetNodeCost(uint32_t uNode)
	{
		const int iBase = DP_Tuning::Get<int>("metagame.node_cost_base");
		const int iPer  = DP_Tuning::Get<int>("metagame.node_cost_per_node");
		return static_cast<uint32_t>(iBase + iPer * static_cast<int>(uNode));
	}

	bool CanUnlockNode(const DP_MetaState& xState, HermitTrack eTrack, uint32_t uNode)
	{
		const uint32_t uTrack = static_cast<uint32_t>(eTrack);
		if (uTrack >= kTRACK_COUNT || uNode >= kNODES_PER_TRACK) return false;
		if (IsNodeUnlocked(xState, eTrack, uNode)) return false;
		// Prefix invariant: only the NEXT node in the track is purchasable.
		if (uNode != GetUnlockedCount(xState, eTrack)) return false;
		return xState.m_uKnotBalance >= GetNodeCost(uNode);
	}

	bool TrySpendUnlock(DP_MetaState& xState, HermitTrack eTrack, uint32_t uNode)
	{
		if (!CanUnlockNode(xState, eTrack, uNode)) return false;
		xState.m_uKnotBalance -= GetNodeCost(uNode);
		xState.m_auTrackUnlockMasks[static_cast<uint32_t>(eTrack)] |= (1u << uNode);
		return true;
	}

	// ========================================================================
	// Gameplay effect scales. Linear from 1.0 (no unlocks) to the tuning
	// "*_at_full" target at 12/12 nodes. All 1.0 for a fresh profile, so
	// bots / tests / the ratified balance matrix are unaffected until the
	// player actually spends Knots.
	// ========================================================================
	namespace
	{
		float TrackProgress01(HermitTrack eTrack)
		{
			return static_cast<float>(GetUnlockedCount(Cached(), eTrack))
				/ static_cast<float>(kNODES_PER_TRACK);
		}

		float LerpToFull(float fAtFull, HermitTrack eTrack)
		{
			const float fT = TrackProgress01(eTrack);
			return 1.0f + (fAtFull - 1.0f) * fT;
		}
	}

	float GetSprintDrainScale()
	{
		return LerpToFull(DP_Tuning::Get<float>("metagame.breath_sprint_drain_scale_at_full"),
			HermitTrack::Breath);
	}

	float GetPossessionCooldownScale()
	{
		return LerpToFull(DP_Tuning::Get<float>("metagame.breath_cooldown_scale_at_full"),
			HermitTrack::Breath);
	}

	float GetFogMemoryScale()
	{
		return LerpToFull(DP_Tuning::Get<float>("metagame.eye_memory_scale_at_full"),
			HermitTrack::Eye);
	}
}
