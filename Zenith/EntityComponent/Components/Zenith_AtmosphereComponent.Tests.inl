#include "UnitTests/Zenith_UnitTests.h"

#ifdef ZENITH_TESTING

#include "EntityComponent/Components/Zenith_AtmosphereComponent.h"
#include "DataStream/Zenith_DataStream.h"
#include "UnitTests/Zenith_TempScene.h"

// ============================================================================
// Zenith_AtmosphereComponent unit tests. The component owns ONLY the
// scene-authored physical atmosphere-medium parameters (Rayleigh/Mie density
// scales + the Henyey-Greenstein phase asymmetry). It must NOT expose the
// engine's radiometric anchor, exposure, sample counts or any renderer state.
// Hosted by Zenith_AtmosphereComponent.cpp (always-linked registrar TU) so
// MSVC /OPT:REF cannot dead-strip the static-init test registrations.
// ============================================================================

ZENITH_TEST(AtmosphereComponent, DefaultsMatchPhysicalEarthMedium)
{
	Zenith_TempScene xScene("AtmosphereDefaults");
	Zenith_AtmosphereComponent& xA = xScene.CreateEntity("Atmos").AddComponent<Zenith_AtmosphereComponent>();
	ZENITH_ASSERT_EQ_FLOAT(xA.GetRayleighScale(), Zenith_GetDefaultAtmosphereRayleighScale(), 0.0f, "default Rayleigh scale");
	ZENITH_ASSERT_EQ_FLOAT(xA.GetMieScale(), Zenith_GetDefaultAtmosphereMieScale(), 0.0f, "default Mie scale");
	ZENITH_ASSERT_EQ_FLOAT(xA.GetMieG(), Zenith_GetDefaultAtmosphereMieG(), 0.0f, "default Mie-G");
}

ZENITH_TEST(AtmosphereComponent, SettersClampNonPhysicalValues)
{
	Zenith_TempScene xScene("AtmosphereClamp");
	Zenith_AtmosphereComponent& xA = xScene.CreateEntity("Atmos").AddComponent<Zenith_AtmosphereComponent>();

	xA.SetRayleighScale(-1.0f);
	ZENITH_ASSERT_EQ_FLOAT(xA.GetRayleighScale(), 0.0f, 0.0f, "negative Rayleigh clamps to 0");
	xA.SetMieScale(-5.0f);
	ZENITH_ASSERT_EQ_FLOAT(xA.GetMieScale(), 0.0f, 0.0f, "negative Mie clamps to 0");

	xA.SetMieG(1.5f);
	ZENITH_ASSERT_EQ_FLOAT(xA.GetMieG(), 0.99f, 0.0f, "Mie-G clamps to 0.99");
	xA.SetMieG(-0.5f);
	ZENITH_ASSERT_EQ_FLOAT(xA.GetMieG(), 0.0f, 0.0f, "Mie-G clamps to 0");

	xA.SetRayleighScale(2.5f);
	ZENITH_ASSERT_EQ_FLOAT(xA.GetRayleighScale(), 2.5f, 0.0f, "valid Rayleigh stored");
}

ZENITH_TEST(AtmosphereComponent, SerializationRoundTripsMediumScales)
{
	Zenith_TempScene xScene("AtmosphereSerial");
	Zenith_AtmosphereComponent& xSource = xScene.CreateEntity("Src").AddComponent<Zenith_AtmosphereComponent>();
	xSource.SetRayleighScale(1.75f);
	xSource.SetMieScale(0.4f);
	xSource.SetMieG(0.82f);

	Zenith_DataStream xStream;
	xSource.WriteToDataStream(xStream);
	xStream.SetCursor(0u);

	Zenith_AtmosphereComponent& xDest = xScene.CreateEntity("Dst").AddComponent<Zenith_AtmosphereComponent>();
	xDest.ReadFromDataStream(xStream);
	ZENITH_ASSERT_EQ_FLOAT(xDest.GetRayleighScale(), 1.75f, 0.0f, "Rayleigh round-trips");
	ZENITH_ASSERT_EQ_FLOAT(xDest.GetMieScale(), 0.4f, 0.0f, "Mie round-trips");
	ZENITH_ASSERT_EQ_FLOAT(xDest.GetMieG(), 0.82f, 0.0f, "Mie-G round-trips");
}

