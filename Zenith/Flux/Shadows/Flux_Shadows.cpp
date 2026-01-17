#include "Zenith.h"

#include "Flux/Shadows/Flux_Shadows.h"

#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Flux_Graphics.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/AnimatedMeshes/Flux_AnimatedMeshes.h"
#include "Flux/StaticMeshes/Flux_StaticMeshes.h"
#include "Flux/Terrain/Flux_Terrain.h"
#include "TaskSystem/Zenith_TaskSystem.h"

static Flux_RenderAttachment g_axCSMs[ZENITH_FLUX_NUM_CSMS];
static Flux_TargetSetup g_axCSMTargetSetups[ZENITH_FLUX_NUM_CSMS];
static Zenith_Maths::Matrix4 g_axShadowMatrices[ZENITH_FLUX_NUM_CSMS];

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_SHADOWS, Flux_Shadows::Render, nullptr);

static Flux_CommandList g_axCommandLists[ZENITH_FLUX_NUM_CSMS] = {{"Shadows"}, {"Shadows"}, {"Shadows"}, {"Shadows"}};
static Flux_DynamicConstantBuffer g_xShadowMatrixBuffers[ZENITH_FLUX_NUM_CSMS];

static Zenith_Maths::Matrix4 g_axSunViewProjMats[ZENITH_FLUX_NUM_CSMS];

DEBUGVAR bool dbg_bEnabled = true;
DEBUGVAR float dbg_fZMultiplier = 8.f;

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

	return std::move(xRet);
}

void Flux_Shadows::Initialise()
{
	Flux_RenderAttachmentBuilder xBuilder;
	xBuilder.m_uWidth = ZENITH_FLUX_CSM_RESOLUTION;
	xBuilder.m_uHeight = ZENITH_FLUX_CSM_RESOLUTION;
	xBuilder.m_eFormat = TEXTURE_FORMAT_D32_SFLOAT;
	xBuilder.m_uMemoryFlags = 1u << MEMORY_FLAGS__SHADER_READ;
	
	
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		xBuilder.BuildDepthStencil(g_axCSMs[u], "CSM " + std::to_string(u));
		g_axCSMTargetSetups[u].AssignDepthStencil(&g_axCSMs[u]);

		Flux_MemoryManager::InitialiseDynamicConstantBuffer(nullptr, sizeof(Zenith_Maths::Matrix4), g_xShadowMatrixBuffers[u]);
	}

	

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({"Render", "Enable", "Shadows"}, dbg_bEnabled);
	Zenith_DebugVariables::AddFloat({"Render", "Shadows", "Z Multiplier"}, dbg_fZMultiplier, -10.f, 10.f);
	Zenith_DebugVariables::AddTexture({"Render", "Shadows", "CSM0" }, g_axCSMs->m_pxSRV);
#endif
}

void Flux_Shadows::Reset()
{
	// Reset all cascade shadow map command lists to ensure no stale GPU resource references, including descriptor bindings
	// This is called when the scene is reset (e.g., Play/Stop transitions in editor)
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		g_axCommandLists[u].Reset(true);
	}

	Zenith_Log(LOG_CATEGORY_SHADOWS, "Flux_Shadows::Reset() - Reset %d shadow cascade command lists", ZENITH_FLUX_NUM_CSMS);
}

void Flux_Shadows::Shutdown()
{
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		// Destroy CSM render attachment
		if (g_axCSMs[u].m_xVRAMHandle.IsValid())
		{
			Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(g_axCSMs[u].m_xVRAMHandle);
			Flux_MemoryManager::QueueVRAMDeletion(pxVRAM, g_axCSMs[u].m_xVRAMHandle,
				g_axCSMs[u].m_pxRTV.m_xImageViewHandle, g_axCSMs[u].m_pxDSV.m_xImageViewHandle,
				g_axCSMs[u].m_pxSRV.m_xImageViewHandle, g_axCSMs[u].m_pxUAV.m_xImageViewHandle);
			g_axCSMs[u].m_xVRAMHandle = Flux_VRAMHandle();
		}

		// Destroy shadow matrix buffer
		Flux_MemoryManager::DestroyDynamicConstantBuffer(g_xShadowMatrixBuffers[u]);
	}
}

