#pragma once

#ifdef ZENITH_TOOLS

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

class Flux_RenderGraph;
class Zenith_Entity;
class Zenith_TransformComponent;

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

// Phase 9: state + behaviour for Flux_Gizmos subsystem.
class Flux_GizmosImpl
{
public:
	Flux_GizmosImpl() = default;
	~Flux_GizmosImpl() = default;

	Flux_GizmosImpl(const Flux_GizmosImpl&) = delete;
	Flux_GizmosImpl& operator=(const Flux_GizmosImpl&) = delete;

	void Initialise();
	void BuildPipelines();
	void Shutdown();
	void Reset();

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

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

	Flux_Pipeline            m_xPipeline;
	Flux_Shader              m_xShader;
	Flux_CommandList         m_xCommandList{"Gizmos"};

	Zenith_Vector<GizmoGeometry> m_xTranslateGeometry;
	Zenith_Vector<GizmoGeometry> m_xRotateGeometry;
	Zenith_Vector<GizmoGeometry> m_xScaleGeometry;
};

#endif // ZENITH_TOOLS
