#include "Core/Zenith_TestFramework.h"
#include "UI/Zenith_UICanvas.h"
#include "Input/Zenith_InputActions.h"
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

	// WP2: one frame of the device + action layers, window-free. The action
	// instance is LOCAL, so the engine's own registrations are never touched.
	struct Zenith_UIFocusNavRig
	{
		Zenith_Input        m_xInput;
		Zenith_Pointers     m_xPointers;
		Zenith_InputActions m_xActions;

		Zenith_UIFocusNavRig()
		{
			m_xActions.Initialise(m_xInput, m_xPointers);
			// Exactly what Zenith_Engine installs at boot: the five reserved UI
			// actions and the platform's default profile.
			m_xActions.RegisterEngineReservedActions();
			m_xActions.RegisterEngineDefaultProfiles();
			// This rig drives focus/confirm with KEYBOARD + GAMEPAD, so it needs a
			// scheme mask that resolves them -- the engine's own default profile is
			// platform-conditional (Windows: desktop KB|MOUSE|PAD; Android: TOUCH
			// only, since a real game replaces it before any of this matters). A
			// game's first RegisterProfile call always wholesale-replaces the engine
			// default (see RegisterProfile), so this mirrors exactly what any real
			// game with a keyboard/gamepad profile does, making the rig's behaviour
			// -- and this test -- platform-independent instead of accidentally
			// passing only on desktop.
			m_xActions.RegisterProfile(1, "TestDesktop", uINPUT_SCHEME_MASK_DESKTOP);
		}

		void BeginFrame()
		{
			m_xInput.DrainPendingPlatformEvents();
			m_xPointers.BeginFrame(1.0f);
		}

		void RaiseEvent(Zenith_InputEventType eType, int32_t iCode)
		{
			Zenith_InputEvent xEvent;
			xEvent.m_eType = eType;
			xEvent.m_iCode = iCode;
			m_xInput.AppendInjectedEvent(xEvent);
		}

		// Steps 8 + 10b: everything the canvas's focus navigation needs to be
		// closed before it runs.
		void CloseReservedStage()
		{
			m_xActions.UpdateProfile();
			m_xActions.FinalizeReservedUI();
		}
	};

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

// ============================================================================
// WP2: focus navigation is driven by the engine-RESERVED UI actions, and the
// canvas is the SOLE owner of keyboard/gamepad activation.
//
// These are the UI half of the UINavActionsDriveFocus / UIConfirmSingleOwner
// pair — the action half lives in Input/Zenith_InputActions.Tests.inl. They sit
// here because Input/ is BELOW UI/ in the layer DAG and may not include a
// canvas; UI/ calling down into the action layer is the legal direction.
// ============================================================================

ZENITH_TEST(UIFocusNav, ReservedActionsDriveFocus)
{
	Zenith_UIButtonTestSimScope xSimScope;

	Zenith_UI::Zenith_UICanvas xCanvas;
	Zenith_UIButtonTestCounter xTopCounter;
	Zenith_UIButtonTestCounter xBottomCounter;

	Zenith_UI::Zenith_UIButton* pxTop = new Zenith_UI::Zenith_UIButton("Top", "TopButton");
	pxTop->SetPosition(100.0f, 100.0f);
	pxTop->SetSize(200.0f, 50.0f);
	pxTop->SetOnClick(&Zenith_UIButtonTestOnClick, &xTopCounter);
	xCanvas.AddElement(pxTop);

	Zenith_UI::Zenith_UIButton* pxBottom = new Zenith_UI::Zenith_UIButton("Bottom", "BottomButton");
	pxBottom->SetPosition(100.0f, 200.0f);
	pxBottom->SetSize(200.0f, 50.0f);
	pxBottom->SetOnClick(&Zenith_UIButtonTestOnClick, &xBottomCounter);
	xCanvas.AddElement(pxBottom);

	pxTop->SetNavigation(nullptr, pxBottom, nullptr, nullptr);
	pxBottom->SetNavigation(pxTop, nullptr, nullptr, nullptr);
	xCanvas.SetFocusedElement(pxTop);

	Zenith_UIFocusNavRig xRig;

	// An ARROW key moves the focus...
	xRig.BeginFrame();
	xRig.RaiseEvent(INPUT_EVENT_KEY_PRESS, ZENITH_KEY_DOWN);
	xRig.CloseReservedStage();
	xCanvas.UpdateFocusNavigation(xRig.m_xActions);
	ZENITH_ASSERT_EQ(xCanvas.GetFocusedElement(), static_cast<Zenith_UI::Zenith_UIElement*>(pxBottom),
		"the down arrow moved the focus");

	// ...and so does the D-PAD, through the SAME action.
	xRig.BeginFrame();
	xRig.RaiseEvent(INPUT_EVENT_KEY_RELEASE, ZENITH_KEY_DOWN);
	xRig.RaiseEvent(INPUT_EVENT_PAD_PRESS, ZENITH_GAMEPAD_BUTTON_DPAD_UP);
	xRig.CloseReservedStage();
	xCanvas.UpdateFocusNavigation(xRig.m_xActions);
	ZENITH_ASSERT_EQ(xCanvas.GetFocusedElement(), static_cast<Zenith_UI::Zenith_UIElement*>(pxTop),
		"the d-pad drives the same navigation the arrows do");

	// A frame with no navigation edge leaves the focus alone (the actions are
	// EDGES, not held state — a key left down must not walk the menu).
	xRig.BeginFrame();
	xRig.CloseReservedStage();
	xCanvas.UpdateFocusNavigation(xRig.m_xActions);
	ZENITH_ASSERT_EQ(xCanvas.GetFocusedElement(), static_cast<Zenith_UI::Zenith_UIElement*>(pxTop),
		"a held button does not repeat");
	ZENITH_ASSERT_EQ(xTopCounter.m_uClicks, 0u, "navigating never activates");
	ZENITH_ASSERT_EQ(xBottomCounter.m_uClicks, 0u, "navigating never activates");
}

