#include "Zenith.h"

#include "Input/Zenith_InputActions.h"
#include "Input/Zenith_Pointers.h"

// ============================================================================
// Small helpers
// ============================================================================

// No CRT string copy: strncpy_s is MSVC-only and strncpy warns under /W4, and
// this file compiles for Android too. Names are short and fixed-width.
static void Zenith_CopyActionName(char* szDest, u_int32 uCapacity, const char* szSource)
{
	u_int32 u = 0;
	if (szSource != nullptr)
	{
		while (u + 1 < uCapacity && szSource[u] != '\0')
		{
			szDest[u] = szSource[u];
			u++;
		}
	}
	szDest[u] = '\0';
}

static bool Zenith_ActionNamesMatch(const char* szA, const char* szB)
{
	if (szA == nullptr || szB == nullptr)
	{
		return false;
	}
	u_int32 u = 0;
	while (szA[u] != '\0' && szA[u] == szB[u])
	{
		u++;
	}
	return szA[u] == szB[u];
}

static bool Zenith_IsMouseButtonCode(int32_t iCode)
{
	return iCode >= 0 && iCode <= ZENITH_MOUSE_BUTTON_LAST;
}

// ============================================================================
// Zenith_InputBinding
// ============================================================================

Zenith_EInputScheme Zenith_InputBinding::GetScheme() const
{
	switch (m_eType)
	{
	case INPUT_BINDING_KEY_SET:
	case INPUT_BINDING_KEY_AXIS1D:
	case INPUT_BINDING_KEY_AXIS2D:
		return INPUT_SCHEME_KEYBOARD;

	case INPUT_BINDING_MOUSE_BUTTON:
	case INPUT_BINDING_MOUSE_WHEEL:
	case INPUT_BINDING_MOUSE_DELTA:
		return INPUT_SCHEME_MOUSE;

	// A VIRTUAL source is published by the on-screen controls, and those are a
	// TOUCH-scheme concept: a profile that does not enable TOUCH is a profile
	// whose on-screen controls are not on screen.
	case INPUT_BINDING_TOUCH_PRIMARY:
	case INPUT_BINDING_VIRTUAL:
		return INPUT_SCHEME_TOUCH;

	case INPUT_BINDING_GAMEPAD_BUTTON:
	case INPUT_BINDING_GAMEPAD_STICK:
	case INPUT_BINDING_GAMEPAD_AXIS:
	case INPUT_BINDING_GAMEPAD_AXIS_AS_BUTTON:
	case INPUT_BINDING_GAMEPAD_TRIGGER_PAIR:
	case INPUT_BINDING_GAMEPAD_DPAD_AXIS2D:
		return INPUT_SCHEME_GAMEPAD;

	// SYSTEM_BACK is mask-EXEMPT: the platform's own navigation gesture must
	// work whatever the active profile is.
	case INPUT_BINDING_SYSTEM_BACK:
	default:
		return INPUT_SCHEME_NONE;
	}
}

bool Zenith_InputBinding::IsTransitionFed() const
{
	switch (m_eType)
	{
	case INPUT_BINDING_KEY_SET:
	case INPUT_BINDING_KEY_AXIS1D:
	case INPUT_BINDING_KEY_AXIS2D:
	case INPUT_BINDING_MOUSE_BUTTON:
	case INPUT_BINDING_GAMEPAD_BUTTON:
	case INPUT_BINDING_GAMEPAD_DPAD_AXIS2D:
	case INPUT_BINDING_SYSTEM_BACK:
	case INPUT_BINDING_VIRTUAL:
		return true;
	default:
		return false;
	}
}

// Which action kinds a binding row can produce a value for.
static bool Zenith_IsBindingKindCompatible(Zenith_EInputBindingType eType, Zenith_EInputActionKind eKind)
{
	switch (eType)
	{
	case INPUT_BINDING_KEY_SET:
	case INPUT_BINDING_MOUSE_BUTTON:
	case INPUT_BINDING_TOUCH_PRIMARY:
	case INPUT_BINDING_SYSTEM_BACK:
	case INPUT_BINDING_GAMEPAD_BUTTON:
	case INPUT_BINDING_GAMEPAD_AXIS_AS_BUTTON:
		return eKind == INPUT_ACTION_BUTTON;

	case INPUT_BINDING_KEY_AXIS1D:
	case INPUT_BINDING_MOUSE_WHEEL:
	case INPUT_BINDING_GAMEPAD_AXIS:
	case INPUT_BINDING_GAMEPAD_TRIGGER_PAIR:
		return eKind == INPUT_ACTION_AXIS1D;

	case INPUT_BINDING_KEY_AXIS2D:
	case INPUT_BINDING_MOUSE_DELTA:
	case INPUT_BINDING_GAMEPAD_STICK:
	case INPUT_BINDING_GAMEPAD_DPAD_AXIS2D:
		return eKind == INPUT_ACTION_AXIS2D;

	// A published virtual source is a button OR a stick, depending on what the
	// widget publishing it is.
	case INPUT_BINDING_VIRTUAL:
		return true;

	default:
		return false;
	}
}

// ============================================================================
// Construction / registration
// ============================================================================

void Zenith_InputActions::Initialise(Zenith_Input& xInput, Zenith_Pointers& xPointers)
{
	m_pxInput    = &xInput;
	m_pxPointers = &xPointers;
}

void Zenith_InputActions::RegisterAction(Zenith_InputActionID uId, const char* szName,
	Zenith_EInputActionKind eKind)
{
	Zenith_Assert(uId < uMAX_ACTIONS, "Action id %u is out of range", uId);
	if (uId >= uMAX_ACTIONS)
	{
		return;
	}
	Zenith_Assert(m_bEngineRegistrationInProgress || uId >= uINPUT_ACTION_FIRST_GAME_ID,
		"Action ids 0-%u are engine-reserved; game actions start at %u",
		static_cast<u_int32>(uINPUT_ACTION_RESERVED_COUNT) - 1u,
		static_cast<u_int32>(uINPUT_ACTION_FIRST_GAME_ID));
	Zenith_Assert(!m_axActions[uId].m_bRegistered, "Action id %u is already registered as '%s'",
		uId, m_axActions[uId].m_szName);

	Action& xAction = m_axActions[uId];
	xAction = Action();
	Zenith_CopyActionName(xAction.m_szName, uMAX_NAME_LENGTH, szName);
	xAction.m_eKind       = eKind;
	xAction.m_bRegistered = true;
}

