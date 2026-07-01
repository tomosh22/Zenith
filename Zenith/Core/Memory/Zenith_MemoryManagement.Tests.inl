// Unit tests for the tiered memory allocator. #included at the bottom of
// Zenith_MemoryManagement.cpp (an always-linked TU, so MSVC can't dead-strip the
// ZENITH_TEST static-init registration). Tier-aware: category-stack + accounting run
// at any tier; the LITE round-trip runs only at LITE; the FULL tracker + budgets run
// only at FULL. Each test is self-contained and side-effect-balanced (alloc+free, or
// save/restore) so it does not pollute the live global allocator state.

#if ZENITH_MEMORY_TRACKING_ANY

// ---- Category scope stack (the attribution mechanism) ----------------------
ZENITH_TEST(MemoryCategory, ScopeStackPushPopNesting)
{
	const Zenith_MemoryCategory eBase = Zenith_MemoryManagement::GetCurrentCategory();
	Zenith_MemoryManagement::PushCategory(MEMORY_CATEGORY_RENDERER);
	ZENITH_ASSERT_TRUE(Zenith_MemoryManagement::GetCurrentCategory() == MEMORY_CATEGORY_RENDERER, "push sets current category");
	Zenith_MemoryManagement::PushCategory(MEMORY_CATEGORY_PHYSICS);
	ZENITH_ASSERT_TRUE(Zenith_MemoryManagement::GetCurrentCategory() == MEMORY_CATEGORY_PHYSICS, "nested push");
	Zenith_MemoryManagement::PopCategory();
	ZENITH_ASSERT_TRUE(Zenith_MemoryManagement::GetCurrentCategory() == MEMORY_CATEGORY_RENDERER, "pop restores previous");
	Zenith_MemoryManagement::PopCategory();
	ZENITH_ASSERT_TRUE(Zenith_MemoryManagement::GetCurrentCategory() == eBase, "pop returns to base");
}

ZENITH_TEST(MemoryCategory, ScopedRAIIRestores)
{
	const Zenith_MemoryCategory eBase = Zenith_MemoryManagement::GetCurrentCategory();
	{
		ZENITH_MEMORY_SCOPE(MEMORY_CATEGORY_ASSET);
		ZENITH_ASSERT_TRUE(Zenith_MemoryManagement::GetCurrentCategory() == MEMORY_CATEGORY_ASSET, "scope sets category");
	}
	ZENITH_ASSERT_TRUE(Zenith_MemoryManagement::GetCurrentCategory() == eBase, "scope restores on exit");
}

// ---- Unified accounting (aggregation invariants; read-only, no pollution) ---
ZENITH_TEST(MemoryAccounting, SplitInvariantAndBounds)
{
	const u_int uCount = Zenith_MemoryAccounting::GetSourceCount();
	ZENITH_ASSERT_TRUE(uCount <= 32u, "source count within capacity");

	// Every source is either RAM or VRAM (disjoint), so the two totals sum to all bytes.
	u_int64 ulSumAll = 0;
	for (u_int i = 0; i < uCount; ++i)
	{
		ulSumAll += Zenith_MemoryAccounting::GetSource(i).m_ulBytes;
	}
	ZENITH_ASSERT_TRUE(Zenith_MemoryAccounting::GetTotalProcessRAM() + Zenith_MemoryAccounting::GetTotalVRAM() == ulSumAll,
		"process RAM + VRAM equals the sum of all source bytes");

	// Out-of-range index returns a safe empty source (no OOB read).
	const Zenith_MemorySource& xEmpty = Zenith_MemoryAccounting::GetSource(9999u);
	ZENITH_ASSERT_TRUE(xEmpty.m_ulBytes == 0 && xEmpty.m_ulAllocCount == 0, "out-of-range source is empty");
}

#endif // ZENITH_MEMORY_TRACKING_ANY

