#include "FxPch.h"

#include "LibA.h"
#include "Internal/LibA_Private.h"

void LibA_Thing::Tick()
{
	// Out-of-line member definition (scenario 5).
}

void LibA_Init()
{
	LibA_PrivateState xState;
	xState.m_iCounter++;
}

// Scenario 13: LibA.Tests.inl is included only from this .cpp, and itself
// includes a LibC header -- making LibA -> LibC test-only.
#include "LibA.Tests.inl"

// Scenario 24: LibA's ONLY reach into LibB, and it arrives through a
// *.Tests.inl LibB owns (not one LibA owns) -- LibA -> LibB is test-only
// too, the mirror image of scenario 13's own-side .Tests.inl.
#include "Shared/LibB_Api.Tests.inl"
