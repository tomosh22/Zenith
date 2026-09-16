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

Verification: **90 Vitest tests, 14 Playwright tests and 1,860 parity cases
passed**. The production web build succeeded, npm audit reported zero vulnerabilities,
all eight documentation checks passed, and the isolated database fence passed seven
checks. Chromium required permission to launch outside the Windows process sandbox;
the permitted rerun passed all browser cases. Coverage is not exhaustive; these results
do not certify the still-pending production cutover.

## Not activated

The real launcher has no `backend.json`, the repository has no root `vault.config.json`,
and the main Git directory has no Vault authority registration. Legacy board/docs
writers remain authoritative. No old source applications or shared services have been
removed.

The plan requires **no ticket In Progress** at cutover. The user directed that epics
must not be In Progress and that the two test tickets should return to To Do. DP-1,
ZM-9, ZM-10, ZM-14, WEB-8 and ATC047-1 were reset to To Do, preserving assignees.
A live database check then confirmed zero tickets In Progress. The four epic resets
used the legacy CLI, WEB-8 used the board UI, and ATC047-1 used an exact-match
administrative transaction with a status history record.

The new Vault mutation engine refuses operations that introduce an epic in
In Progress, including human moves and raw editor saves. Child tickets can still
enter In Progress. Imported legacy epics can be corrected without changing the
frozen compatibility reference.

The user explicitly approved the production write freeze after the initial automatic
approval rejection. The database fence is now applied to legacy board/docs writes;
unrelated shared consumers remain available. A fresh frozen snapshot is being captured.

Live cutover still requires validating that snapshot, retaining the dump, tagging the
pre-cutover SHA, installing the switch in one commit, and watching a small production ticket.
Only then can the old board/docs applications be retired. PostgreSQL and realtime have
other consumers and must not be removed wholesale.

Before the first new claim, true rollback remains possible. After any claim, use forward
recovery; the thaw command refuses a journaled or committed claim even after a revert.
There is no automatic reconciliation from Git back into PostgreSQL.

Existing tick gate semantics are unchanged. The separate `inputsSha256` proposal and
stronger certification of concurrent source changes are not implemented here.
