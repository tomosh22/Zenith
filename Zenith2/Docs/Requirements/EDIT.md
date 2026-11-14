# EDIT requirements

## Purpose

The requirement catalogue of the EDIT area (editor and collaboration): the editor foundation (stable identities used through SCENE, the command system, undo transactions, document ownership, selection), the editor process and its worlds, play in editor, the shell, viewports, cameras, picking, gizmos and snapping, the panels and the extension interface, the asset browser and reimport, previews and thumbnails, hot reload, the authoring-document migration UI, autosave and crash recovery, source control, merge-friendly files, automation and the live link, the authoring tools EDIT builds over other areas' documents, and the usability walkthroughs. Each requirement is one testable obligation, traced to the design family `Zenith2/Docs/Design/EDIT.md` and its split files under `Zenith2/Docs/Design/EDIT/`. A requirement that covers another area's inventory item (terrain sculpting, animation and UI visual authoring, the project wizard, the audio workflow) is EDIT's because EDIT owns the tool; the owning area keeps the document and the operation.

The catalogue is two files: this one holds the purpose, the requirements whose Stage is S1 to S3, the coverage of inventory items of both files and the exclusions; `Zenith2/Docs/Requirements/EDIT/Stage4Plus.md` holds the requirements whose Stage is S4 to S7, in the same format and the same `Z2-R-EDIT-NNN` numbering. `C:/dev/Zenith2-drafts/areas/EDIT/requirements.json` lists both.

## Requirements

### Z2-R-EDIT-001 Editor executable target
- **Statement:** ZenithEditor must be one zenith_add_tool executable in Zenith2/Tools/ZenithEditor/ with SUPPORTED ZENITH_TOOLS_HOST, declaring ZenithEditorCore, ZenithAuthoring and every engine library, module and backend it includes directly, with no root header and no Player linking any of its code.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-024, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** the Libgraph and SourceLayout gate step reports for the target; `Test_EditorCatalogue_CoversDeclaredModules` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT.md` → "ZenithEditor target"; `Zenith2/Docs/Design/EDIT.md` → "Libraries and third-party dependencies"; `Zenith2/Docs/Design/EDIT.md` → "Targets"

### Z2-R-EDIT-002 Editor command line
- **Statement:** The editor must parse its own options (--game, --scene, --automation-script, --automation-report, --exit-after-script, --automation-endpoint, --recovery, --no-cook-server, --layout, --user-dir, --game-root), validate the game name with MOD's rule through Zenith_IsValidGameName, forward every other argument in order to the runtime command line, and exit with code 2 on a missing, unknown or invalid game.
- **Covers:** Z2-I-EDIT-01, Z2-I-EDIT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorCommandLine_ParsesEditorOptions`, `Test_EditorCommandLine_MissingGame_Usage`, `Test_EditorCommandLine_BadGameNameRefused`, `Test_EditorCommandLine_HeadlessScriptResolvesRecoveryDiscard` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Executable and command line"

### Z2-R-EDIT-003 Composition from the game record
- **Statement:** The editor must build its runtime composition from the game's composition.json and its module catalogue, computing the windows composition hash with the C++ form of MOD's algorithm, and refuse to open a game whose recomputed hash differs from the record, with exit code 3.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorComposition_MatchesRecordHash`, `Test_EditorComposition_StaleRecord_Refused`, `Test_EditorComposition_HashMatchesMinimalGolden` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor composition"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Module catalogue"

### Z2-R-EDIT-004 Module catalogue matches the declared libraries
- **Statement:** The module catalogue must hold exactly one row per module library and per module backend library that ZenithEditor declares in DEPENDS, as recorded in declared_deps.json, built from the modules' own descriptors and backend-binding thunks.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorCatalogue_CoversDeclaredModules` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Module catalogue"; `Zenith2/Docs/Design/EDIT.md` → "Dependency growth rule"

