#include "Zenith.h"

#include "Flux/Primitives/Flux_PrimitivesImpl.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Flux_BackendTypes.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"

#include <cmath>

#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

// Phase 7g: subsystem state moved to Flux_PrimitivesImpl held by Zenith_Engine.
//
// Cross-subsystem deps (FluxGraphics / VulkanMemory) are reached via g_xEngine at
// point of use. The former file-static render helpers are now Flux_PrimitivesImpl
// members so their self-reaches resolve through `this`. The non-capturing
// ExecuteGBuffer / hot-reload fn-pointer trampolines below recover the singleton
// via g_xEngine.Primitives().
//
// The per-primitive instance structs are also there (named Flux_PrimitivesXxxInstance);
// these aliases preserve the original short names used throughout this file.
using SphereInstance   = Flux_PrimitivesSphereInstance;
using CubeInstance     = Flux_PrimitivesCubeInstance;
using LineInstance     = Flux_PrimitivesLineInstance;
using CapsuleInstance  = Flux_PrimitivesCapsuleInstance;
using CylinderInstance = Flux_PrimitivesCylinderInstance;
using TriangleInstance = Flux_PrimitivesTriangleInstance;

// .cpp-local vertex + push-constant types (only used inside this TU).
struct PrimitiveVertex
{
	Zenith_Maths::Vector3 m_xPosition;
	Zenith_Maths::Vector3 m_xNormal;
	Zenith_Maths::Vector3 m_xColor;
};

struct PrimitivePushConstant
{
	Zenith_Maths::Matrix4 m_xModelMatrix;
	Zenith_Maths::Vector3 m_xColor;
	float m_fPadding;
};

static constexpr u_int s_uMaxTriangles = 8192;  // Max triangles per frame

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

void Flux_PrimitivesImpl::BuildPipelines()
{
	// Load shaders
	m_xPrimitivesShader.Initialise(FluxShaderProgram::Primitives);

	// Define vertex layout (Position, Normal, Color)
	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);  // Position
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);  // Normal
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);  // Color (unused, from push constant)
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	// Build GBuffer pipeline (solid shading)
	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_aeColourAttachmentFormats[MRT_INDEX_DIFFUSE] = MRT_FORMAT_DIFFUSE;
	xPipelineSpec.m_aeColourAttachmentFormats[MRT_INDEX_NORMALSAMBIENT] = MRT_FORMAT_NORMALSAMBIENT;
	xPipelineSpec.m_aeColourAttachmentFormats[MRT_INDEX_MATERIAL] = MRT_FORMAT_MATERIAL;
	xPipelineSpec.m_uNumColourAttachments = MRT_INDEX_COUNT;
	xPipelineSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
	xPipelineSpec.m_pxShader = &m_xPrimitivesShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	m_xPrimitivesShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

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

	Flux_PipelineBuilder::FromSpecification(m_xPrimitivesPipeline, xPipelineSpec);

	// Wireframe variant
	xPipelineSpec.m_bWireframe = true;
	Flux_PipelineBuilder::FromSpecification(m_xPrimitivesWireframePipeline, xPipelineSpec);
}

static void ExecuteGBuffer(Flux_CommandList* pxCmdList, void* pUserData);

