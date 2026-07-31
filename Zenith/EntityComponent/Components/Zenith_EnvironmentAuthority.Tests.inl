//------------------------------------------------------------------------------
// Zenith_EnvironmentAuthority gather tests. The gather resolves ONE coherent
// environment per frame: the Zenith_SunComponent + Zenith_AtmosphereComponent
// are read TOGETHER from one authoritative environment entity (active scene
// wins, else lowest stable entity ID) and never mixed across loaded scenes.
// Included at the bottom of Zenith_LightComponent.cpp (the gather host TU).
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

#include "EntityComponent/Components/Zenith_SunComponent.h"
#include "EntityComponent/Components/Zenith_AtmosphereComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Core/Zenith_EnvironmentAuthority.h"
#include "UnitTests/Zenith_TempScene.h"

// A multi-global-environment conflict is a HARD authoring error in a tools
// build (it is silent data loss). Every test below that deliberately builds one
// scopes the assert off -- otherwise the suite would abort on the very
// behaviour it is checking. Runtime builds compile this to nothing.
#ifdef ZENITH_TOOLS
	#define ZENITH_TEST_ALLOW_ENVIRONMENT_CONFLICT() \
		Zenith_ScopedEnvironmentConflictAssertSuppression xAllowConflict
#else
	#define ZENITH_TEST_ALLOW_ENVIRONMENT_CONFLICT() ((void)0)
#endif

ZENITH_TEST(EnvironmentAuthority, ActiveSceneWinsThenLowestStableEntityID)
{
	ZENITH_TEST_ALLOW_ENVIRONMENT_CONFLICT();
	Zenith_TempScene xSceneA("EnvConflictA");
	Zenith_Entity xEntityA = xSceneA.CreateEntity("SunA");
	xEntityA.AddComponent<Zenith_SunComponent>().SetDirection(Zenith_Maths::Vector3(-1.0f, -1.0f, 0.0f));

	Zenith_TempScene xSceneB("EnvConflictB");
	Zenith_Entity xEntityB = xSceneB.CreateEntity("SunB");
	xEntityB.AddComponent<Zenith_SunComponent>().SetDirection(Zenith_Maths::Vector3(1.0f, -1.0f, 0.0f));
	Zenith_Entity xEntityB2 = xSceneB.CreateEntity("SunB2");
	xEntityB2.AddComponent<Zenith_SunComponent>().SetDirection(Zenith_Maths::Vector3(0.0f, -1.0f, 1.0f));

	// xSceneB was created last -> active scene is B.
	Zenith_EnvironmentAuthorityData xResolved;
	g_pfnZenithEnvironmentAuthorityGather(xResolved);
	ZENITH_ASSERT_TRUE(xResolved.m_bSunAuthored);
	ZENITH_ASSERT_FALSE(xResolved.m_bAtmosphereAuthored);
	ZENITH_ASSERT_EQ(xResolved.m_uSunAuthoredCount, 3u);
	ZENITH_ASSERT_EQ(xResolved.m_uAtmosphereAuthoredCount, 0u);
	ZENITH_ASSERT_EQ(xResolved.m_uEnvironmentEntityCount, 3u);
	ZENITH_ASSERT_TRUE(xResolved.m_bEnvironmentSourceIsInActiveScene);
	ZENITH_ASSERT_EQ(xResolved.m_uEnvironmentEntityIndex, xEntityB.GetEntityID().m_uIndex);

	g_xEngine.Scenes().SetActiveScene(xSceneA.Scene());
	g_pfnZenithEnvironmentAuthorityGather(xResolved);
	ZENITH_ASSERT_EQ(xResolved.m_uEnvironmentEntityIndex, xEntityA.GetEntityID().m_uIndex);
	ZENITH_ASSERT_TRUE(xResolved.m_bEnvironmentSourceIsInActiveScene);
	ZENITH_ASSERT_TRUE(xResolved.m_bSunAuthored);
	ZENITH_ASSERT_EQ(xResolved.m_uEnvironmentEntityCount, 3u);
}

