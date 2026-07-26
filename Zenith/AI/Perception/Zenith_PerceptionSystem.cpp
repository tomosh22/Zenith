#include "Zenith.h"
#include "Profiling/Zenith_Profiling.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "AI/Zenith_AIWorldHooks.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Physics/Zenith_Physics.h"  // AI->Physics: sibling leaf

Zenith_Vector<Zenith_PerceptionSystem::ScenePerception> Zenith_PerceptionSystem::s_axScenes;

// ============================================================================
// Bucket resolution
// ============================================================================

bool Zenith_PerceptionSystem::IsBucketLive(const ScenePerception& xBucket)
{
	// A bucket is live only while its stored generation-checked handle still
	// names a loaded scene. Handle equality alone is not enough: scene slots are
	// recycled, so a stale bucket and a brand-new scene can share an index and
	// differ only in generation.
	return xBucket.m_xScene.IsValid();
}

Zenith_PerceptionSystem::ScenePerception* Zenith_PerceptionSystem::FindSceneBucket(Zenith_Scene xScene)
{
	const int iHandle = xScene.GetHandle();
	if (iHandle < 0 || static_cast<uint32_t>(iHandle) >= s_axScenes.GetSize())
	{
		return nullptr;
	}
	ScenePerception& xBucket = s_axScenes.Get(static_cast<uint32_t>(iHandle));
	// Full-handle compare (handle AND generation) so a recycled slot never
	// hands back the previous occupant's records.
	if (xBucket.m_xScene != xScene)
	{
		return nullptr;
	}
	return &xBucket;
}

Zenith_PerceptionSystem::ScenePerception& Zenith_PerceptionSystem::EnsureSceneBucket(Zenith_Scene xScene)
{
	const int iHandle = xScene.GetHandle();
	Zenith_Assert(iHandle >= 0, "EnsureSceneBucket: invalid scene handle");
	while (s_axScenes.GetSize() <= static_cast<uint32_t>(iHandle))
	{
		s_axScenes.PushBack(ScenePerception());
	}
	ScenePerception& xBucket = s_axScenes.Get(static_cast<uint32_t>(iHandle));
	if (xBucket.m_xScene != xScene)
	{
		// Claiming a slot whose previous occupant is gone. Anything still in it
		// belonged to a destroyed scene (OnSceneDestroyed normally clears it
		// first; this is the belt-and-braces path for a scene freed without the
		// hook wired, e.g. a leaf-only unit run).
		xBucket.m_axAgents.Clear();
		xBucket.m_axTargets.Clear();
		xBucket.m_axSounds.Clear();
		xBucket.m_xScene = xScene;
	}
	return xBucket;
}

Zenith_Scene Zenith_PerceptionSystem::ResolveOwningScene(Zenith_EntityID xEntityID)
{
	Zenith_Entity xEntity = Zenith_SceneSystem::Get().ResolveEntity(xEntityID);
	if (!xEntity.IsValid())
	{
		return Zenith_Scene::INVALID_SCENE;
	}
	return xEntity.GetScene();
}

Zenith_Scene Zenith_PerceptionSystem::ResolveRegistrationScene(Zenith_EntityID xEntityID)
{
	const Zenith_Scene xOwn = ResolveOwningScene(xEntityID);
	if (xOwn.IsValid())
	{
		return xOwn;
	}
	// The entity does not resolve (registered before its scene finished loading,
	// or a synthetic ID from a test). Fall back to the active scene so the
	// registration is never silently dropped -- it then dies with the active
	// scene, which is strictly better than outliving every world.
	return Zenith_SceneSystem::Get().GetActiveScene();
}

Zenith_PerceptionSystem::AgentRecord* Zenith_PerceptionSystem::FindAgentRecord(Zenith_EntityID xAgentID)
{
	for (uint32_t uScene = 0; uScene < s_axScenes.GetSize(); ++uScene)
	{
		ScenePerception& xBucket = s_axScenes.Get(uScene);
		if (!IsBucketLive(xBucket)) continue;
		for (uint32_t u = 0; u < xBucket.m_axAgents.GetSize(); ++u)
		{
			if (xBucket.m_axAgents.Get(u).m_xAgentID == xAgentID)
			{
				return &xBucket.m_axAgents.Get(u);
			}
		}
	}
	return nullptr;
}

Zenith_PerceptionSystem::AgentPerceptionData* Zenith_PerceptionSystem::FindAgentData(Zenith_EntityID xAgentID)
{
	AgentRecord* pxRecord = FindAgentRecord(xAgentID);
	return pxRecord ? &pxRecord->m_xData : nullptr;
}

// ============================================================================
// Lifecycle
// ============================================================================

void Zenith_PerceptionSystem::Initialise()
{
	s_axScenes.Clear();
	Zenith_Log(LOG_CATEGORY_AI, "PerceptionSystem initialized");
}

void Zenith_PerceptionSystem::Shutdown()
{
	s_axScenes.Clear();
}

void Zenith_PerceptionSystem::Reset()
{
	s_axScenes.Clear();
}

