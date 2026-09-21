---
id: "e0c2f5a9-499e-4c75-bae8-ed2632e960de"
key: "ZEN-92"
type: "TASK"
title: "Two of three capture harnesses cannot hold a BEFORE and an AFTER set at once"
category: "Engine"
parent: null
sprint: null
status: "Done"
assignee: "agent@zenith.invalid"
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 2
position: 92
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-21T15:54:56.823Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: null
claimAttempt: null
---
## Goal

Of the three capture harnesses a single ticket's visual evidence can depend on,
only one can hold a BEFORE and an AFTER set at the same time.

| harness | writes to | separable runs? |
|---|---|---|
| `RT_PhotoTour` | `Build/artifacts/rendertest/phototour/<tag>/` via `--phototour-tag` | **yes** |
| `Test_MaterialEditorLivePreview` | `C:/tmp/mat_shot_*.tga`, hardcoded | no |
| `ZM_AnimEditorShowcase` | `Build/artifacts/zenithmon/animeditor/*.tga`, fixed | no |

So a before/after comparison across the material preview or the animation
preview requires the caller to copy artifacts out between runs by hand, every
time, and getting it wrong silently produces a comparison against the wrong
images rather than an error. `RT_PhotoTour` already shows the right shape.

The hardcoded `C:/tmp/` destination is separately worth removing: it is outside
the repo, outside `Build/artifacts/`, and therefore outside anything the
artifact conventions or `.gitignore` govern.

This surfaced on 2026-09-19 taking ZEN-12's evidence, where the BEFORE sets had
to be hand-copied into run scratch before the AFTER runs could be started.

## Definition of Done

- [ ] `Test_MaterialEditorLivePreview` and `ZM_AnimEditorShowcase` each accept a
      run tag and write under a per-tag directory, in the shape `RT_PhotoTour`
      already uses
- [ ] Neither writes outside `Build/artifacts/` any more
- [ ] With no tag supplied, each still writes somewhere predictable — a default
      tag, not a silent overwrite of the last run
- [ ] Running either twice with different tags leaves both sets on disk
- [ ] Every affected game's pinned baseline bumped from an OBSERVED run, if the
      change adds or removes a unit

## Comments

ran on sonnet/low via SIMPLE+LOW · category Engine

**Done** — merged to `master` as `42d797d3`. Verify phase: 20 lines, exit 0,
0 skipped, 0 flakes. Guard clean. Pins observed unchanged (Combat 2718,
Zenithmon 4571); no registration was added, so nothing to bump.

