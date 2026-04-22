#pragma once

// Internal header shared across the Zenith_Vulkan_MemoryManager translation
// units. Scaffolding for the future _Aliasing.cpp / _Buffers.cpp / _Textures.cpp
// / _Views.cpp / _Registries.cpp split — callers OUTSIDE the MemoryManager
// implementation must not include this header.
//
// Today this header is a single shared struct; during the T2.1 decomposition
// it will grow to hold:
//   - helpers currently static inside Zenith_Vulkan_MemoryManager.cpp
//     (BuildAliasedImageCreateInfo, MakeProbeSignature) promoted to
//     external linkage so the Aliasing.cpp sibling can call them
//   - any other state that has to cross the split boundary
//
// Additions must stay minimal — the goal is shared state between sibling TUs,
// not a parallel public API. Anything a caller outside the MemoryManager
// would ever need belongs in the public Zenith_Vulkan_MemoryManager.h.

// Cache entry used by Zenith_Vulkan_MemoryManager::ProbeImageMemoryRequirements.
// The cache itself (s_xProbeCache) is a file-local static in the aliasing-
// owning TU; this struct lives here so a future sibling TU that reads or
// warms the cache has a shared declaration.
struct ProbeCacheEntry
{
	u_int64 m_ulSize      = 0;
	u_int64 m_ulAlignment = 0;
};
