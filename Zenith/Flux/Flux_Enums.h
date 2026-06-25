#pragma once

// Engine-wide configuration constants are now in ZenithConfig.h
// This file includes them for backward compatibility with existing code
#include "Core/ZenithConfig.h"

enum CommandType
{
	COMMANDTYPE_GRAPHICS,
	COMMANDTYPE_COMPUTE,
	COMMANDTYPE_COPY,
	COMMANDTYPE_PRESENT,
	COMMANDTYPE_MAX
};

enum AllocationType
{
	ALLOCATION_TYPE_BUFFER,
	ALLOCATION_TYPE_TEXTURE,
	ALLOCATION_TYPE_COUNT,
};

enum TextureType
{
	TEXTURE_TYPE_2D,
	TEXTURE_TYPE_3D,
	TEXTURE_TYPE_CUBE
};

enum ResourceAccess
{
	RESOURCE_ACCESS_UNDEFINED,          // No prior state — first touch, contents discardable
	RESOURCE_ACCESS_READ_SRV,           // Sampled read (fragment | compute) — stage discrimination is handled inside the access translator, not the enum.
	RESOURCE_ACCESS_READ_DEPTH,         // Depth attachment read-only
	RESOURCE_ACCESS_WRITE_RTV,          // Color attachment write
	RESOURCE_ACCESS_WRITE_DSV,          // Depth attachment write
	RESOURCE_ACCESS_WRITE_UAV,          // Compute/storage write
	RESOURCE_ACCESS_READWRITE_UAV,      // Compute read-modify-write
	RESOURCE_ACCESS_READ_INDIRECT_ARG,  // Buffer-only: indirect-draw / dispatch-indirect arguments read by the GPU command processor.
	RESOURCE_ACCESS_READ_BUFFER_SRV,    // Buffer-only: read-only StructuredBuffer<T> SSBO (vs. RW counterpart in WRITE_UAV / READWRITE_UAV).
	RESOURCE_ACCESS_HOST_TRANSFER_WRITE, // Buffer-only: synthetic predecessor for a host-issued vkCmdCopyBuffer (staging upload). Lets the next pass that reads the buffer get a TransferWrite@eTransfer → ShaderRead barrier emitted in its prologue. Pushed into the live state map by Flux_RenderGraph::MarkBufferHostWritten — never appears in a pass's read/write list.
};

enum TextureFormat
{
	TEXTURE_FORMAT_NONE,

	TEXTURE_FORMAT_COLOUR_BEGIN,//////////////////////
	TEXTURE_FORMAT_RGB8_UNORM,
	TEXTURE_FORMAT_RGBA8_UNORM,
	TEXTURE_FORMAT_BGRA8_SRGB,
	TEXTURE_FORMAT_RGBA8_SRGB,
	TEXTURE_FORMAT_R16G16B16A16_UNORM,
	TEXTURE_FORMAT_BGRA8_UNORM,
	TEXTURE_FORMAT_R16G16B16A16_SFLOAT,
	TEXTURE_FORMAT_R32G32B32A32_SFLOAT,
	TEXTURE_FORMAT_R32G32B32_SFLOAT,
	TEXTURE_FORMAT_R32G32_SFLOAT,         // 64-bit RG float (for HiZ min-max depth)
	// BC Compressed formats
	TEXTURE_FORMAT_BC1_RGB_UNORM,      // 4 bits/pixel, RGB, no alpha (6:1 compression)
	TEXTURE_FORMAT_BC1_RGBA_UNORM,     // 4 bits/pixel, RGB + 1-bit alpha (6:1 compression)
	TEXTURE_FORMAT_BC3_RGBA_UNORM,     // 8 bits/pixel, RGB + smooth alpha (4:1 compression)
	TEXTURE_FORMAT_BC5_RG_UNORM,       // 8 bits/pixel, RG only, ideal for normal maps (4:1 compression)
	TEXTURE_FORMAT_BC7_RGBA_UNORM,     // 8 bits/pixel, high quality RGBA (4:1 compression)
	// Single-channel formats
	TEXTURE_FORMAT_R8_UNORM,           // 8-bit unsigned normalized (SSAO, masks)
	TEXTURE_FORMAT_R16_UNORM,          // 16-bit unsigned normalized (heightmaps)
	TEXTURE_FORMAT_R32_SFLOAT,         // 32-bit float
	TEXTURE_FORMAT_R16G16_SFLOAT,      // 32-bit RG float (for BRDF LUT, etc.)
	TEXTURE_FORMAT_COLOUR_END,/////////////////////////

