#include "Zenith.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"     // Zenith_SceneSystem::Get() (Phase 2.2)
#include "ZenithECS/Internal/Zenith_RenderTaskState.h" // Zenith_ECS_EntityStore() / Zenith_ECS_IsMainThread()
#include "ZenithECS/Zenith_ComponentMeta.h"
// Phase 5b: the concrete-component Components/ include is gone -- this
// TU no longer names any concrete component (the removed transform-accessor moved to
// Zenith_Scene.cpp). The matching ecs_leaf_allowlist.txt entry goes stale.
//------------------------------------------------------------------------------
// Scene Data Access
//------------------------------------------------------------------------------

Zenith_SceneData* Zenith_Entity::GetSceneData() const
{
	// Phase 6a: cache removed. The accessor already has to read the slot to
	// validate (index bounds, occupancy, generation), so the prior cache hit
	// saved at most one vector lookup. Re-resolving every time eliminates the
	// "cache returns pointer to unloaded scene" failure mode (which helped the
	// reported Play→Menu→Stop→Play→Menu bug stay invisible until GetComponent)
	// and lets Zenith_Entity be a true value-type handle with no mutable state.
	if (!m_xEntityID.IsValid()) return nullptr;
	if (m_xEntityID.m_uIndex >= Zenith_ECS_EntityStore().m_axEntitySlots.GetSize()) return nullptr;
	const Zenith_SceneData::Zenith_EntitySlot& xSlot = Zenith_ECS_EntityStore().m_axEntitySlots.Get(m_xEntityID.m_uIndex);
	if (!xSlot.IsOccupied() || xSlot.m_uGeneration != m_xEntityID.m_uGeneration)
		return nullptr;
	return Zenith_SceneSystem::Get().GetSceneDataByHandle(xSlot.m_iSceneHandle);
}

//------------------------------------------------------------------------------
// Validity Check
//------------------------------------------------------------------------------

bool Zenith_Entity::IsValid() const
{
	Zenith_SceneData* pxSceneData = GetSceneData();
	return pxSceneData != nullptr && pxSceneData->EntityExists(m_xEntityID);
}

Zenith_Scene Zenith_Entity::GetScene() const
{
	if (!m_xEntityID.IsValid()) return Zenith_Scene::INVALID_SCENE;
	if (m_xEntityID.m_uIndex >= Zenith_ECS_EntityStore().m_axEntitySlots.GetSize()) return Zenith_Scene::INVALID_SCENE;

	const Zenith_SceneData::Zenith_EntitySlot& xSlot = Zenith_ECS_EntityStore().m_axEntitySlots.Get(m_xEntityID.m_uIndex);
	if (!xSlot.IsOccupied() || xSlot.m_uGeneration != m_xEntityID.m_uGeneration)
		return Zenith_Scene::INVALID_SCENE;

	// Use the global slot's current scene handle (survives cross-scene moves)
	return Zenith_SceneSystem::Get().GetSceneFromHandle(xSlot.m_iSceneHandle);
}

//------------------------------------------------------------------------------
// Constructors - Create NEW entities in the scene
//------------------------------------------------------------------------------

// Constructor from SceneData pointer and EntityID (used by SceneData::GetEntity)
Zenith_Entity::Zenith_Entity(Zenith_SceneData* pxSceneData, Zenith_EntityID xID)
	: m_xEntityID(xID)
{
	Zenith_Assert(pxSceneData != nullptr, "Entity created with null scene data");
	// Phase 6a: scene-data pointer no longer cached. GetSceneData re-resolves
	// from the slot's m_iSceneHandle on every call. The pxSceneData parameter
	// is retained for API stability but only the assertion uses it now.
	(void)pxSceneData;
}

// NOTE: the creating ctor Zenith_Entity(Zenith_SceneData*, const std::string&)
// was removed in Phase 3 (ECS leaf-extraction). Its body now lives in
// Zenith_SceneSystem::CreateEntityInScene (Internal/Zenith_SceneSystem_EntityOwnership.cpp),
// reached via g_xEngine.Scenes().CreateEntity(...) / CreateEntityBare(...). The
// engine's default component(s) are added there by the engine-installed
// m_pfnAddDefaultComponents hook, not by naming any component in the ECS leaf.

