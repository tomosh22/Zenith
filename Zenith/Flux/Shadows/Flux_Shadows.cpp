#include "Zenith.h"

#include "Flux/Shadows/Flux_Shadows.h"

#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/StaticMeshes/Flux_StaticMeshes.h"
#include "Flux/Terrain/Flux_Terrain.h"

static Flux_RenderAttachment g_axCSMs[ZENITH_FLUX_NUM_CSMS];
static Flux_TargetSetup g_axCSMTargetSetups[ZENITH_FLUX_NUM_CSMS];
static Zenith_Maths::Matrix4 g_axShadowMatrices[ZENITH_FLUX_NUM_CSMS];

static Flux_CommandBuffer g_xCommandBuffer;
static Flux_ConstantBuffer g_xShadowMatrixBuffers[ZENITH_FLUX_NUM_CSMS];

static Zenith_Maths::Matrix4 g_axSunViewProjMats[ZENITH_FLUX_NUM_CSMS];

#ifdef ZENITH_DEBUG_VARIABLES
DEBUGVAR Zenith_Maths::Vector3 dbg_xTestPoints[ZENITH_FLUX_NUM_CSMS];
DEBUGVAR Zenith_Maths::Vector3 dbg_xFrustumCenters[ZENITH_FLUX_NUM_CSMS];
DEBUGVAR float dbg_fZMultiplier = 10.f;
#endif

struct FrustumCorners
{
	Zenith_Maths::Vector3 GetCenter()
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
	g_xCommandBuffer.Initialise();

	Flux_RenderAttachmentBuilder xBuilder;
	xBuilder.m_uWidth = ZENITH_FLUX_CSM_RESOLUTION;
	xBuilder.m_uHeight = ZENITH_FLUX_CSM_RESOLUTION;
	xBuilder.m_eDepthStencilFormat = DEPTHSTENCIL_FORMAT_D32_SFLOAT;
	
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		xBuilder.Build(g_axCSMs[u], RENDER_TARGET_TYPE_DEPTHSTENCIL);
		g_axCSMTargetSetups[u].AssignDepthStencil(&g_axCSMs[u]);

		Flux_MemoryManager::InitialiseConstantBuffer(nullptr, sizeof(Zenith_Maths::Matrix4), g_xShadowMatrixBuffers[u]);
	}

	

#ifdef ZENITH_DEBUG_VARIABLES
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		Zenith_DebugVariables::AddVector3({ "Shadows", "Frustum Center" + std::to_string(u)}, dbg_xFrustumCenters[u], -10000, 10000);
		Zenith_DebugVariables::AddVector3({ "Shadows", "Test Point" + std::to_string(u) }, dbg_xTestPoints[u], -10000, 10000);
	}
	Zenith_DebugVariables::AddFloat({"Shadows", "Z Multiplier"}, dbg_fZMultiplier, -10.f, 10.f);
	Zenith_DebugVariables::AddTexture({ "Shadows", "CSM0" }, g_axCSMs->m_axTargetTextures[0]);
#endif
}

void Flux_Shadows::Render()
{

	UpdateShadowMatrices();

	g_xCommandBuffer.BeginRecording();
	
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		g_xCommandBuffer.SubmitTargetSetup(g_axCSMTargetSetups[u], false, true, false);
		g_xCommandBuffer.SetPipeline(&Flux_StaticMeshes::GetShadowPipeline());

		g_xCommandBuffer.BeginBind(BINDING_FREQUENCY_PER_FRAME);
		g_xCommandBuffer.BindBuffer(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer(), 0);

		g_xCommandBuffer.BeginBind(BINDING_FREQUENCY_PER_DRAW);
		g_xCommandBuffer.BindBuffer(&g_xShadowMatrixBuffers[u].GetBuffer(), 0);

		Flux_StaticMeshes::RenderToShadowMap(g_xCommandBuffer);

		if (false)
		{
			g_xCommandBuffer.SetPipeline(&Flux_Terrain::GetShadowPipeline());

			g_xCommandBuffer.BeginBind(BINDING_FREQUENCY_PER_FRAME);
			g_xCommandBuffer.BindBuffer(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer(), 0);

			g_xCommandBuffer.BeginBind(BINDING_FREQUENCY_PER_DRAW);
			g_xCommandBuffer.BindBuffer(&g_xShadowMatrixBuffers[u].GetBuffer(), 0);

			Flux_Terrain::RenderToShadowMap(g_xCommandBuffer);
		}

		g_xCommandBuffer.EndRenderPass();

	}
	
	g_xCommandBuffer.EndRecording(RENDER_ORDER_CSM, false);
}

