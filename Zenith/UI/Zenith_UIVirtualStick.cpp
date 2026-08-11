#include "Zenith.h"
#include "UI/Zenith_UIVirtualStick.h"
#include "UI/Zenith_UICanvas.h"
#include "Input/Zenith_InputActions.h"
#include "Input/Zenith_Pointers.h"
// No Core/Zenith_Engine.h: this widget reads NO global state at all. The action
// layer and the pointer table both arrive as parameters through the canvas walk
// (B13's singleton ratchet), which is also what lets a unit drive it against
// LOCAL instances of both.
#ifdef ZENITH_INPUT_SIMULATOR
#include "Input/Zenith_InputSimulator.h"
#endif

#ifdef ZENITH_TOOLS
#include "Core/Zenith_EditorQuery.h"
#include "imgui.h"
#endif

namespace Zenith_UI {

static constexpr uint32_t UI_VIRTUAL_STICK_VERSION = 1;

// The largest deadzone that still leaves a usable band between it and the rim.
static constexpr float fMAX_DEADZONE_FRACTION = 0.95f;

Zenith_UIVirtualStick::Zenith_UIVirtualStick(const std::string& strName)
	: Zenith_UIElement(strName)
{
	// A default that already satisfies the B9 minimum touch target at 1x, so a
	// stick dropped in the editor is usable before anyone sizes it.
	m_xSize = { 160.0f, 160.0f };

	// On-screen controls are NOT focus targets: focus navigation is the
	// keyboard/gamepad path, and a stick is the one control that path cannot
	// operate. Leaving it focusable would let an arrow key walk onto it and then
	// have UI_CONFIRM do nothing.
	m_bFocusable = false;

	m_xColor = { 1.0f, 1.0f, 1.0f, 0.35f };
}

void Zenith_UIVirtualStick::SetDeadzoneFraction(float fFraction)
{
	m_fDeadzoneFraction = glm::clamp(fFraction, 0.0f, fMAX_DEADZONE_FRACTION);
}

void Zenith_UIVirtualStick::SetAction(const char* szActionName)
{
	const std::string strNew = szActionName != nullptr ? szActionName : "";
	if (strNew == m_strActionName)
	{
		// Re-targeting to the action we are already on is not a retarget, and
		// must not interrupt a gesture in flight.
		return;
	}

	m_strActionName = strNew;

	if (m_bEngaged)
	{
		// B9: the OLD action's release, the KEPT claim and the disarm all happen
		// at the next 10d — this call can arrive from anywhere in the frame and
		// has no action layer to write into. m_uPublishedSource still names the
		// old source, which is exactly what PublishRelease needs.
		m_bDisarmed = true;
	}
}

bool Zenith_UIVirtualStick::IsInputSuppressedByEditor() const
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

u_int16 Zenith_UIVirtualStick::ResolveVirtualSource(const Zenith_InputActions& xActions) const
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

