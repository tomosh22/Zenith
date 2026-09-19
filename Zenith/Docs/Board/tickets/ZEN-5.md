---
key: ZEN-5
type: TASK
title: "Zenith_TerrainComponent's //private: is commented out, so the lockstep invariant has no access control behind it"
category: Engine
parent: "ZEN-24"
sprint: null
status: Done
assignee: agent@zenith.invalid
priority: MEDIUM
complexity: MODERATE
risk: MEDIUM
storyPoints: 5
position: 1
labels: []
fixVersions: []
blocks: []
links: []
source:
  id: ge8d8l43pb559wic2uthq4wv
  risk: MEDIUM
  type: TASK
  title: "Zenith_TerrainComponent's //private: is commented out, so the lockstep invariant has no access control behind it"
  number: 5
  flagged: false
  due_date: null
  position: 1
  priority: MEDIUM
  severity: null
  parent_id: null
  sprint_id: null
  status_id: dxhlckk5kgxv4mgp4e8lcu7s
  complexity: MODERATE
  created_at: 2026-08-21T12:08:22.426593
  project_id: r1m7idj9n6rkv5b1iulqz2ho
  resolution: DONE
  updated_at: 2026-08-21T15:10:40.416
  assignee_id: n9xhr52s8n49fa832egoqf4z
  category_id: xmu47blrutzwzsqblzurhc9p
  environment: null
  reporter_id: n9xhr52s8n49fa832egoqf4z
  resolved_at: 2026-08-21T15:10:40.408
  story_points: null
  reproducibility: null
updatedAt: "2026-09-19T18:03:38.981Z"
---
## Goal

`Zenith/EntityComponent/Components/Zenith_TerrainComponent.h:522` reads:

```cpp
//private:
	Zenith_Entity m_xParentEntity;

	Flux_MeshGeometry* m_pxPhysicsGeometry = nullptr;
	// Per-chunk acceleration for TryGetGroundHeightAt ...
	PhysicsChunkSpan* m_pxPhysicsChunkSpans = nullptr;
	uint32_t m_uPhysicsChunkSpanCount = 0u;
	...
```

The access specifier is **commented out**, so every member from there on is `public`. The class already grants `friend` to the specific TUs that legitimately need in — so the friendship is doing nothing while the members are open to everything.

**Why this is worth fixing rather than shrugging at.** ZEN-2 and ZEN-4 built a lockstep invariant on those members: `m_pxPhysicsChunkSpans` must die with `m_pxPhysicsGeometry`, and the header now states that as a simple, unqualified rule. That rule holds only because *"every site in this file is the only writer"* — which nothing enforces while the members are public. ZEN-4 tightened the invariant; this is the access control that would make it hold.

**Found independently by two agents that never saw each other's output**: the ZEN-2 reviewer flagged it as non-blocking finding 6, and the ZEN-4 worker surfaced it unprompted while tracing discard sites. It also makes `FreePhysicsChunkSpans()`'s own doc comment inaccurate, since it describes itself as a "private member of the same class".

Pre-existing, and predates both tickets — it is not a regression from either.

## Definition of Done

- [ ] The specifier is uncommented, or deliberately removed with a recorded reason if something genuinely needs the members public
- [ ] Every resulting compile error is fixed by widening `friend` or adding an accessor, never by re-opening the section wholesale
- [ ] `FreePhysicsChunkSpans()`'s doc comment is accurate about its own access
- [ ] Pins unmoved: this is access control, not behaviour

## Note on scope

Uncommenting one line may surface a long tail of external readers. If it does, **report the count and stop rather than widening access to make it compile** — a friend list that grows to fit every caller is the same problem with extra steps. That outcome is a legitimate result worth its own decision.

<!-- agent:worklog:start ZEN-5 -->
## Work Log — 2026-08-21

**Outcome**: merged to `master` as `fdd61c6c` · ran on sonnet/medium via MODERATE+MEDIUM · category Engine

**What changed**

- `Zenith/EntityComponent/Components/Zenith_TerrainComponent.h` — uncommented the `//private:` specifier and split the block it opens into alternating `private:`/`public:` sub-blocks, each carve-out carrying the reason beside it. 65 insertions, 3 deletions, one file.

**Files** (`git diff --stat`)

```
 Zenith/EntityComponent/Components/Zenith_TerrainComponent.h | 68 +++++++++++++++---
 1 file changed, 65 insertions(+), 3 deletions(-)
```

**Gates**

| Command | Result |
|---|---|
| `pwsh -NoProfile -File Tools\zenith.ps1 build engine` | exit 0, 44s (Vulkan_vs2022_Debug_Win64_True) |
| `pwsh -NoProfile -File Tools\zenith.ps1 build Combat --headless` | exit 0, 20s |
| `pwsh -NoProfile -File Tools\zenith.ps1 test Combat --headless` | exit 0, 20s — 16 passed, 0 failed |
| `pwsh -NoProfile -ExecutionPolicy Bypass -File Tools\run_unit_gate.ps1 -Exe Games\Combat\...\combat.exe -Game Combat -TimeoutSec 600` | exit 0, 38s — **1650 ran == baseline 1650**, 0 failed |

