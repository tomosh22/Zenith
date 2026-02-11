#include "Zenith.h"

#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"
#include "Flux/Particles/Flux_ParticleGPU.h"
#include "DataStream/Zenith_DataStream.h"

ZENITH_REGISTER_COMPONENT(Zenith_ParticleEmitterComponent, "ParticleEmitter")

// Forward declaration of helper function
static Zenith_Maths::Vector3 GetRandomDirectionInCone(const Zenith_Maths::Vector3& xDir, float fSpreadAngleDegrees);

Zenith_ParticleEmitterComponent::Zenith_ParticleEmitterComponent(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void Zenith_ParticleEmitterComponent::SetConfig(Flux_ParticleEmitterConfig* pxConfig)
{
	// Unregister from GPU system if previously registered
	if (m_uGPUEmitterID != UINT32_MAX)
	{
		Flux_ParticleGPU::UnregisterEmitter(m_uGPUEmitterID);
		m_uGPUEmitterID = UINT32_MAX;
	}

	m_pxConfig = pxConfig;

	if (pxConfig != nullptr)
	{
		// NOTE: GPU compute particle rendering is not yet implemented.
		// The compute shader infrastructure is in place, but we can't use
		// Flux_ReadWriteBuffer as vertex instance data with current abstraction.
		// All emitters use CPU mode for now.
		if (pxConfig->m_bUseGPUCompute)
		{
			Zenith_Log(LOG_CATEGORY_PARTICLES, "GPU compute particles not fully implemented, using CPU fallback");
		}

		// Always use CPU mode (GPU rendering not yet supported)
		m_axParticles.Clear();
		m_axParticles.Reserve(pxConfig->m_uMaxParticles);
		for (uint32_t i = 0; i < pxConfig->m_uMaxParticles; ++i)
		{
			m_axParticles.PushBack(Flux_Particle());
		}
		m_uAliveCount = 0;
	}
}

void Zenith_ParticleEmitterComponent::Emit(uint32_t uCount)
{
	if (m_pxConfig == nullptr)
	{
		return;
	}

	Zenith_Maths::Vector3 xPos = GetEmitPosition();
	Zenith_Maths::Vector3 xDir = GetEmitDirection();

	// Always use CPU spawn (GPU rendering not yet supported)
	for (uint32_t i = 0; i < uCount && m_uAliveCount < m_pxConfig->m_uMaxParticles; ++i)
	{
		SpawnParticle(xPos, xDir);
	}
}

void Zenith_ParticleEmitterComponent::SetEmitPosition(const Zenith_Maths::Vector3& xPos)
{
	m_bUsePositionOverride = true;
	m_xOverridePosition = xPos;
}

void Zenith_ParticleEmitterComponent::SetEmitDirection(const Zenith_Maths::Vector3& xDir)
{
	m_bUsePositionOverride = true;
	m_xOverrideDirection = xDir;
}

void Zenith_ParticleEmitterComponent::ClearPositionOverride()
{
	m_bUsePositionOverride = false;
}

bool Zenith_ParticleEmitterComponent::UsesGPUCompute() const
{
	// GPU compute particle rendering is not yet implemented.
	// Always return false so all emitters use the CPU rendering path.
	// When GPU rendering is implemented, restore:
	// return m_pxConfig != nullptr && m_pxConfig->m_bUseGPUCompute;
	return false;
}

void Zenith_ParticleEmitterComponent::Update(float fDt)
{
	if (m_pxConfig == nullptr)
	{
		return;
	}

	// Always use CPU simulation (GPU compute rendering not yet implemented)
	SimulateCPU(fDt);

	// Handle continuous spawning (works for both CPU and GPU modes)
	if (m_bEmitting && m_pxConfig->m_fSpawnRate > 0.0f)
	{
		m_fSpawnAccumulator += fDt * m_pxConfig->m_fSpawnRate;

		// Calculate how many particles to spawn this frame
		uint32_t uSpawnCount = static_cast<uint32_t>(m_fSpawnAccumulator);
		if (uSpawnCount > 0)
		{
			m_fSpawnAccumulator -= static_cast<float>(uSpawnCount);
			Emit(uSpawnCount);  // This handles CPU vs GPU path
		}
	}
}

void Zenith_ParticleEmitterComponent::SimulateCPU(float fDt)
{
	if (m_pxConfig == nullptr)
	{
		return;
	}

	// Update existing particles using swap-and-pop for dead particles
	for (uint32_t i = 0; i < m_uAliveCount; )
	{
		Flux_Particle& xP = m_axParticles.Get(i);

		// Update age
		float fAge = xP.GetAge() + fDt;
		xP.SetAge(fAge);

		if (fAge >= xP.GetLifetime())
		{
			// Dead particle - swap with last alive and decrement count
			if (i < m_uAliveCount - 1)
			{
				m_axParticles.Get(i) = m_axParticles.Get(m_uAliveCount - 1);
			}
			m_uAliveCount--;
			// Don't increment i - we need to check the swapped particle
		}
		else
		{
			// Alive - apply physics
			Zenith_Maths::Vector3 xVel = xP.GetVelocity();

			// Apply gravity
			xVel += m_pxConfig->m_xGravity * fDt;

			// Apply drag
			if (m_pxConfig->m_fDrag > 0.0f)
			{
				xVel *= (1.0f - m_pxConfig->m_fDrag * fDt);
			}

			// Apply turbulence (random velocity perturbation)
			if (m_pxConfig->m_fTurbulence > 0.0f)
			{
				float fTurbulence = m_pxConfig->m_fTurbulence;
				Zenith_Maths::Vector3 xJitter(
					(RandomFloat() * 2.0f - 1.0f) * fTurbulence,
					(RandomFloat() * 2.0f - 1.0f) * fTurbulence,
					(RandomFloat() * 2.0f - 1.0f) * fTurbulence
				);
				xVel += xJitter * fDt;
			}

			xP.SetVelocity(xVel);

			// Update position
			Zenith_Maths::Vector3 xPos = xP.GetPosition();
			xPos += xVel * fDt;
			xP.SetPosition(xPos);

			// Update rotation
			float fRot = xP.GetRotation() + xP.GetRotationSpeed() * fDt;
			xP.SetRotation(fRot);

			i++;
		}
	}
}

void Zenith_ParticleEmitterComponent::SpawnParticle(const Zenith_Maths::Vector3& xPos, const Zenith_Maths::Vector3& xDir)
{
	if (m_pxConfig == nullptr || m_uAliveCount >= m_pxConfig->m_uMaxParticles)
	{
		return;
	}

	Flux_Particle& xP = m_axParticles.Get(m_uAliveCount);

	// Position and age (offset by spawn radius)
	Zenith_Maths::Vector3 xSpawnPos = xPos;
	if (m_pxConfig->m_fSpawnRadius > 0.0f)
	{
		float fRadius = m_pxConfig->m_fSpawnRadius;
		xSpawnPos.x += (RandomFloat() * 2.0f - 1.0f) * fRadius;
		xSpawnPos.y += (RandomFloat() * 2.0f - 1.0f) * fRadius;
		xSpawnPos.z += (RandomFloat() * 2.0f - 1.0f) * fRadius;
	}
	xP.SetPosition(xSpawnPos);
	xP.SetAge(0.0f);

	// Lifetime
	float fLifetime = m_pxConfig->m_fLifetimeMin +
		RandomFloat() * (m_pxConfig->m_fLifetimeMax - m_pxConfig->m_fLifetimeMin);
	xP.SetLifetime(fLifetime);

	// Velocity - random direction within cone, random speed
	Zenith_Maths::Vector3 xRandomDir = GetRandomDirectionInCone(xDir, m_pxConfig->m_fSpreadAngleDegrees);
	float fSpeed = m_pxConfig->m_fSpeedMin +
		RandomFloat() * (m_pxConfig->m_fSpeedMax - m_pxConfig->m_fSpeedMin);
	xP.SetVelocity(xRandomDir * fSpeed);

	// Color
	xP.m_xColorStart = m_pxConfig->m_xColorStart;
	xP.m_xColorEnd = m_pxConfig->m_xColorEnd;

	// Size
	xP.SetSizeStart(m_pxConfig->m_fSizeStart);
	xP.SetSizeEnd(m_pxConfig->m_fSizeEnd);

	// Rotation
	float fRotation = m_pxConfig->m_fRotationMin +
		RandomFloat() * (m_pxConfig->m_fRotationMax - m_pxConfig->m_fRotationMin);
	xP.SetRotation(fRotation);

	float fRotationSpeed = m_pxConfig->m_fRotationSpeedMin +
		RandomFloat() * (m_pxConfig->m_fRotationSpeedMax - m_pxConfig->m_fRotationSpeedMin);
	xP.SetRotationSpeed(fRotationSpeed);

	// Clear padding
	xP.m_xPadding = Zenith_Maths::Vector4(0.0f);

	m_uAliveCount++;
}

Zenith_Maths::Vector3 Zenith_ParticleEmitterComponent::GetEmitPosition() const
{
	if (m_bUsePositionOverride)
	{
		return m_xOverridePosition;
	}

	// Use transform component position
	if (m_xParentEntity.HasComponent<Zenith_TransformComponent>())
	{
		Zenith_Maths::Vector3 xPos;
		m_xParentEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		return xPos;
	}

	return Zenith_Maths::Vector3(0.0f);
}

Zenith_Maths::Vector3 Zenith_ParticleEmitterComponent::GetEmitDirection() const
{
	if (m_bUsePositionOverride)
	{
		return glm::normalize(m_xOverrideDirection);
	}

	// Use config default direction
	if (m_pxConfig != nullptr)
	{
		return glm::normalize(m_pxConfig->m_xEmitDirection);
	}

	return Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
}

// Helper function to get a random direction within a cone
static Zenith_Maths::Vector3 GetRandomDirectionInCone(const Zenith_Maths::Vector3& xDir, float fSpreadAngleDegrees)
{
	if (fSpreadAngleDegrees <= 0.0f)
	{
		return glm::normalize(xDir);
	}

	// Use a simple approach: generate random rotation around the direction
	static std::mt19937 s_xRng{ std::random_device{}() };
	static std::uniform_real_distribution<float> s_xDist{ 0.0f, 1.0f };

	float fSpreadRad = glm::radians(fSpreadAngleDegrees);
	float fPhi = s_xDist(s_xRng) * 2.0f * 3.14159265359f;
	float fCosTheta = 1.0f - s_xDist(s_xRng) * (1.0f - cos(fSpreadRad));
	float fSinTheta = sqrt(1.0f - fCosTheta * fCosTheta);

	// Local direction in cone space (pointing up +Y)
	Zenith_Maths::Vector3 xLocalDir(
		fSinTheta * cos(fPhi),
		fCosTheta,
		fSinTheta * sin(fPhi)
	);

	// Rotate to align with emit direction
	Zenith_Maths::Vector3 xUp(0.0f, 1.0f, 0.0f);
	Zenith_Maths::Vector3 xEmitNorm = glm::normalize(xDir);

	if (glm::abs(glm::dot(xUp, xEmitNorm)) > 0.999f)
	{
		// Emit direction is nearly parallel to up
		return xEmitNorm.y > 0.0f ? xLocalDir : -xLocalDir;
	}

	// Build rotation from up to emit direction
	Zenith_Maths::Vector3 xAxis = glm::normalize(glm::cross(xUp, xEmitNorm));
	float fAngle = acos(glm::clamp(glm::dot(xUp, xEmitNorm), -1.0f, 1.0f));
	Zenith_Maths::Quaternion xRot = glm::angleAxis(fAngle, xAxis);

	return glm::normalize(xRot * xLocalDir);
}

void Zenith_ParticleEmitterComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Version for forward compatibility
	uint32_t uVersion = 3;
	xStream << uVersion;

	// State
	xStream << m_bEmitting;

	// Config name for registry lookup on scene restore
	std::string strConfigName = (m_pxConfig != nullptr) ? m_pxConfig->GetRegisteredName() : "";
	xStream << strConfigName;

	// Version 3+: Position override data
	xStream << m_bUsePositionOverride;
	xStream << m_xOverridePosition.x;
	xStream << m_xOverridePosition.y;
	xStream << m_xOverridePosition.z;
	xStream << m_xOverrideDirection.x;
	xStream << m_xOverrideDirection.y;
	xStream << m_xOverrideDirection.z;
}

