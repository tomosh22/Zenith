#include "Zenith.h"

#include "Flux/Particles/Flux_ParticleGPU.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"

#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "Vulkan/Zenith_Vulkan_Pipeline.h"
#include "DebugVariables/Zenith_DebugVariables.h"

// Maximum particles across all GPU emitters
static constexpr uint32_t s_uMaxGPUParticles = 4096;

// Compute workgroup size (must match shader)
static constexpr uint32_t s_uWorkgroupSize = 64;

// ========== GPU Emitter Registry ==========

struct GPUEmitterData
{
	Flux_ParticleEmitterConfig* m_pxConfig = nullptr;
	uint32_t m_uMaxParticles = 0;
	uint32_t m_uBaseOffset = 0;  // Offset into global particle buffer
	uint32_t m_uCurrentParticleCount = 0;  // Current alive particles in this emitter's range

	// Pending spawn data
	uint32_t m_uPendingSpawnCount = 0;
	Zenith_Maths::Vector3 m_xSpawnPosition;
	Zenith_Maths::Vector3 m_xSpawnDirection;
};

static Zenith_Vector<GPUEmitterData> s_axEmitters;
static uint32_t s_uNextEmitterID = 0;
static uint32_t s_uTotalAllocatedParticles = 0;

// CPU staging buffer for spawning new particles
static Flux_Particle* s_pxStagingBuffer = nullptr;
static uint32_t s_uStagingBufferSize = 0;

// Random number generator for particle spawning
#include <random>
static std::mt19937 s_xRng{ std::random_device{}() };
static std::uniform_real_distribution<float> s_xDist{ 0.0f, 1.0f };
static float RandomFloat() { return s_xDist(s_xRng); }

// ========== GPU Buffers ==========

// Double-buffered particle storage (ping-pong)
static Flux_ReadWriteBuffer s_xParticleBufferA;
static Flux_ReadWriteBuffer s_xParticleBufferB;
static bool s_bUseBufferA = true;  // Current read buffer

// Render instance output buffer (needs UAV for compute write, used as vertex buffer for rendering)
static Flux_ReadWriteBuffer s_xInstanceBuffer;

// Atomic counter for alive particles (read back for draw count)
static Flux_IndirectBuffer s_xCounterBuffer;
static uint32_t s_uAliveCount = 0;

// ========== Compute Pipeline ==========

static Flux_Pipeline s_xComputePipeline;
static Flux_Shader s_xComputeShader;
static Flux_RootSig s_xComputeRootSig;
static Flux_CommandList s_xComputeCommandList("Particle GPU Compute");

// Push constants for compute shader
struct ParticleComputeConstants
{
	float m_fDeltaTime;
	uint32_t m_uParticleCount;
	float m_fPad0;
	float m_fPad1;
	Zenith_Maths::Vector4 m_xGravity;  // xyz=gravity, w=drag
};

DEBUGVAR bool dbg_bEnableGPUParticles = true;

