#include "Zenith.h"

#include "Source/DP_Tuning.h"
#include "Source/DP_Json.h"

#include "Collections/Zenith_Vector.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>

namespace
{
	using DP_Json::JsonValue;
	using DP_Json::JsonObjectEntry;
	using DP_Json::LoadJsonFile;
	using enum DP_Json::JsonType;

	// ---------------------------------------------------------------------------
	// Cache storage. Flat-dotted-key -> numeric/bool entries. A single linear
	// scan per Get<T>() is fine — Tuning.json has < 100 leaf entries, lookup
	// happens out-of-hot-path (gameplay caches its own copies after read).
	// ---------------------------------------------------------------------------
	struct DottedKVPair
	{
		std::string m_strKey;
		double m_fNumber = 0.0;
		bool   m_bBool   = false;
		bool   m_bIsBool = false; // true if this entry came from a JSON bool, not a number
	};

	Zenith_Vector<DottedKVPair>* s_pxKvCache = nullptr;
	bool s_bInitialized = false;

	// Recursively flatten an object tree into dotted-key leaf entries.
	// Skips keys starting with '_' so json comments / metadata never enter
	// the cache. Arrays / strings / nulls at leaf positions are dropped
	// silently — Tuning.json has none.
	void FlattenObject(const JsonValue& xNode, const std::string& strPrefix,
	                   Zenith_Vector<DottedKVPair>& xOut)
	{
		if (xNode.m_eType == JSON_OBJECT)
		{
			for (u_int u = 0; u < xNode.m_axObject.GetSize(); ++u)
			{
				const JsonObjectEntry& xJsonEntry = xNode.m_axObject.Get(u);
				const std::string& strKey = xJsonEntry.m_strKey;
				if (!strKey.empty() && strKey[0] == '_') continue;

				std::string strNext = strPrefix.empty()
				                      ? strKey
				                      : (strPrefix + "." + strKey);
				FlattenObject(xJsonEntry.m_xValue, strNext, xOut);
			}
		}
		else if (xNode.m_eType == JSON_NUMBER || xNode.m_eType == JSON_BOOL)
		{
			DottedKVPair xEntry;
			xEntry.m_strKey  = strPrefix;
			xEntry.m_fNumber = xNode.m_fNumber;
			xEntry.m_bBool   = xNode.m_bBool;
			xEntry.m_bIsBool = (xNode.m_eType == JSON_BOOL);
			xOut.PushBack(xEntry);
		}
		// JSON_ARRAY / JSON_STRING / JSON_NULL -> drop silently.
	}

	const DottedKVPair* FindByKey(const char* szKey)
	{
		if (!s_pxKvCache) return nullptr;
		// Zenith_Vector has no STL iterator — index-based loop per CLAUDE.md.
		for (u_int u = 0; u < s_pxKvCache->GetSize(); ++u)
		{
			const DottedKVPair& xKV = s_pxKvCache->Get(u);
			if (xKV.m_strKey == szKey) return &xKV;
		}
		return nullptr;
	}
}

// ============================================================================
// Public API
// ============================================================================
namespace DP_Tuning
{
	void Initialize()
	{
		if (s_bInitialized) return;

		s_pxKvCache = new Zenith_Vector<DottedKVPair>();

		// GAME_ASSETS_DIR is the per-project assets folder
		// (Games/DevilsPlayground/Assets/). Tuning.json lives one level up
		// in the sibling Config/ folder.
		const std::filesystem::path xPath =
			(std::filesystem::path(GAME_ASSETS_DIR) / ".." / "Config" / "Tuning.json").lexically_normal();

		JsonValue xRoot;
		if (!LoadJsonFile(xPath, xRoot))
		{
			Zenith_Assert(false, "DP_Tuning: failed to load %s", xPath.string().c_str());
			// Q-2026-05-12-006 fix: release the heap allocation before bailing.
			// Without this, the cache leaks until Shutdown -- which guards on
			// s_bInitialized and won't run because we never set it true.
			delete s_pxKvCache;
			s_pxKvCache = nullptr;
			return;
		}

		FlattenObject(xRoot, "", *s_pxKvCache);

		Zenith_Log(LOG_CATEGORY_ASSET,
			"DP_Tuning: loaded %zu values from %s",
			static_cast<size_t>(s_pxKvCache->GetSize()), xPath.string().c_str());

		s_bInitialized = true;
	}

	void Shutdown()
	{
		if (!s_bInitialized) return;

		delete s_pxKvCache;
		s_pxKvCache = nullptr;
		s_bInitialized = false;
	}

	template<> float Get<float>(const char* szDottedKey)
	{
		Zenith_Assert(s_bInitialized, "DP_Tuning::Get called before Initialize");
		const DottedKVPair* pxKV = FindByKey(szDottedKey);
		Zenith_Assert(pxKV != nullptr, "DP_Tuning: missing key '%s'", szDottedKey);
		Zenith_Assert(!pxKV->m_bIsBool,
			"DP_Tuning: key '%s' is a bool, not a number", szDottedKey);
		return static_cast<float>(pxKV->m_fNumber);
	}

	template<> int Get<int>(const char* szDottedKey)
	{
		Zenith_Assert(s_bInitialized, "DP_Tuning::Get called before Initialize");
		const DottedKVPair* pxKV = FindByKey(szDottedKey);
		Zenith_Assert(pxKV != nullptr, "DP_Tuning: missing key '%s'", szDottedKey);
		Zenith_Assert(!pxKV->m_bIsBool,
			"DP_Tuning: key '%s' is a bool, not a number", szDottedKey);
		// std::lround so 4.9 -> 5 cleanly rather than truncating.
		return static_cast<int>(std::lround(pxKV->m_fNumber));
	}

	template<> bool Get<bool>(const char* szDottedKey)
	{
		Zenith_Assert(s_bInitialized, "DP_Tuning::Get called before Initialize");
		const DottedKVPair* pxKV = FindByKey(szDottedKey);
		Zenith_Assert(pxKV != nullptr, "DP_Tuning: missing key '%s'", szDottedKey);
		Zenith_Assert(pxKV->m_bIsBool,
			"DP_Tuning: key '%s' is a number, not a bool", szDottedKey);
		return pxKV->m_bBool;
	}
}
