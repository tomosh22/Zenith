#pragma once

#include "AssetHandling/Zenith_Asset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "DataStream/Zenith_DataStream.h"
#include "Maths/Zenith_Maths.h"
#include <string>
#include <unordered_map>

/**
 * Flux_ParticleEmitterConfig - Serializable asset for configuring particle emitters
 *
 * This class defines all the parameters for a particle emitter, including:
 * - Spawn settings (rate, burst count, max particles)
 * - Lifetime range
 * - Initial velocity (cone emitter with direction and spread)
 * - Physics (gravity, drag)
 * - Color over lifetime
 * - Size over lifetime
 * - Visual settings (texture, blend mode)
 * - Compute mode (CPU or GPU)
 *
 * Configs are created programmatically in game code (Project_LoadInitialScene)
 * and can be shared between multiple emitter components.
 */
class Flux_ParticleEmitterConfig : public Zenith_Asset
{
public:
	ZENITH_ASSET_TYPE_NAME(Flux_ParticleEmitterConfig)

	Flux_ParticleEmitterConfig() = default;
	virtual ~Flux_ParticleEmitterConfig() = default;

	//--- Global Config Registry ---//
	// Allows configs to be looked up by name after scene restore

	// Register a config with a unique name (call from Project_LoadInitialScene)
	static void Register(const std::string& strName, Flux_ParticleEmitterConfig* pxConfig)
	{
		s_xConfigRegistry[strName] = pxConfig;
		pxConfig->m_strRegisteredName = strName;
	}

	// Find a registered config by name (returns nullptr if not found)
	static Flux_ParticleEmitterConfig* Find(const std::string& strName)
	{
		auto it = s_xConfigRegistry.find(strName);
		return (it != s_xConfigRegistry.end()) ? it->second : nullptr;
	}

	// Unregister a config (call from Project_Shutdown)
	static void Unregister(const std::string& strName)
	{
		s_xConfigRegistry.erase(strName);
	}

	// Clear all registered configs
	static void ClearRegistry()
	{
		s_xConfigRegistry.clear();
	}

	// Get the registered name of this config (empty if not registered)
	const std::string& GetRegisteredName() const { return m_strRegisteredName; }

	//--- Spawn Settings ---//

	// Particles spawned per second (for continuous emission)
	// Set to 0 for burst-only emitters
	float m_fSpawnRate = 10.0f;

	// Number of particles to spawn in a single burst
	// Set to 0 to disable burst mode
	uint32_t m_uBurstCount = 0;

	// Maximum number of particles this emitter can have alive at once
	uint32_t m_uMaxParticles = 256;

	//--- Lifetime ---//

	// Minimum particle lifetime in seconds
	float m_fLifetimeMin = 1.0f;

	// Maximum particle lifetime in seconds
	float m_fLifetimeMax = 2.0f;

	//--- Velocity (Cone Emitter) ---//

	// Direction of emission (will be normalized)
	Zenith_Maths::Vector3 m_xEmitDirection = { 0.0f, 1.0f, 0.0f };

	// Half-angle of the emission cone in degrees
	// 0 = particles emit exactly in m_xEmitDirection
	// 90 = particles emit in a hemisphere
	// 180 = particles emit in all directions
	float m_fSpreadAngleDegrees = 30.0f;

	// Minimum initial speed
	float m_fSpeedMin = 5.0f;

	// Maximum initial speed
	float m_fSpeedMax = 10.0f;

	//--- Physics ---//

	// Constant acceleration applied to particles (typically gravity)
	Zenith_Maths::Vector3 m_xGravity = { 0.0f, -9.8f, 0.0f };

	// Velocity damping per second (0 = no drag, 1 = full stop in 1 second)
	float m_fDrag = 0.0f;

	//--- Color Over Lifetime ---//

	// Color at spawn (RGBA, premultiplied alpha)
	Zenith_Maths::Vector4 m_xColorStart = { 1.0f, 1.0f, 1.0f, 1.0f };

	// Color at death (RGBA, premultiplied alpha)
	Zenith_Maths::Vector4 m_xColorEnd = { 1.0f, 1.0f, 1.0f, 0.0f };

	//--- Size Over Lifetime ---//

	// Size at spawn (in world units)
	float m_fSizeStart = 1.0f;

	// Size at death (in world units)
	float m_fSizeEnd = 0.5f;

	//--- Rotation ---//

	// Initial rotation range in radians
	float m_fRotationMin = 0.0f;
	float m_fRotationMax = 0.0f;

	// Rotation speed range in radians per second
	float m_fRotationSpeedMin = 0.0f;
	float m_fRotationSpeedMax = 0.0f;

	//--- Visual Settings ---//

	// Path to particle texture (empty = colored quads with circular gradient)
	std::string m_strTexturePath;

	//--- Compute Mode ---//

	// If true, particles are simulated on the GPU via compute shader
	// If false, particles are simulated on the CPU
	// GPU mode is better for large particle counts (>1000)
	// CPU mode is better for small bursts and effects that need precise control
	bool m_bUseGPUCompute = false;

	//--- Serialization ---//

