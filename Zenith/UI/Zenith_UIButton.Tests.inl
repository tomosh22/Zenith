#include "Core/Zenith_TestFramework.h"
#include "UI/Zenith_UICanvas.h"
#include "Input/Zenith_Pointers.h"
#ifdef ZENITH_INPUT_SIMULATOR
#include "Input/Zenith_InputSimulator.h"
#endif

// ============================================================================
// WP1: the standard-control pointer capture (frame contract step 10c).
//
// These drive a real canvas + button against a LOCAL Zenith_Pointers instance,
// so the engine's own table is never touched (B13).
//
// The simulator is enabled for the duration: a TOOLS build suppresses widget
// input while the editor is STOPPED (the canvas is being authored, not played),
// and that is exactly the state a unit test at boot runs in. Enabling the
// simulator is what an automated test does, and it also makes the editor's
// viewport remap a no-op, so the raw surface coordinates below are the
// canvas-space coordinates the button hit-tests against.
// ============================================================================

namespace
{
	struct Zenith_UIButtonTestCounter
	{
		u_int32 m_uClicks = 0;
	};

	void Zenith_UIButtonTestOnClick(void* pxUserData)
	{
		static_cast<Zenith_UIButtonTestCounter*>(pxUserData)->m_uClicks++;
	}

	// RAII so a failing assertion cannot leave the simulator enabled for the
	// rest of the boot batch.
	struct Zenith_UIButtonTestSimScope
	{
		Zenith_UIButtonTestSimScope()
		{
#ifdef ZENITH_INPUT_SIMULATOR
			Zenith_InputSimulator::Enable();
#endif
		}
		~Zenith_UIButtonTestSimScope()
		{
#ifdef ZENITH_INPUT_SIMULATOR
			Zenith_InputSimulator::Disable();
#endif
		}
	};

	Zenith_InputEvent Zenith_MakeUITouchEvent(Zenith_InputEventType eType, int32_t iPointerId, float fX, float fY)
	{
		Zenith_InputEvent xEvent;
		xEvent.m_eType    = eType;
		xEvent.m_iCode    = iPointerId;
		xEvent.m_fX       = fX;
		xEvent.m_fY       = fY;
		xEvent.m_fAnchorX = fX;
		xEvent.m_fAnchorY = fY;
		return xEvent;
	}

	Zenith_UI::Zenith_UIButton* Zenith_AddTestButton(Zenith_UI::Zenith_UICanvas& xCanvas,
		Zenith_UIButtonTestCounter& xCounter)
	{
		// Anchor + pivot are top-left by default, so these bounds are absolute
		// regardless of whatever size the window reports: (100,100)-(300,150).
		Zenith_UI::Zenith_UIButton* pxButton = new Zenith_UI::Zenith_UIButton("Go", "PointerCaptureTestButton");
		pxButton->SetPosition(100.0f, 100.0f);
		pxButton->SetSize(200.0f, 50.0f);
		pxButton->SetOnClick(&Zenith_UIButtonTestOnClick, &xCounter);
		xCanvas.AddElement(pxButton);
		return pxButton;
	}
}

