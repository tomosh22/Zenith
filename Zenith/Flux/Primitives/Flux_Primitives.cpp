#include "Zenith.h"

#include "Flux/Primitives/Flux_Primitives.h"

#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Collections/Zenith_Vector.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "DebugVariables/Zenith_DebugVariables.h"

#include <cmath>

// ========== STATIC DATA ==========

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_PRIMITIVES, Flux_Primitives::Render, nullptr);
static Flux_CommandList g_xCommandList("Primitives");

// Shaders and pipelines
static Flux_Shader s_xPrimitivesShader;
static Flux_Pipeline s_xPrimitivesPipeline;
static Flux_Pipeline s_xPrimitivesWireframePipeline;
static Flux_Pipeline s_xLinesPipeline;

// Shared geometry for primitives (unit meshes, transformed via push constants)
static Flux_VertexBuffer s_xSphereVertexBuffer;
static Flux_IndexBuffer s_xSphereIndexBuffer;
static u_int s_uSphereIndexCount = 0;

static Flux_VertexBuffer s_xCubeVertexBuffer;
static Flux_IndexBuffer s_xCubeIndexBuffer;
static u_int s_uCubeIndexCount = 0;

static Flux_VertexBuffer s_xCapsuleVertexBuffer;
static Flux_IndexBuffer s_xCapsuleIndexBuffer;
static u_int s_uCapsuleIndexCount = 0;

static Flux_VertexBuffer s_xCylinderVertexBuffer;
static Flux_IndexBuffer s_xCylinderIndexBuffer;
static u_int s_uCylinderIndexCount = 0;

static Flux_VertexBuffer s_xLineVertexBuffer;
static Flux_IndexBuffer s_xLineIndexBuffer;
static u_int s_uLineIndexCount = 0;

// Debug variables
DEBUGVAR bool dbg_bEnablePrimitives = true;

// ========== INSTANCE DATA STRUCTURES ==========

// Vertex format: Position (vec3), Normal (vec3), Color (vec3)
struct PrimitiveVertex
{
	Zenith_Maths::Vector3 m_xPosition;
	Zenith_Maths::Vector3 m_xNormal;
	Zenith_Maths::Vector3 m_xColor;
};

// Push constant: 4x4 model matrix + vec3 color + padding
struct PrimitivePushConstant
{
	Zenith_Maths::Matrix4 m_xModelMatrix;
	Zenith_Maths::Vector3 m_xColor;
	float m_fPadding;
};

// Instance data for each primitive type
struct SphereInstance
{
	Zenith_Maths::Vector3 m_xCenter;
	float m_fRadius;
	Zenith_Maths::Vector3 m_xColor;
};

struct CubeInstance
{
	Zenith_Maths::Vector3 m_xCenter;
	Zenith_Maths::Vector3 m_xHalfExtents;
	Zenith_Maths::Vector3 m_xColor;
	bool m_bWireframe;
};

struct LineInstance
{
	Zenith_Maths::Vector3 m_xStart;
	Zenith_Maths::Vector3 m_xEnd;
	Zenith_Maths::Vector3 m_xColor;
	float m_fThickness;
};

struct CapsuleInstance
{
	Zenith_Maths::Vector3 m_xStart;
	Zenith_Maths::Vector3 m_xEnd;
	float m_fRadius;
	Zenith_Maths::Vector3 m_xColor;
};

struct CylinderInstance
{
	Zenith_Maths::Vector3 m_xStart;
	Zenith_Maths::Vector3 m_xEnd;
	float m_fRadius;
	Zenith_Maths::Vector3 m_xColor;
};

// Per-frame instance queues
static Zenith_Vector<SphereInstance> g_xSphereInstances;
static Zenith_Vector<CubeInstance> g_xCubeInstances;
static Zenith_Vector<LineInstance> g_xLineInstances;
static Zenith_Vector<CapsuleInstance> g_xCapsuleInstances;
static Zenith_Vector<CylinderInstance> g_xCylinderInstances;

// Mutex for thread-safe AddXXX calls (in case called from multiple threads)
static Zenith_Mutex g_xInstanceMutex;

// ========== PROCEDURAL MESH GENERATION ==========

/**
 * Generate a unit sphere (radius 1.0, centered at origin) using UV sphere algorithm
 * @param uLatitudeSegments Number of latitude divisions (more = smoother)
 * @param uLongitudeSegments Number of longitude divisions (more = smoother)
 */
