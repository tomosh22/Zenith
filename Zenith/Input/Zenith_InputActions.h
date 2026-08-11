#pragma once

#include "Input/Zenith_Input.h"
#include "Maths/Zenith_Maths.h"

class Zenith_Pointers;

// ============================================================================
// Zenith_InputActions — the ACTION layer (B4 / B5 / B8).
//
// Sits directly on top of Zenith_Input (the device layer) and Zenith_Pointers
// (the pointer table). A game asks "is JUMP held" rather than "is SPACE down",
// and the binding table decides which device columns — SCHEMES — answer that
// question this frame.
//
// FRAME CONTRACT (Zenith_Core / Zenith_UISystem own the call sites; see
// Zenith_Input.h for steps 1-7):
//   8.  UpdateProfile()      — activity detection over this frame's transitions
//       (EDGES and threshold CROSSINGS only, with hysteresis; system-nav
//       excluded) -> auto profile switch / forced override. On ANY mask change
//       the MASK-CHANGE SYNTHESIS runs here, BEFORE any replay. Also opens the
//       action frame: per-action edges are cleared and the source shadow is
//       snapshotted so both close stages replay from the same start state.
//   10b. FinalizeReservedUI() — closes ONLY the engine-reserved, claim-
//       independent UI actions (ids 0-15). Runs inside the UI input phase after
//       the layout refresh and BEFORE the standard-control capture walk, so a
//       widget claim can never suppress UI navigation.
//   10e. FinalizeGameplay()  — closes every remaining action AFTER the capture
//       walk. Pointer-sourced bindings (TOUCH_PRIMARY, and MOUSE_BUTTON
//       transitions fed by a pointer — the B3 projection and the real Windows
//       mouse alike) SUPPRESS any transition whose pointer is CLAIMED by then;
//       a suppressed press suppresses its matching release. Keyboard and
//       gamepad transitions are NEVER claim-filtered.
//   11. Game logic: all action state is closed, claims and suppression final.
//
// The queries are NON-CONSUMING reads of closed state: two readers in the same
// frame both see an edge, and mutual exclusion stays the caller's job.
//
// This is a PLAIN INSTANCE CLASS. The engine owns one (g_xEngine.Actions());
// unit tests construct LOCAL instances and inject local device/pointer layers,
// so they never collide with a game's registrations on the global (B13). This
// TU must never reach the engine singleton — Zenith_Engine / Zenith_Core /
// Zenith_UISystem drive it, exactly as they drive Zenith_Pointers.
// ============================================================================

// The binding-table COLUMNS. A profile is a named mask over these.
enum Zenith_EInputScheme : u_int8
{
	INPUT_SCHEME_KEYBOARD = 0,
	INPUT_SCHEME_MOUSE,
	INPUT_SCHEME_TOUCH,
	INPUT_SCHEME_GAMEPAD,
	INPUT_SCHEME_COUNT,
	INPUT_SCHEME_NONE = 0xFF
};

static constexpr u_int8 uINPUT_SCHEME_MASK_KEYBOARD = 1u << INPUT_SCHEME_KEYBOARD;
static constexpr u_int8 uINPUT_SCHEME_MASK_MOUSE    = 1u << INPUT_SCHEME_MOUSE;
static constexpr u_int8 uINPUT_SCHEME_MASK_TOUCH    = 1u << INPUT_SCHEME_TOUCH;
static constexpr u_int8 uINPUT_SCHEME_MASK_GAMEPAD  = 1u << INPUT_SCHEME_GAMEPAD;
// The desktop trio the engine-default Windows profile carries.
static constexpr u_int8 uINPUT_SCHEME_MASK_DESKTOP  =
	uINPUT_SCHEME_MASK_KEYBOARD | uINPUT_SCHEME_MASK_MOUSE | uINPUT_SCHEME_MASK_GAMEPAD;

// What an action's value IS. RegisterBinding asserts the binding can produce it.
enum Zenith_EInputActionKind : u_int8
{
	INPUT_ACTION_BUTTON = 0,
	INPUT_ACTION_AXIS1D,
	INPUT_ACTION_AXIS2D
};

