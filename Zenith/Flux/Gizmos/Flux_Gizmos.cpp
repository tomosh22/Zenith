#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "DebugVariables/Zenith_DebugVariables.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Flux/Gizmos/Flux_Gizmos.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Primitives/Flux_Primitives.h"
#include "Maths/Zenith_Maths_Intersections.h"
#include "TaskSystem/Zenith_TaskSystem.h"

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
DEBUGVAR bool dbg_bRenderGizmos = true;
DEBUGVAR float dbg_fGizmoAlpha = 1.0f;

// Static member initialization
Zenith_Entity* Flux_Gizmos::s_pxTargetEntity = nullptr;
GizmoMode Flux_Gizmos::s_eMode = GizmoMode::Translate;
GizmoComponent Flux_Gizmos::s_eHoveredComponent = GizmoComponent::None;
GizmoComponent Flux_Gizmos::s_eActiveComponent = GizmoComponent::None;
bool Flux_Gizmos::s_bIsInteracting = false;

Zenith_Maths::Vector3 Flux_Gizmos::s_xInteractionStartPos(0, 0, 0);
Zenith_Maths::Vector3 Flux_Gizmos::s_xInitialEntityPosition(0, 0, 0);
Zenith_Maths::Quaternion Flux_Gizmos::s_xInitialEntityRotation(1, 0, 0, 0);
Zenith_Maths::Vector3 Flux_Gizmos::s_xInitialEntityScale(1, 1, 1);
float Flux_Gizmos::s_fGizmoScale = 1.0f;

Flux_Pipeline Flux_Gizmos::s_xPipeline;
Flux_Shader Flux_Gizmos::s_xShader;
Flux_CommandList Flux_Gizmos::s_xCommandList("Gizmos");

Zenith_Vector<Flux_Gizmos::GizmoGeometry> Flux_Gizmos::s_xTranslateGeometry;
Zenith_Vector<Flux_Gizmos::GizmoGeometry> Flux_Gizmos::s_xRotateGeometry;
Zenith_Vector<Flux_Gizmos::GizmoGeometry> Flux_Gizmos::s_xScaleGeometry;

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_GIZMOS, Flux_Gizmos::Render, nullptr);

void Flux_Gizmos::Initialise()
{
	// Load shaders
	s_xShader.Initialise("Gizmos/Flux_Gizmos.vert", "Gizmos/Flux_Gizmos.frag");

	// Create pipeline specification
	Flux_PipelineSpecification xSpec;
	xSpec.m_pxShader = &s_xShader;
	xSpec.m_pxTargetSetup = &Flux_Graphics::s_xFinalRenderTarget;  // Render to final target with depth

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

	// Pipeline layout - needs frame constants (in Common.fxh)
	Flux_PipelineLayout& xLayout = xSpec.m_xPipelineLayout;
	xLayout.m_uNumDescriptorSets = 1;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Frame constants

	xSpec.m_bUsePushConstants = true;

	// Build pipeline
	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xSpec);

	// Generate gizmo geometry
	GenerateTranslationGizmoGeometry();
	GenerateRotationGizmoGeometry();
	GenerateScaleGizmoGeometry();

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({"Editor", "Gizmos", "Render"}, dbg_bRenderGizmos);
	Zenith_DebugVariables::AddFloat({"Editor", "Gizmos", "Alpha"}, dbg_fGizmoAlpha, 0.0f, 1.0f);
#endif

	Zenith_Log(LOG_CATEGORY_GIZMOS, "Flux_Gizmos initialised");
}

void Flux_Gizmos::Shutdown()
{
	// Destroy GPU buffers for all gizmo geometry
	auto DestroyGeometryBuffers = [](Zenith_Vector<GizmoGeometry>& xGeometry)
	{
		for (uint32_t i = 0; i < xGeometry.GetSize(); ++i)
		{
			Flux_MemoryManager::DestroyVertexBuffer(xGeometry.Get(i).m_xVertexBuffer);
			Flux_MemoryManager::DestroyIndexBuffer(xGeometry.Get(i).m_xIndexBuffer);
		}
		xGeometry.Clear();
	};

	DestroyGeometryBuffers(s_xTranslateGeometry);
	DestroyGeometryBuffers(s_xRotateGeometry);
	DestroyGeometryBuffers(s_xScaleGeometry);

	Zenith_Log(LOG_CATEGORY_GIZMOS, "Flux_Gizmos shut down");
}

