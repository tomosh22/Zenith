#pragma once

#include "Flux/Primitives/Flux_Primitives.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Maths/Zenith_Maths.h"

// Per-primitive instance types. These were file-static in Flux_Primitives.cpp
// before Phase 7g; promoted here so the per-frame instance queues can become
// members of Flux_PrimitivesImpl.
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

// Phase 7g: per-Engine state for Primitives subsystem.
class Flux_PrimitivesImpl
{
public:
	Flux_PrimitivesImpl() = default;
	~Flux_PrimitivesImpl() = default;

	Flux_PrimitivesImpl(const Flux_PrimitivesImpl&) = delete;
	Flux_PrimitivesImpl& operator=(const Flux_PrimitivesImpl&) = delete;

	// Shaders and pipelines.
	Flux_Shader   m_xPrimitivesShader;
	Flux_Pipeline m_xPrimitivesPipeline;
	Flux_Pipeline m_xPrimitivesWireframePipeline;
	Flux_Pipeline m_xLinesPipeline;

	// Shared unit meshes (transformed via push constants).
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

	// Per-frame instance queues.
	Zenith_Vector<Flux_PrimitivesSphereInstance>   m_xSphereInstances;
	Zenith_Vector<Flux_PrimitivesCubeInstance>     m_xCubeInstances;
	Zenith_Vector<Flux_PrimitivesLineInstance>     m_xLineInstances;
	Zenith_Vector<Flux_PrimitivesCapsuleInstance>  m_xCapsuleInstances;
	Zenith_Vector<Flux_PrimitivesCylinderInstance> m_xCylinderInstances;
	Zenith_Vector<Flux_PrimitivesTriangleInstance> m_xTriangleInstances;

	// Dynamic triangle buffers (reused each frame).
	Flux_DynamicVertexBuffer m_xTriangleDynamicVertexBuffer;
	Flux_IndexBuffer         m_xTriangleIndexBuffer;
	bool                     m_bTriangleBuffersInitialised = false;

	// Thread-safe AddXXX calls.
	Zenith_Mutex m_xInstanceMutex;
};
