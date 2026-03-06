#pragma once

#ifdef ZENITH_TOOLS

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Maths/Zenith_Maths.h"

class Zenith_Entity;
class Zenith_TransformComponent;

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
	ScaleXYZ  // Uniform scale (center cube)
};

enum class GizmoMode
{
	Translate,
	Rotate,
	Scale
};

class Flux_Gizmos
{
public:
	static void Initialise();
	static void Shutdown();
	static void Reset();  // Clear state when scene resets (e.g., Play/Stop transitions)

	// Rendering
	static void Render(void*);
	static void SubmitRenderTask();
	static void WaitForRenderTask();

	// Interaction
	static void SetTargetEntity(Zenith_Entity* pxEntity);
	static void SetGizmoMode(GizmoMode eMode);
	static GizmoMode GetGizmoMode() { return s_eMode; }

	// Mouse interaction
	static void BeginInteraction(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir);
	static void UpdateInteraction(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir);
	static void EndInteraction();
	static bool IsInteracting() { return s_bIsInteracting; }

	// Gizmo state
	static GizmoComponent GetHoveredComponent() { return s_eHoveredComponent; }
	static GizmoComponent GetActiveComponent() { return s_eActiveComponent; }

	friend class Zenith_UnitTests;

private:
	// Geometry buffers (separate for each component)
	struct GizmoGeometry
	{
		Flux_VertexBuffer m_xVertexBuffer;
		Flux_IndexBuffer m_xIndexBuffer;
		uint32_t m_uIndexCount;
		Zenith_Maths::Vector3 m_xColor;
		GizmoComponent m_eComponent;
	};

	// Scene/transform validity check - returns pointer to TransformComponent if target entity is valid, nullptr otherwise
	static Zenith_TransformComponent* GetEditableTransform();

	// Interleave position and color data into a flat float array (6 floats per vertex: xyz + rgb)
	static void InterleaveVertexData(Zenith_Vector<float>& xOut, const Zenith_Vector<Zenith_Maths::Vector3>& xPositions, const Zenith_Vector<Zenith_Maths::Vector3>& xColors);

	// Create GPU vertex/index buffers from geometry data and push a GizmoGeometry entry into the list
	static void UploadGizmoGeometry(Zenith_Vector<GizmoGeometry>& xGeometryList, const Zenith_Vector<float>& xVertexData, const Zenith_Vector<uint32_t>& xIndices, const Zenith_Maths::Vector3& xColor, GizmoComponent eComponent);

	// Find the parameter t along the axis line closest to a ray (line-line closest point).
	// Returns false if the lines are parallel (within epsilon).
	static bool GetLineLineClosestPointParameter(const Zenith_Maths::Vector3& xAxisOrigin, const Zenith_Maths::Vector3& xAxis, const Zenith_Maths::Vector3& xRayOrigin, const Zenith_Maths::Vector3& xRayDir, float& fOutT);

	// Compute a perpendicular tangent frame (tangent + bitangent) from an axis direction
	static void ComputeTangentFrame(const Zenith_Maths::Vector3& xAxis, Zenith_Maths::Vector3& xOutTangent, Zenith_Maths::Vector3& xOutBitangent);

	// Geometry generation
	static void GenerateTranslationGizmoGeometry();
	static void GenerateRotationGizmoGeometry();
	static void GenerateScaleGizmoGeometry();
	static void GenerateArrowGeometry(Zenith_Vector<GizmoGeometry>& geometryList, const Zenith_Maths::Vector3& axis, const Zenith_Maths::Vector3& color, GizmoComponent component);
	static void GenerateCircleGeometry(Zenith_Vector<GizmoGeometry>& geometryList, const Zenith_Maths::Vector3& normal, const Zenith_Maths::Vector3& color, GizmoComponent component);
	static void GenerateCubeGeometry(Zenith_Vector<GizmoGeometry>& geometryList, const Zenith_Maths::Vector3& offset, const Zenith_Maths::Vector3& color, GizmoComponent component);

	// Ray-gizmo intersection (world-space versions - preferred)
	static GizmoComponent RaycastGizmo(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir, float& outDistance);

	// Transform manipulation
	static void ApplyTranslation(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir);
	static void ApplyRotation(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir);
	static void ApplyScale(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir);

	// State
	static Zenith_Entity* s_pxTargetEntity;
	static GizmoMode s_eMode;
	static GizmoComponent s_eHoveredComponent;
	static GizmoComponent s_eActiveComponent;
	static bool s_bIsInteracting;

	// Interaction state
	static Zenith_Maths::Vector3 s_xInteractionStartPos;
	static Zenith_Maths::Vector3 s_xInitialEntityPosition;
	static Zenith_Maths::Quaternion s_xInitialEntityRotation;
	static Zenith_Maths::Vector3 s_xInitialEntityScale;
	static float s_fGizmoScale;  // Scale gizmo based on camera distance

	// Rendering resources
	static Flux_Pipeline s_xPipeline;
	static Flux_Shader s_xShader;
	static Flux_CommandList s_xCommandList;

	static Zenith_Vector<GizmoGeometry> s_xTranslateGeometry;
	static Zenith_Vector<GizmoGeometry> s_xRotateGeometry;
	static Zenith_Vector<GizmoGeometry> s_xScaleGeometry;
};

#endif // ZENITH_TOOLS