void Flux_Gizmos::Reset()
{
	// Reset command list to ensure no stale GPU resource references, including descriptor bindings
	// This is called when the scene is reset (e.g., Play/Stop transitions in editor)
	s_xCommandList.Reset(true);

	// Clear target entity reference (will be invalid after scene reset)
	s_pxTargetEntity = nullptr;

	// Reset interaction state
	s_eHoveredComponent = GizmoComponent::None;
	s_eActiveComponent = GizmoComponent::None;
	s_bIsInteracting = false;

	Zenith_Log(LOG_CATEGORY_GIZMOS, "Flux_Gizmos::Reset() - Reset command list and cleared entity reference");
}

void Flux_Gizmos::Render(void*)
{
	if (!dbg_bRenderGizmos || !s_pxTargetEntity)
	{
		return;
	}

	// Get entity transform
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	if (!xScene.EntityHasComponent<Zenith_TransformComponent>(s_pxTargetEntity->GetEntityID()))
	{
		Zenith_Log(LOG_CATEGORY_GIZMOS, "Gizmos: Entity has no transform component");
		return;
	}

	Zenith_TransformComponent& xTransform = xScene.GetComponentFromEntity<Zenith_TransformComponent>(s_pxTargetEntity->GetEntityID());

	// Calculate gizmo scale based on camera distance for consistent screen size
	Zenith_Maths::Vector3 xEntityPos;
	xTransform.GetPosition(xEntityPos);
	Zenith_Maths::Vector3 xCameraPos = Flux_Graphics::GetCameraPosition();
	float fDistance = glm::length(xEntityPos - xCameraPos);
	s_fGizmoScale = fDistance / GIZMO_AUTO_SCALE_DISTANCE;

	// Build gizmo transform matrix
	Zenith_Maths::Matrix4 xGizmoMatrix = glm::translate(Zenith_Maths::Matrix4(1.0f), xEntityPos);
	xGizmoMatrix = glm::scale(xGizmoMatrix, Zenith_Maths::Vector3(s_fGizmoScale));

	// Select geometry based on mode
	Zenith_Vector<GizmoGeometry>* pxGeometry = nullptr;
	switch (s_eMode)
	{
		case GizmoMode::Translate: pxGeometry = &s_xTranslateGeometry; break;
		case GizmoMode::Rotate: pxGeometry = &s_xRotateGeometry; break;
		case GizmoMode::Scale: pxGeometry = &s_xScaleGeometry; break;
	}

	if (!pxGeometry || pxGeometry->GetSize() == 0)
	{
		Zenith_Log(LOG_CATEGORY_GIZMOS, "Gizmos: No geometry - mode=%d, size=%d", s_eMode, pxGeometry ? pxGeometry->GetSize() : 0);
		return;
	}

	// Visualize gizmo interaction bounding boxes for debugging
	Flux_Primitives::AddWireframeCube(xEntityPos + Zenith_Maths::Vector3(1, 0, 0) * GIZMO_ARROW_LENGTH * 0.5f * s_fGizmoScale,
		Zenith_Maths::Vector3(GIZMO_ARROW_LENGTH * s_fGizmoScale * 0.5f, GIZMO_INTERACTION_THRESHOLD * s_fGizmoScale, GIZMO_INTERACTION_THRESHOLD * s_fGizmoScale),
		Zenith_Maths::Vector3(1, 0, 0));
	Flux_Primitives::AddWireframeCube(xEntityPos + Zenith_Maths::Vector3(0, 1, 0) * GIZMO_ARROW_LENGTH * 0.5f * s_fGizmoScale,
		Zenith_Maths::Vector3(GIZMO_INTERACTION_THRESHOLD * s_fGizmoScale, GIZMO_ARROW_LENGTH * s_fGizmoScale * 0.5f, GIZMO_INTERACTION_THRESHOLD * s_fGizmoScale),
		Zenith_Maths::Vector3(0, 1, 0));
	Flux_Primitives::AddWireframeCube(xEntityPos + Zenith_Maths::Vector3(0, 0, 1) * GIZMO_ARROW_LENGTH * 0.5f * s_fGizmoScale,
		Zenith_Maths::Vector3(GIZMO_INTERACTION_THRESHOLD * s_fGizmoScale, GIZMO_INTERACTION_THRESHOLD * s_fGizmoScale, GIZMO_ARROW_LENGTH * s_fGizmoScale * 0.5f),
		Zenith_Maths::Vector3(0, 0, 1));

	// Record rendering commands
	s_xCommandList.Reset(false);
	s_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPipeline);

	// Bind frame constants (set 0, binding 0)
	s_xCommandList.AddCommand<Flux_CommandBeginBind>(0);
	s_xCommandList.AddCommand<Flux_CommandBindCBV>(&Flux_Graphics::s_xFrameConstantsBuffer.GetCBV(), 0);

	// Render each gizmo component
	for (uint32_t i = 0; i < pxGeometry->GetSize(); ++i)
	{
		const GizmoGeometry& xGeom = pxGeometry->Get(i);

		// Set vertex and index buffers
		s_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&xGeom.m_xVertexBuffer);
		s_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&xGeom.m_xIndexBuffer);

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
		if (xGeom.m_eComponent == s_eHoveredComponent && !s_bIsInteracting)
			xPushConstants.m_fHighlightIntensity = 0.5f;
		else if (xGeom.m_eComponent == s_eActiveComponent && s_bIsInteracting)
			xPushConstants.m_fHighlightIntensity = 1.0f;

		s_xCommandList.AddCommand<Flux_CommandPushConstant>(&xPushConstants, sizeof(xPushConstants));

		// Draw
		s_xCommandList.AddCommand<Flux_CommandDrawIndexed>(xGeom.m_uIndexCount);
	}

	// Submit to rendering pass (after scene but before UI)
	Flux::SubmitCommandList(&s_xCommandList, Flux_Graphics::s_xFinalRenderTarget, RENDER_ORDER_TEXT);
}

