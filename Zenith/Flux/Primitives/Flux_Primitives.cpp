#include "Zenith.h"

#include "Flux/Primitives/Flux_Primitives.h"

#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
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

// Cached binding handles from shader reflection
static Flux_BindingHandle s_xFrameConstantsBinding;

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

struct TriangleInstance
{
	Zenith_Maths::Vector3 m_xV0;
	Zenith_Maths::Vector3 m_xV1;
	Zenith_Maths::Vector3 m_xV2;
	Zenith_Maths::Vector3 m_xColor;
};

// Per-frame instance queues
static Zenith_Vector<SphereInstance> g_xSphereInstances;
static Zenith_Vector<CubeInstance> g_xCubeInstances;
static Zenith_Vector<LineInstance> g_xLineInstances;
static Zenith_Vector<CapsuleInstance> g_xCapsuleInstances;
static Zenith_Vector<CylinderInstance> g_xCylinderInstances;
static Zenith_Vector<TriangleInstance> g_xTriangleInstances;

// Dynamic buffers for triangles (reused each frame, data uploaded rather than recreated)
// This avoids recreating GPU buffers every frame which causes memory leaks
static Flux_DynamicVertexBuffer s_xTriangleDynamicVertexBuffer;
static Flux_IndexBuffer s_xTriangleIndexBuffer;
static bool s_bTriangleBuffersInitialised = false;
static constexpr u_int s_uMaxTriangles = 8192;  // Max triangles per frame

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
		xLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Scratch buffer for push constants

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

	// Cache binding handles from shader reflection
	const Flux_ShaderReflection& xReflection = s_xPrimitivesShader.GetReflection();
	s_xFrameConstantsBinding = xReflection.GetBinding("FrameConstants");

	// Pre-allocate triangle buffers (dynamic vertex buffer, static index buffer)
	// This avoids recreating GPU buffers every frame which causes memory leaks
	{
		const size_t uVertexBufferSize = s_uMaxTriangles * 3 * sizeof(PrimitiveVertex);
		const size_t uIndexBufferSize = s_uMaxTriangles * 3 * sizeof(u_int);

		Flux_MemoryManager::InitialiseDynamicVertexBuffer(nullptr, uVertexBufferSize, s_xTriangleDynamicVertexBuffer, false);
		Flux_MemoryManager::InitialiseIndexBuffer(nullptr, uIndexBufferSize, s_xTriangleIndexBuffer);
		s_bTriangleBuffersInitialised = true;
	}

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Primitives" }, dbg_bEnablePrimitives);
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Primitives initialised");
}

void Flux_Primitives::Reset()
{
	// Reset command list to ensure no stale GPU resource references, including descriptor bindings
	// This is called when the scene is reset (e.g., Play/Stop transitions in editor)
	g_xCommandList.Reset(true);
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Primitives::Reset() - Reset command list");
}

