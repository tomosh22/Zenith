#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/Flux_RefcountDiffRegistry.h"

// ============================================================================
// Flux_RefcountDiffRegistry<Key, Payload> unit tests — the ONE refcount-diff sync
// registry the bucket / mesh-geometry / skinned-id / skinned-pose registries all
// instantiate. Pure CPU; a synthetic key + an int payload + a mock provider exercise
// the create/dedup/retire/recycle/committed/topology/build-fail/iteration contract
// that every instantiation inherits. (The domain registries keep their own focused
// tests on top of this.)
// ============================================================================

// Synthetic test key (global scope — a Zenith_Hash specialisation can't live in an
// anonymous namespace). Distinct name so it never collides with a real key.
struct Flux_RefcountTestKey
{
	u_int m_u = 0u;
	bool operator==(const Flux_RefcountTestKey& xOther) const { return m_u == xOther.m_u; }
};

template<>
struct Zenith_Hash<Flux_RefcountTestKey>
{
	u_int64 operator()(const Flux_RefcountTestKey& xKey) const noexcept
	{
		return (static_cast<u_int64>(xKey.m_u) ^ 0xcbf29ce484222325ull) * 0x100000001b3ull;
	}
};

namespace
{
	using TestRegistry = Flux_RefcountDiffRegistry<Flux_RefcountTestKey, int>;

	int g_iRDRBuilds        = 0;
	int g_iRDRDestroys      = 0;
	int g_iRDRLastDestroyed = -1;

	void RDR_ResetCounters()
	{
		g_iRDRBuilds        = 0;
		g_iRDRDestroys      = 0;
		g_iRDRLastDestroyed = -1;
	}

	bool RDR_MockBuild(const Flux_RefcountTestKey& xKey, int& iOut)
	{
		++g_iRDRBuilds;
		iOut = static_cast<int>(xKey.m_u) * 10 + 1;   // non-zero, identity-derived
		return true;
	}
	void RDR_MockDestroy(int& iPayload)
	{
		++g_iRDRDestroys;
		g_iRDRLastDestroyed = iPayload;
		iPayload = -1;
	}
	bool RDR_FailBuild(const Flux_RefcountTestKey&, int& iOut)
	{
		++g_iRDRBuilds;
		iOut = 0;
		return false;   // build failure => invalid sentinel, no entry
	}

	TestRegistry::Provider RDR_MockProvider()
	{
		TestRegistry::Provider x;
		x.m_pfnBuild   = &RDR_MockBuild;
		x.m_pfnDestroy = &RDR_MockDestroy;
		return x;
	}

	Flux_RefcountTestKey RDRK(u_int u) { Flux_RefcountTestKey k; k.m_u = u; return k; }
}

ZENITH_TEST(RefcountDiffRegistry, IdOnlyModeDedupAndCommittedRefcount)
{
	// No provider -> id-only mode (the bucket / skinned-id shape).
	TestRegistry xReg;
	RDR_ResetCounters();

	xReg.BeginSync();
	const u_int uA  = xReg.Reference(RDRK(1));
	const u_int uB  = xReg.Reference(RDRK(2));
	const u_int uA2 = xReg.Reference(RDRK(1));   // same key again
	xReg.EndSync();

	ZENITH_ASSERT_EQ(uA, uA2, "same key -> same stable slot");
	ZENITH_ASSERT_NE(uA, uB, "distinct keys -> distinct slots");
	ZENITH_ASSERT_EQ(xReg.GetLiveCount(), 2u, "two distinct keys -> two live entries");
	ZENITH_ASSERT_EQ(g_iRDRBuilds, 0, "id-only mode never invokes a build");
	ZENITH_ASSERT_EQ(xReg.GetCommittedRefcount(RDRK(1)), 2u, "committed refcount counts referencing items");
	ZENITH_ASSERT_EQ(xReg.GetCommittedRefcount(RDRK(2)), 1u, "single-reference key commits 1");
	ZENITH_ASSERT_TRUE(xReg.HasKey(RDRK(1)), "referenced key is present");
}

