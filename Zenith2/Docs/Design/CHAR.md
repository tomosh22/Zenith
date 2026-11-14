# CHAR design: character production

The design of the Character production area (CHAR): the tools and data conventions with which a character is made, from a DCC export to a playable, animated, lip-synced, modular character in a game. CHAR owns rig and control authoring on top of ANIM's rig document, pose debugging, the production side of animation update budgeting, modular characters, the authoring rules that keep animation and physics authority consistent for characters, facial animation and lip-sync workflows, the D-SEL-MOTIONMATCH ruling, the classification of hair alternatives, and the complete character workflow (import → rig and retarget → author the animation graph → preview → in game) with its timed usability walkthrough. CHAR has no runtime code: every behaviour a character shows at game time is ANIM's, PHYS's, GEOM's, AUDIO's or CINE's, and CHAR's code is tool-side, in the tool library `ZenithAuthoring`, in ZenithCook and in ZenithEditor. This entry document holds the skeleton sections, the public API and the top-level contracts; the detail is in the seven section files of "Document family".

## Purpose and scope

### What CHAR owns

| Capability | Where |
|---|---|
| rig profiles, the profile matcher, mirror and chain detection, rest-pose classification, rig document generation, the facial rig template, the default ragdoll document | `Zenith2/Docs/Design/CHAR/Rigging.md` |
| retarget setup against shared library rigs, and the shared library rig rule | `Zenith2/Docs/Design/CHAR/Rigging.md → "Retarget setup"`, `"Shared library rig rule"` |
| control sets (posing controls) and pose books (expressions, visemes, hand poses) cooked into ANIM pose-library clips | `Zenith2/Docs/Design/CHAR/Rigging.md → "Control set document"`, `"Pose book cook"` |
| the viseme set, the face part of the graph templates, automatic viseme pose books, the character project settings document | `Zenith2/Docs/Design/CHAR/Facial.md` |
| lip-sync: the lip-sync document, sync from AUDIO's banks, the deterministic viseme estimator, external viseme import, manual overrides, the locale-strided lip-sync clip and its importer, the playback contract | `Zenith2/Docs/Design/CHAR/Facial.md` |
| modular characters: the assembly document, body regions, slots, loadouts, prefab generation with derived IDs, hand-edit protection, rigid attachments | `Zenith2/Docs/Design/CHAR/Modular.md` |
| the classification of hair alternatives and the hair-card authoring rules | `Zenith2/Docs/Design/CHAR/Modular.md → "Hair alternatives classification"` |
| the character authority matrix, the authority lint and the character integration suite | `Zenith2/Docs/Design/CHAR/Authority.md` |
| pose debugging tools, the production side of update budgeting (class presets, the budget panel, the class lint, the content-rule measures of character classes), the validation catalogue | `Zenith2/Docs/Design/CHAR/Debugging.md` |
| the complete character workflow: the editor extension, its commands, automation steps and events, the import wizard, graph templates, preview scenarios, the in-game step, the motion-matching authoring-cost harness, the timed walkthrough and its rehearsal | `Zenith2/Docs/Design/CHAR/Workflow.md` |
| the D-SEL-MOTIONMATCH ruling (the decision item Z2-I-CHAR-09) | "Classification and selection decisions CHAR owns" |

### Explicit exclusions

| Not CHAR's | Owner | Basis |
|---|---|---|
| skeletons, clips, graphs, their cooked formats and cook steps, the rig, mask, clip, graph and retarget documents, graph evaluation, IK, retargeting at run time, pose libraries and followers at run time, motion matching's runtime (`Zenith_MotionMatcher`, node kind 15) and its runtime harness, the update-budgeting mechanism | ANIM | `Zenith2/Docs/Design/ANIM.md → "What ANIM owns"`; [Z2-N-BUDGET-438] |
| ragdoll bodies, the ragdoll document and its cook, the character controller and its phase-4 move | PHYS | `Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Scope of this file"`; the orchestrator's Q72 and PHYS-RECOVER rulings |
| skinned meshes, morph targets, skinning, materials (the `alphaTest` domain, `twoSided`, anisotropy) | GEOM | `Zenith2/Docs/Design/GEOM/Skinning.md → "Scope of this file"`; `Zenith2/Docs/Design/GEOM/Materials.md → "Material document schema"` |
| glTF and FBX parsing, axis, handedness and unit conversion, the character import preset, the bounds and facing report, audio decoding | CONTENT | `Zenith2/Docs/Design/CONTENT.md → "What CONTENT owns"`; FBX is D-SEL-FBX (open, Stage 4) |
| dialogue lines, banks, dialogue playback, time-locked voices, the audio workflow and its walkthrough | AUDIO | `Zenith2/Docs/Design/AUDIO/Mixer.md → "Dialogue lines"`; `Zenith2/Docs/Design/AUDIO.md` lists lip-sync as CHAR's and exposes line markers and the voice clock only |
| cinematic sequences, dialogue sections, face clips in sequences, the cinematic writer | CINE | `Zenith2/Docs/Design/CINE/Playback.md → "Dialogue tracks"` |
| the editor shell, panels framework, ANIM's animation panels, the preview host, the command system, automation scripts, EDIT's four walkthroughs | EDIT | `Zenith2/Docs/Design/EDIT/Panels.md → "Extension interface"`; `Zenith2/Docs/Design/EDIT/AuthoringTools.md → "Animation tools"` |
| prefab documents, their resolution and derived IDs | SCENE | `Zenith2/Docs/Design/SCENE/Prefabs.md → "Authoring identities"` |
| equipment switching, the voice-language setting as game state, the flagship slices' dialogue and equipment samples | the game (GAME's samples) | Z2-D-077; cross-area row CH4 |
| hair and groom, strand hair, hair shells | excluded | Z2-D-043, Z2-D-078; "Hair alternatives classification" |
| cloth on characters | the D-CLASS-SIM (open, Stage 2) ruling's owner | `Zenith2/Docs/DecisionLog.md → "D-CLASS-SIM"` names CHAR a consumer for cloth |

### Inventory items covered

All sixteen CHAR items of `Zenith2/Docs/Requirements/Inventory.md → "Inventory v1"` (section "CHAR Character production"):

| Item | Capability | Stage | Specified in |
|---|---|---|---|
| `Z2-I-CHAR-01` | rig authoring | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` |
| `Z2-I-CHAR-02` | control authoring | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md → "Control set document"` … `"Pose capture and mirroring"`; `Zenith2/Docs/Design/CHAR/Workflow.md → "Posing panel"` |
| `Z2-I-CHAR-03` | pose debugging | S4 | `Zenith2/Docs/Design/CHAR/Debugging.md → "Pose debugger panel"` … `"Pose history recorder"` |
| `Z2-I-CHAR-04` | animation update budgeting | S4 | the production side here (`Zenith2/Docs/Design/CHAR/Debugging.md → "Character class presets"` … `"Content rule measures"`); the runtime is ANIM's ("Split with ANIM") |
| `Z2-I-CHAR-05` | modular characters | S4 | `Zenith2/Docs/Design/CHAR/Modular.md` |
| `Z2-I-CHAR-06` | animation/physics authority | S3 | `Zenith2/Docs/Design/CHAR/Authority.md` |
| `Z2-I-CHAR-07` | facial animation workflow | S4 | `Zenith2/Docs/Design/CHAR/Facial.md → "Viseme set"` … `"Character project settings document"`; pose books |
| `Z2-I-CHAR-08` | lip-sync workflow | S4 | `Zenith2/Docs/Design/CHAR/Facial.md → "Lip-sync document"` … `"Voice locale as a simulation input"` |
| `Z2-I-CHAR-09` | decision on motion matching (D-SEL-MOTIONMATCH) | S3 | "Classification and selection decisions CHAR owns"; `Zenith2/Docs/Design/CHAR/Workflow.md → "Motion-matching authoring-cost harness"` |
| `Z2-I-CHAR-10` | decision on hair alternatives | S4 | `Zenith2/Docs/Design/CHAR/Modular.md → "Hair alternatives classification"`, `"Hair card authoring rules"` |
| `Z2-I-CHAR-11` … `-15` | the complete character workflow: import, rig/retarget, author the graph, preview, in game | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` |
| `Z2-I-CHAR-16` | usability acceptance as a timed scripted walkthrough | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md → "Walkthrough character workflow"` … `"Walkthrough fixtures"` |

### Split with ANIM

The plan gives CHAR, in Appendix A, "rig and control authoring, pose debugging, animation update budgeting, modular characters, animation/physics authority, facial animation and lip-sync workflows; decisions on motion matching (D-SEL-MOTIONMATCH) and hair alternatives (hair excluded; cards/shells classified); a complete character workflow (import → rig/retarget → author the animation graph → preview → in-game) with usability acceptance as a timed scripted walkthrough", and gives ANIM "skeleton import, compression/streaming, blend trees/state machines, additive layers, masks, synchronization, events, root motion, retargeting, IK, motion warping, facial animation, ragdoll transitions; visual authoring". Budgets.md adds: "Animation budgeting (significance-based update-rate reduction) … [is an] implementation ticket in ANIM" ([Z2-N-BUDGET-438]). The split, item by item:

| Topic | ANIM (runtime and data) | CHAR (production and decisions) |
|---|---|---|
| rigs | the skeleton, the rig document, `anim.skeleton` (`Zenith2/Docs/Design/ANIM/Formats.md → "Rig document schema"`) | profiles, detection, generation of the rig document's content, the facial template, library-rig rules |
| controls | parameters, curves, IK goals, `POSE_BLEND` | control sets, pose books and their cook, the posing panel |
| pose debugging | `GetDebugState`, `anim.dump`, the skeleton overlay of development builds | the pose debugger panel, snapshots, diffs, history |
| update budgeting | significance, rate tiers, the evaluation cap, the `animation.budget.*` keys and their rows (`Zenith2/Docs/Design/ANIM/Runtime.md → "Update-rate budgeting"`) | class presets, the budget panel, the class lint, the character-class content rules |
| modular characters | pose followers (`Zenith2/Docs/Design/ANIM/Runtime.md → "Pose followers"`) | assemblies, regions, loadouts, prefab generation |
| facial animation and lip-sync | curves, morph remap, pose libraries, slots (`Zenith2/Docs/Design/ANIM/Runtime.md → "Facial animation"`); "CHAR's lip-sync workflow produces the viseme curves; ANIM plays and blends them" | the viseme set, the estimator, lip-sync documents and clips, the playback contract |
| motion matching | the motion matcher, its runtime harness `Z2-R-ANIM-178`, node kind 15 `Z2-R-ANIM-179` (blocked by the ruling) | the ruling (Z2-I-CHAR-09), the authoring-cost harness, the motion-matching template |
| retargeting | `Zenith_RetargetMap`, retargeted clip cooking | creating retarget documents from library rigs, the rest-to-rest check |
| authority | ANIM's phase-3, phase-4 and phase-6 systems | the matrix as a consequence, the lint, the end-to-end suite |

ANIM's design records the same split (`Zenith2/Docs/Design/ANIM.md → "Explicit exclusions"`, row "rig and control authoring workflows … the D-SEL-MOTIONMATCH ruling | CHAR", and "Classification decisions ANIM owns": "the runtime of `Z2-I-CHAR-04` (animation update budgeting) is ANIM's … while the item stays CHAR's").

### Document family

| File | Holds | `##` headings |
|---|---|---|
| `Zenith2/Docs/Design/CHAR.md` (this file) | the skeleton sections, the public API, the reconciliation items | the A3 skeleton and "Reconciliation items" |
| `Zenith2/Docs/Design/CHAR/Rigging.md` | rigs, retargeting, controls, pose books | Scope of this file · Rig profile table · Joint name normalisation · Profile matching algorithm · Generic profile assignment · Topology verification · Mirror pair detection · Chain and IK template detection · Rest pose classification · Rig document generation · Facial rig template · Default ragdoll generation · Shared library rig rule · Retarget setup · Control set document · Control evaluation · Pose book document · Pose book cook · Pose capture and mirroring · Rigging limits |
| `Zenith2/Docs/Design/CHAR/Facial.md` | faces and lip-sync | Scope of this file · Viseme set · Face graph template · Automatic viseme pose books · Character project settings document · Lip-sync document · Sync from bank · Estimator input · Estimator framing and spectrum · Estimator features · Estimator classification · Estimator smoothing and normalisation · Phrase markers · External viseme import · Manual overrides · Locale-strided lip-sync clip · Window markers and events · Clip intermediate writer · Lip-sync importer · Lip-sync playback contract · Voice locale as a simulation input · Facial and lip-sync limits |
| `Zenith2/Docs/Design/CHAR/Modular.md` | modular characters and hair | Scope of this file · Assembly model · Assembly document · Body regions · Slots and loadouts · Generated prefab layout · Generated entity identity · Loadout variants · Prefab generation algorithm · Hand-edit protection · Keep as variant algorithm · Assembly validation rules · Rigid attachments · Part caps and cost · Runtime part switching · Hair alternatives classification · Hair card authoring rules |
| `Zenith2/Docs/Design/CHAR/Authority.md` | authority for characters | Scope of this file · Fields in the matrix · Character authority matrix · State transitions and their tick · Authority lint inputs · Authority lint rules · Character integration suite · Integration fixtures |
| `Zenith2/Docs/Design/CHAR/Debugging.md` | pose debugging, budgeting workflow, validation | Scope of this file · Pose debugger panel · Pose sources · Joint table · Skeleton overlays · Pose snapshot format · Pose diff algorithm · Pose history recorder · Character class presets · Budget panel · Class rate lint · Content rule measures · Validation catalogue · Validation panel |
| `Zenith2/Docs/Design/CHAR/Workflow.md` | the workflow and its tools | Scope of this file · Workflow overview · Editor extension · Character commands · Command argument forms · Automation steps · Editor events · Character preferences · Character import wizard · Workflow panel · Rig profile panel · Retarget step · Graph template catalogue · Template clip binding · Template instantiation · Preview scenarios · Scenario runner · In-game step · Assembly panel · Posing panel · Lip-sync panel · Motion-matching authoring-cost harness · Walkthrough character workflow · Walkthrough rehearsal · Walkthrough fixtures |
| `Zenith2/Docs/Design/CHAR/Tests.md` | every named test | Scope of this file · Test targets · Rigging tests · Facial and lip-sync tests · Modular and hair tests · Authority tests · Debugging budgeting and validation tests · Editor and workflow tests · Document and registration tests · Integration tests · Fuzz targets · Benchmarks · Fixtures and generators |
| `Zenith2/Docs/Design/CHAR/Types.md` | the member types and coded-field enumerations of the root header, declared in full | Scope of this file · Control and pose book member types · Clip member types · Assembly member types · Template and scenario types · Validation member types · Pose debugging member types · Enumerations for coded fields |
| `Zenith2/Docs/Requirements/CHAR.md` | the requirement catalogue `Z2-R-CHAR-001` … `Z2-R-CHAR-146` | Purpose · Requirements · Coverage of inventory items · Excluded and deferred |

### Conventions

- Names are Zenith Hungarian (Z2-D-034); CHAR's types are prefixed `Zenith_Char` and its tests `Test_Char…`; CHAR's ZenithAuthoring subdirectory is `Internal/CHAR/` and its root header `Zenith_AuthoringCharacter.h` (Q84 (a), `Zenith2/Docs/Design/EDIT.md → "Tool-side code the editor needs"`).
- `<!-- elaboration -->` marks a detail this design adds; `<!-- assumption: pending <AREA> -->` blocks state an interface another area has not yet provided, each with its cross-area row (`C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`, rows `CH1` … `CH22`).
- Units are CORE's (metres, seconds, radians, unit quaternions; right-handed, +Y up, model front +Z, model left +X).
- No CHAR source calls a CRT transcendental: sines, cosines, exponentials, logarithms and arctangents are CORE's deterministic functions, and no CHAR source includes `<cmath>` or `<math.h>`: square roots are CORE's `Zenith_Sqrt` (the X314-FMA3 ruling; `Zenith2/Docs/Design/CHAR/Facial.md → "Estimator input"`).
- Obligations here are traced to `Z2-R-CHAR-NNN` ("Requirement index").

## Dependencies

### Areas CHAR depends on

| Area | What CHAR uses |
|---|---|
| ANIM | the skeleton asset and its published properties, the rig, mask, graph, clip annotation and retarget documents, ANIM's curve-and-pose clip intermediate writer (row CH15), `Zenith_CompileAnimationGraph`, `Zenith_AnimationPreview`, `Zenith_RetargetMap`, the pose operations and IK solvers, `GetDebugState`, `GetStats`, the rate function `Zenith_AnimComputeRateDivisor` (row CH15), the motion-matching database builder and matcher, the measures of the Skeleton and AnimationGraph content-rule quantities (row CH15) |
| PHYS | the ragdoll document schema, the ragdoll and controller component property IDs, the ragdoll schedule CHAR's suite asserts |
| GEOM | skinned-mesh skeleton IDs and morph-target name hashes, the material document's domain and flags, the Mesh content-rule measures (row CH17), and the rigid-attach cook setting (row CH6) |
| SCENE | prefab documents, the override block, `Zenith_DeriveAuthoringID`, the resolver, canonical JSON, the migration registry, the journaled write |
| ASSET | importers, the import context, cook steps, content rules, the cook test access |
| CONTENT | the glTF parser behind the import report, the character preset, `Zenith_DecodeAudioSource`, crash isolation |
| AUDIO | bank documents and dialogue lines, marker flags, time-locked dialogue, the audition API |
| CINE | dialogue sections and the `cine.face`/`cine.body` slots (rows CN14, CH3) |
| EDIT | the extension interface, commands, automation, the preview host, the widgets, the play controller, the fixture game |
| ECS, MOD, CORE | the JSON field-set form and world hashes, the runtime test access for the suite, containers, hashing, SHA-256, the deterministic transcendental functions of `Zenith_Transcendental.h` (`Zenith2/Docs/Design/CORE/Maths.md → "Transcendental declarations"`; row CH21, answered) |
| GAME | the voice-locale command and the game's settings script (row CH18) |

### Headings this area relies on

