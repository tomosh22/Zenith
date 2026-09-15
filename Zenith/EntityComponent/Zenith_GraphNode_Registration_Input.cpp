#include "Zenith.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "Core/Zenith_Engine.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputActions.h"
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
// Everything routes through DeviceInput() / ActionLayer() below, which are the
// engine's own instances and Zenith_InputSimulator-aware - automated tests drive
// these nodes through real input paths.
//
// TWO LAYERS LIVE HERE, DELIBERATELY. The Key/Mouse/Pointer nodes are the
// DEVICE layer (a graph naming a physical code); the Action nodes at the bottom
// are the ACTION layer (a graph naming what the player MEANT, with the game's
// binding table deciding which device answered). Neither replaces the other: a
// debug-camera graph legitimately wants "the F key", and a gameplay graph
// legitimately wants "Dodge" so a pad, a keyboard and a thumb on glass all work
// without a branch.
//------------------------------------------------------------------------------

namespace
{
	//==========================================================================
	// The TWO engine-singleton reaches this whole TU is allowed.
	//
	// Every node body below goes through these instead of naming g_xEngine
	// itself. Spelling the singleton once per node put this file at 21 reaches --
	// over the architecture gate's per-file ceiling -- for no gain: the accessors
	// inline away, and a node body reads better naming the LAYER it is asking
	// (DEVICE vs ACTION) than repeating the global.
	//==========================================================================

	Zenith_Input& DeviceInput()
	{
		return g_xEngine.Input();
	}

	Zenith_InputActions& ActionLayer()
	{
		return g_xEngine.Actions();
	}

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
			return DeviceInput().WasKeyPressedThisFrame(m_iKeyCode)
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
			return DeviceInput().WasKeyReleasedThisFrame(m_iKeyCode)
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
			return DeviceInput().IsKeyDown(m_iKeyCode)
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
			Zenith_Input& xInput = DeviceInput();
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

