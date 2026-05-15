#include "Zenith.h"

#include "PublicInterfaces.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Navigation/Zenith_NavMeshGenerator.h"
#include "AI/Components/Zenith_AIAgentComponent.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"

#include "DP_Tuning.h"
#include "../Components/DPVillager_Behaviour.h"
#include "../Components/Priest_Behaviour.h"

#include <unordered_map>

// ============================================================================
// W0-stub coordinator state.
// Wave-2 streams (B2 player, B3 items, B6 fog, etc.) extend each section with
// real implementations as they land. Until then every function returns a safe
// default so the project compiles and links.
// ============================================================================

namespace
{
	// ---- DP_Player state (B2 fills in) ----
	Zenith_EntityID g_xPossessedVillager = INVALID_ENTITY_ID;

	// MVP-1.5: possession-cooldown timer. Decrements per frame via
	// DP_Player::TickPossessionCooldown (called from
	// DPPlayerController_Behaviour::OnUpdate). Set by
	// TryVoluntaryPossessSwitch on a successful voluntary switch /
	// release. Death + apprehend paths bypass this entirely (they
	// call SetPossessedVillager directly), matching the Tuning.json
	// canon: "cooldown_after_burnout_s = 0.0".
	float g_fPossessionCooldownRemaining = 0.0f;

	// MVP-1.8: anchor position. Set by SetPossessedVillager and
	// TryVoluntaryPossessSwitch from the new villager's transform
	// position on any successful possession. TryVoluntaryPossessSwitch
	// then enforces that subsequent voluntary switches stay within
	// `possession.range_from_anchor_m` (default 15 m) of the anchor.
	// SetPossessedVillager(INVALID) clears the anchor; the next
	// successful possession re-seeds it.
	Zenith_Maths::Vector3 g_xPossessionAnchor = Zenith_Maths::Vector3(0.0f);
	bool                  g_bHasPossessionAnchor = false;

	// MVP-1.6: per-villager scent table. Keyed by packed EntityID so
	// stale entries from destroyed villagers don't collide with reused
	// indices (the generation counter survives the pack). Reads are
	// done via GetDemonScent which returns 0 for missing entries.
	std::unordered_map<uint64_t, float> g_xDemonScent;

	// MVP-2.1.1 Devout channel state. Set by TryVoluntaryPossessSwitch
	// when the target is Devout (and channel_devout_s > 0). Decremented
	// by TickChannel per frame. Cleared by completion, InterruptChannel,
	// or ResetForTest.
	Zenith_EntityID g_xChannelTarget   = INVALID_ENTITY_ID;
	float           g_fChannelRemaining = 0.0f;
	bool            g_bChannelActive    = false;

#ifdef ZENITH_INPUT_SIMULATOR
	// MVP-1.9: test-build omniscient fallback toggle. Default ON so
	// pre-1.9 tests work without code changes. New sight-based tests
	// set this false in Setup to verify the production-shape (no
	// fallback) detection path.
	bool g_bTestOmniscientFallback = true;
#endif

	// Resolves a villager handle to its world position. Returns false
	// if the entity has no transform / no scene-data binding (e.g.,
	// passed a stale handle after the villager was destroyed).
	bool TryGetVillagerPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return false;
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return false;
		xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xOut);
		return true;
	}

	// Per-villager held-item record. Mutated by DP_Player::SetHeldItem /
	// RemoveHeldItem, read by DP_Player::GetHeldItem*. EntityID hashes via
	// (m_uIndex,m_uGeneration) — pack to uint64_t for std::unordered_map.
	uint64_t PackEntityID(Zenith_EntityID xID)
	{
		return (static_cast<uint64_t>(xID.m_uGeneration) << 32) | static_cast<uint64_t>(xID.m_uIndex);
	}

	// Inverse of PackEntityID -- recovers an EntityID from the packed
	// uint64_t key used in side-table std::unordered_maps. Used by the
	// scent table's "find highest" scan to translate a winning map entry
	// back into a handle the blackboard can store.
	Zenith_EntityID UnpackEntityID(uint64_t uKey)
	{
		Zenith_EntityID xID;
		xID.m_uIndex      = static_cast<uint32_t>(uKey & 0xFFFFFFFFull);
		xID.m_uGeneration = static_cast<uint32_t>((uKey >> 32) & 0xFFFFFFFFull);
		return xID;
	}

	struct VillagerHeldRecord
	{
		Zenith_EntityID m_xItem = INVALID_ENTITY_ID;
		DP_ItemTag      m_eTag  = DP_ItemTag::None;
	};
	std::unordered_map<uint64_t, VillagerHeldRecord> g_xHeldItems;

	// ---- DP_Items side-table (B3 fills in via DPItemBase_Behaviour::OnAwake/OnDestroy) ----
	std::unordered_map<uint64_t, DP_ItemTag> g_xItemTagTable;

	// ---- DP_Fog hole table (B6 fills in via DPFogPass_Behaviour::OnUpdate) ----
	struct FogHole
	{
		Zenith_EntityID m_xId;
		float           m_fRadius;
	};
	std::unordered_map<uint64_t, FogHole> g_xFogHoles;

	// MVP-2.4.5 memory fog state. Keyed by snapped grid cell (1 m
	// resolution); value is the cell's age in seconds since the most
	// recent RecordMemoryReveal call that hit this cell. Per the JSON
	// `_comment_memory_states`, age maps to a 4-state visibility
	// model. No implicit pruning for MVP -- entries past
	// memory_dim_s stay in VISITED_HIDDEN.
	struct MemoryCellKey
	{
		int32_t iX;
		int32_t iZ;
		bool operator==(const MemoryCellKey& o) const { return iX == o.iX && iZ == o.iZ; }
	};
	struct MemoryCellKeyHash
	{
		size_t operator()(const MemoryCellKey& k) const
		{
			// Cantor-pairing-style hash; sufficient for our scale.
			const uint64_t ux = static_cast<uint64_t>(static_cast<uint32_t>(k.iX));
			const uint64_t uz = static_cast<uint64_t>(static_cast<uint32_t>(k.iZ));
			return static_cast<size_t>((ux * 73856093u) ^ (uz * 19349663u));
		}
	};
	std::unordered_map<MemoryCellKey, float, MemoryCellKeyHash> g_xMemoryReveals;

	// ---- DP_Win state (B3 Pentagram fills in) ----
	uint32_t g_uCollectedObjectivesMask = 0;
	bool     g_bHasWon = false;
}

