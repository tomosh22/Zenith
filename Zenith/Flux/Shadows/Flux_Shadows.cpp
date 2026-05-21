#include "Zenith.h"

#include "Flux/Shadows/Flux_Shadows.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"

#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/AnimatedMeshes/Flux_AnimatedMeshesImpl.h"
#include "Flux/StaticMeshes/Flux_StaticMeshesImpl.h"
#include "Flux/Terrain/Flux_Terrain.h"

// Graph-owned transient — backing Flux_RenderAttachment is allocated and
// destroyed by the render graph, sized from the descriptor in SetupRenderGraph.
// CSM_FORMAT is declared in Flux_Shadows.h so subsystems that build shadow
// pipelines at Initialise() time can reference it.




DEBUGVAR float dbg_fZMultiplier = 8.f;

static Flux_RenderAttachment& GetCSM(u_int uIndex)
{
	Zenith_Assert(g_xEngine.Shadows().m_pxGraph, "Flux_Shadows::GetCSM: graph pointer is null");
	return g_xEngine.Shadows().m_pxGraph->GetTransientAttachment(g_xEngine.Shadows().m_axCSMHandles[uIndex]);
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

void Flux_Shadows::Initialise()
{
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		Flux_MemoryManager::InitialiseDynamicConstantBuffer(nullptr, sizeof(Zenith_Maths::Matrix4), g_xEngine.Shadows().m_xShadowMatrixBuffers[u]);
	}

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddFloat({"Render", "Shadows", "Z Multiplier"}, dbg_fZMultiplier, -10.f, 10.f);
#endif
}

void Flux_Shadows::Shutdown()
{
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		Flux_MemoryManager::DestroyDynamicConstantBuffer(g_xEngine.Shadows().m_xShadowMatrixBuffers[u]);
	}

	g_xEngine.Shadows().m_pxGraph = nullptr;
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
	Flux_Shadows::UpdateShadowMatrices();
}

static void ExecuteShadowCascade(Flux_CommandList* pxCommandList, void* pUserData)
{
	if (!Zenith_GraphicsOptions::Get().m_bShadowsEnabled)
	{
		return;
	}

	const uint32_t u = Flux_UnpackUserData<uint32_t>(pUserData);

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.StaticMeshes().GetShadowPipeline());

	// RenderToShadowMap handles all bindings via shader reflection
	g_xEngine.StaticMeshes().RenderToShadowMap(*pxCommandList, g_xEngine.Shadows().m_xShadowMatrixBuffers[u]);

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.AnimatedMeshes().GetShadowPipeline());

	// RenderToShadowMap handles all bindings via shader reflection
	g_xEngine.AnimatedMeshes().RenderToShadowMap(*pxCommandList, g_xEngine.Shadows().m_xShadowMatrixBuffers[u]);

	// #TODO: Enable terrain shadow casting
	// Flux_Terrain::RenderToShadowMap(*pxCommandList, g_xEngine.Shadows().m_xShadowMatrixBuffers[u]);
}

void Flux_Shadows::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	g_xEngine.Shadows().m_pxGraph = &xGraph;

	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		Flux_TransientTextureDesc xCSMDesc;
		xCSMDesc.m_uWidth = ZENITH_FLUX_CSM_RESOLUTION;
		xCSMDesc.m_uHeight = ZENITH_FLUX_CSM_RESOLUTION;
		xCSMDesc.m_eFormat = CSM_FORMAT;
		xCSMDesc.m_uMemoryFlags = 1u << MEMORY_FLAGS__SHADER_READ;
		xCSMDesc.m_bIsDepthStencil = true;

		g_xEngine.Shadows().m_axCSMHandles[u] = xGraph.CreateTransient(xCSMDesc);
	}

	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		// CSM targets are depth textures — declared as DSV writes, not RTV.
		// Cascade 0 owns the CPU-side matrix update via its pre-execute callback;
		// other cascades have no GPU dependency on each other so they record in parallel.
		const Flux_PassHandle xPass = xGraph.AddPass(s_aszShadowCascadePassNames[u], ExecuteShadowCascade)
			.UserData(u)
			.ClearTargets()
			.WritesTransient(g_xEngine.Shadows().m_axCSMHandles[u], RESOURCE_ACCESS_WRITE_DSV);
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
Flux_RenderAttachment* Flux_Shadows::GetCSMTargetSetup(const uint32_t uIndex, uint32_t& uNumColour, Flux_RenderAttachment*& pxDepthStencil)
{
	Zenith_Assert(uIndex < ZENITH_FLUX_NUM_CSMS, "GetCSMTargetSetup: cascade index %u out of bounds (max %u)", uIndex, ZENITH_FLUX_NUM_CSMS);
	uNumColour = 0;
	pxDepthStencil = &GetCSM(uIndex);
	Zenith_Assert(pxDepthStencil != nullptr, "GetCSMTargetSetup: depth-stencil out-param must be non-null per contract");
	return nullptr;
}

