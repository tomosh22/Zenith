#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Editor/TerrainEditor/Zenith_TerrainEditor.h"

#include "Core/Zenith_CommandLine.h"
#include "EntityComponent/Components/Zenith_InstancedMeshComponent.h"
#include "Flux/InstancedMeshes/Flux_InstanceGroup.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"

#include <cmath>

//=============================================================================
// TreePaint — scatters the boot-generated ProceduralTree assets as GPU
// instances. Two lockstep entities carry the tree: opaque bark trunk and
// alpha-tested leaf cards (instance groups are single-material). Both
// components VAT-sway in the wind; persistence rides the scene file via
// Zenith_InstancedMeshComponent's instance serialization.
//=============================================================================

namespace
{
	constexpr const char* szTREE_TRUNK_ENTITY  = "TerrainTrees_Trunk";
	constexpr const char* szTREE_LEAVES_ENTITY = "TerrainTrees_Leaves";
	constexpr float fTREE_SWAY_DURATION = 4.0f;   // matches the generated clip

	// Bounding sphere shared by all instances of a group (local space, scaled
	// by the instance transform): generous enough for the ~7.5m tree + crown.
	constexpr float fTREE_BOUNDS_RADIUS = 8.5f;

	Zenith_InstancedMeshComponent* ResolveTreeComponent(Zenith_EntityID uEntity)
	{
		if (uEntity == INVALID_ENTITY_ID)
		{
			return nullptr;
		}
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneDataForEntity(uEntity);
		if (pxSceneData == nullptr || !pxSceneData->EntityExists(uEntity))
		{
			return nullptr;
		}
		Zenith_Entity xEntity = pxSceneData->GetEntity(uEntity);
		if (!xEntity.HasComponent<Zenith_InstancedMeshComponent>())
		{
			return nullptr;
		}
		return &xEntity.GetComponent<Zenith_InstancedMeshComponent>();
	}

	void ConfigureTreeComponent(Zenith_InstancedMeshComponent& xComp,
		const char* szMeshBase, const char* szMaterialFile)
	{
		const std::string strDir = std::string(ENGINE_ASSETS_DIR) + "Meshes/ProceduralTree/";
		xComp.LoadMesh(strDir + szMeshBase + ZENITH_MESH_ASSET_EXT);
		xComp.LoadMaterial(std::string("engine:Meshes/ProceduralTree/") + szMaterialFile);
		xComp.LoadAnimationTexture(strDir + szMeshBase + "_Sway.zanmt");
		xComp.SetAnimationDuration(fTREE_SWAY_DURATION);
		xComp.SetBounds(Zenith_Maths::Vector3(0.0f, 4.5f, 0.0f), fTREE_BOUNDS_RADIUS);
	}
}

void Zenith_TerrainEditor::SetTreeBrushSettings(u_int uTreesPerDab, float fScaleMin,
	float fScaleMax, float fSpacing, float fMaxSlopeDeg, u_int uSeed)
{
	m_xBrush.m_uTreesPerDab    = uTreesPerDab;
	m_xBrush.m_fTreeScaleMin   = fScaleMin;
	m_xBrush.m_fTreeScaleMax   = fScaleMax;
	m_xBrush.m_fTreeSpacing    = fSpacing;
	m_xBrush.m_fTreeMaxSlopeDeg = fMaxSlopeDeg;
	// xorshift state must never be zero; uSeed == 0 keeps the fixed default so a
	// re-authored scene scatters byte-identically.
	m_uTreeRngState = (uSeed != 0u) ? uSeed : 0x51A7E5u;
}

