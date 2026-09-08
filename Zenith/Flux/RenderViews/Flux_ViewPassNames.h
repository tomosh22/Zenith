#pragma once

#include "Flux/RenderViews/Flux_RenderViews.h"

// =====================================================================
// Flux_ViewPassNames — per-view-slot render-graph pass names.
//
// A render-graph pass name must be UNIQUE across the graph and must have
// STATIC LIFETIME, because two consumers keep the returned pointer for the
// life of the process rather than copying the bytes:
//   * Flux_RenderGraph.cpp:351 — pxPass->m_szName = szName;
//   * Vulkan/Zenith_Vulkan.cpp:2201 — m_aszGPUTimerNames[uIdx] = szName;
// A per-view pass chain therefore cannot format its names into a stack
// buffer or a Zenith_Vector<std::string>: the pointer would dangle. The
// in-tree idiom is a hand-written static literal table per feature (see
// Flux/Shadows/Flux_Shadows.cpp:179-187, whose comment gives exactly this
// reason: "prevents dangling stack-buffer pointers passed to AddPass"), and
// today HiZ, HDR, SSAO, SSR and SSGI each carry their own second table for
// the preview view. This pool replaces that idiom with ONE interning arena.
//
// CONTRACT — read this before calling:
//   * szBase MUST be a static-lifetime string (a literal). Slot 0 returns
//     szBase ITSELF by pointer, so the graph ends up storing the caller's
//     pointer exactly as it does today. This is the same contract AddPass
//     already imposes (Flux_RenderGraph.h:254-256).
//   * Every returned pointer is stable for the life of the pool. The pool's
//     storage is a FIXED char arena that never reallocates, so a stored
//     pointer never moves.
//   * There is deliberately NO reset/clear. The render graph and the GPU
//     timer table hold pointers INTO this arena, so resetting it would be a
//     use-after-free that no gate could observe. Tests that need a fresh
//     pool construct their own Flux_ViewPassNamePool.
//
// Bases are keyed by string CONTENT (strcmp), never by pointer. MSVC /GF
// pools identical literals within a TU, so a pointer key would look correct
// in a single-TU test and then intern the same base twice across TUs.
// =====================================================================

// Distinct BASE names the pool can hold. 45 distinct bases exist in the tree
// today (HiZ 12, HDR 10, SSAO/SSR/SSGI 4 each, Skybox 2, Decals 2, one each for
// UnifiedMesh / DeferredShading / Fog / Particles / SDFs / Translucency, plus
// the one legacy base below), so 64 leaves headroom without making the arena
// large. Exceeding it asserts and degrades to the base name.
inline constexpr u_int kuFLUX_VIEW_PASS_NAME_MAX_BASES = 64u;

// Maximum bytes (NUL included) of one composed "<base> (<suffix>)" name. The
// longest base in the tree is "HDR_BloomDownsample Mip1" (24) which composes to
// 38 with the longest suffix, so 64 is comfortable. A base that would overflow
// asserts and degrades to the base name.
inline constexpr u_int kuFLUX_VIEW_PASS_NAME_MAX_LEN = 64u;

// Entries are only ever needed for the NON-main slots — slot 0 is returned by
// pointer identity and stores nothing.
inline constexpr u_int kuFLUX_VIEW_PASS_NAME_NUM_SUFFIXED_SLOTS = FLUX_MAX_RENDER_VIEWS - 1u;

// The interning arena. Construct one per owner; the process-global instance
// behind Flux_ViewPassName() below is the one production code should use.
class Flux_ViewPassNamePool
{
public:
	// uCapacityBases caps how many DISTINCT bases may be interned. It exists so
	// a test can construct a small pool and observe the capacity assert without
	// disturbing the global pool; production always wants the default. A value
	// above kuFLUX_VIEW_PASS_NAME_MAX_BASES asserts and clamps (the arena is a
	// fixed array — the capacity can never exceed its extent).
	explicit Flux_ViewPassNamePool(u_int uCapacityBases = kuFLUX_VIEW_PASS_NAME_MAX_BASES);

	// The name for (szBase, uViewSlot). Slot 0 returns szBase by POINTER
	// IDENTITY; every other slot returns an interned, stable, NUL-terminated
	// "<base> (<suffix>)" (or the historical spelling, for the one row in the
	// legacy table). Repeated calls with the same (content, slot) return the
	// SAME pointer. On any degraded path (null/oversized base, slot out of
	// range, capacity exhausted) the call asserts and returns szBase — a
	// duplicate name is a far better shipping outcome than a crash, and the
	// graph's own duplicate-name assert still fires under ZENITH_RUNTIME_CHECKS.
	const char* Name(const char* szBase, u_int uViewSlot);

	u_int GetUsedBaseCount() const { return m_uUsedBases; }
	u_int GetCapacityBases() const { return m_uCapacityBases; }

private:
	// Returns kuInvalidBase on a refusal (see Name's degraded paths).
	static constexpr u_int kuInvalidBase = ~0u;
	u_int FindOrAddBase(const char* szBase);
	// Writes the composed name into pcOut (kuFLUX_VIEW_PASS_NAME_MAX_LEN bytes).
	// Returns false (and asserts) if it would not fit.
	static bool Compose(char* pcOut, const char* szBase, u_int uViewSlot);

	// FIXED, NEVER-REALLOCATING storage. Not a Zenith_Vector / Zenith_HashMap /
	// std::string: consumers keep these pointers forever (see the header note).
	char  m_aacBases[kuFLUX_VIEW_PASS_NAME_MAX_BASES][kuFLUX_VIEW_PASS_NAME_MAX_LEN] = {};
	char  m_aaacNames[kuFLUX_VIEW_PASS_NAME_MAX_BASES][kuFLUX_VIEW_PASS_NAME_NUM_SUFFIXED_SLOTS][kuFLUX_VIEW_PASS_NAME_MAX_LEN] = {};
	u_int m_uCapacityBases = kuFLUX_VIEW_PASS_NAME_MAX_BASES;
	u_int m_uUsedBases     = 0u;
};

// The process-global pool. szBase must be a static-lifetime literal (slot 0 is
// returned by pointer identity — see the contract above).
const char* Flux_ViewPassName(const char* szBase, u_int uViewSlot);

// The per-slot suffix, exposed so a test can build the expected string from the
// SAME table the pool uses instead of transcribing it. Returns nullptr for slot
// 0 (which has no suffix — it is pointer identity) and for an out-of-range slot.
const char* Flux_ViewSlotSuffix(u_int uViewSlot);

// Anchors this TU against /OPT:REF so its ZENITH_TEST registrars survive. Called
// from Flux_RendererImpl::EarlyInitialise(); a declaration alone anchors nothing.
bool Flux_ViewPassNames_ForceLink();
