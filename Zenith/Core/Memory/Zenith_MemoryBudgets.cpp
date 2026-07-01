#include "Zenith.h"

#if ZENITH_MEMORY_TRACKING_ANY

#include "Memory/Zenith_MemoryBudgets.h"
#include "Memory/Zenith_MemoryManagement.h"
#include "Memory/Zenith_MemoryFrameSample.h"

Zenith_MemoryBudget Zenith_MemoryBudgets::s_axBudgets[MEMORY_CATEGORY_COUNT] = {};
bool Zenith_MemoryBudgets::s_abWarningLogged[MEMORY_CATEGORY_COUNT] = {};
bool Zenith_MemoryBudgets::s_abOverBudgetLogged[MEMORY_CATEGORY_COUNT] = {};
Zenith_MemoryBudgetEnforcement Zenith_MemoryBudgets::s_eEnforcement = MEMORY_BUDGET_ENFORCE_LOG_ONLY;

namespace
{
	constexpr u_int64 kMB = 1024ull * 1024ull;

	// Default per-category tripwires. Generous ceilings, not targets; 0 = unbudgeted.
	struct DefaultBudget { Zenith_MemoryCategory m_eCat; u_int64 m_ulBytes; };
	const DefaultBudget g_axDefaults[] = {
		{ MEMORY_CATEGORY_RENDERER,    512 * kMB },
		{ MEMORY_CATEGORY_ASSET,      1024 * kMB },
		{ MEMORY_CATEGORY_TERRAIN,     256 * kMB },
		{ MEMORY_CATEGORY_PHYSICS,     128 * kMB },
		{ MEMORY_CATEGORY_SCENE,       128 * kMB },
		{ MEMORY_CATEGORY_GPU_STAGING, 128 * kMB },
		{ MEMORY_CATEGORY_ECS,          64 * kMB },
		{ MEMORY_CATEGORY_ANIMATION,    64 * kMB },
		{ MEMORY_CATEGORY_AUDIO,        64 * kMB },
		{ MEMORY_CATEGORY_AI,           32 * kMB },
		{ MEMORY_CATEGORY_UI,           32 * kMB },
		{ MEMORY_CATEGORY_SCRIPTING,    16 * kMB },
	};

	// Current live bytes for a category, from the tier-agnostic per-frame snapshot
	// (FULL: under the tracker lock; LITE: from the atomic counters). Never a raw
	// unlocked read of the live stats.
	u_int64 CurrentCategoryBytes(Zenith_MemoryCategory eCategory)
	{
		if (eCategory >= MEMORY_CATEGORY_COUNT)
		{
			return 0;
		}
		const Zenith_MemoryFrameSample xSample = Zenith_MemoryManagement::SampleFrame();
		return xSample.m_aulCategoryBytes[eCategory];
	}
}

void Zenith_MemoryBudgets::Initialise()
{
	for (u_int i = 0; i < MEMORY_CATEGORY_COUNT; ++i)
	{
		s_axBudgets[i].m_ulBudgetBytes = 0;
		s_axBudgets[i].m_ulWarningBytes = 0;
		s_axBudgets[i].m_bEnabled = false;
		s_abWarningLogged[i] = false;
		s_abOverBudgetLogged[i] = false;
	}
	for (u_int i = 0; i < COUNT_OF(g_axDefaults); ++i)
	{
		SetBudget(g_axDefaults[i].m_eCat, g_axDefaults[i].m_ulBytes);
	}
#ifdef ZENITH_DEBUG
	s_eEnforcement = MEMORY_BUDGET_ENFORCE_CHECK;
#else
	s_eEnforcement = MEMORY_BUDGET_ENFORCE_LOG_ONLY;
#endif
}

void Zenith_MemoryBudgets::SetEnforcementLevel(Zenith_MemoryBudgetEnforcement eLevel)
{
	s_eEnforcement = eLevel;
}

Zenith_MemoryBudgetEnforcement Zenith_MemoryBudgets::GetEnforcementLevel()
{
	return s_eEnforcement;
}

