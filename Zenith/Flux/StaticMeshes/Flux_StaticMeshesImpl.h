#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

class Flux_DynamicConstantBuffer;
class Flux_ModelInstance;
class Zenith_ModelComponent;

// Cross-subsystem dependency injected into Initialise (Wave-15 DI seam, built on
// the Wave-14 Flux_QuadsImpl / Flux_SDFsImpl template). Forward-declared here; the
// full header is pulled in by Flux_StaticMeshes.cpp. Flux_GraphicsImpl is the ONLY
// injectable cross-subsystem dep — the ECS reach (g_xEngine.Scenes() inside
// GatherDrawPacket) stays SELF-ROUTED because Scenes is ECS, not a Flux render dep,
// and injecting it would re-open the Flux<->ECS layering gate (WS13.A CI gate).
class Flux_GraphicsImpl;

// Per-frame draw item resolved on the main thread during Prepare. The model
// matrix is resolved here (not in the record path) so the live
// Zenith_TransformComponent read happens on the main thread too — the record
// callbacks run on worker threads and must not touch the ECS.
struct Flux_StaticMeshDrawItem
{
	Zenith_ModelComponent* m_pxModel = nullptr;
	Flux_ModelInstance*    m_pxModelInstance = nullptr;
	Zenith_Maths::Matrix4  m_xModelMatrix;
};

// Phase 9: state + behaviour for static-meshes subsystem.
//
// Wave-15 DI seam (mirrors Flux_QuadsImpl): the lone INJECTABLE cross-subsystem
// dependency (Flux_GraphicsImpl) is injected through Initialise as an explicit
// reference and stored as a member pointer, rather than reached for via
// g_xEngine.FluxGraphics() inside the methods. g_xEngine self-lookup survives only
// in the non-capturing fn-pointer trampolines (the static ExecuteGBuffer graph
// callback, the file-static RenderModelInstanceMeshes helper, and the ZENITH_TOOLS
// hot-reload callback) — those cannot capture state, so they re-enter via
// g_xEngine.StaticMeshes() to reach this singleton instance and then route their
// FluxGraphics reach-ins through the injected member. The ECS reach inside
// GatherDrawPacket (g_xEngine.Scenes()) stays self-routed by design.
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