void Flux_Primitives::Shutdown()
{
	// Destroy all vertex and index buffers
	Flux_MemoryManager::DestroyVertexBuffer(s_xSphereVertexBuffer);
	Flux_MemoryManager::DestroyIndexBuffer(s_xSphereIndexBuffer);

	Flux_MemoryManager::DestroyVertexBuffer(s_xCubeVertexBuffer);
	Flux_MemoryManager::DestroyIndexBuffer(s_xCubeIndexBuffer);

	Flux_MemoryManager::DestroyVertexBuffer(s_xCapsuleVertexBuffer);
	Flux_MemoryManager::DestroyIndexBuffer(s_xCapsuleIndexBuffer);

	Flux_MemoryManager::DestroyVertexBuffer(s_xCylinderVertexBuffer);
	Flux_MemoryManager::DestroyIndexBuffer(s_xCylinderIndexBuffer);

	Flux_MemoryManager::DestroyVertexBuffer(s_xLineVertexBuffer);
	Flux_MemoryManager::DestroyIndexBuffer(s_xLineIndexBuffer);

	// Destroy pre-allocated triangle buffers
	if (s_bTriangleBuffersInitialised)
	{
		Flux_MemoryManager::DestroyDynamicVertexBuffer(s_xTriangleDynamicVertexBuffer);
		Flux_MemoryManager::DestroyIndexBuffer(s_xTriangleIndexBuffer);
		s_bTriangleBuffersInitialised = false;
	}

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Primitives shut down");
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

void Flux_Primitives::AddTriangle(const Zenith_Maths::Vector3& xV0, const Zenith_Maths::Vector3& xV1,
	const Zenith_Maths::Vector3& xV2, const Zenith_Maths::Vector3& xColor)
{
	g_xInstanceMutex.Lock();
	TriangleInstance xInstance;
	xInstance.m_xV0 = xV0;
	xInstance.m_xV1 = xV1;
	xInstance.m_xV2 = xV2;
	xInstance.m_xColor = xColor;
	g_xTriangleInstances.PushBack(xInstance);
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
	g_xTriangleInstances.Clear();
	g_xInstanceMutex.Unlock();
}

// ========== RENDERING ==========

void Flux_Primitives::Render(void*)
{
	if (!dbg_bEnablePrimitives)
	{
		return;
	}

	// Make local copies of instance data under lock to avoid data race
	// (Add*() functions can be called from other threads while Render() iterates)
	Zenith_Vector<SphereInstance> xLocalSphereInstances;
	Zenith_Vector<CubeInstance> xLocalCubeInstances;
	Zenith_Vector<LineInstance> xLocalLineInstances;
	Zenith_Vector<CapsuleInstance> xLocalCapsuleInstances;
	Zenith_Vector<CylinderInstance> xLocalCylinderInstances;
	Zenith_Vector<TriangleInstance> xLocalTriangleInstances;

	{
		Zenith_ScopedMutexLock xLock(g_xInstanceMutex);

		// Early-out if no primitives queued (check under lock)
		if (g_xSphereInstances.GetSize() == 0 &&
			g_xCubeInstances.GetSize() == 0 &&
			g_xLineInstances.GetSize() == 0 &&
			g_xCapsuleInstances.GetSize() == 0 &&
			g_xCylinderInstances.GetSize() == 0 &&
			g_xTriangleInstances.GetSize() == 0)
		{
			return;
		}

		// Copy all instance data - this is fast for typical counts
		for (u_int i = 0; i < g_xSphereInstances.GetSize(); ++i)
			xLocalSphereInstances.PushBack(g_xSphereInstances.Get(i));
		for (u_int i = 0; i < g_xCubeInstances.GetSize(); ++i)
			xLocalCubeInstances.PushBack(g_xCubeInstances.Get(i));
		for (u_int i = 0; i < g_xLineInstances.GetSize(); ++i)
			xLocalLineInstances.PushBack(g_xLineInstances.Get(i));
		for (u_int i = 0; i < g_xCapsuleInstances.GetSize(); ++i)
			xLocalCapsuleInstances.PushBack(g_xCapsuleInstances.Get(i));
		for (u_int i = 0; i < g_xCylinderInstances.GetSize(); ++i)
			xLocalCylinderInstances.PushBack(g_xCylinderInstances.Get(i));
		for (u_int i = 0; i < g_xTriangleInstances.GetSize(); ++i)
			xLocalTriangleInstances.PushBack(g_xTriangleInstances.Get(i));

		// Clear global instances now that we have local copies
		g_xSphereInstances.Clear();
		g_xCubeInstances.Clear();
		g_xLineInstances.Clear();
		g_xCapsuleInstances.Clear();
		g_xCylinderInstances.Clear();
		g_xTriangleInstances.Clear();
	}

	g_xCommandList.Reset(false);  // Don't clear GBuffer targets (other geometry already rendered)

	// Create binder and bind frame constants (same for all primitives)
	Flux_ShaderBinder xBinder(g_xCommandList);
	xBinder.BindCBV(s_xFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());

	// ========== RENDER SPHERES ==========
	if (xLocalSphereInstances.GetSize() > 0)
	{
		g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPrimitivesPipeline);
		g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&s_xSphereVertexBuffer);
		g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&s_xSphereIndexBuffer);

		for (u_int i = 0; i < xLocalSphereInstances.GetSize(); ++i)
		{
			const SphereInstance& xInstance = xLocalSphereInstances.Get(i);

			// Build model matrix: translate to center, scale by radius
			Zenith_Maths::Matrix4 xModelMatrix = Zenith_Maths::Translate(Zenith_Maths::Matrix4(1.0f), xInstance.m_xCenter);
			xModelMatrix = Zenith_Maths::Scale(xModelMatrix, Zenith_Maths::Vector3(xInstance.m_fRadius));

			PrimitivePushConstant xPushConstant;
			xPushConstant.m_xModelMatrix = xModelMatrix;
			xPushConstant.m_xColor = xInstance.m_xColor;
			xPushConstant.m_fPadding = 0.0f;

			xBinder.PushConstant(&xPushConstant, sizeof(PrimitivePushConstant));
			g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(s_uSphereIndexCount);
		}
	}

	// ========== RENDER CUBES ==========
	if (xLocalCubeInstances.GetSize() > 0)
	{
		for (u_int i = 0; i < xLocalCubeInstances.GetSize(); ++i)
		{
			const CubeInstance& xInstance = xLocalCubeInstances.Get(i);

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

			xBinder.PushConstant(&xPushConstant, sizeof(PrimitivePushConstant));
			g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(s_uCubeIndexCount);
		}
	}

	// ========== RENDER LINES ==========
	if (xLocalLineInstances.GetSize() > 0)
	{
		g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPrimitivesPipeline);
		g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&s_xLineVertexBuffer);
		g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&s_xLineIndexBuffer);

		for (u_int i = 0; i < xLocalLineInstances.GetSize(); ++i)
		{
			const LineInstance& xInstance = xLocalLineInstances.Get(i);

			// Build model matrix to transform unit line (0, -1, 0) -> (0, 1, 0) to start -> end
			Zenith_Maths::Vector3 xDirection = xInstance.m_xEnd - xInstance.m_xStart;
			float fLength = Zenith_Maths::Length(xDirection);

			// Skip degenerate lines (zero length) to prevent NaN from normalization
			constexpr float fMinLength = 1e-6f;
			if (fLength < fMinLength)
			{
				continue;
			}

			Zenith_Maths::Vector3 xNormalizedDir = xDirection / fLength;  // Safe: fLength >= fMinLength

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

			xBinder.PushConstant(&xPushConstant, sizeof(PrimitivePushConstant));
			g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(s_uLineIndexCount);
		}
	}

	// ========== RENDER CAPSULES ==========
	if (xLocalCapsuleInstances.GetSize() > 0)
	{
		g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPrimitivesPipeline);
		g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&s_xCapsuleVertexBuffer);
		g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&s_xCapsuleIndexBuffer);

		for (u_int i = 0; i < xLocalCapsuleInstances.GetSize(); ++i)
		{
			const CapsuleInstance& xInstance = xLocalCapsuleInstances.Get(i);

			// Build model matrix to align unit capsule (Y-axis) with start->end
			Zenith_Maths::Vector3 xDirection = xInstance.m_xEnd - xInstance.m_xStart;
			float fLength = Zenith_Maths::Length(xDirection);

			// Skip degenerate capsules (zero length) to prevent NaN from normalization
			constexpr float fMinLength = 1e-6f;
			if (fLength < fMinLength)
			{
				continue;
			}

			Zenith_Maths::Vector3 xNormalizedDir = xDirection / fLength;  // Safe: fLength >= fMinLength

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

			xBinder.PushConstant(&xPushConstant, sizeof(PrimitivePushConstant));
			g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(s_uCapsuleIndexCount);
		}
	}

	// ========== RENDER CYLINDERS ==========
	if (xLocalCylinderInstances.GetSize() > 0)
	{
		g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPrimitivesPipeline);
		g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&s_xCylinderVertexBuffer);
		g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&s_xCylinderIndexBuffer);

		for (u_int i = 0; i < xLocalCylinderInstances.GetSize(); ++i)
		{
			const CylinderInstance& xInstance = xLocalCylinderInstances.Get(i);

			// Build model matrix to align unit cylinder (Y-axis) with start->end
			Zenith_Maths::Vector3 xDirection = xInstance.m_xEnd - xInstance.m_xStart;
			float fLength = Zenith_Maths::Length(xDirection);

			// Skip degenerate cylinders where start == end (would cause NaN from normalization)
			constexpr float fMinLength = 1e-6f;
			if (fLength < fMinLength)
			{
				continue;
			}
			Zenith_Maths::Vector3 xNormalizedDir = xDirection / fLength;

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

			xBinder.PushConstant(&xPushConstant, sizeof(PrimitivePushConstant));
			g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(s_uCylinderIndexCount);
		}
	}

	// ========== RENDER TRIANGLES ==========
	if (xLocalTriangleInstances.GetSize() > 0 && s_bTriangleBuffersInitialised)
	{
		// Clamp to max triangles to avoid buffer overflow
		u_int uTriangleCount = xLocalTriangleInstances.GetSize();
		if (uTriangleCount > s_uMaxTriangles)
		{
			Zenith_Log(LOG_CATEGORY_RENDERER, "Warning: Triangle count %u exceeds max %u, clamping", uTriangleCount, s_uMaxTriangles);
			uTriangleCount = s_uMaxTriangles;
		}

		// Generate vertex and index data for all triangles
		// Use stack arrays for small counts, heap for large
		const u_int uVertexCount = uTriangleCount * 3;
		const u_int uIndexCount = uTriangleCount * 3;

		Zenith_Vector<PrimitiveVertex> xTriangleVertices;
		Zenith_Vector<u_int> xTriangleIndices;
		xTriangleVertices.Reserve(uVertexCount);
		xTriangleIndices.Reserve(uIndexCount);

		for (u_int i = 0; i < uTriangleCount; ++i)
		{
			const TriangleInstance& xInstance = xLocalTriangleInstances.Get(i);

			// Calculate face normal from edges (CCW winding)
			Zenith_Maths::Vector3 xEdge1 = xInstance.m_xV1 - xInstance.m_xV0;
			Zenith_Maths::Vector3 xEdge2 = xInstance.m_xV2 - xInstance.m_xV0;
			Zenith_Maths::Vector3 xNormal = Zenith_Maths::Cross(xEdge1, xEdge2);
			float fLen = Zenith_Maths::Length(xNormal);
			if (fLen > 0.0001f)
			{
				xNormal = xNormal / fLen;
			}
			else
			{
				xNormal = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);  // Default up
			}

			// Add 3 vertices for this triangle
			u_int uBaseVertex = xTriangleVertices.GetSize();

			PrimitiveVertex xV0, xV1, xV2;
			xV0.m_xPosition = xInstance.m_xV0;
			xV0.m_xNormal = xNormal;
			xV0.m_xColor = xInstance.m_xColor;

			xV1.m_xPosition = xInstance.m_xV1;
			xV1.m_xNormal = xNormal;
			xV1.m_xColor = xInstance.m_xColor;

			xV2.m_xPosition = xInstance.m_xV2;
			xV2.m_xNormal = xNormal;
			xV2.m_xColor = xInstance.m_xColor;

			xTriangleVertices.PushBack(xV0);
			xTriangleVertices.PushBack(xV1);
			xTriangleVertices.PushBack(xV2);

			// Add indices (CCW)
			xTriangleIndices.PushBack(uBaseVertex + 0);
			xTriangleIndices.PushBack(uBaseVertex + 1);
			xTriangleIndices.PushBack(uBaseVertex + 2);
		}

		// Upload vertex and index data to pre-allocated buffers (no buffer recreation!)
		Flux_MemoryManager::UploadBufferData(
			s_xTriangleDynamicVertexBuffer.GetBuffer().m_xVRAMHandle,
			xTriangleVertices.GetDataPointer(),
			xTriangleVertices.GetSize() * sizeof(PrimitiveVertex)
		);
		Flux_MemoryManager::UploadBufferData(
			s_xTriangleIndexBuffer.GetBuffer().m_xVRAMHandle,
			xTriangleIndices.GetDataPointer(),
			xTriangleIndices.GetSize() * sizeof(u_int)
		);

		// Render all triangles with identity transform (vertices are in world space)
		g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPrimitivesPipeline);
		g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&s_xTriangleDynamicVertexBuffer);
		g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&s_xTriangleIndexBuffer);

		PrimitivePushConstant xPushConstant;
		xPushConstant.m_xModelMatrix = Zenith_Maths::Matrix4(1.0f);  // Identity
		xPushConstant.m_xColor = Zenith_Maths::Vector3(1.0f);  // Color is per-vertex
		xPushConstant.m_fPadding = 0.0f;

		xBinder.PushConstant(&xPushConstant, sizeof(PrimitivePushConstant));
		g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(xTriangleIndices.GetSize());
	}

	// Submit command list to GBuffer target at RENDER_ORDER_PRIMITIVES
	Flux::SubmitCommandList(&g_xCommandList, Flux_Graphics::s_xMRTTarget, RENDER_ORDER_PRIMITIVES);

	// Note: Instances already cleared inside the lock above
}

