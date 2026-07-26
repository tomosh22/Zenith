//------------------------------------------------------------------------------
// Zenith_AutomatedTest order-control unit tests.
// Included at the bottom of Zenith_AutomatedTest.cpp (the module-owns-its-tests
// pattern - see Zenith_PropertySystem.cpp / Zenith_CommandLine.cpp).
//
// Scope: the PURE helpers behind --automated-tests / --batch-order. The parse
// entry point itself is deliberately NOT exercised here -- ParseCommandLine
// terminates the process on every error path (by design: a mis-specified test
// selection must never fall through into a half-configured run), and it runs
// AFTER this suite anyway, so a test that poked it would either kill the boot
// or have its state overwritten moments later.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

namespace
{
	// Distinct dummy nodes for the order-transform tests. Only identity
	// matters, so the payload pointers stay null.
	struct BatchOrderFixture
	{
		Zenith_AutomatedTestNode        m_axNodes[4] = {};
		const Zenith_AutomatedTestNode* m_apxOrder[4] = {};

		BatchOrderFixture()
		{
			for (int i = 0; i < 4; ++i) m_apxOrder[i] = &m_axNodes[i];
		}

		// Renders the current permutation as "0123" so a failure message
		// shows the actual ordering rather than four pointer values.
		void Describe(char* acOut, int iCount) const
		{
			for (int i = 0; i < iCount; ++i)
			{
				acOut[i] = static_cast<char>('0' + static_cast<int>(m_apxOrder[i] - m_axNodes));
			}
			acOut[iCount] = '\0';
		}
	};

	// SplitCommaList splits IN PLACE, so every case needs a fresh writable
	// copy of the spec. memcpy rather than strcpy: the CRT deprecation is a
	// hard error under this build's warning level.
	void SetSpec(char* acBuf, size_t uSize, const char* szSpec)
	{
		const size_t uLen = std::strlen(szSpec);
		Zenith_Assert(uLen + 1 <= uSize, "test spec does not fit the fixture buffer");
		std::memcpy(acBuf, szSpec, uLen + 1);
	}

	// Helper: apply a spec to the fixture and return the rendered permutation.
	std::string ApplyOrderAndDescribe(const char* szSpec, int iCount)
	{
		BatchOrderFixture xFixture;
		const BatchOrderSpec xSpec = ParseBatchOrderSpec(szSpec);
		ApplyBatchOrder(xFixture.m_apxOrder, iCount, xSpec);
		char acBuf[8] = {};
		xFixture.Describe(acBuf, iCount);
		return std::string(acBuf);
	}
}

// ============================================================================
// ParseBatchOrderSpec
// ============================================================================

ZENITH_TEST(AutomatedTestOrder, BatchOrderAbsentIsIdentity)
{
	// The flag being absent must be the historical --all-automated-tests
	// behaviour, not a special case the callers have to remember.
	const BatchOrderSpec xNull = ParseBatchOrderSpec(nullptr);
	ZENITH_ASSERT_TRUE(xNull.m_bValid, "nullptr spec must be valid");
	ZENITH_ASSERT_TRUE(xNull.m_eKind == BatchOrderKind::Registration, "nullptr spec must be the identity transform");

	const BatchOrderSpec xEmpty = ParseBatchOrderSpec("");
	ZENITH_ASSERT_TRUE(xEmpty.m_bValid, "empty spec must be valid");
	ZENITH_ASSERT_TRUE(xEmpty.m_eKind == BatchOrderKind::Registration, "empty spec must be the identity transform");
}

ZENITH_TEST(AutomatedTestOrder, BatchOrderReverse)
{
	const BatchOrderSpec xSpec = ParseBatchOrderSpec("reverse");
	ZENITH_ASSERT_TRUE(xSpec.m_bValid, "'reverse' must parse");
	ZENITH_ASSERT_TRUE(xSpec.m_eKind == BatchOrderKind::Reverse, "'reverse' must select the reverse transform");
}

ZENITH_TEST(AutomatedTestOrder, BatchOrderRotateParsesValue)
{
	const BatchOrderSpec xZero = ParseBatchOrderSpec("rotate:0");
	ZENITH_ASSERT_TRUE(xZero.m_bValid, "'rotate:0' must parse");
	ZENITH_ASSERT_TRUE(xZero.m_eKind == BatchOrderKind::Rotate, "'rotate:0' must select the rotate transform");
	ZENITH_ASSERT_EQ(xZero.m_iRotate, 0, "rotate value");

	const BatchOrderSpec xThree = ParseBatchOrderSpec("rotate:3");
	ZENITH_ASSERT_TRUE(xThree.m_bValid, "'rotate:3' must parse");
	ZENITH_ASSERT_EQ(xThree.m_iRotate, 3, "rotate value");
}