`zagent gates ZEN-5` selected 4 gates, all from category Engine, nothing
unioned in — correct, the diff is entirely inside Engine's own `paths`.

**Extra verification not in the gate list** (see Deviations):

| Command | Result |
|---|---|
| `zenith build Zenithmon --headless` | exit 0, 9s — recompiled 3 TUs incl. `ZM_TerrainAuthoring.cpp` |
| `zenith build CityBuilder --headless` | exit 0, 11s — recompiled 26 TUs incl. `CB_RoadTerrain.cpp` |
| `zenith build RenderTest --headless` | exit 0, 8s — recompiled 4 TUs incl. `TerrainEditorSmoke.cpp`, `RenderTest.cpp` |

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `Zenith_TerrainComponent.h` | **Nothing at runtime, and nothing could.** The diff is access specifiers and comments; it has no executable line. What verifies it is the COMPILER, in every TU that includes the header and touches those members. The gate list covers the engine and editor TUs — `Zenith_TerrainEditor_Bake.cpp` did recompile, confirming the friend reasoning. It does NOT cover the three games that consume the header; those were compiled by hand (above). No unit test executes an access specifier and adding one would be meaningless. |

**Reviewer** (`zagent-reviewer`, sonnet — triggered by risk MEDIUM in `reviewOn`
AND by the DoD asserting an ABSENCE: *"never by re-opening the section
wholesale"*)

- **No blocking findings.** Traced the specifier chain before and after:
  every `public:` added sits on a member that was already public via the
  commented-out specifier (a no-op), every `private:` is a genuine
  tightening, and nothing above the original `//private:` line is touched.
- Confirmed no config-dependent access: no member existing in both
  configs has a specifier inside the `#ifdef ZENITH_TOOLS` block, and the
  unconditional `private:` after `#endif` is untouched.
- Confirmed all four `friend` declarations exist and name the right types,
  and that the `Zenith_TerrainEditor_Bake.cpp` call sites — including one
  nested two lambdas deep — are covered, since a lambda shares the access
  context of its innermost enclosing non-closure scope.
- Independently grepped every newly-private member across all six games:
  **zero real call sites**; every hit was an unrelated component reusing
  the name `m_xParentEntity`, a prose comment, or a same-named function in
  another namespace (`Flux_InstanceCullingUtil::ExtractFrustumPlanes`).
- Non-blocking, flagged for the record: the DoD's "uncommented, or
  deliberately removed" reads as a binary choice at SECTION granularity;
  the worker made it at MEMBER granularity. The reviewer verified each of
  the five public carve-outs is individually accurate rather than a
  disguised wholesale reopen, and the one invariant the ticket actually
  cared about took no exceptions.

**Definition of Done**

- [x] The specifier is uncommented — with five members deliberately left
      public, each carrying its recorded reason in the header
- [x] Every resulting compile error fixed by widening `friend` or adding an
      accessor, never by re-opening wholesale — in the event, **no** friend
      and **no** accessor was needed; the carve-outs were pre-existing
      public members that stayed public
- [x] `FreePhysicsChunkSpans()`'s doc comment is accurate about its own
      access — reviewer traced it against all 7 call sites
- [x] Pins unmoved — Combat gate reports 1650 ran against baseline 1650

**Deviations / follow-ups**

- **Corrections made to this ticket's own text before inlining it (I3).**
  The body cites the specifier at line **522**; it was at **540**. The
  body drifted. Everything else in its claim was verified against the
  working tree before the prompt was written, and the worker was told to
  work from the current file rather than the quoted line numbers.
- **I ran three gates that are not on the list**, by hand. The Engine gate
  list builds `engine` + `Combat`, and **Combat references
  `Zenith_TerrainComponent` in zero source files** — while Zenithmon (12),
  CityBuilder (4) and RenderTest (4) do. So all four gates could have gone
  green with three games left un-compilable. They build clean, but that is
  a fact I established by convention, not by mechanism, and the next
  engine-header ticket has nothing making anyone repeat it. Filed as a
  Suggestions row.
- **Docs mirror skipped**: `changed.txt` holds one path and it matches
  neither `Docs/` nor `CLAUDE.md`.
- Narrowing `m_pxStreamingState` to a real accessor would touch
  `Games/RenderTest/RenderTest.cpp`, `Games/CityBuilder/Source/CB_RoadTerrain.cpp`,
  `Games/RenderTest/Tests/TerrainEditorSmoke.cpp` and
  `Flux/Terrain/Flux_TerrainStreamingManagerImpl.h`'s doc comment — a
  cross-game API change, not an access-control cleanup. Worth its own
  ticket if wanted.
<!-- agent:worklog:end -->