void Flux_ParticleGPU::Initialise()
{
	// Initialize compute shader
	s_xComputeShader.InitialiseCompute("Particles/Flux_ParticleUpdate.comp");

	// Build compute root signature
	// Binding 0: Frame constants (not currently used, but reserved)
	// Binding 1: Scratch buffer for push constants
	// Binding 2: Input particles (storage buffer, read) (was 0)
	// Binding 3: Output particles (storage buffer, write) (was 1)
	// Binding 4: Instance buffer (storage buffer, write) (was 2)
	// Binding 5: Counter buffer (storage buffer, read/write atomic) (was 3)
	Flux_PipelineLayout xComputeLayout;
	xComputeLayout.m_uNumDescriptorSets = 1;
	xComputeLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // Input particles (keep at 0 for now - will be storage buffer)
	xComputeLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_BUFFER;          // Scratch buffer for push constants
	xComputeLayout.m_axDescriptorSetLayouts[0].m_axBindings[2].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // Output particles (was 1)
	xComputeLayout.m_axDescriptorSetLayouts[0].m_axBindings[3].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // Instance buffer (was 2)
	xComputeLayout.m_axDescriptorSetLayouts[0].m_axBindings[4].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // Counter buffer (was 3)
	xComputeLayout.m_axDescriptorSetLayouts[0].m_axBindings[5].m_eType = DESCRIPTOR_TYPE_MAX;

	Zenith_Vulkan_RootSigBuilder::FromSpecification(s_xComputeRootSig, xComputeLayout);

	// Build compute pipeline
	Zenith_Vulkan_ComputePipelineBuilder xComputeBuilder;
	xComputeBuilder.WithShader(s_xComputeShader)
		.WithLayout(s_xComputeRootSig.m_xLayout)
		.Build(s_xComputePipeline);

	s_xComputePipeline.m_xRootSig = s_xComputeRootSig;

	// Allocate particle buffers (double-buffered)
	Flux_MemoryManager::InitialiseReadWriteBuffer(
		nullptr,
		sizeof(Flux_Particle) * s_uMaxGPUParticles,
		s_xParticleBufferA
	);
	Flux_MemoryManager::InitialiseReadWriteBuffer(
		nullptr,
		sizeof(Flux_Particle) * s_uMaxGPUParticles,
		s_xParticleBufferB
	);

	// Allocate instance buffer for rendering (storage buffer for compute, vertex buffer for rendering)
	Flux_MemoryManager::InitialiseReadWriteBuffer(
		nullptr,
		sizeof(Flux_ParticleInstance) * s_uMaxGPUParticles,
		s_xInstanceBuffer
	);

	// Allocate counter buffer (single uint32_t)
	Flux_MemoryManager::InitialiseIndirectBuffer(
		sizeof(uint32_t),
		s_xCounterBuffer
	);

	// Allocate CPU staging buffer for spawning
	s_pxStagingBuffer = new Flux_Particle[s_uMaxGPUParticles];
	s_uStagingBufferSize = s_uMaxGPUParticles;

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "GPU Particles" }, dbg_bEnableGPUParticles);
#endif

	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_ParticleGPU initialised (max %u particles)", s_uMaxGPUParticles);
}

void Flux_ParticleGPU::Shutdown()
{
	Flux_MemoryManager::DestroyReadWriteBuffer(s_xParticleBufferA);
	Flux_MemoryManager::DestroyReadWriteBuffer(s_xParticleBufferB);
	Flux_MemoryManager::DestroyReadWriteBuffer(s_xInstanceBuffer);
	Flux_MemoryManager::DestroyIndirectBuffer(s_xCounterBuffer);

	// Free CPU staging buffer
	delete[] s_pxStagingBuffer;
	s_pxStagingBuffer = nullptr;
	s_uStagingBufferSize = 0;

	s_axEmitters.Clear();
	s_uNextEmitterID = 0;
	s_uTotalAllocatedParticles = 0;

	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_ParticleGPU shut down");
}

void Flux_ParticleGPU::Reset()
{
	// Clear emitter spawn queues
	for (uint32_t i = 0; i < s_axEmitters.GetSize(); ++i)
	{
		s_axEmitters.Get(i).m_uPendingSpawnCount = 0;
	}

	s_uAliveCount = 0;
	s_xComputeCommandList.Reset(true);

	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_ParticleGPU::Reset()");
}

