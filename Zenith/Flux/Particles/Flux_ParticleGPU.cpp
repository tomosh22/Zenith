#include "Zenith.h"
#include "Flux/Particles/Flux_Particles_Shaders.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Particles/Flux_ParticleGPUImpl.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"

#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GPUScene.h"        // Flux_PackResetIndirectCommand — the shared indirect packer
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Shaders/Generated/Particles.h" // typed binding handles
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"

#include <cstring>   // std::memset — zeroing a reservation before it goes live

// The particle indirect block is the ENGINE's indirect block: same five words, same
// packer. Tying the two constants means a VkDrawIndexedIndirectCommand layout change
// can never move one and leave the other.
static_assert(uFLUX_PARTICLE_INDIRECT_WORDS == uFLUX_GPUSCENE_INDIRECT_WORDS,
	"the particle draw command is a VkDrawIndexedIndirectCommand like every other indirect draw in Flux");

namespace
{
	float RandomFloat(Flux_ParticleGPUImpl& xImpl)
	{
		return xImpl.m_xDist(xImpl.m_xRng);
	}

	Zenith_Maths::Vector3 GetRandomDirectionInCone(Flux_ParticleGPUImpl& xImpl,
		const Zenith_Maths::Vector3& xDir, float fSpreadAngleDegrees)
	{
		if (fSpreadAngleDegrees <= 0.0f)
		{
			return glm::normalize(xDir);
		}

		float fSpreadRad = glm::radians(fSpreadAngleDegrees);
		float fPhi = RandomFloat(xImpl) * 2.0f * 3.14159265359f;
		float fCosTheta = 1.0f - RandomFloat(xImpl) * (1.0f - cos(fSpreadRad));
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

	// Mirrors PushConstantsLayout in Flux_ParticleUpdate.slang. One of these is
	// staged per EMITTER per frame — everything that varies per emitter lives here
	// rather than in the particle record, which is what keeps that record at 96 B
	// with no emitter back-reference.
	struct ParticleComputeConstants
	{
		float    m_fDeltaTime;
		uint32_t m_uBaseOffset;
		uint32_t m_uParticleCount;
		float    m_fTurbulence;
		Zenith_Maths::Vector4 m_xGravity;   // xyz=gravity, w=drag
		uint32_t m_uInstanceBase;
		uint32_t m_uInstanceCapacity;
		uint32_t m_uArgsWordBase;
		uint32_t m_uPad0;
	};

	// Pinned field-by-field against the reflected block — the generated struct is
	// the only thing that has actually seen the shader.
	namespace PUGen = Flux_Generated_Particles::ParticleUpdate;
	static_assert(sizeof(ParticleComputeConstants) == sizeof(PUGen::PushConstants_CB),
		"ParticleComputeConstants drifted from Flux_ParticleUpdate.slang's PushConstantsLayout");
	static_assert(offsetof(ParticleComputeConstants, m_fDeltaTime)      == offsetof(PUGen::PushConstants_CB, m_fdeltaTime),        "deltaTime lane moved");
	static_assert(offsetof(ParticleComputeConstants, m_uBaseOffset)     == offsetof(PUGen::PushConstants_CB, m_ubaseOffset),       "baseOffset lane moved");
	static_assert(offsetof(ParticleComputeConstants, m_uParticleCount)  == offsetof(PUGen::PushConstants_CB, m_uparticleCount),    "particleCount lane moved");
	static_assert(offsetof(ParticleComputeConstants, m_fTurbulence)     == offsetof(PUGen::PushConstants_CB, m_fturbulence),       "turbulence lane moved");
	static_assert(offsetof(ParticleComputeConstants, m_xGravity)        == offsetof(PUGen::PushConstants_CB, m_agravity),          "gravity lane moved");
	static_assert(offsetof(ParticleComputeConstants, m_uInstanceBase)   == offsetof(PUGen::PushConstants_CB, m_uinstanceBase),     "instanceBase lane moved");
	static_assert(offsetof(ParticleComputeConstants, m_uInstanceCapacity) == offsetof(PUGen::PushConstants_CB, m_uinstanceCapacity), "instanceCapacity lane moved");
	static_assert(offsetof(ParticleComputeConstants, m_uArgsWordBase)   == offsetof(PUGen::PushConstants_CB, m_uargsWordBase),     "argsWordBase lane moved");
}

void Flux_ParticleGPUImpl::BuildPipelines()
{
	m_xComputeShader.Initialise(Flux_ParticlesShaders::xParticleUpdate);

	const Flux_ShaderReflection& xReflection = m_xComputeShader.GetReflection();
	Flux_RootSigBuilder::FromReflection(m_xComputeRootSig, xReflection);

	Flux_ComputePipelineBuilder::BuildFromShader(m_xComputePipeline, m_xComputeShader, m_xComputeRootSig);
}

void Flux_ParticleGPUImpl::Initialise()
{
	// NOTE: no BuildPipelines() here. The Particles feature owns BOTH shader
	// programs, so its own BuildPipelines (which runs immediately before this, and
	// again on every hot-reload of either program) has already built the compute
	// pipeline. Building it a second time here would leak the first module.

	// The staging block doubles as the zero-fill source below, so it is allocated
	// FIRST and cleared once.
	m_pxStagingBuffer = new Flux_Particle[s_uMaxGPUParticles];
	m_uStagingBufferSize = s_uMaxGPUParticles;
	std::memset(m_pxStagingBuffer, 0, sizeof(Flux_Particle) * s_uMaxGPUParticles);

	Flux_MemoryManager& xVulkanMemory = g_xEngine.FluxMemory();

	// Both pool halves are created FROM the zeroed block, not from nullptr. A never-
	// spawned slot must read back age 0 / lifetime 0 so the compute step classifies it
	// dead-on-entry; fresh VRAM is undefined, and a garbage lifetime would hatch a
	// particle at a garbage position on the very first dispatch.
	xVulkanMemory.InitialiseReadWriteBuffer(
		m_pxStagingBuffer,
		sizeof(Flux_Particle) * s_uMaxGPUParticles,
		m_xParticleBufferA
	);
	xVulkanMemory.InitialiseReadWriteBuffer(
		m_pxStagingBuffer,
		sizeof(Flux_Particle) * s_uMaxGPUParticles,
		m_xParticleBufferB
	);

	// bAlsoVertexBuffer: the compute pass writes it as a UAV and ExecuteParticles
	// binds it as vertex binding 1. Sized for BOTH blend partitions.
	xVulkanMemory.InitialiseReadWriteBuffer(
		nullptr,
		sizeof(Flux_ParticleInstance) * s_uInstanceCapacity,
		m_xInstanceBuffer,
		/*bAlsoVertexBuffer*/ true
	);

	xVulkanMemory.InitialiseIndirectBuffer(
		uFLUX_PARTICLE_INDIRECT_STRIDE * uFLUX_PARTICLE_PARTITION_COUNT,
		m_xIndirectArgsBuffer
	);

	// Seed the commands once at boot as well as every frame: the frame seeding is
	// skipped while the system is inactive, and a zero-instanceCount command is the
	// only safe thing for the indirect buffer to hold in the meantime.
	SeedIndirectCommands();

	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_ParticleGPU initialised (max %u particles, %u instance slots)",
		s_uMaxGPUParticles, s_uInstanceCapacity);
}

