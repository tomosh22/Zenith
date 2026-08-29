# ScriptTest

A **zero-gameplay-C++ game**: seven scenes of working behaviour — motion, input,
physics, events, state machines and UI — with no game component and no game graph
node behind any of it. It exists to prove the Behaviour Graph runtime
(`Zenith/Scripting/` + `Zenith/EntityComponent/Zenith_GraphOps.*`) is complete
enough to build a game with, and to keep it that way: a node the graph layer is
missing shows up here as a scene that cannot be authored, not as a quiet fallback
into C++.

## THE CLAIM

> **ScriptTest defines no game ECS components and no game graph-node
> implementations; all gameplay behaviour is executed by engine-owned graph
> nodes.**

`ST_NoGameExtensionsContract` enforces that mechanically, by **registry
provenance** rather than by grepping source:

- **Node registry set-equality** — reset the node registry, re-derive it from the
  engine registrar alone, and assert the set is unchanged. A game-registered node
  type would make the two sets differ.
- **Component-meta allowlist** — every registered component name must be an
  engine one.
- **Per-builder node-type membership** — every node type each of the fifteen
  `BuildGraph_ST_*` builders emits must be in the engine-derived registry.

**The constant project accessors are exempt, and that is expected, not tolerated.**
`Project_GetName` / `Project_GetGameAssetsDirectory` return compile-time constants
and are called freely at any time — `Zenith_EditorPanel_ContentBrowser.cpp:78` and
`:923` both call `Project_GetGameAssetsDirectory()` from panel render code, so the
editor calls it **every frame** (`:78` sits in `RenderBreadcrumbs`, which the
toolbar row calls at `:426`). A rule phrased as "the project hooks are never
called after boot" would be false on the first editor frame; a constant returned
per frame is not a decision.

### Two halves the test CANNOT see (convention, enforced by review)

The test file states these itself, at the top of `Tests/ScriptTest_Contracts.cpp`,
so nobody reads a green run as a stronger guarantee than it is:

| Property | Why no test can settle it |
|---|---|
| **(i) Hook purity.** `Project_RegisterGameComponents` creates materials; `Project_InitializeResources` and `Project_RegisterEditorAutomationSteps` author assets, then get out of the way. None may branch on gameplay state or grow a per-frame path. | The test observes **registries**, and a runtime decision inside a hook registers nothing — so it is invisible here. The constant-accessor exemption above is what makes this a judgement rather than a rule. |
| **(ii) Name shadowing — the COMPONENT half only.** Both provenance checks compare names, and a name is not an identity. | `Zenith_ComponentMetaRegistry::RegisterComponent` ends in an unconditional `m_xMetaByName[strTypeName] = xMeta;` (`Zenith/ZenithECS/Zenith_ComponentMeta.h:420`), so a same-name registration **overwrites** the engine row rather than being rejected: a game type registered as `"Transform"` would pass the allowlist having replaced the engine's Transform outright. **The graph-node registry is not vulnerable to this** — it rejects a duplicate by name and logs it, so the node half is safe from the same trick. |

If you find yourself wanting either, the answer is a **new engine node**, not a
game one. Adding a game component or a game node to this project is a redesign of
what it is for, not a fix.

## File map

```
Games/ScriptTest/
  ScriptTest.zproj      # Build descriptor. "android": false -- win64 only.
  ScriptTest_Graphs.h   # THE STRING CONTRACT + the 15 builder declarations
  ScriptTest.cpp        # Project_* hooks, the 15 BuildGraph_ST_* builders,
                        #   the 7 scene recipes, the build-index registration
  Tests/                # The automated tests (see the inventory below)
  Assets/
    Scenes/*.zscen      # COMMITTED (7) -- and re-authored on every tools boot
    Graphs/*.bgraph     # gitignored bake product (15), tools boots only
    Meshes/UnitCube|UnitSphere.{zasset,zmodel}   # gitignored bake product, tools boots only
    Prefabs/ST_Ball.zprfb                        # gitignored bake product, tools boots only
    Textures/ST_*.ztxtr # gitignored, 7 1x1 colour textures -- rewritten by EVERY
                        #   boot (the unconditional material hook), so they can
                        #   never go stale or missing
```

