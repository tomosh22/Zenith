#pragma once

#include "UI/Zenith_UIElement.h"
#include <string>

/**
 * Zenith_UIVirtualStick - the on-screen thumbstick (B9).
 *
 * Publishes an AXIS2D value into the action layer through a VIRTUAL binding
 * source, so a game reads "Move" and never learns whether the answer came from
 * WASD, a gamepad stick or a thumb on glass.
 *
 * INPUT LIVES IN THE INPUT PHASE. Everything that decides a value happens in
 * UpdateVirtualInput (frame contract step 10d); Render() only draws what that
 * decided. A frame that submits no render work still has to answer a thumb, and
 * a control whose axis only updated on rendered frames would stutter exactly
 * when the game is busiest.
 *
 * GEOMETRY IS DENSITY-SCALED. Radius, deadzone and activation slop are authored
 * in LOGICAL pixels and multiplied by Zenith_Pointers::GetDisplayScale() at use
 * time, and the activation rect is never smaller than the B9 minimum touch
 * target. The pointer table hands out RAW SURFACE pixels, so an unscaled
 * threshold is a different physical distance on every panel.
 */

namespace Zenith_UI {

class Zenith_UIVirtualStick : public Zenith_UIElement
{
public:
	// FIXED    - the base stays at the authored rect's centre, so the very first
	//            frame of a gesture already carries a direction (a thumb landing
	//            at the rim means "full tilt that way").
	// FLOATING - the base RE-CENTRES at the down position inside the activation
	//            rect, so the first frame reads zero and the player's thumb never
	//            teleports the stick. The usual choice for a free-placement stick
	//            over the left half of the screen.
	enum class StickMode : uint32_t { FIXED, FLOATING };

	Zenith_UIVirtualStick(const std::string& strName = "UIVirtualStick");
	virtual ~Zenith_UIVirtualStick() = default;

	virtual UIElementType GetType() const override { return UIElementType::VirtualStick; }

	// ========== Action target (B9 retarget API) ==========
	//
	// Names an ACTION; the action's own VIRTUAL binding row names the source id
	// this widget publishes to, so a rebind stays a binding-table edit.
	//
	// Calling it MID-GESTURE is the retarget case the contract fixes: the OLD
	// action gets a release transition, the pointer claim is KEPT (the gesture
	// stays consumed, so nothing downstream sees a finger it never saw arrive),
	// and this widget is DISARMED for the new action until a FRESH down.
	//
	// The game-context controllers that drive it live at frame step 9 (WP3b).
	void SetAction(const char* szActionName);
	const std::string& GetActionName() const { return m_strActionName; }

	// ========== Geometry (LOGICAL pixels; scaled by the display scale) ==========

	void SetMode(StickMode eMode) { m_eMode = eMode; }
	StickMode GetMode() const { return m_eMode; }

	void SetRadius(float fLogicalPx) { m_fRadius = fLogicalPx > 0.0f ? fLogicalPx : 1.0f; }
	float GetRadius() const { return m_fRadius; }

	// Fraction of the radius inside which the axis is EXACTLY zero.
	void SetDeadzoneFraction(float fFraction);
	float GetDeadzoneFraction() const { return m_fDeadzoneFraction; }

	// Extra reach around the authored bounds, before the minimum-touch-target
	// growth. A stick sitting in a screen corner wants a generous one.
	void SetActivationSlop(float fLogicalPx) { m_fActivationSlop = fLogicalPx; }
	float GetActivationSlop() const { return m_fActivationSlop; }

	void SetKnobColor(const Zenith_Maths::Vector4& xColor) { m_xKnobColor = xColor; }
	Zenith_Maths::Vector4 GetKnobColor() const { return m_xKnobColor; }

	// ========== Live state (the visual pass and the units read these) ==========

