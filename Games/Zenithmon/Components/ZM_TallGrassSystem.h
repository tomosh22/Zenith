#pragma once

#include "Zenithmon/Source/World/ZM_GrassDensityMap.h"
#include "Zenithmon/Source/Data/ZM_BattleRNG.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"      // ZM_SCENE_ID (not reachable via the two above)
#include "ZenithECS/Zenith_Entity.h"

class Zenith_DataStream;

// A quantized 1 m world tile (floor of world X/Z). Self-contained so the API does
// not depend on an engine math vector type.
struct ZM_GrassTile
{
	int m_iX = 0;
	int m_iZ = 0;
};

// Gameplay-side tall-grass owner (S5). Keeps its OWN CPU density copy (independent
// of ZM_TerrainGrass's render feed), quantizes the active player to 1 m tiles, and
// on a tile transition onto grass rolls a per-route encounter (ZM_EncounterZone)
// and dispatches ZM_OnWildEncounter. Shares its entity with the terrain component.
// EMITS the event only -- it does NOT load the battle (item 3 subscribes).
class ZM_TallGrassSystem
{
public:
	static constexpr u_int  uSERIALIZATION_VERSION   = 1u;
	static constexpr float  fGRASS_DENSITY_THRESHOLD = 0.5f;

	ZM_TallGrassSystem() = delete;
	explicit ZM_TallGrassSystem(Zenith_Entity& xParentEntity);
	ZM_TallGrassSystem(const ZM_TallGrassSystem&) = delete;
	ZM_TallGrassSystem& operator=(const ZM_TallGrassSystem&) = delete;
	ZM_TallGrassSystem(ZM_TallGrassSystem&&) noexcept = default;
	ZM_TallGrassSystem& operator=(ZM_TallGrassSystem&&) noexcept = default;

	void OnAwake();
	void OnUpdate(float fDeltaTime);
	void OnDestroy();

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

	bool HasDensityMap() const;

	// --- pure, headless-testable static helpers (no entity/scene/Flux state) ---
	static ZM_GrassTile QuantizeToTile(float fWorldX, float fWorldZ);            // floorf on each axis
	static bool IsTileTransition(ZM_GrassTile xLast, bool bHasLast, ZM_GrassTile xCurrent);  // true iff bHasLast && tiles differ
	static bool IsGrassDensity(float fDensity);                                  // fDensity >= fGRASS_DENSITY_THRESHOLD

#ifdef ZENITH_INPUT_SIMULATOR
	void ForceEncounterOnNextTransitionForTests();   // next on-grass transition forces an encounter
	void ForceEncounterOnNextTransitionForTests(ZM_SPECIES_ID eSpecies, u_int uLevel); // NEW: explicit species/level
	void SetRngSeedForTests(u_int64 ulSeed);         // m_xRng = ZM_BattleRNG(ulSeed)
#endif

private:
	// Fixed salt XOR'd with the resolved scene id to seed m_xRng in OnAwake. No wall
	// clock / global RNG is ever consulted, so every boot rolls the same stream.
	static constexpr u_int64 ulRNG_SEED_SALT = 0x5A4D477241535321ull;   // "ZMGrASS!"

	bool TrySampleActivePlayerXZ(float& fXOut, float& fZOut) const;  // via ZM_GameStateManager seam
	ZM_SCENE_ID ResolveActiveSceneId() const;                        // active build index -> ZM_SCENE_ID

	Zenith_Entity      m_xParentEntity;
	ZM_GrassDensityMap m_xDensityMap;
	ZM_BattleRNG       m_xRng;
	ZM_GrassTile       m_xLastTile;
	bool               m_bHasLastTile = false;
	bool               m_bForceEncounter = false;
	bool          m_bForceExplicit = false;
	ZM_SPECIES_ID m_eForcedSpecies = ZM_SPECIES_NONE;
	u_int         m_uForcedLevel   = 0u;
};