ZENITH_TEST(RefcountDiffRegistry, TopologyFlagsCreateRetireNotSteadyState)
{
	TestRegistry xReg;

	// Frame 1: first reference -> create -> topology changed.
	xReg.BeginSync();
	xReg.Reference(RDRK(1));
	xReg.EndSync();
	ZENITH_ASSERT_TRUE(xReg.WasTopologyChangedThisSync(), "first create flags a topology change");
	ZENITH_ASSERT_FALSE(xReg.WasAnyRetiredThisSync(), "a create is not a retire");

	// Frame 2: identical re-sync -> NO topology change (count is data, not topology).
	xReg.BeginSync();
	xReg.Reference(RDRK(1));
	xReg.EndSync();
	ZENITH_ASSERT_FALSE(xReg.WasTopologyChangedThisSync(), "identical re-sync is not a topology change");

	// Frame 3: drop the key -> retire -> topology changed + any-retired.
	xReg.BeginSync();
	xReg.EndSync();
	ZENITH_ASSERT_TRUE(xReg.WasTopologyChangedThisSync(), "retire flags a topology change");
	ZENITH_ASSERT_TRUE(xReg.WasAnyRetiredThisSync(), "retire sets the any-retired signal");
	ZENITH_ASSERT_EQ(xReg.GetLiveCount(), 0u, "the dropped key's entry is retired");
}

ZENITH_TEST(RefcountDiffRegistry, RetiredSlotIsRecycledWithoutHighWaterGrowth)
{
	TestRegistry xReg;

	// Frame 1: A + B -> high-water 2.
	xReg.BeginSync();
	xReg.Reference(RDRK(1));
	const u_int uB = xReg.Reference(RDRK(2));
	xReg.EndSync();
	ZENITH_ASSERT_EQ(xReg.GetHighWaterSlots(), 2u, "two keys -> high-water 2");

	// Frame 2: drop B.
	xReg.BeginSync();
	xReg.Reference(RDRK(1));
	xReg.EndSync();

	// Frame 3: new key C recycles B's freed slot; high-water does not grow.
	xReg.BeginSync();
	xReg.Reference(RDRK(1));
	const u_int uC = xReg.Reference(RDRK(3));
	xReg.EndSync();

	ZENITH_ASSERT_EQ(uC, uB, "a new key recycles a retired slot");
	ZENITH_ASSERT_EQ(xReg.GetHighWaterSlots(), 2u, "recycled slot -> high-water unchanged");
}

ZENITH_TEST(RefcountDiffRegistry, ProviderBuildsOnceAndDestroysOnLastReference)
{
	TestRegistry xReg;
	xReg.SetProvider(RDR_MockProvider());
	RDR_ResetCounters();

	// Frame 1: A + B built once each; repeated A reference does not rebuild.
	xReg.BeginSync();
	const u_int uA = xReg.Reference(RDRK(1));
	xReg.Reference(RDRK(1));
	const u_int uB = xReg.Reference(RDRK(2));
	const int iBuiltB = *xReg.TryGetPayload(uB);
	xReg.EndSync();
	ZENITH_ASSERT_EQ(g_iRDRBuilds, 2, "each distinct key builds exactly once");
	ZENITH_ASSERT_EQ(*xReg.TryGetPayload(uA), 11, "payload is the provider-built value (1*10+1)");

	// Frame 2: only A referenced -> B torn down with its built payload.
	xReg.BeginSync();
	xReg.Reference(RDRK(1));
	xReg.EndSync();
	ZENITH_ASSERT_EQ(g_iRDRDestroys, 1, "the unreferenced key's payload is destroyed");
	ZENITH_ASSERT_EQ(g_iRDRLastDestroyed, iBuiltB, "the correct (B's) payload is destroyed");
	ZENITH_ASSERT_EQ(xReg.GetLiveCount(), 1u, "only A remains live");
}

