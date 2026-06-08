#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_Entity.h"
#include "Core/Zenith_GizmoTransformAccess.h" // Wave 3: drives the EC-side transform write-accessor instead of including Zenith_TransformComponent.h
#include "Flux/Gizmos/Flux_GizmosImpl.h"
#include "Flux/Gizmos/Flux_GizmosImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Primitives/Flux_PrimitivesImpl.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Maths/Zenith_Maths_Intersections.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/Slang/Flux_ShaderHotReload.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"

// Constants
static constexpr float GIZMO_BASE_SIZE = 1.0f;
static constexpr float GIZMO_ARROW_LENGTH = 1.2f;
static constexpr float GIZMO_ARROW_HEAD_LENGTH = 0.3f;
static constexpr float GIZMO_ARROW_HEAD_RADIUS = 0.1f;
static constexpr float GIZMO_ARROW_SHAFT_RADIUS = 0.03f;
static constexpr float GIZMO_CIRCLE_RADIUS = 1.0f;
static constexpr uint32_t GIZMO_CIRCLE_SEGMENTS = 64;
static constexpr float GIZMO_CUBE_SIZE = 0.15f;
static constexpr float GIZMO_INTERACTION_THRESHOLD = 0.2f;  // Distance threshold for ray-gizmo intersection
static constexpr float GIZMO_INTERACTION_LENGTH_MULTIPLIER = 1.0f; // FIXED: Was 10.0 - caused false positive hits far from arrow
static constexpr float GIZMO_AUTO_SCALE_DISTANCE = 5.0f;     // Distance at which gizmo is 1.0 scale

// Debug variables
DEBUGVAR float dbg_fGizmoAlpha = 1.0f;

// Static member initialization




static void ExecuteGizmos(Flux_CommandList* pxCommandList, void* pUserData);

void Flux_GizmosImpl::BuildPipelines()
{
	// Load shaders
	m_xShader.Initialise(FluxShaderProgram::Gizmos);

	// Create pipeline specification
	Flux_PipelineSpecification xSpec;
	xSpec.m_pxShader = &m_xShader;
	xSpec.m_aeColourAttachmentFormats[0] = FINAL_RT_FORMAT;
	xSpec.m_uNumColourAttachments = 1;

	// Vertex input description
	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3); // Position
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3); // Color
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	xSpec.m_xVertexInputDesc = xVertexDesc;

	// Depth state - test but don't write (gizmos always visible on top)
	xSpec.m_bDepthTestEnabled = false;
	xSpec.m_bDepthWriteEnabled = false;

	// Blending for transparency
	xSpec.m_axBlendStates[0].m_bBlendEnabled = true;
	xSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_SRCALPHA;
	xSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONEMINUSSRCALPHA;

	// Pipeline layout from shader reflection
	m_xShader.GetReflection().PopulateLayout(xSpec.m_xPipelineLayout);

	// Build pipeline
	Flux_PipelineBuilder::FromSpecification(m_xPipeline, xSpec);
}

void Flux_GizmosImpl::Initialise(Flux_GraphicsImpl& xFluxGraphics, Flux_PrimitivesImpl& xPrimitives, Zenith_Vulkan_MemoryManager& xVulkanMemory)
{
	m_pxFluxGraphics = &xFluxGraphics;
	m_pxPrimitives   = &xPrimitives;
	m_pxVulkanMemory = &xVulkanMemory;

	BuildPipelines();

	// Generate gizmo geometry
	GenerateTranslationGizmoGeometry();
	GenerateRotationGizmoGeometry();
	GenerateScaleGizmoGeometry();

#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddFloat({"Editor", "Gizmos", "Alpha"}, dbg_fGizmoAlpha, 0.0f, 1.0f);
#endif

	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::Gizmos,
	};
	Flux_ShaderHotReload::RegisterSubsystem([](){ g_xEngine.Gizmos().BuildPipelines(); },
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));

	Zenith_Log(LOG_CATEGORY_GIZMOS, "Flux_Gizmos initialised");
}

void Flux_GizmosImpl::Shutdown()
{
	// Destroy GPU buffers for all gizmo geometry
	Zenith_Vulkan_MemoryManager* pxVulkanMemory = m_pxVulkanMemory;
	auto DestroyGeometryBuffers = [pxVulkanMemory](Zenith_Vector<Flux_GizmosImpl::GizmoGeometry>& xGeometry)
	{
		for (uint32_t i = 0; i < xGeometry.GetSize(); ++i)
		{
			pxVulkanMemory->DestroyVertexBuffer(xGeometry.Get(i).m_xVertexBuffer);
			pxVulkanMemory->DestroyIndexBuffer(xGeometry.Get(i).m_xIndexBuffer);
		}
		xGeometry.Clear();
	};

	DestroyGeometryBuffers(m_xTranslateGeometry);
	DestroyGeometryBuffers(m_xRotateGeometry);
	DestroyGeometryBuffers(m_xScaleGeometry);

	m_pxFluxGraphics = nullptr;
	m_pxPrimitives   = nullptr;
	m_pxVulkanMemory = nullptr;

	Zenith_Log(LOG_CATEGORY_GIZMOS, "Flux_Gizmos shut down");
}

