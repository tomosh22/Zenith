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
// ONE CALL PER FEATURE. RegisterDefaultFeatures() adds every feature with a
// single RegisterFeature<&Zenith_Engine::X>(reg, "X") call, written in the order
// passes are DECLARED to the render graph. That one source order drives all three
// walks automatically — adding a feature is exactly one call placed at the right
// spot:
//   * Init     — features in registration order (forward). Dependency-safe in ANY
//                order beyond "FluxGraphics first": every subsystem's Initialise /
//                Shutdown touches only foundation (FluxGraphics, memory, backend) +
//                its own state, never another feature (verified), so the render-
//                graph declaration order is also a valid init/teardown order.
//   * Setup    — RegisterFeature auto-appends the feature's SetupRenderGraph step
//                at the call site (via AddSetupStep). RegisterDefaultFeatures is
//                now ALL features — no hand-written irregular steps remain (the
//                former transient-creation + final-RT-layout-transition steps are
//                now FluxGraphics / Flux_Present SetupRenderGraph methods).
//                This declaration order is LOAD-BEARING — the graph topologically
//                sorts passes by declared Reads/Writes, SEEDED by declaration order
//                two ways:
//                  1. PRODUCERS BEFORE CONSUMERS — a reader links only to a writer
//                     declared EARLIER (FindBestWriter requires writer < reader); a
//                     consumer-before-producer slip silently drops the edge AND the
//                     barrier, now caught at Compile by
//                     Flux_RenderGraph::ValidateProducerBeforeConsumer (a Zenith_Check).
//                  2. Multiple writers of one resource run in declaration order
//                     (the write-after-write tiebreak).
//   * Shutdown — features in REVERSE registration order (auto-derived by
//                RunShutdown). Safe in any order for the same reason init is.
// Passes touching disjoint resources are order-independent.
// ---------------------------------------------------------------------------

class Flux_RenderGraph;
struct Flux_ShaderDecl; // per-feature shader decls (Flux/<Feature>/Flux_<Feature>_Shaders.h)

// Upper bound on the number of registered features. ~25 today; 40 leaves slack
// without making the fixed array wasteful. A runtime Zenith_Assert in Register()
// trips if the registration list ever exceeds this (count is a runtime value).
static constexpr u_int FLUX_MAX_FEATURES = 40;

// One step in the single ordered SetupRenderGraph walk. Every step is a feature's
// SetupRenderGraph trampoline, auto-appended by RegisterFeature via AddSetupStep.
// AddSetupStep stays a public primitive (used by Register; also callable directly
// by out-of-tree owners such as a game's own pass), but RegisterDefaultFeatures no
// longer hand-writes any irregular steps. See the ORDERING note.
struct Flux_SetupStep
{
	const char* m_szName              = nullptr;
	void      (*m_pfnSetup)(Flux_RenderGraph&) = nullptr;
};

// Setup walk holds every feature's setup trampoline (~25) plus the handful of
// irregular steps, so it can exceed the feature count. +8 leaves slack; Register
// (auto-append) and AddSetupStep assert against this bound at runtime.
static constexpr u_int FLUX_MAX_SETUP_STEPS = FLUX_MAX_FEATURES + 8;

// One registered renderer feature. With the uniform mandatory interface (see
// FluxRenderFeature) RegisterFeature wires ALL FOUR trampolines for every feature —
// a feature with nothing to do in a phase supplies a no-op method, so these are
// non-null in practice. Captureless free-function trampolines only (no
// std::function) — see the .cpp. (The fields default to null only so the struct is
// trivially zero-constructible for Reset.)
struct Flux_FeatureDesc
{
	const char*  m_szName              = nullptr;
	void       (*m_pfnInitialise)()                      = nullptr;
	void       (*m_pfnSetupRenderGraph)(Flux_RenderGraph&) = nullptr;
	void       (*m_pfnShutdown)()                        = nullptr;
	// Rebuild this feature's GPU pipelines from its (now-recompiled) shaders — what
	// the Slang hot-reload watcher fires when a .slang owned by the feature changes
	// (see Flux_ShaderHotReload::AutoRegisterFeatures). A no-op for features that own
	// no pipelines (FluxGraphics, DynamicLights, Shadows — Shadows reuses the mesh
	// subsystems' shadow programs); AutoRegisterFeatures only calls it for programs
	// that map to this feature, so those no-ops are never actually invoked.
	void       (*m_pfnBuildPipelines)()                  = nullptr;

	// The shader programs this feature OWNS — the feature's apxALL array (set by
	// RegisterFeature). Drives hot-reload (each owned program's rebuild is this
	// feature's BuildPipelines) and the catalog<->feature parity check
	// (Flux_ShaderCatalog::ValidateFeatureParity). nullptr/0 for features that own
	// no pipelines (FluxGraphics / Shadows / DynamicLights).
	const Flux_ShaderDecl* const* m_paxShaders          = nullptr;
	u_int                         m_uShaderCount         = 0;
};

