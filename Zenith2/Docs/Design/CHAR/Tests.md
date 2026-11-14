# CHAR design: test catalogue

Section file of `Zenith2/Docs/Design/CHAR.md`. Every named CHAR test, the file it lives in, the target that builds it, the case it asserts and the gate step that runs it. Test names follow the WorkerGuide's rule (the doctest case `"<Library>/<Type>/<Behaviour>"`, cited as `Test_<Type>_<Behaviour>`); every CHAR type is prefixed `Char`, so CHAR's names are unique programme-wide (TEST-NAME-UNIQUE). Cases written `[<Backend>]` run once per physics backend (Jolt, PhysX) as separate CTest cases. No CHAR test needs a GPU; no binary fixture is committed (Q55): every source is generated at test time from declared constants. Every expected value below is derived from this specification (GOLDEN-FILES); the one output that is not derivable by hand, the cross-host determinism digest, is compared by a `needs-human` acceptance item, never committed.

## Scope of this file

| Group | Heading |
|---|---|
| where the tests live and which gate runs them | "Test targets" |
| rig, controls, pose books | "Rigging tests" |
| faces and lip-sync | "Facial and lip-sync tests" |
| modular characters and hair | "Modular and hair tests" |
| authority | "Authority tests" |
| pose debugging, budgeting, validation | "Debugging budgeting and validation tests" |
| editor tools and the workflow | "Editor and workflow tests" |
| documents and registration | "Document and registration tests" |
| end to end | "Integration tests" |
| robustness (mutated-corpus cases in place of fuzz targets) | "Fuzz targets" |
| measurement | "Benchmarks" |
| generated content | "Fixtures and generators" |

## Test targets