void Flux_GizmosImpl::Reset()
{
	// Clear target entity reference (will be invalid after scene reset)
	m_pxTargetEntity = nullptr;

	// Reset interaction state
	m_eHoveredComponent = GizmoComponent::None;
	m_eActiveComponent = GizmoComponent::None;
	m_bIsInteracting = false;

	Zenith_Log(LOG_CATEGORY_GIZMOS, "Flux_GizmosImpl::Reset() - Cleared entity reference");
}

// ==================== EXTRACTED HELPERS ====================

Zenith_Entity* Flux_GizmosImpl::GetGizmoTargetWithTransform()
{
	// Wave 3: the gizmo no longer names Zenith_TransformComponent. It returns the
	// target entity iff it currently has a transform, resolved EC-side through
	// g_xGizmoTransformAccess (whose HasTransform preserves the original entity-OWN-
	// scene resolution — multi-scene editing still works). Callers drive get/set TRS
	// through the same accessor, so the renderer-side gizmo includes no EC component.
	if (!m_pxTargetEntity)
	{
		return nullptr;
	}
	if (!g_xGizmoTransformAccess.m_pfnHasTransform(m_pxTargetEntity))
	{
		return nullptr;
	}
	return m_pxTargetEntity;
}

void Flux_GizmosImpl::InterleaveVertexData(Zenith_Vector<float>& xOut, const Zenith_Vector<Zenith_Maths::Vector3>& xPositions, const Zenith_Vector<Zenith_Maths::Vector3>& xColors)
{
	for (uint32_t i = 0; i < xPositions.GetSize(); ++i)
	{
		xOut.PushBack(xPositions.Get(i).x);
		xOut.PushBack(xPositions.Get(i).y);
		xOut.PushBack(xPositions.Get(i).z);
		xOut.PushBack(xColors.Get(i).x);
		xOut.PushBack(xColors.Get(i).y);
		xOut.PushBack(xColors.Get(i).z);
	}
}

void Flux_GizmosImpl::UploadGizmoGeometry(Zenith_Vector<Flux_GizmosImpl::GizmoGeometry>& xGeometryList, const Zenith_Vector<float>& xVertexData, const Zenith_Vector<uint32_t>& xIndices, const Zenith_Maths::Vector3& xColor, GizmoComponent eComponent)
{
	GizmoGeometry xGeom;
	xGeom.m_eComponent = eComponent;
	xGeom.m_xColor = xColor;
	xGeom.m_uIndexCount = xIndices.GetSize();

	m_pxVulkanMemory->InitialiseVertexBuffer(
		xVertexData.GetDataPointer(),
		xVertexData.GetSize() * sizeof(float),
		xGeom.m_xVertexBuffer
	);

	m_pxVulkanMemory->InitialiseIndexBuffer(
		xIndices.GetDataPointer(),
		xIndices.GetSize() * sizeof(uint32_t),
		xGeom.m_xIndexBuffer
	);

	xGeometryList.PushBack(xGeom);
}

bool Flux_GizmosImpl::GetLineLineClosestPointParameter(const Zenith_Maths::Vector3& xAxisOrigin, const Zenith_Maths::Vector3& xAxis, const Zenith_Maths::Vector3& xRayOrigin, const Zenith_Maths::Vector3& xRayDir, float& fOutT)
{
	// Standard line-line closest point formula:
	// w = AxisOrigin - RayOrigin
	// t = (b*e - c*d) / (a*c - b*b)
	// where a = axis.axis, b = axis.rayDir, c = rayDir.rayDir, d = axis.w, e = rayDir.w

	Zenith_Maths::Vector3 w = xAxisOrigin - xRayOrigin;

	float a = glm::dot(xAxis, xAxis);
	float b = glm::dot(xAxis, xRayDir);
	float c = glm::dot(xRayDir, xRayDir);
	float d = glm::dot(xAxis, w);
	float e = glm::dot(xRayDir, w);

	float fDenom = a * c - b * b;

	if (glm::abs(fDenom) < 0.0001f)
	{
		return false;
	}

	fOutT = (b * e - c * d) / fDenom;
	return true;
}

void Flux_GizmosImpl::ComputeTangentFrame(const Zenith_Maths::Vector3& xAxis, Zenith_Maths::Vector3& xOutTangent, Zenith_Maths::Vector3& xOutBitangent)
{
	Zenith_Maths::Vector3 xPerpendicular = glm::abs(xAxis.x) > 0.9f ? Zenith_Maths::Vector3(0, 1, 0) : Zenith_Maths::Vector3(1, 0, 0);
	xOutTangent = glm::normalize(glm::cross(xAxis, xPerpendicular));
	xOutBitangent = glm::cross(xAxis, xOutTangent);
}

// ==================== PREPARE (main thread) ====================

