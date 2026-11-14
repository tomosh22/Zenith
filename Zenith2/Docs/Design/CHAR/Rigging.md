# CHAR design: rig profiles, rig and retarget authoring, controls and pose books

Section file of `Zenith2/Docs/Design/CHAR.md`. It specifies how a character's rig is authored on top of ANIM's skeleton and rig document: the rig profile table and the deterministic profile matcher, mirror and chain detection, rest-pose classification, the generation of `.animrig.json` content through undoable edits, the facial rig template, the default ragdoll document, retarget setup against a shared library rig, control sets (the posing controls of a character) and pose books (the authored expressions and visemes cooked into ANIM pose-library clips). Every format ANIM or PHYS owns is cited, never restated: the rig document is `Zenith2/Docs/Design/ANIM/Formats.md → "Rig document schema"`, the retarget document `Zenith2/Docs/Design/ANIM/Formats.md → "Retarget document schema"`, the ragdoll document `Zenith2/Docs/Design/PHYS/Formats.md → "Ragdoll document"`.

## Scope of this file

| Topic | Heading |
|---|---|
| what a rig profile is and the profiles CHAR ships | "Rig profile table" |
| name normalisation and tokens | "Joint name normalisation" |
| matching a skeleton against a profile | "Profile matching algorithm" |
| the generic, token-driven profile | "Generic profile assignment" |
| checking matches against the hierarchy | "Topology verification" |
| left and right pairs | "Mirror pair detection" |
| retarget and IK chains | "Chain and IK template detection" |
| T-pose, A-pose, facing and up | "Rest pose classification" |
| writing the rig document | "Rig document generation" |
| face roles and curves | "Facial rig template" |
| a default ragdoll | "Default ragdoll generation" |
| one rest pose per shared rig | "Shared library rig rule" |
| the retarget document | "Retarget setup" |
| control sets | "Control set document" and "Control evaluation" |
| pose books | "Pose book document", "Pose book cook" and "Pose capture and mirroring" |
| limits | "Rigging limits" |

## Rig profile table

<!-- elaboration -->
A rig profile is a constexpr table (the profile IDs are `Zenith_CharRigProfileID` of `Zenith2/Docs/Design/CHAR.md → "Constants and enumerations"`: `ZENITH_HUMANOID`, the engine's own names, the role names in lower snake case; `MIXAMO`, the `mixamorig:` namespace and `LeftUpLeg`/`LeftLeg`/`LeftFoot` style; `MANNEQUIN_STYLE`, `pelvis`, `spine_01`, `clavicle_l`, `upperarm_l`, `thigh_l`, `calf_l`, `ball_l`; `RIGIFY_DEFORM`, `DEF-spine`, `DEF-upper_arm.L`, `DEF-thigh.L`; `GENERIC`, token-driven) mapping each `Zenith_JointRole` (`Zenith2/Docs/Design/ANIM/Formats.md → "Rig sections"`, values 1 to 54, plus CHAR's face roles 55 to 62 of "Facial rig template") to the normalised joint names a DCC convention uses for it. Profiles live in `Zenith2/Tools/Authoring/Internal/CHAR/Zenith_CharRigProfiles.cpp` as `constexpr Zenith_CharRigProfileDesc` arrays; they are code, reviewed in diffs, and never read from a file, so the matcher has no input but the skeleton.

```cpp
// Zenith2/Tools/Authoring/Internal/CHAR/Zenith_CharRigProfiles.h (internal)
#pragma once
#include "Authoring/Zenith_AuthoringCharacter.h"   // Zenith_CharRigProfileID, uCHAR_RIG_PROFILE_COUNT ("Constants and enumerations")

struct Zenith_CharRoleNames
{
	Zenith_JointRole m_eRole;
	Zenith_Span<const char* const> m_aszNames;            // normalised names, most specific first
};

struct Zenith_CharRigProfileDesc
{
	Zenith_CharRigProfileID m_eID;
	const char* m_szName;                                 // "Mixamo"
	Zenith_Span<const Zenith_CharRoleNames> m_axRoles;    // ascending role; empty for CHAR_RIG_PROFILE_GENERIC
};

Zenith_Span<const Zenith_CharRigProfileDesc> Zenith_CharGetRigProfiles();   // ascending ID
```

The exact-name profiles (normalised as "Joint name normalisation" states; `_l`/`_r`, `left`/`right` written as shown):

| Role | `ZENITH_HUMANOID` | `MIXAMO` | `MANNEQUIN_STYLE` | `RIGIFY_DEFORM` |
|---|---|---|---|---|
| `HIPS` | `hips` | `hips` | `pelvis` | `def_spine` |
| `SPINE` | `spine` | `spine` | `spine_01` | `def_spine_001` |
| `CHEST` | `chest` | `spine1` | `spine_02` | `def_spine_002` |
| `UPPER_CHEST` | `upper_chest` | `spine2` | `spine_03` | `def_spine_003` |
| `NECK` | `neck` | `neck` | `neck_01` | `def_spine_004` |
| `HEAD` | `head` | `head` | `head` | `def_spine_006`, `def_spine_005` |
| `LEFT_EYE`, `RIGHT_EYE` | `eye_l`, `eye_r` | `lefteye`, `righteye` | `eye_l`, `eye_r` | `def_eye_l`, `def_eye_r` |
| `LEFT_SHOULDER` | `shoulder_l` | `leftshoulder` | `clavicle_l` | `def_shoulder_l` |
| `LEFT_UPPER_ARM` | `upper_arm_l` | `leftarm` | `upperarm_l` | `def_upper_arm_l` |
| `LEFT_LOWER_ARM` | `lower_arm_l` | `leftforearm` | `lowerarm_l` | `def_forearm_l` |
| `LEFT_HAND` | `hand_l` | `lefthand` | `hand_l` | `def_hand_l` |
| `LEFT_UPPER_LEG` | `upper_leg_l` | `leftupleg` | `thigh_l` | `def_thigh_l` |
| `LEFT_LOWER_LEG` | `lower_leg_l` | `leftleg` | `calf_l` | `def_shin_l` |
| `LEFT_FOOT` | `foot_l` | `leftfoot` | `foot_l` | `def_foot_l` |
| `LEFT_TOES` | `toes_l` | `lefttoebase` | `ball_l` | `def_toe_l` |
| `LEFT_THUMB_1` … `_3` | `thumb_1_l` … `thumb_3_l` | `lefthandthumb1` … `3` | `thumb_01_l` … `thumb_03_l` | `def_thumb_01_l` … `03_l` |
| `LEFT_INDEX_1` … `_3` | `index_1_l` … | `lefthandindex1` … | `index_01_l` … | `def_f_index_01_l` … |
| `LEFT_MIDDLE_1` … `_3` | `middle_1_l` … | `lefthandmiddle1` … | `middle_01_l` … | `def_f_middle_01_l` … |
| `LEFT_RING_1` … `_3` | `ring_1_l` … | `lefthandring1` … | `ring_01_l` … | `def_f_ring_01_l` … |
| `LEFT_LITTLE_1` … `_3` | `little_1_l` … | `lefthandpinky1` … | `pinky_01_l` … | `def_f_pinky_01_l` … |
| right-side roles | the same with `_r` | `right…` | `_r` | `_r` |
| face roles 55–62 | `jaw`, `tongue`, `brow_l`, `brow_r`, `lid_upper_l`, `lid_upper_r`, `lid_lower_l`, `lid_lower_r` | none | `jaw`, none | `def_jaw`, none |