void Zenith_InputActions::RegisterBinding(Zenith_InputActionID uId, const Zenith_InputBinding& xBinding)
{
	Zenith_Assert(uId < uMAX_ACTIONS && m_axActions[uId].m_bRegistered,
		"RegisterBinding for unregistered action id %u", uId);
	if (uId >= uMAX_ACTIONS || !m_axActions[uId].m_bRegistered)
	{
		return;
	}

	Action& xAction = m_axActions[uId];
	Zenith_Assert(xAction.m_uBindingCount < uMAX_BINDINGS_PER_ACTION,
		"Action '%s' already has the maximum %u bindings", xAction.m_szName, uMAX_BINDINGS_PER_ACTION);
	if (xAction.m_uBindingCount >= uMAX_BINDINGS_PER_ACTION)
	{
		return;
	}

	Zenith_Assert(Zenith_IsBindingKindCompatible(xBinding.m_eType, xAction.m_eKind),
		"Binding type %u cannot drive action '%s' (wrong value kind)",
		static_cast<u_int32>(xBinding.m_eType), xAction.m_szName);

	// A reserved UI action must stay claim-INDEPENDENT and device-sourced: it is
	// closed BEFORE the capture walk, so a pointer- or widget-sourced row on it
	// would be read at a point where its own suppression rule cannot exist yet.
	const bool bPointerSourced = xBinding.m_eType == INPUT_BINDING_TOUCH_PRIMARY
		|| xBinding.m_eType == INPUT_BINDING_MOUSE_BUTTON
		|| xBinding.m_eType == INPUT_BINDING_MOUSE_WHEEL
		|| xBinding.m_eType == INPUT_BINDING_MOUSE_DELTA;
	const bool bReservedAction = uId < uINPUT_ACTION_RESERVED_COUNT;
	Zenith_Assert(!bReservedAction || (!bPointerSourced && xBinding.m_eType != INPUT_BINDING_VIRTUAL),
		"Reserved UI action '%s' rejects pointer-sourced and virtual bindings", xAction.m_szName);
	if (bReservedAction && (bPointerSourced || xBinding.m_eType == INPUT_BINDING_VIRTUAL))
	{
		return;
	}

	xAction.m_axBindings[xAction.m_uBindingCount] = xBinding;
	xAction.m_abBindingHeld[xAction.m_uBindingCount] = false;
	xAction.m_uBindingCount++;

	// Push the "is Back bound?" answer DOWN into the device layer. The platform
	// entry point that decides whether to CONSUME the system Back gesture sits
	// BELOW this layer and cannot ask it; Actions -> Input is the legal direction,
	// and the reference was injected at Initialise. Doing it here rather than
	// making the platform walk this class's registered actions is what keeps the
	// answer a one-word read instead of an up-edge out of the platform layer.
	if (xBinding.m_eType == INPUT_BINDING_SYSTEM_BACK && m_pxInput != nullptr)
	{
		m_pxInput->NotifySystemBackBindingRegistered();
	}
}

Zenith_InputActionID Zenith_InputActions::FindActionByName(const char* szName) const
{
	if (szName == nullptr)
	{
		return uINPUT_ACTION_INVALID;
	}
	for (u_int32 u = 0; u < uMAX_ACTIONS; u++)
	{
		if (m_axActions[u].m_bRegistered && Zenith_ActionNamesMatch(m_axActions[u].m_szName, szName))
		{
			return static_cast<Zenith_InputActionID>(u);
		}
	}
	return uINPUT_ACTION_INVALID;
}

bool Zenith_InputActions::IsActionRegistered(Zenith_InputActionID uId) const
{
	return uId < uMAX_ACTIONS && m_axActions[uId].m_bRegistered;
}

Zenith_EInputActionKind Zenith_InputActions::GetActionKind(Zenith_InputActionID uId) const
{
	Zenith_Assert(IsActionRegistered(uId), "GetActionKind on unregistered action id %u", uId);
	return uId < uMAX_ACTIONS ? m_axActions[uId].m_eKind : INPUT_ACTION_BUTTON;
}

u_int32 Zenith_InputActions::GetBindingCount(Zenith_InputActionID uId) const
{
	return uId < uMAX_ACTIONS ? m_axActions[uId].m_uBindingCount : 0;
}

const Zenith_InputBinding& Zenith_InputActions::GetBinding(Zenith_InputActionID uId, u_int32 uIndex) const
{
	Zenith_Assert(uId < uMAX_ACTIONS && uIndex < uMAX_BINDINGS_PER_ACTION, "GetBinding out of range");
	return m_axActions[uId].m_axBindings[uIndex];
}

void Zenith_InputActions::RegisterEngineReservedActions()
{
	m_bEngineRegistrationInProgress = true;

	// The five reserved UI actions. Bindings are DEVICE-ONLY by contract: the
	// canvas consumes them at 10b, before any widget has taken a claim.
	RegisterAction(INPUT_ACTION_UI_NAV_UP,    "UI_NAV_UP",    INPUT_ACTION_BUTTON);
	RegisterBinding(INPUT_ACTION_UI_NAV_UP,   Zenith_InputBinding::KeySet(ZENITH_KEY_UP));
	RegisterBinding(INPUT_ACTION_UI_NAV_UP,   Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_DPAD_UP));

	RegisterAction(INPUT_ACTION_UI_NAV_DOWN,  "UI_NAV_DOWN",  INPUT_ACTION_BUTTON);
	RegisterBinding(INPUT_ACTION_UI_NAV_DOWN, Zenith_InputBinding::KeySet(ZENITH_KEY_DOWN));
	RegisterBinding(INPUT_ACTION_UI_NAV_DOWN, Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_DPAD_DOWN));

	RegisterAction(INPUT_ACTION_UI_NAV_LEFT,  "UI_NAV_LEFT",  INPUT_ACTION_BUTTON);
	RegisterBinding(INPUT_ACTION_UI_NAV_LEFT, Zenith_InputBinding::KeySet(ZENITH_KEY_LEFT));
	RegisterBinding(INPUT_ACTION_UI_NAV_LEFT, Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_DPAD_LEFT));

	RegisterAction(INPUT_ACTION_UI_NAV_RIGHT, "UI_NAV_RIGHT", INPUT_ACTION_BUTTON);
	RegisterBinding(INPUT_ACTION_UI_NAV_RIGHT, Zenith_InputBinding::KeySet(ZENITH_KEY_RIGHT));
	RegisterBinding(INPUT_ACTION_UI_NAV_RIGHT, Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_DPAD_RIGHT));

	RegisterAction(INPUT_ACTION_UI_CONFIRM,   "UI_CONFIRM",   INPUT_ACTION_BUTTON);
	RegisterBinding(INPUT_ACTION_UI_CONFIRM,  Zenith_InputBinding::KeySet(ZENITH_KEY_ENTER, ZENITH_KEY_SPACE));
	RegisterBinding(INPUT_ACTION_UI_CONFIRM,  Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_A));

	m_bEngineRegistrationInProgress = false;
}

void Zenith_InputActions::RegisterEngineDefaultProfiles()
{
	// One profile for the platform this build runs on, so a game that never
	// registers a profile of its own still resolves every binding. The FIRST
	// game RegisterProfile call clears these wholesale.
	m_bEngineRegistrationInProgress = true;
#ifdef ZENITH_ANDROID
	RegisterProfile(uPROFILE_ENGINE_TOUCH, "EngineTouch", uINPUT_SCHEME_MASK_TOUCH);
#else
	RegisterProfile(uPROFILE_ENGINE_DESKTOP, "EngineDesktop", uINPUT_SCHEME_MASK_DESKTOP);
#endif
	m_bEngineRegistrationInProgress = false;
	m_bEngineDefaultProfiles = true;
}

// ============================================================================
// Profiles
// ============================================================================

u_int32 Zenith_InputActions::FindProfileSlot(u_int8 uProfileId) const
{
	for (u_int32 u = 0; u < m_uProfileCount; u++)
	{
		if (m_axProfiles[u].m_bRegistered && m_axProfiles[u].m_uId == uProfileId)
		{
			return u;
		}
	}
	return uMAX_PROFILES;
}