ZENITH_TEST(UIFocusNav, UIConfirmSingleOwner)
{
	Zenith_UIButtonTestSimScope xSimScope;

	Zenith_UI::Zenith_UICanvas xCanvas;
	Zenith_UIButtonTestCounter xFocusedCounter;
	Zenith_UIButtonTestCounter xOtherCounter;

	Zenith_UI::Zenith_UIButton* pxFocused = new Zenith_UI::Zenith_UIButton("A", "FocusedButton");
	pxFocused->SetPosition(100.0f, 100.0f);
	pxFocused->SetSize(200.0f, 50.0f);
	pxFocused->SetOnClick(&Zenith_UIButtonTestOnClick, &xFocusedCounter);
	xCanvas.AddElement(pxFocused);

	Zenith_UI::Zenith_UIButton* pxOther = new Zenith_UI::Zenith_UIButton("B", "OtherButton");
	pxOther->SetPosition(100.0f, 200.0f);
	pxOther->SetSize(200.0f, 50.0f);
	pxOther->SetOnClick(&Zenith_UIButtonTestOnClick, &xOtherCounter);
	xCanvas.AddElement(pxOther);

	xCanvas.SetFocusedElement(pxFocused);

	Zenith_UIFocusNavRig xRig;
	Zenith_Pointers xWalkPointers;

	// ENTER: exactly ONE activation, of the FOCUSED element, from the CANVAS.
	// The button's own Enter/Space path is gone, so a second activation here
	// would mean it had come back.
	xRig.BeginFrame();
	xRig.RaiseEvent(INPUT_EVENT_KEY_PRESS, ZENITH_KEY_ENTER);
	xRig.CloseReservedStage();
	xCanvas.UpdateFocusNavigation(xRig.m_xActions);
	// The visual pass and the capture walk both run afterwards, exactly as the
	// frame contract has them; neither may add an activation of its own.
	xCanvas.UpdatePointerInput(xWalkPointers, 0.016f);
	xCanvas.Update(0.016f);

	ZENITH_ASSERT_EQ(xFocusedCounter.m_uClicks, 1u, "the focused button activated exactly once");
	ZENITH_ASSERT_EQ(xOtherCounter.m_uClicks, 0u, "and nothing else did");

	// SPACE and pad A reach the same single owner.
	xRig.BeginFrame();
	xRig.RaiseEvent(INPUT_EVENT_KEY_RELEASE, ZENITH_KEY_ENTER);
	xRig.RaiseEvent(INPUT_EVENT_KEY_PRESS, ZENITH_KEY_SPACE);
	xRig.CloseReservedStage();
	xCanvas.UpdateFocusNavigation(xRig.m_xActions);
	xCanvas.Update(0.016f);
	ZENITH_ASSERT_EQ(xFocusedCounter.m_uClicks, 2u, "Space confirms through the canvas");

	xRig.BeginFrame();
	xRig.RaiseEvent(INPUT_EVENT_KEY_RELEASE, ZENITH_KEY_SPACE);
	xRig.RaiseEvent(INPUT_EVENT_PAD_PRESS, ZENITH_GAMEPAD_BUTTON_A);
	xRig.CloseReservedStage();
	xCanvas.UpdateFocusNavigation(xRig.m_xActions);
	xCanvas.Update(0.016f);
	ZENITH_ASSERT_EQ(xFocusedCounter.m_uClicks, 3u, "pad A confirms through the canvas");
	ZENITH_ASSERT_EQ(xOtherCounter.m_uClicks, 0u, "still nothing else");
}
