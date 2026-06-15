// Per-lib PCH-create TU for the ZenithPhysics leaf library. Mirrors
// ZenithECS/Internal/Zenith_ECS.cpp: ZenithPhysics CREATEs its own Zenith.pch
// from this trivial TU (PrecompiledHeader=Create), and every other ZenithPhysics
// TU compiles /Yu"Zenith.h" against it. The binary .pch cannot be shared across
// projects that compile with different flags, so each lib builds its own.
#include "Zenith.h"
