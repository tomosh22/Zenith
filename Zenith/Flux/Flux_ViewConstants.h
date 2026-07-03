#pragma once

#include "Maths/Zenith_Maths.h"

#include <cstddef>	// offsetof — spine CB layout guards below

// =====================================================================
// The spine constant-buffer payloads — GLOBAL (set 0, binding 0) and VIEW
// (set 1, binding 0) — plus the CPU-side camera-matrix source struct.
//
// Extracted from Flux_GraphicsImpl (which keeps nested aliases so existing
// `Flux_GraphicsImpl::ViewConstants` references compile unchanged) so the
// render-view registry (Flux/RenderViews/Flux_RenderViews.h) can carry a
// per-view ViewConstants payload without including the whole graphics impl.
//
// Layouts mirror the matching Slang GlobalConstantsLayout / ViewConstantsLayout
// in Shaders/Common/Bindings.slang — change them in lockstep.
// =====================================================================

struct Flux_FrameConstants
{
	// Value-initialised to identity so the FIRST frame (before BuildCameraMatrices first
	// succeeds — UploadFrameConstants skips the refresh while the camera is invalid) reads
	// a DEFINED view-proj rather than indeterminate VRAM. The scene-graph snapshot stamps
	// its camera frustum from m_xViewProjMat, so an indeterminate value would cull every
	// renderable against garbage on frame 0. In-class initializers don't change the GPU
	// constant-buffer layout (the struct stays trivially copyable for the memcpy upload).
	Zenith_Maths::Matrix4 m_xViewMat        = Zenith_Maths::Matrix4(1.0f);
	Zenith_Maths::Matrix4 m_xProjMat        = Zenith_Maths::Matrix4(1.0f);
	Zenith_Maths::Matrix4 m_xViewProjMat    = Zenith_Maths::Matrix4(1.0f);
	Zenith_Maths::Matrix4 m_xInvViewProjMat = Zenith_Maths::Matrix4(1.0f);
	Zenith_Maths::Matrix4 m_xInvViewMat     = Zenith_Maths::Matrix4(1.0f);
	Zenith_Maths::Matrix4 m_xInvProjMat     = Zenith_Maths::Matrix4(1.0f);
	Zenith_Maths::Vector4 m_xCamPos_Pad;
	Zenith_Maths::Vector4 m_xSunDir_Pad;
	Zenith_Maths::Vector4 m_xSunColour_Pad;
	Zenith_Maths::UVector2 m_xScreenDims;
	Zenith_Maths::Vector2 m_xRcpScreenDims;
#ifdef ZENITH_TOOLS
	u_int m_uQuadUtilisationAnalysis;
	u_int m_uTargetPixelsPerTri;
#else
	u_int m_uPad0;
	u_int m_uPad1;
#endif
	Zenith_Maths::Vector2 m_xCameraNearFar;
};

// The GLOBAL (view-invariant) and VIEW (per-camera) spine frequencies — what
// every shader on the ParameterBlock spine reads (GlobalConstants @ set 0 +
// ViewConstants @ set 1). Both are filled each frame from the CPU-side
// FrameConstants (UploadFrameConstants). (FrameConstants is no longer
// GPU-uploaded — it survives as the CPU camera-matrix struct + mirror source.)
// (The sun moved into Flux_ViewConstants: each render view owns its sun so a
// secondary view — the editor material preview — can light itself independently.)
struct Flux_GlobalConstants
{
	float                  m_fTimeSeconds = 0.0f;
	u_int                  m_uFrameIndex  = 0;
	Zenith_Maths::Vector2  m_xPad;
};