void Flux_ParticleGPUImpl::Shutdown()
{
	m_xComputePipeline.Reset();
	m_xComputeShader.Reset();
	m_xComputeRootSig = Flux_RootSig();

	Flux_MemoryManager& xVulkanMemory = g_xEngine.FluxMemory();
	xVulkanMemory.DestroyReadWriteBuffer(m_xParticleBufferA);
	xVulkanMemory.DestroyReadWriteBuffer(m_xParticleBufferB);
	xVulkanMemory.DestroyReadWriteBuffer(m_xInstanceBuffer);
	xVulkanMemory.DestroyIndirectBuffer(m_xIndirectArgsBuffer);

	delete[] m_pxStagingBuffer;
	m_pxStagingBuffer = nullptr;
	m_uStagingBufferSize = 0;

	m_axEmitters.Clear();
	m_uTotalAllocatedParticles = 0;
	m_bActiveThisFrame = false;

	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_ParticleGPU shut down");
}

void Flux_ParticleGPUImpl::Reset()
{
	// Scene lifecycle. Deliberately NARROW: it drops the queued spawns and the frame
	// latch, and touches nothing about the rings.
	//
	// Ring state is per-EMITTER, not per-scene, and an emitter's lifetime is its
	// component's — one that dies unregisters (which clears its ring), and a slot
	// handed to a new emitter is blanked by RegisterEmitter. Clearing the rings HERE
	// would instead desynchronise the CPU bookkeeping from VRAM for any emitter that
	// SURVIVES the load (a DontDestroyOnLoad one): its particles would still be in the
	// pool, still integrating and still drawing, while the CPU believed the ring was
	// empty and re-spawned over them from slot 0. The CPU path likewise keeps a
	// persistent emitter's particles across a load, so this keeps the two alike.
	for (uint32_t i = 0; i < m_axEmitters.GetSize(); ++i)
	{
		m_axEmitters.Get(i).m_uPendingSpawnCount = 0;
	}

	m_bActiveThisFrame = false;

	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_ParticleGPUImpl::Reset()");
}