void Flux_Shadows::Render(void*)
{
	if (!dbg_bEnabled)
	{
		return;
	}

	UpdateShadowMatrices();
	
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		{
			g_axCommandLists[u].Reset(true);
			g_axCommandLists[u].AddCommand<Flux_CommandSetPipeline>(&Flux_StaticMeshes::GetShadowPipeline());

			// RenderToShadowMap handles all bindings via shader reflection
			Flux_StaticMeshes::RenderToShadowMap(g_axCommandLists[u], g_xShadowMatrixBuffers[u]);
		}

		{
			g_axCommandLists[u].AddCommand<Flux_CommandSetPipeline>(&Flux_AnimatedMeshes::GetShadowPipeline());

			// RenderToShadowMap handles all bindings via shader reflection
			Flux_AnimatedMeshes::RenderToShadowMap(g_axCommandLists[u], g_xShadowMatrixBuffers[u]);
		}

		if (false)
		{
			g_axCommandLists[u].AddCommand<Flux_CommandSetPipeline>(&Flux_Terrain::GetShadowPipeline());

			// RenderToShadowMap handles all bindings via shader reflection
			Flux_Terrain::RenderToShadowMap(g_axCommandLists[u], g_xShadowMatrixBuffers[u]);
		}

		Flux::SubmitCommandList(&g_axCommandLists[u], g_axCSMTargetSetups[u], RENDER_ORDER_CSM);
	}
	
	
}

void Flux_Shadows::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_Shadows::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

Flux_TargetSetup& Flux_Shadows::GetCSMTargetSetup(const uint32_t uIndex)
{
	return g_axCSMTargetSetups[uIndex];
}

Zenith_Maths::Matrix4 Flux_Shadows::GetSunViewProjMatrix(const uint32_t uIndex)
{
	return g_axSunViewProjMats[uIndex];
}

Flux_ShaderResourceView& Flux_Shadows::GetCSMSRV(const uint32_t u)
{
	return dbg_bEnabled ? g_axCSMs[u].m_pxSRV : Flux_Graphics::s_pxWhiteTexture->m_xSRV;
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
		
		float fMinX = FLT_MAX;
		float fMaxX = -FLT_MAX;
		float fMinY = FLT_MAX;
		float fMaxY = -FLT_MAX;
		float fMinZ = FLT_MAX;
		float fMaxZ = -FLT_MAX;
		
		for (uint32_t uCorner = 0; uCorner < 8; uCorner++)
		{
			Zenith_Maths::Vector3 xCornerLightSpace = xSunViewMat * Zenith_Maths::Vector4(xFrustumCorners.m_axCorners[uCorner], 1);
			fMinX = xCornerLightSpace.x < fMinX ? xCornerLightSpace.x : fMinX;
			fMaxX = xCornerLightSpace.x > fMaxX ? xCornerLightSpace.x : fMaxX;
			fMinY = xCornerLightSpace.y < fMinY ? xCornerLightSpace.y : fMinY;
			fMaxY = xCornerLightSpace.y > fMaxY ? xCornerLightSpace.y : fMaxY;
			fMinZ = xCornerLightSpace.z < fMinZ ? xCornerLightSpace.z : fMinZ;
			fMaxZ = xCornerLightSpace.z > fMaxZ ? xCornerLightSpace.z : fMaxZ;
		}
		
		const float fZRange = fMaxZ - fMinZ;
		fMinZ -= fZRange * dbg_fZMultiplier;
		
		xSunViewMat = glm::lookAt(xFrustumCenter - xSunDir * (fMaxZ + fZRange * dbg_fZMultiplier), xFrustumCenter, xUp);

		g_axSunViewProjMats[u] = glm::ortho(fMinX, fMaxX, fMinY, fMaxY, 0.0f, fMaxZ - fMinZ + fZRange * dbg_fZMultiplier) * xSunViewMat;

		Flux_MemoryManager::UploadBufferData(g_xShadowMatrixBuffers[u].GetBuffer().m_xVRAMHandle, &g_axSunViewProjMats[u], sizeof(g_axSunViewProjMats[u]));
	}

	Zenith_Profiling::EndProfile(ZENITH_PROFILE_INDEX__FLUX_SHADOWS_UPDATE_MATRICES);
}
