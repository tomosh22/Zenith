# Zenith2 rendering: vertex data format

Section file of `Zenith2/Docs/Rendering.md`. It details the entry-file section "Vertex data format". Normative statements share the `REND` tag family of the entry file. These are universal rules for every platform, after Android's vertex-data guidance (Z2-D-062). The CPU codec lives in the leaf library `ZenithVertexFormat` (`Zenith2/Engine/ZenithVertexFormat/`, owned by GEOM, depending on ZenithCore alone); the GPU half is `Zenith2/Shaders/Common/VertexFormats.slang`.

## Vertex declaration in Slang

Vertex layouts are declared in Slang and nowhere else, which is Zenith's model carried over. A vertex-input struct's field declares its semantic and its stream.

```slang
// Zenith2/Shaders/Common/MeshVertex.slang
struct Flux_MeshVertex
{
	[Stream(0)] [VtxFmt("snorm16x4")] float4 m_xPosition : POSITION;     // dequantised with the primitive's scale and bias
	[Stream(1)] [VtxFmt("snorm16x4")] float4 m_xQTangent : QTANGENT;
	[Stream(1)] [VtxFmt("unorm16x2")] float2 m_xUV0 : TEXCOORD0;         // remapped with the primitive's UV0 scale and bias
	[Stream(2)] [VtxFmt("unorm16x2")] float2 m_xUV1 : TEXCOORD1;         // lightmapped materials; the null stream when the code has no UV1
	[Stream(2)] [VtxFmt("unorm8x4")] float4 m_xColour : COLOR0;          // vertex-colour materials; the null stream when the code has no colour
};

struct Flux_MeshVertexSkin
{
	[Stream(3)] [VtxFmt("uint8x4")] uint4 m_xJoints : JOINTS0;
	[Stream(3)] [VtxFmt("unorm8x4")] float4 m_xWeights : WEIGHTS0;
};

// Per-instance stream 4 of 2D sprites is TWOD's SPRITE stream-set family, declared in
// Zenith2/Shaders/Common/SpriteStreamSets.slang and generated into Flux_StreamSets.generated.h: the three stream-sets
// 0x3001 (sprite:canvas, 40-byte instances), 0x3002 (sprite:world, 48 bytes) and 0x3003 (sprite:tilechunk, 36 bytes),
// stream 4 at locations 8–13 (Zenith2/Docs/Design/TWOD/Rendering.md → "Sprite stream-sets").
```

| `[VtxFmt]` string | Storage | Bytes | Declared Slang type |
|---|---|---|---|
| `float32` | 32-bit float | 4 | `float` |
| `float32x2` | 2 × 32-bit float | 8 | `float2` |
| `float32x3` | 3 × 32-bit float | 12 | `float3` |
| `float32x4` | 4 × 32-bit float | 16 | `float4` |
| `float16x2` | 2 × half | 4 | `float2` |
| `float16x4` | 4 × half | 8 | `float4` |
| `snorm16x2` | 2 × signed normalised 16-bit | 4 | `float2` |
| `snorm16x4` | 4 × signed normalised 16-bit | 8 | `float4` |
| `unorm16x2` | 2 × unsigned normalised 16-bit | 4 | `float2` |
| `unorm16x4` | 4 × unsigned normalised 16-bit | 8 | `float4` |
| `unorm8x4` | 4 × unsigned normalised 8-bit | 4 | `float4` |
| `snorm8x4` | 4 × signed normalised 8-bit | 4 | `float4` |
| `uint8x4` | 4 × unsigned 8-bit integer | 4 | `uint4` |
| `uint16x4` | 4 × unsigned 16-bit integer | 8 | `uint4` |
| `uint32` | 32-bit unsigned integer | 4 | `uint` |

