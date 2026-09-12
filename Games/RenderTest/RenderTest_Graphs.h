#pragma once

// ============================================================================
// RenderTest_Graphs -- the declaration of the ONE RenderTest builder that was
// still file-static, so a test TU can build the production definition
// in-process (the RenderTest_Tennis.h:97-103 shape, which
// BuildGraph_RenderTestTennisBrain already follows).
//
// Only ONE function lives here. BuildGraph_RenderTestTennisBrain is declared in
// RenderTest_Tennis.h:103 and is deliberately NOT tools-gated -- the
// RT_TennisBrain* contract tests build it in every config. The
// BuildTennisBrain_* helpers stay `static` in RenderTest.cpp: each authors a
// fragment onto a Zenith_EngineGraphBuilder, not a graph, so there is no
// definition to validate on its own.
//
// ★ #ifdef ZENITH_TOOLS, because the DEFINITION is. Unlike the tennis brain,
// BuildGraph_RenderTestPlayerActions sits inside RenderTest.cpp's tools block
// (RenderTest.cpp:1735-2990) with the rest of the authoring, so declaring it
// unconditionally would let a `_False` config compile a caller that cannot
// LINK -- a break the Null_*_True gate never sees.
// ============================================================================

#ifdef ZENITH_TOOLS

class Zenith_GraphBuilder;

// game:Graphs/RenderTest_PlayerActions.bgraph - the discrete PRESS decisions
// (interact / reload / fire / cycle tennis camera), one OnActionPressed source
// each. Continuous holds stay C++.
void BuildGraph_RenderTestPlayerActions(Zenith_GraphBuilder& xBuilder);

#endif // ZENITH_TOOLS
