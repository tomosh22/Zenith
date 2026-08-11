#include "Zenith.h"
#include "UI/Zenith_UIVirtualButton.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIStyleRenderer.h"
#include "Input/Zenith_InputActions.h"
#include "Input/Zenith_Pointers.h"
// No Core/Zenith_Engine.h: everything this widget needs arrives as a parameter
// through the canvas walk (B13's singleton ratchet).
#ifdef ZENITH_INPUT_SIMULATOR
#include "Input/Zenith_InputSimulator.h"
#endif

#ifdef ZENITH_TOOLS
#include "Core/Zenith_EditorQuery.h"
#include "imgui.h"
#endif

namespace Zenith_UI {

static constexpr uint32_t UI_VIRTUAL_BUTTON_VERSION = 1;

Zenith_UIVirtualButton::Zenith_UIVirtualButton(const std::string& strName)
	: Zenith_UIElement(strName)
{
	// Already at the B9 minimum touch target at 1x before anyone sizes it.
	m_xSize = { 96.0f, 96.0f };

	// On-screen controls are NOT focus targets — the keyboard/gamepad path has
	// its own bindings for the same action, and letting focus land here would
	// give UI_CONFIRM a target that does nothing.
	m_bFocusable = false;

	m_xColor = { 1.0f, 1.0f, 1.0f, 0.35f };
}

void Zenith_UIVirtualButton::SetAction(const char* szActionName)
{
	const std::string strNew = szActionName != nullptr ? szActionName : "";
	if (strNew == m_strActionName)
	{
		// Re-targeting to the action we are already on is not a retarget.
		return;
	}

	m_strActionName = strNew;

	if (m_bHeld)
	{
		// B9: the release of the OLD action, the KEPT claim and the disarm all
		// land at the next 10d — m_uPublishedSource still names the old source,
		// which is what PublishRelease needs.
		m_bDisarmed = true;
	}
}

bool Zenith_UIVirtualButton::IsInputSuppressedByEditor() const
{
#ifdef ZENITH_TOOLS
	return g_xEditorQuery.m_pfnIsEditorStopped()
#ifdef ZENITH_INPUT_SIMULATOR
		&& !Zenith_InputSimulator::IsEnabled()
#endif
		;
#else
	return false;
#endif
}

u_int16 Zenith_UIVirtualButton::ResolveVirtualSource(const Zenith_InputActions& xActions) const
{
	if (m_strActionName.empty())
	{
		return uVIRTUAL_SOURCE_NONE;
	}

	const Zenith_InputActionID uAction = xActions.FindActionByName(m_strActionName.c_str());
	if (uAction == uINPUT_ACTION_INVALID)
	{
		return uVIRTUAL_SOURCE_NONE;
	}

	// The action's own VIRTUAL binding row names the source, so a rebind is a
	// binding-table edit rather than a canvas edit.
	const u_int32 uBindingCount = xActions.GetBindingCount(uAction);
	for (u_int32 u = 0; u < uBindingCount; u++)
	{
		const Zenith_InputBinding& xBinding = xActions.GetBinding(uAction, u);
		if (xBinding.m_eType == INPUT_BINDING_VIRTUAL && xBinding.m_iCode >= 0
			&& xBinding.m_iCode < static_cast<int32_t>(Zenith_InputActions::uMAX_VIRTUAL_SOURCES))
		{
			return static_cast<u_int16>(xBinding.m_iCode);
		}
	}
	return uVIRTUAL_SOURCE_NONE;
}

Zenith_Maths::Vector4 Zenith_UIVirtualButton::GetHitRect(float fDisplayScale) const
{
	return ResolveTouchTargetRect(GetScreenBounds(), m_fHitSlop, fDisplayScale);
}

void Zenith_UIVirtualButton::Engage(Zenith_InputActions& xActions)
{
	const u_int16 uSource = ResolveVirtualSource(xActions);

	// The action moved under us without a SetAction (a game re-registered its
	// bindings mid-gesture): release the old source before the new one rises.
	if (m_uPublishedSource != uVIRTUAL_SOURCE_NONE && m_uPublishedSource != uSource)
	{
		PublishRelease(xActions);
	}

	m_bHeld = true;

	if (uSource == uVIRTUAL_SOURCE_NONE)
	{
		return;
	}

	m_uPublishedSource = uSource;
	// Publishing the same level twice is a no-op, so this runs every frame of
	// the hold without producing a second rise.
	xActions.PublishVirtualButton(uSource, true);
}

void Zenith_UIVirtualButton::PublishRelease(Zenith_InputActions& xActions)
{
	m_bHeld = false;

	if (m_uPublishedSource == uVIRTUAL_SOURCE_NONE)
	{
		return;
	}

	xActions.PublishVirtualButton(m_uPublishedSource, false);
	m_uPublishedSource = uVIRTUAL_SOURCE_NONE;
}

void Zenith_UIVirtualButton::UpdateVirtualInput(Zenith_InputActions& xActions, Zenith_Pointers& xPointers, float fDt)
{
	(void)fDt;   // held state is edge-driven; only a drag would need a rate

	// B9: on-screen controls exist only while the ACTIVE profile carries TOUCH.
	// Latched for the visual pass, which has no action layer.
	m_bTouchSchemeActive = (xActions.GetActiveSchemeMask() & uINPUT_SCHEME_MASK_TOUCH) != 0;

	const bool bEnabled = m_bVisible
		&& IsGroupInteractable()
		&& m_bTouchSchemeActive
		&& !IsInputSuppressedByEditor();

	// ---- Following a capture ----------------------------------------------
	const Zenith_Pointer* pxCaptured = ResolveCapturedPointer(xPointers);
	if (pxCaptured != nullptr)
	{
		if (pxCaptured->m_bUpThisFrame || pxCaptured->m_bCancelledThisFrame)
		{
			PublishRelease(xActions);
			ReleaseCapturedPointer();
			m_bDisarmed = false;
			return;
		}

		if (!bEnabled || m_bDisarmed)
		{
			// Retargeted, hidden or masked out MID-HOLD: release the old action,
			// KEEP the claim (B9), stay disarmed until this pointer is gone.
			PublishRelease(xActions);
			m_bDisarmed = true;
			return;
		}

		// Deliberately NOT re-tested against the hit rect. The CLAIM is what
		// holds this button: a thumb that creeps a few pixels off a fire button
		// under its own recoil must not stop firing, and the claim is kept
		// through a drag-off by contract anyway. Zenith_UIButton, a MENU control
		// that must let you slide off to cancel, makes the opposite choice.
		Engage(xActions);
		return;
	}

	// ---- Nothing in flight -------------------------------------------------
	PublishRelease(xActions);
	m_bDisarmed = false;

	if (!bEnabled)
	{
		return;
	}

	const Zenith_Maths::Vector4 xHitRect = GetHitRect(xPointers.GetDisplayScale());

	for (u_int32 u = 0; u < Zenith_Pointers::uMAX_POINTERS; u++)
	{
		const Zenith_Pointer& xPointer = xPointers.GetPointer(u);
		if (!xPointer.m_bDownThisFrame || xPointer.IsClaimed())
		{
			continue;
		}
		if (!ContainsSurfacePositionInRect(xPointer.m_xPosition, xHitRect))
		{
			continue;
		}
		if (!CapturePointer(xPointers, xPointers.GetHandle(u)))
		{
			continue;
		}

		Engage(xActions);

		// Down AND up inside ONE frame — every quick tap. Both publishes land as
		// ORDERED virtual transitions, so 10e replays a genuine rise AND fall and
		// the action fires both edges.
		if (xPointer.m_bUpThisFrame || xPointer.m_bCancelledThisFrame)
		{
			PublishRelease(xActions);
			ReleaseCapturedPointer();
		}
		break;
	}
}

void Zenith_UIVirtualButton::Render(Zenith_UICanvas& xCanvas)
{
	// The mask answer is LATCHED by 10d: the visual pass cannot ask the action
	// layer, and a pass a frame may skip must never decide anything about input.
	if (!m_bVisible || !m_bTouchSchemeActive)
	{
		return;
	}

	const float fAlpha = GetEffectiveAlpha();
	const Zenith_Maths::Vector4 xBounds = GetScreenBounds();

	UIStyle xStyle;
	xStyle.m_xFillColor = m_bHeld ? m_xPressedColor : m_xColor;
	// A pill: half the shorter side is the largest radius that still rounds.
	xStyle.m_fCornerRadius = glm::min(xBounds.z - xBounds.x, xBounds.w - xBounds.y) * 0.5f;

	UIStyleRenderer::RenderStyledRect(xCanvas, xStyle, xBounds, fAlpha);

	Zenith_UIElement::Render(xCanvas);
}

void Zenith_UIVirtualButton::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_UIElement::WriteToDataStream(xStream);