		// The event-source STASH shape (the collision sources' pattern): a
		// configured destination the source publishes its payload into.
		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_OnMouseMoved)
		ZENITH_GRAPH_PIN_SELECTOR_WRITE(StoreDelta, "m_strStoreDeltaVar", PROPERTY_TYPE_VECTOR2)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Maths::Vector2_64 xDelta;
			DeviceInput().GetMouseDelta(xDelta);
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
	// Query readers expose computed values through OUTPUT slots only. They do
	// not bind or publish blackboard variables; ReadMousePickRay may FAIL.
	//==========================================================================

	// Key state -> bool OUTPUT slot. Mode: 0 = held, 1 = pressed this frame.
	class Zenith_GraphNode_ReadKeyState : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ReadKeyState)
	public:
		ZENITH_PROPERTY(int32_t, m_iKeyCode, ZENITH_KEY_LEFT_SHIFT)
		ZENITH_PROPERTY(int32_t, m_iMode, 0)
		// Key codes and modes are device configuration, never blackboard values.
		static constexpr u_int uPIN_Result = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_ReadKeyState)
		ZENITH_GRAPH_PIN_OUTPUT(Result, PROPERTY_TYPE_BOOL)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			const bool bState = (m_iMode == 1)
				? DeviceInput().WasKeyPressedThisFrame(m_iKeyCode)
				: DeviceInput().IsKeyDown(m_iKeyCode);
			SetOutput<bool>(xContext, uPIN_Result, bState);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ReadKeyState"; }
	};

	// Four-key quad -> direction vec3 OUTPUT slot (X = right, Z = forward), optionally
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
		// The four key codes and the normalize flag are device configuration.
		static constexpr u_int uPIN_Result = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_ReadMovementAxis)
		ZENITH_GRAPH_PIN_OUTPUT(Result, PROPERTY_TYPE_VECTOR3)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Input& xInput = DeviceInput();
			Zenith_Maths::Vector3 xDirection(0.0f);
			if (xInput.IsKeyDown(m_iKeyForward)) { xDirection.z += 1.0f; }
			if (xInput.IsKeyDown(m_iKeyBack))    { xDirection.z -= 1.0f; }
			if (xInput.IsKeyDown(m_iKeyRight))   { xDirection.x += 1.0f; }
			if (xInput.IsKeyDown(m_iKeyLeft))    { xDirection.x -= 1.0f; }
			if (m_bNormalize && (xDirection.x != 0.0f || xDirection.z != 0.0f))
			{
				xDirection = glm::normalize(xDirection);
			}
			SetOutput<Zenith_Maths::Vector3>(xContext, uPIN_Result, xDirection);
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
		static constexpr u_int uPIN_Result = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_ReadInputAxis)
		ZENITH_GRAPH_PIN_OUTPUT(Result, PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			float fAxis = 0.0f;
			if (DeviceInput().IsKeyDown(m_iPositiveKey)) { fAxis += 1.0f; }
			if (DeviceInput().IsKeyDown(m_iNegativeKey)) { fAxis -= 1.0f; }
			SetOutput<float>(xContext, uPIN_Result, fAxis);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ReadInputAxis"; }
	};

	class Zenith_GraphNode_ReadMousePosition : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ReadMousePosition)
	public:
		static constexpr u_int uPIN_Result = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_ReadMousePosition)
		ZENITH_GRAPH_PIN_OUTPUT(Result, PROPERTY_TYPE_VECTOR2)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Maths::Vector2_64 xPosition;
			DeviceInput().GetMousePosition(xPosition);
			SetOutput<Zenith_Maths::Vector2>(xContext, uPIN_Result,
				Zenith_Maths::Vector2(static_cast<float>(xPosition.x), static_cast<float>(xPosition.y)));
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
		// m_fSensitivity has no var partner: a scale factor, not an input pin.
		static constexpr u_int uPIN_Result = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_ReadMouseDelta)
		ZENITH_GRAPH_PIN_OUTPUT(Result, PROPERTY_TYPE_VECTOR2)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Maths::Vector2_64 xDelta;
			DeviceInput().GetMouseDelta(xDelta);
			SetOutput<Zenith_Maths::Vector2>(xContext, uPIN_Result, Zenith_Maths::Vector2(
				static_cast<float>(xDelta.x) * m_fSensitivity,
				static_cast<float>(xDelta.y) * m_fSensitivity));
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
		static constexpr u_int uPIN_Result = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_ReadMouseButtonHeld)
		ZENITH_GRAPH_PIN_OUTPUT(Result, PROPERTY_TYPE_BOOL)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			SetOutput<bool>(xContext, uPIN_Result, DeviceInput().IsMouseButtonHeld(m_iButton));
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ReadMouseButtonHeld"; }
	};

	class Zenith_GraphNode_ReadMouseWheel : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ReadMouseWheel)
	public:
		static constexpr u_int uPIN_Result = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_ReadMouseWheel)
		ZENITH_GRAPH_PIN_OUTPUT(Result, PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			SetOutput<float>(xContext, uPIN_Result, DeviceInput().GetMouseWheelDelta());
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ReadMouseWheel"; }
	};

	// One slot of the pointer table -> OUTPUT slots. The engine tracks up to
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
		// Four independent computed OUTPUT slots are latched on every SUCCESS.
		// m_iPointerIndex selects an engine device slot; it is neither a graph
		// pin nor a blackboard selector.
		static constexpr u_int uPIN_Down = 0u;
		static constexpr u_int uPIN_Position = 1u;
		static constexpr u_int uPIN_Tap = 2u;
		static constexpr u_int uPIN_Count = 3u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_ReadPointer)
		ZENITH_GRAPH_PIN_OUTPUT(Down, PROPERTY_TYPE_BOOL)
		ZENITH_GRAPH_PIN_OUTPUT(Position, PROPERTY_TYPE_VECTOR2)
		ZENITH_GRAPH_PIN_OUTPUT(Tap, PROPERTY_TYPE_BOOL)
		ZENITH_GRAPH_PIN_OUTPUT(Count, PROPERTY_TYPE_INT32)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Pointers& xPointers = g_xEngine.Pointers();
			const bool bValidSlot = m_iPointerIndex >= 0
				&& static_cast<u_int32>(m_iPointerIndex) < Zenith_Pointers::uMAX_POINTERS;
			// A slot that does not exist reads as an absent pointer rather than
			// failing the chain: a graph polling slot 1 must not stall the frames
			// where only one finger is down.
			const Zenith_Pointer* pxPointer = bValidSlot ? &xPointers.GetPointer(static_cast<u_int32>(m_iPointerIndex)) : nullptr;

			// Down, Position, and Tap receive false/zero values for an absent or
			// invalid device slot. Count always reports the global active count.
			SetOutput<bool>(xContext, uPIN_Down, pxPointer != nullptr && pxPointer->IsDown());
			SetOutput<Zenith_Maths::Vector2>(xContext, uPIN_Position,
				pxPointer != nullptr ? pxPointer->m_xPosition : Zenith_Maths::Vector2(0.0f, 0.0f));
			SetOutput<bool>(xContext, uPIN_Tap, pxPointer != nullptr && pxPointer->m_bTapThisFrame);
			SetOutput<int32_t>(xContext, uPIN_Count, static_cast<int32_t>(xPointers.GetActivePointerCount()));
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ReadPointer"; }
	};

	// Main-camera ray under the mouse cursor -> origin + direction vec3 OUTPUT
	// slots. Wire Direction to Raycast directly; Origin requires an explicit
	// Vector3 writer and Raycast's permanent TargetPosition reference.
	// FAILURE when no loaded scene has a resolvable main camera.
	class Zenith_GraphNode_ReadMousePickRay : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ReadMousePickRay)
	public:
		static constexpr u_int uPIN_Origin = 0u;
		static constexpr u_int uPIN_Direction = 1u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_ReadMousePickRay)
		ZENITH_GRAPH_PIN_OUTPUT(Origin, PROPERTY_TYPE_VECTOR3)
		ZENITH_GRAPH_PIN_OUTPUT(Direction, PROPERTY_TYPE_VECTOR3)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			// The FAILURE sits ABOVE every accessor, so an unresolvable main camera
			// builds no pin state at all: there is no slot to read, and a consumer
			// wire off either output must be gated on SUCCESS (this node has no
			// routable failure pin).
			Zenith_CameraComponent* pxCamera = Zenith_GetMainCameraAcrossScenes();
			if (pxCamera == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const Zenith_PhysicsQuery::Ray xRay = Zenith_PhysicsQuery::BuildRayFromMouse(*pxCamera);
			SetOutput<Zenith_Maths::Vector3>(xContext, uPIN_Origin, xRay.m_xOrigin);
			SetOutput<Zenith_Maths::Vector3>(xContext, uPIN_Direction, xRay.m_xDirection);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ReadMousePickRay"; }
	};

	//==========================================================================
	// ACTION-LAYER nodes (B10)
	//
	// These name an ACTION, never a device code, and read the closed state of the
	// action layer. A graph anchored on ON_UPDATE runs at frame-contract step 11,
	// after both close stages (10b / 10e), so an action source node sees THIS
	// frame's edges - exactly what a C++ consumer sees.
	//==========================================================================

	// Name -> id resolution, cached per NODE INSTANCE.
	//
	// FindActionByName is a linear walk of 128 slots and these nodes execute
	// every frame on every graph carrying one, so resolving per Execute would
	// pay that walk forever. The cache re-resolves only when the property
	// STRING changes (an editor edit, a hot reload, or a param blob loaded over
	// the node), which is the only way the answer can move: the game's
	// registrations are installed once at boot.
	//
	// An unresolvable name makes the node INERT after exactly ONE logged error
	// - a graph authored against an action this game never registered must not
	// spam a line per frame, and must never take the process down. An EMPTY
	// name is silent: that is an unconfigured node, not a mistake.
	class Zenith_GraphActionRef
	{
	public:
		Zenith_InputActionID Resolve(const std::string& strActionName)
		{
			if (m_bResolved && m_strResolvedName == strActionName)
			{
				return m_uActionID;
			}

			m_strResolvedName = strActionName;
			m_bResolved       = true;
			m_uActionID       = strActionName.empty()
				? uINPUT_ACTION_INVALID
				: ActionLayer().FindActionByName(strActionName.c_str());

			if (m_uActionID == uINPUT_ACTION_INVALID && !strActionName.empty())
			{
				Zenith_Error(LOG_CATEGORY_CORE,
					"Behaviour graph action node names '%s', which no registered action matches; node is inert",
					strActionName.c_str());
			}
			return m_uActionID;
		}

	private:
		std::string          m_strResolvedName;
		Zenith_InputActionID m_uActionID  = uINPUT_ACTION_INVALID;
		bool                 m_bResolved  = false;
	};

	// Fires the frame the action's aggregate rises.
	class Zenith_GraphNode_OnActionPressed : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_OnActionPressed)
	public:
		ZENITH_PROPERTY(std::string, m_strAction, "")

		GraphNodeStatus Execute(Zenith_GraphContext&) override
		{
			const Zenith_InputActionID uAction = m_xActionRef.Resolve(m_strAction);
			return (uAction != uINPUT_ACTION_INVALID && ActionLayer().WasPressedThisFrame(uAction))
				? GRAPH_NODE_STATUS_SUCCESS : GRAPH_NODE_STATUS_FAILURE;
		}
		const char* GetTypeName() const override { return "OnActionPressed"; }

	private:
		Zenith_GraphActionRef m_xActionRef;
	};

	// Fires the frame the action's aggregate falls. A same-frame tap fires this
	// AND OnActionPressed - the action layer replays the frame's ordered
	// transitions rather than sampling a level, so a press and its release
	// inside one frame are both real edges.
	class Zenith_GraphNode_OnActionReleased : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_OnActionReleased)
	public:
		ZENITH_PROPERTY(std::string, m_strAction, "")

		GraphNodeStatus Execute(Zenith_GraphContext&) override
		{
			const Zenith_InputActionID uAction = m_xActionRef.Resolve(m_strAction);
			return (uAction != uINPUT_ACTION_INVALID && ActionLayer().WasReleasedThisFrame(uAction))
				? GRAPH_NODE_STATUS_SUCCESS : GRAPH_NODE_STATUS_FAILURE;
		}
		const char* GetTypeName() const override { return "OnActionReleased"; }

	private:
		Zenith_GraphActionRef m_xActionRef;
	};

	// Fires every frame the action is held.
	class Zenith_GraphNode_OnActionHeld : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_OnActionHeld)
	public:
		ZENITH_PROPERTY(std::string, m_strAction, "")

		GraphNodeStatus Execute(Zenith_GraphContext&) override
		{
			const Zenith_InputActionID uAction = m_xActionRef.Resolve(m_strAction);
			return (uAction != uINPUT_ACTION_INVALID && ActionLayer().IsHeld(uAction))
				? GRAPH_NODE_STATUS_SUCCESS : GRAPH_NODE_STATUS_FAILURE;
		}
		const char* GetTypeName() const override { return "OnActionHeld"; }

	private:
		Zenith_GraphActionRef m_xActionRef;
	};

	// AXIS1D action -> float var. Unlike the device-layer query nodes this one
	// FAILS (and writes nothing) when the action does not resolve: a chain
	// downstream of it would otherwise run on a zero that is indistinguishable
	// from a centred stick.
	class Zenith_GraphNode_ReadActionAxis1D : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ReadActionAxis1D)
	public:
		ZENITH_PROPERTY(std::string, m_strAction, "")
		// m_strAction names a registered INPUT ACTION, not a blackboard
		// variable - it is resolved against g_xEngine.Actions() and is not a pin.
		static constexpr u_int uPIN_Result = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_ReadActionAxis1D)
		ZENITH_GRAPH_PIN_OUTPUT(Result, PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			// The unresolvable-action FAILURE stays ABOVE the write, so an inert
			// node builds no pin state and latches nothing.
			const Zenith_InputActionID uAction = m_xActionRef.Resolve(m_strAction);
			if (uAction == uINPUT_ACTION_INVALID)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			SetOutput<float>(xContext, uPIN_Result, ActionLayer().GetAxis1D(uAction));
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ReadActionAxis1D"; }

	private:
		Zenith_GraphActionRef m_xActionRef;
	};

	// AXIS2D action -> vec2 var. +y is FORWARD and diagonals are UNNORMALISED
	// (Zenith_InputActions::ResolveMoveComposite is the contract); a graph that
	// wants a unit direction normalizes it itself, exactly as a C++ consumer
	// does.
	class Zenith_GraphNode_ReadActionAxis2D : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ReadActionAxis2D)
	public:
		ZENITH_PROPERTY(std::string, m_strAction, "")
		static constexpr u_int uPIN_Result = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_ReadActionAxis2D)
		ZENITH_GRAPH_PIN_OUTPUT(Result, PROPERTY_TYPE_VECTOR2)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			// As ReadActionAxis1D: the FAILURE precedes the write.
			const Zenith_InputActionID uAction = m_xActionRef.Resolve(m_strAction);
			if (uAction == uINPUT_ACTION_INVALID)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_Maths::Vector2 xAxis(0.0f, 0.0f);
			ActionLayer().GetAxis2D(uAction, xAxis);
			SetOutput<Zenith_Maths::Vector2>(xContext, uPIN_Result, xAxis);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ReadActionAxis2D"; }

	private:
		Zenith_GraphActionRef m_xActionRef;
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

	// Action layer (B10). Same category as the device nodes: a designer picking
	// input for a graph should see both halves in one place, with the action
	// nodes reading as the obvious default.
	xRegistry.RegisterNodeType<Zenith_GraphNode_OnActionPressed>("OnActionPressed", GRAPH_EVENT_ON_UPDATE, 1, false, "Input");
	xRegistry.RegisterNodeType<Zenith_GraphNode_OnActionReleased>("OnActionReleased", GRAPH_EVENT_ON_UPDATE, 1, false, "Input");
	xRegistry.RegisterNodeType<Zenith_GraphNode_OnActionHeld>("OnActionHeld", GRAPH_EVENT_ON_UPDATE, 1, false, "Input");
	xRegistry.RegisterNodeType<Zenith_GraphNode_ReadActionAxis1D>("ReadActionAxis1D", GRAPH_EVENT_NONE, 1, false, "Input");
	xRegistry.RegisterNodeType<Zenith_GraphNode_ReadActionAxis2D>("ReadActionAxis2D", GRAPH_EVENT_NONE, 1, false, "Input");
}

#include "EntityComponent/Zenith_GraphNode_Registration_Input.Tests.inl"