// ============================================================================
// DP_Player
// ============================================================================
namespace DP_Player
{
	Zenith_EntityID GetPossessedVillager()
	{
		return g_xPossessedVillager;
	}

	void SetPossessedVillager(Zenith_EntityID xId)
	{
		g_xPossessedVillager = xId;
		// MVP-1.8: keep the anchor in sync with the latest possession.
		// System callers (death cleared possession, apprehend cleared
		// possession, test setup) reach this function; clearing to
		// INVALID drops the anchor so the next voluntary attempt gets
		// a fresh seed without inheriting a stale anchor from a long-
		// dead villager.
		if (xId.IsValid() && TryGetVillagerPos(xId, g_xPossessionAnchor))
		{
			g_bHasPossessionAnchor = true;
		}
		else
		{
			g_bHasPossessionAnchor = false;
		}
	}

	// Internal helper used by both the immediate-possession path and
	// the channel-completion path. Updates possessed handle, sets
	// anchor, arms cooldown, bumps scent. Caller is responsible for
	// having already passed the gates (cooldown / state / range).
	static void CommitVoluntaryPossession(
		Zenith_EntityID xId,
		const Zenith_Maths::Vector3& xNewPos,
		bool bGotNewPos)
	{
		g_xPossessedVillager = xId;
		g_fPossessionCooldownRemaining =
			DP_Tuning::Get<float>("possession.cooldown_after_voluntary_switch_s");

		if (bGotNewPos)
		{
			g_xPossessionAnchor = xNewPos;
			g_bHasPossessionAnchor = true;
		}
		else
		{
			g_bHasPossessionAnchor = false;
		}

		// MVP-1.6 scent: only successful possession bumps. Tuning.json's
		// scent comment explicitly says channel-interrupted switches
		// produce no scent -- so the bump lives on this commit path,
		// not on TryVoluntaryPossessSwitch's entry.
		if (xId.IsValid())
		{
			const float fPerPossession =
				DP_Tuning::Get<float>("possession.demon_scent_per_possession");
			const float fMaxScent =
				DP_Tuning::Get<float>("possession.demon_scent_max");
			const uint64_t uKey = PackEntityID(xId);
			float fNew = g_xDemonScent[uKey] + fPerPossession;
			if (fNew > fMaxScent) fNew = fMaxScent;
			g_xDemonScent[uKey] = fNew;
		}
	}

	bool TryVoluntaryPossessSwitch(Zenith_EntityID xId)
	{
		// Idempotent re-click: clicking the same villager you're already
		// possessing is a no-op (and doesn't waste the cooldown window
		// or trip the range gate).
		if (xId.IsValid()
			&& g_xPossessedVillager.IsValid()
			&& xId.m_uIndex == g_xPossessedVillager.m_uIndex
			&& xId.m_uGeneration == g_xPossessedVillager.m_uGeneration)
		{
			return true;
		}

		// MVP-2.1.1: refuse a new switch attempt while a Devout channel
		// is in progress. Idempotent re-click on the SAME channel
		// target is also a no-op (the channel continues). A different
		// target is refused so spam-clicks during channel don't
		// confuse the state machine.
		if (g_bChannelActive)
		{
			if (xId.IsValid()
				&& g_xChannelTarget.IsValid()
				&& xId.m_uIndex == g_xChannelTarget.m_uIndex
				&& xId.m_uGeneration == g_xChannelTarget.m_uGeneration)
			{
				return true;
			}
			return false;
		}

		// Cooldown gate. Death/apprehend never set the cooldown, so a
		// pending switch after a respawn always succeeds without delay.
		if (g_fPossessionCooldownRemaining > 0.0f)
		{
			return false;
		}

		// MVP-1.4.1-3: state gate. Refuse fainted (still recovering) or
		// dead villagers. SetPossessedVillager (system path) bypasses
		// this -- only the player-driven voluntary switch enforces it.
		if (xId.IsValid())
		{
			Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xId);
			if (pxScene != nullptr)
			{
				Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
				if (xEnt.IsValid() && xEnt.HasComponent<Zenith_ScriptComponent>())
				{
					DPVillager_Behaviour* pxV =
						xEnt.GetComponent<Zenith_ScriptComponent>()
							.GetScript<DPVillager_Behaviour>();
					if (pxV != nullptr && !pxV->IsPossessable())
					{
						return false;
					}
				}
			}
		}

