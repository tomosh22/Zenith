#include "Zenith.h"

#include "Input/Zenith_Pointers.h"

// Monotonic seconds, matching the stamp the device FIFO puts on every event.
// The mouse projection synthesizes its own transitions (there is no FIFO entry
// behind a projected press), so it needs the same clock to make down-times
// comparable with a real finger's.
static double Zenith_GetPointerTimestampSeconds()
{
	const auto xNow = std::chrono::high_resolution_clock::now().time_since_epoch();
	return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(xNow).count()) / 1.0e9;
}

static float Zenith_PointerDistance(const Zenith_Maths::Vector2& xA, const Zenith_Maths::Vector2& xB)
{
	const float fDX = xA.x - xB.x;
	const float fDY = xA.y - xB.y;
	return std::sqrt(fDX * fDX + fDY * fDY);
}

// ============================================================================
// Frame lifecycle
// ============================================================================

void Zenith_Pointers::BeginFrame(float fDisplayScale)
{
	m_fDisplayScale = fDisplayScale > 0.0f ? fDisplayScale : 1.0f;

	for (u_int32 u = 0; u < uMAX_POINTERS; u++)
	{
		Zenith_Pointer& xPointer = m_axPointers[u];

		// Retire whatever ended LAST frame. Deferring the free by a frame is what
		// lets a claimer see the terminal edge and release against a live handle;
		// the generation bump on free is what makes the handle it kept stale.
		if (xPointer.m_bPendingRetire)
		{
			xPointer.m_bActive       = false;
			xPointer.m_bPendingRetire = false;
			xPointer.m_bFromTouch    = false;
			xPointer.m_iPlatformId   = -1;
			xPointer.m_ulClaimOwner  = ulPOINTER_OWNER_NONE;
			xPointer.m_uGeneration++;
		}

		xPointer.m_bDownThisFrame      = false;
		xPointer.m_bUpThisFrame        = false;
		xPointer.m_bCancelledThisFrame = false;
		xPointer.m_bTapThisFrame       = false;
	}

	m_uStagedConsumed        = 0;
	m_bMousePressConsumed    = false;
	m_bMouseReleaseConsumed  = false;
	m_bSawTouchEventThisFrame = false;
}

void Zenith_Pointers::ApplyPlatform(Zenith_Input& xInput, float fDisplayScale)
{
	BeginFrame(fDisplayScale);
	ConsumeStagedTail(xInput);
	ApplyMouseFromInput(xInput);
	m_bArmed = xInput.IsInputArmed();
}

void Zenith_Pointers::ApplyInjection(Zenith_Input& xInput)
{
	// NO BeginFrame: the automated-test Steps ran after step 4, so this frame's
	// edges and slots must survive. Only the tail the Steps appended is new.
	ConsumeStagedTail(xInput);
	ApplyMouseFromInput(xInput);
	m_bArmed = xInput.IsInputArmed();
}

void Zenith_Pointers::ConsumeStagedTail(Zenith_Input& xInput)
{
	const u_int32 uCount = xInput.GetStagedTouchCount();
	// A drain that reset the staged stream (a new frame, or a barrier that voided
	// everything before it) leaves fewer entries than we have consumed. Restart
	// from the beginning rather than skipping the survivors.
	if (uCount < m_uStagedConsumed)
	{
		m_uStagedConsumed = 0;
	}
	for (u_int32 u = m_uStagedConsumed; u < uCount; u++)
	{
		ApplyEvent(xInput.GetStagedTouch(u));
	}
	m_uStagedConsumed = uCount;
}

void Zenith_Pointers::ApplyMouseFromInput(Zenith_Input& xInput)
{
	Zenith_Maths::Vector2_64 xPosition;
	xInput.GetMousePosition(xPosition);
	ApplyMouseState(
		xInput.WasKeyPressedThisFrame(ZENITH_MOUSE_BUTTON_LEFT),
		xInput.WasKeyReleasedThisFrame(ZENITH_MOUSE_BUTTON_LEFT),
		Zenith_Maths::Vector2(static_cast<float>(xPosition.x), static_cast<float>(xPosition.y)),
		Zenith_GetPointerTimestampSeconds());
}

// ============================================================================
// Event application
// ============================================================================