void Zenith_PerceptionSystem::OnSceneDestroyed(Zenith_Scene xScene)
{
	ScenePerception* pxBucket = FindSceneBucket(xScene);
	if (pxBucket != nullptr)
	{
		pxBucket->m_axAgents.Clear();
		pxBucket->m_axTargets.Clear();
		pxBucket->m_axSounds.Clear();
		pxBucket->m_xScene = Zenith_Scene::INVALID_SCENE;
	}

	// Sweep cross-references held by agents in OTHER (surviving) scenes: a
	// perceived-target memory naming an entity of the dead scene, and the
	// derived primary target. Without the re-derive, an agent whose primary
	// pointed into the dead scene keeps reporting it until its next Update tick
	// -- and a paused / never-again-ticked agent keeps it forever.
	for (uint32_t uScene = 0; uScene < s_axScenes.GetSize(); ++uScene)
	{
		ScenePerception& xBucket = s_axScenes.Get(uScene);
		if (!IsBucketLive(xBucket)) continue;
		for (uint32_t uAgent = 0; uAgent < xBucket.m_axAgents.GetSize(); ++uAgent)
		{
			AgentPerceptionData& xData = xBucket.m_axAgents.Get(uAgent).m_xData;
			for (uint32_t u = 0; u < xData.m_axPerceivedTargets.GetSize(); )
			{
				const Zenith_EntityID xTargetID = xData.m_axPerceivedTargets.Get(u).m_xEntityID;
				if (!Zenith_SceneSystem::Get().ResolveEntity(xTargetID).IsValid())
				{
					xData.m_axPerceivedTargets.RemoveSwap(u);
				}
				else
				{
					++u;
				}
			}
			UpdatePrimaryTarget(xData);
		}
	}
}

void Zenith_PerceptionSystem::OnEntityOwnerSceneChanged(Zenith_EntityID xEntityID,
	Zenith_Scene xOldScene, Zenith_Scene xNewScene)
{
	if (xOldScene == xNewScene) return;

	ScenePerception* pxOld = FindSceneBucket(xOldScene);
	if (pxOld == nullptr) return;   // nothing registered for the entity's old home

	// Move the agent record, preserving its accumulated awareness/memory: a
	// DontDestroyOnLoad promotion must not amnesia the agent.
	for (uint32_t u = 0; u < pxOld->m_axAgents.GetSize(); ++u)
	{
		if (pxOld->m_axAgents.Get(u).m_xAgentID != xEntityID) continue;
		AgentRecord xMoved = std::move(pxOld->m_axAgents.Get(u));
		pxOld->m_axAgents.RemoveSwap(u);
		// EnsureSceneBucket may grow s_axScenes and invalidate pxOld -- take the
		// destination reference only after the move out of the source is done,
		// and do not touch pxOld afterwards.
		EnsureSceneBucket(xNewScene).m_axAgents.PushBack(std::move(xMoved));
		break;
	}

	pxOld = FindSceneBucket(xOldScene);
	if (pxOld == nullptr) return;
	for (uint32_t u = 0; u < pxOld->m_axTargets.GetSize(); ++u)
	{
		if (pxOld->m_axTargets.Get(u).m_xTargetID != xEntityID) continue;
		TargetRecord xMoved = pxOld->m_axTargets.Get(u);
		pxOld->m_axTargets.RemoveSwap(u);
		EnsureSceneBucket(xNewScene).m_axTargets.PushBack(xMoved);
		break;
	}
}

void Zenith_PerceptionSystem::Update(float fDt)
{
	// No active-scene lookup here: every agent and target is reached through its
	// OWN scene's bucket, so agents in the persistent scene or in additively-
	// loaded scenes are perceived correctly. Matches Unity's
	// SceneManager.GetActiveScene contract — "the active Scene has no impact on
	// what Scenes are rendered" (and by extension, no impact on which entities
	// are queried).
	// Ref: https://docs.unity3d.com/ScriptReference/SceneManagement.SceneManager.GetActiveScene.html
	Zenith_Profiling::ScopeZone xProfileScope(ZENITH_PROFILE_ZONE("AI Perception Update"));

	// Sounds age out UNCONDITIONALLY, before the no-agents early-out. They used
	// to tick only when at least one agent was registered, so a sound emitted in
	// an agent-less window never spent its 0.5s lifetime and stayed audible for
	// the rest of the process -- outliving its emitter, its scene, and (in a
	// batch run) its test.
	UpdateActiveSounds(fDt);

	bool bAnyAgents = false;
	for (uint32_t uScene = 0; uScene < s_axScenes.GetSize() && !bAnyAgents; ++uScene)
	{
		const ScenePerception& xBucket = s_axScenes.Get(uScene);
		bAnyAgents = IsBucketLive(xBucket) && xBucket.m_axAgents.GetSize() > 0;
	}
	if (!bAnyAgents)
	{
		return;
	}

	UpdateSightPerception(fDt);
	UpdateHearingPerception();
	UpdateMemoryDecay(fDt);
}

void Zenith_PerceptionSystem::RegisterAgent(Zenith_EntityID xAgentID)
{
	if (FindAgentRecord(xAgentID) != nullptr)
	{
		return;   // already registered (idempotent, as before)
	}

	const Zenith_Scene xScene = ResolveRegistrationScene(xAgentID);
	if (!xScene.IsValid())
	{
		Zenith_Warning(LOG_CATEGORY_AI,
			"RegisterAgent: entity %u has no owning scene and there is no active scene; not registered",
			xAgentID.m_uIndex);
		return;
	}

	AgentRecord xRecord;
	xRecord.m_xAgentID = xAgentID;
	EnsureSceneBucket(xScene).m_axAgents.PushBack(std::move(xRecord));
	Zenith_Log(LOG_CATEGORY_AI, "Registered perception agent: %u", xAgentID.m_uIndex);
}