void Flux_PrimitivesImpl::Initialise()
{
	Flux_MemoryManager& xVulkanMemory = g_xEngine.FluxMemory();

	// Generate procedural meshes
	Zenith_Vector<PrimitiveVertex> xVertices;
	Zenith_Vector<u_int> xIndices;

	// Unit sphere
	GenerateUnitSphere(xVertices, xIndices);
	m_uSphereIndexCount = xIndices.GetSize();
	xVulkanMemory.InitialiseVertexBuffer(xVertices.GetDataPointer(), xVertices.GetSize() * sizeof(PrimitiveVertex), m_xSphereVertexBuffer);
	xVulkanMemory.InitialiseIndexBuffer(xIndices.GetDataPointer(), xIndices.GetSize() * sizeof(u_int), m_xSphereIndexBuffer);

	// Unit cube
	GenerateUnitCube(xVertices, xIndices);
	m_uCubeIndexCount = xIndices.GetSize();
	xVulkanMemory.InitialiseVertexBuffer(xVertices.GetDataPointer(), xVertices.GetSize() * sizeof(PrimitiveVertex), m_xCubeVertexBuffer);
	xVulkanMemory.InitialiseIndexBuffer(xIndices.GetDataPointer(), xIndices.GetSize() * sizeof(u_int), m_xCubeIndexBuffer);

	// Unit capsule
	GenerateUnitCapsule(xVertices, xIndices);
	m_uCapsuleIndexCount = xIndices.GetSize();
	xVulkanMemory.InitialiseVertexBuffer(xVertices.GetDataPointer(), xVertices.GetSize() * sizeof(PrimitiveVertex), m_xCapsuleVertexBuffer);
	xVulkanMemory.InitialiseIndexBuffer(xIndices.GetDataPointer(), xIndices.GetSize() * sizeof(u_int), m_xCapsuleIndexBuffer);

	// Unit cylinder
	GenerateUnitCylinder(xVertices, xIndices);
	m_uCylinderIndexCount = xIndices.GetSize();
	xVulkanMemory.InitialiseVertexBuffer(xVertices.GetDataPointer(), xVertices.GetSize() * sizeof(PrimitiveVertex), m_xCylinderVertexBuffer);
	xVulkanMemory.InitialiseIndexBuffer(xIndices.GetDataPointer(), xIndices.GetSize() * sizeof(u_int), m_xCylinderIndexBuffer);

	// Unit line
	GenerateUnitLine(xVertices, xIndices);
	m_uLineIndexCount = xIndices.GetSize();
	xVulkanMemory.InitialiseVertexBuffer(xVertices.GetDataPointer(), xVertices.GetSize() * sizeof(PrimitiveVertex), m_xLineVertexBuffer);
	xVulkanMemory.InitialiseIndexBuffer(xIndices.GetDataPointer(), xIndices.GetSize() * sizeof(u_int), m_xLineIndexBuffer);

	BuildPipelines();

	// Pre-allocate triangle buffers (dynamic vertex buffer, static index buffer)
	// This avoids recreating GPU buffers every frame which causes memory leaks
	{
		const size_t uVertexBufferSize = s_uMaxTriangles * 3 * sizeof(PrimitiveVertex);
		const size_t uIndexBufferSize = s_uMaxTriangles * 3 * sizeof(u_int);

		xVulkanMemory.InitialiseDynamicVertexBuffer(nullptr, uVertexBufferSize, m_xTriangleDynamicVertexBuffer, false);
		xVulkanMemory.InitialiseIndexBuffer(nullptr, uIndexBufferSize, m_xTriangleIndexBuffer);
		m_bTriangleBuffersInitialised = true;
	}

#ifdef ZENITH_DEBUG_VARIABLES
#endif

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::Primitives,
	};
	Flux_ShaderHotReload::RegisterSubsystem([](){ g_xEngine.Primitives().BuildPipelines(); },
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Primitives initialised");
}

void Flux_PrimitivesImpl::Shutdown()
{
	// Destroy all vertex and index buffers
	Flux_MemoryManager& xVulkanMemory = g_xEngine.FluxMemory();
	xVulkanMemory.DestroyVertexBuffer(m_xSphereVertexBuffer);
	xVulkanMemory.DestroyIndexBuffer(m_xSphereIndexBuffer);

	xVulkanMemory.DestroyVertexBuffer(m_xCubeVertexBuffer);
	xVulkanMemory.DestroyIndexBuffer(m_xCubeIndexBuffer);

	xVulkanMemory.DestroyVertexBuffer(m_xCapsuleVertexBuffer);
	xVulkanMemory.DestroyIndexBuffer(m_xCapsuleIndexBuffer);

	xVulkanMemory.DestroyVertexBuffer(m_xCylinderVertexBuffer);
	xVulkanMemory.DestroyIndexBuffer(m_xCylinderIndexBuffer);

	xVulkanMemory.DestroyVertexBuffer(m_xLineVertexBuffer);
	xVulkanMemory.DestroyIndexBuffer(m_xLineIndexBuffer);

	// Destroy pre-allocated triangle buffers
	if (m_bTriangleBuffersInitialised)
	{
		xVulkanMemory.DestroyDynamicVertexBuffer(m_xTriangleDynamicVertexBuffer);
		xVulkanMemory.DestroyIndexBuffer(m_xTriangleIndexBuffer);
		m_bTriangleBuffersInitialised = false;
	}

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Primitives shut down");
}