void Zenith_Pointers::ApplyEvent(const Zenith_InputEvent& xEvent)
{
	if (xEvent.m_eType == INPUT_EVENT_LIFECYCLE_RESET)
	{
		// B2: a barrier voids every staged and live pointer, raising the CANCEL
		// edges, and disarms until the device layer re-arms.
		CancelAll();
		m_bArmed = false;
		return;
	}

	if (!m_bArmed)
	{
		return;
	}

	switch (xEvent.m_eType)
	{
	case INPUT_EVENT_TOUCH_DOWN:
	case INPUT_EVENT_TOUCH_MOVE:
	case INPUT_EVENT_TOUCH_UP:
	case INPUT_EVENT_TOUCH_CANCEL:
		break;
	default:
		return;
	}

	m_bSawTouchEventThisFrame = true;

	const int32_t iPointerId = xEvent.m_iCode;
	u_int32 uSlot = FindSlotByPlatformId(iPointerId);

	if (xEvent.m_eType == INPUT_EVENT_TOUCH_DOWN)
	{
		// A DOWN for a pointer that is somehow already live means the UP was lost.
		// Cancel the stale gesture rather than silently continuing it, so nothing
		// downstream sees a finger that jumped.
		if (uSlot != Zenith_PointerHandle::uINVALID_SLOT)
		{
			EndPointer(m_axPointers[uSlot], true);
		}
		AllocateSlot(iPointerId, true, xEvent.m_fX, xEvent.m_fY, xEvent.m_fTimestamp);
		return;
	}

	if (uSlot == Zenith_PointerHandle::uINVALID_SLOT)
	{
		// MOVE/UP for a pointer we never saw go down (dropped under FIFO pressure,
		// or arriving while disarmed). Nothing to update.
		return;
	}

	Zenith_Pointer& xPointer = m_axPointers[uSlot];
	UpdateSlotPosition(xPointer, xEvent.m_fX, xEvent.m_fY, xEvent.m_fExcursion);

	if (xEvent.m_eType == INPUT_EVENT_TOUCH_MOVE)
	{
		return;
	}

	const bool bCancelled = (xEvent.m_eType == INPUT_EVENT_TOUCH_CANCEL);
	if (!bCancelled)
	{
		// The gesture's duration is measured against the event stamps, so a
		// coalesced MOVE storm cannot inflate it.
		const double fDuration = xEvent.m_fTimestamp - xPointer.m_fDownTime;
		xPointer.m_bTapThisFrame = !xPointer.IsClaimed()
			&& fDuration <= static_cast<double>(fTAP_MAX_DURATION_SECONDS)
			&& xPointer.m_fMaxExcursion <= fTAP_MAX_EXCURSION_PX * m_fDisplayScale;
	}
	EndPointer(xPointer, bCancelled);
}

void Zenith_Pointers::ApplyMouseState(bool bPressedEdge, bool bReleasedEdge,
	const Zenith_Maths::Vector2& xPosition, double fTimestamp)
{
	if (!m_bArmed)
	{
		return;
	}

	// B3, the touch direction: on a touch device the mouse view is the projected
	// FIRST finger, so the touch stream already owns the primary slot. Running the
	// projection as well would open a second pointer for the same finger.
	if (m_bSawTouchEventThisFrame || IsTouchOwningPointer())
	{
		return;
	}

	if (bPressedEdge && !m_bMousePressConsumed)
	{
		m_bMousePressConsumed = true;
		if (!m_bMouseDown)
		{
			m_bMouseDown = true;
			AllocateSlot(iMOUSE_PLATFORM_ID, false, xPosition.x, xPosition.y, fTimestamp);
		}
	}

	const u_int32 uSlot = FindSlotByPlatformId(iMOUSE_PLATFORM_ID);
	if (m_bMouseDown && uSlot != Zenith_PointerHandle::uINVALID_SLOT)
	{
		Zenith_Pointer& xPointer = m_axPointers[uSlot];
		if (!xPointer.m_bUpThisFrame && !xPointer.m_bCancelledThisFrame)
		{
			UpdateSlotPosition(xPointer, xPosition.x, xPosition.y, 0.0f);
		}

		if (bReleasedEdge && !m_bMouseReleaseConsumed)
		{
			m_bMouseReleaseConsumed = true;
			m_bMouseDown = false;
			const double fDuration = fTimestamp - xPointer.m_fDownTime;
			xPointer.m_bTapThisFrame = !xPointer.IsClaimed()
				&& fDuration <= static_cast<double>(fTAP_MAX_DURATION_SECONDS)
				&& xPointer.m_fMaxExcursion <= fTAP_MAX_EXCURSION_PX * m_fDisplayScale;
			EndPointer(xPointer, false);
		}
	}
	else if (bReleasedEdge && !m_bMouseReleaseConsumed)
	{
		// The release arrived with no live projected pointer (press eaten by a
		// barrier, or the button was already down when input re-armed). Consume the
		// edge so the second apply phase does not act on it either.
		m_bMouseReleaseConsumed = true;
		m_bMouseDown = false;
	}
}

void Zenith_Pointers::CancelAll()
{
	for (u_int32 u = 0; u < uMAX_POINTERS; u++)
	{
		Zenith_Pointer& xPointer = m_axPointers[u];
		if (!xPointer.m_bActive || xPointer.m_bPendingRetire)
		{
			continue;
		}
		EndPointer(xPointer, true);
	}
	m_bMouseDown = false;
}

