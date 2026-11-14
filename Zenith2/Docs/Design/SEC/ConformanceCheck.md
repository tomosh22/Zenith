# SEC design: the SecurityConformance gate step

This file is part of the SEC design family; the entry document is `Zenith2/Docs/Design/SEC.md`. It specifies the gate step that makes the parser-limit conformance rule, the fuzz inventory rules, the path-traversal matrix and the update-key record mechanical: its inputs, command line, every check with its message, its report and its failing fixtures. The step is a protected gate script, so its implementation is the PROC `needs-human` hand-off SEC-P1 (`C:/dev/Zenith2-drafts/findings/A5_proc_handoffs.md`, section SEC); this file is the specification that hand-off implements, and no ordinary ticket edits the script.

## Scope of this file

| Topic | Heading |
|---|---|
| what the step reads | "Inputs" |
| how it is invoked and what it returns | "Command line and exit codes" |
| SC01 to SC16 | "Checks" |
| output | "Messages", "Report file" |
| proving every check can fail | "Fixtures" |
| where it runs and what it costs | "Gate placement and budget" |
| the protected edits | "PROC hand-off" |

## Inputs

| Input | Path | Owner |
|---|---|---|
| the parser inventory registry | `Zenith2/Tools/Security/parser_inventory.json` | SEC (unprotected; every parser-owning ticket edits its entry) |
| the path-traversal matrix | `Zenith2/Tools/Security/path_matrix.json` | SEC (unprotected) |
| the update-key record | `Zenith2/Tools/Security/update_keys.json` | SEC (unprotected, in `reviewPaths`) |
| the parser limits table | `Zenith2/Docs/Architecture.md`, heading "Parser limits" | Architecture |
| the decision log | `Zenith2/Docs/DecisionLog.md` | every ruling ticket |
| the declared dependencies | `Zenith2/out/<checkout-hash>/declared_deps.json` | written by the configure ([Z2-N-ARCH-182]) |
| the source tree | every tracked and untracked file under `Zenith2/Engine/`, `Zenith2/Tools/`, `Zenith2/Player/`, `Zenith2/Tests/` (`git status --porcelain --untracked-files=all` plus `git ls-files`) | every area |