void Flux_PrimitivesImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	xGraph.AddPass("Primitives GBuffer", ExecuteGBuffer)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetDepthAttachment(),                       RESOURCE_ACCESS_WRITE_DSV);
}

void Flux_PrimitivesImpl::AddSphere(const Zenith_Maths::Vector3& xCenter, float fRadius, const Zenith_Maths::Vector3& xColor)
{
	m_xInstanceMutex.Lock();
	SphereInstance xInstance;
	xInstance.m_xCenter = xCenter;
	xInstance.m_fRadius = fRadius;
	xInstance.m_xColor = xColor;
	m_xSphereInstances.PushBack(xInstance);
	m_xInstanceMutex.Unlock();
}

void Flux_PrimitivesImpl::AddCube(const Zenith_Maths::Vector3& xCenter, const Zenith_Maths::Vector3& xHalfExtents, const Zenith_Maths::Vector3& xColor)
{
	m_xInstanceMutex.Lock();
	CubeInstance xInstance;
	xInstance.m_xCenter = xCenter;
	xInstance.m_xHalfExtents = xHalfExtents;
	xInstance.m_xColor = xColor;
	xInstance.m_bWireframe = false;
	m_xCubeInstances.PushBack(xInstance);
	m_xInstanceMutex.Unlock();
}

void Flux_PrimitivesImpl::AddWireframeCube(const Zenith_Maths::Vector3& xCenter, const Zenith_Maths::Vector3& xHalfExtents, const Zenith_Maths::Vector3& xColor)
{
	m_xInstanceMutex.Lock();
	CubeInstance xInstance;
	xInstance.m_xCenter = xCenter;
	xInstance.m_xHalfExtents = xHalfExtents;
	xInstance.m_xColor = xColor;
	xInstance.m_bWireframe = true;
	m_xCubeInstances.PushBack(xInstance);
	m_xInstanceMutex.Unlock();
}

void Flux_PrimitivesImpl::AddLine(const Zenith_Maths::Vector3& xStart, const Zenith_Maths::Vector3& xEnd, const Zenith_Maths::Vector3& xColor, float fThickness)
{
	m_xInstanceMutex.Lock();
	LineInstance xInstance;
	xInstance.m_xStart = xStart;
	xInstance.m_xEnd = xEnd;
	xInstance.m_xColor = xColor;
	xInstance.m_fThickness = fThickness;
	m_xLineInstances.PushBack(xInstance);
	m_xInstanceMutex.Unlock();
}

void Flux_PrimitivesImpl::AddCapsule(const Zenith_Maths::Vector3& xStart, const Zenith_Maths::Vector3& xEnd, float fRadius, const Zenith_Maths::Vector3& xColor)
{
	m_xInstanceMutex.Lock();
	CapsuleInstance xInstance;
	xInstance.m_xStart = xStart;
	xInstance.m_xEnd = xEnd;
	xInstance.m_fRadius = fRadius;
	xInstance.m_xColor = xColor;
	m_xCapsuleInstances.PushBack(xInstance);
	m_xInstanceMutex.Unlock();
}

void Flux_PrimitivesImpl::AddCylinder(const Zenith_Maths::Vector3& xStart, const Zenith_Maths::Vector3& xEnd, float fRadius, const Zenith_Maths::Vector3& xColor)
{
	m_xInstanceMutex.Lock();
	CylinderInstance xInstance;
	xInstance.m_xStart = xStart;
	xInstance.m_xEnd = xEnd;
	xInstance.m_fRadius = fRadius;
	xInstance.m_xColor = xColor;
	m_xCylinderInstances.PushBack(xInstance);
	m_xInstanceMutex.Unlock();
}

