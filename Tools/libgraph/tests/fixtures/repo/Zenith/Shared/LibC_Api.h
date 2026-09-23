#pragma once

// Scenario 14: LibC declares LibB (see Sharpmake_LibC.cs), and this header
// includes LibB's entry header -- so LibC -> LibB is declared+used. LibC_Api.h
// itself is also the far end of scenario 13 (LibA's test-only reach) and
// scenario 20's neighbour namespace (unrelated to the Middleware duplicate).
#include "Shared/LibB_Api.h"

void LibC_Api_Touch();