	// The widget publishes to a SOURCE, not to an action. Which source is the
	// action's own VIRTUAL binding row's business, so a game rebinds an on-screen
	// control by editing its binding table and never by editing its canvas.
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

Zenith_Maths::Vector2 Zenith_UIVirtualStick::ToCanvasPosition(const Zenith_Maths::Vector2& xSurfacePos) const
{
	float fX = xSurfacePos.x;
	float fY = xSurfacePos.y;
	TransformSurfacePosition(fX, fY);
	return { fX, fY };
}

Zenith_Maths::Vector2 Zenith_UIVirtualStick::GetBoundsCentre() const
{
	const Zenith_Maths::Vector4 xBounds = GetScreenBounds();
	return { (xBounds.x + xBounds.z) * 0.5f, (xBounds.y + xBounds.w) * 0.5f };
}

Zenith_Maths::Vector4 Zenith_UIVirtualStick::GetActivationRect(float fDisplayScale) const
{
	return ResolveTouchTargetRect(GetScreenBounds(), m_fActivationSlop, fDisplayScale);
}

Zenith_Maths::Vector2 Zenith_UIVirtualStick::ResolveAxis(const Zenith_Maths::Vector2& xBaseCentre,
	const Zenith_Maths::Vector2& xPointerCanvasPos, float fRadiusPx, float fDeadzoneFraction)
{
	const float fDX = xPointerCanvasPos.x - xBaseCentre.x;
	// Canvas +Y is DOWN, the action layer's +Y is FORWARD. One flip, here.
	const float fDY = xBaseCentre.y - xPointerCanvasPos.y;

	const float fLength = std::sqrt(fDX * fDX + fDY * fDY);
	if (fRadiusPx <= 0.0f || fLength <= 0.0f)
	{
		return { 0.0f, 0.0f };
	}

	const float fDeadzonePx = fRadiusPx * glm::clamp(fDeadzoneFraction, 0.0f, fMAX_DEADZONE_FRACTION);
	if (fLength <= fDeadzonePx)
	{
		// EXACTLY zero, not merely small: a resting thumb must not drift a
		// character across a room, and consumers compare the axis against 0.
		return { 0.0f, 0.0f };
	}

	// Rescaled so the magnitude leaves the deadzone at 0 and reaches 1 AT the
	// radius. Without the rescale the axis would jump from 0 to
	// deadzone/radius the instant the thumb crossed the boundary — a visible
	// flick on every input.
	const float fSpan = fRadiusPx - fDeadzonePx;
	const float fMagnitude = glm::min((fLength - fDeadzonePx) / fSpan, 1.0f);

	return { (fDX / fLength) * fMagnitude, (fDY / fLength) * fMagnitude };
}

void Zenith_UIVirtualStick::Engage(Zenith_InputActions& xActions, Zenith_Pointers& xPointers,
	const Zenith_Maths::Vector2& xSurfacePos)
{
	const u_int16 uSource = ResolveVirtualSource(xActions);

	// The action moved under us without a SetAction (a game re-registered its
	// bindings mid-gesture). Same rule as a retarget: the old source is released
	// before the new one ever rises.
	if (m_uPublishedSource != uVIRTUAL_SOURCE_NONE && m_uPublishedSource != uSource)
	{
		PublishRelease(xActions);
	}

	const float fScale = xPointers.GetDisplayScale();
	m_fLatchedDisplayScale = fScale > 0.0f ? fScale : 1.0f;

	m_xAxis = ResolveAxis(m_xBaseCentre, ToCanvasPosition(xSurfacePos),
		m_fRadius * m_fLatchedDisplayScale, m_fDeadzoneFraction);
	m_bEngaged = true;

	if (uSource == uVIRTUAL_SOURCE_NONE)
	{
		return;
	}

	m_uPublishedSource = uSource;
	// The axis is a LEVEL; the held bit is an ordered TRANSITION. Publishing the
	// same level twice is a no-op, so this may run every frame of the gesture.
	xActions.PublishVirtualAxis(uSource, m_xAxis.x, m_xAxis.y);
	xActions.PublishVirtualButton(uSource, true);
}

void Zenith_UIVirtualStick::PublishRelease(Zenith_InputActions& xActions)
{
	m_bEngaged = false;
	m_xAxis = { 0.0f, 0.0f };

	if (m_uPublishedSource == uVIRTUAL_SOURCE_NONE)
	{
		return;
	}

	xActions.PublishVirtualAxis(m_uPublishedSource, 0.0f, 0.0f);
	xActions.PublishVirtualButton(m_uPublishedSource, false);
	m_uPublishedSource = uVIRTUAL_SOURCE_NONE;
}

void Zenith_UIVirtualStick::UpdateVirtualInput(Zenith_InputActions& xActions, Zenith_Pointers& xPointers, float fDt)
{
	(void)fDt;   // a stick is position-driven; nothing here needs a rate

	// B9: an on-screen control exists only while the ACTIVE profile carries the
	// TOUCH scheme. Latched for the visual pass, which cannot ask.
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
			// The gesture ended: release the action, hand the claim back, and
			// re-arm. A disarm only lasts until a FRESH down, and this was the
			// end of the gesture it was disarmed against.
			PublishRelease(xActions);
			ReleaseCapturedPointer();
			m_bDisarmed = false;
			return;
		}

		if (!bEnabled || m_bDisarmed)
		{
			// Retargeted, hidden, or masked out MID-GESTURE. The old action gets
			// its release; the claim is KEPT so the gesture stays consumed (B9)
			// and no pointer-fed row downstream sees a finger it never saw
			// arrive; and the widget stays disarmed until this pointer is gone.
			PublishRelease(xActions);
			m_bDisarmed = true;
			return;
		}

