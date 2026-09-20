#include "Zenith.h"

// =============================================================================
// SentinelUI — leaf-proof executable for ZenithUI. See
// Build/Sharpmake_SentinelUI.cs for the full rationale AND the important
// caveat below — this sentinel proves a WEAKER claim than its six siblings.
//
// ★ FINDING (discovered building this sentinel, not assumed going in): a
// SentinelECS-style proof — construct one real, concrete class from the
// library and drive it — is NOT achievable for ZenithUI without pulling in
// the ENTIRE render backend + EngineComposition (g_xEngine). The reason is
// structural, not a misplaced file: Zenith_UIElement.cpp (the base class
// EVERY concrete widget's constructor needs, including Zenith_UIRect's) also
// defines `Zenith_UIElement::CreateElement(UIElementType)` — a factory
// function with a switch-case `new`-ing every sibling widget type, including
// `Zenith_UIOverlay` — plus `Zenith_UIElement::Render(Zenith_UICanvas&)` and
// a direct `g_xEngine.Input().GetMousePosition(...)` call. Because Sharpmake
// links at .obj granularity, constructing ANY ONE element pulls this ENTIRE
// .obj, including the factory's references to every sibling constructor and
// Render's reach into the live renderer — which cascades through
// Zenith_UICanvas.obj into ~25 `Zenith_Engine::<Subsystem>()` accessors, four
// `Zenith_Active*()` free functions, and a dozen `Zenith_Vulkan_*` backend
// methods (measured: 113 unresolved externals). None of that is declared in
// Sharpmake_ZenithUI.cs's dependency list (Core/Maths/DataStream/
// AssetHandling/Flux/Input/Profiling) because it is reached only through
// g_xEngine, which no leaf project — not even ZenithFlux itself — declares an
// edge to; only the aggregate (ZenithProject) defines it.
//
// This is architecturally CORRECT, not a defect: a UI widget's whole job is
// to submit real draw calls to the live renderer every frame, so real
// widget code is inherently coupled to the live engine singleton. It does
// mean ZenithUI, unlike ZenithECS/ZenithPhysics/ZenithAI/ZenithScripting, is
// NOT severable into an EngineComposition-free leaf at this granularity —
// stubbing the ~110-symbol surface (a meaningful slice of the Vulkan backend
// plus every Flux subsystem accessor) would not be a platform shim, it would
// be hand-faking the renderer, which is fragile and misrepresents what the
// sentinel proves. So this sentinel deliberately does NOT construct a
// Zenith_UIElement (or any subclass). It proves the two things that ARE
// true without that reach: the declared Sharpmake dependency edges resolve
// as link inputs, and ZenithUI's public data type (UIStyle) integrates at
// compile time; the Flux-leaf proof is the same GPU-free RecordDrawCall call
// Tests/SentinelFlux/main.cpp uses.
// =============================================================================

#include "UI/Zenith_UIStyle.h"
#include "Flux/Zenith_RenderBus.h"

namespace
{
	int s_iFailures = 0;

	void Expect(bool bCondition, const char* szWhat)
	{
		if (!bCondition)
		{
			++s_iFailures;
			Zenith_Error(LOG_CATEGORY_UNITTEST, "SentinelUI FAIL: %s", szWhat);
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "SentinelUI ok:   %s", szWhat);
		}
	}
}

int main()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "SentinelUI: UI leaf-proof starting "
		"(zenithui.lib's declared dependency edges + a GPU-free Flux touch; "
		"see this file's own header comment for why it stops short of "
		"constructing a live Zenith_UIElement)");

	// 1. UIStyle — ZenithUI's public data type, header-only, no link pull.
	//    Proves the library's own headers (which transitively reach imgui +
	//    Flux + Maths types) compile cleanly against this sentinel's include
	//    path set.
	Zenith_UI::UIStyle xStyle;
	xStyle.m_fCornerRadius = 4.0f;
	xStyle.m_bShadowEnabled = true;
	Expect(xStyle.m_fCornerRadius == 4.0f, "UIStyle is constructible and assignable");

	// 2. ZenithFlux — a real, GPU-free out-of-line call (same one
	//    Tests/SentinelFlux/main.cpp exercises), forcing zenithflux.lib
	//    (ZenithUI's own declared dependency) to link.
	Zenith_RenderBus::RecordDrawCall("SentinelUI.Probe", Zenith_Maths::Matrix4(1.0f), 0u, 0u);
	Expect(true, "Zenith_RenderBus::RecordDrawCall resolves from zenithflux.lib");

	if (s_iFailures == 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "SentinelUI: PASS (declared dependency edges resolve; see header comment for scope)");
		return 0;
	}

	Zenith_Error(LOG_CATEGORY_UNITTEST, "SentinelUI: FAIL (%d expectation(s) failed)", s_iFailures);
	return 1;
}
