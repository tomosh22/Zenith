#pragma once

// ============================================================================
// Zenith_VertexAttributeTypes
//
// The vertex-attribute type tag. It names a GPU input format, but it is ALSO
// part of the serialized mesh format: a .zmesh stores one element descriptor
// per vertex attribute, each carrying this enum's value, so any code that
// READS a baked mesh has to name these tags without being renderer code.
// Owning it here is what lets an asset-side reader validate a file's element
// table without taking an edge into Flux.
//
// Flux/Flux_Enums.h includes this header, so every existing consumer that
// reaches ShaderDataType through the Flux enum header is unaffected.
// ============================================================================

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
