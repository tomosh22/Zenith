#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ScriptTest_GymBehaviour.cpp -- WP-2c: the BEHAVIOURAL gym tests (C7-C12).
//
// WP-2a/2b pin the SHAPE of the game (slots resolve, elements exist, scenes
// carry the entities they should). Nothing there can see a graph that builds
// perfectly and then does nothing -- a Timer that never fires, a sensor that
// never reports, a state machine whose transition events reach no lamp. These
// nine tests DRIVE each gym through the real seams a player would and assert
// on the observable result:
//
//   ST_HubNavigation_Test  - every hub button AND every number key reaches its
//                            gym, and Escape returns from every one of them.
//                            The row count is derived, not written down.
//   ST_InputGym_Test       - held W moves the cube in +Z, releasing it stops
//                            the cube, and Space clears the grounded raycast
//                            and lifts it.
//   ST_EventsGym_Test      - the plate's TARGETED FireCustomEvent raises the
//                            door and stepping off lowers it; the bell's
//                            BROADCAST pulses ALL THREE listeners that nothing
//                            names, and each settles back afterwards.
//   ST_PhysicsGym_Test     - the spawn/kill loop actually loops: both counters
//                            climb, Space spawns on demand (attributed against
//                            a just-observed timer spawn), the HUD text the
//                            spawner writes cross-entity matches the counter,
//                            and the live-ball population stays bounded (i.e.
//                            balls are really destroyed, not merely counted).
//   ST_MotionGym_Test      - all three transform drivers move: RotateEntity,
//                            the Repeat/Tween ping-pong, and the blackboard-
//                            maths bob.
//   ST_StateGym_Test       - the StateMachine reaches Green on schedule AND
//                            its TLEnter_/TLExit_ chains reached the lamps.
//   ST_UIGym_Test          - two buttons and a key mutate one blackboard int,
//                            the clock/modulo/compare chain crosses the hot
//                            boundary in both directions, and the BarFill
//                            element's COLOUR is read back on both sides of it.
//   ST_FlowGym_Test        - the multi-way flow constructs, which no other
//                            scene reaches: Once, Cooldown, Gate,
//                            WaitForCondition, SwitchOnInt, SwitchOnString,
//                            Selector, ForEach, CallGraph, the three list
//                            mutators and LogicBlackboardBool, each against a
//                            distinct observable.
//   ST_AIGym_Test          - all twelve AI nodes, authored with NO game C++:
//                            EnsureNavAgent wires an agent to the scene's
//                            navmesh, the walker paths / stops / slows /
//                            wanders, and the perception family reports the
//                            prey, hears it and forgets it on the symmetric
//                            unregister.
//
// SINGLE SPELLING. Every scene index, entity name, UI element name and
// blackboard variable comes from ScriptTest_Graphs.h -- the same header the
// builders and scene recipes read. A test that restated one of those literals
// would prove only that the test agrees with itself. Values the header does
// NOT own (authored world positions, tween durations) are spelled here with a
// comment naming where in ScriptTest.cpp they are authored.
//
// dt: the harness PINS 1/60 across the world reset, Setup and every Step (see
// Zenith_AutomatedTest.cpp's m_fFixedDt, defaulted rather than "unset"), so
// these tests deliberately do NOT call SetFixedDt/ClearFixedDt themselves --
// a ClearFixedDt in Verify would unpin the timebase for the rest of the frame
// for no gain. All frame counts below are therefore 60 Hz.
//
// Headless-safe: nothing here reads a pixel, so every test leaves
// m_bRequiresGraphics at its struct default (false) and runs on the Null
// backend.
// ============================================================================

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"
#include "Physics/Zenith_Physics.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Query.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UIText.h"

#include "ScriptTest/ScriptTest_Graphs.h"

#include <cmath>
#include <string>

// ============================================================================
// Shared probes
// ============================================================================

namespace
{
	// --- Scene / entity resolution -------------------------------------------

	int32_t ST_ActiveSceneIndex()
	{
		Zenith_SceneSystem& xScenes = g_xEngine.Scenes();
		return static_cast<int32_t>(xScenes.GetSceneInfo(xScenes.GetActiveScene()).m_iBuildIndex);
	}

	bool ST_IsSceneActive(int32_t iBuildIndex)
	{
		return ST_ActiveSceneIndex() == iBuildIndex;
	}

	// Active-scene name lookup. Returns an INVALID entity when there is no
	// active scene or nothing carries the name -- never asserts, because every
	// caller here polls across a scene load and must tolerate "not yet".
	Zenith_Entity ST_FindEntity(const char* szName)
	{
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
		if (pxSceneData == nullptr)
		{
			return Zenith_Entity();
		}
		return pxSceneData->FindEntityByName(szName);
	}

	bool ST_EntityExists(const char* szName)
	{
		return ST_FindEntity(szName).IsValid();
	}

	// --- Transform reads ------------------------------------------------------

	bool ST_GetPosition(const char* szName, Zenith_Maths::Vector3& xOut)
	{
		Zenith_Entity xEntity = ST_FindEntity(szName);
		if (!xEntity.IsValid())
		{
			return false;
		}
		Zenith_TransformComponent* pxTransform = xEntity.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr)
		{
			return false;
		}
		pxTransform->GetPosition(xOut);
		return true;
	}

	bool ST_GetScale(const char* szName, Zenith_Maths::Vector3& xOut)
	{
		Zenith_Entity xEntity = ST_FindEntity(szName);
		if (!xEntity.IsValid())
		{
			return false;
		}
		Zenith_TransformComponent* pxTransform = xEntity.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr)
		{
			return false;
		}
		pxTransform->GetScale(xOut);
		return true;
	}

	bool ST_GetRotation(const char* szName, Zenith_Maths::Quat& xOut)
	{
		Zenith_Entity xEntity = ST_FindEntity(szName);
		if (!xEntity.IsValid())
		{
			return false;
		}
		Zenith_TransformComponent* pxTransform = xEntity.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr)
		{
			return false;
		}
		pxTransform->GetRotation(xOut);
		return true;
	}

	// --- Blackboard reads -----------------------------------------------------

	// One graph slot's live blackboard. The slot INDEX is the attach order in
	// ScriptTest.cpp's scene recipes (the B1 slot order the header's Graphs
	// block documents), so each caller names it at its own call site.
	Zenith_GraphBlackboard* ST_Blackboard(const char* szEntity, u_int uSlot)
	{
		Zenith_Entity xEntity = ST_FindEntity(szEntity);
		if (!xEntity.IsValid())
		{
			return nullptr;
		}
		Zenith_GraphComponent* pxGraphs = xEntity.TryGetComponent<Zenith_GraphComponent>();
		if (pxGraphs == nullptr || uSlot >= pxGraphs->GetGraphCount())
		{
			return nullptr;
		}
		Zenith_BehaviourGraph* pxGraph = pxGraphs->GetGraphAt(uSlot);
		if (pxGraph == nullptr)
		{
			return nullptr;
		}
		return &pxGraph->GetBlackboard();
	}

	int32_t ST_ReadInt(const char* szEntity, u_int uSlot, const char* szVar, int32_t iDefault)
	{
		const Zenith_GraphBlackboard* pxBlackboard = ST_Blackboard(szEntity, uSlot);
		return pxBlackboard != nullptr ? pxBlackboard->GetInt32(szVar, iDefault) : iDefault;
	}

	float ST_ReadFloat(const char* szEntity, u_int uSlot, const char* szVar, float fDefault)
	{
		const Zenith_GraphBlackboard* pxBlackboard = ST_Blackboard(szEntity, uSlot);
		return pxBlackboard != nullptr ? pxBlackboard->GetFloat(szVar, fDefault) : fDefault;
	}

	bool ST_ReadBool(const char* szEntity, u_int uSlot, const char* szVar, bool bDefault)
	{
		const Zenith_GraphBlackboard* pxBlackboard = ST_Blackboard(szEntity, uSlot);
		return pxBlackboard != nullptr ? pxBlackboard->GetBool(szVar, bDefault) : bDefault;
	}

	std::string ST_ReadString(const char* szEntity, u_int uSlot, const char* szVar, const char* szDefault)
	{
		const Zenith_GraphBlackboard* pxBlackboard = ST_Blackboard(szEntity, uSlot);
		return pxBlackboard != nullptr ? pxBlackboard->GetString(szVar, szDefault) : std::string(szDefault);
	}

	Zenith_Maths::Vector3 ST_ReadVec3(const char* szEntity, u_int uSlot, const char* szVar)
	{
		const Zenith_GraphBlackboard* pxBlackboard = ST_Blackboard(szEntity, uSlot);
		return pxBlackboard != nullptr
			? pxBlackboard->GetVector3(szVar) : Zenith_Maths::Vector3(0.0f);
	}

	// A packed Zenith_EntityID, which is how every cross-entity reference in
	// these graphs travels. 0 is the "nothing there" sentinel: no entity ever
	// packs to it.
	u_int64 ST_ReadPacked(const char* szEntity, u_int uSlot, const char* szVar)
	{
		const Zenith_GraphBlackboard* pxBlackboard = ST_Blackboard(szEntity, uSlot);
		return pxBlackboard != nullptr ? pxBlackboard->GetPackedEntityID(szVar, 0) : 0;
	}

	float ST_XZDistance(const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB)
	{
		const float fDX = xA.x - xB.x;
		const float fDZ = xA.z - xB.z;
		return std::sqrt(fDX * fDX + fDZ * fDZ);
	}

	// --- Body-aware placement -------------------------------------------------

	// The same shape Zenith_GraphNode_SetEntityPosition takes on its teleport
	// branch: zero both velocities, then Zenith_Physics::TeleportBody, which
	// fires the pose-changed hook so the transform commits THIS frame. A bare
	// SetPosition on an entity with a live dynamic body desyncs the two -- the
	// body keeps its old pose and drags the transform back on the next sweep.
	// (TeleportBody also forces IDENTITY rotation; harmless here, the cube's
	// rotation is locked by ST_PlayerMove's OnStart chain anyway.)
	bool ST_TeleportEntity(const char* szName, const Zenith_Maths::Vector3& xPosition)
	{
		Zenith_Entity xEntity = ST_FindEntity(szName);
		if (!xEntity.IsValid())
		{
			return false;
		}
		Zenith_TransformComponent* pxTransform = xEntity.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr)
		{
			return false;
		}

		Zenith_ColliderComponent* pxCollider = xEntity.TryGetComponent<Zenith_ColliderComponent>();
		Zenith_Physics& xPhysics = g_xEngine.Physics();
		if (pxCollider != nullptr && pxCollider->HasValidBody() && xPhysics.HasActiveSimulation())
		{
			const Zenith_PhysicsBodyID xBodyID = pxCollider->GetBodyID();
			xPhysics.SetLinearVelocity(xBodyID, Zenith_Maths::Vector3(0.0f));
			xPhysics.SetAngularVelocity(xBodyID, Zenith_Maths::Vector3(0.0f));
			xPhysics.TeleportBody(xBodyID, xPosition);
			return true;
		}

		pxTransform->SetPosition(xPosition);
		return true;
	}

	// --- UI -------------------------------------------------------------------

	// Every gym's canvas lives on the GameManager. FindElement<T> is an
	// unchecked static_cast (RTTI is off), so the type is verified first.
	Zenith_UI::Zenith_UIButton* ST_FindButton(const char* szButton)
	{
		Zenith_Entity xManager = ST_FindEntity(ScriptTest::Entities::szGAME_MANAGER);
		if (!xManager.IsValid())
		{
			return nullptr;
		}
		Zenith_UIComponent* pxUI = xManager.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr)
		{
			return nullptr;
		}
		Zenith_UI::Zenith_UIElement* pxElement = pxUI->FindElement(szButton);
		if (pxElement == nullptr || pxElement->GetType() != Zenith_UI::UIElementType::Button)
		{
			return nullptr;
		}
		return static_cast<Zenith_UI::Zenith_UIButton*>(pxElement);
	}

	// Activate() runs the button's installed callback directly. The callback is
	// the OnUIButtonClicked trampoline, which the graph node installs on its
	// FIRST OnUpdate after a scene load -- so a click before that lands on a
	// null callback and is silently lost. Every caller warms up first.
	bool ST_ClickButton(const char* szButton)
	{
		Zenith_UI::Zenith_UIButton* pxButton = ST_FindButton(szButton);
		if (pxButton == nullptr)
		{
			return false;
		}
		pxButton->Activate();
		return true;
	}

	// The text-bearing twin of ST_FindButton, and the ONE probe in this suite
	// that reads what a SetUIText node actually wrote.
	//
	// ★ WITHOUT IT, SetUIText IS UNOBSERVED. It is chain-TERMINAL in four of its
	// five chains, so a typo'd m_strElement or a broken m_strTargetVar returns
	// FAILURE, aborts nothing downstream (there is no downstream) and leaves the
	// authored seed text on screen forever -- every other assertion in this file
	// still passes. Reading the string is the only way that shows up.
	//
	// Same unchecked-static_cast discipline as ST_FindButton: FindElement<T> does
	// not verify, so the type is checked first. Zenith_GraphNode_SetUIText makes
	// exactly the same check and FAILS on a non-text element rather than casting
	// blindly, so a mismatch here is the node's own contract, not a test detail.
	bool ST_ReadUIText(const char* szElement, std::string& strOut)
	{
		Zenith_Entity xManager = ST_FindEntity(ScriptTest::Entities::szGAME_MANAGER);
		if (!xManager.IsValid())
		{
			return false;
		}
		Zenith_UIComponent* pxUI = xManager.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr)
		{
			return false;
		}
		Zenith_UI::Zenith_UIElement* pxElement = pxUI->FindElement(szElement);
		if (pxElement == nullptr || pxElement->GetType() != Zenith_UI::UIElementType::Text)
		{
			return false;
		}
		strOut = static_cast<Zenith_UI::Zenith_UIText*>(pxElement)->GetText();
		return true;
	}

	// The colour twin of ST_ReadUIText, and the ONLY thing in this suite that can
	// see a SetUIColor node at all.
	//
	// ★ BOTH SetUIColor NODES ARE CHAIN-TERMINAL, one on each of ST_UIPlayground's
	// Branch pins. A typo'd m_strElement returns FAILURE with nothing downstream
	// to abort, so the bar simply keeps its authored colour while every other
	// assertion in this file -- including the 'hot' flag that selects between the
	// two -- still passes. Reading the element's colour is what closes that.
	//
	// The type branch mirrors the node's own: Zenith_GraphNode_SetUIColor writes a
	// Button's NORMAL style rather than its base colour (buttons render per-state
	// styles and ignore m_xColor), so the read has to come back out of the same
	// place. BarFill is a Rect, so GetColor() is the branch that fires today --
	// but a probe that only knew about Rects would silently read a stale white
	// off any button a later gym points a SetUIColor at.
	bool ST_ReadUIColor(const char* szElement, Zenith_Maths::Vector4& xOut)
	{
		Zenith_Entity xManager = ST_FindEntity(ScriptTest::Entities::szGAME_MANAGER);
		if (!xManager.IsValid())
		{
			return false;
		}
		Zenith_UIComponent* pxUI = xManager.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr)
		{
			return false;
		}
		Zenith_UI::Zenith_UIElement* pxElement = pxUI->FindElement(szElement);
		if (pxElement == nullptr)
		{
			return false;
		}
		xOut = pxElement->GetType() == Zenith_UI::UIElementType::Button
			? static_cast<Zenith_UI::Zenith_UIButton*>(pxElement)->GetNormalColor()
			: pxElement->GetColor();
		return true;
	}

	// Componentwise equality for an authored RGBA that made one trip through
	// Zenith_PropertyValue and the .bgraph serializer. Both are float32 the whole
	// way, so this is generous by a wide margin -- it exists to keep the assertion
	// about "the node ran and wrote ITS colour" rather than about float exactness.
	bool ST_ColourNear(const Zenith_Maths::Vector4& xA, const Zenith_Maths::Vector4& xB, float fEpsilon)
	{
		return std::fabs(xA.x - xB.x) <= fEpsilon
			&& std::fabs(xA.y - xB.y) <= fEpsilon
			&& std::fabs(xA.z - xB.z) <= fEpsilon
			&& std::fabs(xA.w - xB.w) <= fEpsilon;
	}
}

// ============================================================================
// ST_HubNavigation_Test (C7)
// ----------------------------------------------------------------------------
// Two scene transitions per hub row per pass: each row entered by CLICKING its
// button and again by pressing its NUMBER KEY, each followed by Escape back to
// the hub. This is the only test that exercises every one of ST_HubFlow's
// independent chains and every gym's ST_EscToHub in one run.
//
// The row COUNT is derived from the table and static_asserted against
// Scenes::iCOUNT, so adding a gym without a hub row is a compile error rather
// than a silent narrowing of what this covers.
//
// Two latencies are structural, not incidental:
//   * the OnUIButtonClicked trampoline installs on the first OnUpdate after a
//     load, and a fresh node instance LATCHES to the click count it finds, so
//     a click before that first tick is swallowed -- hence the warm-up frames
//     after every load;
//   * a LoadSceneByIndex fired from inside a graph runs during the scene
//     update pass and is deferred to the end of that frame, so the new build
//     index appears on a later frame -- hence the polls.
// ============================================================================

namespace
{
	struct ST_HubRow
	{
		const char*    m_szButton;
		Zenith_KeyCode m_eKey;
		int32_t        m_iScene;
	};

