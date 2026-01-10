#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Particles/Flux_ParticleData.h"

class Flux_ParticleEmitterConfig;

/**
 * GPU-based particle compute system.
 *
 * Uses compute shaders to update particles on the GPU, with double-buffered
 * particle storage (ping-pong pattern) for read/write separation.
 *
 * Features:
 * - Compute shader particle update (position, velocity, age)
 * - Atomic counter for alive particle count
 * - Direct render instance generation (no CPU readback)
 * - Per-emitter integration via RegisterEmitter/UnregisterEmitter
 */
class Flux_ParticleGPU
{
public:
	static void Initialise();
	static void Shutdown();
	static void Reset();  // Clear state when scene resets

	/**
	 * Register a GPU emitter for compute processing.
	 * Returns an emitter ID for later reference.
	 */
	static uint32_t RegisterEmitter(Flux_ParticleEmitterConfig* pxConfig, uint32_t uMaxParticles);

	/**
	 * Unregister a GPU emitter.
	 */
	static void UnregisterEmitter(uint32_t uEmitterID);

	/**
	 * Queue particle spawns for a GPU emitter.
	 * Particles will be spawned on the next compute dispatch.
	 */
	static void QueueSpawn(uint32_t uEmitterID, uint32_t uCount,
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Vector3& xDirection);

	/**
	 * Spawn particles immediately into the GPU buffer.
	 * Called during DispatchCompute to process pending spawns.
	 */
	static void ProcessPendingSpawns();

	/**
	 * Run the compute shader to update all GPU particles.
	 * Called at RENDER_ORDER_PARTICLES_COMPUTE.
	 */
	static void DispatchCompute();

	/**
	 * Get the instance buffer for rendering GPU particles.
	 */
	static Flux_ReadWriteBuffer& GetInstanceBuffer();

	/**
	 * Get the number of alive GPU particles for rendering.
	 */
	static uint32_t GetAliveCount();

	/**
	 * Check if any GPU emitters are registered.
	 */
	static bool HasGPUEmitters();
};
