#include "Zenith.h"

// ============================================================================
// ZM_Tests_DialogueBox -- S6 item 2 (SC2) unit tests for ZM_UI_DialogueBox, the
// overworld dialogue box's headless model: the all-or-nothing line queue, the
// typewriter clock, and the four-outcome Confirm() advance, plus the headless
// halves of the presentation / push seams (both short-circuit before they touch a
// scene). Everything here is PURE -- no scene, no graphics, no baked assets -- so
// every fixture is deterministic and hermetic and no RequestSkip is needed.
//
// The reveal rate is 45 glyphs/second and the visible count is
// floor(elapsed * 45) clamped to [0, total] (ZM_UI_BattleHUD::
// ComputeVisibleGlyphCount, the ONE reveal formula). Every timing assertion
// below is computed against that, never guessed at a boundary: the shared long
// line is exactly 45 glyphs, so elapsed 0.5 reveals exactly 22.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "ZenithECS/Zenith_Entity.h"                   // the invalid-handle presentation guard
#include "Zenithmon/Components/ZM_UI_MenuStack.h"      // the host enum + the push seam
#include "Zenithmon/Source/UI/ZM_UI_DialogueBox.h"

namespace
{
	// Exactly 45 glyphs (asserted in every test that depends on the count), so
	// elapsed 0.5s -> floor(0.5 * 45) == 22 revealed, strictly inside (0, 45).
	constexpr const char* szLONG_LINE = "The tall grass rustles as something moves in!";
	constexpr const char* szSECOND_LINE = "Nothing there. Just the wind.";
	constexpr int iLONG_LINE_GLYPHS = 45;

	// Elapsed that reveals PART of the long line, and one that over-shoots it.
	constexpr float fPARTIAL_ELAPSED = 0.5f;    // -> 22 of 45
	constexpr int   iPARTIAL_GLYPHS  = 22;
	constexpr float fFULL_ELAPSED    = 2.0f;    // -> 90 uncapped, clamps to 45

	// The SC8 prompt's two button labels. Deliberately NOT "Yes"/"No": distinct strings
	// prove the box stores what it was armed with rather than a hard-coded pair.
	constexpr const char* szYES_LABEL = "Rest up";
	constexpr const char* szNO_LABEL  = "Not now";
}

// ---- Fresh state ------------------------------------------------------------

ZENITH_TEST(ZM_DialogueBox, Fresh_IsInactiveAndEmpty)
{
	ZM_UI_DialogueBox xBox;
	ZENITH_ASSERT_FALSE(xBox.IsActive(), "a fresh dialogue box is not active");
	ZENITH_ASSERT_EQ(xBox.GetQueuedLineCount(), 0u, "a fresh box has no queued lines");
	ZENITH_ASSERT_EQ(xBox.GetRemainingLineCount(), 0u, "a fresh box has nothing remaining");
	ZENITH_ASSERT_EQ(xBox.GetCurrentLineIndex(), 0u, "a fresh box's line index is 0");
	ZENITH_ASSERT_TRUE(xBox.GetCurrentLine().empty(),
		"an inactive box's current line is the shared empty string (never out of range)");
	ZENITH_ASSERT_EQ(xBox.GetCurrentLineGlyphTotal(), 0, "an empty current line has 0 glyphs");
	ZENITH_ASSERT_TRUE(xBox.IsRevealComplete(),
		"an inactive box is vacuously reveal-complete (nothing is being typed)");
}

// ---- QueueLine --------------------------------------------------------------

ZENITH_TEST(ZM_DialogueBox, QueueLine_RejectsNullAndEmpty)
{
	ZM_UI_DialogueBox xBox;
	ZENITH_ASSERT_FALSE(xBox.QueueLine(nullptr), "a null line is rejected");
	ZENITH_ASSERT_FALSE(xBox.QueueLine(""), "an empty line is rejected (it would show a blank box)");
	ZENITH_ASSERT_EQ(xBox.GetQueuedLineCount(), 0u, "a rejected line never mutates the queue");
	ZENITH_ASSERT_FALSE(xBox.IsActive(), "a rejected line never activates the box");
}

ZENITH_TEST(ZM_DialogueBox, QueueLine_ActivatesAndSetsCurrentLine)
{
	ZM_UI_DialogueBox xBox;
	ZENITH_ASSERT_TRUE(xBox.QueueLine(szLONG_LINE), "queueing a real line succeeds");
	ZENITH_ASSERT_TRUE(xBox.IsActive(), "one queued line makes the box active");
	ZENITH_ASSERT_EQ(xBox.GetQueuedLineCount(), 1u, "one queued line -> count 1");
	ZENITH_ASSERT_EQ(xBox.GetRemainingLineCount(), 1u, "one queued line -> one remaining");
	ZENITH_ASSERT_EQ(xBox.GetCurrentLineIndex(), 0u, "the first line is at index 0");
	ZENITH_ASSERT_STREQ(xBox.GetCurrentLine().c_str(), szLONG_LINE,
		"the current line is the one that was queued");
	ZENITH_ASSERT_EQ(xBox.GetCurrentLineGlyphTotal(), iLONG_LINE_GLYPHS,
		"the shared long-line fixture is exactly 45 glyphs (the timing tests depend on it)");
}

