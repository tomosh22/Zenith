// Flux Shader System Overhaul — Stage 3b. View-shading permission mask as two
// specialization constants. Included ONLY by the two lit programs
// (DeferredShading, Translucent_Forward) via a textual #include (no bindings, so
// it does not shift the spine's descriptor-set spaces).
//
// Both default TRUE, so the FULL-mode pipeline behaves EXACTLY as before —
// `PERMITTED && ViewFlagEnabled()` collapses to `ViewFlagEnabled()`. The BASIC-mode
// pipeline bakes FALSE, so the compiler constant-folds the guard and STRIPS the
// CSM / cluster-light clauses entirely for views that carry neither (shadow-
// cascade-derived + the material preview, flags 0b000). g_uViewFlags stays a live
// CB field: the spec constant is a PERMISSION mask ANDed OVER the retained runtime
// flag, so a runtime shadow/cluster toggle still works within the FULL pipeline.
//
// IDs are auto-assigned in declaration order (SHADOWS = 0, CLUSTER = 1); the C++
// side resolves them BY NAME through reflection (Flux_ResolveSpecConstants), so a
// hot-reload that renumbers them stays correct with no rebuild.
#ifndef FLUX_COMMON_VIEWMODES
#define FLUX_COMMON_VIEWMODES

[SpecializationConstant] const bool FLUX_SC_VIEW_SHADOWS_PERMITTED        = true;
[SpecializationConstant] const bool FLUX_SC_VIEW_CLUSTER_LIGHTS_PERMITTED = true;

#endif // FLUX_COMMON_VIEWMODES
