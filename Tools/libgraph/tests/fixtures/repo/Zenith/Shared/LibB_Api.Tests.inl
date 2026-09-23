// Scenario 24: test include from an ordinary TU (dst is the .Tests.inl).
// Included directly from Zenith/LibA/LibA.cpp -- LibA has no other include
// of any LibB file, so this is LibA's ONLY path to LibB, and it arrives
// through a *.Tests.inl (this file itself, not one LibA owns). The touched
// LibB_Widget construction is header-only (no out-of-line call), so the
// link side stays empty on purpose -- LibA.txt must carry no UNDEF
// resolving to a LibB-owned symbol, keeping the LibA->LibB edge test-only
// at both the include and the link level.
#include "LibB_Api.h"

static void LibB_Api_Tests_Touch()
{
	LibB_Widget xWidget;
	(void)xWidget;
}