| Heading | Used for |
|---|---|
| `Zenith2/Docs/Design/ANIM/Formats.md → "Rig sections"`, `"Rig document schema"`, `"Clip intermediate"`, `"Clip annotation document schema"`, `"Retarget document schema"`, `"Mask document schema"`, `"Graph document schema"`, `"Skeleton info section"`, `"Graph info section"`, `"Skeleton validation"` | everything CHAR writes for ANIM to cook |
| `Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationWorld header"`, `"Zenith_AnimationPreview header"`, `"Zenith_AnimationRetarget header"`, `"Zenith_AnimationMotionMatch header"`, `"Facial animation"`, `"Update-rate budgeting"`, `"Ragdoll transitions"`, `"Animation.ApplyRootMotion"` | the runtime CHAR's tools read and its suite proves |
| `Zenith2/Docs/Design/ANIM/Graph.md → "Node catalogue"`, `"Slots"`, `"Motion matching node"` | the templates |
| `Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md → "Character move algorithm"`, `"Ragdoll state machine"`, `"Ragdoll blend and root"`; `Zenith2/Docs/Design/PHYS/Formats.md → "Ragdoll document"`; `Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Zenith_Ragdoll header"`, `"Zenith_CharacterController header"` | authority and the default ragdoll |
| `Zenith2/Docs/Architecture/FramePhases.md → "The twelve phases"`, `"Arbitration rules"`; `Zenith2/Docs/Architecture/LifecycleCommands.md → "Ragdoll enter and recover"`, `"Cinematic start, interrupt and skip"`, `"Teleport"` | the authority matrix |
| `Zenith2/Docs/Design/SCENE/Format.md → "Prefab document schema"`, `"Override block"`; `Zenith2/Docs/Design/SCENE/Prefabs.md → "Derivation"`, `"Resolution algorithm"`; `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_MigrationRegistry header"`, `"Canonical rules"`, `"Journaled write"` | generated prefabs and CHAR's documents |
| `Zenith2/Docs/Design/ASSET/Import.md → "Zenith_ImportContext"`; `Zenith2/Docs/Design/ASSET/Cook.md → "Content rules"`, `"Zenith_CookStepRegistry"`, `"Step registration functions"` | importers and measures |
| `Zenith2/Docs/Design/CONTENT/Importers.md → "Zenith_MediaDecoders header"`, `"Import report member"`, `"Generated model prefab"`; `Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Shipped presets"`; `Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Bounds and facing report"` | import and audio decoding |
| `Zenith2/Docs/Design/AUDIO/Mixer.md → "Dialogue lines"`, `"Time-locked voices"`, `"Locale selection"`; `Zenith2/Docs/Design/AUDIO/Formats.md → "Markers and loop points section"`, `"Bank document schema"` | lip-sync |
| `Zenith2/Docs/Design/EDIT/Panels.md → "Extension interface"`, `"Preview host"`; `Zenith2/Docs/Design/EDIT/Automation.md → "Automation script format"`, `"Walkthrough rules"`; `Zenith2/Docs/Design/EDIT/ToolDeclarations.md → "Extension registration types"`; `Zenith2/Docs/Design/EDIT/Tests.md → "Fixtures"` | the editor tools and the rehearsal |
| `Zenith2/Docs/Budgets.md → "Usability walkthrough targets"`, `"ContentSpecs schema"`, `"CinematicSlice acceptance"`, `"W-Characters"` | the walkthrough row, character classes, sign-off items |
| `Zenith2/Docs/Design/CORE/Maths.md → "Numerical conventions"`, `"Coordinate systems"`; `Zenith2/Docs/Architecture/BuildSystem.md → "MSVC flags for the win and win-asan presets"` | `dPI`, the Euler convention of ragdoll offsets, the no-contraction flags |
| `Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_PrefabResolver header"`; `Zenith2/Docs/Design/SCENE/Format.md → "Override block"`, `"Prefab-instance object"` | Keep as variant, scene placement |

### Interfaces CHAR declares

| Interface | Declared in | Implemented by | Purpose |
|---|---|---|---|
| `Zenith_CharAssetQuery` | `Zenith_AuthoringCharacter.h` ("Asset query interface") | the editor over its session's loaded assets (`Zenith2/Tools/ZenithEditor/Internal/Character/Zenith_CharEditorAssetQuery.cpp`); the tests over fixtures (`Zenith_CharTestFixtures.h`) | facts about other areas' assets the validators need, without a dependency of `ZenithAuthoring` on the editor |

CHAR implements EDIT's `Zenith_EditorExtension`, ASSET's `Zenith_Importer` three times and registers content-rule measures; it stores no pointer into a higher library.

### Assumed interfaces from pending design documents

<!-- assumption: pending ANIM, PHYS, GEOM, EDIT, ASSET, AUDIO, CINE, SCENE, GAME -->
| Need | Owner | CHAR's assumption | Row |
|---|---|---|---|
| face roles 55–62; a `mask` member on graph slot entries; `POSE_BLEND` applying curve deltas of its poses; a non-CONTENT importer writing `anim.clip` intermediates and annotation settings for a skeleton of another source | ANIM | "Facial rig template", "Face graph template", "Pose book cook" | CH1 |
| the curve-and-pose intermediate writer `Zenith_WriteCurveClipIntermediate` in `Zenith_AuthoringAnimation.h`; the pure `Zenith_AnimComputeRateDivisor`; ANIM's own measures for (Skeleton, `bones`), (Skeleton, `ikChains`), (AnimationGraph, `layers`); `BuildFromFrames` and `Zenith_MotionMatchSourceClip` landing with the Stage 3 harness ticket | ANIM | "Clip intermediate writer", "Budget panel", "Content rule measures", "Motion-matching authoring-cost harness" | CH15 |
| the Luau `Animation.GetMarkerTime(clip, name)`; slot tag bit 14 `ANIM_SLOT_TAG_FULL_RATE` forcing rate divisor 1 while active, and a `tag` option of the Luau `PlaySlot` | ANIM | "Lip-sync playback contract" | CH16 |
| dialogue line IDs as `Zenith_Hash64` of the line text; the lip-sync phrase flag of markers; `Audio.PlayDialogue` honouring the time-locked flag for gameplay lines; the audition API callable from CHAR's panel | AUDIO | "Sync from bank", "Lip-sync playback contract" | CH2 |
| dialogue face clips resolve their window from the voice-locale tag through the clip's markers and stop at the first window-end event of the play; the tag passed at cinematic start; the CinematicSlice walkthrough content produced by a `needs-human` content ticket. Until CH19 lands, CINE plays a face clip from time 0 for the section's duration, so cinematic faces show window 0 whatever the voice locale and a section longer than S reads into window 1; CINE lip-sync sections must not exceed S and CHAR's CinematicSlice lip-sync sign-off item stays blocked by CH19 | CINE | "Lip-sync playback contract", "Walkthrough character workflow" | CH3, CH19 |
| the voice-locale tag as simulation state, changed only by a recorded command at a tick boundary, saved, and re-issued after a restore whose user setting differs; equipment switching by visibility; the slices' dialogue samples playing lip-sync clips | GAME | "Voice locale as a simulation input", "Runtime part switching" | CH4, CH18 |
| the play-world resource accessor of the live animation debugger; an overlay contributor hook; the animated-character preview of a prefab with followers; the walkthrough fixture dependency on the editor fixture games; CHAR's row of the ZenithAuthoring owners table; the `Audio` module in EditorFixture's `modules` | EDIT | "Pose sources", "Skeleton overlays", "Assembly panel", "Walkthrough fixtures", "ZenithAuthoring CHAR files" | CH5, CH14, CH22 |
| the `skin.rigidAttach` mesh setting; morph-target name hashes (row AN4); GEOM's measure of (Mesh, `blendShapes`) | GEOM | "Rigid attachments", "Automatic viseme pose books", "Content rule measures" | CH6, CH17 |
| the importer registration slot | ASSET | "Importers and cook registration" | CH7 |
| the ragdoll document members and component property 3; the Euler convention of `offset.rotationDegrees` | PHYS | "Default ragdoll generation", "Generated prefab layout" | CH8, CH20 |
| generated prefab documents written with SCENE's prefab canonical schema by a tool outside SCENE | SCENE | "Prefab generation algorithm" | CH11 |
<!-- /assumption -->

### Areas that consume CHAR

| Area | Uses |
|---|---|
| CINE | lip-sync clips as dialogue face clips; the template slots `cine.body`, `cine.face` |
| GAME (flagship samples) | generated character prefabs, part entity names, lip-sync clips and the playback contract |
| EDIT | CHAR's panels in its extension table; the character walkthrough beside its four |
| ANIM | the D-SEL-MOTIONMATCH ruling, which unblocks or excludes node kind 15 |
| BUDGET | the walkthrough row's step list; the character classes for ContentSpecs |

## Library and file layout

### Targets

| Target | Kind | CHAR's part | `ZENITH_SUPPORTED` |
|---|---|---|---|
| `ZenithAuthoring` | static tool library (Q84 (a)) | `Zenith_AuthoringCharacter.h`, `Internal/CHAR/` | `ZENITH_TOOLS_HOST` |
| `ZenithCook` | tool | `Internal/Character/` (importers, measures) | `ZENITH_TOOLS_HOST` |
| `ZenithEditor` | tool | `Internal/Character/` (the extension) | `ZENITH_TOOLS_HOST` |
| `Zenith_Integration_Character` | integration suite | `Zenith2/Tests/Character/` | narrowed by its dependencies |
| `Zenith_Bench_CharMotionMatchAuthoring` | benchmark | `Zenith2/Benchmarks/CharacterAuthoring/` | the presets of its dependencies |

No CHAR source is in an engine library, and no Player links a CHAR target ([Z2-N-ARCH-241]: tool targets are outside every Shipping link closure).

### ZenithAuthoring CHAR files

<!-- assumption: pending EDIT (row CH14) -->
```
Zenith2/Tools/Authoring/
	Zenith_AuthoringCharacter.h               CHAR's root header ("Public API")
	Internal/CHAR/
		Zenith_CharDocumentTypes.cpp          Zenith_RegisterCharacterDocumentTypes; the five canonical schemas
		Zenith_CharRigProfiles.h/.cpp         the profile tables (Rigging.md "Rig profile table")
		Zenith_CharRigNames.cpp               normalisation and tokens
		Zenith_CharRigDetect.cpp              matching, generic assignment, topology, mirror, chains, rest pose
		Zenith_CharRigDocument.cpp            rig document edits, the facial template
		Zenith_CharRagdoll.cpp                the default ragdoll document
		Zenith_CharRetarget.cpp               retarget documents and the rest-to-rest check
		Zenith_CharControls.cpp               control set reader and evaluation
		Zenith_CharPoseBook.cpp               pose book reader, curve clip building, capture and mirror
		Zenith_CharClipTimes.cpp              Zenith_CharRoundClipTimes and the conversion to ANIM's writer descriptor
		Zenith_CharFft.h/.cpp                 Zenith_CharFft512
		Zenith_CharLipSyncEstimator.h/.cpp    the estimator and its three internal stages (Facial.md "Estimator input")
		Zenith_CharVisemeImport.cpp           zenith2.visemes reader and conversion
		Zenith_CharLipSyncLayout.cpp          overrides, phrases, the strided layout, markers and events
		Zenith_CharLipSyncDocument.cpp        the lip-sync document reader
		Zenith_CharVisemePoseBooks.cpp        automatic viseme pose books
		Zenith_CharAssembly.cpp               assembly reader, visibility, validation
		Zenith_CharAssemblyPrefabs.cpp        prefab generation
		Zenith_CharGraphTemplates.h/.cpp      template texts, binding, instantiation
		Zenith_CharScenario.cpp               scenario reader and runner
		Zenith_CharAuthority.cpp              the authority lint
		Zenith_CharValidation.cpp             the validation catalogue
		Zenith_CharPoseSnapshot.cpp           snapshot reader, writer and diff
		Zenith_CharBudget.cpp                 rate prediction through ANIM's rate function
	Tests/CHAR/                               Zenith_CharRig_Tests.cpp, Zenith_CharLipSync_Tests.cpp, Zenith_CharAssembly_Tests.cpp,
	                                          Zenith_CharAuthority_Tests.cpp, Zenith_CharPose_Tests.cpp, Zenith_CharValidation_Tests.cpp,
	                                          Zenith_CharDocuments_Tests.cpp, Zenith_CharBudget_Tests.cpp, Zenith_CharRobustness_Tests.cpp
	Tests/Support/Zenith_CharTestFixtures.h   header-only generators (X184)
	Tests/Fixtures/Migration/CHAR/<Type>/     v1.json, v1.expected.json per document type
```
By the dependency growth rule of the AUTHORING-CONTRACT ruling (`Zenith2/Docs/Design/EDIT.md → "ZenithAuthoring contract rules"`, rule 4; core-doc item X301), each CHAR diff appends to `ZenithAuthoring`'s `DEPENDS` every library its files include that the list on `master` does not yet hold: the Stage 3 API slice `ZenithScene` and `ZenithDocument`; the first Stage 4 rig diff `ZenithAnimation`; the first estimator diff `ZenithContentCore` (the audio decoder). CHAR never states that an edge is already declared. Row CH14 asks EDIT to add CHAR's row to its owners table (`Zenith2/Docs/Design/EDIT.md → "ZenithAuthoring owners"`): owner CHAR, `<Dir>` `CHAR`, root header `Zenith_AuthoringCharacter.h`, edges ZenithScene, ZenithDocument, ZenithAnimation, ZenithContentCore, what the editor calls (every function of "Public API" and `Zenith_RegisterCharacterDocumentTypes`), tests `Tests/CHAR/` and fixtures `Tests/Fixtures/Migration/CHAR/<Type>/`.
<!-- /assumption -->

### ZenithCook CHAR files

```
Zenith2/Tools/ZenithCook/Internal/Character/
	Zenith_CharacterCook.h                    Zenith_RegisterCharacterImporters
	Zenith_CharLipSyncImporter.cpp            char.lipsync
	Zenith_CharPoseBookImporter.cpp           char.posebook
	Zenith_CharDocumentImporter.cpp           char.document (validation only)
Zenith2/Tools/ZenithCook/Tests/Character/    Zenith_CharCook_Tests.cpp, Zenith_CharPreview_Tests.cpp, Zenith_CharFixtures_Generate.cpp
```

### ZenithEditor CHAR files

```
Zenith2/Tools/ZenithEditor/Internal/Character/
	Zenith_CharacterExtension.cpp             the extension's Register
	Zenith_CharCommands.cpp                   the character.* commands
	Zenith_CharAutomationSteps.cpp            the seven automation steps and the events
	Zenith_CharEditorAssetQuery.cpp           Zenith_CharAssetQuery over the session's assets
	Zenith_CharWorkflowPanel.cpp, Zenith_CharImportWizard.cpp, Zenith_CharRigProfilePanel.cpp, Zenith_CharAssemblyPanel.cpp,
	Zenith_CharPosingPanel.cpp, Zenith_CharLipSyncPanel.cpp, Zenith_CharPoseDebuggerPanel.cpp, Zenith_CharValidationPanel.cpp,
	Zenith_CharBudgetPanel.cpp                the nine panels
	Zenith_CharPoseHistory.cpp                the recorder ring and its cache consumer
	Zenith_CharPlayDriver.cpp                 character.play.spawn and character.play.drive
Zenith2/Tools/ZenithEditor/Tests/Character/  Zenith_CharEditor_Tests.cpp, Zenith_CharPoseDebugger_Tests.cpp, Zenith_CharBudget_Tests.cpp,
                                             Zenith_CharWalkthrough_Tests.cpp
Zenith2/Tools/ZenithEditor/Tests/Fixtures/Scripts/Walkthroughs/CharacterWorkflow.editorscript.json
```

### Integration suite and benchmark files

```
Zenith2/Tests/Character/
	CMakeLists.txt                            zenith_add_integration_tests(Character DEPENDS ZenithRuntime ZenithAnimation ZenithPhysics
	                                          ZenithPhysics_Jolt ZenithPhysics_PhysX ZenithScript) (+ ZenithCinematics and ZenithAudio from Stage 4)
	Zenith_CharIntegration_Tests.cpp          Authority.md "Character integration suite"
	Zenith_CharLipSync_Tests.cpp              the lip-sync playback and locale cases
	Zenith_CharModular_Tests.cpp              the visibility switch
	Scripts/LipSyncPlayer.luau                the Luau test behaviour of the playback contract case
	Scripts/VoiceLocaleSettings.luau          the suite's implementation of the voice-locale rules (Facial.md "Voice locale as a simulation input")
Zenith2/Benchmarks/CharacterAuthoring/       Zenith_Bench_CharMotionMatchAuthoring.cpp, Zenith_BenchCharMmSource.h, Zenith_BenchCharMmEvaluator.h/.cpp,
                                             Zenith_BenchCharTemplateTexts.h, CMakeLists.txt (zenith_add_benchmark; DEPENDS ZenithCore ZenithAnimation)
```

### Committed content files

CHAR commits no engine content: rig profiles, templates and built-in scenarios are code. Games commit their CHAR documents (`*.charproject.json`, `*.character.json`, `*.lipsync.json`, `*.posebook.json`, `*.charcontrols.json`) and CHAR's other source files (`*.visemes.json`, `*.posebooknames.json`, `*.charscenario.json`, and `*.posesnapshot.json` when kept under `Assets/`) with `.meta` sidecars like every source: `char.document` claims those four suffixes as zero-product sources it validates ("Document validation importer"), so none is ignored with `COOK_WARNING_UNRECOGNISED_FILE` (`Zenith2/Docs/Design/ASSET/Database.md → "What is an asset source"`). CinematicSlice's walkthrough starting content (`Zenith2/Docs/Design/CHAR/Workflow.md → "Walkthrough character workflow"`) is committed by CINE's `needs-human` content-production ticket (row CH19), never by a worker (Q55).

### ZENITH_SUPPORTED matrix

Every CHAR tool target is `ZENITH_TOOLS_HOST` (the `win` preset, `Zenith2/Docs/Architecture/BuildSystem.md → "The ZENITH_SUPPORTED matrix"`), so none is in `win-asan` and CHAR has no fuzz target (its readers are covered by the mutated-corpus cases of `Zenith2/Docs/Design/CHAR/Tests.md → "Fuzz targets"`); the integration suite and the benchmark take their dependencies' matrix; nothing CHAR builds runs on Android, and nothing is in `linux-tsan`.

## Public API

### API rules

- Every fallible public function returns `Zenith_Result<…, Zenith_CharError>`; every public type carries one contract block ([Z2-N-CONTRACT-030], [Z2-N-CONTRACT-111]).
- Functions are pure over their inputs unless their contract says otherwise: they read no file, no clock and no global; the editor, the cook and the tests supply inputs, so every result is reproducible.
- Untrusted content (documents, audio, visemes files, snapshots, scenarios) never reaches an assert: it fails with `Zenith_CharError` or a diagnostic ([Z2-N-CONTRACT-472]).
- No public header includes a third-party header or an STL type (tool code may use the STL inside `Internal/` only).