void Zenith_MemoryBudgets::SetBudget(Zenith_MemoryCategory eCategory, u_int64 ulBudget, u_int64 ulWarning /*= 0*/)
{
	if (eCategory >= MEMORY_CATEGORY_COUNT)
	{
		return;
	}

	s_axBudgets[eCategory].m_ulBudgetBytes = ulBudget;

	// Default warning to 80% of budget if not specified
	if (ulWarning == 0 && ulBudget > 0)
	{
		s_axBudgets[eCategory].m_ulWarningBytes = (ulBudget * 80) / 100;
	}
	else
	{
		s_axBudgets[eCategory].m_ulWarningBytes = ulWarning;
	}

	s_axBudgets[eCategory].m_bEnabled = (ulBudget > 0);
	s_abWarningLogged[eCategory] = false;
	s_abOverBudgetLogged[eCategory] = false;
}

void Zenith_MemoryBudgets::ClearBudget(Zenith_MemoryCategory eCategory)
{
	if (eCategory >= MEMORY_CATEGORY_COUNT)
	{
		return;
	}

	s_axBudgets[eCategory].m_ulBudgetBytes = 0;
	s_axBudgets[eCategory].m_ulWarningBytes = 0;
	s_axBudgets[eCategory].m_bEnabled = false;
	s_abWarningLogged[eCategory] = false;
	s_abOverBudgetLogged[eCategory] = false;
}

bool Zenith_MemoryBudgets::IsOverBudget(Zenith_MemoryCategory eCategory)
{
	if (eCategory >= MEMORY_CATEGORY_COUNT || !s_axBudgets[eCategory].m_bEnabled)
	{
		return false;
	}
	return CurrentCategoryBytes(eCategory) > s_axBudgets[eCategory].m_ulBudgetBytes;
}

bool Zenith_MemoryBudgets::IsNearBudget(Zenith_MemoryCategory eCategory)
{
	if (eCategory >= MEMORY_CATEGORY_COUNT || !s_axBudgets[eCategory].m_bEnabled)
	{
		return false;
	}
	if (s_axBudgets[eCategory].m_ulWarningBytes == 0)
	{
		return false;
	}
	return CurrentCategoryBytes(eCategory) >= s_axBudgets[eCategory].m_ulWarningBytes;
}

u_int64 Zenith_MemoryBudgets::GetBudget(Zenith_MemoryCategory eCategory)
{
	if (eCategory >= MEMORY_CATEGORY_COUNT)
	{
		return 0;
	}
	return s_axBudgets[eCategory].m_ulBudgetBytes;
}

u_int64 Zenith_MemoryBudgets::GetWarningThreshold(Zenith_MemoryCategory eCategory)
{
	if (eCategory >= MEMORY_CATEGORY_COUNT)
	{
		return 0;
	}
	return s_axBudgets[eCategory].m_ulWarningBytes;
}

float Zenith_MemoryBudgets::GetBudgetUsagePercent(Zenith_MemoryCategory eCategory)
{
	if (eCategory >= MEMORY_CATEGORY_COUNT || !s_axBudgets[eCategory].m_bEnabled)
	{
		return 0.0f;
	}
	if (s_axBudgets[eCategory].m_ulBudgetBytes == 0)
	{
		return 0.0f;
	}
	return (static_cast<float>(CurrentCategoryBytes(eCategory)) /
		static_cast<float>(s_axBudgets[eCategory].m_ulBudgetBytes)) * 100.0f;
}