void Flux_Gizmos::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_Gizmos::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

void Flux_Gizmos::SetTargetEntity(Zenith_Entity* pxEntity)
{
	s_pxTargetEntity = pxEntity;

	// Reset interaction state when changing target
	s_bIsInteracting = false;
	s_eActiveComponent = GizmoComponent::None;
	s_eHoveredComponent = GizmoComponent::None;
}

void Flux_Gizmos::SetGizmoMode(GizmoMode eMode)
{
	s_eMode = eMode;

	// Reset interaction state when changing mode
	s_bIsInteracting = false;
	s_eActiveComponent = GizmoComponent::None;
	s_eHoveredComponent = GizmoComponent::None;
}

void Flux_Gizmos::BeginInteraction(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir)
{
	if (!s_pxTargetEntity)
	{
		Zenith_Log(LOG_CATEGORY_GIZMOS, "BeginInteraction: No target entity");
		return;
	}

	// Raycast against gizmo to find which component was clicked
	float fDistance;
	GizmoComponent eHitComponent = RaycastGizmo(rayOrigin, rayDir, fDistance);

	if (eHitComponent != GizmoComponent::None)
	{
		s_bIsInteracting = true;
		s_eActiveComponent = eHitComponent;
		s_xInteractionStartPos = rayOrigin + rayDir * fDistance;

		// Store initial entity transform
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		if (xScene.EntityHasComponent<Zenith_TransformComponent>(s_pxTargetEntity->GetEntityID()))
		{
			Zenith_TransformComponent& xTransform = xScene.GetComponentFromEntity<Zenith_TransformComponent>(s_pxTargetEntity->GetEntityID());
			xTransform.GetPosition(s_xInitialEntityPosition);
			xTransform.GetRotation(s_xInitialEntityRotation);
			xTransform.GetScale(s_xInitialEntityScale);
			}
	}
}

void Flux_Gizmos::UpdateInteraction(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir)
{
	if (!s_bIsInteracting || !s_pxTargetEntity)
		return;

	// Apply transformation based on gizmo mode
	switch (s_eMode)
	{
		case GizmoMode::Translate: ApplyTranslation(rayOrigin, rayDir); break;
		case GizmoMode::Rotate: ApplyRotation(rayOrigin, rayDir); break;
		case GizmoMode::Scale: ApplyScale(rayOrigin, rayDir); break;
	}
}