### Header map

| Header | Declares | Section |
|---|---|---|
| `Zenith2/Tools/Authoring/Zenith_AuthoringCharacter.h` | constants, enumerations, errors, diagnostics, every CHAR value type and function, `Zenith_CharAssetQuery` | "Constants and enumerations" … "Document registration API", and the member types of `Zenith2/Docs/Design/CHAR/Types.md` |
| `Zenith2/Tools/ZenithCook/Internal/Character/Zenith_CharacterCook.h` | the importer registration function | "Importers and cook registration" |
| `Zenith2/Tools/ZenithEditor/Internal/Character/Zenith_CharacterExtension.h` | the editor extension object | "Editor extension header" |

### Constants and enumerations

```cpp
// Zenith2/Tools/Authoring/Zenith_AuthoringCharacter.h
#pragma once
// Stage 3 (the API slice of the authority types):
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Array.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_JsonDocument.h"
#include "ZenithScene/Zenith_Transform.h"
#include "ZenithDocument/Zenith_MigrationRegistry.h"
// Added by the Stage 4 slice, each in the diff that first needs it:
#include "ZenithCore/Zenith_Heap.h"
#include "ZenithCore/Zenith_Arena.h"
#include "ZenithCore/Zenith_Matrix.h"
#include "ZenithScene/Zenith_SceneData.h"               // Zenith_SceneData (Keep as variant)
#include "ZenithAnimation/Zenith_AnimationTypes.h"
#include "ZenithAnimation/Zenith_AnimationPreview.h"
#include "ContentCore/Zenith_MediaDecoders.h"          // Zenith_DecodedAudio (CONTENT)

inline constexpr uint32 uCHAR_VISEME_COUNT = 15;
inline constexpr uint32 uCHAR_VISEME_SET_CURVES = 16;        // the fifteen visemes and char.lipsync.energy
inline constexpr uint32 uCHAR_MAX_VOICE_LOCALES = 16;
inline constexpr uint32 uCHAR_MAX_LIPSYNC_LINES = 4096;
inline constexpr uint32 uCHAR_MAX_ASSEMBLY_PARTS = 64;
inline constexpr uint32 uCHAR_MAX_VISIBLE_PARTS = 16;
inline constexpr uint32 uCHAR_MAX_LOADOUTS = 32;
inline constexpr uint32 uCHAR_MAX_CONTROLS = 256;
inline constexpr uint32 uCHAR_MAX_POSES = 255;
inline constexpr uint32 uCHAR_MAX_RECORDED_CHARACTERS = 4;
inline constexpr uint32 uCHAR_RIG_PROFILE_COUNT = 5;
inline constexpr uint32 uCHAR_ROLE_SLOTS = 63;                // Zenith_JointRole 0 … 62 (face roles 55 … 62, row CH1)
inline constexpr uint32 uCHAR_NO_PARENT = 0xFFFFFFFFu;
inline constexpr uint32 uCHAR_LIPSYNC_ESTIMATOR_VERSION = 1;  // joins the char.lipsync importer version

/// @contract enum Zenith_CharRigProfileID
/// Ownership: Plain enumeration of Rigging.md "Rig profile table".
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Tool-side; values never move.
/// @endcontract
enum Zenith_CharRigProfileID : uint8
{
	CHAR_RIG_PROFILE_NONE = 0, CHAR_RIG_PROFILE_ZENITH_HUMANOID, CHAR_RIG_PROFILE_MIXAMO, CHAR_RIG_PROFILE_MANNEQUIN_STYLE,
	CHAR_RIG_PROFILE_RIGIFY_DEFORM, CHAR_RIG_PROFILE_GENERIC,
};

/// @contract enum Zenith_CharVisemeID
/// Ownership: Plain enumeration of Facial.md "Viseme set".
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Values are curve indices of every lip-sync clip; they never move.
/// @endcontract
enum Zenith_CharVisemeID : uint8
{
	CHAR_VISEME_SIL = 0, CHAR_VISEME_PP, CHAR_VISEME_FF, CHAR_VISEME_TH, CHAR_VISEME_DD, CHAR_VISEME_KK, CHAR_VISEME_CH,
	CHAR_VISEME_SS, CHAR_VISEME_NN, CHAR_VISEME_RR, CHAR_VISEME_AA, CHAR_VISEME_E, CHAR_VISEME_IH, CHAR_VISEME_OH, CHAR_VISEME_OU,
};
Zenith_StringView Zenith_CharGetVisemeCurveName(uint32 uCurve);   // 0 … 15; 15 is "char.lipsync.energy"

/// @contract enum Zenith_CharRestPoseClass
/// Ownership: Plain enumeration of Rigging.md "Rest pose classification".
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_CharRestPoseClass : uint8 { CHAR_REST_POSE_T = 1, CHAR_REST_POSE_A, CHAR_REST_POSE_OTHER };

/// @contract enum Zenith_CharFacing
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_CharFacing : uint8 { CHAR_FACING_OK = 1, CHAR_FACING_BACKWARDS, CHAR_FACING_MIRRORED, CHAR_FACING_UNKNOWN };
```

### Errors and diagnostics

```cpp
/// @contract enum Zenith_CharError
/// Ownership: Plain enumeration (the enumeration form of [Z2-N-CONTRACT-480]).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Each value's meaning is its comment.
/// Version: since Z2 Stage 3. Values keep their meaning ([Z2-N-CONTRACT-467]).
/// @endcontract
enum Zenith_CharError : uint8
{
	CHAR_ERROR_OUT_OF_MEMORY = 1,     // an allocation failed
	CHAR_ERROR_INVALID_DOCUMENT,      // a document violates its schema (the diagnostics name the key path)
	CHAR_ERROR_INVALID_ARGUMENT,      // a caller-supplied value is out of range (a span size, an index)
	CHAR_ERROR_SKELETON_MISMATCH,     // assets of different skeletons where one is required
	CHAR_ERROR_PROFILE_INCOMPLETE,    // a humanoid profile lacks a required role
	CHAR_ERROR_LIMIT_EXCEEDED,        // a documented limit was exceeded
	CHAR_ERROR_AUDIO_DECODE,          // CONTENT's decoder refused an audio source
	CHAR_ERROR_LOCALE_UNKNOWN,        // a locale tag is not in the project's voice locales
	CHAR_ERROR_PREFAB_HAND_EDITED,    // a generated prefab differs from its recorded digest
	CHAR_ERROR_TEMPLATE_UNKNOWN,      // no graph template of that name
	CHAR_ERROR_BINDING_MISSING,       // a required template role has no clip
	CHAR_ERROR_SNAPSHOT_INVALID,      // a pose snapshot is malformed or of an unknown version
	CHAR_ERROR_NOT_AVAILABLE,         // a facility the call needs is absent (no renderer for a preview, no play world)
	CHAR_ERROR_EDIT_UNREPRESENTABLE,  // Keep as variant met an edit no override block expresses (Modular.md "Keep as variant algorithm")
};
constexpr const char* Zenith_ToString(Zenith_CharError eError);

/// @contract enum Zenith_CharSeverity
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_CharSeverity : uint8 { CHAR_SEVERITY_INFO = 1, CHAR_SEVERITY_WARNING, CHAR_SEVERITY_ERROR };

/// @contract enum Zenith_CharDiagnosticCode
/// Ownership: Plain enumeration; one value per diagnostic the section files name.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3 (the authority block), Stage 4 (the rest). Values never move.
/// @endcontract
enum Zenith_CharDiagnosticCode : uint16
{
	// 100: rigs
	CHAR_DIAG_RIG_NAME_COLLISION = 100, CHAR_DIAG_RIG_JOINT_TWO_ROLES, CHAR_DIAG_RIG_PROFILE_INCOMPLETE, CHAR_DIAG_RIG_TOPOLOGY,
	CHAR_DIAG_RIG_SIDE_SWAPPED, CHAR_DIAG_RIG_ROLE_KEPT, CHAR_DIAG_MIRROR_ASYMMETRIC, CHAR_DIAG_MIRROR_AMBIGUOUS,
	// 200: rest pose, library rigs, retargeting
	CHAR_DIAG_REST_UP_AXIS = 200, CHAR_DIAG_REST_FACING_BACKWARDS, CHAR_DIAG_REST_MIRRORED, CHAR_DIAG_REST_HEIGHT,
	CHAR_DIAG_REST_SCALED_JOINTS, CHAR_DIAG_LIBRARY_RIG_INVALID, CHAR_DIAG_RETARGET_REST_MISMATCH,
	// 300: controls and pose books
	CHAR_DIAG_CONTROL_TARGET_UNKNOWN = 300, CHAR_DIAG_POSEBOOK_UNKNOWN_NAME, CHAR_DIAG_POSEBOOK_NAMES_MISSING,
	// 400: faces and lip-sync
	CHAR_DIAG_VISEME_UNMAPPED = 400, CHAR_DIAG_LIPSYNC_AUDIO_OUTSIDE, CHAR_DIAG_LIPSYNC_LOCALES_DIFFER, CHAR_DIAG_LIPSYNC_OVERRIDE_SUM,
	CHAR_DIAG_LIPSYNC_LINE_ID, CHAR_DIAG_LIPSYNC_STALE, CHAR_DIAG_LIPSYNC_LOCALE_MISSING, CHAR_DIAG_VISEMES_TIME_ORDER,
	CHAR_DIAG_CLIP_KEY_COLLAPSED, CHAR_DIAG_VISEMES_DURATION_MISMATCH, CHAR_DIAG_LIPSYNC_BANK_DEFAULT_LOCALE,
	// 500: assemblies and hair
	CHAR_DIAG_ASSEMBLY_SKELETON_MISMATCH = 500, CHAR_DIAG_ASSEMBLY_CLASS_UNKNOWN, CHAR_DIAG_ASSEMBLY_SLOT_CONFLICT,
	CHAR_DIAG_ASSEMBLY_TOO_MANY_PARTS, CHAR_DIAG_ASSEMBLY_REGION_INVALID, CHAR_DIAG_ASSEMBLY_REGION_HOLE, CHAR_DIAG_ASSEMBLY_REGION_OVERLAP,
	CHAR_DIAG_ASSEMBLY_SOCKET_UNKNOWN, CHAR_DIAG_ASSEMBLY_MATERIAL_INVALID, CHAR_DIAG_ASSEMBLY_PART_UNUSED,
	CHAR_DIAG_ASSEMBLY_ROOT_COMPONENT_RESERVED, CHAR_DIAG_ASSEMBLY_PATH_TAKEN, CHAR_DIAG_ASSEMBLY_RIGID_UNAVAILABLE,
	CHAR_DIAG_ASSEMBLY_EDIT_UNREPRESENTABLE,
	CHAR_DIAG_HAIR_NOT_ALPHA_TESTED = 550, CHAR_DIAG_HAIR_NOT_TWO_SIDED, CHAR_DIAG_HAIR_NO_ANISOTROPY, CHAR_DIAG_HAIR_SLOT_NOT_EXCLUSIVE,
	CHAR_DIAG_HAIR_NO_SHADOW,
	// 600: authority and budgeting
	CHAR_DIAG_AUTHORITY_ANIMATOR_INCOMPLETE = 600, CHAR_DIAG_AUTHORITY_ROOT_MOTION_UNUSED, CHAR_DIAG_AUTHORITY_CONTROLLER_STARVED,
	CHAR_DIAG_AUTHORITY_RAGDOLL_MISMATCH, CHAR_DIAG_AUTHORITY_DYNAMIC_BODY_ON_CHARACTER, CHAR_DIAG_AUTHORITY_MOVED_STATIC_COLLIDER,
	CHAR_DIAG_AUTHORITY_FOLLOWER_INVALID, CHAR_DIAG_AUTHORITY_REDUCED_RATE_THROUGH_COLLISION, CHAR_DIAG_AUTHORITY_CLASS_RATE,
	CHAR_DIAG_BUDGET_BIAS_OVERRIDDEN = 650, CHAR_DIAG_BUDGET_TOO_MANY_HEROES,
	// 700: import, graphs, project
	CHAR_DIAG_IMPORT_NO_SKELETON = 700, CHAR_DIAG_GRAPH_SLOT_MISSING, CHAR_DIAG_PROJECT_DUPLICATE, CHAR_DIAG_PROJECT_INVALID,
	CHAR_DIAG_DOCUMENT_INVALID, CHAR_DIAG_PROJECT_CLASS_NO_CONTENT_RULE,
};

/// @contract value Zenith_CharDiagnostic
/// Ownership: Plain value appended to caller-owned Zenith_Array instances.
/// Thread affinity: IMMUTABLE once appended; members non-const: its producer default-constructs it and fills it member
///   by member.
/// Invalidation: None.
/// Units: m_uIndex is the joint, part, line, control, rule-target or key index the message names (0xFFFFFFFF none).
/// Errors: None; messages longer than 255 bytes are truncated at a code-point boundary.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_CharDiagnostic
{
	Zenith_CharDiagnosticCode m_eCode = CHAR_DIAG_DOCUMENT_INVALID;
	Zenith_CharSeverity m_eSeverity = CHAR_SEVERITY_ERROR;
	uint8 m_uReserved = 0;
	uint32 m_uIndex = 0xFFFFFFFFu;
	Zenith_PersistentID m_xDocument;                 // the document or asset the diagnostic is about
	char m_acRule[8] = {};                           // "AU3", "MV9", "CV12"; empty for rule-less diagnostics
	char m_acMessage[256] = {};                      // UTF-8, NUL-terminated
};
constexpr const char* Zenith_ToString(Zenith_CharDiagnosticCode eCode);
```

### Rig authoring API

```cpp
/// @contract value Zenith_CharSkeletonView
/// Ownership: Borrowed views the caller keeps alive for the call (the editor from a loaded Zenith_SkeletonAsset, tests from
///   fixtures).
/// Thread affinity: IMMUTABLE for the call; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: Rest model matrices in metres, model space (+Y up, front +Z).
/// Errors: Functions refuse spans of unequal length or more than uANIM_MAX_JOINTS joints with CHAR_ERROR_INVALID_ARGUMENT.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharSkeletonView
{
	Zenith_PersistentID m_xSkeleton;
	Zenith_Span<const Zenith_StringView> m_astrNames;     // J joint names, joint order
	Zenith_Span<const uint16> m_auParents;               // J, 0xFFFF for joint 0
	Zenith_Span<const Zenith_Matrix4> m_axRestModel;      // J
	float m_fMaxReach = 0.0f;                             // the skeleton's maximum reach
};

/// @contract value Zenith_CharProfileMatch
/// Ownership: Owns its diagnostics array (the heap the caller's array uses).
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: m_fScore a fraction of the 17 required roles.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharProfileMatch
{
	Zenith_CharRigProfileID m_eProfile = CHAR_RIG_PROFILE_NONE;
	bool m_bHumanoidComplete = false;
	float m_fScore = 0.0f;
	uint32 m_uAssignedRoles = 0;
	uint32 m_auRoleToJoint[uCHAR_ROLE_SLOTS] = {};       // uANIM_INVALID_INDEX when unassigned (set by the matcher)
	Zenith_Array<Zenith_CharDiagnostic> m_axDiagnostics;
};

/// @contract value Zenith_CharRestPoseReport
/// Ownership: Owns its diagnostics array.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Degrees and metres.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharRestPoseReport
{
	Zenith_CharRestPoseClass m_eClass = CHAR_REST_POSE_OTHER;
	Zenith_CharFacing m_eFacing = CHAR_FACING_UNKNOWN;
	float m_fLeftArmElevationDegrees = 0.0f, m_fRightArmElevationDegrees = 0.0f;
	float m_fUpAngleDegrees = 0.0f;
	float m_fHeightMetres = 0.0f;
	uint32 m_uScaledJointCount = 0;
	Zenith_Array<Zenith_CharDiagnostic> m_axDiagnostics;
};

/// @contract value Zenith_CharJsonEdit
/// Ownership: Owns its pointer and value text.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: m_strPointer an RFC 6901 JSON pointer into the target document; m_strValueJson canonical JSON text.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharJsonEdit
{
	Zenith_CharJsonEditOp m_eOp = CHAR_JSON_EDIT_OP_SET;  // Types.md "Enumerations for coded fields"
	Zenith_Array<char> m_strPointer;
	Zenith_Array<char> m_strValueJson;                    // empty for remove
};
using Zenith_CharRigEditList = Zenith_Array<Zenith_CharJsonEdit>;

/// @contract value Zenith_CharRigBuildInputs
/// Ownership: Borrowed views for the call.
/// Thread affinity: IMMUTABLE for the call; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: As the views.
/// Errors: None of its own.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharRigBuildInputs
{
	const Zenith_CharSkeletonView* m_pxSkeleton = nullptr;
	const Zenith_CharProfileMatch* m_pxMatch = nullptr;
	Zenith_Span<const uint32> m_auMirror;                // from Zenith_CharDetectMirrorPairs
	const Zenith_JsonDocument* m_pxExistingRig = nullptr; // null: a new document
	Zenith_Span<const Zenith_PersistentID> m_axShellMeshes;
	Zenith_Span<const Zenith_StringView> m_astrControlCurves;   // Face-group control curves for the facial template
	bool m_bFacial = false;
	bool m_bOverwrite = false;
};

/// @contract functions Zenith_CharRigFunctions
/// Ownership: Pure functions; outputs are caller-owned and allocated from the given heap or the outputs' own arrays.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: As Rigging.md.
/// Errors: CHAR_ERROR_INVALID_ARGUMENT, CHAR_ERROR_LIMIT_EXCEEDED, CHAR_ERROR_OUT_OF_MEMORY, CHAR_ERROR_SKELETON_MISMATCH;
///   content problems are diagnostics, never errors.
/// Version: since Z2 Stage 4. Deterministic: identical inputs give identical bits on every machine running one build
///   (CORE's deterministic functions, no floating-point contraction; Facial.md "Estimator input").
/// @endcontract
Zenith_Result<void, Zenith_CharError> Zenith_CharNormaliseJointName(Zenith_StringView strName, uint32 uJointIndex,
	Zenith_Array<char>& xOut);
Zenith_Result<void, Zenith_CharError> Zenith_CharMatchRigProfile(const Zenith_CharSkeletonView& xSkeleton,
	Zenith_CharRigProfileID eProfile, Zenith_CharProfileMatch& xOut);
Zenith_Result<void, Zenith_CharError> Zenith_CharDetectRigProfile(const Zenith_CharSkeletonView& xSkeleton,
	Zenith_CharProfileMatch& xOut);
Zenith_Result<void, Zenith_CharError> Zenith_CharDetectMirrorPairs(const Zenith_CharSkeletonView& xSkeleton,
	Zenith_Span<uint32> auMirrorOut, Zenith_Array<Zenith_CharDiagnostic>& axDiagnostics);
Zenith_Result<void, Zenith_CharError> Zenith_CharClassifyRestPose(const Zenith_CharSkeletonView& xSkeleton,
	const Zenith_CharProfileMatch& xMatch, const float afHeightRangeMetres[2], Zenith_CharRestPoseReport& xOut);
Zenith_Result<void, Zenith_CharError> Zenith_CharBuildRigDocumentEdits(const Zenith_CharRigBuildInputs& xInputs,
	Zenith_CharRigEditList& xOut, Zenith_Heap& xHeap);
Zenith_Result<void, Zenith_CharError> Zenith_CharBuildDefaultRagdoll(const Zenith_CharSkeletonView& xSkeleton,
	const Zenith_CharProfileMatch& xMatch, float fMassKilograms, Zenith_StringView strSkeletonPath, Zenith_Array<char>& xJsonOut);
Zenith_Result<void, Zenith_CharError> Zenith_CharBuildRetargetDocument(const Zenith_CharProfileMatch& xSource,
	const Zenith_CharProfileMatch& xTarget, const Zenith_CharSkeletonView& xSourceView, const Zenith_CharSkeletonView& xTargetView,
	Zenith_Span<const Zenith_PersistentID> axClips, Zenith_Array<char>& xJsonOut, Zenith_Array<Zenith_CharDiagnostic>& axDiagnostics);
Zenith_Result<void, Zenith_CharError> Zenith_CharCheckRetargetRestPose(const Zenith_SkeletonAsset& xSource,
	const Zenith_SkeletonAsset& xTarget, Zenith_Heap& xHeap, Zenith_Array<Zenith_CharDiagnostic>& axDiagnostics);
```

