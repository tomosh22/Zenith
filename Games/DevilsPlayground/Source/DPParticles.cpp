#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Source/DPParticles.h"
#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"

#include <array>
#include <string>

namespace DP_Particles
{
	namespace
	{
		// =============================================================
		// Per-Kind config storage. Allocated in Initialize, freed in
		// Shutdown. Registered with the global Flux_ParticleEmitterConfig
		// registry under the kKindNames string so the engine can resolve
		// configs by name during scene restore. Keyed by Kind index for
		// O(1) lookup at burst time.
		// =============================================================
		constexpr int kNumKinds = static_cast<int>(Kind::COUNT);

		constexpr const char* kKindNames[kNumKinds] = {
			"DP_ForgeSparks",
			"DP_DoorOpenDust",
			"DP_DoorLockRejected",
			"DP_ChestOpenDust",
			"DP_PentagramRitual",
			"DP_BellSoulRing",
			"DP_BogWaterSteam",
			"DP_PriestAlert",
			"DP_HighScentAura",
			"DP_DevoutChannel",
			"DP_BeggarStealthAura",
			"DP_ChildToolRefusal",
		};

		// Per-Kind burst count. The Flux_ParticleEmitterConfig has
		// m_uBurstCount but that's the DEFAULT; Burst() passes an
		// explicit count to Emit() so we can vary by kind without
		// touching the config (cleaner for visualisation tuning).
		constexpr uint32_t kKindBurstCount[kNumKinds] = {
			24,  // ForgeSparks      -- generous; sparks read as plural
			16,  // DoorOpenDust     -- modest puff
			8,   // DoorLockRejected -- small, sharp red flash
			16,  // ChestOpenDust
			32,  // PentagramRitual  -- ritualistic; high count for swirl
			48,  // BellSoulRing     -- biggest; radial ring
			20,  // BogWaterSteam
			12,  // PriestAlert
			0,   // HighScentAura     -- continuous emission, no per-burst count
			0,   // DevoutChannel     -- continuous emission
			0,   // BeggarStealthAura -- continuous emission
			10,  // ChildToolRefusal  -- small sharp red-X burst
		};

		// Vertical offset added to burst-position Y. PriestAlert needs
		// to clear the priest's head (~1.8 m); other effects sit at
		// ground level (forge top, chest top, etc).
		constexpr float kKindHeightOffset[kNumKinds] = {
			0.8f,  // ForgeSparks       (forge body height)
			0.5f,  // DoorOpenDust      (mid-door)
			1.0f,  // DoorLockRejected  (eye level so player notices)
			0.6f,  // ChestOpenDust     (lid level)
			0.3f,  // PentagramRitual   (just above pentagram plane)
			1.0f,  // BellSoulRing      (eye level)
			0.1f,  // BogWaterSteam     (puddle level)
			2.2f,  // PriestAlert       (above priest head; priest ~1.8 m)
			0.9f,  // HighScentAura     (mid-body; the demonic stain reads as torso-emitted)
			1.2f,  // DevoutChannel     (above villager head; candlelight reads as halo)
			0.7f,  // BeggarStealthAura (mid-body; subtle ground-level haze)
			1.1f,  // ChildToolRefusal  (chest level; the X "blocks" the item visually)
		};

		Flux_ParticleEmitterConfig* g_apxConfigs[kNumKinds] = { nullptr };
		Zenith_EntityID g_axEmitterEntities[kNumKinds];

		// Subscription handles -- stored so Shutdown can unsubscribe
		// cleanly without leaving stale lambdas in the dispatcher.
		Zenith_EventHandle g_xSubInteract             = INVALID_EVENT_HANDLE;
		Zenith_EventHandle g_xSubDoorOpened           = INVALID_EVENT_HANDLE;
		Zenith_EventHandle g_xSubDoorLockRejected     = INVALID_EVENT_HANDLE;
		Zenith_EventHandle g_xSubChestOpened          = INVALID_EVENT_HANDLE;
		Zenith_EventHandle g_xSubForgeCrafted         = INVALID_EVENT_HANDLE;
		Zenith_EventHandle g_xSubObjectivePlaced      = INVALID_EVENT_HANDLE;
		Zenith_EventHandle g_xSubBellRing             = INVALID_EVENT_HANDLE;
		Zenith_EventHandle g_xSubItemEvaporated       = INVALID_EVENT_HANDLE;
		Zenith_EventHandle g_xSubPriestAlerted        = INVALID_EVENT_HANDLE;
		Zenith_EventHandle g_xSubChildToolRefused     = INVALID_EVENT_HANDLE;

