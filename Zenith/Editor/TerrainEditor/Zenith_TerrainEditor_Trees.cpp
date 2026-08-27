#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Editor/TerrainEditor/Zenith_TerrainEditor.h"

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
//
// The TRUNK also carries a per-instance collider config, so every painted tree
// gets one static Jolt capsule and the player collides with the grove instead
// of walking through it. The leaves deliberately do not — leaf cards are foliage
// you brush past, and a second capsule per tree would double the body count for
// nothing. Bodies are the component's, not the scene's: they are created from
// the serialized config on load and destroyed with the instance.
//=============================================================================

namespace
{
	// The entity NAMES are public members of Zenith_TerrainEditor (they are the
	// observable outcome a unit pins and RenderTest's asset guard greps for), so
	// they are deliberately not re-declared here.
	constexpr float fTREE_SWAY_DURATION = 4.0f;   // matches the generated clip

	// Bounding sphere shared by all instances of a group (local space, scaled
	// by the instance transform): generous enough for the ~7.5m tree + crown.
	constexpr float fTREE_BOUNDS_RADIUS = 8.5f;

	// Trunk collider, in the trunk mesh's LOCAL space (scaled per instance).
	// Generator dims, from Zenith_Tools_TreeAssetExport.cpp BuildTreeGraph:
	// trunk height 6.6-7.4 m, base radius 0.30-0.34 m tapering to ~62%. The
	// radius is the base-radius FLOOR -- it matches the visual at player height
	// and is only slightly proud near the crown, which is above head height and
	// so never touched. HalfCyl 3.2 + offset 3.5 span local y in [0.3, 7.0] for
	// an average trunk (capsule caps included).
	//
	// Plain literals, deliberately: these three floats serialize into
	// RenderTest.zscen through Zenith_InstancedMeshComponent, so they must never
	// be computed through glm/libm at authoring time (see the
	// ZENITH_AUTHORING_DETERMINISM pin below).
	constexpr float fTREE_TRUNK_COLLIDER_RADIUS = 0.30f;
	constexpr float fTREE_TRUNK_COLLIDER_HALF_HEIGHT = 3.2f;
	constexpr float fTREE_TRUNK_COLLIDER_Y_OFFSET = 3.5f;

	// The cached target IDs are never cleared — not on Open, not on Close, not on a
	// scene change — so this is the ONLY thing standing between a destroyed target
	// and a stale write.
	//
	// The generation half of Zenith_EntityID is NOT sufficient on its own, and the
	// comment that used to claim it was is the reason this now takes a name. A
	// generation only discriminates within ONE entity-store lifetime:
	// Zenith_SceneSystem::ResetForNextTest calls Zenith_ECS_EntityStore().Reset(),
	// which wipes slots AND generations, and the unit batch runs it after every test
	// while these cached IDs live in the editor singleton for the whole boot. After
	// such a reset the cached (slot, generation) pair can compare equal to a live,
	// completely unrelated entity — and if that entity happens to carry an
	// instanced-mesh component, ApplyTreeDab would scatter its trees into the wrong
	// instance group.
	//
	// So the NAME is the authority, exactly as it is in EnsureTreeEntities' adopt
	// path: an ID that resolves to anything not called szTREE_TRUNK_ENTITY /
	// szTREE_LEAVES_ENTITY is rejected and the caller re-authors by name. On the
	// healthy path the check always passes (the IDs are only ever assigned from
	// entities found or created under those names), so nothing about authored bytes
	// changes.
	Zenith_InstancedMeshComponent* ResolveTreeComponent(Zenith_EntityID uEntity,
		const char* szExpectedName)
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
		if (xEntity.GetName() != szExpectedName)
		{
			return nullptr;
		}
		Zenith_InstancedMeshComponent* pxComp = xEntity.TryGetComponent<Zenith_InstancedMeshComponent>();
		if (pxComp == nullptr)
		{
			return nullptr;
		}
		return pxComp;
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
	// ZEN-6: this used to `return false` on its FIRST line under
	// Zenith_IsNullRenderer(), on the reasoning that "instance groups allocate GPU
	// buffers on first spawn". That conflated two different things and skipped the
	// wrong one. Creating the entities and their Zenith_InstancedMeshComponents is
	// SCENE DATA — it is what Zenith_InstancedMeshComponent::WriteToDataStream
	// serializes — while the GPU allocation is Flux_InstanceGroup's buffer init,
	// which on the Null backend is already Zenith_Null_MemoryManager handing back
	// dummy handles and copying nothing. So the entity half must run on every
	// backend and the GPU half needs no branch at all.
	//
	// The proof that the whole chain below is backend-neutral is that a headless
	// boot ALREADY executes it, verbatim, every time it loads the committed scene:
	// Zenith_InstancedMeshComponent::ReadFromDataStream runs LoadMesh ->
	// EnsureInstanceGroupCreated -> Reserve -> InitialiseGPUBuffers ->
	// SpawnInstanceWithMatrix for each of RenderTest's 2520 tree instances.
	//
	// Consequence, and the whole point of the ticket: a Null tools boot now
	// authors an entity-COMPLETE world, so Zenith_Editor::SaveActiveScene no
	// longer needs a publish guard to stop it deleting content it never created.
	if (ResolveTreeComponent(m_uTreeTrunkEntity, szTREE_TRUNK_ENTITY) != nullptr &&
		ResolveTreeComponent(m_uTreeLeavesEntity, szTREE_LEAVES_ENTITY) != nullptr)
	{
		return true;
	}

	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
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
		// CREATE path only. An ADOPTED trunk carries its own serialized config
		// (v5+), and re-authoring one would overwrite a scene's deliberate
		// choice; the setter's idempotence guard makes adding this to the adopt
		// path safe later if a migration ever wants it.
		xComp.SetInstanceColliderCapsule(fTREE_TRUNK_COLLIDER_RADIUS,
			fTREE_TRUNK_COLLIDER_HALF_HEIGHT, fTREE_TRUNK_COLLIDER_Y_OFFSET);
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
	return ResolveTreeComponent(m_uTreeTrunkEntity, szTREE_TRUNK_ENTITY) != nullptr &&
	       ResolveTreeComponent(m_uTreeLeavesEntity, szTREE_LEAVES_ENTITY) != nullptr;
}