// ========== HELPER FUNCTIONS ==========

void Flux_Primitives::AddCross(const Zenith_Maths::Vector3& xCenter, float fSize, const Zenith_Maths::Vector3& xColor)
{
	// X axis line
	AddLine(xCenter - Zenith_Maths::Vector3(fSize, 0.0f, 0.0f),
		xCenter + Zenith_Maths::Vector3(fSize, 0.0f, 0.0f), xColor);
	// Y axis line
	AddLine(xCenter - Zenith_Maths::Vector3(0.0f, fSize, 0.0f),
		xCenter + Zenith_Maths::Vector3(0.0f, fSize, 0.0f), xColor);
	// Z axis line
	AddLine(xCenter - Zenith_Maths::Vector3(0.0f, 0.0f, fSize),
		xCenter + Zenith_Maths::Vector3(0.0f, 0.0f, fSize), xColor);
}

void Flux_Primitives::AddCircle(const Zenith_Maths::Vector3& xCenter, float fRadius, const Zenith_Maths::Vector3& xColor,
	const Zenith_Maths::Vector3& xNormal, uint32_t uSegments)
{
	const float PI = 3.14159265359f;

	// Build orthonormal basis around normal
	Zenith_Maths::Vector3 xUp = (fabsf(xNormal.y) < 0.999f)
		? Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)
		: Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xRight = Zenith_Maths::Normalize(Zenith_Maths::Cross(xUp, xNormal));
	Zenith_Maths::Vector3 xForward = Zenith_Maths::Cross(xNormal, xRight);

	Zenith_Maths::Vector3 xPrevPoint;
	for (uint32_t u = 0; u <= uSegments; ++u)
	{
		float fAngle = (static_cast<float>(u) / static_cast<float>(uSegments)) * 2.0f * PI;
		float fCos = cosf(fAngle);
		float fSin = sinf(fAngle);

		Zenith_Maths::Vector3 xPoint = xCenter + (xRight * fCos + xForward * fSin) * fRadius;

		if (u > 0)
		{
			AddLine(xPrevPoint, xPoint, xColor);
		}

		xPrevPoint = xPoint;
	}
}

