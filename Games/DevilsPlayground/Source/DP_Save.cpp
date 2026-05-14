#include "Zenith.h"

#include "DP_Save.h"

namespace DP_Save
{
	void Save(const DP_RunState& xState, Zenith_DataStream& xOutStream)
	{
		// Magic + version. Magic catches non-DP blobs early so the
		// version check below doesn't report "unsupported version 0x..."
		// for what's actually garbage.
		xOutStream << kMAGIC;
		xOutStream << xState.m_uSchemaVersion;

		// Possession state. EntityID is a {index, generation} pair --
		// serialise the components individually so the format isn't
		// tied to the exact in-memory layout (which could change with
		// engine refactors).
		xOutStream << xState.m_xPossessedVillager.m_uIndex;
		xOutStream << xState.m_xPossessedVillager.m_uGeneration;
		xOutStream << xState.m_fPossessedLife;

		// Held items: count + entries.
		const uint32_t uHeldCount =
			static_cast<uint32_t>(xState.m_axHeldItems.GetSize());
		xOutStream << uHeldCount;
		for (uint32_t u = 0; u < uHeldCount; ++u)
		{
			const DP_HeldItemEntry& xE = xState.m_axHeldItems.Get(u);
			xOutStream << xE.m_xVillager.m_uIndex;
			xOutStream << xE.m_xVillager.m_uGeneration;
			xOutStream << xE.m_xItem.m_uIndex;
			xOutStream << xE.m_xItem.m_uGeneration;
			// Enum tag -> uint32_t cast for forward-compat (adding
			// values to DP_ItemTag won't change byte layout).
			const uint32_t uTag = static_cast<uint32_t>(xE.m_eTag);
			xOutStream << uTag;
		}

		// Scent table: count + entries.
		const uint32_t uScentCount =
			static_cast<uint32_t>(xState.m_axScent.GetSize());
		xOutStream << uScentCount;
		for (uint32_t u = 0; u < uScentCount; ++u)
		{
			const DP_ScentEntry& xE = xState.m_axScent.Get(u);
			xOutStream << xE.m_xVillager.m_uIndex;
			xOutStream << xE.m_xVillager.m_uGeneration;
			xOutStream << xE.m_fScent;
		}

		xOutStream << xState.m_uObjectivesMask;
		xOutStream << xState.m_fDawnTimerRemaining;
	}

	namespace
	{
		// Helper: returns true if `uBytesNeeded` more bytes are
		// available at the current cursor. Used before every read to
		// fail-soft instead of asserting on truncation.
		bool HasBytes(const Zenith_DataStream& xStream, uint64_t ulBytesNeeded)
		{
			return xStream.GetCursor() + ulBytesNeeded <= xStream.GetSize();
		}
	}