static void GenerateUnitSphere(Zenith_Vector<PrimitiveVertex>& xVertices, Zenith_Vector<u_int>& xIndices, u_int uLatitudeSegments = 16, u_int uLongitudeSegments = 32)
{
	xVertices.Clear();
	xIndices.Clear();

	const float PI = 3.14159265359f;

	// Generate vertices
	for (u_int lat = 0; lat <= uLatitudeSegments; ++lat)
	{
		float theta = lat * PI / uLatitudeSegments;  // 0 to PI (top to bottom)
		float sinTheta = sinf(theta);
		float cosTheta = cosf(theta);

		for (u_int lon = 0; lon <= uLongitudeSegments; ++lon)
		{
			float phi = lon * 2.0f * PI / uLongitudeSegments;  // 0 to 2PI (around equator)
			float sinPhi = sinf(phi);
			float cosPhi = cosf(phi);

			PrimitiveVertex xVertex;
			xVertex.m_xPosition.x = sinTheta * cosPhi;
			xVertex.m_xPosition.y = cosTheta;
			xVertex.m_xPosition.z = sinTheta * sinPhi;
			xVertex.m_xNormal = xVertex.m_xPosition;  // For unit sphere, normal = position
			xVertex.m_xColor = Zenith_Maths::Vector3(1, 1, 1);  // Will be overridden by push constant

			xVertices.PushBack(xVertex);
		}
	}

	// Generate indices (CCW winding)
	for (u_int lat = 0; lat < uLatitudeSegments; ++lat)
	{
		for (u_int lon = 0; lon < uLongitudeSegments; ++lon)
		{
			u_int current = lat * (uLongitudeSegments + 1) + lon;
			u_int next = current + uLongitudeSegments + 1;

			// Triangle 1
			xIndices.PushBack(current);
			xIndices.PushBack(next);
			xIndices.PushBack(current + 1);

			// Triangle 2
			xIndices.PushBack(current + 1);
			xIndices.PushBack(next);
			xIndices.PushBack(next + 1);
		}
	}
}

/**
 * Generate a unit cube (side length 2.0, centered at origin, ranging from -1 to +1)
 */
static void GenerateUnitCube(Zenith_Vector<PrimitiveVertex>& xVertices, Zenith_Vector<u_int>& xIndices)
{
	xVertices.Clear();
	xIndices.Clear();

	// Cube vertices with normals (24 vertices - 4 per face for correct normals)
	const Zenith_Maths::Vector3 positions[24] = {
		// Front face (+Z)
		{-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1},
		// Back face (-Z)
		{ 1, -1, -1}, {-1, -1, -1}, {-1,  1, -1}, { 1,  1, -1},
		// Top face (+Y)
		{-1,  1,  1}, { 1,  1,  1}, { 1,  1, -1}, {-1,  1, -1},
		// Bottom face (-Y)
		{-1, -1, -1}, { 1, -1, -1}, { 1, -1,  1}, {-1, -1,  1},
		// Right face (+X)
		{ 1, -1,  1}, { 1, -1, -1}, { 1,  1, -1}, { 1,  1,  1},
		// Left face (-X)
		{-1, -1, -1}, {-1, -1,  1}, {-1,  1,  1}, {-1,  1, -1}
	};

	const Zenith_Maths::Vector3 normals[6] = {
		{ 0,  0,  1},  // Front
		{ 0,  0, -1},  // Back
		{ 0,  1,  0},  // Top
		{ 0, -1,  0},  // Bottom
		{ 1,  0,  0},  // Right
		{-1,  0,  0}   // Left
	};

	for (u_int face = 0; face < 6; ++face)
	{
		for (u_int vert = 0; vert < 4; ++vert)
		{
			PrimitiveVertex xVertex;
			xVertex.m_xPosition = positions[face * 4 + vert];
			xVertex.m_xNormal = normals[face];
			xVertex.m_xColor = Zenith_Maths::Vector3(1, 1, 1);
			xVertices.PushBack(xVertex);
		}
	}

	// Indices (CCW winding, 6 faces * 2 triangles * 3 indices = 36)
	for (u_int face = 0; face < 6; ++face)
	{
		u_int base = face * 4;
		xIndices.PushBack(base + 0);
		xIndices.PushBack(base + 1);
		xIndices.PushBack(base + 2);

		xIndices.PushBack(base + 0);
		xIndices.PushBack(base + 2);
		xIndices.PushBack(base + 3);
	}
}

/**
 * Generate a unit capsule (height 2.0 from y=-1 to y=+1, radius 0.5, centered at origin)
 * A capsule is a cylinder with hemispherical caps
 */