A profile's role row may list several names; the first that exists in the skeleton is taken. A name appears in at most one role row of a profile (a static assertion in `Zenith_CharRigProfiles.cpp` checks the tables, and `Test_CharRigProfile_TablesWellFormed` asserts ascending roles, unique names and normalised spellings).

## Joint name normalisation

`Zenith_CharNormaliseJointName(Zenith_StringView strName, uint32 uJointIndex, Zenith_Array<char>& xOut)` (declared in `Zenith2/Docs/Design/CHAR.md → "Rig authoring API"`; `uJointIndex` names the joint in step 4), applied to every joint name before any comparison:

1. Drop everything up to and including the last `:` or `|` (DCC namespaces: `mixamorig:Hips` → `Hips`).
2. ASCII letters to lower case; every byte that is not an ASCII letter or digit becomes `_` (so `DEF-upper_arm.L` → `def_upper_arm_l`, `Spine 01` → `spine_01`); non-ASCII bytes (UTF-8 sequences) become `_` byte by byte.
3. Collapse runs of `_` to one; trim leading and trailing `_`.
4. An empty result is the name `joint_<index>`.

Tokens (for the generic profile and mirror detection) are computed on the original name before step 2: split at non-alphanumeric bytes, then at each lower-to-upper case boundary and each letter-digit boundary (`LeftUpLeg` → `Left`, `Up`, `Leg`; `spine01` → `spine`, `01`), then lower-cased; a token of digits is its integer value. Both functions are O(length). `Test_CharRigNames_Normalise` asserts the four examples above and an all-UTF-8 name.

## Profile matching algorithm

`Zenith_CharMatchRigProfile(const Zenith_CharSkeletonView& xSkeleton, Zenith_CharRigProfileID eProfile, Zenith_CharProfileMatch& xOut)` (declared in `Zenith2/Docs/Design/CHAR.md → "Rig authoring API"`):