void Flux_Primitives::AddArrow(const Zenith_Maths::Vector3& xStart, const Zenith_Maths::Vector3& xEnd,
	const Zenith_Maths::Vector3& xColor, float fThickness, float fHeadSize)
{
	// Main shaft
	AddLine(xStart, xEnd, xColor, fThickness);

	// Arrowhead
	Zenith_Maths::Vector3 xDirection = xEnd - xStart;
	float fLength = Zenith_Maths::Length(xDirection);
	if (fLength < 0.001f)
	{
		return;
	}

	xDirection = xDirection / fLength;

	// Build orthonormal basis
	Zenith_Maths::Vector3 xUp = (fabsf(xDirection.y) < 0.999f)
		? Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)
		: Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xRight = Zenith_Maths::Normalize(Zenith_Maths::Cross(xUp, xDirection));
	Zenith_Maths::Vector3 xRealUp = Zenith_Maths::Cross(xDirection, xRight);

	// Arrowhead lines
	float fHeadLength = fLength * fHeadSize;
	float fHeadWidth = fHeadLength * 0.5f;

	Zenith_Maths::Vector3 xHeadBase = xEnd - xDirection * fHeadLength;

	AddLine(xEnd, xHeadBase + xRight * fHeadWidth, xColor, fThickness);
	AddLine(xEnd, xHeadBase - xRight * fHeadWidth, xColor, fThickness);
	AddLine(xEnd, xHeadBase + xRealUp * fHeadWidth, xColor, fThickness);
	AddLine(xEnd, xHeadBase - xRealUp * fHeadWidth, xColor, fThickness);
}

