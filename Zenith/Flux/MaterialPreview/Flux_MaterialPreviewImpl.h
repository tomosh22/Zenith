#pragma once

#ifdef ZENITH_TOOLS

#include "Flux/Flux.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Maths/Zenith_Maths.h"

class Flux_CommandList;
class Zenith_MaterialAsset;
class Zenith_MeshGeometryAsset;

enum MaterialPreviewMesh : u_int
{
	MATERIAL_PREVIEW_MESH_SPHERE = 0,
	MATERIAL_PREVIEW_MESH_CUBE,
	MATERIAL_PREVIEW_MESH_PLANE,
	MATERIAL_PREVIEW_MESH_CYLINDER,
	MATERIAL_PREVIEW_MESH_COUNT
};

// TOOLS-only offscreen renderer behind the Material Editor's live preview
// (UE-style): a procedural primitive carrying the edited material, lit by the
// scene's IBL (irradiance + prefiltered + BRDF LUT) and one rotatable
// directional light, with the IBL environment visible as the background.
//
// Targets are SUBSYSTEM-OWNED persistent attachments (IBL CreateRenderTargets
// pattern) — they survive render-graph rebuilds, so the LDR SRV handed to
// ImGui stays valid frame to frame. Three passes appended at the end of the
// setup walk: Background (environment cubemap through the orbit camera),
// Mesh (the shared Translucent_Forward uber-material shader against the
// preview targets), Tonemap (fixed-exposure ACES to the LDR target). All
// record callbacks early-out when the panel is closed.
//
// Parameters are plain UBO bytes — every edit shows the SAME frame, no
// shader recompile (the UE Material Instance property).
class Flux_MaterialPreviewImpl
{
public:
	static constexpr u_int uPREVIEW_SIZE = 512;

	Flux_MaterialPreviewImpl() = default;
	~Flux_MaterialPreviewImpl() = default;

	Flux_MaterialPreviewImpl(const Flux_MaterialPreviewImpl&) = delete;
	Flux_MaterialPreviewImpl& operator=(const Flux_MaterialPreviewImpl&) = delete;

	void Initialise();
	void Shutdown();
	void BuildPipelines();

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	//--------------------------------------------------------------------------
	// Panel-facing API (main thread)
	//--------------------------------------------------------------------------

	// The panel calls this every frame it is visible; rendering early-outs
	// while inactive. Auto-cleared each frame (visibility = liveness).
	void SetActive(bool bActive) { m_bActive = bActive; }
	bool IsActive() const { return m_bActive; }

	void SetMaterial(Zenith_MaterialAsset* pxMaterial);
	Zenith_MaterialAsset* GetMaterial() { return m_xMaterial.GetDirect() ? m_xMaterial.GetDirect() : m_xMaterial.Resolve(); }

	void SetPreviewMesh(MaterialPreviewMesh eMesh) { m_eMesh = eMesh; }
	MaterialPreviewMesh GetPreviewMesh() const { return m_eMesh; }

	// Orbit camera: LMB-drag rotates, wheel zooms.
	void OrbitCamera(float fDeltaYaw, float fDeltaPitch);
	void ZoomCamera(float fDelta);

	// Light direction: L+drag (UE convention).
	void OrbitLight(float fDeltaYaw, float fDeltaPitch);
	void SetLightAngles(float fYaw, float fPitch) { m_fLightYaw = fYaw; m_fLightPitch = fPitch; }
	void GetLightAngles(float& fYaw, float& fPitch) const { fYaw = m_fLightYaw; fPitch = m_fLightPitch; }

	// The ImGui-visible LDR target (stable across graph rebuilds).
	const Flux_ShaderResourceView& GetPreviewSRV() { return m_xPreviewLDR.SRV(); }

	//--------------------------------------------------------------------------
	// Internals (public for the non-capturing graph trampolines)
	//--------------------------------------------------------------------------

	void UploadPreviewFrameConstants();	// Prepare (main thread)
	Zenith_MeshGeometryAsset* GetActiveMeshGeometry();
	Zenith_Maths::Matrix4 GetActiveMeshModelMatrix() const;

	bool m_bActive = false;

	MaterialHandle m_xMaterial;
	MaterialPreviewMesh m_eMesh = MATERIAL_PREVIEW_MESH_SPHERE;

	// Orbit state (radians; distance in metres around a unit primitive).
	float m_fCameraYaw = 0.6f;
	float m_fCameraPitch = 0.35f;
	float m_fCameraDistance = 1.6f;
	float m_fLightYaw = 0.8f;
	float m_fLightPitch = 0.7f;

	// Persistent preview targets.
	Flux_RenderAttachment m_xPreviewHDR;	// RGBA16F lit scene
	Flux_RenderAttachment m_xPreviewDepth;	// D32
	Flux_RenderAttachment m_xPreviewLDR;	// RGBA8 (ImGui samples this)

	// Preview-camera FrameConstants clone (set=0 b0 for all preview passes).
	Flux_GraphicsImpl::FrameConstants m_xPreviewFrameConstants;
	Flux_DynamicConstantBuffer        m_xPreviewFrameConstantsBuffer;

	// Shaders + pipelines. The mesh pass reuses the Translucent_Forward
	// program against the preview targets; blend mode picks the pipeline.
	Flux_Shader   m_xMeshShader;
	Flux_Pipeline m_xMeshPipelineOpaque;
	Flux_Pipeline m_xMeshPipelineTranslucent;
	Flux_Pipeline m_xMeshPipelineAdditive;
	Flux_Shader   m_xBackgroundShader;
	Flux_Pipeline m_xBackgroundPipeline;
	Flux_Shader   m_xTonemapShader;
	Flux_Pipeline m_xTonemapPipeline;

	// Pinned procedural preview primitives (registry-cached).
	MeshGeometryHandle m_axMeshes[MATERIAL_PREVIEW_MESH_COUNT];
};

#endif // ZENITH_TOOLS
