#include "Zenith.h"
#include "Profiling/Zenith_Profiling.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "AI/Zenith_AIWorldHooks.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Physics/Zenith_Physics.h"  // AI->Physics: sibling leaf

Zenith_HashMap<uint64_t, Zenith_PerceptionSystem::AgentPerceptionData> Zenith_PerceptionSystem::s_xAgentData;
Zenith_HashMap<uint64_t, Zenith_PerceptionSystem::TargetInfo> Zenith_PerceptionSystem::s_xTargets;
Zenith_Vector<Zenith_SoundStimulus> Zenith_PerceptionSystem::s_axActiveSounds;

void Zenith_PerceptionSystem::Initialise()
{
	s_xAgentData.Clear();
	s_xTargets.Clear();
	s_axActiveSounds.Clear();
	Zenith_Log(LOG_CATEGORY_AI, "PerceptionSystem initialized");
}

void Zenith_PerceptionSystem::Shutdown()
{
	s_xAgentData.Clear();
	s_xTargets.Clear();
	s_axActiveSounds.Clear();
}

void Zenith_PerceptionSystem::Reset()
{
	s_xAgentData.Clear();
	s_xTargets.Clear();
	s_axActiveSounds.Clear();
}

void Zenith_PerceptionSystem::Update(float fDt)
{
	// Audit §3.18 fix: no active-scene lookup here. Each agent resolves its
	// own scene via GetSceneDataForEntity(EntityID) inside UpdateSightPerception
	// and UpdateHearingPerception, so agents in the persistent scene or in
	// additively-loaded scenes are perceived correctly. Matches Unity's
	// SceneManager.GetActiveScene contract — "the active Scene has no impact on
	// what Scenes are rendered" (and by extension, no impact on which entities
	// are queried).
	// Ref: https://docs.unity3d.com/ScriptReference/SceneManagement.SceneManager.GetActiveScene.html
	Zenith_Profiling::ScopeZone xProfileScope(ZENITH_PROFILE_ZONE("AI Perception Update"));

	if (s_xAgentData.IsEmpty())
	{
		return;
	}

	UpdateActiveSounds(fDt);
	UpdateSightPerception(fDt);
	UpdateHearingPerception();
	UpdateMemoryDecay(fDt);
}

void Zenith_PerceptionSystem::RegisterAgent(Zenith_EntityID xAgentID)
{
	uint64_t uKey = xAgentID.GetPacked();
	if (!s_xAgentData.Contains(uKey))
	{
		s_xAgentData[uKey] = AgentPerceptionData();
		Zenith_Log(LOG_CATEGORY_AI, "Registered perception agent: %u", xAgentID.m_uIndex);
	}
}

void Zenith_PerceptionSystem::UnregisterAgent(Zenith_EntityID xAgentID)
{
	s_xAgentData.Remove(xAgentID.GetPacked());
}

void Zenith_PerceptionSystem::SetSightConfig(Zenith_EntityID xAgentID, const Zenith_SightConfig& xConfig)
{
	AgentPerceptionData* pxData = s_xAgentData.TryGet(xAgentID.GetPacked());
	if (pxData)
	{
		pxData->m_xSightConfig = xConfig;
	}
}

void Zenith_PerceptionSystem::SetHearingConfig(Zenith_EntityID xAgentID, const Zenith_HearingConfig& xConfig)
{
	AgentPerceptionData* pxData = s_xAgentData.TryGet(xAgentID.GetPacked());
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

	s_axActiveSounds.PushBack(xSound);
}

void Zenith_PerceptionSystem::EmitDamageStimulus(Zenith_EntityID xVictim,
	Zenith_EntityID xAttacker)
{
	AgentPerceptionData* pxData = s_xAgentData.TryGet(xVictim.GetPacked());
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
	TargetInfo xInfo;
	xInfo.m_bHostile = bHostile;
	s_xTargets[xTargetID.GetPacked()] = xInfo;
}

void Zenith_PerceptionSystem::UnregisterTarget(Zenith_EntityID xTargetID)
{
	s_xTargets.Remove(xTargetID.GetPacked());

	// Remove from all agent perceptions
	for (Zenith_HashMap<uint64_t, AgentPerceptionData>::Iterator xIt(s_xAgentData); !xIt.Done(); xIt.Next())
	{
		Zenith_Vector<Zenith_PerceivedTarget>& axTargets = xIt.GetValueMutable().m_axPerceivedTargets;
		for (uint32_t u = 0; u < axTargets.GetSize(); )
		{
			if (axTargets.Get(u).m_xEntityID == xTargetID)
			{
				axTargets.RemoveSwap(u);
			}
			else
			{
				++u;
			}
		}
	}
}

void Zenith_PerceptionSystem::SetTargetHostile(Zenith_EntityID xTargetID, bool bHostile)
{
	TargetInfo* pxInfo = s_xTargets.TryGet(xTargetID.GetPacked());
	if (pxInfo)
	{
		pxInfo->m_bHostile = bHostile;
	}
}