//------------------------------------------------------------------------------
// Hierarchy Enable/Disable Propagation
//------------------------------------------------------------------------------

void Zenith_Entity::PropagateHierarchyEnabled(Zenith_SceneData* pxSceneData, Zenith_EntityID xParentID, bool bBecomingActive)
{
	// Iterative tree walk using explicit stack (avoids stack overflow on deep hierarchies)
	Zenith_Vector<Zenith_EntityID> axStack;
	axStack.PushBack(xParentID);

	while (axStack.GetSize() > 0)
	{
		Zenith_EntityID xCurrentID = axStack.GetBack();
		axStack.PopBack();

		Zenith_Entity xCurrent = pxSceneData->GetEntity(xCurrentID);
		const Zenith_Vector<Zenith_EntityID>& axChildIDs = xCurrent.GetChildEntityIDs();

		for (u_int i = 0; i < axChildIDs.GetSize(); ++i)
		{
			Zenith_EntityID xChildID = axChildIDs.Get(i);
			if (!pxSceneData->EntityExists(xChildID)) continue;

			Zenith_Entity xChild = pxSceneData->GetEntity(xChildID);
			if (!xChild.IsEnabled()) continue;  // Only propagate to children whose activeSelf is true

			if (bBecomingActive)
			{
				if (!pxSceneData->IsOnEnableDispatched(xChildID))
				{
					Zenith_ComponentMetaRegistry::Get().DispatchOnEnable(xChild);
					pxSceneData->SetOnEnableDispatched(xChildID, true);
				}
			}
			else
			{
				if (pxSceneData->IsOnEnableDispatched(xChildID))
				{
					Zenith_ComponentMetaRegistry::Get().DispatchOnDisable(xChild);
					pxSceneData->SetOnEnableDispatched(xChildID, false);
				}
			}

			// Push child for processing its subtree
			axStack.PushBack(xChildID);
		}
	}
}

//------------------------------------------------------------------------------
// Entity State Accessors (delegate to EntitySlot)
//------------------------------------------------------------------------------

const std::string& Zenith_Entity::GetName() const
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "GetName must be called from main thread");
	Zenith_SceneData* pxSceneData = GetSceneData();
	Zenith_Assert(pxSceneData != nullptr && pxSceneData->EntityExists(m_xEntityID),
		"GetName: Entity handle is invalid (idx=%u, gen=%u)", m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	return Zenith_ECS_EntityStore().m_axEntitySlots.Get(m_xEntityID.m_uIndex).m_strName;
}

void Zenith_Entity::SetName(const std::string& strName)
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "SetName must be called from main thread");
	Zenith_SceneData* pxSceneData = GetSceneData();
	Zenith_Assert(pxSceneData != nullptr && pxSceneData->EntityExists(m_xEntityID),
		"SetName: Entity handle is invalid (idx=%u, gen=%u)", m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	Zenith_ECS_EntityStore().m_axEntitySlots.Get(m_xEntityID.m_uIndex).m_strName = strName;
	pxSceneData->MarkDirty();
}

bool Zenith_Entity::IsEnabled() const
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "IsEnabled must be called from main thread");
	Zenith_SceneData* pxSceneData = GetSceneData();
	Zenith_Assert(pxSceneData != nullptr && pxSceneData->EntityExists(m_xEntityID),
		"IsEnabled: Entity handle is invalid (idx=%u, gen=%u)", m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	return Zenith_ECS_EntityStore().m_axEntitySlots.Get(m_xEntityID.m_uIndex).m_bEnabled;
}

