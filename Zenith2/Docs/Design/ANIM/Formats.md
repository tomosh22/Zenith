# ANIM design: cooked formats, intermediates, documents and compression

Section file of `Zenith2/Docs/Design/ANIM.md`. It specifies the three cooked formats ANIM owns (`.zskel`, `.zanim`, `.zagraph`), the two import intermediates CONTENT's importers write through ANIM's helpers, the conversion from glTF skins and animations, the clip compression algorithm with its error bounds, the authoring documents and the parser limits. The asset kinds, magics and extensions are ASSET's table (`Zenith2/Docs/Design/ASSET.md → "Asset kinds"`, codes 7, 8 and 9); the container every cooked file uses is ASSET's (`Zenith2/Docs/Design/ASSET/Formats.md → "Header layout"`, `Zenith2/Docs/Design/ASSET/Formats.md → "Section table"`).

## Scope of this file

| Topic | Heading |
|---|---|
| rules shared by every ANIM format | "Common format rules" |
| the skeleton asset | "Skeleton file" |
| the clip asset | "Clip file" |
| the compiled graph asset | "Graph file" |
| the bit rates of quantised tracks | "Bit-rate table" |
| how a clip is compressed and what error it may carry | "Compression algorithm" |
| what CONTENT's importers write | "Skeleton intermediate", "Clip intermediate" |
| glTF skins and animations to ANIM data | "Import conversion" |
| the authoring documents | "Authoring documents" |
| every parser's limits | "Parser limits" |

## Common format rules

- Every ANIM cooked file is an ASSET cooked asset file: the 64-byte header, the 40-byte section entries sorted by (kind, index), 16-byte section alignment, and section compression recorded in each entry's COMPRESS descriptor ([Z2-N-CONTRACT-527], Z2-D-004). The format version in the header is the kind's ANIM format version: skeleton 1, clip 1, graph 1 (`uANIM_SKELETON_FORMAT_VERSION`, `uANIM_CLIP_FORMAT_VERSION`, `uANIM_GRAPH_FORMAT_VERSION` in `Zenith_AnimationTypes.h`), registered as `Zenith_AssetKindLoaderDesc::m_uFormatVersion`; a file of another version is refused by ASSET's check C5 before ANIM sees it.
- Section kinds: skeleton `0x0101 … 0x010D`, clip `0x0201 … 0x020B`, graph `0x0301 … 0x0312`. A kind outside the format's set is refused with `ANIMATION_ERROR_CORRUPT` (detail: the kind); a section that the format marks "exactly one" and that is absent or duplicated is refused the same way.
- All integers are little-endian; floats are IEEE 754 binary32 little-endian; every record is written and read field by field, never by `memcpy` of a C++ object (Z2-D-038). A reserved field is zero when written and refused when non-zero (`ANIMATION_ERROR_CORRUPT`).
- Every count, offset and size read from a section is checked against the section's decoded length with `Zenith_CheckedAdd`/`Zenith_CheckedMul` before use ([Z2-N-ARCH-951]); a violation is `ANIMATION_ERROR_CORRUPT` from the kind loader's `Fail` and never an assert ([Z2-N-CONTRACT-472]).
- Name hashes are `Zenith_Hash64` (seed 0, D-SEL-HASH (open, Stage 1)) of the UTF-8 bytes; because cooked files re-cook on any engine version change ([Z2-N-CONTRACT-529]), a D-SEL-HASH ruling moves every hash at once and breaks no file.
- Joint transforms are stored as `t` (3 × f32, metres), `r` (4 × f32, x, y, z, w, unit, `w ≥ 0`), `s` (3 × f32), the 40-byte layout of `Zenith_Transform` (`Zenith2/Docs/Design/SCENE/Transforms.md → "Transform value type"`); 3 × 4 matrices are 12 × f32 row-major, the layout of `Flux_JointMatrix` (`Zenith2/Docs/Design/GEOM/Skinning.md → "Flux_Skinning header"`), with the implicit fourth row (0, 0, 0, 1).
- Coordinate conventions are CORE's: right-handed, +Y up, model front +Z (`Zenith2/Docs/Design/CORE/Maths.md → "Coordinate systems"`).

## Skeleton file

The skeleton asset (`Animation.Skeleton`, `.zskel`, magic `ZSKL`) is the one definition of a rig: joint hierarchy, joint names, **the one rest pose**, the rig annotations (roles, chains, IK definitions, sockets, mirror pairs, shell distances) and the curve table. One skeleton has exactly one rest pose, and the rest pose is the bind pose: every mesh bound to the skeleton is modelled in it ("Skeleton validation", rule SV9), so no import ever re-poses a mesh.

### Skeleton sections

| Kind | Name | Count | Content |
|---|---|---|---|
| `0x0101` | `ANIM_SECTION_SKEL_INFO` | exactly one | "Skeleton info section" |
| `0x0102` | `ANIM_SECTION_SKEL_HIERARCHY` | exactly one | parent joint index per joint, `u16` × J; `0xFFFF` for joint 0 alone |
| `0x0103` | `ANIM_SECTION_SKEL_REST_LOCAL` | exactly one | rest local transform per joint, 40 bytes × J |
| `0x0104` | `ANIM_SECTION_SKEL_REST_MODEL_INVERSE` | exactly one | inverse rest model matrix per joint, 48 bytes × J |
| `0x0105` | `ANIM_SECTION_SKEL_NAMES` | exactly one | "Joint name sections" |
| `0x0106` | `ANIM_SECTION_SKEL_NAME_INDEX` | exactly one | "Joint name sections" |
| `0x0107` | `ANIM_SECTION_SKEL_ROLES` | zero or one | "Rig sections" |
| `0x0108` | `ANIM_SECTION_SKEL_CHAINS` | zero or one | "Rig sections" |
| `0x0109` | `ANIM_SECTION_SKEL_IK_DEFS` | zero or one | "Rig sections" |
| `0x010A` | `ANIM_SECTION_SKEL_CURVES` | zero or one | "Curve table section" |
| `0x010B` | `ANIM_SECTION_SKEL_SOCKETS` | zero or one | "Rig sections" |
| `0x010C` | `ANIM_SECTION_SKEL_MIRROR` | zero or one | "Rig sections" |
| `0x010D` | `ANIM_SECTION_SKEL_SHELL` | exactly one | shell distance per joint, f32 × J, metres ("Error metric") |

Every skeleton section is small (a 1,024-joint rig is under 200 KiB in total); the kind loader requests all of them in one round.

### Skeleton info section

<!-- elaboration -->
| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | joint count J | 1 … `uANIM_MAX_JOINTS` (1,024) |
| 4 | 4 | curve count C | 0 … `uANIM_MAX_CURVES` (512) |
| 8 | 2 | socket count | 0 … 256 |
| 10 | 2 | chain count | 0 … 64 |
| 12 | 2 | IK definition count | 0 … 32 |
| 14 | 2 | role count | 0 … `uANIM_JOINT_ROLE_COUNT` (54) |
| 16 | 2 | flags | bit 0 mirror section present; bit 1 humanoid profile complete (every role marked required in "Rig sections" is assigned); bits 2–15 zero |
| 18 | 2 | reserved | 0 |
| 20 | 4 | maximum reach R | metres: the largest root-relative sum of bone lengths over every joint (the R of GEOM's posed-extent check, [Z2-N-REND-892]) |
| 24 | 4 | hips height | metres: the rest model-space `y` of the `JOINT_ROLE_HIPS` joint, 0 when unassigned; the retarget translation scale's input ("Retarget map construction" of `Zenith2/Docs/Design/ANIM/Runtime.md`) |
| 28 | 4 | reserved | 0 |
| 32 | 8 | rest pose hash | `Zenith_Hash64` of the decoded `ANIM_SECTION_SKEL_REST_LOCAL` bytes |
| 40 | 8 | hierarchy hash | `Zenith_Hash64` of, for each joint in order, the name hash then the parent index as `u16` |
| 48 | 16 | reserved | 0 |

### Joint hierarchy and rest pose sections

- Joint order is depth-first pre-order from joint 0, children in ascending source-node order; so `parent(j) < j` for every `j ≥ 1`, and model-space composition is one forward pass. The loader refuses a parent index `≥ j` (`ANIMATION_ERROR_CORRUPT`).
- `ANIM_SECTION_SKEL_REST_LOCAL` holds the rest local transform `L_j` (relative to the parent; joint 0 relative to model space). `ANIM_SECTION_SKEL_REST_MODEL_INVERSE` holds `B_j = (M_j)⁻¹` where `M_0 = TRS(L_0)` and `M_j = M_parent(j) · TRS(L_j)`, computed by the cook in `double` and rounded once to f32; the loader recomputes `M_j` in f32 and keeps both (the recomputed rest model for poses, the cooked inverse for retargeting and validation).
- Every `r` is unit within 1e-5 and has `w ≥ 0`; every scale component lies in [1e-4, 1e4]; the loader refuses others.

### Joint name sections

`ANIM_SECTION_SKEL_NAMES`: J records of 16 bytes `{ u64 name hash; u32 string offset; u16 string length (1 … 63); u16 reserved }`, then the UTF-8 blob (no terminators), padded with zeros to a multiple of 4. `ANIM_SECTION_SKEL_NAME_INDEX`: J records of 16 bytes `{ u64 name hash; u16 joint; u16 reserved; u32 reserved }` sorted by ascending hash, so `Zenith_SkeletonAsset::FindJoint` is a binary search. Two joints with one name, or two names with one hash, are refused by the cook (`ANIM_COOK_DETAIL_DUPLICATE_JOINT_NAME`, `ANIM_COOK_DETAIL_NAME_HASH_COLLISION`).

### Rig sections

The rig annotations come from the rig document of "Rig document schema"; a skeleton without one has none of these sections.

`ANIM_SECTION_SKEL_ROLES`: records `{ u16 role; u16 joint }`, sorted by role, each role at most once. The role enumeration `Zenith_JointRole` (`Zenith_AnimationTypes.h`) is the humanoid profile retargeting reads:

| Values | Roles | Required for "humanoid profile complete" |
|---|---|---|
| 1–8 | `HIPS`, `SPINE`, `CHEST`, `UPPER_CHEST`, `NECK`, `HEAD`, `LEFT_EYE`, `RIGHT_EYE` | `HIPS`, `SPINE`, `CHEST`, `NECK`, `HEAD` |
| 9–16 | `LEFT_SHOULDER`, `LEFT_UPPER_ARM`, `LEFT_LOWER_ARM`, `LEFT_HAND`, `RIGHT_SHOULDER`, `RIGHT_UPPER_ARM`, `RIGHT_LOWER_ARM`, `RIGHT_HAND` | the six arm segments except the shoulders |
| 17–24 | `LEFT_UPPER_LEG`, `LEFT_LOWER_LEG`, `LEFT_FOOT`, `LEFT_TOES`, `RIGHT_UPPER_LEG`, `RIGHT_LOWER_LEG`, `RIGHT_FOOT`, `RIGHT_TOES` | the six leg segments except the toes |
| 25–39 | `LEFT_THUMB_1`, `LEFT_THUMB_2`, `LEFT_THUMB_3`, `LEFT_INDEX_1` … `LEFT_LITTLE_3` (five fingers × three phalanges, thumb, index, middle, ring, little) | none |
| 40–54 | `RIGHT_THUMB_1` … `RIGHT_LITTLE_3`, in the same order | none |

<!-- elaboration -->
The enumeration in `Zenith_AnimationTypes.h` lists every value explicitly with its number; `JOINT_ROLE_NONE = 0`, and `uANIM_JOINT_ROLE_COUNT = 54` is the number of defined roles. Values from 55 are free for face roles a CHAR rig profile may add; a new role takes the next value and never renumbers an existing one, because roles are cooked into skeleton files.

`ANIM_SECTION_SKEL_CHAINS`: records of 16 bytes `{ u64 name hash; u16 start joint; u16 end joint; u8 chain role (`Zenith_ChainRole`: `SPINE`, `NECK`, `LEFT_ARM`, `RIGHT_ARM`, `LEFT_LEG`, `RIGHT_LEG`, `TAIL`, `CUSTOM`); u8 flags (bit 0 retarget by IK goal); u16 reserved }`; the end joint is a descendant of the start joint and the chain is the path between them.

`ANIM_SECTION_SKEL_IK_DEFS`: records of 32 bytes `{ u64 name hash; u8 kind (`Zenith_IKKind`: `TWO_BONE` 1, `CHAIN` 2, `LOOK_AT` 3, `FOOT` 4); u8 joint count (2 … 4); u16 flags; u16 joints[4]; f32 hint[3] }`. For `TWO_BONE` the joints are upper, middle, end and the hint is the rest model-space bend direction of the middle joint; for `CHAIN` the start and end joint (the chain between them, at most 32 joints); for `LOOK_AT` up to four joints, root-most first, and the hint is the joint-local forward axis; for `FOOT` the ankle, the toe and the leg's `TWO_BONE` definition index (in `joints[2]`), and the hint's `y` is the ankle's rest height above the sole (metres).

`ANIM_SECTION_SKEL_SOCKETS`: records of 40 bytes `{ u64 name hash; u16 joint; u16 reserved; f32 t[3]; f32 r[4] }`, an offset transform in the joint's space, sorted by name hash.

`ANIM_SECTION_SKEL_MIRROR`: J records of 4 bytes `{ u16 mirror joint; u8 mirror axis (0 X, 1 Y, 2 Z: the normal of the mirror plane in model space); u8 reserved }`; a joint on the plane names itself; the mirror relation is symmetric (the cook verifies `mirror(mirror(j)) = j`).

### Curve table section

`ANIM_SECTION_SKEL_CURVES`: C records of 16 bytes `{ u64 name hash; u8 kind (`Zenith_CurveKind`: `MORPH` 1, `CONTROL` 2, `MATERIAL` 3, `GAMEPLAY` 4); u8 flags (bit 0 additive-blended); u16 reserved; f32 default value }`. The table is the set of float channels a pose carries beside its joints: morph-target weights named like the mesh's targets (GEOM's `GEOM_SECTION_MORPH` name hash, `Zenith2/Docs/Design/GEOM/Skinning.md → "Morph targets"`), facial control curves, material parameters and gameplay curves (the last are simulation-visible through `Zenith_AnimationWorld::GetCurve`). The cook builds the table from the rig document's `curves` member and the union of curve names in the imported meshes and clips of the source, sorted by name hash.

