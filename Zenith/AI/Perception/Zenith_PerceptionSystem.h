#pragma once

#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_Scene.h"

class Zenith_SceneData;

/**
 * Stimulus mask bits for perception types
 */
enum PerceptionStimulusType : uint32_t
{
	PERCEPTION_STIMULUS_NONE = 0,
	PERCEPTION_STIMULUS_SIGHT = 1 << 0,
	PERCEPTION_STIMULUS_HEARING = 1 << 1,
	PERCEPTION_STIMULUS_DAMAGE = 1 << 2,

	PERCEPTION_STIMULUS_ALL = PERCEPTION_STIMULUS_SIGHT | PERCEPTION_STIMULUS_HEARING | PERCEPTION_STIMULUS_DAMAGE
};

/**
 * Zenith_PerceivedTarget - Information about a perceived entity
 */
struct Zenith_PerceivedTarget
{
	Zenith_EntityID m_xEntityID;
	Zenith_Maths::Vector3 m_xLastKnownPosition;
	Zenith_Maths::Vector3 m_xEstimatedVelocity;
	float m_fTimeSinceLastSeen = 0.0f;
	float m_fAwareness = 0.0f;           // 0 = unaware, 1 = fully aware
	bool m_bCurrentlyVisible = false;
	uint32_t m_uStimulusMask = 0;        // Which senses detected this target
	bool m_bHostile = false;
};

/**
 * Zenith_SightConfig - Configuration for sight perception
 */
struct Zenith_SightConfig
{
	float m_fMaxRange = 30.0f;              // Maximum sight distance
	float m_fFOVAngle = 90.0f;              // Primary FOV in degrees
	float m_fPeripheralAngle = 120.0f;      // Peripheral FOV in degrees
	float m_fPeripheralMultiplier = 0.5f;   // Awareness gain in peripheral vision
	float m_fEyeHeight = 1.6f;              // Eye height offset from position
	bool m_bRequireLineOfSight = true;      // Perform LOS raycasts
	float m_fAwarenessGainRate = 2.0f;      // Awareness gain per second when visible
	float m_fAwarenessDecayRate = 0.5f;     // Awareness loss per second when not visible
};

/**
 * Zenith_HearingConfig - Configuration for hearing perception
 */
struct Zenith_HearingConfig
{
	float m_fMaxRange = 20.0f;              // Maximum hearing distance
	float m_fLoudnessThreshold = 0.1f;      // Minimum loudness to detect
	bool m_bCheckOcclusion = false;         // Check for wall occlusion
};

/**
 * Zenith_SoundStimulus - A sound event in the world
 */
struct Zenith_SoundStimulus
{
	Zenith_Maths::Vector3 m_xPosition;
	float m_fLoudness = 1.0f;               // Base loudness
	float m_fRadius = 10.0f;                // Propagation radius
	Zenith_EntityID m_xSourceEntity;
	float m_fTimeRemaining = 0.0f;          // How long the sound persists
};

/**
 * Zenith_PerceptionSystem - perception manager
 *
 * Handles registration of AI agents, processing of perception senses,
 * and emission of stimuli (sounds, damage events).
 *
 * OWNERSHIP: perception state is SCENE-OWNED, not process-owned. Every agent,
 * target and in-flight sound lives in a bucket keyed by the owning scene, so it
 * is destroyed with that scene -- the per-World-subsystem model (Unreal's
 * UWorldSubsystem, Unity's per-Scene state). Nothing here outlives the world it
 * describes, which is what makes a scene reload a genuine clean slate rather
 * than something a caller has to remember to reset by hand.
 *
 * Registration order IS iteration order: the buckets hold registration-ordered
 * vectors, not hash maps. A hash map's walk order depends on its insert/remove/
 * rehash HISTORY, so a predecessor's registrations could reorder a later,
 * cleanly-registered agent's update walk -- a silent cross-test determinism leak
 * even when every entry was unregistered correctly.
 *
 * Queries and Update span EVERY loaded scene's bucket: additively-loaded scenes
 * form one perceptual world (an agent in scene A can see a target in scene B),
 * matching the cross-scene contract the sight/hearing passes already had.
 */
class Zenith_PerceptionSystem
{
public:
	// ========== System Lifecycle ==========

	static void Initialise();
	static void Shutdown();
	// Ticks sight / hearing / damage / memory-decay for every registered agent.
	// Each agent resolves its owning scene internally via GetSceneDataForEntity,
	// so cross-scene perception (agents in additive or persistent scenes) works
	// without a scene argument — matching Unity's GameObject.scene intrinsic.
	static void Update(float fDt);
	static void Reset();

	// ========== Scene-lifecycle notifications ==========
	//
	// Driven by the ECS runtime hooks (Zenith_ECSRuntimeHooks::
	// m_pfnSceneDestroyed / m_pfnEntityOwnerSceneChanged), which the engine
	// installs. The ECS leaf sits BELOW the AI leaf and so cannot name this
	// class; the engine forwards. Calling them directly is legitimate in tests.

