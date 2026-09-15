#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"
#include "Flux/Particles/Flux_ParticlesImpl.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include <cstdlib>
#include <cstring>
#ifdef ZENITH_TOOLS
#include "Editor/Zenith_EditorAutomation.h"
#include "UI/Zenith_UI.h"
#endif

const char* Project_GetName() { return "Foundry"; }
// Process-start selection only; each view remains a static scene with no controls.
const char* Project_GetMockupName()
{
	static const std::string strName = []() {
		char szValue[32] = {}; size_t uLength = 0;
		getenv_s(&uLength, szValue, sizeof(szValue), "FOUNDRY_MOCKUP");
		if (strcmp(szValue, "Logistics") == 0 || strcmp(szValue, "MapDefense") == 0)
			return std::string(szValue);
		return std::string("Workshop");
	}();
	return strName.c_str();
}
const char* Project_GetGameAssetsDirectory() { return GAME_ASSETS_DIR; }
namespace
{
    Flux_ParticleEmitterConfig g_xSmoke;
    Flux_ParticleEmitterConfig g_xProcessSteam;
    void BindSmokeTexture()
    {
        // Flux currently shares a billboard texture across its blend partitions.
        // Set it for this game only; do not replace the engine's default asset.
        auto* texture = Zenith_AssetRegistry::GetView<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Particles/Smoke.ztxtr");
        Zenith_Assert(texture, "Foundry smoke texture must be baked before loading the scene");
        g_xEngine.Particles().m_xParticleTexture.Set(texture);
    }
}
void Project_RegisterGameComponents()
{
    auto setup = [](Flux_ParticleEmitterConfig& c, float size, float opacity) {
        c.m_bUseGPUCompute = true;
        c.m_bAdditiveBlending = false;
        c.m_uMaxParticles = 128;
        c.m_fSpawnRate = 12.f;
        c.m_fLifetimeMin = 2.0f; c.m_fLifetimeMax = 2.8f;
        c.m_fSpawnRadius = .055f;
        c.m_xEmitDirection = { .08f, 1.f, .04f };
        c.m_fSpreadAngleDegrees = 12.f;
        c.m_fSpeedMin = .38f; c.m_fSpeedMax = .60f;
        c.m_xGravity = { .06f, .08f, .015f };
        c.m_fDrag = .10f; c.m_fTurbulence = .06f;
        c.m_xColorStart = { .63f, .65f, .62f, opacity };
        c.m_xColorEnd = { .76f, .77f, .74f, 0.f };
        c.m_fSizeStart = size; c.m_fSizeEnd = size * 2.7f;
        c.m_strTexturePath = GAME_ASSETS_DIR "Textures/Particles/Smoke.ztxtr";
    };
    setup(g_xSmoke, .26f, .30f);
    setup(g_xProcessSteam, .20f, .23f);
    Flux_ParticleEmitterConfig::Register("Foundry_GPU_Smoke", &g_xSmoke);
    Flux_ParticleEmitterConfig::Register("Foundry_GPU_ProcessSteam", &g_xProcessSteam);
}
void Project_Shutdown()
{
    Flux_ParticleEmitterConfig::Unregister("Foundry_GPU_Smoke");
    Flux_ParticleEmitterConfig::Unregister("Foundry_GPU_ProcessSteam");
}
void Project_SetGraphicsOptions(Zenith_GraphicsOptions& xOptions)
{
	xOptions.m_uWindowWidth = 1920;
	xOptions.m_uWindowHeight = 1080;
	xOptions.m_bFogEnabled = false;
	xOptions.m_bSkyboxEnabled = false;
	xOptions.m_bGrassWindEnabled = false;
	xOptions.m_bGizmosEnabled = false;
	xOptions.m_bCPUParticlesEnabled = false;
	xOptions.m_bGPUParticlesEnabled = true;
}
void Project_LoadInitialScene()
{
    BindSmokeTexture();
	const std::string strScene = std::string(GAME_ASSETS_DIR "Scenes/") +
		(strcmp(Project_GetMockupName(), "Workshop") == 0 ? "Main" : Project_GetMockupName()) + ZENITH_SCENE_EXT;
	g_xEngine.Scenes().RegisterSceneBuildIndex(0, strScene.c_str());
	g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}