ZENITH_TEST(AtmosphereComponent, V2ParametersDefaultAndClamp)
{
	Zenith_TempScene xScene("AtmosphereV2Clamp");
	Zenith_AtmosphereComponent& xA = xScene.CreateEntity("Atmos").AddComponent<Zenith_AtmosphereComponent>();

	ZENITH_ASSERT_EQ_FLOAT(xA.GetRayleighScaleHeight(), Zenith_GetDefaultAtmosphereRayleighScaleHeight(), 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xA.GetMieScaleHeight(), Zenith_GetDefaultAtmosphereMieScaleHeight(), 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xA.GetGroundAlbedo(), Zenith_GetDefaultAtmosphereGroundAlbedo(), 0.0f);
	ZENITH_ASSERT_FALSE(xA.IsLocalBlendVolume(), "a fresh atmosphere is GLOBAL -- the back-compatible default");

	// A zero scale height makes the density integral degenerate, so it clamps.
	xA.SetRayleighScaleHeight(0.0f);
	ZENITH_ASSERT_GT(xA.GetRayleighScaleHeight(), 0.0f, "scale height never reaches zero");
	xA.SetMieScaleHeight(-100.0f);
	ZENITH_ASSERT_GT(xA.GetMieScaleHeight(), 0.0f, "negative scale height clamps positive");

	// A Lambertian albedo above 1 emits more than it receives.
	xA.SetGroundAlbedo(3.0f);
	ZENITH_ASSERT_EQ_FLOAT(xA.GetGroundAlbedo(), 1.0f, 0.0f, "albedo clamps to 1");
	xA.SetGroundAlbedo(-1.0f);
	ZENITH_ASSERT_EQ_FLOAT(xA.GetGroundAlbedo(), 0.0f, 0.0f, "albedo clamps to 0");

	// The falloff band can never swallow the whole sphere -- that would make the
	// volume weightless everywhere and look like it had simply stopped working.
	xA.SetBlendRadius(100.0f);
	xA.SetBlendFalloff(500.0f);
	ZENITH_ASSERT_LT(xA.GetBlendFalloff(), xA.GetBlendRadius(), "falloff is clamped inside the radius");
	// Shrinking the radius under an existing falloff re-clamps it, not the reverse.
	xA.SetBlendFalloff(40.0f);
	xA.SetBlendRadius(10.0f);
	ZENITH_ASSERT_LT(xA.GetBlendFalloff(), xA.GetBlendRadius(), "shrinking the radius re-clamps the band");
}

ZENITH_TEST(AtmosphereComponent, SerializationRoundTripsV2Parameters)
{
	Zenith_TempScene xScene("AtmosphereSerialV2");
	Zenith_AtmosphereComponent& xSource = xScene.CreateEntity("Src").AddComponent<Zenith_AtmosphereComponent>();
	xSource.SetRayleighScale(1.75f);
	xSource.SetMieScale(0.4f);
	xSource.SetMieG(0.82f);
	xSource.SetRayleighScaleHeight(9500.0f);
	xSource.SetMieScaleHeight(650.0f);
	xSource.SetGroundAlbedo(0.62f);
	xSource.SetBlendRadius(250.0f);
	xSource.SetBlendFalloff(40.0f);
	xSource.SetBlendPriority(3.5f);

	Zenith_DataStream xStream;
	xSource.WriteToDataStream(xStream);
	xStream.SetCursor(0u);

	Zenith_AtmosphereComponent& xDest = xScene.CreateEntity("Dst").AddComponent<Zenith_AtmosphereComponent>();
	xDest.ReadFromDataStream(xStream);
	ZENITH_ASSERT_EQ_FLOAT(xDest.GetRayleighScaleHeight(), 9500.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xDest.GetMieScaleHeight(), 650.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xDest.GetGroundAlbedo(), 0.62f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xDest.GetBlendRadius(), 250.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xDest.GetBlendFalloff(), 40.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xDest.GetBlendPriority(), 3.5f, 0.0f);
	ZENITH_ASSERT_TRUE(xDest.IsLocalBlendVolume());
}