bool Zenith_Entity::IsActiveInHierarchy() const
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "IsActiveInHierarchy must be called from main thread");
	Zenith_SceneData* pxSceneData = GetSceneData();
	if (!pxSceneData || !pxSceneData->EntityExists(m_xEntityID)) return false;
	if (pxSceneData->IsBeingDestroyed()) return false;

	Zenith_SceneData::Zenith_EntitySlot& xSlot = Zenith_ECS_EntityStore().m_axEntitySlots.Get(m_xEntityID.m_uIndex);

	// Check own enabled flag first (fast path)
	if (!xSlot.m_bEnabled) return false;

	// Use cached value if clean
	if (!xSlot.m_bActiveInHierarchyDirty)
	{
		return xSlot.m_bActiveInHierarchy;
	}

	// Rebuild: walk up the SLOT parent chain checking each ancestor's enabled flag.
	// Phase 5a: the hierarchy now lives on the slot, so the walk no longer needs any
	// owner component — it reads each ancestor slot's m_xParentEntityID directly.
	bool bActive = true;
	Zenith_EntityID xCurrentParent = xSlot.m_xParentEntityID;
	while (xCurrentParent.IsValid())
	{
		if (xCurrentParent.m_uIndex >= Zenith_ECS_EntityStore().m_axEntitySlots.GetSize()) { bActive = false; break; }
		const Zenith_SceneData::Zenith_EntitySlot& xParentSlot = Zenith_ECS_EntityStore().m_axEntitySlots.Get(xCurrentParent.m_uIndex);
		if (!xParentSlot.IsOccupied() || xParentSlot.m_uGeneration != xCurrentParent.m_uGeneration) { bActive = false; break; }
		if (!xParentSlot.m_bEnabled) { bActive = false; break; }

		// Walk to the parent's parent via the slot (single source of truth).
		xCurrentParent = xParentSlot.m_xParentEntityID;
	}

	xSlot.m_bActiveInHierarchy = bActive;
	xSlot.m_bActiveInHierarchyDirty = false;
	return bActive;
}

void Zenith_Entity::DispatchEnableLifecycle(Zenith_SceneData* pxSceneData)
{
	Zenith_SceneData::Zenith_EntitySlot& xSlot = Zenith_ECS_EntityStore().m_axEntitySlots.Get(m_xEntityID.m_uIndex);
	bool bActiveInHierarchy = IsActiveInHierarchy();
	if (bActiveInHierarchy)
	{
		Zenith_ComponentMetaRegistry::Get().DispatchOnEnable(*this);
		xSlot.m_bOnEnableDispatched = true;

		// Unity behavior: Start() is called on the first frame AFTER the entity becomes active,
		// not in the same call stack as SetActive(true). Defer to DispatchPendingStarts.
		if (!pxSceneData->IsEntityStarted(m_xEntityID))
		{
			pxSceneData->MarkEntityPendingStart(m_xEntityID);
		}
	}

	// Propagate to children whose activeSelf is true (Unity's activeInHierarchy behavior)
	PropagateHierarchyEnabled(pxSceneData, m_xEntityID, bActiveInHierarchy);
}

void Zenith_Entity::DispatchDisableLifecycle(Zenith_SceneData* pxSceneData)
{
	Zenith_SceneData::Zenith_EntitySlot& xSlot = Zenith_ECS_EntityStore().m_axEntitySlots.Get(m_xEntityID.m_uIndex);
	if (xSlot.m_bOnEnableDispatched)
	{
		Zenith_ComponentMetaRegistry::Get().DispatchOnDisable(*this);
		xSlot.m_bOnEnableDispatched = false;
	}

	// When a parent is disabled, children that were activeInHierarchy receive OnDisable.
	PropagateHierarchyEnabled(pxSceneData, m_xEntityID, false);
}

void Zenith_Entity::SetEnabled(bool bEnabled)
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "SetEnabled must be called from main thread");
	Zenith_SceneData* pxSceneData = GetSceneData();
	Zenith_Assert(pxSceneData != nullptr && pxSceneData->EntityExists(m_xEntityID),
		"SetEnabled: Entity handle is invalid (idx=%u, gen=%u)", m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	Zenith_SceneData::Zenith_EntitySlot& xSlot = Zenith_ECS_EntityStore().m_axEntitySlots.Get(m_xEntityID.m_uIndex);
	if (xSlot.m_bEnabled == bEnabled)
	{
		return;
	}

	xSlot.m_bEnabled = bEnabled;
	pxSceneData->MarkDirty();

	Zenith_SceneData::InvalidateActiveInHierarchyCache(m_xEntityID);

	if (bEnabled)
	{
		DispatchEnableLifecycle(pxSceneData);
	}
	else
	{
		DispatchDisableLifecycle(pxSceneData);
	}
}

bool Zenith_Entity::IsTransient() const
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "IsTransient must be called from main thread");
	Zenith_SceneData* pxSceneData = GetSceneData();
	Zenith_Assert(pxSceneData != nullptr && pxSceneData->EntityExists(m_xEntityID),
		"IsTransient: Entity handle is invalid (idx=%u, gen=%u)", m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	return Zenith_ECS_EntityStore().m_axEntitySlots.Get(m_xEntityID.m_uIndex).m_bTransient;
}