ZENITH_TEST(EnvironmentAuthority, NoCrossSceneMixingOfSunAndAtmosphere)
{
	ZENITH_TEST_ALLOW_ENVIRONMENT_CONFLICT();
	// Atmosphere on an entity in scene A, Sun on an entity in scene B. Whichever
	// scene wins, the OTHER component must NOT be pulled from the losing scene.
	Zenith_TempScene xSceneA("MixA");
	Zenith_Maths::Vector3 xAtmoDir(0.0f);
	{
		Zenith_Entity xAtmoEntity = xSceneA.CreateEntity("Atmo");
		Zenith_AtmosphereComponent& xAtmo = xAtmoEntity.AddComponent<Zenith_AtmosphereComponent>();
		xAtmo.SetRayleighScale(2.0f);
		xAtmo.SetMieScale(0.5f);
		xAtmo.SetMieG(0.8f);
	}

	Zenith_TempScene xSceneB("MixB");
	{
		Zenith_Entity xSunEntity = xSceneB.CreateEntity("Sun");
		xSunEntity.AddComponent<Zenith_SunComponent>().SetDirection(Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f));
	}

	// Active scene = B (created last): the B Sun's entity wins. The A atmosphere
	// is NOT mixed in -> atmosphere falls back to physical defaults.
	Zenith_EnvironmentAuthorityData xResolved;
	g_pfnZenithEnvironmentAuthorityGather(xResolved);
	ZENITH_ASSERT_TRUE(xResolved.m_bSunAuthored, "B's Sun is the winner");
	ZENITH_ASSERT_FALSE(xResolved.m_bAtmosphereAuthored, "A's atmosphere must not cross into B's winning entity");
	ZENITH_ASSERT_NEAR_VEC3(xResolved.m_xSunDirection, Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f), 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xResolved.m_fRayleighScale, Zenith_GetDefaultAtmosphereRayleighScale(), 0.0f, "atmosphere stays default");

	// Flip active scene to A: A's atmosphere entity now wins. B's Sun is NOT
	// mixed in -> the Sun keeps the exact legacy fallback direction.
	g_xEngine.Scenes().SetActiveScene(xSceneA.Scene());
	g_pfnZenithEnvironmentAuthorityGather(xResolved);
	ZENITH_ASSERT_TRUE(xResolved.m_bAtmosphereAuthored, "A's atmosphere is the winner");
	ZENITH_ASSERT_FALSE(xResolved.m_bSunAuthored, "B's Sun must not cross into A's winning entity");
	ZENITH_ASSERT_NEAR_VEC3(xResolved.m_xSunDirection, Zenith_GetDefaultSunDirection(), 0.0f,
		"Sun stays at the legacy fallback when the winner only authored an atmosphere");
	ZENITH_ASSERT_EQ_FLOAT(xResolved.m_fRayleighScale, 2.0f, 0.0f, "A's Rayleigh scale reaches the snapshot");
	ZENITH_ASSERT_EQ_FLOAT(xResolved.m_fMieG, 0.8f, 0.0f, "A's Mie-G reaches the snapshot");
}

ZENITH_TEST(EnvironmentAuthority, CoLocatedComponentsResolveAsOneCoherentEnvironment)
{
	// The intended authoring shape: one environment entity with BOTH the Sun and
	// the atmosphere. The snapshot carries both halves, resolved together.
	Zenith_TempScene xScene("CoLocated");
	Zenith_Entity xEnv = xScene.CreateEntity("Env");
	Zenith_SunComponent& xSun = xEnv.AddComponent<Zenith_SunComponent>();
	xSun.SetTimeOfDayAngleDegrees(90.0f);
	Zenith_AtmosphereComponent& xAtmo = xEnv.AddComponent<Zenith_AtmosphereComponent>();
	xAtmo.SetRayleighScale(1.5f);
	xAtmo.SetMieScale(1.5f);

	Zenith_EnvironmentAuthorityData xResolved;
	g_pfnZenithEnvironmentAuthorityGather(xResolved);
	ZENITH_ASSERT_TRUE(xResolved.m_bSunAuthored);
	ZENITH_ASSERT_TRUE(xResolved.m_bAtmosphereAuthored);
	ZENITH_ASSERT_EQ(xResolved.m_uEnvironmentEntityCount, 1u);
	ZENITH_ASSERT_EQ(xResolved.m_uEnvironmentEntityIndex, xEnv.GetEntityID().m_uIndex);
	ZENITH_ASSERT_NEAR_VEC3(xResolved.m_xSunDirection, Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f), 0.0001f,
		"noon sun resolves straight down");
	ZENITH_ASSERT_EQ_FLOAT(xResolved.m_fRayleighScale, 1.5f, 0.0f, "atmosphere scale co-resolved");
}