The schemas of the three SEC files are `Zenith2/Docs/Design/SEC.md` → "Parser inventory registry", "Path matrix registry" and `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "update_keys.json". The step reads them through the readers of the protected module `Zenith2/Tools/Gates/Security/Zenith2SecurityGates.psm1` (`Zenith2/Docs/Design/SEC.md` → "Zenith2SecurityGates module"), so every line of logic the step runs is protected; only the data files are outside `Zenith2/Tools/Gates/`, under `reviewPaths` (hand-off SEC-P8).

## Command line and exit codes

```
pwsh -NoProfile -File Zenith2\Tools\Gates\SecurityConformance.ps1 [-Root <repository root>] [-Out <directory>]
```

- `-Root` defaults to the repository root derived from the script's own location; `-Out` defaults to `Zenith2/out/<checkout-hash>/gates/`.
- The step is self-sufficient: when `declared_deps.json` is absent it runs the `Configure` step first, as every step does ([Z2-N-CONTRACT-573]).
- Exit 0: no finding other than known findings. Exit 1: at least one finding. Exit 2: a usage error, an unreadable input or a failed configure.
- The step writes only under `-Out` and reads everything else.

## Checks

Each check runs over every entry and reports every finding, so one run lists all of them:

| Check | Rule | Fails when |
|---|---|---|
| SC01 | registry well-formed | a registry file is missing, is not valid JSON, or has the wrong `schema` or `schemaVersion` |
| SC02 | entry fields | an `id` does not match `^PI-[A-Z]+-\d{3}$` or repeats; `library` is no `libraries[].name` or `tools[].name` of `declared_deps.json`; a `boundary` value is outside the nine classes; `tier` differs from the lowest tier of the entry's classes; an `owner` is neither `^Z2T-[A-Z]+-\d{3}$` nor `pending:<AREA>`; `status` holds a value outside "Entry columns" of `Zenith2/Docs/Design/SEC/ParserInventory.md`; `fuzz.ubsan` differs from whether `linux-tsan` is in the owning target's `supported` (a `pending:` owner of a `gap` entry is reported under `known`) |
| SC03 | limits located | (a) a value of `limitsRows` names no row of the Architecture table "Parser limits" by its first cell (compared after removing backticks and trimming); (b) `limitsDesign` is set and is not a `path → "heading"` that resolves; (c) `limitsRows` is empty, the status is not `property`, and either the status lacks both `pending-row` and `limits-in-design` or `limitsDesign` is null; (d) the status carries `pending-row` or `limits-in-design` while `limitsRows` is not empty. An entry with rows and no `limitsDesign` passes |
| SC04 | table covered | an Architecture "Parser limits" row is named by no value of any entry's `limitsRows` |
| SC05 | limit constants declared | a name in `constants` has no `inline constexpr` declaration (`inline constexpr <type> <name> =`) in a file under the entry's library directory |
| SC06 | limit tests exist | a name in `limitTests` (`Test_<Type>_<Behaviour>`) has no `TEST_CASE("…/<Type>/<Behaviour>")` under the library's `Tests/` or under `Zenith2/Tests/` |
| SC07 | fuzz source and count exist | an entry with a `fuzz.stem` has no `Tests/Fuzz/<Stem>.cpp`, or no `Tests/Fuzz/<Stem>.count` holding one decimal line of at least 1, under its library or tool directory (`Zenith2/Docs/Design/SEC/Fuzzing.md` → "HWASan replay on the S24") |
| SC08 | stem rule | a stem breaks `Zenith2/Docs/Design/SEC/Fuzzing.md` → "Stem naming rule" and the entry's status is not `naming` |
| SC09 | no orphan target | a `Tests/Fuzz/*.cpp` file exists whose stem no entry names |
| SC10 | target buildable | a fuzz source's owning target lacks `win-asan` in `supported` and its stem is not in `declared_deps.json`'s `toolFuzzTargets[]`, and the entry's status is not `dead-target` |
| SC11 | findings are known | an entry has status `gap`, `naming` or `dead-target` but its `finding` field names no row `F1` to `F8` of `Zenith2/Docs/Design/SEC/ParserInventory.md` → "Conformance findings"; or an entry's status says `gap`, `naming` or `dead-target` while SC07, SC08 and SC10 find it conformant (a stale status). A file `Tests/Fuzz/Zenith_Fuzz_<Stem>.cpp` whose `<Stem>` an entry with the known status `naming` names (finding F8, NET) is reported by SC09 under `known`, not as an orphan, until the rename |
| SC12 | exemptions bounded | `covered` or `outside-scope` on an entry of tier 1 or 2; `covered` naming an `id` that does not exist; an `acceptedFindings` value that is not a `Z2-D-NNN` heading of `DecisionLog.md` |
| SC13 | matrix tests exist | a test named in `path_matrix.json` has no `TEST_CASE` (by the SC06 mapping) unless its record carries `pending` with a cross-area row ID |
| SC14 | update keys valid | `Test-Zenith2UpdateKeys.ps1` (through `Read-Zenith2UpdateKeys`) reports any rule of `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "update_keys.json" failed; the file and the script both land at Stage 2, the file with `"keys": []`, so the check is green from the step's first run |
| SC15 | no secret-named configuration key | a configuration key registered in a C++ literal (`Register[A-Za-z]*\(\s*"([A-Za-z0-9_.]+)"`) or present as an object key of a `*.config.json` or `Internal/Config/**/*.json` file has a final dot-separated component that, lower-cased, ends with `token`, `secret`, `password` or `ticket` (so `online.sessionToken` and `net.authToken` are found), and is not in the registry's `configKeyAllow` with a decision ID |
| SC16 | durations complete | `durations.stage2to5` or `durations.stage6` lacks a positive integer for tier 1, 2 or 3; a `maxLen` lies outside 1 to 16,777,216 |

A known finding is an entry whose status and `finding` agree with "Conformance findings"; the step lists it in the report under `known` and does not fail on it, so the step is green while the owners' revisions close the findings and turns red on any new nonconformance.

## Messages

One line per finding, in this form, sorted by check then entry:

```
SecurityConformance: SC05: PI-PLAT-001: constant uVIRTUAL_PATH_MAX_BYTES is not declared under Zenith2/Engine/ZenithPlatform/
SecurityConformance: SC09: Zenith2/Engine/ZenithAudio/Tests/Fuzz/BankFile2.cpp: no inventory entry names stem BankFile2
SecurityConformance: SC14: update keys: OpenWorldSlice 1.0: two active keys
```

The last line is `SecurityConformance: <n> finding(s), <k> known` and the exit code follows.

## Report file

`<Out>/SecurityConformance.json`, `zenith2.security-conformance` version 1:

```json
{
    "schema": "zenith2.security-conformance",
    "schemaVersion": 1,
    "treeSha": "<40 hex>",
    "entries": 137,
    "findings": [ { "check": "SC05", "entry": "PI-PLAT-001", "message": "…" } ],
    "known": [ { "check": "SC10", "entry": "PI-ASSET-006", "finding": "F1" } ],
    "result": "pass"
}
```

`result` is `pass` or `fail`; the file is rewritten on every run.

## Fixtures

`Zenith2/Tools/Gates/Tests/SecurityConformance/` (protected) holds one miniature tree per check: a passing tree (`pass/`) with two libraries, one entry each, their constants, tests, fuzz sources, a three-row Architecture excerpt, a matrix with one row, an update-key record and a declared-dependencies file; and one tree per check (`sc01/` to `sc16/`), each the passing tree with exactly one change that breaks exactly that check; `sc03b/` and `sc03c/` are two more SC03 trees, each with one entry whose `limitsRows` is empty: in `sc03b/` the status has neither `pending-row` nor `limits-in-design`, in `sc03c/` the status is `limits-in-design` but `limitsDesign` is null, and `sc15/`'s key is `online.sessionToken`. `Zenith2/Tools/Gates/Tests/Test-SecurityConformance.ps1` runs the step with `-Root` on each tree and asserts: `pass/` exits 0 with no finding; each `scNN/` exits 1 with exactly one finding, of check `SCNN`; a tree whose only nonconformance is a known finding exits 0 and lists it under `known`; a tree with no `declared_deps.json` and a configure that fails exits 2. The same script then runs the step with `-Root` at the repository itself, over the committed registries, and asserts exit 0, so the first live run is green by construction and a registry edit that would red it fails the fixture script first.

## Gate placement and budget

- The step joins the base `Zenith2` category's gate list from Stage 2, the stage in which the first parsers land, so every ticket runs it (hand-off SEC-P1).
- It is a text scan over the source tree and three JSON files: its timeout is the pending row "Step timeout SecurityConformance" requested from `Zenith2/Docs/Budgets.md` (core-doc row X415), producer "the Stage 2 PROC ticket landing SecurityConformance", in the shape of `Z2-B-PROC-016` (Step timeout ForbiddenCheck).
- Core-doc row X414 adds the step to `Zenith2/Docs/Contracts.md` → "Enforceability matrix" (table "Gate steps") and to the gate-step list of `Zenith2/CLAUDE.md` (a protected edit, inside SEC-P1).

## PROC hand-off

SEC-P1 is one `needs-human` PROC ticket that creates `Zenith2/Tools/Gates/SecurityConformance.ps1`, its fixtures and `Test-SecurityConformance.ps1`, adds the line `pwsh -NoProfile -File Zenith2\Tools\Gates\SecurityConformance.ps1` to the `Zenith2` category's `gates` in `zagent.project.json`, lists the step in `Zenith2/CLAUDE.md`, and runs the fixture assert script in the `Coverage` step's script run; the readers it imports are SEC-P9's. SEC's requirements Z2-R-SEC-033 (the step) and Z2-R-SEC-034 (its fixtures) are realisable only by it, so their leaf is that ticket (ruling BUDGET-KIND-LEAF). Every other SEC requirement that only a protected edit realises names its hand-off as its leaf in the same way:

| Hand-off | Requirements |
|---|---|
| SEC-P2 | Z2-R-SEC-048, Z2-R-SEC-049 |
| SEC-P3 | Z2-R-SEC-050 |
| SEC-P4 | Z2-R-SEC-069, Z2-R-SEC-070 |
| SEC-P5 | Z2-R-SEC-100, Z2-R-SEC-102, Z2-R-SEC-105, Z2-R-SEC-121 |
| SEC-P6 | Z2-R-SEC-103 |
| SEC-P7 | Z2-R-SEC-104, Z2-R-SEC-105 |
| SEC-P8 | Z2-R-SEC-089 |
| SEC-P9 | Z2-R-SEC-086 and Z2-R-SEC-125 (the update-key check), Z2-R-SEC-100, Z2-R-SEC-104, Z2-R-SEC-105, Z2-R-SEC-106 and Z2-R-SEC-122 (the scan logic and its tests) |
