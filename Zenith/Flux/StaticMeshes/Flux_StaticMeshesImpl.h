#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

class Flux_DynamicConstantBuffer;
class Flux_ModelInstance;
class Flux_CommandList;
class Flux_ShaderBinder;
class Zenith_MaterialAsset;

// Cross-subsystem dependency injected into Initialise (Wave-15 DI seam, built on
// the Wave-14 Flux_QuadsImpl / Flux_SDFsImpl template). Forward-declared here; the
// full header is pulled in by Flux_StaticMeshes.cpp. Wave 3: the model+transform read
// that GatherDrawPacket used to do is now performed by the EC-side gatherer
// (g_pfnZenithModelGather), so this TU names no EntityComponent type.
class Flux_GraphicsImpl;

// Per-frame draw item resolved on the main thread during Prepare. The (instance,
// matrix) pairs come from the EC-side model gather; the record callbacks run on
// worker threads and only touch these resolved Flux pointers / matrices.
struct Flux_StaticMeshDrawItem
{
	Flux_ModelInstance*    m_pxModelInstance = nullptr;
	Zenith_Maths::Matrix4  m_xModelMatrix;
};

// Phase 9: state + behaviour for static-meshes subsystem.
//
// Wave-15 DI seam (mirrors Flux_QuadsImpl): the lone INJECTABLE cross-subsystem
// dependency (Flux_GraphicsImpl) is injected through Initialise as an explicit
// reference and stored as a member pointer, rather than reached for via
// g_xEngine.FluxGraphics() inside the methods.
//
// g_xEngine de-globalization (mirrors Flux_Quads/Flux_Text/Flux_SDFs): every
// g_xEngine.StaticMeshes() self-lookup inside an INSTANCE method is now plain
// member access (this->), and the two file-static record helpers that reached the
// singleton (RenderModelInstanceMeshes, DrawStaticMesh) were PROMOTED to private
// member methods so they reach the shader / injected FluxGraphics directly. The
// only g_xEngine reaches that remain are:
//   * the non-capturing fn-pointer TRAMPOLINES — the static ExecuteGBuffer
//     render-graph callback, the Prepare lambda, and the ZENITH_TOOLS hot-reload
//     lambda — which cannot capture state, so they re-enter via
//     g_xEngine.StaticMeshes() to recover this singleton and then drive everything
//     through the recovered instance; and
//   * the ECS reach inside GatherDrawPacket (g_xEngine.Scenes()), which stays
//     self-routed BY DESIGN — Scenes is ECS, not a Flux render dep, and injecting
//     it would re-open the Flux<->ECS layering gate (WS13.A CI gate).
class Flux_StaticMeshesImpl
{
public:
	Flux_StaticMeshesImpl() = default;
	~Flux_StaticMeshesImpl() = default;

	Flux_StaticMeshesImpl(const Flux_StaticMeshesImpl&) = delete;
	Flux_StaticMeshesImpl& operator=(const Flux_StaticMeshesImpl&) = delete;

	// Cross-subsystem dep is injected here and stored into m_pxGraphics below.
	// This is the Wave-14 DI template: explicit ref param -> stored member pointer.
	void Initialise(Flux_GraphicsImpl& xGraphics);
	void Shutdown();
	void BuildPipelines();

	void RenderToShadowMap(Flux_CommandList& xCmdBuf, const Flux_DynamicConstantBuffer& xShadowMatrixBuffer);

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// Prepare callback: gathers the per-frame draw packet on the main thread.
	void GatherDrawPacket(void* pUserData);

	// Record-path helpers, promoted from file-static free functions during the
	// g_xEngine de-globalization: both reached the singleton (the GBuffer shader /
	// the injected FluxGraphics), so they are member methods now and access those
	// directly. RenderModelInstanceMeshes is called from the recovered instance
	// inside the ExecuteGBuffer trampoline (xZZ.RenderModelInstanceMeshes(...)),
	// which is why it stays public alongside the already-public record state.
	void RenderModelInstanceMeshes(Flux_CommandList* pxCmdList, Flux_ShaderBinder& xBinder,
		Flux_ModelInstance* pxModelInstance, const Zenith_Maths::Matrix4& xModelMatrix);
	void DrawStaticMesh(Flux_CommandList* pxCmdList, Flux_ShaderBinder& xBinder,
		const Zenith_Maths::Matrix4& xModelMatrix,
		Zenith_MaterialAsset* pxMaterial,
		u_int uIndexCount);

	Flux_Pipeline& GetShadowPipeline() { return m_xShadowPipeline; }

	// Per-frame draw packet, populated in GatherDrawPacket (main thread) and
	// consumed by both the GBuffer record callback and all 4 shadow cascades.
	Zenith_Vector<Flux_StaticMeshDrawItem> m_xDrawPacket;

	Flux_Shader   m_xGBufferShader;
	Flux_Pipeline m_xGBufferPipeline;
	Flux_Shader   m_xShadowShader;
	Flux_Pipeline m_xShadowPipeline;

	// Injected cross-subsystem dependency (stored by Initialise). Default nullptr
	// so a default-constructed instance is headless-safe; the real boot path wires
	// it in via the StaticMeshes init trampoline (Flux_FeatureRegistry.cpp).
	Flux_GraphicsImpl* m_pxGraphics = nullptr;
};