	// Hub order. The button name and the scene index are the header's; the key
	// codes are the engine's own constants, spelled in ST_BuildHubKeyChain the
	// same way.
	const ST_HubRow g_axHubRows[] = {
		{ ScriptTest::UINames::szBTN_MOTION,  ZENITH_KEY_1, ScriptTest::Scenes::iGYM_MOTION  },
		{ ScriptTest::UINames::szBTN_INPUT,   ZENITH_KEY_2, ScriptTest::Scenes::iGYM_INPUT   },
		{ ScriptTest::UINames::szBTN_PHYSICS, ZENITH_KEY_3, ScriptTest::Scenes::iGYM_PHYSICS },
		{ ScriptTest::UINames::szBTN_EVENTS,  ZENITH_KEY_4, ScriptTest::Scenes::iGYM_EVENTS  },
		{ ScriptTest::UINames::szBTN_STATE,   ZENITH_KEY_5, ScriptTest::Scenes::iGYM_STATE   },
		{ ScriptTest::UINames::szBTN_UI,      ZENITH_KEY_6, ScriptTest::Scenes::iGYM_UI      },
		{ ScriptTest::UINames::szBTN_FLOW,    ZENITH_KEY_7, ScriptTest::Scenes::iGYM_FLOW    },
		{ ScriptTest::UINames::szBTN_AI,      ZENITH_KEY_8, ScriptTest::Scenes::iGYM_AI      },
	};

	constexpr u_int iST_HUB_ROWS = static_cast<u_int>(sizeof(g_axHubRows) / sizeof(g_axHubRows[0]));

	// ★ A GYM ADDED WITHOUT A HUB ROW IS THE FAILURE THIS CATCHES, and it used
	// to be catchable only by reading. Every scene except the hub itself is
	// reachable from the hub by construction, so the row count IS iCOUNT - 1;
	// a new gym that forgot its row would otherwise leave this test quietly
	// exercising the same six it always did while every assertion still passed.
	static_assert(iST_HUB_ROWS + 1u == static_cast<u_int>(ScriptTest::Scenes::iCOUNT),
		"every gym needs a hub row here -- otherwise ST_HubNavigation_Test silently "
		"stops covering the newest one");

	// Two passes over the table: buttons, then keys. Derived rather than written
	// down, for the same reason as the static_assert above.
	constexpr int   iST_HUB_STEPS       = static_cast<int>(iST_HUB_ROWS) * 2;
	constexpr u_int32 uST_HUB_ALL_ROWS  = (1u << iST_HUB_ROWS) - 1u;
	constexpr int   iST_HUB_WARM_FRAMES = 3;	// trampoline install + latch
	constexpr int   iST_HUB_POLL_LIMIT  = 45;	// deferred load + settle

	enum class HubPhase { WaitHub, WarmHub, Trigger, AwaitGym, WarmGym, AwaitHub, Done };

	HubPhase g_eHubPhase   = HubPhase::WaitHub;
	int      g_iHubStep    = 0;
	int      g_iHubWarm    = 0;
	int      g_iHubPoll    = 0;
	u_int32  g_uHubButtonsReached = 0;	// bit i = row i entered by its button
	u_int32  g_uHubKeysReached    = 0;	// bit i = row i entered by its key
	int      g_iHubEscReturns     = 0;
	bool     g_bHubClickFailed    = false;
	bool     g_bHubTimedOut       = false;

	const ST_HubRow& CurrentHubRow()
	{
		return g_axHubRows[static_cast<u_int>(g_iHubStep) % iST_HUB_ROWS];
	}

	bool HubStepIsKeyPass()
	{
		return static_cast<u_int>(g_iHubStep) >= iST_HUB_ROWS;
	}
}

static void Setup_HubNavigation()
{
	g_eHubPhase = HubPhase::WaitHub;
	g_iHubStep = 0;
	g_iHubWarm = 0;
	g_iHubPoll = 0;
	g_uHubButtonsReached = 0;
	g_uHubKeysReached = 0;
	g_iHubEscReturns = 0;
	g_bHubClickFailed = false;
	g_bHubTimedOut = false;
}

static bool Step_HubNavigation(int iFrame)
{
	switch (g_eHubPhase)
	{
	case HubPhase::WaitHub:
		// The harness already reloaded build index 0 and settled; this only
		// guards against a slow first frame.
		if (ST_IsSceneActive(ScriptTest::Scenes::iHUB))
		{
			g_iHubWarm = 0;
			g_eHubPhase = HubPhase::WarmHub;
			return true;
		}
		if (iFrame > 120)
		{
			g_bHubTimedOut = true;
			g_eHubPhase = HubPhase::Done;
			return false;
		}
		return true;

	case HubPhase::WarmHub:
		if (++g_iHubWarm >= iST_HUB_WARM_FRAMES)
		{
			g_eHubPhase = HubPhase::Trigger;
		}
		return true;

	case HubPhase::Trigger:
	{
		const ST_HubRow& xRow = CurrentHubRow();
		if (HubStepIsKeyPass())
		{
			Zenith_InputSimulator::SimulateKeyPress(xRow.m_eKey);
		}
		else if (!ST_ClickButton(xRow.m_szButton))
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[HubNavigation] button '%s' not found on the hub canvas", xRow.m_szButton);
			g_bHubClickFailed = true;
			g_eHubPhase = HubPhase::Done;
			return false;
		}
		g_iHubPoll = 0;
		g_eHubPhase = HubPhase::AwaitGym;
		return true;
	}

	case HubPhase::AwaitGym:
	{
		const ST_HubRow& xRow = CurrentHubRow();
		if (ST_IsSceneActive(xRow.m_iScene))
		{
			const u_int uRow = static_cast<u_int>(g_iHubStep) % iST_HUB_ROWS;
			if (HubStepIsKeyPass())
			{
				g_uHubKeysReached |= (1u << uRow);
			}
			else
			{
				g_uHubButtonsReached |= (1u << uRow);
			}
			g_iHubWarm = 0;
			g_eHubPhase = HubPhase::WarmGym;
			return true;
		}
		if (++g_iHubPoll > iST_HUB_POLL_LIMIT)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[HubNavigation] step %d (%s) never reached scene %d (active %d)",
				g_iHubStep, HubStepIsKeyPass() ? "key" : "button", xRow.m_iScene, ST_ActiveSceneIndex());
			g_bHubTimedOut = true;
			g_eHubPhase = HubPhase::Done;
			return false;
		}
		return true;
	}

	case HubPhase::WarmGym:
		if (++g_iHubWarm >= iST_HUB_WARM_FRAMES)
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
			g_iHubPoll = 0;
			g_eHubPhase = HubPhase::AwaitHub;
		}
		return true;

	case HubPhase::AwaitHub:
		if (ST_IsSceneActive(ScriptTest::Scenes::iHUB))
		{
			++g_iHubEscReturns;
			++g_iHubStep;
			if (g_iHubStep >= iST_HUB_STEPS)
			{
				g_eHubPhase = HubPhase::Done;
				return false;
			}
			g_iHubWarm = 0;
			g_eHubPhase = HubPhase::WarmHub;
			return true;
		}
		if (++g_iHubPoll > iST_HUB_POLL_LIMIT)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[HubNavigation] Escape from scene %d never returned to the hub (active %d)",
				CurrentHubRow().m_iScene, ST_ActiveSceneIndex());
			g_bHubTimedOut = true;
			g_eHubPhase = HubPhase::Done;
			return false;
		}
		return true;

	case HubPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_HubNavigation()
{
	if (g_bHubClickFailed)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[HubNavigation] aborted: a hub button could not be resolved");
		return false;
	}
	if (g_bHubTimedOut)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[HubNavigation] aborted: a transition timed out (step %d)", g_iHubStep);
		return false;
	}
	if (g_uHubButtonsReached != uST_HUB_ALL_ROWS)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[HubNavigation] button rows reached = 0x%02X, expected 0x%02X",
			g_uHubButtonsReached, uST_HUB_ALL_ROWS);
		return false;
	}
	if (g_uHubKeysReached != uST_HUB_ALL_ROWS)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[HubNavigation] key rows reached = 0x%02X, expected 0x%02X",
			g_uHubKeysReached, uST_HUB_ALL_ROWS);
		return false;
	}
	if (g_iHubEscReturns != iST_HUB_STEPS)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[HubNavigation] Escape returned %d times, expected %d",
			g_iHubEscReturns, iST_HUB_STEPS);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xHubNavigationTest = {
	"ST_HubNavigation_Test",
	&Setup_HubNavigation,
	&Step_HubNavigation,
	&Verify_HubNavigation,
	// Per step: warm + a deferred load + warm + a deferred unload, each capped by
	// iST_HUB_POLL_LIMIT. 200 frames a step is roughly double the observed cost,
	// and it scales with the table rather than needing a bump per gym.
	/*maxFrames*/ iST_HUB_STEPS * 200,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xHubNavigationTest);

// ============================================================================
// ST_InputGym_Test (C8)
// ----------------------------------------------------------------------------
// ST_PlayerMove reads the WASD quad through ReadMovementAxis, whose forward key
// writes +Z (Zenith_GraphNode_Registration_Input.cpp: "xDirection.z += 1.0f"),
// scales it by 6 and hands it to SetVelocity with m_bSetY = false. So a held W
// is +6 m/s on Z with gravity still owning Y, and releasing it makes moveDir
// zero -- SetVelocity(0,0,0) with Y preserved -- which STOPS the cube rather
// than letting it coast. Both halves are asserted; a graph that only ever set
// velocity on press would pass the first and fail the second.
//
// The rest height is MEASURED, not assumed: the cube is authored at y = 1.5 and
// settles onto the floor, and the jump is asserted as a rise ABOVE that
// measured rest rather than against a hard-coded number.
// ============================================================================

namespace
{
	constexpr int iST_INPUT_SETTLE_FRAMES = 40;	// the drop from the authored y = 1.5
	constexpr int iST_INPUT_HOLD_FRAMES   = 60;	// 1.0 s at 6 m/s = 6 m, well inside the 15-wide floor
	constexpr int iST_INPUT_DECAY_FRAMES  = 20;
	constexpr int iST_INPUT_RISE_FRAMES   = 40;

	enum class InputPhase { Boot, WaitScene, Settle, HoldW, Decay, AwaitRise, Done };

	InputPhase g_eInputPhase   = InputPhase::Boot;
	int        g_iInputFrame   = 0;
	float      g_fInputZStart  = 0.0f;
	float      g_fInputZHeld   = 0.0f;
	float      g_fInputZStop   = 0.0f;
	float      g_fInputRestY   = 0.0f;
	float      g_fInputPeakY   = 0.0f;
	bool       g_bInputWHeld   = false;
	bool       g_bInputReady   = false;
	bool       g_bInputDone    = false;
}

static void Setup_InputGym()
{
	g_eInputPhase = InputPhase::Boot;
	g_iInputFrame = 0;
	g_fInputZStart = 0.0f;
	g_fInputZHeld = 0.0f;
	g_fInputZStop = 0.0f;
	g_fInputRestY = 0.0f;
	g_fInputPeakY = 0.0f;
	g_bInputWHeld = false;
	g_bInputReady = false;
	g_bInputDone = false;
}

static bool Step_InputGym(int iFrame)
{
	switch (g_eInputPhase)
	{
	case InputPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(ScriptTest::Scenes::iGYM_INPUT, SCENE_LOAD_SINGLE);
		g_eInputPhase = InputPhase::WaitScene;
		return true;

	case InputPhase::WaitScene:
		if (ST_IsSceneActive(ScriptTest::Scenes::iGYM_INPUT)
			&& ST_EntityExists(ScriptTest::Entities::szPLAYER_CUBE))
		{
			g_bInputReady = true;
			g_iInputFrame = 0;
			g_eInputPhase = InputPhase::Settle;
			return true;
		}
		return iFrame < 180;

	case InputPhase::Settle:
	{
		if (++g_iInputFrame < iST_INPUT_SETTLE_FRAMES)
		{
			return true;
		}
		Zenith_Maths::Vector3 xPosition(0.0f);
		if (!ST_GetPosition(ScriptTest::Entities::szPLAYER_CUBE, xPosition))
		{
			return false;
		}
		g_fInputZStart = xPosition.z;
		// Held, not tapped: SimulateKeyDown with no matching Up leaves the key
		// asserted in the device layer's event-fed held table, which is what
		// ReadMovementAxis polls through IsKeyDown.
		Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_W);
		g_bInputWHeld = true;
		g_iInputFrame = 0;
		g_eInputPhase = InputPhase::HoldW;
		return true;
	}

	case InputPhase::HoldW:
	{
		if (++g_iInputFrame < iST_INPUT_HOLD_FRAMES)
		{
			return true;
		}
		Zenith_Maths::Vector3 xPosition(0.0f);
		if (!ST_GetPosition(ScriptTest::Entities::szPLAYER_CUBE, xPosition))
		{
			return false;
		}
		g_fInputZHeld = xPosition.z;
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_W);
		g_bInputWHeld = false;
		g_iInputFrame = 0;
		g_eInputPhase = InputPhase::Decay;
		return true;
	}

	case InputPhase::Decay:
	{
		if (++g_iInputFrame < iST_INPUT_DECAY_FRAMES)
		{
			return true;
		}
		Zenith_Maths::Vector3 xPosition(0.0f);
		if (!ST_GetPosition(ScriptTest::Entities::szPLAYER_CUBE, xPosition))
		{
			return false;
		}
		g_fInputZStop = xPosition.z;
		g_fInputRestY = xPosition.y;
		g_fInputPeakY = xPosition.y;
		// The grounded raycast (0.8 m down, self excluded) must find the floor
		// for the impulse to run at all -- FAILURE aborts the chain, which is
		// exactly the no-double-jump gate.
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_SPACE);
		g_iInputFrame = 0;
		g_eInputPhase = InputPhase::AwaitRise;
		return true;
	}

	case InputPhase::AwaitRise:
	{
		Zenith_Maths::Vector3 xPosition(0.0f);
		if (ST_GetPosition(ScriptTest::Entities::szPLAYER_CUBE, xPosition) && xPosition.y > g_fInputPeakY)
		{
			g_fInputPeakY = xPosition.y;
		}
		if (g_fInputPeakY - g_fInputRestY > 0.5f || ++g_iInputFrame > iST_INPUT_RISE_FRAMES)
		{
			g_bInputDone = true;
			g_eInputPhase = InputPhase::Done;
			return false;
		}
		return true;
	}

	case InputPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_InputGym()
{
	if (g_bInputWHeld)
	{
		// Never leave a key asserted for the next test, even on a timeout.
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_W);
		g_bInputWHeld = false;
	}
	if (!g_bInputReady)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[InputGym] Gym_Input never became active with a PlayerCube");
		return false;
	}
	if (!g_bInputDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[InputGym] never completed (phase %d)", static_cast<int>(g_eInputPhase));
		return false;
	}

	const float fTravel = g_fInputZHeld - g_fInputZStart;
	if (fTravel <= 1.0f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[InputGym] held W moved the cube %.3f on +Z, expected > 1.0 (z %.3f -> %.3f)",
			fTravel, g_fInputZStart, g_fInputZHeld);
		return false;
	}
	const float fCoast = std::abs(g_fInputZStop - g_fInputZHeld);
	if (fCoast >= 0.5f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[InputGym] cube coasted %.3f after W was released, expected < 0.5", fCoast);
		return false;
	}
	const float fRise = g_fInputPeakY - g_fInputRestY;
	if (fRise <= 0.5f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[InputGym] Space lifted the cube %.3f above rest %.3f, expected > 0.5", fRise, g_fInputRestY);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xInputGymTest = {
	"ST_InputGym_Test",
	&Setup_InputGym,
	&Step_InputGym,
	&Verify_InputGym,
	/*maxFrames*/ 420,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xInputGymTest);

// ============================================================================
// ST_EventsGym_Test (C9)
// ----------------------------------------------------------------------------
// Both cross-entity messaging shapes:
//   * TARGETED -- the plate's OnCollisionEnter looks the door up by name and
//     fires OpenDoor at it; OnCollisionExit fires CloseDoor. The door's own
//     graph decides that means a TweenPosition between y = 1.5 and y = 4.5.
//   * BROADCAST -- key B fires Bell at every GraphComponent in every loaded
//     scene; the three listeners are named nowhere.
//
// The plate is a STATIC SENSOR, so it never stops the cube -- a cube resting on
// the floor at the plate's XZ still overlaps the plate's slab, which is what
// makes "stand on it" a persistent Enter rather than a pass-through Enter/Exit
// pair. Leaving is a teleport away, not a step down.
//
// ★ ALL THREE LISTENERS, AND BOTH HALVES OF THE POP. Two ways this could pass
// while being broken, and each needs its own clause:
//   * reading only BellListener_A would survive a regression from BROADCAST to
//     TARGETED delivery -- one named listener would still pulse and the other
//     two would sit at their authored 0.8 forever. So all three peaks are
//     tracked and all three must cross 1.1.
//   * stopping at the peak would survive a broken suspended-anchor resume. The
//     graph is OnCustomEvent -> TweenScale(1.3, 0.15 s) -> WaitForTween ->
//     TweenScale(0.8, 0.3 s), and WaitForTween SUSPENDS the chain: it is
//     ON_UPDATE re-driving the stalled anchor that reaches the second tween. If
//     that resume ever broke, every listener would stick at 1.3 and a peak-only
//     test would call it a pass. So the settle is asserted too -- each listener
//     must come back DOWN past 0.9.
// ============================================================================

namespace
{
	// Authored in ScriptTest.cpp's ST_AuthorGymEventsScene: the plate sits at
	// (4, 0.1, 0) with scale (2, 0.2, 2) and the cube starts at (0, 1.5, -4).
	// Neither is in the header, so both are spelled here once.
	const Zenith_Maths::Vector3 g_xEventsOnPlate(4.0f, 1.0f, 0.0f);
	const Zenith_Maths::Vector3 g_xEventsOffPlate(0.0f, 1.0f, -4.0f);

