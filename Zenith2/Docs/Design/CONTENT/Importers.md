# CONTENT design: source parsers and importers

This file is part of the CONTENT design family; the entry document is `Zenith2/Docs/Design/CONTENT.md`. It specifies the host-only parser library `ZenithContentCore` (glTF 2.0, images, audio), the four importers CONTENT implements against ASSET's `Zenith_Importer` (`Zenith2/Docs/Design/ASSET/Import.md → "Importer interface"`), every product they declare and every intermediate they write, the importers' crash isolation, the parser limits and the import detail codes. The importers live in `Zenith2/Tools/ZenithCook/Internal/Importers/`; the parsers in `Zenith2/Tools/ContentCore/`.

## Scope of this file

| Topic | Heading |
|---|---|
| the parser library | "ZenithContentCore API" |
| how importers join the cook | "Importer registration" |
| the glTF importer and its products | "glTF importer", "Product keys", "Meshes", "Skins", "Morph targets", "Animations", "Materials", "Textures referenced by materials", "Cameras", "Lights", "Generated model prefab", "Physics collision products" |
| images and audio | "Image importer", "Audio importer" |
| FBX | "FBX importer" |
| the report and the failure rules | "Import report member", "Crash isolation", "Parser limits", "Import detail codes" |

## ZenithContentCore API

### Version and limit constants

```cpp
// Zenith2/Tools/ContentCore/Zenith_ContentTypes.h (with the error types of Zenith2/Docs/Design/CONTENT.md "Errors")
inline constexpr uint32 uCONTENT_GLTF_IMPORT_VERSION = 1;    // joins the gltf importer's m_uVersion; bumped by any change to its intermediates for unchanged inputs
inline constexpr uint32 uCONTENT_IMAGE_IMPORT_VERSION = 1;
inline constexpr uint32 uCONTENT_AUDIO_IMPORT_VERSION = 1;
inline constexpr uint32 uCONTENT_FBX_IMPORT_VERSION = 1;
// Parser limits ("Parser limits"): one constant per limit, each tested at the limit and at limit + 1 (limit − 1 for the
// minimum) through the pure check functions below ([Z2-N-ARCH-953]).
inline constexpr uint64 ulCONTENT_GLTF_MAX_FILE_BYTES = 2147483648ull;               // 2 GiB: a .gltf or .glb, and each external buffer
inline constexpr uint64 ulCONTENT_GLTF_MAX_JSON_BYTES = 67108864ull;                 // 64 MiB: the JSON chunk (Zenith_JsonLimits apply within it)
inline constexpr uint64 ulCONTENT_GLTF_MAX_DATA_URI_BYTES = 268435456ull;            // 256 MiB decoded, per data: URI
inline constexpr uint32 uCONTENT_GLTF_MAX_URI_BYTES = 4096;
inline constexpr uint32 uCONTENT_GLTF_MAX_NODES = 65536;
inline constexpr uint32 uCONTENT_GLTF_MAX_NODE_DEPTH = 64;
inline constexpr uint32 uCONTENT_GLTF_MAX_MESHES = 16384;
inline constexpr uint32 uCONTENT_GLTF_MAX_PRIMITIVES_PER_MESH = 255;
inline constexpr uint32 uCONTENT_GLTF_MAX_VERTICES_PER_PRIMITIVE = 16777216;
inline constexpr uint32 uCONTENT_GLTF_MAX_VERTICES_PER_COMBINED_MESH = 16777216;
inline constexpr uint32 uCONTENT_GLTF_MAX_ACCESSORS = 1048576;
inline constexpr uint32 uCONTENT_GLTF_MAX_BUFFER_VIEWS = 1048576;
inline constexpr uint32 uCONTENT_GLTF_MAX_BUFFERS = 4096;
inline constexpr uint32 uCONTENT_GLTF_MAX_MATERIALS = 4096;
inline constexpr uint32 uCONTENT_GLTF_MAX_TEXTURES = 4096;
inline constexpr uint32 uCONTENT_GLTF_MAX_IMAGES = 4096;
inline constexpr uint32 uCONTENT_GLTF_MAX_SAMPLERS = 4096;
inline constexpr uint32 uCONTENT_GLTF_MAX_SKINS = 1024;
inline constexpr uint32 uCONTENT_GLTF_MAX_JOINTS_PER_SKIN = 1024;
inline constexpr uint32 uCONTENT_GLTF_MAX_MORPH_TARGETS_PER_MESH = 256;
inline constexpr uint32 uCONTENT_GLTF_MAX_ANIMATIONS = 4096;
inline constexpr uint32 uCONTENT_GLTF_MAX_CHANNELS_PER_ANIMATION = 65536;
inline constexpr uint32 uCONTENT_GLTF_MAX_KEYS_PER_CHANNEL = 16777216;
inline constexpr uint32 uCONTENT_GLTF_MAX_CAMERAS = 4096;
inline constexpr uint32 uCONTENT_GLTF_MAX_LIGHTS = 4096;
inline constexpr uint32 uCONTENT_GLTF_MAX_INSTANCES_PER_NODE = 65536;
inline constexpr uint32 uCONTENT_GLTF_MAX_SCENES = 256;
inline constexpr uint32 uCONTENT_IMAGE_MAX_EXTENT = 16384;                           // per axis (STBI_MAX_DIMENSIONS)
inline constexpr uint64 ulCONTENT_IMAGE_MAX_DECODED_BYTES = 2147483648ull;           // width × height × 8
inline constexpr uint64 ulCONTENT_IMAGE_MAX_FILE_BYTES = 1073741824ull;
inline constexpr uint32 uCONTENT_AUDIO_MAX_CHANNELS = 8;                             // and at least 1
inline constexpr uint32 uCONTENT_AUDIO_MIN_SAMPLE_RATE = 8000;
inline constexpr uint32 uCONTENT_AUDIO_MAX_SAMPLE_RATE = 192000;
inline constexpr uint64 ulCONTENT_AUDIO_MAX_DECODED_BYTES = 2147483648ull;           // frames × channels × 4
inline constexpr uint64 ulCONTENT_AUDIO_MAX_FILE_BYTES = 2147483648ull;
inline constexpr uint32 uCONTENT_AUDIO_MAX_MARKERS = 64;                             // kept; further ones dropped with a warning
inline constexpr uint32 uCONTENT_AUDIO_MAX_RIFF_CHUNKS = 4096;                       // WAV and AIFF chunks per file
inline constexpr uint32 uCONTENT_AUDIO_MAX_FLAC_METADATA_BLOCKS = 1024;
inline constexpr uint64 ulCONTENT_AUDIO_MAX_VORBIS_WORK_BYTES = 67108864ull;         // 64 MiB: stb_vorbis's working block

/// @contract value Zenith_ContentLimitRow
/// Ownership: constexpr data; one row per constant above, in the order declared.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: The constant's own unit (bytes, counts, hertz).
/// Errors: None.
/// Version: since Z2 Stage 3. Tool-side; the limit tests and the cross-doc parser-limit rows enumerate it.
/// @endcontract
enum Zenith_ContentLimitKind : uint8
{
	CONTENT_LIMIT_KIND_MAXIMUM = 1,   // a value above m_ulValue fails
	CONTENT_LIMIT_KIND_MINIMUM,       // a value below m_ulValue fails (uCONTENT_AUDIO_MIN_SAMPLE_RATE)
	CONTENT_LIMIT_KIND_KEPT,          // further elements are dropped with a warning, never a failure (uCONTENT_AUDIO_MAX_MARKERS)
};
struct Zenith_ContentLimitRow { const char* const m_szConstant = nullptr; const char* const m_szParser = nullptr; const uint64 m_ulValue = 0; const Zenith_ContentLimitKind m_eKind = {}; };
Zenith_Span<const Zenith_ContentLimitRow> Zenith_GetContentLimits();   // 39 rows, declaration order

/// @contract value Zenith_ContentLimitCounts
/// Ownership: Plain values. Each parser fills its struct from the source's headers, counts and declared sizes before any
///   allocation sized by them and calls its check function; tests fill the members directly.
/// Thread affinity: IMMUTABLE. Members non-const: the parser counts into it element by element; the decoder counts into
///   it.
/// Invalidation: None.
/// Units: Bytes, counts and hertz; each member is compared with the constant its comment names.
/// Errors: None of its own.
/// Version: since Z2 Stage 3. Tool-side.
/// @endcontract
struct Zenith_GltfCounts
{
	uint64 m_ulFileBytes = 0;                    // ulCONTENT_GLTF_MAX_FILE_BYTES: the .gltf or .glb, then each external buffer in turn
	uint64 m_ulJsonBytes = 0;                    // ulCONTENT_GLTF_MAX_JSON_BYTES
	uint64 m_ulLargestDataUriBytes = 0;          // ulCONTENT_GLTF_MAX_DATA_URI_BYTES: decoded size, from the base64 length
	uint64 m_ulLongestUriBytes = 0;              // uCONTENT_GLTF_MAX_URI_BYTES
	uint64 m_ulNodes = 0, m_ulNodeDepth = 0;     // uCONTENT_GLTF_MAX_NODES, uCONTENT_GLTF_MAX_NODE_DEPTH
	uint64 m_ulMeshes = 0, m_ulMostPrimitivesPerMesh = 0;                       // _MESHES, _PRIMITIVES_PER_MESH
	uint64 m_ulMostVerticesPerPrimitive = 0, m_ulMostVerticesPerCombinedMesh = 0;   // _VERTICES_PER_PRIMITIVE, _PER_COMBINED_MESH
	uint64 m_ulAccessors = 0, m_ulBufferViews = 0, m_ulBuffers = 0;            // _ACCESSORS, _BUFFER_VIEWS, _BUFFERS
	uint64 m_ulMaterials = 0, m_ulTextures = 0, m_ulImages = 0, m_ulSamplers = 0;   // _MATERIALS, _TEXTURES, _IMAGES, _SAMPLERS
	uint64 m_ulSkins = 0, m_ulMostJointsPerSkin = 0, m_ulMostMorphTargetsPerMesh = 0;   // _SKINS, _JOINTS_PER_SKIN, _MORPH_TARGETS_PER_MESH
	uint64 m_ulAnimations = 0, m_ulMostChannelsPerAnimation = 0, m_ulMostKeysPerChannel = 0;   // _ANIMATIONS, _CHANNELS_…, _KEYS_…
	uint64 m_ulCameras = 0, m_ulLights = 0, m_ulMostInstancesPerNode = 0, m_ulScenes = 0;   // _CAMERAS, _LIGHTS, _INSTANCES_PER_NODE, _SCENES
};
struct Zenith_ImageCounts
{
	uint64 m_ulFileBytes = 0;                    // ulCONTENT_IMAGE_MAX_FILE_BYTES
	uint64 m_ulWidth = 0, m_ulHeight = 0;        // uCONTENT_IMAGE_MAX_EXTENT, each axis
	uint64 m_ulDecodedBytes = 0;                 // ulCONTENT_IMAGE_MAX_DECODED_BYTES: width × height × (4 or 8), overflow-checked
};
struct Zenith_AudioCounts
{
	uint64 m_ulFileBytes = 0;                                   // ulCONTENT_AUDIO_MAX_FILE_BYTES
	uint64 m_ulChannels = 1;                                    // uCONTENT_AUDIO_MAX_CHANNELS; 0 also fails that row (range 1 … 8)
	uint64 m_ulSampleRate = uCONTENT_AUDIO_MIN_SAMPLE_RATE;     // uCONTENT_AUDIO_MIN_SAMPLE_RATE, uCONTENT_AUDIO_MAX_SAMPLE_RATE
	uint64 m_ulDecodedBytes = 0;                                // ulCONTENT_AUDIO_MAX_DECODED_BYTES: frames × channels × 4
	uint64 m_ulRiffChunks = 0;                                  // uCONTENT_AUDIO_MAX_RIFF_CHUNKS
	uint64 m_ulFlacMetadataBlocks = 0;                          // uCONTENT_AUDIO_MAX_FLAC_METADATA_BLOCKS
	uint64 m_ulVorbisWorkBytes = 0;                             // ulCONTENT_AUDIO_MAX_VORBIS_WORK_BYTES: the next working block
};

/// @contract functions Zenith_ContentLimitChecks
/// Ownership: None; pure functions of their argument.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: As Zenith_ContentLimitCounts.
/// Errors: CONTENT_ERROR_LIMIT_EXCEEDED with m_uDetail CONTENT_IMPORT_DETAIL_LIMIT and m_uIndex the index in
///   Zenith_GetContentLimits() of the first violated row in declaration order; a default-constructed struct passes.
/// Version: since Z2 Stage 3. Tool-side; the parsers call them, never a second copy of the comparisons (the Vorbis decoder's
///   lowered-maximum failure compares a block with an option, not with a constant: Importers.md "Zenith_MediaDecoders header").
/// @endcontract
Zenith_Result<void, Zenith_ContentError> Zenith_CheckGltfCounts(const Zenith_GltfCounts& xCounts);
Zenith_Result<void, Zenith_ContentError> Zenith_CheckImageCounts(const Zenith_ImageCounts& xCounts);
Zenith_Result<void, Zenith_ContentError> Zenith_CheckAudioCounts(const Zenith_AudioCounts& xCounts);
```

The parsers fill their counts struct and call its check function at each point where the counted value becomes known (the glTF parse at step 4 of "Zenith_GltfSource header", before an external buffer is read and before a `data:` URI is decoded; the image decoders after the header; the audio decoders after the header, after each chunk or metadata block counted, and before each Vorbis working block is allocated). Tests drive the functions directly ("Parser limits", "Limit reachability").

### Zenith_ContentConversion header

