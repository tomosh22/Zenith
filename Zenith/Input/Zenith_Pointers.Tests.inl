#include "UnitTests/Zenith_UnitTests.h"
#include "Input/Zenith_Pointers.h"
#include "Input/Zenith_Input.h"
#include "Maths/Zenith_Maths.h"

// ============================================================================
// WP1: the pointer table (B7).
//
// Everything here drives LOCAL Zenith_Pointers instances through the
// window-free cores (BeginFrame / ApplyEvent / ApplyMouseState /
// ConsumeStagedTail), so no window, engine or swapchain is required and the
// global table the engine owns is never touched (B13).
// ============================================================================

namespace
{
	Zenith_InputEvent Zenith_MakePointerEvent(Zenith_InputEventType eType, int32_t iPointerId,
		float fX, float fY, double fTimestamp = 0.0)
	{
		Zenith_InputEvent xEvent;
		xEvent.m_eType      = eType;
		xEvent.m_iCode      = iPointerId;
		xEvent.m_fX         = fX;
		xEvent.m_fY         = fY;
		xEvent.m_fAnchorX   = fX;
		xEvent.m_fAnchorY   = fY;
		xEvent.m_fTimestamp = fTimestamp;
		return xEvent;
	}

	// One frame of "nothing happened", so a test can watch a slot retire.
	void Zenith_StepEmptyPointerFrame(Zenith_Pointers& xPointers)
	{
		xPointers.BeginFrame(1.0f);
	}
}

// A pointer's life is three phases and they must be distinguishable: the DOWN
// edge fires only on the frame it arrived, MOVE carries position without
// re-raising it, and the UP edge leaves the slot readable for exactly that
// frame before it recycles.
ZENITH_TEST(Pointers, DownMoveUpPhases)
{
	Zenith_Pointers xPointers;

	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_DOWN, 7, 100.0f, 200.0f, 1.0));

	const Zenith_PointerHandle xHandle = xPointers.FindByPlatformId(7);
	ZENITH_ASSERT_TRUE(xHandle.IsValid(), "DOWN allocates a slot for the platform id");

	const Zenith_Pointer* pxPointer = xPointers.Resolve(xHandle);
	ZENITH_ASSERT_NOT_NULL(pxPointer, "handle resolves on the down frame");
	ZENITH_ASSERT_TRUE(pxPointer->m_bDownThisFrame, "down edge raised on the arrival frame");
	ZENITH_ASSERT_TRUE(pxPointer->IsDown(), "pointer is down");
	ZENITH_ASSERT_EQ_FLOAT(pxPointer->m_xPosition.x, 100.0f, 1e-4f, "down position x");
	ZENITH_ASSERT_EQ_FLOAT(pxPointer->m_xStartPosition.y, 200.0f, 1e-4f, "start position y");

	// MOVE frame: position advances, the down edge does NOT persist.
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_MOVE, 7, 140.0f, 200.0f, 1.1));
	pxPointer = xPointers.Resolve(xHandle);
	ZENITH_ASSERT_NOT_NULL(pxPointer, "handle still live across a move");
	ZENITH_ASSERT_FALSE(pxPointer->m_bDownThisFrame, "down edge lives exactly one frame");
	ZENITH_ASSERT_EQ_FLOAT(pxPointer->m_xPosition.x, 140.0f, 1e-4f, "move advanced the position");
	ZENITH_ASSERT_EQ_FLOAT(pxPointer->m_fMaxExcursion, 40.0f, 1e-4f, "excursion measured from the start position");

	// UP frame: the edge is readable and the slot is STILL resolvable, so a
	// claimer running later in the same frame can see the gesture end.
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_UP, 7, 140.0f, 200.0f, 1.2));
	pxPointer = xPointers.Resolve(xHandle);
	ZENITH_ASSERT_NOT_NULL(pxPointer, "slot survives its terminal frame");
	ZENITH_ASSERT_TRUE(pxPointer->m_bUpThisFrame, "up edge raised");
	ZENITH_ASSERT_FALSE(pxPointer->IsDown(), "no longer down on the terminal frame");
	ZENITH_ASSERT_EQ(xPointers.GetActivePointerCount(), 0u, "a terminal pointer is not counted as active");

	// Next frame the slot recycles and the old handle is stale.
	Zenith_StepEmptyPointerFrame(xPointers);
	ZENITH_ASSERT_NULL(xPointers.Resolve(xHandle), "handle goes stale once the slot recycles");
	ZENITH_ASSERT_FALSE(xPointers.FindByPlatformId(7).IsValid(), "platform id is free again");
}

