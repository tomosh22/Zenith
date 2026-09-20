// Precompiled-header CREATE translation unit for ZenithEntityComponent.
//
// AGDE/clang precompiles the ENTIRE PrecompSource via `-xc++-header`, so a
// definition-bearing create TU bakes its own symbols into the binary .pch and
// every consumer that /Yu's against it re-emits them -- hundreds of duplicate
// symbols at the game link. This file must therefore stay definition-free:
// nothing but the PCH master header. Mirrors Zenith/Flux/Internal/Zenith_FluxPCH.cpp
// and Zenith/AssetHandling/Internal/Zenith_AssetHandlingPCH.cpp.
#include "Zenith.h"
