#include "Zenith.h"

#include "Flux/RenderViews/Flux_ViewPassNames.h"

#include <cstring>   // strcmp / strlen / memcpy — the content key and the arena writes

// ============================================================================
// Flux_ViewPassNames — implementation.
//
// Pure CPU, no device, no engine singleton. See the header for the lifetime
// contract; this file owns the two tables (slot suffixes + the one legacy row)
// and the interning arena's linear content-keyed lookup.
// ============================================================================

namespace
{
	// Slot 6 is the dedicated animation-preview view. Unit D2-a adds it to
	// Flux_RenderViews.h as kuFluxViewSlotPreviewAnim; that constant does not
	// exist yet and this unit deliberately does NOT add it, so the slot index is
	// spelled out here with this note. Every other slot below uses the real
	// constant (kuFluxViewSlotMain / kuFluxViewSlotShadowFirst /
	// kuFluxViewSlotPreview) rather than a literal.
	constexpr u_int kuViewSlotPreviewAnimProvisional = 6u;

	// One suffix per view slot, total over FLUX_MAX_RENDER_VIEWS.
	//
	//   0 (kuFluxViewSlotMain)        nullptr — identity, no suffix is ever composed
	//   1..4 (kuFluxViewSlotShadowFirst + c)
	//                                 present for totality but NEVER USED today: the
	//                                 cascades keep their hand-named "Shadow Cascade N"
	//                                 passes (Flux/Shadows/Flux_Shadows.cpp:180-187) and
	//                                 nothing routes a cascade pass through this pool.
	//   5 (kuFluxViewSlotPreview)     "Preview" — the 44 historical "<base> (Preview)" names
	//   6 (kuViewSlotPreviewAnimProvisional)
	//                                 "AnimPreview"
	//   7                             "View7" — the last fixed slot, unassigned today
	const char* const s_aszViewSlotSuffixes[FLUX_MAX_RENDER_VIEWS] =
	{
		nullptr,
		"Shadow 0",
		"Shadow 1",
		"Shadow 2",
		"Shadow 3",
		"Preview",
		"AnimPreview",
		"View7",
	};
	static_assert(FLUX_MAX_RENDER_VIEWS == 8u,
		"s_aszViewSlotSuffixes has one row per view slot — add/remove rows with FLUX_MAX_RENDER_VIEWS");
	static_assert(kuFluxViewSlotPreview == 5u,
		"the suffix table's row comments describe slot 5 as the material preview");
	static_assert(kuViewSlotPreviewAnimProvisional < FLUX_MAX_RENDER_VIEWS,
		"the animation-preview slot must fit the fixed view registry");
	static_assert(kuViewSlotPreviewAnimProvisional == kuFluxViewSlotPreview + 1u,
		"the animation-preview view sits immediately after the material preview");

	// The legacy table: names that are NOT of the generic "<base> (<suffix>)"
	// form and must keep their historical spelling so no existing pass literal
	// changes. It has exactly ONE row.
	//
	// Of the 45 preview pass names in the tree, 44 are exactly "<base> (Preview)".
	// The outlier is the LDR-transition pass declared at
	// Flux/HDR/Flux_HDR.cpp:746, which spells it as a PREFIX:
	// "Preview LDR Transition". Its base is PREVIEW-ONLY — there is no slot-0
	// pass named "LDR Transition". Unit D1-e composed that site through
	// Flux_ViewPassName("LDR Transition", uViewSlot) and gave the pass its
	// .View(uViewSlot), so the historical spelling now comes from THIS row rather
	// than from a literal, and the pass records on the preview slot instead of
	// falling through to slot 0.
	//
	// Slot 6 is deliberately NOT a row: it falls through to the generic form and
	// yields "LDR Transition (AnimPreview)".
	//
	// Keyed by CONTENT (strcmp), never by pointer — MSVC /GF pools identical
	// literals, so a pointer key would match in one TU and miss in another.
	struct ViewPassNameLegacyRow
	{
		const char* m_szBase;
		u_int       m_uViewSlot;
		const char* m_szHistoricalName;
	};