| Semantic | Meaning |
|---|---|
| `POSITION` | object-space position |
| `QTANGENT` | the tangent frame quaternion |
| `NORMAL_OCT` | octahedral normal (normal-only variant) |
| `TEXCOORD0`, `TEXCOORD1` | UV0, UV1 (lightmap) |
| `COLOR0` | vertex colour |
| `JOINTS0`, `WEIGHTS0` | skinning |
| `INSTANCE_RECT`, `INSTANCE_COLOR`, `INSTANCE_DATA0`–`INSTANCE_DATA3` | per-instance streams of non-GPU-scene draws (2D sprites, debug geometry, UI) |

- [Z2-N-REND-868] `[VtxFmt("…")]` gives the storage format wherever it differs from the declared type, and the fetch unit widens and converts, so a compressed field is still declared `float4`; a field without `[VtxFmt]` stores its declared 32-bit type.
- [Z2-N-REND-869] `[Stream(n)]` places a field in stream n (0–7) and `[PerInstance]` puts it on an instance-rate stream; every field of one stream shares its rate, and fields of a stream are packed in declaration order at 4-byte alignment.
- [Z2-N-REND-870] The `[VtxFmt]` strings and the semantics are closed vocabularies, exactly the two tables above; any other string, or a lane-count mismatch between the storage format and the declared type, is a shader compile error (`ZSC_ERROR_VERTEX_FORMAT`), reported by the `ShaderChecks.VertexFormatVocabulary` lint.

## Generated layout tables

The offline `ZenithShaderCompiler` reflects each vertex struct and bakes the layout into the program's generated header: per-attribute semantic, format, stream and offset, plus per-stream stride. The table types live in `Zenith_VertexFormat.h`.

```cpp
// Zenith2/Engine/ZenithVertexFormat/Zenith_VertexFormat.h (public root header)
enum Zenith_VertexFormatType : uint8
{
	ZENITH_VERTEX_FORMAT_FLOAT32, ZENITH_VERTEX_FORMAT_FLOAT32X2, ZENITH_VERTEX_FORMAT_FLOAT32X3, ZENITH_VERTEX_FORMAT_FLOAT32X4,
	ZENITH_VERTEX_FORMAT_FLOAT16X2, ZENITH_VERTEX_FORMAT_FLOAT16X4,
	ZENITH_VERTEX_FORMAT_SNORM16X2, ZENITH_VERTEX_FORMAT_SNORM16X4, ZENITH_VERTEX_FORMAT_UNORM16X2, ZENITH_VERTEX_FORMAT_UNORM16X4,
	ZENITH_VERTEX_FORMAT_UNORM8X4, ZENITH_VERTEX_FORMAT_SNORM8X4, ZENITH_VERTEX_FORMAT_UINT8X4, ZENITH_VERTEX_FORMAT_UINT16X4,
	ZENITH_VERTEX_FORMAT_UINT32,
	ZENITH_VERTEX_FORMAT_COUNT
};

enum Zenith_VertexSemantic : uint8
{
	ZENITH_VERTEX_SEMANTIC_POSITION, ZENITH_VERTEX_SEMANTIC_QTANGENT, ZENITH_VERTEX_SEMANTIC_NORMAL_OCT,
	ZENITH_VERTEX_SEMANTIC_TEXCOORD, ZENITH_VERTEX_SEMANTIC_COLOR, ZENITH_VERTEX_SEMANTIC_JOINTS, ZENITH_VERTEX_SEMANTIC_WEIGHTS,
	ZENITH_VERTEX_SEMANTIC_INSTANCE_RECT, ZENITH_VERTEX_SEMANTIC_INSTANCE_COLOR, ZENITH_VERTEX_SEMANTIC_INSTANCE_DATA,
	ZENITH_VERTEX_SEMANTIC_COUNT
};

enum Zenith_VertexInputRate : uint8 { ZENITH_VERTEX_INPUT_RATE_VERTEX, ZENITH_VERTEX_INPUT_RATE_INSTANCE };

struct Zenith_VertexAttributeDesc
{
	Zenith_VertexSemantic m_eSemantic;
	uint8 m_uSemanticIndex;
	Zenith_VertexFormatType m_eFormat;
	uint8 m_uStream;
	uint16 m_uOffset;        // byte offset within the stream's element
	uint16 m_uLocation;      // shader input location / D3D12 semantic slot
};

struct Zenith_VertexStreamDesc
{
	uint8 m_uStream;
	Zenith_VertexInputRate m_eRate;
	uint16 m_uStride;
};

struct Zenith_VertexLayoutDesc
{
	const Zenith_VertexAttributeDesc* m_pxAttributes;
	uint32 m_uAttributeCount;
	const Zenith_VertexStreamDesc* m_pxStreams;
	uint32 m_uStreamCount;
	uint64 m_ulLayoutHash;   // hash over the attribute and stream tables; the stream-set identity
};

uint32 Zenith_GetVertexFormatBytes(Zenith_VertexFormatType eFormat);
```

