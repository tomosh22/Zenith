#include "Core/Zenith_TestFramework.h"
#include "ZenithECS/Zenith_ComponentMeta.h"

//==============================================================================
// Zenith_ComponentMetaRegistry -- serialization-order collision detection.
//
// Serialization order is a hand-assigned GLOBAL namespace: engine built-ins take
// the low numbers, AI the middle, and every game stacks its own components above
// them. Zenith_ComponentMetaRegistry::Finalize() sorts by that key, so two
// components sharing an order are ordered arbitrarily against each other -- and
// since the pre-sort source is a hash-map walk and std::sort is not stable, that
// ordering was not even reproducible between builds. Because serialization order
// decides the byte order components are written in, a collision was a silent,
// nondeterministic scene-output hazard that nothing detected.
//
// These units live ENGINE-SIDE rather than next to the implementation on purpose:
// ZenithECS is an L1 leaf library that may depend only on ZenithBase, and the test
// framework lives in Zenith/Core. Putting a .Tests.inl inside the leaf would break
// both the SentinelECS link proof and the ECS-leaf ratchet. Hosting them here (an
// always-linked engine TU that already names every concrete component) keeps the
// leaf clean and still exercises the leaf's public API.
//==============================================================================

namespace
{
	// A meta carrying only the field under test. Zenith_ComponentMeta is a plain
	// struct of PODs + function pointers, so a synthetic row needs no registration,
	// no component type and no scene -- which is the whole point of testing the
	// collision scan through the pure static rather than through the boot-sealed
	// singleton.
	Zenith_ComponentMeta MakeOrderOnlyMeta(const char* szName, u_int uOrder)
	{
		Zenith_ComponentMeta xMeta;
		xMeta.m_strTypeName = szName;
		xMeta.m_uSerializationOrder = uOrder;
		return xMeta;
	}
}

// Baseline: a correctly-numbered registry reports nothing. Fails if the scan ever
// starts reporting a collision between DIFFERENT orders (an off-by-one in the
// adjacent-pair walk, or comparing the wrong field).
ZENITH_TEST(ECSComponentMeta, DuplicateOrders_UniqueOrdersReportNone)
{
	Zenith_ComponentMeta xA = MakeOrderOnlyMeta("Alpha", 0u);
	Zenith_ComponentMeta xB = MakeOrderOnlyMeta("Bravo", 1u);
	Zenith_ComponentMeta xC = MakeOrderOnlyMeta("Charlie", 100u);

	Zenith_Vector<const Zenith_ComponentMeta*> xSorted;
	xSorted.PushBack(&xA);
	xSorted.PushBack(&xB);
	xSorted.PushBack(&xC);

	ZENITH_ASSERT_EQ(
		Zenith_ComponentMetaRegistry::CountDuplicateSerializationOrders(xSorted), 0u,
		"three distinct serialization orders must report no collision");
}

// THE case the whole change exists for: two components handed the same order.
// Fails if the scan stops comparing adjacent orders at all -- i.e. if the
// detection is removed or short-circuited.
ZENITH_TEST(ECSComponentMeta, DuplicateOrders_OneCollidingPairIsDetected)
{
	Zenith_ComponentMeta xA = MakeOrderOnlyMeta("Alpha", 7u);
	Zenith_ComponentMeta xB = MakeOrderOnlyMeta("Bravo", 7u);

	Zenith_Vector<const Zenith_ComponentMeta*> xSorted;
	xSorted.PushBack(&xA);
	xSorted.PushBack(&xB);

	ZENITH_ASSERT_EQ(
		Zenith_ComponentMetaRegistry::CountDuplicateSerializationOrders(xSorted), 1u,
		"two components sharing serialization order 7 are exactly one colliding pair");
}

// Three-way collision. Pins that the walk keeps counting past the first hit rather
// than returning on it -- a scan that early-outs would report 1 here and silently
// hide the third offender from Finalize()'s per-pair diagnostic.
ZENITH_TEST(ECSComponentMeta, DuplicateOrders_ThreeAtOneOrderCountAsTwoPairs)
{
	Zenith_ComponentMeta xA = MakeOrderOnlyMeta("Alpha", 42u);
	Zenith_ComponentMeta xB = MakeOrderOnlyMeta("Bravo", 42u);
	Zenith_ComponentMeta xC = MakeOrderOnlyMeta("Charlie", 42u);

	Zenith_Vector<const Zenith_ComponentMeta*> xSorted;
	xSorted.PushBack(&xA);
	xSorted.PushBack(&xB);
	xSorted.PushBack(&xC);

	ZENITH_ASSERT_EQ(
		Zenith_ComponentMetaRegistry::CountDuplicateSerializationOrders(xSorted), 2u,
		"three components on one order are two ADJACENT colliding pairs");
}

