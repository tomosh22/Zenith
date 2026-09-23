// Scenario 26 (pch-only). MidLib declares no dependency on LibA (see
// Sharpmake_MidLib.cs -- still zero AddPublicDependency calls), and this is
// its ONLY include -- "FxPch.h" is physically Zenith/LibA/FxPch.h, LibA's
// own file and the model's pch_root. MidLib's only reach to LibA is
// therefore the PCH header itself: base class `pch`, attributed to LibA
// (the pch_root owner). Middleware edges ARE classified (only game edges
// are exempt), so this is a real MidLib -> LibA pch edge, not a game one.
#include "FxPch.h"

void MidLib_Init()
{
}

// Scenario 27 (link-only). MidLib calls a LibB function through an extern
// declaration, with NO LibB include anywhere in this file, and MidLib
// declares no dependency on LibB either. Base class: no include at all, but
// a link symbol -> link-only. LibB_Widget_Poke() is defined out of line in
// Zenith/Shared/LibB_Impl.cpp, with no LibB header declaring it (mirrors
// scenario 18's LibC_ExportAll shape). dumpbin: MidLib.txt UNDEFs
// LibB_Widget_Poke; LibB.txt owns it.
extern void LibB_Widget_Poke();

void MidLib_PokeLibB()
{
	LibB_Widget_Poke();
}