		// Test accessor backing storage.
		uint32_t g_auBurstCounts[kNumKinds] = { 0 };

		bool g_bInitialized = false;

		// -----------------------------------------------------------------
		// Config construction helpers. Each constructs a tuned
		// Flux_ParticleEmitterConfig matching the visual semantic of one
		// Kind. All CPU-simulated (small particle counts; no GPU benefit).
		//
		// Per-emitter constants (lifetimes / colors / spawn rates / spread
		// angles / sizes) live inline rather than in Config/Tuning.json by
		// design: they are visual *design data* for one effect each, not
		// gameplay knobs that designers tune across runs. Routing ~180
		// values through DP_Tuning::Get<float> would bloat Tuning.json into
		// an unreadable wall of `particles.forge_sparks.lifetime_min_s = 0.25`
		// keys and make each Make*() function less readable, with no
		// practical iteration win -- each value is hand-tuned once when
		// the visual is authored and rarely revisited.
		//
		// Gameplay-affecting values (e.g. priest hearing radius, life
		// timer) DO go through DP_Tuning; particle visuals do not. If
		// this ever changes -- e.g. accessibility settings adjust effect
		// intensity -- the right move is to add a small set of cross-
		// emitter multipliers (overall_intensity, etc.) to DP_Tuning, not
		// to migrate per-value.
		// -----------------------------------------------------------------
		Flux_ParticleEmitterConfig* MakeForgeSparks()
		{
			auto* p = new Flux_ParticleEmitterConfig();
			p->m_fSpawnRate         = 0.0f;          // burst-only
			p->m_uBurstCount        = 0;
			p->m_uMaxParticles      = 64;
			p->m_fLifetimeMin       = 0.25f;
			p->m_fLifetimeMax       = 0.55f;
			p->m_xEmitDirection     = { 0.0f, 1.0f, 0.0f };
			p->m_fSpreadAngleDegrees = 75.0f;        // wide cone (sparks fly)
			p->m_fSpeedMin          = 2.0f;
			p->m_fSpeedMax          = 5.5f;
			p->m_xGravity           = { 0.0f, -8.0f, 0.0f };
			p->m_fDrag              = 1.5f;
			p->m_xColorStart        = { 1.0f, 0.85f, 0.30f, 1.0f };  // bright yellow-orange
			p->m_xColorEnd          = { 1.0f, 0.35f, 0.10f, 0.0f };  // deep orange, fade
			p->m_fSizeStart         = 0.08f;
			p->m_fSizeEnd           = 0.02f;
			p->m_bAdditiveBlending  = true;
			p->m_bUseGPUCompute     = false;
			return p;
		}

		Flux_ParticleEmitterConfig* MakeDoorOpenDust()
		{
			auto* p = new Flux_ParticleEmitterConfig();
			p->m_fSpawnRate         = 0.0f;
			p->m_uBurstCount        = 0;
			p->m_uMaxParticles      = 48;
			p->m_fLifetimeMin       = 0.4f;
			p->m_fLifetimeMax       = 0.9f;
			p->m_fSpawnRadius       = 0.3f;
			p->m_xEmitDirection     = { 0.0f, 1.0f, 0.0f };
			p->m_fSpreadAngleDegrees = 50.0f;
			p->m_fSpeedMin          = 0.4f;
			p->m_fSpeedMax          = 1.2f;
			p->m_xGravity           = { 0.0f, -0.8f, 0.0f };
			p->m_fDrag              = 2.0f;
			p->m_xColorStart        = { 0.65f, 0.55f, 0.40f, 0.7f };  // pale dusty brown
			p->m_xColorEnd          = { 0.65f, 0.55f, 0.40f, 0.0f };
			p->m_fSizeStart         = 0.15f;
			p->m_fSizeEnd           = 0.30f;
			p->m_bAdditiveBlending  = false;
			p->m_bUseGPUCompute     = false;
			return p;
		}

		Flux_ParticleEmitterConfig* MakeDoorLockRejected()
		{
			auto* p = new Flux_ParticleEmitterConfig();
			p->m_fSpawnRate         = 0.0f;
			p->m_uBurstCount        = 0;
			p->m_uMaxParticles      = 16;
			p->m_fLifetimeMin       = 0.20f;
			p->m_fLifetimeMax       = 0.40f;
			p->m_fSpawnRadius       = 0.2f;
			p->m_xEmitDirection     = { 0.0f, 0.0f, -1.0f };   // burst toward player
			p->m_fSpreadAngleDegrees = 90.0f;
			p->m_fSpeedMin          = 0.3f;
			p->m_fSpeedMax          = 1.0f;
			p->m_xGravity           = { 0.0f, -2.0f, 0.0f };
			p->m_fDrag              = 4.0f;
			p->m_xColorStart        = { 0.90f, 0.20f, 0.15f, 1.0f };  // alarm red
			p->m_xColorEnd          = { 0.50f, 0.10f, 0.10f, 0.0f };
			p->m_fSizeStart         = 0.12f;
			p->m_fSizeEnd           = 0.06f;
			p->m_bAdditiveBlending  = true;
			p->m_bUseGPUCompute     = false;
			return p;
		}

