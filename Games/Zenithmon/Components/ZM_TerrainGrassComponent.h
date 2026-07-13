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

	bool HasCPUMap() const { return m_xDensityMap.IsLoaded(); }
	bool IsGrassApplied() const { return m_bGrassApplied; }
	bool HasTerminalFailure() const { return m_bTerminalFailure; }
	u_int GetGeneratedBladeCount() const { return m_uGeneratedBladeCount; }
	float GetAppliedDensityScale() const { return m_fAppliedDensityScale; }
	uint32_t GetRetryFrameCount() const { return m_uRetryFrameCount; }
	const ZM_GrassDensityMap& GetDensityMap() const { return m_xDensityMap; }

private:
	static constexpr uint32_t uGRASS_RETRY_FRAME_CAP = 300;
	static constexpr float fGRASS_DENSITY_SCALE = 0.15f;

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
