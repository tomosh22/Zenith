#pragma once

#include "ZenithECS/Zenith_Entity.h"
#include "Zenithmon/Source/Data/ZM_NpcData.h"   // ZM_NPC_ID / ZM_NPC_ROLE -- the row this component IS

class Zenith_DataStream;

// ============================================================================
// ZM_Interactable (S6 item 3 SC4) -- the ECS component that makes an authored NPC
// TALKABLE: it says WHICH ZM_NpcData row an entity is, how far its reach extends,
// whether it is currently a candidate at all, and it owns the ONE dispatch point
// that turns "the player pressed E at me" into a raised screen.
//
// Serialization order 113 (registered in Zenithmon.cpp, twice: the
// ZENITH_REGISTER_COMPONENT macro AND the ZENITH_TOOLS editor "Add Component"
// registry -- miss the second and the component silently vanishes from the editor
// menu). The order is a WITHIN-ENTITY tiebreak only: it decides the order this
// component's hooks run relative to OTHER components ON THE SAME ENTITY. It does
// NOT order entities against each other -- cross-entity dispatch follows scene-slot
// / entity-creation order, and the DontDestroyOnLoad ZM_MenuRoot (ZM_UI_MenuStack,
// order 112) lives in the PERSISTENT scene and therefore always updates ahead of a
// scene-owned NPC regardless of numbering. DO NOT try to "fix" an input-ordering
// question by renumbering this component; the ZM_ShouldInteract menu-open gate is
// what serialises the interact edge against the menu, not the component order.
//
// Interact() is deliberately the SINGLE dispatch point. S7 adds a Behaviour-Graph
// branch to it in roughly a dozen lines: one more ZM_NPC_RAISE_KIND arm plus a
// graph-run call in the switch below -- no other file has to move.
// ============================================================================

// Which already-shipped ZM_UI_MenuStack raise seam a role talks through. Split out
// of Interact() as a PURE enum + pure mapping function so the role -> seam decision
// is unit-testable headlessly, with no singleton, no scene and no screen raised.
// APPEND-only (units walk it).
enum ZM_NPC_RAISE_KIND : u_int
{
	ZM_NPC_RAISE_NONE = 0u,     // unknown / unmapped role: raises nothing, and LOGS
	ZM_NPC_RAISE_DIALOGUE,      // -> ZM_UI_MenuStack::TryPushDialogue(row lines)
	ZM_NPC_RAISE_SHOP,          // -> ZM_UI_MenuStack::TryOpenShop(row stock)
	ZM_NPC_RAISE_CARE_CENTER,   // -> ZM_UI_MenuStack::TryOpenCareCenterPrompt()

	// NOT a kind -- the walkable bound the totality unit iterates to.
	ZM_NPC_RAISE_COUNT
};

// The role -> seam map. TOTAL: every ZM_NPC_ROLE below ZM_NPC_ROLE_COUNT maps to a
// real seam, and anything outside the enumerated range yields ZM_NPC_RAISE_NONE
// (which Interact() reports as a warning rather than a silent no-op).
ZM_NPC_RAISE_KIND ZM_RaiseKindForRole(ZM_NPC_ROLE eRole);

// A stable short name for a raise kind, for log lines and unit failure messages.
// TOTAL: never returns nullptr, never indexes out of bounds ("UNKNOWN" otherwise).
const char* ZM_NpcRaiseKindName(ZM_NPC_RAISE_KIND eKind);

class ZM_Interactable
{
public:
	static constexpr u_int uSERIALIZATION_VERSION = 1u;

	// Reach BONUS added to fZM_INTERACT_MAX_DISTANCE by the picker, so a physically
	// large interactable (a shop counter, a sign post) can be addressed from its edge.
	// Default zero: an unconfigured component gets exactly the global reach, never more.
	static constexpr float fDEFAULT_RADIUS = 0.0f;
	// Upper bound on that bonus. A mis-authored huge radius would let one NPC swallow
	// every interact press in the town, so the setter clamps rather than trusting.
	static constexpr float fMAX_RADIUS = 8.0f;

	ZM_Interactable() = delete;
	explicit ZM_Interactable(Zenith_Entity& xParentEntity);

	// Component pools relocate their elements (move-construct + destruct the source),
	// so moves must exist; copies are deleted. Every member is a POD or a movable
	// handle, so defaulted moves are correct.
	ZM_Interactable(const ZM_Interactable&) = delete;
	ZM_Interactable& operator=(const ZM_Interactable&) = delete;
	ZM_Interactable(ZM_Interactable&&) noexcept = default;
	ZM_Interactable& operator=(ZM_Interactable&&) noexcept = default;

	void OnStart();

	ZM_NPC_ID GetNpcId() const { return m_eNpcId; }
	// Rejects an out-of-range id by storing ZM_NPC_NONE (which makes the component
	// non-interactable) rather than keeping a stale row. Returns whether it took.
	bool SetNpcId(ZM_NPC_ID eId);

	float GetRadius() const { return m_fRadius; }
	// Clamps into [0, fMAX_RADIUS]; a non-finite value resets to fDEFAULT_RADIUS.
	// Returns whether the requested value was taken verbatim.
	bool SetRadius(float fRadius);

	// The live candidacy answer that feeds ZM_InteractProbe::m_bEnabled: the authored
	// flag AND a real NPC row behind it. The conjunction is deliberate -- an entity
	// that carries the component but was never configured must not absorb the interact
	// press and leave the player standing in front of a mute object.
	bool IsInteractable() const { return m_bInteractable && m_eNpcId < ZM_NPC_COUNT; }
	void SetInteractable(bool bInteractable) { m_bInteractable = bInteractable; }

	// Fire this NPC's role: ONE switch over ZM_RaiseKindForRole(row.m_eRole) onto the
	// three shipped ZM_UI_MenuStack seams. Returns whether a screen was actually
	// raised. A refusal (no menu singleton, a full dialogue queue, a rejected stock
	// list) and an unknown / unconfigured role both LOG a warning naming the NPC and
	// the role -- Shortfalls 1.6: a mis-authored NPC must never be silently mute.
	bool Interact();

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

private:
	// Stored BY VALUE (never a reference): a reference member would dangle on the
	// temporary ctor handle and break the pool's move-construct.
	Zenith_Entity m_xParentEntity;
	ZM_NPC_ID     m_eNpcId        = ZM_NPC_NONE;
	float         m_fRadius       = fDEFAULT_RADIUS;
	// Defaults FALSE. A freshly added, unconfigured component is inert by
	// construction -- SC5 authoring turns each NPC on explicitly.
	bool          m_bInteractable = false;
};