### Controls and pose book API

```cpp
/// @contract value Zenith_CharControlSet
/// Ownership: Owns its arrays; built by Zenith_CharReadControlSet from a migrated document.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: As Rigging.md "Control set document".
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharControlSet
{
	Zenith_PersistentID m_xSkeleton;
	Zenith_Array<Zenith_CharControl> m_axControls;          // document order (Types.md "Control and pose book member types")
	Zenith_Array<Zenith_Array<char>> m_astrGroups;
};

/// @contract value Zenith_CharControlOutputs
/// Ownership: Owns its arrays; reused across calls (cleared by Zenith_CharEvaluateControls).
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: As Rigging.md "Control evaluation".
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharControlOutputs
{
	Zenith_Array<Zenith_CharParameterValue> m_axParameters;  // in control order; the element types are declared in
	Zenith_Array<Zenith_CharCurveValue> m_axCurves;          //   Types.md "Control and pose book member types"
	Zenith_Array<Zenith_CharPoseWeight> m_axPoses;
	Zenith_Array<Zenith_CharJointRotation> m_axRotations;
	Zenith_Array<Zenith_CharIKGoalOutput> m_axIKGoals;       // in goal order
	uint32 m_uIgnoredNonFinite = 0;
	uint32 m_uSkippedUnresolved = 0;                       // controls skipped because their target is unresolved
	uint32 m_uRefusedCalls = 0;                            // calls refused for span lengths (Rigging.md "Control evaluation")
};

/// @contract value Zenith_CharPoseBook
/// Ownership: Owns its arrays; built by Zenith_CharReadPoseBook.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Local transforms in metres; curve units.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharPoseBook
{
	Zenith_PersistentID m_xSkeleton;
	Zenith_Array<Zenith_CharPose> m_axPoses;                 // document order (Types.md "Control and pose book member types")
};

/// @contract value Zenith_CharCurveClip
/// Ownership: Owns its channels.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Seconds (double before writing); metres, unit quaternions, curve units.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharCurveClip
{
	Zenith_PersistentID m_xSkeletonProduct;
	double m_dDurationSeconds = 0.0;
	Zenith_Array<Zenith_CharClipChannel> m_axChannels;        // Types.md "Clip member types"
};

/// @contract functions Zenith_CharControlAndPoseBookFunctions
/// Ownership: Pure functions; outputs caller-owned.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: As Rigging.md.
/// Errors: CHAR_ERROR_INVALID_DOCUMENT (with diagnostics), CHAR_ERROR_LIMIT_EXCEEDED, CHAR_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 4.
/// @endcontract
Zenith_Result<void, Zenith_CharError> Zenith_CharReadControlSet(const Zenith_JsonDocument& xDocument,
	Zenith_CharControlSet& xOut, Zenith_Array<Zenith_CharDiagnostic>& axDiagnostics);
Zenith_Result<void, Zenith_CharError> Zenith_CharResolveControlSet(Zenith_CharControlSet& xInOut,
	const Zenith_CharSkeletonView& xSkeleton, Zenith_Span<const Zenith_CharPoseBook* const> apxPoseBooks,
	Zenith_Array<Zenith_CharDiagnostic>& axDiagnostics);   // fills target indices; CHAR_DIAG_CONTROL_TARGET_UNKNOWN
// afValues: one per control in document order; axPoints: one per Point control in document order (Rigging.md "Control evaluation").
void Zenith_CharEvaluateControls(const Zenith_CharControlSet& xSet, Zenith_Span<const float> afValues,
	Zenith_Span<const Zenith_Vector3> axPoints, Zenith_CharControlOutputs& xOut);
Zenith_Result<void, Zenith_CharError> Zenith_CharReadPoseBook(const Zenith_JsonDocument& xDocument,
	Zenith_CharPoseBook& xOut, Zenith_Array<Zenith_CharDiagnostic>& axDiagnostics);
Zenith_Result<void, Zenith_CharError> Zenith_CharBuildPoseBookClip(const Zenith_CharPoseBook& xBook,
	Zenith_Span<const Zenith_StringView> astrJointNames, Zenith_Span<const Zenith_Transform> axRestLocal,
	Zenith_Span<const Zenith_StringView> astrCurveNames, Zenith_Span<const float> afCurveDefaults, Zenith_CharCurveClip& xOut);
// Rounds every time to f32 once and drops the second of each equal pair (CHAR_DIAG_CLIP_KEY_COLLAPSED, counted per channel);
// the importers then convert the clip to ANIM's writer descriptor (Facial.md "Clip intermediate writer", row CH15).
Zenith_Result<void, Zenith_CharError> Zenith_CharRoundClipTimes(Zenith_CharCurveClip& xInOut,
	Zenith_Array<Zenith_CharDiagnostic>& axDiagnostics);
```

### Lip-sync API

```cpp
/// @contract value Zenith_CharLipSyncSettings
/// Ownership: Plain value; the defaults are Facial.md "Lip-sync document".
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: Decibels, seconds, value units, Hz.
/// Errors: Out-of-range members are refused by the document reader, never here.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharLipSyncSettings
{
	double m_dSilenceDb = -45.0;
	double m_dLeadSeconds = 0.04;
	double m_dAttackSeconds = 0.025;
	double m_dReleaseSeconds = 0.06;
	double m_dReduceEpsilon = 0.002;
	uint32 m_uSampleRateHz = 60;
	double m_dPadSeconds = 0.25;
};

/// @contract value Zenith_CharVisemeTrack
/// Ownership: Owns sixteen key arrays (one per viseme-set curve, Zenith_CharGetVisemeCurveName order).
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Seconds from the line's start; weights in [0, 1].
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharVisemeTrack
{
	double m_dDurationSeconds = 0.0;
	Zenith_Array<double> m_adTimes[uCHAR_VISEME_SET_CURVES];
	Zenith_Array<float> m_afValues[uCHAR_VISEME_SET_CURVES];
};

/// @contract value Zenith_CharLipSyncLayout
/// Ownership: Owns its clip, markers and events.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Seconds.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharLipSyncLayout
{
	double m_dStrideSeconds = 0.0;
	Zenith_CharCurveClip m_xClip;                          // sixteen curve channels over L windows
	Zenith_Array<Zenith_CharClipMarker> m_axMarkers;       // one "char.lipsync.<tag>" per window (Types.md "Clip member types")
	Zenith_Array<Zenith_CharClipEvent> m_axEvents;         // one "char.lipsync.windowEnd" per window, payload l
};

/// @contract functions Zenith_CharLipSyncFunctions
/// Ownership: Pure functions; scratch from the arena passed, outputs caller-owned.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: As Facial.md.
/// Errors: CHAR_ERROR_AUDIO_DECODE (never produced here: decoding is the caller's), CHAR_ERROR_INVALID_DOCUMENT,
///   CHAR_ERROR_LIMIT_EXCEEDED, CHAR_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 4. Deterministic: identical inputs give identical bits on every machine running one build: no
///   CRT transcendental (CORE's Zenith_Sin, Zenith_Cos, Zenith_Exp, Zenith_Ln, Zenith_Log10), binary64 in a fixed order, no floating-point
///   contraction (Facial.md "Estimator input"; the X314-FMA3 ruling).
/// @endcontract
Zenith_Result<void, Zenith_CharError> Zenith_CharEstimateVisemes(const Zenith_DecodedAudio& xAudio,
	const Zenith_CharLipSyncSettings& xSettings, Zenith_CharVisemeTrack& xOut, Zenith_Arena& xScratch);
Zenith_Result<void, Zenith_CharError> Zenith_CharConvertVisemeCues(const Zenith_JsonDocument& xVisemesFile,
	const Zenith_CharLipSyncSettings& xSettings, Zenith_CharVisemeTrack& xOut, Zenith_Array<Zenith_CharDiagnostic>& axDiagnostics);
Zenith_Result<void, Zenith_CharError> Zenith_CharApplyPhrases(Zenith_Span<const double> adPhraseStarts,
	Zenith_CharVisemeTrack& xTrack);
Zenith_Result<void, Zenith_CharError> Zenith_CharApplyOverrides(const Zenith_JsonDocument& xDocument,
	Zenith_JsonNode xOverrides, Zenith_CharVisemeTrack& xTrack, Zenith_Array<Zenith_CharDiagnostic>& axDiagnostics);
Zenith_Result<void, Zenith_CharError> Zenith_CharLayoutLipSyncClip(const Zenith_PersistentID& xSkeletonProduct,
	Zenith_Span<const Zenith_StringView> astrLocales, Zenith_Span<const Zenith_CharVisemeTrack* const> apxTracks,   // null: silent
	const Zenith_CharLipSyncSettings& xSettings, Zenith_CharLipSyncLayout& xOut);
void Zenith_CharFft512(const double adIn[512], double adReOut[512], double adImOut[512]);
```

### Assembly and template API

```cpp
/// @contract value Zenith_CharAssemblyView
/// Ownership: Owns its arrays; built by Zenith_CharReadAssembly from a migrated assembly document.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: As Modular.md "Assembly document".
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharAssemblyView
{
	Zenith_PersistentID m_xID, m_xSkeleton, m_xGraph, m_xRagdoll;
	Zenith_Array<char> m_strName, m_strClass;
	Zenith_Array<Zenith_Array<char>> m_astrRegions;
	Zenith_Array<Zenith_CharSlot> m_axSlots;               // document order; the element types are declared in
	Zenith_Array<Zenith_CharPart> m_axParts;               //   Types.md "Assembly member types"; parts in ascending part ID,
	Zenith_Array<Zenith_CharLoadout> m_axLoadouts;         //   loadouts in ascending loadout ID
	uint32 m_uDefaultLoadout = 0;
	const Zenith_JsonDocument* m_pxDocument = nullptr;     // for rootComponents and generated, borrowed
};

/// @contract value Zenith_CharClassDesc
/// Ownership: Plain value read from the project document.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: None.
/// Units: Metres, kilograms, a factor.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharClassDesc
{
	float m_afHeightRangeMetres[2] = { 1.2f, 2.3f };
	float m_fMassKilograms = 70.0f;
	float m_fSignificanceBias = 1.0f;
	bool m_bForceFullRate = false;
	bool m_bAllowReducedRootMotion = false;
};

/// @contract value Zenith_CharGeneratedDocument
/// Ownership: Owns its bytes.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Bytes of canonical JSON.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharGeneratedDocument
{
	Zenith_PersistentID m_xID;
	Zenith_PersistentID m_xLoadout;                         // null for the base prefab
	Zenith_Array<uint8> m_auBytes;
};

/// @contract value Zenith_CharTemplateRequest
/// Ownership: Borrowed views for the call.
/// Thread affinity: IMMUTABLE for the call; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: None.
/// Errors: None of its own.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharTemplateRequest
{
	Zenith_StringView m_strTemplate;                        // "char.humanoidLocomotion"
	Zenith_PersistentID m_xSkeleton, m_xGraphDocument;
	Zenith_Span<const Zenith_CharClipCandidate> m_axClips; // Types.md "Template and scenario types"
	Zenith_Span<const Zenith_CharRoleBinding> m_axExplicit; // the command's bindings argument
	Zenith_Span<const Zenith_PersistentID> m_axMaskDocuments;   // two IDs: LipSyncCurves, Face (used only with m_bFacial)
	Zenith_PersistentID m_xFacePoseBook;                    // null: no POSE_BLEND entries
	bool m_bFacial = false;
};

/// @contract value Zenith_CharTemplateOutput
/// Ownership: Owns its documents.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Bytes of canonical JSON.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharTemplateOutput
{
	Zenith_Array<uint8> m_auGraphDocument;
	Zenith_Array<uint8> m_auLipSyncMask, m_auFaceMask;     // empty without m_bFacial
	Zenith_Array<Zenith_CharRoleBinding> m_axBinding;       // every bound role
	Zenith_Array<Zenith_CharDiagnostic> m_axDiagnostics;
};

/// @contract functions Zenith_CharAssemblyAndTemplateFunctions
/// Ownership: Pure functions; outputs caller-owned from the given heap.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: As Modular.md and Workflow.md.
/// Errors: CHAR_ERROR_INVALID_DOCUMENT, CHAR_ERROR_LIMIT_EXCEEDED, CHAR_ERROR_TEMPLATE_UNKNOWN, CHAR_ERROR_BINDING_MISSING,
///   CHAR_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 4.
/// @endcontract
Zenith_Result<void, Zenith_CharError> Zenith_CharReadAssembly(const Zenith_JsonDocument& xDocument,
	Zenith_CharAssemblyView& xOut, Zenith_Array<Zenith_CharDiagnostic>& axDiagnostics);
void Zenith_CharComputeVisibility(const Zenith_CharAssemblyView& xAssembly, uint32 uLoadout, Zenith_Span<bool> abVisibleOut);
Zenith_Result<void, Zenith_CharError> Zenith_CharBuildAssemblyPrefabs(const Zenith_CharAssemblyView& xAssembly,
	const Zenith_CharClassDesc& xClass, bool bGraphHasIK, Zenith_Span<const Zenith_PersistentID> axDocumentIDs,
	Zenith_Array<Zenith_CharGeneratedDocument>& axOut, Zenith_Heap& xHeap);   // axDocumentIDs: base, then non-default loadouts
Zenith_Span<const Zenith_CharTemplateDesc> Zenith_CharGetGraphTemplates();
Zenith_Result<void, Zenith_CharError> Zenith_CharBindTemplateClips(const Zenith_CharTemplateDesc& xTemplate,
	Zenith_Span<const Zenith_CharClipCandidate> axClips, Zenith_Span<const Zenith_CharRoleBinding> axExplicit,
	Zenith_Array<Zenith_CharRoleBinding>& axOut, Zenith_Array<Zenith_CharDiagnostic>& axDiagnostics);
Zenith_Result<void, Zenith_CharError> Zenith_CharInstantiateTemplate(const Zenith_CharTemplateRequest& xRequest,
	Zenith_CharTemplateOutput& xOut, Zenith_Heap& xHeap);
Zenith_Result<void, Zenith_CharError> Zenith_CharReadScenario(Zenith_ByteSpan xBytes, Zenith_CharScenario& xOut);
Zenith_Result<void, Zenith_CharError> Zenith_CharBuildBuiltInScenario(const Zenith_CharTemplateDesc& xTemplate,
	Zenith_StringView strName, Zenith_Span<const Zenith_CharClipCandidate> axBoundClips, Zenith_Span<const Zenith_CharRoleBinding> axBinding,
	const Zenith_CharClipCandidate* pxLipSyncClip, Zenith_CharScenario& xOut);   // Workflow.md "Preview scenarios"; null
	                                                        // pxLipSyncClip: lipsync-line is CHAR_ERROR_NOT_AVAILABLE
Zenith_Result<void, Zenith_CharError> Zenith_CharRunScenario(const Zenith_CharScenario& xScenario,
	Zenith_AnimationPreview& xPreview, uint32 uTickHz, Zenith_CharScenarioResult& xOut);
// Keep as variant (Modular.md "Keep as variant algorithm"): the editor resolves E and G with SCENE's Zenith_ResolveDocument
// into xResolvedEdited and xResolvedGenerated; xEdited is E's document (its document-level members, parents and sort keys).
// CHAR_ERROR_EDIT_UNREPRESENTABLE with one diagnostic per unrepresentable edit, writing nothing to xVariantJsonOut.
Zenith_Result<void, Zenith_CharError> Zenith_CharBuildKeepAsVariant(const Zenith_JsonDocument& xEdited,
	const Zenith_SceneData& xResolvedEdited, const Zenith_SceneData& xResolvedGenerated, const Zenith_PersistentID& xVariantID,
	Zenith_Array<char>& xVariantJsonOut, Zenith_Array<Zenith_CharDiagnostic>& axDiagnostics);
```

`Zenith_CharTemplateDesc`, `Zenith_CharClipCandidate`, `Zenith_CharRoleBinding`, `Zenith_CharScenario` and `Zenith_CharScenarioResult` are declared in full, each with its contract block, in `Zenith2/Docs/Design/CHAR/Types.md → "Template and scenario types"`.

### Validation API