	xStream << UI_VIRTUAL_BUTTON_VERSION;
	xStream << m_strActionName;
	xStream << m_fHitSlop;
	xStream << m_xPressedColor.x; xStream << m_xPressedColor.y; xStream << m_xPressedColor.z; xStream << m_xPressedColor.w;
}

void Zenith_UIVirtualButton::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_UIElement::ReadFromDataStream(xStream);

	uint32_t uVersion;
	xStream >> uVersion;

	Zenith_Assert(uVersion <= UI_VIRTUAL_BUTTON_VERSION, "UIVirtualButton version mismatch");

	xStream >> m_strActionName;
	xStream >> m_fHitSlop;
	xStream >> m_xPressedColor.x; xStream >> m_xPressedColor.y; xStream >> m_xPressedColor.z; xStream >> m_xPressedColor.w;

	// Runtime state is never serialized: a deserialized control is never
	// mid-gesture, and a stale "held" would publish an action nobody pressed.
	m_bHeld = false;
	m_bDisarmed = false;
	m_uPublishedSource = uVIRTUAL_SOURCE_NONE;
}

#ifdef ZENITH_TOOLS
void Zenith_UIVirtualButton::RenderPropertiesPanel()
{
	Zenith_UIElement::RenderPropertiesPanel();

	ImGui::PushID("UIVirtualButtonProps");

	ImGui::Separator();
	ImGui::Text("Virtual Button Properties");

	char szActionBuffer[Zenith_InputActions::uMAX_NAME_LENGTH];
	strncpy_s(szActionBuffer, m_strActionName.c_str(), sizeof(szActionBuffer) - 1);
	if (ImGui::InputText("Action", szActionBuffer, sizeof(szActionBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
	{
		SetAction(szActionBuffer);
	}

	ImGui::DragFloat("Hit Slop (logical px)", &m_fHitSlop, 1.0f, 0.0f, 256.0f);

	float fPressedColor[4] = { m_xPressedColor.x, m_xPressedColor.y, m_xPressedColor.z, m_xPressedColor.w };
	if (ImGui::ColorEdit4("Pressed Color", fPressedColor))
	{
		m_xPressedColor = { fPressedColor[0], fPressedColor[1], fPressedColor[2], fPressedColor[3] };
	}

	ImGui::Separator();
	ImGui::Text("Held: %s", m_bHeld ? "Yes" : "No");
	ImGui::Text("Disarmed: %s", m_bDisarmed ? "Yes" : "No");
	ImGui::Text("Touch scheme active: %s", m_bTouchSchemeActive ? "Yes" : "No");

	ImGui::PopID();
}
#endif

} // namespace Zenith_UI
