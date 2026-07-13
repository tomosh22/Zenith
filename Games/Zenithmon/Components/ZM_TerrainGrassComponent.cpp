#include "Zenith.h"

#include "Zenithmon/Components/ZM_TerrainGrassComponent.h"

#include "Core/Zenith_CommandLine.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "Flux/Vegetation/Flux_GrassImpl.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

ZM_TerrainGrass::ZM_TerrainGrass(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void ZM_TerrainGrass::OnAwake()
{
	ClearComponentState();
	m_bHeadless = Zenith_CommandLine::IsHeadless();

	Zenith_TerrainComponent* pxTerrain =
		m_xParentEntity.TryGetComponent<Zenith_TerrainComponent>();
	if (pxTerrain == nullptr)
	{
		WarnTerminalOnce("[Zenithmon] ZM_TerrainGrass must share its entity with Terrain");
		return;
	}

	const std::string strDensityPath = ZM_GrassDensityMap::BuildCanonicalPath(
		pxTerrain->GetTerrainAssetDirectory());
	if (!m_xDensityMap.Load(strDensityPath))
	{
		WarnTerminalOnce(
			"[Zenithmon] grass density map missing or malformed; grass disabled for this terrain instance");
		return;
	}

	// CPU ownership is useful in headless runs; Flux is deliberately untouched.
	if (m_bHeadless)
	{
		return;
	}

	// The terrain component normally has its combined physics geometry by Awake.
	// If streaming/deserialization order delays it, OnUpdate owns the bounded retry.
	TryApplyToReadyTerrain();
}

void ZM_TerrainGrass::OnUpdate(float)
{
	if (m_bHeadless || m_bGrassApplied || m_bTerminalFailure || !m_xDensityMap.IsLoaded())
	{
		return;
	}

	if (m_uRetryFrameCount >= uGRASS_RETRY_FRAME_CAP)
	{
		return;
	}

	++m_uRetryFrameCount;
	if (TryApplyToReadyTerrain())
	{
		return;
	}

	if (m_uRetryFrameCount == uGRASS_RETRY_FRAME_CAP)
	{
		WarnTerminalOnce(
			"[Zenithmon] terrain physics did not become ready within 300 frames; grass disabled for this terrain instance");
	}
}

void ZM_TerrainGrass::OnDestroy()
{
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux_GrassImpl& xGrass = g_xEngine.Grass();
		xGrass.ClearSceneData();
	}
	ClearComponentState();
}

void ZM_TerrainGrass::WriteToDataStream(Zenith_DataStream& xStream) const
{
	const u_int uComponentVersion = 1;
	xStream << uComponentVersion;
}

void ZM_TerrainGrass::ReadFromDataStream(Zenith_DataStream& xStream)
{
	u_int uComponentVersion = 0;
	xStream >> uComponentVersion;
	ClearComponentState();
}

#ifdef ZENITH_TOOLS
void ZM_TerrainGrass::RenderPropertiesPanel()
{
	ImGui::Text("CPU density: %s", m_xDensityMap.IsLoaded() ? "loaded" : "unloaded");
	ImGui::Text("Grass applied: %s", m_bGrassApplied ? "true" : "false");
	ImGui::Text("Generated blades: %u", m_uGeneratedBladeCount);
	ImGui::Text("Physics retry frames: %u", m_uRetryFrameCount);
}
#endif

bool ZM_TerrainGrass::TryApplyToReadyTerrain()
{
	Zenith_TerrainComponent* pxTerrain =
		m_xParentEntity.TryGetComponent<Zenith_TerrainComponent>();
	if (pxTerrain == nullptr)
	{
		WarnTerminalOnce("[Zenithmon] ZM_TerrainGrass lost its terrain owner");
		return false;
	}
	if (!pxTerrain->HasPhysicsGeometry())
	{
		return false;
	}

	Flux_GrassImpl& xGrass = g_xEngine.Grass();
	xGrass.ClearSceneData();
	xGrass.SetDensityScale(fGRASS_DENSITY_SCALE);
	// The render callback mirrors its debug-variable value back into Flux every
	// frame. Capture the setter result here, before that later presentation-time
	// override, so tests observe the density used for this generation.
	m_fAppliedDensityScale = xGrass.GetDensityScale();
	xGrass.SetDensityMap(m_xDensityMap.GetPixels(), ZM_GrassDensityMap::uEXPECTED_WIDTH,
		ZM_GrassDensityMap::uEXPECTED_HEIGHT, ZM_GrassDensityMap::fWORLD_SIZE);
	xGrass.GenerateFromTerrain(pxTerrain->GetPhysicsMeshGeometry());

	m_uGeneratedBladeCount = xGrass.GetGeneratedInstanceCount();
	m_bGrassApplied = true;
	return true;
}

void ZM_TerrainGrass::WarnTerminalOnce(const char* szMessage)
{
	m_bTerminalFailure = true;
	if (!m_bWarned)
	{
		Zenith_Warning(LOG_CATEGORY_TERRAIN, "%s", szMessage);
		m_bWarned = true;
	}
}

void ZM_TerrainGrass::ClearComponentState()
{
	m_xDensityMap.Clear();
	m_bGrassApplied = false;
	m_bTerminalFailure = false;
	m_bWarned = false;
	m_bHeadless = false;
	m_uRetryFrameCount = 0;
	m_uGeneratedBladeCount = 0;
	m_fAppliedDensityScale = 0.0f;
}
