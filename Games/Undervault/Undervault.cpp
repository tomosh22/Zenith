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

const char* Project_GetName() { return "Undervault"; }
const char* Project_GetGameAssetsDirectory() { return GAME_ASSETS_DIR; }
const char* Project_GetMockupName()
{
	static const std::string strView = []() {
		char szValue[32] = {}; size_t uLength = 0;
		getenv_s(&uLength, szValue, sizeof(szValue), "UNDERVAULT_MOCKUP");
		return std::string(strcmp(szValue, "Planning") == 0 || strcmp(szValue, "Flooding") == 0 ? szValue : "Colony");
	}();
	return strView.c_str();
}
namespace
{
	Flux_ParticleEmitterConfig g_xMist;
	Flux_ParticleEmitterConfig g_xSplash;
	void BindMistTexture()
	{
		auto* pxTexture=Zenith_AssetRegistry::GetView<Zenith_TextureAsset>(GAME_ASSETS_DIR "Textures/Particles/Mist.ztxtr");
		Zenith_Assert(pxTexture,"Undervault mist texture must be baked before loading the scene");
		g_xEngine.Particles().m_xParticleTexture.Set(pxTexture);
	}
}
void Project_RegisterGameComponents()
{
	g_xMist.m_bUseGPUCompute=true;g_xMist.m_bAdditiveBlending=false;
	g_xMist.m_uMaxParticles=160;g_xMist.m_fSpawnRate=20.f;
	g_xMist.m_fLifetimeMin=2.5f;g_xMist.m_fLifetimeMax=4.f;
	g_xMist.m_fSpawnRadius=1.05f;g_xMist.m_xEmitDirection={.1f,.1f,0.f};
	g_xMist.m_fSpreadAngleDegrees=75.f;g_xMist.m_fSpeedMin=.025f;g_xMist.m_fSpeedMax=.06f;
	g_xMist.m_xGravity={0.f,0.f,0.f};g_xMist.m_fDrag=.2f;g_xMist.m_fTurbulence=.025f;
	g_xMist.m_xColorStart={.31f,.58f,.18f,.04f};g_xMist.m_xColorEnd={.31f,.52f,.18f,0.f};
	g_xMist.m_fSizeStart=.65f;g_xMist.m_fSizeEnd=1.25f;
	g_xSplash.m_bUseGPUCompute=true;g_xSplash.m_bAdditiveBlending=false;
	g_xSplash.m_uMaxParticles=96;g_xSplash.m_fSpawnRate=36.f;
	g_xSplash.m_fLifetimeMin=.35f;g_xSplash.m_fLifetimeMax=.65f;
	g_xSplash.m_fSpawnRadius=.18f;g_xSplash.m_xEmitDirection={0.f,1.f,0.f};
	g_xSplash.m_fSpreadAngleDegrees=55.f;g_xSplash.m_fSpeedMin=.25f;g_xSplash.m_fSpeedMax=.7f;
	g_xSplash.m_xGravity={0.f,-1.4f,0.f};g_xSplash.m_fDrag=.05f;g_xSplash.m_fTurbulence=.04f;
	g_xSplash.m_xColorStart={.42f,.82f,.91f,.56f};g_xSplash.m_xColorEnd={.35f,.75f,.86f,0.f};
	g_xSplash.m_fSizeStart=.06f;g_xSplash.m_fSizeEnd=.19f;
	Flux_ParticleEmitterConfig::Register("Undervault_GPU_GasMist",&g_xMist);
	Flux_ParticleEmitterConfig::Register("Undervault_GPU_WaterSplash",&g_xSplash);
}
void Project_Shutdown()
{
	Flux_ParticleEmitterConfig::Unregister("Undervault_GPU_GasMist");
	Flux_ParticleEmitterConfig::Unregister("Undervault_GPU_WaterSplash");
}
void Project_SetGraphicsOptions(Zenith_GraphicsOptions& xOptions)
{
	xOptions.m_uWindowWidth = 1920; xOptions.m_uWindowHeight = 1080;
	xOptions.m_bFogEnabled = false; xOptions.m_bSkyboxEnabled = false;
	xOptions.m_bGrassWindEnabled = false; xOptions.m_bGizmosEnabled = false;
	xOptions.m_bCPUParticlesEnabled = false; xOptions.m_bGPUParticlesEnabled = true;
	xOptions.m_bShadowsEnabled = false;
	xOptions.m_bHDRAutoExposureEnabled = false;
	xOptions.m_bSkyboxAtmosphereEnabled = false;
	xOptions.m_xSkyboxColour = {.055f, .065f, .08f};
}
void Project_LoadInitialScene()
{
	BindMistTexture();
	const std::string strScene = std::string(GAME_ASSETS_DIR "Scenes/") + Project_GetMockupName() + ZENITH_SCENE_EXT;
	g_xEngine.Scenes().RegisterSceneBuildIndex(0, strScene.c_str());
	g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}
