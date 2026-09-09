#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/RenderViews/Flux_RenderViews.h"	// FLUX_MAX_RENDER_VIEWS (per-view normals clones)
#include "Maths/Zenith_Maths.h"

class Zenith_TextureAsset;
class Flux_RenderGraph;
class Flux_GraphicsImpl;

// ===== INSTANCE STRUCT (mirrors Flux_Decals_Apply.slang) =====
//
// 160 bytes. xyz/w packing keeps std430 alignment clean. Reserved slots
// are intentional — adding fields without breaking the size assert is a
// readability win when iterating.
//
// Lives here (not in the .cpp) so the dense GPU staging array can be a
// Flux_DecalsImpl member; the layout is unchanged from when it was a
// .cpp-local struct.
struct DecalInstance
{
	Zenith_Maths::Matrix4 m_xWorld;          // 64 — unit-cube-local -> world (T * R * S)
	Zenith_Maths::Matrix4 m_xWorldInverse;   // 64 — world -> unit-cube-local
	// xyz = unit-length projection axis (surface normal at hit); w = fade opacity.
	// Packed into one float4 for std430 alignment, not because the two are one concept.
	Zenith_Maths::Vector4 m_xAxisOpacity;    // 16
	Zenith_Maths::Vector4 m_xParams;         // 16 — x = normal-alignment threshold; y = mode (0 = procedural bullet hole, 1 = textured brush indicator); zw reserved
	Zenith_Maths::Vector4 m_xColour;         // 16 — brush-indicator tint (rgb) + master alpha (w); unused by mode 0
};
static_assert(sizeof(DecalInstance) == 176, "DecalInstance must match Flux_Decals_Apply.slang");

// CPU-side pool slot. Ring buffer for slot recycling — m_uNextSlot tracks the
// next slot to overwrite when SpawnDecal is called. Lives here (not in the
// .cpp) so the CPU ring pool can be a Flux_DecalsImpl member.
struct CpuDecalSlot
{
	bool                  m_bActive            = false;
	Zenith_Maths::Vector3 m_xPosition;
	Zenith_Maths::Vector3 m_xNormal;           // unit-length projection axis
	float                 m_fSize              = 0.0f;
	float                 m_fInitialLifetime   = 0.0f;
	float                 m_fRemainingLifetime = 0.0f;
	DecalInstance         m_xInstance{};
};

// Phase 9: state + behaviour for Decals subsystem.
//
// Cross-subsystem dependencies (FluxGraphics / Swapchain / VulkanMemory / Frame)
// are reached via g_xEngine at point of use. The non-capturing fn-pointer
// trampolines (the Execute* graph callbacks and the Prepare callback) cannot
// capture state, so they re-enter via g_xEngine.Decals() to reach this
// singleton instance.
class Flux_DecalsImpl
{
public:
	Flux_DecalsImpl() = default;
	~Flux_DecalsImpl() = default;

	Flux_DecalsImpl(const Flux_DecalsImpl&) = delete;
	Flux_DecalsImpl& operator=(const Flux_DecalsImpl&) = delete;

	void Initialise();
	void Shutdown();
	void BuildPipelines();
	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// ONE view's entire "Decal Normals Copy" + "Decal Apply" declaration, clone
	// transient included. Driven once per ACTIVE FULL-PIPELINE view by
	// SetupRenderGraph's ForEachActiveFullPipelineView walk, in ascending slot
	// order. The graphics reference is passed in rather than re-reached through
	// g_xEngine: SetupRenderGraph already holds it hoisted, and this subsystem was
	// sitting on its singleton-allowlist ceiling when the walk was written.
	void SetupViewPasses(Flux_RenderGraph& xGraph, u_int uSlot, Flux_GraphicsImpl& xGraphics);

	// Promoted from a file-static helper; VulkanMemory is reached via g_xEngine
	// at point of use. Public because the (former free-function) call site sits
	// in Initialise; kept callable for symmetry with the rest of the init path.
	void InitialiseDecalIndexBuffer();

	void SpawnDecal(const Zenith_Maths::Vector3& xPosition,
	                const Zenith_Maths::Vector3& xNormal,
	                Zenith_TextureAsset*         pxTexture,
	                float                        fSize,
	                float                        fLifetime);