// The binding union (B8). Every entry is one row of one scheme's column.
enum Zenith_EInputBindingType : u_int8
{
	INPUT_BINDING_NONE = 0,
	INPUT_BINDING_KEY_SET,                 // button: any key in the set
	INPUT_BINDING_KEY_AXIS1D,              // axis1D: negative set / positive set
	INPUT_BINDING_KEY_AXIS2D,              // axis2D: forward/back/left/right sets
	INPUT_BINDING_MOUSE_BUTTON,            // button; claim-filtered at 10e
	INPUT_BINDING_MOUSE_WHEEL,             // axis1D: this frame's wheel * scale
	INPUT_BINDING_MOUSE_DELTA,             // axis2D: this frame's displacement * scale
	INPUT_BINDING_TOUCH_PRIMARY,           // button: primary pointer down AND unclaimed
	INPUT_BINDING_SYSTEM_BACK,             // button; MASK-EXEMPT, fed by SYSTEM_BACK events
	INPUT_BINDING_GAMEPAD_BUTTON,          // button
	INPUT_BINDING_GAMEPAD_STICK,           // axis2D, independent per-axis inversion
	INPUT_BINDING_GAMEPAD_AXIS,            // axis1D
	INPUT_BINDING_GAMEPAD_AXIS_AS_BUTTON,  // button with press/release hysteresis
	INPUT_BINDING_GAMEPAD_TRIGGER_PAIR,    // axis1D: RT - LT
	INPUT_BINDING_GAMEPAD_DPAD_AXIS2D,     // axis2D composite from the d-pad
	INPUT_BINDING_VIRTUAL,                 // fed by a widget publish (WP3a)
	INPUT_BINDING_COUNT
};

typedef u_int16 Zenith_InputActionID;
static constexpr Zenith_InputActionID uINPUT_ACTION_INVALID = 0xFFFF;

// Engine-reserved action ids. Games may not register anything below
// uINPUT_ACTION_FIRST_GAME_ID; the engine registers these itself at boot and
// the UI canvas is their SOLE consumer.
enum Zenith_EInputReservedAction : u_int16
{
	INPUT_ACTION_UI_NAV_UP    = 0,
	INPUT_ACTION_UI_NAV_DOWN  = 1,
	INPUT_ACTION_UI_NAV_LEFT  = 2,
	INPUT_ACTION_UI_NAV_RIGHT = 3,
	INPUT_ACTION_UI_CONFIRM   = 4
};
static constexpr Zenith_InputActionID uINPUT_ACTION_RESERVED_COUNT  = 16;
static constexpr Zenith_InputActionID uINPUT_ACTION_FIRST_GAME_ID   = uINPUT_ACTION_RESERVED_COUNT;

// One row of the binding table. Deliberately a flat POD rather than a real
// union: the whole table is a fixed array inside the action registry, the
// per-type fields are a handful of scalars, and a POD keeps the constexpr
// factories below trivially usable in a game's static binding list.
struct Zenith_InputBinding
{
	static constexpr u_int32 uMAX_KEY_SETS     = 4;
	static constexpr u_int32 uMAX_KEYS_PER_SET = 4;

	// Set indices, by binding type.
	static constexpr u_int32 uSET_NEGATIVE = 0;   // KEY_AXIS1D
	static constexpr u_int32 uSET_POSITIVE = 1;
	static constexpr u_int32 uSET_FORWARD  = 0;   // KEY_AXIS2D
	static constexpr u_int32 uSET_BACK     = 1;
	static constexpr u_int32 uSET_LEFT     = 2;
	static constexpr u_int32 uSET_RIGHT    = 3;

	Zenith_EInputBindingType m_eType = INPUT_BINDING_NONE;

	int32_t m_aaiKeys[uMAX_KEY_SETS][uMAX_KEYS_PER_SET] = {};
	u_int8  m_auKeyCount[uMAX_KEY_SETS] = {};

	// Mouse button / pad button / axis index / virtual source id.
	int32_t m_iCode   = 0;
	// Gamepad index; 0 for every other device.
	int32_t m_iDevice = 0;

	// Independent per-axis scale, so a stick can be inverted on one axis only.
	float m_fScaleX = 1.0f;
	float m_fScaleY = 1.0f;
	// GAMEPAD_AXIS_AS_BUTTON hysteresis. Press at/above, release at/below.
	float m_fPressAt   = 0.5f;
	float m_fReleaseAt = 0.4f;