```cpp
/// @contract value Zenith_CharEntityRecord
/// Ownership: Borrowed view into a resolved document the caller keeps alive for the call.
/// Thread affinity: IMMUTABLE for the call; members non-const: an element its producer writes in place in an arena or
///   Zenith_Array.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_CharEntityRecord
{
	Zenith_PersistentID m_xEntity;
	uint32 m_uParent = uCHAR_NO_PARENT;                     // index into the record span
	const Zenith_JsonDocument* m_pxDocument = nullptr;
	Zenith_Span<const Zenith_CharComponentRef> m_axComponents;   // sorted by stable name (Types.md "Validation member types")
	Zenith_PersistentID m_xGeneratingAssembly;               // null when not generated by an assembly
};

/// @contract value Zenith_CharValidationRequest
/// Ownership: Borrowed views for the call.
/// Thread affinity: IMMUTABLE for the call; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: None.
/// Errors: None of its own.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharValidationRequest
{
	Zenith_Span<const Zenith_CharDocumentRef> m_axDocuments;  // Types.md "Validation member types"
	const Zenith_JsonDocument* m_pxProject = nullptr;         // the game's project document, null when absent (CV13 reports it)
	uint32 m_uProjectDocumentCount = 0;
};

/// @contract functions Zenith_CharValidationFunctions
/// Ownership: Pure functions over the inputs and the query interface; diagnostics appended to the caller's array.
/// Thread affinity: ANY_THREAD, as the query implementation allows (the editor's is MAIN_THREAD).
/// Invalidation: None.
/// Units: None.
/// Errors: CHAR_ERROR_OUT_OF_MEMORY only; every content problem is a diagnostic.
/// Version: since Z2 Stage 3 (the authority lint), Stage 4 (the rest).
/// @endcontract
Zenith_Result<void, Zenith_CharError> Zenith_CharLintCharacterAuthority(Zenith_Span<const Zenith_CharEntityRecord> axEntities,
	Zenith_CharAssetQuery& xQuery, Zenith_Array<Zenith_CharDiagnostic>& axOut);
Zenith_Result<void, Zenith_CharError> Zenith_CharValidateAssembly(const Zenith_CharAssemblyView& xAssembly,
	Zenith_CharAssetQuery& xQuery, Zenith_Array<Zenith_CharDiagnostic>& axOut);
Zenith_Result<void, Zenith_CharError> Zenith_CharValidate(const Zenith_CharValidationRequest& xRequest,
	Zenith_CharAssetQuery& xQuery, Zenith_Array<Zenith_CharDiagnostic>& axOut);
```

### Asset query interface

```cpp
/// @contract value Zenith_CharMaterialFacts
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharMaterialFacts
{
	Zenith_CharMaterialDomain m_eDomain = CHAR_MATERIAL_DOMAIN_OPAQUE;   // Types.md "Enumerations for coded fields"
	bool m_bTwoSided = false;
	bool m_bAnisotropy = false;    // surfaceModelFlags holds "anisotropy"
};

/// @contract interface Zenith_CharAssetQuery
/// Ownership: Implemented by the caller (the editor over its loaded assets, tests over fixtures); borrowed for one call.
/// Thread affinity: The implementation's; CHAR calls it on the thread that called the CHAR function.
/// Invalidation: None.
/// Units: None.
/// Errors: Each member returns false for an asset it cannot answer for (unknown, not loaded, wrong kind); CHAR then reports
///   the rule as unverifiable with an information diagnostic, never an error.
/// Reentrancy: NONE. An implementation never calls back into CHAR.
/// Version: since Z2 Stage 3 (skeleton members), Stage 4 (the rest).
/// @endcontract
class Zenith_CharAssetQuery
{
public:
	virtual ~Zenith_CharAssetQuery() = default;
	virtual bool GetGraphSkeleton(const Zenith_PersistentID& xGraph, Zenith_PersistentID& xOut) = 0;
	virtual bool GetRagdollSkeleton(const Zenith_PersistentID& xRagdoll, Zenith_PersistentID& xOut) = 0;
	virtual bool GetMeshSkeleton(const Zenith_PersistentID& xMesh, Zenith_PersistentID& xOut, bool& bSkinnedOut) = 0;
	virtual bool GetMeshMaterials(const Zenith_PersistentID& xMesh, Zenith_Array<Zenith_PersistentID>& axOut) = 0;
	virtual bool GetMaterialFacts(const Zenith_PersistentID& xMaterial, Zenith_CharMaterialFacts& xOut) = 0;
	virtual bool GetMeshMorphTargetHashes(const Zenith_PersistentID& xMesh, Zenith_Array<uint64>& axOut) = 0;
	virtual const Zenith_CharSkeletonView* FindSkeletonView(const Zenith_PersistentID& xSkeleton) = 0;
	virtual bool HasSocket(const Zenith_PersistentID& xSkeleton, uint64 ulSocketHash) = 0;
	virtual bool GetAssetLabels(const Zenith_PersistentID& xAsset, Zenith_Array<Zenith_Array<char>>& astrOut) = 0;
	virtual bool FileExists(Zenith_StringView strPath) = 0;
	// The selectors (.meta labels) of the content-rule table's rows of one table kind, for CV17; the editor answers from the
	// cook server's loaded Zenith2/Docs/ContentSpecs.rules.json (Debugging.md "Content rule measures").
	virtual bool GetContentRuleSelectors(Zenith_StringView strTable, Zenith_Array<Zenith_Array<char>>& astrOut) = 0;
	// A bank's locale tags of one dialogue line, in the bank's order, for CV18 (Facial.md "Sync from bank" step 7).
	virtual bool GetBankLineLocales(const Zenith_PersistentID& xBank, Zenith_StringView strLine,
		Zenith_Array<Zenith_Array<char>>& astrOut) = 0;
};
```

### Pose debugging API

```cpp
/// @contract value Zenith_CharPoseSnapshot
/// Ownership: Owns its arrays.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Local transforms in metres and unit quaternions; curve units.
/// Errors: None.
/// Version: since Z2 Stage 4. Serialized as zenith2.char-pose-snapshot version 1 (Debugging.md "Pose snapshot format").
/// @endcontract
struct Zenith_CharPoseSnapshot
{
	Zenith_PersistentID m_xSkeleton;
	uint64 m_ulRestPoseHash = 0;
	Zenith_CharPoseSourceKind m_eSourceKind = CHAR_POSE_SOURCE_FILE;   // Types.md "Enumerations for coded fields"
	Zenith_PersistentID m_xEntity;
	uint64 m_ulTick = 0;
	Zenith_Array<Zenith_Array<char>> m_astrJointNames;
	Zenith_Array<Zenith_Transform> m_axLocal;
	Zenith_Array<Zenith_Array<char>> m_astrCurveNames;
	Zenith_Array<float> m_afCurveValues;
};

/// @contract value Zenith_CharPoseDiffTolerance
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Metres, radians, scale factor, curve units.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharPoseDiffTolerance { const float m_fMetres = 1e-4f, m_fRadians = 1e-3f, m_fScale = 1e-4f, m_fCurve = 1e-4f; };

/// @contract value Zenith_CharPoseDiff
/// Ownership: Owns its arrays.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: As the tolerance.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharPoseDiff
{
	Zenith_Array<Zenith_CharJointDelta> m_axJoints;          // Types.md "Pose debugging member types"
	Zenith_Array<Zenith_CharCurveDelta> m_axCurves;
	Zenith_Array<Zenith_Array<char>> m_astrUnpaired;
	float m_fMaxMetres = 0.0f, m_fMaxRadians = 0.0f, m_fMaxScale = 0.0f;
	uint32 m_uMaxMetresJoint = 0, m_uMaxRadiansJoint = 0, m_uMaxScaleJoint = 0;
};

/// @contract value Zenith_CharBudgetKeys
/// Ownership: Plain value: ANIM's animation.budget.* distances as configured.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Metres.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharBudgetKeys { const float m_fFullRateDistance = 15.0f, m_fHalfRateDistance = 40.0f, m_fQuarterRateDistance = 80.0f; };

/// @contract functions Zenith_CharPoseDebugFunctions
/// Ownership: Pure functions; outputs caller-owned.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: As Debugging.md.
/// Errors: CHAR_ERROR_SNAPSHOT_INVALID, CHAR_ERROR_LIMIT_EXCEEDED, CHAR_ERROR_SKELETON_MISMATCH (no pairing possible),
///   CHAR_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 4.
/// @endcontract
Zenith_Result<void, Zenith_CharError> Zenith_CharWritePoseSnapshot(const Zenith_CharPoseSnapshot& xSnapshot,
	Zenith_Array<char>& xJsonOut);
Zenith_Result<void, Zenith_CharError> Zenith_CharReadPoseSnapshot(Zenith_ByteSpan xBytes, Zenith_CharPoseSnapshot& xOut);
Zenith_Result<void, Zenith_CharError> Zenith_CharDiffPoses(const Zenith_CharPoseSnapshot& xA, const Zenith_CharPoseSnapshot& xB,
	const Zenith_CharPoseDiffTolerance& xTolerance, Zenith_CharPoseDiff& xOut);
// 1, 2, 4 or 8: ANIM's Zenith_AnimComputeRateDivisor (row CH15) with bForceFullRate = the full-rate flag or bRagdollActive.
uint8 Zenith_CharPredictRateDivisor(float fDistanceMetres, float fBias, uint32 uAnimatorFlags, bool bRagdollActive,
	const Zenith_CharBudgetKeys& xKeys);
```

### Document registration API

```cpp
/// @contract functions Zenith_CharDocumentRegistration
/// Ownership: Registers CHAR's five document types into the caller's registry.
/// Thread affinity: MAIN_THREAD, before the registry is frozen.
/// Invalidation: None.
/// Units: None.
/// Errors: The first failing RegisterDocumentType result unchanged (DOCUMENT_ERROR_DUPLICATE_REGISTRATION, …).
/// Version: since Z2 Stage 4. Called by ZenithCook's services and by the editor's document type registration.
/// @endcontract
Zenith_Result<void, Zenith_DocumentError> Zenith_RegisterCharacterDocumentTypes(Zenith_MigrationRegistry& xRegistry);
```

### Importers and cook registration

```cpp
// Zenith2/Tools/ZenithCook/Internal/Character/Zenith_CharacterCook.h (ZenithCook-internal; ASSET's step-registration rule)
#pragma once
#include "ZenithCore/Zenith_Result.h"

class Zenith_ImporterRegistry;
struct Zenith_ImportError;

// Registers char.lipsync (.lipsync.json, out of process), char.posebook (.posebook.json) and char.document
// (.character.json, .charcontrols.json, .charproject.json, .visemes.json, .posebooknames.json, .charscenario.json,
// .posesnapshot.json; validation only, zero products), at the cook services' importer slot after CONTENT's
// (Zenith_PfnRegisterImporters, row CH7). CHAR registers no cook step and no content-rule measure (Debugging.md
// "Content rule measures"), so it has no step registration function.
Zenith_Result<void, Zenith_ImportError> Zenith_RegisterCharacterImporters(Zenith_ImporterRegistry& xRegistry);
```

| Importer | Suffixes | Products | Version | Out of process |
|---|---|---|---|---|
| `char.lipsync` | `.lipsync.json` | `anim:lip.<line ID text>` (`Animation.Clip`) per line | 1 + `uCHAR_LIPSYNC_ESTIMATOR_VERSION` | yes |
| `char.posebook` | `.posebook.json` | `anim:posebook` (`Animation.Clip`, primary) | 1 | no |
| `char.document` | `.character.json`, `.charcontrols.json`, `.charproject.json`, `.visemes.json`, `.posebooknames.json`, `.charscenario.json`, `.posesnapshot.json` | none | 1 | no |

### Editor extension header

```cpp
// Zenith2/Tools/ZenithEditor/Internal/Character/Zenith_CharacterExtension.h (ZenithEditor-internal)
/// @contract type Zenith_CharacterEditorExtension
/// Ownership: A constexpr-constructed entry of EDIT's extension table; never freed.
/// Thread affinity: MAIN_THREAD; Register runs once at editor startup step 12.
/// Invalidation: None.
/// Units: None.
/// Errors: Register returns EDIT's registration errors unchanged.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_CharacterEditorExtension final : public Zenith_EditorExtension
{
public:
	const char* GetName() const override;     // "Character"
	Zenith_Result<void, Zenith_EditorError> Register(Zenith_EditorExtensionContext& xContext) override;
};
```

### Handle kinds used

| Handle | Kind | CHAR's use |
|---|---|---|
| `Zenith_PersistentID` | persistent ID | every document, asset, part, loadout and entity reference CHAR writes |
| `Zenith_AssetHandle<…>` | shared asset handle | held by the editor's asset query and previews, never by ZenithAuthoring |
| `Zenith_EntityHandle` | world handle | play-world characters in the editor's pose debugger and play driver; never serialized |

## Data structures and formats

### Format index

| Format | Kind | Specified in |
|---|---|---|
| `.character.json` (`zenith2.character` v1) | authoring document | `Zenith2/Docs/Design/CHAR/Modular.md → "Assembly document"` |
| `.lipsync.json` (`zenith2.lipsync` v1) | authoring document | `Zenith2/Docs/Design/CHAR/Facial.md → "Lip-sync document"` |
| `.posebook.json` (`zenith2.posebook` v1) | authoring document | `Zenith2/Docs/Design/CHAR/Rigging.md → "Pose book document"` |
| `.charcontrols.json` (`zenith2.charcontrols` v1) | authoring document | `Zenith2/Docs/Design/CHAR/Rigging.md → "Control set document"` |
| `.charproject.json` (`zenith2.charproject` v1) | authoring document | `Zenith2/Docs/Design/CHAR/Facial.md → "Character project settings document"` |
| `.visemes.json` (`zenith2.visemes` v1) | viseme timing input (not migrated; a reader refuses another version) | `Zenith2/Docs/Design/CHAR/Facial.md → "External viseme import"` |
| `<name>.posebooknames.json` (`zenith2.posebook-names` v1) | editor-written import input | `Zenith2/Docs/Design/CHAR/Rigging.md → "Pose book name validation"` |
| `.posesnapshot.json` (`zenith2.char-pose-snapshot` v1) | development artefact | `Zenith2/Docs/Design/CHAR/Debugging.md → "Pose snapshot format"` |
| `.charscenario.json` (`zenith2.char-scenario` v1) | test and preview input | `Zenith2/Docs/Design/CHAR/Workflow.md → "Preview scenarios"` |
| ZICL v1 of CHAR's clips | ANIM's clip intermediate, written by ANIM's writer from CHAR's curve clips | `Zenith2/Docs/Design/CHAR/Facial.md → "Clip intermediate writer"` |
| generated `.prefab.json` | SCENE's prefab document | `Zenith2/Docs/Design/CHAR/Modular.md → "Generated prefab layout"` |

### Document type table

| Stable name | Schema tag | Suffix | First version | Stage |
|---|---|---|---|---|
| `Zenith.CharacterAssemblyDocument` | `zenith2.character` | `.character.json` | 1 | S4 |
| `Zenith.CharacterLipSyncDocument` | `zenith2.lipsync` | `.lipsync.json` | 1 | S4 |
| `Zenith.CharacterPoseBookDocument` | `zenith2.posebook` | `.posebook.json` | 1 | S4 |
| `Zenith.CharacterControlSetDocument` | `zenith2.charcontrols` | `.charcontrols.json` | 1 | S4 |
| `Zenith.CharacterProjectDocument` | `zenith2.charproject` | `.charproject.json` | 1 | S4 |

Each is canonical JSON under SCENE's canonical rules, with `id` as its ID key and `schemaVersion` as its version key; keyed maps sort by key (rule C4), ordered arrays keep their order (rule C8).

### Character document limits

| Reader | Quantity | Limit |
|---|---|---|
| every CHAR document | bytes | 64 MiB (SCENE's open limit) |
| assembly | regions, slots, parts, loadouts, parts per loadout, root components | 16, 16, 64, 32, 64, 64 |
| lip-sync | lines, locales, overrides per entry, keys per override, phrases per entry, audio seconds per entry | 4,096, 16, 256, 65,536, 1,024, 600 |
| visemes file | cues | 65,536 |
| pose book | poses, joints per pose, curves per pose | 255, 1,024, 512 |
| control set | controls, groups | 256, 32 |
| project | voice locales, classes | 16, 16 |
| pose snapshot | joints, curves | 1,024, 512 |
| scenario | steps, assertions, duration | 4,096, 256, 600 s |

These rows join `Zenith2/Docs/Architecture.md → "Security architecture"` (table "Parser limits") through core-doc item X329; each is tested at the limit and at limit + 1 ([Z2-N-ARCH-953]).

### Byte order and serialization rules

CHAR writes JSON documents only (Z2-D-038); the ZICL intermediates of its clips are written by ANIM's writer, so CHAR encodes no byte of another area's format and nothing CHAR writes depends on a C++ layout. Floats in documents are written with 9 significant digits (exact for f32), times in seconds rounded to 1e-6 where the section files say so.

## Algorithms and state machines

### Algorithm index

| Algorithm | Complexity | Specified in |
|---|---|---|
| joint name normalisation and tokens | O(name length) | `Zenith2/Docs/Design/CHAR/Rigging.md → "Joint name normalisation"` |
| profile matching (all profiles) | O(P × R × n log J + J) | `Zenith2/Docs/Design/CHAR/Rigging.md → "Profile matching algorithm"` |
| generic assignment | O(J × depth) | `Zenith2/Docs/Design/CHAR/Rigging.md → "Generic profile assignment"` |
| mirror detection | O(J log J) | `Zenith2/Docs/Design/CHAR/Rigging.md → "Mirror pair detection"` |
| rest-pose classification | O(J) | `Zenith2/Docs/Design/CHAR/Rigging.md → "Rest pose classification"` |
| viseme estimation | O(S × 31 + I × N log N) for S samples, I frames, N = 512 | `Zenith2/Docs/Design/CHAR/Facial.md → "Estimator input"` … `"Estimator smoothing and normalisation"` |
| key reduction (RDP) | O(K log K) typical, O(K²) worst | `Zenith2/Docs/Design/CHAR/Facial.md → "Estimator smoothing and normalisation"` |
| strided layout | O(L × K) | `Zenith2/Docs/Design/CHAR/Facial.md → "Locale-strided lip-sync clip"` |
| visibility and prefab generation | O(parts × loadouts) | `Zenith2/Docs/Design/CHAR/Modular.md → "Prefab generation algorithm"` |
| authority lint | O(entities × components) | `Zenith2/Docs/Design/CHAR/Authority.md → "Authority lint rules"` |
| template binding and instantiation | O(roles × clips × tokens + nodes) | `Zenith2/Docs/Design/CHAR/Workflow.md → "Template instantiation"` |
| scenario run | O(ticks × the preview evaluation) | `Zenith2/Docs/Design/CHAR/Workflow.md → "Scenario runner"` |
| pose diff | O(J log J) | `Zenith2/Docs/Design/CHAR/Debugging.md → "Pose diff algorithm"` |

### Generated document state machine

The state of each generated prefab document, as the assembly panel shows it (`Zenith2/Docs/Design/CHAR/Modular.md → "Hand-edit protection"`):

| State | Event | Next | Action |
|---|---|---|---|
| ABSENT (no `generated` record) | generate | CURRENT | write the document and record its GUID, path and digest |
| CURRENT | the assembly or a class preset changes | STALE | none (shown) |
| STALE | generate | CURRENT | rewrite, record the new digest |
| CURRENT or STALE | the file's bytes change outside the generator (digest differs) | EDITED | none (shown) |
| EDITED | generate without `force` | EDITED | refuse with `CHAR_ERROR_PREFAB_HAND_EDITED` |
| EDITED | Keep as variant, every edit representable | CURRENT | create the variant with the edits, regenerate with `force` (`Zenith2/Docs/Design/CHAR/Modular.md → "Keep as variant algorithm"`) |
| EDITED | Keep as variant, an edit unrepresentable | EDITED | refuse with `CHAR_ERROR_EDIT_UNREPRESENTABLE`, write nothing |
| EDITED | Discard edits (generate with `force`) | CURRENT | rewrite |
| any | its loadout is deleted, then generate | ABSENT | move the file to the editor trash, drop the record |

### Workflow step state machine

Each step of the workflow panel is NOT_STARTED, IN_PROGRESS (some of its documents exist) or DONE (its condition of `Zenith2/Docs/Design/CHAR/Workflow.md → "Workflow panel"` holds); a step's state is recomputed from documents on every `historyCommitted` and `cookIdle` event, so undo and external edits move it back as readily as forward. A step whose predecessor is not DONE is shown blocked but remains usable (the order is guidance, not a lock).

### Lip-sync entry state machine

| State | Event | Next |
|---|---|---|
| MISSING (no entry) | sync from bank, the bank has the line | ESTIMATE (or SILENT when the bank has no sound for the locale) |
| ESTIMATE | set source Import with a visemes file | IMPORT |
| ESTIMATE or IMPORT | set source Silent | SILENT |
| SILENT | sync from bank, the bank now has a sound | ESTIMATE |
| any | the line leaves the bank, then sync | MISSING (overrides dropped, reported) |
| any | the audio or visemes file changes | the same state; the importer re-runs through its import-input edge |

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| CHAR value types | `OWNER_THREAD` or `IMMUTABLE` as their contracts state |
| CHAR functions | `ANY_THREAD`, pure; the cook runs them on importer threads and worker processes, the editor on its main thread or a background operation |
| `Zenith_CharAssetQuery` | the implementation's (the editor's is `MAIN_THREAD`) |
| the editor extension, panels and commands | `MAIN_THREAD`, editor frame step 2 (`Zenith2/Docs/Design/EDIT/Foundation.md → "Editor frame"`) |
| the pose history ring | `MAIN_THREAD`; its cache consumer callback follows EDIT's cache consumer contract |

