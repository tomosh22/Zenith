---
id: "0693dc2a-dd98-4e28-be6d-3f57346510ab"
key: "ZEN-73"
type: "TASK"
title: "doc_lint check C9: reconcile AssetManifest roster counts and generator version stamps against the headers"
category: "Engine"
parent: "ZEN-28"
sprint: null
status: "Done"
assignee: "agent@zenith.invalid"
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 3
position: 73
labels: ["docs"]
fixVersions: []
blocks: []
updatedAt: "2026-09-21T04:33:06.181Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
claimAttempt: null
---
## Goal
Suggestions row 2026-08-25 (gates): *"doc_lint needs a check that reconciles AssetManifest.md's roster arithmetic and generator version stamps against the headers. C1 cannot be widened to cover them."* `Games/Zenithmon/Docs/AssetManifest.md` states per-family counts (152 species x 15 files, 34 humans, 30 buildings, 25 props) and the `uZM_*GEN_VERSION` stamps; every generator bump (ZM-D-219/221 bumped buildings 1 to 3, props 2 to 3, humans 3 to 6, creatures 3 to 4) has to be mirrored by hand and nothing reds when it is not.

Add doc_lint check C9 (C8 is the ArtBrief tick reconciliation): parse the roster counts and version stamps from `AssetManifest.md` and compare them to the compiled constants in the generator headers (`uZM_*GEN_VERSION`, the species/human/building/prop table sizes); report each mismatch by name. Self-test in `Tools/Test_*` harness style with a fixture that fails; run over Zenithmon and fix the current drift at its source.

## Definition of Done
- [ ] Check C9 reconciles every roster count and generator version stamp in `AssetManifest.md` against the headers, with a self-test
- [ ] Zenithmon passes it with the drift fixed in the document
- [ ] The Suggestions row is resolved with this key
- [ ] Every game's pinned baseline bumped from an OBSERVED run

## Comments

ran on sonnet/low via SIMPLE+LOW · category Engine

**Done.** Committed to `master` as `179a9aa5`. `doc_lint` gained **C11** —
`Get-ZmRosterCount` + `Check-RosterCounts` — reconciling the SPECIES / HUMAN / BUILDING / PROP
roster-count claims in `AssetManifest.md` and `TestPlan.md` against the `ZM_<FAMILY>_ID` enums,
with a 9-assertion self-test. Six drifted claims found and fixed: props 25 → 32 (two sites),
humans 34 → 35 (four).

**★ Read this before taking the ticket at face value.** Roughly half of it was already done,
and the Suggestions row it cites was already resolved:

* It asks for "C9". `C9` (ticket-key citations) and `C10` (prop/version stamps) were both
  taken — the new check is **C11**.
* Its framing, *"every generator bump has to be mirrored by hand and nothing reds when it is
  not"*, is **false for version stamps**: `C10` already reconciles every `uZM_*GEN_VERSION`,
  discovering generators by glob rather than naming them.
* The cited row (2026-08-25) has carried `Status: done` since `C10` landed, with a Resolution
  that accurately describes that work. **I did not re-resolve it** — overwriting an accurate
  record of C10 with a C11 narrative destroys a better record than the replacement. The row's
  ask is closed; this ticket extended the closure. Recorded as `ZEN-D-022`.

The remaining work was real: `C10` covered the **prop** roster only, so three families went
unchecked, and the looser phrasings the docs use for props were uncovered.

Gates: **28/28 green** on verify, 0 skipped, 0 flakes — both the Zenithmon and Tooling
categories unioned in, taking 15 lines to 28, including `doc_lint` across **five games**
(which is the graceful-degradation proof for the four with no `ZM_*Data.h`). Unit gates
unchanged at 2718 / 4571, so no pin moved. `zagent guard` clean, zero excursions.

**A green `doc_lint` only proves the current documents match.** Three hand runs stand behind
this, because no gate runs either assert script: the self-test at **9/9** (two assertions
pinning the absence of C10/C11 double-reporting in both directions), `doc_lint -Game Zenithmon`
at **all 11 checks pass**, and — the one that matters — re-introducing both prop drift sites,
which reds with **two `C11` violations and no `C10` duplicate**. Without that last step the
clean result is indistinguishable from a check that matches nothing.

Two design calls: `C10` left untouched (its test pins violation strings by exact text), and
`C11` anchored on enumerated family-specific phrasings because the tilde cuts both ways —
`~25 sets` for terrain is a real approximation over a family with no roster enum, while
`~25 models` for props was a stale exact number.

Also filed a Suggestions row: neither doc_lint assert script is in any gate list, and adding
them needs a human edit to the protected `zagent.project.json`.

<!-- agent:worklog:start ZEN-73 -->
## Work Log — 2026-09-21

ran on sonnet/low via SIMPLE+LOW · category Engine

**Outcome**: committed to `master` as `179a9aa5` · ran on sonnet/low via SIMPLE+LOW

