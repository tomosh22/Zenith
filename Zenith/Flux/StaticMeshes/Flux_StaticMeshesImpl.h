#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

class Flux_DynamicConstantBuffer;
class Flux_ModelInstance;
class Zenith_ModelComponent;

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
};
