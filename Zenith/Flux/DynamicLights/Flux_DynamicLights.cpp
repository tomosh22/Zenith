#include "Zenith.h"

#include "Flux/DynamicLights/Flux_DynamicLights.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_Query.h"
#include "EntityComponent/Components/Zenith_LightComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
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

// 64 bytes — sized to spot's worst-case footprint so all light types
// share one struct. Padding fields stay zero on CPU staging.
struct LightInstance
{
	Zenith_Maths::Vector4 m_xPositionRange;   // xyz=pos, w=range
	Zenith_Maths::Vector4 m_xColorIntensity;  // xyz=color, w=intensity
	Zenith_Maths::Vector4 m_xDirectionInner;  // xyz=dir, w=cos(inner)
	Zenith_Maths::Vector4 m_xTypeOuter;       // x=cos(outer), y=type tag, zw=pad
};
static_assert(sizeof(LightInstance) == 64, "LightInstance must be 64 bytes — must match Common.Lighting.slang LightInstance");

// Light type tags must match Common.Lighting.slang LIGHT_TYPE_*.
static constexpr float fLIGHT_TYPE_TAG_POINT       = 0.0f;
static constexpr float fLIGHT_TYPE_TAG_SPOT        = 1.0f;
static constexpr float fLIGHT_TYPE_TAG_DIRECTIONAL = 2.0f;

// ========== STATIC STATE ==========

bool Flux_DynamicLights::s_bInitialised = false;

// Cached frustum for culling (updated each frame).
static Zenith_Frustum s_xCameraFrustum;

// CPU staging — flat array of all light types, packed by GatherLightsFromScene.
static LightInstance s_axLightStaging[Flux_DynamicLights::uMAX_LIGHTS];
static u_int         s_uLightCount = 0;

// GPU-side: host-visible, frame-indexed. Host-visible is correct here
// because this buffer is CPU-uploaded each frame (unlike the cluster
// outputs which are GPU-written and use Flux_ReadWriteBuffer).
static Flux_DynamicReadWriteBuffer s_xLightBuffer;

// Direction vector normalization epsilon (prevents NaN from zero-length vectors).
static constexpr float fDIRECTION_EPSILON = 0.0001f;

// Minimum intensity threshold - lights below this are skipped.
static constexpr float fMIN_LIGHT_INTENSITY = 0.001f;

// ========== FRUSTUM CULLING HELPERS ==========

