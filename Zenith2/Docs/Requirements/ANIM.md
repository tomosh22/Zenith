# ANIM requirement catalogue

## Purpose

This catalogue breaks the ANIM inventory items (`Zenith2/Docs/Requirements/Inventory.md → "Inventory v1"`, section "ANIM Animation") and the cross-area item Z2-I-CHAR-04 (animation update budgeting, CHAR's item whose runtime ANIM implements) into testable requirements `Z2-R-ANIM-NNN`. Each requirement names the inventory items it covers, the stage whose acceptance it serves (which is also its fix version), its classification, its governing decisions and budget rows, the artefact that proves it, and the headings of the ANIM design family (`Zenith2/Docs/Design/ANIM.md` and its section files) that specify it; the design entry's "Requirement index" is the reverse map. The catalogue is two files with one numbering: this file holds the purpose, the requirements of Stages 1 and 3, the coverage of inventory items and the exclusions; `Zenith2/Docs/Requirements/ANIM/Stage4Plus.md` holds the requirements of Stages 4 and 6. ANIM owns every requirement here; where another area supplies part of the evidence (PHYS's ragdolls, ray casts and character controller, GAME's authority census, GEOM's skinned cook and skinning, RPROD's instance keys, SAVE's snapshot, SCRIPT's bindings, EDIT's panels, CHAR's walkthrough), that area is named in the evidence and in `Zenith2/Docs/Design/ANIM.md → "Dependencies"`, and the cross-area row it depends on is named (`AN1` … `AN17`, `Zenith2/Docs/Design/ANIM.md → "Reconciliation items"`). A budget named "pending row" is a row proposed in `Zenith2/Docs/Design/ANIM.md → "Pending row needs"` and allocated by its producer before the requirement's tickets are claimable. Every test named in the evidence is specified in `Zenith2/Docs/Design/ANIM/Tests.md`; every test runs headless and generates its fixtures at test time.

## Requirements

### Z2-R-ANIM-001 Module library target

- **Statement:** `ZenithAnimation` is a `KIND MODULE` static library with `MODULE_NAME Animation` in `Zenith2/Engine/ZenithAnimation/`, declares `ZENITH_GPU` and exactly the `DEPENDS` list of the table "Target summary" (including the module edge to `ZenithPhysics`), contains no platform-specific source, and no Player links it unless the game's `game.json` selects `Animation`.
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-02, Z2-I-ANIM-03, Z2-I-ANIM-04, Z2-I-ANIM-05, Z2-I-ANIM-06, Z2-I-ANIM-07, Z2-I-ANIM-08, Z2-I-ANIM-09, Z2-I-ANIM-10, Z2-I-ANIM-11, Z2-I-ANIM-12, Z2-I-ANIM-13, Z2-I-ANIM-14, Z2-I-ANIM-15, Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-001, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `Libgraph` (declared edges against `declared_deps.json`), `CompileCoverage` and `SourceLayout` on the ticket that lands the library; a game without the module links no ZenithAnimation symbol (Libgraph link surface). Pending ARCH (row AN2): the edges, the layer and the Animation → Physics module dependency land in Architecture.md first, and the scaffold ticket is `blocks`-linked from that change.
- **Design:** Zenith2/Docs/Design/ANIM.md → "Target summary"; Zenith2/Docs/Design/ANIM.md → "ZenithAnimation files"; Zenith2/Docs/Design/ANIM.md → "ZENITH_SUPPORTED matrix"

### Z2-R-ANIM-002 Module entry points

- **Statement:** `Zenith_Module_Animation.h` declares exactly `Zenith_Module_Animation_RegisterTypes` and `Zenith_Module_Animation_Start`, and their steps (and the world-attach steps) register ANIM's types, configuration keys, kind loaders, command kinds, Luau bindings, memory consumer, world resources, systems, the arbitration declaration and the extractor in the order of the step table, and no document type (those are tool-side).
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-02, Z2-I-ANIM-03, Z2-I-ANIM-04, Z2-I-ANIM-05, Z2-I-ANIM-06, Z2-I-ANIM-07, Z2-I-ANIM-08, Z2-I-ANIM-09, Z2-I-ANIM-10, Z2-I-ANIM-11, Z2-I-ANIM-12, Z2-I-ANIM-13, Z2-I-ANIM-14, Z2-I-ANIM-15, Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimationModule_HeadlessRuns` and `Test_AnimationModule_AuthorityTableRegistered` in `ZenithAnimation_Tests` (`Test` step); `ApiContractCheck` over the header.
- **Design:** Zenith2/Docs/Design/ANIM.md → "Zenith_Module_Animation header"; Zenith2/Docs/Design/ANIM.md → "Component registration"

### Z2-R-ANIM-003 Construction steps unwind

- **Statement:** Every construction step of module registration, module start and world attach has a failure-injection test that fails that step alone and asserts the module error, zero leaks, and no registration, configuration callback or world resource left behind.
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-02, Z2-I-ANIM-03, Z2-I-ANIM-04, Z2-I-ANIM-05, Z2-I-ANIM-06, Z2-I-ANIM-07, Z2-I-ANIM-08, Z2-I-ANIM-09, Z2-I-ANIM-10, Z2-I-ANIM-11, Z2-I-ANIM-12, Z2-I-ANIM-13, Z2-I-ANIM-14, Z2-I-ANIM-15, Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** The ten tests `Test_AnimationModule_Create_FailsAt<Step>_Unwinds` (one per step of the step table) in `ZenithAnimation_Tests` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM.md → "Zenith_Module_Animation header"; Zenith2/Docs/Design/ANIM.md → "Construction and shutdown"; Zenith2/Docs/Design/ANIM/Tests.md → "Module binding and failure tests"

### Z2-R-ANIM-004 Public headers carry contracts

- **Statement:** Every public type of the ZenithAnimation headers carries one contract block (ownership, thread affinity, invalidation, units, errors, version), every fallible function returns `Zenith_Result<…, Zenith_AnimationError>`, and each header compiles alone with no STL type and no third-party header.
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-02, Z2-I-ANIM-03, Z2-I-ANIM-04, Z2-I-ANIM-05, Z2-I-ANIM-06, Z2-I-ANIM-07, Z2-I-ANIM-08, Z2-I-ANIM-09, Z2-I-ANIM-10, Z2-I-ANIM-11, Z2-I-ANIM-12, Z2-I-ANIM-13, Z2-I-ANIM-14, Z2-I-ANIM-15, Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-047, Z2-D-045, Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck`, `HeaderSelfContained` and `ForbiddenCheck` over the module's public headers (`Libgraph`/`Checks` gate steps).
- **Design:** Zenith2/Docs/Design/ANIM.md → "Rules across the ANIM API"; Zenith2/Docs/Design/ANIM.md → "Header map"

### Z2-R-ANIM-005 API-only slices first

- **Statement:** The first ANIM tickets land the three API-only slices of "API-only slices" (declarations, contract blocks, stub definitions) so GAME, PHYS, CINE, SCRIPT, GEOM and CONTENT tickets build against them before the implementations.
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-02, Z2-I-ANIM-03, Z2-I-ANIM-04, Z2-I-ANIM-05, Z2-I-ANIM-06, Z2-I-ANIM-07, Z2-I-ANIM-08, Z2-I-ANIM-09, Z2-I-ANIM-10, Z2-I-ANIM-11, Z2-I-ANIM-12, Z2-I-ANIM-13, Z2-I-ANIM-14, Z2-I-ANIM-15, Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-070
- **Budgets:** none
- **Acceptance evidence:** `HeaderSelfContained` and `ApiContractCheck` on each slice ticket; the dependent areas' tickets are `blocks`-linked from the slice that declares what they use.
- **Design:** Zenith2/Docs/Design/ANIM.md → "API-only slices"; Zenith2/Docs/Design/ANIM.md → "Ticket slices and ordering"

### Z2-R-ANIM-006 Error enumerations

- **Statement:** `Zenith_AnimationError` and `Zenith_AnimationCookDetail` carry exactly the values of "Zenith_AnimationErrors header", each value has a `Zenith_ToString` name, runtime failures report an error value (never an assert) and cook failures report `COOK_ERROR_STEP_FAILED` with the cook detail named in each validation rule.
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-02, Z2-I-ANIM-03, Z2-I-ANIM-04, Z2-I-ANIM-05, Z2-I-ANIM-06, Z2-I-ANIM-07, Z2-I-ANIM-08, Z2-I-ANIM-09, Z2-I-ANIM-10, Z2-I-ANIM-11, Z2-I-ANIM-12, Z2-I-ANIM-13, Z2-I-ANIM-14, Z2-I-ANIM-15, Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimFailure_CorruptAssetsFailLoad` and `Test_CookSkeleton_RejectsEachRule` (`Test` step); `ApiContractCheck` over `Zenith_AnimationErrors.h`.
- **Design:** Zenith2/Docs/Design/ANIM.md → "Zenith_AnimationErrors header"; Zenith2/Docs/Design/ANIM.md → "Error enumerations"

### Z2-R-ANIM-007 Headless evaluation

- **Statement:** A headless world (the `Test` step on hosts without a GPU device) runs every ANIM simulation system: animators evaluate, root motion and events publish and the extractor registers; no ANIM test requires a GPU, so no ANIM ticket is `needs-gpu`.
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-02, Z2-I-ANIM-03, Z2-I-ANIM-04, Z2-I-ANIM-05, Z2-I-ANIM-06, Z2-I-ANIM-07, Z2-I-ANIM-08, Z2-I-ANIM-09, Z2-I-ANIM-10, Z2-I-ANIM-11, Z2-I-ANIM-12, Z2-I-ANIM-13, Z2-I-ANIM-14, Z2-I-ANIM-15, Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimationModule_HeadlessRuns` in `ZenithAnimation_Tests` in the `Test` step on a host without a GPU device.
- **Design:** Zenith2/Docs/Design/ANIM.md → "Test targets and gate steps"; Zenith2/Docs/Design/ANIM/Tests.md → "Test targets"

### Z2-R-ANIM-008 Stage 3 components

- **Statement:** The components `Zenith.Animation.Animator`, `Zenith.Animation.RootMotion` and `Zenith.Animation.Pose` are registered with the property IDs, types, defaults, ranges and phase authority of "Zenith_AnimationComponents header"; property IDs are never reused.
- **Covers:** Z2-I-ANIM-04, Z2-I-ANIM-05, Z2-I-ANIM-10, Z2-I-ANIM-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-034
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimationModule_AuthorityTableRegistered` (`Test` step); ECS's reflection ID check over the registered types. The registering ticket adds the types' rows to `Zenith2/Docs/Research/Phases/AuthorityCensus.md` in the same diff ("Component registration"), checked by GAME's `Test_AuthorityCensus_CharacterModules` (the census test of the suite that links ZenithAnimation; `Test_AuthorityCensus_OpenWorldModules` at Stage 4; row GA20) (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM.md → "Zenith_AnimationComponents header"; Zenith2/Docs/Design/ANIM.md → "Component registration"

### Z2-R-ANIM-010 Field authority

- **Statement:** Every ANIM-registered property has exactly the authority phase of "Field authority table": phase 3 owns `Zenith.Animation.RootMotion`, phase 6 arbitrates `Zenith.Animation.Pose.PhysicsWeight` with the precedence (Cinematic, Ragdoll, Animation); ANIM's phase-3 and phase-6 nodes write no entity transform, its only transform write is the phase-4 `SetProperty` of `Animation.ApplyRootMotion` on `Zenith.LocalTransform` of entities without a character controller, and it writes no property it does not own except through commands.
- **Covers:** Z2-I-ANIM-10, Z2-I-ANIM-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, D-PHASES (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimationModule_AuthorityTableRegistered`, `Test_RootMotion_PhaseThreeNeverApplies` and `Test_RootMotion_AppliedInPhaseFourWithoutController` (`Test` step); ECS's authority assertion in assert-enabled builds over every integration run; GAME's `Test_AuthorityCensus_CharacterModules` over ANIM's census rows ("Component registration"; row GA20).
- **Design:** Zenith2/Docs/Design/ANIM.md → "Field authority table"; Zenith2/Docs/Design/ANIM.md → "Frame-graph placement"; Zenith2/Docs/Design/ANIM.md → "Component registration"

### Z2-R-ANIM-011 World resources

- **Statement:** Each attached world holds three world resources, each with exactly one declared writer node per stage: `Zenith.Animation.Residency` (authority `PHASE_ANIMATION`, written through the merge node of `Animation.Significance`: the instance table and residency records, block allocation, the binding and stream caches, the request table and in-flight counters), `Zenith.Animation.World` (authority `PHASE_ANIMATION`, written by `Animation.Evaluate`: parameter blocks and every instance's evaluation state) and `Zenith.Animation.FinalPoses` (authority `PHASE_POST_PHYSICS`, written by `Animation.PostPhysicsBlend`: the final local and model poses with their two-tick history and the ragdoll recovery state), laid out as "Resource partition" and "Pose store layout" state, with no pose data in components; every reader of an instance declares a read of `Zenith.Animation.Residency`, and no ANIM system names a generated arbitration node in an ordering, so every world that attaches the module compiles its schedule with or without Cinematics.
- **Covers:** Z2-I-ANIM-04, Z2-I-ANIM-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-040
- **Budgets:** Z2-B-ANIM-001
- **Acceptance evidence:** `Test_AnimationModule_ScheduleCompiles` (integration suite), `Test_AnimationBindings_GettersReadCommitted` and `Test_AnimExtract_InterpolationFullRate` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationWorld header"; Zenith2/Docs/Design/ANIM/Runtime.md → "Pose store layout"; Zenith2/Docs/Design/ANIM.md → "Design rulings owned by ANIM"

### Z2-R-ANIM-012 Dual quaternion maths

- **Statement:** `Zenith_DualQuaternion` and its functions (make, decompose, multiply, inverse, normalise, blend with sign alignment, screw-power `Zenith_PowDualQuaternion`) are rigid: a blend of two rigid transforms that share a fixed point keeps that point fixed; the screw power canonicalises the sign first (so `dq` and `−dq` give bit-identical results and a turn powers the short way round), evaluates angles below 1e-3 rad by series with no division by a vanishing sine and a truncation error below `5e-14 × |t|` for e ∈ [−0.5, 2], and `Pow(D, 1/k)` composed k times equals D within 1e-5.
- **Covers:** Z2-I-ANIM-10, Z2-I-ANIM-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-034
- **Budgets:** none
- **Acceptance evidence:** `Test_DualQuaternion_RigidRoundTrip`, `Test_DualQuaternion_BlendKeepsSharedPivot`, `Test_DualQuaternion_LinearTranslationBlendMovesPivot` (the negative control), `Test_DualQuaternion_PowComposesExactly`, `Test_DualQuaternion_BlendSignAlignment`, `Test_DualQuaternion_PowCanonicalisesSign`, `Test_DualQuaternion_PowNearIdentityFinite`, `Test_DualQuaternion_PowSeriesBranchContinuity` and `Test_DualQuaternion_PowHalfTurn` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM.md → "Zenith_AnimationMath header"; Zenith2/Docs/Design/ANIM/Graph.md → "Rigid delta blending"

### Z2-R-ANIM-013 Skeleton file format

- **Statement:** A cooked skeleton is a `.zskel` file of asset kind 7 (magic `ZSKL`, format version 1) carrying ASSET's 64-byte cooked header and the section kinds of "Skeleton sections": the info section, the joint hierarchy and rest pose sections, the joint name sections, the rig sections and the curve table, all little-endian and written field by field.
- **Covers:** Z2-I-ANIM-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048, Z2-D-004
- **Budgets:** Z2-B-ANIM-001
- **Acceptance evidence:** `Test_SkeletonAsset_LoadsValidFile` over generated bytes (`Test` step); `Zenith_Fuzz_SkeletonFile` in the `win-asan` sanitizer line.
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton file"; Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton sections"; Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton info section"; Zenith2/Docs/Design/ANIM/Formats.md → "Joint hierarchy and rest pose sections"; Zenith2/Docs/Design/ANIM/Formats.md → "Joint name sections"

### Z2-R-ANIM-014 Skeleton validation

- **Statement:** The skeleton cook and the skeleton loader enforce rules SV1 to SV8 (joint limit, one root with parents before children, unique hash-unique names, finite rest transforms with unit rotations, no shear, valid rig references, a symmetric mirror table, shell distances in range), the cook failing with the named cook detail and the loader with `ANIMATION_ERROR_CORRUPT`.
- **Covers:** Z2-I-ANIM-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_SkeletonAsset_RejectsJointLimit`, `Test_SkeletonAsset_RejectsParentOrder`, `Test_SkeletonAsset_RejectsBadRestTransform`, `Test_SkeletonAsset_RejectsInvalidRig`, `Test_SkeletonAsset_RejectsAsymmetricMirror`, `Test_SkeletonAsset_RejectsShellRange`, `Test_SkeletonAsset_RejectsCorruptOffsets` and `Test_CookSkeleton_RejectsEachRule` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton validation"

### Z2-R-ANIM-015 Joint lookup

- **Statement:** `Zenith_SkeletonAsset` finds a joint by name hash in O(log J) by binary search over the sorted name-hash table and returns joint indices in the skeleton's parent-before-child order, which every pose, clip track and remap uses.
- **Covers:** Z2-I-ANIM-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_SkeletonAsset_FindJointBinarySearch` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_SkeletonAsset header"; Zenith2/Docs/Design/ANIM/Formats.md → "Joint name sections"

### Z2-R-ANIM-016 Skins grouped into skeletons

- **Statement:** The import helper `Zenith_BuildSkeletons` groups a source's skins into skeletons by connected joint hierarchy, roots each skeleton at the lowest common ancestor of its joints, and inserts a synthetic root when the joints have no common ancestor.
- **Covers:** Z2-I-ANIM-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimImport_GroupsSkinsIntoSkeletons`, `Test_AnimImport_LowestCommonAncestorRoot` and `Test_AnimImport_SyntheticRoot` over generated glTF text (`ZenithCook_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Import conversion"; Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton extraction from a glTF skin"

### Z2-R-ANIM-017 Rest pose from inverse bind matrices

- **Statement:** The skeleton's rest pose is derived from the skins' inverse bind matrices (the bind pose), never from the node transforms of an animation frame, so the rest pose and every mesh's bind pose are the same pose.
- **Covers:** Z2-I-ANIM-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimImport_RestPoseFromInverseBinds` (`ZenithCook_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Rest pose from inverse bind matrices"

### Z2-R-ANIM-018 One skeleton one rest pose

- **Statement:** A skeleton has exactly one rest pose (rule SV9): skins of one source that bind the same joint with inverse bind matrices differing beyond the rest-pose tolerance fail import and cook with `ANIM_COOK_DETAIL_REST_POSE_CONFLICT`; no step rebinds, re-poses or deforms a mesh to make two skins agree.
- **Covers:** Z2-I-ANIM-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimImport_RestPoseConflictRefused` (`ZenithCook_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton validation"; Zenith2/Docs/Design/ANIM/Formats.md → "Rest pose from inverse bind matrices"; Zenith2/Docs/Design/ANIM.md → "Design rulings owned by ANIM"

### Z2-R-ANIM-019 Import refusals and limits

- **Statement:** The import helpers refuse duplicate joint names, a sheared joint transform, and a source above 65,536 nodes or 64 skeletons with `Zenith_AnimationImportError`, and return every refusal with its node name and code rather than a partial skeleton.
- **Covers:** Z2-I-ANIM-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimImport_DuplicateJointNameRefused`, `Test_AnimImport_ShearRefused` and `Test_AnimationFormats_LimitsAtAndAboveLimit` (`Test` step); `Zenith_Fuzz_AnimIntermediates` in the `win-asan` line.
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Import conversion"; Zenith2/Docs/Design/ANIM/Formats.md → "Parser limits"

### Z2-R-ANIM-020 Stable import products

- **Statement:** The animation half of the glTF importer writes one `anim.skeleton` intermediate per skeleton and one `anim.clip` intermediate per animation and skeleton, with product keys derived from source names, so reimporting a source with a renamed animation keeps the GUIDs of untouched products; FBX sources reach the same helpers only through the conversion path D-SEL-FBX selects.
- **Covers:** Z2-I-ANIM-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, D-SEL-FBX (open, Stage 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimImport_ProductKeysStable` (`ZenithCook_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton intermediate"; Zenith2/Docs/Design/ANIM/Formats.md → "Clip intermediate"; Zenith2/Docs/Design/ANIM/Formats.md → "Import conversion"

### Z2-R-ANIM-021 Mesh bind check against the rest pose

- **Statement:** A skinned mesh binds to a skeleton only in that skeleton's rest pose: GEOM's skinned-mesh cook compares each mesh joint's inverse bind matrix with the skeleton's published `restModelInverse` and refuses a mesh beyond the rest-pose tolerance (row AN4); ANIM publishes the property and the tolerance.
- **Covers:** Z2-I-ANIM-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_CookSkeleton_PublishesProperties` (`ZenithCook_Tests`, `Test` step) for ANIM's half; GEOM's skinned-mesh cook refusal test that row AN4 asks GEOM's catalogue to carry.
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton validation"; Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton published properties"

### Z2-R-ANIM-022 Skeleton published properties

- **Statement:** The `anim.skeleton` step publishes exactly nine properties (`jointCount`, `parents`, `restModel`, `restModelInverse`, `maxReach`, `jointNameHashes`, `restPoseHash`, `curveNameHashes`, `shellMeshes`) as cook-input edges, so GEOM's skinned cook and ANIM's clip and graph steps re-run exactly when one changes.
- **Covers:** Z2-I-ANIM-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_CookSkeleton_PublishesProperties` (`ZenithCook_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton published properties"; Zenith2/Docs/Design/ANIM.md → "Cook steps and derived-data keys"

### Z2-R-ANIM-023 Rig document applied at cook

- **Statement:** The rig document (`.animrig.json`, schema version 1) named by a skeleton's importer settings supplies joint roles, sockets, curves, shell distances and the `shellMeshes` list at cook; its SHA-256 joins the `anim.skeleton` key, and a rig naming an absent joint fails with `ANIM_COOK_DETAIL_RIG_INVALID`.
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-038, D-SEL-JSON (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_CookSkeleton_RigDocumentApplied` (`ZenithCook_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Rig document schema"; Zenith2/Docs/Design/ANIM/Formats.md → "Rig sections"; Zenith2/Docs/Design/ANIM.md → "Cook steps and derived-data keys"

### Z2-R-ANIM-025 Curve table

- **Statement:** A skeleton carries a curve table (at most 512 curves, sorted by name hash), and every pose carries one f32 per curve beside its J joint transforms, so facial and gameplay curves blend with the pose.
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_SkeletonAsset_LoadsValidFile` (curve section) and `Test_CookSkeleton_RigDocumentApplied` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Curve table section"; Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationPose header"

### Z2-R-ANIM-026 Skeleton loader

- **Statement:** The kind-7 loader validates a `.zskel` file with ASSET's header checks and SV1 to SV8 and produces an immutable `Zenith_SkeletonAsset` shared by every instance, refusing a corrupt or mismatched-version file without a partial object.
- **Covers:** Z2-I-ANIM-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-040
- **Budgets:** Z2-B-ANIM-001
- **Acceptance evidence:** `Test_SkeletonAsset_LoadsValidFile` and `Test_AnimFailure_CorruptAssetsFailLoad` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Asset kind loaders"; Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_SkeletonAsset header"

### Z2-R-ANIM-027 Clip file format

- **Statement:** A cooked clip is a `.zanim` file of asset kind 8 (magic `ZANM`, format version 1; runtime type `Zenith_AnimationClip`, as ASSET's kind table names it) carrying the info section, track table, clip ranges, 16-frame segments, root motion, events, markers and stream table of "Clip sections", with raw (32-bit) tracks a valid encoding of every segment.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048, Z2-D-004
- **Budgets:** Z2-B-ANIM-001
- **Acceptance evidence:** `Test_AnimationClip_LoadsRawClip` over generated bytes (`Test` step); `Zenith_Fuzz_ClipFile` in the `win-asan` line.
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Clip file"; Zenith2/Docs/Design/ANIM/Formats.md → "Clip sections"; Zenith2/Docs/Design/ANIM/Formats.md → "Clip info section"; Zenith2/Docs/Design/ANIM/Formats.md → "Track table section"; Zenith2/Docs/Design/ANIM/Formats.md → "Segment section"

### Z2-R-ANIM-028 Clip validation

- **Statement:** The clip loader enforces rules CV1 to CV8 (frame and segment consistency, joint and curve counts at bind, bit-rate indices and stream sizes, finite ranges, sorted events and markers, root-motion records, a gap-free stream table), failing with `ANIMATION_ERROR_CORRUPT` or `ANIMATION_ERROR_SKELETON_MISMATCH`.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimationClip_RejectsInconsistentFrames`, `Test_AnimationClip_RejectsBadBitRate`, `Test_AnimationClip_RejectsUnsortedEvents` and `Test_AnimationClip_RejectsStreamTableGap` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Clip validation"

### Z2-R-ANIM-029 Clip resampling

- **Statement:** The `anim.clip` step resamples every channel to the clip's uniform sample rate (default 30 Hz) honouring step, linear and cubic-spline source interpolation, keeps a partial last interval exact, and records duration, rate and frame count consistently.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CookClip_ResampleStepLinearCubic` and `Test_CookClip_PartialLastFrame` (`ZenithCook_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Clip resampling"; Zenith2/Docs/Design/ANIM/Formats.md → "Clip intermediate"

### Z2-R-ANIM-030 Loop closure

- **Statement:** A clip cooked as a loop has its in-place last frame equal to its first within the clip's error bound: the cook closes the loop after root-motion extraction (so the root-motion track keeps the per-loop motion) and reports a source discontinuity above 10 × ε with `ANIM_COOK_WARNING_LOOP_DISCONTINUITY`; the rule is cook-side only (CV6).
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CookClip_LoopClosedAndWarned` (`ZenithCook_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Clip resampling"; Zenith2/Docs/Design/ANIM/Formats.md → "Clip validation"; Zenith2/Docs/Design/ANIM/Formats.md → "Clip cook order"

### Z2-R-ANIM-031 Clip sampling

- **Statement:** `Zenith_SampleClip` samples any time in O(J + C) per call with constant-time segment lookup, interpolates a partial last interval and wraps a loop clip, and leaves its output untouched (returning not-resident) when a needed segment of a streamed clip is absent.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-ANIM-002
- **Acceptance evidence:** `Test_ClipSampling_PartialLastInterval`, `Test_ClipSampling_LoopWrap` and `Test_ClipSampling_NotResidentLeavesOutput` and `Test_ClipSampling_SegmentBoundary` (`Test` step); `Zenith_Bench_ClipSampling` with `--smoke`.
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationClip header"; Zenith2/Docs/Design/ANIM/Formats.md → "Segment section"

### Z2-R-ANIM-032 Clip intermediates and raw clip property

- **Statement:** Clip intermediates written by `Zenith_WriteClipIntermediates` hold per-joint channels keyed by the target joint's name (and a curve channel by its curve name), and the `anim.clip` step publishes `anim.clipInfo` and `anim.rawClip` (the resampled tracks before root-motion extraction and loop closure, with the clip's resolved cook settings: root-motion mode and motion joint, loop flag, additive, compression and streaming settings) so additive references, retargeted products and motion-matching databases re-cook from raw tracks, never from another product's quantised data.
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CookClip_AdditiveReferences` and `Test_CookClip_RetargetedProduct` (`ZenithCook_Tests`); `Zenith_Fuzz_AnimIntermediates` in the `win-asan` line.
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Clip intermediate"; Zenith2/Docs/Design/ANIM.md → "Cook steps and derived-data keys"; Zenith2/Docs/Design/ANIM/Formats.md → "Clip cook order"

### Z2-R-ANIM-033 Root motion extraction at cook

- **Statement:** Root motion is extracted at cook in the modes of `Zenith_RootMotionMode` (none, ground-plane translation with yaw, full translation with yaw, full transform), through the virtual root `V_k` so the motion track is `Q_k = V_0⁻¹ · V_k` and the motion joint's track keeps the in-place pose; a motion joint whose ancestor is animated fails the step. Extraction runs on the resampled frames before loop closure ("Clip cook order"), so a looping walk publishes its per-loop motion as `Q(D)`.
- **Covers:** Z2-I-ANIM-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CookClip_RootMotionModes`, `Test_CookClip_LoopRootMotionKeepsPerLoopMotion` and `Test_CookClip_RootMotionAncestorAnimatedRefused` (`ZenithCook_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Root motion extraction at cook"; Zenith2/Docs/Design/ANIM/Formats.md → "Root motion section"; Zenith2/Docs/Design/ANIM/Formats.md → "Clip cook order"

### Z2-R-ANIM-034 Additive clip cooking

- **Statement:** An additive clip is cooked against its in-place reference (a frame of itself, a frame of another clip with that clip's own root-motion extraction applied, or the rest pose) in local or model space as its settings state, storing the delta so that applying it at weight 1 to the reference reproduces the source clip within the clip's bound.
- **Covers:** Z2-I-ANIM-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CookClip_AdditiveReferences` (`ZenithCook_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Additive clip cooking"

### Z2-R-ANIM-035 Clip annotation documents

- **Statement:** A clip annotation document (`.animclip.json`, schema version 1) supplies events, notify states, sync markers, curves, the loop flag and root-motion, additive and compression settings; the cook writes sorted event and marker sections from it and its SHA-256 joins the `anim.clip` key.
- **Covers:** Z2-I-ANIM-08, Z2-I-ANIM-09, Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-038, D-SEL-JSON (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_CookClip_EventsAndMarkersFromAnnotation` (`ZenithCook_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Clip annotation document schema"; Zenith2/Docs/Design/ANIM/Formats.md → "Event and marker sections"

### Z2-R-ANIM-055 Asset kind loaders registered

- **Statement:** The module registers the kind loaders of kinds 7, 8 and 9 (`Animation.Skeleton`, `Animation.Clip`, `Animation.Graph`) at start, each producing an immutable shared asset object.
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_SkeletonAsset_LoadsValidFile`, `Test_AnimationClip_LoadsRawClip` and `Test_AnimationGraphAsset_ParameterLookup` through a real `Zenith_AssetRegistry` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Asset kind loaders"

### Z2-R-ANIM-056 Bindings published at tick boundaries

- **Statement:** An animator's graph, skeleton and clips bind only when their loads publish at a tick boundary in completion-ID order, with completion IDs allocated in ascending simulation ID by the merge function of `Animation.Significance` on the world's merge chain (it declares `ConsumesMergeCapacity`, as [Z2-N-SCRIPT-341] requires), so load timing never changes a result; the source is declared with `uMaxInFlight = animation.bindingQueueCapacity + animation.streaming.maxReadsInFlight` and `uMaxResultBytes = 24`, binding loads in flight never exceed `animation.bindingQueueCapacity` (so the completion queue never overflows) and chunk reads never exceed `animation.streaming.maxReadsInFlight`, a need beyond its bound is deferred to a subsequent run in its order without an ID, and every completion ID ANIM allocates is completed exactly once, an abandoned request's with the abandoned payload (pending X255).
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimBinding_PublishedAtTickBoundary`, `Test_AnimBinding_CompletionIDsInSimulationOrder` and `Test_AnimBinding_InFlightCapDefersInOrder` (`Test` step); `Test_AnimBinding_AbandonedRequestCompletesItsID` and the schedule order of `Test_AnimationModule_ScheduleCompiles` (integration suite, `Test` step). Pending MOD and SCRIPT (row AN1): the module publication sink lands first and the binding ticket is `blocks`-linked from it.
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Asset bindings"; Zenith2/Docs/Design/ANIM.md → "Determinism obligations"; Zenith2/Docs/Design/ANIM/Runtime.md → "Systems"; Zenith2/Docs/Design/ANIM/Runtime.md → "Animation.Significance"

### Z2-R-ANIM-057 Binding mismatch falls back to rest pose

- **Statement:** A graph whose skeleton GUID or rest-pose hash differs from a clip's, or whose load fails, leaves the instance `BINDING_FAILED`: logged once, no evaluation, and the extractor writes the skeleton's rest pose when the skeleton is bound.
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimBinding_MismatchFailsToRestPose` and `Test_AnimFailure_MissingClipRestPose` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Asset bindings"; Zenith2/Docs/Design/ANIM/Runtime.md → "Instance lifecycle state machine"; Zenith2/Docs/Design/ANIM.md → "Failure table"

### Z2-R-ANIM-058 Binding cache

- **Statement:** Each world caches its bound assets by GUID, so a second animator naming an already bound graph binds in the same tick boundary without a new load.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** Z2-B-ANIM-001
- **Acceptance evidence:** `Test_AnimBinding_CacheBindsImmediately` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Asset bindings"

### Z2-R-ANIM-059 No asset resolution in phase nodes

- **Statement:** No ANIM phase node calls `Zenith_AssetRegistry::Resolve` or reads asset residency directly; every asset reaches a phase node through a published binding.
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimBinding_NoResolveInPhaseNodes` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Asset bindings"; Zenith2/Docs/Design/ANIM.md → "Design rulings owned by ANIM"

### Z2-R-ANIM-060 Instance table ordering

- **Statement:** The instance table is keyed and iterated by simulation ID, independent of chunk layout, archetype moves and compaction.
- **Covers:** Z2-I-ANIM-04, Z2-I-ANIM-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimInstance_TableOrderIndependentOfLayout` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Instance table"

### Z2-R-ANIM-061 Instance lifecycle

- **Statement:** Each animator instance follows "Instance lifecycle state machine" (binding, bound, binding failed, disabled, destroyed) exactly, releasing bindings and memory at the end of `Animation.Significance` when its row disappears.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimInstance_LifecycleTransitions` (drives every row of the table, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Instance lifecycle state machine"

### Z2-R-ANIM-062 Instance cap

- **Statement:** At most `animation.maxInstances` (default 4,096) instances exist per world; animators beyond the cap are refused in ascending simulation ID and counted, never in layout order.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-ANIM-001
- **Acceptance evidence:** `Test_AnimInstance_CapRefusesInSimulationOrder` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Instance table"; Zenith2/Docs/Design/ANIM.md → "Configuration keys"

### Z2-R-ANIM-063 Graph file format

- **Statement:** A cooked graph is a `.zagraph` file of asset kind 9 (magic `ZAGR`, format version 1) carrying the info, node table, parameter table, state machine, blend space, mask and layer sections of "Graph sections", with nodes in post order.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** Z2-B-ANIM-001
- **Acceptance evidence:** `Test_AnimationGraphAsset_ParameterLookup` over a generated graph (`Test` step); `Zenith_Fuzz_GraphFile` in the `win-asan` line.
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Graph file"; Zenith2/Docs/Design/ANIM/Formats.md → "Graph sections"; Zenith2/Docs/Design/ANIM/Formats.md → "Graph info section"; Zenith2/Docs/Design/ANIM/Formats.md → "Node table section"; Zenith2/Docs/Design/ANIM/Formats.md → "Parameter table section"

### Z2-R-ANIM-064 Graph validation

- **Statement:** The graph loader enforces rules GV1 to GV6 (limits and bounds with children before parents, payload sizes, parameter kinds, non-overlapping instance state, the recomputed pose-stack depth, skeleton agreement at bind).
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimationGraphAsset_RejectsChildAfterParent`, `Test_AnimationGraphAsset_RejectsOverlappingInstanceState`, `Test_AnimationGraphAsset_RejectsPoseStackMismatch` and `Test_GraphCompiler_SkeletonMismatchRefused` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Graph validation"

### Z2-R-ANIM-065 Graph document schema

- **Statement:** Graphs are authored as `.animgraph.json` documents (schema `zenith2.animgraph`, version 1) holding parameters, nodes, state machines, layers, slots, sync groups and IK steps as "Graph document schema" defines.
- **Covers:** Z2-I-ANIM-04, Z2-I-ANIM-05, Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-038, D-SEL-JSON (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimDocuments_CanonicalRoundTrip` (`ZenithAuthoring_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Graph document schema"

### Z2-R-ANIM-066 Graph compilation order

- **Statement:** `Zenith_CompileAnimationGraph` emits the node table in post order with subtree sizes, computes the pose-stack depth (at most 16) and refuses a node shared by two parents.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphCompiler_PostOrderAndSubtreeSizes`, `Test_GraphCompiler_PoseStackDepth` and `Test_GraphCompiler_SharedNodeRefused` (`ZenithAuthoring_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Compile steps"; Zenith2/Docs/Design/ANIM/Graph.md → "Graph model"

### Z2-R-ANIM-067 Compile diagnostics

- **Statement:** One compile reports every diagnostic of the document (code, node path, message) rather than stopping at the first, and returns the blend-space triangulations for display, so the editor shows all problems at once.
- **Covers:** Z2-I-ANIM-04, Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphCompiler_AllErrorsInOneCompile` (`ZenithAuthoring_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Compile diagnostics"

### Z2-R-ANIM-068 Deterministic graph compilation

- **Statement:** The same document, masks, skeleton properties and clip information compile to byte-identical graph files and the same graph hash.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphCompiler_Deterministic` (`ZenithAuthoring_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Compile steps"

### Z2-R-ANIM-069 Parameters sorted and reserved

- **Statement:** Graph parameters (float, int, bool, trigger) are stored sorted by name hash with their defaults and ranges; names with the `Zenith.` prefix are reserved, added by the compiler when referenced (`Zenith.RagdollRecover`, `Zenith.RagdollFacing`, `Zenith.Speed`, `Zenith.Teleported`) and refused in a document.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphCompiler_ParametersSortedAndReserved` and `Test_AnimationGraphAsset_ParameterLookup` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Parameter kinds"; Zenith2/Docs/Design/ANIM/Graph.md → "Reserved parameters"

### Z2-R-ANIM-070 Parameter commands at sync points

- **Statement:** Parameters change only through ANIM's command kinds played back at sync points in merge order, so a phase-2 write is visible to the same tick's phase-3 evaluation; a command naming an unknown parameter or the wrong kind is dropped and counted.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimGraph_ParameterCommandAppliedNextPhase` and `Test_AnimGraph_UnknownParameterCommandDropped` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Parameter commands"; Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationCommands header"

### Z2-R-ANIM-071 Update pass

- **Statement:** Each evaluation runs an update pass over the node table (times, weights, sync phases, transitions, events) before the pose pass, so times and events are independent of which children the pose pass culls.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-ANIM-002
- **Acceptance evidence:** `Test_AnimGraph_UpdatePassAdvancesTimes` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Update pass"; Zenith2/Docs/Design/ANIM/Graph.md → "Evaluation algorithm"

### Z2-R-ANIM-072 Pose pass without heap allocation

- **Statement:** The pose pass evaluates nodes in post order on a pose stack of at most 16 entries in the sub-job's node arena, allocating nothing from the heap per evaluation.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** Z2-B-ANIM-002, Z2-B-CORE-005
- **Acceptance evidence:** `Test_GraphCompiler_PoseStackDepth` and `Test_AnimationGraphAsset_RejectsPoseStackMismatch` (`Test` step); `Zenith_Bench_AnimEvaluation` with `--smoke`.
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Pose pass"; Zenith2/Docs/Design/ANIM/Graph.md → "Pose stack"

### Z2-R-ANIM-073 Weight culling

- **Statement:** A child whose path weight is below 1e-4 is not sampled and the blend renormalises over the evaluated children, while times, sync phases, events and root motion are unchanged.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-ANIM-002
- **Acceptance evidence:** `Test_AnimGraph_WeightCullingKeepsTimesAndEvents` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Weight culling"

### Z2-R-ANIM-074 Local-space pose blend keeps pivots

- **Statement:** Two poses blend per joint in local space (translation lerp, rotation nlerp with sign alignment, scale lerp, curves lerp), which is rigid per joint, so a joint lying on its parent's rotation pivot stays fixed through the blend.
- **Covers:** Z2-I-ANIM-04, Z2-I-ANIM-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_PoseBlend_JointOnPivotFixed` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Local-space blend"

### Z2-R-ANIM-075 N-way blend

- **Statement:** An N-way blend normalises its weights and equals the pairwise fold of the same inputs within 1e-5, counting degenerate rotation sums with `anim.blend.degenerate_delta` rather than producing NaN.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PoseBlend_NWayMatchesPairwise` and `Test_PoseBlend_DegenerateRotationCounted` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "N-way blend"

### Z2-R-ANIM-076 Local to model conversion

- **Statement:** `Zenith_LocalToModel` and `Zenith_ModelToLocal` convert in one parent-before-child pass, O(J), matching a reference composition within 1e-5.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-034
- **Budgets:** none
- **Acceptance evidence:** `Test_LocalToModel_MatchesReference` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Model-space conversion"

### Z2-R-ANIM-077 Blend1D weights

- **Statement:** A 1D blend space weights its two neighbouring samples linearly by the parameter, clamping outside the sample range.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_BlendSpace1D_Weights` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Blend1D weights"; Zenith2/Docs/Design/ANIM/Formats.md → "Blend space section"

### Z2-R-ANIM-078 Blend2D triangulation

- **Statement:** The compiler triangulates a 2D blend space's samples with a deterministic Delaunay triangulation and refuses collinear or duplicate samples.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_BlendSpace2D_TriangulationDeterministic` and `Test_BlendSpace2D_DegenerateRefused` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Blend2D triangulation"

### Z2-R-ANIM-079 Blend2D weights

- **Statement:** A 2D blend space weights the samples of the containing triangle barycentrically, projects a point outside the hull to the nearest hull point, and gives weights continuous across triangle edges.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_BlendSpace2D_OutsideHullProjects` and `Test_BlendSpace2D_WeightsContinuousAcrossEdges` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Blend2D weights"

### Z2-R-ANIM-080 Switch and time-scale nodes

- **Statement:** A switch node changes its selected child by a crossfade or, with its inertial flag, at once under an instance-level inertial take, and a time-scale node scales its subtree's advance by its parameter, as "Node catalogue" defines.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_SwitchNode_InertialOnChange` and `Test_TimeScaleNode_ScalesAdvance` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Node catalogue"; Zenith2/Docs/Design/ANIM/Graph.md → "Inertial blending"

### Z2-R-ANIM-081 Slots

- **Statement:** A named slot plays a one-shot clip (`Zenith_RecordPlaySlot`) after the clip binds, blends it in and out over its blend times, switches a playing slot to a new clip at once under an instance-level inertial take of the blend-in time (the 32-byte slot state holds one clip), and `Zenith_RecordSetSlotTime` moves its time without firing the events it skips; compiled graphs are never mutated.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimSlot_PlayAfterBinding`, `Test_AnimSlot_BlendInOut`, `Test_AnimSlot_ClipChangeIsInertial` and `Test_AnimSlot_SetTimeSkipsEvents` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Slots"; Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationCommands header"; Zenith2/Docs/Design/ANIM/Graph.md → "Inertial blending"

### Z2-R-ANIM-082 Slot root motion

- **Statement:** A slot that allows root motion contributes its clip's delta with the slot's path weight to the instance's root-motion blend.
- **Covers:** Z2-I-ANIM-04, Z2-I-ANIM-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimSlot_RootMotionWeighted` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Slots"; Zenith2/Docs/Design/ANIM/Graph.md → "Root motion blending"

### Z2-R-ANIM-083 State machine tables

- **Statement:** State machines are compiled into the state, transition and condition tables of "State machine tables", with any-state transitions evaluated first and each list in priority order.
- **Covers:** Z2-I-ANIM-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_StateMachine_AnyStateFirstThenPriority` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "State machine tables"; Zenith2/Docs/Design/ANIM/Formats.md → "State machine sections"

### Z2-R-ANIM-084 Transition conditions

- **Statement:** Conditions evaluate exactly as `Zenith_ConditionOp` defines (greater, less, equal, not equal on the stored f32; true, false; triggered; state time at least; clip ready; event fired), up to eight per transition, all of which must pass.
- **Covers:** Z2-I-ANIM-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_StateMachine_ConditionOperators` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Transition evaluation"

### Z2-R-ANIM-085 Exit times

- **Statement:** A transition with an exit time passes on the evaluation whose advance crosses that normalised time, including across a loop wrap, and never twice for one crossing.
- **Covers:** Z2-I-ANIM-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_StateMachine_ExitTimeCrossing` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Transition evaluation"

### Z2-R-ANIM-086 Interrupt sources

- **Statement:** While transitioning, only the candidates of the newest record's interrupt source (`NONE`, `CURRENT`, `NEXT`, `CURRENT_THEN_NEXT`, `NEXT_THEN_CURRENT`) and the any-state transitions are evaluated.
- **Covers:** Z2-I-ANIM-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_StateMachine_InterruptSources` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Transition evaluation"

### Z2-R-ANIM-087 Crossfade fold

- **Statement:** Active transition records (at most four) fold oldest first with linear or smoothstep curves, and the state weights the update pass assigns sum to one.
- **Covers:** Z2-I-ANIM-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-ANIM-002
- **Acceptance evidence:** `Test_StateMachine_CrossfadeWeightsSumToOne` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Transition blending"

### Z2-R-ANIM-088 Interrupt overflow becomes inertial

- **Statement:** A crossfade interrupt taken while four records are active clears the records and requests one instance-level inertial take of the candidate's duration, measured against the instance's latest output, so the crossfade stack is bounded and the pose does not jump beyond the quintic's own first step.
- **Covers:** Z2-I-ANIM-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-ANIM-002
- **Acceptance evidence:** `Test_StateMachine_FifthInterruptBecomesInertial` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "State machine state table"; Zenith2/Docs/Design/ANIM/Graph.md → "Inertial blending"

### Z2-R-ANIM-089 Inertial blending

- **Statement:** Inertialisation is per instance: every take (an inertial transition or switch, the fifth interrupt, `ForceState`, a slot clip change, a cinematic skip, a motion-matching switch, a hot reload) records only a pending duration, and one inertialiser per instance, run after the pose pass on the whole graph output and before motion warping and IK, reads the instance's current and previous history poses (before the history rotates) and the last evaluation's Δt, stores `(x0, v0, t1)` per channel in the instance's one `48 × J + 4 × C + 16`-byte buffer (present when the graph has a switch, state machine, slot or motion-matching node), adds the offset to that output from the take's own evaluation on, and decays it with Bollo's quintic recomputed in double precision from that state, reaching zero offset at t1 and never overshooting the clamp, so no layer above the taking node is popped or applied twice.
- **Covers:** Z2-I-ANIM-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_InertialBlend_QuinticMatchesReference`, `Test_InertialBlend_OffsetReachesZeroAtT1`, `Test_InertialBlend_VelocityFromHistoryAtReducedRate`, `Test_InertialBlend_NoPopUnderOverrideLayer` and `Test_InertialBlend_ForceStateWithoutInertialTransitions` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Inertial blending"; Zenith2/Docs/Design/ANIM/Runtime.md → "Animation.Evaluate"; Zenith2/Docs/Design/ANIM/Runtime.md → "Pose store layout"

### Z2-R-ANIM-090 Zero-duration chains

- **Statement:** Zero-duration transitions chain at most four times per evaluation, after which the machine stops in the reached state until the next evaluation.
- **Covers:** Z2-I-ANIM-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_StateMachine_ZeroDurationChainBounded` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Transition evaluation"; Zenith2/Docs/Design/ANIM/Graph.md → "State machine state table"

### Z2-R-ANIM-091 Forced state and reset

- **Statement:** `Animation.ForceState` enters the named state ignoring conditions under an instance-level inertial take of its seconds (none for 0 s), whether or not the machine has inertial transitions, and a reset (teleport without keep-phase, rebind, `Animation.ResetGraph`) returns every machine to its entry state with no records and time zero and clears the instance's inertial state.
- **Covers:** Z2-I-ANIM-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StateMachine_ForceStateAndReset` and `Test_InertialBlend_ForceStateWithoutInertialTransitions` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "State machine state table"; Zenith2/Docs/Design/ANIM/Graph.md → "Inertial blending"

### Z2-R-ANIM-092 Triggers

- **Statement:** A trigger holds for its declared count of evaluations (1 to 255), is consumed by the first transition taken with a `TRIGGERED` condition naming it, and otherwise clears when its counter reaches zero.
- **Covers:** Z2-I-ANIM-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimGraph_TriggerHoldAndConsume` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Triggers"

### Z2-R-ANIM-093 State changed events

- **Statement:** Each taken transition emits one `Zenith.Animation.StateChanged` event naming the machine, source and destination states, delivered with the notify events.
- **Covers:** Z2-I-ANIM-05, Z2-I-ANIM-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_StateMachine_StateChangedEventEmitted` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Transition evaluation"; Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationEvents header"

### Z2-R-ANIM-094 Local additive application

- **Statement:** Applying a local-space additive pose at weight w composes `rest-relative delta^w` onto each joint's local transform, and applying a pose's own delta to its reference reproduces it within 1e-6.
- **Covers:** Z2-I-ANIM-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditivePose_LocalRoundTrip` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Additive application"

### Z2-R-ANIM-095 Model-space additive application

- **Statement:** A model-space additive rotates each joint about its own model-space origin, so additive leans and aims do not translate joints off their pivots.
- **Covers:** Z2-I-ANIM-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditivePose_ModelSpaceAboutJointOrigin` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Additive application"

### Z2-R-ANIM-096 Layer blend modes

- **Statement:** Layers apply in index order above the base layer in override or additive mode with their weight and mask, and a layer whose weight is zero is not evaluated.
- **Covers:** Z2-I-ANIM-06, Z2-I-ANIM-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-ANIM-002
- **Acceptance evidence:** `Test_AnimLayers_OverrideAndAdditiveOrder` and `Test_AnimLayers_ZeroWeightLayerSkipped` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Layer blend modes"; Zenith2/Docs/Design/ANIM/Formats.md → "Mask and layer sections"

### Z2-R-ANIM-097 Mask documents and compiled weights

- **Statement:** A mask document (`.animmask.json`) assigns per-joint weights with recursive entries, each entry overriding the entries before it for the joints they reach; the compiler bakes one weight per joint into the graph.
- **Covers:** Z2-I-ANIM-07, Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079, D-SEL-JSON (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphCompiler_MaskCompiled` and `Test_AnimMask_RecursiveEntriesOverride` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Mask document schema"; Zenith2/Docs/Design/ANIM/Graph.md → "Mask weights"

### Z2-R-ANIM-098 Model-space rotation blending

- **Statement:** A layer flagged model-space rotation blends masked joints' rotations in model space, so an upper-body layer keeps its world orientation while the base layer turns the hips.
- **Covers:** Z2-I-ANIM-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PoseMasked_ModelRotationWorldStable` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Model-space rotation blending"

### Z2-R-ANIM-099 Layered blend node

- **Statement:** The layered-blend node evaluates its own layers with the same rules as the graph's layers and gives the same pose for the same inputs.
- **Covers:** Z2-I-ANIM-06, Z2-I-ANIM-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_LayeredNode_MatchesLayers` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Node catalogue"; Zenith2/Docs/Design/ANIM/Graph.md → "Layer blend modes"

### Z2-R-ANIM-100 Sync group leadership

- **Statement:** Each sync group's leader is its highest-weight member (ties by node index), followers take the leader's phase, and a member joining the group enters in step.
- **Covers:** Z2-I-ANIM-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_SyncGroup_LeaderHighestWeight` and `Test_SyncGroup_JoinerEntersInStep` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Sync groups"

### Z2-R-ANIM-101 Normalised-time sync

- **Statement:** In normalised mode followers advance to the leader's normalised time, so clips of different lengths keep one cadence.
- **Covers:** Z2-I-ANIM-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_SyncGroup_NormalisedCadence` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Normalised-time sync"

### Z2-R-ANIM-102 Marker sync

- **Statement:** In marker mode followers align to the leader's position between matching sync markers; a follower lacking the leader's markers falls back to normalised sync and counts `anim.sync.marker_fallback_delta`.
- **Covers:** Z2-I-ANIM-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_SyncGroup_MarkerAlignment` and `Test_SyncGroup_MarkerFallbackCounted` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Marker sync"; Zenith2/Docs/Design/ANIM/Formats.md → "Event and marker sections"

### Z2-R-ANIM-103 Event crossing rules

- **Statement:** An instant event fires once per crossing of its time over the advanced interval, including each loop wrap and backward playback, and a clip node's first evaluation includes time 0.
- **Covers:** Z2-I-ANIM-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimEvents_LoopWrapFiresOncePerCrossing`, `Test_AnimEvents_BackwardPlayback` and `Test_AnimEvents_FirstEvaluationIncludesZero` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Event firing rules"

### Z2-R-ANIM-104 Notify state pairing

- **Statement:** A notify state fires `BEGIN` and `END` in pairs; when its clip stops contributing (weight zero, state left, slot emptied, reset) the open state fires `END` in that evaluation.
- **Covers:** Z2-I-ANIM-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimEvents_NotifyStateEndOnExit` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Event firing rules"

### Z2-R-ANIM-105 Event fire modes

- **Statement:** Events fire by mode: `ALWAYS`, `LEADER_ONLY` (only from the sync leader or an ungrouped clip) and `WEIGHTED` (only at path weight at least the event's minimum).
- **Covers:** Z2-I-ANIM-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimEvents_WeightGate` and `Test_AnimEvents_LeaderOnly` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Event firing rules"

### Z2-R-ANIM-106 Event order and cap

- **Statement:** Fired events are ordered by layer, node, crossing order and event index, and at most 64 fire per instance per evaluation; the rest are dropped in that order and counted.
- **Covers:** Z2-I-ANIM-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimEvents_OrderAndCap` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Event firing rules"

### Z2-R-ANIM-107 Event delivery

- **Statement:** Each fired event is emitted as one `Zenith.Animation.Notify` ECS event carrying the entity, event name hash, the name interned in the world's binding-cache entry (the shared clip is never written), kind, payload and sub-tick time, merged at phase 3's sync point and delivered in phase 9 of the same tick to C++ readers and scripts.
- **Covers:** Z2-I-ANIM-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimEvents_DeliveredInPhaseNine`, `Test_AnimBinding_EventNamesPerWorld` and `Test_AnimationBindings_EventsReachScripts` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Event delivery"; Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationEvents header"; Zenith2/Docs/Design/ANIM/Runtime.md → "Asset bindings"

### Z2-R-ANIM-108 Root motion sampling

- **Statement:** A root-motion delta over an advance is `Q(t)⁻¹ · Q(t')` with each whole loop contributing `Q(D)`, and a backward advance gives the inverse of the forward delta.
- **Covers:** Z2-I-ANIM-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_RootMotion_DeltaWithWraps` and `Test_RootMotion_BackwardIsInverse` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Root motion sampling"

### Z2-R-ANIM-109 Root motion blended rigidly

- **Statement:** Root-motion contributions of the base layer and root-motion slots blend as dual quaternions with normalised weights; blending two deltas that share a pivot keeps the pivot, which a linear-translation blend does not.
- **Covers:** Z2-I-ANIM-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_RootMotion_BlendViaDualQuaternions`, `Test_DualQuaternion_BlendKeepsSharedPivot` and the negative control `Test_DualQuaternion_LinearTranslationBlendMovesPivot` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Root motion blending"; Zenith2/Docs/Design/ANIM/Graph.md → "Rigid delta blending"

### Z2-R-ANIM-110 Root motion published never applied

- **Statement:** The instance's delta is published in entity-local space into `Zenith.Animation.RootMotion` in phase 3, its `DeltaPosition` already multiplied by the entity's `Zenith.LocalTransform` scale so that every reader applies it with the entity's rotation alone, and phase 3 never applies it; mode `IGNORE` publishes identity.
- **Covers:** Z2-I-ANIM-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_RootMotion_PhaseThreeNeverApplies`, `Test_RootMotion_IgnoreModeZero` and `Test_RootMotion_EntityScaleAppliedOnce` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Root motion publication"; Zenith2/Docs/Design/ANIM.md → "Field authority table"; Zenith2/Docs/Design/ANIM.md → "Zenith_AnimationComponents header"; Zenith2/Docs/Design/ANIM/Runtime.md → "Animation.Evaluate"

### Z2-R-ANIM-111 Root motion applied in phase 4

- **Statement:** In phase 4 of the tick that published it, a root-motion delta moves an entity without a physics character controller through `Animation.ApplyRootMotion` (`p + r · Δp` with the pre-scaled `DeltaPosition`, `r · Δq`, as `SetProperty` commands applied at sync point 4) and is left to PHYS's character-controller node for an entity with one, which applies the same `Zenith_Rotate(q₀, DeltaPosition)`, so the entity's scale is applied once either way; the two node queries partition the rows, so no entity is moved twice and no ordering edge is needed (row AN6).
- **Covers:** Z2-I-ANIM-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, D-PHASES (open, Stage 1)
- **Budgets:** Z2-B-GAME-001
- **Acceptance evidence:** `Test_RootMotion_AppliedInPhaseFourWithoutController` and `Test_RootMotion_ControllerEntitiesLeftToPhysics` and `Test_RootMotion_EntityScaleAppliedOnce` (`Test` step); `Test_AnimIntegration_RootMotionThroughMovement` in `Zenith_Integration_Animation` (`Test` step), `blocks`-linked from PHYS's character-controller ticket (row AN6).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Animation.ApplyRootMotion"; Zenith2/Docs/Design/ANIM/Graph.md → "Root motion publication"; Zenith2/Docs/Design/ANIM/Tests.md → "Integration scenarios"

### Z2-R-ANIM-112 Ragdoll ownership precedence

- **Statement:** `Animation.PostPhysicsOwnership` and `Animation.RagdollOwnership` write `Zenith.Animation.Pose.PhysicsWeight` under the precedence (Cinematic, Ragdoll, Animation), the ragdoll writer after PHYS's weight advance of the same tick, so a cinematic writer overrides a ragdoll and a ragdoll overrides animation in the same tick.
- **Covers:** Z2-I-ANIM-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-ANIM-003
- **Acceptance evidence:** `Test_RagdollOwnership_PrecedenceCinematicRagdollAnimation` and `Test_RagdollOwnership_ReadsThisTickWeight` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Animation.PostPhysicsOwnership"; Zenith2/Docs/Design/ANIM/Runtime.md → "Animation.RagdollOwnership"; Zenith2/Docs/Design/ANIM.md → "Field authority table"; Zenith2/Docs/Design/ANIM/Runtime.md → "Systems"

### Z2-R-ANIM-113 Ragdoll enter

- **Statement:** At PHYS's ragdoll enter playback, ANIM's pose source returns from `GetEntryModelPose` the instance's final model pose, which no node writes between the latest phase 6 and the playback's sync point, so PHYS creates the ragdoll bodies at the pose the character shows (row AN6).
- **Covers:** Z2-I-ANIM-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** `Test_RagdollEnter_SnapshotsFinalPose` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Ragdoll enter"; Zenith2/Docs/Design/ANIM/Runtime.md → "Lifecycle listener"

### Z2-R-ANIM-114 Ragdoll blend

- **Statement:** `Animation.PostPhysicsBlend` blends each joint's animated local transform with its ragdoll local transform by the arbitrated weight, rigid per joint, so every shared pivot stays fixed; O(J + bodies) per instance.
- **Covers:** Z2-I-ANIM-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-003
- **Budgets:** Z2-B-ANIM-003, Z2-B-PHYS-002
- **Acceptance evidence:** `Test_RagdollBlend_LocalBlendKeepsPivots` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Ragdoll blend"; Zenith2/Docs/Design/ANIM/Runtime.md → "Animation.PostPhysicsBlend"

### Z2-R-ANIM-115 Ragdoll recover facing

- **Statement:** At `LIFECYCLE_EVENT_RAGDOLL_RECOVER` the listener sets `Zenith.RagdollFacing` from the hips' final rotation (face down below −0.25) and fires `Zenith.RagdollRecover`, and plays a named get-up clip in the `Zenith.GetUp` slot when the graph has one.
- **Covers:** Z2-I-ANIM-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_RagdollRecover_FacingAndTrigger` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Ragdoll recover"

### Z2-R-ANIM-116 Ragdoll recover pivot matching

- **Statement:** The recover blend applies the rigid hips offset `O = H_ragdoll · H_anim⁻¹` (after PHYS has placed the entity root under the ragdoll, and bounded to 0.5 m and 90°, beyond which no offset is used and the recovery is counted) to the animated pose as the screw power `Pow(O, 1 − p)` before blending with the ragdoll pose by the arbitrated weight, with the fraction `p = 1 − w_r / w_1` derived from PHYS's own ragdoll weight `w_r` (`w_1` its value at ANIM's first blend after the event, which PHYS leaves at the playback's weight, 1 from ACTIVE, by the orchestrator's PHYS-RECOVER ruling pending PHYS revision 1), so the ragdoll's contribution is never offset, at the first recover tick p = 0, the animated input's hips equal the ragdoll's exactly and a recovery from ACTIVE shows exactly the ragdoll pose, at the tick PHYS's weight reaches 0 the final pose is the animated pose with no residual offset, the hips pivot moves along one screw motion and no joint jumps.
- **Covers:** Z2-I-ANIM-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_RagdollRecover_OffsetBlendKeepsHipsPivot` (closed-form oracle), its negative control `Test_RagdollRecover_LinearOffsetBlendMovesPivot`, `Test_RagdollRecover_NoDoubleOffsetAtFirstTick` (driven by PHYS's real `Physics.RagdollBlend` weight advance in a Jolt world) and `Test_RagdollRecover_LargeOffsetRejectedCounted` (`Test` step); the entity placement and the weight schedule are PHYS's (row AN6).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Ragdoll recover"; Zenith2/Docs/Design/ANIM/Graph.md → "Rigid delta blending"; Zenith2/Docs/Design/ANIM/Runtime.md → "Animation.PostPhysicsBlend"; Zenith2/Docs/Design/ANIM/Runtime.md → "Systems"

### Z2-R-ANIM-117 Ragdoll round trip

- **Statement:** A character animates, enters ragdoll mid-locomotion, settles, recovers into a get-up and resumes locomotion with no joint jump above 1e-3 m between ticks, on Jolt in CI and on the backend D-SEL-PHYSX-ANDROID selects on the S24.
- **Covers:** Z2-I-ANIM-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003, Z2-D-080, D-SEL-PHYSX-ANDROID (open, Stage 1)
- **Budgets:** Z2-B-ANIM-003, Z2-B-PHYS-002
- **Acceptance evidence:** `Test_AnimIntegration_RagdollRoundTrip` and ANIM's assertions in `Test_Scenario_TeleportDuringRagdoll` (`Zenith_Integration_Animation`, `Test` step and stage acceptance), `blocks`-linked from PHYS's ragdoll ticket (row AN6).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Ragdoll transitions"; Zenith2/Docs/Design/ANIM/Tests.md → "Integration scenarios"

### Z2-R-ANIM-118 Teleport handling

- **Statement:** `LIFECYCLE_EVENT_TELEPORTED` resets the graph (unless `TELEPORT_FLAG_KEEP_ANIMATION_PHASE`), zeroes pending root motion, fires `Zenith.Teleported` and flags the next extracted pose `FLUX_SKINNED_POSE_FLAG_HISTORY_RESET`.
- **Covers:** Z2-I-ANIM-10, Z2-I-ANIM-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimLifecycle_TeleportResetsRootAndHistory`, `Test_AnimLifecycle_TeleportKeepPhase` and `Test_AnimExtract_HistoryResetOnTeleport` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Lifecycle listener"

### Z2-R-ANIM-119 Cinematic skip and interrupt

- **Statement:** A cinematic skip moves instances to their post-skip state through an instance-level inertial take of 0.15 s (its cinematic slots emptied at once) with no visible pop, and a cinematic interrupt blends the cinematic slot out over its blend-out time (row AN9).
- **Covers:** Z2-I-ANIM-05, Z2-I-ANIM-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimLifecycle_CinematicSkipInertialNoPop` and `Test_AnimLifecycle_CinematicInterruptBlendsOut` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Lifecycle listener"; Zenith2/Docs/Design/ANIM/Graph.md → "Inertial blending"

### Z2-R-ANIM-136 Extractor placement and reads

- **Statement:** `Animation.ExtractPoses` is registered with `AddExtractor` and `Before("Render.Seal@W0", true)`, declares exactly the reads of "Extractor declared reads", and writes one `Flux_RenderSkinnedPose` per visible skinned mesh per frame; O(J) per mesh.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** Z2-B-RPROD-003
- **Acceptance evidence:** `Test_AnimationModule_HeadlessRuns` (registration) and `Test_AnimIntegration_SkinnedPoseInRenderWorld` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Extractor declared reads"; Zenith2/Docs/Design/ANIM/Runtime.md → "Extraction algorithm"

### Z2-R-ANIM-137 Pose key matches the instance extractor

- **Statement:** Each skinned pose's key is `Flux_MakeEntityInstanceKey` of the entity's persistent ID and world tag, the key RPROD's engine instance extractor computes for the same entity (row RP29).
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimExtract_KeyMatchesInstanceExtractor` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Extraction algorithm"

### Z2-R-ANIM-138 Pose interpolation

- **Statement:** The rendered pose interpolates the instance's last two final poses by the frame's interpolation alpha and the instance's rate divisor, so a full-rate instance shows `lerp(pose(T − 1), pose(T), α)` and a reduced-rate instance moves smoothly.
- **Covers:** Z2-I-ANIM-04, Z2-I-CHAR-04 (CHAR)
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimExtract_InterpolationFullRate` (`Test` step) and, from Stage 4, `Test_AnimExtract_InterpolationReducedRate`.
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Pose interpolation"

### Z2-R-ANIM-139 Extraction refusals

- **Statement:** A mesh whose skeleton differs from the instance's is refused and counted, a full skinned-pose slot table is counted, and a headless world (no runtime mesh) writes no pose with no per-frame retry beyond the availability check.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-050, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimExtract_SkeletonMismatchRefused`, `Test_AnimExtract_SlotFullCounted`, `Test_AnimExtract_HeadlessWritesNothing` and `Test_AnimExtract_RemapCacheWorkerIndependent` (the remap cache and the once-only refusals are mutated only by the extractor node's body, so the results are worker-independent; also under `win-asan`) (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Extraction algorithm"; Zenith2/Docs/Design/ANIM.md → "Failure table"

### Z2-R-ANIM-147 Animator save records

- **Statement:** Through the world's `Zenith_SaveParticipant`, ANIM saves its world prologue (binding cache, stream residency, and every request with a completion ID that has not published, each marked issued, parked success or parked failure) and one `Zenith_AnimatorSaveRecord` per instance (version 1, little-endian, field by field, ascending simulation ID, including the inertial state and the pose history), reports every capture EXACT, and restores without allocating a completion ID: it re-acquires the recorded assets and the assets of parked binding successes, reads resident and parked-success chunks into new blocks behind SAVE's readiness step (listing what it waits for through `ListRestoreWaits`), binds every instance before the first restored tick, declares `ASYNC_SOURCE_ANIMATION` in `GetReissuedSourceKinds` and re-issues only the issued requests under their recorded IDs, each after `MarkReissued`, and never re-issues a parked one, so the results SAVE restores as completed publish at t + 1 onto the state the uninterrupted run had and the next ticks match it (rows AN8, AN17).
- **Covers:** Z2-I-ANIM-04, Z2-I-ANIM-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimSave_RecordRoundTrip`, `Test_AnimSave_RestoreBindsBeforeFirstTick`, `Test_AnimSave_RestoreAllocatesNoCompletionID`, `Test_AnimSave_InFlightReissuedWithRecordedIDs`, `Test_AnimSave_ParkedCompletionPublishesAfterRestore`, `Test_AnimSave_ReissueDeclaredAndMarked`, `Test_AnimSave_StreamResidencyRestored` and `Test_AnimSave_RestoreBindFailureCounted` (`Test` step), `blocks`-linked from SAVE's `Zenith_SaveParticipant` ticket (SAVE's row SV17) and its restore-readiness ticket (`IsRestoreReady`, `ListRestoreWaits`; SAVE revision 2).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Save state"; Zenith2/Docs/Design/ANIM.md → "Design rulings owned by ANIM"

### Z2-R-ANIM-148 Restore after a graph edit

- **Statement:** A save record restores onto an edited graph by matching nodes by stable ID, dropping state of removed nodes and resetting added ones.
- **Covers:** Z2-I-ANIM-04, Z2-I-ANIM-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimSave_RestoreAfterGraphEdit` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Save state"; Zenith2/Docs/Design/ANIM/Graph.md → "Graph hot reload"

### Z2-R-ANIM-151 Buffered Luau setters

- **Statement:** The `Animation` binding table's setters (`SetFloat`, `SetInt`, `SetBool`, `SetTrigger`, `ResetTrigger`, `PlaySlot`, `StopSlot`, `ForceState`, `ResetGraph`, `SetIKGoal`, `SetWarpTarget`, `SetCurveOverride`, `PrefetchClip`) record buffered commands, and a failing invocation discards its command (row AN10).
- **Covers:** Z2-I-ANIM-04, Z2-I-ANIM-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimationBindings_SettersBufferCommands` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Luau bindings"

### Z2-R-ANIM-152 Read-world Luau getters

- **Statement:** The getters (`GetFloat`, `GetInt`, `GetBool`, `GetState`, `GetSocket`, `GetCurve`, `IsClipReady`, `GetClipLength`) read the values committed by the latest evaluation and declare the resource IDs they read.
- **Covers:** Z2-I-ANIM-04, Z2-I-ANIM-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimationBindings_GettersReadCommitted` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Luau bindings"

### Z2-R-ANIM-153 Luau misuse errors

- **Statement:** Each binding raises the controlled Luau error of its table row for invalid input (non-finite values, unknown names, out-of-range indices and times) with its misuse code of ANIM's block ZS2000–ZS2099 ("Misuse codes"; assigned by the orchestrator's Q81 ruling; X244 added the row to `Zenith2/Docs/Scripting/MisuseCatalogue.md → "Code ranges"`) and never asserts.
- **Covers:** Z2-I-ANIM-04, Z2-I-ANIM-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimationBindings_MisuseRaisesLuauError` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Luau bindings"; Zenith2/Docs/Design/ANIM.md → "Luau errors and asserts"; Zenith2/Docs/Design/ANIM/Runtime.md → "Misuse codes"

### Z2-R-ANIM-154 Events reach scripts

- **Statement:** Scripts receive `Zenith.Animation.Notify` and state-changed events in phase 9 through SCRIPT's event delivery.
- **Covers:** Z2-I-ANIM-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimationBindings_EventsReachScripts` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Luau bindings"; Zenith2/Docs/Design/ANIM/Graph.md → "Event delivery"

### Z2-R-ANIM-155 Counters

- **Statement:** Every counter of "Counters" is published with its kind and `_delta` naming, including evaluations per tier, stalls, dropped events, binding failures and resident bytes.
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-02, Z2-I-ANIM-03, Z2-I-ANIM-04, Z2-I-ANIM-05, Z2-I-ANIM-06, Z2-I-ANIM-07, Z2-I-ANIM-08, Z2-I-ANIM-09, Z2-I-ANIM-10, Z2-I-ANIM-11, Z2-I-ANIM-12, Z2-I-ANIM-13, Z2-I-ANIM-14, Z2-I-ANIM-15, Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** Z2-B-ANIM-001, Z2-B-ANIM-002
- **Acceptance evidence:** `Test_AnimCounters_Published` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM.md → "Counters"

### Z2-R-ANIM-156 Profiler zones

- **Statement:** The phase nodes, their sub-jobs and the extractor open the profiler zones of "Profiler zones", which the budget rows' MP-CPUZONE measurements read.
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-02, Z2-I-ANIM-03, Z2-I-ANIM-04, Z2-I-ANIM-05, Z2-I-ANIM-06, Z2-I-ANIM-07, Z2-I-ANIM-08, Z2-I-ANIM-09, Z2-I-ANIM-10, Z2-I-ANIM-11, Z2-I-ANIM-12, Z2-I-ANIM-13, Z2-I-ANIM-14, Z2-I-ANIM-15, Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-ANIM-002, Z2-B-ANIM-003
- **Acceptance evidence:** `Zenith_Bench_AnimEvaluation` with `--smoke` (`Test` step) reporting the zones' spans.
- **Design:** Zenith2/Docs/Design/ANIM.md → "Profiler zones"; Zenith2/Docs/Design/ANIM.md → "On-screen profiler rows"

### Z2-R-ANIM-157 Dump and debug state

- **Statement:** `anim.dump` writes an entity's machines, states, transitions, parameters and rate, and `Zenith_AnimationWorld::GetDebugState` fills every member of `Zenith_AnimDebugState` with the same state for EDIT's live debugger and CHAR's tools (truncation flagged), as `GetStats` fills `Zenith_AnimationWorldStats` for the profiler panel.
- **Covers:** Z2-I-ANIM-04, Z2-I-ANIM-05, Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimDump_ContainsState` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM.md → "Frame-dump entries"; Zenith2/Docs/Design/ANIM.md → "Debug views"; Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationWorld header"

### Z2-R-ANIM-158 Configuration keys

- **Statement:** ANIM registers exactly the keys of "Configuration keys" with their types, defaults, ranges and admitted layers; simulation-affecting keys exclude the quality-tier and user layers and are written into every replay header.
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-02, Z2-I-ANIM-03, Z2-I-ANIM-04, Z2-I-ANIM-05, Z2-I-ANIM-06, Z2-I-ANIM-07, Z2-I-ANIM-08, Z2-I-ANIM-09, Z2-I-ANIM-10, Z2-I-ANIM-11, Z2-I-ANIM-12, Z2-I-ANIM-13, Z2-I-ANIM-14, Z2-I-ANIM-15, Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimSignificance_IgnoresQualityTier`, `Test_AnimBinding_InFlightCapDefersInOrder` (`animation.bindingQueueCapacity` and `animation.streaming.maxReadsInFlight`, both simulation-affecting) and `Test_AnimDeterminism_ReplayBitIdentical` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM.md → "Configuration keys"

### Z2-R-ANIM-159 Memory consumer reports resident bytes

- **Statement:** The module registers one memory consumer for `MEMORY_SUBSYSTEM_SIMULATION` whose resident bytes are the sum of instance blocks, records, stream caches, binding caches, retarget maps and remap tables, published as `anim.memory_bytes`.
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-02, Z2-I-ANIM-03, Z2-I-ANIM-04, Z2-I-ANIM-05, Z2-I-ANIM-06, Z2-I-ANIM-07, Z2-I-ANIM-08, Z2-I-ANIM-09, Z2-I-ANIM-10, Z2-I-ANIM-11, Z2-I-ANIM-12, Z2-I-ANIM-13, Z2-I-ANIM-14, Z2-I-ANIM-15, Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** Z2-B-ANIM-001
- **Acceptance evidence:** `Test_AnimationMemory_ReportsResidentBytes` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Memory consumer"

### Z2-R-ANIM-160 Animation sheds nothing under pressure

- **Statement:** Animation is simulation memory and sheds nothing at any pressure level, so memory pressure never changes a simulation result.
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-02, Z2-I-ANIM-03, Z2-I-ANIM-04, Z2-I-ANIM-05, Z2-I-ANIM-06, Z2-I-ANIM-07, Z2-I-ANIM-08, Z2-I-ANIM-09, Z2-I-ANIM-10, Z2-I-ANIM-11, Z2-I-ANIM-12, Z2-I-ANIM-13, Z2-I-ANIM-14, Z2-I-ANIM-15, Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-ANIM-001
- **Acceptance evidence:** `Test_AnimationMemory_ForcedCriticalShedsNothing` (`Test` step); ANIM's row (subsystem `MEMORY_SUBSYSTEM_SIMULATION`, consumer `anim`, nothing shed) appended to `Zenith2/Tests/Memory/Internal/Zenith_MemorySheddingCases.h` in the memory-consumer ticket's diff and run by the memory integration suite (`Test` step; A5 row CB8, Z2-R-CORE-152).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Memory consumer"

### Z2-R-ANIM-161 Failure table behaviours

- **Statement:** Each row of "Failure table" behaves as stated: missing or corrupt assets fail their load with an error and leave instances in rest pose or unbound, with one log line and a counter, never an assert.
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-02, Z2-I-ANIM-03, Z2-I-ANIM-04, Z2-I-ANIM-05, Z2-I-ANIM-06, Z2-I-ANIM-07, Z2-I-ANIM-08, Z2-I-ANIM-09, Z2-I-ANIM-10, Z2-I-ANIM-11, Z2-I-ANIM-12, Z2-I-ANIM-13, Z2-I-ANIM-14, Z2-I-ANIM-15, Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimFailure_MissingClipRestPose` and `Test_AnimFailure_CorruptAssetsFailLoad` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM.md → "Failure table"

### Z2-R-ANIM-162 Parser limits

- **Statement:** Every loader, intermediate reader, import helper and document reader enforces its row of "Parser limits", accepting input at the limit and refusing it at limit + 1.
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-02, Z2-I-ANIM-03, Z2-I-ANIM-04, Z2-I-ANIM-05, Z2-I-ANIM-06, Z2-I-ANIM-07, Z2-I-ANIM-08, Z2-I-ANIM-09, Z2-I-ANIM-10, Z2-I-ANIM-11, Z2-I-ANIM-12, Z2-I-ANIM-13, Z2-I-ANIM-14, Z2-I-ANIM-15, Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimationFormats_LimitsAtAndAboveLimit` (`Test` step). Pending ARCH (row AN2): the rows join "Security architecture".
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Parser limits"

### Z2-R-ANIM-163 Fuzz targets

- **Statement:** The five fuzz targets (`Zenith_Fuzz_SkeletonFile`, `Zenith_Fuzz_ClipFile`, `Zenith_Fuzz_GraphFile`, `Zenith_Fuzz_AnimIntermediates`, `Zenith_Fuzz_AnimDocuments`) build and run their smoke duration with no crash, sanitizer report or unbounded allocation.
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-02, Z2-I-ANIM-03, Z2-I-ANIM-04, Z2-I-ANIM-05, Z2-I-ANIM-06, Z2-I-ANIM-07, Z2-I-ANIM-08, Z2-I-ANIM-09, Z2-I-ANIM-10, Z2-I-ANIM-11, Z2-I-ANIM-12, Z2-I-ANIM-13, Z2-I-ANIM-14, Z2-I-ANIM-15, Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** The fuzz targets in the change-triggered `Sanitizers -Preset win-asan` line.
- **Design:** Zenith2/Docs/Design/ANIM/Tests.md → "Fuzz targets"

### Z2-R-ANIM-165 Worker and layout independence

- **Statement:** A 1,200-tick run of 100 animated characters gives bit-identical tick hashes, pose bytes and event sequences with 1, 2, 8 and all workers, forced chunk compaction and two chunk capacities, on every ECS backend.
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-02, Z2-I-ANIM-03, Z2-I-ANIM-04, Z2-I-ANIM-05, Z2-I-ANIM-06, Z2-I-ANIM-07, Z2-I-ANIM-08, Z2-I-ANIM-09, Z2-I-ANIM-10, Z2-I-ANIM-11, Z2-I-ANIM-12, Z2-I-ANIM-13, Z2-I-ANIM-14, Z2-I-ANIM-15, Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimDeterminism_WorkerAndLayoutIndependent` in `Zenith_Integration_Animation` (`Test` step and stage acceptance).
- **Design:** Zenith2/Docs/Design/ANIM/Tests.md → "Determinism tests"; Zenith2/Docs/Design/ANIM.md → "Determinism obligations"

### Z2-R-ANIM-166 Replay bit-identical

- **Statement:** Recording and replaying an animation run reproduces every tick hash, including binding publications with I/O delayed differently in the replay.
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-02, Z2-I-ANIM-03, Z2-I-ANIM-04, Z2-I-ANIM-05, Z2-I-ANIM-06, Z2-I-ANIM-07, Z2-I-ANIM-08, Z2-I-ANIM-09, Z2-I-ANIM-10, Z2-I-ANIM-11, Z2-I-ANIM-12, Z2-I-ANIM-13, Z2-I-ANIM-14, Z2-I-ANIM-15, Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimDeterminism_ReplayBitIdentical` (`Zenith_Integration_Animation`, `Test` step and stage acceptance).
- **Design:** Zenith2/Docs/Design/ANIM/Tests.md → "Determinism tests"; Zenith2/Docs/Design/ANIM.md → "Determinism obligations"

### Z2-R-ANIM-168 Document types registered tool-side

- **Statement:** `Zenith_RegisterAnimationDocumentTypes`, in the tool library `ZenithAuthoring` that ZenithCook and ZenithEditor both link (ruling Q84 (a)), registers the graph, mask, rig and clip annotation document types (and from Stage 4 the retarget type), so `ZenithAnimation` has no `ZenithDocument` edge.
- **Covers:** Z2-I-ANIM-16, Z2-I-ANIM-04, Z2-I-ANIM-07, Z2-I-ANIM-01, Z2-I-ANIM-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimDocuments_RegisteredToolSide` (`ZenithAuthoring_Tests`, `Test` step); `Libgraph` shows no ZenithAnimation → ZenithDocument edge.
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Document type table"; Zenith2/Docs/Design/ANIM.md → "Migration and re-cook"; Zenith2/Docs/Design/ANIM.md → "ZenithAuthoring ANIM files"

### Z2-R-ANIM-169 Canonical documents

- **Statement:** Each ANIM document type writes the canonical form of SCENE's document framework, which round-trips byte-identically and preserves unknown fields.
- **Covers:** Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048, D-SEL-JSON (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimDocuments_CanonicalRoundTrip` and `Test_AnimDocuments_UnknownFieldsPreserved` (`ZenithAuthoring_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Authoring documents"; Zenith2/Docs/Design/ANIM.md → "Migration and re-cook"

### Z2-R-ANIM-170 Document migration fixtures

- **Statement:** Every ANIM document schema version increment ships one migration step and a JSON fixture under `Tools/ZenithCook/Tests/Fixtures/Migration/Animation/<Type>/`.
- **Covers:** Z2-I-ANIM-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimDocuments_MigrationFixtures` (`ZenithAuthoring_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM.md → "Migration and re-cook"

### Z2-R-ANIM-174 W-Characters workload scale

- **Statement:** The Stage 1 workload scale ticket for W-Characters, a `needs-human` ruling TASK on ANIM's epic ([Z2-N-BUDGET-195]), commits `Zenith2/Docs/Research/Workloads/W-Characters.md` with the v1 content scale it records (100 skinned characters of 60 bones and 100 AI agents, as `Zenith2/Docs/Budgets.md` → "W-Characters" fixes them), the scripted-path specification (the camera orbit and the AI goals), the start-scene description and the loop rule, and the `name`, `version`, `game`, `graphics`, `rows` and `metrics` fields of the workload's `*.workload.json`, leaving `startScene` and `scriptedPath` empty.
- **Covers:** Z2-I-CHAR-04 (CHAR), Z2-I-ANIM-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** The ruling ticket's committed `Zenith2/Docs/Research/Workloads/W-Characters.md` and workload fields (`needs-human`), in Stage 1 acceptance.
- **Design:** Zenith2/Docs/Design/ANIM.md → "Stage table"; Zenith2/Docs/Design/ANIM.md → "Rows owned"

### Z2-R-ANIM-175 Stage 1 budget harness

- **Statement:** The harness under `Zenith2/Benchmarks/Budgets/ANIM/` measures the reference engine's skeletal update at the W-Characters scale and ships a CTest smoke run; it records nothing itself.
- **Covers:** Z2-I-CHAR-04 (CHAR), Z2-I-ANIM-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-084, Z2-D-012
- **Budgets:** Z2-B-ANIM-001, Z2-B-ANIM-002, Z2-B-ANIM-003
- **Acceptance evidence:** The harness smoke run in the `Test` step; the measurement report is the ruling ticket's (`Z2-R-ANIM-176`).
- **Design:** Zenith2/Docs/Design/ANIM.md → "Rows owned"; Zenith2/Docs/Design/ANIM.md → "Stage table"

### Z2-R-ANIM-176 Stage 1 budget ruling

- **Statement:** The Stage 1 per-system budget ticket for ANIM allocates `Z2-B-ANIM-001`, `Z2-B-ANIM-002` and `Z2-B-ANIM-003` from the harness's measurements.
- **Covers:** Z2-I-CHAR-04 (CHAR), Z2-I-ANIM-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-084
- **Budgets:** Z2-B-ANIM-001, Z2-B-ANIM-002, Z2-B-ANIM-003
- **Acceptance evidence:** The `needs-human` ruling ticket's measured report under `Zenith2/Docs/Research/` from the harness runs on the reference desktop and the S24, and its row values in `Zenith2/Docs/Budgets.md`, in Stage 1 acceptance.
- **Design:** Zenith2/Docs/Design/ANIM.md → "Rows owned"

### Z2-R-ANIM-177 W-Characters measured at Stage 3

- **Statement:** The W-Characters run on the reference desktop measures ANIM's memory, phase and post-physics rows within their allocations, with W-Characters' frame-time rows measured unreduced.
- **Covers:** Z2-I-ANIM-04, Z2-I-ANIM-05, Z2-I-ANIM-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-021
- **Budgets:** Z2-B-ANIM-001, Z2-B-ANIM-002, Z2-B-ANIM-003, Z2-B-SHIP-001, Z2-B-SHIP-002
- **Acceptance evidence:** The W-Characters device run record in Stage 3 acceptance.
- **Design:** Zenith2/Docs/Design/ANIM.md → "Device and acceptance runs"; Zenith2/Docs/Design/ANIM.md → "Rows measured against"

### Z2-R-ANIM-178 Motion-matching harness

- **Statement:** The harness under `Zenith2/Benchmarks/MotionMatching/` builds a motion-matching database at the W-Characters scale with `Zenith_MotionMatchDatabase::Build` and measures `Zenith_MotionMatcher::Search` cost and the database memory against `Z2-B-ANIM-001` and `Z2-B-ANIM-002`, with a CTest smoke run, as the measurement D-SEL-MOTIONMATCH's ruling reads; the matcher's pruned search equals the brute-force search.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S3
- **Classification:** research
- **Decisions:** Z2-D-084, D-SEL-MOTIONMATCH (open, Stage 3)
- **Budgets:** Z2-B-ANIM-001, Z2-B-ANIM-002
- **Acceptance evidence:** The harness smoke run in the `Test` step; `Test_MotionMatch_PrunedSearchEqualsBruteForce`, `Test_MotionMatch_ContinuationWinsTies` and `Test_MotionMatch_DatabaseDeterministic` (`Test` step); the measurement report is the `needs-human` D-SEL-MOTIONMATCH ruling ticket's.
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationMotionMatch header"; Zenith2/Docs/Design/ANIM/Graph.md → "Motion matching node"; Zenith2/Docs/Design/ANIM.md → "Open decisions"

## Coverage of inventory items

Requirements of either catalogue file; a requirement of Stage 4 or 6 is in `Zenith2/Docs/Requirements/ANIM/Stage4Plus.md`.

| Item | Text | Requirements |
|---|---|---|
| Z2-I-ANIM-01 | skeleton import | Z2-R-ANIM-001, Z2-R-ANIM-002, Z2-R-ANIM-003, Z2-R-ANIM-004, Z2-R-ANIM-005, Z2-R-ANIM-006, Z2-R-ANIM-007, Z2-R-ANIM-013, Z2-R-ANIM-014, Z2-R-ANIM-015, Z2-R-ANIM-016, Z2-R-ANIM-017, Z2-R-ANIM-018, Z2-R-ANIM-019, Z2-R-ANIM-020, Z2-R-ANIM-021, Z2-R-ANIM-022, Z2-R-ANIM-023, Z2-R-ANIM-024, Z2-R-ANIM-025, Z2-R-ANIM-026, Z2-R-ANIM-032, Z2-R-ANIM-055, Z2-R-ANIM-056, Z2-R-ANIM-057, Z2-R-ANIM-059, Z2-R-ANIM-155, Z2-R-ANIM-156, Z2-R-ANIM-158, Z2-R-ANIM-159, Z2-R-ANIM-160, Z2-R-ANIM-161, Z2-R-ANIM-162, Z2-R-ANIM-163, Z2-R-ANIM-164, Z2-R-ANIM-165, Z2-R-ANIM-166, Z2-R-ANIM-168, Z2-R-ANIM-183, Z2-R-ANIM-186 |
| Z2-I-ANIM-02 | animation compression | Z2-R-ANIM-001, Z2-R-ANIM-002, Z2-R-ANIM-003, Z2-R-ANIM-004, Z2-R-ANIM-005, Z2-R-ANIM-006, Z2-R-ANIM-007, Z2-R-ANIM-036, Z2-R-ANIM-037, Z2-R-ANIM-038, Z2-R-ANIM-039, Z2-R-ANIM-040, Z2-R-ANIM-041, Z2-R-ANIM-042, Z2-R-ANIM-043, Z2-R-ANIM-044, Z2-R-ANIM-045, Z2-R-ANIM-155, Z2-R-ANIM-156, Z2-R-ANIM-158, Z2-R-ANIM-159, Z2-R-ANIM-160, Z2-R-ANIM-161, Z2-R-ANIM-162, Z2-R-ANIM-163, Z2-R-ANIM-164, Z2-R-ANIM-165, Z2-R-ANIM-166 |
| Z2-I-ANIM-03 | animation streaming | Z2-R-ANIM-001, Z2-R-ANIM-002, Z2-R-ANIM-003, Z2-R-ANIM-004, Z2-R-ANIM-005, Z2-R-ANIM-006, Z2-R-ANIM-007, Z2-R-ANIM-046, Z2-R-ANIM-047, Z2-R-ANIM-048, Z2-R-ANIM-049, Z2-R-ANIM-050, Z2-R-ANIM-051, Z2-R-ANIM-052, Z2-R-ANIM-053, Z2-R-ANIM-054, Z2-R-ANIM-149, Z2-R-ANIM-155, Z2-R-ANIM-156, Z2-R-ANIM-158, Z2-R-ANIM-159, Z2-R-ANIM-160, Z2-R-ANIM-161, Z2-R-ANIM-162, Z2-R-ANIM-163, Z2-R-ANIM-164, Z2-R-ANIM-165, Z2-R-ANIM-166, Z2-R-ANIM-167 |
| Z2-I-ANIM-04 | blend trees | Z2-R-ANIM-001, Z2-R-ANIM-002, Z2-R-ANIM-003, Z2-R-ANIM-004, Z2-R-ANIM-005, Z2-R-ANIM-006, Z2-R-ANIM-007, Z2-R-ANIM-008, Z2-R-ANIM-011, Z2-R-ANIM-027, Z2-R-ANIM-028, Z2-R-ANIM-029, Z2-R-ANIM-030, Z2-R-ANIM-031, Z2-R-ANIM-032, Z2-R-ANIM-055, Z2-R-ANIM-056, Z2-R-ANIM-057, Z2-R-ANIM-058, Z2-R-ANIM-059, Z2-R-ANIM-060, Z2-R-ANIM-061, Z2-R-ANIM-062, Z2-R-ANIM-063, Z2-R-ANIM-064, Z2-R-ANIM-065, Z2-R-ANIM-066, Z2-R-ANIM-067, Z2-R-ANIM-068, Z2-R-ANIM-069, Z2-R-ANIM-070, Z2-R-ANIM-071, Z2-R-ANIM-072, Z2-R-ANIM-073, Z2-R-ANIM-074, Z2-R-ANIM-075, Z2-R-ANIM-076, Z2-R-ANIM-077, Z2-R-ANIM-078, Z2-R-ANIM-079, Z2-R-ANIM-080, Z2-R-ANIM-081, Z2-R-ANIM-082, Z2-R-ANIM-135, Z2-R-ANIM-136, Z2-R-ANIM-137, Z2-R-ANIM-138, Z2-R-ANIM-139, Z2-R-ANIM-147, Z2-R-ANIM-148, Z2-R-ANIM-150, Z2-R-ANIM-151, Z2-R-ANIM-152, Z2-R-ANIM-153, Z2-R-ANIM-155, Z2-R-ANIM-156, Z2-R-ANIM-157, Z2-R-ANIM-158, Z2-R-ANIM-159, Z2-R-ANIM-160, Z2-R-ANIM-161, Z2-R-ANIM-162, Z2-R-ANIM-163, Z2-R-ANIM-164, Z2-R-ANIM-165, Z2-R-ANIM-166, Z2-R-ANIM-168, Z2-R-ANIM-172, Z2-R-ANIM-174, Z2-R-ANIM-175, Z2-R-ANIM-176, Z2-R-ANIM-177, Z2-R-ANIM-178, Z2-R-ANIM-179, Z2-R-ANIM-180, Z2-R-ANIM-183 |
| Z2-I-ANIM-05 | state machines | Z2-R-ANIM-001, Z2-R-ANIM-002, Z2-R-ANIM-003, Z2-R-ANIM-004, Z2-R-ANIM-005, Z2-R-ANIM-006, Z2-R-ANIM-007, Z2-R-ANIM-008, Z2-R-ANIM-054, Z2-R-ANIM-060, Z2-R-ANIM-065, Z2-R-ANIM-083, Z2-R-ANIM-084, Z2-R-ANIM-085, Z2-R-ANIM-086, Z2-R-ANIM-087, Z2-R-ANIM-088, Z2-R-ANIM-089, Z2-R-ANIM-090, Z2-R-ANIM-091, Z2-R-ANIM-092, Z2-R-ANIM-093, Z2-R-ANIM-119, Z2-R-ANIM-145, Z2-R-ANIM-147, Z2-R-ANIM-148, Z2-R-ANIM-151, Z2-R-ANIM-152, Z2-R-ANIM-153, Z2-R-ANIM-155, Z2-R-ANIM-156, Z2-R-ANIM-157, Z2-R-ANIM-158, Z2-R-ANIM-159, Z2-R-ANIM-160, Z2-R-ANIM-161, Z2-R-ANIM-162, Z2-R-ANIM-163, Z2-R-ANIM-164, Z2-R-ANIM-165, Z2-R-ANIM-166, Z2-R-ANIM-177, Z2-R-ANIM-180 |
| Z2-I-ANIM-06 | additive layers | Z2-R-ANIM-001, Z2-R-ANIM-002, Z2-R-ANIM-003, Z2-R-ANIM-004, Z2-R-ANIM-005, Z2-R-ANIM-006, Z2-R-ANIM-007, Z2-R-ANIM-034, Z2-R-ANIM-094, Z2-R-ANIM-095, Z2-R-ANIM-096, Z2-R-ANIM-099, Z2-R-ANIM-155, Z2-R-ANIM-156, Z2-R-ANIM-158, Z2-R-ANIM-159, Z2-R-ANIM-160, Z2-R-ANIM-161, Z2-R-ANIM-162, Z2-R-ANIM-163, Z2-R-ANIM-164, Z2-R-ANIM-165, Z2-R-ANIM-166 |
| Z2-I-ANIM-07 | masks | Z2-R-ANIM-001, Z2-R-ANIM-002, Z2-R-ANIM-003, Z2-R-ANIM-004, Z2-R-ANIM-005, Z2-R-ANIM-006, Z2-R-ANIM-007, Z2-R-ANIM-096, Z2-R-ANIM-097, Z2-R-ANIM-098, Z2-R-ANIM-099, Z2-R-ANIM-155, Z2-R-ANIM-156, Z2-R-ANIM-158, Z2-R-ANIM-159, Z2-R-ANIM-160, Z2-R-ANIM-161, Z2-R-ANIM-162, Z2-R-ANIM-163, Z2-R-ANIM-164, Z2-R-ANIM-165, Z2-R-ANIM-166, Z2-R-ANIM-168 |
| Z2-I-ANIM-08 | synchronization | Z2-R-ANIM-001, Z2-R-ANIM-002, Z2-R-ANIM-003, Z2-R-ANIM-004, Z2-R-ANIM-005, Z2-R-ANIM-006, Z2-R-ANIM-007, Z2-R-ANIM-035, Z2-R-ANIM-100, Z2-R-ANIM-101, Z2-R-ANIM-102, Z2-R-ANIM-155, Z2-R-ANIM-156, Z2-R-ANIM-158, Z2-R-ANIM-159, Z2-R-ANIM-160, Z2-R-ANIM-161, Z2-R-ANIM-162, Z2-R-ANIM-163, Z2-R-ANIM-164, Z2-R-ANIM-165, Z2-R-ANIM-166 |
| Z2-I-ANIM-09 | animation events | Z2-R-ANIM-001, Z2-R-ANIM-002, Z2-R-ANIM-003, Z2-R-ANIM-004, Z2-R-ANIM-005, Z2-R-ANIM-006, Z2-R-ANIM-007, Z2-R-ANIM-035, Z2-R-ANIM-093, Z2-R-ANIM-103, Z2-R-ANIM-104, Z2-R-ANIM-105, Z2-R-ANIM-106, Z2-R-ANIM-107, Z2-R-ANIM-154, Z2-R-ANIM-155, Z2-R-ANIM-156, Z2-R-ANIM-158, Z2-R-ANIM-159, Z2-R-ANIM-160, Z2-R-ANIM-161, Z2-R-ANIM-162, Z2-R-ANIM-163, Z2-R-ANIM-164, Z2-R-ANIM-165, Z2-R-ANIM-166, Z2-R-ANIM-168, Z2-R-ANIM-185 |
| Z2-I-ANIM-10 | root motion | Z2-R-ANIM-001, Z2-R-ANIM-002, Z2-R-ANIM-003, Z2-R-ANIM-004, Z2-R-ANIM-005, Z2-R-ANIM-006, Z2-R-ANIM-007, Z2-R-ANIM-008, Z2-R-ANIM-010, Z2-R-ANIM-012, Z2-R-ANIM-033, Z2-R-ANIM-044, Z2-R-ANIM-082, Z2-R-ANIM-108, Z2-R-ANIM-109, Z2-R-ANIM-110, Z2-R-ANIM-111, Z2-R-ANIM-118, Z2-R-ANIM-122, Z2-R-ANIM-130, Z2-R-ANIM-135, Z2-R-ANIM-144, Z2-R-ANIM-155, Z2-R-ANIM-156, Z2-R-ANIM-158, Z2-R-ANIM-159, Z2-R-ANIM-160, Z2-R-ANIM-161, Z2-R-ANIM-162, Z2-R-ANIM-163, Z2-R-ANIM-164, Z2-R-ANIM-165, Z2-R-ANIM-166 |
| Z2-I-ANIM-11 | retargeting | Z2-R-ANIM-001, Z2-R-ANIM-002, Z2-R-ANIM-003, Z2-R-ANIM-004, Z2-R-ANIM-005, Z2-R-ANIM-006, Z2-R-ANIM-007, Z2-R-ANIM-024, Z2-R-ANIM-120, Z2-R-ANIM-121, Z2-R-ANIM-122, Z2-R-ANIM-123, Z2-R-ANIM-124, Z2-R-ANIM-155, Z2-R-ANIM-156, Z2-R-ANIM-158, Z2-R-ANIM-159, Z2-R-ANIM-160, Z2-R-ANIM-161, Z2-R-ANIM-162, Z2-R-ANIM-163, Z2-R-ANIM-164, Z2-R-ANIM-165, Z2-R-ANIM-166, Z2-R-ANIM-167 |
| Z2-I-ANIM-12 | IK | Z2-R-ANIM-001, Z2-R-ANIM-002, Z2-R-ANIM-003, Z2-R-ANIM-004, Z2-R-ANIM-005, Z2-R-ANIM-006, Z2-R-ANIM-007, Z2-R-ANIM-009, Z2-R-ANIM-024, Z2-R-ANIM-125, Z2-R-ANIM-126, Z2-R-ANIM-127, Z2-R-ANIM-128, Z2-R-ANIM-129, Z2-R-ANIM-155, Z2-R-ANIM-156, Z2-R-ANIM-158, Z2-R-ANIM-159, Z2-R-ANIM-160, Z2-R-ANIM-161, Z2-R-ANIM-162, Z2-R-ANIM-163, Z2-R-ANIM-164, Z2-R-ANIM-165, Z2-R-ANIM-166, Z2-R-ANIM-167 |
| Z2-I-ANIM-13 | motion warping | Z2-R-ANIM-001, Z2-R-ANIM-002, Z2-R-ANIM-003, Z2-R-ANIM-004, Z2-R-ANIM-005, Z2-R-ANIM-006, Z2-R-ANIM-007, Z2-R-ANIM-009, Z2-R-ANIM-130, Z2-R-ANIM-155, Z2-R-ANIM-156, Z2-R-ANIM-158, Z2-R-ANIM-159, Z2-R-ANIM-160, Z2-R-ANIM-161, Z2-R-ANIM-162, Z2-R-ANIM-163, Z2-R-ANIM-164, Z2-R-ANIM-165, Z2-R-ANIM-166, Z2-R-ANIM-167 |
| Z2-I-ANIM-14 | facial animation | Z2-R-ANIM-001, Z2-R-ANIM-002, Z2-R-ANIM-003, Z2-R-ANIM-004, Z2-R-ANIM-005, Z2-R-ANIM-006, Z2-R-ANIM-007, Z2-R-ANIM-009, Z2-R-ANIM-025, Z2-R-ANIM-043, Z2-R-ANIM-131, Z2-R-ANIM-132, Z2-R-ANIM-133, Z2-R-ANIM-134, Z2-R-ANIM-155, Z2-R-ANIM-156, Z2-R-ANIM-158, Z2-R-ANIM-159, Z2-R-ANIM-160, Z2-R-ANIM-161, Z2-R-ANIM-162, Z2-R-ANIM-163, Z2-R-ANIM-164, Z2-R-ANIM-165, Z2-R-ANIM-166, Z2-R-ANIM-181, Z2-R-ANIM-184, Z2-R-ANIM-185 |
| Z2-I-ANIM-15 | ragdoll transitions | Z2-R-ANIM-001, Z2-R-ANIM-002, Z2-R-ANIM-003, Z2-R-ANIM-004, Z2-R-ANIM-005, Z2-R-ANIM-006, Z2-R-ANIM-007, Z2-R-ANIM-008, Z2-R-ANIM-010, Z2-R-ANIM-011, Z2-R-ANIM-012, Z2-R-ANIM-074, Z2-R-ANIM-112, Z2-R-ANIM-113, Z2-R-ANIM-114, Z2-R-ANIM-115, Z2-R-ANIM-116, Z2-R-ANIM-117, Z2-R-ANIM-118, Z2-R-ANIM-119, Z2-R-ANIM-149, Z2-R-ANIM-155, Z2-R-ANIM-156, Z2-R-ANIM-158, Z2-R-ANIM-159, Z2-R-ANIM-160, Z2-R-ANIM-161, Z2-R-ANIM-162, Z2-R-ANIM-163, Z2-R-ANIM-164, Z2-R-ANIM-165, Z2-R-ANIM-166, Z2-R-ANIM-177, Z2-R-ANIM-180 |
| Z2-I-ANIM-16 | animation visual authoring | Z2-R-ANIM-001, Z2-R-ANIM-002, Z2-R-ANIM-003, Z2-R-ANIM-004, Z2-R-ANIM-005, Z2-R-ANIM-006, Z2-R-ANIM-007, Z2-R-ANIM-023, Z2-R-ANIM-035, Z2-R-ANIM-065, Z2-R-ANIM-067, Z2-R-ANIM-097, Z2-R-ANIM-124, Z2-R-ANIM-155, Z2-R-ANIM-156, Z2-R-ANIM-157, Z2-R-ANIM-158, Z2-R-ANIM-159, Z2-R-ANIM-160, Z2-R-ANIM-161, Z2-R-ANIM-162, Z2-R-ANIM-163, Z2-R-ANIM-164, Z2-R-ANIM-165, Z2-R-ANIM-166, Z2-R-ANIM-168, Z2-R-ANIM-169, Z2-R-ANIM-170, Z2-R-ANIM-171, Z2-R-ANIM-172, Z2-R-ANIM-173, Z2-R-ANIM-181, Z2-R-ANIM-186 |
| Z2-I-CHAR-04 | animation update budgeting (CHAR) | Z2-R-ANIM-138, Z2-R-ANIM-140, Z2-R-ANIM-141, Z2-R-ANIM-142, Z2-R-ANIM-143, Z2-R-ANIM-144, Z2-R-ANIM-145, Z2-R-ANIM-146, Z2-R-ANIM-167, Z2-R-ANIM-174, Z2-R-ANIM-175, Z2-R-ANIM-176, Z2-R-ANIM-180, Z2-R-ANIM-182, Z2-R-ANIM-184 |

## Excluded and deferred

Every ANIM inventory item is classified required: Z2-I-ANIM-01, -04 … -10 and -15 at Stage 3, Z2-I-ANIM-02, -03, -11 … -14 and -16 at Stage 4, and Z2-I-CHAR-04's runtime at Stage 4; no ANIM requirement is deferred, excluded or game-side. One requirement is classified research: the D-SEL-MOTIONMATCH (open, Stage 3) harness, whose measurement the ruling reads. The motion-matching node (`Z2-R-ANIM-179`) is classified required and names D-SEL-MOTIONMATCH (open, Stage 3) in its Decisions, so its tickets are blocked by the ruling ticket; a ruling that excludes motion matching changes the inventory through `Zenith2/Docs/Requirements/Inventory.md → "Change rule"`, which reclassifies the requirement in that diff. Requirements staged S1 are the budget producer and the W-Characters scale; the S6 requirements are shipping qualification. The boundaries that keep the area bounded are exclusions of other capabilities from ANIM:

| Not in ANIM | Rationale | Decision or owner |
|---|---|---|
| vertex formats, skinned vertex streams, the skinned-mesh cook and GPU skinning (linear blend skinning, morph targets) | geometry and the skinning pass are GEOM's; ANIM supplies poses and curve weights | GEOM; Z2-D-062; `Zenith2/Docs/Design/GEOM/Skinning.md → "Flux_Skinning header"` |
| the skinned-vertex pool and its allocator | an owner-defined pool of RHI's pool mechanism, owned by GEOM | RHI, GEOM; Z2-D-058 |
| the GPU front end, per-pass tables, instance keys and render extraction scheduling | RPROD's; ANIM registers one extractor | RPROD; Z2-D-050 |
| applying root motion to an entity with a physics character controller, and the controller | PHYS's controller node in phase 4 (ANIM applies root motion to every other entity, `Z2-R-ANIM-111`) | PHYS; the orchestrator's Q72 ruling; [Z2-N-ARCH-475] |
| the phase contract and the authority census | GAME registers no phase-4 node; ANIM adds its census rows | GAME; Z2-D-080 |
| ragdoll bodies, constraints and simulation; collision queries | PHYS's; ANIM blends poses and owns the transitions | PHYS; Z2-D-003 |
| dual-quaternion skinning | GEOM's linear blend skinning stays the GPU path; ANIM's rigid blends are CPU-side deltas | ANIM ruling A11 (`Zenith2/Docs/Design/ANIM.md → "Design rulings owned by ANIM"`) |
| the animation editor panels and their undo | EDIT's tools over ANIM's documents | EDIT; Z2-D-079 |
| cinematic sequencing and tracks | CINE drives ANIM through slots and the cinematic writer source | CINE |
| character production workflows, the motion-matching ruling | CHAR's items (Z2-I-CHAR-09 holds the ruling) | CHAR; Z2-D-084 |
| source import of glTF files and the DCC matrix | CONTENT's importer calls ANIM's helpers | CONTENT |
| baking a pose change into a mesh, per-mesh rebinding | never done: one skeleton has one rest pose, and retargeting carries rest differences as rotations | ANIM ruling A4 |