static void GenerateUnitCapsule(Zenith_Vector<PrimitiveVertex>& xVertices, Zenith_Vector<u_int>& xIndices, u_int uSegments = 16)
{
	xVertices.Clear();
	xIndices.Clear();

	const float PI = 3.14159265359f;
	const float fCylinderHalfHeight = 0.5f;
	const float fRadius = 0.5f;

	// Top hemisphere (y > 0)
	for (u_int lat = 0; lat <= uSegments / 2; ++lat)
	{
		float theta = lat * PI / uSegments;  // 0 to PI/2
		float sinTheta = sinf(theta);
		float cosTheta = cosf(theta);

		for (u_int lon = 0; lon <= uSegments; ++lon)
		{
			float phi = lon * 2.0f * PI / uSegments;
			float sinPhi = sinf(phi);
			float cosPhi = cosf(phi);

			PrimitiveVertex xVertex;
			xVertex.m_xPosition.x = fRadius * sinTheta * cosPhi;
			xVertex.m_xPosition.y = fCylinderHalfHeight + fRadius * cosTheta;
			xVertex.m_xPosition.z = fRadius * sinTheta * sinPhi;
			xVertex.m_xNormal = Zenith_Maths::Normalize(Zenith_Maths::Vector3(sinTheta * cosPhi, cosTheta, sinTheta * sinPhi));
			xVertex.m_xColor = Zenith_Maths::Vector3(1, 1, 1);
			xVertices.PushBack(xVertex);
		}
	}

	u_int uTopHemisphereVertCount = xVertices.GetSize();

	// Bottom hemisphere (y < 0)
	for (u_int lat = uSegments / 2; lat <= uSegments; ++lat)
	{
		float theta = lat * PI / uSegments;  // PI/2 to PI
		float sinTheta = sinf(theta);
		float cosTheta = cosf(theta);

		for (u_int lon = 0; lon <= uSegments; ++lon)
		{
			float phi = lon * 2.0f * PI / uSegments;
			float sinPhi = sinf(phi);
			float cosPhi = cosf(phi);

			PrimitiveVertex xVertex;
			xVertex.m_xPosition.x = fRadius * sinTheta * cosPhi;
			xVertex.m_xPosition.y = -fCylinderHalfHeight + fRadius * cosTheta;
			xVertex.m_xPosition.z = fRadius * sinTheta * sinPhi;
			xVertex.m_xNormal = Zenith_Maths::Normalize(Zenith_Maths::Vector3(sinTheta * cosPhi, cosTheta, sinTheta * sinPhi));
			xVertex.m_xColor = Zenith_Maths::Vector3(1, 1, 1);
			xVertices.PushBack(xVertex);
		}
	}

	// Generate indices for hemispheres
	// Top hemisphere
	for (u_int lat = 0; lat < uSegments / 2; ++lat)
	{
		for (u_int lon = 0; lon < uSegments; ++lon)
		{
			u_int current = lat * (uSegments + 1) + lon;
			u_int next = current + uSegments + 1;

			xIndices.PushBack(current);
			xIndices.PushBack(next);
			xIndices.PushBack(current + 1);

			xIndices.PushBack(current + 1);
			xIndices.PushBack(next);
			xIndices.PushBack(next + 1);
		}
	}

	// Bottom hemisphere (offset indices)
	for (u_int lat = 0; lat < uSegments / 2; ++lat)
	{
		for (u_int lon = 0; lon < uSegments; ++lon)
		{
			u_int current = uTopHemisphereVertCount + lat * (uSegments + 1) + lon;
			u_int next = current + uSegments + 1;

			xIndices.PushBack(current);
			xIndices.PushBack(next);
			xIndices.PushBack(current + 1);

			xIndices.PushBack(current + 1);
			xIndices.PushBack(next);
			xIndices.PushBack(next + 1);
		}
	}
}

/**
 * Generate a unit cylinder (height 2.0 from y=-1 to y=+1, radius 0.5, centered at origin)
 */
