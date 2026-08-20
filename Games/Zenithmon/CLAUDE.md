# Zenithmon

Pokemon Sword/Shield-class monster-collecting RPG built entirely on Zenith
systems: ~150 original species, 8 gyms, turn-based battles, all assets
procedurally generated and baked by tools builds.

> **Read `Docs/Status.md` first each session** — it carries the pinned baselines,
> the committed-asset hashes and the current task; the S0-S7 narrative moved
> verbatim to `Docs/History.md` on 2026-08-18. Then `Docs/Roadmap.md` for the
> S0-S12 stage plan, which now carries the board keys, and `Docs/Board.md` for how
> the docs and the board relate. `Docs/AgentBriefing.md` is the session onboarding guide;
> `Docs/MasterPlan.md` is the full approved program plan behind the Roadmap;
> `Docs/Scope.md` is the binding in/out list. Game code uses the `ZM_` prefix.
> Unattended development runs on `Docs/StartPrompts.md` prompt 0 (the
> lifecycle loop).

Current stage, verified test baseline, and in-flight work live in
`Docs/Status.md`; do not duplicate that fast-moving state here. Operating
policy is ZM-D-031: work directly on `master`, never create a branch, PR, or
worktree, and treat the full local gate as the authority before commit/push.
`zm-tests` runs after the push as a backstop.

## File structure

```
Games/Zenithmon/
  Zenithmon.zproj                # Build descriptor (name, android flag, extras)
  Zenithmon.cpp                  # Project_* entry points, component registration,
                                 #   SaveData init, between-tests hook
  Components/                    # ECS-facing game components
  Source/Battle/                 # Headless deterministic battle engine
  Source/Data/                   # Compiled const gameplay tables + pure formulas
  Source/World/                  # Scene placement + terrain/encounter authoring --
                                 #   ZM_DawnmerePlacement.{h,cpp},
                                 #   ZM_PlayerHomePlacement.h, ZM_ProfLabPlacement.h
                                 #   (the compiled anchors each scene is authored from)
                                 #   ZM_HumanBody.h -- THE body contract: how big a
                                 #     person is (1.8 m tall, 0.8 m footprint, a
                                 #     0.4/0.5 capsule), installed explicitly and
                                 #     NEVER derived from transform scale (ZM-D-181)
                                 #   ZM_HumanAssetPolicy.{h,cpp} -- the injectable
                                 #     "is the human bake loadable right now?" seam;
                                 #     how a test forces the cold-start fallback
                                 #     without touching shared baked files
  Source/ZM_Bindings.h           # THE ACTION TABLE (input program C2) -- the only
                                 #   production file allowed to spell a raw key, pad
                                 #   button or virtual-source id; also the game's
                                 #   NON-CONSUMING action readers
  Source/UI/, Source/Save/, ...  # + Core, Gen, Graph, Interaction, Nav, Party,
                                 #   Shop, CareCenter -- by-value, non-ECS logic
  Tests/ZM_Tests_*.cpp           # Boot, data, stats, battle, and integrity units
  Tests/ZM_AutoTests_*.cpp       # Harness-managed automated/windowed tests
  Assets/Scenes/                 # BOOT-AUTHORED (see below) and COMMITTED --
                                 #   all FIVE scenes are tracked (ZM-D-148/174)
  Assets/Navmesh/                # Dawnmere.znavmesh -- COMMITTED, CI-loadable
  Docs/                          # Cross-session knowledge base --
                                 #   Status (live pins + current task), Roadmap (the
                                 #   S0-S12 plan, keyed to the board), Board (how the
                                 #   docs and the board relate), History (the S0-S7
                                 #   narrative, moved out of Status 2026-08-18),
                                 #   GDD, DecisionLog, Questions, Shortfalls, ...
  CLAUDE.md                      # This file
```

## Build & run

New games are managed by the `zenith` CLI (no Sharpmake edits):

