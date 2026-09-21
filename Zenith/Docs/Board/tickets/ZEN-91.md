---
id: "24cfb1a0-ea58-4b9a-9a79-3782f7c4109d"
key: "ZEN-91"
type: "BUG"
title: "Any --automated-test boot re-authors committed scenes on either backend, breaking the stated boot-shape-independence invariant"
category: "Engine"
parent: null
sprint: null
status: "Blocked"
assignee: "agent@zenith.invalid"
priority: "MEDIUM"
complexity: "MODERATE"
risk: "MEDIUM"
storyPoints: 5
position: 91
labels: ["blocked-scope"]
fixVersions: []
blocks: []
updatedAt: "2026-09-21T14:57:00.752Z"
flagged: false
severity: "S2_MAJOR"
reproducibility: null
dueDate: null
environment: null
claimAttempt: null
---
## Goal

**Any `--automated-test` boot re-authors committed scenes, on EITHER backend, on
an otherwise clean tree, with no source change involved.**

MEASURED repeatedly on 2026-09-19 while taking before/after evidence for ZEN-12,
each time starting from a clean `git status`:

| run | config | left modified |
|---|---|---|
| `devilsplayground.exe --automated-test Test_MaterialEditorLivePreview` | `Vulkan_*_True` | `Games/DevilsPlayground/Assets/Scenes/ProcLevel.zscen` |
| `zenithmon.exe --automated-test ZM_AnimEditorShowcase` | `Vulkan_*_True` | `Games/Zenithmon/Assets/Scenes/Dawnmere.zscen`, plus an untracked `Games/Zenithmon/Assets/Authored/Creatures/Fernfawn/` |
| `rendertest.exe --automated-test RT_RenderGraphViewStructure` | **`Null_*_True`, headless** | `Games/RenderTest/Assets/Scenes/RenderTest.zscen` |

The first two happened identically BOTH before and after ZEN-12's source change,
which is what establishes this as drift from the shipped binaries rather than
from any diff.

Two things follow.

**1. `Games/Zenithmon/CLAUDE.md` states an invariant that is currently false on
`master`**: *"Scene bytes are boot-shape-independent … so a boot must NOT leave a
scene modified in `git status`. If one ever does, that is a regression of that
property -- investigate it rather than just re-committing."* Three scenes across
three games do exactly that today.

**2. The tick protocol's warning is scoped to the wrong thing.** It reads "A
WINDOWED TEST RUN IS AUTHORING, BEFORE IT TESTS". ZEN-6 removed the headless
`.zscen` publish guard, so a `Null_` boot authors too — demonstrated by the
RenderTest row above. Any tick that runs an automated test for evidence and then
stages with `git add -A` sweeps unrelated committed scenes into its own commit.
Nothing mechanical catches this: `zagent guard` sees the files only while they
are still dirty, and the excursion check reports files that reach the commit
without distinguishing ticket work from capture drift.

This ticket is the investigation that `Games/Zenithmon/CLAUDE.md` asks for.

## Definition of Done

- [ ] The cause of each of the three scenes' drift is identified and stated —
      whether it is one cause or three
- [ ] A boot that loads and runs an automated test leaves every committed
      `.zscen` byte-identical, or each survivor is recorded with the reason its
      re-authoring is legitimate and the invariant's wording is corrected to match
- [ ] The `Authored/Creatures/Fernfawn/` files a showcase run creates are either
      not created, or created somewhere gitignored, or documented as intended
      output
- [ ] Proven by booting each of the three automated tests twice from a clean
      tree and showing `git status` empty after both
- [ ] Every affected game's pinned baseline bumped from an OBSERVED run, if the
      change adds or removes a unit

## Comments

ran on sonnet/medium via MODERATE+MEDIUM · category Engine

**Blocked (`blocked-scope`)** — committed to `master` as `d27774fc`, gates green
(verify: 20 lines, exit 0, 0 skipped, 0 flakes), guard clean. The investigation
this ticket was filed to do is **complete**; it is parked because one of its
three scenes needs a separate fix and two DoD boxes cannot be ticked honestly
until someone rules on whether that fix belongs here. Questions row filed.