		Flux_ParticleEmitterConfig* MakeChestOpenDust()
		{
			auto* p = new Flux_ParticleEmitterConfig();
			p->m_fSpawnRate         = 0.0f;
			p->m_uBurstCount        = 0;
			p->m_uMaxParticles      = 48;
			p->m_fLifetimeMin       = 0.4f;
			p->m_fLifetimeMax       = 1.0f;
			p->m_fSpawnRadius       = 0.4f;
			p->m_xEmitDirection     = { 0.0f, 1.0f, 0.0f };
			p->m_fSpreadAngleDegrees = 65.0f;
			p->m_fSpeedMin          = 0.5f;
			p->m_fSpeedMax          = 1.4f;
			p->m_xGravity           = { 0.0f, -0.6f, 0.0f };
			p->m_fDrag              = 1.8f;
			p->m_xColorStart        = { 0.70f, 0.60f, 0.45f, 0.7f };  // slightly more golden than door dust
			p->m_xColorEnd          = { 0.70f, 0.60f, 0.45f, 0.0f };
			p->m_fSizeStart         = 0.18f;
			p->m_fSizeEnd           = 0.30f;
			p->m_bAdditiveBlending  = false;
			p->m_bUseGPUCompute     = false;
			return p;
		}

		Flux_ParticleEmitterConfig* MakePentagramRitual()
		{
			auto* p = new Flux_ParticleEmitterConfig();
			p->m_fSpawnRate         = 0.0f;
			p->m_uBurstCount        = 0;
			p->m_uMaxParticles      = 96;
			p->m_fLifetimeMin       = 0.6f;
			p->m_fLifetimeMax       = 1.4f;
			p->m_fSpawnRadius       = 1.0f;          // ritual circle radius
			p->m_xEmitDirection     = { 0.0f, 1.0f, 0.0f };
			p->m_fSpreadAngleDegrees = 30.0f;
			p->m_fSpeedMin          = 1.2f;
			p->m_fSpeedMax          = 2.5f;
			p->m_xGravity           = { 0.0f, -0.3f, 0.0f };
			p->m_fDrag              = 1.2f;
			p->m_xColorStart        = { 0.65f, 0.20f, 0.80f, 1.0f };  // deep violet
			p->m_xColorEnd          = { 0.95f, 0.40f, 1.00f, 0.0f };  // ritualistic pink-white fade
			p->m_fSizeStart         = 0.15f;
			p->m_fSizeEnd           = 0.04f;
			p->m_fRotationSpeedMin  = -3.0f;
			p->m_fRotationSpeedMax  = 3.0f;
			p->m_bAdditiveBlending  = true;
			p->m_fTurbulence        = 0.8f;
			p->m_bUseGPUCompute     = false;
			return p;
		}

		Flux_ParticleEmitterConfig* MakeBellSoulRing()
		{
			auto* p = new Flux_ParticleEmitterConfig();
			p->m_fSpawnRate         = 0.0f;
			p->m_uBurstCount        = 0;
			p->m_uMaxParticles      = 96;
			p->m_fLifetimeMin       = 0.8f;
			p->m_fLifetimeMax       = 1.4f;
			p->m_fSpawnRadius       = 0.5f;
			p->m_xEmitDirection     = { 0.0f, 0.05f, 0.0f };
			p->m_fSpreadAngleDegrees = 180.0f;       // emit in all directions (radial ring)
			p->m_fSpeedMin          = 4.0f;
			p->m_fSpeedMax          = 8.0f;
			p->m_xGravity           = { 0.0f, 0.0f, 0.0f };
			p->m_fDrag              = 2.5f;
			p->m_xColorStart        = { 1.0f, 0.85f, 0.40f, 1.0f };   // bell-gold
			p->m_xColorEnd          = { 1.0f, 0.75f, 0.25f, 0.0f };
			p->m_fSizeStart         = 0.12f;
			p->m_fSizeEnd           = 0.40f;          // expand outward
			p->m_bAdditiveBlending  = true;
			p->m_bUseGPUCompute     = false;
			return p;
		}