// The click contract: claim on DOWN inside, keep the claim through a drag OFF
// the button, and fire only on a release that comes back INSIDE.
ZENITH_TEST(UIPointerCapture, StandardButtonCaptureDragOffRelease)
{
	Zenith_UIButtonTestSimScope xSimScope;

	Zenith_UI::Zenith_UICanvas xCanvas;
	Zenith_UIButtonTestCounter xCounter;
	Zenith_AddTestButton(xCanvas, xCounter);

	Zenith_Pointers xPointers;

	// --- Down inside: the button takes the claim -------------------------
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakeUITouchEvent(INPUT_EVENT_TOUCH_DOWN, 1, 150.0f, 120.0f));
	const Zenith_PointerHandle xHandle = xPointers.FindByPlatformId(1);
	ZENITH_ASSERT_TRUE(xHandle.IsValid(), "the press produced a pointer");
	ZENITH_ASSERT_FALSE(xPointers.IsClaimed(xHandle), "unclaimed before the UI walk");

	xCanvas.UpdatePointerInput(xPointers, 0.016f);
	ZENITH_ASSERT_TRUE(xPointers.IsClaimed(xHandle), "the button claims a press inside its bounds");
	ZENITH_ASSERT_EQ(xCounter.m_uClicks, 0u, "click-on-RELEASE: nothing fires on the down");

	// --- Drag off: the claim is KEPT, so nothing can steal the gesture ----
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakeUITouchEvent(INPUT_EVENT_TOUCH_MOVE, 1, 900.0f, 600.0f));
	xCanvas.UpdatePointerInput(xPointers, 0.016f);
	ZENITH_ASSERT_TRUE(xPointers.IsClaimed(xHandle), "dragging off keeps the claim");
	ZENITH_ASSERT_EQ(xCounter.m_uClicks, 0u, "dragging off does not click");

	// --- Release OUTSIDE: no click, and the claim is given back ----------
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakeUITouchEvent(INPUT_EVENT_TOUCH_UP, 1, 900.0f, 600.0f));
	xCanvas.UpdatePointerInput(xPointers, 0.016f);
	ZENITH_ASSERT_EQ(xCounter.m_uClicks, 0u, "releasing outside the bounds does not click");
	ZENITH_ASSERT_FALSE(xPointers.IsClaimed(xHandle), "the claim is released on the terminal edge");

	// --- Down inside, drag off, drag BACK, release inside: one click -----
	xPointers.BeginFrame(1.0f);   // retires the finished slot
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakeUITouchEvent(INPUT_EVENT_TOUCH_DOWN, 1, 150.0f, 120.0f));
	xCanvas.UpdatePointerInput(xPointers, 0.016f);

	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakeUITouchEvent(INPUT_EVENT_TOUCH_MOVE, 1, 900.0f, 600.0f));
	xCanvas.UpdatePointerInput(xPointers, 0.016f);

	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakeUITouchEvent(INPUT_EVENT_TOUCH_MOVE, 1, 160.0f, 130.0f));
	xCanvas.UpdatePointerInput(xPointers, 0.016f);

	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakeUITouchEvent(INPUT_EVENT_TOUCH_UP, 1, 160.0f, 130.0f));
	xCanvas.UpdatePointerInput(xPointers, 0.016f);
	ZENITH_ASSERT_EQ(xCounter.m_uClicks, 1u, "coming back inside before releasing clicks exactly once");
	ZENITH_ASSERT_TRUE(xCanvas.WasPointerActivateThisFrame(), "the canvas activate edge is raised with the click");

	// --- Press that never touched the button: no claim, no click ---------
	xPointers.BeginFrame(1.0f);
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakeUITouchEvent(INPUT_EVENT_TOUCH_DOWN, 2, 900.0f, 600.0f));
	const Zenith_PointerHandle xOutside = xPointers.FindByPlatformId(2);
	xCanvas.UpdatePointerInput(xPointers, 0.016f);
	ZENITH_ASSERT_FALSE(xPointers.IsClaimed(xOutside), "a press outside every widget stays unclaimed");
	ZENITH_ASSERT_EQ(xCounter.m_uClicks, 1u, "no extra click from the outside press");

	// --- Press AND release inside ONE frame: still a click ---------------
	// Every quick tap on a touch device, and every simulated click whose down
	// and up land in the same tick. Both edges ride one pointer, so the button
	// must complete the click there rather than waiting for a frame that the
	// gesture has already outlived.
	xPointers.BeginFrame(1.0f);
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakeUITouchEvent(INPUT_EVENT_TOUCH_DOWN, 3, 150.0f, 120.0f));
	xPointers.ApplyEvent(Zenith_MakeUITouchEvent(INPUT_EVENT_TOUCH_UP, 3, 150.0f, 120.0f));
	xCanvas.UpdatePointerInput(xPointers, 0.016f);
	ZENITH_ASSERT_EQ(xCounter.m_uClicks, 2u, "a press and release inside one frame still clicks");
}

