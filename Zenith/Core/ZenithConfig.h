#pragma once

/**
 * ZenithConfig.h - Central Engine Configuration
 *
 * This header provides a single source of truth for engine-wide configuration constants.
 * These values affect core engine behavior and MUST be kept consistent across all systems.
 *
 * For terrain-specific configuration, see: Flux/Terrain/Flux_TerrainConfig.h
 *
 * Configuration Categories:
 *   1. Frame Pipelining - GPU/CPU synchronization
 *   2. Threading - Worker thread counts
 *   3. Asset Limits - Maximum pool sizes
 *   4. Vertex Formats - Mesh data layouts
 */

#include <cstdint>

namespace ZenithConfig {

// ============================================================================
// FRAME PIPELINING
// ============================================================================
// Controls how many frames can be in-flight simultaneously for CPU/GPU overlap.
//
// Value: 2 = Double buffering (CPU prepares frame N+1 while GPU renders frame N)
// Value: 3 = Triple buffering (more latency, smoother framerate)
//
// CRITICAL: Changing this affects:
//   - Vulkan swapchain image count
//   - Command buffer allocation counts
//   - Descriptor pool sizing
//   - Deferred deletion frame counts
//   - Dynamic buffer per-frame allocations
//
// Files using this constant:
//   - Zenith/Vulkan/Zenith_Vulkan_Swapchain.cpp
//   - Zenith/Vulkan/Zenith_Vulkan_MemoryManager.cpp
//   - Zenith/Vulkan/Zenith_Vulkan_CommandBuffer.cpp
//   - Zenith/Flux/Flux_Buffers.h
//   - Zenith/Editor/Zenith_Editor.cpp

static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;


// ============================================================================
// THREADING
// ============================================================================
// Number of worker threads for parallel command buffer recording.
//
// These threads are used by Flux to record Vulkan commands in parallel.
// Separate from Jolt Physics threads (which use hardware_concurrency - 1).
//
// CRITICAL: Changing this affects:
//   - Vulkan command pool allocation (one pool per worker per frame)
//   - Descriptor pool allocation (one pool per worker)
//   - Flux render task distribution
//
// Recommended: Match to (physical_cores - 2) for best balance
//              Reserve 1 for main thread, 1 for OS/background
//
// Files using this constant:
//   - Zenith/Vulkan/Zenith_Vulkan.cpp
//   - Zenith/Vulkan/Zenith_Vulkan_CommandBuffer.cpp
//   - Zenith/Flux/Flux.cpp
//   - Zenith/Flux/Flux.h

static constexpr uint32_t FLUX_NUM_WORKER_THREADS = 8;


// ============================================================================
// ASSET LIMITS
// ============================================================================
// Maximum number of assets that can be loaded simultaneously.
// These are used by Zenith_AssetRegistry for resource tracking.
//
// Memory impact (approximate):
//   - Each texture slot: ~16 bytes metadata (not including GPU memory)
//   - Each mesh slot: ~64 bytes metadata (not including GPU memory)
//   - Each material slot: ~128 bytes

static constexpr uint32_t MAX_TEXTURES = 1024;
static constexpr uint32_t MAX_MESHES = 16384 * 2;  // 32768
static constexpr uint32_t MAX_MATERIALS = 1024;


// ============================================================================
// VULKAN LIMITS
// ============================================================================
// Maximum render targets and descriptor bindings per shader.

static constexpr uint32_t FLUX_MAX_TARGETS = 8;
static constexpr uint32_t FLUX_MAX_DESCRIPTOR_BINDINGS = 32;
static constexpr uint32_t FLUX_MAX_DESCRIPTOR_SET_LAYOUTS = 5;


// ============================================================================
// VERTEX FORMATS
// ============================================================================
// Standard vertex stride for static meshes.
// Layout: Position(12) + UV(8) + Normal(12) + Tangent(12) + Bitangent(12) + Color(4) = 60 bytes
//
// CRITICAL: This MUST match the vertex layout in:
//   - Asset export tools (mesh converter)
//   - Shader input layouts
//   - Terrain vertex format (see Flux_TerrainConfig.h)
//
// Changing this value requires updating ALL mesh files and shaders.

static constexpr uint32_t STATIC_MESH_VERTEX_STRIDE = 60;


// ============================================================================
// GIZMO CONFIGURATION
// ============================================================================
// Editor gizmo rendering and interaction constants.
//
// GIZMO_AUTO_SCALE_DISTANCE: Distance at which gizmo appears at 1:1 scale
// GIZMO_ARROW_LENGTH: Length of translation arrows in local space
// GIZMO_INTERACTION_MULTIPLIER: Hit detection extends this far beyond visual
//                               (Required for clicking thin arrows from oblique angles)

static constexpr float GIZMO_AUTO_SCALE_DISTANCE = 50.0f;
static constexpr float GIZMO_ARROW_LENGTH = 1.2f;
static constexpr float GIZMO_INTERACTION_LENGTH_MULTIPLIER = 10.0f;


// ============================================================================
// PHYSICS CONFIGURATION
// ============================================================================
// Physics mesh generation quality levels.
//
// LOW: AABB bounding box only (fastest, least accurate)
// MEDIUM: Convex hull approximation (good balance)
// HIGH: Decimated mesh (slowest, most accurate)

enum class PhysicsMeshQuality : uint32_t
{
    LOW = 0,     // AABB only
    MEDIUM = 1,  // Convex hull
    HIGH = 2     // Decimated mesh
};

static constexpr PhysicsMeshQuality DEFAULT_PHYSICS_MESH_QUALITY = PhysicsMeshQuality::HIGH;


} // namespace ZenithConfig

// ============================================================================
// LEGACY MACROS (For backward compatibility)
// ============================================================================
// These macros maintain compatibility with code using the old defines.
// New code should use ZenithConfig:: namespace instead.

#ifndef MAX_FRAMES_IN_FLIGHT
#define MAX_FRAMES_IN_FLIGHT ZenithConfig::MAX_FRAMES_IN_FLIGHT
#endif

#ifndef FLUX_NUM_WORKER_THREADS
#define FLUX_NUM_WORKER_THREADS ZenithConfig::FLUX_NUM_WORKER_THREADS
#endif

#ifndef FLUX_MAX_TARGETS
#define FLUX_MAX_TARGETS ZenithConfig::FLUX_MAX_TARGETS
#endif

#ifndef FLUX_MAX_DESCRIPTOR_BINDINGS
#define FLUX_MAX_DESCRIPTOR_BINDINGS ZenithConfig::FLUX_MAX_DESCRIPTOR_BINDINGS
#endif

#ifndef FLUX_MAX_DESCRIPTOR_SET_LAYOUTS
#define FLUX_MAX_DESCRIPTOR_SET_LAYOUTS ZenithConfig::FLUX_MAX_DESCRIPTOR_SET_LAYOUTS
#endif