ZENITH_TEST(AtmosphereComponent, V1StreamsLoadWithPhysicalDefaults)
{
	// Committed scenes hold v1 blobs. They must load without a re-save, and the
	// fields v1 never had must take the values that reproduce v1 behaviour
	// exactly -- including radius 0, which keeps the entity a GLOBAL atmosphere.
	Zenith_DataStream xStream;
	const u_int uV1 = 1u;
	const float fRayleigh = 2.25f;
	const float fMie = 0.6f;
	const float fMieG = 0.71f;
	xStream << uV1;
	xStream << fRayleigh;
	xStream << fMie;
	xStream << fMieG;
	xStream.SetCursor(0u);

	Zenith_TempScene xScene("AtmosphereV1Load");
	Zenith_AtmosphereComponent& xDest = xScene.CreateEntity("Dst").AddComponent<Zenith_AtmosphereComponent>();
	xDest.ReadFromDataStream(xStream);

	ZENITH_ASSERT_EQ_FLOAT(xDest.GetRayleighScale(), fRayleigh, 0.0f, "v1 payload still reads");
	ZENITH_ASSERT_EQ_FLOAT(xDest.GetMieG(), fMieG, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xDest.GetRayleighScaleHeight(), Zenith_GetDefaultAtmosphereRayleighScaleHeight(), 0.0f,
		"v1 gets the Earth scale height, which is what it always integrated");
	ZENITH_ASSERT_EQ_FLOAT(xDest.GetMieScaleHeight(), Zenith_GetDefaultAtmosphereMieScaleHeight(), 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xDest.GetGroundAlbedo(), Zenith_GetDefaultAtmosphereGroundAlbedo(), 0.0f,
		"v1 gets the 0.25 the shader constant used to hard-code");
	ZENITH_ASSERT_FALSE(xDest.IsLocalBlendVolume(), "a v1 atmosphere stays GLOBAL");
}

ZENITH_TEST(AtmosphereComponent, HasNoRadiometricAnchorOrExposureField)
{
	// The component is a physical-medium descriptor only. The resolved
	// environment snapshot it feeds (Zenith_EnvironmentAuthorityData) must carry
	// the medium scales but no solar radiance/intensity/exposure channel -- the
	// engine's single radiometric anchor is POLICY and lives only in Flux
	// (AtmosphereConfig::fSUN_INTENSITY). This is a compile-time data-shape lock:
	// a default-constructed snapshot has exactly the atmosphere scales at their
	// physical defaults and no anchor contribution to assert against.
	Zenith_EnvironmentAuthorityData xDefault;
	ZENITH_ASSERT_FALSE(xDefault.m_bAtmosphereAuthored, "default snapshot has no authored atmosphere");
	ZENITH_ASSERT_EQ_FLOAT(xDefault.m_fRayleighScale, Zenith_GetDefaultAtmosphereRayleighScale(), 0.0f, "snapshot default Rayleigh");
	ZENITH_ASSERT_EQ_FLOAT(xDefault.m_fMieScale, Zenith_GetDefaultAtmosphereMieScale(), 0.0f, "snapshot default Mie");
	ZENITH_ASSERT_EQ_FLOAT(xDefault.m_fMieG, Zenith_GetDefaultAtmosphereMieG(), 0.0f, "snapshot default Mie-G");
	// The snapshot has no Sun intensity / radiance / exposure member at all by
	// construction; the only "authored" boolean beside atmosphere is the Sun
	// geometry flag. So authoring an atmosphere alone leaves the Sun at its
	// exact legacy fallback direction (the directory contract).
	Zenith_TempScene xScene("AtmosphereNoAnchor");
	xScene.CreateEntity("Env").AddComponent<Zenith_AtmosphereComponent>();
	Zenith_EnvironmentAuthorityData xEnv;
	g_pfnZenithEnvironmentAuthorityGather(xEnv);
	ZENITH_ASSERT_TRUE(xEnv.m_bAtmosphereAuthored, "atmosphere authored");
	ZENITH_ASSERT_FALSE(xEnv.m_bSunAuthored, "no Sun authored");
	ZENITH_ASSERT_NEAR_VEC3(xEnv.m_xSunDirection, Zenith_GetDefaultSunDirection(), 0.0f,
		"atmosphere-only entity: Sun keeps the exact legacy fallback direction");
}

#endif // ZENITH_TESTING