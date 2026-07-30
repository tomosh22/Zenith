#include "UnitTests/Zenith_UnitTests.h"

#ifdef ZENITH_TESTING

#include "DataStream/Zenith_DataStream.h"
#include "Flux/Skybox/Flux_AtmosphereTransmittance.h"
#include "UnitTests/Zenith_TempScene.h"

ZENITH_TEST(SunComponent, DirectionVectorNormalizesAndRejectsZero)
{
	Zenith_TempScene xScene("SunDirectionVector");
	Zenith_Entity xEntity = xScene.CreateEntity("Sun");
	Zenith_SunComponent& xSun = xEntity.AddComponent<Zenith_SunComponent>();

	xSun.SetDirection(Zenith_Maths::Vector3(-2.0f, -3.0f, -4.0f));
	ZENITH_ASSERT_EQ_FLOAT(Zenith_Maths::Length(xSun.GetWorldDirection()), 1.0f, 0.0001f,
		"authored direction must normalize");

	xSun.SetDirection(Zenith_Maths::Vector3(0.0f));
	ZENITH_ASSERT_NEAR_VEC3(xSun.GetWorldDirection(), Zenith_GetDefaultSunDirection(), 0.0f);
}

ZENITH_TEST(SunComponent, TimeOfDayOrbitHasNamedCardinalPositions)
{
	Zenith_TempScene xScene("SunTimeOfDayCardinals");
	Zenith_SunComponent& xSun = xScene.CreateEntity("Sun").AddComponent<Zenith_SunComponent>();
	xSun.SetOrbitAzimuthDegrees(0.0f);

	xSun.SetTimeOfDayAngleDegrees(0.0f);
	ZENITH_ASSERT_NEAR_VEC3(xSun.GetWorldDirection(), Zenith_Maths::Vector3(-1.0f, 0.0f, 0.0f), 0.0001f);
	xSun.SetTimeOfDayAngleDegrees(90.0f);
	ZENITH_ASSERT_NEAR_VEC3(xSun.GetWorldDirection(), Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f), 0.0001f);
	xSun.SetTimeOfDayAngleDegrees(180.0f);
	ZENITH_ASSERT_NEAR_VEC3(xSun.GetWorldDirection(), Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), 0.0001f);
	xSun.SetTimeOfDayAngleDegrees(270.0f);
	ZENITH_ASSERT_NEAR_VEC3(xSun.GetWorldDirection(), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), 0.0001f);
}

ZENITH_TEST(SunComponent, MidnightProducesZeroDerivedSunKey)
{
	Zenith_TempScene xScene("SunMidnightTransmittance");
	Zenith_SunComponent& xSun = xScene.CreateEntity("Sun").AddComponent<Zenith_SunComponent>();
	xSun.SetTimeOfDayAngleDegrees(270.0f);
	const Zenith_Maths::Vector4 xKey = Flux_AtmosphereTransmittance::ComputeSunColourRadiance(
		xSun.GetWorldDirection(), AtmosphereConfig::fSUN_INTENSITY, 1.0f, 1.0f);
	ZENITH_ASSERT_EQ_FLOAT(xKey.x, 0.0f, 0.0f, "midnight red");
	ZENITH_ASSERT_EQ_FLOAT(xKey.y, 0.0f, 0.0f, "midnight green");
	ZENITH_ASSERT_EQ_FLOAT(xKey.z, 0.0f, 0.0f, "midnight blue");
	ZENITH_ASSERT_EQ_FLOAT(xKey.w, AtmosphereConfig::fSUN_INTENSITY, 0.0f,
		"the one anchor remains packed even when transmittance is zero");
}

ZENITH_TEST(SunComponent, SerializationRoundTripsGeometryOnly)
{
	Zenith_TempScene xScene("SunSerialization");
	Zenith_SunComponent& xSource = xScene.CreateEntity("Source").AddComponent<Zenith_SunComponent>();
	xSource.SetOrbitAzimuthDegrees(37.0f);
	xSource.SetTimeOfDayAngleDegrees(245.0f);

	Zenith_DataStream xStream;
	xSource.WriteToDataStream(xStream);
	xStream.SetCursor(0u);

	Zenith_SunComponent& xDest = xScene.CreateEntity("Dest").AddComponent<Zenith_SunComponent>();
	xDest.ReadFromDataStream(xStream);
	ZENITH_ASSERT_TRUE(xDest.GetDirectionMode() == SUN_DIRECTION_MODE_TIME_OF_DAY);
	ZENITH_ASSERT_EQ_FLOAT(xDest.GetTimeOfDayAngleDegrees(), 245.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xDest.GetOrbitAzimuthDegrees(), 37.0f, 0.0f);
	ZENITH_ASSERT_NEAR_VEC3(xDest.GetWorldDirection(), xSource.GetWorldDirection(), 0.0001f);
}

ZENITH_TEST(SunComponent, ActiveSceneWinsConflictsThenLowestEntityID)
{
	Zenith_TempScene xSceneA("SunConflictA");
	Zenith_Entity xEntityA = xSceneA.CreateEntity("SunA");
	xEntityA.AddComponent<Zenith_SunComponent>().SetDirection(Zenith_Maths::Vector3(-1.0f, -1.0f, 0.0f));

	Zenith_TempScene xSceneB("SunConflictB");
	Zenith_Entity xEntityB = xSceneB.CreateEntity("SunB");
	xEntityB.AddComponent<Zenith_SunComponent>().SetDirection(Zenith_Maths::Vector3(1.0f, -1.0f, 0.0f));
	Zenith_Entity xEntityB2 = xSceneB.CreateEntity("SunB2");
	xEntityB2.AddComponent<Zenith_SunComponent>().SetDirection(Zenith_Maths::Vector3(0.0f, -1.0f, 1.0f));

	Zenith_SunAuthorityData xResolved;
	g_pfnZenithSunAuthorityGather(xResolved);
	ZENITH_ASSERT_TRUE(xResolved.m_bAuthored);
	ZENITH_ASSERT_EQ(xResolved.m_uAuthoredCount, 3u);
	ZENITH_ASSERT_TRUE(xResolved.m_bSourceIsInActiveScene);
	ZENITH_ASSERT_EQ(xResolved.m_uSourceEntityIndex, xEntityB.GetEntityID().m_uIndex);

	g_xEngine.Scenes().SetActiveScene(xSceneA.Scene());
	g_pfnZenithSunAuthorityGather(xResolved);
	ZENITH_ASSERT_EQ(xResolved.m_uSourceEntityIndex, xEntityA.GetEntityID().m_uIndex);
	ZENITH_ASSERT_TRUE(xResolved.m_bSourceIsInActiveScene);
}

#endif // ZENITH_TESTING
