#include "Zenith.h"
#include "Flux/Particles/Flux_Particles_Shaders.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Shaders/Generated/Particles.h" // typed binding handles
#include "Profiling/Zenith_Profiling.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Particles/Flux_ParticlesImpl.h"
#include "Flux/Particles/Flux_ParticleData.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"
#include "Flux/Particles/Flux_ParticleGPUImpl.h"

#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "AssetHandling/Zenith_TextureAsset.h"
// Wave 3: emitters are ticked + gathered EC-side; the renderer consumes neutral
// Zenith_ParticleEmitterRenderData (+ Zenith_ParticleData) via g_pfnZenithParticleGather.
#include "Core/Zenith_RenderGather.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"




// Maximum particles across all emitters
static constexpr uint32_t s_uMaxParticles = 4096;

// Per-pass draw constants — must match ParticleConstantsLayout in Flux_Particles.slang.
// Carries the billboard texture's bindless slot (g_axTextures[] index).
struct Flux_ParticleDrawConstants
{
	u_int m_uTexIdx;
	u_int m_auPad[3];
};
static_assert(sizeof(Flux_ParticleDrawConstants) == 16, "Flux_ParticleDrawConstants must match ParticleConstantsLayout (16 bytes)");

// CPU-side instance buffers for staging (partitioned by blend mode)


// Pinned via TextureHandle so UnloadUnused never frees the particle atlas mid-frame.

static void ExecuteParticles(Flux_CommandBuffer* pxCommandList, void* pUserData);

void Flux_ParticlesImpl::BuildPipelines()
{
	m_xShader.Initialise(Flux_ParticlesShaders::xParticles);

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_aeColourAttachmentFormats[0] = HDR_SCENE_FORMAT;
	xPipelineSpec.m_uNumColourAttachments = 1;
	xPipelineSpec.m_pxShader = &m_xShader;
	// Binding 0 = the shared unit quad (20 B), binding 1 = Flux_ParticleInstance (20 B).
	// The split comes from the [PerInstance] tags in Flux_Particles.slang; the offsets
	// and strides are pinned against the struct in Flux_ParticleData.h.
	xPipelineSpec.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xPipelineSpec.m_pxVertexLayout = &Flux_Generated_Particles::Particles::kVertexLayout;

	m_xShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

	xPipelineSpec.m_bDepthWriteEnabled = false;

	// Alpha blending pipeline (SrcAlpha / OneMinusSrcAlpha)
	xPipelineSpec.m_axBlendStates[0].m_bBlendEnabled = true;
	xPipelineSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_SRCALPHA;
	xPipelineSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONEMINUSSRCALPHA;

	Flux_PipelineBuilder::FromSpecification(m_xPipelineAlpha, xPipelineSpec);

	// Additive blending pipeline (SrcAlpha / One)
	xPipelineSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONE;

	Flux_PipelineBuilder::FromSpecification(m_xPipelineAdditive, xPipelineSpec);

	// Rebuild the GPU compute pipeline alongside the rasterisation ones so a
	// shader edit to either Particles.slang or ParticleUpdate.slang triggers a
	// single coordinated rebuild.
	g_xEngine.ParticleGPU().BuildPipelines();
}

void Flux_ParticlesImpl::Initialise()
{
	BuildPipelines();

	// The GPU compute path's VRAM. Its pipeline was already built by the
	// BuildPipelines above (one feature owns both programs, so one rebuild covers
	// both on hot-reload) — Initialise here allocates resources only.
	g_xEngine.ParticleGPU().Initialise();

	// Allocate instance buffers for both blend modes
	auto& xVulkanMemory = g_xEngine.FluxMemory();
	xVulkanMemory.InitialiseDynamicVertexBuffer(nullptr, s_uMaxParticles * sizeof(Flux_ParticleInstance), m_xInstanceBufferAlpha, false);
	xVulkanMemory.InitialiseDynamicVertexBuffer(nullptr, s_uMaxParticles * sizeof(Flux_ParticleInstance), m_xInstanceBufferAdditive, false);

	// Load default particle texture (pinned)
	if (Zenith_TextureAsset* pxParticle = Zenith_AssetRegistry::GetView<Zenith_TextureAsset>(ENGINE_ASSETS_DIR"Textures/Particles/particleSwirl" ZENITH_TEXTURE_EXT))
	{
		m_xParticleTexture.Set(pxParticle);
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_PARTICLES, "Warning: Failed to load particle texture, using white texture");
		m_xParticleTexture = g_xEngine.FluxGraphics().m_xWhiteTexture;
	}

	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_Particles initialised (max %u particles)", s_uMaxParticles);
}