	TEXTURE_FORMAT_DEPTH_STENCIL_BEGIN,////////////////
	TEXTURE_FORMAT_D32_SFLOAT,
	TEXTURE_FORMAT_DEPTH_STENCIL_END,//////////////////
};

inline bool IsDepthFormat(TextureFormat eFormat)
{
	return eFormat > TEXTURE_FORMAT_DEPTH_STENCIL_BEGIN && eFormat < TEXTURE_FORMAT_DEPTH_STENCIL_END;
}

enum BlendFactor
{
	BLEND_FACTOR_DISABLED,
	BLEND_FACTOR_ZERO,
	BLEND_FACTOR_SRCALPHA,
	BLEND_FACTOR_ONEMINUSSRCALPHA,
	BLEND_FACTOR_ONE
};

enum DepthCompareFunc
{
	DEPTH_COMPARE_FUNC_DISABLED,
	DEPTH_COMPARE_FUNC_LESSEQUAL,
	DEPTH_COMPARE_FUNC_GREATEREQUAL,
	DEPTH_COMPARE_FUNC_NEVER,
	DEPTH_COMPARE_FUNC_ALWAYS
};

enum MeshTopology
{
	MESH_TOPOLOGY_NONE,
	MESH_TOPOLOGY_TRIANGLES,
	MESH_TOPOLOGY_TRIANGLESTRIPS,
};

enum ShaderDataType
{
	SHADER_DATA_TYPE_FLOAT,
	SHADER_DATA_TYPE_FLOAT2,
	SHADER_DATA_TYPE_FLOAT3,
	SHADER_DATA_TYPE_FLOAT4,
	SHADER_DATA_TYPE_INT,
	SHADER_DATA_TYPE_INT2,
	SHADER_DATA_TYPE_INT3,
	SHADER_DATA_TYPE_INT4,
	SHADER_DATA_TYPE_UINT,
	SHADER_DATA_TYPE_UINT2,
	SHADER_DATA_TYPE_UINT3,
	SHADER_DATA_TYPE_UINT4,
	SHADER_DATA_TYPE_MAT3,
	SHADER_DATA_TYPE_MAT4,
	SHADER_DATA_TYPE_BOOL,
	// Packed vertex attribute types
	SHADER_DATA_TYPE_HALF2,				// float16x2 (4 bytes) - maps to VK_FORMAT_R16G16_SFLOAT
	SHADER_DATA_TYPE_SNORM10_10_10_2,	// A2B10G10R10 signed normalized (4 bytes) - maps to VK_FORMAT_A2B10G10R10_SNORM_PACK32
	SHADER_DATA_TYPE_NONE
};

// Canonical resource taxonomy carried through reflection -> pipeline layout.
// Captures the distinctions Slang reflection exposes (separate vs combined
// texture/sampler, structured-buffer variants, RW textures, unbounded arrays,
// parameter blocks) so layout code and every backend can project the right
// descriptor type. (Lives here, alongside the legacy BindingType it replaces,
// so both Flux_Types.h and the Slang reflection layer can name it.)
enum FluxResourceKind : u_int
{
	FLUX_RESOURCE_KIND_UNKNOWN                  = 0,
	FLUX_RESOURCE_KIND_CONSTANT_BUFFER          = 1,  // cbuffer / uniform block
	FLUX_RESOURCE_KIND_STRUCTURED_BUFFER        = 2,  // StructuredBuffer<T> (read-only SSBO)
	FLUX_RESOURCE_KIND_RW_STRUCTURED_BUFFER     = 3,  // RWStructuredBuffer<T>
	FLUX_RESOURCE_KIND_BYTE_ADDRESS_BUFFER      = 4,
	FLUX_RESOURCE_KIND_RW_BYTE_ADDRESS_BUFFER   = 5,
	FLUX_RESOURCE_KIND_TEXTURE                  = 6,  // Texture2D etc. (separate texture)
	FLUX_RESOURCE_KIND_RW_TEXTURE               = 7,  // RWTexture2D etc. (storage image)
	FLUX_RESOURCE_KIND_SAMPLER                  = 8,  // SamplerState
	FLUX_RESOURCE_KIND_COMBINED_TEXTURE_SAMPLER = 9,  // Sampler2D / sampler2D
	FLUX_RESOURCE_KIND_ACCELERATION_STRUCTURE   = 10,
	FLUX_RESOURCE_KIND_UNBOUNDED_TEXTURE_ARRAY  = 11,
	FLUX_RESOURCE_KIND_PARAMETER_BLOCK          = 12, // ParameterBlock<T>
};

