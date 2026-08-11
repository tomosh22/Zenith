#pragma once

#include "UI/Zenith_UIElement.h"
#include <string>

/**
 * Zenith_UIVirtualButton - the on-screen action button (B9).
 *
 * Publishes HELD state into the action layer through a VIRTUAL binding source,
 * so a game reads "Jump" and never learns whether the answer came from the
 * space bar, pad A, or a thumb on glass.
 *
 * The sibling of Zenith_UIVirtualStick and it obeys the same three rules:
 *   - input decisions live ONLY in UpdateVirtualInput (frame contract step 10d),
 *     never in the skippable visual pass;
 *   - geometry is authored in LOGICAL pixels and density-scaled at use time,
 *     never smaller than the B9 minimum touch target;
 *   - a retarget or a hide MID-GESTURE releases the OLD action, KEEPS the
 *     pointer claim, and disarms until a fresh DOWN.
 *
 * NOT a Zenith_UIButton. That one is a MENU control: it fires a callback on a
 * release inside its bounds, it is focusable, and it un-presses when the finger
 * drags off. This one is a GAMEPLAY control: it has no callback, no focus, and
 * it answers "is it held", because that is the question a fire button asks.
 */

namespace Zenith_UI {

class Zenith_UIVirtualButton : public Zenith_UIElement
{
public:
	Zenith_UIVirtualButton(const std::string& strName = "UIVirtualButton");
	virtual ~Zenith_UIVirtualButton() = default;

	virtual UIElementType GetType() const override { return UIElementType::VirtualButton; }

	// ========== Action target (B9 retarget API) ==========
	//
	// Names an ACTION; the action's own VIRTUAL binding row names the source id
	// this widget publishes to. Calling it MID-GESTURE releases the OLD action,
	// KEEPS the claim (the gesture stays consumed) and disarms this widget for
	// the new action until a FRESH down. Game context controllers drive it at
	// frame step 9 (WP3b).
	void SetAction(const char* szActionName);
	const std::string& GetActionName() const { return m_strActionName; }

	// ========== Geometry (LOGICAL pixels; scaled by the display scale) ==========

	// Extra reach around the authored bounds, applied before the
	// minimum-touch-target growth.
	void SetHitSlop(float fLogicalPx) { m_fHitSlop = fLogicalPx; }
	float GetHitSlop() const { return m_fHitSlop; }

	void SetPressedColor(const Zenith_Maths::Vector4& xColor) { m_xPressedColor = xColor; }
	Zenith_Maths::Vector4 GetPressedColor() const { return m_xPressedColor; }

	// ========== Live state ==========

	bool IsHeld() const { return m_bHeld; }
	// True between a retarget/hide MID-GESTURE and the end of that gesture.
	bool IsDisarmed() const { return m_bDisarmed; }

	// The rect actually hit-tested: authored bounds + density-scaled hit slop,
	// grown to the minimum touch target.
	Zenith_Maths::Vector4 GetHitRect(float fDisplayScale) const;

	// ========== Overrides ==========

	virtual void UpdateVirtualInput(Zenith_InputActions& xActions, Zenith_Pointers& xPointers, float fDt) override;
	virtual void Render(Zenith_UICanvas& xCanvas) override;
	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

#ifdef ZENITH_TOOLS
	virtual void RenderPropertiesPanel() override;
#endif

	// "No VIRTUAL source resolved" — see Zenith_UIVirtualStick for the rule.
	static constexpr u_int16 uVIRTUAL_SOURCE_NONE = 0xFFFF;

private:
	bool IsInputSuppressedByEditor() const;
	u_int16 ResolveVirtualSource(const Zenith_InputActions& xActions) const;

	void Engage(Zenith_InputActions& xActions);
	// Fall the held bit on whatever source we last published to. Idempotent.
	void PublishRelease(Zenith_InputActions& xActions);

	std::string m_strActionName;

	float m_fHitSlop = 8.0f;
	Zenith_Maths::Vector4 m_xPressedColor = {1.0f, 1.0f, 1.0f, 0.75f};

	// ---- runtime (not serialized) ----
	bool m_bHeld = false;
	bool m_bDisarmed = false;
	u_int16 m_uPublishedSource = uVIRTUAL_SOURCE_NONE;

	// Latched by 10d for the visual pass (which has no action layer).
	bool m_bTouchSchemeActive = false;
};

} // namespace Zenith_UI
