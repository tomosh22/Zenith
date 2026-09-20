// Precompiled-header CREATE translation unit for ZenithEditor.
//
// AGDE/clang precompiles the ENTIRE PrecompSource via `-xc++-header`, so a
// definition-bearing create TU bakes its own symbols into the binary .pch and
// every consumer that /Yu's against it re-emits them -- hundreds of duplicate
// symbols at the game link. This file must therefore stay definition-free:
// nothing but the PCH master header. Mirrors Zenith/Flux/Internal/Zenith_FluxPCH.cpp,
// Zenith/UI/Internal/Zenith_UIPCH.cpp and Zenith/Scripting/Internal/Zenith_ScriptingPCH.cpp.
//
// (ZenithEditor never actually generates an agde configuration -- see
// Sharpmake_ZenithEditor.cs's "WIN64 x TOOLS=TRUE ONLY" header-comment
// paragraph -- so the AGDE hazard above cannot fire for this project today.
// The file still follows the same definition-free convention as every other
// leaf's create TU, both for consistency and in case a future phase ever
// widens the target set.)
#include "Zenith.h"
