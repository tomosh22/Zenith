# GEOM design: the vertex format library and the shared codec

Section file of `Zenith2/Docs/Design/GEOM.md`. It specifies the leaf library `ZenithVertexFormat` (`Zenith2/Engine/ZenithVertexFormat/`, owned by GEOM, depending on ZenithCore alone), the GPU half of the codec under `Zenith2/Shaders/Common/`, the stream-set codes that name every vertex layout, the Slang declarations every layout comes from, and the tests that prove both halves agree bit for bit. The normative source is `Zenith2/Docs/Rendering.md → "Vertex data format"` and its section file `Zenith2/Docs/Rendering/VertexData.md`; this file elaborates it and never contradicts it. Obligations here are traced by the `Z2-R-GEOM-NNN` requirements of `Zenith2/Docs/Requirements/GEOM.md`.

## Scope of this file

| Topic | Heading |
|---|---|
| how both codec halves compute identical bits on any conforming device | "The exact arithmetic kernel" |
| SNORM, UNORM and integer lane codecs | "Scalar quantisation codecs" |
| the QTangent encode with its bias rule, and decode | "QTangent codec realisation" |
| octahedral normals | "Octahedral codec realisation" |
| the integer-only half-float codec | "Half-float codec realisation" |
| position and UV quantisation with scale and bias | "Position and UV quantisation" |
| every public declaration of the library | "Zenith_VertexKernel header", "Zenith_VertexFormat header", "Stream-set codes" |
| the 16-bit code that names a vertex layout, its canonical names, the reachable set, and the Slang generator and generated table that define each layout | "Stream-set codes", "Reachable stream-set rule", "Stream-set declarations in Slang", "Declared stream-sets" |
| the codes of the SPRITE, DEBUG and UI families that their owning areas declare | "Owner-family stream-sets" |
| how a program's vertex struct is specialised per stream-set, how an attribute a stream-set lacks is bound (the null stream), and which stream-sets are compiled | "Program stream-set specialisation" |
| the rules that keep depth-equal passes and paths bit-consistent | "Position decode invariance", "Decode-route consistency" |
| the Slang modules of the GPU half | "The GPU codec modules" |
| every test of the library and the GPU half | "Vertex format tests" |

<!-- informative -->
Zenith's `Flux/Flux_VertexCodec.h` was a header-only codec that delegated to GLM's packers and warned that decoded floats could not be compared bit for bit because the header was compiled under `/fp:fast`. Its reason to exist was sound — a bit layout duplicated between the terrain exporter and the editor had drifted — and Zenith2 keeps that idea (one codec, one library), but makes the bits reproducible across the CPU and every GPU by construction instead of by hope.
<!-- /informative -->

## The exact arithmetic kernel

The plan and `Zenith2/Docs/Rendering/VertexData.md → "Codec tests"` require the CPU codec and `VertexFormats.slang` to produce identical bits for encoders and decoders alike. Hardware square root, reciprocal and division are not correctly rounded on every GPU (Vulkan allows several ULP), and compilers may fuse a multiply and an add. The kernel removes both sources of difference: every codec function in both halves is written exclusively in the operations below, in the same order, so any device whose binary32 addition, subtraction and multiplication are correctly rounded produces the CPU's bits.

<!-- elaboration -->
### Kernel operations

| Operation | CPU (`Zenith_VertexKernel.h`) | GPU (`Zenith2/Shaders/Common/VertexKernel.slang`) | Definition |
|---|---|---|---|
| clean | `Zenith_VfClean(float)` | `Flux_VfClean(float)` | `abs(x) < fVF_TINY ? +0.0 : x`; a `-0.0` input returns `+0.0` |
| add, subtract, multiply | `Zenith_VfAdd`, `Zenith_VfSub`, `Zenith_VfMul` | `Flux_VfAdd`, `Flux_VfSub`, `Flux_VfMul` | the IEEE-754 binary32 operation, round to nearest even, followed by clean; never contracted into a fused operation |
| minimum, maximum, absolute value, negate, compare | `Zenith_VfMin`, `Zenith_VfMax`, `Zenith_VfAbs`, `Zenith_VfNeg`, the relational operators | the same names | exact by definition |
| floor | `Zenith_VfFloor` | `Flux_VfFloor` | exact |
| round half away from zero | `Zenith_VfRoundHalfAwayToInt(float) → sint32` | `Flux_VfRoundHalfAwayToInt(float) → int` | `a = abs(x); f = floor(a); r = a − f` (exact because `f ≤ a < f + 1`); `k = int(f) + (r >= 0.5 ? 1 : 0)`; return `x < 0 ? −k : k`. The tempting `floor(a + 0.5)` is not used: `0.49999997 + 0.5` rounds to 1.0 |
| reciprocal | `Zenith_VfRcp(float)` | `Flux_VfRcp(float)` | for `x > 0`: `r = asfloat(0x7EF311C7u − asuint(x))`, then exactly four iterations `r = VfMul(r, VfSub(2.0, VfMul(x, r)))`; for `x < 0`: `−VfRcp(−x)`; `x == 0` is a precondition failure (callers guard it) |
| reciprocal square root | `Zenith_VfRsqrt(float)` | `Flux_VfRsqrt(float)` | for `x > 0`: `r = asfloat(0x5F375A86u − (asuint(x) >> 1))`, then exactly three iterations `h = VfMul(0.5, x); t = VfMul(r, r); t = VfMul(h, t); t = VfSub(1.5, t); r = VfMul(r, t)`; `x ≤ 0` is a precondition failure |
| square root | `Zenith_VfSqrt(float)` | `Flux_VfSqrt(float)` | `x == 0 ? 0 : VfMul(x, VfRsqrt(x))` |
| division | `Zenith_VfDiv(float a, float b)` | `Flux_VfDiv` | `VfMul(a, VfRcp(b))` |
| dot products | `Zenith_VfDot3`, `Zenith_VfDot4` | `Flux_VfDot3`, `Flux_VfDot4` | left to right: `VfAdd(VfAdd(VfMul(a.x,b.x), VfMul(a.y,b.y)), VfMul(a.z,b.z))`, then `+ VfMul(a.w,b.w)` for four lanes |
| cross product | `Zenith_VfCross` | `Flux_VfCross` | `(VfSub(VfMul(a.y,b.z), VfMul(a.z,b.y)), VfSub(VfMul(a.z,b.x), VfMul(a.x,b.z)), VfSub(VfMul(a.x,b.y), VfMul(a.y,b.x)))` |
| normalise | `Zenith_VfNormalize3(v, bool& bDegenerate)`, `Zenith_VfNormalize4` | `Flux_VfNormalize3`, `Flux_VfNormalize4` | `l2 = Dot(v, v)`; degenerate when `l2 < fVF_DEGENERATE`; otherwise `v × VfRsqrt(l2)` per lane |
| float to integer bits and back | `std::bit_cast<uint32>`, `std::bit_cast<float>` | `asuint`, `asfloat` | exact |
| integer to float | `static_cast<float>(sint32)` for magnitudes below 2^24 | `float(int)` | exact in that range, the only range the codec uses |

### Kernel constants

| Constant | Value | Bits | Use |
|---|---|---|---|
| `fVF_TINY` | 2^-40 (9.094947e-13) | `0x2B800000` | the clean threshold |
| `fVF_DEGENERATE` | 2^-40 | `0x2B800000` | squared-length threshold of a degenerate vector |
| `fVF_SNORM16_RCP` | the binary32 value nearest 1/32767 | `0x38000100` | SNORM16 decode |
| `fVF_UNORM16_RCP` | nearest 1/65535 | `0x37800080` | UNORM16 decode |
| `fVF_UNORM8_RCP` | nearest 1/255 | `0x3B808081` | UNORM8 decode |
| `fVF_SNORM8_RCP` | nearest 1/127 | `0x3C010204` | SNORM8 decode |
| `fVF_QTANGENT_BIAS` | nearest 1/32767 | `0x38000100` | the QTangent `w` floor |
| `fVF_MIN_EXTENT` | 2^-20 m | `0x35800000` | the smallest quantisation half-extent per axis |
| `uVF_RCP_ITERATIONS` | 4 | — | Newton steps of `VfRcp` |
| `uVF_RSQRT_ITERATIONS` | 3 | — | Newton steps of `VfRsqrt` |

<!-- informative -->
Decoding by multiplication with the nearest reciprocal is not the correctly rounded quotient: for SNORM16, 1,536 of the 65,535 codes decode one ULP away from `round(c / 32767)`; for UNORM16 512, for UNORM8 126, for SNORM8 16 (measured exhaustively in the drafting session). The kernel accepts that, because the obligation is agreement between the two codec halves, which the reciprocal constant guarantees; agreement with a device's fixed-function conversion is measured separately ("Decode-route consistency").
<!-- /informative -->

### Kernel preconditions and device conformance

| Precondition | Why | Enforced by |
|---|---|---|
| every input is finite, and every magnitude the codec forms stays below 2^40 | no overflow and no NaN inside a Newton sequence | encoder entry checks (`VERTEX_FORMAT_ERROR_NON_FINITE_INPUT` on the CPU); the GPU encoders receive only values the skinning pass produced from finite data |
| every kernel input is 0 or of magnitude at least 2^-40, and every Newton seed of `VfRcp` and `VfRsqrt` is at least 2^-41 over the admitted range (the `VfRcp` seed of 2^40 is about 2^-40.1, below `fVF_TINY`, and seeds are not cleaned) | every operand of a `VfMul` is then at least 2^-63 in magnitude, every product at least 2^-126 and every non-zero difference of cleaned values at least 2^-63, so no result is subnormal before the clean and a device's flush-to-zero setting cannot change a result | the clean after every operation; `Test_VertexKernel_SeedsInRange` |
| binary32 add, sub and mul are correctly rounded (round to nearest even) | the basis of bit identity | `Test_VertexKernel_DeviceArithmeticConformance` on every CI and reference device |
| no contraction of a multiply and an add | a fused result differs from the two-step result | CPU: the compile flags of `Zenith2/Docs/Architecture/BuildSystem.md → "Compile and link flags"` (no floating-point contraction); GPU: every kernel function is declared with the `precise` modifier, which Slang lowers to SPIR-V `NoContraction` and DXIL `precise` |
| integer seeds use 32-bit unsigned arithmetic only | no `shaderInt16` or `shaderInt64` is needed | the baseline profile |

- Every codec function in the CPU and GPU halves calls kernel operations alone for floating-point work; a bare `a * b + c` in codec code is a defect found by `Test_VertexFormat_CpuGpuBitMatch`.
- `Test_VertexKernel_RsqrtAccuracy` and `Test_VertexKernel_RcpAccuracy` bound the Newton results at 2 ULP from the correctly rounded value over every binary32 value in [1, 4) and 10^6 fixed samples of [2^-40, 2^40]; accuracy is a quality property, reproducibility is the correctness property.

## Scalar quantisation codecs

<!-- elaboration -->
| Format | Encode (per lane) | Decode (per lane) |
|---|---|---|
| SNORM16 | `(sint16) VfRoundHalfAwayToInt(VfMul(VfMin(VfMax(v, −1), 1), 32767))` | `VfMax(VfMul(float(c), fVF_SNORM16_RCP), −1)` |
| UNORM16 | `(uint16) VfRoundHalfAwayToInt(VfMul(VfMin(VfMax(v, 0), 1), 65535))` | `VfMul(float(c), fVF_UNORM16_RCP)` |
| SNORM8 | as SNORM16 with 127 | `VfMax(VfMul(float(c), fVF_SNORM8_RCP), −1)` |
| UNORM8 | as UNORM16 with 255 | `VfMul(float(c), fVF_UNORM8_RCP)` |
| UINT8, UINT16, UINT32 | the integer value; an out-of-range value is `VERTEX_FORMAT_ERROR_VALUE_RANGE` on the CPU | the integer value |
| FLOAT16 | "Half-float codec realisation" | same |
| FLOAT32 | the bits of the input | the bits |

Lane order is little-endian and component 0 occupies the lowest address, matching how the fetch unit reads `R16G16B16A16_SNORM` and its relatives.

### Weight quantisation

Skin weights are quantised so the four stored weights sum exactly to the format's maximum:

```
QuantiseWeights(w[4], M):                       // M = 255 (UNORM8x4) or 65535 (UNORM16x4); w already the 4 largest, renormalised
	for i in 0..3: q[i] = VfRoundHalfAwayToInt(VfMul(w[i], M))
	residual = M − (q[0] + q[1] + q[2] + q[3])     // integer arithmetic; |residual| ≤ 2
	j = the index of the largest q[i], ties to the lowest index
	q[j] = q[j] + residual
	return q
```

- A vertex with more than four influences keeps the four largest (ties by the lower joint index) and renormalises them in double precision before `QuantiseWeights`; a vertex with no influence is bound to the mesh's root joint with weight `M`. Both rules are applied by the cook, never by the importer, so every importer gives the same result.

## QTangent codec realisation

The algorithm is the one of `Zenith2/Docs/Rendering/VertexData.md → "QTangent codec"` ([Z2-N-REND-876]); the pseudocode below is its realisation in kernel operations, in the evaluation order both halves use. `R_rc` names row r, column c of the rotation whose columns are `t`, `bU`, `n`.

