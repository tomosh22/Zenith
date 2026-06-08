#pragma once

#include "Core/Zenith.h"			// u_int (Core/Zenith.h:18) + engine prelude; ZenithConfig.h alone does not define u_int
#include "Core/ZenithConfig.h"

// ---------------------------------------------------------------------------
// Flux_FeatureRegistry (Wave-13.B)
//
// Collapses the THREE hand-maintained ordered feature lists that used to live
// inline in Flux.cpp — the LateInitialise init block, the SetupRenderGraph
// block, and the reverse-order Shutdown block — into one table that
// Flux_RendererImpl walks. Adding a renderer feature was ~7 scattered edits;
// it is now a handful of co-located Register* lines in
// Flux_FeatureRegistry.cpp::RegisterDefaultFeatures() (plus the matching
// golden-order entries the debug assert validates).
//
// This is a PURE internal refactor. The external API and every g_xEngine
// accessor are unchanged, and the change is trivially revertible (delete these
// two files and restore the inline lists). It must NOT pull EntityComponent
// in: the WS8.7 layering gate (CI-enforced as of WS13.A) forbids any
// Flux -> EntityComponent include. This header therefore includes ONLY
// Core/Flux types and forward-declares Flux_RenderGraph.
//
// ORDERING IS DEPENDENCY-LOAD-BEARING. Init order, the four SetupRenderGraph
// sub-walks, and the Shutdown order each encode real subsystem dependencies
// (see the comment block in Flux_RendererImpl::LateInitialise). A reorder is a
// latent boot/render crash, so RegisterDefaultFeatures() runs a debug
// VerifyOrder() that asserts the emitted sequences against hardcoded golden
// arrays transcribed from the pre-refactor Flux.cpp. A reorder fires at boot.
// ---------------------------------------------------------------------------

class Flux_RenderGraph;

// Upper bound on the number of registered features. ~25 today; 40 leaves slack
// without making the fixed array wasteful. A runtime Zenith_Assert in Register()
// trips if the registration list ever exceeds this (count is a runtime value).
static constexpr u_int FLUX_MAX_FEATURES = 40;

// Which SetupRenderGraph sub-walk a feature belongs to. The full setup order
// INTERLEAVES irregular cross-cuts (FluxGraphics/HDR SetupTransients, Skybox
// aerial perspective, the post-fog game hook, HDR's second SetupRenderGraph
// touch, the final-RT layout-transition pass) that are NOT plain feature
// SetupRenderGraph calls, so a single monolithic walk cannot reproduce it.
// Flux_RendererImpl::SetupRenderGraph runs these four sub-walks in order,
// emitting the inline irregulars between them. Within a phase, features run in
// registration (setup-order) sequence.
enum Flux_FeatureSetupPhase : u_int
{
	// Preprocessing -> geometry (G-buffer writers) -> decals -> screen-space
	// effects -> clustering + deferred lighting. Everything up to and including
	// DeferredShading, before the aerial-perspective irregular.
	FLUX_SETUP_PHASE_PREPASS_TO_LIGHTING = 0,
	// SSAO + Fog. Runs after the Skybox aerial-perspective irregular and before
	// the post-fog game hook irregular.
	FLUX_SETUP_PHASE_SSAO_FOG,
	// SDFs + Particles + HDR (HDR's SECOND touch — bloom/tonemap composite).
	// Runs after the post-fog game hook.
	FLUX_SETUP_PHASE_POST_PROCESS,
	// UI quads + text + (tools) gizmos. Runs before the final-RT irregular.
	FLUX_SETUP_PHASE_UI,

	FLUX_SETUP_PHASE_COUNT,
	// Sentinel for features that do NOT participate in any setup sub-walk (none
	// today, but keeps the contract explicit).
	FLUX_SETUP_PHASE_NONE = 0xFFFFFFFFu,
};

// One registered renderer feature. Every function pointer is NULLABLE; a null
// pointer means "this feature does not participate in that phase". Captureless
// free-function trampolines only (no std::function) — see the .cpp.
struct Flux_FeatureDesc
{
	const char*  m_szName              = nullptr;
	void       (*m_pfnInitialise)()                      = nullptr;
	void       (*m_pfnSetupRenderGraph)(Flux_RenderGraph&) = nullptr;
	void       (*m_pfnShutdown)()                        = nullptr;
};

// Fixed-size, registration-ordered feature table. Single instance, reached via
// Get(). NOT a constinit global with non-trivial members — it is a function-
// local static (see Get()), so it carries no static-init-order hazard and is
// trivially reset for the idempotent re-registration path.
class Flux_FeatureRegistry
{
public:
	// The one registry instance. Function-local static => constructed on first
	// use, zero static-init cost.
	static Flux_FeatureRegistry& Get();

	// Populate the registry with the engine's default feature set, IN INIT
	// ORDER, then record the setup-order sub-walk membership and the shutdown
	// order. IDEMPOTENT: re-entry resets the table first, so headless boots that
	// skip LateInitialise and unit tests that re-init Flux in-process can call
	// this repeatedly. Runs VerifyOrder() in debug builds.
	static void RegisterDefaultFeatures();

