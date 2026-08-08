// =====================================================================
// Common/VertexFormats — the vertex-layout AUTHORING vocabulary.
//
// Two things live here, both serving the compressed-vertex work:
//
//   [VtxFmt("<fmt>")]  declares the STORAGE format of a vertex-input field when
//                      it differs from the declared type. Fetch hardware widens
//                      and converts, so a four-byte SNORM10:10:10:2 attribute is
//                      still declared `float4` in the VsIn — without the tag the
//                      generator would infer a sixteen-byte float4 and bake a
//                      wrong stride. Legal strings are exactly the ones
//                      Flux_ParseVertexFormatString accepts
//                      (Flux/Slang/Flux_SlangCompiler.cpp); a typo is a hard
//                      compile error, never a silent fall-back to the declared
//                      type.
//   [PerInstance]      moves a field off the per-vertex stream (binding 0) onto
//                      the per-instance stream (binding 1).
//
// Slang has no built-in vertex-format attribute, so both are declared the way the
// stdlib declares its own: [__AttributeUsage(_AttributeTargets.Var)] on a struct
// whose name ends in "Attribute". THE SUFFIX IS LOAD-BEARING — Slang strips it, so
// `struct VtxFmtAttribute` is what makes `[VtxFmt(...)]` resolve and is also the
// name reflection reports back ("VtxFmt").
//
// TEXTUAL #include (NOT `import`): an attribute must be resolvable by name in the
// scope of the shader that spells it, and an imported module would namespace-wrap
// these declarations out of that scope. Declares no ParameterBlock, so — like
// Common/ViewModes.slang — it cannot shift the spine's set 0/1/2 assignment and may
// be included in any order relative to the other textual includes.
// =====================================================================

#ifndef FLUX_COMMON_VERTEXFORMATS_SLANG
#define FLUX_COMMON_VERTEXFORMATS_SLANG

[__AttributeUsage(_AttributeTargets.Var)]
struct VtxFmtAttribute { string format; };

[__AttributeUsage(_AttributeTargets.Var)]
struct PerInstanceAttribute {};

// The shader-side mirror of Flux_UnpackSnorm16x4Position (Flux/Flux_VertexCodec.h)
// and it must stay EXACTLY that: a snorm16 position decodes on the GPU only if the
// dequant here is the precise inverse of the quantisation an exporter baked, and the
// two live in different languages with no compiler to tie them together.
//
// xFetched arrives already converted by the fetch hardware — a snorm16x4 attribute
// reads as a float4 in [-1,1] — so the whole transform is the affine map back onto
// the authored box: xBias + (n * 0.5 + 0.5) * xScale, where the box is the mesh's
// tight AABB (m_xBias = min, m_xScale = extent).
float3 Flux_DequantPosition(float4 xFetched, float3 xScale, float3 xBias)
{
	return xBias + (xFetched.xyz * 0.5 + 0.5) * xScale;
}

// ---- manual codecs, for the buffers no fetch hardware touches ----------------
// A VERTEX-INPUT attribute is decoded by the fetch unit, so the tags above are all a
// draw needs. The compute-skinning pre-pass has no fetch unit: it reads the packed
// bind-pose pool and writes the packed skinned arena as raw u32 SSBO words (a Slang
// StructuredBuffer<float3> would std430-align each vec3 to 16 B, which is not the
// packed layout), so it packs and unpacks by hand. These are the Slang mirror of
// Flux/Flux_VertexCodec.h and must stay bit-for-bit that: the arena the skinner
// writes is fetched by the SAME pipelines that fetch a CPU-packed static mesh, so
// the two producers have to agree on every bit.
//
// The one licensed divergence is the rounding of an exact .5 tie — GLM rounds away
// from zero, the GPU to even. No byte is ever produced by both sides (the CPU owns
// the mesh streams, the GPU owns the arena), so a tie can only shift one quantisation
// step within a stream, never disagree across one. DECODING, which does cross the
// boundary (the GPU reads a CPU-packed bind-pose pool), is exact on both sides.

// ---- float16 <-> float32, in 32-BIT INTEGER ARITHMETIC ONLY -----------------
// NOT f32tof16 / f16tof32: Slang lowers those onto a 16-bit integer type, which makes
// the module declare the SPIR-V `Int16` capability, and vkCreateShaderModule REJECTS
// that unless VkPhysicalDeviceFeatures::shaderInt16 is enabled — a device feature this
// engine does not require and would have to require on every backend and every Android
// device. The conversions below touch nothing narrower than a uint, so they impose no
// feature at all. Round-to-nearest-EVEN, with subnormals and infinities, so a half is
// the same value here as one the CPU codec produced.

