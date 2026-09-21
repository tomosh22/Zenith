#pragma once

#include "Collections/Zenith_Vector.h"
#include "Flux/Slang/Flux_SpineLint.h"   // reuses the pure comment/string stripper + token helpers

#include <string>
#include <cstring>

// =====================================================================
// Flux_NormalDecodeLint  (Photorealism.md §4 — "Nothing pairs a texture's
// exported compression with the decode its shader assumes.")
//
// A PURE text lint, in the same shape as Flux_SpineLint: normal-map decode
// has exactly one legitimate site — `UnpackNormalMapTS` in
// Common/Material.slang — because the encoding (BC5, R+G only, Z rebuilt)
// is a policy decision declared per texture in TextureUsage.ztexdecl, not
// something a feature shader can see. §1.10 was exactly this gap reopened:
// a shader that re-derives the decode itself can silently disagree with the
// exporter (BC5's stored blue channel reads 0, decoding Z to -1, which
// pointed every terrain shading normal into the ground and killed CSM
// reception with zero failing tests). This lint rejects the open-coded
// shape wherever it resurfaces.
//
// The hazard is false positives: `<x> * 2.0 - 1.0` is overwhelmingly the
// UV -> NDC remap idiom elsewhere in this codebase (Text, Grass, Particles,
// SSAO, IBL, Quads, Bindings, Fog, Skybox, ...), and none of those involve a
// texture sample. So the rule matches specifically the SAMPLE-PLUS-SWIZZLE
// prefix, not the arithmetic alone:
//
//   (Sample|SampleLevel|Load)( ... ) . <xy|xyz|rg|rgb>  *  2[.0][f]  -  1[.0][f]
//
// tolerant of arbitrary whitespace/newlines between tokens and of the
// `2`/`2.0`/`2.0f`/`2f` and `1`/`1.0`/`1.0f`/`1f` spellings. Scanned on
// comment/string-stripped source (via Flux_SpineLint::StripCommentsAndStrings)
// so a mention inside `//`/`/* */`/`"…"` never trips it.
//
// ScanSource is device-free + I/O-free (operates on an in-memory source
// string), so it is unit-testable AND callable from FluxCompiler over a
// SHADER_SOURCE_ROOT walk, exactly like Flux_SpineLint.
// =====================================================================
namespace Flux_NormalDecodeLint
{
	enum Rule
	{
		RULE_NORMAL_DECODE,   // open-coded Sample(...).swizzle * 2 - 1 outside Common/Material.slang
	};

	struct Violation
	{
		std::string m_strFile;             // source path as handed to the scanner
		u_int       m_uLine = 0;           // 1-based line of the offending Sample/SampleLevel/Load call
		Rule        m_eRule = RULE_NORMAL_DECODE;
		std::string m_strDetail;           // the offending shape, e.g. "Sample(...).xy * 2 - 1"
	};