```
zenith build Zenithmon          # Vulkan_vs2022_Debug_Win64_True
zenith run   Zenithmon          # launch the newest built exe
zenith open  Zenithmon          # regen + open the solution in Visual Studio
```

Or directly:

```
msbuild Games\Zenithmon\zenithmon_win64.sln /t:Zenithmon /p:Configuration=Vulkan_vs2022_Debug_Win64_True /p:Platform=x64
Games\Zenithmon\Build\output\win64\vulkan_vs2022_debug_win64_true\zenithmon.exe
```

## Scene authoring + committed assets (IMPORTANT)

**EIGHT assets ARE committed** (verify with
`git ls-files Games/Zenithmon/Assets`): `Assets/Navmesh/Dawnmere.znavmesh`
(ZM-D-147) and all **seven** `Assets/Scenes/*.zscen` -- `Battle`, `Dawnmere`,
`FrontEnd`, `PlayerHome`, `ProfLab` (the fifth added by ZM-D-174), and
`Route1` + `Thornacre` (added by R1-2 step 2, ZM-D-199) (ZM-D-148).
`.gitignore` re-includes
`**/*.zscen` and `**/*.znavmesh` at any depth, which is what lets CI verify
navigation and scene content with no GPU and no bake. **On a normal clone you
need no bake step: the committed bytes are what the game loads**, in `_True`,
`_False` and Android builds alike.

Scenes are AUTHORED by `Project_RegisterEditorAutomationSteps` -- a
**tools-only** function, compiled out of non-tools builds -- and saved via
`AddStep_SaveScene`. So if you ever need to REGENERATE them (a deliberate
re-author, not routine work):

* Only a **`*_True` config** (e.g. `Vulkan_vs2022_Debug_Win64_True`) can author
  a scene at all. A `_False` boot can only load.
* The navmesh and `Dawnmere.zscen` additionally need a **windowed** boot, and
  Dawnmere needs every terrain recipe already warm -- so on a fresh clone it
  takes **two** `_True` boots to regenerate them (boot 1 bakes terrain, boot 2
  authors Dawnmere).
* Dawnmere's re-author is gated behind an explicit authoring mode
  (`sceneAuthoring=AUTHOR_DAWNMERE`); a `DEFERRED` boot silently authors nothing
  and still looks successful.

Scene bytes are boot-shape-independent (dense authoring-order file indices), so a
boot must NOT leave a scene modified in `git status`. If one ever does, that is a
regression of that property -- investigate it rather than just re-committing.

That invariant has been lost **twice**, both times on `Npc_RivalVesper`'s rotation
and both times with every existing guard green. Read both before touching it:

* **ZM-D-179** (2026-08-01) -- `Zenith_TransformComponent` serialized the LIVE JOLT
  BODY's pose, so any authored entity with a body saved a rotation that depended on
  physics state. Serialization now emits the transform's own cached pose unless the
  body genuinely moved, and a tools-only guard bit-compares the rival's serialized
  rotation with `ZM_DawnmereVesperFacing()` immediately before the save.
* **ZM-D-183** (2026-08-04) -- the AUTHORED VALUE ITSELF was build-configuration
  dependent. `ZM_DawnmereVesperFacing()` was a runtime `std::atan2` fed to
  `glm::angleAxis`, and MSVC Debug and Release codegen disagree on those by 1-2 ULP,
  so a **Release** tools boot and a **Debug** tools boot wrote different bytes and
  the file ping-ponged in git forever. The facing is now four FROZEN `std::bit_cast`
  constants, authored via `AddStep_SetTransformRotationQuat` (verbatim, no math).