ZENITH_TEST(ZM_DialogueBox, QueueLine_RejectsPastCapacity)
{
	ZM_UI_DialogueBox xBox;
	for (u_int i = 0u; i < ZM_UI_DialogueBox::uMAX_QUEUED_LINES; ++i)
	{
		ZENITH_ASSERT_TRUE(xBox.QueueLine("line"), "queues up to capacity all succeed");
	}
	ZENITH_ASSERT_EQ(xBox.GetQueuedLineCount(), ZM_UI_DialogueBox::uMAX_QUEUED_LINES,
		"the queue fills exactly to capacity");
	ZENITH_ASSERT_FALSE(xBox.QueueLine("one too many"),
		"the 9th line is rejected (capacity is uMAX_QUEUED_LINES == 8)");
	ZENITH_ASSERT_EQ(xBox.GetQueuedLineCount(), ZM_UI_DialogueBox::uMAX_QUEUED_LINES,
		"a rejected line does not grow the queue");
}

// ---- QueueLines (all-or-nothing) -------------------------------------------

ZENITH_TEST(ZM_DialogueBox, QueueLines_NullEntryRejectsWholeBatch)
{
	ZM_UI_DialogueBox xBox;
	const char* aszLines[3] = { "first", nullptr, "third" };
	ZENITH_ASSERT_FALSE(xBox.QueueLines(aszLines, 3u),
		"a null entry anywhere in the batch rejects the whole batch");
	ZENITH_ASSERT_EQ(xBox.GetQueuedLineCount(), 0u,
		"an all-or-nothing rejection queues NOTHING (not even the valid leading entries)");
	ZENITH_ASSERT_FALSE(xBox.IsActive(), "a rejected batch never activates the box");
}

ZENITH_TEST(ZM_DialogueBox, QueueLines_RejectsEmptyCountAndNullArray)
{
	ZM_UI_DialogueBox xBox;
	const char* aszLines[1] = { "only" };
	ZENITH_ASSERT_FALSE(xBox.QueueLines(aszLines, 0u), "a zero-length batch is rejected");
	ZENITH_ASSERT_FALSE(xBox.QueueLines(nullptr, 2u), "a null array is rejected");
	ZENITH_ASSERT_EQ(xBox.GetQueuedLineCount(), 0u, "neither rejection mutates the queue");
}

ZENITH_TEST(ZM_DialogueBox, QueueLines_RejectsOverCapacityBatch)
{
	ZM_UI_DialogueBox xBox;
	const char* aszLines[ZM_UI_DialogueBox::uMAX_QUEUED_LINES + 1u] = {
		"a", "b", "c", "d", "e", "f", "g", "h", "i" };
	ZENITH_ASSERT_FALSE(xBox.QueueLines(aszLines, ZM_UI_DialogueBox::uMAX_QUEUED_LINES + 1u),
		"a batch larger than the remaining capacity is rejected outright");
	ZENITH_ASSERT_EQ(xBox.GetQueuedLineCount(), 0u,
		"an over-capacity batch queues nothing (never a partial conversation)");

	// The same batch minus one entry fits exactly.
	ZENITH_ASSERT_TRUE(xBox.QueueLines(aszLines, ZM_UI_DialogueBox::uMAX_QUEUED_LINES),
		"a batch that exactly fills the queue is accepted");
	ZENITH_ASSERT_EQ(xBox.GetQueuedLineCount(), ZM_UI_DialogueBox::uMAX_QUEUED_LINES,
		"the accepted batch queued every line");
}

// ---- Tick / reveal ----------------------------------------------------------

ZENITH_TEST(ZM_DialogueBox, Tick_RevealsProgressively)
{
	ZM_UI_DialogueBox xBox;
	xBox.QueueLine(szLONG_LINE);
	ZENITH_ASSERT_EQ(xBox.GetCurrentLineGlyphTotal(), iLONG_LINE_GLYPHS,
		"the long-line fixture is 45 glyphs");
	ZENITH_ASSERT_EQ(xBox.GetVisibleGlyphCount(), 0,
		"before any tick nothing is revealed yet");

	xBox.Tick(fPARTIAL_ELAPSED);
	ZENITH_ASSERT_EQ(xBox.GetVisibleGlyphCount(), iPARTIAL_GLYPHS,
		"0.5s at 45 glyphs/sec reveals exactly floor(22.5) == 22 glyphs");
	ZENITH_ASSERT_FALSE(xBox.IsRevealComplete(),
		"22 of 45 glyphs is a reveal still in flight");
}

ZENITH_TEST(ZM_DialogueBox, Tick_IgnoresNegativeDtAndInactiveBox)
{
	// The inactive half is observed THROUGH a line queued afterwards: an inactive box
	// has 0 glyphs, so its visible count is 0 whether or not Tick guarded on
	// IsActive(). Banking the idle second would make the next line appear
	// pre-revealed (1.0s == 45 glyphs, the whole fixture line).
	ZM_UI_DialogueBox xInactive;
	xInactive.Tick(1.0f);
	ZENITH_ASSERT_FALSE(xInactive.IsActive(), "ticking never activates a box");
	xInactive.QueueLine(szLONG_LINE);
	ZENITH_ASSERT_EQ(xInactive.GetVisibleGlyphCount(), 0,
		"a Tick taken while inactive must not have banked elapsed time for the next line");

	ZM_UI_DialogueBox xBox;
	xBox.QueueLine(szLONG_LINE);
	xBox.Tick(fPARTIAL_ELAPSED);
	xBox.Tick(-5.0f);
	ZENITH_ASSERT_EQ(xBox.GetVisibleGlyphCount(), iPARTIAL_GLYPHS,
		"a negative dt is ignored -- it must never rewind the typewriter");
}

