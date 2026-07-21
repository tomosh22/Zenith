#pragma once

#include "Zenithmon/Source/Party/ZM_Monster.h"

// ============================================================================
// ZM_BoxStorage -- the durable overflow collection for caught monsters.
// Storage is a fixed 16 x 30 grid with deterministic box-major / slot-major
// placement. It owns no heap memory and performs no serialization or I/O.
// ============================================================================

static const u_int uZM_BOX_COUNT = 16u;
static const u_int uZM_BOX_SLOTS_PER_BOX = 30u;
static const u_int uZM_BOX_CAPACITY = uZM_BOX_COUNT * uZM_BOX_SLOTS_PER_BOX;

struct ZM_BoxStorage
{
public:
	u_int Count() const  { return m_uCount; }
	bool IsFull() const  { return m_uCount >= uZM_BOX_CAPACITY; }

	// Copy into an exact slot for transactional restore. Invalid coordinates
	// reject without mutation; replacing an occupied slot preserves the count.
	bool StoreAt(u_int uBox, u_int uSlot, const ZM_Monster& xMonster)
	{
		if (uBox >= uZM_BOX_COUNT || uSlot >= uZM_BOX_SLOTS_PER_BOX) { return false; }
		ZM_BoxSlot& xDestination = m_aaxSlots[uBox][uSlot];
		const bool bWasOccupied = xDestination.m_bOccupied;
		xDestination.m_xMonster = xMonster;
		xDestination.m_bOccupied = true;
		if (!bWasOccupied) { ++m_uCount; }
		return true;
	}

	// Clear an exact slot. Invalid and already-empty slots are harmless false
	// returns; a successful clear resets the record and decrements the count.
	bool ClearAt(u_int uBox, u_int uSlot)
	{
		if (uBox >= uZM_BOX_COUNT || uSlot >= uZM_BOX_SLOTS_PER_BOX) { return false; }
		ZM_BoxSlot& xDestination = m_aaxSlots[uBox][uSlot];
		if (!xDestination.m_bOccupied) { return false; }
		xDestination = ZM_BoxSlot{};
		--m_uCount;
		return true;
	}

	// Copy into the first unoccupied slot, scanning every slot in box-major order.
	// A full grid rejects without changing any slot or the live count.
	bool StoreFirstFree(const ZM_Monster& xMonster)
	{
		if (IsFull()) { return false; }
		for (u_int uBox = 0u; uBox < uZM_BOX_COUNT; ++uBox)
		{
			for (u_int uSlot = 0u; uSlot < uZM_BOX_SLOTS_PER_BOX; ++uSlot)
			{
				ZM_BoxSlot& xDestination = m_aaxSlots[uBox][uSlot];
				if (xDestination.m_bOccupied) { continue; }

				xDestination.m_xMonster = xMonster;
				xDestination.m_bOccupied = true;
				++m_uCount;
				return true;
			}
		}
		return false;
	}

	// Bounds-safe read. Empty slots and invalid coordinates are both represented
	// by nullptr; callers cannot accidentally index outside the fixed grid.
	const ZM_Monster* TryGet(u_int uBox, u_int uSlot) const
	{
		if (uBox >= uZM_BOX_COUNT || uSlot >= uZM_BOX_SLOTS_PER_BOX) { return nullptr; }
		const ZM_BoxSlot& xSlot = m_aaxSlots[uBox][uSlot];
		return xSlot.m_bOccupied ? &xSlot.m_xMonster : nullptr;
	}

private:
	struct ZM_BoxSlot
	{
		ZM_Monster m_xMonster;
		bool       m_bOccupied = false;
	};

	ZM_BoxSlot m_aaxSlots[uZM_BOX_COUNT][uZM_BOX_SLOTS_PER_BOX] = {};
	u_int      m_uCount = 0u;
};
