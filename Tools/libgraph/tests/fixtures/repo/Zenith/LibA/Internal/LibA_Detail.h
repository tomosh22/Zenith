#pragma once

// Scenario 3: an encapsulation break. This header lives under LibA's own
// Internal/ (declared-internal, the ZenithECS convention), but is included
// directly by Zenith/EngineComposition/Composition.cpp -- an allowlisted
// seam (Tools/architecture_allowlist.txt: "encapsulation:" kind).
struct LibA_DetailState
{
	int m_iInternalOnly = 0;
};