		Engage(xActions, xPointers, pxCaptured->m_xPosition);
		return;
	}

	// ---- Nothing in flight -------------------------------------------------
	// Idempotent when there was nothing published; it is what closes a gesture
	// whose pointer vanished without this widget seeing the terminal edge.
	PublishRelease(xActions);
	m_bDisarmed = false;

	if (!bEnabled)
	{
		return;
	}

	const float fScale = xPointers.GetDisplayScale();
	m_fLatchedDisplayScale = fScale > 0.0f ? fScale : 1.0f;
	const Zenith_Maths::Vector4 xActivation = GetActivationRect(m_fLatchedDisplayScale);

	for (u_int32 u = 0; u < Zenith_Pointers::uMAX_POINTERS; u++)
	{
		const Zenith_Pointer& xPointer = xPointers.GetPointer(u);
		if (!xPointer.m_bDownThisFrame || xPointer.IsClaimed())
		{
			continue;
		}
		if (!ContainsSurfacePositionInRect(xPointer.m_xPosition, xActivation))
		{
			continue;
		}
		if (!CapturePointer(xPointers, xPointers.GetHandle(u)))
		{
			continue;
		}

		// FLOATING recentres on the down position, so the first frame reads zero
		// and the stick appears under the thumb rather than dragging it to the
		// authored centre.
		m_xBaseCentre = (m_eMode == StickMode::FLOATING)
			? ToCanvasPosition(xPointer.m_xPosition)
			: GetBoundsCentre();

		Engage(xActions, xPointers, xPointer.m_xPosition);

		// Down AND up inside ONE frame. The whole gesture rides the pointer just
		// claimed, so close it here rather than waiting for a frame it has
		// already outlived.
		if (xPointer.m_bUpThisFrame || xPointer.m_bCancelledThisFrame)
		{
			PublishRelease(xActions);
			ReleaseCapturedPointer();
		}
		break;
	}
}

void Zenith_UIVirtualStick::Render(Zenith_UICanvas& xCanvas)
{
	// B9: on screen only while the ACTIVE profile carries TOUCH. The mask answer
	// is LATCHED by 10d — the visual pass has no action layer, deliberately, and
	// a pass a frame may skip must never be where an input question is asked.
	if (!m_bVisible || !m_bTouchSchemeActive)
	{
		return;
	}

	const float fAlpha = GetEffectiveAlpha();
	const float fRadiusPx = m_fRadius * m_fLatchedDisplayScale;
	const Zenith_Maths::Vector2 xBase = m_bEngaged ? m_xBaseCentre : GetBoundsCentre();

	// A quad with a corner radius of half its side IS a circle.
	const Zenith_Maths::Vector4 xBaseBounds = {
		xBase.x - fRadiusPx, xBase.y - fRadiusPx,
		xBase.x + fRadiusPx, xBase.y + fRadiusPx
	};
	Zenith_Maths::Vector4 xBaseColor = m_xColor;
	xBaseColor.w *= fAlpha;
	xCanvas.SubmitQuad(xBaseBounds, xBaseColor, 0, fRadiusPx);

	// The knob sits where the thumb pulled it. m_xAxis is in ENGINE convention
	// (+y forward), so the y flips back on the way to the screen.
	const float fKnobRadiusPx = fRadiusPx * 0.4f;
	const Zenith_Maths::Vector2 xKnob = {
		xBase.x + m_xAxis.x * fRadiusPx,
		xBase.y - m_xAxis.y * fRadiusPx
	};
	const Zenith_Maths::Vector4 xKnobBounds = {
		xKnob.x - fKnobRadiusPx, xKnob.y - fKnobRadiusPx,
		xKnob.x + fKnobRadiusPx, xKnob.y + fKnobRadiusPx
	};
	Zenith_Maths::Vector4 xKnobColor = m_xKnobColor;
	xKnobColor.w *= fAlpha;
	xCanvas.SubmitQuad(xKnobBounds, xKnobColor, 0, fKnobRadiusPx);

	Zenith_UIElement::Render(xCanvas);
}