	// Drop every registration (count -> 0). Used by RegisterDefaultFeatures for
	// idempotency and available to tests.
	void Reset();

	// Append a feature in INIT order. Asserts on overflow / duplicate name.
	// Returns the index of the just-added feature so the caller can wire its
	// setup-phase / shutdown participation by index without a name lookup.
	u_int Register(const char* szName,
		void (*pfnInitialise)(),
		void (*pfnSetupRenderGraph)(Flux_RenderGraph&),
		void (*pfnShutdown)());

	// Record that the feature at uFeatureIndex participates in setup sub-walk
	// ePhase. Call order across features defines the within-phase setup order.
	void AddToSetupWalk(u_int uFeatureIndex, Flux_FeatureSetupPhase ePhase);

	// Record that the feature at uFeatureIndex participates in the registry-
	// driven Shutdown walk. Call order defines shutdown order. (FluxGraphics /
	// HDR / Gizmos shut down INLINE in Flux_RendererImpl::Shutdown and are
	// deliberately NOT added here — see the .cpp.)
	void AddToShutdownWalk(u_int uFeatureIndex);

	// W6.1 (declarative lifecycle ordering): declare that the feature at
	// uFeatureIndex must INITIALISE AFTER the feature named szDependsOnName — a real
	// init dependency (uFeatureIndex's Initialise trampoline consumes that feature
	// via g_xEngine, so it must already be brought up). These declarations form a
	// dependency graph over the init order; VerifyInitDependencies() then asserts the
	// hand-maintained init order is a valid topological order of it. The runtime init
	// order is UNCHANGED (still registration order) — this only makes a future reorder
	// that puts a feature before a dependency it consumes a STRUCTURAL boot error
	// (caught by the dependency graph), not just a snapshot mismatch against the golden
	// array (which a reorder + golden-update would silently pass).
	void DeclareInitDependsOn(u_int uFeatureIndex, const char* szDependsOnName);

	// Init-order feature view. The init walk iterates [0, GetNumFeatures()) and
	// calls m_pfnInitialise where non-null.
	const Flux_FeatureDesc* GetFeatures() const { return m_axFeatures; }
	u_int GetNumFeatures() const { return m_uNumFeatures; }

	// Run one SetupRenderGraph sub-walk: invoke m_pfnSetupRenderGraph for every
	// feature tagged with ePhase, in recorded setup order, skipping nulls.
	void RunSetupPhase(Flux_RenderGraph& xGraph, Flux_FeatureSetupPhase ePhase) const;

	// Walk the explicit shutdown order, invoking m_pfnShutdown where non-null.
	void RunShutdown() const;

#ifdef ZENITH_RUNTIME_CHECKS
	// Lifecycle backstop (W6.2): verify the emitted init-name sequence, each setup
	// sub-walk name sequence, and the shutdown-name sequence EXACTLY equal the golden
	// arrays transcribed from the pre-refactor Flux.cpp. Called at the tail of
	// RegisterDefaultFeatures. A reorder is caught here at boot (via Zenith_Check, so
	// it now SURVIVES Release builds — not just Debug) rather than as a subtle
	// render/teardown corruption later.
	void VerifyOrder() const;

	// W6.1: verify the init order is a valid topological order of the declared init
	// dependency graph (DeclareInitDependsOn) — every feature initialises strictly
	// after every feature it depends on. Catches a dependency-violating reorder
	// (e.g. DeferredShading before HDR) structurally, even if the golden array was
	// updated to match the bad order. Release-survivable (Zenith_Check).
	void VerifyInitDependencies() const;
#endif

private:
	Flux_FeatureRegistry() = default;

	Flux_FeatureDesc m_axFeatures[FLUX_MAX_FEATURES];
	u_int            m_uNumFeatures = 0;

	// Setup order: indices into m_axFeatures, in setup sequence, each tagged with
	// its sub-walk phase. RunSetupPhase filters this by phase preserving order.
	u_int                  m_auSetupOrder[FLUX_MAX_FEATURES];
	Flux_FeatureSetupPhase m_aeSetupPhase[FLUX_MAX_FEATURES];
	u_int                  m_uNumSetup = 0;

	// Shutdown order: indices into m_axFeatures, in shutdown sequence.
	u_int m_auShutdownOrder[FLUX_MAX_FEATURES];
	u_int m_uNumShutdown = 0;

	// W6.1: declared init dependency edges — m_auDepFeature[i] must init AFTER the
	// feature named m_aszDepName[i]. Verified by VerifyInitDependencies. Sized for
	// up to 4 deps per feature (the heaviest, DeferredShading, declares 7; ~16 total).
	u_int       m_auDepFeature[FLUX_MAX_FEATURES * 4];
	const char* m_aszDepName[FLUX_MAX_FEATURES * 4];
	u_int       m_uNumInitDeps = 0;
};