<!-- elaboration -->
```
EncodeQTangentCore(t, n, fSign):                 // t, n: 3 lanes each, cleaned; fSign: +1 or −1
	// 1. Gram-Schmidt against n
	d   = VfDot3(n, t)
	tp  = (VfSub(t.x, VfMul(n.x, d)), VfSub(t.y, VfMul(n.y, d)), VfSub(t.z, VfMul(n.z, d)))
	t   = VfNormalize3(tp, bDegenerate)
	if bDegenerate:                               // tangent parallel to the normal, or zero
		e = axis of the smallest |n| component, ties x before y before z   // (1,0,0), (0,1,0) or (0,0,1)
		d = VfDot3(n, e)
		t = VfNormalize3(e − n × d)                // per lane VfSub(e.i, VfMul(n.i, d)); never degenerate for unit n
	// 2. unsigned bitangent: (t, bU, n) is a proper rotation
	bU  = VfCross(n, t)
	// 3. Shepperd's method on R = [t | bU | n]
	R00 = t.x;  R10 = t.y;  R20 = t.z
	R01 = bU.x; R11 = bU.y; R21 = bU.z
	R02 = n.x;  R12 = n.y;  R22 = n.z
	tr  = VfAdd(VfAdd(R00, R11), R22)
	if tr > 0:
		s = VfMul(VfSqrt(VfAdd(tr, 1)), 2);  i = VfRcp(s)
		q = (VfMul(VfSub(R21, R12), i), VfMul(VfSub(R02, R20), i), VfMul(VfSub(R10, R01), i), VfMul(0.25, s))
	else if R00 > R11 and R00 > R22:
		s = VfMul(VfSqrt(VfSub(VfSub(VfAdd(1, R00), R11), R22)), 2);  i = VfRcp(s)
		q = (VfMul(0.25, s), VfMul(VfAdd(R01, R10), i), VfMul(VfAdd(R02, R20), i), VfMul(VfSub(R21, R12), i))
	else if R11 > R22:
		s = VfMul(VfSqrt(VfSub(VfSub(VfAdd(1, R11), R00), R22)), 2);  i = VfRcp(s)
		q = (VfMul(VfAdd(R01, R10), i), VfMul(0.25, s), VfMul(VfAdd(R12, R21), i), VfMul(VfSub(R02, R20), i))
	else:
		s = VfMul(VfSqrt(VfSub(VfSub(VfAdd(1, R22), R00), R11)), 2);  i = VfRcp(s)
		q = (VfMul(VfAdd(R02, R20), i), VfMul(VfAdd(R12, R21), i), VfMul(0.25, s), VfMul(VfSub(R10, R01), i))
	q   = VfNormalize4(q)                          // q = (x, y, z, w)
	// 4. canonical hemisphere
	if q.w < 0: q = −q                             // same rotation, now w ≥ 0
	// 5. the bias rule: |w| never quantises to 0
	if q.w < fVF_QTANGENT_BIAS:
		q.w = fVF_QTANGENT_BIAS
		c   = VfSqrt(VfSub(1, VfMul(fVF_QTANGENT_BIAS, fVF_QTANGENT_BIAS)))
		k   = VfMul(c, VfRsqrt(VfDot3(q.xyz, q.xyz)))   // |q.xyz| ≈ 1 here, never degenerate
		q.xyz = (VfMul(q.x, k), VfMul(q.y, k), VfMul(q.z, k))
	// 6. reflection in the sign of w
	if fSign < 0: q = −q
	// 7. quantise
	return (SNORM16(q.x), SNORM16(q.y), SNORM16(q.z), SNORM16(q.w))   // "Scalar quantisation codecs"

EncodeQTangent(t, bSigned, n):                   // the cook's entry: MikkTSpace tangent, signed bitangent, normal
	clean every lane; n = VfNormalize3(n) (a degenerate normal is VERTEX_FORMAT_ERROR_DEGENERATE_NORMAL)
	tBuild = the t of step 1 above, bU = VfCross(n, tBuild)
	fSign  = VfDot3(bU, bSigned) < 0 ? −1 : +1
	return EncodeQTangentCore(t, n, fSign)

EncodeQTangentFromFrame(t, n, fSign):            // the skinning entry: sign carried from the bind-pose QTangent
	return EncodeQTangentCore(t, VfNormalize3(n), fSign)

DecodeQTangent(s):                               // four sint16
	q    = (SNORM16 decode of each lane)            // VfMax(VfMul(float(c), fVF_SNORM16_RCP), −1)
	sign = q.w < 0 ? −1 : +1
	q    = VfNormalize4(q)                          // never degenerate: |w| ≥ one quantum after encode
	x2 = VfMul(q.x, q.x); y2 = VfMul(q.y, q.y); z2 = VfMul(q.z, q.z)
	xy = VfMul(q.x, q.y); xz = VfMul(q.x, q.z); yz = VfMul(q.y, q.z)
	wx = VfMul(q.w, q.x); wy = VfMul(q.w, q.y); wz = VfMul(q.w, q.z)
	t  = (VfSub(1, VfMul(2, VfAdd(y2, z2))), VfMul(2, VfAdd(xy, wz)), VfMul(2, VfSub(xz, wy)))
	bU = (VfMul(2, VfSub(xy, wz)), VfSub(1, VfMul(2, VfAdd(x2, z2))), VfMul(2, VfAdd(yz, wx)))
	n  = VfCross(t, bU)                             // never sign-flipped
	return (t, bU × sign, n, sign)
```

- The decoded normal is `cross(t, bU)` in every case; the handedness sign multiplies the bitangent alone, so `Test_VertexFormat_QTangentNormalNeverFlipped` holds by construction.
- Because the Newton-based normalisation is not correctly rounded, a decoded `t` has unit length to within 4 ULP; consumers normalise again only where a pass needs exact unit length (the material input normal is re-normalised after interpolation anyway).

## Octahedral codec realisation

<!-- elaboration -->
```
EncodeOct(n):                                   // unit n, cleaned
	s  = VfAdd(VfAdd(VfAbs(n.x), VfAbs(n.y)), VfAbs(n.z))
	i  = VfRcp(s)
	p  = (VfMul(n.x, i), VfMul(n.y, i))
	if n.z < 0:
		p = (VfMul(VfSub(1, VfAbs(p.y)), SignNotZero(p.x)), VfMul(VfSub(1, VfAbs(p.x)), SignNotZero(p.y)))
	return (SNORM16(p.x), SNORM16(p.y))          // vertex streams, SNORM16x2

DecodeOct(c):
	p = (SNORM16 decode of c.x, of c.y)
	v = (p.x, p.y, VfSub(VfSub(1, VfAbs(p.x)), VfAbs(p.y)))
	k = VfMax(VfNeg(v.z), 0)
	v.x = v.x >= 0 ? VfSub(v.x, k) : VfAdd(v.x, k)
	v.y = v.y >= 0 ? VfSub(v.y, k) : VfAdd(v.y, k)
	return VfNormalize3(v)
```

`SignNotZero(v)` is `v >= 0 ? +1 : −1`. The render-target form (`× 0.5 + 0.5` into `RG16_UNORM`, [Z2-N-REND-877]) is written by pass shaders with ordinary arithmetic and is not a codec function: the ROP's UNORM conversion is fixed-function and never compared bit for bit with the CPU.

## Half-float codec realisation

The half codec is the integer-only algorithm of `Zenith2/Docs/Rendering/VertexData.md → "Half-float codec"` ([Z2-N-REND-878]) with no floating-point operation at all, so it is bit-identical by construction: `Zenith_F32ToF16(float) → uint16` and `Zenith_F16ToF32(uint16) → float` on the CPU (through `std::bit_cast`), `Flux_F32ToF16Bits(uint) → uint` and `Flux_F16BitsToF32(uint) → uint` in `Half.slang`. A binary32 subnormal input has exponent field 0, so `e = −112 < −10` and it encodes to a signed zero on both halves, whatever the device's denormal mode. `firstbithigh` on the GPU is replaced on the CPU by `std::bit_width(mant) − 1`.

## Position and UV quantisation

<!-- elaboration -->
| Quantity | Encode | Decode (vertex shader, `precise`) | Stored per mesh |
|---|---|---|---|
| position | `bias = (min + max) × 0.5`, `scale_i = max((max_i − min_i) × 0.5, fVF_MIN_EXTENT)` per axis, computed in double and rounded to binary32; `v_i = VfMul(VfSub(p_i, bias_i), VfRcp(scale_i))`; SNORM16 per lane, `w` stored as 0 | `p_i = VfAdd(VfMul(f_i, scale_i), bias_i)` in the kernel order; `w := 1` | `m_xPositionScale`, `m_xPositionBias` of every primitive record of the mesh (`Zenith2/Docs/Rendering/GpuResources.md → "Primitive record (128 bytes)"`) |
| position, FLOAT32x3 fallback | the source position | as above with `scale = (1, 1, 1)` and `bias = (0, 0, 0)`, so the shader code is identical | same fields |
| UV0, UV1 | `bias = uvMin`, `scale_i = max(uvMax_i − uvMin_i, 2^-20)`; `v = VfMul(VfSub(uv, bias), VfRcp(scale))`; UNORM16 | `uv = f × scale + bias` in kernel order | `m_xUV0Scale/Bias`, `m_xUV1Scale/Bias` |
| UV, FLOAT16x2 fallback | the source UV as half | `scale = (1, 1)`, `bias = (0, 0)` | same fields |

- The box and the UV ranges are those of the whole mesh across every LOD and every part, so every sub-mesh of a split, and every LOD, shares them ([Z2-N-REND-882]; `Zenith2/Docs/Design/GEOM/Meshes.md → "Quantisation box and precision checks"`).
- The fallback formats keep the decode formula and substitute identity scale and bias, so a precision fallback changes the vertex input layout of the PSO and never the shader's arithmetic.

## Zenith_VertexKernel header

```cpp
// Zenith2/Engine/ZenithVertexFormat/Zenith_VertexKernel.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Vector.h"

/// @contract functions Zenith_VertexKernel
/// Ownership: Pure functions over values; nothing is allocated.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Dimensionless unless the caller's values carry units.
/// Errors: Zenith_VfRcp of 0 and Zenith_VfRsqrt of a value ≤ 0 are ZENITH_ASSERT (callers guard them); every other
///   function is total over finite inputs below 2^40 in magnitude.
/// Version: since Z2 Stage 2. The operation sequence of every function is frozen: a change is a codec change that
///   re-cooks every mesh and edits Zenith2/Shaders/Common/VertexKernel.slang in the same diff.
/// @endcontract
constexpr float fVF_TINY = 9.094947017729282e-13f;           // 2^-40
constexpr float fVF_DEGENERATE = 9.094947017729282e-13f;     // 2^-40
constexpr float fVF_SNORM16_RCP = 3.0518509e-05f;            // bits 0x38000100
constexpr float fVF_UNORM16_RCP = 1.5259022e-05f;            // bits 0x37800080
constexpr float fVF_UNORM8_RCP = 3.9215688e-03f;             // bits 0x3B808081
constexpr float fVF_SNORM8_RCP = 7.8740157e-03f;             // bits 0x3C010204
constexpr float fVF_QTANGENT_BIAS = 3.0518509e-05f;          // bits 0x38000100
constexpr float fVF_MIN_EXTENT = 9.5367432e-07f;             // 2^-20
constexpr uint32 uVF_RCP_ITERATIONS = 4;
constexpr uint32 uVF_RSQRT_ITERATIONS = 3;

float Zenith_VfClean(float fValue);
float Zenith_VfAdd(float fA, float fB);
float Zenith_VfSub(float fA, float fB);
float Zenith_VfMul(float fA, float fB);
float Zenith_VfMin(float fA, float fB);
float Zenith_VfMax(float fA, float fB);
float Zenith_VfAbs(float fValue);
float Zenith_VfNeg(float fValue);
float Zenith_VfFloor(float fValue);
sint32 Zenith_VfRoundHalfAwayToInt(float fValue);
float Zenith_VfRcp(float fValue);
float Zenith_VfRsqrt(float fValue);
float Zenith_VfSqrt(float fValue);
float Zenith_VfDiv(float fNumerator, float fDenominator);
float Zenith_VfDot3(const Zenith_Vector3& xA, const Zenith_Vector3& xB);
float Zenith_VfDot4(const Zenith_Vector4& xA, const Zenith_Vector4& xB);
Zenith_Vector3 Zenith_VfCross(const Zenith_Vector3& xA, const Zenith_Vector3& xB);
Zenith_Vector3 Zenith_VfNormalize3(const Zenith_Vector3& xValue, bool& bDegenerateOut);
Zenith_Vector4 Zenith_VfNormalize4(const Zenith_Vector4& xValue, bool& bDegenerateOut);
```

Implementation: `Zenith2/Engine/ZenithVertexFormat/Internal/Zenith_VertexKernel.cpp`. Each arithmetic function stores its operands into `volatile`-free locals and returns `Zenith_VfClean(fA + fB)` etc.; the no-contraction flag of the build makes the separate statements binding, and `Test_VertexKernel_NoContraction` asserts `Zenith_VfAdd(Zenith_VfMul(a, b), c)` differs from the fused result for inputs known to differ.

## Zenith_VertexFormat header

The layout-table types are exactly those of `Zenith2/Docs/Rendering/VertexData.md → "Generated layout tables"`: `Zenith_VertexFormatType` (15 formats and `ZENITH_VERTEX_FORMAT_COUNT`), `Zenith_VertexSemantic`, `Zenith_VertexInputRate`, `Zenith_VertexAttributeDesc` (8 bytes), `Zenith_VertexStreamDesc` (4 bytes), `Zenith_VertexLayoutDesc` and `Zenith_GetVertexFormatBytes`. This header declares them with their contract blocks and adds the codec.