// WS11.A keystone: the Gizmos pass is the last live-ECS read inside a Flux
// worker record callback. This Prepare runs on the main thread (before any
// record task is dispatched — see Flux_RenderGraph::Execute) and performs the
// GetEditableTransform ECS read here, computes the gizmo matrix/scale/entity
// position, snapshots the interaction-highlight state, and (under ZENITH_DEBUG)
// issues the three interaction-bound wireframe cubes — also on the main thread,
// where Flux_Primitives mutation is safe. The worker record callback then reads
// ONLY m_xDrawPacket. Mirrors Flux_StaticMeshesImpl::GatherDrawPacket.
void Flux_GizmosImpl::GatherGizmoPacket(void*)
{
	Flux_GizmoDrawPacket& xPacket = m_xDrawPacket;
	xPacket.m_bValid = false;

	if (!Zenith_GraphicsOptions::Get().m_bGizmosEnabled)
	{
		return;
	}

	Zenith_Entity* pxEntity = GetGizmoTargetWithTransform();
	if (!pxEntity)
	{
		return;
	}

	// Calculate gizmo scale based on camera distance for consistent screen size
	Zenith_Maths::Vector3 xEntityPos;
	g_xGizmoTransformAccess.m_pfnGetPosition(pxEntity, xEntityPos);
	Zenith_Maths::Vector3 xCameraPos = m_pxFluxGraphics->GetCameraPosition();
	float fDistance = glm::length(xEntityPos - xCameraPos);
	// Keep the live member in sync — the interaction raycast path (RaycastGizmo /
	// ApplyTranslation / ApplyScale) reads m_fGizmoScale outside the packet.
	m_fGizmoScale = fDistance / GIZMO_AUTO_SCALE_DISTANCE;

#ifdef ZENITH_DEBUG
	// Visualize gizmo interaction bounding boxes for debugging. Now issued on the
	// main thread from Prepare (was previously a worker-thread shared-state write).
	m_pxPrimitives->AddWireframeCube(xEntityPos + Zenith_Maths::Vector3(1, 0, 0) * GIZMO_ARROW_LENGTH * 0.5f * m_fGizmoScale,
		Zenith_Maths::Vector3(GIZMO_ARROW_LENGTH * m_fGizmoScale * 0.5f, GIZMO_INTERACTION_THRESHOLD * m_fGizmoScale, GIZMO_INTERACTION_THRESHOLD * m_fGizmoScale),
		Zenith_Maths::Vector3(1, 0, 0));
	m_pxPrimitives->AddWireframeCube(xEntityPos + Zenith_Maths::Vector3(0, 1, 0) * GIZMO_ARROW_LENGTH * 0.5f * m_fGizmoScale,
		Zenith_Maths::Vector3(GIZMO_INTERACTION_THRESHOLD * m_fGizmoScale, GIZMO_ARROW_LENGTH * m_fGizmoScale * 0.5f, GIZMO_INTERACTION_THRESHOLD * m_fGizmoScale),
		Zenith_Maths::Vector3(0, 1, 0));
	m_pxPrimitives->AddWireframeCube(xEntityPos + Zenith_Maths::Vector3(0, 0, 1) * GIZMO_ARROW_LENGTH * 0.5f * m_fGizmoScale,
		Zenith_Maths::Vector3(GIZMO_INTERACTION_THRESHOLD * m_fGizmoScale, GIZMO_INTERACTION_THRESHOLD * m_fGizmoScale, GIZMO_ARROW_LENGTH * m_fGizmoScale * 0.5f),
		Zenith_Maths::Vector3(0, 0, 1));
#endif

	// Build gizmo transform matrix
	Zenith_Maths::Matrix4 xGizmoMatrix = glm::translate(Zenith_Maths::Matrix4(1.0f), xEntityPos);
	xGizmoMatrix = glm::scale(xGizmoMatrix, Zenith_Maths::Vector3(m_fGizmoScale));

	// Snapshot everything the worker record callback needs.
	xPacket.m_xGizmoMatrix      = xGizmoMatrix;
	xPacket.m_xEntityPos        = xEntityPos;
	xPacket.m_fGizmoScale       = m_fGizmoScale;
	xPacket.m_eMode             = m_eMode;
	xPacket.m_eHoveredComponent = m_eHoveredComponent;
	xPacket.m_eActiveComponent  = m_eActiveComponent;
	xPacket.m_bIsInteracting    = m_bIsInteracting;
	xPacket.m_bValid            = true;
}

// ==================== RENDERING ====================

static void ExecuteGizmos(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;

	// Worker thread: read ONLY the packet gathered on the main thread in
	// GatherGizmoPacket. No GetEditableTransform, no ECS access, no Primitives
	// mutation here. m_bValid covers the gizmos-disabled and no-editable-target
	// cases (both resolved during Prepare).
	Flux_GizmosImpl& xGizmos = g_xEngine.Gizmos();
	const Flux_GizmoDrawPacket& xPacket = xGizmos.m_xDrawPacket;
	if (!xPacket.m_bValid)
	{
		return;
	}

	const Zenith_Maths::Matrix4& xGizmoMatrix = xPacket.m_xGizmoMatrix;

	// Select geometry based on mode
	Zenith_Vector<Flux_GizmosImpl::GizmoGeometry>* pxGeometry = nullptr;
	switch (xPacket.m_eMode)
	{
		case GizmoMode::Translate: pxGeometry = &xGizmos.m_xTranslateGeometry; break;
		case GizmoMode::Rotate: pxGeometry = &xGizmos.m_xRotateGeometry; break;
		case GizmoMode::Scale: pxGeometry = &xGizmos.m_xScaleGeometry; break;
	}

	if (!pxGeometry || pxGeometry->GetSize() == 0)
	{
		return;
	}

	// Record rendering commands
	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&xGizmos.m_xPipeline);

	// Create binder once - bind frame constants once (same for all gizmo components)
	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindCBV(xGizmos.m_xShader, "FrameConstants", &xGizmos.m_pxFluxGraphics->m_xFrameConstantsBuffer.GetCBV());

	// Render each gizmo component
	for (uint32_t i = 0; i < pxGeometry->GetSize(); ++i)
	{
		const Flux_GizmosImpl::GizmoGeometry& xGeom = pxGeometry->Get(i);

		// Set vertex and index buffers
		pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&xGeom.m_xVertexBuffer);
		pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&xGeom.m_xIndexBuffer);

		// Prepare push constants
		struct GizmoPushConstants
		{
			Zenith_Maths::Matrix4 m_xModelMatrix;
			float m_fHighlightIntensity;
			float m_fPad[3];
		} xPushConstants;

		xPushConstants.m_xModelMatrix = xGizmoMatrix;

		// Highlight hovered or active component
		xPushConstants.m_fHighlightIntensity = 0.0f;
		if (xGeom.m_eComponent == xPacket.m_eHoveredComponent && !xPacket.m_bIsInteracting)
			xPushConstants.m_fHighlightIntensity = 0.5f;
		else if (xGeom.m_eComponent == xPacket.m_eActiveComponent && xPacket.m_bIsInteracting)
			xPushConstants.m_fHighlightIntensity = 1.0f;

		xBinder.BindDrawConstants(xGizmos.m_xShader, "GizmoPushConstants", &xPushConstants, sizeof(xPushConstants));

		// Draw
		pxCommandList->AddCommand<Flux_CommandDrawIndexed>(xGeom.m_uIndexCount);
	}
}

