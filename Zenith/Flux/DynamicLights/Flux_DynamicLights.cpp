#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Flux/DynamicLights/Flux_DynamicLightsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_BackendTypes.h"
// Wave 3: lights are gathered EC-side into renderer-neutral Zenith_LightRenderData
// (no Zenith_LightComponent.h / Zenith_TransformComponent.h here). The renderer keeps
// the frustum cull, intensity threshold, direction validation and GPU staging.
#include "Core/Zenith_RenderGather.h"
#include "Maths/Zenith_FrustumCulling.h"
#include "Core/Zenith_GraphicsOptions.h"

#include <cmath>
#include <algorithm>
#include <optional>

// =====================================================================
// Flux_DynamicLights — gather + upload front-end for the clustered
// deferred lighting pipeline.
//
// This subsystem used to render light volumes (spheres/cones/quads)
// directly into the HDR scene target. That technique disabled depth
// testing (because depth was bound as an SRV for world-pos reconstruct)
// so every fragment behind a back-face volume ran the full Cook-Torrance
// BRDF + 5 G-buffer samples — overlapping volumes multiplied the cost
// per pixel. After the clustered-deferred rewrite, this file's job
// shrinks to: walk the ECS, frustum-cull, priority-sort, pack into a
// single GPU buffer. The actual lighting evaluation moved into
// Flux_DeferredShading (per-pixel cluster loop) with cluster lists
// built by Flux_LightClustering.
// =====================================================================

// ========== INSTANCE STRUCT (mirrors Common.Lighting.slang) ==========
//
// LightInstance now lives on Flux_DynamicLightsImpl (in the header) so the
// CPU staging array (m_axLightStaging) can be an Impl member.

// Light type tags must match Common.Lighting.slang LIGHT_TYPE_*.
static constexpr float fLIGHT_TYPE_TAG_POINT       = 0.0f;
static constexpr float fLIGHT_TYPE_TAG_SPOT        = 1.0f;
static constexpr float fLIGHT_TYPE_TAG_DIRECTIONAL = 2.0f;

// ========== STATIC STATE ==========


// Cached frustum for culling (updated each frame).

// CPU staging now lives on Flux_DynamicLightsImpl::m_axLightStaging — a flat
// array of all light types, packed by GatherLightsFromScene.

// GPU-side: host-visible, frame-indexed. Host-visible is correct here
// because this buffer is CPU-uploaded each frame (unlike the cluster
// outputs which are GPU-written and use Flux_ReadWriteBuffer).

// Direction vector normalization epsilon (prevents NaN from zero-length vectors).
static constexpr float fDIRECTION_EPSILON = 0.0001f;

// Minimum intensity threshold - lights below this are skipped.
static constexpr float fMIN_LIGHT_INTENSITY = 0.001f;

// ========== FRUSTUM CULLING HELPERS ==========

// Test if a sphere (point light bounding volume) intersects the camera frustum.
static bool IsSphereFrustumVisible(const Zenith_Frustum& xFrustum, const Zenith_Maths::Vector3& xCenter, float fRadius)
{
	for (int i = 0; i < 6; ++i)
	{
		const Zenith_Plane& xPlane = xFrustum.m_axPlanes[i];
		float fDistance = xPlane.GetSignedDistance(xCenter);
		if (fDistance < -fRadius)
		{
			return false;
		}
	}
	return true;
}

// Build an orthonormal basis from a direction vector.
static void BuildOrthonormalBasis(const Zenith_Maths::Vector3& xDirection, Zenith_Maths::Vector3& xRight, Zenith_Maths::Vector3& xUp)
{
	Zenith_Maths::Vector3 xRef = (fabsf(xDirection.y) < 0.9f)
		? Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)
		: Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);

	xRight = Zenith_Maths::Normalize(Zenith_Maths::Cross(xDirection, xRef));
	xUp = Zenith_Maths::Cross(xRight, xDirection);
}