void Flux_Gizmos::EndInteraction()
{
	s_bIsInteracting = false;
	s_eActiveComponent = GizmoComponent::None;
}

// ==================== GEOMETRY GENERATION ====================

void Flux_Gizmos::GenerateTranslationGizmoGeometry()
{
	// X axis (Red)
	GenerateArrowGeometry(s_xTranslateGeometry, Zenith_Maths::Vector3(1, 0, 0), Zenith_Maths::Vector3(1, 0, 0), GizmoComponent::TranslateX);
	// Y axis (Green)
	GenerateArrowGeometry(s_xTranslateGeometry, Zenith_Maths::Vector3(0, 1, 0), Zenith_Maths::Vector3(0, 1, 0), GizmoComponent::TranslateY);
	// Z axis (Blue)
	GenerateArrowGeometry(s_xTranslateGeometry, Zenith_Maths::Vector3(0, 0, 1), Zenith_Maths::Vector3(0, 0, 1), GizmoComponent::TranslateZ);
}

void Flux_Gizmos::GenerateRotationGizmoGeometry()
{
	// X axis circle (Red)
	GenerateCircleGeometry(s_xRotateGeometry, Zenith_Maths::Vector3(1, 0, 0), Zenith_Maths::Vector3(1, 0, 0), GizmoComponent::RotateX);
	// Y axis circle (Green)
	GenerateCircleGeometry(s_xRotateGeometry, Zenith_Maths::Vector3(0, 1, 0), Zenith_Maths::Vector3(0, 1, 0), GizmoComponent::RotateY);
	// Z axis circle (Blue)
	GenerateCircleGeometry(s_xRotateGeometry, Zenith_Maths::Vector3(0, 0, 1), Zenith_Maths::Vector3(0, 0, 1), GizmoComponent::RotateZ);
}

void Flux_Gizmos::GenerateScaleGizmoGeometry()
{
	// X axis (Red)
	GenerateArrowGeometry(s_xScaleGeometry, Zenith_Maths::Vector3(1, 0, 0), Zenith_Maths::Vector3(1, 0, 0), GizmoComponent::ScaleX);
	// Y axis (Green)
	GenerateArrowGeometry(s_xScaleGeometry, Zenith_Maths::Vector3(0, 1, 0), Zenith_Maths::Vector3(0, 1, 0), GizmoComponent::ScaleY);
	// Z axis (Blue)
	GenerateArrowGeometry(s_xScaleGeometry, Zenith_Maths::Vector3(0, 0, 1), Zenith_Maths::Vector3(0, 0, 1), GizmoComponent::ScaleZ);
	// Center cube for uniform scale (White)
	GenerateCubeGeometry(s_xScaleGeometry, Zenith_Maths::Vector3(0, 0, 0), Zenith_Maths::Vector3(1, 1, 1), GizmoComponent::ScaleXYZ);
}

void Flux_Gizmos::GenerateArrowGeometry(Zenith_Vector<GizmoGeometry>& geometryList, const Zenith_Maths::Vector3& axis, const Zenith_Maths::Vector3& color, GizmoComponent component)
{
	Zenith_Vector<Zenith_Maths::Vector3> positions;
	Zenith_Vector<Zenith_Maths::Vector3> colors;
	Zenith_Vector<uint32_t> indices;

	// Create arrow shaft (thin cylinder)
	const uint32_t shaftSegments = 8;
	Zenith_Maths::Vector3 perpendicular = glm::abs(axis.x) > 0.9f ? Zenith_Maths::Vector3(0, 1, 0) : Zenith_Maths::Vector3(1, 0, 0);
	Zenith_Maths::Vector3 tangent = glm::normalize(glm::cross(axis, perpendicular));
	Zenith_Maths::Vector3 bitangent = glm::cross(axis, tangent);

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

	// Create GPU buffers
	GizmoGeometry geom;
	geom.m_eComponent = component;
	geom.m_xColor = color;
	geom.m_uIndexCount = indices.GetSize();

	// Interleave position and color data
	Zenith_Vector<float> vertexData;
	for (uint32_t i = 0; i < positions.GetSize(); ++i)
	{
		vertexData.PushBack(positions.Get(i).x);
		vertexData.PushBack(positions.Get(i).y);
		vertexData.PushBack(positions.Get(i).z);
		vertexData.PushBack(colors.Get(i).x);
		vertexData.PushBack(colors.Get(i).y);
		vertexData.PushBack(colors.Get(i).z);
	}

	// Create vertex buffer
	Flux_MemoryManager::InitialiseVertexBuffer(
		vertexData.GetDataPointer(),
		vertexData.GetSize() * sizeof(float),
		geom.m_xVertexBuffer
	);

	// Create index buffer
	Flux_MemoryManager::InitialiseIndexBuffer(
		indices.GetDataPointer(),
		indices.GetSize() * sizeof(uint32_t),
		geom.m_xIndexBuffer
	);

	geometryList.PushBack(geom);
}

