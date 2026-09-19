# Implementation and cutover status

Updated 2026-09-18.

## Implemented

- Read-only export, scanner, index and 1,860-case legacy reference comparison.
- Shared Git-backed mutation engine, claim attempts, workflow checks, human takeover,
  operation IDs, atomic replacements and recovery after hard process termination.
- Mutation CLI and launcher adapter, including automatic gate inputs and durable scratch.
- Local Markdown editor, preview, wiki links, search, backlinks, board, backlog and roadmap.
- Local project workspace: Board, Backlog, Roadmap, Releases, Reports, Workflow,
  Fields and Board settings sections per project; an issue panel with comments,
  directed links, parent/children family and history; create-issue and
  create-project flows; workflow-backed status select with transition screens.
- Allowlisted `POST /api/actions` mutation surface beside `POST /api/commands`,
  the `--web-root` server flag, 423-LOCKED auto-retry and 409-STALE draft
  recovery in the browser client, guarded INVALID_PARENT diagnostics, and
  config-record fallback modes for projects without metadata exports.
- Isolated migration preparation, full entity audit, database write fence and rehearsal.
- One authoritative checkout with direct integration; no automatic branch switching.

The isolated real-command rehearsal used a copy of ZEN-7: To Do → In Progress → Done,
then reverted the finish and claim commits in that order, restoring To Do. The supplied
244-ticket fixture was unchanged. Hard-kill recovery is tested separately from that
normal rehearsal. The migration audit retained 235 mapped and nine archived tickets,
with all 36 entity tables and 215 columns checked.

Test reports are generated under ignored `artifacts/`; `npm run test:all` rebuilds the
web app, runs Vitest and Playwright, then checks the frozen reference. The separate
database fence test uses a disposable clone of the isolated snapshot database. It
does not freeze the live database.

Verification (observed 2026-09-18): **680 Vitest tests in 61 files, 30 Playwright
tests and 1,860 parity cases passed**, and the production web build succeeded.
Coverage measured statements 84.24, branches 72.75, functions 83.70 and lines
88.92 against the configured 76/72/63/80 thresholds. Branch coverage clears its
threshold by well under a point, so a change that adds branches without tests
reds the gate rather than drifting quietly. Coverage is not exhaustive; these
results are supplemented by the production verification below.

## Production cutover

The launcher now dispatches straight to the local Vault — `Tools/zagent/zagent.ps1`
execs the Vault CLI unconditionally, with no backend-selection file or concept.
The root `vault.config.json` enables Git-backed writes in the authoritative checkout.
Legacy board/docs database writes are fenced; shared services remain available.

The six formerly In Progress items (DP-1, ZM-9, ZM-10, ZM-14, WEB-8 and
ATC047-1) were reset to To Do with assignees preserved. The final frozen snapshot
contains 244 tickets, zero In Progress, 157 activity rows and 196 links. Its
1,860 read-command comparisons pass and the 36-table entity audit has no differences.
Migration preserved 235 mapped tickets, nine read-only archived tickets, and
68 existing living documents without overwriting them.

The backup is retained under `.zagent/cutover-20260916/upstream.dump`; its SHA-256
and the snapshot/freeze evidence are recorded in `migration/`. The pre-cutover
recovery tag is `vault-pre-cutover-20260916` at `44f4f7a9`.

The mutation engine rejects new In Progress epics, including raw editor saves
and type conversions. Archived board roots refuse writes. All mutable project
and category policy uses direct integration, including DevilsPlayground.

Production verification ticket ZEN-23 completed Done through the real launcher.
The app observed its claim and saved a note in commit `7ce66504`; CLI history
reported that save. The real verify runner passed the suite current at that
cutover (93 Vitest tests, 14 Playwright tests), 1,860 parity cases and all eight
documentation checks, with no skipped or deferred gates. Finish committed as `13a5a53c`. The completed checklist is recorded
in `769911ca`. Recovery inspection found no pending operations.

The first production claim has occurred: recovery is now forward-only. The thaw
command refuses a journaled or committed claim even after a revert.
There is no automatic reconciliation from Git back into PostgreSQL.

Existing tick gate semantics are unchanged. The separate `inputsSha256` proposal and
stronger certification of concurrent source changes are not implemented here.

The production UI check also corrected the status picker: it now uses the selected
project's workflow catalog and excludes labels and unrelated project statuses.
That phase's final Vitest regression run passed 94 tests; the current counts are
reported above.

Local Jira and Notion listeners (3001/3004) are stopped. Retained web, Figma,
Mermaid, realtime and PostgreSQL listeners remain present (3000/3002/3003/3005/5432).
The old repository disables normal startup/deploy paths and retains source and shared
schemas for historical audit and other consumers. Remote Fly deployment state was
not verified or changed. See migration/completion.json for final local checks.