uint Flux_FloatToHalfBits(float fValue)
{
	const uint uBits = asuint(fValue);
	const uint uSign = (uBits >> 16u) & 0x8000u;
	const uint uAbs  = uBits & 0x7FFFFFFFu;

	// Inf / NaN. A NaN keeps a non-zero mantissa so it cannot decode as infinity.
	if (uAbs >= 0x7F800000u)
	{
		return uSign | 0x7C00u | ((uAbs > 0x7F800000u) ? 0x200u : 0u);
	}

	const int  iExponent = int(uAbs >> 23u) - 127;
	const uint uMantissa = uAbs & 0x7FFFFFu;

	if (iExponent > 15)          // past half's largest finite -> infinity
	{
		return uSign | 0x7C00u;
	}
	if (iExponent >= -14)        // normal half: drop 13 mantissa bits
	{
		uint uHalf = uSign | (uint(iExponent + 15) << 10u) | (uMantissa >> 13u);
		const uint uRemainder = uMantissa & 0x1FFFu;
		// The +1 carry propagates out of the mantissa into the exponent field on
		// its own, which is exactly the behaviour a rounding overflow needs.
		if ((uRemainder > 0x1000u) || ((uRemainder == 0x1000u) && ((uMantissa >> 13u) & 1u) != 0u))
		{
			uHalf += 1u;
		}
		return uHalf;
	}
	if (iExponent >= -25)        // subnormal half: value = mantissa * 2^-24
	{
		const uint uFull    = uMantissa | 0x800000u;   // restore the implicit 1
		const uint uShift   = uint(-(iExponent + 1));  // 14..24
		const uint uHalfWay = 1u << (uShift - 1u);
		const uint uRemainder = uFull & ((1u << uShift) - 1u);
		uint uSubnormal = uFull >> uShift;
		if ((uRemainder > uHalfWay) || ((uRemainder == uHalfWay) && ((uSubnormal & 1u) != 0u)))
		{
			uSubnormal += 1u;    // may carry into the smallest normal, which is correct
		}
		return uSign | uSubnormal;
	}
	return uSign;                // underflow -> signed zero
}

float Flux_HalfBitsToFloat(uint uHalf)
{
	const uint uSign     = (uHalf & 0x8000u) << 16u;
	const uint uExponent = (uHalf >> 10u) & 0x1Fu;
	const uint uMantissa = uHalf & 0x3FFu;

	if (uExponent == 0u)
	{
		// Zero or subnormal. mantissa * 2^-24 is exact in float, and a zero
		// mantissa gives a zero whose sign the OR below preserves.
		return asfloat(uSign | asuint(float(uMantissa) * asfloat(0x33800000u)));
	}
	if (uExponent == 31u)
	{
		return asfloat(uSign | 0x7F800000u | (uMantissa << 13u));
	}
	return asfloat(uSign | ((uExponent + 112u) << 23u) | (uMantissa << 13u));   // 127 - 15 == 112
}

uint Flux_PackHalf2(float2 xValue)
{
	return Flux_FloatToHalfBits(xValue.x) | (Flux_FloatToHalfBits(xValue.y) << 16u);
}

float2 Flux_UnpackHalf2(uint uPacked)
{
	return float2(Flux_HalfBitsToFloat(uPacked & 0xFFFFu), Flux_HalfBitsToFloat(uPacked >> 16u));
}

// Lanes 0-1 in .x, lanes 2-3 in .y — the two consecutive u32 words a half4 occupies,
// low lane first, matching glm::packHalf4x16's little-endian u64.
uint2 Flux_PackHalf4(float4 xValue)
{
	return uint2(Flux_PackHalf2(xValue.xy), Flux_PackHalf2(xValue.zw));
}

float4 Flux_UnpackHalf4(uint2 xPacked)
{
	return float4(Flux_UnpackHalf2(xPacked.x), Flux_UnpackHalf2(xPacked.y));
}

uint Flux_PackUnorm8x4(float4 xValue)
{
	uint4 xBytes = uint4(round(saturate(xValue) * 255.0));
	return (xBytes.x & 0xFFu) | ((xBytes.y & 0xFFu) << 8u) | ((xBytes.z & 0xFFu) << 16u) | ((xBytes.w & 0xFFu) << 24u);
}

float4 Flux_UnpackUnorm8x4(uint uPacked)
{
	return float4(uint4(uPacked, uPacked >> 8u, uPacked >> 16u, uPacked >> 24u) & 0xFFu) / 255.0;
}

uint4 Flux_UnpackUint8x4(uint uPacked)
{
	return uint4(uPacked, uPacked >> 8u, uPacked >> 16u, uPacked >> 24u) & 0xFFu;
}

// R/G/B are 10-bit snorm in [-511,511]; A is 2-bit snorm in {-1,0,1}. The clamp
// before the scale is load-bearing (it is what makes -512 / -2 unreachable, so the
// unpack's sign extension can never produce a value outside [-1,1]).
uint Flux_PackSnorm10_10_10_2(float4 xValue)
{
	float4 xClamped = clamp(xValue, -1.0, 1.0);
	int3 xRGB = int3(round(xClamped.xyz * 511.0));
	int  iA   = int(round(xClamped.w));
	return (uint(xRGB.x) & 0x3FFu)
		| ((uint(xRGB.y) & 0x3FFu) << 10u)
		| ((uint(xRGB.z) & 0x3FFu) << 20u)
		| ((uint(iA)     & 0x3u)   << 30u);
}

float4 Flux_UnpackSnorm10_10_10_2(uint uPacked)
{
	// Sign-extend each field by shifting it to the top of a 32-bit word and back
	// down arithmetically — the branchless form of the C++ mirror's XOR-subtract.
	int3 xRGB = int3(asint(uPacked << 22u), asint(uPacked << 12u), asint(uPacked << 2u)) >> 22;
	int  iA   = asint(uPacked) >> 30;
	return float4(clamp(float3(xRGB) / 511.0, -1.0, 1.0), clamp(float(iA), -1.0, 1.0));
}

#endif // FLUX_COMMON_VERTEXFORMATS_SLANG