// Test if a sphere (point light bounding volume) intersects the camera frustum.
static bool IsSphereFrustumVisible(const Zenith_Maths::Vector3& xCenter, float fRadius)
{
	for (int i = 0; i < 6; ++i)
	{
		const Zenith_Plane& xPlane = s_xCameraFrustum.m_axPlanes[i];
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

	if (!IsSphereFrustumVisible(xBoundCenter, fBoundRadius))
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
		const Zenith_Plane& xPlane = s_xCameraFrustum.m_axPlanes[i];

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

// Used when total lights exceed uMAX_LIGHTS — pick the highest-priority
// uMAX_LIGHTS to keep, drop the rest.
struct LightSortKey
{
	float m_fPriority;
	u_int m_uIndex;

	bool operator<(const LightSortKey& other) const
	{
		// Sort descending by priority — highest first.
		return m_fPriority > other.m_fPriority;
	}
};
static Zenith_Vector<LightSortKey> s_xSortBuffer;

static float CalculateLightPriority(const Zenith_Maths::Vector3& xLightPos, float fIntensity, float fRange)
{
	Zenith_Maths::Vector3 xCamPos = g_xEngine.FluxGraphics().m_xFrameConstants.m_xCamPos_Pad;
	float fDistance = Zenith_Maths::Length(xLightPos - xCamPos);
	return (fIntensity * fRange) / (fDistance + 1.0f);
}

// ========== PUBLIC API ==========

void Flux_DynamicLights::Initialise()
{
	// One flat GPU buffer for all lights (point + spot + directional).
	const u_int64 ulLightBufferSize = uMAX_LIGHTS * sizeof(LightInstance);

	// Zero-initialize so frame-0 reads don't see garbage.
	Zenith_Vector<LightInstance> xZeroed(uMAX_LIGHTS);
	for (u_int u = 0; u < uMAX_LIGHTS; ++u) xZeroed.EmplaceBack();
	Flux_MemoryManager::InitialiseDynamicReadWriteBuffer(xZeroed.GetDataPointer(), ulLightBufferSize, s_xLightBuffer);

	// Pre-allocate priority sort buffer to avoid per-frame allocs.
	s_xSortBuffer.Reserve(uMAX_LIGHTS * 2);

	s_uLightCount = 0;

	s_bInitialised = true;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_DynamicLights initialised (clustered-deferred gather front-end, max %u lights)", uMAX_LIGHTS);
}

void Flux_DynamicLights::Shutdown()
{
	if (!s_bInitialised)
	{
		return;
	}

	Flux_MemoryManager::DestroyDynamicReadWriteBuffer(s_xLightBuffer);
	s_uLightCount = 0;
	s_bInitialised = false;

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_DynamicLights shut down");
}

void Flux_DynamicLights::Reset()
{
	// Light count is reset in GatherLightsFromScene().
}

// ========== GATHER ==========

// Stage helpers — pack each accepted light into the unified buffer.

static void StagePointLight(const Zenith_Maths::Vector3& xPosition, float fRange,
	const Zenith_Maths::Vector3& xColor, float fIntensity)
{
	if (s_uLightCount >= Flux_DynamicLights::uMAX_LIGHTS) return;
	LightInstance& xOut = s_axLightStaging[s_uLightCount++];
	xOut.m_xPositionRange  = { xPosition.x, xPosition.y, xPosition.z, fRange };
	xOut.m_xColorIntensity = { xColor.x, xColor.y, xColor.z, fIntensity };
	xOut.m_xDirectionInner = { 0.0f, 0.0f, 0.0f, 0.0f };
	xOut.m_xTypeOuter      = { 0.0f, fLIGHT_TYPE_TAG_POINT, 0.0f, 0.0f };
}

static void StageSpotLight(const Zenith_Maths::Vector3& xPosition, float fRange,
	const Zenith_Maths::Vector3& xColor, float fIntensity,
	const Zenith_Maths::Vector3& xDirection, float fCosInner, float fCosOuter)
{
	if (s_uLightCount >= Flux_DynamicLights::uMAX_LIGHTS) return;
	LightInstance& xOut = s_axLightStaging[s_uLightCount++];
	xOut.m_xPositionRange  = { xPosition.x, xPosition.y, xPosition.z, fRange };
	xOut.m_xColorIntensity = { xColor.x, xColor.y, xColor.z, fIntensity };
	xOut.m_xDirectionInner = { xDirection.x, xDirection.y, xDirection.z, fCosInner };
	xOut.m_xTypeOuter      = { fCosOuter, fLIGHT_TYPE_TAG_SPOT, 0.0f, 0.0f };
}

static void StageDirectionalLight(const Zenith_Maths::Vector3& xDirection,
	const Zenith_Maths::Vector3& xColor, float fIntensity)
{
	if (s_uLightCount >= Flux_DynamicLights::uMAX_LIGHTS) return;
	LightInstance& xOut = s_axLightStaging[s_uLightCount++];
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

	float GetSortPriority() const
	{
		if (m_eType == DIRECTIONAL) return 1e30f; // never drop directionals on sort
		return CalculateLightPriority(m_xPosition, m_fIntensity, m_fRange);
	}
};

static void StagePending(const PendingLight& xL)
{
	switch (xL.m_eType)
	{
	case PendingLight::POINT:
		StagePointLight(xL.m_xPosition, xL.m_fRange, xL.m_xColor, xL.m_fIntensity);
		break;
	case PendingLight::SPOT:
		StageSpotLight(xL.m_xPosition, xL.m_fRange, xL.m_xColor, xL.m_fIntensity,
			xL.m_xDirection, xL.m_fCosInner, xL.m_fCosOuter);
		break;
	case PendingLight::DIRECTIONAL:
		StageDirectionalLight(xL.m_xDirection, xL.m_xColor, xL.m_fIntensity);
		break;
	}
}

// Build a candidate point-light from a Zenith_LightComponent. Returns nullopt
// if the light is frustum-culled.
static std::optional<PendingLight> ProcessPointLightCandidate(Zenith_LightComponent& xLight,
	const Zenith_Maths::Vector3& xColor, float fIntensity)
{
	Zenith_Maths::Vector3 xPosition = xLight.GetWorldPosition();
	float fRange = xLight.GetRange();
	if (!IsSphereFrustumVisible(xPosition, fRange)) return std::nullopt;

	PendingLight xL;
	xL.m_eType = PendingLight::POINT;
	xL.m_xColor = xColor;
	xL.m_fIntensity = fIntensity;
	xL.m_xPosition = xPosition;
	xL.m_fRange = fRange;
	xL.m_xDirection = Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f);
	xL.m_fCosInner = 0.0f;
	xL.m_fCosOuter = 0.0f;
	return xL;
}

// Build a candidate spot-light. Returns nullopt if zero-direction (logs and
// skips) or frustum-culled. Validates inner <= outer and outer angle range.
static std::optional<PendingLight> ProcessSpotLightCandidate(Zenith_LightComponent& xLight,
	const Zenith_Maths::Vector3& xColor, float fIntensity, Zenith_EntityID uID)
{
	Zenith_Maths::Vector3 xPosition = xLight.GetWorldPosition();
	float fRange = xLight.GetRange();

	float fInnerAngle = xLight.GetSpotInnerAngle();
	float fOuterAngle = xLight.GetSpotOuterAngle();
	Zenith_Assert(fInnerAngle <= fOuterAngle,
		"Spot light inner angle (%.2f) must be <= outer angle (%.2f)", fInnerAngle, fOuterAngle);
	Zenith_Assert(fOuterAngle > 0.0f && fOuterAngle < glm::pi<float>(),
		"Spot light outer angle (%.2f) out of valid range", fOuterAngle);

	Zenith_Maths::Vector3 xDirection = xLight.GetWorldDirection();
	float fDirLength = Zenith_Maths::Length(xDirection);
	if (fDirLength < fDIRECTION_EPSILON)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Skipping spot light with zero-length direction (Entity %u)", uID.m_uIndex);
		return std::nullopt;
	}
	xDirection /= fDirLength;

	if (!IsConeFrustumVisible(xPosition, xDirection, fRange, fOuterAngle)) return std::nullopt;

	PendingLight xL;
	xL.m_eType = PendingLight::SPOT;
	xL.m_xColor = xColor;
	xL.m_fIntensity = fIntensity;
	xL.m_xPosition = xPosition;
	xL.m_fRange = fRange;
	xL.m_xDirection = xDirection;
	xL.m_fCosInner = cosf(fInnerAngle);
	xL.m_fCosOuter = cosf(fOuterAngle);
	return xL;
}

