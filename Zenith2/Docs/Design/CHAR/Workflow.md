# CHAR design: the complete character workflow and its editor tools

Section file of `Zenith2/Docs/Design/CHAR.md`. It specifies the complete character workflow, import → rig and retarget → author the animation graph → preview → in game, as editor tools over the documents of ANIM, PHYS, SCENE and CHAR: CHAR's editor extension (panels, commands, automation steps, events, preferences), the character import wizard, the workflow panel, the rig profile, assembly, posing and lip-sync panels, the graph template catalogue and its instantiation, preview scenarios and their runner, the in-game step, the motion-matching authoring-cost harness, and the usability walkthrough with its automation rehearsal. Every tool changes documents only through EDIT's command system (`Zenith2/Docs/Design/EDIT/Foundation.md → "Commands and history"`), so undo, save, migration and recovery are EDIT's common machinery; ANIM's own panels (graph editor, blend space, mask, rig setup, clip timeline, retarget setup, preview, live debugger) are EDIT's (`Zenith2/Docs/Design/EDIT/AuthoringTools.md → "Animation tools"`) and CHAR's tools open them, never duplicate them.

## Scope of this file

| Topic | Heading |
|---|---|
| the five steps and their artefacts | "Workflow overview" |
| what CHAR registers in the editor | "Editor extension", "Character commands", "Command argument forms", "Automation steps", "Editor events", "Character preferences" |
| step 1 | "Character import wizard" |
| the hub | "Workflow panel" |
| step 2 | "Rig profile panel", "Retarget step" |
| step 3 | "Graph template catalogue", "Template clip binding", "Template instantiation" |
| step 4 | "Preview scenarios", "Scenario runner" |
| step 5 | "In-game step" |
| other panels | "Assembly panel", "Posing panel", "Lip-sync panel" |
| D-SEL-MOTIONMATCH evidence | "Motion-matching authoring-cost harness" |
| usability acceptance | "Walkthrough character workflow", "Walkthrough rehearsal", "Walkthrough fixtures" |

## Workflow overview

| Step | The author does | Documents produced | Owner of the format | CHAR tool |
|---|---|---|---|---|
| 1 import | brings a skinned `.glb` (FBX per D-SEL-FBX (open, Stage 4)) into the game with the character preset | the source's `.meta` (preset, class label, expectations); the `skel:`, `mesh:`, `anim:` products | ASSET, CONTENT, ANIM | "Character import wizard" |
| 2 rig and retarget | assigns the humanoid profile, mirror pairs, chains, IK, facial template, a default ragdoll; retargets the shared library's clips | `.animrig.json`, `.ragdoll.json`, `.animretarget.json` | ANIM, PHYS | "Rig profile panel", "Retarget step" |
| 3 author the graph | instantiates a graph template bound to the clips, then edits it in ANIM's graph editor | `.animgraph.json`, `.animmask.json` | ANIM | "Graph template catalogue" |
| 4 preview | runs preview scenarios and scrubs in ANIM's preview | none (scenario results in the session) | — | "Preview scenarios" |
| 5 in game | assembles the parts, generates the prefab, plays it in a scene | `.character.json`, generated `.prefab.json` | CHAR, SCENE | "Assembly panel", "In-game step" |

Facial work (pose books, lip-sync documents, control sets) attaches to steps 2 and 5 ("Posing panel", "Lip-sync panel").

## Editor extension

`Zenith_CharacterEditorExtension` (`Zenith2/Tools/ZenithEditor/Internal/Character/Zenith_CharacterExtension.cpp`), an entry of EDIT's extension table (`Zenith2/Docs/Design/EDIT/Panels.md → "Extension table"`, the CHAR row, directory `Internal/Character/`), registers in `Register`:

| Registration | Items |
|---|---|
| panel types | `Zenith.Editor.Character.Workflow` (single), `.ImportWizard` (single), `.RigProfile` (multi, per skeleton), `.Assembly` (multi, document panel of `.character.json`), `.Posing` (multi), `.LipSync` (multi, document panel of `.lipsync.json`), `.PoseDebugger` (multi), `.Validation` (single), `.Budget` (single) |
| document handlers | `Zenith.CharacterAssemblyDocument` → `.Assembly`; `Zenith.CharacterLipSyncDocument` → `.LipSync`; `Zenith.CharacterControlSetDocument` and `Zenith.CharacterPoseBookDocument` → `.Posing`; `Zenith.CharacterProjectDocument` → EDIT's JSON document editor; each with `m_pfnValidate` running the document's rules of "Validation catalogue" (`Zenith2/Docs/Design/CHAR/Debugging.md`) |
| commands | "Character commands" |
| automation steps | "Automation steps" |
| preferences | "Character preferences" |

Registration fails with EDIT's errors (`EDITOR_ERROR_EXTENSION_DUPLICATE`, `EDITOR_ERROR_EXTENSION_LIMIT`) unchanged. `Test_CharEditorExtension_RegistersEverything` creates a headless session with the extension and asserts every panel type, handler, command and step is registered once.

## Character commands

Every command is `EDITOR_COMMAND_FLAG_AUTOMATABLE`; those that change documents also carry `EDITOR_COMMAND_FLAG_CHANGES_DOCUMENTS` and produce exactly one history record; `EDIT_ONLY` unless noted. Arguments are validated by EDIT before `Execute` (`Zenith2/Docs/Design/EDIT/Automation.md → "Command descriptors"`); every argument that names an asset, a document or an entity accepts the forms of "Command argument forms", so an automation script can name everything a run creates.

