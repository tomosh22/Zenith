#include "Zenith.h"

#include "Flux/Shadows/Flux_Shadows.h"

#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Flux_Graphics.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/AnimatedMeshes/Flux_AnimatedMeshes.h"
#include "Flux/StaticMeshes/Flux_StaticMeshes.h"
#include "Flux/Terrain/Flux_Terrain.h"

// ---- Transient path (graph-owned allocation) ----
static Flux_TransientHandle g_axCSMHandles[ZENITH_FLUX_NUM_CSMS];
static Flux_RenderGraph* s_pxGraph = nullptr;

// ---- Fallback path (subsystem-owned allocation) ----
static Flux_RenderAttachment g_axCSMs_Owned[ZENITH_FLUX_NUM_CSMS];

// ---- Toggle: which path is active this frame ----
static bool s_bUsingTransients = false;

// CSM format (constant -- pipeline building doesn't depend on allocation path)
static constexpr TextureFormat CSM_FORMAT = TEXTURE_FORMAT_D32_SFLOAT;

static Zenith_Maths::Matrix4 g_axShadowMatrices[ZENITH_FLUX_NUM_CSMS];

static Flux_DynamicConstantBuffer g_xShadowMatrixBuffers[ZENITH_FLUX_NUM_CSMS];

static Zenith_Maths::Matrix4 g_axSunViewProjMats[ZENITH_FLUX_NUM_CSMS];

DEBUGVAR bool dbg_bEnabled = true;
DEBUGVAR float dbg_fZMultiplier = 8.f;

// ---- Helper to get the right CSM attachment regardless of path ----

static Flux_RenderAttachment& GetCSM(u_int uIndex)
{
	if (s_bUsingTransients)
		return s_pxGraph->GetTransientAttachment(g_axCSMHandles[uIndex]);
	return g_axCSMs_Owned[uIndex];
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
	// Owned targets are always created -- used when transients are toggled off.
	Flux_RenderAttachmentBuilder xBuilder;
	xBuilder.m_uWidth = ZENITH_FLUX_CSM_RESOLUTION;
	xBuilder.m_uHeight = ZENITH_FLUX_CSM_RESOLUTION;
	xBuilder.m_eFormat = CSM_FORMAT;
	xBuilder.m_uMemoryFlags = 1u << MEMORY_FLAGS__SHADER_READ;

	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		xBuilder.BuildDepthStencil(g_axCSMs_Owned[u], "CSM " + std::to_string(u) + " (owned)");

		Flux_MemoryManager::InitialiseDynamicConstantBuffer(nullptr, sizeof(Zenith_Maths::Matrix4), g_xShadowMatrixBuffers[u]);
	}

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({"Render", "Enable", "Shadows"}, dbg_bEnabled);
	Zenith_DebugVariables::AddFloat({"Render", "Shadows", "Z Multiplier"}, dbg_fZMultiplier, -10.f, 10.f);
	Zenith_DebugVariables::AddTexture({"Render", "Shadows", "CSM0" }, g_axCSMs_Owned->SRV());
#endif
}

void Flux_Shadows::Shutdown()
{
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		// Destroy owned CSM render attachment
		if (g_axCSMs_Owned[u].m_xVRAMHandle.IsValid())
		{
			Flux_VRAM* pxVRAM = Flux_PlatformAPI::GetVRAM(g_axCSMs_Owned[u].m_xVRAMHandle);
			Flux_MemoryManager::QueueVRAMDeletion(pxVRAM, g_axCSMs_Owned[u].m_xVRAMHandle,
				g_axCSMs_Owned[u].RTV().m_xImageViewHandle, g_axCSMs_Owned[u].DSV().m_xImageViewHandle,
				g_axCSMs_Owned[u].SRV().m_xImageViewHandle, g_axCSMs_Owned[u].UAV(0).m_xImageViewHandle);
		}

		// Destroy shadow matrix buffer
		Flux_MemoryManager::DestroyDynamicConstantBuffer(g_xShadowMatrixBuffers[u]);
	}

	s_pxGraph = nullptr;
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

// W23: small ints packed into user-data void* — matches the convention used in HDR.
static inline void* PackSmallInt(u_int u) { return reinterpret_cast<void*>(static_cast<uintptr_t>(u)); }
static inline u_int UnpackSmallInt(void* p) { return static_cast<u_int>(reinterpret_cast<uintptr_t>(p)); }