		// MVP-1.8: range gate. Anchor was set by the prior successful
		// possession (or by a direct SetPossessedVillager). If no anchor
		// has ever been set (fresh process / immediately after a death
		// that cleared it), skip the range check -- the first voluntary
		// switch establishes the anchor.
		Zenith_Maths::Vector3 xNewPos(0.0f);
		const bool bGotNewPos = xId.IsValid() && TryGetVillagerPos(xId, xNewPos);
		if (g_bHasPossessionAnchor && bGotNewPos)
		{
			const float fDx = xNewPos.x - g_xPossessionAnchor.x;
			const float fDz = xNewPos.z - g_xPossessionAnchor.z;
			const float fDist = std::sqrt(fDx * fDx + fDz * fDz);
			const float fMaxRange =
				DP_Tuning::Get<float>("possession.range_from_anchor_m");
			if (fDist > fMaxRange)
			{
				return false;
			}
		}

		// MVP-2.1.1: Devout channel dispatch. If the target's archetype
		// has a non-zero channel duration, start the channel instead
		// of committing immediately. The possession stays on the SOURCE
		// villager until TickChannel completes the timer (and dispatches
		// to CommitVoluntaryPossession). Non-Devout (channel_default_s
		// = 0) commits immediately.
		float fChannelSec = DP_Tuning::Get<float>("possession.channel_default_s");
		if (xId.IsValid())
		{
			Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xId);
			if (pxScene != nullptr)
			{
				Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
				if (xEnt.IsValid() && xEnt.HasComponent<Zenith_ScriptComponent>())
				{
					DPVillager_Behaviour* pxV =
						xEnt.GetComponent<Zenith_ScriptComponent>()
							.GetScript<DPVillager_Behaviour>();
					if (pxV != nullptr && pxV->GetArchetypeId() == "Devout")
					{
						fChannelSec = DP_Tuning::Get<float>("possession.channel_devout_s");
					}
				}
			}
		}

		if (fChannelSec > 0.0f)
		{
			// Start channel; commit deferred to TickChannel. Possession
			// state stays on the SOURCE villager (g_xPossessedVillager
			// unchanged) so a priest still pursues the original body.
			g_xChannelTarget    = xId;
			g_fChannelRemaining = fChannelSec;
			g_bChannelActive    = true;
			return true;
		}

		// Immediate commit path (non-Devout, channel = 0).
		CommitVoluntaryPossession(xId, xNewPos, bGotNewPos);
		return true;
	}

	// MVP-2.1.1/2 Devout channel API.
	bool IsChanneling()
	{
		return g_bChannelActive;
	}

	Zenith_EntityID GetChannelTarget()
	{
		return g_xChannelTarget;
	}

	float GetChannelRemaining()
	{
		return g_fChannelRemaining;
	}

	void InterruptChannel()
	{
		if (!g_bChannelActive) return;
		g_bChannelActive    = false;
		g_xChannelTarget    = INVALID_ENTITY_ID;
		g_fChannelRemaining = 0.0f;
		// NOTE: cooldown is NOT armed on interrupt (Tuning.json's
		// scent comment specifies channel-interrupted switches produce
		// no scent + no cooldown). The player can immediately retry
		// onto a different villager.
	}

	void TickChannel(float fDt)
	{
		if (!g_bChannelActive) return;

		// MVP-2.1.2 interrupt check: any priest within
		// channel_interrupt_distance_m of the channel target cancels
		// the channel before the timer runs down. The check runs
		// BEFORE the timer decrement so even the completion frame can
		// be interrupted.
		Zenith_Maths::Vector3 xTargetPos(0.0f);
		const bool bGotTargetPos =
			g_xChannelTarget.IsValid()
			&& TryGetVillagerPos(g_xChannelTarget, xTargetPos);
		if (bGotTargetPos)
		{
			const float fInterruptDist =
				DP_Tuning::Get<float>("possession.channel_interrupt_distance_m");
			bool bInterrupted = false;
			DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
				[&bInterrupted, &xTargetPos, fInterruptDist]
				(Zenith_EntityID xPriestId, Priest_Behaviour&)
				{
					if (bInterrupted) return;
					Zenith_Maths::Vector3 xPriestPos;
					if (!TryGetVillagerPos(xPriestId, xPriestPos)) return;
					const float fDx = xPriestPos.x - xTargetPos.x;
					const float fDz = xPriestPos.z - xTargetPos.z;
					const float fDistSq = fDx * fDx + fDz * fDz;
					if (fDistSq < fInterruptDist * fInterruptDist)
					{
						bInterrupted = true;
					}
				});
			if (bInterrupted)
			{
				InterruptChannel();
				return;
			}
		}

		g_fChannelRemaining -= fDt;
		if (g_fChannelRemaining <= 0.0f)
		{
			// Commit the deferred possession.
			Zenith_EntityID xTarget = g_xChannelTarget;
			g_bChannelActive    = false;
			g_xChannelTarget    = INVALID_ENTITY_ID;
			g_fChannelRemaining = 0.0f;
			Zenith_Maths::Vector3 xCommitPos(0.0f);
			const bool bGotPos =
				xTarget.IsValid() && TryGetVillagerPos(xTarget, xCommitPos);
			CommitVoluntaryPossession(xTarget, xCommitPos, bGotPos);
		}
	}

	void TickPossessionCooldown(float fDt)
	{
		if (g_fPossessionCooldownRemaining <= 0.0f) return;
		g_fPossessionCooldownRemaining -= fDt;
		if (g_fPossessionCooldownRemaining < 0.0f)
		{
			g_fPossessionCooldownRemaining = 0.0f;
		}
	}

	float GetPossessionCooldownRemaining()
	{
		return g_fPossessionCooldownRemaining;
	}