	const ViewPassNameLegacyRow s_axViewPassNameLegacyRows[] =
	{
		{ "LDR Transition", kuFluxViewSlotPreview, "Preview LDR Transition" },
	};

	const char* FindLegacyName(const char* szBase, u_int uViewSlot)
	{
		for (const ViewPassNameLegacyRow& xRow : s_axViewPassNameLegacyRows)
		{
			if (xRow.m_uViewSlot == uViewSlot && strcmp(xRow.m_szBase, szBase) == 0)
			{
				return xRow.m_szHistoricalName;
			}
		}
		return nullptr;
	}
}

const char* Flux_ViewSlotSuffix(u_int uViewSlot)
{
	if (uViewSlot >= FLUX_MAX_RENDER_VIEWS)
	{
		return nullptr;
	}
	return s_aszViewSlotSuffixes[uViewSlot];
}

Flux_ViewPassNamePool::Flux_ViewPassNamePool(u_int uCapacityBases)
	: m_uCapacityBases(uCapacityBases)
	, m_uUsedBases(0u)
{
	Zenith_Assert(uCapacityBases <= kuFLUX_VIEW_PASS_NAME_MAX_BASES,
		"Flux_ViewPassNamePool: capacity %u exceeds the fixed arena extent %u", uCapacityBases, kuFLUX_VIEW_PASS_NAME_MAX_BASES);
	if (m_uCapacityBases > kuFLUX_VIEW_PASS_NAME_MAX_BASES)
	{
		m_uCapacityBases = kuFLUX_VIEW_PASS_NAME_MAX_BASES;
	}
}

bool Flux_ViewPassNamePool::Compose(char* pcOut, const char* szBase, u_int uViewSlot)
{
	const char* szLegacy = FindLegacyName(szBase, uViewSlot);
	if (szLegacy != nullptr)
	{
		const u_int uLegacyLen = static_cast<u_int>(strlen(szLegacy));
		Zenith_Assert(uLegacyLen + 1u <= kuFLUX_VIEW_PASS_NAME_MAX_LEN,
			"Flux_ViewPassNamePool: legacy name '%s' does not fit the %u-byte arena entry", szLegacy, kuFLUX_VIEW_PASS_NAME_MAX_LEN);
		if (uLegacyLen + 1u > kuFLUX_VIEW_PASS_NAME_MAX_LEN)
		{
			return false;
		}
		memcpy(pcOut, szLegacy, uLegacyLen + 1u);
		return true;
	}

	const char* szSuffix = Flux_ViewSlotSuffix(uViewSlot);
	Zenith_Assert(szSuffix != nullptr,
		"Flux_ViewPassNamePool: view slot %u has no suffix (slot 0 is pointer identity and must never reach Compose)", uViewSlot);
	if (szSuffix == nullptr)
	{
		return false;
	}

	// "<base>" + " (" + "<suffix>" + ")" + NUL
	const u_int uBaseLen   = static_cast<u_int>(strlen(szBase));
	const u_int uSuffixLen = static_cast<u_int>(strlen(szSuffix));
	const u_int uTotal     = uBaseLen + 2u + uSuffixLen + 1u + 1u;
	Zenith_Assert(uTotal <= kuFLUX_VIEW_PASS_NAME_MAX_LEN,
		"Flux_ViewPassNamePool: '%s (%s)' needs %u bytes, arena entries are %u", szBase, szSuffix, uTotal, kuFLUX_VIEW_PASS_NAME_MAX_LEN);
	if (uTotal > kuFLUX_VIEW_PASS_NAME_MAX_LEN)
	{
		return false;
	}

	u_int uWrite = 0u;
	memcpy(pcOut, szBase, uBaseLen);
	uWrite += uBaseLen;
	pcOut[uWrite++] = ' ';
	pcOut[uWrite++] = '(';
	memcpy(pcOut + uWrite, szSuffix, uSuffixLen);
	uWrite += uSuffixLen;
	pcOut[uWrite++] = ')';
	pcOut[uWrite]   = '\0';
	return true;
}