struct Flux_ViewConstants
{
	Zenith_Maths::Matrix4 m_xViewMat        = Zenith_Maths::Matrix4(1.0f);
	Zenith_Maths::Matrix4 m_xProjMat        = Zenith_Maths::Matrix4(1.0f);
	Zenith_Maths::Matrix4 m_xViewProjMat    = Zenith_Maths::Matrix4(1.0f);
	Zenith_Maths::Matrix4 m_xInvViewProjMat = Zenith_Maths::Matrix4(1.0f);
	Zenith_Maths::Matrix4 m_xInvViewMat     = Zenith_Maths::Matrix4(1.0f);
	Zenith_Maths::Matrix4 m_xInvProjMat     = Zenith_Maths::Matrix4(1.0f);
	Zenith_Maths::Vector4 m_xCamPos_Pad;
	Zenith_Maths::UVector2 m_xScreenDims;
	Zenith_Maths::Vector2 m_xRcpScreenDims;
#ifdef ZENITH_TOOLS
	u_int m_uQuadUtilisationAnalysis;
	u_int m_uTargetPixelsPerTri;
#else
	u_int m_uPad0;
	u_int m_uPad1;
#endif
	Zenith_Maths::Vector2 m_xCameraNearFar;
	// Per-view sun (moved from Flux_GlobalConstants): main view = scene sun;
	// preview view = its own orbit light.
	Zenith_Maths::Vector4 m_xSunDir_Pad;
	Zenith_Maths::Vector4 m_xSunColour_Pad;
	// Per-view feature flags (FLUX_VIEW_FLAG_* in Flux/RenderViews/Flux_RenderViews.h,
	// mirrored in Common/Bindings.slang) + this view's registry slot.
	u_int                 m_uViewFlags = 0u;
	u_int                 m_uViewSlot  = 0u;
	// Reciprocal OUTPUT (swapchain) dims for this view. Under temporal upscaling the main view's
	// m_xRcpScreenDims above is the RENDER resolution, but the UI/text overlay (quads/text) draws
	// onto the OUTPUT-res FinalRT — so they read THIS field (g_xRcpOutputDims) for their pixel->NDC
	// mapping. For a non-upscaled view it just equals m_xRcpScreenDims. Repurposed from the former
	// 8-byte pad @472 (was m_xViewPad) → ZERO sizeof change; the spine stays 624B.
	Zenith_Maths::Vector2 m_xRcpOutputDims;
	// --- TAA temporal foundation (Stage 2) ---------------------------------
	// The UNJITTERED current + previous view-proj (the main view's m_xViewProjMat
	// above carries the sub-pixel jitter in its GPU copy; these two never do). The
	// velocity pass reprojects with them so motion vectors are jitter-free at the
	// source, and the GPU cull reads m_xViewProjMatNoJitter (never the jittered
	// m_xViewProjMat). Non-main views (cascades/preview) stage NoJitter == their own
	// unjittered view-proj and PrevNoJitter == current (they never do velocity/jitter).
	// m_xJitterUV_PrevJitterUV: xy = this frame's jitter offset in UV space, zw =
	// previous frame's (both zero when jitter is disabled).
	Zenith_Maths::Matrix4 m_xViewProjMatNoJitter     = Zenith_Maths::Matrix4(1.0f);
	Zenith_Maths::Matrix4 m_xPrevViewProjMatNoJitter = Zenith_Maths::Matrix4(1.0f);
	Zenith_Maths::Vector4 m_xJitterUV_PrevJitterUV   = Zenith_Maths::Vector4(0.0f);
};

// Spine CB layout is hand-authored in Common/Bindings.slang (there is no single
// canonical Generated/*_CB header for the spine). These pin the C++ mirrors to the
// reflected 624B/16B std140 layout that every Generated/*.h already static_asserts
// against Slang reflection (g_xView_CB @ 624 with sun@432/448, g_uViewFlags@464,
// g_uViewSlot@468, the TAA NoJitter matrices@480/544 and jitter-UV@608; g_xGlobal_CB
// @ 16). A future member add/reorder therefore fails the build instead of silently
// corrupting the sizeof()-driven per-view CB upload (Flux_Graphics.cpp
// UploadRenderViewConstants). Offsets are asserted only on fields outside the
// ZENITH_TOOLS #ifdef (it swaps field NAMES, not sizes) so one set of asserts covers
// all four config permutations.
static_assert(sizeof(Flux_GlobalConstants) == 16, "Flux_GlobalConstants drifted from g_xGlobal_CB (16B)");
static_assert(offsetof(Flux_GlobalConstants, m_uFrameIndex) == 4, "g_uFrameIndex offset drifted");

static_assert(sizeof(Flux_ViewConstants) == 624, "Flux_ViewConstants drifted from g_xView_CB (624B)");
static_assert(offsetof(Flux_ViewConstants, m_xCameraNearFar) == 424, "nearFar offset drifted");
static_assert(offsetof(Flux_ViewConstants, m_xSunDir_Pad)    == 432, "per-view sun dir offset drifted");
static_assert(offsetof(Flux_ViewConstants, m_xSunColour_Pad) == 448, "per-view sun colour offset drifted");
static_assert(offsetof(Flux_ViewConstants, m_uViewFlags)     == 464, "g_uViewFlags offset drifted");
static_assert(offsetof(Flux_ViewConstants, m_uViewSlot)      == 468, "g_uViewSlot offset drifted");
static_assert(offsetof(Flux_ViewConstants, m_xRcpOutputDims) == 472, "TAA rcp-output-dims offset drifted (repurposed pad @472)");
static_assert(offsetof(Flux_ViewConstants, m_xViewProjMatNoJitter)     == 480, "TAA NoJitter view-proj offset drifted");
static_assert(offsetof(Flux_ViewConstants, m_xPrevViewProjMatNoJitter) == 544, "TAA prev NoJitter view-proj offset drifted");
static_assert(offsetof(Flux_ViewConstants, m_xJitterUV_PrevJitterUV)   == 608, "TAA jitter-UV offset drifted");