	// The door tweens 1.5 -> 4.5 over 0.8 s; 2.5 is a third of the way up, so
	// it cannot be reached by anything but a running tween.
	constexpr float fST_DOOR_OPEN_Y      = 2.5f;
	constexpr int   iST_EVENTS_SETTLE    = 30;
	constexpr int   iST_DOOR_POLL_LIMIT  = 200;	// 0.8 s tween + fall + event latency
	constexpr int   iST_BELL_POLL_LIMIT  = 40;	// the pulse peaks 0.15 s after the key
	// The raise tween is 0.8 s (48 frames) and the door crosses 2.5 about a
	// third of the way through it. Stepping off THERE would start the lowering
	// tween while the raising one is still live, so the close half would be
	// measuring two tweens fighting rather than the CloseDoor event. Let the
	// first one finish.
	constexpr int   iST_DOOR_HOLD_OPEN   = 60;
	// The whole pop is 0.15 s + 0.3 s = 0.45 s (27 frames) measured from the
	// event, and the peak clause consumes the first few of those, so the settle
	// has ~20 to run. 90 is generous on purpose: a window that only just fits
	// would turn a slow frame into a flake, and the clause is here to catch a
	// resume that never happens at all, not one that is late.
	constexpr int   iST_BELL_SETTLE_LIMIT = 90;

	// Authored scale is 0.8, the pulse tweens to 1.3 and settles back to 0.8, so
	// 1.1 can only be the grow tween running and 0.9 can only be the settle one.
	constexpr float fST_BELL_PEAK_MIN    = 1.1f;
	constexpr float fST_BELL_SETTLE_MAX  = 0.9f;

	constexpr int   iST_BELL_LISTENERS   = 3;

	// The three listeners share ONE .bgraph and are named nowhere in it -- which
	// is the whole point of the broadcast, and the reason all three are read.
	const char* const g_apszBellListeners[iST_BELL_LISTENERS] = {
		ScriptTest::Entities::szBELL_LISTENER_A,
		ScriptTest::Entities::szBELL_LISTENER_B,
		ScriptTest::Entities::szBELL_LISTENER_C,
	};

	enum class EventsPhase { Boot, WaitScene, Settle, AwaitOpen, HoldOpen, AwaitClose, AwaitPulse, AwaitSettle, Done };

	EventsPhase g_eEventsPhase   = EventsPhase::Boot;
	int         g_iEventsFrame   = 0;
	float       g_fDoorPeakY     = 0.0f;
	float       g_fDoorRestY     = 0.0f;
	float       g_afBellPeakScale[iST_BELL_LISTENERS]   = {};
	float       g_afBellSettleScale[iST_BELL_LISTENERS] = {};
	bool        g_bEventsReady   = false;
	bool        g_bDoorOpened    = false;
	bool        g_bDoorClosed    = false;
	bool        g_bBellPulsed    = false;
	bool        g_bBellSettled   = false;
	bool        g_bEventsDone    = false;

	// Peaks track the MAXIMUM scale.x ever seen; settles track the MINIMUM seen
	// once the peak clause is satisfied. Both are extrema over a window rather
	// than a sample at a chosen instant, because a tween's value at any single
	// frame depends on where in its curve the poll happens to land.
	void ST_SampleBellPeaks()
	{
		Zenith_Maths::Vector3 xScale(0.0f);
		for (int i = 0; i < iST_BELL_LISTENERS; ++i)
		{
			if (ST_GetScale(g_apszBellListeners[i], xScale) && xScale.x > g_afBellPeakScale[i])
			{
				g_afBellPeakScale[i] = xScale.x;
			}
		}
	}

	void ST_SampleBellSettles()
	{
		Zenith_Maths::Vector3 xScale(0.0f);
		for (int i = 0; i < iST_BELL_LISTENERS; ++i)
		{
			if (ST_GetScale(g_apszBellListeners[i], xScale) && xScale.x < g_afBellSettleScale[i])
			{
				g_afBellSettleScale[i] = xScale.x;
			}
		}
	}

	bool ST_AllBellsPeaked()
	{
		for (int i = 0; i < iST_BELL_LISTENERS; ++i)
		{
			if (!(g_afBellPeakScale[i] > fST_BELL_PEAK_MIN))
			{
				return false;
			}
		}
		return true;
	}

	bool ST_AllBellsSettled()
	{
		for (int i = 0; i < iST_BELL_LISTENERS; ++i)
		{
			if (!(g_afBellSettleScale[i] < fST_BELL_SETTLE_MAX))
			{
				return false;
			}
		}
		return true;
	}
}

static void Setup_EventsGym()
{
	g_eEventsPhase = EventsPhase::Boot;
	g_iEventsFrame = 0;
	g_fDoorPeakY = 0.0f;
	g_fDoorRestY = 0.0f;
	for (int i = 0; i < iST_BELL_LISTENERS; ++i)
	{
		g_afBellPeakScale[i] = 0.0f;
		g_afBellSettleScale[i] = 1.0e30f;
	}
	g_bEventsReady = false;
	g_bDoorOpened = false;
	g_bDoorClosed = false;
	g_bBellPulsed = false;
	g_bBellSettled = false;
	g_bEventsDone = false;
}

static bool Step_EventsGym(int iFrame)
{
	switch (g_eEventsPhase)
	{
	case EventsPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(ScriptTest::Scenes::iGYM_EVENTS, SCENE_LOAD_SINGLE);
		g_eEventsPhase = EventsPhase::WaitScene;
		return true;

	case EventsPhase::WaitScene:
		if (ST_IsSceneActive(ScriptTest::Scenes::iGYM_EVENTS)
			&& ST_EntityExists(ScriptTest::Entities::szPLAYER_CUBE)
			&& ST_EntityExists(ScriptTest::Entities::szPRESSURE_PLATE)
			&& ST_EntityExists(ScriptTest::Entities::szGYM_DOOR)
			&& ST_EntityExists(ScriptTest::Entities::szBELL_LISTENER_A)
			&& ST_EntityExists(ScriptTest::Entities::szBELL_LISTENER_B)
			&& ST_EntityExists(ScriptTest::Entities::szBELL_LISTENER_C))
		{
			g_bEventsReady = true;
			g_iEventsFrame = 0;
			g_eEventsPhase = EventsPhase::Settle;
			return true;
		}
		return iFrame < 180;

	case EventsPhase::Settle:
		if (++g_iEventsFrame < iST_EVENTS_SETTLE)
		{
			return true;
		}
		if (!ST_TeleportEntity(ScriptTest::Entities::szPLAYER_CUBE, g_xEventsOnPlate))
		{
			return false;
		}
		g_iEventsFrame = 0;
		g_eEventsPhase = EventsPhase::AwaitOpen;
		return true;

	case EventsPhase::AwaitOpen:
	{
		Zenith_Maths::Vector3 xDoor(0.0f);
		if (ST_GetPosition(ScriptTest::Entities::szGYM_DOOR, xDoor) && xDoor.y > g_fDoorPeakY)
		{
			g_fDoorPeakY = xDoor.y;
		}
		if (g_fDoorPeakY > fST_DOOR_OPEN_Y)
		{
			g_bDoorOpened = true;
			g_iEventsFrame = 0;
			g_eEventsPhase = EventsPhase::HoldOpen;
			return true;
		}
		if (++g_iEventsFrame > iST_DOOR_POLL_LIMIT)
		{
			g_eEventsPhase = EventsPhase::Done;
			return false;
		}
		return true;
	}

	case EventsPhase::HoldOpen:
	{
		Zenith_Maths::Vector3 xDoor(0.0f);
		if (ST_GetPosition(ScriptTest::Entities::szGYM_DOOR, xDoor) && xDoor.y > g_fDoorPeakY)
		{
			g_fDoorPeakY = xDoor.y;
		}
		if (++g_iEventsFrame < iST_DOOR_HOLD_OPEN)
		{
			return true;
		}
		if (!ST_TeleportEntity(ScriptTest::Entities::szPLAYER_CUBE, g_xEventsOffPlate))
		{
			return false;
		}
		g_fDoorRestY = g_fDoorPeakY;
		g_iEventsFrame = 0;
		g_eEventsPhase = EventsPhase::AwaitClose;
		return true;
	}

	case EventsPhase::AwaitClose:
	{
		Zenith_Maths::Vector3 xDoor(0.0f);
		if (ST_GetPosition(ScriptTest::Entities::szGYM_DOOR, xDoor))
		{
			g_fDoorRestY = xDoor.y;
			if (xDoor.y < fST_DOOR_OPEN_Y)
			{
				g_bDoorClosed = true;
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_B);
				g_iEventsFrame = 0;
				g_eEventsPhase = EventsPhase::AwaitPulse;
				return true;
			}
		}
		if (++g_iEventsFrame > iST_DOOR_POLL_LIMIT)
		{
			g_eEventsPhase = EventsPhase::Done;
			return false;
		}
		return true;
	}

	case EventsPhase::AwaitPulse:
	{
		ST_SampleBellPeaks();
		if (ST_AllBellsPeaked())
		{
			g_bBellPulsed = true;
			g_iEventsFrame = 0;
			g_eEventsPhase = EventsPhase::AwaitSettle;
			return true;
		}
		if (++g_iEventsFrame > iST_BELL_POLL_LIMIT)
		{
			g_bEventsDone = true;
			g_eEventsPhase = EventsPhase::Done;
			return false;
		}
		return true;
	}

	case EventsPhase::AwaitSettle:
	{
		// The peaks keep updating: a listener may still be climbing when the
		// last of the three crosses 1.1, and its own settle floor must not be
		// seeded from a frame before its grow tween finished.
		ST_SampleBellPeaks();
		ST_SampleBellSettles();
		if (ST_AllBellsSettled())
		{
			g_bBellSettled = true;
			g_bEventsDone = true;
			g_eEventsPhase = EventsPhase::Done;
			return false;
		}
		if (++g_iEventsFrame > iST_BELL_SETTLE_LIMIT)
		{
			g_bEventsDone = true;
			g_eEventsPhase = EventsPhase::Done;
			return false;
		}
		return true;
	}

	case EventsPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_EventsGym()
{
	if (!g_bEventsReady)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[EventsGym] Gym_Events never became active with its entities");
		return false;
	}
	if (!g_bDoorOpened)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[EventsGym] standing on the plate never raised the door past %.2f (peak %.3f)",
			fST_DOOR_OPEN_Y, g_fDoorPeakY);
		return false;
	}
	if (!g_bDoorClosed)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[EventsGym] leaving the plate never lowered the door below %.2f (last %.3f)",
			fST_DOOR_OPEN_Y, g_fDoorRestY);
		return false;
	}
	if (!g_bBellPulsed)
	{
		for (int i = 0; i < iST_BELL_LISTENERS; ++i)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[EventsGym] key B: %s peak scale.x %.3f (expected > %.2f)",
				g_apszBellListeners[i], g_afBellPeakScale[i], fST_BELL_PEAK_MIN);
		}
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[EventsGym] the Bell broadcast did not pulse all %d listeners -- a listener that "
			"nothing names was not reached", iST_BELL_LISTENERS);
		return false;
	}
	if (!g_bBellSettled)
	{
		for (int i = 0; i < iST_BELL_LISTENERS; ++i)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[EventsGym] settle: %s fell only to scale.x %.3f (expected < %.2f, peak %.3f)",
				g_apszBellListeners[i], g_afBellSettleScale[i], fST_BELL_SETTLE_MAX, g_afBellPeakScale[i]);
		}
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[EventsGym] the pulse never settled within %d frames -- WaitForTween suspended the "
			"chain and the second TweenScale was never reached", iST_BELL_SETTLE_LIMIT);
		return false;
	}
	if (!g_bEventsDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[EventsGym] never completed (phase %d)", static_cast<int>(g_eEventsPhase));
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xEventsGymTest = {
	"ST_EventsGym_Test",
	&Setup_EventsGym,
	&Step_EventsGym,
	&Verify_EventsGym,
	// Worst case is every poll running to its limit: 30 settle + 200 open +
	// 60 hold + 200 close + 40 pulse + 90 settle = 620, plus the scene wait.
	// 800 leaves the timeouts room to REPORT rather than being cut short by the
	// harness, which would lose the named failure.
	/*maxFrames*/ 800,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xEventsGymTest);

// ============================================================================
// ST_PhysicsGym_Test (C10)
// ----------------------------------------------------------------------------
// The spawner's Timer, the prefab spawn, the kill volume's static-sensor
// OnCollisionEnter and its DestroyEntity are ONE loop, and only a test that
// watches all four at once can tell a working loop from a spawner that counts
// while nothing is ever destroyed. So: both counters must climb, AND the live
// ball population must stay bounded.
//
// The bound is what proves destruction. Balls spawn every 1.5 s and fall ~14 m
// (y = 6 to the sensor slab at y = -8) in ~1.7 s, so a healthy loop holds two
// or three at a time forever; a loop where DestroyEntity never ran would climb
// past six inside the sample window.
//
// ★ THE MANUAL-SPAWN CLAUSE HAS TO BEAT THE TIMER, OR IT PROVES NOTHING. Both
// the Timer chain and the OnKeyPressed chain increment the SAME spawnCount, so
// "the counter rose after Space" is only evidence about the key if no timer
// spawn could have landed in the same window. Polling N frames after a blind
// press gives the timer an N-in-90 chance of supplying the increment, and a
// completely dead OnKeyPressed -> SpawnPrefab chain then passes at that rate.
//
// So the press is SYNCHRONISED to the timer instead of racing it: wait until a
// timer spawn is actually observed, press Space on that same frame, and require
// the next increment within a handful of frames. The next timer spawn is then a
// full period away (~87 frames), so the increment has exactly one possible
// author. The observation frame is the frame AFTER the spawning scene update,
// which is why the window is measured from there.
// ============================================================================

namespace
{
	constexpr int iST_PHYSICS_RUN_FRAMES   = 600;	// 10 s -> ~6 spawns at 1.5 s
	// One timer period is 1.5 s = 90 frames, so a live timer is always seen well
	// inside this; exceeding it means the Timer itself stopped firing.
	constexpr int iST_PHYSICS_TIMER_POLL   = 120;
	// A Step's SimulateKeyPress is applied by ApplySimulatorInjection BEFORE the
	// same frame's game logic (Zenith_Core.cpp's frame contract), so the graph
	// sees the press that frame and the counter it writes is readable on the
	// next Step -- a one-frame latency. Four sample frames is generous for that
	// and still ~83 frames clear of the next timer spawn.
	constexpr int iST_PHYSICS_MANUAL_POLL  = 3;
	constexpr int iST_PHYSICS_MIN_COUNT    = 3;
	constexpr int iST_PHYSICS_MAX_LIVE     = 6;

	enum class PhysicsPhase { Boot, WaitScene, Run, AwaitTimerSpawn, AwaitManual, Done };

	PhysicsPhase g_ePhysicsPhase   = PhysicsPhase::Boot;
	int          g_iPhysicsFrame   = 0;
	int32_t      g_iSpawnCount     = 0;
	int32_t      g_iKillCount      = 0;
	int32_t      g_iSpawnAtRunEnd  = 0;
	int32_t      g_iSpawnBeforeKey = 0;
	int32_t      g_iSpawnAfterKey  = 0;
	int          g_iLiveBallPeak   = 0;
	std::string  g_strSpawnedText;
	std::string  g_strSpawnedExpected;
	bool         g_bPhysicsReady   = false;
	bool         g_bTimerSpawnSeen = false;
	bool         g_bManualSpawned  = false;
	bool         g_bSpawnedTextRead = false;
	bool         g_bPhysicsDone    = false;

	// Names are not unique and SpawnPrefab stamps the prefab root with the one
	// the header owns, so a prefix match covers both a verbatim name and any
	// future uniquifying suffix.
	int ST_CountLiveBalls()
	{
		const std::string strPrefix(ScriptTest::Entities::szBALL);
		int iCount = 0;
		g_xEngine.Scenes().QueryActiveScene<Zenith_TransformComponent>().ForEach(
			[&iCount, &strPrefix](Zenith_EntityID xID, Zenith_TransformComponent&)
			{
				Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xID);
				if (!xEntity.IsValid())
				{
					return;
				}
				const std::string& strName = xEntity.GetName();
				if (strName.size() >= strPrefix.size()
					&& strName.compare(0, strPrefix.size(), strPrefix) == 0)
				{
					++iCount;
				}
			});
		return iCount;
	}

	void ST_SampleBallCounters()
	{
		g_iSpawnCount = ST_ReadInt(
			ScriptTest::Entities::szSPAWNER, 0u, ScriptTest::Vars::szSPAWN_COUNT, g_iSpawnCount);
		g_iKillCount = ST_ReadInt(
			ScriptTest::Entities::szKILL_VOLUME, 0u, ScriptTest::Vars::szKILL_COUNT, g_iKillCount);
		const int iLive = ST_CountLiveBalls();
		if (iLive > g_iLiveBallPeak)
		{
			g_iLiveBallPeak = iLive;
		}
	}
}

static void Setup_PhysicsGym()
{
	g_ePhysicsPhase = PhysicsPhase::Boot;
	g_iPhysicsFrame = 0;
	g_iSpawnCount = 0;
	g_iKillCount = 0;
	g_iSpawnAtRunEnd = 0;
	g_iSpawnBeforeKey = 0;
	g_iSpawnAfterKey = 0;
	g_iLiveBallPeak = 0;
	g_strSpawnedText.clear();
	g_strSpawnedExpected.clear();
	g_bPhysicsReady = false;
	g_bTimerSpawnSeen = false;
	g_bManualSpawned = false;
	g_bSpawnedTextRead = false;
	g_bPhysicsDone = false;
}

