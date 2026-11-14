# CHAR design: pose debugging, animation budgeting workflow and validation

Section file of `Zenith2/Docs/Design/CHAR.md`. It specifies the editor's pose-debugging tools (the pose debugger panel, its overlays, pose snapshots and their diff, the pose history recorder), the production side of animation update budgeting (character class presets, the budget panel, the class lint and the content-rule measures for character classes), and the validation catalogue that gathers every CHAR check into one report. The runtime of update budgeting is ANIM's (`Zenith2/Docs/Design/ANIM/Runtime.md → "Update-rate budgeting"`, [Z2-N-BUDGET-438]); the debug state the tools display is ANIM's (`Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationWorld header"`, `GetDebugState`, `GetStats`); the editor shell and panels are EDIT's (`Zenith2/Docs/Design/EDIT/Panels.md → "Extension interface"`).

## Scope of this file

| Topic | Heading |
|---|---|
| inspecting a live pose | "Pose debugger panel", "Pose sources", "Joint table", "Skeleton overlays" |
| freezing and comparing poses | "Pose snapshot format", "Pose diff algorithm" |
| looking back in time | "Pose history recorder" |
| budgeting as authored data | "Character class presets", "Budget panel", "Class rate lint" |
| ContentSpecs character classes at cook | "Content rule measures" |
| one report of every check | "Validation catalogue", "Validation panel" |

## Pose debugger panel

Panel type `Zenith.Editor.Character.PoseDebugger` (multi-instance, menu `Window/Character/Pose Debugger`), registered by CHAR's editor extension (`Zenith2/Docs/Design/CHAR/Workflow.md → "Editor extension"`). It shows one character at a time, chosen from the selection or pinned, and has four regions:

| Region | Content |
|---|---|
| header | the pose source ("Pose sources"), the entity's name and simulation ID, ANIM's status, rate divisor and phase offset, significance and distance to the camera, the graph hash, the last evaluation tick (all from `Zenith_AnimDebugState`) |
| graph state | per layer: weight, the first machine's state, time in state, normalised time, transition progress; per slot: clip, time, weight, tag; the latest events (`m_axLastEvents`); the inertialiser's state; the parameters (`m_aulParameterHash` resolved to names through the bound graph document) |
| joint table | "Joint table" |
| actions | Snapshot, Compare, Pin, Freeze (pause the play world, `play.pause`), Step (`play.step 1`), Record history ("Pose history recorder") |

The panel reads state once per editor frame, after the frame's simulation, never during a tick; it changes no simulation state (`Test_CharPoseDebugger_ReadsWithoutWriting` asserts identical world-state hashes of a play world with and without the panel open over 300 ticks).

## Pose sources

| Source | Read through | Available |
|---|---|---|
| a play world's character | `Zenith_AnimationWorld::GetDebugState` and `Zenith_AnimationFinalPoses::GetFinalModelPose` of the play world's resources, as EDIT's live animation debugger reads them (`Zenith2/Docs/Design/EDIT/AuthoringTools.md → "Live animation debugger"`) | development builds of the editor; `ANIMATION_ERROR_NOT_AVAILABLE` never occurs in the editor |
| an animation preview | `Zenith_AnimationPreview::GetModelPose` and `GetDebugState` of the preview panel's preview (`Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationPreview header"`) | always, headless included |
| a snapshot file | "Pose snapshot format" | always |

