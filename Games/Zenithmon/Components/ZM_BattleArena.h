#pragma once

#include "ZenithECS/Zenith_Entity.h"
#include "AssetHandling/Zenith_AssetHandle.h"      // MeshGeometryHandle, MaterialHandle
#include "Zenithmon/Source/Data/ZM_PropData.h"     // ZM_PROP_ID, ZM_PROP_BIOME

class Zenith_DataStream;

// Battle-arena biome selector. One entry per ZM_PROP_DRESSING_* set, in the same
// order as ZM_PROP_BIOME's six real biomes (MEADOW..CANYON). Component-local so the
// arena API does not leak the prop-roster ordering. APPEND-only if ever extended.
enum ZM_BATTLE_BIOME : u_int
{
	ZM_BATTLE_BIOME_MEADOW,
	ZM_BATTLE_BIOME_VOLCANIC,
	ZM_BATTLE_BIOME_COAST,
	ZM_BATTLE_BIOME_WETLAND,
	ZM_BATTLE_BIOME_SNOW,
	ZM_BATTLE_BIOME_CANYON,

	ZM_BATTLE_BIOME_COUNT
};

// Manages the one battle arena (build index 1): an always-visible enclosing dome +
// two battle platforms, plus six per-biome dressing sets of which exactly one is
// shown at a time. Spawns all of it in OnStart (the ZM_GreyboxVisual idiom); SetBiome
// swaps which dressing set is enabled. The arena root is authored at world Y =
// fARENA_WORLD_Y (-2000 m) so the S5 additive load never overlaps the overworld.
class ZM_BattleArena
{
public:
	static constexpr u_int  uSERIALIZATION_VERSION = 1u;
	static constexpr u_int  uBIOME_COUNT           = ZM_BATTLE_BIOME_COUNT;   // 6
	static constexpr float  fARENA_WORLD_Y         = -2000.0f;

	// Dome + two platforms + one dressing set per biome. The arena's children are
	// spawned as ROOT entities in the arena's OWN scene (never parented), so this
	// is the full set that dies with an UnloadScene(battle).
	static constexpr u_int  uCHILD_COUNT           = 1u + 2u + ZM_BATTLE_BIOME_COUNT;

	ZM_BattleArena() = delete;
	explicit ZM_BattleArena(Zenith_Entity& xParentEntity);

	ZM_BattleArena(const ZM_BattleArena&) = delete;
	ZM_BattleArena& operator=(const ZM_BattleArena&) = delete;
	ZM_BattleArena(ZM_BattleArena&&) noexcept = default;
	ZM_BattleArena& operator=(ZM_BattleArena&&) noexcept = default;

	// Builds the arena once (dome + platforms + 6 dressing sets) and applies the
	// persisted active biome. Safe on both live-authored and disk-loaded scenes.
	void OnStart();

	// Show exactly eBiome's dressing set; hide the other five. Dome + platforms
	// unaffected. Idempotent. Returns false for an out-of-range biome (state
	// unchanged). Before OnStart runs it records the selection and returns true;
	// visibility is applied when the arena is built.
	bool SetBiome(ZM_BATTLE_BIOME eBiome);

	ZM_BATTLE_BIOME GetActiveBiome() const { return m_eActiveBiome; }
	bool            IsBuilt()        const { return m_bBuilt; }

	// True iff the arena is built AND every child entity actually spawned: the
	// dome, both platforms, and all six dressing sets. IsBuilt() alone only means
	// BuildArena() returned; this is the structural "geometry got created"
	// invariant the windowed test asserts (a regression that spawned nothing
	// would still flip m_bBuilt but fails here).
	bool            IsFullyBuilt()   const;

	// Stable child enumeration for tests and for item 4's battle director.
	// Index order is: 0 = dome, 1..2 = platforms (player, enemy),
	// 3..3+uBIOME_COUNT-1 = dressing sets in ZM_BATTLE_BIOME order.
	// Returns INVALID_ENTITY_ID when out of range or not yet built.
	Zenith_EntityID GetChildEntityID(u_int uIndex) const;

	// --- pure, headless-testable helpers (no entity/graphics state) ---

	// The ZM_PROP_DRESSING_* prop that dresses eBiome. Returns ZM_PROP_NONE out of range.
	static ZM_PROP_ID DressingPropForBiome(ZM_BATTLE_BIOME eBiome);

	// Bit i set iff dressing set i is visible for eBiome. Exactly one bit
	// (== 1u << eBiome) for a valid biome; 0 for out-of-range.
	static u_int VisibilityMaskForBiome(ZM_BATTLE_BIOME eBiome);

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

private:
	void BuildArena();            // OnStart, once: spawn dome + platforms + 6 dressing sets
	void ApplyBiomeVisibility();  // SetEnabled(active), SetEnabled(false) on the other five

	Zenith_Entity      m_xParentEntity;
	Zenith_Entity      m_xDomeEntity;
	Zenith_Entity      m_axPlatformEntities[2];
	Zenith_Entity      m_axDressingEntities[ZM_BATTLE_BIOME_COUNT];

	MeshGeometryHandle m_xDomeGeometry;
	MeshGeometryHandle m_xPlatformGeometry;
	MaterialHandle     m_xDomeMaterial;
	MaterialHandle     m_xPlatformMaterial;

	ZM_BATTLE_BIOME    m_eActiveBiome = ZM_BATTLE_BIOME_MEADOW;
	bool               m_bBuilt = false;
};
