#include "UnitTests/Zenith_UnitTests.h"
#include "Core/Zenith_CommandLine.h"
#include "DataStream/Zenith_DataStream.h"
#include "ZenithECS/Zenith_Entity.h"
#include "RenderTest/RenderTest_Guns.h"
#include "RenderTest/Components/RenderTest_GunComponent.h"
#include "RenderTest/Components/RenderTest_TennisPlayerComponent.h"
#include "RenderTest/Components/RenderTest_BootstrapComponent.h"

// Coverage for the testbed per-entity config serialization (WS4 / U3) and the
// headless grass-apply result (W1). The components (de)serialize without touching
// their owning entity, so they round-trip with an invalid dummy handle — no scene.

ZENITH_TEST(RenderTestTestbed, GunTypeAmmoRoundTrip)
{
	Zenith_Entity xDummy;
	RenderTest_GunComponent xSrc(xDummy);
	xSrc.Init(RenderTest_Guns::GetSpec(RenderTest_Guns::GunType::Rifle));
	xSrc.SetAmmo(3, 11);

	Zenith_DataStream xStream;
	xSrc.WriteToDataStream(xStream);
	xStream.SetCursor(0);

	RenderTest_GunComponent xDst(xDummy);
	xDst.ReadFromDataStream(xStream);

	ZENITH_ASSERT_TRUE(xDst.GetType() == RenderTest_Guns::GunType::Rifle, "gun type must round-trip");
	ZENITH_ASSERT_EQ(xDst.GetAmmoInClip(), 3u, "clip ammo must round-trip");
	ZENITH_ASSERT_EQ(xDst.GetReserve(), 11u, "reserve ammo must round-trip");
}

ZENITH_TEST(RenderTestTestbed, GunCorruptTypeDefaults)
{
	// Hand-craft a v2 payload with an out-of-range type -> must clamp to the default.
	Zenith_DataStream xStream;
	const u_int uVersion = 2;
	xStream << uVersion;
	const uint32_t uBadType = 999;
	xStream << uBadType;
	const uint32_t uClip = 5;
	const uint32_t uReserve = 7;
	xStream << uClip;
	xStream << uReserve;
	xStream.SetCursor(0);

	Zenith_Entity xDummy;
	RenderTest_GunComponent xGun(xDummy);
	xGun.ReadFromDataStream(xStream);

	ZENITH_ASSERT_TRUE(xGun.GetType() == RenderTest_Guns::GunType::Pistol,
		"out-of-range gun type must fall back to the default (Pistol)");
	// Both ammo fields still apply after the spec rebuild (Init resets them to the
	// spec defaults, then SetAmmo restores the serialized values — order-dependent).
	ZENITH_ASSERT_EQ(xGun.GetAmmoInClip(), 5u, "clip ammo applies after default-spec rebuild");
	ZENITH_ASSERT_EQ(xGun.GetReserve(), 7u, "reserve ammo applies after default-spec rebuild");
}

ZENITH_TEST(RenderTestTestbed, GunV1LoadsAsDefault)
{
	// A v1 payload (version tag only) must read back as the default spec, not crash.
	Zenith_DataStream xStream;
	const u_int uVersion = 1;
	xStream << uVersion;
	xStream.SetCursor(0);

	Zenith_Entity xDummy;
	RenderTest_GunComponent xGun(xDummy);
	xGun.ReadFromDataStream(xStream);
	ZENITH_ASSERT_TRUE(xGun.GetType() == RenderTest_Guns::GunType::Pistol, "v1 -> default pistol");
}

ZENITH_TEST(RenderTestTestbed, TennisSideRoundTrip)
{
	Zenith_Entity xDummy;

	// Far side.
	{
		RenderTest_TennisPlayerComponent xSrc(xDummy);
		xSrc.Init(false);
		Zenith_DataStream xStream;
		xSrc.WriteToDataStream(xStream);
		xStream.SetCursor(0);
		RenderTest_TennisPlayerComponent xDst(xDummy);
		xDst.ReadFromDataStream(xStream);
		ZENITH_ASSERT_FALSE(xDst.IsNearSide(), "far-side NPC must round-trip");
	}
	// Near side.
	{
		RenderTest_TennisPlayerComponent xSrc(xDummy);
		xSrc.Init(true);
		Zenith_DataStream xStream;
		xSrc.WriteToDataStream(xStream);
		xStream.SetCursor(0);
		RenderTest_TennisPlayerComponent xDst(xDummy);
		xDst.ReadFromDataStream(xStream);
		ZENITH_ASSERT_TRUE(xDst.IsNearSide(), "near-side NPC must round-trip");
	}
}

ZENITH_TEST(RenderTestTestbed, GrassApplyHeadlessSkips)
{
	// Headless: Grass owns GPU resources, so the apply must report SkippedHeadless
	// (the bootstrap treats that as done, no warn/retry). Windowed runs exercise the
	// other results via the live terrain, so this is headless-only.
	if (!Zenith_CommandLine::IsHeadless())
	{
		ZENITH_SKIP("grass-apply result test is headless-only");
	}
	ZENITH_ASSERT_TRUE(RenderTest_TryApplyGrassDensityFromDisk() == RenderTest_GrassApplyResult::SkippedHeadless,
		"headless grass apply must report SkippedHeadless");
}