<!-- assumption: pending EDIT (row CH5) -->
The editor reaches a play world's `Zenith.Animation.World`, `Zenith.Animation.FinalPoses` and `Zenith.Animation.Residency` resources outside a frame through the same accessor EDIT's live animation debugger uses; row CH5 asks EDIT to name that accessor (`Zenith2/Docs/Design/EDIT/Foundation.md → "Play in editor"` holds the play world's simulation), and the pose debugger calls nothing else.
<!-- /assumption -->

## Joint table

One row per joint in joint order, collapsible by hierarchy:

| Column | Value |
|---|---|
| name, role | the skeleton's joint name; the rig document's role, if any |
| local t, r (degrees as Euler XYZ for display; the quaternion in a tooltip), s | the final local transform: from the model pose, `local_j = inverse(model_parent(j)) · model_j`, decomposed with `Zenith_DecomposeAffine` (`Zenith2/Docs/Design/SCENE/Transforms.md → "Transform value type"`) |
| model t | the translation of `model_j` in the entity's model space, metres |
| Δ rest | translation distance (mm) and rotation angle (degrees) of the local transform from the rest local transform |
| flags | shear (`bHasShearOut` of the decomposition: never expected), non-finite (any component not finite), scaled (a scale component outside [0.999, 1.001]) |

Rows with a flag sort first when "problems first" is on; a non-finite or sheared joint is shown in red and counted in the header. `Test_CharPoseDebugger_JointTableFromModelPose` asserts the decomposition and the Δ rest columns on a preview of the fixture skeleton posed by a fixture clip.

## Skeleton overlays

Drawn into the panel's own preview viewport and, when "show in scene view" is on, into every scene view's gizmo line list (`Zenith2/Docs/Design/EDIT/Viewports.md → "Overlay records"`, `m_axGizmoLines`):

| Overlay | Geometry | Colour |
|---|---|---|
| bones | one line per joint from the parent's model position to its own | by rate tier: k = 1 white, 2 cyan, 4 yellow, 8 orange (ANIM's rate-tier colouring, `Zenith2/Docs/Design/ANIM.md → "Debug views"`) |
| joint axes | three lines of 0.05 m (scaled by `editor.character.axisLength`) along the joint's model X, Y, Z | red, green, blue |
| rest ghost | the rest pose's bones, placed at the entity's world transform | grey at 50% alpha |
| mirror links | a dashed line (8 segments) between each mirror pair | magenta |
| sockets | a 0.03 m axis triad at each socket's model transform | as joint axes |
| names | the joint names, drawn by ImGui at the projected positions in the panel's viewport only | white |

<!-- assumption: pending EDIT (row CH5) -->
Contributing lines to scene views needs an extension point that appends to a scene view's overlay line list; row CH5 asks EDIT for `Zenith_EditorExtensionContext::RegisterOverlayContributor(Zenith_PfnEditorOverlayContributor)`, called on the main thread while the overlay snapshot is built. Without it, the overlays draw in the panel's viewport only.
<!-- /assumption -->

## Pose snapshot format

A pose snapshot freezes one pose for comparison and for automation assertions. File `*.posesnapshot.json`, format `zenith2.char-pose-snapshot`, version 1; a development artefact, not an authoring document (no migration chain: a reader refuses a version it does not know with `CHAR_ERROR_SNAPSHOT_INVALID`):

```json
{
	"$schema": "zenith2.char-pose-snapshot",
	"schemaVersion": 1,
	"skeleton": "1b7e…",
	"restPoseHash": "8c1f00a3d2b77e10",
	"source": { "kind": "play", "entity": "5e0b…", "tick": 1840 },
	"joints": [
		{ "name": "hips", "t": [0.0, 0.973, 0.012], "r": [0.0, 0.0174, 0.0, 0.9998], "s": [1.0, 1.0, 1.0] }
	],
	"curves": { "char.viseme.aa": 0.42 }
}
```

`joints` holds every joint in joint order with its final local transform (floats written with 9 significant digits, so a round trip of an f32 is exact); `source.kind` is `play`, `preview` or `file`; `curves` every curve whose value differs from its default by more than 1e-6. `Zenith_CharWritePoseSnapshot` and `Zenith_CharReadPoseSnapshot` (`Zenith2/Docs/Design/CHAR.md → "Pose debugging API"`) are the writer and the validating reader (at most 1,024 joints and 512 curves, the document limit 64 MiB). `Test_CharPoseSnapshot_RoundTripExact` asserts a written and re-read snapshot equals the original bit for bit; `Test_CharPoseSnapshot_RejectsUnknownVersion` and `Test_CharPoseSnapshot_RejectsOverLimits` cover the reader.

## Pose diff algorithm

`Zenith_CharDiffPoses(const Zenith_CharPoseSnapshot& xA, const Zenith_CharPoseSnapshot& xB, const Zenith_CharPoseDiffTolerance&, Zenith_CharPoseDiff& xOut)`:

