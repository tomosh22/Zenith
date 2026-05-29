#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_RenderGraph;

// ---------------------------------------------------------------------------
// Flux_ScreenSpaceEffectBase — CRTP base shared by the screen-space effect
// subsystems (SSR, SSGI). Captures the ~95% of state and lifecycle that is
// byte-identical between them: the graph pointer, the initialised flag, the
// four shader+pipeline quads, and the shared Shutdown skeleton.
//
// NO virtuals (workstream mandate). The pieces that diverge between effects
// are reached through static dispatch: static_cast<TDerived*>(this)->XxxImpl().
// Per-effect members (SSR's aux/confidence handles + CBV, SSGI's resolution
// divisor) stay in the derived classes — this base deliberately does NOT
// unify them.
// ---------------------------------------------------------------------------
template<typename TDerived>
class Flux_ScreenSpaceEffectBase
{
public:
	bool IsInitialised() const { return m_bInitialised; }

	// Shared teardown skeleton. The derived supplies ShutdownImpl() for its own
	// resources (SSR destroys its CBV; SSGI has none) — reached via static
	// dispatch so the base needs no vtable.
	void Shutdown()
	{
		if (!m_bInitialised)
			return;

		static_cast<TDerived*>(this)->ShutdownImpl();

		m_pxGraph = nullptr;
		m_bInitialised = false;
	}

	// Shared state. Public so the existing g_xEngine.SSR()/SSGI() call sites and
	// the debug-texture / hot-reload blocks reach these members unchanged after
	// they move up off the derived classes.
	Flux_RenderGraph* m_pxGraph      = nullptr;
	bool              m_bInitialised = false;

	Flux_Shader   m_xRayMarchShader;
	Flux_Shader   m_xUpsampleShader;
	Flux_Shader   m_xDenoiseHShader;
	Flux_Shader   m_xDenoiseVShader;
	Flux_Pipeline m_xRayMarchPipeline;
	Flux_Pipeline m_xUpsamplePipeline;
	Flux_Pipeline m_xDenoiseHPipeline;
	Flux_Pipeline m_xDenoiseVPipeline;
};

// ---------------------------------------------------------------------------
// Flux_CommittedHandleSelector — generic carrier for the committed-transient /
// RequestGraphRebuild protocol that SSR and SSGI both follow.
//
// At the tail of SetupRenderGraph the effect commits which transient the
// deferred pass will read (Commit). On a later frame, if the live selection
// diverges from the one used to build the graph, the effect must request a
// full graph rebuild (RequestRebuildIfSelectionChanged returns true).
//
// TSelection is the key to respecting the divergence between effects:
//   - SSR instantiates with a plain bool (the roughness-blur toggle).
//   - SSGI instantiates with a tiny POD { bool m_bDenoise; u_int m_uDivisor; }
//     so the SAME selector covers SSGI's denoise toggle AND its resolution
//     divisor in one comparison.
//
// Pure value comparison — std::function-free.
// ---------------------------------------------------------------------------
template<typename TSelection>
class Flux_CommittedHandleSelector
{
public:
	// Seed the committed handle from the live selection at SetupRenderGraph exit.
	// Returns the committed handle for convenience.
	Flux_TransientHandle Commit(Flux_TransientHandle xWhenEnabled, Flux_TransientHandle xWhenDisabled, bool bEnabled, const TSelection& xSelection)
	{
		m_xLastSelection   = xSelection;
		m_xCommittedHandle = bEnabled ? xWhenEnabled : xWhenDisabled;
		return m_xCommittedHandle;
	}

	// True when the live selection differs from the one the graph was built
	// with — the caller must then g_xEngine.FluxRenderer().RequestGraphRebuild().
	bool RequestRebuildIfSelectionChanged(const TSelection& xSelection) const
	{
		return !(xSelection == m_xLastSelection);
	}

	Flux_TransientHandle GetCommittedHandle() const { return m_xCommittedHandle; }

private:
	Flux_TransientHandle m_xCommittedHandle;
	TSelection           m_xLastSelection{};
};