void Zenith_Entity::SetTransient(bool bTransient)
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "SetTransient must be called from main thread");
	Zenith_SceneData* pxSceneData = GetSceneData();
	Zenith_Assert(pxSceneData != nullptr && pxSceneData->EntityExists(m_xEntityID),
		"SetTransient: Entity handle is invalid (idx=%u, gen=%u)", m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	Zenith_ECS_EntityStore().m_axEntitySlots.Get(m_xEntityID.m_uIndex).m_bTransient = bTransient;
}

//------------------------------------------------------------------------------
// Persistence Across Scene Loads
//------------------------------------------------------------------------------

void Zenith_Entity::DontDestroyOnLoad()
{
	Zenith_SceneSystem::Get().MarkEntityPersistent(*this);
}

//------------------------------------------------------------------------------
// Parent/Child Hierarchy (SLOT-BACKED — single source of truth, Phase 5a)
//
// The parent/child links live on Zenith_EntitySlot. These methods read/write the
// slot via the process-wide entity store (Zenith_ECS_EntityStore()), the same
// path the rest of the ECS uses. The algorithms here are ported VERBATIM from the
// former component-side hierarchy implementation (same checks, same
// order, same warnings/asserts, same cache-invalidation) -- only the backing
// storage changed from a per-component member to a per-slot member.
//------------------------------------------------------------------------------

namespace
{
	// File-local slot accessor by EntityID. Mirrors the inline pattern used in
	// GetName/SetEnabled/etc. above. Caller has already validated the entity.
	Zenith_EntitySlot& ECS_SlotByID(Zenith_EntityID xID)
	{
		return Zenith_ECS_EntityStore().m_axEntitySlots.Get(xID.m_uIndex);
	}
}

Zenith_EntityID Zenith_Entity::GetParentEntityID() const
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "GetParentEntityID must be called from main thread");
	Zenith_Assert(IsValid(), "GetParentEntityID: Entity handle is invalid (idx=%u, gen=%u)",
		m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	return ECS_SlotByID(m_xEntityID).m_xParentEntityID;
}

bool Zenith_Entity::HasParent() const
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "HasParent must be called from main thread");
	Zenith_Assert(IsValid(), "HasParent: Entity handle is invalid (idx=%u, gen=%u)",
		m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	return ECS_SlotByID(m_xEntityID).m_xParentEntityID.IsValid();
}

bool Zenith_Entity::IsDescendantOf(Zenith_EntityID uAncestorID) const
{
	// Slot-backed port of the former component-side IsDescendantOf. Walk up the
	// SLOT parent chain looking for the ancestor, with the same depth guard.
	if (uAncestorID == INVALID_ENTITY_ID)
	{
		return false;
	}

	Zenith_SceneData* pxSceneData = GetSceneData();
	if (pxSceneData == nullptr)
	{
		return false;
	}

	if (!pxSceneData->EntityExists(m_xEntityID)) return false;
	Zenith_EntityID uCurrentID = ECS_SlotByID(m_xEntityID).m_xParentEntityID;

	constexpr u_int MAX_HIERARCHY_DEPTH = 1000;
	u_int uDepth = 0;

	while (uCurrentID != INVALID_ENTITY_ID && uDepth < MAX_HIERARCHY_DEPTH)
	{
		if (uCurrentID == uAncestorID)
		{
			return true;
		}

		if (!pxSceneData->EntityExists(uCurrentID))
		{
			return false;
		}

		uCurrentID = ECS_SlotByID(uCurrentID).m_xParentEntityID;
		++uDepth;
	}

	if (uDepth >= MAX_HIERARCHY_DEPTH)
	{
		Zenith_Error(LOG_CATEGORY_ECS, "IsDescendantOf: Max hierarchy depth %u exceeded for entity %u - possible circular reference",
			MAX_HIERARCHY_DEPTH, m_xEntityID.m_uIndex);
	}

	return false;
}