ZENITH_TEST(ZM_DialogueBox, Tick_ClampsToTheWholeLine)
{
	ZM_UI_DialogueBox xBox;
	xBox.QueueLine(szLONG_LINE);
	xBox.Tick(fFULL_ELAPSED);
	ZENITH_ASSERT_EQ(xBox.GetVisibleGlyphCount(), iLONG_LINE_GLYPHS,
		"2.0s would reveal 90 glyphs -- it clamps to the line's 45 and never exceeds it");
	ZENITH_ASSERT_TRUE(xBox.IsRevealComplete(),
		"a fully revealed line reports the reveal complete");
}

// ---- Confirm ----------------------------------------------------------------

ZENITH_TEST(ZM_DialogueBox, Confirm_IncompleteRevealCompletesIt)
{
	ZM_UI_DialogueBox xBox;
	xBox.QueueLine(szLONG_LINE);
	xBox.QueueLine(szSECOND_LINE);
	xBox.Tick(fPARTIAL_ELAPSED);

	ZENITH_ASSERT_EQ((u_int)xBox.Confirm(), (u_int)ZM_DIALOGUE_ADVANCE_COMPLETED_REVEAL,
		"a confirm during the typewriter completes the reveal instead of advancing");
	ZENITH_ASSERT_EQ(xBox.GetCurrentLineIndex(), 0u,
		"completing the reveal leaves the line index untouched");
	ZENITH_ASSERT_TRUE(xBox.IsRevealComplete(),
		"the line is fully revealed immediately, with no further ticking");
	ZENITH_ASSERT_EQ(xBox.GetVisibleGlyphCount(), iLONG_LINE_GLYPHS,
		"the whole line is visible after the snap");
}

ZENITH_TEST(ZM_DialogueBox, Confirm_CompleteRevealAdvancesToNextLine)
{
	ZM_UI_DialogueBox xBox;
	xBox.QueueLine(szLONG_LINE);
	xBox.QueueLine(szSECOND_LINE);
	xBox.Tick(fFULL_ELAPSED);

	ZENITH_ASSERT_EQ((u_int)xBox.Confirm(), (u_int)ZM_DIALOGUE_ADVANCE_NEXT_LINE,
		"a confirm on a fully revealed line moves to the next queued line");
	ZENITH_ASSERT_EQ(xBox.GetCurrentLineIndex(), 1u, "the line index advanced to 1");
	ZENITH_ASSERT_STREQ(xBox.GetCurrentLine().c_str(), szSECOND_LINE,
		"the current line is now the second queued line");
	ZENITH_ASSERT_EQ(xBox.GetVisibleGlyphCount(), 0,
		"the clock reset AND the instant flag cleared, so the new line starts un-revealed");
	ZENITH_ASSERT_FALSE(xBox.IsRevealComplete(),
		"the new line types out from scratch");
}

ZENITH_TEST(ZM_DialogueBox, Confirm_LastLineClosesAndResets)
{
	ZM_UI_DialogueBox xBox;
	xBox.QueueLine(szSECOND_LINE);
	xBox.Tick(fFULL_ELAPSED);

	ZENITH_ASSERT_EQ((u_int)xBox.Confirm(), (u_int)ZM_DIALOGUE_ADVANCE_CLOSED,
		"confirming the LAST line closes the dialogue");
	ZENITH_ASSERT_FALSE(xBox.IsActive(), "a closed dialogue is inactive");
	ZENITH_ASSERT_EQ(xBox.GetQueuedLineCount(), 0u, "closing resets the queue");
	ZENITH_ASSERT_EQ(xBox.GetCurrentLineIndex(), 0u, "closing resets the line index");
	ZENITH_ASSERT_TRUE(xBox.GetCurrentLine().empty(), "a closed box has no current line");
}

ZENITH_TEST(ZM_DialogueBox, Confirm_InactiveIsIgnored)
{
	ZM_UI_DialogueBox xBox;
	ZENITH_ASSERT_EQ((u_int)xBox.Confirm(), (u_int)ZM_DIALOGUE_ADVANCE_IGNORED,
		"confirming an inactive box does nothing");
	ZENITH_ASSERT_FALSE(xBox.IsActive(), "an ignored confirm never activates the box");
	ZENITH_ASSERT_EQ(xBox.GetQueuedLineCount(), 0u, "an ignored confirm never mutates the queue");
	ZENITH_ASSERT_EQ(xBox.GetCurrentLineIndex(), 0u, "an ignored confirm never moves the index");
}

ZENITH_TEST(ZM_DialogueBox, Confirm_ShortLineAdvancesRatherThanCompleting)
{
	ZM_UI_DialogueBox xBox;
	xBox.QueueLine("!");                 // one glyph -- revealed within a single tick
	xBox.QueueLine(szSECOND_LINE);
	xBox.Tick(1.0f);                     // 45 glyphs' worth: far past a 1-glyph line

	ZENITH_ASSERT_EQ(xBox.GetCurrentLineGlyphTotal(), 1, "the short line is one glyph");
	ZENITH_ASSERT_TRUE(xBox.IsRevealComplete(), "one tick fully reveals a 1-glyph line");
	ZENITH_ASSERT_EQ((u_int)xBox.Confirm(), (u_int)ZM_DIALOGUE_ADVANCE_NEXT_LINE,
		"an already-complete line advances on the FIRST press (no wasted 'complete reveal' press)");
	ZENITH_ASSERT_EQ(xBox.GetCurrentLineIndex(), 1u, "the box moved to the second line");
}