void Flux_PrimitivesImpl::AddTriangle(const Zenith_Maths::Vector3& xV0, const Zenith_Maths::Vector3& xV1,
	const Zenith_Maths::Vector3& xV2, const Zenith_Maths::Vector3& xColor)
{
	m_xInstanceMutex.Lock();
	TriangleInstance xInstance;
	xInstance.m_xV0 = xV0;
	xInstance.m_xV1 = xV1;
	xInstance.m_xV2 = xV2;
	xInstance.m_xColor = xColor;
	m_xTriangleInstances.PushBack(xInstance);
	m_xInstanceMutex.Unlock();
}

void Flux_PrimitivesImpl::Clear()
{
	m_xInstanceMutex.Lock();
	m_xSphereInstances.Clear();
	m_xCubeInstances.Clear();
	m_xLineInstances.Clear();
	m_xCapsuleInstances.Clear();
	m_xCylinderInstances.Clear();
	m_xTriangleInstances.Clear();
	m_xInstanceMutex.Unlock();
}

// ========== RENDERING ==========

// Compute rotation aligning unit Y-axis (0,1,0) with the direction from xStart to xEnd.
// Used by line, capsule, and cylinder primitives — all of their unit meshes lie along Y
// and need to be rotated to point start→end. Returns m_bValid=false for zero-length
// segments so callers can `continue` without producing NaN from the normalize.
struct YAxisAlignment
{
	Zenith_Maths::Quaternion m_xRotation;
	Zenith_Maths::Vector3 m_xNormalizedDirection;
	float m_fLength;
	bool m_bValid;
};

static YAxisAlignment ComputeYAxisAlignment(const Zenith_Maths::Vector3& xStart, const Zenith_Maths::Vector3& xEnd)
{
	YAxisAlignment xResult;
	Zenith_Maths::Vector3 xDirection = xEnd - xStart;
	xResult.m_fLength = Zenith_Maths::Length(xDirection);
	constexpr float fMinLength = 1e-6f;
	if (xResult.m_fLength < fMinLength)
	{
		xResult.m_bValid = false;
		return xResult;
	}
	xResult.m_bValid = true;
	xResult.m_xNormalizedDirection = xDirection / xResult.m_fLength;

	Zenith_Maths::Vector3 xUp(0, 1, 0);
	if (fabsf(Zenith_Maths::Dot(xUp, xResult.m_xNormalizedDirection)) < 0.9999f)
	{
		Zenith_Maths::Vector3 xAxis = Zenith_Maths::Normalize(Zenith_Maths::Cross(xUp, xResult.m_xNormalizedDirection));
		float fAngle = acosf(Zenith_Maths::Dot(xUp, xResult.m_xNormalizedDirection));
		xResult.m_xRotation = Zenith_Maths::AngleAxis(fAngle, xAxis);
	}
	else
	{
		xResult.m_xRotation = Zenith_Maths::Quaternion(1, 0, 0, 0);
	}
	return xResult;
}

// Push the per-instance constant (model matrix + colour) and emit the indexed draw call.
void Flux_PrimitivesImpl::EmitPrimitiveDraw(Flux_CommandList* pxCmdList, Flux_ShaderBinder& xBinder,
	const Zenith_Maths::Matrix4& xModelMatrix,
	const Zenith_Maths::Vector3& xColor,
	u_int uIndexCount)
{
	PrimitivePushConstant xPushConstant;
	xPushConstant.m_xModelMatrix = xModelMatrix;
	xPushConstant.m_xColor = xColor;
	xPushConstant.m_fPadding = 0.0f;

	// Slang reflection keys on the variable name, not the GLSL block instance.
	xBinder.BindDrawConstants(m_xPrimitivesShader, "PrimitivePushConstant", &xPushConstant, sizeof(PrimitivePushConstant));
	pxCmdList->AddCommand<Flux_CommandDrawIndexed>(uIndexCount);
}