### Skeleton published properties

The `anim.skeleton` cook step publishes, through `Zenith_CookContext::PublishProperty` (`Zenith2/Docs/Design/ASSET/Cook.md → "Zenith_CookContext"`), the properties other steps read with `ReadDependencyOutput`; GEOM's skinned-mesh cook reads the first five (cross-area row GB26 of the A5 tickets, row AN4):

| Property | Bytes | Content |
|---|---|---|
| `jointCount` | 4 | J, `u32` |
| `parents` | 2 J | the hierarchy section's bytes |
| `restModel` | 48 J | `M_j`, 3 × 4 row-major, in `double` rounded once |
| `restModelInverse` | 48 J | the `ANIM_SECTION_SKEL_REST_MODEL_INVERSE` bytes |
| `maxReach` | 4 | R, f32 |
| `jointNameHashes` | 8 J | name hash per joint, in joint order |
| `restPoseHash` | 8 | the info section's rest pose hash |
| `curveNameHashes` | 8 C | the curve table's hashes, in table order |
| `shellMeshes` | 16 N | the rig document's `shellMeshes` persistent IDs, in document order (0 bytes without a rig or the member); read by the `anim.clip` step only |

### Skeleton validation

The cook refuses a skeleton (`COOK_ERROR_STEP_FAILED`, detail the `Zenith_AnimationCookDetail` named) and the loader refuses a file (`ANIMATION_ERROR_CORRUPT`) on each rule:

| Rule | Check | Cook detail |
|---|---|---|
| SV1 | 1 ≤ J ≤ 1,024 | `ANIM_COOK_DETAIL_JOINT_LIMIT` |
| SV2 | exactly one root, `parent(j) < j` | `ANIM_COOK_DETAIL_HIERARCHY` |
| SV3 | names 1 … 63 UTF-8 bytes, unique, hash-unique | `ANIM_COOK_DETAIL_DUPLICATE_JOINT_NAME`, `ANIM_COOK_DETAIL_NAME_HASH_COLLISION` |
| SV4 | rest transforms finite, rotations unit with `w ≥ 0`, scales in [1e-4, 1e4] | `ANIM_COOK_DETAIL_REST_POSE_INVALID` |
| SV5 | no rest model matrix has shear (`Zenith_DecomposeAffine` reports `bHasShearOut`, `Zenith2/Docs/Design/SCENE/Transforms.md → "Transform value type"`) | `ANIM_COOK_DETAIL_REST_POSE_SHEAR` |
| SV6 | roles name existing joints, each at most once; chains connect a joint to a descendant; IK definitions name a valid parent chain | `ANIM_COOK_DETAIL_RIG_INVALID` |
| SV7 | mirror table symmetric | `ANIM_COOK_DETAIL_RIG_INVALID` |
| SV8 | shell distances in [1e-3, 10] m | `ANIM_COOK_DETAIL_RIG_INVALID` |
| SV9 | one rest pose: every skin of the source that binds a joint of this skeleton agrees on that joint's inverse bind matrix within the rest-pose tolerance ("Rest pose from inverse bind matrices") | `ANIM_COOK_DETAIL_REST_POSE_CONFLICT` |

SV9 is also enforced on the mesh side: GEOM's skinned-mesh cook compares each mesh joint's inverse bind matrix with `restModelInverse[remap[j]]` and refuses a mesh that differs by more than the same tolerance (cross-area row AN4), so a mesh can never bind to a skeleton in a pose other than the skeleton's rest pose and no step ever rebinds or deforms a mesh to make them agree.

## Clip file

The clip asset (`Animation.Clip`, `.zanim`, magic `ZANM`) holds one animation of one skeleton: uniformly sampled joint tracks and curves, quantised per segment, plus the root-motion track, events, sync markers and the stream table.

### Clip sections

| Kind | Name | Count | Content |
|---|---|---|---|
| `0x0201` | `ANIM_SECTION_CLIP_INFO` | exactly one | "Clip info section" |
| `0x0202` | `ANIM_SECTION_CLIP_TRACKS` | exactly one | "Track table section" |
| `0x0203` | `ANIM_SECTION_CLIP_CONSTANTS` | exactly one | the values of constant tracks, in track order: rotation 4 × f32, translation and scale 3 × f32 |
| `0x0204` | `ANIM_SECTION_CLIP_RANGES` | exactly one | "Clip range section" |
| `0x0205` | `ANIM_SECTION_CLIP_SEGMENT` | one per segment, index = segment number | "Segment section" |
| `0x0206` | `ANIM_SECTION_CLIP_ROOT_MOTION` | zero or one | "Root motion section" |
| `0x0207` | `ANIM_SECTION_CLIP_EVENTS` | zero or one | "Event and marker sections" |
| `0x0208` | `ANIM_SECTION_CLIP_MARKERS` | zero or one | "Event and marker sections" |
| `0x0209` | `ANIM_SECTION_CLIP_STRINGS` | zero or one | UTF-8 names of events and markers |
| `0x020A` | `ANIM_SECTION_CLIP_CURVES` | zero or one | the curve track table and clip-level curve ranges ("Track table section") |
| `0x020B` | `ANIM_SECTION_CLIP_STREAM_TABLE` | zero or one (present exactly when the clip is streamed) | "Stream table section" |

Every section except the segments is the clip's **header set**: it is resident whenever the clip asset is ACTIVE. Segments are resident with the header set for a clip that is not streamed and are streamed per chunk otherwise ("Clip streaming" of `Zenith2/Docs/Design/ANIM/Runtime.md`).

### Clip info section

<!-- elaboration -->
| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 16 | skeleton GUID | the `Zenith_PersistentID` of the skeleton the tracks index |
| 16 | 4 | duration D | seconds, finite, 0 … 3,600; 0 only for a single-pose clip |
| 20 | 4 | sample rate | Hz, one of 15, 24, 25, 30, 48, 50, 60, 120 |
| 24 | 4 | frame count F | 1 … 1,048,576; `F = 1` exactly when D = 0; otherwise `F = ceil(D × rate − 1e-4) + 1` |
| 28 | 2 | segment frames | 16 (`uANIM_SEGMENT_FRAMES`) |
| 30 | 2 | flags | bit 0 loop, bit 1 additive, bit 2 root motion present, bit 3 streamed, bit 4 pose library, bit 5 retargeted, bit 6 mirrorable (the skeleton has a mirror section); others zero |
| 32 | 4 | segment count S | `ceil(F / 16)` |
| 36 | 4 | joint count | equals the skeleton's J |
| 40 | 4 | curve count | equals the skeleton's C |
| 44 | 4 | event count | 0 … 4,096 |
| 48 | 4 | marker count | 0 … 4,096 |
| 52 | 1 | additive reference | `Zenith_AdditiveReference`: 0 none, 1 first frame, 2 rest pose, 3 another clip's frame |
| 53 | 1 | additive space | 0 local, 1 model |
| 54 | 1 | root motion mode | `Zenith_RootMotionMode`: 0 none, 1 XZ plus yaw, 2 XYZ plus yaw, 3 full |
| 55 | 1 | compression mode | 0 raw (every animated track at bit rate 18), 1 quantised |
| 56 | 2 | root motion joint | the joint whose motion is extracted; 0 when none |
| 58 | 2 | reserved | 0 |
| 60 | 4 | joint error threshold ε | metres |
| 64 | 4 | maximum measured joint error | metres, the largest `E(j, k)` of the clip; every joint-frame meets its target `max(ε, E_raw(j, k))` ("Error bound verification") |
| 68 | 4 | curve error threshold ε_c | absolute curve units |
| 72 | 4 | maximum measured curve error | ≤ ε_c |
| 76 | 4 | stream chunk count | 0 for a resident clip |
| 80 | 4 | segments per chunk | 0, or 1 … 64 |
| 84 | 8 | skeleton rest pose hash | the skeleton's rest pose hash at cook; a binding to a skeleton with another hash is refused at bind time (`ANIMATION_ERROR_SKELETON_MISMATCH`) |
| 92 | 16 | additive reference clip GUID | reference 3 only; zero otherwise |
| 108 | 4 | additive reference frame | reference 1 and 3 |
| 112 | 16 | reserved | 0 |