// ---- Reset / queueing while active -----------------------------------------

ZENITH_TEST(ZM_DialogueBox, Reset_ClearsMidFlightState)
{
	// GENUINELY mid-flight: a part-elapsed clock (0.5s) AND the instant flag set by a
	// confirm during the reveal. (Confirming a COMPLETE reveal would advance the line,
	// which already zeroes both fields -- Reset would then have nothing to clear.)
	ZM_UI_DialogueBox xBox;
	xBox.QueueLine(szLONG_LINE);
	xBox.QueueLine(szSECOND_LINE);
	xBox.Tick(fPARTIAL_ELAPSED);
	xBox.Confirm();                      // -> COMPLETED_REVEAL: instant flag set, clock kept

	xBox.Reset();
	ZENITH_ASSERT_FALSE(xBox.IsActive(), "Reset deactivates the box");
	ZENITH_ASSERT_EQ(xBox.GetQueuedLineCount(), 0u, "Reset clears the queue");
	ZENITH_ASSERT_EQ(xBox.GetRemainingLineCount(), 0u, "Reset leaves nothing remaining");
	ZENITH_ASSERT_EQ(xBox.GetCurrentLineIndex(), 0u, "Reset rewinds the line index");
	ZENITH_ASSERT_TRUE(xBox.GetCurrentLine().empty(), "Reset clears the current line");

	// Observe the two cleared fields through a fresh line: a leaked clock reads 22, a
	// leaked instant flag reads the whole 45.
	ZENITH_ASSERT_TRUE(xBox.QueueLine(szLONG_LINE), "a reset box still accepts lines");
	ZENITH_ASSERT_EQ(xBox.GetVisibleGlyphCount(), 0,
		"Reset cleared both the elapsed clock and the instant flag -- the next line types "
		"from scratch");
}

ZENITH_TEST(ZM_DialogueBox, QueueLine_WhileActiveDoesNotDisturbTheReveal)
{
	ZM_UI_DialogueBox xBox;
	xBox.QueueLine(szLONG_LINE);
	xBox.Tick(fPARTIAL_ELAPSED);

	ZENITH_ASSERT_TRUE(xBox.QueueLine(szSECOND_LINE), "a line may be appended while active");
	ZENITH_ASSERT_EQ(xBox.GetQueuedLineCount(), 2u, "the appended line joined the queue");
	ZENITH_ASSERT_EQ(xBox.GetCurrentLineIndex(), 0u,
		"appending never re-points the box at another line");
	ZENITH_ASSERT_STREQ(xBox.GetCurrentLine().c_str(), szLONG_LINE,
		"the in-flight line is still the current one");
	ZENITH_ASSERT_EQ(xBox.GetVisibleGlyphCount(), iPARTIAL_GLYPHS,
		"appending never resets the in-flight typewriter clock");
	ZENITH_ASSERT_EQ(xBox.GetRemainingLineCount(), 2u,
		"the appended line counts toward what is left to read");
}

// ---- The full two-line "talk" flow -----------------------------------------

ZENITH_TEST(ZM_DialogueBox, TalkFlow_FourConfirmsWalkTwoLines)
{
	ZM_UI_DialogueBox xBox;
	const char* aszLines[2] = { szLONG_LINE, szSECOND_LINE };
	ZENITH_ASSERT_TRUE(xBox.QueueLines(aszLines, 2u), "the two-line batch is accepted");

	// No ticking: every line starts un-revealed, so each takes two presses.
	ZENITH_ASSERT_EQ((u_int)xBox.Confirm(), (u_int)ZM_DIALOGUE_ADVANCE_COMPLETED_REVEAL,
		"press 1 completes line 0's reveal");
	ZENITH_ASSERT_EQ((u_int)xBox.Confirm(), (u_int)ZM_DIALOGUE_ADVANCE_NEXT_LINE,
		"press 2 advances to line 1");
	ZENITH_ASSERT_EQ((u_int)xBox.Confirm(), (u_int)ZM_DIALOGUE_ADVANCE_COMPLETED_REVEAL,
		"press 3 completes line 1's reveal");
	ZENITH_ASSERT_EQ((u_int)xBox.Confirm(), (u_int)ZM_DIALOGUE_ADVANCE_CLOSED,
		"press 4 consumes the last line and closes the box");
	ZENITH_ASSERT_FALSE(xBox.IsActive(), "the talk left the box inactive");
}

ZENITH_TEST(ZM_DialogueBox, TalkFlow_RemainingCountTracksProgress)
{
	ZM_UI_DialogueBox xBox;
	const char* aszLines[2] = { szLONG_LINE, szSECOND_LINE };
	xBox.QueueLines(aszLines, 2u);

	ZENITH_ASSERT_EQ(xBox.GetRemainingLineCount(), 2u, "both lines are still to be read");
	xBox.Confirm();   // completes line 0's reveal -- no line consumed
	ZENITH_ASSERT_EQ(xBox.GetRemainingLineCount(), 2u,
		"completing a reveal does not consume the line");
	xBox.Confirm();   // -> line 1
	ZENITH_ASSERT_EQ(xBox.GetRemainingLineCount(), 1u, "one line left after advancing");
	xBox.Confirm();   // completes line 1's reveal
	ZENITH_ASSERT_EQ(xBox.GetRemainingLineCount(), 1u,
		"the last line is still remaining until it is confirmed away");
	xBox.Confirm();   // -> closed
	ZENITH_ASSERT_EQ(xBox.GetRemainingLineCount(), 0u, "a closed box has nothing remaining");
}

