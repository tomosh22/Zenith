# GAME design — OpenWorldSlice gameplay and acceptance

This file is part of the GAME design family; the entry document is `Zenith2/Docs/Design/GAME.md`. It specifies what GAME owns inside the flagship slice `Zenith2/Games/OpenWorldSlice/` (`Zenith2/Docs/Design/MOD.md` → "Repository games" lists the game as GAME's): the four game-side Luau samples — interaction, quest, dialogue and inventory (inventory items Z2-I-GAME-11 to -14, game-side by Z2-D-077) — built on the framework without the engine knowing any of them (Z2-I-GAME-10), the slice's gametests, and the acceptance package of `Zenith2/Docs/Budgets.md` → "Flagship slice acceptance package" (Z2-D-021). Other areas own their content in the slice (ENV the terrain and weather, STREAM the cell layout, AI the navigation, CINE the quest scenes, UI the menus and HUD); this file names their hooks only.

## Scope of this file

| Topic | Heading |
|---|---|
| where everything lives, and the rule for game-side tickets | "Slice layout and ticket rule" |
| the four samples | "Gameplay sample rules" |
| the data the samples read | "Sample data tables" |
| title, settings and pause menus, saving, and the accessible-flow route | "Menus, save and accessible-flow route" |
| the slice's online content | "Online content" |
| the dialogue voice locale and character parts | "Voice locale and character parts" |
| time of day and weather | "Time of day and weather" |
| tests of the samples | "Sample tests" |
| the acceptance package | "Acceptance package" |

## Slice layout and ticket rule

```text
Zenith2/Games/OpenWorldSlice/
  game.json                       MOD's format; GAME owns the file (module list of Design/MOD.md "Repository games")
  CMakeLists.txt                  one zenith_add_game call
  Scripts/
    Interaction/Interactable.luau Interaction/Interactor.luau
    Inventory/Inventory.luau      Inventory/Pickup.luau          Inventory/Consumables.luau
    Quest/QuestLog.luau           Quest/Objectives.luau          Quest/ReachZone.luau
    Dialogue/DialogueRunner.luau  Dialogue/Conditions.luau
    Player/CameraInput.luau       Player/PlayerStart.luau
    Data/Items.luau Data/Quests.luau Data/Dialogues.luau
    Acceptance/Asserts.luau       Acceptance/SegmentMarkers.luau
    Tests/                        Luau unit tests, one per sample (not cooked: ZenithCook excludes Tests/)
  Scenes/  Schemas/  Assets/      the integration tickets and the content areas
  Tests/   *.gametest.json, *.input.json (the integration tickets; input scripts hand-written)
  Acceptance/ Playthrough.md Playthrough.replay FeatureChecklist.md Budgets.md BugBar.md SignOff.md
```

**Ticket rule** (`Zenith2/Docs/Requirements/Inventory.md`, classification `game-side`): each sample is delivered by one leaf ticket under the GAME epic with fix version `Z2 Stage 4` whose diff touches only `Zenith2/Games/OpenWorldSlice/Scripts/` and carries the sample's own Luau unit test under `Scripts/Tests/` (tests in the same ticket as the code). No sample ticket touches `Zenith2/Engine/` (games contain no C++ and no engine source, Z2-D-002; game rules never enter the engine, Z2-D-077): a sample that finds the framework short files an engine ticket and blocks on it. Consequently the samples declare no data-defined schema of their own — their persistent state is behaviour state (`Zenith.Field.Map`, `.Array`, `.Table`; `Zenith2/Docs/Scripting.md` → "Declared state schema"), which saves, migrates and hot-reloads like any reflected type, plus GAME's components.

Everything outside `Scripts/` — scenes, prefabs, the `Interactable` tags, the streaming sources, the gametests under `Tests/`, `game.json` and `CMakeLists.txt` — belongs to the integration tickets, which are ordinary `required` work of epic GAME (not game-side), and recorded runs belong to `needs-human` recording tickets in the Z2T-GEOM-015 shape (the WORKER-EVIDENCE ruling: no worker ticket records a run, a count or a timing). The exact titles, which consumers cite (SCRIPT's row SB19 among them):

| Title | Stage, kind | Diff and evidence |
|---|---|---|
| "OpenWorldSlice interaction sample" | S4, game-side leaf | `Scripts/Interaction/*.luau`, `Scripts/Tests/Interaction.test.luau` |
| "OpenWorldSlice inventory sample" | S4, game-side leaf | `Scripts/Inventory/*.luau`, `Scripts/Data/Items.luau`, `Scripts/Tests/Inventory.test.luau` |
| "OpenWorldSlice quest sample" | S4, game-side leaf | `Scripts/Quest/*.luau`, `Scripts/Data/Quests.luau`, `Scripts/Tests/QuestLog.test.luau` |
| "OpenWorldSlice dialogue sample" | S4, game-side leaf | `Scripts/Dialogue/*.luau`, `Scripts/Data/Dialogues.luau`, `Scripts/Tests/DialogueRunner.test.luau`, `Scripts/Tests/DataTables.test.luau` (blocked by the other three, whose tables it cross-checks) |
| "OpenWorldSlice sample integration" | S4, required (epic GAME), blocked by the four sample tickets | the scenes and prefabs placing the samples, `Scripts/Player/*.luau`, `Scripts/Acceptance/Asserts.luau` and `SegmentMarkers.luau` for segments S01–S04, the pawn's and camera's `Zenith.Streaming.Source` (below), and every gametest of § "Sample tests" with its hand-written `.input.json`; its evidence is those gametests registered with CTest, which the Test gate step runs |
| "OpenWorldSlice playthrough recording S01–S04" | S4, `needs-human` recording (Z2T-GEOM-015 shape), blocked by the integration ticket | the human records `Acceptance/Playthrough.replay` for S01–S04 with `input.recordScript` in a windowed session and fills the segments' `Start` and `Duration` columns of `Playthrough.md` from that run |
| "OpenWorldSlice playthrough integration" | S6, required (epic GAME) | the content, asserts and segment markers of S05–S08, and the four gametests `Tests/PlaythroughS05.gametest.json` … `Tests/PlaythroughS08.gametest.json`, each with its hand-written `.input.json` and its start scene `Tests/PlaythroughS05` … `Tests/PlaythroughS08` (§ "Sample tests"); its evidence is those gametests registered with CTest (`Game_OpenWorldSlice_PlaythroughS05` … `_PlaythroughS08`), which the Test gate step runs |
| "OpenWorldSlice full playthrough recording" | S6, `needs-human` recording, blocked by the S6 integration ticket | the full `Playthrough.replay` (S01–S08) and every segment's timing |
| "Stage 1 flagship acceptance ticket for OpenWorldSlice" | S1, `needs-human` (the producer name of `Zenith2/Docs/Budgets.md`) | the package definition files of § "Acceptance package" |

The playthrough scripts of the other two flagship slices are not GAME's: CinematicSlice's belongs to CINE and Game2D's to TWOD, the areas `Zenith2/Docs/Design/MOD.md` → "Repository games" assigns those games to, each through its own Stage 1 flagship acceptance ticket and recording tickets.

Streaming sources (STREAM's row ST18): the sample integration ticket puts `Zenith.Streaming.Source` on the player pawn (with `Primary` set) and on the camera entity, and `Scripts/Player/PlayerStart.luau` reads the pawn's source `Ready` and `MissingRequiredCells` to hold the loading screen and post-teleport input until the required cells are active, and `TeleportSequence` to detect a completed teleport; STREAM never pauses the simulation (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Teleports and readiness").

## Gameplay sample rules

All four use only `@zenith`, `@types`, `@gameplay/*` and the bindings of other modules, and they talk to each other only through `Zenith.Gameplay.Message` channels (`Zenith2/Docs/Design/GAME/Framework.md` → "The message event"), so any one can be replaced without touching the others:

| Channel | Sender → target | Payload / Magnitude |
|---|---|---|
| `Interaction.Available`, `Interaction.Unavailable` | interactable → pawn | the prompt key / — |
| `Interaction.Used` | pawn → interactable | the verb / — |
| `Inventory.Add`, `Inventory.Remove` | anyone → inventory owner | item ID / count |
| `Inventory.Changed` | inventory owner → itself (broadcast to subscribers of the owner) | item ID / new count |
| `Inventory.Rejected` | inventory owner → instigator | item ID / requested count |
| `Quest.Start` | anyone → quest log owner | quest ID / — |
| `Quest.Started`, `Quest.StepCompleted`, `Quest.Completed` | quest log owner → itself | quest ID / step index |
| `Quest.Progress` | anyone → quest log owner | objective target / amount |
| `Dialogue.Line`, `Dialogue.Choices` | runner → player | node ID / choice count |
| `Dialogue.Ended` | runner → player | dialogue ID / chosen exit index |

The player pawn carries the tag `Player.Pawn` (authored), `Zenith.Gameplay.Tags`, `.Attributes` (Health, Stamina), `.Effects`, `.Abilities`, `.Timers`, `.StateMachines`, `.Actions` and `.Pawn` (auto-possess); the player entity carries `Zenith.Gameplay.Player` and the camera director's reference; the pawn and the camera entity carry `Zenith.Streaming.Source` (the pawn's with `Primary`, § "Slice layout and ticket rule"); the inventory and quest log behaviours run on the pawn, so they carry over with it through a world transition (`Zenith.Runtime.CarryOver` on the pawn; `Zenith2/Docs/Design/GAME/WorldHost.md` → "Carry-over").

### Interaction

| Module | Kind, phase | Rule |
|---|---|---|
| `Interaction/Interactable.luau` | behaviour, Late | on the physics trigger events of its own trigger volume (PHYS's `Zenith.Physics.TriggerEntered`/`TriggerExited`, assumed below) by an entity whose tags match `Player.Pawn`: sends `Interaction.Available` (payload: its declared `Prompt` state field) or `Interaction.Unavailable` to that pawn; on `Interaction.Used` targeted at itself, sends the message on its configured `UseChannel` (state field; for example `Door.Toggle`, `Pickup.Take`, `Dialogue.Begin`) to itself, which the entity's own feature behaviour handles |
| `Interaction/Interactor.luau` | behaviour, Late, on the pawn | state `Candidates = Field.Array(Field.Entity(), 8)`; `Available` appends (a ninth is ignored), `Unavailable` removes; each tick in `Run`, the focus is the candidate with the smallest squared distance from the pawn's transform (views of the candidates' `T.Transform`; ties: the lower `entity:Id()`), written to state `Focus`; when `Input.WasPressed(ctx, "Gameplay.Interact")` and `Focus` is set, sends `Interaction.Used` (target `Focus`, instigator the pawn) |

<!-- assumption: pending PHYS -->
The Interactable sample assumes PHYS emits trigger-overlap events with the trigger's and the other body's entities, delivered in phase 9 ("`Zenith.Physics.TriggerEntered { Trigger, Other }`" and "`TriggerExited`"). Cross-area row GA3 (c); if PHYS's names differ, only `Interactable.luau` changes.
<!-- /assumption -->

### Inventory

`Inventory/Inventory.luau` (behaviour, Late, on the pawn):

```luau
State = {
	Items = Zenith.Field.Map(Zenith.Field.String(), Zenith.Field.U32(0), 64),   -- item ID → count
	Revision = Zenith.Field.U32(0),
}
```

| Message | Rule |
|---|---|
| `Inventory.Add (item, n)` | `item` must be in `Data/Items.luau` (else `Inventory.Rejected`); new = min(count + n, the item's `MaxStack`); a 65th distinct item is rejected; on a change `Revision += 1` and `Inventory.Changed (item, new)`; the overflow above `MaxStack` is rejected with `Inventory.Rejected (item, overflow)` |
| `Inventory.Remove (item, n)` | n > count → `Inventory.Rejected`, nothing changes (all or nothing); a count reaching 0 removes the key |
| `Inventory.Use (item)` (from the HUD) | a consumable (`Data/Items.luau` `Use = { Effect = … }`) is removed by one and its effect applied to the pawn with `Zenith.Gameplay.ApplyEffect(ctx, pawn, pawn, Effects.Spec(…))` — a potion is a GAME effect with an `ADD` modifier on `Health` (`Inventory/Consumables.luau` builds the specs once at module scope) |

`Inventory/Pickup.luau` (behaviour, Late): on `Pickup.Take` sends `Inventory.Add (ItemId, Count)` to the instigator and destroys itself with `ctx:Destroy(self.Entity)` only after the inventory's `Inventory.Changed` naming its item arrives in a subsequent tick (the pickup keeps state `Pending = true` meanwhile, and a `Rejected` clears it), so a full inventory never eats a pickup.

### Quest

`Quest/QuestLog.luau` (behaviour, Late, on the pawn):

```luau
State = {
	Quests = Zenith.Field.Map(Zenith.Field.String(), Zenith.Field.Table({
		Step = Zenith.Field.U32(1),          -- index into the quest's Steps
		Progress = Zenith.Field.U32(0),      -- toward the step's objective count
		Status = Zenith.Field.Enum({ "Active", "Completed", "Failed" }, "Active"),
	}), 32),
}
```

| Input | Rule |
|---|---|
| `Quest.Start (quest)` | a quest in `Data/Quests.luau` not yet in `Quests` is added at step 1; records `Zenith.Gameplay.AddTag(ctx, pawn, "Quest.<quest>.Active")`; sends `Quest.Started` |
| `Inventory.Changed (item, count)` | for each active quest whose step objective is `{ Kind = "Collect", Item = item, Count = c }`: `Progress = min(count, c)` |
| `Interaction.Used`, `Dialogue.Ended`, `Quest.Progress` | objectives `Interact`, `Talk` (dialogue ID and exit index) and `Reach` (sent by `Quest/ReachZone.luau`, a trigger behaviour) add 1 to `Progress` |
| progress reaches the objective count | `Quest.StepCompleted`; the step's `Rewards` are sent as `Inventory.Add`; the next step starts; after the last step `Status = "Completed"`, the tag `Quest.<quest>.Active` is removed and `Quest.<quest>.Done` added, `Quest.Completed` sent |
| a step with `StartCinematic = <asset>` | records CINE's start command for the quest scene (`Zenith_RecordStartCinematic` through CINE's binding); the step completes on CINE's end or skip event |

Quest flags are GAME tags on the pawn, so dialogue conditions, doors and AI read them with `Tags.Has` and no quest code: the slice has at most 6 quests, 12 tags, within the 16-tag limit with room for the status tags of effects (`Zenith2/Docs/Design/GAME/Framework.md` → "Tag component").

### Dialogue

`Dialogue/DialogueRunner.luau` (behaviour, Late, on each NPC) runs a dialogue graph of `Data/Dialogues.luau`:

1. On `Dialogue.Begin` (from its `Interactable`): if the pawn already carries `Dialogue.Active`, ignore; otherwise `AddTag(ctx, pawn, "Dialogue.Active")`, `Input.PushContext(ctx, "Dialogue")` (`Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Script-facing input API"), set the NPC's machine `Dialogue` to the start node (`StateMachine` helper), and enter its coroutine loop.
2. Per node: send `Dialogue.Line (node)` to the player for the HUD (UI's dialogue panel subscribes; GA13), then for the node's choices whose `Condition` holds (`Dialogue/Conditions.luau`: `{ HasTag = "Quest.Herbs.Done" }`, `{ HasItem = "Herb", Count = 3 }` — the latter answered from the last `Inventory.Changed` values the runner cached) send `Dialogue.Choices (count)`.
3. Wait: either `Input.WasPressed(ctx, "Dialogue.Advance")` / `"Dialogue.Choose1"` … `"Dialogue.Choose4"`, or, for a node with `AutoAdvanceSeconds`, a GAME timer started with `StartTimer(ctx, self.Entity, "Dialogue.Auto", s)` and awaited with `Async.Await` on a `WaitTimer` action — one wait loop, `Zenith.WaitTicks(ctx, 1)` per tick.
4. A choice's `Effects` are messages sent on the player's behalf (`Quest.Start`, `Inventory.Remove`); its `Next` is the next node; a node without `Next` ends the dialogue: `RemoveTag(ctx, pawn, "Dialogue.Active")`, `Input.PopContext`, `Dialogue.Ended (dialogue, exit index)`.

`OnRecover` (after a save load or hot reload) reads the machine `Dialogue` and restarts at that node: the node is the only position that matters, so no coroutine position is ever serialized ([Z2-N-SCRIPT-393]).

## Sample data tables

The tables are frozen Luau modules (constants at module scope, allowed for chunk systems and behaviours), validated by `Scripts/Tests/DataTables.test.luau`:

| Module | Shape |
|---|---|
| `Data/Items.luau` | `{ [itemId]: { Name: string (localisation key), MaxStack: number (1–999), Use: { Effect: EffectSpecArgs }? } }`; item IDs match `^[A-Z][A-Za-z0-9]{0,31}$` |
| `Data/Quests.luau` | `{ [questId]: { Title: string, Steps: { { Objective: { Kind: "Collect" \| "Interact" \| "Talk" \| "Reach", Item: string?, Target: string?, Dialogue: string?, Exit: number?, Count: number }, Rewards: { { Item: string, Count: number } }?, StartCinematic: string? } } } }` (at most 12 steps) |
| `Data/Dialogues.luau` | `{ [dialogueId]: { Start: string, Nodes: { [nodeId]: { Speaker: string, Line: string, AutoAdvanceSeconds: number?, Choices: { { Text: string, Condition: Condition?, Effects: { Message }?, Next: string? } }?, Next: string? } } } }` (at most 64 nodes, 4 choices per node) |

Every cross-reference (a quest naming an item, a dialogue choice starting a quest, a `Next` naming a node) is checked by the data test; a dangling reference fails it with the table path.

## Menus, save and accessible-flow route

Stage 4 content (cross-area rows UI24 and UiB15, SaB9), game-side Luau and data only (Z2-D-077, Z2-D-002), written by one leaf ticket:

| File (under `Zenith2/Games/OpenWorldSlice/` unless absolute) | Content |
|---|---|
| `Scenes/Title.scene.json` | the title world: a camera with `Zenith.Render.Camera`, the session entity carrying `Scripts/UI/TitleMenu.luau`; it becomes the game's startup scene (`Config/game.config.json` `content.startupScene`) |
| `Assets/UI/MainMenu.ui.json` | the buttons `NewGame`, `Continue`, `Settings`, `Quit` in that navigation order |
| `Assets/UI/Settings.ui.json` | the selection `DialogueLanguage` (the project's dialogue locales), the engine accessibility settings screen embedded as UI's instance, and `Back` |
| `Assets/UI/PauseMenu.ui.json` | `Resume` (`presentationAction` `resumeWorld`), `Save`, `Load` (`resumeWorld`), `QuitToTitle` (`resumeWorld`) |
| `Scripts/UI/TitleMenu.luau` | a Late behaviour: `NewGame` requests `Scene.RequestTransition` to `Scenes/Wake`; `Continue` calls `Save.Load` with the newest slot of `Save.ListSlots()` (disabled when none); `Settings` shows the settings screen; `Quit` calls `Game.Quit`; the title holds `NewGame` and `Continue` disabled until the first `Online.IdentityChanged` or 10 s (§ "Online content") |
| `Scripts/UI/PauseMenu.luau` | a Late behaviour on the player's session entity: the action `Menu.Pause` shows the pause screen and calls `Game.RequestPause(ctx, "Menu")`; in the first resumed tick after a `resumeWorld` button's `UI.Activated`: `Save` calls `Save.Request("manual", "Manual save")` and closes the menu, `Load` follows segment S07's sequence (§ "Acceptance package"), `QuitToTitle` requests `Scene.RequestTransition` to `Scenes/Title` |
| `Localization/en/menus.strings.json` | every visible menu string |
| `Zenith2/Tests/UIAccessibility/Routes/OpenWorldSlice.route.json` | the route of `Zenith2/Docs/Design/UI/Accessibility.md` → "End-to-end accessible flows": `gateTicks` 1,800; title → `Settings` (navigate, submit, wait for `UI.Activated` `Settings`), change `DialogueLanguage` once, `Back`, `NewGame` (wait for `UI.Activated` `NewGame`, timeout 600), then the first ten minutes as the S01–S04 action steps of `Tests/PlaythroughS01.input.json` and the S02–S04 steps of `Playthrough.md` restated as route steps (`Player.Move`, `Gameplay.Interact`, `Menu.Pause` values at the segment ticks) |

Every existing gametest selects its own start scene through `set` (`content.startupScene`); the two that start at Wake (`PlaythroughS01` and `PlaythroughRecordedS01S04`) gain `"content.startupScene": "content:/Scenes/Wake.scene.json"` in the same diff, so the new startup scene changes no existing test. The save menu and the quit-relaunch-load segment are what SAVE's `needs-human` recording of the save segment plays (row SaB9); segment S07 (Stage 6) reuses this pause menu.

## Online content

Stage 4 content of `Zenith2/Docs/Design/ONLINE.md` → "Flagship slice integration" (rows ON10, ON21, ON23 (b)), game-side: exactly the six files that section lists, with OpenWorldSlice's catalogue — eight achievements — six unlocked once, `ows.wake` (S01 done), `ows.herbs` (three herbs held), `ows.elder` (`Quest.Herbs.Active`), `ows.interior` (S04 done), `ows.saved` (a manual save written) and `ows.complete` (`Quest.Herbs.Done`), and two incremental, `ows.herbalist` (30 herbs, step 1) and `ows.wanderer` (10 distinct streaming cells entered, step 1); the entitlement `ows.dlc.lantern` (the sample DLC, a lantern item added to the inventory table when owned); cloud kinds manual, quick and autosave. Each achievement is earned by the gameplay script whose state names it (Z2-D-077: what earns an achievement is game-side), through `Online.UnlockAchievement`/`Online.IncrementAchievement`. `Scripts/Online/AccountFlow.luau` and `CloudConflictFlow.luau` follow that section's behaviour, including ON21's `Online.QueryCloudSlot` confirmation and the `IndexUnreadable` repair message and ON23 (b)'s reasons `SignedIn`, `Switched` and `Attached` handled alike; the title world's hold is `TitleMenu.luau`'s (§ "Menus, save and accessible-flow route"). The world host side of ON23 (a), carrying nothing over across a profile change, is `Zenith2/Docs/Design/GAME/WorldHost.md` → "Carry-over" rule 5.

## Voice locale and character parts

The slice applies CHAR's voice-locale model (`Zenith2/Docs/Design/CHAR/Facial.md` → "Voice locale as a simulation input"; rows CH4, CH18) as game-side content:

1. `Schemas/VoiceLocale.schema.json` declares the component `OpenWorldSlice.VoiceLocale` (one field `Tag`, string of at most 16 bytes, authority `CommandsOnly`), placed on the session entity of every gameplay scene and saved with it (rule 4).
2. `Scripts/Settings/VoiceLocaleSettings.luau` (a Late behaviour on the session entity) is the only writer of `Tag` (rules 1 and 2): it records the change when the recorded effective-dialogue-locale input arrives — on a user change of `DialogueLanguage` (rule 3) and at the first tick after a restore whose tag differs (rule 5). That input is an engine path, not game content: a recorded event carrying the effective tag AUDIO will use, delivered at a tick boundary and recorded with the tick's inputs like PLAT's `Platform.LocaleChanged`; it is requested as A5 cross-area row GmB33, and until it exists the script is written against its stated payload and its ticket stays `draft`.
3. The dialogue sample's runner plays each line's lip-sync clip, when the line names one, under CHAR's "Lip-sync playback contract", reading the tag from `OpenWorldSlice.VoiceLocale`, never from `audio.dialogue.locale` (rule 6; CH4 (c)).
4. Equipment is shown by part visibility (CH4 (b), `Zenith2/Docs/Design/CHAR/Modular.md` → "Runtime part switching"): equipping the `ows.dlc.lantern` item records `SetProperty` of the pawn's lantern part's `Zenith.Render.MeshInstance` property 15 to visible, unequipping to hidden.

## Time of day and weather

`Scripts/World/DayNight.luau` (a Late behaviour on the session entity of the outdoor scenes; rows EN13, EnB19's day/night script) consumes ENV's gameplay events (`Zenith2/Docs/Design/ENV/WaterWeather.md` → "Weather events for gameplay"): at `WorldStarted` with origin `STARTUP` or `TRANSITION` and no saved clock it calls `Environment.SetTimeOfDay(8)` and `Environment.SetTimeScale(scale)` with `scale` the session entity's `OpenWorldSlice.DayNightConfig` field `TimeScale` (`Schemas/DayNightConfig.schema.json`, one f32 field, authority `CommandsOnly`; 60 in the slice's scenes, one game hour per minute); on `Zenith.Environment.TimeOfDay` with marker `SUNSET` it sets the state machine `Lamp` of every `World.Lamp`-tagged entity to `On` and on `SUNRISE` to `Off` (`Gameplay.SetState`); at the `HOUR` marker of hour 12 of each day it calls `Environment.RequestWeather("Rain", 30)` and at hour 14 `Environment.RequestWeather("Clear", 30)`; on `Zenith.Environment.WeatherChanged` with `bArrived` and a rain intensity above 0.5 it adds the tag `World.Raining` to the session entity, and removes it on the arrival of a state without rain. The lamp and rain prefabs carry VFX and AUDIO emitter components spawned by ENV (row EN13), not by this script. The Stage 4 W-Outdoor content, the W-OpenWorld and W-Cinematic milestones and the WS-SUSTAIN procedure of row EnB19 are not OpenWorldSlice's content.

## Sample tests

| Test | Kind, ticket | Proves |
|---|---|---|
| `Scripts/Tests/Interaction.test.luau` | Luau unit over a mock `ctx` (SCRIPT's runner), the interaction sample ticket | focus selection (smallest squared distance, ties to the lower `entity:Id()`, a ninth candidate ignored); `Available` and `Unavailable` add and remove candidates; `Interaction.Used` is sent to the focus with the pawn as instigator only on the `Gameplay.Interact` press; an `Interactable` routes `Interaction.Used` to its `UseChannel` |
| `Scripts/Tests/DataTables.test.luau` | Luau unit, the dialogue sample ticket | § "Sample data tables" shapes and references |
| `Scripts/Tests/Inventory.test.luau` | Luau unit over a mock `ctx`, the inventory sample ticket | add, stack cap, 65th item, all-or-nothing remove, consumable effect spec |
| `Scripts/Tests/QuestLog.test.luau` | Luau unit, the quest sample ticket | step progression for each objective kind, rewards, tags recorded |
| `Scripts/Tests/DialogueRunner.test.luau` | Luau unit, the dialogue sample ticket | conditions, choices, end, `OnRecover` restarting at the saved node |
| `Tests/InteractionFlow.gametest.json` + `.input.json` | gametest (`Zenith2/Docs/Design/MOD.md` → "The gametest format"), 600 ticks, scene `Tests/InteractionArena`; the sample integration ticket | walking into a pickup's volume and pressing Interact adds the item; `Acceptance/Asserts.luau` calls `Game.ReportTestFailure` when the inventory does not hold it by tick 590 |
| `Tests/InteractionFlowNoPress.gametest.json` + `.input.json` | gametest, 600 ticks, `set` `content.startupScene` = `content:/Tests/InteractionArenaNoPress.scene.json` (the arena with `Acceptance/NoPressAsserts.luau` in place of `Acceptance/Asserts.luau`); the sample integration ticket | an input that walks into the pickup's volume and never presses Interact: the assert behaviour calls `Game.ReportTestFailure` when the inventory holds the item or `Interaction.Used` arrived by tick 590, so the gametest passes on exit code 0 exactly when nothing was picked up (MOD's gametest passes on exit code 0 alone) |
| `Tests/QuestFlow.gametest.json` | gametest, 3,600 ticks; the sample integration ticket | start, collect, talk, complete; the `Quest.Herbs.Done` tag present at the end |
| `Tests/DialogueFlowA.gametest.json`, `Tests/DialogueFlowB.gametest.json` | two gametests, 1,800 ticks each (CTest names `Game_OpenWorldSlice_DialogueFlowA` and `_DialogueFlowB`, from the file stems, `Zenith2/Docs/Design/MOD/GameBuild.md` → "Game test registration"); the sample integration ticket | a two-choice dialogue reached through each branch, one input script per file |
| `Tests/SaveLoadQuest.gametest.json` | gametest, 2,400 ticks (Stage 4, with SAVE); the sample integration ticket | save mid-quest, load, finish: the restored quest log, inventory and dialogue node equal the saved ones |
| `Tests/TransitionCarry.gametest.json` | gametest, 1,200 ticks; the sample integration ticket | entering the village interior (a scene transition) carries the pawn with inventory, quest log and an active effect with its remaining ticks |
| `Tests/PlaythroughS01.gametest.json` + `.input.json` | gametest, the length of segment S01; the sample integration ticket | the segment S01 assertions of § "Acceptance package" (pawn possessed, follow rig active); its input script is hand-written from the segment's steps (spec-derivable, the GOLDEN-FILES ruling), never cut from a recorded replay |
| `Tests/PlaythroughS05.gametest.json` … `Tests/PlaythroughS08.gametest.json`, each + `.input.json` | four gametests (Stage 6), each 7,200 ticks (the minimum run length's 120 s at 60 Hz, [Z2-N-BUDGET-424]) and each on its start scene `Tests/PlaythroughS0<n>` — the segment's `Checkpoint` of `Playthrough.md` rebuilt as a scene: the pawn at the segment's start with the inventory, quest-log and tag state the earlier segments leave, applied by a setup behaviour from the `Data/` tables (no recorded save is read); the playthrough integration ticket | the segment's end assertions of § "Acceptance package" — S05: Health above 0 and the dash ability's cooldown observed; S06: the director resumed with a blend and the restored properties equal their snapshots; S07: the loaded state equals the saved state after a save, a quit to the menu (MENU pause) and a load chosen from it, in the order of the S07 row of § "Acceptance package"; S08: `Quest.Herbs.Done` present and the reward item held — checked by `Acceptance/Asserts.luau`; each input script is hand-written from the segment's steps (the GOLDEN-FILES ruling), never cut from the recorded replay |

| `Tests/PlaythroughS07NoLoad.gametest.json` + `.input.json` | gametest (Stage 6), 7,200 ticks on `Tests/PlaythroughS07` (`set` `content.startupScene`); the Stage 6 segment integration ticket | an input that opens the pause menu and presses `Resume`, never `Load`: the assert behaviour calls `Game.ReportTestFailure` when the world's `sessionOrdinal` (`Game.GetWorldState`) changed or no `UI.Activated` of `Resume` arrived by the last tick, so the run passes on exit code 0 exactly when no load happened |
| `Tests/TitleToPlay.gametest.json` + `.input.json` | gametest (Stage 4), 3,000 ticks on `Tests/TitleToPlay` (`Scenes/Title` plus a carried-over assert entity); the menus ticket | title → `NewGame` → Wake → pause → `Save` → `QuitToTitle` → `Continue`: the assert entity, carried through both transitions and saved with Wake, checks `manual` in `Save.ListSlots()` back at the title and the possessed pawn in the loaded world |
| `Tests/OnlineCatalogue.gametest.json` + `.input.json` | gametest (Stage 4), 2,400 ticks on `Tests/InteractionArena` with three herbs, offline backend | `Online.QueryAchievement(ctx, "ows.herbs")` reports unlocked after the third pickup and `ows.herbalist` progress 3 |
| `Tests/DayNight.gametest.json` | gametest (Stage 4), 3,600 ticks, `set` `content.startupScene` = `content:/Tests/DayNightArena.scene.json` (the village layout with `DayNightConfig.TimeScale` 720, twelve game hours per real minute) | lamps `On` after the first sunset and `Off` after the next sunrise, `World.Raining` added after 12:00 and removed after 14:00 |
| `Scripts/Tests/DayNight.test.luau` | Luau unit over a mock `ctx`, the day/night ticket | each marker's action; a `WeatherChanged` without `bArrived` changes nothing |
| `Scripts/Tests/VoiceLocale.test.luau` | Luau unit over a mock `ctx`, the voice-locale ticket | the recorded input changes `Tag` once; an equal tag records nothing; the post-restore comparison issues one change |

The CTest names are `Game_OpenWorldSlice_<Stem>` from the file stems (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Game test registration"): `Game_OpenWorldSlice_InteractionFlow`, `Game_OpenWorldSlice_QuestFlow`, `Game_OpenWorldSlice_DialogueFlowA`, `Game_OpenWorldSlice_DialogueFlowB`, `Game_OpenWorldSlice_SaveLoadQuest`, `Game_OpenWorldSlice_TransitionCarry` and `Game_OpenWorldSlice_PlaythroughS01` (Stage 4), and `Game_OpenWorldSlice_PlaythroughS05`, `_PlaythroughS06`, `_PlaythroughS07` and `_PlaythroughS08` (Stage 6). Each gametest asserts with `Acceptance/Asserts.luau`, a Late behaviour placed by the test scene that checks its segment's assertions and calls `Zenith.Game.ReportTestFailure(ctx, "<assertion text>")` on a failure, so the run exits with code 5 (`Zenith2/Docs/Design/GAME/WorldHost.md` → "Game script bindings"). `Test_Gameplay_RegistersNoGameNames` (`Zenith2/Docs/Design/GAME/Tests.md` → "ZenithGameplay unit tests") is the engine-side proof that none of these names exists in the engine.

## Acceptance package

The package is `Zenith2/Docs/Budgets.md` → "Flagship slice acceptance package" ([Z2-N-BUDGET-420] … [Z2-N-BUDGET-432]), produced by the Stage 1 flagship acceptance ticket for OpenWorldSlice (its definition files), completed in content by the integration tickets of Stages 4 and 6, and in recorded evidence by the two `needs-human` recording tickets (the WORKER-EVIDENCE ruling; titles in § "Slice layout and ticket rule").

| File | Written by | Content |
|---|---|---|
| `Playthrough.md` | Stage 1 acceptance ticket (segments, checkpoints, feature and requirement IDs, assertions); the `Start` and `Duration` columns by "OpenWorldSlice playthrough recording S01–S04" (S01–S04) and "OpenWorldSlice full playthrough recording" (all), from the run each records | the table of [Z2-N-BUDGET-423]; segments below |
| `Playthrough.replay` | "OpenWorldSlice playthrough recording S01–S04" (segments S01–S04), then "OpenWorldSlice full playthrough recording" (all) — both `needs-human`, recorded by the human in a windowed session | a `zenith2.input-script` version 1 JSON text file (`Zenith2/Docs/Design/PLAT/SimulationInput.md` → "The input-script format"), `exclusive: true`; the Player plays it with `--input-script`; no worker ticket writes or edits it |
| `FeatureChecklist.md` | Stage 1 ticket | the table of [Z2-N-BUDGET-425]; every GAME requirement assigned to the slice (the Coverage table of `Zenith2/Docs/Requirements/GAME.md`) appears in an item |
| `Budgets.md` | Stage 1 ticket | the row IDs of [Z2-N-BUDGET-426]'s OpenWorldSlice line, including Z2-B-GAME-001…003 and Z2-B-SHIP-019/-020/-023; no values |
| `BugBar.md` | Stage 1 ticket | the bar of [Z2-N-BUDGET-427] with the cap row Z2-B-SHIP-020 |
| `SignOff.md` | Stage 1 ticket (items); `needs-human` tickets (verdicts, [Z2-N-BUDGET-431]) | subjective items only: camera feel (follow lag, collision pull-in, blends), dialogue pacing, quest clarity |

Playthrough segments (each at least the minimum run length, [Z2-N-BUDGET-424]; total 20–30 minutes, Z2-B-SHIP-023):

| Segment | Content exercised | GAME features | Assertions at the end |
|---|---|---|---|
| S01 Wake | startup, auto-possession, camera follow, look input | WorldStarted, possession, cameras | the pawn is possessed; the director's active rig is the follow rig |
| S02 Village | interaction, pickups, inventory | interaction, inventory, messages | 3 herbs held |
| S03 Elder | dialogue, quest start | dialogue, quest, tags, input contexts | `Quest.Herbs.Active` present; the `Dialogue` input context popped |
| S04 Interior | world transition into the elder's house and back | transitions, carry-over, gameplay time | inventory, quest log and effects equal across both swaps |
| S05 Forest | streaming at scale, AI encounters, abilities and effects (Stage 6) | abilities, effects, attributes, async actions | Health above 0; the dash ability's cooldown observed |
| S06 Ruins | cinematic quest scene, skip (Stage 6) | cinematic suspension, restore | the director resumed with a blend; restored properties equal their snapshots |
| S07 Save | save, quit to menu, load (Stage 6): a behaviour saves; the pause-menu script opens the menu screen and requests `Game.RequestPause(ctx, "Menu")`; the menu's Load button carries the `presentationAction` `resumeWorld`, so UI's C++ removes `WORLD_PAUSE_REASON_MENU` (`Zenith2/Docs/Design/UI/Interaction.md` → "Pause menus" steps 1–3; a MENU-paused world runs no script and publishes nothing, so no script could issue the load while paused) and then publishes the button's `UI.Activated` event, which the slice's menu Late behaviour handles in the first resumed tick by closing the menu, showing the loading screen and calling `Save.Load(slot)`; the old world keeps ticking behind the loading screen until the swap (`Zenith2/Docs/Design/GAME/WorldHost.md` → "Transition sources") | pause (MENU), save-load transition | the loaded state equals the saved state |
| S08 Return | quest completion, rewards (Stage 6) | quest, inventory | `Quest.Herbs.Done` present; reward item held |

<!-- assumption: pending PLAT -->
The recording tickets' sessions need the development console commands `input.recordScript <path>` and `input.stopRecordScript`, which write the input world's phase-1 action commands as `zenith2.input-script` events (one event per action whose value changed, ticks counted across world swaps like `--exit-after-ticks`), and PLAT's input-script `tick` must count primary-world ticks summed across swaps, because segment S04 swaps worlds. Cross-area row GA6.
<!-- /assumption -->

The acceptance runs are the stage acceptance tickets' (category `Zenith2Acceptance`, run by a human, Z2-D-083): "Stage 4 acceptance" plays the S01–S04 replay and "Stage 6 acceptance" the full one, on Windows and the Galaxy S24 Ultra, with `--input-script` and `--exit-after-ticks` set to the script's last tick plus 300, record the budget evidence of the rows in `Budgets.md`, and fail when any segment assertion reported a failure (exit code 5); those run records are the evidence of the first-playable and full-playthrough requirements. The integration smoke runs segment S01 headless as the gametest `Tests/PlaythroughS01.gametest.json` with its hand-written input script, so every engine change that breaks the slice's opening is caught by `Game_OpenWorldSlice_PlaythroughS01` in CI, independent of any recording; from Stage 6 the four segment gametests `Game_OpenWorldSlice_PlaythroughS05` … `_PlaythroughS08` smoke segments S05–S08 the same way, each from its own start scene.