ZENITH_TEST(EnvironmentAuthority, PartialRigKeepsLegacyDefaultsForMissingHalf)
{
	// A scene authored with ONLY a Sun (the DevilsPlayground shape) resolves the
	// Sun geometry and keeps the physical DEFAULT atmosphere -- identical to the
	// pre-component engine. No warning fires (a single environment entity).
	Zenith_TempScene xScene("SunOnly");
	Zenith_Entity xSun = xScene.CreateEntity("Sun");
	Zenith_SunComponent& xSunC = xSun.AddComponent<Zenith_SunComponent>();
	xSunC.SetTimeOfDayAngleDegrees(270.0f);

	Zenith_EnvironmentAuthorityData xResolved;
	g_pfnZenithEnvironmentAuthorityGather(xResolved);
	ZENITH_ASSERT_TRUE(xResolved.m_bSunAuthored);
	ZENITH_ASSERT_FALSE(xResolved.m_bAtmosphereAuthored);
	ZENITH_ASSERT_EQ(xResolved.m_uEnvironmentEntityCount, 1u);
	ZENITH_ASSERT_EQ_FLOAT(xResolved.m_fRayleighScale, Zenith_GetDefaultAtmosphereRayleighScale(), 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xResolved.m_fMieScale, Zenith_GetDefaultAtmosphereMieScale(), 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xResolved.m_fMieG, Zenith_GetDefaultAtmosphereMieG(), 0.0f);
}

ZENITH_TEST(EnvironmentAuthority, AuthoredScaleHeightsAndGroundAlbedoReachTheSnapshot)
{
	// The v2 medium parameters must survive the gather. Ground albedo in
	// particular has no other route to the renderer: it is capture-only, so if
	// it stops here the IBL silently falls back to 0.25 for every world.
	Zenith_TempScene xScene("AuthoredMedium");
	Zenith_Entity xEnv = xScene.CreateEntity("Env");
	xEnv.AddComponent<Zenith_SunComponent>().SetTimeOfDayAngleDegrees(90.0f);
	Zenith_AtmosphereComponent& xAtmo = xEnv.AddComponent<Zenith_AtmosphereComponent>();
	xAtmo.SetRayleighScaleHeight(11000.0f);
	xAtmo.SetMieScaleHeight(400.0f);
	xAtmo.SetGroundAlbedo(0.7f);   // snow

	Zenith_EnvironmentAuthorityData xResolved;
	g_pfnZenithEnvironmentAuthorityGather(xResolved);
	ZENITH_ASSERT_TRUE(xResolved.m_bAtmosphereAuthored);
	ZENITH_ASSERT_EQ_FLOAT(xResolved.m_fRayleighScaleHeight, 11000.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xResolved.m_fMieScaleHeight, 400.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xResolved.m_fGroundAlbedo, 0.7f, 0.0f);
	ZENITH_ASSERT_EQ(xResolved.m_uBlendVolumesApplied, 0u, "a global atmosphere is not a blend volume");
}

// ============================================================================
// Local atmosphere blend volumes.
//
// A volume is weighted at the MAIN CAMERA, so these build a camera entity and
// move it. The Sun is deliberately never blended (it is celestial), so these
// only ever assert on the medium.
// ============================================================================

namespace
{
	// The blend position comes from the NEUTRAL render gather, which is exactly
	// the boundary Flux consumes. A test can therefore stand in for the camera by
	// swapping the gather function pointer -- no scene main-camera registration,
	// no concrete camera component, and the same code path production uses.
	Zenith_Maths::Vector3 g_xEnvTestViewPosition(0.0f);
	bool                  g_bEnvTestViewValid = false;

	void EnvTestCameraGather(Zenith_CameraRenderData& xOut)
	{
		xOut = Zenith_CameraRenderData();
		xOut.m_bValid = g_bEnvTestViewValid;
		xOut.m_xPositionPad = Zenith_Maths::Vector4(
			g_xEnvTestViewPosition.x, g_xEnvTestViewPosition.y, g_xEnvTestViewPosition.z, 1.0f);
	}

	// RAII: the gather pointer is process-global, so it must be restored however
	// the test exits.
	struct EnvTestScopedView
	{
		Zenith_CameraGatherFn m_pfnPrevious;
		explicit EnvTestScopedView(const Zenith_Maths::Vector3& xPos)
			: m_pfnPrevious(g_pfnZenithCameraGather)
		{
			g_xEnvTestViewPosition = xPos;
			g_bEnvTestViewValid = true;
			g_pfnZenithCameraGather = &EnvTestCameraGather;
		}
		~EnvTestScopedView()
		{
			g_pfnZenithCameraGather = m_pfnPrevious;
			g_bEnvTestViewValid = false;
		}
		void MoveTo(const Zenith_Maths::Vector3& xPos) { g_xEnvTestViewPosition = xPos; }
	};

	Zenith_Entity EnvTestCreateVolume(Zenith_TempScene& xScene, const char* szName,
		const Zenith_Maths::Vector3& xCentre, float fRadius, float fFalloff, float fPriority,
		float fRayleighScale, float fMieScale)
	{
		Zenith_Entity xEntity = xScene.CreateEntity(szName);
		xEntity.GetComponent<Zenith_TransformComponent>().SetPosition(xCentre);
		Zenith_AtmosphereComponent& xAtmo = xEntity.AddComponent<Zenith_AtmosphereComponent>();
		xAtmo.SetBlendRadius(fRadius);
		xAtmo.SetBlendFalloff(fFalloff);
		xAtmo.SetBlendPriority(fPriority);
		xAtmo.SetRayleighScale(fRayleighScale);
		xAtmo.SetMieScale(fMieScale);
		return xEntity;
	}
}