void Zenith_PerceptionSystem::UnregisterAgent(Zenith_EntityID xAgentID)
{
	// Scans every bucket rather than resolving the entity's scene: this runs
	// from OnDestroy, where the entity may already be unresolvable.
	for (uint32_t uScene = 0; uScene < s_axScenes.GetSize(); ++uScene)
	{
		ScenePerception& xBucket = s_axScenes.Get(uScene);
		for (uint32_t u = 0; u < xBucket.m_axAgents.GetSize(); ++u)
		{
			if (xBucket.m_axAgents.Get(u).m_xAgentID == xAgentID)
			{
				xBucket.m_axAgents.RemoveSwap(u);
				return;
			}
		}
	}
}

void Zenith_PerceptionSystem::SetSightConfig(Zenith_EntityID xAgentID, const Zenith_SightConfig& xConfig)
{
	AgentPerceptionData* pxData = FindAgentData(xAgentID);
	if (pxData)
	{
		pxData->m_xSightConfig = xConfig;
	}
}

void Zenith_PerceptionSystem::SetHearingConfig(Zenith_EntityID xAgentID, const Zenith_HearingConfig& xConfig)
{
	AgentPerceptionData* pxData = FindAgentData(xAgentID);
	if (pxData)
	{
		pxData->m_xHearingConfig = xConfig;
	}
}

void Zenith_PerceptionSystem::EmitSoundStimulus(const Zenith_Maths::Vector3& xPosition,
	float fLoudness, float fRadius, Zenith_EntityID xSource)
{
	Zenith_SoundStimulus xSound;
	xSound.m_xPosition = xPosition;
	xSound.m_fLoudness = fLoudness;
	xSound.m_fRadius = fRadius;
	xSound.m_xSourceEntity = xSource;
	xSound.m_fTimeRemaining = 0.5f;  // Sounds persist briefly

	// Attributed to the emitter's scene (active scene when the emitter does not
	// resolve). Sounds are 0.5s transients, so they are never re-homed when the
	// emitter moves scene -- they simply expire where they were made.
	const Zenith_Scene xScene = ResolveRegistrationScene(xSource);
	if (!xScene.IsValid())
	{
		return;   // no world to hear it in
	}
	EnsureSceneBucket(xScene).m_axSounds.PushBack(xSound);
}

void Zenith_PerceptionSystem::EmitDamageStimulus(Zenith_EntityID xVictim,
	Zenith_EntityID xAttacker)
{
	AgentPerceptionData* pxData = FindAgentData(xVictim);
	if (!pxData)
	{
		return;
	}

	// Immediate full awareness of attacker
	Zenith_PerceivedTarget* pxTarget = FindOrCreateTarget(*pxData, xAttacker);
	if (pxTarget)
	{
		pxTarget->m_fAwareness = 1.0f;
		pxTarget->m_fTimeSinceLastSeen = 0.0f;
		pxTarget->m_uStimulusMask |= PERCEPTION_STIMULUS_DAMAGE;
		pxTarget->m_bHostile = true;

		// Audit §3.18 fix: resolve attacker's OWN scene via the AI world-hooks seam.
		// Previously used GetActiveScene which silently dropped attacker-position
		// updates when the attacker lived in a non-active scene (persistent entity,
		// additively-loaded scene, etc.). Ref: Unity's GameObject.scene contract —
		// https://docs.unity3d.com/ScriptReference/GameObject-scene.html
		Zenith_AI_GetEntityPosition(xAttacker, pxTarget->m_xLastKnownPosition);

		UpdatePrimaryTarget(*pxData);
	}
}

void Zenith_PerceptionSystem::RegisterTarget(Zenith_EntityID xTargetID, bool bHostile)
{
	// Re-registration updates in place (the hash-map assignment this replaces
	// did the same), so a caller re-registering a target does not duplicate it.
	for (uint32_t uScene = 0; uScene < s_axScenes.GetSize(); ++uScene)
	{
		ScenePerception& xBucket = s_axScenes.Get(uScene);
		if (!IsBucketLive(xBucket)) continue;
		for (uint32_t u = 0; u < xBucket.m_axTargets.GetSize(); ++u)
		{
			if (xBucket.m_axTargets.Get(u).m_xTargetID == xTargetID)
			{
				xBucket.m_axTargets.Get(u).m_xInfo.m_bHostile = bHostile;
				return;
			}
		}
	}

	const Zenith_Scene xScene = ResolveRegistrationScene(xTargetID);
	if (!xScene.IsValid())
	{
		Zenith_Warning(LOG_CATEGORY_AI,
			"RegisterTarget: entity %u has no owning scene and there is no active scene; not registered",
			xTargetID.m_uIndex);
		return;
	}

	TargetRecord xRecord;
	xRecord.m_xTargetID = xTargetID;
	xRecord.m_xInfo.m_bHostile = bHostile;
	EnsureSceneBucket(xScene).m_axTargets.PushBack(xRecord);
}

