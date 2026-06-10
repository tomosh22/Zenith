#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Maths/Zenith_Maths.h"

class Flux_CommandList;
class Flux_ShaderBinder;

// Per-primitive instance types (file-static before Phase 7g; promoted to
// engine-owned arrays).
struct Flux_PrimitivesSphereInstance
{
	Zenith_Maths::Vector3 m_xCenter;
	float                 m_fRadius;
	Zenith_Maths::Vector3 m_xColor;
};

struct Flux_PrimitivesCubeInstance
{
	Zenith_Maths::Vector3 m_xCenter;
	Zenith_Maths::Vector3 m_xHalfExtents;
	Zenith_Maths::Vector3 m_xColor;
	bool                  m_bWireframe;
};

struct Flux_PrimitivesLineInstance
{
	Zenith_Maths::Vector3 m_xStart;
	Zenith_Maths::Vector3 m_xEnd;
	Zenith_Maths::Vector3 m_xColor;
	float                 m_fThickness;
};

struct Flux_PrimitivesCapsuleInstance
{
	Zenith_Maths::Vector3 m_xStart;
	Zenith_Maths::Vector3 m_xEnd;
	float                 m_fRadius;
	Zenith_Maths::Vector3 m_xColor;
};

struct Flux_PrimitivesCylinderInstance
{
	Zenith_Maths::Vector3 m_xStart;
	Zenith_Maths::Vector3 m_xEnd;
	float                 m_fRadius;
	Zenith_Maths::Vector3 m_xColor;
};

struct Flux_PrimitivesTriangleInstance
{
	Zenith_Maths::Vector3 m_xV0;
	Zenith_Maths::Vector3 m_xV1;
	Zenith_Maths::Vector3 m_xV2;
	Zenith_Maths::Vector3 m_xColor;
};

// Phase 9: state + behaviour for Primitives subsystem.
//
// Cross-subsystem deps (Flux_GraphicsImpl and the memory manager) are reached via
// g_xEngine at point of use. The former file-static render helpers
// (RenderSpherePrimitives/...) are members so their reaches resolve through `this`.
// The non-capturing fn-pointer trampolines (the ExecuteGBuffer graph callback and
// the ZENITH_TOOLS hot-reload callback) cannot capture state, so they re-enter via
// g_xEngine.Primitives() to reach this singleton instance.
class Flux_PrimitivesImpl
{
public:
	Flux_PrimitivesImpl() = default;
	~Flux_PrimitivesImpl() = default;

	Flux_PrimitivesImpl(const Flux_PrimitivesImpl&) = delete;
	Flux_PrimitivesImpl& operator=(const Flux_PrimitivesImpl&) = delete;

	void Initialise();
	void BuildPipelines();
	void Shutdown();
	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	void AddSphere(const Zenith_Maths::Vector3& xCenter, float fRadius, const Zenith_Maths::Vector3& xColor);
	void AddCube(const Zenith_Maths::Vector3& xCenter, const Zenith_Maths::Vector3& xHalfExtents, const Zenith_Maths::Vector3& xColor);
	void AddWireframeCube(const Zenith_Maths::Vector3& xCenter, const Zenith_Maths::Vector3& xHalfExtents, const Zenith_Maths::Vector3& xColor);
	void AddLine(const Zenith_Maths::Vector3& xStart, const Zenith_Maths::Vector3& xEnd, const Zenith_Maths::Vector3& xColor, float fThickness = 0.02f);
	void AddCapsule(const Zenith_Maths::Vector3& xStart, const Zenith_Maths::Vector3& xEnd, float fRadius, const Zenith_Maths::Vector3& xColor);
	void AddCylinder(const Zenith_Maths::Vector3& xStart, const Zenith_Maths::Vector3& xEnd, float fRadius, const Zenith_Maths::Vector3& xColor);
	void AddTriangle(const Zenith_Maths::Vector3& xV0, const Zenith_Maths::Vector3& xV1,
		const Zenith_Maths::Vector3& xV2, const Zenith_Maths::Vector3& xColor);

	void Clear();

