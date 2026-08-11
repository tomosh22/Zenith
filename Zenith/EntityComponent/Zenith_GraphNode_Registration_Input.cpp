#include "Zenith.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "Core/Zenith_Engine.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_KeyCodes.h"
#include "Input/Zenith_Pointers.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"
#include "EntityComponent/Zenith_PhysicsQuery.h"

//------------------------------------------------------------------------------
// Engine Behaviour Graph node library - Input domain.
//
// Event sources register under GRAPH_EVENT_ON_UPDATE and gate themselves in
// Execute (the Timer pattern: SUCCESS = run the chain this frame). Edge modes
// (pressed / released) latch previous state as per-instance members - the
// established per-graph-instance state pattern.
//
// Everything routes through g_xEngine.Input(), which is Zenith_InputSimulator-
// aware - automated tests drive these nodes through real input paths.
//------------------------------------------------------------------------------

namespace
{
	//==========================================================================
	// Event sources (ON_UPDATE-anchored gates)
	//==========================================================================

	// Fires the frame the key transitions up -> down.
	class Zenith_GraphNode_OnKeyPressed : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_OnKeyPressed)
	public:
		ZENITH_PROPERTY(int32_t, m_iKeyCode, ZENITH_KEY_SPACE)

		GraphNodeStatus Execute(Zenith_GraphContext&) override
		{
			return g_xEngine.Input().WasKeyPressedThisFrame(m_iKeyCode)
				? GRAPH_NODE_STATUS_SUCCESS : GRAPH_NODE_STATUS_FAILURE;
		}
		const char* GetTypeName() const override { return "OnKeyPressed"; }
	};

	// Fires the frame the key transitions down -> up. Reads the DEVICE release
	// edge: the per-instance latch this used to carry could only see transitions
	// that straddled two of its own Executes, so a tap inside one frame, or a
	// release that arrived while the graph was not running, was silently lost.
	class Zenith_GraphNode_OnKeyReleased : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_OnKeyReleased)
	public:
		ZENITH_PROPERTY(int32_t, m_iKeyCode, ZENITH_KEY_SPACE)

		GraphNodeStatus Execute(Zenith_GraphContext&) override
		{
			return g_xEngine.Input().WasKeyReleasedThisFrame(m_iKeyCode)
				? GRAPH_NODE_STATUS_SUCCESS : GRAPH_NODE_STATUS_FAILURE;
		}
		const char* GetTypeName() const override { return "OnKeyReleased"; }
	};

	// Fires every frame the key is held.
	class Zenith_GraphNode_OnKeyHeld : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_OnKeyHeld)
	public:
		ZENITH_PROPERTY(int32_t, m_iKeyCode, ZENITH_KEY_W)

		GraphNodeStatus Execute(Zenith_GraphContext&) override
		{
			return g_xEngine.Input().IsKeyDown(m_iKeyCode)
				? GRAPH_NODE_STATUS_SUCCESS : GRAPH_NODE_STATUS_FAILURE;
		}
		const char* GetTypeName() const override { return "OnKeyHeld"; }
	};

	// Mouse-button gate. Mode: 0 = pressed edge, 1 = held, 2 = released edge.
	// Both edges come from the DEVICE (mouse buttons live in the same key array,
	// indices 0-7), not from a per-instance latch: the latch could only see a
	// transition that straddled two of this node's own Executes, so a tap that
	// began and ended inside one frame produced nothing at all.
	class Zenith_GraphNode_OnMouseButton : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_OnMouseButton)
	public:
		ZENITH_PROPERTY(int32_t, m_iButton, ZENITH_MOUSE_BUTTON_LEFT)
		ZENITH_PROPERTY(int32_t, m_iMode, 0)

		GraphNodeStatus Execute(Zenith_GraphContext&) override
		{
			Zenith_Input& xInput = g_xEngine.Input();
			bool bFire = false;
			switch (m_iMode)
			{
			case 0: bFire = xInput.WasKeyPressedThisFrame(m_iButton); break;
			case 1: bFire = xInput.IsMouseButtonHeld(m_iButton); break;
			case 2: bFire = xInput.WasKeyReleasedThisFrame(m_iButton); break;
			default: break;
			}
			return bFire ? GRAPH_NODE_STATUS_SUCCESS : GRAPH_NODE_STATUS_FAILURE;
		}
		const char* GetTypeName() const override { return "OnMouseButton"; }
	};

	// Fires when the mouse moved this frame; stashes the delta (vec2) into a
	// blackboard variable (the collision-source stash pattern).
	class Zenith_GraphNode_OnMouseMoved : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_OnMouseMoved)
	public:
		ZENITH_PROPERTY(std::string, m_strStoreDeltaVar, "mouseDelta")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Maths::Vector2_64 xDelta;
			g_xEngine.Input().GetMouseDelta(xDelta);
			if (xDelta.x == 0.0 && xDelta.y == 0.0)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			if (!m_strStoreDeltaVar.empty())
			{
				Zenith_PropertyValue xValue;
				xValue.SetVector2(Zenith_Maths::Vector2(static_cast<float>(xDelta.x), static_cast<float>(xDelta.y)));
				xContext.m_pxBlackboard->SetValue(m_strStoreDeltaVar, xValue);
			}
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "OnMouseMoved"; }
	};

	//==========================================================================
	// Query actions (always SUCCESS; write blackboard)
	//==========================================================================

	// Key state -> bool var. Mode: 0 = held, 1 = pressed this frame.
	class Zenith_GraphNode_ReadKeyState : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ReadKeyState)
	public:
		ZENITH_PROPERTY(int32_t, m_iKeyCode, ZENITH_KEY_LEFT_SHIFT)
		ZENITH_PROPERTY(int32_t, m_iMode, 0)
		ZENITH_PROPERTY(std::string, m_strResultVar, "key")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			const bool bState = (m_iMode == 1)
				? g_xEngine.Input().WasKeyPressedThisFrame(m_iKeyCode)
				: g_xEngine.Input().IsKeyDown(m_iKeyCode);
			Zenith_PropertyValue xValue;
			xValue.SetBool(bState);
			xContext.m_pxBlackboard->SetValue(m_strResultVar, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ReadKeyState"; }
	};

	// Four-key quad -> direction vec3 (X = right, Z = forward), optionally
	// normalized. Camera-relative rotation is a separate concern
	// (ReadCameraBasis + vector math nodes).
	class Zenith_GraphNode_ReadMovementAxis : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ReadMovementAxis)
	public:
		ZENITH_PROPERTY(int32_t, m_iKeyForward, ZENITH_KEY_W)
		ZENITH_PROPERTY(int32_t, m_iKeyBack, ZENITH_KEY_S)
		ZENITH_PROPERTY(int32_t, m_iKeyLeft, ZENITH_KEY_A)
		ZENITH_PROPERTY(int32_t, m_iKeyRight, ZENITH_KEY_D)
		ZENITH_PROPERTY(bool, m_bNormalize, true)
		ZENITH_PROPERTY(std::string, m_strResultVar, "moveDir")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Input& xInput = g_xEngine.Input();
			Zenith_Maths::Vector3 xDirection(0.0f);
			if (xInput.IsKeyDown(m_iKeyForward)) { xDirection.z += 1.0f; }
			if (xInput.IsKeyDown(m_iKeyBack))    { xDirection.z -= 1.0f; }
			if (xInput.IsKeyDown(m_iKeyRight))   { xDirection.x += 1.0f; }
			if (xInput.IsKeyDown(m_iKeyLeft))    { xDirection.x -= 1.0f; }
			if (m_bNormalize && (xDirection.x != 0.0f || xDirection.z != 0.0f))
			{
				xDirection = glm::normalize(xDirection);
			}
			Zenith_PropertyValue xValue;
			xValue.SetVector3(xDirection);
			xContext.m_pxBlackboard->SetValue(m_strResultVar, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ReadMovementAxis"; }
	};

	// Two-key axis -> float var (-1 / 0 / +1).
	class Zenith_GraphNode_ReadInputAxis : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ReadInputAxis)
	public:
		ZENITH_PROPERTY(int32_t, m_iNegativeKey, ZENITH_KEY_A)
		ZENITH_PROPERTY(int32_t, m_iPositiveKey, ZENITH_KEY_D)
		ZENITH_PROPERTY(std::string, m_strResultVar, "axis")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			float fAxis = 0.0f;
			if (g_xEngine.Input().IsKeyDown(m_iPositiveKey)) { fAxis += 1.0f; }
			if (g_xEngine.Input().IsKeyDown(m_iNegativeKey)) { fAxis -= 1.0f; }
			Zenith_PropertyValue xValue;
			xValue.SetFloat(fAxis);
			xContext.m_pxBlackboard->SetValue(m_strResultVar, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ReadInputAxis"; }
	};

	class Zenith_GraphNode_ReadMousePosition : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ReadMousePosition)
	public:
		ZENITH_PROPERTY(std::string, m_strResultVar, "mousePos")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Maths::Vector2_64 xPosition;
			g_xEngine.Input().GetMousePosition(xPosition);
			Zenith_PropertyValue xValue;
			xValue.SetVector2(Zenith_Maths::Vector2(static_cast<float>(xPosition.x), static_cast<float>(xPosition.y)));
			xContext.m_pxBlackboard->SetValue(m_strResultVar, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ReadMousePosition"; }
	};

	class Zenith_GraphNode_ReadMouseDelta : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ReadMouseDelta)
	public:
		ZENITH_PROPERTY(float, m_fSensitivity, 1.0f)
		ZENITH_PROPERTY(std::string, m_strResultVar, "mouseDelta")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Maths::Vector2_64 xDelta;
			g_xEngine.Input().GetMouseDelta(xDelta);
			Zenith_PropertyValue xValue;
			xValue.SetVector2(Zenith_Maths::Vector2(
				static_cast<float>(xDelta.x) * m_fSensitivity,
				static_cast<float>(xDelta.y) * m_fSensitivity));
			xContext.m_pxBlackboard->SetValue(m_strResultVar, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ReadMouseDelta"; }
	};

	class Zenith_GraphNode_ReadMouseButtonHeld : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ReadMouseButtonHeld)
	public:
		ZENITH_PROPERTY(int32_t, m_iButton, ZENITH_MOUSE_BUTTON_LEFT)
		ZENITH_PROPERTY(std::string, m_strResultVar, "mouseHeld")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_PropertyValue xValue;
			xValue.SetBool(g_xEngine.Input().IsMouseButtonHeld(m_iButton));
			xContext.m_pxBlackboard->SetValue(m_strResultVar, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ReadMouseButtonHeld"; }
	};

	class Zenith_GraphNode_ReadMouseWheel : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ReadMouseWheel)
	public:
		ZENITH_PROPERTY(std::string, m_strResultVar, "wheel")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_PropertyValue xValue;
			xValue.SetFloat(g_xEngine.Input().GetMouseWheelDelta());
			xContext.m_pxBlackboard->SetValue(m_strResultVar, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ReadMouseWheel"; }
	};

	// One slot of the pointer table -> blackboard. The engine tracks up to
	// Zenith_Pointers::uMAX_POINTERS simultaneous pointers, so a graph has to
	// say WHICH one it means.
	//
	// Slot 0 is the primary pointer, which B3 keeps in step with the mouse view
	// on every platform — a graph written against slot 0 works on desktop and on
	// a touch device without a branch.
	//
	// Positions are RAW SURFACE pixels (B7); canvas-space remapping is a UI-layer
	// concern and deliberately does not happen here.
	class Zenith_GraphNode_ReadPointer : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ReadPointer)
	public:
		ZENITH_PROPERTY(int32_t, m_iPointerIndex, 0)
		ZENITH_PROPERTY(std::string, m_strDownVar, "pointerDown")
		ZENITH_PROPERTY(std::string, m_strPositionVar, "pointerPos")
		ZENITH_PROPERTY(std::string, m_strTapVar, "")
		ZENITH_PROPERTY(std::string, m_strCountVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Pointers& xPointers = g_xEngine.Pointers();
			const bool bValidSlot = m_iPointerIndex >= 0
				&& static_cast<u_int32>(m_iPointerIndex) < Zenith_Pointers::uMAX_POINTERS;
			// A slot that does not exist reads as an absent pointer rather than
			// failing the chain: a graph polling slot 1 must not stall the frames
			// where only one finger is down.
			const Zenith_Pointer* pxPointer = bValidSlot ? &xPointers.GetPointer(static_cast<u_int32>(m_iPointerIndex)) : nullptr;

			Zenith_PropertyValue xValue;
			if (!m_strDownVar.empty())
			{
				xValue.SetBool(pxPointer != nullptr && pxPointer->IsDown());
				xContext.m_pxBlackboard->SetValue(m_strDownVar, xValue);
			}
			if (!m_strPositionVar.empty())
			{
				xValue.SetVector2(pxPointer != nullptr ? pxPointer->m_xPosition : Zenith_Maths::Vector2(0.0f, 0.0f));
				xContext.m_pxBlackboard->SetValue(m_strPositionVar, xValue);
			}
			if (!m_strTapVar.empty())
			{
				xValue.SetBool(pxPointer != nullptr && pxPointer->m_bTapThisFrame);
				xContext.m_pxBlackboard->SetValue(m_strTapVar, xValue);
			}
			if (!m_strCountVar.empty())
			{
				xValue.SetInt32(static_cast<int32_t>(xPointers.GetActivePointerCount()));
				xContext.m_pxBlackboard->SetValue(m_strCountVar, xValue);
			}
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ReadPointer"; }
	};

	// Main-camera ray under the mouse cursor -> origin + direction vec3 vars
	// (feed the Physics Raycast node's origin/direction vars for picking).
	// FAILURE when no loaded scene has a resolvable main camera.
	class Zenith_GraphNode_ReadMousePickRay : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ReadMousePickRay)
	public:
		ZENITH_PROPERTY(std::string, m_strOriginVar, "rayOrigin")
		ZENITH_PROPERTY(std::string, m_strDirectionVar, "rayDir")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_CameraComponent* pxCamera = Zenith_GetMainCameraAcrossScenes();
			if (pxCamera == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const Zenith_PhysicsQuery::Ray xRay = Zenith_PhysicsQuery::BuildRayFromMouse(*pxCamera);
			Zenith_PropertyValue xValue;
			if (!m_strOriginVar.empty())
			{
				xValue.SetVector3(xRay.m_xOrigin);
				xContext.m_pxBlackboard->SetValue(m_strOriginVar, xValue);
			}
			if (!m_strDirectionVar.empty())
			{
				xValue.SetVector3(xRay.m_xDirection);
				xContext.m_pxBlackboard->SetValue(m_strDirectionVar, xValue);
			}
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ReadMousePickRay"; }
	};
}