void Flux_Gizmos::GenerateCircleGeometry(Zenith_Vector<GizmoGeometry>& geometryList, const Zenith_Maths::Vector3& normal, const Zenith_Maths::Vector3& color, GizmoComponent component)
{
	Zenith_Vector<Zenith_Maths::Vector3> positions;
	Zenith_Vector<Zenith_Maths::Vector3> colors;
	Zenith_Vector<uint32_t> indices;

	// Find perpendicular vectors for the circle plane
	Zenith_Maths::Vector3 perpendicular = glm::abs(normal.x) > 0.9f ? Zenith_Maths::Vector3(0, 1, 0) : Zenith_Maths::Vector3(1, 0, 0);
	Zenith_Maths::Vector3 tangent = glm::normalize(glm::cross(normal, perpendicular));
	Zenith_Maths::Vector3 bitangent = glm::cross(normal, tangent);

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

	// Create GPU buffers
	GizmoGeometry geom;
	geom.m_eComponent = component;
	geom.m_xColor = color;
	geom.m_uIndexCount = indices.GetSize();

	// Interleave position and color data
	Zenith_Vector<float> vertexData;
	for (uint32_t i = 0; i < positions.GetSize(); ++i)
	{
		vertexData.PushBack(positions.Get(i).x);
		vertexData.PushBack(positions.Get(i).y);
		vertexData.PushBack(positions.Get(i).z);
		vertexData.PushBack(colors.Get(i).x);
		vertexData.PushBack(colors.Get(i).y);
		vertexData.PushBack(colors.Get(i).z);
	}

	Flux_MemoryManager::InitialiseVertexBuffer(
		vertexData.GetDataPointer(),
		vertexData.GetSize() * sizeof(float),
		geom.m_xVertexBuffer
	);

	Flux_MemoryManager::InitialiseIndexBuffer(
		indices.GetDataPointer(),
		indices.GetSize() * sizeof(uint32_t),
		geom.m_xIndexBuffer
	);

	geometryList.PushBack(geom);
}

void Flux_Gizmos::GenerateCubeGeometry(Zenith_Vector<GizmoGeometry>& geometryList, const Zenith_Maths::Vector3& offset, const Zenith_Maths::Vector3& color, GizmoComponent component)
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

	// Create GPU buffers
	GizmoGeometry geom;
	geom.m_eComponent = component;
	geom.m_xColor = color;
	geom.m_uIndexCount = indices.GetSize();

	Zenith_Vector<float> vertexData;
	for (uint32_t i = 0; i < positions.GetSize(); ++i)
	{
		vertexData.PushBack(positions.Get(i).x);
		vertexData.PushBack(positions.Get(i).y);
		vertexData.PushBack(positions.Get(i).z);
		vertexData.PushBack(colors.Get(i).x);
		vertexData.PushBack(colors.Get(i).y);
		vertexData.PushBack(colors.Get(i).z);
	}

	Flux_MemoryManager::InitialiseVertexBuffer(
		vertexData.GetDataPointer(),
		vertexData.GetSize() * sizeof(float),
		geom.m_xVertexBuffer
	);

	Flux_MemoryManager::InitialiseIndexBuffer(
		indices.GetDataPointer(),
		indices.GetSize() * sizeof(uint32_t),
		geom.m_xIndexBuffer
	);

	geometryList.PushBack(geom);
}

// ==================== RAYCASTING ====================

