#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

class Flux_DynamicConstantBuffer;
class Flux_ModelInstance;
class Flux_ShaderBinder;
class Zenith_MaterialAsset;
class Flux_RenderSceneSnapshot;

// Phase 2: the model+transform read that GatherDrawPacket used to do is now sourced
// from the engine-owned Flux_RenderSceneSnapshot (injected via SetSnapshot), so this
// TU names no EntityComponent type.

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

	void RenderToShadowMap(Flux_CommandBuffer& xCmdBuf, u_int uCascade);

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// Phase 2: injected once at the composition root (Zenith_Engine.cpp). The renderer
	// owns the uncullled Flux_RenderSceneSnapshot and rebuilds it once per frame before
	// any Prepare runs; GatherDrawPacket reads it instead of running its own ECS scan.
	// Injection (not g_xEngine.FluxRenderer()) keeps this TU off the singleton ratchet.
	void SetSnapshot(const Flux_RenderSceneSnapshot* pxSnapshot) { m_pxSnapshot = pxSnapshot; }

	// Prepare callback (G-buffer pass): ensures both the camera packet (frustum-culled, for
	// ExecuteGBuffer) and the shadow packet (uncullled, for RenderToShadowMap) are built for
	// the current snapshot.
	void GatherDrawPacket(void* pUserData);

	// Phase 3: generation-guarded packet builders. Each is a no-op if already built for the
	// current snapshot generation, so whichever Prepare runs first (G-buffer vs shadow
	// cascade-0) builds it. EnsureCameraPacket camera-frustum-CULLS (drops off-screen opaque
	// draws); EnsureShadowPacket is UNCULLLED (off-screen casters still cast). Calling
	// EnsureShadowPacket from the shadow Prepare too is what keeps shadows correct when the
	// G-buffer pass is force-disabled.
	void EnsureCameraPacket();
	void EnsureShadowPacket();

	// Pure, GPU-free packet builders the Ensure* wrappers delegate to (the generation guard +
	// member storage stay on the wrappers). Static + snapshot-driven so unit tests can drive the
	// EXACT cull decision the G-buffer/shadow consumers apply against a hand-built snapshot,
	// WITHOUT constructing a GPU-backed subsystem (whose Flux_Shader/Flux_Pipeline members reach
	// the backend device in their destructors). BuildCameraPacket camera-frustum-CULLS iff the
	// snapshot's camera frustum is valid (skipped otherwise, so a camera-invalid frame culls
	// nothing); BuildShadowPacket is UNCULLLED. Both skip skinned-animated items.
	static void BuildCameraPacket(const Flux_RenderSceneSnapshot& xSnapshot, Zenith_Vector<Flux_StaticMeshDrawItem>& xOut);
	static void BuildShadowPacket(const Flux_RenderSceneSnapshot& xSnapshot, Zenith_Vector<Flux_StaticMeshDrawItem>& xOut);

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

	// Phase 3: split per-frame draw packets, populated on the main thread (Prepare).
	//  * m_xCameraDrawPacket — frustum-CULLED; consumed by the GBuffer record callback.
	//  * m_xShadowDrawPacket  — UNCULLLED; consumed by all 4 shadow cascades.
	// Each carries the snapshot generation it was built for (UINT32_MAX = never built) so
	// the Ensure* builders rebuild once per snapshot and no-op on repeat calls.
	Zenith_Vector<Flux_StaticMeshDrawItem> m_xCameraDrawPacket;
	Zenith_Vector<Flux_StaticMeshDrawItem> m_xShadowDrawPacket;
	uint32_t m_uCameraPacketGen = UINT32_MAX;
	uint32_t m_uShadowPacketGen = UINT32_MAX;

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

	// Phase 2: the engine-owned uncullled scene snapshot, injected at the composition
	// root (non-owning). Read in GatherDrawPacket; null until injected (early boot).
	const Flux_RenderSceneSnapshot* m_pxSnapshot = nullptr;
};
