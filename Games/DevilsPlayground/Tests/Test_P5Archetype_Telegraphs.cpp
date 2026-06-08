#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Source/DPParticles.h"
#include "Components/DPVillager_Behaviour.h"

#include <cstdio>

// ============================================================================
// Test_P5Archetype_Telegraphs (2026-05-21)
//
// Pins the three archetype-specific in-world telegraphs:
//   * ChildToolRefusal -- one-shot red-X burst dispatched by DPItemBase
//     when a possessed Child tries to auto-pickup a tool. Verified by
//     dispatching DP_OnChildToolRefused directly + watching the burst
//     counter increment.
//   * UpdateBeggarStealthAura -- emitter toggles on with valid villager,
//     off when bShow=false / villager invalid.
//   * UpdateDevoutChannelAura -- same shape as BeggarStealthAura.
//
// All three are exercised in pure-namespace mode (no full gameplay
// scene needed). The bootstrap creates the emitter entities; we
// dispatch events / call the update APIs and observe the result.
// ============================================================================

namespace
{
	enum Phase : int { kP_Start, kP_WaitScene, kP_Run, kP_Verify, kP_Done };

	int                     g_iPhase = kP_Start;
	uint32_t                g_uChildRefusalBefore = 0;
	uint32_t                g_uChildRefusalAfter = 0;
	bool                    g_bBeggarOn = false;
	bool                    g_bBeggarOff = true;
	bool                    g_bDevoutOn = false;
	bool                    g_bDevoutOff = true;
	int                     g_iWait = 0;
}

static bool IsAuraEmitting(DP_Particles::Kind eKind)
{
	const Zenith_EntityID xId = DP_Particles::GetEmitterEntityForTest(eKind);
	if (!xId.IsValid()) return false;
	Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
	if (pxScene == nullptr) return false;
	Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
	if (!xEnt.IsValid() || !xEnt.HasComponent<Zenith_ParticleEmitterComponent>()) return false;
	return xEnt.GetComponent<Zenith_ParticleEmitterComponent>().IsEmitting();
}

static void Setup_P5ArchetypeTelegraphs()
{
	g_iPhase = kP_Start;
	g_uChildRefusalBefore = 0;
	g_uChildRefusalAfter = 0;
	g_bBeggarOn = false;
	g_bBeggarOff = true;
	g_bDevoutOn = false;
	g_bDevoutOff = true;
	g_iWait = 0;
}

static bool Step_P5ArchetypeTelegraphs(int /*iFrame*/)
{
	switch (g_iPhase)
	{
	case kP_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kP_WaitScene;
		g_iWait = 0;
		return true;

	case kP_WaitScene:
		++g_iWait;
		if (g_iWait < 6) return true;
		g_iPhase = kP_Run;
		return true;

	case kP_Run:
	{
		DP_Particles::ResetBurstCountsForTest();
		g_uChildRefusalBefore =
			DP_Particles::GetBurstCountForTest(DP_Particles::Kind::ChildToolRefusal);

		// Pick any villager for the entity-arg slots.
		Zenith_EntityID xAny;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xAny](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				if (!xAny.IsValid()) xAny = xId;
			});

		// Dispatch ChildToolRefusal event + expect a burst.
		Zenith_EventDispatcher::Get().Dispatch(DP_OnChildToolRefused{
			xAny, xAny, DP_ItemTag::Iron, Zenith_Maths::Vector3(10.f, 0.f, 10.f) });
		g_uChildRefusalAfter =
			DP_Particles::GetBurstCountForTest(DP_Particles::Kind::ChildToolRefusal);

		// BeggarStealthAura: on with valid villager, off otherwise.
		DP_Particles::UpdateBeggarStealthAura(xAny, true);
		g_bBeggarOn = IsAuraEmitting(DP_Particles::Kind::BeggarStealthAura);
		DP_Particles::UpdateBeggarStealthAura(xAny, false);
		g_bBeggarOff = IsAuraEmitting(DP_Particles::Kind::BeggarStealthAura);

		// DevoutChannel: same shape.
		DP_Particles::UpdateDevoutChannelAura(xAny, true);
		g_bDevoutOn = IsAuraEmitting(DP_Particles::Kind::DevoutChannel);
		DP_Particles::UpdateDevoutChannelAura(xAny, false);
		g_bDevoutOff = IsAuraEmitting(DP_Particles::Kind::DevoutChannel);

		std::printf("[P5Archetype] child_burst=%u->%u beggarOn=%d beggarOff=%d devoutOn=%d devoutOff=%d\n",
			g_uChildRefusalBefore, g_uChildRefusalAfter,
			(int)g_bBeggarOn, (int)g_bBeggarOff, (int)g_bDevoutOn, (int)g_bDevoutOff);
		std::fflush(stdout);
		g_iPhase = kP_Done;
		return false;
	}

	case kP_Done:
	default:
		return false;
	}
}

static bool Verify_P5ArchetypeTelegraphs()
{
	if (g_uChildRefusalAfter <= g_uChildRefusalBefore)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P5Archetype: ChildToolRefusal didn't burst on DP_OnChildToolRefused dispatch");
		return false;
	}
	if (!g_bBeggarOn)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P5Archetype: BeggarStealthAura didn't emit when UpdateBeggarStealthAura(villager, true) called");
		return false;
	}
	if (g_bBeggarOff)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P5Archetype: BeggarStealthAura still emitting after UpdateBeggarStealthAura(villager, false)");
		return false;
	}
	if (!g_bDevoutOn)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P5Archetype: DevoutChannel didn't emit when UpdateDevoutChannelAura(villager, true) called");
		return false;
	}
	if (g_bDevoutOff)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P5Archetype: DevoutChannel still emitting after UpdateDevoutChannelAura(villager, false)");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP5ArchetypeTest = {
	"Test_P5Archetype_Telegraphs",
	&Setup_P5ArchetypeTelegraphs,
	&Step_P5ArchetypeTelegraphs,
	&Verify_P5ArchetypeTelegraphs,
	120
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP5ArchetypeTest);

#endif // ZENITH_INPUT_SIMULATOR
