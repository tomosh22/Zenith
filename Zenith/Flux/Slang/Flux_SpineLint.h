#pragma once

#include "Collections/Zenith_Vector.h"

#include <string>
#include <cstring>

// =====================================================================
// Flux_SpineLint  (Flux Shader System Overhaul — D6, the 5th shader gate)
//
// A PURE text lint that enforces the spine access-control contract: the three
// persistent descriptor sets (GLOBAL / VIEW / BINDLESS) are declared and poked
// ONLY in Common/Bindings.slang; every other shader reaches them through the
// free-function accessor facade. Three rules, scanned on comment/string-stripped
// source so tokens inside comments or strings never trip it:
//
//   (1) RULE_SPINE_POKE        — `g_xGlobalSet.` / `g_xViewSet.` / `g_xBindlessSet.`
//                                member access outside Bindings.slang
//   (2) RULE_SPINE_EXTENSION   — `extension GlobalParams|ViewParams|BindlessParams`
//                                outside Bindings.slang (would pierce `private`)
//   (3) RULE_SPINE_BLOCK_REDECL — `ParameterBlock<GlobalParams|ViewParams|
//                                 BindlessParams>` redeclaration outside Bindings.slang
//
// ScanSource is device-free + I/O-free (operates on an in-memory source string),
// so it is unit-testable AND callable from FluxCompiler over a SHADER_SOURCE_ROOT
// walk. Report-only until the Stage-2 spine migration lands, then enforcing.
//
// Names are kept in lockstep with Common/Bindings.slang (the ParameterBlock
// INSTANCE names for pokes; the element-STRUCT names for extension/redeclaration).
// =====================================================================
namespace Flux_SpineLint
{
	enum Rule
	{
		RULE_SPINE_POKE,          // g_xViewSet.member outside Bindings.slang
		RULE_SPINE_EXTENSION,     // extension ViewParams outside Bindings.slang
		RULE_SPINE_BLOCK_REDECL,  // ParameterBlock<ViewParams> outside Bindings.slang
	};

	struct Violation
	{
		std::string m_strFile;             // source path as handed to the scanner
		u_int       m_uLine = 0;           // 1-based line of the offending token
		Rule        m_eRule = RULE_SPINE_POKE;
		std::string m_strDetail;           // the offending identifier (e.g. "g_xViewSet")
	};

	// The three spine ParameterBlock INSTANCE names (a `.member` after one of these
	// is a poke) and the three element-STRUCT type names (extension / redeclaration
	// targets). Lockstep with Common/Bindings.slang.
	inline constexpr const char* const kaszSpineSetNames[3]    = { "g_xGlobalSet", "g_xViewSet", "g_xBindlessSet" };
	inline constexpr const char* const kaszSpineStructNames[3] = { "GlobalParams", "ViewParams", "BindlessParams" };

	inline bool IsIdentChar(char c)
	{
		return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
	}