// Deterministic-FP: every value this function computes — scatter position, yaw quat,
// per-instance scale — is SERIALIZED into the scene file by
// Zenith_InstancedMeshComponent. Under the project's /fp:fast the trig and the
// mul/add chains here resolve differently at /Od and /O2, which made RenderTest's
// 2520 tree instances (19234 bytes) differ between a Debug and a Release tools boot.
// The rejection tests are inside the pin too: a 1-ULP shift there could accept a tree
// one build rejects, which would desync the RNG stream and change the whole scatter.
ZENITH_AUTHORING_DETERMINISM_BEGIN

void Zenith_TerrainEditor::ApplyTreeDab(float fWorldX, float fWorldZ, float fRadius,
	float fStrength, bool bErase)
{
	if (!EnsureTreeEntities())
	{
		return;
	}
	Zenith_InstancedMeshComponent* pxTrunk = ResolveTreeComponent(m_uTreeTrunkEntity, szTREE_TRUNK_ENTITY);
	Zenith_InstancedMeshComponent* pxLeaves = ResolveTreeComponent(m_uTreeLeavesEntity, szTREE_LEAVES_ENTITY);
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
	// AuthoringRadians, not glm::radians. The pin above this function is a pragma at
	// THIS call site and does not reach into glm: the template takes its FP model
	// from its own definition point and is a COMDAT shared with every /fp:fast TU.
	// Same source expression either way — degrees * 0.01745329251994329576923690768489f,
	// transcribed from glm/trigonometric.inl — so this is a compile-model swap, not a
	// math change. See Zenith_Maths.h.
	const float fMaxSlopeTan = tanf(Zenith_Maths::AuthoringRadians(m_xBrush.m_fTreeMaxSlopeDeg));
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
		const Zenith_Maths::Quat xYaw = Zenith_Maths::AuthoringRotationY(NextFloat01() * 6.2831853f);
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

ZENITH_AUTHORING_DETERMINISM_END

void Zenith_TerrainEditor::TickTreeSway(float fDt)
{
	// Playing mode advances via the component's OnUpdate lifecycle hook;
	// this editor-side tick keeps the wind alive in Stopped/Paused so
	// placement previews sway.
	Zenith_InstancedMeshComponent* pxTrunk = ResolveTreeComponent(m_uTreeTrunkEntity, szTREE_TRUNK_ENTITY);
	Zenith_InstancedMeshComponent* pxLeaves = ResolveTreeComponent(m_uTreeLeavesEntity, szTREE_LEAVES_ENTITY);
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
