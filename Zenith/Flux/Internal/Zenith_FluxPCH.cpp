// Precompiled-header CREATE translation unit for ZenithFlux.
//
// AGDE/clang precompiles the ENTIRE PrecompSource via `-xc++-header`, so a
// definition-bearing create TU bakes its own symbols into the binary .pch and
// every consumer that /Yu's against it re-emits them -- hundreds of duplicate
// symbols at the game link. This file must therefore stay definition-free:
// nothing but the PCH master header. Mirrors Zenith/Scripting/Internal/Zenith_ScriptingPCH.cpp
// and Zenith/Physics/Internal/Zenith_PhysicsPCH.cpp.
#include "Zenith.h"