// Two fingers are two independent records: ending one must not disturb the
// other's position, excursion or slot.
ZENITH_TEST(Pointers, MultiPointerIndependence)
{
	Zenith_Pointers xPointers;

	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_DOWN, 1, 10.0f, 10.0f, 0.0));
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_DOWN, 2, 500.0f, 400.0f, 0.0));
	ZENITH_ASSERT_EQ(xPointers.GetActivePointerCount(), 2u, "both fingers occupy slots");

	const Zenith_PointerHandle xFirst  = xPointers.FindByPlatformId(1);
	const Zenith_PointerHandle xSecond = xPointers.FindByPlatformId(2);
	ZENITH_ASSERT_NE(xFirst.m_uSlot, xSecond.m_uSlot, "distinct platform ids take distinct slots");

	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_MOVE, 2, 560.0f, 400.0f, 0.1));
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_UP,   1, 10.0f, 10.0f, 0.1));

	const Zenith_Pointer* pxSecond = xPointers.Resolve(xSecond);
	ZENITH_ASSERT_NOT_NULL(pxSecond, "the surviving finger is untouched by the other's UP");
	ZENITH_ASSERT_TRUE(pxSecond->IsDown(), "surviving finger still down");
	ZENITH_ASSERT_EQ_FLOAT(pxSecond->m_xPosition.x, 560.0f, 1e-4f, "surviving finger kept its own position");
	ZENITH_ASSERT_EQ_FLOAT(pxSecond->m_fMaxExcursion, 60.0f, 1e-4f, "surviving finger kept its own excursion");

	const Zenith_Pointer* pxFirst = xPointers.Resolve(xFirst);
	ZENITH_ASSERT_NOT_NULL(pxFirst, "the lifted finger is still readable on its terminal frame");
	ZENITH_ASSERT_TRUE(pxFirst->m_bUpThisFrame, "lifted finger raised its own up edge");
	ZENITH_ASSERT_FALSE(pxSecond->m_bUpThisFrame, "the up edge did not leak across slots");

	// The lifted slot recycles; the other keeps running with the SAME handle.
	Zenith_StepEmptyPointerFrame(xPointers);
	ZENITH_ASSERT_NULL(xPointers.Resolve(xFirst), "lifted finger's handle is stale");
	ZENITH_ASSERT_NOT_NULL(xPointers.Resolve(xSecond), "surviving finger's handle is unaffected by the recycle");
}

// A lifecycle barrier (focus loss / pause) must not leave a phantom finger: the
// pointer is cancelled with an edge a consumer can see, claims are dropped, and
// nothing new is accepted until the device layer re-arms.
ZENITH_TEST(Pointers, CancelClearsActiveAndRaisesEdge)
{
	Zenith_Pointers xPointers;

	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_DOWN, 3, 50.0f, 50.0f, 0.0));
	const Zenith_PointerHandle xHandle = xPointers.FindByPlatformId(3);
	ZENITH_ASSERT_TRUE(xPointers.ClaimPointer(xHandle, 0x1234u), "claim taken before the barrier");

	// An explicit CANCEL for the pointer alone.
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_CANCEL, 3, 50.0f, 50.0f, 0.1));
	const Zenith_Pointer* pxPointer = xPointers.Resolve(xHandle);
	ZENITH_ASSERT_NOT_NULL(pxPointer, "cancelled pointer is readable on its terminal frame");
	ZENITH_ASSERT_TRUE(pxPointer->m_bCancelledThisFrame, "cancel edge raised");
	ZENITH_ASSERT_FALSE(pxPointer->IsDown(), "a cancelled pointer is not down");
	ZENITH_ASSERT_FALSE(pxPointer->m_bTapThisFrame, "a cancel is never a tap");
	ZENITH_ASSERT_TRUE(pxPointer->IsClaimed(), "the claim survives the terminal frame so its owner can react");

	Zenith_StepEmptyPointerFrame(xPointers);
	ZENITH_ASSERT_NULL(xPointers.Resolve(xHandle), "cancelled slot recycles like any other");

	// Now the same through a LIFECYCLE_RESET barrier, which also disarms.
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_DOWN, 4, 20.0f, 20.0f, 0.0));
	const Zenith_PointerHandle xSecond = xPointers.FindByPlatformId(4);

	Zenith_InputEvent xBarrier;
	xBarrier.m_eType = INPUT_EVENT_LIFECYCLE_RESET;
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(xBarrier);
	const Zenith_Pointer* pxSecond = xPointers.Resolve(xSecond);
	ZENITH_ASSERT_NOT_NULL(pxSecond, "barrier leaves the record readable for one frame");
	ZENITH_ASSERT_TRUE(pxSecond->m_bCancelledThisFrame, "barrier raises the cancel edge");
	ZENITH_ASSERT_FALSE(xPointers.IsArmed(), "barrier disarms the table");

	// Disarmed: a fresh finger is ignored rather than half-tracked.
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_DOWN, 5, 30.0f, 30.0f, 0.0));
	ZENITH_ASSERT_FALSE(xPointers.FindByPlatformId(5).IsValid(), "no pointers accepted while disarmed");
}