static bool Step_PhysicsGym(int iFrame)
{
	switch (g_ePhysicsPhase)
	{
	case PhysicsPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(ScriptTest::Scenes::iGYM_PHYSICS, SCENE_LOAD_SINGLE);
		g_ePhysicsPhase = PhysicsPhase::WaitScene;
		return true;

	case PhysicsPhase::WaitScene:
		if (ST_IsSceneActive(ScriptTest::Scenes::iGYM_PHYSICS)
			&& ST_EntityExists(ScriptTest::Entities::szSPAWNER)
			&& ST_EntityExists(ScriptTest::Entities::szKILL_VOLUME))
		{
			g_bPhysicsReady = true;
			g_iPhysicsFrame = 0;
			g_ePhysicsPhase = PhysicsPhase::Run;
			return true;
		}
		return iFrame < 180;

	case PhysicsPhase::Run:
		ST_SampleBallCounters();
		if (++g_iPhysicsFrame < iST_PHYSICS_RUN_FRAMES)
		{
			return true;
		}
		g_iSpawnAtRunEnd = g_iSpawnCount;
		g_iPhysicsFrame = 0;
		g_ePhysicsPhase = PhysicsPhase::AwaitTimerSpawn;
		return true;

	case PhysicsPhase::AwaitTimerSpawn:
		ST_SampleBallCounters();
		if (g_iSpawnCount > g_iSpawnAtRunEnd)
		{
			// A timer spawn landed on the PREVIOUS scene update, so the next one
			// is a full 90-frame period away. Everything the counter does from
			// here until then belongs to the key -- which is the on-demand chain:
			// a SECOND SpawnPrefab instance under an OnKeyPressed source (exec
			// fan-in is forbidden, so it cannot be the timer's node).
			g_bTimerSpawnSeen = true;
			g_iSpawnBeforeKey = g_iSpawnCount;
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_SPACE);
			g_iPhysicsFrame = 0;
			g_ePhysicsPhase = PhysicsPhase::AwaitManual;
			return true;
		}
		if (++g_iPhysicsFrame > iST_PHYSICS_TIMER_POLL)
		{
			g_bPhysicsDone = true;
			g_ePhysicsPhase = PhysicsPhase::Done;
			return false;
		}
		return true;

	case PhysicsPhase::AwaitManual:
		ST_SampleBallCounters();
		g_iSpawnAfterKey = g_iSpawnCount;
		if (g_iSpawnAfterKey > g_iSpawnBeforeKey)
		{
			g_bManualSpawned = true;
			// The HUD, read on the SAME frame as the counter it mirrors. The
			// Spawner's graph resolved the GameManager by name at OnStart and
			// stashed a packed EntityID in Vars::szUI_TARGET; every spawn's
			// SetUIText addresses the HUD THROUGH that variable. Nothing else in
			// this suite exercises m_strTargetVar end to end -- a broken one
			// leaves the authored seed text sitting here while spawnCount climbs,
			// and the node's FAILURE is swallowed because it terminates its chain.
			g_bSpawnedTextRead = ST_ReadUIText(ScriptTest::UINames::szSPAWNED, g_strSpawnedText);
			// Built from the LIVE blackboard value rather than a literal, so this
			// is an agreement between two readings of one number, not a restated
			// constant. The prefix mirrors the builder's "Spawned: {}" (an int32
			// renders through "%d", so no decimals appear).
			g_strSpawnedExpected = std::string("Spawned: ") + std::to_string(g_iSpawnAfterKey);
			g_bPhysicsDone = true;
			g_ePhysicsPhase = PhysicsPhase::Done;
			return false;
		}
		if (++g_iPhysicsFrame > iST_PHYSICS_MANUAL_POLL)
		{
			g_bPhysicsDone = true;
			g_ePhysicsPhase = PhysicsPhase::Done;
			return false;
		}
		return true;

	case PhysicsPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_PhysicsGym()
{
	if (!g_bPhysicsReady)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PhysicsGym] Gym_Physics never became active with its entities");
		return false;
	}
	if (!g_bPhysicsDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PhysicsGym] never completed (phase %d)", static_cast<int>(g_ePhysicsPhase));
		return false;
	}
	if (g_iSpawnCount < iST_PHYSICS_MIN_COUNT)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PhysicsGym] spawnCount %d, expected >= %d",
			g_iSpawnCount, iST_PHYSICS_MIN_COUNT);
		return false;
	}
	if (g_iKillCount < iST_PHYSICS_MIN_COUNT)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PhysicsGym] killCount %d, expected >= %d (spawnCount %d)",
			g_iKillCount, iST_PHYSICS_MIN_COUNT, g_iSpawnCount);
		return false;
	}
	// Reported before the key clause: without a synchronising timer spawn the key
	// result would have no clean attribution window, so "Space did not spawn"
	// would be the wrong diagnosis.
	if (!g_bTimerSpawnSeen)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[PhysicsGym] no timer spawn within %d frames of the run phase (spawnCount %d) -- "
			"the Timer stopped firing, so the Space press has no attribution window",
			iST_PHYSICS_TIMER_POLL, g_iSpawnCount);
		return false;
	}
	if (!g_bManualSpawned)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[PhysicsGym] Space did not spawn within %d frames of an observed timer spawn "
			"(spawnCount %d -> %d) -- the next timer spawn was ~87 frames away, so nothing "
			"else could have supplied one",
			iST_PHYSICS_MANUAL_POLL, g_iSpawnBeforeKey, g_iSpawnAfterKey);
		return false;
	}
	if (!g_bSpawnedTextRead)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[PhysicsGym] the '%s' readout is not a text element on the %s canvas",
			ScriptTest::UINames::szSPAWNED, ScriptTest::Entities::szGAME_MANAGER);
		return false;
	}
	if (g_strSpawnedText != g_strSpawnedExpected)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[PhysicsGym] '%s' reads \"%s\", expected \"%s\" -- the cross-entity SetUIText "
			"never reached the HUD through '%s'",
			ScriptTest::UINames::szSPAWNED, g_strSpawnedText.c_str(), g_strSpawnedExpected.c_str(),
			ScriptTest::Vars::szUI_TARGET);
		return false;
	}
	if (g_iLiveBallPeak > iST_PHYSICS_MAX_LIVE)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[PhysicsGym] live balls peaked at %d (limit %d) -- balls are not being destroyed",
			g_iLiveBallPeak, iST_PHYSICS_MAX_LIVE);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xPhysicsGymTest = {
	"ST_PhysicsGym_Test",
	&Setup_PhysicsGym,
	&Step_PhysicsGym,
	&Verify_PhysicsGym,
	// 600 run + up to 121 waiting for a timer spawn + 4 waiting for the key,
	// plus the scene wait. 900 leaves both new timeouts room to REPORT.
	/*maxFrames*/ 900,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPhysicsGymTest);

// ============================================================================
// ST_MotionGym_Test (C11)
// ----------------------------------------------------------------------------
// Three transform drivers, three different mechanisms: RotateEntity scaling by
// the dispatched dt, a Repeat/TweenPosition/WaitForTween loop, and a
// blackboard cos() feeding TranslateEntity.
//
// ★ THE TWO TRAVELLING PROBES TRACK MIN/MAX RATHER THAN DIFFING TWO SAMPLES.
// The ping-pong's legs are 1.5 s (90 frames) each, so ANY fixed sample spacing
// can straddle an endpoint symmetrically and read a delta near zero from a
// perfectly working tween -- the tighter assertion is the observed EXCURSION
// over a window longer than a leg, which no stalled entity can produce. The
// same reasoning covers the bob (period pi s). Only the spinner, whose motion
// is monotonic, is sampled as a pair.
// ============================================================================

namespace
{
	constexpr int iST_MOTION_SPIN_GAP    = 45;	// 0.75 s of a 45 deg/s yaw = 33.75 deg
	constexpr int iST_MOTION_SAMPLE_SPAN = 240;	// 4 s: > 2 ping-pong legs, > 1 bob period

	enum class MotionPhase { Boot, WaitScene, Sample, Done };

	MotionPhase        g_eMotionPhase = MotionPhase::Boot;
	int                g_iMotionFrame = 0;
	Zenith_Maths::Quat g_xSpinFirst(1.0f, 0.0f, 0.0f, 0.0f);
	Zenith_Maths::Quat g_xSpinSecond(1.0f, 0.0f, 0.0f, 0.0f);
	float              g_fPingPongMinX = 0.0f;
	float              g_fPingPongMaxX = 0.0f;
	float              g_fBobberMinY = 0.0f;
	float              g_fBobberMaxY = 0.0f;
	bool               g_bMotionReady = false;
	bool               g_bSpinSampled = false;
	bool               g_bMotionDone = false;
}

static void Setup_MotionGym()
{
	g_eMotionPhase = MotionPhase::Boot;
	g_iMotionFrame = 0;
	g_xSpinFirst = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
	g_xSpinSecond = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
	g_fPingPongMinX = 1.0e30f;
	g_fPingPongMaxX = -1.0e30f;
	g_fBobberMinY = 1.0e30f;
	g_fBobberMaxY = -1.0e30f;
	g_bMotionReady = false;
	g_bSpinSampled = false;
	g_bMotionDone = false;
}

static bool Step_MotionGym(int iFrame)
{
	switch (g_eMotionPhase)
	{
	case MotionPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(ScriptTest::Scenes::iGYM_MOTION, SCENE_LOAD_SINGLE);
		g_eMotionPhase = MotionPhase::WaitScene;
		return true;

	case MotionPhase::WaitScene:
		if (ST_IsSceneActive(ScriptTest::Scenes::iGYM_MOTION)
			&& ST_EntityExists(ScriptTest::Entities::szSPINNER)
			&& ST_EntityExists(ScriptTest::Entities::szPING_PONG)
			&& ST_EntityExists(ScriptTest::Entities::szBOBBER))
		{
			g_bMotionReady = true;
			g_iMotionFrame = 0;
			g_eMotionPhase = MotionPhase::Sample;
			return true;
		}
		return iFrame < 180;

	case MotionPhase::Sample:
	{
		if (g_iMotionFrame == 0)
		{
			ST_GetRotation(ScriptTest::Entities::szSPINNER, g_xSpinFirst);
		}
		else if (g_iMotionFrame == iST_MOTION_SPIN_GAP)
		{
			g_bSpinSampled = ST_GetRotation(ScriptTest::Entities::szSPINNER, g_xSpinSecond);
		}

		Zenith_Maths::Vector3 xPosition(0.0f);
		if (ST_GetPosition(ScriptTest::Entities::szPING_PONG, xPosition))
		{
			g_fPingPongMinX = xPosition.x < g_fPingPongMinX ? xPosition.x : g_fPingPongMinX;
			g_fPingPongMaxX = xPosition.x > g_fPingPongMaxX ? xPosition.x : g_fPingPongMaxX;
		}
		if (ST_GetPosition(ScriptTest::Entities::szBOBBER, xPosition))
		{
			g_fBobberMinY = xPosition.y < g_fBobberMinY ? xPosition.y : g_fBobberMinY;
			g_fBobberMaxY = xPosition.y > g_fBobberMaxY ? xPosition.y : g_fBobberMaxY;
		}

		if (++g_iMotionFrame >= iST_MOTION_SAMPLE_SPAN)
		{
			g_bMotionDone = true;
			g_eMotionPhase = MotionPhase::Done;
			return false;
		}
		return true;
	}

	case MotionPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_MotionGym()
{
	if (!g_bMotionReady)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[MotionGym] Gym_Motion never became active with its three props");
		return false;
	}
	if (!g_bMotionDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[MotionGym] never completed (phase %d)", static_cast<int>(g_eMotionPhase));
		return false;
	}
	if (!g_bSpinSampled)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[MotionGym] the spinner's second rotation sample was never taken");
		return false;
	}

	// |dot| == 1 is the same orientation (quaternion double cover), so a
	// spinner that never turned reads 1.0 here. 33.75 deg of yaw reads ~0.957.
	// Spelled component-wise rather than through glm::dot so the check does not
	// depend on which quaternion header happens to be transitively visible.
	const float fAlignment = std::abs(
		g_xSpinFirst.w * g_xSpinSecond.w +
		g_xSpinFirst.x * g_xSpinSecond.x +
		g_xSpinFirst.y * g_xSpinSecond.y +
		g_xSpinFirst.z * g_xSpinSecond.z);
	if (fAlignment > 0.999f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[MotionGym] the spinner did not rotate over %d frames (|dot| %.5f)",
			iST_MOTION_SPIN_GAP, fAlignment);
		return false;
	}

	const float fPingPongSpan = g_fPingPongMaxX - g_fPingPongMinX;
	if (fPingPongSpan <= 2.0f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[MotionGym] PingPong X excursion %.3f over %d frames, expected > 2.0 (x %.3f..%.3f)",
			fPingPongSpan, iST_MOTION_SAMPLE_SPAN, g_fPingPongMinX, g_fPingPongMaxX);
		return false;
	}

	const float fBobSpan = g_fBobberMaxY - g_fBobberMinY;
	if (fBobSpan <= 0.2f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[MotionGym] Bobber Y excursion %.3f over %d frames, expected > 0.2 (y %.3f..%.3f)",
			fBobSpan, iST_MOTION_SAMPLE_SPAN, g_fBobberMinY, g_fBobberMaxY);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xMotionGymTest = {
	"ST_MotionGym_Test",
	&Setup_MotionGym,
	&Step_MotionGym,
	&Verify_MotionGym,
	/*maxFrames*/ 460,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xMotionGymTest);

// ============================================================================
// ST_StateGym_Test (C12a)
// ----------------------------------------------------------------------------
// Red holds for 3 s and then writes light = 1. That alone would pass with the
// lamps unwired, so the test also reads both lamps: Enter_Green must have
// scaled Lamp_Green to 1.4 and Exit_Red must have put Lamp_Red back to 1.0.
//
// ★ THE TRANSITION EVENTS LAG THE VARIABLE BY ONE DISPATCH. The Red body's
// SetBlackboardInt writes light on frame N; the StateMachine only NOTICES on
// its next fire (frame N+1), and that is when Exit_Red / Enter_Green run. So
// the lamps are read after a grace window, not on the frame light flips --
// reading them immediately would find Lamp_Green still at its authored 1.0.
// ============================================================================

namespace
{
	constexpr u_int iST_STATE_TRAFFIC_SLOT = 1u;	// slot 0 is ST_EscToHub
	constexpr int   iST_STATE_POLL_LIMIT   = 300;	// Red is 3 s = 180 frames
	constexpr int   iST_STATE_LAMP_GRACE   = 6;
	constexpr int32_t iST_LIGHT_GREEN      = 1;		// pin 1 of "Red,Green,Amber"

	enum class StatePhase { Boot, WaitScene, AwaitGreen, LampGrace, Done };

	StatePhase g_eStatePhase   = StatePhase::Boot;
	int        g_iStateFrame   = 0;
	int32_t    g_iLightSeen    = -1;
	float      g_fGreenLampX   = 0.0f;
	float      g_fRedLampX     = 0.0f;
	bool       g_bStateReady   = false;
	bool       g_bSawGreen     = false;
	bool       g_bLampsRead    = false;
}

static void Setup_StateGym()
{
	g_eStatePhase = StatePhase::Boot;
	g_iStateFrame = 0;
	g_iLightSeen = -1;
	g_fGreenLampX = 0.0f;
	g_fRedLampX = 0.0f;
	g_bStateReady = false;
	g_bSawGreen = false;
	g_bLampsRead = false;
}

static bool Step_StateGym(int iFrame)
{
	switch (g_eStatePhase)
	{
	case StatePhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(ScriptTest::Scenes::iGYM_STATE, SCENE_LOAD_SINGLE);
		g_eStatePhase = StatePhase::WaitScene;
		return true;

	case StatePhase::WaitScene:
		if (ST_IsSceneActive(ScriptTest::Scenes::iGYM_STATE)
			&& ST_EntityExists(ScriptTest::Entities::szLAMP_RED)
			&& ST_EntityExists(ScriptTest::Entities::szLAMP_GREEN)
			&& ST_Blackboard(ScriptTest::Entities::szGAME_MANAGER, iST_STATE_TRAFFIC_SLOT) != nullptr)
		{
			g_bStateReady = true;
			g_iStateFrame = 0;
			g_eStatePhase = StatePhase::AwaitGreen;
			return true;
		}
		return iFrame < 180;

	case StatePhase::AwaitGreen:
		g_iLightSeen = ST_ReadInt(
			ScriptTest::Entities::szGAME_MANAGER, iST_STATE_TRAFFIC_SLOT, ScriptTest::Vars::szLIGHT, -1);
		if (g_iLightSeen == iST_LIGHT_GREEN)
		{
			g_bSawGreen = true;
			g_iStateFrame = 0;
			g_eStatePhase = StatePhase::LampGrace;
			return true;
		}
		if (++g_iStateFrame > iST_STATE_POLL_LIMIT)
		{
			g_eStatePhase = StatePhase::Done;
			return false;
		}
		return true;

	case StatePhase::LampGrace:
	{
		if (++g_iStateFrame < iST_STATE_LAMP_GRACE)
		{
			return true;
		}
		Zenith_Maths::Vector3 xScale(0.0f);
		const bool bGreen = ST_GetScale(ScriptTest::Entities::szLAMP_GREEN, xScale);
		g_fGreenLampX = bGreen ? xScale.x : 0.0f;
		const bool bRed = ST_GetScale(ScriptTest::Entities::szLAMP_RED, xScale);
		g_fRedLampX = bRed ? xScale.x : 0.0f;
		g_bLampsRead = bGreen && bRed;
		g_eStatePhase = StatePhase::Done;
		return false;
	}

	case StatePhase::Done:
		return false;
	}
	return false;
}

static bool Verify_StateGym()
{
	if (!g_bStateReady)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[StateGym] Gym_State never became active with its lamps + traffic graph");
		return false;
	}
	if (!g_bSawGreen)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[StateGym] '%s' never reached %d within %d frames (last %d)",
			ScriptTest::Vars::szLIGHT, iST_LIGHT_GREEN, iST_STATE_POLL_LIMIT, g_iLightSeen);
		return false;
	}
	if (!g_bLampsRead)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[StateGym] a lamp transform could not be read after the transition");
		return false;
	}
	// Enter_Green writes an ABSOLUTE 1.4 scale, so this cannot be satisfied by
	// the authored 1.0.
	if (g_fGreenLampX <= 1.3f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[StateGym] Enter_Green never scaled %s (scale.x %.3f, expected > 1.3)",
			ScriptTest::Entities::szLAMP_GREEN, g_fGreenLampX);
		return false;
	}
	// Exit_Red writes an ABSOLUTE 1.0 back.
	if (std::abs(g_fRedLampX - 1.0f) > 0.05f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[StateGym] Exit_Red left %s at scale.x %.3f, expected ~1.0",
			ScriptTest::Entities::szLAMP_RED, g_fRedLampX);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xStateGymTest = {
	"ST_StateGym_Test",
	&Setup_StateGym,
	&Step_StateGym,
	&Verify_StateGym,
	/*maxFrames*/ 560,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xStateGymTest);