ZENITH_TEST(AutomatedTestOrder, BatchOrderRejectsMalformedSpecs)
{
	// Negative rotations are rejected outright rather than silently wrapped,
	// and an over-long digit run is rejected before it can overflow the int
	// conversion.
	const char* apszBad[] = { "rotate:-1", "rotate:", "rotate:2x", "rotate:+1", "rotate:1234567890123", "bogus", "Reverse" };
	for (const char* szBad : apszBad)
	{
		const BatchOrderSpec xSpec = ParseBatchOrderSpec(szBad);
		ZENITH_ASSERT_FALSE(xSpec.m_bValid, "spec '%s' must be rejected", szBad);
	}
}

// ============================================================================
// NormalizeRotation
// ============================================================================

ZENITH_TEST(AutomatedTestOrder, NormalizeRotationWrapsModuloSuiteSize)
{
	ZENITH_ASSERT_EQ(NormalizeRotation(0, 5), 0, "rotate:0 is the identity");
	ZENITH_ASSERT_EQ(NormalizeRotation(2, 5), 2, "in-range rotation passes through");
	ZENITH_ASSERT_EQ(NormalizeRotation(5, 5), 0, "a full turn is the identity");
	ZENITH_ASSERT_EQ(NormalizeRotation(7, 5), 2, "rotation wraps modulo the suite size");
	// Degenerate inputs must not divide by zero or produce a bad index.
	ZENITH_ASSERT_EQ(NormalizeRotation(3, 0), 0, "empty suite normalises to 0");
	ZENITH_ASSERT_EQ(NormalizeRotation(-1, 5), 0, "a negative rotation clamps to 0 (the parser rejects it upstream)");
}

// ============================================================================
// ApplyBatchOrder
// ============================================================================

ZENITH_TEST(AutomatedTestOrder, ApplyBatchOrderTransforms)
{
	ZENITH_ASSERT_EQ(ApplyOrderAndDescribe(nullptr,    4), std::string("0123"), "absent spec must not reorder");
	ZENITH_ASSERT_EQ(ApplyOrderAndDescribe("reverse",  4), std::string("3210"), "reverse");
	ZENITH_ASSERT_EQ(ApplyOrderAndDescribe("rotate:0", 4), std::string("0123"), "rotate:0 is registration order");
	ZENITH_ASSERT_EQ(ApplyOrderAndDescribe("rotate:1", 4), std::string("1230"), "rotate left by 1");
	ZENITH_ASSERT_EQ(ApplyOrderAndDescribe("rotate:3", 4), std::string("3012"), "rotate left by 3");
	// A full turn and a wrapped rotation are the same permutation as their
	// normalised counterparts -- this is what makes rotate:<N> safe to pass
	// blindly from a script that doesn't know the suite size.
	ZENITH_ASSERT_EQ(ApplyOrderAndDescribe("rotate:4", 4), std::string("0123"), "rotate by the suite size is the identity");
	ZENITH_ASSERT_EQ(ApplyOrderAndDescribe("rotate:5", 4), std::string("1230"), "rotate:5 == rotate:1 for a 4-test suite");
}

ZENITH_TEST(AutomatedTestOrder, ApplyBatchOrderDegenerateCounts)
{
	// A one-test or empty list must be a no-op for every transform rather
	// than indexing off the end.
	ZENITH_ASSERT_EQ(ApplyOrderAndDescribe("reverse",  1), std::string("0"), "single-entry reverse");
	ZENITH_ASSERT_EQ(ApplyOrderAndDescribe("rotate:1", 1), std::string("0"), "single-entry rotate");
	ZENITH_ASSERT_EQ(ApplyOrderAndDescribe("reverse",  0), std::string(""),  "empty reverse");

	BatchOrderFixture xFixture;
	const BatchOrderSpec xSpec = ParseBatchOrderSpec("reverse");
	ApplyBatchOrder(nullptr, 4, xSpec);   // must not dereference
	ZENITH_ASSERT_TRUE(xFixture.m_apxOrder[0] == &xFixture.m_axNodes[0], "null array must be a no-op");
}

// ============================================================================
// SplitCommaList
// ============================================================================

ZENITH_TEST(AutomatedTestOrder, SplitCommaListBasics)
{
	char acBuf[64];
	const char* apszOut[8] = {};

	SetSpec(acBuf, sizeof(acBuf), "A,B,C");
	int iCount = SplitCommaList(acBuf, apszOut, 8);
	ZENITH_ASSERT_EQ(iCount, 3, "three tokens");
	ZENITH_ASSERT_STREQ(apszOut[0], "A", "first token");
	ZENITH_ASSERT_STREQ(apszOut[1], "B", "second token");
	ZENITH_ASSERT_STREQ(apszOut[2], "C", "third token");

	SetSpec(acBuf, sizeof(acBuf), "Solo");
	iCount = SplitCommaList(acBuf, apszOut, 8);
	ZENITH_ASSERT_EQ(iCount, 1, "a bare name is one token");
	ZENITH_ASSERT_STREQ(apszOut[0], "Solo", "sole token");
}

