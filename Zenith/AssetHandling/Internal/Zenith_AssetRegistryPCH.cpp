// Precompiled-header CREATE translation unit for ZenithAssetRegistry (the
// lower, Flux-free half of the Zenith/AssetHandling split — Phase 4i; see
// Build/Sharpmake_ZenithAssetRegistry.cs).
//
// AGDE/clang precompiles the ENTIRE PrecompSource via `-xc++-header`, so a
// definition-bearing create TU bakes its own symbols into the binary .pch and
// every consumer that /Yu's against it re-emits them -- hundreds of duplicate
// symbols at the game link. This file must therefore stay definition-free:
// nothing but the PCH master header. Mirrors its sibling
// Internal/Zenith_AssetHandlingPCH.cpp (which ZenithAssetHandling still owns --
// each of the two projects sharing this source root owns exactly one create TU,
// and excludes the other's from its own membership).
#include "Zenith.h"