	// Is this path the one legitimate decode site (…/Common/Material.slang)? Slash-agnostic,
	// case-insensitive on the suffix, matching Flux_SpineLint::IsBindingsFile's shape. Note this
	// deliberately does NOT match Common/MaterialSurface.slang, whose two open-coded sites are
	// genuine violations this lint must catch (fixed at the source instead — see MaterialSurface.slang).
	inline bool IsMaterialSlangFile(const char* szPath)
	{
		if (!szPath) return false;
		std::string str(szPath);
		for (char& c : str) { if (c == '\\') c = '/'; }
		std::string strLower = str;
		for (char& c : strLower) { if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a'); }
		const char* szNeedle = "common/material.slang";
		const size_t nNeedle = std::strlen(szNeedle);
		return strLower.size() >= nNeedle && strLower.compare(strLower.size() - nNeedle, nNeedle, szNeedle) == 0;
	}

	namespace Detail
	{
		inline bool IsAllowedSwizzle(const std::string& s, size_t uStart, size_t uLen)
		{
			static const char* const kaszSwizzles[4] = { "xy", "xyz", "rg", "rgb" };
			for (const char* szSwiz : kaszSwizzles)
			{
				if (Flux_SpineLint::Detail::TokenEquals(s, uStart, uLen, szSwiz)) return true;
			}
			return false;
		}

		// Parses a numeric literal token (digits ['.' digits] ['f'|'F']) starting at s[i].
		// On success advances i past the whole token and returns the leading integer-digit
		// substring (e.g. "2" from "2.0f", "1" from "1"). Returns an empty string and leaves
		// i unchanged if s[i] is not a digit.
		inline std::string ParseNumberIntPart(const std::string& s, size_t& i)
		{
			size_t j = i;
			if (j >= s.size() || !(s[j] >= '0' && s[j] <= '9')) return std::string();
			const size_t uIntStart = j;
			while (j < s.size() && s[j] >= '0' && s[j] <= '9') j++;
			const std::string strInt = s.substr(uIntStart, j - uIntStart);
			if (j < s.size() && s[j] == '.')
			{
				j++;
				while (j < s.size() && s[j] >= '0' && s[j] <= '9') j++;
			}
			if (j < s.size() && (s[j] == 'f' || s[j] == 'F')) j++;
			i = j;
			return strInt;
		}

		// Finds the index one-past the ')' matching the '(' at s[uOpenParen], tracking nesting
		// depth so an argument list containing its own parens (e.g. a nested function call)
		// does not close early. Returns s.size() (not found) if unbalanced.
		inline size_t FindMatchingCloseParen(const std::string& s, size_t uOpenParen)
		{
			size_t uDepth = 1;
			size_t k = uOpenParen + 1;
			for (; k < s.size() && uDepth > 0; k++)
			{
				if (s[k] == '(') uDepth++;
				else if (s[k] == ')') uDepth--;
			}
			return (uDepth == 0) ? k : s.size();
		}
	}

	// Scan ONE shader source string for an open-coded normal-map decode. Appends any
	// violations to axOut (with 1-based line numbers) and returns true iff the source is
	// CLEAN. bIsMaterialFile exempts the one legitimate decode site.
	inline bool ScanSource(const char* szFile, const char* szSource, bool bIsMaterialFile,
	                       Zenith_Vector<Violation>& axOut)
	{
		if (bIsMaterialFile) return true;   // Common/Material.slang legitimately owns the decode
		if (!szSource) return true;

		const std::string s = Flux_SpineLint::StripCommentsAndStrings(szSource);
		const size_t uCountBefore = axOut.GetSize();

		static const char* const kaszSampleFns[3] = { "SampleLevel", "Sample", "Load" };

		u_int uLine = 1;
		for (size_t i = 0; i < s.size(); )
		{
			const char c = s[i];
			if (c == '\n') { uLine++; i++; continue; }

			// Only inspect at an identifier boundary (previous char is not ident).
			const bool bAtIdentStart = Flux_SpineLint::IsIdentChar(c) && (i == 0 || !Flux_SpineLint::IsIdentChar(s[i - 1]));
			if (!bAtIdentStart) { i++; continue; }

			const size_t uLen = Flux_SpineLint::Detail::IdentLen(s, i);
			const u_int  uCallLine = uLine;

			const char* szMatchedFn = nullptr;
			for (const char* szFn : kaszSampleFns)
			{
				if (Flux_SpineLint::Detail::TokenEquals(s, i, uLen, szFn)) { szMatchedFn = szFn; break; }
			}

			if (szMatchedFn)
			{
				size_t j = Flux_SpineLint::Detail::SkipWs(s, i + uLen);
				if (j < s.size() && s[j] == '(')
				{
					const size_t uAfterClose = Detail::FindMatchingCloseParen(s, j);
					if (uAfterClose < s.size())
					{
						size_t m = Flux_SpineLint::Detail::SkipWs(s, uAfterClose);
						if (m < s.size() && s[m] == '.')
						{
							m = Flux_SpineLint::Detail::SkipWs(s, m + 1);
							const size_t uSwizLen = Flux_SpineLint::Detail::IdentLen(s, m);
							if (uSwizLen > 0 && Detail::IsAllowedSwizzle(s, m, uSwizLen))
							{
								size_t n = Flux_SpineLint::Detail::SkipWs(s, m + uSwizLen);
								if (n < s.size() && s[n] == '*')
								{
									n = Flux_SpineLint::Detail::SkipWs(s, n + 1);
									const std::string strFirst = Detail::ParseNumberIntPart(s, n);
									if (strFirst == "2")
									{
										n = Flux_SpineLint::Detail::SkipWs(s, n);
										if (n < s.size() && s[n] == '-')
										{
											n = Flux_SpineLint::Detail::SkipWs(s, n + 1);
											const std::string strSecond = Detail::ParseNumberIntPart(s, n);
											if (strSecond == "1")
											{
												Violation xV;
												xV.m_strFile   = szFile ? szFile : "";
												xV.m_uLine     = uCallLine;
												xV.m_eRule     = RULE_NORMAL_DECODE;
												xV.m_strDetail = std::string(szMatchedFn) + "(...)."
													+ s.substr(m, uSwizLen) + " * 2 - 1";
												axOut.PushBack(xV);
											}
										}
									}
								}
							}
						}
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
		case RULE_NORMAL_DECODE: return "normal-decode";
		}
		return "?";
	}
}
