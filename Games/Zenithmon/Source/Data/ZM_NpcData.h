#pragma once

#include "Zenithmon/Source/Data/ZM_HumanData.h"        // ZM_HUMAN_ID (which townsfolk appearance a row wears)
#include "Zenithmon/Source/Data/ZM_ItemData.h"         // ZM_ITEM_ID (a shopkeep's stock list)
#include "Zenithmon/Source/Data/ZM_StoryFlags.h"       // ZM_StoryGate / ZM_StoryFlagSet (the per-row line gate)
#include "Zenithmon/Source/UI/ZM_UI_DialogueBox.h"     // uMAX_QUEUED_LINES -- the line cap is DERIVED from it

// ============================================================================
// ZM_NpcData (S6 item 3 SC3) -- the authored NPC roster: WHO stands in Dawnmere,
// WHAT they say, and WHICH already-shipped screen they raise when the player
// walks up and presses the interact key.
//
// This is CONTENT ONLY, in the ZM-D-009 compiled-const-table idiom that
// ZM_ItemData / ZM_HumanData / ZM_WorldSpec already use: a save-stable enum, a
// compiled `const` C array of rows, and bounds-asserted free-function accessors.
// There is NO ECS, NO scene, NO UI and NO allocation here -- per-row line and
// stock arrays are static tables referenced by pointer + count, exactly as
// ZM_WorldSpec references its connection / spawn-tag / encounter arrays.
//
// Nothing instantiates these rows yet. SC4 adds the ZM_Interactable component
// whose Interact() is a 3-arm switch over ZM_NPC_ROLE (the three arms map 1:1
// onto the raise seams ZM_UI_MenuStack already ships -- TryPushDialogue,
// TryOpenShop, TryOpenCareCenterPrompt), and SC5 authors the four NPCs into the
// Dawnmere scene. Keeping the content here means a mis-authored row is a BOOT
// FAILURE (Tests/ZM_Tests_NpcData.cpp) rather than a mute NPC discovered by hand.
//
// The ZM_NPC_ID order is save-stable once content ships -- APPEND before
// ZM_NPC_COUNT, never reorder.
// ============================================================================

// What an NPC does when the player interacts with it. Drives SC4's 3-arm
// dispatch switch; every value is exercised by at least one row (a tested
// invariant), so no arm ships without content behind it. APPEND-only.
enum ZM_NPC_ROLE : u_int
{
	ZM_NPC_ROLE_TALKER = 0u,   // -> TryPushDialogue: just says its lines
	ZM_NPC_ROLE_SHOPKEEP,      // -> TryOpenShop with this row's stock
	ZM_NPC_ROLE_CARETAKER,     // -> TryOpenCareCenterPrompt (the heal yes/no)

	ZM_NPC_ROLE_COUNT
};

// Every authored NPC, in roster order. IDs are contiguous 0..ZM_NPC_COUNT-1 and
// the row index equals the id (asserted by ZM_Tests_NpcData).
enum ZM_NPC_ID : u_int
{
	ZM_NPC_VILLAGER,           // Dawnmere flavour talker
	ZM_NPC_TRADE_POST_CLERK,   // the Trade Post counter (the town's shop)
	ZM_NPC_CARETAKER,          // the Care Center's heal prompt
	ZM_NPC_WANDERER,           // a second talker; SC8 gives it a waypoint patrol
	ZM_NPC_ROUTE_WARDEN,       // the S7 story gate: refuses the way on until cleared

	ZM_NPC_COUNT,
	ZM_NPC_NONE = ZM_NPC_COUNT   // "no NPC" sentinel
};

// ---- Row caps (spelled once, HERE) ------------------------------------------

