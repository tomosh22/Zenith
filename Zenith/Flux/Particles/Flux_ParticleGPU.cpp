#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Particles/Flux_ParticleGPUImpl.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"

#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"

namespace
{
	float RandomFloat()
	{
		Flux_ParticleGPUImpl& xImpl = g_xEngine.ParticleGPU();
		return xImpl.m_xDist(xImpl.m_xRng);
	}

	Zenith_Maths::Vector3 GetRandomDirectionInCone(const Zenith_Maths::Vector3& xDir, float fSpreadAngleDegrees)
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

	struct ParticleComputeConstants
	{
		float m_fDeltaTime;
		uint32_t m_uParticleCount;
		float m_fTurbulence;
		float m_fPad1;
		Zenith_Maths::Vector4 m_xGravity;  // xyz=gravity, w=drag
	};
}

void Flux_ParticleGPUImpl::BuildPipelines()
{
	m_xComputeShader.Initialise(FluxShaderProgram::ParticleUpdate);

	const Flux_ShaderReflection& xReflection = m_xComputeShader.GetReflection();
	Flux_RootSigBuilder::FromReflection(m_xComputeRootSig, xReflection);

	Flux_ComputePipelineBuilder::BuildFromShader(m_xComputePipeline, m_xComputeShader, m_xComputeRootSig);
}

void Flux_ParticleGPUImpl::Initialise()
{
	BuildPipelines();

	g_xEngine.VulkanMemory().InitialiseReadWriteBuffer(
		nullptr,
		sizeof(Flux_Particle) * s_uMaxGPUParticles,
		m_xParticleBufferA
	);
	g_xEngine.VulkanMemory().InitialiseReadWriteBuffer(
		nullptr,
		sizeof(Flux_Particle) * s_uMaxGPUParticles,
		m_xParticleBufferB
	);

	g_xEngine.VulkanMemory().InitialiseReadWriteBuffer(
		nullptr,
		sizeof(Flux_ParticleInstance) * s_uMaxGPUParticles,
		m_xInstanceBuffer
	);

	g_xEngine.VulkanMemory().InitialiseIndirectBuffer(
		sizeof(uint32_t),
		m_xCounterBuffer
	);

	m_pxStagingBuffer = new Flux_Particle[s_uMaxGPUParticles];
	m_uStagingBufferSize = s_uMaxGPUParticles;

	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_ParticleGPU initialised (max %u particles)", s_uMaxGPUParticles);
}

void Flux_ParticleGPUImpl::Shutdown()
{
	m_xComputePipeline.Reset();
	m_xComputeShader.Reset();
	m_xComputeRootSig = Flux_RootSig();
	m_xComputeCommandList.Reset();

	g_xEngine.VulkanMemory().DestroyReadWriteBuffer(m_xParticleBufferA);
	g_xEngine.VulkanMemory().DestroyReadWriteBuffer(m_xParticleBufferB);
	g_xEngine.VulkanMemory().DestroyReadWriteBuffer(m_xInstanceBuffer);
	g_xEngine.VulkanMemory().DestroyIndirectBuffer(m_xCounterBuffer);

	delete[] m_pxStagingBuffer;
	m_pxStagingBuffer = nullptr;
	m_uStagingBufferSize = 0;

	m_axEmitters.Clear();
	m_uNextEmitterID = 0;
	m_uTotalAllocatedParticles = 0;

	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_ParticleGPU shut down");
}

void Flux_ParticleGPUImpl::Reset()
{
	for (uint32_t i = 0; i < m_axEmitters.GetSize(); ++i)
	{
		m_axEmitters.Get(i).m_uPendingSpawnCount = 0;
	}

	m_uAliveCount = 0;
	m_xComputeCommandList.Reset();

	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_ParticleGPUImpl::Reset()");
}

