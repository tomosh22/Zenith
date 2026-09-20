// Precompiled-header CREATE translation unit for ZenithWindows.
//
// AGDE/clang precompiles the ENTIRE PrecompSource via `-xc++-header`, so a
// definition-bearing create TU bakes its own symbols into the binary .pch and
// every consumer that /Yu's against it re-emits them -- hundreds of duplicate
// symbols at the game link. This file must therefore stay definition-free:
// nothing but the PCH master header. (ZenithWindows itself only ever builds
// win64 -- see Sharpmake_ZenithWindows.cs -- so no AGDE config ever compiles
// this file, but it stays definition-free anyway to mirror every other
// per-lib PCH create TU and keep the rule uniform, not file-specific.)
// Mirrors Zenith/Physics/Internal/Zenith_PhysicsPCH.cpp and
// Zenith/EngineComposition/Zenith_EnginePCH.cpp.
#include "Zenith.h"