**The ticket's stated cause is wrong.** Boots do NOT break scene-byte
boot-shape-independence: a `Null_` and a `Vulkan_` boot of DevilsPlayground
authored byte-identical output. The three example scenes have **three different
answers**:

- `RenderTest.zscen` — **does not reproduce.** The boot re-authors it and
  reports `IDENTICAL … write skipped`. Run by hand; per `ZEN-D-003` no gate runs
  that test.
- `ProcLevel.zscen` — **stale artifact.** 8 bytes, decoding as
  `Zenith_SunComponent::m_xDirection`, which initialises from
  `Zenith_GetDefaultSunDirection()` — changed by `d5d01253`, while the scene's
  last publish (`7e7e3037`) predates it, and DevilsPlayground never calls
  `AddStep_SetSunDirection`. **Re-published**, proven identical across
  `Null_*_Debug`, `Vulkan_*_Debug` and `Vulkan_*_Release`.
- `Dawnmere.zscen` — **a live ZM-D-183 recurrence.** Three mutually distinct
  states: committed, Debug (15 bytes off, ×4 identical), Release (6 bytes off);
  Debug vs Release 9 bytes. ~2 ULP, same file and magnitude as ZM-D-183 but not
  Vesper's facing. **Deliberately not re-published** — filed as **ZM-118**.

**The review changed the outcome.** Four identical Debug boots had this
diagnosed as "stale" and re-published. The reviewer challenged it because no
cross-configuration boot had been run; one Release boot produced a third state
and the re-publish was reverted. Without that, this tick would have committed a
scene the next Release boot dirties again — hiding the defect behind a clean
tree.

Also fixed: `ZM_AnimEditorShowcase` no longer leaves Fernfawn clips in the
tracked `Assets/Authored/` tree (`.gitignore` deliberately re-includes it, so
`git add -A` was sweeping them into unrelated commits). It now promotes into
gitignored `Build/artifacts/` through the existing
`SetAuthoredRootOverride` seam, with a `Teardown`. Verified by boot.

`Games/Zenithmon/CLAUDE.md`'s invariant now names **three** failure shapes with
ZM-D-183 under cross-configuration, where its own postmortem puts it — the first
draft filed it under same-config nondeterminism and contradicted the file's own
surviving description. `doc_lint` cannot catch that; the review did.

**Unmet:** DoD 4 (two of three tests leave `git status` clean; Zenithmon does
not, by design) and DoD 2's first branch. Both trace to Dawnmere alone.

Durable finding: **`ZEN-D-030`**. Follow-ups: **ZM-118** (freeze the
config-dependent value), plus a Shortfalls row for a pre-existing intermittent
SIGSEGV in DevilsPlayground's late shutdown, found by this ticket's own proof
run (test PASSED, tree clean, dies after task-system shutdown).

<!-- agent:worklog:start ZEN-91 -->
## Work Log — 2026-09-21

ran on sonnet/medium via MODERATE+MEDIUM · category Engine

**Outcome**: committed to `master` as `d27774fc`. **Parked Blocked with
`blocked-scope`** — the investigation is complete and two of three scenes are
resolved, but the third needs its own fix (ZM-118) and DoD items 2 and 4 cannot
be ticked honestly until someone rules on whether that fix belongs inside this
ticket. Questions row filed. · ran on sonnet/medium via MODERATE+MEDIUM

---

**★ THE TICKET'S STATED CAUSE IS WRONG, AND THE THREE SCENES HAVE THREE
DIFFERENT ANSWERS.**

The Goal asserts boots are *"breaking the stated boot-shape-independence
invariant"*. They are not. A `Null_` boot and a `Vulkan_` boot of
DevilsPlayground authored **byte-identical** output, and every repeat within a
configuration agreed. The invariant's first clause is true and is now measured.