Frame k (0 ≤ k < F − 1) sits at time `k / rate`; frame F − 1 sits at time D, so the last interval may be shorter than `1 / rate` and no source is time-scaled to fit the grid.

### Track table section

`ANIM_SECTION_CLIP_TRACKS` holds J records of 4 bytes `{ u8 rotation kind; u8 translation kind; u8 scale kind; u8 reserved }`, each kind a `Zenith_TrackKind`: 0 `DEFAULT` (the skeleton's rest value; identity for an additive clip), 1 `CONSTANT` (one value in the constants section), 2 `ANIMATED` (quantised per segment). Constant tracks are enumerated in (joint ascending, rotation, translation, scale) order into the constants section; animated tracks in the same order define the **animated track index** that the range and segment sections use. A clip with root motion has the root-motion joint's removed motion already applied to its tracks ("Root motion extraction at cook").

`ANIM_SECTION_CLIP_CURVES` holds C records of 12 bytes `{ u8 kind (Zenith_TrackKind); u8 reserved[3]; f32 a; f32 b }`: for `CONSTANT`, `a` is the value; for `ANIMATED`, `a` and `b` are the clip-level minimum and extent; for `DEFAULT`, the curve takes the skeleton's default value. Animated curves are enumerated in curve order into the **animated curve index**.

### Clip range section

`ANIM_SECTION_CLIP_RANGES` holds, per animated track, 24 bytes: three components × `{ f32 minimum; f32 extent }`, where the components of a rotation are the `x, y, z` of the quaternion canonicalised to `w ≥ 0` ("Range reduction"). An extent is ≥ 0; an extent of 0 marks a **clip-constant component**, constant across the clip (common in an animated track: a rotation about one axis keeps two of x, y, z at 0). A clip-constant component of an animated track stores both segment-range bytes as 0 at whichever bit-rate index 0 … 17 its track has in a segment, stores 0 for each of its bit-stream values, and decodes to the minimum exactly ("Range reduction" step 3).

### Segment section

Segment s covers frames `[16 s, min(16 s + 16, F))`. Layout (all offsets from the section start):

| Part | Size | Content |
|---|---|---|
| header | 8 | `u32` frame count n (1 … 16); `u32` bit-stream bytes |
| bit-rate table | A + A_c, padded to 4 | one `u8` bit-rate index per animated track (A), then per animated curve (A_c) ("Bit-rate table") |
| segment ranges | 6 × A_q + 2 × A_cq, padded to 4 | for each animated track, in animated track index order, whose **bit-rate index** in this segment is 0 … 17 (A_q of them; a track at index 18 stores no range), three `{ u8 minimum q; u8 extent q }` pairs, one per component; then for each animated curve whose bit-rate index is 0 … 17 (A_cq of them), one pair ("Range reduction"). For bit-rate index 0 the extent byte is 0 and the minimum byte holds the segment's constant; for a clip-constant component (clip extent 0) both bytes are 0 at every index 0 … 17 ("Range reduction" step 3) |
| bit stream | as the header states, plus 8 zero bytes | frame-major: for frame 0 … n − 1, for each animated track in index order, its three components; then for each animated curve; each value `b` bits wide (b from the track's index), packed LSB-first into a little-endian bit sequence; a value at bit offset `o` is read with one unaligned 64-bit little-endian load of the bytes from `o / 8` and a shift of `o mod 8` |

Within a segment every value of a track has the same width, so the bit offset of track t in frame k is `k × W + P_t`, with W the frame stride in bits and `P_t` the prefix sum of the widths of the tracks before t; the kind loader computes `P_t` and W once per segment when it decodes the segment's tables and stores them in the clip object ("Zenith_AnimationClip header" of `Zenith2/Docs/Design/ANIM/Runtime.md`).

### Root motion section

`ANIM_SECTION_CLIP_ROOT_MOTION` holds F records of 28 bytes `{ f32 t[3]; f32 r[4] }`: the motion transform `Q_k` of frame k in the clip's root-motion space, `Q_0` the identity. Root motion is never quantised: an error of 0.2 mm per frame would accumulate over a loop. Sampling and blending are `Zenith2/Docs/Design/ANIM/Graph.md → "Root motion sampling"`.

### Event and marker sections

`ANIM_SECTION_CLIP_EVENTS`: records of 32 bytes, sorted by (time, name hash):

| Offset | Size | Field |
|---|---|---|
| 0 | 8 | name hash |
| 8 | 4 | time (s), 0 … D |
| 12 | 4 | duration (s): 0 for an instant notify; > 0 for a notify state (begin at time, end at time + duration, wrapping for a loop clip) |
| 16 | 4 | string offset of the name in the strings section |
| 20 | 2 | name length (1 … 127) |
| 22 | 1 | fire mode (`Zenith_EventFireMode`: 0 `WEIGHTED`, 1 `LEADER_ONLY`, 2 `ALWAYS`) |
| 23 | 1 | flags: bit 0 gameplay (delivered to simulation consumers), bit 1 audio, bit 2 VFX, bit 3 warp window (a notify state naming a motion-warp window) |
| 24 | 4 | minimum weight, [0, 1] (default 0.25) |
| 28 | 4 | integer payload (`i32`) |

`ANIM_SECTION_CLIP_MARKERS`: records of 16 bytes `{ u64 name hash; f32 time; u32 string offset }`, sorted by time; two markers at one time are refused. Markers drive marker synchronisation (`Zenith2/Docs/Design/ANIM/Graph.md → "Marker sync"`).

### Clip published properties

<!-- elaboration -->
Besides `anim.clipInfo` and `anim.rawClip` (`Zenith2/Docs/Design/ANIM.md → "Cook steps and derived-data keys"`), the `anim.clip` step publishes, through `Zenith_CookContext::PublishProperty`, the property `anim.clip.markers` (row CN27), so a dependent cook step reads a clip's markers and instant events with `ReadDependencyOutput(clip, "anim.clip.markers")` as a keyed cook-input edge without parsing ANIM's cooked format (CINE's `cine.sequence` step builds each dialogue face clip's lip-sync window table from it, `Zenith2/Docs/Design/CINE/Authoring.md → "cine.sequence step"`). Layout, little-endian, written field by field:

| Part | Layout |
|---|---|
| markers | `u32` marker count M (0 … `uANIM_MAX_CLIP_MARKERS`), then M records `{ u64 name hash; f32 time }` (12 bytes, no padding), ascending time: the records of `ANIM_SECTION_CLIP_MARKERS` without their string offsets |
| instant events | `u32` count E (0 … `uANIM_MAX_CLIP_EVENTS`), then E records `{ u64 name hash; f32 time; u32 payload }` (16 bytes), ascending (time, name hash): the records of `ANIM_SECTION_CLIP_EVENTS` whose duration is 0 (notify states are excluded), the payload the `i32` bit pattern |

The total is `8 + 12 M + 16 E` bytes (an empty clip publishes 8 bytes). Times are the cooked clip's times (after the clip range shift), so they equal what `Zenith_AnimationClip::GetMarkers` and `GetEvents` return at run time. The property is a pure function of the clip's events and markers, so editing an annotation re-runs exactly the dependent steps whose key reads it.

### Stream table section

`ANIM_SECTION_CLIP_STREAM_TABLE`: one record of 8 bytes `{ u32 first segment; u16 segment count; u16 reserved }` per chunk, in order, covering every segment exactly once; every chunk but the last has the info section's "segments per chunk". The cook streams a clip when its importer settings allow streaming and its total segment bytes exceed `anim.streaming.thresholdBytes` (default 262,144); the default chunk is 4 segments (64 frames, 2.13 s at 30 Hz).

### Clip validation

| Rule | Check | Failure |
|---|---|---|
| CV1 | D, rate and F consistent as "Clip info section" states; S = `ceil(F / 16)`; every segment present with n frames | `ANIMATION_ERROR_CORRUPT` |
| CV2 | joint and curve counts equal the bound skeleton's (checked at bind) | `ANIMATION_ERROR_SKELETON_MISMATCH` |
| CV3 | every bit-rate index ≤ 18; the segment-range part's size equals `6 × A_q + 2 × A_cq` padded to 4, with, per component, both bytes 0 when that component's clip extent is 0 (a clip-constant component, at any index 0 … 17), and otherwise the extent byte of an index 1 … 17 entry in [1, 255 − minimum] and of an index 0 entry 0; every bit-stream size equals `n × W / 8` rounded up | `ANIMATION_ERROR_CORRUPT` |
| CV4 | constants and ranges finite; extents ≥ 0 | `ANIMATION_ERROR_CORRUPT` |
| CV5 | events and markers sorted, times in [0, D], strings in bounds and valid UTF-8 | `ANIMATION_ERROR_CORRUPT` |
| CV6 | a loop clip's in-place last frame decodes to its first frame within ε (the cook enforces it, "Clip cook order"; a cook-side rule only: the loader does not re-check it, because the error bound it relies on is the cook's measurement) | cook detail `ANIM_COOK_DETAIL_LOOP_NOT_CLOSED` |
| CV7 | root motion records finite, rotations unit | `ANIMATION_ERROR_CORRUPT` |
| CV8 | the stream table covers every segment once | `ANIMATION_ERROR_CORRUPT` |

## Graph file

The graph asset (`Animation.Graph`, `.zagraph`, magic `ZAGR`) is a compiled animation graph: a flat node array in evaluation order, parameter, state machine, blend-space, layer, mask, slot, sync-group and IK-stage tables, the clip table and the per-instance memory layout. Its semantics are `Zenith2/Docs/Design/ANIM/Graph.md`; this heading fixes the bytes.

### Graph sections

| Kind | Name | Count | Record |
|---|---|---|---|
| `0x0301` | `ANIM_SECTION_GRAPH_INFO` | exactly one | "Graph info section" |
| `0x0302` | `ANIM_SECTION_GRAPH_NODES` | exactly one | "Node table section" |
| `0x0303` | `ANIM_SECTION_GRAPH_NODE_DATA` | exactly one | per-kind node payloads, 4-byte aligned, referenced by offset |
| `0x0304` | `ANIM_SECTION_GRAPH_PARAMETERS` | exactly one | "Parameter table section" |
| `0x0305` | `ANIM_SECTION_GRAPH_STATE_MACHINES` | zero or one | "State machine sections" |
| `0x0306` | `ANIM_SECTION_GRAPH_STATES` | zero or one | "State machine sections" |
| `0x0307` | `ANIM_SECTION_GRAPH_TRANSITIONS` | zero or one | "State machine sections" |
| `0x0308` | `ANIM_SECTION_GRAPH_CONDITIONS` | zero or one | "State machine sections" |
| `0x0309` | `ANIM_SECTION_GRAPH_BLEND_SPACES` | zero or one | "Blend space section" |
| `0x030A` | `ANIM_SECTION_GRAPH_MASKS` | zero or one | "Mask and layer sections" |
| `0x030B` | `ANIM_SECTION_GRAPH_LAYERS` | exactly one | "Mask and layer sections" |
| `0x030C` | `ANIM_SECTION_GRAPH_SYNC_GROUPS` | zero or one | `{ u64 name hash; u8 mode (0 normalised time, 1 markers); u8 reserved[3]; u32 reserved }` (16 bytes) |
| `0x030D` | `ANIM_SECTION_GRAPH_CLIPS` | exactly one | `{ Zenith_PersistentID clip (16); u32 flags (bit 0 retargeted from another skeleton); u32 reserved }` (24 bytes); every clip a hard dependency of the graph in the catalogue |
| `0x030E` | `ANIM_SECTION_GRAPH_SLOTS` | zero or one | `{ u64 name hash; u16 mask index (0xFFFF full body); u16 flags (bit 0 root motion allowed); f32 default blend in (s); f32 default blend out (s); u32 reserved }` (24 bytes) |
| `0x030F` | `ANIM_SECTION_GRAPH_IK_STAGE` | zero or one | "Mask and layer sections" |
| `0x0310` | `ANIM_SECTION_GRAPH_STRINGS` | exactly one | UTF-8 names of nodes, states, parameters and slots, for diagnostics and Luau |
| `0x0311` | `ANIM_SECTION_GRAPH_STABLE_IDS` | exactly one | the authoring `Zenith_PersistentID` of every node, state, transition and parameter, by table index (24 bytes: 16 ID, u8 table, u8 reserved, u16 reserved, u32 index) |
| `0x0312` | `ANIM_SECTION_GRAPH_MOTION_DATABASE` | zero or one | gated by D-SEL-MOTIONMATCH (open, Stage 3): `Zenith2/Docs/Design/ANIM/Graph.md → "Motion matching node"` |

### Graph info section

<!-- elaboration -->
| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 16 | skeleton GUID | the skeleton every clip of the graph is cooked for or retargeted to |
| 16 | 4 | node count | 1 … 4,096 |
| 20 | 4 | parameter count | 0 … 256 |
| 24 | 4 | state machine count | 0 … 256 |
| 28 | 4 | layer count | 1 … 8 |
| 32 | 4 | clip count | 0 … 1,024 |
| 36 | 2 | slot count | 0 … 8 |
| 38 | 2 | sync group count | 0 … 32 |
| 40 | 2 | mask count | 0 … 64 |
| 42 | 2 | pose stack depth | the compiler's computed maximum, 1 … 16 |
| 44 | 4 | instance memory bytes | the per-instance state block size, 16-byte multiple, at most 65,536 |
| 48 | 8 | graph hash | `Zenith_Hash64` of every other decoded section, in kind order (the hot-reload identity) |
| 56 | 8 | skeleton rest pose hash | as the clip's |

### Node table section

Node records are 32 bytes, in **evaluation order** (post-order: every child precedes its parent; the root of each layer is the last node of its subtree):

| Offset | Size | Field |
|---|---|---|
| 0 | 2 | kind (`Zenith_AnimNodeKind`, `Zenith2/Docs/Design/ANIM/Graph.md → "Node catalogue"`) |
| 2 | 2 | flags: bit 0 has sync group, bit 1 additive output, bit 2 fires events |
| 4 | 4 | payload offset in `ANIM_SECTION_GRAPH_NODE_DATA` |
| 8 | 4 | payload bytes |
| 12 | 4 | instance state offset in the per-instance block |
| 16 | 2 | child count (0 … 64) |
| 18 | 2 | reserved |
| 20 | 4 | child list offset in the node data (child count × `u16` node indices) |
| 24 | 4 | sync group index (`0xFFFFFFFF`: none) |
| 28 | 4 | name string offset |

A child index is smaller than its parent's index; the loader refuses any other.

### Parameter table section

Records of 32 bytes `{ u64 name hash; u8 kind (`Zenith_AnimParameterKind`: 1 FLOAT, 2 INT, 3 BOOL, 4 TRIGGER); u8 flags (bit 0 reserved engine parameter); u16 trigger hold ticks (1 … 255, TRIGGER only); u32 name string offset; f32 default; f32 minimum; f32 maximum; u32 reserved }`, sorted by name hash; an INT and a BOOL store their values as f32 (an INT is exact up to 2^24).

### State machine sections

| Section | Record | Fields |
|---|---|---|
| state machines | 24 bytes | `u16 first state; u16 state count; u16 entry state (machine-local); u16 any-state transition count; u32 first any-state transition; u32 instance state offset; u32 name string offset; u32 reserved` |
| states | 24 bytes | `u16 root node (the state's subtree root); u16 flags (bit 0 loop clips, bit 1 entry resets time); f32 speed; u32 first transition; u16 transition count; u16 reserved; u32 name string offset; u32 reserved` |
| transitions | 32 bytes | `u16 destination state (machine-local); u16 flags (bit 0 has exit time, bit 1 can target self, bit 2 sync destination time, bit 3 inertial); f32 duration (s, 0 … 60); f32 exit time (normalised, 0 … 1, or 0); u8 interrupt source (`Zenith_InterruptSource`: 0 none, 1 current, 2 next, 3 current then next, 4 next then current); u8 blend curve (`Zenith_BlendCurve`: 0 linear, 1 smoothstep); u16 condition count (0 … 8); u32 first condition; f32 destination start time (normalised); u32 name string offset; u32 reserved` |
| conditions | 16 bytes | `u16 parameter index (0xFFFF: none); u8 operation (`Zenith_ConditionOp`: 1 GREATER, 2 LESS, 3 EQUAL, 4 NOT_EQUAL, 5 TRUE, 6 FALSE, 7 TRIGGERED, 8 STATE_TIME_AT_LEAST, 9 CLIP_READY, 10 EVENT_FIRED); u8 reserved; f32 operand; u64 name hash (EVENT_FIRED: the event name; otherwise 0)` |

Transitions are stored in priority order (the authored order); a state's transitions are contiguous, and a machine's any-state transitions are contiguous and evaluated first ("Transition evaluation" of `Zenith2/Docs/Design/ANIM/Graph.md`).

### Blend space section

The section holds, in order: the blend-space headers, then every sample, then every triangle, then every hull edge.

| Record | Size | Fields |
|---|---|---|
| header | 24 | `u16 sample count (2 … 64); u16 triangle count (0 for 1D); u16 hull edge count (0 for 1D); u16 flags (bit 0 two-dimensional); u16 parameter X; u16 parameter Y (0xFFFF for 1D); u32 first sample; u32 first triangle; u32 first hull edge` |
| sample | 12 | `f32 x; f32 y (0 for 1D); u16 child slot; u16 reserved`; sorted by x for a 1D space |
| triangle | 8 | `u16 a; u16 b; u16 c; u16 reserved`, counter-clockwise sample indices |
| hull edge | 4 | `u16 a; u16 b`, counter-clockwise around the hull |

The triangulation is the compiler's ("Blend2D triangulation" of `Zenith2/Docs/Design/ANIM/Graph.md`).

### Mask and layer sections

`ANIM_SECTION_GRAPH_MASKS`: per mask, J bytes of joint weights (`u8`, weight = value / 255) then C bytes of curve weights, padded to 4; masks are compiled from mask documents ("Mask document schema"), so a mask is skeleton-specific.

`ANIM_SECTION_GRAPH_LAYERS`: per layer 24 bytes `{ u16 root node; u16 mask index (0xFFFF none); u8 blend mode (`Zenith_LayerBlendMode`: 0 override, 1 additive); u8 rotation space (0 local, 1 model); u16 weight parameter (0xFFFF: constant); f32 weight; u16 sync with layer (0xFFFF none); u16 flags; u32 name string offset; u32 reserved }`; layer 0 is the base layer, blend mode override, weight 1, no mask.

`ANIM_SECTION_GRAPH_IK_STAGE`: per IK step 32 bytes `{ u64 IK definition name hash (resolved against the skeleton at bind); u8 solver kind (`Zenith_IKKind`); u8 goal source (0 IK goal component slot, 1 foot placement, 2 look-at target); u8 goal slot (0 … 7); u8 flags (bit 0 apply rotation, bit 1 soft); u16 weight parameter (0xFFFF constant); u16 reserved; f32 weight; f32 softness (m); u32 reserved[2] }`, applied in record order after the layers ("IK stage order" of `Zenith2/Docs/Design/ANIM/Runtime.md`).

### Graph validation

| Rule | Check | Failure |
|---|---|---|
| GV1 | counts within "Graph info section" limits; every offset and child index in bounds; children precede parents | `ANIMATION_ERROR_CORRUPT` |
| GV2 | each node payload has the size its kind defines | `ANIMATION_ERROR_CORRUPT` |
| GV3 | every parameter index in a node, condition or layer names a parameter of the kind the user needs | `ANIMATION_ERROR_CORRUPT` |
| GV4 | instance offsets of distinct nodes do not overlap and lie inside the instance block | `ANIMATION_ERROR_CORRUPT` |
| GV5 | the pose stack depth the loader recomputes equals the stored one | `ANIMATION_ERROR_CORRUPT` |
| GV6 | the skeleton GUID and rest pose hash match every clip's at bind | `ANIMATION_ERROR_SKELETON_MISMATCH` |

## Bit-rate table

A bit-rate index selects the width of every component of one track (or one curve) within one segment:

| Index | Bits per component | Range reduction | Decode |
|---|---|---|---|
| 0 | 0 | none | the component is the segment's constant `m + (qMin / 255) × e`, from its minimum byte (a value constant within the segment; m exactly for a clip-constant component, whose bytes are 0) |
| 1 … 14 | 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 | segment | `Dequantise` below |
| 15 | 18 | segment | `Dequantise` |
| 16 | 20 | segment | `Dequantise` |
| 17 | 23 | segment | `Dequantise` |
| 18 | 32 | none | the raw f32 value |

`uANIM_BIT_RATE_COUNT = 19`; the table is `auANIM_BIT_RATE_BITS` in `Internal/Sampling/Zenith_AnimBitRates.h` and is part of the format (a change is a format version bump).

<!-- elaboration -->
Index 0 still stores its two segment-range bytes (the constant's minimum byte, and an extent byte of 0), because a component constant within a segment is rarely constant across the clip; like every other rate it is chosen only when its decoded value meets the error target ("Bit-rate optimisation").

## Compression algorithm

The `anim.clip` cook step compresses every clip with the mode its settings select: `raw` (Stage 3: every animated track at index 18) or `quantised` (Stage 4, the default from then). The algorithm is deterministic: the same intermediate, settings and step version give byte-identical output on every machine (`Test_ClipCompression_Deterministic`).

### Track classification

<!-- elaboration -->
Channels are classified one at a time in (joint ascending, rotation, translation, scale) order, over the F resampled frames, and the classification is **cumulative**: each test runs with every channel classified before it replaced by its classified value (the rest value or its constant) and every channel not yet classified kept raw. For the channel under test:

1. `DEFAULT` when replacing it by the rest value (for an additive clip, the identity) passes the test below.
2. Otherwise `CONSTANT` when replacing it by frame 0's value passes; the stored constant is frame 0's value.
3. Otherwise `ANIMATED` (it stays raw for the tests that follow).

The test requires the hierarchical error E ("Error metric") of every joint to stay ≤ ε / 2 at every frame. Because each test includes every earlier replacement, the **combined** error of all classified channels is ≤ ε / 2 at every joint and frame when classification ends (errors of classified channels on one chain cannot add up past the half-threshold), so quantisation always has the other half, and the all-raw encoding of the animated tracks meets the bound up to its own f32 rounding. Curves are classified the same way with `|value − candidate| ≤ ε_c / 2` (curves are independent, so cumulation does not arise). O(channels × F × D) for subtree size D.

### Range reduction

1. Rotations are canonicalised per frame: `q := −q` when `w < 0`, so `w ≥ 0` and only `x, y, z` are stored; the decoder reconstructs `w = sqrt(max(0, 1 − x² − y² − z²))` and then `Zenith_Normalize`.
2. Clip level: per animated track component, `minimum = min over frames`, `extent = max − min`, stored in f32 (`ANIM_SECTION_CLIP_RANGES`); an extent below 1e-30 is stored as 0 (the component is then clip-constant at its minimum, which moves no value by more than 1e-30, below every error target), so every non-zero extent e gives `e / 255` a normal f32 and `segExt > 0` below.
3. Segment level, evaluated in `double` from the stored f32 clip range (m, e) and the segment's own minimum `a` and maximum `b` of the double-precision resampled values, so the bytes are identical on every machine. **Clip-constant component (`e = 0`)**, at any bit-rate index 0 … 17: both bytes are 0, nothing is divided by e or by a segment extent, every bit-stream value of the component is 0 (step 4 is not evaluated for it), and the decoder needs no special case, because its formulas give m exactly (`m + (0 / 255) × 0` for index 0, `Dequantise(0, b, m, 0) = m` for indices 1 … 17). Otherwise (`e > 0`), for bit-rate indices 1 … 17: `qMin = floor(255 × (a − m) / e)` clamped to [0, 254] and `qExt = ceil(255 × (b − m) / e) − qMin` clamped to [1, 255 − qMin] (never empty, because `qMin ≤ 254`), giving the segment range `segMin = m + (qMin / 255) × e`, `segExt = (qExt / 255) × e`, which contains [a, b] by construction (a segment at the clip maximum, `a = b = m + e`, gets `qMin = 254`, `qExt = 1`). Bit-rate index 0 (a component constant within the segment, `a = b`): `qMin = round_half_even(255 × (a − m) / e)` clamped to [0, 255], extent byte 0, decoding to `m + (qMin / 255) × e`, which differs from `a` by up to `e / 510`; whether that meets the error target is tested like any other rate.
4. Quantisation at width b (1 ≤ b ≤ 23) of a component with `e > 0` (so `segExt ≥ e / 255 > 0`): `q = round_half_even(((v − segMin) / segExt) × (2^b − 1))` clamped to [0, 2^b − 1], evaluated in `double` with `segMin` and `segExt` the f32 values the decoder computes from the bytes.

`Dequantise(q, b, segMin, segExt) = segMin + (float(q) / float(2^b − 1)) × segExt`, evaluated in f32 in exactly that order; integer to float conversions are exact for b ≤ 23. The CPU decoder of the runtime and the cook's verifier share this function (`Internal/Sampling/Zenith_AnimQuantise.h`), so the cook measures the error the runtime will produce.

### Error metric

The error of a joint j at a frame is measured in the clip's model space through the whole hierarchy, the virtual-vertex measure:

```
E(j, k) = max over v ∈ { (d_j, 0, 0), (0, d_j, 0), (0, 0, d_j) } of | M̃_j(k) · v − M_j(k) · v |
	M_j(k)  = model transform of joint j at frame k from the double-precision resampled tracks (after root-motion extraction, loop closure and additive conversion, never rounded to f32)
	M̃_j(k) = the same from the candidate (decoded) tracks, every ancestor's candidate included
	d_j     = the joint's shell distance (ANIM_SECTION_SKEL_SHELL)
```

<!-- elaboration -->
**Shell distances** follow one deterministic rule, `d_j = max(r_j, g_j)`:

- `r_j` is the skeleton's own distance, cooked by `anim.skeleton` into `ANIM_SECTION_SKEL_SHELL`: the rig document's `shellDistances` value for the joint, else its `default`, else `max(0.03 m, the joint's longest child bone)`;
- `g_j` is the largest `skinShellDistance[j]` over the meshes the rig document lists in `shellMeshes` (0 when it lists none): GEOM's skinned-mesh cook publishes, per mesh, `skinShellDistance` as J f32 values indexed by skeleton joint (the largest bind-pose distance from the joint of a vertex weighted to it, 0 for a joint no vertex is weighted to; row AN4).

The `anim.clip` step alone reads `g_j`: it reads the skeleton's published `shellMeshes` ("Skeleton published properties") and, for each listed mesh, `ReadDependencyOutput(mesh, "skinShellDistance")`, which records each as a cook-input edge; `anim.skeleton` never reads a mesh. There is no cook cycle: GEOM's skinned-mesh cook reads `anim.skeleton`'s properties, `anim.clip` reads both, and nothing reads `anim.clip`'s output back. A mesh not listed in `shellMeshes` never affects a clip, so a clip's key is a function of named inputs only, and editing a listed mesh re-cooks the skeleton's clips. The skeleton's runtime `GetShellDistances` returns `r_j`.

The threshold ε is the clip's `anim.compression.errorMetres` (default 0.0002 m; range 1e-5 … 0.01 m; the cinematic preset 0.00005 m). The measure is evaluated in `double`.

**Additive clips.** An additive clip's tracks are deltas, and the displacement a delta error causes depends on the pose it is applied to, so for an additive clip `M_j(k)` and `M̃_j(k)` are the model transforms of the delta applied at weight 1 to the clip's **additive reference pose** P ("Additive clip cooking"), in the clip's additive space, from the raw delta tracks and from the candidate tracks respectively; `M_j(k)` is therefore the source frame the delta was computed from. The bound holds relative to P: applied to another base pose, the displacement of the same delta error is bounded by the same rotation and translation errors times that base's levers, which the cook does not measure. Classification, the repair loop and verification use this measure unchanged.

### Bit-rate optimisation

Per segment, independently and in ascending segment order:

<!-- elaboration -->
The **error target** of a joint-frame is `τ(j, k) = max(ε, E_raw(j, k))`, where `E_raw` is the error of the clip with every channel (classified ones included) stored raw at index 18, that is rounded once to f32, measured against the double-precision `M_j(k)` of "Error metric": the f32 rounding floor of the format, which no encoding can beat. So a clip the raw encoding represents can always be cooked.

1. Initial rates: for each animated track in index order, index 0 when every component's values in the segment are equal and the index-0 decoding of the track alone (every other track raw, classified channels at their classified values) gives `E(j', k) ≤ τ(j', k)` for every joint j' in the track's subtree and every frame k of the segment; otherwise the smallest index r ≥ 1 that does.
2. Repair loop: compute `E(j, k)` for every joint and frame of the segment with every track at its current rate. The **open set** starts as every joint. While some joint of the open set has `E(j, k) > τ(j, k)` at some frame:
	1. take the worst open joint j* (largest max-over-frames excess `E − τ`; ties to the lower joint index);
	2. the candidates are the animated tracks of j* and of each of its ancestors whose index is below 18;
	3. when there is a candidate: for each, evaluate the max-over-frames excess of j* with that candidate's index raised by one, and raise the candidate with the smallest resulting excess; ties go to the deeper joint, then rotation before translation before scale;
	4. when there is no candidate (every animated track of the chain is raw) and the chain has a classified (`DEFAULT` or `CONSTANT`) channel: demote the most recently classified one of the chain (the last in classification order) to `ANIMATED`, pinned at index 18 in every segment of the clip (a demoted channel is never given a lower rate and, being at 18, is never a candidate), and **restart the optimisation from segment 0**: steps 1 and 2 run again for every segment with the new animated-track set, because the demotion changes the errors of segments already final (a descendant's error is not monotone in an ancestor's precision when errors partially cancelled), so no segment keeps rates chosen against the old set; the combined classification bound of "Track classification" makes this branch unreachable except through f32 rounding, but it is defined;
	5. when there is neither, remove j* from the open set: its error is the raw encoding's, which is its target by definition.
3. Termination: a **pass** runs steps 1 and 2 over every segment in ascending order and ends early at a demotion. Within a pass each iteration of a segment raises one index (at most `18 × A` per segment, A the pass's animated-track count) or removes one joint from the open set (at most J per segment). Demotions are at most `3 × J` per clip (each demotes a distinct classified channel, which is never classified again), so there are at most `1 + 3 × J` passes, and the optimisation ends after at most `(1 + 3 × J) × S × (18 × A + J + 1)` iterations, with A ≤ 3 × J the final animated-track count and S the segment count, with every joint-frame of every segment within its target in the last pass.

Complexity per segment: the initial pass is O(A × R × n × D) and each repair iteration O(h × n × h) for chain height h ≤ 32 and n ≤ 16 frames, where D is the subtree size; a 60-joint, 1-second clip compresses in under 50 ms on the reference desktop (measured by `Zenith_Bench_ClipCompression`, a report, not a budget).

### Curve compression

Each animated curve is optimised alone per segment: the smallest index whose decoded values all satisfy `|v̂ − v| ≤ ε_c` (default `anim.compression.curveError` = 0.001, range 1e-5 … 0.05), index 0 when constant in the segment. Curves never raise joint rates.

### Error bound verification

After optimisation the cook decodes the whole clip through the runtime decoder function and recomputes `E(j, k)` for every joint and frame and the curve error for every curve and frame; it writes the maxima into the info section. A joint-frame above its target `τ(j, k)` is a defect of the optimiser, and the step fails with `ANIM_COOK_DETAIL_ERROR_BOUND_VIOLATED` naming the clip, the joint and the frame (never silently shipped); the curve check uses ε_c. The bound is verified **at frames only**: between frames the runtime interpolates decoded frames (linear translation and scale, normalised linear rotation, then forward kinematics), and the error of that interpolation against the source is not bounded by the frame error, so the design claims nothing between frames; the raw encoding interpolates the same way. The cook report member `animation.compression` lists per clip the raw bytes, the cooked bytes, the ratio, ε and the measured maximum.

## Skeleton intermediate

CONTENT's importers write the blob `anim.skeleton` for each `skel:<name>` product by calling ANIM's `Zenith_WriteSkeletonIntermediate` (`Tools/ZenithCook/Internal/Animation/Zenith_AnimImport.h`, "ZenithCook ANIM files" of `Zenith2/Docs/Design/ANIM.md`):

<!-- elaboration -->
| Part | Layout |
|---|---|
| header | magic `ZISK` (u32), version u16 (1), flags u16 (0), joint count u32, skin count u32 |
| joints | per joint: name length u16, UTF-8 name, parent u16 (`0xFFFF` root), rest local `t`, `r`, `s` (40 bytes), has-inverse-bind u8, source node index u32 |
| skins | per skin: name length u16, UTF-8 name, joint count u32, then per joint `{ u16 skeleton joint; f32 inverse bind 3 × 4 row-major }` (50 bytes) |
| morph names | name count u32, then per name: length u16, UTF-8 name (the morph-target names of every mesh bound to the skeleton, deduplicated, sorted by byte order) |

The rest local transforms are already the rest pose of "Rest pose from inverse bind matrices"; the skins are kept so the step re-verifies rule SV9 and so the cook report can name the conflicting skin.

## Clip intermediate

The blob `anim.clip` for each `anim:<name>` product, written by ZenithCook's `Zenith_WriteClipIntermediates` for glTF sources and, for curve-and-pose sources without glTF node tables (CHAR's lip-sync and pose-book importers, row CH15), by ZenithAuthoring's `Zenith_WriteCurveClipIntermediate` (`Zenith2/Docs/Design/ANIM.md → "ZenithAuthoring ANIM files"`), which writes the same layout with flag bit 0 clear and interpolation 1 or 2 only:

<!-- elaboration -->
| Part | Layout |
|---|---|
| header | magic `ZICL` (u32), version u16 (1), flags u16 (bit 0 retarget spec present), skeleton product GUID (16), source duration f32, channel count u32 |
| channels | per channel: target joint name length u16 + UTF-8 name (for a weights channel the curve name), path u8 (`Zenith_ImportedPath`: 1 translation, 2 rotation, 3 scale, 4 curve), interpolation u8 (1 step, 2 linear, 3 cubic spline), key count u32, times f32 × N (strictly increasing, ≥ 0), values: f32 × N × width (width 3, 4, 3, 1), and for cubic spline the in- and out-tangents, f32 × N × width each |
| retarget spec | present when flag bit 0: source clip GUID (16), target skeleton GUID (16), settings length u32 + the retarget document's canonical JSON |

A clip intermediate never holds resampled data: the step resamples ("Clip resampling") so the sample rate is a cook setting, and changing it re-cooks without re-importing.

## Import conversion

The conversion runs in ANIM's helpers, which CONTENT's glTF importer calls with neutral structs after it has parsed the file (fastgltf, ZenithCook's dependency) and converted axes, handedness and units (CONTENT's conventions, `Zenith2/Docs/Requirements/Inventory.md → "Inventory v1"`, the CONTENT row). glTF 2.0 already matches CORE's coordinate system, so a glTF needs no axis change.

```cpp
// Zenith2/Tools/ZenithCook/Internal/Animation/Zenith_AnimImport.h (tool-side; ASSET's tool-header conventions apply)
enum Zenith_ImportedPath : uint8 { IMPORTED_PATH_TRANSLATION = 1, IMPORTED_PATH_ROTATION, IMPORTED_PATH_SCALE, IMPORTED_PATH_WEIGHTS };
enum Zenith_ImportedInterpolation : uint8 { IMPORTED_INTERPOLATION_STEP = 1, IMPORTED_INTERPOLATION_LINEAR, IMPORTED_INTERPOLATION_CUBIC_SPLINE };

struct Zenith_ImportedNode
{
	Zenith_StringView m_strName;
	uint32 m_uParent = 0xFFFFFFFFu;          // node index; 0xFFFFFFFF for a scene root
	Zenith_Transform m_xLocal;                // the node-local transform, already in engine conventions
};

struct Zenith_ImportedSkin
{
	Zenith_StringView m_strName;
	Zenith_Span<const uint32> m_auJoints;               // node indices
	Zenith_Span<const Zenith_Matrix4> m_axInverseBind;   // one per joint, in the skin's (model) space
	uint32 m_uSkeletonRoot = 0xFFFFFFFFu;               // the skin's skeleton node; 0xFFFFFFFF: none given
};

struct Zenith_ImportedChannel
{
	uint32 m_uNode = 0;
	Zenith_ImportedPath m_ePath = IMPORTED_PATH_TRANSLATION;
	Zenith_ImportedInterpolation m_eInterpolation = IMPORTED_INTERPOLATION_LINEAR;
	Zenith_Span<const float> m_afTimes;
	Zenith_Span<const float> m_afValues;                 // N × width, or 3 × N × width for cubic spline (in-tangent, value, out-tangent)
	uint32 m_uMorphTarget = 0;                          // WEIGHTS: the first target index the values start at
};

struct Zenith_ImportedAnimation
{
	Zenith_StringView m_strName;
	Zenith_Span<const Zenith_ImportedChannel> m_axChannels;
};

enum Zenith_AnimationImportErrorCode : uint8
{
	ANIM_IMPORT_ERROR_INVALID_INPUT = 1,     // a span of the wrong size, a node index out of range, a non-finite value
	ANIM_IMPORT_ERROR_SKELETON,              // the detail names the rule (Zenith_AnimationCookDetail)
	ANIM_IMPORT_ERROR_CLIP,
	ANIM_IMPORT_ERROR_IMPORT_CONTEXT,        // DeclareProduct or WriteIntermediate failed; the detail is its Zenith_ImportError code
};

struct Zenith_AnimationImportError          // 8 bytes, the struct form of [Z2-N-CONTRACT-480]
{
	Zenith_AnimationImportErrorCode m_eCode;
	uint8 m_uReserved = 0;
	uint16 m_uDetail = 0;
	uint32 m_uIndex = 0;                    // the joint, skin, channel or node index the error names
};

struct Zenith_SkeletonImportResult           // filled by Zenith_BuildSkeletons, read by Zenith_WriteClipIntermediates
{
	uint32 m_uSkeletonCount = 0;
	Zenith_AssetGUID m_axSkeletons[64];      // product GUIDs, in group order; at most 64 skeletons per source
	uint16 m_auNodeToJoint[65536];          // node index → joint index in its skeleton, 0xFFFF when not a joint
	uint8 m_auNodeToSkeleton[65536];        // node index → skeleton slot, 0xFF when not a joint
};

Zenith_Result<void, Zenith_AnimationImportError> Zenith_BuildSkeletons(Zenith_Span<const Zenith_ImportedNode> axNodes,
	Zenith_Span<const Zenith_ImportedSkin> axSkins, Zenith_Span<const Zenith_StringView> astrMorphNames,
	Zenith_ImportContext& xContext, Zenith_SkeletonImportResult& xOut);
Zenith_Result<void, Zenith_AnimationImportError> Zenith_WriteClipIntermediates(
	Zenith_Span<const Zenith_ImportedNode> axNodes, Zenith_Span<const Zenith_ImportedAnimation> axAnimations,
	const Zenith_SkeletonImportResult& xSkeletons, Zenith_ImportContext& xContext);
```

A source with more than 65,536 nodes or 64 skeletons fails with `ANIM_IMPORT_ERROR_INVALID_INPUT` (both limits are the import helpers' row of "Parser limits"); the result structure is large and lives in the importer's scratch arena, never on a stack.

`Zenith_BuildSkeletons` declares one `skel:<root joint name>` product per skeleton and writes its intermediate; `Zenith_WriteClipIntermediates` declares one `anim:<animation name>` product per (animation, skeleton) pair the animation touches (`anim:<name>@<root>` when an animation drives two skeletons) through `Zenith_ImportContext::DeclareProduct` (`Zenith2/Docs/Design/ASSET/Import.md → "Zenith_ImportContext"`), with the identity rules of `Zenith2/Docs/Design/ASSET/Database.md → "Sub-asset identity"`.

### Product fingerprints and rename recognition

<!-- assumption: pending ASSET (CONTENT row CT22: `SetProductFingerprint`, the `fingerprint` member and `GetPreviousProducts` returning it) -->
ANIM's helpers record a fingerprint for every `skel:` and `anim:` product and recognise renames with CONTENT's rule (CONTENT row CT25 (c); `Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Product fingerprints"`, "Recognition steps"). A fingerprint is the first 8 bytes of `Zenith_Sha256Digest`, read big-endian as a `uint64` (written as 16 lowercase hex digits by ASSET), over a tag and an identity description, every integer little-endian:

| Kind prefix | Tag | Identity description | Survives |
|---|---|---|---|
| `skel:` | `ZFPS` | joint count J (`u32`), then the J joint names sorted by byte order, each as `u16` length + UTF-8 bytes | a rename of the root joint or of the source file; a re-parenting that keeps the joint set |
| `anim:` | `ZFPC` | channel count (`u32`) of the product's intermediate, then the source duration as its IEEE-754 `f32` bit pattern (`u32`) | a rename of the animation; key edits that keep the channel set and the duration |

Recognition runs in each helper before it declares its products, per prefix, on the previous entries `GetPreviousProducts()` returns, with steps 2 to 5 of CONTENT's "Recognition steps": P = the previous keys of the prefix that are live, not excluded and not produced again by this import; N = this import's keys of the prefix that the `.meta` does not hold; when |P| = |N| = 1 and the fingerprints are equal the helper calls `RenameProduct(old, new)` before `DeclareProduct(new, …)` and reports `ANIM_COOK_WARNING_RENAME_ASSUMED`; when |P| = |N| = 1 and they differ, or the old entry has none, no rename (`ANIM_COOK_WARNING_RENAME_NO_EVIDENCE`); when either set is larger than 1, no rename (`ANIM_COOK_WARNING_RENAME_AMBIGUOUS`, both sets listed). Every declared product then gets `SetProductFingerprint(guid, fingerprint)`. A two-skeleton clip key `anim:<animation>@<root>` is in the `anim:` set like any other. Recognition depends only on the previous entries and the new keys and fingerprints, so a re-import is deterministic.
<!-- /assumption -->

### Skeleton extraction from a glTF skin

1. Group skins whose joint sets intersect; each group is one skeleton, so two meshes skinned to one rig share it (a modular character).
2. The root is the skin's `skeleton` node when every skin of the group names the same node and it is an ancestor of every joint; otherwise the lowest common ancestor of every joint of the group. A group with no common ancestor below the scene root gets a synthetic root joint `Root` with the identity rest transform.
3. The joint set is every joint of the group plus every node on the paths from them to the root; a node on a path that is not a skin joint is a joint with no inverse bind matrix.
4. Order: depth-first pre-order from the root, children in ascending node index.
5. Names: the node names, which must be 1 … 63 bytes and unique within the skeleton (`ANIM_IMPORT_DETAIL_DUPLICATE_JOINT_NAME` otherwise; an unnamed node is named `Node<index>`).

Complexity O(N log N) in the node count; a 1,024-joint skeleton builds in microseconds.

### Rest pose from inverse bind matrices

The rest pose is the bind pose. For a joint with an inverse bind matrix the rest model matrix is `M_j = inverse(IBM_j)` in the space the inverse bind matrices are expressed in, which is the skeleton's model space. Every skin of the group binding joint j must agree: two inverse bind matrices `A`, `B` agree when `max |A_rc − B_rc| ≤ 1e-5 × max(1, max |A_rc|)` (the rest-pose tolerance, `fANIM_REST_POSE_TOLERANCE`); otherwise `ANIM_IMPORT_DETAIL_REST_POSE_CONFLICT` names both skins and the joint, and the import fails. **No step resolves the conflict by re-posing or rebinding a mesh**: the remedy is an authoring fix (a shared rig in one rest pose), because a pose change baked through skin weights distorts the mesh in a way no weighting removes.

A joint without an inverse bind matrix takes `M_j = M_c · inverse(TRS(N_c))` from its first child c (in joint order) that has a rest model matrix, where `N_c` is the child's node-local transform; every other such child must agree within the tolerance (`ANIM_IMPORT_DETAIL_REST_POSE_CONFLICT`); a joint with no such descendant (an end node) takes `M_parent · TRS(N_j)`. The rest local transform is `L_j = Zenith_DecomposeTRS(inverse(M_parent(j)) · M_j)` (`Zenith2/Docs/Design/CORE/Maths.md → "Matrix types"`), computed in `double`; a shear or a scale below 1e-4 fails with `ANIM_IMPORT_DETAIL_REST_POSE_SHEAR`.

### Clip cook order

<!-- elaboration -->
The `anim.clip` step runs its stages in this order, each reading the previous one's output:

1. **Resample** every channel ("Clip resampling"); the result, with the clip's resolved cook settings (root-motion mode and motion joint, loop flag, additive settings: reference kind, frame, reference clip GUID and space; compression settings: ε, ε_c and mode; streaming eligibility), is the published `anim.rawClip`.
2. **Extract root motion** from the unclosed resampled frames ("Root motion extraction at cook"), so the last frame of a looping walk keeps its displacement: `Q_{F−1} = V_0⁻¹ · V_{F−1}` is the per-loop motion and `Q(D)` of "Root motion sampling" (`Zenith2/Docs/Design/ANIM/Graph.md`) is one loop's travel.
3. **Close the loop** (loop clips only) on the in-place pose: measure the hierarchical error between in-place frame F − 1 and frame 0; when it exceeds 10 × ε report `ANIM_COOK_WARNING_LOOP_DISCONTINUITY` with the error; in every case write frame 0's in-place values into frame F − 1 of every joint and curve track (never into the root-motion track), so the decoded loop is closed (CV6).
4. **Additive** conversion when configured ("Additive clip cooking").
5. **Classify and compress** ("Compression algorithm"), then verify.

Closing the loop before extraction would overwrite the motion joint's last frame with frame 0 and make `Q_{F−1}` the identity, so a looping walk would publish no motion per loop; `Test_CookClip_LoopRootMotionKeepsPerLoopMotion` guards the order.

### Clip import settings

<!-- elaboration -->
Every key the `anim.clip` step reads through `GetSettings` is one row of this table. The table is published as data in `Zenith2/Tools/ZenithCook/Internal/Animation/Zenith_AnimImport.h`: `inline constexpr Zenith_ImporterSettingDesc axANIM_CLIP_IMPORT_SETTINGS[]` (`Zenith2/Docs/Design/ASSET/Import.md` → "Importer registry"), in this row order. An importer that produces `anim:` products (CONTENT's `gltf`, A3 cross-area row CT11) appends these descs to its own `m_axSettings` verbatim as source-wide defaults; it adds no row and changes no default. One clip's own values live in its clip annotation document, never in this table.

| Key | Type | Default | Range or values | Read by |
|---|---|---|---|---|
| `anim.sampleRateHz` | INT | `30` | 1 … 240 | "Clip resampling" |
| `anim.clipRange` | FLOAT_ARRAY (2) | `[]` (the whole clip) | `[start, end]` in source seconds, start < end | "Clip resampling" step 1 |
| `anim.loop` | BOOL | `false` | — | "Clip cook order" stage 3 |
| `anim.rootMotion.mode` | ENUM | `"none"` | `none`, `xzYaw`, `xyzYaw`, `full` (info byte 54 values 0 … 3) | "Root motion extraction at cook" |
| `anim.rootMotion.joint` | STRING | `""` (the default joint rule) | a joint name of the clip's skeleton | the same |
| `anim.additive.reference` | ENUM | `"none"` | `none`, `firstFrame`, `rest`, `clip` | "Additive clip cooking" |
| `anim.additive.frame` | INT | `0` | 0 … 65,535; the frame of `firstFrame` or `clip` | the same |
| `anim.additive.clip` | STRING | `""` | a 36-character clip GUID; required for `clip` | the same |
| `anim.additive.space` | ENUM | `"local"` | `local`, `model` (info byte 53) | the same |
| `anim.compression.mode` | ENUM | `"quantised"` | `raw`, `quantised` (info byte 55) | "Error metric" |
| `anim.compression.errorMetres` | FLOAT | `0.0002` | 1e-5 … 0.01 | the same |
| `anim.compression.curveError` | FLOAT | `0.001` | 1e-5 … 0.05 | the same |
| `anim.streaming.allowed` | BOOL | `true` | — | "Clip sections" (`ANIM_SECTION_CLIP_STREAM_TABLE`) |

No row is platform-overridable. A value outside its range or values is refused by ASSET's settings validation (`IMPORT_ERROR_SETTING_INVALID`), and the clip is not cooked. `Test_AnimClipSettings_TableMatchesStep` asserts that the step's `GetSettings` calls name exactly these keys, in this order, with these types and defaults.

### Clip resampling

The `anim.clip` step resamples every channel at the clip's sample rate (setting `anim.sampleRateHz`, default 30):

1. D = the largest last key time of the channels that drive the skeleton (or the setting `anim.clipRange` when given, `[start, end]` in source seconds, shifted to start at 0).
2. F and the frame times as "Clip info section" states.
3. Each channel is evaluated at each frame time exactly as glTF 2.0 defines its sampler: `STEP` holds the previous key; `LINEAR` interpolates translations, scales and weights linearly and rotations by spherical linear interpolation of the shortest arc; `CUBICSPLINE` evaluates the cubic Hermite spline with the stored tangents scaled by the key interval, then normalises a rotation. Before the first key the first value holds; after the last key the last value holds. Evaluation is in `double`.
4. A joint with no channel takes its rest value for that path (so the track classifies `DEFAULT`).
5. Loop clips (setting `anim.loop`, or a clip annotation's `loop`) are closed after root-motion extraction, not here ("Clip cook order" stage 3).
6. Morph-weight channels become curve tracks of the skeleton curve whose name is the mesh's target name; weights are clamped to [−1, 1].

### Root motion extraction at cook

With root motion mode m ≠ none and motion joint R (setting `anim.rootMotion.joint`, default joint 0 when its track is animated, else the `JOINT_ROLE_HIPS` joint):

1. Every ancestor of R must have `DEFAULT` or `CONSTANT` tracks; otherwise the step fails with `ANIM_COOK_DETAIL_ROOT_MOTION_ANCESTOR_ANIMATED`, because motion extracted below an animated ancestor has no single meaning.
2. `G_k` = R's model transform at frame k, with position `p_k` and rotation `q_k`; `M_R` is R's rest model transform with rotation `q_rest`.
3. The virtual root `V_k`: modes 1 and 2, `V_k = T(π(p_k)) · Yaw(q_k · q_rest⁻¹)`, where π keeps (x, 0, z) in mode 1 and (x, y, z) in mode 2 and `Yaw(q)` is the twist of q about +Y (swing-twist decomposition: the normalised `(0, q_y, 0, q_w)`, the identity when `|(q_y, q_w)| < 1e-6`); mode 3, `V_k = G_k · M_R⁻¹`.
4. The motion transform `Q_k = V_0⁻¹ · V_k` (so `Q_0` is the identity), written raw to `ANIM_SECTION_CLIP_ROOT_MOTION`.
5. The in-place pose: `G'_k = V_0 · V_k⁻¹ · G_k`, written back into R's local track (`L'_R(k) = M_parent(R)⁻¹ · G'_k`, the parent's model transform being constant by step 1), so every frame's pose stands where frame 0's virtual root put it, and the motion lives in Q alone.

Evaluation is in `double`; a mode-1 clip whose virtual root climbs keeps the climb in its pose, a mode-2 clip moves it into Q. Extraction runs on the unclosed frames ("Clip cook order"), so for a loop clip `Q_{F−1}` is one loop's motion and the in-place frame F − 1 is then closed onto frame 0.

### Additive clip cooking

With `anim.additive.reference` set: the reference pose P is the clip's own in-place frame (`firstFrame`, frame index), the rest pose, or another clip's frame (`clip`: GUID and frame, read through `ReadDependencyOutput(clip, "anim.rawClip")`, to which the cook applies that clip's own root-motion extraction, steps 2 to 5 of "Root motion extraction at cook" with the mode and motion joint `anim.rawClip` carries, so P is in place like the frames it is subtracted from). Per joint, local space: `r_d = r_P⁻¹ · r`, `t_d = t − t_P`, `s_d = s ⊘ s_P` (component-wise); model space (`anim.additive.space = "model"`): the same with model transforms, `r_d = r · r_P⁻¹` (a rotation applied about the joint's own origin), `t_d = t − t_P`. The delta tracks are classified and compressed with identity as the default value.

### Retargeted clip cooking

A retarget document ("Retarget document schema") declares one product `anim:<source clip name>@<target skeleton name>` per listed clip; its intermediate carries the retarget spec. The step reads the source clip's published `anim.rawClip` (the resampled raw tracks before root-motion extraction and loop closure), both skeletons' properties, builds the retarget map of `Zenith2/Docs/Design/ANIM/Runtime.md → "Retarget map construction"` and applies it frame by frame ("Runtime retarget application", which scales the root translation by the map's root scale), then runs stages 2 to 5 of "Clip cook order" for the target skeleton: root motion is re-extracted from the retargeted frames with the source's root-motion mode and the target's motion joint (the target joint of the source motion joint's role, else the target's joint 0), so Q carries the scaled translation and the rotation unchanged. The product's cook settings are the source's resolved settings that `anim.rawClip` carries (its loop flag for stage 3, its additive settings for stage 4, its compression settings for stage 5, its streaming eligibility), each member overridden by the product's own importer settings in the retarget document's `.meta` (sub-asset key `anim:<source clip name>@<target skeleton name>`, the members a clip annotation document overrides: `loop`, `additive`, `compression`, `streaming`); an additive reference of another clip is that clip's `anim.rawClip` frame retargeted through the same map and extracted with the target's motion joint, and a rest-pose reference is the target's rest pose. The cooked clip carries the target skeleton's GUID and rest pose hash and the retargeted flag. Retargeting changes only rotations (and the root translation scale); it never touches a mesh.

## Authoring documents

ANIM's authoring documents are JSON, canonical (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical rules"`), versioned and migrated ([Z2-N-CONTRACT-552], Z2-D-048); their types are registered tool-side ("Migration and re-cook" of `Zenith2/Docs/Design/ANIM.md`).

### Document type table

| Stable name | Schema tag | Suffix | First version | Stage | Read by |
|---|---|---|---|---|---|
| `Zenith.AnimationGraphDocument` | `zenith2.animgraph` | `.animgraph.json` | 1 | S3 | the `anim.graph` importer and step; the editor |
| `Zenith.AnimationMaskDocument` | `zenith2.animmask` | `.animmask.json` | 1 | S3 | the graph compiler (masks are compiled into graphs) |
| `Zenith.AnimationRigDocument` | `zenith2.animrig` | `.animrig.json` | 1 | S3 (roles, sockets, shell distances, curves), S4 (chains, IK definitions, mirror) | the `anim.skeleton` step |
| `Zenith.AnimationClipDocument` | `zenith2.animclip` | `.animclip.json` | 1 | S3 | the `anim.clip` step (events, markers, loop, root motion, additive, compression, streaming) |
| `Zenith.AnimationRetargetDocument` | `zenith2.animretarget` | `.animretarget.json` | 1 | S4 | the `anim.retarget` importer and the `anim.clip` step |

A clip or rig document is attached to its product by the product's importer settings in the source's `.meta` (`"annotation": "<document GUID>"` under the `anim:<name>` sub-asset, `"rig": "<document GUID>"` under `skel:<name>`), and the step opens it with `Zenith_CookContext::OpenSourceDocument` (`Zenith2/Docs/Design/ASSET/Cook.md → "Source documents in cook steps"`); importer settings give every clip of a source its defaults and the clip document overrides them per clip.

### Graph document schema

```json
{
	"$schema": "zenith2.animgraph",
	"schemaVersion": 1,
	"id": "6f1c2a40-7d3e-4b51-9a22-0c4e8f7b1d93",
	"skeleton": "1b7e9f02-44c1-4d0a-8f3e-92aa5c61e0b4",
	"parameters": {
		"0a4f…": { "name": "Speed", "kind": "Float", "default": 0.0, "min": 0.0, "max": 8.0 },
		"5c31…": { "name": "Jump", "kind": "Trigger", "holdTicks": 2 }
	},
	"layers": [
		{ "id": "9d2e…", "name": "Base", "root": "3b7a…", "mode": "Override", "weight": 1.0 },
		{ "id": "e140…", "name": "UpperBody", "root": "77c0…", "mode": "Override", "weight": 1.0, "weightParameter": "c8a1…", "mask": "a4d2…", "rotationSpace": "Model" }
	],
	"nodes": {
		"3b7a…": { "kind": "StateMachine", "name": "Locomotion", "entry": "41f0…",
			"states": { "41f0…": { "name": "Idle", "root": "b9e2…", "editor": { "x": 0, "y": 0, "collapsed": false } },
				"8a6c…": { "name": "Move", "root": "02dd…", "editor": { "x": 220, "y": 0, "collapsed": false } } },
			"transitions": [
				{ "id": "d5e9…", "from": "41f0…", "to": "8a6c…", "duration": 0.2, "blend": "Inertial",
					"conditions": [ { "parameter": "0a4f…", "op": "Greater", "value": 0.1 } ] },
				{ "id": "f71b…", "from": "Any", "to": "41f0…", "duration": 0.15, "interrupt": "Next",
					"conditions": [ { "parameter": "5c31…", "op": "Triggered" } ] }
			] },
		"b9e2…": { "kind": "Clip", "clip": "c0de…", "loop": true, "editor": { "x": 120, "y": 160, "collapsed": true } },
		"02dd…": { "kind": "Blend1D", "parameter": "0a4f…", "syncGroup": "77aa…",
			"samples": [ { "value": 1.5, "node": "1a1a…" }, { "value": 4.0, "node": "2b2b…" }, { "value": 7.0, "node": "3c3c…" } ] }
	},
	"syncGroups": { "77aa…": { "name": "Feet", "mode": "Markers" } },
	"slots": { "ab01…": { "name": "FullBody", "blendIn": 0.2, "blendOut": 0.25, "rootMotion": true } },
	"ik": [ { "definition": "LeftFoot", "source": "FootPlacement", "weight": 1.0 } ],
	"editorComments": [ { "x": 80, "y": -60, "w": 320, "h": 40, "text": "Locomotion: idle and move" } ]
}
```

(IDs are abbreviated here; documents hold the 36-character form.) Keys: `parameters`, `nodes`, `syncGroups` and `slots` are persistent-ID maps (canonical rule C4); `layers`, `transitions`, `samples`, `conditions` and `ik` are ordered arrays (C8), because their order is data (layer order, transition priority, sample order); every node and every state may carry the optional known member `editor` `{ "x": number, "y": number, "collapsed": boolean }` (canvas position in canvas units and the collapsed flag), and the graph the optional known top-level member `editorComments`, an ordered array (C8) of `{ "x", "y", "w", "h": number, "text": string }` canvas comments; both are part of the canonical schema, so they round-trip canonically and raise no unknown-field warning, and the compiler ignores them (they change neither the compiled sections nor the graph hash, though the document's SHA-256 in the cook key changes, as for any edit). EDIT's canvas writes them (`Zenith2/Docs/Design/EDIT/AuthoringTools.md → "Animation tools"`, row ED17). The schema version stays 1: no graph document predates this member set. A node's kind-specific members are the payload columns of `Zenith2/Docs/Design/ANIM/Graph.md → "Node catalogue"`. Values: `kind` ∈ {`Float`, `Int`, `Bool`, `Trigger`}; `mode` ∈ {`Override`, `Additive`}; `rotationSpace` ∈ {`Local`, `Model`}; `blend` ∈ {`Linear`, `Smoothstep`, `Inertial`}; `interrupt` ∈ {`None`, `Current`, `Next`, `CurrentThenNext`, `NextThenCurrent`}; `op` ∈ {`Greater`, `Less`, `Equal`, `NotEqual`, `True`, `False`, `Triggered`, `StateTimeAtLeast`, `ClipReady`, `EventFired`}.

### Mask document schema

```json
{
	"$schema": "zenith2.animmask", "schemaVersion": 1, "id": "a4d2…", "skeleton": "1b7e…",
	"default": 0.0,
	"entries": [ { "joint": "spine_01", "weight": 1.0, "recursive": true }, { "joint": "neck_01", "weight": 0.5, "recursive": false } ],
	"curves": [ { "curve": "jawOpen", "weight": 1.0 } ]
}
```

Entries apply in order: a recursive entry sets the joint and every descendant, and an entry overrides every entry before it; weights are in [0, 1] and quantise to `round(weight × 255)`. An unknown joint or curve name is a compile error naming the mask, the entry index and the name.

### Rig document schema

| Member | Type | Meaning |
|---|---|---|
| `skeleton` | persistent ID | the skeleton product it annotates |
| `roles` | keyed map role name → joint name | the humanoid profile ("Rig sections") |
| `chains` | ordered array of `{ "name", "start", "end", "role", "retargetByIK" }` | retarget and IK chains |
| `ik` | ordered array of `{ "name", "kind", "joints", "hint" }` | IK definitions; `hint` a 3-vector |
| `sockets` | keyed map name → `{ "joint", "position", "rotation" }` | attachment points |
| `mirror` | `{ "axis": "X", "pairs": [["hand_l", "hand_r"], …] }` | the mirror table; unlisted joints mirror onto themselves |
| `shellDistances` | `{ "default": 0.1, "joints": { name: metres } }` | the error metric's distances `r_j` ("Error metric") |
| `shellMeshes` | ordered array of persistent IDs of skinned-mesh products | the meshes whose published `skinShellDistance` raises the clip step's distances (`g_j`, "Error metric"); absent or empty: none |
| `curves` | keyed map name → `{ "kind", "default", "additive" }` | curves beyond the meshes' morph targets |

### Clip annotation document schema

| Member | Type | Meaning |
|---|---|---|
| `clip` | persistent ID | the `anim:` product |
| `range` | `[start, end]` seconds or absent | the source range |
| `loop` | bool | loop clip |
| `rootMotion` | `{ "mode": "XZYaw", "joint": "pelvis" }` | "Root motion extraction at cook" |
| `additive` | `{ "reference": "FirstFrame", "frame": 0, "clip": null, "space": "Local" }` | "Additive clip cooking" |
| `compression` | `{ "errorMetres": 0.0002, "curveError": 0.001, "mode": "Quantised" }` | per-clip overrides |
| `streaming` | `{ "allowed": true }` | eligibility for streaming |
| `poseLibrary` | bool | every frame is a named pose; `markers` name the frames |
| `events` | persistent-ID map → `{ "name", "time", "duration", "fireMode", "minWeight", "flags": [...], "payload" }` | notifies and notify states |
| `markers` | persistent-ID map → `{ "name", "time" }` | sync markers |
| `curves` | keyed map curve name → `{ "keys": [[time, value], …] }` | authored curve keys (linear), overriding imported curves of that name |

### Retarget document schema

| Member | Type | Meaning |
|---|---|---|
| `sourceSkeleton`, `targetSkeleton` | persistent IDs | the two rigs; both must carry a complete humanoid profile, or every target joint must be mapped explicitly |
| `chainMap` | keyed map target chain name → source chain name | overrides of the role-derived mapping |
| `jointMap` | keyed map target joint name → source joint name or `null` | per-joint overrides; `null` keeps the target rest rotation |
| `rootTranslation` | `"ScaleByHipsHeight"`, `"Copy"` or `"None"` | the root translation rule |
| `clips` | ordered array of source clip product GUIDs | one product per entry |

## Parser limits

<!-- elaboration -->
The constants live in `Zenith_AnimationTypes.h`; each parser tests each at the limit and at limit + 1 ([Z2-N-ARCH-953]); the rows are the "animation assets" row of `Zenith2/Docs/Architecture.md → "Parser limits"` (cross-area row AN2, cross-doc item X210 (2)):

| Parser | Quantity | Limit |
|---|---|---|
| skeleton loader | joints, curves, sockets, chains, IK definitions | 1,024; 512; 256; 64; 32 |
| skeleton loader | name length | 63 bytes |
| clip loader | frames, segments, events, markers | 432,001; 27,001; 4,096; 4,096 (the frame and segment limits are implied by the duration and rate limits of "Clip info section": `3,600 s × 120 Hz + 1` frames, `ceil(432,001 / 16)` segments) |
| clip loader | event name length | 127 bytes |
| clip loader | duration | 3,600 s |
| graph loader | nodes, parameters, state machines, states per machine, transitions per machine, conditions per transition | 4,096; 256; 256; 256; 2,048; 8 |
| graph loader | layers, slots, masks, sync groups, clips, blend samples | 8; 8; 64; 32; 1,024; 64 |
| graph loader | instance memory, pose stack | 65,536 bytes; 16 |
| intermediate readers | channels per clip, keys per channel | 65,536; 16,777,216 |
| import helpers | nodes per source, skeletons per source | 65,536; 64 |
| document readers | document bytes | 64 MiB (SCENE's open limit) |
