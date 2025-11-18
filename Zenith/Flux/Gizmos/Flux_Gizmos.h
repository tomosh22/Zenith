#pragma once

#ifdef ZENITH_TOOLS

#include "Flux/Flux.h"
#include "Maths/Zenith_Maths.h"

class Zenith_Entity;

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

private:
	// Geometry generation
	static void GenerateTranslationGizmoGeometry();
	static void GenerateRotationGizmoGeometry();
	static void GenerateScaleGizmoGeometry();
	static void GenerateArrowGeometry(Zenith_Vector<Zenith_Maths::Vector3>& positions, Zenith_Vector<uint32_t>& indices, const Zenith_Maths::Vector3& axis, const Zenith_Maths::Vector3& color);
	static void GenerateCircleGeometry(Zenith_Vector<Zenith_Maths::Vector3>& positions, Zenith_Vector<uint32_t>& indices, const Zenith_Maths::Vector3& normal, const Zenith_Maths::Vector3& color);
	static void GenerateCubeGeometry(Zenith_Vector<Zenith_Maths::Vector3>& positions, Zenith_Vector<uint32_t>& indices, const Zenith_Maths::Vector3& offset, const Zenith_Maths::Vector3& color);

	// Ray-gizmo intersection
	static GizmoComponent RaycastGizmo(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir, float& outDistance);
	static bool RayIntersectsArrow(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir, const Zenith_Maths::Vector3& axis, float& outDistance);
	static bool RayIntersectsCircle(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir, const Zenith_Maths::Vector3& normal, float& outDistance);
	static bool RayIntersectsCube(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir, const Zenith_Maths::Vector3& center, float& outDistance);

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

	// Geometry buffers (separate for each component)
	struct GizmoGeometry
	{
		Flux_Buffer m_xVertexBuffer;
		Flux_Buffer m_xIndexBuffer;
		uint32_t m_uIndexCount;
		Zenith_Maths::Vector3 m_xColor;
		GizmoComponent m_eComponent;
	};

	static Zenith_Vector<GizmoGeometry> s_xTranslateGeometry;
	static Zenith_Vector<GizmoGeometry> s_xRotateGeometry;
	static Zenith_Vector<GizmoGeometry> s_xScaleGeometry;
};

#endif // ZENITH_TOOLS
