# EDIT requirement catalogue: Stages 4 to 7

## Purpose

The second file of the EDIT catalogue: the requirements whose `Stage` is S4, S5, S6 or S7, in the same format and the same `Z2-R-EDIT-NNN` numbering as `Zenith2/Docs/Requirements/EDIT.md`, which holds the purpose of the whole catalogue, the requirements of Stages 1 to 3, the coverage of inventory items of both files and the exclusions.

## Requirements

### Z2-R-EDIT-033 Unchanged documents write nothing
- **Statement:** Saving a clean document must perform no storage write, and saving after an edit must change only the edited objects' lines.
- **Covers:** Z2-I-EDIT-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorDocuments_SaveUnchangedWritesNothing`, `Test_EditorSave_InsertChangesOnlyNewLines` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Rules for files the editor writes"

### Z2-R-EDIT-070 Prefab root picking
- **Statement:** A click on an entity of a prefab instance must select the instance root first and the entity on a second click on the same pixel within 500 ms, or at once with Alt.
- **Covers:** Z2-I-EDIT-02, Z2-I-EDIT-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorPicking_PrefabRootThenChild` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Click picking"

### Z2-R-EDIT-086 Panel and extension interfaces
- **Statement:** Panels and extensions must plug in through the static extension table and interfaces, with duplicate registrations refused and the panel and command limits enforced.
- **Covers:** Z2-I-EDIT-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-075
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorCommands_DumpListsEveryCommand` in the Test gate step (`ZenithEditor_Tests`, `win`); review of extension diffs
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "Panel interface"; `Zenith2/Docs/Design/EDIT/Panels.md` → "Extension interface"; `Zenith2/Docs/Design/EDIT/Panels.md` → "Extension table"

### Z2-R-EDIT-087 Games extend the editor through data
- **Statement:** Games must extend the editor only through data: a schema's editor hints drive the generic inspector, and automation scripts drive commands.
- **Covers:** Z2-I-EDIT-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-002, Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorInspector_DataDefinedAndCppAlike`, `Test_EditorInspector_RangeClamp` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "Extension interface"; `Zenith2/Docs/Design/EDIT/Panels.md` → "Inspector data source"

### Z2-R-EDIT-088 Area-owned panels follow the extension contract
- **Statement:** A panel another area specifies must be implemented in its Internal area directory against the extension interface, change documents only through commands and carry its tests under its Tests area directory.
- **Covers:** Z2-I-EDIT-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** review of each area-owned panel diff; the area tests in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "Area-owned panels"