The C++ does exactly three things: create the seven materials the scenes
reference by path (**not** tools-gated — a `_False` boot loads a committed scene
whose material paths must resolve with no authoring pass), generate the two
primitive meshes (tools only), and author the graphs, the ball prefab and the
seven scenes (tools only). The materials are **texture-backed, not
base-colour-tinted**: each one's colour lives in a 1×1 `.ztxtr` the hook writes
(Combat's `ExportColoredTexture` pattern) with the base-colour factor left white,
because the uber-shader multiplies texture × factor and a colour in both places
would square itself.

## The 15 graphs

Attach order per entity **is** the slot order, so the slot column is a contract,
not a description.

| Asset | Builder | Attached to (scene / entity → slot) |
|---|---|---|
| `ST_EscToHub` | `BuildGraph_ST_EscToHub` | every gym's `GameManager` → slot 0 (all six) |
| `ST_HubFlow` | `BuildGraph_ST_HubFlow` | Hub / `GameManager` → 0 |
| `ST_Spin` | `BuildGraph_ST_Spin` | Gym_Motion / `Spinner` |
| `ST_PingPong` | `BuildGraph_ST_PingPong` | Gym_Motion / `PingPong` |
| `ST_SineBob` | `BuildGraph_ST_SineBob` | Gym_Motion / `Bobber` |
| `ST_PlayerMove` | `BuildGraph_ST_PlayerMove` | Gym_Input **and** Gym_Events / `PlayerCube` → 0 |
| `ST_Jump` | `BuildGraph_ST_Jump` | Gym_Input **and** Gym_Events / `PlayerCube` → 1 |
| `ST_BallSpawner` | `BuildGraph_ST_BallSpawner` | Gym_Physics / `Spawner` |
| `ST_KillVolume` | `BuildGraph_ST_KillVolume` | Gym_Physics / `KillVolume` |
| `ST_PressurePlate` | `BuildGraph_ST_PressurePlate` | Gym_Events / `PressurePlate` |
| `ST_Door` | `BuildGraph_ST_Door` | Gym_Events / `Gym_Door` |
| `ST_BellRing` | `BuildGraph_ST_BellRing` | Gym_Events / `GameManager` → 1 |
| `ST_BellListener` | `BuildGraph_ST_BellListener` | Gym_Events / `BellListener_A`, `_B`, `_C` (the same asset, three entities) |
| `ST_TrafficLight` | `BuildGraph_ST_TrafficLight` | Gym_State / `GameManager` → 1 |
| `ST_UIPlayground` | `BuildGraph_ST_UIPlayground` | Gym_UI / `GameManager` → 1 |

Two shapes govern every builder above and are worth knowing before editing one:

- **No exec fan-in.** A node may not have two exec predecessors, so a chain that
  would converge duplicates the node instance per pin instead. `ST_HubFlow`'s
  twelve `LoadSceneByIndex` instances (six buttons + six keys) are the extreme
  case.
- **Every non-`ON_UPDATE` dispatch carries `dt = 0`.** Not only custom events:
  `OnStart`, `OnCollisionEnter`/`Exit` and `OnCustomEvent` (including the
  StateMachine's `TLEnter_*`/`TLExit_*` transitions) all fire with a zero delta —
  `Zenith_GraphComponent::OnStart` is literally
  `FireEventOnSlots(GRAPH_EVENT_ON_START, 0.0f, nullptr)`. So no `Wait`, `Timer`
  or dt-scaled node may appear under **any** of them; `ON_UPDATE` is the only
  source that carries a real delta. Tweens are safe under all of them —
  `Zenith_TweenComponent` self-ticks — which is exactly what lets `ST_PingPong`
  hang an endless tween loop off `OnStart`.
- **★ StateMachine transition events LAG their variable by one dispatch.** A state
  body writes the blackboard on frame N; the `StateMachine` only notices on its
  next fire, so `Exit_<old>` / `Enter_<new>` run on frame N+1. Anything reading a
  lamp (or any other transition effect) on the frame the variable flips will find
  the *old* value — `ST_StateGym_Test` reads after a grace window for exactly this
  reason.

## The seven scenes

The **index** is graph contract: every `LoadSceneByIndex` node names one, and
`Project_LoadInitialScene` registers the same index against the same path.

| # | Scene | What it demonstrates |
|---|---|---|
| 0 | `Hub` | UI navigation — six buttons and six keys, each loading a gym by build index |
| 1 | `Gym_Motion` | Transform motion three ways: `ST_Spin` (a rate), `ST_PingPong` (an endless `Repeat` driving tweens from a fire-once source), `ST_SineBob` (blackboard math into a translate) |
| 2 | `Gym_Input` | The WASD quad → velocity, plus a raycast-gated jump (grounded check before the impulse). **Only the W axis is exercised by a test** (C4 hermetically, C8 in the scene); one axis stands in for four |
| 3 | `Gym_Physics` | A timer-driven prefab spawner (and Space for one on demand) + a static sensor kill volume, with cross-entity UI counters written through a packed `EntityID` in a blackboard var — the *Spawned* readout's text is asserted against the live counter (C10), so that target var is covered end to end |
| 4 | `Gym_Events` | Targeted custom events — pressure plate → `OpenDoor`/`CloseDoor` at an entity it looked up by name — and a broadcast: one `Bell` pulsing all three independent listeners, each of which then settles back |
| 5 | `Gym_State` | A `StateMachine` traffic light (Red → Green → Amber) with enter/exit visual events on the three lamps |
| 6 | `Gym_UI` | Buttons and keys → blackboard → text, fill and colour binding: a formatted clock, a 5 s sawtooth fill bar, a colour that flips past 80%. Coverage is uneven and deliberately stated: **text is asserted** (C12b reads the `Counter` element back), **fill only structurally** (via the `fill01`/`hot` vars its chain feeds), **colour not at all** — both `SetUIColor` nodes are terminal, so nothing but an eye can see them |

Every scene also authors a **`Sun`** entity (time-of-day 55°, late morning) —
without one the environment authority falls back to a near-horizon default and
every prop reads near-black on screen — and the five gyms with geometry add a
`KeyLight` fill. Both are furniture, not behaviour: no graph touches them.

**★ Every scene camera carries a 180° yaw, and it is load-bearing.** Yaw 0 faces
**+Z** in this engine (`Zenith_CameraComponent::GetFacingDir`), and every recipe
here places its camera at *positive* Z looking back at content around the origin.
`ST_AddSceneCamera` applies the half-turn once for all seven; author a new camera
without it and the whole scene sits **behind** the camera — the GPU frustum cull
then (correctly) rejects every object, and the only symptom is a sky-and-UI
screenshot that looks exactly like a renderer bug. A sky-only view verifies
*pitch* (the horizon height) but can never verify *yaw*, which is precisely how
this shipped facing backwards and cost a two-session hunt through an innocent
pipeline before anyone checked which way the camera pointed.

## The string contract — `ScriptTest_Graphs.h`

Zero gameplay C++ makes a whole class of strings load-bearing **and unchecked**:
an entity name a `FindEntityByName` node looks up, a UI element name a `SetUIText`
node addresses, a blackboard variable two nodes share, a custom event one graph
fires and another listens for, a graph asset path, a scene build index. None is
compile-checked; a typo produces a graph that builds cleanly and does nothing.

So each is spelled **exactly once**, in `ScriptTest_Graphs.h`, and shared by the
two things that must agree: the builders and scene recipes in `ScriptTest.cpp`,
and the tests. **A test that restated a literal would prove only that the test
agrees with itself.** Namespaces: `Graphs`, `Scenes`, `Entities`, `UINames`,
`Vars`, `Events`, `Materials`, `Meshes`, `Prefabs`.

`Events::szTL_*` is the sharpest case: `StateMachine` composes its event names as
`"<m_strEventPrefix>Enter_<stateName>"`, so `szTL_PREFIX`, `szTL_STATE_NAMES` and
the six `TLEnter_*`/`TLExit_*` spellings have to agree character for character.

## Iterating on a graph

Two directions, and they are not symmetric:

1. **Edit the builder** — change `BuildGraph_ST_*` in `ScriptTest.cpp`, then boot
   any tools (`*_True`) config. The authoring pass rewrites the `.bgraph`.
2. **Edit live in the graph editor** — open the `.bgraph`, change it, Save. Hot
   reload applies it to live instances immediately, with no reboot. This is the
   fast loop for tuning.

**★ Boot re-authoring OVERWRITES live edits.** The builders are the single source
of truth: every tools boot regenerates all fifteen graphs from scratch. So a
graph-editor experiment is a *sketch* — transcribe what you liked back into the
builder before the next boot, or it is gone.

That is also why the three **contract** tests (C2–C4) are hermetic: each builds
the graph it is about in-process from the same `BuildGraph_ST_*` function, so
there is no `.bgraph` on disk to go stale and no dependency on a prior tools run
having left one behind.

**That scope is the whole claim — the other nine tests DO depend on bake
products.** They load the committed `.zscen`, whose slots reference the fifteen
`.bgraph`, the two generated meshes and the ball prefab, and every one of those
is gitignored and written only by a `*_True` boot's authoring pass. A single
tools process authors them before any test runs, so the suite is self-sufficient
*in that process* — but it is exactly why a `_False` config cannot run the full
suite from a fresh checkout, and why the first run of one must be `*_True`.

## First run, and what is tracked

**The first build + run of a fresh checkout must be a `*_True` (tools) config.**
It bakes the graphs, the two meshes and the ball prefab, all of which are
gitignored. `_False` configs run no automation at all and simply load what is on
disk.

The **scenes are different: all seven `.zscen` are COMMITTED** — and also
re-authored on every tools boot. `st-tests.yml` is what keeps those two facts
consistent: it derives the committed set from `git ls-files` (never a hardcoded
list — a scene missing from one would never be deleted, never differ, and its
recipe would escape the guard forever), deletes them, re-authors them cold from
the recipes, and asserts that exactly those files come back with byte-identical
contents and no strays. A recipe that stops reproducing its committed scene reds
a required check instead of drifting silently. Do not run that deletion locally —
it would throw away in-progress scene work.

**★ EVERY `zenith test ScriptTest` RUN REWRITES ALL SEVEN TRACKED SCENE FILES,
and byte-determinism is the only reason `git status` stays clean afterwards.**
The moment authoring stops being deterministic, an ordinary gate run dirties the
working tree — which also trips the agent loop's dirty-tree precondition and
silently blocks the queue. That is what the CI cold-bake guard exists to catch,
and why it fails on a `git diff` rather than merely on a missing file.

## Build and test

```
zenith build ScriptTest --headless                      # Null_vs2022_Debug_Win64_True (the CI build)
zenith test  ScriptTest --headless                      # the whole suite, one process
zenith test  ScriptTest --headless --tests ST_MotionGym_Test   # one test (comma-separate for an ordered probe)
zenith build ScriptTest --config D3D12_vs2022_Debug_Win64_False # the backend-neutrality link proof
```

Those first, second and fourth lines are the ScriptTest gates in
`zagent.project.json`, and `.github/workflows/st-tests.yml` runs them verbatim.
There is **no unit-gate line and no `Tools/unit_baselines.json` row**: this game
adds no boot units, and `Test_UnitBaselineManifest.ps1` requires every row there
to be gated or declared advisory, so an ungated pin cannot be added silently.

## The manual demo (what a person should see)

```
zenith run ScriptTest --config Vulkan_vs2022_Debug_Win64_True
```

**Always pass `--config`.** A bare `zenith run` launches the *newest* built exe,
which is very often the `Null_` build a gate just produced — and that one creates
its window hidden, so it looks like nothing happened.

1. From the hub, click each of the six buttons and press **Esc** to come back.
   All six round-trips work, and the hub is reachable from every gym.
2. In **Gym_State**, watch the lamps cycle **Red → Green → Amber** on their own.
3. In **Gym_Physics**, press **Space** and watch the *Spawned* and *Killed*
   counters move as balls drop past the platform into the kill volume.
4. Then — the demo this game exists for — open `ST_Spin.bgraph` in the graph
   editor, change `m_fDegreesPerSecond` (it ships at `45.0`), and **Save**. The
   spinner's speed changes **live, without a reboot**.

## Test inventory

Thirteen tests in four files. **Every one is headless-safe** — nothing reads a
pixel, so none sets `m_bRequiresGraphics` and the whole suite runs in the Null
gate rather than being skipped-as-passed there. All are guarded by
`#ifdef ZENITH_INPUT_SIMULATOR`, which is unconditional in `Zenith.h`.

| Test | File (C#) | What it proves |
|---|---|---|
| `ScriptTest_Boot_Test` | `ScriptTest_BootCharacterization.cpp` | The game boots to a valid active scene. Also the cheapest way to drive the boot authoring pass — it is what the CI cold-bake step runs. |
| `ST_NoGameExtensionsContract` | `ScriptTest_Contracts.cpp` (C2) | THE CLAIM, mechanically: node-registry reset/re-derive set-equality, the component-meta allowlist, and per-builder node-type membership. Read its header for the two properties it *cannot* see. |
| `ST_TrafficLightContract` | `ScriptTest_Contracts.cpp` (C3) | The `StateMachine` + `Wait` cadence, read off the **blackboard** rather than off a rendered lamp. |
| `ST_PlayerMoveContract` | `ScriptTest_Contracts.cpp` (C4) | The input → blackboard half of the movement chain, driven through the real device layer. |
| `ST_SceneAssetIntegrity` | `ScriptTest_SceneIntegrity.cpp` (C5) | A byte-substring scan of the seven `.zscen` **files**: every one still names its authored entities and its attached `.bgraph` slot paths, carries `ST_EscToHub` (every gym does; the hub does not), leaks no prefab-scratch entity, and is not a header-only stub. It resolves nothing and looks at no model, material or prefab. Never loads a scene, so it reddens in the configuration that *damaged* the asset — but in a `*_True` build the bytes it reads are the ones **this boot just authored**; only the CI cold-bake step compares them to the committed ones. |
| `ST_AllScenesBoot` | `ScriptTest_SceneIntegrity.cpp` (C6) | Asserts on the seven scenes **loaded**: every build index in order, key entities resolving by name, and every `Zenith_GraphComponent` slot carrying a resolved graph with zero unresolved nodes. |
| `ST_HubNavigation_Test` | `ScriptTest_GymBehaviour.cpp` (C7) | All 6 hub buttons **and** all 6 number keys reach their gym, and Escape returns from every one — 24 scene transitions. |
| `ST_InputGym_Test` | `ScriptTest_GymBehaviour.cpp` (C8) | Held W moves the cube in +Z, releasing stops it, and Space clears the grounded raycast and lifts it. |
| `ST_EventsGym_Test` | `ScriptTest_GymBehaviour.cpp` (C9) | The plate's **targeted** `FireCustomEvent` raises the door and stepping off lowers it; one `Bell` **broadcast** pulses **all three** listeners that nothing names, and **each settles back** — the settle is what proves `WaitForTween`'s suspended anchor resumed, which a peak-only read would miss. |
| `ST_PhysicsGym_Test` | `ScriptTest_GymBehaviour.cpp` (C10) | The spawn/kill loop actually loops: both counters climb, Space spawns on demand (pressed on the frame a **timer** spawn is observed, so the next increment has only one possible author), the *Spawned* HUD text written **cross-entity through the packed-`EntityID` target var** matches the live counter, and the live-ball population stays **bounded** — i.e. balls are really destroyed, not merely counted. |
| `ST_MotionGym_Test` | `ScriptTest_GymBehaviour.cpp` (C11) | All three transform drivers move: `RotateEntity`, the `Repeat`/Tween ping-pong, and the blackboard-maths bob. |
| `ST_StateGym_Test` | `ScriptTest_GymBehaviour.cpp` (C12a) | The `StateMachine` reaches Green on schedule **and** its `TLEnter_`/`TLExit_` chains reached the lamps (Enter_Green scaled `Lamp_Green` to 1.4, Exit_Red put `Lamp_Red` back to 1.0) — the variable alone would pass with the lamps unwired. |
| `ST_UIGym_Test` | `ScriptTest_GymBehaviour.cpp` (C12b) | Two buttons and a key mutate one blackboard int **and the `Counter` element's text is read back** (`SetUIText` is chain-terminal, so a typo'd element name fails silently otherwise); the clock/modulo/compare chain crosses the hot boundary in both directions. **Fill** is covered only structurally — `fill01` is asserted in `(0, 1]` and the `hot` flag downstream of it flips, so a broken chain aborts before either — and **colour is not asserted at all**: both `SetUIColor` nodes are terminal and visual-only. |

**C5 and C6 are deliberately not redundant.** A slot whose `.bgraph` cannot be
loaded keeps its path and override bytes verbatim (the unresolved-slot contract in
`Zenith/EntityComponent/CLAUDE.md`), so the path survives a round trip while the
graph itself is null — C5 stays green on a scene whose every graph is dead.
Conversely, a scene the boot re-authored one entity short loads perfectly well;
only the committed bytes remember what was meant to be there.

The behavioural seven exist because nothing in C5/C6 can see **a graph that builds
perfectly and then does nothing** — a `Timer` that never fires, a sensor that never
reports, a state machine whose transitions reach no lamp. They drive each gym
through the seams a player would. The harness pins `dt` at 1/60 across reset,
Setup and every Step, so their frame counts are 60 Hz and they do not call
`SetFixedDt`/`ClearFixedDt` themselves.

## Android

Win64 only (`"android": false`). To add an Android build: copy an existing game's
`Android/` Gradle tree (e.g. `Games/Combat/Android`), retarget its package/name,
set `"android": true` in the descriptor, and run `zenith regen`.