uint32_t Flux_ParticleGPU::RegisterEmitter(Flux_ParticleEmitterConfig* pxConfig, uint32_t uMaxParticles)
{
	if (s_uTotalAllocatedParticles + uMaxParticles > s_uMaxGPUParticles)
	{
		Zenith_Log(LOG_CATEGORY_PARTICLES, "ERROR: Cannot register GPU emitter - would exceed max particles (%u + %u > %u)",
			s_uTotalAllocatedParticles, uMaxParticles, s_uMaxGPUParticles);
		return UINT32_MAX;
	}

	GPUEmitterData xEmitter;
	xEmitter.m_pxConfig = pxConfig;
	xEmitter.m_uMaxParticles = uMaxParticles;
	xEmitter.m_uBaseOffset = s_uTotalAllocatedParticles;
	xEmitter.m_uPendingSpawnCount = 0;

	s_uTotalAllocatedParticles += uMaxParticles;

	uint32_t uID = s_uNextEmitterID++;
	s_axEmitters.PushBack(xEmitter);

	Zenith_Log(LOG_CATEGORY_PARTICLES, "Registered GPU emitter %u (max %u particles, offset %u)",
		uID, uMaxParticles, xEmitter.m_uBaseOffset);

	return uID;
}

void Flux_ParticleGPU::UnregisterEmitter(uint32_t uEmitterID)
{
	if (uEmitterID >= s_axEmitters.GetSize())
	{
		return;
	}

	// Mark as inactive (simple approach - doesn't reclaim space)
	s_axEmitters.Get(uEmitterID).m_pxConfig = nullptr;
	s_axEmitters.Get(uEmitterID).m_uMaxParticles = 0;

	Zenith_Log(LOG_CATEGORY_PARTICLES, "Unregistered GPU emitter %u", uEmitterID);
}

void Flux_ParticleGPU::QueueSpawn(uint32_t uEmitterID, uint32_t uCount,
	const Zenith_Maths::Vector3& xPosition,
	const Zenith_Maths::Vector3& xDirection)
{
	if (uEmitterID >= s_axEmitters.GetSize())
	{
		return;
	}

	GPUEmitterData& xEmitter = s_axEmitters.Get(uEmitterID);
	if (xEmitter.m_pxConfig == nullptr)
	{
		return;
	}

	// Accumulate spawn requests (will be processed on next dispatch)
	xEmitter.m_uPendingSpawnCount += uCount;
	xEmitter.m_xSpawnPosition = xPosition;
	xEmitter.m_xSpawnDirection = xDirection;
}

// Helper to get a random direction within a cone
static Zenith_Maths::Vector3 GetRandomDirectionInCone(const Zenith_Maths::Vector3& xDir, float fSpreadAngleDegrees)
{
	if (fSpreadAngleDegrees <= 0.0f)
	{
		return glm::normalize(xDir);
	}

	float fSpreadRad = glm::radians(fSpreadAngleDegrees);
	float fPhi = RandomFloat() * 2.0f * 3.14159265359f;
	float fCosTheta = 1.0f - RandomFloat() * (1.0f - cos(fSpreadRad));
	float fSinTheta = sqrt(1.0f - fCosTheta * fCosTheta);

	Zenith_Maths::Vector3 xLocalDir(
		fSinTheta * cos(fPhi),
		fCosTheta,
		fSinTheta * sin(fPhi)
	);

	Zenith_Maths::Vector3 xUp(0.0f, 1.0f, 0.0f);
	Zenith_Maths::Vector3 xEmitNorm = glm::normalize(xDir);

	if (glm::abs(glm::dot(xUp, xEmitNorm)) > 0.999f)
	{
		return xEmitNorm.y > 0.0f ? xLocalDir : -xLocalDir;
	}

	Zenith_Maths::Vector3 xAxis = glm::normalize(glm::cross(xUp, xEmitNorm));
	float fAngle = acos(glm::clamp(glm::dot(xUp, xEmitNorm), -1.0f, 1.0f));
	Zenith_Maths::Quaternion xRot = glm::angleAxis(fAngle, xAxis);

	return glm::normalize(xRot * xLocalDir);
}