// A claim is a declaration that some widget consumed the press, so the gameplay
// layer must not ALSO see it as a tap.
ZENITH_TEST(Pointers, ClaimBlocksTap)
{
	Zenith_Pointers xPointers;

	// Unclaimed: short + still => tap.
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_DOWN, 1, 10.0f, 10.0f, 5.00));
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_UP, 1, 11.0f, 10.0f, 5.10));
	ZENITH_ASSERT_TRUE(xPointers.WasTapThisFrame(), "an unclaimed short still press is a tap");

	Zenith_Maths::Vector2 xTapPos(0.0f, 0.0f);
	ZENITH_ASSERT_TRUE(xPointers.GetTapPosition(xTapPos), "tap position reported");
	ZENITH_ASSERT_EQ_FLOAT(xTapPos.x, 10.0f, 1e-4f, "tap reports where the finger went DOWN");

	Zenith_StepEmptyPointerFrame(xPointers);

	// Claimed: identical gesture, no tap.
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_DOWN, 1, 10.0f, 10.0f, 6.00));
	const Zenith_PointerHandle xHandle = xPointers.FindByPlatformId(1);
	ZENITH_ASSERT_TRUE(xPointers.ClaimPointer(xHandle, 0xABCDu), "widget claims the press");

	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_UP, 1, 11.0f, 10.0f, 6.10));
	ZENITH_ASSERT_FALSE(xPointers.WasTapThisFrame(), "a claimed press never becomes a tap");

	// A second owner cannot steal a live claim: FIRST claim wins.
	Zenith_StepEmptyPointerFrame(xPointers);
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_DOWN, 1, 10.0f, 10.0f, 7.00));
	const Zenith_PointerHandle xThird = xPointers.FindByPlatformId(1);
	ZENITH_ASSERT_TRUE(xPointers.ClaimPointer(xThird, 0x1111u), "first claimer wins");
	ZENITH_ASSERT_FALSE(xPointers.ClaimPointer(xThird, 0x2222u), "second claimer is rejected");
	ZENITH_ASSERT_EQ(xPointers.GetClaimOwner(xThird), static_cast<Zenith_PointerOwner>(0x1111u), "owner unchanged");
	ZENITH_ASSERT_TRUE(xPointers.ClaimPointer(xThird, 0x1111u), "re-claiming by the same owner is idempotent");
}