	// Which column this row lives in. SYSTEM_BACK is mask-EXEMPT and answers
	// INPUT_SCHEME_NONE; a VIRTUAL source belongs to TOUCH (it is published by
	// the on-screen controls, which are a touch-scheme concept).
	Zenith_EInputScheme GetScheme() const;
	bool IsMaskExempt() const { return m_eType == INPUT_BINDING_SYSTEM_BACK; }
	// True when this row's held state is fed by the ORDERED transition log
	// rather than sampled as a level at close time.
	bool IsTransitionFed() const;

	// ---- constexpr factories (B8) ----------------------------------------
	static constexpr Zenith_InputBinding KeySet(int32_t iKeyA, int32_t iKeyB = -1,
		int32_t iKeyC = -1, int32_t iKeyD = -1)
	{
		Zenith_InputBinding xBinding;
		xBinding.m_eType = INPUT_BINDING_KEY_SET;
		xBinding.PushKey(uSET_NEGATIVE, iKeyA);
		xBinding.PushKey(uSET_NEGATIVE, iKeyB);
		xBinding.PushKey(uSET_NEGATIVE, iKeyC);
		xBinding.PushKey(uSET_NEGATIVE, iKeyD);
		return xBinding;
	}

	static constexpr Zenith_InputBinding KeyAxis1D(int32_t iNegative, int32_t iPositive)
	{
		Zenith_InputBinding xBinding;
		xBinding.m_eType = INPUT_BINDING_KEY_AXIS1D;
		xBinding.PushKey(uSET_NEGATIVE, iNegative);
		xBinding.PushKey(uSET_POSITIVE, iPositive);
		return xBinding;
	}

	static constexpr Zenith_InputBinding KeyAxis2D(int32_t iForward, int32_t iBack,
		int32_t iLeft, int32_t iRight)
	{
		Zenith_InputBinding xBinding;
		xBinding.m_eType = INPUT_BINDING_KEY_AXIS2D;
		xBinding.PushKey(uSET_FORWARD, iForward);
		xBinding.PushKey(uSET_BACK,    iBack);
		xBinding.PushKey(uSET_LEFT,    iLeft);
		xBinding.PushKey(uSET_RIGHT,   iRight);
		return xBinding;
	}

	// Alternate key for one of the sets above (WASD + arrows on one binding).
	constexpr Zenith_InputBinding WithKey(u_int32 uSet, int32_t iKey) const
	{
		Zenith_InputBinding xBinding = *this;
		xBinding.PushKey(uSet, iKey);
		return xBinding;
	}

	static constexpr Zenith_InputBinding MouseButton(int32_t iButton)
	{
		Zenith_InputBinding xBinding;
		xBinding.m_eType = INPUT_BINDING_MOUSE_BUTTON;
		xBinding.m_iCode = iButton;
		return xBinding;
	}

	static constexpr Zenith_InputBinding MouseWheel(float fScale)
	{
		Zenith_InputBinding xBinding;
		xBinding.m_eType   = INPUT_BINDING_MOUSE_WHEEL;
		xBinding.m_fScaleX = fScale;
		return xBinding;
	}

	static constexpr Zenith_InputBinding MouseDelta(float fScaleX, float fScaleY)
	{
		Zenith_InputBinding xBinding;
		xBinding.m_eType   = INPUT_BINDING_MOUSE_DELTA;
		xBinding.m_fScaleX = fScaleX;
		xBinding.m_fScaleY = fScaleY;
		return xBinding;
	}

	static constexpr Zenith_InputBinding TouchPrimary()
	{
		Zenith_InputBinding xBinding;
		xBinding.m_eType = INPUT_BINDING_TOUCH_PRIMARY;
		return xBinding;
	}

	static constexpr Zenith_InputBinding SystemBack()
	{
		Zenith_InputBinding xBinding;
		xBinding.m_eType = INPUT_BINDING_SYSTEM_BACK;
		return xBinding;
	}

	static constexpr Zenith_InputBinding GamepadButton(int32_t iButton, int32_t iGamepad = 0)
	{
		Zenith_InputBinding xBinding;
		xBinding.m_eType   = INPUT_BINDING_GAMEPAD_BUTTON;
		xBinding.m_iCode   = iButton;
		xBinding.m_iDevice = iGamepad;
		return xBinding;
	}