Added `doc_lint.ps1` check **C11** — `Get-ZmRosterCount` + `Check-RosterCounts` — reconciling
the SPECIES / HUMAN / BUILDING / PROP roster-count claims in `AssetManifest.md` and
`TestPlan.md` against the `ZM_<FAMILY>_ID` enums in `Games/Zenithmon/Source/Data/*.h`, and
fixed the six drifted counts it surfaced.

**★ Roughly half this ticket was already done, and the Suggestions row it cites was already
resolved.** Established before dispatch, not discovered by a worker:

- The ticket says *"Add doc_lint check C9"*. `C9` is `Check-KeyCitations` and `C10` is
  `Check-PropGenStamps` — every id `C1`–`C10` was in use. The new check is `C11`.
- The ticket's framing — *"every generator bump has to be mirrored by hand and nothing reds
  when it is not"* — is **false for version stamps**. `C10` already reconciles every
  `uZM_*GEN_VERSION`, discovering generators by globbing `Source/Gen/ZM_*Gen.h` rather than
  naming them, against both linted documents.
- The cited Suggestions row (`j8d273n6…`, 2026-08-25) was already `Status: done`, and its
  Resolution describes precisely that: *"Fixed, and widened well past the original ask.
  doc_lint gained C10 … It covers `ZM_PROP_COUNT` roster arithmetic AND every generator
  version stamp."*

What genuinely remained: `C10` covers the **prop** roster only, so species, humans and
buildings were unchecked, and the looser phrasings the docs actually use for props were
uncovered. That is what `C11` adds, and it was worth doing — six claims had drifted.

**What changed**

- `Tools/doc_lint.ps1` — `Get-ZmRosterCount` (generalised from `Get-ZmPropRosterCount`),
  `Check-RosterCounts` (`C11`), wired into the invocation list; header list and banner
  updated 10 → 11. **`C10` untouched.**
- `Tools/Test_DocLintRosterCounts.ps1` (new) — 9 assertions in the black-box harness style of
  `Test_DocLintRosterStamps.ps1`.
- `Games/Zenithmon/Docs/AssetManifest.md` (3 sites), `TestPlan.md` (3 sites) — the drift.

**The six drift sites**, all against measured header counts (SPECIES 152, HUMAN 35,
BUILDING 30, PROP 32):

