#include "Zenith.h"

#include "Zenithmon/Components/ZM_GroundItemProp.h"

#include <cmath>    // std::isfinite -- the SetRadius guard, matching ZM_Interactable
#include <cstdio>   // snprintf -- the found-item dialogue line

#include "DataStream/Zenith_DataStream.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"   // TryGetGameState -- the live save
#include "Zenithmon/Components/ZM_UI_MenuStack.h"       // TryPushDialogue -- the raise seam
#include "Zenithmon/Source/Data/ZM_ItemData.h"          // ZM_GetItemName -- what the line says

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

// The found-item line. One line, one prop, built into a stack buffer because
// ZM_UI_DialogueBox::QueueLines COPIES into its own std::string slots -- so
// nothing here has to outlive the call.
static constexpr u_int uLINE_CAPACITY = 96u;

ZM_GroundItemProp::ZM_GroundItemProp(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void ZM_GroundItemProp::OnStart()
{
	// Nothing to establish. The authored id arrives either from ReadFromDataStream
	// (a loaded scene) or from SetGroundItemId (an authoring step), and the
	// collected answer is READ FROM THE SAVE on every query rather than latched
	// here -- which is precisely why this component needs no start-up state and
	// survives a save/load round trip without restoring anything.
	//
	// A prop whose id never took is left as ZM_GROUND_ITEM_NONE and is reported by
	// IsInteractable() as what it is. Warning here would fire once per unconfigured
	// prop per load, which is noise on a scene mid-authoring.
}

bool ZM_GroundItemProp::SetGroundItemId(ZM_GROUND_ITEM_ID eId)
{
	if ((u_int)eId >= (u_int)ZM_GROUND_ITEM_COUNT)
	{
		// FAIL CLOSED, exactly as ZM_Interactable::SetNpcId does: storing the
		// sentinel makes the prop non-interactable, where keeping the previous row
		// would leave a bad authoring value pointing at the WRONG prop -- and a prop
		// is a one-shot, so the wrong one is unrecoverable for the life of the save.
		m_eGroundItemId = ZM_GROUND_ITEM_NONE;
		return false;
	}

	m_eGroundItemId = eId;
	return true;
}

bool ZM_GroundItemProp::SetRadius(float fRadius)
{
	if (!std::isfinite(fRadius))
	{
		m_fRadius = fDEFAULT_RADIUS;
		return false;
	}
	if (fRadius < 0.0f)
	{
		m_fRadius = 0.0f;
		return false;
	}
	if (fRadius > fMAX_RADIUS)
	{
		m_fRadius = fMAX_RADIUS;
		return false;
	}

	m_fRadius = fRadius;
	return true;
}

bool ZM_GroundItemProp::IsCollected() const
{
	if ((u_int)m_eGroundItemId >= (u_int)ZM_GROUND_ITEM_COUNT)
	{
		// An unconfigured prop is not "collected" -- it is not a prop at all. Saying
		// false here keeps IsCollected answering exactly one question; the
		// registered-id half of the candidacy rule is IsInteractable's, not this
		// function's.
		return false;
	}

	ZM_GameState* pxGameState = nullptr;
	if (!ZM_GameStateManager::TryGetGameState(pxGameState) || pxGameState == nullptr)
	{
		// NOTHING HAS HAPPENED YET. A manager-less context -- a headless dispatch
		// unit, anything running before the singleton exists -- is treated as a fresh
		// save, which is the answer a fresh save would actually give.
		return false;
	}

	return pxGameState->m_xCollectedGroundItems.IsSet(m_eGroundItemId);
}

bool ZM_GroundItemProp::Interact()
{
	if ((u_int)m_eGroundItemId >= (u_int)ZM_GROUND_ITEM_COUNT)
	{
		Zenith_Warning(LOG_CATEGORY_GAMEPLAY,
			"[ZM_GroundItemProp] Interact on an UNCONFIGURED prop (no registry row) -- nothing raised");
		return false;
	}

	ZM_GameState* pxGameState = nullptr;
	if (!ZM_GameStateManager::TryGetGameState(pxGameState) || pxGameState == nullptr)
	{
		// Distinct from the unconfigured case above and deliberately so: this prop is
		// authored correctly and there is simply no save to put anything in. Silent
		// would make a genuinely broken boot look like an inert prop.
		Zenith_Warning(LOG_CATEGORY_GAMEPLAY,
			"[ZM_GroundItemProp] Interact on prop '%s' with NO live game state -- nothing picked up",
			ZM_GroundItemName(m_eGroundItemId));
		return false;
	}

	// ★ THE WHOLE RULE IS DELEGATED. This component never asks "is there room" or
	// "was it taken" on its own account -- ZM_TryPickUpGroundItem is the one place
	// the ordering (add first, mark only on success) and the blocker precedence
	// live, and a second opinion here could only ever drift from it.
	++m_uPickupAttemptCount;
	const ZM_GROUND_ITEM_PICKUP eResult =
		ZM_TryPickUpGroundItem(*pxGameState, m_eGroundItemId);
	m_eLastPickupResult = eResult;

	if (eResult != ZM_GROUND_ITEM_PICKUP_OK)
	{
		// A refusal is CONTENT, not an error: a full bag is an ordinary thing to walk
		// into, and an already-collected prop is unreachable through the picker
		// anyway (IsInteractable is false). Say so and raise nothing.
		Zenith_Log(LOG_CATEGORY_GAMEPLAY,
			"[ZM_GroundItemProp] Pickup of '%s' refused: %s",
			ZM_GroundItemName(m_eGroundItemId),
			ZM_GroundItemPickupName(eResult));
		return false;
	}

	const ZM_GroundItemInfo& xRow = ZM_GetGroundItemInfo(m_eGroundItemId);
	char acLine[uLINE_CAPACITY] = {};
	snprintf(acLine, sizeof(acLine), "Found %s x%u!",
		ZM_GetItemName(xRow.m_eItem), xRow.m_uCount);

	const char* aszLines[] = { acLine };

	// ★ THE ITEM IS ALREADY IN THE BAG BY THIS POINT, AND THAT ORDER IS CORRECT.
	// The pickup is the durable half and the dialogue is presentation; refusing the
	// pickup because a dialogue queue was full would lose the item, while a silent
	// successful pickup merely lacks its line. The return value below reports the
	// RAISE, so a caller counting raises is not told a screen went up when none did.
	return ZM_UI_MenuStack::TryPushDialogue(aszLines, 1u);
}

void ZM_GroundItemProp::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << uSERIALIZATION_VERSION;
	xStream << (u_int)m_eGroundItemId;
	xStream << m_fRadius;
}

