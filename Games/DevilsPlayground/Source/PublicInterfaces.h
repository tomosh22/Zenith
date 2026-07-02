#pragma once

// PublicInterfaces.h — thin convenience header. Each DP_* namespace now
// owns its own header under Source/, but this aggregator stays for
// back-compat with the ~105 existing includers + as the single
// "everything DP gameplay-side" surface. Newer code should prefer the
// per-namespace headers (DP_Player.h, DP_Items.h, …) so the include
// graph stays tight.

#include "DPCommonTypes.h"

#include "DP_Player.h"
#include "DP_Items.h"
#include "DP_Interactables.h"
#include "DP_AI.h"
#include "DP_Fog.h"
#include "DP_Win.h"
#include "DP_Night.h"
#include "DP_Query.h"
#include "DP_Knots.h"
#include "DP_MetaSave.h"