	// iStickAxisX is the X axis of the pair (ZENITH_GAMEPAD_AXIS_LEFT_X or
	// _RIGHT_X); the Y axis of the same stick is iStickAxisX + 1.
	static constexpr Zenith_InputBinding GamepadStick(int32_t iStickAxisX, float fScaleX,
		float fScaleY, int32_t iGamepad = 0)
	{
		Zenith_InputBinding xBinding;
		xBinding.m_eType   = INPUT_BINDING_GAMEPAD_STICK;
		xBinding.m_iCode   = iStickAxisX;
		xBinding.m_iDevice = iGamepad;
		xBinding.m_fScaleX = fScaleX;
		xBinding.m_fScaleY = fScaleY;
		return xBinding;
	}

	static constexpr Zenith_InputBinding GamepadAxis(int32_t iAxis, float fScale, int32_t iGamepad = 0)
	{
		Zenith_InputBinding xBinding;
		xBinding.m_eType   = INPUT_BINDING_GAMEPAD_AXIS;
		xBinding.m_iCode   = iAxis;
		xBinding.m_iDevice = iGamepad;
		xBinding.m_fScaleX = fScale;
		return xBinding;
	}

	static constexpr Zenith_InputBinding GamepadAxisAsButton(int32_t iAxis, float fPressAt,
		float fReleaseAt, int32_t iGamepad = 0)
	{
		Zenith_InputBinding xBinding;
		xBinding.m_eType      = INPUT_BINDING_GAMEPAD_AXIS_AS_BUTTON;
		xBinding.m_iCode      = iAxis;
		xBinding.m_iDevice    = iGamepad;
		xBinding.m_fPressAt   = fPressAt;
		xBinding.m_fReleaseAt = fReleaseAt;
		return xBinding;
	}

	static constexpr Zenith_InputBinding GamepadTriggerPair(float fScale, int32_t iGamepad = 0)
	{
		Zenith_InputBinding xBinding;
		xBinding.m_eType   = INPUT_BINDING_GAMEPAD_TRIGGER_PAIR;
		xBinding.m_iDevice = iGamepad;
		xBinding.m_fScaleX = fScale;
		return xBinding;
	}

	static constexpr Zenith_InputBinding GamepadDpadAxis2D(int32_t iGamepad = 0)
	{
		Zenith_InputBinding xBinding;
		xBinding.m_eType   = INPUT_BINDING_GAMEPAD_DPAD_AXIS2D;
		xBinding.m_iDevice = iGamepad;
		return xBinding;
	}

	static constexpr Zenith_InputBinding Virtual(u_int16 uVirtualId)
	{
		Zenith_InputBinding xBinding;
		xBinding.m_eType = INPUT_BINDING_VIRTUAL;
		xBinding.m_iCode = static_cast<int32_t>(uVirtualId);
		return xBinding;
	}

private:
	// Silently ignores a negative key (the factories' unused defaults) and a
	// full set, so the constexpr factories stay expression-shaped.
	constexpr void PushKey(u_int32 uSet, int32_t iKey)
	{
		if (iKey < 0 || uSet >= uMAX_KEY_SETS || m_auKeyCount[uSet] >= uMAX_KEYS_PER_SET)
		{
			return;
		}
		m_aaiKeys[uSet][m_auKeyCount[uSet]] = iKey;
		m_auKeyCount[uSet]++;
	}
};

// Profile / last-used-device notification. Function POINTERS, not the ECS event
// dispatcher: Input sits BELOW ZenithECS and must not reach it.
typedef void (*Zenith_InputProfileChangedCallback)(void* pxUserData, u_int8 uProfileId);
typedef void (*Zenith_InputLastDeviceCallback)(void* pxUserData, Zenith_EInputScheme eScheme);

class Zenith_InputActions
{
public:
	Zenith_InputActions() = default;
	~Zenith_InputActions() = default;

	Zenith_InputActions(const Zenith_InputActions&) = delete;
	Zenith_InputActions& operator=(const Zenith_InputActions&) = delete;

	static constexpr u_int32 uMAX_ACTIONS             = 128;
	static constexpr u_int32 uMAX_BINDINGS_PER_ACTION = 4;
	static constexpr u_int32 uMAX_PROFILES            = 8;
	static constexpr u_int32 uMAX_VIRTUAL_SOURCES     = 32;
	static constexpr u_int32 uMAX_VIRTUAL_TRANSITIONS = 32;
	static constexpr u_int32 uMAX_CALLBACKS           = 8;
	static constexpr u_int32 uMAX_NAME_LENGTH         = 32;