void Zenith_InputActions::RegisterProfile(u_int8 uProfileId, const char* szName, u_int8 uSchemeMask)
{
	Zenith_Assert(uProfileId != uPROFILE_AUTO, "Profile id 0xFF is reserved for AUTO");
	if (uProfileId == uPROFILE_AUTO)
	{
		return;
	}

	// The engine's defaults exist only until the game has an opinion. The first
	// game registration replaces them outright rather than competing with them
	// for a scheme.
	if (m_bEngineDefaultProfiles && !m_bEngineRegistrationInProgress)
	{
		m_uProfileCount          = 0;
		m_uActiveProfile         = uPROFILE_AUTO;
		m_bEngineDefaultProfiles = false;
		for (u_int32 u = 0; u < uMAX_PROFILES; u++)
		{
			m_axProfiles[u] = Profile();
		}
	}

	Zenith_Assert(FindProfileSlot(uProfileId) == uMAX_PROFILES, "Profile id %u registered twice", uProfileId);
	Zenith_Assert(m_uProfileCount < uMAX_PROFILES, "Too many input profiles (max %u)", uMAX_PROFILES);
	if (m_uProfileCount >= uMAX_PROFILES || FindProfileSlot(uProfileId) != uMAX_PROFILES)
	{
		return;
	}

	// A scheme resolves to exactly ONE profile, or the auto-switch has no answer.
	for (u_int32 u = 0; u < m_uProfileCount; u++)
	{
		Zenith_Assert((m_axProfiles[u].m_uSchemeMask & uSchemeMask) == 0,
			"Scheme mask 0x%02X of profile '%s' overlaps already-registered profile '%s'",
			uSchemeMask, szName != nullptr ? szName : "", m_axProfiles[u].m_szName);
	}

	Profile& xProfile = m_axProfiles[m_uProfileCount];
	xProfile = Profile();
	Zenith_CopyActionName(xProfile.m_szName, uMAX_NAME_LENGTH, szName);
	xProfile.m_uId         = uProfileId;
	xProfile.m_uSchemeMask = uSchemeMask;
	xProfile.m_bRegistered = true;
	m_uProfileCount++;

	if (FindProfileSlot(m_uActiveProfile) == uMAX_PROFILES)
	{
		ResolveBootDefaultProfile();
	}
}

bool Zenith_InputActions::IsProfileRegistered(u_int8 uProfileId) const
{
	return FindProfileSlot(uProfileId) != uMAX_PROFILES;
}

u_int8 Zenith_InputActions::GetProfileSchemeMask(u_int8 uProfileId) const
{
	const u_int32 uSlot = FindProfileSlot(uProfileId);
	return uSlot != uMAX_PROFILES ? m_axProfiles[uSlot].m_uSchemeMask : 0;
}

u_int8 Zenith_InputActions::GetActiveSchemeMask() const
{
	return GetProfileSchemeMask(m_uActiveProfile);
}

u_int8 Zenith_InputActions::FindProfileByName(const char* szName) const
{
	// An empty name is the persisted spelling of AUTO, so it must never match a
	// profile whose own name happens to be empty.
	if (szName == nullptr || szName[0] == '\0')
	{
		return uPROFILE_AUTO;
	}
	for (u_int32 u = 0; u < m_uProfileCount; u++)
	{
		if (m_axProfiles[u].m_bRegistered && Zenith_ActionNamesMatch(m_axProfiles[u].m_szName, szName))
		{
			return m_axProfiles[u].m_uId;
		}
	}
	return uPROFILE_AUTO;
}

const char* Zenith_InputActions::GetProfileName(u_int8 uProfileId) const
{
	const u_int32 uSlot = FindProfileSlot(uProfileId);
	return uSlot != uMAX_PROFILES ? m_axProfiles[uSlot].m_szName : "";
}

void Zenith_InputActions::ResolveBootDefaultProfile()
{
	// Windows boots into whichever profile owns the KEYBOARD, Android into
	// whichever owns TOUCH; anything else takes the first registered profile.
#ifdef ZENITH_ANDROID
	const Zenith_EInputScheme eBootScheme = INPUT_SCHEME_TOUCH;
#else
	const Zenith_EInputScheme eBootScheme = INPUT_SCHEME_KEYBOARD;
#endif
	const u_int8 uBootProfile = FindProfileForScheme(eBootScheme);
	if (uBootProfile != uPROFILE_AUTO)
	{
		SetActiveProfile(uBootProfile);
		return;
	}
	if (m_uProfileCount > 0)
	{
		SetActiveProfile(m_axProfiles[0].m_uId);
	}
}

u_int8 Zenith_InputActions::FindProfileForScheme(Zenith_EInputScheme eScheme) const
{
	if (eScheme >= INPUT_SCHEME_COUNT)
	{
		return uPROFILE_AUTO;
	}
	const u_int8 uMask = static_cast<u_int8>(1u << eScheme);
	for (u_int32 u = 0; u < m_uProfileCount; u++)
	{
		if (m_axProfiles[u].m_bRegistered && (m_axProfiles[u].m_uSchemeMask & uMask) != 0)
		{
			return m_axProfiles[u].m_uId;
		}
	}
	return uPROFILE_AUTO;
}

void Zenith_InputActions::SetActiveProfile(u_int8 uProfileId)
{
	if (m_uActiveProfile == uProfileId)
	{
		return;
	}
	m_uActiveProfile = uProfileId;
	RebaseUnderNewMask();
	NotifyProfileChanged();
}

void Zenith_InputActions::SetProfileOverride(u_int8 uProfileId)
{
	Zenith_Assert(IsProfileRegistered(uProfileId), "SetProfileOverride to unregistered profile %u", uProfileId);
	if (!IsProfileRegistered(uProfileId))
	{
		return;
	}
	m_bOverrideActive  = true;
	m_uOverrideProfile = uProfileId;
	SetActiveProfile(uProfileId);
}

void Zenith_InputActions::ClearOverride()
{
	if (!m_bOverrideActive)
	{
		return;
	}
	m_bOverrideActive  = false;
	m_uOverrideProfile = uPROFILE_AUTO;

	// Back to AUTO: the last-used device decides where we land, falling back to
	// the boot default when nothing has been used yet.
	const u_int8 uProfile = FindProfileForScheme(m_eLastUsedDevice);
	if (uProfile != uPROFILE_AUTO)
	{
		SetActiveProfile(uProfile);
		return;
	}
	ResolveBootDefaultProfile();
}

void Zenith_InputActions::AddProfileChangedCallback(Zenith_InputProfileChangedCallback pfnCallback, void* pxUserData)
{
	Zenith_Assert(m_uProfileCallbackCount < uMAX_CALLBACKS, "Too many profile-changed callbacks");
	if (pfnCallback == nullptr || m_uProfileCallbackCount >= uMAX_CALLBACKS)
	{
		return;
	}
	m_apfnProfileCallbacks[m_uProfileCallbackCount]   = pfnCallback;
	m_apxProfileCallbackData[m_uProfileCallbackCount] = pxUserData;
	m_uProfileCallbackCount++;
}