| Target | Files | Gate step |
|---|---|---|
| `ZenithAuthoring_Tests` (EDIT's target of the tool library, `Zenith2/Docs/Design/EDIT.md → "ZenithAuthoring contract rules"`) | `Zenith2/Tools/Authoring/Tests/CHAR/Zenith_Char<Group>_Tests.cpp` | `Test` |
| `ZenithCook_Tests` (ASSET's cook test target) | `Zenith2/Tools/ZenithCook/Tests/Character/Zenith_CharCook_Tests.cpp`, `Zenith_CharPreview_Tests.cpp` (the cases that need cooked clips and graphs in a `Zenith_AnimationPreview`), `Zenith_CharFixtures_Generate.cpp` (the integration fixture set) | `Test` |
| `ZenithEditor_Tests` (EDIT's) | `Zenith2/Tools/ZenithEditor/Tests/Character/Zenith_Char<Panel>_Tests.cpp`, `Zenith_CharWalkthrough_Tests.cpp` | `Test` (cases in `z2-edit-cooked` require EDIT's fixture games) |
| `Zenith_Integration_Character` | `Zenith2/Tests/Character/Zenith_CharIntegration_Tests.cpp`, `Zenith_CharLipSync_Tests.cpp`, `Zenith_CharModular_Tests.cpp`, `Scripts/*.luau` | `Test`; stage acceptance |
| `Zenith_Bench_CharMotionMatchAuthoring` | `Zenith2/Benchmarks/CharacterAuthoring/` | its smoke case in `Test`; the full run by the `needs-human` ruling ticket |
| `ZenithAuthoring.CharNoCrtTranscendental` | CTest (`cmake -P`): `Zenith2/CMake/Tests/Zenith_CharNoCrtTranscendental.cmake`, registered in `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` | `Test` (the source scan of `Zenith2/Docs/Design/CHAR/Facial.md → "Estimator input"`) |

`Libgraph`, `ApiContractCheck`, `HeaderSelfContained`, `ForbiddenCheck` and `Complexity` cover CHAR's code as every target's. The tool targets build in the `win` presets only (`ZENITH_TOOLS_HOST`), so no CHAR test runs on Android; the runtime behaviour CHAR depends on is tested on the S24 by its owners. A case that needs a cooked clip, graph or pose book cooks it in memory with ASSET's cook test access (`Zenith_CookTestAccess.h`, ruling GEOM-N2) and therefore lives in `ZenithCook_Tests`; a `ZenithAuthoring_Tests` case uses no cooked asset.

## Rigging tests

File `Zenith_CharRig_Tests.cpp` (`ZenithAuthoring_Tests`) unless noted.

| Test | Asserts |
|---|---|
| `Test_CharRigProfile_TablesWellFormed` | every profile's roles ascend, names are unique within a profile and already normalised |
| `Test_CharRigNames_Normalise` | `mixamorig:Hips` → `hips`, `DEF-upper_arm.L` → `def_upper_arm_l`, `Spine 01` → `spine_01`, an all-UTF-8 name at joint index 7 → `joint_7`; tokens of `LeftUpLeg` and `spine01` |
| `Test_CharRigProfile_DetectsMixamo`, `…DetectsMannequinStyle`, `…DetectsRigify`, `…DetectsZenithHumanoid` | each generated rig (`Zenith_CharMakeSkeletonView` in its style) is matched with score 1.0 by its profile and with a lower score by every other |
| `Test_CharRigProfile_TieBreaksByAssignedThenID` | equal scores resolve by assigned count, then profile ID |
| `Test_CharRigGeneric_AssignsUnnamedHumanoid` | the unnamed-token style (`Bone_Thigh_Left`) is assigned completely by the generic profile |
| `Test_CharRigGeneric_KeywordNotReusedDownChain` | a `Leg`/`Leg`/`Foot` chain assigns UPPER_LEG and LOWER_LEG once each |
| `Test_CharRigTopology_UnassignsNonDescendant` | a HAND not below LOWER_ARM is unassigned with `CHAR_DIAG_RIG_TOPOLOGY` |
| `Test_CharRigTopology_ReportsSwappedSides` | a left arm at negative x reports `CHAR_DIAG_RIG_SIDE_SWAPPED` and assigns nothing differently |
| `Test_CharMirror_DetectsAndChecksPositions` | pairs found by token swap; a pair 3 cm off the mirror position is reported asymmetric and unpaired |
| `Test_CharMirror_AmbiguousLeftUnpaired` | a joint claimed by two counterparts is unpaired and reported |
| `Test_CharChains_GeneratedForCompleteProfile` | the chains and IK arrays for the Mixamo fixture, in table order |
| `Test_CharChains_StraightLegHintIsFront` | a straight leg's two-bone hint is (0, 0, 1) |
| `Test_CharRestPose_ClassifiesTAndA` | elevation 0° → T_POSE, −45° → A_POSE, −20° → OTHER |
| `Test_CharRestPose_DetectsBackwardsFacing` | the fixture rotated 180° about +Y reports facing backwards with the `transform.sourceFront` hint |
| `Test_CharRestPose_DetectsMirrored` | the fixture with x negated reports mirrored |
| `Test_CharRestPose_ReportsUnitError` | the fixture scaled 100× reports a height of 168 m outside the hero range [1.4, 2.2] with the `transform.unitScale` hint; unscaled it reports 1.68 m and no diagnostic |
| `Test_CharRigDocument_ApplyTwiceIsNoOp` | a second application produces an empty edit list |
| `Test_CharRigDocument_KeepsAuthoredRoleUnlessOverwrite` | an authored role differing from detection is kept and listed; with `overwrite` it is replaced |
| `Test_CharRigDocument_ShellMeshesFromSource` | `shellMeshes` lists the source's skinned meshes of the skeleton, ascending |
| `Test_CharRigDocument_CookAcceptsGenerated` (`ZenithCook_Tests`) | ANIM's `anim.skeleton` step cooks the generated document and `IsHumanoidComplete()` is true |
| `Test_CharFacialTemplate_AddsVisemeCurves` | the sixteen viseme-set curves with kinds and defaults; face roles of the profile |
| `Test_CharFacialTemplate_CurveLimitRefused` | 497 existing curves plus sixteen refuse with `CHAR_ERROR_LIMIT_EXCEEDED` and no edit |
| `Test_CharRagdoll_DefaultFromProfile` | the values of "Default ragdoll generation" (`Zenith2/Docs/Design/CHAR/Rigging.md`) for the Mixamo fixture at 70 kg: parts, parents, constraints, the masses (total 65.1 kg), the hips and both upper-arm capsules |
| `Test_CharRagdoll_IndependentOfCrtFma3Path` | `_set_FMA3_enable(1)` then `(0)`, the flag read back and restored by a scope guard: byte-identical unrounded binary64 geometry of `Zenith_CharComputeRagdollGeometry` and byte-identical documents for the four named styles (Windows; an INFO line and no claim on a host without FMA3) |
| `Test_CharRagdoll_CookAccepts` (`ZenithCook_Tests`) | PHYS's `physics.ragdoll` step cooks it |
| `Test_CharLibraryRig_ValidationRules` | each library-rig rule's diagnostic on a fixture breaking only it |
| `Test_CharRetarget_CreateWritesRoleMapping` | the document's members for two complete profiles |
| `Test_CharRetarget_IncompleteProfileListsJoints` | unmapped target joints listed with `CHAR_ERROR_PROFILE_INCOMPLETE` |
| `Test_CharRetarget_RestCheckUsesAnimMap` | the T-posed source retargets to the A-posed target's rest within 1e-5; a corrupted target reports per joint |
| `Test_CharControls_ResolveFillsIndices` | a joint and a pose resolved to their indices; an unknown pose name stays unresolved, reports `CHAR_DIAG_CONTROL_TARGET_UNKNOWN` and is skipped (and counted) by evaluation |
| `Test_CharControls_SpanLengthsChecked` | `afValues` shorter than the control count, or `axPoints` not one per Point control, outputs nothing and counts the refused call |
| `Test_CharControls_EvaluateOrderAndClamp` | parameters, curves, poses, rotations, IK in order; values clamped |
| `Test_CharControls_PoseWeightFromRange` | weight `(v − min) / (max − min)` |
| `Test_CharControls_NonFiniteIgnored` | a NaN value and an infinite point are ignored and counted |
| `Test_CharControls_PoseMatchesAnimPoseBlend` (`ZenithCook_Tests`, `Zenith_CharPreview_Tests.cpp`) | a Pose control at weight 1 equals ANIM's `POSE_BLEND` over the cooked pose book within 1e-5 |
| `Test_CharPoseBook_CookFramesAndMarkers` (`ZenithCook_Tests`) | frame 0 rest, frame k pose k − 1, one marker per pose, the pose-library flag |
| `Test_CharPoseBook_DeterministicIntermediate` (`ZenithCook_Tests`) | identical documents give byte-identical intermediates |
| `Test_CharPoseBook_CaptureWritesOnlyDifferences` | only components beyond 1e-5 of rest are written |
| `Test_CharPoseBook_MirrorSwapsSidesAndCurves` | `Smile.L` mirrors to `Smile.R` with joints and `…Left`/`…Right` curves swapped |
| `Test_CharRigLimits_AtAndAboveLimit` | every limit of "Rigging limits" (`Zenith2/Docs/Design/CHAR/Rigging.md`) at the limit and limit + 1 |

## Facial and lip-sync tests

File `Zenith_CharLipSync_Tests.cpp` (`ZenithAuthoring_Tests`) unless noted; the estimator cases are specified in `Zenith2/Docs/Design/CHAR/Facial.md → "Estimator input"` … `"Estimator smoothing and normalisation"`.

| Test | Asserts |
|---|---|
| `Test_CharFft_MatchesDirectDft` | the 512-point FFT equals an O(N²) DFT within 1e-9 relative on five generated signals |
| `Test_CharEstimator_ClassifiesGeneratedSignals` | the seven rows of "Estimator classification": classes per frame and feature ranges, including the 16 kHz identity case |
| `Test_CharEstimator_KeyTimeDomain` | the three lead times of "Estimator smoothing and normalisation" |
| `Test_CharEstimator_WeightsSumToOneBeforeReduction` | `Σ_{v ≠ sil} w_v ≤ 1 + 1e-12` and `|Σ_v w_v − 1| ≤ 1e-12` at every key of the binary64 frame track |
| `Test_CharEstimator_WeightsSumWithinReductionTolerance` | `|Σ_v w_v(t) − 1| ≤ 15 × reduceEpsilon + 1e-6` at every key time of every viseme curve of the stored f32 track, at `reduceEpsilon` 0.002 and 0 |
| `Test_CharEstimator_DeterministicOutput` | two runs and a copied input give byte-identical tracks |
| `Test_CharEstimator_IndependentOfCrtFma3Path` | `_set_FMA3_enable(1)` then `(0)`, the flag read back and restored by a scope guard: byte-identical binary64 frame features and frame tracks, and stored tracks, for every signal (Windows; an INFO line and no claim on a host without FMA3) |
| `Test_CharDeterminism_WritesDigests` | the digest file of "Estimator input" is written with one case per signal track and per ragdoll style |
| `Test_CharEstimator_KeyReductionWithinEpsilon` | stored keys reproduce every binary64 frame-track key within `reduceEpsilon + 6e-8` |
| `Test_CharEstimator_PhraseStartsForceRest` | rest keys at `p − 0.05` and p for every phrase start |
| `Test_CharVisemeImport_ConvertsCues` | cues to curves through the smoothing |
| `Test_CharVisemeImport_RejectsUnorderedTimes` | equal or decreasing times fail with the key path |
| `Test_CharVisemeImport_DurationFromAudio` | a 2.31 s file over 2.0 s of audio: `d_v` 2.0, `CHAR_DIAG_VISEMES_DURATION_MISMATCH`, no key past 2.0 |
| `Test_CharLipSyncOverrides_ReplaceRange` | the override's range is spliced; outside keys untouched; a sum above 1.05 warns |
| `Test_CharLipSyncClip_StrideAndWindows` | S = 1.85, D = 5.55 for durations 1.0, 1.6, 0 s at 60 Hz, `padSeconds` 0.25; window boundary keys |
| `Test_CharLipSyncClip_StrideCeilTolerance` | a 0.4 s line at `padSeconds` 0.2 and 60 Hz: `(0.4 + 0.2) × 60` = 36.00000000000001 gives S = 0.6 (36 samples, 37 without the tolerance) |
| `Test_CharLipSyncClip_DurationLimitRefused` | `L × S > 3,600` refused |
| `Test_CharClipTimes_RoundAndCollapse` | f32 rounding of times; a pair 1e-9 s apart collapses with `CHAR_DIAG_CLIP_KEY_COLLAPSED` counted once |
| `Test_CharClipIntermediate_AnimCookAccepts` (`ZenithCook_Tests`) | ANIM's writer and `anim.clip` step accept a lip-sync clip; sampled curves match at three times within 0.002 |
| `Test_CharLipSyncImporter_ProductsPerLine` (`ZenithCook_Tests`) | two lines × two locales give two products with their intermediates and annotation settings |
| `Test_CharLipSyncImporter_MissingAudioFails` (`ZenithCook_Tests`) | the error names the line, the locale and the path |
| `Test_CharLipSyncImporter_ReimportBitIdentical` (`ZenithCook_Tests`) | a reimport of unchanged inputs gives identical intermediates and the same import key |
| `Test_CharLipSyncLimits_AtAndAboveLimit` | every limit of "Facial and lip-sync limits" |
| `Test_CharVisemePoseBook_DirectNamesWin` | `viseme_aa` wins over the ARKit combination |
| `Test_CharVisemePoseBook_ArkitCombination` | the thirteen ARKit-style targets give the combination table's values; with `mouthUpperUpRight` removed, `ff` is unmapped (`CHAR_DIAG_VISEME_UNMAPPED`) |
| `Test_CharFaceTemplate_GraphCompilesClean` | the instantiated face part compiles with no diagnostic (`Zenith_CompileAnimationGraph` in memory) |
| `Test_CharFaceTemplate_EmptySlotsKeepBodyFace` (`ZenithCook_Tests`, `Zenith_CharPreview_Tests.cpp`) | a body clip's face curve passes through with empty slots |
| `Test_CharFaceTemplate_LipSyncCurvesOnly` (`ZenithCook_Tests`, `Zenith_CharPreview_Tests.cpp`) | a lip-sync clip in `char.lipsync` changes no joint |
| `Test_CharLipSyncSync_AddsUpdatesRemovesLines` (`ZenithEditor_Tests`) | sync over a fixture bank edit |
| `Test_CharLipSyncSync_PathOutsideRefused` (`ZenithEditor_Tests`) | an audio path needing `..` refuses the command |
| `Test_CharLipSyncSync_KeepsAuthoredSources` (`ZenithEditor_Tests`) | sources, visemes, phrases and overrides survive a sync |
| `Test_CharLipSyncSync_SecondRunNoEdit` (`ZenithEditor_Tests`) | an unchanged bank gives no edit |
| `Test_CharLipSyncSync_ReportsBankDefaultLocale` (`ZenithEditor_Tests`) | a line listing `fr` before `en` under `voiceLocales` `["en", "fr"]` is reported |
| `Test_CharLipSyncSync_DigestIgnoresLineEndings` (`ZenithEditor_Tests`) | a bank rewritten with CRLF line endings is not shown stale |
| `Test_CharLipSyncPanel_PhraseDragIsOneRecord` (`ZenithEditor_Tests`) | a marker drag over several frames commits one `setPhrases` record |
| `Test_CharLipSyncPanel_AuditionReportsHeadless` (`ZenithEditor_Tests`) | the audition command's data headless: `preview` false, `windowStart` S for locale index 1 |

## Modular and hair tests

File `Zenith_CharAssembly_Tests.cpp` (`ZenithAuthoring_Tests`) unless noted.

| Test | Asserts |
|---|---|
| `Test_CharAssembly_VisibilityRules` | the three visibility rules and both region diagnostics |
| `Test_CharAssembly_DerivedIDsStableAcrossRegeneration` | IDs survive a rename, a reorder and an added part; fixture scene overrides still resolve |
| `Test_CharAssembly_VariantOverridesOnlyVisibility` | each variant's `base` block holds only property 15 changes |
| `Test_CharAssembly_GenerationIsCanonicalAndIdempotent` (`ZenithEditor_Tests`) | the second generation writes nothing |
| `Test_CharAssembly_GeneratedPrefabResolves` | SCENE's resolver gives the expected entities, leaders and visibilities for the base and a variant |
| `Test_CharAssembly_RetiredLoadoutTrashed` (`ZenithEditor_Tests`) | a deleted loadout's variant goes to the editor trash in the same batch |
| `Test_CharAssembly_HandEditRefusedWithoutForce` (`ZenithEditor_Tests`) | a hand-edited generated prefab refuses generation; `force` overwrites |
| `Test_CharAssembly_DigestIgnoresLineEndings` (`ZenithEditor_Tests`) | a generated prefab rewritten with CRLF line endings is not refused |
| `Test_CharAssembly_KeepAsVariantPreservesEdit` (`ZenithEditor_Tests`) | "Keep as variant algorithm" (`Zenith2/Docs/Design/CHAR/Modular.md`): the five representable edits land in V's block and survive the forced regeneration; a renamed part entity is refused with `CHAR_ERROR_EDIT_UNREPRESENTABLE` and nothing is written |
| `Test_CharAssembly_KeepAsVariantOfLoadoutVariant` (`ZenithEditor_Tests`) | the chain V → loadout variant → base resolves, and counts toward the variant chain limit |
| `Test_CharAssemblyCreate_NamesSlotsAndRegions` | the part-name rule, the slot and region table, the collision suffix, `Part<n>` and the `P` prefix |
| `Test_CharAssemblyValidation_EachRule` | one case per rule MV1 … MV13 (MV12 three times) |
| `Test_CharAssemblyValidation_CleanFixturePasses` | the walkthrough assembly has no diagnostic |
| `Test_CharRigidAttachment_WritesSocketSetting` | the `skin.rigidAttach` setting JSON from a fixture socket |
| `Test_CharHairRules_EachRule` | one case per rule HC1 … HC5 |

## Authority tests

File `Zenith_CharAuthority_Tests.cpp` (`ZenithAuthoring_Tests`).

| Test | Asserts |
|---|---|
| `Test_CharAuthority_AU1AnimatorIncomplete`, `…AU2RootMotionUnused`, `…AU3ControllerStarved`, `…AU4RagdollMismatch`, `…AU5DynamicBodyOnCharacter`, `…AU6MovedStaticCollider`, `…AU7FollowerInvalid`, `…AU8ReducedRateThroughCollision`, `…AU9ClassRate` | each rule of "Authority lint rules" (`Zenith2/Docs/Design/CHAR/Authority.md`) on a fixture breaking only that rule; AU5 also with MotionType absent (the dynamic default) and with MotionType 2 (no diagnostic) |
| `Test_CharAuthority_CleanCharacterPasses` | the integration fixture character has no diagnostic |
| `Test_CharAuthority_FlagshipCharactersClean` (Stage 4) | no error on any committed flagship character prefab |

## Debugging budgeting and validation tests

| Test | File (target) | Asserts |
|---|---|---|
| `Test_CharPoseSnapshot_RoundTripExact` | `Zenith_CharPose_Tests.cpp` (`ZenithAuthoring_Tests`) | write and read back bit-identical |
| `Test_CharPoseSnapshot_RejectsUnknownVersion` | same | version 2 refused with `CHAR_ERROR_SNAPSHOT_INVALID` |
| `Test_CharPoseSnapshot_RejectsOverLimits` | same | 1,025 joints and 513 curves refused |
| `Test_CharPoseDiff_DetectsEachDelta` | same | one differing joint per delta kind |
| `Test_CharPoseDiff_SignIndependentRotation` | same | q and −q give `dr = 0` |
| `Test_CharPoseDiff_PairsByNameAcrossRigs` | same | name pairing between a T-posed and an A-posed rig |
| `Test_CharPoseDebugger_ReadsWithoutWriting` | `Zenith_CharPoseDebugger_Tests.cpp` (`ZenithEditor_Tests`, `z2-edit-cooked`) | equal world hashes over 300 ticks with and without the panel |
| `Test_CharPoseDebugger_JointTableFromModelPose` | same | the decomposition and Δ rest columns |
| `Test_CharPoseHistory_RingOrderAndCapacity` | same | order and overwrite at capacity |
| `Test_CharPoseHistory_ShedsHalfUnderPressure` | same | the cache consumer frees the oldest half |
| `Test_CharBudget_PredictMatchesAnimRule` | `Zenith_CharBudget_Tests.cpp` (`ZenithAuthoring_Tests`) | twelve (distance, bias) cases equal `Zenith_AnimComputeRateDivisor`; the full-rate flag and an active ragdoll each give 1 |
| `Test_CharBudget_PanelRowCap` | `Zenith_CharBudget_Tests.cpp` (`ZenithEditor_Tests`) | 256 rows at most |
| `Test_CharContentRules_ClassLabelSelectsLimit` | `Zenith_CharCook_Tests.cpp` (`ZenithCook_Tests`) | a crowd-labelled 70-joint skeleton fails `COOK_ERROR_RULE` against ANIM's registered `bones` measure |
| `Test_CharValidation_RunsEveryRuleOnce` | `Zenith_CharValidation_Tests.cpp` (`ZenithAuthoring_Tests`) | one diagnostic per rule CV1 … CV18; CV14's fixture holds a `character`-labelled source without a class label (reported) and a library rig labelled `char-library-rig` alone (not reported: it is not a character source) |
| `Test_CharValidation_SortedDeterministically` | same | the documented order |

## Editor and workflow tests

`ZenithEditor_Tests`, files `Zenith2/Tools/ZenithEditor/Tests/Character/`, unless noted.

| Test | Asserts |
|---|---|
| `Test_CharEditorExtension_RegistersEverything` | every panel type, handler, command and step registered once |
| `Test_CharCommands_EachChangingCommandIsOneRecord` | one record per document-changing command; undo restores bytes |
| `Test_CharCommands_InvalidArgsRefused` | a missing required argument of each command is `EDITOR_ERROR_COMMAND_ARGS` |
| `Test_CharCommands_ArgumentFormsResolve` | every form of "Command argument forms" (`Zenith2/Docs/Design/CHAR/Workflow.md`): an asset by ID, by path and by `#` product key; an entity by ID, by hierarchy path and through an instance root; `$entity:`; an unknown path and a duplicate sibling name refused |
| `Test_CharAutomation_StepsValidateMembers` | unknown members and wrong kinds refused before step 1 |
| `Test_CharImportWizard_WritesCharacterSettings` | the step-3 settings JSON, after the GUID assignment |
| `Test_CharImportWizard_FrontMarkerSecondPass` | the second settings apply with a `char.front` node, none without |
| `Test_CharImportWizard_ReportsBackwardsFacing` | `facing: backwards` and the suggested setting |
| `Test_CharWorkflowPanel_StepStatesFromDocuments` | each step state on fixture sessions, the three retarget conditions included |
| `Test_CharWorkflowPanel_DriveRunsTimeline` | the Drive button issues `character.play.drive` with the chosen timeline |
| `Test_CharTemplateBinding_MatchesByTokens`, `…ExplicitBindingWins`, `…RequiredMissingFails` (`ZenithAuthoring_Tests`) | "Template clip binding" (`Zenith2/Docs/Design/CHAR/Workflow.md`), the walkthrough clip set and the `in air` counter-example |
| `Test_CharTemplate_InstantiatesAndCompiles` (`ZenithAuthoring_Tests`) | every template, with all and with no optional roles, compiles clean |
| `Test_CharTemplate_ReinstantiationKeepsIDs` (`ZenithAuthoring_Tests`) | node, state and transition IDs are stable |
| `Test_CharTemplate_HasCinematicAndGetUpSlots` (`ZenithAuthoring_Tests`) | the five slot names |
| `Test_CharTemplate_HarnessTextsMatch` (`ZenithAuthoring_Tests`, Stage 4) | each shipped template text equals the authoring-cost harness's constant byte for byte |
| `Test_CharScenario_RunsBuiltInsOnFixtureGraph` (`ZenithCook_Tests`, `Zenith_CharPreview_Tests.cpp`) | every built-in scenario passes on the facial template over the walkthrough clips at 30 Hz; `lipsync-line` plays a clip the test builds from `CHAR_SIGNAL_VOWEL_AA` (1.0 s, locales `en` and `fr`) through the estimator, `Zenith_CharLayoutLipSyncClip`, ANIM's writer and the cook test access (`padSeconds` 0.25: S = 1.25 s, e_0 = 1.05 s, duration 1.15 s < S); with no lip-sync clip the scenario is `CHAR_ERROR_NOT_AVAILABLE` and not run |
| `Test_CharScenario_FailingAssertionReported` (`ZenithCook_Tests`, `Zenith_CharPreview_Tests.cpp`) | a wrong `stateAt` fails with the observed state |
| `Test_CharScenario_Deterministic` (`ZenithCook_Tests`, `Zenith_CharPreview_Tests.cpp`) | identical results over two runs |
| `Test_CharInGame_SpawnBindsWithinTicks` (`z2-edit-cooked`) | `BOUND` within 10 ticks; at least 1 m moved in 2 s under the drive |
| `Test_CharInGame_PlacePrefabWithIdAndReference` (`z2-edit-cooked`) | the instance key is the given `id`, the root sits at the player start, the reference holds the instance root's derived ID |
| `Test_CharWalkthrough_SetupWritesSources` (`z2-fixture-setup`) | the walkthrough sources exist in the scratch copy; each `.glb` parses with CONTENT's glTF parser with 62 joints, 8 or 1 meshes and 0 or 7 animations; each WAV decodes to its generator's frame count |
| `Test_Walkthrough_CharacterWorkflow_Rehearsal` (`z2-edit-cooked`) | the rehearsal script passes and the walkthrough's check holds |

## Document and registration tests

| Test | File (target) | Asserts |
|---|---|---|
| `Test_CharDocuments_RegistersFiveTypes` | `Zenith_CharDocuments_Tests.cpp` (`ZenithAuthoring_Tests`) | the five types register with their stable names, tags, suffixes and canonical schemas; a second registration is `DOCUMENT_ERROR_DUPLICATE_REGISTRATION` |
| `Test_CharDocuments_CanonicalRoundTrip` | same | each type's example document reads and writes back canonically, byte for byte |
| `Test_CharDocuments_UnknownMembersPreserved` | same | an unknown member survives a round trip with `MIGRATION_WARNING_UNKNOWN_FIELD` |
| `Test_CharDocuments_MigrationFixtures` | same | every `v<N>.json` under `Tests/Fixtures/Migration/CHAR/<Type>/` migrates to its `v<N>.expected.json` (version 1: open-and-write identity) |
| `Test_CharImporters_RegisterThree` | `Zenith_CharCook_Tests.cpp` (`ZenithCook_Tests`) | `char.lipsync`, `char.posebook`, `char.document` claim their suffixes; registration after freeze refused |
| `Test_CharDocumentImporter_ValidatesWithoutProducts` | same | an assembly, a control set, a project document, a visemes file, a pose book names file, a scenario and a pose snapshot import with zero products and report their rule diagnostics; none raises `COOK_WARNING_UNRECOGNISED_FILE` |

## Integration tests

`Zenith_Integration_Character`; the authority cases are specified in `Zenith2/Docs/Design/CHAR/Authority.md → "Character integration suite"`, the lip-sync cases in `Zenith2/Docs/Design/CHAR/Facial.md → "Lip-sync playback contract"` and `"Voice locale as a simulation input"`.

| Test | Stage | Asserts |
|---|---|---|
| `Test_CharIntegration_RootMotionThroughController[<Backend>]` | S3 | "Character integration suite" |
| `Test_CharIntegration_RootMotionWithoutController[<Backend>]` | S3 | same |
| `Test_CharIntegration_ControllerVelocityModeIgnoresRootMotion[<Backend>]` | S3 | same |
| `Test_CharIntegration_RagdollOwnershipSequence[<Backend>]` | S3 | same |
| `Test_CharIntegration_TeleportDuringLocomotionAndRagdoll[<Backend>]` | S3 | same |
| `Test_CharIntegration_AuthorityDeterminism` | S3 | same |
| `Test_CharIntegration_CinematicHoldRestores[<Backend>]` | S4 | same |
| `Test_CharFixtures_StageThreeRootMatchesGenerator` (`ZenithCook_Tests`) | S4 | the Stage 3 writer's root entity equals the generator's root entity byte for byte |
| `Test_CharIntegration_ModularVisibilitySwitch` | S4 | a coat switched on: the torso's pose stops being extracted, the coat's starts, no follower refused |
| `Test_CharIntegration_LipSyncPlaybackContract` | S4 | the window of tag `fr`, the subtag fallback `fr-CA` and the absent `de`; the stop on the first window end; the slot time below 2 × S every tick at `padSeconds` 0.2 and 30 Hz; the curves within 0.002 of window 1's keys at five times |
| `Test_CharIntegration_LipSyncLocaleIsSimulationInput` | S4 | a replay recorded with v = `fr` replays with identical world hashes under a presentation locale of `en` |
| `Test_CharIntegration_LipSyncLocaleAfterRestore` | S4 | the post-load voice-locale command, window 1 after the restore, and the replay of the restored session |
| `Test_CharIntegration_LipSyncLocaleUnchangedAfterRestore` | S4 | no command when the setting matches the saved tag |

## Fuzz targets

CHAR's readers live in ZenithAuthoring, which is built for `ZENITH_TOOLS_HOST` (the `win` preset) alone and is absent from `win-asan`, where fuzz targets are created (`Zenith2/Docs/Design/EDIT.md → "ZenithAuthoring contract rules"`, rule 2; `Zenith2/Docs/Architecture/BuildSystem.md → "The ZENITH_SUPPORTED matrix"`). CHAR therefore has no `Zenith_Fuzz_*` target: each reader of untrusted CHAR input is exercised by a deterministic mutated-corpus case in `Zenith2/Tools/Authoring/Tests/CHAR/Zenith_CharRobustness_Tests.cpp` (`ZenithAuthoring_Tests`, `Test` step). Each case builds its seeds in process from the constants of `Zenith_CharTestFixtures.h` (the example documents the document tests use: two per reader, so no other test case and no file on disk is involved), derives 10,000 inputs per seed with a fixed LCG (the generator of `CHAR_SIGNAL_FRICATIVE`, seed 1): bit flips, byte insertions and deletions, truncation at every 64th offset, numbers replaced by ±1e30, NaN text and 64 KiB strings, and duplicated or reordered keys; asserts that every input is accepted or refused with `Zenith_CharError` or a diagnostic, never an assert, a crash or an allocation left behind (the test heap's leak check); and asserts at the end that exactly 20,000 inputs ran.

Known limit: without AddressSanitizer an out-of-bounds read rarely crashes, so these cases are weaker than fuzz targets under ASan; CHAR's readers have no ASan coverage while ZenithAuthoring is `ZENITH_TOOLS_HOST`-only.

| Test | Reader |
|---|---|
| `Test_CharRobustness_LipSyncDocument` | the lip-sync document reader and validator |
| `Test_CharRobustness_Visemes` | the `zenith2.visemes` reader |
| `Test_CharRobustness_PoseBook` | the pose book reader and its names file |
| `Test_CharRobustness_Assembly` | the assembly reader and validator |
| `Test_CharRobustness_PoseSnapshot` | the snapshot reader |
| `Test_CharRobustness_Scenario` | the scenario reader |

## Benchmarks

`Zenith_Bench_CharMotionMatchAuthoring` ("Motion-matching authoring-cost harness" of `Zenith2/Docs/Design/CHAR/Workflow.md`); `Bench_CharMotionMatchAuthoring_Smoke` is its CTest smoke case in the `Test` step: one C = 10 database build and one pose of each candidate through the harness-local evaluator, passing when both succeed and the report file parses. The harness generates its own source frames and links no CHAR test support.

## Fixtures and generators

`Zenith2/Tools/Authoring/Tests/Support/Zenith_CharTestFixtures.h` is header-only ([Z2-N-ARCH-255], X184): inline generators returning in-memory sources from the constants below. Every sine is `Zenith_Sin`. Every GUID a generator writes is a **named** constant of the header (table "Fixture identities" below), never a number counted in write order, so the Stage 3 and Stage 4 writers derive the same IDs; ASSET derives no sub-asset GUID: a product key the `.meta`'s `subAssets` lists keeps its GUID and an unlisted key gets a new one (`Zenith2/Docs/Design/ASSET/Database.md → "Sub-asset identity"`, `Zenith2/Docs/Design/ASSET/Import.md → "Identity preservation on reimport"`), so every fixture `.meta` the setup writes lists its products' named GUIDs in `subAssets` (`{ "key", "guid", "kind" }`, sorted by key, `Zenith2/Docs/Design/ASSET/Formats.md → "Sub-asset entries"`), and a document's primary product is the document's own GUID. Both writers name products through the one helper `Zenith_CharFixtureProductID(const Zenith_PersistentID& xSource, Zenith_StringView strProductKey)`, a lookup over the table below that returns the constant listed for (source, key), returns the source GUID itself for a document's primary product (empty key), and `ZENITH_ASSERT`s on any other pair.

**The humanoid layout** (`aCHAR_FIXTURE_JOINTS`, 62 joints, one per `Zenith_JointRole` 1 … 62, in this joint order; rest local translation = own position − parent's, identity rotation, unit scale; positions in metres, model space, left at +x):

| Joints | Zenith-style name | Mixamo-style name | Parent | Rest model position |
|---|---|---|---|---|
| 0 | `hips` | `Hips` | — | (0, 0.95, 0) |
| 1 | `spine` | `Spine` | 0 | (0, 1.05, 0) |
| 2 | `chest` | `Spine1` | 1 | (0, 1.20, 0) |
| 3 | `upper_chest` | `Spine2` | 2 | (0, 1.35, 0) |
| 4 | `neck` | `Neck` | 3 | (0, 1.50, 0) |
| 5 | `head` | `Head` | 4 | (0, 1.60, 0) |
| 6, 7 | `eye_l`, `eye_r` | `LeftEye`, `RightEye` | 5 | (±0.03, 1.68, 0.08) |
| 8 | `jaw` | `jaw` | 5 | (0, 1.58, 0.03) |
| 9 | `tongue` | `tongue` | 8 | (0, 1.57, 0.06) |
| 10, 11 | `brow_l`, `brow_r` | as Zenith | 5 | (±0.03, 1.72, 0.09) |
| 12, 13 | `lid_upper_l`, `lid_upper_r` | as Zenith | 5 | (±0.03, 1.69, 0.09) |
| 14, 15 | `lid_lower_l`, `lid_lower_r` | as Zenith | 5 | (±0.03, 1.67, 0.09) |
| 16 (left), 35 (right) | `shoulder_<s>` | `<S>Shoulder` | 3 | (±0.05, 1.45, 0) |
| 17, 36 | `upper_arm_<s>` | `<S>Arm` | the shoulder | (±0.18, 1.45, 0) |
| 18, 37 | `lower_arm_<s>` | `<S>ForeArm` | the upper arm | (±0.45, 1.45, 0) |
| 19, 38 | `hand_<s>` | `<S>Hand` | the lower arm | (±0.70, 1.45, 0) |
| 20–22, 39–41 | `thumb_<k>_<s>` | `<S>HandThumb<k>` | the hand, then phalanx k − 1 | (±(0.72 + 0.03 (k − 1)), 1.43, 0.04) |
| 23–25, 42–44 | `index_<k>_<s>` | `<S>HandIndex<k>` | as thumb | (±(0.78 + 0.03 (k − 1)), 1.45, 0.03) |
| 26–28, 45–47 | `middle_<k>_<s>` | `<S>HandMiddle<k>` | as thumb | (±(0.78 + 0.03 (k − 1)), 1.45, 0.01) |
| 29–31, 48–50 | `ring_<k>_<s>` | `<S>HandRing<k>` | as thumb | (±(0.78 + 0.03 (k − 1)), 1.45, −0.01) |
| 32–34, 51–53 | `little_<k>_<s>` | `<S>HandPinky<k>` | as thumb | (±(0.77 + 0.03 (k − 1)), 1.45, −0.03) |
| 54, 58 | `upper_leg_<s>` | `<S>UpLeg` | 0 | (±0.10, 0.92, 0) |
| 55, 59 | `lower_leg_<s>` | `<S>Leg` | the upper leg | (±0.10, 0.50, 0) |
| 56, 60 | `foot_<s>` | `<S>Foot` | the lower leg | (±0.10, 0.08, 0) |
| 57, 61 | `toes_<s>` | `<S>ToeBase` | the foot | (±0.10, 0.02, 0.12) |

(`<s>` is `l` or `r`, `<S>` `Left` or `Right`, + for left.) The mannequin-style and Rigify-style names are the "Rig profile table" names of each role in that style's own spelling (`thigh_l`, `DEF-thigh.L`), and joints the style names nowhere take the Zenith-style name; the unnamed-token style names joint j `Bone_<Word>_<Side>` from the generic profile's keywords (`Bone_Pelvis`, `Bone_Spine_1` … `Bone_Spine_3`, `Bone_Neck`, `Bone_Head`, `Bone_Eye_Left`, `Bone_Clavicle_Left`, `Bone_UpperArm_Left`, `Bone_Forearm_Left`, `Bone_Hand_Left`, `Bone_Thumb_1_Left`, `Bone_Thigh_Left`, `Bone_Calf_Left`, `Bone_Foot_Left`, `Bone_Toe_Left`, …). The layout gives a height of 1.60 − 0.02 + 0.1 = 1.68 m, both arm elevations 0° (T-pose), facing +Z.

| Generator | Produces |
|---|---|
| `Zenith_CharMakeSkeletonView(eNaming, eRestPose, uFlags)` | the layout as a `Zenith_CharSkeletonView`; `eNaming` one of `CHAR_FIXTURE_NAMING_ZENITH`, `_MIXAMO`, `_MANNEQUIN`, `_RIGIFY`, `_UNNAMED`; `eRestPose` T, or A (local rotations of −45° about +Z on `upper_arm_l` and +45° on `upper_arm_r`, descendants' model positions following); `uFlags` bit 0 a 180° rotation about +Y at the hips, bit 1 x negated, bit 2 every position × 100 |
| `Zenith_CharMakeHumanoidGltf(eNaming, eMeshes, uMorphTargets, bClips)` | a glTF 2.0 binary: the joints as nodes with their rest TRS, one skin over every joint (inverse binds the inverse rest model matrices), the meshes of `eMeshes` below, `uMorphTargets` (0 or 13) targets on `Head`, and with `bClips` the clips of the clip table as glTF animations named by the table |
| `Zenith_CharMakeLocomotionClips` | the clip table below, as glTF animation channels and as the clip annotation settings each `anim:<Name>` product carries |
| `Zenith_CharMakeLibraryMeta`, `Zenith_CharMakeRigDocument(eNaming)` | the library rig's `.meta` (fixed GUID, labels `char-library-rig`, every product's annotation) and a rig document with every role of the layout |
| `Zenith_CharMakeSignal(eKind, fSeconds, uRateHz, uSeed)` | the estimator's test signals below, `round(fSeconds × uRateHz)` float samples |
| `Zenith_CharMakeWav(xSignal)` | a mono PCM16 RIFF WAV, each sample `clamp(round(x × 32,767), −32,768, 32,767)` |
| `Zenith_CharMakeBankDocument(lines, locales, phraseMarkers)` | an AUDIO bank document of dialogue lines referencing the WAVs, locale entries in the given order, and the given markers on a sound (name, seconds, flag bit 1 lip-sync phrase start; AUDIO's bank-document marker source) |
| `Zenith_CharMakeProjectDocument(locales, libraryRig)` | a project document with the three default classes of "Character project settings document" and `hero.libraryRig` |
| `Zenith_CharMakeFixtureAssembly()` | the integration fixture's assembly document of the table "Integration fixture assembly" below |

**Meshes** (`eMeshes`). Each listed joint contributes an axis-aligned box of half extent h centred at the midpoint of the joint and its first child (a joint without children: the joint + (0, 0.05, 0)): 24 vertices with face normals, 36 indices, every vertex skinned with weight 1 to that joint.

| Mode | Meshes (joints; h; material) |
|---|---|
| `CHAR_FIXTURE_MESHES_WALKTHROUGH` | `Head` (head; 0.06; `Skin`), `Torso` (hips, spine, chest, upper chest), `Arms` (both upper and lower arms), `Hands` (both hands), `Legs` (both upper and lower legs), `Feet` (both feet), all h 0.06 and `Skin`; `Coat` (spine, chest, upper chest, both upper and lower arms; 0.08; `Cloth`); `HairCards`: four 0.2 × 0.2 m quads at head + (0, 0.1, 0), turned 0°, 45°, 90° and 135° about +Y, skinned to the head, material `Hair` |
| `CHAR_FIXTURE_MESHES_REGIONS` | the six region meshes above |
| `CHAR_FIXTURE_MESHES_PROXY` | `Proxy` (hips; 0.1; `Skin`) |

Materials: `Skin` and `Cloth` opaque; `Hair` `alphaMode` MASK, `alphaCutoff` 0.5, `doubleSided` true, which CONTENT's import maps to GEOM's `alphaTest` domain and `twoSided` (`Zenith2/Docs/Design/GEOM/Materials.md → "Material document schema"`). The thirteen morph targets, in this order, displace every vertex of `Head` by (0, 0, 0.001 × (k + 1)) for target k: `jawOpen`, `mouthClose`, `mouthFunnel`, `mouthPressLeft`, `mouthPressRight`, `mouthPucker`, `mouthRollLower`, `mouthSmileLeft`, `mouthSmileRight`, `mouthStretchLeft`, `mouthStretchRight`, `mouthUpperUpLeft`, `mouthUpperUpRight` (every curve of the ARKit combination table of "Automatic viseme pose books").

**Fixture identities** (`Zenith_CharTestFixtures.h` constants, version-4 form):

| Constant | GUID | Names |
|---|---|---|
| `xCHAR_FIXTURE_HUMANOID_SOURCE_GUID` | `00000000-0000-4000-8000-000000000101` | the integration humanoid glTF's `.meta`; its skeleton product is `skel:hips` |
| `xCHAR_FIXTURE_GRAPH_GUID` | `00000000-0000-4000-8000-000000000102` | the integration graph document (`id` and `.meta`) |
| `xCHAR_FIXTURE_RAGDOLL_GUID` | `00000000-0000-4000-8000-000000000103` | the integration ragdoll document's `.meta` |
| `xCHAR_FIXTURE_ASSEMBLY_GUID` | `00000000-0000-4000-8000-000000000104` | the fixture assembly document |
| `xCHAR_FIXTURE_BASE_PREFAB_GUID` | `00000000-0000-4000-8000-000000000105` | the base prefab (both writers; the root's ID is `Zenith_DeriveAuthoringID(this, L("root"))`) |
| `xCHAR_FIXTURE_TRAVEL_PREFAB_GUID` | `00000000-0000-4000-8000-000000000106` | the Travel loadout variant |
| `xCHAR_FIXTURE_HUMANOID_SKELETON_GUID` | `00000000-0000-4000-8000-000000000107` | the humanoid's `skel:hips` product (`subAssets` of `xCHAR_FIXTURE_HUMANOID_SOURCE_GUID`'s `.meta`, kind `Skeleton`) |
| `axCHAR_FIXTURE_MESH_GUIDS[8]` | `…0131` … `…0138` | its `mesh:Head`, `mesh:Torso`, `mesh:Arms`, `mesh:Hands`, `mesh:Legs`, `mesh:Feet`, `mesh:Coat`, `mesh:HairCards` products (kind `Mesh`; the materials `material:Skin`, `material:Cloth`, `material:Hair` are `…0141` … `…0143`) |
| `xCHAR_FIXTURE_CLIP_WALK_GUID`, `xCHAR_FIXTURE_CLIP_GETUP_GUID` | `…0108`, `…0109` | its `anim:IntegrationWalk` and `anim:GetUp` products (kind `AnimationClip`), which the integration graph names |
| `axCHAR_FIXTURE_PART_GUIDS[8]` | `…0111` … `…0118` | parts `Head`, `Torso`, `Arms`, `Hands`, `Legs`, `Feet`, `Coat`, `HairCards` |
| `xCHAR_FIXTURE_DEFAULT_LOADOUT_GUID`, `xCHAR_FIXTURE_TRAVEL_LOADOUT_GUID` | `…0121`, `…0122` | the two loadouts |
| `xCHAR_FIXTURE_LIBRARY_SOURCE_GUID` | `00000000-0000-4000-8000-000000000201` | the walkthrough library rig's `.meta` |
| `xCHAR_FIXTURE_LIBRARY_SKELETON_GUID` | `00000000-0000-4000-8000-000000000204` | the library rig's `skel:hips` product (`subAssets` of its `.meta`); `hero.libraryRig` names it |
| `axCHAR_FIXTURE_LIBRARY_CLIP_GUIDS[7]` | `…0211` … `…0217` | the library rig's `anim:Idle`, `anim:Walk`, `anim:Jog`, `anim:Run`, `anim:JumpStart`, `anim:JumpLoop`, `anim:Land` products (the library mesh `mesh:Proxy` is `…0218`) |
| `xCHAR_FIXTURE_BANK_GUID`, `xCHAR_FIXTURE_PROJECT_GUID` | `…0202`, `…0203` | the walkthrough bank and project document |

**Integration fixture assembly** (`Zenith_CharMakeFixtureAssembly`):

| Member | Value |
|---|---|
| `id`, `name`, `class` | `xCHAR_FIXTURE_ASSEMBLY_GUID`, `IntegrationRanger`, `hero` (so `SignificanceBias` 4.0 and `ANIMATOR_FLAG_FORCE_FULL_RATE`, no reduced-rate root motion: the authority scenarios evaluate every tick) |
| `skeleton`, `graph`, `ragdoll` | the `skel:hips` product of `xCHAR_FIXTURE_HUMANOID_SOURCE_GUID`, the graph product of `xCHAR_FIXTURE_GRAPH_GUID`, the ragdoll product of `xCHAR_FIXTURE_RAGDOLL_GUID`, each through `Zenith_CharFixtureProductID` |
| `regions`, `slots` | Head, Torso, Arms, Hands, Legs, Feet; `Body` (non-exclusive), `Hair`, `Outfit` (exclusive) |
| `parts` | the eight parts of `axCHAR_FIXTURE_PART_GUIDS` on the meshes of `CHAR_FIXTURE_MESHES_WALKTHROUGH` (six Body parts with their regions, `Coat` in Outfit hiding Torso and Arms, `HairCards` in Hair with `hairCards` true) |
| `loadouts`, `defaultLoadout` | `Default` (the six Body parts and `HairCards`), `Travel` (all eight); default `Default` |
| `rootComponents` | `{ "Zenith.Physics.CharacterController": { "version": 1, "fields": { "1": 0.3, "2": 1.6, "11": 2 } } }`: Radius 0.3 m, Height 1.6 m, Mode 2 `ROOT_MOTION_WITH_GRAVITY` (state L1 of "Character authority matrix"); every other property at PHYS's registered default (`Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Zenith_CharacterController header"`), the L2 and L3 cases changing Mode or removing the component by command at run time |
| `generated` | `base` → `xCHAR_FIXTURE_BASE_PREFAB_GUID`, `Travel` → `xCHAR_FIXTURE_TRAVEL_PREFAB_GUID` |

The integration graph has no `ik` array, so neither writer emits `Zenith.Animation.IKGoals`. The Stage 3 writer `Zenith_CharWriteStageThreeRootPrefab` writes a prefab of ID `xCHAR_FIXTURE_BASE_PREFAB_GUID`, `name` `IntegrationRanger`, whose single entity is the root of "Generated prefab layout" for this table: `Zenith.LocalTransform` (identity); `Zenith.Animation.Animator` (Graph, PlayRate 1, RootMotion `EXTRACT`, Enabled true, SignificanceBias 4.0, Flags `ANIMATOR_FLAG_FORCE_FULL_RATE`); `Zenith.Animation.RootMotion` and `Zenith.Animation.Pose` with no fields; `Zenith.Physics.Ragdoll` (property 3 the ragdoll product); and the `rootComponents` object above verbatim, with the same constants the generator reads.

**Clip table** (`aCHAR_FIXTURE_CLIPS`; keys at 30 Hz, linear, `duration × 30 + 1` keys; the hips translate along +Z at the root speed, annotated `rootMotion { "mode": "XZYaw", "joint": "hips" }`; an "upper legs ±a°" motion is a rotation about +X of `a × Zenith_Sin(2 × dPI × t / period)` degrees on `upper_leg_l` and its negation on `upper_leg_r`; events are instant, fire mode weighted, `minWeight` 0.5, flags gameplay):

| Name | Duration (s) | Loop | Root speed (m/s) | Joint motion | Markers | Events |
|---|---|---|---|---|---|---|
| `Idle` | 2.0 | yes | 0 | `spine` ±2° about +X, period 2.0 | — | — |
| `Walk` | 1.0 | yes | 1.4 | upper legs ±25°, period 1.0 | `Foot.L` 0.0, `Foot.R` 0.5 | `Footstep` 0.0 (payload 0), 0.5 (payload 1) |
| `Jog` | 0.8 | yes | 3.5 | upper legs ±30°, period 0.8 | `Foot.L` 0.0, `Foot.R` 0.4 | `Footstep` 0.0, 0.4 |
| `Run` | 0.6 | yes | 6.0 | upper legs ±40°, period 0.6 | `Foot.L` 0.0, `Foot.R` 0.3 | `Footstep` 0.0, 0.3 |
| `JumpStart` | 0.4 | no | 0 | both lower legs from 0° to −30° about +X, linear | — | `Jump.TakeOff` 0.35 |
| `JumpLoop` | 0.5 | yes | 0 | both lower legs at −30° | — | — |
| `Land` | 0.5 | no | 0 | both lower legs from −30° to 0°, linear | — | `Footstep` 0.1 (payload 2) |
| `IntegrationWalk` | 2.0 | yes | 1.4 | as `Walk` | `Foot.L` 0.0, 1.0; `Foot.R` 0.5, 1.5 | `Footstep` at the four marker times |
| `GetUp` | 1.0 | no | 0 | the hips from (0, 0.2, 0) to (0, 0.95, 0), linear | — | — |

The library rig (the walkthrough's `Humanoid.glb`) carries the first seven rows; the integration fixture the rows `IntegrationWalk` and `GetUp`.

**Signals** (`Zenith_CharMakeSignal`; sample n at rate R; `A12` = 0.251189 and `A24` = 0.0630957, the amplitudes of −12 and −24 dBFS written as literals):

| Kind | x[n] |
|---|---|
| `CHAR_SIGNAL_SILENCE` | 0 |
| `CHAR_SIGNAL_VOWEL_OU` | `A12 × (sin(2π 250 n / R) + sin(2π 500 n / R))` |
| `CHAR_SIGNAL_VOWEL_AA` | `A12 × (sin(2π 700 n / R) + sin(2π 1,100 n / R) + sin(2π 1,400 n / R))` |
| `CHAR_SIGNAL_VOWEL_IH` | `A24 × sin(2π 300 n / R) + A12 × (sin(2π 2,500 n / R) + sin(2π 3,200 n / R))` |
| `CHAR_SIGNAL_FRICATIVE` | `0.1 × (e[n] − e[n − 1])` with `e[−1] = 0`, `e[n] = 2 × s_{n+1} / 2³² − 1`, `s_0 = uSeed` (default `0x2545F491`), `s_{k+1} = (1,664,525 × s_k + 1,013,904,223) mod 2³²` in 32-bit unsigned arithmetic: white LCG noise, differenced once |
| `CHAR_SIGNAL_BURST` | 0 for `n < round(0.2 × R)`, else `CHAR_SIGNAL_VOWEL_AA`'s x[n] |

The fixture sets are `CHAR_IntegrationFixtures` (`Zenith2/Docs/Design/CHAR/Authority.md → "Integration fixtures"`) and `CharWalkthroughSources` (`Zenith2/Docs/Design/CHAR/Workflow.md → "Walkthrough fixtures"`); the cook-step tests use ASSET's cook test access over in-memory sources.
