#include "Zenith.h"

#include "Zenithmon/Components/ZM_BattleArena.h"

#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "Core/Zenith_Engine.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Source/Gen/ZM_PropGen.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

#include <cmath>

// The six dressing props, indexed by ZM_BATTLE_BIOME. Save-stable ordering: it
// mirrors ZM_PROP_BIOME's six real biomes (MEADOW..CANYON). NEVER reorder.
static const ZM_PROP_ID s_aeDressingProp[ZM_BATTLE_BIOME_COUNT] =
{
	ZM_PROP_DRESSING_MEADOW,
	ZM_PROP_DRESSING_VOLCANIC,
	ZM_PROP_DRESSING_COAST,
	ZM_PROP_DRESSING_WETLAND,
	ZM_PROP_DRESSING_SNOW,
	ZM_PROP_DRESSING_CANYON,
};

// Stable child-entity names, one per dressing set (deterministic; no formatting).
static const char* const s_aszDressingName[ZM_BATTLE_BIOME_COUNT] =
{
	"BattleDressingMeadow",
	"BattleDressingVolcanic",
	"BattleDressingCoast",
	"BattleDressingWetland",
	"BattleDressingSnow",
	"BattleDressingCanyon",
};

ZM_BattleArena::ZM_BattleArena(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void ZM_BattleArena::OnStart()
{
	if (m_bBuilt || !m_xParentEntity.IsValid())
	{
		return;
	}

	BuildArena();
	ApplyBiomeVisibility();
	m_bBuilt = true;
}

void ZM_BattleArena::BuildArena()
{
	// The arena's OWN scene, NOT the active scene (ZM-D-089): S5 item 3 loads the
	// battle ADDITIVELY over a still-active overworld, and DispatchPendingStarts
	// fires this OnStart before any component OnUpdate can move focus.
	// GetSceneData() re-resolves the parent entity's own slot every call, so the
	// children can never orphan into the overworld.
	Zenith_SceneData* pxSceneData = m_xParentEntity.GetSceneData();
	if (pxSceneData == nullptr)
	{
		return;
	}

	// -- Dome: a large outward-normal sphere enclosing the arena. Back-face-culled
	// from inside (acceptable for S5 item 1; the visual gate tunes appearance). --
	m_xDomeEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "BattleDome");
	m_xDomeGeometry = Zenith_MeshGeometryAsset::CreateUnitSphere(32u);
	m_xDomeMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();

	Zenith_MeshGeometryAsset* pxDomeGeomAsset = m_xDomeGeometry.GetDirect();
	Zenith_MaterialAsset* pxDomeMaterial = m_xDomeMaterial.GetDirect();
	if (pxDomeGeomAsset != nullptr && pxDomeMaterial != nullptr && m_xDomeEntity.IsValid())
	{
		pxDomeMaterial->SetName("ZM_BattleDome");
		pxDomeMaterial->SetBaseColor({ 0.16f, 0.20f, 0.30f, 1.0f });   // muted interior sky
		pxDomeMaterial->SetRoughness(0.85f);
		pxDomeMaterial->SetMetallic(0.0f);

		Zenith_TransformComponent& xTransform =
			m_xDomeEntity.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition({ 0.0f, fARENA_WORLD_Y, 0.0f });
		xTransform.SetScale({ 40.0f, 40.0f, 40.0f });

		Flux_MeshGeometry* pxDomeGeom = pxDomeGeomAsset->GetGeometry();
		if (pxDomeGeom != nullptr)
		{
			m_xDomeEntity.AddComponent<Zenith_ModelComponent>()
				.AddMeshEntry(*pxDomeGeom, *pxDomeMaterial);
		}
	}

	// -- Two battle platforms (share one cube geometry + one stone material). --
	m_xPlatformGeometry = Zenith_MeshGeometryAsset::CreateUnitCube();
	m_xPlatformMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();

	Zenith_MaterialAsset* pxPlatformMaterial = m_xPlatformMaterial.GetDirect();
	if (pxPlatformMaterial != nullptr)
	{
		pxPlatformMaterial->SetName("ZM_BattlePlatform");
		pxPlatformMaterial->SetBaseColor({ 0.44f, 0.43f, 0.40f, 1.0f });   // stone-ish
		pxPlatformMaterial->SetRoughness(0.92f);
		pxPlatformMaterial->SetMetallic(0.0f);
	}

	Zenith_MeshGeometryAsset* pxPlatformGeomAsset = m_xPlatformGeometry.GetDirect();
	Flux_MeshGeometry* pxPlatformGeom =
		pxPlatformGeomAsset != nullptr ? pxPlatformGeomAsset->GetGeometry() : nullptr;

	const Zenith_Maths::Vector3 axPlatformPos[2] =
	{
		{ -5.0f, fARENA_WORLD_Y, 0.0f },   // player side
		{  5.0f, fARENA_WORLD_Y, 0.0f },   // enemy side
	};
	const char* const aszPlatformName[2] = { "BattlePlatformPlayer", "BattlePlatformEnemy" };

	for (u_int i = 0; i < 2u; ++i)
	{
		m_axPlatformEntities[i] = g_xEngine.Scenes().CreateEntity(pxSceneData, aszPlatformName[i]);
		if (!m_axPlatformEntities[i].IsValid())
		{
			continue;
		}

		Zenith_TransformComponent& xTransform =
			m_axPlatformEntities[i].GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(axPlatformPos[i]);
		xTransform.SetScale({ 4.0f, 0.4f, 4.0f });   // flattened slab

		if (pxPlatformGeom != nullptr && pxPlatformMaterial != nullptr)
		{
			m_axPlatformEntities[i].AddComponent<Zenith_ModelComponent>()
				.AddMeshEntry(*pxPlatformGeom, *pxPlatformMaterial);
		}
	}

	// -- Six per-biome dressing sets: a deterministic ring around the arena. Each
	// entity always exists; a missing/failed baked prop just leaves it model-less. --
	const float fRingRadius = 12.0f;
	const float fTwoPi = 6.28318530717958647692f;
	for (u_int e = 0; e < ZM_BATTLE_BIOME_COUNT; ++e)
	{
		m_axDressingEntities[e] = g_xEngine.Scenes().CreateEntity(pxSceneData, s_aszDressingName[e]);
		if (!m_axDressingEntities[e].IsValid())
		{
			continue;
		}

		const float fAngle = fTwoPi * static_cast<float>(e) / static_cast<float>(ZM_BATTLE_BIOME_COUNT);
		const float fX = fRingRadius * std::sin(fAngle);
		const float fZ = fRingRadius * std::cos(fAngle);

		Zenith_TransformComponent& xTransform =
			m_axDressingEntities[e].GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition({ fX, fARENA_WORLD_Y, fZ });

		char szRef[256];
		if (ZM_PropAssetPath(DressingPropForBiome(static_cast<ZM_BATTLE_BIOME>(e)),
			ZM_PROP_ASSET_MODEL, szRef, sizeof(szRef)))
		{
			m_axDressingEntities[e].AddComponent<Zenith_ModelComponent>().LoadModel(szRef);
		}
	}
}