```cpp
// Zenith2/Engine/ZenithVertexFormat/Zenith_VertexFormat.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Vector.h"

/// @contract enum Zenith_VertexFormatError
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Returned by every fallible function of this library; see the table "Vertex format errors" of
///   Zenith2/Docs/Design/GEOM.md → "Threading lifetime and error contract".
/// Version: since Z2 Stage 2. Enumerators keep their meaning once shipped.
/// @endcontract
enum Zenith_VertexFormatError : uint8
{
	VERTEX_FORMAT_ERROR_UNKNOWN_FORMAT = 1,
	VERTEX_FORMAT_ERROR_UNKNOWN_SEMANTIC,
	VERTEX_FORMAT_ERROR_BAD_LAYOUT,
	VERTEX_FORMAT_ERROR_BAD_STREAM_SET,
	VERTEX_FORMAT_ERROR_NON_FINITE_INPUT,
	VERTEX_FORMAT_ERROR_DEGENERATE_NORMAL,
	VERTEX_FORMAT_ERROR_VALUE_RANGE,
	VERTEX_FORMAT_ERROR_DESTINATION_TOO_SMALL,
	VERTEX_FORMAT_ERROR_COUNT_OVERFLOW,
	VERTEX_FORMAT_ERROR_BAD_NAME,
	VERTEX_FORMAT_ERROR_UNDECLARED_STREAM_SET,
};
constexpr const char* Zenith_ToString(Zenith_VertexFormatError eError);

// Zenith_VertexFormatType, Zenith_VertexSemantic, Zenith_VertexInputRate, Zenith_VertexAttributeDesc,
// Zenith_VertexStreamDesc, Zenith_VertexLayoutDesc: exactly as Zenith2/Docs/Rendering/VertexData.md
// "Generated layout tables", each preceded by its contract block (kind enum or value; Thread affinity IMMUTABLE, the
// value blocks adding "Members non-const: ZenithShaderCompiler fills reflected layouts field by field and sets
// m_ulLayoutHash after the layout is complete.";
// Version since Z2 Stage 2, "values are part of the shader blob container and cooked meshes; a change re-cooks").
static_assert(sizeof(Zenith_VertexAttributeDesc) == 8);
static_assert(sizeof(Zenith_VertexStreamDesc) == 4);

constexpr uint32 uVERTEX_MAX_STREAMS = 8;             // [Stream(n)], n in 0..7
constexpr uint32 uVERTEX_MAX_ATTRIBUTES = 16;
constexpr uint32 uVERTEX_NULL_STREAM = 7;             // the stride-0 null stream of specialised tables; reserved: a declaration using it is ZSC_ERROR_STREAM_SET_LOCATION
constexpr uint32 uVERTEX_NULL_STREAM_BYTES = 32;      // the zero buffer bound there; null attributes satisfy offset + bytes <= 32

/// @contract value Zenith_VertexStreamSetCode
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned into descriptors, records and tables.
/// Invalidation: None.
/// Units: None; a bit field per Zenith2/Docs/Design/GEOM/VertexFormat.md "Stream-set code fields".
/// Errors: Zenith_ValidateStreamSetCode returns VERTEX_FORMAT_ERROR_BAD_STREAM_SET for a reserved value.
/// Version: since Z2 Stage 2. Bit assignments never change; a new field takes reserved bits.
/// @endcontract
struct Zenith_VertexStreamSetCode
{
	uint16 m_uBits = 0;
	friend constexpr bool operator==(Zenith_VertexStreamSetCode, Zenith_VertexStreamSetCode) = default;
};

/// @contract value Zenith_StreamSetLayoutEntry
/// Ownership: Plain value; entries live in the constexpr tables ZenithShaderCompiler generates, and m_xLayout points into
///   the same generated header.
/// Thread affinity: IMMUTABLE.
///   Members non-const: ZenithShaderCompiler sorts the entries by code before it writes the table.
/// Invalidation: None; generated tables have static storage duration.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Part of the generated-header contract; a change regenerates every header.
/// @endcontract
struct Zenith_StreamSetLayoutEntry
{
	Zenith_VertexStreamSetCode m_xCode;
	Zenith_VertexLayoutDesc m_xLayout;
};

/// @contract value Zenith_PackedQTangent
/// Ownership: Plain value; its byte image is the stream-1 SNORM16x4 lanes x, y, z, w.
/// Thread affinity: IMMUTABLE.
///   Members non-const: vertex streams are packed element by element.
/// Invalidation: None.
/// Units: SNORM16 codes.
/// Errors: None.
/// Version: since Z2 Stage 2. Byte image fixed.
/// @endcontract
struct Zenith_PackedQTangent { sint16 m_aiLanes[4]; };
static_assert(sizeof(Zenith_PackedQTangent) == 8);

/// @contract value Zenith_TangentFrame
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None.
/// Units: Unit vectors (to 4 ULP); m_fSign is +1 or −1.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_TangentFrame
{
	Zenith_Vector3 m_xTangent;
	Zenith_Vector3 m_xBitangent;     // the shading bitangent: the unsigned bitangent times m_fSign
	Zenith_Vector3 m_xNormal;        // cross(tangent, unsigned bitangent); never sign-flipped
	float m_fSign = 1.0f;
};

/// @contract value Zenith_QuantisationBox
/// Ownership: Plain value; stored per mesh in the cooked file and in every primitive record.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None.
/// Units: Metres (positions) or UV units (UV boxes).
/// Errors: Zenith_MakeQuantisationBox returns VERTEX_FORMAT_ERROR_NON_FINITE_INPUT for a non-finite bound.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_QuantisationBox
{
	Zenith_Vector3 m_xScale;         // half-extent per axis, ≥ fVF_MIN_EXTENT (positions); range per axis (UVs, z unused)
	Zenith_Vector3 m_xBias;          // centre (positions); minimum (UVs, z unused)
};

/// @contract functions Zenith_VertexCodec
/// Ownership: Pure functions; spans are borrowed for the call and never retained.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Positions in metres; UVs in UV units; normals and tangents unit length.
/// Errors: Zenith_Result<…, Zenith_VertexFormatError>: NON_FINITE_INPUT for NaN or infinity, DEGENERATE_NORMAL for a
///   zero normal, VALUE_RANGE for an integer lane that does not fit, DESTINATION_TOO_SMALL for a short span,
///   COUNT_OVERFLOW when count × stride overflows uint64. The scalar encoders clamp and never fail.
/// Version: since Z2 Stage 2. Bit-identical to Zenith2/Shaders/Common/VertexFormats.slang for every input
///   (Zenith2/Docs/Design/GEOM/VertexFormat.md "Vertex format tests"); a change re-cooks every mesh.
/// @endcontract
uint32 Zenith_GetVertexFormatBytes(Zenith_VertexFormatType eFormat);
uint32 Zenith_GetVertexFormatLanes(Zenith_VertexFormatType eFormat);
Zenith_Result<Zenith_VertexFormatType, Zenith_VertexFormatError> Zenith_ParseVertexFormat(const char* szVtxFmt);   // "snorm16x4" → ZENITH_VERTEX_FORMAT_SNORM16X4
const char* Zenith_GetVertexFormatName(Zenith_VertexFormatType eFormat);

sint16 Zenith_EncodeSnorm16(float fValue);
float Zenith_DecodeSnorm16(sint16 iCode);
uint16 Zenith_EncodeUnorm16(float fValue);
float Zenith_DecodeUnorm16(uint16 uCode);
sint8 Zenith_EncodeSnorm8(float fValue);
float Zenith_DecodeSnorm8(sint8 iCode);
uint8 Zenith_EncodeUnorm8(float fValue);
float Zenith_DecodeUnorm8(uint8 uCode);
uint16 Zenith_F32ToF16(float fValue);
float Zenith_F16ToF32(uint16 uHalf);

Zenith_Result<Zenith_PackedQTangent, Zenith_VertexFormatError> Zenith_EncodeQTangent(
	const Zenith_Vector3& xTangent, const Zenith_Vector3& xSignedBitangent, const Zenith_Vector3& xNormal);
Zenith_Result<Zenith_PackedQTangent, Zenith_VertexFormatError> Zenith_EncodeQTangentFromFrame(
	const Zenith_Vector3& xTangent, const Zenith_Vector3& xNormal, float fSign);
Zenith_TangentFrame Zenith_DecodeQTangent(const Zenith_PackedQTangent& xPacked);
Zenith_Result<uint32, Zenith_VertexFormatError> Zenith_EncodeOctahedral(const Zenith_Vector3& xUnitNormal);   // lanes x | y << 16
Zenith_Vector3 Zenith_DecodeOctahedral(uint32 uPacked);
void Zenith_QuantiseWeights(const float (&afWeights)[4], uint32 uMaximum, uint16 (&auOut)[4]);   // uMaximum 255 or 65535

Zenith_Result<Zenith_QuantisationBox, Zenith_VertexFormatError> Zenith_MakePositionBox(
	const Zenith_Vector3& xMin, const Zenith_Vector3& xMax);
Zenith_Result<Zenith_QuantisationBox, Zenith_VertexFormatError> Zenith_MakeUVBox(
	float fMinU, float fMinV, float fMaxU, float fMaxV);
Zenith_Result<void, Zenith_VertexFormatError> Zenith_QuantisePositions(Zenith_Span<const Zenith_Vector3> axPositions,
	const Zenith_QuantisationBox& xBox, Zenith_MutableByteSpan xDestination, uint32 uStride);   // SNORM16x4, w = 0
Zenith_Vector3 Zenith_DequantisePosition(const sint16 (&aiLanes)[4], const Zenith_QuantisationBox& xBox);
Zenith_Result<void, Zenith_VertexFormatError> Zenith_QuantiseUVs(Zenith_Span<const Zenith_Vector2> axUVs,
	const Zenith_QuantisationBox& xBox, Zenith_MutableByteSpan xDestination, uint32 uStride);   // UNORM16x2

/// @contract value Zenith_VertexSourceStreams
/// Ownership: Borrowed views over the caller's decoded source attributes, one entry per semantic; a span may be empty.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None; the spans must outlive the Zenith_PackVertexStreams call.
/// Units: As Zenith_VertexCodec.
/// Errors: None; Zenith_PackVertexStreams validates the spans.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_VertexSourceStreams
{
	uint32 m_uVertexCount = 0;
	Zenith_Span<const Zenith_Vector3> m_axPositions;
	Zenith_Span<const Zenith_Vector3> m_axNormals;
	Zenith_Span<const Zenith_Vector4> m_axTangents;       // xyz tangent, w the MikkTSpace bitangent sign
	Zenith_Span<const Zenith_Vector2> m_axUV0;
	Zenith_Span<const Zenith_Vector2> m_axUV1;
	Zenith_Span<const Zenith_Vector4> m_axColours;        // linear, straight alpha
	Zenith_Span<const uint16> m_auJoints;                // 4 per vertex, mesh-local joint indices
	Zenith_Span<const float> m_afWeights;                 // 4 per vertex, renormalised
	Zenith_QuantisationBox m_xPositionBox;
	Zenith_QuantisationBox m_xUV0Box;
	Zenith_QuantisationBox m_xUV1Box;
};

// Packs every stream of xLayout into axDestinations[stream], in the byte order and at the offsets the layout names. The
// cook passes the generated entry of axFLUX_STREAMSET_LAYOUTS for the mesh's code ("Stream-set declarations in Slang"),
// the table runtime PSO creation is built from ([Z2-N-REND-872]); the packer itself holds no layout.
Zenith_Result<void, Zenith_VertexFormatError> Zenith_PackVertexStreams(const Zenith_VertexSourceStreams& xSource,
	const Zenith_VertexLayoutDesc& xLayout, Zenith_Span<const Zenith_MutableByteSpan> axDestinations);
Zenith_Result<void, Zenith_VertexFormatError> Zenith_ValidateVertexLayout(const Zenith_VertexLayoutDesc& xLayout);
uint64 Zenith_ComputeVertexLayoutHash(Zenith_Span<const Zenith_VertexAttributeDesc> axAttributes,
	Zenith_Span<const Zenith_VertexStreamDesc> axStreams);
```

### Layout validation and hashing

<!-- elaboration -->
| Rule of `Zenith_ValidateVertexLayout` | Error |
|---|---|
| every format and semantic value is below its `COUNT` | `UNKNOWN_FORMAT`, `UNKNOWN_SEMANTIC` |
| every attribute's stream appears once in the stream table, streams ascend, at most `uVERTEX_MAX_STREAMS` | `BAD_LAYOUT` |
| offsets are multiples of 4, attributes of one stream do not overlap, and `offset + bytes ≤ stride` | `BAD_LAYOUT` |
| every stride is a multiple of 4 and at most 64; a stride of 0 is allowed for stream `uVERTEX_NULL_STREAM` (7) alone, and stream 7, when present, has stride 0: a non-zero stride on stream 7 is refused, because stream 7 is reserved to the null stream. Stream 7's attributes satisfy `offset + bytes ≤ uVERTEX_NULL_STREAM_BYTES` (32) instead of `≤ stride` ("Program stream-set specialisation", row "null stream") | `BAD_LAYOUT` |
| a (semantic, semantic index) pair appears at most once | `BAD_LAYOUT` |
| a declaration table (an entry of `axFLUX_STREAMSET_LAYOUTS`): `m_ulLayoutHash` equals `Zenith_ComputeVertexLayoutHash` of the tables. A program's specialised table instead carries its declaration's hash (the value a mesh records), which `Zenith_ValidateVertexLayout` does not recompute; the shader compiler asserts the copy when it emits the table (`Test_ShaderCompiler_StreamSetTables`) | `BAD_LAYOUT` |

`Zenith_ComputeVertexLayoutHash` hashes, with `Zenith_Hash64Stream` seed 0 (`Zenith2/Docs/Design/CORE/Maths.md → "Zenith_Hash64"`), the canonical byte serialisation: the attribute count (uint32 LE), then each attribute in ascending (stream, offset) order as semantic u8, semantic index u8, format u8, stream u8, offset u16 LE, location u16 LE; then the stream count (uint32 LE) and each stream as stream u8, rate u8, stride u16 LE. It never hashes struct memory ([Z2-N-ARCH-884], Z2-D-038). The hash algorithm is D-SEL-HASH (open, Stage 1); until the ruling the function exists in the API-only slice and no cooked mesh records a hash.

## Stream-set codes

A stream-set is one complete vertex layout: which streams exist, what each holds and in which formats. Every mesh, every procedural instance, every indirect bucket ([Z2-N-REND-887]) and every raster permutation (the `m_uStreamSetID` of `Zenith_RHIPermutationKey`, `Zenith2/Docs/Rendering/ShaderSystem.md → "Permutations"`) names one by its 16-bit code. The code is a stable bit field, never a dense runtime index, so a recorded PSO set, a persistent pipeline cache entry and a cooked mesh all mean the same layout in every build.

A code names a layout; it never defines one. Every layout is a Slang vertex-input struct of `Zenith2/Shaders/Common/StreamSets.slang` ("Stream-set declarations in Slang"), and C++ sees it only through the table `ZenithShaderCompiler` generates from that declaration, which runtime PSO creation and the cook's packer both read ([Z2-N-REND-865], [Z2-N-REND-872]). No C++ function computes an offset, a stride or a location.

<!-- elaboration -->
### Stream-set code fields

