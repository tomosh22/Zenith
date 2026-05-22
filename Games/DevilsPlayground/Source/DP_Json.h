#pragma once

// Shared JSON parser used by DP's config-loading paths (DPMaterials, DP_Tuning,
// DP_Archetypes, DP_Reagents). Extracted 2026-05-22 from the four byte-identical
// anonymous-namespace copies that lived in those .cpp files; the lift-on-Nth-
// consumer condition documented in Docs/DecisionLog.md PR #3 (2026-05-12) has
// now fired.
//
// Internal data still uses std::vector — this header is the Phase 2a "extract
// verbatim" step, deliberately matching the pre-refactor behaviour byte for
// byte. The Phase 2b commit swaps to Zenith_Vector.

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace DP_Json
{
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

		const JsonValue* FindKey(const char* szKey) const;
	};

	// Reads + parses a JSON file. Returns false on file-open or parse error;
	// xOut is left in an indeterminate state on failure (callers reset it).
	bool LoadJsonFile(const std::filesystem::path& xPath, JsonValue& xOut);
}