void Flux_GizmosImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// WS11.A: hang the main-thread Prepare on the Gizmos pass so the editable-
	// transform ECS read + Primitives mutation happen before any worker record
	// task runs (mirrors Flux_StaticMeshesImpl::SetupRenderGraph). ExecuteGizmos
	// then reads only the gathered packet.
	xGraph.AddPass("Gizmos", ExecuteGizmos)
		.Prepare([](void* p){ g_xEngine.Gizmos().GatherGizmoPacket(p); })
		.Writes(m_pxFluxGraphics->GetFinalRenderTarget(), RESOURCE_ACCESS_WRITE_RTV);
}

void Flux_GizmosImpl::SetTargetEntity(Zenith_Entity* pxEntity)
{
	m_pxTargetEntity = pxEntity;

	// Reset interaction state when changing target
	m_bIsInteracting = false;
	m_eActiveComponent = GizmoComponent::None;
	m_eHoveredComponent = GizmoComponent::None;
}

void Flux_GizmosImpl::SetGizmoMode(GizmoMode eMode)
{
	m_eMode = eMode;

	// Reset interaction state when changing mode
	m_bIsInteracting = false;
	m_eActiveComponent = GizmoComponent::None;
	m_eHoveredComponent = GizmoComponent::None;
}

void Flux_GizmosImpl::BeginInteraction(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir)
{
	if (!m_pxTargetEntity)
	{
		Zenith_Log(LOG_CATEGORY_GIZMOS, "BeginInteraction: No target entity");
		return;
	}

	// Raycast against gizmo to find which component was clicked
	float fDistance;
	GizmoComponent eHitComponent = RaycastGizmo(rayOrigin, rayDir, fDistance);

	if (eHitComponent != GizmoComponent::None)
	{
		m_bIsInteracting = true;
		m_eActiveComponent = eHitComponent;
		m_xInteractionStartPos = rayOrigin + rayDir * fDistance;

		// Store initial entity transform
		Zenith_Entity* pxEntity = GetGizmoTargetWithTransform();
		if (pxEntity)
		{
			g_xGizmoTransformAccess.m_pfnGetPosition(pxEntity, m_xInitialEntityPosition);
			g_xGizmoTransformAccess.m_pfnGetRotation(pxEntity, m_xInitialEntityRotation);
			g_xGizmoTransformAccess.m_pfnGetScale(pxEntity, m_xInitialEntityScale);
		}
	}
}

void Flux_GizmosImpl::UpdateInteraction(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir)
{
	if (!m_bIsInteracting || !m_pxTargetEntity)
		return;

	// Apply transformation based on gizmo mode
	switch (m_eMode)
	{
		case GizmoMode::Translate: ApplyTranslation(rayOrigin, rayDir); break;
		case GizmoMode::Rotate: ApplyRotation(rayOrigin, rayDir); break;
		case GizmoMode::Scale: ApplyScale(rayOrigin, rayDir); break;
	}
}

void Flux_GizmosImpl::EndInteraction()
{
	m_bIsInteracting = false;
	m_eActiveComponent = GizmoComponent::None;
}

// ==================== GEOMETRY GENERATION ====================

void Flux_GizmosImpl::GenerateTranslationGizmoGeometry()
{
	// X axis (Red)
	GenerateArrowGeometry(m_xTranslateGeometry, Zenith_Maths::Vector3(1, 0, 0), Zenith_Maths::Vector3(1, 0, 0), GizmoComponent::TranslateX);
	// Y axis (Green)
	GenerateArrowGeometry(m_xTranslateGeometry, Zenith_Maths::Vector3(0, 1, 0), Zenith_Maths::Vector3(0, 1, 0), GizmoComponent::TranslateY);
	// Z axis (Blue)
	GenerateArrowGeometry(m_xTranslateGeometry, Zenith_Maths::Vector3(0, 0, 1), Zenith_Maths::Vector3(0, 0, 1), GizmoComponent::TranslateZ);
}

