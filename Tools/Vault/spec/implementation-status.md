# Implementation and cutover status

Updated 2026-09-16.

## Implemented

- Read-only export, scanner, index and 1,860-case legacy reference comparison.
- Shared Git-backed mutation engine, claim attempts, workflow checks, human takeover,
  operation IDs, atomic replacements and recovery after hard process termination.
- Mutation CLI and launcher adapter, including automatic gate inputs and durable scratch.
- Local Markdown editor, preview, wiki links, search, backlinks, board, backlog and roadmap.
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

Verification: **94 Vitest tests, 14 Playwright tests and 1,860 parity cases
passed**. The production web build succeeded, npm audit reported zero vulnerabilities,
all eight documentation checks passed, and the isolated database fence passed seven
checks. Chromium required permission to launch outside the Windows process sandbox;
the permitted rerun passed all browser cases. Coverage is not exhaustive; these results
are supplemented by the production verification below.

## Production cutover

The launcher now selects the local Vault through `Tools/zagent/backend.json`.
The root `vault.config.json` enables Git-backed writes in the authoritative checkout.
Legacy board/docs database writes are fenced; shared services remain available.

The six formerly In Progress items (DP-1, ZM-9, ZM-10, ZM-14, WEB-8 and
ATC047-1) were reset to To Do with assignees preserved. The final frozen snapshot
contains 244 tickets, zero In Progress, 157 activity rows and 196 links. Its
1,860 read-command comparisons pass and the 36-table entity audit has no differences.
Migration preserved 235 mapped tickets, nine read-only archived tickets, and
68 existing living documents without overwriting them.

The backup is retained under `.zagent/cutover-20260916/saas.dump`; its SHA-256
and the snapshot/freeze evidence are recorded in `migration/`. The pre-cutover
recovery tag is `vault-pre-cutover-20260916` at `44f4f7a9`.

The mutation engine rejects new In Progress epics, including raw editor saves
and type conversions. Archived board roots refuse writes. All mutable project
and category policy uses direct integration, including DevilsPlayground.

Production verification ticket ZEN-23 completed Done through the real launcher.
The app observed its claim and saved a note in commit `7ce66504`; CLI history
reported that save. The real verify runner passed 93 Vitest tests, 14 Playwright
tests, 1,860 parity cases and all eight documentation checks, with no skipped or
deferred gates. Finish committed as `13a5a53c`. The completed checklist is recorded
in `769911ca`. Recovery inspection found no pending operations.

The first production claim has occurred: recovery is now forward-only. The thaw
command refuses a journaled or committed claim even after a revert.
There is no automatic reconciliation from Git back into PostgreSQL.

Existing tick gate semantics are unchanged. The separate `inputsSha256` proposal and
stronger certification of concurrent source changes are not implemented here.

The production UI check also corrected the status picker: it now uses the selected
project's workflow catalog and excludes labels and unrelated project statuses.
The final Vitest regression run passed 94 tests.