GizmoComponent Flux_Gizmos::RaycastGizmo(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir, float& outDistance)
{
	if (!s_pxTargetEntity)
		return GizmoComponent::None;

	// Get entity position (gizmo center)
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	if (!xScene.EntityHasComponent<Zenith_TransformComponent>(s_pxTargetEntity->GetEntityID()))
		return GizmoComponent::None;

	Zenith_TransformComponent& xTransform = xScene.GetComponentFromEntity<Zenith_TransformComponent>(s_pxTargetEntity->GetEntityID());
	Zenith_Maths::Vector3 xGizmoPos;
	xTransform.GetPosition(xGizmoPos);

	// FIXED: Do all calculations in WORLD space
	// Translate ray origin relative to gizmo center, but keep same units
	Zenith_Maths::Vector3 relativeRayOrigin = rayOrigin - xGizmoPos;

	// Scale thresholds and lengths to world space
	float worldArrowLength = GIZMO_ARROW_LENGTH * s_fGizmoScale * GIZMO_INTERACTION_LENGTH_MULTIPLIER;
	float worldInteractionThreshold = GIZMO_INTERACTION_THRESHOLD * s_fGizmoScale;
	float worldCircleRadius = GIZMO_CIRCLE_RADIUS * s_fGizmoScale;
	float worldCubeSize = GIZMO_CUBE_SIZE * s_fGizmoScale;

	float closestDistance = FLT_MAX;
	GizmoComponent closestComponent = GizmoComponent::None;

	// Test against axes based on mode
	if (s_eMode == GizmoMode::Translate || s_eMode == GizmoMode::Scale)
	{
		// Test X axis
		float dist;
		if (Zenith_Maths::Intersections::RayIntersectsCylinder(relativeRayOrigin, rayDir, Zenith_Maths::Vector3(1, 0, 0), worldInteractionThreshold, worldArrowLength, dist) && dist < closestDistance)
		{
			closestDistance = dist;
			closestComponent = (s_eMode == GizmoMode::Translate) ? GizmoComponent::TranslateX : GizmoComponent::ScaleX;
		}
		// Test Y axis
		if (Zenith_Maths::Intersections::RayIntersectsCylinder(relativeRayOrigin, rayDir, Zenith_Maths::Vector3(0, 1, 0), worldInteractionThreshold, worldArrowLength, dist) && dist < closestDistance)
		{
			closestDistance = dist;
			closestComponent = (s_eMode == GizmoMode::Translate) ? GizmoComponent::TranslateY : GizmoComponent::ScaleY;
		}
		// Test Z axis
		if (Zenith_Maths::Intersections::RayIntersectsCylinder(relativeRayOrigin, rayDir, Zenith_Maths::Vector3(0, 0, 1), worldInteractionThreshold, worldArrowLength, dist) && dist < closestDistance)
		{
			closestDistance = dist;
			closestComponent = (s_eMode == GizmoMode::Translate) ? GizmoComponent::TranslateZ : GizmoComponent::ScaleZ;
		}

		// Test center cube for scale mode
		if (s_eMode == GizmoMode::Scale)
		{
			if (Zenith_Maths::Intersections::RayIntersectsAABB(relativeRayOrigin, rayDir, Zenith_Maths::Vector3(0, 0, 0), worldCubeSize, dist) && dist < closestDistance)
			{
				closestDistance = dist;
				closestComponent = GizmoComponent::ScaleXYZ;
			}
		}
	}
	else if (s_eMode == GizmoMode::Rotate)
	{
		// Test rotation circles
		float dist;
		if (Zenith_Maths::Intersections::RayIntersectsCircle(relativeRayOrigin, rayDir, Zenith_Maths::Vector3(1, 0, 0), worldCircleRadius, worldInteractionThreshold, dist) && dist < closestDistance)
		{
			closestDistance = dist;
			closestComponent = GizmoComponent::RotateX;
		}
		if (Zenith_Maths::Intersections::RayIntersectsCircle(relativeRayOrigin, rayDir, Zenith_Maths::Vector3(0, 1, 0), worldCircleRadius, worldInteractionThreshold, dist) && dist < closestDistance)
		{
			closestDistance = dist;
			closestComponent = GizmoComponent::RotateY;
		}
		if (Zenith_Maths::Intersections::RayIntersectsCircle(relativeRayOrigin, rayDir, Zenith_Maths::Vector3(0, 0, 1), worldCircleRadius, worldInteractionThreshold, dist) && dist < closestDistance)
		{
			closestDistance = dist;
			closestComponent = GizmoComponent::RotateZ;
		}
	}

	// Distance is already in world space, no conversion needed
	outDistance = closestDistance;
	return closestComponent;
}