> **★ IF THIS BREAKS A THIRD TIME, DO NOT ASSUME IT IS THE SERIALIZER.** ZM-D-183
> presented identically to ZM-D-179 and had an unrelated cause. The diff tells you
> which: `WriteToDataStream` takes position and rotation from the SAME source, so a
> falling/moved body CANNOT produce a drifted rotation beside a bit-identical
> position. Rotation-only drift means the authored value moved, not the body.
>
> **★ AND DO NOT TRUST THE PRE-SAVE GUARD TO CATCH IT.** It compares the serialized
> bytes against `ZM_DawnmereVesperFacing()` evaluated in the SAME BINARY, so when the
> computation itself moves, both sides move together and it logs a clean
> `authored == serialised == liveBody` while writing bytes that differ from git. The
> headless guard on the COMMITTED bytes is
> `Tests/ZM_Tests_CommittedSceneBytes.cpp` -- that is the one that can actually see
> it, because CI runs `Null_..._True` and never performs the windowed authoring boot
> the pre-save guard lives on.
>
> **★ RULE:** any authored entity whose rotation lands in a COMMITTED scene file uses
> `AddStep_SetTransformRotationQuat` with a frozen constant. `AddStep_SetTransformYaw`
> / `...RotationEuler` build their quaternion with libm at authoring time and are only
> safe for transient or gitignored scenes. (Identity is exact in every config, which
> is why the four shipped townsfolk never surfaced this.)

See `Docs/DecisionLog.md` ZM-D-179 and ZM-D-183 / `Docs/Questions.md`
Q-2026-08-01-002.

## Controls (the action table)

Every control is an ACTION registered in **`Source/ZM_Bindings.h`** — the one
production file in this game allowed to spell a raw key, pad button or
virtual-source id (input program C2). **Three profiles, ONE SCHEME EACH**:
`P_KEYBOARD {KEYBOARD}`, `P_TOUCH {TOUCH}` and `P_GAMEPAD {GAMEPAD}`, and the
active one switches automatically on the first input from another device.
Windows boots into `P_KEYBOARD`, Android into `P_TOUCH`.

| Action (registered name) | Keyboard | Touch (on-screen) | Gamepad |
|---|---|---|---|
| MOVE `"Move"` | W/A/S/D + arrows | virtual stick | Left stick + d-pad |
| RUN `"Run"` | Shift (either) ★ | virtual button | B ★ |
| INTERACT `"Interact"` | E | virtual button | X |
| CONFIRM `"Confirm"` | Enter / Space | virtual button | A |
| CANCEL `"Cancel"` | Escape / Backspace | virtual button | B ★ + **SYSTEM_BACK** † |
| MENU `"Menu"` | M / Tab | virtual button | Start |
| MENU_UP `"MenuUp"` | W / Up ‡ | — (tap the entry) ‡ | d-pad Up |
| MENU_DOWN `"MenuDown"` | S / Down ‡ | — (tap the entry) ‡ | d-pad Down |

The registered NAME strings matter: they are what an on-screen control's
`SetAction` takes and what a Behaviour Graph node would name, so they are
contract, not decoration.

**MOUSE deliberately belongs to no profile.** This game has no mouse-sourced
action; menu taps ride the pointer table
(`Zenith_UICanvas::NotifyPointerActivate`), which is not scheme-gated, so
clicking a button on desktop works without a MOUSE column existing.