| file | was | now |
|---|---|---|
| `AssetManifest.md:102` | `\| Props \| ~25 models` | `~32` |
| `AssetManifest.md:265` | `re-skinning all 34` | `35` |
| `AssetManifest.md:347` | `## 3. Buildings (30) and props (25)` | `props (32)` |
| `TestPlan.md:833` | `~34 humanoid NPC roster rows` | `~35` |
| `TestPlan.md:845` | `` `RosterTotality` (all 34 rows build `` | `35` |
| `TestPlan.md:868` | `across all 34 models` | `35` |

`:845` was the one I flagged as "check it, do not assume" — it names a unit's behaviour
rather than a family size. The worker read `ZM_Tests_HumanGen.cpp` and confirmed
`HumanGen_RosterTotality` walks `ZM_HUMAN_COUNT` dynamically, so the literal 34 was drift.

**Two design decisions, made here rather than left open**

1. **Do not touch `C10`.** `Test_DocLintRosterStamps.ps1` pins its violations by exact
   string (`VIOLATION [C10]…claims 4 prop roster row(s)…has 3`), so re-homing prop checking
   into `C11` would red a passing test for no gain. `C11` uses only phrasings `C10` does not
   match, so the two cannot double-report — asserted twice in the new test, in both
   directions, and confirmed on the real documents.
2. **The tilde is the hazard and it cuts both ways.** `## 4. Terrain sets (…~25 sets)` is a
   genuine approximation over a family with **no roster enum**; `~25 models` for props was a
   stale exact number; `~35 models` for humans is an approximation that happens to be right.
   `C11` is therefore anchored on enumerated family-specific phrasings, never on "a number
   near a family word".

**Gates** — `-Phase verify`: `state: complete`, `exitCode: 0`, **28/28 green, 0 skipped,
0 deferred, 0 flakes**, `headSha` matched HEAD. `zagent gates` unioned in **two** categories:
Zenithmon (on the two doc files) and Tooling (on the two `Tools/*.ps1` files), taking 15 lines
to 28.

| Command | Result |
|---|---|
| 11 engine/game builds, incl. two `D3D12_*_False` non-tools compiles | PASS |
| `zenith test Combat --headless` / `zenith test Zenithmon --headless` | PASS |
| `run_unit_gate.ps1 -Game Combat` | PASS — 2718 / 2718, 0 failed |
| `run_unit_gate.ps1 -Game Zenithmon` | PASS — 4571 / 4571, 0 failed |
| `doc_lint.ps1` × **five games** (Zenithmon, DevilsPlayground, Foundry, Undervault, Hearth) | PASS |
| Tooling assert scripts + `Tools/Vault` `npm test` | PASS |
| **hand run** `Tools/Test_DocLintRosterCounts.ps1` | **9/9 assertions, exit 0** |
| **hand run** `doc_lint.ps1 -Game Zenithmon` | **ALL 11 CHECKS PASS** |
| **hand run** same, with both prop drift sites re-introduced | **exit 1, two `VIOLATION [C11]` lines, no `C10` duplicate** |

`zagent guard ZEN-73`: `ok: true`, no violations, `gateSelectionStale: false`,
`gateVerdictFresh: true`.

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `Tools/doc_lint.ps1` (`C11`) | **Five `doc_lint` gate lines execute it** — on Zenithmon with real headers, and on four games with none, which is the graceful-degradation path proven on the real tree rather than on a fixture. Its ability to CATCH drift is exercised only by the hand runs. |
| `Tools/Test_DocLintRosterCounts.ps1` | **Nothing.** No gate runs it — see below. Executed by the hand run above. |
| `AssetManifest.md`, `TestPlan.md` | Read by `doc_lint -Game Zenithmon`, which is a gate line. The six corrected numbers are exactly what `C11` now asserts. |

★ **A green `doc_lint` does not prove `C11` works.** It proves the current documents match.
Only the anti-vacuity probe — re-introducing real drift and watching it red — distinguishes a
working check from one that matches nothing, and no gate performs it.

**Definition of Done**

- [x] The check reconciles every roster count in `AssetManifest.md` / `TestPlan.md` against
      the headers, with a self-test — `C11` over all four families, 9 assertions. Filed as
      "C9"; it is C11, because C9 and C10 were taken.
- [x] Zenithmon passes it with the drift fixed in the document — all 11 checks pass, six
      sites corrected.
- [x] The Suggestions row is resolved — **it already was, under different work, and I
      deliberately did not re-resolve it.** Row `j8d273n6…` has carried `Status: done` since
      C10 landed, with a Resolution that accurately describes C10 discovering generators by
      glob and covering prop roster arithmetic. Overwriting that with a C11 narrative would
      destroy a better record than the replacement. The row's ask is closed and traceable;
      this ticket extended the closure rather than effecting it. Recorded as `ZEN-D-022`.
- [x] Every game's pinned baseline bumped from an OBSERVED run — **observed: no move.**
      PowerShell and Markdown only, no C++ units. Both unit gates reported `ran == baseline`
      exactly (Combat 2718, Zenithmon 4571) in both phases, so `Tools/unit_baselines.json` is
      untouched and no LIVE PIN narration changed.

**Durable findings**: `ZEN-D-022`

- `ZEN-D-022` — a ticket filed against a Suggestions row must be re-checked against that
  row's Status and Resolution before it is claimed: the fix may already have shipped, and the
  ticket will not know. Includes the rule not to re-resolve an already-resolved row to
  satisfy a DoD box.

**Deviations / follow-ups**

- **Zero excursions.** All four changed files are on `filelist.txt`.
- **One fix the worker deliberately left, which I took.** `doc_lint.ps1`'s own exit-code
  comment read *"0 -- all 7 checks pass"*, stale since the file reached 10. The worker
  updated the header list and the banner but left this third site and flagged it as
  pre-existing and out of scope. In a ticket whose entire subject is documents contradicting
  code, shipping a file that contradicts itself is the wrong call — so I replaced the count
  with a pointer to where the count lives, rather than adding a fourth place to forget it.
- **A documented blind spot the worker surfaced rather than hid.** Every check in
  `doc_lint.ps1` reads one line at a time, so a claim split across a Markdown line wrap is
  invisible to all of them — and `AssetManifest.md:265` ("all 34\nhumans") is exactly that
  shape. It was fixed by hand and the gap documented in `C11`'s header, with the reasoning
  for not joining adjacent lines (a join risks colliding two unrelated claims on consecutive
  lines, a new failure mode nobody has measured). `TestPlan.md`'s
  `BuildingGen_RosterTotality (all\n30 rows…` wraps the same way and was never stale, which
  is evidence the shape recurs independently of correctness.
- **Suggestions row filed**: neither doc_lint assert script is in any gate list.
  `Test_DocLintRosterStamps.ps1` (C10's, 17 assertions) has never been gated, and the new
  `Test_DocLintRosterCounts.ps1` is not either. The `Tooling` category names its assert
  scripts individually and already lists `Test_DocLintKeyCitations.ps1`, so the pattern
  exists — but `zagent.project.json` is a `protectedPath`, so adding the two lines is a human
  edit.
- **Out of scope, deliberately, and still stale:** `Games/Zenithmon/Docs/Shortfalls.md:233`
  and `DecisionLog.md:8927` both say "34". The DecisionLog is append-only history and records
  what was true when written; Shortfalls is outside the two-document set `C10` and `C11`
  lint. Neither was edited, and `C11` was not widened to reach them.
- **Not checked by anyone:** whether `C11`'s phrasing list covers renderings a future doc
  rewrite might use (it is deliberately narrow, matching the restraint of `C1`/`C7`/`C9`/`C10`);
  and whether any other game will ever carry `ZM_*Data.h`-shaped rosters, in which case the
  Zenithmon-specific `ZM_` prefix in the scraper becomes a limit rather than a scoping choice.
<!-- agent:worklog:end -->
