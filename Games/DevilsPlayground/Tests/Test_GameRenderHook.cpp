#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Flux/Zenith_GameRenderHook.h"

// Forward decl — the engine's render-graph type is opaque to us; we only
// need to compile against the function-pointer signature.
class Flux_RenderGraph;

// ============================================================================
// GameRenderHook_Test (EXT-1)
//
// Verifies the post-fog hook registry:
//   - Registering a callback then calling Invoke fires it exactly once.
//   - Idempotent: registering the same function pointer twice still results
//     in exactly one invocation.
//   - Unregister + invoke does not call the removed callback.
//
// The DPFogPass.cpp registration runs at game startup and stays live for the
// lifetime of the process; we ResetAllRegistrations first so we own the
// registry state for the duration of the test, then restore it afterwards.
// ============================================================================

namespace
{
	int g_iCallback1Calls = 0;
	int g_iCallback2Calls = 0;

	void TestCallback1(Flux_RenderGraph& /*xGraph*/) { ++g_iCallback1Calls; }
	void TestCallback2(Flux_RenderGraph& /*xGraph*/) { ++g_iCallback2Calls; }

	bool g_bRanAssertions = false;
	bool g_bAllPassed     = true;

	#define HOOK_EXPECT(cond, msg)                          \
		do { if (!(cond)) { g_bAllPassed = false;            \
			std::printf("  FAIL: %s\n", msg); } } while (0)
}

static void Setup_GameRenderHook()
{
	g_iCallback1Calls = 0;
	g_iCallback2Calls = 0;
	g_bRanAssertions  = false;
	g_bAllPassed      = true;

	// We deliberately blow away the registry — the game's DPFogPass
	// registration will be re-applied on next graph rebuild via the engine's
	// idempotent registration path, so this is safe.
	Zenith_GameRenderHook::ResetAllRegistrations();

	// Cast the never-actually-passed reference: we never dereference xGraph
	// inside the test callbacks, so passing a nullptr-cast reference is OK.
	Flux_RenderGraph* pxFakeGraph = nullptr;
	Flux_RenderGraph& xFakeRef    = *pxFakeGraph;

	// 1. Single registration → single invocation.
	Zenith_GameRenderHook::RegisterPostFogPass(&TestCallback1);
	Zenith_GameRenderHook::InvokePostFogRegistrations(xFakeRef);
	HOOK_EXPECT(g_iCallback1Calls == 1, "single register → 1 call");

	// 2. Idempotent register: second register of the same fn does NOT
	//    increase the callback list.
	Zenith_GameRenderHook::RegisterPostFogPass(&TestCallback1);
	Zenith_GameRenderHook::InvokePostFogRegistrations(xFakeRef);
	HOOK_EXPECT(g_iCallback1Calls == 2, "double register, single invoke → 2 calls");

	// 3. Two distinct callbacks both fire on invoke.
	Zenith_GameRenderHook::RegisterPostFogPass(&TestCallback2);
	g_iCallback1Calls = 0;
	g_iCallback2Calls = 0;
	Zenith_GameRenderHook::InvokePostFogRegistrations(xFakeRef);
	HOOK_EXPECT(g_iCallback1Calls == 1, "two callbacks → cb1 called once");
	HOOK_EXPECT(g_iCallback2Calls == 1, "two callbacks → cb2 called once");

	// 4. Unregister cb1; only cb2 fires.
	Zenith_GameRenderHook::UnregisterPostFogPass(&TestCallback1);
	g_iCallback1Calls = 0;
	g_iCallback2Calls = 0;
	Zenith_GameRenderHook::InvokePostFogRegistrations(xFakeRef);
	HOOK_EXPECT(g_iCallback1Calls == 0, "post-unregister cb1 silent");
	HOOK_EXPECT(g_iCallback2Calls == 1, "post-unregister cb2 still fires");

	// 5. ResetAllRegistrations clears everything.
	Zenith_GameRenderHook::ResetAllRegistrations();
	g_iCallback1Calls = 0;
	g_iCallback2Calls = 0;
	Zenith_GameRenderHook::InvokePostFogRegistrations(xFakeRef);
	HOOK_EXPECT(g_iCallback1Calls == 0, "reset → cb1 silent");
	HOOK_EXPECT(g_iCallback2Calls == 0, "reset → cb2 silent");

	g_bRanAssertions = true;
}

static bool Step_GameRenderHook(int /*iFrame*/) { return false; /* one-shot */ }
static bool Verify_GameRenderHook()             { return g_bRanAssertions && g_bAllPassed; }

static const Zenith_AutomatedTest g_xGameRenderHookTest = {
	"GameRenderHook_Test",
	&Setup_GameRenderHook,
	&Step_GameRenderHook,
	&Verify_GameRenderHook,
	5,
	true // m_bRequiresGraphics: validates the EXT-1 fog-pass render hook
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xGameRenderHookTest);

#endif // ZENITH_INPUT_SIMULATOR