#ifdef ZENITH_INPUT_SIMULATOR
	void SetTestOmniscientFallback(bool bEnabled)
	{
		g_bTestOmniscientFallback = bEnabled;
	}
	bool IsTestOmniscientFallbackEnabled()
	{
		return g_bTestOmniscientFallback;
	}
#endif

	float GetDemonScent(Zenith_EntityID xVillager)
	{
		if (!xVillager.IsValid()) return 0.0f;
		const auto it = g_xDemonScent.find(PackEntityID(xVillager));
		if (it == g_xDemonScent.end()) return 0.0f;
		return it->second;
	}

	void TickDemonScent(float fDt)
	{
		if (g_xDemonScent.empty()) return;
		const float fDecayPerSec =
			DP_Tuning::Get<float>("possession.demon_scent_decay_per_s");
		const float fDecay = fDecayPerSec * fDt;
		// Iterate-and-erase pattern -- entries reaching <= 0 are removed
		// so the table doesn't accumulate dead handles. Reading via
		// GetDemonScent returns 0 for a missing entry, so removal is
		// observationally identical to "value = 0".
		for (auto it = g_xDemonScent.begin(); it != g_xDemonScent.end(); )
		{
			it->second -= fDecay;
			if (it->second <= 0.0f)
			{
				it = g_xDemonScent.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	void WriteHighestScentToBlackboard()
	{
		// Find the highest-scent villager.
		Zenith_EntityID xHighestId = INVALID_ENTITY_ID;
		float fHighestScent = 0.0f;
		for (const auto& xPair : g_xDemonScent)
		{
			if (xPair.second > fHighestScent)
			{
				fHighestScent = xPair.second;
				xHighestId = UnpackEntityID(xPair.first);
			}
		}

		// Write the handle to every AIAgentComponent's blackboard slot
		// across all loaded scenes. In production this is the priest's
		// blackboard; in gym scenes there may be no AIAgentComponent
		// at all (no-op). We iterate via scene Query rather than coupling
		// PublicInterfaces.cpp to Priest_Behaviour -- AIAgentComponent
		// is engine-side, Priest_Behaviour is game-side, and the data
		// path doesn't need to know which behaviour reads the key.
		const uint32_t uSlotCount = Zenith_SceneManager::GetSceneSlotCount();
		for (uint32_t uSlot = 0; uSlot < uSlotCount; ++uSlot)
		{
			Zenith_SceneData* pxScene =
				Zenith_SceneManager::GetLoadedSceneDataAtSlot(uSlot);
			if (pxScene == nullptr) continue;
			pxScene->Query<Zenith_AIAgentComponent>().ForEach(
				[xHighestId]
				(Zenith_EntityID, Zenith_AIAgentComponent& xAgent)
				{
					xAgent.GetBlackboard().SetEntityID(
						DP_AI::BB_KEY_HIGH_SCENT_TARGET, xHighestId);
				});
		}
	}

	DP_ItemTag GetHeldItemTag(Zenith_EntityID xVillager)
	{
		auto it = g_xHeldItems.find(PackEntityID(xVillager));
		if (it == g_xHeldItems.end()) return DP_ItemTag::None;
		return it->second.m_eTag;
	}

	Zenith_EntityID GetHeldItemEntity(Zenith_EntityID xVillager)
	{
		auto it = g_xHeldItems.find(PackEntityID(xVillager));
		if (it == g_xHeldItems.end()) return INVALID_ENTITY_ID;
		return it->second.m_xItem;
	}

	void SetHeldItem(Zenith_EntityID xVillager, Zenith_EntityID xItem)
	{
		VillagerHeldRecord xRec;
		xRec.m_xItem = xItem;
		xRec.m_eTag  = DP_Items::GetItemTag(xItem);
		g_xHeldItems[PackEntityID(xVillager)] = xRec;
	}

	// SourceBugFixed: GameJam0 ADPVillager::RemoveHeldItem null-derefs when no
	// held item. This port guards with early-return.
	void RemoveHeldItem(Zenith_EntityID xVillager)
	{
		auto it = g_xHeldItems.find(PackEntityID(xVillager));
		if (it == g_xHeldItems.end()) return;
		g_xHeldItems.erase(it);
	}

	void ResetForNewRun()
	{
		g_xPossessedVillager = INVALID_ENTITY_ID;
		g_xHeldItems.clear();
		g_fPossessionCooldownRemaining = 0.0f;
		g_xPossessionAnchor = Zenith_Maths::Vector3(0.0f);
		g_bHasPossessionAnchor = false;
		g_xDemonScent.clear();
		// MVP-2.1.1 channel state.
		g_xChannelTarget    = INVALID_ENTITY_ID;
		g_fChannelRemaining = 0.0f;
		g_bChannelActive    = false;
#ifdef ZENITH_INPUT_SIMULATOR
		// Restore the omniscient fallback default so each batched
		// test starts from a known state. MVP-1.9 sight tests
		// re-disable it in their own Setup.
		g_bTestOmniscientFallback = true;
#endif
	}
}

// ============================================================================
// DP_Items
// ============================================================================
namespace DP_Items
{
	DP_ItemTag GetItemTag(Zenith_EntityID xItem)
	{
		auto it = g_xItemTagTable.find(PackEntityID(xItem));
		if (it == g_xItemTagTable.end()) return DP_ItemTag::None;
		return it->second;
	}

	Vec3 GetItemWorldPos(Zenith_EntityID xItem)
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xItem);
		if (pxScene == nullptr) return Vec3(0.0f);
		Zenith_Entity xEnt = pxScene->TryGetEntity(xItem);
		if (!xEnt.IsValid()) return Vec3(0.0f);
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return Vec3(0.0f);
		Vec3 xPos;
		xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		return xPos;
	}

	bool TryConsumeKeyForUnlock(Zenith_EntityID xVillager, DP_ItemTag eRequiredKey)
	{
		// SkeletonKey is a master key — opens any lock.
		const DP_ItemTag eHeld = DP_Player::GetHeldItemTag(xVillager);
		if (eHeld == DP_ItemTag::None) return false;
		if (eHeld != eRequiredKey && eHeld != DP_ItemTag::SkeletonKey) return false;

		// SkeletonKey persists across uses (matches source); a regular Key is consumed.
		if (eHeld != DP_ItemTag::SkeletonKey)
		{
			Zenith_EntityID xItem = DP_Player::GetHeldItemEntity(xVillager);
			DP_Player::RemoveHeldItem(xVillager);
			if (xItem.IsValid())
			{
				Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xItem);
				if (pxScene != nullptr)
				{
					Zenith_Entity xEnt = pxScene->TryGetEntity(xItem);
					if (xEnt.IsValid())
					{
						Zenith_SceneManager::Destroy(xEnt);
					}
				}
			}
		}
		return true;
	}

	// SourceBugFixed: GameJam0 AItemManager::FindItemByType derefs on miss.
	// This port returns invalid EntityID; callers null-check.
	Zenith_EntityID FindItemByTag(DP_ItemTag eTag)
	{
		for (const auto& [uPacked, eItemTag] : g_xItemTagTable)
		{
			if (eItemTag == eTag)
			{
				const uint32_t uIndex      = static_cast<uint32_t>(uPacked & 0xFFFFFFFFu);
				const uint32_t uGeneration = static_cast<uint32_t>(uPacked >> 32);
				Zenith_EntityID xId;
				xId.m_uIndex = uIndex;
				xId.m_uGeneration = uGeneration;
				return xId;
			}
		}
		return INVALID_ENTITY_ID;
	}

	// Internal: B3 will call these from DPItemBase_Behaviour OnAwake/OnDestroy.
	void Internal_RegisterItemTag(Zenith_EntityID xItem, DP_ItemTag eTag)
	{
		g_xItemTagTable[PackEntityID(xItem)] = eTag;
	}
	void Internal_UnregisterItemTag(Zenith_EntityID xItem)
	{
		g_xItemTagTable.erase(PackEntityID(xItem));
	}
}

