#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Particles/Flux_ParticleData.h"
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

#include <random>

class Flux_ParticleEmitterConfig;
class Flux_CommandList;
class Zenith_Vulkan_MemoryManager;
class FrameContext;

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
class Flux_ParticleGPUImpl
{
public:
	static constexpr uint32_t s_uMaxGPUParticles = 4096;
	static constexpr uint32_t s_uWorkgroupSize   = 64;

	Flux_ParticleGPUImpl() = default;
	~Flux_ParticleGPUImpl() = default;

	Flux_ParticleGPUImpl(const Flux_ParticleGPUImpl&) = delete;
	Flux_ParticleGPUImpl& operator=(const Flux_ParticleGPUImpl&) = delete;

	void Initialise(Zenith_Vulkan_MemoryManager& xVulkanMemory, FrameContext& xFrame);
	void BuildPipelines();
	void Shutdown();
	void Reset();

	/**
	 * Register a GPU emitter for compute processing.
	 * Returns an emitter ID for later reference.
	 */
	uint32_t RegisterEmitter(Flux_ParticleEmitterConfig* pxConfig, uint32_t uMaxParticles);

	/**
	 * Unregister a GPU emitter.
	 */
	void UnregisterEmitter(uint32_t uEmitterID);

	/**
	 * Queue particle spawns for a GPU emitter.
	 * Particles will be spawned on the next compute dispatch.
	 */
	void QueueSpawn(uint32_t uEmitterID, uint32_t uCount,
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Vector3& xDirection);

	/**
	 * Spawn particles immediately into the GPU buffer.
	 * Called during DispatchCompute to process pending spawns.
	 */
	void ProcessPendingSpawns();

	/**
	 * CPU-side pre-execute: process spawns, upload counter reset.
	 * Must run sequentially before parallel recording.
	 */
	void PreExecuteCompute();

	/**
	 * Record compute shader commands to update all GPU particles.
	 */
	void DispatchCompute(Flux_CommandList* pxCmdList);

	/**
	 * Get the instance buffer for rendering GPU particles.
	 */
	Flux_ReadWriteBuffer& GetInstanceBuffer() { return m_xInstanceBuffer; }

	/**
	 * Get the number of alive GPU particles for rendering.
	 */
	uint32_t GetAliveCount() const { return m_uAliveCount; }

	/**
	 * Check if any GPU emitters are registered.
	 */
	bool HasGPUEmitters() const;

	struct EmitterData
	{
		Flux_ParticleEmitterConfig* m_pxConfig            = nullptr;
		uint32_t                    m_uMaxParticles       = 0;
		uint32_t                    m_uBaseOffset         = 0;
		uint32_t                    m_uCurrentParticleCount = 0;

		uint32_t                    m_uPendingSpawnCount  = 0;
		Zenith_Maths::Vector3       m_xSpawnPosition;
		Zenith_Maths::Vector3       m_xSpawnDirection;
	};

	// Injected cross-subsystem dependencies (stored in Initialise).
	Zenith_Vulkan_MemoryManager* m_pxVulkanMemory       = nullptr;
	FrameContext*                m_pxFrame              = nullptr;

	Zenith_Vector<EmitterData> m_axEmitters;
	uint32_t                   m_uNextEmitterID         = 0;
	uint32_t                   m_uTotalAllocatedParticles = 0;

	Flux_Particle*             m_pxStagingBuffer        = nullptr;
	uint32_t                   m_uStagingBufferSize     = 0;

	std::mt19937                          m_xRng{ std::random_device{}() };
	std::uniform_real_distribution<float> m_xDist{ 0.0f, 1.0f };

	Flux_ReadWriteBuffer m_xParticleBufferA;
	Flux_ReadWriteBuffer m_xParticleBufferB;
	bool                 m_bUseBufferA = true;

	Flux_ReadWriteBuffer m_xInstanceBuffer;

	Flux_IndirectBuffer  m_xCounterBuffer;
	uint32_t             m_uAliveCount = 0;

	Flux_Pipeline    m_xComputePipeline;
	Flux_Shader      m_xComputeShader;
	Flux_RootSig     m_xComputeRootSig;
	Flux_CommandList m_xComputeCommandList{ "Particle GPU Compute" };
};