const Zenith_Vector<Zenith_PerceivedTarget>* Zenith_PerceptionSystem::GetPerceivedTargets(Zenith_EntityID xAgentID)
{
	const AgentPerceptionData* pxData = s_xAgentData.TryGet(xAgentID.GetPacked());
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
	const AgentPerceptionData* pxData = s_xAgentData.TryGet(xAgentID.GetPacked());
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
	const AgentPerceptionData* pxData = s_xAgentData.TryGet(xAgentID.GetPacked());
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
	const AgentPerceptionData* pxData = s_xAgentData.TryGet(xAgentID.GetPacked());
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

	for (Zenith_HashMap<uint64_t, AgentPerceptionData>::Iterator xAgentIt(s_xAgentData); !xAgentIt.Done(); xAgentIt.Next())
	{
		Zenith_EntityID xAgentID = Zenith_EntityID::FromPacked(xAgentIt.GetKey());
		AgentPerceptionData& xData = xAgentIt.GetValueMutable();

		// Audit §3.18 fix: resolve agent's OWN transform via the AI world-hooks
		// seam — supports agents in any loaded scene, not just the active one.
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

		// Check each registered target
		for (Zenith_HashMap<uint64_t, TargetInfo>::Iterator xTargetIt(s_xTargets); !xTargetIt.Done(); xTargetIt.Next())
		{
			Zenith_EntityID xTargetID = Zenith_EntityID::FromPacked(xTargetIt.GetKey());

			// Don't perceive self
			if (xTargetID == xAgentID)
			{
				continue;
			}

			// Audit §3.18 fix: resolve each target's own scene — cross-scene
			// perception (e.g. a persistent player entity, or a target in an
			// additively-loaded scene) now works as Unity would expect.
			Zenith_SceneData* pxTargetScene = Zenith_SceneSystem::Get().GetSceneDataForEntity(xTargetID);
			if (!pxTargetScene)
			{
				continue;
			}
			Zenith_Entity xTargetEntity = pxTargetScene->TryGetEntity(xTargetID);
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
			pxTarget->m_bHostile = xTargetIt.GetValue().m_bHostile;

			// Awareness gain (peripheral vision is slower)
			float fGainRate = xData.m_xSightConfig.m_fAwarenessGainRate;
			if (xEval.m_bInPeripheral)
			{
				fGainRate *= xData.m_xSightConfig.m_fPeripheralMultiplier;
			}
			fGainRate *= xEval.m_fDistanceFactor;

			pxTarget->m_fAwareness = std::min(1.0f, pxTarget->m_fAwareness + fGainRate * fDt);
		}

		UpdatePrimaryTarget(xData);
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
	// Audit §3.18 fix: resolve each agent's OWN scene instead of the active
	// scene — agents in persistent or additively-loaded scenes must still hear.
	// Ref: https://docs.unity3d.com/ScriptReference/GameObject-scene.html
	for (Zenith_HashMap<uint64_t, AgentPerceptionData>::Iterator xAgentIt(s_xAgentData); !xAgentIt.Done(); xAgentIt.Next())
	{
		Zenith_EntityID xAgentID = Zenith_EntityID::FromPacked(xAgentIt.GetKey());
		AgentPerceptionData& xData = xAgentIt.GetValueMutable();

		// Resolve the agent's OWN transform via the AI world-hooks seam. A false
		// return covers no-scene / stale-handle / no-transform — skip.
		Zenith_Maths::Vector3 xAgentPos;
		if (!Zenith_AI_GetEntityPosition(xAgentID, xAgentPos))
		{
			continue;
		}

		// Check each active sound
		for (uint32_t u = 0; u < s_axActiveSounds.GetSize(); ++u)
		{
			const Zenith_SoundStimulus& xSound = s_axActiveSounds.Get(u);

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

		UpdatePrimaryTarget(xData);
	}
}

void Zenith_PerceptionSystem::UpdateMemoryDecay(float fDt)
{
	for (Zenith_HashMap<uint64_t, AgentPerceptionData>::Iterator xAgentIt(s_xAgentData); !xAgentIt.Done(); xAgentIt.Next())
	{
		AgentPerceptionData& xData = xAgentIt.GetValueMutable();

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

void Zenith_PerceptionSystem::UpdateActiveSounds(float fDt)
{
	for (uint32_t u = 0; u < s_axActiveSounds.GetSize(); )
	{
		s_axActiveSounds.Get(u).m_fTimeRemaining -= fDt;
		if (s_axActiveSounds.Get(u).m_fTimeRemaining <= 0.0f)
		{
			s_axActiveSounds.RemoveSwap(u);
		}
		else
		{
			++u;
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

#ifdef ZENITH_TOOLS
void Zenith_PerceptionSystem::DebugDrawAgent(Zenith_EntityID xAgentID,
	const Zenith_Maths::Vector3& xAgentPos,
	const Zenith_Maths::Vector3& xForward)
{
	const AgentPerceptionData* pxData = s_xAgentData.TryGet(xAgentID.GetPacked());
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