static void GenerateUnitCylinder(Zenith_Vector<PrimitiveVertex>& xVertices, Zenith_Vector<u_int>& xIndices, u_int uSegments = 32)
{
	xVertices.Clear();
	xIndices.Clear();

	const float PI = 3.14159265359f;
	const float fRadius = 0.5f;
	const float fHalfHeight = 1.0f;

	// Side vertices (two rings, top and bottom)
	for (u_int i = 0; i <= uSegments; ++i)
	{
		float angle = i * 2.0f * PI / uSegments;
		float cosAngle = cosf(angle);
		float sinAngle = sinf(angle);

		Zenith_Maths::Vector3 normal(cosAngle, 0.0f, sinAngle);

		// Bottom vertex
		PrimitiveVertex xBottomVert;
		xBottomVert.m_xPosition = Zenith_Maths::Vector3(fRadius * cosAngle, -fHalfHeight, fRadius * sinAngle);
		xBottomVert.m_xNormal = normal;
		xBottomVert.m_xColor = Zenith_Maths::Vector3(1, 1, 1);
		xVertices.PushBack(xBottomVert);

		// Top vertex
		PrimitiveVertex xTopVert;
		xTopVert.m_xPosition = Zenith_Maths::Vector3(fRadius * cosAngle, fHalfHeight, fRadius * sinAngle);
		xTopVert.m_xNormal = normal;
		xTopVert.m_xColor = Zenith_Maths::Vector3(1, 1, 1);
		xVertices.PushBack(xTopVert);
	}

	// Side indices
	for (u_int i = 0; i < uSegments; ++i)
	{
		u_int bottom1 = i * 2;
		u_int top1 = i * 2 + 1;
		u_int bottom2 = (i + 1) * 2;
		u_int top2 = (i + 1) * 2 + 1;

		// Triangle 1
		xIndices.PushBack(bottom1);
		xIndices.PushBack(bottom2);
		xIndices.PushBack(top1);

		// Triangle 2
		xIndices.PushBack(top1);
		xIndices.PushBack(bottom2);
		xIndices.PushBack(top2);
	}

	// Cap vertices and indices (optional, simpler without caps for debug visuals)
	// For now, leaving caps out to reduce complexity
}

/**
 * Generate a line segment mesh (essentially a thin quad/billboard facing camera)
 * For simplicity, we'll use a unit line from (0, -1, 0) to (0, 1, 0) and transform it
 */
static void GenerateUnitLine(Zenith_Vector<PrimitiveVertex>& xVertices, Zenith_Vector<u_int>& xIndices)
{
	xVertices.Clear();
	xIndices.Clear();

	// A line is represented as a thin box (4 vertices forming a quad)
	// Unit line goes from (0, -1, 0) to (0, 1, 0) with thickness along X and Z
	const float fHalfThickness = 0.5f;

	PrimitiveVertex v0, v1, v2, v3;
	v0.m_xPosition = Zenith_Maths::Vector3(-fHalfThickness, -1.0f, 0.0f);
	v0.m_xNormal = Zenith_Maths::Vector3(0, 0, 1);
	v0.m_xColor = Zenith_Maths::Vector3(1, 1, 1);

	v1.m_xPosition = Zenith_Maths::Vector3(fHalfThickness, -1.0f, 0.0f);
	v1.m_xNormal = Zenith_Maths::Vector3(0, 0, 1);
	v1.m_xColor = Zenith_Maths::Vector3(1, 1, 1);

	v2.m_xPosition = Zenith_Maths::Vector3(fHalfThickness, 1.0f, 0.0f);
	v2.m_xNormal = Zenith_Maths::Vector3(0, 0, 1);
	v2.m_xColor = Zenith_Maths::Vector3(1, 1, 1);

	v3.m_xPosition = Zenith_Maths::Vector3(-fHalfThickness, 1.0f, 0.0f);
	v3.m_xNormal = Zenith_Maths::Vector3(0, 0, 1);
	v3.m_xColor = Zenith_Maths::Vector3(1, 1, 1);

	xVertices.PushBack(v0);
	xVertices.PushBack(v1);
	xVertices.PushBack(v2);
	xVertices.PushBack(v3);

	// Two triangles
	xIndices.PushBack(0);
	xIndices.PushBack(1);
	xIndices.PushBack(2);

	xIndices.PushBack(0);
	xIndices.PushBack(2);
	xIndices.PushBack(3);
}

// ========== PUBLIC API ==========

