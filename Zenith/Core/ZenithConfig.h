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

#ifdef ZENITH_ANDROID
static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 4; // Android surfaces require >= 4 swapchain images
#else
static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
#endif


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
static constexpr uint32_t FLUX_MAX_MIPS = 12;     // Maximum mip levels (supports up to 4096x4096)
// Maximum array layers per attachment. Cubes use 6, 2D array textures use up
// to this. The render graph's barrier key (Flux_BarrierKey) packs the layer
// index into 16 bits, so this constant must stay <= 0xFFFF; the static_assert
// next to MakeBarrierKey enforces that pin at compile time.
static constexpr uint32_t FLUX_MAX_LAYERS = 256;
// Max array layers a single Flux_RenderAttachment stores per-layer DSVs for.
// Only array depth attachments (CSM = 4 cascades) use these; kept small because
// every Flux_RenderAttachment carries the per-layer DSV array inline. Bump if a
// future array attachment needs more layers (the builder asserts the bound).
static constexpr uint32_t FLUX_MAX_ATTACHMENT_LAYERS = 8;
static constexpr uint32_t FLUX_MAX_BINDINGS_PER_GROUP = 32;
// Binding-model spine: set 0 GLOBAL, 1 VIEW, 2 BINDLESS, 3 PASS, 4 DRAW, 5 reserved (future TLAS).
static constexpr uint32_t FLUX_MAX_BINDING_GROUPS = 6;
// Max specialization constants a single pipeline spec / shader reflection carries
// (Flux Shader System Overhaul — Stage 3a). Spec constants occupy no descriptor
// binding; this bounds the fixed-size Flux_SpecConstantTable and the backend's
// per-pipeline VkSpecializationInfo scratch arrays.
static constexpr uint32_t FLUX_MAX_SPEC_CONSTANTS = 8;

// Bindless combined-image-sampler table (set 2 = BINDLESS, g_axTextures[]).
// TARGET is the desired capacity; the runtime size is clamped to the device's
// update-after-bind descriptor limits at boot (Zenith_Vulkan::QueryDescriptorIndexingLimits)
// and stored in m_uBindlessTableSize. MIN is the min-spec floor the boot asserts —
// a device that cannot host at least this many is rejected (no non-bindless fallback).
static constexpr uint32_t FLUX_BINDLESS_TABLE_SIZE_TARGET = 16384;
static constexpr uint32_t FLUX_BINDLESS_TABLE_SIZE_MIN    = 1000;

// Static mesh vertex stride. The engine does not use this constant directly;
// the authoritative layout (pos12 + uv8 + normal12 + tangent12 + bitangent12 +
// colour4 = 60 bytes) lives in the mesh converter tool's source. Changing the
// vertex layout requires touching the converter, shader input layouts, and
// the terrain vertex format (see Flux_TerrainConfig.h) in lockstep.


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

#ifndef FLUX_MAX_BINDINGS_PER_GROUP
#define FLUX_MAX_BINDINGS_PER_GROUP ZenithConfig::FLUX_MAX_BINDINGS_PER_GROUP
#endif

#ifndef FLUX_MAX_BINDING_GROUPS
#define FLUX_MAX_BINDING_GROUPS ZenithConfig::FLUX_MAX_BINDING_GROUPS
#endif

#ifndef FLUX_MAX_SPEC_CONSTANTS
#define FLUX_MAX_SPEC_CONSTANTS ZenithConfig::FLUX_MAX_SPEC_CONSTANTS
#endif

#ifndef FLUX_BINDLESS_TABLE_SIZE_TARGET
#define FLUX_BINDLESS_TABLE_SIZE_TARGET ZenithConfig::FLUX_BINDLESS_TABLE_SIZE_TARGET
#endif

#ifndef FLUX_BINDLESS_TABLE_SIZE_MIN
#define FLUX_BINDLESS_TABLE_SIZE_MIN ZenithConfig::FLUX_BINDLESS_TABLE_SIZE_MIN
#endif

#ifndef FLUX_MAX_MIPS
#define FLUX_MAX_MIPS ZenithConfig::FLUX_MAX_MIPS
#endif

#ifndef FLUX_MAX_LAYERS
#define FLUX_MAX_LAYERS ZenithConfig::FLUX_MAX_LAYERS
#endif

#ifndef FLUX_MAX_ATTACHMENT_LAYERS
#define FLUX_MAX_ATTACHMENT_LAYERS ZenithConfig::FLUX_MAX_ATTACHMENT_LAYERS
#endif