ZENITH_TEST(EnvironmentBlend, VolumeWeightFalloffIsSmoothAndBounded)
{
	// Pure helper: no ECS, no camera.
	ZENITH_ASSERT_EQ_FLOAT(Zenith_ComputeBlendVolumeWeight(0.0f, 100.0f, 20.0f), 1.0f, 0.0f, "dead centre");
	ZENITH_ASSERT_EQ_FLOAT(Zenith_ComputeBlendVolumeWeight(80.0f, 100.0f, 20.0f), 1.0f, 0.0f, "inner edge of the band");
	ZENITH_ASSERT_EQ_FLOAT(Zenith_ComputeBlendVolumeWeight(100.0f, 100.0f, 20.0f), 0.0f, 0.0f, "at the radius");
	ZENITH_ASSERT_EQ_FLOAT(Zenith_ComputeBlendVolumeWeight(150.0f, 100.0f, 20.0f), 0.0f, 0.0f, "outside");
	ZENITH_ASSERT_EQ_FLOAT(Zenith_ComputeBlendVolumeWeight(90.0f, 100.0f, 20.0f), 0.5f, 0.0001f, "band midpoint");
	// A hard-edged volume (no band) is 1 right up to the radius.
	ZENITH_ASSERT_EQ_FLOAT(Zenith_ComputeBlendVolumeWeight(99.0f, 100.0f, 0.0f), 1.0f, 0.0f, "hard edge inside");
	ZENITH_ASSERT_EQ_FLOAT(Zenith_ComputeBlendVolumeWeight(100.0f, 100.0f, 0.0f), 0.0f, 0.0f, "hard edge at radius");
	// radius 0 means "not a volume" -- it must never contribute.
	ZENITH_ASSERT_EQ_FLOAT(Zenith_ComputeBlendVolumeWeight(0.0f, 0.0f, 0.0f), 0.0f, 0.0f, "global is weightless");
}

ZENITH_TEST(EnvironmentBlend, LayerBlendIsALerpAndZeroWeightIsANoOp)
{
	Zenith_AtmosphereMedium xBase;
	xBase.m_fRayleighScale = 1.0f;
	xBase.m_fMieScale      = 1.0f;
	xBase.m_fGroundAlbedo  = 0.25f;
	Zenith_AtmosphereMedium xLayer;
	xLayer.m_fRayleighScale = 3.0f;
	xLayer.m_fMieScale      = 5.0f;
	xLayer.m_fGroundAlbedo  = 0.85f;

	const Zenith_AtmosphereMedium xNone = Zenith_BlendAtmosphereLayer(xBase, xLayer, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xNone.m_fRayleighScale, 1.0f, 0.0f, "zero weight changes nothing");
	ZENITH_ASSERT_EQ_FLOAT(xNone.m_fGroundAlbedo, 0.25f, 0.0f);

	const Zenith_AtmosphereMedium xFull = Zenith_BlendAtmosphereLayer(xBase, xLayer, 1.0f);
	ZENITH_ASSERT_EQ_FLOAT(xFull.m_fRayleighScale, 3.0f, 0.0f, "full weight is the layer");
	ZENITH_ASSERT_EQ_FLOAT(xFull.m_fMieScale, 5.0f, 0.0f);

	const Zenith_AtmosphereMedium xHalf = Zenith_BlendAtmosphereLayer(xBase, xLayer, 0.5f);
	ZENITH_ASSERT_EQ_FLOAT(xHalf.m_fRayleighScale, 2.0f, 0.0001f, "half weight is the midpoint");
	ZENITH_ASSERT_EQ_FLOAT(xHalf.m_fMieScale, 3.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xHalf.m_fGroundAlbedo, 0.55f, 0.0001f);

	// Weights above 1 clamp rather than extrapolating past the layer.
	const Zenith_AtmosphereMedium xOver = Zenith_BlendAtmosphereLayer(xBase, xLayer, 4.0f);
	ZENITH_ASSERT_EQ_FLOAT(xOver.m_fRayleighScale, 3.0f, 0.0f, "weight clamps at 1");
}

