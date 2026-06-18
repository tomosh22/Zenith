#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

class Flux_DynamicConstantBuffer;
class Flux_ModelInstance;
class Flux_ShaderBinder;
class Zenith_MaterialAsset;

// Wave 3: the model+transform read that GatherDrawPacket used to do is now
// performed by the EC-side gatherer (g_pfnZenithModelGather), so this TU names
// no EntityComponent type.

// Per-frame draw item resolved on the main thread during Prepare. The (instance,
// matrix) pairs come from the EC-side model gather; the record callbacks run on
// worker threads and only touch these resolved Flux pointers / matrices.
struct Flux_StaticMeshDrawItem
{
	Flux_ModelInstance*    m_pxModelInstance = nullptr;
	Zenith_Maths::Matrix4  m_xModelMatrix;
};

// Phase 9: state + behaviour for static-meshes subsystem. Cross-subsystem deps
// (FluxGraphics) are reached via g_xEngine at point of use; the non-capturing
// fn-pointer trampolines (ExecuteGBuffer, the Prepare lambda, the ZENITH_TOOLS
// hot-reload lambda) re-enter via g_xEngine.StaticMeshes() since they cannot
// capture.
class Flux_StaticMeshesImpl
{
public:
	Flux_StaticMeshesImpl() = default;
	~Flux_StaticMeshesImpl() = default;

	Flux_StaticMeshesImpl(const Flux_StaticMeshesImpl&) = delete;
	Flux_StaticMeshesImpl& operator=(const Flux_StaticMeshesImpl&) = delete;

	void Initialise();
	void Shutdown();
	void BuildPipelines();

	void RenderToShadowMap(Flux_CommandBuffer& xCmdBuf, const Flux_DynamicConstantBuffer& xShadowMatrixBuffer);

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// Prepare callback: gathers the per-frame draw packet on the main thread.
	void GatherDrawPacket(void* pUserData);

	// Record-path helpers, promoted from file-static free functions so they can
	// access the GBuffer shader directly. RenderModelInstanceMeshes is called
	// from the recovered instance inside the ExecuteGBuffer trampoline
	// (xZZ.RenderModelInstanceMeshes(...)), which is why it stays public
	// alongside the already-public record state.
	// bTwoSidedPass selects which materials this walk draws (meshes whose
	// material's two-sidedness doesn't match the active cull pipeline are
	// skipped — ExecuteGBuffer walks the packet once per cull mode).
	void RenderModelInstanceMeshes(Flux_CommandBuffer* pxCmdList, Flux_ShaderBinder& xBinder,
		Flux_ModelInstance* pxModelInstance, const Zenith_Maths::Matrix4& xModelMatrix,
		bool bTwoSidedPass);
	void DrawStaticMesh(Flux_CommandBuffer* pxCmdList, Flux_ShaderBinder& xBinder,
		const Zenith_Maths::Matrix4& xModelMatrix,
		Zenith_MaterialAsset* pxMaterial,
		u_int uIndexCount);

	Flux_Pipeline& GetShadowPipeline() { return m_xShadowPipeline; }

	// Per-frame draw packet, populated in GatherDrawPacket (main thread) and
	// consumed by both the GBuffer record callback and all 4 shadow cascades.
	Zenith_Vector<Flux_StaticMeshDrawItem> m_xDrawPacket;

	Flux_Shader   m_xGBufferShader;
	// Cull mode is baked into the pipeline (no dynamic cull on Android), so
	// per-material two-sidedness needs a permutation pair: one-sided =
	// CULL_MODE_BACK, two-sided = CULL_MODE_NONE.
	Flux_Pipeline m_xGBufferPipeline;			// one-sided (cull back)
	Flux_Pipeline m_xGBufferPipelineTwoSided;	// two-sided (cull none)
	Flux_Shader   m_xShadowShader;
	Flux_Pipeline m_xShadowPipeline;

	// Debug safety valve: route every draw through the two-sided pipeline
	// (cull none — the engine's historical behaviour).
	bool m_bDbgForceCullNone = false;
};
