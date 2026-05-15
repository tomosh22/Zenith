#include "Zenith.h"

#include "Source/DP_Tuning.h"

#include "Collections/Zenith_Vector.h"

#include <cmath>
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
	// Tiny JSON parser (hand-rolled). Cloned verbatim from DPMaterials.cpp's
	// anonymous namespace — the two copies are an intentional, scope-controlled
	// duplication. Refactoring to a shared utility is explicitly deferred.
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
			for (const auto& xPair : xNode.m_axObject)
			{
				const std::string& strKey = xPair.first;
				if (!strKey.empty() && strKey[0] == '_') continue;

				std::string strNext = strPrefix.empty()
				                      ? strKey
				                      : (strPrefix + "." + strKey);
				FlattenObject(xPair.second, strNext, xOut);
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