// ============================================================================
// DP_Interactables
// ============================================================================
namespace DP_Interactables
{
	void MarkAsInteractable(Zenith_EntityID /*xId*/, Kind /*eKind*/, void* /*pUserData*/)
	{
		// W0 stub. B3 wires this into DPInteractable_Behaviour during script
		// attach; this entry point is reserved for non-behaviour entities (props
		// the editor can flag as interactable).
	}
}

// ============================================================================
// DP_AI
// ============================================================================
namespace
{
	// MVP-1.2.2: real navmesh generated from active-scene collider geometry
	// via Zenith_NavMeshGenerator::GenerateFromScene. With the full engine
	// stack (PRs #32 walls block / #33 perf / #35 collider opt-out / #36
	// portal stitch / #37 region-keyed verts) and a production ground-plane
	// collider in AuthorGameLevelScene, the generated navmesh is connected
	// across the whole playable area.
	//
	// Cache key is the active scene's build index, stable across handle
	// reuse so batched tests reloading the same scene share one ~850ms
	// generation. Cache invalidation is the explicit ResetLevelNavMesh
	// call.
	//
	// Fallback path stays for scenes with no static-collider geometry
	// (FrontEnd menu) -- synthetic 200m flat quad keeps priest spawn /
	// patrol APIs functional without crashing on a null navmesh pointer.
	Zenith_NavMesh* g_pxLevelNavMesh = nullptr;
	int             g_iCachedNavMeshBuildIndex = -1;