void Flux_ParticleGPUImpl::SeedIndirectCommands()
{
	uint32_t auArgs[uFLUX_PARTICLE_PARTITION_COUNT * uFLUX_PARTICLE_INDIRECT_WORDS];
	for (uint32_t u = 0; u < uFLUX_PARTICLE_PARTITION_COUNT; ++u)
	{
		// instanceCount starts at 0 and IS the compute pass's atomic; firstInstance
		// stays 0 because the partition base is applied by binding the vertex stream
		// at the partition's byte offset (which needs no device feature, unlike a
		// non-zero firstInstance in an indirect command).
		Flux_PackResetIndirectCommand(&auArgs[u * uFLUX_PARTICLE_INDIRECT_WORDS], uFLUX_PARTICLE_QUAD_INDEX_COUNT);
	}

	g_xEngine.FluxMemory().UploadBufferData(
		m_xIndirectArgsBuffer.GetBuffer().m_xVRAMHandle,
		auArgs,
		sizeof(auArgs)
	);
}

void Flux_ParticleGPUImpl::ZeroEmitterRange(const EmitterData& xEmitter)
{
	if (m_pxStagingBuffer == nullptr || xEmitter.m_uReservedParticles == 0)
	{
		return;
	}

	const size_t uBytes = sizeof(Flux_Particle) * xEmitter.m_uReservedParticles;
	const size_t uDest  = sizeof(Flux_Particle) * xEmitter.m_uBaseOffset;
	std::memset(m_pxStagingBuffer, 0, uBytes);

	// BOTH halves of the ping-pong: whichever is the input on the next dispatch has
	// to read dead slots, and the other becomes the input the frame after.
	Flux_MemoryManager& xVulkanMemory = g_xEngine.FluxMemory();
	xVulkanMemory.UploadBufferDataAtOffset(m_xParticleBufferA.GetBuffer().m_xVRAMHandle, m_pxStagingBuffer, uBytes, uDest);
	xVulkanMemory.UploadBufferDataAtOffset(m_xParticleBufferB.GetBuffer().m_xVRAMHandle, m_pxStagingBuffer, uBytes, uDest);
}

uint32_t Flux_ParticleGPUImpl::RegisterEmitter(Flux_ParticleEmitterConfig* pxConfig, uint32_t uMaxParticles)
{
	if (pxConfig == nullptr || uMaxParticles == 0)
	{
		return UINT32_MAX;
	}

	// Prefer a retired reservation that already fits. The pool is carved by base
	// offset and nothing compacts it, so handing the range back to the free pool
	// instead would let a register/unregister cycle exhaust it.
	for (uint32_t i = 0; i < m_axEmitters.GetSize(); ++i)
	{
		EmitterData& xFree = m_axEmitters.Get(i);
		if (xFree.m_pxConfig != nullptr || xFree.m_uReservedParticles < uMaxParticles)
		{
			continue;
		}

		xFree.m_pxConfig              = pxConfig;
		xFree.m_uMaxParticles         = uMaxParticles;
		xFree.m_uCurrentParticleCount = 0;
		xFree.m_uWriteCursor          = 0;
		xFree.m_uPendingSpawnCount    = 0;
		ZeroEmitterRange(xFree);

		Zenith_Log(LOG_CATEGORY_PARTICLES, "Reused GPU emitter slot %u (max %u particles, reserved %u, offset %u)",
			i, uMaxParticles, xFree.m_uReservedParticles, xFree.m_uBaseOffset);
		return i;
	}

	if (m_uTotalAllocatedParticles + uMaxParticles > s_uMaxGPUParticles)
	{
		Zenith_Log(LOG_CATEGORY_PARTICLES, "ERROR: Cannot register GPU emitter - would exceed max particles (%u + %u > %u)",
			m_uTotalAllocatedParticles, uMaxParticles, s_uMaxGPUParticles);
		return UINT32_MAX;
	}

	EmitterData xEmitter;
	xEmitter.m_pxConfig           = pxConfig;
	xEmitter.m_uReservedParticles = uMaxParticles;
	xEmitter.m_uMaxParticles      = uMaxParticles;
	xEmitter.m_uBaseOffset        = m_uTotalAllocatedParticles;

	m_uTotalAllocatedParticles += uMaxParticles;

	const uint32_t uID = m_axEmitters.GetSize();
	m_axEmitters.PushBack(xEmitter);
	ZeroEmitterRange(m_axEmitters.Get(uID));

	Zenith_Log(LOG_CATEGORY_PARTICLES, "Registered GPU emitter %u (max %u particles, offset %u)",
		uID, uMaxParticles, xEmitter.m_uBaseOffset);

	return uID;
}