// ============================================================================
// ST_UIGym_Test (C12b)
// ----------------------------------------------------------------------------
// Two halves of ST_UIPlayground:
//   * three INPUT sources (BtnPlus, BtnMinus, Up) each own a private copy of
//     the bump chain and all three write one blackboard int -- +1, -1, +1;
//   * the per-frame clock -> modulo -> divide -> compare chain, whose 'hot'
//     bool must be false early in a 5 s cycle and true past 80% of it.
//
// ★ ONE CLICK PER TICK, MAXIMUM. OnUIButtonClicked compares the click counter
// against what it saw on its LAST fire, so two Activate() calls inside one
// frame are a single edge and would read as count == 1. Every click here is
// separated by a gap, and the gap doubles as the deferred-fire window.
//
// The hot boundary is read from the same frame's clock: the whole chain --
// advance, modulo, divide, compare -- runs in one OnUpdate, so clock and hot
// are always consistent with each other.
//
// ★ AND THE COUNTER'S TEXT, WHICH IS THE ONLY SetUIText THIS FILE READS BESIDES
// ST_PhysicsGym_Test'S. The bump chain is AddBlackboardInt -> SetUIText, and the
// SetUIText is chain-TERMINAL: a typo'd element name makes the node return
// FAILURE with nothing downstream to notice, so the blackboard int would keep
// its exact expected value while the readout stayed frozen on its authored seed.
// Reading the element's string is what closes that.
//
// ★ THE SAME ARGUMENT, TWICE OVER, FOR THE BAR'S COLOUR. Both SetUIColor nodes
// hang off the Branch's true/false pins with nothing after them, so the 'hot'
// flag asserted above proves only that the Branch had the right INPUT -- the two
// pins could reach nothing at all and every other check here would still pass.
// So the colour is sampled on the SAME frames as the flag and checked three
// ways: the two samples must DIFFER (a frozen bar fails), and each must match
// the constant ScriptTest_Graphs.h hands the builder (a mis-wired pin fails).
// ============================================================================

namespace
{
	constexpr u_int iST_UI_PLAYGROUND_SLOT = 1u;	// slot 0 is ST_EscToHub
	constexpr int   iST_UI_WARM_FRAMES     = 4;		// trampoline install + latch
	constexpr int   iST_UI_ACT_GAP         = 5;		// >= 1 tick between clicks
	constexpr int   iST_UI_SAMPLE_FRAMES   = 30;
	constexpr float fST_UI_COOL_CLOCK      = 1.0f;	// fill01 = 0.2 -> not hot
	constexpr float fST_UI_HOT_CLOCK       = 4.6f;	// fill01 = 0.92 -> hot
	constexpr int   iST_UI_CLOCK_POLL      = 420;	// 4.6 s = 276 frames, plus slack
	constexpr float fST_UI_COLOUR_EPSILON  = 1e-4f;	// float32 the whole way; see ST_ColourNear

	enum class UIPhase { Boot, WaitScene, Warm, Act, Sample, AwaitCool, AwaitHot, Done };

	UIPhase g_eUIPhase      = UIPhase::Boot;
	int     g_iUIFrame      = 0;
	int     g_iUIAct        = 0;
	int32_t g_iCountAfterPlus  = -1;
	int32_t g_iCountAfterMinus = -1;
	int32_t g_iCountAfterUp    = -1;
	float   g_fUIClockSample   = -1.0f;
	float   g_fUIFillSample    = -1.0f;
	std::string g_strUICounterText;
	bool    g_bUICounterTextRead = false;
	bool    g_bUIReady      = false;
	bool    g_bUIClickFailed = false;
	bool    g_bHotAtCool    = true;
	bool    g_bHotAtHot     = false;
	bool    g_bSawCool      = false;
	bool    g_bSawHot       = false;
	Zenith_Maths::Vector4 g_xBarColourAtCool = Zenith_Maths::Vector4(0.0f);
	Zenith_Maths::Vector4 g_xBarColourAtHot  = Zenith_Maths::Vector4(0.0f);
	bool    g_bBarColourReadCool = false;
	bool    g_bBarColourReadHot  = false;

	int32_t ST_ReadUICount()
	{
		return ST_ReadInt(
			ScriptTest::Entities::szGAME_MANAGER, iST_UI_PLAYGROUND_SLOT, ScriptTest::Vars::szCOUNT, -1);
	}

	float ST_ReadUIClock()
	{
		return ST_ReadFloat(
			ScriptTest::Entities::szGAME_MANAGER, iST_UI_PLAYGROUND_SLOT, ScriptTest::Vars::szCLOCK, -1.0f);
	}
}

static void Setup_UIGym()
{
	g_eUIPhase = UIPhase::Boot;
	g_iUIFrame = 0;
	g_iUIAct = 0;
	g_iCountAfterPlus = -1;
	g_iCountAfterMinus = -1;
	g_iCountAfterUp = -1;
	g_fUIClockSample = -1.0f;
	g_fUIFillSample = -1.0f;
	g_strUICounterText.clear();
	g_bUICounterTextRead = false;
	g_bUIReady = false;
	g_bUIClickFailed = false;
	g_bHotAtCool = true;
	g_bHotAtHot = false;
	g_bSawCool = false;
	g_bSawHot = false;
	g_xBarColourAtCool = Zenith_Maths::Vector4(0.0f);
	g_xBarColourAtHot = Zenith_Maths::Vector4(0.0f);
	g_bBarColourReadCool = false;
	g_bBarColourReadHot = false;
}

static bool Step_UIGym(int iFrame)
{
	switch (g_eUIPhase)
	{
	case UIPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(ScriptTest::Scenes::iGYM_UI, SCENE_LOAD_SINGLE);
		g_eUIPhase = UIPhase::WaitScene;
		return true;

	case UIPhase::WaitScene:
		if (ST_IsSceneActive(ScriptTest::Scenes::iGYM_UI)
			&& ST_FindButton(ScriptTest::UINames::szBTN_PLUS) != nullptr
			&& ST_FindButton(ScriptTest::UINames::szBTN_MINUS) != nullptr
			&& ST_Blackboard(ScriptTest::Entities::szGAME_MANAGER, iST_UI_PLAYGROUND_SLOT) != nullptr)
		{
			g_bUIReady = true;
			g_iUIFrame = 0;
			g_eUIPhase = UIPhase::Warm;
			return true;
		}
		return iFrame < 180;

	case UIPhase::Warm:
		if (++g_iUIFrame >= iST_UI_WARM_FRAMES)
		{
			g_iUIFrame = 0;
			g_iUIAct = 0;
			g_eUIPhase = UIPhase::Act;
		}
		return true;

	case UIPhase::Act:
	{
		if (++g_iUIFrame < iST_UI_ACT_GAP)
		{
			return true;
		}
		g_iUIFrame = 0;

		bool bClicked = true;
		switch (g_iUIAct)
		{
		case 0:
			bClicked = ST_ClickButton(ScriptTest::UINames::szBTN_PLUS);
			break;
		case 1:
			bClicked = ST_ClickButton(ScriptTest::UINames::szBTN_PLUS);
			break;
		case 2:
			g_iCountAfterPlus = ST_ReadUICount();
			bClicked = ST_ClickButton(ScriptTest::UINames::szBTN_MINUS);
			break;
		case 3:
			g_iCountAfterMinus = ST_ReadUICount();
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_UP);
			break;
		default:
			g_iCountAfterUp = ST_ReadUICount();
			// The count has reached its final value here (two up, one down, one
			// key), and the SAME Step frame reads what the bump chain's terminal
			// SetUIText painted from it.
			g_bUICounterTextRead = ST_ReadUIText(ScriptTest::UINames::szCOUNTER, g_strUICounterText);
			g_iUIFrame = 0;
			g_eUIPhase = UIPhase::Sample;
			return true;
		}

		if (!bClicked)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[UIGym] a playground button vanished mid-test (act %d)", g_iUIAct);
			g_bUIClickFailed = true;
			g_eUIPhase = UIPhase::Done;
			return false;
		}
		++g_iUIAct;
		return true;
	}

	case UIPhase::Sample:
		if (++g_iUIFrame < iST_UI_SAMPLE_FRAMES)
		{
			return true;
		}
		g_fUIClockSample = ST_ReadUIClock();
		g_fUIFillSample = ST_ReadFloat(
			ScriptTest::Entities::szGAME_MANAGER, iST_UI_PLAYGROUND_SLOT, ScriptTest::Vars::szFILL01, -1.0f);
		g_iUIFrame = 0;
		g_eUIPhase = UIPhase::AwaitCool;
		return true;

	case UIPhase::AwaitCool:
		if (ST_ReadUIClock() >= fST_UI_COOL_CLOCK)
		{
			g_bSawCool = true;
			g_bHotAtCool = ST_ReadBool(
				ScriptTest::Entities::szGAME_MANAGER, iST_UI_PLAYGROUND_SLOT, ScriptTest::Vars::szHOT, true);
			// Same frame as the flag, deliberately: advance -> modulo -> divide ->
			// compare -> Branch -> SetUIColor is ONE OnUpdate chain, so the colour on
			// the element and the 'hot' bool that selected it are always consistent
			// with each other and with the clock this phase polled.
			g_bBarColourReadCool = ST_ReadUIColor(ScriptTest::UINames::szBAR_FILL, g_xBarColourAtCool);
			g_iUIFrame = 0;
			g_eUIPhase = UIPhase::AwaitHot;
			return true;
		}
		if (++g_iUIFrame > iST_UI_CLOCK_POLL)
		{
			g_eUIPhase = UIPhase::Done;
			return false;
		}
		return true;

	case UIPhase::AwaitHot:
		if (ST_ReadUIClock() >= fST_UI_HOT_CLOCK)
		{
			g_bSawHot = true;
			g_bHotAtHot = ST_ReadBool(
				ScriptTest::Entities::szGAME_MANAGER, iST_UI_PLAYGROUND_SLOT, ScriptTest::Vars::szHOT, false);
			g_bBarColourReadHot = ST_ReadUIColor(ScriptTest::UINames::szBAR_FILL, g_xBarColourAtHot);
			g_eUIPhase = UIPhase::Done;
			return false;
		}
		if (++g_iUIFrame > iST_UI_CLOCK_POLL)
		{
			g_eUIPhase = UIPhase::Done;
			return false;
		}
		return true;

	case UIPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_UIGym()
{
	if (!g_bUIReady)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[UIGym] Gym_UI never became active with its buttons + playground graph");
		return false;
	}
	if (g_bUIClickFailed)
	{
		return false;
	}
	if (g_iCountAfterPlus != 2)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[UIGym] '%s' after two BtnPlus clicks = %d, expected 2",
			ScriptTest::Vars::szCOUNT, g_iCountAfterPlus);
		return false;
	}
	if (g_iCountAfterMinus != 1)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[UIGym] '%s' after BtnMinus = %d, expected 1",
			ScriptTest::Vars::szCOUNT, g_iCountAfterMinus);
		return false;
	}
	if (g_iCountAfterUp != 2)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[UIGym] '%s' after the Up key = %d, expected 2",
			ScriptTest::Vars::szCOUNT, g_iCountAfterUp);
		return false;
	}
	if (!g_bUICounterTextRead)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[UIGym] the '%s' readout is not a text element on the %s canvas",
			ScriptTest::UINames::szCOUNTER, ScriptTest::Entities::szGAME_MANAGER);
		return false;
	}
	// The RESULT of the bump chain's SetUIText, whose format string lives in the
	// builder (ST_BuildCounterBumpChain in ScriptTest.cpp) as "Count: {}" over
	// Vars::szCOUNT: "{}" is replaced by the value and an int32 renders through
	// "%d", so a count of 2 paints exactly this. Only the outcome is restated
	// here -- the format is the builder's to own.
	if (g_strUICounterText != "Count: 2")
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[UIGym] '%s' reads \"%s\", expected \"Count: 2\" -- '%s' is %d, so the terminal "
			"SetUIText never repainted the readout",
			ScriptTest::UINames::szCOUNTER, g_strUICounterText.c_str(),
			ScriptTest::Vars::szCOUNT, g_iCountAfterUp);
		return false;
	}
	if (!(g_fUIClockSample > 0.0f))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[UIGym] '%s' never advanced (%.4f)",
			ScriptTest::Vars::szCLOCK, g_fUIClockSample);
		return false;
	}
	if (!(g_fUIFillSample > 0.0f) || g_fUIFillSample > 1.0f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[UIGym] '%s' = %.4f, expected (0, 1]",
			ScriptTest::Vars::szFILL01, g_fUIFillSample);
		return false;
	}
	if (!g_bSawCool || !g_bSawHot)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[UIGym] the clock never reached both sample points (cool %d, hot %d, last clock %.3f)",
			g_bSawCool ? 1 : 0, g_bSawHot ? 1 : 0, ST_ReadUIClock());
		return false;
	}
	if (g_bHotAtCool)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[UIGym] '%s' was true at clock ~%.1f s, expected false",
			ScriptTest::Vars::szHOT, fST_UI_COOL_CLOCK);
		return false;
	}
	if (!g_bHotAtHot)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[UIGym] '%s' was false at clock ~%.1f s, expected true",
			ScriptTest::Vars::szHOT, fST_UI_HOT_CLOCK);
		return false;
	}
	// --- and the two terminal SetUIColor nodes -----------------------------
	// The flag above only proves the Branch had the right INPUT. These four
	// checks prove the Branch's two pins reached their nodes and each wrote its
	// own authored colour: both reads succeeded, the two samples DIFFER (so the
	// bar is not simply frozen on whatever the recipe left it), and each matches
	// the constant its builder used.
	if (!g_bBarColourReadCool || !g_bBarColourReadHot)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[UIGym] the '%s' bar is not readable on the %s canvas (cool %d, hot %d)",
			ScriptTest::UINames::szBAR_FILL, ScriptTest::Entities::szGAME_MANAGER,
			g_bBarColourReadCool ? 1 : 0, g_bBarColourReadHot ? 1 : 0);
		return false;
	}
	if (ST_ColourNear(g_xBarColourAtCool, g_xBarColourAtHot, fST_UI_COLOUR_EPSILON))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[UIGym] '%s' is the same colour either side of the hot boundary "
			"(%.3f %.3f %.3f %.3f) -- the Branch's two SetUIColor pins are not both wired",
			ScriptTest::UINames::szBAR_FILL,
			g_xBarColourAtCool.x, g_xBarColourAtCool.y, g_xBarColourAtCool.z, g_xBarColourAtCool.w);
		return false;
	}
	if (!ST_ColourNear(g_xBarColourAtCool, ScriptTest::Colours::xBAR_COOL, fST_UI_COLOUR_EPSILON))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[UIGym] '%s' at clock ~%.1f s reads (%.3f %.3f %.3f %.3f), expected the COOL colour "
			"(%.3f %.3f %.3f %.3f)",
			ScriptTest::UINames::szBAR_FILL, fST_UI_COOL_CLOCK,
			g_xBarColourAtCool.x, g_xBarColourAtCool.y, g_xBarColourAtCool.z, g_xBarColourAtCool.w,
			ScriptTest::Colours::xBAR_COOL.x, ScriptTest::Colours::xBAR_COOL.y,
			ScriptTest::Colours::xBAR_COOL.z, ScriptTest::Colours::xBAR_COOL.w);
		return false;
	}
	if (!ST_ColourNear(g_xBarColourAtHot, ScriptTest::Colours::xBAR_HOT, fST_UI_COLOUR_EPSILON))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[UIGym] '%s' at clock ~%.1f s reads (%.3f %.3f %.3f %.3f), expected the HOT colour "
			"(%.3f %.3f %.3f %.3f)",
			ScriptTest::UINames::szBAR_FILL, fST_UI_HOT_CLOCK,
			g_xBarColourAtHot.x, g_xBarColourAtHot.y, g_xBarColourAtHot.z, g_xBarColourAtHot.w,
			ScriptTest::Colours::xBAR_HOT.x, ScriptTest::Colours::xBAR_HOT.y,
			ScriptTest::Colours::xBAR_HOT.z, ScriptTest::Colours::xBAR_HOT.w);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xUIGymTest = {
	"ST_UIGym_Test",
	&Setup_UIGym,
	&Step_UIGym,
	&Verify_UIGym,
	/*maxFrames*/ 700,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xUIGymTest);