// The generation is the whole point of the handle: a widget that kept LAST
// gesture's handle must not be able to free THIS gesture's claim.
ZENITH_TEST(Pointers, StaleGenerationClaimReleaseRejected)
{
	Zenith_Pointers xPointers;

	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_DOWN, 1, 10.0f, 10.0f, 0.0));
	const Zenith_PointerHandle xOld = xPointers.FindByPlatformId(1);

	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_UP, 1, 10.0f, 10.0f, 0.1));
	Zenith_StepEmptyPointerFrame(xPointers);   // slot recycles, generation++

	// A new gesture lands in the same slot with a NEW generation.
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_DOWN, 1, 10.0f, 10.0f, 1.0));
	const Zenith_PointerHandle xNew = xPointers.FindByPlatformId(1);
	ZENITH_ASSERT_EQ(xNew.m_uSlot, xOld.m_uSlot, "the slot was reused");
	ZENITH_ASSERT_NE(xNew.m_uGeneration, xOld.m_uGeneration, "reuse bumped the generation");

	ZENITH_ASSERT_TRUE(xPointers.ClaimPointer(xNew, 0x55u), "the new gesture is claimed");
	ZENITH_ASSERT_FALSE(xPointers.ClaimPointer(xOld, 0x66u), "a stale handle cannot claim");

	// The stale release must be rejected even when the owner token matches.
	xPointers.ReleaseClaim(xOld, 0x55u);
	ZENITH_ASSERT_TRUE(xPointers.IsClaimed(xNew), "stale-generation release rejected");

	// A live release by a NON-owner is rejected too.
	xPointers.ReleaseClaim(xNew, 0x77u);
	ZENITH_ASSERT_TRUE(xPointers.IsClaimed(xNew), "release by a non-owner rejected");

	xPointers.ReleaseClaim(xNew, 0x55u);
	ZENITH_ASSERT_FALSE(xPointers.IsClaimed(xNew), "the owner can release its own claim");
}

// B3 (permanent): on a pointing-device platform the mouse IS pointer 0, so
// every unmigrated mouse consumer and every migrated widget see one gesture.
ZENITH_TEST(Pointers, WindowsMouseIsPointerZero)
{
	Zenith_Pointers xPointers;

	xPointers.BeginFrame(1.0f);
	xPointers.ApplyMouseState(true, false, Zenith_Maths::Vector2(300.0f, 400.0f), 0.0);

	const Zenith_PointerHandle xHandle = xPointers.FindByPlatformId(Zenith_Pointers::iMOUSE_PLATFORM_ID);
	ZENITH_ASSERT_TRUE(xHandle.IsValid(), "a left-button press opens pointer 0");
	ZENITH_ASSERT_EQ(xHandle.m_uSlot, 0u, "the mouse takes the primary slot");

	const Zenith_Pointer* pxPointer = xPointers.Resolve(xHandle);
	ZENITH_ASSERT_NOT_NULL(pxPointer, "mouse pointer resolves");
	ZENITH_ASSERT_TRUE(pxPointer->m_bDownThisFrame, "mouse press raises the down edge");
	ZENITH_ASSERT_FALSE(pxPointer->m_bFromTouch, "the mouse projection is not a touch-origin pointer");
	ZENITH_ASSERT_EQ_FLOAT(pxPointer->m_xPosition.x, 300.0f, 1e-4f, "mouse position adopted");

	// The SAME edge is visible at step 4 and step 7; it must open exactly one
	// pointer, not one per phase.
	xPointers.ApplyMouseState(true, false, Zenith_Maths::Vector2(305.0f, 400.0f), 0.0);
	ZENITH_ASSERT_EQ(xPointers.GetActivePointerCount(), 1u, "a press edge is consumed once per frame");
	ZENITH_ASSERT_EQ_FLOAT(xPointers.Resolve(xHandle)->m_xPosition.x, 305.0f, 1e-4f, "the second phase still tracks motion");

	// Drag, then release.
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyMouseState(false, false, Zenith_Maths::Vector2(340.0f, 400.0f), 0.1);
	ZENITH_ASSERT_EQ_FLOAT(xPointers.Resolve(xHandle)->m_fMaxExcursion, 40.0f, 1e-4f, "mouse drag accumulates excursion");

	xPointers.BeginFrame(1.0f);
	xPointers.ApplyMouseState(false, true, Zenith_Maths::Vector2(340.0f, 400.0f), 0.2);
	ZENITH_ASSERT_TRUE(xPointers.Resolve(xHandle)->m_bUpThisFrame, "mouse release ends the pointer");

	// A real finger owns the primary slot instead: the projection must not open a
	// second pointer for the same gesture (Android feeds the mouse view FROM touch).
	Zenith_StepEmptyPointerFrame(xPointers);
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_DOWN, 0, 12.0f, 12.0f, 0.0));
	xPointers.ApplyMouseState(true, false, Zenith_Maths::Vector2(12.0f, 12.0f), 0.0);
	ZENITH_ASSERT_EQ(xPointers.GetActivePointerCount(), 1u, "the touch stream suppresses the mouse projection");
	ZENITH_ASSERT_TRUE(xPointers.Resolve(xPointers.FindByPlatformId(0))->m_bFromTouch, "the surviving pointer is the finger");
}