// Test if a cone (spot light volume) intersects the camera frustum.
static bool IsConeFrustumVisible(
	const Zenith_Frustum& xFrustum,
	const Zenith_Maths::Vector3& xApex,
	const Zenith_Maths::Vector3& xDirection,
	float fRange,
	float fOuterAngle)
{
	// First: quick bounding sphere test (conservative).
	float fSinOuter = sinf(fOuterAngle);
	float fHalfRange = fRange * 0.5f;
	Zenith_Maths::Vector3 xBoundCenter = xApex + xDirection * fHalfRange;
	float fBoundRadius = fRange * fSinOuter + fHalfRange;

	if (!IsSphereFrustumVisible(xFrustum, xBoundCenter, fBoundRadius))
	{
		return false;
	}

	// Second: test key cone points against each frustum plane.
	Zenith_Maths::Vector3 xBaseCenter = xApex + xDirection * fRange;
	float fBaseRadius = fRange * tanf(fOuterAngle);

	Zenith_Maths::Vector3 xRight, xUp;
	BuildOrthonormalBasis(xDirection, xRight, xUp);

	for (int i = 0; i < 6; ++i)
	{
		const Zenith_Plane& xPlane = xFrustum.m_axPlanes[i];

		float fApexDist = xPlane.GetSignedDistance(xApex);
		float fBaseCenterDist = xPlane.GetSignedDistance(xBaseCenter);
		float fBaseRight = xPlane.GetSignedDistance(xBaseCenter + xRight * fBaseRadius);
		float fBaseLeft = xPlane.GetSignedDistance(xBaseCenter - xRight * fBaseRadius);
		float fBaseUp = xPlane.GetSignedDistance(xBaseCenter + xUp * fBaseRadius);
		float fBaseDown = xPlane.GetSignedDistance(xBaseCenter - xUp * fBaseRadius);

		float fMaxDist = fApexDist;
		fMaxDist = std::max(fMaxDist, fBaseCenterDist);
		fMaxDist = std::max(fMaxDist, fBaseRight);
		fMaxDist = std::max(fMaxDist, fBaseLeft);
		fMaxDist = std::max(fMaxDist, fBaseUp);
		fMaxDist = std::max(fMaxDist, fBaseDown);

		if (fMaxDist < 0.0f)
		{
			return false;
		}
	}

	return true;
}

// ========== PRIORITY SORT ==========
//
// LightSortKey now lives on Flux_DynamicLightsImpl (in the header) so the
// priority sort scratch buffer (m_xSortBuffer) can be an Impl member.

static float CalculateLightPriority(const Flux_GraphicsImpl& xFluxGraphics,
	const Zenith_Maths::Vector3& xLightPos, float fIntensity, float fRange)
{
	Zenith_Maths::Vector3 xCamPos = xFluxGraphics.m_xFrameConstants.m_xCamPos_Pad;
	float fDistance = Zenith_Maths::Length(xLightPos - xCamPos);
	return (fIntensity * fRange) / (fDistance + 1.0f);
}

// ========== PUBLIC API ==========

void Flux_DynamicLightsImpl::Initialise()
{
	// One flat GPU buffer for all lights (point + spot + directional).
	const u_int64 ulLightBufferSize = uMAX_LIGHTS * sizeof(LightInstance);

	// Zero-initialize so frame-0 reads don't see garbage.
	Zenith_Vector<LightInstance> xZeroed(uMAX_LIGHTS);
	for (u_int u = 0; u < uMAX_LIGHTS; ++u) xZeroed.EmplaceBack();
	g_xEngine.FluxMemory().InitialiseDynamicReadWriteBuffer(xZeroed.GetDataPointer(), ulLightBufferSize, m_xLightBuffer);

	// Pre-allocate priority sort buffer to avoid per-frame allocs.
	m_xSortBuffer.Reserve(uMAX_LIGHTS * 2);

	m_uLightCount = 0;

	m_bInitialised = true;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_DynamicLights initialised (clustered-deferred gather front-end, max %u lights)", uMAX_LIGHTS);
}

void Flux_DynamicLightsImpl::Shutdown()
{
	if (!m_bInitialised)
	{
		return;
	}

	g_xEngine.FluxMemory().DestroyDynamicReadWriteBuffer(m_xLightBuffer);
	m_uLightCount = 0;
	m_bInitialised = false;

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_DynamicLights shut down");
}