	void BuildSyntheticFlatNavMesh()
	{
		// Build a single quad covering [-50, 250] × [-50, 250] at y=1.0 (the
		// priest/villager spawn elevation). One large convex polygon means
		// any pursuit destination lands inside the navmesh, and patrol
		// random-point sampling has the entire walkable area to choose from.
		//
		// Note: dynamic-obstacle support is wired up in Zenith_Pathfinding
		// via Zenith_NavMeshPolygon::FLAG_BLOCKED, but a single-polygon mesh
		// can't selectively block a doorway region — blocking the only
		// polygon would block everything. The plumbing is in place for when
		// this navmesh is replaced by Zenith_NavMeshGenerator's
		// scene-driven output (which produces many smaller polygons aligned
		// with collider geometry); until then, DPDoor::SyncNavMeshBlock is a
		// no-op against a one-poly mesh by design.
		const float fMinX = -50.0f;
		const float fMaxX = 250.0f;
		const float fMinZ = -50.0f;
		const float fMaxZ = 250.0f;
		const float fY    = 1.0f;

		g_pxLevelNavMesh = new Zenith_NavMesh();

		// CCW winding when viewed from above (Y-up). See navmesh CLAUDE.md
		// "Polygon Winding Order (Critical)" — V0=BL, V1=TL, V2=TR, V3=BR
		// gives positive +Y normal.
		const uint32_t uV0 = g_pxLevelNavMesh->AddVertex({ fMinX, fY, fMinZ }); // BL
		const uint32_t uV1 = g_pxLevelNavMesh->AddVertex({ fMinX, fY, fMaxZ }); // TL
		const uint32_t uV2 = g_pxLevelNavMesh->AddVertex({ fMaxX, fY, fMaxZ }); // TR
		const uint32_t uV3 = g_pxLevelNavMesh->AddVertex({ fMaxX, fY, fMinZ }); // BR

		Zenith_Vector<uint32_t> axIndices;
		axIndices.PushBack(uV0);
		axIndices.PushBack(uV1);
		axIndices.PushBack(uV2);
		axIndices.PushBack(uV3);
		g_pxLevelNavMesh->AddPolygon(axIndices);

		g_pxLevelNavMesh->ComputeSpatialData();
		g_pxLevelNavMesh->ComputeAdjacency();
		g_pxLevelNavMesh->BuildSpatialGrid();

		Zenith_Log(LOG_CATEGORY_AI,
			"DP_AI: built synthetic flat navmesh (%u verts, %u polys, bounds [%g..%g] x [%g..%g] at y=%g)",
			g_pxLevelNavMesh->GetVertexCount(),
			g_pxLevelNavMesh->GetPolygonCount(),
			fMinX, fMaxX, fMinZ, fMaxZ, fY);
	}
}

namespace DP_AI
{
	void EmitNoise(Vec3 xPos, float fLoudness, float fRadius, Zenith_EntityID xSource)
	{
		Zenith_PerceptionSystem::EmitSoundStimulus(xPos, fLoudness, fRadius, xSource);
	}

