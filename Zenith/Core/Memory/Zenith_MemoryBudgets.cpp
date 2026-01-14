#include "Zenith.h"

#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED

#include "Memory/Zenith_MemoryBudgets.h"
#include "Memory/Zenith_MemoryTracker.h"

Zenith_MemoryBudget Zenith_MemoryBudgets::s_axBudgets[MEMORY_CATEGORY_COUNT] = {};
bool Zenith_MemoryBudgets::s_abWarningLogged[MEMORY_CATEGORY_COUNT] = {};
bool Zenith_MemoryBudgets::s_abOverBudgetLogged[MEMORY_CATEGORY_COUNT] = {};

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

	// Reset warning flags
	s_abWarningLogged[eCategory] = false;
	s_abOverBudgetLogged[eCategory] = false;

	Zenith_Log(LOG_CATEGORY_CORE, "Memory budget set for %s: %llu bytes (warning at %llu)",
		GetMemoryCategoryName(eCategory),
		ulBudget,
		s_axBudgets[eCategory].m_ulWarningBytes);
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

	const Zenith_MemoryStats& xStats = Zenith_MemoryTracker::GetStats();
	return xStats.m_aulCategoryAllocated[eCategory] > s_axBudgets[eCategory].m_ulBudgetBytes;
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

	const Zenith_MemoryStats& xStats = Zenith_MemoryTracker::GetStats();
	return xStats.m_aulCategoryAllocated[eCategory] >= s_axBudgets[eCategory].m_ulWarningBytes;
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

	const Zenith_MemoryStats& xStats = Zenith_MemoryTracker::GetStats();
	return (static_cast<float>(xStats.m_aulCategoryAllocated[eCategory]) /
		static_cast<float>(s_axBudgets[eCategory].m_ulBudgetBytes)) * 100.0f;
}

void Zenith_MemoryBudgets::CheckAllBudgets()
{
	const Zenith_MemoryStats& xStats = Zenith_MemoryTracker::GetStats();

	for (u_int i = 0; i < MEMORY_CATEGORY_COUNT; ++i)
	{
		Zenith_MemoryCategory eCategory = static_cast<Zenith_MemoryCategory>(i);

		if (!s_axBudgets[i].m_bEnabled)
		{
			continue;
		}

		u_int64 ulCurrent = xStats.m_aulCategoryAllocated[i];

		// Check if over budget
		if (ulCurrent > s_axBudgets[i].m_ulBudgetBytes)
		{
			if (!s_abOverBudgetLogged[i])
			{
				Zenith_Error(LOG_CATEGORY_CORE,
					"MEMORY BUDGET EXCEEDED: %s using %llu bytes (budget: %llu, %.1f%%)",
					GetMemoryCategoryName(eCategory),
					ulCurrent,
					s_axBudgets[i].m_ulBudgetBytes,
					GetBudgetUsagePercent(eCategory));
				s_abOverBudgetLogged[i] = true;
			}
		}
		else
		{
			s_abOverBudgetLogged[i] = false;

			// Check if near budget
			if (s_axBudgets[i].m_ulWarningBytes > 0 && ulCurrent >= s_axBudgets[i].m_ulWarningBytes)
			{
				if (!s_abWarningLogged[i])
				{
					Zenith_Log(LOG_CATEGORY_CORE,
						"Memory budget warning: %s using %llu bytes (%.1f%% of %llu budget)",
						GetMemoryCategoryName(eCategory),
						ulCurrent,
						GetBudgetUsagePercent(eCategory),
						s_axBudgets[i].m_ulBudgetBytes);
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

#endif // ZENITH_MEMORY_MANAGEMENT_ENABLED