void Zenith_InputActions::AddLastUsedDeviceCallback(Zenith_InputLastDeviceCallback pfnCallback, void* pxUserData)
{
	Zenith_Assert(m_uDeviceCallbackCount < uMAX_CALLBACKS, "Too many last-used-device callbacks");
	if (pfnCallback == nullptr || m_uDeviceCallbackCount >= uMAX_CALLBACKS)
	{
		return;
	}
	m_apfnDeviceCallbacks[m_uDeviceCallbackCount]   = pfnCallback;
	m_apxDeviceCallbackData[m_uDeviceCallbackCount] = pxUserData;
	m_uDeviceCallbackCount++;
}

void Zenith_InputActions::NotifyProfileChanged()
{
	for (u_int32 u = 0; u < m_uProfileCallbackCount; u++)
	{
		m_apfnProfileCallbacks[u](m_apxProfileCallbackData[u], m_uActiveProfile);
	}
}

void Zenith_InputActions::NotifyLastUsedDevice()
{
	for (u_int32 u = 0; u < m_uDeviceCallbackCount; u++)
	{
		m_apfnDeviceCallbacks[u](m_apxDeviceCallbackData[u], m_eLastUsedDevice);
	}
}

// ============================================================================
// Source shadow + aggregation
// ============================================================================

void Zenith_InputActions::ApplyTransitionToShadow(const Zenith_InputEvent& xEvent)
{
	switch (xEvent.m_eType)
	{
	case INPUT_EVENT_KEY_PRESS:
	case INPUT_EVENT_MOUSE_PRESS:
		if (xEvent.m_iCode >= 0 && xEvent.m_iCode < Zenith_Input::MAX_KEY_CODES)
		{
			m_abKeyShadow[xEvent.m_iCode] = true;
		}
		break;

	case INPUT_EVENT_KEY_RELEASE:
	case INPUT_EVENT_MOUSE_RELEASE:
		if (xEvent.m_iCode >= 0 && xEvent.m_iCode < Zenith_Input::MAX_KEY_CODES)
		{
			m_abKeyShadow[xEvent.m_iCode] = false;
		}
		break;

	case INPUT_EVENT_PAD_PRESS:
	case INPUT_EVENT_PAD_RELEASE:
		if (xEvent.m_iDevice >= 0 && xEvent.m_iDevice < Zenith_Input::MAX_GAMEPADS
			&& xEvent.m_iCode >= 0 && xEvent.m_iCode < Zenith_GamepadSnapshot::iBUTTON_COUNT)
		{
			m_aabPadShadow[xEvent.m_iDevice][xEvent.m_iCode] = (xEvent.m_eType == INPUT_EVENT_PAD_PRESS);
		}
		break;

	default:
		// Wheel / touch / lifecycle / connect / SYSTEM_BACK carry no held state
		// of their own. (A yanked pad's held buttons arrive as explicit
		// PAD_RELEASE transitions BEFORE the disconnect, so nothing is stuck.)
		break;
	}
}

void Zenith_InputActions::SnapshotShadow()
{
	memcpy(m_abKeyShadowAtFrameStart, m_abKeyShadow, sizeof(m_abKeyShadow));
	memcpy(m_aabPadShadowAtFrameStart, m_aabPadShadow, sizeof(m_aabPadShadow));
}

void Zenith_InputActions::RestoreShadowToFrameStart()
{
	memcpy(m_abKeyShadow, m_abKeyShadowAtFrameStart, sizeof(m_abKeyShadow));
	memcpy(m_aabPadShadow, m_aabPadShadowAtFrameStart, sizeof(m_aabPadShadow));
}

bool Zenith_InputActions::IsKeySetHeld(const Zenith_InputBinding& xBinding, u_int32 uSet) const
{
	const u_int32 uCount = xBinding.m_auKeyCount[uSet];
	for (u_int32 u = 0; u < uCount; u++)
	{
		const int32_t iKey = xBinding.m_aaiKeys[uSet][u];
		if (iKey >= 0 && iKey < Zenith_Input::MAX_KEY_CODES && m_abKeyShadow[iKey])
		{
			return true;
		}
	}
	return false;
}

bool Zenith_InputActions::ComputeTransitionFedHeld(const Zenith_InputBinding& xBinding) const
{
	switch (xBinding.m_eType)
	{
	case INPUT_BINDING_KEY_SET:
		return IsKeySetHeld(xBinding, Zenith_InputBinding::uSET_NEGATIVE);

	case INPUT_BINDING_KEY_AXIS1D:
	case INPUT_BINDING_KEY_AXIS2D:
	{
		// An axis action is "held" while any of its direction keys is down —
		// that is what makes WasPressedThisFrame on a move action mean "the
		// movement started this frame".
		for (u_int32 u = 0; u < Zenith_InputBinding::uMAX_KEY_SETS; u++)
		{
			if (IsKeySetHeld(xBinding, u))
			{
				return true;
			}
		}
		return false;
	}

	case INPUT_BINDING_MOUSE_BUTTON:
	{
		const int32_t iButton = xBinding.m_iCode;
		if (!Zenith_IsMouseButtonCode(iButton))
		{
			return false;
		}
		// Claim suppression is a property of the ROW, not of the shadow: the
		// device state is real, it just is not ours this frame.
		return m_abKeyShadow[iButton] && !m_abMouseSuppressed[iButton];
	}

	case INPUT_BINDING_GAMEPAD_BUTTON:
		if (xBinding.m_iDevice < 0 || xBinding.m_iDevice >= Zenith_Input::MAX_GAMEPADS
			|| xBinding.m_iCode < 0 || xBinding.m_iCode >= Zenith_GamepadSnapshot::iBUTTON_COUNT)
		{
			return false;
		}
		return m_aabPadShadow[xBinding.m_iDevice][xBinding.m_iCode];

	case INPUT_BINDING_GAMEPAD_DPAD_AXIS2D:
	{
		if (xBinding.m_iDevice < 0 || xBinding.m_iDevice >= Zenith_Input::MAX_GAMEPADS)
		{
			return false;
		}
		const bool* pbPad = m_aabPadShadow[xBinding.m_iDevice];
		return pbPad[ZENITH_GAMEPAD_BUTTON_DPAD_UP] || pbPad[ZENITH_GAMEPAD_BUTTON_DPAD_DOWN]
			|| pbPad[ZENITH_GAMEPAD_BUTTON_DPAD_LEFT] || pbPad[ZENITH_GAMEPAD_BUTTON_DPAD_RIGHT];
	}

	// One-shot navigation gesture: it has no held phase at all, so the replay
	// PULSES it (rise then fall inside the same transition step).
	case INPUT_BINDING_SYSTEM_BACK:
		return m_bSystemBackPulse;

	case INPUT_BINDING_VIRTUAL:
	{
		const int32_t iVirtual = xBinding.m_iCode;
		return iVirtual >= 0 && iVirtual < static_cast<int32_t>(uMAX_VIRTUAL_SOURCES)
			&& m_abVirtualHeld[iVirtual];
	}

	default:
		return false;
	}
}

void Zenith_InputActions::RefreshTransitionFedHeld(Action& xAction)
{
	for (u_int32 u = 0; u < xAction.m_uBindingCount; u++)
	{
		const Zenith_InputBinding& xBinding = xAction.m_axBindings[u];
		if (xBinding.IsTransitionFed())
		{
			xAction.m_abBindingHeld[u] = ComputeTransitionFedHeld(xBinding);
		}
	}
}

