//------------------------------------------------------------------------------
// Flux_Decals unit tests.
// Included at the bottom of Flux_Decals.cpp (the module-owns-its-tests pattern).
//
// Focus: Reset() as a WORLD-state clear. The decal ring holds gameplay-spawned
// world marks, so it belongs in the SCENE_LOAD_SINGLE render-systems reset
// alongside Terrain/Text/Particles/Skybox/Fog/Grass; it was missing from that
// list and its only clear was a ZENITH_TESTING-only helper with no production
// caller. These pin the clear itself and its idempotence -- the render-systems
// reset now fires twice per harness world reset (once up front, once inside the
// following SINGLE load), so a non-idempotent body would be a live defect.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

ZENITH_TEST(FluxDecals, ResetClearsTheGameplayRing)
{
	Flux_DecalsImpl& xDecals = g_xEngine.Decals();
	xDecals.Reset();

	xDecals.SpawnDecal(Zenith_Maths::Vector3(10.0f, 0.0f, 10.0f),
		Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), nullptr, 0.5f, 10.0f);
	xDecals.SpawnDecal(Zenith_Maths::Vector3(20.0f, 0.0f, 20.0f),
		Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), nullptr, 0.5f, 10.0f);
	ZENITH_ASSERT_TRUE(xDecals.GetSlotForTest(0).m_bActive, "first spawn occupies slot 0");
	ZENITH_ASSERT_TRUE(xDecals.GetSlotForTest(1).m_bActive, "second spawn occupies slot 1");
	ZENITH_ASSERT_EQ(xDecals.TickAndPackDense(0.016f), 2u, "both decals pack");

	xDecals.Reset();

	ZENITH_ASSERT_FALSE(xDecals.GetSlotForTest(0).m_bActive, "Reset must deactivate slot 0");
	ZENITH_ASSERT_FALSE(xDecals.GetSlotForTest(1).m_bActive, "Reset must deactivate slot 1");
	ZENITH_ASSERT_EQ(xDecals.GetActiveCountForTest(), 0u, "Reset must zero the active count");
	ZENITH_ASSERT_EQ(xDecals.TickAndPackDense(0.016f), 0u, "nothing packs after a reset");
}

ZENITH_TEST(FluxDecals, ResetRewindsTheRingCursor)
{
	// The cursor must rewind too, not just the active flags: a stale cursor
	// would make the next world's first decal land mid-ring, so the ring would
	// wrap early and drop marks the player just made.
	Flux_DecalsImpl& xDecals = g_xEngine.Decals();
	xDecals.Reset();

	xDecals.SpawnDecal(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f),
		Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), nullptr, 0.5f, 10.0f);
	xDecals.SpawnDecal(Zenith_Maths::Vector3(2.0f, 0.0f, 2.0f),
		Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), nullptr, 0.5f, 10.0f);
	xDecals.Reset();

	xDecals.SpawnDecal(Zenith_Maths::Vector3(3.0f, 0.0f, 3.0f),
		Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), nullptr, 0.5f, 10.0f);
	ZENITH_ASSERT_TRUE(xDecals.GetSlotForTest(0).m_bActive, "post-reset spawn must reuse slot 0");
	ZENITH_ASSERT_EQ_FLOAT(xDecals.GetSlotForTest(0).m_xPosition.x, 3.0f, 0.001f,
		"slot 0 must hold the NEW decal, not the pre-reset one");
	ZENITH_ASSERT_FALSE(xDecals.GetSlotForTest(1).m_bActive, "slot 1 must stay clear");

	xDecals.Reset();
}

ZENITH_TEST(FluxDecals, ResetIsIdempotent)
{
	// The harness world reset fires the render-systems reset, then the boot-scene
	// SINGLE load fires it AGAIN. A second Reset on an already-reset system must
	// be a no-op, not (say) an underflowing count.
	Flux_DecalsImpl& xDecals = g_xEngine.Decals();
	xDecals.SpawnDecal(Zenith_Maths::Vector3(5.0f, 0.0f, 5.0f),
		Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), nullptr, 0.5f, 10.0f);

	xDecals.Reset();
	const u_int uAfterFirst = xDecals.GetActiveCountForTest();
	xDecals.Reset();
	const u_int uAfterSecond = xDecals.GetActiveCountForTest();

	ZENITH_ASSERT_EQ(uAfterFirst, 0u, "first reset clears");
	ZENITH_ASSERT_EQ(uAfterSecond, uAfterFirst, "a second reset changes nothing");
	ZENITH_ASSERT_EQ(xDecals.TickAndPackDense(0.016f), 0u, "and the system still packs nothing");
}

ZENITH_TEST(FluxDecals, ResetDisarmsTheEditorIndicatorSlot)
{
	// The editor brush indicator is deliberately ONE-frame state (re-armed every
	// frame the cursor is valid), so clearing it costs nothing -- while leaving
	// it armed would stamp an indicator for the world that just went away.
	Flux_DecalsImpl& xDecals = g_xEngine.Decals();
	xDecals.Reset();

	xDecals.SetEditorDecal(Zenith_Maths::Vector3(100.0f, 50.0f, 100.0f),
		30.0f, 60.0f, Zenith_Maths::Vector4(1.0f, 0.0f, 0.0f, 0.85f), nullptr);
	xDecals.Reset();
	ZENITH_ASSERT_EQ(xDecals.TickAndPackDense(0.016f), 0u,
		"a reset between arm and pack must drop the indicator");

	xDecals.Reset();
}

#endif // ZENITH_TESTING