| Bits | Field | Values |
|---|---|---|
| 0–1 | `POS` | 0 `SNORM16X4` (stream 0, unified pool); 1 `FLOAT32X3` (stream 0, unified pool; the precision fallback); 2 `POOL16` (FLOAT16x4 positions pulled from the skinned-vertex pool by the vertex stage; no stream 0, so a program's specialised table binds its `POSITION` to the null stream, "Program stream-set specialisation"); 3 `POOL32` (FLOAT32x4 pool fallback; no stream 0, likewise) |
| 2–3 | `SURF` | 0 `QTANGENT` (SNORM16x4, stream 1); 1 `OCT` (octahedral normal SNORM16x2, stream 1, the normal-only variant); 2 `NONE` (no surface attribute: the TERRAIN family alone; never a MESH or PROCEDURAL code, by cross-doc ruling X140, "Canonical stream-set layouts"); 3 reserved |
| 4 | `UV0` | UV0 present in stream 1 |
| 5 | `UV0F` | 0 UNORM16x2; 1 FLOAT16x2 (the fallback) |
| 6 | `UV1` | UV1 present in stream 2 |
| 7 | `UV1F` | 0 UNORM16x2; 1 FLOAT16x2 |
| 8 | `COL` | vertex colour UNORM8x4 in stream 2 |
| 9–11 | `SKIN` | 0 none; 1 `J8W8` (UINT8x4 joints, UNORM8x4 weights); 2 `J16W8`; 3 `J8W16`; 4 `J16W16`; 5–7 reserved (stream 3) |
| 12–14 | `FAMILY` | 0 `MESH`; 1 `TERRAIN`; 2 `PROCEDURAL` (streams 0 and 1 bound from the two regions of the procedural-geometry pool, `Zenith2/Docs/Design/GEOM/Procedural.md → "Procedural stream-sets and pool layout"`); 3 `SPRITE` (TWOD's per-instance streams); 4 `DEBUG` (DEVT); 5 `UI` (UI); 6–7 reserved |
| 15 | reserved | 0 |

For the `SPRITE`, `DEBUG` and `UI` families, bits 0–11 are not these fields but the layout's index within its family, which the owning area assigns ("Owner-family stream-sets").

The code of the default mesh `pos-snorm16x4|surface-qt16-uv16` is `0x0010`. `Zenith_GetDefaultStreamSet` returns each family's default:

| Family | Default code | Canonical name |
|---|---|---|
| `MESH` | `0x0010` | `pos-snorm16x4|surface-qt16-uv16` |
| `TERRAIN` | `0x1008` | `terrain:pos-snorm16x4` (SURF `NONE`, no UV0) |
| `PROCEDURAL` | `0x2010` | `procedural:pos-snorm16x4|surface-qt16-uv16` |
| `SPRITE` | `0x3001` | `sprite:canvas` (TWOD, "Owner-family stream-sets") |
| `DEBUG`, `UI` | none here | `VERTEX_FORMAT_ERROR_BAD_STREAM_SET`; each owning area (DEVT, UI) names its family default in its own design and adds the row to this table in the diff that declares its stream-sets. DEVT declares no `DEBUG` stream-set (A3 row DV18) |

`Zenith_VertexStreamSetCode` and the generated-table entry `Zenith_StreamSetLayoutEntry` are declared in `Zenith_VertexFormat.h` ("Zenith_VertexFormat header"), because a generated header may include that header and no other of this library ([Z2-N-REND-116]); field decoding, names and lookups are in `Zenith_VertexStreamSet.h`.

```cpp
// Zenith2/Engine/ZenithVertexFormat/Zenith_VertexStreamSet.h
#pragma once
#include "ZenithVertexFormat/Zenith_VertexFormat.h"
#include "ZenithCore/Zenith_StringView.h"

enum Zenith_StreamSetPosition : uint8 { STREAM_SET_POS_SNORM16X4, STREAM_SET_POS_FLOAT32X3, STREAM_SET_POS_POOL16, STREAM_SET_POS_POOL32 };
enum Zenith_StreamSetSurface : uint8 { STREAM_SET_SURF_QTANGENT, STREAM_SET_SURF_OCT, STREAM_SET_SURF_NONE };
enum Zenith_StreamSetSkin : uint8 { STREAM_SET_SKIN_NONE, STREAM_SET_SKIN_J8W8, STREAM_SET_SKIN_J16W8, STREAM_SET_SKIN_J8W16, STREAM_SET_SKIN_J16W16 };
enum Zenith_StreamSetFamily : uint8 { STREAM_SET_FAMILY_MESH, STREAM_SET_FAMILY_TERRAIN, STREAM_SET_FAMILY_PROCEDURAL,
	STREAM_SET_FAMILY_SPRITE, STREAM_SET_FAMILY_DEBUG, STREAM_SET_FAMILY_UI };
// Each enumeration above carries its own contract block (kind enum, IMMUTABLE) in the header.

/// @contract value Zenith_StreamSetFields
/// Ownership: Plain value; the decoded form of a code.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_StreamSetFields
{
	Zenith_StreamSetPosition m_ePosition = STREAM_SET_POS_SNORM16X4;
	Zenith_StreamSetSurface m_eSurface = STREAM_SET_SURF_QTANGENT;
	bool m_bUV0 = true;
	bool m_bUV0Half = false;
	bool m_bUV1 = false;
	bool m_bUV1Half = false;
	bool m_bColour = false;
	Zenith_StreamSetSkin m_eSkin = STREAM_SET_SKIN_NONE;
	Zenith_StreamSetFamily m_eFamily = STREAM_SET_FAMILY_MESH;
	uint16 m_uOwnerIndex = 0;     // SPRITE, DEBUG, UI: bits 0–11 of the code; 0 for MESH, TERRAIN, PROCEDURAL
};

/// @contract functions Zenith_VertexStreamSets
/// Ownership: Pure functions; outputs are written into caller storage; Zenith_FindStreamSetLayout returns a pointer into
///   the caller's table.
/// Thread affinity: ANY_THREAD.
/// Invalidation: The pointer Zenith_FindStreamSetLayout returns is valid while axTable is; generated tables have static
///   storage duration.
/// Units: None.
/// Errors: VERTEX_FORMAT_ERROR_BAD_STREAM_SET for reserved field values or an inconsistent combination (UV0F without
///   UV0, UV1F without UV1, SKIN with a pool position, a SURF other than NONE in the TERRAIN family, an owner-family
///   code absent from axVERTEX_OWNER_STREAM_SETS);
///   VERTEX_FORMAT_ERROR_BAD_NAME for a name that does not parse; VERTEX_FORMAT_ERROR_UNDECLARED_STREAM_SET when the
///   table holds no entry for the code; VERTEX_FORMAT_ERROR_BAD_LAYOUT when a declared layout's attributes differ from
///   the ones its code names.
/// Version: since Z2 Stage 2. Names are frozen per code; layouts are whatever StreamSets.slang declares.
/// @endcontract
constexpr Zenith_VertexStreamSetCode Zenith_EncodeStreamSet(const Zenith_StreamSetFields& xFields);
constexpr Zenith_StreamSetFields Zenith_DecodeStreamSet(Zenith_VertexStreamSetCode xCode);
Zenith_Result<void, Zenith_VertexFormatError> Zenith_ValidateStreamSetCode(Zenith_VertexStreamSetCode xCode);
Zenith_Result<uint32, Zenith_VertexFormatError> Zenith_FormatStreamSetName(Zenith_VertexStreamSetCode xCode,
	Zenith_Span<char> xDestination);       // returns the length; at most 96 bytes are ever needed
Zenith_Result<Zenith_VertexStreamSetCode, Zenith_VertexFormatError> Zenith_ParseStreamSetName(Zenith_StringView strName);
// Binary search of a generated table sorted by ascending code (axFLUX_STREAMSET_LAYOUTS, or a program's
// axFLUX_<PROGRAM>_VERTEX_LAYOUTS); the one way C++ obtains a stream-set's layout.
Zenith_Result<const Zenith_VertexLayoutDesc*, Zenith_VertexFormatError> Zenith_FindStreamSetLayout(
	Zenith_Span<const Zenith_StreamSetLayoutEntry> axTable, Zenith_VertexStreamSetCode xCode);
// The shader compiler's declaration check: the reflected layout holds exactly the (semantic, semantic index, format)
// triples the code's fields name. It reads semantics and formats only; offsets, strides and locations are Slang's.
Zenith_Result<void, Zenith_VertexFormatError> Zenith_CheckStreamSetDeclaration(Zenith_VertexStreamSetCode xCode,
	const Zenith_VertexLayoutDesc& xReflected);
constexpr Zenith_VertexStreamSetCode Zenith_GetRasterStreamSet(Zenith_VertexStreamSetCode xCookedCode);
Zenith_Result<Zenith_VertexStreamSetCode, Zenith_VertexFormatError> Zenith_GetDefaultStreamSet(
	Zenith_StreamSetFamily eFamily);   // the table above; BAD_STREAM_SET for DEBUG and UI
// The reachable-set rule of "Reachable stream-set rule": true exactly for the codes StreamSets.slang must declare. It reads
// the code's fields alone; it computes no offset, stride or location.
constexpr bool Zenith_IsReachableStreamSet(Zenith_VertexStreamSetCode xCode);
constexpr uint32 uVERTEX_REACHABLE_STREAM_SET_COUNT = 255;   // 252 MESH + 1 TERRAIN + 2 PROCEDURAL
```

`Zenith_GetRasterStreamSet(x)`: when SKIN is not none, the result has SKIN = none and POS = `POOL32` when the cooked POS is `FLOAT32X3`, `POOL16` when it is `SNORM16X4`; an unskinned code is returned unchanged. The cooked code alone decides, because the cook ties the two position fallbacks of a skinned mesh together ("Fallback derivation rule"). The raster code names the PSO permutation, the stream-set of the mesh's buckets (`Zenith2/Docs/Design/RPROD/Culling.md → "Bucket registry"`) and the pool of the streams a raster draw binds; the cooked code of a skinned mesh names its skin-source pool (`Zenith2/Docs/Design/GEOM/Meshes.md → "Geometry pools"`).

### Canonical stream-set names

| Segment (in stream order, joined by `|`) | Written when | Text |
|---|---|---|
| family prefix | FAMILY ≠ MESH | `terrain:`, `procedural:`, `sprite:`, `debug:`, `ui:` before the first segment; an owner-family code is the prefix and its table name alone (`sprite:canvas`), with no segment |
| stream 0 | POS is SNORM16X4 or FLOAT32X3 | `pos-snorm16x4`, `pos-float32x3` |
| pool positions | POS is POOL16 or POOL32 | `pos-pool16x4`, `pos-pool32x4` |
| stream 1 | SURF ≠ NONE or UV0 | `surface-` then `qt16` / `oct16` / `none`, then `-uv16` or `-uvf16` when UV0 |
| stream 2 | UV1 or COL | `opt-` then `uv1-uv16` / `uv1-uvf16` when UV1, then `-col8` (or `col8` alone) when COL |
| stream 3 | SKIN ≠ none | `skin-j8w8`, `skin-j16w8`, `skin-j8w16`, `skin-j16w16` |

Examples: the default mesh `pos-snorm16x4|surface-qt16-uv16` (the name of `Zenith2/Docs/Rendering/ShaderSystem.md → "PSO precache sets"`); a skinned character `pos-snorm16x4|surface-qt16-uv16|skin-j8w8` and its raster form `pos-pool16x4|surface-qt16-uv16`; a lightmapped prop `pos-snorm16x4|surface-oct16-uv16|opt-uv1-uv16`; a terrain grid `terrain:pos-snorm16x4`. Formatting and parsing are exact inverses over every valid code (`Test_StreamSet_NameRoundTripEveryCode`). A valid code is not necessarily a declared one: validity is the bit field's rule, and declaration is decided by the reachable-set rule below and StreamSets.slang.

### Owner-family stream-sets

<!-- elaboration: answers A3 row TD6 and A5 row TwB3 -->
A code of the `SPRITE`, `DEBUG` or `UI` family names a per-instance layout that its owning area declares in a `[StreamSet]` struct under `Zenith2/Shaders/Common/`. Its bits 0–11 are the layout's index within the family, 1 … 4,095. GEOM keeps the closed table of these codes in `Zenith_VertexStreamSet.h`. An owning area adds its rows, in its own diff, with GEOM as reviewer:

```cpp
// Zenith_VertexStreamSet.h additions
struct Zenith_OwnerStreamSet { Zenith_VertexStreamSetCode m_xCode; Zenith_StreamSetFamily m_eFamily; const char* m_szName; };
inline constexpr Zenith_OwnerStreamSet axVERTEX_OWNER_STREAM_SETS[] =   // ascending code
{
	{ { 0x3001 }, STREAM_SET_FAMILY_SPRITE, "canvas" },
	{ { 0x3002 }, STREAM_SET_FAMILY_SPRITE, "world" },
	{ { 0x3003 }, STREAM_SET_FAMILY_SPRITE, "tilechunk" },
};
```

| Code | Canonical name | Owner | Declared in |
|---|---|---|---|
| `0x3001` | `sprite:canvas` | TWOD | `Zenith2/Shaders/Common/SpriteStreamSets.slang` (`Zenith2/Docs/Design/TWOD/Rendering.md → "Sprite stream-sets"`) |
| `0x3002` | `sprite:world` | TWOD | the same |
| `0x3003` | `sprite:tilechunk` | TWOD | the same |

| Function | Owner-family rule |
|---|---|
| `Zenith_ValidateStreamSetCode` | accepts an owner-family code exactly when it is an entry of `axVERTEX_OWNER_STREAM_SETS`. Bit 15, a reserved family or any other code of the three families is `VERTEX_FORMAT_ERROR_BAD_STREAM_SET` |
| `Zenith_DecodeStreamSet`, `Zenith_EncodeStreamSet` | decode gives `m_eFamily` and `m_uOwnerIndex` = bits 0–11, with every other field at its default value. Encode of an owner family writes the family and `m_uOwnerIndex` alone |
| `Zenith_FormatStreamSetName`, `Zenith_ParseStreamSetName` | the family prefix and the table's name, both ways. An owner-family prefix followed by a name the table lacks is `VERTEX_FORMAT_ERROR_BAD_NAME` |
| `Zenith_CheckStreamSetDeclaration` | an owner-family code checks the shape alone. It needs at least one attribute. Every attribute must sit on stream 4, 5 or 6 at input rate `ZENITH_VERTEX_INPUT_RATE_INSTANCE`, with an `INSTANCE_*` semantic (location 8 onwards, "Canonical stream-set layouts"). An attribute on streams 0–3 or 7, a vertex-rate stream or another semantic is `VERTEX_FORMAT_ERROR_BAD_LAYOUT`. The fields, offsets and strides are the owner's, and the owner's test checks them (TWOD's `Test_SpriteStreamSet_LayoutsMatchTable`) |
| `Zenith_IsReachableStreamSet` | false: owner families stay outside the reachable-set rule and its count of 255 |
| `Zenith_GetDefaultStreamSet` | `SPRITE` → `0x3001` |

The `--stream-sets` command reflects owner-family structs like any other, through the rules of "Stream-set declarations in Slang": `ZSC_ERROR_STREAM_SET_CODE` for a code the table lacks, `ZSC_ERROR_STREAM_SET_MISMATCH` for a struct that fails the shape check. It requires no struct for a table row, so a GEOM row may land before its owner's `.slang` file. `Test_StreamSet_OwnerFamilyCodes` checks every function row for the three SPRITE codes, `0x3004` and a `DEBUG` code. `Test_StreamSet_OwnerDeclarationShape` checks that the canvas layout of TWOD's table passes the shape check, and that the same layout with one attribute moved to stream 7, to stream 0 or to vertex rate fails it.

### Reachable stream-set rule

A code is *reachable* when the cook, the terrain renderer or the procedural system can produce it: when the "Stream-set choice" table of `Zenith2/Docs/Design/GEOM/Meshes.md` and the "Fallback derivation rule" below can yield it for some mesh, or when it is the terrain grid's or a procedural generator's code. StreamSets.slang declares exactly the reachable codes of the MESH, TERRAIN and PROCEDURAL families. `Zenith_IsReachableStreamSet` states the rule over the decoded fields:

<!-- elaboration -->
| Family | A valid code is reachable exactly when | Count | Why |
|---|---|---|---|
| `MESH`, unskinned | POS ∈ {`SNORM16X4`, `FLOAT32X3`}, SURF ∈ {`QTANGENT`, `OCT`}, SKIN none; UV0 any of {absent, UNORM16, FLOAT16}; stream 2 any of {absent, UV1 UNORM16, UV1 FLOAT16, COL, UV1 UNORM16 + COL, UV1 FLOAT16 + COL} | 2 × 2 × 3 × 6 = 72 | POS follows the position check; SURF follows the normal-map test; each UV set is present when the source has it and a material reads it (UV1 also when lightmapped), in the format its own UV check picks; COL follows the material reflection. The axes are independent, so every combination occurs. `QTANGENT` without UV0 occurs for a normal map sampled through UV1, and `OCT` without UV0 for an untextured lit mesh |
| `MESH`, skinned (cooked) | POS ∈ {`SNORM16X4`, `FLOAT32X3`}, SURF `QTANGENT`, SKIN ∈ {`J8W8`, `J16W8`, `J8W16`, `J16W16`}; UV0 and stream 2 as above | 2 × 1 × 3 × 6 × 4 = 144 | a skinned mesh always carries a QTangent; J8/J16 follows the joint count and W8/W16 the weight check; POS `FLOAT32X3` follows either tied position check |
| `MESH`, skinned (raster) | POS ∈ {`POOL16`, `POOL32`}, SURF `QTANGENT`, SKIN none; UV0 and stream 2 as above | 2 × 1 × 3 × 6 = 36 | `Zenith_GetRasterStreamSet` of every skinned cooked code; `POOL32` exactly when the cooked POS is `FLOAT32X3` |
| `TERRAIN` | POS `SNORM16X4`, SURF `NONE`, no UV0, UV1, COL or SKIN | 1 | the one grid mesh. Its positions carry integer grid indices that the vertex stage recovers by rounding ("Morphing and displacement" of `Zenith2/Docs/Design/GEOM/Meshes.md`), so no precision check applies |
| `PROCEDURAL` | POS `SNORM16X4`, SURF `QTANGENT`, UV0 UNORM16 or FLOAT16, no UV1, COL or SKIN | 2 | the generator's declared UV0 format; SNORM16 over a declared box always passes the position check (`Zenith2/Docs/Design/GEOM/Procedural.md → "Procedural stream-sets"`) |
| `SPRITE`, `DEBUG`, `UI` | never: their layouts are their owning areas' and outside this rule | 0 | — |
| total | | 255 (`uVERTEX_REACHABLE_STREAM_SET_COUNT`) | |

- SURF `OCT` with SKIN ≠ none, and a pool POS with any SKIN, are unreachable because the choice table cannot produce them. So is SURF `NONE` in the MESH family: cross-doc ruling X140 rejected a surface-less MESH stream 1, so an unlit-only mesh cooks with the octahedral surface stream (`Zenith2/Docs/Design/GEOM/Meshes.md → "Stream-set choice"`) and the declared set stays 255 (cross-area row GM18).
- Every mandated precision fallback of every family is therefore declared wherever its check can fail: FLOAT32x3 position, FLOAT16 UV0 and UV1, UINT16x4 joints, UNORM16x4 weights and the FLOAT32 skinned pool are each an axis value that the product crosses with every other axis ([Z2-N-REND-879], [Z2-N-REND-880]). A failed precision check is never a cook error. `Test_StreamSet_EveryFallbackDeclared` asserts both halves of this.

### Stream-set declarations in Slang

Every reachable stream-set is one vertex-input struct of `Zenith2/Shaders/Common/StreamSets.slang`, attributed `[StreamSet(<code>)]` and written with the `[Stream(n)]` and `[VtxFmt]` attributes of `Zenith2/Docs/Rendering/VertexData.md → "Vertex declaration in Slang"`. The structs are not written one by one. StreamSets.slang expands one generator macro, `FLUX_SS_DECLARE`, over the reachable product, using the segment macros of `Zenith2/Shaders/Common/StreamSetFields.slang`. The expansion is done by the Slang preprocessor alone: no tool writes Slang, and the declarations stay the one source of every layout (plan §5.9). Fields come out in stream order and, within a stream, in the attribute order of the canonical name. The SPRITE, DEBUG and UI families declare their stream-sets the same way, in a module of the owning area under `Zenith2/Shaders/Common/` (for example `Common/SpriteStreamSets.slang`), outside the reachable-set rule.

<!-- elaboration -->
`StreamSetFields.slang` defines, for every value of every axis, a macro `<AXIS>_<VALUE>_FIELDS` that expands to that value's field declarations (empty when the value adds none) and a macro `<AXIS>_<VALUE>_BITS` that expands to its code bits, parenthesised:

| Axis | Value (macro suffix) | `_FIELDS` | `_BITS` |
|---|---|---|---|
| `FLUX_SS_FAMILY` | `MESH`, `TERRAIN`, `PROCEDURAL` | none | `(0u << 12)`, `(1u << 12)`, `(2u << 12)` |
| `FLUX_SS_POS` | `SNORM16X4` | `[Stream(0)] [VtxFmt("snorm16x4")] float4 m_xPosition : POSITION;` | `(0u)` |
| | `FLOAT32X3` | `[Stream(0)] [VtxFmt("float32x3")] float3 m_xPosition : POSITION;` | `(1u)` |
| | `POOL16X4`, `POOL32X4` | none (the position is pulled from the skinned pool) | `(2u)`, `(3u)` |
| `FLUX_SS_SURF` | `QT16` | `[Stream(1)] [VtxFmt("snorm16x4")] float4 m_xQTangent : QTANGENT;` | `(0u << 2)` |
| | `OCT16` | `[Stream(1)] [VtxFmt("snorm16x2")] float2 m_xNormalOct : NORMAL_OCT;` | `(1u << 2)` |
| | `NONE` | none | `(2u << 2)` |
| `FLUX_SS_UV0` | `NONE` | none | `(0u)` |
| | `UV16` | `[Stream(1)] [VtxFmt("unorm16x2")] float2 m_xUV0 : TEXCOORD0;` | `(1u << 4)` |
| | `UVF16` | `[Stream(1)] [VtxFmt("float16x2")] float2 m_xUV0 : TEXCOORD0;` | `((1u << 4) | (1u << 5))` |
| `FLUX_SS_OPT` | `NONE` | none | `(0u)` |
| | `UV16` | `[Stream(2)] [VtxFmt("unorm16x2")] float2 m_xUV1 : TEXCOORD1;` | `(1u << 6)` |
| | `UVF16` | `[Stream(2)] [VtxFmt("float16x2")] float2 m_xUV1 : TEXCOORD1;` | `((1u << 6) | (1u << 7))` |
| | `COL8` | `[Stream(2)] [VtxFmt("unorm8x4")] float4 m_xColour : COLOR0;` | `(1u << 8)` |
| | `UV16_COL8`, `UVF16_COL8` | the UV1 field of `UV16` or `UVF16`, then the `COL8` field | `((1u << 6) | (1u << 8))`, `((1u << 6) | (1u << 7) | (1u << 8))` |
| `FLUX_SS_SKIN` | `NONE` | none | `(0u << 9)` |
| | `J8W8`, `J16W8`, `J8W16`, `J16W16` | `[Stream(3)] [VtxFmt("uint8x4" or "uint16x4")] uint4 m_xJoints : JOINTS0;` then `[Stream(3)] [VtxFmt("unorm8x4" or "unorm16x4")] float4 m_xWeights : WEIGHTS0;` | `(1u << 9)`, `(2u << 9)`, `(3u << 9)`, `(4u << 9)` |

```slang
// Zenith2/Shaders/Common/StreamSets.slang (the whole file)
#include "StreamSetFields.slang"

#define FLUX_SS_DECLARE(F, P, S, U, O, K) \
	[StreamSet(FLUX_SS_FAMILY_##F##_BITS | FLUX_SS_POS_##P##_BITS | FLUX_SS_SURF_##S##_BITS | \
		FLUX_SS_UV0_##U##_BITS | FLUX_SS_OPT_##O##_BITS | FLUX_SS_SKIN_##K##_BITS)] \
	struct Flux_StreamSet_##F##_##P##_##S##_##U##_##O##_##K \
	{ \
		FLUX_SS_POS_##P##_FIELDS FLUX_SS_SURF_##S##_FIELDS FLUX_SS_UV0_##U##_FIELDS \
		FLUX_SS_OPT_##O##_FIELDS FLUX_SS_SKIN_##K##_FIELDS \
	};

// X(F, P, S, U, O, K) over the six stream-2 values, and over the three UV0 values times those
#define FLUX_SS_EACH_OPT(X, F, P, S, U, K) \
	X(F, P, S, U, NONE, K) X(F, P, S, U, UV16, K) X(F, P, S, U, UVF16, K) \
	X(F, P, S, U, COL8, K) X(F, P, S, U, UV16_COL8, K) X(F, P, S, U, UVF16_COL8, K)
#define FLUX_SS_EACH_UV0_OPT(X, F, P, S, K) \
	FLUX_SS_EACH_OPT(X, F, P, S, NONE, K) FLUX_SS_EACH_OPT(X, F, P, S, UV16, K) FLUX_SS_EACH_OPT(X, F, P, S, UVF16, K)

// MESH, unskinned: 72
FLUX_SS_EACH_UV0_OPT(FLUX_SS_DECLARE, MESH, SNORM16X4, QT16, NONE)
FLUX_SS_EACH_UV0_OPT(FLUX_SS_DECLARE, MESH, SNORM16X4, OCT16, NONE)
FLUX_SS_EACH_UV0_OPT(FLUX_SS_DECLARE, MESH, FLOAT32X3, QT16, NONE)
FLUX_SS_EACH_UV0_OPT(FLUX_SS_DECLARE, MESH, FLOAT32X3, OCT16, NONE)
// MESH, skinned (cooked): 144
FLUX_SS_EACH_UV0_OPT(FLUX_SS_DECLARE, MESH, SNORM16X4, QT16, J8W8)
FLUX_SS_EACH_UV0_OPT(FLUX_SS_DECLARE, MESH, SNORM16X4, QT16, J16W8)
FLUX_SS_EACH_UV0_OPT(FLUX_SS_DECLARE, MESH, SNORM16X4, QT16, J8W16)
FLUX_SS_EACH_UV0_OPT(FLUX_SS_DECLARE, MESH, SNORM16X4, QT16, J16W16)
FLUX_SS_EACH_UV0_OPT(FLUX_SS_DECLARE, MESH, FLOAT32X3, QT16, J8W8)
FLUX_SS_EACH_UV0_OPT(FLUX_SS_DECLARE, MESH, FLOAT32X3, QT16, J16W8)
FLUX_SS_EACH_UV0_OPT(FLUX_SS_DECLARE, MESH, FLOAT32X3, QT16, J8W16)
FLUX_SS_EACH_UV0_OPT(FLUX_SS_DECLARE, MESH, FLOAT32X3, QT16, J16W16)
// MESH, skinned (raster): 36
FLUX_SS_EACH_UV0_OPT(FLUX_SS_DECLARE, MESH, POOL16X4, QT16, NONE)
FLUX_SS_EACH_UV0_OPT(FLUX_SS_DECLARE, MESH, POOL32X4, QT16, NONE)
// TERRAIN: 1
FLUX_SS_DECLARE(TERRAIN, SNORM16X4, NONE, NONE, NONE, NONE)
// PROCEDURAL: 2
FLUX_SS_DECLARE(PROCEDURAL, SNORM16X4, QT16, UV16, NONE, NONE)
FLUX_SS_DECLARE(PROCEDURAL, SNORM16X4, QT16, UVF16, NONE, NONE)
```

For example, `FLUX_SS_DECLARE(MESH, SNORM16X4, QT16, UV16, NONE, NONE)` expands to `[StreamSet((0u << 12) | (0u) | (0u << 2) | (1u << 4) | (0u) | (0u << 9))] struct Flux_StreamSet_MESH_SNORM16X4_QT16_UV16_NONE_NONE { [Stream(0)] … POSITION; [Stream(1)] … QTANGENT; [Stream(1)] … TEXCOORD0; };`, which is code `0x0010`. The struct name is not part of the identity. The attribute's argument is an integer constant expression, which Slang folds and the compiler reads through the reflection's attribute argument. The compiler derives the canonical name with `Zenith_FormatStreamSetName`. `[StreamSet(<code>)]` joins the closed attribute vocabulary of `Zenith2/Docs/Rendering/ShaderSystem.md → "Slang authoring vocabulary"` as a struct attribute (cross-area row GM24). Adding a value to an axis takes one `_FIELDS`/`_BITS` pair, the generator lines that cross it, and the matching edit of `Zenith_IsReachableStreamSet`. The compiler rules below reject a generator the edit gets wrong.

<!-- elaboration -->
| Compiler rule (the `--stream-sets` command) | Error |
|---|---|
| every struct under `Zenith2/Shaders/Common/` carrying `[StreamSet]` is reflected by the rules of [Z2-N-REND-868]–[Z2-N-REND-870]: attributes, formats, offsets in declaration order at 4-byte alignment, strides, and locations from the semantic table of "Canonical stream-set layouts" | `ZSC_ERROR_VERTEX_FORMAT` for a vocabulary violation |
| the attribute's argument folds to an integer code that `Zenith_ValidateStreamSetCode` accepts | `ZSC_ERROR_STREAM_SET_CODE`, naming the struct and the value |
| the reflected layout holds exactly the attributes the code names, in the code's formats (`Zenith_CheckStreamSetDeclaration`) | `ZSC_ERROR_STREAM_SET_MISMATCH`, naming the struct, the code and the first differing attribute |
| no two structs declare one code | `ZSC_ERROR_STREAM_SET_DUPLICATE`, naming both structs |
| `[StreamSet]` outside `Common/`, or on a struct with a field that is not a vertex input | `ZSC_ERROR_STREAM_SET_LOCATION` |
| no `[StreamSet]` struct of any family (SPRITE, DEBUG and UI included) and no family struct of `MeshVertex.slang` places a field in `[Stream(7)]`: stream `uVERTEX_NULL_STREAM` is reserved to the null stream of specialised tables ("Program stream-set specialisation"), so no declared stream can collide with the null binding of a program that draws both | `ZSC_ERROR_STREAM_SET_LOCATION`, naming the struct and the field |
| the declared MESH, TERRAIN and PROCEDURAL codes are exactly the codes `Zenith_IsReachableStreamSet` accepts | `ZSC_ERROR_STREAM_SET_MISSING` for a reachable code with no struct, naming its canonical name; `ZSC_ERROR_STREAM_SET_UNREACHABLE` for a declared code the rule rejects, naming the struct |
| a family struct of `MeshVertex.slang` (`Flux_MeshVertex` for MESH and PROCEDURAL, `Flux_TerrainVertex` for TERRAIN) holds every attribute of its family default's declaration with the same semantic, format, stream and offset (it may add attributes in further streams) | `ZSC_ERROR_STREAM_SET_MISMATCH` |
| every name of `Zenith2/Shaders/StreamSets.list` parses (`Zenith_ParseStreamSetName`) to a declared code | `ZSC_ERROR_STREAM_SET_NAME` |

The command is one aggregate custom command of `ZenithShadersGenerated` beside the shared-types command ([Z2-N-REND-135]): `ZenithShaderCompiler --stream-sets --shader-root Zenith2/Shaders --header-dir ${ZENITH_OUT}/generated/ZenithShadersGenerated/ --depfile <file>`, whose inputs are the `.slang` files of `Zenith2/Shaders/Common/` and on whose output every per-program command depends. It writes `${ZENITH_OUT}/generated/ZenithShadersGenerated/Flux/Shaders/Flux_StreamSets.generated.h`:

```cpp
// generated, Flux/Shaders/Flux_StreamSets.generated.h; includes Zenith_VertexFormat.h alone
inline constexpr Zenith_VertexAttributeDesc axFLUX_STREAMSET_ATTRIBUTES_0010[] =
{
	{ ZENITH_VERTEX_SEMANTIC_POSITION, 0, ZENITH_VERTEX_FORMAT_SNORM16X4, 0, 0, 0 },
	{ ZENITH_VERTEX_SEMANTIC_QTANGENT, 0, ZENITH_VERTEX_FORMAT_SNORM16X4, 1, 0, 1 },
	{ ZENITH_VERTEX_SEMANTIC_TEXCOORD, 0, ZENITH_VERTEX_FORMAT_UNORM16X2, 1, 8, 2 },
};
inline constexpr Zenith_VertexStreamDesc axFLUX_STREAMSET_STREAMS_0010[] =
{
	{ 0, ZENITH_VERTEX_INPUT_RATE_VERTEX, 8 },
	{ 1, ZENITH_VERTEX_INPUT_RATE_VERTEX, 12 },
};
// ... one pair per declared stream-set, the pairs in ascending code order, then the index in ascending code order:
inline constexpr Zenith_StreamSetLayoutEntry axFLUX_STREAMSET_LAYOUTS[] =
{
	{ { 0x0010 }, { axFLUX_STREAMSET_ATTRIBUTES_0010, 3, axFLUX_STREAMSET_STREAMS_0010, 2, 0x0000000000000000ull } },
	// ...
};
```

The layout hash of each entry is `Zenith_ComputeVertexLayoutHash` of the reflected tables ("Layout validation and hashing"; 0 until D-SEL-HASH (open, Stage 1) is ruled, as that section states). Every array of the header is emitted in ascending code order, each attribute array in ascending (stream, offset) order, with fixed formatting. The command is byte-deterministic: the same `Common/` sources give the same header bytes on every host (`Test_StreamSet_HeaderDeterministic`). The table has 255 entries of at most 7 attributes and 4 streams each, under 32 KiB of constant data.

| Consumer | Reads |
|---|---|
| runtime PSO creation | the program's specialised tables ("Program stream-set specialisation"), which the compiler copies from the declarations ([Z2-N-REND-871]) |
| the cook's packer | `axFLUX_STREAMSET_LAYOUTS` through `Zenith_FindStreamSetLayout`, passed to `Zenith_PackVertexStreams`; ZenithCook therefore declares `ZenithShadersGenerated` in its `DEPENDS` ([Z2-N-REND-872]) |
| the mesh reader | `axFLUX_STREAMSET_LAYOUTS`: the cooked code must be declared and its recorded hash must equal the entry's (`Zenith2/Docs/Design/GEOM/Meshes.md → "Reader validation"`) |
| the cook's stream-set choice | `Zenith_FindStreamSetLayout`. The choice function returns only reachable codes (`Test_Cook_ChoiceReachesOnlyDeclaredCodes`), and the compiler refuses a table missing a reachable code (`ZSC_ERROR_STREAM_SET_MISSING`), so `GEOM_COOK_ERROR_STREAM_SET_UNDECLARED` is a defence only: it names the mesh and the code when a table lacks the chosen code, and it is never the outcome of a failed precision check |

<!-- assumption: pending MOD -->
The library table of `Zenith2/Docs/Architecture.md` lists ZenithCook's dependencies without `ZenithShadersGenerated`. The edge ZenithCook → ZenithShadersGenerated is a tools-only edge; ZenithShadersGenerated depends on ZenithCore, ZenithVertexFormat and ZenithRHI alone ([Z2-N-REND-108]), so no cycle forms. It is requested in the cross-area notes, and GEOM's cook code is written against it.
<!-- /assumption -->

<!-- assumption: pending RPROD -->
`[StreamSet(<code>)]` is not yet in `Zenith2/Docs/Rendering/ShaderSystem.md → "Slang authoring vocabulary"`. The `--stream-sets` command, `Flux_StreamSets.generated.h` and the seven `ZSC_ERROR_STREAM_SET_*` codes (`CODE`, `MISMATCH`, `DUPLICATE`, `LOCATION`, `MISSING`, `UNREACHABLE`, `NAME`) are not yet in "The ZenithShaderCompiler tool" and "Generated parameter headers". The cross-area notes request them (GM3, amended by GM24), with a row for [Z2-N-REND-116]'s element table.
<!-- /assumption -->

### Declared stream-sets

StreamSets.slang declares exactly the reachable set of "Reachable stream-set rule", 255 codes, as the product below. The cook never invents a layout and never refuses a precision fallback, because every code its choice can produce is in this product.

| Group | Canonical names (the product) | Count |
|---|---|---|
| MESH, unskinned | `pos-{snorm16x4, float32x3}` × stream 1 `surface-{qt16, oct16}{, -uv16, -uvf16}` × stream 2 {absent, `opt-uv1-uv16`, `opt-uv1-uvf16`, `opt-col8`, `opt-uv1-uv16-col8`, `opt-uv1-uvf16-col8`} | 72 |
| MESH, skinned (cooked) | `pos-{snorm16x4, float32x3}` × `surface-qt16{, -uv16, -uvf16}` × the six stream-2 values × `skin-{j8w8, j16w8, j8w16, j16w16}` | 144 |
| MESH, skinned (raster) | `pos-{pool16x4, pool32x4}` × `surface-qt16{, -uv16, -uvf16}` × the six stream-2 values | 36 |
| TERRAIN | `terrain:pos-snorm16x4` | 1 |
| PROCEDURAL | `procedural:pos-snorm16x4|surface-qt16-{uv16, uvf16}` | 2 |
| total | | 255 |

Declaring a code costs one entry of `axFLUX_STREAMSET_LAYOUTS` and nothing else. Program blobs are compiled only for the codes of `StreamSets.list` and of the game's `stream_sets.json`, that is, the codes cooked content uses ("Program stream-set specialisation", rows "engine build" and "cook"). The per-game permutation count stays capped by `Z2-B-RPROD-024` Shader permutation limit per game (pending, [Z2-N-REND-880]).

The two pool forms of one stream-1 and stream-2 combination declare identical vertex inputs (the pool is not a vertex stream), so they share a layout hash while naming different codes; the code, not the hash, selects `Flux_FetchPoolVertex`'s FLOAT16 or FLOAT32 read.

### Canonical stream-set layouts

These are the layouts the declarations above produce under [Z2-N-REND-869]. The table is the review checklist for StreamSets.slang and the fixture of its test oracle; no code reads it.

| Stream | Attributes in order (semantic, format) | Stride |
|---|---|---|
| 0 | POS SNORM16X4: `POSITION` SNORM16X4 · POS FLOAT32X3: `POSITION` FLOAT32X3 · POS POOL16/POOL32: stream absent | 8 · 12 · — |
| 1 | SURF QTANGENT: `QTANGENT` SNORM16X4; SURF OCT: `NORMAL_OCT` SNORM16X2; SURF NONE: no surface attribute; then, when UV0, `TEXCOORD` index 0 UNORM16X2 or FLOAT16X2 | see the stride table below |
| 2 | when UV1, `TEXCOORD` index 1 UNORM16X2 or FLOAT16X2; then, when COL, `COLOR` UNORM8X4 | 4 or 8 |
| 3 | `JOINTS` UINT8X4 or UINT16X4, then `WEIGHTS` UNORM8X4 or UNORM16X4 | 8, 12 or 16 |

<!-- elaboration -->
| Stream 1 variant | Stride | Row of Rendering's stride table |
|---|---|---|
| `surface-qt16-uv16`, `surface-qt16-uvf16` | 12 | the default, and the FLOAT16 UV0 fallback |
| `surface-oct16-uv16`, `surface-oct16-uvf16` | 8 | the normal-only variant |
| `surface-oct16` | 4 | the normal-only variant without UV0 |
| SURF `NONE` without UV0 | stream absent | none: the TERRAIN family alone, outside [Z2-N-REND-875]'s default family; no MESH or PROCEDURAL code has SURF `NONE` |

Cross-doc ruling X140 (orchestrator, GEOM round 3) rejected a no-surface stream 1 for the MESH family: `Zenith2/Docs/Rendering/VertexData.md → "Default stream layout"` and its stride table stay as they are, [Z2-N-REND-875] binds the default family to them, StreamSets.slang declares no MESH-family `NONE` stream-set, and the cook chooses `OCT` for a mesh whose materials are all unlit. The declared set therefore stays 255 codes (cross-area row GM18).

| Semantic | Shader input location (`m_uLocation`) |
|---|---|
| `POSITION` | 0 |
| `QTANGENT`, `NORMAL_OCT` | 1 |
| `TEXCOORD` 0 | 2 |
| `TEXCOORD` 1 | 3 |
| `COLOR` 0 | 4 |
| `JOINTS` 0 | 5 |
| `WEIGHTS` 0 | 6 |
| `INSTANCE_*` | 8 onwards, assigned by the owning family (TWOD, DEVT, UI) |

Every stream in this table has input rate `ZENITH_VERTEX_INPUT_RATE_VERTEX`; the instance-rate streams of the SPRITE, DEBUG and UI families are specified by their owning areas, and GEOM validates their layouts with the same rules. The layout hash a cooked mesh records is the one of its code's entry in `axFLUX_STREAMSET_LAYOUTS` ([Z2-N-REND-872]).

`Test_StreamSet_DeclarationsMatchOracle` (`Zenith2/Tools/ZenithShaderCompiler/Tests/`) holds a test-only oracle, `BuildOracleLayout(code)`, that derives this table from a code. It asserts that every entry of the generated `axFLUX_STREAMSET_LAYOUTS` equals the oracle's layout, and that the table's MESH, TERRAIN and PROCEDURAL entries are exactly the 255 codes `Zenith_IsReachableStreamSet` accepts. The oracle is not a source of truth: it lives in the test target, no shipping or tool target links it, and when the two disagree, review corrects either the Slang declaration or the oracle.

### Fallback derivation rule

Every precision fallback ("Precision checks and fallbacks" of `Zenith2/Docs/Rendering/VertexData.md`) is a distinct declaration of "Declared stream-sets", because each field change below maps a reachable code to a reachable code; its code differs from the unfallen code in the fields below. For a skinned mesh, the joint and weight fallbacks change the cooked code alone (its skin-source pool and its `ComputeSkinning` permutation); the raster code is unchanged.

| Failed check | Field change |
|---|---|
| position error above tolerance | POS `SNORM16X4` → `FLOAT32X3`; for a skinned mesh also the raster POS `POOL16` → `POOL32` |
| UV0 texel error above tolerance, FLOAT16 better | UV0F 0 → 1 |
| UV1 texel error above tolerance, FLOAT16 better | UV1F 0 → 1 |
| more than 256 joints | SKIN J8x → J16x |
| weight error above 1 mm | SKIN xW8 → xW16 |
| posed-extent half error above 1 mm | the raster POS `POOL16` → `POOL32`, and also POS `SNORM16X4` → `FLOAT32X3` |

A skinned mesh's two position fallbacks are tied: either failed check sets both, because Rendering's skinning and morph programs have one position permutation, `bPERM_POSITION32`, for the bind-position input and the pool output together (`Zenith2/Docs/Design/GEOM/Skinning.md → "Compute skinning dispatch"`). The report records the tie as `Zenith2/Docs/Design/GEOM/Meshes.md → "Precision fallback report format"` states: one record per failed check, plus a forced `position` record carrying `"tiedBy": "bPERM_POSITION32"` when only the posed-extent check failed.

The normal-only variant (SURF `OCT`) is chosen by the cook for meshes none of whose materials samples a normal map, and for meshes with unlit materials alone (ruling X140); neither is a fallback, and neither is reported as one.

## Program stream-set specialisation

Rendering's permutation enumeration multiplies raster programs by "the vertex stream-sets the program draws (from its vertex-input declaration and every precision-fallback layout it names)" (`Zenith2/Docs/Rendering/ShaderSystem.md → "Permutations"`, step 5). This section states how the compiler derives those stream-sets and their tables from the declarations, so the compiler (RPROD's tool, linking ZenithVertexFormat) and the cook agree.

<!-- elaboration -->
| Rule | Specification |
|---|---|
| family | a raster program's vertex-input struct is one of the family structs of `Zenith2/Shaders/Common/MeshVertex.slang` (`Flux_MeshVertex` draws the MESH and PROCEDURAL families, `Flux_TerrainVertex` the TERRAIN family) or an owning area's family struct; its declarations contain its family default's declared stream-set (a compiler rule of "Stream-set declarations in Slang") |
| drawable stream-sets | only raster codes are drawable: a code with SKIN ≠ none (a cooked skinned code) is never drawable, because it keys only its compute-only skin-source pool (`Zenith2/Docs/Design/GEOM/Meshes.md → "Geometry pools"`) and its raster form is the pool code `Zenith_GetRasterStreamSet` returns. For a program that declares a surface semantic (`QTANGENT` or `NORMAL_OCT`): every declared code of the program's families that holds POSITION or is a pool code (POS `POOL16`/`POOL32`), that has SKIN none, and that holds a surface attribute in either form. For a program that declares no surface semantic (position-only depth and shadow programs, and the terrain programs): every declared code of its families that holds POSITION or is a pool code, and that has SKIN none. UV0, UV1 and COLOR never restrict drawability, because an attribute the program declares and the code lacks is bound to the null stream (row "null stream"). A stream the code has and the program does not read is bound but unused. So `pos-*|surface-oct16` (no UV0) is drawable by every MESH program, and `terrain:pos-snorm16x4` by every TERRAIN program |
| surface substitution | when the code's surface form differs from the program's semantic, the specialised attribute keeps the program's semantic, semantic index and location, and takes the code's format, stream and offset. The D3D12 semantic name and the Vulkan location therefore match the shader's input. The shader reads a `float4`; an SNORM16X2 format supplies lanes x and y, and the fetch unit fills z = 0, w = 1 on both backends. `Flux_DecodeSurface` decodes the lanes per `uFLUX_STREAMSET_SURFACE` |
| null stream | an attribute the program declares and the code lacks goes to stream `uVERTEX_NULL_STREAM` (7) at input rate `VERTEX` and stride 0. The null attributes are packed contiguously in the program's declaration order, each at the previous one's offset plus its size, each in the format the family default declares for its semantic (`POSITION` SNORM16X4, 8 bytes; `TEXCOORD` UNORM16X2 and `COLOR` UNORM8X4, 4 bytes each). For `Flux_MeshVertex` (declaration order POSITION, QTANGENT, TEXCOORD0, TEXCOORD1, COLOR0) a pool code's table therefore holds POSITION at offset 0 and, when the code lacks them, UV0 at 8, UV1 at 12 and colour at 16, and an unskinned code's table (which always holds POSITION) holds UV0, UV1 and colour at 0, 4 and 8: at most four null attributes, 20 bytes. POSITION is a null attribute of exactly the pool codes (POS `POOL16`/`POOL32`), whose declarations have no stream 0: every permutation of a `Flux_MeshVertex` program declares `m_xPosition : POSITION` at location 0, because `bPERM_SKINNED` is a link-time constant that cannot remove a field from the vertex-input struct (`Zenith2/Docs/Rendering/ShaderSystem.md → "Slang authoring vocabulary"`), and a PSO whose input state provides no attribute for a consumed location is invalid on both backends; the `bPERM_SKINNED = true` branch of `Flux_TransformObjectToClip` ignores the zero it reads (row "skinned programs"). Every draw that uses such a table binds `Flux_NullVertexStream` there: a `uVERTEX_NULL_STREAM_BYTES` (32-byte) zero-filled `Flux_PersistentReadOnlyBuffer` with usage `VERTEX`, created once with the renderer and bound unchanged every frame. A zero code decodes to 0 in every format, so an absent UV reads (0, 0), an absent colour (0, 0, 0, 0) and a null position (0, 0, 0, 0). PSO consequence: on Vulkan the table yields one extra `VkVertexInputBindingDescription { binding = 7, stride = 0, inputRate = VK_VERTEX_INPUT_RATE_VERTEX }` and one `VkVertexInputAttributeDescription { location, binding = 7, format, offset }` per null attribute; on D3D12 it yields one `D3D12_INPUT_ELEMENT_DESC { SemanticName, SemanticIndex, Format, InputSlot = 7, AlignedByteOffset, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }` per null attribute, and the draw binds a `D3D12_VERTEX_BUFFER_VIEW { BufferLocation, SizeInBytes = 32, StrideInBytes = 0 }`. For a pool code the Vulkan input state has no binding 0 and the D3D12 input layout no element in slot 0: POSITION's description names binding 7 (slot 7). Both backends accept stride 0 on a per-vertex binding, and the null stream is part of the specialised table, so PSO creation still reads the generated table alone ([Z2-N-REND-871]). The binding is requested from RPROD's draw recording as cross-area row GM25 |
| skinned programs | a pool code (POS `POOL16` or `POOL32`) is drawable only by a program's `bPERM_SKINNED = true` permutation; every other code is drawable only by its `bPERM_SKINNED = false` permutation. `bPERM_SKINNED` stays a `[Permutation]` link-time constant, which the compiler couples to the stream-set: it compiles a pool code only with `true` and every other code only with `false`, so it is a function of the stream-set, never an independent dimension of the enumeration, and `Zenith_GetRasterStreamSet` selects it. The vertex input of every permutation is the program's one struct: a pool code's specialised table binds that struct's `POSITION` to the null stream (row "null stream"), and the `bPERM_SKINNED = true` branch of `Flux_TransformObjectToClip` ignores `m_xPosition` and fetches position and QTangent through `Flux_FetchPoolVertex` with `uFLUX_STREAMSET_POSITION` (`Zenith2/Docs/Design/GEOM/Skinning.md → "Skinned vertex pool layout"`) |
| specialised table | for each drawable code, the generated header emits the attributes the program declares that the code holds, copied with their formats, offsets, strides and locations from the code's reflected declaration (a substituted surface as the row above states), then the null-stream attributes, and the declaration's layout hash (the null stream and the attribute subset do not enter it): `axFLUX_<PROGRAM>_VERTEX_ATTRIBUTES_<CODE>` and `axFLUX_<PROGRAM>_VERTEX_STREAMS_<CODE>` (`<CODE>` four upper-case hex digits), plus `axFLUX_<PROGRAM>_VERTEX_LAYOUTS[]`, an array of `Zenith_StreamSetLayoutEntry` in ascending code order; the unsuffixed tables of `Zenith2/Docs/Rendering/VertexData.md → "Generated layout tables"` remain the family default |
| link-time constants | each compiled stream-set sets `uFLUX_STREAMSET_FAMILY`, `uFLUX_STREAMSET_POSITION`, `uFLUX_STREAMSET_SURFACE`, `bFLUX_STREAMSET_UV0`, `bFLUX_STREAMSET_UV1`, `bFLUX_STREAMSET_COLOUR` (Slang link-time constants read by the family decode functions), so a program's source is written once. `Flux_DecodeUV0`, `Flux_DecodeUV1` and the colour read return the null stream's zero without applying scale and bias when their constant is false, so a material sees (0, 0) and (0, 0, 0, 0) exactly |
| engine build | the specialised tables cover every drawable declared code (they are small constant arrays: at most 110 codes per `Flux_MeshVertex` program (the 72 unskinned and 36 pool MESH codes and the 2 PROCEDURAL codes; never the 144 cooked skinned codes) of at most 7 attributes and 5 streams, under 12 KiB); engine raster programs' blobs are compiled for the codes listed in `Zenith2/Shaders/StreamSets.list` (one canonical name per line: the default of each family, the pool forms, and the stream-sets of engine content; a name that is not declared is `ZSC_ERROR_STREAM_SET_NAME`) |
| cook | the cook writes `stream_sets.json` (every raster code the game's cooked meshes use) and the shader compiler compiles engine and material raster program blobs for exactly the union of that set and `StreamSets.list`; a declared code content uses that no program can draw is `GEOM_COOK_ERROR_STREAM_SET_UNDRAWABLE` |

<!-- assumption: pending RPROD -->
`ZenithShaderCompiler` accepts `--stream-set-list <file>` (cook mode, the `stream_sets.json` above), emits the specialised tables of the table above, and sets the listed link-time constants; `Zenith2/Docs/Rendering/ShaderSystem.md → "The ZenithShaderCompiler tool"` gains the flag and "Generated parameter headers" the table rows in the diff that implements this rule.
<!-- /assumption -->

## Position decode invariance

The opaque colour passes test depth `EQUAL` against the prepass ([Z2-N-REND-722]); two programs that decode a position through different instruction sequences can produce different depths and fail that test.

- Every raster program computes clip position exclusively through `Flux_TransformObjectToClip(Flux_MeshVertexInput, Flux_InstanceRecord, Flux_PrimitiveRecord, float4x4 xViewProjection)` of `MeshVertex.slang`, whose body is declared `precise`: dequantisation in kernel order, then `mul(xLocalToWorld, p)` and `mul(xViewProjection, pWorld)` with fixed evaluation order.
- The skinned-pool path (`POOL16`, `POOL32`: the `bPERM_SKINNED = true` permutation, whose specialised table binds `POSITION` to the null stream and whose `Flux_TransformObjectToClip` branch ignores that zero and takes the position from `Flux_FetchPoolVertex`), the terrain path and the procedural path (fixed-function streams 0 and 1 from the procedural pool) use the same function after their own position fetch, so every depth-writing and depth-testing pass of one instance produces bit-identical `SV_Position`.
- `Test_Vertex_DepthEqualAcrossPasses` renders one mesh per stream-set family through the prepass and the opaque pass and asserts zero pixels fail the `EQUAL` test.

## Decode-route consistency

<!-- elaboration -->
A vertex attribute reaches a shader by one of two routes: the fixed-function fetch unit (raster passes) or a manual decode through `VertexFormats.slang` (compute and mesh-shader passes, and vertex pulling). The CPU and GPU codec halves agree bit for bit; the fetch unit's conversion is specified by each API only to a tolerance.

| Rule | Consequence |
|---|---|
| every pass that must agree bit for bit with another pass on the same vertex uses the same route | the prepass, opaque, G-buffer and shadow passes all fetch through the fixed-function unit; the visibility-buffer raster uses the manual route in both its mesh-shader and its fallback form, so `Test_HiZ_MeshletFallbackMatchesMeshShader` compares like with like |
| the material resolve reconstructs attributes through the manual route | it never needs to match the fetch unit, because the visibility buffer draws no fixed-function pass of the same instance |
| the fetch unit's agreement with the manual route is measured, not assumed | `Test_VertexFormat_FetchUnitParity` renders every format through both routes on each CI and reference device and records per format the count of differing lanes and the largest difference in ULP in the test's result record |

<!-- assumption: pending RPROD -->
`Zenith2/Docs/Rendering.md → "Binding model"` ([Z2-N-REND-074]) states that `VertexFormats.slang` matches the fixed-function conversion bit for bit; no API guarantees that. GEOM asks RPROD to restate [Z2-N-REND-074] as the decode-route consistency rule above (recorded in the cross-area notes); until then GEOM implements the rule and reports the parity measurement.
<!-- /assumption -->

## The GPU codec modules

| Module (`Zenith2/Shaders/Common/`) | Contents | CPU counterpart |
|---|---|---|
| `StreamSetFields.slang`, `StreamSets.slang` | the segment macros, and the generator expanding one `[StreamSet(<code>)]` vertex-input struct per reachable stream-set of the MESH, TERRAIN and PROCEDURAL families ("Stream-set declarations in Slang") | none: C++ reads the generated `Flux_StreamSets.generated.h` |
| `VertexKernel.slang` | `Flux_VfClean`, `Flux_VfAdd`, `Flux_VfSub`, `Flux_VfMul`, `Flux_VfMin`, `Flux_VfMax`, `Flux_VfAbs`, `Flux_VfNeg`, `Flux_VfFloor`, `Flux_VfRoundHalfAwayToInt`, `Flux_VfRcp`, `Flux_VfRsqrt`, `Flux_VfSqrt`, `Flux_VfDiv`, `Flux_VfDot3`, `Flux_VfDot4`, `Flux_VfCross`, `Flux_VfNormalize3`, `Flux_VfNormalize4`, every one `precise` | `Zenith_VertexKernel.h` |
| `Half.slang` | `Flux_F32ToF16Bits`, `Flux_F16BitsToF32` (integer only) | `Zenith_F32ToF16`, `Zenith_F16ToF32` |
| `Octahedral.slang` | `Flux_EncodeOctahedral(float3) → uint`, `Flux_DecodeOctahedral(uint) → float3`, plus the render-target helpers `Flux_OctToUnorm(float3) → float2` and `Flux_UnormToOct(float2) → float3` (not codec functions) | `Zenith_EncodeOctahedral`, `Zenith_DecodeOctahedral` |
| `VertexFormats.slang` | `Flux_DecodeSnorm16x4(uint2) → float4`, `Flux_DecodeSnorm16x2(uint) → float2`, `Flux_DecodeUnorm16x2(uint) → float2`, `Flux_DecodeUnorm8x4(uint) → float4`, `Flux_DecodeUint8x4(uint) → uint4`, `Flux_DecodeUint16x4(uint2) → uint4`, `Flux_DecodeUnorm16x4(uint2) → float4`, `Flux_DecodeFloat16x2(uint) → float2`, `Flux_DecodeFloat16x4(uint2) → float4`, the matching encoders `Flux_EncodeSnorm16x4(float4) → uint2` and so on, `Flux_EncodeQTangent(float3 t, float3 n, float fSign) → uint2`, `Flux_DecodeQTangent(uint2) → Flux_TangentFrame`, `Flux_LoadIndex16(ByteAddressBuffer, uint uIndex) → uint` (the halfword select of [Z2-N-REND-890]) | `Zenith_VertexFormat.h` |
| `MeshVertex.slang` | the family structs `Flux_MeshVertex` (Rendering's declaration: POSITION, QTANGENT, TEXCOORD0, plus `[Stream(2)] [VtxFmt("unorm16x2")] float2 m_xUV1 : TEXCOORD1;` and `[Stream(2)] [VtxFmt("unorm8x4")] float4 m_xColour : COLOR0;`, which lightmapped and vertex-colour materials read; the null stream supplies them whenever a code lacks them; the two fields are requested from Rendering's declaration as cross-area row GM24), `Flux_TerrainVertex` (exactly `[Stream(0)] [VtxFmt("snorm16x4")] float4 m_xPosition : POSITION;`) and `Flux_MeshVertexSkin` (Rendering's stream-3 struct: the compute skinning and morph passes' view of stream 3, read through the manual route; it is never a raster vertex input); `Flux_DecodeSurface` (QTangent or octahedral per `uFLUX_STREAMSET_SURFACE`); `Flux_DecodeUV0`, `Flux_DecodeUV1`; `Flux_FetchPoolVertex(ByteAddressBuffer, uint uGranule, uint uVertex, uint uPosition) → Flux_PoolVertex`, which reads vertex `uVertex` of the slot starting at granule `uGranule` at byte `uGranule × 16 + uVertex × 16` (FLOAT16 layout, `uPosition` = `POOL16`) or `uGranule × 16 + uVertex × 24` (FLOAT32 layout, `POOL32`; `Zenith2/Docs/Design/GEOM/Skinning.md → "Skinned vertex pool layout"`); `Flux_TransformObjectToClip` ("Position decode invariance"; under `bPERM_SKINNED = true` it ignores `m_xPosition`, which the null stream supplies, and fetches the pool vertex with `uFLUX_STREAMSET_POSITION`) | — |

- Lane packing in the GPU half follows the byte image of "Scalar quantisation codecs": `uint2` lanes are read from byte-address buffers as two 32-bit words, lane 0 in the low 16 bits of word 0.
- `Flux_EncodeQTangent` implements `EncodeQTangentFromFrame`; the cook-only entry `EncodeQTangent` has no GPU form, because no GPU pass starts from a signed bitangent.

## Vertex format tests

Every test below is in `Zenith2/Engine/ZenithVertexFormat/Tests/` unless its row names another directory; GPU cases run through the compute test harness of `ZenithFlux_Tests` on WARP and lavapipe (and, as `needs-human` device runs, on the RTX 3070 and the S24).

<!-- elaboration -->
| Test | File | Asserts |
|---|---|---|
| `Test_VertexKernel_RoundHalfAwayEdges` | `Zenith_VertexKernel_Tests.cpp` | 0.49999997, 0.5, 1.5, −0.5, −2.5, 32766.5 and every value of the form k + 0.5 for k < 2^22 round away from zero; the `floor(x + 0.5)` counter-example rounds correctly |
| `Test_VertexKernel_RcpAccuracy` | same | `Zenith_VfRcp` within 2 ULP of the correctly rounded reciprocal over [1, 4) exhaustively and 10^6 fixed samples of [2^-40, 2^40] |
| `Test_VertexKernel_RsqrtAccuracy` | same | as above for `Zenith_VfRsqrt` |
| `Test_VertexKernel_CleanNeverSubnormal` | same | for 10^6 fixed random operand pairs of cleaned values, no result of `VfAdd`, `VfSub`, `VfMul` is subnormal and `-0.0` never appears |
| `Test_VertexKernel_SeedsInRange` | same | the `VfRcp` and `VfRsqrt` seeds of 10^6 fixed x in [2^-40, 2^40], and of both range ends, are finite and at least 2^-41, and no Newton step on them forms a subnormal |
| `Test_VertexKernel_NoContraction` | same | `VfAdd(VfMul(a, b), c)` equals the two-step result, not `std::fma(a, b, c)`, for the inputs where they differ |
| `Test_VertexKernel_DeviceArithmeticConformance` | `Zenith2/Engine/ZenithFlux/Tests/VertexFormat/Flux_VertexKernelGpu_Tests.cpp` | on the device, binary32 add, sub and mul of 10^6 fixed operand pairs equal the CPU's bits |
| `Test_VertexFormatErrors_ToStringEveryEnumerator` | `Zenith_VertexFormat_Tests.cpp` | the eleven `Zenith_VertexFormatError` strings are non-null, distinct and differ from the out-of-range string (the Flux enumerations are `Test_GeomErrors_ToStringEveryEnumerator`, `Zenith2/Docs/Design/GEOM.md → "Test catalogue index"`) |
| `Test_VertexFormat_RoundTripEveryFormat` | `Zenith_VertexFormat_Tests.cpp` | every format of the vocabulary round-trips within half a quantum ([Z2-N-REND-895]) |
| `Test_VertexFormat_ScalarCodecsExhaustive` | same | every SNORM16, UNORM16, SNORM8 and UNORM8 code decodes and re-encodes to itself |
| `Test_VertexFormat_CpuGpuBitMatch` | `Flux_VertexKernelGpu_Tests.cpp` | 1,000,000 fixed random inputs per format and per codec function (encoders and decoders, QTangent and octahedral included) give identical bits on the CPU and the GPU |
| `Test_VertexFormat_QTangentBiasRule` | `Zenith_VertexFormat_Tests.cpp` | inputs with `w` = 0, just below 1/32767 and mirrored frames decode with the correct sign, a stored `w` lane whose code has magnitude at least 1, and a normal equal to `cross(t, b)` |
| `Test_VertexFormat_QTangentNormalNeverFlipped` | same | a mirrored frame decodes to the unmirrored frame's normal |
| `Test_VertexFormat_QTangentDegenerateTangent` | same | a tangent parallel to the normal, and a zero tangent, encode through the fallback axis and decode to an orthonormal frame |
| `Test_VertexFormat_QTangentReferenceVectors` | same | 64 committed frames (`Tests/Fixtures/QTangentVectors.json`) encode to their committed codes, so an unintended change of the operation sequence fails |
| `Test_VertexFormat_OctahedralEdges` | same | poles, equator and the four fold creases round-trip within 1e-4 radians |
| `Test_VertexFormat_HalfExhaustive` | same | `F16ToF32` of all 65,536 patterns and `F32ToF16` of every float adjacent to a rounding boundary equal an IEEE reference; subnormal binary32 inputs give signed zero |
| `Test_VertexFormat_WeightsSumExact` | same | 10^5 fixed weight sets quantise to sums of exactly 255 and 65535 |
| `Test_VertexFormat_NonFiniteRejected` | same | NaN or infinity in any lane returns `VERTEX_FORMAT_ERROR_NON_FINITE_INPUT` |
| `Test_VertexFormat_LayoutValidation` | `Zenith_VertexLayout_Tests.cpp` | one failing fixture per row of "Layout validation and hashing" returns its error |
| `Test_VertexFormat_LayoutHashCanonical` | same | reordering attribute declarations with identical content gives the same hash; any field change gives a different hash |
| `Test_StreamSet_NameRoundTripEveryCode` | `Zenith_VertexStreamSet_Tests.cpp` | every valid code formats and parses back to itself; every reserved value is rejected |
| `Test_StreamSet_FindLayout` | same | `Zenith_FindStreamSetLayout` finds every entry of a fixture table sorted by code, returns `VERTEX_FORMAT_ERROR_UNDECLARED_STREAM_SET` for a code between entries and beyond both ends |
| `Test_StreamSet_CheckDeclaration` | same | `Zenith_CheckStreamSetDeclaration` accepts a fixture layout holding exactly the code's attributes and rejects, with `VERTEX_FORMAT_ERROR_BAD_LAYOUT`, a missing attribute, an extra attribute and a wrong format, one fixture each |
| `Test_StreamSet_DeclarationsMatchOracle` | `Zenith2/Tools/ZenithShaderCompiler/Tests/Zsc_StreamSets_Tests.cpp` | every entry of the generated `axFLUX_STREAMSET_LAYOUTS` equals the test-only oracle's layout for its code ("Canonical stream-set layouts"); the table holds 255 MESH, TERRAIN and PROCEDURAL entries (72 + 144 + 36 + 1 + 2 by group), exactly the codes `Zenith_IsReachableStreamSet` accepts; and the table is sorted by code |
| `Test_StreamSet_EveryFallbackDeclared` | same | (a) for every declared code c and every row of "Fallback derivation rule" that applies to c's family and fields, the code with that row's field change is declared (for example `pos-snorm16x4|surface-qt16-uv16|opt-uv1-uv16|skin-j8w8` → UV1F gives `…|opt-uv1-uvf16|skin-j8w8`, → W16 gives `…|skin-j8w16`, → the posed-extent row gives `pos-float32x3|…` with raster form `pos-pool32x4|…`); (b) the skinned character of the fixtures with a FLOAT16 UV0, and a lightmapped prop with a FLOAT16 UV1, each have their chosen code in the table; (c) the terrain and both procedural codes are declared |
| `Test_StreamSet_ReachableRule` | `Zenith_VertexStreamSet_Tests.cpp` | over all 65,536 code values, `Zenith_IsReachableStreamSet` accepts exactly 255, every one valid; it rejects `pos-snorm16x4|surface-oct16-uv16|skin-j8w8` (octahedral skinned), a pool code with SKIN, `terrain:pos-snorm16x4|surface-none-uv16`, a MESH code with SURF `NONE` and every SPRITE, DEBUG and UI code |
| `Test_StreamSet_DefaultPerFamily` | same | `Zenith_GetDefaultStreamSet` returns `0x0010`, `0x1008` and `0x2010` for MESH, TERRAIN and PROCEDURAL, each reachable and named as the defaults table states, `0x3001` (`sprite:canvas`, valid and not reachable) for SPRITE, and `VERTEX_FORMAT_ERROR_BAD_STREAM_SET` for DEBUG and UI |
| `Test_StreamSet_OwnerFamilyCodes`, `Test_StreamSet_OwnerDeclarationShape` | same | "Owner-family stream-sets" |
| `Test_StreamSet_HeaderDeterministic` | `Zsc_StreamSets_Tests.cpp` | two runs of `--stream-sets` over the same `Common/` tree, one with the directory listing reversed, write byte-identical headers whose arrays ascend by code |
| `Test_StreamSet_DeclarationErrors` | same | one fixture module per compiler rule of "Stream-set declarations in Slang" fails with its code: `ZSC_ERROR_STREAM_SET_CODE` (argument 0x8000), `_MISMATCH` (a generator macro emitting UNORM16X2 for `UVF16`), `_DUPLICATE`, `_LOCATION`, `_MISSING` (one generator line deleted, naming the first missing canonical name), `_UNREACHABLE` (an octahedral skinned struct added), `_NAME` (an undeclared name in `StreamSets.list`); and the stream-7 rule: a SPRITE fixture struct with a `[Stream(7)]` field is `_LOCATION`, and a layout with stride 4 on stream 7 fails `Zenith_ValidateVertexLayout` with `BAD_LAYOUT` |
| `Test_StreamSet_RasterForm` | `Zenith_VertexStreamSet_Tests.cpp` | skinned codes map to pool codes with SKIN cleared, `POOL32` exactly when the cooked POS is `FLOAT32X3`; unskinned codes are unchanged |
| `Test_VertexFormat_PackerMatchesLayout` | `Zenith_VertexPacker_Tests.cpp` | `Zenith_PackVertexStreams`, given each entry of the generated `axFLUX_STREAMSET_LAYOUTS` of the MESH family, writes each attribute at the entry's offset and stride |
| `Test_VertexFormat_FetchUnitParity` | `Flux_VertexKernelGpu_Tests.cpp` | records, per format and device, the lanes where fixed-function fetch and the manual decode differ (pass condition: the record is written; the counts are evidence) |
| `Test_Vertex_DepthEqualAcrossPasses` | `Zenith2/Engine/ZenithFlux/Tests/VertexFormat/Flux_VertexDecode_Tests.cpp` | zero `EQUAL`-test failures between prepass and opaque for each family |
| `Test_StreamSet_NullStreamBinding` | `Zenith2/Engine/ZenithFlux/Tests/VertexFormat/Flux_VertexDecode_Tests.cpp` | a fixture MESH program that declares UV0, UV1 and COLOR0 and writes them to colour, drawn with `pos-snorm16x4|surface-oct16` on Vulkan (lavapipe) and D3D12 (WARP): the specialised table has attributes at stream 7, stride 0, offsets 0, 4 and 8; the draw binds the 32-byte zero buffer there; every pixel reads UV0 = UV1 = (0, 0) and colour (0, 0, 0, 0); validation is clean on both backends. The same program drawn with `pos-snorm16x4|surface-qt16-uv16|opt-uv1-uv16-col8` reads the stored values and binds no null stream. Pool case: the program's `bPERM_SKINNED = true` permutation drawn with `pos-pool16x4|surface-qt16-uv16` has POSITION (SNORM16X4) at stream 7 offset 0, UV1 at 8 and COLOR0 at 12, no stream 0 in either backend's input state, binds the 32-byte zero buffer at binding 7, creates its PSO with validation clean on both backends, and draws the skinned fixture at the positions `Flux_FetchPoolVertex` returns (zero pixels differ from the skinned reference) |
| `Test_Vertex_LayoutMismatchRejected` | `Zenith2/Engine/ZenithFlux/Tests/VertexFormat/Flux_VertexDecode_Tests.cpp` | binding a mesh whose layout hash differs from the program's fails with `FLUX_VERTEX_ERROR_LAYOUT_MISMATCH` ([Z2-N-REND-895]) |
| fuzz `Zenith_Fuzz_StreamSetName` | `Tests/Fuzz/StreamSetName.cpp` | arbitrary bytes into `Zenith_ParseStreamSetName` never crash and never return a code that fails validation |