1. Normalise every joint name (J names, O(Σ length)). Build a sorted array of (normalised name, joint) pairs; a normalised name shared by two joints is reported (`CHAR_DIAG_RIG_NAME_COLLISION`, naming both) and neither joint is matched by name.
2. For `CHAR_RIG_PROFILE_GENERIC` run "Generic profile assignment" and go to step 4.
3. For each role row in ascending role order, for each listed name in order, binary-search the name; the first hit assigns the role to that joint, unless the joint already holds a role (then `CHAR_DIAG_RIG_JOINT_TWO_ROLES` and the role met second stays unassigned).
4. Run "Topology verification"; it may unassign roles.
5. Fill `xOut`: the role → joint array (`uANIM_INVALID_INDEX` unassigned), the score `assigned required roles / 17` (the seventeen roles marked required in ANIM's "Rig sections" table: hips, spine, chest, neck and head; upper arm, lower arm and hand on each side; upper leg, lower leg and foot on each side), the count of all assigned roles, the diagnostics in the order they arose.

`Zenith_CharDetectRigProfile` runs step 1 once and steps 2 to 5 for every profile in ascending ID, and returns the best: the highest score, then the most assigned roles, then the lowest ID; a best score below 1.0 is returned with `m_bHumanoidComplete` false and the diagnostic `CHAR_DIAG_RIG_PROFILE_INCOMPLETE` listing the unassigned required roles. Complexity O(P × (R × n log J) + J) for P profiles, R roles and n names per role. `Test_CharRigProfile_DetectsMixamo`, `Test_CharRigProfile_DetectsMannequinStyle`, `Test_CharRigProfile_DetectsRigify`, `Test_CharRigProfile_DetectsZenithHumanoid` assert that each generated fixture skeleton (`Zenith_CharTestFixtures.h`, "Fixtures and generators" of `Zenith2/Docs/Design/CHAR/Tests.md`) is matched completely by its own profile and by no other profile with a higher score; `Test_CharRigProfile_TieBreaksByAssignedThenID` asserts the order.

## Generic profile assignment

<!-- elaboration -->
The generic profile assigns roles from tokens and topology when no exact-name profile is complete. Each step reads only the tokens, the hierarchy and the rest model positions `p_j` (the translation of `restModel_j`), and every choice has a deterministic tie-break (lowest joint index).

| Step | Assignment |
|---|---|
| 1 side | a joint's side is LEFT when a token is `l`, `left`, `lt` or `lft`, or its first token starts with `left`; RIGHT for `r`, `right`, `rt`, `rgt`, or a first token starting with `right`; else NONE |
| 2 hips | the joint with a token in {`hips`, `hip`, `pelvis`} and the largest subtree; none: the lowest-index joint with at least three children each having a subtree of at least three joints |
| 3 legs | for each side, among descendants of hips with that side, the chain of four joints along the hierarchy whose tokens contain, in order, {`thigh`, `upleg`, `upperleg`, `leg`} → UPPER_LEG, {`calf`, `shin`, `lowerleg`, `leg`, `knee`} → LOWER_LEG, {`foot`, `ankle`} → FOOT, {`toe`, `toes`, `toebase`, `ball`} → TOES; a keyword used by an ancestor of the chain is not reused by a descendant (so `leg` is UPPER_LEG only when no `thigh`/`upleg` exists above it) |
| 4 head | the joint with a token `head` nearest hips by hierarchy depth; none: the leaf of the longest side-NONE descendant path of hips that does not enter a leg |
| 5 spine column | the hierarchy path hips → head; NECK = the path joint with token `neck` nearest head, else head's parent; the remaining joints strictly between hips and NECK are the spine joints s_1 … s_k in path order: SPINE = s_1, CHEST = s_k when k ≤ 2 else s_{k−1}, UPPER_CHEST = s_k when k ≥ 3 |
| 6 arms | for each side, the descendant chain of the spine column with that side whose tokens contain {`clavicle`, `shoulder`, `collar`} → SHOULDER (optional), {`upperarm`, `arm`, `upper`} → UPPER_ARM, {`forearm`, `lowerarm`, `elbow`} → LOWER_ARM, {`hand`, `wrist`} → HAND |
| 7 fingers | each child chain of HAND whose first joint's tokens contain {`thumb`}, {`index`, `pointer`}, {`middle`, `mid`}, {`ring`}, {`little`, `pinky`, `small`}, in chain order → phalanges 1, 2, 3 (a chain of 4 drops its last joint, the tip) |
| 8 eyes | children of HEAD with a token `eye` and a side |

Joints left without a role keep none. The generic profile never assigns a face role (face roles come from the facial template). `Test_CharRigGeneric_AssignsUnnamedHumanoid` asserts a fixture whose names follow none of the exact profiles (`Bone_Thigh_Left` style) is assigned completely; `Test_CharRigGeneric_KeywordNotReusedDownChain` asserts step 3's rule on a `Leg`/`Leg`/`Foot` chain.

## Topology verification

After assignment, each role pair of this table must hold, or the child role of the pair is unassigned with `CHAR_DIAG_RIG_TOPOLOGY` naming both roles and joints:

| Role | Must be a strict descendant of |
|---|---|
| SPINE, LEFT_UPPER_LEG, RIGHT_UPPER_LEG | HIPS |
| CHEST | SPINE |
| UPPER_CHEST | CHEST |
| NECK | UPPER_CHEST when assigned, else CHEST |
| HEAD | NECK |
| LEFT_EYE, RIGHT_EYE, JAW, TONGUE (as descendant of JAW when assigned), BROW and LID roles | HEAD |
| `<side>`_SHOULDER | the highest assigned spine role |
| `<side>`_UPPER_ARM | `<side>`_SHOULDER when assigned, else the highest assigned spine role |
| `<side>`_LOWER_ARM, `<side>`_HAND, `<side>`_LOWER_LEG, `<side>`_FOOT, `<side>`_TOES | the previous segment of the same limb |
| finger phalanx 1, 2, 3 | `<side>`_HAND, phalanx 1, phalanx 2 |

Verification also checks sides geometrically: every assigned LEFT role's rest position has `x ≥ −0.01 m` relative to HIPS's x and every RIGHT role's `x ≤ 0.01 m` when the rest pose faces +Z ("Rest pose classification"); a violation is `CHAR_DIAG_RIG_SIDE_SWAPPED` and swaps nothing (a facing problem is reported there, not repaired here). Complexity O(R × depth). `Test_CharRigTopology_UnassignsNonDescendant` and `Test_CharRigTopology_ReportsSwappedSides` assert both rules.

## Mirror pair detection

`Zenith_CharDetectMirrorPairs(const Zenith_CharSkeletonView&, Zenith_Span<uint32> auMirrorOut, Zenith_Array<Zenith_CharDiagnostic>&)`:

1. For each joint j with a side (step 1 of "Generic profile assignment"), build its counterpart name by replacing the side token with the opposite side's token of the same form (`l`↔`r`, `left`↔`right`, `lt`↔`rt`, `lft`↔`rgt`, first-token prefix `left…`↔`right…`, preserving case of the original bytes), and look the exact original name up.
2. A found counterpart k is accepted when `|p_j − M(p_k)| ≤ max(0.01 m, 0.02 × maxReach)` where `M` reflects through the plane `x = p_hips.x` (axis X, the model left-right axis of CORE's conventions, `Zenith2/Docs/Design/CORE/Maths.md → "Coordinate systems"`); otherwise `CHAR_DIAG_MIRROR_ASYMMETRIC` naming both and the distance, and j is left unpaired.
3. Pairs must be symmetric (j → k and k → j); a joint claimed by two others is unpaired and reported (`CHAR_DIAG_MIRROR_AMBIGUOUS`).
4. Every unpaired joint maps to itself.

The output is the rig document's `mirror` member: `{ "axis": "X", "pairs": [[left name, right name], …] }`, pairs in ascending joint index of the left joint. ANIM's cook verifies `mirror(mirror(j)) = j` (`Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton validation"`, SV7); the detector never emits a table that fails it. `Test_CharMirror_DetectsAndChecksPositions` and `Test_CharMirror_AmbiguousLeftUnpaired` cover it.

## Chain and IK template detection

From an assigned profile, CHAR writes these chains (rig document `chains`, `Zenith2/Docs/Design/ANIM/Formats.md → "Rig document schema"`) and IK definitions (`ik`), each only when every joint it names is assigned:

| Name | Kind | Joints | Hint or flags |
|---|---|---|---|
| `Spine` | chain `SPINE` | SPINE → highest spine role | retargetByIK false |
| `Neck` | chain `NECK` | NECK → HEAD | false |
| `LeftArm`, `RightArm` | chain `LEFT_ARM`, `RIGHT_ARM` | UPPER_ARM → HAND | false |
| `LeftLeg`, `RightLeg` | chain `LEFT_LEG`, `RIGHT_LEG` | UPPER_LEG → FOOT | true |
| `LeftLegIK`, `RightLegIK` | IK `TWO_BONE` | UPPER_LEG, LOWER_LEG, FOOT | hint = the rest model-space direction from the hip-ankle line's closest point to the knee, normalised; when shorter than 1 mm (a straight leg), (0, 0, 1), the model front |
| `LeftArmIK`, `RightArmIK` | IK `TWO_BONE` | UPPER_ARM, LOWER_ARM, HAND | hint as above at the elbow; straight arm: (0, 0, −1) |
| `LeftFoot`, `RightFoot` | IK `FOOT` | FOOT, TOES, the leg's two-bone definition index | hint `y` = the ankle's rest height above the lower of the two toes' rest heights, clamped to [0.02, 0.2] m |
| `LookAt` | IK `LOOK_AT` | up to four of UPPER_CHEST (or CHEST), NECK, HEAD, in that order | hint = (0, 0, 1) expressed in HEAD's local frame: `inverse(R_head_rest) · (0, 0, 1)` |

The order of the `chains` and `ik` arrays is the order of this table, so the generated document is canonical and stable. `Test_CharChains_GeneratedForCompleteProfile` asserts the arrays for the Mixamo fixture and `Test_CharChains_StraightLegHintIsFront` the fallback.

## Rest pose classification

`Zenith_CharClassifyRestPose(const Zenith_CharSkeletonView& xSkeleton, const Zenith_CharProfileMatch& xMatch, const float afHeightRangeMetres[2], Zenith_CharRestPoseReport& xOut)` (declared in `Zenith2/Docs/Design/CHAR.md → "Rig authoring API"`; `afHeightRangeMetres` is the class's `heightRangeMetres`) measures, from rest model positions, in binary64 with CORE's deterministic functions (an angle of a vector against the horizontal is `Zenith_Atan2(v.y, Zenith_Sqrt(v.x² + v.z²))`, the angle between unit vectors a and b is `Zenith_Atan2(|a × b|, a · b)`; "Estimator input" of `Zenith2/Docs/Design/CHAR/Facial.md` states the rule):

| Measure | Definition | Classes |
|---|---|---|
| arm elevation e (per side) | the angle of `p_lower_arm − p_upper_arm` above the horizontal plane, degrees | `T_POSE` when both `|e| ≤ 15°`; `A_POSE` when both e ∈ [−60°, −30°]; `OTHER` otherwise |
| up axis | the angle between `p_head − p_hips` and +Y | correct when ≤ 15°; otherwise `CHAR_DIAG_REST_UP_AXIS` with the measured angle and the hint `transform.sourceUp` |
| facing | `f = sign((p_left_toes − p_left_foot).z + (p_right_toes − p_right_foot).z)` and `s = sign(p_left_upper_arm.x − p_right_upper_arm.x)` | `f > 0, s > 0`: faces +Z (correct); `f < 0, s < 0`: faces backwards, `CHAR_DIAG_REST_FACING_BACKWARDS` with the hint `transform.sourceFront = -z`; `f > 0, s < 0` or `f < 0, s > 0`: mirrored, `CHAR_DIAG_REST_MIRRORED` (a handedness or naming error, never repaired) |
| height | `p_head.y − min(p_left_toes.y, p_right_toes.y)` plus the head joint's shell distance (the rig document's `shellDistances`, default 0.1 m) | reported in metres; outside `afHeightRangeMetres` (the class range of the character project document, "Character project settings document" of `Zenith2/Docs/Design/CHAR/Facial.md`) is `CHAR_DIAG_REST_HEIGHT` with the hint `transform.unitScale` |
| scale | every rest scale within 1e-3 of 1 | otherwise `CHAR_DIAG_REST_SCALED_JOINTS` listing them (a rest scale is legal for ANIM, but a retarget library rig of unit scale is the rule of "Shared library rig rule") |

<!-- informative -->
The reference engine's imported characters read as facing backwards because front-face winding and facing were never checked against the skeleton (the glTF winding lesson). The facing measure uses the feet and arms, which a mesh's winding cannot change, so CONTENT's bounds-and-facing report (`Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Bounds and facing report"`) and this classification check two independent things.
<!-- /informative -->

Classification never changes a transform. `Test_CharRestPose_ClassifiesTAndA`, `Test_CharRestPose_DetectsBackwardsFacing`, `Test_CharRestPose_DetectsMirrored`, `Test_CharRestPose_ReportsUnitError` assert each row on generated fixtures (a fixture is a fixture skeleton rotated 180° about +Y, one with X negated, one scaled by 100).

## Rig document generation

`Zenith_CharBuildRigDocumentEdits(const Zenith_CharRigBuildInputs&, Zenith_CharRigEditList& xOut, Zenith_Heap&)` turns the detection results into the members of an `.animrig.json` document, written by the editor command `character.rig.applyProfile` as JSON edits (`Zenith2/Docs/Design/EDIT/Foundation.md → "JSON edits"`) so the change is one undo record:

| Member | Written value | When the document already has it |
|---|---|---|
| `skeleton` | the skeleton product GUID | kept; a different GUID fails the command (`CHAR_ERROR_SKELETON_MISMATCH`) |
| `roles` | every assigned role, by ANIM's role names → joint names | replaced role by role: a role present in the document with another joint is kept and listed as `CHAR_DIAG_RIG_ROLE_KEPT` unless the command's `overwrite` argument is true |
| `chains`, `ik` | "Chain and IK template detection" | replaced as whole arrays only with `overwrite`; otherwise entries whose `name` is absent are appended in table order |
| `mirror` | "Mirror pair detection" | replaced with `overwrite`; otherwise kept |
| `shellDistances` | `{ "default": 0.1 }` when absent | kept |
| `shellMeshes` | the GUIDs of the skinned-mesh products of the same source whose skin section names this skeleton, in ascending GUID | the union, ascending |
| `curves` | the facial template's curves when the command's `facial` argument is true ("Facial rig template") | merged by name; an existing curve keeps its kind and default |
| `sockets` | none generated, except `Head.LookTarget` (HEAD, position (0, 0, 0.3), identity) when the profile has HEAD and the document has no sockets | kept |

The function is pure and returns the edit list in member order; the command applies it as one transaction, with the preview of the diff shown before Apply (the rig profile panel, `Zenith2/Docs/Design/CHAR/Workflow.md → "Rig profile panel"`). Applying the same detection twice produces no edit (`Test_CharRigDocument_ApplyTwiceIsNoOp`). `Test_CharRigDocument_KeepsAuthoredRoleUnlessOverwrite` and `Test_CharRigDocument_ShellMeshesFromSource` cover the merge rules; `Test_CharRigDocument_CookAcceptsGenerated` cooks a generated document through ANIM's `anim.skeleton` step with ASSET's cook test access (`Zenith_CookTestAccess.h`, ruling GEOM-N2) and asserts `IsHumanoidComplete()` on the loaded skeleton.

## Facial rig template

<!-- assumption: pending ANIM (row CH1) -->
CHAR adds eight face roles to `Zenith_JointRole`, at the free values ANIM reserves for them (`Zenith2/Docs/Design/ANIM/Formats.md → "Rig sections"`: "values from 55 are free for face roles a CHAR rig profile may add"): `JOINT_ROLE_JAW` 55, `JOINT_ROLE_TONGUE` 56, `JOINT_ROLE_LEFT_BROW` 57, `JOINT_ROLE_RIGHT_BROW` 58, `JOINT_ROLE_LEFT_UPPER_LID` 59, `JOINT_ROLE_RIGHT_UPPER_LID` 60, `JOINT_ROLE_LEFT_LOWER_LID` 61, `JOINT_ROLE_RIGHT_LOWER_LID` 62, none required for "humanoid profile complete"; `uANIM_JOINT_ROLE_COUNT` becomes 62. The enumerators live in ANIM's `Zenith_AnimationTypes.h`; the ticket adding them is ANIM's (row CH1), and CHAR's facial tickets block on it.
<!-- /assumption -->

The facial template (`character.rig.addFacialTemplate`, or the `facial` argument of `character.rig.applyProfile`) adds to the rig document:

1. The face roles the profile assigns (the last row of "Rig profile table").
2. The sixteen `CONTROL` curves of the viseme set (`Zenith2/Docs/Design/CHAR/Facial.md → "Viseme set"`), none additive: the fifteen viseme curves `char.viseme.<v>` (default 0, except `char.viseme.sil`, default 1) and `char.lipsync.energy` (default 0).
3. The expression curves of the control set the character uses ("Control set document"), `CONTROL`, default 0.
4. No `MORPH` curve: morph-target curves come from the meshes' target names through ANIM's curve table construction (`Zenith2/Docs/Design/ANIM/Formats.md → "Curve table section"`).

The curve count after the template is at most 16 + the control count + the mesh targets; above `uANIM_MAX_CURVES` (512) the command fails with `CHAR_ERROR_LIMIT_EXCEEDED` before any edit. `Test_CharFacialTemplate_AddsVisemeCurves` asserts the sixteen names, kinds and defaults; `Test_CharFacialTemplate_CurveLimitRefused` asserts the limit.

## Default ragdoll generation

<!-- assumption: pending PHYS (rows CH8, CH20) -->
`character.ragdoll.generateDefault` writes a new `.ragdoll.json` (`Zenith2/Docs/Design/PHYS/Formats.md → "Ragdoll document"`, document type `Zenith.RagdollDocument`, PHYS's schema version 1) beside the rig document, from the profile alone, through `Zenith_CharBuildDefaultRagdoll(const Zenith_CharSkeletonView& xSkeleton, const Zenith_CharProfileMatch& xMatch, float fMassKilograms, Zenith_StringView strSkeletonPath, Zenith_Array<char>& xJsonOut)` (`Zenith2/Docs/Design/CHAR.md → "Rig authoring API"`). CHAR writes PHYS's members and no other; PHYS's cook (`physics.ragdoll`) validates it (row CH8 asks PHYS to confirm the members and the `skeleton` path form). PHYS's example writes `offset.rotationDegrees` as three numbers without naming their convention; CHAR writes `[pitch, yaw, roll]` of CORE's Euler convention (`R = R_y(yaw) · R_x(pitch) · R_z(roll)`, `Zenith2/Docs/Design/CORE/Maths.md → "Coordinate systems"`), and row CH20 asks PHYS to state that convention for the member.
<!-- /assumption -->

| Part (joint role) | Parent | Shape | Mass fraction | Constraint |
|---|---|---|---|---|
| HIPS | — | capsule along the HIPS → SPINE segment | 0.15 | — |
| CHEST (UPPER_CHEST when assigned) | HIPS | box, half extents (0.5 × shoulder span, 0.5 × chest → NECK length, 0.35 × shoulder span) | 0.25 | SwingTwist, twist [−20°, 20°], cones 30° / 20° |
| HEAD | CHEST | sphere, radius 0.5 × NECK → HEAD length + 0.06 m | 0.08 | SwingTwist, twist [−45°, 45°], cones 40° / 40° |
| UPPER_ARM per side | CHEST | capsule along UPPER_ARM → LOWER_ARM | 0.03 | SwingTwist, twist [−60°, 60°], cones 80° / 60° |
| LOWER_ARM per side | UPPER_ARM | capsule along LOWER_ARM → HAND | 0.02 | Hinge [0°, 140°] |
| UPPER_LEG per side | HIPS | capsule along UPPER_LEG → LOWER_LEG | 0.10 | SwingTwist, twist [−30°, 30°], cones 70° / 30° |
| LOWER_LEG per side | UPPER_LEG | capsule along LOWER_LEG → FOOT | 0.06 | Hinge [−140°, 0°] |
| FOOT per side | LOWER_LEG | box, half extents (0.05, 0.04, 0.5 × FOOT → TOES length + 0.02) | 0.015 | SwingTwist, twist [−10°, 10°], cones 30° / 20° |

The fractions sum to 0.93 (0.15 + 0.25 + 0.08 + 2 × (0.03 + 0.02 + 0.10 + 0.06 + 0.015)); the remaining 0.07 is the neck and the hands, which have no part. Each part's mass is its fraction times the class mass (`classes[].massKilograms` of the character project document, default 70 kg). The shoulder span is `|p_LEFT_UPPER_ARM − p_RIGHT_UPPER_ARM|`; a length "A → B" is `|p_B − p_A|` of rest model positions.

Geometry, in binary64, for a part on joint j whose shape follows the segment from j to the joint c the table names (for the head, c is the head joint itself and the segment is NECK → HEAD):

1. `s = p_c − p_j` in model space; `L = Zenith_Sqrt(s · s)`; `u = R_jᵀ s / L`, the unit segment direction in j's rest local frame (`R_j` the rotation of `restModel_j`, orthonormal because a library rig has unit rest scales; a scaled joint is already the warning CV2).
2. A capsule has `radius r = clamp(0.22 × L, 0.04, 0.15)`, `halfHeight = max(0.01, 0.5 × L − r)`, offset position `0.5 × L × u`, and the rotation that turns the capsule's +Y axis onto u: the shortest-arc quaternion `q = normalise(w = 1 + u_y, x = u_z, y = 0, z = −u_x)` (the cross product +Y × u), or the half turn about +X `(w 0, x 1, y 0, z 0)` when `1 + u_y < 1e-9`; `normalise` divides by `Zenith_Sqrt` of the squared norm.
3. A box has the offset position `0.5 × L × u` and the identity rotation (its axes are j's local axes). The head sphere's offset position is `0.5 × L × u_head`, with `u_head` the NECK → HEAD direction in HEAD's frame.
4. The rotation is written as `rotationDegrees = [pitch, yaw, roll]` with `pitch = asin(clamp(2 (w x − y z), −1, 1))`, `yaw = Zenith_Atan2(2 (x z + w y), 1 − 2 (x² + y²))`, `roll = Zenith_Atan2(2 (x y + w z), 1 − 2 (x² + z²))`, `asin(a)` computed as `Zenith_Atan2(a, Zenith_Sqrt(max(0, 1 − a²)))`, each converted to degrees by `× 180 / dPI`. No CRT transcendental is called (the rule of `Zenith2/Docs/Design/CHAR/Facial.md → "Estimator input"`).
5. Steps 1 to 4 are the internal `Zenith_CharComputeRagdollGeometry(const Zenith_CharSkeletonView&, const Zenith_CharProfileMatch&, Zenith_Array<Zenith_CharRagdollPartGeometry>& axOut)` of `Internal/CHAR/Zenith_CharRagdoll.h`, which returns per part the unrounded binary64 offset position, quaternion, Euler angles in degrees, radius, half height and half extents (`struct Zenith_CharRagdollPartGeometry { uint32 m_uJoint; double m_adOffset[3]; double m_adQuaternion[4]; double m_adRotationDegrees[3]; double m_dRadius, m_dHalfHeight; double m_adHalfExtents[3]; }`), the values the FMA3 test compares byte for byte. `rootHeightOffset` is the hips' rest height. Every number is rounded to 1e-4 (`round(x × 10,000) / 10,000`, halves away from zero) before writing, so the document is byte-identical on every machine of one build; `Test_CharRagdoll_IndependentOfCrtFma3Path` and the cross-host digests of `Zenith2/Docs/Design/CHAR/Facial.md → "Estimator input"` prove it.

`Test_CharRagdoll_DefaultFromProfile` asserts, for the Mixamo-named fixture at 70 kg (`Zenith2/Docs/Design/CHAR/Tests.md → "Fixtures and generators"`, identity rest rotations): the parts, parents and constraints of the table; the masses (HIPS 10.5 kg, CHEST 17.5, HEAD 5.6, each upper arm 2.1, lower arm 1.4, upper leg 7.0, lower leg 4.2, foot 1.05; total 65.1 = 0.93 × 70); the hips capsule (L = 0.1: radius 0.04, halfHeight 0.01, offset (0, 0.05, 0), rotation [0, 0, 0]); the left upper-arm capsule (L = 0.27: radius 0.0594, halfHeight 0.0756, offset (0.135, 0, 0), rotation [0, 0, −90]) and the right one (offset (−0.135, 0, 0), rotation [0, 0, 90]). `Test_CharRagdoll_CookAccepts` cooks it through PHYS's step with the cook test access.

## Shared library rig rule

<!-- informative -->
The reference engine's humans became correct only when every artist import bound undeformed to one T-posed shared rig: one skeleton is one rest pose, a pose change is never baked into a mesh, and a rebind blend must be rigid. ANIM already enforces the first two at import and cook (`Zenith2/Docs/Design/ANIM/Formats.md → "Rest pose from inverse bind matrices"`, SV9); CHAR's tools encode the workflow consequences.
<!-- /informative -->

- A game's shared animation library is authored on library rigs: skeletons whose rig document carries a complete humanoid profile, unit rest scales, a +Z facing and a T-pose or A-pose rest classification. CHAR's validation reports a library rig that breaks any of these (`CHAR_DIAG_LIBRARY_RIG_INVALID`); a source is a library rig when its `.meta` carries the label `char-library-rig`.
- A character whose meshes come from another rest pose is never rebound to the library rig: it gets its own skeleton and a retarget document from the library rig ("Retarget setup"), and ANIM's retargeting carries the rest-pose difference through per-joint corrections (`Zenith2/Docs/Design/ANIM/Runtime.md → "Retarget map construction"`).
- No CHAR tool writes a mesh, a skin weight or an inverse bind matrix; every rig operation edits documents ANIM, PHYS or CHAR owns.

`Test_CharLibraryRig_ValidationRules` asserts each diagnostic on fixture rigs.

## Retarget setup

`character.retarget.create [sourceSkeleton, targetSkeleton, clips, path]` writes a new `.animretarget.json` (`Zenith2/Docs/Design/ANIM/Formats.md → "Retarget document schema"`):

1. Both skeletons must be loaded and both rig documents must carry a complete humanoid profile; otherwise the command lists, from `Zenith_CharMatchRigProfile` on each, every target joint with no role mapping and fails with `CHAR_ERROR_PROFILE_INCOMPLETE` (the author then maps them in the retarget panel, which writes `jointMap` entries).
2. `rootTranslation` is `"ScaleByHipsHeight"`; `chainMap` is empty (the role-derived mapping); `jointMap` maps every target face role's joint to `null` when the source has no such role (face joints keep the target rest).
3. `clips` is the argument's list, or, with `clips` omitted, every `anim:` product of the source skeleton's source asset, ascending GUID.
4. The command then builds a `Zenith_RetargetMap` (`Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationRetarget header"`) from the two loaded skeletons and applies it to the source's rest pose; the result must equal the target's rest pose within 1e-5 per component (the identity ANIM's `Test_Retarget_RestToRestIsIdentity` proves for its fixtures); a larger difference is reported per joint (`CHAR_DIAG_RETARGET_REST_MISMATCH`) and the document is still written, because the difference names a rig error the author fixes.

`Test_CharRetarget_CreateWritesRoleMapping` and `Test_CharRetarget_IncompleteProfileListsJoints` are unit tests over fixtures; `Test_CharRetarget_RestCheckUsesAnimMap` asserts step 4 against a T-posed source and an A-posed target.

## Control set document

A control set names the high-level controls an animator poses a character with and that a face is driven by: file `*.charcontrols.json`, document type `Zenith.CharacterControlSetDocument`, schema tag `zenith2.charcontrols`, version 1, canonical (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical rules"`):

```json
{
	"$schema": "zenith2.charcontrols",
	"schemaVersion": 1,
	"id": "2b6f0c1e-8a35-4d77-9c10-4f0e5a2d7b61",
	"skeleton": "1b7e9f02-44c1-4d0a-8f3e-92aa5c61e0b4",
	"controls": {
		"7d41…": { "name": "Smile.L", "group": "Face", "min": 0.0, "max": 1.0, "default": 0.0,
			"target": { "kind": "Pose", "poseBook": "e3a9…", "pose": "Smile.L" } },
		"9a02…": { "name": "Jaw.Open", "group": "Face", "min": 0.0, "max": 1.0, "default": 0.0,
			"target": { "kind": "JointRotation", "joint": "jaw", "axis": [1, 0, 0], "degreesAtMax": 25.0 } },
		"c55e…": { "name": "LookAt", "group": "Body", "kind": "Point",
			"target": { "kind": "IKGoal", "goal": 0, "definition": "LookAt" } },
		"0e7b…": { "name": "Speed", "group": "Locomotion", "min": 0.0, "max": 8.0, "default": 0.0,
			"target": { "kind": "Parameter", "parameter": "Speed" } }
	},
	"groups": [ "Body", "Locomotion", "Face" ]
}
```

| Member | Type | Rule |
|---|---|---|
| `skeleton` | persistent ID | the skeleton product the controls address |
| `controls` | persistent-ID map (rule C4) | at most 256 controls (`uCHAR_MAX_CONTROLS`) |
| `controls.*.name` | string | 1 … 63 bytes, unique within the document |
| `controls.*.group` | string | one of `groups` |
| `controls.*.kind` | `"Scalar"` (default) or `"Point"` | a Point control is a world-space position (IK and look-at goals) |
| `controls.*.min`, `max`, `default` | numbers | Scalar only; `min < max`, `default` in range |
| `controls.*.target.kind` | `"Parameter"`, `"Curve"`, `"Pose"`, `"JointRotation"`, `"IKGoal"` | "Control evaluation" |
| `groups` | ordered array (rule C8) | display order, at most 32 |

An unknown joint, curve, pose or parameter name is a validation error naming the control and the name (`CHAR_DIAG_CONTROL_TARGET_UNKNOWN`), reported by the document importer ("Document validation importer" of `Zenith2/Docs/Design/CHAR.md`) and the posing panel; it never fails a load.

## Control evaluation

`Zenith_CharEvaluateControls(const Zenith_CharControlSet&, Zenith_Span<const float> afValues, Zenith_Span<const Zenith_Vector3> axPoints, Zenith_CharControlOutputs& xOut)` turns control values into the inputs the preview applies; it is used by the posing panel and by tests, never by the runtime:

| Target kind | Output | Applied by |
|---|---|---|
| `Parameter` | (parameter name hash, value) | `Zenith_AnimationPreview::SetParameter` |
| `Curve` | (curve name hash, value) | the preview pose's curve, written after evaluation |
| `Pose` | (`Zenith_CharPoseWeight`: the pose book, the pose's index `m_uPose` in it, weight = (value − min) / (max − min)) | the pose book's poses blended onto the evaluated pose as additive deltas from the pose book's neutral pose, `pose = base ⊕ Σ w_k (P_k ⊖ P_0)`, the formula of `Zenith2/Docs/Design/ANIM/Runtime.md → "Pose libraries"`, computed with ANIM's pose operations (`Zenith_ApplyAdditivePose`) |
| `JointRotation` | (`Zenith_CharJointRotation`: the joint index `m_uJoint`, unit axis in the joint's local frame, angle = `degreesAtMax × (value − min) / (max − min)`) | a local rotation post-multiplied onto the joint after the pose book step, the quaternion built with `Zenith_Sin` and `Zenith_Cos` of half the angle in radians |
| `IKGoal` | (`Zenith_CharIKGoalOutput`: goal index 0 … 7, the definition's name hash, model-space position) | the posing panel's IK step: `Zenith_SolveTwoBoneIK`, `Zenith_SolveChainIK` or `Zenith_SolveLookAt` (`Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationIK header"`) by the definition's kind |

Before evaluation the control set is resolved against its skeleton and pose books by `Zenith_CharResolveControlSet(Zenith_CharControlSet& xInOut, const Zenith_CharSkeletonView& xSkeleton, Zenith_Span<const Zenith_CharPoseBook* const> apxPoseBooks, Zenith_Array<Zenith_CharDiagnostic>& axDiagnostics)` (`Zenith2/Docs/Design/CHAR.md → "Controls and pose book API"`): for each control in document order it fills the target's indices (`Zenith2/Docs/Design/CHAR/Types.md → "Control and pose book member types"`): JointRotation `m_uJoint`, the index of the skeleton joint whose name equals `m_strName` after normalisation; Pose `m_uPose`, the index in the pose book of `m_xPoseBook` (found among `apxPoseBooks` by ID) whose pose name equals `m_strName`; Parameter, Curve and IKGoal need no index (their name hashes are the keys; the posing panel's IK step finds the IK definition by that hash in the rig document's `ik` array), so `Zenith_CharReadControlSet` already sets their `m_bResolved` true; a set that was read but never resolved therefore evaluates its name-keyed controls and skips (and counts) only its JointRotation and Pose controls. A name it cannot resolve leaves the index at its unresolved value (`uCHAR_UNRESOLVED`), sets `m_bResolved` false and appends `CHAR_DIAG_CONTROL_TARGET_UNKNOWN` naming the control and the name; the function fails only with `CHAR_ERROR_OUT_OF_MEMORY`. `Zenith_CharEvaluateControls` requires `afValues.size()` = the control count (one value per control in document order; a Point control's value is ignored) and `axPoints.size()` = the number of Point controls (in document order), and otherwise outputs nothing and counts the call in `m_uRefusedCalls`; it skips every control whose target is unresolved, counting it in `m_uSkippedUnresolved`.

Order: parameters (before `Advance`), then curves, pose-book poses in control order, joint rotations in control order, IK goals in goal order. Values are clamped to [min, max]; a non-finite value or point is ignored and counted in the panel's diagnostics. The function is O(controls + poses × J). `Test_CharControls_ResolveFillsIndices` (a joint and a pose resolved; an unknown pose name left unresolved with the diagnostic and skipped by evaluation), `Test_CharControls_EvaluateOrderAndClamp`, `Test_CharControls_PoseWeightFromRange`, `Test_CharControls_SpanLengthsChecked` and `Test_CharControls_NonFiniteIgnored` cover it; `Test_CharControls_PoseMatchesAnimPoseBlend` (`ZenithCook_Tests`: the pose book is cooked with the cook test access, ruling GEOM-N2, and the graph compiled in memory with `Zenith_CompileAnimationGraph`) asserts that a Pose control at full weight equals ANIM's `POSE_BLEND` node over the cooked pose book clip within 1e-5 (the posing panel shows what the runtime will show).

## Pose book document

A pose book is a named set of poses of one skeleton: the expressions and visemes of a joint-driven face, the viseme poses of a morph-driven face, or hand poses. File `*.posebook.json`, document type `Zenith.CharacterPoseBookDocument`, schema tag `zenith2.posebook`, version 1:

```json
{
	"$schema": "zenith2.posebook",
	"schemaVersion": 1,
	"id": "e3a9…",
	"skeleton": "1b7e…",
	"neutral": "rest",
	"poses": [
		{ "name": "viseme.aa", "joints": { "jaw": { "r": [0.2164, 0, 0, 0.9763] }, "lip_lower": { "t": [0, -0.004, 0.001] } },
		  "curves": { "jawOpen": 0.55 } },
		{ "name": "Smile.L", "joints": { "lip_corner_l": { "t": [0.006, 0.004, 0] } }, "curves": { "mouthSmileLeft": 1.0 } }
	]
}
```

| Member | Type | Rule |
|---|---|---|
| `skeleton` | persistent ID | the skeleton the poses are of |
| `neutral` | `"rest"` | frame 0 of the cooked clip is the skeleton's rest pose (the only value at version 1) |
| `poses` | ordered array (rule C8) | at most 255 poses (`uCHAR_MAX_POSES`); a pose's index + 1 is its frame |
| `poses[].name` | string | 1 … 63 bytes, unique; the frame's marker name |
| `poses[].joints` | keyed map joint name → `{ "t"?, "r"?, "s"? }` | the joint's local transform components that differ from rest; absent components are rest; `r` a unit quaternion (x, y, z, w) normalised by the cook |
| `poses[].curves` | keyed map curve name → number | curve values that differ from the curve's default |

## Pose book cook

The importer `char.posebook` (`Zenith2/Docs/Design/CHAR.md → "Importers and cook registration"`) declares one product `anim:posebook` (kind `Animation.Clip`, primary) and writes ANIM's clip intermediate `anim.clip` (`Zenith2/Docs/Design/ANIM/Formats.md → "Clip intermediate"`, magic `ZICL`, version 1) through ANIM's writer (`Zenith2/Docs/Design/CHAR/Facial.md → "Clip intermediate writer"`, row CH15):

1. Frame rate 30 Hz; F = poses + 1 frames; frame 0 the rest pose, frame k the pose k − 1.
2. For each joint named by any pose: three channels (translation, rotation, scale) with `STEP` interpolation and F keys at times k / 30, the value of each frame (rest where the pose does not name the joint or the component); for each curve named by any pose, one curve channel likewise (the curve default where absent).
3. The product's importer settings set `anim.sampleRateHz` = 30, `anim.loop` false, root-motion mode none, compression `errorMetres` 0.0001 and `curveError` 0.0005 (half ANIM's defaults: faces are judged close up), streaming not allowed, and the clip annotation's `poseLibrary` true with one marker per frame k ≥ 1 named `poses[k − 1].name` at time k / 30 (the clip annotation is the generated sub-asset settings of the product, `Zenith2/Docs/Design/ANIM/Formats.md → "Clip annotation document schema"`).

<!-- assumption: pending ANIM (row CH1) -->
The importer writes the pose-library flag and the markers through the product's settings, which `anim.clip` reads as a clip annotation's `poseLibrary` and `markers` members; row CH1 asks ANIM to confirm that an importer other than CONTENT's glTF importer may write `anim.clip` intermediates for a skeleton product of another source (as ANIM's own retarget importer does) and that these settings reach the step.
<!-- /assumption -->

The cooked clip is what `POSE_BLEND` reads (`Zenith2/Docs/Design/ANIM/Graph.md → "Node catalogue"`, kind 12). A joint or curve the skeleton lacks fails the import with `IMPORT_ERROR_SOURCE_INVALID`, detail `CHAR_DIAG_POSEBOOK_UNKNOWN_NAME`, checked as "Pose book name validation" states. `Test_CharPoseBook_CookFramesAndMarkers` asserts the frame layout and markers; `Test_CharPoseBook_DeterministicIntermediate` asserts byte-identical intermediates for identical documents.

### Pose book name validation

An importer cannot read another asset's cooked output, so the pose book importer validates names against the skeleton's rig document and source names it can read as import inputs: the document's sibling file `<posebook name>.posebooknames.json`, written by the editor's pose book panel whenever it saves the pose book (the skeleton's joint names in joint order and its curve names in table order, `{ "$schema": "zenith2.posebook-names", "schemaVersion": 1, "joints": […], "curves": […] }`), read with `Zenith_ImportContext::ReadInput` (`Zenith2/Docs/Design/ASSET/Import.md → "Zenith_ImportContext"`), so a skeleton change re-imports the pose book through the import-input edge. A missing names file skips name validation with the warning `CHAR_DIAG_POSEBOOK_NAMES_MISSING`; ANIM's `anim.clip` step then refuses unknown names itself.

## Pose capture and mirroring

The posing panel writes pose book entries through JSON edits (one undo record per capture):

- **Capture** (`character.posebook.capturePose [posebook, name, joints]`; a tracked document, so CHAR's no-CRT-transcendental rule applies): from the preview's current local pose, for each joint of the `joints` argument (default: every joint whose local transform differs from rest by more than 1e-5 per component), the components that differ, rounded to 1e-6; curves whose value differs from default by more than 1e-6.
- **Mirror** (`character.posebook.mirrorPose [posebook, source, name]`): the source pose reflected through the rig document's mirror table with ANIM's mirror rule (`Zenith_MirrorPose`, `Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationPose header"`); names ending `.L`/`.R` (or `_l`/`_r`) swap; curves whose names end `Left`/`Right` (or `.L`/`.R`) swap values.

`Test_CharPoseBook_CaptureWritesOnlyDifferences` and `Test_CharPoseBook_MirrorSwapsSidesAndCurves` cover both.

## Rigging limits

| Quantity | Limit | Error |
|---|---|---|
| joints in a skeleton view | 1,024 (`uANIM_MAX_JOINTS`) | `CHAR_ERROR_LIMIT_EXCEEDED` |
| joint name bytes | 63 | as ANIM refuses |
| controls per control set | 256 | `CHAR_ERROR_LIMIT_EXCEEDED` |
| control groups | 32 | `CHAR_ERROR_LIMIT_EXCEEDED` |
| poses per pose book | 255 | `CHAR_ERROR_LIMIT_EXCEEDED` |
| joints per pose | 1,024 | `CHAR_ERROR_LIMIT_EXCEEDED` |
| document bytes | 64 MiB (SCENE's open limit) | `DOCUMENT_ERROR_TOO_LARGE` |

Each limit is tested at the limit and at limit + 1 ([Z2-N-ARCH-953]) by `Test_CharRigLimits_AtAndAboveLimit`.
