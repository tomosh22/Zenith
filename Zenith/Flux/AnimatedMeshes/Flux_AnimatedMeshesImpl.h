#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

class Flux_DynamicConstantBuffer;
class Flux_ModelInstance;
class Flux_SkeletonInstance;
class Zenith_ModelComponent;

// Cross-subsystem dependency injected into Initialise (Wave-15 DI seam, twin of
// Flux_StaticMeshesImpl, built on the WS9.2 Flux_HiZImpl / Wave-11 Flux_SSAOImpl /
// Wave-14 Flux_QuadsImpl template). Forward-declared here; the full header is
// pulled in by Flux_AnimatedMeshes.cpp. Flux_GraphicsImpl is the ONLY render dep.
// The Prepare-gather's g_xEngine.Scenes() reach is an ECS lookup that stays
// self-routed (NOT injected); bone buffers are sourced from the gathered
// Zenith_AnimatorComponent/Flux_SkeletonInstance, not from a render dep.
class Flux_GraphicsImpl;

// Per-frame draw item resolved on the main thread during Prepare. The model
// matrix is resolved here (not in the record path) so the live
// Zenith_TransformComponent read happens on the main thread too. The skeleton
// instance pointer is resolved from the live Zenith_ModelComponent here as well
// — the record callbacks run on worker threads and must not touch the ECS. The
// worker path only dereferences m_pxModelInstance / m_pxSkeleton (heap-stable
// Flux objects), never the component.
struct Flux_AnimatedMeshDrawItem
{
	Zenith_ModelComponent* m_pxModel = nullptr;
	Flux_ModelInstance*    m_pxModelInstance = nullptr;
	Flux_SkeletonInstance* m_pxSkeleton = nullptr;
	Zenith_Maths::Matrix4  m_xModelMatrix;
};

// Phase 9: state + behaviour for animated-meshes subsystem.
//
// Wave-15 DI seam (twin of Flux_StaticMeshesImpl; mirrors Flux_QuadsImpl): the
// lone render dependency (Flux_GraphicsImpl) is INJECTED through Initialise as an
// explicit reference and stored as a member pointer, rather than reached for via
// g_xEngine.FluxGraphics() inside the instance methods. Instance methods now also
// use plain direct member access for their own state instead of round-tripping
// through g_xEngine.AnimatedMeshes(). The only place g_xEngine self-lookup
// survives is the non-capturing fn-pointer trampolines (the ExecuteGBuffer graph
// callback, the SetupRenderGraph Prepare lambda, and the ZENITH_TOOLS hot-reload
// callback) — those cannot capture state, so they re-enter via
// g_xEngine.AnimatedMeshes() to recover this singleton instance and then route
// their members + FluxGraphics reach-in through it. The WS7 Prepare-gather's
// g_xEngine.Scenes() reach is an ECS lookup and stays self-routed (deliberately
// NOT injected).
class Flux_AnimatedMeshesImpl
{
public:
	Flux_AnimatedMeshesImpl() = default;
	~Flux_AnimatedMeshesImpl() = default;

	Flux_AnimatedMeshesImpl(const Flux_AnimatedMeshesImpl&) = delete;
	Flux_AnimatedMeshesImpl& operator=(const Flux_AnimatedMeshesImpl&) = delete;

	// Render dep is injected here and stored into m_pxGraphics below. This is the
	// WS9.2 DI template: explicit ref param -> stored member pointer.
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
	Zenith_Vector<Flux_AnimatedMeshDrawItem> m_xDrawPacket;

	Flux_Shader   m_xGBufferShader;
	Flux_Pipeline m_xGBufferPipeline;
	Flux_Shader   m_xShadowShader;
	Flux_Pipeline m_xShadowPipeline;

	// Injected render dependency (stored by Initialise). Default nullptr so a
	// default-constructed instance is headless-safe; the real boot path wires it
	// in via the AnimatedMeshes init trampoline (Flux_FeatureRegistry.cpp).
	Flux_GraphicsImpl* m_pxGraphics = nullptr;
};