		Flux_ParticleEmitterConfig* MakeBogWaterSteam()
		{
			auto* p = new Flux_ParticleEmitterConfig();
			p->m_fSpawnRate         = 0.0f;
			p->m_uBurstCount        = 0;
			p->m_uMaxParticles      = 32;
			p->m_fLifetimeMin       = 1.0f;
			p->m_fLifetimeMax       = 2.0f;
			p->m_fSpawnRadius       = 0.25f;
			p->m_xEmitDirection     = { 0.0f, 1.0f, 0.0f };
			p->m_fSpreadAngleDegrees = 25.0f;        // narrow rising plume
			p->m_fSpeedMin          = 0.4f;
			p->m_fSpeedMax          = 1.0f;
			p->m_xGravity           = { 0.0f, 0.3f, 0.0f };  // gentle upward
			p->m_fDrag              = 1.0f;
			p->m_xColorStart        = { 0.85f, 0.90f, 0.85f, 0.6f };  // pale grey-green steam
			p->m_xColorEnd          = { 0.95f, 0.95f, 0.95f, 0.0f };
			p->m_fSizeStart         = 0.20f;
			p->m_fSizeEnd           = 0.55f;
			p->m_bAdditiveBlending  = false;
			p->m_fTurbulence        = 0.5f;
			p->m_bUseGPUCompute     = false;
			return p;
		}

		Flux_ParticleEmitterConfig* MakeHighScentAura()
		{
			// Continuous-emission emitter, NOT burst-based. Lives around
			// the highest-scent villager while their scent is above the
			// hound-bark threshold (0.5 by Tuning.json default). Soft
			// violet/black smoke wisps reading as "this body smells of
			// demon."
			auto* p = new Flux_ParticleEmitterConfig();
			p->m_fSpawnRate         = 18.0f;          // continuous trickle
			p->m_uBurstCount        = 0;
			p->m_uMaxParticles      = 64;
			p->m_fLifetimeMin       = 0.8f;
			p->m_fLifetimeMax       = 1.6f;
			p->m_fSpawnRadius       = 0.40f;          // body-width column
			p->m_xEmitDirection     = { 0.0f, 1.0f, 0.0f };
			p->m_fSpreadAngleDegrees = 35.0f;
			p->m_fSpeedMin          = 0.5f;
			p->m_fSpeedMax          = 1.2f;
			p->m_xGravity           = { 0.0f, 0.4f, 0.0f };  // gentle rise (smoke)
			p->m_fDrag              = 0.8f;
			p->m_xColorStart        = { 0.55f, 0.25f, 0.75f, 0.55f }; // violet
			p->m_xColorEnd          = { 0.30f, 0.10f, 0.40f, 0.0f };  // fade darker
			p->m_fSizeStart         = 0.18f;
			p->m_fSizeEnd           = 0.40f;
			p->m_bAdditiveBlending  = true;
			p->m_fTurbulence        = 0.6f;
			p->m_bUseGPUCompute     = false;
			return p;
		}

		Flux_ParticleEmitterConfig* MakePriestAlert()
		{
			auto* p = new Flux_ParticleEmitterConfig();
			p->m_fSpawnRate         = 0.0f;
			p->m_uBurstCount        = 0;
			p->m_uMaxParticles      = 32;
			p->m_fLifetimeMin       = 0.5f;
			p->m_fLifetimeMax       = 1.0f;
			p->m_fSpawnRadius       = 0.05f;          // tight column above head
			p->m_xEmitDirection     = { 0.0f, 1.0f, 0.0f };
			p->m_fSpreadAngleDegrees = 8.0f;          // very narrow -- reads as one stack
			p->m_fSpeedMin          = 1.5f;
			p->m_fSpeedMax          = 2.5f;
			p->m_xGravity           = { 0.0f, -1.5f, 0.0f };
			p->m_fDrag              = 0.5f;
			p->m_xColorStart        = { 1.0f, 0.20f, 0.20f, 1.0f };  // bright alarm red
			p->m_xColorEnd          = { 1.0f, 0.10f, 0.05f, 0.0f };
			p->m_fSizeStart         = 0.18f;
			p->m_fSizeEnd           = 0.12f;
			p->m_bAdditiveBlending  = true;
			p->m_bUseGPUCompute     = false;
			return p;
		}

