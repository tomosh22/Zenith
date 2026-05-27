#pragma once

// Render targets used to live here. They moved to graph-owned transient
// resources during the render-graph migration. Look for them in:
//   - Flux/Flux_RenderGraph.h        (transient handle API)
//   - Flux/Flux.cpp / SetupRenderGraph()  (per-frame declaration)
//   - Flux/<Subsystem>/<Subsystem>Impl.h   (per-pass owned attachments)
// This header is kept so existing includes don't break; new code should
// not include it directly.
#include "Flux/Flux.h"