// ============================================================================
// ST_FlowGym_Test (C13)
// ----------------------------------------------------------------------------
// Gym_Flow is the only scene that reaches the MULTI-WAY flow constructs, and
// this is the only thing that can see them work. Every row below is a distinct
// observable: a row an unwired graph could also satisfy is not a row.
//
//   Once .............. bonus == 1 EXACTLY. Not >= 1 -- "at least once" would
//                       pass on a node that fired every frame.
//   Cooldown .......... two presses one frame apart raise the counter by ONE,
//                       and a third press a second later raises it again. The
//                       second half is what separates a throttle from a
//                       permanently-closed gate.
//   Gate .............. a press before the plate arms it changes nothing.
//   WaitForCondition .. 'armed' stays false while 'ready' is false and flips
//                       within a few frames of the plate's event -- a one-shot
//                       OnStart chain that waited, which only works because the
//                       ON_UPDATE dispatch re-drives suspended anchors.
//   SwitchOnInt ....... mode 0/1/2 pick their label AND their own nozzle scale;
//                       mode 3 takes the DEFAULT pin. The scale is what proves
//                       WHICH pin ran.
//   SwitchOnString .... the label maps back to 0/1/2, and an unlisted one to -1
//                       through its default pin.
//   Selector .......... with no alarm, normalRuns climbs and alarmRuns is zero.
//                       With one, alarmRuns climbs AND normalRuns FREEZES --
//                       preemption has no other signature.
//   ForEach ........... one pass over a 3-element bag: visited == 3, idx == 2.
//   ListAdd/Count ..... the bag tracks the dispense counter.
//   ListRemoveAt ...... the count drops by one AND the new head is the FORMER
//                       SECOND element. A swap-remove would leave the last one
//                       there instead, which is the only way to tell them apart.
//   ListClear ......... the count is zero, and the GetListElement after it FAILS
//                       -- observable only as a sentinel the aborted chain never
//                       wrote, because a graph has no status variable.
//   LogicBlackboardBool the full 2x2 of (armed, jammed) -> canDispense, computed
//                       as NOT then N-ary AND.
//   CallGraph ......... 'score' rises 10 per dispense, and 'score' is the
//                       CALLER'S variable -- which is the shared-blackboard
//                       contract. A child with its own board leaves it at 0.
//
// ★ THE COOLDOWN WINDOWS ARE FRAME COUNTS, AND THAT IS SOUND. Cooldown reads
// Zenith_GraphContext::m_fTimeSeconds, which is FrameContext::GetTimePassed() --
// an ACCUMULATION of dt, not a wall clock. The harness pins dt at 1/60 across
// every Step, so 0.75 s is exactly 45 frames here however fast the machine runs.
// The 55-frame lead-ins below are that plus a wide margin.
//
// ★ AND THE FIRST, GATED PRESS BURNS THE COOLDOWN. Chain 4 is Cooldown THEN
// Gate, so a press that the gate rejects has still passed the throttle. Every
// dispense below therefore waits out a full window, including the first.
// ============================================================================

namespace
{
	constexpr u_int iST_FLOW_DISPENSER_SLOT = 1u;	// slot 0 is ST_EscToHub

	// 0.75 s = 45 frames at the harness's pinned dt, plus margin.
	constexpr int iST_FLOW_COOLDOWN_LEAD = 55;
	// Long enough for both OnUpdate switch chains and the HUD chain to settle.
	constexpr int iST_FLOW_SETTLE        = 8;

	int32_t ST_FlowInt(const char* szVar, int32_t iDefault)
	{
		return ST_ReadInt(ScriptTest::Entities::szGAME_MANAGER, iST_FLOW_DISPENSER_SLOT, szVar, iDefault);
	}

	bool ST_FlowBool(const char* szVar, bool bDefault)
	{
		return ST_ReadBool(ScriptTest::Entities::szGAME_MANAGER, iST_FLOW_DISPENSER_SLOT, szVar, bDefault);
	}

	std::string ST_FlowString(const char* szVar)
	{
		return ST_ReadString(ScriptTest::Entities::szGAME_MANAGER, iST_FLOW_DISPENSER_SLOT, szVar, "<unset>");
	}

	float ST_FlowNozzleScale()
	{
		Zenith_Maths::Vector3 xScale(0.0f);
		return ST_GetScale(ScriptTest::Entities::szNOZZLE, xScale) ? xScale.y : -1.0f;
	}

	// One SwitchOnInt outcome: what the int switch wrote, what the string switch
	// made of it, and what the world looks like as a result.
	struct ST_FlowModeSample
	{
		std::string m_strLabel = "<unsampled>";
		int32_t     m_iLabelIndex = -99;
		float       m_fNozzleScale = -1.0f;
	};

	ST_FlowModeSample g_axFlowModes[4];

	int32_t g_iFlowBonusEarly       = -1;
	int32_t g_iFlowBonusLate        = -1;
	int32_t g_iFlowDispensedUnarmed = -1;
	int32_t g_iFlowDispensedDouble  = -1;
	int32_t g_iFlowDispensedSecond  = -1;
	int32_t g_iFlowDispensedThird   = -1;
	int32_t g_iFlowBagAfterFill     = -1;
	int32_t g_iFlowScoreAfterFill   = -1;
	int32_t g_iFlowVisited          = -1;
	int32_t g_iFlowLastIdx          = -99;
	int32_t g_iFlowBagAfterDrop     = -1;
	int32_t g_iFlowHeadAfterDrop    = -99;
	int32_t g_iFlowBagAfterClear    = -1;
	int32_t g_iFlowSentinel         = -99;
	int32_t g_iFlowNormalA          = -1;
	int32_t g_iFlowNormalB          = -1;
	int32_t g_iFlowNormalC          = -1;
	int32_t g_iFlowAlarmA           = -1;
	int32_t g_iFlowAlarmB           = -1;
	int32_t g_iFlowAlarmC           = -1;
	bool    g_bFlowArmedBeforePlate = true;
	bool    g_bFlowArmedAfterPlate  = false;
	// The (armed, jammed) truth table, in the order the stages walk it.
	bool    g_abFlowCanDispense[4]  = { true, false, true, true };

	bool    g_bFlowReady            = false;
	bool    g_bFlowNozzleMissing    = false;

	void ST_FlowSampleMode(u_int uMode)
	{
		ST_FlowModeSample& xSample = g_axFlowModes[uMode];
		xSample.m_strLabel = ST_FlowString(ScriptTest::Vars::szLABEL);
		xSample.m_iLabelIndex = ST_FlowInt(ScriptTest::Vars::szLABEL_INDEX, -99);
		xSample.m_fNozzleScale = ST_FlowNozzleScale();
		if (xSample.m_fNozzleScale < 0.0f)
		{
			g_bFlowNozzleMissing = true;
		}
	}

	// --- the script ---------------------------------------------------------
	// A flat, ordered list rather than a phase enum: this test drives thirty-one
	// steps and a switch over thirty-one states would be the same table written
	// less legibly. Each row waits m_iLeadFrames, then runs its body once.
	struct ST_FlowStage
	{
		int  m_iLeadFrames;
		void (*m_pfnBody)();
	};

	void ST_FlowPressSpace() { Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_SPACE); }
	void ST_FlowPressPlate() { Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_P); }
	void ST_FlowPressMode()  { Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_M); }
	void ST_FlowPressWalk()  { Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_F); }
	void ST_FlowPressDrop()  { Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_R); }
	void ST_FlowPressEmpty() { Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_C); }
	void ST_FlowPressAlarm() { Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_A); }
	void ST_FlowPressJam()   { Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_J); }
	void ST_FlowPressUnarm() { Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_U); }

	void ST_FlowSampleStart()
	{
		g_iFlowBonusEarly = ST_FlowInt(ScriptTest::Vars::szBONUS, -1);
		g_bFlowArmedBeforePlate = ST_FlowBool(ScriptTest::Vars::szARMED, true);
		g_abFlowCanDispense[0] = ST_FlowBool(ScriptTest::Vars::szCAN_DISPENSE, true);
		g_iFlowNormalA = ST_FlowInt(ScriptTest::Vars::szNORMAL_RUNS, -1);
		g_iFlowAlarmA = ST_FlowInt(ScriptTest::Vars::szALARM_RUNS, -1);
		ST_FlowSampleMode(0);
	}

	void ST_FlowSampleUnarmedPress()
	{
		g_iFlowDispensedUnarmed = ST_FlowInt(ScriptTest::Vars::szDISPENSED, -1);
	}

	void ST_FlowSampleArmed()
	{
		g_bFlowArmedAfterPlate = ST_FlowBool(ScriptTest::Vars::szARMED, false);
		g_abFlowCanDispense[1] = ST_FlowBool(ScriptTest::Vars::szCAN_DISPENSE, false);
	}

	void ST_FlowSampleDouble()  { g_iFlowDispensedDouble = ST_FlowInt(ScriptTest::Vars::szDISPENSED, -1); }
	void ST_FlowSampleSecond()  { g_iFlowDispensedSecond = ST_FlowInt(ScriptTest::Vars::szDISPENSED, -1); }

	void ST_FlowSampleThird()
	{
		g_iFlowDispensedThird = ST_FlowInt(ScriptTest::Vars::szDISPENSED, -1);
		g_iFlowBagAfterFill = ST_FlowInt(ScriptTest::Vars::szBAG_COUNT, -1);
		g_iFlowScoreAfterFill = ST_FlowInt(ScriptTest::Vars::szSCORE, -1);
	}

	void ST_FlowSampleWalk()
	{
		g_iFlowVisited = ST_FlowInt(ScriptTest::Vars::szVISITED, -1);
		g_iFlowLastIdx = ST_FlowInt(ScriptTest::Vars::szIDX, -99);
	}

	void ST_FlowSampleDrop()
	{
		g_iFlowBagAfterDrop = ST_FlowInt(ScriptTest::Vars::szBAG_COUNT, -1);
		g_iFlowHeadAfterDrop = ST_FlowInt(ScriptTest::Vars::szHEAD, -99);
	}

	void ST_FlowSampleClear()
	{
		g_iFlowBagAfterClear = ST_FlowInt(ScriptTest::Vars::szBAG_COUNT, -1);
		g_iFlowSentinel = ST_FlowInt(ScriptTest::Vars::szSENTINEL, -99);
	}

	void ST_FlowSampleMode1() { ST_FlowSampleMode(1); }
	void ST_FlowSampleMode2() { ST_FlowSampleMode(2); }
	void ST_FlowSampleMode3() { ST_FlowSampleMode(3); }

	void ST_FlowSampleAlarmB()
	{
		g_iFlowNormalB = ST_FlowInt(ScriptTest::Vars::szNORMAL_RUNS, -1);
		g_iFlowAlarmB = ST_FlowInt(ScriptTest::Vars::szALARM_RUNS, -1);
	}

	void ST_FlowSampleAlarmC()
	{
		g_iFlowNormalC = ST_FlowInt(ScriptTest::Vars::szNORMAL_RUNS, -1);
		g_iFlowAlarmC = ST_FlowInt(ScriptTest::Vars::szALARM_RUNS, -1);
	}

	void ST_FlowSampleJammed()  { g_abFlowCanDispense[2] = ST_FlowBool(ScriptTest::Vars::szCAN_DISPENSE, true); }

	void ST_FlowSampleUnarmed()
	{
		g_abFlowCanDispense[3] = ST_FlowBool(ScriptTest::Vars::szCAN_DISPENSE, true);
		g_iFlowBonusLate = ST_FlowInt(ScriptTest::Vars::szBONUS, -1);
	}

	const ST_FlowStage g_axFlowStages[] =
	{
		{ iST_FLOW_SETTLE,        &ST_FlowSampleStart },
		{ 2,                      &ST_FlowPressSpace },			// GATED: armed is false
		{ iST_FLOW_SETTLE,        &ST_FlowSampleUnarmedPress },
		{ 2,                      &ST_FlowPressPlate },
		{ iST_FLOW_SETTLE,        &ST_FlowSampleArmed },
		{ iST_FLOW_COOLDOWN_LEAD, &ST_FlowPressSpace },			// the gated press burned the window
		{ 1,                      &ST_FlowPressSpace },			// ...one frame later: swallowed
		{ iST_FLOW_SETTLE,        &ST_FlowSampleDouble },
		{ iST_FLOW_COOLDOWN_LEAD, &ST_FlowPressSpace },
		{ iST_FLOW_SETTLE,        &ST_FlowSampleSecond },
		{ iST_FLOW_COOLDOWN_LEAD, &ST_FlowPressSpace },
		{ iST_FLOW_SETTLE,        &ST_FlowSampleThird },
		{ 2,                      &ST_FlowPressWalk },
		{ iST_FLOW_SETTLE,        &ST_FlowSampleWalk },
		{ 2,                      &ST_FlowPressDrop },
		{ iST_FLOW_SETTLE,        &ST_FlowSampleDrop },
		{ 2,                      &ST_FlowPressEmpty },
		{ iST_FLOW_SETTLE,        &ST_FlowSampleClear },
		{ 2,                      &ST_FlowPressMode },
		{ iST_FLOW_SETTLE,        &ST_FlowSampleMode1 },
		{ 2,                      &ST_FlowPressMode },
		{ iST_FLOW_SETTLE,        &ST_FlowSampleMode2 },
		{ 2,                      &ST_FlowPressMode },
		{ iST_FLOW_SETTLE,        &ST_FlowSampleMode3 },
		{ 2,                      &ST_FlowPressAlarm },
		{ iST_FLOW_SETTLE,        &ST_FlowSampleAlarmB },
		{ 40,                     &ST_FlowSampleAlarmC },
		{ 2,                      &ST_FlowPressJam },
		{ iST_FLOW_SETTLE,        &ST_FlowSampleJammed },
		{ 2,                      &ST_FlowPressUnarm },
		{ iST_FLOW_SETTLE,        &ST_FlowSampleUnarmed },
	};

	constexpr u_int uST_FLOW_STAGES = static_cast<u_int>(sizeof(g_axFlowStages) / sizeof(g_axFlowStages[0]));

	enum class FlowPhase { Boot, WaitScene, Script, Done };

	FlowPhase g_eFlowPhase = FlowPhase::Boot;
	u_int     g_uFlowStage = 0;
	int       g_iFlowWait  = 0;
	bool      g_bFlowFinished = false;
}

static void Setup_FlowGym()
{
	g_eFlowPhase = FlowPhase::Boot;
	g_uFlowStage = 0;
	g_iFlowWait = 0;
	g_bFlowReady = false;
	g_bFlowFinished = false;
	g_bFlowNozzleMissing = false;

	for (u_int u = 0; u < 4u; ++u)
	{
		g_axFlowModes[u] = ST_FlowModeSample();
	}

	g_iFlowBonusEarly = -1;
	g_iFlowBonusLate = -1;
	g_iFlowDispensedUnarmed = -1;
	g_iFlowDispensedDouble = -1;
	g_iFlowDispensedSecond = -1;
	g_iFlowDispensedThird = -1;
	g_iFlowBagAfterFill = -1;
	g_iFlowScoreAfterFill = -1;
	g_iFlowVisited = -1;
	g_iFlowLastIdx = -99;
	g_iFlowBagAfterDrop = -1;
	g_iFlowHeadAfterDrop = -99;
	g_iFlowBagAfterClear = -1;
	g_iFlowSentinel = -99;
	g_iFlowNormalA = -1;
	g_iFlowNormalB = -1;
	g_iFlowNormalC = -1;
	g_iFlowAlarmA = -1;
	g_iFlowAlarmB = -1;
	g_iFlowAlarmC = -1;
	g_bFlowArmedBeforePlate = true;
	g_bFlowArmedAfterPlate = false;
	// Seeded to the values that FAIL, so a stage that never ran cannot pass by
	// leaving its slot at something plausible.
	g_abFlowCanDispense[0] = true;
	g_abFlowCanDispense[1] = false;
	g_abFlowCanDispense[2] = true;
	g_abFlowCanDispense[3] = true;
}

static bool Step_FlowGym(int iFrame)
{
	switch (g_eFlowPhase)
	{
	case FlowPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(ScriptTest::Scenes::iGYM_FLOW, SCENE_LOAD_SINGLE);
		g_eFlowPhase = FlowPhase::WaitScene;
		return true;

	case FlowPhase::WaitScene:
		// The Plate is part of the readiness gate: its graph is what arms the
		// dispenser, so a scene that loaded without it would make every later
		// stage fail for a reason this phase can name instead.
		if (ST_IsSceneActive(ScriptTest::Scenes::iGYM_FLOW)
			&& ST_EntityExists(ScriptTest::Entities::szPLATE)
			&& ST_EntityExists(ScriptTest::Entities::szNOZZLE)
			&& ST_Blackboard(ScriptTest::Entities::szGAME_MANAGER, iST_FLOW_DISPENSER_SLOT) != nullptr)
		{
			g_bFlowReady = true;
			g_iFlowWait = 0;
			g_eFlowPhase = FlowPhase::Script;
			return true;
		}
		return iFrame < 180;

	case FlowPhase::Script:
		if (++g_iFlowWait < g_axFlowStages[g_uFlowStage].m_iLeadFrames)
		{
			return true;
		}
		g_iFlowWait = 0;
		g_axFlowStages[g_uFlowStage].m_pfnBody();
		if (++g_uFlowStage >= uST_FLOW_STAGES)
		{
			g_bFlowFinished = true;
			g_eFlowPhase = FlowPhase::Done;
			return false;
		}
		return true;

	case FlowPhase::Done:
		return false;
	}
	return false;
}

namespace
{
	// Every mode expectation in one place: the label SwitchOnInt writes, the
	// index SwitchOnString derives from it, and the nozzle scale that identifies
	// the pin. Row 3 is the DEFAULT pin of both switches.
	struct ST_FlowModeExpectation
	{
		const char* m_szLabel;
		int32_t     m_iLabelIndex;
		float       m_fNozzleScale;
	};

	const ST_FlowModeExpectation g_axFlowModeExpectations[4] = {
		{ ScriptTest::Labels::szRED,   0,  1.0f },
		{ ScriptTest::Labels::szGREEN, 1,  1.4f },
		{ ScriptTest::Labels::szBLUE,  2,  1.8f },
		{ ScriptTest::Labels::szNONE, -1,  0.6f },	// default pins
	};
}