| Ticket's row | Re-measured result |
|---|---|
| `RT_RenderGraphViewStructure` (Null) | **Does not reproduce.** `[ScenePublish] IDENTICAL … re-authored the committed bytes exactly (93 entities/431263 bytes); write skipped`. Ran by hand — per `ZEN-D-003` no gate runs it. |
| `Test_MaterialEditorLivePreview` | **Reproduces on Null AND Vulkan**, byte-identical to each other. **Shape 2 — stale artifact.** |
| `ZM_AnimEditorShowcase` | **Reproduces (Vulkan only).** **Shape 3 — configuration-dependent.** Not authored at all headless. |

**ProcLevel — cause pinned to a single line of engine history.** Exactly 8 bytes
differ, decoding as three floats `(-0.40,-0.70,-0.55)` → `(-0.55,-0.50,-0.67)`.
Those are `Zenith_SunComponent::m_xDirection`, initialised from
`Zenith_GetDefaultSunDirection()` (`Zenith_EnvironmentAuthority.h:31`). That
function returns the second triple today and returned the first before commit
`d5d01253` (3 Sep 2026); `ProcLevel.zscen` was last committed in `7e7e3037`,
which predates it. DevilsPlayground authors the sun with
`AddStep_SetSunTimeOfDay(270, 35)` and never `AddStep_SetSunDirection` — which
**exists** (`Zenith_EditorAutomation.h:937`) and is used by Zenithmon and
Foundry — so the field kept whatever default was compiled in and was serialized.
Re-published.

**Dawnmere — a live ZM-D-183 recurrence, and I nearly shipped the wrong fix.**

| state | differs from committed |
|---|---|
| committed | — |
| `Vulkan_*_Debug_True` (×4, byte-identical) | 15 bytes |
| `Vulkan_*_Release_True` | 6 bytes |

Debug and Release differ from **each other** by 9 bytes. ~2 ULP, five groups of
three identical floats. Same file and same magnitude as ZM-D-183 (MSVC
Debug/Release codegen on `atan2`/`angleAxis`) but **not** Vesper's facing, which
is already frozen and is a quaternion where these are vec3s. **Reverted the
re-publish**; filed **ZM-118** with the byte offsets.

**Files**

```
 Games/DevilsPlayground/Assets/Scenes/ProcLevel.zscen   (binary, re-published)
 Games/Zenithmon/CLAUDE.md                              (invariant rewritten)
 Games/Zenithmon/Tests/ZM_AnimEditorShowcase.cpp        (Fernfawn leak fixed)
```

**Gates** — verify phase, 20 lines, **exit 0, 0 skipped, 0 deferred, 0 flakes**.
`zagent gates` unioned Zenithmon and DevilsPlayground into the Engine list
because the diff reaches both. Pins observed unchanged: **Combat 2718**,
**Zenithmon 4571**. `zagent guard`: `ok`, selection current, verdict fresh.

**The DoD 4 proof, run in full**

| Test | boot 1 | boot 2 | `git status` |
|---|---|---|---|
| `RT_RenderGraphViewStructure` | 0 | 0 | **empty both** |
| `Test_MaterialEditorLivePreview` | **139 (SIGSEGV)** | 0 | **empty both** |
| `ZM_AnimEditorShowcase` | 0 | 0 | **Dawnmere modified both** |