void Flux_DynamicLightsImpl::Reset()
{
	// Light count is reset in GatherLightsFromScene().
}

// ========== GATHER ==========

// Stage helpers — pack each accepted light into the unified buffer.

static void StagePointLight(Flux_DynamicLightsImpl& xImpl, u_int& uLightCount, const Zenith_Maths::Vector3& xPosition, float fRange,
	const Zenith_Maths::Vector3& xColor, float fIntensity)
{
	if (uLightCount >= Flux_DynamicLightsImpl::uMAX_LIGHTS) return;
	LightInstance& xOut = xImpl.m_axLightStaging[uLightCount++];
	xOut.m_xPositionRange  = { xPosition.x, xPosition.y, xPosition.z, fRange };
	xOut.m_xColorIntensity = { xColor.x, xColor.y, xColor.z, fIntensity };
	xOut.m_xDirectionInner = { 0.0f, 0.0f, 0.0f, 0.0f };
	xOut.m_xTypeOuter      = { 0.0f, fLIGHT_TYPE_TAG_POINT, 0.0f, 0.0f };
}

static void StageSpotLight(Flux_DynamicLightsImpl& xImpl, u_int& uLightCount, const Zenith_Maths::Vector3& xPosition, float fRange,
	const Zenith_Maths::Vector3& xColor, float fIntensity,
	const Zenith_Maths::Vector3& xDirection, float fCosInner, float fCosOuter)
{
	if (uLightCount >= Flux_DynamicLightsImpl::uMAX_LIGHTS) return;
	LightInstance& xOut = xImpl.m_axLightStaging[uLightCount++];
	xOut.m_xPositionRange  = { xPosition.x, xPosition.y, xPosition.z, fRange };
	xOut.m_xColorIntensity = { xColor.x, xColor.y, xColor.z, fIntensity };
	xOut.m_xDirectionInner = { xDirection.x, xDirection.y, xDirection.z, fCosInner };
	xOut.m_xTypeOuter      = { fCosOuter, fLIGHT_TYPE_TAG_SPOT, 0.0f, 0.0f };
}

static void StageDirectionalLight(Flux_DynamicLightsImpl& xImpl, u_int& uLightCount, const Zenith_Maths::Vector3& xDirection,
	const Zenith_Maths::Vector3& xColor, float fIntensity)
{
	if (uLightCount >= Flux_DynamicLightsImpl::uMAX_LIGHTS) return;
	LightInstance& xOut = xImpl.m_axLightStaging[uLightCount++];
	xOut.m_xPositionRange  = { 0.0f, 0.0f, 0.0f, 0.0f };
	xOut.m_xColorIntensity = { xColor.x, xColor.y, xColor.z, fIntensity };
	xOut.m_xDirectionInner = { xDirection.x, xDirection.y, xDirection.z, 0.0f };
	xOut.m_xTypeOuter      = { 0.0f, fLIGHT_TYPE_TAG_DIRECTIONAL, 0.0f, 0.0f };
}

// Per-type try/gather helpers — each one checks the light is renderable,
// frustum-culls if applicable, and stages into the unified buffer.
//
// Returns true if the light was kept (passed culling), false if rejected.

struct PendingLight
{
	enum Type { POINT, SPOT, DIRECTIONAL };
	Type m_eType;

	// Common fields.
	Zenith_Maths::Vector3 m_xColor;
	float m_fIntensity;

	// Point/spot.
	Zenith_Maths::Vector3 m_xPosition;
	float m_fRange;

	// Spot/directional.
	Zenith_Maths::Vector3 m_xDirection;

	// Spot only.
	float m_fCosInner;
	float m_fCosOuter;

	float GetSortPriority(const Flux_GraphicsImpl& xFluxGraphics) const
	{
		if (m_eType == DIRECTIONAL) return 1e30f; // never drop directionals on sort
		return CalculateLightPriority(xFluxGraphics, m_xPosition, m_fIntensity, m_fRange);
	}
};