// A widget destroyed mid-gesture must hand its claim back. Without this the
// slot stays consumed until the finger lifts, silently swallowing the gesture
// for everything else on the canvas.
ZENITH_TEST(UIPointerCapture, OwnerReleaseOnWidgetDestruction)
{
	Zenith_UIButtonTestSimScope xSimScope;

	Zenith_UI::Zenith_UICanvas xCanvas;
	Zenith_UIButtonTestCounter xCounter;
	Zenith_UI::Zenith_UIButton* pxButton = Zenith_AddTestButton(xCanvas, xCounter);

	Zenith_Pointers xPointers;

	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakeUITouchEvent(INPUT_EVENT_TOUCH_DOWN, 1, 150.0f, 120.0f));
	const Zenith_PointerHandle xHandle = xPointers.FindByPlatformId(1);
	xCanvas.UpdatePointerInput(xPointers, 0.016f);
	ZENITH_ASSERT_TRUE(xPointers.IsClaimed(xHandle), "the button owns the pointer");

	// The finger is still down when the widget goes away.
	xCanvas.RemoveElement(pxButton);
	ZENITH_ASSERT_FALSE(xPointers.IsClaimed(xHandle), "destruction released the owner's claim");

	// ...and the pointer is genuinely free again, not merely marked unclaimed.
	ZENITH_ASSERT_TRUE(xPointers.ClaimPointer(xHandle, 0x5150u), "someone else can claim the freed pointer");
}

// B1 10c mutation rules: a click callback may build or destroy UI, and the walk
// must survive it. Creation is deferred to after the pass; destruction is
// deferred deletion, so the snapshot the walk holds can never dangle.
ZENITH_TEST(UIPointerCapture, InputPassDefersElementMutation)
{
	Zenith_UIButtonTestSimScope xSimScope;

	// A callback that removes the very button it was fired from, and adds a new
	// element, from INSIDE the walk.
	struct MutatingContext
	{
		Zenith_UI::Zenith_UICanvas*  m_pxCanvas = nullptr;
		Zenith_UI::Zenith_UIElement* m_pxSelf   = nullptr;
		bool                         m_bPassWasActive = false;
	};
	struct Local
	{
		static void OnClick(void* pxUserData)
		{
			MutatingContext* pxCtx = static_cast<MutatingContext*>(pxUserData);
			pxCtx->m_bPassWasActive = pxCtx->m_pxCanvas->IsInputPassActive();
			pxCtx->m_pxCanvas->AddElement(new Zenith_UI::Zenith_UIElement("SpawnedDuringPass"));
			pxCtx->m_pxCanvas->RemoveElement(pxCtx->m_pxSelf);
		}
	};

	Zenith_UI::Zenith_UICanvas xCanvas;
	MutatingContext xContext;

	Zenith_UI::Zenith_UIButton* pxButton = new Zenith_UI::Zenith_UIButton("Go", "SelfDestructButton");
	pxButton->SetPosition(100.0f, 100.0f);
	pxButton->SetSize(200.0f, 50.0f);
	pxButton->SetOnClick(&Local::OnClick, &xContext);
	xCanvas.AddElement(pxButton);

	xContext.m_pxCanvas = &xCanvas;
	xContext.m_pxSelf   = pxButton;
	ZENITH_ASSERT_EQ(xCanvas.GetElementCount(), static_cast<size_t>(1), "one root element before the click");

	Zenith_Pointers xPointers;
	xPointers.BeginFrame(1.0f);
	xPointers.ApplyEvent(Zenith_MakeUITouchEvent(INPUT_EVENT_TOUCH_DOWN, 1, 150.0f, 120.0f));
	xPointers.ApplyEvent(Zenith_MakeUITouchEvent(INPUT_EVENT_TOUCH_UP, 1, 150.0f, 120.0f));
	xCanvas.UpdatePointerInput(xPointers, 0.016f);

	ZENITH_ASSERT_TRUE(xContext.m_bPassWasActive, "the callback really did run inside the input pass");
	// The removed button is gone and the spawned element has joined the canvas —
	// both applied only once the walk unwound.
	ZENITH_ASSERT_EQ(xCanvas.GetElementCount(), static_cast<size_t>(1), "removal + creation both applied after the pass");
	ZENITH_ASSERT_NOT_NULL(xCanvas.FindElement("SpawnedDuringPass"), "the deferred creation landed");
	ZENITH_ASSERT_NULL(xCanvas.FindElement("SelfDestructButton"), "the deferred destruction landed");
}
