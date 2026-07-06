# Test Game

The engine's gameplay sandbox: a minimal two-scene game used to exercise core
engine features end-to-end — physics toys driven by Behaviour Graphs, an
editor-attachable player controller with a graph-bound shoot action, and the
graph-authoring automation itself. It is the smallest live example of the
"systems = C++ components, logic = graphs" doctrine.

## Files

```
Test.cpp                              # Project_* hooks: component + graph-node registration,
                                      #   boot authoring (graphs + MainMenu/Test scenes)
Components/
  Test_PlayerControllerComponent.{h,cpp} # Editor-attachable player: WASD/fly-cam movement,
                                      #   camera orbit, HUD (health/compass/inventory).
                                      #   E-press fires the "Shoot" custom event into the
                                      #   entity's GraphComponent; Shoot() (bullet ring-slot
                                      #   pooling + prefab apply + launch impulse) stays C++
                                      #   as the systems body the graph calls back into.
  Test_GraphNodes.h                   # Node library (see table below)
Tests/
  Test_ShootCharacterization.cpp      # Shoot-action characterization test
Assets/
  Scenes/                             # MainMenu.zscen (index 0), Test.zscen (index 1) —
                                      #   boot-authored via editor automation.
                                      #   terrain_test.zscen is orphaned: on disk but
                                      #   unreferenced by code or build indices.
  Graphs/                             # Boot-authored Behaviour Graphs (regenerated every
                                      #   tools boot): Test_Spinner / Test_Spring /
                                      #   Test_PlayerActions .bgraph
  Prefabs/                            # Bullet prefab (NOTE: the committed Bullet.zprfb is a
                                      #   stale v1 the current loader rejects; the shoot test
                                      #   builds its own fixture prefab at runtime)
```

## Behaviour Graphs

Authored every tools boot in `Project_RegisterEditorAutomationSteps` through
`Zenith_EditorAutomation` graph steps (runtime docs in
`Zenith/Scripting/CLAUDE.md`):

W1 conversion: graphs are authored through `AddStep_GraphBuild` (programmatic
`Zenith_GraphBuilder` functions `BuildGraph_*` in Test.cpp — the builder's
first client); the physics-toy mega-nodes were decomposed into engine nodes
and deleted.

| Graph | Attached to | Logic |
|---|---|---|
| `Test_Spinner.bgraph` | "Spinner" entity (authored, `AddStep_AttachGraph`) | OnUpdate → `SetAngularVelocity`(0,2,0) → `SetVelocity`(zero, all axes — the anchor) |
| `Test_Spring.bgraph` | "Spring" entity (authored) | declared var `springTarget`=(0,5,0); OnUpdate → `ReadEntityPosition`→springPos → `MathBlackboardVector3`(sub: target−pos)→springForce → `ApplyForce`(springForce) |
| `Test_PlayerActions.bgraph` | the player entity (whoever attaches `TestPlayerController` also attaches this) | OnCustomEvent("Shoot") → `TestSpawnProjectile` (executes the component's `Shoot()` systems body) |

### Node library (`Components/Test_GraphNodes.h`)

| Node | What it does |
|---|---|
| `TestSpawnProjectile` | Calls `Test_PlayerControllerComponent::Shoot()` on the context entity (the systems seam — pooling/prefab/impulse stay C++) |

Registered via `Test_RegisterGraphNodes()` from
`Project_RegisterGameComponents`.

## Tests

`Tests/Test_PhysicsToysCharacterization.cpp` — `Test_PhysicsToys_Test` loads
the Test scene and pins the toys' observable physics (Spinner angular velocity
(0,2,0) + zero linear drift; Spring X pulled toward the target — the X axis
isolates the spring force from gravity). Written against the C++ mega-nodes
first; green unchanged against the decomposed engine-node graphs.

`Tests/Test_ShootCharacterization.cpp` — `Test_ShootAction_Test` constructs
the player the way a user authors it (capsule collider + camera +
`TestPlayerController` + the actions graph), builds a fresh bullet fixture
prefab (CreateFromEntity → SaveToFile → registry), presses E through the real
input path, and asserts a "Bullet0" entity exists with the launch velocity
applied. Written against the C++ shoot first; identical pre/post the graph
conversion. Headless-runnable:

```
test.exe --automated-test Test_ShootAction_Test --headless --exit-after-frames 5000 --fixed-dt 0.01666 --skip-unit-tests
```

> The `vulkan_`-prefixed output dir may be missing middleware DLLs after a
> rebuild (0xC0000135) — copy `*.dll` from a sibling game's matching output
> dir (e.g. Combat's).

## Build indices

| Index | Scene | Purpose |
|------:|-------|---------|
| 0 | MainMenu | Boots here (title + Play button placeholder) |
| 1 | Test | Gameplay sandbox: Spinner + Spring physics-toy entities |
