// =============================================================================
// Zenith_SceneManagerGuards.h
// -----------------------------------------------------------------------------
// Phase 5e: The real guard types now live as top-level types in
// Zenith_SceneSystemGuards.h. This header just creates nested-type aliases on
// Zenith_SceneManager so existing call sites that resolve names via
// `Zenith_SceneManager::PrefabInstantiationGuard` continue to compile during
// the Phase 5e migration. After Phase 5e codemods the call sites to the
// top-level names, both this header and Zenith_SceneManager itself disappear.
// =============================================================================

using LifecycleDeferralGuard     = Zenith_LifecycleDeferralGuard;
using PrefabInstantiationGuard   = Zenith_PrefabInstantiationGuard;
using SceneUpdateDeferralGuard   = Zenith_SceneUpdateDeferralGuard;
using SceneCreationTargetScope   = Zenith_SceneCreationTargetScope;
