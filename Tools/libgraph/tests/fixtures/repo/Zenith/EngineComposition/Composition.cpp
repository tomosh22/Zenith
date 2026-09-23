#include "FxPch.h"

// Scenario 3: an encapsulation break -- Composition.cpp (owned by the
// "Zenith" aggregate node) reaches into LibA's own Internal/ header
// directly. Allowlisted via Tools/architecture_allowlist.txt's
// "encapsulation:" line.
#include "LibA/Internal/LibA_Detail.h"

// Scenario 18: link-only. No LibC header is included; LibC_ExportAll() is
// declared extern and called directly, resolving only at link time against
// LibC's own out-of-line definition (Zenith/Shared/LibC_Impl.cpp).
extern void LibC_ExportAll();

void Composition_Run()
{
	LibA_DetailState xState;
	xState.m_iInternalOnly = 1;

	LibC_ExportAll();
}
