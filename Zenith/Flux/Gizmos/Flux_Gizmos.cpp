#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Flux/Gizmos/Flux_Gizmos.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "DebugVariables/Zenith_DebugVariables.h"

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

	Zenith_Log("Flux_Gizmos initialised");
}

void Flux_Gizmos::Shutdown()
{
	// Clear geometry buffers (Flux_VertexBuffer and Flux_IndexBuffer handle their own cleanup)
	s_xTranslateGeometry.Clear();
	s_xRotateGeometry.Clear();
	s_xScaleGeometry.Clear();
}

void Flux_Gizmos::Render(void*)
{
	if (!dbg_bRenderGizmos || !s_pxTargetEntity)
		return;

	// Get entity transform
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	if (!xScene.EntityHasComponent<Zenith_TransformComponent>(s_pxTargetEntity->GetEntityID()))
		return;

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
		return;

	// Record rendering commands
	s_xCommandList.Reset(false);
	s_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPipeline);

	// Bind frame constants (set 0, binding 0)
	s_xCommandList.AddCommand<Flux_CommandBeginBind>(0);
	s_xCommandList.AddCommand<Flux_CommandBindCBV>(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer().m_xCBV, 0);

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
		return;

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

	// Generate circle vertices
	for (uint32_t i = 0; i < GIZMO_CIRCLE_SEGMENTS; ++i)
	{
		float angle = (float)i / GIZMO_CIRCLE_SEGMENTS * 2.0f * 3.14159f;
		Zenith_Maths::Vector3 pos = tangent * cosf(angle) * GIZMO_CIRCLE_RADIUS + bitangent * sinf(angle) * GIZMO_CIRCLE_RADIUS;

		positions.PushBack(pos);
		colors.PushBack(color);
	}

	// Generate line indices
	for (uint32_t i = 0; i < GIZMO_CIRCLE_SEGMENTS; ++i)
	{
		indices.PushBack(i);
		indices.PushBack((i + 1) % GIZMO_CIRCLE_SEGMENTS);
		indices.PushBack(i);  // Degenerate triangle to create lines (will need line topology)
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

	// Get entity position
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	if (!xScene.EntityHasComponent<Zenith_TransformComponent>(s_pxTargetEntity->GetEntityID()))
		return GizmoComponent::None;

	Zenith_TransformComponent& xTransform = xScene.GetComponentFromEntity<Zenith_TransformComponent>(s_pxTargetEntity->GetEntityID());
	Zenith_Maths::Vector3 xGizmoPos;
	xTransform.GetPosition(xGizmoPos);

	// Transform ray to gizmo local space
	Zenith_Maths::Vector3 localRayOrigin = (rayOrigin - xGizmoPos) / s_fGizmoScale;
	Zenith_Maths::Vector3 localRayDir = rayDir;

	float closestDistance = FLT_MAX;
	GizmoComponent closestComponent = GizmoComponent::None;

	// Test against axes based on mode
	if (s_eMode == GizmoMode::Translate || s_eMode == GizmoMode::Scale)
	{
		// Test X axis
		float dist;
		if (RayIntersectsArrow(localRayOrigin, localRayDir, Zenith_Maths::Vector3(1, 0, 0), dist) && dist < closestDistance)
		{
			closestDistance = dist;
			closestComponent = (s_eMode == GizmoMode::Translate) ? GizmoComponent::TranslateX : GizmoComponent::ScaleX;
		}
		// Test Y axis
		if (RayIntersectsArrow(localRayOrigin, localRayDir, Zenith_Maths::Vector3(0, 1, 0), dist) && dist < closestDistance)
		{
			closestDistance = dist;
			closestComponent = (s_eMode == GizmoMode::Translate) ? GizmoComponent::TranslateY : GizmoComponent::ScaleY;
		}
		// Test Z axis
		if (RayIntersectsArrow(localRayOrigin, localRayDir, Zenith_Maths::Vector3(0, 0, 1), dist) && dist < closestDistance)
		{
			closestDistance = dist;
			closestComponent = (s_eMode == GizmoMode::Translate) ? GizmoComponent::TranslateZ : GizmoComponent::ScaleZ;
		}

		// Test center cube for scale mode
		if (s_eMode == GizmoMode::Scale)
		{
			if (RayIntersectsCube(localRayOrigin, localRayDir, Zenith_Maths::Vector3(0, 0, 0), dist) && dist < closestDistance)
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
		if (RayIntersectsCircle(localRayOrigin, localRayDir, Zenith_Maths::Vector3(1, 0, 0), dist) && dist < closestDistance)
		{
			closestDistance = dist;
			closestComponent = GizmoComponent::RotateX;
		}
		if (RayIntersectsCircle(localRayOrigin, localRayDir, Zenith_Maths::Vector3(0, 1, 0), dist) && dist < closestDistance)
		{
			closestDistance = dist;
			closestComponent = GizmoComponent::RotateY;
		}
		if (RayIntersectsCircle(localRayOrigin, localRayDir, Zenith_Maths::Vector3(0, 0, 1), dist) && dist < closestDistance)
		{
			closestDistance = dist;
			closestComponent = GizmoComponent::RotateZ;
		}
	}

	outDistance = closestDistance * s_fGizmoScale;
	return closestComponent;
}

bool Flux_Gizmos::RayIntersectsArrow(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir, const Zenith_Maths::Vector3& axis, float& outDistance)
{
	// Simplified: Treat arrow as cylinder for intersection
	// More accurate would test cone and cylinder separately

	// Ray-cylinder intersection (infinite cylinder along axis)
	Zenith_Maths::Vector3 ao = rayOrigin;
	float dotAxisDir = glm::dot(axis, rayDir);
	float dotAxisOrigin = glm::dot(axis, ao);

	// Quadratic coefficients
	float a = glm::dot(rayDir, rayDir) - dotAxisDir * dotAxisDir;
	float b = 2.0f * (glm::dot(rayDir, ao) - dotAxisDir * dotAxisOrigin);
	float c = glm::dot(ao, ao) - dotAxisOrigin * dotAxisOrigin - GIZMO_INTERACTION_THRESHOLD * GIZMO_INTERACTION_THRESHOLD;

	float discriminant = b * b - 4.0f * a * c;
	if (discriminant < 0.0f)
		return false;

	float t = (-b - sqrtf(discriminant)) / (2.0f * a);
	if (t < 0.0f)
		t = (-b + sqrtf(discriminant)) / (2.0f * a);

	if (t < 0.0f)
		return false;

	// Check if hit point is within arrow length
	Zenith_Maths::Vector3 hitPoint = rayOrigin + rayDir * t;
	float alongAxis = glm::dot(hitPoint, axis);

	if (alongAxis >= 0.0f && alongAxis <= GIZMO_ARROW_LENGTH)
	{
		outDistance = t;
		return true;
	}

	return false;
}

bool Flux_Gizmos::RayIntersectsCircle(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir, const Zenith_Maths::Vector3& normal, float& outDistance)
{
	// Ray-plane intersection
	float denom = glm::dot(normal, rayDir);
	if (fabsf(denom) < 0.0001f)
		return false;  // Parallel

	float t = -glm::dot(normal, rayOrigin) / denom;
	if (t < 0.0f)
		return false;

	// Check if hit point is near the circle
	Zenith_Maths::Vector3 hitPoint = rayOrigin + rayDir * t;
	float distFromCenter = glm::length(hitPoint);

	if (fabsf(distFromCenter - GIZMO_CIRCLE_RADIUS) < GIZMO_INTERACTION_THRESHOLD)
	{
		outDistance = t;
		return true;
	}

	return false;
}

bool Flux_Gizmos::RayIntersectsCube(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir, const Zenith_Maths::Vector3& center, float& outDistance)
{
	// Simple AABB intersection
	float half = GIZMO_CUBE_SIZE * 0.5f;
	Zenith_Maths::Vector3 boxMin = center - Zenith_Maths::Vector3(half);
	Zenith_Maths::Vector3 boxMax = center + Zenith_Maths::Vector3(half);

	Zenith_Maths::Vector3 invDir = 1.0f / rayDir;
	Zenith_Maths::Vector3 t0 = (boxMin - rayOrigin) * invDir;
	Zenith_Maths::Vector3 t1 = (boxMax - rayOrigin) * invDir;

	Zenith_Maths::Vector3 tmin = glm::min(t0, t1);
	Zenith_Maths::Vector3 tmax = glm::max(t0, t1);

	float tNear = glm::max(glm::max(tmin.x, tmin.y), tmin.z);
	float tFar = glm::min(glm::min(tmax.x, tmax.y), tmax.z);

	if (tNear > tFar || tFar < 0.0f)
		return false;

	outDistance = tNear > 0.0f ? tNear : tFar;
	return true;
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

	// Project ray onto axis to find closest point on axis to ray
	Zenith_Maths::Vector3 toRayOrigin = rayOrigin - s_xInitialEntityPosition;
	float rayDotAxis = glm::dot(rayDir, axis);
	float originDotAxis = glm::dot(toRayOrigin, axis);

	// Find parameter t along axis where it's closest to the ray
	// This is a line-line closest point problem
	float t = originDotAxis - glm::dot(rayDir - axis * rayDotAxis, toRayOrigin) / (1.0f - rayDotAxis * rayDotAxis + 0.0001f);

	Zenith_Maths::Vector3 newPosition = s_xInitialEntityPosition + axis * t;
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

	// Calculate scale factor based on ray movement
	Zenith_Maths::Vector3 toRayOrigin = rayOrigin - s_xInitialEntityPosition;
	float initialDist = glm::length(s_xInteractionStartPos - s_xInitialEntityPosition);
	float currentDist = glm::length(toRayOrigin + rayDir * glm::dot(rayDir, toRayOrigin));

	float scaleFactor = currentDist / (initialDist + 0.0001f);

	// Apply scale based on active component
	Zenith_Maths::Vector3 newScale = s_xInitialEntityScale;

	switch (s_eActiveComponent)
	{
		case GizmoComponent::ScaleX: newScale.x *= scaleFactor; break;
		case GizmoComponent::ScaleY: newScale.y *= scaleFactor; break;
		case GizmoComponent::ScaleZ: newScale.z *= scaleFactor; break;
		case GizmoComponent::ScaleXYZ: newScale *= scaleFactor; break;  // Uniform scale
		default: return;
	}

	xTransform.SetScale(newScale);
}

#endif // ZENITH_TOOLS