void Flux_PrimitivesImpl::RenderSpherePrimitives(Flux_CommandList* pxCmdList, Flux_ShaderBinder& xBinder,
	const Zenith_Vector<SphereInstance>& xInstances)
{
	if (xInstances.GetSize() == 0) return;

	pxCmdList->AddCommand<Flux_CommandSetPipeline>(&m_xPrimitivesPipeline);
	pxCmdList->AddCommand<Flux_CommandSetVertexBuffer>(&m_xSphereVertexBuffer);
	pxCmdList->AddCommand<Flux_CommandSetIndexBuffer>(&m_xSphereIndexBuffer);

	for (u_int i = 0; i < xInstances.GetSize(); ++i)
	{
		const SphereInstance& xInstance = xInstances.Get(i);
		Zenith_Maths::Matrix4 xModelMatrix = Zenith_Maths::Translate(Zenith_Maths::Matrix4(1.0f), xInstance.m_xCenter);
		xModelMatrix = Zenith_Maths::Scale(xModelMatrix, Zenith_Maths::Vector3(xInstance.m_fRadius));
		EmitPrimitiveDraw(pxCmdList, xBinder, xModelMatrix, xInstance.m_xColor, m_uSphereIndexCount);
	}
}

void Flux_PrimitivesImpl::RenderCubePrimitives(Flux_CommandList* pxCmdList, Flux_ShaderBinder& xBinder,
	const Zenith_Vector<CubeInstance>& xInstances)
{
	if (xInstances.GetSize() == 0) return;

	for (u_int i = 0; i < xInstances.GetSize(); ++i)
	{
		const CubeInstance& xInstance = xInstances.Get(i);
		// Wireframe state varies per cube, so set the pipeline inside the loop rather than
		// pre-sorting by wireframe — wireframe cubes are rare enough that a per-instance
		// pipeline switch is cheaper than two separate batches.
		pxCmdList->AddCommand<Flux_CommandSetPipeline>(
			xInstance.m_bWireframe ? &m_xPrimitivesWireframePipeline : &m_xPrimitivesPipeline);
		pxCmdList->AddCommand<Flux_CommandSetVertexBuffer>(&m_xCubeVertexBuffer);
		pxCmdList->AddCommand<Flux_CommandSetIndexBuffer>(&m_xCubeIndexBuffer);

		Zenith_Maths::Matrix4 xModelMatrix = Zenith_Maths::Translate(Zenith_Maths::Matrix4(1.0f), xInstance.m_xCenter);
		xModelMatrix = Zenith_Maths::Scale(xModelMatrix, xInstance.m_xHalfExtents);
		EmitPrimitiveDraw(pxCmdList, xBinder, xModelMatrix, xInstance.m_xColor, m_uCubeIndexCount);
	}
}

void Flux_PrimitivesImpl::RenderLinePrimitives(Flux_CommandList* pxCmdList, Flux_ShaderBinder& xBinder,
	const Zenith_Vector<LineInstance>& xInstances)
{
	if (xInstances.GetSize() == 0) return;

	pxCmdList->AddCommand<Flux_CommandSetPipeline>(&m_xPrimitivesPipeline);
	pxCmdList->AddCommand<Flux_CommandSetVertexBuffer>(&m_xLineVertexBuffer);
	pxCmdList->AddCommand<Flux_CommandSetIndexBuffer>(&m_xLineIndexBuffer);

	for (u_int i = 0; i < xInstances.GetSize(); ++i)
	{
		const LineInstance& xInstance = xInstances.Get(i);
		YAxisAlignment xAlign = ComputeYAxisAlignment(xInstance.m_xStart, xInstance.m_xEnd);
		if (!xAlign.m_bValid) continue;

		Zenith_Maths::Matrix4 xModelMatrix = Zenith_Maths::Translate(Zenith_Maths::Matrix4(1.0f), xInstance.m_xStart);
		xModelMatrix = xModelMatrix * Zenith_Maths::Mat4Cast(xAlign.m_xRotation);
		xModelMatrix = Zenith_Maths::Scale(xModelMatrix, Zenith_Maths::Vector3(xInstance.m_fThickness, xAlign.m_fLength * 0.5f, xInstance.m_fThickness));
		EmitPrimitiveDraw(pxCmdList, xBinder, xModelMatrix, xInstance.m_xColor, m_uLineIndexCount);
	}
}