Zenith_MemoryWorstOffender Zenith_MemoryBudgets::GetWorstOffender()
{
	Zenith_MemoryWorstOffender xWorst;
	const Zenith_MemoryFrameSample xSample = Zenith_MemoryManagement::SampleFrame();
	for (u_int i = 0; i < MEMORY_CATEGORY_COUNT; ++i)
	{
		if (!s_axBudgets[i].m_bEnabled || s_axBudgets[i].m_ulBudgetBytes == 0)
		{
			continue;
		}
		if (s_axBudgets[i].m_ulWarningBytes > 0 && xSample.m_aulCategoryBytes[i] < s_axBudgets[i].m_ulWarningBytes)
		{
			continue;   // below the warning line — not an offender
		}
		const float fPct = (static_cast<float>(xSample.m_aulCategoryBytes[i]) /
			static_cast<float>(s_axBudgets[i].m_ulBudgetBytes)) * 100.0f;
		if (fPct > xWorst.m_fUsagePercent)
		{
			xWorst.m_fUsagePercent = fPct;
			xWorst.m_eCategory = static_cast<Zenith_MemoryCategory>(i);
		}
	}
	return xWorst;
}

void Zenith_MemoryBudgets::CheckAllBudgets()
{
	// One tier-agnostic snapshot (FULL: locked copy; LITE: atomics) — never a torn read.
	const Zenith_MemoryFrameSample xSample = Zenith_MemoryManagement::SampleFrame();

	for (u_int i = 0; i < MEMORY_CATEGORY_COUNT; ++i)
	{
		if (!s_axBudgets[i].m_bEnabled)
		{
			continue;
		}

		const Zenith_MemoryCategory eCategory = static_cast<Zenith_MemoryCategory>(i);
		const u_int64 ulCurrent = xSample.m_aulCategoryBytes[i];
		const double fPercent = (s_axBudgets[i].m_ulBudgetBytes > 0)
			? (static_cast<double>(ulCurrent) / static_cast<double>(s_axBudgets[i].m_ulBudgetBytes)) * 100.0
			: 0.0;

		if (ulCurrent > s_axBudgets[i].m_ulBudgetBytes)
		{
			if (!s_abOverBudgetLogged[i])
			{
				// Escalate per the enforcement level — once per crossing edge (latched).
				if (s_eEnforcement == MEMORY_BUDGET_ENFORCE_ASSERT)
				{
					Zenith_Assert(false, "MEMORY BUDGET EXCEEDED: %s using %llu bytes (budget %llu, %.1f%%)",
						GetMemoryCategoryName(eCategory), ulCurrent, s_axBudgets[i].m_ulBudgetBytes, fPercent);
				}
				else if (s_eEnforcement == MEMORY_BUDGET_ENFORCE_CHECK)
				{
					Zenith_Check(false, "MEMORY BUDGET EXCEEDED: %s using %llu bytes (budget %llu, %.1f%%)",
						GetMemoryCategoryName(eCategory), ulCurrent, s_axBudgets[i].m_ulBudgetBytes, fPercent);
				}
				else
				{
					Zenith_Error(LOG_CATEGORY_CORE, "MEMORY BUDGET EXCEEDED: %s using %llu bytes (budget %llu, %.1f%%)",
						GetMemoryCategoryName(eCategory), ulCurrent, s_axBudgets[i].m_ulBudgetBytes, fPercent);
				}
				s_abOverBudgetLogged[i] = true;
			}
		}
		else
		{
			s_abOverBudgetLogged[i] = false;

			if (s_axBudgets[i].m_ulWarningBytes > 0 && ulCurrent >= s_axBudgets[i].m_ulWarningBytes)
			{
				if (!s_abWarningLogged[i])
				{
					Zenith_Log(LOG_CATEGORY_CORE, "Memory budget warning: %s using %llu bytes (%.1f%% of %llu budget)",
						GetMemoryCategoryName(eCategory), ulCurrent, fPercent, s_axBudgets[i].m_ulBudgetBytes);
					s_abWarningLogged[i] = true;
				}
			}
			else
			{
				s_abWarningLogged[i] = false;
			}
		}
	}
}

const Zenith_MemoryBudget& Zenith_MemoryBudgets::GetBudgetInfo(Zenith_MemoryCategory eCategory)
{
	static Zenith_MemoryBudget s_xEmpty = { 0, 0, false };
	if (eCategory >= MEMORY_CATEGORY_COUNT)
	{
		return s_xEmpty;
	}
	return s_axBudgets[eCategory];
}

#endif // ZENITH_MEMORY_TRACKING_ANY