### Z2-R-EDIT-005 Startup unwinds on failure
- **Statement:** The editor's main must install the root heap first and dispatch the crash helper second, returning the helper's exit code without starting the editor, and the crash handler the runtime creates at its startup step 7 must carry the same identity { "ZenithEditor", 0 } through Zenith_ToolRuntimeDesc::m_xCrashIdentity; every startup step that fails must unwind every earlier step in reverse, leave no allocation behind, and return the exit code its row names.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorStartup_UnwindsEachStep` in the Test gate step (`ZenithEditor_Tests`, `win`); `Test_EditorStartup_CrashHelperArgumentReachesHelper` in the Test gate step (`ZenithEditor_Tests`, `z2-stress`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Startup sequence"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Runtime tool access"

### Z2-R-EDIT-006 One frame graph for the editor
- **Statement:** The editor must drive the runtime one frame at a time through the runtime tool access, adding no second frame loop and no second frame graph, make each tool-access call only in the hooks the hook table allows, perform a runtime restart only between frames in its run loop, read world inspectors only between RunFrame calls, and receive the script world's halt and reload events on the main thread through the tool hooks only.
- **Covers:** Z2-I-EDIT-10, Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorShell_DrawSnapshotTwoSlots`, `Test_EditorViewports_ExtractorWritesSnapshotRequests`, `Test_EditorPlay_HaltedScriptStopsPlay` in the Test gate step (`ZenithEditor_Tests`, `win`); `Test_EditorFrame_CallsOnlyInLegalHooks` in the Test gate step (`ZenithEditor_Tests`, `editor-render`, `z2-edit-cooked`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor frame"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Runtime tool access"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Tool access calls per hook"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor run loop"

### Z2-R-EDIT-007 Orderly shutdown
- **Statement:** Shutdown must keep running editor frames while a pending asynchronous record settles or its bounded deadline passes, then stop play and prompt for or resolve dirty documents in those frames, then join each editor thread at its documented step, flush and delete clean autosaves, write the per-user files from Stage 4, shut the cook server and the shader watcher down and destroy every world before the runtime, in the documented order.
- **Covers:** Z2-I-EDIT-01, Z2-I-EDIT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorShutdown_OrderAndCleanAutosaves` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Shutdown sequence"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor run loop"

### Z2-R-EDIT-008 Document type registration
- **Statement:** The editor must register every owner's document types into its one migration registry through Zenith_RegisterEditorDocumentTypes, in the order and from the ZenithAuthoring or ZenithScene headers of the registration table, adding each owner's row only after that owner's move, and freeze it before any document opens; it must never migrate a .meta document in process and never open a document of a type no row registers.
- **Covers:** Z2-I-EDIT-15, Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorMigration_MetaThroughServerToken` in the Test gate step (`ZenithEditorCore_Tests`, `win`); `Test_EditorDocumentTypes_RegistrationOrderMatchesAsset` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Storage roots and the migration registry"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor document type registrations"

### Z2-R-EDIT-009 Edit world is a projection of documents
- **Statement:** The edit world must be changed only by projections of document changes applied out of tick, and nothing may read an edit world's component values to write a document.
- **Covers:** Z2-I-EDIT-01, Z2-I-EDIT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorProjection_EachEditKind`, `Test_EditorProjection_OpenSceneEqualsActivation` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Edit world rules"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Edit world projection"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Projection per edit kind"

### Z2-R-EDIT-010 Edit world creation
- **Statement:** Every edit world (the main one and each prefab stage) must be created through the runtime tool access with world purpose WORLD_PURPOSE_EDIT, the ECS edit-world flag and origin WORLD_ORIGIN_STARTUP, and must never be the input world; preview worlds must have purpose WORLD_PURPOSE_PREVIEW and play worlds WORLD_PURPOSE_PLAY with origin WORLD_ORIGIN_EDITOR_PLAY.
- **Covers:** Z2-I-EDIT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorWorlds_EditWorldFlagAndPurpose`, `Test_EditorWorlds_AnimateToggleUsesEditorReason` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "World kinds"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Edit world rules"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Runtime tool access"

### Z2-R-EDIT-011 No gameplay simulation in edit worlds
- **Statement:** In a world of purpose WORLD_PURPOSE_EDIT or WORLD_PURPOSE_PREVIEW no script behaviour, physics step, AI update or gameplay-framework system may run, the world purpose read through Zenith_WorldAttachContext::GetWorldPurpose and never through the world origin.
- **Covers:** Z2-I-EDIT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorWorlds_NoScriptOrPhysicsStepInEditWorld` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Module behaviour in edit and preview worlds"

### Z2-R-EDIT-012 Projection buffer splitting and errors
- **Statement:** A projection larger than its buffer cap must be applied as consecutive buffers with the same result, and a failed out-of-tick playback must mark the document for reconcile without rolling the history back.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorProjection_BufferSplitAtCap`, `Test_EditorProjection_ErrorMarksReconcile` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Edit world projection"

### Z2-R-EDIT-013 Reconcile restores resolved state
- **Statement:** A reconcile of a scene document must spawn every resolved record absent from the edit world, add, remove or set exactly the components and properties whose encoded bytes differ, reparent entities whose parent differs, destroy every entity of the document the resolved data lacks, and leave the editor proxy entities untouched.
- **Covers:** Z2-I-EDIT-01, Z2-I-EDIT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorProjection_ReconcileRestoresDriftedWorld` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Reconcile algorithm"

### Z2-R-EDIT-014 Unknown components not created in the edit world
- **Statement:** A component whose type the registry does not know must not be created in the edit world and must be kept in the document and on save.
- **Covers:** Z2-I-EDIT-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorProjection_UnknownComponentNotCreated`, `Test_EditorInspector_UnknownComponentReadOnly` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Projection per edit kind"; `Zenith2/Docs/Design/EDIT/Panels.md` → "Unknown and newer data"

### Z2-R-EDIT-015 Preview worlds
- **Statement:** Preview worlds must be created by the preview host with purpose WORLD_PURPOSE_PREVIEW, tick only while their panel is visible, be pooled up to editor.preview.maxWorlds with least-recently-shown eviction, and hold no document projection.
- **Covers:** Z2-I-EDIT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorWorlds_PreviewPoolEvictsLRU`, `Test_EditorPreviewHost_PreviewRequestsPerPanel` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Preview worlds"; `Zenith2/Docs/Design/EDIT/Panels.md` → "Preview host"

### Z2-R-EDIT-016 Play state machine
- **Statement:** Each play world must follow the play state machine table exactly, ignoring and counting every unlisted event, with a script world halt taking effect from the main-thread tool hook and a DEBUGGER reason during a step cancelling the step.
- **Covers:** Z2-I-EDIT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-EDIT-006
- **Acceptance evidence:** `Test_EditorPlay_StateMachineTable`, `Test_EditorPlay_StepCancelledByDebugger` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Play state machine"

### Z2-R-EDIT-017 Play copies the edit world by serialization
- **Statement:** Play must copy the edit world by serialization of the active scene set's document-owned root entities and normalise the copy to the resolved documents, keeping every persistent ID, so the play world's start state is a function of the documents alone; each subsequent edit-world-only write is proven by its own case in the stage that adds it.
- **Covers:** Z2-I-EDIT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-038
- **Budgets:** Z2-B-EDIT-006
- **Acceptance evidence:** `Test_EditorPlay_StartCapturesAndActivates`, `Test_EditorPlay_CaptureExcludesEditWorldOnlyState`, `Test_EditorPlay_StartCooksPlaySet` in the Test gate step (`ZenithEditor_Tests`, `win`); `Test_EditorPlay_CaptureExcludesWeatherScrub` (Stage 4), `Test_EditorPlay_CaptureExcludesInPlaceEffect` (Stage 5) in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Starting play"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Play copy normalisation"

### Z2-R-EDIT-018 Play start failure leaves nothing
- **Statement:** A play-start refusal (a queued schema window, a migration apply, a pending asynchronous record, an unresolved active scene) or a failure (world creation, activation, cook) must leave no play world and no change, returning to EDITING with the error shown.
- **Covers:** Z2-I-EDIT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorPlay_StartFailureLeavesEditing`, `Test_EditorPlay_RefusedWhilePendingOrUnresolved` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Play state machine"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Starting play"

### Z2-R-EDIT-019 Stop leaves the edit world unchanged
- **Statement:** Stopping play must destroy the play world and leave the edit world and every open document byte-identical to their state before Play.
- **Covers:** Z2-I-EDIT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorPlay_StopLeavesEditWorldUnchanged` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Stopping play"

### Z2-R-EDIT-020 Editor pause reason
- **Statement:** The editor's pause and resume must add and remove only WORLD_PAUSE_REASON_EDITOR, so a game-set pause stays applied.
- **Covers:** Z2-I-EDIT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorPlay_PauseUsesEditorReason` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Pause and single step"

### Z2-R-EDIT-021 Single step runs exactly n ticks
- **Statement:** Step n must run exactly n fixed ticks, at least one per frame and at most the frame's unpaused tick count T, without consuming or filling the accumulator, the accumulator restarting from 0 at Resume.
- **Covers:** Z2-I-EDIT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorPlay_StepRunsExactlyN`, `Test_EditorPlay_StepCancelledByDebugger` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Pause and single step"

### Z2-R-EDIT-022 Several independent play worlds
- **Statement:** Up to editor.play.maxWorlds play worlds must run from their own play copies with no shared mutable state and a seed that is a function of the edit world's seed alone, never of the play ordinal, each reproducing the tick-state hash of a fresh session's solo run given the same input timeline and the same edits at the same ticks.
- **Covers:** Z2-I-EDIT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorPlay_TwoWorldsIndependent`, `Test_EditorPlay_MaxWorldsRefused` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Several play worlds"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Starting play"

### Z2-R-EDIT-023 Play-world edits are discarded at Stop
- **Statement:** Edits made in a play world must go to that world's own history and never reach a document, and Stop must discard them.
- **Covers:** Z2-I-EDIT-10, Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorHistory_PlayHistorySeparate`, `Test_EditorInspector_PlayModeEditsGoToPlayWorld` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Play-world edits"

### Z2-R-EDIT-024 Apply play values to documents
- **Statement:** Apply to document must copy the selected play-world entities' differing property values into their documents as one undoable record, writing overrides on instance entities.
- **Covers:** Z2-I-EDIT-10, Z2-I-EDIT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorPlay_ApplyToDocumentIsUndoable` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Applying play values to documents"

### Z2-R-EDIT-025 Halted script world stops play
- **Statement:** A play world whose script world halts must go to STOPPING when the runtime delivers the halt through the main-thread tool hook OnWorldHalted, leaving the edit world untouched.
- **Covers:** Z2-I-EDIT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorPlay_HaltedScriptStopsPlay` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Play state machine"

### Z2-R-EDIT-026 Input routing during play
- **Statement:** Only the play world whose game viewport has focus may be the input world, no world may receive input while another panel has focus, and only the reserved play chords may reach the editor while a game viewport is focused.
- **Covers:** Z2-I-EDIT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorPlay_InputRoutingFollowsFocus`, `Test_EditorShell_ClaimantClaimsOutsideGameViewport` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Input routing during play"; `Zenith2/Docs/Design/EDIT/Viewports.md` → "Game viewport input"; `Zenith2/Docs/Design/EDIT/Viewports.md` → "Editor input claimant"

### Z2-R-EDIT-027 One open document per path
- **Statement:** Each document must be opened once per session and addressed by its persistent document ID, never by a pointer kept across frames.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorDocuments_OpenOncePerSession` in the Test gate step (`ZenithEditorCore_Tests`, `win`); `Test_EditorDocuments_RenameKeepsIdentity` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Document table"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Document identity and ownership"

### Z2-R-EDIT-028 Document ownership through commands
- **Statement:** Every change to an open document must go through a command of its session, and no panel or tool may change a document directly.
- **Covers:** Z2-I-EDIT-01, Z2-I-EDIT-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorUndo_EveryCommandRoundTrips` in the Test gate step (`ZenithEditor_Tests`, `win`); review of each panel diff
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Document identity and ownership"; `Zenith2/Docs/Design/EDIT/Panels.md` → "Panel interface"

### Z2-R-EDIT-029 Migrated documents save the plan bytes
- **Statement:** An older document must open migrated in memory, and its next save must write exactly the bytes the migration plan computes for it.
- **Covers:** Z2-I-EDIT-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorDocuments_OpenMigratedThenSave`, `Test_EditorSave_MigratedEqualsPlanBytes` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Document states"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Saving"

### Z2-R-EDIT-030 Newer documents are read-only
- **Statement:** A document written by a newer tool, a document of a read-only root and every document of a read-only session must refuse every edit command before any change, with EDITOR_ERROR_DOCUMENT_READ_ONLY.
- **Covers:** Z2-I-EDIT-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorDocuments_NewerIsReadOnly` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Read-only and newer documents"

### Z2-R-EDIT-031 Journaled saves with expected digests
- **Statement:** Saving must write documents through the root migrator's journaled batch with each document's last disk digest as its expected digest, one batch per root.
- **Covers:** Z2-I-EDIT-12, Z2-I-EDIT-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorDocuments_SaveStaleDigestConflicts` in the Test gate step (`ZenithEditorCore_Tests`, `win`); `Test_EditorTerrainSink_OneBatchPerSave` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Saving"

### Z2-R-EDIT-032 Save failures keep documents dirty
- **Statement:** A save refused by the writer lock, a stale digest or low disk must leave every document of the batch dirty and say why.
- **Covers:** Z2-I-EDIT-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorDocuments_SaveStorageLockedStaysDirty`, `Test_EditorDocuments_SaveLowDiskStaysDirty`, `Test_EditorDocuments_SaveStaleDigestConflicts` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Saving"

### Z2-R-EDIT-034 External changes
- **Statement:** Every change of a source path, reported by the editor's host watchers or the cook server's changed notification, must reach the document table's one external-change entry, which must reload a clean open document and prune only that document's records as closing it does, without a fence, make a dirty document CONFLICTED with keep-mine and take-disk, mark a removed one MISSING, and ignore a second report of the same change and a change whose digest the editor noted for its own write.
- **Covers:** Z2-I-EDIT-11, Z2-I-EDIT-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorDocuments_ExternalChangeReloadsClean`, `Test_EditorDocuments_ExternalChangeConflictsDirty`, `Test_EditorHistory_ReloadPrunesOnlyThatDocument` in the Test gate step (`ZenithEditorCore_Tests`, `win`); `Test_EditorProjection_ExternalReloadReconciles` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "External changes"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Host watchers"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Document table"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Closing a document and its history"

### Z2-R-EDIT-035 JSON edits with exact inverses
- **Statement:** Every non-scene document must be edited through pointer-addressed JSON edits whose application returns an inverse that restores the canonical bytes exactly, leaving the tree unchanged on any error.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorJsonEdit_InverseRestoresBytes`, `Test_EditorJsonEdit_InvalidPointerRefused`, `Test_EditorJsonEdit_KeepsUnknownMemberAnchors` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "JSON edits"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "JSON edit inverses"

### Z2-R-EDIT-036 One record per command
- **Statement:** Executing a command must apply its changes at once and record them as one history record with its label, documents and the selection before and after; a command applying no change must record nothing.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorHistory_ExecuteRecordsOneRecord`, `Test_EditorHistory_NoChangeCommandRecordsNothing` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Command records"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Executing a command"

### Z2-R-EDIT-037 Atomic transactions
- **Statement:** A command that fails at any change must revert every change it applied in reverse order and record nothing.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorHistory_FailedCommandLeavesNoTrace` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Transactions"

### Z2-R-EDIT-038 Undo and redo
- **Statement:** Undo and redo must apply a record's inverses in reverse order and its forward changes in order, restoring each document's state ID and the selection, and an undo or redo of a record holding an asynchronous change must stay pending until its requests complete.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorHistory_UndoRedoRestoreBytes`, `Test_EditorHistory_NewCommandClearsRedo`, `Test_EditorHistory_UndoToEntry` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Undo and redo"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Asynchronous changes"

### Z2-R-EDIT-039 Failed undo compensates
- **Statement:** An inverse that fails must be compensated so the documents are left as before the attempt; the record must become a fence when the cause is permanent or the inverse is synchronous, and stay undoable when an asynchronous inverse failed for a transient cause, the cause of an unknown outcome being decided by its resolution.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorHistory_FailedUndoCompensates`, `Test_EditorHistory_AsyncChangeLocksHistory` in the Test gate step (`ZenithEditorCore_Tests`, `win`); `Test_EditorAssetOps_UndoSettingsServerConflict` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Undo and redo"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "History fences"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Asynchronous changes"

### Z2-R-EDIT-040 No reentrant commands
- **Statement:** A command executed while another executes must fail with EDITOR_ERROR_COMMAND_REENTRANT, and undo and redo must be refused inside a transaction or interaction.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorHistory_ReentrantExecuteRefused` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Executing a command"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Undo and redo"

### Z2-R-EDIT-041 Interactions and merging
- **Statement:** Commands inside one interaction must merge into one record, and outside interactions commands with equal non-zero merge keys within editor.undo.mergeWindowMs must merge only when nothing intervened.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorHistory_InteractionMergesAll`, `Test_EditorHistory_MergeWindowRespected` in the Test gate step (`ZenithEditorCore_Tests`, `win`); `Test_EditorGizmo_DragIsOneRecord` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Interactions and merging"

### Z2-R-EDIT-042 History memory bound
- **Statement:** The history must stay within editor.undo.maxRecords and editor.undo.maxBytes by evicting whole oldest records, bound terrain records by editor.undo.maxTerrainRecordBytes and other records by editor.undo.maxRecordBytes, exempt document-bytes and schema records from the per-record cap, and fence a record over its cap.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-EDIT-011
- **Acceptance evidence:** `Test_EditorHistory_MemoryBoundEvictsOldest`, `Test_EditorHistory_RecordCountBound`, `Test_EditorHistory_OversizeRecordBecomesFence`, `Test_EditorHistory_MigrationRecordExemptFromRecordCap` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "History memory bound"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "History fences"

### Z2-R-EDIT-043 Closing a document prunes its history
- **Statement:** Closing a document, or reloading it from disk, must remove every record touching it and every older record of a document such a record links, and nothing else.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorHistory_CloseDocumentPrunesDependentRecords`, `Test_EditorHistory_ReloadPrunesOnlyThatDocument` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Closing a document and its history"

### Z2-R-EDIT-044 Dirty state follows state IDs
- **Statement:** A document must be dirty exactly when its state ID differs from its saved state ID, so undoing back to the saved state makes it clean.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorHistory_DirtyFollowsStateID` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Dirty tracking"

### Z2-R-EDIT-045 Undo round trips for every command
- **Statement:** Every document-changing command must satisfy the round-trip invariants: Execute then Undo restores, Undo then Redo restores, and any operation sequence ends in the state of its surviving commands.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorUndo_EveryCommandRoundTrips` in the Test gate step (`ZenithEditor_Tests`, `win`); `Test_EditorUndo_PropertySequences` in the Test gate step (`ZenithEditorCore_Tests`, `z2-stress`, `win`); `Test_EditorUndo_TerrainPropertySequences` in the Test gate step (`ZenithEditor_Tests`, `z2-stress`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Round-trip invariants"

### Z2-R-EDIT-046 Selection by persistent references
- **Statement:** The selection must name objects by persistent IDs, survive undo, redo, reconcile and reload, map to play-world entities by persistent ID, and refuse growth beyond editor.selection.maxObjects.
- **Covers:** Z2-I-EDIT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSelection_SetAddToggleRemove`, `Test_EditorSelection_LimitRefused`, `Test_EditorSelection_SurvivesUndoAndReload` in the Test gate step (`ZenithEditorCore_Tests`, `win`); `Test_EditorSelection_MapsToPlayWorld` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Selection model"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Selection persistence and mapping"

### Z2-R-EDIT-047 Selection operations
- **Statement:** Click, Ctrl-click, Shift-click, marquee, select-all and select-referencing must produce the selections the operation table defines.
- **Covers:** Z2-I-EDIT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSelection_SetAddToggleRemove` in the Test gate step (`ZenithEditorCore_Tests`, `win`); `Test_EditorPicking_MarqueeFrustum` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Selection operations"

### Z2-R-EDIT-048 Selection restored by undo
- **Statement:** Selection changes must not be history records, and undo and redo must restore each record's selection before and after.
- **Covers:** Z2-I-EDIT-02, Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSelection_UndoRestoresSelection` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Selection and undo"

### Z2-R-EDIT-049 Transform edits apply to selection roots
- **Statement:** Transform edits must apply to the selection's root entities alone, so no entity moves twice.
- **Covers:** Z2-I-EDIT-02, Z2-I-EDIT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSelection_RootsExcludeSelectedDescendants` in the Test gate step (`ZenithEditorCore_Tests`, `win`); `Test_EditorGizmo_RootsOnly` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Selection model"; `Zenith2/Docs/Design/EDIT/Viewports.md` → "Multi-object edits"

### Z2-R-EDIT-050 Paste with fresh identities
- **Statement:** Paste and duplicate must give pasted objects fresh authoring IDs from the session's seeded random source (or the command's idSeed) with internal references rewritten, place them with SCENE's sort-key rule including rebalancing, and record one undoable record.
- **Covers:** Z2-I-EDIT-02, Z2-I-EDIT-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorClipboard_PasteRemapsIDs`, `Test_EditorClipboard_PasteRebalancesFullGroup` in the Test gate step (`ZenithEditorCore_Tests`, `win`); `Test_EditorSession_SeededIDsReproducible` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Copy cut paste and duplicate"

### Z2-R-EDIT-051 Clipboard document is untrusted
- **Statement:** Pasting from the system clipboard must accept only a zenith2.editor-clipboard document of a known version and kind, parsed under the default limits, and keep unknown components as preserved data.
- **Covers:** Z2-I-EDIT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorClipboard_SystemClipboardRoundTrip`, `Test_EditorClipboard_ForeignTextRefused`, `Test_EditorClipboard_UnknownComponentPreserved` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Formats.md` → "Clipboard document"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Copy cut paste and duplicate"

### Z2-R-EDIT-052 ImGui fed from PLAT input
- **Statement:** The shell must feed ImGui from PLAT's presentation result alone, using no ImGui platform backend and no SDL or Win32 type outside ZenithPlatform.
- **Covers:** Z2-I-EDIT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorShell_ImGuiAdapterMapsInput` in the Test gate step (`ZenithEditor_Tests`, `win`); the Libgraph gate step
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "ImGui platform adapter"

### Z2-R-EDIT-053 Editor input claimant
- **Statement:** The editor's claimant, registered after UI's, must claim every pointer press outside a focused game viewport and every action while ImGui wants the keyboard.
- **Covers:** Z2-I-EDIT-03, Z2-I-EDIT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorShell_ClaimantClaimsOutsideGameViewport` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Editor input claimant"

### Z2-R-EDIT-054 ImGui rendered through the render graph
- **Statement:** ImGui must be drawn by an overlay-point view extension that adds its pass only to the shell view, recognised by its view ID, sampling viewport output textures through declared SRV reads and recording no command list of its own.
- **Covers:** Z2-I-EDIT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-055, Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorShell_DrawSnapshotTwoSlots`, `Test_EditorShell_DrawSnapshotCapDropsWholeLists`, `Test_EditorShell_ExtensionsFilterByViewID` in the Test gate step (`ZenithEditor_Tests`, `win`); `Test_EditorRender_ShellCompositeOnly`, `Test_EditorRender_ViewportTextureSampledByShell` in the Test gate step (`ZenithEditor_Tests`, `editor-render`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "ImGui renderer"; `Zenith2/Docs/Design/EDIT/Viewports.md` → "Shell view request"

### Z2-R-EDIT-055 Texture references are generational and owned
- **Statement:** ImGui texture references must be generational slots of a table that owns every referenced tool texture and pins every referenced asset, reaches the renderer only through its declared texture backend, and reclaims a released slot and destroys its texture only after every snapshot that could resolve it has been recorded, so a reference never draws another texture.
- **Covers:** Z2-I-EDIT-05, Z2-I-EDIT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorTextureTable_StaleReferenceDrawsChecker`, `Test_EditorTextureTable_ResizeWhileDrawnNeverAliases` in the Test gate step (`ZenithEditor_Tests`, `win`); `Test_EditorTextureTable_AssetPinnedWhileReferenced`, `Test_EditorRender_StaleTextureShowsChecker` in the Test gate step (`ZenithEditor_Tests`, `editor-render`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Editor texture references"; `Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Texture backend header"

### Z2-R-EDIT-056 Display scale and fonts
- **Statement:** A display-scale change must rebuild the font atlas once and rescale the style from its unscaled copy, with vector icons that need no image asset.
- **Covers:** Z2-I-EDIT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-034
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorShell_DisplayScaleRebuildsAtlas` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Fonts and DPI"

### Z2-R-EDIT-057 Shortcuts dispatch commands
- **Statement:** Every shortcut must be a binding from a chord to a registered command ID, dispatched only when no widget wants the keyboard, with per-user overrides.
- **Covers:** Z2-I-EDIT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-034
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorShell_ShortcutsDispatchCommands` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Shortcuts"; `Zenith2/Docs/Design/EDIT/Formats.md` → "Preferences file"

### Z2-R-EDIT-058 Editor views through the render world
- **Statement:** Scene, game and preview viewports must stage their views as render-world requests of their kinds, game views writing the game camera as a VIEW_KIND_EDITOR request into a tool texture from worlds of purpose WORLD_PURPOSE_PLAY only, and no editor world writing VIEW_KIND_MAIN.
- **Covers:** Z2-I-EDIT-06, Z2-I-EDIT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorViewports_ViewRequestMembers`, `Test_EditorViewports_HiddenViewportRequestsNothing` in the Test gate step (`ZenithEditor_Tests`, `win`); `Test_EditorRender_TwoViewportsIndependentHistories` in the Test gate step (`ZenithEditor_Tests`, `editor-render`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Viewport kinds"; `Zenith2/Docs/Design/EDIT/Viewports.md` → "Viewport views"

### Z2-R-EDIT-059 Viewport resize resets history
- **Statement:** A viewport resize must recreate its output in the same frame in the order register-new, swap-reference, release-old, and set the camera-cut bit once.
- **Covers:** Z2-I-EDIT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorViewports_ResizeSetsCameraCutOnce`, `Test_EditorTextureTable_ResizeWhileDrawnNeverAliases` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Viewport views"; `Zenith2/Docs/Design/EDIT/Viewports.md` → "Editor texture references"

### Z2-R-EDIT-060 Editor extractor
- **Statement:** Each edit, play and preview world must carry one Editor.Extract node that writes the frame snapshot's view requests and, in an edit world with a terrain, the terrain preview records.
- **Covers:** Z2-I-EDIT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorViewports_ExtractorWritesSnapshotRequests`, `Test_EditorTerrain_PreviewCallSites` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Editor extractor"

### Z2-R-EDIT-061 Headless editing
- **Statement:** A headless editor must build every ImGui frame, run panels, commands, play and automation unchanged, pick through the CPU path, render nothing, and save the same document bytes as a windowed session.
- **Covers:** Z2-I-EDIT-13, Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorHeadless_OpenEditSaveRoundTrip` in the Test gate step (`ZenithEditor_Tests`, `z2-edit-cooked`, `win`); `Test_EditorHeadless_PickingUsesCpuPath`, `Test_EditorHeadless_PlayRunsTicks`, `Test_EditorHeadless_PreviewsReportNoRenderer` in the Test gate step (`ZenithEditor_Tests`, `win`); `Test_EditorHeadless_SavedBytesEqualWindowed` in the Test gate step (`ZenithEditor_Tests`, `editor-render`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Headless viewports"

### Z2-R-EDIT-062 Scene publish from a headless boot
- **Statement:** A scene authored entirely by commands in a headless editor session must be saved, cooked by ZenithCook and booted by the registered test game's Player with exit code 0, and load with the expected entity count.
- **Covers:** Z2-I-EDIT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorHeadless_ScenePublishFromHeadlessBoot` in the Test gate step (`ZenithEditor_Tests`, `z2-stress`, `z2-edit-cooked`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Headless viewports"; `Zenith2/Docs/Design/EDIT/Tests.md` → "Headless editing tests"; `Zenith2/Docs/Design/EDIT/Tests.md` → "Fixtures"

### Z2-R-EDIT-063 Camera schemes
- **Statement:** The editor camera must offer the Unity and Unreal control schemes of the camera table, selected by editor.camera.scheme.
- **Covers:** Z2-I-EDIT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorCamera_UnrealSchemeGestures` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Camera schemes"

### Z2-R-EDIT-064 Look in raw device counts
- **Statement:** Mouse look while the cursor is captured must convert the raw device-count delta with editor.camera.lookDegreesPerCount and never use the points-based position delta.
- **Covers:** Z2-I-EDIT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorCamera_LookUsesRawCounts`, `Test_EditorCamera_PitchClamped` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Fly navigation"

### Z2-R-EDIT-065 Fly, orbit, pan and dolly
- **Statement:** Fly speed scaling, orbit about the pivot, pan keeping the pivot-depth point under the cursor and dolly must follow the camera formulas.
- **Covers:** Z2-I-EDIT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorCamera_FlySpeedScaling`, `Test_EditorCamera_PanKeepsPointUnderCursor` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Fly navigation"; `Zenith2/Docs/Design/EDIT/Viewports.md` → "Orbit pan and dolly"

### Z2-R-EDIT-066 Framing the selection
- **Statement:** Frame selection must fit the selection's bounding sphere by the framing formula in perspective and orthographic views and set the camera cut at its end.
- **Covers:** Z2-I-EDIT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorCamera_FrameSelectionDistance` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Framing"

### Z2-R-EDIT-067 Orthographic views and bookmarks
- **Statement:** The view menu must offer the six orthographic views and perspective, and camera bookmarks must be stored per user in the session file, never in a document.
- **Covers:** Z2-I-EDIT-03, Z2-I-EDIT-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorFormats_SessionFileRoundTrip` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Orthographic views"; `Zenith2/Docs/Design/EDIT/Viewports.md` → "Camera bookmarks"

### Z2-R-EDIT-068 CPU bounds picking
- **Statement:** Click picking must cast the pixel-centre ray, test registered bounds with the slab test front to back and prefer the nearest precise hit, in windowed and headless sessions.
- **Covers:** Z2-I-EDIT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorPicking_RayFromPixel`, `Test_EditorPicking_SlabTest`, `Test_EditorPicking_NearestPreciseHitWins` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Click picking"; `Zenith2/Docs/Design/EDIT/Viewports.md` → "CPU bounds picking"

### Z2-R-EDIT-069 Precise mesh picking
- **Statement:** Precise picking must test the mesh's decoded LOD-0 triangles through a cached BVH, terrain by a height-field ray march, and skinned meshes in bind pose, within the editor-caches budget.
- **Covers:** Z2-I-EDIT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** Z2-B-EDIT-001
- **Acceptance evidence:** `Test_EditorPicking_MeshBvhMatchesBruteForce`, `Test_EditorPicking_PickMeshCacheLRU`, `Test_EditorPicking_TerrainMarch` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Precise mesh picking"

### Z2-R-EDIT-071 Marquee selection
- **Statement:** A marquee must select the entities whose world bounds intersect its frustum, skipping hidden and locked entities, combined by Shift and Alt.
- **Covers:** Z2-I-EDIT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorPicking_MarqueeFrustum` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Marquee selection"

### Z2-R-EDIT-072 Handles are picked first
- **Statement:** Gizmo handles must be tested analytically before objects, the nearest handle winning with axes before planes before the centre.
- **Covers:** Z2-I-EDIT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorPicking_HandleBeforeObject` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Gizmo handle picking"

### Z2-R-EDIT-073 Gizmo modes spaces and pivots
- **Statement:** The gizmo must offer the translate, rotate, scale and combined modes in world, local, view and parent spaces with the three pivot modes, scale always in local space.
- **Covers:** Z2-I-EDIT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorGizmo_DragIsOneRecord`, `Test_EditorGizmo_LocalTransformUnderScaledParent` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Gizmo modes and spaces"; `Zenith2/Docs/Design/EDIT/Viewports.md` → "Pivot"

### Z2-R-EDIT-074 Constant screen-size handles
- **Statement:** Gizmo handles must keep a constant screen size and hide an axis nearly parallel to the view and a plane seen edge-on.
- **Covers:** Z2-I-EDIT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorGizmoMath_HandleSizeConstantOnScreen`, `Test_EditorGizmoMath_HiddenAxisWhenParallel` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Handle sizing"

### Z2-R-EDIT-075 Translate drag mathematics
- **Statement:** Axis, plane and screen-plane drags must follow the closest-point and ray-plane formulas, contributing no motion for parallel rays, edge-on planes and hits behind the camera.
- **Covers:** Z2-I-EDIT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorGizmoMath_AxisClosestPoint`, `Test_EditorGizmoMath_PlaneDrag` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Translate drag mathematics"

### Z2-R-EDIT-076 Rotate drag mathematics
- **Statement:** Ring and trackball rotation must accumulate the signed angle beyond 180 degrees and keep edge-on rings draggable.
- **Covers:** Z2-I-EDIT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorGizmoMath_RotateAccumulatesBeyond180`, `Test_EditorGizmoMath_EdgeOnRingStaysDraggable` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Rotate drag mathematics"

### Z2-R-EDIT-077 Scale drag mathematics
- **Statement:** Scale drags must multiply the start scale by a factor clamped at 0.001, never producing a zero or negative scale.
- **Covers:** Z2-I-EDIT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorGizmoMath_ScaleClampedAboveZero` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Scale drag mathematics"

### Z2-R-EDIT-078 Gizmo edits write local transforms
- **Statement:** A gizmo drag must write the decomposed local transform against the parent's world matrix as one interaction record, refusing a singular parent.
- **Covers:** Z2-I-EDIT-03, Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorGizmo_DragIsOneRecord`, `Test_EditorGizmo_LocalTransformUnderScaledParent`, `Test_EditorGizmo_SingularParentRefused` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Applying a gizmo delta"

### Z2-R-EDIT-079 Component gizmos
- **Statement:** Components with spatial parameters must be editable through registered gizmo providers using the same handle shapes and mathematics, each drag one property edit.
- **Covers:** Z2-I-EDIT-03, Z2-I-EDIT-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorUndo_EveryCommandRoundTrips` in the Test gate step (`ZenithEditor_Tests`, `win`) (the component-gizmo commands)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Component gizmos"

### Z2-R-EDIT-080 Grid snapping
- **Statement:** Translate snapping must snap the dragged axes to the grid in absolute mode or the delta in relative mode with increment g.
- **Covers:** Z2-I-EDIT-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSnapping_GridAbsolute`, `Test_EditorSnapping_GridRelative` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Grid snapping"; `Zenith2/Docs/Design/EDIT/Viewports.md` → "Snap settings"

### Z2-R-EDIT-081 Rotation and scale snapping
- **Statement:** Rotation must snap the accumulated angle to exact multiples of the increment and scale must snap the factor, never below the increment.
- **Covers:** Z2-I-EDIT-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSnapping_RotationExactMultiples`, `Test_EditorSnapping_ScaleFactor` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Rotation snapping"; `Zenith2/Docs/Design/EDIT/Viewports.md` → "Scale snapping"

### Z2-R-EDIT-082 Surface and vertex snapping
- **Statement:** Surface snapping must place the pivot on the hit under the cursor with optional normal alignment, and vertex snapping must land the nearest vertex on the nearest target vertex within 12 pixels.
- **Covers:** Z2-I-EDIT-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSnapping_SurfaceAndNormalAlign`, `Test_EditorSnapping_VertexSnap` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Surface snapping"; `Zenith2/Docs/Design/EDIT/Viewports.md` → "Vertex snapping"

### Z2-R-EDIT-083 Snap toggle inversion
- **Statement:** Holding Ctrl during a drag must invert the snap toggles for that drag.
- **Covers:** Z2-I-EDIT-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSnapping_CtrlInvertsToggle` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Snap settings"

### Z2-R-EDIT-084 Overlay passes within the GPU rules
- **Statement:** The overlay passes must run only for the scene views listed in the frame's overlay snapshot, recognised by view ID, use targets of at most 16 bits per channel, and test depth only where specified.
- **Covers:** Z2-I-EDIT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-014, Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorRender_GridLinesAtIncrement`, `Test_EditorRender_SelectionOutlineAroundSelected`, `Test_EditorRender_GizmoAxisColours`, `Test_EditorRender_SyncValidationClean` in the Test gate step (`ZenithEditor_Tests`, `editor-render`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Editor overlay passes"; `Zenith2/Docs/Design/EDIT/Viewports.md` → "Grid pass"; `Zenith2/Docs/Design/EDIT/Viewports.md` → "Selection outline pass"; `Zenith2/Docs/Design/EDIT/Viewports.md` → "Gizmo pass"

### Z2-R-EDIT-085 Overlay snapshot caps
- **Statement:** Overlay data must travel in the editor's two-slot snapshot within editor.overlay.maxVertices and editor.overlay.maxSelectedInstances, truncating in selection order and counting the frame.
- **Covers:** Z2-I-EDIT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorShell_DrawSnapshotTwoSlots` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Overlay records"

### Z2-R-EDIT-089 Hierarchy from documents
- **Statement:** The hierarchy must list the active scene set's objects from the documents in sibling order, lazily and clipped, with instance, missing, unknown and read-only markers.
- **Covers:** Z2-I-EDIT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorHierarchy_SiblingOrderFromDocument`, `Test_EditorHierarchy_LargeSceneLazy` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "Hierarchy data source"

### Z2-R-EDIT-090 Hierarchy edits
- **Statement:** Create, rename, reparent and reorder, delete and unpack must produce the documented scene edits, reparenting keeping the world transform in the same record and invalid drops refused before any edit.
- **Covers:** Z2-I-EDIT-01, Z2-I-EDIT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorHierarchy_ReparentKeepsWorldTransform`, `Test_EditorHierarchy_ReorderUsesSortKeyBetween`, `Test_EditorHierarchy_InvalidDropRefused` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "Hierarchy operations"

### Z2-R-EDIT-091 Inspector through reflection
- **Statement:** The inspector must show document values through the reflection walk with no branch on a type's origin, write edits into a gathered copy and record SET_PROPERTY of the difference, clamped to declared ranges.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorInspector_EditRecordsSetPropertyDiff`, `Test_EditorInspector_DataDefinedAndCppAlike`, `Test_EditorInspector_RangeClamp` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "Inspector data source"; `Zenith2/Docs/Design/EDIT/Panels.md` → "Property editors"

### Z2-R-EDIT-092 Multi-object inspector edits
- **Statement:** With several entities selected the inspector must show shared components with mixed markers and set a value on every entity in one transaction.
- **Covers:** Z2-I-EDIT-01, Z2-I-EDIT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorInspector_MultiEditOneTransaction` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "Multi-object editing"

### Z2-R-EDIT-096 Asset browser through the cook server
- **Statement:** The asset browser must list, search and inspect assets through the cook server's requests alone, concatenating pages in order and invalidating cached pages on notifications.
- **Covers:** Z2-I-EDIT-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorAssetBrowser_ListPagesConcatenated`, `Test_EditorAssetBrowser_NotificationInvalidatesFolder` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "Asset browser data source"; `Zenith2/Docs/Design/EDIT/Panels.md` → "Asset browser views"

### Z2-R-EDIT-097 Import by copy
- **Statement:** Import must copy a dropped or chosen file into the game's source tree under a unique name, assign GUIDs and import through the server, confirm move suggestions, and record one undoable record.
- **Covers:** Z2-I-EDIT-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorAssetBrowser_ImportCopiesNeverMoves`, `Test_EditorAssetBrowser_ImportUndoTrashes` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "Import and drag and drop"

### Z2-R-EDIT-098 Importer settings with digest check
- **Statement:** Importer settings must be applied with setSettings and the record's .meta digest as one undoable record, refreshing and applying nothing on COOK_ERROR_META_CHANGED.
- **Covers:** Z2-I-EDIT-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorAssetInspector_SettingsApplyAndConflict` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "Asset inspector"

### Z2-R-EDIT-099 Dependencies and provenance views
- **Statement:** The asset browser must show an asset's dependencies, dependents and provenance from the server's requests and select the open scenes' entities referencing it.
- **Covers:** Z2-I-EDIT-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorAssetBrowser_DependencyAndProvenanceViews` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "Dependencies and provenance"

### Z2-R-EDIT-100 Preview host
- **Statement:** Each visible preview panel must issue one VIEW_KIND_PREVIEW request rendered by RPROD's preview path, Forward+ until Z2T-RPROD-112 lands and Forward after it, and a hidden panel none.
- **Covers:** Z2-I-EDIT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorPreviewHost_PreviewRequestsPerPanel`, `Test_EditorWorlds_PreviewPoolEvictsLRU` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "Preview host"

### Z2-R-EDIT-101 Material preview
- **Statement:** Material parameter edits must be recorded as JSON edits whose forward and inverse changes each re-apply the document's parameters, packed by the reflection, to the runtime material through a projection handler and ReplaceParameters, with preview keys read from the document, and be proven on the rendered preview without a double of the material resource.
- **Covers:** Z2-I-EDIT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorMaterial_LiveParametersAndValidate`, `Test_EditorMaterial_PackingMatchesReflection` in the Test gate step (`ZenithEditor_Tests`, `win`); `Test_EditorMaterial_UndoRestoresLiveParameters` in the Test gate step (`ZenithEditor_Tests`, `editor-render`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "Material preview"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Material tools"

### Z2-R-EDIT-102 Thumbnails
- **Statement:** Thumbnails must render at most editor.thumbnails.perFrame per frame, be read back through operation handles, be keyed by the record's import key and regenerate after a reimport.
- **Covers:** Z2-I-EDIT-06, Z2-I-EDIT-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorThumbnails_KeyInvalidatesOnReimport`, `Test_EditorThumbnails_PerFrameCap` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "Thumbnail generation"

### Z2-R-EDIT-103 Editor caches under the governor
- **Statement:** The editor caches (decoded thumbnails, idle preview worlds, pick meshes) must shed by the governor's levels through the editor's MEMORY_SUBSYSTEM_EDITOR_CACHES consumer, which frees pick meshes in the callback and only marks thumbnails and preview worlds for the preview host to release in the same frame's step 6, whose case is tested in ZenithEditor_Tests, and the disk cache must be pruned at startup.
- **Covers:** Z2-I-EDIT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-EDIT-001
- **Acceptance evidence:** `Test_EditorCacheConsumer_ShedsAtWarningAndCritical` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "Thumbnail cache"

### Z2-R-EDIT-104 Console and script console
- **Statement:** The console must run DEVT console lines and evaluate Luau lines with ConsoleEvaluate on the focused play world through the editor's compile thunk.
- **Covers:** Z2-I-EDIT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorConsole_LuauLineUsesConsoleEvaluate` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "Console"

### Z2-R-EDIT-105 Log and diagnostics panels
- **Statement:** The log panel must show the process log ring with filters and the diagnostics panel must collect script, cook, document, shader and schema diagnostics with a go-to action.
- **Covers:** Z2-I-EDIT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorScriptReload_CompileErrorToDiagnostics`, `Test_EditorShaderReload_ErrorRecordKeepsOld` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "Log panel"; `Zenith2/Docs/Design/EDIT/Panels.md` → "Diagnostics panel"

### Z2-R-EDIT-106 Profiler toggle and editor provider
- **Statement:** The editor must create DEVT's overlay for editor views, toggle it from the toolbar and F3, and contribute the editor panel provider from its zones.
- **Covers:** Z2-I-EDIT-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorProfiler_ToggleAndProvider` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "Profiler integration"

### Z2-R-EDIT-107 Cook server process
- **Statement:** The cook server must run as one ZenithCook --serve child per game and platform from the editor's own executable directory with --out under the output root, inside a kill-on-close job object, with non-blocking pipe threads.
- **Covers:** Z2-I-EDIT-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorCookClient_RealServerHandshake` in the Test gate step (`ZenithEditor_Tests`, `z2-stress`, `z2-edit-cooked`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Server process"

### Z2-R-EDIT-108 Cook client state machine
- **Statement:** The cook client must follow its state machine: handshake, watch, restart with back-off, retry every 2 s while the output tree is locked, and stop at a version mismatch.
- **Covers:** Z2-I-EDIT-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorCookClient_ProtocolAgainstFakeServer`, `Test_EditorCookClient_StateMachineTable`, `Test_EditorCookClient_BackoffAndFailed` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Client state machine"

### Z2-R-EDIT-109 Cook request protocol
- **Statement:** The client must assign unique request ids, concatenate pages with the final counts checked, support cancel, and fail requests clearly on errors, restarts and a full queue.
- **Covers:** Z2-I-EDIT-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorCookClient_ProtocolAgainstFakeServer`, `Test_EditorCookClient_RequestQueueFullRejects` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Request queue"; `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Cook client failures"

### Z2-R-EDIT-110 Reimport reloads every world
- **Statement:** A reimport result or published notification must, through the editor's asset reload sink, call OnMountsChanged when the catalogue changed and RequestReload for every changed active asset, and invalidate thumbnails and pick meshes.
- **Covers:** Z2-I-EDIT-05, Z2-I-EDIT-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** Z2-B-EDIT-004
- **Acceptance evidence:** `Test_EditorReimport_ReloadsActiveAssets`, `Test_EditorReimport_PublishedNotificationReloads` in the Test gate step (`ZenithEditor_Tests`, `win`); `Test_EditorIntegration_ReimportEndToEnd` in the Test gate step (`ZenithEditor_Tests`, `z2-stress`, `z2-edit-cooked`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Reimport flow"; `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Notifications"; `Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Asset reload sink header"

### Z2-R-EDIT-111 Reimport timing
- **Statement:** Asset reimport must be timed from the source file's save or the command to the first frame showing the reloaded asset, as the MP-EDIT sample of Z2-B-EDIT-004.
- **Covers:** Z2-I-EDIT-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-EDIT-004
- **Acceptance evidence:** `Test_EditorAutomation_MeasurementsUseEvents` in the Test gate step (`ZenithEditor_Tests`, `win`); the W-Editor budget run reports under `Zenith2/Docs/Research/Budgets/EDIT/`
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Reimport timing"; `Zenith2/Docs/Design/EDIT/Automation.md` → "Timed operations"

### Z2-R-EDIT-116 In-process script compilation
- **Statement:** A changed script, reported by the storage roots' host watchers, must be compiled in process with the production options and checked by the stateless check, its diagnostics shown, its bytecode equal to the cooked form, and its definitions written only to the untracked generated Luau directory.
- **Covers:** Z2-I-EDIT-20
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-017
- **Budgets:** Z2-B-EDIT-002
- **Acceptance evidence:** `Test_EditorScriptReload_CompileErrorToDiagnostics`, `Test_EditorScriptReload_StatelessRejectionToDiagnostics`, `Test_EditorScriptReload_BytecodeEqualsCooked` in the Test gate step (`ZenithEditor_Tests`, `win`); `Test_EditorScriptReload_DefinitionsRegenerated` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Script reload"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Host watchers"

### Z2-R-EDIT-117 Script reload queued on play worlds
- **Statement:** Accepted bytecode must be queued with RequestHotReload on every play world and on no edit world, and definitions must be regenerated on registry changes.
- **Covers:** Z2-I-EDIT-20
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-023
- **Budgets:** Z2-B-EDIT-002
- **Acceptance evidence:** `Test_EditorScriptReload_QueuesOnEveryPlayWorld`, `Test_EditorScriptReload_DefinitionsRegenerated`, `Test_EditorPlay_HotReloadCatchUp` in the Test gate step (`ZenithEditor_Tests`, `win`); `Test_EditorIntegration_ScriptReloadEndToEnd` in the Test gate step (`ZenithEditor_Tests`, `z2-stress`, `z2-edit-cooked`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Script reload"

### Z2-R-EDIT-118 Script reload timing
- **Statement:** Script hot reload must be timed from the module file's save to the script world's live notification delivered through the main-thread tool hook, a measurement whose event was dropped being discarded.
- **Covers:** Z2-I-EDIT-20
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-EDIT-002
- **Acceptance evidence:** `Test_EditorAutomation_MeasurementsUseEvents` in the Test gate step (`ZenithEditor_Tests`, `win`); the W-Editor budget run reports
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Script reload timing"; `Zenith2/Docs/Design/EDIT/Automation.md` → "Timed operations"

### Z2-R-EDIT-119 Shader journal applied in order
- **Statement:** The reload watcher must apply the shader reload journal, whose changes the journal directory's own non-recursive host watcher reports, strictly in seq order, never skipping a record, waiting for partial lines and re-reading after a gap.
- **Covers:** Z2-I-EDIT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** Z2-B-EDIT-003
- **Acceptance evidence:** `Test_EditorShaderReload_AppliesInSeqOrder`, `Test_EditorShaderReload_PartialLineWaits` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Shader reload"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Host watchers"

### Z2-R-EDIT-120 Shader reload responses
- **Statement:** An ok record must reload the container keeping old PSOs until replacements are ready, an engine layout change must keep the old blobs and report a rebuild, a material layout change must re-cook its constants, and an error must keep the old container.
- **Covers:** Z2-I-EDIT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** Z2-B-EDIT-003
- **Acceptance evidence:** `Test_EditorShaderReload_LayoutChangedEngineProgram`, `Test_EditorShaderReload_LayoutChangedMaterialRecooks`, `Test_EditorShaderReload_ErrorRecordKeepsOld` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Shader reload"

### Z2-R-EDIT-121 Shader reload timing
- **Statement:** Shader blob reload must be timed from the .slang save to the first frame drawn with the rebuilt PSOs as the MP-EDIT sample of Z2-B-EDIT-003, a layout change being reported instead of timed.
- **Covers:** Z2-I-EDIT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-EDIT-003
- **Acceptance evidence:** `Test_EditorAutomation_MeasurementsUseEvents` in the Test gate step (`ZenithEditor_Tests`, `win`); the W-Editor budget run reports
- **Design:** `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Shader reload timing"

### Z2-R-EDIT-122 Scene open timing
- **Statement:** Scene open must be timed from the open command to the first editor frame with the scene projected and submitted, as the MP-EDIT sample of Z2-B-EDIT-005, and must never cook.
- **Covers:** Z2-I-EDIT-18
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-EDIT-005
- **Acceptance evidence:** `Test_EditorProjection_OpenSceneEqualsActivation`, `Test_EditorAutomation_MeasurementsUseEvents` in the Test gate step (`ZenithEditor_Tests`, `win`); the W-Editor budget run reports
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Reconcile algorithm"; `Zenith2/Docs/Design/EDIT/Automation.md` → "Timed operations"

### Z2-R-EDIT-123 Play-in-editor start timing
- **Statement:** Play-in-editor start must be timed from the Play command to the first simulated tick of the play world, as the MP-EDIT sample of Z2-B-EDIT-006.
- **Covers:** Z2-I-EDIT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-EDIT-006
- **Acceptance evidence:** `Test_EditorPlay_StartCapturesAndActivates`, `Test_EditorAutomation_MeasurementsUseEvents` in the Test gate step (`ZenithEditor_Tests`, `win`); the W-Editor budget run reports
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Starting play"; `Zenith2/Docs/Design/EDIT/Automation.md` → "Timed operations"

### Z2-R-EDIT-124 W-Editor measurement procedure
- **Statement:** The committed W-Editor script under the game's Workload directory must perform each timed operation five times from the same state, include the play-world and schema recompile triggers, and record MP-EDIT samples the median rule judges.
- **Covers:** Z2-I-EDIT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-EDIT-002, Z2-B-EDIT-003, Z2-B-EDIT-004, Z2-B-EDIT-005, Z2-B-EDIT-006
- **Acceptance evidence:** the `needs-human` W-Editor run reports committed under `Zenith2/Docs/Research/Budgets/EDIT/`
- **Design:** `Zenith2/Docs/Design/EDIT/Automation.md` → "Measurement procedure"; `Zenith2/Docs/Design/EDIT.md` → "Untracked editor files"

### Z2-R-EDIT-128 Terrain strokes
- **Statement:** A terrain drag must emit dabs at the spacing rule, apply each frame's dabs with ApplyStroke as one segment of one interaction record, and be deterministic for the same dab list.
- **Covers:** Z2-I-EDIT-01, Z2-I-ENV-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** Z2-B-EDIT-012
- **Acceptance evidence:** `Test_EditorTerrain_DabSpacing`, `Test_EditorTerrain_StrokeIsOneRecordOfSegments`, `Test_EditorTerrain_StrokeDeterministic` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Sculpt and paint strokes"

### Z2-R-EDIT-129 Terrain undo through the history
- **Statement:** Each terrain stroke must be one record of its frame segments, undone newest first and redone oldest first, bounded by editor.undo.maxTerrainRecordBytes (the value of Z2-B-EDIT-011) and fenced above it.
- **Covers:** Z2-I-EDIT-01, Z2-I-ENV-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** Z2-B-EDIT-011
- **Acceptance evidence:** `Test_EditorTerrain_UndoChainNeverStale`, `Test_EditorTerrain_OversizeStrokeFences` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Terrain undo"

### Z2-R-EDIT-130 Terrain live preview call sites
- **Statement:** The editor must queue dirty regions after each stroke frame, call Prepare in the frame prologue and WriteRecords from the edit world's extractor.
- **Covers:** Z2-I-ENV-01, Z2-I-EDIT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** Z2-B-EDIT-012
- **Acceptance evidence:** `Test_EditorTerrain_PreviewCallSites` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Terrain live preview"

### Z2-R-EDIT-131 Terrain sculpt preview latency
- **Statement:** Terrain sculpt preview latency must be timed from a dab's ApplyStroke to the first frame drawing the replaced height chain, as Z2-B-EDIT-012.
- **Covers:** Z2-I-ENV-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-EDIT-012
- **Acceptance evidence:** `Test_EditorAutomation_MeasurementsUseEvents` in the Test gate step (`ZenithEditor_Tests`, `win`); the W-Editor budget run reports
- **Design:** `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Terrain live preview"; `Zenith2/Docs/Design/EDIT/Automation.md` → "Timed operations"

### Z2-R-EDIT-132 Regeneration after a stroke
- **Statement:** At the end of a stroke, undo or redo the editor must regenerate the stroke's regions once, cancelling and restarting for the union when a new stroke starts.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorTerrain_RegenerationAtStrokeEnd` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Regeneration after a stroke"

### Z2-R-EDIT-133 Terrain panel and creation wizard
- **Statement:** The terrain panel must expose ENV's brush kinds and settings, the layer palette and the tile grid, and the creation wizard must create, save, import and place a terrain in one transaction.
- **Covers:** Z2-I-ENV-01, Z2-I-ENV-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorUndo_EveryCommandRoundTrips` in the Test gate step (`ZenithEditor_Tests`, `win`) (the terrain commands)
- **Design:** `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Terrain panel"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Terrain creation wizard"

### Z2-R-EDIT-134 Foliage scatter and placement preview
- **Statement:** Foliage scatter and erase must be terrain strokes, and the placement preview must run the same Zenith_Place the cook runs for the tiles in view.
- **Covers:** Z2-I-ENV-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorTerrain_StrokeDeterministic` in the Test gate step (`ZenithEditor_Tests`, `win`) (the scatter kind)
- **Design:** `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Foliage tools"

### Z2-R-EDIT-136 Lighting tools
- **Statement:** Lights and probes must be edited through registered inspectors and gizmos, a baked probe's capture must be an undoable document edit and a realtime probe's capture an edit-world-only write, and grading edits must preview through the scene view's camera lighting.
- **Covers:** Z2-I-EDIT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorLighting_CaptureNowBakedIsRecorded` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Light and probe inspectors"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Probe capture"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Grading editor"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Edit-world-only writes"

### Z2-R-EDIT-162 Thumbnail cache file
- **Statement:** Thumbnails must be cached under the build output as versioned digest-protected files, regenerated when stale or damaged.
- **Covers:** Z2-I-EDIT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorFormats_ThumbnailFileRoundTrip` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Formats.md` → "Thumbnail file"; `Zenith2/Docs/Design/EDIT/Panels.md` → "Thumbnail cache"

### Z2-R-EDIT-163 Command registry
- **Statement:** Every editor action must be a registered command with a stable identifier, flags and an argument schema checked before execution, shared by menus, shortcuts, scripts and the live link.
- **Covers:** Z2-I-EDIT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-034
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorCommands_ArgsValidated`, `Test_EditorCommands_DumpListsEveryCommand` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Automation.md` → "Command identifiers"; `Zenith2/Docs/Design/EDIT/Automation.md` → "Command descriptors"; `Zenith2/Docs/Design/EDIT/Automation.md` → "Built-in commands"

### Z2-R-EDIT-173 Undo and redo acceptance evidence
- **Statement:** The Stage 3 acceptance must hold the undo and redo evidence: the round-trip tests of every command and the property sequences over the three document kinds, green.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorUndo_EveryCommandRoundTrips`, `Test_EditorUndo_PropertySequences`, `Test_EditorHeadless_UndoRoundTripScript` in the Stage 3 acceptance suite
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Round-trip invariants"; `Zenith2/Docs/Design/EDIT.md` → "Stage 3"

### Z2-R-EDIT-174 Independent play worlds acceptance evidence
- **Statement:** The Stage 3 acceptance must hold the independent play worlds evidence: two play worlds independent of each other and of the edit world.
- **Covers:** Z2-I-EDIT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorPlay_TwoWorldsIndependent`, `Test_EditorPlay_StopLeavesEditWorldUnchanged` in the Stage 3 acceptance suite
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Several play worlds"; `Zenith2/Docs/Design/EDIT.md` → "Stage 3"

### Z2-R-EDIT-176 Editor render validation
- **Statement:** Every editor render test must run with Vulkan synchronization validation and D3D12 GPU-based validation clean.
- **Covers:** Z2-I-EDIT-03, Z2-I-EDIT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorRender_SyncValidationClean` in the Test gate step (`ZenithEditor_Tests`, `editor-render`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Tests.md` → "Render tests"; `Zenith2/Docs/Design/EDIT/Tests.md` → "Suites and gate steps"

### Z2-R-EDIT-179 ZenithEditorCore tool library
- **Statement:** The session, documents, commands, history, selection, clipboard, events, worlds, projection, play controller, cook client and migration and schema models must be the static zenith_add_tool library ZenithEditorCore in Zenith2/Tools/EditorCore/, SUPPORTED ZENITH_TOOLS_HOST, whose public headers include no ImGui, STL or third-party header and which ZenithEditor and the integration suites link.
- **Covers:** Z2-I-EDIT-01, Z2-I-EDIT-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** the Libgraph, SourceLayout, ApiContractCheck and HeaderSelfContained gate step reports for the target; the configure of `Zenith_Integration_EditorMigration` and `Zenith_Integration_EditorSchema` linking it
- **Design:** `Zenith2/Docs/Design/EDIT.md` → "ZenithEditorCore target"; `Zenith2/Docs/Design/EDIT.md` → "Libraries and third-party dependencies"; `Zenith2/Docs/Design/EDIT/Declarations.md` → "ZenithEditorCore headers"

### Z2-R-EDIT-180 ZenithAuthoring tool library contract
- **Statement:** The editor must reach every owner's tool-side functions only through the owner's root header of the static tool library ZenithAuthoring (SUPPORTED ZENITH_TOOLS_HOST), whose contract EDIT owns: one Internal/<Dir>/ directory and one root header per owner, tests in ZenithAuthoring_Tests under Tests/<Dir>/, migration fixtures under Tests/Fixtures/Migration/<Dir>/<Type>/, no other library's Internal/ source, no ZenithCook or ZenithEditor type, and a DEPENDS list that grows only by each owner's move.
- **Covers:** Z2-I-EDIT-01, Z2-I-ANIM-16, Z2-I-AUDIO-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** the Libgraph gate step report (no ZenithEditor include of a ZenithCook header); `Test_EditorDocumentTypes_RegistrationOrderMatchesAsset` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT.md` → "Tool-side code the editor needs"; `Zenith2/Docs/Design/EDIT.md` → "Libraries and third-party dependencies"; `Zenith2/Docs/Design/EDIT.md` → "ZenithAuthoring contract rules"; `Zenith2/Docs/Design/EDIT.md` → "ZenithAuthoring owners"

### Z2-R-EDIT-181 Dependency lists grow with their includes
- **Statement:** Each ticket whose diff first includes a library's root header from ZenithEditorCore, ZenithAuthoring or ZenithEditor must add that library to the including target's DEPENDS in the same diff, and no target may declare a library it does not include.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** the Libgraph gate step report of every EDIT diff (used-but-undeclared and declared-but-unused edges); the configure of the Stage 2 pull-forward `ZenithEditorCore`
- **Design:** `Zenith2/Docs/Design/EDIT.md` → "Dependency growth rule"

### Z2-R-EDIT-182 Host paths from the executable location
- **Statement:** The editor must derive the output root, the Zenith2 root, the games root and the host tools directory from its own executable path, refuse with exit code 3 when no declared_deps.json lies three directories above it, and read composition.json before the platform layer exists with its own host read.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorHostPaths_ResolveFromExecutable` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Host paths"; `Zenith2/Docs/Design/EDIT/Declarations.md` → "Host paths header"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Startup sequence"

### Z2-R-EDIT-183 Tool services injected once
- **Statement:** The runtime tool access must fill Zenith_ToolServices once at creation, the editor's composition root must hand each reference to the constructors that need it, and no editor object may look a service up afterwards.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** the ForbiddenCheck and TestSeparationCheck gate step reports (rule 7 of X258: no *ToolAccess include outside tool sources and tests); `Test_EditorApp_RestartRuntimeKeepsSession` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Runtime tool access"; `Zenith2/Docs/Design/EDIT.md` → "API rules for a tool"

### Z2-R-EDIT-184 Editor proxy entities
- **Statement:** Each edit world must hold one editor proxy entity per scene view showing it, owned by the editor owner, with an ID derived from the edit world's seed and the viewport ID, carrying the camera pose, a Zenith.VFX.Viewer when VFX is selected and, on the most recently focused view's proxy, the Zenith.Environment.Anchor, and no reconcile, pick, hierarchy, copy, save or play copy may include a proxy.
- **Covers:** Z2-I-EDIT-10, Z2-I-EDIT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorProxies_ViewerAndAnchorFollowSceneView` in the Test gate step (`ZenithEditor_Tests`, `win`); `Test_EditorPlay_CaptureExcludesEditWorldOnlyState` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor proxy entities"; `Zenith2/Docs/Design/EDIT/Declarations.md` → "World set header"

### Z2-R-EDIT-185 Withheld components in edit worlds
- **Statement:** The projection must not create a document's Zenith.Environment.Anchor in an edit world, keeping it in the document and restoring it in the play copy, so an edit world's one anchor is the active scene view's proxy.
- **Covers:** Z2-I-EDIT-10, Z2-I-ENV-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorProxies_ViewerAndAnchorFollowSceneView`, `Test_EditorPlay_CaptureExcludesEditWorldOnlyState` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor proxy entities"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Projection per edit kind"

### Z2-R-EDIT-186 Edit-world-only writes stay out of documents and play worlds
- **Statement:** Every edit-world-only write (in-place effect play, realtime probe capture, time-of-day and weather scrubbing, the edit world's pause, the proxies) must target a document-owned component, a proxy, a world resource or the world control, never record a history entry or dirty a document, and never reach a play world.
- **Covers:** Z2-I-EDIT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorPlay_CaptureExcludesEditWorldOnlyState` in the Test gate step (`ZenithEditor_Tests`, `win`); `Test_EditorEnvironment_TimeOfDayNotRecorded`, `Test_EditorLighting_CaptureNowBakedIsRecorded` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Edit-world-only writes"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Time of day and weather scrubbing"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Probe capture"

### Z2-R-EDIT-187 Play copy normalisation
- **Statement:** The play copy must drop captured entities and components the resolved documents do not hold, replace every captured field set whose encoded bytes differ by the documents' set, and add the documents' components or entities the capture lacks, in the documents' activation order.
- **Covers:** Z2-I-EDIT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-EDIT-006
- **Acceptance evidence:** `Test_EditorPlay_CaptureExcludesEditWorldOnlyState` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Play copy normalisation"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Starting play"

### Z2-R-EDIT-188 Play copy drift is reported and reconciled
- **Statement:** A document component or entity absent from the edit world's capture that is not a withheld type must be counted as drift, logged once naming the document, restored from the document in the play copy, and followed by a reconcile request of that document.
- **Covers:** Z2-I-EDIT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorPlay_CaptureExcludesEditWorldOnlyState` (its drift case) in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Play copy normalisation"

### Z2-R-EDIT-189 Asynchronous records lock the history
- **Statement:** While a record holding an asynchronous change is pending it must be on neither stack, and every document-changing command, undo, redo, save, close, Play and apply must be refused with EDITOR_ERROR_HISTORY_PENDING while commands that change no document still run.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorHistory_AsyncChangeLocksHistory` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Asynchronous changes"; `Zenith2/Docs/Design/EDIT/Declarations.md` → "History header"

### Z2-R-EDIT-190 Asynchronous failures are compensated
- **Statement:** A failed asynchronous change must be compensated for exactly what it applied and every completed change of its direction inverted in reverse, leaving the history and documents as before the attempt, and a failed compensation must leave an incomplete fence that publishes every dirty touched document in `EDITOR_EVENT_HISTORY_FAILED` and keeps it dirty, never reloading it; from Stage 4 the autosave writes those documents at once.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorHistory_AsyncChangeLocksHistory` in the Test gate step (`ZenithEditorCore_Tests`, `win`); `Test_EditorAutosave_IncompleteFenceWritesTouchedDocuments` (Stage 4) in the Test gate step (`ZenithEditor_Tests`, `win`); `Test_EditorAssetOps_RenameServerRestartCompensates` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Asynchronous changes"; `Zenith2/Docs/Design/EDIT/Declarations.md` → "Change header"

### Z2-R-EDIT-191 Asynchronous timeout and cancel
- **Statement:** A pending asynchronous change must be cancelled after editor.history.asyncTimeoutSeconds without any line from the cook server, or by the user's cancel, and its outcome resolved from the server's state before any compensation.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorHistory_AsyncTimeoutIsInactivity` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Asynchronous changes"

### Z2-R-EDIT-193 Settings undo refuses a changed meta
- **Statement:** Undoing or redoing an importer-settings change must start from a fresh record and, when its settings and platforms objects equal those the record's last completed direction wrote, send setSettings with that record's digest, so a reimport that rewrote only the .meta's sub-assets never blocks it; when they differ, the .meta was changed elsewhere and the undo or redo must be refused with no write, leave the .meta unchanged and fence the record.
- **Covers:** Z2-I-EDIT-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorAssetOps_UndoSettingsServerConflict`, `Test_EditorAssetOps_SettingsUndoAfterSubAssetRewrite` in the Test gate step (`ZenithEditor_Tests`, `win`); `Test_EditorIntegration_ReimportEndToEnd` in the Test gate step (`ZenithEditor_Tests`, `z2-stress`, `z2-edit-cooked`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Panels.md` → "Asset inspector"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Asynchronous changes"

### Z2-R-EDIT-198 Texture table reclaims after two frames
- **Statement:** A released texture slot must keep resolving for frames before its first frame without it, be reclaimed with its generation incremented only two iterations after that frame, and only then destroy its tool texture through the backend's deferred deletion or drop its asset handle.
- **Covers:** Z2-I-EDIT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorTextureTable_ResizeWhileDrawnNeverAliases`, `Test_EditorTextureTable_StaleReferenceDrawsChecker` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Editor texture references"

### Z2-R-EDIT-199 View extensions filter by view ID
- **Statement:** The editor's view extensions must be registered at the editor's startup step 12 with no frame running and add passes only to the views whose IDs they computed (the shell view, the snapshot's scene views), never to a game view or another view of the same kind.
- **Covers:** Z2-I-EDIT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorShell_ExtensionsFilterByViewID` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "ImGui renderer"; `Zenith2/Docs/Design/EDIT/Viewports.md` → "Editor overlay passes"

### Z2-R-EDIT-200 Game views follow the world purpose
- **Statement:** A game view must render the play world's own camera into its viewport's tool texture, gated by the world purpose WORLD_PURPOSE_PLAY and the GameViewTarget resource, never into the swapchain.
- **Covers:** Z2-I-EDIT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorViewports_ViewRequestMembers` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Viewports.md` → "Viewport views"

### Z2-R-EDIT-201 Projection handlers run on every change
- **Statement:** A projection handler registered for a non-scene document type must run after every forward and every inverse change of a document of that type with the document's current state.
- **Covers:** Z2-I-EDIT-01, Z2-I-EDIT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorMaterial_UndoRestoresLiveParameters` in the Test gate step (`ZenithEditor_Tests`, `editor-render`, `win`); `Test_EditorProjection_EachEditKind` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Projection per edit kind"; `Zenith2/Docs/Design/EDIT/Declarations.md` → "Projection change header"

### Z2-R-EDIT-202 Document state replacement keeps the history
- **Statement:** Replacing a document's state by canonical bytes for a document-bytes change, a schema undo or a recovery must set the given state ID and disk digest without pruning or fencing the history.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorDocuments_ReplaceStateKeepsHistory` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Document interface"

### Z2-R-EDIT-206 The EditorFixture test game and scratch copies
- **Statement:** EDIT's tests that need a real game must use the registered game Zenith2/Games/EditorFixture, whose game.json passes the configure and whose tracked tree references no source it does not hold, through a fixture-setup copy under ${ZENITH_OUT}/test-fixtures/EDIT, never writing the tracked game directory.
- **Covers:** Z2-I-EDIT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorFixture_SetupCopiesGames` registered as CTest fixture `EditEditorFixtureGames` in the Test gate step (`ZenithEditor_Tests`, `z2-fixture-setup`, `win`); every `z2-edit-cooked` case runs on that copy ([Z2-N-ARCH-025])
- **Design:** `Zenith2/Docs/Design/EDIT/Tests.md` → "Fixtures"; `Zenith2/Docs/Design/EDIT.md` → "Test game"

### Z2-R-EDIT-207 Script world events on the main thread
- **Statement:** The editor must act on a script world's halt and live-module events only in the main-thread tool hooks after RunFrame, never from the thread that observed them.
- **Covers:** Z2-I-EDIT-10, Z2-I-EDIT-20
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorPlay_HaltedScriptStopsPlay` in the Test gate step (`ZenithEditor_Tests`, `win`); `Test_EditorIntegration_ScriptReloadEndToEnd` in the Test gate step (`ZenithEditor_Tests`, `z2-stress`, `z2-edit-cooked`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor frame"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Runtime tool access"; `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Script reload timing"

### Z2-R-EDIT-211 Tool access calls only in legal hooks
- **Statement:** Every editor call into the runtime tool access must be made only in the hooks or the between-frames position the hook table allows: worlds created, destroyed and extended only in OnInputFrame, OnFrameEnd or between frames, no call from the governor's prologue callbacks or inside the schema window, and the runtime destroyed or re-created only by the run loop between frames.
- **Covers:** Z2-I-EDIT-10, Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorFrame_CallsOnlyInLegalHooks` in the Test gate step (`ZenithEditor_Tests`, `editor-render`, `z2-edit-cooked`, `win`); `Test_EditorCacheConsumer_ShedsAtWarningAndCritical` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Tool access calls per hook"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor run loop"

### Z2-R-EDIT-213 Unknown request outcomes are resolved from server state
- **Statement:** A cook request that ends cancelled, timed out or failed by a server restart must be resolved from the server's reported state (a setSettings through record, a .meta migration through migratePreview) as completed, no effect, partial, changed elsewhere or unknown before any compensation; a change with a synchronous local part must never resolve no effect, so a file operation whose requests are all idempotent (assignGuids, import, reimport) resolves partial at once and runs its documented compensation unchanged; and an unknown settings outcome must be re-resolved from a fresh record, writing the starting settings back only when that record holds the target settings and never overwriting a third writer's change.
- **Covers:** Z2-I-EDIT-01, Z2-I-EDIT-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorAssetOps_SettingsCancelledAfterWriteCommits`, `Test_EditorAssetOps_RenameServerRestartCompensates` in the Test gate step (`ZenithEditor_Tests`, `win`); `Test_EditorHistory_AsyncTimeoutIsInactivity` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Asynchronous changes"; `Zenith2/Docs/Design/EDIT/Declarations.md` → "Change header"

### Z2-R-EDIT-215 Editor-originated writes go through the document model
- **Statement:** Every file write the editor originates for an authoring document must check each open target first, refuse the whole operation for a dirty, conflicted, missing or migrated open document, update an open clean document in memory in the same step with its new bytes, state and disk digest, note its own digests so its notifications never reload or conflict it, and carry expected digests on its inverse writes.
- **Covers:** Z2-I-EDIT-01, Z2-I-EDIT-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorDocuments_PlannedWriteRule`, `Test_EditorDocuments_ExternalChangeReloadsClean` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor-originated writes"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "External changes"

### Z2-R-EDIT-217 Reproducible authoring IDs
- **Statement:** Every authoring ID the editor generates must come from one session random source seeded by --random-seed, the automation script's seed or entropy, with redo reusing recorded IDs and the proxies drawing none, so a seeded windowed and headless run of one script generate the same IDs and save the same bytes.
- **Covers:** Z2-I-EDIT-01, Z2-I-EDIT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorSession_SeededIDsReproducible` in the Test gate step (`ZenithEditor_Tests`, `win`); `Test_EditorHeadless_SavedBytesEqualWindowed` in the Test gate step (`ZenithEditor_Tests`, `editor-render`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Session random source"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Executable and command line"

### Z2-R-EDIT-218 Library creation ticket
- **Statement:** EDIT must create the empty ZenithAuthoring and ZenithEditorCore targets, with DEPENDS ZenithCore, the library root header, the README and one test each, in one Stage 2 ticket that every owner's authoring ticket and EDIT's pull-forward slice block on.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `Test_AuthoringLibrary_VersionLinked` in the Test gate step (`ZenithAuthoring_Tests`, `win`); `Test_EditorIDs_CommandIDHashesText` in the Test gate step (`ZenithEditorCore_Tests`, `win`); the SourceLayout and Libgraph gate step reports for both targets
- **Design:** `Zenith2/Docs/Design/EDIT.md` → "Library creation ticket"; `Zenith2/Docs/Design/EDIT.md` → "ZenithAuthoring contract rules"

### Z2-R-EDIT-219 Deferred changes build from earlier results
- **Statement:** A change whose payload needs an earlier asynchronous change's result must be a deferred operation that builds and applies its scene edits from that result when the history reaches it, inside the same record, and undo must apply its inverses.
- **Covers:** Z2-I-EDIT-01, Z2-I-ENV-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorTerrain_WizardOneRecordPlacesImportedTerrain` in the Test gate step (`ZenithEditor_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Executing a command"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Terrain creation wizard"; `Zenith2/Docs/Design/EDIT/Declarations.md` → "Change header"

### Z2-R-EDIT-221 Host watchers report source and journal changes
- **Statement:** The editor must watch every storage root recursively and the shader reload journal's directory with its own host watchers, independent of the runtime, coalescing reports of one path within the debounce, ignoring dot-prefixed directories, bounding each hand-off queue at 4,096 records and reporting any lost report as a rescan, and route every change to the document table's one external-change entry, the script reload and the journal tailer.
- **Covers:** Z2-I-EDIT-12, Z2-I-EDIT-20
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorHostWatcher_ReportsSourceAndJournalChanges`, `Test_EditorHostWatcher_QueueCoalescesAndBounds`, `Test_EditorDocuments_ExternalChangeReloadsClean` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Foundation.md` → "Host watchers"; `Zenith2/Docs/Design/EDIT/Declarations.md` → "Host watcher header"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "External changes"

### Z2-R-EDIT-224 Every session carries a type registry
- **Statement:** Every editor session, a document-only one included, must be created with a non-null type registry and string table, its services' for a runtime session and caller-owned otherwise, which the document table passes to every document factory, so a document-only session opens, edits, undoes and pastes scene documents.
- **Covers:** Z2-I-EDIT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorUndo_PropertySequences` in the Test gate step (`ZenithEditorCore_Tests`, `z2-stress`, `win`); `Test_EditorClipboard_PasteRemapsIDs` in the Test gate step (`ZenithEditorCore_Tests`, `win`)
- **Design:** `Zenith2/Docs/Design/EDIT/Declarations.md` → "Session description"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Session object"; `Zenith2/Docs/Design/EDIT/Tests.md` → "Fixtures"

## Coverage of inventory items

| Item | Requirements |
|---|---|
| Z2-I-EDIT-01 | Z2-R-EDIT-001, Z2-R-EDIT-002, Z2-R-EDIT-003, Z2-R-EDIT-004, Z2-R-EDIT-005, Z2-R-EDIT-006, Z2-R-EDIT-007, Z2-R-EDIT-008, Z2-R-EDIT-009, Z2-R-EDIT-012, Z2-R-EDIT-013, Z2-R-EDIT-023, Z2-R-EDIT-027, Z2-R-EDIT-028, Z2-R-EDIT-035, Z2-R-EDIT-036, Z2-R-EDIT-037, Z2-R-EDIT-038, Z2-R-EDIT-039, Z2-R-EDIT-040, Z2-R-EDIT-041, Z2-R-EDIT-042, Z2-R-EDIT-043, Z2-R-EDIT-044, Z2-R-EDIT-045, Z2-R-EDIT-048, Z2-R-EDIT-061, Z2-R-EDIT-078, Z2-R-EDIT-090, Z2-R-EDIT-091, Z2-R-EDIT-092, Z2-R-EDIT-095, Z2-R-EDIT-128, Z2-R-EDIT-129, Z2-R-EDIT-135, Z2-R-EDIT-173, Z2-R-EDIT-179, Z2-R-EDIT-180, Z2-R-EDIT-181, Z2-R-EDIT-182, Z2-R-EDIT-183, Z2-R-EDIT-189, Z2-R-EDIT-190, Z2-R-EDIT-191, Z2-R-EDIT-195, Z2-R-EDIT-197, Z2-R-EDIT-201, Z2-R-EDIT-202, Z2-R-EDIT-211, Z2-R-EDIT-213, Z2-R-EDIT-215, Z2-R-EDIT-217, Z2-R-EDIT-218, Z2-R-EDIT-219, Z2-R-EDIT-222, Z2-R-EDIT-223, Z2-R-EDIT-224 |
| Z2-I-EDIT-02 | Z2-R-EDIT-046, Z2-R-EDIT-047, Z2-R-EDIT-048, Z2-R-EDIT-049, Z2-R-EDIT-050, Z2-R-EDIT-051, Z2-R-EDIT-068, Z2-R-EDIT-069, Z2-R-EDIT-070, Z2-R-EDIT-071, Z2-R-EDIT-089, Z2-R-EDIT-090, Z2-R-EDIT-092 |
| Z2-I-EDIT-03 | Z2-R-EDIT-049, Z2-R-EDIT-052, Z2-R-EDIT-053, Z2-R-EDIT-054, Z2-R-EDIT-056, Z2-R-EDIT-063, Z2-R-EDIT-064, Z2-R-EDIT-065, Z2-R-EDIT-066, Z2-R-EDIT-067, Z2-R-EDIT-072, Z2-R-EDIT-073, Z2-R-EDIT-074, Z2-R-EDIT-075, Z2-R-EDIT-076, Z2-R-EDIT-077, Z2-R-EDIT-078, Z2-R-EDIT-079, Z2-R-EDIT-084, Z2-R-EDIT-085, Z2-R-EDIT-136, Z2-R-EDIT-176, Z2-R-EDIT-199, Z2-R-EDIT-209 |
| Z2-I-EDIT-04 | Z2-R-EDIT-080, Z2-R-EDIT-081, Z2-R-EDIT-082, Z2-R-EDIT-083 |
| Z2-I-EDIT-05 | Z2-R-EDIT-055, Z2-R-EDIT-096, Z2-R-EDIT-097, Z2-R-EDIT-098, Z2-R-EDIT-099, Z2-R-EDIT-102, Z2-R-EDIT-107, Z2-R-EDIT-108, Z2-R-EDIT-109, Z2-R-EDIT-110, Z2-R-EDIT-112, Z2-R-EDIT-113, Z2-R-EDIT-114, Z2-R-EDIT-115, Z2-R-EDIT-137, Z2-R-EDIT-193, Z2-R-EDIT-194, Z2-R-EDIT-213, Z2-R-EDIT-216, Z2-R-EDIT-223 |
| Z2-I-EDIT-06 | Z2-R-EDIT-015, Z2-R-EDIT-055, Z2-R-EDIT-058, Z2-R-EDIT-059, Z2-R-EDIT-060, Z2-R-EDIT-100, Z2-R-EDIT-101, Z2-R-EDIT-102, Z2-R-EDIT-103, Z2-R-EDIT-119, Z2-R-EDIT-120, Z2-R-EDIT-121, Z2-R-EDIT-130, Z2-R-EDIT-138, Z2-R-EDIT-162, Z2-R-EDIT-176, Z2-R-EDIT-184, Z2-R-EDIT-198, Z2-R-EDIT-201, Z2-R-EDIT-210, Z2-R-EDIT-220 |
| Z2-I-EDIT-07 | Z2-R-EDIT-013, Z2-R-EDIT-024, Z2-R-EDIT-070, Z2-R-EDIT-125, Z2-R-EDIT-126, Z2-R-EDIT-127 |
| Z2-I-EDIT-08 | Z2-R-EDIT-007, Z2-R-EDIT-152, Z2-R-EDIT-153, Z2-R-EDIT-196, Z2-R-EDIT-212, Z2-R-EDIT-220 |
| Z2-I-EDIT-09 | Z2-R-EDIT-154, Z2-R-EDIT-155, Z2-R-EDIT-175, Z2-R-EDIT-212 |
| Z2-I-EDIT-10 | Z2-R-EDIT-006, Z2-R-EDIT-009, Z2-R-EDIT-010, Z2-R-EDIT-011, Z2-R-EDIT-016, Z2-R-EDIT-017, Z2-R-EDIT-018, Z2-R-EDIT-019, Z2-R-EDIT-020, Z2-R-EDIT-021, Z2-R-EDIT-022, Z2-R-EDIT-023, Z2-R-EDIT-024, Z2-R-EDIT-025, Z2-R-EDIT-026, Z2-R-EDIT-053, Z2-R-EDIT-058, Z2-R-EDIT-123, Z2-R-EDIT-174, Z2-R-EDIT-184, Z2-R-EDIT-185, Z2-R-EDIT-186, Z2-R-EDIT-187, Z2-R-EDIT-188, Z2-R-EDIT-200, Z2-R-EDIT-207, Z2-R-EDIT-211 |
| Z2-I-EDIT-11 | Z2-R-EDIT-034, Z2-R-EDIT-156, Z2-R-EDIT-157, Z2-R-EDIT-158, Z2-R-EDIT-159, Z2-R-EDIT-178 |
| Z2-I-EDIT-12 | Z2-R-EDIT-014, Z2-R-EDIT-031, Z2-R-EDIT-032, Z2-R-EDIT-033, Z2-R-EDIT-034, Z2-R-EDIT-050, Z2-R-EDIT-067, Z2-R-EDIT-146, Z2-R-EDIT-158, Z2-R-EDIT-160, Z2-R-EDIT-161, Z2-R-EDIT-208, Z2-R-EDIT-215, Z2-R-EDIT-221 |
| Z2-I-EDIT-13 | Z2-R-EDIT-002, Z2-R-EDIT-057, Z2-R-EDIT-061, Z2-R-EDIT-062, Z2-R-EDIT-104, Z2-R-EDIT-105, Z2-R-EDIT-124, Z2-R-EDIT-144, Z2-R-EDIT-163, Z2-R-EDIT-164, Z2-R-EDIT-165, Z2-R-EDIT-166, Z2-R-EDIT-167, Z2-R-EDIT-168, Z2-R-EDIT-177, Z2-R-EDIT-192, Z2-R-EDIT-206, Z2-R-EDIT-217 |
| Z2-I-EDIT-14 | Z2-R-EDIT-028, Z2-R-EDIT-079, Z2-R-EDIT-086, Z2-R-EDIT-087, Z2-R-EDIT-088, Z2-R-EDIT-106, Z2-R-EDIT-139 |
| Z2-I-EDIT-15 | Z2-R-EDIT-008, Z2-R-EDIT-029, Z2-R-EDIT-030, Z2-R-EDIT-093, Z2-R-EDIT-094, Z2-R-EDIT-095, Z2-R-EDIT-147, Z2-R-EDIT-151, Z2-R-EDIT-179, Z2-R-EDIT-195, Z2-R-EDIT-196, Z2-R-EDIT-197, Z2-R-EDIT-203, Z2-R-EDIT-204, Z2-R-EDIT-205, Z2-R-EDIT-214, Z2-R-EDIT-216, Z2-R-EDIT-222 |
| Z2-I-EDIT-16 | Z2-R-EDIT-031, Z2-R-EDIT-094, Z2-R-EDIT-148, Z2-R-EDIT-150, Z2-R-EDIT-151, Z2-R-EDIT-205, Z2-R-EDIT-214 |
| Z2-I-EDIT-17 | Z2-R-EDIT-149, Z2-R-EDIT-150, Z2-R-EDIT-151, Z2-R-EDIT-205 |
| Z2-I-EDIT-18 | Z2-R-EDIT-122, Z2-R-EDIT-169 |
| Z2-I-EDIT-19 | Z2-R-EDIT-170 |
| Z2-I-EDIT-20 | Z2-R-EDIT-116, Z2-R-EDIT-117, Z2-R-EDIT-118, Z2-R-EDIT-171, Z2-R-EDIT-207, Z2-R-EDIT-221 |
| Z2-I-EDIT-21 | Z2-R-EDIT-110, Z2-R-EDIT-111, Z2-R-EDIT-172 |
| Z2-I-ANIM-16 | Z2-R-EDIT-140, Z2-R-EDIT-141, Z2-R-EDIT-180 |
| Z2-I-AUDIO-22 | Z2-R-EDIT-143, Z2-R-EDIT-180 |
| Z2-I-ENV-01 | Z2-R-EDIT-128, Z2-R-EDIT-129, Z2-R-EDIT-130, Z2-R-EDIT-131, Z2-R-EDIT-132, Z2-R-EDIT-133, Z2-R-EDIT-185, Z2-R-EDIT-219 |
| Z2-I-ENV-02 | Z2-R-EDIT-133 |
| Z2-I-ENV-03 | Z2-R-EDIT-132, Z2-R-EDIT-134 |
| Z2-I-MOD-09 | Z2-R-EDIT-145 |
| Z2-I-UI-14 | Z2-R-EDIT-142 |

## Excluded and deferred

| Scope | Classification | Decision | Rationale |
|---|---|---|---|
| dynamic editor plugins, third-party editor extensions, mod tools | excluded | Z2-D-024, Z2-D-075 | extensions are static C++ compiled into the editor (the extension table); modding is excluded |
| an editor build for Android, consoles or XR | excluded for Android; deferred to Stage 7 for consoles and XR (Z2-R-EDIT-177) | Z2-D-010, Z2-D-071, Z2-D-072 | the editor is a Windows host tool (`ZENITH_TOOLS_HOST`); other platforms receive editor-authored content through the cook |
| node-graph material and VFX editors | excluded | Z2-D-079 | materials and VFX are authored in code with previews and inspectors |
| multi-user live co-editing of one document | deferred to Stage 7 as per-partition ownership with WORLD (Z2-R-EDIT-178) | Z2-D-048 | merge-friendly files and source control serve concurrent work through Stage 6 |
| automatic source-control operations (commit, push, pull, branch) | excluded | none (an EDIT design choice) | source control is never automatic; the editor offers explicit commands only |
| ImGui multi-viewport OS windows | excluded | none (an EDIT design choice) | one OS window with docking; panels float inside it, so the renderer presents one swapchain |
