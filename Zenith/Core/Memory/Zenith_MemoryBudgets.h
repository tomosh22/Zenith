#pragma once

#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED

#include "Zenith_MemoryCategories.h"

struct Zenith_MemoryBudget
{
	u_int64 m_ulBudgetBytes;    // Max allowed for category (0 = unlimited)
	u_int64 m_ulWarningBytes;   // Threshold for warning (0 = no warning)
	bool m_bEnabled;            // Whether budget is enforced
};

class Zenith_MemoryBudgets
{
public:
	static void Initialise();

	// Set budget for a category (ulWarning defaults to 80% of budget if 0)
	static void SetBudget(Zenith_MemoryCategory eCategory, u_int64 ulBudget, u_int64 ulWarning = 0);
	static void ClearBudget(Zenith_MemoryCategory eCategory);

	// Query budget status
	static bool IsOverBudget(Zenith_MemoryCategory eCategory);
	static bool IsNearBudget(Zenith_MemoryCategory eCategory);  // Past warning threshold
	static u_int64 GetBudget(Zenith_MemoryCategory eCategory);
	static u_int64 GetWarningThreshold(Zenith_MemoryCategory eCategory);
	static float GetBudgetUsagePercent(Zenith_MemoryCategory eCategory);

	// Check all budgets and log warnings
	static void CheckAllBudgets();

	// Get budget info for UI
	static const Zenith_MemoryBudget& GetBudgetInfo(Zenith_MemoryCategory eCategory);

private:
	static Zenith_MemoryBudget s_axBudgets[MEMORY_CATEGORY_COUNT];
	static bool s_abWarningLogged[MEMORY_CATEGORY_COUNT];  // Prevent spam
	static bool s_abOverBudgetLogged[MEMORY_CATEGORY_COUNT];
};

#endif // ZENITH_MEMORY_MANAGEMENT_ENABLED