		Flux_ParticleEmitterConfig* MakeDevoutChannel()
		{
			// Continuous emission while the player channels a Devout
			// possession. Soft yellow / candlelight motes rising slowly
			// around the channel target -- reads as ritualistic charge.
			auto* p = new Flux_ParticleEmitterConfig();
			p->m_fSpawnRate         = 26.0f;
			p->m_uBurstCount        = 0;
			p->m_uMaxParticles      = 72;
			p->m_fLifetimeMin       = 0.6f;
			p->m_fLifetimeMax       = 1.1f;
			p->m_fSpawnRadius       = 0.5f;
			p->m_xEmitDirection     = { 0.0f, 1.0f, 0.0f };
			p->m_fSpreadAngleDegrees = 25.0f;
			p->m_fSpeedMin          = 0.8f;
			p->m_fSpeedMax          = 1.4f;
			p->m_xGravity           = { 0.0f, 0.6f, 0.0f };
			p->m_fDrag              = 0.4f;
			p->m_xColorStart        = { 1.0f, 0.92f, 0.55f, 0.9f };  // candleflame
			p->m_xColorEnd          = { 1.0f, 0.70f, 0.20f, 0.0f };
			p->m_fSizeStart         = 0.10f;
			p->m_fSizeEnd           = 0.05f;
			p->m_bAdditiveBlending  = true;
			p->m_bUseGPUCompute     = false;
			return p;
		}

		Flux_ParticleEmitterConfig* MakeBeggarStealthAura()
		{
			// Continuous, dim grey haze around a possessed Beggar.
			// Reads as "this villager is overlooked / unseen" -- the
			// player learns that this body is functionally invisible
			// to Aelfric.
			auto* p = new Flux_ParticleEmitterConfig();
			p->m_fSpawnRate         = 12.0f;
			p->m_uBurstCount        = 0;
			p->m_uMaxParticles      = 48;
			p->m_fLifetimeMin       = 1.0f;
			p->m_fLifetimeMax       = 1.8f;
			p->m_fSpawnRadius       = 0.50f;
			p->m_xEmitDirection     = { 0.0f, 1.0f, 0.0f };
			p->m_fSpreadAngleDegrees = 40.0f;
			p->m_fSpeedMin          = 0.2f;
			p->m_fSpeedMax          = 0.6f;
			p->m_xGravity           = { 0.0f, 0.0f, 0.0f };
			p->m_fDrag              = 1.5f;
			p->m_xColorStart        = { 0.55f, 0.55f, 0.55f, 0.35f }; // dim grey
			p->m_xColorEnd          = { 0.70f, 0.70f, 0.75f, 0.0f };
			p->m_fSizeStart         = 0.20f;
			p->m_fSizeEnd           = 0.45f;
			p->m_bAdditiveBlending  = false;
			p->m_fTurbulence        = 0.3f;
			p->m_bUseGPUCompute     = false;
			return p;
		}

		Flux_ParticleEmitterConfig* MakeChildToolRefusal()
		{
			// Sharp red-X-like burst when a Child villager's
			// auto-pickup is rejected. Bright, fast, brief -- meant to
			// read as "no, blocked".
			auto* p = new Flux_ParticleEmitterConfig();
			p->m_fSpawnRate         = 0.0f;
			p->m_uBurstCount        = 0;
			p->m_uMaxParticles      = 20;
			p->m_fLifetimeMin       = 0.25f;
			p->m_fLifetimeMax       = 0.45f;
			p->m_fSpawnRadius       = 0.10f;
			p->m_xEmitDirection     = { 0.0f, 1.0f, 0.0f };
			p->m_fSpreadAngleDegrees = 60.0f;
			p->m_fSpeedMin          = 1.2f;
			p->m_fSpeedMax          = 2.5f;
			p->m_xGravity           = { 0.0f, -3.0f, 0.0f };
			p->m_fDrag              = 1.0f;
			p->m_xColorStart        = { 0.95f, 0.25f, 0.20f, 1.0f };  // alarm red
			p->m_xColorEnd          = { 0.50f, 0.10f, 0.10f, 0.0f };
			p->m_fSizeStart         = 0.10f;
			p->m_fSizeEnd           = 0.04f;
			p->m_bAdditiveBlending  = true;
			p->m_bUseGPUCompute     = false;
			return p;
		}

