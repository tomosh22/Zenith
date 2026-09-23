#include "FxPch.h"

// Scenario 25: reachable both via the PCH and directly. FxPch.h already
// pulls this header in (scenario 1: FxPch.h -> "MidLib/MidLib.h"), but LibB
// also declares MidLib directly (see Sharpmake_LibB.cs) and includes it here
// too -- so LibB -> MidLib stays declared+used via this direct include, and
// closure_no_pch("Zenith/Shared/LibB_Impl.cpp") must still contain
// Middleware/MidLib/MidLib.h even with the PCH-only reach subtracted out.
#include "MidLib/MidLib.h"

// Scenario 14: LibB does NOT declare LibC (see Sharpmake_LibB.cs), so this
// include makes LibB -> LibC used-undeclared. LibC -> LibB is also declared
// (via LibC_Api.h), so the pair forms a 2-node cycle; this edge is also
// allowlisted via Tools/layering_allowlist.txt's plain-format line
// "Zenith/Shared/LibB_Impl.cpp => Shared/LibC_Api.h".
#include "Shared/LibC_Api.h"

// Scenario 15: a conditional include. The rule: an include nested in any
// #if/#ifdef/#ifndef block, except a top-of-file #ifndef X / #define X
// include guard. This one is a genuine conditional use, not a guard, since
// it isn't the file's own #pragma once/#ifndef-#define pair.
#ifdef FX_TOOLS
#include "LibA/LibA_Extra.h"
#endif

void LibB_Impl_CallsLibC()
{
	LibC_Api_Touch();
}

#ifdef FX_TOOLS
void LibB_Impl_ToolsOnly()
{
	LibA_ExtraThing xExtra;
	(void)xExtra;
}
#endif

// Scenario 27 (link-only): the far end of MidLib.cpp's extern-declared call.
// No LibB header declares this -- MidLib reaches it purely at link time.
void LibB_Widget_Poke()
{
}