### Frame-graph placement

None: CHAR registers no frame-graph node, no system and no extractor in any world. The editor reads play-world state between frames through EDIT's paths.

### Lock-order entries

None: CHAR takes no `Zenith_Mutex`.

### Cancellation and shutdown

A long CHAR operation in the editor (generation, validation of a whole game, template instantiation with compile) runs as an EDIT background operation and honours its cancellation between documents, writing nothing for a cancelled generation (the journaled batch is not committed). The cook's importers follow ASSET's cancellation. The editor extension owns no thread; its pose history rings are freed with their panels and at shutdown before the session.

### Error enumerations

`Zenith_CharError` (function results), `Zenith_CharDiagnosticCode` with `Zenith_CharSeverity` (content findings), EDIT's `Zenith_EditorError` (commands, unchanged), ASSET's `Zenith_ImportError` and `Zenith_CookError` (importers and measures: CHAR maps a `Zenith_CharError` to `IMPORT_ERROR_SOURCE_INVALID`, or `IMPORT_ERROR_OUT_OF_MEMORY` for `CHAR_ERROR_OUT_OF_MEMORY`, with the diagnostic reported through the import context).

### Luau errors and asserts

CHAR exposes nothing to Luau: no binding table, no misuse code (the block ZS2800–ZS2899 is reserved for CHAR by ruling MISUSE-BLOCKS-2, core-doc item X326, with no code in use), no asynchronous source kind. Asserts guard CHAR's own preconditions alone (span lengths computed by CHAR, indices CHAR derived); every document, audio file, snapshot and scenario is untrusted input and fails with an error or a diagnostic.

### Determinism obligations

- Every CHAR function is a pure function of its inputs, so the cook's outputs (lip-sync and pose-book intermediates) are reproducible and the cook's keys are honest (`Zenith2/Docs/Design/ASSET/Cook.md → "Deterministic ordering"`).
- Every baked or tracked output CHAR produces (lip-sync clips, pose books, default ragdolls, pose captures, rig and assembly documents, generated prefabs) is identical bit for bit on every machine running one build, the X314-FMA3 ruling: no CHAR source calls a CRT transcendental ([Z2-N-ARCH-888]: CORE's `Zenith_Sin`, `Zenith_Cos`, `Zenith_Exp`, `Zenith_Ln`, `Zenith_Log10`, `Zenith_Atan2` and `Zenith_Sqrt` only, no `<cmath>` or `<math.h>` include, gated by the CTest `ZenithAuthoring.CharNoCrtTranscendental`), the estimator and the ragdoll generator compute in binary64 in a fixed operation order, and CHAR's sources compile without floating-point contraction ([Z2-N-ARCH-188]); the tests and the cross-host acceptance item are "Estimator input" of `Zenith2/Docs/Design/CHAR/Facial.md`.
- CHAR's generated documents are canonical, so a regeneration with unchanged inputs writes nothing.
- At run time CHAR's data obeys Z2-D-019 through its owners: lip-sync windows are chosen from the voice-locale tag of simulation state, which enters only through a recorded command at a tick boundary, is saved, and is re-issued after a restore whose user setting differs, never from a presentation setting ("Voice locale as a simulation input" of `Zenith2/Docs/Design/CHAR/Facial.md`, the orchestrator's ruling CHAR-R1).

## Diagnostics and profiling

### Counters