		// Build all eight configs in the right slot. Caller owns
		// deallocation via Shutdown.
		void BuildAllConfigs()
		{
			g_apxConfigs[static_cast<int>(Kind::ForgeSparks)]       = MakeForgeSparks();
			g_apxConfigs[static_cast<int>(Kind::DoorOpenDust)]      = MakeDoorOpenDust();
			g_apxConfigs[static_cast<int>(Kind::DoorLockRejected)]  = MakeDoorLockRejected();
			g_apxConfigs[static_cast<int>(Kind::ChestOpenDust)]     = MakeChestOpenDust();
			g_apxConfigs[static_cast<int>(Kind::PentagramRitual)]   = MakePentagramRitual();
			g_apxConfigs[static_cast<int>(Kind::BellSoulRing)]      = MakeBellSoulRing();
			g_apxConfigs[static_cast<int>(Kind::BogWaterSteam)]     = MakeBogWaterSteam();
			g_apxConfigs[static_cast<int>(Kind::PriestAlert)]       = MakePriestAlert();
			g_apxConfigs[static_cast<int>(Kind::HighScentAura)]     = MakeHighScentAura();
			g_apxConfigs[static_cast<int>(Kind::DevoutChannel)]     = MakeDevoutChannel();
			g_apxConfigs[static_cast<int>(Kind::BeggarStealthAura)] = MakeBeggarStealthAura();
			g_apxConfigs[static_cast<int>(Kind::ChildToolRefusal)]  = MakeChildToolRefusal();

			for (int i = 0; i < kNumKinds; ++i)
			{
				Flux_ParticleEmitterConfig::Register(kKindNames[i], g_apxConfigs[i]);
			}
		}

		void TearDownAllConfigs()
		{
			for (int i = 0; i < kNumKinds; ++i)
			{
				Flux_ParticleEmitterConfig::Unregister(kKindNames[i]);
				delete g_apxConfigs[i];
				g_apxConfigs[i] = nullptr;
			}
		}

		// -----------------------------------------------------------------
		// Per-event handlers. Each is a tiny function-pointer-friendly
		// translator from a DP_On<Foo> event to a Burst() call. Designed
		// to be cheap on the dispatcher hot path -- no allocations, no
		// scene queries beyond the entity-position lookup.
		// -----------------------------------------------------------------
		void OnDoorOpened(const DP_OnDoorOpened& xEv)
		{
			BurstAtEntity(Kind::DoorOpenDust, xEv.m_xDoor);
		}

		void OnDoorLockRejected(const DP_OnDoorLockRejected& xEv)
		{
			BurstAtEntity(Kind::DoorLockRejected, xEv.m_xDoor);
		}

		void OnChestOpened(const DP_OnChestOpened& xEv)
		{
			BurstAtEntity(Kind::ChestOpenDust, xEv.m_xChest);
		}

		void OnForgeCrafted(const DP_OnForgeCrafted& xEv)
		{
			BurstAtEntity(Kind::ForgeSparks, xEv.m_xForge);
		}

		void OnObjectivePlaced(const DP_OnObjectivePlaced& xEv)
		{
			BurstAtEntity(Kind::PentagramRitual, xEv.m_xPentagram);
		}

		void OnBellRing(const DP_OnBellRing& xEv)
		{
			Burst(Kind::BellSoulRing, xEv.m_xPosition);
		}

		void OnItemEvaporated(const DP_OnItemEvaporated& xEv)
		{
			// Only BogWater for MVP, but the kind table is keyed by
			// special_behaviour so post-MVP reagents that also
			// evaporate produce the same steam burst without code
			// changes here. (DP_Reagents config controls the
			// special_behaviour string.)
			Burst(Kind::BogWaterSteam, xEv.m_xPosition);
		}

		void OnPriestAlerted(const DP_OnPriestAlerted& xEv)
		{
			// All three alert kinds produce the same "!" -- the kind
			// distinction is for the HUD (different awareness state
			// glyph), not the world telegraph.
			Burst(Kind::PriestAlert, xEv.m_xPosition);
		}

		void OnChildToolRefused(const DP_OnChildToolRefused& xEv)
		{
			Burst(Kind::ChildToolRefusal, xEv.m_xPosition);
		}

