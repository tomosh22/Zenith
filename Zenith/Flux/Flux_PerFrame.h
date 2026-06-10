#pragma once

#include "Core/ZenithConfig.h"

// The Flux_PerFrame static-facade class has been replaced by member
// methods on Flux_RendererImpl. Migrate:
//   Flux_PerFrame::X(...)                  → g_xEngine.FluxRenderer().X(...)
//   Flux_PerFrame::Initialise()            → g_xEngine.FluxRenderer().PerFrameInitialise()
//   Flux_PerFrame::Shutdown()              → g_xEngine.FluxRenderer().PerFrameShutdown()
//   Flux_PerFrame::OnFrameBeginFunc        → Flux_RendererImpl::OnFrameBeginFunc
//   Flux_PerFrame::OnFrameEndFunc          → Flux_RendererImpl::OnFrameEndFunc
// The monotonic frame counter moved further: it now lives on FrameContext —
//   read via g_xEngine.Frame().GetFrameIndex() / GetRingIndex().
//
// This header is kept so existing `#include "Flux/Flux_PerFrame.h"`
// statements stay valid during the Phase 2.5 transition — the file may
// be deleted in a later cleanup pass once those includes are scrubbed.