// Frame contract step 7: an automated-test Step runs AFTER step 4, so whatever
// it injected has to land in the SAME frame — otherwise every simulated tap is
// a frame late and a press+release inside one Step is invisible.
ZENITH_TEST(Pointers, SimulatedTouchAppliesAtInjectionPoint)
{
	Zenith_Input    xInput;
	Zenith_Pointers xPointers;

	// Step 4: the drain produced nothing this frame.
	xPointers.BeginFrame(1.0f);
	xPointers.ConsumeStagedTail(xInput);
	ZENITH_ASSERT_EQ(xPointers.GetActivePointerCount(), 0u, "nothing staged at the drain");

	// The Step injects a touch (SimulateTouchDown -> AppendInjectedEvent), which
	// appends to the staged stream after step 4 has already run.
	Zenith_InputEvent xDown = Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_DOWN, 2, 640.0f, 360.0f, 0.0);
	xInput.AppendInjectedEvent(xDown);

	// Step 7 consumes only the new tail.
	xPointers.ConsumeStagedTail(xInput);
	ZENITH_ASSERT_EQ(xPointers.GetActivePointerCount(), 1u, "the injected touch lands in the same frame");

	const Zenith_PointerHandle xHandle = xPointers.FindByPlatformId(2);
	ZENITH_ASSERT_TRUE(xHandle.IsValid(), "injected pointer id tracked");
	ZENITH_ASSERT_TRUE(xPointers.Resolve(xHandle)->m_bDownThisFrame, "injected down edge visible this frame");
	ZENITH_ASSERT_EQ_FLOAT(xPointers.Resolve(xHandle)->m_xPosition.y, 360.0f, 1e-4f, "injected position adopted");

	// Re-running the tail consume must not replay what step 4 or step 7 already
	// applied — a third phase would otherwise re-raise the same DOWN.
	xPointers.ConsumeStagedTail(xInput);
	ZENITH_ASSERT_EQ(xPointers.GetActivePointerCount(), 1u, "consumed entries are not replayed");
}

// A tap is short AND still. The stillness threshold is in LOGICAL pixels, so it
// has to scale with the display: 15 physical px is a different gesture on a
// 3x-density panel than on a desktop monitor.
ZENITH_TEST(Pointers, TapMaxExcursion)
{
	Zenith_Pointers xPointers;

	// Just inside the threshold at scale 1.
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_DOWN, 1, 0.0f, 0.0f, 0.0));
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_UP, 1, 14.0f, 0.0f, 0.05));
	ZENITH_ASSERT_TRUE(xPointers.WasTapThisFrame(), "14 px of travel is still a tap at scale 1");

	// Just outside it.
	Zenith_StepEmptyPointerFrame(xPointers);
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_DOWN, 1, 0.0f, 0.0f, 0.0));
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_UP, 1, 16.0f, 0.0f, 0.05));
	ZENITH_ASSERT_FALSE(xPointers.WasTapThisFrame(), "16 px of travel is a drag, not a tap");

	// The SAME 16 px is well inside the threshold on a 2x display.
	Zenith_StepEmptyPointerFrame(xPointers);
	xPointers.BeginFrame(2.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_DOWN, 1, 0.0f, 0.0f, 0.0));
	xPointers.BeginFrame(2.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_UP, 1, 16.0f, 0.0f, 0.05));
	ZENITH_ASSERT_TRUE(xPointers.WasTapThisFrame(), "the excursion threshold scales with the display");

	// Wander recorded by a COALESCED move: the final position is back at the
	// start, so only the FIFO's running max can tell this was not a tap.
	Zenith_StepEmptyPointerFrame(xPointers);
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_DOWN, 1, 0.0f, 0.0f, 0.0));
	Zenith_InputEvent xWander = Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_MOVE, 1, 0.0f, 0.0f, 0.02);
	xWander.m_fExcursion = 120.0f;
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(xWander);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_UP, 1, 0.0f, 0.0f, 0.05));
	ZENITH_ASSERT_FALSE(xPointers.WasTapThisFrame(), "coalesced wander defeats the tap test");

	// Too SLOW is not a tap either, however still the finger was.
	Zenith_StepEmptyPointerFrame(xPointers);
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_DOWN, 1, 0.0f, 0.0f, 0.0));
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_UP, 1, 0.0f, 0.0f, 0.9));
	ZENITH_ASSERT_FALSE(xPointers.WasTapThisFrame(), "a long press is not a tap");
}

