// Precompiled-header CREATE translation unit for ZenithAndroid.
//
// AGDE/clang precompiles the ENTIRE PrecompSource via `-xc++-header`, so a
// definition-bearing create TU bakes its own symbols into the binary .pch and
// every consumer that /Yu's against it re-emits them -- hundreds of duplicate
// symbols at the game link. This is exactly why Zenith_EnginePCH.cpp exists
// for the aggregate's own agde config, and why this file must stay
// definition-free too: nothing but the PCH master header. Mirrors
// Zenith/Physics/Internal/Zenith_PhysicsPCH.cpp and
// Zenith/EngineComposition/Zenith_EnginePCH.cpp.
#include "Zenith.h"
