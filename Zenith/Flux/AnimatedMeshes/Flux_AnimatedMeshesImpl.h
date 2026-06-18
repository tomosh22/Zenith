#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

class Flux_DynamicConstantBuffer;
class Flux_ModelInstance;
class Flux_SkeletonInstance;

// Wave 3: the model+transform+skeleton read that the Prepare-gather did is now
// done by the EC-side model gatherer (g_pfnZenithModelGather), so this TU names
// no EntityComponent type; the skeleton is read from the gathered
// Flux_ModelInstance.

// Per-frame draw item resolved on the main thread during Prepare. The (instance,
// skeleton, matrix) come from the EC-side model gather; the record callbacks run on
// worker threads and only dereference m_pxModelInstance / m_pxSkeleton (heap-stable
// Flux objects).
struct Flux_AnimatedMeshDrawItem
{
	Flux_ModelInstance*    m_pxModelInstance = nullptr;
	Flux_SkeletonInstance* m_pxSkeleton = nullptr;
	Zenith_Maths::Matrix4  m_xModelMatrix;
};

// Phase 9: state + behaviour for animated-meshes subsystem. Cross-subsystem
// deps (FluxGraphics) are reached via g_xEngine at point of use; the
// non-capturing fn-pointer trampolines (ExecuteGBuffer, the Prepare lambda,
// the ZENITH_TOOLS hot-reload lambda) re-enter via g_xEngine.AnimatedMeshes()
// since they cannot capture.
class Flux_AnimatedMeshesImpl
{
public:
	Flux_AnimatedMeshesImpl() = default;
	~Flux_AnimatedMeshesImpl() = default;

	Flux_AnimatedMeshesImpl(const Flux_AnimatedMeshesImpl&) = delete;
	Flux_AnimatedMeshesImpl& operator=(const Flux_AnimatedMeshesImpl&) = delete;

	void Initialise();
	void Shutdown();
	void BuildPipelines();

	void RenderToShadowMap(Flux_CommandBuffer& xCmdBuf, const Flux_DynamicConstantBuffer& xShadowMatrixBuffer);

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// Prepare callback: gathers the per-frame draw packet on the main thread.
	void GatherDrawPacket(void* pUserData);

	Flux_Pipeline& GetShadowPipeline() { return m_xShadowPipeline; }

	// Per-frame draw packet, populated in GatherDrawPacket (main thread) and
	// consumed by both the GBuffer record callback and all 4 shadow cascades.
	Zenith_Vector<Flux_AnimatedMeshDrawItem> m_xDrawPacket;

	Flux_Shader   m_xGBufferShader;
	// Cull permutation pair (see Flux_StaticMeshesImpl): one-sided culls back
	// faces, two-sided renders both with the shader-side normal flip.
	Flux_Pipeline m_xGBufferPipeline;			// one-sided (cull back)
	Flux_Pipeline m_xGBufferPipelineTwoSided;	// two-sided (cull none)
	Flux_Shader   m_xShadowShader;
	Flux_Pipeline m_xShadowPipeline;
};