static void StagePending(Flux_DynamicLightsImpl& xImpl, u_int& uLightCount, const PendingLight& xL)
{
	switch (xL.m_eType)
	{
	case PendingLight::POINT:
		StagePointLight(xImpl, uLightCount, xL.m_xPosition, xL.m_fRange, xL.m_xColor, xL.m_fIntensity);
		break;
	case PendingLight::SPOT:
		StageSpotLight(xImpl, uLightCount, xL.m_xPosition, xL.m_fRange, xL.m_xColor, xL.m_fIntensity,
			xL.m_xDirection, xL.m_fCosInner, xL.m_fCosOuter);
		break;
	case PendingLight::DIRECTIONAL:
		StageDirectionalLight(xImpl, uLightCount, xL.m_xDirection, xL.m_xColor, xL.m_fIntensity);
		break;
	}
}

// Build a candidate point-light from gathered Zenith_LightRenderData. Returns nullopt
// if the light is frustum-culled.
static std::optional<PendingLight> ProcessPointLightCandidate(const Zenith_Frustum& xFrustum, const Zenith_LightRenderData& xData)
{
	if (!IsSphereFrustumVisible(xFrustum, xData.m_xWorldPosition, xData.m_fRange)) return std::nullopt;

	PendingLight xL;
	xL.m_eType = PendingLight::POINT;
	xL.m_xColor = xData.m_xColor;
	xL.m_fIntensity = xData.m_fIntensity;
	xL.m_xPosition = xData.m_xWorldPosition;
	xL.m_fRange = xData.m_fRange;
	xL.m_xDirection = Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f);
	xL.m_fCosInner = 0.0f;
	xL.m_fCosOuter = 0.0f;
	return xL;
}

// Build a candidate spot-light. Returns nullopt if zero-direction (logs and
// skips) or frustum-culled. Validates inner <= outer and outer angle range.
static std::optional<PendingLight> ProcessSpotLightCandidate(const Zenith_Frustum& xFrustum, const Zenith_LightRenderData& xData)
{
	Zenith_Maths::Vector3 xPosition = xData.m_xWorldPosition;
	float fRange = xData.m_fRange;

	float fInnerAngle = xData.m_fSpotInnerAngle;
	float fOuterAngle = xData.m_fSpotOuterAngle;
	Zenith_Assert(fInnerAngle <= fOuterAngle,
		"Spot light inner angle (%.2f) must be <= outer angle (%.2f)", fInnerAngle, fOuterAngle);
	Zenith_Assert(fOuterAngle > 0.0f && fOuterAngle < glm::pi<float>(),
		"Spot light outer angle (%.2f) out of valid range", fOuterAngle);

	Zenith_Maths::Vector3 xDirection = xData.m_xWorldDirection;
	float fDirLength = Zenith_Maths::Length(xDirection);
	if (fDirLength < fDIRECTION_EPSILON)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Skipping spot light with zero-length direction (Entity %u)", xData.m_uEntityIndex);
		return std::nullopt;
	}
	xDirection /= fDirLength;

	if (!IsConeFrustumVisible(xFrustum, xPosition, xDirection, fRange, fOuterAngle)) return std::nullopt;

	PendingLight xL;
	xL.m_eType = PendingLight::SPOT;
	xL.m_xColor = xData.m_xColor;
	xL.m_fIntensity = xData.m_fIntensity;
	xL.m_xPosition = xPosition;
	xL.m_fRange = fRange;
	xL.m_xDirection = xDirection;
	xL.m_fCosInner = cosf(fInnerAngle);
	xL.m_fCosOuter = cosf(fOuterAngle);
	return xL;
}

