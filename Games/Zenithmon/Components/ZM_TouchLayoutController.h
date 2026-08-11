#pragma once

#include "ZenithECS/Zenith_Entity.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"   // ZM_MENU_SCREEN (by value in the pure resolver)

class Zenith_DataStream;
class Zenith_UIComponent;

// ============================================================================
// Zenithmon input program WP3b (B11) -- ZM_TouchLayoutController: the game-side
// HUD context machine for the on-screen controls.
//
// A new ECS component (order 114) living on the persistent ZM_TouchRoot entity
// (authored in FrontEnd, DontDestroyOnLoad, with its OWN Zenith_UIComponent
// carrying the four B9 widgets), mirroring ZM_MenuRoot + ZM_UI_MenuStack.
//
// It owns exactly one decision: which ACTION each on-screen control targets, and
// whether it is on screen at all, for the UI state the game is in right now.
//
//   OVERWORLD -- stick = MOVE, A = INTERACT, B = RUN (held), MENU top-right
//   DIALOGUE  -- A = CONFIRM, B = CANCEL, stick hidden, MENU hidden
//   MENU      -- A = CONFIRM, B = CANCEL, stick hidden, MENU hidden
//   BATTLE    -- as MENU (a battle is a menu with monsters in it)
//   TITLE     -- A = CONFIRM only; everything else hidden
//
// ★ IT RETARGETS, IT NEVER RE-CREATES. The battle scene loads ADDITIVELY over
// the overworld and the HUD is persistent, so a context change is four
// SetAction / SetVisible calls on the SAME widgets. Destroying and rebuilding a
// widget mid-gesture would strand a claimed pointer with nothing to release it;
// SetAction is defined for exactly that case (B9: the OLD action gets a release,
// the claim is KEPT, the widget is DISARMED until a fresh DOWN).
//
// ★ IT APPLIES IN OnUpdate, NOT OnStart. OnStart runs during the tools AUTHORING
// boot too, and this component must never write a widget property before
// AddStep_SaveScene reads it back -- that is how a committed scene starts
// churning (ZM-D-179 / ZM-D-183). Authored values come from the automation
// steps in Zenithmon.cpp and from nowhere else.
//
// The context resolver and the per-context layout table are PURE statics with no
// ECS and no graphics, so the boot units pin the contract itself rather than a
// presentation of it.
// ============================================================================

// The HUD contexts, in B11 order. Save-stable is not a concern (nothing here is
// serialized), but the boot units index this by ordinal.
enum ZM_TOUCH_CONTEXT : u_int
{
	ZM_TOUCH_CONTEXT_OVERWORLD = 0u,
	ZM_TOUCH_CONTEXT_DIALOGUE,
	ZM_TOUCH_CONTEXT_MENU,
	ZM_TOUCH_CONTEXT_BATTLE,
	ZM_TOUCH_CONTEXT_TITLE,

	ZM_TOUCH_CONTEXT_COUNT
};

// What one context does to the four controls. A null action NAME means "this
// control has no job here" -- the widget is hidden AND left pointing at nothing,
// so a stale target cannot fire from behind a hidden control.
struct ZM_TouchLayout
{
	const char* m_szStickAction  = nullptr;
	const char* m_szButtonAAction = nullptr;
	const char* m_szButtonBAction = nullptr;
	const char* m_szButtonMenuAction = nullptr;
};

class ZM_TouchLayoutController
{
public:
	static constexpr u_int uSERIALIZATION_VERSION = 1u;

	// Authored element names -- the SINGLE SOURCE of the Zenithmon.cpp authoring
	// contract and of this component's runtime re-resolution. Never cache the
	// pointers across frames: the canvas owns them and the ECS pool relocates us.
	static constexpr const char* szSTICK_NAME       = "Touch_Stick";
	static constexpr const char* szBUTTON_A_NAME    = "Touch_ButtonA";
	static constexpr const char* szBUTTON_B_NAME    = "Touch_ButtonB";
	static constexpr const char* szBUTTON_MENU_NAME = "Touch_ButtonMenu";

	// Sort band: ABOVE the menu panels / buttons (9000 / 9001) so a control is
	// never buried under the dialogue box it is there to advance, and BELOW the
	// two fade overlays (WarpFade 10000 / BattleFade 10001) so a fade still
	// covers everything.
	static constexpr int iTOUCH_CONTROL_SORT_ORDER = 9500;

	ZM_TouchLayoutController() = delete;
	explicit ZM_TouchLayoutController(Zenith_Entity& xParentEntity);

	// Move-CONSTRUCTIBLE only (the ECS pool move-constructs on Grow /
	// swap-and-pop / DontDestroyOnLoad). Every member is a POD or a
	// Zenith_Entity, so noexcept-default is well-formed. Copy deleted.
	ZM_TouchLayoutController(const ZM_TouchLayoutController&) = delete;
	ZM_TouchLayoutController& operator=(const ZM_TouchLayoutController&) = delete;
	ZM_TouchLayoutController(ZM_TouchLayoutController&&) noexcept = default;
	ZM_TouchLayoutController& operator=(ZM_TouchLayoutController&&) noexcept = default;

	void OnStart();
	void OnUpdate(float fDeltaSeconds);
	void OnDestroy();

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);
#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

	// ---- PURE decision surface (no scene / graphics -- unit-tested verbatim) ----

	// The B11 context, from the three facts that decide it. BATTLE outranks
	// everything (the transition owns the screen from enter to exit); a raised
	// DIALOGUE outranks the menu it stacked on; TITLE is identified by the top
	// screen, not by the scene, so it cannot disagree with ZM_UI_MenuStack.
	static ZM_TOUCH_CONTEXT ResolveContext(bool bBattleActive, bool bMenuOpen,
		ZM_MENU_SCREEN eTopScreen);
	// The four action targets for a context. An out-of-range context folds to
	// OVERWORLD, which is the only layout that can never strand the player.
	static ZM_TouchLayout LayoutForContext(ZM_TOUCH_CONTEXT eContext);
	static const char* ContextName(ZM_TOUCH_CONTEXT eContext);

	// ---- Live observation (the tests read these) ----
	ZM_TOUCH_CONTEXT GetContext() const { return m_eContext; }
	bool HasAppliedOnce() const { return m_bAppliedOnce; }

	// The context the live singleton is in, or OVERWORLD when none resolves.
	static ZM_TOUCH_CONTEXT GetLiveContext();
	static bool TryGetUniqueSingletonEntityID(Zenith_EntityID& xEntityIDOut);
	// Windowed-test teardown: forget the applied context so the next test's first
	// OnUpdate re-applies from scratch instead of trusting a stale latch.
	static void ResetRuntimeStateForTests();

private:
	// Re-resolve the four authored widgets by name and push eContext's layout
	// into them. Safe to call every frame; only called when the context MOVED.
	void ApplyLayout(ZM_TOUCH_CONTEXT eContext);
	Zenith_UIComponent* ResolveUI() const;

	Zenith_Entity    m_xParentEntity;
	ZM_TOUCH_CONTEXT m_eContext     = ZM_TOUCH_CONTEXT_OVERWORLD;
	bool             m_bAppliedOnce = false;
};