// `inline` so all of these have ONE definition across every TU: the
// ZENITH_ASSERT_* macros bind their operands by const reference, so the units
// odr-use them.
//
// The line cap is DERIVED from the dialogue box's queue capacity rather than
// re-spelled, because the two must never drift: ZM_UI_DialogueBox::QueueLines is
// ALL-OR-NOTHING, so a row that outgrew the queue would not merely lose its last
// line -- it would leave that NPC completely MUTE.
inline constexpr u_int uZM_NPC_MAX_LINES = ZM_UI_DialogueBox::uMAX_QUEUED_LINES;
// Upper bound on a shopkeep's stock list. Generous: the Dawnmere clerk stocks
// six, and the cap exists so a runaway row is caught at boot, not at the counter.
inline constexpr u_int uZM_NPC_MAX_STOCK = 16u;

// One authored NPC row. m_paszLines / m_paeStock point at per-row static tables
// (null when the count is 0); nothing here allocates or owns.
//
// A row's lines are its GREETING only. Text that already belongs to a shipped
// system is NOT duplicated here: the Care Center's question, its two answer
// labels and its post-heal line come from ZM_CareCenterPromptLine /
// ZM_CareCenterYesLabel / ZM_CareCenterNoLabel / ZM_CareCenterHealedLine
// (Source/CareCenter/ZM_CareCenter.h), which SC4 calls at the dispatch site.
//
// m_paeStock is meaningful for ZM_NPC_ROLE_SHOPKEEP only; every other role
// carries a count of 0 (a tested invariant -- stock on a role that never opens a
// shop is dead content).
struct ZM_NpcData
{
	ZM_NPC_ID			m_eId;
	const char*			m_szDisplayName;   // shown in dialogue / debug
	ZM_NPC_ROLE			m_eRole;
	ZM_HUMAN_ID			m_eHuman;          // which townsfolk appearance to wear
	const char* const*	m_paszLines;       // dialogue lines, row-owned (count 0 => null)
	u_int				m_uLineCount;
	const ZM_ITEM_ID*	m_paeStock;        // shop stock (SHOPKEEP only), row-owned
	u_int				m_uStockCount;
	bool				m_bWanders;        // SC8 gives this one a waypoint patrol
	// ---- S7 item 2 SC1: the story gate, APPENDED AT THE END -----------------
	// These three are last on purpose. Every row above is a POSITIONAL aggregate
	// initializer, so inserting a field mid-struct silently shifts each trailing
	// value one column left -- m_bWanders would swallow the gate and the Wanderer
	// would stop patrolling with no compile error to say why.
	//
	// A default gate is unconditional, so an ungated row is simply one that leaves
	// m_eFlag at ZM_STORY_FLAG_NONE and carries no gated array.
	ZM_StoryGate		m_xLineGate;        // NONE == ungated
	const char* const*	m_paszGatedLines;   // spoken while the gate FAILS (null <=> count 0)
	u_int				m_uGatedLineCount;
};

// Table accessors (bounds-asserted). ZM_GetNpcData indexes by ZM_NPC_ID.
const ZM_NpcData&	ZM_GetNpcData(ZM_NPC_ID eId);
u_int				ZM_GetNpcCount();   // == ZM_NPC_COUNT

// PURE. Which line array this row says RIGHT NOW: the ordinary lines when the
// gate passes (or the row authored no gated set), the gated set otherwise.
//
// The COUNT guarantee exists because ZM_UI_DialogueBox::QueueLines is
// ALL-OR-NOTHING: a count above uZM_NPC_MAX_LINES would not truncate the
// conversation, it would make that NPC completely MUTE.
//
// The NULL guarantee is about this selector's OWN contract, not the callee's.
// QueueLines rejects a null array outright, so a (null, non-zero) pair is
// already mute rather than fatal there -- but it is self-contradictory, and
// without the guard the clamp would happily emit (nullptr, min(N, cap)): a count
// that passes every check attached to a pointer that does not. Emitting neither
// makes the guarantee the selector's own rather than a property borrowed from
// whichever screen happens to consume it.
void ZM_SelectNpcLines(const ZM_NpcData& xRow, const ZM_StoryFlagSet& xFlags,
	const char* const*& paszLinesOut, u_int& uCountOut);