bool Zenith_InputActions::ComputeAggregateHeld(const Action& xAction) const
{
	const u_int8 uMask = GetActiveSchemeMask();
	for (u_int32 u = 0; u < xAction.m_uBindingCount; u++)
	{
		if (!xAction.m_abBindingHeld[u])
		{
			continue;
		}
		const Zenith_InputBinding& xBinding = xAction.m_axBindings[u];
		if (xBinding.IsMaskExempt())
		{
			return true;
		}
		const Zenith_EInputScheme eScheme = xBinding.GetScheme();
		if (eScheme < INPUT_SCHEME_COUNT && (uMask & (1u << eScheme)) != 0)
		{
			return true;
		}
	}
	return false;
}

// The B4 rule in one place: rise => pressed, fall => released, and a same-frame
// tap fires BOTH because each is latched, not assigned.
void Zenith_InputActions::CommitAggregate(Action& xAction)
{
	const bool bNewHeld = ComputeAggregateHeld(xAction);
	if (bNewHeld && !xAction.m_bHeld)
	{
		xAction.m_bPressed = true;
	}
	else if (!bNewHeld && xAction.m_bHeld)
	{
		xAction.m_bReleased = true;
	}
	xAction.m_bHeld = bNewHeld;
}

// ============================================================================
// Frame lifecycle
// ============================================================================

void Zenith_InputActions::OpenActionFrame()
{
	for (u_int32 u = 0; u < uMAX_ACTIONS; u++)
	{
		Action& xAction = m_axActions[u];
		if (!xAction.m_bRegistered)
		{
			continue;
		}
		xAction.m_bPressed  = false;
		xAction.m_bReleased = false;
		xAction.m_bClosed   = false;
		// A synthesized release that no close stage has published yet survives
		// this reset — see Action::m_bSynthesizedRelease.
		if (xAction.m_bSynthesizedRelease)
		{
			xAction.m_bReleased = true;
		}
	}
	m_uVirtualTransitionCount = 0;
	m_bSystemBackPulse        = false;

	// Both close stages replay the SAME transition log, so the second one has to
	// start from the state the first one started from — otherwise a source whose
	// value a later transition changes would already read its future value.
	SnapshotShadow();
}

void Zenith_InputActions::UpdateProfile()
{
	Zenith_Assert(m_pxInput != nullptr, "Zenith_InputActions::UpdateProfile before Initialise");
	if (m_pxInput == nullptr)
	{
		return;
	}

	OpenActionFrame();

	const u_int8 uActivity = DetectActivityMask();
	const Zenith_EInputScheme eWinner = PickActivityWinner(uActivity);
	if (eWinner == INPUT_SCHEME_NONE)
	{
		return;
	}

	// The last-used device is finer than the profile: it changes even when the
	// scheme that produced it already belongs to the active profile.
	if (m_eLastUsedDevice != eWinner)
	{
		m_eLastUsedDevice = eWinner;
		NotifyLastUsedDevice();
	}

	if (m_bOverrideActive)
	{
		return;
	}

	// AUTO: activity for a scheme owned by another REGISTERED profile switches
	// to it. Activity for a scheme no profile claims is ignored outright.
	const u_int8 uProfile = FindProfileForScheme(eWinner);
	if (uProfile != uPROFILE_AUTO && uProfile != m_uActiveProfile)
	{
		SetActiveProfile(uProfile);
	}
}

bool Zenith_InputActions::IsActionInStage(const Action& xAction, Zenith_InputActionID uId, bool bReservedStage) const
{
	if (!xAction.m_bRegistered || xAction.m_bClosed)
	{
		return false;
	}
	// The reserved stage takes ids 0-15 only; the gameplay stage takes whatever
	// is left, so a frame that never ran the UI phase still closes everything.
	return !bReservedStage || uId < uINPUT_ACTION_RESERVED_COUNT;
}

void Zenith_InputActions::FinalizeReservedUI()
{
	CloseStage(true);
}

void Zenith_InputActions::FinalizeGameplay()
{
	CloseStage(false);
}

void Zenith_InputActions::CloseStage(bool bReservedStage)
{
	Zenith_Assert(m_pxInput != nullptr, "Zenith_InputActions close stage before Initialise");
	if (m_pxInput == nullptr)
	{
		return;
	}

	RestoreShadowToFrameStart();

	ApplyLevelSources(bReservedStage);
	ReplayTransitions(bReservedStage);
	ReplayVirtualTransitions(bReservedStage);
	ResolveAxisValues(bReservedStage);

	for (u_int32 u = 0; u < uMAX_ACTIONS; u++)
	{
		Action& xAction = m_axActions[u];
		if (IsActionInStage(xAction, static_cast<Zenith_InputActionID>(u), bReservedStage))
		{
			xAction.m_bClosed             = true;
			xAction.m_bSynthesizedRelease = false;
		}
	}
}

// Level-sampled rows (the pointer and the analogue-as-button), applied before
// the ordered replay: their edges are frame-level facts, not log entries.
void Zenith_InputActions::ApplyLevelSources(bool bReservedStage)
{
	const Zenith_Pointer* pxPrimary = nullptr;
	if (m_pxPointers != nullptr)
	{
		pxPrimary = m_pxPointers->Resolve(m_pxPointers->GetPrimaryPointer());
	}

	for (u_int32 uAction = 0; uAction < uMAX_ACTIONS; uAction++)
	{
		Action& xAction = m_axActions[uAction];
		if (!IsActionInStage(xAction, static_cast<Zenith_InputActionID>(uAction), bReservedStage))
		{
			continue;
		}

		for (u_int32 uBinding = 0; uBinding < xAction.m_uBindingCount; uBinding++)
		{
			const Zenith_InputBinding& xBinding = xAction.m_axBindings[uBinding];

			if (xBinding.m_eType == INPUT_BINDING_TOUCH_PRIMARY)
			{
				// A CLAIMED pointer is a consumed one: the widget that took it
				// owns the gesture, so the binding never sees it.
				const bool bUnclaimed = pxPrimary != nullptr && !pxPrimary->IsClaimed();
				const bool bDownEdge  = bUnclaimed && pxPrimary->m_bDownThisFrame;
				const bool bLevel     = bUnclaimed && pxPrimary->IsDown();

				if (bDownEdge)
				{
					// Raise first, then settle: a press and release inside one
					// frame (every quick tap) must fire BOTH edges.
					xAction.m_abBindingHeld[uBinding] = true;
					CommitAggregate(xAction);
				}
				xAction.m_abBindingHeld[uBinding] = bLevel;
				CommitAggregate(xAction);
				continue;
			}

			if (xBinding.m_eType == INPUT_BINDING_GAMEPAD_AXIS_AS_BUTTON)
			{
				const float fValue = m_pxInput->GetGamepadAxis(xBinding.m_iCode, xBinding.m_iDevice);
				// The row's own held state IS the hysteresis latch, so two rows
				// on one axis with different thresholds cannot fight.
				const bool bWasLatched = xAction.m_abBindingHeld[uBinding];
				bool bLatched = bWasLatched;
				if (!bWasLatched && fValue >= xBinding.m_fPressAt)
				{
					bLatched = true;
				}
				else if (bWasLatched && fValue <= xBinding.m_fReleaseAt)
				{
					bLatched = false;
				}
				xAction.m_abBindingHeld[uBinding] = bLatched;
				CommitAggregate(xAction);
			}
		}
	}
}

