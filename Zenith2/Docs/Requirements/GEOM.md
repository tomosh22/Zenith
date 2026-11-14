# GEOM requirement catalogue

## Purpose

This catalogue breaks the GEOM inventory items (`Zenith2/Docs/Requirements/Inventory.md → "Inventory v1"`, section "GEOM Geometry & materials") into testable requirements `Z2-R-GEOM-NNN`. Each requirement names the inventory items it covers, the stage whose acceptance it serves, its classification, its governing decisions and budget rows, the artefact that proves it, and the headings of the GEOM design family (`Zenith2/Docs/Design/GEOM.md` and its section files) that specify it. The design entry's "Requirement index" is the reverse map. GEOM owns every requirement here; where another area's ticket supplies evidence (RPROD's shader compiler and culling, ASSET's cook orchestration, EDIT's preview), the area is named in the evidence and in `Zenith2/Docs/Design/GEOM.md → "Dependencies"`. Requirements Z2-R-GEOM-160 to Z2-R-GEOM-167 cover the RHI item Z2-I-RHI-08 (GPU scene tables and pools): GEOM owns its procedural-geometry producer, pool layout and generator interface by the orchestrator's ruling on Q38, and RHI owns the item's GPU scene.

## Requirements

### Z2-R-GEOM-001 Leaf library with a Core-only dependency

- **Statement:** `ZenithVertexFormat` is an engine library in `Zenith2/Engine/ZenithVertexFormat/` whose `CMakeLists.txt` is `zenith_add_library(ZenithVertexFormat DEPENDS ZenithCore)`, declares `ZENITH_ALL`, contains no platform code, and is linked by ZenithFlux, ZenithCook, ZenithShaderCompiler and ZenithEditor without any of them including its `Internal/` headers.
- **Covers:** Z2-I-GEOM-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `Libgraph` gate step (declared edges against `declared_deps.json`, encapsulation) and `CompileCoverage` on the ticket that lands the library; review of the `CMakeLists.txt`.
- **Design:** Zenith2/Docs/Design/GEOM.md → "Target summary"; Zenith2/Docs/Design/GEOM.md → "ZenithVertexFormat files"; Zenith2/Docs/Design/GEOM.md → "Libraries GEOM code depends on"

### Z2-R-GEOM-002 Public headers carry contracts and are self-contained

- **Statement:** Every public type of `Zenith_VertexKernel.h`, `Zenith_VertexFormat.h` and `Zenith_VertexStreamSet.h` carries one contract block, every fallible function returns `Zenith_Result<…, Zenith_VertexFormatError>`, and each header compiles alone.
- **Covers:** Z2-I-GEOM-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062, Z2-D-047, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck` and `HeaderSelfContained` gate steps over the three headers; `Test_VertexFormat_Example_Compiles` (the cited example) in the `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Zenith_VertexKernel header"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "Zenith_VertexFormat header"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set codes"; Zenith2/Docs/Design/GEOM.md → "Rules across the GEOM API"

### Z2-R-GEOM-003 API-only slice lands first

- **Statement:** The first `ZenithVertexFormat` ticket lands the three public headers with their declarations and contract blocks and stub definitions only, so shader-compiler and cook tickets can build against the API before the codec is implemented.
- **Covers:** Z2-I-GEOM-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** The API-only ticket passes `Build`, `ApiContractCheck` and `HeaderSelfContained`; the dependent tickets are `blocks`-linked from it.
- **Design:** Zenith2/Docs/Design/GEOM.md → "Ticket slices and ordering"

### Z2-R-GEOM-004 Codec pulled forward into Stage 1

- **Statement:** The kernel, scalar, QTangent, octahedral, half, quantisation, weight and stream-set code slices are scheduled in the Stage 1 sprint with fix version `Z2 Stage 2`, because they depend on no open decision; the layout hash waits for D-SEL-HASH.
- **Covers:** Z2-I-GEOM-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** Sprint and fix-version routing of the slices on the board (the session coverage checker); the hash slice is `blocks`-linked from the D-SEL-HASH ruling ticket.
- **Design:** Zenith2/Docs/Design/GEOM.md → "Stage table"

### Z2-R-GEOM-005 Closed format vocabulary

- **Statement:** `Zenith_VertexFormatType` holds exactly the 15 formats of the vocabulary with the byte sizes and lane counts of the Rendering table, and `Zenith_ParseVertexFormat` accepts exactly the 15 `[VtxFmt]` strings and rejects every other string with `VERTEX_FORMAT_ERROR_UNKNOWN_FORMAT`.
- **Covers:** Z2-I-GEOM-01, Z2-I-GEOM-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_VertexFormat_RoundTripEveryFormat` and `Test_VertexFormat_LayoutValidation` in the `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Zenith_VertexFormat header"

### Z2-R-GEOM-006 Closed semantic vocabulary and fixed locations

- **Statement:** Semantics are exactly the vocabulary of `Zenith_VertexSemantic`, and each maps to the fixed shader input location of the canonical location table (POSITION 0, QTANGENT and NORMAL_OCT 1, TEXCOORD0 2, TEXCOORD1 3, COLOR0 4, JOINTS0 5, WEIGHTS0 6).
- **Covers:** Z2-I-GEOM-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamSet_DeclarationsMatchOracle` in `Zenith2/Tools/ZenithShaderCompiler/Tests/`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Canonical stream-set layouts"

### Z2-R-GEOM-007 Layout-table types match the generated headers

- **Statement:** `Zenith_VertexAttributeDesc` (8 bytes), `Zenith_VertexStreamDesc` (4 bytes) and `Zenith_VertexLayoutDesc` have exactly the fields of the Rendering declaration, are trivially copyable, and are the only types generated shader headers use for vertex tables.
- **Covers:** Z2-I-GEOM-01, Z2-I-GEOM-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062, Z2-D-015
- **Budgets:** none
- **Acceptance evidence:** `static_assert`s in the header; `HeaderSelfContained` over a generated header; `Test_ShaderCompiler_HeaderOffsetsMatchReflection` (RPROD) in the `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Zenith_VertexFormat header"

### Z2-R-GEOM-008 Layout validation

- **Statement:** `Zenith_ValidateVertexLayout` rejects every violation of the layout rules (unknown values, streams out of order or above 8, unaligned or overlapping offsets, strides above 64 or not multiples of 4, a stride of 0 on any stream but the null stream 7, a non-zero stride on stream 7, a stream-7 attribute ending beyond byte 32, duplicate semantics, and a declaration table's layout hash differing from its recomputation) with the listed error, and accepts every declared stream-set layout and every specialised table with a null stream.
- **Covers:** Z2-I-GEOM-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_VertexFormat_LayoutValidation` (one failing fixture per rule) in the `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Layout validation and hashing"

### Z2-R-GEOM-009 Canonical layout hash

- **Statement:** `Zenith_ComputeVertexLayoutHash` hashes the canonical little-endian serialisation of the attribute and stream tables in (stream, offset) order with `Zenith_Hash64Stream` seed 0 and never hashes struct memory, so equal layouts hash equally regardless of declaration order.
- **Covers:** Z2-I-GEOM-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062, Z2-D-038, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_VertexFormat_LayoutHashCanonical` in the `Test` step; the slice is blocked by the D-SEL-HASH ruling ticket.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Layout validation and hashing"

### Z2-R-GEOM-010 Kernel operation set

- **Statement:** Every floating-point operation of both codec halves is a kernel operation: IEEE binary32 add, sub and mul followed by the clean at 2^-40, min, max, abs, negate, floor and comparisons, in the evaluation orders the kernel table fixes.
- **Covers:** Z2-I-GEOM-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_VertexKernel_CleanNeverSubnormal`, `Test_VertexKernel_SeedsInRange` (every Newton seed over [2^-40, 2^40] is at least 2^-41) and `Test_VertexFormat_CpuGpuBitMatch` in the `Test` step on WARP and lavapipe.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Kernel operations"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "Kernel constants"

### Z2-R-GEOM-011 Newton reciprocal and square root

- **Statement:** `VfRcp` is the seed `0x7EF311C7 − bits(x)` followed by exactly four Newton steps, `VfRsqrt` the seed `0x5F375A86 − (bits(x) >> 1)` followed by exactly three, `VfSqrt(x) = x × VfRsqrt(x)` and `VfDiv(a, b) = a × VfRcp(b)`, identically on the CPU and the GPU, with results within 2 ULP of the correctly rounded value over [2^-40, 2^40].
- **Covers:** Z2-I-GEOM-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_VertexKernel_RcpAccuracy`, `Test_VertexKernel_RsqrtAccuracy` and `Test_VertexFormat_CpuGpuBitMatch` in the `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Kernel operations"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "Kernel preconditions and device conformance"

### Z2-R-GEOM-012 Round half away from zero without the add trick

- **Statement:** `VfRoundHalfAwayToInt` computes `f = floor(|x|)`, rounds up when `|x| − f ≥ 0.5`, and restores the sign, so 0.49999997 rounds to 0 and every k + 0.5 rounds away from zero.
- **Covers:** Z2-I-GEOM-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_VertexKernel_RoundHalfAwayEdges` in the `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Kernel operations"

### Z2-R-GEOM-013 No contraction in either half

- **Statement:** No multiply and add of codec code is fused: the CPU half is built without floating-point contraction and every GPU kernel function is declared `precise`.
- **Covers:** Z2-I-GEOM-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_VertexKernel_NoContraction` in the `Test` step; `Test_VertexFormat_CpuGpuBitMatch` on every device.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Kernel preconditions and device conformance"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "The GPU codec modules"

### Z2-R-GEOM-014 Device arithmetic conformance recorded

- **Statement:** Every CI and reference device runs the conformance case asserting that its binary32 add, sub and mul of 10^6 fixed operand pairs equal the CPU bits, and a failing device is recorded as a device finding.
- **Covers:** Z2-I-GEOM-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062, Z2-D-013
- **Budgets:** none
- **Acceptance evidence:** `Test_VertexKernel_DeviceArithmeticConformance` on WARP and lavapipe in the `Test` step; RTX 3070 and S24 runs as `needs-human` device steps of the Stage 2 acceptance.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Kernel preconditions and device conformance"; Zenith2/Docs/Design/GEOM.md → "Device and acceptance runs"

### Z2-R-GEOM-015 Scalar lane codecs

- **Statement:** SNORM16, UNORM16, SNORM8 and UNORM8 encode by clamping, multiplying by the format maximum and rounding half away from zero, and decode by multiplying by the listed reciprocal constant (SNORM decodes clamped to −1); every code re-encodes to itself.
- **Covers:** Z2-I-GEOM-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_VertexFormat_ScalarCodecsExhaustive` and `Test_VertexFormat_RoundTripEveryFormat` in the `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Scalar quantisation codecs"

### Z2-R-GEOM-016 QTangent encode with the bias rule

- **Statement:** The QTangent encoder is exactly the kernel pseudocode: Gram-Schmidt of t against n, `bU = cross(n, t)`, Shepperd's method on [t | bU | n], normalisation, `w ≥ 0`, `|w|` raised to at least 1/32767 with xyz rescaled by `sqrt(1 − eps²)/|xyz|`, the reflection sign in the sign of `w`, SNORM16 quantisation.
- **Covers:** Z2-I-GEOM-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_VertexFormat_QTangentBiasRule` and `Test_VertexFormat_QTangentReferenceVectors` (committed codes) in the `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "QTangent codec realisation"

### Z2-R-GEOM-017 QTangent degenerate tangent

- **Statement:** A tangent parallel to the normal or of zero length is replaced by the Gram-Schmidt of the axis of the smallest normal component (ties x, y, z), so every encode yields an orthonormal frame; a zero normal is `VERTEX_FORMAT_ERROR_DEGENERATE_NORMAL`.
- **Covers:** Z2-I-GEOM-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_VertexFormat_QTangentDegenerateTangent` and `Test_VertexFormat_NonFiniteRejected` in the `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "QTangent codec realisation"

### Z2-R-GEOM-018 QTangent decode never flips the normal

- **Statement:** The decoder returns `t`, the unsigned bitangent times the sign of `w`, and `n = cross(t, bU)`, so a mirrored frame decodes to the unmirrored normal.
- **Covers:** Z2-I-GEOM-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_VertexFormat_QTangentNormalNeverFlipped` in the `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "QTangent codec realisation"

### Z2-R-GEOM-019 Octahedral codec

- **Statement:** Octahedral normals encode and decode exactly by the kernel pseudocode to SNORM16x2 in vertex streams, and the render-target `× 0.5 + 0.5` form is a pass helper, not a codec function.
- **Covers:** Z2-I-GEOM-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_VertexFormat_OctahedralEdges` and `Test_VertexFormat_CpuGpuBitMatch` in the `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Octahedral codec realisation"

### Z2-R-GEOM-020 Integer-only half codec

- **Statement:** The half codec uses no floating-point operation in either half, rounds to nearest even, handles zero, subnormals, infinity and NaN, and encodes binary32 subnormal inputs as signed zero on every device.
- **Covers:** Z2-I-GEOM-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_VertexFormat_HalfExhaustive` in the `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Half-float codec realisation"

### Z2-R-GEOM-021 Position quantisation to the tight box

- **Statement:** Positions are SNORM16x4 over the mesh's tight AABB (centre bias, half-extent scale of at least 2^-20 m per axis, w stored 0), decoded in the vertex stage as `f × scale + bias` in kernel order; the FLOAT32x3 fallback stores raw positions with identity scale and bias.
- **Covers:** Z2-I-GEOM-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_VertexFormat_RoundTripEveryFormat` and `Test_Cook_PrecisionFallbacksReported` in the `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Position and UV quantisation"

### Z2-R-GEOM-022 UV remapping with scale and bias

- **Statement:** UV0 and UV1 are UNORM16x2 over each channel's range across every LOD and part, each channel with its own scale and bias in the primitive record; the FLOAT16x2 fallback stores raw UVs with identity scale and bias.
- **Covers:** Z2-I-GEOM-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_VertexFormat_RoundTripEveryFormat` and `Test_Cook_PrecisionFallbacksReported` in the `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Position and UV quantisation"

### Z2-R-GEOM-023 Weights sum exactly

- **Statement:** Skin weights keep the four largest influences, renormalised, and quantise so the four stored values sum exactly to 255 (UNORM8x4) or 65535 (UNORM16x4), the largest weight absorbing the residual.
- **Covers:** Z2-I-GEOM-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_VertexFormat_WeightsSumExact` and `Test_Cook_SkinWeightsTopFour` in the `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Weight quantisation"

### Z2-R-GEOM-024 CPU and GPU halves match bit for bit

- **Statement:** For 1,000,000 fixed random inputs per format and per codec function, encoders and decoders alike, the CPU codec and `VertexFormats.slang` produce identical bits.
- **Covers:** Z2-I-GEOM-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_VertexFormat_CpuGpuBitMatch` on WARP and lavapipe in the `Test` step; RTX 3070 and S24 device runs in the Stage 2 acceptance.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Vertex format tests"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "The GPU codec modules"

### Z2-R-GEOM-025 GPU codec module contents

- **Statement:** The GPU half is the five modules `VertexKernel.slang`, `Half.slang`, `Octahedral.slang`, `VertexFormats.slang` and `MeshVertex.slang` with exactly the listed functions, including the QTangent encoder from a frame, the 16-bit index halfword select, and `Flux_FetchPoolVertex` reading vertex v of granule g at byte `g × 16 + v × 16` (FLOAT16 pool) or `g × 16 + v × 24` (FLOAT32 pool).
- **Covers:** Z2-I-GEOM-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `ShaderChecks` gate step compiles them; `Test_VertexFormat_CpuGpuBitMatch` exercises every function.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "The GPU codec modules"

### Z2-R-GEOM-026 Stream-set code layout

- **Statement:** Every vertex layout is named by a 16-bit stream-set code with the POS, SURF, UV0, UV0F, UV1, UV1F, COL, SKIN and FAMILY fields at the listed bits, and `Zenith_ValidateStreamSetCode` rejects reserved and inconsistent values.
- **Covers:** Z2-I-GEOM-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamSet_NameRoundTripEveryCode` in the `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set code fields"

### Z2-R-GEOM-027 Canonical stream-set names

- **Statement:** Every valid code formats to exactly one canonical name of the segment grammar (the default mesh is `pos-snorm16x4|surface-qt16-uv16`) and parses back to itself; malformed names are rejected.
- **Covers:** Z2-I-GEOM-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamSet_NameRoundTripEveryCode` and the fuzz target `Zenith_Fuzz_StreamSetName` (`win-asan` smoke).
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Canonical stream-set names"

### Z2-R-GEOM-028 Stream-sets declared in Slang alone

- **Statement:** Every reachable MESH, TERRAIN and PROCEDURAL stream-set is one `[StreamSet(<code>)]` vertex-input struct of `Zenith2/Shaders/Common/StreamSets.slang`, expanded by the one preprocessor generator `FLUX_SS_DECLARE` over the reachable product from the segment macros of `StreamSetFields.slang`; the `--stream-sets` aggregate command reflects them into `Flux_StreamSets.generated.h` (`axFLUX_STREAMSET_LAYOUTS`, 255 entries sorted by code, with the reflected layout hash), rejects a code, mismatch, duplicate, location, missing, unreachable or list-name error with its `ZSC_ERROR_STREAM_SET_*` code, and no C++ source computes a stream offset, stride or location; the C++ oracle of the canonical layouts exists only in the shader compiler's test target.
- **Covers:** Z2-I-GEOM-01, Z2-I-GEOM-03, Z2-I-GEOM-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062, Z2-D-015
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamSet_DeclarationsMatchOracle`, `Test_StreamSet_DeclarationErrors` and `Test_StreamSet_HeaderDeterministic` in `Zenith2/Tools/ZenithShaderCompiler/Tests/`, `Test` step; review that no shipping or tool target links the oracle (`Libgraph`).
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Reachable stream-set rule"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set declarations in Slang"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "Declared stream-sets"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "Canonical stream-set layouts"

### Z2-R-GEOM-029 Fallbacks map to field changes

- **Statement:** Each precision fallback is a distinct declared stream-set whose code differs from the unfallen code in the listed fields (POS, UV0F, UV1F, SKIN), so a failed precision check always selects a declared code and is never a cook error; a skinned mesh's position and posed-extent fallbacks are tied, either check setting both FLOAT32X3 bind positions and the FLOAT32 pool (`bPERM_POSITION32`); the joint and weight fallbacks of a skinned mesh change its cooked code alone; the normal-only variant is a layout choice and is not reported as a fallback.
- **Covers:** Z2-I-GEOM-08, Z2-I-GEOM-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_PrecisionFallbacksReported`, `Test_Cook_SkinnedPositionTie` and `Test_StreamSet_EveryFallbackDeclared` in the `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Fallback derivation rule"

### Z2-R-GEOM-030 Program stream-set specialisation

- **Statement:** The shader compiler emits, for each raster program and each drawable declared stream-set, the program's attributes that the code holds, copied with their formats, offsets, strides and locations from that stream-set's reflected Slang declaration (a substituted surface keeps the program's semantic and location), then a null-stream attribute for each attribute the code lacks, plus the declaration's layout hash and the stream-set link-time constants, so a program source is written once; only raster codes are drawable (a cooked skinned code, SKIN not none, never is), a code being drawable when it holds POSITION or is a pool code and, for a program that declares a surface semantic, holds a surface attribute in either form, so a `Flux_MeshVertex` program has at most 110 tables; a pool code (POOL16, POOL32) is compiled only with the link-time constant `bPERM_SKINNED = true` and every other code only with `false`, and a pool code's table binds the program's POSITION to the null stream, which that permutation ignores in favour of `Flux_FetchPoolVertex`.
- **Covers:** Z2-I-GEOM-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062, Z2-D-015
- **Budgets:** none
- **Acceptance evidence:** `Test_ShaderCompiler_StreamSetTables` (with the `surface-oct16`, terrain, procedural and `pos-pool16x4|surface-qt16-uv16` cases, and the negative case of the cooked skinned code `pos-snorm16x4|surface-qt16-uv16|skin-j8w8`, which has no table) in `Zenith2/Tools/ZenithShaderCompiler/Tests/`, `Test` step; `Test_Vertex_LayoutMismatchRejected`.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Program stream-set specialisation"

### Z2-R-GEOM-031 Stream-sets compiled from the engine list and content

- **Statement:** Engine raster program blobs are compiled at build time for `Zenith2/Shaders/StreamSets.list` (every name declared) and at cook time for the union with the game's `stream_sets.json`; a declared code content uses that no program can draw is `GEOM_COOK_ERROR_STREAM_SET_UNDRAWABLE`.
- **Covers:** Z2-I-GEOM-01, Z2-I-GEOM-08, Z2-I-GEOM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062, Z2-D-025
- **Budgets:** Z2-B-RPROD-024
- **Acceptance evidence:** `Test_Permutations_EnumerationMatchesUse` in `Zenith2/Tools/ZenithCook/Tests/Materials/`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Program stream-set specialisation"; Zenith2/Docs/Design/GEOM/Materials.md → "Material permutation enumeration"

### Z2-R-GEOM-032 Position decode invariance

- **Statement:** Every raster program computes clip position only through the `precise` function `Flux_TransformObjectToClip`, after its family's position fetch, so every depth-writing and depth-testing pass of one instance produces bit-identical `SV_Position`.
- **Covers:** Z2-I-GEOM-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062, Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** `Test_Vertex_DepthEqualAcrossPasses` in `Zenith2/Engine/ZenithFlux/Tests/VertexFormat/`, `Test` step on WARP and lavapipe.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Position decode invariance"

### Z2-R-GEOM-033 Decode-route consistency

- **Statement:** Passes that must agree bit for bit on one vertex use the same decode route (all fixed-function, or all manual), and the agreement of the fetch unit with the manual decoders is measured and recorded per format and device.
- **Covers:** Z2-I-GEOM-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_VertexFormat_FetchUnitParity` result records on WARP, lavapipe, RTX 3070 and S24; `Test_HiZ_MeshletFallbackMatchesMeshShader` (RPROD) in Stage 5.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Decode-route consistency"

### Z2-R-GEOM-034 Packer driven by the generated table

- **Statement:** `Zenith_PackVertexStreams` is the one packer of the cook, holds no layout of its own, and is driven by the mesh code's entry of the generated `axFLUX_STREAMSET_LAYOUTS` (ZenithCook declares `ZenithShadersGenerated`), so cooked bytes and runtime vertex input come from one Slang declaration; the mesh records that entry's hash; `GEOM_COOK_ERROR_STREAM_SET_UNDECLARED` is raised only by a lookup in a table that lacks the chosen code, never by a precision outcome.
- **Covers:** Z2-I-GEOM-01, Z2-I-GEOM-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_VertexFormat_PackerMatchesLayout`, `Test_Cook_PackerReadsGeneratedTable` and `Test_Cook_StreamSetUndeclared` in the `Test` step; `Libgraph` over ZenithCook's declared edges.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Zenith_VertexFormat header"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set declarations in Slang"; Zenith2/Docs/Design/GEOM/Meshes.md → "Mesh cook pipeline"

### Z2-R-GEOM-035 Layout mismatch refused at bind

- **Statement:** Binding a mesh whose recorded layout hash differs from the program's table for its stream-set fails with `FLUX_VERTEX_ERROR_LAYOUT_MISMATCH`, skips the draw and counts `Flux.Geometry.LayoutMismatches`.
- **Covers:** Z2-I-GEOM-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Vertex_LayoutMismatchRejected` in the `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM.md → "Flux_GeometryErrors header"; Zenith2/Docs/Design/GEOM/Meshes.md → "Reader validation"

### Z2-R-GEOM-036 Mesh input validation

- **Statement:** The mesh cook validates the intermediate mesh against the parser limits (16,777,216 vertices, 255 material slots) and the section limit (S at most 255, the instance record's 8-bit field, so at most 2,040 parts), finite attributes and valid indices, fails with the listed cook errors, removes degenerate triangles with a warning, repairs zero normals with a warning, and warns on suspect winding.
- **Covers:** Z2-I-GEOM-07, Z2-I-GEOM-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_WindingSuspectWarns`, `Test_Cook_SectionLimit` and `Test_Mesh_ReaderRejectsCorruption`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Input validation"

### Z2-R-GEOM-037 Weld before quantisation

- **Statement:** The cook welds exact duplicate vertices over every attribute stream before quantisation, so vertices differing below the quantisation step and UV seams keep separate vertices.
- **Covers:** Z2-I-GEOM-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_WeldExactOnly` and `Test_CookMesh_RepeatCookIdentical` over the weld fixture in `Zenith_CookMesh_Tests.cpp`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Weld"

### Z2-R-GEOM-038 Tangent generation

- **Statement:** A mesh with a normal-mapped material and no source tangents gets MikkTSpace tangents over UV0 and a second weld; a skinned mesh without normal-mapped materials gets tangents from the orthonormal basis of each normal.
- **Covers:** Z2-I-GEOM-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_TangentsGenerated` in `Zenith_CookMesh_Tests.cpp`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Tangent generation"

### Z2-R-GEOM-039 Stream-set choice

- **Statement:** The cook chooses each mesh's stream-set from its materials and precision checks as the choice table specifies (QTangent for normal-mapped or skinned meshes, octahedral otherwise, unlit-only meshes included, because cross-doc ruling X140 gives the MESH family no surface-less stream-set; UV and colour streams only when read); every code the choice can produce is reachable and declared, and the lookup's `GEOM_COOK_ERROR_STREAM_SET_UNDECLARED` is a defence against a table that lacks it.
- **Covers:** Z2-I-GEOM-05, Z2-I-GEOM-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_StreamSetChoice`, `Test_Cook_ChoiceReachesOnlyDeclaredCodes` and `Test_Cook_StreamSetUndeclared` in `Zenith2/Tools/ZenithCook/Tests/Geometry/`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Stream-set choice"

### Z2-R-GEOM-040 Shared quantisation across LODs and parts

- **Statement:** One position box and one UV box per channel cover every vertex of every LOD and every part of a mesh, and every primitive record of the mesh carries them.
- **Covers:** Z2-I-GEOM-03, Z2-I-GEOM-06, Z2-I-GEOM-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_MeshSplitSharesQuantisation` in the `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Quantisation box and precision checks"

### Z2-R-GEOM-041 Position precision check

- **Statement:** The position check fails when the maximum decoded error exceeds max(2^-16 × the AABB diagonal, 1 mm) (preset-tightenable), and a failed check takes FLOAT32x3.
- **Covers:** Z2-I-GEOM-08, Z2-I-GEOM-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_PrecisionFallbacksReported` (a 200 m mesh stays SNORM16x4 by default and falls back under the tight preset), `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Quantisation box and precision checks"

### Z2-R-GEOM-042 UV precision check

- **Statement:** The UV check measures the error in texels at the largest resolution sampled with that channel (default reference 4,096), fails above 1/8 texel, chooses FLOAT16x2 when better, and keeps the lower-error format with `GEOM_COOK_WARNING_UV_PRECISION` when both fail.
- **Covers:** Z2-I-GEOM-08, Z2-I-GEOM-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_PrecisionFallbacksReported`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Quantisation box and precision checks"

### Z2-R-GEOM-043 Joint and weight precision checks

- **Statement:** Meshes above 256 joints take UINT16x4 joints, and meshes whose weight error `Σ |Δw_i| × L_i` exceeds 1 mm take UNORM16x4 weights.
- **Covers:** Z2-I-GEOM-08, Z2-I-GEOM-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_SkinJoints16Fallback` and `Test_Cook_PrecisionFallbacksReported`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Quantisation box and precision checks"

### Z2-R-GEOM-044 Fallbacks reported, never silent

- **Statement:** Every failed precision check writes one record (mesh, source, attribute, metric, unit, tolerance, chosen format, cooked and raster stream-set) to the cook report's `vertexFallbacks` and the same record to the `.zmesh` precision section, sorted deterministically, and a skinned mesh whose posed-extent check alone failed also writes a forced `position` record carrying `tiedBy` (the record rule).
- **Covers:** Z2-I-GEOM-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_NoFallbackIsSilent` (field-by-field equality of report and section) and `Test_Cook_SkinnedPositionTie`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Precision fallback report format"; Zenith2/Docs/Design/GEOM.md → "Cook report members"

### Z2-R-GEOM-045 meshoptimizer order

- **Statement:** Per LOD the cook runs vertex-cache optimisation, overdraw optimisation at threshold 1.05, vertex-fetch remapping, the split, position-only indices and meshlets, in that order.
- **Covers:** Z2-I-GEOM-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_OptimisationOrder`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Optimisation order"

### Z2-R-GEOM-046 Splitting at 65,535 vertices

- **Statement:** The cook splits each material's triangles into parts of at most 65,535 unique vertices in optimised order, within the mesh's limit of 255 sections; no index buffer is 32-bit and primitive restart is disabled.
- **Covers:** Z2-I-GEOM-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_SplitAt65535`, `Test` step; the lint `Cook.NoIndex32`.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Splitting and part table"

### Z2-R-GEOM-047 Split without cracks

- **Statement:** Parts duplicate shared boundary vertices with identical encoded bytes, so a split mesh renders with no crack against its unsplit reference.
- **Covers:** Z2-I-GEOM-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_MeshSplitSharesQuantisation` and `Test_Cook_MeshSplitNoCracksRendered` (WARP and lavapipe), `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Splitting and part table"

### Z2-R-GEOM-048 Sections stable across LODs

- **Statement:** Section numbering reserves `P_m` sections per material slot (its largest part count in any LOD), so a section index names the same material slot in every LOD, and unused sections are empty parts with index count 0; S = Σ P_m is at most `uGEOM_MAX_SECTIONS` = 255, beyond which the cook fails with `GEOM_COOK_ERROR_MESH_LIMIT` and the reader with `FLUX_GEOMETRY_ERROR_LIMIT`.
- **Covers:** Z2-I-GEOM-07, Z2-I-GEOM-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_SectionsStableAcrossLODs` and `Test_Cook_SectionLimit`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Mesh terms"; Zenith2/Docs/Design/GEOM/Meshes.md → "Splitting and part table"

### Z2-R-GEOM-049 Position-only index buffer

- **Statement:** Each part gets a position-only index buffer from `meshopt_generateShadowIndexBuffer` over its packed stream 0 when it reduces unique vertices below 90%, used by shadow views and opaque depth-only buckets and never by the Forward+ prepass or alpha-tested buckets.
- **Covers:** Z2-I-GEOM-03, Z2-I-GEOM-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_PositionOnlyIndicesCanonical`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Position-only indices"

### Z2-R-GEOM-050 Part bounds and UV density

- **Statement:** Every part records its AABB, bounding sphere and per-channel UV density in metres per UV unit, computed deterministically.
- **Covers:** Z2-I-GEOM-20, Z2-I-GEOM-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_PartBoundsAndUVDensity` (analytic quad, zero-UV-area part, closed-form AABB and sphere) in `Zenith_CookMesh_Tests.cpp`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Bounds and UV density"

### Z2-R-GEOM-051 Mesh cook determinism

- **Statement:** A clean cook and an incremental cook of the same mesh inputs produce byte-identical `.zmesh` files.
- **Covers:** Z2-I-GEOM-02, Z2-I-GEOM-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CookMesh_RepeatCookIdentical` and `Test_GeomCook_CleanEqualsIncrementalAllKinds`, `Test` step; `Test_GeomCook_MultiProcessSameBytes` with the Stage 4 worker pool.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Mesh cook pipeline"

### Z2-R-GEOM-052 GEOM cooked file header

- **Statement:** Every GEOM cooked file (`.zmesh`, `.zmat`, `.ztex`) and GEOM's sections of `.zterrain` use ASSET's cooked container field for field (the 64-byte common header, the 40-byte section entries embedding COMPRESS's 24-byte descriptor, 16-byte placement in (kind, index) order and checks C1–C12), written through `Zenith_CookedFileWriter`; GEOM defines only the magics, kinds, format versions (1) and section kinds, readers select each section's codec from its descriptor, and no GEOM code writes or parses header or table bytes of its own.
- **Covers:** Z2-I-GEOM-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-004, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** ASSET's `Test_CookedFile_WriterRoundTrip`, `Test_CookedFile_EachCheckRejects` and `Test_CookedFile_VersionMismatchRefused` (`Zenith2/Docs/Design/ASSET/Tests.md`); GEOM's `Test_GeomFiles_UseAssetContainer` in `Zenith2/Tools/ZenithCook/Tests/Geometry/` and the kind-specific reader tests of each format, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM.md → "GEOM cooked file header"; Zenith2/Docs/Design/GEOM.md → "Byte order and serialization rules"

### Z2-R-GEOM-053 Mesh file records

- **Statement:** The `.zmesh` sections and the mesh info (112 bytes), LOD (24 bytes), part (84 bytes, with the offsets listed) and precision (12 bytes per record) records have exactly the listed layouts, written field by field in little-endian order.
- **Covers:** Z2-I-GEOM-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Mesh_ReaderRejectsCorruption` round-trips every fixture; `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "The cooked mesh format"

### Z2-R-GEOM-054 Mesh reader validation

- **Statement:** The mesh reader takes ASSET's validated `Zenith_CookedFileView` and the sections ZenithAssets decoded, runs only the kind-specific rows (the view's magic, kind and format version as a defence; the section kinds, indices and counts; part, index and meshlet ranges; the stream-set code and its layout hash) and returns the listed error without asserting or allocating; header, section-table, section-check and decode failures are ASSET's checks, applied before the loader runs.
- **Covers:** Z2-I-GEOM-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Mesh_ReaderRejectsCorruption`, `Test_Mesh_VersionMismatchRefused`, and `Zenith_Fuzz_MeshFile` through `Zenith_ParseCookedHeader` (`win-asan` smoke, full duration in stage acceptance).
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Reader validation"

### Z2-R-GEOM-055 Mesh resource API

- **Statement:** `Flux_MeshResource` and `Flux_MeshManager` expose exactly the declared members, `Flux_MeshID` is an 8-byte slot-and-generation value that never serializes, and a stale ID resolves to null.
- **Covers:** Z2-I-GEOM-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck`; `Test_Mesh_InstanceDrawsAfterResident`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Flux_MeshResource header"

### Z2-R-GEOM-056 Raster and skin-source stream-set pools

- **Statement:** Every mesh is suballocated into the pool of its raster stream-set code (one buffer per fixed-function stream the raster code has, sharing one element allocator so one `vertexOffset` addresses every bound stream), a skinned mesh also into the compute-only skin-source pool of its cooked code (streams 0 and 3, same vertex order), and its indices into the one 16-bit index pool at even offsets; every draw of one bucket binds the one raster pool of the bucket key's raster stream-set, and a skinned raster draw binds streams 1–2 of its raster pool plus the skinned-vertex pool.
- **Covers:** Z2-I-GEOM-07, Z2-I-GEOM-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062, Z2-D-058
- **Budgets:** Z2-B-GEOM-001
- **Acceptance evidence:** `Test_Mesh_PoolGrowPreservesContents`, `Test_Mesh_FreedRangeReusedAfterRetirement` and `Test_Mesh_PoolsKeyedByRasterAndCookedCode`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Geometry pools"; Zenith2/Docs/Design/GEOM/Meshes.md → "Pool allocator"

### Z2-R-GEOM-057 Buffer usage unions

- **Statement:** Each pool buffer is created with its usage union (vertex or index, storage read, transfer), and the union is part of the physical-resource cache key.
- **Covers:** Z2-I-GEOM-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Mesh_BufferUsageUnion`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Geometry pools"

### Z2-R-GEOM-058 Pool growth within the budget

- **Statement:** An exhausted pool grows by resize-and-copy up to the clamp of `Z2-B-GEOM-001`, preserving every element offset, and an allocation past the clamp fails the object's GPU residency (`ReportGpuFailed` with `POOL_FULL`) without evicting anything.
- **Covers:** Z2-I-GEOM-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-058, Z2-D-012
- **Budgets:** Z2-B-GEOM-001
- **Acceptance evidence:** `Test_Mesh_PoolGrowPreservesContents` and `Test_Mesh_PoolFullFailsLoad`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Pool growth"

### Z2-R-GEOM-059 Pools are persistent read-only

- **Statement:** Pool buffers are `Flux_PersistentReadOnly`: new meshes are uploaded on the copy queue into unreferenced ranges, published after the upload completes, and freed ranges return to the allocator only after retirement.
- **Covers:** Z2-I-GEOM-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_Mesh_FreedRangeReusedAfterRetirement` and `Test_GPUScene_SlotReuseAcrossFramesInFlight`, `Test` step with validation clean.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Geometry pools"; Zenith2/Docs/Design/GEOM/Meshes.md → "Mesh residency state machine"

### Z2-R-GEOM-060 Mesh residency state machine

- **Statement:** A mesh moves through the states of the residency table driven by `Flux_MeshKindLoader`'s members: `BeginLoad` requests every section, `OnSectionsLoaded` validates the decoded bytes, pushes the mesh onto the load queue and calls `Activate` in the same call (GPU residency PENDING before any drain runs, the activation order of ASSET's "Delivery and activation"), the `Flux.BuildGraph` drain allocates the ranges and queues every upload, completion of every upload releases the staging and calls `ReportGpuResident` (`RESIDENT`), and a pool at its budget clamp or a failed upload after activation is `ReportGpuFailed` (residency FAILED), never `Fail`; instances of a non-resident mesh draw nothing and draw from the first frame after `RESIDENT`.
- **Covers:** Z2-I-GEOM-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_Mesh_InstanceDrawsAfterResident` and `Test_GeomLoaders_Contract`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Mesh residency state machine"; Zenith2/Docs/Design/GEOM.md → "Asset kind loaders"

### Z2-R-GEOM-061 Mesh load cancellation

- **Statement:** Cancelling a mesh load before activation (`Cancel`) releases every pool range, primitive record, staging buffer and allocation before `AcknowledgeCancel`, and unloading a mesh whose uploads are queued or staged (`Unload`) cancels them and frees its ranges and records through deferred deletion before `AcknowledgeUnload`.
- **Covers:** Z2-I-GEOM-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_Mesh_LoadCancel1000Cycles` (`z2-stress`) and `Test_GeomLoaders_Contract`, `Test` step and `Sanitizers`.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Mesh residency state machine"; Zenith2/Docs/Design/GEOM.md → "Cancellation"

### Z2-R-GEOM-062 Mesh device-loss reload

- **Statement:** After device loss every active mesh receives a device-recovery load (`m_bDeviceRecovery`), which re-allocates and re-uploads its ranges for the existing object, activates with `m_pExistingObject` and redraws bit-identically; GEOM keeps no CPU copy of a resident mesh, so `DropCpuCopies` returns 0.
- **Covers:** Z2-I-GEOM-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_Mesh_DeviceLossReloads` and `Test_GeomLoaders_Contract`, `Test` step; the Stage 6 recovery acceptance.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Mesh residency state machine"; Zenith2/Docs/Design/GEOM.md → "Asset kind loaders"; Zenith2/Docs/Design/GEOM.md → "Failure table"

### Z2-R-GEOM-063 Authored LOD chains

- **Statement:** With `lod.mode` `authored`, each authored level becomes a LOD whose threshold comes from the authored screen coverage or from the pixel-error formula with the measured error.
- **Covers:** Z2-I-GEOM-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_AuthoredLODChain` in `Zenith_CookLOD_Tests.cpp`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "LOD generation"

### Z2-R-GEOM-064 LOD generation

- **Statement:** With `lod.mode` `generate`, LODs are `meshopt_simplifyWithAttributes` of the previous level at the preset ratios, weights and maximum error, stopping at the first level above 0.9 of the previous index count, above the maximum error, or at 8 levels, deterministically.
- **Covers:** Z2-I-GEOM-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_LODGenerationStops` and `Test_Cook_LODDeterministic`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "LOD generation"

### Z2-R-GEOM-065 LOD thresholds from pixel error

- **Statement:** Each LOD threshold is `min(previous, 2 r p / (1,080 × e_{l+1}))` with the preset pixel error p, the last is 0, and thresholds never increase with the level.
- **Covers:** Z2-I-GEOM-10, Z2-I-GEOM-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_LODThresholdsMonotonic`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "LOD generation"

### Z2-R-GEOM-066 LOD selection

- **Statement:** The GPU culling and the CPU transparent collection select LOD by the projected-size algorithm with the tier scale, bias and forced LOD (`0xFFFFFFFF` unset; a set value forces `min(value, L − 1)`), bit-identically in both, one LOD per instance for every section, from the 32-byte `[FluxShared]` `Flux_LODView` whose orthographic state is bit 0 of a `uint32` flag word.
- **Covers:** Z2-I-GEOM-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-060
- **Budgets:** none
- **Acceptance evidence:** `Test_LOD_SelectionAnalytic` and `Test_LOD_CpuGpuAgree`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "LOD selection"; Zenith2/Docs/Design/GEOM.md → "Header map"

### Z2-R-GEOM-067 Shadow LOD from the reference view

- **Statement:** Shadow views select LOD from their world's main view (or the first editor view), so a shadow shows the geometry the camera sees.
- **Covers:** Z2-I-GEOM-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_LOD_ShadowUsesReferenceView`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "LOD selection"

### Z2-R-GEOM-068 Shared records for instancing

- **Statement:** All instances of a mesh share its primitive records, and instances with the same material assignment share one reference-counted material-slot range, allocated by the GPU scene and freed after retirement; culling compacts one indirect command per draw key (material slot, LOD) per bucket and orders visible instances by instance index within a draw key.
- **Covers:** Z2-I-GEOM-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** Z2-B-RPROD-001
- **Acceptance evidence:** `Test_Instancing_MaterialSlotsShared`, `Test` step; RPROD's draw-key tests.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Instancing"

### Z2-R-GEOM-069 Per-instance data

- **Statement:** A material reads the instance's 32-bit custom data through `Flux_GetInstanceCustomData`, and GPU-scene draws use no per-instance vertex stream.
- **Covers:** Z2-I-GEOM-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_Instancing_CustomDataReachesMaterial`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Instancing"

### Z2-R-GEOM-070 Meshlet generation

- **Statement:** For desktop cooks each part gets meshlets of at most 64 vertices and 128 triangles (cone weight 0.25, optimised), with 8-bit local triangle indices, 16-bit part-local vertex indices and bounds and cones, covering exactly the part's triangles.
- **Covers:** Z2-I-GEOM-13
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_MeshletLimits` and `Test_Cook_MeshletCoverage`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Meshlets"

### Z2-R-GEOM-071 Meshlet record and pools

- **Statement:** `Flux_MeshletRecord` is the 32-byte `[FluxShared]` struct of the listed layout in `Common/Meshlets.slang`, and meshlet records, vertices and triangles live in persistent read-only pools addressed by the primitive record.
- **Covers:** Z2-I-GEOM-13
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-015, Z2-D-058
- **Budgets:** Z2-B-GEOM-001
- **Acceptance evidence:** `Test_ShaderCompiler_SharedTypesLayoutMatchesReflection` (RPROD) and `Test_HiZ_MeshletFallbackMatchesMeshShader`, `Test` step on WARP and lavapipe.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Meshlets"; Zenith2/Docs/Design/GEOM/Meshes.md → "Geometry pools"

### Z2-R-GEOM-072 Terrain node selection

- **Statement:** Terrain nodes are selected on the CPU by the CDLOD quadtree rule with leaf 16 m and horizontal distances `16 × 2^l × r`, r at least 8 (rejected below), from the LOD reference view for every view, each node one GPU-scene instance of the shared grid mesh whose `m_uCustomData` packs level and node indices as `TerrainGrid.slang` decodes them; a node wholly outside the terrain is never emitted and a straddling node is tested with its rectangle clipped to the terrain; from Stage 4, when texture streaming lands (Z2-R-GEOM-122), tile residency is requested through `SetExternalRequirement` and a level-0 node over a tile without mip 0 is replaced by its parent and counted; in Stage 3 every tile is resident with its full chain.
- **Covers:** Z2-I-GEOM-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_Terrain_NodeSelectionDeterministic`, `Test_Terrain_ShadowMatchesMain` and `Test_Terrain_LODRatioBelowEightRejected`, `Test` step; the residency clause by `Test_TextureStreaming_ExternalRequirement` with Z2-R-GEOM-122 in Stage 4.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Node selection"

### Z2-R-GEOM-073 Terrain morphing without cracks

- **Statement:** Terrain vertices are computed from integer global sample indices clamped to the terrain's sample range, displaced by heights read with texel `Load` at mip min(l, 8) of point-decimated height tiles (tile index `tz × m_uTilesX + tx`) and morphed toward the coarser grid over the last 30% of each level's distance, so adjacent nodes differ by at most one level and every shared vertex is bit-identical on both sides on frames without a residency clamp; normals are central differences, one-sided over the clamped span at the first and last sample; nodes over hole tiles draw in the alpha-tested bucket with the `bPERM_TERRAIN_HOLES` variant, which discards where the hole mask read at the same mip, at the level-0 index of the nearest level-m texel, is 0.
- **Covers:** Z2-I-GEOM-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Terrain_NoCracksAcrossLevels` (analytic shared-vertex cases at levels 0 and 3 on the 1,024 m fixture and 7, 8 and 9 on the generated 32,768 m terrain, level-difference and rendered cases), `Test_Terrain_EdgeClamped` and `Test_Terrain_HolesDiscardAtLevelMip`, `Test` step on WARP and lavapipe.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Morphing and displacement"

### Z2-R-GEOM-074 Terrain data and cook

- **Statement:** The terrain geometry cook `Zenith_CookTerrainGeometry` takes ENV's `Zenith_TerrainGeometryCookInput` (origin, extents, tile counts 1 … 256, sample spacing, tight height range at least 0.01 m wide, LOD distance ratio in [8, 64], and exactly one tile per grid cell with 257 × 257 binary32 heights and an optional 257 × 257 sample hole mask of 0 and 255, shared borders bit-identical), quantises the heights with `floor((h − hMin) / (hMax − hMin) × 65535 + 0.5)` in double into 257 × 257 R16_UNORM tile images whose texture cook gives 9 point-decimated mips (mip l texel (i, j) equals mip 0 texel (i·2^l, j·2^l), extents 257 … 2), hole masks with the same rule, computes the node min/max pyramid over each node's closed rectangle, and the runtime fills the 80-byte `Flux_TerrainRecord`; an extent that is not a whole number of tiles (256 × sample spacing) per axis fails with `GEOM_COOK_ERROR_TERRAIN_EXTENT` naming the two nearest multiples, and any other input rule with `GEOM_COOK_ERROR_TERRAIN_TILE`.
- **Covers:** Z2-I-GEOM-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CookTerrain_TilesAndPyramid`, `Test_CookTerrain_InputRejected` and `Test_CookTerrain_ExtentRejected` in `Zenith2/Tools/ZenithCook/Tests/Terrain/`, `Test_CookTexture_HeightDecimation` and `Test_Terrain_RecordLayout`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain data"; Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain cook contract"

### Z2-R-GEOM-075 Terrain renderer API

- **Statement:** `Flux_TerrainRenderer` exposes `AddTerrain`, `RemoveTerrain`, `UpdateNodeBounds`, `GetSelectedNodeCount` and `GetRecordIndex` with the declared contract, for ENV's extractor, GPU side and editor; `GetRecordIndex` returns the terrain's record index in 0 … `uGEOM_MAX_TERRAINS` − 1, constant from `AddTerrain`'s return to `RemoveTerrain`, and `uGEOM_TERRAIN_RECORD_INVALID` for a null or stale ID, never failing, allocating or asserting; `AddTerrain` rejects a descriptor whose extents are not its tile counts × 256 × sample spacing.
- **Covers:** Z2-I-GEOM-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck`; `Test_Terrain_NodeSelectionDeterministic`, `Test_Terrain_RecordConstantBeforeFirstDraw` and `Test_CookTerrain_ExtentRejected`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain public API"; Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain record constant"

### Z2-R-GEOM-076 Terrain in the deferred and visibility-buffer paths

- **Statement:** Terrain draws in the G-buffer and, through the manual decode route, in the visibility raster and material resolve, with the same `TerrainVertex` function.
- **Covers:** Z2-I-GEOM-19, Z2-I-GEOM-13
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Test_Terrain_NoCracksAcrossLevels` on the Deferred and visibility-buffer paths; `PathParity` gate step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Morphing and displacement"

### Z2-R-GEOM-077 Skin section of the mesh

- **Statement:** A skinned mesh carries the skin section (skeleton GUID, joint count, remap, posed extent, inverse bind matrices) and mesh-local joint indices, validated by the cook with `GEOM_COOK_ERROR_SKIN_BIND`.
- **Covers:** Z2-I-GEOM-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_SkinWeightsTopFour` and the skin fixtures of `Zenith_CookSkin_Tests.cpp`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Skinning.md → "Skinned mesh data"

### Z2-R-GEOM-078 Posed-extent check

- **Statement:** The cook computes `E = |p_root| + R + D` and chooses the FLOAT16x4 pool layout when `2^(floor(log2 E) − 11)` is at most 1 mm, else FLOAT32x4, reported as a fallback.
- **Covers:** Z2-I-GEOM-14, Z2-I-GEOM-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_PosedExtentFallback`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Skinning.md → "Posed-extent check"

### Z2-R-GEOM-079 Skinned-pose producer interface

- **Statement:** `Flux_RenderSkinnedPose` (instance key, mesh ID, model-space joint matrices by skeleton joint index, morph weights, flags) is the element of the render world's skinned poses, and Flux validates each pose, skipping and counting invalid ones.
- **Covers:** Z2-I-GEOM-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_Skinning_InvalidPoseSkipped`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Skinning.md → "Flux_Skinning header"

### Z2-R-GEOM-080 Skinning palette on the CPU

- **Statement:** Skinning matrices `S_j = M_{remap[j]} × B_j` are computed on the CPU on `Flux.BuildGraph` and laid out by prefix sums of joint counts, independent of the worker count.
- **Covers:** Z2-I-GEOM-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-RHI-004
- **Acceptance evidence:** `Test_Skinning_PaletteLayoutWorkerIndependent`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Skinning.md → "Skinning palette computation"

### Z2-R-GEOM-081 Skinned vertex pool

- **Statement:** The skinned-vertex pool is two graph buffers swapped per frame, allocated in 16-byte granules from `m_uPoolSlot`, holding FLOAT16x4 (vertex v at byte `g × 16 + v × 16`) or FLOAT32x4 (at `g × 16 + v × 24`) positions and SNORM16x4 QTangents per vertex, within `Z2-B-GEOM-001`; raster passes read it through `Flux_FetchPoolVertex` in the `bPERM_SKINNED = true` permutation selected by the raster code, whose `Flux_TransformObjectToClip` ignores the zero POSITION that the pool code's specialised table binds from the null stream.
- **Covers:** Z2-I-GEOM-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062, Z2-D-058
- **Budgets:** Z2-B-GEOM-001
- **Acceptance evidence:** `Test_GPUScene_SkinnedSameFrameAllPasses`, `Test_Skinning_LODSwitchReallocates`, `Test_Skinning_Float32PoolAddressing` and the pool case of `Test_StreamSet_NullStreamBinding`, `Test` step with validation clean.
- **Design:** Zenith2/Docs/Design/GEOM/Skinning.md → "Skinned vertex pool layout"

### Z2-R-GEOM-082 Compute skinning dispatch

- **Statement:** Skinning runs one dispatch per cooked skinned stream-set, in ascending code order, from the frame's internal `Flux_SkinningBatch`, binding that code's skin-source pool (streams 0 and 3) and its raster code's pool (stream 1) and selecting Rendering's enumerated permutations `bPERM_JOINTS16` = SKIN ∈ {J16W8, J16W16}, `bPERM_WEIGHTS16` = SKIN ∈ {J8W16, J16W16} and `bPERM_POSITION32` = POS FLOAT32X3 (input and pool output together), with no format read from data; one thread per vertex, records found by binary search over group prefix sums, linear blend skinning of four weights in kernel operations, re-encoding the QTangent with the bias rule.
- **Covers:** Z2-I-GEOM-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** Z2-B-GEOM-004
- **Acceptance evidence:** `Test_Skinning_MatchesCpuReference` (all 8 permutations), `Test_Skinning_PermutationPerCode` (all 144 cooked skinned codes), `Test_Skinning_BatchLayout` and `Test_Skinning_QTangentReencodedWithBias`, `Test` step on WARP and lavapipe.
- **Design:** Zenith2/Docs/Design/GEOM/Skinning.md → "Compute skinning dispatch"; Zenith2/Docs/Design/GEOM/Skinning.md → "Flux_SkinningBatch"

### Z2-R-GEOM-083 Skinned instances drawn the frame they are skinned

- **Statement:** A skinned mesh is drawn in the depth, base and shadow passes the frame it is skinned, with correct positions, the graph ordering producer before consumers.
- **Covers:** Z2-I-GEOM-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_GPUScene_SkinnedSameFrameAllPasses`, `Test` step with validation clean.
- **Design:** Zenith2/Docs/Design/GEOM/Skinning.md → "Skinning tests"

### Z2-R-GEOM-084 Skinned velocity and history reset

- **Statement:** Velocity of skinned geometry reads the previous pool half, and a history-reset, newly allocated or reallocated instance writes both halves so its velocity is zero.
- **Covers:** Z2-I-GEOM-14, Z2-I-GEOM-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_Skinning_HistoryResetZeroVelocity`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Skinning.md → "Velocity and history reset"

### Z2-R-GEOM-085 Skinned LOD handling

- **Statement:** Skinning deforms the LOD an instance drew last frame, reading that LOD's bind positions and skin streams from the skin-source pool at the LOD's first vertex and its bind QTangents from the raster pool at the `m_iVertexOffset` of the LOD's first non-empty section, reallocating its skinned-pool slot and resetting its history for one frame on a LOD change; raster passes draw the new LOD through the same `bPERM_SKINNED` permutation.
- **Covers:** Z2-I-GEOM-14, Z2-I-GEOM-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Skinning_LODSwitchReallocates`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Skinning.md → "Skinned vertex pool layout"

### Z2-R-GEOM-086 Morph target format

- **Statement:** Morph targets are stored sparsely per LOD (24-byte deltas of position, normal and tangent in FLOAT16, thresholds 1e-5 m and 1e-4), with generated-LOD deltas mapped through the simplifier remap, in the morph-delta pool.
- **Covers:** Z2-I-GEOM-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** Z2-B-GEOM-001
- **Acceptance evidence:** `Test_Cook_MorphDeltasSparse`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Skinning.md → "Morph targets"

### Z2-R-GEOM-087 Morph target application

- **Statement:** The morph pass writes a scratch of bind attributes plus the weighted deltas of the at most 16 largest-magnitude weights (ties by index) in ascending target order before skinning, clamping weights to [−1, 1].
- **Covers:** Z2-I-GEOM-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** Z2-B-GEOM-004
- **Acceptance evidence:** `Test_Skinning_MorphApplied` and `Test_Skinning_MorphTopSixteen`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Skinning.md → "Morph targets"

### Z2-R-GEOM-088 Material document schema

- **Statement:** A material document follows the complete schema version 1 (keys, JSON types, defaults and per-domain objects; `blend` optional for every domain, default `alpha` for transparent and `none` otherwise, `none` rejected for transparent and anything but `none` rejected elsewhere) and its unknown top-level or object keys are preserved with a warning.
- **Covers:** Z2-I-GEOM-16, Z2-I-GEOM-17, Z2-I-GEOM-18
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Material_DocumentRoundTrip` and `Test_Material_UnknownFieldPreserved`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Material document schema"

### Z2-R-GEOM-089 Material migration chain

- **Statement:** Material documents migrate through the `Flux.Material` chain with preview, backup and journaled write, with a fixture per schema version.
- **Covers:** Z2-I-GEOM-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Material_MigrationFixtures`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Material document schema"; Zenith2/Docs/Design/GEOM.md → "Migration and re-cook"

### Z2-R-GEOM-090 Material module conventions

- **Statement:** Material types follow the module conventions (Params within 1,024 bytes in scalar layout, texture usage and UV channel attributes, sampler companion fields, material-level permutation constants, declared shading model) and the reflection exposes them.
- **Covers:** Z2-I-GEOM-16, Z2-I-GEOM-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-015, D-SLANG (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Material_CookTextureUsageChecked` and `Test_Material_ConstantsPackedAtReflectedOffsets`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Material module conventions"; Zenith2/Docs/Design/GEOM/Materials.md → "Material reflection input"

### Z2-R-GEOM-091 Material cook validation

- **Statement:** The material cook rejects unknown names, type mismatches, out-of-set permutation values, missing non-optional textures, domain keys of another domain, unresolved or wrong-usage textures and forbidden samplers, each naming the document, the key and the expectation.
- **Covers:** Z2-I-GEOM-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_Material_CookRejectsTypeMismatch`, `…UnknownPermutationValue`, `…UnknownName`, `…MissingTexture`, `…TextureUsageChecked`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Material cook"

### Z2-R-GEOM-092 Material variant ID

- **Statement:** A material's variant ID is `Zenith_Hash64Stream` over its type ID and its permutation value indices, stable across cooks and independent of parameter values.
- **Covers:** Z2-I-GEOM-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-025, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Material_VariantIDStable`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Material cook"

### Z2-R-GEOM-093 Material file format

- **Statement:** The `.zmat` info, constants and texture binding sections have exactly the listed layouts, and the reader validates every rule of the material reader table without asserting or allocating.
- **Covers:** Z2-I-GEOM-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Material_ReaderRejectsCorruption` and `Test_Material_VersionMismatchRefused` in `Zenith2/Engine/ZenithFlux/Tests/Materials/Flux_MaterialReader_Tests.cpp`, and `Zenith_Fuzz_MaterialFile` (`win-asan` smoke).
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "The cooked material format"; Zenith2/Docs/Design/GEOM/Materials.md → "Material reader validation"

### Z2-R-GEOM-094 Material resource API

- **Statement:** `Flux_MaterialResource`, `Flux_MaterialManager` and `Flux_MaterialID` expose exactly the declared members and contracts.
- **Covers:** Z2-I-GEOM-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck`; `Test_Material_SlotRebindNoPSOChange`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Flux_MaterialResource header"

### Z2-R-GEOM-095 Material constants and slots

- **Statement:** A loaded material's constants block holds its parameters and real texture descriptors in a persistent read-only pool, and each mesh section gets a material-slot record with the bucket IDs of its domain's passes: the GPU scene allocates and reference counts the slot range, and `Flux_MaterialSlotCache` supplies the record images and acquires the depth, colour and shadow buckets through `Flux_BucketRegistry` with RPROD's key, whose stream-set is the mesh's raster code.
- **Covers:** Z2-I-GEOM-16, Z2-I-GEOM-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-058, D-SLANG (open, Stage 1)
- **Budgets:** Z2-B-RPROD-001
- **Acceptance evidence:** `Test_Material_SlotRebindNoPSOChange`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Material slots and constants"

### Z2-R-GEOM-096 Parameter replacement by swap

- **Statement:** Parameter replacements and texture swaps write new constants blocks and rewrite slot records through the scatter upload, never writing a block in place.
- **Covers:** Z2-I-GEOM-16, Z2-I-GEOM-20
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_Material_ParameterSwapNeverInPlace` and `Test_Material_TextureSwapRewritesBlocks`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Material slots and constants"

### Z2-R-GEOM-097 Permutation enumeration

- **Statement:** The cook enumerates the game's material variants and used stream-sets, writes `material_variants.json` and `stream_sets.json`, and compiles exactly the permutations of used (program, variant, stream-set) triples for material programs.
- **Covers:** Z2-I-GEOM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-025, Z2-D-015
- **Budgets:** Z2-B-RPROD-024
- **Acceptance evidence:** `Test_Permutations_EnumerationMatchesUse`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Material permutation enumeration"

### Z2-R-GEOM-098 Missing permutation is a cook error

- **Statement:** A used triple absent from the compiled set fails the cook with `COOK_ERROR_PERMUTATION_MISSING` naming the asset, program and key; at runtime a missing variant uses the missing-PSO fallback.
- **Covers:** Z2-I-GEOM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** none
- **Acceptance evidence:** `Test_Permutations_MissingIsCookError`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Material permutation enumeration"; Zenith2/Docs/Design/GEOM/Materials.md → "Material fallbacks"

### Z2-R-GEOM-099 Permutation count report

- **Statement:** The cook report lists per program its key, stream-set and variant counts and their product, and the per-game total and limit, deterministically.
- **Covers:** Z2-I-GEOM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** Z2-B-RPROD-024
- **Acceptance evidence:** `Test_Permutations_ReportDeterministic`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Permutation limit check and report"

### Z2-R-GEOM-100 Permutation limit enforced

- **Statement:** A per-game permutation total above `Z2-B-RPROD-024` fails the cook with `COOK_ERROR_PERMUTATION_LIMIT` listing the ten largest programs; while the row is pending the check reports without failing.
- **Covers:** Z2-I-GEOM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-025, Z2-D-012
- **Budgets:** Z2-B-RPROD-024
- **Acceptance evidence:** `Test_Permutations_LimitIsCookError`, `Test` step; blocked by the Stage 1 per-system budget ticket for RPROD.
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Permutation limit check and report"

### Z2-R-GEOM-101 Transparent blend modes

- **Statement:** Transparent materials blend with exactly the alpha, premultiplied and additive states of the fixed blend table and write no depth.
- **Covers:** Z2-I-GEOM-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** Z2-B-RPROD-010
- **Acceptance evidence:** `Test_Transparent_BlendStates`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Transparency"

### Z2-R-GEOM-102 Transparent ordering and two-sided pairs

- **Statement:** Transparent sections draw back to front by bounds-centre depth plus sort bias with the Rendering tie-breaks, and a two-sided `backThenFront` material draws its back faces before its front faces.
- **Covers:** Z2-I-GEOM-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_Transparent_TwoSidedBackThenFront` and `Test_HiZ_TransparentOrderPreserved`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Transparency"

### Z2-R-GEOM-103 Transparent refraction and shadows

- **Statement:** Refraction reads the opaque colour copy through the material's IOR and thickness, and `castShadows` `alphaTest` draws the material in the alpha-tested shadow buckets with its shadow cutoff.
- **Covers:** Z2-I-GEOM-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_Transparent_ShadowMode`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Transparency"

### Z2-R-GEOM-104 Decal records and render decals

- **Statement:** `Flux_DecalRecord` has the 96-byte layout listed and `Flux_RenderDecal` is the render world's decal element; invalid decals are dropped and counted.
- **Covers:** Z2-I-GEOM-18
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-015
- **Budgets:** Z2-B-LIGHT-001
- **Acceptance evidence:** `Test_ShaderCompiler_SharedTypesLayoutMatchesReflection` (RPROD) and `Test_Decal_ChannelMask`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Decal records"

### Z2-R-GEOM-105 Decal order and blending

- **Statement:** Decals apply in (priority, stable key) order, fade by angle and box, and blend only the channels of their mask into `Flux_SurfaceData` with reoriented normal blending.
- **Covers:** Z2-I-GEOM-18
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Test_Decal_OrderDeterministic`, `Test_Decal_ChannelMask` and `Test_Decal_AngleFade`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Decal order and application"

### Z2-R-GEOM-106 Deferred and per-object decals

- **Statement:** The Deferred path blends decals into GB0–GB2 with channel write masks, and the Forward path applies at most four per-object decals in record order.
- **Covers:** Z2-I-GEOM-18
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** Z2-B-GEOM-006
- **Acceptance evidence:** `Test_Decal_OrderDeterministic` on the Deferred and Forward paths; `PathParity` gate step.
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Decal order and application"

### Z2-R-GEOM-107 Clear coat surface model

- **Statement:** The clear coat model adds coat strength and roughness to `Flux_SurfaceData`, a fixed-F0 isotropic GGX coat lobe and base attenuation, on every path.
- **Covers:** Z2-I-GEOM-22
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-035, Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_Material_ClearCoatTypeReflects`, `Test_Material_ClearCoatDefaultLitUnchanged` and `Test_Material_ExtendedGBufferRoundTrip`, `Test` step; the lobe's energy proof is LIGHT's `Test_BRDF_AdvancedLobesFurnace` (ruling GB22).
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Advanced surface models"; Zenith2/Docs/Design/GEOM.md → "Classification decisions GEOM owns"

### Z2-R-GEOM-108 Anisotropy surface model

- **Statement:** The anisotropy model adds strength and direction and evaluates anisotropic GGX with the listed α_t and α_b and the anisotropic Smith visibility.
- **Covers:** Z2-I-GEOM-22
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Test_Material_AnisotropyTypeReflects` and `Test_Material_AnisotropyDefaultLitUnchanged`, `Test` step; the lobe's energy proof is LIGHT's `Test_BRDF_AdvancedLobesFurnace` (ruling GB22).
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Advanced surface models"

### Z2-R-GEOM-109 Sheen surface model

- **Statement:** The cloth model adds sheen colour and roughness with the Charlie distribution, the Neubelt visibility and the albedo-LUT energy scaling.
- **Covers:** Z2-I-GEOM-22
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Test_Material_SheenTypeReflects` and `Test_Material_SheenDefaultLitUnchanged`, `Test` step; the lobe's energy proof is LIGHT's `Test_BRDF_AdvancedLobesFurnace` (ruling GB22).
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Advanced surface models"

### Z2-R-GEOM-110 Subsurface approximation surface model

- **Statement:** The subsurface model adds subsurface colour and thickness with wrapped diffuse (w = 0.5) and thickness-attenuated transmission, and no screen-space diffusion pass.
- **Covers:** Z2-I-GEOM-22
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Test_Material_SubsurfaceTypeReflects` and `Test_Material_SubsurfaceDefaultLitUnchanged`, `Test` step; the lobe's energy proof is LIGHT's `Test_BRDF_AdvancedLobesFurnace` (ruling GB22).
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Advanced surface models"

### Z2-R-GEOM-111 Extended surface data in the G-buffer

- **Statement:** The shading model byte keeps the base model in bits 0–1 and the clear-coat and anisotropy flags in bits 2–3; the Deferred and visibility-buffer paths store extended parameters in GB4 R8G8B8A8_UNORM written only by the advanced-surfaces permutation (ruling X80, applied in `Zenith2/Docs/Rendering.md`), so every surface model renders on every path.
- **Covers:** Z2-I-GEOM-22
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-035, Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** `Test_Material_ShadingModelByte`, `Test_Material_ExtendedGBufferRoundTrip` and the `PathParity` gate step on extended-model parity scenes.
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Advanced surface models"

### Z2-R-GEOM-112 Material fallbacks

- **Statement:** A missing, failed or non-resident material draws the default material in shipping builds and the development checker in development builds, counted and logged once per asset.
- **Covers:** Z2-I-GEOM-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_Material_FallbackForMissing`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Material fallbacks"; Zenith2/Docs/Design/GEOM.md → "Failure table"

### Z2-R-GEOM-113 Material preview and reload support

- **Statement:** GEOM provides the material resource, the shader watch-journal reload of material constants and `ReplaceParameters` for EDIT's preview and inspector.
- **Covers:** Z2-I-GEOM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** Z2-B-EDIT-003
- **Acceptance evidence:** `Test_Material_ParameterSwapNeverInPlace` and the editor preview test of EDIT (`Zenith2/Docs/Design/EDIT.md`), `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM.md → "Editor surfaces"

### Z2-R-GEOM-114 Texture usage classes

- **Statement:** Every texture carries one declared usage class (`FLUX_TEXTURE_USAGE_COLOUR`, `_LINEAR`, `_NORMAL`, `_MASK`, `_HEIGHT`, `_HOLE_MASK`, `_SPLAT_INDEX`, `_SPLAT_WEIGHT`) with the listed format per platform and colour space, and the cook never infers the class from a name.
- **Covers:** Z2-I-GEOM-20, Z2-I-GEOM-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-013
- **Budgets:** none
- **Acceptance evidence:** `Test_CookTexture_FormatsPerPlatform`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Textures.md → "Texture usage classes"

### Z2-R-GEOM-115 Mip generation

- **Statement:** The texture cook generates the full mip chain by 2 × 2 averaging (3-wide at odd edges) in linear light for colour, renormalises normal maps and preserves alpha coverage for alpha-tested textures; `HEIGHT` and `HOLE_MASK` textures, of extent `2^n + 1`, are point-decimated instead (mip l texel (i, j) = mip 0 texel (i·2^l, j·2^l), extent `((w − 1) >> l) + 1`).
- **Covers:** Z2-I-GEOM-20, Z2-I-GEOM-19
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CookTexture_MipChainLinearLight`, `…OddExtentFold`, `…NormalRenormalised`, `…AlphaCoveragePreserved` and `Test_CookTexture_HeightDecimation`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Textures.md → "Texture cook pipeline"

### Z2-R-GEOM-116 Block encoding through KTX-Software

- **Statement:** Colour, linear, normal and mask textures are encoded to UASTC single-threaded and transcoded to BC7 on Windows and ASTC 4×4 on Android, deterministically, and no runtime target links KTX-Software.
- **Covers:** Z2-I-GEOM-20
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-013, Z2-D-082
- **Budgets:** Z2-B-PACK-002
- **Acceptance evidence:** `Test_CookTexture_Deterministic` and `Test_CookTexture_FormatsPerPlatform`, `Test` step; `Libgraph` link surface.
- **Design:** Zenith2/Docs/Design/GEOM/Textures.md → "Texture cook pipeline"

### Z2-R-GEOM-117 Texture file format

- **Statement:** The `.ztex` info, tail and per-mip sections have the listed layouts and order, with the tail rule of 64 KiB per layer and mip extents and sizes computed by the per-usage rule (point-decimated `2^n + 1` extents for height and hole textures).
- **Covers:** Z2-I-GEOM-20
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CookTexture_TailRule` and the reader round trips, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Textures.md → "The cooked texture format"

### Z2-R-GEOM-118 Texture reader validation

- **Statement:** The texture reader validates versions, descriptors, computed sizes, the parser limits (16,384, 15 mips, 2,048 layers) and the format for the usage and platform, returning errors without asserting.
- **Covers:** Z2-I-GEOM-20
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Texture_ReaderRejectsCorruption`, `Test_Texture_LimitsAtAndPastLimit` and `Zenith_Fuzz_TextureFile`.
- **Design:** Zenith2/Docs/Design/GEOM/Textures.md → "The cooked texture format"

### Z2-R-GEOM-119 Sampler policy

- **Statement:** Each material texture resolves to a static sampler by the document override or the usage, domain and address-hint default, written into its sampler companion field; no runtime sampler exists, materials cannot select the comparison or reserved entries, and terrain height and hole textures are read with texel `Load` and use no sampler.
- **Covers:** Z2-I-GEOM-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-054
- **Budgets:** none
- **Acceptance evidence:** `Test_SamplerPolicy_DefaultsPerUsage`, `…OverrideResolved`, `…ForbiddenEntriesRejected`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Textures.md → "Sampler policy"

### Z2-R-GEOM-120 Texture resource API

- **Statement:** `Flux_TextureResource`, `Flux_TextureManager`, `Flux_TextureID`, `Flux_TextureResidency` and `Flux_TextureChainDesc` expose exactly the declared members and contracts, including `Flux_TextureResource::IsFallbackBound`, `Flux_TextureResidency::m_bReplaced` and `Flux_TextureManager::ReplaceChainFromMemory`.
- **Covers:** Z2-I-GEOM-20
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck`; `Test_Texture_FallbackPerUsage`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Textures.md → "Flux_Texture header"

### Z2-R-GEOM-121 Tail residency

- **Statement:** A texture's tail mips load with the asset and stay resident, the resident range is always a suffix of the chain, and a texture is drawable once its tail is resident.
- **Covers:** Z2-I-GEOM-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** Z2-B-GEOM-002
- **Acceptance evidence:** `Test_TextureStreaming_BudgetRespected`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Textures.md → "Texture residency model"

### Z2-R-GEOM-122 Required-mip computation

- **Statement:** Required mips come from the projected texel density of instance bounds and part UV densities over a round-robin instance window and the streaming views, with tier and pressure biases, independent of the worker count, merged with external requirements (terrain tiles); a texture not seen, and an external requirement not restated, for 120 frames (`uGEOM_TEXTURE_UNSEEN_FRAMES`) requires only its tail.
- **Covers:** Z2-I-GEOM-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_TextureStreaming_RequiredMipAnalytic`, `Test_TextureStreaming_WorkerIndependent` and `Test_TextureStreaming_ExternalRequirement` (which also covers the 120-frame lapse), `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Textures.md → "Required-mip computation"

### Z2-R-GEOM-123 Residency budget

- **Statement:** Streamed bytes stay within `Z2-B-GEOM-002` times the pressure scale, increases are granted and decreases taken in priority order.
- **Covers:** Z2-I-GEOM-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-058, Z2-D-012
- **Budgets:** Z2-B-GEOM-002
- **Acceptance evidence:** `Test_TextureStreaming_BudgetRespected`, `Test` step; W-Outdoor and W-OpenWorld measurements.
- **Design:** Zenith2/Docs/Design/GEOM/Textures.md → "Texture residency model"

### Z2-R-GEOM-124 Streaming state machine

- **Statement:** Each streamed texture follows the state table with at most one change in flight, a transition-peak reservation before allocation, reads, decodes, uploads and copies, and a swap at the first frame past the copy value.
- **Covers:** Z2-I-GEOM-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** Z2-B-CORE-008
- **Acceptance evidence:** `Test_TextureStreaming_NeverWritesInPlace` and `Test_TextureStreaming_PeakRefusedDefers`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Textures.md → "Streaming state machine"

### Z2-R-GEOM-125 Reallocate-and-swap

- **Statement:** A residency change creates a new texture filled on the copy queue, swapped into every referencing material block by the scatter upload after the copy completes, and the old texture retires through deferred deletion; no texture in use is written in place.
- **Covers:** Z2-I-GEOM-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_TextureStreaming_NeverWritesInPlace`, `Test` step with validation clean.
- **Design:** Zenith2/Docs/Design/GEOM/Textures.md → "Reallocate-and-swap realisation"

### Z2-R-GEOM-126 Retained mips copied on the GPU

- **Statement:** An increase reads only the new mips from disk and copies retained mips from the current texture; a decrease reads nothing.
- **Covers:** Z2-I-GEOM-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** Z2-B-GEOM-003
- **Acceptance evidence:** `Test_TextureStreaming_RetainedMipsCopied`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Textures.md → "Reallocate-and-swap realisation"

### Z2-R-GEOM-127 Two tables sharing a texture during a swap

- **Statement:** Two tables (two materials and a decal) referencing one texture render correctly before, during and after a streaming swap.
- **Covers:** Z2-I-GEOM-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_GPUScene_SharedTextureDuringStreamingSwap`, `Test` step with validation clean.
- **Design:** Zenith2/Docs/Design/GEOM/Textures.md → "Reallocate-and-swap realisation"

### Z2-R-GEOM-128 Streaming cancellation

- **Statement:** Cancelling a residency change in any state releases the new texture, descriptor, staging, reservation, the streamer's asset handle and every operation before returning to the previous residency; streamed mips are read with `Zenith_AssetReader::ReadRange` through a counted `Zenith_AnyAssetHandle` that the streamer holds only while reads are in flight, and each read mip passes `Zenith_VerifySection` before it is decoded.
- **Covers:** Z2-I-GEOM-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_TextureStreaming_Cancel1000Cycles` (`z2-stress`), `Test` and `Sanitizers`.
- **Design:** Zenith2/Docs/Design/GEOM/Textures.md → "Streaming state machine"; Zenith2/Docs/Design/GEOM.md → "Cancellation"

### Z2-R-GEOM-129 Texture memory-pressure shedding

- **Statement:** At warning the pool target scales by 0.75 and the mip bias rises by 1; at critical the bias rises by 2 and textures outside the visible set drop to their tails; each reverts one frame after the level falls.
- **Covers:** Z2-I-GEOM-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** Z2-B-GEOM-002
- **Acceptance evidence:** `Test_TextureStreaming_PressureShedding` and `Test_Memory_WarningShedding`/`Test_Memory_CriticalShedding` (RPROD), `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Textures.md → "Texture residency model"; Zenith2/Docs/Design/GEOM.md → "Failure table"

### Z2-R-GEOM-130 Streaming upload arbitration

- **Statement:** All GEOM copy-queue uploads go through `Flux_StreamingUploader` in the class order and shares listed, within `Z2-B-GEOM-003` per frame, deferring and never dropping excess bytes.
- **Covers:** Z2-I-GEOM-20, Z2-I-GEOM-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** Z2-B-GEOM-003, Z2-B-RHI-006
- **Acceptance evidence:** `Test_TextureStreaming_UploadArbitration`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Textures.md → "Streaming upload arbitration"

### Z2-R-GEOM-131 Virtual texturing boundary

- **Statement:** A texture is either streamed by GEOM or virtual (ADVGFX), each counted in its own budget row, and VT tile uploads use the uploader's `VT_TILE` class.
- **Covers:** Z2-I-GEOM-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** Z2-B-ADVGFX-001, Z2-B-ADVGFX-002
- **Acceptance evidence:** `Test_TextureStreaming_UploadArbitration` with VT tiles queued; ADVGFX's VT tests.
- **Design:** Zenith2/Docs/Design/GEOM/Textures.md → "Virtual texturing boundary"

### Z2-R-GEOM-132 Fallback textures

- **Statement:** A missing, failed or not yet tail-resident texture binds the always-resident fallback of its usage (shipping and development forms), counted and logged once per asset.
- **Covers:** Z2-I-GEOM-20, Z2-I-GEOM-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_Texture_FallbackPerUsage`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Textures.md → "Fallback textures"

### Z2-R-GEOM-133 D-VTX-TANGENTLESS harness

- **Statement:** The harness under `Zenith2/Benchmarks/VertexTangentless/` implements the development-only `bPERM_TANGENTLESS` variant of the Forward+ opaque, G-buffer and resolve programs on the normal-only stream-set, the asset set and the three risk measurements, with a CTest smoke run.
- **Covers:** Z2-I-GEOM-09
- **Stage:** S3
- **Classification:** research
- **Decisions:** Z2-D-084, Z2-D-062, D-VTX-TANGENTLESS (open, Stage 3)
- **Budgets:** none
- **Acceptance evidence:** The harness smoke run in the `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM.md → "Open decisions"; Zenith2/Docs/Design/GEOM.md → "Stage table"

### Z2-R-GEOM-134 D-VTX-TANGENTLESS measurement and ruling

- **Statement:** The ruling ticket measures the three risks on the S24 and the RTX 3070 by the research procedure, commits the report under `Zenith2/Docs/Research/VertexTangentless/`, and appends the ruling (per tier, per material class, or not at all) to the decision log.
- **Covers:** Z2-I-GEOM-09
- **Stage:** S3
- **Classification:** research
- **Decisions:** Z2-D-084, Z2-D-097, D-VTX-TANGENTLESS (open, Stage 3)
- **Budgets:** none
- **Acceptance evidence:** The committed report and the DecisionLog entry; the `needs-human` ruling ticket.
- **Design:** Zenith2/Docs/Design/GEOM.md → "Open decisions"

### Z2-R-GEOM-135 QTangent default until the ruling

- **Statement:** Until D-VTX-TANGENTLESS is ruled no default changes: meshes with normal-mapped materials use QTangent and the tangentless variant exists only in the harness.
- **Covers:** Z2-I-GEOM-09, Z2-I-GEOM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062, D-VTX-TANGENTLESS (open, Stage 3)
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_StreamSetChoice` in the `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM.md → "Open decisions"; Zenith2/Docs/Design/GEOM/Meshes.md → "Stream-set choice"

### Z2-R-GEOM-136 GEOM counters

- **Statement:** GEOM emits every counter of the counters table once per render frame through the profiler hooks, including the queue counters.
- **Covers:** Z2-I-GEOM-07, Z2-I-GEOM-14, Z2-I-GEOM-16, Z2-I-GEOM-20
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_GeomCounters_Emitted`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM.md → "Counters"

### Z2-R-GEOM-137 GEOM debug views

- **Statement:** The debug views `r.debug.geometry` 1–4, `r.debug.textureMips`, `r.debug.forceLOD` and `r.debug.terrainNodes` exist in development builds.
- **Covers:** Z2-I-GEOM-11, Z2-I-GEOM-20, Z2-I-GEOM-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_GeomDebugViews_Render` in `Zenith2/Engine/ZenithFlux/Tests/Geometry/`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM.md → "Debug views"

### Z2-R-GEOM-138 GEOM frame-dump section

- **Statement:** The frame dump carries a `geometry` object with pool occupancy, streaming requests in flight, skinning counts and terrain node count.
- **Covers:** Z2-I-GEOM-07, Z2-I-GEOM-20
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_GeomFrameDump_Section`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM.md → "Frame-dump entries"

### Z2-R-GEOM-139 Cook diagnostics

- **Statement:** Every GEOM cook diagnostic of the table names the asset, source path and offending element, is recorded in the cook report, and errors fail the asset and the package.
- **Covers:** Z2-I-GEOM-08, Z2-I-GEOM-16, Z2-I-GEOM-20
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_DiagnosticsNameAssetAndKey` (one fixture per diagnostic code of the table, asserting the asset GUID, the source path and the offending key or element in the diagnostic and the report), `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM.md → "Cook diagnostics"

### Z2-R-GEOM-140 Error enumerations

- **Statement:** `Zenith_VertexFormatError`, `Flux_GeometryErrorCode` (with the 8-byte record `Flux_GeometryError`) and `Flux_VertexError` are numbered from 1, have `Zenith_ToString`, and each enumerator means what its table row states.
- **Covers:** Z2-I-GEOM-02, Z2-I-GEOM-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck`; `Test_GeomErrors_ToStringEveryEnumerator`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM.md → "Error enumerations"; Zenith2/Docs/Design/GEOM.md → "Geometry error codes"; Zenith2/Docs/Design/GEOM.md → "Vertex format errors"; Zenith2/Docs/Design/GEOM.md → "Flux_GeometryErrors header"

### Z2-R-GEOM-141 No GEOM lock and bounded queues

- **Statement:** GEOM adds no mutex; its three bounded queues have the listed capacities and full policies (the load queue sized from `asset.registry.maxAssets` with `QUEUE_FULL_POLICY_FATAL`), and every GEOM structure is mutated on the `Flux.BuildGraph` node alone, in the eight-step order of "Frame-level flow of GEOM work" (drain the load queues and finalise loads; apply parameter replacements, texture swaps and whole-chain replacements whose copies completed; apply the procedural records and build the procedural frame; apply render-world deltas that reference meshes, materials and procedural instances; terrain node selection, which states the external mip requirements of the tiles; texture streaming `Update`; the skinning palette, dispatch records and batch; declare the prologue passes).
- **Covers:** Z2-I-GEOM-07, Z2-I-GEOM-20
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Sanitizers` (`linux-tsan` cannot run Flux; `win-asan` stress of the load queue), `Test_Mesh_LoadCancel1000Cycles` and `Test_GeomFrame_StepOrder` (a recording spy of the eight steps observes them in order every frame).
- **Design:** Zenith2/Docs/Design/GEOM.md → "Lock-order entries"; Zenith2/Docs/Design/GEOM.md → "Frame-graph placement"; Zenith2/Docs/Design/GEOM.md → "Frame-level flow of GEOM work"

### Z2-R-GEOM-142 Construction unwinds

- **Statement:** Each GEOM manager's factory unwinds exactly what it built when a step fails.
- **Covers:** Z2-I-GEOM-07, Z2-I-GEOM-16, Z2-I-GEOM-20
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_GeomManagers_Create_FailsAt<Step>_Unwinds` per step, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM.md → "Construction and shutdown"

### Z2-R-GEOM-143 GEOM touches no simulation state

- **Statement:** GEOM work runs in the render subgraph, background jobs and cook processes, reads no simulation state and cannot change a simulation result.
- **Covers:** Z2-I-GEOM-07, Z2-I-GEOM-20
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_MemoryGovernor_ShedsNoSimulationState` (CORE) with texture shedding active; review.
- **Design:** Zenith2/Docs/Design/GEOM.md → "Frame-graph placement"

### Z2-R-GEOM-144 GEOM budget rows allocated

- **Statement:** The Stage 1 per-system budget ticket for GEOM, blocked by the GEOM budget harness, allocates `Z2-B-GEOM-001` to `-006`, and each enforcing ticket is blocked by it.
- **Covers:** Z2-I-GEOM-07, Z2-I-GEOM-20, Z2-I-GEOM-14, Z2-I-GEOM-18
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-084
- **Budgets:** Z2-B-GEOM-001, Z2-B-GEOM-002, Z2-B-GEOM-003, Z2-B-GEOM-004, Z2-B-GEOM-005, Z2-B-GEOM-006
- **Acceptance evidence:** The committed allocation in `Zenith2/Docs/Budgets.md` and the DecisionLog entry of the `needs-human` budget ticket.
- **Design:** Zenith2/Docs/Design/GEOM.md → "Rows owned"

### Z2-R-GEOM-145 GEOM budget harness

- **Statement:** The harness `Zenith2/Benchmarks/Budgets/GEOM/` measures pool bytes, texture residency, upload bytes and skinning GPU time on the reference engine workloads, with a CTest smoke run.
- **Covers:** Z2-I-GEOM-07, Z2-I-GEOM-20, Z2-I-GEOM-14
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-084
- **Budgets:** Z2-B-GEOM-001, Z2-B-GEOM-002, Z2-B-GEOM-003, Z2-B-GEOM-004
- **Acceptance evidence:** The harness smoke run in the `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM.md → "Rows owned"; Zenith2/Docs/Design/GEOM.md → "Test targets and gate steps"

### Z2-R-GEOM-146 Pending GEOM row needs recorded

- **Statement:** The rows "Terrain geometry GPU", "Material constants memory" and "Texture streaming request-to-resident latency" are added to Budgets as pending rows produced by the GEOM budget ticket before any ticket enforces them.
- **Covers:** Z2-I-GEOM-19, Z2-I-GEOM-16, Z2-I-GEOM-20
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** The Budgets revision adding the rows; the cross-area notes row.
- **Design:** Zenith2/Docs/Design/GEOM.md → "Pending row needs"

### Z2-R-GEOM-147 Workload measurements

- **Statement:** Skinning, streaming, pool and decal rows are measured in W-Characters, W-Outdoor, W-Indoor and the flagship slices per the Budgets procedures, within their allocated values.
- **Covers:** Z2-I-GEOM-14, Z2-I-GEOM-20, Z2-I-GEOM-18, Z2-I-GEOM-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-GEOM-001, Z2-B-GEOM-002, Z2-B-GEOM-003, Z2-B-GEOM-004, Z2-B-GEOM-006
- **Acceptance evidence:** The workload measurement records of the Stage 3 to Stage 5 acceptances.
- **Design:** Zenith2/Docs/Design/GEOM.md → "Rows measured against"; Zenith2/Docs/Design/GEOM.md → "Device and acceptance runs"

### Z2-R-GEOM-148 Cook steps are pure and multi-process safe

- **Statement:** Each registered GEOM cook step (`geom.mesh`, `geom.material`, `geom.texture`, `geom.permutations`) implements `Zenith_CookStep` with the listed descriptor, adds its derived-data key inputs in the listed order, is a pure function of them, runs in a worker process, writes outputs only through `Zenith_CookContext::WriteAssetFile` or `WriteFile` and its report member only through `AddReportMember` (placed under the report's `members` object), and leaves no partial file when the worker crashes.
- **Covers:** Z2-I-GEOM-07, Z2-I-GEOM-16, Z2-I-GEOM-20
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CookMesh_RepeatCookIdentical`, `Test_CookTexture_Deterministic`, `Test_GeomCook_CleanEqualsIncrementalAllKinds`; with the Stage 4 worker pool, `Test_GeomCook_MultiProcessSameBytes` and `Test_GeomCook_CrashedWorkerLeavesNoFile`.
- **Design:** Zenith2/Docs/Design/GEOM.md → "Cook steps and derived-data keys"; Zenith2/Docs/Design/GEOM.md → "Multi-process cooking"

### Z2-R-GEOM-149 Cooked data re-cooks on version change

- **Statement:** A change to the kernel, a codec, a stream-set layout or a GEOM format record bumps the engine cook or format version in the same diff, and the runtime refuses files of other versions.
- **Covers:** Z2-I-GEOM-02, Z2-I-GEOM-07, Z2-I-GEOM-20
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Mesh_VersionMismatchRefused`, `Test_Material_VersionMismatchRefused` and the texture reader's version case of `Test_Texture_ReaderRejectsCorruption`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM.md → "Migration and re-cook"

### Z2-R-GEOM-150 Engine content cooked, not copied

- **Statement:** Fallback textures, the terrain grid mesh, engine materials and preview meshes are cooked from `Zenith2/Engine/ZenithFlux/Content/` and `Zenith2/Shaders/Materials/`, never copied into a game.
- **Covers:** Z2-I-GEOM-20, Z2-I-GEOM-16, Z2-I-GEOM-19
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Test_Texture_FallbackPerUsage` and the engine-content cook check of ASSET.
- **Design:** Zenith2/Docs/Design/GEOM.md → "ZenithFlux GEOM files"; Zenith2/Docs/Design/GEOM/Textures.md → "Fallback textures"

### Z2-R-GEOM-151 Every reachable stream-set declared

- **Statement:** `Zenith_IsReachableStreamSet` accepts exactly the 255 codes of the reachable-set rule (72 unskinned, 144 skinned cooked and 36 skinned raster MESH codes, 1 TERRAIN and 2 PROCEDURAL codes), and the declared MESH, TERRAIN and PROCEDURAL codes of `StreamSets.slang` equal that set, a reachable code without a declaration being `ZSC_ERROR_STREAM_SET_MISSING` and a declared unreachable code `ZSC_ERROR_STREAM_SET_UNREACHABLE`.
- **Covers:** Z2-I-GEOM-01, Z2-I-GEOM-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062, Z2-D-015
- **Budgets:** Z2-B-RPROD-024
- **Acceptance evidence:** `Test_StreamSet_ReachableRule` in `Zenith2/Engine/ZenithVertexFormat/Tests/` and `Test_StreamSet_DeclarationErrors` in `Zenith2/Tools/ZenithShaderCompiler/Tests/`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Reachable stream-set rule"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set declarations in Slang"

### Z2-R-GEOM-152 Every mandated fallback of every family declared

- **Statement:** For every declared code and every row of the fallback derivation rule that applies to it, the code with that field change is declared, so the FLOAT32x3 position, FLOAT16 UV0 and UV1, UINT16x4 joint, UNORM16x4 weight and FLOAT32 skinned-pool fallbacks exist for skinned, lightmapped and coloured meshes alike, and the cook's choice function returns only declared codes and every one of the 216 cooked MESH codes.
- **Covers:** Z2-I-GEOM-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** Z2-B-RPROD-024
- **Acceptance evidence:** `Test_StreamSet_EveryFallbackDeclared` in `Zenith2/Tools/ZenithShaderCompiler/Tests/Zsc_StreamSets_Tests.cpp` and `Test_Cook_ChoiceReachesOnlyDeclaredCodes` in `Zenith2/Tools/ZenithCook/Tests/Geometry/Zenith_CookStreamSet_Tests.cpp`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Reachable stream-set rule"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "Fallback derivation rule"; Zenith2/Docs/Design/GEOM/Meshes.md → "Stream-set choice"

### Z2-R-GEOM-153 Null vertex stream for an absent attribute

- **Statement:** An attribute a program declares and a drawn stream-set lacks (POSITION for a pool code; UV0, UV1 and COLOR0 for any code) is placed in stream 7 at stride 0, packed contiguously in the program's declaration order in the family default's format for its semantic (for `Flux_MeshVertex` and a pool code: POSITION SNORM16X4 at 0, UV0 8, UV1 12, colour 16; at most four attributes, 20 bytes), the draw binds the renderer's 32-byte zero-filled `Flux_NullVertexStream` there, the attribute reads as zero, both backends build the PSO from the specialised table alone (one extra stride-0 binding on Vulkan, one input element per null attribute in slot 7 and a 32-byte stride-0 vertex buffer view on D3D12), and stream 7 is reserved: no `[StreamSet]` or family struct declares it (`ZSC_ERROR_STREAM_SET_LOCATION`) and `Zenith_ValidateVertexLayout` refuses a non-zero stride on it.
- **Covers:** Z2-I-GEOM-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062, Z2-D-015
- **Budgets:** none
- **Acceptance evidence:** `Test_StreamSet_NullStreamBinding` (with the pool case) in `Zenith2/Engine/ZenithFlux/Tests/VertexFormat/` on lavapipe and WARP with validation clean, `Test_ShaderCompiler_StreamSetTables` and the stream-7 fixtures of `Test_StreamSet_DeclarationErrors`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Program stream-set specialisation"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "Layout validation and hashing"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set declarations in Slang"

### Z2-R-GEOM-154 Surface substitution keeps the program semantic

- **Statement:** When a drawn code's surface form differs from the program's declared surface semantic, the specialised attribute keeps the program's semantic, semantic index and location and takes the code's format, stream and offset, and `Flux_DecodeSurface` decodes per `uFLUX_STREAMSET_SURFACE`, so octahedral and QTangent codes are drawable by one program on both backends.
- **Covers:** Z2-I-GEOM-04, Z2-I-GEOM-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_ShaderCompiler_StreamSetTables` (the `surface-oct16` case) and `Test_Vertex_DepthEqualAcrossPasses`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/VertexFormat.md → "Program stream-set specialisation"

### Z2-R-GEOM-155 Skin-source pool keyed by the cooked code

- **Statement:** A skinned mesh's bind positions (stream 0) and joints and weights (stream 3) live in a compute-only skin-source pool keyed by its cooked code, allocated with the same vertex order as its raster range, never bound to a raster draw, and read by the skinning and morph dispatches at the record's `m_iSourceVertexOffset`, while the bind QTangent is read from the raster pool at the primitive's `m_iVertexOffset`.
- **Covers:** Z2-I-GEOM-14, Z2-I-GEOM-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062, Z2-D-058
- **Budgets:** Z2-B-GEOM-001
- **Acceptance evidence:** `Test_Mesh_PoolsKeyedByRasterAndCookedCode` and `Test_Skinning_PermutationPerCode`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Geometry pools"; Zenith2/Docs/Design/GEOM/Skinning.md → "Compute skinning dispatch"

### Z2-R-GEOM-156 Tied fallback record rule

- **Statement:** A skinned mesh whose posed-extent check alone fails writes a `skinnedPool` record and a forced `position` record whose metric and tolerance are the position check's own passing values and which alone carries `"tiedBy": "bPERM_POSITION32"`, mirrored by flags bit 0 of its precision-section record; a failed position check alone writes one `position` record, and both failing write both records without `tiedBy`.
- **Covers:** Z2-I-GEOM-08, Z2-I-GEOM-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_SkinnedPositionTie`, `Test_Cook_PosedExtentFallback` and `Test_Cook_NoFallbackIsSilent`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Precision fallback report format"; Zenith2/Docs/Design/GEOM/Meshes.md → "The cooked mesh format"; Zenith2/Docs/Design/GEOM/Skinning.md → "Posed-extent check"

### Z2-R-GEOM-157 Terrain extent is a whole number of tiles

- **Statement:** A terrain's extent along each axis equals its tile count × 256 × sample spacing; `Zenith_CookTerrainGeometry` fails any other extent of its input with `GEOM_COOK_ERROR_TERRAIN_EXTENT` naming the axis, the extent and the two nearest multiples, never padding or cropping, and `AddTerrain` rejects a disagreeing descriptor with `FLUX_GEOMETRY_ERROR_INVALID_DESC`.
- **Covers:** Z2-I-GEOM-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CookTerrain_ExtentRejected`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain data"; Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain cook contract"; Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain public API"; Zenith2/Docs/Design/GEOM.md → "Cook diagnostics"

### Z2-R-GEOM-158 Terrain edge handling

- **Statement:** No node lying wholly outside the terrain is emitted; a straddling node's vertex indices are clamped to `[0, samples − 1]` per axis so no geometry extends past the terrain; and normals at the first and last sample are one-sided differences over the clamped span computed in signed arithmetic.
- **Covers:** Z2-I-GEOM-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Terrain_EdgeClamped` on the 384 m fixture, `Test` step on WARP and lavapipe.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Node selection"; Zenith2/Docs/Design/GEOM/Meshes.md → "Morphing and displacement"

### Z2-R-GEOM-159 Material reader validation

- **Statement:** The `.zmat` reader treats files as untrusted and returns the listed error for a bad header, a version mismatch, a missing or duplicated section, out-of-range info fields, a constants size above 1,024 bytes or disagreeing with its section, and a binding whose offset, usage class or UV channel is invalid, allocating nothing and leaving the material `FAILED`.
- **Covers:** Z2-I-GEOM-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Material_ReaderRejectsCorruption` (one mutated fixture per rule) and `Test_Material_VersionMismatchRefused`, `Test` step; `Zenith_Fuzz_MaterialFile` `win-asan` smoke.
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Material reader validation"

### Z2-R-GEOM-160 Procedural geometry interface

- **Statement:** `Flux_ProceduralGeometry.h` declares `Flux_ProceduralUVFormat`, `Flux_ProceduralGeneratorDesc`, `Flux_ProceduralSizing`, the interface `Zenith_ProceduralGeometrySource` (`GetDesc`, `ComputeSizing`, `WriteIndices`, all `const`, called on the `Flux.BuildGraph` node with render-world bytes alone), `Flux_ProceduralInstanceFlag`, `Flux_RenderProceduralInstance` and `Flux_ProceduralGeometrySystem` with `RegisterSource` and `UnregisterSource`, each with its contract block, and the frame value `Flux_ProceduralFrame` is internal.
- **Covers:** Z2-I-RHI-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058, Z2-D-047
- **Budgets:** Z2-B-GEOM-005
- **Acceptance evidence:** `ApiContractCheck` and `HeaderSelfContained` over the header; `Test_Procedural_RegisterValidates`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Procedural.md → "Flux_ProceduralGeometry header"; Zenith2/Docs/Design/GEOM/Procedural.md → "Registration and sizing"

### Z2-R-GEOM-161 Procedural stream-sets and pool layout

- **Statement:** Procedural instances use the declared codes `0x2010` and `0x2030` (the generator's declared UV0 format), and the procedural-geometry pool is one persistent-imported buffer of C vertices with stream 0 at bytes `[0, 8C)` and stream 1 at `[R1, R1 + 12C)`, `R1 = align(8C, 256)`, sharing one vertex allocator, with the usage union of the table, growing by a two-region resize-and-copy within `Z2-B-GEOM-001` and failing with `POOL_FULL` at the clamp without eviction.
- **Covers:** Z2-I-RHI-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062, Z2-D-058
- **Budgets:** Z2-B-GEOM-001
- **Acceptance evidence:** `Test_Procedural_StreamSetPerUVFormat`, `Test_Procedural_WriteVertexMatchesCpuCodec`, `Test_Procedural_PoolGrowPreservesContents` and `Test_Procedural_PoolFullNotDrawn`, `Test` step on WARP and lavapipe.
- **Design:** Zenith2/Docs/Design/GEOM/Procedural.md → "Procedural stream-sets and pool layout"

### Z2-R-GEOM-162 Generator registration

- **Statement:** `RegisterSource` refuses after shutdown began, beyond 64 generators, and for a descriptor whose name is empty, too long, outside `[A-Za-z0-9_.]` or colliding, whose program is not a compute program with `Flux_ProceduralDispatchData` first, whose frame parameter count is not a multiple of 16 in [16, 256], or whose UV format or reference resolution is invalid, and otherwise returns the name's `Zenith_Hash64` key; `UnregisterSource` retires every instance of the generator.
- **Covers:** Z2-I-RHI-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_Procedural_RegisterValidates`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Procedural.md → "Registration and sizing"

### Z2-R-GEOM-163 Procedural instance admission

- **Statement:** A new or re-versioned procedural record is admitted by resolving the generator, calling `ComputeSizing` (1 … 65,535 vertices, 3 … 393,216 indices in multiples of 3, finite bounds and UV range), building the position and UV boxes, allocating the slot and index range, writing and range-checking the indices and uploading them, so an invalid record allocates nothing and a topology change keeps drawing the old version until the new one is drawable.
- **Covers:** Z2-I-RHI-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058, Z2-D-062
- **Budgets:** Z2-B-GEOM-001
- **Acceptance evidence:** `Test_Procedural_AdmissionLimits`, `Test_Procedural_TopologyChangeKeepsOldUntilReady` and `Test_Procedural_StreamSetPerUVFormat`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Procedural.md → "Registration and sizing"

### Z2-R-GEOM-164 Procedural regeneration frame

- **Statement:** Each frame, before the GPU scene applies the render-world deltas, the procedural system collects the records flagged `REGENERATE` (the last per instance winning), sorts them by (generator key, instance key), writes the 64-byte `Flux_ProceduralRecord` array and the frame parameters into one upload-ring block, and builds one `Flux_ProceduralDispatch` per generator with group prefix sums and the merged regenerated ranges, deterministically; `Flux_ProceduralWriteVertex` writes each vertex in kernel operations bit-identically to the CPU codec. A `REGENERATE` record for a key with a `PENDING` replacement regenerates the replacement's slot, the old slot is never regenerated after the replacement is admitted and retires when the replacement becomes `DRAWABLE`; a generator's `Params` begins with `Flux_ProceduralDispatchData m_xDispatch` and the two mandated resource fields and may add further persistent read-only resource fields and plain-data fields after them.
- **Covers:** Z2-I-RHI-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058, Z2-D-019
- **Budgets:** Z2-B-GEOM-005
- **Acceptance evidence:** `Test_Procedural_RegeneratesOnlyFlaggedSlots`, `Test_Procedural_FrameDeterministic`, `Test_Procedural_TopologyChangeKeepsOldUntilReady` and `Test_Procedural_WriteVertexMatchesCpuCodec`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Procedural.md → "Per-frame regeneration"; Zenith2/Docs/Design/GEOM/Procedural.md → "Instance states"

### Z2-R-GEOM-165 Flux_AddProceduralGeometry contract

- **Statement:** `Flux_AddProceduralGeometry(Flux_RenderGraph&, const Flux_ProceduralFrame&, Flux_FrameShared&)` declares exactly one async-compute `ProceduralGeometry` pass when the frame has a dispatch and none otherwise, reads the generator inputs, writes `m_xProceduralPool` with full coverage over exactly the regenerated ranges of both regions, issues one dispatch per generator in order, records no barrier and cannot fail.
- **Covers:** Z2-I-RHI-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-055, Z2-D-056, Z2-D-058
- **Budgets:** Z2-B-GEOM-005
- **Acceptance evidence:** `Test_Procedural_PassDeclaredOnlyWhenDirty` and `Test_Procedural_RibbonSeenAfterRegeneration` (GEOM's counterpart of RHI's `Test_GPUScene_ProceduralRegeneratedMidFrame`, with the engine ribbon generator), `Test` step with validation clean.
- **Design:** Zenith2/Docs/Design/GEOM/Procedural.md → "Flux_AddProceduralGeometry contract"

### Z2-R-GEOM-166 Drawing procedural instances

- **Statement:** A drawable procedural instance is one GPU-scene instance with `FLUX_INSTANCE_FLAG_PROCEDURAL`, `m_uPoolSlot` and a one-primitive record whose `m_iVertexOffset` is its slot's first vertex, drawn in its code's buckets through fixed-function streams 0 and 1 of the procedural pool by `Flux_MeshVertex` programs and `Flux_TransformObjectToClip`, so it appears in the depth, colour and shadow passes of the frame it is regenerated with depth-equal agreement.
- **Covers:** Z2-I-RHI-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058, Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** `Test_Procedural_DrawsInEveryPass` and `Test_Procedural_DrawableAfterRegeneration`, `Test` step on WARP and lavapipe with validation clean.
- **Design:** Zenith2/Docs/Design/GEOM/Procedural.md → "Drawing procedural instances"

### Z2-R-GEOM-167 Procedural instance lifecycle and recovery

- **Statement:** Procedural instances move through `PENDING`, `DRAWABLE` and `RETIRING` as the state table states, cancel queued index uploads and free their slot and index range through deferred deletion on removal, return every resource over 1,000 randomised cycles, and after device loss re-upload their indices and regenerate from the full-state records before drawing again.
- **Covers:** Z2-I-RHI-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** Z2-B-GEOM-001
- **Acceptance evidence:** `Test_Procedural_Lifecycle1000Cycles` and `Test_Procedural_DeviceLossRegenerates`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Procedural.md → "Registration and sizing"; Zenith2/Docs/Design/GEOM/Procedural.md → "Procedural failure handling"

### Z2-R-GEOM-168 Asset kind loaders

- **Statement:** GEOM's three kind loaders `Flux_MeshKindLoader`, `Flux_MaterialKindLoader` and `Flux_TextureKindLoader` implement `Zenith_AssetKindLoader` with the listed descriptors (names, format version 1, concurrency caps) and first section rounds, are registered by `Flux_Renderer` at construction, push their object onto `flux.geometry.loadQueue` and call `Activate` from `OnSectionsLoaded` before any drain runs, allocate and queue the uploads at the `Flux.BuildGraph` drain, report GPU residency when those uploads complete and `ReportGpuFailed` for a pool-full allocation, a failed texture creation or a failed upload after activation, report reader and staging failures through `Fail` before activation, end a cancellation in `AcknowledgeCancel` and an unload in `AcknowledgeUnload` (from the drain for a queued object) with nothing left allocated, recreate the existing object's GPU side on a device-recovery load without queuing it twice, and return 0 from `DropCpuCopies`; the load queue's capacity is the smallest power of two at least 2 × `asset.registry.maxAssets`, with `QUEUE_FULL_POLICY_FATAL`, because an object is queued at most once and a registry slot has at most two objects.
- **Covers:** Z2-I-GEOM-07, Z2-I-GEOM-16, Z2-I-GEOM-20
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_GeomLoaders_Contract` in `Zenith2/Engine/ZenithFlux/Tests/Geometry/` (including the activation-before-drain, `ReportGpuFailed` and load-queue capacity cases), `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM.md → "Asset kind loaders"; Zenith2/Docs/Design/GEOM/Meshes.md → "Mesh residency state machine"; Zenith2/Docs/Design/GEOM/Materials.md → "Material slots and constants"; Zenith2/Docs/Design/GEOM/Textures.md → "Texture residency model"; Zenith2/Docs/Design/GEOM.md → "Lock-order entries"

### Z2-R-GEOM-169 Terrain geometry sections

- **Statement:** GEOM's terrain cook writes the four `.zterrain` sections at index 0 with the listed layouts: 0x0101 the 56-byte geometry header, 0x0102 the node min/max pyramid (level-major from level 0, row-major within a level, node index `o_l + z × n_lx + x`), 0x0103 and 0x0104 the height- and hole-tile GUID tables (row-major, no null height entry, a zero hole entry for a tile without holes); the `Environment.TerrainGeometry` stage of ENV's importer `env.terrain` writes them as the terrain product's intermediate `geomSections` and ENV's ASSET-phase step `env.terrain` writes them unchanged into the `.zterrain`, and `Flux_ReadTerrainGeometrySections` reads them field by field for ENV's terrain kind loader, returning `CORRUPT_SECTION` or `LIMIT` for any disagreement without asserting.
- **Covers:** Z2-I-GEOM-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CookTerrain_TilesAndPyramid`, the terrain case of `Test_GeomFiles_UseAssetContainer` and `Test_Terrain_SectionReaderRejectsCorruption`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain cook contract"; Zenith2/Docs/Design/GEOM.md → "Format index"

### Z2-R-GEOM-170 Terrain splat usage classes

- **Statement:** `FLUX_TEXTURE_USAGE_SPLAT_INDEX` and `FLUX_TEXTURE_USAGE_SPLAT_WEIGHT` are `R8G8B8A8_UNORM` on both platforms, stored uncompressed, at most 512 texels per axis, never streamed (tail first mip 0, the whole chain resident with the asset), with point-decimated index mips (mip l texel (i, j) equals mip 0 texel (i·2^l, j·2^l)) and integer-mean weight mips, the fallbacks (0, 0, 0, 0) and (1, 0, 0, 0), and are never bound through a material field; ENV's packed 128 × 128 detail-density image is a `SPLAT_WEIGHT` texture whose channel meaning is ENV's and whose channels GEOM stores, decimates (8 mips, integer mean) and binds as raw values, and `Flux_TextureResource::IsFallbackBound()` reports when `GetSRV()` returns the usage fallback, so a density consumer binds none then.
- **Covers:** Z2-I-GEOM-19, Z2-I-GEOM-20
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CookTexture_SplatMips`, the splat cases of `Test_CookTexture_TailRule` `Test_Texture_FallbackPerUsage` (including `IsFallbackBound`) and the density case of `Test_CookTexture_SourceIntermediate`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Textures.md → "Texture usage classes"; Zenith2/Docs/Design/GEOM/Textures.md → "Texture cook pipeline"; Zenith2/Docs/Design/GEOM/Textures.md → "The cooked texture format"; Zenith2/Docs/Design/GEOM/Textures.md → "Fallback textures"

### Z2-R-GEOM-171 Terrain record constant written before the first draw

- **Statement:** `Flux_TerrainRenderer::AddTerrain`, on `Flux.BuildGraph`, assigns the terrain's record index R (the lowest index in 0 … 1,023 that no live terrain holds and no frame in flight can read), writes its `Flux_TerrainRecord` at R and writes R into the `m_uTerrainRecord` constant of `Flux_TerrainDesc::m_xMaterial` through a new constants block at the material's cooked terrain record offset, and inserts none of the terrain's node instances before the build-node step that publishes that block; every subsequent block of the bound material (a parameter replacement, a texture swap's rewrite, a device-recovery load) keeps R; a material that is missing or FAILED, declares no `m_uTerrainRecord` or already backs a live terrain, a 1,025th live terrain and a full constants pool fail with `MISSING_DEPENDENCY`, `INVALID_DESC` (`m_uDetail` 10), `LIMIT` and `POOL_FULL`, leaving no terrain and no reserved index; and a removed terrain's index is reused only after the last frame that could read its record retires.
- **Covers:** Z2-I-GEOM-19, Z2-I-GEOM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_Terrain_RecordConstantBeforeFirstDraw` in `Zenith2/Engine/ZenithFlux/Tests/Geometry/Flux_Terrain_Tests.cpp` on WARP and lavapipe with validation clean, and the offset case of `Test_Material_CookFromGeneratedDocument`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain record constant"; Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain public API"; Zenith2/Docs/Design/GEOM/Materials.md → "Material slots and constants"; Zenith2/Docs/Design/GEOM/Materials.md → "The cooked material format"; Zenith2/Docs/Design/GEOM.md → "Frame-level flow of GEOM work"

### Z2-R-GEOM-172 Whole-chain texture replace from memory

- **Statement:** `Flux_TextureManager::ReplaceChainFromMemory`, on `Flux.BuildGraph`, replaces the whole mip chain of a tail-resident `HEIGHT`, `HOLE_MASK`, `SPLAT_INDEX` or `SPLAT_WEIGHT` texture from CPU bytes: it validates the usage, extent, mip count and per-mip sizes and the class's chain rule texel by texel (point decimation for heights, holes and splat indices, the integer mean for splat weights, hole bytes 0 or 255), reserves the texture-swap peak, copies the chain into staging, creates a new texture with its own descriptor, uploads it in class `BLOCKING` and publishes it by reallocate-and-swap at the first frame past the copies (material blocks and terrain tile tables rewritten, the old texture retired through deferred deletion); a failure returns its error with nothing created and the texture unchanged, a second call supersedes an unpublished one, and a replaced texture is pinned out of streaming until a reload or a device recovery restores its cooked chain.
- **Covers:** Z2-I-GEOM-20, Z2-I-GEOM-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** Z2-B-CORE-008
- **Acceptance evidence:** `Test_Texture_ReplaceChainFromMemory` in `Zenith2/Engine/ZenithFlux/Tests/Textures/Flux_TextureManager_Tests.cpp` on WARP and lavapipe with validation clean, `Test` step; its 1,000-cycle supersession case under `Sanitizers` (`win-asan`).
- **Design:** Zenith2/Docs/Design/GEOM/Textures.md → "Texture replace from memory"; Zenith2/Docs/Design/GEOM/Textures.md → "Flux_Texture header"; Zenith2/Docs/Design/GEOM/Textures.md → "Streaming state machine"

### Z2-R-GEOM-173 Texture source image intermediate

- **Statement:** The texture step reads one `geom.textureSource` blob per texture product (a 32-byte little-endian header: magic `ZTSI`, version 1, the usage or 0 for the preset's, texel layout `R8`, `R16`, `R8G8B8A8` or `R16G16B16A16`, extent, layers, colour space, address hint and byte count; then the layer-major, top-down texels), which every importer that declares a texture product writes with `Zenith_WriteTextureSource`, ENV's `env.terrain` for its height, hole, splat and density products included; an importer-declared usage or address hint wins over the preset, and a contradiction with an explicit preset value, a layout or colour space not allowed for the usage, a size mismatch, a hole byte other than 0 and 255 or a missing blob is `GEOM_COOK_ERROR_TEXTURE_SOURCE`; the step writes the `.ztex` under the GUID `DeclareProduct` gave the importer and never mints one; and the blob's bytes are its first derived-data key input.
- **Covers:** Z2-I-GEOM-20, Z2-I-GEOM-19
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CookTexture_SourceIntermediate` in `Zenith2/Tools/ZenithCook/Tests/Textures/Zenith_CookTexture_Tests.cpp` and the `GEOM_COOK_ERROR_TEXTURE_SOURCE` case of `Test_Cook_DiagnosticsNameAssetAndKey`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Textures.md → "Texture source image intermediate"; Zenith2/Docs/Design/GEOM/Textures.md → "Texture cook pipeline"; Zenith2/Docs/Design/GEOM.md → "Cook steps and derived-data keys"; Zenith2/Docs/Design/GEOM.md → "Cook diagnostics"

### Z2-R-GEOM-174 Material document intermediate

- **Statement:** The material step reads its document from the product's `geom.materialDocument` blob, the UTF-8 bytes of a material document written by ASSET's `zenith-doc` importer for an authored `.material.json` (unchanged) or by the importer that generates the material (ENV's terrain, detail and water materials) at schema version 1 with `id` the product's GUID and texture references by GUID; a missing blob or an `id` other than the item's GUID is `GEOM_COOK_ERROR_MATERIAL_SCHEMA`; the step records the constants offset of a `uint m_uTerrainRecord` field, or 0xFFFFFFFF without one, in the info record, and the reader rejects an offset that is unaligned, out of range or overlaps a texture binding; and the blob's bytes are its first derived-data key input.
- **Covers:** Z2-I-GEOM-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_Material_CookFromGeneratedDocument` in `Zenith2/Tools/ZenithCook/Tests/Materials/Zenith_CookMaterial_Tests.cpp`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Material document intermediate"; Zenith2/Docs/Design/GEOM/Materials.md → "Material cook"; Zenith2/Docs/Design/GEOM/Materials.md → "The cooked material format"; Zenith2/Docs/Design/GEOM/Materials.md → "Material reader validation"

### Z2-R-GEOM-175 Per-mesh custom cull distance flag

- **Statement:** A mesh cooked with the import preset key `cull.customDistance` true sets mesh-info flag bit 6 of its `.zmesh`, the key enters the mesh's derived-data key, every primitive record image `Flux_MeshWritePrimitiveRecords` writes for it carries `FLUX_PRIMITIVE_FLAG_CUSTOM_CULL_DISTANCE` (bit 29 of `m_uStreamSetAndLOD`), and `Flux_MeshResource::HasCustomCullDistance()` returns true; a mesh cooked without the key has the bit clear in its info and every record and returns false, and the reader refuses a set mesh-info flag bit above 6.
- **Covers:** Z2-I-GEOM-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-062, Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_CustomCullDistancePreset` in `Zenith2/Tools/ZenithCook/Tests/Geometry/Zenith_CookMesh_Tests.cpp` and `Test_Mesh_CustomCullDistanceFlag` in `Zenith2/Engine/ZenithFlux/Tests/Geometry/Flux_MeshManager_Tests.cpp`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "The cooked mesh format"; Zenith2/Docs/Design/GEOM/Meshes.md → "Flux_MeshResource header"; Zenith2/Docs/Design/GEOM.md → "Import preset keys GEOM reads"

### Z2-R-GEOM-176 Render-target textures

- **Statement:** A `geom-constant-texture` document with `"renderTarget": true` (4 channels, 8 or 16 bits, at most 4,096 × 4,096) cooks to a `Flux.Texture` asset with one uncompressed mip (`R8G8B8A8_UNORM`, `_SRGB` or `R16G16B16A16_SFLOAT`), tail first mip 0 and info flag bit 3; the loader creates it with sampled and colour-target usage, never streams it, and once RESIDENT `Flux_TextureResource::GetRenderTargetHandle()` returns a writable texture whose GPU writes in one frame are sampled through `GetSRV()` in the next, while before residency or after a failure the handle is null and the usage fallback is bound; any other shape with `renderTarget` is refused at import, and a cooked file with bit 3 and any other format or chain is refused by the reader.
- **Covers:** Z2-I-GEOM-20
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-058, Z2-D-054
- **Budgets:** none
- **Acceptance evidence:** `Test_Texture_RenderTargetLoadsWritable` in `Zenith2/Engine/ZenithFlux/Tests/Textures/Flux_TextureManager_Tests.cpp` on WARP and lavapipe and `Test_CookTexture_RenderTargetSingleMip` in `Zenith2/Tools/ZenithCook/Tests/Textures/Zenith_CookTexture_Tests.cpp`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Textures.md → "Render-target textures"; Zenith2/Docs/Design/GEOM/Textures.md → "Flux_Texture header"; Zenith2/Docs/Design/GEOM/Textures.md → "The cooked texture format"

### Z2-R-GEOM-177 Standard material surface weather

- **Statement:** From Stage 4 `Material_Standard`'s `Evaluate` ends with ENV's `Env_ApplySurfaceWeather` over the frame's weather block, with the plain parameter `m_fPorosity` (default 0.5) and exposure 1, so wetness darkens albedo and lowers roughness by that function's formula, and a frame with wetness and snow 0 leaves the surface bit-identical to the function-free result.
- **Covers:** Z2-I-GEOM-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** `Test_Material_StandardWeatherApplied` in `Zenith2/Engine/ZenithFlux/Tests/Materials/Flux_Material_Tests.cpp` on WARP and lavapipe, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Materials.md → "Material module conventions"

### Z2-R-GEOM-178 Skinned mesh binds in the skeleton rest pose

- **Statement:** The skin cook reads the skeleton's published `restModelInverse` and refuses, with `GEOM_COOK_ERROR_SKIN_REST_POSE` carried in `COOK_ERROR_STEP_FAILED`, a mesh any of whose joints' inverse bind differs from the skeleton's entry for its remapped joint by more than `1e-5 × max(1, max |b_ik|)` over the 12 affine entries, naming the mesh, both joints and the difference; it never rebinds or re-poses a mesh.
- **Covers:** Z2-I-GEOM-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_SkinRestPoseMismatchRejected` in `Zenith2/Tools/ZenithCook/Tests/Geometry/Zenith_CookSkin_Tests.cpp`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Skinning.md → "Skinned mesh data"

### Z2-R-GEOM-179 Skin shell distance published per mesh

- **Statement:** The skin cook publishes, per skinned mesh, the property `skinShellDistance`: J little-endian f32 values indexed by skeleton joint (J the skeleton's `jointCount`), each the largest bind-pose distance from that joint to a vertex with a non-zero quantised weight on it and 0 for an unweighted joint, read only by ANIM's `anim.clip` step.
- **Covers:** Z2-I-GEOM-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_SkinShellDistancePublished` in `Zenith2/Tools/ZenithCook/Tests/Geometry/Zenith_CookSkin_Tests.cpp`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Skinning.md → "Skinned mesh data"

### Z2-R-GEOM-180 Skeleton ID and morph target names on the mesh resource

- **Statement:** `Flux_MeshResource::GetSkeletonID()` returns the skeleton persistent ID of the mesh's skin section (the null ID for an unskinned mesh) and `GetMorphTargetNameHash(uTarget)` returns the `Zenith_Hash64` of target `uTarget`'s name in LOD 0's morph table order, asserting on an index at or beyond `GetMorphTargetCount()`.
- **Covers:** Z2-I-GEOM-14, Z2-I-GEOM-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Mesh_SkeletonIDFromSkinSection` in `Zenith2/Engine/ZenithFlux/Tests/Geometry/Flux_GeometryPools_Tests.cpp` and `Test_Skinning_MorphTargetNameHash` in `Zenith2/Engine/ZenithFlux/Tests/Skinning/Flux_Skinning_Tests.cpp`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Flux_MeshResource header"; Zenith2/Docs/Design/GEOM/Skinning.md → "Morph targets"

### Z2-R-GEOM-181 Terrain adapter UVs and grid diagonal

- **Statement:** `TerrainGrid.slang` fills the terrain material's `Flux_MaterialInput` with `m_xUV0` = the fragment's world XZ in metres and `m_xUV0Grad` its explicit screen-space gradients (`m_xUV1` and its gradients equal them), and the engine terrain grid mesh splits every quad along its (a, b)–(a + 1, b + 1) diagonal with both triangles facing +Y, the split ENV's CPU height query and PHYS's heightfield use.
- **Covers:** Z2-I-GEOM-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062, Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Test_Terrain_AdapterUVIsWorldXZ` in `Zenith2/Engine/ZenithFlux/Tests/Terrain/Flux_Terrain_Tests.cpp` on WARP and lavapipe and `Test_Import_GridMesh` in `Zenith2/Tools/ZenithCook/Tests/Geometry/Zenith_GeomImporters_Tests.cpp`, `Test` step.
- **Design:** Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain data"; Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain geometry"

## Coverage of inventory items

| Item | Text | Requirements |
|---|---|---|
| Z2-I-GEOM-01 | vertex data format: Slang-declared streams | Z2-R-GEOM-005, Z2-R-GEOM-006, Z2-R-GEOM-007, Z2-R-GEOM-008, Z2-R-GEOM-009, Z2-R-GEOM-026, Z2-R-GEOM-027, Z2-R-GEOM-028, Z2-R-GEOM-030, Z2-R-GEOM-031, Z2-R-GEOM-034, Z2-R-GEOM-035, Z2-R-GEOM-151, Z2-R-GEOM-153 |
| Z2-I-GEOM-02 | vertex data format: the shared CPU/GPU codec | Z2-R-GEOM-001, Z2-R-GEOM-002, Z2-R-GEOM-003, Z2-R-GEOM-004, Z2-R-GEOM-005, Z2-R-GEOM-007, Z2-R-GEOM-010, Z2-R-GEOM-011, Z2-R-GEOM-012, Z2-R-GEOM-013, Z2-R-GEOM-014, Z2-R-GEOM-015, Z2-R-GEOM-020, Z2-R-GEOM-024, Z2-R-GEOM-025, Z2-R-GEOM-033, Z2-R-GEOM-034, Z2-R-GEOM-037, Z2-R-GEOM-051, Z2-R-GEOM-140, Z2-R-GEOM-149 |
| Z2-I-GEOM-03 | vertex data format: a position stream | Z2-R-GEOM-021, Z2-R-GEOM-028, Z2-R-GEOM-032, Z2-R-GEOM-040, Z2-R-GEOM-041, Z2-R-GEOM-049 |
| Z2-I-GEOM-04 | vertex data format: QTangent normals | Z2-R-GEOM-016, Z2-R-GEOM-017, Z2-R-GEOM-018, Z2-R-GEOM-038, Z2-R-GEOM-135, Z2-R-GEOM-154 |
| Z2-I-GEOM-05 | vertex data format: octahedral normals | Z2-R-GEOM-019, Z2-R-GEOM-029, Z2-R-GEOM-039, Z2-R-GEOM-154 |
| Z2-I-GEOM-06 | vertex data format: UNORM16 UVs with scale and bias | Z2-R-GEOM-022, Z2-R-GEOM-040, Z2-R-GEOM-042 |
| Z2-I-GEOM-07 | vertex data format: 16-bit indices with mesh splitting | Z2-R-GEOM-036, Z2-R-GEOM-040, Z2-R-GEOM-045, Z2-R-GEOM-046, Z2-R-GEOM-047, Z2-R-GEOM-048, Z2-R-GEOM-049, Z2-R-GEOM-051, Z2-R-GEOM-052, Z2-R-GEOM-053, Z2-R-GEOM-054, Z2-R-GEOM-055, Z2-R-GEOM-056, Z2-R-GEOM-057, Z2-R-GEOM-058, Z2-R-GEOM-059, Z2-R-GEOM-060, Z2-R-GEOM-061, Z2-R-GEOM-062, Z2-R-GEOM-130, Z2-R-GEOM-136, Z2-R-GEOM-138, Z2-R-GEOM-140, Z2-R-GEOM-141, Z2-R-GEOM-142, Z2-R-GEOM-143, Z2-R-GEOM-144, Z2-R-GEOM-145, Z2-R-GEOM-147, Z2-R-GEOM-148, Z2-R-GEOM-149, Z2-R-GEOM-155, Z2-R-GEOM-168 |
| Z2-I-GEOM-08 | vertex data format: precision-checked fallbacks | Z2-R-GEOM-028, Z2-R-GEOM-029, Z2-R-GEOM-031, Z2-R-GEOM-036, Z2-R-GEOM-039, Z2-R-GEOM-041, Z2-R-GEOM-042, Z2-R-GEOM-043, Z2-R-GEOM-044, Z2-R-GEOM-078, Z2-R-GEOM-139, Z2-R-GEOM-151, Z2-R-GEOM-152, Z2-R-GEOM-156 |
| Z2-I-GEOM-09 | the D-VTX-TANGENTLESS experiment | Z2-R-GEOM-133, Z2-R-GEOM-134, Z2-R-GEOM-135 |
| Z2-I-GEOM-10 | LOD generation | Z2-R-GEOM-064, Z2-R-GEOM-065 |
| Z2-I-GEOM-11 | LOD selection | Z2-R-GEOM-048, Z2-R-GEOM-050, Z2-R-GEOM-063, Z2-R-GEOM-065, Z2-R-GEOM-066, Z2-R-GEOM-067, Z2-R-GEOM-085, Z2-R-GEOM-137 |
| Z2-I-GEOM-12 | instancing | Z2-R-GEOM-056, Z2-R-GEOM-068, Z2-R-GEOM-069, Z2-R-GEOM-095, Z2-R-GEOM-175 |
| Z2-I-GEOM-13 | meshlets | Z2-R-GEOM-070, Z2-R-GEOM-071, Z2-R-GEOM-076 |
| Z2-I-GEOM-14 | skinning | Z2-R-GEOM-023, Z2-R-GEOM-043, Z2-R-GEOM-077, Z2-R-GEOM-078, Z2-R-GEOM-079, Z2-R-GEOM-080, Z2-R-GEOM-081, Z2-R-GEOM-082, Z2-R-GEOM-083, Z2-R-GEOM-084, Z2-R-GEOM-085, Z2-R-GEOM-136, Z2-R-GEOM-144, Z2-R-GEOM-145, Z2-R-GEOM-147, Z2-R-GEOM-155, Z2-R-GEOM-156, Z2-R-GEOM-178, Z2-R-GEOM-179, Z2-R-GEOM-180 |
| Z2-I-GEOM-15 | morph targets | Z2-R-GEOM-084, Z2-R-GEOM-086, Z2-R-GEOM-087, Z2-R-GEOM-180 |
| Z2-I-GEOM-16 | material permutations | Z2-R-GEOM-031, Z2-R-GEOM-088, Z2-R-GEOM-089, Z2-R-GEOM-090, Z2-R-GEOM-091, Z2-R-GEOM-092, Z2-R-GEOM-093, Z2-R-GEOM-094, Z2-R-GEOM-095, Z2-R-GEOM-096, Z2-R-GEOM-097, Z2-R-GEOM-098, Z2-R-GEOM-099, Z2-R-GEOM-100, Z2-R-GEOM-112, Z2-R-GEOM-113, Z2-R-GEOM-136, Z2-R-GEOM-139, Z2-R-GEOM-142, Z2-R-GEOM-146, Z2-R-GEOM-148, Z2-R-GEOM-150, Z2-R-GEOM-159, Z2-R-GEOM-168, Z2-R-GEOM-171, Z2-R-GEOM-174, Z2-R-GEOM-177 |
| Z2-I-GEOM-17 | transparency | Z2-R-GEOM-088, Z2-R-GEOM-101, Z2-R-GEOM-102, Z2-R-GEOM-103 |
| Z2-I-GEOM-18 | decals | Z2-R-GEOM-088, Z2-R-GEOM-104, Z2-R-GEOM-105, Z2-R-GEOM-106, Z2-R-GEOM-144, Z2-R-GEOM-147 |
| Z2-I-GEOM-19 | terrain geometry | Z2-R-GEOM-072, Z2-R-GEOM-073, Z2-R-GEOM-074, Z2-R-GEOM-075, Z2-R-GEOM-076, Z2-R-GEOM-115, Z2-R-GEOM-137, Z2-R-GEOM-146, Z2-R-GEOM-150, Z2-R-GEOM-157, Z2-R-GEOM-158, Z2-R-GEOM-169, Z2-R-GEOM-170, Z2-R-GEOM-171, Z2-R-GEOM-172, Z2-R-GEOM-173, Z2-R-GEOM-181 |
| Z2-I-GEOM-20 | texture streaming | Z2-R-GEOM-050, Z2-R-GEOM-096, Z2-R-GEOM-114, Z2-R-GEOM-115, Z2-R-GEOM-116, Z2-R-GEOM-117, Z2-R-GEOM-118, Z2-R-GEOM-120, Z2-R-GEOM-121, Z2-R-GEOM-122, Z2-R-GEOM-123, Z2-R-GEOM-124, Z2-R-GEOM-125, Z2-R-GEOM-126, Z2-R-GEOM-127, Z2-R-GEOM-128, Z2-R-GEOM-129, Z2-R-GEOM-130, Z2-R-GEOM-131, Z2-R-GEOM-132, Z2-R-GEOM-136, Z2-R-GEOM-137, Z2-R-GEOM-138, Z2-R-GEOM-139, Z2-R-GEOM-141, Z2-R-GEOM-142, Z2-R-GEOM-143, Z2-R-GEOM-144, Z2-R-GEOM-145, Z2-R-GEOM-146, Z2-R-GEOM-147, Z2-R-GEOM-148, Z2-R-GEOM-149, Z2-R-GEOM-150, Z2-R-GEOM-168, Z2-R-GEOM-170, Z2-R-GEOM-172, Z2-R-GEOM-173, Z2-R-GEOM-176 |
| Z2-I-GEOM-21 | sampler policy | Z2-R-GEOM-090, Z2-R-GEOM-114, Z2-R-GEOM-119, Z2-R-GEOM-132 |
| Z2-I-GEOM-22 | advanced surface models | Z2-R-GEOM-107, Z2-R-GEOM-108, Z2-R-GEOM-109, Z2-R-GEOM-110, Z2-R-GEOM-111 |

## Excluded and deferred

Z2-I-GEOM-19 (terrain geometry) is placed S4 by the Inventory; its requirements serve Stage 3 by ruling Q45, because the Roadmap's Stage 3 W-Outdoor deliverable needs terrain (the Inventory row changes in core-doc batch 4: pending core-doc batch 4 (Q45)); its deferred-path and visibility-buffer requirement stays S5. Every GEOM inventory item is classified required; Z2-I-GEOM-09 is served by two research requirements (the harness and the ruling) and one required requirement (QTangent stays the default). No GEOM requirement is deferred, excluded or game-side. The boundaries that keep the area bounded are exclusions of other capabilities from GEOM:

| Not in GEOM | Rationale | Decision or owner |
|---|---|---|
| a material node graph | materials are Slang surface functions plus a `.json` | Z2-D-079 |
| virtualized geometry and hardware ray tracing | deferred to Stage 7 | Z2-D-043 (ADVGFX) |
| virtual texturing | required in Stage 4, owned by ADVGFX; GEOM supplies the cook steps and the upload class | Z2-D-043 |
| hair and groom geometry | excluded | Z2-D-078, Z2-D-043 |
| order-independent transparency | per-instance sorting meets the slices | design ruling G13 of `Zenith2/Docs/Design/GEOM.md → "Design rulings owned by GEOM"` |
| dual-quaternion skinning | linear blend skinning with four weights | design ruling G11 |
| mesh LOD streaming | meshes load whole; world-scale LOD is HLOD | STREAM (Z2-I-STREAM items) |
| importers, import presets, the DCC matrix | content production | CONTENT; D-SEL-FBX (open, Stage 4) |
| terrain sculpting, painting, materials, collision and navigation generation | environment authoring | ENV |
| procedural placement, reproducible procedural world generation, GPU particles | they may implement `Zenith_ProceduralGeometrySource`; GEOM owns the regeneration pool, pass and draw path | ENV (Z2-I-ENV-10), WORLD (Z2-I-WORLD-05), VFX |