u_int Flux_ViewPassNamePool::FindOrAddBase(const char* szBase)
{
	// Linear strcmp scan over the USED entries. At most 64 bases x 7 slots, and
	// only ever called during graph setup — the cost is irrelevant beside the
	// correctness of keying by content rather than by pointer.
	for (u_int u = 0; u < m_uUsedBases; u++)
	{
		if (strcmp(m_aacBases[u], szBase) == 0)
		{
			return u;
		}
	}

	const u_int uBaseLen = static_cast<u_int>(strlen(szBase));
	Zenith_Assert(uBaseLen > 0u && uBaseLen < kuFLUX_VIEW_PASS_NAME_MAX_LEN,
		"Flux_ViewPassNamePool: base name of length %u does not fit the %u-byte arena entry", uBaseLen, kuFLUX_VIEW_PASS_NAME_MAX_LEN);
	if (uBaseLen == 0u || uBaseLen >= kuFLUX_VIEW_PASS_NAME_MAX_LEN)
	{
		return kuInvalidBase;
	}

	Zenith_Assert(m_uUsedBases < m_uCapacityBases,
		"Flux_ViewPassNamePool: no room for base '%s' (capacity %u distinct bases)", szBase, m_uCapacityBases);
	if (m_uUsedBases >= m_uCapacityBases)
	{
		return kuInvalidBase;
	}

	// The pool COPIES the base, so a caller may key off a runtime-built buffer
	// even though slot 0 still hands its own pointer straight back.
	memcpy(m_aacBases[m_uUsedBases], szBase, uBaseLen + 1u);
	return m_uUsedBases++;
}

const char* Flux_ViewPassNamePool::Name(const char* szBase, u_int uViewSlot)
{
	Zenith_Assert(szBase != nullptr, "Flux_ViewPassNamePool::Name: null base name");
	if (szBase == nullptr)
	{
		return "";
	}

	// Slot 0 is POINTER IDENTITY. FindPass / SetPassForceDisabled / the profiler
	// labels all key off the main view's historical names, so the main view must
	// keep handing the graph the caller's own literal.
	if (uViewSlot == kuFluxViewSlotMain)
	{
		return szBase;
	}

	Zenith_Assert(uViewSlot < FLUX_MAX_RENDER_VIEWS,
		"Flux_ViewPassNamePool::Name: view slot %u out of range", uViewSlot);
	if (uViewSlot >= FLUX_MAX_RENDER_VIEWS)
	{
		return szBase;
	}

	const u_int uBase = FindOrAddBase(szBase);
	if (uBase == kuInvalidBase)
	{
		return szBase;
	}

	char* pcEntry = m_aaacNames[uBase][uViewSlot - 1u];
	if (pcEntry[0] == '\0')
	{
		if (!Compose(pcEntry, szBase, uViewSlot))
		{
			pcEntry[0] = '\0';
			return szBase;
		}
	}
	return pcEntry;
}

namespace
{
	// The sanctioned cross-TU singleton shape: a function-local static, so the
	// arena is constructed on first use and never before. There is no reset —
	// see the header (the graph and the GPU timer table hold pointers into it).
	Flux_ViewPassNamePool& GlobalViewPassNamePool()
	{
		static Flux_ViewPassNamePool ls_xPool;
		return ls_xPool;
	}
}

const char* Flux_ViewPassName(const char* szBase, u_int uViewSlot)
{
	return GlobalViewPassNamePool().Name(szBase, uViewSlot);
}

bool Flux_ViewPassNames_ForceLink()
{
	return true;
}

#include "Flux/RenderViews/Flux_ViewPassNames.Tests.inl"