// ---- LITE allocator round-trip (offset base-recovery + alignment) ----------
#if ZENITH_MEMORY_TRACKING_ANY && !ZENITH_MEMORY_TRACKING_FULL
ZENITH_TEST(MemoryLite, RoundTripAlignmentAndOverAlignment)
{
	const size_t auAligns[] = { 0u, 16u, 32u, 64u, 256u, (1u << 21) }; // incl. >1 MiB (review #11)
	for (size_t a = 0; a < sizeof(auAligns) / sizeof(auAligns[0]); ++a)
	{
		const size_t ulAlign = auAligns[a];
		void* p = Zenith_MemoryManagement::AllocateLite(200, ulAlign, MEMORY_CATEGORY_TEMP);
		ZENITH_ASSERT_TRUE(p != nullptr, "LITE allocation is non-null");
		if (ulAlign > 0)
		{
			ZENITH_ASSERT_TRUE((reinterpret_cast<uintptr_t>(p) % ulAlign) == 0, "LITE user pointer honours requested alignment");
		}
		memset(p, 0xAB, 200);                              // exercise the whole user region
		Zenith_MemoryManagement::DeallocateLite(p);        // must recover the real base via the header offset
	}
}
#endif

// ---- FULL tracker round-trip (guard placement + alignment + copy-under-lock)
#if ZENITH_MEMORY_TRACKING_FULL
ZENITH_TEST(MemoryTracker, TrackedRoundTripAlignmentAndFrontGuard)
{
	const size_t auAligns[] = { 0u, 16u, 32u, 64u };
	for (size_t a = 0; a < sizeof(auAligns) / sizeof(auAligns[0]); ++a)
	{
		const size_t ulAlign = auAligns[a];
		void* p = Zenith_MemoryManagement::AllocateTracked(128, ulAlign, MEMORY_CATEGORY_GENERAL, nullptr, 0);
		ZENITH_ASSERT_TRUE(p != nullptr, "tracked allocation is non-null");
		const size_t ulExpect = (ulAlign > 0) ? ulAlign : alignof(std::max_align_t);
		ZENITH_ASSERT_TRUE((reinterpret_cast<uintptr_t>(p) % ulExpect) == 0, "tracked user pointer is correctly aligned");
		// Front guard lives in the 4 bytes immediately before the user pointer.
		const u_int32 uFront = *reinterpret_cast<const u_int32*>(static_cast<const u_int8*>(p) - uGUARD_SIZE);
		ZENITH_ASSERT_TRUE(uFront == uMEMORY_GUARD_PATTERN, "front guard is present just before the user pointer");
		memset(p, 0xAB, 128);
		Zenith_MemoryManagement::DeallocateTracked(p);
	}
}

ZENITH_TEST(MemoryTracker, CopyAllocationHitAndMiss)
{
	void* p = Zenith_MemoryManagement::AllocateTracked(64, 0, MEMORY_CATEGORY_GENERAL, nullptr, 0);
	Zenith_AllocationRecord xRec;
	ZENITH_ASSERT_TRUE(Zenith_MemoryTracker::CopyAllocation(p, xRec), "CopyAllocation finds a tracked pointer");
	ZENITH_ASSERT_TRUE(xRec.m_ulSize == 64 && xRec.m_pAddress == p, "copied record matches the allocation");
	Zenith_MemoryManagement::DeallocateTracked(p);

	int iOnStack = 0;
	ZENITH_ASSERT_TRUE(!Zenith_MemoryTracker::CopyAllocation(&iOnStack, xRec), "CopyAllocation misses an untracked pointer");
}

ZENITH_TEST(MemoryBudgets, SetWarningDefaultAndClear)
{
	const Zenith_MemoryBudget xSaved = Zenith_MemoryBudgets::GetBudgetInfo(MEMORY_CATEGORY_AI);

	Zenith_MemoryBudgets::SetBudget(MEMORY_CATEGORY_AI, 1000000, 0);
	ZENITH_ASSERT_TRUE(Zenith_MemoryBudgets::GetBudget(MEMORY_CATEGORY_AI) == 1000000, "budget is set");
	ZENITH_ASSERT_TRUE(Zenith_MemoryBudgets::GetWarningThreshold(MEMORY_CATEGORY_AI) == 800000, "warning defaults to 80% of budget");

	Zenith_MemoryBudgets::ClearBudget(MEMORY_CATEGORY_AI);
	ZENITH_ASSERT_TRUE(!Zenith_MemoryBudgets::GetBudgetInfo(MEMORY_CATEGORY_AI).m_bEnabled, "clear disables the budget");

	// Restore prior state so the test is side-effect free.
	if (xSaved.m_bEnabled)
	{
		Zenith_MemoryBudgets::SetBudget(MEMORY_CATEGORY_AI, xSaved.m_ulBudgetBytes, xSaved.m_ulWarningBytes);
	}
}
#endif // ZENITH_MEMORY_TRACKING_FULL