void Zenith_InputActions::ReplayTransitions(bool bReservedStage)
{
	const u_int32 uCount = m_pxInput->GetTransitionCount();
	for (u_int32 uTransition = 0; uTransition < uCount; uTransition++)
	{
		const Zenith_InputEvent& xEvent = m_pxInput->GetTransition(uTransition);

		// Claim suppression is a GAMEPLAY-stage rule: the reserved UI actions
		// close before any claim exists and carry no pointer-sourced rows.
		if (!bReservedStage)
		{
			UpdateMouseSuppression(xEvent);
		}

		const bool bSystemBack = (xEvent.m_eType == INPUT_EVENT_SYSTEM_BACK);
		if (!bSystemBack)
		{
			ApplyTransitionToShadow(xEvent);
		}

		m_bSystemBackPulse = bSystemBack;
		for (u_int32 uAction = 0; uAction < uMAX_ACTIONS; uAction++)
		{
			Action& xAction = m_axActions[uAction];
			if (!IsActionInStage(xAction, static_cast<Zenith_InputActionID>(uAction), bReservedStage))
			{
				continue;
			}
			RefreshTransitionFedHeld(xAction);
			CommitAggregate(xAction);
		}

		if (bSystemBack)
		{
			// ...and fall again in the same step, so the gesture is a pulse
			// rather than a permanently held action.
			m_bSystemBackPulse = false;
			for (u_int32 uAction = 0; uAction < uMAX_ACTIONS; uAction++)
			{
				Action& xAction = m_axActions[uAction];
				if (!IsActionInStage(xAction, static_cast<Zenith_InputActionID>(uAction), bReservedStage))
				{
					continue;
				}
				RefreshTransitionFedHeld(xAction);
				CommitAggregate(xAction);
			}
		}
		m_bSystemBackPulse = false;

		if (!bReservedStage && xEvent.m_eType == INPUT_EVENT_MOUSE_RELEASE
			&& Zenith_IsMouseButtonCode(xEvent.m_iCode))
		{
			// Cleared only AFTER the release was replayed, so a suppressed press
			// suppresses its matching release too.
			m_abMouseSuppressed[xEvent.m_iCode] = false;
		}
	}
}

void Zenith_InputActions::ReplayVirtualTransitions(bool bReservedStage)
{
	// Virtual sources are published between the capture walk and 10e, and
	// reserved actions reject virtual rows outright — so the gameplay stage is
	// the only one that replays them, and it advances the shadow exactly once.
	if (bReservedStage)
	{
		return;
	}

	for (u_int32 u = 0; u < m_uVirtualTransitionCount; u++)
	{
		const VirtualTransition& xTransition = m_axVirtualTransitions[u];
		if (xTransition.m_uVirtualId >= uMAX_VIRTUAL_SOURCES)
		{
			continue;
		}
		m_abVirtualHeld[xTransition.m_uVirtualId] = xTransition.m_bDown;

		for (u_int32 uAction = 0; uAction < uMAX_ACTIONS; uAction++)
		{
			Action& xAction = m_axActions[uAction];
			if (!IsActionInStage(xAction, static_cast<Zenith_InputActionID>(uAction), bReservedStage))
			{
				continue;
			}
			RefreshTransitionFedHeld(xAction);
			CommitAggregate(xAction);
		}
	}
}

// Axis values are LAST-STATE, not replayed (B4). Where several enabled rows can
// answer, the one with the largest magnitude wins — so a resting stick never
// cancels the keyboard row beside it.
void Zenith_InputActions::ResolveAxisValues(bool bReservedStage)
{
	const u_int8 uMask = GetActiveSchemeMask();

	for (u_int32 uAction = 0; uAction < uMAX_ACTIONS; uAction++)
	{
		Action& xAction = m_axActions[uAction];
		if (!IsActionInStage(xAction, static_cast<Zenith_InputActionID>(uAction), bReservedStage))
		{
			continue;
		}

		float fBestX = 0.0f;
		float fBestY = 0.0f;
		float fBestMagnitude = -1.0f;

		for (u_int32 uBinding = 0; uBinding < xAction.m_uBindingCount; uBinding++)
		{
			const Zenith_InputBinding& xBinding = xAction.m_axBindings[uBinding];
			const Zenith_EInputScheme eScheme = xBinding.GetScheme();
			if (!xBinding.IsMaskExempt()
				&& (eScheme >= INPUT_SCHEME_COUNT || (uMask & (1u << eScheme)) == 0))
			{
				continue;
			}

			float fX = 0.0f;
			float fY = 0.0f;

			switch (xBinding.m_eType)
			{
			case INPUT_BINDING_KEY_AXIS1D:
				fX = (IsKeySetHeld(xBinding, Zenith_InputBinding::uSET_POSITIVE) ? 1.0f : 0.0f)
					- (IsKeySetHeld(xBinding, Zenith_InputBinding::uSET_NEGATIVE) ? 1.0f : 0.0f);
				break;

			case INPUT_BINDING_KEY_AXIS2D:
			{
				const Zenith_Maths::Vector2 xMove = ResolveMoveComposite(
					IsKeySetHeld(xBinding, Zenith_InputBinding::uSET_FORWARD),
					IsKeySetHeld(xBinding, Zenith_InputBinding::uSET_BACK),
					IsKeySetHeld(xBinding, Zenith_InputBinding::uSET_LEFT),
					IsKeySetHeld(xBinding, Zenith_InputBinding::uSET_RIGHT));
				fX = xMove.x;
				fY = xMove.y;
				break;
			}

			case INPUT_BINDING_MOUSE_WHEEL:
				fX = m_pxInput->GetMouseWheelDelta() * xBinding.m_fScaleX;
				break;

			case INPUT_BINDING_MOUSE_DELTA:
			{
				Zenith_Maths::Vector2_64 xDelta;
				m_pxInput->GetMouseDelta(xDelta);
				fX = static_cast<float>(xDelta.x) * xBinding.m_fScaleX;
				fY = static_cast<float>(xDelta.y) * xBinding.m_fScaleY;
				break;
			}

			case INPUT_BINDING_GAMEPAD_STICK:
				fX = m_pxInput->GetGamepadAxis(xBinding.m_iCode, xBinding.m_iDevice) * xBinding.m_fScaleX;
				fY = m_pxInput->GetGamepadAxis(xBinding.m_iCode + 1, xBinding.m_iDevice) * xBinding.m_fScaleY;
				break;

			case INPUT_BINDING_GAMEPAD_AXIS:
				fX = m_pxInput->GetGamepadAxis(xBinding.m_iCode, xBinding.m_iDevice) * xBinding.m_fScaleX;
				break;

			case INPUT_BINDING_GAMEPAD_TRIGGER_PAIR:
				fX = (m_pxInput->GetGamepadRightTrigger(xBinding.m_iDevice)
					- m_pxInput->GetGamepadLeftTrigger(xBinding.m_iDevice)) * xBinding.m_fScaleX;
				break;

			case INPUT_BINDING_GAMEPAD_DPAD_AXIS2D:
			{
				if (xBinding.m_iDevice < 0 || xBinding.m_iDevice >= Zenith_Input::MAX_GAMEPADS)
				{
					break;
				}
				const bool* pbPad = m_aabPadShadow[xBinding.m_iDevice];
				const Zenith_Maths::Vector2 xMove = ResolveMoveComposite(
					pbPad[ZENITH_GAMEPAD_BUTTON_DPAD_UP], pbPad[ZENITH_GAMEPAD_BUTTON_DPAD_DOWN],
					pbPad[ZENITH_GAMEPAD_BUTTON_DPAD_LEFT], pbPad[ZENITH_GAMEPAD_BUTTON_DPAD_RIGHT]);
				fX = xMove.x;
				fY = xMove.y;
				break;
			}

			case INPUT_BINDING_VIRTUAL:
				if (xAction.m_eKind == INPUT_ACTION_BUTTON)
				{
					fX = xAction.m_abBindingHeld[uBinding] ? 1.0f : 0.0f;
				}
				else if (xBinding.m_iCode >= 0 && xBinding.m_iCode < static_cast<int32_t>(uMAX_VIRTUAL_SOURCES))
				{
					fX = m_afVirtualAxisX[xBinding.m_iCode] * xBinding.m_fScaleX;
					fY = m_afVirtualAxisY[xBinding.m_iCode] * xBinding.m_fScaleY;
				}
				break;

			default:
				// Every button row: its axis reading is its held state, so a
				// button bound into an axis slot still reads 0/1.
				fX = xAction.m_abBindingHeld[uBinding] ? 1.0f : 0.0f;
				break;
			}

			const float fMagnitude = fX * fX + fY * fY;
			if (fMagnitude > fBestMagnitude)
			{
				fBestMagnitude = fMagnitude;
				fBestX = fX;
				fBestY = fY;
			}
		}

		xAction.m_fAxisX = fBestX;
		xAction.m_fAxisY = fBestY;
	}
}

