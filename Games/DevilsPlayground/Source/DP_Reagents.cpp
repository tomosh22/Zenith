#include "Zenith.h"

#include "Source/DP_Reagents.h"
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
	// a pointer into this vector. Linear lookup over 14 entries is fine.
	// ---------------------------------------------------------------------------
	Zenith_Vector<DP_Reagents::Reagent>* s_pxReagents = nullptr;
	bool s_bInitialized = false;

	float ReadNumber(const JsonValue& xObj, const char* szKey, float fFallback = 0.0f)
	{
		const JsonValue* pxV = xObj.FindKey(szKey);
		if (pxV == nullptr || pxV->m_eType != JSON_NUMBER) return fFallback;
		return static_cast<float>(pxV->m_fNumber);
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

	void ParseTint(const JsonValue& xObj, DP_Reagents::Reagent& xOut)
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

	const DP_Reagents::Reagent* FindById(const char* szId)
	{
		if (s_pxReagents == nullptr) return nullptr;
		for (u_int u = 0; u < s_pxReagents->GetSize(); ++u)
		{
			const DP_Reagents::Reagent& xR = s_pxReagents->Get(u);
			if (xR.id == szId) return &s_pxReagents->Get(u);
		}
		return nullptr;
	}
}

// ============================================================================
// Public API
// ============================================================================
namespace DP_Reagents
{
	void Initialize()
	{
		if (s_bInitialized) return;

		s_pxReagents = new Zenith_Vector<Reagent>();

		const std::filesystem::path xPath =
			(std::filesystem::path(GAME_ASSETS_DIR) / ".." / "Config" / "Reagents.json").lexically_normal();

		JsonValue xRoot;
		if (!LoadJsonFile(xPath, xRoot))
		{
			Zenith_Assert(false, "DP_Reagents: failed to load %s", xPath.string().c_str());
			return;
		}

		const JsonValue* pxArr = xRoot.FindKey("reagents");
		Zenith_Assert(pxArr != nullptr && pxArr->m_eType == JSON_ARRAY,
			"DP_Reagents: 'reagents' key missing or not an array");
		if (pxArr == nullptr || pxArr->m_eType != JSON_ARRAY) return;

		for (u_int u = 0; u < pxArr->m_axArray.GetSize(); ++u)
		{
			const JsonValue& xEntry = pxArr->m_axArray.Get(u);
			if (xEntry.m_eType != JSON_OBJECT) continue;
			Reagent xR;
			xR.id                   = ReadString(xEntry, "id");
			xR.display_name_key     = ReadString(xEntry, "display_name_key");
			xR.mvp                  = ReadBool(xEntry, "mvp", false);
			xR.pickup_channel_s     = ReadNumber(xEntry, "pickup_channel_s");
			xR.special_behaviour    = ReadString(xEntry, "special_behaviour");
			xR.evaporate_duration_s = ReadNumber(xEntry, "evaporate_duration_s");
			xR.rarity               = ReadNumber(xEntry, "rarity");
			ParseTint(xEntry, xR);
			if (!xR.id.empty())
			{
				s_pxReagents->PushBack(xR);
			}
		}

		Zenith_Log(LOG_CATEGORY_ASSET,
			"DP_Reagents: loaded %zu reagents from %s",
			static_cast<size_t>(s_pxReagents->GetSize()), xPath.string().c_str());

		s_bInitialized = true;
	}

	void Shutdown()
	{
		if (!s_bInitialized) return;
		delete s_pxReagents;
		s_pxReagents = nullptr;
		s_bInitialized = false;
	}

	const Reagent* Get(const char* szId)
	{
		Zenith_Assert(s_bInitialized, "DP_Reagents::Get called before Initialize");
		const Reagent* pxR = FindById(szId);
		Zenith_Assert(pxR != nullptr, "DP_Reagents: unknown reagent id '%s'", szId);
		return pxR;
	}

	const Reagent* TryGet(const char* szId)
	{
		if (!s_bInitialized) return nullptr;
		return FindById(szId);
	}

	size_t Count()
	{
		if (!s_bInitialized || s_pxReagents == nullptr) return 0;
		return static_cast<size_t>(s_pxReagents->GetSize());
	}

	const Reagent* GetByIndex(size_t uIdx)
	{
		if (!s_bInitialized || s_pxReagents == nullptr) return nullptr;
		if (uIdx >= static_cast<size_t>(s_pxReagents->GetSize())) return nullptr;
		return &s_pxReagents->Get(static_cast<u_int>(uIdx));
	}

	bool IsMvp(const char* szId)
	{
		const Reagent* pxR = FindById(szId);
		return pxR != nullptr && pxR->mvp;
	}
}
