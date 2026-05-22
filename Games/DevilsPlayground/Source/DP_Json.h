#pragma once

// Shared JSON parser used by DP's config-loading paths (DPMaterials, DP_Tuning,
// DP_Archetypes, DP_Reagents). Extracted 2026-05-22 from the four byte-identical
// anonymous-namespace copies that lived in those .cpp files; the lift-on-Nth-
// consumer condition documented in Docs/DecisionLog.md PR #3 (2026-05-12) has
// now fired.
//
// 2026-05-22 Phase 2b: internal containers swapped from std::vector to
// Zenith_Vector for convention compliance with the rest of the engine
// (memory pool, no STL allocator). Note that Zenith_Vector has no begin/end /
// operator[]; iterate with index loops via GetSize() + Get(u).

#include "Collections/Zenith_Vector.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>

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
		Zenith_Vector<JsonValue> m_axArray;
		Zenith_Vector<std::pair<std::string, JsonValue>> m_axObject;

		const JsonValue* FindKey(const char* szKey) const;
	};

	// Reads + parses a JSON file. Returns false on file-open or parse error;
	// xOut is left in an indeterminate state on failure (callers reset it).
	bool LoadJsonFile(const std::filesystem::path& xPath, JsonValue& xOut);
}
