#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Shadows/Flux_ShadowsImpl.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"

#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_BackendTypes.h"
#include "Profiling/Zenith_Profiling.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/AnimatedMeshes/Flux_AnimatedMeshesImpl.h"
#include "Flux/StaticMeshes/Flux_StaticMeshesImpl.h"
#include "Flux/Terrain/Flux_TerrainImpl.h"

// Graph-owned transient — backing Flux_RenderAttachment is allocated and
// destroyed by the render graph, sized from the descriptor in SetupRenderGraph.
// CSM_FORMAT is declared in Flux_Shadows.h so subsystems that build shadow
// pipelines at Initialise() time can reference it.




DEBUGVAR float dbg_fZMultiplier = 8.f;

Flux_RenderAttachment& Flux_ShadowsImpl::GetCSM(u_int uIndex)
{
	Zenith_Assert(m_pxGraph, "Flux_ShadowsImpl::GetCSM: graph pointer is null");
	return m_pxGraph->GetTransientAttachment(m_axCSMHandles[uIndex]);
}

struct FrustumCorners
{
	const Zenith_Maths::Vector3 GetCenter() const
	{
		Zenith_Maths::Vector3 xRet(0,0,0);
		for (uint32_t u = 0; u < 8; u++) xRet += m_axCorners[u];
		xRet /= 8;
		return xRet;
	}
	Zenith_Maths::Vector3 m_axCorners[8];
};

struct AABB3D
{
	Zenith_Maths::Vector3 m_xMin = Zenith_Maths::Vector3(FLT_MAX);
	Zenith_Maths::Vector3 m_xMax = Zenith_Maths::Vector3(-FLT_MAX);

	void Expand(const Zenith_Maths::Vector3& xPoint)
	{
		m_xMin = glm::min(m_xMin, xPoint);
		m_xMax = glm::max(m_xMax, xPoint);
	}
};

static AABB3D ComputeAABBFromTransformedPoints(const Zenith_Maths::Vector3* pxPoints, uint32_t uCount, const Zenith_Maths::Matrix4& xTransform)
{
	AABB3D xAABB;
	for (uint32_t u = 0; u < uCount; u++)
	{
		xAABB.Expand(xTransform * Zenith_Maths::Vector4(pxPoints[u], 1));
	}
	return xAABB;
}

static FrustumCorners WorldSpaceFrustumCornersFromInverseViewProjMatrix(const Zenith_Maths::Matrix4& xInvViewProjMat)
{
	FrustumCorners xRet;
	uint32_t uCount = 0;
	for (uint32_t uX = 0; uX < 2; uX++)
	{
		for (uint32_t uY = 0; uY < 2; uY++)
		{
			for (uint32_t uZ = 0; uZ < 2; uZ++)
			{
				Zenith_Maths::Vector4 xCorner = xInvViewProjMat * Zenith_Maths::Vector4(2.f * uX - 1.f, 2.f * uY - 1.f, uZ ? 1.f : 0.f, 1.f);
				xRet.m_axCorners[uCount++] = Zenith_Maths::Vector3(xCorner) / xCorner.w;
			}
		}
	}

	return xRet;
}

void Flux_ShadowsImpl::Initialise(Flux_MemoryManager& xVulkanMemory, Flux_GraphicsImpl& xFluxGraphics, Zenith_Profiling& xProfiling)
{
	m_pxVulkanMemory = &xVulkanMemory;
	m_pxFluxGraphics = &xFluxGraphics;
	m_pxProfiling = &xProfiling;

	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		m_pxVulkanMemory->InitialiseDynamicConstantBuffer(nullptr, sizeof(Zenith_Maths::Matrix4), m_xShadowMatrixBuffers[u]);
	}

#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddFloat({"Render", "Shadows", "Z Multiplier"}, dbg_fZMultiplier, -10.f, 10.f);
#endif
}

void Flux_ShadowsImpl::Shutdown()
{
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		m_pxVulkanMemory->DestroyDynamicConstantBuffer(m_xShadowMatrixBuffers[u]);
	}

	m_pxGraph = nullptr;

	m_pxVulkanMemory = nullptr;
	m_pxFluxGraphics = nullptr;
	m_pxProfiling = nullptr;
}