void Flux_ParticleGPU::ProcessPendingSpawns()
{
	if (s_pxStagingBuffer == nullptr)
	{
		return;
	}

	// Determine which buffer we're writing to (the current input buffer that compute will read)
	Flux_ReadWriteBuffer& xInputBuffer = s_bUseBufferA ? s_xParticleBufferA : s_xParticleBufferB;

	// Process each emitter's pending spawns
	for (uint32_t uEmitterIdx = 0; uEmitterIdx < s_axEmitters.GetSize(); ++uEmitterIdx)
	{
		GPUEmitterData& xEmitter = s_axEmitters.Get(uEmitterIdx);

		if (xEmitter.m_pxConfig == nullptr || xEmitter.m_uPendingSpawnCount == 0)
		{
			continue;
		}

		Flux_ParticleEmitterConfig* pxConfig = xEmitter.m_pxConfig;

		// Calculate how many particles we can actually spawn
		uint32_t uAvailableSlots = xEmitter.m_uMaxParticles - xEmitter.m_uCurrentParticleCount;
		uint32_t uSpawnCount = (xEmitter.m_uPendingSpawnCount < uAvailableSlots)
			? xEmitter.m_uPendingSpawnCount : uAvailableSlots;

		if (uSpawnCount == 0)
		{
			xEmitter.m_uPendingSpawnCount = 0;
			continue;
		}

		// Fill staging buffer with new particles
		for (uint32_t i = 0; i < uSpawnCount; ++i)
		{
			Flux_Particle& xP = s_pxStagingBuffer[i];

			// Position and age (start at 0)
			xP.SetPosition(xEmitter.m_xSpawnPosition);
			xP.SetAge(0.0f);

			// Lifetime
			float fLifetime = pxConfig->m_fLifetimeMin +
				RandomFloat() * (pxConfig->m_fLifetimeMax - pxConfig->m_fLifetimeMin);
			xP.SetLifetime(fLifetime);

			// Velocity
			Zenith_Maths::Vector3 xRandomDir = GetRandomDirectionInCone(
				xEmitter.m_xSpawnDirection, pxConfig->m_fSpreadAngleDegrees);
			float fSpeed = pxConfig->m_fSpeedMin +
				RandomFloat() * (pxConfig->m_fSpeedMax - pxConfig->m_fSpeedMin);
			xP.SetVelocity(xRandomDir * fSpeed);

			// Colors
			xP.m_xColorStart = pxConfig->m_xColorStart;
			xP.m_xColorEnd = pxConfig->m_xColorEnd;

			// Size
			xP.SetSizeStart(pxConfig->m_fSizeStart);
			xP.SetSizeEnd(pxConfig->m_fSizeEnd);

			// Rotation
			float fRotation = pxConfig->m_fRotationMin +
				RandomFloat() * (pxConfig->m_fRotationMax - pxConfig->m_fRotationMin);
			xP.SetRotation(fRotation);

			float fRotationSpeed = pxConfig->m_fRotationSpeedMin +
				RandomFloat() * (pxConfig->m_fRotationSpeedMax - pxConfig->m_fRotationSpeedMin);
			xP.SetRotationSpeed(fRotationSpeed);

			xP.m_xPadding = Zenith_Maths::Vector4(0.0f);
		}

		// Upload to GPU buffer at the emitter's offset + current particle count
		uint32_t uUploadOffset = xEmitter.m_uBaseOffset + xEmitter.m_uCurrentParticleCount;
		Flux_MemoryManager::UploadBufferDataAtOffset(
			xInputBuffer.GetBuffer().m_xVRAMHandle,
			s_pxStagingBuffer,
			uSpawnCount * sizeof(Flux_Particle),
			uUploadOffset * sizeof(Flux_Particle)  // Offset in bytes
		);

		xEmitter.m_uCurrentParticleCount += uSpawnCount;
		xEmitter.m_uPendingSpawnCount = 0;

		Zenith_Log(LOG_CATEGORY_PARTICLES, "GPU: Spawned %u particles for emitter %u (total: %u)",
			uSpawnCount, uEmitterIdx, xEmitter.m_uCurrentParticleCount);
	}
}

