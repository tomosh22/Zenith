#pragma once

#include <cstdint>

// Shared CPU/GPU terrain constants layout.  Flux verifies this byte count
// against reflected shader data; components use it only to allocate storage.
inline constexpr uint32_t uZENITH_TERRAIN_CONSTANTS_BUFFER_BYTES = 64u;
