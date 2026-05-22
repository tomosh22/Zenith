#include "Zenith.h"

#include "Source/DP_Archetypes.h"
#include "Source/DP_Json.h"

#include "Collections/Zenith_Vector.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace
{
	using DP_Json::JsonValue;
	using DP_Json::LoadJsonFile;
	using enum DP_Json::JsonType;

	// ---------------------------------------------------------------------------
	// Cache storage. Stable for the lifetime of the namespace -- Get(id) returns
	// a pointer into this vector. Linear lookup over the 24 entries is fine; the
	// API is not called from hot paths.
	// ---------------------------------------------------------------------------
	Zenith_Vector<DP_Archetypes::Archetype>* s_pxArchetypes = nullptr;
	bool s_bInitialized = false;

	// Helper: read a number with a fallback if the key is missing or the wrong
	// type. JSON-comment / metadata fields (_comment etc.) are silently skipped.
	float ReadNumber(const JsonValue& xObj, const char* szKey, float fFallback = 0.0f)
	{
		const JsonValue* pxV = xObj.FindKey(szKey);
		if (pxV == nullptr || pxV->m_eType != JSON_NUMBER) return fFallback;
		return static_cast<float>(pxV->m_fNumber);
	}

	int ReadInt(const JsonValue& xObj, const char* szKey, int iFallback = 0)
	{
		const JsonValue* pxV = xObj.FindKey(szKey);
		if (pxV == nullptr || pxV->m_eType != JSON_NUMBER) return iFallback;
		return static_cast<int>(pxV->m_fNumber);
	}

	bool ReadBool(const JsonValue& xObj, const char* szKey, bool bFallback = false)
	{
		const JsonValue* pxV = xObj.FindKey(szKey);
		if (pxV == nullptr || pxV->m_eType != JSON_BOOL) return bFallback;
		return pxV->m_bBool;
	}

	std::string ReadString(const JsonValue& xObj, const char* szKey)
	{
		const JsonValue* pxV = xObj.FindKey(szKey);
		if (pxV == nullptr || pxV->m_eType != JSON_STRING) return std::string();
		return pxV->m_strString;
	}

	void ParseTint(const JsonValue& xObj, DP_Archetypes::Archetype& xOut)
	{
		const JsonValue* pxArr = xObj.FindKey("tint_rgb");
		if (pxArr == nullptr || pxArr->m_eType != JSON_ARRAY) return;
		if (pxArr->m_axArray.GetSize() < 3) return;
		const JsonValue& xR = pxArr->m_axArray.Get(0);
		const JsonValue& xG = pxArr->m_axArray.Get(1);
		const JsonValue& xB = pxArr->m_axArray.Get(2);
		if (xR.m_eType == JSON_NUMBER) xOut.tint_r = static_cast<float>(xR.m_fNumber);
		if (xG.m_eType == JSON_NUMBER) xOut.tint_g = static_cast<float>(xG.m_fNumber);
		if (xB.m_eType == JSON_NUMBER) xOut.tint_b = static_cast<float>(xB.m_fNumber);
	}

	const DP_Archetypes::Archetype* FindById(const char* szId)
	{
		if (s_pxArchetypes == nullptr) return nullptr;
		for (u_int u = 0; u < s_pxArchetypes->GetSize(); ++u)
		{
			const DP_Archetypes::Archetype& xA = s_pxArchetypes->Get(u);
			if (xA.id == szId) return &s_pxArchetypes->Get(u);
		}
		return nullptr;
	}
}

// ============================================================================
// Public API
// ============================================================================
namespace DP_Archetypes
{
	void Initialize()
	{
		if (s_bInitialized) return;

		s_pxArchetypes = new Zenith_Vector<Archetype>();

		const std::filesystem::path xPath =
			(std::filesystem::path(GAME_ASSETS_DIR) / ".." / "Config" / "Archetypes.json").lexically_normal();

		JsonValue xRoot;
		if (!LoadJsonFile(xPath, xRoot))
		{
			Zenith_Assert(false, "DP_Archetypes: failed to load %s", xPath.string().c_str());
			return;
		}

		const JsonValue* pxArr = xRoot.FindKey("archetypes");
		Zenith_Assert(pxArr != nullptr && pxArr->m_eType == JSON_ARRAY,
			"DP_Archetypes: 'archetypes' key missing or not an array");
		if (pxArr == nullptr || pxArr->m_eType != JSON_ARRAY) return;

		for (u_int u = 0; u < pxArr->m_axArray.GetSize(); ++u)
		{
			const JsonValue& xEntry = pxArr->m_axArray.Get(u);
			if (xEntry.m_eType != JSON_OBJECT) continue;
			Archetype xA;
			xA.id                   = ReadString(xEntry, "id");
			xA.display_name_key     = ReadString(xEntry, "display_name_key");
			xA.mvp                  = ReadBool(xEntry, "mvp", false);
			xA.life_timer_s         = ReadNumber(xEntry, "life_timer_s");
			xA.walk_speed_mps       = ReadNumber(xEntry, "walk_speed_mps");
			xA.jog_speed_mps        = ReadNumber(xEntry, "jog_speed_mps");
			xA.sprint_speed_mps     = ReadNumber(xEntry, "sprint_speed_mps");
			xA.possession_channel_s = ReadNumber(xEntry, "possession_channel_s");
			xA.demon_scent_floor    = ReadNumber(xEntry, "demon_scent_floor");
			xA.rarity               = ReadNumber(xEntry, "rarity");
			xA.min_spawns           = ReadInt(xEntry, "min_spawns");
			xA.max_spawns           = ReadInt(xEntry, "max_spawns");
			ParseTint(xEntry, xA);
			if (!xA.id.empty())
			{
				s_pxArchetypes->PushBack(xA);
			}
		}

		Zenith_Log(LOG_CATEGORY_ASSET,
			"DP_Archetypes: loaded %zu archetypes from %s",
			static_cast<size_t>(s_pxArchetypes->GetSize()), xPath.string().c_str());

		s_bInitialized = true;
	}

	void Shutdown()
	{
		if (!s_bInitialized) return;
		delete s_pxArchetypes;
		s_pxArchetypes = nullptr;
		s_bInitialized = false;
	}

	const Archetype* Get(const char* szId)
	{
		Zenith_Assert(s_bInitialized, "DP_Archetypes::Get called before Initialize");
		const Archetype* pxA = FindById(szId);
		Zenith_Assert(pxA != nullptr, "DP_Archetypes: unknown archetype id '%s'", szId);
		return pxA;
	}

	size_t Count()
	{
		if (!s_bInitialized || s_pxArchetypes == nullptr) return 0;
		return static_cast<size_t>(s_pxArchetypes->GetSize());
	}

	const Archetype* GetByIndex(size_t uIdx)
	{
		if (!s_bInitialized || s_pxArchetypes == nullptr) return nullptr;
		if (uIdx >= static_cast<size_t>(s_pxArchetypes->GetSize())) return nullptr;
		return &s_pxArchetypes->Get(static_cast<u_int>(uIdx));
	}

	bool IsMvp(const char* szId)
	{
		const Archetype* pxA = FindById(szId);
		return pxA != nullptr && pxA->mvp;
	}
}