static bool Verify_FlowGym()
{
	if (!g_bFlowReady)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[FlowGym] Gym_Flow never became active with its Plate, Nozzle and dispenser graph");
		return false;
	}
	if (!g_bFlowFinished)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[FlowGym] the script stopped at stage %u of %u",
			g_uFlowStage, uST_FLOW_STAGES);
		return false;
	}

	// --- Once ---------------------------------------------------------------
	// EXACTLY one, at both ends of the run. ">= 1" would pass on a node that
	// fired every frame, which is the failure Once exists to prevent.
	if (g_iFlowBonusEarly != 1 || g_iFlowBonusLate != 1)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[FlowGym] '%s' = %d early / %d late, expected exactly 1 both times -- Once let its chain through more than once",
			ScriptTest::Vars::szBONUS, g_iFlowBonusEarly, g_iFlowBonusLate);
		return false;
	}

	// --- WaitForCondition + Gate --------------------------------------------
	if (g_bFlowArmedBeforePlate)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[FlowGym] '%s' was already true before the plate fired '%s' -- WaitForCondition did not wait",
			ScriptTest::Vars::szARMED, ScriptTest::Events::szPLATE_ARMED);
		return false;
	}
	if (g_iFlowDispensedUnarmed != 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[FlowGym] '%s' = %d after a press taken while unarmed, expected 0 -- the Gate is not gating",
			ScriptTest::Vars::szDISPENSED, g_iFlowDispensedUnarmed);
		return false;
	}
	if (!g_bFlowArmedAfterPlate)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[FlowGym] '%s' never turned true after the plate's event -- the suspended OnStart chain was not re-driven",
			ScriptTest::Vars::szARMED);
		return false;
	}

	// --- Cooldown -----------------------------------------------------------
	if (g_iFlowDispensedDouble != 1)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[FlowGym] two presses one frame apart gave '%s' = %d, expected 1 -- the Cooldown let both through",
			ScriptTest::Vars::szDISPENSED, g_iFlowDispensedDouble);
		return false;
	}
	if (g_iFlowDispensedSecond != 2 || g_iFlowDispensedThird != 3)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[FlowGym] '%s' = %d then %d after two further presses a window apart, expected 2 then 3 -- "
			"the Cooldown never re-opened",
			ScriptTest::Vars::szDISPENSED, g_iFlowDispensedSecond, g_iFlowDispensedThird);
		return false;
	}

	// --- ListAdd + GetListCount + CallGraph ---------------------------------
	if (g_iFlowBagAfterFill != g_iFlowDispensedThird)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[FlowGym] '%s' = %d but '%s' = %d -- ListAdd and the counter disagree",
			ScriptTest::Vars::szBAG_COUNT, g_iFlowBagAfterFill,
			ScriptTest::Vars::szDISPENSED, g_iFlowDispensedThird);
		return false;
	}
	// 10 per call is the child's own m_iDelta; the point of the assertion is
	// that the value landed on the CALLER'S blackboard at all.
	if (g_iFlowScoreAfterFill != g_iFlowDispensedThird * 10)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[FlowGym] '%s' = %d after %d dispenses, expected %d -- the CallGraph child did not write the caller's blackboard",
			ScriptTest::Vars::szSCORE, g_iFlowScoreAfterFill, g_iFlowDispensedThird, g_iFlowDispensedThird * 10);
		return false;
	}

	// --- ForEach ------------------------------------------------------------
	if (g_iFlowVisited != 3 || g_iFlowLastIdx != 2)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[FlowGym] one ForEach pass over a 3-element bag gave '%s' = %d and '%s' = %d, expected 3 and 2",
			ScriptTest::Vars::szVISITED, g_iFlowVisited, ScriptTest::Vars::szIDX, g_iFlowLastIdx);
		return false;
	}

	// --- ListRemoveAt -------------------------------------------------------
	// The bag was [1, 2, 3]; dropping index 0 must leave [2, 3]. A swap-remove
	// would leave [3, 2] and make the head 3 -- that value, not the count, is
	// what distinguishes the two.
	if (g_iFlowBagAfterDrop != 2)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[FlowGym] '%s' = %d after ListRemoveAt, expected 2",
			ScriptTest::Vars::szBAG_COUNT, g_iFlowBagAfterDrop);
		return false;
	}
	if (g_iFlowHeadAfterDrop != 2)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[FlowGym] '%s' = %d after dropping index 0 of [1,2,3], expected 2 -- ListRemoveAt reordered the list",
			ScriptTest::Vars::szHEAD, g_iFlowHeadAfterDrop);
		return false;
	}

	// --- ListClear, and the abort it causes ---------------------------------
	if (g_iFlowBagAfterClear != 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[FlowGym] '%s' = %d after ListClear, expected 0",
			ScriptTest::Vars::szBAG_COUNT, g_iFlowBagAfterClear);
		return false;
	}
	if (g_iFlowSentinel != -1)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[FlowGym] '%s' = %d, expected its declared -1 -- GetListElement on the emptied bag returned SUCCESS "
			"and let the rest of the chain run",
			ScriptTest::Vars::szSENTINEL, g_iFlowSentinel);
		return false;
	}

	// --- SwitchOnInt + SwitchOnString ---------------------------------------
	for (u_int uMode = 0; uMode < 4u; ++uMode)
	{
		const ST_FlowModeExpectation& xWant = g_axFlowModeExpectations[uMode];
		const ST_FlowModeSample& xGot = g_axFlowModes[uMode];
		if (xGot.m_strLabel != xWant.m_szLabel)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[FlowGym] mode %u wrote '%s' = \"%s\", expected \"%s\"",
				uMode, ScriptTest::Vars::szLABEL, xGot.m_strLabel.c_str(), xWant.m_szLabel);
			return false;
		}
		if (xGot.m_iLabelIndex != xWant.m_iLabelIndex)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[FlowGym] mode %u gave '%s' = %d, expected %d -- SwitchOnString matched the wrong pin for \"%s\"",
				uMode, ScriptTest::Vars::szLABEL_INDEX, xGot.m_iLabelIndex, xWant.m_iLabelIndex, xWant.m_szLabel);
			return false;
		}
		if (std::fabs(xGot.m_fNozzleScale - xWant.m_fNozzleScale) > 0.01f)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[FlowGym] mode %u left %s at scale %.3f, expected %.3f -- a different SwitchOnInt pin ran",
				uMode, ScriptTest::Entities::szNOZZLE, xGot.m_fNozzleScale, xWant.m_fNozzleScale);
			return false;
		}
	}
	if (g_bFlowNozzleMissing)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[FlowGym] %s vanished mid-test", ScriptTest::Entities::szNOZZLE);
		return false;
	}

	// --- Selector -----------------------------------------------------------
	if (g_iFlowAlarmA != 0 || g_iFlowNormalA <= 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[FlowGym] before the alarm: '%s' = %d (expected 0), '%s' = %d (expected > 0) -- "
			"the Selector is not falling through its gated pin 0",
			ScriptTest::Vars::szALARM_RUNS, g_iFlowAlarmA, ScriptTest::Vars::szNORMAL_RUNS, g_iFlowNormalA);
		return false;
	}
	if (g_iFlowNormalB <= g_iFlowNormalA)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[FlowGym] '%s' did not climb between the two pre-alarm samples (%d then %d)",
			ScriptTest::Vars::szNORMAL_RUNS, g_iFlowNormalA, g_iFlowNormalB);
		return false;
	}
	if (g_iFlowAlarmC <= g_iFlowAlarmB)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[FlowGym] '%s' did not climb after the alarm (%d then %d) -- pin 0 never took over",
			ScriptTest::Vars::szALARM_RUNS, g_iFlowAlarmB, g_iFlowAlarmC);
		return false;
	}
	// ★ PREEMPTION HAS NO OTHER SIGNATURE. Nothing in the alarm branch touches
	// normalRuns, so the ONLY evidence pin 1 stopped being reached is that its
	// counter went flat while the other one moved.
	if (g_iFlowNormalC != g_iFlowNormalB)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[FlowGym] '%s' kept climbing after the alarm (%d then %d) -- the Selector ran BOTH branches",
			ScriptTest::Vars::szNORMAL_RUNS, g_iFlowNormalB, g_iFlowNormalC);
		return false;
	}

	// --- LogicBlackboardBool: the full (armed, jammed) table -----------------
	const bool abExpected[4] = { false, true, false, false };
	const char* aszWhen[4] = {
		"armed=false jammed=false (start)",
		"armed=true  jammed=false (after the plate)",
		"armed=true  jammed=true  (after J)",
		"armed=false jammed=true  (after U)",
	};
	for (u_int u = 0; u < 4u; ++u)
	{
		if (g_abFlowCanDispense[u] != abExpected[u])
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[FlowGym] '%s' = %d at %s, expected %d",
				ScriptTest::Vars::szCAN_DISPENSE, g_abFlowCanDispense[u] ? 1 : 0,
				aszWhen[u], abExpected[u] ? 1 : 0);
			return false;
		}
	}

	return true;
}

static const Zenith_AutomatedTest g_xFlowGymTest = {
	"ST_FlowGym_Test",
	&Setup_FlowGym,
	&Step_FlowGym,
	&Verify_FlowGym,
	/*maxFrames*/ 1200,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xFlowGymTest);

// ============================================================================
// ST_AIGym_Test (C14)
// ----------------------------------------------------------------------------
// All twelve AI nodes, driven from graphs with no game C++ behind any of it --
// which was impossible before EnsureNavAgent existed: SetNavMeshAgent's only
// callers were game components, so every nav node returned FAILURE on a null
// pointer and a graph could not navigate at all.
//
// ★ HEADLESS, AND DELIBERATELY NOT m_bRequiresGraphics. Navigation and
// perception carry 13 headless automated tests across DevilsPlayground and
// Zenithmon plus dozens of boot units, and the line-of-sight check is a Jolt
// raycast proven headless by Test_P1Priest_PursuesAfterLineOfSight. A SKIPPED
// test counts as a PASS, so gating this one on graphics would make it go quiet
// exactly when it broke.
//
// ★ PERCEPTION IS ASSERTED BEFORE ANYTHING MOVES. The nav agent rewrites the
// walker's ROTATION to face its path, and the sight cone is 90 degrees wide --
// so every cone/range/LOS assertion happens while the walker still holds its
// authored facing, looking straight down +Z at the prey.
//
// ★ AND THE SOUND IS EMITTED BY THE PREY, QUERIED BY THE WALKER A FRAME LATER.
// Two independent reasons: agents never hear their own sounds, and
// Zenith_AI::Update runs AFTER the scene update, so a query in the same frame as
// its emit always reads stale.
// ============================================================================

namespace
{
	constexpr u_int iST_AI_WALKER_SLOT = 0u;	// the Walker carries only ST_NavWalker

	// 0.5 s at the harness's pinned dt -- one full awareness ramp at the default
	// 2.0/s gain rate, so the prey is fully perceived by the time it is sampled.
	constexpr int   iST_AI_PERCEIVE_FRAMES = 30;
	// Long enough for the agent to reach its cruising speed before a sample.
	constexpr int   iST_AI_SETTLE_FRAMES   = 20;
	// The window both speed samples are measured over. Same length for both, so
	// the comparison is purely about speed.
	constexpr int   iST_AI_SPEED_WINDOW    = 30;
	constexpr int   iST_AI_ARRIVE_POLL     = 600;
	// The graph's NavMoveTo acceptance radius, plus a little slack for the frame
	// the arrival is observed on.
	constexpr float fST_AI_ARRIVE_RADIUS   = 1.6f;
	// ST_NavWalker's FindRandomReachablePoint m_fRadius.
	constexpr float fST_AI_WANDER_RADIUS   = 6.0f;

	int32_t ST_AIInt(const char* szVar)   { return ST_ReadInt(ScriptTest::Entities::szWALKER, iST_AI_WALKER_SLOT, szVar, -99); }
	bool    ST_AIBool(const char* szVar)  { return ST_ReadBool(ScriptTest::Entities::szWALKER, iST_AI_WALKER_SLOT, szVar, false); }
	float   ST_AIFloat(const char* szVar) { return ST_ReadFloat(ScriptTest::Entities::szWALKER, iST_AI_WALKER_SLOT, szVar, -1.0f); }
	Zenith_Maths::Vector3 ST_AIVec3(const char* szVar) { return ST_ReadVec3(ScriptTest::Entities::szWALKER, iST_AI_WALKER_SLOT, szVar); }
	u_int64 ST_AIPacked(const char* szVar) { return ST_ReadPacked(ScriptTest::Entities::szWALKER, iST_AI_WALKER_SLOT, szVar); }

	Zenith_Maths::Vector3 ST_AIWalkerPos()
	{
		Zenith_Maths::Vector3 xPos(0.0f);
		ST_GetPosition(ScriptTest::Entities::szWALKER, xPos);
		return xPos;
	}

	// --- recorded observations ---------------------------------------------
	bool    g_bAIReady            = false;
	bool    g_bAIFinished         = false;
	bool    g_bAINavReady         = false;

	u_int64 g_ulAIPreyPacked      = 0;
	int32_t g_iAIPerceivedCount   = -1;
	u_int64 g_ulAIFirstTarget     = 0;
	bool    g_bAIPrimarySeen      = false;
	u_int64 g_ulAIPrimary         = 0;
	float   g_fAIAwareness        = -1.0f;

	u_int64 g_ulAIHeardSource     = 0;
	Zenith_Maths::Vector3 g_xAIHeardPos = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 g_xAIPreyPos  = Zenith_Maths::Vector3(0.0f);

	Zenith_Maths::Vector3 g_xAIStartPos = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 g_xAIFullA    = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 g_xAIFullB    = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 g_xAIStopA    = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 g_xAIStopB    = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 g_xAIHalfA    = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 g_xAIHalfB    = Zenith_Maths::Vector3(0.0f);

	int32_t g_iAINavStateMoving   = -99;
	int32_t g_iAINavStateStopped  = -99;
	float   g_fAINavLeftEarly     = -1.0f;
	float   g_fAINavLeftLate      = -1.0f;
	float   g_fAINavSpeedSample   = -1.0f;

	bool    g_bAIArrived          = false;
	Zenith_Maths::Vector3 g_xAIArrivedAt   = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 g_xAIFirstDest   = Zenith_Maths::Vector3(0.0f);

	Zenith_Maths::Vector3 g_xAIWanderFrom  = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 g_xAIWanderPoint = Zenith_Maths::Vector3(0.0f);
	bool    g_bAIWanderArrived    = false;
	Zenith_Maths::Vector3 g_xAIWanderEnd   = Zenith_Maths::Vector3(0.0f);

	int32_t g_iAICountAfterRetire = -1;
	bool    g_bAIPrimaryAfterRetire = true;
	bool    g_bAIPreyGone         = false;

	// --- the script ---------------------------------------------------------
	// Same flat table as ST_FlowGym_Test, plus an optional POLL: a stage may wait
	// for a condition (arrival) rather than a fixed number of frames, which is
	// the only honest way to write "it got there" without pinning a travel time.
	struct ST_AIStage
	{
		int  m_iLeadFrames;
		bool (*m_pfnPoll)();	// null = no poll
		int  m_iPollLimit;
		void (*m_pfnBody)();
	};

	bool ST_AIArrivedAtDest()
	{
		return ST_XZDistance(ST_AIWalkerPos(), ST_AIVec3(ScriptTest::Vars::szDEST)) <= fST_AI_ARRIVE_RADIUS;
	}

	void ST_AIPressGo()     { Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_SPACE); }
	void ST_AIPressStop()   { Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_X); }
	void ST_AIPressWander() { Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_W); }
	void ST_AIPressNoise()  { Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_S); }
	void ST_AIPressRetire() { Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_K); }

	void ST_AISampleWired()
	{
		g_bAINavReady = ST_AIBool(ScriptTest::Vars::szNAV_READY);
	}

	void ST_AISamplePerception()
	{
		Zenith_Entity xPrey = ST_FindEntity(ScriptTest::Entities::szPREY);
		g_ulAIPreyPacked = xPrey.IsValid() ? xPrey.GetEntityID().GetPacked() : 0;
		ST_GetPosition(ScriptTest::Entities::szPREY, g_xAIPreyPos);

		g_iAIPerceivedCount = ST_AIInt(ScriptTest::Vars::szPERCEIVED_N);
		g_ulAIFirstTarget = ST_AIPacked(ScriptTest::Vars::szFIRST_TARGET);
		g_bAIPrimarySeen = ST_AIBool(ScriptTest::Vars::szPRIMARY_SEEN);
		g_ulAIPrimary = ST_AIPacked(ScriptTest::Vars::szPRIMARY);
		g_fAIAwareness = ST_AIFloat(ScriptTest::Vars::szAWARENESS);
	}

	void ST_AISampleHeard()
	{
		g_ulAIHeardSource = ST_AIPacked(ScriptTest::Vars::szHEARD_SOURCE);
		g_xAIHeardPos = ST_AIVec3(ScriptTest::Vars::szHEARD_POS);
	}

	void ST_AIStartRun()
	{
		g_xAIStartPos = ST_AIWalkerPos();
		g_xAIFirstDest = ST_AIVec3(ScriptTest::Vars::szDEST);
		ST_AIPressGo();
	}

	void ST_AISampleFullA() { g_xAIFullA = ST_AIWalkerPos(); }

	void ST_AISampleFullB()
	{
		g_xAIFullB = ST_AIWalkerPos();
		g_iAINavStateMoving = ST_AIInt(ScriptTest::Vars::szNAV_STATE);
		g_fAINavLeftEarly = ST_AIFloat(ScriptTest::Vars::szNAV_LEFT);
		const Zenith_Maths::Vector3 xVel = ST_AIVec3(ScriptTest::Vars::szNAV_VEL);
		g_fAINavSpeedSample = std::sqrt(xVel.x * xVel.x + xVel.y * xVel.y + xVel.z * xVel.z);
	}

	void ST_AIStopRun()
	{
		ST_AIPressStop();
		g_xAIStopA = ST_AIWalkerPos();
	}

	void ST_AISampleStopped()
	{
		g_xAIStopB = ST_AIWalkerPos();
		g_iAINavStateStopped = ST_AIInt(ScriptTest::Vars::szNAV_STATE);
	}

	void ST_AIResumeSlow()
	{
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_N);	// half speed
		ST_AIPressGo();
	}

	void ST_AISampleHalfA() { g_xAIHalfA = ST_AIWalkerPos(); }

	void ST_AISampleHalfB()
	{
		g_xAIHalfB = ST_AIWalkerPos();
		g_fAINavLeftLate = ST_AIFloat(ScriptTest::Vars::szNAV_LEFT);
	}

	void ST_AISampleArrived()
	{
		g_bAIArrived = ST_AIArrivedAtDest();
		g_xAIArrivedAt = ST_AIWalkerPos();
	}

	void ST_AIStartWander()
	{
		g_xAIWanderFrom = ST_AIWalkerPos();
		ST_AIPressWander();
	}

	void ST_AISampleWanderPoint()
	{
		// FindRandomReachablePoint writes straight into 'dest', so the movement
		// chain picks it up unchanged -- which is what turns "in radius" into
		// "reachable".
		g_xAIWanderPoint = ST_AIVec3(ScriptTest::Vars::szDEST);
	}

	void ST_AISampleWanderArrived()
	{
		g_bAIWanderArrived = ST_AIArrivedAtDest();
		g_xAIWanderEnd = ST_AIWalkerPos();
	}

	void ST_AISampleRetired()
	{
		g_iAICountAfterRetire = ST_AIInt(ScriptTest::Vars::szPERCEIVED_N);
		g_bAIPrimaryAfterRetire = ST_AIBool(ScriptTest::Vars::szPRIMARY_SEEN);
		g_bAIPreyGone = !ST_EntityExists(ScriptTest::Entities::szPREY);
	}

	const ST_AIStage g_axAIStages[] =
	{
		{ 10,                     nullptr,              0,                  &ST_AISampleWired },
		{ iST_AI_PERCEIVE_FRAMES, nullptr,              0,                  &ST_AISamplePerception },
		{ 2,                      nullptr,              0,                  &ST_AIPressNoise },
		{ 3,                      nullptr,              0,                  &ST_AISampleHeard },	// a LATER frame than the emit
		{ 2,                      nullptr,              0,                  &ST_AIStartRun },
		{ iST_AI_SETTLE_FRAMES,   nullptr,              0,                  &ST_AISampleFullA },
		{ iST_AI_SPEED_WINDOW,    nullptr,              0,                  &ST_AISampleFullB },
		{ 2,                      nullptr,              0,                  &ST_AIStopRun },
		{ 30,                     nullptr,              0,                  &ST_AISampleStopped },
		{ 2,                      nullptr,              0,                  &ST_AIResumeSlow },
		{ iST_AI_SETTLE_FRAMES,   nullptr,              0,                  &ST_AISampleHalfA },
		{ iST_AI_SPEED_WINDOW,    nullptr,              0,                  &ST_AISampleHalfB },
		{ 1,                      &ST_AIArrivedAtDest,  iST_AI_ARRIVE_POLL, &ST_AISampleArrived },
		{ 2,                      nullptr,              0,                  &ST_AIStartWander },
		{ 3,                      nullptr,              0,                  &ST_AISampleWanderPoint },
		{ 1,                      &ST_AIArrivedAtDest,  iST_AI_ARRIVE_POLL, &ST_AISampleWanderArrived },
		{ 2,                      nullptr,              0,                  &ST_AIPressRetire },
		{ 20,                     nullptr,              0,                  &ST_AISampleRetired },
	};

	constexpr u_int uST_AI_STAGES = static_cast<u_int>(sizeof(g_axAIStages) / sizeof(g_axAIStages[0]));

	enum class AIPhase { Boot, WaitScene, Script, Done };

	AIPhase g_eAIPhase = AIPhase::Boot;
	u_int   g_uAIStage = 0;
	int     g_iAIWait  = 0;
	int     g_iAIPoll  = 0;
	bool    g_bAIPollTimedOut = false;
}

