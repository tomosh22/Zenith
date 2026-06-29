#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

class Flux_ModelInstance;
class Flux_MeshInstance;
class Flux_ShaderBinder;
class Zenith_MaterialAsset;
class Flux_RenderSceneSnapshot;

// Per-frame draw item resolved on the main thread during Prepare: one
// translucent/additive SUBMESH (the gather walks every model's meshes and
// pulls out only the blend-mode-routed ones — the opaque unified path skips
// them symmetrically). Sorted back-to-front by view-space depth before
// the record callback runs.
struct Flux_TranslucentDrawItem
{
	Flux_MeshInstance*    m_pxMeshInstance = nullptr;
	Zenith_MaterialAsset* m_pxMaterial = nullptr;
	Zenith_Maths::Matrix4 m_xModelMatrix;
	float                 m_fViewDepthSq = 0.0f;	// camera-distance² (sort key)
	bool                  m_bAdditive = false;
	bool                  m_bTwoSided = false;
};

// Forward-lit translucency: Translucent/Additive blend-mode materials render
// here AFTER the deferred composite (between SSAO and Fog in the setup walk:
// glass is not darkened by the post-hoc SSAO multiply, and fog still
// composites over it). Full lighting inline — IBL + sun/CSM + clustered
// lights — via Shaders/Translucency/Flux_Translucent_Forward.slang, which
// shares the uber-material evaluation with the G-buffer writers.
//
// v1 scope: STATIC meshes only. Skinned-animated translucent submeshes are
// skipped with a one-shot warning (forward bone path is future work).
class Flux_TranslucencyImpl
{
public:
	Flux_TranslucencyImpl() = default;
	~Flux_TranslucencyImpl() = default;

	Flux_TranslucencyImpl(const Flux_TranslucencyImpl&) = delete;
	Flux_TranslucencyImpl& operator=(const Flux_TranslucencyImpl&) = delete;

	void Initialise();
	void Shutdown();
	void BuildPipelines();

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// Phase 2: engine-owned uncullled snapshot injected at the composition root. The
	// Prepare reads it instead of running its own ECS scan. Keeps this TU off the ratchet.
	void SetSnapshot(const Flux_RenderSceneSnapshot* pxSnapshot) { m_pxSnapshot = pxSnapshot; }

	// Prepare callback: gathers + depth-sorts the per-frame packet (main thread).
	void GatherDrawPacket(void* pUserData);

	// Per-frame draw packet (gather-sorted back-to-front).
	Zenith_Vector<Flux_TranslucentDrawItem> m_xDrawPacket;

	Flux_Shader   m_xShader;
	// Blend x cull permutations (cull is baked into pipelines; blend mode is
	// per-material state).
	Flux_Pipeline m_xPipelineTranslucent;			// alpha blend, cull back
	Flux_Pipeline m_xPipelineTranslucentTwoSided;	// alpha blend, cull none
	Flux_Pipeline m_xPipelineAdditive;				// additive,    cull back
	Flux_Pipeline m_xPipelineAdditiveTwoSided;		// additive,    cull none

	// One-shot warning latch for unsupported animated translucent submeshes.
	bool m_bWarnedAnimatedTranslucent = false;

	// Phase 2: engine-owned uncullled scene snapshot, injected at the composition root
	// (non-owning). Read in GatherDrawPacket; null until injected (early boot).
	const Flux_RenderSceneSnapshot* m_pxSnapshot = nullptr;
};