† **CANCEL carries the platform Back gesture, and that row is MASK-EXEMPT.**
Android's system Back must close a menu whatever profile is active, and it is
excluded from activity detection so it cannot itself change one. It is also what
makes `Zenith_Android_Main` CONSUME Back for this game (a game with no
SYSTEM_BACK row leaves the platform's own behaviour alone) — see
`Zenith/Android/CLAUDE.md`.

★ **Pad B is bound twice, and Shift/RUN is a held read while CANCEL is an edge
read.** `ReadRunHeld` asks `IsHeld`; `ReadCancelPressed` asks
`WasPressedThisFrame`. INTERACT is deliberately NOT aliased onto CONFIRM's face
(X vs A): the two are live in different contexts on touch, but on a pad both
exist at once and aliasing them would open a menu every time the player talked
to somebody.

‡ MENU_UP / MENU_DOWN are the battle menu's vertical cursor. Same keys as
forward/back movement by design, but as their OWN actions so a rebind of walking
cannot silently rebind the battle cursor. No TOUCH row — a touch player taps the
entry directly.

Menu navigation (arrows or the d-pad to move focus, Enter/Space or pad A to
activate, tap/click to press a button) is the ENGINE-reserved UI action set
(ids 0-15), which every game gets without registering anything — which is also
why the pad faces and d-pad above are shared with it.

The four on-screen controls live on the persistent `ZM_TouchRoot` entity's UI
component (stick bottom-left, A/B bottom-right, MENU top-right, all authored
HIDDEN); `ZM_TouchLayoutController` shows the ones the current context wants, at
frame-contract step 9. Each publishes through **one virtual source per ACTION,
never per widget**.

## Testing

```
pwsh -File Tools\zenith.ps1 test Zenithmon --headless    # full batch
pwsh -File Tools\zenith.ps1 test Zenithmon --filter ZM_Boot_Test
```

Test DISCOVERY and every headless run use the **Null** exe (`zenith build
Zenithmon --headless` first -- headless is a build config, not a flag).
Unit tests (`Tests/ZM_Tests_*.cpp`, `ZENITH_TEST`) run at every boot before the
scene loads; automated tests (`Tests/ZM_AutoTests_*.cpp`) run via the harness.
Conventions (state-setters only, between-tests hook, RequestSkip when baked
assets are absent) are documented in `Docs/TestPlan.md`. CI gate:
`.github/workflows/zm-tests.yml` (required check `zm-tests`).

## The agent board (project `ZM`)

Zenithmon's WORK ITEMS live on a Jira board, worked by an autonomous
Claude Code loop. **`Docs/Board.md` is the full mapping**; this section is
what bites if you do not know the board exists.

**Zenithmon has its OWN board project — `ZM`.** It used to be a category
inside `ZEN`; it is a project now, because epics, sprints, releases and a
burndown are things a project has and a component cannot. The engine
keeps `ZEN` and DevilsPlayground has `DP`. All three are served by this
one checkout, so the loop still runs exactly one ticket at a time.

**What moved, and what did not.** `Docs/Roadmap.md` and `Docs/Status.md`
remain the SPEC — the roadmap carries each stage's epic key and each
item's issue key, and where the two disagree the checkbox wins, scored
against its literal text (ZM-D-162). What the board carries that a
document cannot is the DEPENDENCIES: the R1-x chain used to be a `deps`
column in a Markdown table, and nothing stopped the loop claiming R1-6
before R1-5 existed. Those are `BLOCKS` links now, and the claim query
refuses a ticket whose predecessor is unfinished.

```
zagent queue   --project ZM      # what the loop would take, and why not
zagent blocked --project ZM      # the whole dependency graph
zagent epic    ZM-9              # S8 and its children
zagent board status --project ZM # roadmap <-> board drift; exit 1 on drift
```

**1. The unit-gate baseline has FOUR pinned sites.** Adding or removing a
`ZM_*` unit means bumping the number in all of them, in one commit:

| Site | What |
|---|---|
| `Games/Zenithmon/Docs/Status.md` | the LIVE PIN block — the authority |
| `.github/workflows/zm-tests.yml` | `-Baseline` on the required check |
| `zagent.project.json` | the loop's Zenithmon gate line |
| `Tools/run_unit_gate.ps1` | the `-Baseline` default — the ENGINE number only |

The engine pin (Null Combat, currently 1638) is spelled in
`zagent.project.json` too, under the `Engine` category. **All four are in
THIS repo**, which is the point — the gate line used to live in a
gitignored file in the board's repo, where a reviewer could not see it
and it could only be remembered. No gate tells you: `run_unit_gate.ps1`
asserts `ran == Baseline` **exactly**, so a grown suite fails with zero
failing tests.

**2. ZM-D-031 is enforced mechanically now.** The `Zenithmon` and
`Engine` categories carry `branching: "direct"`, so the loop commits
straight to `master` and hard-refuses `git switch -c`, `git worktree` and
`gh pr`. DevilsPlayground overrides it to `"branch"` because its
`Docs/CIPolicy.md` is a squash-merge PR policy. Nothing is pushed —
`push: false` everywhere.

**3. Windowed authoring stays human, by engine law.** A headless run may
CREATE a `.zscen` but never CHANGE one (the publish guard), so any slice
that re-authors a committed scene is assigned to a person on the board
and never enters the queue. R1-2 step 3, R1-3, R1-6 and R1-10 are all in
that class. Handing one to the loop would either no-op or trip the guard.

**4. `Docs/` is MIRRORED into Notion, and stays authoritative here.**
`zagent docs sync` renders this directory into a page tree under
Agent › Documentation › Zenithmon — one page per file, plus an index
page with a child per part for anything past the per-page budget
(`Docs/DecisionLog.md` is 932 KB, twelve parts). It is one-way. Nothing
writes back to these files, and nothing should: `Status.md` is the
baseline authority above, `zagent decide` appends to `DecisionLog.md`
here, and a loop worker has no network — it reads `Docs/AgentBriefing.md`
off disk with the Read tool. **Edit the Markdown; re-run the sync.** A
Notion page a human has edited is reported and skipped, never replaced.
(Every write is verified against the persisted snapshot; a re-write
appends, confirms, then drops the superseded blocks, so a write that
fails to land leaves the old text rather than a blank page.)

You can READ any of it back without a browser —
`zagent docs read Zenithmon/Status`, or
`zagent docs read Zenithmon/DecisionLog --recursive` to get all twelve
parts as one document — which is useful when you want the synced text
and not the file. And "nothing writes back" is now enforced rather than
asked for: `zagent docs write Zenithmon/Status …` is REFUSED, naming
`Games/Zenithmon/Docs/Status.md` as the thing to edit. Forcing it past
that guard is what makes the page stop tracking the file, because every
later sync then reads the moved fingerprint as a human edit and skips
the page — silently, and for good.

A dirty tree here blocks the loop entirely — its precondition check
treats uncommitted changes as fatal rather than something to work around
(it must, since `direct` mode commits in place). Leave `master` clean.

## Where to go next

* `Docs/Roadmap.md` -- what's next, stage by stage (S0-S12).
* `Components/ZM_GameComponent.h` -- lifecycle hooks (`OnStart`, `OnUpdate`,
  `WriteToDataStream`/`ReadFromDataStream`) are concept-detected by the
  component-meta registry; there is no base class.
* `Zenithmon.cpp` -- the `Project_*` contract + the boot-authored scene.
* For richer examples: `Games/Combat` (multi-scene + Behaviour Graphs),
  `Games/DevilsPlayground` (physics + the Docs/ governance model this game
  copies), `Games/RenderTest` (rendering + terrain authoring showcase).

## Android

This game ships an Android build (`"android": true` in `Zenithmon.zproj`), with
its Gradle tree under `Games/Zenithmon/Android/` (`com.zenith.zenithmon`). It is
multi-ABI: `app/build.gradle` applies the shared axis
`Build/zenith_android_abis.gradle`, so `abiFilters` and the `jniLibs` roots come
from the one list that mirrors `ZenithAndroidAbi.All` in
`Build/Sharpmake_Common.cs` — arm64-v8a for devices, x86_64 for the emulator
(the only ABI a dev box with no ARM hardware can execute).

```
msbuild Games\Zenithmon\zenithmon_agde.sln /t:Zenithmon ^
  /p:Configuration=Vulkan_x86_64_vs2022_Debug_Agde_False /p:Platform=Android-x86_64
adb install -r -t <apk>        REM -t is REQUIRED: AGP marks debug APKs testOnly
```

The manifest declares `landscape` as a game-design choice. Orientation is no
longer a technical constraint: the swapchain requests an `IDENTITY` preTransform
so the compositor handles any surface rotation — including landscape on a
portrait-native panel, which is what the emulator (`Medium_Phone`) is.
See `Zenith/Android/CLAUDE.md` for the full bring-up sequence, the two ABI
spellings, and how surface rotation is handled.