// Build a candidate directional-light. Returns nullopt if zero-direction.
// Directionals are not frustum-culled (they're effectively at infinity).
static std::optional<PendingLight> ProcessDirectionalLightCandidate(const Zenith_LightRenderData& xData)
{
	Zenith_Maths::Vector3 xDirection = xData.m_xWorldDirection;
	float fDirLength = Zenith_Maths::Length(xDirection);
	if (fDirLength < fDIRECTION_EPSILON)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Skipping directional light with zero-length direction (Entity %u)", xData.m_uEntityIndex);
		return std::nullopt;
	}
	xDirection /= fDirLength;

	PendingLight xL;
	xL.m_eType = PendingLight::DIRECTIONAL;
	xL.m_xColor = xData.m_xColor;
	xL.m_fIntensity = xData.m_fIntensity;
	xL.m_xPosition = Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f);
	xL.m_fRange = 0.0f;
	xL.m_xDirection = xDirection;
	xL.m_fCosInner = 0.0f;
	xL.m_fCosOuter = 0.0f;
	return xL;
}

// Stage all pending lights into xImpl.m_axLightStaging directional-first.
//
// CRITICAL INVARIANT: directional-first ordering must hold both before AND
// after priority trimming. The clustering compute shader iterates the
// unified buffer in order and clamps at MAX_LIGHTS_PER_CLUSTER (64) per
// cluster — packing directionals at the front guarantees they're never
// silently dropped by a cluster that's saturated by point/spot lights at
// lower indices. The CPU priority sort gives directionals effectively-
// infinite priority so they're never dropped; the two-pass stage below
// keeps them at the *front* of the buffer too.
//
// Under the cap: stage in arrival order, directional-first.
// Over the cap: priority-sort descending, keep the top uMAX_LIGHTS, then
// stage that subset directional-first.
static void StageLightsWithPriority(Flux_DynamicLightsImpl& xImpl, const Flux_GraphicsImpl& xFluxGraphics, u_int& uLightCount,
	const Zenith_Vector<PendingLight>& xPending, u_int uTotal)
{
	const u_int uMAX_LIGHTS = Flux_DynamicLightsImpl::uMAX_LIGHTS;

	if (uTotal <= uMAX_LIGHTS)
	{
		for (u_int i = 0; i < uTotal; ++i)
			if (xPending.Get(i).m_eType == PendingLight::DIRECTIONAL) StagePending(xImpl, uLightCount, xPending.Get(i));
		for (u_int i = 0; i < uTotal; ++i)
			if (xPending.Get(i).m_eType != PendingLight::DIRECTIONAL) StagePending(xImpl, uLightCount, xPending.Get(i));
		return;
	}

	xImpl.m_xSortBuffer.Clear();
	for (u_int i = 0; i < uTotal; ++i)
	{
		xImpl.m_xSortBuffer.PushBack({ xPending.Get(i).GetSortPriority(xFluxGraphics), i });
	}
	std::sort(xImpl.m_xSortBuffer.GetDataPointer(),
			  xImpl.m_xSortBuffer.GetDataPointer() + xImpl.m_xSortBuffer.GetSize());

	for (u_int i = 0; i < uMAX_LIGHTS; ++i)
	{
		const PendingLight& xL = xPending.Get(xImpl.m_xSortBuffer.Get(i).m_uIndex);
		if (xL.m_eType == PendingLight::DIRECTIONAL) StagePending(xImpl, uLightCount, xL);
	}
	for (u_int i = 0; i < uMAX_LIGHTS; ++i)
	{
		const PendingLight& xL = xPending.Get(xImpl.m_xSortBuffer.Get(i).m_uIndex);
		if (xL.m_eType != PendingLight::DIRECTIONAL) StagePending(xImpl, uLightCount, xL);
	}

	const u_int uDropped = uTotal - uMAX_LIGHTS;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Dropped %u dynamic lights (limit: %u, kept highest priority)",
		uDropped, uMAX_LIGHTS);
}

