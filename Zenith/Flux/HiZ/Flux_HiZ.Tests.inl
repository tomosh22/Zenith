#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/HiZ/Flux_HiZImpl.h"
#include "Flux/RenderViews/Flux_ViewPassNames.h"

// ============================================================================
// Flux_HiZ unit tests — the two pure, device-free pieces of the per-view setup:
// the pass-name BASE table (and what Flux_ViewPassName composes from it for a
// non-main slot) and the mip-count formula every view derives its chain length
// from. Both run headless: no graph, no device, no engine singleton.
//
// Hosted from the bottom of Flux_HiZ.cpp, so the base table and the file-scope
// count constant are visible here directly — the table has exactly one home.
// ============================================================================

// Deleting a hand-written second name table only stays safe if something pins
// the strings it used to hold. This transcribes BOTH historical tables — the 12
// slot-0 bases and the 12 " (Preview)" spellings — and checks the pool
// reproduces the preview column from the base column. A typo in a base literal
// (or a suffix-table edit in Flux_ViewPassNames.cpp) changes a pass name that
// FindPass / SetPassForceDisabled / the GPU-timer labels key off, and no build
// gate would notice: names are strings the graph merely stores.
ZENITH_TEST(HiZ, BaseNamesAreTheTwelveHistoricalLiterals)
{
	// The historical slot-0 names, transcribed rather than derived.
	static const char* const s_aszExpectedBases[] = {
		"HiZ Mip 0",  "HiZ Mip 1",  "HiZ Mip 2",  "HiZ Mip 3",
		"HiZ Mip 4",  "HiZ Mip 5",  "HiZ Mip 6",  "HiZ Mip 7",
		"HiZ Mip 8",  "HiZ Mip 9",  "HiZ Mip 10", "HiZ Mip 11"
	};
	// The historical preview names, exactly as the deleted second table spelled
	// them out in Flux_HiZ.cpp before this unit.
	static const char* const s_aszExpectedPreview[] = {
		"HiZ Mip 0 (Preview)",  "HiZ Mip 1 (Preview)",  "HiZ Mip 2 (Preview)",  "HiZ Mip 3 (Preview)",
		"HiZ Mip 4 (Preview)",  "HiZ Mip 5 (Preview)",  "HiZ Mip 6 (Preview)",  "HiZ Mip 7 (Preview)",
		"HiZ Mip 8 (Preview)",  "HiZ Mip 9 (Preview)",  "HiZ Mip 10 (Preview)", "HiZ Mip 11 (Preview)"
	};
	static_assert(sizeof(s_aszExpectedBases)   / sizeof(s_aszExpectedBases[0])   == 12u, "12 historical base names");
	static_assert(sizeof(s_aszExpectedPreview) / sizeof(s_aszExpectedPreview[0]) == 12u, "12 historical preview names");

	ZENITH_ASSERT_EQ(uHIZ_NUM_PASS_NAMES, 12u, "the base table still has one entry per mip of the 12-mip chain");
	ZENITH_ASSERT_EQ(uHIZ_NUM_PASS_NAMES, Flux_HiZImpl::uHIZ_MAX_MIPS, "base table length == the mip cap");

	for (u_int uMip = 0; uMip < uHIZ_NUM_PASS_NAMES; uMip++)
	{
		ZENITH_ASSERT_STREQ(s_aszHiZPassNames[uMip], s_aszExpectedBases[uMip],
			"base name for mip %u is unchanged", uMip);

		// Slot 0 is POINTER IDENTITY, not just an equal string: the graph stores
		// the pointer, and the main view must keep handing it the table's own
		// literal exactly as the pre-pool code did.
		ZENITH_ASSERT_TRUE(Flux_ViewPassName(s_aszHiZPassNames[uMip], kuFluxViewSlotMain) == s_aszHiZPassNames[uMip],
			"slot 0 returns the base pointer itself for mip %u", uMip);

		// The preview slot must compose the historical spelling, character for
		// character — this is the typo guard the suffix table alone cannot give.
		const char* const szPreview = Flux_ViewPassName(s_aszHiZPassNames[uMip], kuFluxViewSlotPreview);
		ZENITH_ASSERT_STREQ(szPreview, s_aszExpectedPreview[uMip],
			"preview name for mip %u matches the deleted preview table", uMip);

		// Interning is stable: a second lookup of the same (content, slot) returns
		// the SAME pointer, which is what makes it legal to hand the graph and the
		// GPU timer table a pointer they keep for the life of the process. The two
		// calls are separate statements so nothing may fold them together.
		const char* const szPreviewAgain = Flux_ViewPassName(s_aszHiZPassNames[uMip], kuFluxViewSlotPreview);
		ZENITH_ASSERT_TRUE(szPreviewAgain == szPreview,
			"repeated preview lookups for mip %u return one stable pointer", uMip);
	}
}

// The chain length every view derives from its own dims. Three points pin the
// shape: an exact power of two, a non-square non-power-of-two (the max
// dimension is what counts, not the width), and the clamp.
ZENITH_TEST(HiZ, ComputeMipCountMatchesTheFormula)
{
	// 512² (the preview view's size): floor(log2(512)) + 1 == 10.
	ZENITH_ASSERT_EQ(Flux_HiZImpl::ComputeMipCount(512u, 512u), 10u, "512x512 -> 10 mips");

	// 1920x1080 (the common main-view case): floor(log2(1920)) + 1 == 11. Both
	// dims happen to give 11 here, so it does NOT discriminate max from min —
	// the 2048x8 case below is the one that does.
	ZENITH_ASSERT_EQ(Flux_HiZImpl::ComputeMipCount(1920u, 1080u), 11u, "1920x1080 -> 11 mips");
	ZENITH_ASSERT_EQ(Flux_HiZImpl::ComputeMipCount(1080u, 1920u), 11u, "the formula is symmetric in its two dims");
	// 2048x8: the MAX dimension drives the chain (12 mips), never the min (which
	// would give 4). Each mip clamps to at least 1 texel, which ExecuteHiZMip's
	// max(1u, uWidth >> uMip) relies on.
	ZENITH_ASSERT_EQ(Flux_HiZImpl::ComputeMipCount(2048u, 8u), 12u, "the max dimension drives the chain, not the min");

	// 4096²: floor(log2(4096)) + 1 == 13, clamped to the 12-mip cap.
	ZENITH_ASSERT_EQ(Flux_HiZImpl::ComputeMipCount(4096u, 4096u), Flux_HiZImpl::uHIZ_MAX_MIPS, "4096x4096 clamps to the cap");
	ZENITH_ASSERT_EQ(Flux_HiZImpl::uHIZ_MAX_MIPS, 12u, "the cap is 12");
}