void Zenith_PerceptionSystem::UnregisterTarget(Zenith_EntityID xTargetID)
{
	// Scans every bucket (including stale ones) rather than resolving the
	// entity's scene: this runs from OnDestroy, where the entity may already be
	// unresolvable.
	for (uint32_t uScene = 0; uScene < s_axScenes.GetSize(); ++uScene)
	{
		ScenePerception& xBucket = s_axScenes.Get(uScene);
		for (uint32_t u = 0; u < xBucket.m_axTargets.GetSize(); ++u)
		{
			if (xBucket.m_axTargets.Get(u).m_xTargetID == xTargetID)
			{
				xBucket.m_axTargets.RemoveSwap(u);
				break;
			}
		}
	}

	// Remove from all agent perceptions, in every scene: an agent in scene A can
	// legitimately hold a memory of a target in scene B.
	for (uint32_t uScene = 0; uScene < s_axScenes.GetSize(); ++uScene)
	{
		ScenePerception& xBucket = s_axScenes.Get(uScene);
		if (!IsBucketLive(xBucket)) continue;
		for (uint32_t uAgent = 0; uAgent < xBucket.m_axAgents.GetSize(); ++uAgent)
		{
			AgentPerceptionData& xData = xBucket.m_axAgents.Get(uAgent).m_xData;
			Zenith_Vector<Zenith_PerceivedTarget>& axTargets = xData.m_axPerceivedTargets;
			bool bRemovedAny = false;
			for (uint32_t u = 0; u < axTargets.GetSize(); )
			{
				if (axTargets.Get(u).m_xEntityID == xTargetID)
				{
					axTargets.RemoveSwap(u);
					bRemovedAny = true;
				}
				else
				{
					++u;
				}
			}
			// Re-derive rather than leave m_xPrimaryTarget naming the entity we
			// just forgot (the pre-scene-ownership sweep left it stale until the
			// agent's next Update tick).
			if (bRemovedAny)
			{
				UpdatePrimaryTarget(xData);
			}
		}
	}
}

void Zenith_PerceptionSystem::SetTargetHostile(Zenith_EntityID xTargetID, bool bHostile)
{
	for (uint32_t uScene = 0; uScene < s_axScenes.GetSize(); ++uScene)
	{
		ScenePerception& xBucket = s_axScenes.Get(uScene);
		if (!IsBucketLive(xBucket)) continue;
		for (uint32_t u = 0; u < xBucket.m_axTargets.GetSize(); ++u)
		{
			if (xBucket.m_axTargets.Get(u).m_xTargetID == xTargetID)
			{
				xBucket.m_axTargets.Get(u).m_xInfo.m_bHostile = bHostile;
				return;
			}
		}
	}
}

const Zenith_Vector<Zenith_PerceivedTarget>* Zenith_PerceptionSystem::GetPerceivedTargets(Zenith_EntityID xAgentID)
{
	const AgentPerceptionData* pxData = FindAgentData(xAgentID);
	if (pxData)
	{
		return &pxData->m_axPerceivedTargets;
	}
	return nullptr;
}

// EXT-6: walk the agent's perceived-targets list and pick the freshest
// hearing-flagged entry. The PerceivedTarget already carries the position
// (m_xLastKnownPosition), source entity, age (m_fTimeSinceLastSeen), and
// the stimulus mask we need to disambiguate sight vs hearing.
Zenith_PerceptionSystem::Zenith_LastHeardSound
Zenith_PerceptionSystem::GetLastHeardSoundFor(Zenith_EntityID xAgentID)
{
	Zenith_LastHeardSound xResult;
	const AgentPerceptionData* pxData = FindAgentData(xAgentID);
	if (!pxData) return xResult;

	const Zenith_Vector<Zenith_PerceivedTarget>& axTargets = pxData->m_axPerceivedTargets;
	float fBestAge = -1.0f; // sentinel for "no candidate yet"
	for (uint32_t i = 0; i < axTargets.GetSize(); ++i)
	{
		const Zenith_PerceivedTarget& xT = axTargets.Get(i);
		if ((xT.m_uStimulusMask & PERCEPTION_STIMULUS_HEARING) == 0) continue;
		if (fBestAge < 0.0f || xT.m_fTimeSinceLastSeen < fBestAge)
		{
			fBestAge = xT.m_fTimeSinceLastSeen;
			xResult.m_bValid       = true;
			xResult.m_xPosition    = xT.m_xLastKnownPosition;
			xResult.m_xSourceEntity = xT.m_xEntityID;
			xResult.m_fAge         = xT.m_fTimeSinceLastSeen;
		}
	}
	return xResult;
}

Zenith_EntityID Zenith_PerceptionSystem::GetPrimaryTarget(Zenith_EntityID xAgentID)
{
	const AgentPerceptionData* pxData = FindAgentData(xAgentID);
	if (pxData)
	{
		return pxData->m_xPrimaryTarget;
	}
	return INVALID_ENTITY_ID;
}

bool Zenith_PerceptionSystem::IsAwareOf(Zenith_EntityID xAgentID, Zenith_EntityID xTargetID)
{
	return GetAwarenessOf(xAgentID, xTargetID) > 0.0f;
}

float Zenith_PerceptionSystem::GetAwarenessOf(Zenith_EntityID xAgentID, Zenith_EntityID xTargetID)
{
	const AgentPerceptionData* pxData = FindAgentData(xAgentID);
	if (!pxData)
	{
		return 0.0f;
	}

	for (uint32_t u = 0; u < pxData->m_axPerceivedTargets.GetSize(); ++u)
	{
		if (pxData->m_axPerceivedTargets.Get(u).m_xEntityID == xTargetID)
		{
			return pxData->m_axPerceivedTargets.Get(u).m_fAwareness;
		}
	}
	return 0.0f;
}