// Verify the directional-first invariant on the staging buffer. Both branches
// of StageLightsWithPriority (under-cap and over-cap) walk directionals first
// then non-directionals; any directional appearing AFTER a non-directional
// would mean a future refactor broke the ordering and the clustering compute
// shader could silently drop directionals when a cluster's 64-light cap is
// reached. Cheap O(N) walk in debug builds; compiles to nothing in release.
#ifdef ZENITH_ASSERT
namespace
{
	void AssertDirectionalFirstInvariant(const Flux_DynamicLightsImpl& xImpl, u_int uLightCount)
	{
		bool bSeenNonDirectional = false;
		for (u_int i = 0; i < uLightCount; ++i)
		{
			const bool bIsDirectional = xImpl.m_axLightStaging[i].m_xTypeOuter.y == fLIGHT_TYPE_TAG_DIRECTIONAL;
			if (!bIsDirectional)
			{
				bSeenNonDirectional = true;
			}
			else
			{
				Zenith_Assert(!bSeenNonDirectional,
					"Flux_DynamicLights: directional-first staging invariant broken — "
					"directional light at index %u follows a non-directional light", i);
			}
		}
	}
}
#endif

void Flux_DynamicLightsImpl::GatherLightsFromScene()
{
	m_uLightCount = 0;

	// Toggling dynamic lights off resets the count to zero. The clustering
	// pass still dispatches every frame to clear stale cluster counts —
	// see Flux_LightClustering.cpp's execute callback.
	if (!Zenith_GraphicsOptions::Get().m_bDynamicLightsVisible)
	{
		// Still upload an empty buffer so the GPU sees count = 0. The
		// LightBuffer is a structured buffer; an unwritten frame would
		// otherwise show stale data from a previous frame at indices the
		// clustering shader tries to read. Counts are bounded by
		// m_uLightCount so this is belt-and-braces only.
		return;
	}

	m_xCameraFrustum.ExtractFromViewProjection(g_xEngine.FluxGraphics().GetViewProjMatrix());

	const Zenith_Frustum& xFrustum = m_xCameraFrustum;

	// Collect candidates first, then priority-sort if we exceed the cap.
	// Single allocation per frame would be ideal — Zenith_Vector grows
	// geometrically so amortised cost is fine for typical light counts.
	Zenith_Vector<PendingLight> xPending;
	xPending.Reserve(uMAX_LIGHTS * 2);

	// Wave 3: lights arrive from the EC-side gatherer as renderer-neutral data.
	// The renderer keeps the intensity threshold + frustum cull + candidate build.
	Zenith_Vector<Zenith_LightRenderData> xLights;
	if (g_pfnZenithLightGather) g_pfnZenithLightGather(xLights);

	for (u_int uLight = 0; uLight < xLights.GetSize(); ++uLight)
	{
		const Zenith_LightRenderData& xData = xLights.Get(uLight);
		if (xData.m_fIntensity < fMIN_LIGHT_INTENSITY) continue;

		std::optional<PendingLight> xCandidate;
		switch (xData.m_eType)
		{
		case ZENITH_LIGHT_RENDER_POINT:       xCandidate = ProcessPointLightCandidate(xFrustum, xData); break;
		case ZENITH_LIGHT_RENDER_SPOT:        xCandidate = ProcessSpotLightCandidate(xFrustum, xData); break;
		case ZENITH_LIGHT_RENDER_DIRECTIONAL: xCandidate = ProcessDirectionalLightCandidate(xData); break;
		}
		if (xCandidate.has_value()) xPending.PushBack(*xCandidate);
	}

	StageLightsWithPriority(*this, g_xEngine.FluxGraphics(), m_uLightCount, xPending, xPending.GetSize());

#ifdef ZENITH_ASSERT
	AssertDirectionalFirstInvariant(*this, m_uLightCount);
#endif

	// Upload to GPU. Flux_MemoryManager::UploadBufferData handles the
	// memory barrier so transfer writes finish before the clustering
	// compute reads the buffer.
	if (m_uLightCount > 0)
	{
		g_xEngine.FluxMemory().UploadBufferData(
			m_xLightBuffer.GetBuffer().m_xVRAMHandle,
			m_axLightStaging,
			m_uLightCount * sizeof(LightInstance));
	}
}

// ========== ACCESSORS FOR DOWNSTREAM PASSES ==========

Flux_ShaderResourceView_Buffer& Flux_DynamicLightsImpl::GetLightBufferSRV()
{
	return m_xLightBuffer.GetSRV();
}