ZENITH_TEST(EnvironmentBlend, LocalVolumeBlendsOverTheGlobalBaseAtTheCamera)
{
	Zenith_TempScene xScene("BlendBasic");
	EnvTestScopedView xView(Zenith_Maths::Vector3(1000.0f, 0.0f, 0.0f));

	// Global base.
	Zenith_Entity xEnv = xScene.CreateEntity("Env");
	xEnv.AddComponent<Zenith_SunComponent>().SetTimeOfDayAngleDegrees(90.0f);
	Zenith_AtmosphereComponent& xGlobal = xEnv.AddComponent<Zenith_AtmosphereComponent>();
	xGlobal.SetRayleighScale(1.0f);
	xGlobal.SetMieScale(1.0f);

	// A dusty basin at the origin: hard-edged so the weight is exactly 1 inside.
	EnvTestCreateVolume(xScene, "Basin", Zenith_Maths::Vector3(0.0f), 100.0f, 0.0f, 0.0f, 4.0f, 8.0f);

	// Camera far outside -> the global base, untouched, and NOT a conflict.
	Zenith_EnvironmentAuthorityData xResolved;
	g_pfnZenithEnvironmentAuthorityGather(xResolved);
	ZENITH_ASSERT_EQ(xResolved.m_uEnvironmentEntityCount, 1u,
		"a local volume never competes for authority, so this is not a conflict");
	ZENITH_ASSERT_EQ(xResolved.m_uBlendVolumesApplied, 0u);
	ZENITH_ASSERT_EQ_FLOAT(xResolved.m_fRayleighScale, 1.0f, 0.0f, "outside the volume: the base");

	// Camera inside -> fully the volume's medium.
	xView.MoveTo(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
	g_pfnZenithEnvironmentAuthorityGather(xResolved);
	ZENITH_ASSERT_EQ(xResolved.m_uBlendVolumesApplied, 1u);
	ZENITH_ASSERT_EQ_FLOAT(xResolved.m_fBlendWeightTotal, 1.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xResolved.m_fRayleighScale, 4.0f, 0.0001f, "inside the volume: its medium");
	ZENITH_ASSERT_EQ_FLOAT(xResolved.m_fMieScale, 8.0f, 0.0001f);
	// The SUN is celestial and must be untouched by where the camera stands.
	ZENITH_ASSERT_NEAR_VEC3(xResolved.m_xSunDirection, Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f), 0.0001f,
		"a local volume never moves the sun");
}

ZENITH_TEST(EnvironmentBlend, OverlappingVolumesApplyByPriorityNotQueryOrder)
{
	Zenith_TempScene xScene("BlendPriority");
	EnvTestScopedView xView(Zenith_Maths::Vector3(0.0f));

	Zenith_Entity xEnv = xScene.CreateEntity("Env");
	xEnv.AddComponent<Zenith_SunComponent>().SetTimeOfDayAngleDegrees(90.0f);
	xEnv.AddComponent<Zenith_AtmosphereComponent>().SetRayleighScale(1.0f);

	// Two hard-edged volumes both containing the camera, both at full weight.
	// The HIGHER priority is applied last and therefore wins outright.
	EnvTestCreateVolume(xScene, "LowPriority",  Zenith_Maths::Vector3(0.0f), 500.0f, 0.0f, -5.0f, 2.0f, 1.0f);
	EnvTestCreateVolume(xScene, "HighPriority", Zenith_Maths::Vector3(0.0f), 500.0f, 0.0f, 10.0f, 7.0f, 1.0f);

	Zenith_EnvironmentAuthorityData xResolved;
	g_pfnZenithEnvironmentAuthorityGather(xResolved);
	ZENITH_ASSERT_EQ(xResolved.m_uBlendVolumesApplied, 2u);
	ZENITH_ASSERT_EQ_FLOAT(xResolved.m_fRayleighScale, 7.0f, 0.0001f,
		"the higher-priority volume is applied last and wins at full weight");
	ZENITH_ASSERT_EQ(xResolved.m_uEnvironmentEntityCount, 1u, "volumes are not authority candidates");

	// Repeat gathers must be identical -- the sort is by (priority, stable ID),
	// never by the order the ECS query happened to visit the pools in.
	for (u_int u = 0u; u < 5u; u++)
	{
		Zenith_EnvironmentAuthorityData xRepeat;
		g_pfnZenithEnvironmentAuthorityGather(xRepeat);
		ZENITH_ASSERT_EQ_FLOAT(xRepeat.m_fRayleighScale, xResolved.m_fRayleighScale, 0.0f,
			"the composed medium is deterministic across gathers");
	}
}