void Zenith_ParticleEmitterComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	uint32_t uVersion;
	xStream >> uVersion;

	if (uVersion >= 1)
	{
		xStream >> m_bEmitting;
	}

	if (uVersion >= 2)
	{
		// Look up config by registered name
		std::string strConfigName;
		xStream >> strConfigName;

		if (!strConfigName.empty())
		{
			Flux_ParticleEmitterConfig* pxConfig = Flux_ParticleEmitterConfig::Find(strConfigName);
			if (pxConfig != nullptr)
			{
				SetConfig(pxConfig);
			}
		}
	}

	if (uVersion >= 3)
	{
		// Position override data
		xStream >> m_bUsePositionOverride;
		xStream >> m_xOverridePosition.x;
		xStream >> m_xOverridePosition.y;
		xStream >> m_xOverridePosition.z;
		xStream >> m_xOverrideDirection.x;
		xStream >> m_xOverrideDirection.y;
		xStream >> m_xOverrideDirection.z;
	}
}

#ifdef ZENITH_TOOLS
void Zenith_ParticleEmitterComponent::RenderPropertiesPanel()
{
	if (ImGui::CollapsingHeader("Particle Emitter", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Emitting", &m_bEmitting);

		ImGui::Text("Alive Particles: %u", m_uAliveCount);

		if (m_pxConfig != nullptr)
		{
			ImGui::Text("Max Particles: %u", m_pxConfig->m_uMaxParticles);
			ImGui::Text("Compute Mode: %s", m_pxConfig->m_bUseGPUCompute ? "GPU" : "CPU");

			if (ImGui::Button("Emit Burst"))
			{
				Emit(m_pxConfig->m_uBurstCount > 0 ? m_pxConfig->m_uBurstCount : 10);
			}

			ImGui::Separator();

			// Show config editor inline
			if (ImGui::TreeNode("Config"))
			{
				m_pxConfig->RenderPropertiesPanel();
				ImGui::TreePop();
			}
		}
		else
		{
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No config assigned");
		}

		ImGui::Separator();

		// Position override
		ImGui::Checkbox("Use Position Override", &m_bUsePositionOverride);
		if (m_bUsePositionOverride)
		{
			ImGui::DragFloat3("Override Position", &m_xOverridePosition.x, 0.1f);
			ImGui::DragFloat3("Override Direction", &m_xOverrideDirection.x, 0.1f);
		}
	}
}
#endif
