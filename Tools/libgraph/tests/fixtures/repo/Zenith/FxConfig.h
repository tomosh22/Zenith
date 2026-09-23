#pragma once

// Scenario 16: ambiguous include, candidate #1. Both this file and
// Zenith/LibA/FxConfig.h exist and are listed by LibA's vcxproj; both
// "Zenith" and "Zenith/LibA" are include dirs, so a plain
// #include "FxConfig.h" from outside either directory could resolve to
// either one. MSVC's quoted-include search order picks the first matching
// include dir, so this one (the "Zenith" root, which precedes "Zenith/LibA"
// in every fixture project's AdditionalIncludeDirectories) wins -- but the
// self-check should still flag it as ambiguous (W2a's concern; this file's
// only job is to exist and be listed).
#define FX_CONFIG_ROOT 1