void Zenith_Entity::SetParent(Zenith_EntityID xParentID)
{
	// Slot-backed port of the former component-side SetParentByID -- the
	// bidirectional-link logic now lives here. Preserves: early-return-if-same,
	// same-scene check, descendant-cycle check, remove-from-old-parent +
	// add-to-new-parent (PushBack order), and the active-cache invalidation.
	Zenith_Assert(Zenith_ECS_IsMainThread(), "SetParent must be called from main thread");
	Zenith_Assert(IsValid(), "SetParent: Entity handle is invalid (idx=%u, gen=%u)",
		m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);

	Zenith_EntityID uNewParentID = xParentID;

	Zenith_EntitySlot& xMySlot = ECS_SlotByID(m_xEntityID);
	if (xMySlot.m_xParentEntityID == uNewParentID) return;

	Zenith_SceneData* pxSceneData = GetSceneData();
	if (pxSceneData == nullptr)
	{
		Zenith_Warning(LOG_CATEGORY_ECS, "SetParent: Entity has no scene");
		return;
	}

	Zenith_EntityID uMyEntityID = m_xEntityID;

	// CIRCULAR HIERARCHY CHECKS (Unity-style safety)
	if (uNewParentID != INVALID_ENTITY_ID)
	{
		// Preserve the former Zenith_Entity::SetParent contract: a valid (non-INVALID)
		// parent ID is asserted to reference an existing entity. The ported
		// component-side checks below also handle a non-existent parent gracefully
		// (warn + return), but this keeps the original debug-assert behavior.
		Zenith_Assert(pxSceneData->EntityExists(uNewParentID), "SetParent: Parent entity (idx=%u, gen=%u) does not exist",
			uNewParentID.m_uIndex, uNewParentID.m_uGeneration);

		// Unity parity: Cannot parent to an entity in a different scene.
		int iParentScene = Zenith_SceneData::GetEntitySceneHandle(uNewParentID);
		int iMyScene = Zenith_SceneData::GetEntitySceneHandle(uMyEntityID);
		Zenith_Assert(iParentScene != -1, "SetParent: Parent entity index out of range");
		Zenith_Assert(iParentScene == iMyScene,
			"SetParent: Cannot parent entity to an entity in a different scene (child scene=%d, parent scene=%d)",
			iMyScene, iParentScene);

		// Cannot parent to self
		if (uNewParentID == uMyEntityID)
		{
			Zenith_Warning(LOG_CATEGORY_ECS, "Cannot parent entity %u to itself", uMyEntityID.m_uIndex);
			return;
		}

		// Cannot parent to a descendant (would create cycle)
		if (pxSceneData->EntityExists(uNewParentID))
		{
			Zenith_Entity xProposedParent(pxSceneData, uNewParentID);
			if (xProposedParent.IsDescendantOf(uMyEntityID))
			{
				Zenith_Warning(LOG_CATEGORY_ECS, "Cannot parent entity %u to %u - would create circular hierarchy",
					uMyEntityID.m_uIndex, uNewParentID.m_uIndex);
				return;
			}
		}
		else
		{
			// Parent entity doesn't exist
			Zenith_Warning(LOG_CATEGORY_ECS, "Cannot parent entity %u to non-existent entity %u",
				uMyEntityID.m_uIndex, uNewParentID.m_uIndex);
			return;
		}
	}

	// Remove from old parent's children
	if (xMySlot.m_xParentEntityID != INVALID_ENTITY_ID && pxSceneData->EntityExists(xMySlot.m_xParentEntityID))
	{
		ECS_SlotByID(xMySlot.m_xParentEntityID).m_xChildEntityIDs.EraseValue(uMyEntityID);
	}

	xMySlot.m_xParentEntityID = uNewParentID;

	// Add to new parent's children (preserve PushBack order)
	if (xMySlot.m_xParentEntityID != INVALID_ENTITY_ID && pxSceneData->EntityExists(xMySlot.m_xParentEntityID))
	{
		ECS_SlotByID(xMySlot.m_xParentEntityID).m_xChildEntityIDs.PushBack(uMyEntityID);
	}

	// Invalidate root entity cache since parent changed (entity may have become/stopped being a root)
	pxSceneData->InvalidateRootEntityCache();

	// Invalidate cached activeInHierarchy (new parent may have different enabled state).
	// (The former Zenith_Entity::SetParent shim did this after delegating to the
	// owning component; preserved here so the cache stays consistent with the slot chain.)
	Zenith_SceneData::InvalidateActiveInHierarchyCache(m_xEntityID);

	// Re-parenting changes this entity's world transform (new ancestor chain) and thus
	// every descendant's — bump the whole subtree so cached world matrices recompute.
	Zenith_SceneData::BumpHierarchyRevision(m_xEntityID);
}