// ==================== TRANSFORM MANIPULATION ====================

void Flux_Gizmos::ApplyTranslation(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir)
{
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	if (!xScene.EntityHasComponent<Zenith_TransformComponent>(s_pxTargetEntity->GetEntityID()))
		return;

	Zenith_TransformComponent& xTransform = xScene.GetComponentFromEntity<Zenith_TransformComponent>(s_pxTargetEntity->GetEntityID());

	// Get constraint axis
	Zenith_Maths::Vector3 axis(0, 0, 0);
	switch (s_eActiveComponent)
	{
		case GizmoComponent::TranslateX: axis = Zenith_Maths::Vector3(1, 0, 0); break;
		case GizmoComponent::TranslateY: axis = Zenith_Maths::Vector3(0, 1, 0); break;
		case GizmoComponent::TranslateZ: axis = Zenith_Maths::Vector3(0, 0, 1); break;
		default: return;
	}

	// CORRECT APPROACH: Track offset from initial click and maintain it
	// 
	// The user clicks at s_xInteractionStartPos on the gizmo.
	// As they drag, we want the gizmo to "follow" the mouse ray along the constraint axis.
	// 
	// The key insight: Find the closest point on the constraint axis to BOTH rays:
	// - Initial ray (at click): Find closest point -> this is our reference offset
	// - Current ray (during drag): Find closest point -> this is where we want to be
	// The difference between these is how much to move the entity.
	
	// The constraint axis always passes through s_xInitialEntityPosition (where entity started)
	// Axis line: P(t) = s_xInitialEntityPosition + t * axis
	
	// First: Find closest point on axis to the INITIAL click position
	Zenith_Maths::Vector3 offsetToClick = s_xInteractionStartPos - s_xInitialEntityPosition;
	float t_initial = glm::dot(offsetToClick, axis);  // Project click onto axis
	
	// Second: Find closest point on axis to the CURRENT mouse ray using line-line closest point
	// Ray: R(s) = rayOrigin + s * rayDir
	// Axis: A(t) = s_xInitialEntityPosition + t * axis
	// We want to find t that minimizes distance between the lines
	//
	// Standard formula: w = AxisOrigin - RayOrigin (P1 - P2)
	// t = (b*e - c*d) / (a*c - b*b)
	// where a = axis·axis, b = axis·rayDir, c = rayDir·rayDir, d = axis·w, e = rayDir·w

	Zenith_Maths::Vector3 w = s_xInitialEntityPosition - rayOrigin;  // FIXED: w = P1 - P2

	float a = 1.0f;                           // dot(axis, axis) = 1 for unit vector
	float b = glm::dot(axis, rayDir);
	float c = glm::dot(rayDir, rayDir);
	float d = glm::dot(axis, w);
	float e = glm::dot(rayDir, w);

	float denom = a * c - b * b;

	// Check if ray is parallel to axis
	if (glm::abs(denom) < 0.0001f)
	{
		return;
	}

	// Solve for t (parameter along axis for closest point to current ray)
	float t_current = (b * e - c * d) / denom;
	
	// The entity should move by the difference
	float delta_t = t_current - t_initial;
	
	// Apply the movement
	Zenith_Maths::Vector3 newPosition = s_xInitialEntityPosition + axis * delta_t;
	
	xTransform.SetPosition(newPosition);
}