static void PreExecuteShadowMatrices(void*)
{
	// W3: CPU-side shadow matrix update runs once per frame on the main thread
	// before parallel cascade recording begins. Used to live inside cascade 0's
	// Execute callback, which forced the cascades to serialize.
	if (!dbg_bEnabled)
	{
		return;
	}
	Flux_Shadows::UpdateShadowMatrices();
}

static void ExecuteShadowCascade(Flux_CommandList* pxCommandList, void* pUserData)
{
	if (!dbg_bEnabled)
	{
		return;
	}

	uint32_t u = UnpackSmallInt(pUserData);

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&Flux_StaticMeshes::GetShadowPipeline());

	// RenderToShadowMap handles all bindings via shader reflection
	Flux_StaticMeshes::RenderToShadowMap(*pxCommandList, g_xShadowMatrixBuffers[u]);

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&Flux_AnimatedMeshes::GetShadowPipeline());

	// RenderToShadowMap handles all bindings via shader reflection
	Flux_AnimatedMeshes::RenderToShadowMap(*pxCommandList, g_xShadowMatrixBuffers[u]);

	// #TODO: Enable terrain shadow casting
	// Flux_Terrain::RenderToShadowMap(*pxCommandList, g_xShadowMatrixBuffers[u]);
}

void Flux_Shadows::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	s_pxGraph = &xGraph;
	s_bUsingTransients = xGraph.AreTransientsEnabled();

	// Declare transient resources if enabled
	if (s_bUsingTransients)
	{
		for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
		{
			Flux_TransientTextureDesc xCSMDesc;
			xCSMDesc.m_uWidth = ZENITH_FLUX_CSM_RESOLUTION;
			xCSMDesc.m_uHeight = ZENITH_FLUX_CSM_RESOLUTION;
			xCSMDesc.m_eFormat = CSM_FORMAT;
			xCSMDesc.m_uMemoryFlags = 1u << MEMORY_FLAGS__SHADER_READ;
			xCSMDesc.m_bIsDepthStencil = true;

			g_axCSMHandles[u] = xGraph.CreateTransient(xCSMDesc);
		}
	}

	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		const u_int uPass = xGraph.AddPass(s_aszShadowCascadePassNames[u], ExecuteShadowCascade, PackSmallInt(u));
		uint32_t uNumColour;
		Flux_RenderAttachment* pxDepthStencil;
		Flux_Shadows::GetCSMTargetSetup(u, uNumColour, pxDepthStencil);
		// Each cascade owns its own target setup and clears its depth.
		xGraph.SetClear(uPass, true);

		// W2: CSM targets are depth textures -- declare as DSV writes, not RTV.
		if (s_bUsingTransients)
			xGraph.WriteTransient(uPass, g_axCSMHandles[u], RESOURCE_ACCESS_WRITE_DSV);
		else
			xGraph.Write(uPass, g_axCSMs_Owned[u], RESOURCE_ACCESS_WRITE_DSV);

		// W3: cascade 0 owns the CPU-side matrix update via its pre-execute callback.
		// No inter-cascade GPU dependency exists, so the 4 cascades now record in parallel.
		if (u == 0)
		{
			xGraph.SetPrepare(uPass, PreExecuteShadowMatrices);
		}
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
	return g_axSunViewProjMats[uIndex];
}

Flux_ShaderResourceView& Flux_Shadows::GetCSMSRV(const uint32_t u)
{
	return dbg_bEnabled ? GetCSM(u).SRV() : Flux_Graphics::s_pxWhiteTexture->m_xSRV;
}

Flux_DynamicConstantBuffer& Flux_Shadows::GetShadowMatrixBuffer(const uint32_t u)
{
	return g_xShadowMatrixBuffers[u];
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

		g_axSunViewProjMats[u] = glm::ortho(xLightAABB.m_xMin.x, xLightAABB.m_xMax.x, xLightAABB.m_xMin.y, xLightAABB.m_xMax.y, 0.0f, fZRange * (1.0f + 2.0f * dbg_fZMultiplier)) * xSunViewMat;

		Flux_MemoryManager::UploadBufferData(g_xShadowMatrixBuffers[u].GetBuffer().m_xVRAMHandle, &g_axSunViewProjMats[u], sizeof(g_axSunViewProjMats[u]));
	}

	Zenith_Profiling::EndProfile(ZENITH_PROFILE_INDEX__FLUX_SHADOWS_UPDATE_MATRICES);
}
