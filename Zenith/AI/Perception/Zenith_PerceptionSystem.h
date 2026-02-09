#pragma once

#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"
#include "EntityComponent/Zenith_Entity.h"
#include <unordered_map>

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
 * Zenith_PerceptionSystem - Global perception manager
 *
 * Handles registration of AI agents, processing of perception senses,
 * and emission of stimuli (sounds, damage events).
 */
class Zenith_PerceptionSystem
{
public:
	// ========== System Lifecycle ==========

	static void Initialise();
	static void Shutdown();
	static void Update(float fDt);
	static void Update(float fDt, Zenith_SceneData& xScene);  // For testing with specific scene
	static void Reset();

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
		Zenith_EntityID xAttacker, float fDamage);

	// ========== Queries ==========

	/**
	 * Get all targets perceived by an agent
	 */
	static const Zenith_Vector<Zenith_PerceivedTarget>* GetPerceivedTargets(Zenith_EntityID xAgentID);

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
	static void DebugDrawAgent(Zenith_EntityID xAgentID,
		const Zenith_Maths::Vector3& xAgentPos,
		const Zenith_Maths::Vector3& xForward);
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

	static std::unordered_map<uint64_t, AgentPerceptionData> s_xAgentData;
	static std::unordered_map<uint64_t, TargetInfo> s_xTargets;
	static Zenith_Vector<Zenith_SoundStimulus> s_axActiveSounds;

	// Update helpers
	static void UpdateSightPerception(float fDt, Zenith_SceneData& xScene);
	static void UpdateHearingPerception(float fDt);
	static void UpdateMemoryDecay(float fDt);
	static void UpdateActiveSounds(float fDt);

	// Sight helpers
	static bool CheckLineOfSight(const Zenith_Maths::Vector3& xFrom,
		const Zenith_Maths::Vector3& xTo, Zenith_SceneData& xScene);
	static float CalculateAngle(const Zenith_Maths::Vector3& xFrom,
		const Zenith_Maths::Vector3& xForward, const Zenith_Maths::Vector3& xTo);

	// Target lookup helpers
	static Zenith_PerceivedTarget* FindOrCreateTarget(AgentPerceptionData& xData,
		Zenith_EntityID xTargetID);
	static void UpdatePrimaryTarget(AgentPerceptionData& xData);
};