void Flux_ParticlesImpl::Reset()
{
	m_uAlphaInstanceCount = 0;
	m_uAdditiveInstanceCount = 0;
	g_xEngine.ParticleGPU().Reset();
	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_ParticlesImpl::Reset()");
}

void Flux_ParticlesImpl::ReleaseAssetReferences()
{
	m_xParticleTexture.Clear();
}

void Flux_ParticlesImpl::Shutdown()
{
	g_xEngine.ParticleGPU().Shutdown();
	auto& xVulkanMemory = g_xEngine.FluxMemory();
	xVulkanMemory.DestroyDynamicVertexBuffer(m_xInstanceBufferAlpha);
	xVulkanMemory.DestroyDynamicVertexBuffer(m_xInstanceBufferAdditive);
	Zenith_Log(LOG_CATEGORY_PARTICLES, "Flux_Particles shut down");
}

void Flux_ParticlesImpl::UpdateEmittersAndBuildInstanceBuffer(float fDt)
{
	ZENITH_PROFILE_SCOPE("Particles CPU Sim & Build");
	m_uAlphaInstanceCount = 0;
	m_uAdditiveInstanceCount = 0;

	// Wave 3: emitters are ticked + queried EC-side (g_pfnZenithParticleGather drives
	// xEmitter.Update(fDt) for every emitter and returns one neutral entry per CPU
	// emitter). This body just builds the GPU instance buffers from that data, so it
	// names no EntityComponent type and reaches no g_xEngine.Scenes().
	//
	// The tick covers GPU emitters too — it is what turns their spawn rate into the
	// QueueSpawn calls PreExecuteCompute drains a few lines later in Render — so it
	// runs whenever EITHER path is enabled, and only the instance BUILD below is
	// gated on the CPU option.
	Zenith_Vector<Zenith_ParticleEmitterRenderData> xEmitters;
	if (g_pfnZenithParticleGather) g_pfnZenithParticleGather(fDt, xEmitters);

	if (!Zenith_GraphicsOptions::Get().m_bCPUParticlesEnabled)
	{
		return;
	}

	for (u_int e = 0; e < xEmitters.GetSize(); ++e)
	{
		const Zenith_ParticleEmitterRenderData& xEmitterData = xEmitters.Get(e);

		// Route to the alpha or additive instance buffer based on the emitter's blend mode.
		Flux_ParticleInstance* pxTargetBuffer = xEmitterData.m_bAdditive ? m_axAdditiveInstances : m_axAlphaInstances;
		uint32_t& uTargetCount = xEmitterData.m_bAdditive ? m_uAdditiveInstanceCount : m_uAlphaInstanceCount;

		for (uint32_t i = 0; i < xEmitterData.m_uAliveCount && uTargetCount < s_uMaxParticles; ++i)
		{
			// Zenith_ParticleData (neutral mirror) exposes the same position/size/colour
			// accessors as Flux_Particle, so the renderer builds the instance directly.
			const Zenith_ParticleData& xP = xEmitterData.m_pxParticles[i];
			pxTargetBuffer[uTargetCount] = Flux_ParticleInstance(xP.GetPosition(), xP.GetCurrentSize(), xP.GetCurrentColor());
			uTargetCount++;
		}
	}
}

