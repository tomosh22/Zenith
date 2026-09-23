#pragma once

// Scenario 10: the middle link of the parent-include stack.
// "LibB_Sibling.h" resolves to ../LibB_Sibling.h only because the includer's
// own directory (Zenith/Shared/detail) and its own parent stack put
// Zenith/Shared/LibB_Api.h's directory (Zenith/Shared) on the search path --
// no project lists "Zenith/Shared" as an include dir directly.
#include "LibB_Sibling.h"

struct LibB_InnerThing
{
	LibB_SiblingThing m_xSibling{};
};