void Zenith_UIVirtualStick::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_UIElement::WriteToDataStream(xStream);

	xStream << UI_VIRTUAL_STICK_VERSION;
	xStream << m_strActionName;
	xStream << static_cast<uint32_t>(m_eMode);
	xStream << m_fRadius;
	xStream << m_fDeadzoneFraction;
	xStream << m_fActivationSlop;
	xStream << m_xKnobColor.x; xStream << m_xKnobColor.y; xStream << m_xKnobColor.z; xStream << m_xKnobColor.w;
}

void Zenith_UIVirtualStick::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Zenith_UIElement::ReadFromDataStream(xStream);

	uint32_t uVersion;
	xStream >> uVersion;

	Zenith_Assert(uVersion <= UI_VIRTUAL_STICK_VERSION, "UIVirtualStick version mismatch");

	xStream >> m_strActionName;

	uint32_t uMode;
	xStream >> uMode;
	m_eMode = static_cast<StickMode>(uMode);

	xStream >> m_fRadius;
	xStream >> m_fDeadzoneFraction;
	xStream >> m_fActivationSlop;
	xStream >> m_xKnobColor.x; xStream >> m_xKnobColor.y; xStream >> m_xKnobColor.z; xStream >> m_xKnobColor.w;

	// Runtime state is never serialized: a deserialized control is never
	// mid-gesture, and a stale "engaged" would publish a held action nobody
	// pressed.
	m_bEngaged = false;
	m_bDisarmed = false;
	m_xAxis = { 0.0f, 0.0f };
	m_uPublishedSource = uVIRTUAL_SOURCE_NONE;
}

#ifdef ZENITH_TOOLS
void Zenith_UIVirtualStick::RenderPropertiesPanel()
{
	Zenith_UIElement::RenderPropertiesPanel();

	ImGui::PushID("UIVirtualStickProps");

	ImGui::Separator();
	ImGui::Text("Virtual Stick Properties");

	char szActionBuffer[Zenith_InputActions::uMAX_NAME_LENGTH];
	strncpy_s(szActionBuffer, m_strActionName.c_str(), sizeof(szActionBuffer) - 1);
	if (ImGui::InputText("Action", szActionBuffer, sizeof(szActionBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
	{
		SetAction(szActionBuffer);
	}

	const char* szModes[] = { "Fixed", "Floating" };
	int iMode = static_cast<int>(m_eMode);
	if (ImGui::Combo("Mode", &iMode, szModes, 2))
	{
		m_eMode = static_cast<StickMode>(iMode);
	}

	ImGui::DragFloat("Radius (logical px)", &m_fRadius, 1.0f, 8.0f, 512.0f);

	float fDeadzone = m_fDeadzoneFraction;
	if (ImGui::DragFloat("Deadzone (fraction)", &fDeadzone, 0.01f, 0.0f, fMAX_DEADZONE_FRACTION))
	{
		SetDeadzoneFraction(fDeadzone);
	}

	ImGui::DragFloat("Activation Slop (logical px)", &m_fActivationSlop, 1.0f, 0.0f, 256.0f);

	float fKnobColor[4] = { m_xKnobColor.x, m_xKnobColor.y, m_xKnobColor.z, m_xKnobColor.w };
	if (ImGui::ColorEdit4("Knob Color", fKnobColor))
	{
		m_xKnobColor = { fKnobColor[0], fKnobColor[1], fKnobColor[2], fKnobColor[3] };
	}

	ImGui::Separator();
	ImGui::Text("Engaged: %s", m_bEngaged ? "Yes" : "No");
	ImGui::Text("Disarmed: %s", m_bDisarmed ? "Yes" : "No");
	ImGui::Text("Axis: %.3f, %.3f", m_xAxis.x, m_xAxis.y);
	ImGui::Text("Touch scheme active: %s", m_bTouchSchemeActive ? "Yes" : "No");

	ImGui::PopID();
}
#endif

} // namespace Zenith_UI

// Hosted here (not in Zenith/Input) because these units name UI types, and an
// Input TU that included UI/ would be a layer-up violation. This .obj is always
// linked: Zenith_UIElement::CreateFromType names Zenith_UIVirtualStick, so MSVC
// cannot dead-strip it and the static test registrars always run.
// ZENITH_TEST macros self-noop when ZENITH_TESTING is undefined, so this include
// stays unconditional (matching every other .Tests.inl host).
#include "UI/Zenith_UIVirtualControls.Tests.inl"
