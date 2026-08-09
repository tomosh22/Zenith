#pragma once

#ifdef ZENITH_TOOLS

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_VertexCodec.h"   // the ONE place the packed colour lane is quantised
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"
#include "Flux/Shaders/Generated/Gizmos.h"   // the baked vertex layout the pins below compare against

#include <cstddef>   // offsetof

class Flux_RenderGraph;
class Zenith_Entity;

// One gizmo vertex, 16 bytes. This used to be an untyped run of six floats pushed
// into a Zenith_Vector<float> by InterleaveVertexData — which meant the ONLY thing
// tying the writer to the fetched layout was the reader's memory of the ordering.
// A struct gives the pins below something to be about: they compare it against the
// layout the Gizmos VS actually declares, so re-ordering either side fails the build.
//
// The colour is unorm8x4 storage behind a float3 shader field: handle tints are the
// per-axis authored colours (1,0,0)/(0,1,0)/(0,0,1) and their highlight variants, all
// in [0,1], so unorm8 is exact at the authoring precision. The fetch supplies the
// fourth byte and the declared float3 discards it — the alpha the packer writes is
// therefore inert, and is set to 1 only so the stored word reads sensibly in a capture.
struct Flux_GizmoVertex
{
	Zenith_Maths::Vector3 m_xPosition;
	u_int                 m_uColour;   // unorm8x4

	void SetColour(const Zenith_Maths::Vector3& xColour) { m_uColour = Flux_PackUnorm8x4(Zenith_Maths::Vector4(xColour, 1.0f)); }
	Zenith_Maths::Vector3 GetColour() const { return Zenith_Maths::Vector3(Flux_UnpackUnorm8x4(m_uColour)); }
};
static_assert(sizeof(Flux_GizmoVertex) == Flux_Generated_Gizmos::Gizmos::kVertexLayout.m_auStrides[0],
	"Flux_GizmoVertex must match the stride the Gizmos VS fetches");
static_assert(offsetof(Flux_GizmoVertex, m_xPosition) == Flux_Generated_Gizmos::Gizmos::kaxVertexAttribs[0].m_uOffset,
	"Flux_GizmoVertex.m_xPosition must sit at the generated POSITION offset");
static_assert(offsetof(Flux_GizmoVertex, m_uColour) == Flux_Generated_Gizmos::Gizmos::kaxVertexAttribs[1].m_uOffset,
	"Flux_GizmoVertex.m_uColour must sit at the generated COLOR offset");

// Full-table pin: a shader-side POSITION/COLOR swap would preserve neither offset now
// that the widths differ, but the type column is what catches a dropped [VtxFmt] tag —
// which would silently re-widen the stream to 24 B with no other symptom.
inline constexpr Flux_VertexLayoutElement kaxFLUX_GIZMO_EXPECTED_LAYOUT[] =
{
	{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, SHADER_DATA_TYPE_FLOAT3,   0u,  0u },
	{ FLUX_VERTEX_SEMANTIC_COLOR,    0u, SHADER_DATA_TYPE_UNORM8X4, 0u, 12u },
};
static_assert(Flux_Generated_Gizmos::Gizmos::kVertexLayout == Flux_VertexLayoutDesc{ kaxFLUX_GIZMO_EXPECTED_LAYOUT, 2u, { 16u, 0u } },
	"The Gizmos program's vertex layout drifted from the pinned contract — re-derive the expected table consciously if the VsIn really changed");

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

	void Initialise();
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

	// Wave 3: returns the target entity iff it currently has a transform (resolved
	// EC-side via g_xGizmoTransformAccess). The gizmo drives TRS get/set through that
	// accessor, so this header names no EntityComponent type.
	Zenith_Entity* GetGizmoTargetWithTransform();

	void InterleaveVertexData(Zenith_Vector<Flux_GizmoVertex>& xOut, const Zenith_Vector<Zenith_Maths::Vector3>& xPositions, const Zenith_Vector<Zenith_Maths::Vector3>& xColors);
	void UploadGizmoGeometry(Zenith_Vector<GizmoGeometry>& xGeometryList, const Zenith_Vector<Flux_GizmoVertex>& xVertexData, const Zenith_Vector<uint32_t>& xIndices, const Zenith_Maths::Vector3& xColor, GizmoComponent eComponent);

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

	Zenith_Vector<GizmoGeometry> m_xTranslateGeometry;
	Zenith_Vector<GizmoGeometry> m_xRotateGeometry;
	Zenith_Vector<GizmoGeometry> m_xScaleGeometry;
};

#endif // ZENITH_TOOLS