void Flux_ParticlesImpl::UploadInstanceData()
{
	ZENITH_PROFILE_SCOPE("Particles GPU Upload");
	// Promoted from a file-static free function to an instance member: buffer/count
	// self-references now resolve through 'this'. VulkanMemory is reached via
	// g_xEngine at point of use.
	auto& xVulkanMemory = g_xEngine.FluxMemory();
	if (m_uAlphaInstanceCount > 0)
	{
		xVulkanMemory.UploadBufferData(
			m_xInstanceBufferAlpha.GetBuffer().m_xVRAMHandle,
			m_axAlphaInstances,
			m_uAlphaInstanceCount * sizeof(Flux_ParticleInstance)
		);
	}
	if (m_uAdditiveInstanceCount > 0)
	{
		xVulkanMemory.UploadBufferData(
			m_xInstanceBufferAdditive.GetBuffer().m_xVRAMHandle,
			m_axAdditiveInstances,
			m_uAdditiveInstanceCount * sizeof(Flux_ParticleInstance)
		);
	}
}

void Flux_ParticlesImpl::Render(void*)
{
	const Zenith_GraphicsOptions& xOptions = Zenith_GraphicsOptions::Get();
	if (!xOptions.m_bCPUParticlesEnabled && !xOptions.m_bGPUParticlesEnabled)
	{
		return;
	}

	// Update all emitters (both CPU and GPU) and build the CPU instance buffers.
	float fDt = g_xEngine.Frame().GetDt();
	UpdateEmittersAndBuildInstanceBuffer(fDt);

	// Upload CPU instance data to GPU
	UploadInstanceData();

	// ...then the GPU path's CPU half, in that order and in this one Prepare: the
	// tick above is what produced this frame's QueueSpawn calls, so draining them
	// here spawns on the SAME frame. (Hanging this off the compute pass's own
	// Prepare would work too — every Prepare runs before any record — but splitting
	// it across two callbacks is how it silently acquired a frame of latency before.)
	g_xEngine.ParticleGPU().PreExecuteCompute();
}