### Z2-R-EDIT-093 Schema panel validation
- **Statement:** The schema model must validate every edited schema into a scratch registry and show the validator's exact messages with positions, applying nothing.
- **Covers:** Z2-I-EDIT-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSchemaPanel_ValidationShowsMessages` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "Schema panel workflow"; `Zenith2/Docs/Design/EDIT/Declarations.md` → "Schema model header"

### Z2-R-EDIT-094 Schema re-registration between play sessions
- **Statement:** A schema change must be applied only while no play world exists, as one asynchronous record that migrates the documents with backup, re-registers the schema inside the hot-reload window, revises the edit worlds' rows and reconciles, or restores the documents on a refusal.
- **Covers:** Z2-I-EDIT-15, Z2-I-EDIT-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSchemaPanel_ApplyRefusedDuringPlay` in the Test gate step (`ZenithEditor_Tests`, `win`); `Test_EditorSchemaPanel_ApplyMigratesDocumentsAndWorld`, `Test_EditorSchemaPanel_WindowRefusalRestores` in the Test gate step (`ZenithEditor_Tests`, `z2-edit-cooked`, `win`); `Test_EditorSchema_ChangeMigratesEditWorld` in `Zenith_Integration_EditorSchema` (Z2T-ECS-100); `Test_EditorIntegration_SchemaChangeEndToEnd` in the Test gate step (`ZenithEditor_Tests`, `z2-edit-cooked`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "Schema panel workflow"; `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Schema change"

### Z2-R-EDIT-095 Schema change undo
- **Statement:** Undoing a schema change must restore the schema and document bytes with the record's state IDs, the schema file's open document included as a target of the batch, request a runtime restart that the run loop performs between frames, and re-project the edit worlds under the old schema, keeping every older record undoable (the schema file's earlier edits included), and redo must restore the new state by the same procedure.
- **Covers:** Z2-I-EDIT-01, Z2-I-EDIT-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSchema_UndoRestoresDocumentsAndRows` in the Test gate step (`ZenithEditor_Tests`, `z2-edit-cooked`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Schema undo"; `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Runtime restart"; `Zenith2/Docs/Design/EDIT/Panels.md` → "Schema panel workflow"

### Z2-R-EDIT-112 Rename and move keep GUIDs
- **Statement:** Rename and move must keep the GUID, rename the .meta and the source under the storage root's writer lock, repair prefab hints in one batch and be one asynchronous record whose failure at any step restores both files and the hints.
- **Covers:** Z2-I-EDIT-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorAssetOps_RenameRepairsHints`, `Test_EditorAssetOps_RenameFailureRollsBack`, `Test_EditorAssetOps_RenameTakesWriterLock`, `Test_EditorAssetOps_RenameServerRestartCompensates` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Rename and move"

### Z2-R-EDIT-113 Delete into the editor trash
- **Statement:** Delete must move the source and .meta into the editor trash with a manifest, repair references per the chosen option, be undoable, and permanently remove files only through the empty-trash command.
- **Covers:** Z2-I-EDIT-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorAssetOps_DeleteMovesToTrash` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Delete and the editor trash"; `Zenith2/Docs/Design/EDIT/Formats.md` → "Editor trash layout"

### Z2-R-EDIT-114 Create and duplicate
- **Statement:** Creating an asset document must save it as a new document then assign its GUID and import it, and a duplicate must take a fresh document ID and GUID.
- **Covers:** Z2-I-EDIT-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorAssetOps_DuplicateGetsFreshIDs` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Create and duplicate"

### Z2-R-EDIT-115 Large repairs through the batch operation
- **Statement:** An asset operation touching more than editor.assetOps.batchThreshold documents must run CONTENT's recoverable batch operation on its own thread with its cook requests forwarded to the main thread, remain one history record whose undo is the batch's Rollback, and leave open clean target documents clean with their history.
- **Covers:** Z2-I-EDIT-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorAssetOps_LargeBatchUsesBatchOperation` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Reference repair"

### Z2-R-EDIT-125 Create prefab from selection
- **Statement:** Creating a prefab must move the selection into a new prefab document and replace it by an instance so outside references still resolve, in one transaction.
- **Covers:** Z2-I-EDIT-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorPrefab_CreateFromSelectionKeepsReferences` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Creating a prefab"

### Z2-R-EDIT-126 Prefab stage
- **Statement:** Opening a prefab must enter a prefab stage edit world projecting it alone, and saving the prefab must reconcile every instance in the main edit world.
- **Covers:** Z2-I-EDIT-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorPrefab_SaveReconcilesInstances` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Prefab stage"

### Z2-R-EDIT-127 Overrides and variants
- **Statement:** Edits of instance entities must become overrides, variants must take their own overrides, and revert, apply-to-prefab, unpack and replace must produce SCENE's edits, refusing read-only prefabs.
- **Covers:** Z2-I-EDIT-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorInspector_OverrideRevertApply`, `Test_EditorPrefab_VariantOverridesOnly`, `Test_EditorPrefab_ApplyToReadOnlyRefused` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Instances overrides and variants"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Prefab apply and revert"; `Zenith2/Docs/Design/EDIT/Panels.md` → "Prefab override display"

### Z2-R-EDIT-135 Environment inspectors validate
- **Statement:** Every environment inspector change must be validated by the document's reader before commit, and time-of-day and weather scrubbing must write only the edit world's environment resource, never a document or a record.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorJsonDocumentEditor_ValidatorRejectsChange`, `Test_EditorEnvironment_TimeOfDayNotRecorded` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Environment inspectors"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Time of day and weather scrubbing"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Edit-world-only writes"

### Z2-R-EDIT-137 Bake panel through the cook server
- **Statement:** Bakes must run out of process as cook server requests with progress and diagnostics, never blocking the editor.
- **Covers:** Z2-I-EDIT-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorBakePanel_CookRequestForOutputs` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Bake panel"

### Z2-R-EDIT-138 VFX inspector and preview
- **Statement:** Effects must be inspected through the VFX component inspector, played in place by edit-world-only writes the play copy replaces, viewed by Zenith.VFX.Viewer components on the scene views' proxy entities, and previewed in a preview world of purpose WORLD_PURPOSE_PREVIEW.
- **Covers:** Z2-I-EDIT-06
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorPreviewHost_PreviewRequestsPerPanel` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "VFX tools"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor proxy entities"

### Z2-R-EDIT-139 Reusable authoring widgets
- **Statement:** The graph canvas, timeline, curve editor and JSON document editor must turn every gesture into commands of the caller's model and keep no document state of their own.
- **Covers:** Z2-I-EDIT-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorGraphCanvas_ConnectRefusesIncompatible`, `Test_EditorGraphCanvas_PasteRemapsNodeIDs`, `Test_EditorTimeline_SnapsToFramesAndKeys`, `Test_EditorCurveEditor_AutoTangents`, `Test_EditorJsonDocumentEditor_ValidatorRejectsChange` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Graph canvas"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Timeline"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Curve editor"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "JSON document editor"

### Z2-R-EDIT-140 Animation panels
- **Statement:** The graph, blend space, mask, rig, clip timeline and retarget editors must edit ANIM's documents through JSON edits, compile graphs in memory for diagnostics and drive the preview.
- **Covers:** Z2-I-ANIM-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorAnimation_GraphEditCompilesAndPreviews`, `Test_EditorAnimation_NodePositionsInEditorMember` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Animation graph editor"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Blend space editor"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Mask editor"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Rig setup"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Clip timeline"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Retarget setup"; `Zenith2/Docs/Design/EDIT.md` → "Tool-side code the editor needs"

### Z2-R-EDIT-141 Animation preview and debugger
- **Statement:** The animation preview must pose its preview world through Zenith_AnimationPreview with parameter, trigger, slot and scrub controls, and the live debugger must show a play-world character's debug state.
- **Covers:** Z2-I-ANIM-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorAnimation_GraphEditCompilesAndPreviews` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Animation preview"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Live animation debugger"

### Z2-R-EDIT-142 UI authoring tool
- **Statement:** The UI authoring tool must record every gesture as one UI model operation with its inverse, lay out a preview context at the chosen surface settings and pick elements by HitTest.
- **Covers:** Z2-I-UI-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorUI_ModelOperationsThroughHistory` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "UI authoring tool"

### Z2-R-EDIT-143 Audio panel hosting
- **Statement:** AUDIO's panels must be hosted with document undo, the timeline and curve widgets, asset drag and drop, and access to the play world's audio system.
- **Covers:** Z2-I-AUDIO-22
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorAudioHosting_PanelsReachSystem` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Audio tools"

### Z2-R-EDIT-144 Project settings through the scaffold
- **Statement:** The project settings panel must edit game.json only through MOD's validator and canonical writer as one undoable record and state that a rebuild and restart are needed.
- **Covers:** Z2-I-EDIT-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-002, Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorProjectSettings_WritesThroughScaffold` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "Project settings panel"

### Z2-R-EDIT-145 New project wizard
- **Statement:** The wizard must create games only through ZenithNewGameScaffold, show the plan and the validator messages verbatim, add no rule of its own, and tell the author how the new game builds.
- **Covers:** Z2-I-MOD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Test_NewProjectWizard_MatchesCli`, `Test_NewProjectWizard_ShowsValidatorMessagesVerbatim` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "New project wizard"

### Z2-R-EDIT-146 Bindings editor
- **Statement:** The bindings editor must validate with PLAT's validator and save through the byte-stable bindings writer as a document write.
- **Covers:** Z2-I-EDIT-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorBindings_SavesThroughCanonicalWriter` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "Input bindings editor"

### Z2-R-EDIT-147 Upgrade preview
- **Statement:** The migration panel must plan every storage root without writing and show, per entry, the versions, the diff hunks, the changed paths, every unknown field kept and every dangling override, with blocked entries first.
- **Covers:** Z2-I-EDIT-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorMigration_PlanOnOpenIsReadOnly`, `Test_EditorMigration_BlockedPlanRefused` in the Test gate step (`ZenithEditorCore_Tests`, `win`); `Test_EditorMigrationPanel_ShowsPlanEntries` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Planning on open"; `Zenith2/Docs/Design/EDIT/Panels.md` → "Migration panel"

### Z2-R-EDIT-148 Backup before migration
- **Statement:** Applying a migration must write the backup batch before any document and the documents through the journaled write inside the writer lock, as one session-undoable record, re-planning on a stale plan.
- **Covers:** Z2-I-EDIT-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorMigration_ApplyBacksUpThenWrites`, `Test_EditorMigration_StalePlanReplans`, `Test_EditorMigration_MetaThroughServerToken` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Applying a migration"

### Z2-R-EDIT-149 Resume after interruption
- **Statement:** Recovery of pending journals must run on every root before any document opens, its report must be shown, and an unrecoverable journal must make only its root read-only.
- **Covers:** Z2-I-EDIT-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorMigration_CorruptJournalMakesRootReadOnly` in the Test gate step (`ZenithEditorCore_Tests`, `win`); `Test_EditorMigration_ResumeAfterKillAtEachJournalState` in the Test gate step (`ZenithEditor_Tests`, `z2-stress`, `z2-edit-cooked`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Resuming after an interruption"

### Z2-R-EDIT-150 Restore a backup batch
- **Statement:** Restoring a backup batch must write its bytes back through one journaled batch with the current digests expected, as one undoable record.
- **Covers:** Z2-I-EDIT-16, Z2-I-EDIT-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorMigration_RestoreBackupBatch` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Restoring a backup"

### Z2-R-EDIT-151 Complete migration coverage
- **Statement:** The migration model must plan, back up, apply and recover every registered document type of every storage root, the .meta documents through the cook server, with fixtures per version for every type.
- **Covers:** Z2-I-EDIT-15, Z2-I-EDIT-16, Z2-I-EDIT-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorIntegration_MigrationUIEndToEnd` in the Test gate step (`ZenithEditor_Tests`, `z2-stress`, `z2-edit-cooked`, `win`); `Test_SceneEditor_EveryDocumentTypeMigrates` in `Zenith_Integration_EditorMigration` (Z2T-SCENE-089)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Migration flow"

### Z2-R-EDIT-152 Autosave schedule
- **Statement:** Every editor.autosave.intervalSeconds, before Play and before a migration, the editor must autosave every dirty document whose state changed, without saving it.
- **Covers:** Z2-I-EDIT-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorAutosave_WritesDirtyOnly`, `Test_EditorAutosave_SaveDeletesAutosave` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Autosave schedule"

### Z2-R-EDIT-153 Autosave write
- **Statement:** Autosaves must be written off the main thread as versioned, digest-protected files under the game's .editor/ root states by temporary file and rename, with a full queue skipping and a failure never blocking editing.
- **Covers:** Z2-I-EDIT-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorAutosave_FileRoundTrip`, `Test_EditorAutosave_DamagedFileRejected`, `Test_EditorAutosave_QueueFullSkips` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Autosave write"; `Zenith2/Docs/Design/EDIT/Formats.md` → "Autosave file"; `Zenith2/Docs/Design/EDIT/Formats.md` → "Recovery manifest"

### Z2-R-EDIT-154 Session lock
- **Statement:** One editor session per game must be enforced by the held session lock file, a second session exiting with code 1 naming the holder.
- **Covers:** Z2-I-EDIT-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSessionLock_SecondEditorRefused` in the Test gate step (`ZenithEditor_Tests`, `z2-edit-cooked`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Session lock and crash detection"; `Zenith2/Docs/Design/EDIT/Formats.md` → "Session lock line"

### Z2-R-EDIT-155 Crash recovery
- **Statement:** After an unclean session, recovery must restore unchanged documents as dirty undoable states, never overwrite a changed file, offer restore as a copy, and keep discarded autosaves until a clean exit.
- **Covers:** Z2-I-EDIT-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorRecovery_ChangedFileRestoresAsCopy` in the Test gate step (`ZenithEditor_Tests`, `z2-edit-cooked`, `win`); `Test_EditorRecovery_DiscardKeepsUntilCleanExit` in the Test gate step (`ZenithEditor_Tests`, `win`); `Test_EditorRecovery_RestoreAfterCrash` in the Test gate step (`ZenithEditor_Tests`, `z2-stress`, `z2-edit-cooked`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Recovery on startup"; `Zenith2/Docs/Design/EDIT/Panels.md` → "Recovery dialog"

### Z2-R-EDIT-156 Git backend
- **Statement:** Source control must run the system git as child processes with argument vectors, no prompts and a 30-second timeout, and hide itself outside a repository.
- **Covers:** Z2-I-EDIT-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSourceControl_NoRepositoryHidesBadges`, `Test_EditorSourceControl_TimeoutTerminatesGit`, `Test_EditorSourceControl_ArgumentsNeverShellInterpreted` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Backend"

### Z2-R-EDIT-157 Status and badges
- **Statement:** Working-tree status, LFS pointer and lock state must be parsed and shown as badges in the hierarchy, asset browser and document tabs.
- **Covers:** Z2-I-EDIT-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSourceControl_ParsesPorcelainV2` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Status"; `Zenith2/Docs/Design/EDIT/Panels.md` → "Source control panel"

### Z2-R-EDIT-158 Diff, revert and conflicts
- **Statement:** Diffs must compare canonicalised working and HEAD bytes; revert must be an undoable journaled document write, and a dirty open document's revert, after its confirmation, must take the HEAD bytes in memory with an undo that restores both the file's pre-revert bytes and the unsaved state with its state ID; a conflicted document must open CONFLICTED with merge-tool, ours and theirs options.
- **Covers:** Z2-I-EDIT-11, Z2-I-EDIT-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSourceControl_DiffCanonicalisesHead`, `Test_EditorSourceControl_RevertIsUndoable`, `Test_EditorSourceControl_ConflictedDocumentOpensConflicted` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Diff revert and resolve"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor-originated writes"; `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Merge conflicts in documents"

### Z2-R-EDIT-159 Commit and locks
- **Statement:** Commit must commit exactly the index with no pathspec, refuse while a staged document is dirty, and LFS-locked binaries must warn before an edit.
- **Covers:** Z2-I-EDIT-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSourceControl_CommitRefusedWhileDirty`, `Test_EditorSourceControl_CommitUsesIndex` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Commit and locks"

### Z2-R-EDIT-160 Owners' canonical writers only
- **Statement:** Every tracked file the editor writes must be written by its owner's canonical writer, with no editor serializer for a tracked file and no per-session value inside it.
- **Covers:** Z2-I-EDIT-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSave_InsertChangesOnlyNewLines` in the Test gate step (`ZenithEditorCore_Tests`, `win`); `Test_EditorBindings_SavesThroughCanonicalWriter`, `Test_EditorProjectSettings_WritesThroughScaffold` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Rules for files the editor writes"

### Z2-R-EDIT-161 Per-user state outside documents
- **Statement:** Session, layout and preferences state must live in untracked per-user files, versioned, with newer versions left untouched and unknown members kept.
- **Covers:** Z2-I-EDIT-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorFormats_SessionFileRoundTrip`, `Test_EditorFormats_NewerVersionUsesDefaults`, `Test_EditorFormats_UnknownMembersKept` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Per-user state stays out of documents"; `Zenith2/Docs/Design/EDIT/Formats.md` → "Session file"; `Zenith2/Docs/Design/EDIT/Formats.md` → "Layout file"; `Zenith2/Docs/Design/EDIT/Formats.md` → "Preferences file"; `Zenith2/Docs/Design/EDIT/Formats.md` → "Common rules"

### Z2-R-EDIT-164 Automation scripts
- **Statement:** Automation scripts must be validated whole before running, seed the session when they carry a seed, run one step per frame, complete a command step when its record commits (an asynchronous record on its commit event) and fail it on its failure event, or at its timeout only after cancelling the record and waiting for its compensation, and write the versioned report.
- **Covers:** Z2-I-EDIT-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorAutomation_ValidatesWholeScriptFirst`, `Test_EditorAutomation_OneStepPerFrame`, `Test_EditorAutomation_WaitEventTimeout`, `Test_EditorAutomation_ReportFormat` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Automation.md` → "Automation script format"; `Zenith2/Docs/Design/EDIT/Automation.md` → "Step kinds"; `Zenith2/Docs/Design/EDIT/Automation.md` → "Waiting and events"; `Zenith2/Docs/Design/EDIT/Automation.md` → "Running a script"; `Zenith2/Docs/Design/EDIT/Automation.md` → "Automation report"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Asynchronous changes"

### Z2-R-EDIT-165 Automation assertions
- **Statement:** A failed assertion must fail its step, record expected and actual values and, with --exit-after-script, end the process with exit code 5.
- **Covers:** Z2-I-EDIT-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorAutomation_AssertionFailureExitsFive` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Automation.md` → "Assertions"

### Z2-R-EDIT-166 Scripts are untrusted
- **Statement:** Automation scripts and live-link lines must be parsed under the default limits, and file-writing steps must be confined to the game directory and the test fixture root.
- **Covers:** Z2-I-EDIT-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorAutomation_WriteFileOutsideGameRefused` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Automation.md` → "Running a script"

### Z2-R-EDIT-167 Live link endpoint security
- **Statement:** The live link must bind loopback only, require the per-session token from a user-only endpoint file, serve one client, rate-limit rejections and exist in the editor tool alone.
- **Covers:** Z2-I-EDIT-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorLiveLink_LoopbackOnly`, `Test_EditorLiveLink_WrongTokenRejected`, `Test_EditorLiveLink_BusyWithScript` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Automation.md` → "Endpoint security"; `Zenith2/Docs/Design/EDIT/Formats.md` → "Endpoint file"

### Z2-R-EDIT-168 Live link protocol
- **Statement:** An authenticated live-link client must run steps through the script runner with results and receive the events it subscribed to.
- **Covers:** Z2-I-EDIT-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorLiveLink_StepsAndEvents` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Automation.md` → "Endpoint protocol"

### Z2-R-EDIT-169 Walkthrough build a scene
- **Statement:** The build-a-scene walkthrough must be completed by a person within Z2-B-EDIT-007 and pass its check, after its automation rehearsal passes.
- **Covers:** Z2-I-EDIT-18
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-EDIT-007
- **Acceptance evidence:** `Test_Walkthrough_BuildScene_Rehearsal` in the Test gate step (`ZenithEditor_Tests`, `z2-stress`, `z2-edit-cooked`, `win`); the `needs-human` Stage 4 walkthrough record (MP-WALKTHROUGH) in the stage acceptance suite
- **Design:** `Zenith2/Docs/Design/EDIT/Automation.md` → "Walkthrough build a scene"; `Zenith2/Docs/Design/EDIT/Automation.md` → "Walkthrough rules"

### Z2-R-EDIT-170 Walkthrough author a prefab variant
- **Statement:** The author-a-prefab-variant walkthrough must be completed by a person within Z2-B-EDIT-008 and pass its check, after its automation rehearsal passes.
- **Covers:** Z2-I-EDIT-19
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-EDIT-008
- **Acceptance evidence:** `Test_Walkthrough_PrefabVariant_Rehearsal` in the Test gate step (`ZenithEditor_Tests`, `z2-stress`, `z2-edit-cooked`, `win`); the `needs-human` Stage 4 walkthrough record in the stage acceptance suite
- **Design:** `Zenith2/Docs/Design/EDIT/Automation.md` → "Walkthrough author a prefab variant"; `Zenith2/Docs/Design/EDIT/Automation.md` → "Walkthrough rules"

### Z2-R-EDIT-171 Walkthrough iterate a script
- **Statement:** The iterate-a-script walkthrough must be completed by a person within Z2-B-EDIT-009 and pass its check, after its automation rehearsal passes.
- **Covers:** Z2-I-EDIT-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-EDIT-009
- **Acceptance evidence:** `Test_Walkthrough_IterateScript_Rehearsal` in the Test gate step (`ZenithEditor_Tests`, `z2-stress`, `z2-edit-cooked`, `win`); the `needs-human` Stage 4 walkthrough record in the stage acceptance suite
- **Design:** `Zenith2/Docs/Design/EDIT/Automation.md` → "Walkthrough iterate a script"; `Zenith2/Docs/Design/EDIT/Automation.md` → "Walkthrough rules"

### Z2-R-EDIT-172 Walkthrough reimport an asset
- **Statement:** The reimport-an-asset walkthrough must be completed by a person within Z2-B-EDIT-010 and pass its check, after its automation rehearsal passes.
- **Covers:** Z2-I-EDIT-21
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-EDIT-010
- **Acceptance evidence:** `Test_Walkthrough_ReimportAsset_Rehearsal` in the Test gate step (`ZenithEditor_Tests`, `z2-stress`, `z2-edit-cooked`, `win`); the `needs-human` Stage 4 walkthrough record in the stage acceptance suite
- **Design:** `Zenith2/Docs/Design/EDIT/Automation.md` → "Walkthrough reimport an asset"; `Zenith2/Docs/Design/EDIT/Automation.md` → "Walkthrough rules"

### Z2-R-EDIT-175 Crash recovery acceptance evidence
- **Statement:** The Stage 4 acceptance must hold the crash recovery evidence: a terminated editor session restored by the next.
- **Covers:** Z2-I-EDIT-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorRecovery_RestoreAfterCrash` in the Stage 4 acceptance suite
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Recovery on startup"; `Zenith2/Docs/Design/EDIT.md` → "Stage 4"

### Z2-R-EDIT-177 Editor on consoles and XR
- **Statement:** The editor targets Windows host tools only; editing for console and XR targets in Stage 7 must reuse the same documents, commands and cook server with a console or XR platform cook, adding no editor build for those platforms.
- **Covers:** Z2-I-EDIT-13
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-071, Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** the Stage 7 console and XR platform tickets' cook of an editor-authored scene for the new platform
- **Design:** `Zenith2/Docs/Design/EDIT.md` → "Stage 7"

### Z2-R-EDIT-178 Collaborative editing of world partitions
- **Statement:** Concurrent editing of one world by several users must be expressed as per-partition document ownership in the document table, delivered with WORLD's partitions.
- **Covers:** Z2-I-EDIT-11
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** WORLD's Stage 7 concurrent-editing tickets with EDIT's per-partition ownership test
- **Design:** `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Tools hosted for other areas"; `Zenith2/Docs/Design/EDIT.md` → "Stage 7"

### Z2-R-EDIT-192 Automation waits on record events
- **Statement:** An automation step or live-link line executing a command whose record is asynchronous must complete on historyCommitted for that record and fail on historyFailed, or at its wait timeout after cancelling the record and waiting for its compensation.
- **Covers:** Z2-I-EDIT-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorAutomation_OneStepPerFrame`, `Test_EditorLiveLink_StepsAndEvents` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Automation.md` → "Running a script"; `Zenith2/Docs/Design/EDIT/Automation.md` → "Waiting and events"

### Z2-R-EDIT-194 Asset file moves under the writer lock
- **Statement:** Every rename, move, delete and restore of a source and its .meta must hold the storage root's writer lock for the moves, released before any server request, and a held lock must fail the operation naming the holder with nothing moved.
- **Covers:** Z2-I-EDIT-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorAssetOps_RenameTakesWriterLock` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Rename and move"; `Zenith2/Docs/Design/EDIT/Declarations.md` → "Storage roots header"

### Z2-R-EDIT-195 Runtime restart keeps the session
- **Statement:** A runtime restart, requested by the history and performed only by the run loop between frames, must tear down every world, texture, extension and registration in the documented order, create the runtime again with the same composition, re-inject the new services by the composition root and re-project the edit worlds, keeping every document's bytes and state ID, the history, the selection and the clipboard.
- **Covers:** Z2-I-EDIT-15, Z2-I-EDIT-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorApp_RestartRuntimeKeepsSession` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Runtime restart"; `Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Application header"

### Z2-R-EDIT-196 A failed runtime restart exits after an autosave
- **Statement:** A runtime restart whose re-creation fails must have autosaved every dirty document first and end the process with exit code 1 through the shutdown sequence.
- **Covers:** Z2-I-EDIT-15, Z2-I-EDIT-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorApp_RestartFailureExitsOneAfterAutosave` in the Test gate step (`ZenithEditor_Tests`, `z2-stress`, `z2-edit-cooked`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Runtime restart"

### Z2-R-EDIT-197 Schema undo keeps older records undoable
- **Statement:** After a schema undo, every older record touching a migrated document must still undo, and the documents must carry the state IDs the schema record stored rather than reloaded ones.
- **Covers:** Z2-I-EDIT-15, Z2-I-EDIT-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSchema_UndoRestoresDocumentsAndRows` in the Test gate step (`ZenithEditor_Tests`, `z2-edit-cooked`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Schema undo"

### Z2-R-EDIT-203 Schema lock through the cook command line
- **Statement:** Committing the schema lock must run ZenithCook schema-lock as a child process without blocking the frame, report exit code 6 as a retryable lock and any other failure with its stderr, and record no history entry.
- **Covers:** Z2-I-EDIT-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSchemaPanel_CommitLockRunsCli` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "Schema panel workflow"; `Zenith2/Docs/Design/EDIT/Declarations.md` → "Schema model header"

### Z2-R-EDIT-204 Migration model excludes meta documents from in-process plans
- **Statement:** The migration model must remove every Zenith.AssetMeta entry from the framework's in-process plans and plan and apply .meta documents through the cook server's migratePreview and migrateApply.
- **Covers:** Z2-I-EDIT-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorMigration_MetaThroughServerToken`, `Test_EditorMigration_PlanOnOpenIsReadOnly` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Planning on open"; `Zenith2/Docs/Design/EDIT/Declarations.md` → "Migration model header"

### Z2-R-EDIT-205 Models drivable without the shell
- **Statement:** The migration and schema flows must be complete in ZenithEditorCore's Zenith_EditorMigrationModel and Zenith_EditorSchemaModel, so a suite that links ZenithEditorCore and creates a session drives them with no shell.
- **Covers:** Z2-I-EDIT-15, Z2-I-EDIT-16, Z2-I-EDIT-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneEditor_EveryDocumentTypeMigrates` in `Zenith_Integration_EditorMigration` (Z2T-SCENE-089); `Test_EditorSchema_ChangeMigratesEditWorld` in `Zenith_Integration_EditorSchema` (Z2T-ECS-100); both in the Test gate step (`win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Declarations.md` → "Migration model header"; `Zenith2/Docs/Design/EDIT/Declarations.md` → "Schema model header"; `Zenith2/Docs/Design/EDIT/Panels.md` → "Migration panel"

### Z2-R-EDIT-208 Editor state directories are admitted untracked
- **Statement:** The editor must write per-user and recovery state only under Games/<Name>/.editor/ (engine content roots' under .editor/roots/<Library>/) from Stage 4, and the W-Editor script only under the game's Workload directory, entries MOD's game-directory allowlist admits, with .editor/ ignored by git.
- **Covers:** Z2-I-EDIT-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** MOD's CMake fixture case `CMake_GameDir_EditorDirectoryAdmitted` (row ED29, MOD follow-up 4) in the Test gate step; `Test_EditorFormats_SessionFileRoundTrip` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT.md` → "Untracked editor files"; `Zenith2/Docs/Design/EDIT/Automation.md` → "Measurement procedure"

### Z2-R-EDIT-209 Drop to rest
- **Statement:** Drop to rest must simulate the selected dynamic bodies and their surroundings for two seconds in PHYS's isolated simulation without stepping the edit world and write the settled poses as one undoable transaction of local-transform edits.
- **Covers:** Z2-I-EDIT-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorPhysics_DropToRestOneRecord` in the Test gate step (`ZenithEditor_Tests`, `z2-edit-cooked`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Drop to rest"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Physics tools"

### Z2-R-EDIT-210 Ragdoll preview hosting
- **Statement:** The editor must host PHYS's ragdoll preview as an area-owned panel over a preview world, with document undo through the command system.
- **Covers:** Z2-I-EDIT-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** PHYS's ragdoll preview panel test under `Tools/ZenithEditor/Tests/Physics/` (`Test_EditorPhysics_RagdollPreviewSteps`, PHYS-ticketed) in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Ragdoll preview"; `Zenith2/Docs/Design/EDIT/Panels.md` → "Area-owned panels"

### Z2-R-EDIT-212 Device loss autosaves and exits six
- **Statement:** When the runtime requests the device-loss exit, the editor must run no further frame, autosave every dirty document within a 7-second deadline, skip the dirty-document prompt, terminate a running git command, the cook server after 500 ms and the shader watcher instead of waiting for them, and return EXIT_CODE_DEVICE_LOST within 9 seconds of the loss, inside the renderer's 10-second window.
- **Covers:** Z2-I-EDIT-08, Z2-I-EDIT-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorApp_DeviceLostAutosavesAndExitsSix` in the Test gate step (`ZenithEditor_Tests`, `editor-render`, `z2-edit-cooked`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Device loss"; `Zenith2/Docs/Design/EDIT.md` → "Failure table"

### Z2-R-EDIT-214 Partial meta migrations are compensated exactly
- **Statement:** A .meta migrateApply interrupted after some items must be resolved to its applied entries and compensated by writing exactly those entries' backup bytes with their post-migration digests expected, leaving every .meta at its original bytes and recording nothing.
- **Covers:** Z2-I-EDIT-15, Z2-I-EDIT-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorMigration_MetaPartialApplyCompensates` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Asynchronous changes"; `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Applying a migration"

### Z2-R-EDIT-216 Repairs and migrations keep open documents and their history
- **Statement:** A rename's or delete's repair, a migration apply, a schema apply, a backup restore and a CONTENT batch must leave every open clean target document clean with its history undoable, and must never reload or overwrite an open dirty document.
- **Covers:** Z2-I-EDIT-05, Z2-I-EDIT-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorAssetOps_RenameWithDirtyReferencerRefused`, `Test_EditorAssetOps_LargeBatchUsesBatchOperation` in the Test gate step (`ZenithEditor_Tests`, `win`); `Test_EditorSchemaPanel_ApplyKeepsOpenDocumentHistory` in the Test gate step (`ZenithEditor_Tests`, `z2-edit-cooked`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor-originated writes"; `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Rename and move"; `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Reference repair"; `Zenith2/Docs/Design/EDIT/Panels.md` → "Schema panel workflow"

### Z2-R-EDIT-220 Editor binary formats registered with PACK
- **Statement:** The autosave format ZAUT and the thumbnail format ZTHB must each have a row in PACK's format registry with its magic, extension, version, compatibility class, specification and the test that refuses another version, added in the diff that introduces the format.
- **Covers:** Z2-I-EDIT-08, Z2-I-EDIT-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorAutosave_DamagedFileRejected`, `Test_EditorFormats_ThumbnailFileRoundTrip` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Formats.md` → "Format registry rows"

### Z2-R-EDIT-222 Runtime restart rebinds platform objects and types
- **Statement:** A runtime restart must release every object the editor borrowed from the old platform before the runtime is destroyed and re-acquire it from the new services (the session clipboard's system clipboard and the ImGui platform adapter's window, text input and clipboard), and re-read every open document under the new type registry with its bytes, state ID and history unchanged.
- **Covers:** Z2-I-EDIT-15, Z2-I-EDIT-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorApp_RestartRebindsPlatformObjects` in the Test gate step (`ZenithEditor_Tests`, `editor-render`, `win`); `Test_EditorApp_RestartRuntimeKeepsSession` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Runtime restart"; `Zenith2/Docs/Design/EDIT/Declarations.md` → "Clipboard header"; `Zenith2/Docs/Design/EDIT/Viewports.md` → "ImGui platform adapter"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Document table"

### Z2-R-EDIT-223 Redo of create, duplicate and import keeps the GUID
- **Statement:** Redoing a create, duplicate or import must move the source and its .meta back from the trash directory its record names and re-import them under the GUID first assigned, failing permanently when the restored .meta carries another GUID, so a redone change built on that GUID never names one that does not resolve.
- **Covers:** Z2-I-EDIT-05, Z2-I-EDIT-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorAssetOps_RedoCreateKeepsGuid`, `Test_EditorTerrain_WizardOneRecordPlacesImportedTerrain` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Asynchronous changes"; `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Create and duplicate"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Terrain creation wizard"; `Zenith2/Docs/Design/EDIT/Formats.md` → "Editor trash layout"