ZENITH_TEST(RefcountDiffRegistry, RebuildsAfterFullRetireOfSameKey)
{
	// Reference -> retire -> Reference the SAME key must REBUILD (retire fully clears the key from the
	// map, so the re-reference is a cache MISS), not resurrect a stale entry, and recycle the freed slot.
	TestRegistry xReg;
	xReg.SetProvider(RDR_MockProvider());
	RDR_ResetCounters();

	xReg.BeginSync();
	const u_int uId1 = xReg.Reference(RDRK(1));
	xReg.EndSync();
	ZENITH_ASSERT_EQ(g_iRDRBuilds, 1, "K1 built once");

	// K1 unreferenced this sync -> retired (payload destroyed, key removed from the map).
	xReg.BeginSync();
	xReg.EndSync();
	ZENITH_ASSERT_EQ(g_iRDRDestroys, 1, "K1's payload destroyed when its last reference dropped");
	ZENITH_ASSERT_FALSE(xReg.HasKey(RDRK(1)), "a fully-retired key is cleared from the registry");
	ZENITH_ASSERT_EQ(xReg.GetLiveCount(), 0u, "no live entries after retire");

	// Re-reference K1 -> rebuild + recycle the freed slot.
	xReg.BeginSync();
	const u_int uId3 = xReg.Reference(RDRK(1));
	xReg.EndSync();
	ZENITH_ASSERT_EQ(g_iRDRBuilds, 2, "re-referencing a fully-retired key rebuilds it (cache miss)");
	ZENITH_ASSERT_EQ(uId3, uId1, "the rebuilt key recycles the retired slot id");
	ZENITH_ASSERT_EQ(xReg.GetLiveCount(), 1u, "K1 live again");
}

ZENITH_TEST(RefcountDiffRegistry, BuildFailureYieldsInvalidSentinelAndNoEntry)
{
	TestRegistry xReg;
	TestRegistry::Provider xFailing;
	xFailing.m_pfnBuild   = &RDR_FailBuild;
	xFailing.m_pfnDestroy = &RDR_MockDestroy;
	xReg.SetProvider(xFailing);
	RDR_ResetCounters();

	xReg.BeginSync();
	const u_int uId = xReg.Reference(RDRK(1));
	xReg.EndSync();

	ZENITH_ASSERT_EQ(uId, uFLUX_REFCOUNT_REGISTRY_INVALID_SLOT, "a failed build returns the invalid sentinel");
	ZENITH_ASSERT_EQ(xReg.GetLiveCount(), 0u, "a failed build registers no live entry");
	ZENITH_ASSERT_FALSE(xReg.HasKey(RDRK(1)), "a failed build leaves no key");
}

ZENITH_TEST(RefcountDiffRegistry, IterationExposesLiveKeysAndSkipsRetiredSlots)
{
	TestRegistry xReg;

	xReg.BeginSync();
	const u_int uA = xReg.Reference(RDRK(7));
	const u_int uB = xReg.Reference(RDRK(8));
	xReg.EndSync();

	ZENITH_ASSERT_EQ(xReg.GetSlotCount(), 2u, "slot count covers both allocated slots");
	ZENITH_ASSERT_TRUE(xReg.IsSlotAlive(uA), "slot A is alive");
	ZENITH_ASSERT_NOT_NULL(xReg.TryGetKey(uA), "live slot yields its key");
	ZENITH_ASSERT_EQ(xReg.TryGetKey(uA)->m_u, 7u, "the iterated key is A's");

	// Drop B -> its slot is retired; iteration must return nullptr for it.
	xReg.BeginSync();
	xReg.Reference(RDRK(7));
	xReg.EndSync();
	ZENITH_ASSERT_FALSE(xReg.IsSlotAlive(uB), "retired slot is not alive");
	ZENITH_ASSERT_NULL(xReg.TryGetKey(uB), "retired slot yields no key");
}
