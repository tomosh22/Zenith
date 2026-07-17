#pragma once

#include "Zenithmon/Source/World/ZM_GrassDensityMap.h"

#include "DataStream/Zenith_DataStream.h"
#include "ZenithECS/Zenith_Entity.h"

// Terrain-local owner of Zenithmon's rendered Dawnmere grass. This is only the
// rendering bridge; S5 owns encounter sampling and all tall-grass gameplay.
class ZM_TerrainGrass
{
public:
	ZM_TerrainGrass() = delete;
	explicit ZM_TerrainGrass(Zenith_Entity& xParentEntity);

	ZM_TerrainGrass(const ZM_TerrainGrass&) = delete;
	ZM_TerrainGrass& operator=(const ZM_TerrainGrass&) = delete;
	ZM_TerrainGrass(ZM_TerrainGrass&&) noexcept = default;
	ZM_TerrainGrass& operator=(ZM_TerrainGrass&&) noexcept = default;

	void OnAwake();
	void OnUpdate(float fDeltaTime);
	void OnDestroy();

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

	// Re-uploads this instance's CPU density map and regenerates its blades against
	// the sibling terrain. The S5 additive-battle round trip CLEARS the engine-owned
	// grass singleton on battle entry (grass cleared entering interiors/battle) and
	// must restore this scene's grass on resume; the engine E5 render-reset hook only
	// fires on SINGLE loads, and this component's OnUpdate cannot self-heal while its
	// scene is paused. Returns true when grass was regenerated. Headless, terminal-
	// failure, and map-less instances return false.
	bool RegenerateForSceneResume();

	bool HasCPUMap() const { return m_xDensityMap.IsLoaded(); }
	bool IsGrassApplied() const { return m_bGrassApplied; }
	bool HasTerminalFailure() const { return m_bTerminalFailure; }
	u_int GetGeneratedBladeCount() const { return m_uGeneratedBladeCount; }
	float GetAppliedDensityScale() const { return m_fAppliedDensityScale; }
	uint32_t GetRetryFrameCount() const { return m_uRetryFrameCount; }
	const ZM_GrassDensityMap& GetDensityMap() const { return m_xDensityMap; }

private:
	static constexpr uint32_t uGRASS_RETRY_FRAME_CAP = 300;
	// Blades/m² multiplier at generation (× GrassConfig::uBLADES_PER_SQM = 50).
	// 0.70 → ~35 blades/m² in fully-painted lawn; with the per-region chunk LOD in
	// Flux_Grass, grass near the camera now renders at LOD0 (full density) rather
	// than the whole terrain collapsing to one distant LOD.
	static constexpr float fGRASS_DENSITY_SCALE = 0.70f;

	bool TryApplyToReadyTerrain();
	void WarnTerminalOnce(const char* szMessage);
	void ClearComponentState();

	Zenith_Entity m_xParentEntity;
	ZM_GrassDensityMap m_xDensityMap;
	bool m_bGrassApplied = false;
	bool m_bTerminalFailure = false;
	bool m_bWarned = false;
	bool m_bHeadless = false;
	uint32_t m_uRetryFrameCount = 0;
	u_int m_uGeneratedBladeCount = 0;
	float m_fAppliedDensityScale = 0.0f;
};
