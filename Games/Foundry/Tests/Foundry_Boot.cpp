#include "Zenith.h"
#ifdef ZENITH_INPUT_SIMULATOR
#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Flux/Flux_ModelInstance.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "Flux/Particles/Flux_ParticleGPUImpl.h"
#include "Flux/Particles/Flux_ParticlesImpl.h"
#include "Core/Zenith_GraphicsOptions.h"
extern const char* Project_GetMockupName();
namespace
{
	bool g_bWorkshopLoaded = false;
	void Setup() { g_bWorkshopLoaded = false; }
	bool Step(int iFrame)
	{
		g_xEngine.Scenes().QueryAllScenes<Zenith_ModelComponent>().ForEach(
			[](Zenith_EntityID, Zenith_ModelComponent& xModel)
			{
				if (xModel.HasModel() && xModel.GetModelPath().find(std::string(Project_GetMockupName())+".zmodel") != std::string::npos)
				{
					Flux_ModelInstance* pxInstance = xModel.GetModelInstance();
					g_bWorkshopLoaded = pxInstance->GetNumMeshes() == 1 && !pxInstance->HasSkeleton()
						&& pxInstance->GetNumDrawSections(0) >= 12;
					for (uint32_t u = 0; u < pxInstance->GetNumDrawSections(0); ++u)
					{
						Flux_MeshDrawSection xSection;
						g_bWorkshopLoaded = g_bWorkshopLoaded && pxInstance->GetDrawSection(0, u, xSection)
							&& xSection.m_uIndexCount > 0 && pxInstance->GetMeshMaterial(0, xSection.m_uMaterialSlot) != nullptr;
					}
				}
			});
		return iFrame < 120 || (!g_bWorkshopLoaded && iFrame < 300);
	}
	bool Verify() { return g_bWorkshopLoaded; }
	const Zenith_AutomatedTest g_xBootTest = { "Foundry_WorkshopBoot_Test", &Setup, &Step, &Verify, 600 };
	ZENITH_AUTOMATED_TEST_REGISTER(g_xBootTest);
	bool SmokeStep(int iFrame) { Step(iFrame); return iFrame < 300; }
	bool VerifyGPUSmoke()
	{
		uint32_t count=0; bool gpuOnly=true;
		g_xEngine.Scenes().QueryAllScenes<Zenith_ParticleEmitterComponent>().ForEach(
			[&](Zenith_EntityID, Zenith_ParticleEmitterComponent& e) {
				++count;
				gpuOnly = gpuOnly && e.UsesGPUCompute() && e.IsEmitting()
					&& e.GetParticles().GetSize()==0 && e.GetAliveCount()==0;
			});
		const std::string view=Project_GetMockupName();
		const uint32_t expected=view=="Workshop" ? 3u : (view=="Logistics" ? 4u : 9u);
		// Read back once at verification, never in the render/update path. This
		// proves compute wrote actual draw instances, not just registered emitters.
		const uint32_t instances=g_xEngine.ParticleGPU().ReadbackPartitionInstanceCount(0);
		Zenith_Log(LOG_CATEGORY_PARTICLES,"Foundry GPU smoke: emitters=%u expected=%u GPU-only=%d indirect instances=%u",count,expected,gpuOnly,instances);
		return g_bWorkshopLoaded && count==expected && gpuOnly && instances>0
			&& g_xEngine.ParticleGPU().IsActiveThisFrame()
			&& !Zenith_GraphicsOptions::Get().m_bCPUParticlesEnabled
			&& g_xEngine.Particles().m_uAlphaInstanceCount==0
			&& g_xEngine.Particles().m_uAdditiveInstanceCount==0;
	}
	const Zenith_AutomatedTest g_xSmokeTest = { "Foundry_GPUSmoke_Test", &Setup, &SmokeStep, &VerifyGPUSmoke, 600, true };
	ZENITH_AUTOMATED_TEST_REGISTER(g_xSmokeTest);
}
#endif
