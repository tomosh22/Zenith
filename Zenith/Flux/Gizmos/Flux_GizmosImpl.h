#pragma once

#ifdef ZENITH_TOOLS

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

class Flux_RenderGraph;
class Zenith_Entity;
class Zenith_TransformComponent;
class Flux_GraphicsImpl;
class Flux_PrimitivesImpl;
class Zenith_Vulkan_MemoryManager;

enum class GizmoComponent
{
	None = 0,
	TranslateX,
	TranslateY,
	TranslateZ,
	RotateX,
	RotateY,
	RotateZ,
	ScaleX,
	ScaleY,
	ScaleZ,
	ScaleXYZ
};

enum class GizmoMode
{
	Translate,
	Rotate,
	Scale
};

// WS11.A: per-frame draw packet resolved on the main thread during the Gizmos
// pass Prepare (GatherGizmoPacket). The gizmo's editable transform is read from
// the live ECS there, and the resulting matrix/scale/entity-pos plus the
// interaction-highlight state are snapshotted here. The worker-thread record
// callback (ExecuteGizmos) reads ONLY this struct — it performs no ECS access
// and no shared-state mutation. m_bValid is false when there is no editable
// target, in which case the record callback early-outs.
struct Flux_GizmoDrawPacket
{
	bool                  m_bValid            = false;
	Zenith_Maths::Matrix4 m_xGizmoMatrix      = Zenith_Maths::Matrix4(1.0f);
	Zenith_Maths::Vector3 m_xEntityPos        = Zenith_Maths::Vector3(0, 0, 0);
	float                 m_fGizmoScale       = 1.0f;
	GizmoMode             m_eMode             = GizmoMode::Translate;
	GizmoComponent        m_eHoveredComponent = GizmoComponent::None;
	GizmoComponent        m_eActiveComponent  = GizmoComponent::None;
	bool                  m_bIsInteracting    = false;
};

// Phase 9: state + behaviour for Flux_Gizmos subsystem.
class Flux_GizmosImpl
{
public:
	Flux_GizmosImpl() = default;
	~Flux_GizmosImpl() = default;

	Flux_GizmosImpl(const Flux_GizmosImpl&) = delete;
	Flux_GizmosImpl& operator=(const Flux_GizmosImpl&) = delete;

	void Initialise(Flux_GraphicsImpl& xFluxGraphics, Flux_PrimitivesImpl& xPrimitives, Zenith_Vulkan_MemoryManager& xVulkanMemory);
	void BuildPipelines();
	void Shutdown();
	void Reset();

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// Prepare callback (main thread): performs the GetEditableTransform ECS read,
	// computes the gizmo matrix/scale/entity-pos, snapshots the interaction state,
	// and (under ZENITH_DEBUG) issues the interaction-bound wireframe cubes — all
	// on the main thread. Results land in m_xDrawPacket for the record callback.
	void GatherGizmoPacket(void* pUserData);

	void SetTargetEntity(Zenith_Entity* pxEntity);
	void SetGizmoMode(GizmoMode eMode);
	GizmoMode GetGizmoMode() const { return m_eMode; }

	void BeginInteraction(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir);
	void UpdateInteraction(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir);
	void EndInteraction();
	bool IsInteracting() const { return m_bIsInteracting; }

	GizmoComponent GetHoveredComponent() const { return m_eHoveredComponent; }
	GizmoComponent GetActiveComponent()  const { return m_eActiveComponent; }

	friend class Zenith_UnitTests;

	struct GizmoGeometry
	{
		Flux_VertexBuffer m_xVertexBuffer;
		Flux_IndexBuffer m_xIndexBuffer;
		uint32_t m_uIndexCount;
		Zenith_Maths::Vector3 m_xColor;
		GizmoComponent m_eComponent;
	};

	Zenith_TransformComponent* GetEditableTransform();

	void InterleaveVertexData(Zenith_Vector<float>& xOut, const Zenith_Vector<Zenith_Maths::Vector3>& xPositions, const Zenith_Vector<Zenith_Maths::Vector3>& xColors);
	void UploadGizmoGeometry(Zenith_Vector<GizmoGeometry>& xGeometryList, const Zenith_Vector<float>& xVertexData, const Zenith_Vector<uint32_t>& xIndices, const Zenith_Maths::Vector3& xColor, GizmoComponent eComponent);

	bool GetLineLineClosestPointParameter(const Zenith_Maths::Vector3& xAxisOrigin, const Zenith_Maths::Vector3& xAxis, const Zenith_Maths::Vector3& xRayOrigin, const Zenith_Maths::Vector3& xRayDir, float& fOutT);
	void ComputeTangentFrame(const Zenith_Maths::Vector3& xAxis, Zenith_Maths::Vector3& xOutTangent, Zenith_Maths::Vector3& xOutBitangent);

	void GenerateTranslationGizmoGeometry();
	void GenerateRotationGizmoGeometry();
	void GenerateScaleGizmoGeometry();
	void GenerateArrowGeometry(Zenith_Vector<GizmoGeometry>& geometryList, const Zenith_Maths::Vector3& axis, const Zenith_Maths::Vector3& color, GizmoComponent component);
	void GenerateCircleGeometry(Zenith_Vector<GizmoGeometry>& geometryList, const Zenith_Maths::Vector3& normal, const Zenith_Maths::Vector3& color, GizmoComponent component);
	void GenerateCubeGeometry(Zenith_Vector<GizmoGeometry>& geometryList, const Zenith_Maths::Vector3& offset, const Zenith_Maths::Vector3& color, GizmoComponent component);

	GizmoComponent RaycastGizmo(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir, float& outDistance);

	void ApplyTranslation(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir);
	void ApplyRotation(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir);
	void ApplyScale(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir);

	Zenith_Entity*           m_pxTargetEntity      = nullptr;
	GizmoMode                m_eMode               = GizmoMode::Translate;
	GizmoComponent           m_eHoveredComponent   = GizmoComponent::None;
	GizmoComponent           m_eActiveComponent    = GizmoComponent::None;
	bool                     m_bIsInteracting      = false;

	Zenith_Maths::Vector3    m_xInteractionStartPos   = Zenith_Maths::Vector3(0, 0, 0);
	Zenith_Maths::Vector3    m_xInitialEntityPosition = Zenith_Maths::Vector3(0, 0, 0);
	Zenith_Maths::Quaternion m_xInitialEntityRotation = Zenith_Maths::Quaternion(1, 0, 0, 0);
	Zenith_Maths::Vector3    m_xInitialEntityScale    = Zenith_Maths::Vector3(1, 1, 1);
	float                    m_fGizmoScale            = 1.0f;

	// WS11.A: per-frame snapshot populated on the main thread in GatherGizmoPacket
	// and consumed by the worker-thread record callback ExecuteGizmos.
	Flux_GizmoDrawPacket     m_xDrawPacket;

	Flux_Pipeline            m_xPipeline;
	Flux_Shader              m_xShader;
	Flux_CommandList         m_xCommandList{"Gizmos"};

	Zenith_Vector<GizmoGeometry> m_xTranslateGeometry;
	Zenith_Vector<GizmoGeometry> m_xRotateGeometry;
	Zenith_Vector<GizmoGeometry> m_xScaleGeometry;

	// Injected engine-infra dependencies (de-globalization pass).
	Flux_GraphicsImpl*           m_pxFluxGraphics = nullptr;
	Flux_PrimitivesImpl*         m_pxPrimitives   = nullptr;
	Zenith_Vulkan_MemoryManager* m_pxVulkanMemory = nullptr;
};

#endif // ZENITH_TOOLS