	void WriteToDataStream(Zenith_DataStream& xStream) const override
	{
		// Version number for forward compatibility
		uint32_t uVersion = 1;
		xStream << uVersion;

		// Spawn settings
		xStream << m_fSpawnRate;
		xStream << m_uBurstCount;
		xStream << m_uMaxParticles;

		// Lifetime
		xStream << m_fLifetimeMin;
		xStream << m_fLifetimeMax;

		// Velocity
		xStream << m_xEmitDirection;
		xStream << m_fSpreadAngleDegrees;
		xStream << m_fSpeedMin;
		xStream << m_fSpeedMax;

		// Physics
		xStream << m_xGravity;
		xStream << m_fDrag;

		// Color
		xStream << m_xColorStart;
		xStream << m_xColorEnd;

		// Size
		xStream << m_fSizeStart;
		xStream << m_fSizeEnd;

		// Rotation
		xStream << m_fRotationMin;
		xStream << m_fRotationMax;
		xStream << m_fRotationSpeedMin;
		xStream << m_fRotationSpeedMax;

		// Visual
		xStream << m_strTexturePath;

		// Compute mode
		xStream << m_bUseGPUCompute;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream) override
	{
		uint32_t uVersion;
		xStream >> uVersion;

		if (uVersion >= 1)
		{
			// Spawn settings
			xStream >> m_fSpawnRate;
			xStream >> m_uBurstCount;
			xStream >> m_uMaxParticles;

			// Lifetime
			xStream >> m_fLifetimeMin;
			xStream >> m_fLifetimeMax;

			// Velocity
			xStream >> m_xEmitDirection;
			xStream >> m_fSpreadAngleDegrees;
			xStream >> m_fSpeedMin;
			xStream >> m_fSpeedMax;

			// Physics
			xStream >> m_xGravity;
			xStream >> m_fDrag;

			// Color
			xStream >> m_xColorStart;
			xStream >> m_xColorEnd;

			// Size
			xStream >> m_fSizeStart;
			xStream >> m_fSizeEnd;

			// Rotation
			xStream >> m_fRotationMin;
			xStream >> m_fRotationMax;
			xStream >> m_fRotationSpeedMin;
			xStream >> m_fRotationSpeedMax;

			// Visual
			xStream >> m_strTexturePath;

			// Compute mode
			xStream >> m_bUseGPUCompute;
		}
	}

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel() override;
#endif

	//--- Helper Methods ---//

	// Get a random lifetime within the configured range
	float GetRandomLifetime() const
	{
		return m_fLifetimeMin + (static_cast<float>(rand()) / RAND_MAX) * (m_fLifetimeMax - m_fLifetimeMin);
	}

	// Get a random speed within the configured range
	float GetRandomSpeed() const
	{
		return m_fSpeedMin + (static_cast<float>(rand()) / RAND_MAX) * (m_fSpeedMax - m_fSpeedMin);
	}

	// Get a random rotation within the configured range
	float GetRandomRotation() const
	{
		return m_fRotationMin + (static_cast<float>(rand()) / RAND_MAX) * (m_fRotationMax - m_fRotationMin);
	}

	// Get a random rotation speed within the configured range
	float GetRandomRotationSpeed() const
	{
		return m_fRotationSpeedMin + (static_cast<float>(rand()) / RAND_MAX) * (m_fRotationSpeedMax - m_fRotationSpeedMin);
	}

	// Get a random direction within the emission cone
	Zenith_Maths::Vector3 GetRandomDirection() const
	{
		if (m_fSpreadAngleDegrees <= 0.0f)
		{
			return glm::normalize(m_xEmitDirection);
		}

		// Generate a random direction within a cone
		float fSpreadRad = glm::radians(m_fSpreadAngleDegrees);
		float fPhi = static_cast<float>(rand()) / RAND_MAX * 2.0f * 3.14159265359f;
		float fCosTheta = 1.0f - (static_cast<float>(rand()) / RAND_MAX) * (1.0f - cos(fSpreadRad));
		float fSinTheta = sqrt(1.0f - fCosTheta * fCosTheta);

		// Local direction in cone space (pointing up +Y)
		Zenith_Maths::Vector3 xLocalDir(
			fSinTheta * cos(fPhi),
			fCosTheta,
			fSinTheta * sin(fPhi)
		);

		// Rotate to align with emit direction
		Zenith_Maths::Vector3 xUp(0.0f, 1.0f, 0.0f);
		Zenith_Maths::Vector3 xEmitNorm = glm::normalize(m_xEmitDirection);

		if (glm::abs(glm::dot(xUp, xEmitNorm)) > 0.999f)
		{
			// Emit direction is nearly parallel to up, use direct rotation
			return xEmitNorm.y > 0.0f ? xLocalDir : -xLocalDir;
		}

		// Build rotation from up to emit direction
		Zenith_Maths::Vector3 xAxis = glm::normalize(glm::cross(xUp, xEmitNorm));
		float fAngle = acos(glm::dot(xUp, xEmitNorm));
		Zenith_Maths::Quaternion xRot = glm::angleAxis(fAngle, xAxis);

		return glm::normalize(xRot * xLocalDir);
	}

private:
	static inline std::unordered_map<std::string, Flux_ParticleEmitterConfig*> s_xConfigRegistry;
	std::string m_strRegisteredName;
};