// Build a candidate directional-light. Returns nullopt if zero-direction.
// Directionals are not frustum-culled (they're effectively at infinity).
static std::optional<PendingLight> ProcessDirectionalLightCandidate(Zenith_LightComponent& xLight,
	const Zenith_Maths::Vector3& xColor, float fIntensity, Zenith_EntityID uID)
{
	Zenith_Maths::Vector3 xDirection = xLight.GetWorldDirection();
	float fDirLength = Zenith_Maths::Length(xDirection);
	if (fDirLength < fDIRECTION_EPSILON)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Skipping directional light with zero-length direction (Entity %u)", uID.m_uIndex);
		return std::nullopt;
	}
	xDirection /= fDirLength;

	PendingLight xL;
	xL.m_eType = PendingLight::DIRECTIONAL;
	xL.m_xColor = xColor;
	xL.m_fIntensity = fIntensity;
	xL.m_xPosition = Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f);
	xL.m_fRange = 0.0f;
	xL.m_xDirection = xDirection;
	xL.m_fCosInner = 0.0f;
	xL.m_fCosOuter = 0.0f;
	return xL;
}

// Stage all pending lights into s_axLightStaging directional-first.
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
static void StageLightsWithPriority(const Zenith_Vector<PendingLight>& xPending, u_int uTotal)
{
	const u_int uMAX_LIGHTS = Flux_DynamicLights::uMAX_LIGHTS;

	if (uTotal <= uMAX_LIGHTS)
	{
		for (u_int i = 0; i < uTotal; ++i)
			if (xPending.Get(i).m_eType == PendingLight::DIRECTIONAL) StagePending(xPending.Get(i));
		for (u_int i = 0; i < uTotal; ++i)
			if (xPending.Get(i).m_eType != PendingLight::DIRECTIONAL) StagePending(xPending.Get(i));
		return;
	}

	s_xSortBuffer.Clear();
	for (u_int i = 0; i < uTotal; ++i)
	{
		s_xSortBuffer.PushBack({ xPending.Get(i).GetSortPriority(), i });
	}
	std::sort(s_xSortBuffer.GetDataPointer(),
			  s_xSortBuffer.GetDataPointer() + s_xSortBuffer.GetSize());

	for (u_int i = 0; i < uMAX_LIGHTS; ++i)
	{
		const PendingLight& xL = xPending.Get(s_xSortBuffer.Get(i).m_uIndex);
		if (xL.m_eType == PendingLight::DIRECTIONAL) StagePending(xL);
	}
	for (u_int i = 0; i < uMAX_LIGHTS; ++i)
	{
		const PendingLight& xL = xPending.Get(s_xSortBuffer.Get(i).m_uIndex);
		if (xL.m_eType != PendingLight::DIRECTIONAL) StagePending(xL);
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
	void AssertDirectionalFirstInvariant()
	{
		bool bSeenNonDirectional = false;
		for (u_int i = 0; i < s_uLightCount; ++i)
		{
			const bool bIsDirectional = s_axLightStaging[i].m_xTypeOuter.y == fLIGHT_TYPE_TAG_DIRECTIONAL;
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

void Flux_DynamicLights::GatherLightsFromScene()
{
	s_uLightCount = 0;

	// Toggling dynamic lights off resets the count to zero. The clustering
	// pass still dispatches every frame to clear stale cluster counts —
	// see Flux_LightClustering.cpp's execute callback.
	if (!Zenith_GraphicsOptions::Get().m_bDynamicLightsVisible)
	{
		// Still upload an empty buffer so the GPU sees count = 0. The
		// LightBuffer is a structured buffer; an unwritten frame would
		// otherwise show stale data from a previous frame at indices the
		// clustering shader tries to read. Counts are bounded by
		// s_uLightCount so this is belt-and-braces only.
		return;
	}

	s_xCameraFrustum.ExtractFromViewProjection(Flux_Graphics::GetViewProjMatrix());

	// Collect candidates first, then priority-sort if we exceed the cap.
	// Single allocation per frame would be ideal — Zenith_Vector grows
	// geometrically so amortised cost is fine for typical light counts.
	Zenith_Vector<PendingLight> xPending;
	xPending.Reserve(uMAX_LIGHTS * 2);

	for (uint32_t uSceneSlot = 0; uSceneSlot < Zenith_SceneManager::GetSceneSlotCount(); ++uSceneSlot)
	{
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneDataAtSlot(uSceneSlot);
		if (!pxSceneData || !pxSceneData->IsLoaded() || pxSceneData->IsUnloading()) continue;

		pxSceneData->Query<Zenith_LightComponent, Zenith_TransformComponent>()
			.ForEach([&xPending](Zenith_EntityID uID, Zenith_LightComponent& xLight, Zenith_TransformComponent&)
		{
			LIGHT_TYPE eType = xLight.GetLightType();
			Zenith_Assert(eType < LIGHT_TYPE_COUNT, "Invalid light type: %u", static_cast<u_int>(eType));

			const Zenith_Maths::Vector3& xColor = xLight.GetColor();
			float fIntensity = xLight.GetIntensity();
			if (fIntensity < fMIN_LIGHT_INTENSITY) return;

			std::optional<PendingLight> xCandidate;
			switch (eType)
			{
			case LIGHT_TYPE_POINT:       xCandidate = ProcessPointLightCandidate(xLight, xColor, fIntensity); break;
			case LIGHT_TYPE_SPOT:        xCandidate = ProcessSpotLightCandidate(xLight, xColor, fIntensity, uID); break;
			case LIGHT_TYPE_DIRECTIONAL: xCandidate = ProcessDirectionalLightCandidate(xLight, xColor, fIntensity, uID); break;
			default: return;
			}
			if (xCandidate.has_value()) xPending.PushBack(*xCandidate);
		});
	}

	StageLightsWithPriority(xPending, xPending.GetSize());

#ifdef ZENITH_ASSERT
	AssertDirectionalFirstInvariant();
#endif

	// Upload to GPU. Flux_MemoryManager::UploadBufferData handles the
	// memory barrier so transfer writes finish before the clustering
	// compute reads the buffer.
	if (s_uLightCount > 0)
	{
		Flux_MemoryManager::UploadBufferData(
			s_xLightBuffer.GetBuffer().m_xVRAMHandle,
			s_axLightStaging,
			s_uLightCount * sizeof(LightInstance));
	}
}

// ========== ACCESSORS FOR DOWNSTREAM PASSES ==========

Flux_ShaderResourceView_Buffer& Flux_DynamicLights::GetLightBufferSRV()
{
	return s_xLightBuffer.GetSRV();
}

Flux_DynamicReadWriteBuffer& Flux_DynamicLights::GetLightBuffer()
{
	return s_xLightBuffer;
}

u_int Flux_DynamicLights::GetLightCount()
{
	return s_uLightCount;
}