void Flux_GizmosImpl::GenerateRotationGizmoGeometry()
{
	// X axis circle (Red)
	GenerateCircleGeometry(m_xRotateGeometry, Zenith_Maths::Vector3(1, 0, 0), Zenith_Maths::Vector3(1, 0, 0), GizmoComponent::RotateX);
	// Y axis circle (Green)
	GenerateCircleGeometry(m_xRotateGeometry, Zenith_Maths::Vector3(0, 1, 0), Zenith_Maths::Vector3(0, 1, 0), GizmoComponent::RotateY);
	// Z axis circle (Blue)
	GenerateCircleGeometry(m_xRotateGeometry, Zenith_Maths::Vector3(0, 0, 1), Zenith_Maths::Vector3(0, 0, 1), GizmoComponent::RotateZ);
}

void Flux_GizmosImpl::GenerateScaleGizmoGeometry()
{
	// X axis (Red)
	GenerateArrowGeometry(m_xScaleGeometry, Zenith_Maths::Vector3(1, 0, 0), Zenith_Maths::Vector3(1, 0, 0), GizmoComponent::ScaleX);
	// Y axis (Green)
	GenerateArrowGeometry(m_xScaleGeometry, Zenith_Maths::Vector3(0, 1, 0), Zenith_Maths::Vector3(0, 1, 0), GizmoComponent::ScaleY);
	// Z axis (Blue)
	GenerateArrowGeometry(m_xScaleGeometry, Zenith_Maths::Vector3(0, 0, 1), Zenith_Maths::Vector3(0, 0, 1), GizmoComponent::ScaleZ);
	// Center cube for uniform scale (White)
	GenerateCubeGeometry(m_xScaleGeometry, Zenith_Maths::Vector3(0, 0, 0), Zenith_Maths::Vector3(1, 1, 1), GizmoComponent::ScaleXYZ);
}

void Flux_GizmosImpl::GenerateArrowGeometry(Zenith_Vector<Flux_GizmosImpl::GizmoGeometry>& geometryList, const Zenith_Maths::Vector3& axis, const Zenith_Maths::Vector3& color, GizmoComponent component)
{
	Zenith_Vector<Zenith_Maths::Vector3> positions;
	Zenith_Vector<Zenith_Maths::Vector3> colors;
	Zenith_Vector<uint32_t> indices;

	// Create arrow shaft (thin cylinder)
	const uint32_t shaftSegments = 8;
	Zenith_Maths::Vector3 tangent, bitangent;
	ComputeTangentFrame(axis, tangent, bitangent);

	// Shaft vertices
	for (uint32_t i = 0; i <= shaftSegments; ++i)
	{
		float angle = (float)i / shaftSegments * 2.0f * 3.14159f;
		Zenith_Maths::Vector3 offset = tangent * cosf(angle) * GIZMO_ARROW_SHAFT_RADIUS + bitangent * sinf(angle) * GIZMO_ARROW_SHAFT_RADIUS;

		positions.PushBack(offset);  // Base
		colors.PushBack(color);
		positions.PushBack(axis * (GIZMO_ARROW_LENGTH - GIZMO_ARROW_HEAD_LENGTH) + offset);  // Top
		colors.PushBack(color);
	}

	// Shaft indices
	for (uint32_t i = 0; i < shaftSegments; ++i)
	{
		uint32_t base = i * 2;
		indices.PushBack(base);
		indices.PushBack(base + 1);
		indices.PushBack(base + 3);

		indices.PushBack(base);
		indices.PushBack(base + 3);
		indices.PushBack(base + 2);
	}

	// Arrow head (cone)
	uint32_t headBaseIndex = positions.GetSize();
	Zenith_Maths::Vector3 headBase = axis * (GIZMO_ARROW_LENGTH - GIZMO_ARROW_HEAD_LENGTH);
	Zenith_Maths::Vector3 headTip = axis * GIZMO_ARROW_LENGTH;

	for (uint32_t i = 0; i <= shaftSegments; ++i)
	{
		float angle = (float)i / shaftSegments * 2.0f * 3.14159f;
		Zenith_Maths::Vector3 offset = tangent * cosf(angle) * GIZMO_ARROW_HEAD_RADIUS + bitangent * sinf(angle) * GIZMO_ARROW_HEAD_RADIUS;

		positions.PushBack(headBase + offset);
		colors.PushBack(color);
	}

	positions.PushBack(headTip);
	colors.PushBack(color);

	// Head indices
	for (uint32_t i = 0; i < shaftSegments; ++i)
	{
		indices.PushBack(headBaseIndex + i);
		indices.PushBack(headBaseIndex + positions.GetSize() - 1);  // Tip
		indices.PushBack(headBaseIndex + i + 1);
	}

	// Interleave and upload
	Zenith_Vector<float> xVertexData;
	InterleaveVertexData(xVertexData, positions, colors);
	UploadGizmoGeometry(geometryList, xVertexData, indices, color, component);
}