void Flux_PrimitivesImpl::RenderCapsulePrimitives(Flux_CommandList* pxCmdList, Flux_ShaderBinder& xBinder,
	const Zenith_Vector<CapsuleInstance>& xInstances)
{
	if (xInstances.GetSize() == 0) return;

	pxCmdList->AddCommand<Flux_CommandSetPipeline>(&m_xPrimitivesPipeline);
	pxCmdList->AddCommand<Flux_CommandSetVertexBuffer>(&m_xCapsuleVertexBuffer);
	pxCmdList->AddCommand<Flux_CommandSetIndexBuffer>(&m_xCapsuleIndexBuffer);

	for (u_int i = 0; i < xInstances.GetSize(); ++i)
	{
		const CapsuleInstance& xInstance = xInstances.Get(i);
		YAxisAlignment xAlign = ComputeYAxisAlignment(xInstance.m_xStart, xInstance.m_xEnd);
		if (!xAlign.m_bValid) continue;

		Zenith_Maths::Vector3 xCenter = (xInstance.m_xStart + xInstance.m_xEnd) * 0.5f;
		Zenith_Maths::Matrix4 xModelMatrix = Zenith_Maths::Translate(Zenith_Maths::Matrix4(1.0f), xCenter);
		xModelMatrix = xModelMatrix * Zenith_Maths::Mat4Cast(xAlign.m_xRotation);
		xModelMatrix = Zenith_Maths::Scale(xModelMatrix, Zenith_Maths::Vector3(xInstance.m_fRadius * 2.0f, xAlign.m_fLength * 0.5f, xInstance.m_fRadius * 2.0f));
		EmitPrimitiveDraw(pxCmdList, xBinder, xModelMatrix, xInstance.m_xColor, m_uCapsuleIndexCount);
	}
}

void Flux_PrimitivesImpl::RenderCylinderPrimitives(Flux_CommandList* pxCmdList, Flux_ShaderBinder& xBinder,
	const Zenith_Vector<CylinderInstance>& xInstances)
{
	if (xInstances.GetSize() == 0) return;

	pxCmdList->AddCommand<Flux_CommandSetPipeline>(&m_xPrimitivesPipeline);
	pxCmdList->AddCommand<Flux_CommandSetVertexBuffer>(&m_xCylinderVertexBuffer);
	pxCmdList->AddCommand<Flux_CommandSetIndexBuffer>(&m_xCylinderIndexBuffer);

	for (u_int i = 0; i < xInstances.GetSize(); ++i)
	{
		const CylinderInstance& xInstance = xInstances.Get(i);
		YAxisAlignment xAlign = ComputeYAxisAlignment(xInstance.m_xStart, xInstance.m_xEnd);
		if (!xAlign.m_bValid) continue;

		Zenith_Maths::Vector3 xCenter = (xInstance.m_xStart + xInstance.m_xEnd) * 0.5f;
		Zenith_Maths::Matrix4 xModelMatrix = Zenith_Maths::Translate(Zenith_Maths::Matrix4(1.0f), xCenter);
		xModelMatrix = xModelMatrix * Zenith_Maths::Mat4Cast(xAlign.m_xRotation);
		xModelMatrix = Zenith_Maths::Scale(xModelMatrix, Zenith_Maths::Vector3(xInstance.m_fRadius * 2.0f, xAlign.m_fLength * 0.5f, xInstance.m_fRadius * 2.0f));
		EmitPrimitiveDraw(pxCmdList, xBinder, xModelMatrix, xInstance.m_xColor, m_uCylinderIndexCount);
	}
}