static void Setup_AIGym()
{
	g_eAIPhase = AIPhase::Boot;
	g_uAIStage = 0;
	g_iAIWait = 0;
	g_iAIPoll = 0;
	g_bAIPollTimedOut = false;
	g_bAIReady = false;
	g_bAIFinished = false;
	g_bAINavReady = false;

	g_ulAIPreyPacked = 0;
	g_iAIPerceivedCount = -1;
	g_ulAIFirstTarget = 0;
	g_bAIPrimarySeen = false;
	g_ulAIPrimary = 0;
	g_fAIAwareness = -1.0f;

	g_ulAIHeardSource = 0;
	g_xAIHeardPos = Zenith_Maths::Vector3(0.0f);
	g_xAIPreyPos = Zenith_Maths::Vector3(0.0f);

	g_xAIStartPos = Zenith_Maths::Vector3(0.0f);
	g_xAIFullA = Zenith_Maths::Vector3(0.0f);
	g_xAIFullB = Zenith_Maths::Vector3(0.0f);
	g_xAIStopA = Zenith_Maths::Vector3(0.0f);
	g_xAIStopB = Zenith_Maths::Vector3(0.0f);
	g_xAIHalfA = Zenith_Maths::Vector3(0.0f);
	g_xAIHalfB = Zenith_Maths::Vector3(0.0f);

	g_iAINavStateMoving = -99;
	g_iAINavStateStopped = -99;
	g_fAINavLeftEarly = -1.0f;
	g_fAINavLeftLate = -1.0f;
	g_fAINavSpeedSample = -1.0f;

	g_bAIArrived = false;
	g_xAIArrivedAt = Zenith_Maths::Vector3(0.0f);
	g_xAIFirstDest = Zenith_Maths::Vector3(0.0f);

	g_xAIWanderFrom = Zenith_Maths::Vector3(0.0f);
	g_xAIWanderPoint = Zenith_Maths::Vector3(0.0f);
	g_bAIWanderArrived = false;
	g_xAIWanderEnd = Zenith_Maths::Vector3(0.0f);

	// Seeded to the values that FAIL, so a stage that never ran cannot pass by
	// leaving its slot at something plausible.
	g_iAICountAfterRetire = -1;
	g_bAIPrimaryAfterRetire = true;
	g_bAIPreyGone = false;
}

static bool Step_AIGym(int iFrame)
{
	switch (g_eAIPhase)
	{
	case AIPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(ScriptTest::Scenes::iGYM_AI, SCENE_LOAD_SINGLE);
		g_eAIPhase = AIPhase::WaitScene;
		return true;

	case AIPhase::WaitScene:
		// All three behaviour-carrying entities are part of the gate: the holder
		// is what EnsureNavAgent discovers, and a scene missing any of them would
		// make every later stage fail for a reason this phase can name instead.
		if (ST_IsSceneActive(ScriptTest::Scenes::iGYM_AI)
			&& ST_EntityExists(ScriptTest::Entities::szNAVMESH_HOLDER)
			&& ST_EntityExists(ScriptTest::Entities::szWALKER)
			&& ST_EntityExists(ScriptTest::Entities::szPREY)
			&& ST_Blackboard(ScriptTest::Entities::szWALKER, iST_AI_WALKER_SLOT) != nullptr)
		{
			g_bAIReady = true;
			g_iAIWait = 0;
			g_eAIPhase = AIPhase::Script;
			return true;
		}
		return iFrame < 240;

	case AIPhase::Script:
	{
		const ST_AIStage& xStage = g_axAIStages[g_uAIStage];
		if (++g_iAIWait < xStage.m_iLeadFrames)
		{
			return true;
		}
		if (xStage.m_pfnPoll != nullptr && !xStage.m_pfnPoll())
		{
			if (++g_iAIPoll > xStage.m_iPollLimit)
			{
				g_bAIPollTimedOut = true;
				g_eAIPhase = AIPhase::Done;
				return false;
			}
			return true;
		}
		g_iAIWait = 0;
		g_iAIPoll = 0;
		xStage.m_pfnBody();
		if (++g_uAIStage >= uST_AI_STAGES)
		{
			g_bAIFinished = true;
			g_eAIPhase = AIPhase::Done;
			return false;
		}
		return true;
	}

	case AIPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_AIGym()
{
	if (!g_bAIReady)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[AIGym] Gym_AI never became active with its %s, %s and %s",
			ScriptTest::Entities::szNAVMESH_HOLDER, ScriptTest::Entities::szWALKER,
			ScriptTest::Entities::szPREY);
		return false;
	}
	if (g_bAIPollTimedOut)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[AIGym] stage %u timed out waiting for arrival: walker at (%.2f, %.2f) is %.2f m from "
			"'%s' (%.2f, %.2f)",
			g_uAIStage, ST_AIWalkerPos().x, ST_AIWalkerPos().z,
			ST_XZDistance(ST_AIWalkerPos(), ST_AIVec3(ScriptTest::Vars::szDEST)),
			ScriptTest::Vars::szDEST,
			ST_AIVec3(ScriptTest::Vars::szDEST).x, ST_AIVec3(ScriptTest::Vars::szDEST).z);
		return false;
	}
	if (!g_bAIFinished)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[AIGym] the script stopped at stage %u of %u",
			g_uAIStage, uST_AI_STAGES);
		return false;
	}

	// --- EnsureNavAgent -----------------------------------------------------
	// The flag is downstream of the node, so it is false unless EnsureNavAgent
	// returned SUCCESS -- which needs the holder's deferred navmesh load to have
	// completed AND an agent to have been allocated and bound.
	if (!g_bAINavReady)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[AIGym] '%s' never turned true -- EnsureNavAgent did not bind an agent to the scene's navmesh",
			ScriptTest::Vars::szNAV_READY);
		return false;
	}

	// --- perception: registration, list, primary, awareness -----------------
	if (g_ulAIPreyPacked == 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[AIGym] %s could not be resolved", ScriptTest::Entities::szPREY);
		return false;
	}
	if (g_iAIPerceivedCount < 1)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[AIGym] '%s' = %d after %d frames, expected >= 1 -- either RegisterPerceptionTarget never ran "
			"or the engine AI tick is not driving perception",
			ScriptTest::Vars::szPERCEIVED_N, g_iAIPerceivedCount, iST_AI_PERCEIVE_FRAMES);
		return false;
	}
	// The LIST, not just the count: element 0 must be the prey itself.
	if (g_ulAIFirstTarget != g_ulAIPreyPacked)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[AIGym] '%s' = %llu, expected %s's packed id %llu",
			ScriptTest::Vars::szFIRST_TARGET, static_cast<unsigned long long>(g_ulAIFirstTarget),
			ScriptTest::Entities::szPREY, static_cast<unsigned long long>(g_ulAIPreyPacked));
		return false;
	}
	if (!g_bAIPrimarySeen || g_ulAIPrimary != g_ulAIPreyPacked)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[AIGym] QueryPrimaryPerceivedTarget: seen %d, '%s' = %llu, expected %llu -- the prey is "
			"registered HOSTILE, so it must surface as the primary",
			g_bAIPrimarySeen ? 1 : 0, ScriptTest::Vars::szPRIMARY,
			static_cast<unsigned long long>(g_ulAIPrimary),
			static_cast<unsigned long long>(g_ulAIPreyPacked));
		return false;
	}
	if (!(g_fAIAwareness > 0.0f))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[AIGym] '%s' = %.3f, expected > 0 while the prey is in the sight cone",
			ScriptTest::Vars::szAWARENESS, g_fAIAwareness);
		return false;
	}

	// --- the sound, emitted by the prey and heard a frame later -------------
	if (g_ulAIHeardSource != g_ulAIPreyPacked)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[AIGym] '%s' = %llu, expected the prey's %llu -- EmitSoundStimulus attributes the sound to its "
			"SOURCE entity, and an agent never hears its own",
			ScriptTest::Vars::szHEARD_SOURCE, static_cast<unsigned long long>(g_ulAIHeardSource),
			static_cast<unsigned long long>(g_ulAIPreyPacked));
		return false;
	}
	if (ST_XZDistance(g_xAIHeardPos, g_xAIPreyPos) > 0.1f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[AIGym] '%s' = (%.2f, %.2f), expected the prey's own position (%.2f, %.2f)",
			ScriptTest::Vars::szHEARD_POS, g_xAIHeardPos.x, g_xAIHeardPos.z,
			g_xAIPreyPos.x, g_xAIPreyPos.z);
		return false;
	}

	// --- SetNavDestination + NavMoveTo + ReadNavState ------------------------
	if (g_iAINavStateMoving != 2)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[AIGym] '%s' = %d while en route, expected 2 (moving)",
			ScriptTest::Vars::szNAV_STATE, g_iAINavStateMoving);
		return false;
	}
	if (!(g_fAINavSpeedSample > 0.1f))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[AIGym] '%s' had magnitude %.3f while en route, expected a moving agent",
			ScriptTest::Vars::szNAV_VEL, g_fAINavSpeedSample);
		return false;
	}
	const float fFullDistance = ST_XZDistance(g_xAIFullB, g_xAIFullA);
	if (!(fFullDistance > 0.5f))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[AIGym] the walker covered %.3f m in %d frames at full speed, expected it to be moving",
			fFullDistance, iST_AI_SPEED_WINDOW);
		return false;
	}

	// --- StopNav, and that the stop PERSISTS ---------------------------------
	// ★ A one-frame check would pass on a stop that was immediately undone by
	// the next repath. Holding still for a further 30 frames is the assertion.
	const float fDriftWhileStopped = ST_XZDistance(g_xAIStopB, g_xAIStopA);
	if (fDriftWhileStopped > 0.15f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[AIGym] the walker drifted %.3f m over 30 frames after StopNav -- the stop did not persist",
			fDriftWhileStopped);
		return false;
	}
	if (g_iAINavStateStopped != 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[AIGym] '%s' = %d after StopNav, expected 0 (none)",
			ScriptTest::Vars::szNAV_STATE, g_iAINavStateStopped);
		return false;
	}

	// --- SetNavSpeed: an ORDERING, never an absolute time --------------------
	const float fHalfDistance = ST_XZDistance(g_xAIHalfB, g_xAIHalfA);
	if (!(fHalfDistance > 0.1f))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[AIGym] the walker covered %.3f m in %d frames after SetNavSpeed -- it never resumed",
			fHalfDistance, iST_AI_SPEED_WINDOW);
		return false;
	}
	if (!(fHalfDistance < fFullDistance * 0.75f))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[AIGym] over the same %d-frame window the walker covered %.3f m at half speed and %.3f m at "
			"full speed -- SetNavSpeed had no effect",
			iST_AI_SPEED_WINDOW, fHalfDistance, fFullDistance);
		return false;
	}
	// The remaining distance shrinks as it goes -- read at two points a leg apart.
	if (!(g_fAINavLeftLate < g_fAINavLeftEarly))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[AIGym] '%s' went %.3f -> %.3f, expected it to shrink as the walker advanced",
			ScriptTest::Vars::szNAV_LEFT, g_fAINavLeftEarly, g_fAINavLeftLate);
		return false;
	}

	// --- arrival -------------------------------------------------------------
	if (!g_bAIArrived)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[AIGym] the walker never reached its first destination");
		return false;
	}
	const float fTravelled = ST_XZDistance(g_xAIArrivedAt, g_xAIStartPos);
	if (!(fTravelled > 2.0f))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[AIGym] the walker moved only %.3f m in total, expected > 2 m", fTravelled);
		return false;
	}
	if (ST_XZDistance(g_xAIArrivedAt, g_xAIFirstDest) > fST_AI_ARRIVE_RADIUS)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[AIGym] the walker stopped %.3f m from (%.2f, %.2f), outside the acceptance radius",
			ST_XZDistance(g_xAIArrivedAt, g_xAIFirstDest), g_xAIFirstDest.x, g_xAIFirstDest.z);
		return false;
	}

	// --- FindRandomReachablePoint --------------------------------------------
	// In radius, on the baked surface, AND actually walked to -- the last of
	// those is what separates "reachable" from "merely nearby".
	const float fWanderOffset = ST_XZDistance(g_xAIWanderPoint, g_xAIWanderFrom);
	if (fWanderOffset > fST_AI_WANDER_RADIUS + 0.01f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[AIGym] the wander point is %.3f m away, outside the requested %.1f m radius",
			fWanderOffset, fST_AI_WANDER_RADIUS);
		return false;
	}
	if (std::fabs(g_xAIWanderPoint.x) > ScriptTest::Navmesh::fHALF_EXTENT + 0.01f
		|| std::fabs(g_xAIWanderPoint.z) > ScriptTest::Navmesh::fHALF_EXTENT + 0.01f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[AIGym] the wander point (%.2f, %.2f) is off the baked %.1f m half-extent",
			g_xAIWanderPoint.x, g_xAIWanderPoint.z, ScriptTest::Navmesh::fHALF_EXTENT);
		return false;
	}
	if (!g_bAIWanderArrived || ST_XZDistance(g_xAIWanderEnd, g_xAIWanderPoint) > fST_AI_ARRIVE_RADIUS)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[AIGym] the walker did not reach the wander point: %.3f m away at (%.2f, %.2f)",
			ST_XZDistance(g_xAIWanderEnd, g_xAIWanderPoint), g_xAIWanderEnd.x, g_xAIWanderEnd.z);
		return false;
	}

	// --- the SYMMETRIC unregister --------------------------------------------
	// ★ THE WINDOW IS THE ASSERTION. Nothing auto-unregisters a destroyed
	// perception target, so without ST_Prey's OnDestroy chain the record survives
	// and only decays out at 0.5 awareness/second -- about 120 frames. Sampling
	// 20 frames after the destroy is inside that decay and outside the
	// unregister, so only the unregister can produce a zero here.
	if (!g_bAIPreyGone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[AIGym] DestroyEntity did not remove %s",
			ScriptTest::Entities::szPREY);
		return false;
	}
	if (g_iAICountAfterRetire != 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[AIGym] '%s' = %d 20 frames after the prey was destroyed, expected 0 -- the OnDestroy "
			"RegisterPerceptionTarget(unregister) chain did not run",
			ScriptTest::Vars::szPERCEIVED_N, g_iAICountAfterRetire);
		return false;
	}
	// And the has-target gate closes. The flag is cleared at the head of its own
	// chain every frame, so it reads "the query succeeded THIS frame" -- a latch
	// cleared from somewhere else would race the retire chain, which runs first
	// in the same dispatch while perception still remembers the doomed prey.
	if (g_bAIPrimaryAfterRetire)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[AIGym] '%s' is still true 20 frames after the prey was retired -- "
			"QueryPrimaryPerceivedTarget still reports a target",
			ScriptTest::Vars::szPRIMARY_SEEN);
		return false;
	}

	return true;
}

static const Zenith_AutomatedTest g_xAIGymTest = {
	"ST_AIGym_Test",
	&Setup_AIGym,
	&Step_AIGym,
	&Verify_AIGym,
	/*maxFrames*/ 2000,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xAIGymTest);

#endif // ZENITH_INPUT_SIMULATOR
