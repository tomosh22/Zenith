#pragma once

// Zenith_ErrorCode - flat error-code enum shared by the asset-loading boundary
// (and any future subsystem that wants a uniform success/error vocabulary).
//
// Header-only with zero engine dependencies: standalone-includable without
// pulling in Zenith.h. We spell the underlying type as `unsigned int` rather
// than the PCH alias `u_int` precisely so this header does not depend on
// Zenith.h (`u_int` == `unsigned int`; see Zenith.h:24). Style mirrors the flat
// `enum class : uintN` shape of Flux_TerrainStreamInResult
// (Flux/Terrain/Flux_TerrainStreamingManagerImpl.h).
enum class Zenith_ErrorCode : unsigned int
{
	SUCCESS = 0,
	FILE_NOT_FOUND,
	BAD_MAGIC,
	VERSION_MISMATCH,
	CORRUPT_DATA,
	GPU_UPLOAD_FAILED,
	OUT_OF_MEMORY,
	INVALID_ARGUMENT,
};