// Persistent pass names (W1: prevents dangling stack-buffer pointers passed to AddPass).
static const char* const s_aszShadowCascadePassNames[ZENITH_FLUX_NUM_CSMS] =
{
	"Shadow Cascade 0",
	"Shadow Cascade 1",
	"Shadow Cascade 2",
	"Shadow Cascade 3",
};
static_assert(ZENITH_FLUX_NUM_CSMS == 4, "s_aszShadowCascadePassNames must match ZENITH_FLUX_NUM_CSMS");

// Cascade index is passed through the graph's typed user-data slot — see
// Flux_PassBuilder::UserData<T> / Flux_UnpackUserData<T> in Flux_RenderGraph.h.

static void PreExecuteShadowMatrices(void*)
{
	// CPU-side shadow matrix update runs once per frame on the main thread
	// before parallel cascade recording begins. Runs as a Prepare callback on
	// cascade 0 so the cascade Execute callbacks can then record in parallel.
	if (!Zenith_GraphicsOptions::Get().m_bShadowsEnabled)
	{
		return;
	}
	g_xEngine.Shadows().UpdateShadowMatrices();
}

static void ExecuteShadowCascade(Flux_CommandList* pxCommandList, void* pUserData)
{
	if (!Zenith_GraphicsOptions::Get().m_bShadowsEnabled)
	{
		return;
	}

	const uint32_t u = Flux_UnpackUserData<uint32_t>(pUserData);

	// Non-capturing trampoline: recover the Shadows singleton, then route its
	// own state (matrix buffers) through xZZ. Sibling subsystems (StaticMeshes /
	// AnimatedMeshes) are recovered as their own singletons.
	Flux_ShadowsImpl& xZZ = g_xEngine.Shadows();

	auto& xStaticMeshes = g_xEngine.StaticMeshes();
	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&xStaticMeshes.GetShadowPipeline());

	// RenderToShadowMap handles all bindings via shader reflection
	xStaticMeshes.RenderToShadowMap(*pxCommandList, xZZ.m_xShadowMatrixBuffers[u]);

	auto& xAnimatedMeshes = g_xEngine.AnimatedMeshes();
	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&xAnimatedMeshes.GetShadowPipeline());

	// RenderToShadowMap handles all bindings via shader reflection
	xAnimatedMeshes.RenderToShadowMap(*pxCommandList, xZZ.m_xShadowMatrixBuffers[u]);

	// #TODO: Enable terrain shadow casting
	// g_xEngine.Terrain().RenderToShadowMap(*pxCommandList, xZZ.m_xShadowMatrixBuffers[u]);
}

void Flux_ShadowsImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	m_pxGraph = &xGraph;

	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		Flux_TransientTextureDesc xCSMDesc;
		xCSMDesc.m_uWidth = ZENITH_FLUX_CSM_RESOLUTION;
		xCSMDesc.m_uHeight = ZENITH_FLUX_CSM_RESOLUTION;
		xCSMDesc.m_eFormat = CSM_FORMAT;
		xCSMDesc.m_uMemoryFlags = 1u << MEMORY_FLAGS__SHADER_READ;
		xCSMDesc.m_bIsDepthStencil = true;

		m_axCSMHandles[u] = xGraph.CreateTransient(xCSMDesc);
	}

	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		// CSM targets are depth textures — declared as DSV writes, not RTV.
		// Cascade 0 owns the CPU-side matrix update via its pre-execute callback;
		// other cascades have no GPU dependency on each other so they record in parallel.
		const Flux_PassHandle xPass = xGraph.AddPass(s_aszShadowCascadePassNames[u], ExecuteShadowCascade)
			.UserData(u)
			.ClearTargets()
			.WritesTransient(m_axCSMHandles[u], RESOURCE_ACCESS_WRITE_DSV);
		if (u == 0)
			xGraph.SetPrepare(xPass, PreExecuteShadowMatrices);
	}
}