ZENITH_TEST(EnvironmentBlend, VolumeWithNoGlobalStillAuthorsTheMedium)
{
	// A scene with local volumes but no global atmosphere: the volume blends
	// over the PHYSICAL DEFAULTS, and the result still counts as authored.
	Zenith_TempScene xScene("BlendNoGlobal");
	EnvTestScopedView xView(Zenith_Maths::Vector3(0.0f));
	EnvTestCreateVolume(xScene, "Fog", Zenith_Maths::Vector3(0.0f), 200.0f, 0.0f, 0.0f, 1.0f, 6.0f);

	Zenith_EnvironmentAuthorityData xResolved;
	g_pfnZenithEnvironmentAuthorityGather(xResolved);
	ZENITH_ASSERT_TRUE(xResolved.m_bAtmosphereAuthored, "a volume authors the medium the renderer uses");
	ZENITH_ASSERT_EQ_FLOAT(xResolved.m_fMieScale, 6.0f, 0.0001f);
	ZENITH_ASSERT_FALSE(xResolved.m_bSunAuthored, "no Sun anywhere -> the legacy fallback direction");
	ZENITH_ASSERT_NEAR_VEC3(xResolved.m_xSunDirection, Zenith_GetDefaultSunDirection(), 0.0f);
}

// ============================================================================
// The deterministic conflict signature (pure helper, no scenes needed).
//
// The throttle only re-warns when this key changes, so anything that changes
// what the warning MEANS must change the key -- including a LOSING candidate's
// Sun/Atmosphere mask, which the old winner-only hash silently ignored.
// ============================================================================

namespace
{
	Zenith_EnvironmentConflictCandidate EnvSigCandidate(uint64_t ulPacked,
		bool bSun, bool bAtmosphere, bool bActive)
	{
		Zenith_EnvironmentConflictCandidate x;
		x.m_ulPackedEntityID = ulPacked;
		x.m_bHasSun          = bSun;
		x.m_bHasAtmosphere   = bAtmosphere;
		x.m_bInActiveScene   = bActive;
		return x;
	}
}

ZENITH_TEST(EnvironmentConflictSignature, RepeatingTheIdenticalConflictIsStable)
{
	const Zenith_EnvironmentConflictCandidate axSet[3] = {
		EnvSigCandidate(11u, true,  false, true),
		EnvSigCandidate(22u, false, true,  false),
		EnvSigCandidate(33u, true,  true,  false),
	};
	const uint64_t ulA = Zenith_ComputeEnvironmentConflictSignature(axSet, 3u, 11u, true);
	const uint64_t ulB = Zenith_ComputeEnvironmentConflictSignature(axSet, 3u, 11u, true);
	ZENITH_ASSERT_EQ(ulA, ulB, "the same conflict must hash the same -- otherwise it warns every frame");
	ZENITH_ASSERT_TRUE(ulA != 0u, "0 is the no-conflict sentinel and must never be produced");
}

ZENITH_TEST(EnvironmentConflictSignature, LosingCandidateMaskChangeIsDistinct)
{
	// THE defect: entity IDs and the winner are unchanged; only a LOSING
	// candidate gains an Atmosphere. The counts the warning reports change, so
	// the key must too.
	const Zenith_EnvironmentConflictCandidate axBefore[2] = {
		EnvSigCandidate(11u, true, false, true),
		EnvSigCandidate(22u, true, false, false),
	};
	const Zenith_EnvironmentConflictCandidate axAfterAtmosphere[2] = {
		EnvSigCandidate(11u, true, false, true),
		EnvSigCandidate(22u, true, true,  false),   // loser gained an Atmosphere
	};
	const Zenith_EnvironmentConflictCandidate axAfterSunLost[2] = {
		EnvSigCandidate(11u, true,  false, true),
		EnvSigCandidate(22u, false, false, false),  // loser lost its Sun
	};
	const Zenith_EnvironmentConflictCandidate axAfterSceneMove[2] = {
		EnvSigCandidate(11u, true, false, true),
		EnvSigCandidate(22u, true, false, true),    // loser moved into the active scene
	};

	const uint64_t ulBefore = Zenith_ComputeEnvironmentConflictSignature(axBefore, 2u, 11u, true);
	ZENITH_ASSERT_TRUE(
		Zenith_ComputeEnvironmentConflictSignature(axAfterAtmosphere, 2u, 11u, true) != ulBefore,
		"a losing candidate gaining an Atmosphere is a new conflict");
	ZENITH_ASSERT_TRUE(
		Zenith_ComputeEnvironmentConflictSignature(axAfterSunLost, 2u, 11u, true) != ulBefore,
		"a losing candidate losing its Sun is a new conflict");
	ZENITH_ASSERT_TRUE(
		Zenith_ComputeEnvironmentConflictSignature(axAfterSceneMove, 2u, 11u, true) != ulBefore,
		"active-scene membership materially affects the winner rule");
}