// Two separate collisions with clean orders between them. Fails if the walk stops
// at the first collision, or if it treats the whole list as one run.
ZENITH_TEST(ECSComponentMeta, DuplicateOrders_SeparateCollisionsAreCountedIndependently)
{
	Zenith_ComponentMeta xA = MakeOrderOnlyMeta("Alpha", 1u);
	Zenith_ComponentMeta xB = MakeOrderOnlyMeta("Bravo", 1u);
	Zenith_ComponentMeta xC = MakeOrderOnlyMeta("Charlie", 2u);
	Zenith_ComponentMeta xD = MakeOrderOnlyMeta("Delta", 9u);
	Zenith_ComponentMeta xE = MakeOrderOnlyMeta("Echo", 9u);

	Zenith_Vector<const Zenith_ComponentMeta*> xSorted;
	xSorted.PushBack(&xA);
	xSorted.PushBack(&xB);
	xSorted.PushBack(&xC);
	xSorted.PushBack(&xD);
	xSorted.PushBack(&xE);

	ZENITH_ASSERT_EQ(
		Zenith_ComponentMetaRegistry::CountDuplicateSerializationOrders(xSorted), 2u,
		"collisions at order 1 and at order 9 are two independent pairs");
}

// Boundaries. An empty or single-entry list has no ADJACENT pair to compare, so the
// walk must not index off the front -- the loop starts at 1 for exactly this reason.
ZENITH_TEST(ECSComponentMeta, DuplicateOrders_EmptyAndSingleAreBoundarySafe)
{
	Zenith_Vector<const Zenith_ComponentMeta*> xEmpty;
	ZENITH_ASSERT_EQ(
		Zenith_ComponentMetaRegistry::CountDuplicateSerializationOrders(xEmpty), 0u,
		"an empty registry has no pair to collide");

	Zenith_ComponentMeta xOnly = MakeOrderOnlyMeta("Solo", 5u);
	Zenith_Vector<const Zenith_ComponentMeta*> xSingle;
	xSingle.PushBack(&xOnly);
	ZENITH_ASSERT_EQ(
		Zenith_ComponentMetaRegistry::CountDuplicateSerializationOrders(xSingle), 0u,
		"a one-component registry has no pair to collide");
}

// ★ THE LOAD-BEARING ONE. Everything above tests the scan; this tests the ACTUAL
// REGISTRY THIS BUILD SHIPS. It is what turns a future copy-pasted serialization
// order into a boot-time unit failure in every game, which is the entire point of
// the change -- the pure units would all still pass with a real collision present.
//
// Guarded against vacuity first: an unfinalized or empty registry would make the
// collision count trivially 0, so both preconditions are asserted before the
// property. (Zenith_Engine::Initialise calls EnsureInitialized well before
// RunAllTests, so the registry is genuinely sealed by the time this runs.)
ZENITH_TEST(ECSComponentMeta, DuplicateOrders_LiveRegistryHasNoCollisions)
{
	const Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();

	ZENITH_ASSERT_TRUE(xRegistry.IsInitialized(),
		"the component registry must be sealed before this unit runs, or the "
		"collision check below would pass over an empty list");

	const Zenith_Vector<const Zenith_ComponentMeta*>& xSorted = xRegistry.GetAllMetasSorted();
	ZENITH_ASSERT_TRUE(xSorted.GetSize() > 1u,
		"the live registry must hold at least two components, or a pairwise "
		"collision check proves nothing");

	ZENITH_ASSERT_EQ(
		Zenith_ComponentMetaRegistry::CountDuplicateSerializationOrders(xSorted), 0u,
		"two registered components share a serialization order -- their relative "
		"serialization order is arbitrary. See the [ComponentMetaRegistry] DUPLICATE "
		"errors logged at boot for the offending pair, and renumber one of them.");
}
