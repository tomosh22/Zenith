#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Maths/Zenith_Maths.h"

// Cross-subsystem dependency injected into Initialise (Wave-15 DI seam, identical
// shape to the Wave-14 Flux_QuadsImpl template). Forward-declared here; the full
// header is pulled in by Flux_Primitives.cpp. Flux_GraphicsImpl is the ONLY
// cross-subsystem dep — VulkanMemory()/DebugVariables() are engine-infra
// singletons and stay direct g_xEngine lookups (same carve-out as Quads/SSAO).
class Flux_GraphicsImpl;

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
// Wave-15 DI seam (mirrors Flux_QuadsImpl): the lone cross-subsystem dependency
// (Flux_GraphicsImpl) is INJECTED through Initialise as an explicit reference and
// stored as a member pointer, rather than reached for via g_xEngine.FluxGraphics()
// inside the instance methods. The only place g_xEngine self-lookup survives is
// the non-capturing fn-pointer trampoline (the ExecuteGBuffer graph callback, and
// the ZENITH_TOOLS hot-reload callback) — those cannot capture state, so they
// re-enter via g_xEngine.Primitives() to reach this singleton instance and then
// route their FluxGraphics reach-ins through the injected member.
class Flux_PrimitivesImpl
{
public:
	Flux_PrimitivesImpl() = default;
	~Flux_PrimitivesImpl() = default;

	Flux_PrimitivesImpl(const Flux_PrimitivesImpl&) = delete;
	Flux_PrimitivesImpl& operator=(const Flux_PrimitivesImpl&) = delete;

	// Cross-subsystem dep is injected here and stored into m_pxGraphics below.
	// This is the WS9.2 DI template: explicit ref param -> stored member pointer.
	void Initialise(Flux_GraphicsImpl& xGraphics);
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

	// Injected cross-subsystem dependency (stored by Initialise). Default nullptr
	// so a default-constructed instance is headless-safe; the real boot path wires
	// it in via the Primitives init trampoline (Flux_FeatureRegistry.cpp).
	Flux_GraphicsImpl* m_pxGraphics = nullptr;
};