The DP exit 139 is a **pre-existing intermittent crash in late shutdown**: the
test PASSED on both boots (181 frames), the scene published IDENTICAL, and the
tree stayed clean. Boot 1's last line is `[TaskSystem] Task system shutdown
complete`; boot 2 continues to the leak report and `Shutdown complete`, so it
dies after all work is done. Four other windowed DP boots this tick exited 0. No
DevilsPlayground code is in this diff. Shortfalls row filed.

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `Games/Zenithmon/Tests/ZM_AnimEditorShowcase.cpp` | **No gate.** It is `requiresGraphics`, so the headless batch skips it. Verified by hand: after the change `Games/Zenithmon/Assets/Authored` does not exist and both clips land in gitignored `Build/artifacts/zenithmon/animeditor/AuthoredScratch/`. Compiled by `zenith build Zenithmon` in the gate list. |
| `Games/Zenithmon/CLAUDE.md` | `doc_lint.ps1 -Game Zenithmon` runs it and passes — but its checks are structural (counts, ids, links, markers), so **nothing mechanically checks whether the taxonomy is correct**. That is exactly the defect the review caught. |
| `Games/DevilsPlayground/Assets/Scenes/ProcLevel.zscen` | **No gate reads scene bytes.** Covered by the double-boot proof plus a third configuration: `Null_*_Debug`, `Vulkan_*_Debug`, `Vulkan_*_Release` all authored identical output and a boot now reports `IDENTICAL … write skipped`. |

**Definition of Done**

- [x] The cause of each of the three scenes' drift is identified and stated —
      whether it is one cause or three. **Three different answers**: not
      reproducing / shape 2 / shape 3, and none of them is the cause the ticket
      states.
- [ ] A boot leaves every committed `.zscen` byte-identical, **or** each
      survivor is recorded with the reason its re-authoring is legitimate and
      the invariant's wording is corrected to match. **First branch fails on
      Dawnmere.** The second is met in mechanism — the survivor is recorded with
      its reason and the wording is corrected — but not in wording: Dawnmere's
      re-authoring is a *defect*, not *legitimate*. Left unticked rather than
      argued into a tick; see the Questions row.
- [x] The `Authored/Creatures/Fernfawn/` files are either not created, or
      created somewhere gitignored, or documented as intended output. **Created
      in gitignored `Build/artifacts/`**, via the existing
      `Zenith_AnimationDocument::SetAuthoredRootOverride` seam the engine's own
      animation tests already use, with a `Teardown` clearing it. Verified by
      boot.
- [ ] Proven by booting each of the three automated tests twice from a clean
      tree and showing `git status` empty after both. **Two of three.** Zenithmon
      leaves `Dawnmere.zscen` modified on both boots, by design, until ZM-118.
- [x] Every affected game's pinned baseline bumped from an OBSERVED run, if the
      change adds or removes a unit. Condition does not hold — no unit added or
      removed; observed runs report Combat 2718 and Zenithmon 4571, both
      unchanged. `Tools/unit_baselines.json` untouched.

**Durable findings**

- `ZEN-D-030` — a drifting scene has THREE shapes, not two; shapes 2 and 3 are
  indistinguishable from inside one configuration, so only a cross-config boot
  separates "stale artifact" (re-publish) from "config-dependent" (freeze).
  Also: an absent `[ScenePublish]` line means the scene was never a candidate,
  which is not the same as a passing one.

**Deviations / follow-ups**

- **The review changed the outcome of this ticket, and that is the headline.**
  It challenged re-publishing Dawnmere on the grounds that no cross-configuration
  boot had been run. One Release boot produced a third distinct state and the
  re-publish was reverted. Without that challenge this tick would have committed
  a scene that the next Release boot dirties again — laundering a live defect
  behind a clean tree, which is precisely what ZM-D-183 describes.
- **A blocking-quality finding in text I wrote.** The first version of the
  invariant rewrite created a two-shape taxonomy and filed **both** ZM-D-179 and
  ZM-D-183 under shape 1 (same-config nondeterminism). ZM-D-183 was
  cross-**configuration**, its own postmortem says so, and the file's unchanged
  line 153 still said so — so the doc contradicted itself, and a future reader
  following the new summary would have reached "stale artifact, re-publish" for
  a config-dependent bug. `doc_lint` cannot see this: its checks are structural.
  Corrected to three shapes.
- **Scope deliberately not widened.** The worker reported that
  `Games/Zenithmon/Docs/{AgentBriefing,AssetManifest,DecisionLog,Glossary,History,Questions}.md`
  also carry the older wording. Left alone; the ticket's "what to change" named
  `CLAUDE.md`, and the corrected taxonomy lives there.
- **`.claude/commands/tick.md` is a protected path.** The ticket's point 2 is
  about its "A WINDOWED TEST RUN IS AUTHORING" warning being scoped to the wrong
  thing. Measured: correct, a `Null_` boot authors too. The loop cannot edit that
  file; recorded here and in `ZEN-D-030` instead.
- **Corrections made to the ticket's own text before inlining it (I3):** the
  Goal's headline diagnosis was reproduced and found false, so the measurements
  were inlined as the specification and the worker was told to prefer them.
<!-- agent:worklog:end -->
