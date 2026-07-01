#pragma once

#if ZENITH_MEMORY_TRACKING_ANY

#include "Zenith_MemoryCategories.h"

struct Zenith_MemoryBudget
{
	u_int64 m_ulBudgetBytes;    // Max allowed for category (0 = unlimited)
	u_int64 m_ulWarningBytes;   // Threshold for warning (0 = no warning)
	bool m_bEnabled;            // Whether budget is enforced
};

// What happens when a category crosses its budget. Escalation is per-crossing-edge
// (latched), never per-frame spam.
enum Zenith_MemoryBudgetEnforcement : u_int8
{
	MEMORY_BUDGET_ENFORCE_LOG_ONLY = 0,  // one Zenith_Error per crossing (Release default)
	MEMORY_BUDGET_ENFORCE_CHECK,         // + Zenith_Check(false, ...) — release-survivable (Debug default)
	MEMORY_BUDGET_ENFORCE_ASSERT,        // + Zenith_Assert(false, ...) — break in the debugger (opt-in)
};

struct Zenith_MemoryWorstOffender
{
	Zenith_MemoryCategory m_eCategory = MEMORY_CATEGORY_GENERAL;
	float                 m_fUsagePercent = 0.0f;   // 0 if no budget is over its warning line
};

// Per-category memory budgets. Works at BOTH tiers: budget checks read the current
// per-category bytes from Zenith_MemoryManagement::SampleFrame() (FULL: under the
// tracker lock; LITE: from the atomic counters), so enforcement is live in Release too.
class Zenith_MemoryBudgets
{
public:
	// Seeds the default per-category tripwires and the tier-appropriate enforcement level.
	static void Initialise();

	// Set budget for a category (ulWarning defaults to 80% of budget if 0)
	static void SetBudget(Zenith_MemoryCategory eCategory, u_int64 ulBudget, u_int64 ulWarning = 0);
	static void ClearBudget(Zenith_MemoryCategory eCategory);

	static void SetEnforcementLevel(Zenith_MemoryBudgetEnforcement eLevel);
	static Zenith_MemoryBudgetEnforcement GetEnforcementLevel();

	// Query budget status
	static bool IsOverBudget(Zenith_MemoryCategory eCategory);
	static bool IsNearBudget(Zenith_MemoryCategory eCategory);  // Past warning threshold
	static u_int64 GetBudget(Zenith_MemoryCategory eCategory);
	static u_int64 GetWarningThreshold(Zenith_MemoryCategory eCategory);
	static float GetBudgetUsagePercent(Zenith_MemoryCategory eCategory);

	// Highest-usage budgeted category that is at/over its warning line (for the HUD).
	static Zenith_MemoryWorstOffender GetWorstOffender();

	// Check all budgets once per frame: log + escalate per the enforcement level.
	static void CheckAllBudgets();

	// Get budget info for UI
	static const Zenith_MemoryBudget& GetBudgetInfo(Zenith_MemoryCategory eCategory);

private:
	static Zenith_MemoryBudget s_axBudgets[MEMORY_CATEGORY_COUNT];
	static bool s_abWarningLogged[MEMORY_CATEGORY_COUNT];  // Prevent spam
	static bool s_abOverBudgetLogged[MEMORY_CATEGORY_COUNT];
	static Zenith_MemoryBudgetEnforcement s_eEnforcement;
};

#endif // ZENITH_MEMORY_TRACKING_ANY