Flux_TargetSetup& Flux_Shadows::GetCSMTargetSetup(const uint32_t uIndex)
{
	return g_axCSMTargetSetups[uIndex];
}

Zenith_Maths::Matrix4 Flux_Shadows::GetSunViewProjMatrix(const uint32_t uIndex)
{
	return g_axSunViewProjMats[uIndex];
}

Flux_Texture& Flux_Shadows::GetCSMTexture(const uint32_t u)
{
	return g_axCSMs[u].m_axTargetTextures[Flux_Swapchain::GetCurrentFrameIndex()];
}

Flux_ConstantBuffer& Flux_Shadows::GetShadowMatrixBuffer(const uint32_t u)
{
	return g_xShadowMatrixBuffers[u];
}

void Flux_Shadows::UpdateShadowMatrices()
{
	const Zenith_Maths::Matrix4& xViewMat = Flux_Graphics::GetViewMatrix();
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		const float fNearPlane = Flux_Graphics::GetFarPlane() / s_afCSMLevels[u];
		const float fFarPlane = Flux_Graphics::GetFarPlane() / s_afCSMLevels[u + 1];

		const Zenith_Maths::Matrix4 xProjMat = Zenith_Maths::PerspectiveProjection(Flux_Graphics::GetFOV(), Flux_Graphics::GetAspectRatio(), fNearPlane, fFarPlane);
		
		const Zenith_Maths::Matrix4 xInvViewProjMat = glm::inverse(xProjMat * xViewMat);

		FrustumCorners xFrustumCorners = WorldSpaceFrustumCornersFromInverseViewProjMatrix(xInvViewProjMat);
		Zenith_Maths::Vector3 xFrustumCenter = xFrustumCorners.GetCenter();

#ifdef ZENITH_DEBUG_VARIABLES
		dbg_xFrustumCenters[u] = xFrustumCenter;
		dbg_xTestPoints[u] = xInvViewProjMat * Zenith_Maths::Vector4(0, 0, 1, 1);
#endif

		Zenith_Maths::Matrix4 xSunViewMat = glm::lookAt(xFrustumCenter - Flux_Graphics::GetSunDir(), xFrustumCenter, Zenith_Maths::Vector3(0, 1, 0));

		float fMinX = FLT_MAX;
		float fMinY = FLT_MAX;
		float fMinZ = FLT_MAX;
		float fMaxX = -FLT_MAX;
		float fMaxY = -FLT_MAX;
		float fMaxZ = -FLT_MAX;
		for (uint32_t u = 0; u < 8; u++)
		{
			Zenith_Maths::Vector3 xTransformedCorner = xSunViewMat * Zenith_Maths::Vector4(xFrustumCorners.m_axCorners[u], 1);
			fMinX = xTransformedCorner.x < fMinX ? xTransformedCorner.x : fMinX;
			fMinY = xTransformedCorner.y < fMinY ? xTransformedCorner.y : fMinY;
			fMinZ = xTransformedCorner.z < fMinZ ? xTransformedCorner.z : fMinZ;

			fMaxX = xTransformedCorner.x > fMaxX ? xTransformedCorner.x : fMaxX;
			fMaxY = xTransformedCorner.y > fMaxY ? xTransformedCorner.y : fMaxY;
			fMaxZ = xTransformedCorner.z > fMaxZ ? xTransformedCorner.z : fMaxZ;
		}

		const float fMultiplierZ = dbg_fZMultiplier;
		fMinZ = fMinZ < 0 ? fMinZ * fMultiplierZ : fMinZ / fMultiplierZ;
		fMaxZ = fMaxZ > 0 ? fMaxZ * fMultiplierZ : fMaxZ / fMultiplierZ;

		g_axSunViewProjMats[u] = glm::ortho(fMinX, fMaxX, fMinY, fMaxY, fMinZ, fMaxZ) * xSunViewMat;

		Flux_MemoryManager::UploadBufferData(g_xShadowMatrixBuffers[u].GetBuffer(), &g_axSunViewProjMats[u], sizeof(g_axSunViewProjMats[u]));
	}

	
}
