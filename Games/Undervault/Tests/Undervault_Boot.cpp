#include "Zenith.h"
#ifdef ZENITH_INPUT_SIMULATOR
#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Flux/Flux_ModelInstance.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "Flux/Particles/Flux_ParticleGPUImpl.h"
#include "Core/Zenith_GraphicsOptions.h"
extern const char* Project_GetMockupName();
namespace
{
	bool g_bCutawayLoaded = false;
	void Setup() { g_bCutawayLoaded = false; }
	bool Step(int iFrame)
	{
		g_xEngine.Scenes().QueryAllScenes<Zenith_ModelComponent>().ForEach(
			[](Zenith_EntityID, Zenith_ModelComponent& xModel)
			{
				if (xModel.HasModel() && xModel.GetModelPath().find(std::string(Project_GetMockupName())+".zmodel") != std::string::npos)
				{
					Flux_ModelInstance* pxInstance = xModel.GetModelInstance();
					g_bCutawayLoaded = pxInstance->GetNumMeshes() == 1 && !pxInstance->HasSkeleton()
						&& pxInstance->GetNumDrawSections(0) >= 12;
					for (uint32_t u = 0; u < pxInstance->GetNumDrawSections(0); ++u)
					{
						Flux_MeshDrawSection xSection;
						g_bCutawayLoaded = g_bCutawayLoaded && pxInstance->GetDrawSection(0, u, xSection)
							&& xSection.m_uIndexCount > 0 && pxInstance->GetMeshMaterial(0, xSection.m_uMaterialSlot) != nullptr;
					}
				}
			});
		return iFrame < 300;
	}
	bool Verify()
	{
		uint32_t uCount=0;bool bGPUOnly=true;
		g_xEngine.Scenes().QueryAllScenes<Zenith_ParticleEmitterComponent>().ForEach(
			[&](Zenith_EntityID,Zenith_ParticleEmitterComponent& xEmitter) {
				++uCount;bGPUOnly=bGPUOnly && xEmitter.UsesGPUCompute() && xEmitter.IsEmitting()
					&& xEmitter.GetParticles().GetSize()==0 && xEmitter.GetAliveCount()==0;
			});
		const uint32_t uExpected=std::string(Project_GetMockupName())=="Flooding" ? 2u : 1u;
		const uint32_t uInstances=g_xEngine.ParticleGPU().ReadbackPartitionInstanceCount(0);
		Zenith_Log(LOG_CATEGORY_PARTICLES,"Undervault GPU ambience: emitters=%u expected=%u GPU-only=%d instances=%u",uCount,uExpected,bGPUOnly,uInstances);
		return g_bCutawayLoaded && bGPUOnly && uCount==uExpected && uInstances>0
			&& !Zenith_GraphicsOptions::Get().m_bCPUParticlesEnabled;
	}
	const Zenith_AutomatedTest g_xBootTest = { "Undervault_MockupBoot_Test", &Setup, &Step, &Verify, 600, true };
	ZENITH_AUTOMATED_TEST_REGISTER(g_xBootTest);
}
#endif