// ============================================================================
// Claim suppression (10e)
// ============================================================================

bool Zenith_InputActions::IsPrimaryPointerClaimed() const
{
	if (m_pxPointers == nullptr)
	{
		return false;
	}
	const Zenith_PointerHandle xHandle = m_pxPointers->GetPrimaryPointer();
	return xHandle.IsValid() && m_pxPointers->IsClaimed(xHandle);
}

void Zenith_InputActions::UpdateMouseSuppression(const Zenith_InputEvent& xEvent)
{
	// B3 makes the mouse view and the primary pointer the same thing, so a
	// MOUSE_PRESS is pointer-sourced whether it came from a real mouse or from
	// a projected first finger. A press taken while a widget owns that pointer
	// is not ours, and neither is the release that ends it.
	if (xEvent.m_eType != INPUT_EVENT_MOUSE_PRESS || !Zenith_IsMouseButtonCode(xEvent.m_iCode))
	{
		return;
	}
	m_abMouseSuppressed[xEvent.m_iCode] = IsPrimaryPointerClaimed();
}

// ============================================================================
// Activity detection (B5)
// ============================================================================

u_int8 Zenith_InputActions::DetectActivityMask()
{
	u_int8 uActivity = 0;

	// EDGES only, never held state — a key that stays down is not a device the
	// player just reached for.
	const u_int32 uTransitionCount = m_pxInput->GetTransitionCount();
	for (u_int32 u = 0; u < uTransitionCount; u++)
	{
		const Zenith_InputEvent& xEvent = m_pxInput->GetTransition(u);
		if (xEvent.m_bSystemNav)
		{
			// A navigation gesture is the SYSTEM talking, not the player
			// picking up a device.
			continue;
		}
		switch (xEvent.m_eType)
		{
		case INPUT_EVENT_KEY_PRESS:   uActivity |= uINPUT_SCHEME_MASK_KEYBOARD; break;
		case INPUT_EVENT_MOUSE_PRESS: uActivity |= uINPUT_SCHEME_MASK_MOUSE;    break;
		case INPUT_EVENT_PAD_PRESS:   uActivity |= uINPUT_SCHEME_MASK_GAMEPAD;  break;
		default: break;
		}
	}

	// A finger arriving. The staged stream (not the transition log) is where
	// touch lives; on a touch device the B3 projection ALSO raises a mouse
	// press, which the TOUCH-first priority below then outranks.
	const u_int32 uStagedCount = m_pxInput->GetStagedTouchCount();
	for (u_int32 u = 0; u < uStagedCount; u++)
	{
		if (m_pxInput->GetStagedTouch(u).m_eType == INPUT_EVENT_TOUCH_DOWN)
		{
			uActivity |= uINPUT_SCHEME_MASK_TOUCH;
			break;
		}
	}

	if (m_pxInput->GetMouseWheelDelta() != 0.0f)
	{
		uActivity |= uINPUT_SCHEME_MASK_MOUSE;
	}

#ifdef ZENITH_WINDOWS
	{
		// Cumulative movement CROSSING the threshold, with the same re-arm rule
		// the sticks use: a continuous drag notifies once, not every frame.
		Zenith_Maths::Vector2_64 xDelta;
		m_pxInput->GetMouseDelta(xDelta);
		const double fMovement = xDelta.x * xDelta.x + xDelta.y * xDelta.y;
		const bool bMoved = fMovement >= static_cast<double>(fMOUSE_ACTIVITY_PIXELS * fMOUSE_ACTIVITY_PIXELS);
		if (bMoved && !m_bMouseMoveActivityLatched)
		{
			uActivity |= uINPUT_SCHEME_MASK_MOUSE;
		}
		m_bMouseMoveActivityLatched = bMoved;
	}
#endif

	for (int iPad = 0; iPad < Zenith_Input::MAX_GAMEPADS; iPad++)
	{
		if (!m_pxInput->IsGamepadConnected(iPad))
		{
			continue;
		}
		for (int iAxis = 0; iAxis < Zenith_GamepadSnapshot::iAXIS_COUNT; iAxis++)
		{
			// GetGamepadAxis already applies the RADIAL stick deadzone, so a
			// stick reads exactly 0 until it leaves it; a trigger is raw [0,1]
			// and gets the same deadzone as its crossing threshold.
			const float fValue = m_pxInput->GetGamepadAxis(iAxis, iPad);
			const float fMagnitude = fValue < 0.0f ? -fValue : fValue;
			const bool bPastThreshold = (iAxis > ZENITH_GAMEPAD_AXIS_RIGHT_Y)
				? fMagnitude > fPAD_ACTIVITY_DEADZONE
				: fMagnitude > 0.0f;

			// The CROSSING is the event, not the level: an axis parked past the
			// deadzone must fall back inside before it can count again.
			bool& bLatched = m_aabPadAxisActivityLatched[iPad][iAxis];
			if (bPastThreshold && !bLatched)
			{
				uActivity |= uINPUT_SCHEME_MASK_GAMEPAD;
			}
			bLatched = bPastThreshold;
		}
	}

	return uActivity;
}

Zenith_EInputScheme Zenith_InputActions::PickActivityWinner(u_int8 uActivityMask) const
{
	// Same-frame priority: TOUCH > GAMEPAD > MOUSE > KEYBOARD. Touch outranks
	// the mouse because on a touch device the mouse press IS the finger.
	if ((uActivityMask & uINPUT_SCHEME_MASK_TOUCH) != 0)    return INPUT_SCHEME_TOUCH;
	if ((uActivityMask & uINPUT_SCHEME_MASK_GAMEPAD) != 0)  return INPUT_SCHEME_GAMEPAD;
	if ((uActivityMask & uINPUT_SCHEME_MASK_MOUSE) != 0)    return INPUT_SCHEME_MOUSE;
	if ((uActivityMask & uINPUT_SCHEME_MASK_KEYBOARD) != 0) return INPUT_SCHEME_KEYBOARD;
	return INPUT_SCHEME_NONE;
}