```cpp
// generated, in <Program>.generated.h
inline constexpr Zenith_VertexAttributeDesc axFLUX_FORWARDPLUSOPAQUE_VERTEX_ATTRIBUTES[] =
{
	{ ZENITH_VERTEX_SEMANTIC_POSITION, 0, ZENITH_VERTEX_FORMAT_SNORM16X4, 0, 0, 0 },
	{ ZENITH_VERTEX_SEMANTIC_QTANGENT, 0, ZENITH_VERTEX_FORMAT_SNORM16X4, 1, 0, 1 },
	{ ZENITH_VERTEX_SEMANTIC_TEXCOORD, 0, ZENITH_VERTEX_FORMAT_UNORM16X2, 1, 8, 2 },
};
inline constexpr Zenith_VertexStreamDesc axFLUX_FORWARDPLUSOPAQUE_VERTEX_STREAMS[] =
{
	{ 0, ZENITH_VERTEX_INPUT_RATE_VERTEX, 8 },
	{ 1, ZENITH_VERTEX_INPUT_RATE_VERTEX, 12 },
};
```

- [Z2-N-REND-871] Runtime PSO creation builds the Vulkan `VkPipelineVertexInputStateCreateInfo` and the D3D12 input layout exclusively from the program's generated layout table, because the runtime has no Slang.
- [Z2-N-REND-872] The cook's vertex packers are driven by the same generated tables (`axFLUX_STREAMSET_LAYOUTS` of `Flux_StreamSets.generated.h`, so ZenithCook links `ZenithShadersGenerated` and the build orders the `--stream-sets` command before ZenithCook's compile), so the writer and the reader cannot disagree; a cooked mesh records the layout hash it was packed for, and the runtime refuses to bind a mesh whose recorded layout hash differs from the program's (`FLUX_VERTEX_ERROR_LAYOUT_MISMATCH`).
- [Z2-N-REND-873] `ZenithVertexFormat` exports the CPU codec (quantise and pack, unpack, QTangent encode and decode, octahedral, half) and the generated layout-table types through its public root header `Zenith_VertexFormat.h`; Flux (runtime), ZenithCook and ZenithEditor all link it, none of them reaches into another library's `Internal/`, and libgraph enforces that.
- [Z2-N-REND-874] The GPU half `Zenith2/Shaders/Common/VertexFormats.slang` holds the manual decoders and encoders for passes with no fetch unit (compute skinning, the visibility-buffer resolve), and matches the CPU codec bit for bit for every format.

## Default stream layout

The default mesh family:

