#pragma once

#ifdef ZENITH_TOOLS

#include "DebugVariables/Zenith_DebugVariables.h"

// Phase 5.7: per-Engine debug-variable tree. Moves the s_xTree static off
// Zenith_DebugVariables (where its leaves stored function-pointer callbacks
// pointing into other now-engine-owned subsystem state) onto an engine
// member so the tree's lifetime brackets the lifetime of every subsystem
// whose state its leaves point at. Zenith_DebugVariables's static facade
// keeps its method surface; the 19 inline Add* methods route through a
// single non-inline helper (AddLeafNodeToEngineTree) defined in the .cpp.
class Zenith_DebugVariablesImpl
{
public:
	Zenith_DebugVariablesImpl() = default;
	~Zenith_DebugVariablesImpl() = default;

	Zenith_DebugVariablesImpl(const Zenith_DebugVariablesImpl&) = delete;
	Zenith_DebugVariablesImpl& operator=(const Zenith_DebugVariablesImpl&) = delete;

	Zenith_DebugVariableTree m_xTree;
};

#endif // ZENITH_TOOLS