void Flux_GizmosImpl::GenerateCircleGeometry(Zenith_Vector<Flux_GizmosImpl::GizmoGeometry>& geometryList, const Zenith_Maths::Vector3& normal, const Zenith_Maths::Vector3& color, GizmoComponent component)
{
	Zenith_Vector<Zenith_Maths::Vector3> positions;
	Zenith_Vector<Zenith_Maths::Vector3> colors;
	Zenith_Vector<uint32_t> indices;

	// Find perpendicular vectors for the circle plane
	Zenith_Maths::Vector3 tangent, bitangent;
	ComputeTangentFrame(normal, tangent, bitangent);

	// FIXED: Generate circle as a 3D tube/ribbon with actual triangle geometry
	// Create two rings (inner and outer) to form a visible tube
	const float tubeThickness = 0.02f;  // Thickness of the tube in local space

	for (uint32_t i = 0; i < GIZMO_CIRCLE_SEGMENTS; ++i)
	{
		float angle = (float)i / GIZMO_CIRCLE_SEGMENTS * 2.0f * 3.14159f;

		// Position on the circle
		Zenith_Maths::Vector3 circlePos = tangent * cosf(angle) * GIZMO_CIRCLE_RADIUS + bitangent * sinf(angle) * GIZMO_CIRCLE_RADIUS;

		// Radial direction for tube thickness
		Zenith_Maths::Vector3 radialDir = glm::normalize(circlePos);

		// Create inner and outer vertices
		Zenith_Maths::Vector3 innerPos = circlePos - radialDir * tubeThickness;
		Zenith_Maths::Vector3 outerPos = circlePos + radialDir * tubeThickness;

		positions.PushBack(innerPos);
		colors.PushBack(color);
		positions.PushBack(outerPos);
		colors.PushBack(color);
	}

	// Generate quad indices (two triangles per segment)
	for (uint32_t i = 0; i < GIZMO_CIRCLE_SEGMENTS; ++i)
	{
		uint32_t baseIdx = i * 2;
		uint32_t nextBaseIdx = ((i + 1) % GIZMO_CIRCLE_SEGMENTS) * 2;

		// First triangle of quad
		indices.PushBack(baseIdx);          // Inner current
		indices.PushBack(baseIdx + 1);      // Outer current
		indices.PushBack(nextBaseIdx);      // Inner next

		// Second triangle of quad
		indices.PushBack(baseIdx + 1);      // Outer current
		indices.PushBack(nextBaseIdx + 1);  // Outer next
		indices.PushBack(nextBaseIdx);      // Inner next
	}

	// Interleave and upload
	Zenith_Vector<float> xVertexData;
	InterleaveVertexData(xVertexData, positions, colors);
	UploadGizmoGeometry(geometryList, xVertexData, indices, color, component);
}

void Flux_GizmosImpl::GenerateCubeGeometry(Zenith_Vector<Flux_GizmosImpl::GizmoGeometry>& geometryList, const Zenith_Maths::Vector3& offset, const Zenith_Maths::Vector3& color, GizmoComponent component)
{
	Zenith_Vector<Zenith_Maths::Vector3> positions;
	Zenith_Vector<Zenith_Maths::Vector3> colors;
	Zenith_Vector<uint32_t> indices;

	float half = GIZMO_CUBE_SIZE * 0.5f;

	// 8 cube vertices
	Zenith_Maths::Vector3 verts[8] = {
		offset + Zenith_Maths::Vector3(-half, -half, -half),
		offset + Zenith_Maths::Vector3( half, -half, -half),
		offset + Zenith_Maths::Vector3( half,  half, -half),
		offset + Zenith_Maths::Vector3(-half,  half, -half),
		offset + Zenith_Maths::Vector3(-half, -half,  half),
		offset + Zenith_Maths::Vector3( half, -half,  half),
		offset + Zenith_Maths::Vector3( half,  half,  half),
		offset + Zenith_Maths::Vector3(-half,  half,  half)
	};

	for (int i = 0; i < 8; ++i)
	{
		positions.PushBack(verts[i]);
		colors.PushBack(color);
	}

	// 12 triangles (2 per face, 6 faces)
	uint32_t cubeIndices[] = {
		0, 1, 2, 0, 2, 3,  // Front
		1, 5, 6, 1, 6, 2,  // Right
		5, 4, 7, 5, 7, 6,  // Back
		4, 0, 3, 4, 3, 7,  // Left
		3, 2, 6, 3, 6, 7,  // Top
		4, 5, 1, 4, 1, 0   // Bottom
	};

	for (uint32_t i = 0; i < 36; ++i)
		indices.PushBack(cubeIndices[i]);

	// Interleave and upload
	Zenith_Vector<float> xVertexData;
	InterleaveVertexData(xVertexData, positions, colors);
	UploadGizmoGeometry(geometryList, xVertexData, indices, color, component);
}

// ==================== RAYCASTING ====================

// Test a single cylindrical axis handle (translate or scale). Returns true if
// the ray hits AND the distance beats the current best; updates the best.
static bool TryIntersectAxisHandle(const Zenith_Maths::Vector3& xRelOrigin, const Zenith_Maths::Vector3& xRayDir,
                                   const Zenith_Maths::Vector3& xAxis, float fThreshold, float fLength,
                                   GizmoComponent eComponent, float& fClosest, GizmoComponent& eClosest)
{
	float fDist;
	if (!Zenith_Maths::Intersections::RayIntersectsCylinder(xRelOrigin, xRayDir, xAxis, fThreshold, fLength, fDist))
		return false;
	if (fDist >= fClosest)
		return false;
	fClosest = fDist;
	eClosest = eComponent;
	return true;
}