1. Pair the joints: by index when both `restPoseHash` values are equal, else by normalised name (`Zenith2/Docs/Design/CHAR/Rigging.md → "Joint name normalisation"`), unpaired joints listed; curves by name.
2. Per pair, in binary64: `dt = |t_A − t_B|` (metres); `dr = 2 × Zenith_Acos(c)` with `c = min(1, |dot(r_A, r_B)|)` (radians, the sign-independent angle, CORE's deterministic `Zenith_Acos` and no CRT call, "Estimator input" of `Zenith2/Docs/Design/CHAR/Facial.md`); `ds = max_i |s_A,i − s_B,i|`.
3. A pair differs when `dt > tolerance.metres` (default 1e-4), `dr > tolerance.radians` (1e-3) or `ds > tolerance.scale` (1e-4); a curve differs when `|v_A − v_B| > tolerance.curve` (1e-4).
4. The result: the differing joints in joint order with their three deltas, the maximum of each delta and its joint, the differing curves, and the unpaired names.

O(J log J) for the name pairing, O(J) otherwise. The pose debugger's Compare action shows the result as a highlighted joint table and a red bone overlay on differing joints. `Test_CharPoseDiff_DetectsEachDelta` asserts one differing joint per delta kind; `Test_CharPoseDiff_SignIndependentRotation` asserts `q` and `−q` give `dr = 0`; `Test_CharPoseDiff_PairsByNameAcrossRigs` asserts name pairing between a T-posed and an A-posed fixture rig.

## Pose history recorder

When Record history is on for a play-world character, the panel appends, after every tick the play world ran, the character's final local pose and curves to a ring of `editor.character.poseHistoryTicks` entries (default 300, range 30 … 3,600), at most 4 characters at once (`uCHAR_MAX_RECORDED_CHARACTERS`); memory is `ticks × (40 × J + 4 × C)` bytes per character (720 KB at the default for J = 60, C = 0), from the editor's heap, registered with EDIT's cache consumer (`Zenith2/Docs/Design/EDIT/ToolDeclarations.md → "Cache consumer header"`) so memory pressure frees the oldest half of each ring. A scrubber selects a recorded tick; the joint table and overlays then show that entry, and Snapshot writes it. A paused world adds no entries; a play world that stops discards its rings. `Test_CharPoseHistory_RingOrderAndCapacity` asserts the ring's order and overwrite at capacity; `Test_CharPoseHistory_ShedsHalfUnderPressure` asserts the consumer's release.

## Character class presets

Animation update budgeting is a runtime mechanism of ANIM, steered per animator by `SignificanceBias` and the flags `ANIMATOR_FLAG_FORCE_FULL_RATE` and `ANIMATOR_FLAG_ALLOW_REDUCED_ROOT_MOTION` (`Zenith2/Docs/Design/ANIM.md → "Zenith_AnimationComponents header"`), and per world by the simulation-affecting `animation.budget.*` keys (`Zenith2/Docs/Design/ANIM.md → "Configuration keys"`). CHAR makes the per-animator values authored data: each character class of the project document (`Zenith2/Docs/Design/CHAR/Facial.md → "Character project settings document"`) carries `significanceBias`, `forceFullRate` and `allowReducedRootMotion`, and the prefab generator writes them into the root's animator ("Generated prefab layout" of `Zenith2/Docs/Design/CHAR/Modular.md`). The engine's default classes are `hero` (bias 4, forced full rate: the player and cinematic leads), `npc` (bias 1) and `crowd` (bias 0.5, reduced-rate root motion allowed); a game edits its project document to add or tune classes, and the world-wide distances stay ANIM's keys. CHAR's tools never write `animation.budget.*` keys: they are configuration, set per platform by ANIM's Stage 4 budget ruling (`Zenith2/Docs/Design/ANIM.md → "Pending row needs"`).

## Budget panel

Panel type `Zenith.Editor.Character.Budget` (single, menu `Window/Character/Animation Budget`), over a play world:

| Section | Content | Source |
|---|---|---|
| tiers | instances per rate divisor 1, 2, 4, 8; evaluations and skips this tick; joints evaluated | `Zenith_AnimationWorld::GetStats` (`m_auInstancesByRate`, `m_uEvaluations`, `m_uEvaluationsSkipped`, `m_ulJointsEvaluated`) |
| per character | name, class, bias, distance, significance, divisor, phase offset; sorted by significance | `GetDebugState` of each animator, at most 256 rows per frame in ascending simulation ID, then sorted |
| what-if | the divisor every character would get with edited distances or biases | `Zenith_CharPredictRateDivisor(float fDistanceMetres, float fBias, uint32 uAnimatorFlags, bool bRagdollActive, const Zenith_CharBudgetKeys& xKeys)` (`Zenith2/Docs/Design/CHAR.md → "Pose debugging API"`), which calls ANIM's own rate function (below) with the forcing input `(uAnimatorFlags & ANIMATOR_FLAG_FORCE_FULL_RATE) != 0 || bRagdollActive`; the other forcing conditions of `Zenith2/Docs/Design/ANIM/Runtime.md → "Rate tiers and phase offsets"` (a cinematic or full-rate slot, a warp window, the first evaluation) are run-time instance state the panel shows as flags from `GetDebugState`; a prediction, never applied |
| cost | the phase-3 time of the play world's `Animation.Evaluate` zone against `Z2-B-ANIM-002` | DEVT's profiler zone read for the play world (`Zenith2/Docs/Design/DEVT/Profiler.md → "Zenith_Profiler header"`) |

CHAR does not restate ANIM's rule: the divisor is computed by ANIM's function, the one `Animation.Significance` itself calls.

<!-- assumption: pending ANIM (row CH15) -->
ANIM declares, in a public header of ZenithAnimation (`Zenith2/Engine/ZenithAnimation/Zenith_AnimationWorld.h`), the pure function `uint8 Zenith_AnimComputeRateDivisor(float fDistanceMetres, float fBias, bool bForceFullRate, const Zenith_AnimBudgetDistances& xDistances)` (1, 2, 4 or 8), with `Zenith_AnimBudgetDistances` holding the three `animation.budget.*` distances, and `Animation.Significance` calls it for every instance. Until it lands, the budget panel's what-if column is absent and its ticket stays `draft`.
<!-- /assumption -->

`Test_CharBudget_PredictMatchesAnimRule` (`ZenithAuthoring_Tests`) asserts, for twelve (distance, bias) cases spanning the three distance keys at the default distances, that `Zenith_CharPredictRateDivisor` returns `Zenith_AnimComputeRateDivisor`'s value, and that `ANIMATOR_FLAG_FORCE_FULL_RATE` and `bRagdollActive` each give 1; `Test_CharBudget_PanelRowCap` (`ZenithEditor_Tests`) asserts the 256-row cap.

## Class rate lint

Rule AU9 of `Zenith2/Docs/Design/CHAR/Authority.md → "Authority lint rules"` (a `forceFullRate` class without the flag) and two further warnings, run by the validation catalogue on every character prefab:

| Rule | Check | Diagnostic |
|---|---|---|
| CB1 | a prefab's animator `SignificanceBias` differs from its class's `significanceBias` | `CHAR_DIAG_BUDGET_BIAS_OVERRIDDEN` (information: a deliberate per-character tuning is allowed) |
| CB2 | a scene places more instances of `forceFullRate` classes than the project's `maxFullRateCharactersPerScene` (default 8) | `CHAR_DIAG_BUDGET_TOO_MANY_HEROES` (warning: full-rate characters bypass the budget) |

## Content rule measures

The character-class table of ContentSpecs ([Z2-N-BUDGET-415]: bones, blend shapes, animation layers, IK chains and LOD0 triangles maximum per character class, IDs `CHARACTER_CLASS_<NAME>`) is checked at cook by ASSET's content-rule mechanism (`Zenith2/Docs/Design/ASSET/Cook.md → "Content rules"`): the driver pairs each table row of an item's kind with the measure that kind's owner registered, and picks the class by the item's `.meta` labels. CHAR registers no measure and reads no cooked byte of another area's format, because "the owning area's step registers the measure of each quantity" and a measure written against another owner's layout breaks silently when that layout changes:

| Kind (script name) | Quantity | Owner that registers the measure |
|---|---|---|
| `Skeleton` | `bones`, `ikChains` | ANIM |
| `AnimationGraph` | `layers` | ANIM |
| `Mesh` | `blendShapes`, `triangles` | GEOM |

CHAR's part is the class selection: each class of the project document carries its label (`character-<class>`, "Character project settings document" of `Zenith2/Docs/Design/CHAR/Facial.md`), the import wizard writes that label into the character source's `.meta` ("Character import wizard" of `Zenith2/Docs/Design/CHAR/Workflow.md`), and the row selector of each `CHARACTER_CLASS_<NAME>` row in `Zenith2/Docs/ContentSpecs.rules.json` is the label `character-<name>` (core-doc item X340 (2)); the default classes `hero`, `npc` and `crowd` are proposed for ContentSpecs by core-doc item X330. A label with no row falls back to ASSET's `*` class, which is why validation rule CV17 warns about it. The ContentSpecs column "Update-rate reduction tiers" documents the divisors a class may reach; CHAR's class presets are what implements them (a class whose row lists only tier 1 is a `forceFullRate` class, every other class allows 1, 2, 4 and 8), and the ContentSpecs writer fills that column from the three default classes (core-doc item X360 (2)). CHAR's validation never reads the Markdown.

<!-- assumption: pending ANIM, GEOM (rows CH15, CH17) -->
Rows CH15 and CH17 ask ANIM and GEOM to register the five measures above in their own cook-step registration functions (CH7's option (a) is withdrawn). Until they land, character-class rules for those quantities are `COOK_ERROR_RULE_TABLE_INVALID` for a table row naming them, so the ContentSpecs writer adds those rows only after the measures exist.
<!-- /assumption -->

`Test_CharContentRules_ClassLabelSelectsLimit` (`ZenithCook_Tests`) cooks a 70-joint skeleton source labelled `character-crowd` against a fixture rule table whose `CHARACTER_CLASS_CROWD` row (selector `character-crowd`) limits `bones` to 64, and asserts `COOK_ERROR_RULE` naming the table, the class and both values; the measure it exercises is ANIM's, so the test is blocked by row CH15.

## Validation catalogue

`Zenith_CharValidate(const Zenith_CharValidationRequest&, Zenith_CharAssetQuery&, Zenith_Array<Zenith_CharDiagnostic>&)` runs, for the documents of the request, every check CHAR defines, in this order, and returns the diagnostics sorted by (document path, severity, rule, message):

| Rule | Applies to | Check (specified in) |
|---|---|---|
| CV1 | rig documents | profile completeness ("Profile matching algorithm" of `Zenith2/Docs/Design/CHAR/Rigging.md`); an error for a library rig, a warning otherwise |
| CV2 | skeletons | rest-pose classification ("Rest pose classification"): facing backwards and mirrored are errors, up axis an error, height and scaled joints warnings |
| CV3 | rig documents | mirror asymmetry and ambiguity warnings ("Mirror pair detection") |
| CV4 | rig documents | every joint and role name exists in the skeleton |
| CV5 | control sets | every target exists ("Control set document") |
| CV6 | pose books | every joint and curve exists ("Pose book name validation") |
| CV7 | assemblies | MV1 … MV13 (`Zenith2/Docs/Design/CHAR/Modular.md → "Assembly validation rules"`), HC1 … HC5 through MV9 |
| CV8 | scenes and prefabs | AU1 … AU9 and CB1, CB2 |
| CV9 | animation graphs of characters (graphs an assembly names) | the five template slots `Action`, `Zenith.GetUp`, `cine.body`, `char.lipsync`, `cine.face` exist (`CHAR_DIAG_GRAPH_SLOT_MISSING`, a warning: CINE and ANIM's get-up need them) |
| CV10 | lip-sync documents | the bank digest is current (warning when stale); every audio and visemes path exists (error); override sums (warning) |
| CV11 | lip-sync documents | every line's clip duration within ANIM's limit (error) |
| CV12 | lip-sync documents | `locales` equals the project's `voiceLocales` (error) |
| CV13 | the game | exactly one project document, valid (error), and each class's `libraryRig`, when present, is a skeleton product whose source carries `char-library-rig` (error) |
| CV14 | skinned meshes and skeletons of a character source: a source whose `.meta` carries the label `character` (written by the import wizard, step 3 of "Character import wizard" of `Zenith2/Docs/Design/CHAR/Workflow.md`); a library rig labelled `char-library-rig` without `character` is not a character source | the `.meta` also carries a `character-*` class label (warning: the content rules then use the `*` class) |
| CV15 | library rigs | "Shared library rig rule" (error) |
| CV16 | retarget documents | the rest-to-rest check of "Retarget setup" (warning per joint) |
| CV17 | the project document | every class's `label` is the selector of a `CHARACTER_CLASS_*` row of the content-rule table (`xQuery.GetContentRuleSelectors`); warning `CHAR_DIAG_PROJECT_CLASS_NO_CONTENT_RULE` naming the class ("Content rule measures") |
| CV18 | lip-sync documents | in the bank, every line's first locale entry is the project's `voiceLocales[0]` (`xQuery.GetBankLineLocales`); warning `CHAR_DIAG_LIPSYNC_BANK_DEFAULT_LOCALE` naming the line ("Sync from bank" of `Zenith2/Docs/Design/CHAR/Facial.md`) |

A document type CHAR does not know is skipped. The function is O(total document size). `Test_CharValidation_RunsEveryRuleOnce` asserts, with a fixture project breaking one rule per document, exactly one diagnostic per rule CV1 … CV18; `Test_CharValidation_SortedDeterministically` asserts the order.

## Validation panel

Panel type `Zenith.Editor.Character.Validation` (single, menu `Window/Character/Validation`): the catalogue's diagnostics for the open game, refreshed after every save of a document of an applicable type and on demand, grouped by document, each row selectable (it opens the document's panel at the offending member through the document handler, `Zenith2/Docs/Design/EDIT/ToolDeclarations.md → "Extension registration types"`). The command `character.validate [paths]` runs the same catalogue for automation and publishes the event `characterValidated` with the counts per severity ("Editor events" of `Zenith2/Docs/Design/CHAR/Workflow.md`).
