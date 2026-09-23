#pragma once

// Scenario 17: GameX reaches LibA ONLY via the leaking header (LibB_Api.h),
// from a header, not a .cpp. It holds a LibA_Handle by value and a
// LibB_Widget* by pointer.
#include "Shared/LibB_Api.h"

struct GameX_Component
{
	LibA_Handle m_hHandle = 0;
	LibB_Widget* m_pxWidget = nullptr;
};
