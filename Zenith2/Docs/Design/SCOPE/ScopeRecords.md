# SCOPE design: scope records and the scope register

This file is part of the SCOPE design family; the entry document is `Zenith2/Docs/Design/SCOPE.md`. It holds one decision record per scope boundary the programme names (consoles, VR/XR, modding, local multiplayer, dedicated servers, online backends), the machine-readable scope register and its checks, the rule that deferred scopes carry full requirement trees now, and the Stage 1 SCOPE decision tickets.

## Scope of this file

The scope table this file elaborates is `Zenith2/Docs/Architecture.md` → "Purpose and scope" (subsection "Scope") and bullets [Z2-N-ARCH-003] and [Z2-N-ARCH-004]; the inventory rows are `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1" (SCOPE). The settled rulings are Z2-D-071 to Z2-D-076; this file adds, per boundary, the options considered, the option chosen and the reason in the form a Stage 1 decision ticket confirms, plus SCOPE's own rulings Z2-D-133 and Z2-D-136 to Z2-D-140 and the two that await the human, SC-R2 and SC-R3 (`Zenith2/Docs/Design/SCOPE.md` → "Rulings this design makes").

## Record format

Each record below has five parts, in this order:

| Part | Content |
|---|---|
| Boundary | what is in and what is out, in one sentence each |
| Options | every option considered, numbered, each with its consequence for the architecture |
| Chosen | the option chosen, by number, and the decision that holds it |
| Reason | why the chosen option wins, in measurable terms where a measure exists |
| Owner and tree | the area that owns the requirement tree, the requirement range or catalogue that holds it, and the checks that enforce the boundary |

A record never re-specifies its owner's design: where another area owns the tree, the record names its heading.

## Consoles record

**Boundary.** In: PlayStation 5, Xbox Series X and S, and Nintendo Switch 2 as Stage 7 platforms, each gated on SDK access (ruling Z2-D-133). Out: earlier generations (PlayStation 4, Xbox One, Nintendo Switch), handheld PC variants as distinct platforms (they run the Windows build), and any console work before its platform's SDK-access ticket is Done.

**Options.**

1. Consoles in scope from Stage 3 with the flagship slices — needs SDK access and devkits now, which the programme does not have; every engine contract would carry a third platform before it is proven on two.
2. Consoles excluded — loses general-purpose AAA scope; an addition after Stage 6 would meet contracts written without them.
3. Consoles deferred to Stage 7, gated per platform on SDK access, with neutral seams kept now and the tickets specified without NDA material.
4. Consoles deferred with no tickets until access exists — leaves the scope undecomposed and uncovered, contradicting the rule that deferred scopes get full trees (Z2-D-070).

**Chosen.** Option 3 (Z2-D-071), with the platform set, `game.json` values and per-platform gating of Z2-D-133. Where NDA material lives is not ruled here: the private-overlay design of `Zenith2/Docs/Design/SCOPE/Consoles.md` is flagged proposal SCOPE-1, an exception to Z2-D-002 and [Z2-N-ARCH-263] that only the human may approve (pending Q98). Until it is ruled, every console ticket is `needs-human`, so no loop worker reads or writes console material in any outcome of Q98.

**Reason.** Option 3 is the only option that keeps every Stage 1 to Stage 6 contract free of console assumptions while proving, now, that each console is an additional backend behind an existing seam (`Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements": 14 seams, each already with two implementations, Windows and Android). The three platforms are the current-generation consoles with a public developer programme; the earlier generations are excluded because their GPUs cannot be assumed to meet the baseline of Z2-D-013 (bindless descriptor indexing at the required limits, 16-bit shader arithmetic, dynamic rendering), and the baseline is not lowered for them.