void Flux_Primitives::AddConeOutline(const Zenith_Maths::Vector3& xApex, const Zenith_Maths::Vector3& xDirection,
	float fAngle, float fLength, const Zenith_Maths::Vector3& xColor, uint32_t uSegments)
{
	const float PI = 3.14159265359f;

	// Build orthonormal basis around direction
	Zenith_Maths::Vector3 xNormDir = Zenith_Maths::Normalize(xDirection);
	Zenith_Maths::Vector3 xUp = (fabsf(xNormDir.y) < 0.999f)
		? Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)
		: Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xRight = Zenith_Maths::Normalize(Zenith_Maths::Cross(xUp, xNormDir));
	Zenith_Maths::Vector3 xRealUp = Zenith_Maths::Cross(xNormDir, xRight);

	// Calculate cone base radius
	float fAngleRad = fAngle * PI / 180.0f;
	float fBaseRadius = fLength * tanf(fAngleRad);

	// Center of cone base
	Zenith_Maths::Vector3 xBaseCenter = xApex + xNormDir * fLength;

	// Draw lines from apex to base circle
	Zenith_Maths::Vector3 xPrevBasePoint;
	for (uint32_t u = 0; u <= uSegments; ++u)
	{
		float fSegAngle = (static_cast<float>(u) / static_cast<float>(uSegments)) * 2.0f * PI;
		float fCos = cosf(fSegAngle);
		float fSin = sinf(fSegAngle);

		Zenith_Maths::Vector3 xBasePoint = xBaseCenter + (xRight * fCos + xRealUp * fSin) * fBaseRadius;

		// Draw line from apex to this base point (only every few segments for cleaner look)
		if (u % 4 == 0 || u == uSegments)
		{
			AddLine(xApex, xBasePoint, xColor);
		}

		// Draw base circle
		if (u > 0)
		{
			AddLine(xPrevBasePoint, xBasePoint, xColor);
		}

		xPrevBasePoint = xBasePoint;
	}
}

