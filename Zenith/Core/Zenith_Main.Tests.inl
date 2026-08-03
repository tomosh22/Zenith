#include "UnitTests/Zenith_UnitTests.h"

// ============================================================================
// Boot-profile dump coordinator.
//
// Three call sites race to emit the boot artifact — the main loop, the
// --memory-capture loop, and the orderly-shutdown fallback — because each covers
// exits the others never see. Exactly one of them must win, and the loser must
// not silently suppress the artifact for a run where only IT fires. That is the
// whole contract of the latch, tested here against a LOCAL latch so the real
// process-lifetime one is untouched.
// ============================================================================

ZENITH_TEST(Core, BootDumpCoordinatorOnce)
{
	bool bLatch = false;

	ZENITH_ASSERT_TRUE(ClaimBootProfileDump("boot.txt", bLatch), "the first claim with a path must win");
	ZENITH_ASSERT_TRUE(bLatch, "a winning claim must take the latch");
	ZENITH_ASSERT_FALSE(ClaimBootProfileDump("boot.txt", bLatch), "a second claim must lose, whatever its reason");
	ZENITH_ASSERT_FALSE(ClaimBootProfileDump("other.txt", bLatch), "a different path cannot re-open a claimed latch");

	// The shutdown fallback is the only call site that fires on an early-exit run
	// (--bench-ecs, --list-automated-tests). It must be able to win outright.
	bool bFallbackOnly = false;
	ZENITH_ASSERT_TRUE(ClaimBootProfileDump("boot.txt", bFallbackOnly), "the shutdown fallback must win when it is the only caller");

	// No --boot-profile-dump: nothing is ever written and the latch stays open, so a
	// later run-configuration change cannot be masked by a stale claim.
	bool bNoPath = false;
	ZENITH_ASSERT_FALSE(ClaimBootProfileDump(nullptr, bNoPath), "no path means no artifact");
	ZENITH_ASSERT_FALSE(bNoPath, "a refused claim must NOT consume the latch");
}