ZENITH_TEST(EnvironmentConflictSignature, IndependentOfCandidateQueryOrder)
{
	// The ECS query visit order is not a contract, so the key must not depend
	// on it. Check every permutation of a 3-candidate set.
	const Zenith_EnvironmentConflictCandidate a = EnvSigCandidate(11u, true,  false, true);
	const Zenith_EnvironmentConflictCandidate b = EnvSigCandidate(22u, false, true,  false);
	const Zenith_EnvironmentConflictCandidate c = EnvSigCandidate(33u, true,  true,  false);

	const Zenith_EnvironmentConflictCandidate axPermutations[6][3] = {
		{ a, b, c }, { a, c, b }, { b, a, c }, { b, c, a }, { c, a, b }, { c, b, a },
	};
	const uint64_t ulReference = Zenith_ComputeEnvironmentConflictSignature(axPermutations[0], 3u, 11u, true);
	for (u_int u = 1u; u < 6u; u++)
	{
		ZENITH_ASSERT_EQ(
			Zenith_ComputeEnvironmentConflictSignature(axPermutations[u], 3u, 11u, true), ulReference,
			"permutation %u must hash identically", u);
	}

	// Sanity: the hash is not a constant that ignores its inputs.
	const Zenith_EnvironmentConflictCandidate axDifferent[3] = {
		a, b, EnvSigCandidate(44u, true, true, false) };
	ZENITH_ASSERT_TRUE(
		Zenith_ComputeEnvironmentConflictSignature(axDifferent, 3u, 11u, true) != ulReference,
		"a different candidate set must hash differently");
}

ZENITH_TEST(EnvironmentConflictSignature, WinnerAndCandidateCountAreCovered)
{
	const Zenith_EnvironmentConflictCandidate axSet[2] = {
		EnvSigCandidate(11u, true, false, true),
		EnvSigCandidate(22u, true, false, true),
	};
	const uint64_t ulWinner11 = Zenith_ComputeEnvironmentConflictSignature(axSet, 2u, 11u, true);
	const uint64_t ulWinner22 = Zenith_ComputeEnvironmentConflictSignature(axSet, 2u, 22u, true);
	ZENITH_ASSERT_TRUE(ulWinner11 != ulWinner22, "a different winner is a different warning");

	const uint64_t ulNoActiveScene = Zenith_ComputeEnvironmentConflictSignature(axSet, 2u, 11u, false);
	ZENITH_ASSERT_TRUE(ulNoActiveScene != ulWinner11, "'no active scene' changes the reported reason");

	const uint64_t ulSubset = Zenith_ComputeEnvironmentConflictSignature(axSet, 1u, 11u, true);
	ZENITH_ASSERT_TRUE(ulSubset != ulWinner11, "the candidate count is covered");
}

ZENITH_TEST(EnvironmentAuthority, ConflictWarningThrottlesPerDistinctConflictSet)
{
	ZENITH_TEST_ALLOW_ENVIRONMENT_CONFLICT();
	// Two environment entities (a conflict) emit a warning keyed by a
	// deterministic signature. Re-gathering the SAME conflict does not re-warn;
	// a scene-authority change (different winner) does. Probed through the test
	// seam that exposes the throttle key.
	Zenith_TempScene xSceneA("ThrottleA");
	Zenith_Entity xEntityA = xSceneA.CreateEntity("SunA");
	xEntityA.AddComponent<Zenith_SunComponent>().SetDirection(Zenith_Maths::Vector3(-1.0f, -1.0f, 0.0f));

	Zenith_TempScene xSceneB("ThrottleB");
	Zenith_Entity xEntityB = xSceneB.CreateEntity("SunB");
	xEntityB.AddComponent<Zenith_SunComponent>().SetDirection(Zenith_Maths::Vector3(1.0f, -1.0f, 0.0f));

	// Active scene = B -> B wins the conflict.
	Zenith_EnvironmentAuthorityData xDummy;
	g_pfnZenithEnvironmentAuthorityGather(xDummy);
	const uint64_t uSig1 = Zenith_GetLastEnvironmentConflictSignatureForTest();
	ZENITH_ASSERT_TRUE(uSig1 != 0u, "a 2-entity conflict produces a non-zero throttle key");

	g_pfnZenithEnvironmentAuthorityGather(xDummy);
	const uint64_t uSig1Repeat = Zenith_GetLastEnvironmentConflictSignatureForTest();
	ZENITH_ASSERT_EQ(uSig1Repeat, uSig1, "repeating the same conflict is throttled (same key)");

	// Change which scene is active -> the winner changes -> a new conflict
	// signature -> a fresh warning would fire.
	g_xEngine.Scenes().SetActiveScene(xSceneA.Scene());
	g_pfnZenithEnvironmentAuthorityGather(xDummy);
	const uint64_t uSig2 = Zenith_GetLastEnvironmentConflictSignatureForTest();
	ZENITH_ASSERT_TRUE(uSig2 != uSig1, "authority change yields a new throttle key -> new warning");
}

