#include "Zenith.h"

#include "Zenithmon/Source/Party/ZM_Bag.h"

// ============================================================================
// ZM_Bag -- the pocketed item container (S6 item 2 SC3). Pure: no ECS, no I/O.
// Every pocket is kept sorted ASCENDING by ZM_ITEM_ID, so a single linear scan
// answers "where is this stack" AND "where would a new stack go" (FindPocketSlot),
// and inserting/erasing is a shift that preserves the order the SC6 Bag screen
// renders. See the header for the all-or-nothing mutator contract.
// ============================================================================

namespace
{
	// Scan an ascending-by-id pocket. True when eItem already has a stack
	// (uOutIndex = its slot); false when it does not (uOutIndex = the slot a new
	// stack must be inserted at to keep the pocket sorted).
	bool FindPocketSlot(const ZM_ItemStack* pxPocket, u_int uStackCount, ZM_ITEM_ID eItem, u_int& uOutIndex)
	{
		for (u_int u = 0u; u < uStackCount; ++u)
		{
			if (pxPocket[u].m_eItem == eItem)
			{
				uOutIndex = u;
				return true;
			}
			if ((u_int)pxPocket[u].m_eItem > (u_int)eItem)
			{
				uOutIndex = u;
				return false;
			}
		}
		uOutIndex = uStackCount;
		return false;
	}

	// The pocket an item routes to. Only ever called for an in-range id.
	u_int PocketIndexFor(ZM_ITEM_ID eItem)
	{
		const u_int uCategory = (u_int)ZM_GetItemData(eItem).m_eCategory;
		Zenith_Assert(uCategory < (u_int)ZM_ITEM_CATEGORY_COUNT,
			"ZM_Bag: item %u has category %u, outside the pocket range", (u_int)eItem, uCategory);
		return uCategory;
	}
}

void ZM_Bag::Clear()
{
	for (u_int uCategory = 0u; uCategory < (u_int)ZM_ITEM_CATEGORY_COUNT; ++uCategory)
	{
		for (u_int uSlot = 0u; uSlot < uZM_BAG_MAX_STACKS_PER_POCKET; ++uSlot)
		{
			m_axPocket[uCategory][uSlot] = ZM_ItemStack();
		}
		m_auPocketCount[uCategory] = 0u;
	}
}

bool ZM_Bag::Add(ZM_ITEM_ID eItem, u_int uCount)
{
	// NONE is DEFINED as ZM_ITEM_COUNT, so the range check covers it.
	if ((u_int)eItem >= (u_int)ZM_ITEM_COUNT) { return false; }
	if (uCount == 0u) { return false; }

	const u_int uCategory = PocketIndexFor(eItem);
	ZM_ItemStack* pxPocket = m_axPocket[uCategory];
	u_int& uStackCount = m_auPocketCount[uCategory];

	u_int uSlot = 0u;
	if (FindPocketSlot(pxPocket, uStackCount, eItem, uSlot))
	{
		// Headroom-first so a huge uCount can never wrap the sum past the cap.
		if (uCount > uZM_BAG_MAX_STACK_COUNT - pxPocket[uSlot].m_uCount) { return false; }
		pxPocket[uSlot].m_uCount += uCount;
		return true;
	}

	if (uCount > uZM_BAG_MAX_STACK_COUNT) { return false; }
	if (uStackCount >= uZM_BAG_MAX_STACKS_PER_POCKET) { return false; }

	// Insert at uSlot, shifting the later (higher-id) stacks up one.
	for (u_int u = uStackCount; u > uSlot; --u)
	{
		pxPocket[u] = pxPocket[u - 1u];
	}
	pxPocket[uSlot].m_eItem = eItem;
	pxPocket[uSlot].m_uCount = uCount;
	++uStackCount;
	return true;
}

bool ZM_Bag::Remove(ZM_ITEM_ID eItem, u_int uCount)
{
	if ((u_int)eItem >= (u_int)ZM_ITEM_COUNT) { return false; }
	if (uCount == 0u) { return false; }

	const u_int uCategory = PocketIndexFor(eItem);
	ZM_ItemStack* pxPocket = m_axPocket[uCategory];
	u_int& uStackCount = m_auPocketCount[uCategory];

	u_int uSlot = 0u;
	if (!FindPocketSlot(pxPocket, uStackCount, eItem, uSlot)) { return false; }
	if (pxPocket[uSlot].m_uCount < uCount) { return false; }

	pxPocket[uSlot].m_uCount -= uCount;
	if (pxPocket[uSlot].m_uCount > 0u) { return true; }

	// A count-0 stack is never stored (SaveFormat module 6): erase it, shifting the
	// later stacks down so the pocket stays sorted AND contiguous.
	for (u_int u = uSlot; u + 1u < uStackCount; ++u)
	{
		pxPocket[u] = pxPocket[u + 1u];
	}
	pxPocket[uStackCount - 1u] = ZM_ItemStack();
	--uStackCount;
	return true;
}

u_int ZM_Bag::GetCount(ZM_ITEM_ID eItem) const
{
	if ((u_int)eItem >= (u_int)ZM_ITEM_COUNT) { return 0u; }

	const u_int uCategory = PocketIndexFor(eItem);
	u_int uSlot = 0u;
	if (!FindPocketSlot(m_axPocket[uCategory], m_auPocketCount[uCategory], eItem, uSlot)) { return 0u; }
	return m_axPocket[uCategory][uSlot].m_uCount;
}

u_int ZM_Bag::PocketStackCount(ZM_ITEM_CATEGORY eCategory) const
{
	if ((u_int)eCategory >= (u_int)ZM_ITEM_CATEGORY_COUNT) { return 0u; }
	return m_auPocketCount[(u_int)eCategory];
}

const ZM_ItemStack& ZM_Bag::PocketStack(ZM_ITEM_CATEGORY eCategory, u_int uIndex) const
{
	// One shared empty stack for every out-of-range query -- never a temporary.
	static const ZM_ItemStack xEMPTY;

	if ((u_int)eCategory >= (u_int)ZM_ITEM_CATEGORY_COUNT) { return xEMPTY; }
	if (uIndex >= m_auPocketCount[(u_int)eCategory]) { return xEMPTY; }
	return m_axPocket[(u_int)eCategory][uIndex];
}

u_int ZM_Bag::TotalStackCount() const
{
	u_int uTotal = 0u;
	for (u_int uCategory = 0u; uCategory < (u_int)ZM_ITEM_CATEGORY_COUNT; ++uCategory)
	{
		uTotal += m_auPocketCount[uCategory];
	}
	return uTotal;
}