#ifdef ZENITH_TOOLS
void Project_InitializeResources() { BindSmokeTexture(); }
void Project_RegisterEditorAutomationSteps()
{
    Zenith_EditorAutomation& a = g_xEngine.EditorAutomation();
    const char* view = Project_GetMockupName();
    const bool workshop = strcmp(view, "Workshop") == 0;
    const bool defense = strcmp(view, "MapDefense") == 0;
    a.AddStep_CreateScene(workshop ? "Main" : view);
    a.AddStep_CreateEntity("PresentationCamera");
    a.AddStep_AddCamera();
    a.AddStep_SetCameraPosition(workshop ? 0.f : 2.f, defense ? 78.f : (workshop ? 37.f : 47.f), defense ? -33.f : (workshop ? -22.f : -28.f));
    a.AddStep_SetCameraPitch(defense ? -1.20f : -1.05f);
    a.AddStep_SetCameraFOV(glm::radians(30.f));
    a.AddStep_SetCameraAspect(16.f / 9.f);
    a.AddStep_SetCameraFar(200.f);
    a.AddStep_SetAsMainCamera();
    a.AddStep_CreateEntity("FoundryDiorama");
    a.AddStep_AddModel();
    a.AddStep_LoadModel((std::string(GAME_ASSETS_DIR "Workshop/") + view + ".zmodel").c_str());
    a.AddStep_CreateEntity("FoundrySun");
    a.AddStep_AddComponent("Sun");
    a.AddStep_SetSunDirection(.4f, -.8f, .3f);
    auto exhaust = [&a](const char* name, float x, float height, float z, bool steam = false) {
        a.AddStep_CreateEntity(name);
        a.AddStep_SetTransformPosition(x, height, z);
        a.AddStep_AddParticleEmitter();
        a.AddStep_SetParticleConfigByName(steam ? "Foundry_GPU_ProcessSteam" : "Foundry_GPU_Smoke");
        a.AddStep_SetParticleEmitting(true);
    };
    // Blender (x,y,z) -> engine (x,z,-y). Stack opening local height is 2.82 m.
    if (workshop)
    {
        for (int i=0; i<3; ++i)
            exhaust(("SmelterExhaust" + std::to_string(i)).c_str(), -1.9f+i*3.1f, 3.06f, 1.432f);
    }
    else if (!defense)
    {
        exhaust("CentralSmelterExhaust", 0.f, 2.05f, .688f);
        exhaust("ProcessVentLeft", -3.f, 2.48f, 6.f, true);
        exhaust("ProcessVentCentre", -1.f, 3.29f, 6.f, true);
        exhaust("ProcessVentRight", 1.f, 2.48f, 6.f, true);
    }
    else
    {
        for (int row=0; row<3; ++row)
            for (int col=0; col<3; ++col)
                exhaust(("FactorySmelterExhaust" + std::to_string(row*3+col)).c_str(),
                    3.f+col*3.f, 2.11f, .296f-row*3.f);
    }
    for (int i = 0; i < (defense ? 0 : (workshop ? 3 : 1)); ++i)
    {
        const std::string name = "FurnaceGlow" + std::to_string(i);
        a.AddStep_CreateEntity(name.c_str());
        a.AddStep_SetTransformPosition(workshop ? -1.9f + i * 3.1f : 0.f, .8f, workshop ? -.2f : -.3f);
        a.AddStep_AddComponent("Light");
        a.AddStep_SetLightColor(1.f, .29f, .04f);
        a.AddStep_SetLightIntensity(115.f);
        a.AddStep_SetLightRange(2.5f);
    }
    // Original transparent artwork contains only the decorative HUD, never the scene.
    // Rasterized Roboto avoids changing the engine-wide font for this static study.
    a.AddStep_CreateEntity("FoundryHUD");
    a.AddStep_AddUI();
    a.AddStep_CreateUIImage("ReferenceHUD");
    a.AddStep_SetUIImageTexturePath("ReferenceHUD", (std::string(GAME_ASSETS_DIR "Textures/UI/HUD") + view + ".ztxtr").c_str());
    a.AddStep_SetUIAnchor("ReferenceHUD", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
    a.AddStep_SetUIPosition("ReferenceHUD", 0.f, 0.f);
    a.AddStep_SetUISize("ReferenceHUD", 1920.f, 1080.f);
    a.AddStep_SaveScene((std::string(GAME_ASSETS_DIR "Scenes/") + (workshop ? "Main" : view) + ZENITH_SCENE_EXT).c_str());
    a.AddStep_UnloadScene();
    a.AddStep_LoadInitialScene(&Project_LoadInitialScene);
}
#endif
