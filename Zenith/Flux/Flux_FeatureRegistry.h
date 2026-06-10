#pragma once

#include "Core/Zenith.h"			// u_int (Core/Zenith.h:18) + engine prelude; ZenithConfig.h alone does not define u_int
#include "Core/ZenithConfig.h"

// ---------------------------------------------------------------------------
// Flux_FeatureRegistry
//
// The single table of renderer features Flux_RendererImpl walks for init /
// render-graph setup / shutdown. The default set lives in
// Flux_FeatureRegistry.cpp::RegisterDefaultFeatures(). It must NOT pull
// EntityComponent in: the layering gate forbids any Flux -> EntityComponent
// include, so this header includes ONLY Core/Flux types and forward-declares
// Flux_RenderGraph.
//
// ORDERING. Init order and Shutdown order are DEPENDENCY-LOAD-BEARING — they
// encode real subsystem dependencies (see the comment block in
// Flux_RendererImpl::LateInitialise); a reorder is a latent boot/teardown
// crash. The SetupRenderGraph walk is a SINGLE ordered list (no discrete
// phases): the render graph COMPUTES pass execution order by topologically
// sorting each pass's declared Reads/Writes. That sort is SEEDED by the walk's
// declaration order in two load-bearing ways, so the walk order is NOT free-form:
//   1. PRODUCERS MUST BE DECLARED BEFORE CONSUMERS. A reader is linked only to a
//      writer of the SAME resource declared EARLIER in the walk
//      (Flux_RenderGraph::FindBestWriter requires uBestWriter < uReader).
//      Declaring a consumer step before its producer silently drops the
//      dependency edge AND the barrier the graph would synthesise — and
//      ValidateOrphanedReads will NOT catch it (a writer exists, just later).
//   2. Multiple writers of the same resource execute in declaration order (the
//      write-after-write chain). This is the tiebreak the graph falls back to
//      when two passes write a resource with no dependency between them.
// Passes touching disjoint resources are order-independent.
// ---------------------------------------------------------------------------

class Flux_RenderGraph;

// Upper bound on the number of registered features. ~25 today; 40 leaves slack
// without making the fixed array wasteful. A runtime Zenith_Assert in Register()
// trips if the registration list ever exceeds this (count is a runtime value).
static constexpr u_int FLUX_MAX_FEATURES = 40;

// One step in the single ordered SetupRenderGraph walk. Most steps are a
// feature's SetupRenderGraph trampoline; a few are "irregulars" — the
// FluxGraphics/HDR transient creation, the Skybox aerial-perspective pass, and
// the final-RT layout-transition pass — which are not plain feature setups but
// share the void(Flux_RenderGraph&) signature. They are ordinary ordered steps
// in the one walk. See the ORDERING note above.
struct Flux_SetupStep
{
	const char* m_szName              = nullptr;
	void      (*m_pfnSetup)(Flux_RenderGraph&) = nullptr;
};

// Setup walk holds every feature's setup trampoline (~25) plus the 5 irregular
// steps, so it can exceed the feature count. +8 leaves slack; AddToSetupWalk /
// AddSetupStep assert against this bound at runtime.
static constexpr u_int FLUX_MAX_SETUP_STEPS = FLUX_MAX_FEATURES + 8;

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
	// ORDER, then record the setup walk and the shutdown order. IDEMPOTENT:
	// re-entry resets the table first, so headless boots that skip
	// LateInitialise and unit tests that re-init Flux in-process can call this
	// repeatedly.
	static void RegisterDefaultFeatures();

	// Drop every registration (count -> 0). Used by RegisterDefaultFeatures for
	// idempotency and available to tests.
	void Reset();

	// Append a feature in INIT order. Asserts on overflow / duplicate name.
	void Register(const char* szName,
		void (*pfnInitialise)(),
		void (*pfnSetupRenderGraph)(Flux_RenderGraph&),
		void (*pfnShutdown)());

	// Append the named feature's SetupRenderGraph trampoline to the single
	// ordered setup walk. Call order defines the walk (= declaration) order.
	// Asserts the name is a registered feature with a SetupRenderGraph
	// trampoline — a feature with none (e.g. DynamicLights) must NOT be added.
	void AddToSetupWalk(const char* szFeatureName);

	// Append a raw named setup step — an "irregular" that is not a registered
	// feature's setup (FluxGraphics/HDR transient creation, Skybox aerial
	// perspective, the final-RT layout-transition pass).
	void AddSetupStep(const char* szName, void (*pfnSetup)(Flux_RenderGraph&));

	// Record that the named feature participates in the registry-driven
	// Shutdown walk. Call order defines shutdown order. (FluxGraphics / HDR /
	// Gizmos shut down INLINE in Flux_RendererImpl::Shutdown and are
	// deliberately NOT added here — see the .cpp.)
	void AddToShutdownWalk(const char* szFeatureName);

	// Init-order feature view. The init walk iterates [0, GetNumFeatures()) and
	// calls m_pfnInitialise where non-null.
	const Flux_FeatureDesc* GetFeatures() const { return m_axFeatures; }
	u_int GetNumFeatures() const { return m_uNumFeatures; }

	// Run the single ordered setup walk: invoke each step's setup fn in walk
	// order. Every step's fn is non-null (asserted at append time). Between steps
	// it tags pass ownership and interleaves any game render features anchored
	// after each step (see Zenith_GameRenderFeatures).
	void RunSetup(Flux_RenderGraph& xGraph) const;

	// True if a setup step with this exact name exists in the walk. Used to
	// validate game-feature runAfter anchors (Zenith_GameRenderFeatures). O(n)
	// strcmp scan; names are static-lifetime literals.
	bool HasSetupStepNamed(const char* szName) const;

	// Walk the explicit shutdown order, invoking m_pfnShutdown where non-null.
	void RunShutdown() const;

private:
	Flux_FeatureRegistry() = default;

	// Resolve a registered feature's index by name. Asserts on miss, so a typo
	// in an AddToSetupWalk / AddToShutdownWalk name fails loudly at boot.
	u_int FindFeatureIndex(const char* szName) const;

	Flux_FeatureDesc m_axFeatures[FLUX_MAX_FEATURES];
	u_int            m_uNumFeatures = 0;

	// Single ordered setup walk: feature setup trampolines + irregular steps, in
	// the order RunSetup invokes them. This IS the declaration order the render
	// graph falls back to when ordering writers of the same resource.
	Flux_SetupStep m_axSetupSteps[FLUX_MAX_SETUP_STEPS];
	u_int          m_uNumSetup = 0;

	// Shutdown order: indices into m_axFeatures, in shutdown sequence.
	u_int m_auShutdownOrder[FLUX_MAX_FEATURES];
	u_int m_uNumShutdown = 0;
};