void Flux_Primitives::Initialise()
{
	// Generate procedural meshes
	Zenith_Vector<PrimitiveVertex> xVertices;
	Zenith_Vector<u_int> xIndices;

	// Unit sphere
	GenerateUnitSphere(xVertices, xIndices);
	s_uSphereIndexCount = xIndices.GetSize();
	Flux_MemoryManager::InitialiseVertexBuffer(xVertices.GetDataPointer(), xVertices.GetSize() * sizeof(PrimitiveVertex), s_xSphereVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(xIndices.GetDataPointer(), xIndices.GetSize() * sizeof(u_int), s_xSphereIndexBuffer);

	// Unit cube
	GenerateUnitCube(xVertices, xIndices);
	s_uCubeIndexCount = xIndices.GetSize();
	Flux_MemoryManager::InitialiseVertexBuffer(xVertices.GetDataPointer(), xVertices.GetSize() * sizeof(PrimitiveVertex), s_xCubeVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(xIndices.GetDataPointer(), xIndices.GetSize() * sizeof(u_int), s_xCubeIndexBuffer);

	// Unit capsule
	GenerateUnitCapsule(xVertices, xIndices);
	s_uCapsuleIndexCount = xIndices.GetSize();
	Flux_MemoryManager::InitialiseVertexBuffer(xVertices.GetDataPointer(), xVertices.GetSize() * sizeof(PrimitiveVertex), s_xCapsuleVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(xIndices.GetDataPointer(), xIndices.GetSize() * sizeof(u_int), s_xCapsuleIndexBuffer);

	// Unit cylinder
	GenerateUnitCylinder(xVertices, xIndices);
	s_uCylinderIndexCount = xIndices.GetSize();
	Flux_MemoryManager::InitialiseVertexBuffer(xVertices.GetDataPointer(), xVertices.GetSize() * sizeof(PrimitiveVertex), s_xCylinderVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(xIndices.GetDataPointer(), xIndices.GetSize() * sizeof(u_int), s_xCylinderIndexBuffer);

	// Unit line
	GenerateUnitLine(xVertices, xIndices);
	s_uLineIndexCount = xIndices.GetSize();
	Flux_MemoryManager::InitialiseVertexBuffer(xVertices.GetDataPointer(), xVertices.GetSize() * sizeof(PrimitiveVertex), s_xLineVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(xIndices.GetDataPointer(), xIndices.GetSize() * sizeof(u_int), s_xLineIndexBuffer);

	// Load shaders
	s_xPrimitivesShader.Initialise("Primitives/Flux_Primitives.vert", "Primitives/Flux_Primitives.frag");

	// Define vertex layout (Position, Normal, Color)
	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);  // Position
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);  // Normal
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);  // Color (unused, from push constant)
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	// Build GBuffer pipeline (solid shading)
	{
		Flux_PipelineSpecification xPipelineSpec;
		xPipelineSpec.m_pxTargetSetup = &Flux_Graphics::s_xMRTTarget;  // Render to GBuffer
		xPipelineSpec.m_pxShader = &s_xPrimitivesShader;
		xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		Flux_PipelineLayout& xLayout = xPipelineSpec.m_xPipelineLayout;
		xLayout.m_uNumDescriptorSets = 1;
		xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Frame constants

		// Standard depth testing for opaque geometry
		xPipelineSpec.m_bDepthTestEnabled = true;
		xPipelineSpec.m_bDepthWriteEnabled = true;
		xPipelineSpec.m_eDepthCompareFunc = DEPTH_COMPARE_FUNC_LESSEQUAL;

		// Blending disabled (opaque)
		for (Flux_BlendState& xBlendState : xPipelineSpec.m_axBlendStates)
		{
			xBlendState.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
			xBlendState.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
			xBlendState.m_bBlendEnabled = false;
		}

		Flux_PipelineBuilder::FromSpecification(s_xPrimitivesPipeline, xPipelineSpec);

		// Wireframe variant
		xPipelineSpec.m_bWireframe = true;
		Flux_PipelineBuilder::FromSpecification(s_xPrimitivesWireframePipeline, xPipelineSpec);
	}

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Primitives" }, dbg_bEnablePrimitives);
#endif

	Zenith_Log("Flux_Primitives initialised");
}

void Flux_Primitives::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_Primitives::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

void Flux_Primitives::AddSphere(const Zenith_Maths::Vector3& xCenter, float fRadius, const Zenith_Maths::Vector3& xColor)
{
	g_xInstanceMutex.Lock();
	SphereInstance xInstance;
	xInstance.m_xCenter = xCenter;
	xInstance.m_fRadius = fRadius;
	xInstance.m_xColor = xColor;
	g_xSphereInstances.PushBack(xInstance);
	g_xInstanceMutex.Unlock();
}

void Flux_Primitives::AddCube(const Zenith_Maths::Vector3& xCenter, const Zenith_Maths::Vector3& xHalfExtents, const Zenith_Maths::Vector3& xColor)
{
	g_xInstanceMutex.Lock();
	CubeInstance xInstance;
	xInstance.m_xCenter = xCenter;
	xInstance.m_xHalfExtents = xHalfExtents;
	xInstance.m_xColor = xColor;
	xInstance.m_bWireframe = false;
	g_xCubeInstances.PushBack(xInstance);
	g_xInstanceMutex.Unlock();
}

void Flux_Primitives::AddWireframeCube(const Zenith_Maths::Vector3& xCenter, const Zenith_Maths::Vector3& xHalfExtents, const Zenith_Maths::Vector3& xColor)
{
	g_xInstanceMutex.Lock();
	CubeInstance xInstance;
	xInstance.m_xCenter = xCenter;
	xInstance.m_xHalfExtents = xHalfExtents;
	xInstance.m_xColor = xColor;
	xInstance.m_bWireframe = true;
	g_xCubeInstances.PushBack(xInstance);
	g_xInstanceMutex.Unlock();
}

void Flux_Primitives::AddLine(const Zenith_Maths::Vector3& xStart, const Zenith_Maths::Vector3& xEnd, const Zenith_Maths::Vector3& xColor, float fThickness)
{
	g_xInstanceMutex.Lock();
	LineInstance xInstance;
	xInstance.m_xStart = xStart;
	xInstance.m_xEnd = xEnd;
	xInstance.m_xColor = xColor;
	xInstance.m_fThickness = fThickness;
	g_xLineInstances.PushBack(xInstance);
	g_xInstanceMutex.Unlock();
}

void Flux_Primitives::AddCapsule(const Zenith_Maths::Vector3& xStart, const Zenith_Maths::Vector3& xEnd, float fRadius, const Zenith_Maths::Vector3& xColor)
{
	g_xInstanceMutex.Lock();
	CapsuleInstance xInstance;
	xInstance.m_xStart = xStart;
	xInstance.m_xEnd = xEnd;
	xInstance.m_fRadius = fRadius;
	xInstance.m_xColor = xColor;
	g_xCapsuleInstances.PushBack(xInstance);
	g_xInstanceMutex.Unlock();
}

void Flux_Primitives::AddCylinder(const Zenith_Maths::Vector3& xStart, const Zenith_Maths::Vector3& xEnd, float fRadius, const Zenith_Maths::Vector3& xColor)
{
	g_xInstanceMutex.Lock();
	CylinderInstance xInstance;
	xInstance.m_xStart = xStart;
	xInstance.m_xEnd = xEnd;
	xInstance.m_fRadius = fRadius;
	xInstance.m_xColor = xColor;
	g_xCylinderInstances.PushBack(xInstance);
	g_xInstanceMutex.Unlock();
}

void Flux_Primitives::Clear()
{
	g_xInstanceMutex.Lock();
	g_xSphereInstances.Clear();
	g_xCubeInstances.Clear();
	g_xLineInstances.Clear();
	g_xCapsuleInstances.Clear();
	g_xCylinderInstances.Clear();
	g_xInstanceMutex.Unlock();
}

// ========== RENDERING ==========

void Flux_Primitives::Render(void*)
{
	if (!dbg_bEnablePrimitives)
	{
		return;
	}

	// Early-out if no primitives queued
	if (g_xSphereInstances.GetSize() == 0 &&
		g_xCubeInstances.GetSize() == 0 &&
		g_xLineInstances.GetSize() == 0 &&
		g_xCapsuleInstances.GetSize() == 0 &&
		g_xCylinderInstances.GetSize() == 0)
	{
		return;
	}

	g_xCommandList.Reset(false);  // Don't clear GBuffer targets (other geometry already rendered)

	// Bind frame constants (set 0, binding 0)
	g_xCommandList.AddCommand<Flux_CommandBeginBind>(0);
	g_xCommandList.AddCommand<Flux_CommandBindCBV>(&Flux_Graphics::s_xFrameConstantsBuffer.GetCBV(), 0);

	// ========== RENDER SPHERES ==========
	if (g_xSphereInstances.GetSize() > 0)
	{
		g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPrimitivesPipeline);
		g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&s_xSphereVertexBuffer);
		g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&s_xSphereIndexBuffer);

		for (u_int i = 0; i < g_xSphereInstances.GetSize(); ++i)
		{
			const SphereInstance& xInstance = g_xSphereInstances.Get(i);

			// Build model matrix: translate to center, scale by radius
			Zenith_Maths::Matrix4 xModelMatrix = Zenith_Maths::Translate(Zenith_Maths::Matrix4(1.0f), xInstance.m_xCenter);
			xModelMatrix = Zenith_Maths::Scale(xModelMatrix, Zenith_Maths::Vector3(xInstance.m_fRadius));

			PrimitivePushConstant xPushConstant;
			xPushConstant.m_xModelMatrix = xModelMatrix;
			xPushConstant.m_xColor = xInstance.m_xColor;
			xPushConstant.m_fPadding = 0.0f;

			g_xCommandList.AddCommand<Flux_CommandPushConstant>(&xPushConstant, sizeof(PrimitivePushConstant));
			g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(s_uSphereIndexCount);
		}
	}

	// ========== RENDER CUBES ==========
	if (g_xCubeInstances.GetSize() > 0)
	{
		for (u_int i = 0; i < g_xCubeInstances.GetSize(); ++i)
		{
			const CubeInstance& xInstance = g_xCubeInstances.Get(i);

			// Set pipeline based on wireframe flag
			if (xInstance.m_bWireframe)
			{
				g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPrimitivesWireframePipeline);
			}
			else
			{
				g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPrimitivesPipeline);
			}

			g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&s_xCubeVertexBuffer);
			g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&s_xCubeIndexBuffer);

			// Build model matrix: translate to center, scale by half extents
			Zenith_Maths::Matrix4 xModelMatrix = Zenith_Maths::Translate(Zenith_Maths::Matrix4(1.0f), xInstance.m_xCenter);
			xModelMatrix = Zenith_Maths::Scale(xModelMatrix, xInstance.m_xHalfExtents);

			PrimitivePushConstant xPushConstant;
			xPushConstant.m_xModelMatrix = xModelMatrix;
			xPushConstant.m_xColor = xInstance.m_xColor;
			xPushConstant.m_fPadding = 0.0f;

			g_xCommandList.AddCommand<Flux_CommandPushConstant>(&xPushConstant, sizeof(PrimitivePushConstant));
			g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(s_uCubeIndexCount);
		}
	}

	// ========== RENDER LINES ==========
	if (g_xLineInstances.GetSize() > 0)
	{
		g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPrimitivesPipeline);
		g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&s_xLineVertexBuffer);
		g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&s_xLineIndexBuffer);

		for (u_int i = 0; i < g_xLineInstances.GetSize(); ++i)
		{
			const LineInstance& xInstance = g_xLineInstances.Get(i);

			// Build model matrix to transform unit line (0, -1, 0) -> (0, 1, 0) to start -> end
			Zenith_Maths::Vector3 xDirection = xInstance.m_xEnd - xInstance.m_xStart;
			float fLength = Zenith_Maths::Length(xDirection);
			Zenith_Maths::Vector3 xNormalizedDir = Zenith_Maths::Normalize(xDirection);

			// Compute rotation to align (0, 1, 0) with xNormalizedDir
			Zenith_Maths::Vector3 xUp(0, 1, 0);
			Zenith_Maths::Quaternion xRotation;
			if (fabsf(Zenith_Maths::Dot(xUp, xNormalizedDir)) < 0.9999f)
			{
				Zenith_Maths::Vector3 xAxis = Zenith_Maths::Normalize(Zenith_Maths::Cross(xUp, xNormalizedDir));
				float fAngle = acosf(Zenith_Maths::Dot(xUp, xNormalizedDir));
				xRotation = Zenith_Maths::AngleAxis(fAngle, xAxis);
			}
			else
			{
				xRotation = Zenith_Maths::Quaternion(1, 0, 0, 0);  // Identity
			}

			Zenith_Maths::Matrix4 xModelMatrix = Zenith_Maths::Translate(Zenith_Maths::Matrix4(1.0f), xInstance.m_xStart);
			xModelMatrix = xModelMatrix * Zenith_Maths::Mat4Cast(xRotation);
			xModelMatrix = Zenith_Maths::Scale(xModelMatrix, Zenith_Maths::Vector3(xInstance.m_fThickness, fLength * 0.5f, xInstance.m_fThickness));

			PrimitivePushConstant xPushConstant;
			xPushConstant.m_xModelMatrix = xModelMatrix;
			xPushConstant.m_xColor = xInstance.m_xColor;
			xPushConstant.m_fPadding = 0.0f;

			g_xCommandList.AddCommand<Flux_CommandPushConstant>(&xPushConstant, sizeof(PrimitivePushConstant));
			g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(s_uLineIndexCount);
		}
	}

	// ========== RENDER CAPSULES ==========
	if (g_xCapsuleInstances.GetSize() > 0)
	{
		g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPrimitivesPipeline);
		g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&s_xCapsuleVertexBuffer);
		g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&s_xCapsuleIndexBuffer);

		for (u_int i = 0; i < g_xCapsuleInstances.GetSize(); ++i)
		{
			const CapsuleInstance& xInstance = g_xCapsuleInstances.Get(i);

			// Build model matrix to align unit capsule (Y-axis) with start->end
			Zenith_Maths::Vector3 xDirection = xInstance.m_xEnd - xInstance.m_xStart;
			float fLength = Zenith_Maths::Length(xDirection);
			Zenith_Maths::Vector3 xNormalizedDir = Zenith_Maths::Normalize(xDirection);

			Zenith_Maths::Vector3 xUp(0, 1, 0);
			Zenith_Maths::Quaternion xRotation;
			if (fabsf(Zenith_Maths::Dot(xUp, xNormalizedDir)) < 0.9999f)
			{
				Zenith_Maths::Vector3 xAxis = Zenith_Maths::Normalize(Zenith_Maths::Cross(xUp, xNormalizedDir));
				float fAngle = acosf(Zenith_Maths::Dot(xUp, xNormalizedDir));
				xRotation = Zenith_Maths::AngleAxis(fAngle, xAxis);
			}
			else
			{
				xRotation = Zenith_Maths::Quaternion(1, 0, 0, 0);
			}

			Zenith_Maths::Vector3 xCenter = (xInstance.m_xStart + xInstance.m_xEnd) * 0.5f;
			Zenith_Maths::Matrix4 xModelMatrix = Zenith_Maths::Translate(Zenith_Maths::Matrix4(1.0f), xCenter);
			xModelMatrix = xModelMatrix * Zenith_Maths::Mat4Cast(xRotation);
			xModelMatrix = Zenith_Maths::Scale(xModelMatrix, Zenith_Maths::Vector3(xInstance.m_fRadius * 2.0f, fLength * 0.5f, xInstance.m_fRadius * 2.0f));

			PrimitivePushConstant xPushConstant;
			xPushConstant.m_xModelMatrix = xModelMatrix;
			xPushConstant.m_xColor = xInstance.m_xColor;
			xPushConstant.m_fPadding = 0.0f;

			g_xCommandList.AddCommand<Flux_CommandPushConstant>(&xPushConstant, sizeof(PrimitivePushConstant));
			g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(s_uCapsuleIndexCount);
		}
	}

	// ========== RENDER CYLINDERS ==========
	if (g_xCylinderInstances.GetSize() > 0)
	{
		g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPrimitivesPipeline);
		g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&s_xCylinderVertexBuffer);
		g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&s_xCylinderIndexBuffer);

		for (u_int i = 0; i < g_xCylinderInstances.GetSize(); ++i)
		{
			const CylinderInstance& xInstance = g_xCylinderInstances.Get(i);

			// Build model matrix to align unit cylinder (Y-axis) with start->end
			Zenith_Maths::Vector3 xDirection = xInstance.m_xEnd - xInstance.m_xStart;
			float fLength = Zenith_Maths::Length(xDirection);
			Zenith_Maths::Vector3 xNormalizedDir = Zenith_Maths::Normalize(xDirection);

			Zenith_Maths::Vector3 xUp(0, 1, 0);
			Zenith_Maths::Quaternion xRotation;
			if (fabsf(Zenith_Maths::Dot(xUp, xNormalizedDir)) < 0.9999f)
			{
				Zenith_Maths::Vector3 xAxis = Zenith_Maths::Normalize(Zenith_Maths::Cross(xUp, xNormalizedDir));
				float fAngle = acosf(Zenith_Maths::Dot(xUp, xNormalizedDir));
				xRotation = Zenith_Maths::AngleAxis(fAngle, xAxis);
			}
			else
			{
				xRotation = Zenith_Maths::Quaternion(1, 0, 0, 0);
			}

			Zenith_Maths::Vector3 xCenter = (xInstance.m_xStart + xInstance.m_xEnd) * 0.5f;
			Zenith_Maths::Matrix4 xModelMatrix = Zenith_Maths::Translate(Zenith_Maths::Matrix4(1.0f), xCenter);
			xModelMatrix = xModelMatrix * Zenith_Maths::Mat4Cast(xRotation);
			xModelMatrix = Zenith_Maths::Scale(xModelMatrix, Zenith_Maths::Vector3(xInstance.m_fRadius * 2.0f, fLength * 0.5f, xInstance.m_fRadius * 2.0f));

			PrimitivePushConstant xPushConstant;
			xPushConstant.m_xModelMatrix = xModelMatrix;
			xPushConstant.m_xColor = xInstance.m_xColor;
			xPushConstant.m_fPadding = 0.0f;

			g_xCommandList.AddCommand<Flux_CommandPushConstant>(&xPushConstant, sizeof(PrimitivePushConstant));
			g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(s_uCylinderIndexCount);
		}
	}

	// Submit command list to GBuffer target at RENDER_ORDER_PRIMITIVES
	Flux::SubmitCommandList(&g_xCommandList, Flux_Graphics::s_xMRTTarget, RENDER_ORDER_PRIMITIVES);

	// Clear instances for next frame
	Clear();
}