void Flux_Primitives::AddArc(const Zenith_Maths::Vector3& xCenter, float fRadius, float fStartAngle, float fEndAngle,
	const Zenith_Maths::Vector3& xColor, const Zenith_Maths::Vector3& xNormal, uint32_t uSegments)
{
	const float PI = 3.14159265359f;

	// Build orthonormal basis around normal
	Zenith_Maths::Vector3 xUp = (fabsf(xNormal.y) < 0.999f)
		? Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)
		: Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xRight = Zenith_Maths::Normalize(Zenith_Maths::Cross(xUp, xNormal));
	Zenith_Maths::Vector3 xForward = Zenith_Maths::Cross(xNormal, xRight);

	float fStartRad = fStartAngle * PI / 180.0f;
	float fEndRad = fEndAngle * PI / 180.0f;
	float fArcLength = fEndRad - fStartRad;

	Zenith_Maths::Vector3 xPrevPoint;
	for (uint32_t u = 0; u <= uSegments; ++u)
	{
		float fT = static_cast<float>(u) / static_cast<float>(uSegments);
		float fAngle = fStartRad + fT * fArcLength;
		float fCos = cosf(fAngle);
		float fSin = sinf(fAngle);

		Zenith_Maths::Vector3 xPoint = xCenter + (xRight * fSin + xForward * fCos) * fRadius;

		if (u > 0)
		{
			AddLine(xPrevPoint, xPoint, xColor);
		}

		xPrevPoint = xPoint;
	}
}

void Flux_Primitives::AddPolygonOutline(const Zenith_Maths::Vector3* axVertices, uint32_t uVertexCount,
	const Zenith_Maths::Vector3& xColor, bool bClosed)
{
	if (uVertexCount < 2)
	{
		return;
	}

	for (uint32_t u = 1; u < uVertexCount; ++u)
	{
		AddLine(axVertices[u - 1], axVertices[u], xColor);
	}

	if (bClosed && uVertexCount > 2)
	{
		AddLine(axVertices[uVertexCount - 1], axVertices[0], xColor);
	}
}

void Flux_Primitives::AddGrid(const Zenith_Maths::Vector3& xCenter, float fSize, uint32_t uDivisions,
	const Zenith_Maths::Vector3& xColor)
{
	float fHalfSize = fSize * 0.5f;
	float fStep = fSize / static_cast<float>(uDivisions);

	// Lines along X axis
	for (uint32_t u = 0; u <= uDivisions; ++u)
	{
		float fZ = -fHalfSize + static_cast<float>(u) * fStep;
		AddLine(
			xCenter + Zenith_Maths::Vector3(-fHalfSize, 0.0f, fZ),
			xCenter + Zenith_Maths::Vector3(fHalfSize, 0.0f, fZ),
			xColor
		);
	}

	// Lines along Z axis
	for (uint32_t u = 0; u <= uDivisions; ++u)
	{
		float fX = -fHalfSize + static_cast<float>(u) * fStep;
		AddLine(
			xCenter + Zenith_Maths::Vector3(fX, 0.0f, -fHalfSize),
			xCenter + Zenith_Maths::Vector3(fX, 0.0f, fHalfSize),
			xColor
		);
	}
}

void Flux_Primitives::AddAxes(const Zenith_Maths::Vector3& xOrigin, float fSize)
{
	// X axis - Red
	AddArrow(xOrigin, xOrigin + Zenith_Maths::Vector3(fSize, 0.0f, 0.0f),
		Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));

	// Y axis - Green
	AddArrow(xOrigin, xOrigin + Zenith_Maths::Vector3(0.0f, fSize, 0.0f),
		Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));

	// Z axis - Blue
	AddArrow(xOrigin, xOrigin + Zenith_Maths::Vector3(0.0f, 0.0f, fSize),
		Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
}