ZENITH_TEST(EnvironmentAuthority, RepeatedIdenticalConflictWarnsExactlyOnce)
{
	ZENITH_TEST_ALLOW_ENVIRONMENT_CONFLICT();
	// Observed at the WARN SITE, not through the signature proxy: a conflict
	// that does not change must produce exactly one Zenith_Warning however many
	// frames gather it.
	Zenith_ResetEnvironmentConflictThrottleForTest();

	Zenith_TempScene xSceneA("WarnOnceA");
	Zenith_Entity xEntityA = xSceneA.CreateEntity("SunA");
	xEntityA.AddComponent<Zenith_SunComponent>().SetDirection(Zenith_Maths::Vector3(-1.0f, -1.0f, 0.0f));

	Zenith_TempScene xSceneB("WarnOnceB");
	Zenith_Entity xEntityB = xSceneB.CreateEntity("SunB");
	xEntityB.AddComponent<Zenith_SunComponent>().SetDirection(Zenith_Maths::Vector3(1.0f, -1.0f, 0.0f));

	Zenith_EnvironmentAuthorityData xResolved;
	for (u_int u = 0u; u < 30u; u++)   // 30 "frames" of the identical conflict
	{
		g_pfnZenithEnvironmentAuthorityGather(xResolved);
	}
	ZENITH_ASSERT_EQ(Zenith_GetEnvironmentConflictWarningCountForTest(), 1u,
		"an unchanged conflict warns once, not once per frame");
	ZENITH_ASSERT_EQ(xResolved.m_uEnvironmentEntityCount, 2u);
}

ZENITH_TEST(EnvironmentAuthority, LosingCandidateGainingAnAtmosphereReWarns)
{
	ZENITH_TEST_ALLOW_ENVIRONMENT_CONFLICT();
	// The regression the winner-only signature hid: the entity IDs and the
	// WINNER are unchanged, but a losing candidate gains an Atmosphere -- the
	// conflict now also means "an authored atmosphere is being ignored", so it
	// must warn again.
	Zenith_ResetEnvironmentConflictThrottleForTest();

	Zenith_TempScene xSceneA("LoserMaskA");
	Zenith_Entity xLoser = xSceneA.CreateEntity("SunA");
	xLoser.AddComponent<Zenith_SunComponent>().SetDirection(Zenith_Maths::Vector3(-1.0f, -1.0f, 0.0f));

	Zenith_TempScene xSceneB("LoserMaskB");
	Zenith_Entity xWinner = xSceneB.CreateEntity("SunB");
	xWinner.AddComponent<Zenith_SunComponent>().SetDirection(Zenith_Maths::Vector3(1.0f, -1.0f, 0.0f));

	Zenith_EnvironmentAuthorityData xResolved;
	g_pfnZenithEnvironmentAuthorityGather(xResolved);
	g_pfnZenithEnvironmentAuthorityGather(xResolved);   // throttled repeat
	const u_int uWarningsBefore = Zenith_GetEnvironmentConflictWarningCountForTest();
	const uint64_t ulSigBefore = Zenith_GetLastEnvironmentConflictSignatureForTest();
	ZENITH_ASSERT_EQ(uWarningsBefore, 1u, "the initial conflict warned once");
	ZENITH_ASSERT_EQ(xResolved.m_uEnvironmentEntityIndex, xWinner.GetEntityID().m_uIndex,
		"the active-scene entity wins");

	// Only the LOSER's component mask changes.
	xLoser.AddComponent<Zenith_AtmosphereComponent>().SetRayleighScale(2.0f);

	g_pfnZenithEnvironmentAuthorityGather(xResolved);
	ZENITH_ASSERT_EQ(xResolved.m_uEnvironmentEntityIndex, xWinner.GetEntityID().m_uIndex,
		"the winner is unchanged");
	ZENITH_ASSERT_EQ(xResolved.m_uEnvironmentEntityCount, 2u, "the entity set is unchanged");
	ZENITH_ASSERT_EQ(xResolved.m_uAtmosphereAuthoredCount, 1u, "the losing candidate now authors an atmosphere");
	ZENITH_ASSERT_TRUE(Zenith_GetLastEnvironmentConflictSignatureForTest() != ulSigBefore,
		"the signature must move when a losing candidate's mask changes");
	ZENITH_ASSERT_EQ(Zenith_GetEnvironmentConflictWarningCountForTest(), 2u,
		"the changed conflict warns again");

	// ... and the new state is itself throttled.
	g_pfnZenithEnvironmentAuthorityGather(xResolved);
	g_pfnZenithEnvironmentAuthorityGather(xResolved);
	ZENITH_ASSERT_EQ(Zenith_GetEnvironmentConflictWarningCountForTest(), 2u,
		"the new conflict is throttled in turn");
}

#endif // ZENITH_TESTING