Zenith_PerceptionSystem::SightEvaluation Zenith_PerceptionSystem::EvaluateSightForTarget(
	const Zenith_Maths::Vector3& xAgentPos,
	const Zenith_Maths::Vector3& xForward,
	const Zenith_SightConfig& xConfig,
	Zenith_Entity& xTargetEntity)
{
	SightEvaluation xResult;

	Zenith_AI_GetEntityPosition(xTargetEntity.GetEntityID(), xResult.m_xTargetPos);
	xResult.m_xTargetPos.y += 1.0f;  // Target center height

	// Squared distance early-out (avoids sqrt for out-of-range targets)
	Zenith_Maths::Vector3 xDelta = xResult.m_xTargetPos - xAgentPos;
	float fDistSq = glm::dot(xDelta, xDelta);
	if (fDistSq > xConfig.m_fMaxRange * xConfig.m_fMaxRange)
	{
		return xResult;
	}
	xResult.m_fDistance = std::sqrt(fDistSq);

	// Angle check
	float fAngle = CalculateAngle(xAgentPos, xForward, xResult.m_xTargetPos);
	bool bInFOV = (fAngle <= xConfig.m_fFOVAngle * 0.5f);
	bool bInPeripheralCone = (fAngle <= xConfig.m_fPeripheralAngle * 0.5f);

	if (!bInFOV && !bInPeripheralCone)
	{
		return xResult;  // Outside all vision cones
	}

	// Line of sight check
	if (xConfig.m_bRequireLineOfSight && !CheckLineOfSight(xAgentPos, xResult.m_xTargetPos))
	{
		return xResult;
	}

	xResult.m_bVisible = true;
	xResult.m_bInPeripheral = !bInFOV && bInPeripheralCone;
	xResult.m_fDistanceFactor = 1.0f - (xResult.m_fDistance / xConfig.m_fMaxRange);
	return xResult;
}

void Zenith_PerceptionSystem::UpdateSightPerception(float fDt)
{
	Zenith_Profiling::ScopeZone xProfileScope(ZENITH_PROFILE_ZONE("AI Perception Sight"));

	// Scene-slot order, then registration order within each scene. Fully
	// determined by scene-load + registration order -- no container history.
	for (uint32_t uAgentScene = 0; uAgentScene < s_axScenes.GetSize(); ++uAgentScene)
	{
	ScenePerception& xAgentBucket = s_axScenes.Get(uAgentScene);
	if (!IsBucketLive(xAgentBucket)) continue;
	for (uint32_t uAgent = 0; uAgent < xAgentBucket.m_axAgents.GetSize(); ++uAgent)
	{
		Zenith_EntityID xAgentID = xAgentBucket.m_axAgents.Get(uAgent).m_xAgentID;
		AgentPerceptionData& xData = xAgentBucket.m_axAgents.Get(uAgent).m_xData;

		// Resolve the agent's OWN transform via the AI world-hooks seam —
		// supports agents in any loaded scene, not just the active one.
		// A false return covers no-scene / stale-handle / no-transform — skip.
		Zenith_Maths::Vector3 xAgentPos;
		if (!Zenith_AI_GetEntityPosition(xAgentID, xAgentPos))
		{
			continue;
		}
		xAgentPos.y += xData.m_xSightConfig.m_fEyeHeight;

		// Forward direction from the agent's rotation. Rotate the +Z basis directly
		// instead of extracting yaw via glm::eulerAngles().y: that asin-based middle
		// (Y) angle collapses for facings more than ~90 deg off +Z (a 180-deg turn
		// decodes to yaw 0, with the rotation pushed into pitch/roll), which pointed
		// the sight cone the wrong way and blinded any -Z-facing agent to targets
		// directly in front of it. quat * +Z is correct for every orientation.
		Zenith_Maths::Quaternion xQuat;
		Zenith_AI_GetEntityRotation(xAgentID, xQuat);
		Zenith_Maths::Vector3 xForward = xQuat * Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		xForward.y = 0.0f;
		const float fFwdLenSq = xForward.x * xForward.x + xForward.z * xForward.z;
		xForward = (fFwdLenSq > 1e-6f) ? Zenith_Maths::Normalize(xForward)
			: Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);

		// Mark all targets as not currently visible
		for (uint32_t u = 0; u < xData.m_axPerceivedTargets.GetSize(); ++u)
		{
			xData.m_axPerceivedTargets.Get(u).m_bCurrentlyVisible = false;
		}

		// Check each registered target, in EVERY loaded scene — cross-scene
		// perception (a persistent player entity, or a target in an additively-
		// loaded scene) works as Unity would expect.
		for (uint32_t uTargetScene = 0; uTargetScene < s_axScenes.GetSize(); ++uTargetScene)
		{
		ScenePerception& xTargetBucket = s_axScenes.Get(uTargetScene);
		if (!IsBucketLive(xTargetBucket)) continue;
		for (uint32_t uTarget = 0; uTarget < xTargetBucket.m_axTargets.GetSize(); ++uTarget)
		{
			const TargetRecord& xTargetRecord = xTargetBucket.m_axTargets.Get(uTarget);
			Zenith_EntityID xTargetID = xTargetRecord.m_xTargetID;

			// Don't perceive self
			if (xTargetID == xAgentID)
			{
				continue;
			}

			Zenith_Entity xTargetEntity = Zenith_SceneSystem::Get().ResolveEntity(xTargetID);
			if (!xTargetEntity.IsValid())
			{
				continue;
			}
			// A false position probe covers stale-handle / no-transform — skip,
			// matching the prior IsValid() + transform-component gate.
			Zenith_Maths::Vector3 xTargetProbe;
			if (!Zenith_AI_GetEntityPosition(xTargetID, xTargetProbe))
			{
				continue;
			}

			SightEvaluation xEval = EvaluateSightForTarget(xAgentPos, xForward, xData.m_xSightConfig, xTargetEntity);
			if (!xEval.m_bVisible)
			{
				continue;
			}

			// Target is visible - update awareness
			Zenith_PerceivedTarget* pxTarget = FindOrCreateTarget(xData, xTargetID);
			pxTarget->m_bCurrentlyVisible = true;
			pxTarget->m_fTimeSinceLastSeen = 0.0f;
			pxTarget->m_xLastKnownPosition = xEval.m_xTargetPos;
			pxTarget->m_uStimulusMask |= PERCEPTION_STIMULUS_SIGHT;
			pxTarget->m_bHostile = xTargetRecord.m_xInfo.m_bHostile;

			// Awareness gain (peripheral vision is slower)
			float fGainRate = xData.m_xSightConfig.m_fAwarenessGainRate;
			if (xEval.m_bInPeripheral)
			{
				fGainRate *= xData.m_xSightConfig.m_fPeripheralMultiplier;
			}
			fGainRate *= xEval.m_fDistanceFactor;

			pxTarget->m_fAwareness = std::min(1.0f, pxTarget->m_fAwareness + fGainRate * fDt);
		}
		}

		UpdatePrimaryTarget(xData);
	}
	}
}