// The compile-time contract for a Flux render feature — the subsystem type behind a
// Zenith_Engine accessor that RegisterFeature<&Zenith_Engine::X> drives. The
// interface is UNIFORM AND MANDATORY: every feature implements all four lifecycle
// methods — Initialise(), SetupRenderGraph(Flux_RenderGraph&), Shutdown() and
// BuildPipelines() — even if a method is a no-op for that feature (e.g. FluxGraphics
// and DynamicLights have no-op SetupRenderGraph/BuildPipelines; Fog has a no-op
// Shutdown; Shadows a no-op BuildPipelines). RegisterFeature wires all four
// unconditionally and is constrained on this concept, so a render system that is
// missing a method (or mis-spelled one) is a clear compile error at the call site —
// "if a feature lacks a function, add it (a no-op stub if there's nothing to do)".
// (Mirrors the concept-driven backend abstraction in Flux_Backend.h.)
template<typename T>
concept FluxRenderFeature = requires(T& xFeature, Flux_RenderGraph& xGraph)
{
	xFeature.Initialise();
	xFeature.SetupRenderGraph(xGraph);
	xFeature.Shutdown();
	xFeature.BuildPipelines();
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

	// Populate the registry with the engine's default feature set via one
	// RegisterFeature call each, in render-graph declaration order (which also
	// serves as init order; shutdown is the reverse). IDEMPOTENT: re-entry resets
	// the table first, so headless boots that skip LateInitialise and unit tests
	// that re-init Flux in-process can call this repeatedly.
	static void RegisterDefaultFeatures();

	// The engine-startup-free population step: append the default feature set to
	// the given (already-empty) registry. RegisterDefaultFeatures is the engine
	// wrapper that Reset()s the singleton (main-thread assert) then calls this.
	// Split out so tooling (FluxCompiler) can build a snapshot WITHOUT booting the
	// engine — Reset()'s g_xEngine.Threading() main-thread assert would otherwise
	// fire on an uninitialised engine.
	static void RegisterDefaultFeaturesInto(Flux_FeatureRegistry& xReg);

	// Build a fresh, fully-populated registry snapshot by value WITHOUT touching
	// the singleton or g_xEngine (the RegisterFeature trampolines reference
	// g_xEngine only when CALLED, never during registration). The registry is
	// trivially copyable (fixed arrays + counts, no owning pointers), so returning
	// by value is faithful. FluxCompiler validates this snapshot's parity against
	// the catalog without engine startup.
	static Flux_FeatureRegistry CreateDefaultSnapshotForValidation();

	// Drop every registration (counts -> 0). Used by RegisterDefaultFeatures for
	// idempotency and available to tests.
	void Reset();

	// Append a feature in registration order. Asserts on overflow / duplicate
	// name. If pfnSetupRenderGraph is non-null the feature is ALSO appended to the
	// setup walk at this position (so one call wires init + setup + shutdown +
	// hot-reload). pfnBuildPipelines is the optional Slang hot-reload rebuild
	// callback (see Flux_FeatureDesc).
	void Register(const char* szName,
		void (*pfnInitialise)(),
		void (*pfnSetupRenderGraph)(Flux_RenderGraph&),
		void (*pfnShutdown)(),
		void (*pfnBuildPipelines)() = nullptr,
		const Flux_ShaderDecl* const* paxShaders = nullptr,
		u_int uShaderCount = 0);

	// Append a raw named setup step — an "irregular" that is not a registered
	// feature's setup (FluxGraphics/HDR transient creation, the final-RT
	// layout-transition pass). Interleave these between
	// RegisterFeature calls to place them in render-graph declaration order.
	void AddSetupStep(const char* szName, void (*pfnSetup)(Flux_RenderGraph&));

	// Init-order feature view. The init walk iterates [0, GetNumFeatures()) and
	// calls m_pfnInitialise where non-null.
	const Flux_FeatureDesc* GetFeatures() const { return m_axFeatures; }
	u_int GetNumFeatures() const { return m_uNumFeatures; }

	// Resolve a registered feature by name, or nullptr if none matches (does NOT
	// assert on miss — the hot-reload auto-wire expects misses for shader programs
	// that no engine feature owns, e.g. Water / ComputeTest).
	const Flux_FeatureDesc* FindFeatureByName(const char* szName) const;

	// Run the single ordered setup walk: invoke each step's setup fn in walk
	// order. Every step's fn is non-null (asserted at append time). Between steps
	// it tags pass ownership and interleaves any game render features anchored
	// after each step (see Zenith_GameRenderFeatures).
	void RunSetup(Flux_RenderGraph& xGraph) const;

	// True if a setup step with this exact name exists in the walk. Used to
	// validate game-feature runAfter anchors (Zenith_GameRenderFeatures). O(n)
	// strcmp scan; names are static-lifetime literals.
	bool HasSetupStepNamed(const char* szName) const;

	// Shut down every feature in REVERSE registration order, invoking m_pfnShutdown
	// where non-null. No separate shutdown list — the reverse of the init order is
	// a correct teardown order (no cross-feature teardown dependencies; see the
	// ORDERING note). The non-feature teardown (Slang / HotReload / ImGui /
	// swapchain / memory) runs after this, inline in Flux_RendererImpl::Shutdown.
	void RunShutdown() const;

private:
	Flux_FeatureRegistry() = default;

	Flux_FeatureDesc m_axFeatures[FLUX_MAX_FEATURES];
	u_int            m_uNumFeatures = 0;

	// Single ordered setup walk: feature setup trampolines (auto-appended by
	// Register) + irregular steps, in the order RunSetup invokes them. This IS the
	// declaration order the render graph falls back to when ordering writers.
	Flux_SetupStep m_axSetupSteps[FLUX_MAX_SETUP_STEPS];
	u_int          m_uNumSetup = 0;
};