ZENITH_TEST(AutomatedTestOrder, SplitCommaListTrimsAndDropsEmpties)
{
	char acBuf[64];
	const char* apszOut[8] = {};

	// Spaces around names survive a shell / CI variable expansion; empty
	// tokens come from a trailing comma. Neither should reach the registry
	// lookup as a "name not found".
	SetSpec(acBuf, sizeof(acBuf), "  A , B\t,");
	int iCount = SplitCommaList(acBuf, apszOut, 8);
	ZENITH_ASSERT_EQ(iCount, 2, "trailing comma must not add an empty token");
	ZENITH_ASSERT_STREQ(apszOut[0], "A", "leading/trailing spaces trimmed");
	ZENITH_ASSERT_STREQ(apszOut[1], "B", "tab trimmed");

	SetSpec(acBuf, sizeof(acBuf), "A,,B");
	iCount = SplitCommaList(acBuf, apszOut, 8);
	ZENITH_ASSERT_EQ(iCount, 2, "empty tokens are dropped");

	SetSpec(acBuf, sizeof(acBuf), ",, ,");
	iCount = SplitCommaList(acBuf, apszOut, 8);
	ZENITH_ASSERT_EQ(iCount, 0, "an all-separator list yields no tokens");

	SetSpec(acBuf, sizeof(acBuf), "");
	iCount = SplitCommaList(acBuf, apszOut, 8);
	ZENITH_ASSERT_EQ(iCount, 0, "an empty spec yields no tokens");
}

ZENITH_TEST(AutomatedTestOrder, SplitCommaListPreservesDuplicates)
{
	// "A,A" is a deliberate self-contamination probe: the splitter must NOT
	// dedupe, or the probe silently degrades into a single run.
	char acBuf[64];
	const char* apszOut[8] = {};
	SetSpec(acBuf, sizeof(acBuf), "A,A,B,A");
	const int iCount = SplitCommaList(acBuf, apszOut, 8);
	ZENITH_ASSERT_EQ(iCount, 4, "duplicates must survive the split");
	ZENITH_ASSERT_STREQ(apszOut[0], "A", "occurrence 1");
	ZENITH_ASSERT_STREQ(apszOut[1], "A", "occurrence 2");
	ZENITH_ASSERT_STREQ(apszOut[3], "A", "occurrence 3");
}

ZENITH_TEST(AutomatedTestOrder, SplitCommaListReportsCapacityOverflow)
{
	// Overflow must be a reportable error (-1), never a silent truncation --
	// a truncated list would run a DIFFERENT experiment than the one asked for.
	char acBuf[64];
	const char* apszOut[2] = {};
	SetSpec(acBuf, sizeof(acBuf), "A,B,C");
	ZENITH_ASSERT_EQ(SplitCommaList(acBuf, apszOut, 2), -1, "exceeding the cap must return -1");

	// Degenerate arguments are a no-op rather than a crash.
	ZENITH_ASSERT_EQ(SplitCommaList(nullptr, apszOut, 2), 0, "null buffer");
	ZENITH_ASSERT_EQ(SplitCommaList(acBuf, nullptr, 2), 0, "null output array");
	ZENITH_ASSERT_EQ(SplitCommaList(acBuf, apszOut, 0), 0, "zero capacity");
}

// ============================================================================
// Mode exclusivity + registry lookup
// ============================================================================

ZENITH_TEST(AutomatedTestOrder, CountSelectedModes)
{
	ZENITH_ASSERT_EQ(CountSelectedModes(false, false, false), 0, "no selection flag");
	ZENITH_ASSERT_EQ(CountSelectedModes(true,  false, false), 1, "--automated-test alone");
	ZENITH_ASSERT_EQ(CountSelectedModes(false, true,  false), 1, "--automated-tests alone");
	ZENITH_ASSERT_EQ(CountSelectedModes(false, false, true),  1, "--all-automated-tests alone");
	// Anything above 1 is what ParseCommandLine turns into exit code 2.
	ZENITH_ASSERT_EQ(CountSelectedModes(true,  false, true),  2, "single + all is ambiguous");
	ZENITH_ASSERT_EQ(CountSelectedModes(false, true,  true),  2, "list + all is ambiguous");
	ZENITH_ASSERT_EQ(CountSelectedModes(true,  true,  true),  3, "all three is ambiguous");
}

ZENITH_TEST(AutomatedTestOrder, FindNodeByNameMissesCleanly)
{
	// Read-only against the live registry: a name nobody registers must
	// resolve to null (this is the check that turns a typo'd
	// --automated-tests entry into a reported error rather than a null deref).
	ZENITH_ASSERT_NULL(FindNodeByName("Zenith_NoSuchAutomatedTest_ZZZ"), "unknown name must not resolve");
	ZENITH_ASSERT_NULL(FindNodeByName(nullptr), "null name must not resolve");
}

#endif // ZENITH_TESTING