	void NotifyAllPriestsOfInvestigatePos(Vec3 xPos)
	{
		// Direct-BB-write fanout, deliberately bypassing the perception
		// system. The perception path clamps each agent's hearing radius
		// at agent_max_range (priest default 30 m) -- so a 200 m bell
		// emit doesn't reach a priest 100 m away. The GDD intent for
		// BellSoul is "map-wide" so we iterate every priest in the
		// active scene and write straight into its BB. The existing
		// DP_BTAction_ClearInvestigatePos node still owns the slot's
		// lifetime: the next time the priest reaches xPos and the
		// wait/clear sequence completes, the slot clears normally.
		DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
			[&xPos](Zenith_EntityID xPriestId, Priest_Behaviour&)
			{
				Zenith_SceneData* pxScene =
					Zenith_SceneManager::GetSceneDataForEntity(xPriestId);
				if (pxScene == nullptr) return;
				Zenith_Entity xEnt = pxScene->TryGetEntity(xPriestId);
				if (!xEnt.IsValid()) return;
				if (!xEnt.HasComponent<Zenith_AIAgentComponent>()) return;
				Zenith_AIAgentComponent& xAg =
					xEnt.GetComponent<Zenith_AIAgentComponent>();
				Zenith_Blackboard& xBB = xAg.GetBlackboard();
				xBB.SetVector3(BB_KEY_INVESTIGATE_POS, xPos);
				xBB.SetBool(BB_KEY_HAS_INVESTIGATE_POS, true);
			});
	}

	const Zenith_NavMesh* GetOrBuildLevelNavMesh()
	{
		// Cache hit: same build-indexed scene as last call.
		Zenith_Scene xActive = Zenith_SceneManager::GetActiveScene();
		const int iActiveBuildIndex = xActive.IsValid()
			? Zenith_SceneManager::GetSceneData(xActive)->GetBuildIndex()
			: -1;
		if (g_pxLevelNavMesh != nullptr && iActiveBuildIndex >= 0
			&& iActiveBuildIndex == g_iCachedNavMeshBuildIndex)
		{
			return g_pxLevelNavMesh;
		}

		// Cache miss -- drop stale + rebuild.
		delete g_pxLevelNavMesh;
		g_pxLevelNavMesh = nullptr;
		g_iCachedNavMeshBuildIndex = -1;

		if (xActive.IsValid())
		{
			Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(xActive);
			if (pxScene != nullptr)
			{
				NavMeshGenerationConfig xCfg{};
				// Tightened agent radius: DP doorways are 0.8-3m gaps and
				// 0.4m default radius erodes narrow doorways. 0.2m keeps
				// every authored doorway passable.
				xCfg.m_fAgentRadius = 0.2f;
				Zenith_NavMesh* pxGenerated =
					Zenith_NavMeshGenerator::GenerateFromScene(*pxScene, xCfg);
				if (pxGenerated != nullptr && pxGenerated->GetPolygonCount() > 0)
				{
					g_pxLevelNavMesh = pxGenerated;
					g_iCachedNavMeshBuildIndex = iActiveBuildIndex;
					Zenith_Log(LOG_CATEGORY_AI,
						"DP_AI: built real navmesh for build-index=%d (%u verts, %u polys)",
						iActiveBuildIndex,
						g_pxLevelNavMesh->GetVertexCount(),
						g_pxLevelNavMesh->GetPolygonCount());
					return g_pxLevelNavMesh;
				}
				delete pxGenerated; // null-safe
				Zenith_Log(LOG_CATEGORY_AI,
					"DP_AI: generator returned empty navmesh for build-index=%d -- falling back to synthetic flat quad",
					iActiveBuildIndex);
			}
		}

		// Fallback: legacy 200m flat quad for scenes with no static collider
		// geometry (FrontEnd menu). NOT cached against the build index --
		// the next call with real geometry will rebuild.
		BuildSyntheticFlatNavMesh();
		return g_pxLevelNavMesh;
	}

	void ResetLevelNavMesh()
	{
		delete g_pxLevelNavMesh;
		g_pxLevelNavMesh = nullptr;
		g_iCachedNavMeshBuildIndex = -1;
	}
}

// ============================================================================
// DP_Fog (B6 fills in with shader/CBV plumbing; this is the surface only)
// ============================================================================
namespace DP_Fog
{
	void RegisterFogHole(Zenith_EntityID xId, float fRadius)
	{
		FogHole xHole;
		xHole.m_xId     = xId;
		xHole.m_fRadius = fRadius;
		g_xFogHoles[PackEntityID(xId)] = xHole;
	}

	void UnregisterFogHole(Zenith_EntityID xId)
	{
		g_xFogHoles.erase(PackEntityID(xId));
	}

	void ClearAllFogHoles()
	{
		g_xFogHoles.clear();
	}

	uint32_t GetFogHoleCount()
	{
		return static_cast<uint32_t>(g_xFogHoles.size());
	}

