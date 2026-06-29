#include "Zenith.h"
#include "Flux/MeshAnimation/Flux_AnimationControllerStore.h"
#include "Flux/MeshAnimation/Flux_AnimationController.h"
// Full Zenith_EntityID definition (slot index + generation). This is the only
// cross-layer (Flux -> EntityComponent) include the store needs: the store is
// keyed by the entity SLOT index. It replaces the far heavier per-TU coupling
// that the old Zenith_AnimatorComponent.h => Flux_AnimationController.h edge
// caused (every TU touching the component dragged in the whole controller).
#include "ZenithECS/Zenith_Entity.h"

Flux_AnimationControllerStore::~Flux_AnimationControllerStore()
{
	// Free every owned controller. Each was individually new'd. Controllers hold no
	// GPU resources (skinning matrices are read CPU-side by the unified compute-skinning
	// path), so no device-lifetime ordering constraint applies here.
	for (u_int u = 0; u < m_xControllers.GetSize(); ++u)
	{
		delete m_xControllers.Get(u);
	}
	m_xControllers.Clear();
	m_xControllerSlots.Clear();
	m_xControllerGenerations.Clear();
	m_xSlotToController.Clear();
}

u_int Flux_AnimationControllerStore::SlotToControllerIndex(u_int uSlot) const
{
	if (uSlot >= m_xSlotToController.GetSize())
	{
		return uINVALID;
	}
	return m_xSlotToController.Get(uSlot);
}

void Flux_AnimationControllerStore::EnsureSlotCapacity(u_int uSlot)
{
	// Lazy grow: pad with the uINVALID sentinel up to and including uSlot.
	while (m_xSlotToController.GetSize() <= uSlot)
	{
		m_xSlotToController.PushBack(uINVALID);
	}
}

Flux_AnimationController& Flux_AnimationControllerStore::GetOrCreate(Zenith_EntityID xID)
{
	const u_int uSlot = xID.m_uIndex;

	const u_int uExisting = SlotToControllerIndex(uSlot);
	if (uExisting != uINVALID)
	{
		if (m_xControllerGenerations.Get(uExisting) == xID.m_uGeneration)
		{
			// Same entity — return its existing controller.
			return *m_xControllers.Get(uExisting);
		}

		// The slot was recycled to a NEW entity (different generation) while the
		// PRIOR owner's controller was still mapped — i.e. the old owner's
		// Destroy never ran before the slot was reissued. Tear the stale
		// controller down so the new entity gets a clean controller rather than
		// inheriting stale animation state, then fall through to create a fresh one.
		DestroyControllerAt(uExisting, uSlot);
	}

	// Allocate a fresh controller and append it to the dense array.
	Flux_AnimationController* pxController = new Flux_AnimationController();
	const u_int uNewIndex = m_xControllers.GetSize();
	m_xControllers.PushBack(pxController);
	m_xControllerSlots.PushBack(uSlot);
	m_xControllerGenerations.PushBack(xID.m_uGeneration);

	EnsureSlotCapacity(uSlot);
	m_xSlotToController.Get(uSlot) = uNewIndex;

	return *pxController;
}

Flux_AnimationController* Flux_AnimationControllerStore::TryGet(Zenith_EntityID xID)
{
	const u_int uIndex = SlotToControllerIndex(xID.m_uIndex);
	if (uIndex == uINVALID)
	{
		return nullptr;
	}
	// A stale id for a recycled slot must NOT resolve to the new occupant.
	if (m_xControllerGenerations.Get(uIndex) != xID.m_uGeneration)
	{
		return nullptr;
	}
	return m_xControllers.Get(uIndex);
}

const Flux_AnimationController* Flux_AnimationControllerStore::TryGet(Zenith_EntityID xID) const
{
	const u_int uIndex = SlotToControllerIndex(xID.m_uIndex);
	if (uIndex == uINVALID)
	{
		return nullptr;
	}
	// A stale id for a recycled slot must NOT resolve to the new occupant.
	if (m_xControllerGenerations.Get(uIndex) != xID.m_uGeneration)
	{
		return nullptr;
	}
	return m_xControllers.Get(uIndex);
}

Flux_AnimationController& Flux_AnimationControllerStore::Get(Zenith_EntityID xID)
{
	Flux_AnimationController* pxController = TryGet(xID);
	Zenith_Assert(pxController != nullptr,
		"Flux_AnimationControllerStore::Get: no controller for entity slot %u. "
		"GetOrCreate must run (OnStart / ctor) before forwarding accessors are used.",
		xID.m_uIndex);
	return *pxController;
}

const Flux_AnimationController& Flux_AnimationControllerStore::Get(Zenith_EntityID xID) const
{
	const Flux_AnimationController* pxController = TryGet(xID);
	Zenith_Assert(pxController != nullptr,
		"Flux_AnimationControllerStore::Get: no controller for entity slot %u. "
		"GetOrCreate must run (OnStart / ctor) before forwarding accessors are used.",
		xID.m_uIndex);
	return *pxController;
}

bool Flux_AnimationControllerStore::Destroy(Zenith_EntityID xID)
{
	const u_int uSlot = xID.m_uIndex;
	const u_int uIndex = SlotToControllerIndex(uSlot);
	if (uIndex == uINVALID)
	{
		// Idempotent: nothing to do. (Already destroyed, or a moved-from
		// component / never-created entity calling Destroy.)
		return false;
	}

	// A stale id for a recycled slot must NOT destroy the new occupant's
	// controller. The new owner keeps its controller; the stale caller simply
	// gets "nothing destroyed" (still idempotent + generation-safe).
	if (m_xControllerGenerations.Get(uIndex) != xID.m_uGeneration)
	{
		return false;
	}

	DestroyControllerAt(uIndex, uSlot);
	return true;
}

void Flux_AnimationControllerStore::DestroyControllerAt(u_int uControllerIndex, u_int uSlot)
{
	// Free the owned controller.
	delete m_xControllers.Get(uControllerIndex);

	// Swap-and-pop the dense arrays. RemoveSwap moves the LAST element into
	// uControllerIndex; we must repoint that moved element's slot entry at
	// uControllerIndex so its lookup stays valid. Capture the moved slot BEFORE
	// the removal.
	const u_int uLastIndex = m_xControllers.GetSize() - 1;
	const u_int uMovedSlot = m_xControllerSlots.Get(uLastIndex);

	m_xControllers.RemoveSwap(uControllerIndex);
	m_xControllerSlots.RemoveSwap(uControllerIndex);
	m_xControllerGenerations.RemoveSwap(uControllerIndex);

	// Clear this slot's mapping first...
	m_xSlotToController.Get(uSlot) = uINVALID;

	// ...then, if a different element was relocated into uControllerIndex,
	// repoint its slot entry. (When uControllerIndex WAS the last element,
	// uMovedSlot == uSlot and the entry we just cleared is correct — guard
	// against re-setting it.)
	if (uControllerIndex != uLastIndex && uMovedSlot != uSlot)
	{
		m_xSlotToController.Get(uMovedSlot) = uControllerIndex;
	}
}
