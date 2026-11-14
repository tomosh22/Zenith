# CONTENT design: the supported-DCC matrix and import conventions

This file is part of the CONTENT design family; the entry document is `Zenith2/Docs/Design/CONTENT.md`. It specifies which digital content creation (DCC) tools reach the pipeline and by which route, which parts of glTF 2.0 and its extensions are supported, the per-feature support matrix, the image and audio source formats, the coordinate, handedness and unit conventions every import enforces with their algorithms, the convention probe asset and the per-DCC fixtures that test them, the fidelity thresholds, and the FBX route governed by D-SEL-FBX (open, Stage 4).

## Scope of this file

| Topic | Heading |
|---|---|
| which tools, by which route | "DCC routes" |
| glTF 2.0 core and extensions | "glTF core support", "glTF extension support" |
| skins, morphs, animation, materials, cameras, lights per route | "Feature support matrix" |
| image and audio formats | "Source format tables" |
| conventions and their algorithms | "Conversion settings", "Basis change algorithm", "Unit conversion", "Winding and the negative-determinant rule", "Normals and tangents on import", "Bounds and facing report" |
| how conventions are tested per DCC | "Convention probe asset", "Per-DCC fixtures" |
| what "the same asset" means numerically | "Fidelity thresholds" |
| FBX | "FBX route" |

## DCC routes

The engine's convention is CORE's (`Zenith2/Docs/Design/CORE/Maths.md → "Coordinate systems"`): right-handed, +Y up, model front +Z, metres, counter-clockwise front faces ([Z2-N-REND-885]). glTF 2.0 defines the same convention, so a DCC route ends in glTF 2.0 and needs no conversion when the exporter is configured as its row states.

<!-- elaboration -->
| DCC | Route into the pipeline | Exporter | Export options the route requires | Matrix status |
|---|---|---|---|---|
| Blender 4.2 LTS | glTF 2.0 (`.glb` or `.gltf` with embedded or external buffers) | the bundled glTF 2.0 add-on | +Y up conversion on; apply modifiers; scene unit scale 1.0 (metric); "Punctual Lights" on; skinning, shape keys and animation on; image format automatic | supported |
| Autodesk 3ds Max 2025 | glTF 2.0 | the built-in glTF exporter | system unit 1 unit = 1 metre; animation baked at 30 Hz; physical materials | supported |
| SideFX Houdini 20.5 | glTF 2.0 | the glTF ROP (KineFX for skinned characters) | units metres; Y up | supported |
| Autodesk Maya 2025 | FBX through the D-SEL-FBX path when that ruling adopts one; otherwise the two-hop route FBX → Blender 4.2 import → glTF 2.0 export with Blender's row | the FBX exporter; then the adopted converter, or Blender | Y up; centimetres are accepted only through the converter or Blender, which convert to metres | supported through a route; the direct-FBX cells are D-SEL-FBX (open, Stage 4) |
| Adobe Substance 3D Painter 10 | images (PNG 8- or 16-bit, JPEG) for materials authored in Zenith2 or carried by a glTF | the texture export | normal maps OpenGL convention, or DirectX with `normal.flipGreen` set ("Conversion settings"); ORM packed occlusion R, roughness G, metallic B | supported for images |
| gltfpack (meshoptimizer's command-line tool, the manifest's pinned version) | glTF 2.0 output of any route, recompressed | `gltfpack -i in.glb -o out.glb` with `-cc` for meshopt compression | none; its output uses `EXT_meshopt_compression`, `KHR_mesh_quantization` and `KHR_texture_transform` | supported |
| audio editors (any tool writing the formats of "Source format tables") | WAV, FLAC, Ogg Vorbis, MP3, AIFF | the tool's export | loop points in a `smpl` chunk, Vorbis comments or AIFF markers ("Source format tables") | supported |

- A route not in this table is unsupported: its files import only if they meet glTF 2.0 or the format tables, and nothing guarantees their conventions.
- The exact DCC and exporter versions each route was verified with are recorded in `dcc_matrix.json` by the recording ticket of the route ("Per-DCC fixtures"); a version change is re-verified by a new recording.

## glTF core support