// Triangles are special: no shared unit mesh, so each triangle becomes 3 vertices
// uploaded to a dynamic vertex buffer (capped at s_uMaxTriangles per frame). All
// triangles draw with identity transform — their world-space vertices and per-vertex
// colour carry the position/colour state.
void Flux_PrimitivesImpl::RenderTrianglePrimitives(Flux_CommandList* pxCmdList, Flux_ShaderBinder& xBinder,
	const Zenith_Vector<TriangleInstance>& xInstances)
{
	if (xInstances.GetSize() == 0 || !m_bTriangleBuffersInitialised) return;

	u_int uTriangleCount = xInstances.GetSize();
	if (uTriangleCount > s_uMaxTriangles)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Warning: Triangle count %u exceeds max %u, clamping", uTriangleCount, s_uMaxTriangles);
		uTriangleCount = s_uMaxTriangles;
	}

	const u_int uVertexCount = uTriangleCount * 3;
	const u_int uIndexCount = uTriangleCount * 3;
	Zenith_Vector<PrimitiveVertex> xVertices;
	Zenith_Vector<u_int> xIndices;
	xVertices.Reserve(uVertexCount);
	xIndices.Reserve(uIndexCount);

	for (u_int i = 0; i < uTriangleCount; ++i)
	{
		const TriangleInstance& xInstance = xInstances.Get(i);
		Zenith_Maths::Vector3 xEdge1 = xInstance.m_xV1 - xInstance.m_xV0;
		Zenith_Maths::Vector3 xEdge2 = xInstance.m_xV2 - xInstance.m_xV0;
		Zenith_Maths::Vector3 xNormal = Zenith_Maths::Cross(xEdge1, xEdge2);
		float fLen = Zenith_Maths::Length(xNormal);
		xNormal = (fLen > 0.0001f) ? (xNormal / fLen) : Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);

		u_int uBaseVertex = xVertices.GetSize();
		PrimitiveVertex xV0, xV1, xV2;
		xV0.m_xPosition = xInstance.m_xV0; xV0.m_xNormal = xNormal; xV0.m_xColor = xInstance.m_xColor;
		xV1.m_xPosition = xInstance.m_xV1; xV1.m_xNormal = xNormal; xV1.m_xColor = xInstance.m_xColor;
		xV2.m_xPosition = xInstance.m_xV2; xV2.m_xNormal = xNormal; xV2.m_xColor = xInstance.m_xColor;
		xVertices.PushBack(xV0);
		xVertices.PushBack(xV1);
		xVertices.PushBack(xV2);
		xIndices.PushBack(uBaseVertex + 0);
		xIndices.PushBack(uBaseVertex + 1);
		xIndices.PushBack(uBaseVertex + 2);
	}

	g_xEngine.FluxMemory().UploadBufferData(m_xTriangleDynamicVertexBuffer.GetBuffer().m_xVRAMHandle,
		xVertices.GetDataPointer(), xVertices.GetSize() * sizeof(PrimitiveVertex));
	g_xEngine.FluxMemory().UploadBufferData(m_xTriangleIndexBuffer.GetBuffer().m_xVRAMHandle,
		xIndices.GetDataPointer(), xIndices.GetSize() * sizeof(u_int));

	pxCmdList->AddCommand<Flux_CommandSetPipeline>(&m_xPrimitivesPipeline);
	pxCmdList->AddCommand<Flux_CommandSetVertexBuffer>(&m_xTriangleDynamicVertexBuffer);
	pxCmdList->AddCommand<Flux_CommandSetIndexBuffer>(&m_xTriangleIndexBuffer);

	EmitPrimitiveDraw(pxCmdList, xBinder, Zenith_Maths::Matrix4(1.0f), Zenith_Maths::Vector3(1.0f), xIndices.GetSize());
}