// Test all three translate/scale axis handles (plus the scale-mode centre
// cube when eMode is Scale) and return the closest hit.
static void IntersectTranslateOrScaleAxes(GizmoMode eMode,
                                          const Zenith_Maths::Vector3& xRelOrigin, const Zenith_Maths::Vector3& xRayDir,
                                          float fArrowLength, float fThreshold, float fCubeSize,
                                          float& fClosest, GizmoComponent& eClosest)
{
	const bool bScale = (eMode == GizmoMode::Scale);
	TryIntersectAxisHandle(xRelOrigin, xRayDir, Zenith_Maths::Vector3(1, 0, 0), fThreshold, fArrowLength, bScale ? GizmoComponent::ScaleX : GizmoComponent::TranslateX, fClosest, eClosest);
	TryIntersectAxisHandle(xRelOrigin, xRayDir, Zenith_Maths::Vector3(0, 1, 0), fThreshold, fArrowLength, bScale ? GizmoComponent::ScaleY : GizmoComponent::TranslateY, fClosest, eClosest);
	TryIntersectAxisHandle(xRelOrigin, xRayDir, Zenith_Maths::Vector3(0, 0, 1), fThreshold, fArrowLength, bScale ? GizmoComponent::ScaleZ : GizmoComponent::TranslateZ, fClosest, eClosest);

	if (bScale)
	{
		float fDist;
		if (Zenith_Maths::Intersections::RayIntersectsAABB(xRelOrigin, xRayDir, Zenith_Maths::Vector3(0, 0, 0), fCubeSize, fDist) && fDist < fClosest)
		{
			fClosest = fDist;
			eClosest = GizmoComponent::ScaleXYZ;
		}
	}
}

// Test the three rotation rings and return the closest hit.
static void IntersectRotateRings(const Zenith_Maths::Vector3& xRelOrigin, const Zenith_Maths::Vector3& xRayDir,
                                 float fRadius, float fThreshold,
                                 float& fClosest, GizmoComponent& eClosest)
{
	struct RingAxis { Zenith_Maths::Vector3 xAxis; GizmoComponent eComponent; };
	const RingAxis axRings[] = {
		{ Zenith_Maths::Vector3(1, 0, 0), GizmoComponent::RotateX },
		{ Zenith_Maths::Vector3(0, 1, 0), GizmoComponent::RotateY },
		{ Zenith_Maths::Vector3(0, 0, 1), GizmoComponent::RotateZ },
	};
	for (const RingAxis& xRing : axRings)
	{
		float fDist;
		if (Zenith_Maths::Intersections::RayIntersectsCircle(xRelOrigin, xRayDir, xRing.xAxis, fRadius, fThreshold, fDist) && fDist < fClosest)
		{
			fClosest = fDist;
			eClosest = xRing.eComponent;
		}
	}
}

GizmoComponent Flux_GizmosImpl::RaycastGizmo(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir, float& outDistance)
{
	Zenith_Entity* pxEntity = GetGizmoTargetWithTransform();
	if (!pxEntity)
		return GizmoComponent::None;

	Zenith_Maths::Vector3 xGizmoPos;
	g_xGizmoTransformAccess.m_pfnGetPosition(pxEntity, xGizmoPos);

	// FIXED: Do all calculations in WORLD space
	// Translate ray origin relative to gizmo center, but keep same units
	Zenith_Maths::Vector3 relativeRayOrigin = rayOrigin - xGizmoPos;

	// Scale thresholds and lengths to world space
	const float worldArrowLength = GIZMO_ARROW_LENGTH * m_fGizmoScale * GIZMO_INTERACTION_LENGTH_MULTIPLIER;
	const float worldInteractionThreshold = GIZMO_INTERACTION_THRESHOLD * m_fGizmoScale;
	const float worldCircleRadius = GIZMO_CIRCLE_RADIUS * m_fGizmoScale;
	const float worldCubeSize = GIZMO_CUBE_SIZE * m_fGizmoScale;

	float closestDistance = FLT_MAX;
	GizmoComponent closestComponent = GizmoComponent::None;

	if (m_eMode == GizmoMode::Translate || m_eMode == GizmoMode::Scale)
	{
		IntersectTranslateOrScaleAxes(m_eMode, relativeRayOrigin, rayDir,
			worldArrowLength, worldInteractionThreshold, worldCubeSize,
			closestDistance, closestComponent);
	}
	else if (m_eMode == GizmoMode::Rotate)
	{
		IntersectRotateRings(relativeRayOrigin, rayDir,
			worldCircleRadius, worldInteractionThreshold,
			closestDistance, closestComponent);
	}

	outDistance = closestDistance;
	return closestComponent;
}

// ==================== TRANSFORM MANIPULATION ====================

void Flux_GizmosImpl::ApplyTranslation(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir)
{
	Zenith_Entity* pxEntity = GetGizmoTargetWithTransform();
	if (!pxEntity)
		return;

	// Get constraint axis
	Zenith_Maths::Vector3 axis(0, 0, 0);
	switch (m_eActiveComponent)
	{
		case GizmoComponent::TranslateX: axis = Zenith_Maths::Vector3(1, 0, 0); break;
		case GizmoComponent::TranslateY: axis = Zenith_Maths::Vector3(0, 1, 0); break;
		case GizmoComponent::TranslateZ: axis = Zenith_Maths::Vector3(0, 0, 1); break;
		default: return;
	}

	// Track offset from initial click and maintain it.
	// Find the closest point on the constraint axis to BOTH the initial click and current ray;
	// the difference determines how much to move the entity.

	// First: Project initial click onto axis
	Zenith_Maths::Vector3 offsetToClick = m_xInteractionStartPos - m_xInitialEntityPosition;
	float fInitialT = glm::dot(offsetToClick, axis);

	// Second: Find closest point on axis to the current mouse ray
	float fCurrentT;
	if (!GetLineLineClosestPointParameter(m_xInitialEntityPosition, axis, rayOrigin, rayDir, fCurrentT))
	{
		return;
	}

	// Apply the movement
	float fDeltaT = fCurrentT - fInitialT;
	Zenith_Maths::Vector3 xNewPosition = m_xInitialEntityPosition + axis * fDeltaT;

	g_xGizmoTransformAccess.m_pfnSetPosition(pxEntity, xNewPosition);
}