void ZM_GroundItemProp::ReadFromDataStream(Zenith_DataStream& xStream)
{
	u_int uVersion = 0u;
	xStream >> uVersion;

	// Reset FIRST, so a version mismatch leaves an unconfigured prop rather than
	// whatever this instance happened to hold -- the ZM_SpawnPoint::ReadFromDataStream
	// shape. A relocated pool element can carry another prop's id.
	m_eGroundItemId = ZM_GROUND_ITEM_NONE;
	m_fRadius       = fDEFAULT_RADIUS;
	if (uVersion != uSERIALIZATION_VERSION)
	{
		return;
	}

	u_int uGroundItemId = (u_int)ZM_GROUND_ITEM_NONE;
	float fRadius       = fDEFAULT_RADIUS;
	xStream >> uGroundItemId;
	xStream >> fRadius;

	// Through the SETTERS, never straight onto the members: a hand-edited or
	// truncated scene must land on the same fail-closed answers an authoring step
	// would get, not bypass them because it arrived over a stream.
	SetGroundItemId((ZM_GROUND_ITEM_ID)uGroundItemId);
	SetRadius(fRadius);
}

#ifdef ZENITH_TOOLS
void ZM_GroundItemProp::RenderPropertiesPanel()
{
	ImGui::Text("Prop: %s", ZM_GroundItemName(m_eGroundItemId));
	if (m_eGroundItemId < ZM_GROUND_ITEM_COUNT)
	{
		const ZM_GroundItemInfo& xRow = ZM_GetGroundItemInfo(m_eGroundItemId);
		ImGui::Text("Yields: %s x%u", ZM_GetItemName(xRow.m_eItem), xRow.m_uCount);
		ImGui::Text("Collected: %s", IsCollected() ? "yes" : "no");
	}
	ImGui::Text("Reach bonus: %.2f", m_fRadius);
}
#endif
