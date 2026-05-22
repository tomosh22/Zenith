#include "Zenith.h"

#include "DP_Json.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace DP_Json
{
	const JsonValue* JsonValue::FindKey(const char* szKey) const
	{
		if (m_eType != JSON_OBJECT) return nullptr;
		for (u_int u = 0; u < m_axObject.GetSize(); ++u)
		{
			const auto& xPair = m_axObject.Get(u);
			if (xPair.first == szKey) return &xPair.second;
		}
		return nullptr;
	}

	namespace
	{
		// Tiny JSON parser (hand-rolled). Supports objects, arrays, strings,
		// numbers, true/false, null. Doesn't support escapes beyond \" \\ \/
		// \n \t \r \b \f. The DP config dumps don't need anything more.
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
					xOut.m_axObject.EmplaceBack(std::move(xKey.m_strString), std::move(xVal));
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
					xOut.m_axArray.PushBack(std::move(xVal));
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
	}

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
}