	// A scene is going away: drop its bucket, then sweep every surviving agent's
	// perceived-target list of entries whose entity no longer resolves, and
	// re-derive that agent's primary target (an unswept primary would otherwise
	// stay pointing at a dead entity until the next Update tick -- and if the
	// agent never ticks again, forever).
	static void OnSceneDestroyed(Zenith_Scene xScene);

	// An entity changed owning scene (MoveToScene / DontDestroyOnLoad). Migrate
	// its agent + target records so a persistent agent registered before the
	// move is genuinely owned by the persistent scene and survives SINGLE loads.
	static void OnEntityOwnerSceneChanged(Zenith_EntityID xEntityID, Zenith_Scene xOldScene, Zenith_Scene xNewScene);

	// ========== Agent Registration ==========

	static void RegisterAgent(Zenith_EntityID xAgentID);
	static void UnregisterAgent(Zenith_EntityID xAgentID);

	// ========== Configuration ==========

	static void SetSightConfig(Zenith_EntityID xAgentID, const Zenith_SightConfig& xConfig);
	static void SetHearingConfig(Zenith_EntityID xAgentID, const Zenith_HearingConfig& xConfig);

	// ========== Stimulus Emission ==========

	/**
	 * Emit a sound at a location
	 */
	static void EmitSoundStimulus(const Zenith_Maths::Vector3& xPosition,
		float fLoudness, float fRadius, Zenith_EntityID xSource);

	/**
	 * Emit a damage event (immediate awareness of attacker)
	 */
	static void EmitDamageStimulus(Zenith_EntityID xVictim,
		Zenith_EntityID xAttacker);

	// ========== Queries ==========

	/**
	 * Get all targets perceived by an agent
	 */
	static const Zenith_Vector<Zenith_PerceivedTarget>* GetPerceivedTargets(Zenith_EntityID xAgentID);

	/**
	 * EXT-6: typed accessor for the most-recent hearing stimulus delivered to
	 * an agent. Returned `m_bValid == false` when the agent has no
	 * hearing-flagged perceived targets.
	 *
	 * Disambiguates sound vs sight using each PerceivedTarget's
	 * m_uStimulusMask (PERCEPTION_STIMULUS_HEARING bit). Among matching
	 * entries, picks the smallest m_fTimeSinceLastSeen as the freshest.
	 *
	 * Used by DevilsPlayground's Priest_Behaviour to bridge perception
	 * stimuli into BB.InvestigatePos / BB.HasInvestigatePos. The plan
	 * argued for a per-agent side-channel populated inside
	 * UpdateHearingPerception, but the perceived-targets list already
	 * carries the data — this wrapper just exposes it typed.
	 */
	struct Zenith_LastHeardSound
	{
		bool                  m_bValid       = false;
		Zenith_Maths::Vector3 m_xPosition    = Zenith_Maths::Vector3(0.0f);
		Zenith_EntityID       m_xSourceEntity;
		float                 m_fAge         = 0.0f;
	};
	static Zenith_LastHeardSound GetLastHeardSoundFor(Zenith_EntityID xAgentID);

	/**
	 * Get the primary (highest awareness) target for an agent
	 */
	static Zenith_EntityID GetPrimaryTarget(Zenith_EntityID xAgentID);

	/**
	 * Check if an agent is aware of a specific entity
	 */
	static bool IsAwareOf(Zenith_EntityID xAgentID, Zenith_EntityID xTargetID);

	/**
	 * Get awareness level of a specific target (0 = unknown, 1 = fully aware)
	 */
	static float GetAwarenessOf(Zenith_EntityID xAgentID, Zenith_EntityID xTargetID);

	// ========== Target Management ==========

	/**
	 * Register an entity as a potential target (enemies/players)
	 */
	static void RegisterTarget(Zenith_EntityID xTargetID, bool bHostile = true);
	static void UnregisterTarget(Zenith_EntityID xTargetID);

	/**
	 * Mark entity as hostile/friendly
	 */
	static void SetTargetHostile(Zenith_EntityID xTargetID, bool bHostile);

#ifdef ZENITH_TOOLS
	// Draw every registered agent's perception state, resolving each agent's pose
	// through the AI world hooks. Sections are gated individually by the
	// AI/Perception/* debug variables; called once per game-logic frame by
	// Zenith_AI::DebugDraw(). Prefer this over DebugDrawAgent -- the single-agent
	// form exists for callers that already hold a pose.
	static void DebugDrawAllAgents();
	static void DebugDrawAgent(Zenith_EntityID xAgentID,
		const Zenith_Maths::Vector3& xAgentPos,
		const Zenith_Maths::Vector3& xForward);
#endif

#ifdef ZENITH_TESTING
	// Observation seams for the scene-ownership tests. Counts are summed across
	// every live bucket; GetAgentIterationOrderForTest reports the exact walk
	// order Update uses, which is what pins "registration order, not container
	// history".
	static uint32_t GetAgentCountForTest();
	static uint32_t GetTargetCountForTest();
	static uint32_t GetActiveSoundCountForTest();
	static uint32_t GetLiveSceneBucketCountForTest();
	static uint32_t GetAgentCountForSceneForTest(Zenith_Scene xScene);
	static void     GetAgentIterationOrderForTest(Zenith_Vector<Zenith_EntityID>& axOut);
#endif

private:
	// Per-agent perception data
	struct AgentPerceptionData
	{
		Zenith_SightConfig m_xSightConfig;
		Zenith_HearingConfig m_xHearingConfig;
		Zenith_Vector<Zenith_PerceivedTarget> m_axPerceivedTargets;
		Zenith_EntityID m_xPrimaryTarget;
	};