Both harnesses now take the engine-owned `--capture-tag <tag>` and write under a
per-tag directory, defaulting to `"run"`. `Test_MaterialEditorLivePreview`'s four
hardcoded `C:/tmp/mat_shot_*.tga` paths are gone; `ZM_AnimEditorShowcase`'s
captures move to `.../animeditor/<tag>/` while `AuthoredScratch` stays at the
untagged root (it is ZEN-91's promote target, not a capture artifact). Both now
fail loudly if their output directory cannot be created — the Zenithmon one
previously ignored its `error_code` entirely.

`RTArg` was deliberately NOT copied: it is RenderTest-local, reads
`__argc`/`__argv`, and is a no-op outside Windows, so following the model
literally would have made three Windows-only argv scanners for one job. The flag
lives in `Zenith_CommandLine`'s existing table, and its coverage folds into the
two existing parser tests.

**Proven by running them, because no gate does** — both are `requiresGraphics`,
so the headless batch skips them and green gates prove only that they compile.
Five DevilsPlayground tags produced five directories of four TGAs; Zenithmon's
`before`/`after` produced two of three, with `AuthoredScratch` unmoved.

**★ A regression I introduced, caught and fixed.** The first implementation used
a namespace-scope `std::string` array for the shot paths, and the harness then
segfaulted 5 of 5 runs. A control build — this file reverted to HEAD, everything
else kept — still crashed 2 of 5, so the crash is PRE-EXISTING (ZEN-91's
shortfall row; test PASSES, captures written, dies after
`[TaskSystem] Task system shutdown complete`). The array had made an intermittent
fault deterministic by adding four atexit destructors to a TU whose teardown
already faults. Fixed `char` buffers return it to 2 of 5. Recorded as
**`ZEN-D-031`**, which also notes that `ZM_AnimEditorShowcase` uses the identical
shape and does NOT crash, so nobody should "fix" Zenithmon's.

Follow-up **ZEN-105**: eight more test files still write captures to `C:/tmp`
(~10 paths) for exactly the reasons this ticket's Goal gives — my scoping grep
covered only the named file. It records the trap that three further `C:/tmp`
references are parser-test string fixtures, two of which exist to pin how an
out-of-repo path is handled.

<!-- agent:worklog:start ZEN-92 -->
## Work Log — 2026-09-21

ran on sonnet/low via SIMPLE+LOW · category Engine

**Outcome**: merged to `master` as `42d797d3` · ran on sonnet/low via SIMPLE+LOW

**The ticket's three claims all held**, which is worth saying because the last
several did not. `RT_PhotoTour` really is the right shape
(`PhotoTour.cpp:330-341`, tag with a `"run"` default);
`Test_MaterialEditorLivePreview` really had four hardcoded `C:/tmp/mat_shot_*.tga`
paths; `ZM_AnimEditorShowcase` really wrote to a fixed directory.

**What changed**

- `Zenith/Core/Zenith_CommandLine.{h,cpp}` — `--capture-tag <tag>` +
  `GetCaptureTag()`, in the existing flag table beside `--screenshot` /
  `--assets-root`, with the same `s_bParsed` guard the neighbouring accessors use.
- `Zenith/Core/Zenith_CommandLine.Tests.inl` — the new flag folded into the two
  existing parser tests (null when absent; captures the argv pointer when
  present). **No new registrations**, so no pin moves.
- `Games/DevilsPlayground/Tests/Test_MaterialEditorLivePreview.cpp` — writes
  `Build/artifacts/devilsplayground/materialpreview/<tag>/`; the four `C:/tmp`
  paths are gone.
- `Games/Zenithmon/Tests/ZM_AnimEditorShowcase.cpp` — captures move to
  `.../animeditor/<tag>/`; `AuthoredScratch` deliberately stays at the untagged
  root.

**★ NOT copying `RTArg` was the one real design call.** It is RenderTest-local
(`PhotoTour.cpp:161`), reads MSVC's `__argc`/`__argv` directly, and is
`#ifdef ZENITH_WINDOWS` with a no-op elsewhere. Following the model literally
would have produced three Windows-only argv scanners for one job. The engine
already had the right home.

**Gates** — verify phase, 20 lines, **exit 0, 0 skipped, 0 deferred, 0 flakes**.
Union pulled in Zenithmon and DevilsPlayground alongside Engine. Pins observed
unchanged: **Combat 2718**, **Zenithmon 4571**. `zagent guard`: ok, selection
current, verdict fresh.

**The DoD's behavioural claim, proven by running it**

Neither harness is run by any gate — both are `requiresGraphics`, so the headless
batch skips them and a green gate proves only that they compile.

| harness | run | result |
|---|---|---|
| `Test_MaterialEditorLivePreview` | 5 tags | 5 directories × 4 TGAs |
| `ZM_AnimEditorShowcase` | `before` + `after` | 2 directories × 3 TGAs |
| `ZM_AnimEditorShowcase` | — | `AuthoredScratch/` unmoved at the untagged root, both Fernfawn clips in it |

Also verified: neither harness cleared stale captures at Setup before this change
(both only called `create_directories`), so the DoD's "clear only your own tag"
hazard does not arise. That was the one thing I flagged to the worker as
unverified, and it checked it rather than assuming.

**★★ A REGRESSION I INTRODUCED, CAUGHT AND FIXED — the substantial part of this
tick.**

The first implementation stored the four shot paths in a namespace-scope
`std::string` array. With it, `Test_MaterialEditorLivePreview` segfaulted **5 of
5** runs. ZEN-91 had measured the same crash at 1 of 6, so that looked like a
self-inflicted deterministic break.

It is not self-inflicted, and establishing that took a control:

| build | exit 139 |
|---|---|
| control — this file at HEAD, my change absent | **2 of 5** |
| `std::string g_astrShotPath[4]` | **5 of 5** |
| `char g_aszShotPath[4][512]` | **2 of 5** |

So the crash is pre-existing (it happens with my change entirely removed), and
the `std::string` array made an intermittent fault deterministic by adding four
atexit destructors to a TU whose teardown already faults. The fixed buffers
return it to the control's rate exactly. Shipped that way, with the measurement
written at the declaration so it is not "modernised" back.

In every case the test PASSES (181 frames), every capture is written, and the
process dies after `[TaskSystem] Task system shutdown complete`. The underlying
teardown bug is untouched and still open under ZEN-91's shortfall row.

★ `ZM_AnimEditorShowcase` uses the same `std::string` array shape and does NOT
crash (7 windowed runs across ZEN-91 and ZEN-92, all exit 0), so this is not a
general rule about static `std::string` — it is specific to a process whose
teardown is already faulting. Recorded in `ZEN-D-031` so nobody "fixes"
Zenithmon's.

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `Zenith/Core/Zenith_CommandLine.{h,cpp}` | The two `CommandLine` parser units, in every game's boot suite — they ran in both observed unit gates. The accessor's guard path is covered by the defaults test. |
| `Zenith/Core/Zenith_CommandLine.Tests.inl` | Itself; assertions are non-vacuous (pointer identity into `argv`, and a null check when the flag is absent). |
| `Games/DevilsPlayground/Tests/Test_MaterialEditorLivePreview.cpp` | **No gate** — `requiresGraphics`. Compiled by `zenith build DevilsPlayground`; behaviour verified by 5 hand runs + a 5-run control + a 5-run fix measurement. |
| `Games/Zenithmon/Tests/ZM_AnimEditorShowcase.cpp` | **No gate**, same reason. Verified by 2 hand runs with different tags. |

**Definition of Done**

- [x] Both harnesses accept a run tag and write under a per-tag directory, in
      `RT_PhotoTour`'s shape — via the engine-owned `--capture-tag`.
- [x] Neither writes outside `Build/artifacts/` — the four `C:/tmp` paths are
      gone; the only remaining mention in that file is a comment explaining why.
- [x] With no tag, each still writes somewhere predictable — `"run"`, the same
      default `RT_PhotoTour` uses, not a silent overwrite.
- [x] Running either twice with different tags leaves both sets on disk —
      measured at 5 tags and 2 tags respectively.
- [x] Every affected game's pinned baseline bumped from an OBSERVED run, if the
      change adds or removes a unit — condition does not hold. No registration
      was added (0 new `ZENITH_TEST` lines); observed runs report Combat 2718 and
      Zenithmon 4571, unchanged. `Tools/unit_baselines.json` untouched.

**Durable findings**

- `ZEN-D-031` — a namespace-scope `std::string` array in a DevilsPlayground test
  TU turns that game's intermittent windowed-teardown SIGSEGV deterministic
  (2/5 → 5/5 → 2/5 across control, string, buffers); the crash is pre-existing,
  the statics only change its rate, and Zenithmon's identical shape is NOT
  affected.

**Deviations / follow-ups**

- **A scoping miss of mine.** I grepped only the file the ticket named. `C:/tmp`
  is still a write destination in **eight more test files** (~10 paths) — six in
  DevilsPlayground, two in RenderTest — with exactly the three problems this
  ticket's Goal describes. The DoD said "neither", meaning the two named
  harnesses, so the worker was right to stop; a repo-wide grep while scoping
  would have surfaced it before the work rather than after. Filed as **ZEN-105**,
  which also records the trap: three further `C:/tmp` references are string
  fixtures in parser tests, two of them existing specifically to pin how an
  out-of-repo absolute path is handled, so a grep-and-replace sweep breaks them.
- **The worker found a fourth harness I had not accounted for.**
  `Games/Zenithmon/Tests/ZM_AutoTests_PhotoTour.cpp` already has its own local
  `PTArg` / `--phototour-tag`. Out of this ticket's two-harness scope; left alone
  and reported rather than silently touched.
- **No review was required** for this ticket (`review.required: false`, risk LOW),
  so the verification above is entirely my own. Worth noting given that it is
  what found the regression.
- **Dawnmere drifted during the Zenithmon verification runs and was restored,
  not staged** — that is ZM-118 / `ZEN-D-030`, expected and not this ticket's.
<!-- agent:worklog:end -->
