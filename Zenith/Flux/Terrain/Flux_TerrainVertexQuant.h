#pragma once

#include "Core/Zenith_TerrainChunkLayout.h"
#include "Core/Zenith_TerrainDimensions.h"
#include "Flux/Flux_VertexCodec.h"

#include <cstring>   // std::memcpy — the packed lanes are not naturally aligned

// ============================================================================
// Flux_TerrainVertexQuant — the bridge between the baked-chunk FILE FORMAT
// (Core/Zenith_TerrainChunkLayout.h: which bytes, which box) and the codec that
// owns the bit layouts (Flux/Flux_VertexCodec.h).
//
// It exists because FIVE producers write terrain vertex bytes — the offline
// exporter, the editor's live sculpt hook, CityBuilder's road carve and its
// stream-in hook, and the runtime chunk validator that reads them back — and
// each one used to spell the position/UV offsets and the float layout itself.
// A quantised position cannot be written that way at all: it needs the box, and
// five copies of a box is five chances for a chunk to decode somewhere else.
//
// Core cannot host this (it would have to name a Flux type) and the codec must
// not (it knows nothing about terrain), so the join lives here, next to the
// terrain system both halves serve.
//
// ALIGNMENT: the 20-byte stride puts every odd vertex's 8-byte position lane on
// a 4-byte boundary, so every access below goes through memcpy rather than a
// reinterpret_cast store.
// ============================================================================

// The authored box a terrain's baked positions are quantised against. Built
// from that terrain's OWN dimensions rather than measured, so a chunk exported
// today and one exported after any future re-bake at the same dimensions decode
// identically (see the box's note in Zenith_TerrainChunkLayout).
//
// PER-TERRAIN, PER-AXIS. The box is the terrain's world extent, so a narrow
// terrain spends its snorm16 precision on the space it actually occupies -- a
// 256m-wide Route1 gets 6x the X precision a 1536m-wide one would. It is still
// shared by every chunk of ONE terrain, which is the property that keeps seams
// closed: a stitch vertex duplicated into two neighbours quantises to the same
// word in both only because both used the same box. A per-chunk fit would open
// a crack along every border.
//
// Y remains the global [0, MAX_TERRAIN_HEIGHT] lid: the exporter's heights are
// a normalised heightmap sample scaled by that one constant, and nothing in
// this change makes height per-terrain.
inline Flux_PosQuant Flux_MakeTerrainPosQuant(const Zenith_TerrainDimensions& xDims)
{
	return Flux_MakePosQuant(
		Zenith_Maths::Vector3(
			Zenith_TerrainChunkLayout::afPOSITION_BOX_MIN[0],
			Zenith_TerrainChunkLayout::afPOSITION_BOX_MIN[1],
			Zenith_TerrainChunkLayout::afPOSITION_BOX_MIN[2]),
		Zenith_Maths::Vector3(
			Zenith_TerrainChunkLayout::afPOSITION_BOX_MIN[0] + xDims.WorldSizeX(),
			Zenith_TerrainChunkLayout::afPOSITION_BOX_MAX[1],
			Zenith_TerrainChunkLayout::afPOSITION_BOX_MIN[2] + xDims.WorldSizeZ()));
}

// The extent a terrain's UV lane is normalised against: the SQUARE authoring
// domain, so both axes share one scale and a UV read back is world metres in
// both. At default dimensions this is 4096, exactly the constant it replaces.
inline float Flux_TerrainUVBoxMax(const Zenith_TerrainDimensions& xDims)
{
	return xDims.MaxWorldSize();
}

// The w lane is the packer's canonical POSITION pad (a homogeneous point); no
// terrain shader reads it.
inline void Flux_WriteTerrainVertexPosition(u_int8* pVertex,
	const Zenith_Maths::Vector3& xPosition, const Flux_PosQuant& xQuant)
{
	const u_int64 ulPacked = Flux_PackSnorm16x4(xPosition, 1.0f, xQuant);
	std::memcpy(pVertex + Zenith_TerrainChunkLayout::uPOSITION_OFFSET, &ulPacked, sizeof(ulPacked));
}

inline Zenith_Maths::Vector3 Flux_ReadTerrainVertexPosition(const u_int8* pVertex,
	const Flux_PosQuant& xQuant)
{
	u_int64 ulPacked = 0;
	std::memcpy(&ulPacked, pVertex + Zenith_TerrainChunkLayout::uPOSITION_OFFSET, sizeof(ulPacked));
	return Flux_UnpackSnorm16x4Position(ulPacked, xQuant);
}

// UVs are AUTHORED WORLD XZ IN METRES, in and out; the normalisation by the
// terrain's square authoring domain is this pair's business and nobody else's.
//
// They used to be "global heightmap pixel coordinates", which was the SAME
// NUMBER only because every terrain was 4096m wide baked from a 4096px image.
// Reading them as metres is what lets a material's world-space tiling stay
// correct at any terrain size without re-tuning: the shader multiplies the
// dequantised lane by the box it was packed against.
inline void Flux_WriteTerrainVertexUV(u_int8* pVertex, const Zenith_Maths::Vector2& xUV, float fUVBoxMax)
{
	const u_int uPacked = Flux_PackUnorm16x2(xUV / fUVBoxMax);
	std::memcpy(pVertex + Zenith_TerrainChunkLayout::uUV_OFFSET, &uPacked, sizeof(uPacked));
}

inline Zenith_Maths::Vector2 Flux_ReadTerrainVertexUV(const u_int8* pVertex, float fUVBoxMax)
{
	u_int uPacked = 0;
	std::memcpy(&uPacked, pVertex + Zenith_TerrainChunkLayout::uUV_OFFSET, sizeof(uPacked));
	return Flux_UnpackUnorm16x2(uPacked) * fUVBoxMax;
}

// Normal and tangent stay SNORM10 words, unchanged by the compression flip — the
// offset accessors exist so no caller has to know where they moved to.
inline void Flux_WriteTerrainVertexNormalWord(u_int8* pVertex, u_int uPacked)
{
	std::memcpy(pVertex + Zenith_TerrainChunkLayout::uNORMAL_OFFSET, &uPacked, sizeof(uPacked));
}

inline void Flux_WriteTerrainVertexTangentWord(u_int8* pVertex, u_int uPacked)
{
	std::memcpy(pVertex + Zenith_TerrainChunkLayout::uTANGENT_OFFSET, &uPacked, sizeof(uPacked));
}