		// Shared implementation for the continuous-emission per-villager
		// auras (HighScent / DevoutChannel / BeggarStealth). Kept
		// distinct from Burst() because the trigger condition is
		// per-frame state, not an event.
		void UpdateContinuousAura(Kind eKind, Zenith_EntityID xVillager, bool bShow)
		{
			if (!g_bInitialized) return;
			const int iKind = static_cast<int>(eKind);
			if (iKind < 0 || iKind >= kNumKinds) return;
			Zenith_EntityID xEmitterId = g_axEmitterEntities[iKind];
			if (!xEmitterId.IsValid()) return;
			Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xEmitterId);
			if (pxScene == nullptr) return;
			Zenith_Entity xEnt = pxScene->TryGetEntity(xEmitterId);
			if (!xEnt.IsValid()) return;
			if (!xEnt.HasComponent<Zenith_ParticleEmitterComponent>()) return;
			Zenith_ParticleEmitterComponent& xEmitter =
				xEnt.GetComponent<Zenith_ParticleEmitterComponent>();
			const bool bWantEmit = bShow && xVillager.IsValid();
			if (!bWantEmit)
			{
				xEmitter.SetEmitting(false);
				return;
			}
			Zenith_SceneData* pxVilScene = g_xEngine.Scenes().GetSceneDataForEntity(xVillager);
			if (pxVilScene == nullptr)
			{
				xEmitter.SetEmitting(false);
				return;
			}
			Zenith_Entity xVilEnt = pxVilScene->TryGetEntity(xVillager);
			if (!xVilEnt.IsValid() || !xVilEnt.HasComponent<Zenith_TransformComponent>())
			{
				xEmitter.SetEmitting(false);
				return;
			}
			Zenith_Maths::Vector3 xPos;
			xVilEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
			xPos.y += kKindHeightOffset[iKind];
			xEmitter.SetEmitPosition(xPos);
			xEmitter.SetEmitting(true);
		}
	}

	// =====================================================================
	// Public API
	// =====================================================================

	void Initialize()
	{
		if (g_bInitialized) return;

		BuildAllConfigs();

		auto& xDispatcher = Zenith_EventDispatcher::Get();
		g_xSubDoorOpened          = xDispatcher.Subscribe<DP_OnDoorOpened>(&OnDoorOpened);
		g_xSubDoorLockRejected    = xDispatcher.Subscribe<DP_OnDoorLockRejected>(&OnDoorLockRejected);
		g_xSubChestOpened         = xDispatcher.Subscribe<DP_OnChestOpened>(&OnChestOpened);
		g_xSubForgeCrafted        = xDispatcher.Subscribe<DP_OnForgeCrafted>(&OnForgeCrafted);
		g_xSubObjectivePlaced     = xDispatcher.Subscribe<DP_OnObjectivePlaced>(&OnObjectivePlaced);
		g_xSubBellRing            = xDispatcher.Subscribe<DP_OnBellRing>(&OnBellRing);
		g_xSubItemEvaporated      = xDispatcher.Subscribe<DP_OnItemEvaporated>(&OnItemEvaporated);
		g_xSubPriestAlerted       = xDispatcher.Subscribe<DP_OnPriestAlerted>(&OnPriestAlerted);
		g_xSubChildToolRefused    = xDispatcher.Subscribe<DP_OnChildToolRefused>(&OnChildToolRefused);

		for (int i = 0; i < kNumKinds; ++i) g_auBurstCounts[i] = 0;
		for (int i = 0; i < kNumKinds; ++i) g_axEmitterEntities[i] = INVALID_ENTITY_ID;

		g_bInitialized = true;
	}

	void Shutdown()
	{
		if (!g_bInitialized) return;

		auto& xDispatcher = Zenith_EventDispatcher::Get();
		xDispatcher.Unsubscribe(g_xSubDoorOpened);
		xDispatcher.Unsubscribe(g_xSubDoorLockRejected);
		xDispatcher.Unsubscribe(g_xSubChestOpened);
		xDispatcher.Unsubscribe(g_xSubForgeCrafted);
		xDispatcher.Unsubscribe(g_xSubObjectivePlaced);
		xDispatcher.Unsubscribe(g_xSubBellRing);
		xDispatcher.Unsubscribe(g_xSubItemEvaporated);
		xDispatcher.Unsubscribe(g_xSubPriestAlerted);
		xDispatcher.Unsubscribe(g_xSubChildToolRefused);
		g_xSubDoorOpened          = INVALID_EVENT_HANDLE;
		g_xSubDoorLockRejected    = INVALID_EVENT_HANDLE;
		g_xSubChestOpened         = INVALID_EVENT_HANDLE;
		g_xSubForgeCrafted        = INVALID_EVENT_HANDLE;
		g_xSubObjectivePlaced     = INVALID_EVENT_HANDLE;
		g_xSubBellRing            = INVALID_EVENT_HANDLE;
		g_xSubItemEvaporated      = INVALID_EVENT_HANDLE;
		g_xSubPriestAlerted       = INVALID_EVENT_HANDLE;
		g_xSubChildToolRefused    = INVALID_EVENT_HANDLE;

		ClearEmitterEntities();
		TearDownAllConfigs();
		g_bInitialized = false;
	}

	void EnsureEmittersInScene()
	{
		if (!g_bInitialized) return;

		// Emitters live in the persistent scene so they survive
		// scene-switches (which is the same pattern Sokoban uses).
		// That way a between-tests scene reload doesn't strand the
		// emitter entities on the unloaded gameplay scene.
		Zenith_Scene xPersistent = g_xEngine.Scenes().GetPersistentScene();
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xPersistent);
		if (pxSceneData == nullptr) return;

		for (int i = 0; i < kNumKinds; ++i)
		{
			// Skip kinds whose emitter is still valid from a prior call.
			Zenith_Entity xExisting = pxSceneData->TryGetEntity(g_axEmitterEntities[i]);
			if (xExisting.IsValid()
				&& xExisting.HasComponent<Zenith_ParticleEmitterComponent>())
			{
				continue;
			}

			std::string strName = std::string("DPParticle_") + kKindNames[i];
			Zenith_Entity xEnt = g_xEngine.Scenes().CreateEntity(pxSceneData, strName.c_str());
			xEnt.AddComponent<Zenith_ParticleEmitterComponent>();
			Zenith_ParticleEmitterComponent& xEmitter =
				xEnt.GetComponent<Zenith_ParticleEmitterComponent>();
			xEmitter.SetConfig(g_apxConfigs[i]);
			xEmitter.SetEmitting(false);  // burst-only; never continuous

			g_axEmitterEntities[i] = xEnt.GetEntityID();
		}
	}

	void ClearEmitterEntities()
	{
		Zenith_Scene xPersistent = g_xEngine.Scenes().GetPersistentScene();
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xPersistent);
		for (int i = 0; i < kNumKinds; ++i)
		{
			if (pxSceneData != nullptr)
			{
				Zenith_Entity xEnt = pxSceneData->TryGetEntity(g_axEmitterEntities[i]);
				if (xEnt.IsValid())
				{
					xEnt.Destroy();
				}
			}
			g_axEmitterEntities[i] = INVALID_ENTITY_ID;
			g_auBurstCounts[i] = 0;
		}
	}

	void Burst(Kind eKind, const Zenith_Maths::Vector3& xWorldPos)
	{
		if (!g_bInitialized) return;
		const int iKind = static_cast<int>(eKind);
		if (iKind < 0 || iKind >= kNumKinds) return;

		// Resolve the emitter entity. The persistent scene is the
		// natural home for these (created in EnsureEmittersInScene),
		// but the lookup is scene-agnostic via GetSceneDataForEntity.
		Zenith_EntityID xId = g_axEmitterEntities[iKind];
		if (!xId.IsValid()) return;
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxSceneData == nullptr) return;
		Zenith_Entity xEnt = pxSceneData->TryGetEntity(xId);
		if (!xEnt.IsValid()) return;
		if (!xEnt.HasComponent<Zenith_ParticleEmitterComponent>()) return;

		Zenith_ParticleEmitterComponent& xEmitter =
			xEnt.GetComponent<Zenith_ParticleEmitterComponent>();

		Zenith_Maths::Vector3 xEmitPos = xWorldPos;
		xEmitPos.y += kKindHeightOffset[iKind];
		xEmitter.SetEmitPosition(xEmitPos);
		xEmitter.Emit(kKindBurstCount[iKind]);

		++g_auBurstCounts[iKind];
	}

	void BurstAtEntity(Kind eKind, Zenith_EntityID xEntity)
	{
		if (!xEntity.IsValid()) return;
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneDataForEntity(xEntity);
		if (pxSceneData == nullptr) return;
		Zenith_Entity xEnt = pxSceneData->TryGetEntity(xEntity);
		if (!xEnt.IsValid()) return;
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return;
		Zenith_Maths::Vector3 xPos;
		xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		Burst(eKind, xPos);
	}

	void UpdateHighScentAura(Zenith_EntityID xVillager, bool bShow)
	{
		UpdateContinuousAura(Kind::HighScentAura, xVillager, bShow);
	}

	void UpdateBeggarStealthAura(Zenith_EntityID xVillager, bool bShow)
	{
		UpdateContinuousAura(Kind::BeggarStealthAura, xVillager, bShow);
	}

	void UpdateDevoutChannelAura(Zenith_EntityID xVillager, bool bShow)
	{
		UpdateContinuousAura(Kind::DevoutChannel, xVillager, bShow);
	}

	uint32_t GetBurstCountForTest(Kind eKind)
	{
		const int i = static_cast<int>(eKind);
		if (i < 0 || i >= kNumKinds) return 0;
		return g_auBurstCounts[i];
	}

	Zenith_EntityID GetEmitterEntityForTest(Kind eKind)
	{
		const int i = static_cast<int>(eKind);
		if (i < 0 || i >= kNumKinds) return INVALID_ENTITY_ID;
		return g_axEmitterEntities[i];
	}

	void ResetBurstCountsForTest()
	{
		for (int i = 0; i < kNumKinds; ++i) g_auBurstCounts[i] = 0;
	}
}