	// 0xFF means AUTO and is therefore not a registrable profile id. The two
	// engine-default profiles take reserved ids of their own so a game's ids
	// (any other value) can never collide with them.
	static constexpr u_int8 uPROFILE_AUTO            = 0xFF;
	static constexpr u_int8 uPROFILE_ENGINE_DESKTOP  = 0xF0;
	static constexpr u_int8 uPROFILE_ENGINE_TOUCH    = 0xF1;

	// Activity thresholds (B5). The mouse one is in RAW SURFACE PIXELS per
	// frame; the pad one is the device layer's own stick deadzone, so "the
	// stick moved" means exactly what every other consumer means by it.
	static constexpr float fMOUSE_ACTIVITY_PIXELS = 3.0f;
	static constexpr float fPAD_ACTIVITY_DEADZONE = Zenith_Input::GAMEPAD_DEADZONE;

	// Dependency-injection seam (the Zenith_UISystem::Initialise pattern): the
	// device layer and the pointer table are handed in, so this TU never names
	// the engine singleton. Unit tests inject LOCAL instances.
	void Initialise(Zenith_Input& xInput, Zenith_Pointers& xPointers);

	// ---- Registration -----------------------------------------------------
	// Game ids must be >= uINPUT_ACTION_FIRST_GAME_ID: 0-15 are engine-reserved
	// and registered by RegisterEngineReservedActions() alone.
	void RegisterAction(Zenith_InputActionID uId, const char* szName, Zenith_EInputActionKind eKind);
	void RegisterBinding(Zenith_InputActionID uId, const Zenith_InputBinding& xBinding);
	Zenith_InputActionID FindActionByName(const char* szName) const;

	bool IsActionRegistered(Zenith_InputActionID uId) const;
	Zenith_EInputActionKind GetActionKind(Zenith_InputActionID uId) const;
	u_int32 GetBindingCount(Zenith_InputActionID uId) const;
	const Zenith_InputBinding& GetBinding(Zenith_InputActionID uId, u_int32 uIndex) const;

	// Engine boot wiring (Zenith_Engine calls both, nothing else may).
	// The five reserved UI actions + their device-only bindings, and the
	// platform's engine-default profile.
	void RegisterEngineReservedActions();
	void RegisterEngineDefaultProfiles();

	// ---- Profiles (B5) ----------------------------------------------------
	// A scheme may appear in at most ONE registered profile (asserted): the
	// auto-switch resolves a scheme to a profile, and two answers is not a
	// resolution. The FIRST game call clears the engine defaults wholesale.
	void RegisterProfile(u_int8 uProfileId, const char* szName, u_int8 uSchemeMask);
	bool IsProfileRegistered(u_int8 uProfileId) const;
	u_int8 GetProfileSchemeMask(u_int8 uProfileId) const;

	// Name lookups. A profile's NAME is its stable identity: the persisted user
	// setting (Zenith_UserSettings, B12) stores the name and never the numeric
	// id, so reordering a game's RegisterProfile calls can never make a saved
	// setting resolve to a DIFFERENT profile. FindProfileByName answers
	// uPROFILE_AUTO for an unknown (or empty, or null) name — which is exactly
	// the "unknown name => AUTO" fallback the settings store depends on.
	u_int8 FindProfileByName(const char* szName) const;
	// "" when the id is not a registered profile.
	const char* GetProfileName(u_int8 uProfileId) const;
	u_int8 GetActiveProfile() const { return m_uActiveProfile; }
	u_int8 GetActiveSchemeMask() const;
	bool AreEngineDefaultProfilesActive() const { return m_bEngineDefaultProfiles; }

	// Forces a profile and SUSPENDS the auto-switch; ClearOverride returns to
	// AUTO. Both run the mask-change synthesis.
	void SetProfileOverride(u_int8 uProfileId);
	void ClearOverride();
	bool IsProfileOverridden() const { return m_bOverrideActive; }

	// Tracked separately from the profile, and FINER: a pad press inside a
	// profile that already owns the pad changes the glyph set without changing
	// the profile.
	Zenith_EInputScheme GetLastUsedDevice() const { return m_eLastUsedDevice; }

	void AddProfileChangedCallback(Zenith_InputProfileChangedCallback pfnCallback, void* pxUserData);
	void AddLastUsedDeviceCallback(Zenith_InputLastDeviceCallback pfnCallback, void* pxUserData);

