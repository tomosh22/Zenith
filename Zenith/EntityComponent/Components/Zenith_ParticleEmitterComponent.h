#pragma once

#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"  // Zenith_EntityID + entity/component templates (no longer transitive via the now-opaque Scene.h)
#include "Core/Zenith_ParticleData.h"  // EC-side mirror of Flux_Particle (keeps this header Flux-free)
#include <random>
#include <string>

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
 * For GPU compute simulation (config m_bUseGPUCompute), the component still owns
 * spawn TIMING but nothing else: it holds a registration in Flux_ParticleGPU's
 * shared pool, forwards each burst as a QueueSpawn, and keeps no CPU particle
 * array at all — the compute pass integrates the particles and emits their draw
 * instances without a readback. m_axParticles / m_uAliveCount stay empty on that
 * path, which is why the render gather skips GPU emitters.
 */
class Zenith_ParticleEmitterComponent
{
public:
	Zenith_ParticleEmitterComponent() = default;
	Zenith_ParticleEmitterComponent(Zenith_Entity& xParentEntity);

	// A GPU emitter holds a pool registration, so this component owns a resource
	// and needs the full ownership treatment (the same shape Zenith_AnimatorComponent
	// uses for its store entry): move transfers the registration and neutralises the
	// source, copy is gone, and the dtor releases whatever is still held. Releasing
	// is idempotent — it clears m_uGPUEmitterID — so OnDestroy and the dtor can both
	// run and exactly one Unregister reaches the pool.
	~Zenith_ParticleEmitterComponent();

	Zenith_ParticleEmitterComponent(Zenith_ParticleEmitterComponent&& xOther) noexcept;
	Zenith_ParticleEmitterComponent& operator=(Zenith_ParticleEmitterComponent&& xOther) noexcept;

	Zenith_ParticleEmitterComponent(const Zenith_ParticleEmitterComponent&) = delete;
	Zenith_ParticleEmitterComponent& operator=(const Zenith_ParticleEmitterComponent&) = delete;

	// ECS lifecycle (concept-detected by the component meta registry).
	void OnDestroy();

	//--- Configuration ---//

	// Set the emitter configuration (not owned, caller manages lifetime)
	void SetConfig(Flux_ParticleEmitterConfig* pxConfig);
	Flux_ParticleEmitterConfig* GetConfig() const { return m_pxConfig; }

	// Look up a registered Flux_ParticleEmitterConfig BY NAME and apply it.
	// Returns false (config left unchanged) if no config with that name exists.
	// The Flux_ParticleEmitterConfig::Find dependency lives in the .cpp (the
	// allow-listed EntityComponent->Flux bridge), so callers -- e.g. the graph-node
	// registrars -- can assign a named config without including the Flux header.
	bool SetConfigByName(const std::string& strConfigName);

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

	// CPU path only — a GPU emitter's particles live in VRAM and are never mirrored
	// back, so both of these read empty for one (use UsesGPUCompute to tell).
	const Zenith_Vector<Zenith_ParticleData>& GetParticles() const { return m_axParticles; }
	uint32_t GetAliveCount() const { return m_uAliveCount; }

	// True once this emitter holds a live registration in the GPU pool. A config
	// that ASKS for GPU compute but could not be registered (pool exhausted) reports
	// false and runs on the CPU — the fallback is silent to every caller but the log.
	bool UsesGPUCompute() const;

	// This emitter's slot in Flux_ParticleGPU's shared pool, or UINT32_MAX when it
	// holds none. Only meaningful while UsesGPUCompute(); it is the key the pool's
	// own accessors take.
	uint32_t GetGPUEmitterID() const { return m_uGPUEmitterID; }

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

	// Hand this emitter's pool registration back. Idempotent (clears the ID), so the
	// dtor and OnDestroy can both call it.
	void ReleaseGPUEmitter();

	// Allocate the CPU particle array for m_pxConfig's capacity (the CPU path, and
	// the fallback when a GPU registration is refused).
	void InitialiseCPUParticles();

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
	Zenith_Vector<Zenith_ParticleData> m_axParticles;
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