Editor counters (DEVT's counter registry in the editor process), each a delta per editor frame unless stated:

| Counter | Kind | Meaning |
|---|---|---|
| `editor.character.validate_runs_delta` | delta | validation catalogue runs |
| `editor.character.diagnostics.errors`, `.warnings` | gauge | the validation panel's current counts |
| `editor.character.prefabs_written_delta`, `.prefabs_refused_delta` | delta | generation results |
| `editor.character.scenarios_passed_delta`, `.scenarios_failed_delta` | delta | scenario runs |
| `editor.character.history_bytes` | gauge | pose history rings |

### Profiler zones

`Char.Validate`, `Char.GeneratePrefabs`, `Char.KeepAsVariant`, `Char.InstantiateTemplate`, `Char.RunScenario` in the editor; `Char.EstimateVisemes`, `Char.LayoutLipSync`, `Char.RoundClipTimes` in the cook's importer threads (ANIM's writer carries ANIM's zone).

### Log categories

`char` (the extension, commands), `char.rig`, `char.lipsync`, `char.assembly`, `char.validate`; the cook-side `char.cook`. A content problem is logged once per (document, rule, index) per validation run.

### Cook report member

The `char.lipsync` importer adds the cook report member `characters.lipSync`: per document, per line, `{ "line", "strideSeconds", "windows", "durations": { tag: seconds }, "keys": { curve: count }, "sources": { tag: "Estimate" | "Import" | "Silent" } }`; `char.posebook` adds `characters.poseBooks` with the pose and channel counts (ASSET's report members, `Zenith2/Docs/Design/ASSET/Cook.md → "Cook report"`).

### Debug views

The pose debugger's overlays (`Zenith2/Docs/Design/CHAR/Debugging.md → "Skeleton overlays"`); the validation panel; ANIM's runtime views are ANIM's.

## Authoring and cooking

### What CHAR authors

| Surface | Documents |
|---|---|
| import wizard | the source `.meta` (through ASSET's settings command) |
| rig profile panel | `.animrig.json` (ANIM's), `.ragdoll.json` (PHYS's) |
| retarget step | `.animretarget.json` (ANIM's) |
| graph templates | `.animgraph.json`, `.animmask.json` (ANIM's), `.posebook.json` (CHAR's) |
| posing panel | `.charcontrols.json`, `.posebook.json` |
| lip-sync panel | `.lipsync.json` |
| assembly panel | `.character.json`, generated `.prefab.json` (SCENE's) |
| project settings | `.charproject.json` (EDIT's JSON document editor) |

### Importer and document table

`Zenith2/Docs/Design/CHAR.md → "Importers and cook registration"` lists the three importers; the lip-sync and pose-book importers produce ANIM clips, cooked by ANIM's `anim.clip` step; CHAR has no cook step of its own.

### Document validation importer

`char.document` claims the assembly, control set and project suffixes and CHAR's four non-document source suffixes (`.visemes.json`, `.posebooknames.json`, `.charscenario.json`, `.posesnapshot.json`, each parsed by its own reader), declares no product, and runs the rules that need no other asset (schema, limits, internal references: MV3 … MV7, MV11, MV12, the control set's schema, the project's CV13, each reader's version and limits), reporting them through the import context; a rule that needs other assets (MV1, MV2, MV8 … MV10, CV5 targets) runs in the editor's validation only. A document with an error fails its import (`IMPORT_ERROR_SOURCE_INVALID`), so `zenith cook` reports a broken assembly without the editor.

### Migration and re-cook

- The five types register tool-side with `Zenith_RegisterCharacterDocumentTypes` (ZenithAuthoring), called by ZenithCook and by the editor, each at version 1 with an empty chain; every newer version adds one step and one fixture under `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/CHAR/<Type>/` ([Z2-N-CONTRACT-530], [Z2-N-CONTRACT-548]); preview, backup, journaled write and unknown-member preservation are SCENE's framework.
- The types join [Z2-N-CONTRACT-552]'s list in the diff that introduces them (core-doc item X328).
- Lip-sync and pose-book clips are cooked assets: they re-cook on any engine version change and whenever an input changes (their import-input edges), never migrate.

## Failure handling

### Failure table

| Failure | Behaviour | Evidence |
|---|---|---|
| a malformed CHAR document | the reader returns `CHAR_ERROR_INVALID_DOCUMENT` with the key path; the editor opens it read-only with the diagnostics; the importer fails the source | `Test_CharDocuments_CanonicalRoundTrip`, the robustness cases |
| an audio source fails to decode | the lip-sync import fails naming the line, the locale and the path | `Test_CharLipSyncImporter_MissingAudioFails` |
| a skeleton whose profile is incomplete | detection reports the unassigned roles; retarget creation refuses with the joints listed | `Test_CharRetarget_IncompleteProfileListsJoints` |
| a character that faces backwards or is mirrored | reported with the corrective setting; nothing is re-posed | `Test_CharRestPose_DetectsBackwardsFacing`, `Test_CharRestPose_DetectsMirrored` |
| a hand-edited generated prefab | generation refuses without `force`; Keep as variant moves representable edits and refuses the rest | `Test_CharAssembly_HandEditRefusedWithoutForce`, `Test_CharAssembly_KeepAsVariantPreservesEdit` |
| a template that cannot bind a required role | instantiation fails naming the role and the candidates | `Test_CharTemplateBinding_RequiredMissingFails` |
| a lip-sync line longer than ANIM's clip limit across its locales | that line fails with `CHAR_ERROR_LIMIT_EXCEEDED`; the other lines import | `Test_CharLipSyncClip_DurationLimitRefused` |
| a pending dependency missing (GEOM's rigid attach, ANIM's slot mask) | the dependent feature is refused with its diagnostic; nothing else is affected | `Test_CharAssemblyValidation_EachRule` (MV8, the rigid-unavailable case) |
| allocation failure | `CHAR_ERROR_OUT_OF_MEMORY` up to the caller; a generation batch is not committed | the failure-injection cases of each test file |
| memory pressure in the editor | the pose history rings shed half | `Test_CharPoseHistory_ShedsHalfUnderPressure` |
| device loss, headless runs | previews are unavailable (`EDITOR_ERROR_PREVIEW_UNAVAILABLE`); scenarios, validation and generation do not need a renderer | `Test_Walkthrough_CharacterWorkflow_Rehearsal` (headless) |
| a walkthrough breach (`Z2-B-CHAR-001`, BR-ACCEPT) | a BUG on the CHAR epic blocks the Stage 4 acceptance | the MP-WALKTHROUGH evidence record |

## Test strategy

### Test targets and gate steps

`Zenith2/Docs/Design/CHAR/Tests.md → "Test targets"`: `ZenithAuthoring_Tests`, `ZenithCook_Tests` and `ZenithEditor_Tests` run CHAR's cases in the `Test` step; `Zenith_Integration_Character` runs in the `Test` step and in stage acceptance; the readers' mutated-corpus robustness cases run in `ZenithAuthoring_Tests` (CHAR has no fuzz target: its tools are absent from `win-asan`); `Libgraph`, `ApiContractCheck`, `HeaderSelfContained`, `ForbiddenCheck` and `Complexity` cover the code. No CHAR test needs a GPU.

### Test catalogue index

| Group | Heading in `Zenith2/Docs/Design/CHAR/Tests.md` |
|---|---|
| rigs, controls, pose books | "Rigging tests" |
| faces and lip-sync | "Facial and lip-sync tests" |
| modular and hair | "Modular and hair tests" |
| authority | "Authority tests", "Integration tests" |
| pose debugging, budgeting, validation | "Debugging budgeting and validation tests" |
| editor tools and the walkthrough rehearsal | "Editor and workflow tests" |
| documents and importers | "Document and registration tests" |
| robustness, measurement, generators | "Fuzz targets", "Benchmarks", "Fixtures and generators" |
| member types | `Zenith2/Docs/Design/CHAR/Types.md` (declarations only; their tests are their functions') |

### Fixtures

Every source is generated at test time from declared constants (`Zenith2/Docs/Design/CHAR/Tests.md → "Fixtures and generators"`); the fixture sets are `CHAR_IntegrationFixtures` and `CharWalkthroughSources`; migration fixtures are JSON; no binary is committed (Q55).

### Acceptance runs

| Run | Stage | Evidence |
|---|---|---|
| the D-SEL-MOTIONMATCH ruling | S3 | the ruling ticket's reports and DecisionLog entry |
| the character suite on both backends | S3, S4 | stage acceptance |
| the CinematicSlice walkthrough starting content | S4 | CINE's `needs-human` content-production ticket (row CH19), blocking the timed run |
| the timed character workflow walkthrough on CinematicSlice | S4 | MP-WALKTHROUGH against `Z2-B-CHAR-001` (`needs-human`) |
| cross-host determinism of CHAR's baked outputs | S4 | the two hosts' `determinism-digests.json` files of one build compared equal (`needs-human`; `Zenith2/Docs/Design/CHAR/Facial.md → "Estimator input"`) |
| W-Characters with class presets and the modular variant | S4 | ANIM's Stage 4 budget ruling runs and the pending modular row (`needs-human`) |
| CinematicSlice sign-off of faces, lip-sync and hair cards | S4, S5 | the slice's `SignOff.md` items ([Z2-N-BUDGET-435]) |

## Budget rows

### Rows owned

| Row | Name | Status | CHAR's part |
|---|---|---|---|
| `Z2-B-CHAR-001` | Walkthrough complete character workflow | pending (Stage 1 per-system budget ticket for CHAR) | the MP-WALKTHROUGH time of "Walkthrough character workflow" on the reference desktop, run on **CinematicSlice** (an MP-WALKTHROUGH run, not a W-workload run; the row's Workloads cell reads `W-Editor` today and core-doc item X340 (1) corrects it to CinematicSlice) |

The Stage 1 per-system budget ticket for CHAR has no harness half ([Z2-N-BUDGET-022]'s non-harness row: its ruling cites reference-engine or published data); it is blocked by the CORE, JOBS and RPROD rulings ([Z2-N-BUDGET-023]) and also commits the walkthrough step list ([Z2-N-BUDGET-326]).

### Rows measured against

| Row | Why |
|---|---|
| `Z2-B-ANIM-001`, `Z2-B-ANIM-002`, `Z2-B-ANIM-003` | the D-SEL-MOTIONMATCH acceptance rule; class presets in W-Characters |
| `Z2-B-RPROD-003` Render extraction CPU | modular followers' extraction runs inside its span |
| `Z2-B-GEOM-004` Compute skinning and morph targets GPU | parts and morph-driven faces |
| `Z2-B-GAME-001` Movement arbitration phase CPU | the authority suite's phase-4 work |

### Pending row needs

Recorded for the Budgets writer in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` (row CH9) and queued as core-doc item X330:

| Proposed row | Unit | Procedure | Workloads | Bound or exclusion | Producer |
|---|---|---|---|---|---|
| Lip-sync import time per audio minute | s | MP-EDIT over `char.lipsync` imports | W-Editor | BR-ACCEPT | Stage 1 per-system budget ticket for CHAR |
| Modular part extraction overhead | ms | MP-CPUZONE (`Animation.ExtractPoses`) in the W-Characters modular variant | W-Characters | inside `Z2-B-RPROD-003`'s span; reported, excluded from bounds | Stage 1 per-system budget ticket for CHAR |

## Stage plan

### Stage table

| Stage | CHAR deliverables | Gated by |
|---|---|---|
| S1 | the Stage 1 per-system budget ticket for CHAR (`Z2-B-CHAR-001`, the walkthrough step list) | the CORE, JOBS and RPROD rulings |
| S2 | none of its own | — |
| S3 | the API-only slice of the authority types; the authority lint (AU1 … AU8); the character integration suite and its fixtures (the root-only prefab, "Integration fixtures" of `Zenith2/Docs/Design/CHAR/Authority.md`); the motion-matching authoring-cost harness, which uses only ANIM's Stage 3 motion-matching header, pose operations and a harness-local evaluator, never node kind 15 or CHAR's Stage 4 templates; the D-SEL-MOTIONMATCH ruling ticket | EDIT's ZenithAuthoring target ticket (Q84 (a)); ANIM's Stage 3 module, components and ragdoll transitions; PHYS's controller and ragdoll (rows AN6, CH8); ANIM's motion-matching harness `Z2-R-ANIM-178` with `BuildFromFrames` (row CH15); D-PHASES (open, Stage 1) |
| S4 | everything else: rigs, controls, pose books, faces, lip-sync, modular characters, hair-card rules, pose debugging, budgeting tools, the workflow tools, templates, scenarios, the walkthrough and its rehearsal, documents and importers, the cinematic authority case; the CinematicSlice walkthrough starting content, a `needs-human` content-production ticket owned by **CINE** (CinematicSlice's owner; row CH19), which blocks the timed walkthrough | ANIM's Stage 4 facial, retarget and editor contracts (rows CH1, AN11) and its writer, rate function, measures, marker lookup and full-rate slot tag (rows CH15, CH16); CORE's deterministic functions (row CH21); EDIT's Stage 4 panels framework and automation (rows CH5, CH14); CONTENT's importers and decoders; AUDIO's banks (row CH2); CINE (rows CH3, CH19); GAME's voice-locale command (rows CH4, CH18); GEOM's rigid attach and measure (rows CH6, CH17); PHYS's Euler convention (row CH20); D-SEL-MOTIONMATCH for the motion-matching template; D-SEL-FBX (open, Stage 4) for FBX in the wizard |
| S5 | rule HC3 (hair-card anisotropy) becomes active with GEOM's anisotropy model | GEOM's `Z2-R-GEOM-108` |
| S6 | retained migration fixtures of every shipped CHAR document version | shipping qualification |
| S7 | none | — |

### API-only slices

1. `Zenith_AuthoringCharacter.h` with the Stage 3 includes of "Constants and enumerations" and the error, diagnostic and authority types (`Zenith_CharEntityRecord`, `Zenith_CharComponentRef`, `Zenith_CharAssetQuery`, `Zenith_CharLintCharacterAuthority`) and stub definitions, in ZenithAuthoring, at Stage 3, before the lint's implementation; its diff appends `ZenithScene` and `ZenithDocument` to ZenithAuthoring's `DEPENDS`, and nothing in it needs ZenithAnimation or ZenithContentCore.
2. The rest of the header (rig, controls, lip-sync, assembly, templates, validation, pose debugging and the member types of `Zenith2/Docs/Design/CHAR/Types.md`) at Stage 4, with the Stage 4 includes, before the editor tickets that call it.

### Ticket slices and ordering

1. Stage 3: the API slice; the lint; the fixture setup case with the root-only prefab and the six authority scenarios (blocked by ANIM's and PHYS's systems); the harness (blocked by ANIM's harness ticket); the ruling (blocked by the harness and ANIM's harness).
2. Stage 4 data first: document registration and the importers' frame; rig profiles, matching, generic, topology, mirror, chains, rest pose; rig document generation and the facial template (after ANIM's face roles); ragdoll; retarget.
3. Controls and pose books; the clip time rounding; the pose book importer (after ANIM's writer, row CH15).
4. The viseme set and project document; the estimator (FFT, features, classification, smoothing); external import; overrides; layout; the lip-sync importer; sync from bank.
5. Assemblies: reader, visibility, validation, generation, hand-edit protection, rigid attachments (after GEOM's setting); hair rules.
6. Templates, binding, instantiation (after ANIM's slot mask), scenarios and the runner.
7. The editor extension and commands; the import wizard; the workflow, rig profile, assembly, posing, lip-sync, pose debugger, validation and budget panels; the play driver.
8. The validation catalogue; the class-label content-rule test (after ANIM's measures, row CH15); the integration cases of Stage 4 and the generator path of the fixture setup case; the walkthrough fixtures and rehearsal; last, the `needs-human` timed walkthrough acceptance item, blocked by CINE's content-production ticket.

## Decisions

### Settled decisions

| ID | Governs |
|---|---|
| Z2-D-078, Z2-D-043 | hair and groom excluded; hair alternatives classified here |
| Z2-D-079 | animation gets visual authoring; CHAR's tools are visual tools over ANIM's documents |
| Z2-D-080 | the phase and authority contract the matrix follows |
| Z2-D-019 | determinism, including lip-sync windows chosen from simulation state |
| Z2-D-084, Z2-D-097 | the motion-matching harness and ruling split; rulings appended to the DecisionLog in the ruling ticket's diff |
| Z2-D-038, Z2-D-048 | JSON documents, migration with preview, backup and unknown-member preservation; cooked clips re-cook |
| Z2-D-024, Z2-D-081 | no plugins; CHAR adds nothing to a Player |
| Z2-D-077 | equipment, dialogue and settings logic is game-side |
| Z2-D-012, Z2-D-021 | the walkthrough row and the flagship slices CHAR is measured in |
| Z2-D-034, Z2-D-045, Z2-D-046, Z2-D-047 | naming, errors, no globals, contract blocks |

### Open decisions

| Decision | Effect on CHAR |
|---|---|
| D-SEL-MOTIONMATCH (open, Stage 3) | CHAR's ruling item; it decides whether the motion-matching template ships and whether ANIM's node kind 15 is implemented |
| D-CLASS-SIM (open, Stage 2) | cloth on characters and simulated hair-card motion; CHAR's hair and modular rules gain rows in its revalidation |
| D-SEL-FBX (open, Stage 4) | whether the import wizard accepts FBX |
| D-PHASES (open, Stage 1) | confirms the field authority the matrix states |
| D-SEL-JSON (open, Stage 1), D-SEL-HASH (open, Stage 1) | the readers behind CHAR's documents; the name hashes of markers and curves (through ANIM) |

### Design rulings owned by CHAR

| Ruling | Choice | Alternatives rejected | Reason |
|---|---|---|---|
| CH-R1 no runtime surface | CHAR is tool-side; runtime behaviour belongs to ANIM, PHYS, GEOM, AUDIO and CINE | a `ZenithCharacter` module with lip-sync and equipment systems | every mechanism exists in an owner; a module would duplicate ANIM's slots and followers and add an Architecture library for no capability |
| CH-R2 parts as followers | one instance per part, no mesh merge | cook-time mesh merging | merging saves no skinning, bakes a loadout into a mesh and would be GEOM's format work |
| CH-R3 region parts replace hiding | garments replace whole region meshes | per-instance triangle hiding | no owner has per-instance sub-mesh hiding; visibility is an existing property |
| CH-R4 generated prefabs | the assembly generates SCENE prefab documents with derived IDs; hand edits go to variants | a runtime assembly component; editing generated files | prefabs are the engine's instancing; derived IDs keep scene overrides valid |
| CH-R5 offline, locale-strided lip-sync | one clip per line, one window per voice locale, chosen by simulation state | runtime audio-driven lip-sync; one clip per (line, locale) | viseme curves are simulation state; runtime analysis would read presentation; one GUID per line fits CINE's face clip and AUDIO's line |
| CH-R6 in-house estimator plus external import | a deterministic band-energy estimator, external timing files and manual overrides | a third-party phoneme aligner in the cook | no new dependency or selection decision; external tools stay outside the engine |
| CH-R7 voice locale as simulation input | the game owns the voice-locale tag as state, changed only by a recorded command at a tick boundary, saved, re-issued at the first tick after a restore whose user setting differs (the orchestrator's ruling CHAR-R1); lip-sync clips resolve the tag through their own markers | reading AUDIO's presentation locale; an index into a locale list duplicated at run time | replays and saves stay exact, and no run-time code needs the project document |
| CH-R8 hair cards required, shells excluded | Z2-D-078 amendment (X327) | shells required; cards excluded | cards need only existing features and the slice names them; shells multiply alpha-tested overdraw |
| CH-R9 character classes as labels | class presets in the project document; content rules by `.meta` label | per-asset limits | ContentSpecs' classes and ANIM's per-animator controls both need one class source |
| CH-R10 tools edit documents only | every rig and assembly tool writes documents; none writes a mesh or a pose into a mesh | rebinding or re-posing meshes | the shared-rig lessons; ANIM's one-rest-pose rule |
| CH-R11 walkthrough rehearsal | a headless automation rehearsal in the `Test` step plus the human-timed run | the human run alone | the human run measures usability, the rehearsal proves every step exists (EDIT's rule) |

### Classification and selection decisions CHAR owns

**D-SEL-MOTIONMATCH (open, Stage 3), Z2-I-CHAR-09.** The question, alternatives and acceptance rule are `Zenith2/Docs/DecisionLog.md → "D-SEL-MOTIONMATCH"` (motion matching in scope, or blend trees and state machines only; CinematicSlice character-quality needs, CPU and memory for W-Characters on the S24 against `Z2-B-ANIM-001` and `Z2-B-ANIM-002`, and the authoring cost of the animation database). The split with ANIM, by the plan's Appendix A ("decisions on motion matching (D-SEL-MOTIONMATCH)" in CHAR's row) and ANIM's design ("the ruling is CHAR's item `Z2-I-CHAR-09`"): ANIM owns the runtime harness (`Z2-R-ANIM-178`, `Zenith2/Benchmarks/MotionMatching/`) and the node (`Z2-R-ANIM-179`); CHAR owns the authoring-cost harness (`Zenith2/Docs/Design/CHAR/Workflow.md → "Motion-matching authoring-cost harness"`) and the `needs-human` ruling ticket, which reads both reports, commits them under `Zenith2/Docs/Research/MotionMatching/`, and appends the ruling with the next free `Z2-D-NNN` (Z2-D-097). A ruling of "required" unblocks ANIM's node tickets and CHAR's motion-matching template; "excluded" reclassifies both requirements through `Zenith2/Docs/Requirements/Inventory.md → "Change rule"` in the ruling's diff.

**Hair alternatives, Z2-I-CHAR-10.** Hair cards required, shells excluded, strand hair excluded (`Zenith2/Docs/Design/CHAR/Modular.md → "Hair alternatives classification"`), recorded as a dated amendment of Z2-D-078 by core-doc item X327 (the decision ID the classification carries); the human's A4 sign-off of scope cuts confirms it.

## Requirement index

Every requirement of `Zenith2/Docs/Requirements/CHAR.md` and the headings that specify it:

| Requirement | Title | Stage | Design headings |
|---|---|---|---|
| Z2-R-CHAR-001 | Tool-side code placement | S3 | `Zenith2/Docs/Design/CHAR.md` → "Targets"; `Zenith2/Docs/Design/CHAR.md` → "ZenithAuthoring CHAR files"; `Zenith2/Docs/Design/CHAR.md` → "ZenithCook CHAR files"; `Zenith2/Docs/Design/CHAR.md` → "ZenithEditor CHAR files" |
| Z2-R-CHAR-002 | No runtime surface | S3 | `Zenith2/Docs/Design/CHAR.md` → "Split with ANIM"; `Zenith2/Docs/Design/CHAR.md` → "Design rulings owned by CHAR"; `Zenith2/Docs/Design/CHAR.md` → "Luau errors and asserts" |
| Z2-R-CHAR-003 | Public header contracts | S3 | `Zenith2/Docs/Design/CHAR.md` → "API rules"; `Zenith2/Docs/Design/CHAR.md` → "Header map" |
| Z2-R-CHAR-004 | Error and diagnostic enumerations | S3 | `Zenith2/Docs/Design/CHAR.md` → "Errors and diagnostics"; `Zenith2/Docs/Design/CHAR.md` → "Error enumerations" |
| Z2-R-CHAR-005 | API-only slice first | S3 | `Zenith2/Docs/Design/CHAR.md` → "API-only slices" |
| Z2-R-CHAR-006 | Character authority matrix | S3 | `Zenith2/Docs/Design/CHAR/Authority.md` → "Fields in the matrix"; `Zenith2/Docs/Design/CHAR/Authority.md` → "Character authority matrix"; `Zenith2/Docs/Design/CHAR/Authority.md` → "State transitions and their tick" |
| Z2-R-CHAR-007 | Authority lint rules AU1 to AU8 | S3 | `Zenith2/Docs/Design/CHAR/Authority.md` → "Authority lint inputs"; `Zenith2/Docs/Design/CHAR/Authority.md` → "Authority lint rules" |
| Z2-R-CHAR-008 | Character integration suite | S3 | `Zenith2/Docs/Design/CHAR/Authority.md` → "Character integration suite"; `Zenith2/Docs/Design/CHAR.md` → "Integration suite and benchmark files" |
| Z2-R-CHAR-009 | Root motion through the controller | S3 | `Zenith2/Docs/Design/CHAR/Authority.md` → "Character authority matrix"; `Zenith2/Docs/Design/CHAR/Authority.md` → "Character integration suite" |
| Z2-R-CHAR-010 | Root motion without a controller | S3 | `Zenith2/Docs/Design/CHAR/Authority.md` → "Character authority matrix"; `Zenith2/Docs/Design/CHAR/Authority.md` → "Character integration suite" |
| Z2-R-CHAR-011 | Velocity mode ignores root motion | S3 | `Zenith2/Docs/Design/CHAR/Authority.md` → "Character authority matrix"; `Zenith2/Docs/Design/CHAR/Authority.md` → "Authority lint rules" |
| Z2-R-CHAR-012 | Ragdoll ownership sequence | S3 | `Zenith2/Docs/Design/CHAR/Authority.md` → "Character authority matrix"; `Zenith2/Docs/Design/CHAR/Authority.md` → "State transitions and their tick"; `Zenith2/Docs/Design/CHAR/Authority.md` → "Character integration suite" |
| Z2-R-CHAR-013 | Teleport during locomotion and ragdoll | S3 | `Zenith2/Docs/Design/CHAR/Authority.md` → "Character authority matrix"; `Zenith2/Docs/Design/CHAR/Authority.md` → "Character integration suite" |
| Z2-R-CHAR-014 | Authority determinism | S3 | `Zenith2/Docs/Design/CHAR/Authority.md` → "Character integration suite" |
| Z2-R-CHAR-015 | Integration fixtures generated at test time | S3 | `Zenith2/Docs/Design/CHAR/Authority.md` → "Integration fixtures"; `Zenith2/Docs/Design/CHAR/Tests.md` → "Fixtures and generators" |
| Z2-R-CHAR-016 | Cinematic hold of a character | S4 | `Zenith2/Docs/Design/CHAR/Authority.md` → "Character authority matrix"; `Zenith2/Docs/Design/CHAR/Authority.md` → "Character integration suite" |
| Z2-R-CHAR-017 | Fixture prefab equals the generator | S4 | `Zenith2/Docs/Design/CHAR/Authority.md` → "Integration fixtures"; `Zenith2/Docs/Design/CHAR/Modular.md` → "Generated prefab layout" |
| Z2-R-CHAR-018 | Flagship characters lint clean | S4 | `Zenith2/Docs/Design/CHAR/Authority.md` → "Authority lint rules" |
| Z2-R-CHAR-019 | Motion-matching authoring-cost harness | S3 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Motion-matching authoring-cost harness"; `Zenith2/Docs/Design/CHAR.md` → "Classification and selection decisions CHAR owns" |
| Z2-R-CHAR-020 | D-SEL-MOTIONMATCH ruled | S3 | `Zenith2/Docs/Design/CHAR.md` → "Classification and selection decisions CHAR owns"; `Zenith2/Docs/Design/CHAR.md` → "Open decisions" |
| Z2-R-CHAR-021 | Motion-matching template follows the ruling | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Graph template catalogue"; `Zenith2/Docs/Design/CHAR.md` → "Classification and selection decisions CHAR owns" |
| Z2-R-CHAR-022 | Walkthrough row allocated | S1 | `Zenith2/Docs/Design/CHAR.md` → "Rows owned"; `Zenith2/Docs/Design/CHAR/Workflow.md` → "Walkthrough character workflow" |
| Z2-R-CHAR-023 | Rig profile table | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` → "Rig profile table" |
| Z2-R-CHAR-024 | Joint name normalisation | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` → "Joint name normalisation" |
| Z2-R-CHAR-025 | Profile matching and detection | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` → "Profile matching algorithm" |
| Z2-R-CHAR-026 | Generic profile assignment | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` → "Generic profile assignment" |
| Z2-R-CHAR-027 | Topology verification | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` → "Topology verification" |
| Z2-R-CHAR-028 | Mirror pair detection | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` → "Mirror pair detection" |
| Z2-R-CHAR-029 | Chains and IK definitions | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` → "Chain and IK template detection" |
| Z2-R-CHAR-030 | Rest pose classification | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` → "Rest pose classification" |
| Z2-R-CHAR-031 | Rig document generation | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` → "Rig document generation" |
| Z2-R-CHAR-032 | Generated rig cooks humanoid complete | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` → "Rig document generation" |
| Z2-R-CHAR-033 | Facial rig template | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` → "Facial rig template"; `Zenith2/Docs/Design/CHAR/Facial.md` → "Viseme set" |
| Z2-R-CHAR-034 | Default ragdoll generation | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` → "Default ragdoll generation" |
| Z2-R-CHAR-035 | Shared library rig rule | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` → "Shared library rig rule" |
| Z2-R-CHAR-036 | Retarget document creation | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` → "Retarget setup"; `Zenith2/Docs/Design/CHAR/Workflow.md` → "Retarget step" |
| Z2-R-CHAR-037 | Retarget rest-to-rest verification | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` → "Retarget setup" |
| Z2-R-CHAR-038 | Rigging limits | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` → "Rigging limits" |
| Z2-R-CHAR-039 | Control set document | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` → "Control set document" |
| Z2-R-CHAR-040 | Control evaluation | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` → "Control evaluation" |
| Z2-R-CHAR-041 | Control poses match the runtime | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` → "Control evaluation" |
| Z2-R-CHAR-042 | Pose book document | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` → "Pose book document" |
| Z2-R-CHAR-043 | Pose book cook | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` → "Pose book cook" |
| Z2-R-CHAR-044 | Pose book name validation | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` → "Pose book name validation" |
| Z2-R-CHAR-045 | Pose capture and mirroring | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` → "Pose capture and mirroring" |
| Z2-R-CHAR-046 | Posing panel | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Posing panel" |
| Z2-R-CHAR-047 | Viseme set | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Viseme set" |
| Z2-R-CHAR-048 | Face graph template | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Face graph template" |
| Z2-R-CHAR-049 | Automatic viseme pose books | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Automatic viseme pose books" |
| Z2-R-CHAR-050 | Character project settings document | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Character project settings document" |
| Z2-R-CHAR-051 | Lip-sync document | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Lip-sync document" |
| Z2-R-CHAR-052 | Sync from bank | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Sync from bank" |
| Z2-R-CHAR-053 | Estimator input conditioning | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Estimator input" |
| Z2-R-CHAR-054 | Estimator framing and spectrum | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Estimator framing and spectrum" |
| Z2-R-CHAR-055 | Estimator features | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Estimator features" |
| Z2-R-CHAR-056 | Estimator classification | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Estimator classification" |
| Z2-R-CHAR-057 | Estimator smoothing and weights | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Estimator smoothing and normalisation" |
| Z2-R-CHAR-058 | Key reduction | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Estimator smoothing and normalisation" |
| Z2-R-CHAR-059 | Phrase markers | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Phrase markers" |
| Z2-R-CHAR-060 | Estimator determinism | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Estimator input"; `Zenith2/Docs/Design/CHAR.md` → "Determinism obligations" |
| Z2-R-CHAR-061 | External viseme import | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "External viseme import" |
| Z2-R-CHAR-062 | Manual overrides | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Manual overrides" |
| Z2-R-CHAR-063 | Locale-strided lip-sync clip | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Locale-strided lip-sync clip" |
| Z2-R-CHAR-064 | Window markers and end events | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Window markers and events" |
| Z2-R-CHAR-065 | Clip intermediate writer | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Clip intermediate writer" |
| Z2-R-CHAR-066 | Lip-sync importer | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Lip-sync importer" |
| Z2-R-CHAR-067 | Lip-sync reimport identity | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Lip-sync importer" |
| Z2-R-CHAR-068 | Lip-sync playback contract | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Lip-sync playback contract" |
| Z2-R-CHAR-069 | Voice locale as a simulation input | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Voice locale as a simulation input" |
| Z2-R-CHAR-070 | Lip-sync panel | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Lip-sync panel" |
| Z2-R-CHAR-071 | Facial and lip-sync limits | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Facial and lip-sync limits" |
| Z2-R-CHAR-072 | Assembly document | S4 | `Zenith2/Docs/Design/CHAR/Modular.md` → "Assembly document"; `Zenith2/Docs/Design/CHAR/Modular.md` → "Assembly model" |
| Z2-R-CHAR-073 | Body region visibility | S4 | `Zenith2/Docs/Design/CHAR/Modular.md` → "Body regions" |
| Z2-R-CHAR-074 | Slots and part caps | S4 | `Zenith2/Docs/Design/CHAR/Modular.md` → "Slots and loadouts"; `Zenith2/Docs/Design/CHAR/Modular.md` → "Part caps and cost" |
| Z2-R-CHAR-075 | Generated prefab layout | S4 | `Zenith2/Docs/Design/CHAR/Modular.md` → "Generated prefab layout" |
| Z2-R-CHAR-076 | Derived entity identity | S4 | `Zenith2/Docs/Design/CHAR/Modular.md` → "Generated entity identity" |
| Z2-R-CHAR-077 | Loadout variants | S4 | `Zenith2/Docs/Design/CHAR/Modular.md` → "Loadout variants" |
| Z2-R-CHAR-078 | Prefab generation | S4 | `Zenith2/Docs/Design/CHAR/Modular.md` → "Prefab generation algorithm" |
| Z2-R-CHAR-079 | Hand-edit protection | S4 | `Zenith2/Docs/Design/CHAR/Modular.md` → "Hand-edit protection"; `Zenith2/Docs/Design/CHAR/Modular.md` → "Prefab generation algorithm" |
| Z2-R-CHAR-080 | Assembly validation | S4 | `Zenith2/Docs/Design/CHAR/Modular.md` → "Assembly validation rules" |
| Z2-R-CHAR-081 | Rigid attachments | S4 | `Zenith2/Docs/Design/CHAR/Modular.md` → "Rigid attachments" |
| Z2-R-CHAR-082 | Modular part cost | S4 | `Zenith2/Docs/Design/CHAR/Modular.md` → "Part caps and cost"; `Zenith2/Docs/Design/CHAR.md` → "Pending row needs" |
| Z2-R-CHAR-083 | Runtime part switching | S4 | `Zenith2/Docs/Design/CHAR/Modular.md` → "Runtime part switching" |
| Z2-R-CHAR-084 | Assembly panel | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Assembly panel" |
| Z2-R-CHAR-085 | Hair alternatives classified | S4 | `Zenith2/Docs/Design/CHAR/Modular.md` → "Hair alternatives classification" |
| Z2-R-CHAR-086 | Hair shells excluded | S4 | `Zenith2/Docs/Design/CHAR/Modular.md` → "Hair alternatives classification" |
| Z2-R-CHAR-087 | Hair card authoring rules | S4 | `Zenith2/Docs/Design/CHAR/Modular.md` → "Hair card authoring rules" |
| Z2-R-CHAR-088 | Hair card anisotropic highlight | S5 | `Zenith2/Docs/Design/CHAR/Modular.md` → "Hair card authoring rules" |
| Z2-R-CHAR-089 | Hair card secondary motion | S4 | `Zenith2/Docs/Design/CHAR/Modular.md` → "Hair alternatives classification" |
| Z2-R-CHAR-090 | Pose debugger reads without writing | S4 | `Zenith2/Docs/Design/CHAR/Debugging.md` → "Pose debugger panel"; `Zenith2/Docs/Design/CHAR/Debugging.md` → "Pose sources" |
| Z2-R-CHAR-091 | Joint table | S4 | `Zenith2/Docs/Design/CHAR/Debugging.md` → "Joint table" |
| Z2-R-CHAR-092 | Skeleton overlays | S4 | `Zenith2/Docs/Design/CHAR/Debugging.md` → "Skeleton overlays" |
| Z2-R-CHAR-093 | Pose snapshot format | S4 | `Zenith2/Docs/Design/CHAR/Debugging.md` → "Pose snapshot format" |
| Z2-R-CHAR-094 | Pose diff | S4 | `Zenith2/Docs/Design/CHAR/Debugging.md` → "Pose diff algorithm" |
| Z2-R-CHAR-095 | Pose history recorder | S4 | `Zenith2/Docs/Design/CHAR/Debugging.md` → "Pose history recorder" |
| Z2-R-CHAR-096 | Pose assertions in automation | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Automation steps"; `Zenith2/Docs/Design/CHAR/Debugging.md` → "Pose diff algorithm" |
| Z2-R-CHAR-097 | Character class presets | S4 | `Zenith2/Docs/Design/CHAR/Debugging.md` → "Character class presets"; `Zenith2/Docs/Design/CHAR/Modular.md` → "Generated prefab layout" |
| Z2-R-CHAR-098 | Budget panel | S4 | `Zenith2/Docs/Design/CHAR/Debugging.md` → "Budget panel" |
| Z2-R-CHAR-099 | Class rate lint | S4 | `Zenith2/Docs/Design/CHAR/Debugging.md` → "Class rate lint" |
| Z2-R-CHAR-100 | Content rule measures | S4 | `Zenith2/Docs/Design/CHAR/Debugging.md` → "Content rule measures" |
| Z2-R-CHAR-101 | Budgeting measured in W-Characters | S4 | `Zenith2/Docs/Design/CHAR/Debugging.md` → "Character class presets"; `Zenith2/Docs/Design/CHAR.md` → "Rows measured against" |
| Z2-R-CHAR-102 | Editor extension | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Editor extension" |
| Z2-R-CHAR-103 | Character commands | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Character commands"; `Zenith2/Docs/Design/CHAR/Workflow.md` → "Command argument forms" |
| Z2-R-CHAR-104 | Automation steps and events | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Automation steps"; `Zenith2/Docs/Design/CHAR/Workflow.md` → "Editor events" |
| Z2-R-CHAR-105 | Character import wizard | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Character import wizard" |
| Z2-R-CHAR-106 | Import facing and unit report | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Character import wizard"; `Zenith2/Docs/Design/CHAR/Rigging.md` → "Rest pose classification" |
| Z2-R-CHAR-107 | Workflow panel | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Workflow panel"; `Zenith2/Docs/Design/CHAR/Workflow.md` → "Workflow overview" |
| Z2-R-CHAR-108 | Rig profile panel | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Rig profile panel" |
| Z2-R-CHAR-109 | Retarget step | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Retarget step" |
| Z2-R-CHAR-110 | Graph template catalogue | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Graph template catalogue" |
| Z2-R-CHAR-111 | Template clip binding | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Template clip binding" |
| Z2-R-CHAR-112 | Template instantiation | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Template instantiation" |
| Z2-R-CHAR-113 | Template slots for cinematics and get-up | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Graph template catalogue" |
| Z2-R-CHAR-114 | Preview scenarios | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Preview scenarios" |
| Z2-R-CHAR-115 | Scenario runner | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Scenario runner" |
| Z2-R-CHAR-116 | In-game step | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "In-game step" |
| Z2-R-CHAR-117 | Validation catalogue | S4 | `Zenith2/Docs/Design/CHAR/Debugging.md` → "Validation catalogue" |
| Z2-R-CHAR-118 | Validation panel and command | S4 | `Zenith2/Docs/Design/CHAR/Debugging.md` → "Validation panel" |
| Z2-R-CHAR-119 | Walkthrough step list | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Walkthrough character workflow" |
| Z2-R-CHAR-120 | Walkthrough rehearsal | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Walkthrough rehearsal"; `Zenith2/Docs/Design/CHAR/Workflow.md` → "Command argument forms" |
| Z2-R-CHAR-121 | Walkthrough fixtures | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Walkthrough fixtures" |
| Z2-R-CHAR-122 | Timed walkthrough run | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Walkthrough character workflow"; `Zenith2/Docs/Design/CHAR.md` → "Acceptance runs" |
| Z2-R-CHAR-123 | Document types registered tool-side | S4 | `Zenith2/Docs/Design/CHAR.md` → "Document registration API"; `Zenith2/Docs/Design/CHAR.md` → "Document type table" |
| Z2-R-CHAR-124 | Canonical documents | S4 | `Zenith2/Docs/Design/CHAR.md` → "Migration and re-cook" |
| Z2-R-CHAR-125 | Document migration fixtures | S4 | `Zenith2/Docs/Design/CHAR.md` → "Migration and re-cook" |
| Z2-R-CHAR-126 | Document type list | S4 | `Zenith2/Docs/Design/CHAR.md` → "Migration and re-cook"; `Zenith2/Docs/Design/CHAR.md` → "Reconciliation items" |
| Z2-R-CHAR-127 | Importer registration | S4 | `Zenith2/Docs/Design/CHAR.md` → "Importers and cook registration" |
| Z2-R-CHAR-128 | Document validation importer | S4 | `Zenith2/Docs/Design/CHAR.md` → "Document validation importer" |
| Z2-R-CHAR-129 | Reader robustness | S4 | `Zenith2/Docs/Design/CHAR/Tests.md` → "Fuzz targets"; `Zenith2/Docs/Design/CHAR.md` → "Character document limits" |
| Z2-R-CHAR-130 | Parser limits recorded | S4 | `Zenith2/Docs/Design/CHAR.md` → "Character document limits" |
| Z2-R-CHAR-131 | Diagnostics and counters | S4 | `Zenith2/Docs/Design/CHAR.md` → "Counters"; `Zenith2/Docs/Design/CHAR.md` → "Log categories"; `Zenith2/Docs/Design/CHAR.md` → "Cook report member" |
| Z2-R-CHAR-132 | Shipped document migration fixtures | S6 | `Zenith2/Docs/Design/CHAR.md` → "Migration and re-cook" |
| Z2-R-CHAR-133 | No CRT transcendental in CHAR code | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Estimator input"; `Zenith2/Docs/Design/CHAR.md` → "Determinism obligations" |
| Z2-R-CHAR-134 | Cross-host determinism of baked outputs | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Estimator input"; `Zenith2/Docs/Design/CHAR.md` → "Acceptance runs" |
| Z2-R-CHAR-135 | Estimator key-time domain | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Estimator framing and spectrum"; `Zenith2/Docs/Design/CHAR/Facial.md` → "Estimator smoothing and normalisation" |
| Z2-R-CHAR-136 | Voice locale after a save restore | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Voice locale as a simulation input" |
| Z2-R-CHAR-137 | Lip-sync padding bound | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Lip-sync playback contract"; `Zenith2/Docs/Design/CHAR/Facial.md` → "Lip-sync document" |
| Z2-R-CHAR-138 | Keep as variant mapping | S4 | `Zenith2/Docs/Design/CHAR/Modular.md` → "Keep as variant algorithm"; `Zenith2/Docs/Design/CHAR/Modular.md` → "Hand-edit protection" |
| Z2-R-CHAR-139 | Generator-owned root components | S4 | `Zenith2/Docs/Design/CHAR/Modular.md` → "Assembly validation rules"; `Zenith2/Docs/Design/CHAR/Modular.md` → "Assembly document" |
| Z2-R-CHAR-140 | Library rig stored per class | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Character project settings document"; `Zenith2/Docs/Design/CHAR/Workflow.md` → "Workflow panel" |
| Z2-R-CHAR-141 | Class labels select content-rule rows | S4 | `Zenith2/Docs/Design/CHAR/Debugging.md` → "Validation catalogue"; `Zenith2/Docs/Design/CHAR/Debugging.md` → "Content rule measures" |
| Z2-R-CHAR-142 | Bank default locale agrees with window 0 | S4 | `Zenith2/Docs/Design/CHAR/Facial.md` → "Sync from bank"; `Zenith2/Docs/Design/CHAR/Debugging.md` → "Validation catalogue" |
| Z2-R-CHAR-143 | Walkthrough starting content | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Walkthrough character workflow"; `Zenith2/Docs/Design/CHAR.md` → "Stage table" |
| Z2-R-CHAR-144 | Scriptable command arguments | S4 | `Zenith2/Docs/Design/CHAR/Workflow.md` → "Command argument forms"; `Zenith2/Docs/Design/CHAR/Workflow.md` → "Walkthrough rehearsal" |
| Z2-R-CHAR-145 | Member types declared in full | S3 | `Zenith2/Docs/Design/CHAR/Types.md` → "Scope of this file"; `Zenith2/Docs/Design/CHAR.md` → "Header map" |
| Z2-R-CHAR-146 | Control set resolution | S4 | `Zenith2/Docs/Design/CHAR/Rigging.md` → "Control evaluation"; `Zenith2/Docs/Design/CHAR.md` → "Controls and pose book API" |

## Reconciliation items

The rows CHAR recorded in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` and `C:/dev/Zenith2-drafts/reviews/CROSS_DOC_ITEMS.md`:

| Row | To | Need |
|---|---|---|
| CH1 | ANIM | face roles 55–62 in `Zenith_JointRole`; a `mask` member on graph slot entries; `POSE_BLEND` applying pose curve deltas; CHAR's importers writing `anim.clip` intermediates and annotation settings for a skeleton product of another source |
| CH2 | AUDIO | line IDs as `Zenith_Hash64` of the line text; the lip-sync phrase marker flag in bank-referenced sound data the editor reads; time-locked gameplay dialogue; the audition API from a CHAR panel |
| CH3 | CINE | dialogue face clips seek to the voice locale's window (`char.lipsync.<tag>` marker) and stop at `char.lipsync.windowEnd`; the voice locale index at cinematic start (amended by CH19: the tag, resolved through the clip's markers) |
| CH4 | GAME | the voice locale as game simulation state written from the settings UI with `audio.dialogue.locale`; equipment switching by part visibility; the flagship samples' lip-sync plays (part (a) amended by CH18) |
| CH5 | EDIT | the play-world resource accessor; an overlay contributor hook; the animated-character preview of a prefab with followers; CHAR's walkthrough beside EDIT's four; the walkthrough fixture's dependency on `EditEditorFixtureGames` |
| CH6 | GEOM | the `skin.rigidAttach` mesh setting |
| CH7 | ASSET (with ANIM, GEOM) | CHAR's importer slot; its measure option (a) is withdrawn by CH15 and CH17 |
| CH8 | PHYS | the ragdoll document members CHAR writes and component property 3 |
| CH9 | BUDGET | the two pending rows (X330); the character classes for ContentSpecs |
| CH10 | CONTENT | the character preset's front-marker convention (`char.front`) |
| CH11 | SCENE | prefab documents written by a tool with SCENE's prefab canonical schema |
| CH12 | SCRIPT | the misuse block ZS2800–ZS2899 reserved, none in use (X326) |
| CH13 | ARCH | the parser-limit rows (X329); its `DEPENDS` part (a) is superseded by the AUTHORING-CONTRACT growth rule and row CH14 |
| CH14 | EDIT | CHAR's row of the ZenithAuthoring owners table |
| CH15 | ANIM | the curve-and-pose clip intermediate writer in `Zenith_AuthoringAnimation.h`; the pure `Zenith_AnimComputeRateDivisor` that `Animation.Significance` calls; ANIM's measures of (Skeleton, `bones`), (Skeleton, `ikChains`), (AnimationGraph, `layers`); `BuildFromFrames` and `Zenith_MotionMatchSourceClip` with the Stage 3 harness ticket |
| CH16 | ANIM | the Luau `Animation.GetMarkerTime(clip, name)`; slot tag bit 14 `ANIM_SLOT_TAG_FULL_RATE` and the Luau `PlaySlot` option `tag` |
| CH17 | GEOM | GEOM's measure of (Mesh, `blendShapes`), beside its `triangles` measure |
| CH18 | GAME | the voice-locale model of the orchestrator's ruling CHAR-R1 (tag as simulation state, recorded command at a tick boundary, saved, re-issued after a restore whose setting differs), amending CH4 (a) |
| CH19 | CINE | the face clip's window from the voice-locale tag through the clip's markers and the stop at the first window end, amending CH3's index; the `needs-human` CinematicSlice walkthrough content-production ticket |
| CH20 | PHYS | the Euler convention of a ragdoll part's `offset.rotationDegrees` |
| CH21 | CORE | the deterministic binary64 functions CHAR calls; **answered**: `Zenith_Transcendental.h` (through `Zenith_Maths.h`), natural log `Zenith_Ln`, tickets Z2T-CORE-100 and Z2T-CORE-101 |
| CH22 | EDIT | `"Audio"` in EditorFixture's `modules`, so the walkthrough rehearsal's dialogue behaviour has AUDIO's bindings |
| X326 | Scripting/MisuseCatalogue.md | the CHAR block row |
| X327 | DecisionLog.md Z2-D-078 | the dated amendment classifying hair cards and shells |
| X328 | Contracts.md [Z2-N-CONTRACT-552] | the five CHAR document types |
| X329 | Architecture.md "Security architecture" | CHAR's parser-limit rows |
| X330 | Budgets.md | the two pending CHAR rows |
| X331 | Architecture.md "Library dependency graph" | withdrawn in revision 1: X301's growth rule covers it (CHAR's own diff appends its edges) |
| X360 | Contracts.md `ForbiddenCheck` rules; ContentSpecs.md "Character classes" | (1) the CRT-transcendental rule over CHAR's tool directories (PROC `needs-human`), superseded by X362, the programme-wide rule filed by the round 2 review; (2) the "Update-rate reduction tiers" column filled from CHAR's class presets |
| X340 | Budgets.md | filed by the round 1 review: Z2-B-CHAR-001's workload cell; the character-class selectors |