void ZM_BattleArena::ApplyBiomeVisibility()
{
	for (u_int e = 0; e < ZM_BATTLE_BIOME_COUNT; ++e)
	{
		if (m_axDressingEntities[e].IsValid())
		{
			m_axDressingEntities[e].SetEnabled(e == static_cast<u_int>(m_eActiveBiome));
		}
	}
}

bool ZM_BattleArena::IsFullyBuilt() const
{
	if (!m_bBuilt || !m_xDomeEntity.IsValid())
	{
		return false;
	}
	for (u_int i = 0; i < 2u; ++i)
	{
		if (!m_axPlatformEntities[i].IsValid())
		{
			return false;
		}
	}
	for (u_int e = 0; e < ZM_BATTLE_BIOME_COUNT; ++e)
	{
		if (!m_axDressingEntities[e].IsValid())
		{
			return false;
		}
	}
	return true;
}

bool ZM_BattleArena::SetBiome(ZM_BATTLE_BIOME eBiome)
{
	if (eBiome >= ZM_BATTLE_BIOME_COUNT)
	{
		return false;
	}
	m_eActiveBiome = eBiome;
	if (m_bBuilt)
	{
		ApplyBiomeVisibility();
	}
	return true;
}

ZM_PROP_ID ZM_BattleArena::DressingPropForBiome(ZM_BATTLE_BIOME eBiome)
{
	if (eBiome >= ZM_BATTLE_BIOME_COUNT)
	{
		return ZM_PROP_NONE;
	}
	return s_aeDressingProp[eBiome];
}

u_int ZM_BattleArena::VisibilityMaskForBiome(ZM_BATTLE_BIOME eBiome)
{
	return (eBiome < ZM_BATTLE_BIOME_COUNT) ? (1u << static_cast<u_int>(eBiome)) : 0u;
}

void ZM_BattleArena::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << uSERIALIZATION_VERSION;
	xStream << static_cast<u_int>(m_eActiveBiome);
}

void ZM_BattleArena::ReadFromDataStream(Zenith_DataStream& xStream)
{
	u_int uVersion = 0u;
	xStream >> uVersion;

	// Reset to defaults before the version/range gates (the ZM_WarpTrigger
	// ClearConfiguration-first idiom): rebuild on the next OnStart regardless of
	// version, and never retain a stale biome from a reused instance on a
	// version-mismatch or out-of-range blob.
	m_bBuilt = false;
	m_eActiveBiome = ZM_BATTLE_BIOME_MEADOW;

	if (uVersion != uSERIALIZATION_VERSION)
	{
		return;
	}

	u_int uBiome = 0u;
	xStream >> uBiome;
	if (uBiome < ZM_BATTLE_BIOME_COUNT)
	{
		m_eActiveBiome = static_cast<ZM_BATTLE_BIOME>(uBiome);
	}
}

Zenith_EntityID ZM_BattleArena::GetChildEntityID(u_int uIndex) const
{
	if (!m_bBuilt || uIndex >= uCHILD_COUNT)
	{
		return INVALID_ENTITY_ID;
	}
	if (uIndex == 0u)
	{
		return m_xDomeEntity.GetEntityID();
	}
	if (uIndex < 3u)
	{
		return m_axPlatformEntities[uIndex - 1u].GetEntityID();
	}
	return m_axDressingEntities[uIndex - 3u].GetEntityID();
}

#ifdef ZENITH_TOOLS
void ZM_BattleArena::RenderPropertiesPanel()
{
	ImGui::Text("Battle arena - active biome: %u (built=%s)",
		static_cast<u_int>(m_eActiveBiome), m_bBuilt ? "true" : "false");
}
#endif