void Flux_ParticleGPUImpl::UnregisterEmitter(uint32_t uEmitterID)
{
	if (uEmitterID >= m_axEmitters.GetSize())
	{
		return;
	}

	// The reservation (base offset + reserved count) is deliberately KEPT so the
	// slot can be handed to the next emitter that fits; only the live state clears.
	EmitterData& xEmitter = m_axEmitters.Get(uEmitterID);
	xEmitter.m_pxConfig              = nullptr;
	xEmitter.m_uMaxParticles         = 0;
	xEmitter.m_uCurrentParticleCount = 0;
	xEmitter.m_uWriteCursor          = 0;
	xEmitter.m_uPendingSpawnCount    = 0;

	Zenith_Log(LOG_CATEGORY_PARTICLES, "Unregistered GPU emitter %u (reservation %u slots at %u kept)",
		uEmitterID, xEmitter.m_uReservedParticles, xEmitter.m_uBaseOffset);
}

void Flux_ParticleGPUImpl::QueueSpawn(uint32_t uEmitterID, uint32_t uCount,
	const Zenith_Maths::Vector3& xPosition,
	const Zenith_Maths::Vector3& xDirection)
{
	if (uEmitterID >= m_axEmitters.GetSize())
	{
		return;
	}

	EmitterData& xEmitter = m_axEmitters.Get(uEmitterID);
	if (xEmitter.m_pxConfig == nullptr)
	{
		return;
	}

	xEmitter.m_uPendingSpawnCount += uCount;
	xEmitter.m_xSpawnPosition = xPosition;
	xEmitter.m_xSpawnDirection = xDirection;
}