// Contract:
//   - Returns the colour attachment pointer (currently always nullptr — CSMs are
//     depth-only today; the return type is reserved for future variance/moment
//     shadow map variants that would carry a colour cascade).
//   - uNumColour is set to the number of colour attachments (0 today).
//   - pxDepthStencil is ALWAYS populated with a non-null Flux_RenderAttachment*
//     for a valid uIndex < ZENITH_FLUX_NUM_CSMS. Callers may dereference it
//     unconditionally.
//
// Callers (Flux_AnimatedMeshes / StaticMeshes / InstancedMeshes / Terrain /
// DeferredShading) rely on the non-null pxDepthStencil guarantee; if this ever
// changes, every caller must be audited for null guards.
Flux_RenderAttachment* Flux_ShadowsImpl::GetCSMTargetSetup(const uint32_t uIndex, uint32_t& uNumColour, Flux_RenderAttachment*& pxDepthStencil)
{
	Zenith_Assert(uIndex < ZENITH_FLUX_NUM_CSMS, "GetCSMTargetSetup: cascade index %u out of bounds (max %u)", uIndex, ZENITH_FLUX_NUM_CSMS);
	uNumColour = 0;
	pxDepthStencil = &GetCSM(uIndex);
	Zenith_Assert(pxDepthStencil != nullptr, "GetCSMTargetSetup: depth-stencil out-param must be non-null per contract");
	return nullptr;
}


Flux_ShaderResourceView& Flux_ShadowsImpl::GetCSMSRV(const uint32_t u)
{
	return Zenith_GraphicsOptions::Get().m_bShadowsEnabled ? GetCSM(u).SRV() : m_pxFluxGraphics->m_xWhiteTexture.GetDirect()->m_xSRV;
}


void Flux_ShadowsImpl::UpdateShadowMatrices()
{
	m_pxProfiling->BeginProfile(ZENITH_PROFILE_INDEX__FLUX_SHADOWS_UPDATE_MATRICES);
	const Zenith_Maths::Matrix4& xViewMat = m_pxFluxGraphics->GetViewMatrix();

	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		const float fNearPlane = m_pxFluxGraphics->GetFarPlane() / s_afCSMLevels[u];
		const float fFarPlane = m_pxFluxGraphics->GetFarPlane() / s_afCSMLevels[u + 1];

		const Zenith_Maths::Matrix4 xProjMat = Zenith_Maths::PerspectiveProjection(m_pxFluxGraphics->GetFOV(), m_pxFluxGraphics->GetAspectRatio(), fNearPlane, fFarPlane);
		const Zenith_Maths::Matrix4 xInvViewProjMat = glm::inverse(xProjMat * xViewMat);

		const FrustumCorners xFrustumCorners = WorldSpaceFrustumCornersFromInverseViewProjMatrix(xInvViewProjMat);
		const Zenith_Maths::Vector3 xFrustumCenter = xFrustumCorners.GetCenter();

		const Zenith_Maths::Vector3& xSunDir = m_pxFluxGraphics->GetSunDir();
		const Zenith_Maths::Vector3 xUp(0, 1, 0);

		Zenith_Maths::Matrix4 xSunViewMat = glm::lookAt(xFrustumCenter - xSunDir, xFrustumCenter, xUp);

		const AABB3D xLightAABB = ComputeAABBFromTransformedPoints(xFrustumCorners.m_axCorners, 8, xSunViewMat);

		const float fZRange = xLightAABB.m_xMax.z - xLightAABB.m_xMin.z;

		xSunViewMat = glm::lookAt(xFrustumCenter - xSunDir * (xLightAABB.m_xMax.z + fZRange * dbg_fZMultiplier), xFrustumCenter, xUp);

		m_axSunViewProjMats[u] = glm::ortho(xLightAABB.m_xMin.x, xLightAABB.m_xMax.x, xLightAABB.m_xMin.y, xLightAABB.m_xMax.y, 0.0f, fZRange * (1.0f + 2.0f * dbg_fZMultiplier)) * xSunViewMat;

		m_pxVulkanMemory->UploadBufferData(m_xShadowMatrixBuffers[u].GetBuffer().m_xVRAMHandle, &m_axSunViewProjMats[u], sizeof(m_axSunViewProjMats[u]));
	}

	m_pxProfiling->EndProfile(ZENITH_PROFILE_INDEX__FLUX_SHADOWS_UPDATE_MATRICES);
}