void Flux_GizmosImpl::ApplyRotation(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir)
{
	Zenith_Entity* pxEntity = GetGizmoTargetWithTransform();
	if (!pxEntity)
		return;


	// Get rotation axis
	Zenith_Maths::Vector3 axis(0, 0, 0);
	switch (m_eActiveComponent)
	{
		case GizmoComponent::RotateX: axis = Zenith_Maths::Vector3(1, 0, 0); break;
		case GizmoComponent::RotateY: axis = Zenith_Maths::Vector3(0, 1, 0); break;
		case GizmoComponent::RotateZ: axis = Zenith_Maths::Vector3(0, 0, 1); break;
		default: return;
	}

	// Calculate angle from initial interaction point to current ray
	// Intersect ray with rotation plane
	float denom = glm::dot(axis, rayDir);
	if (fabsf(denom) < 0.0001f)
		return;

	Zenith_Maths::Vector3 planePoint = m_xInitialEntityPosition;
	float t = glm::dot(axis, planePoint - rayOrigin) / denom;
	if (t < 0.0f)
		return;

	Zenith_Maths::Vector3 currentPoint = rayOrigin + rayDir * t;

	// Calculate angle
	Zenith_Maths::Vector3 initialVec = glm::normalize(m_xInteractionStartPos - m_xInitialEntityPosition);
	Zenith_Maths::Vector3 currentVec = glm::normalize(currentPoint - m_xInitialEntityPosition);

	float angle = acosf(glm::clamp(glm::dot(initialVec, currentVec), -1.0f, 1.0f));
	Zenith_Maths::Vector3 cross = glm::cross(initialVec, currentVec);
	if (glm::dot(cross, axis) < 0.0f)
		angle = -angle;

	// Apply rotation
	Zenith_Maths::Quaternion deltaRotation = glm::angleAxis(angle, axis);
	Zenith_Maths::Quaternion newRotation = deltaRotation * m_xInitialEntityRotation;
	g_xGizmoTransformAccess.m_pfnSetRotation(pxEntity, newRotation);
}

void Flux_GizmosImpl::ApplyScale(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir)
{
	Zenith_Entity* pxEntity = GetGizmoTargetWithTransform();
	if (!pxEntity)
		return;

	// Get constraint axis
	Zenith_Maths::Vector3 axis(0, 0, 0);
	bool bUniformScale = false;

	switch (m_eActiveComponent)
	{
		case GizmoComponent::ScaleX: axis = Zenith_Maths::Vector3(1, 0, 0); break;
		case GizmoComponent::ScaleY: axis = Zenith_Maths::Vector3(0, 1, 0); break;
		case GizmoComponent::ScaleZ: axis = Zenith_Maths::Vector3(0, 0, 1); break;
		case GizmoComponent::ScaleXYZ:
			axis = Zenith_Maths::Vector3(1, 1, 1);
			bUniformScale = true;
			break;
		default: return;
	}

	// For uniform scale, use the camera view direction as the constraint axis
	if (bUniformScale)
	{
		Zenith_Maths::Vector3 xCameraPos = m_pxFluxGraphics->GetCameraPosition();
		axis = glm::normalize(m_xInitialEntityPosition - xCameraPos);
	}

	// Project initial click onto axis
	Zenith_Maths::Vector3 offsetToClick = m_xInteractionStartPos - m_xInitialEntityPosition;
	float fInitialT = glm::dot(offsetToClick, axis);

	// Find closest point on axis to the current mouse ray
	float fCurrentT;
	if (!GetLineLineClosestPointParameter(m_xInitialEntityPosition, axis, rayOrigin, rayDir, fCurrentT))
		return;

	// Calculate scale factor based on movement along axis
	float fDeltaT = fCurrentT - fInitialT;

	// Convert delta to scale factor
	const float fScaleSpeed = 0.5f;
	float fScaleFactor = 1.0f + (fDeltaT * fScaleSpeed);

	// Clamp to prevent negative or zero scale
	fScaleFactor = glm::max(fScaleFactor, 0.01f);

	// Apply scale based on active component
	Zenith_Maths::Vector3 xNewScale = m_xInitialEntityScale;

	if (bUniformScale)
	{
		xNewScale *= fScaleFactor;
	}
	else
	{
		switch (m_eActiveComponent)
		{
			case GizmoComponent::ScaleX: xNewScale.x *= fScaleFactor; break;
			case GizmoComponent::ScaleY: xNewScale.y *= fScaleFactor; break;
			case GizmoComponent::ScaleZ: xNewScale.z *= fScaleFactor; break;
			default: return;
		}
	}

	g_xGizmoTransformAccess.m_pfnSetScale(pxEntity, xNewScale);
}

#endif // ZENITH_TOOLS

// Phase 7e: out-of-line accessor bodies.
