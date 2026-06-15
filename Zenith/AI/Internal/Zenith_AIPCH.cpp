// Per-lib PCH-create TU for the ZenithAI leaf library. Mirrors
// ZenithECS/Internal/Zenith_ECS.cpp + Physics/Internal/Zenith_PhysicsPCH.cpp:
// ZenithAI CREATEs its own Zenith.pch from this trivial TU (PrecompiledHeader=Create),
// and every other ZenithAI TU compiles /Yu"Zenith.h" against it.
#include "Zenith.h"
