#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

class Flux_DynamicConstantBuffer;
class Flux_ModelInstance;
class Flux_SkeletonInstance;
class Flux_RenderSceneSnapshot;

// Phase 2: the model+transform+skeleton read that the Prepare-gather did is now done
// by the engine-owned Flux_RenderSceneSnapshot (injected via SetSnapshot), so this TU
// names no EntityComponent type; the skeleton is read from the snapshot's Flux_ModelInstance.

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

	void RenderToShadowMap(Flux_CommandBuffer& xCmdBuf, u_int uCascade);

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// Phase 2: injected once at the composition root (Zenith_Engine.cpp) — the engine-owned
	// uncullled snapshot the renderer rebuilds once per frame. GatherDrawPacket reads it
	// instead of running its own ECS scan. Injection keeps this TU off the singleton ratchet.
	void SetSnapshot(const Flux_RenderSceneSnapshot* pxSnapshot) { m_pxSnapshot = pxSnapshot; }

	// Prepare callback (animated G-buffer pass): ensures the animated packet for the
	// current snapshot.
	void GatherDrawPacket(void* pUserData);

	// Phase 3: generation-guarded packet builder. Animated meshes are NOT camera-culled
	// (no conservative animation bounds yet), so there is a single uncullled packet used by
	// both the G-buffer pass and the shadow cascades. Called from the animated G-buffer
	// Prepare AND the shadow cascade-0 Prepare (generation-guarded) so the packet is built
	// even when the G-buffer pass is force-disabled.
	void EnsureAnimatedPacket();

	Flux_Pipeline& GetShadowPipeline() { return m_xShadowPipeline; }

	// Per-frame draw packet, populated by EnsureAnimatedPacket (main thread) and consumed by
	// both the GBuffer record callback and all 4 shadow cascades. m_uAnimatedPacketGen is the
	// snapshot generation it was built for (UINT32_MAX = never built).
	Zenith_Vector<Flux_AnimatedMeshDrawItem> m_xDrawPacket;
	uint32_t m_uAnimatedPacketGen = UINT32_MAX;

	Flux_Shader   m_xGBufferShader;
	// Cull permutation pair (see Flux_StaticMeshesImpl): one-sided culls back
	// faces, two-sided renders both with the shader-side normal flip.
	Flux_Pipeline m_xGBufferPipeline;			// one-sided (cull back)
	Flux_Pipeline m_xGBufferPipelineTwoSided;	// two-sided (cull none)
	Flux_Shader   m_xShadowShader;
	Flux_Pipeline m_xShadowPipeline;

	// Phase 2: engine-owned uncullled scene snapshot, injected at the composition root
	// (non-owning). Read in GatherDrawPacket; null until injected (early boot).
	const Flux_RenderSceneSnapshot* m_pxSnapshot = nullptr;
};