**Owner and tree.** SCOPE: `Zenith2/Docs/Design/SCOPE/Consoles.md`; the requirement IDs the register's `consoles` row lists; enforced by the coverage gate step's routing and link checks (every console ticket in the Stage 7 sprint, `blocks`-linked from its platform's SDK-access ticket) and by `Pwsh_ScopeRegister_Valid` (§ "Register checks").

## XR record

**Boundary.** In: virtual reality through OpenXR 1.1 on Windows PC VR, both RHIs, the opaque environment blend mode, in Stage 7 (ruling SC-R3); XR-ready multi-view and input from Stage 2 (Z2-D-072). Out: a console's own headset (excluded); standalone Android headsets and passthrough, which are D-XR-PLATFORMS (open, Stage 7); vendor-specific XR SDKs (every call goes through OpenXR); hand tracking, eye tracking, foveated rendering and body tracking extensions (no inventory item; a future extension enters through a new decision).

**Options.**

1. XR in scope now — no flagship slice needs it before shipping qualification, and it would make every rendering ticket carry two eyes from Stage 3.
2. XR excluded with no readiness — a retrofit restructures views (one camera), input (device-bound) and pacing (the swapchain present).
3. XR deferred to Stage 7 through OpenXR, with multi-view and input kept XR-ready by named checks.
4. XR deferred through a vendor SDK per headset — couples the engine to each vendor and multiplies the tree.

**Chosen.** Option 3 (Z2-D-072), with SC-R3 (OpenXR 1.1, Windows PC VR, opaque; pending Q99, the human's confirmation of the narrowing and of the console-headset exclusion), Z2-D-136 (the control-mirror input), Z2-D-137 (pre-device startup), Z2-D-138 (the test strategy) and Z2-D-139 (readiness checks bind earlier tickets).

**Reason.** OpenXR is the cross-vendor API every PC VR runtime implements (Meta's, SteamVR's, and the open-source Monado), so one integration reaches the reference matrix (`Zenith2/Docs/Design/SCOPE/XrTests.md` → "Reference device matrix"). Version 1.1 is chosen over 1.0 because it promotes the local-floor reference space to core, which the reference-space choice uses without an extension (`Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Reference spaces"); a runtime offering only 1.0 is refused with a logged reason. Windows alone is chosen because the one validated Android device is the S24 (Z2-D-010), which has no headset; adding a second validated Android device is exactly the question D-XR-PLATFORMS asks. A console headset is excluded rather than deferred because it needs a console SDK and a second, non-OpenXR integration, the cost option 4 rejects.

**Owner and tree.** SCOPE for the runtime, session, frame cycle, swapchains, layers, spaces and the OpenXR device source; RPROD for the stereo views (`Zenith2/Docs/Design/RPROD/Views.md` → "Advanced multi-view"), PLAT for the POSE value and the `interactionProfile` key (`Zenith2/Docs/Design/PLAT/Input.md` → "XR-ready design"), GAME for the stereo camera output (`Zenith2/Docs/Design/GAME/Cameras.md` → "Camera model"). Enforced by the readiness checks of `Zenith2/Docs/Design/SCOPE/XrReadiness.md` and the `z2-xr` tests.

## Modding record

**Boundary.** Out entirely: third-party mod loading of any form — code, scripts, content packs or data overrides from outside the game's own package, patch, loose and user mounts. In: nothing; the user mount holds saves, settings and user bindings and never provides content.

**Options.**

1. Native-code mods (dynamic libraries) — contradicts static modules (Z2-D-024) and the no-runtime-code-loading rule.
2. Script-only modding through Luau — needs a stable script API across releases and an untrusted-content boundary the sandbox does not claim (Z2-D-016 governs gameplay scripts the game ships, not foreign ones).
3. Data-only mods (content packs mounted above the package) — needs a mount precedence above `content:/` that the virtual filesystem forbids, and cooked data refused on every engine version change (Z2-D-048) would break each pack at each release.
4. Modding deferred to Stage 7 — the static-module and in-repo model (Z2-D-002, Z2-D-024) would have to be reversed then.
5. Modding excluded.

**Chosen.** Option 5 (Z2-D-075).

**Reason.** Options 1 to 4 each contradict a settled choice; none is needed by a flagship slice; excluding it removes a trust boundary from the security architecture.

**Owner and tree.** No tree (excluded). Enforcement belongs to MOD and ARCH and is listed in the register: [Z2-N-ARCH-262] (no runtime code loading), [Z2-N-ARCH-274] (`ForbiddenCheck` on `LoadLibrary*` and `dlopen`), [Z2-N-ARCH-264] (no content outside the four mounts), and MOD's exclusion requirement (`Zenith2/Docs/Design/MOD.md` → "Exclusions").

## Local multiplayer record

**Boundary.** Out: more than one local player in one process — split-screen, shared-screen co-op and hot-seat play with per-player input. In: one local player (player slot 0) whose every input device belongs to slot 0; other player slots exist only for networked players of NET's Stage 7 tree.

**Options.**

1. Split-screen in scope — per-player render views, input contexts, UI focus, audio listeners and camera directors in every system, for no flagship slice.
2. Shared-screen local co-op only (one view, several players) — still needs per-player input contexts, device pairing and per-player UI focus.
3. Deferred to Stage 7 — no planned slice or stage needs it, and keeping it possible would make every view and input design carry per-player cost now.
4. Excluded.

**Chosen.** Option 4 (Z2-D-076).

**Reason.** Option 4 lets render views never be per-player and every local device belong to slot 0, which simplifies RPROD (at most one main view), PLAT (an index-less gamepad path binds every gamepad) and UI (one focus).

**Owner and tree.** No tree (excluded). Enforced by PLAT's device rule (`Zenith2/Docs/Design/PLAT/Input.md` → "Control paths": every device belongs to player slot 0), by RPROD's single main view (`Flux_ViewList::m_uMainCount` is 0 or 1, `Zenith2/Docs/Design/RPROD/Views.md` → "Flux_ViewContext header"), and by SCOPE's `Test_ScopeExclusion_LocalDevicesShareSlotZero` (`Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Exclusion checks SCOPE owns").

## Dedicated servers record

**Boundary.** In, deferred to Stage 7: networked multiplayer and dedicated server builds, with foundations in Stage 1 and a two-process loopback proof in Stage 3 (Z2-D-073). Out of SCOPE: the tree itself.

**Options.**

1. Networking and dedicated servers in scope for shipping — single-player production comes first.
2. No foundations until Stage 7 — network IDs, authority and fixed ticks retrofitted would restructure ECS and scripting.
3. Deferred with foundations now and a loopback proof in Stage 3.
4. Excluded — loses the general-purpose scope the plan names.

**Chosen.** Option 3 (Z2-D-073).

**Reason.** The foundations (stable network IDs, a server-authoritative default, fixed ticks, replayable commands shared with input recording, a headless configuration) are small now and decisive in Stage 7 (`Zenith2/Docs/Architecture.md` → "Multiplayer foundations").

**Owner and tree.** NET (`Zenith2/Docs/Design/NET.md`, with the Stage 7 tree in `Zenith2/Docs/Design/NET/Stage7.md` → "Dedicated server builds" and its catalogue `Zenith2/Docs/Requirements/NET/Stage7.md`, Z2-R-NET-108 to Z2-R-NET-158); the dedicated server's platform is D-NET-SERVER-PLATFORM (open, Stage 7), NET's, and its gate lines are PROC's (`Zenith2/Docs/Design/PROC/GateList.md` → "Stage 7 lines"). SCOPE's assumption, recorded in cross-area row SP11: on a console, network transport is implemented by that console's backend behind NET's transport seam, so NET's tree needs no console-specific requirement. The register's `dedicated-servers` row lists NET's range explicitly.

## Online backends record

**Boundary.** In: the `Zenith_OnlineServices` adapter with an offline implementation in Stage 4, and the Steam and Google Play backends in Stage 6 (Z2-D-074). Deferred with consoles: each console's platform services, as a backend of the same adapter (`Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements"). Out of SCOPE: the tree itself.

**Options.**

1. A store SDK called directly by gameplay — couples gameplay to one store and blocks testing without it.
2. No online services — account and cloud-save flows are part of shipping qualification.
3. An adapter with an offline implementation first, store backends in Stage 6, console services behind the same adapter in Stage 7.

**Chosen.** Option 3 (Z2-D-074).

**Reason.** The offline implementation lets the account-change, entitlement and cloud-conflict flows be built and tested without a store (`Zenith2/Docs/Design/ONLINE/Backends.md` → "Backend contract").

**Owner and tree.** ONLINE (`Zenith2/Docs/Design/ONLINE.md`; its Stage 6 backend requirements Z2-R-ONLINE-051, -052, -116 to -135, -152 and -153, listed explicitly in the register's `online-backends` row), and SCOPE for the console backends' requirement (`Zenith2/Docs/Design/SCOPE/Consoles.md` → "Per-platform requirement tree").

## The scope register

The register is `Zenith2/Docs/Scope/ScopeRegister.json`, UTF-8, LF line endings, two-space indentation, keys in the order below. Ruling Z2-D-140: every boundary lists its requirements as explicit IDs or ID ranges of a named catalogue, never as a catalogue group, and nothing in the register depends on the label of a ruling, so the replacement of SC-R labels by their entry IDs breaks no check.

```json
{
	"schema": "zenith2.scope-register",
	"schemaVersion": 1,
	"meta": { "catalogue": "Zenith2/Docs/Requirements/SCOPE.md", "ids": ["Z2-R-SCOPE-001", "Z2-R-SCOPE-002", "Z2-R-SCOPE-003", "Z2-R-SCOPE-010"] },
	"boundaries": [
		{
			"id": "consoles",
			"title": "Consoles: PlayStation 5, Xbox Series X and S, Nintendo Switch 2",
			"classification": "deferred",
			"stage": "S7",
			"decisions": ["Z2-D-071"],
			"rulings": ["Z2-D-133"],
			"owner": "SCOPE",
			"inventory": ["Z2-I-SCOPE-01", "Z2-I-SCOPE-02", "Z2-I-SCOPE-03"],
			"requirements": [
				{ "catalogue": "Zenith2/Docs/Requirements/SCOPE.md", "ids": ["Z2-R-SCOPE-004", "Z2-R-SCOPE-011", "Z2-R-SCOPE-012", "Z2-R-SCOPE-014", "Z2-R-SCOPE-117", "Z2-R-SCOPE-118"] },
				{ "catalogue": "Zenith2/Docs/Requirements/SCOPE.md", "range": ["Z2-R-SCOPE-093", "Z2-R-SCOPE-115"] }
			],
			"design": [{ "path": "Zenith2/Docs/Design/SCOPE/Consoles.md", "heading": "Per-platform requirement tree" }],
			"enforcement": [{ "kind": "coverage", "detail": "every console ticket blocks-linked from its platform's SDK-access ticket" }],
			"sdkGate": { "platforms": ["playstation5", "xboxseries", "switch2"] }
		}
	],
	"readiness": [
		{
			"id": "XRR-V1",
			"title": "VIEW_KIND_STEREO declared and rejected until Stage 7",
			"requiredFrom": "S2",
			"retiredAt": "S7",
			"owner": "RPROD",
			"heading": { "path": "Zenith2/Docs/Design/RPROD/Views.md", "heading": "View list construction" },
			"tests": ["Test_View_StereoRejected"],
			"replacedBy": ["Test_View_StereoWithoutRuntimeRejected"]
		}
	]
}
```

| Key | Type | Rule |
|---|---|---|
| `schema` | string | exactly `zenith2.scope-register` |
| `schemaVersion` | integer | `1`; any other value fails the check with `ScopeRegister.json: schemaVersion <n> is not supported` |
| `meta` | object | `{ catalogue, ids }`: SCOPE requirements that belong to the register itself rather than to a boundary |
| `boundaries` | array | one object per scope boundary; ids unique, `^[a-z][a-z0-9-]{1,47}$` |
| `boundaries[].classification` | string | `required`, `deferred`, `excluded`, `game-side` or `research` |
| `boundaries[].stage` | string | `S0` … `S7`, or `—` for `excluded` |
| `boundaries[].decisions` | array of string | at least one `Z2-D-NNN`, each a heading of `Zenith2/Docs/DecisionLog.md` |
| `boundaries[].rulings` | array of string | each either a SCOPE label `SC-R<n>` that is a row of `Zenith2/Docs/Design/SCOPE.md` → "Rulings this design makes", or a `Z2-D-NNN` heading of the decision log; the replacement of a label by its entry ID is a value change, never a schema change |
| `boundaries[].owner` | string | an area code of `Zenith2/Docs/Requirements/Inventory.md` → "Area codes" |
| `boundaries[].inventory` | array of string | `Z2-I-<CODE>-NN` IDs of that file |
| `boundaries[].requirements` | array | at least one `{ catalogue, ids }` (explicit `Z2-R-<CODE>-NNN` IDs) or `{ catalogue, range }` (`[first, last]` of one code, inclusive, first ≤ last) entry; every ID must be a `### Z2-R-…` heading of that catalogue file |
| `boundaries[].design` | array | `{ path, heading }` pairs, each resolving to exactly one heading |
| `boundaries[].enforcement` | array | `{ kind, detail }` with `kind` one of `coverage`, `normative` (detail: a `[Z2-N-…]` ID), `test` (detail: a test name), `requirement` (detail: a `Z2-R-…` ID), `review` |
| `boundaries[].sdkGate` | object | consoles only: the `game.json` platform values whose SDK-access tickets gate their tickets |
| `readiness` | array | one object per readiness check of `Zenith2/Docs/Design/SCOPE/XrReadiness.md`, ids `^XRR-[VIUO][0-9]{1,2}$` unique |
| `readiness[].requiredFrom` | string | the stage from which the check holds |
| `readiness[].retiredAt` | string or absent | the stage whose ticket replaces the tests with `replacedBy` |
| `readiness[].heading` | object | the owner heading the check binds |
| `readiness[].tests` | array of string | test case IDs (`Test_<Type>_<Behaviour>`), each declared in exactly one doctest `TEST_CASE` of the tree once the check's stage has landed |

The register holds, at Stage 1, eight boundaries with these requirement lists:

| Boundary | Classification | Requirements |
|---|---|---|
| `consoles` | deferred, S7 | SCOPE 004, 011, 012, 014, 093 to 115, 117, 118 |
| `xr` | deferred, S7 | SCOPE 005, 013, 019 to 033, 035 to 092, 116, 119 (two ranges, so 034 stays with `local-multiplayer`) |
| `modding` | excluded | SCOPE 006, 015 |
| `local-multiplayer` | excluded | SCOPE 007, 016, 034 |
| `dedicated-servers` | deferred, S7 | SCOPE 008; NET 108 to 158 (`Zenith2/Docs/Requirements/NET/Stage7.md`) |
| `online-backends` | deferred, S6 | SCOPE 009; ONLINE 051, 052, 116 to 135, 152, 153 (`Zenith2/Docs/Requirements/ONLINE.md`) |
| `console-xr` | excluded | SCOPE 017 |
| `xr-platforms` | research, S7 | SCOPE 018 |

`meta` holds SCOPE 001, 002, 003 and 010. Every SCOPE requirement appears in exactly one boundary or in `meta`.

## Register checks

Two checks, both `pwsh` scripts registered by name in `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` as CTests running `pwsh -NoProfile -File <script> -Root <ZENITH_ROOT>` with a 60-second timeout.

### Pwsh_ScopeRegister_Valid

Label `z2-cmake`, run by every `Test` step. Steps, each failure printing `ScopeRegister.json: <pointer>: <message>` and making the test fail, all failures reported before exit:

1. Parse the file with `ConvertFrom-Json -Depth 32`; a parse error fails with the line.
2. Check `schema` and `schemaVersion`.
3. For each boundary: the id pattern and uniqueness; `classification` and `stage` in their sets, and `stage` `—` exactly when `classification` is `excluded`.
4. Read every `### Z2-D-NNN` heading of `Zenith2/Docs/DecisionLog.md`; each `decisions` entry must be one of them.
5. Each `rulings` entry is either a `Z2-D-NNN` heading of the decision log (step 4's set) or an `SC-R<n>` label in the first column of the table under `Zenith2/Docs/Design/SCOPE.md` → "Rulings this design makes" (the cell's text up to the first space).
6. Each `inventory` entry must appear as a row ID in `Zenith2/Docs/Requirements/Inventory.md`.
7. Each `requirements` entry: the catalogue file (and, for NET, ONLINE and other areas whose catalogue is split, the named section file) exists; read its `### Z2-R-<CODE>-NNN <title>` headings and, under each, its `**Stage:**` and `**Classification:**` lines; every `ids` member and every ID of a `range` (expanded numerically) is one of those headings. Group lines and bold paragraphs are never read.
8. Each `design` pair resolves to exactly one heading of that file (the docs checker's rule, repeated here so the gate catches a rename).
9. Over exactly the requirements step 7 resolved for the boundary: a `deferred` boundary has at least one requirement whose `Stage` is the boundary's stage and whose `Classification` is `deferred` or `required`; an `excluded` boundary has at least one requirement classified `excluded` whose `Decisions` line names at least one `Z2-D-` ID; a `research` boundary has a requirement classified `research`.
10. An `excluded` boundary has at least one `enforcement` entry of kind `normative`, `test` or `requirement`.
11. For each readiness row: the pattern and uniqueness of its id, stages in order (`requiredFrom` < `retiredAt` when both exist), `heading` resolving, at least one test name matching `^Test_[A-Za-z0-9]+_[A-Za-z0-9]+$`, `replacedBy` present exactly when `retiredAt` is.
12. Every `### Z2-R-SCOPE-NNN` heading of `Zenith2/Docs/Requirements/SCOPE.md` is listed exactly once across `meta` and every boundary's SCOPE entries.

Fixtures (generated by the test's own setup into `${ZENITH_OUT}/test-fixtures/SCOPE/register/`, never committed): a valid register over a generated three-requirement catalogue; one register per failure of steps 2 to 12, including a range whose first is after its last, an ID absent from the catalogue, an excluded boundary whose only requirement is `deferred`, a ruling label absent from the rulings table, a ruling given as a `Z2-D-NNN` that the decision log holds (passes), a SCOPE requirement listed twice, a requirement inside a range and also listed explicitly, and a requirement in no boundary. `Pwsh_ScopeRegister_Valid` runs the script over the tracked file, and its companion `Pwsh_ScopeRegister_ValidFixtures` runs it over each fixture and asserts the valid ones pass and each other fails naming its step.

### Pwsh_ScopeRegister_ReadinessTestsPresent

Label `z2-acceptance-scope`, excluded from the default `Test` run by the acceptance label rule and run by every stage acceptance (`Zenith2/Docs/Design/PROC/BaseSteps.md` → "-Acceptance -Stage N"). It reads the stage N from the environment variable `ZENITH_ACCEPTANCE_STAGE` (row SP10: `Run-Committed.ps1 -Acceptance -Stage N` exports it to every line); an absent or non-numeric value fails with `ZENITH_ACCEPTANCE_STAGE is not set; this check runs only in a stage acceptance`.

1. Collect every doctest case name in `Zenith2/Engine/**/Tests/**`, `Zenith2/Tests/**` and `Zenith2/Tools/**/Tests/**` by scanning `TEST_CASE("…")` literals; map each to its test ID (`Test_<Type>_<Behaviour>` from `<Library>/<Type>/<Behaviour>`, `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates").
2. For each readiness row with `requiredFrom` ≤ N and (no `retiredAt` or `retiredAt` > N): every name of `tests` occurs exactly once; otherwise fail with `readiness <id>: test <name> is missing (owner <owner>, <path> → "<heading>")` or `… is declared <k> times`.
3. For each row with `retiredAt` ≤ N: every name of `replacedBy` occurs exactly once.

The check proves that no ticket after the row's stage removed or renamed a readiness test without replacing it; that the test passes is proven by the ordinary `Test` step in the same acceptance run.

## Deferred scopes and their trees

Every deferred boundary carries a requirement tree now, at the detail of a required one (Z2-D-070): consoles in `Zenith2/Docs/Design/SCOPE/Consoles.md`, XR in the four XR files of this family, networking in NET's catalogue, online backends in ONLINE's. A deferred requirement names its stage (S7, or S6 for the online backends), its acceptance evidence and its design heading exactly as a required one does; the coverage gate step demands a leaf implementation ticket and an integration or acceptance ticket for each, filed in its stage's sprint. Register check step 9 makes a boundary without such a requirement fail the `Test` step.

## Stage 1 SCOPE decision tickets

Six `needs-human` TASKs, kind `decision`, in the Stage 1 sprint, fix version `Z2 Stage 1`, one per record above. Each ticket's diff:

1. confirms its record of this file as the record of its decision (the human compares it with the settled entry and with Stage 1 evidence: for consoles and XR, that no Stage 1 ruling — D-SLANG, D-SEL-PHYSX-ANDROID, the S24 profile — changes the record; for dedicated servers and online backends, that NET's and ONLINE's catalogues hold the trees);
2. for consoles and XR, appends to `Zenith2/Docs/DecisionLog.md` the SCOPE rulings of that record that are not yet appended (SC-R3 only after Q99 is answered; Z2-D-133 and Z2-D-136 to Z2-D-139 are appended already), with the numbers reserved for them, unless the core-doc patch already appended them; the consoles ticket appends the overlay entry (number 134) only when the human approved flagged proposal SCOPE-1 (Q98), and otherwise records the human's alternative;
3. replaces the boundary's `rulings` in the register by the appended `Z2-D-NNN` IDs.

A seventh ticket, an ordinary TASK (kind `implementation`, Stage 1 sprint), lands `Zenith2/Docs/Scope/ScopeRegister.json`, both checks, the fixture generator and the test registrations; the six decision tickets are blocked by it, so each edits a register that exists.