bool Zenith_PerceptionSystem::EvaluateHearingForSound(
	const Zenith_Maths::Vector3& xAgentPos,
	const Zenith_HearingConfig& xConfig,
	const Zenith_SoundStimulus& xSound,
	float& fOutAwarenessGain)
{
	fOutAwarenessGain = 0.0f;

	// Squared distance early-out (avoids sqrt for out-of-range sounds)
	Zenith_Maths::Vector3 xDelta = xSound.m_xPosition - xAgentPos;
	float fDistSq = glm::dot(xDelta, xDelta);
	float fMaxDist = std::min(xSound.m_fRadius, xConfig.m_fMaxRange);
	if (fDistSq > fMaxDist * fMaxDist)
	{
		return false;
	}
	float fDist = std::sqrt(fDistSq);

	// Calculate perceived loudness with falloff
	float fFalloff = 1.0f - (fDist / xSound.m_fRadius);
	float fPerceivedLoudness = xSound.m_fLoudness * fFalloff;

	if (fPerceivedLoudness < xConfig.m_fLoudnessThreshold)
	{
		return false;
	}

	fOutAwarenessGain = fPerceivedLoudness * 0.5f;
	return true;
}

void Zenith_PerceptionSystem::UpdateHearingPerception()
{
	// Agents in persistent or additively-loaded scenes must still hear, and must
	// hear sounds made in any loaded scene.
	// Ref: https://docs.unity3d.com/ScriptReference/GameObject-scene.html
	for (uint32_t uAgentScene = 0; uAgentScene < s_axScenes.GetSize(); ++uAgentScene)
	{
	ScenePerception& xAgentBucket = s_axScenes.Get(uAgentScene);
	if (!IsBucketLive(xAgentBucket)) continue;
	for (uint32_t uAgent = 0; uAgent < xAgentBucket.m_axAgents.GetSize(); ++uAgent)
	{
		Zenith_EntityID xAgentID = xAgentBucket.m_axAgents.Get(uAgent).m_xAgentID;
		AgentPerceptionData& xData = xAgentBucket.m_axAgents.Get(uAgent).m_xData;

		// Resolve the agent's OWN transform via the AI world-hooks seam. A false
		// return covers no-scene / stale-handle / no-transform — skip.
		Zenith_Maths::Vector3 xAgentPos;
		if (!Zenith_AI_GetEntityPosition(xAgentID, xAgentPos))
		{
			continue;
		}

		// Check each active sound, in every loaded scene
		for (uint32_t uSoundScene = 0; uSoundScene < s_axScenes.GetSize(); ++uSoundScene)
		{
		ScenePerception& xSoundBucket = s_axScenes.Get(uSoundScene);
		if (!IsBucketLive(xSoundBucket)) continue;
		for (uint32_t u = 0; u < xSoundBucket.m_axSounds.GetSize(); ++u)
		{
			const Zenith_SoundStimulus& xSound = xSoundBucket.m_axSounds.Get(u);

			// Don't hear own sounds
			if (xSound.m_xSourceEntity == xAgentID)
			{
				continue;
			}

			float fAwarenessGain = 0.0f;
			if (!EvaluateHearingForSound(xAgentPos, xData.m_xHearingConfig, xSound, fAwarenessGain))
			{
				continue;
			}

			// Heard the sound - update perception of source
			if (xSound.m_xSourceEntity.IsValid())
			{
				Zenith_PerceivedTarget* pxTarget = FindOrCreateTarget(xData, xSound.m_xSourceEntity);
				pxTarget->m_xLastKnownPosition = xSound.m_xPosition;
				pxTarget->m_fTimeSinceLastSeen = 0.0f;
				pxTarget->m_uStimulusMask |= PERCEPTION_STIMULUS_HEARING;
				pxTarget->m_fAwareness = std::min(1.0f, pxTarget->m_fAwareness + fAwarenessGain);
			}
		}
		}

		UpdatePrimaryTarget(xData);
	}
	}
}