void Zenith_RegisterEngineGraphNodes_Input()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();

	// Event sources (ON_UPDATE-anchored self-gating).
	xRegistry.RegisterNodeType<Zenith_GraphNode_OnKeyPressed>("OnKeyPressed", GRAPH_EVENT_ON_UPDATE, 1, false, "Input");
	xRegistry.RegisterNodeType<Zenith_GraphNode_OnKeyReleased>("OnKeyReleased", GRAPH_EVENT_ON_UPDATE, 1, false, "Input");
	xRegistry.RegisterNodeType<Zenith_GraphNode_OnKeyHeld>("OnKeyHeld", GRAPH_EVENT_ON_UPDATE, 1, false, "Input");
	xRegistry.RegisterNodeType<Zenith_GraphNode_OnMouseButton>("OnMouseButton", GRAPH_EVENT_ON_UPDATE, 1, false, "Input");
	xRegistry.RegisterNodeType<Zenith_GraphNode_OnMouseMoved>("OnMouseMoved", GRAPH_EVENT_ON_UPDATE, 1, false, "Input");

	// Query actions.
	xRegistry.RegisterNodeType<Zenith_GraphNode_ReadKeyState>("ReadKeyState", GRAPH_EVENT_NONE, 1, false, "Input");
	xRegistry.RegisterNodeType<Zenith_GraphNode_ReadMovementAxis>("ReadMovementAxis", GRAPH_EVENT_NONE, 1, false, "Input");
	xRegistry.RegisterNodeType<Zenith_GraphNode_ReadInputAxis>("ReadInputAxis", GRAPH_EVENT_NONE, 1, false, "Input");
	xRegistry.RegisterNodeType<Zenith_GraphNode_ReadMousePosition>("ReadMousePosition", GRAPH_EVENT_NONE, 1, false, "Input");
	xRegistry.RegisterNodeType<Zenith_GraphNode_ReadMouseDelta>("ReadMouseDelta", GRAPH_EVENT_NONE, 1, false, "Input");
	xRegistry.RegisterNodeType<Zenith_GraphNode_ReadMouseButtonHeld>("ReadMouseButtonHeld", GRAPH_EVENT_NONE, 1, false, "Input");
	xRegistry.RegisterNodeType<Zenith_GraphNode_ReadMouseWheel>("ReadMouseWheel", GRAPH_EVENT_NONE, 1, false, "Input");
	xRegistry.RegisterNodeType<Zenith_GraphNode_ReadPointer>("ReadPointer", GRAPH_EVENT_NONE, 1, false, "Input");
	xRegistry.RegisterNodeType<Zenith_GraphNode_ReadMousePickRay>("ReadMousePickRay", GRAPH_EVENT_NONE, 1, false, "Input");
}