void Zenith_Pointers::ResetTransientForTest()
{
	for (u_int32 u = 0; u < uMAX_POINTERS; u++)
	{
		m_axPointers[u] = Zenith_Pointer();
	}
	m_uStagedConsumed         = 0;
	m_bMouseDown              = false;
	m_bMousePressConsumed     = false;
	m_bMouseReleaseConsumed   = false;
	m_bSawTouchEventThisFrame = false;
	m_bArmed                  = true;
	m_fDisplayScale           = 1.0f;
}

// ============================================================================
// Slot management
// ============================================================================

u_int32 Zenith_Pointers::FindSlotByPlatformId(int32_t iPlatformId) const
{
	for (u_int32 u = 0; u < uMAX_POINTERS; u++)
	{
		const Zenith_Pointer& xPointer = m_axPointers[u];
		// A slot whose gesture ended this frame is NOT a match: its id belongs to a
		// finished pointer, and a fresh DOWN for the same id is a new identity.
		if (xPointer.m_bActive && !xPointer.m_bPendingRetire && xPointer.m_iPlatformId == iPlatformId)
		{
			return u;
		}
	}
	return Zenith_PointerHandle::uINVALID_SLOT;
}

u_int32 Zenith_Pointers::AllocateSlot(int32_t iPlatformId, bool bFromTouch, float fX, float fY, double fTimestamp)
{
	for (u_int32 u = 0; u < uMAX_POINTERS; u++)
	{
		Zenith_Pointer& xPointer = m_axPointers[u];
		if (xPointer.m_bActive)
		{
			continue;
		}

		const u_int32 uGeneration = xPointer.m_uGeneration;
		xPointer = Zenith_Pointer();
		xPointer.m_uGeneration    = uGeneration;
		xPointer.m_iPlatformId    = iPlatformId;
		xPointer.m_bActive        = true;
		xPointer.m_bFromTouch     = bFromTouch;
		xPointer.m_bDownThisFrame = true;
		xPointer.m_xPosition      = { fX, fY };
		xPointer.m_xStartPosition = { fX, fY };
		xPointer.m_fDownTime      = fTimestamp;
		return u;
	}

	Zenith_Warning(LOG_CATEGORY_INPUT, "Pointer table full (%u); dropping pointer %d", uMAX_POINTERS, iPlatformId);
	return Zenith_PointerHandle::uINVALID_SLOT;
}

void Zenith_Pointers::UpdateSlotPosition(Zenith_Pointer& xPointer, float fX, float fY, float fEventExcursion)
{
	xPointer.m_xPosition = { fX, fY };

	const float fFromStart = Zenith_PointerDistance(xPointer.m_xPosition, xPointer.m_xStartPosition);
	if (fFromStart > xPointer.m_fMaxExcursion)
	{
		xPointer.m_fMaxExcursion = fFromStart;
	}
	// The FIFO's own running max, carried on a coalesced MOVE. Without it a finger
	// that wandered far and returned inside one frame reads as never having moved,
	// because only the final position survived the coalesce.
	if (fEventExcursion > xPointer.m_fMaxExcursion)
	{
		xPointer.m_fMaxExcursion = fEventExcursion;
	}
}

void Zenith_Pointers::EndPointer(Zenith_Pointer& xPointer, bool bCancelled)
{
	if (bCancelled)
	{
		xPointer.m_bCancelledThisFrame = true;
		xPointer.m_bTapThisFrame       = false;
	}
	else
	{
		xPointer.m_bUpThisFrame = true;
	}
	// The claim deliberately SURVIVES this frame — the widget holding it has not
	// run yet and must see the terminal edge against a live handle. BeginFrame
	// frees the slot (and the claim with it) next frame.
	xPointer.m_bPendingRetire = true;
}

bool Zenith_Pointers::IsTouchOwningPointer() const
{
	for (u_int32 u = 0; u < uMAX_POINTERS; u++)
	{
		if (m_axPointers[u].m_bActive && m_axPointers[u].m_bFromTouch)
		{
			return true;
		}
	}
	return false;
}

// ============================================================================
// Queries
// ============================================================================

Zenith_PointerHandle Zenith_Pointers::GetHandle(u_int32 uSlot) const
{
	Zenith_PointerHandle xHandle;
	if (uSlot >= uMAX_POINTERS || !m_axPointers[uSlot].m_bActive)
	{
		return xHandle;
	}
	xHandle.m_uSlot       = uSlot;
	xHandle.m_uGeneration = m_axPointers[uSlot].m_uGeneration;
	return xHandle;
}

