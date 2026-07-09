#pragma once

#include "Scripting/Zenith_GraphBuilder.h"

//------------------------------------------------------------------------------
// Zenith_GraphChain - a node handle returned by the fluent engine-node factories
// (Zenith_EngineGraphBuilder). It carries the just-created node's id and a back
// pointer to the builder, and offers two things:
//
//   * implicit conversion to the node's u_int id, so a factory result drops
//     straight into the existing id/Edge model
//     (const u_int uSwitch = xB.OnCustomEvent(...);  ...Edge(uSwitch, 0, x)); and
//   * .Then()/.ThenPin() linear/branch sugar
//     (xB.OnCustomEvent("Tick").Then(uPre).Then(uSM);).
//
// ONE return type, no return-type overloading. Leaf-safe: it names only the
// Scripting builder + u_int.
//------------------------------------------------------------------------------
class Zenith_GraphChain
{
public:
	Zenith_GraphChain(Zenith_GraphBuilder& xBuilder, u_int uNodeID)
		: m_pxBuilder(&xBuilder)
		, m_uNodeID(uNodeID)
	{
	}

	// The chain IS its anchor node id in every id-taking context.
	operator u_int() const { return m_uNodeID; }
	u_int Id() const { return m_uNodeID; }

	// Pin-0 linear sugar: Chain(anchor, uNext), re-anchored on uNext.
	Zenith_GraphChain Then(u_int uNext) const
	{
		m_pxBuilder->Chain(m_uNodeID, uNext);
		return Zenith_GraphChain(*m_pxBuilder, uNext);
	}

	// Explicit-pin branch sugar: Edge(anchor, uPin, uNext), re-anchored on uNext.
	Zenith_GraphChain ThenPin(u_int uPin, u_int uNext) const
	{
		m_pxBuilder->Edge(m_uNodeID, uPin, uNext);
		return Zenith_GraphChain(*m_pxBuilder, uNext);
	}

private:
	Zenith_GraphBuilder* m_pxBuilder;
	u_int m_uNodeID;
};