Zenith_Maths::Matrix4 Flux_Shadows::GetSunViewProjMatrix(const uint32_t uIndex)
{
	return g_xEngine.Shadows().m_axSunViewProjMats[uIndex];
}

Flux_ShaderResourceView& Flux_Shadows::GetCSMSRV(const uint32_t u)
{
	return Zenith_GraphicsOptions::Get().m_bShadowsEnabled ? GetCSM(u).SRV() : g_xEngine.FluxGraphics().m_xWhiteTexture.GetDirect()->m_xSRV;
}

Flux_DynamicConstantBuffer& Flux_Shadows::GetShadowMatrixBuffer(const uint32_t u)
{
	return g_xEngine.Shadows().m_xShadowMatrixBuffers[u];
}

void Flux_Shadows::UpdateShadowMatrices()
{
	Zenith_Profiling::BeginProfile(ZENITH_PROFILE_INDEX__FLUX_SHADOWS_UPDATE_MATRICES);
	const Zenith_Maths::Matrix4& xViewMat = Flux_Graphics::GetViewMatrix();
	
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		const float fNearPlane = Flux_Graphics::GetFarPlane() / s_afCSMLevels[u];
		const float fFarPlane = Flux_Graphics::GetFarPlane() / s_afCSMLevels[u + 1];

		const Zenith_Maths::Matrix4 xProjMat = Zenith_Maths::PerspectiveProjection(Flux_Graphics::GetFOV(), Flux_Graphics::GetAspectRatio(), fNearPlane, fFarPlane);
		const Zenith_Maths::Matrix4 xInvViewProjMat = glm::inverse(xProjMat * xViewMat);

		const FrustumCorners xFrustumCorners = WorldSpaceFrustumCornersFromInverseViewProjMatrix(xInvViewProjMat);
		const Zenith_Maths::Vector3 xFrustumCenter = xFrustumCorners.GetCenter();

		const Zenith_Maths::Vector3& xSunDir = Flux_Graphics::GetSunDir();
		const Zenith_Maths::Vector3 xUp(0, 1, 0);
		
		Zenith_Maths::Matrix4 xSunViewMat = glm::lookAt(xFrustumCenter - xSunDir, xFrustumCenter, xUp);
		
		const AABB3D xLightAABB = ComputeAABBFromTransformedPoints(xFrustumCorners.m_axCorners, 8, xSunViewMat);

		const float fZRange = xLightAABB.m_xMax.z - xLightAABB.m_xMin.z;

		xSunViewMat = glm::lookAt(xFrustumCenter - xSunDir * (xLightAABB.m_xMax.z + fZRange * dbg_fZMultiplier), xFrustumCenter, xUp);

		g_xEngine.Shadows().m_axSunViewProjMats[u] = glm::ortho(xLightAABB.m_xMin.x, xLightAABB.m_xMax.x, xLightAABB.m_xMin.y, xLightAABB.m_xMax.y, 0.0f, fZRange * (1.0f + 2.0f * dbg_fZMultiplier)) * xSunViewMat;

		Flux_MemoryManager::UploadBufferData(g_xEngine.Shadows().m_xShadowMatrixBuffers[u].GetBuffer().m_xVRAMHandle, &g_xEngine.Shadows().m_axSunViewProjMats[u], sizeof(g_xEngine.Shadows().m_axSunViewProjMats[u]));
	}

	Zenith_Profiling::EndProfile(ZENITH_PROFILE_INDEX__FLUX_SHADOWS_UPDATE_MATRICES);
}
