// Definition-free precompiled-header create TU for the aggregate Zenith engine
// lib on AGDE/Android (clang).
//
// WHY THIS EXISTS: clang builds a PCH by precompiling the ENTIRE file passed to
// -xc++-header. The engine lib historically used Zenith_Core.cpp as its
// PrecompSource, but that file DEFINES functions (Zenith_Core::UpdateTimers /
// Zenith_MainLoop / ...) and #includes EngineComposition/Zenith_UnitTests.Tests.inl.
// Under clang those definitions get baked into Zenith.h.pch, and every TU that uses
// the PCH then re-emits them -> hundreds of duplicate-symbol errors when a game
// .so links the engine archive. MSVC's /Yc only captures the header prefix (up to
// the matching #include), so Windows is unaffected and keeps Zenith_Core.cpp.
//
// This file lives in Zenith/EngineComposition/ alongside the other composition
// translation units (Zenith_Core.cpp, Zenith_Engine.cpp, ...); a later phase
// re-roots the aggregate engine project's PrecompSource lookup here.
//
// This TU contains NOTHING but the master-header include, so the AGDE PCH carries
// only Zenith.h's declarations -- no engine definitions leak into every TU.
#include "Zenith.h"