// ---- Presentation / push seams (headless halves) ----------------------------

ZENITH_TEST(ZM_DialogueBox, Present_OnInvalidRootIsABestEffortNoOp)
{
	// Both presentation methods short-circuit on an INVALID root handle before they
	// touch any scene, so the guard itself is headlessly testable: a default-constructed
	// Zenith_Entity resolves no scene data. The contract pinned here is that
	// presentation NEVER mutates the model (the queue and the reveal are the caller's).
	ZM_UI_DialogueBox xBox;
	ZENITH_ASSERT_TRUE(xBox.QueueLine(szLONG_LINE), "the fixture line is queued");
	xBox.Tick(fPARTIAL_ELAPSED);

	Zenith_Entity xNoRoot;
	ZENITH_ASSERT_FALSE(xNoRoot.IsValid(), "a default-constructed entity handle is invalid");
	xBox.Present(xNoRoot);
	xBox.Hide(xNoRoot);

	ZENITH_ASSERT_TRUE(xBox.IsActive(), "presenting to a missing root never closes the box");
	ZENITH_ASSERT_EQ(xBox.GetCurrentLineIndex(), 0u, "presentation never advances the line");
	ZENITH_ASSERT_EQ(xBox.GetVisibleGlyphCount(), iPARTIAL_GLYPHS,
		"presentation never touches the typewriter clock");
	ZENITH_ASSERT_EQ(xBox.GetQueuedLineCount(), 1u, "Hide does not drop the queue (Reset does)");
}

ZENITH_TEST(ZM_DialogueBox, TryPushDialogue_WithoutASingletonIsRejected)
{
	// Unit tests run at boot BEFORE any scene loads, so no ZM_MenuRoot singleton has
	// claimed itself yet -- the static seam must report failure instead of resolving a
	// stale entity id (an NPC that talks with no menu root gets a clean `false`).
	const char* aszLines[2] = { szLONG_LINE, szSECOND_LINE };
	ZENITH_ASSERT_FALSE(ZM_UI_MenuStack::TryPushDialogue(aszLines, 2u),
		"TryPushDialogue reports failure when there is no live ZM_UI_MenuStack singleton");
}

// ---- ArmChoice (S6 item 2 SC8) ---------------------------------------------

ZENITH_TEST(ZM_DialogueBox, ArmChoice_RejectsNullAndEmptyLabels)
{
	ZM_UI_DialogueBox xBox;
	ZENITH_ASSERT_FALSE(xBox.ArmChoice(nullptr, szNO_LABEL), "a null YES label is rejected");
	ZENITH_ASSERT_FALSE(xBox.ArmChoice(szYES_LABEL, nullptr), "a null NO label is rejected");
	ZENITH_ASSERT_FALSE(xBox.ArmChoice("", szNO_LABEL),
		"an empty YES label is rejected (it would draw an unreadable button)");
	ZENITH_ASSERT_FALSE(xBox.ArmChoice(szYES_LABEL, ""), "an empty NO label is rejected");
	ZENITH_ASSERT_FALSE(xBox.IsChoiceArmed(), "no rejected arm ever armed the choice");
	ZENITH_ASSERT_TRUE(xBox.GetYesLabel().empty(), "a rejected arm never stored a YES label");
	ZENITH_ASSERT_TRUE(xBox.GetNoLabel().empty(), "a rejected arm never stored a NO label");
}

ZENITH_TEST(ZM_DialogueBox, ArmChoice_StoresLabelsAndRejectsADoubleArm)
{
	ZM_UI_DialogueBox xBox;
	ZENITH_ASSERT_TRUE(xBox.ArmChoice(szYES_LABEL, szNO_LABEL), "the first arm is accepted");
	ZENITH_ASSERT_TRUE(xBox.IsChoiceArmed(), "the box reports the choice armed");
	ZENITH_ASSERT_STREQ(xBox.GetYesLabel().c_str(), szYES_LABEL, "the YES label was stored");
	ZENITH_ASSERT_STREQ(xBox.GetNoLabel().c_str(), szNO_LABEL, "the NO label was stored");
	ZENITH_ASSERT_EQ((u_int)xBox.GetChoice(), (u_int)ZM_DIALOGUE_CHOICE_NONE,
		"arming a choice does not answer it");

	ZENITH_ASSERT_FALSE(xBox.ArmChoice("Sure", "Nope"),
		"a SECOND arm is rejected -- it would silently replace the question on screen");
	ZENITH_ASSERT_STREQ(xBox.GetYesLabel().c_str(), szYES_LABEL,
		"the rejected re-arm left the original YES label untouched");
	ZENITH_ASSERT_STREQ(xBox.GetNoLabel().c_str(), szNO_LABEL,
		"the rejected re-arm left the original NO label untouched");
}

ZENITH_TEST(ZM_DialogueBox, ArmChoice_IsIndependentOfTheQueue)
{
	// Armed BEFORE anything is queued: legal (a caller arms alongside QueueLines, in
	// either order), and it neither activates the box nor makes it await an answer.
	ZM_UI_DialogueBox xBox;
	ZENITH_ASSERT_TRUE(xBox.ArmChoice(szYES_LABEL, szNO_LABEL), "arming an empty box is legal");
	ZENITH_ASSERT_FALSE(xBox.IsActive(), "arming never activates the box");
	ZENITH_ASSERT_FALSE(xBox.IsAwaitingChoice(),
		"an armed choice with NOTHING queued is not awaiting -- no line has been read");
	ZENITH_ASSERT_TRUE(xBox.QueueLine(szSECOND_LINE), "the line still queues afterwards");
	ZENITH_ASSERT_TRUE(xBox.IsActive(), "the queued line activated the box");
	ZENITH_ASSERT_FALSE(xBox.IsAwaitingChoice(), "the line is still unread");
}