| Stream | Content | Format | Bytes |
|---|---|---|---|
| 0: position (dedicated) | position, quantised to the mesh's tight AABB | SNORM16x4 (w stored as 0), dequantised by a per-mesh scale and bias from the GPU scene primitive record. FLOAT32x3 only if the cook's precision check (max error against the source, relative to the mesh extent) fails, which is recorded per mesh | 8 (12) |
| 1: surface | tangent frame as a QTangent (a unit quaternion for the tangent-space rotation) | SNORM16x4; see "QTangent codec". That is the same 8 bytes as Zenith's two SNORM10:10:10:2 words: a quality gain, not a size gain | 8 |
| 1 | UV0 | UNORM16x2. Any range is remapped by the cook into [0,1] with a per-mesh UV scale and bias, applied in the vertex shader. FLOAT16x2 only if the precision check fails | 4 |
| 1 (normal-only variant) | a normal for meshes without normal maps | octahedral SNORM16x2, replacing the QTangent | 4 |
| 2: optional | UV1 (lightmap) UNORM16x2; vertex colour UNORM8x4 | as listed; UV1 has its own scale and bias | 4 / 4 |
| 3: skin | joint indices; weights | UINT8x4 (UINT16x4 only if > 256 joints); UNORM8x4 (UNORM16x4 only if the cook's error check fails) | 4–8 / 4–8 |

| Stream | Default stride | Fallback strides |
|---|---|---|
| 0 | 8 | 12 (FLOAT32x3 position) |
| 1 | 12 (QTangent + UV0) | 8 (octahedral normal + UV0); a FLOAT16x2 UV0 keeps the stride at 12 |
| 2 | 4 or 8 (UV1 and/or colour) | — |
| 3 | 8 (UINT8x4 + UNORM8x4) | 12 or 16 (UINT16x4 and/or UNORM16x4) |

- [Z2-N-REND-875] Every mesh of the default family uses the streams and formats of the table above, with position alone in stream 0, and a 32-bit vertex format appears exclusively where a documented precision check of "Precision checks and fallbacks" requires it; the cook reports every fallback.

## QTangent codec

The tangent `t` and the unsigned bitangent `b` are extracted from the quaternion, and the normal is derived by cross product, `n = cross(t, b)`. It is not sign-flipped, because the quaternion is a proper rotation and mirroring does not change the normal. The handedness sign applies to the bitangent alone: the shading bitangent is `b × sign`, for mirrored UVs. The sign is stored in the sign of `w`: the cook negates `q` so that `w ≥ 0` means no reflection. `abs(w)` is biased to at least one SNORM16 quantum (1/32767), with xyz renormalised, so `w` does not quantise to 0.

```
Encode(t, bSigned, n):                         // unit inputs (MikkTSpace tangent and signed bitangent)
	t     = normalize(t - n * dot(n, t))         // Gram-Schmidt against n
	bU    = cross(n, t)                          // unsigned bitangent: (t, bU, n) is a proper rotation, n = cross(t, bU)
	sign  = dot(bU, bSigned) < 0 ? -1 : +1
	q     = normalize(QuaternionFromBasis(t, bU, n))   // Shepperd's method, columns t, bU, n
	if q.w < 0: q = -q                           // same rotation, now w >= 0
	eps   = 1 / 32767
	if q.w < eps:
		q.w   = eps
		q.xyz = q.xyz * (sqrt(1 - eps * eps) / length(q.xyz))
	if sign < 0: q = -q                          // w < 0 encodes the reflection
	return (int16) round(clamp(q, -1, 1) * 32767)  // per component, round half away from zero

Decode(s):                                     // s: four int16 values
	q     = max(float(s) / 32767, -1)
	sign  = q.w < 0 ? -1 : +1
	q     = normalize(q)                         // q and -q are the same rotation
	t     = (1 - 2(q.y² + q.z²),  2(q.x q.y + q.w q.z),  2(q.x q.z - q.w q.y))
	bU    = (2(q.x q.y - q.w q.z),  1 - 2(q.x² + q.z²),  2(q.y q.z + q.w q.x))
	n     = cross(t, bU)
	bShading = bU * sign
```

- [Z2-N-REND-876] QTangents are encoded and decoded by the pseudocode above as realised in kernel operations by `Zenith2/Docs/Design/GEOM/VertexFormat.md → "The exact arithmetic kernel"` (reciprocal constants, fixed Newton sequences, results cleaned below 2⁻⁴⁰, `precise`) in both codec halves: the normal is `cross(t, b)` and is never sign-flipped, the handedness sign applies to the bitangent alone through the sign of `w`, and `abs(w)` is biased to at least 1/32767 with xyz renormalised before quantisation.

## Octahedral codec

```
EncodeOct(n):                                  // unit n
	p = n.xy / (abs(n.x) + abs(n.y) + abs(n.z))
	if n.z < 0: p = (1 - abs(p.yx)) * SignNotZero(p)   // SignNotZero(v) = v >= 0 ? +1 : -1, per component
	return snorm16x2(p)                          // vertex streams; render targets store p * 0.5 + 0.5 as RG16_UNORM

DecodeOct(p):                                  // p in [-1, 1]^2
	v = (p.x, p.y, 1 - abs(p.x) - abs(p.y))
	k = max(-v.z, 0)
	v.x += v.x >= 0 ? -k : k
	v.y += v.y >= 0 ? -k : k
	return normalize(v)
```

- [Z2-N-REND-877] Octahedral normals use the encoding above, as realised in kernel operations by `Zenith2/Docs/Design/GEOM/VertexFormat.md → "The exact arithmetic kernel"`, in vertex streams (SNORM16x2), the Forward+ and Forward prepass normal target and the G-buffer normal (RG16_UNORM, remapped by `× 0.5 + 0.5`), in both codec halves.

## Half-float codec

The GPU half-float codec uses 32-bit integer arithmetic exclusively, so it needs no `shaderInt16`, as in Zenith. It is round-to-nearest-even and handles zero, subnormals, infinity and NaN.

```
uint F32ToF16Bits(uint f)                      // f: the float's bit pattern
	sign = (f >> 16) & 0x8000
	exp  = (f >> 23) & 0xFF
	mant = f & 0x7FFFFF
	if exp == 0xFF: return sign | 0x7C00 | (mant != 0 ? 0x200 | (mant >> 13) : 0)   // inf or quiet NaN
	e = int(exp) - 112                           // rebias 127 -> 15
	if e >= 31: return sign | 0x7C00             // overflow -> inf
	if e <= 0:                                   // subnormal half or zero
		if e < -10: return sign                  // below half the smallest subnormal -> signed zero
		m     = mant | 0x800000
		shift = 14 - e
		h     = m >> shift
		rem   = m & ((1 << shift) - 1)
		half  = 1 << (shift - 1)
		if rem > half or (rem == half and (h & 1)): h += 1
		return sign | h
	h   = (e << 10) | (mant >> 13)
	rem = mant & 0x1FFF
	if rem > 0x1000 or (rem == 0x1000 and (h & 1)): h += 1   // a carry into the exponent is correct, up to inf
	return sign | h

uint F16BitsToF32(uint h)
	sign = (h & 0x8000) << 16
	exp  = (h >> 10) & 0x1F
	mant = h & 0x3FF
	if exp == 0x1F: return sign | 0x7F800000 | (mant << 13)
	if exp == 0:
		if mant == 0: return sign
		p = firstbithigh(mant)                   // 0..9
		mant = (mant << (10 - p)) & 0x3FF
		return sign | ((p + 103) << 23) | (mant << 13)
	return sign | ((exp + 112) << 23) | (mant << 13)
```

- [Z2-N-REND-878] The half-float codec is the integer-only algorithm above in both halves (`Zenith_F32ToF16`/`Zenith_F16ToF32` on the CPU, `Flux_F32ToF16Bits`/`Flux_F16BitsToF32` in `Half.slang`), and the vertex codec's integer-only half conversion is kept regardless of `shaderFloat16`, so compute codecs never need `shaderInt16`.

## Precision checks and fallbacks

<!-- elaboration -->
| Attribute | Default | Metric the cook computes | Default tolerance (importer preset, per mesh) | Fallback |
|---|---|---|---|---|
| position | SNORM16x4 over the tight AABB | max over vertices of `abs(p_decoded − p_source)`, and that value divided by the mesh extent (AABB diagonal) | relative 2⁻¹⁶ of the extent, or absolute 1 mm, whichever is looser: fails when the error exceeds max(2⁻¹⁶ × extent, 1 mm) | FLOAT32x3 |
| UV0, UV1 | UNORM16x2 over the remapped range | max UV error in texels at the largest texture resolution bound to the mesh's materials | 1/8 texel | FLOAT16x2 when its texel error is lower; if both fail, the lower-error format is kept and the cook warns `COOK_WARNING_UV_PRECISION` |
| joint indices | UINT8x4 | skeleton joint count | ≤ 256 joints | UINT16x4 |
| weights | UNORM8x4, renormalised so the four sum to exactly 255 (the largest weight absorbs the residual) | max over vertices of `Σ_i abs(w_i,quantised − w_i) × L_i`, L_i the bind-pose distance from the vertex to joint i | 1 mm | UNORM16x4 |
| skinned pool position | FLOAT16x4, object space | half-float quantum at the conservative posed extent (see "Skinned vertex pool") | error ≤ 1 mm | FLOAT32x4 pool layout |

<!-- elaboration -->
<!-- informative -->
Derivation for the position row. SNORM16 over the tight AABB has 65,535 levels per axis, so its worst-case error is half a quantum, `axis_extent / 131,070 ≈ 7.6e-6 × axis_extent`, which is at most `7.6e-6 × extent`. The plan's relative check at 2⁻¹⁶ (≈ 1.53e-5) therefore passes for every mesh with about a 2× margin, and the absolute 1 mm term (an elaboration) only ever loosens it. A 200 m terrain chunk or building (error ≈ 1.5 mm, relative 7.6e-6) stays SNORM16x4. The FLOAT32x3 fallback happens when an importer preset tightens both tolerances for a precision-critical asset, for example parts that share seams with separately quantised meshes.
<!-- /informative -->

- [Z2-N-REND-879] The cook runs every precision check of the table above per mesh, applies exactly the listed fallback when a check fails, and writes every fallback (mesh, attribute, metric value, tolerance, chosen format) to the game's cook report under `vertexFallbacks`.
- [Z2-N-REND-880] Every precision fallback (FLOAT32x3 position, FLOAT16 UV, UINT16x4 joints, UNORM16x4 weights, FLOAT32 skinned positions) is a distinct Slang layout, and so a distinct stream-set pool and PSO permutation; the cook reports the permutation count per game, and `Z2-B-RPROD-024` Shader permutation limit per game (pending) caps it.

## Index buffers and mesh splitting

| Step | Action |
|---|---|
| 1 | meshoptimizer vertex-cache optimisation (`meshopt_optimizeVertexCache`) |
| 2 | overdraw optimisation (`meshopt_optimizeOverdraw`, threshold 1.05) |
| 3 | vertex-fetch remapping (`meshopt_optimizeVertexFetchRemap`, then remap every stream) |
| 4 | split: walk triangles in the optimised order, adding each to the current sub-mesh while its unique vertex count stays ≤ 65,535, otherwise starting a new sub-mesh |
| 5 | per sub-mesh, the position-only index buffer (`meshopt_generateShadowIndexBuffer` over the quantised position stream) |
| 6 | meshlets for the visibility-buffer path (`meshopt_buildMeshlets`, at most 64 vertices and 128 triangles per meshlet, 8-bit local indices), with bounds and normal cones (`meshopt_computeMeshletBounds`) |

- [Z2-N-REND-881] Index buffers are 16-bit: the cook splits any mesh with more than 65,535 vertices into sub-meshes by step 4 above, primitive restart is disabled, and there are no 32-bit index buffers anywhere.
- [Z2-N-REND-882] Every sub-mesh shares its parent's position quantisation box and UV scale and bias, so shared boundary vertices decode identically and never crack.
- [Z2-N-REND-883] Meshlets use 8-bit local indices, with at most 64 vertices and 128 triangles per meshlet.
- [Z2-N-REND-884] Position-only draws (shadow views, and the opaque bucket of pure depth-only passes) use the position-only index buffer, which re-indexes each triangle to a canonical vertex per unique position for better post-transform cache hits; it is not used by the Forward+ prepass, which reads stream 1 for normals and velocity, nor by alpha-tested buckets, which need UVs.
- [Z2-N-REND-885] Triangles are wound counter-clockwise when viewed from their front face; the importer keeps glTF's counter-clockwise winding and reverses the index order of any node whose world transform has a negative determinant.

## Unified stream pools

- [Z2-N-REND-886] Every mesh is suballocated into a per-stream unified pool and drawn through `vertexOffset` / `BaseVertexLocation` (int32, so 16-bit indices suffice), which is what GPU-driven indirect draws need; the 16-bit indices of every mesh live in one unified index pool addressed by `m_uFirstIndex`; the pools of every fixed-function-bound stream are keyed by the raster stream-set code, and a skinned mesh's bind-position and skin streams live in a compute-only skin-source pool keyed by its cooked code (SKIN included), addressed by the skinning dispatch record and never bound to a raster draw.
- [Z2-N-REND-887] Indirect buckets are keyed by (pipeline, raster stream-set code) — the code of the PSO's stream-set, `POOL16`/`POOL32` for a skinned mesh — so every draw in one bucket binds the same fixed-function stream pools, and two skinned meshes whose cooked codes differ only in their joint or weight format share a bucket and its raster pools.
- [Z2-N-REND-888] The unified pools are `Flux_PersistentReadOnly`: a new mesh is uploaded on the copy queue into a region no table entry references, and a freed region is reused only after retirement (see `Zenith2/Docs/Rendering/GpuResources.md` "Persistent resources").

## Buffer usage unions

- [Z2-N-REND-889] Mesh and pool buffers are created with the union of their declared uses (for example vertex + index + storage for buffers that the visibility-buffer resolve or skinning also reads or writes), and the union is part of the physical-resource cache key.
- [Z2-N-REND-890] Raster passes fetch through the fixed-function unit; compute passes read the same buffers through bindless storage views, and read 16-bit indices as u32 words with a halfword select (`(word >> ((i & 1) × 16)) & 0xFFFF`).

## Skinned vertex pool

- [Z2-N-REND-891] The skinned-vertex pool is written by compute, and because posed vertices leave the bind-pose box, pool positions are FLOAT16x4 in object space, not the SNORM16 bind-pose quantisation.
- [Z2-N-REND-892] The cook's precision check for the pool uses a conservative posed extent, because the posed extent is unknown at cook time: `E = |bind position of the root joint| + R + D`, where R is the skeleton's maximum reach (the largest root-relative sum of bone-chain lengths over every joint) and D is the largest bind-pose distance from a vertex to its most-weighted joint; the half-float error at magnitude E is `2^(floor(log2(E)) − 11)`, and a mesh whose error exceeds 1 mm gets a FLOAT32x4 pool layout, reported like any other fallback.
- [Z2-N-REND-893] Skinning re-encodes the tangent frame as a QTangent with the bias rule, so the GPU codec has encoders as well as decoders (the QTangent encode with its bias, and half-float packing), both bit-matched to the CPU codec by tests.
- [Z2-N-REND-894] The pool is double-buffered for velocity (see `Zenith2/Docs/Rendering/GpuResources.md` "Pools").

## Codec tests

| Test | Asserts |
|---|---|
| `Test_VertexFormat_RoundTripEveryFormat` | CPU quantise then unpack stays within half a quantum for every format of the vocabulary |
| `Test_VertexFormat_CpuGpuBitMatch` | for 1,000,000 fixed random inputs per format, the CPU codec and `VertexFormats.slang` (run in a compute test harness on WARP and lavapipe) produce identical bits, encoders and decoders both |
| `Test_VertexFormat_QTangentBiasRule` | inputs with `w` = 0, `w` just below 1/32767, and mirrored frames decode with the correct sign and a normal equal to `cross(t, b)` |
| `Test_VertexFormat_QTangentNormalNeverFlipped` | the decoded normal of a mirrored frame equals the unmirrored frame's normal |
| `Test_VertexFormat_OctahedralEdges` | the poles, the equator and the four fold creases round-trip within 1e-4 radians |
| `Test_VertexFormat_HalfExhaustive` | `F16BitsToF32` of every one of the 65,536 patterns, and `F32ToF16Bits` of every float adjacent to a half rounding boundary, equal an IEEE reference |
| `Test_Cook_MeshSplitSharesQuantisation` | a 200,000-vertex mesh splits into sub-meshes whose shared boundary vertices decode bit-identically |
| `Test_Cook_PrecisionFallbacksReported` | fixture meshes that fail each check get the listed fallback and a cook-report entry; a 200 m mesh stays SNORM16x4 under the default tolerances, and the same mesh with a preset that tightens both position tolerances falls back to FLOAT32x3 |
| `Test_Vertex_LayoutMismatchRejected` | binding a mesh with a different layout hash fails with `FLUX_VERTEX_ERROR_LAYOUT_MISMATCH` |

- [Z2-N-REND-895] The codec tests of the table above exist (CPU tests in `Zenith2/Engine/ZenithVertexFormat/Tests/`, cook tests in `Zenith2/Tools/ZenithCook/Tests/`, Flux tests in `Zenith2/Engine/ZenithFlux/Tests/`) and pass; every format of the vocabulary is round-tripped across both codec halves.

## D-VTX-TANGENTLESS harness

D-VTX-TANGENTLESS (open, Stage 3) is an experiment, run as a Stage 3 harness ticket plus a `needs-human` ruling ticket (Z2-D-084). QTangent is the default until the ruling.

| Element | Specification |
|---|---|
| Hypothesis | store only an octahedral normal (2 components, SNORM16x2) and derive the tangent and bitangent per pixel from screen-space derivatives of position and UV (Schüler's normal mapping without precomputed tangents, and Mikkelsen's surface-gradient framework); normal mapping then needs 2 normal components in the vertex buffer |
| Harness location | `Zenith2/Benchmarks/VertexTangentless/`, with a CTest smoke run (one asset, one frame, both variants) in the normal Test step |
| Variant | a development-only `bPERM_TANGENTLESS` permutation of the Forward+ opaque, G-buffer and visibility-buffer resolve programs, fed by the normal-only stream-1 variant |
| Asset set | Khronos glTF-Sample-Assets NormalTangentTest, NormalTangentMirrorTest, DamagedHelmet and Sponza; the W-Characters character set; the W-Outdoor terrain material set |
| Risk 1 | mismatch with normal maps baked in MikkTSpace: FLIP mean and 99th percentile of each asset against the QTangent reference |
| Risk 2 | derivative artefacts at triangle and quad edges, and in the visibility-buffer path (which uses analytic gradients): FLIP 99th percentile restricted to pixels within 2 px of a triangle edge, from a triangle-ID debug output |
| Risk 3 | ALU cost versus bandwidth saved, on the S24 and the RTX 3070: opaque-pass GPU time, vertex-fetch bytes per frame (stream 1 shrinks from 12 to 8 bytes) and the vendor bandwidth counters |
| Procedure | the measurement procedure of "Shading path research methodology" in the entry file |
| Report | `Zenith2/Docs/Research/VertexTangentless/<date>/report.json` plus raw CSV: per asset, per path, per platform, every metric above with medians and 95% confidence intervals |
| Ruling | adopt per quality tier, per material class, or not at all |

- [Z2-N-REND-896] The D-VTX-TANGENTLESS (open, Stage 3) harness implements every element of the table above, measures the three risks on the stated asset set on the S24 and the RTX 3070, and changes no default: QTangent stays the default until the ruling is recorded as a decision.