	// Registered potential targets
	struct TargetInfo
	{
		bool m_bHostile = true;
	};

	// Registration records. Deliberately vectors keyed by a linear scan rather
	// than hash-map entries: agent/target counts are small (tens), and the
	// linear scan buys deterministic iteration that no hash container can offer.
	struct AgentRecord
	{
		Zenith_EntityID     m_xAgentID;
		AgentPerceptionData m_xData;
	};

	struct TargetRecord
	{
		Zenith_EntityID m_xTargetID;
		TargetInfo      m_xInfo;
	};

	// Everything perception owns for one scene. Freed wholesale when that scene
	// is destroyed, so no per-entry unregistration is required for correctness
	// (agents/targets still unregister themselves; this is the backstop that
	// makes a missed unregister a non-leak).
	struct ScenePerception
	{
		Zenith_Scene                        m_xScene = Zenith_Scene::INVALID_SCENE;
		Zenith_Vector<AgentRecord>          m_axAgents;
		Zenith_Vector<TargetRecord>         m_axTargets;
		Zenith_Vector<Zenith_SoundStimulus> m_axSounds;
	};

	// Indexed by scene handle; grows to the high-water mark of allocated scene
	// slots and never shrinks (slot count is tiny). A slot whose m_xScene is
	// INVALID_SCENE -- or whose stored handle no longer matches the live scene
	// of the same index -- is free.
	static Zenith_Vector<ScenePerception> s_axScenes;

	// Bucket resolution.
	static ScenePerception* FindSceneBucket(Zenith_Scene xScene);
	static ScenePerception& EnsureSceneBucket(Zenith_Scene xScene);
	static bool             IsBucketLive(const ScenePerception& xBucket);
	// Owning scene of an entity, or INVALID_SCENE when it cannot be resolved.
	static Zenith_Scene     ResolveOwningScene(Zenith_EntityID xEntityID);
	// The scene a NEW registration for this entity should land in: the entity's
	// own scene, falling back to the active scene so a registration is never
	// silently dropped.
	static Zenith_Scene     ResolveRegistrationScene(Zenith_EntityID xEntityID);
	// Whole-world record lookup: scans every live bucket. Used by the by-entity
	// API (config setters, queries, unregistration) so it stays correct even
	// when the entity is already half-destroyed and no longer resolves to a
	// scene -- which is exactly the state OnDestroy unregisters from.
	static AgentRecord*     FindAgentRecord(Zenith_EntityID xAgentID);
	static AgentPerceptionData* FindAgentData(Zenith_EntityID xAgentID);

	// Update helpers. Each agent/target resolves its own scene, so these helpers
	// take no scene parameter and span every loaded scene's bucket.
	static void UpdateSightPerception(float fDt);
	static void UpdateHearingPerception();
	static void UpdateMemoryDecay(float fDt);
	static void UpdateActiveSounds(float fDt);

	// Result of evaluating a single target for sight perception
	struct SightEvaluation
	{
		bool m_bVisible = false;
		bool m_bInPeripheral = false;
		float m_fDistance = 0.0f;
		float m_fDistanceFactor = 0.0f;  // 1.0 = close, 0.0 = at max range
		Zenith_Maths::Vector3 m_xTargetPos;
	};

	// Sight helpers
	static SightEvaluation EvaluateSightForTarget(
		const Zenith_Maths::Vector3& xAgentPos,
		const Zenith_Maths::Vector3& xForward,
		const Zenith_SightConfig& xConfig,
		Zenith_Entity& xTargetEntity);

	static bool CheckLineOfSight(const Zenith_Maths::Vector3& xFrom,
		const Zenith_Maths::Vector3& xTo);
	static float CalculateAngle(const Zenith_Maths::Vector3& xFrom,
		const Zenith_Maths::Vector3& xForward, const Zenith_Maths::Vector3& xTo);

	// Hearing helpers
	static bool EvaluateHearingForSound(
		const Zenith_Maths::Vector3& xAgentPos,
		const Zenith_HearingConfig& xConfig,
		const Zenith_SoundStimulus& xSound,
		float& fOutAwarenessGain);

	// Target lookup helpers
	static Zenith_PerceivedTarget* FindOrCreateTarget(AgentPerceptionData& xData,
		Zenith_EntityID xTargetID);
	static void UpdatePrimaryTarget(AgentPerceptionData& xData);
};