	// Editor brush indicator: ONE persistent slot outside the gameplay ring,
	// projected straight down with a textured-brush shader mode (per-call
	// tint, diffuse-only G-buffer write). One-frame lifetime: arming lasts
	// for exactly the next Prepare/pack — the editor re-arms every frame
	// while its cursor is valid, so a missed frame (mode change, session
	// close, crash) makes the indicator vanish instead of going stale.
	// fDiameter is the visible end-to-end size (matches SpawnDecal's fSize
	// convention); fVerticalExtent is the projection-axis box depth and must
	// cover the terrain relief under the brush.
	void SetEditorDecal(const Zenith_Maths::Vector3& xCentre,
	                    float                        fDiameter,
	                    float                        fVerticalExtent,
	                    const Zenith_Maths::Vector4& xColour,
	                    Zenith_TextureAsset*         pxTexture);

	// Build the dense GPU staging array from active CPU slots, ticking
	// per-slot lifetimes by fDt and deactivating expired slots. Returns the
	// number of active decals after the tick. Promoted from a file-static
	// helper so it can read/write the now-member ring pool + staging array
	// directly; the only g_xEngine reach left inside is Primitives() for the
	// debug spheres. Called only from the PrepareDecals trampoline, which
	// already holds a cached Decals() reference.
	u_int TickAndPackDense(float fDt);

	// Drop every decal. Called by the SCENE_LOAD_SINGLE render-systems reset
	// alongside Terrain/Text/Particles/Skybox/Fog/Grass: the ring holds
	// gameplay-spawned WORLD marks (bullet holes stamped at world positions), so
	// leaving them across a scene load leaves the previous world's scorch marks
	// floating in the new one. This was previously a ZENITH_TESTING-only
	// ResetForTest with no production caller -- the same omission Grass had.
	void Reset();

	bool IsInitialised() const { return m_bInitialised; }

	static constexpr u_int uMAX_DECALS = 64;

	// Ring slots + the editor brush-indicator slot.
	static constexpr u_int uMAX_DECAL_INSTANCES = uMAX_DECALS + 1;

#ifdef ZENITH_TESTING
	struct TestSlotView
	{
		Zenith_Maths::Vector3 m_xPosition;
		Zenith_Maths::Vector3 m_xNormal;
		float                 m_fRemainingLifetime;
		bool                  m_bActive;
	};
	u_int        GetActiveCountForTest();
	TestSlotView GetSlotForTest(u_int uSlotIndex);
#endif

	bool                        m_bInitialised      = false;
	u_int                       m_uNextSlot         = 0;
	u_int                       m_uActiveDecalCount = 0;

	Flux_RenderGraph*           m_pxGraph = nullptr;
	// The normals clone, ONE PER VIEW SLOT. A slot's entry is created (and thus
	// valid) only for the views SetupRenderGraph declared a pass pair for that
	// build — slot 0 always, every other full-pipeline slot while its owner has
	// the view up. ExecuteApply indexes it by the RECORDING pass's view slot, so
	// the record side can never pair one view's cube draw with another view's
	// clone. (There are deliberately no cached PASS handles beside it: the pair
	// this feature declares is always-enabled and nothing ever re-read the two
	// members that used to sit here.)
	Flux_TransientHandle        m_axNormalsCopyHandles[FLUX_MAX_RENDER_VIEWS];

	Flux_Shader                 m_xNormalsCopyShader;
	Flux_Shader                 m_xApplyShader;
	Flux_Pipeline               m_xNormalsCopyPipeline;
	Flux_Pipeline               m_xApplyPipeline;

	Flux_DynamicReadWriteBuffer m_xDecalBuffer;
	Flux_IndexBuffer            m_xDecalIndexBuffer;

	// CPU-side pool. Ring buffer for slot recycling — m_uNextSlot tracks the
	// next slot to overwrite when SpawnDecal is called. (Relocated from a
	// module-scope static; same type, same default-empty state.)
	CpuDecalSlot                m_axDecalSlots[uMAX_DECALS];

	// Dense GPU staging — packed at upload time so SV_InstanceID indexes
	// 0..uActiveDecalCount-1 contiguously regardless of CPU ring layout.
	// (Relocated from a module-scope static.) Sized +1 for the editor slot.
	DecalInstance               m_axDecalStaging[uMAX_DECAL_INSTANCES];

	// Editor brush-indicator slot (see SetEditorDecal). The armed flag is
	// consumed by the next TickAndPackDense; the texture pointer survives the
	// disarm because the record callback binds it later the same frame.
	DecalInstance               m_xEditorDecalInstance{};
	Zenith_TextureAsset*        m_pxEditorDecalTexture = nullptr;
	bool                        m_bEditorDecalArmed = false;
};