static void ExecuteParticles(Flux_CommandBuffer* pxCommandList, void* pUserData)
{
	(void)pUserData;
	// Per-view parity: scene particles must never appear in the preview view (its
	// flags carry no FLUX_VIEW_FLAG_SCENE_CONTENT) — the "Particles (Preview)"
	// instance exists for structural parity and records nothing.
	if (Flux_RenderGraph::GetCurrentRecordingPassViewSlot() != kuFluxViewSlotMain)
	{
		return;
	}
	// Emitter sim + instance upload runs once per frame in Render (registered as
	// this pass's Prepare, main thread). This record callback only emits draw
	// commands from the already-populated instance counts/buffers — no ECS
	// mutation (xEmitter.Update) on the worker thread.

	// Non-capturing graph callback (void(*)(Flux_CommandBuffer*, void*)) — it cannot
	// capture, so it re-enters via g_xEngine.Particles() to reach the singleton
	// instance; other cross-subsystem deps are reached via g_xEngine at point of
	// use (mirrors ExecuteSSAOGenerate / ExecuteQuads).
	Flux_ParticlesImpl& xParticles = g_xEngine.Particles();
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	Flux_ParticleGPUImpl& xParticleGPU = g_xEngine.ParticleGPU();

	// The two paths are independent — a scene can run either, both or neither — so
	// each is gated on its OWN option. The GPU half reads the frame latch rather
	// than the option so it can never draw from indirect args PreExecuteCompute
	// declined to seed.
	const bool bDrawCPU = Zenith_GraphicsOptions::Get().m_bCPUParticlesEnabled
		&& (xParticles.m_uAlphaInstanceCount > 0 || xParticles.m_uAdditiveInstanceCount > 0);
	const bool bDrawGPU = xParticleGPU.IsActiveThisFrame();

	if (!bDrawCPU && !bDrawGPU)
	{
		return;
	}

	// Billboard texture goes through the bindless table (set 2 g_axTextures).
	// Mark it bindless once (idempotent guard); radial sprite → CLAMP. Shared by
	// both paths — they draw the same sprite with the same pipelines.
	Zenith_TextureAsset* pxParticleTex = xParticles.m_xParticleTexture.GetDirect();
	if (pxParticleTex->m_xSRV.m_uBindlessIndex == uFLUX_INVALID_BINDLESS_INDEX)
	{
		pxParticleTex->MarkAsBindless(/*bRepeatAddressing*/ false);
	}
	Flux_ParticleDrawConstants xConstants{};
	xConstants.m_uTexIdx = pxParticleTex->m_xSRV.m_uBindlessIndex;

	namespace PT = Flux_Generated_Particles::Particles;

	if (bDrawCPU)
	{
		// Alpha-blended particles
		if (xParticles.m_uAlphaInstanceCount > 0)
		{
			pxCommandList->SetPipeline(&xParticles.m_xPipelineAlpha);

			pxCommandList->SetVertexBuffer(xGraphics.m_xQuadMesh.GetVertexBuffer(), 0);
			pxCommandList->SetIndexBuffer(xGraphics.m_xQuadMesh.GetIndexBuffer());
			pxCommandList->SetVertexBuffer(xParticles.m_xInstanceBufferAlpha, 1);

			Flux_ShaderBinder xBinder(*pxCommandList);
			xBinder.BindDrawConstants(PT::hParticleConstants, &xConstants, static_cast<u_int>(sizeof(xConstants)));
			pxCommandList->UseBindlessTextures(2);

			pxCommandList->DrawIndexed(uFLUX_PARTICLE_QUAD_INDEX_COUNT, xParticles.m_uAlphaInstanceCount);
		}

		// Additive particles
		if (xParticles.m_uAdditiveInstanceCount > 0)
		{
			pxCommandList->SetPipeline(&xParticles.m_xPipelineAdditive);

			pxCommandList->SetVertexBuffer(xGraphics.m_xQuadMesh.GetVertexBuffer(), 0);
			pxCommandList->SetIndexBuffer(xGraphics.m_xQuadMesh.GetIndexBuffer());
			pxCommandList->SetVertexBuffer(xParticles.m_xInstanceBufferAdditive, 1);

			Flux_ShaderBinder xBinder(*pxCommandList);
			xBinder.BindDrawConstants(PT::hParticleConstants, &xConstants, static_cast<u_int>(sizeof(xConstants)));
			pxCommandList->UseBindlessTextures(2);

			pxCommandList->DrawIndexed(uFLUX_PARTICLE_QUAD_INDEX_COUNT, xParticles.m_uAdditiveInstanceCount);
		}
	}

	// GPU-simulated particles. Nothing here knows how many there are: the compute
	// pass compacted the survivors into one blend partition each and counted them
	// into that partition's VkDrawIndexedIndirectCommand, so both draws are recorded
	// unconditionally and an empty partition draws instanceCount 0. The partition
	// BASE is applied by binding the instance stream at the partition's byte offset
	// (not via the command's firstInstance, which would need drawIndirectFirstInstance).
	if (bDrawGPU)
	{
		for (u_int uPartition = 0; uPartition < uFLUX_PARTICLE_PARTITION_COUNT; ++uPartition)
		{
			pxCommandList->SetPipeline(uPartition == uFLUX_PARTICLE_PARTITION_ADDITIVE
				? &xParticles.m_xPipelineAdditive
				: &xParticles.m_xPipelineAlpha);

			pxCommandList->SetVertexBuffer(xGraphics.m_xQuadMesh.GetVertexBuffer(), 0);
			pxCommandList->SetIndexBuffer(xGraphics.m_xQuadMesh.GetIndexBuffer());
			pxCommandList->SetVertexBuffer(xParticleGPU.GetInstanceBuffer(), 1,
				Flux_ParticleGPUImpl::GetPartitionByteOffset(uPartition));

			Flux_ShaderBinder xBinder(*pxCommandList);
			xBinder.BindDrawConstants(PT::hParticleConstants, &xConstants, static_cast<u_int>(sizeof(xConstants)));
			pxCommandList->UseBindlessTextures(2);

			pxCommandList->DrawIndexedIndirect(&xParticleGPU.GetIndirectArgsBuffer(), 1u,
				Flux_ParticleGPUImpl::GetPartitionArgsByteOffset(uPartition),
				uFLUX_PARTICLE_INDIRECT_STRIDE);
		}
	}
}