// Backend-neutral descriptor-category predicates over FluxResourceKind. These
// mirror the descriptor-type buckets the layout/descriptor-write paths key on
// (uniform buffer / storage buffer / sampled texture / storage image), matching
// the buckets the legacy BindingType used (all texture-ish kinds share the
// combined-image-sampler descriptor; both structured-buffer variants share the
// storage-buffer descriptor — read/write is disambiguated by the bound view).
inline bool FluxKindIsUniformBuffer(FluxResourceKind e)
{
	return e == FLUX_RESOURCE_KIND_CONSTANT_BUFFER;
}
inline bool FluxKindIsStorageBuffer(FluxResourceKind e)
{
	return e == FLUX_RESOURCE_KIND_STRUCTURED_BUFFER
		|| e == FLUX_RESOURCE_KIND_RW_STRUCTURED_BUFFER
		|| e == FLUX_RESOURCE_KIND_BYTE_ADDRESS_BUFFER
		|| e == FLUX_RESOURCE_KIND_RW_BYTE_ADDRESS_BUFFER;
}
inline bool FluxKindIsSampledTexture(FluxResourceKind e)
{
	return e == FLUX_RESOURCE_KIND_TEXTURE
		|| e == FLUX_RESOURCE_KIND_COMBINED_TEXTURE_SAMPLER
		|| e == FLUX_RESOURCE_KIND_SAMPLER;
}
inline bool FluxKindIsStorageImage(FluxResourceKind e)
{
	return e == FLUX_RESOURCE_KIND_RW_TEXTURE;
}
inline bool FluxKindIsUnboundedArray(FluxResourceKind e)
{
	return e == FLUX_RESOURCE_KIND_UNBOUNDED_TEXTURE_ARRAY;
}

enum LoadAction
{
	LOAD_ACTION_DONTCARE,
	LOAD_ACTION_LOAD,
	LOAD_ACTION_CLEAR
};

enum StoreAction
{
	STORE_ACTION_DONTCARE,
	STORE_ACTION_STORE,
};

enum RenderTargetUsage
{
	RENDER_TARGET_USAGE_RENDERTARGET,
	RENDER_TARGET_USAGE_SHADERREAD,
	RENDER_TARGET_USAGE_PRESENT
};

enum MemoryResidency
{
	MEMORY_RESIDENCY_CPU,
	MEMORY_RESIDENCY_GPU
};

enum BufferUsage
{
	BUFFER_USAGE_VERTEXBUFFER,
	BUFFER_USAGE_INDEXBUFFER
};

enum MRTIndex
{
	MRT_INDEX_DIFFUSE,
	MRT_INDEX_NORMALSAMBIENT,
	MRT_INDEX_MATERIAL,			// R=roughness, G=metallic, B=specular, A=packed flags (shading model + clear coat)
	MRT_INDEX_EMISSIVE,			// RGB=HDR emissive colour, A=clear-coat roughness
	MRT_INDEX_COUNT,
};

enum MemoryFlags : u_int
{
	MEMORY_FLAGS__NONE = 0,
	MEMORY_FLAGS__SHADER_READ,
	MEMORY_FLAGS__UNORDERED_ACCESS,
	MEMORY_FLAGS__VERTEX_BUFFER,
	MEMORY_FLAGS__INDEX_BUFFER,
	MEMORY_FLAGS__INDIRECT_BUFFER,
	MEMORY_FLAGS__BINDLESS,
};

enum CullMode
{
	CULL_MODE_NONE,   // No culling (render both faces)
	CULL_MODE_FRONT,  // Cull front faces
	CULL_MODE_BACK,   // Cull back faces (default)
};