const Zenith_Pointer* Zenith_Pointers::Resolve(const Zenith_PointerHandle& xHandle) const
{
	if (!xHandle.IsValid() || xHandle.m_uSlot >= uMAX_POINTERS)
	{
		return nullptr;
	}
	const Zenith_Pointer& xPointer = m_axPointers[xHandle.m_uSlot];
	if (!xPointer.m_bActive || xPointer.m_uGeneration != xHandle.m_uGeneration)
	{
		return nullptr;
	}
	return &xPointer;
}

Zenith_PointerHandle Zenith_Pointers::FindByPlatformId(int32_t iPlatformId) const
{
	return GetHandle(FindSlotByPlatformId(iPlatformId));
}

Zenith_PointerHandle Zenith_Pointers::GetPrimaryPointer() const
{
	for (u_int32 u = 0; u < uMAX_POINTERS; u++)
	{
		if (m_axPointers[u].m_bActive)
		{
			return GetHandle(u);
		}
	}
	return Zenith_PointerHandle();
}

u_int32 Zenith_Pointers::GetActivePointerCount() const
{
	u_int32 uCount = 0;
	for (u_int32 u = 0; u < uMAX_POINTERS; u++)
	{
		if (m_axPointers[u].m_bActive && !m_axPointers[u].m_bPendingRetire)
		{
			uCount++;
		}
	}
	return uCount;
}

bool Zenith_Pointers::WasTapThisFrame() const
{
	for (u_int32 u = 0; u < uMAX_POINTERS; u++)
	{
		if (m_axPointers[u].m_bTapThisFrame)
		{
			return true;
		}
	}
	return false;
}

bool Zenith_Pointers::GetTapPosition(Zenith_Maths::Vector2& xOut) const
{
	for (u_int32 u = 0; u < uMAX_POINTERS; u++)
	{
		if (m_axPointers[u].m_bTapThisFrame)
		{
			xOut = m_axPointers[u].m_xStartPosition;
			return true;
		}
	}
	return false;
}

// ============================================================================
// Claims
// ============================================================================

bool Zenith_Pointers::ClaimPointer(const Zenith_PointerHandle& xHandle, Zenith_PointerOwner ulOwner)
{
	if (ulOwner == ulPOINTER_OWNER_NONE)
	{
		return false;
	}
	const Zenith_Pointer* pxConst = Resolve(xHandle);
	if (pxConst == nullptr)
	{
		return false;
	}
	Zenith_Pointer& xPointer = m_axPointers[xHandle.m_uSlot];
	if (xPointer.m_ulClaimOwner != ulPOINTER_OWNER_NONE && xPointer.m_ulClaimOwner != ulOwner)
	{
		// FIRST claim wins. The walk order is deterministic, so which widget that
		// is does not depend on anything but the canvas order.
		return false;
	}
	xPointer.m_ulClaimOwner = ulOwner;
	// A claimed pointer is a consumed one: it can no longer become a tap for the
	// gameplay layer, even if the claim is released before the finger lifts.
	xPointer.m_bTapThisFrame = false;
	return true;
}

void Zenith_Pointers::ReleaseClaim(const Zenith_PointerHandle& xHandle, Zenith_PointerOwner ulOwner)
{
	// Resolve() is what rejects a STALE generation: a widget still holding last
	// gesture's handle must not free the claim of whoever owns the slot now.
	if (Resolve(xHandle) == nullptr)
	{
		return;
	}
	Zenith_Pointer& xPointer = m_axPointers[xHandle.m_uSlot];
	if (xPointer.m_ulClaimOwner != ulOwner)
	{
		return;
	}
	xPointer.m_ulClaimOwner = ulPOINTER_OWNER_NONE;
}

bool Zenith_Pointers::IsClaimed(const Zenith_PointerHandle& xHandle) const
{
	const Zenith_Pointer* pxPointer = Resolve(xHandle);
	return pxPointer != nullptr && pxPointer->IsClaimed();
}

Zenith_PointerOwner Zenith_Pointers::GetClaimOwner(const Zenith_PointerHandle& xHandle) const
{
	const Zenith_Pointer* pxPointer = Resolve(xHandle);
	return pxPointer != nullptr ? pxPointer->m_ulClaimOwner : ulPOINTER_OWNER_NONE;
}

void Zenith_Pointers::ReleaseAllClaimsForOwner(Zenith_PointerOwner ulOwner)
{
	if (ulOwner == ulPOINTER_OWNER_NONE)
	{
		return;
	}
	for (u_int32 u = 0; u < uMAX_POINTERS; u++)
	{
		if (m_axPointers[u].m_ulClaimOwner == ulOwner)
		{
			m_axPointers[u].m_ulClaimOwner = ulPOINTER_OWNER_NONE;
		}
	}
}

#include "Input/Zenith_Pointers.Tests.inl"