// ---- The unchanged (no choice armed) close path -----------------------------

ZENITH_TEST(ZM_DialogueBox, Confirm_WithNoChoiceArmedStillClosesAndResets)
{
	// The SC2 regression pin: with nothing armed, SC8 must not have changed a thing.
	ZM_UI_DialogueBox xBox;
	xBox.QueueLine(szSECOND_LINE);
	xBox.Tick(fFULL_ELAPSED);

	ZENITH_ASSERT_FALSE(xBox.IsChoiceArmed(), "nothing is armed on this box");
	ZENITH_ASSERT_EQ((u_int)xBox.Confirm(), (u_int)ZM_DIALOGUE_ADVANCE_CLOSED,
		"with NO choice armed the last line still CLOSES (never AWAITING_CHOICE)");
	ZENITH_ASSERT_FALSE(xBox.IsActive(), "a closed dialogue is inactive");
	ZENITH_ASSERT_FALSE(xBox.IsAwaitingChoice(), "an unarmed box never awaits an answer");
	ZENITH_ASSERT_EQ(xBox.GetQueuedLineCount(), 0u, "closing still resets the queue");
	ZENITH_ASSERT_EQ((u_int)xBox.GetChoice(), (u_int)ZM_DIALOGUE_CHOICE_NONE,
		"a plain conversation never produces an answer");
}

// ---- Confirming into the wait ----------------------------------------------

ZENITH_TEST(ZM_DialogueBox, Confirm_LastLineWithAChoiceArmedAwaitsInsteadOfClosing)
{
	ZM_UI_DialogueBox xBox;
	xBox.QueueLine(szSECOND_LINE);
	xBox.ArmChoice(szYES_LABEL, szNO_LABEL);
	xBox.Tick(fFULL_ELAPSED);

	ZENITH_ASSERT_EQ((u_int)xBox.Confirm(), (u_int)ZM_DIALOGUE_ADVANCE_AWAITING_CHOICE,
		"confirming past the LAST line with a choice armed opens the question");
	ZENITH_ASSERT_TRUE(xBox.IsAwaitingChoice(), "the box is now awaiting the answer");
	ZENITH_ASSERT_EQ(xBox.GetQueuedLineCount(), 1u,
		"the wait did NOT reset the box -- the question stays resolvable (and on screen)");
	ZENITH_ASSERT_EQ((u_int)xBox.GetChoice(), (u_int)ZM_DIALOGUE_CHOICE_NONE,
		"reaching the wait answers nothing by itself");
}

ZENITH_TEST(ZM_DialogueBox, Confirm_WhileAwaitingResolvesNothing)
{
	ZM_UI_DialogueBox xBox;
	xBox.QueueLine(szSECOND_LINE);
	xBox.ArmChoice(szYES_LABEL, szNO_LABEL);
	xBox.Tick(fFULL_ELAPSED);
	xBox.Confirm();   // -> AWAITING_CHOICE

	ZENITH_ASSERT_EQ((u_int)xBox.Confirm(), (u_int)ZM_DIALOGUE_ADVANCE_AWAITING_CHOICE,
		"a further confirm re-reports the wait");
	ZENITH_ASSERT_TRUE(xBox.IsAwaitingChoice(), "...and the box is still waiting");
	ZENITH_ASSERT_EQ((u_int)xBox.GetChoice(), (u_int)ZM_DIALOGUE_CHOICE_NONE,
		"a bare confirm must never PICK for the player -- the answer is by element NAME");
}

ZENITH_TEST(ZM_DialogueBox, IsAwaitingChoice_IsFalseWhileLinesRemainUnread)
{
	ZM_UI_DialogueBox xBox;
	const char* aszLines[2] = { szLONG_LINE, szSECOND_LINE };
	xBox.QueueLines(aszLines, 2u);
	xBox.ArmChoice(szYES_LABEL, szNO_LABEL);

	ZENITH_ASSERT_FALSE(xBox.IsAwaitingChoice(), "nothing has been read yet");
	xBox.Tick(fFULL_ELAPSED);
	ZENITH_ASSERT_EQ((u_int)xBox.Confirm(), (u_int)ZM_DIALOGUE_ADVANCE_NEXT_LINE,
		"the first confirm advances to line 1 as usual -- the armed choice changes nothing here");
	ZENITH_ASSERT_FALSE(xBox.IsAwaitingChoice(), "line 1 is still unread");
	xBox.Tick(fFULL_ELAPSED);
	ZENITH_ASSERT_EQ((u_int)xBox.Confirm(), (u_int)ZM_DIALOGUE_ADVANCE_AWAITING_CHOICE,
		"only the LAST line opens the question");
	ZENITH_ASSERT_TRUE(xBox.IsAwaitingChoice(), "every line is read -- the box awaits the answer");
}

// ---- ResolveChoice (by element NAME) ---------------------------------------

namespace
{
	// Read the prompt to its end so the box is awaiting an answer.
	void ZM_ArmAndReachTheChoice(ZM_UI_DialogueBox& xBox)
	{
		xBox.QueueLine(szSECOND_LINE);
		xBox.ArmChoice(szYES_LABEL, szNO_LABEL);
		xBox.Tick(fFULL_ELAPSED);
		xBox.Confirm();
	}
}