```cpp
// Zenith2/Tools/ContentCore/Zenith_ContentConversion.h
/// @contract enum Zenith_ContentAxis
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; the importer maps the setting strings "+x" … "-z" to these values.
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
enum Zenith_ContentAxis : uint8
{
	CONTENT_AXIS_POS_X = 1, CONTENT_AXIS_NEG_X, CONTENT_AXIS_POS_Y, CONTENT_AXIS_NEG_Y, CONTENT_AXIS_POS_Z, CONTENT_AXIS_NEG_Z,
};

/// @contract enum Zenith_ContentMirror
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Tool-side; "none", "x", "y", "z".
/// @endcontract
enum Zenith_ContentMirror : uint8 { CONTENT_MIRROR_NONE = 1, CONTENT_MIRROR_X, CONTENT_MIRROR_Y, CONTENT_MIRROR_Z };

/// @contract value Zenith_ContentConversionDesc
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: m_fUnitScale in metres per source unit.
/// Errors: Zenith_MakeContentConversion refuses non-perpendicular axes and a scale outside [0.0001, 10000]
///   (CONTENT_ERROR_INVALID_ARGUMENT, detail CONTENT_IMPORT_DETAIL_AXES_NOT_PERPENDICULAR or _UNIT_SCALE_RANGE).
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
struct Zenith_ContentConversionDesc
{
	const Zenith_ContentAxis m_eSourceUp = CONTENT_AXIS_POS_Y;
	const Zenith_ContentAxis m_eSourceFront = CONTENT_AXIS_POS_Z;
	const Zenith_ContentMirror m_eMirror = CONTENT_MIRROR_NONE;
	const float m_fUnitScale = 1.0f;
};

/// @contract type Zenith_ContentConversion
/// Ownership: A trivially copyable value holding the private members below; a default-constructed conversion is the identity
///   (Q = I, det Q = +1, s = 1, K the identity quaternion), so a Zenith_GltfSourceDesc whose m_xConversion is left default
///   converts nothing.
/// Thread affinity: IMMUTABLE. Members non-const: Zenith_MakeContentConversion default-constructs it and sets the
///   private members.
/// Invalidation: None.
/// Units: Metres after conversion; radians for rotations.
/// Errors: None; every member is total for finite inputs and returns the input unchanged when IsIdentity().
/// Version: since Z2 Stage 4 (identity-only use from Stage 3). Tool-side. The algorithm is
///   Zenith2/Docs/Design/CONTENT/DccMatrix.md "Basis change algorithm".
/// @endcontract
class Zenith_ContentConversion
{
public:
	Zenith_ContentConversion();                                          // the identity; m_xViewCorrection = (0, 0, 0, 1)
	bool IsIdentity() const;
	float GetUnitScale() const;
	float GetDeterminantSign() const;                         // det Q: +1 or -1
	Zenith_Vector3 ConvertPoint(const Zenith_Vector3& xPoint) const;                 // A · p
	Zenith_Vector3 ConvertDirection(const Zenith_Vector3& xDirection) const;         // normalize(Q · d)
	Zenith_Vector4 ConvertTangent(const Zenith_Vector4& xTangent) const;            // (Q · t, w · det Q)
	Zenith_Matrix4 ConvertMatrix(const Zenith_Matrix4& xMatrix) const;              // B · M · B⁻¹
	Zenith_Quaternion ConvertRotation(const Zenith_Quaternion& xRotation) const;     // (w, det Q · Q · v): Q · r · Qᵀ, linear in q
	Zenith_Vector3 ConvertScale(const Zenith_Vector3& xScale) const;                // diag(Q · diag(s) · Qᵀ): permuted, signs kept
	Zenith_Quaternion GetViewCorrection() const;                                    // K; identity when Q = I

private:
	friend Zenith_Result<Zenith_ContentConversion, Zenith_ContentError> Zenith_MakeContentConversion(
		const Zenith_ContentConversionDesc&);
	// Row i of Q as a signed source-axis index: engine axis i = sign(a) × source axis (|a| − 1), a ∈ {±1, ±2, ±3}.
	sint8 m_aiSourceAxisOfEngineAxis[3] = { 1, 2, 3 };
	bool m_bIdentity = true;                         // Q = I and s = 1
	float m_fDeterminantSign = 1.0f;                 // det Q
	float m_fUnitScale = 1.0f;                       // s
	Zenith_Quaternion m_xViewCorrection;             // K
};

Zenith_Result<Zenith_ContentConversion, Zenith_ContentError> Zenith_MakeContentConversion(
	const Zenith_ContentConversionDesc& xDesc);
```

### Zenith_GltfSource header

```cpp
// Zenith2/Tools/ContentCore/Zenith_GltfSource.h
/// @contract value Zenith_GltfReadRequest
/// Ownership: Plain value passed to the read callback; the URI view points into the parsed JSON for the call.
/// Thread affinity: The parse call's thread.
/// Invalidation: After the callback returns.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 3. Tool-side.
/// @endcontract
struct Zenith_GltfReadRequest { Zenith_StringView m_strRelativeUri; uint64 m_ulMaxBytes; bool m_bImage; };

// The importer's adapter over Zenith_ImportContext::ReadInput (relative, validated, recorded as an import input).
using Zenith_PfnGltfRead = Zenith_Result<Zenith_ByteSpan, Zenith_ContentError> (*)(void* pUserData,
	const Zenith_GltfReadRequest& xRequest);

/// @contract value Zenith_GltfSourceDesc
/// Ownership: Plain value read by Zenith_ParseGltfSource; the bytes, the heap and the callback's user data are borrowed for
///   the call.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: A null heap or callback is CONTENT_ERROR_INVALID_ARGUMENT.
/// Version: since Z2 Stage 3. Tool-side.
/// @endcontract
struct Zenith_GltfSourceDesc
{
	const Zenith_ByteSpan m_xSourceBytes = {};  // the .gltf or .glb file
	const Zenith_StringView m_strSourcePath = {}; // Zenith2-relative, for diagnostics only
	const Zenith_PfnGltfRead m_pfnRead = nullptr; // external buffers and images
	void* const m_pReadUserData = nullptr;
	const Zenith_ContentConversion m_xConversion = {}; // DccMatrix.md "Basis change algorithm"
	const uint32 m_uIgnoreImportFlags = 0;     // CONTENT_GLTF_SKIP_* bits: skins, morphs, animations, cameras, lights
	Zenith_Heap* const m_pxHeap = nullptr;
};

enum Zenith_GltfAlphaMode : uint8 { GLTF_ALPHA_MODE_OPAQUE = 1, GLTF_ALPHA_MODE_MASK, GLTF_ALPHA_MODE_BLEND };
enum Zenith_GltfWrap : uint8 { GLTF_WRAP_REPEAT = 1, GLTF_WRAP_CLAMP, GLTF_WRAP_MIRRORED };
enum Zenith_GltfLightType : uint8 { GLTF_LIGHT_DIRECTIONAL = 1, GLTF_LIGHT_POINT, GLTF_LIGHT_SPOT };
enum Zenith_GltfPath : uint8 { GLTF_PATH_TRANSLATION = 1, GLTF_PATH_ROTATION, GLTF_PATH_SCALE, GLTF_PATH_WEIGHTS };
enum Zenith_GltfInterpolation : uint8 { GLTF_INTERPOLATION_STEP = 1, GLTF_INTERPOLATION_LINEAR, GLTF_INTERPOLATION_CUBIC_SPLINE };
inline constexpr uint32 uGLTF_NONE = 0xFFFFFFFFu;

/// @contract value Zenith_GltfSourceElements
/// Ownership: Each struct below is a plain value inside its Zenith_GltfSourceScene; every span and view points into the scene's
///   own storage.
/// Thread affinity: IMMUTABLE once returned.
/// Invalidation: With the owning scene.
/// Units: Metres, radians, linear colour, candela and lux, seconds, after the conversion of the desc.
/// Errors: None.
/// Version: since Z2 Stage 3 (cameras, lights, instancing, transforms of KHR_texture_transform since Stage 4). Tool-side.
/// @endcontract
struct Zenith_GltfTextureRef
{
	const uint32 m_uImage = uGLTF_NONE;         // index into m_axImages; none when the slot is empty
	const uint8 m_uTexCoord = 0;                // 0 or 1
	const Zenith_GltfWrap m_eWrapS = GLTF_WRAP_REPEAT, m_eWrapT = GLTF_WRAP_REPEAT;
	const bool m_bHasTransform = false;          // KHR_texture_transform
	const float m_afOffset[2] = { 0.0f, 0.0f };
	const float m_fRotation = 0.0f;
	const float m_afScale[2] = { 1.0f, 1.0f };
	const float m_fScaleOrStrength = 1.0f;       // normalTexture.scale or occlusionTexture.strength
};
struct Zenith_GltfSourceImage
{
	const Zenith_StringView m_strName = {};      // the image's name, else its URI's file stem, else empty
	const Zenith_StringView m_strUri = {};       // non-empty for an external file
	const Zenith_ByteSpan m_xEncodedBytes = {};  // embedded (buffer view or data URI); empty for an external file
	const bool m_bBasisu = false;                // KHR_texture_basisu (KTX2 bytes)
};
struct Zenith_GltfSourceMaterial
{
	const Zenith_StringView m_strName = {}, m_strZenithId = {};
	const Zenith_GltfAlphaMode m_eAlphaMode = GLTF_ALPHA_MODE_OPAQUE;
	const float m_fAlphaCutoff = 0.5f;
	const bool m_bDoubleSided = false, m_bUnlit = false;
	const float m_afBaseColourFactor[4] = { 1, 1, 1, 1 };
	const float m_fMetallicFactor = 1.0f, m_fRoughnessFactor = 1.0f;
	const float m_afEmissiveFactor[3] = { 0, 0, 0 };
	const float m_fEmissiveStrength = 1.0f;
	const Zenith_GltfTextureRef m_xBaseColour = {}, m_xMetallicRoughness = {}, m_xNormal = {}, m_xOcclusion = {}, m_xEmissive = {};
	const uint32 m_uIgnoredExtensionMask = 0;   // bits of the ignored material extensions, for the report
};
struct Zenith_GltfSourceMorphTarget
{
	const Zenith_StringView m_strName = {};
	const Zenith_Span<const Zenith_Vector3> m_axPositionDeltas = {}, m_axNormalDeltas = {}, m_axTangentDeltas = {}; // per vertex; empty when absent
};
struct Zenith_GltfSourcePrimitive
{
	const uint32 m_uMaterial = uGLTF_NONE;      // none: the glTF default material
	const Zenith_Span<const Zenith_Vector3> m_axPositions = {}, m_axNormals = {};
	const Zenith_Span<const Zenith_Vector4> m_axTangents = {};         // empty when absent or discarded
	const Zenith_Span<const Zenith_Vector2> m_axUV0 = {}, m_axUV1 = {};
	const Zenith_Span<const Zenith_Vector4> m_axColours = {};          // linear RGBA
	const uint8 m_uInfluences = 0;                                    // 0, 4 or 8
	const Zenith_Span<const uint16> m_auJoints = {};                  // m_uInfluences per vertex, skin joint indices
	const Zenith_Span<const float> m_afWeights = {};                   // m_uInfluences per vertex, normalised
	const Zenith_Span<const uint32> m_auIndices = {};                 // triangle list, counter-clockwise
	const Zenith_Span<const Zenith_GltfSourceMorphTarget> m_axMorphTargets = {};
	const bool m_bFlatNormalsGenerated = false, m_bTangentsDiscarded = false;
};
struct Zenith_GltfSourceMesh
{
	const Zenith_StringView m_strName = {}, m_strZenithId = {};
	const Zenith_Span<const Zenith_GltfSourcePrimitive> m_axPrimitives = {};
	const Zenith_Span<const float> m_afDefaultMorphWeights = {};
	const Zenith_Span<const uint32> m_auAuthoredLods = {};            // MSFT_lod: mesh indices of LOD 1 … n
	const Zenith_Span<const float> m_afScreenCoverage = {};            // MSFT_screencoverage, one per level, or empty
	const uint32 m_uUseCount = 0, m_uMirroredUseCount = 0;            // nodes referencing it (with a negative determinant)
};
struct Zenith_GltfSourceNode
{
	const Zenith_StringView m_strName = {}, m_strZenithId = {};
	const uint32 m_uParent = uGLTF_NONE;
	const Zenith_Span<const uint32> m_auChildren = {};
	const Zenith_Transform m_xLocal = {};        // converted TRS
	const Zenith_Matrix4 m_xWorld = {};          // converted world matrix in its scene
	const bool m_bNegativeDeterminant = false;   // det(m_xWorld) < 0
	const uint32 m_uMesh = uGLTF_NONE, m_uSkin = uGLTF_NONE, m_uCamera = uGLTF_NONE, m_uLight = uGLTF_NONE;
	const Zenith_Span<const Zenith_Transform> m_axInstances = {};      // EXT_mesh_gpu_instancing, converted
};
struct Zenith_GltfSourceSkin
{
	const Zenith_StringView m_strName = {};
	const Zenith_Span<const uint32> m_auJoints = {};                  // node indices
	const Zenith_Span<const Zenith_Matrix4> m_axInverseBind = {};      // converted; identity when absent
	const uint32 m_uSkeletonRoot = uGLTF_NONE;
};
struct Zenith_GltfSourceChannel
{
	const uint32 m_uNode = 0;
	const Zenith_GltfPath m_ePath = GLTF_PATH_TRANSLATION;
	const Zenith_GltfInterpolation m_eInterpolation = GLTF_INTERPOLATION_LINEAR;
	const Zenith_Span<const float> m_afTimes = {};                     // seconds, strictly increasing
	const Zenith_Span<const float> m_afValues = {};                    // converted; N × width or 3 × N × width (cubic spline)
};
struct Zenith_GltfSourceAnimation { const Zenith_StringView m_strName = {}, m_strZenithId = {}; const Zenith_Span<const Zenith_GltfSourceChannel> m_axChannels = {}; };
struct Zenith_GltfSourceCamera
{
	const Zenith_StringView m_strName = {};
	const bool m_bPerspective = true;
	const float m_fYFovRadians = 0.0f, m_fNearMetres = 0.0f, m_fFarMetres = 0.0f; // far 0: infinite
	const float m_fOrthoHalfHeightMetres = 0.0f;                              // ymag
};
struct Zenith_GltfSourceLight
{
	const Zenith_StringView m_strName = {};
	const Zenith_GltfLightType m_eType = GLTF_LIGHT_POINT;
	const float m_afColour[3] = { 1, 1, 1 };
	const float m_fIntensity = 1.0f;             // candela (point, spot) or lux (directional)
	const float m_fRangeMetres = 0.0f;           // 0: undefined in the source
	const float m_fInnerConeRadians = 0.0f, m_fOuterConeRadians = 0.7853982f;
};
struct Zenith_GltfSourceSceneRoot { const Zenith_StringView m_strName = {}, m_strZenithId = {}; const Zenith_Span<const uint32> m_auRootNodes = {}; };
struct Zenith_GltfSourceWarning { const Zenith_ContentImportDetail m_eDetail = {}; const uint32 m_uIndex = 0; };

/// @contract type Zenith_GltfSourceScene
/// Ownership: Created by Zenith_ParseGltfSource as a Zenith_Owned; owns every array its elements point into (heap-backed
///   blocks, released by its destructor).
/// Thread affinity: IMMUTABLE after creation; read by the importer's thread. Members non-const: the parser that creates
///   it fills the members one by one.
/// Invalidation: None while it lives.
/// Units: As Zenith_GltfSourceElements.
/// Errors: None of its own.
/// Version: since Z2 Stage 3. Tool-side.
/// @endcontract
class Zenith_GltfSourceScene
{
public:
	Zenith_Span<const Zenith_GltfSourceNode> m_axNodes;
	Zenith_Span<const Zenith_GltfSourceMesh> m_axMeshes;
	Zenith_Span<const Zenith_GltfSourceMaterial> m_axMaterials;
	Zenith_Span<const Zenith_GltfSourceImage> m_axImages;
	Zenith_Span<const Zenith_GltfSourceSkin> m_axSkins;
	Zenith_Span<const Zenith_GltfSourceAnimation> m_axAnimations;
	Zenith_Span<const Zenith_GltfSourceCamera> m_axCameras;
	Zenith_Span<const Zenith_GltfSourceLight> m_axLights;
	Zenith_Span<const Zenith_GltfSourceSceneRoot> m_axScenes;
	uint32 m_uDefaultScene = 0;
	Zenith_StringView m_strGenerator, m_strCopyright;                  // asset.generator, asset.copyright
	Zenith_Span<const Zenith_GltfSourceWarning> m_axWarnings;          // sorted (detail, index), at most 1,024 kept
};

Zenith_Result<Zenith_Owned<Zenith_GltfSourceScene>, Zenith_ContentError> Zenith_ParseGltfSource(
	const Zenith_GltfSourceDesc& xDesc);
```