// ============================================================================
// Mask-change synthesis (B4)
// ============================================================================

void Zenith_InputActions::RebaseUnderNewMask()
{
	// Rebase every action from the CURRENT source states. A held action whose
	// bindings just left the mask synthesizes a RELEASE; a newly enabled source
	// that is already down makes the action held WITHOUT a pressed edge, because
	// an edge requires a genuine transition and held tracks reality.
	for (u_int32 u = 0; u < uMAX_ACTIONS; u++)
	{
		Action& xAction = m_axActions[u];
		if (!xAction.m_bRegistered)
		{
			continue;
		}

		RefreshTransitionFedHeld(xAction);
		const bool bNewHeld = ComputeAggregateHeld(xAction);
		if (xAction.m_bHeld && !bNewHeld)
		{
			xAction.m_bReleased         = true;
			xAction.m_bSynthesizedRelease = true;
		}
		xAction.m_bHeld = bNewHeld;
	}
}

// ============================================================================
// Virtual sources (the WP3a publish API bottoms out here)
// ============================================================================

void Zenith_InputActions::PublishVirtualButton(u_int16 uVirtualId, bool bDown)
{
	if (uVirtualId >= uMAX_VIRTUAL_SOURCES)
	{
		return;
	}

	// The published level is the last transition queued for this source this
	// frame, or the replay shadow when there is none. Publishing the same level
	// twice is a no-op, so a widget may publish every frame it is held.
	bool bCurrent = m_abVirtualHeld[uVirtualId];
	for (u_int32 u = m_uVirtualTransitionCount; u > 0; u--)
	{
		if (m_axVirtualTransitions[u - 1].m_uVirtualId == uVirtualId)
		{
			bCurrent = m_axVirtualTransitions[u - 1].m_bDown;
			break;
		}
	}
	if (bCurrent == bDown)
	{
		return;
	}

	if (m_uVirtualTransitionCount >= uMAX_VIRTUAL_TRANSITIONS)
	{
		Zenith_Warning(LOG_CATEGORY_INPUT, "Virtual transition list full (%u); dropping publish",
			uMAX_VIRTUAL_TRANSITIONS);
		return;
	}
	m_axVirtualTransitions[m_uVirtualTransitionCount].m_uVirtualId = uVirtualId;
	m_axVirtualTransitions[m_uVirtualTransitionCount].m_bDown      = bDown;
	m_uVirtualTransitionCount++;
}

void Zenith_InputActions::PublishVirtualAxis(u_int16 uVirtualId, float fX, float fY)
{
	if (uVirtualId >= uMAX_VIRTUAL_SOURCES)
	{
		return;
	}
	m_afVirtualAxisX[uVirtualId] = fX;
	m_afVirtualAxisY[uVirtualId] = fY;
}

// ============================================================================
// Queries
// ============================================================================

bool Zenith_InputActions::IsHeld(Zenith_InputActionID uId) const
{
	return uId < uMAX_ACTIONS && m_axActions[uId].m_bRegistered && m_axActions[uId].m_bHeld;
}

bool Zenith_InputActions::WasPressedThisFrame(Zenith_InputActionID uId) const
{
	return uId < uMAX_ACTIONS && m_axActions[uId].m_bRegistered && m_axActions[uId].m_bPressed;
}

bool Zenith_InputActions::WasReleasedThisFrame(Zenith_InputActionID uId) const
{
	return uId < uMAX_ACTIONS && m_axActions[uId].m_bRegistered && m_axActions[uId].m_bReleased;
}

float Zenith_InputActions::GetAxis1D(Zenith_InputActionID uId) const
{
	return uId < uMAX_ACTIONS && m_axActions[uId].m_bRegistered ? m_axActions[uId].m_fAxisX : 0.0f;
}

void Zenith_InputActions::GetAxis2D(Zenith_InputActionID uId, Zenith_Maths::Vector2& xOut) const
{
	if (uId >= uMAX_ACTIONS || !m_axActions[uId].m_bRegistered)
	{
		xOut = Zenith_Maths::Vector2(0.0f, 0.0f);
		return;
	}
	xOut = Zenith_Maths::Vector2(m_axActions[uId].m_fAxisX, m_axActions[uId].m_fAxisY);
}

Zenith_Maths::Vector2 Zenith_InputActions::ResolveMoveComposite(bool bForward, bool bBack,
	bool bLeft, bool bRight)
{
	// +y is forward, diagonals are UNNORMALISED, opposite keys CANCEL.
	Zenith_Maths::Vector2 xMove(0.0f, 0.0f);
	if (bForward) xMove.y += 1.0f;
	if (bBack)    xMove.y -= 1.0f;
	if (bLeft)    xMove.x -= 1.0f;
	if (bRight)   xMove.x += 1.0f;
	return xMove;
}

// ============================================================================
// Test reset
// ============================================================================

void Zenith_InputActions::ResetTransientForTest()
{
	for (u_int32 u = 0; u < uMAX_ACTIONS; u++)
	{
		Action& xAction = m_axActions[u];
		if (!xAction.m_bRegistered)
		{
			continue;
		}
		// Registrations survive: the name, kind and binding rows are what a game
		// installed at boot and no test boundary may take them away.
		for (u_int32 uBinding = 0; uBinding < uMAX_BINDINGS_PER_ACTION; uBinding++)
		{
			xAction.m_abBindingHeld[uBinding] = false;
		}
		xAction.m_bHeld     = false;
		xAction.m_bPressed  = false;
		xAction.m_bReleased = false;
		xAction.m_bClosed   = false;
		xAction.m_bSynthesizedRelease = false;
		xAction.m_fAxisX    = 0.0f;
		xAction.m_fAxisY    = 0.0f;
	}

	memset(m_abKeyShadow, 0, sizeof(m_abKeyShadow));
	memset(m_aabPadShadow, 0, sizeof(m_aabPadShadow));
	memset(m_abKeyShadowAtFrameStart, 0, sizeof(m_abKeyShadowAtFrameStart));
	memset(m_aabPadShadowAtFrameStart, 0, sizeof(m_aabPadShadowAtFrameStart));
	memset(m_abMouseSuppressed, 0, sizeof(m_abMouseSuppressed));
	memset(m_abVirtualHeld, 0, sizeof(m_abVirtualHeld));
	memset(m_afVirtualAxisX, 0, sizeof(m_afVirtualAxisX));
	memset(m_afVirtualAxisY, 0, sizeof(m_afVirtualAxisY));
	m_uVirtualTransitionCount = 0;
	m_bSystemBackPulse        = false;

	memset(m_aabPadAxisActivityLatched, 0, sizeof(m_aabPadAxisActivityLatched));
	m_bMouseMoveActivityLatched = false;

	// Back to AUTO, on the boot-default profile. Profile REGISTRATIONS stay.
	m_bOverrideActive  = false;
	m_uOverrideProfile = uPROFILE_AUTO;
	m_eLastUsedDevice  = INPUT_SCHEME_NONE;
	m_uActiveProfile   = uPROFILE_AUTO;
	ResolveBootDefaultProfile();
}

#include "Input/Zenith_InputActions.Tests.inl"