// Teardown path: a widget dying mid-gesture releases through its owner token,
// and only its OWN claims go.
ZENITH_TEST(Pointers, ReleaseAllClaimsForOwnerLeavesOtherOwners)
{
	Zenith_Pointers xPointers;

	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_DOWN, 1, 10.0f, 10.0f, 0.0));
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_DOWN, 2, 20.0f, 20.0f, 0.0));

	const Zenith_PointerHandle xA = xPointers.FindByPlatformId(1);
	const Zenith_PointerHandle xB = xPointers.FindByPlatformId(2);
	ZENITH_ASSERT_TRUE(xPointers.ClaimPointer(xA, 0xAAu), "owner A claims its pointer");
	ZENITH_ASSERT_TRUE(xPointers.ClaimPointer(xB, 0xBBu), "owner B claims its pointer");

	xPointers.ReleaseAllClaimsForOwner(0xAAu);
	ZENITH_ASSERT_FALSE(xPointers.IsClaimed(xA), "owner A's claim released");
	ZENITH_ASSERT_TRUE(xPointers.IsClaimed(xB), "owner B's claim untouched");

	// Releasing a token that owns nothing is a no-op, so teardown can call it blindly.
	xPointers.ReleaseAllClaimsForOwner(0xCCu);
	ZENITH_ASSERT_TRUE(xPointers.IsClaimed(xB), "an unknown owner releases nothing");
}

// Between automated tests the table must not leak a held finger, a claim or a
// disarmed lifecycle state into the next test.
ZENITH_TEST(Pointers, ResetTransientForTestClearsTable)
{
	Zenith_Pointers xPointers;

	xPointers.BeginFrame(2.0f);
	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_DOWN, 1, 10.0f, 10.0f, 0.0));
	xPointers.ClaimPointer(xPointers.FindByPlatformId(1), 0x99u);
	Zenith_InputEvent xBarrier;
	xBarrier.m_eType = INPUT_EVENT_LIFECYCLE_RESET;
	xPointers.ApplyEvent(xBarrier);
	ZENITH_ASSERT_FALSE(xPointers.IsArmed(), "barrier disarmed the table");

	xPointers.ResetTransientForTest();
	ZENITH_ASSERT_EQ(xPointers.GetActivePointerCount(), 0u, "no pointers survive the reset");
	ZENITH_ASSERT_TRUE(xPointers.IsArmed(), "the reset re-arms");
	ZENITH_ASSERT_FALSE(xPointers.FindByPlatformId(1).IsValid(), "platform ids released");
	ZENITH_ASSERT_EQ_FLOAT(xPointers.GetDisplayScale(), 1.0f, 1e-4f, "display scale back to its default");
}

// The table is finite; a ninth simultaneous finger must be dropped cleanly
// rather than corrupting an existing slot.
ZENITH_TEST(Pointers, PointerTableSaturates)
{
	Zenith_Pointers xPointers;

	xPointers.BeginFrame(1.0f);
	for (u_int32 u = 0; u < Zenith_Pointers::uMAX_POINTERS; u++)
	{
		xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_DOWN,
			static_cast<int32_t>(u), static_cast<float>(u), 0.0f, 0.0));
	}
	ZENITH_ASSERT_EQ(xPointers.GetActivePointerCount(), Zenith_Pointers::uMAX_POINTERS, "table filled");

	xPointers.ApplyEvent(Zenith_MakePointerEvent(INPUT_EVENT_TOUCH_DOWN, 99, 500.0f, 500.0f, 0.0));
	ZENITH_ASSERT_EQ(xPointers.GetActivePointerCount(), Zenith_Pointers::uMAX_POINTERS, "the overflow finger is dropped");
	ZENITH_ASSERT_FALSE(xPointers.FindByPlatformId(99).IsValid(), "no slot invented for the overflow finger");
	ZENITH_ASSERT_TRUE(xPointers.FindByPlatformId(0).IsValid(), "existing pointers untouched by the overflow");
}
