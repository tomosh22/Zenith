#include "Zenith.h"

#include "Zenithmon/Components/ZM_Interactable.h"

#include "DataStream/Zenith_DataStream.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"   // the three shipped raise seams

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

#include <cmath>   // std::isfinite (radius sanitising)

// ============================================================================
// ZM_Interactable (S6 item 3 SC4). See the header for the contract. The role ->
// seam map is PURE and lives at the top so a unit can exercise it with no scene,
// no singleton and nothing raised; Interact() below is the only impure part.
// ============================================================================

ZM_NPC_RAISE_KIND ZM_RaiseKindForRole(ZM_NPC_ROLE eRole)
{
	switch (eRole)
	{
	case ZM_NPC_ROLE_TALKER:    return ZM_NPC_RAISE_DIALOGUE;
	case ZM_NPC_ROLE_SHOPKEEP:  return ZM_NPC_RAISE_SHOP;
	case ZM_NPC_ROLE_CARETAKER: return ZM_NPC_RAISE_CARE_CENTER;
	// A switch, not a table lookup, precisely so ZM_NPC_ROLE_COUNT and anything
	// past it land here instead of reading off the end of an array.
	default:                    return ZM_NPC_RAISE_NONE;
	}
}

const char* ZM_NpcRaiseKindName(ZM_NPC_RAISE_KIND eKind)
{
	switch (eKind)
	{
	case ZM_NPC_RAISE_NONE:        return "NONE";
	case ZM_NPC_RAISE_DIALOGUE:    return "DIALOGUE";
	case ZM_NPC_RAISE_SHOP:        return "SHOP";
	case ZM_NPC_RAISE_CARE_CENTER: return "CARE_CENTER";
	default:                       return "UNKNOWN";
	}
}

ZM_Interactable::ZM_Interactable(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void ZM_Interactable::OnStart()
{
	// Re-validate what deserialization / authoring left behind. A row id that no
	// longer exists (the roster shrank) must not survive as a live candidate.
	if (m_eNpcId >= ZM_NPC_COUNT)
	{
		m_eNpcId = ZM_NPC_NONE;
		m_bInteractable = false;
	}
	SetRadius(m_fRadius);
}

bool ZM_Interactable::SetNpcId(ZM_NPC_ID eId)
{
	if (eId >= ZM_NPC_COUNT)
	{
		// Fail CLOSED: clearing the row (rather than keeping the previous one) means a
		// bad authoring value produces an inert NPC, never a wrong conversation.
		m_eNpcId = ZM_NPC_NONE;
		m_bInteractable = false;
		return false;
	}
	m_eNpcId = eId;
	return true;
}

bool ZM_Interactable::SetRadius(float fRadius)
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

bool ZM_Interactable::Interact()
{
	if (m_eNpcId >= ZM_NPC_COUNT)
	{
		Zenith_Warning(LOG_CATEGORY_GAMEPLAY,
			"[ZM_Interactable] Interact on an UNCONFIGURED interactable (no NPC row) -- nothing raised");
		return false;
	}

	const ZM_NpcData& xRow = ZM_GetNpcData(m_eNpcId);
	const ZM_NPC_RAISE_KIND eKind = ZM_RaiseKindForRole(xRow.m_eRole);

	bool bRaised = false;
	switch (eKind)
	{
	case ZM_NPC_RAISE_DIALOGUE:
		bRaised = ZM_UI_MenuStack::TryPushDialogue(xRow.m_paszLines, xRow.m_uLineCount);
		break;
	case ZM_NPC_RAISE_SHOP:
		bRaised = ZM_UI_MenuStack::TryOpenShop(xRow.m_paeStock, xRow.m_uStockCount);
		break;
	case ZM_NPC_RAISE_CARE_CENTER:
		bRaised = ZM_UI_MenuStack::TryOpenCareCenterPrompt();
		break;
	default:
		// An unmapped role is CONTENT breakage, not a runtime condition: it must be
		// loud rather than a silent no-op (Shortfalls 1.6).
		Zenith_Warning(LOG_CATEGORY_GAMEPLAY,
			"[ZM_Interactable] NPC '%s' (id %u) has UNMAPPED role %u -- no seam to raise",
			xRow.m_szDisplayName, (u_int)m_eNpcId, (u_int)xRow.m_eRole);
		return false;
	}

	if (!bRaised)
	{
		// The seam refused. Previously this was silent, so a mis-authored NPC read as
		// a mute one with no diagnostic anywhere; name the NPC and the seam it tried.
		Zenith_Warning(LOG_CATEGORY_GAMEPLAY,
			"[ZM_Interactable] NPC '%s' (id %u, role %u) could not raise its %s screen "
			"-- the seam refused (no ZM_MenuRoot singleton, or the screen rejected its content)",
			xRow.m_szDisplayName, (u_int)m_eNpcId, (u_int)xRow.m_eRole,
			ZM_NpcRaiseKindName(eKind));
	}
	return bRaised;
}

void ZM_Interactable::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << uSERIALIZATION_VERSION;
	xStream << (u_int)m_eNpcId;
	xStream << m_fRadius;
	xStream << m_bInteractable;
}

void ZM_Interactable::ReadFromDataStream(Zenith_DataStream& xStream)
{
	u_int uVersion = 0u;
	xStream >> uVersion;
	m_eNpcId = ZM_NPC_NONE;
	m_fRadius = fDEFAULT_RADIUS;
	m_bInteractable = false;
	if (uVersion != uSERIALIZATION_VERSION)
	{
		return;
	}

	u_int uNpcId = (u_int)ZM_NPC_NONE;
	float fRadius = fDEFAULT_RADIUS;
	bool bInteractable = false;
	xStream >> uNpcId;
	xStream >> fRadius;
	xStream >> bInteractable;

	// Route every field through the validating setters, so a hand-edited or stale
	// scene file cannot install a state the live setters would have refused.
	SetNpcId((ZM_NPC_ID)uNpcId);
	SetRadius(fRadius);
	SetInteractable(bInteractable);
}

#ifdef ZENITH_TOOLS
void ZM_Interactable::RenderPropertiesPanel()
{
	const bool bHasRow = m_eNpcId < ZM_NPC_COUNT;
	ImGui::Text("NPC: %s (id %u)",
		bHasRow ? ZM_GetNpcData(m_eNpcId).m_szDisplayName : "<none>", (u_int)m_eNpcId);
	ImGui::Text("Role raise kind: %s", ZM_NpcRaiseKindName(bHasRow
		? ZM_RaiseKindForRole(ZM_GetNpcData(m_eNpcId).m_eRole)
		: ZM_NPC_RAISE_NONE));
	ImGui::Text("Reach bonus: %.2f", m_fRadius);
	ImGui::Text("Interactable: %s", IsInteractable() ? "true" : "false");
}
#endif
