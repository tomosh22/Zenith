#include "Zenith.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Physics/Zenith_Physics.h"
#include "Profiling/Zenith_Profiling.h"

#ifdef ZENITH_TOOLS
#include "Flux/Primitives/Flux_Primitives.h"
#endif

std::unordered_map<uint64_t, Zenith_PerceptionSystem::AgentPerceptionData> Zenith_PerceptionSystem::s_xAgentData;
std::unordered_map<uint64_t, Zenith_PerceptionSystem::TargetInfo> Zenith_PerceptionSystem::s_xTargets;
Zenith_Vector<Zenith_SoundStimulus> Zenith_PerceptionSystem::s_axActiveSounds;

void Zenith_PerceptionSystem::Initialise()
{
	s_xAgentData.clear();
	s_xTargets.clear();
	s_axActiveSounds.Clear();
	Zenith_Log(LOG_CATEGORY_AI, "PerceptionSystem initialized");
}

void Zenith_PerceptionSystem::Shutdown()
{
	s_xAgentData.clear();
	s_xTargets.clear();
	s_axActiveSounds.Clear();
}

void Zenith_PerceptionSystem::Reset()
{
	s_xAgentData.clear();
	s_xTargets.clear();
	s_axActiveSounds.Clear();
}

void Zenith_PerceptionSystem::Update(float fDt)
{
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	if (pxSceneData)
	{
		Update(fDt, *pxSceneData);
	}
}

void Zenith_PerceptionSystem::Update(float fDt, Zenith_SceneData& xScene)
{
	Zenith_Profiling::Scope xProfileScope(ZENITH_PROFILE_INDEX__AI_PERCEPTION_UPDATE);

	if (s_xAgentData.empty())
	{
		return;
	}

	// Update active sounds
	UpdateActiveSounds(fDt);

	// Update sight perception
	UpdateSightPerception(fDt, xScene);

	// Update hearing perception
	UpdateHearingPerception();

	// Update memory decay
	UpdateMemoryDecay(fDt);
}

void Zenith_PerceptionSystem::RegisterAgent(Zenith_EntityID xAgentID)
{
	uint64_t uKey = xAgentID.GetPacked();
	if (s_xAgentData.find(uKey) == s_xAgentData.end())
	{
		s_xAgentData[uKey] = AgentPerceptionData();
		Zenith_Log(LOG_CATEGORY_AI, "Registered perception agent: %u", xAgentID.m_uIndex);
	}
}

void Zenith_PerceptionSystem::UnregisterAgent(Zenith_EntityID xAgentID)
{
	s_xAgentData.erase(xAgentID.GetPacked());
}

void Zenith_PerceptionSystem::SetSightConfig(Zenith_EntityID xAgentID, const Zenith_SightConfig& xConfig)
{
	auto it = s_xAgentData.find(xAgentID.GetPacked());
	if (it != s_xAgentData.end())
	{
		it->second.m_xSightConfig = xConfig;
	}
}