void Flux_ParticleGPUImpl::ProcessPendingSpawns()
{
	if (m_pxStagingBuffer == nullptr)
	{
		return;
	}

	// Spawns land in the buffer THIS frame's dispatch reads: the ping-pong flips at
	// the end of DispatchCompute, so the "input" here is the same one the dispatch
	// is about to integrate.
	Flux_ReadWriteBuffer& xInputBuffer = m_bUseBufferA ? m_xParticleBufferA : m_xParticleBufferB;

	for (uint32_t uEmitterIdx = 0; uEmitterIdx < m_axEmitters.GetSize(); ++uEmitterIdx)
	{
		EmitterData& xEmitter = m_axEmitters.Get(uEmitterIdx);

		if (xEmitter.m_pxConfig == nullptr || xEmitter.m_uPendingSpawnCount == 0)
		{
			continue;
		}

		const Flux_ParticleSpawnRuns xRuns = Flux_SplitParticleSpawnRing(
			xEmitter.m_uWriteCursor, xEmitter.m_uPendingSpawnCount, xEmitter.m_uMaxParticles);
		const uint32_t uSpawnCount = xRuns.TotalCount();

		if (uSpawnCount == 0)
		{
			xEmitter.m_uPendingSpawnCount = 0;
			continue;
		}

		Flux_ParticleEmitterConfig* pxConfig = xEmitter.m_pxConfig;

		for (uint32_t i = 0; i < uSpawnCount; ++i)
		{
			Flux_Particle& xP = m_pxStagingBuffer[i];

			Zenith_Maths::Vector3 xSpawnPos = xEmitter.m_xSpawnPosition;
			if (pxConfig->m_fSpawnRadius > 0.0f)
			{
				float fRadius = pxConfig->m_fSpawnRadius;
				xSpawnPos.x += (RandomFloat(*this) * 2.0f - 1.0f) * fRadius;
				xSpawnPos.y += (RandomFloat(*this) * 2.0f - 1.0f) * fRadius;
				xSpawnPos.z += (RandomFloat(*this) * 2.0f - 1.0f) * fRadius;
			}
			xP.SetPosition(xSpawnPos);
			xP.SetAge(0.0f);

			float fLifetime = pxConfig->m_fLifetimeMin +
				RandomFloat(*this) * (pxConfig->m_fLifetimeMax - pxConfig->m_fLifetimeMin);
			xP.SetLifetime(fLifetime);

			Zenith_Maths::Vector3 xRandomDir = GetRandomDirectionInCone(*this,
				xEmitter.m_xSpawnDirection, pxConfig->m_fSpreadAngleDegrees);
			float fSpeed = pxConfig->m_fSpeedMin +
				RandomFloat(*this) * (pxConfig->m_fSpeedMax - pxConfig->m_fSpeedMin);
			xP.SetVelocity(xRandomDir * fSpeed);

			xP.m_xColorStart = pxConfig->m_xColorStart;
			xP.m_xColorEnd = pxConfig->m_xColorEnd;

			xP.SetSizeStart(pxConfig->m_fSizeStart);
			xP.SetSizeEnd(pxConfig->m_fSizeEnd);

			float fRotation = pxConfig->m_fRotationMin +
				RandomFloat(*this) * (pxConfig->m_fRotationMax - pxConfig->m_fRotationMin);
			xP.SetRotation(fRotation);

			float fRotationSpeed = pxConfig->m_fRotationSpeedMin +
				RandomFloat(*this) * (pxConfig->m_fRotationSpeedMax - pxConfig->m_fRotationSpeedMin);
			xP.SetRotationSpeed(fRotationSpeed);

			xP.m_xPadding = Zenith_Maths::Vector4(0.0f);
		}

		// Up to two contiguous runs — the ring can wrap mid-burst.
		Flux_MemoryManager& xVulkanMemory = g_xEngine.FluxMemory();
		xVulkanMemory.UploadBufferDataAtOffset(
			xInputBuffer.GetBuffer().m_xVRAMHandle,
			m_pxStagingBuffer,
			xRuns.m_uFirstCount * sizeof(Flux_Particle),
			(xEmitter.m_uBaseOffset + xRuns.m_uFirstStart) * sizeof(Flux_Particle)
		);
		if (xRuns.m_uSecondCount > 0)
		{
			xVulkanMemory.UploadBufferDataAtOffset(
				xInputBuffer.GetBuffer().m_xVRAMHandle,
				m_pxStagingBuffer + xRuns.m_uFirstCount,
				xRuns.m_uSecondCount * sizeof(Flux_Particle),
				xEmitter.m_uBaseOffset * sizeof(Flux_Particle)
			);
		}

		xEmitter.m_uWriteCursor = xRuns.m_uNextCursor;
		xEmitter.m_uCurrentParticleCount = Flux_AdvanceParticleRingOccupancy(
			xEmitter.m_uCurrentParticleCount, uSpawnCount, xEmitter.m_uMaxParticles);
		xEmitter.m_uPendingSpawnCount = 0;
	}
}

void Flux_ParticleGPUImpl::PreExecuteCompute()
{
	// ONE latch for the whole frame, read by the dispatch AND by the draw. Deciding
	// twice would let a mid-frame option flip record draws against args this never
	// seeded.
	m_bActiveThisFrame = Zenith_GraphicsOptions::Get().m_bGPUParticlesEnabled && HasGPUEmitters();
	if (!m_bActiveThisFrame)
	{
		return;
	}

	ProcessPendingSpawns();
	SeedIndirectCommands();
}