	bool IsEngaged() const { return m_bEngaged; }
	// True between a retarget/hide MID-GESTURE and the end of that gesture. A
	// disarmed stick publishes nothing and cannot claim.
	bool IsDisarmed() const { return m_bDisarmed; }
	// The engine-convention axis (+y FORWARD), zero while not engaged.
	Zenith_Maths::Vector2 GetAxis() const { return m_xAxis; }
	// Canvas-space centre the axis is measured from. FLOATING moves it.
	Zenith_Maths::Vector2 GetBaseCentre() const { return m_xBaseCentre; }

	// The rect actually hit-tested: authored bounds + density-scaled slop, grown
	// to the minimum touch target.
	Zenith_Maths::Vector4 GetActivationRect(float fDisplayScale) const;

	// The axis curve, PURE and static so it can be pinned without a canvas, a
	// pointer table or a window.
	//
	// Canvas +Y is DOWN and the engine's axis2D convention (B4
	// ResolveMoveComposite) is +Y FORWARD, so the flip lives HERE, once —
	// otherwise every consumer would have to remember which of the two spaces a
	// virtual stick answers in.
	static Zenith_Maths::Vector2 ResolveAxis(const Zenith_Maths::Vector2& xBaseCentre,
		const Zenith_Maths::Vector2& xPointerCanvasPos, float fRadiusPx, float fDeadzoneFraction);

	// ========== Overrides ==========

	virtual void UpdateVirtualInput(Zenith_InputActions& xActions, Zenith_Pointers& xPointers, float fDt) override;
	virtual void Render(Zenith_UICanvas& xCanvas) override;
	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

#ifdef ZENITH_TOOLS
	virtual void RenderPropertiesPanel() override;
#endif

	// "No VIRTUAL source resolved" — the action is unregistered, or carries no
	// virtual row. A gesture is still claimed (it is ours), it just has nowhere
	// to publish; inventing an id would drive whatever else happens to use it.
	static constexpr u_int16 uVIRTUAL_SOURCE_NONE = 0xFFFF;

private:
	// True while the editor is Stopped and no simulator is driving: the canvas is
	// being AUTHORED, not played, so touches belong to the editor.
	bool IsInputSuppressedByEditor() const;

	// The action's VIRTUAL binding row -> the source id to publish to.
	u_int16 ResolveVirtualSource(const Zenith_InputActions& xActions) const;

	Zenith_Maths::Vector2 ToCanvasPosition(const Zenith_Maths::Vector2& xSurfacePos) const;
	Zenith_Maths::Vector2 GetBoundsCentre() const;

	// Recompute the axis from this pointer position and publish it (level) plus
	// the held bit (transition).
	void Engage(Zenith_InputActions& xActions, Zenith_Pointers& xPointers,
		const Zenith_Maths::Vector2& xSurfacePos);
	// Zero the level and FALL the held bit on whatever source we last published
	// to. Idempotent. The held fall is what produces the action's release EDGE at
	// 10e — an axis that merely goes quiet leaves the action held forever.
	void PublishRelease(Zenith_InputActions& xActions);

	std::string m_strActionName;

	StickMode m_eMode = StickMode::FIXED;
	float m_fRadius = 80.0f;
	float m_fDeadzoneFraction = 0.15f;
	float m_fActivationSlop = 0.0f;
	Zenith_Maths::Vector4 m_xKnobColor = {0.85f, 0.85f, 0.90f, 0.85f};

	// ---- runtime (not serialized) ----
	bool m_bEngaged = false;
	bool m_bDisarmed = false;
	Zenith_Maths::Vector2 m_xAxis = {0.0f, 0.0f};
	Zenith_Maths::Vector2 m_xBaseCentre = {0.0f, 0.0f};

	// The source the LAST publish went to. Held separately from the resolved id
	// because a retarget has to release the OLD one, and SetAction has no action
	// layer to reach for.
	u_int16 m_uPublishedSource = uVIRTUAL_SOURCE_NONE;

	// Latched by the 10d pass for the VISUAL pass, which has no action layer and
	// no pointer table by design (it is skippable, and nothing that decides input
	// may live there).
	bool m_bTouchSchemeActive = false;
	float m_fLatchedDisplayScale = 1.0f;
};

} // namespace Zenith_UI
