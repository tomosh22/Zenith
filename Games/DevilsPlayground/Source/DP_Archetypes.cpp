#include "Zenith.h"

#include "Source/DP_Archetypes.h"

#include "Collections/Zenith_Vector.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
	// ---------------------------------------------------------------------------
	// Tiny JSON parser (hand-rolled). Mirrors the parser in DP_Tuning.cpp /
	// DPMaterials.cpp's anonymous namespaces. Per the existing DP_Tuning comment,
	// the duplication is intentional and scope-controlled; a shared utility lift
	// is explicitly deferred.
	// ---------------------------------------------------------------------------
	enum JsonType : uint8_t
	{
		JSON_NULL,
		JSON_BOOL,
		JSON_NUMBER,
		JSON_STRING,
		JSON_ARRAY,
		JSON_OBJECT,
	};

	struct JsonValue
	{
		JsonType m_eType = JSON_NULL;
		double m_fNumber = 0.0;
		bool m_bBool = false;
		std::string m_strString;
		std::vector<JsonValue> m_axArray;
		std::vector<std::pair<std::string, JsonValue>> m_axObject;

		const JsonValue* FindKey(const char* szKey) const
		{
			if (m_eType != JSON_OBJECT) return nullptr;
			for (const auto& xPair : m_axObject)
			{
				if (xPair.first == szKey) return &xPair.second;
			}
			return nullptr;
		}
	};

	class JsonParser
	{
	public:
		explicit JsonParser(const std::string& strSrc) : m_strSrc(strSrc), m_uPos(0) {}

		bool Parse(JsonValue& xOut)
		{
			SkipWhitespace();
			if (!ParseValue(xOut)) return false;
			SkipWhitespace();
			return m_uPos == m_strSrc.size();
		}

	private:
		const std::string& m_strSrc;
		size_t m_uPos;

		bool AtEnd() const { return m_uPos >= m_strSrc.size(); }
		char Peek() const { return m_strSrc[m_uPos]; }

		void SkipWhitespace()
		{
			while (!AtEnd())
			{
				char c = m_strSrc[m_uPos];
				if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++m_uPos;
				else break;
			}
		}

		bool ParseValue(JsonValue& xOut)
		{
			SkipWhitespace();
			if (AtEnd()) return false;
			char c = Peek();
			if (c == '{') return ParseObject(xOut);
			if (c == '[') return ParseArray(xOut);
			if (c == '"') return ParseString(xOut);
			if (c == 't' || c == 'f') return ParseBool(xOut);
			if (c == 'n') return ParseNull(xOut);
			if (c == '-' || (c >= '0' && c <= '9')) return ParseNumber(xOut);
			return false;
		}

		bool ParseObject(JsonValue& xOut)
		{
			xOut.m_eType = JSON_OBJECT;
			++m_uPos;
			SkipWhitespace();
			if (!AtEnd() && Peek() == '}') { ++m_uPos; return true; }
			while (!AtEnd())
			{
				SkipWhitespace();
				JsonValue xKey;
				if (!ParseString(xKey)) return false;
				SkipWhitespace();
				if (AtEnd() || Peek() != ':') return false;
				++m_uPos;
				JsonValue xVal;
				if (!ParseValue(xVal)) return false;
				xOut.m_axObject.emplace_back(std::move(xKey.m_strString), std::move(xVal));
				SkipWhitespace();
				if (AtEnd()) return false;
				if (Peek() == ',') { ++m_uPos; continue; }
				if (Peek() == '}') { ++m_uPos; return true; }
				return false;
			}
			return false;
		}

		bool ParseArray(JsonValue& xOut)
		{
			xOut.m_eType = JSON_ARRAY;
			++m_uPos;
			SkipWhitespace();
			if (!AtEnd() && Peek() == ']') { ++m_uPos; return true; }
			while (!AtEnd())
			{
				JsonValue xVal;
				if (!ParseValue(xVal)) return false;
				xOut.m_axArray.push_back(std::move(xVal));
				SkipWhitespace();
				if (AtEnd()) return false;
				if (Peek() == ',') { ++m_uPos; continue; }
				if (Peek() == ']') { ++m_uPos; return true; }
				return false;
			}
			return false;
		}

		bool ParseString(JsonValue& xOut)
		{
			if (AtEnd() || Peek() != '"') return false;
			++m_uPos;
			xOut.m_eType = JSON_STRING;
			while (!AtEnd())
			{
				char c = m_strSrc[m_uPos++];
				if (c == '"') return true;
				if (c == '\\' && !AtEnd())
				{
					char cEsc = m_strSrc[m_uPos++];
					switch (cEsc)
					{
					case '"': xOut.m_strString.push_back('"'); break;
					case '\\': xOut.m_strString.push_back('\\'); break;
					case '/': xOut.m_strString.push_back('/'); break;
					case 'n': xOut.m_strString.push_back('\n'); break;
					case 't': xOut.m_strString.push_back('\t'); break;
					case 'r': xOut.m_strString.push_back('\r'); break;
					case 'b': xOut.m_strString.push_back('\b'); break;
					case 'f': xOut.m_strString.push_back('\f'); break;
					default:  xOut.m_strString.push_back(cEsc); break;
					}
				}
				else
				{
					xOut.m_strString.push_back(c);
				}
			}
			return false;
		}

		bool ParseNumber(JsonValue& xOut)
		{
			size_t uStart = m_uPos;
			if (!AtEnd() && Peek() == '-') ++m_uPos;
			while (!AtEnd())
			{
				char c = Peek();
				bool bDigit = (c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E'
				           || c == '+' || c == '-';
				if (!bDigit) break;
				++m_uPos;
			}
			if (m_uPos == uStart) return false;
			xOut.m_eType = JSON_NUMBER;
			std::string strNum = m_strSrc.substr(uStart, m_uPos - uStart);
			xOut.m_fNumber = std::strtod(strNum.c_str(), nullptr);
			return true;
		}

		bool ParseBool(JsonValue& xOut)
		{
			if (m_strSrc.compare(m_uPos, 4, "true") == 0)
			{
				m_uPos += 4;
				xOut.m_eType = JSON_BOOL;
				xOut.m_bBool = true;
				return true;
			}
			if (m_strSrc.compare(m_uPos, 5, "false") == 0)
			{
				m_uPos += 5;
				xOut.m_eType = JSON_BOOL;
				xOut.m_bBool = false;
				return true;
			}
			return false;
		}

		bool ParseNull(JsonValue& xOut)
		{
			if (m_strSrc.compare(m_uPos, 4, "null") == 0)
			{
				m_uPos += 4;
				xOut.m_eType = JSON_NULL;
				return true;
			}
			return false;
		}
	};

	bool LoadJsonFile(const std::filesystem::path& xPath, JsonValue& xOut)
	{
		std::ifstream xIn(xPath, std::ios::binary);
		if (!xIn.is_open()) return false;
		std::stringstream xBuf;
		xBuf << xIn.rdbuf();
		std::string strSrc = xBuf.str();
		JsonParser xParser(strSrc);
		return xParser.Parse(xOut);
	}

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
		if (pxArr->m_axArray.size() < 3) return;
		const JsonValue& xR = pxArr->m_axArray[0];
		const JsonValue& xG = pxArr->m_axArray[1];
		const JsonValue& xB = pxArr->m_axArray[2];
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

		for (const auto& xEntry : pxArr->m_axArray)
		{
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