uint32_t Flux_ParticleGPUImpl::RegisterEmitter(Flux_ParticleEmitterConfig* pxConfig, uint32_t uMaxParticles)
{
	if (m_uTotalAllocatedParticles + uMaxParticles > s_uMaxGPUParticles)
	{
		Zenith_Log(LOG_CATEGORY_PARTICLES, "ERROR: Cannot register GPU emitter - would exceed max particles (%u + %u > %u)",
			m_uTotalAllocatedParticles, uMaxParticles, s_uMaxGPUParticles);
		return UINT32_MAX;
	}

	EmitterData xEmitter;
	xEmitter.m_pxConfig = pxConfig;
	xEmitter.m_uMaxParticles = uMaxParticles;
	xEmitter.m_uBaseOffset = m_uTotalAllocatedParticles;
	xEmitter.m_uPendingSpawnCount = 0;

	m_uTotalAllocatedParticles += uMaxParticles;

	uint32_t uID = m_uNextEmitterID++;
	m_axEmitters.PushBack(xEmitter);

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

	m_axEmitters.Get(uEmitterID).m_pxConfig = nullptr;
	m_axEmitters.Get(uEmitterID).m_uMaxParticles = 0;

	Zenith_Log(LOG_CATEGORY_PARTICLES, "Unregistered GPU emitter %u", uEmitterID);
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

	Flux_ReadWriteBuffer& xInputBuffer = m_bUseBufferA ? m_xParticleBufferA : m_xParticleBufferB;

	for (uint32_t uEmitterIdx = 0; uEmitterIdx < m_axEmitters.GetSize(); ++uEmitterIdx)
	{
		EmitterData& xEmitter = m_axEmitters.Get(uEmitterIdx);

		if (xEmitter.m_pxConfig == nullptr || xEmitter.m_uPendingSpawnCount == 0)
		{
			continue;
		}

		Flux_ParticleEmitterConfig* pxConfig = xEmitter.m_pxConfig;

		uint32_t uAvailableSlots = xEmitter.m_uMaxParticles - xEmitter.m_uCurrentParticleCount;
		uint32_t uSpawnCount = (xEmitter.m_uPendingSpawnCount < uAvailableSlots)
			? xEmitter.m_uPendingSpawnCount : uAvailableSlots;

		if (uSpawnCount == 0)
		{
			xEmitter.m_uPendingSpawnCount = 0;
			continue;
		}

		for (uint32_t i = 0; i < uSpawnCount; ++i)
		{
			Flux_Particle& xP = m_pxStagingBuffer[i];

			Zenith_Maths::Vector3 xSpawnPos = xEmitter.m_xSpawnPosition;
			if (pxConfig->m_fSpawnRadius > 0.0f)
			{
				float fRadius = pxConfig->m_fSpawnRadius;
				xSpawnPos.x += (RandomFloat() * 2.0f - 1.0f) * fRadius;
				xSpawnPos.y += (RandomFloat() * 2.0f - 1.0f) * fRadius;
				xSpawnPos.z += (RandomFloat() * 2.0f - 1.0f) * fRadius;
			}
			xP.SetPosition(xSpawnPos);
			xP.SetAge(0.0f);

			float fLifetime = pxConfig->m_fLifetimeMin +
				RandomFloat() * (pxConfig->m_fLifetimeMax - pxConfig->m_fLifetimeMin);
			xP.SetLifetime(fLifetime);

			Zenith_Maths::Vector3 xRandomDir = GetRandomDirectionInCone(
				xEmitter.m_xSpawnDirection, pxConfig->m_fSpreadAngleDegrees);
			float fSpeed = pxConfig->m_fSpeedMin +
				RandomFloat() * (pxConfig->m_fSpeedMax - pxConfig->m_fSpeedMin);
			xP.SetVelocity(xRandomDir * fSpeed);

			xP.m_xColorStart = pxConfig->m_xColorStart;
			xP.m_xColorEnd = pxConfig->m_xColorEnd;

			xP.SetSizeStart(pxConfig->m_fSizeStart);
			xP.SetSizeEnd(pxConfig->m_fSizeEnd);

			float fRotation = pxConfig->m_fRotationMin +
				RandomFloat() * (pxConfig->m_fRotationMax - pxConfig->m_fRotationMin);
			xP.SetRotation(fRotation);

			float fRotationSpeed = pxConfig->m_fRotationSpeedMin +
				RandomFloat() * (pxConfig->m_fRotationSpeedMax - pxConfig->m_fRotationSpeedMin);
			xP.SetRotationSpeed(fRotationSpeed);

			xP.m_xPadding = Zenith_Maths::Vector4(0.0f);
		}

		uint32_t uUploadOffset = xEmitter.m_uBaseOffset + xEmitter.m_uCurrentParticleCount;
		g_xEngine.VulkanMemory().UploadBufferDataAtOffset(
			xInputBuffer.GetBuffer().m_xVRAMHandle,
			m_pxStagingBuffer,
			uSpawnCount * sizeof(Flux_Particle),
			uUploadOffset * sizeof(Flux_Particle)
		);

		xEmitter.m_uCurrentParticleCount += uSpawnCount;
		xEmitter.m_uPendingSpawnCount = 0;

		Zenith_Log(LOG_CATEGORY_PARTICLES, "GPU: Spawned %u particles for emitter %u (total: %u)",
			uSpawnCount, uEmitterIdx, xEmitter.m_uCurrentParticleCount);
	}
}

