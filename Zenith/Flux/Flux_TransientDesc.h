#pragma once

// Render-graph transient resource descriptors + the opaque, generation-stamped
// handles (transient resource / pass) the graph hands back. Split out of Flux.h.
#include "Flux/Flux_Enums.h"   // TextureFormat, TextureType

// ---- Transient resource descriptors (render-graph-owned) ----------------
// Subsystems declare these instead of allocating their own Flux_RenderAttachment.
// The graph allocates the backing resource at Compile() time and may alias
// non-overlapping lifetimes in a future version.
//
// #TODO: Add Flux_TransientBufferDesc + Flux_RenderGraph::CreateTransientBuffer
// once a concrete use case emerges. The graph already emits buffer barriers
// (see Flux_RenderGraph::SynthesizeBarriers), so the missing piece is just
// graph-owned buffer allocation + aliasing pools. Audit existing
// Flux_ReadWriteBuffer / Flux_IndirectBuffer usage first — most of today's
// instances (HDR histogram, instance-group transforms, particle ping-pong
// buffers, etc.) carry data ACROSS frames and are NOT transient candidates.
// CBVs are explicitly out of scope: the per-frame scratch UBO path covers
// that case already and aliasing tiny constant buffers wins nothing.

struct Flux_TransientTextureDesc
{
	u_int m_uWidth = 0;
	u_int m_uHeight = 0;
	u_int m_uDepth = 1;       // >1 for 3D textures
	TextureFormat m_eFormat = TEXTURE_FORMAT_RGBA8_SRGB;
	TextureType m_eTextureType = TEXTURE_TYPE_2D;
	u_int m_uNumMips = 1;
	// Array layers. >1 makes an array texture (e.g. the 4-cascade CSM depth array,
	// sampled as Sampler2DArray). Only the 2D depth-stencil path honors this today;
	// cubes set their own 6 layers via m_eTextureType == TEXTURE_TYPE_CUBE.
	u_int m_uNumLayers = 1;
	u_int m_uMemoryFlags = 0; // MemoryFlags bitmask (e.g., 1u << MEMORY_FLAGS__SHADER_READ)
	bool m_bIsDepthStencil = false; // true → uses BuildDepthStencil instead of BuildColour
};

struct Flux_TransientHandle
{
	u_int m_uIndex = UINT32_MAX;
	// Generation of the Flux_RenderGraph that issued this handle. Compared in
	// ReadTransient / WriteTransient / GetTransientAttachment and asserts if
	// the graph has been Clear()'d / rebuilt since — catches "cached handle
	// from last compile" bugs that would otherwise silently reference a
	// deallocated transient slot or the wrong slot after re-ordering.
	u_int m_uGeneration = 0;
	// Per-graph-instance ID (monotonic counter on Flux_RenderGraph). Ensures
	// two graphs that happen to share (index, generation) issue handles that
	// still compare unequal — matters for unit tests and any future path that
	// holds multiple graphs live concurrently. 0 is the "no graph" sentinel.
	u_int m_uGraphInstanceID = 0;
	bool IsValid() const { return m_uIndex != UINT32_MAX; }
	// Equality is by index + generation + graph-instance. Handles from
	// different graphs or different generations of the same graph never compare
	// equal even if they share an index.
	bool operator==(const Flux_TransientHandle& rhs) const { return m_uIndex == rhs.m_uIndex && m_uGeneration == rhs.m_uGeneration && m_uGraphInstanceID == rhs.m_uGraphInstanceID; }
	bool operator!=(const Flux_TransientHandle& rhs) const { return !(*this == rhs); }
};

// Opaque, type-safe pass handle. Replaces the raw `u_int uPassIndex` that
// Flux_RenderGraph::AddPass used to return. A Flux_PassHandle carries the
// graph's generation counter so that passing a stale handle (e.g. one saved
// across a graph rebuild on window resize) trips an assertion instead of
// silently addressing a different pass.
struct Flux_PassHandle
{
	u_int m_uIndex = UINT32_MAX;
	u_int m_uGeneration = 0;
	// Per-graph-instance ID (see Flux_TransientHandle::m_uGraphInstanceID).
	u_int m_uGraphInstanceID = 0;
	bool IsValid() const { return m_uIndex != UINT32_MAX; }
	// Equality is by index + generation + graph-instance. Two handles with the
	// same index but different generations (e.g. one from before Clear(), one
	// from after) are NOT equal; neither are handles from different graphs.
	bool operator==(const Flux_PassHandle& rhs) const { return m_uIndex == rhs.m_uIndex && m_uGeneration == rhs.m_uGeneration && m_uGraphInstanceID == rhs.m_uGraphInstanceID; }
	bool operator!=(const Flux_PassHandle& rhs) const { return !(*this == rhs); }
};