void Zenith_PerceptionSystem::SetHearingConfig(Zenith_EntityID xAgentID, const Zenith_HearingConfig& xConfig)
{
	auto it = s_xAgentData.find(xAgentID.GetPacked());
	if (it != s_xAgentData.end())
	{
		it->second.m_xHearingConfig = xConfig;
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
	auto it = s_xAgentData.find(xVictim.GetPacked());
	if (it == s_xAgentData.end())
	{
		return;
	}

	// Immediate full awareness of attacker
	Zenith_PerceivedTarget* pxTarget = FindOrCreateTarget(it->second, xAttacker);
	if (pxTarget)
	{
		pxTarget->m_fAwareness = 1.0f;
		pxTarget->m_fTimeSinceLastSeen = 0.0f;
		pxTarget->m_uStimulusMask |= PERCEPTION_STIMULUS_DAMAGE;
		pxTarget->m_bHostile = true;

		// Get attacker position
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		if (pxSceneData)
		{
			Zenith_Entity xAttackerEntity = pxSceneData->TryGetEntity(xAttacker);
			if (xAttackerEntity.IsValid() && xAttackerEntity.HasComponent<Zenith_TransformComponent>())
			{
				xAttackerEntity.GetComponent<Zenith_TransformComponent>().GetPosition(pxTarget->m_xLastKnownPosition);
			}
		}

		UpdatePrimaryTarget(it->second);
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
	s_xTargets.erase(xTargetID.GetPacked());

	// Remove from all agent perceptions
	for (auto& xPair : s_xAgentData)
	{
		auto& axTargets = xPair.second.m_axPerceivedTargets;
		for (uint32_t u = 0; u < axTargets.GetSize(); )
		{
			if (axTargets.Get(u).m_xEntityID == xTargetID)
			{
				// Swap with last and pop
				uint32_t uLast = axTargets.GetSize() - 1;
				if (u != uLast)
				{
					axTargets.Get(u) = axTargets.Get(uLast);
				}
				axTargets.PopBack();
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
	auto it = s_xTargets.find(xTargetID.GetPacked());
	if (it != s_xTargets.end())
	{
		it->second.m_bHostile = bHostile;
	}
}

const Zenith_Vector<Zenith_PerceivedTarget>* Zenith_PerceptionSystem::GetPerceivedTargets(Zenith_EntityID xAgentID)
{
	auto it = s_xAgentData.find(xAgentID.GetPacked());
	if (it != s_xAgentData.end())
	{
		return &it->second.m_axPerceivedTargets;
	}
	return nullptr;
}

Zenith_EntityID Zenith_PerceptionSystem::GetPrimaryTarget(Zenith_EntityID xAgentID)
{
	auto it = s_xAgentData.find(xAgentID.GetPacked());
	if (it != s_xAgentData.end())
	{
		return it->second.m_xPrimaryTarget;
	}
	return INVALID_ENTITY_ID;
}

bool Zenith_PerceptionSystem::IsAwareOf(Zenith_EntityID xAgentID, Zenith_EntityID xTargetID)
{
	return GetAwarenessOf(xAgentID, xTargetID) > 0.0f;
}

float Zenith_PerceptionSystem::GetAwarenessOf(Zenith_EntityID xAgentID, Zenith_EntityID xTargetID)
{
	auto it = s_xAgentData.find(xAgentID.GetPacked());
	if (it == s_xAgentData.end())
	{
		return 0.0f;
	}

	for (uint32_t u = 0; u < it->second.m_axPerceivedTargets.GetSize(); ++u)
	{
		if (it->second.m_axPerceivedTargets.Get(u).m_xEntityID == xTargetID)
		{
			return it->second.m_axPerceivedTargets.Get(u).m_fAwareness;
		}
	}
	return 0.0f;
}

void Zenith_PerceptionSystem::UpdateSightPerception(float fDt, Zenith_SceneData& xScene)
{
	Zenith_Profiling::Scope xProfileScope(ZENITH_PROFILE_INDEX__AI_PERCEPTION_SIGHT);

	for (auto& xPair : s_xAgentData)
	{
		Zenith_EntityID xAgentID = Zenith_EntityID::FromPacked(xPair.first);
		AgentPerceptionData& xData = xPair.second;

		Zenith_Entity xAgentEntity = xScene.TryGetEntity(xAgentID);
		if (!xAgentEntity.IsValid() || !xAgentEntity.HasComponent<Zenith_TransformComponent>())
		{
			continue;
		}

		Zenith_TransformComponent& xAgentTransform = xAgentEntity.GetComponent<Zenith_TransformComponent>();
		Zenith_Maths::Vector3 xAgentPos;
		xAgentTransform.GetPosition(xAgentPos);
		xAgentPos.y += xData.m_xSightConfig.m_fEyeHeight;

		// Calculate forward direction from rotation
		Zenith_Maths::Quaternion xQuat;
		xAgentTransform.GetRotation(xQuat);
		Zenith_Maths::Vector3 xRot = glm::eulerAngles(xQuat);
		float fYaw = xRot.y;
		Zenith_Maths::Vector3 xForward(std::sin(fYaw), 0.0f, std::cos(fYaw));

		// Mark all targets as not currently visible
		for (uint32_t u = 0; u < xData.m_axPerceivedTargets.GetSize(); ++u)
		{
			xData.m_axPerceivedTargets.Get(u).m_bCurrentlyVisible = false;
		}

		// Check each registered target
		for (auto& xTargetPair : s_xTargets)
		{
			Zenith_EntityID xTargetID = Zenith_EntityID::FromPacked(xTargetPair.first);

			// Don't perceive self
			if (xTargetID == xAgentID)
			{
				continue;
			}

			Zenith_Entity xTargetEntity = xScene.TryGetEntity(xTargetID);
			if (!xTargetEntity.IsValid() || !xTargetEntity.HasComponent<Zenith_TransformComponent>())
			{
				continue;
			}

			Zenith_TransformComponent& xTargetTransform = xTargetEntity.GetComponent<Zenith_TransformComponent>();
			Zenith_Maths::Vector3 xTargetPos;
			xTargetTransform.GetPosition(xTargetPos);
			xTargetPos.y += 1.0f;  // Target center height

			// Distance check
			float fDist = Zenith_Maths::Length(xTargetPos - xAgentPos);
			if (fDist > xData.m_xSightConfig.m_fMaxRange)
			{
				continue;
			}

			// Angle check
			float fAngle = CalculateAngle(xAgentPos, xForward, xTargetPos);
			bool bInFOV = (fAngle <= xData.m_xSightConfig.m_fFOVAngle * 0.5f);
			bool bInPeripheral = (fAngle <= xData.m_xSightConfig.m_fPeripheralAngle * 0.5f);

			if (!bInFOV && !bInPeripheral)
			{
				continue;  // Outside all vision cones
			}

			// Line of sight check
			bool bCanSee = true;
			if (xData.m_xSightConfig.m_bRequireLineOfSight)
			{
				bCanSee = CheckLineOfSight(xAgentPos, xTargetPos);
			}

			if (!bCanSee)
			{
				continue;
			}

			// Target is visible - update awareness
			Zenith_PerceivedTarget* pxTarget = FindOrCreateTarget(xData, xTargetID);
			pxTarget->m_bCurrentlyVisible = true;
			pxTarget->m_fTimeSinceLastSeen = 0.0f;
			pxTarget->m_xLastKnownPosition = xTargetPos;
			pxTarget->m_uStimulusMask |= PERCEPTION_STIMULUS_SIGHT;
			pxTarget->m_bHostile = xTargetPair.second.m_bHostile;

			// Update estimated velocity
			// (Would need previous position stored for proper velocity estimation)

			// Awareness gain
			float fGainRate = xData.m_xSightConfig.m_fAwarenessGainRate;
			if (!bInFOV && bInPeripheral)
			{
				fGainRate *= xData.m_xSightConfig.m_fPeripheralMultiplier;
			}

			// Distance affects awareness gain (closer = faster)
			float fDistFactor = 1.0f - (fDist / xData.m_xSightConfig.m_fMaxRange);
			fGainRate *= fDistFactor;

			pxTarget->m_fAwareness = std::min(1.0f, pxTarget->m_fAwareness + fGainRate * fDt);
		}

		UpdatePrimaryTarget(xData);
	}
}

void Zenith_PerceptionSystem::UpdateHearingPerception()
{
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	if (!pxSceneData)
	{
		return;
	}

	for (auto& xPair : s_xAgentData)
	{
		Zenith_EntityID xAgentID = Zenith_EntityID::FromPacked(xPair.first);
		AgentPerceptionData& xData = xPair.second;

		Zenith_Entity xAgentEntity = pxSceneData->TryGetEntity(xAgentID);
		if (!xAgentEntity.IsValid() || !xAgentEntity.HasComponent<Zenith_TransformComponent>())
		{
			continue;
		}

		Zenith_Maths::Vector3 xAgentPos;
		xAgentEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xAgentPos);

		// Check each active sound
		for (uint32_t u = 0; u < s_axActiveSounds.GetSize(); ++u)
		{
			const Zenith_SoundStimulus& xSound = s_axActiveSounds.Get(u);

			// Don't hear own sounds
			if (xSound.m_xSourceEntity == xAgentID)
			{
				continue;
			}

			// Distance check
			float fDist = Zenith_Maths::Length(xSound.m_xPosition - xAgentPos);
			if (fDist > xSound.m_fRadius || fDist > xData.m_xHearingConfig.m_fMaxRange)
			{
				continue;
			}

			// Calculate perceived loudness with falloff
			float fFalloff = 1.0f - (fDist / xSound.m_fRadius);
			float fPerceivedLoudness = xSound.m_fLoudness * fFalloff;

			if (fPerceivedLoudness < xData.m_xHearingConfig.m_fLoudnessThreshold)
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

				// Sound awareness is based on loudness
				float fAwarenessGain = fPerceivedLoudness * 0.5f;
				pxTarget->m_fAwareness = std::min(1.0f, pxTarget->m_fAwareness + fAwarenessGain);
			}
		}

		UpdatePrimaryTarget(xData);
	}
}

void Zenith_PerceptionSystem::UpdateMemoryDecay(float fDt)
{
	for (auto& xPair : s_xAgentData)
	{
		AgentPerceptionData& xData = xPair.second;

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
					// Swap with last and pop
					uint32_t uLast = xData.m_axPerceivedTargets.GetSize() - 1;
					if (u != uLast)
					{
						xData.m_axPerceivedTargets.Get(u) = xData.m_axPerceivedTargets.Get(uLast);
					}
					xData.m_axPerceivedTargets.PopBack();
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
			// Swap with last and pop
			uint32_t uLast = s_axActiveSounds.GetSize() - 1;
			if (u != uLast)
			{
				s_axActiveSounds.Get(u) = s_axActiveSounds.Get(uLast);
			}
			s_axActiveSounds.PopBack();
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

	Zenith_Physics::RaycastResult xResult = Zenith_Physics::Raycast(xFrom, xDirection, fDistance);

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
	auto it = s_xAgentData.find(xAgentID.GetPacked());
	if (it == s_xAgentData.end())
	{
		return;
	}

	const AgentPerceptionData& xData = it->second;
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

		Flux_Primitives::AddLine(xEyePos, xEyePos + xDir * xConfig.m_fMaxRange, xColor, 0.02f);
	};

	DrawConeEdge(fFOVRad, xFOVColor);
	DrawConeEdge(-fFOVRad, xFOVColor);
	DrawConeEdge(fPeriphRad, xPeripheralColor);
	DrawConeEdge(-fPeriphRad, xPeripheralColor);

	// Draw forward direction
	Flux_Primitives::AddLine(xEyePos, xEyePos + xForward * 2.0f, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), 0.03f);

	// Draw perceived targets
	for (uint32_t u = 0; u < xData.m_axPerceivedTargets.GetSize(); ++u)
	{
		const Zenith_PerceivedTarget& xTarget = xData.m_axPerceivedTargets.Get(u);

		// Color based on awareness (green = low, red = high)
		Zenith_Maths::Vector3 xColor(xTarget.m_fAwareness, 1.0f - xTarget.m_fAwareness, 0.0f);

		// Line to last known position
		Flux_Primitives::AddLine(xEyePos, xTarget.m_xLastKnownPosition, xColor, 0.015f);

		// Sphere at last known position
		Flux_Primitives::AddSphere(xTarget.m_xLastKnownPosition, 0.15f, xColor);
	}
}
#endif
