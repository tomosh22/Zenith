#pragma once

#include <cstddef>

// ============================================================================
// Zenith_Tools_MeshoptDecode -- decoders for the vertex/index codecs and vertex
// filters used by EXT_meshopt_compression, the glTF extension gltfpack emits.
//
// ★★ WHY THIS EXISTS AT ALL: ASSIMP CANNOT READ A gltfpack FILE. A meshopt GLB
// declares a FALLBACK BUFFER -- a buffer entry with a non-zero byteLength, no
// "uri", and an EXT_meshopt_compression.fallback flag -- and Assimp, which does
// not know the extension, rejects the whole file with
//
//     GLTF: buffer with non-zero length missing the "uri" attribute
//
// before a single vertex is read. It is not a degraded import, it is no import,
// so every .glb with this extension needs the streams reconstructed HERE before
// anything downstream (Zenith_Tools_GlbImport) can see geometry.
//
// ★ THE FORMAT IS NOT DOCUMENTED PROSE, IT IS A REFERENCE IMPLEMENTATION, so
// every routine below was validated BYTE-FOR-BYTE against upstream
// meshoptimizer's own decoder on real gltfpack output before being written here.
// Three details were established that way rather than reasoned about, and each
// one silently corrupts a mesh if guessed:
//
//   * the fe<15 triangle path reads vertexfifo[(vo - 1 - fec)], while the
//     codeaux path reads vertexfifo[(vo - feb)] with NO -1. The asymmetry is
//     real; using one form for both decodes ~90% of a mesh correctly and then
//     diverges, which reads as a corrupt tail rather than as a decoder bug.
//   * index codec v1 lowers fecmax to 13, and fec 13/14 mean "last - 1" and
//     "last + 1". v0 has neither.
//   * codetri 0xf0..0xfd index the 16-byte codeaux table in the stream's tail;
//     0xfe reads a codeaux byte from the data stream; 0xff does that AND makes
//     `a` a free delta index rather than a freshly allocated vertex. 0xff is
//     rare -- three occurrences in the 5060-triangle mesh this was written for --
//     so a decoder that ignores it passes casual inspection.
//
// Zenith_Tests_MeshoptDecode pins all of it against golden vectors taken from
// the reference implementation, which is the only reason any of the above can be
// trusted later.
//
// TOOLS-ONLY by placement (Tools/ compiles into ZenithTools) and free of engine
// dependencies -- no Zenith_Vector, no asset types, no logging -- so the unit
// test can drive it on raw byte arrays.
// ============================================================================
namespace Zenith_Tools_MeshoptDecode
{
	// The vertex filters EXT_meshopt_compression may name on a bufferView. Applied
	// AFTER the vertex codec, in place, over the decoded byte stream.
	enum MESHOPT_FILTER
	{
		MESHOPT_FILTER_NONE = 0,
		MESHOPT_FILTER_OCTAHEDRAL,
		MESHOPT_FILTER_QUATERNION,
		MESHOPT_FILTER_EXPONENTIAL,
	};

	// Decode an ATTRIBUTES-mode stream. Writes ulVertexCount * ulVertexSize bytes
	// to pDest. Returns false on a malformed header, an unsupported codec version,
	// or any read that would run past pSrc + ulSrcSize -- never a partial write
	// that the caller could mistake for geometry.
	bool DecodeVertexBuffer(
		void* pDest,
		size_t ulVertexCount,
		size_t ulVertexSize,
		const unsigned char* pSrc,
		size_t ulSrcSize);

	// Decode a TRIANGLES-mode stream into ulIndexCount 32-bit indices. ulIndexCount
	// must be a multiple of 3. Indices are NOT range-checked against a vertex count
	// here (the decoder does not know one); Zenith_Tools_GlbImport does that.
	bool DecodeIndexBuffer(
		unsigned int* puDest,
		size_t ulIndexCount,
		const unsigned char* pSrc,
		size_t ulSrcSize);

	// Apply a vertex filter in place. ulCount is the ELEMENT count and ulStride the
	// bytes per element, matching the bufferView's count/byteStride. A stride the
	// filter does not accept is a no-op returning false rather than a scribble.
	bool ApplyFilter(
		void* pBuffer,
		size_t ulCount,
		size_t ulStride,
		MESHOPT_FILTER eFilter);

	// "EXPONENTIAL" / "OCTAHEDRAL" / "QUATERNION" / absent -> the enum. Returns
	// false for any other spelling, so an extension revision that adds a filter
	// fails loudly instead of importing a scrambled stream.
	bool ParseFilterName(const char* szName, MESHOPT_FILTER& eFilterOut);
}