	uint32_t GatherFogHolePositions(Vec4* pxOutHoles, uint32_t uMaxHoles)
	{
		if (pxOutHoles == nullptr || uMaxHoles == 0) return 0;
		uint32_t uWritten = 0;
		for (const auto& [uPacked, xHole] : g_xFogHoles)
		{
			if (uWritten >= uMaxHoles) break;
			Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xHole.m_xId);
			if (pxScene == nullptr) continue;
			Zenith_Entity xEnt = pxScene->TryGetEntity(xHole.m_xId);
			if (!xEnt.IsValid()) continue;
			if (!xEnt.HasComponent<Zenith_TransformComponent>()) continue;
			Vec3 xPos;
			xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
			pxOutHoles[uWritten++] = Vec4(xPos.x, xPos.y, xPos.z, xHole.m_fRadius);
		}
		return uWritten;
	}

	// ========================================================================
	// MVP-2.4.5: Memory fog implementation. State machine lives here so
	// the API surface in PublicInterfaces.h has a single owner.
	// ========================================================================
	namespace
	{
		// 1 m grid resolution: small enough that a moving villager's
		// fog hole touches multiple cells per tick (each becomes its
		// own memory entry), large enough that the cell count stays
		// bounded across a 200 m * 200 m level.
		MemoryCellKey CellKeyForPosition(const Vec3& xPos)
		{
			MemoryCellKey k;
			k.iX = static_cast<int32_t>(std::floor(xPos.x));
			k.iZ = static_cast<int32_t>(std::floor(xPos.z));
			return k;
		}
	}

	void RecordMemoryReveal(Vec3 xPosition)
	{
		const MemoryCellKey k = CellKeyForPosition(xPosition);
		// Either insert with age 0 (fresh cell) or reset existing
		// cell's age. operator[] does both atomically.
		g_xMemoryReveals[k] = 0.0f;
	}

	void TickMemoryFog(float fDt)
	{
		if (fDt <= 0.0f) return;
		for (auto& xPair : g_xMemoryReveals)
		{
			xPair.second += fDt;
		}
	}

	MemoryTileState GetMemoryStateAt(Vec3 xPosition)
	{
		const MemoryCellKey k = CellKeyForPosition(xPosition);
		const auto it = g_xMemoryReveals.find(k);
		if (it == g_xMemoryReveals.end()) return MemoryTileState::NeverSeen;
		const float fAge = it->second;
		const float fVisibleThreshold =
			DP_Tuning::Get<float>("fog_of_war.memory_visible_s");
		const float fDimThreshold =
			DP_Tuning::Get<float>("fog_of_war.memory_dim_s");
		if (fAge <= fVisibleThreshold) return MemoryTileState::VisitedVisible;
		if (fAge <= fDimThreshold)     return MemoryTileState::VisitedDim;
		return MemoryTileState::VisitedHidden;
	}

	uint32_t GetMemoryRevealCount()
	{
		return static_cast<uint32_t>(g_xMemoryReveals.size());
	}

	void ClearAllMemoryReveals()
	{
		g_xMemoryReveals.clear();
	}
}

// ============================================================================
// DP_Win
// ============================================================================
namespace DP_Win
{
	uint32_t GetCollectedObjectivesMask()
	{
		return g_uCollectedObjectivesMask;
	}

	bool HasWon()
	{
		return g_bHasWon;
	}

	void NotifyObjectiveCollected(DP_ItemTag eObjective)
	{
		const uint32_t uBit = DP_ObjectiveTagToBit(eObjective);
		if (uBit == 0) return;
		g_uCollectedObjectivesMask |= uBit;
		if (g_uCollectedObjectivesMask == DP_ALL_OBJECTIVES_MASK && !g_bHasWon)
		{
			g_bHasWon = true;
			Zenith_EventDispatcher::Get().Dispatch(DP_OnVictory{});
		}
	}

	void Reset()
	{
		g_uCollectedObjectivesMask = 0;
		g_bHasWon = false;
	}
}

// ============================================================================
// DP_Night (MVP-1.3.5 Dawn half)
// ============================================================================
namespace
{
	// Per-run timer state. Set by StartNight, ticked by TickNight,
	// cleared by Reset (between-tests hook + future "menu -> game"
	// flow). The dispatch flag prevents repeat DP_OnRunLost emits if
	// TickNight is called multiple frames after timer hits 0.
	float g_fNightRemainingSec = 0.0f;
	bool  g_bNightActive       = false;
	bool  g_bDawnDispatched    = false;
}

namespace DP_Night
{
	void StartNight(float fDurationSeconds)
	{
		g_fNightRemainingSec = (fDurationSeconds > 0.0f) ? fDurationSeconds : 0.0f;
		g_bNightActive       = true;
		// Re-arm: a new run begins, even if a prior dawn already
		// dispatched.
		g_bDawnDispatched    = false;
	}

	void TickNight(float fDt)
	{
		if (!g_bNightActive)        return;
		if (g_bDawnDispatched)
		{
			// Stay at 0; do not dispatch again until StartNight()
			// or Reset() re-arms. Pinning the remaining time at 0
			// (rather than letting it drift negative) keeps the
			// GetNightTimeRemaining() readout stable for HUD use.
			g_fNightRemainingSec = 0.0f;
			return;
		}
		g_fNightRemainingSec -= fDt;
		if (g_fNightRemainingSec <= 0.0f)
		{
			g_fNightRemainingSec = 0.0f;
			g_bDawnDispatched    = true;
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnRunLost{ DP_RunLostCause::Dawn });
		}
	}

	float GetNightTimeRemaining()
	{
		return g_fNightRemainingSec;
	}

	bool IsNightActive()
	{
		return g_bNightActive;
	}

	bool HasDawnReached()
	{
		return g_bDawnDispatched;
	}

	void Reset()
	{
		g_fNightRemainingSec = 0.0f;
		g_bNightActive       = false;
		g_bDawnDispatched    = false;
	}
}