	// ---- Frame lifecycle --------------------------------------------------
	void UpdateProfile();        // step 8
	void FinalizeReservedUI();   // step 10b
	void FinalizeGameplay();     // step 10e

	// ---- Virtual sources --------------------------------------------------
	// The publish API the on-screen controls will use (WP3a) bottoms out here.
	// A button publish becomes an ORDERED virtual transition replayed at 10e,
	// after this frame's device transitions; an axis publish is level state.
	void PublishVirtualButton(u_int16 uVirtualId, bool bDown);
	void PublishVirtualAxis(u_int16 uVirtualId, float fX, float fY);

	// ---- Queries (NON-CONSUMING reads of closed state) --------------------
	bool IsHeld(Zenith_InputActionID uId) const;
	bool WasPressedThisFrame(Zenith_InputActionID uId) const;
	bool WasReleasedThisFrame(Zenith_InputActionID uId) const;
	float GetAxis1D(Zenith_InputActionID uId) const;
	void GetAxis2D(Zenith_InputActionID uId, Zenith_Maths::Vector2& xOut) const;

	// Between automated tests: aggregates, edges, axis values, the source
	// shadow, virtual sources, the profile override (-> AUTO) and the last-used
	// device. REGISTRATIONS (actions, bindings, profiles, callbacks) survive.
	void ResetTransientForTest();

	// The composite-move rule, pure and public because it is a contract, not an
	// implementation detail: +y is FORWARD, diagonals are UNNORMALISED, and
	// opposite keys CANCEL.
	static Zenith_Maths::Vector2 ResolveMoveComposite(bool bForward, bool bBack, bool bLeft, bool bRight);

private:
	struct Action
	{
		char                     m_szName[uMAX_NAME_LENGTH] = {};
		Zenith_EInputActionKind  m_eKind        = INPUT_ACTION_BUTTON;
		bool                     m_bRegistered  = false;
		u_int8                   m_uBindingCount = 0;
		Zenith_InputBinding      m_axBindings[uMAX_BINDINGS_PER_ACTION] = {};

		// Per-binding held state — the aggregate is the OR over the bindings
		// the active mask enables. Keeping it per binding is what makes
		// "release one binding while another stays held" fire nothing.
		bool m_abBindingHeld[uMAX_BINDINGS_PER_ACTION] = {};

		bool  m_bHeld     = false;
		bool  m_bPressed  = false;
		bool  m_bReleased = false;
		bool  m_bClosed   = false;
		// A release the mask-change synthesis produced, held over the next frame
		// boundary until a close stage has actually PUBLISHED it. Without this a
		// SetProfileOverride from game logic (step 11, after both closes) would
		// have its synthesized release wiped by the next frame's edge reset
		// before any reader could see it.
		bool  m_bSynthesizedRelease = false;
		float m_fAxisX    = 0.0f;
		float m_fAxisY    = 0.0f;
	};

	struct Profile
	{
		char   m_szName[uMAX_NAME_LENGTH] = {};
		u_int8 m_uId         = uPROFILE_AUTO;
		u_int8 m_uSchemeMask = 0;
		bool   m_bRegistered = false;
	};

	struct VirtualTransition
	{
		u_int16 m_uVirtualId = 0;
		bool    m_bDown      = false;
	};

	// --- close stages ---
	void OpenActionFrame();
	void CloseStage(bool bReservedStage);
	void ApplyLevelSources(bool bReservedStage);
	void ReplayTransitions(bool bReservedStage);
	void ReplayVirtualTransitions(bool bReservedStage);
	void ResolveAxisValues(bool bReservedStage);
	bool IsActionInStage(const Action& xAction, Zenith_InputActionID uId, bool bReservedStage) const;

	// --- source shadow ---
	void ApplyTransitionToShadow(const Zenith_InputEvent& xEvent);
	void SnapshotShadow();
	void RestoreShadowToFrameStart();
	bool IsKeySetHeld(const Zenith_InputBinding& xBinding, u_int32 uSet) const;
	bool ComputeTransitionFedHeld(const Zenith_InputBinding& xBinding) const;
	void RefreshTransitionFedHeld(Action& xAction);
	bool ComputeAggregateHeld(const Action& xAction) const;
	void CommitAggregate(Action& xAction);