void Zenith_PerceptionSystem::UpdateMemoryDecay(float fDt)
{
	for (uint32_t uScene = 0; uScene < s_axScenes.GetSize(); ++uScene)
	{
	ScenePerception& xBucket = s_axScenes.Get(uScene);
	if (!IsBucketLive(xBucket)) continue;
	for (uint32_t uAgent = 0; uAgent < xBucket.m_axAgents.GetSize(); ++uAgent)
	{
		AgentPerceptionData& xData = xBucket.m_axAgents.Get(uAgent).m_xData;

		for (uint32_t u = 0; u < xData.m_axPerceivedTargets.GetSize(); )
		{
			Zenith_PerceivedTarget& xTarget = xData.m_axPerceivedTargets.Get(u);

			if (!xTarget.m_bCurrentlyVisible)
			{
				// Decay awareness
				xTarget.m_fTimeSinceLastSeen += fDt;
				xTarget.m_fAwareness -= xData.m_xSightConfig.m_fAwarenessDecayRate * fDt;

				// Remove fully forgotten targets
				if (xTarget.m_fAwareness <= 0.0f)
				{
					xData.m_axPerceivedTargets.RemoveSwap(u);
					continue;
				}
			}

			++u;
		}

		UpdatePrimaryTarget(xData);
	}
	}
}

void Zenith_PerceptionSystem::UpdateActiveSounds(float fDt)
{
	for (uint32_t uScene = 0; uScene < s_axScenes.GetSize(); ++uScene)
	{
		ScenePerception& xBucket = s_axScenes.Get(uScene);
		if (!IsBucketLive(xBucket)) continue;
		Zenith_Vector<Zenith_SoundStimulus>& axSounds = xBucket.m_axSounds;
		for (uint32_t u = 0; u < axSounds.GetSize(); )
		{
			axSounds.Get(u).m_fTimeRemaining -= fDt;
			if (axSounds.Get(u).m_fTimeRemaining <= 0.0f)
			{
				axSounds.RemoveSwap(u);
			}
			else
			{
				++u;
			}
		}
	}
}

bool Zenith_PerceptionSystem::CheckLineOfSight(const Zenith_Maths::Vector3& xFrom,
	const Zenith_Maths::Vector3& xTo)
{
	// Use physics raycast to check for occlusion
	Zenith_Maths::Vector3 xDirection = xTo - xFrom;
	float fDistance = Zenith_Maths::Length(xDirection);

	if (fDistance < 0.001f)
	{
		return true;  // Same position, assume clear LOS
	}

	Zenith_Physics::RaycastResult xResult = Zenith_Physics::Get().Raycast(xFrom, xDirection, fDistance);

	// If we didn't hit anything, line of sight is clear
	if (!xResult.m_bHit)
	{
		return true;
	}

	// If we hit something but it's very close to the target position,
	// consider it as hitting the target itself (within tolerance)
	float fTolerance = 0.5f;
	if (Zenith_Maths::Length(xResult.m_xHitPoint - xTo) < fTolerance)
	{
		return true;
	}

	// Otherwise, something is blocking the line of sight
	return false;
}

float Zenith_PerceptionSystem::CalculateAngle(const Zenith_Maths::Vector3& xFrom,
	const Zenith_Maths::Vector3& xForward, const Zenith_Maths::Vector3& xTo)
{
	Zenith_Maths::Vector3 xDir = Zenith_Maths::Normalize(xTo - xFrom);

	// Use only XZ plane for angle calculation
	Zenith_Maths::Vector3 xDirXZ = Zenith_Maths::Normalize(Zenith_Maths::Vector3(xDir.x, 0.0f, xDir.z));
	Zenith_Maths::Vector3 xFwdXZ = Zenith_Maths::Normalize(Zenith_Maths::Vector3(xForward.x, 0.0f, xForward.z));

	float fDot = Zenith_Maths::Dot(xDirXZ, xFwdXZ);
	fDot = std::max(-1.0f, std::min(1.0f, fDot));

	return std::acos(fDot) * (180.0f / 3.14159265f);
}

Zenith_PerceivedTarget* Zenith_PerceptionSystem::FindOrCreateTarget(AgentPerceptionData& xData,
	Zenith_EntityID xTargetID)
{
	// Find existing
	for (uint32_t u = 0; u < xData.m_axPerceivedTargets.GetSize(); ++u)
	{
		if (xData.m_axPerceivedTargets.Get(u).m_xEntityID == xTargetID)
		{
			return &xData.m_axPerceivedTargets.Get(u);
		}
	}

	// Create new
	Zenith_PerceivedTarget xTarget;
	xTarget.m_xEntityID = xTargetID;
	xTarget.m_fAwareness = 0.0f;
	xTarget.m_fTimeSinceLastSeen = 0.0f;
	xTarget.m_bCurrentlyVisible = false;
	xTarget.m_uStimulusMask = 0;

	xData.m_axPerceivedTargets.PushBack(xTarget);
	return &xData.m_axPerceivedTargets.Get(xData.m_axPerceivedTargets.GetSize() - 1);
}

void Zenith_PerceptionSystem::UpdatePrimaryTarget(AgentPerceptionData& xData)
{
	xData.m_xPrimaryTarget = INVALID_ENTITY_ID;
	float fHighestAwareness = 0.0f;

	for (uint32_t u = 0; u < xData.m_axPerceivedTargets.GetSize(); ++u)
	{
		const Zenith_PerceivedTarget& xTarget = xData.m_axPerceivedTargets.Get(u);
		if (xTarget.m_bHostile && xTarget.m_fAwareness > fHighestAwareness)
		{
			fHighestAwareness = xTarget.m_fAwareness;
			xData.m_xPrimaryTarget = xTarget.m_xEntityID;
		}
	}
}