void Flux_ParticleGPUImpl::PreExecuteCompute()
{
	if (!Zenith_GraphicsOptions::Get().m_bGPUParticlesEnabled || m_axEmitters.GetSize() == 0)
		return;

	ProcessPendingSpawns();

	uint32_t uZero = 0;
	g_xEngine.VulkanMemory().UploadBufferData(
		m_xCounterBuffer.GetBuffer().m_xVRAMHandle,
		&uZero,
		sizeof(uint32_t)
	);
}

void Flux_ParticleGPUImpl::DispatchCompute(Flux_CommandList* pxCmdList)
{
	if (!Zenith_GraphicsOptions::Get().m_bGPUParticlesEnabled || m_axEmitters.GetSize() == 0)
		return;

	float fDt = g_xEngine.Frame().GetDt();

	Flux_ReadWriteBuffer& xInputBuffer  = m_bUseBufferA ? m_xParticleBufferA : m_xParticleBufferB;
	Flux_ReadWriteBuffer& xOutputBuffer = m_bUseBufferA ? m_xParticleBufferB : m_xParticleBufferA;

	pxCmdList->AddCommand<Flux_CommandBindComputePipeline>(&m_xComputePipeline);

	ParticleComputeConstants xConstants;
	xConstants.m_fDeltaTime = fDt;
	xConstants.m_uParticleCount = m_uTotalAllocatedParticles;
	xConstants.m_fTurbulence = 0.0f;
	xConstants.m_xGravity = Zenith_Maths::Vector4(0.0f, -9.8f, 0.0f, 0.0f);

	for (uint32_t i = 0; i < m_axEmitters.GetSize(); ++i)
	{
		if (m_axEmitters.Get(i).m_pxConfig != nullptr)
		{
			xConstants.m_fTurbulence = m_axEmitters.Get(i).m_pxConfig->m_fTurbulence;
			break;
		}
	}

	Flux_ShaderBinder xBinder(*pxCmdList);
	xBinder.BindUAV_Buffer(m_xComputeShader, "InputParticles",  &xInputBuffer.GetUAV());
	xBinder.BindUAV_Buffer(m_xComputeShader, "OutputParticles", &xOutputBuffer.GetUAV());
	xBinder.BindUAV_Buffer(m_xComputeShader, "InstanceBuffer",  &m_xInstanceBuffer.GetUAV());
	xBinder.BindUAV_Buffer(m_xComputeShader, "aliveCount",      &m_xCounterBuffer.GetUAV());
	xBinder.BindDrawConstants(m_xComputeShader, "PushConstants", &xConstants, sizeof(xConstants));

	uint32_t uWorkgroups = (m_uTotalAllocatedParticles + s_uWorkgroupSize - 1) / s_uWorkgroupSize;
	pxCmdList->AddCommand<Flux_CommandDispatch>(uWorkgroups, 1, 1);

	m_bUseBufferA = !m_bUseBufferA;

	for (uint32_t i = 0; i < m_axEmitters.GetSize(); ++i)
	{
		m_axEmitters.Get(i).m_uPendingSpawnCount = 0;
	}

	m_uAliveCount = m_uTotalAllocatedParticles;
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