bool Zenith_TerrainEditor::EnsureTreeEntities()
{
	// Instance groups allocate GPU buffers on first spawn — windowed only.
	if (Zenith_CommandLine::IsHeadless())
	{
		return false;
	}

	if (ResolveTreeComponent(m_uTreeTrunkEntity) != nullptr &&
		ResolveTreeComponent(m_uTreeLeavesEntity) != nullptr)
	{
		return true;
	}

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	if (pxSceneData == nullptr)
	{
		return false;
	}

	// Adopt existing entities first (scene reload, or a saved scene that
	// already carries painted trees).
	Zenith_Entity xTrunk = pxSceneData->FindEntityByName(szTREE_TRUNK_ENTITY);
	Zenith_Entity xLeaves = pxSceneData->FindEntityByName(szTREE_LEAVES_ENTITY);

	if (!xTrunk.IsValid())
	{
		xTrunk = g_xEngine.Scenes().CreateEntity(pxSceneData, szTREE_TRUNK_ENTITY);
		xTrunk.SetTransient(false);
		Zenith_InstancedMeshComponent& xComp = xTrunk.AddComponent<Zenith_InstancedMeshComponent>();
		ConfigureTreeComponent(xComp, "Tree_Trunk", "Tree_Bark.zmtrl");
	}
	if (!xLeaves.IsValid())
	{
		xLeaves = g_xEngine.Scenes().CreateEntity(pxSceneData, szTREE_LEAVES_ENTITY);
		xLeaves.SetTransient(false);
		Zenith_InstancedMeshComponent& xComp = xLeaves.AddComponent<Zenith_InstancedMeshComponent>();
		ConfigureTreeComponent(xComp, "Tree_Leaves", "Tree_Leaves.zmtrl");
	}

	m_uTreeTrunkEntity = xTrunk.GetEntityID();
	m_uTreeLeavesEntity = xLeaves.GetEntityID();
	return ResolveTreeComponent(m_uTreeTrunkEntity) != nullptr &&
	       ResolveTreeComponent(m_uTreeLeavesEntity) != nullptr;
}