| glTF 2.0 core element | Behaviour |
|---|---|
| container | `.gltf` (UTF-8 JSON) and `.glb` (binary container: header magic `glTF`, version 2, a JSON chunk and at most one BIN chunk); any other version is `CONTENT_IMPORT_DETAIL_GLTF_VERSION` |
| `asset.version` | `"2.0"` required; `asset.minVersion` above `"2.0"` is refused |
| buffers | the GLB BIN chunk; `data:` URIs (base64, `application/octet-stream` or `application/gltf-buffer`); relative file URIs read through `Zenith_ImportContext::ReadInput` (recorded as import inputs; a `.bin` buffer file so recorded is not an asset source, and ASSET's scan classifies a file recorded as another source's import input without `COOK_WARNING_UNRECOGNISED_FILE`, pending ASSET, row CT24); absolute paths, `..` beyond the content root and remote URIs (`http:`, `https:`) are refused (`IMPORT_ERROR_INPUT_OUTSIDE_ROOT`) |
| buffer views and accessors | every component type (`BYTE`, `UNSIGNED_BYTE`, `SHORT`, `UNSIGNED_SHORT`, `UNSIGNED_INT`, `FLOAT`), every type (`SCALAR` … `MAT4`), `normalized`, byte strides, sparse accessors (indices and values substituted over the base or zeros), `min`/`max` ignored (recomputed) |
| scenes and nodes | every scene; `scene` (the default, else scene 0); node hierarchies with `matrix` or TRS (both on one node is `CONTENT_IMPORT_DETAIL_NODE_TRANSFORM`); a node with several parents or a cycle is refused |
| meshes and primitives | modes `TRIANGLES`, `TRIANGLE_STRIP`, `TRIANGLE_FAN` (converted to triangle lists); `POINTS` and `LINE*` skipped with `CONTENT_WARNING_PRIMITIVE_MODE_SKIPPED`; attributes `POSITION` (required), `NORMAL`, `TANGENT`, `TEXCOORD_0`, `TEXCOORD_1` (further sets ignored with a warning), `COLOR_0` (further sets ignored), `JOINTS_0`/`WEIGHTS_0` and `JOINTS_1`/`WEIGHTS_1` (8 influences; further sets refused); non-indexed primitives get sequential indices |
| morph targets | `POSITION`, `NORMAL`, `TANGENT` deltas; `mesh.weights` default weights; `extras.targetNames` names targets (else `Target<n>`) |
| skins | `joints`, `inverseBindMatrices` (absent means identity), `skeleton` |
| animations | channels `translation`, `rotation`, `scale`, `weights`; samplers `STEP`, `LINEAR`, `CUBICSPLINE`; inputs strictly increasing |
| materials | `pbrMetallicRoughness` (factors and textures), `normalTexture` (with `scale`), `occlusionTexture` (with `strength`), `emissiveTexture`, `emissiveFactor`, `alphaMode` (`OPAQUE`, `MASK`, `BLEND`), `alphaCutoff`, `doubleSided`; the glTF default material for primitives without one |
| textures, images, samplers | images by URI or buffer view, MIME `image/png`, `image/jpeg`; sampler wrap modes (`REPEAT` → wrap, `CLAMP_TO_EDGE` → clamp, `MIRRORED_REPEAT` → wrap with `CONTENT_WARNING_SAMPLER_MIRRORED`); filters are ignored (GEOM's sampler policy decides) |
| cameras | `perspective` (`yfov`, `znear`, `zfar` optional, `aspectRatio` ignored), `orthographic` (`xmag` ignored, `ymag`, `znear`, `zfar`) |
| `extras` | read only for `zenithId` (Importers.md "Product keys"), `targetNames` and, on an `MSFT_lod` root node, `MSFT_screencoverage` (an array of numbers, one per level, which that extension stores in `extras`, not in an extension object; read from the JSON chunk as Importers.md "Zenith_GltfSource header" step 2 states); other `extras` are ignored |
| `extensionsUsed`, `extensionsRequired` | "glTF extension support" |

## glTF extension support

<!-- elaboration -->
| Extension | Status | Stage | Behaviour |
|---|---|---|---|
| `KHR_mesh_quantization` | supported | S3 | quantized `POSITION`, `NORMAL`, `TANGENT`, `TEXCOORD_n` accessors decoded to `float` with their `normalized` flag |
| `EXT_meshopt_compression` | supported | S3 | compressed buffer views decoded with `meshopt_decodeVertexBuffer`, `meshopt_decodeIndexBuffer`, `meshopt_decodeIndexSequence` and the filters `OCTAHEDRAL`, `QUATERNION`, `EXPONENTIAL`; the fallback buffer is never read |
| `KHR_texture_transform` | supported | S4 | Importers.md "Textures referenced by materials" |
| `KHR_texture_basisu` | supported | S4 | the KTX2 image transcoded to RGBA8 through KTX-Software and re-encoded by GEOM (`CONTENT_WARNING_BASISU_REENCODED`) |
| `KHR_materials_emissive_strength` | supported | S4 | multiplies `emissiveFactor` |
| `KHR_materials_unlit` | supported | S4 | `Material_Unlit` |
| `KHR_lights_punctual` | supported | S4 | Importers.md "Lights" |
| `MSFT_lod`, `MSFT_screencoverage` | supported | S4 | the authored LOD chain of GEOM's `lod.mode = authored` (`Zenith2/Docs/Design/GEOM/Meshes.md → "LOD generation"`): `MSFT_lod` from the node's `extensions`, `MSFT_screencoverage` from the same node's `extras` ("glTF core support"), both read from the JSON chunk when the pinned fastgltf does not parse them |
| `EXT_mesh_gpu_instancing` | supported | S4 | each instance becomes one generated prefab entity (at most 65,536 per node) |
| `KHR_materials_clearcoat`, `KHR_materials_sheen` | ignored with `CONTENT_WARNING_EXTENSION_IGNORED` until Stage 5, then mapped | S5 | Importers.md "Materials" |
| `KHR_materials_ior`, `_specular`, `_transmission`, `_volume`, `_iridescence`, `_anisotropy`, `_dispersion`, `_variants` | ignored with `CONTENT_WARNING_EXTENSION_IGNORED` | — | no engine material carries them |
| `KHR_animation_pointer` | ignored with `CONTENT_WARNING_EXTENSION_IGNORED` | — | the channels are dropped |
| `KHR_xmp_json_ld` | ignored | — | provenance comes from the `.meta`, not the source |
| `KHR_draco_mesh_compression` | refused when required; ignored, with the fallback attributes used, when only used | — | no Draco decoder is a dependency |
| `EXT_texture_webp`, `EXT_texture_avif` | refused when required; ignored, with the fallback image used, when only used | — | no WebP or AVIF decoder is a dependency |
| any other extension | refused when listed in `extensionsRequired`; ignored with `CONTENT_WARNING_EXTENSION_IGNORED` when only used | — | — |

A refused extension fails the import with `IMPORT_ERROR_SOURCE_INVALID`, detail `CONTENT_IMPORT_DETAIL_REQUIRED_EXTENSION`, and the message names the extension and the source; an ignored one is one warning per (source, extension).

## Feature support matrix

Levels: **full** — every glTF feature of the row imports and the route's fixture passes "Fidelity thresholds"; **route** — supported through the route's intermediate step; **none** — the DCC route cannot express the feature, and the importer receives nothing for it.

<!-- elaboration -->
| Feature | Blender | 3ds Max | Houdini | Maya | gltfpack | What the importer does with it |
|---|---|---|---|---|---|---|
| skins | full (4 or 8 influences) | full | full (KineFX) | route | full (preserves) | Importers.md "Skins" |
| morphs | full (shape keys; normals optional) | full | full | route | full | Importers.md "Morph targets" |
| animation | full (actions, baked or sampled) | full (baked 30 Hz) | full | route | full | Importers.md "Animations" |
| materials | full (Principled BSDF → metallic-roughness) | full (physical material) | full (principled shader) | route | full | Importers.md "Materials" |
| cameras | full | full | full | route | full | Importers.md "Cameras" |
| lights | full (`KHR_lights_punctual`) | full | none (the ROP writes no lights) | route | full (preserves) | Importers.md "Lights" |
| axis conversion | exporter (+Y up) | exporter | exporter | route | n/a | "Basis change algorithm" for misconfigured sources |
| handedness | n/a (glTF is right-handed) | n/a | n/a | route | n/a | "Basis change algorithm" (`transform.mirror`) |
| unit conversion | exporter (unit scale 1.0) | exporter (1 unit = 1 m) | exporter | route (converter or Blender) | n/a | "Unit conversion" (`transform.unitScale`) |

- Each cell other than none and n/a is proven by a recorded fixture of that route passing its feature's test ("Per-DCC fixtures"); a cell the recording ticket finds unattainable for the pinned exporter is changed to the measured level in that ticket's diff of this table, with the reason, and the importer's behaviour does not change.
- The importer's behaviour per feature is DCC-independent: it implements the glTF rows of "glTF core support" and "glTF extension support"; the matrix records what each route delivers into them.

## Source format tables

### Scene sources

| Extension | Importer | Notes |
|---|---|---|
| `.gltf`, `.glb` | `gltf` | "glTF core support" |
| `.fbx` | `fbx` | "FBX route" |
| `.obj`, `.dae`, `.usd`, `.usda`, `.usdc`, `.usdz`, `.3ds`, `.blend`, `.max`, `.ma`, `.mb`, `.hip` | none | not asset sources: the scan warns `COOK_WARNING_UNRECOGNISED_FILE` (`Zenith2/Docs/Design/ASSET/Database.md → "What is an asset source"`); DCC working files belong outside the content roots |

### Image sources

| Extension | Formats decoded | Output layout | Notes |
|---|---|---|---|
| `.png` | 1, 2, 4, 8 and 16 bits per channel; grey, grey-alpha, palette, RGB, RGBA; interlaced | `R8G8B8A8` (8-bit and lower sources) or `R16G16B16A16` (16-bit sources) | the gAMA, cHRM, iCCP and sRGB chunks are ignored: colour space is the preset's `texture.usage` (GEOM) |
| `.jpg`, `.jpeg` | baseline and progressive, 8-bit, grey, YCbCr; arithmetic coding and 12-bit refused | `R8G8B8A8` with alpha 255 | EXIF orientation is ignored and warned (`CONTENT_WARNING_EXIF_ORIENTATION_IGNORED`) when it is not 1 |
| `.tga` | uncompressed and RLE, 8-bit grey, 24-bit, 32-bit; colour-mapped refused | `R8G8B8A8` | origin bit honoured (bottom-left sources flipped) |
| `.bmp` | uncompressed 24- and 32-bit, BI_BITFIELDS 32-bit | `R8G8B8A8` | RLE refused |
| `.exr`, `.hdr`, `.psd`, `.tif`, `.tiff`, `.webp`, `.dds`, `.ktx2` | none | — | not image sources: HDR environment sources are LIGHT's (`Zenith2/Docs/Design/LIGHT.md`), and a `.ktx2` reaches the pipeline only inside a glTF through `KHR_texture_basisu` |

### Audio sources

| Extension | Formats decoded | Loop points | Markers |
|---|---|---|---|
| `.wav` | PCM 8, 16, 24, 32-bit integer; IEEE float 32 and 64; WAVE_FORMAT_EXTENSIBLE with its channel mask; RF64 | the first loop of the `smpl` chunk | `cue ` points with `LIST`/`adtl`/`labl` names |
| `.aif`, `.aiff` | AIFF and AIFF-C (`NONE`, `sowt`, `fl32`) PCM | the sustain loop of the `INST` chunk through its `MARK` IDs | `MARK` chunk entries |
| `.flac` | 8 … 32-bit, 1 … 8 channels | Vorbis comments `LOOPSTART` and `LOOPLENGTH` or `LOOPEND` (frames) | Vorbis comments `CUE<n>=<frame>:<name>` |
| `.ogg` | Ogg Vorbis (not Opus) | the same Vorbis comments | the same |
| `.mp3` | MPEG-1 and -2 Layer III, encoder delay and padding from the LAME/Xing header trimmed | none (an MP3 loop is set through `audio.loopStart`/`audio.loopEnd`) | none |

Every decoded source becomes the `decoded-audio` blob of `Zenith2/Docs/Design/AUDIO/Authoring.md → "Audio importer boundary"` (Importers.md "Audio importer").

## Conversion settings

The `gltf` and `fbx` importers declare these settings ("Zenith_ImporterDesc" of `Zenith2/Docs/Design/ASSET/Import.md → "Importer interface"`); defaults are the glTF convention, so a correctly exported source needs none:

| Key | Type | Default | Values or range | Meaning |
|---|---|---|---|---|
| `transform.sourceUp` | ENUM | `+y` | `+x`, `-x`, `+y`, `-y`, `+z`, `-z` | the source's up direction in its own coordinates |
| `transform.sourceFront` | ENUM | `+z` | the same six | the direction the source's assets face; must be perpendicular to `transform.sourceUp` (`IMPORT_ERROR_SETTING_INVALID`, detail `CONTENT_IMPORT_DETAIL_AXES_NOT_PERPENDICULAR`, otherwise) |
| `transform.mirror` | ENUM | `none` | `none`, `x`, `y`, `z` | an engine axis negated after the axis change: converts a left-handed source |
| `transform.unitScale` | FLOAT | 1.0 | 0.0001 … 10,000 | metres per source unit (0.01 for a centimetre export) |
| `normal.flipGreen` | BOOL | false | — | negate tangent-space Y of every normal map the source carries (a DirectX-convention map); the `image` importer declares the same key |

The image importer's `normal.flipGreen` applies to an image whose `texture.usage` is `normal`; a glTF's flag applies to its embedded normal maps. External images carry their own flag in their own `.meta`.

## Basis change algorithm

<!-- elaboration -->
Given the settings, `Zenith_MakeContentConversion` (`Zenith2/Tools/ContentCore/Zenith_ContentConversion.h`, Importers.md "ZenithContentCore API") builds:

1. U = the unit vector of `transform.sourceUp`, F = the unit vector of `transform.sourceFront`, both in source coordinates; U · F ≠ 0 is the refusal above.
2. X_src = U × F, the source-space direction that becomes engine +X (engine +X = +Y × +Z).
3. R = the 3 × 3 matrix whose rows are X_src, U, F; R is a signed permutation with det R = +1, so `R · v` maps a source vector to engine axes exactly (no rounding).
4. M = the identity, or `diag(−1, 1, 1)`, `diag(1, −1, 1)`, `diag(1, 1, −1)` for `transform.mirror` `x`, `y`, `z`.
5. Q = M · R (orthogonal, signed permutation, det Q = det M = ±1); s = `transform.unitScale`; A = s · Q; B = the 4 × 4 affine matrix with linear part A and zero translation.
6. `bIdentity` = (Q = I and s = 1), the glTF default; every step below is skipped when it is set.

The conversion is applied by `Zenith_ParseGltfSource` before any product is built, so every importer product, ANIM's helpers and GEOM's intermediate receive engine-convention data:

| Datum | Converted value |
|---|---|
| vertex position p (mesh-local) | A · p |
| normal n | normalize(Q · n) (A⁻ᵀ = Q / s for an orthogonal Q) |
| tangent (t, w) | (normalize(Q · t), w × det Q) |
| morph deltas Δp, Δn, Δt | A · Δp, Q · Δn, Q · Δt |
| node local matrix L | B · L · B⁻¹ (so world matrices become B · W · B⁻¹ and every product of the hierarchy stays consistent) |
| node TRS (t, r, s) | t′ = A · t; the rotation matrix r′ = Q · r · Qᵀ (a proper rotation even when det Q = −1, because det(Q · r · Qᵀ) = det(Q)² · det r = 1); s′ = the diagonal of Q · diag(s) · Qᵀ, which for a signed permutation Q equals (s_π(0), s_π(1), s_π(2)) **with every sign kept** (entry i is Σ_k Q_ik² · s_k, and Q_ik² is 1 for the one non-zero k = π(i)), so a mirrored node (a negative scale component) stays mirrored |
| inverse bind matrix I | B · I · B⁻¹ |
| animation keys (and cubic-spline tangents) | translation: A · t (in-tangents and out-tangents likewise); rotation: the quaternion q = (w, v) becomes q′ = (w, det Q · (Q · v)), which is linear in q and therefore converts cubic-spline tangents (not unit quaternions) by the same formula, and keeps the source key's hemisphere (w is never negated); scale: permuted with signs kept, as for node TRS; weights unchanged |
| camera and spot or directional light orientation | the node's converted transform plus, when Q ≠ I (a unit-scale-only conversion, Q = I and s ≠ 1, creates no child), a generated child entity whose local rotation K is the proper rotation with K · (0, 0, −1) = Q · (0, 0, −1) and K · (0, 1, 0) = Q · (0, 1, 0), carrying the component (Importers.md "Cameras", "Lights"), so the view and emission directions are the source's |
| triangle winding | reversed for every triangle of every mesh when det Q = −1 ("Winding and the negative-determinant rule") |

The conjugation keeps the converted world equal to B applied to the source world, so the probe's markers land where "Convention probe asset" states for every combination of settings (`Test_ContentConversion_EveryAxisPairRoundTrips` asserts it for all 24 (U, F) pairs × 4 mirrors × 3 scales). Conjugation also preserves every determinant (det(B · W · B⁻¹) = det W), so a node whose source world matrix is mirrored is mirrored after any conversion and selects the mirrored variant of "Winding and the negative-determinant rule" (`Test_ContentConversion_NegativeScaleSurvivesConversion`).

### Matrix nodes and the negative-determinant decomposition

A node given as `matrix` is decomposed by ContentCore itself, never by fastgltf (the parser runs with `DecomposeNodeMatrices` off, so the result does not depend on the pinned fastgltf version's decomposition), in `Internal/Gltf/Zenith_GltfScene.cpp`:

1. The translation t is column 3. Let c0, c1, c2 be the columns of the 3 × 3 linear part L and d = det L.
2. s_i = ‖c_i‖ for i = 0, 1, 2. A column with s_i ≥ 1e-12 is non-zero and its direction is ĉ_i = c_i / s_i; a column with s_i < 1e-12 has scale 0 (s_i = 0) and its direction is completed at step 5. The non-zero directions are only normalised here, never orthonormalised, so that step 3 can see shear.
3. Shear check: every pair of non-zero directions satisfies |ĉ_i · ĉ_j| ≤ 1e-4; otherwise the matrix has shear and the import fails `CONTENT_IMPORT_DETAIL_NODE_TRANSFORM` naming the node.
4. When d < 0 (which needs all three columns non-zero): s_0 = −s_0 and ĉ_0 = −ĉ_0, so that the rotation is proper; the mirror is carried by the sign of the x scale, which is what the glTF TRS form of the same node would carry.
5. Completion, indices taken mod 3, so that every non-zero direction keeps its direction and det R = +1 by construction: one zero column i: ĉ_i = ĉ_(i+1) × ĉ_(i+2); two zero columns, with k the non-zero one: e = the standard basis vector with the smallest |e · ĉ_k| (ties to the lowest index), ĉ_(k+1) = normalize(e − (e · ĉ_k) · ĉ_k), ĉ_(k+2) = ĉ_k × ĉ_(k+1); three zero columns (a fully collapsed node): R = I.
6. R = [ĉ_0, ĉ_1, ĉ_2] (columns); R must satisfy det R > 0, which steps 4 and 5 guarantee for every matrix that passed step 3 (a failure is `CONTENT_IMPORT_DETAIL_NODE_TRANSFORM`, never an assert).
7. The quaternion of R is extracted by Shepperd's method (the largest of w², x², y², z² chosen as the pivot) and normalised with w ≥ 0, so T · R · S = M within 1e-5 relative per element (`Test_GltfSource_MatrixNodeNegativeDeterminantDecomposes` asserts the round trip and the sign rule for a matrix with det −1, one with det +8, a scale-0 column, and the 30° rotation about +Z with x scale 0, whose c0 = 0, c1 = (−0.5, 0.866, 0) and c2 = (0, 0, 1) complete to ĉ_0 = ĉ_1 × ĉ_2 = (0.866, 0.5, 0), so R is the 30° rotation and T · R · S = M; orthonormalising after inserting a basis vector would instead have turned c1 into +Y).

The decomposed TRS is then converted by the "node TRS" row above, so a negative-determinant `matrix` node and its equivalent negative-scale TRS node import identically.

## Unit conversion

- s multiplies positions, translations (node, animation, inverse-bind), morph position deltas, camera `znear`, `zfar` and orthographic `ymag`, and light `range`; it never multiplies angles, colours, intensities, normals, tangents, UVs or weights.
- A light's intensity is not scaled: glTF punctual intensities are candela and lux, unit-independent quantities.
- glTF sources are metres by specification; `transform.unitScale` exists for sources whose exporter was misconfigured, and the bounds report ("Bounds and facing report") makes such a source visible.

## Winding and the negative-determinant rule

- Triangles keep glTF's counter-clockwise winding ([Z2-N-REND-885]): the importer never reverses an index buffer except in the two cases below. `TRIANGLE_STRIP` conversion alternates the vertex order of odd triangles so every emitted triangle is counter-clockwise; `TRIANGLE_FAN` emits (0, i, i + 1).
- When det Q = −1 (a `transform.mirror` conversion), every triangle (a, b, c) becomes (a, c, b) and every tangent sign w is negated (the table above).
- A node whose world matrix (after conversion) has a negative determinant references a **mirrored variant** of its mesh: the product `mesh:<key>@mirrored`, identical to `mesh:<key>` except that every triangle is reversed and every tangent sign w negated, so the instance renders front faces counter-clockwise on screen and reconstructs its bitangent with the right handedness; the variant is declared only when such a node exists (`Test_GltfImporter_MirroredNodeUsesReversedVariant`).
- With `mesh.combine = perSource` ("Meshes" of Importers.md) the importer bakes every static instance into one mesh: positions W · p, normals normalize(W⁻ᵀ · n), tangents (normalize(W · t), w × sign det W), and triangles reversed exactly for instances with det W < 0.
- A skinned mesh ignores its node transform (glTF 2.0); a joint whose world matrix has a negative determinant is warned `CONTENT_WARNING_MIRRORED_JOINT`, because skinning by it flips winding locally.
- The importer's output is checked as GEOM checks it: `Test_GltfImporter_ProbeCubeWindsOutward` asserts that for every triangle of the probe cube (n_face · (centroid − cube centre)) > 0, which a reversed import fails.

<!-- informative -->
The reference engine wound front faces `cross(C − A, B − A)`, the opposite of glTF, so its importer reversed every triangle and then had to special-case reflected nodes; every `.glb` came out inside-out until both were right. With one winding convention shared by the format, the engine and the RHI's front-face state ([Z2-N-REND-198]), the importer has nothing to reverse in the common case, and the probe test fails loudly if anything does.
<!-- /informative -->

## Normals and tangents on import

| Situation | Behaviour |
|---|---|
| a primitive without `NORMAL` | flat normals, as glTF 2.0 requires: each triangle gets its own three vertices with the face normal; `CONTENT_WARNING_FLAT_NORMALS_GENERATED` once per mesh |
| a normal that is non-finite or shorter than 1e-6 | replaced by the area-weighted face normal of the vertex's triangles (GEOM's repair would do the same; doing it here keeps tangents sane); counted in the report |
| a normal not unit length (‖n‖ outside [0.999, 1.001]) | normalised |
| a primitive with `TANGENT` where any tangent is non-finite, shorter than 1e-6, or has |t · n| > 0.1 after normalisation, or w ∉ {−1, +1} | every tangent of that primitive is discarded (`CONTENT_WARNING_TANGENTS_DISCARDED`, naming the mesh and the first bad vertex) so GEOM generates MikkTSpace tangents for it (`Zenith2/Docs/Design/GEOM/Meshes.md → "Tangent generation"`); a partially valid tangent set is never passed through |
| a valid source tangent | orthogonalised against its normal (Gram–Schmidt) and normalised; w kept |
| a UV that is non-finite | the import fails `IMPORT_ERROR_SOURCE_INVALID`, detail `CONTENT_IMPORT_DETAIL_NON_FINITE`, naming the attribute and vertex |
| a degenerate UV triangle (zero UV area) in a primitive that needs generated tangents | reported `CONTENT_WARNING_DEGENERATE_UV` with its count; GEOM's MikkTSpace pass then gives those vertices its own fallback |

Every position, normal, tangent, UV, colour and weight leaving the importer is finite (`Test_GltfImporter_NoNonFiniteAttributeReachesIntermediate` over the fuzz seeds and the mutated fixtures).

<!-- informative -->
The reference engine's tangent frames came out NaN on every import: source tangents were trusted, and degenerate UVs fed the generator undefined bases. The rule here is all-or-nothing per primitive, so a mesh never mixes trusted and generated tangents across one seam.
<!-- /informative -->

## Bounds and facing report

Every `gltf` and `fbx` import writes, in the `contentImports` report member (Importers.md "Import report member"):

- per mesh product: the model-space axis-aligned bounds in metres after conversion;
- per scene: the world-space bounds in metres of every mesh instance of the scene after conversion, its extent (x, y, z), and its height (the y extent);
- the world positions of the nodes named by `validate.frontMarkerNode`, when set, and the yaw about +Y, measured from +Z, of the marker's offset from the scene's root entity: d = marker world position − root world position (the root entity is the scene's single root node, or the generated root at the origin when the scene has several), yaw = `Zenith_Atan2(d.x, d.z)` × 180 / π in degrees (CORE's deterministic function, `Zenith2/Docs/Design/CONTENT.md → "Common format rules"`); a d whose horizontal length (x, z) is below 1 mm reports a null yaw and the expectation warns `CONTENT_WARNING_EXPECTATION_FAILED` ("front marker on the up axis").

The `gltf` and `fbx` importers declare these expectation settings, which presets set per asset class:

| Key | Type | Default | Meaning |
|---|---|---|---|
| `validate.heightRangeMetres` | FLOAT_ARRAY (2) | `[]` (no check) | the default scene's height must lie in `[min, max]` |
| `validate.maxExtentMetres` | FLOAT | 0 (no check) | every extent component at most this |
| `validate.frontMarkerNode` | STRING | `""` (no check) | a node name whose world position's yaw must be within `validate.frontToleranceDegrees` of 0 |
| `validate.frontToleranceDegrees` | FLOAT | 30 | 0 … 180 |
| `validate.strict` | BOOL | false | a failed expectation fails the import (`IMPORT_ERROR_SOURCE_INVALID`, detail `CONTENT_IMPORT_DETAIL_EXPECTATION_FAILED`) instead of warning `CONTENT_WARNING_EXPECTATION_FAILED` |

A height outside a character preset's range is almost always a unit error (a centimetre export reads 170 m), and a front marker at yaw 180° is a character facing backwards; both are named in the message with the measured value and the setting that would correct it (`transform.unitScale = 0.01`, `transform.sourceFront = -z`).

## Convention probe asset

The probe is the one asset every route builds, exports and imports; its expected values are derived from this specification, never recorded.

<!-- elaboration -->
| Element | Specification (engine convention: metres, +Y up, +Z front) |
|---|---|
| scene `ConventionProbe` | root node `Probe` at the origin, identity |
| mesh `ProbeCube` on node `Body` | a cube of edge 1 m centred at (0, 0.5, 0), 24 vertices (4 per face, flat normals), one UV square per face, material `ProbeMaterial`; one morph target `Bulge` moving the four +Z-face vertices by (0, 0, 0.1); authored `TANGENT` on every vertex: (+1, 0, 0) with w = +1 on the ±Z and ±Y faces, (0, 0, +1) with w = −1 on the ±X faces (each orthogonal to its face normal, so the importer's validation passes them through) |
| node `FrontMarker` | empty, at (0, 0.5, 1.0) |
| node `UpMarker` | empty, at (0, 1.5, 0) |
| node `RightMarker` | empty, at (1.0, 0.5, 0) |
| node `ProbeCamera` | perspective, `yfov` 0.8 rad, `znear` 0.1, `zfar` 100; at (0, 1, 5), identity rotation (looking along −Z toward the cube) |
| node `ProbeSun` | directional light, 10 lux, white; rotation −45° about +X, so it emits along (0, −0.7071, −0.7071) |
| node `ProbeSpot` | spot light at (2, 2, 0), 100 cd, inner 0.3 rad, outer 0.5 rad, range 10 m, emitting along −Y (rotation −90° about +X) |
| node `Rig` with skinned mesh `ProbeBar` | joints `Joint0` at (3, 0, 0) and `Joint1` at (3, 1, 0) (child of Joint0); a 0.2 × 2 × 0.2 m bar centred on x = 3, z = 0 from y = 0 to 2: 16 vertices in 4 square rings of 4 at y = 0, 0.9, 1.1 and 2 (ring corners at x = 3 ± 0.1, z = ± 0.1), 28 triangles (4 sides × 3 bands × 2 = 24, plus 2 per end cap), the rings at y = 0 and 0.9 weighted 1.0 to Joint0 and the rings at y = 1.1 and 2 weighted 1.0 to Joint1, so the band between y = 0.9 and 1.1 blends linearly |
| animation `Bend` | `Joint1` rotation from identity to 90° about +X over 1.0 s, two `LINEAR` keys |
| material `ProbeMaterial` | base colour factor (0.8, 0.1, 0.1, 1), metallic 0, roughness 0.5, no textures |

| Expected after import (any route, any correct settings) | Tolerance |
|---|---|
| `FrontMarker` world position (0, 0.5, 1.0); yaw 0° | 1 mm; 0.1° |
| `UpMarker` (0, 1.5, 0); `RightMarker` (1.0, 0.5, 0) | 1 mm |
| cube bounds [−0.5, 0, −0.5] … [0.5, 1, 0.5] | 1 mm |
| every cube triangle winds outward (n_face · (centroid − (0, 0.5, 0)) > 0) | exact |
| every cube vertex's reconstructed bitangent B = w · (n × t), from the imported normal, tangent and sign, equals its face's row of the bitangent table below | 0.1° |
| camera: `Zenith.Render.Camera` Active false, FovYDegrees 45.8366, NearMetres 0.1, FarMetres 100; world view direction (0, 0, −1) | exact; 0.01°; relative 1e-4; 0.1° |
| sun: `Zenith.Lighting.DirectionalLight` IlluminanceLux 10; world emission direction (0, −0.7071, −0.7071) | 1%; 0.1° |
| spot: InnerAngleDegrees 17.1887, OuterAngleDegrees 28.6479, Intensity 100 (candela), RangeMetres 10; emission (0, −1, 0) | 0.01°; 1%; 1 mm; 0.1° |
| `Bulge` maximum position delta (0, 0, 0.1) on the +Z face | 1 mm |
| skeleton `skel:Joint0`: 2 joints, Joint1 rest local translation (0, 1, 0); `Bend` at t = 1 s: Joint1 local rotation 90° about +X | 1 mm; 0.1° |
| material `material:ProbeMaterial`: `m_xBaseColourFactor` (0.8, 0.1, 0.1, 1), `m_fMetallicFactor` 0, `m_fRoughnessFactor` 0.5 | 1/255 |

The bitangent table is the independent oracle of the tangent rule (w × det Q in "Basis change algorithm"): it is written by hand from the face frames above, in engine coordinates, and is never computed through `Zenith_ContentConversion::ConvertTangent`:

| Face | Normal n | Tangent t | w | Bitangent B = w · (n × t) |
|---|---|---|---|---|
| +X | (1, 0, 0) | (0, 0, 1) | −1 | (0, 1, 0) |
| −X | (−1, 0, 0) | (0, 0, 1) | −1 | (0, −1, 0) |
| +Y | (0, 1, 0) | (1, 0, 0) | +1 | (0, 0, −1) |
| −Y | (0, −1, 0) | (1, 0, 0) | +1 | (0, 0, 1) |
| +Z | (0, 0, 1) | (1, 0, 0) | +1 | (0, 1, 0) |
| −Z | (0, 0, −1) | (1, 0, 0) | +1 | (0, −1, 0) |

A source written in another convention carries Q⁻¹ · n, Q⁻¹ · t and w · det Q (the builder applies the inverse conversion, as it does for positions), so after a correct import every face's engine-space frame, and therefore its bitangent, is this table's row; an importer that drops the w × det Q rule reconstructs the negated bitangent on every face under `transform.mirror` and fails the row by 180°. `Test_GltfImporter_ProbeTangentFrameUnderConversion` runs the check at identity, under `transform.sourceUp=+z, transform.sourceFront=-y` and under `transform.mirror=x`.

The probe's build scripts are committed as text so every route rebuilds the same scene: `Zenith2/Tools/ZenithCook/Tests/Fixtures/Content/Dcc/Probe/make_probe_blender.py` (Blender Python), `make_probe_max.ms` (MaxScript), `make_probe_houdini.py` (Houdini Python), `make_probe_maya.py` (Maya Python); a worker may write and review them, and only the recording ticket runs them.

## Per-DCC fixtures

### Fixture layout

```text
Zenith2/Tools/ZenithCook/Tests/Fixtures/Content/Dcc/
	dcc_matrix.json                      the machine-readable matrix (below)
	Probe/make_probe_*.{py,ms}           the probe build scripts
	<Route>/ConventionProbe.gltf         the route's export, text with data URIs (or .glb, always LFS-tracked: .glb is in the storage policy's binaryExtensions)
	<Route>/ConventionProbe.gltf.meta    its sidecar, default settings
	<Route>/Feature<Name>.gltf (+ .meta) one per feature cell the probe does not cover (an 8-influence skin, a 4-level MSFT_lod chain)
	<Route>/RECORDING.md                 exporter and DCC versions, export options, the recording ticket and date
```

`<Route>` is `Blender`, `Max`, `Houdini`, `Maya` (its route's output), `Gltfpack` (gltfpack over the Blender export).

### DCC matrix file

The file `dcc_matrix.json` beside the route directories:

```json
{
	"$schema": "zenith2.dcc-matrix",
	"schemaVersion": 1,
	"routes": [
		{
			"route": "Blender",
			"dcc": "Blender",
			"dccVersion": "4.2.3",
			"exporter": "io_scene_gltf2",
			"exporterVersion": "4.2.70",
			"fixtures": ["ConventionProbe.gltf"],
			"features": { "skins": "full", "morphs": "full", "animation": "full", "materials": "full", "cameras": "full", "lights": "full" }
		}
	]
}
```

| Key | Rule |
|---|---|
| `routes[].route` | one of the `<Route>` names; unique |
| `dccVersion`, `exporterVersion` | the versions the recording used, strings of at most 32 bytes |
| `fixtures` | files of the route's directory; each must exist and import |
| `features` | the six features, each `full`, `route` or `none`, equal to the "Feature support matrix" cell of the route |

`Test_DccMatrix_TableAgreesWithDesign` parses the "Feature support matrix" table of this file (a ZenithCook test may read Markdown with the STL): it reads exactly the six rows whose first cell is `skins`, `morphs`, `animation`, `materials`, `cameras` or `lights` (the three convention rows below them are not features and are skipped by name), takes the first whitespace-delimited word of each route cell (`full (4 or 8 influences)` reads `full`), and asserts every route's `features` equal its column; `Test_DccMatrix_EveryRowHasPassingFixture` imports every listed fixture through the real importers in `Zenith_MakeImportTestContext` contexts (`Zenith2/Docs/Design/GEOM.md → "Fixtures"`) and runs the probe expectations of "Convention probe asset" on `ConventionProbe` and each feature's checks on its `Feature*` file.

Both tests pass on a matrix with no recorded route, because before any recording ticket lands `dcc_matrix.json` lists none. The Stage 4 evidence is therefore the acceptance-only case `Test_DccMatrix_EverySupportedRouteRecorded` (suite `z2-acceptance-content`, which the default `Test` run excludes and the Stage 4 acceptance runs, `Zenith2/Docs/Design/CONTENT/Tests.md → "Convention tests"`): it asserts that every route of "DCC routes" whose "Matrix status" begins with `supported` (Blender, Max, Houdini, Maya through its route, Gltfpack) has a `dcc_matrix.json` row, that the row lists `ConventionProbe.gltf` or `ConventionProbe.glb`, and that every feature cell of the route that is not `none` or `n/a` is proven by a listed fixture that `Test_DccMatrix_EveryRowHasPassingFixture` imports.

### Recording procedure

A `needs-human` recording ticket per route (Tests.md "Needs-human recordings"): run the route's probe script in the pinned DCC, export with the route's options, add the files and `RECORDING.md`, update `dcc_matrix.json`, and run `ZenithCook_Tests --test-case=*DccMatrix*` and `--test-suite=z2-acceptance-content`; the ticket commits the exports. A recording that cannot reach a cell's level changes that cell in "Feature support matrix" with the reason in the same diff.

## Fidelity thresholds

Two imports of one asset are the same asset (for D-SEL-FBX and for a re-recording) when, after both are imported with default settings and matched by node name and product key:

<!-- elaboration -->
| Quantity | Threshold |
|---|---|
| node hierarchy and names | identical |
| vertex positions (matched by nearest neighbour in model space) | ≤ 0.1 mm |
| normals | ≤ 0.5° |
| UVs | ≤ 1e-4 |
| skin: joint names per influence | identical; weights ≤ 1/255 |
| morph deltas | ≤ 0.1 mm |
| animation sampled at 30 Hz over the clip | joint local rotation ≤ 0.1°, translation ≤ 0.1 mm, scale ≤ 1e-4 |
| materials | domain, two-sidedness identical; factors ≤ 1/255; texture bindings by role identical |
| cameras | projection kind identical; FovYDegrees ≤ 0.01°; planes relative ≤ 1e-4 |
| lights | kind identical; intensity ≤ 1%; colour ≤ 1/255; cone angles ≤ 0.01° |
| convention probe expectations | every row of "Convention probe asset" |

`Zenith_CompareImportedAssets` (in `Zenith_Bench_FbxFidelity`, "FBX route") implements the comparison and reports each failing quantity with its worst value.

## FBX route

D-SEL-FBX (open, Stage 4; `Zenith2/Docs/DecisionLog.md → "Open decisions"`) decides whether FBX reaches the pipeline through a conversion path. CONTENT owns its harness and ruling tickets:

1. **Harness ticket** (ordinary, `kind: harness`): `Zenith2/Benchmarks/FbxFidelity/` builds `Zenith_Bench_FbxFidelity`, which, for each candidate (FBX2glTF, Blender run headless with `--background --python`, a converter on the Autodesk FBX SDK), runs the candidate as a child process on every FBX export of the reference set (the probe and feature fixtures exported from Maya and 3ds Max as FBX by the recording tickets), imports its glTF output through the real `gltf` importer, imports the same asset's glTF export, and compares the two with "Fidelity thresholds"; it also converts each file twice and requires byte-identical output (a converter that is not deterministic fails), and requires self-contained output: one `.glb` with every buffer and image embedded (the candidate's embedding option: FBX2glTF `--binary --embed`, Blender's `export_format='GLB'` with images embedded, the SDK converter's equivalent), because the importer refuses an external URI in converter output (Importers.md "FBX importer"). `--smoke` runs one candidate over the probe; the full run writes `Zenith2/Docs/Research/D-SEL-FBX/report.md` with every quantity per candidate per fixture.
2. **Ruling ticket** (`needs-human`, `kind: decision`): runs the harness, reviews each passing candidate's licence for the licence inventory (SHIP), and appends the ruling to `Zenith2/Docs/DecisionLog.md` in its diff: the adopted candidate, or glTF-only when no candidate passes both the fidelity and the licence review.

The `fbx` importer implements both outcomes (Importers.md "FBX importer"); every importer ticket that depends on the outcome is `blocks`-linked from the ruling ticket ([Z2-N-ARCH-981]).