ZENITH_TEST(ZM_DialogueBox, ResolveChoice_YesNameAnswersYesAndClosesTheBox)
{
	ZM_UI_DialogueBox xBox;
	ZM_ArmAndReachTheChoice(xBox);
	ZENITH_ASSERT_TRUE(xBox.IsAwaitingChoice(), "the fixture reached the question");

	ZENITH_ASSERT_EQ((u_int)xBox.ResolveChoice(ZM_UI_DialogueBox::szYES_NAME),
		(u_int)ZM_DIALOGUE_CHOICE_YES, "the YES element name answers YES");
	ZENITH_ASSERT_EQ((u_int)xBox.GetChoice(), (u_int)ZM_DIALOGUE_CHOICE_YES,
		"the answer SURVIVES the reset the resolve performs");
	ZENITH_ASSERT_FALSE(xBox.IsActive(), "an answered box is inactive");
	ZENITH_ASSERT_FALSE(xBox.IsAwaitingChoice(), "...and is no longer awaiting");
	ZENITH_ASSERT_FALSE(xBox.IsChoiceArmed(), "...and no longer holds an armed choice");
	ZENITH_ASSERT_EQ(xBox.GetQueuedLineCount(), 0u, "the resolve dropped the queue");
}

ZENITH_TEST(ZM_DialogueBox, ResolveChoice_NoNameAnswersNo)
{
	ZM_UI_DialogueBox xBox;
	ZM_ArmAndReachTheChoice(xBox);

	ZENITH_ASSERT_EQ((u_int)xBox.ResolveChoice(ZM_UI_DialogueBox::szNO_NAME),
		(u_int)ZM_DIALOGUE_CHOICE_NO, "the NO element name answers NO");
	ZENITH_ASSERT_EQ((u_int)xBox.GetChoice(), (u_int)ZM_DIALOGUE_CHOICE_NO,
		"the stored answer is NO");
	ZENITH_ASSERT_FALSE(xBox.IsAwaitingChoice(), "an answered box stops awaiting");
}

ZENITH_TEST(ZM_DialogueBox, ResolveChoice_UnknownAndNullNamesLeaveItAwaiting)
{
	ZM_UI_DialogueBox xBox;
	ZM_ArmAndReachTheChoice(xBox);

	ZENITH_ASSERT_EQ((u_int)xBox.ResolveChoice(nullptr), (u_int)ZM_DIALOGUE_CHOICE_NONE,
		"a null focused name answers nothing");
	ZENITH_ASSERT_EQ((u_int)xBox.ResolveChoice("Menu_RootExit"), (u_int)ZM_DIALOGUE_CHOICE_NONE,
		"a FOREIGN element holding the focus answers nothing");
	ZENITH_ASSERT_TRUE(xBox.IsAwaitingChoice(),
		"neither miss dismissed the question -- the box is still waiting for a real answer");
	ZENITH_ASSERT_EQ((u_int)xBox.GetChoice(), (u_int)ZM_DIALOGUE_CHOICE_NONE,
		"...and nothing was stored");
}

ZENITH_TEST(ZM_DialogueBox, ResolveChoice_IsInertWhenNothingIsAwaiting)
{
	// A stray confirm on a plain conversation must never fabricate an answer.
	ZM_UI_DialogueBox xBox;
	xBox.QueueLine(szSECOND_LINE);
	ZENITH_ASSERT_EQ((u_int)xBox.ResolveChoice(ZM_UI_DialogueBox::szYES_NAME),
		(u_int)ZM_DIALOGUE_CHOICE_NONE, "resolving with no choice armed answers nothing");
	ZENITH_ASSERT_TRUE(xBox.IsActive(), "...and never closed the conversation");
	ZENITH_ASSERT_EQ((u_int)xBox.GetChoice(), (u_int)ZM_DIALOGUE_CHOICE_NONE,
		"...and stored nothing");
}

// ---- CancelChoice -----------------------------------------------------------

ZENITH_TEST(ZM_DialogueBox, CancelChoice_ResolvesNoWhileAwaiting)
{
	ZM_UI_DialogueBox xBox;
	ZM_ArmAndReachTheChoice(xBox);

	ZENITH_ASSERT_EQ((u_int)xBox.CancelChoice(), (u_int)ZM_DIALOGUE_CHOICE_NO,
		"cancelling an open question answers NO -- a prompt is never a dead end");
	ZENITH_ASSERT_EQ((u_int)xBox.GetChoice(), (u_int)ZM_DIALOGUE_CHOICE_NO,
		"the NO answer is stored like any other");
	ZENITH_ASSERT_FALSE(xBox.IsAwaitingChoice(), "the box stopped awaiting");
	ZENITH_ASSERT_FALSE(xBox.IsActive(), "...and is inactive");
}

ZENITH_TEST(ZM_DialogueBox, CancelChoice_IsInertWhileLinesAreStillBeingRead)
{
	// The LINES stay modal: cancel must not skip them, so it answers nothing until the
	// question is actually up.
	ZM_UI_DialogueBox xBox;
	xBox.QueueLine(szLONG_LINE);
	xBox.ArmChoice(szYES_LABEL, szNO_LABEL);

	ZENITH_ASSERT_EQ((u_int)xBox.CancelChoice(), (u_int)ZM_DIALOGUE_CHOICE_NONE,
		"cancel answers nothing while a line is still unread");
	ZENITH_ASSERT_TRUE(xBox.IsActive(), "...and never dismissed the line");
	ZENITH_ASSERT_TRUE(xBox.IsChoiceArmed(), "...and left the choice armed");
	ZENITH_ASSERT_EQ((u_int)xBox.GetChoice(), (u_int)ZM_DIALOGUE_CHOICE_NONE,
		"...and stored no answer");
}

