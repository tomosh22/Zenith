#pragma once

// Internal header shared across the Zenith_Vulkan_MemoryManager translation
// units (_Aliasing / _Buffers / _Textures / _Views / _Registries + the core
// file). Callers OUTSIDE the MemoryManager implementation must not include
// this header. Additions must stay minimal — the goal is shared state between
// sibling TUs, not a parallel public API; anything a caller outside the
// MemoryManager would ever need belongs in Zenith_Vulkan_MemoryManager.h.

#include "Collections/Zenith_HashMap.h"

// Cache entry used by Zenith_Vulkan_MemoryManager::ProbeImageMemoryRequirements.
struct ProbeCacheEntry
{
	u_int64 m_ulSize      = 0;
	u_int64 m_ulAlignment = 0;
};

// The probe cache lives as a function-local static in the _Aliasing TU (its
// owner); the core TU's Initialise/Shutdown clear it through this accessor.
Zenith_HashMap<u_int64, ProbeCacheEntry>& Zenith_VulkanMemory_ProbeCache();