	// Is this path the spine home (…/Common/Bindings.slang)? Slash-agnostic; matches
	// on the trailing "Common/Bindings.slang" so it is robust to absolute vs relative.
	inline bool IsBindingsFile(const char* szPath)
	{
		if (!szPath) return false;
		std::string str(szPath);
		for (char& c : str) { if (c == '\\') c = '/'; }
		// Normalise case only for the comparison suffix (Windows paths).
		std::string strLower = str;
		for (char& c : strLower) { if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a'); }
		const char* szNeedle = "common/bindings.slang";
		const size_t nNeedle = std::strlen(szNeedle);
		return strLower.size() >= nNeedle && strLower.compare(strLower.size() - nNeedle, nNeedle, szNeedle) == 0;
	}

	// Comment/string stripping is factored into per-state steppers so each function
	// stays low-cognitive-complexity (the whole-machine form tripped the gate).
	namespace Detail
	{
		enum StripState { STRIP_CODE, STRIP_LINE, STRIP_BLOCK, STRIP_STRING, STRIP_CHAR };

		// Each stepper appends the replacement char(s) for `c` and returns the next
		// state; uExtra receives EXTRA input chars consumed (two-char tokens // /* */
		// and backslash-escapes). Replacements preserve newlines (line-number parity)
		// and tabs in code/line-comments/block-comments; string interiors collapse to
		// single spaces.
		inline StripState StepCode(char c, char cNext, std::string& strOut, u_int& uExtra)
		{
			uExtra = 0u;
			if (c == '/' && cNext == '/') { strOut += ' '; return STRIP_LINE; }
			if (c == '/' && cNext == '*') { strOut += ' '; return STRIP_BLOCK; }
			if (c == '"')  { strOut += ' '; return STRIP_STRING; }
			if (c == '\'') { strOut += ' '; return STRIP_CHAR; }
			strOut += c;
			return STRIP_CODE;
		}
		inline StripState StepLine(char c, std::string& strOut)
		{
			if (c == '\n') { strOut += '\n'; return STRIP_CODE; }
			strOut += (c == '\t' ? '\t' : ' ');
			return STRIP_LINE;
		}
		inline StripState StepBlock(char c, char cNext, std::string& strOut, u_int& uExtra)
		{
			uExtra = 0u;
			if (c == '*' && cNext == '/') { strOut += "  "; uExtra = 1u; return STRIP_CODE; }
			strOut += (c == '\n' ? '\n' : (c == '\t' ? '\t' : ' '));
			return STRIP_BLOCK;
		}
		inline StripState StepQuoted(char c, char cNext, char cQuote, StripState eThis, std::string& strOut, u_int& uExtra)
		{
			uExtra = 0u;
			if (c == '\\' && cNext != '\0') { strOut += "  "; uExtra = 1u; return eThis; }
			if (c == cQuote) { strOut += ' '; return STRIP_CODE; }
			strOut += (c == '\n' ? '\n' : ' ');
			return eThis;
		}
	}

	// Replace // and /* */ comments and "…" / '…' string/char literals with spaces,
	// preserving newlines so line numbers in the stripped copy match the original.
	inline std::string StripCommentsAndStrings(const char* szSource)
	{
		std::string strOut;
		if (!szSource) return strOut;
		const std::string strIn(szSource);
		strOut.reserve(strIn.size());

		Detail::StripState eState = Detail::STRIP_CODE;
		for (size_t i = 0; i < strIn.size(); i++)
		{
			const char c     = strIn[i];
			const char cNext = (i + 1 < strIn.size()) ? strIn[i + 1] : '\0';
			u_int uExtra = 0u;
			switch (eState)
			{
			case Detail::STRIP_CODE:   eState = Detail::StepCode(c, cNext, strOut, uExtra); break;
			case Detail::STRIP_LINE:   eState = Detail::StepLine(c, strOut); break;
			case Detail::STRIP_BLOCK:  eState = Detail::StepBlock(c, cNext, strOut, uExtra); break;
			case Detail::STRIP_STRING: eState = Detail::StepQuoted(c, cNext, '"',  Detail::STRIP_STRING, strOut, uExtra); break;
			case Detail::STRIP_CHAR:   eState = Detail::StepQuoted(c, cNext, '\'', Detail::STRIP_CHAR,   strOut, uExtra); break;
			}
			i += uExtra;
		}
		return strOut;
	}

	namespace Detail
	{
		// Length of the identifier starting at s[i] (0 if s[i] is not an ident start).
		inline size_t IdentLen(const std::string& s, size_t i)
		{
			size_t n = 0;
			while (i + n < s.size() && IsIdentChar(s[i + n])) n++;
			return n;
		}
		inline bool TokenEquals(const std::string& s, size_t start, size_t len, const char* sz)
		{
			return std::strlen(sz) == len && s.compare(start, len, sz) == 0;
		}
		// Skip whitespace forward from j.
		inline size_t SkipWs(const std::string& s, size_t j)
		{
			while (j < s.size() && (s[j] == ' ' || s[j] == '\t' || s[j] == '\n' || s[j] == '\r')) j++;
			return j;
		}
	}

	// Scan ONE shader source string for the three rules. Appends any violations to
	// axOut (with 1-based line numbers) and returns true iff the source is CLEAN.
	// bIsBindingsFile exempts the spine home entirely.
	inline bool ScanSource(const char* szFile, const char* szSource, bool bIsBindingsFile,
	                       Zenith_Vector<Violation>& axOut)
	{
		if (bIsBindingsFile) return true;   // the spine home legitimately declares + pokes the sets
		if (!szSource) return true;

		const std::string s = StripCommentsAndStrings(szSource);
		const size_t uCountBefore = axOut.GetSize();

		u_int uLine = 1;
		for (size_t i = 0; i < s.size(); )
		{
			const char c = s[i];
			if (c == '\n') { uLine++; i++; continue; }

			// Only inspect at an identifier boundary (previous char is not ident).
			const bool bAtIdentStart = IsIdentChar(c) && (i == 0 || !IsIdentChar(s[i - 1]));
			if (!bAtIdentStart) { i++; continue; }

			const size_t uLen = Detail::IdentLen(s, i);
			auto fnAdd = [&](Rule eRule, const char* szDetail)
			{
				Violation xV;
				xV.m_strFile   = szFile ? szFile : "";
				xV.m_uLine     = uLine;
				xV.m_eRule     = eRule;
				xV.m_strDetail = szDetail;
				axOut.PushBack(xV);
			};

			// (1) Spine poke: identifier == a set name, immediately (post-ws) followed by '.'.
			for (const char* szSet : kaszSpineSetNames)
			{
				if (Detail::TokenEquals(s, i, uLen, szSet))
				{
					const size_t j = Detail::SkipWs(s, i + uLen);
					if (j < s.size() && s[j] == '.') fnAdd(RULE_SPINE_POKE, szSet);
					break;
				}
			}

			// (2) extension <SpineStruct>
			if (Detail::TokenEquals(s, i, uLen, "extension"))
			{
				const size_t j = Detail::SkipWs(s, i + uLen);
				const size_t k = Detail::IdentLen(s, j);
				for (const char* szStruct : kaszSpineStructNames)
				{
					if (k && Detail::TokenEquals(s, j, k, szStruct)) { fnAdd(RULE_SPINE_EXTENSION, szStruct); break; }
				}
			}

			// (3) ParameterBlock < <SpineStruct> >
			if (Detail::TokenEquals(s, i, uLen, "ParameterBlock"))
			{
				size_t j = Detail::SkipWs(s, i + uLen);
				if (j < s.size() && s[j] == '<')
				{
					j = Detail::SkipWs(s, j + 1);
					const size_t k = Detail::IdentLen(s, j);
					for (const char* szStruct : kaszSpineStructNames)
					{
						if (k && Detail::TokenEquals(s, j, k, szStruct)) { fnAdd(RULE_SPINE_BLOCK_REDECL, szStruct); break; }
					}
				}
			}

			i += uLen;   // advance past the whole identifier
		}

		return axOut.GetSize() == uCountBefore;
	}

	inline const char* RuleName(Rule eRule)
	{
		switch (eRule)
		{
		case RULE_SPINE_POKE:         return "spine-poke";
		case RULE_SPINE_EXTENSION:    return "spine-extension";
		case RULE_SPINE_BLOCK_REDECL: return "spine-block-redecl";
		}
		return "?";
	}
}