void Flux_ParticleGPUImpl::DispatchCompute(Flux_CommandBuffer* pxCmdList)
{
	if (!m_bActiveThisFrame)
	{
		return;
	}

	const float fDt = g_xEngine.Frame().GetDt();

	Flux_ReadWriteBuffer& xInputBuffer  = m_bUseBufferA ? m_xParticleBufferA : m_xParticleBufferB;
	Flux_ReadWriteBuffer& xOutputBuffer = m_bUseBufferA ? m_xParticleBufferB : m_xParticleBufferA;

	pxCmdList->BindComputePipeline(&m_xComputePipeline);

	namespace PU = Flux_Generated_Particles::ParticleUpdate;
	Flux_ShaderBinder xBinder(*pxCmdList);
	// The four buffers are frame-uniform; only the per-emitter constants change, so
	// they are staged once and every dispatch below inherits them.
	xBinder.BindUAV_Buffer(PU::hInputParticles,  &xInputBuffer.GetUAV());
	xBinder.BindUAV_Buffer(PU::hOutputParticles, &xOutputBuffer.GetUAV());
	xBinder.BindUAV_Buffer(PU::hInstanceBuffer,  &m_xInstanceBuffer.GetUAV());
	xBinder.BindUAV_Buffer(PU::hIndirectArgs,    &m_xIndirectArgsBuffer.GetUAV());

	// ONE DISPATCH PER EMITTER. Emitters own disjoint pool slices and claim instance
	// slots through a device-scope atomic, so the dispatches need no barrier between
	// them — they never write the same word, and the counter is atomic by construction.
	for (uint32_t i = 0; i < m_axEmitters.GetSize(); ++i)
	{
		const EmitterData& xEmitter = m_axEmitters.Get(i);
		if (xEmitter.m_pxConfig == nullptr || xEmitter.m_uMaxParticles == 0)
		{
			continue;
		}

		const Flux_ParticleEmitterConfig& xConfig = *xEmitter.m_pxConfig;
		const uint32_t uPartition = xConfig.m_bAdditiveBlending
			? uFLUX_PARTICLE_PARTITION_ADDITIVE
			: uFLUX_PARTICLE_PARTITION_ALPHA;

		ParticleComputeConstants xConstants{};
		xConstants.m_fDeltaTime       = fDt;
		xConstants.m_uBaseOffset      = xEmitter.m_uBaseOffset;
		xConstants.m_uParticleCount   = xEmitter.m_uMaxParticles;
		xConstants.m_fTurbulence      = xConfig.m_fTurbulence;
		xConstants.m_xGravity         = Zenith_Maths::Vector4(xConfig.m_xGravity, xConfig.m_fDrag);
		xConstants.m_uInstanceBase    = uPartition * s_uPartitionCapacity;
		xConstants.m_uInstanceCapacity = s_uPartitionCapacity;
		xConstants.m_uArgsWordBase    = uPartition * uFLUX_PARTICLE_INDIRECT_WORDS;

		xBinder.BindDrawConstants(PU::hPushConstants, &xConstants, sizeof(xConstants));

		const uint32_t uWorkgroups = (xEmitter.m_uMaxParticles + s_uWorkgroupSize - 1) / s_uWorkgroupSize;
		pxCmdList->Dispatch(uWorkgroups, 1, 1);
	}

	m_bUseBufferA = !m_bUseBufferA;
}

bool Flux_ParticleGPUImpl::HasGPUEmitters() const
{
	for (uint32_t i = 0; i < m_axEmitters.GetSize(); ++i)
	{
		if (m_axEmitters.Get(i).m_pxConfig != nullptr)
		{
			return true;
		}
	}
	return false;
}

uint32_t Flux_ParticleGPUImpl::GetEmitterParticleCount(uint32_t uEmitterID) const
{
	if (uEmitterID >= m_axEmitters.GetSize())
	{
		return 0;
	}
	return m_axEmitters.Get(uEmitterID).m_uCurrentParticleCount;
}

uint32_t Flux_ParticleGPUImpl::ReadbackPartitionInstanceCount(u_int uPartition)
{
	if (uPartition >= uFLUX_PARTICLE_PARTITION_COUNT
		|| !m_xIndirectArgsBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		return 0u;
	}

	// Explicit slow path — see the header. Reads the WHOLE block in one download
	// (it is 40 bytes) and picks the partition's instanceCount out of it.
	uint32_t auArgs[uFLUX_PARTICLE_PARTITION_COUNT * uFLUX_PARTICLE_INDIRECT_WORDS] = {};
	g_xEngine.FluxMemory().DownloadBufferData(
		m_xIndirectArgsBuffer.GetBuffer().m_xVRAMHandle, auArgs, sizeof(auArgs));

	return auArgs[uPartition * uFLUX_PARTICLE_INDIRECT_WORDS + 1u];   // word 1 = instanceCount
}

// GPU-driven pool addressing + the cross-language pins. Hosted beside the code it
// covers; this TU is always linked (Particles is a registered render feature), so the
// ZENITH_TEST static registrations cannot be dead-stripped.
#include "Flux/Particles/Flux_ParticleGPU.Tests.inl"