void Zenith_Entity::DetachFromParent()
{
	SetParent(INVALID_ENTITY_ID);
}

void Zenith_Entity::DetachAllChildren()
{
	// Slot-backed port of the former component-side DetachAllChildren. Clear each
	// child's slot parent directly (in vector order), then bulk-clear our list —
	// avoids O(n^2) from per-child EraseValue on our own list.
	Zenith_Assert(Zenith_ECS_IsMainThread(), "DetachAllChildren must be called from main thread");
	Zenith_SceneData* pxSceneData = GetSceneData();
	if (pxSceneData == nullptr || !pxSceneData->EntityExists(m_xEntityID))
	{
		// No scene / stale handle — just clear our list directly if reachable.
		if (m_xEntityID.m_uIndex < Zenith_ECS_EntityStore().m_axEntitySlots.GetSize())
		{
			ECS_SlotByID(m_xEntityID).m_xChildEntityIDs.Clear();
		}
		return;
	}

	Zenith_EntitySlot& xMySlot = ECS_SlotByID(m_xEntityID);
	for (u_int i = 0; i < xMySlot.m_xChildEntityIDs.GetSize(); ++i)
	{
		Zenith_EntityID uChildID = xMySlot.m_xChildEntityIDs.Get(i);
		if (pxSceneData->EntityExists(uChildID))
		{
			ECS_SlotByID(uChildID).m_xParentEntityID = INVALID_ENTITY_ID;
			// The child (now a root) and its descendants lose this entity's transform
			// contribution — bump each detached subtree so their caches recompute. The
			// grandchild links are still intact, so the DFS reaches them.
			Zenith_SceneData::BumpHierarchyRevision(uChildID);
		}
	}
	xMySlot.m_xChildEntityIDs.Clear();
	pxSceneData->InvalidateRootEntityCache();
}

const Zenith_Vector<Zenith_EntityID>& Zenith_Entity::GetChildEntityIDs() const
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "GetChildEntityIDs must be called from main thread");
	Zenith_Assert(IsValid(), "GetChildEntityIDs: Entity handle is invalid (idx=%u, gen=%u)",
		m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	return ECS_SlotByID(m_xEntityID).m_xChildEntityIDs;
}

bool Zenith_Entity::HasChildren() const
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "HasChildren must be called from main thread");
	Zenith_Assert(IsValid(), "HasChildren: Entity handle is invalid (idx=%u, gen=%u)",
		m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	return ECS_SlotByID(m_xEntityID).m_xChildEntityIDs.GetSize() > 0;
}

uint32_t Zenith_Entity::GetChildCount() const
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "GetChildCount must be called from main thread");
	Zenith_Assert(IsValid(), "GetChildCount: Entity handle is invalid (idx=%u, gen=%u)",
		m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	return static_cast<uint32_t>(ECS_SlotByID(m_xEntityID).m_xChildEntityIDs.GetSize());
}

bool Zenith_Entity::IsRoot() const
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "IsRoot must be called from main thread");
	Zenith_Assert(IsValid(), "IsRoot: Entity handle is invalid (idx=%u, gen=%u)",
		m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	return !ECS_SlotByID(m_xEntityID).m_xParentEntityID.IsValid();
}

// Phase 5b: the removed transform-accessor named a concrete engine component
// type (a non-hierarchy reference), so to keep this ECS-leaf TU free of any
// Components/ include its definition was relocated to Zenith_Scene.cpp (a sibling
// ECS TU that already carries the concrete-component include). The declaration stays in
// Zenith_Entity.h; behaviour is unchanged.

//------------------------------------------------------------------------------
// Pending-parent file index (slot-backed, load-time only)
//------------------------------------------------------------------------------