#ifdef ZENITH_TOOLS
void Project_InitializeResources() { BindMistTexture(); }
void Project_RegisterEditorAutomationSteps()
{
	Zenith_EditorAutomation& a = g_xEngine.EditorAutomation();
	const std::string view = Project_GetMockupName();
	a.AddStep_CreateScene(view.c_str());
	a.AddStep_CreateEntity("FixedCutawayCamera"); a.AddStep_AddCamera();
	// The public camera API exposes perspective only. A distant 8-degree lens
	// approximates the reference's orthographic framing without engine changes.
	a.AddStep_SetCameraPosition(0.f, 6.1f, -100.f);
	a.AddStep_SetCameraPitch(0.f); a.AddStep_SetCameraYaw(0.f);
	a.AddStep_SetCameraFOV(glm::radians(8.f)); a.AddStep_SetCameraAspect(16.f/9.f);
	a.AddStep_SetCameraFar(180.f); a.AddStep_SetAsMainCamera();
	a.AddStep_CreateEntity("UndervaultCutaway"); a.AddStep_AddModel();
	a.AddStep_LoadModel((std::string(GAME_ASSETS_DIR "Dioramas/") + view + ".zmodel").c_str());
	a.AddStep_CreateEntity("SubterraneanEnvironment"); a.AddStep_AddComponent("Sun");
	a.AddStep_SetSunTimeOfDay(270.f,0.f);
	int index = 0;
	auto light = [&](float x, float h, float r=1.f, float g=.73f, float b=.40f, float power=190.f, float range=4.5f) {
		a.AddStep_CreateEntity(("VaultLamp" + std::to_string(index++)).c_str());
		a.AddStep_SetTransformPosition(x,h,-1.7f); a.AddStep_AddComponent("Light");
		a.AddStep_SetLightColor(r,g,b); a.AddStep_SetLightIntensity(power); a.AddStep_SetLightRange(range);
	};
	// Broad, dim cool fill keeps unlit geology readable without daylight.
	light(0.f,6.f,.40f,.53f,.70f,1100.f,40.f);
	a.AddStep_SetTransformPosition(0.f,6.f,-8.f);
	if (view == "Colony")
	{
		for (auto p : {glm::vec2(-9.7f,10.1f),{-6.8f,9.25f},{-.8f,9.55f},{-6.5f,5.35f},{-2.4f,4.65f},{6.1f,7.7f},{8.2f,6.1f},{11.4f,7.8f}}) light(p.x,p.y);
		light(-9.f,3.3f,1.f,.64f,.28f,95.f,3.5f);
		light(2.2f,7.13f,.05f,.9f,.83f,130.f,4.5f); light(4.65f,3.1f,.02f,.51f,.75f,45.f,4.3f);
	}
	else if (view == "Planning")
	{
		for (auto p : {glm::vec2(-6.8f,10.15f),{-3.7f,10.15f},{1.5f,10.15f},{6.7f,10.15f},{-7.2f,6.8f},{2.1f,6.8f},{6.6f,6.8f},{.8f,3.7f}}) light(p.x,p.y);
		light(-10.f,6.f,.08f,.48f,1.f,150.f,8.f);light(11.f,6.f,1.f,.12f,.01f,180.f,8.f);
		light(-6.1f,6.2f,.02f,.51f,.75f,45.f,2.8f);
	}
	else
	{
		for (auto p : {glm::vec2(-7.9f,10.3f),{-4.3f,10.45f},{-.4f,10.3f},{3.f,11.f},{6.5f,10.45f},{10.8f,10.45f},{6.5f,6.25f},{10.6f,6.15f}}) light(p.x,p.y);
		light(-6.8f,5.3f,.18f,.56f,.80f,140.f,6.f);
		light(3.f,5.4f,.6f,.72f,.9f,70.f,4.f);
		light(-5.2f,4.05f,.02f,.51f,.75f,90.f,12.1f);light(7.1f,4.05f,.02f,.51f,.75f,45.f,8.2f);
	}
	auto emitter=[&](const char* name,float x,float h,const char* config) {
		a.AddStep_CreateEntity(name);a.AddStep_SetTransformPosition(x,h,-1.25f);
		a.AddStep_AddParticleEmitter();a.AddStep_SetParticleConfigByName(config);a.AddStep_SetParticleEmitting(true);
	};
	if(view=="Planning") emitter("CO2Pocket",3.6f,2.2f,"Undervault_GPU_GasMist");
	else if(view=="Colony") emitter("CisternSplash",4.5f,3.15f,"Undervault_GPU_WaterSplash");
	else
	{
		emitter("BreachSplash",-6.15f,4.1f,"Undervault_GPU_WaterSplash");
		emitter("BreachSpray",-6.55f,4.65f,"Undervault_GPU_WaterSplash");
	}
	a.AddStep_CreateEntity("UndervaultHUD"); a.AddStep_AddUI();
	a.AddStep_CreateUIImage("DecorativeHUD");
	a.AddStep_SetUIImageTexturePath("DecorativeHUD",(std::string(GAME_ASSETS_DIR "Textures/UI/HUD")+view+".ztxtr").c_str());
	a.AddStep_SetUIAnchor("DecorativeHUD",static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	a.AddStep_SetUIPosition("DecorativeHUD",0.f,0.f);a.AddStep_SetUISize("DecorativeHUD",1920.f,1080.f);
	a.AddStep_SaveScene((std::string(GAME_ASSETS_DIR "Scenes/")+view+ZENITH_SCENE_EXT).c_str());
	a.AddStep_UnloadScene();a.AddStep_LoadInitialScene(&Project_LoadInitialScene);
}
#endif