	// --- activity + profiles ---
	u_int8 DetectActivityMask();
	Zenith_EInputScheme PickActivityWinner(u_int8 uActivityMask) const;
	u_int8 FindProfileForScheme(Zenith_EInputScheme eScheme) const;
	u_int32 FindProfileSlot(u_int8 uProfileId) const;
	void SetActiveProfile(u_int8 uProfileId);
	void ResolveBootDefaultProfile();
	void RebaseUnderNewMask();
	void NotifyProfileChanged();
	void NotifyLastUsedDevice();

	// --- claim suppression (10e only) ---
	void UpdateMouseSuppression(const Zenith_InputEvent& xEvent);
	bool IsPrimaryPointerClaimed() const;

	Zenith_Input*    m_pxInput    = nullptr;
	Zenith_Pointers* m_pxPointers = nullptr;

	Action  m_axActions[uMAX_ACTIONS];
	Profile m_axProfiles[uMAX_PROFILES];
	u_int32 m_uProfileCount = 0;

	u_int8 m_uActiveProfile   = uPROFILE_AUTO;
	u_int8 m_uOverrideProfile = uPROFILE_AUTO;
	bool   m_bOverrideActive  = false;
	bool   m_bEngineDefaultProfiles = false;

	Zenith_EInputScheme m_eLastUsedDevice = INPUT_SCHEME_NONE;

	// Source shadow: the held state the ORDERED replay walks. Fed by the
	// transition log rather than polled, because a replay needs the state AT
	// each transition, not the state after all of them.
	bool m_abKeyShadow[Zenith_Input::MAX_KEY_CODES] = {};
	bool m_aabPadShadow[Zenith_Input::MAX_GAMEPADS][Zenith_GamepadSnapshot::iBUTTON_COUNT] = {};
	// ...and its start-of-frame copy. BOTH close stages replay the same log, so
	// the second one has to start where the first one did.
	bool m_abKeyShadowAtFrameStart[Zenith_Input::MAX_KEY_CODES] = {};
	bool m_aabPadShadowAtFrameStart[Zenith_Input::MAX_GAMEPADS][Zenith_GamepadSnapshot::iBUTTON_COUNT] = {};

	// A mouse button whose PRESS was suppressed by a claim stays suppressed
	// until its release, so the release cannot leak through on its own.
	bool m_abMouseSuppressed[ZENITH_MOUSE_BUTTON_LAST + 1] = {};

	// Raised for exactly the length of one SYSTEM_BACK replay step: the gesture
	// has no held phase, so the row PULSES (rise then fall) inside that step.
	bool m_bSystemBackPulse = false;

	// Activity CROSSING latches: "was past the threshold last time we looked".
	// An axis that stays past the deadzone notifies ONCE — it has to fall back
	// inside before it can count again. Stored as latched-not-armed so the
	// all-zero default IS the resting state.
	bool m_aabPadAxisActivityLatched[Zenith_Input::MAX_GAMEPADS][Zenith_GamepadSnapshot::iAXIS_COUNT] = {};
	bool m_bMouseMoveActivityLatched = false;

	bool  m_abVirtualHeld[uMAX_VIRTUAL_SOURCES]  = {};
	float m_afVirtualAxisX[uMAX_VIRTUAL_SOURCES] = {};
	float m_afVirtualAxisY[uMAX_VIRTUAL_SOURCES] = {};
	VirtualTransition m_axVirtualTransitions[uMAX_VIRTUAL_TRANSITIONS] = {};
	u_int32           m_uVirtualTransitionCount = 0;

	Zenith_InputProfileChangedCallback m_apfnProfileCallbacks[uMAX_CALLBACKS] = {};
	void*                              m_apxProfileCallbackData[uMAX_CALLBACKS] = {};
	u_int32                            m_uProfileCallbackCount = 0;
	Zenith_InputLastDeviceCallback     m_apfnDeviceCallbacks[uMAX_CALLBACKS] = {};
	void*                              m_apxDeviceCallbackData[uMAX_CALLBACKS] = {};
	u_int32                            m_uDeviceCallbackCount = 0;

	// Set while RegisterEngineReservedActions / RegisterEngineDefaultProfiles
	// run, so the "games may not touch ids 0-15 / the engine defaults" asserts
	// do not fire on the engine's own registration.
	bool m_bEngineRegistrationInProgress = false;
};
