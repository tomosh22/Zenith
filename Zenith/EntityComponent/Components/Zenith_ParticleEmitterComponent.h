#pragma once

#include "EntityComponent/Zenith_Scene.h"
#include "Flux/Particles/Flux_ParticleData.h"
#include "Collections/Zenith_Vector.h"
#include <random>

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

class Flux_ParticleEmitterConfig;
class Zenith_DataStream;

/**
 * Zenith_ParticleEmitterComponent - Manages particle emission and simulation
 *
 * This component handles:
 * - Continuous emission at a configurable rate
 * - Burst emission for one-shot effects
 * - CPU-based particle simulation (position, velocity, color, size over time)
 * - Position/direction override for spawning at specific locations
 *
 * For GPU compute simulation, the component still manages spawn timing
 * but delegates physics updates to the GPU via Flux_ParticleGPU.
 */
class Zenith_ParticleEmitterComponent
{
public:
	Zenith_ParticleEmitterComponent() = default;
	Zenith_ParticleEmitterComponent(Zenith_Entity& xParentEntity);
	~Zenith_ParticleEmitterComponent() = default;

	//--- Configuration ---//

	// Set the emitter configuration (not owned, caller manages lifetime)
	void SetConfig(Flux_ParticleEmitterConfig* pxConfig);
	Flux_ParticleEmitterConfig* GetConfig() const { return m_pxConfig; }

	//--- Emission Control ---//

	// Emit a burst of particles immediately
	void Emit(uint32_t uCount);

	// Enable/disable continuous emission
	void SetEmitting(bool bEmitting) { m_bEmitting = bEmitting; }
	bool IsEmitting() const { return m_bEmitting; }

	//--- Position Override ---//

	// Override the emission position (instead of using transform component)
	void SetEmitPosition(const Zenith_Maths::Vector3& xPos);

	// Override the emission direction
	void SetEmitDirection(const Zenith_Maths::Vector3& xDir);

	// Clear position/direction override, use transform component instead
	void ClearPositionOverride();

	//--- Lifecycle ---//

	// Called every frame by the particle system
	void Update(float fDt);

	//--- Particle Access (for rendering) ---//

	const Zenith_Vector<Flux_Particle>& GetParticles() const { return m_axParticles; }
	uint32_t GetAliveCount() const { return m_uAliveCount; }

	// Check if this emitter uses GPU compute
	bool UsesGPUCompute() const;

	//--- Entity Access ---//

	Zenith_Entity& GetParentEntity() { return m_xParentEntity; }
	const Zenith_Entity& GetParentEntity() const { return m_xParentEntity; }

	//--- Serialization ---//

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

private:
	//--- Internal Methods ---//

	// Run CPU simulation for one frame
	void SimulateCPU(float fDt);

	// Spawn a single particle at the given position/direction
	void SpawnParticle(const Zenith_Maths::Vector3& xPos, const Zenith_Maths::Vector3& xDir);

	// Get the current emission position (override or transform)
	Zenith_Maths::Vector3 GetEmitPosition() const;

	// Get the current emission direction (override or config default)
	Zenith_Maths::Vector3 GetEmitDirection() const;

	// Get a random float in [0, 1]
	float RandomFloat() { return m_xDistribution(m_xRng); }

	//--- State ---//

	Zenith_Entity m_xParentEntity;
	Flux_ParticleEmitterConfig* m_pxConfig = nullptr;

	// Particle storage (CPU simulation)
	Zenith_Vector<Flux_Particle> m_axParticles;
	uint32_t m_uAliveCount = 0;

	// GPU emitter ID (UINT32_MAX if not using GPU compute)
	uint32_t m_uGPUEmitterID = UINT32_MAX;

	// Emission state
	bool m_bEmitting = false;
	float m_fSpawnAccumulator = 0.0f;

	// Position/direction override
	bool m_bUsePositionOverride = false;
	Zenith_Maths::Vector3 m_xOverridePosition = { 0.0f, 0.0f, 0.0f };
	Zenith_Maths::Vector3 m_xOverrideDirection = { 0.0f, 1.0f, 0.0f };

	// Random number generator
	std::mt19937 m_xRng{ std::random_device{}() };
	std::uniform_real_distribution<float> m_xDistribution{ 0.0f, 1.0f };
};
