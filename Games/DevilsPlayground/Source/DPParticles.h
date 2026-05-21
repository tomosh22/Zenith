#pragma once

// =============================================================================
// DPParticles -- in-world particle telegraphs for DP interactions.
//
// Every DP interactable (forge / door / chest / pentagram / BellSoul /
// BogWater) and the priest fire a short-lived particle burst on a
// significant event so the player can SEE what just happened. Without
// these, every interaction is "press F, item in hand changes colour" --
// the placeholder visual language gives zero feedback on whether the
// craft / unlock / open actually succeeded.
//
// Architecture (modeled on Sokoban's dust trail):
//
//   1. Project_RegisterScriptBehaviours / DevilsPlayground::InitializeResources
//      calls DP_Particles::Initialize() once at boot. That registers eight
//      Flux_ParticleEmitterConfigs with the global registry (so the engine
//      can resolve them by name) and subscribes to the DP events that
//      should trigger a burst.
//
//   2. ProcLevelBootstrap (or any other scene-side init) calls
//      DP_Particles::EnsureEmittersInScene() after the gameplay scene is
//      ready. That creates one persistent-scene emitter ENTITY per effect
//      kind, each with a Zenith_ParticleEmitterComponent referencing the
//      matching config. The entities sit at (0,0,0) until a burst is
//      requested, at which point Burst() repositions and emits N particles.
//
//   3. Event handlers (registered in Initialize) translate each
//      DP_On<Foo> event into a Burst(kind, position) call.
//
// Lifecycle:
//   - Initialize / Shutdown wrap the global registration (configs +
//     event subscriptions). Idempotent.
//   - EnsureEmittersInScene / ClearEmitterEntities wrap the per-scene
//     entity creation. EnsureEmittersInScene is idempotent (won't
//     re-create entities that already exist + are valid).
//
// All configs use CPU simulation (small burst counts, short lifetimes --
// no GPU compute benefit). Additive blending where appropriate
// (sparks / ritual swirls glow; dust / steam don't).
// =============================================================================

#include "EntityComponent/Zenith_Entity.h"
#include "Maths/Zenith_Maths.h"

namespace DP_Particles
{
	enum class Kind : uint8_t
	{
		ForgeSparks       = 0,  // orange/yellow sparks burst, additive
		DoorOpenDust      = 1,  // brown dust puff, no blend
		DoorLockRejected  = 2,  // red shake puff (player needs key)
		ChestOpenDust     = 3,  // brown dust puff, no blend
		PentagramRitual   = 4,  // purple/violet ritual swirl, additive
		BellSoulRing      = 5,  // gold radial bell-ring (large radius), additive
		BogWaterSteam     = 6,  // grey-white steam, slow rise
		PriestAlert       = 7,  // red "!" burst above priest head, additive
		HighScentAura     = 8,  // continuous violet aura around the high-scent
		                        //   villager. Distinct from the other kinds: NOT
		                        //   burst-based -- continuously emits while the
		                        //   tracked villager has scent above threshold.
		DevoutChannel     = 9,  // continuous candlelight motes around a Devout
		                        //   being possessed while the 0.8 s channel
		                        //   completes. Player feedback for "the
		                        //   possession is in progress, don't move yet."
		BeggarStealthAura = 10, // continuous dim grey halo around any possessed
		                        //   Beggar. Telegraphs "you are invisible to
		                        //   Aelfric" -- otherwise the Beggar-ignored
		                        //   rule is a silent BridgePerception filter
		                        //   the player can't see.
		ChildToolRefusal  = 11, // one-shot red-X burst when a Child villager
		                        //   tries to auto-pickup a tool (Iron / Key)
		                        //   and the proximity-pickup is rejected.

		COUNT
	};

	// ----- One-time global init -----

	// Register the eight Flux_ParticleEmitterConfigs with the global config
	// registry. Subscribes to the DP events that trigger bursts. Called
	// from DevilsPlayground::InitializeResources. Idempotent.
	void Initialize();

	// Unregister configs + unsubscribe events. Called from
	// DevilsPlayground::CleanupResources. Idempotent.
	void Shutdown();

	// ----- Per-scene emitter entities -----

	// Create one persistent-scene emitter entity per Kind. Called by
	// DPProcLevelBootstrap_Behaviour::OnAwake after the bootstrap has
	// inserted itself into the gameplay scene. Idempotent -- skips kinds
	// whose emitter EntityID is still valid from a prior call.
	void EnsureEmittersInScene();

	// Destroy all emitter entities. Called from the between-tests hook
	// + Shutdown so a fresh scene gets a fresh emitter set. Idempotent.
	void ClearEmitterEntities();

	// ----- Burst API (callable any time after EnsureEmittersInScene) -----

	// Emit a one-shot burst of the configured size at xWorldPos. No-op
	// if the emitter entity for this kind doesn't exist (e.g. a test
	// that loads ProcLevel but doesn't run the bootstrap). The emit
	// direction is config-driven (Forge sparks go up + out, Bog steam
	// goes straight up, etc.) -- callers don't supply direction.
	void Burst(Kind eKind, const Zenith_Maths::Vector3& xWorldPos);

	// Variant that resolves the entity's world position itself. Convenient
	// for "burst at this villager / door / chest entity" sites. No-op on
	// invalid entity.
	void BurstAtEntity(Kind eKind, Zenith_EntityID xEntity);

	// ----- Continuous-emission API (for the HighScentAura kind) -----

	// Reposition the HighScentAura emitter to track a villager + toggle
	// emission. Called by DPPlayerController_Behaviour once per frame
	// (after WriteHighestScentToBlackboard) with the highest-scent
	// villager + a bShow flag derived from scent >= threshold. Passing
	// INVALID_ENTITY_ID for xVillager OR bShow=false stops emission.
	// Idempotent.
	void UpdateHighScentAura(Zenith_EntityID xVillager, bool bShow);

	// 2026-05-21: archetype-aware aura updates. Same shape as
	// UpdateHighScentAura but for the Beggar + Devout effects.
	// Called once per frame from DPPlayerController_Behaviour to
	// track:
	//   * The currently-possessed Beggar (BeggarStealthAura emits).
	//   * The current Devout channel target during a 0.8 s channel
	//     (DevoutChannel emits).
	// Each is a SetEmitting(true) + SetEmitPosition call when the
	// state holds; off otherwise. Idempotent.
	void UpdateBeggarStealthAura(Zenith_EntityID xVillager, bool bShow);
	void UpdateDevoutChannelAura(Zenith_EntityID xVillager, bool bShow);

	// ----- Test accessors (ZENITH_INPUT_SIMULATOR only) -----
	//
	// Particle effects are visual flair, but the dispatch chain is
	// gameplay-relevant: "did the burst fire?" is the contract these
	// tests pin. The total burst count per kind survives between-tests
	// resets (cleared in ClearEmitterEntities).

	uint32_t GetBurstCountForTest(Kind eKind);
	Zenith_EntityID GetEmitterEntityForTest(Kind eKind);
	void ResetBurstCountsForTest();
}