static void ExecuteGBuffer(Flux_CommandList* pxCmdList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bPrimitivesEnabled)
	{
		return;
	}

	// Non-capturing graph callback (void(*)(Flux_CommandList*, void*)) — it cannot
	// capture, so it re-enters via g_xEngine.Primitives() to reach the singleton
	// instance FIRST; cross-subsystem deps (FluxGraphics / VulkanMemory) are
	// reached via g_xEngine at point of use (mirrors ExecuteSSAOGenerate).
	Flux_PrimitivesImpl& xPrimitives = g_xEngine.Primitives();

	// Drain the instance queues under lock — Add*() runs on game-thread workers while
	// ExecuteGBuffer runs on the render thread, so we copy out & clear while holding
	// xPrimitives.m_xInstanceMutex.
	Zenith_Vector<SphereInstance> xLocalSphereInstances;
	Zenith_Vector<CubeInstance> xLocalCubeInstances;
	Zenith_Vector<LineInstance> xLocalLineInstances;
	Zenith_Vector<CapsuleInstance> xLocalCapsuleInstances;
	Zenith_Vector<CylinderInstance> xLocalCylinderInstances;
	Zenith_Vector<TriangleInstance> xLocalTriangleInstances;
	{
		Zenith_ScopedMutexLock xLock(xPrimitives.m_xInstanceMutex);
		if (xPrimitives.m_xSphereInstances.GetSize() == 0 && xPrimitives.m_xCubeInstances.GetSize() == 0 &&
			xPrimitives.m_xLineInstances.GetSize() == 0 && xPrimitives.m_xCapsuleInstances.GetSize() == 0 &&
			xPrimitives.m_xCylinderInstances.GetSize() == 0 && xPrimitives.m_xTriangleInstances.GetSize() == 0)
		{
			return;
		}
		for (u_int i = 0; i < xPrimitives.m_xSphereInstances.GetSize(); ++i) xLocalSphereInstances.PushBack(xPrimitives.m_xSphereInstances.Get(i));
		for (u_int i = 0; i < xPrimitives.m_xCubeInstances.GetSize(); ++i) xLocalCubeInstances.PushBack(xPrimitives.m_xCubeInstances.Get(i));
		for (u_int i = 0; i < xPrimitives.m_xLineInstances.GetSize(); ++i) xLocalLineInstances.PushBack(xPrimitives.m_xLineInstances.Get(i));
		for (u_int i = 0; i < xPrimitives.m_xCapsuleInstances.GetSize(); ++i) xLocalCapsuleInstances.PushBack(xPrimitives.m_xCapsuleInstances.Get(i));
		for (u_int i = 0; i < xPrimitives.m_xCylinderInstances.GetSize(); ++i) xLocalCylinderInstances.PushBack(xPrimitives.m_xCylinderInstances.Get(i));
		for (u_int i = 0; i < xPrimitives.m_xTriangleInstances.GetSize(); ++i) xLocalTriangleInstances.PushBack(xPrimitives.m_xTriangleInstances.Get(i));
		xPrimitives.m_xSphereInstances.Clear();
		xPrimitives.m_xCubeInstances.Clear();
		xPrimitives.m_xLineInstances.Clear();
		xPrimitives.m_xCapsuleInstances.Clear();
		xPrimitives.m_xCylinderInstances.Clear();
		xPrimitives.m_xTriangleInstances.Clear();
	}

	Flux_ShaderBinder xBinder(*pxCmdList);
	xBinder.BindCBV(xPrimitives.m_xPrimitivesShader, "FrameConstants", &g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV());

	xPrimitives.RenderSpherePrimitives(pxCmdList, xBinder, xLocalSphereInstances);
	xPrimitives.RenderCubePrimitives(pxCmdList, xBinder, xLocalCubeInstances);
	xPrimitives.RenderLinePrimitives(pxCmdList, xBinder, xLocalLineInstances);
	xPrimitives.RenderCapsulePrimitives(pxCmdList, xBinder, xLocalCapsuleInstances);
	xPrimitives.RenderCylinderPrimitives(pxCmdList, xBinder, xLocalCylinderInstances);
	xPrimitives.RenderTrianglePrimitives(pxCmdList, xBinder, xLocalTriangleInstances);
}

// ========== HELPER FUNCTIONS ==========

void Flux_PrimitivesImpl::AddCross(const Zenith_Maths::Vector3& xCenter, float fSize, const Zenith_Maths::Vector3& xColor)
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

void Flux_PrimitivesImpl::AddCircle(const Zenith_Maths::Vector3& xCenter, float fRadius, const Zenith_Maths::Vector3& xColor,
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

void Flux_PrimitivesImpl::AddArrow(const Zenith_Maths::Vector3& xStart, const Zenith_Maths::Vector3& xEnd,
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

void Flux_PrimitivesImpl::AddConeOutline(const Zenith_Maths::Vector3& xApex, const Zenith_Maths::Vector3& xDirection,
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

void Flux_PrimitivesImpl::AddArc(const Zenith_Maths::Vector3& xCenter, float fRadius, float fStartAngle, float fEndAngle,
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

void Flux_PrimitivesImpl::AddPolygonOutline(const Zenith_Maths::Vector3* axVertices, uint32_t uVertexCount,
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

void Flux_PrimitivesImpl::AddGrid(const Zenith_Maths::Vector3& xCenter, float fSize, uint32_t uDivisions,
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

void Flux_PrimitivesImpl::AddAxes(const Zenith_Maths::Vector3& xOrigin, float fSize)
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