| Identifier | Arguments | Effect | Specified in |
|---|---|---|---|
| `character.import` | `source` (path), `class` (string) | the wizard's settings and import, asynchronous (the record commits when the import completes); the library rig is the class's `libraryRig` | "Character import wizard" |
| `character.rig.detectProfile` | `skeleton` (asset) | runs detection and returns the match as the command's data; changes nothing | `Zenith2/Docs/Design/CHAR/Rigging.md → "Profile matching algorithm"` |
| `character.rig.applyProfile` | `skeleton` (asset), `rigDocument` (path), `profile` (optional, default the best), `facial` (bool), `overwrite` (bool) | writes the rig document | `Zenith2/Docs/Design/CHAR/Rigging.md → "Rig document generation"` |
| `character.rig.addFacialTemplate` | `rigDocument` (asset) | adds face roles and curves | `Zenith2/Docs/Design/CHAR/Rigging.md → "Facial rig template"` |
| `character.ragdoll.generateDefault` | `skeleton` (asset), `path`, `class` | writes a ragdoll document | `Zenith2/Docs/Design/CHAR/Rigging.md → "Default ragdoll generation"` |
| `character.retarget.create` | `sourceSkeleton` (asset), `targetSkeleton` (asset), `clips` (optional array of assets), `path` | writes a retarget document | `Zenith2/Docs/Design/CHAR/Rigging.md → "Retarget setup"` |
| `character.graph.instantiateTemplate` | `template`, `skeleton` (asset), `path`, `bindings` (optional object role → clip asset), `facial` (bool) | writes the graph and its masks | "Template instantiation" |
| `character.graph.setBlendSample` | `graph` (asset), `state` (state name), `sample` (index), `value` (number) | sets `samples[sample].value` of the `Blend1D` node that is the root of the named state (the first match in layer order, then machine order), one JSON edit; the command form of the graph editor's sample field (`Zenith2/Docs/Design/EDIT/AuthoringTools.md → "Animation graph editor"`); a missing state, a root that is no `Blend1D` or an index out of range fails with `EDITOR_ERROR_COMMAND_ARGS` | "Graph template catalogue" |
| `character.preview.runScenario` | `graph` (asset), `scenario` (name or path) | runs a scenario; changes nothing; publishes `characterScenarioFinished` | "Scenario runner" |
| `character.assembly.create` | `skeleton` (asset), `graph` (asset), `path`, `class`, `ragdoll` (optional asset; default the one `Physics.Ragdoll` product of a `.ragdoll.json` naming the skeleton beside the rig document, none when there is not exactly one) | writes an assembly with one part per skinned mesh of the skeleton's source | "Assembly panel" |
| `character.assembly.generatePrefabs` | `assembly` (asset), `force` (bool) | generates the prefabs | `Zenith2/Docs/Design/CHAR/Modular.md → "Prefab generation algorithm"` |
| `character.assembly.keepAsVariant` | `assembly` (asset), `document` (asset: the edited generated prefab), `path` | the "Keep as variant" action | `Zenith2/Docs/Design/CHAR/Modular.md → "Keep as variant algorithm"` |
| `character.assembly.setLoadout` | `assembly` (asset), `name`, `parts` (part names) | creates or replaces the named loadout | `Zenith2/Docs/Design/CHAR/Modular.md → "Slots and loadouts"` |
| `character.assembly.setPartRegions` | `assembly` (asset), `part` (name), `bodyRegion` (optional), `hidesRegions` (optional) | replaces the part's region members | `Zenith2/Docs/Design/CHAR/Modular.md → "Body regions"` |
| `character.scene.placePrefab` | `scene` (path), `prefab` (asset), `id` (optional persistent ID of the new prefab-instance object, refused with `EDITOR_ERROR_COMMAND_ARGS` when it exists in the document, as `entity.create`'s `id`), `parentPath` (optional entity, default the scene root), `position` (a vector, or a position reference), `yawDegrees` (default 0) | adds a prefab instance to the open scene document: one JSON edit of its `instances` (`Zenith2/Docs/Design/SCENE/Format.md → "Prefab-instance object"`) whose `overrides.properties` set the root's `Zenith.LocalTransform` position and yaw; without `id` the editor's session random source draws it | "In-game step" |
| `character.scene.setReference` | `scene` (path), `entity` (entity), `type` (component stable name), `path` (property path), `target` (entity or asset) | sets one ENTITY-typed or asset-typed property of a scene entity to the resolved ID of `target`, one JSON edit through `component.setProperty`'s path; the command form of dropping a reference onto an inspector field | "In-game step" |
| `character.lipSync.create` | `bank` (asset), `skeleton` (asset), `path` | writes a lip-sync document and syncs it | `Zenith2/Docs/Design/CHAR/Facial.md → "Lip-sync document"` |
| `character.lipSync.syncFromBank` | `document` (asset) | | `Zenith2/Docs/Design/CHAR/Facial.md → "Sync from bank"` |
| `character.lipSync.setSource` | `document` (asset), `line`, `locale`, `source`, `visemes` (optional path) | | "Lip-sync panel" |
| `character.lipSync.setPhrases` | `document` (asset), `line`, `locale`, `phrases` (ascending seconds) | replaces the entry's `phrases`, one JSON edit; the command form of dragging the panel's phrase markers | `Zenith2/Docs/Design/CHAR/Facial.md → "Phrase markers"` |
| `character.lipSync.audition` | `document` (asset), `line`, `locale` | starts the panel's audition of that line and locale ("Lip-sync panel"); changes nothing; returns `{ "windowStart": seconds, "audio": true or false, "preview": true or false }` as data (`audio` false when AUDIO's audition is unavailable, `preview` false headless) and fails only for an unknown line or locale | "Lip-sync panel" |
| `character.posebook.generateVisemes` | `skeleton` (asset), `meshes` (assets), `path` | | `Zenith2/Docs/Design/CHAR/Facial.md → "Automatic viseme pose books"` |
| `character.posebook.capturePose`, `character.posebook.mirrorPose` | as specified | | `Zenith2/Docs/Design/CHAR/Rigging.md → "Pose capture and mirroring"` |
| `character.pose.snapshot` | `source` (`play` or `preview`), `entity` (entity, play only), `path` | writes a snapshot file under the game's `Saved/Character/` or the given path; changes no document | `Zenith2/Docs/Design/CHAR/Debugging.md → "Pose snapshot format"` |
| `character.pose.diff` | `a`, `b` (snapshot paths), tolerances | returns the diff as data | `Zenith2/Docs/Design/CHAR/Debugging.md → "Pose diff algorithm"` |
| `character.validate` | `paths` (optional; default the whole game) | returns diagnostics; publishes `characterValidated` | `Zenith2/Docs/Design/CHAR/Debugging.md → "Validation catalogue"` |
| `character.play.spawn` | `prefab` (asset), `position`, `yawDegrees` | `PLAY_ONLY`: spawns the prefab in the focused play world as a play-world edit | "In-game step" |
| `character.play.drive` | `entity` (entity), `timeline` (a scenario name of the character graph's template, a `.charscenario.json` path, or an inline step array of "Preview scenarios") | `PLAY_ONLY`: sets the character's animator parameters over time in the play world | "In-game step" |

`Test_CharCommands_EachChangingCommandIsOneRecord` executes every document-changing command on fixtures and asserts one record whose undo restores every document byte for byte; `Test_CharCommands_InvalidArgsRefused` asserts `EDITOR_ERROR_COMMAND_ARGS` for a missing required argument of each; `Test_CharCommands_ArgumentFormsResolve` asserts every form of "Command argument forms" on a fixture session.

## Command argument forms

| Kind | Forms accepted | Resolution |
|---|---|---|
| asset | a persistent ID (36-character form); a game-relative source path, naming the source's primary product or, for a document, the document; `<path>#<product key>` for a sub-asset product (`Assets/Characters/Walkthrough/Ranger.glb#skel:Hips`, `Assets/Audio/Dialogue/Walkthrough.lipsync.json#anim:lip.Walkthrough.Line01`) | the session's asset database (`Zenith2/Docs/Design/ASSET/Database.md → "Zenith_AssetDatabase API"`) by logical path, then the sub-asset key (`Zenith2/Docs/Design/ASSET/Database.md → "Sub-asset identity"`); an unknown path or key fails the command with `EDITOR_ERROR_COMMAND_ARGS` naming it |
| path | a game-relative path where a command writes a new document | refused when it leaves the game directory |
| entity | a persistent ID, or a hierarchy path `"/Root/Child"` of entity names (EDIT's `select` form, `Zenith2/Docs/Design/EDIT/Automation.md → "Step kinds"`), resolved in the open scene document for an `EDIT_ONLY` command and in the focused play world for a `PLAY_ONLY` one; a path segment crosses into a prefab instance by the instance root's name | a path naming no entity, or two siblings of one name, fails with `EDITOR_ERROR_COMMAND_ARGS` |
| position reference | `"$entity:<hierarchy path>"`: the world position of that entity in the open scene document, read when the command runs | as entity |

The forms are resolved before `Execute`, so a recorded command (history, the automation report) holds the resolved IDs and replays identically.

## Automation steps

Registered through `RegisterAutomationStep` (`Zenith2/Docs/Design/EDIT/ToolDeclarations.md → "Extension registration types"`); members naming assets or entities take the forms of "Command argument forms":

| Step kind | Members | Completes when | Fails when |
|---|---|---|---|
| `characterScenario` | `graph`, `scenario`, `timeoutSeconds` (default 60) | the runner finished and every assertion passed | an assertion failed (the report lists it), or the timeout |
| `assertCharacterValid` | `paths` (optional), `maxWarnings` (default unbounded) | the catalogue reported no error and at most `maxWarnings` warnings | otherwise; the report lists the diagnostics |
| `assertImportReport` | `source` (path), `facing` (`ok`, `backwards`, `mirrored`), `skeletons` (count), `heightMinMetres`, `heightMaxMetres` | the last `character.import` of the source in this session reported that facing and skeleton count, and a height in the range | otherwise, or no import of the source ran; the report lists the reported values |
| `assertPoseMatches` | `source`, `entity`, `snapshot`, `metres`, `radians` | the live pose matches the snapshot within the tolerances | a pose differs (the report lists the worst joints) |
| `assertCharacterSlot` | `entity`, `slot`, `clip` (asset), `startTime` (optional seconds), `tolerance` (default 1e-4) | the play world's character plays `clip` in `slot` (`GetDebugState`, the slot rows, read as the pose debugger reads them, `Zenith2/Docs/Design/CHAR/Debugging.md → "Pose sources"`) and, with `startTime`, the slot's start time equals it within the tolerance | the slot is empty or plays another clip |
| `assertLipSyncEntry` | `document`, `line`, `locale`, `phrases` (optional array), `source` (optional) | the document's entry has exactly those phrases (within 1e-6 s) and that source | otherwise; the report lists the entry's values |
| `characterDrive` | `entity`, `timeline` (as `character.play.drive`), `waitToEnd` (bool) | the timeline's last step was applied (with `waitToEnd`, after its end time) | the entity is not in the play world |

`Test_CharAutomation_StepsValidateMembers` asserts an unknown member and a wrong kind are refused before the first step (`EDITOR_ERROR_AUTOMATION_SCRIPT`).

## Editor events

Published on EDIT's event bus (`Zenith2/Docs/Design/EDIT/Automation.md → "Waiting and events"`), so `waitEvent` steps and panels can wait on them:

| Event | Members | Published |
|---|---|---|
| `characterImported` | `source`, `skeletons` (count), `facing` (`ok`, `backwards`, `mirrored`), `heightMetres` | after the import record of `character.import` commits and the rest-pose classification ran |
| `characterValidated` | `errors`, `warnings`, `infos` | after `character.validate` |
| `characterScenarioFinished` | `graph`, `scenario`, `passed`, `failures` | after a scenario run |
| `characterPrefabsGenerated` | `assembly`, `written` (count), `refused` (count) | after `character.assembly.generatePrefabs` |

## Character preferences

| Key | Kind | Default | Meaning |
|---|---|---|---|
| `editor.character.axisLength` | number, metres, [0.005, 1] | 0.05 | joint axis length of the overlays |
| `editor.character.poseHistoryTicks` | integer, [30, 3,600] | 300 | the pose history ring |
| `editor.character.showOverlaysInSceneViews` | bool | false | scene-view overlays of the pose debugger |
| `editor.character.scenarioTickHz` | integer, {30, 60} | 30 | the scenario runner's step |

## Character import wizard

The import wizard panel (`Zenith.Editor.Character.ImportWizard`) collects a source file and a class of the project document, shows the class's library rig (`classes.<class>.libraryRig` of `Zenith2/Docs/Design/CHAR/Facial.md → "Character project settings document"`, edited in the project document, never stored by the wizard), then runs `character.import` as one asynchronous record:

1. Refuse a source that is not `.glb`, `.gltf` (or `.fbx` when D-SEL-FBX (open, Stage 4) selects a conversion path; otherwise `.fbx` is refused with the message that names the decision).
2. When the source has no `.meta`, ask the cook server to assign its GUID (`assignGuids`, which writes the `.meta`, the path EDIT's asset creation uses, `Zenith2/Docs/Design/EDIT/Pipeline.md → "Create and duplicate"`).
3. Build the source's importer settings: CONTENT's shipped preset `mesh.character` (`Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Shipped presets"`); labels `character` and the class's label (`character-<class>`); `validate.heightRangeMetres` the class's range; `validate.frontMarkerNode` empty; `validate.strict` false. Apply them with `asset.setSettings` and import with `asset.import` (`Zenith2/Docs/Design/EDIT/Automation.md → "Built-in commands"`).
4. When the import completes, read CONTENT's import report member (`Zenith2/Docs/Design/CONTENT/Importers.md → "Import report member"`): the node names, the scene height, the front marker's yaw. When the project's `frontMarkerNode` names a node of the source, apply `validate.frontMarkerNode` = that name with a second `asset.setSettings` and `asset.reimport`, in the same record; then run the rest-pose classification on each `skel:` product (`Zenith2/Docs/Design/CHAR/Rigging.md → "Rest pose classification"`). The record commits after these requests.
5. Show the results; a facing or unit problem shows CONTENT's suggested setting (`transform.sourceFront`, `transform.unitScale`) with an "Apply and reimport" button (one further `asset.setSettings` + `asset.reimport` record).
6. More than one skeleton product: the wizard lists them and the author picks the character's; none: the source is not a character (`CHAR_DIAG_IMPORT_NO_SKELETON`).
7. Record the session's import report for `assertImportReport`, publish `characterImported`, and open the workflow panel on the character.

`Test_CharImportWizard_WritesCharacterSettings` asserts the settings JSON of step 3 for a fixture source without a `.meta`, and that the GUID was assigned before the settings apply; `Test_CharImportWizard_FrontMarkerSecondPass` asserts step 4's second settings apply for a source with a `char.front` node and its absence without one; `Test_CharImportWizard_ReportsBackwardsFacing` imports a fixture rotated 180° and asserts the event's `facing` is `backwards` and the suggested setting.

## Workflow panel

`Zenith.Editor.Character.Workflow` lists the game's characters (each assembly document, and each skeleton of a `character`-labelled source without an assembly) and, for the selected one, the five steps with a state computed from documents alone (except "preview", which is session state):

| Step | Done when | Action button |
|---|---|---|
| import | a skeleton product exists and its rest-pose classification has no error (CV2) | open the import wizard |
| rig | a rig document names the skeleton and CV1 reports a complete profile | `character.rig.applyProfile` through the rig profile panel |
| retarget | the skeleton is a library rig (its source carries `char-library-rig`); or the class has no `libraryRig`; or a retarget document whose source skeleton is the class's `libraryRig` and whose target is this skeleton exists | `character.retarget.create` from the class's `libraryRig` |
| graph | a graph document of the skeleton exists and `Zenith_CompileAnimationGraph` reports no error | `character.graph.instantiateTemplate`, then open ANIM's graph editor |
| preview | the template's built-in scenarios passed in this session for the current graph hash; `lipsync-line` counts only when the game has a lip-sync clip of the skeleton (a cooked `anim:lip.<line>` product of a lip-sync document whose `skeleton` is this one), so a facial graph reaches DONE before any lip-sync document exists | `character.preview.runScenario` |
| in game | an assembly names the graph, its prefabs are generated and current, and validation reports no error | open the assembly panel, then `character.play.spawn`; while a play world runs, **Drive** runs `character.play.drive` on the play-world character selected in the outliner with the timeline chosen in the button's drop-down (the built-in scenarios of the graph's template, then the game's `.charscenario.json` files) |

The character's class is the assembly's `class`, or, before an assembly exists, the class whose label the source's `.meta` carries. A step's state is recomputed after every `historyCommitted` and `cookIdle` event. `Test_CharWorkflowPanel_StepStatesFromDocuments` asserts each state on fixture sessions, including the three retarget conditions; `Test_CharWorkflowPanel_DriveRunsTimeline` asserts the Drive button issues `character.play.drive` with the chosen timeline.

## Rig profile panel

`Zenith.Editor.Character.RigProfile` shows, for one skeleton: the detection result (profile, score, the role → joint table with unassigned required roles highlighted), the diagnostics, the mirror pairs, chains and IK definitions that would be written, the rest-pose report, and the diff of the rig document that Apply would make (SCENE's document diff over the edit list, `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_DocumentDiff header"`). A role can be reassigned by clicking a joint in the panel's preview (EDIT's joint picking of the rig setup panel) before Apply; Apply runs `character.rig.applyProfile` with the edited mapping. "Generate ragdoll" runs `character.ragdoll.generateDefault`; "Open rig setup" opens ANIM's rig setup panel on the document for fine edits.

## Retarget step

The workflow's retarget step runs `character.retarget.create` from the class's `libraryRig` (the project document's member, "Workflow panel") to the character's skeleton with every library clip, then opens ANIM's retarget setup panel (`Zenith2/Docs/Design/EDIT/AuthoringTools.md → "Retarget setup"`) for the live preview; the products `anim:<clip>@<skeleton>` appear when the cook server has imported the retarget document (`cookIdle`). A class with no `libraryRig` shows the step done and the button disabled, with the reason.

## Graph template catalogue

Templates are constexpr JSON texts in `Zenith2/Tools/Authoring/Internal/CHAR/Zenith_CharGraphTemplates.cpp` (Stage 4; byte-equal to the Stage 3 copies the authoring-cost harness measures, "Motion-matching authoring-cost harness"), each a graph document with placeholders (`{{skeleton}}`, `{{clip:<role>}}`, `{{mask:<name>}}`, `{{id:<key>}}`) and a role table:

| Template | Use | Clip roles (required in bold) | Graph |
|---|---|---|---|
| `char.humanoidLocomotion` | humanoid players and NPCs | **idle**, **walk**, jog, run, jumpStart, inAir, land | below |
| `char.humanoidMotionMatching` | humanoid locomotion through node kind 15, offered only when the D-SEL-MOTIONMATCH (open, Stage 3) ruling classifies motion matching required | **idle**, **locomotion** (every clip whose name has a locomotion token: walk, jog, run, turn, start, stop) | the humanoid template with the Move state's blend replaced by a `MOTION_MATCH` node over the locomotion clips and the parameters `DesiredVelocityX`, `DesiredVelocityZ`, `DesiredFacingX`, `DesiredFacingZ` (`Zenith2/Docs/Design/ANIM/Graph.md → "Motion matching node"`), keeping the humanoid template's `Grounded`, `Jump`, jump states and, with `facial`, `FaceWeight`; `Speed` is not declared |
| `char.simpleLoop` | props, creatures, crowd extras | **loop** | one clip node and the five slots |

`char.humanoidLocomotion`:

| Part | Content |
|---|---|
| parameters | `Speed` Float [0, 8] default 0; `Grounded` Bool default true; `Jump` Trigger, hold 2 ticks; `FaceWeight` Float [0, 1] default 1 (only with `facial`) |
| machine `Locomotion` (layer `Base`) | states `Idle` (clip idle, loop), `Move` (a `BLEND1D` on `Speed` over walk 1.4, jog 3.5, run 6.0 in sync group `Feet` with marker sync; with jog and run absent, the walk clip node alone), `JumpStart` (jumpStart), `InAir` (inAir, loop), `Land` (land) |
| transitions (priority order) | Any → `JumpStart` on `Jump` Triggered and `Grounded` True, 0.1 s inertial; `Idle` → `Move` on `Speed` Greater 0.1, 0.2 s inertial; `Move` → `Idle` on `Speed` Less 0.05, 0.25 s inertial; `JumpStart` → `InAir` on StateTimeAtLeast 0.9 × the clip's duration, 0.1 s crossfade; `InAir` → `Land` on `Grounded` True, 0.1 s inertial; `Land` → `Idle` on StateTimeAtLeast 0.8 × duration, 0.2 s inertial; without the jump clips the jump states and their transitions are omitted |
| root path after the machine | `SLOT` `Action` (blend 0.2 / 0.25, root motion true) → `SLOT` `Zenith.GetUp` (0 / 0.3, root motion true; ANIM's ragdoll get-up slot, `Zenith2/Docs/Design/ANIM/Runtime.md → "Ragdoll recover"`) → `SLOT` `cine.body` (0.25 / 0.25, root motion true; CINE's, row CN14) → the face part of `Zenith2/Docs/Design/CHAR/Facial.md → "Face graph template"` when `facial` |
| IK | `LeftFoot`, `RightFoot` foot placement weight 1; `LookAt` from IK goal 0 weight 1 (the rig's generated definitions, `Zenith2/Docs/Design/CHAR/Rigging.md → "Chain and IK template detection"`) |
| editor members | canvas positions for every node and state on a 220-unit grid, and one comment per region |

## Template clip binding

`Zenith_CharBindTemplateClips`, declared in `Zenith2/Docs/Design/CHAR.md → "Assembly and template API"` as

```cpp
Zenith_Result<void, Zenith_CharError> Zenith_CharBindTemplateClips(const Zenith_CharTemplateDesc& xTemplate,
	Zenith_Span<const Zenith_CharClipCandidate> axClips, Zenith_Span<const Zenith_CharRoleBinding> axExplicit,
	Zenith_Array<Zenith_CharRoleBinding>& axOut, Zenith_Array<Zenith_CharDiagnostic>& axDiagnostics);
```

binds roles to clips. Candidates are the clips of the skeleton (its own `anim:` products and the retargeted `anim:<clip>@<skeleton>` products), each named by its source animation name: the `<name>` of `anim:<name>` and of `anim:<name>@<skeleton>` (so the retargeted `anim:Walk@Hips` is the candidate `Walk`). For each role in the template's order: tokenise each candidate's name (`Zenith2/Docs/Design/CHAR/Rigging.md → "Joint name normalisation"`); a candidate matches the role when it holds every required token and no excluded token:

| Role | Required tokens (any of) | Excluded tokens |
|---|---|---|
| idle | `idle` | `turn`, `to` |
| walk | `walk` | `start`, `stop`, `turn`, `back`, `strafe` |
| jog | `jog` | as walk |
| run | `run`, `sprint` | as walk |
| jumpStart | (`jump`) and (`start`, `up`, `takeoff`) | — |
| inAir | (`jump`, `fall`) and (`loop`, `air`, `fall`, `mid`) | `start`, `land` |
| land | `land` | — |
| loop | any | — |

Among matches the binding takes the one with the fewest tokens, then the shortest name, then the lowest byte order; a clip is bound to at most one role (roles in order). An entry of `axExplicit` wins over matching. An unbound required role fails with `CHAR_ERROR_BINDING_MISSING` and a diagnostic naming the role and the candidates; an unbound optional role removes its node as the template states. `axOut` lists every bound role in the template's order. The walkthrough's library clips (`Zenith2/Docs/Design/CHAR/Tests.md → "Fixtures and generators"`) bind as `Idle` → idle, `Walk` → walk, `Jog` → jog, `Run` → run, `JumpStart` → jumpStart (tokens `jump`, `start`), `JumpLoop` → inAir (`jump`, `loop`), `Land` → land. `Test_CharTemplateBinding_MatchesByTokens` (that set, plus the counter-example `in air`, whose tokens `in`, `air` lack `jump` and `fall` and so bind nothing), `Test_CharTemplateBinding_ExplicitBindingWins` and `Test_CharTemplateBinding_RequiredMissingFails` cover it.

## Template instantiation

`Zenith_CharInstantiateTemplate(const Zenith_CharTemplateRequest&, Zenith_CharTemplateOutput& xOut, Zenith_Heap&)`:

1. Bind clips ("Template clip binding").
2. Substitute placeholders: `{{skeleton}}` the skeleton GUID; `{{clip:<role>}}` the bound clip's GUID; `{{id:<key>}}` `Zenith_DeriveAuthoringID(graph document ID, L("tpl/" ‖ key))` with the `L` of "Generated entity identity" (`Zenith2/Docs/Design/CHAR/Modular.md`), so instantiating again over the same document keeps every node, state and transition ID; `{{mask:<name>}}` the GUID of the generated mask document.
3. Remove the nodes of unbound optional roles and every transition or sample that names them; collapse a `BLEND1D` left with one sample into its clip node.
4. With `facial`, append the face part and generate the two masks and, when the skeleton's meshes have morph targets and no face pose book exists, the automatic viseme pose book.
5. Write the result as canonical JSON (ANIM's graph document schema, `Zenith2/Docs/Design/ANIM/Formats.md → "Graph document schema"`) and compile it in memory with `Zenith_CompileAnimationGraph`; a diagnostic of severity error fails the command with the diagnostics listed (a template that does not compile is a CHAR defect).

`Test_CharTemplate_InstantiatesAndCompiles` (every template, with every optional role bound and with every optional role absent) asserts no compile diagnostic; `Test_CharTemplate_ReinstantiationKeepsIDs` asserts step 2's stability; `Test_CharTemplate_HasCinematicAndGetUpSlots` asserts the five slot names.

## Preview scenarios

A preview scenario is a scripted parameter timeline with assertions, run against a graph in ANIM's preview (no world, no physics). Built-in scenarios live with their template; a game adds its own as `*.charscenario.json` files (format `zenith2.char-scenario`, version 1, not an authoring document; the `char.document` importer validates it, `Zenith2/Docs/Design/CHAR.md → "Document validation importer"`):

```json
{
	"$schema": "zenith2.char-scenario",
	"schemaVersion": 1,
	"name": "walk-run-stop",
	"durationSeconds": 7.0,
	"steps": [
		{ "at": 0.0, "set": { "Speed": 0.0 } },
		{ "at": 1.0, "set": { "Speed": 1.4 } },
		{ "at": 3.0, "set": { "Speed": 6.0 } },
		{ "at": 5.0, "set": { "Speed": 0.0 } }
	],
	"assertions": [
		{ "kind": "stateAt", "at": 0.5, "layer": 0, "state": "Idle" },
		{ "kind": "stateAt", "at": 2.5, "layer": 0, "state": "Move" },
		{ "kind": "stateAt", "at": 6.9, "layer": 0, "state": "Idle" },
		{ "kind": "rootDistance", "min": 7.4, "max": 22.2 },
		{ "kind": "finite" }
	]
}
```

| Step member | Effect |
|---|---|
| `set` | `SetParameter` for each name |
| `trigger` | `SetTrigger` |
| `playSlot` | `{ "slot", "clip" (role or GUID), "blendIn", "blendOut" }` → `PlaySlot` |

| Assertion | Passes when |
|---|---|
| `stateAt` | at the first tick at or after `at`, the layer's machine is in `state` (`GetDebugState`, name hash) and not in transition |
| `eventCount` | between `from` and `to` (default the whole run), the number of fired events named `name` lies in [`min`, `max`] |
| `rootDistance` | the horizontal length of the composed root motion over the run lies in [`min`, `max`] metres |
| `finite` | every pose of every tick is finite |
| `poseMatches` | at `at`, the pose matches the snapshot file within `metres` and `radians` ("Pose diff algorithm" of `Zenith2/Docs/Design/CHAR/Debugging.md`) |

Built-in scenarios, with every time derived from the bound clips so they hold for any clip set that binds:

| Scenario | Steps | Assertions |
|---|---|---|
| `walk-run-stop` | the example above | the three `stateAt` assertions, `finite`, and `rootDistance` with `min` = 0.5 × and `max` = 1.5 × `D = v_walk × 2 + v_run × 2`, the bound clips' published root speeds times the two 2 s segments (with jog and run absent, `v_run` is the walk speed); the walkthrough's clips give D = 1.4 × 2 + 6.0 × 2 = 14.8 m, so [7.4, 22.2] |
| `idle-hold` | 10 s of `Speed` 0 | `stateAt` Idle at 9.9; `rootDistance` max 0.05 m |
| `jump` (with the jump clips) | `trigger` Jump at `a` = 1.0; `set` Grounded false at `a` + 0.1; `set` Grounded true at `g = a + 0.9 × d_jumpStart + 0.1 + 0.4` | `stateAt` JumpStart at `a + 0.2` (after the 0.1 s inertial entry); `stateAt` InAir at `a + 0.9 × d_jumpStart + 0.1 + 0.1` (the StateTimeAtLeast condition, the 0.1 s crossfade and two ticks of margin at 30 Hz, rounded up to 0.1); `stateAt` Idle at `g + 0.1 + 0.8 × d_land + 0.2 + 0.1` (the 0.1 s landing blend, Land's StateTimeAtLeast and its 0.2 s exit, 0.1 margin); `durationSeconds` that time + 0.5 |
| `lipsync-line` (with `facial`, and only when `Zenith_CharBuildBuiltInScenario` receives a lip-sync clip candidate `pxLipSyncClip`; null gives `CHAR_ERROR_NOT_AVAILABLE` and the scenario is not offered) | `playSlot` of that clip in `char.lipsync` at time 0 (window 0); `durationSeconds` = `e_0 + 0.1`, with `e_0` the clip's first `char.lipsync.windowEnd` time (`m_dFirstWindowEndSeconds` of the candidate); the scenario never stops the slot, and `e_0 + 0.1` stays inside window 0 because `padSeconds ≥ 0.2` gives `e_0 ≤ S − 0.15` | `finite`; the `char.lipsync.windowEnd` event count 1 |

For the walkthrough's clips (`d_jumpStart` = 0.4 s, `d_land` = 0.5 s, "Fixtures and generators" of `Zenith2/Docs/Design/CHAR/Tests.md`): Jump at 1.0, Grounded false at 1.1, JumpStart asserted at 1.2, InAir at 1.56, Grounded true at 1.86, Idle at 2.66, duration 3.16 s. At 30 Hz the trigger applies at tick 30 (1.0 s) and its inertial entry ends by tick 33; `JumpStart → InAir` fires at the first tick whose state time reaches 0.9 × 0.4 = 0.36 s, tick 40 or 41 (1.333 or 1.367 s, according to whether the entering tick counts), so its crossfade ends by 1.467 s, before 1.56; the Grounded step at 1.86 lies in [55 / 30, 56 / 30) and applies at tick 55 (1.833 s), Land's StateTimeAtLeast 0.4 s is met by tick 67 (2.233 s) and the 0.2 s exit ends by 2.467 s, before 2.66.

## Scenario runner

`Zenith_CharRunScenario`, declared in `Zenith2/Docs/Design/CHAR.md → "Assembly and template API"` as

```cpp
Zenith_Result<void, Zenith_CharError> Zenith_CharRunScenario(const Zenith_CharScenario& xScenario,
	Zenith_AnimationPreview& xPreview, uint32 uTickHz, Zenith_CharScenarioResult& xOut);
```

runs, for tick n = 0 … `ceil(duration × hz − 1e-9) − 1` at `hz` = `uTickHz` (the editor passes `editor.character.scenarioTickHz`; tests pass 30): apply every step whose `at` lies in `[n / hz, (n + 1) / hz)` in array order; `Advance(1 / hz)`; record the state of each layer, the fired events (`GetEvents`), the root-motion delta (`GetRootMotion`, composed with `Zenith_MultiplyDualQuaternions` into a running total) and finiteness; then evaluate the assertions. A `uTickHz` outside {30, 60} is `CHAR_ERROR_INVALID_ARGUMENT`. The runner needs no renderer and no world, so it runs headless and in tests. A preview whose clips are not all resident is a caller error (the preview requires every clip resident, `Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationPreview header"`). O(ticks × the preview's evaluation). `Test_CharScenario_RunsBuiltInsOnFixtureGraph` (`ZenithCook_Tests`: the template instantiated over the walkthrough clips, cooked with the cook test access) runs every built-in and asserts all pass; `Test_CharScenario_FailingAssertionReported` asserts a deliberately wrong `stateAt` fails with the observed state in the result; `Test_CharScenario_Deterministic` asserts identical results over two runs.

## In-game step

The in-game step plays the generated prefab in a scene of the game:

1. Open (or keep) a scene; place the base prefab or a loadout variant from the asset browser (EDIT's placement).
2. `play.start` (EDIT); the character binds in its first ticks (ANIM's `BINDING` → `BOUND` through tick-boundary publication).
3. `character.play.drive` applies a scenario timeline to the play-world character: at each editor frame, for each step whose time has come in the play world's simulated time, record `Zenith_RecordSetAnimParameter` (or `Zenith_RecordSetAnimTrigger`, `Zenith_RecordPlaySlot`, `Zenith_AnimationCommands.h`, `Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationCommands header"`) as a play-world edit applied out of tick between frames (`Zenith2/Docs/Design/EDIT/Foundation.md → "Play-world edits"`); a character with a controller in a root-motion mode then moves through collision.
4. The pose debugger and the live animation debugger inspect it; Snapshot freezes a pose.

In the scene document, `character.scene.placePrefab` places an instance (its `id` and position reference make the placement scriptable) and `character.scene.setReference` points another entity's ENTITY- or asset-typed property at it, the two edits a game's dialogue or gameplay entity needs to find the character. `character.play.spawn` spawns a prefab at a position in the focused play world without editing the scene (a play-world edit of `Zenith_RecordInstantiatePrefab`, `Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_LifecycleCommands header"`). `Test_CharInGame_SpawnBindsWithinTicks` (in `z2-edit-cooked`) spawns the walkthrough's generated prefab in a headless play world of the fixture game and asserts the animator is `BOUND` within 10 ticks and moves under `character.play.drive` by at least 1 m in 2 s; `Test_CharInGame_PlacePrefabWithIdAndReference` places the prefab with an explicit `id` at `$entity:/PlayerStart`, sets a reference to it, and asserts the instance object's key, its root position and the referencing field's value (the instance root's derived ID, `Zenith2/Docs/Design/SCENE/Prefabs.md → "Derivation"`).

## Assembly panel

`Zenith.Editor.Character.Assembly` edits an assembly document: the part list (add from the asset browser by dropping skinned meshes; a dropped mesh becomes a part named and slotted by the rules of `character.assembly.create` below), the slot and region lists, the loadout grid (rows parts, columns loadouts, checkboxes; conflicts and region holes marked in the cells), the root components (EDIT's inspector over `rootComponents`), the generation status per generated document (current, stale, edited outside, "Hand-edit protection" of `Zenith2/Docs/Design/CHAR/Modular.md`, with its **Discard edits** and **Keep as variant** actions), and a preview of the selected loadout: an `EDITOR_PREVIEW_ANIMATED_CHARACTER` preview of the base prefab with the loadout's visibilities applied in the preview world, driven by the graph's idle (`Zenith2/Docs/Design/EDIT/ToolDeclarations.md → "Preview host header"`). Generate runs `character.assembly.generatePrefabs`.

`character.assembly.create` seeds the document:

1. Slots `Body` (non-exclusive), `Hair`, `Outfit`, `Boots` (exclusive), in that order.
2. One part per skinned mesh of the skeleton's source, in the source's mesh order. The part **name** is the mesh name with every byte outside `[A-Za-z0-9]` removed, its first letter upper-cased, a leading digit prefixed with `P`, truncated to 32 bytes; a name already taken gets the suffix `2`, then `3`, and so on (truncated first so the suffixed name stays within 32 bytes); an empty result is `Part<n>` with n the mesh index. The result always matches `^[A-Za-z][A-Za-z0-9]{0,31}$`.
3. Each part's slot and region from the mesh name's tokens (`Zenith2/Docs/Design/CHAR/Rigging.md → "Joint name normalisation"`), the first row that matches winning:

| Tokens (any of) | Slot | Region | Other |
|---|---|---|---|
| `hair` | Hair | — | `hairCards` true |
| `coat`, `jacket`, `shirt`, `outfit` | Outfit | — | — |
| `boot`, `boots`, `shoe` | Boots | — | — |
| `head`, `face` | Body | Head | — |
| `torso`, `chest`, `body` | Body | Torso | — |
| `arm`, `arms` | Body | Arms | — |
| `hand`, `hands` | Body | Hands | — |
| `leg`, `legs` | Body | Legs | — |
| `foot`, `feet` | Body | Feet | — |
| anything else | Body | — | — |

4. `regions`: the regions step 3 found, in the order Head, Torso, Arms, Hands, Legs, Feet.
5. One loadout `Default` holding every Body and Hair part; `defaultLoadout` is it; `ragdoll` from the command's argument; `rootComponents` empty.

The walkthrough's Ranger meshes `Head`, `Torso`, `Arms`, `Hands`, `Legs`, `Feet`, `Coat` and `HairCards` become parts of those names (the six regions, Outfit for `Coat`, Hair for `HairCards`). `Test_CharAssemblyCreate_NamesSlotsAndRegions` asserts that mapping, a name collision (`Torso` twice → `Torso`, `Torso2`), a name of only punctuation (`Part3`) and a leading digit (`2Hat` → `P2Hat`).

<!-- assumption: pending EDIT (row CH5) -->
The animated-character preview kind takes a prefab with followers (several skinned meshes driven by one `Zenith_AnimationPreview`); row CH5 asks EDIT's preview host to pose every follower mesh of a previewed prefab from the leader's preview pose, as ANIM's extractor does in a world.
<!-- /assumption -->

## Posing panel

`Zenith.Editor.Character.Posing` opens a control set or a pose book over an animated-character preview of its skeleton: sliders per Scalar control grouped by `groups`, a draggable point gizmo per Point control in the preview, the evaluated result ("Control evaluation" of `Zenith2/Docs/Design/CHAR/Rigging.md`), and the pose book's pose list with Capture, Mirror, Delete and Preview (blend a pose in at 0 … 1). Every change of a document is a command; slider drags merge into one record per interaction (`Zenith2/Docs/Design/EDIT/Foundation.md → "Interactions and merging"`).

## Lip-sync panel

`Zenith.Editor.Character.LipSync` opens a lip-sync document:

| Region | Content |
|---|---|
| lines | every line with per-locale status: source, duration, estimated or imported, override count, stale (the bank digest), missing audio |
| timeline | EDIT's timeline widget (`Zenith2/Docs/Design/EDIT/AuthoringTools.md → "Timeline"`) over the selected (line, locale) with the audio waveform, the phrase markers (draggable; a drag ends in one `character.lipSync.setPhrases` record), and the sixteen curves in EDIT's curve editor (`Zenith2/Docs/Design/EDIT/AuthoringTools.md → "Curve editor"`); a curve edit writes an override over the edited range |
| audition | `character.lipSync.audition`: plays the line's locale sound through AUDIO's audition (`Zenith_AudioSystem::Audition`, `Zenith2/Docs/Design/AUDIO/Authoring.md` audition) and the cooked lip-sync clip in an animated-character preview of the face skeleton from the locale's window start; both start in the same editor frame and the preview advances by the frame time, so the two drift by at most one frame (authoring sync, not the game's time-locked sync) |
| actions | Sync from bank, Set source (Estimate, Import, Silent), Import visemes file, Reimport (asks the cook server to re-import the document), Validate |

The panel reads the estimator's output from the cooked clip (it never runs the estimator in process), so what it shows is what the game plays. `Test_CharLipSyncPanel_PhraseDragIsOneRecord` asserts a marker drag of several frames commits one `setPhrases` record; `Test_CharLipSyncPanel_AuditionReportsHeadless` asserts the audition command's data in a headless session (`preview` false, `windowStart` S for locale index 1).

## Motion-matching authoring-cost harness

The D-SEL-MOTIONMATCH acceptance rule includes "the authoring cost of the animation database" (`Zenith2/Docs/DecisionLog.md → "D-SEL-MOTIONMATCH"`); ANIM's harness measures runtime CPU and memory (`Zenith2/Docs/Design/ANIM.md → "Open decisions"`, `Z2-R-ANIM-178`); CHAR's harness measures the authoring cost of the two candidates, A (blend trees and state machines only) and B (motion matching).

The harness uses only what exists before the ruling, so the evidence does not depend on the decision it informs:

- ANIM's motion-matching header, which lands with ANIM's harness ticket (`Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationMotionMatch header"`): `Zenith_MotionMatchDatabase::BuildFromFrames`, `GetView`, `GetResidentBytes`, `Zenith_MotionMatcher::NormaliseQuery` and `Search`;
- ANIM's Stage 3 pose operation `Zenith_BlendPoses` (`Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationPose header"`);
- a harness-local evaluator and harness-local source frames.

It never compiles or evaluates node kind 15 (`Z2-R-ANIM-179`, whose tickets the ruling blocks), never calls `Zenith_CompileAnimationGraph`, and links no CHAR Stage 4 code (no `Zenith_CharGraphTemplates.cpp`). Its `DEPENDS` are `ZenithCore` and `ZenithAnimation`.

<!-- assumption: pending ANIM (row CH15) -->
`Zenith_MotionMatchSourceClip` carries "Version: since Z2 Stage 4" in ANIM's header while the header lands with the Stage 3 harness ticket; row CH15 asks ANIM to land `BuildFromFrames` and `Zenith_MotionMatchSourceClip` with that ticket. If ANIM keeps them at Stage 4, CHAR's harness ticket moves to the first Stage 4 slice and the ruling ticket waits for it.
<!-- /assumption -->

Files in `Zenith2/Benchmarks/CharacterAuthoring/`: `Zenith_Bench_CharMotionMatchAuthoring.cpp` (the cases), `Zenith_BenchCharMmSource.h` (the source frames), `Zenith_BenchCharMmEvaluator.h` and `.cpp` (the evaluator), `Zenith_BenchCharTemplateTexts.h` (the two template document texts as constants), `CMakeLists.txt` (`zenith_add_benchmark`).

**Source frames** (`Zenith_BenchCharMmSource.h`, header-only, from constants). A skeleton of J = 62 joints: joint 0 the hips (parent `0xFFFF`), joint j ≥ 1 with parent `(j − 1) / 2` (integer division); left foot joint 7, right foot joint 8. Clip c (c = 0 … C − 1) has F = 60 frames at 30 Hz (2 s), loops, speed `v_c = 1.4 + 4.6 × (c mod 10) / 9` m/s; frame k, joint j ≥ 1: translation (0, 0.1, 0), rotation about +X by `0.3 × Zenith_Sin(2 × dPI × (1 + c mod 3) × k / 30 + 0.1 × j)` radians, unit scale; joint 0: identity; the motion track `Q_k` translates by `(0, 0, v_c × k / 30)`.

**Evaluator** (`Zenith_BenchCharMmEvaluator`), both candidates producing a local pose for a desired speed v in [0, 6]:

- A: the `char.humanoidLocomotion` Move blend over three clips bound as walk (speed 1.4), jog (3.5) and run (6.0) — the clips c = 0, 4 and 9 of the set, whose speeds are 1.4, 3.444 and 6.0 —: the bracketing pair (a, b) by speed, `w = clamp((v − v_a) / (v_b − v_a), 0, 1)`, each sampled at the shared normalised phase by linear interpolation of its source frames (translation lerp, rotation nlerp), blended with `Zenith_BlendPoses`.
- B: the 27-value query of ANIM's feature order (feet positions 6, foot velocities 6, hips velocity 3, trajectory positions 6, trajectory facings 6) built by the evaluator from the current pose and the desired straight-line trajectory at 1/3, 2/3 and 1 s (x and z), normalised with `NormaliseQuery`, matched with `Search` (current frame `uANIM_MOTION_MATCH_NO_FRAME`), and the found frame's clip sampled at its time as in A.

| Case | Measures |
|---|---|
| database build | `BuildFromFrames` time (median of 10) and `GetResidentBytes` for C = 10, 50 and 200 clips |
| clip tagging | the clips an author must tag or annotate: A 7 (the bound roles of `char.humanoidLocomotion`), B C + 1 (every database clip and the idle); counted, not timed |
| parameters | the parameter count of each template text without `facial`: A 3 (`Speed`, `Grounded`, `Jump`), B 6 (`DesiredVelocityX`, `DesiredVelocityZ`, `DesiredFacingX`, `DesiredFacingZ`, `Grounded`, `Jump`) |
| template bytes | the canonical byte size of each template document text in `Zenith_BenchCharTemplateTexts.h` |
| iteration | the time from a clip annotation edit to a playable pose (median of 10): A re-binds the three clips and evaluates one pose; B rebuilds the database from the edited frames with `BuildFromFrames`, then builds one query, searches and samples; at C = 10, 50, 200 (the edit shortens clip 0 to 50 frames) |

The template texts live in the harness at Stage 3. At Stage 4 the shipped texts move into `Zenith2/Tools/Authoring/Internal/CHAR/Zenith_CharGraphTemplates.cpp` (`char.humanoidLocomotion` always; `char.humanoidMotionMatching` only when the ruling classifies motion matching required), and `Test_CharTemplate_HarnessTextsMatch` (`ZenithAuthoring_Tests`, Stage 4) asserts each shipped text equals the harness constant byte for byte, so the measured sizes stay those of what ships.

It writes `Zenith2/out/<hash>/bench/CharMotionMatchAuthoring.json` (`{ "schemaVersion": 1, "cases": [ { "name", "candidate", "clips", "medianMs", "ci95Ms", "bytes", "count" } ] }`). The CTest smoke `Bench_CharMotionMatchAuthoring_Smoke` runs the C = 10 build and one pose of each candidate once in the `Test` step and passes when both succeed and the report parses. The `needs-human` ruling ticket (CHAR's, "Classification and selection decisions CHAR owns" of `Zenith2/Docs/Design/CHAR.md`) runs the full harness on the reference desktop, commits the report under `Zenith2/Docs/Research/MotionMatching/`, reads it with ANIM's harness report, and appends the ruling to `Zenith2/Docs/DecisionLog.md` (Z2-D-084, Z2-D-097).

## Walkthrough character workflow

The timed usability walkthrough of the complete character workflow (inventory item Z2-I-CHAR-16; row `Z2-B-CHAR-001`; [Z2-N-BUDGET-325], [Z2-N-BUDGET-326]), run by a person with MP-WALKTHROUGH on the reference desktop with a fresh checkout of CinematicSlice at the starting state, under EDIT's walkthrough rules (`Zenith2/Docs/Design/EDIT/Automation.md → "Walkthrough rules"`): a stopwatch runs from step 1 to the last; the run fails when the time exceeds the row's value plus its tolerance, when a step cannot be completed as written, or when the check fails. The step list is committed as `Zenith2/Docs/Research/Walkthroughs/CharacterWorkflow.md` by the row's producer, the Stage 1 per-system budget ticket for CHAR, from this list.

**Starting state.** CinematicSlice open in `ZenithEditor` on its walkthrough scene, with this content committed:

| Content | Paths (game-relative) | Producer |
|---|---|---|
| the character source: a skinned humanoid with Mixamo-style joint names (`Hips`, `LeftUpLeg`, …), the 62-joint layout of `Zenith_CharMakeHumanoidGltf`, six region meshes `Head`, `Torso`, `Arms`, `Hands`, `Legs`, `Feet`, a `Coat` and a `HairCards` mesh, thirteen ARKit-style morph targets on `Head`, no animation, and no `.meta` | `Assets/Characters/Walkthrough/Ranger.glb` | the `needs-human` content-production ticket below |
| the library rig: the same layout with Zenith-style names and one skinned proxy mesh (a glTF skeleton comes from a skin), the seven clips of the locomotion table with its markers and events in its `.meta` (label `char-library-rig` only: a library rig is not a character source, "Validation catalogue" CV14 of `Zenith2/Docs/Design/CHAR/Debugging.md`), and its rig document | `Assets/Characters/Library/Humanoid.glb`, `.glb.meta`, `Humanoid.animrig.json` | the same ticket |
| three dialogue lines `Walkthrough.Line01` … `Line03`, 1 to 2 s each, in `en` and `fr`, 48 kHz mono PCM16 | `Assets/Audio/Dialogue/en/Ranger_Line01.wav` … `fr/Ranger_Line03.wav` | the same ticket |
| the dialogue bank naming the six sounds, `en` listed first in every line, and two markers on the sound of line 1 in `en` with the lip-sync phrase-start flag (bit 1 of AUDIO's markers record), `phrase0` at 0.0 s and `phrase1` at 0.5 s, given in the bank document, one of the two marker sources of `Zenith2/Docs/Design/AUDIO/Formats.md → "Markers and loop points section"` | `Assets/Audio/Banks/Walkthrough.bank.json` | the same ticket |
| the project document: `voiceLocales` `["en", "fr"]`, the three default classes, `hero.libraryRig` the library rig's skeleton product | `Assets/Characters/CinematicSlice.charproject.json` | the same ticket |
| the walkthrough scene: entities `Floor`, `Camera`, `Sun`, `PlayerStart` and `DialogueTrigger`; `DialogueTrigger` carries the data-defined component `CinematicSlice.WalkthroughDialogue` (schema `Schemas/WalkthroughDialogue.schema.json`: `Speaker` entity, `Clip` asset of kind `AnimationClip`, `Line` string `Walkthrough.Line01`, `VoiceLocale` string `en`, `Play` bool false) and the behaviour `Scripts/Character/WalkthroughDialogue.luau` | `Scenes/CharacterWalkthrough.scene.json`, the schema, the script | the same ticket |

The behaviour (`Zenith2/Docs/Scripting.md → "Behaviour declaration"`; phase `Late`; `Subscribes` `Zenith.Animation.Notify`) is the scene's test interaction: on the first tick on which `Play` is true it plays `Line` on `Speaker` by "Lip-sync playback contract" of `Zenith2/Docs/Design/CHAR/Facial.md` (the window of `VoiceLocale` in `Clip`, `Audio.PlayDialogue` time-locked, the full-rate slot tag) and sets `Play` false; on the first `char.lipsync.windowEnd` of that play on `Speaker` it stops the slot.

<!-- elaboration -->
The content-production ticket is `needs-human` because its deliverables are binary (Q55: workers never commit binary files) and their usability value is a person's judgement: it is CINE's, the owner of CinematicSlice (`Zenith2/Docs/Design/CINE.md → "Purpose and scope"`), filed in CINE's A5 batch (cross-area row CH19) against the specification above, and CHAR's timed walkthrough ticket is blocked by it. The rehearsal never uses this content: it runs on sources generated at test time ("Walkthrough fixtures").
<!-- /elaboration -->

Steps:

1. Import `Ranger.glb` with the character import wizard, class `hero`; confirm the report shows facing `ok`, one skeleton and a height inside the hero range [1.4, 2.2] m.
2. Rig: accept the detected profile with the facial template; generate the default ragdoll.
3. Retarget: create the retarget from the library rig with every library clip; wait for the retargeted clips to appear.
4. Graph: instantiate `char.humanoidLocomotion` with `facial`; open it in the graph editor and change the Move blend's run sample to 5.5.
5. Preview: run `walk-run-stop` and `jump`; scrub the walk in the preview and see the `Footstep` events at 0 and 0.5 s of each cycle.
6. Lip-sync: create the lip-sync document for the walkthrough bank and the Ranger skeleton (line 1's `en` entry shows two phrase markers, at 0.0 and 0.5 s, from the bank's markers); audition line 1 in `en` and in `fr`; drag line 1's second `en` phrase marker from 0.5 s to 0.6 s.
7. Assemble: create the assembly (it seeds the six region parts, the coat and the hair cards); set the coat's hidden regions to Torso and Arms; create a loadout `Travel` with every part; generate the prefabs; the validation panel shows no error and no warning.
8. In game: place the `Travel` prefab at the player start; drag the placed `Ranger` onto `DialogueTrigger`'s `Speaker` field and line 1's lip-sync clip onto its `Clip` field; play; drive `walk-run-stop` with the workflow panel's Drive button; tick `DialogueTrigger`'s `Play` in the inspector and watch the face speak line 1; stop; save the scene.

Check: the saved scene re-opens with the character and both references; it plays and binds; the validation panel shows no error; the lip-sync clip of line 1 exists with L = 2 windows (`voiceLocales` `["en", "fr"]`), both non-silent.

## Walkthrough rehearsal

`Zenith2/Tools/ZenithEditor/Tests/Fixtures/Scripts/Walkthroughs/CharacterWorkflow.editorscript.json` (EDIT's walkthrough script directory, `Zenith2/Docs/Design/EDIT/Automation.md → "Walkthrough rehearsal tests"`; `game` `EditorFixture`, `seed` 1) performs the same steps through commands on the generated content of "Walkthrough fixtures" (EDIT's automation script format, `Zenith2/Docs/Design/EDIT/Automation.md → "Automation script format"`), every argument a literal of "Command argument forms". `Test_Walkthrough_CharacterWorkflow_Rehearsal` (`Zenith2/Tools/ZenithEditor/Tests/Character/Zenith_CharWalkthrough_Tests.cpp`, suite `z2-edit-cooked`, CTest `FIXTURES_REQUIRED CharWalkthroughSources`) launches a child editor `--headless --game ${ZENITH_OUT}/test-fixtures/CHAR/Games/EditorFixture --automation-script <script> --exit-after-script --random-seed 1` and asserts exit code 0 and the walkthrough's check. Paths below abbreviate `Assets/Characters/Walkthrough/` as `W/`; the script writes them in full:

| # | Steps | Assert |
|---|---|---|
| 1 | `command character.import { source: "W/Ranger.glb", class: "hero" }`; `waitEvent characterImported { match: { source: "W/Ranger.glb" } }` | `assertImportReport { source: "W/Ranger.glb", facing: "ok", skeletons: 1, heightMinMetres: 1.4, heightMaxMetres: 2.2 }` (the fixture's height is 1.68 m) |
| 2 | `command character.rig.applyProfile { skeleton: "W/Ranger.glb#skel:Hips", rigDocument: "W/Ranger.animrig.json", facial: true }`; `command character.ragdoll.generateDefault { skeleton: "W/Ranger.glb#skel:Hips", path: "W/Ranger.ragdoll.json", class: "hero" }`; `waitEvent cookIdle` | `assertCharacterValid { paths: ["W/Ranger.animrig.json", "W/Ranger.ragdoll.json"], maxWarnings: 0 }` |
| 3 | `command character.retarget.create { sourceSkeleton: "Assets/Characters/Library/Humanoid.glb#skel:hips", targetSkeleton: "W/Ranger.glb#skel:Hips", path: "W/Ranger.animretarget.json" }`; `waitEvent cookIdle` | `assertCharacterValid { paths: ["W/Ranger.animretarget.json"], maxWarnings: 0 }` (no CV16 warning: both rigs share one rest layout) |
| 4 | `command character.graph.instantiateTemplate { template: "char.humanoidLocomotion", skeleton: "W/Ranger.glb#skel:Hips", path: "W/Ranger.animgraph.json", facial: true }`; `command character.graph.setBlendSample { graph: "W/Ranger.animgraph.json", state: "Move", sample: 2, value: 5.5 }`; `waitEvent cookIdle` | `assert noDiagnostics { source: "anim.graph" }` |
| 5 | `characterScenario { graph: "W/Ranger.animgraph.json", scenario: "walk-run-stop" }`; `characterScenario { graph: "W/Ranger.animgraph.json", scenario: "jump" }` | both complete (the run sample at 5.5 keeps `Speed` 6.0 in Move) |
| 6 | `command character.lipSync.create { bank: "Assets/Audio/Banks/Walkthrough.bank.json", skeleton: "W/Ranger.glb#skel:Hips", path: "Assets/Audio/Dialogue/Walkthrough.lipsync.json" }`; `waitEvent cookIdle`; `assertLipSyncEntry { document: "Assets/Audio/Dialogue/Walkthrough.lipsync.json", line: "Walkthrough.Line01", locale: "en", phrases: [0.0, 0.5] }` (from the bank's markers); `command character.lipSync.audition { document: "Assets/Audio/Dialogue/Walkthrough.lipsync.json", line: "Walkthrough.Line01", locale: "en" }`; the same for `fr`; `command character.lipSync.setPhrases { document: …, line: "Walkthrough.Line01", locale: "en", phrases: [0.0, 0.6] }`; `waitEvent cookIdle` | both audition commands succeed; `assertCharacterValid { paths: ["Assets/Audio/Dialogue/Walkthrough.lipsync.json"], maxWarnings: 0 }` |
| 7 | `command character.assembly.create { skeleton: "W/Ranger.glb#skel:Hips", graph: "W/Ranger.animgraph.json", path: "W/Ranger.character.json", class: "hero", ragdoll: "W/Ranger.ragdoll.json" }`; `command character.assembly.setPartRegions { assembly: "W/Ranger.character.json", part: "Coat", hidesRegions: ["Torso", "Arms"] }`; `command character.assembly.setLoadout { assembly: "W/Ranger.character.json", name: "Travel", parts: ["Head", "Torso", "Arms", "Hands", "Legs", "Feet", "Coat", "HairCards"] }`; `command character.assembly.generatePrefabs { assembly: "W/Ranger.character.json" }`; `waitEvent characterPrefabsGenerated { match: { refused: 0, written: 2 } }` | `assertCharacterValid { maxWarnings: 0 }` over the whole game (derivation below) |
| 8 | `openScene { path: "Scenes/CharacterWalkthrough.scene.json" }`; `command character.scene.placePrefab { scene: "Scenes/CharacterWalkthrough.scene.json", prefab: "W/Ranger.Travel.prefab.json", id: "c4a1f2d0-5b6e-4c3a-9d8e-7f0a1b2c3d4e", position: "$entity:/PlayerStart" }`; `command character.scene.setReference { scene: …, entity: "/DialogueTrigger", type: "EditorFixture.WalkthroughDialogue", path: "Speaker", target: "/Ranger" }`; the same with `path: "Clip"`, `target: "Assets/Audio/Dialogue/Walkthrough.lipsync.json#anim:lip.Walkthrough.Line01"`; `play`; `characterDrive { entity: "/Ranger", timeline: "walk-run-stop", waitToEnd: true }`; `setProperty { entity: "/DialogueTrigger", type: "EditorFixture.WalkthroughDialogue", path: "Play", value: true }` (a play-world edit, `Zenith2/Docs/Design/EDIT/Foundation.md → "Play-world edits"`); `waitTicks { count: 3, world: "play" }` | during play, `assert entityCount { world: "play", equals: 14 }` (the scene's 5 entities, the character's root and 8 parts) and, after the wait, `assertCharacterSlot { entity: "/Ranger", slot: "char.lipsync", clip: "Assets/Audio/Dialogue/Walkthrough.lipsync.json#anim:lip.Walkthrough.Line01", startTime: 0 }` (window 0, `en`) |
| 9 | `stop`; `command document.save { path: "Scenes/CharacterWalkthrough.scene.json" }`; `exit { documents: "save" }` | the walkthrough's check, asserted by the test after the script: the scene re-opens, the instance resolves with both references, and the product of line 1 has the markers `char.lipsync.en` at 0 and `char.lipsync.fr` at S and two `char.lipsync.windowEnd` events, both windows holding non-rest keys |

Why step 7 expects no warning: the Default loadout holds every region part and the hair, Travel holds the coat over Torso and Arms and the other region parts, so no region has a hole or an overlap (MV7); every part is in a loadout (MV11); the hair material is alpha-tested and two-sided and the part casts shadows (HC1, HC2, HC5; HC3 is inactive before Stage 5) in an exclusive slot (HC4); the import wizard wrote the labels `character` and `character-hero` on `Ranger.glb`, the one character source (CV14: a character source is one whose `.meta` carries `character`; the library rig `Humanoid.glb`, labelled `char-library-rig` alone, is not one, and EditorFixture's own content carries neither label); the generator wrote the hero class's bias and full-rate flag (CB1, AU9); the root has no controller and no collider (AU2, AU3, AU6 do not apply); the graph has the five template slots (CV9); the rigs are complete, symmetric, unscaled and T-posed (CV1 to CV3, CV15, CV16); the lip-sync document is synced and its bank lists `en` first (CV10, CV12, CV18); the three default class labels have their `CHARACTER_CLASS_*` rows in `Zenith2/Docs/ContentSpecs.rules.json` (CV17; core-doc items X330, X340). `Test_CharAssemblyValidation_CleanFixturePasses` asserts the same zero on the same assembly.

The rehearsal covers every human step by a command except pure gestures: scrubbing the preview and reading the footstep events (step 5) are EDIT's preview and timeline widget interactions, rehearsed by EDIT's authoring-tool tests (`Zenith2/Docs/Design/EDIT/Tests.md → "Authoring tool tests"`); the graph-editor field edit, the phrase drag, the audition, the reference drags and the inspector tick have the command forms above. The timed human run is the Stage 4 acceptance evidence (`needs-human`); the rehearsal is its precondition, run in the `Test` step.

## Walkthrough fixtures

The setup case `Test_CharWalkthrough_SetupWritesSources` (`ZenithEditor_Tests`, doctest suite `z2-fixture-setup`, CTest `FIXTURES_SETUP CharWalkthroughSources` with `FIXTURES_REQUIRED EditEditorFixtureGames`, EDIT's set of `Zenith2/Docs/Design/EDIT/Tests.md → "Fixtures"`) copies EDIT's scratch `Games/EditorFixture/` into `${ZENITH_OUT}/test-fixtures/CHAR/Games/EditorFixture/` and writes into the copy, from the declared constants of `Zenith2/Tools/Authoring/Tests/Support/Zenith_CharTestFixtures.h` ("Fixtures and generators" of `Zenith2/Docs/Design/CHAR/Tests.md`), the starting state's content at the same game-relative paths:

| Content | Generator |
|---|---|
| `Assets/Characters/Walkthrough/Ranger.glb` | `Zenith_CharMakeHumanoidGltf(CHAR_FIXTURE_NAMING_MIXAMO, CHAR_FIXTURE_MESHES_WALKTHROUGH, 13, false)` |
| `Assets/Characters/Library/Humanoid.glb` with its `.meta` (label `char-library-rig`, the clip annotations of the locomotion table) and `Humanoid.animrig.json` | `Zenith_CharMakeHumanoidGltf(CHAR_FIXTURE_NAMING_ZENITH, CHAR_FIXTURE_MESHES_PROXY, 0, true)`, `Zenith_CharMakeLibraryMeta`, `Zenith_CharMakeRigDocument` |
| the six WAVs (line 1: `CHAR_SIGNAL_VOWEL_AA` 1.0 s; line 2: `CHAR_SIGNAL_BURST` 1.5 s; line 3: `CHAR_SIGNAL_FRICATIVE` then `CHAR_SIGNAL_VOWEL_OU`, 1.0 s each; the `fr` files the same signals with the LCG seed + 1) | `Zenith_CharMakeSignal`, `Zenith_CharMakeWav` |
| the bank, `en` first in every line, with the two phrase markers of line 1 in `en` (0.0 and 0.5 s, flag bit 1) | `Zenith_CharMakeBankDocument` |
| `Assets/Characters/EditorFixture.charproject.json` (`voiceLocales` `["en", "fr"]`, the three default classes, `hero.libraryRig` the library skeleton product, `xCHAR_FIXTURE_LIBRARY_SKELETON_GUID`, which the setup lists in the library rig `.meta`'s `subAssets` and names through `Zenith_CharFixtureProductID`) | `Zenith_CharMakeProjectDocument` |
| the walkthrough scene, the `EditorFixture.WalkthroughDialogue` schema and `Scripts/Character/WalkthroughDialogue.luau` | text constants in `Zenith_CharTestFixtures.h` |

<!-- assumption: pending EDIT (row CH22) -->
The copy's `composition.json` is EditorFixture's own, which the editor's play runtime is built from (`Zenith2/Docs/Design/EDIT/Foundation.md → "Editor composition"`); a copy cannot add a module, because a changed `game.json` makes the composition record stale. Row CH22 asks EDIT to add `"Audio"` to EditorFixture's `modules` (today `["Animation", "Environment", "Gameplay", "Physics"]`, `Zenith2/Docs/Design/EDIT/Tests.md → "Fixtures"`), so the dialogue behaviour's `Audio.PlayDialogue` and `Audio.StopInstance` have AUDIO's bindings in the rehearsal's play world. Until it lands, the rehearsal ticket stays `draft`.
<!-- /assumption -->

Nothing binary is committed (Q55): the binaries are written at test time and the text files from constants, so the content is identical on every run. The rehearsal test requires the set; `Test_CharWalkthrough_SetupWritesSources` asserts each file exists, each `.glb` parses with CONTENT's glTF parser with the expected joint (62), mesh (8 and 1) and animation (0 and 7) counts, and each WAV decodes to its generator's frame count.