static void ExecuteParticleCompute(Flux_CommandBuffer* pxCmdList, void*)
{
	// Non-capturing graph callback: ParticleGPU is reached via g_xEngine at
	// point of use (mirrors ExecuteParticles).
	g_xEngine.ParticleGPU().DispatchCompute(pxCmdList);
}

// TODO(taa-translucent-velocity): particles write NO TAA motion vectors, deliberately.
//
// The four findings that apply to every alpha-blended layer are written out once, at
// Flux/Translucency/Flux_Translucency.cpp::SetupRenderGraph — in short: this pass does
// NOT leave velocity at the (0,0) clear (the opaque surface behind already wrote a
// coherent vector, because particles draw after the G-buffer and never write depth);
// one R16G16 slot cannot carry the two motions in `a*sprite + (1-a)*background`; and
// displacing the history fetch by a non-depth-writing layer's motion breaks the
// velocity<->depth coherence the resolve's disocclusion test rests on, so history is
// REJECTED rather than reprojected. The per-attachment blend state such a change would
// need already exists (see that comment) — the plumbing is not the blocker.
//
// TWO THINGS ARE PARTICLE-SPECIFIC AND STRONGER THAN THE SHARED ARGUMENT:
//
//  1. A SPRITE CLOUD HAS NO WELL-DEFINED PER-PIXEL MOTION VECTOR AT ALL. N overlapping
//     semi-transparent billboards plus a background contribute N+1 distinct motions to
//     one colour. This is not a precision problem an alpha threshold can rescue: the
//     engine's sprite (Textures/Particles/particleSwirl) is a soft radial falloff whose
//     above-threshold alpha is a small central core, with the many low-alpha layers
//     around it — the ones actually responsible for the ghosting — permanently below
//     any threshold. Whatever the buffer ends up holding is the last core that happened
//     to pass the depth test, which is an arbitrary choice among the overlaps.
//
//  2. THE DATA IS CHEAP ON THE GPU PATH AND EXPENSIVE TO DELIVER. Flux_ParticleUpdate.slang
//     already integrates from the particle record's velocity (m_xVelocity_Lifetime), so
//     `prevPos = pos - vel*dt` is one line there. Getting it to the DRAW is what costs:
//     Flux_ParticleInstance is a pinned 20 B (position/size + packed colour) and carrying
//     a prev position takes it to 32 B, which moves uFLUX_PARTICLE_INSTANCE_WORDS, the
//     generated Particles vertex layout, uINSTANCE_WORDS in the compute writer, and the
//     six static_asserts in Flux_ParticleData.h that tie those together. The CPU path has
//     no previous position anywhere — Flux_ParticleInstance is all the renderer receives.
//
// SO IF PARTICLE GHOSTING NEEDS FIXING, FIX IT DIRECTLY: mark the pixels particles drew
// and reject history there. That is what writing their velocity would achieve anyway
// (finding 3), at a fraction of the cost and without claiming a motion vector the
// content does not have. Full write-up: Flux/TAA/CLAUDE.md, section "Translucency and
// Particles deliberately write NO velocity".
void Flux_ParticlesImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// GPU particle compute pass. Its CPU half (spawn upload + indirect seeding) is
	// the TAIL of the draw pass's Prepare rather than a Prepare of its own — every
	// Prepare in the graph runs before any record, and keeping the emitter tick and
	// the spawn drain in one callback is what guarantees a spawn reaches the GPU on
	// the frame it was requested.
	Flux_PassHandle xComputePass = xGraph.AddPass("Particles Compute", ExecuteParticleCompute);

	// Render (main-thread Prepare) does the emitter sim + instance-buffer upload
	// before any record callback runs; ExecuteParticles (worker) then only emits
	// draw commands. This moves the xEmitter.Update ECS mutation off the worker
	// thread. Render was previously defined but never registered — this is a new,
	// required edge (without it the CPU particle sim/upload would never run).
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	Flux_PassHandle xDrawPass = xGraph.AddPass("Particles", ExecuteParticles)
		.Prepare([](void* p){ g_xEngine.Particles().Render(p); })
		.Writes(xGraphics.GetHDRSceneTarget(), RESOURCE_ACCESS_WRITE_RTV)
		.DependsOn(xComputePass);

	// The GPU path's buffer traffic, declared so the graph synthesises the
	// compute-write -> vertex-fetch / indirect-read barriers. Before this the pass
	// pair carried only a DependsOn, which orders the passes but emits no memory
	// barrier — correct only while nothing actually read what the compute wrote.
	//
	// BOTH halves of the particle ping-pong get the SAME declaration, which is what
	// makes the per-frame role swap invisible to the graph: the compiled barriers
	// are identical under either parity, so DispatchCompute is free to decide which
	// is input and which is output at record time. (Same trick as the grass
	// displacement ping-pong.)
	Flux_ParticleGPUImpl& xParticleGPU = g_xEngine.ParticleGPU();
	xGraph.WriteBuffer(xComputePass, xParticleGPU.m_xParticleBufferA.GetBuffer(),      RESOURCE_ACCESS_READWRITE_UAV);
	xGraph.WriteBuffer(xComputePass, xParticleGPU.m_xParticleBufferB.GetBuffer(),      RESOURCE_ACCESS_READWRITE_UAV);
	xGraph.WriteBuffer(xComputePass, xParticleGPU.GetInstanceBuffer().GetBuffer(),     RESOURCE_ACCESS_WRITE_UAV);
	xGraph.WriteBuffer(xComputePass, xParticleGPU.GetIndirectArgsBuffer().GetBuffer(), RESOURCE_ACCESS_READWRITE_UAV);

	xGraph.ReadBuffer(xDrawPass, xParticleGPU.GetInstanceBuffer().GetBuffer(),     RESOURCE_ACCESS_READ_VERTEX_BUFFER);
	xGraph.ReadBuffer(xDrawPass, xParticleGPU.GetIndirectArgsBuffer().GetBuffer(), RESOURCE_ACCESS_READ_INDIRECT_ARG);

	// Preview view (S5c): parity instance writing the preview HDR target. Scene
	// particles never render in the preview (no FLUX_VIEW_FLAG_SCENE_CONTENT) —
	// ExecuteParticles early-outs for non-main views. The compute/sim pass is
	// NOT duplicated and no Prepare is attached (the main pass's Prepare owns
	// the once-per-frame emitter sim). No ClearTargets — "Apply Lighting
	// (Preview)" owns the preview HDR clear.
	if (xGraphics.RenderViews().IsViewActive(kuFluxViewSlotPreview))
	{
		xGraph.AddPass("Particles (Preview)", ExecuteParticles)
			.View  (kuFluxViewSlotPreview)
			.Writes(xGraphics.GetHDRSceneTarget(kuFluxViewSlotPreview), RESOURCE_ACCESS_WRITE_RTV);
	}
}

// Packed per-particle instance lane (compressed-vertex Phase 6). Hosted beside the
// code it covers; this TU is always linked (Particles is a registered render
// feature), so the ZENITH_TEST static registrations cannot be dead-stripped.
#include "Flux/Particles/Flux_ParticleInstance.Tests.inl"