void Zenith_TerrainEditor::ApplyTreeDab(float fWorldX, float fWorldZ, float fRadius,
	float fStrength, bool bErase)
{
	if (!EnsureTreeEntities())
	{
		return;
	}
	Zenith_InstancedMeshComponent* pxTrunk = ResolveTreeComponent(m_uTreeTrunkEntity);
	Zenith_InstancedMeshComponent* pxLeaves = ResolveTreeComponent(m_uTreeLeavesEntity);
	if (pxTrunk == nullptr || pxLeaves == nullptr ||
		pxTrunk->GetInstanceGroup() == nullptr)
	{
		return;
	}
	Flux_InstanceGroup* pxTrunkGroup = pxTrunk->GetInstanceGroup();

	auto NextFloat01 = [this]() -> float
	{
		u_int x = m_uTreeRngState;
		x ^= x << 13; x ^= x >> 17; x ^= x << 5;
		m_uTreeRngState = x;
		return static_cast<float>(x & 0xFFFFFFu) / 16777215.0f;
	};

	if (bErase)
	{
		// Remove every enabled tree whose trunk lands inside the brush disc.
		// Identical RemoveInstance calls on both groups keep them in lockstep.
		Zenith_Vector<uint32_t> xEnabled;
		pxTrunkGroup->ComputeVisibleIndices(xEnabled);
		const Zenith_Vector<Zenith_Maths::Matrix4>& axTransforms = pxTrunkGroup->GetTransforms();
		const float fRadiusSq = fRadius * fRadius;
		for (u_int u = 0; u < xEnabled.GetSize(); u++)
		{
			const uint32_t uSlot = xEnabled.Get(u);
			const Zenith_Maths::Matrix4& xM = axTransforms.Get(uSlot);
			const float fDX = xM[3].x - fWorldX;
			const float fDZ = xM[3].z - fWorldZ;
			if (fDX * fDX + fDZ * fDZ <= fRadiusSq)
			{
				pxTrunk->DespawnInstance(uSlot);
				pxLeaves->DespawnInstance(uSlot);
			}
		}
		return;
	}

	// Scatter placement: density scales with brush strength; rejection
	// sampling enforces slope and spacing limits.
	const u_int uTarget = std::max(1u, static_cast<u_int>(
		static_cast<float>(m_xBrush.m_uTreesPerDab) * std::max(0.1f, fStrength) + 0.5f));
	const float fMaxSlopeTan = tanf(glm::radians(m_xBrush.m_fTreeMaxSlopeDeg));
	const float fSpacingSq = m_xBrush.m_fTreeSpacing * m_xBrush.m_fTreeSpacing;

	// Snapshot the enabled instances once for the spacing test (positions
	// placed THIS dab are appended below so intra-dab spacing holds too).
	Zenith_Vector<Zenith_Maths::Vector2> xExisting;
	{
		Zenith_Vector<uint32_t> xEnabled;
		pxTrunkGroup->ComputeVisibleIndices(xEnabled);
		const Zenith_Vector<Zenith_Maths::Matrix4>& axTransforms = pxTrunkGroup->GetTransforms();
		const float fQueryRadius = fRadius + m_xBrush.m_fTreeSpacing;
		const float fQueryRadiusSq = fQueryRadius * fQueryRadius;
		for (u_int u = 0; u < xEnabled.GetSize(); u++)
		{
			const Zenith_Maths::Matrix4& xM = axTransforms.Get(xEnabled.Get(u));
			const float fDX = xM[3].x - fWorldX;
			const float fDZ = xM[3].z - fWorldZ;
			if (fDX * fDX + fDZ * fDZ <= fQueryRadiusSq)
			{
				xExisting.PushBack({ xM[3].x, xM[3].z });
			}
		}
	}

	u_int uPlaced = 0;
	for (u_int uAttempt = 0; uAttempt < uTarget * 6 && uPlaced < uTarget; uAttempt++)
	{
		const float fAngle = NextFloat01() * 6.2831853f;
		const float fDist = sqrtf(NextFloat01()) * fRadius;   // uniform over the disc
		const float fPX = fWorldX + cosf(fAngle) * fDist;
		const float fPZ = fWorldZ + sinf(fAngle) * fDist;

		// Slope rejection (central differences over 1m).
		const float fHL = SampleHeightWorld(fPX - 1.0f, fPZ);
		const float fHR = SampleHeightWorld(fPX + 1.0f, fPZ);
		const float fHD = SampleHeightWorld(fPX, fPZ - 1.0f);
		const float fHU = SampleHeightWorld(fPX, fPZ + 1.0f);
		const float fSlopeTan = 0.5f * sqrtf((fHR - fHL) * (fHR - fHL) + (fHU - fHD) * (fHU - fHD));
		if (fSlopeTan > fMaxSlopeTan)
		{
			continue;
		}

		// Spacing rejection.
		bool bTooClose = false;
		for (u_int u = 0; u < xExisting.GetSize(); u++)
		{
			const float fDX = xExisting.Get(u).x - fPX;
			const float fDZ = xExisting.Get(u).y - fPZ;
			if (fDX * fDX + fDZ * fDZ < fSpacingSq)
			{
				bTooClose = true;
				break;
			}
		}
		if (bTooClose)
		{
			continue;
		}

		// Place: random yaw, uniform scale in range, roots sunk slightly.
		const float fY = SampleHeightWorld(fPX, fPZ) - 0.08f;
		const float fScale = m_xBrush.m_fTreeScaleMin +
			(m_xBrush.m_fTreeScaleMax - m_xBrush.m_fTreeScaleMin) * NextFloat01();
		const Zenith_Maths::Quat xYaw = glm::angleAxis(NextFloat01() * 6.2831853f,
			Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		const Zenith_Maths::Vector3 xPos(fPX, fY, fPZ);
		const Zenith_Maths::Vector3 xScale(fScale, fScale * (0.95f + 0.1f * NextFloat01()), fScale);

		const uint32_t uTrunkID = pxTrunk->SpawnInstance(xPos, xYaw, xScale);
		const uint32_t uLeavesID = pxLeaves->SpawnInstance(xPos, xYaw, xScale);
		Zenith_Assert(uTrunkID == uLeavesID,
			"TreePaint: trunk/leaves instance IDs diverged (%u vs %u) — lockstep broken",
			uTrunkID, uLeavesID);

		// Same wind phase on both halves of the tree.
		const float fPhase = NextFloat01();
		pxTrunk->SetInstanceAnimationByIndex(uTrunkID, 0, fPhase);
		pxLeaves->SetInstanceAnimationByIndex(uLeavesID, 0, fPhase);

		xExisting.PushBack({ fPX, fPZ });
		uPlaced++;
	}
}

void Zenith_TerrainEditor::TickTreeSway(float fDt)
{
	// Playing mode advances via the component's OnUpdate lifecycle hook;
	// this editor-side tick keeps the wind alive in Stopped/Paused so
	// placement previews sway.
	Zenith_InstancedMeshComponent* pxTrunk = ResolveTreeComponent(m_uTreeTrunkEntity);
	Zenith_InstancedMeshComponent* pxLeaves = ResolveTreeComponent(m_uTreeLeavesEntity);
	if (pxTrunk != nullptr)
	{
		pxTrunk->Update(fDt);
	}
	if (pxLeaves != nullptr)
	{
		pxLeaves->Update(fDt);
	}
}

#endif // ZENITH_TOOLS