	void AddCross(const Zenith_Maths::Vector3& xCenter, float fSize, const Zenith_Maths::Vector3& xColor);
	void AddCircle(const Zenith_Maths::Vector3& xCenter, float fRadius, const Zenith_Maths::Vector3& xColor,
		const Zenith_Maths::Vector3& xNormal = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), uint32_t uSegments = 32);
	void AddArrow(const Zenith_Maths::Vector3& xStart, const Zenith_Maths::Vector3& xEnd,
		const Zenith_Maths::Vector3& xColor, float fThickness = 0.02f, float fHeadSize = 0.15f);
	void AddConeOutline(const Zenith_Maths::Vector3& xApex, const Zenith_Maths::Vector3& xDirection,
		float fAngle, float fLength, const Zenith_Maths::Vector3& xColor, uint32_t uSegments = 16);
	void AddArc(const Zenith_Maths::Vector3& xCenter, float fRadius, float fStartAngle, float fEndAngle,
		const Zenith_Maths::Vector3& xColor, const Zenith_Maths::Vector3& xNormal = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f),
		uint32_t uSegments = 16);
	void AddPolygonOutline(const Zenith_Maths::Vector3* axVertices, uint32_t uVertexCount,
		const Zenith_Maths::Vector3& xColor, bool bClosed = true);
	void AddGrid(const Zenith_Maths::Vector3& xCenter, float fSize, uint32_t uDivisions,
		const Zenith_Maths::Vector3& xColor);
	void AddAxes(const Zenith_Maths::Vector3& xOrigin, float fSize);

	// Render helpers — promoted from .cpp file-static free functions so their
	// reaches resolve through `this`. PUBLIC because the non-capturing
	// ExecuteGBuffer graph trampoline (a free function) calls them on the
	// singleton instance it recovers via g_xEngine.Primitives().
	void EmitPrimitiveDraw(Flux_CommandList* pxCmdList, Flux_ShaderBinder& xBinder,
		const Zenith_Maths::Matrix4& xModelMatrix,
		const Zenith_Maths::Vector3& xColor,
		u_int uIndexCount);
	void RenderSpherePrimitives(Flux_CommandList* pxCmdList, Flux_ShaderBinder& xBinder,
		const Zenith_Vector<Flux_PrimitivesSphereInstance>& xInstances);
	void RenderCubePrimitives(Flux_CommandList* pxCmdList, Flux_ShaderBinder& xBinder,
		const Zenith_Vector<Flux_PrimitivesCubeInstance>& xInstances);
	void RenderLinePrimitives(Flux_CommandList* pxCmdList, Flux_ShaderBinder& xBinder,
		const Zenith_Vector<Flux_PrimitivesLineInstance>& xInstances);
	void RenderCapsulePrimitives(Flux_CommandList* pxCmdList, Flux_ShaderBinder& xBinder,
		const Zenith_Vector<Flux_PrimitivesCapsuleInstance>& xInstances);
	void RenderCylinderPrimitives(Flux_CommandList* pxCmdList, Flux_ShaderBinder& xBinder,
		const Zenith_Vector<Flux_PrimitivesCylinderInstance>& xInstances);
	void RenderTrianglePrimitives(Flux_CommandList* pxCmdList, Flux_ShaderBinder& xBinder,
		const Zenith_Vector<Flux_PrimitivesTriangleInstance>& xInstances);

	Flux_Shader   m_xPrimitivesShader;
	Flux_Pipeline m_xPrimitivesPipeline;
	Flux_Pipeline m_xPrimitivesWireframePipeline;
	Flux_Pipeline m_xLinesPipeline;

	Flux_VertexBuffer m_xSphereVertexBuffer;
	Flux_IndexBuffer  m_xSphereIndexBuffer;
	u_int             m_uSphereIndexCount = 0;

	Flux_VertexBuffer m_xCubeVertexBuffer;
	Flux_IndexBuffer  m_xCubeIndexBuffer;
	u_int             m_uCubeIndexCount = 0;

	Flux_VertexBuffer m_xCapsuleVertexBuffer;
	Flux_IndexBuffer  m_xCapsuleIndexBuffer;
	u_int             m_uCapsuleIndexCount = 0;

	Flux_VertexBuffer m_xCylinderVertexBuffer;
	Flux_IndexBuffer  m_xCylinderIndexBuffer;
	u_int             m_uCylinderIndexCount = 0;

	Flux_VertexBuffer m_xLineVertexBuffer;
	Flux_IndexBuffer  m_xLineIndexBuffer;
	u_int             m_uLineIndexCount = 0;

	Zenith_Vector<Flux_PrimitivesSphereInstance>   m_xSphereInstances;
	Zenith_Vector<Flux_PrimitivesCubeInstance>     m_xCubeInstances;
	Zenith_Vector<Flux_PrimitivesLineInstance>     m_xLineInstances;
	Zenith_Vector<Flux_PrimitivesCapsuleInstance>  m_xCapsuleInstances;
	Zenith_Vector<Flux_PrimitivesCylinderInstance> m_xCylinderInstances;
	Zenith_Vector<Flux_PrimitivesTriangleInstance> m_xTriangleInstances;

	Flux_DynamicVertexBuffer m_xTriangleDynamicVertexBuffer;
	Flux_IndexBuffer         m_xTriangleIndexBuffer;
	bool                     m_bTriangleBuffersInitialised = false;

	Zenith_Mutex m_xInstanceMutex;
};