// ---- Reset clears the whole prompt -----------------------------------------

ZENITH_TEST(ZM_DialogueBox, Reset_ClearsAStillArmedChoiceAndItsLabels)
{
	// Reset is exercised on a box that is STILL ARMED. The sibling test below resolves
	// first, but ResolveChoice already performs a full Reset internally -- so asserting
	// "armed is false / the labels are empty" AFTER a resolve would pass whether or not
	// the Reset under test did anything at all. This fixture pins the pre-state first.
	ZM_UI_DialogueBox xBox;
	ZENITH_ASSERT_TRUE(xBox.QueueLine(szSECOND_LINE), "the fixture queues a prompt line");
	ZENITH_ASSERT_TRUE(xBox.ArmChoice(szYES_LABEL, szNO_LABEL), "the fixture arms the choice");
	ZENITH_ASSERT_TRUE(xBox.IsChoiceArmed(), "PRE-STATE: the choice really is armed");
	ZENITH_ASSERT_FALSE(xBox.GetYesLabel().empty(), "PRE-STATE: the YES label is set");
	ZENITH_ASSERT_FALSE(xBox.GetNoLabel().empty(), "PRE-STATE: the NO label is set");
	ZENITH_ASSERT_EQ(xBox.GetQueuedLineCount(), 1u, "PRE-STATE: the line is queued");

	xBox.Reset();
	ZENITH_ASSERT_FALSE(xBox.IsChoiceArmed(), "Reset clears the armed choice");
	ZENITH_ASSERT_FALSE(xBox.IsAwaitingChoice(), "Reset leaves nothing awaiting");
	ZENITH_ASSERT_TRUE(xBox.GetYesLabel().empty(), "Reset clears the YES label");
	ZENITH_ASSERT_TRUE(xBox.GetNoLabel().empty(), "Reset clears the NO label");
	ZENITH_ASSERT_EQ(xBox.GetQueuedLineCount(), 0u, "Reset drops the queue too");
	ZENITH_ASSERT_TRUE(xBox.ArmChoice("Again", "Never"), "a reset box can be armed afresh");
}

ZENITH_TEST(ZM_DialogueBox, Reset_ClearsTheStoredAnswerFromAResolvedPrompt)
{
	// The ANSWER is the one piece of choice state that deliberately SURVIVES a resolve
	// (ResolveChoice resets everything else and writes it back), so it is the only thing
	// this fixture can meaningfully assert Reset clears.
	ZM_UI_DialogueBox xBox;
	ZM_ArmAndReachTheChoice(xBox);
	ZENITH_ASSERT_EQ((u_int)xBox.ResolveChoice(ZM_UI_DialogueBox::szYES_NAME),
		(u_int)ZM_DIALOGUE_CHOICE_YES, "the fixture answered YES");
	ZENITH_ASSERT_EQ((u_int)xBox.GetChoice(), (u_int)ZM_DIALOGUE_CHOICE_YES,
		"PRE-STATE: the answer survived the resolve, so Reset has something to clear");

	xBox.Reset();
	ZENITH_ASSERT_EQ((u_int)xBox.GetChoice(), (u_int)ZM_DIALOGUE_CHOICE_NONE,
		"Reset clears the STORED ANSWER -- a reused box must never report the last one");
}

// ---- Host enum guard --------------------------------------------------------

ZENITH_TEST(ZM_DialogueBox, HostScreenEnum_DialogueIsAppendedAndNotARootEntry)
{
	// Save-stable append: DIALOGUE was added AFTER DEX, so the four pre-existing
	// screens keep their SC1 values. Pinning the literal catches an accidental
	// reorder that would silently re-map serialized screen ids.
	ZENITH_ASSERT_EQ((u_int)ZM_MENU_SCREEN_DIALOGUE, 5u,
		"DIALOGUE is the 6th enumerator (NONE/ROOT/PARTY/BAG/DEX/DIALOGUE) -- append-only");
	ZENITH_ASSERT_TRUE((u_int)ZM_MENU_SCREEN_DIALOGUE < (u_int)ZM_MENU_SCREEN_COUNT,
		"DIALOGUE is within the enumerated range");

	// The SC8 outcome is APPENDED: the four SC2 values keep their numbering, so a stack
	// or a test comparing against CLOSED still means CLOSED.
	ZENITH_ASSERT_EQ((u_int)ZM_DIALOGUE_ADVANCE_CLOSED, 3u,
		"CLOSED is still the 4th advance outcome");
	ZENITH_ASSERT_EQ((u_int)ZM_DIALOGUE_ADVANCE_AWAITING_CHOICE, 4u,
		"AWAITING_CHOICE was APPENDED after CLOSED, never inserted");

	for (u_int uAction = (u_int)ZM_MENU_ACTION_NONE; uAction <= (u_int)ZM_MENU_ACTION_CLOSE; ++uAction)
	{
		ZENITH_ASSERT_NE(
			(u_int)ZM_UI_MenuStack::RootActionToScreen((ZM_MENU_ACTION)uAction),
			(u_int)ZM_MENU_SCREEN_DIALOGUE,
			"no ROOT entry opens the dialogue -- it is raised only by PushDialogueLines");
	}
}