`Zenith_ParseGltfSource`, in order (each failure returns its error and frees what was built):

1. Detect the container: the first 4 bytes `glTF` (little-endian 0x46546C67) make a `.glb`; check header version 2 and total length equal to the input length; the first chunk is JSON (type 0x4E4F534A), the optional second BIN (0x004E4942); any other chunk order, an overlapping or truncated chunk, or a third chunk is `SOURCE_INVALID` (`CONTENT_IMPORT_DETAIL_GLB_CHUNK`). Otherwise the input is `.gltf` JSON.
2. Parse the JSON with fastgltf over the memory buffer (`fastgltf::Parser` with `fastgltf::Extensions` set to exactly the "supported" rows of `Zenith2/Docs/Design/CONTENT/DccMatrix.md → "glTF extension support"`, options `DontRequireValidAssetMember` off, `LoadExternalBuffers` and `LoadExternalImages` off, `DecomposeNodeMatrices` off, because ContentCore decomposes matrices itself, `Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Matrix nodes and the negative-determinant decomposition"`); the JSON limits of `Zenith2/Docs/Architecture.md → "Security architecture"` apply first (the JSON chunk is checked against them before fastgltf sees it). `MSFT_lod` (`extensions.MSFT_lod.ids` of a node) and `MSFT_screencoverage` (an array in the LOD root node's `extras`, not an extension object) are read by ContentCore itself from the JSON chunk through CORE's `Zenith_JsonDocument` whenever the pinned fastgltf does not expose them as parsed members; the integrating ticket records in `Internal/Gltf/Zenith_GltfParse.cpp`'s header comment which of the two applies to the pinned version, and `Test_GltfSource_ExtensionGating` covers both members either way.
3. Check `extensionsRequired` against the support table ("glTF extension support" refusal rule); record one warning per ignored used extension.
4. Fill a `Zenith_GltfCounts` from the parsed counts (the deepest node by a walk of the parent table; accessor `count` members as declared) and call `Zenith_CheckGltfCounts` before allocating per-element storage. This step precedes every buffer read of step 5 and every accessor bounds check of step 6, so a small file declaring a count above a limit fails `LIMIT_EXCEEDED` here, whatever its buffers hold.
5. Resolve buffers: GLB BIN, `data:` URIs (base64; the decoded size, ⌊3 × length / 4⌋ minus the padding characters, goes into `m_ulLargestDataUriBytes` and is checked before decoding), external URIs through `m_pfnRead` (relative, with `m_ulMaxBytes` the remaining limit; the returned span's size goes into `m_ulFileBytes` and is checked); a failed read is the callback's error.
6. Decode every accessor the scene uses (`Internal/Gltf/Zenith_GltfAccessors.cpp`): bounds-checked offsets (offset + stride × (count − 1) + element size ≤ view length, overflow-checked), sparse substitution, normalisation, `KHR_mesh_quantization` types, `EXT_meshopt_compression` views decoded first through meshoptimizer into a heap block; non-finite floats in positions, normals, tangents, UVs, weights, times or values are `SOURCE_INVALID` (`CONTENT_IMPORT_DETAIL_NON_FINITE`).
7. Build nodes: local TRS (a `matrix` node decomposed by DccMatrix.md "Matrix nodes and the negative-determinant decomposition", so a negative determinant becomes a negative x scale with a proper rotation; a matrix with shear fails `CONTENT_IMPORT_DETAIL_NODE_TRANSFORM`), the parent table (a second parent or a cycle fails), depth ≤ 64, world matrices per scene by a pre-order walk, determinants, then `ConvertMatrix`/`ConvertRotation`/`ConvertScale` of the conversion.
8. Build primitives (DccMatrix.md "Normals and tangents on import", "Winding and the negative-determinant rule"): strips and fans to lists, generated indices, flat normals, normal repair, tangent validation, weight normalisation ("Skins"), conversion of every attribute, the triangle reversal when det Q = −1.
9. Build skins, animations (channels with converted values), materials (with texture references), images (embedded bytes or URIs; never decoded here), cameras and lights (KHR_lights_punctual).
10. Sort the warnings and return the scene.

Complexity: O(file bytes + accessor elements); memory: the decoded attributes plus at most the largest meshopt view twice.

### Zenith_MediaDecoders header

```cpp
// Zenith2/Tools/ContentCore/Zenith_MediaDecoders.h
enum Zenith_ImageSourceFormat : uint8 { IMAGE_SOURCE_FORMAT_PNG = 1, IMAGE_SOURCE_FORMAT_JPEG, IMAGE_SOURCE_FORMAT_TGA, IMAGE_SOURCE_FORMAT_BMP };
enum Zenith_DecodedImageLayout : uint8 { DECODED_IMAGE_LAYOUT_RGBA8 = 1, DECODED_IMAGE_LAYOUT_RGBA16 };

/// @contract value Zenith_DecodedImage
/// Ownership: Plain value; m_xTexels points into the caller's output array.
/// Thread affinity: IMMUTABLE once returned.
/// Invalidation: With the output array.
/// Units: Texels; rows from the top; channels little-endian; straight alpha.
/// Errors: None.
/// Version: since Z2 Stage 3. Tool-side.
/// @endcontract
struct Zenith_DecodedImage
{
	const uint32 m_uWidth = 0, m_uHeight = 0;
	const Zenith_DecodedImageLayout m_eLayout = DECODED_IMAGE_LAYOUT_RGBA8;
	const Zenith_ByteSpan m_xTexels = {};
	const bool m_bHadExifOrientation = false;
};

/// @contract functions Zenith_MediaDecoderFunctions
/// Ownership: Decoded data is appended to the caller's Zenith_Array (heap-backed); nothing else is retained.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Bytes, texels, frames, hertz.
/// Errors: CONTENT_ERROR_SOURCE_INVALID (detail the failing structure), CONTENT_ERROR_LIMIT_EXCEEDED (the limit), CONTENT_ERROR_UNSUPPORTED
///   (a format variant the tables refuse), CONTENT_ERROR_OUT_OF_MEMORY; never an assert on input bytes ([Z2-N-ARCH-951]).
/// Version: since Z2 Stage 3. Tool-side.
/// @endcontract
Zenith_Result<Zenith_ImageSourceFormat, Zenith_ContentError> Zenith_DetectImageSourceFormat(Zenith_ByteSpan xBytes,
	Zenith_StringView strExtension);   // magic first (PNG, JPEG, BMP); TGA by extension alone
Zenith_Result<Zenith_DecodedImage, Zenith_ContentError> Zenith_DecodeImageSource(Zenith_ByteSpan xBytes,
	Zenith_ImageSourceFormat eFormat, Zenith_Array<uint8>& auTexelsOut);

enum Zenith_AudioSourceFormat : uint8 { AUDIO_SOURCE_FORMAT_WAV = 1, AUDIO_SOURCE_FORMAT_AIFF, AUDIO_SOURCE_FORMAT_FLAC,
	AUDIO_SOURCE_FORMAT_OGG_VORBIS, AUDIO_SOURCE_FORMAT_MP3 };
inline constexpr uint64 ulCONTENT_AUDIO_NO_LOOP = 0xFFFFFFFFFFFFFFFFull;
struct Zenith_DecodedAudioMarker { uint64 m_ulFrame; char m_acName[48]; };   // NUL-padded UTF-8, truncated at a code-point boundary

/// @contract value Zenith_DecodedAudio
/// Ownership: Plain value; m_afSamples and m_axMarkers point into the caller's output arrays.
/// Thread affinity: IMMUTABLE once returned. Members non-const: filled through the out-parameter of the decoders.
/// Invalidation: With the output arrays.
/// Units: Frames at m_uSampleRate; samples float in [-1, 1] before the step's clamp.
/// Errors: None.
/// Version: since Z2 Stage 3. Tool-side.
/// @endcontract
struct Zenith_DecodedAudio
{
	uint32 m_uSampleRate = 0;
	uint16 m_uChannels = 0;
	uint32 m_uChannelMask = 0;                  // WAVE_FORMAT_EXTENSIBLE mask; 0 = the default order for the count
	uint64 m_ulFrameCount = 0;
	uint64 m_ulLoopStart = ulCONTENT_AUDIO_NO_LOOP, m_ulLoopEnd = ulCONTENT_AUDIO_NO_LOOP;   // end exclusive
	Zenith_Span<const Zenith_DecodedAudioMarker> m_axMarkers;          // ascending frame, at most 64
	Zenith_Span<const float> m_afSamples;                               // interleaved, m_ulFrameCount × m_uChannels
};

/// @contract value Zenith_AudioDecodeStats
/// Ownership: Plain value owned by the caller; the decoder writes it through Zenith_AudioDecodeOptions::m_pxStatsOut.
/// Thread affinity: IMMUTABLE once the decode call returns; written only by that call. Members non-const: the decode
///   call writes it through m_pxStatsOut.
/// Invalidation: None.
/// Units: Counts; bytes.
/// Errors: None of its own. Filled on success and on failure alike: the decode resets it to zero on entry, then records each
///   stb_vorbis open attempt as it is made.
/// Version: since Z2 Stage 3. Tool-side; a declared observation seam, so the Vorbis growth test reads the block sequence on an
///   ordinary Zenith_Heap and no heap is doubled.
/// @endcontract
struct Zenith_AudioDecodeStats
{
	uint32 m_uVorbisOpens = 0;                    // stb_vorbis_open_memory calls made, each with one working block
	uint64 m_aulVorbisBlockBytes[32] = {};        // the block of open i at index i (i < 32); 64 B to 64 MiB is 21 doublings
};

/// @contract value Zenith_AudioDecodeOptions
/// Ownership: Plain value; the heap and the stats are borrowed for the call.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: Zenith_DecodeAudioSource returns CONTENT_ERROR_INVALID_ARGUMENT for an initial block of 0 or above the maximum, or a
///   maximum above ulCONTENT_AUDIO_MAX_VORBIS_WORK_BYTES (the options lower the cap, never raise it).
/// Version: since Z2 Stage 3. Tool-side; a declared seam with production defaults, so the Vorbis growth test sets its own sizes
///   and observes the block sequence through m_pxStatsOut.
/// @endcontract
struct Zenith_AudioDecodeOptions
{
	const uint64 m_ulVorbisInitialWorkBytes = 262144;                  // 256 KiB: the first stb_vorbis working block
	const uint64 m_ulVorbisMaxWorkBytes = ulCONTENT_AUDIO_MAX_VORBIS_WORK_BYTES;
	Zenith_Heap* const m_pxHeap = nullptr;                              // the decoders' working memory; null: Zenith_GetRootHeap()
	Zenith_AudioDecodeStats* const m_pxStatsOut = nullptr;              // null: no stats; production passes null
};

// Default options.
Zenith_Result<Zenith_DecodedAudio, Zenith_ContentError> Zenith_DecodeAudioSource(Zenith_ByteSpan xBytes,
	Zenith_AudioSourceFormat eFormat, Zenith_Array<float>& afSamplesOut, Zenith_Array<Zenith_DecodedAudioMarker>& axMarkersOut);
Zenith_Result<Zenith_DecodedAudio, Zenith_ContentError> Zenith_DecodeAudioSource(Zenith_ByteSpan xBytes,
	Zenith_AudioSourceFormat eFormat, const Zenith_AudioDecodeOptions& xOptions, Zenith_Array<float>& afSamplesOut,
	Zenith_Array<Zenith_DecodedAudioMarker>& axMarkersOut);
// Writes AUDIO's decoded-audio layout (Zenith2/Docs/Design/AUDIO/Authoring.md "Audio importer boundary").
Zenith_Result<void, Zenith_ContentError> Zenith_WriteDecodedAudioBlob(const Zenith_DecodedAudio& xAudio,
	Zenith_Array<uint8>& auOut);
```

The single-file libraries are compiled in exactly one translation unit each (`Internal/ThirdParty/`), with `STBI_NO_STDIO`, `STBI_ONLY_PNG`, `STBI_ONLY_JPEG`, `STBI_ONLY_TGA`, `STBI_ONLY_BMP`, `STBI_NO_HDR`, `STBI_NO_LINEAR`, `STBI_MAX_DIMENSIONS 16384`, `STBI_THREAD_LOCAL` left to its default, `DR_WAV_NO_STDIO`, `DR_FLAC_NO_STDIO`, `DR_MP3_NO_STDIO`, `STB_VORBIS_NO_STDIO` and `STB_VORBIS_NO_PUSHDATA_API`, and every allocation routed to `Zenith_Heap` by one of three routes:

| Library | Allocation route |
|---|---|
| stb_image | the macros `STBI_MALLOC(sz)`, `STBI_REALLOC_SIZED(p, oldsz, newsz)` and `STBI_FREE(p)`, defined in `Zenith_StbImageUnit.cpp` over the root heap (`Zenith_GetRootHeap()`, the allowlisted root-heap variable of `Zenith2/Docs/Contracts.md → "No mutable global state"`): `STBI_MALLOC` is `TryAllocate(sz, 16)`, returning null on failure, never `Allocate` and never the OOM-handler path ([Z2-N-CONTRACT-197]); `STBI_REALLOC_SIZED` is `TryAllocate(newsz, 16)`, a copy of `min(oldsz, newsz)` bytes and `Free(p)`, returning null and leaving `p` alive on failure; `STBI_FREE` is `Free`. The macros take no user data, so the route cannot be the decode call's heap. A null decode result is mapped by `stbi_failure_reason()`: `"outofmem"` is `CONTENT_ERROR_OUT_OF_MEMORY`, any other reason `CONTENT_ERROR_SOURCE_INVALID` (`CONTENT_IMPORT_DETAIL_IMAGE_DECODE`) |
| dr_wav, dr_flac, dr_mp3 | the `drwav_allocation_callbacks`, `drflac_allocation_callbacks` and `drmp3_allocation_callbacks` arguments of `drwav_init_memory`, `drflac_open_memory` and `drmp3_init_memory`, whose `pUserData` is `Zenith_AudioDecodeOptions::m_pxHeap` (the root heap when null) and whose functions call `TryAllocate` and `Free`; no macro and no CRT allocation |
| stb_vorbis | `stb_vorbis_open_memory(data, length, &error, &xAlloc)` with `stb_vorbis_alloc { alloc_buffer, alloc_buffer_length_in_bytes }` over a block from the options' heap: first `m_ulVorbisInitialWorkBytes` (256 KiB by default); each open is recorded in `m_pxStatsOut` (when set) before it is made; on `VORBIS_outofmem` the block is freed and, when it was already `m_ulVorbisMaxWorkBytes`, the decode fails by the cap-failure rule below; otherwise the next block is min(2 × size, `m_ulVorbisMaxWorkBytes`), `Zenith_AudioCounts::m_ulVorbisWorkBytes` is set to it and checked through `Zenith_CheckAudioCounts` (which it always passes, the block being at most the maximum and the maximum at most the constant), and the open is retried. With the defaults the sequence is 256 KiB, 512 KiB, …, 64 MiB (nine opens at most). **Cap-failure rule:** when the block that failed was `m_ulVorbisMaxWorkBytes`, the decoder sets `m_ulVorbisWorkBytes` to 2 × that block, the block the decoder would need next, and calls `Zenith_CheckAudioCounts`; with the default maximum (2 × 64 MiB > 64 MiB) that call fails `CONTENT_ERROR_LIMIT_EXCEEDED` (`CONTENT_IMPORT_DETAIL_LIMIT`, `m_uIndex` the `ulCONTENT_AUDIO_MAX_VORBIS_WORK_BYTES` row) and the decode returns it. With a lowered maximum the doubled block can pass the check (2 × 128 B is far below 64 MiB); the decoder then returns the same `CONTENT_ERROR_LIMIT_EXCEEDED` with the same detail and the same row index directly, because an option only lowers that row's value and never adds a row. That direct return compares the block with the option, never with the row's constant, so it is not a second copy of the check function's comparison. With an alloc buffer stb_vorbis never calls the CRT `malloc` (the size is not known before the open, because `stb_vorbis_get_info` needs an opened decoder) |

The OOM fault sweeps of `Zenith2/Docs/Design/CONTENT/Tests.md` reach dr_wav, dr_flac, dr_mp3 and stb_vorbis through the options' heap (a test heap with `Zenith_HeapDesc::m_ulFailAfterAllocations`, [Z2-N-CONTRACT-225]). stb_image is exempt by name: its macros can reach only the process's root heap, whose failure injection a unit test cannot set without failing every other allocation of the test executable, so its out-of-memory path is proven by the mapping above and the fuzz target's allocation limit (`-malloc_limit_mb`), never by a sweep.

No single-file library calls a CRT transcendental: each translation unit `#define`s the transcendental functions the pinned library calls to CORE's deterministic transcendental functions (X314; "Common format rules" of `Zenith2/Docs/Design/CONTENT.md`) before its implementation `#include`, under the CT-R2 suppression ruling. For stb_vorbis 1.22 these include `sin` and `cos` (`compute_window`, `compute_twiddle_factors`) and `exp`, `log` and `pow` (`lookup1_values`); stb_image calls `pow` only in the code `STBI_NO_HDR` and `STBI_NO_LINEAR` remove. The integrating ticket greps each pinned source for `sin`, `cos`, `tan`, `atan`, `atan2`, `exp`, `log`, `log10`, `pow` and their `f` forms, routes every hit, and records the list in the unit's header comment; a pinned version that calls one the list lacks is routed in the same diff.

<!-- assumption: pending CORE -->
CORE's deterministic transcendental functions (X314; ruling X314-FMA3, which a CORE follow-up ticket specifies) are named here as WORLD's row WD29 names them, `Zenith_Sin(double)`, `Zenith_Cos(double)` and `Zenith_Atan2(double, double)`, and CONTENT assumes `Zenith_Exp(double)`, `Zenith_Log(double)` and `Zenith_Pow(double, double)` with the same contract (a fixed polynomial evaluation specified in binary64, identical bits on every CPU of one build). Single-precision call sites cast to `double` and narrow the result once. When CORE's ticket fixes other names, the `#define` lines and the calls of "Image importer" follow them in its diff.
<!-- /assumption -->

Integer PCM is converted to float by the libraries' own float readers (`drwav_read_pcm_frames_f32`, `drflac_read_pcm_frames_f32`, `drmp3_read_pcm_frames_f32`, `stb_vorbis_get_samples_float_interleaved`): 8-bit WAV PCM, which is unsigned, maps as (x − 128) / 128; signed 16-, 24- and 32-bit PCM as x / 2^(bits − 1); float sources pass through.

The manifest pins their versions (stb-image 2.30, dr-wav 0.13.17, dr-flac 0.12.43, dr-mp3 0.6.40, stb-vorbis 1.22; licences MIT or public domain, recorded by SHIP; rows CT16, CT28 and design ruling CT-R2); manifest entry names are lower-case kebab and one entry per library (there is no `dr_libs` entry). fastgltf parses through simdjson, so the `fastgltf` entry carries `dependsOn: ["simdjson"]` and both build with exceptions off as every third-party row does: simdjson with `SIMDJSON_EXCEPTIONS=0`, fastgltf with its exception-free configuration (`FASTGLTF_COMPILE_AS_CPP20=ON`, `/EHs-c-` and `_HAS_EXCEPTIONS=0`); a pinned fastgltf release that does not build so is a contradiction the integrating ticket reports, never a reason to enable exceptions.

<!-- assumption: pending ARCH, SHIP -->
The five single-file libraries join `Zenith2/Docs/Architecture.md → "Third-party dependencies"` with owner `ZenithContentCore`, integration `header-only`, platform `windows`, usage `tools`, and fastgltf, simdjson (as fastgltf's dependency) and meshoptimizer gain `ZenithContentCore` as an owner beside ZenithCook (cross-doc rows X283, X284 and their name correction X335; manifest and licence rows CT16, CT28).
<!-- /assumption -->

## Importer registration

```cpp
// Zenith2/Tools/ZenithCook/Internal/Importers/Zenith_ContentImporters.h
/// @contract functions Zenith_ContentImporterRegistration
/// Ownership: Registers the four static importer instances with the caller's registry.
/// Thread affinity: MAIN_THREAD, cook services item 4, after ASSET's passthrough importers, before every module's importers,
///   before Freeze.
/// Invalidation: None.
/// Units: None.
/// Errors: The registry's IMPORT_ERROR_INVALID_DESC, returned unchanged after the first refused importer.
/// Version: since Z2 Stage 3 (gltf, image, audio); fbx since Stage 4.
/// @endcontract
Zenith_Result<void, Zenith_ImportError> Zenith_RegisterContentImporters(Zenith_ImporterRegistry& xRegistry);
```

<!-- assumption: pending ASSET -->
The cook services call `Zenith_RegisterContentImporters` at item 4 immediately after ASSET's passthrough importers, and UI's `Zenith_RegisterUIImporters` and every other module's importer registration after it (row CT4); `ZenithEditor` never registers importers (it reaches import through the cook server).
<!-- /assumption -->

Every desc has `m_bOutOfProcess` true ("Crash isolation"); its `m_aszToolVersions` lists the lower-case kebab manifest names `fastgltf`, `simdjson` and `meshoptimizer` (`gltf`, `fbx`), `stb-image` (`image`; and `gltf` for ORM packing), `dr-wav`, `dr-flac`, `dr-mp3` and `stb-vorbis` (`audio`), `ktx-software` (`gltf` for `KHR_texture_basisu`) and the D-SEL-FBX converter's entry (`fbx`).

## glTF importer

### Settings

The `gltf` desc (`m_szID` `"gltf"`, extensions `.gltf` and `.glb`, `m_uVersion` = 1 + `uCONTENT_GLTF_IMPORT_VERSION` + `uANIM_IMPORT_VERSION`, `m_uSettingsVersion` 1, produced kinds (ASSET's script names) `Mesh`, `Material`, `Texture`, `Skeleton`, `AnimationClip`, `Prefab`, `CollisionShape`) declares, in this order (P: platform-overridable). Every key is source-wide: there is no per-product settings layer, because the per-product values the plan needs are carried by owner documents (ANIM's clip and rig documents, attached to the product's sub-asset entry) and by CONTENT's product members (`Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Applying product overrides"`):

| Key | Type | Default | Range or values | P | Read by |
|---|---|---|---|---|---|
| `transform.sourceUp`, `transform.sourceFront`, `transform.mirror`, `transform.unitScale` | ENUM, ENUM, ENUM, FLOAT | `+y`, `+z`, `none`, 1.0 | `Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Conversion settings"` | — | the importer |
| `normal.flipGreen` | BOOL | false | — | — | the importer (embedded normal maps) |
| `import.meshes`, `import.materials`, `import.textures`, `import.skins`, `import.morphTargets`, `import.animations`, `import.cameras`, `import.lights`, `import.prefab` | BOOL | true | — | — | the importer |
| `import.unusedMeshes` | BOOL | false | — | — | the importer |
| `mesh.combine` | ENUM | `perMesh` | `perMesh`, `perSource` | — | the importer ("Meshes") |
| `mesh.excludeNodes` | STRING | `""` | `;`-separated globs over node name paths (`*`, `?`, `**`), at most 1,024 bytes | — | the importer |
| `validate.heightRangeMetres`, `validate.maxExtentMetres`, `validate.frontMarkerNode`, `validate.frontToleranceDegrees`, `validate.strict` | FLOAT_ARRAY (2), FLOAT, STRING, FLOAT, BOOL | `[]`, 0, `""`, 30, false | DccMatrix.md "Bounds and facing report" | — | the importer |
| `precision.positionRelative`, `precision.positionAbsoluteMetres`, `precision.uvTexels`, `precision.uvReferenceResolution`, `precision.weightMetres`, `precision.posedMetres` | FLOAT, FLOAT, FLOAT, INT, FLOAT, FLOAT | 1.52587890625e-05, 0.001, 0.125, 4096, 0.001, 0.001 | `Zenith2/Docs/Design/GEOM/Meshes.md → "Quantisation box and precision checks"` | — | GEOM's mesh step |
| `lod.mode` | ENUM | `generate` | `authored`, `generate`, `none` | — | GEOM |
| `lod.ratios` | FLOAT_ARRAY (≤ 7) | `[0.5, 0.25, 0.125, 0.0625, 0.03125]` | each in (0, 1), decreasing | — | GEOM |
| `lod.maxError`, `lod.pixelError`, `lod.normalWeight`, `lod.uvWeight` | FLOAT | 0.05, 1.0, 0.5, 1.0 | GEOM's ranges | — | GEOM |
| `lod.positionOnlyThreshold` | FLOAT | 0.9 | (0, 1]: the unique-vertex ratio below which GEOM stores position-only indices (`Zenith2/Docs/Design/GEOM/Meshes.md → "Position-only indices"`) | — | GEOM's mesh step |
| `lightmapped` | BOOL | false | — | — | GEOM (stream-set choice) |
| `cull.customDistance` | BOOL | false | — | — | GEOM's mesh step: mesh-info flag bit 6, so every primitive record carries `FLUX_PRIMITIVE_FLAG_CUSTOM_CULL_DISTANCE` (`Zenith2/Docs/Design/GEOM.md → "Import preset keys GEOM reads"`; ENV's foliage need, row RP21) |
| `texture.quality`, `texture.uastcRdoLambda`, `texture.alphaCoverageCutoff`, `texture.virtual` | ENUM, FLOAT, FLOAT, BOOL | `high`, 0, 0, false | the assumption below | `texture.quality`, `texture.uastcRdoLambda` | GEOM's texture step (embedded and packed textures) |
| the clip settings of "Animations" | — | — | — | — | ANIM's clip step: each clip's defaults, which the clip's annotation document overrides per clip |

<!-- assumption: pending GEOM -->
GEOM's design names `texture.quality`, `texture.uastcRdoLambda`, `texture.alphaCoverageCutoff` and `texture.virtual` (`Zenith2/Docs/Design/GEOM.md → "Import preset keys GEOM reads"`) without types or defaults; CONTENT assumes `texture.quality` ENUM `low`, `medium`, `high` default `high`, `texture.uastcRdoLambda` FLOAT default 0 (RDO off) range [0, 10], `texture.alphaCoverageCutoff` FLOAT default 0 (off) range [0, 1], `texture.virtual` BOOL default false, and the range (0, 1] for `lod.positionOnlyThreshold`, whose 90% default GEOM states (rows CT10, CT21). `texture.usage` and `texture.addressHint` are not declared by `gltf`: its texture products carry their usage and address hint in the source-image header, which wins (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture source image intermediate"`).
<!-- /assumption -->

`Test_ContentImporters_DeclaredKeysAreGeomKeys` compares every key that the `gltf` and `image` descs declare with a "Read by" cell naming GEOM against the key column of `Zenith2/Docs/Design/GEOM.md → "Import preset keys GEOM reads"` (read as Markdown), so a key GEOM does not read, such as a misspelt cull key that rule P6 would only warn about, fails the test.

### Import algorithm

`Zenith_GltfImporter::Import(Zenith_ImportContext& xContext)`:

1. Fault hook ("Crash isolation").
2. Read the resolved settings; build the conversion with `Zenith_MakeContentConversion` (an invalid combination is `IMPORT_ERROR_SETTING_INVALID` with the detail).
3. `Zenith_ParseGltfSource` over `ReadSource()`, with the read callback over `ReadInput` and skip flags from the `import.*` settings; map its error to `IMPORT_ERROR_SOURCE_INVALID`, `_LIMIT_EXCEEDED` or `_OUT_OF_MEMORY`, reporting the detail and index through `Report`.
4. Apply `mesh.excludeNodes`: excluded nodes and their subtrees are dropped from every product (their meshes stay unless no other node uses them).
5. Read `GetPreviousProducts()` (rows CT2 and CT22): each previous sub-asset entry's key, GUID, kind, orphaned flag, fingerprint and owner members, the members as canonical compact JSON text (`m_strMembersJson`, `{}` when none) that the importer parses into its arena; validate CONTENT's members with `Zenith_ValidateProductOverridesJson`; compute every product key ("Product keys") and every product's fingerprint (`Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Product fingerprints"`); run rename recognition (`Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Rename recognition"`) and call `RenameProduct` for each recognised rename.
6. Declare the products in this order, each with `DeclareProduct(key, kind, primary)` followed by `SetProductFingerprint(guid, fingerprint)` (pending ASSET, row CT22), skipping every product whose key's previous entry carries `exclude: true`, whether that entry is live or orphaned ("Applying product overrides"): textures, materials, skeletons and clips (through ANIM's helpers), meshes (and mirrored variants), collision, prefabs. The primary product is the default scene's prefab when `import.prefab` is set, else the first non-excluded mesh in key order, else none.
7. Write each product's intermediate ("Meshes" … "Physics collision products"), applying the material remap (`Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Applying product overrides"`).
8. Measure bounds and expectations (DccMatrix.md "Bounds and facing report"); add the `contentImports` member ("Import report member"); report every warning (at most 256 by ASSET's limit, further ones counted in the member).

The import is a pure function of the source bytes, the bytes of every input read, the resolved settings, the previous sub-asset entries (keys, GUIDs, fingerprints, owner members) and the importer version: no clock, address, thread count, hash-map iteration order or host path reaches an intermediate; every list is processed in source index order and every map in key byte order (`Test_GltfImporter_SameInputsSameIntermediates` imports twice, in process and in a worker, and compares every blob).

## Product keys

| Product | Key | Kind (ASSET's script name: the `subAssets` `kind` and the kind passed to `DeclareProduct`) |
|---|---|---|
| a glTF mesh | `mesh:<name>` | `Mesh` |
| its mirrored variant | `mesh:<name>@mirrored` | `Mesh` |
| the combined mesh (`mesh.combine = perSource`) | `mesh:<source file stem>@combined` (the stem escaped as a name, rule 3) | `Mesh` |
| a material | `material:<name>`; the glTF default material `material:@default` | `Material` |
| an embedded image used in role usage u | `texture:<image name>@<u>` (u ∈ `colour`, `normal`, `mask`, `linear`) | `Texture` |
| a packed occlusion-roughness-metallic texture | `texture:<material name>/orm` | `Texture` |
| a skeleton | `skel:<root joint name>` (ANIM's helper declares it) | `Skeleton` |
| a clip | `anim:<animation name>` or `anim:<animation name>@<root>` (ANIM's helper) | `AnimationClip` |
| a scene's prefab | `prefab:<scene name>` | `Prefab` |
| the collision shape | `collision:shape` | `CollisionShape` |

Name rules, applied to every `<name>`:

1. The name is `extras.zenithId` when the object carries one matching `^[A-Za-z0-9_.-]{1,64}$` (an identity an artist or a DCC script sets so renames never change the key); otherwise the object's `name`. Two objects of one kind with one `zenithId` fail the import with `IMPORT_ERROR_SOURCE_INVALID`, detail `CONTENT_IMPORT_DETAIL_DUPLICATE_ZENITH_ID`, naming both.
2. Control characters (U+0000 … U+001F, U+007F) are removed; a name that is empty after that becomes `<Kind><index>` (`Mesh3`, `Material0`, `Node7`) with `CONTENT_WARNING_UNNAMED_OBJECT`, because index-derived keys change when the source is reordered.
3. **Escaping.** Each of the four reserved bytes is percent-encoded in one left-to-right pass over the name's UTF-8 bytes: `%` → `%25`, `/` → `%2F`, `#` → `%23`, `@` → `%40` (upper-case hex); every other byte is kept. A `zenithId` name contains none of the four, so rule 1's names are unchanged. The escaped text is hashed and stored as the key, never shown: the report and the editor show the object's own name beside its key.
4. An escaped name longer than 200 bytes is cut at the largest boundary at or below 200 bytes that splits neither a UTF-8 code point nor a `%XX` escape, and suffixed `~` followed by the first 4 bytes of `Zenith_Sha256Digest` over the whole escaped name, written as 8 lowercase hex digits: a mesh named 300 × `a` has the key `mesh:` + 200 × `a` + `~9835fa6b`. In a key built from two names (`anim:<animation name>@<root>`) each name is escaped and cut the same way at 100 bytes (ANIM's helper applies both, rows CT25 and CT33), so every key stays within ASSET's 255-byte limit (`Zenith2/Docs/Design/ASSET/Formats.md → "Sub-asset entries"`; the longest form is 5 + 109 + 1 + 109 = 224 bytes). Keys depend on no open decision: SHA-256 is settled, and `Zenith_Hash64`, whose function D-SEL-HASH (open, Stage 1) chooses, is never used in a persisted key.
5. Duplicate names of one kind get `#2`, `#3`, … in source index order (`mesh:Rock`, `mesh:Rock#2`); names are compared after rules 2–4, as escaped and cut text, so two long names that differ only after the cut and share the 4-byte suffix are duplicates here; names taken from `zenithId` are assigned first and never suffixed, so an object whose name equals another object's `zenithId` takes the suffix.

**Product keys are unique by construction.** After rule 3 a name holds no `/`, `#` or `@`, so every byte sequence those three introduce comes from a rule, never from a source: `#<n>` (rule 5), `@mirrored`, `@combined`, `@default`, `@<u>` and `/orm` (the table). Two products of one kind prefix therefore share a key only when their escaped, cut names are equal, which rule 5 makes impossible within a kind; an escaped name that rule 4 cuts is at least 197 bytes long before its 9-byte suffix, so it never equals an uncut name of at most 200 bytes. `Test_GltfImporter_NodeKeysEscapeReservedCharacters` asserts the cases.

### Node keys

Generated entity IDs ("Generated model prefab") are derived from node keys:

1. A node carrying `extras.zenithId` (rule 1's pattern) has the key `id:<zenithId>`, independent of its name, its parent and its position among its siblings; `zenithId` values are unique among the nodes of one source (a duplicate fails `IMPORT_ERROR_SOURCE_INVALID`, `CONTENT_IMPORT_DETAIL_DUPLICATE_ZENITH_ID`, naming both nodes).
2. Any other node's key is its parent's key, `/`, and its own segment; a root node's parent key is the empty string. A node's segment is its name after rules 2, 3 and 4 of "Product keys" (control characters removed, an empty name replaced by `Node<index>`, the four reserved bytes percent-encoded, a long name cut and suffixed), so a segment is never empty and never contains `/`, `#` or `@`; siblings with equal segments are suffixed `#2`, `#3`, … in child order and warned `CONTENT_WARNING_DUPLICATE_SIBLING_NAME` (naming the parent's key and the segment), because reordering such siblings swaps their keys, their entity IDs and the overrides that target them.
3. A path key is therefore rooted at the nearest ancestor carrying a `zenithId`, or at the scene root: `/Hero/Armature/Hips` without identities; `id:Hips/Spine/Chest` beneath a node whose `zenithId` is `Hips`; a root node named `A/B` keys as `/A%2FB`, never as the `/A/B` of a root `A` with a child `B`.
4. Derived entities have keys no node can have, because a segment never begins with `@` and is never empty: the generated root (a scene with several root nodes) is `/`, a view-correction child `<node key>/@view`, an instance of `EXT_mesh_gpu_instancing` `<node key>/@instance<n>` (n decimal, from 0, in the extension's accessor order).

Node keys are unique by construction within a source: siblings' segments are made distinct by the `#<n>` suffix, which no escaped name can contain; `/` separates segments and appears in no segment, so the key of a path determines the path; an identity key is `id:<zid>` with no `/` (the zenithId pattern admits none) and a path key contains `/` (rooted at the scene root, `/…`, or at a zenithId, `id:<zid>/…`), and `zenithId` values are unique, so the two forms never meet; and the derived suffixes `/@view` and `/@instance<n>` and the root key `/` cannot be produced by a node. Two entities of one generated prefab therefore never share an entity ID except through a SHA-256 collision of their local IDs. `Test_GltfImporter_NodeKeysEscapeReservedCharacters` asserts the escaped key text, written by hand, for a root `A/B` beside a root `A` with a child `B` (`/A%2FB`, `/A`, `/A/B`), a camera node `Cam` with a child named `view` under `transform.sourceUp=+z` (`/Cam/view` and the view-correction child `/Cam/@view`), and siblings `Rock`, `Rock`, `Rock#2` (`/Rock`, `/Rock#2`, `/Rock%232`, with meshes of the same names keyed `mesh:Rock`, `mesh:Rock#2`, `mesh:Rock%232`), and that every entity ID and product key of the source is distinct.

No node rename is recognised: the importer keeps no stored state of previous node keys, and a node's identity is exactly its key. Which edits a node's entity ID survives is stated in `Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Reimport guarantees"`.

## Meshes

### Mesh source content

<!-- assumption: pending GEOM -->
GEOM's mesh step reads one intermediate blob `mesh` per mesh product (`Zenith2/Docs/Design/GEOM.md → "Fixtures"` names its writer `Zenith_WriteCookMeshSource`); GEOM owns its byte layout. CONTENT fills GEOM's structure `Zenith_CookMeshSource` (`Tools/ZenithCook/Internal/Geometry/Zenith_CookMeshSource.h`, row CT9) with exactly these members, and GEOM's design states their byte form:

| Member | Content CONTENT writes |
|---|---|
| `m_axPositions`, `m_axNormals` | per vertex, metres and unit vectors in the mesh's model space after conversion |
| `m_axTangents` | per vertex (xyz, w ±1), or empty when any primitive's tangents were discarded or absent (GEOM generates) |
| `m_axUV0`, `m_axUV1` | per vertex, or empty |
| `m_axColours` | per vertex linear RGBA, or empty |
| `m_uInfluences`, `m_auJoints`, `m_afWeights` | 0, 4 or 8 per vertex; mesh joint indices; weights summing to 1 |
| `m_auIndices`, `m_auTriangleSlots` | the LOD 0 triangle list (counter-clockwise) and the material slot of each triangle |
| `m_axSlots` | per material slot: the material product's GUID (or the remap's replacement) and the source material name |
| `m_axMorphTargets` | per target: name and dense per-vertex position, normal and tangent deltas |
| `m_axAuthoredLods` | per authored level 1 … n: its own vertex and index arrays and its screen coverage (−1 when absent) |
| `m_xSkeleton`, `m_auJointRemap`, `m_axInverseBind` | the skeleton product GUID; per mesh joint the skeleton joint index (`Zenith_SkeletonImportResult::m_auNodeToJoint`) and the inverse bind matrix |
<!-- /assumption -->

### Building a mesh product

1. Material slots: the distinct materials of the mesh's primitives in order of first appearance (a primitive without a material uses `material:@default`); at most 255 (`IMPORT_ERROR_LIMIT_EXCEEDED`, GEOM's slot limit).
2. Vertices: the primitives' vertices concatenated in primitive order (glTF primitives share no vertices), indices offset accordingly; every triangle's slot is its primitive's.
3. Attributes present in some primitives and absent in others: UV sets and colours absent in a primitive are written as zeros and white for its vertices (the stream exists for the whole mesh); tangents follow the all-or-nothing rule across the whole mesh (DccMatrix.md "Normals and tangents on import").
4. UV set choice: when every texture of a primitive's material uses `texCoord` 1, that primitive's UV1 is written as UV0 and its UV0 as UV1, because engine materials sample UV0 by default (`[UVChannel]`, `Zenith2/Docs/Design/GEOM/Materials.md → "Material module conventions"`); a material mixing `texCoord` 0 and 1 keeps the source sets and is warned `CONTENT_WARNING_MIXED_TEXCOORDS`.
5. Skinned meshes: written in the skin's space (the node transform is ignored, glTF 2.0); joints remapped per "Skins".
6. Mirrored variant: identical, with every triangle reversed and every tangent w negated (DccMatrix.md "Winding and the negative-determinant rule").
7. `mesh.combine = perSource`: every non-skinned mesh instance of the default scene baked into one mesh in scene space (DccMatrix.md "Winding and the negative-determinant rule"), its slots the union of materials; skinned meshes are still separate products; a combined mesh's vertex count above 16,777,216 is `IMPORT_ERROR_LIMIT_EXCEEDED`.
8. `import.unusedMeshes` false: a mesh no node of any scene references is not declared.

## Skins

1. The importer converts the parsed scene to ANIM's neutral arrays (`Zenith2/Docs/Design/ANIM/Formats.md → "Import conversion"`): one `Zenith_ImportedNode` per node (name, parent, converted local transform), one `Zenith_ImportedSkin` per skin (joint node indices, converted inverse binds, skeleton root), the morph-target names of every mesh bound to a skin, sorted and deduplicated; the arrays live in the importer's scratch arena.
2. It calls `Zenith_BuildSkeletons(nodes, skins, morphNames, context, result)`; ANIM declares the `skel:` products and writes their intermediates. An ANIM error is reported with its detail and index and fails the import with `IMPORT_ERROR_SOURCE_INVALID` (ANIM's `ANIM_IMPORT_ERROR_IMPORT_CONTEXT` maps to its carried `Zenith_ImportError`).
3. Weights per vertex: up to 8 influences (`JOINTS_0/1`, `WEIGHTS_0/1`); negative weights are `SOURCE_INVALID` (`CONTENT_IMPORT_DETAIL_SKIN_WEIGHT`); weights are normalised to sum 1 in `double` when the sum lies in [0.5, 2] (outside it: `CONTENT_WARNING_SKIN_WEIGHTS_RENORMALISED` and still normalised); a vertex whose weights are all 0 is bound fully to the skin's first joint with `CONTENT_WARNING_SKIN_UNWEIGHTED_VERTEX`; influences are sorted by descending weight, ties by ascending joint index, and zero weights placed last with joint 0. GEOM reduces to four after its own weight rule.
4. A joint index at or above the skin's joint count is `SOURCE_INVALID` (`CONTENT_IMPORT_DETAIL_SKIN_JOINT`); a skin of more than 1,024 joints is `LIMIT_EXCEEDED` (GEOM's mesh joint limit).
5. The mesh's joint remap is, per skin joint j, `m_auNodeToJoint[skin.joints[j]]`, and the skeleton GUID `m_axSkeletons[m_auNodeToSkeleton[skin.joints[0]]]`.

## Morph targets

- Up to 256 targets per mesh (GEOM's section limit); deltas converted (DccMatrix.md "Basis change algorithm"); a target missing an attribute writes zero deltas for it; names from `extras.targetNames`, else `Target<n>`, unique per mesh (`#n` suffixes).
- The mesh's default weights (`mesh.weights`) are written into the generated prefab's mesh-instance entity only when the mesh-instance component carries morph weights (it does not in Stage 4: the defaults are reported in the import member and the skeleton's curve names carry the target names for ANIM, `Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton intermediate"`).
- `import.morphTargets` false drops every target.

## Animations

1. Each glTF animation becomes ANIM `Zenith_ImportedAnimation` with its channels (node, path, interpolation, times, converted values, the first morph index for weights); `Zenith_WriteClipIntermediates(nodes, animations, skeletonResult, context)` declares the `anim:` products and writes their intermediates.
2. A channel targeting a node that is not a joint of any skeleton (an animated prop) is reported `CONTENT_WARNING_NON_JOINT_CHANNEL` and dropped in Stage 4: node animation outside skeletons is CINE's timeline content.
3. Clip settings are ANIM's step's; the `gltf` desc declares them as **source-wide defaults** only, so a preset or the `.meta` `settings` set them for every clip of the source. One clip's own values (its range, loop flag, root motion, additive reference, compression, streaming, events and markers) are ANIM's clip annotation document, attached by the `annotation` member of the clip's sub-asset entry (`Zenith2/Docs/Design/ANIM/Formats.md → "Document type table"`), which overrides these defaults per clip; CONTENT declares no second per-clip mechanism:

<!-- assumption: pending ANIM -->
| Key | Type | Default | Range |
|---|---|---|---|
| `anim.sampleRateHz` | INT | 30 | 1 … 240 |
| `anim.clipRange` | FLOAT_ARRAY (2) | `[]` (the whole clip) | `[start, end]` source seconds, start < end |
| `anim.loop` | BOOL | false | — |
| `anim.rootMotion.joint` | STRING | `""` (ANIM's default joint) | a joint name |
| `anim.compression.errorMetres` | FLOAT | 0.0002 | 1e-5 … 0.01 |
| `anim.compression.curveError` | FLOAT | 0.001 | 1e-5 … 0.05 |

ANIM's design names these keys and defaults in `Zenith2/Docs/Design/ANIM/Formats.md → "Clip resampling"`, "Root motion extraction at cook" and "Error metric"; the complete list, including the root-motion mode, the additive reference and the streaming flag with their value sets, is ANIM's "clip import settings" table, which the `gltf` desc declares verbatim, as source-wide defaults, when ANIM publishes it (rows CT11, CT25).
<!-- /assumption -->

## Materials

Every material product's intermediate is a generated material document written as the `geom.materialDocument` blob (`Zenith2/Docs/Design/GEOM/Materials.md → "Material document intermediate"`): canonical JSON, `schemaVersion` 1, `id` the product GUID, no unknown keys.

| glTF | Generated document |
|---|---|
| any material without `KHR_materials_unlit` | `module` `Zenith2/Shaders/Materials/Standard.slang`, `type` `Material_Standard` |
| `KHR_materials_unlit` (Stage 4) | `module` `Zenith2/Shaders/Materials/Unlit.slang`, `type` `Material_Unlit`; base colour factor and texture only |
| `alphaMode` `OPAQUE` | `domain` `opaque` |
| `alphaMode` `MASK` | `domain` `alphaTest`, `alphaCutoff` = the material's `alphaCutoff` |
| `alphaMode` `BLEND` | `domain` `transparent`, `blend` `alpha` |
| `doubleSided` | `twoSided` |
| `baseColorFactor` | `parameters.m_xBaseColourFactor` (linear RGBA, as glTF stores it) |
| `metallicFactor`, `roughnessFactor` | `parameters.m_fMetallicFactor`, `parameters.m_fRoughnessFactor` |
| `emissiveFactor` × `KHR_materials_emissive_strength` | `parameters.m_xEmissiveFactor` (RGB, linear) |
| `normalTexture.scale`, `occlusionTexture.strength` | `parameters.m_fNormalScale`, `parameters.m_fOcclusionStrength` |
| `baseColorTexture` | `textures.m_xBaseColour` |
| `normalTexture` | `textures.m_xNormal` |
| `metallicRoughnessTexture` and `occlusionTexture` | `textures.m_xORM` ("Textures referenced by materials") |
| `emissiveTexture` | `textures.m_xEmissive` |
| a slot with no texture | the field is absent (its `[Optional]` null substitute) |
| the glTF default material (`material:@default`) | `Material_Standard`, opaque, base colour (1, 1, 1, 1), metallic 1, roughness 1, no texture (glTF 2.0 §3.9.6) |

<!-- assumption: pending GEOM -->
`m_xBaseColour`, `m_xNormal`, `m_xORM`, `m_xBaseColourFactor`, `m_fRoughnessFactor` and `m_fMetallicFactor` are the names `Zenith2/Docs/Rendering.md → "Materials"` shows for `Material_Standard`; `m_xEmissive`, `m_xEmissiveFactor`, `m_fNormalScale` and `m_fOcclusionStrength` are CONTENT's assumption for the emissive and factor fields GEOM lists in words (row CT10). GEOM's step validates every name against the reflection, so a wrong assumption fails the material cook loudly ([Z2-N-REND-248]) rather than rendering wrongly.
<!-- /assumption -->

- Ignored material extensions ("glTF extension support") are reported once per (material, extension).
- Stage 5: `KHR_materials_clearcoat` maps to GEOM's `ClearCoat.slang` type with the clear-coat factor, roughness and textures, and `KHR_materials_sheen` to `Cloth.slang`'s sheen colour and roughness; the parameter names are GEOM's Stage 5 reflection (row CT10), and until GEOM publishes them the two extensions stay ignored with a warning.

<!-- informative -->
Z2-D-079 keeps materials as code plus a parameter document; the importer therefore never creates a shader or a node graph, only a document for an engine material type. A team that wants its own look assigns an authored `.material.json` to a slot through the material remap override (PresetsAndReimport.md "Applying product overrides") and the remap survives every reimport.
<!-- /informative -->

## Textures referenced by materials

### External and embedded images

- An image with a file URI is an asset of its own: the importer calls `ResolveReferencePath(uri)` and binds the returned GUID; the image's own `.meta` and preset declare its usage, and GEOM's material step checks that usage against the field (`COOK_ERROR_MATERIAL_TEXTURE_USAGE`, `Zenith2/Docs/Design/GEOM/Materials.md → "Material cook"`). A URI outside the content roots is `IMPORT_ERROR_INPUT_OUTSIDE_ROOT`; an extension the image importer does not claim is `IMPORT_ERROR_SOURCE_INVALID` (`CONTENT_IMPORT_DETAIL_IMAGE_FORMAT`).
- An embedded image (buffer view or data URI) becomes a texture product per role it is used in: base colour and emissive → `colour` (sRGB), normal → `normal`, the packed ORM → `mask`, a metallic-roughness image used directly → `mask`. The importer decodes it (`Zenith_DecodeImageSource`; KTX2 through KTX-Software's `ktxTexture2_TranscodeBasis` to `KTX_TTF_RGBA32` for `KHR_texture_basisu`), applies `normal.flipGreen` to a normal role (y → 255 − y for 8-bit, 65535 − y for 16-bit), and writes the `geom.textureSource` blob through GEOM's `Zenith_WriteTextureSource` with the role's usage, the colour space (sRGB for `colour` only), the layout of the decoded image and the address hint of the glTF sampler (wrap 0, clamp 1; mirrored written as wrap with a warning).
- `import.textures` false: embedded images are not declared and their fields are left absent.

### Occlusion roughness metallic packing

`Material_Standard` reads one MASK texture `m_xORM` with occlusion in R, roughness in G and metallic in B (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture usage classes"`). glTF stores roughness in G and metallic in B of `metallicRoughnessTexture`, and occlusion in R of a separate `occlusionTexture`, which may be the same image:

| Source | ORM binding |
|---|---|
| no metallic-roughness texture and no occlusion texture | `m_xORM` absent |
| occlusion and metallic-roughness reference the same image with the same `texCoord` and transform | that image used directly (an external asset, or an embedded `texture:<image>@mask`) |
| otherwise | a packed product `texture:<material name>/orm`, 8-bit: R = occlusion's R (255 when absent), G and B = metallic-roughness's G and B (255 when absent), A = 255; the two images are read (`ReadInput` for external files) and decoded; differing extents are resampled to the larger of each axis by bilinear filtering in linear space; a 16-bit source is reduced to 8 bits by rounding |

### Texture transforms

`KHR_texture_transform` (Stage 4) is baked into UVs, because no engine material carries a UV transform: when every texture a primitive's material samples on one UV set has the same transform, that UV set of the primitive's vertices becomes `uv′ = T · uv` with `T = Translation(offset) · Rotation(rotation) · Scale(scale)` (glTF's definition); when the textures of one set disagree, the base colour texture's transform is baked and `CONTENT_WARNING_TEXTURE_TRANSFORM_CONFLICT` names the material. A `texCoord` override inside the extension selects the set as `texCoord` does.

## Cameras

Each node with a camera (Stage 4, `import.cameras`) becomes, in the generated prefab, a `Zenith.Render.Camera` component (`Zenith2/Docs/Design/RPROD/Extractors.md → "Render component registration"`) on the node's entity, or on a generated child entity with the conversion's view correction when Q ≠ I (DccMatrix.md "Basis change algorithm"; a unit-scale-only conversion creates no child, so changing `transform.unitScale` never moves a component to a new entity):

| Property (ID) | Value |
|---|---|
| 1 Active | false: an imported camera is inactive until a person enables it on the instance, so a prop or character whose source carries a camera never competes with the scene's own main camera |
| 2 Priority | 0 |
| 3 Projection | 1 perspective, 2 orthographic |
| 4 FovYDegrees | `yfov` × 180 / π (perspective) |
| 5 OrthoHeightMetres | 2 × `ymag` × s (orthographic) |
| 6 NearMetres | `znear` × s |
| 7 FarMetres | `zfar` × s, 0 when absent (infinite, perspective only; an orthographic camera without `zfar` is `SOURCE_INVALID`, glTF requires it) |
| 8 … 15 | defaults |

`aspectRatio` and `xmag` are not imported: the view's aspect is the viewport's.

## Lights

Each node with a `KHR_lights_punctual` light (Stage 4, `import.lights`) becomes a LIGHT component (`Zenith2/Docs/Design/LIGHT.md → "Flux_LightComponents header"`) on the node's entity, or on the view-correction child as for cameras:

| glTF | Component and properties |
|---|---|
| `directional` | `Zenith.Lighting.DirectionalLight`: Colour = `color`, IlluminanceLux = `intensity` |
| `point` | `Zenith.Lighting.PointLight`: Colour, Intensity = `intensity`, IntensityUnit 0 (candela), RangeMetres = `range` × s, or when `range` is absent the distance at which the illuminance falls to 0.01 lux, `sqrt(intensity / 0.01)`, clamped to [0.1, 1,000] m |
| `spot` | `Zenith.Lighting.SpotLight`: as point, plus InnerAngleDegrees = `innerConeAngle` × 180 / π, OuterAngleDegrees = `outerConeAngle` × 180 / π |
| every light | TemperatureKelvin 0 (colour used), CastsShadows true, the other properties their defaults |

<!-- assumption: pending LIGHT -->
LIGHT's spot angles are measured from the cone axis, as glTF's are (row CT17); if LIGHT defines full cone angles the factor is 2 and the probe's expected values in DccMatrix.md "Convention probe asset" double.
<!-- /assumption -->

## Generated model prefab

For each glTF scene (Stage 4, `import.prefab`), the importer declares `prefab:<scene name>` and writes its intermediate `scene.prefabDocument`: the UTF-8 bytes of a prefab document of `Zenith2/Docs/Design/SCENE/Format.md → "Prefab document schema"`, written with SCENE's prefab canonical schema (`Zenith_GetPrefabCanonicalSchema`), so the document is canonical by construction:

| Member | Value |
|---|---|
| `$schema`, `schemaVersion` | `zenith2.prefab`, 1 |
| `id` | the product's GUID |
| `name` | the scene name |
| `root` | the root entity's ID |
| `variantOf` | null |
| `entities` | one entity per node of the scene (after `mesh.excludeNodes`), plus the generated root when the scene has several root nodes, plus one view-correction child per camera or directional or spot light node when Q ≠ I, plus one entity per `EXT_mesh_gpu_instancing` instance |
| `instances` | `{}` |

Each entity:

| Member | Value |
|---|---|
| ID | `Zenith_DeriveAuthoringID(prefab GUID, L)` (`Zenith2/Docs/Design/SCENE/Prefabs.md → "Derivation"`), where L is the local ID of the node key ("Node keys"): bytes 0 … 15 of `Zenith_Sha256Digest("ZCNK" ‖ UTF-8 node key)` read as `ulHigh` (bytes 0 … 7, little-endian) and `ulLow` (bytes 8 … 15), with SCENE's version-4 bit rule applied (step 3 of "Derivation"); the generated root's key is `/`, a view-correction child's `<node key>/@view`, an instance's `<node key>/@instance<n>` ("Node keys" rule 4) |
| `name` | the node's name (empty names stay empty) |
| `parent` | the parent node's entity ID; the root's parent is null |
| `sortKey` | `Zenith_MakeEvenSortKeys(siblingCount)` in child order (`Zenith2/Docs/Design/SCENE/Format.md → "Sort key insertion algorithm"`) |
| `components.Zenith.LocalTransform` | version 1, fields 1 Position, 2 Rotation, 3 Scale from the converted local TRS |
| `components.Zenith.Render.MeshInstance` | for a mesh node: version 1, field 1 Mesh = the mesh product's GUID (the mirrored variant for a negative-determinant node); other fields absent (defaults) |
| camera and light components | "Cameras", "Lights" |

Because the entity IDs depend only on the prefab's GUID and the node key, an override written against an entity of the model prefab (in a scene instance or a variant) still targets the same entity after any reimport that keeps the node's key: for a node with `zenithId`, after any rename, reparent or reorder of the node or its ancestors; for a node without one, after any edit that keeps its segment path from its nearest `zenithId` ancestor (or the scene root) and its rank among equal-named siblings (`Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Reimport guarantees"`). An edit that changes a key gives the node a new ID; overrides of the old ID become SCENE's dangling overrides, kept and reported, never re-targeted.

<!-- assumption: pending SCENE -->
SCENE's `scene.prefab` step cooks a product whose source is not a `.prefab.json` from its `scene.prefabDocument` intermediate (read with `ReadIntermediate`), and the cook's and the editor's `Zenith_PrefabProvider` resolve such a prefab ID through the same blob, so instances and variants of a model prefab resolve and cook (row CT13). Until it lands, `import.prefab` defaults to false in the Stage 4 preset set.
<!-- /assumption -->

## Physics collision products

When the source's `.meta` carries a `physics` member whose `collision` is not `None` (`Zenith2/Docs/Design/PHYS/Formats.md → "Collision importer settings"`), the `gltf` importer declares `collision:shape` (kind `CollisionShape`) and writes its `physics.collisionSource` intermediate with PHYS's writer: the mesh product GUID and the converted world matrix of every node selected by `nodes`, the `collision` mode, the `material` reference, and `maxHullVertices`; PHYS's `physics.collision` step reads the meshes' cooked positions through `ReadDependencyOutput`.

The `material` member is read as a physical-material asset reference by GUID (the 36-character text the editor's asset picker writes, as every authoring reference is, [Z2-N-CONTRACT-546]), passed to the writer unchanged; a value that is not a canonical GUID text fails `IMPORT_ERROR_SETTING_INVALID` naming `physics.material`. A path is not resolved: `ResolveReferencePath` takes source-relative paths without `..`, and PHYS's example writes a game-relative one (`"Assets/Physics/Stone.physmat"`), which no source-relative form can express for a source outside `Assets/Physics/`.

<!-- assumption: pending PHYS, ASSET -->
PHYS names the sub-asset `collision`, which ASSET's key grammar (`<kind prefix>:<name>`) does not admit; CONTENT uses `collision:shape`. The blob writer `Zenith_WritePhysicsCollisionSource` and its layout are PHYS's (row CT12); PHYS's `material` member holds a GUID rather than a path (row CT26); the `physics` member is a top-level `.meta` member ASSET's schema must admit (row CT1 (a)).
<!-- /assumption -->

## Image importer

| Desc member | Value |
|---|---|
| `m_szID` | `"image"` |
| extensions | `.png`, `.jpg`, `.jpeg`, `.tga`, `.bmp` |
| `m_uVersion` | 1 + `uCONTENT_IMAGE_IMPORT_VERSION` |
| produced kinds | `Texture` |

| Key | Type | Default | Values | P | Read by |
|---|---|---|---|---|---|
| `texture.usage` | ENUM | `unset` | `unset`, `colour`, `linear`, `normal`, `mask` | — | the importer (header usage) |
| `texture.addressHint` | ENUM | `wrap` | `wrap`, `clamp` | — | the importer (header hint) |
| `texture.quality`, `texture.uastcRdoLambda`, `texture.alphaCoverageCutoff`, `texture.virtual` | as the `gltf` table | | | `texture.quality`, `texture.uastcRdoLambda` | GEOM's texture step |
| `normal.flipGreen` | BOOL | false | — | — | the importer |
| `image.maxExtent` | INT | 0 (none) | 0 … 16,384 | — | the importer: a source larger on either axis is downscaled by a box filter in linear space to fit, keeping the aspect ratio ("Colour downscale") |

`Import`:

1. Fault hook; read the settings. `texture.usage = unset` fails `IMPORT_ERROR_SETTING_INVALID`, detail `CONTENT_IMPORT_DETAIL_USAGE_UNSET`, with the message "<path>: texture.usage is unset; assign an import preset (the editor's preset picker, or `ZenithContent batch run --game <Name> --spec <file>` with a `setPreset` specification) or set texture.usage in the .meta" (no usage is inferred from the file name or the channels). Every image no preset's `match` block selects reaches this failure on its first import: no shipped preset matches every image (`Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Shipped presets"`).
2. Detect and decode (`Zenith_DetectImageSourceFormat`, `Zenith_DecodeImageSource`).
3. Apply `normal.flipGreen` (usage `normal` only), `image.maxExtent`.
4. Declare the one primary product `texture:main` (kind `Texture`); write `geom.textureSource` with the resolved `texture.usage` as its non-zero header usage (never 0, the "preset's usage" form, because `unset` already failed at step 1), `R8G8B8A8` or `R16G16B16A16`, colour space sRGB for `colour` and linear otherwise, and the resolved address hint (0 wrap, 1 clamp; never 0xFF); GEOM's writer table names the zero form for this importer, and row CT21 asks GEOM to record the non-zero form.
5. Report the decoded extent and layout in the `contentImports` member.

<!-- informative -->
The reference engine inferred a texture's colour space and compression from its file name and silently fell back when a set was missing from a hand-kept list, which cost every terrain shadow once a normal map decoded as colour. Here the usage is declared, by a preset chosen when the `.meta` is created and committed with it (PresetsAndReimport.md "Preset matching"), and an undeclared usage fails the import loudly.
<!-- /informative -->

### Colour downscale

`image.maxExtent` downscales by an integer box filter: the factor f is the smallest integer with ⌈W / f⌉ and ⌈H / f⌉ both at most the setting, each output texel is the mean of its f × f source block (blocks at the right and bottom edges average the texels they hold), and alpha is averaged unweighted. For `texture.usage = colour` the R, G and B channels are averaged in linear light and re-encoded:

1. Decode: an 8-bit value v maps through a 256-entry table T, T[v] = c / 12.92 when c = v / 255 ≤ 0.04045, else `Zenith_Pow((c + 0.055) / 1.055, 2.4)`, evaluated in binary64 with CORE's deterministic `Zenith_Pow` and kept in binary64 (the table is built once per import); a 16-bit value uses the same formula with c = v / 65,535, evaluated per texel.
2. Average the f² decoded values in binary64 in raster order of the block.
3. Encode: x ≤ 0.0031308 gives 12.92 × x, else 1.055 × `Zenith_Pow(x, 1 / 2.4)` − 0.055; the result × 255 (or 65,535) is rounded to nearest, ties to even, and clamped.

Every other usage averages the stored values in binary64 and rounds and clamps as step 3 does. No CRT transcendental is called, so the downscaled texture source, which one host may put into the shared store and another read, has identical bytes on every machine (`Zenith2/Docs/Design/CONTENT.md → "Common format rules"`); `Test_ImageImporter_UsagesAndLayouts` asserts a 4 × 4 colour image downscaled to 2 × 2 equals the values computed by hand from this rule.

## Audio importer

| Desc member | Value |
|---|---|
| `m_szID` | `"audio"` |
| extensions | `.wav`, `.flac`, `.ogg`, `.mp3`, `.aif`, `.aiff` |
| `m_uVersion` | 1 + `uCONTENT_AUDIO_IMPORT_VERSION` |
| produced kinds | `Sound` |
| settings | exactly the eleven keys of `Zenith2/Docs/Design/AUDIO/Formats.md → "Sound import settings"` with their types, defaults and ranges (`audio.loadMode`, `audio.codec`, `audio.sampleRate`, `audio.channels`, `audio.normalise`, `audio.targetLufs`, `audio.bitrateKbps`, `audio.loopStart`, `audio.loopEnd`, `audio.trimSilence`, `audio.streamThresholdSeconds`; row AU10) |

`Import`:

1. Fault hook; decode with `Zenith_DecodeAudioSource` (default `Zenith_AudioDecodeOptions`; format by extension; a WAV, AIFF, FLAC or Ogg file whose magic disagrees with its extension is `SOURCE_INVALID`, detail `CONTENT_IMPORT_DETAIL_AUDIO_FORMAT`).
2. Loop points: `audio.loopStart`/`audio.loopEnd` of −1 take the source's loop (the table of `Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Source format tables"`); explicit values must satisfy 0 ≤ start < end ≤ frame count (`IMPORT_ERROR_SETTING_INVALID`); an inclusive source loop end (WAV `smpl` stores the last frame) is converted to exclusive.
3. Markers: at most 64, ascending, names truncated to 47 bytes at a code-point boundary; further ones reported `CONTENT_WARNING_AUDIO_MARKERS_DROPPED`.
4. Samples converted to float in [−1, 1] by the libraries' float readers ("Zenith_MediaDecoders header"): 8-bit WAV PCM, which is unsigned, as (x − 128) / 128; signed integer PCM of 16, 24 and 32 bits as x / 2^(bits − 1); float sources passed through; values outside [−1, 1] are left for the step, which clamps and counts.
5. Declare `sound:main` (primary, kind `Sound`); write `decoded-audio` with `Zenith_WriteDecodedAudioBlob` in AUDIO's layout (magic `ZAPC`, version 1, the rate, channels, 32, frames, loop start and end, markers, channel mask, interleaved samples).
6. Report rate, channels, frames and the loop in the member.

## FBX importer

`Zenith_FbxImporter` (the noun of the DecisionLog's D-SEL-FBX entry; `m_szID` `"fbx"`, extension `.fbx`, `m_uVersion` 1 + `uCONTENT_FBX_IMPORT_VERSION`, settings the `gltf` table, Stage 4) has exactly two forms, chosen by the D-SEL-FBX (open, Stage 4) ruling in the implementing ticket's diff:

| Ruling | Behaviour |
|---|---|
| a conversion path adopted | `Import`: fault hook; write the source bytes to `${ZENITH_OUT}/Temp/fbx/<32 hex of the import key>/in.fbx`; run the adopted converter as a child process (argument vector, no shell; working directory that directory; timeout `cook.content.fbxConverterTimeoutSeconds`, default 300; the process terminated on timeout, `IMPORT_ERROR_WORKER_TIMEOUT`); read `out.glb`; delete the directory; then run the `gltf` import algorithm over `out.glb` with the source's settings, so products, keys, overrides and reports are the glTF ones, with a read callback that refuses every external URI (the converter must embed every buffer and image; an external URI fails `IMPORT_ERROR_SOURCE_INVALID`, detail `CONTENT_IMPORT_DETAIL_FBX_CONVERTER`, message "converter output references an external file <uri>", because `ReadInput` resolves against the `.fbx`'s directory, not the converter's); the converter's pinned version joins `m_aszToolVersions`; a converter exit code other than 0, or a missing output, is `IMPORT_ERROR_SOURCE_INVALID`, detail `CONTENT_IMPORT_DETAIL_FBX_CONVERTER`, with the converter's last 4 KiB of standard error in the report |
| glTF-only | `Import` fails every `.fbx` with `IMPORT_ERROR_SOURCE_INVALID`, detail `CONTENT_IMPORT_DETAIL_FBX_UNSUPPORTED`, message "FBX is not supported (D-SEL-FBX ruled glTF-only); export glTF 2.0 through a route of the DCC matrix" — the importer still claims `.fbx`, so an FBX file is never silently ignored |

The converter runs inside the cook worker process that runs the import, so a converter crash or hang is contained twice (its own process, then the worker).

## Import report member

Each CONTENT importer adds one element to the cook report's `contentImports` member (pending ASSET, row CT2); the member is an array sorted by source GUID:

```json
{
	"source": "0b6e2d4c-8f1a-4c2e-9d3b-5a7e1f0c2b44",
	"path": "Games/OpenWorldSlice/Assets/Characters/Hero.glb",
	"importer": "gltf",
	"generator": "Khronos glTF Blender I/O v4.2.70",
	"products": [ { "key": "mesh:Body", "guid": "9a02…", "kind": "Mesh", "boundsMetres": [[-0.4, 0.0, -0.2], [0.4, 1.78, 0.25]] } ],
	"scene": { "name": "Scene", "boundsMetres": [[-0.4, 0.0, -0.2], [0.4, 1.78, 0.25]], "heightMetres": 1.78, "frontMarkerYawDegrees": null },
	"counts": { "nodes": 74, "meshes": 1, "triangles": 41210, "materials": 3, "textures": 5, "skins": 1, "joints": 60, "morphTargets": 12, "animations": 0, "cameras": 0, "lights": 0 },
	"renames": [ { "kind": "Mesh", "from": "mesh:Body_old", "to": "mesh:Body", "guid": "9a02…" } ],
	"excluded": [ "mesh:Proxy" ],
	"warnings": [ { "detail": "CONTENT_WARNING_TANGENTS_DISCARDED", "index": 0, "count": 1 } ],
	"warningsDropped": 0
}
```

`renames` lists every `RenameProduct` call of the import, so the person can undo an unwanted one with a sub-asset remap batch; `excluded` lists the keys skipped because their entry carries `exclude: true`.

For `image`: `products`, `"image": { "width", "height", "layout", "usage" }`; for `audio`: `"audio": { "sampleRate", "channels", "frames", "loopStart", "loopEnd", "markers" }`. No time, host or user appears.

## Crash isolation

- Every CONTENT importer's desc sets `m_bOutOfProcess` true, because each runs a third-party parser over untrusted bytes; ASSET's pool therefore runs every import in a `ZenithCook --worker` process (`Zenith2/Docs/Design/ASSET/Cook.md → "Worker processes"`), and a crash, hang or abort fails that item alone (`IMPORT_ERROR_WORKER_CRASHED`, `_WORKER_TIMEOUT`) while the cook continues and publishes every independent item. `--in-process` exists for debugging and for tests that are not about isolation.
- An import writes intermediates only through `WriteIntermediate`, which ASSET stores as complete DDC entries after the import returns, so a killed worker leaves no partial intermediate.
- The fault-source cook key `cook.content.faultSource` (a CONTENT importer-owned cook key, `Zenith2/Docs/Design/CONTENT.md → "Configuration and cook keys"`): `crash:<Zenith2-relative source path>` makes every CONTENT importer whose `GetSourcePath()` equals the path call `ZENITH_FATAL("content fault injection: <path>")` at step 1, terminating its process abnormally; `hang:<path>` makes it loop `Zenith_SleepUs(100000)` (CORE's blocking wait) until terminated when it runs in a worker process (`GetJobContext()` is null there); in process, where the import may run on a job worker that must never block, `hang:` behaves as `crash:`. The key is read from the services' command line in the worker (pending ASSET, row CT5); its value is not a key input, because a faulted import produces no output.
- `Test_ContentCrash_WorkerCrashContained`, `Test_ContentCrash_WorkerHangTimesOut` and `Test_ContentCrash_NoPartialIntermediate` (Tests.md "Importer tests") prove the containment with the real importers; ASSET's `Test_Import_WorkerCrashContained` proves the pool with its fake importer.

## Parser limits

Constants of `Zenith_ContentTypes.h`; a violation is `CONTENT_ERROR_LIMIT_EXCEEDED` (`IMPORT_ERROR_LIMIT_EXCEEDED` at the importer), found by the check functions of "Version and limit constants" before any allocation sized by the value; each is tested at the limit and at limit + 1 (limit − 1 for the minimum) through those functions ([Z2-N-ARCH-953]); the rows join `Zenith2/Docs/Architecture.md → "Security architecture"` table "Parser limits" through the cross-doc row of CT15:

<!-- elaboration -->
| Parser | Limit | Value | Constant |
|---|---|---|---|
| glTF | file (`.gltf`, `.glb`) and each external buffer, bytes | 2,147,483,648 (2 GiB) | `ulCONTENT_GLTF_MAX_FILE_BYTES` |
| glTF | JSON chunk, bytes | 67,108,864 (64 MiB) | `ulCONTENT_GLTF_MAX_JSON_BYTES` |
| glTF | JSON structure within the chunk | depth 64, arrays of 1,048,576 elements, objects of 65,536 keys | none (CORE's `Zenith_JsonLimits`) |
| glTF | decoded `data:` URI, bytes each | 268,435,456 (256 MiB) | `ulCONTENT_GLTF_MAX_DATA_URI_BYTES` |
| glTF | URI length, bytes | 4,096 | `uCONTENT_GLTF_MAX_URI_BYTES` |
| glTF | nodes | 65,536 | `uCONTENT_GLTF_MAX_NODES` |
| glTF | node depth | 64 | `uCONTENT_GLTF_MAX_NODE_DEPTH` |
| glTF | meshes | 16,384 | `uCONTENT_GLTF_MAX_MESHES` |
| glTF | primitives per mesh | 255 | `uCONTENT_GLTF_MAX_PRIMITIVES_PER_MESH` |
| glTF | vertices per primitive | 16,777,216 | `uCONTENT_GLTF_MAX_VERTICES_PER_PRIMITIVE` |
| glTF | vertices per combined mesh | 16,777,216 | `uCONTENT_GLTF_MAX_VERTICES_PER_COMBINED_MESH` |
| glTF | accessors | 1,048,576 | `uCONTENT_GLTF_MAX_ACCESSORS` |
| glTF | buffer views | 1,048,576 | `uCONTENT_GLTF_MAX_BUFFER_VIEWS` |
| glTF | buffers | 4,096 | `uCONTENT_GLTF_MAX_BUFFERS` |
| glTF | materials | 4,096 | `uCONTENT_GLTF_MAX_MATERIALS` |
| glTF | textures | 4,096 | `uCONTENT_GLTF_MAX_TEXTURES` |
| glTF | images | 4,096 | `uCONTENT_GLTF_MAX_IMAGES` |
| glTF | samplers | 4,096 | `uCONTENT_GLTF_MAX_SAMPLERS` |
| glTF | skins | 1,024 | `uCONTENT_GLTF_MAX_SKINS` |
| glTF | joints per skin | 1,024 | `uCONTENT_GLTF_MAX_JOINTS_PER_SKIN` |
| glTF | morph targets per mesh | 256 | `uCONTENT_GLTF_MAX_MORPH_TARGETS_PER_MESH` |
| glTF | animations | 4,096 | `uCONTENT_GLTF_MAX_ANIMATIONS` |
| glTF | channels per animation | 65,536 | `uCONTENT_GLTF_MAX_CHANNELS_PER_ANIMATION` |
| glTF | keys per channel | 16,777,216 | `uCONTENT_GLTF_MAX_KEYS_PER_CHANNEL` |
| glTF | cameras | 4,096 | `uCONTENT_GLTF_MAX_CAMERAS` |
| glTF | lights | 4,096 | `uCONTENT_GLTF_MAX_LIGHTS` |
| glTF | instances per node (`EXT_mesh_gpu_instancing`) | 65,536 | `uCONTENT_GLTF_MAX_INSTANCES_PER_NODE` |
| glTF | scenes | 256 | `uCONTENT_GLTF_MAX_SCENES` |
| images | width and height, each (`STBI_MAX_DIMENSIONS`) | 16,384 | `uCONTENT_IMAGE_MAX_EXTENT` |
| images | decoded bytes (width × height × 4 or 8) | 2,147,483,648 (2 GiB) | `ulCONTENT_IMAGE_MAX_DECODED_BYTES` |
| images | file, bytes | 1,073,741,824 (1 GiB) | `ulCONTENT_IMAGE_MAX_FILE_BYTES` |
| audio | channels (and at least 1) | 8 | `uCONTENT_AUDIO_MAX_CHANNELS` |
| audio | sample rate minimum, Hz | 8,000 | `uCONTENT_AUDIO_MIN_SAMPLE_RATE` |
| audio | sample rate maximum, Hz | 192,000 | `uCONTENT_AUDIO_MAX_SAMPLE_RATE` |
| audio | decoded bytes (frames × channels × 4) | 2,147,483,648 (2 GiB) | `ulCONTENT_AUDIO_MAX_DECODED_BYTES` |
| audio | file, bytes | 2,147,483,648 (2 GiB) | `ulCONTENT_AUDIO_MAX_FILE_BYTES` |
| audio | markers kept (further ones dropped with a warning) | 64 | `uCONTENT_AUDIO_MAX_MARKERS` |
| WAV and AIFF | chunks per file (every chunk length also checked against the file before reading) | 4,096 | `uCONTENT_AUDIO_MAX_RIFF_CHUNKS` |
| FLAC | metadata blocks | 1,024 | `uCONTENT_AUDIO_MAX_FLAC_METADATA_BLOCKS` |
| Ogg Vorbis | decoder working memory, bytes | 67,108,864 (64 MiB) | `ulCONTENT_AUDIO_MAX_VORBIS_WORK_BYTES` |
| Ogg | pages | bounded by the file size; every page CRC checked by stb_vorbis | none (`ulCONTENT_AUDIO_MAX_FILE_BYTES` bounds it) |

`Test_ContentLimits_TableAgreesWithConstants` reads this table as Markdown with one rule per cell: a Constant cell that begins with `none` names no CONTENT constant and is skipped (it states its bound in the Limit and Value cells); every other Constant cell is exactly one backticked constant, and its Value cell begins with that constant's value written in decimal with `,` thousands separators (any parenthesised text after the number is ignored). The test asserts that the set of named constants equals the 39 rows of `Zenith_GetContentLimits()` and that each value equals the row's `m_ulValue`.

### Limit reachability

Every row is tested through its check function at the limit and at limit + 1 (`uCONTENT_AUDIO_MIN_SAMPLE_RATE` at the limit and limit − 1; `uCONTENT_AUDIO_MAX_CHANNELS` also at 0), with every other member at its default, asserting success, then `LIMIT_EXCEEDED` with the row's index in `m_uIndex`. Those cases are the [Z2-N-ARCH-953] evidence. A file-level case is added where a file of at most 70 MiB reaches the limit before any other limit, and only there:

| Limits | File-level case | Why no file-level case otherwise |
|---|---|---|
| URI length, nodes, node depth, meshes, primitives per mesh, buffers, materials, textures, images, samplers, skins, joints per skin, morph targets per mesh, animations, channels per animation, cameras, lights, instances per node, scenes; the JSON chunk | builder-written `.gltf` at the limit and one above (the JSON chunk as 64 MiB and 64 MiB + 1 byte of whitespace padding) | — |
| accessors, buffer views | none | each equals the JSON array limit (1,048,576 elements), so one above fails first by the JSON limit (`CONTENT_IMPORT_DETAIL_JSON`); the JSON limit's own file case is CORE's |
| vertices per primitive, keys per channel | a builder-written `.gltf` of a few KiB declaring a POSITION accessor (respectively an animation sampler's input accessor) with `count` 16,777,217 over a 12-byte buffer fails `LIMIT_EXCEEDED` at step 4 of "Zenith_GltfSource header", before any buffer read or bounds check; the at-limit value is the check-function case alone, because a file declaring 16,777,216 passes step 4 and then fails step 6's bounds check | — |
| vertices per combined mesh | none | the declared counts are checked first ("Zenith_GltfSource header" step 4), so a small file reaches the limit (65,536 nodes each instancing one 257-vertex mesh sum to 16,842,752); the value is covered through `Zenith_CheckGltfCounts` and the computation of the sum by `Test_GltfLimits_CountsFromSources`, and a file-level case would add only the summation those cases already assert |
| file bytes, decoded `data:` URI bytes | none | an input above the limit is 256 MiB to 2 GiB; the parser's computation of each counted value from the source (the base64 length rule, an external buffer's size) is asserted by `Test_GltfLimits_CountsFromSources` over small sources |
| image extent | the 16,384 × 1 and 16,385 × 1 PNGs, and a header claiming 16,384² over a 1 KiB file, which fails before allocating | — |
| image decoded bytes | none | it equals 16,384² × 8, so it cannot be exceeded once both extents pass (dominated by the extent row) |
| audio decoded bytes | a builder-written FLAC of about 1 KiB whose STREAMINFO claims 2²⁹ + 1 frames of one channel ((2²⁹ + 1) × 1 × 4 bytes > 2 GiB) fails `LIMIT_EXCEEDED` after the header, before any sample buffer is allocated; the at-limit value (2²⁹ frames, exactly 2 GiB) is the check-function case alone, because a file claiming it would size a 2 GiB buffer | — |
| image file bytes, audio file bytes | none | an input above the limit is 1 GiB or more |
| audio channels, both sample rates, RIFF chunks, FLAC metadata blocks | builder-written WAV and FLAC headers with a few frames at and beyond each bound | — |
| markers kept | a WAV with 64 and with 65 `cue ` points: 64 kept, and 64 kept with one `CONTENT_WARNING_AUDIO_MARKERS_DROPPED` | — |
| Vorbis working memory | the recorded Ogg through `Zenith_AudioDecodeOptions` (`Test_MediaDecoders_VorbisWorkBlockGrows`) | — |

## Import detail codes

```cpp
// Zenith2/Tools/ContentCore/Zenith_ContentTypes.h
/// @contract enum Zenith_ContentImportDetail
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; the detail of IMPORT_ERROR_SOURCE_INVALID, _LIMIT_EXCEEDED and _SETTING_INVALID raised by CONTENT importers, and of
///   CONTENT_ERROR_SOURCE_INVALID, _LIMIT_EXCEEDED, _UNSUPPORTED; the values 0x8000 and above are warnings, reported, never failing.
/// Version: since Z2 Stage 3. Tool-side; values are appended, never renumbered.
/// @endcontract
enum Zenith_ContentImportDetail : uint16
{
	CONTENT_IMPORT_DETAIL_GLTF_VERSION = 1,
	CONTENT_IMPORT_DETAIL_GLB_CHUNK,
	CONTENT_IMPORT_DETAIL_JSON,
	CONTENT_IMPORT_DETAIL_REQUIRED_EXTENSION,
	CONTENT_IMPORT_DETAIL_ACCESSOR_BOUNDS,
	CONTENT_IMPORT_DETAIL_ACCESSOR_TYPE,
	CONTENT_IMPORT_DETAIL_BUFFER_URI,
	CONTENT_IMPORT_DETAIL_MESHOPT_DECODE,
	CONTENT_IMPORT_DETAIL_NON_FINITE,
	CONTENT_IMPORT_DETAIL_INDEX_RANGE,
	CONTENT_IMPORT_DETAIL_NODE_TRANSFORM,
	CONTENT_IMPORT_DETAIL_NODE_HIERARCHY,
	CONTENT_IMPORT_DETAIL_SKIN_WEIGHT,
	CONTENT_IMPORT_DETAIL_SKIN_JOINT,
	CONTENT_IMPORT_DETAIL_ANIMATION_TIMES,
	CONTENT_IMPORT_DETAIL_CAMERA,
	CONTENT_IMPORT_DETAIL_LIGHT,
	CONTENT_IMPORT_DETAIL_IMAGE_FORMAT,
	CONTENT_IMPORT_DETAIL_IMAGE_DECODE,
	CONTENT_IMPORT_DETAIL_AUDIO_FORMAT,
	CONTENT_IMPORT_DETAIL_AUDIO_DECODE,
	CONTENT_IMPORT_DETAIL_AUDIO_CHUNK,
	CONTENT_IMPORT_DETAIL_AXES_NOT_PERPENDICULAR,
	CONTENT_IMPORT_DETAIL_UNIT_SCALE_RANGE,
	CONTENT_IMPORT_DETAIL_USAGE_UNSET,
	CONTENT_IMPORT_DETAIL_EXPECTATION_FAILED,
	CONTENT_IMPORT_DETAIL_FBX_CONVERTER,
	CONTENT_IMPORT_DETAIL_FBX_UNSUPPORTED,
	CONTENT_IMPORT_DETAIL_OVERRIDES,
	CONTENT_IMPORT_DETAIL_LIMIT,
	CONTENT_IMPORT_DETAIL_DUPLICATE_ZENITH_ID,         // two objects of one kind, or two nodes, share an extras.zenithId

	CONTENT_WARNING_EXTENSION_IGNORED = 0x8000,
	CONTENT_WARNING_PRIMITIVE_MODE_SKIPPED,
	CONTENT_WARNING_UNNAMED_OBJECT,
	CONTENT_WARNING_FLAT_NORMALS_GENERATED,
	CONTENT_WARNING_NORMALS_REPAIRED,
	CONTENT_WARNING_TANGENTS_DISCARDED,
	CONTENT_WARNING_DEGENERATE_UV,
	CONTENT_WARNING_MIRRORED_JOINT,
	CONTENT_WARNING_SKIN_WEIGHTS_RENORMALISED,
	CONTENT_WARNING_SKIN_UNWEIGHTED_VERTEX,
	CONTENT_WARNING_NON_JOINT_CHANNEL,
	CONTENT_WARNING_MIXED_TEXCOORDS,
	CONTENT_WARNING_TEXTURE_TRANSFORM_CONFLICT,
	CONTENT_WARNING_SAMPLER_MIRRORED,
	CONTENT_WARNING_BASISU_REENCODED,
	CONTENT_WARNING_EXIF_ORIENTATION_IGNORED,
	CONTENT_WARNING_AUDIO_MARKERS_DROPPED,
	CONTENT_WARNING_EXPECTATION_FAILED,
	CONTENT_WARNING_RENAME_AMBIGUOUS,
	CONTENT_WARNING_OVERRIDE_DANGLING,
	CONTENT_WARNING_DUPLICATE_SIBLING_NAME,            // equal-named siblings keyed by child order ("Node keys")
	CONTENT_WARNING_RENAME_ASSUMED,                    // a recognised rename, naming both keys (PresetsAndReimport.md "Rename recognition")
	CONTENT_WARNING_RENAME_NO_EVIDENCE,                // one-to-one but the fingerprints differ: orphaned and re-keyed
};
constexpr const char* Zenith_ToString(Zenith_ContentImportDetail eDetail);
```

Each report line is `<source path>: <Zenith_ToString(detail)>: <message>` with the element index (node, mesh, accessor, chunk) where one applies.