void Zenith_Entity::SetPendingParentFileIndex(uint32_t uIndex)
{
	Zenith_Assert(IsValid(), "SetPendingParentFileIndex: Entity handle is invalid (idx=%u, gen=%u)",
		m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	ECS_SlotByID(m_xEntityID).m_uPendingParentFileIndex = uIndex;
}

uint32_t Zenith_Entity::GetPendingParentFileIndex() const
{
	Zenith_Assert(IsValid(), "GetPendingParentFileIndex: Entity handle is invalid (idx=%u, gen=%u)",
		m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	return ECS_SlotByID(m_xEntityID).m_uPendingParentFileIndex;
}

void Zenith_Entity::ClearPendingParentFileIndex()
{
	Zenith_Assert(IsValid(), "ClearPendingParentFileIndex: Entity handle is invalid (idx=%u, gen=%u)",
		m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	ECS_SlotByID(m_xEntityID).m_uPendingParentFileIndex = Zenith_EntityID::INVALID_INDEX;
}

//------------------------------------------------------------------------------
// Serialization
//------------------------------------------------------------------------------

void Zenith_Entity::WriteToDataStream(Zenith_DataStream& xStream)
{
	Zenith_Assert(IsValid(), "WriteToDataStream: Entity handle is invalid");

	// Scene v7 entity RECORD: [fileIndex][name][parentFileIndex] then components.
	// Write entity index only (generation is runtime-only for stale detection).
	xStream << m_xEntityID.m_uIndex;
	xStream << GetName();  // Get name from slot

	// Phase 7a: the hierarchy parent file-index now lives in the entity record (it left
	// the legacy component payload). The file-index scheme is the slot index m_uIndex -- the same
	// value the loader keys xFileIndexToNewID on (each entity writes its own m_uIndex as
	// its file index), so the parent's m_uIndex is exactly the parent's file index. A
	// root entity writes INVALID_INDEX.
	Zenith_EntityID xParentID = GetParentEntityID();
	uint32_t uParentFileIndex = xParentID.IsValid() ? xParentID.m_uIndex : Zenith_EntityID::INVALID_INDEX;
	xStream << uParentFileIndex;

	// Serialize all components using the ComponentMeta registry (the owning component
	// now writes its own payload only -- no parent).
	Zenith_ComponentMetaRegistry::Get().SerializeEntityComponents(*this, xStream);
}

void Zenith_Entity::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Symmetric counterpart to WriteToDataStream (current/v7 format). NOTE: the on-disk
	// scene loader does NOT call this -- it uses Zenith_SceneData::ReadEntityFromDataStream,
	// which version-gates the record layout for legacy v3/4/5/6 files. This method assumes
	// the CURRENT (v7) record layout: [fileIndex][name][parentFileIndex] then components.
	// Read entity index - generation will be assigned fresh on load
	uint32_t uFileIndex;
	xStream >> uFileIndex;

	std::string strName;
	xStream >> strName;

	// v7: read the parent file-index from the record and SINK it to the slot
	// pending-parent (the scene's post-load resolve maps it to a real EntityID).
	uint32_t uParentFileIndex;
	xStream >> uParentFileIndex;

	// Set the name in the slot if we have a valid entity
	Zenith_SceneData* pxSceneData = GetSceneData();
	if (pxSceneData != nullptr && IsValid())
	{
		Zenith_ECS_EntityStore().m_axEntitySlots.Get(m_xEntityID.m_uIndex).m_strName = strName;
		SetPendingParentFileIndex(uParentFileIndex);
	}

	// Note: m_xEntityID is set by the scene during loading, not here
	// (old format support - scene handles ID assignment now)

	// Deserialize all components using the ComponentMeta registry. The owning component's
	// versioned reader (schema 7) reads its own payload only; the parent already came from the record above.
	Zenith_ComponentMetaRegistry::Get().DeserializeEntityComponents(*this, xStream);
}

//------------------------------------------------------------------------------
// Destruction (Unity-style API)
//------------------------------------------------------------------------------

void Zenith_Entity::Destroy()
{
	Zenith_SceneSystem::Get().Destroy(*this);
}

void Zenith_Entity::Destroy(float fDelay)
{
	Zenith_SceneSystem::Get().Destroy(*this, fDelay);
}

void Zenith_Entity::DestroyImmediate()
{
	Zenith_SceneSystem::Get().DestroyImmediate(*this);
}

//------------------------------------------------------------------------------
// Cross-scene movement
//------------------------------------------------------------------------------

void Zenith_Entity::MoveToScene(Zenith_Scene xTarget)
{
	Zenith_SceneSystem::Get().MoveEntityToScene(*this, xTarget);
}