	bool TryLoad(Zenith_DataStream& xInStream, DP_RunState& xOutState)
	{
		// Always start by stamping a fresh default state. If we
		// fail-soft anywhere below, the caller gets a well-defined
		// "use a fresh run" state rather than half-deserialised junk.
		xOutState = DP_RunState{};

		// Magic.
		if (!HasBytes(xInStream, sizeof(uint32_t)))
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"DP_Save::TryLoad: stream too small (%llu bytes) for magic prefix",
				xInStream.GetSize());
			xOutState = DP_RunState{};
			return false;
		}
		uint32_t uMagic = 0;
		xInStream >> uMagic;
		if (uMagic != kMAGIC)
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"DP_Save::TryLoad: magic mismatch (got 0x%08X, expected 0x%08X) -- not a DP save blob",
				uMagic, kMAGIC);
			xOutState = DP_RunState{};
			return false;
		}

		// Version.
		if (!HasBytes(xInStream, sizeof(uint32_t)))
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"DP_Save::TryLoad: stream truncated after magic, no version present");
			xOutState = DP_RunState{};
			return false;
		}
		uint32_t uVersion = 0;
		xInStream >> uVersion;
		if (uVersion < kMIN_SUPPORTED_SCHEMA_VERSION
			|| uVersion > kCURRENT_SCHEMA_VERSION)
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"DP_Save::TryLoad: unsupported schema version %u (supported [%u, %u]) -- falling back to default",
				uVersion, kMIN_SUPPORTED_SCHEMA_VERSION, kCURRENT_SCHEMA_VERSION);
			xOutState = DP_RunState{};
			return false;
		}
		xOutState.m_uSchemaVersion = uVersion;

		// Possessed villager (index + generation + life).
		if (!HasBytes(xInStream, sizeof(uint32_t) * 2 + sizeof(float)))
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"DP_Save::TryLoad: stream truncated reading possession state");
			xOutState = DP_RunState{};
			return false;
		}
		xInStream >> xOutState.m_xPossessedVillager.m_uIndex;
		xInStream >> xOutState.m_xPossessedVillager.m_uGeneration;
		xInStream >> xOutState.m_fPossessedLife;

		// Held items table.
		if (!HasBytes(xInStream, sizeof(uint32_t)))
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"DP_Save::TryLoad: stream truncated before held-items count");
			xOutState = DP_RunState{};
			return false;
		}
		uint32_t uHeldCount = 0;
		xInStream >> uHeldCount;
		if (uHeldCount > kMAX_ENTRIES)
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"DP_Save::TryLoad: held-items count %u exceeds sanity cap %u",
				uHeldCount, kMAX_ENTRIES);
			xOutState = DP_RunState{};
			return false;
		}
		const uint64_t ulHeldBytes =
			static_cast<uint64_t>(uHeldCount) * (sizeof(uint32_t) * 5);
		if (!HasBytes(xInStream, ulHeldBytes))
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"DP_Save::TryLoad: stream truncated reading %u held-items entries",
				uHeldCount);
			xOutState = DP_RunState{};
			return false;
		}
		for (uint32_t u = 0; u < uHeldCount; ++u)
		{
			DP_HeldItemEntry xE;
			xInStream >> xE.m_xVillager.m_uIndex;
			xInStream >> xE.m_xVillager.m_uGeneration;
			xInStream >> xE.m_xItem.m_uIndex;
			xInStream >> xE.m_xItem.m_uGeneration;
			uint32_t uTag = 0;
			xInStream >> uTag;
			xE.m_eTag = static_cast<DP_ItemTag>(uTag);
			xOutState.m_axHeldItems.PushBack(xE);
		}

		// Scent table.
		if (!HasBytes(xInStream, sizeof(uint32_t)))
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"DP_Save::TryLoad: stream truncated before scent count");
			xOutState = DP_RunState{};
			return false;
		}
		uint32_t uScentCount = 0;
		xInStream >> uScentCount;
		if (uScentCount > kMAX_ENTRIES)
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"DP_Save::TryLoad: scent count %u exceeds sanity cap %u",
				uScentCount, kMAX_ENTRIES);
			xOutState = DP_RunState{};
			return false;
		}
		const uint64_t ulScentBytes =
			static_cast<uint64_t>(uScentCount) * (sizeof(uint32_t) * 2 + sizeof(float));
		if (!HasBytes(xInStream, ulScentBytes))
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"DP_Save::TryLoad: stream truncated reading %u scent entries",
				uScentCount);
			xOutState = DP_RunState{};
			return false;
		}
		for (uint32_t u = 0; u < uScentCount; ++u)
		{
			DP_ScentEntry xE;
			xInStream >> xE.m_xVillager.m_uIndex;
			xInStream >> xE.m_xVillager.m_uGeneration;
			xInStream >> xE.m_fScent;
			xOutState.m_axScent.PushBack(xE);
		}

		// Objectives mask + dawn timer.
		if (!HasBytes(xInStream, sizeof(uint32_t) + sizeof(float)))
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"DP_Save::TryLoad: stream truncated before tail fields");
			xOutState = DP_RunState{};
			return false;
		}
		xInStream >> xOutState.m_uObjectivesMask;
		xInStream >> xOutState.m_fDawnTimerRemaining;

		return true;
	}
}