#ifdef ZENITH_TESTING
uint32_t Zenith_PerceptionSystem::GetAgentCountForTest()
{
	uint32_t uCount = 0;
	for (uint32_t u = 0; u < s_axScenes.GetSize(); ++u)
	{
		const ScenePerception& xBucket = s_axScenes.Get(u);
		if (IsBucketLive(xBucket)) uCount += xBucket.m_axAgents.GetSize();
	}
	return uCount;
}

uint32_t Zenith_PerceptionSystem::GetTargetCountForTest()
{
	uint32_t uCount = 0;
	for (uint32_t u = 0; u < s_axScenes.GetSize(); ++u)
	{
		const ScenePerception& xBucket = s_axScenes.Get(u);
		if (IsBucketLive(xBucket)) uCount += xBucket.m_axTargets.GetSize();
	}
	return uCount;
}

uint32_t Zenith_PerceptionSystem::GetActiveSoundCountForTest()
{
	uint32_t uCount = 0;
	for (uint32_t u = 0; u < s_axScenes.GetSize(); ++u)
	{
		const ScenePerception& xBucket = s_axScenes.Get(u);
		if (IsBucketLive(xBucket)) uCount += xBucket.m_axSounds.GetSize();
	}
	return uCount;
}

uint32_t Zenith_PerceptionSystem::GetLiveSceneBucketCountForTest()
{
	uint32_t uCount = 0;
	for (uint32_t u = 0; u < s_axScenes.GetSize(); ++u)
	{
		if (IsBucketLive(s_axScenes.Get(u))) ++uCount;
	}
	return uCount;
}

uint32_t Zenith_PerceptionSystem::GetAgentCountForSceneForTest(Zenith_Scene xScene)
{
	const ScenePerception* pxBucket = FindSceneBucket(xScene);
	return pxBucket ? pxBucket->m_axAgents.GetSize() : 0u;
}

void Zenith_PerceptionSystem::GetAgentIterationOrderForTest(Zenith_Vector<Zenith_EntityID>& axOut)
{
	axOut.Clear();
	for (uint32_t uScene = 0; uScene < s_axScenes.GetSize(); ++uScene)
	{
		const ScenePerception& xBucket = s_axScenes.Get(uScene);
		if (!IsBucketLive(xBucket)) continue;
		for (uint32_t u = 0; u < xBucket.m_axAgents.GetSize(); ++u)
		{
			axOut.PushBack(xBucket.m_axAgents.Get(u).m_xAgentID);
		}
	}
}
#endif // ZENITH_TESTING

#ifdef ZENITH_TOOLS
void Zenith_PerceptionSystem::DebugDrawAgent(Zenith_EntityID xAgentID,
	const Zenith_Maths::Vector3& xAgentPos,
	const Zenith_Maths::Vector3& xForward)
{
	const AgentPerceptionData* pxData = FindAgentData(xAgentID);
	if (!pxData)
	{
		return;
	}

	const AgentPerceptionData& xData = *pxData;
	const Zenith_SightConfig& xConfig = xData.m_xSightConfig;

	Zenith_Maths::Vector3 xEyePos = xAgentPos;
	xEyePos.y += xConfig.m_fEyeHeight;

	// Draw FOV cone edges
	const Zenith_Maths::Vector3 xFOVColor(1.0f, 1.0f, 0.0f);
	const Zenith_Maths::Vector3 xPeripheralColor(1.0f, 0.5f, 0.0f);

	float fFOVRad = xConfig.m_fFOVAngle * 0.5f * (3.14159265f / 180.0f);
	float fPeriphRad = xConfig.m_fPeripheralAngle * 0.5f * (3.14159265f / 180.0f);

	// Draw FOV lines
	auto DrawConeEdge = [&](float fAngle, const Zenith_Maths::Vector3& xColor)
	{
		float fCos = std::cos(fAngle);
		float fSin = std::sin(fAngle);

		// Rotate forward by angle around Y axis
		Zenith_Maths::Vector3 xDir;
		xDir.x = xForward.x * fCos - xForward.z * fSin;
		xDir.y = 0.0f;
		xDir.z = xForward.x * fSin + xForward.z * fCos;
		xDir = Zenith_Maths::Normalize(xDir);

		Zenith_AI_DebugDrawLine(xEyePos, xEyePos + xDir * xConfig.m_fMaxRange, xColor, 0.02f);
	};

	DrawConeEdge(fFOVRad, xFOVColor);
	DrawConeEdge(-fFOVRad, xFOVColor);
	DrawConeEdge(fPeriphRad, xPeripheralColor);
	DrawConeEdge(-fPeriphRad, xPeripheralColor);

	// Draw forward direction
	Zenith_AI_DebugDrawLine(xEyePos, xEyePos + xForward * 2.0f, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), 0.03f);

	// Draw perceived targets
	for (uint32_t u = 0; u < xData.m_axPerceivedTargets.GetSize(); ++u)
	{
		const Zenith_PerceivedTarget& xTarget = xData.m_axPerceivedTargets.Get(u);

		// Color based on awareness (green = low, red = high)
		Zenith_Maths::Vector3 xColor(xTarget.m_fAwareness, 1.0f - xTarget.m_fAwareness, 0.0f);

		// Line to last known position
		Zenith_AI_DebugDrawLine(xEyePos, xTarget.m_xLastKnownPosition, xColor, 0.015f);

		// Sphere at last known position
		Zenith_AI_DebugDrawSphere(xTarget.m_xLastKnownPosition, 0.15f, xColor);
	}
}
#endif