void Flux_Gizmos::ApplyRotation(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir)
{
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	if (!xScene.EntityHasComponent<Zenith_TransformComponent>(s_pxTargetEntity->GetEntityID()))
		return;

	Zenith_TransformComponent& xTransform = xScene.GetComponentFromEntity<Zenith_TransformComponent>(s_pxTargetEntity->GetEntityID());

	// Get rotation axis
	Zenith_Maths::Vector3 axis(0, 0, 0);
	switch (s_eActiveComponent)
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

	Zenith_Maths::Vector3 planePoint = s_xInitialEntityPosition;
	float t = glm::dot(axis, planePoint - rayOrigin) / denom;
	if (t < 0.0f)
		return;

	Zenith_Maths::Vector3 currentPoint = rayOrigin + rayDir * t;

	// Calculate angle
	Zenith_Maths::Vector3 initialVec = glm::normalize(s_xInteractionStartPos - s_xInitialEntityPosition);
	Zenith_Maths::Vector3 currentVec = glm::normalize(currentPoint - s_xInitialEntityPosition);

	float angle = acosf(glm::clamp(glm::dot(initialVec, currentVec), -1.0f, 1.0f));
	Zenith_Maths::Vector3 cross = glm::cross(initialVec, currentVec);
	if (glm::dot(cross, axis) < 0.0f)
		angle = -angle;

	// Apply rotation
	Zenith_Maths::Quaternion deltaRotation = glm::angleAxis(angle, axis);
	Zenith_Maths::Quaternion newRotation = deltaRotation * s_xInitialEntityRotation;
	xTransform.SetRotation(newRotation);
}

void Flux_Gizmos::ApplyScale(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir)
{
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	if (!xScene.EntityHasComponent<Zenith_TransformComponent>(s_pxTargetEntity->GetEntityID()))
		return;

	Zenith_TransformComponent& xTransform = xScene.GetComponentFromEntity<Zenith_TransformComponent>(s_pxTargetEntity->GetEntityID());

	// Get constraint axis
	Zenith_Maths::Vector3 axis(0, 0, 0);
	bool bUniformScale = false;

	switch (s_eActiveComponent)
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

	// FIXED: Use same line-line closest point algorithm as translation
	// Scale is calculated based on offset along the constraint axis
	//
	// The key insight: measure how far along the axis the user has dragged
	// and convert that distance to a scale multiplier

	// For uniform scale, use the camera view direction as the constraint "axis"
	if (bUniformScale)
	{
		// Get camera position and forward direction
		Zenith_Maths::Vector3 xCameraPos = Flux_Graphics::GetCameraPosition();
		axis = glm::normalize(s_xInitialEntityPosition - xCameraPos);
	}

	// Find closest point on axis to the INITIAL click position
	Zenith_Maths::Vector3 offsetToClick = s_xInteractionStartPos - s_xInitialEntityPosition;
	float t_initial = glm::dot(offsetToClick, axis);  // Project click onto axis

	// Find closest point on axis to the CURRENT mouse ray
	// Using line-line closest point formula (same as translation)
	Zenith_Maths::Vector3 w = s_xInitialEntityPosition - rayOrigin;

	float a = 1.0f;  // dot(axis, axis) = 1 for unit vector
	float b = glm::dot(axis, rayDir);
	float c = glm::dot(rayDir, rayDir);
	float d = glm::dot(axis, w);
	float e = glm::dot(rayDir, w);

	float denom = a * c - b * b;

	// Check if ray is parallel to axis
	if (glm::abs(denom) < 0.0001f)
		return;

	// Solve for t (parameter along axis for closest point to current ray)
	float t_current = (b * e - c * d) / denom;

	// Calculate scale factor based on movement along axis
	// delta = how far we've moved along the axis
	float delta_t = t_current - t_initial;

	// Convert delta to scale factor
	// Use a scaling factor to make the manipulation feel natural
	// A movement of 1.0 unit along axis = 1.0 additional scale (2x total)
	const float scaleSpeed = 0.5f;  // Adjust for sensitivity
	float scaleFactor = 1.0f + (delta_t * scaleSpeed);

	// Clamp to prevent negative or zero scale
	scaleFactor = glm::max(scaleFactor, 0.01f);

	// Apply scale based on active component
	Zenith_Maths::Vector3 newScale = s_xInitialEntityScale;

	if (bUniformScale)
	{
		// Uniform scaling
		newScale *= scaleFactor;
	}
	else
	{
		// Per-axis scaling
		switch (s_eActiveComponent)
		{
			case GizmoComponent::ScaleX: newScale.x *= scaleFactor; break;
			case GizmoComponent::ScaleY: newScale.y *= scaleFactor; break;
			case GizmoComponent::ScaleZ: newScale.z *= scaleFactor; break;
			default: return;
		}
	}

	xTransform.SetScale(newScale);
}

#endif // ZENITH_TOOLS