void Flux_ParticleGPU::DispatchCompute()
{
	if (!dbg_bEnableGPUParticles)
	{
		return;
	}

	if (s_axEmitters.GetSize() == 0)
	{
		return;
	}

	// Process pending particle spawns before compute dispatch
	ProcessPendingSpawns();

	// Get delta time
	float fDt = Zenith_Core::GetDt();

	// Determine which buffers to use (ping-pong)
	Flux_ReadWriteBuffer& xInputBuffer = s_bUseBufferA ? s_xParticleBufferA : s_xParticleBufferB;
	Flux_ReadWriteBuffer& xOutputBuffer = s_bUseBufferA ? s_xParticleBufferB : s_xParticleBufferA;

	// Reset counter to zero
	uint32_t uZero = 0;
	Flux_MemoryManager::UploadBufferData(
		s_xCounterBuffer.GetBuffer().m_xVRAMHandle,
		&uZero,
		sizeof(uint32_t)
	);

	s_xComputeCommandList.Reset(false);

	s_xComputeCommandList.AddCommand<Flux_CommandBindComputePipeline>(&s_xComputePipeline);

	s_xComputeCommandList.AddCommand<Flux_CommandBeginBind>(0);
	s_xComputeCommandList.AddCommand<Flux_CommandBindUAV_Buffer>(&xInputBuffer.GetUAV(), 0);
	s_xComputeCommandList.AddCommand<Flux_CommandBindUAV_Buffer>(&xOutputBuffer.GetUAV(), 2);   // Was 1
	s_xComputeCommandList.AddCommand<Flux_CommandBindUAV_Buffer>(&s_xInstanceBuffer.GetUAV(), 3);  // Was 2
	s_xComputeCommandList.AddCommand<Flux_CommandBindUAV_Buffer>(&s_xCounterBuffer.GetUAV(), 4);   // Was 3

	// Set up push constants
	ParticleComputeConstants xConstants;
	xConstants.m_fDeltaTime = fDt;
	xConstants.m_uParticleCount = s_uTotalAllocatedParticles;
	xConstants.m_xGravity = Zenith_Maths::Vector4(0.0f, -9.8f, 0.0f, 0.0f);  // Default gravity

	s_xComputeCommandList.AddCommand<Flux_CommandPushConstant>(&xConstants, sizeof(xConstants));

	// Dispatch compute shader
	uint32_t uWorkgroups = (s_uTotalAllocatedParticles + s_uWorkgroupSize - 1) / s_uWorkgroupSize;
	s_xComputeCommandList.AddCommand<Flux_CommandDispatch>(uWorkgroups, 1, 1);

	Flux::SubmitCommandList(&s_xComputeCommandList, Flux_Graphics::s_xNullTargetSetup, RENDER_ORDER_PARTICLES_COMPUTE);

	// Swap buffers for next frame
	s_bUseBufferA = !s_bUseBufferA;

	// Clear pending spawns (processed by compute shader)
	for (uint32_t i = 0; i < s_axEmitters.GetSize(); ++i)
	{
		s_axEmitters.Get(i).m_uPendingSpawnCount = 0;
	}

	// Note: We can't read back the counter immediately as compute hasn't finished yet
	// The alive count will be available on the next frame
	// For now, use a conservative estimate
	s_uAliveCount = s_uTotalAllocatedParticles;
}

Flux_ReadWriteBuffer& Flux_ParticleGPU::GetInstanceBuffer()
{
	return s_xInstanceBuffer;
}

uint32_t Flux_ParticleGPU::GetAliveCount()
{
	return s_uAliveCount;
}

bool Flux_ParticleGPU::HasGPUEmitters()
{
	for (uint32_t i = 0; i < s_axEmitters.GetSize(); ++i)
	{
		if (s_axEmitters.Get(i).m_pxConfig != nullptr)
		{
			return true;
		}
	}
	return false;
}
