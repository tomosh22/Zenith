# Zenith Vault

A local Markdown board and document editor with a local project workspace, a
disposable index, compatible CLI, Git-backed mutations, and a localhost browser
app. PostgreSQL and the previous board application's checkout are not runtime dependencies.

**Deployment status:** the production launcher uses the local Vault —
`Tools/zagent/zagent.ps1` dispatches straight to it, unconditionally; there is
no backend-selection file or concept. Legacy board/docs writes are fenced in PostgreSQL.
The pre-cutover recovery tag is `vault-pre-cutover-20260916`; migration evidence is in
the repository root `migration/` directory. The frozen test fixture stays read-only.

Start the production app from the repository root:

```powershell
node Tools/Vault/server/http.mjs --vault C:/dev/Zenith --port 4317
```

Open http://127.0.0.1:4317. The CLI remains `Tools/zagent/zagent.ps1`.

## Project workspace

The browser app serves a project workspace beside the document editor: per-project
Board, Backlog, Roadmap, Releases, Reports, Workflow, Fields, and Board settings
sections, plus an issue panel with comments, directed links, parent/children
family, and history. Issue and project creation, sprint and version controls,
and workflow/field/board settings all submit through the same operation engine.
Writes go through `POST /api/commands` and the allowlisted `POST /api/actions`
surface; both enforce the per-launch CSRF token, opened revisions, read-only
projects, and Git-backed atomic commits. The browser client retries 423 LOCKED
writes automatically and recovers a 409 STALE response by reloading the issue
while preserving the draft. `--web-root` selects the served web bundle
(`web/dist` by default; pass another directory for a different build). The API
contract is in `spec/project-ui-api.md`; tracked UI gaps are in
`spec/jira-ui-parity.md`.

## Run

Requires Node 22 or later. From this directory:

```powershell
npm ci --ignore-scripts
npm test
npm run test:e2e
npm run test:coverage
npm run parity
npm run build
npm run serve -- --vault fixtures/snapshot/vault --port 4317
node cli/zagent.mjs --vault fixtures/snapshot/vault queue --project FD --json
node cli/zagent.mjs --vault fixtures/snapshot/vault show FD-18 --json
```

The explicit `--vault` argument selects the authority. Writes require a Git checkout with
an initial commit and `writes.enabled: true` in `vault.config.json`. Keep the captured
fixture read-only; use a disposable copy for experimentation. `cli/client.mjs` is the
launcher adapter and writes the legacy `.zagent` result and ticket scratch files.

## Testing

`npm test` runs Vitest checks for the scanner, index, watcher, command logic,
export preservation, HTTP API, and durable operations. `npm run test:e2e` runs
Playwright CLI process tests and Chromium browser tests against isolated vault copies.
Install the matching browser once with `npx playwright install chromium`.

`npm run test:coverage` records runtime coverage. Test reports and coverage
files live under the ignored `artifacts/` directory. The frozen parity suite
remains a separate check against the captured legacy responses.
`npm run test:all` runs Vitest, Playwright, and the independent parity check.

The suite includes all 1,860 frozen parity cases plus tests that edit files,
invalidate and restore required supporting files, and verify refused mutations
preserve vault bytes. Coverage includes CLI, server, and JavaScript scripts;
it is not exhaustive, and database snapshot/capture tooling still requires the
separate reproduction procedure below.

`server/text-edit.mjs` preserves untouched bytes when changing fields or bodies.
Tests check byte-identical no-op saves and single-line status edits across all
244 exported tickets. Separate process tests forcibly terminate writers during
multi-file replacement and after commit, then verify recovery.

## Editing and recovery

The app provides a Markdown source editor, sanitized preview, search, wiki links,
backlinks, and board queries. Saves use the same operation engine as CLI mutations.
The server listens on loopback, checks origins, and requires a per-launch token for
browser writes. Document reads are restricted to indexed Markdown.

Managed writers share a Git reference lock, including callers from other worktrees.
The first mutable checkout registers its authority under the common Git directory;
equivalent locations in other worktrees resolve there. An explicit nested fixture
remains isolated. A busy writer refuses the operation; retry after it completes.

Each operation journals its ID, request fingerprint, before/after hashes and bytes,
result, and commit. It validates the proposed graph before replacing files and commits
only its changed paths, preserving unrelated staged and unstaged files. Saving a target
commits the complete validated target file, including external edits that were reloaded.
Unmanaged editors do not participate in the lock; observed revision changes refuse a
save, but this is not an OS-level transaction against arbitrary external writers.

Inspect incomplete operations without changing files:

```powershell
node scripts/recover.mjs --vault ROOT
node scripts/recover.mjs --vault ROOT --apply
```

Recovery accepts only a journaled before or after state. A third-party edit causes a
conflict and remains untouched. Do not delete the journal or force an overwrite to
bypass a conflict. Preserve the files and inspect the recorded operation first.

Agent claims persist an attempt ID. Subsequent agent changes must still own that
attempt; human takeover prevents stale close-out. Comment-only abandonment preserves
the human's assignee and status. Workflow transitions are checked for both CLI changes
and raw editor saves.
Epics cannot be moved into In Progress; claim and progress apply to their child
tickets. Existing imported epic statuses can be corrected without rewriting the
frozen reference fixture.

## Files and authority

- `fixtures/snapshot/vault/`: isolated Markdown export. Production cutover requires a
  fresh frozen capture; do not install this historical fixture as the live board.
- Tickets carry editable canonical fields and the original description as their Markdown
  body. `source` retains original identifiers and fields not yet given a presentation.
  Canonical fields take precedence over their historical source counterparts.
- Sprint and configuration catalogs are Markdown with YAML frontmatter. `metadata.md`
  retains supporting relational records, including comments and knowledge databases;
  SELECT option IDs and definitions are retained together. Unused legacy-project catalogs
  are archived there, rather than silently discarded.
- `history/pre-migration.md` preserves original activity rows. After import, committed
  changes supply new history, including external commits. Git cannot reconstruct intermediate external edits that
  were never committed. Frozen pre-migration history is never rewritten.
- The index is disposable. Invalid structured data makes the repo-wide claim check fail
  closed; reading files does not repair or rewrite them.

## Evidence

The captured board contains 244 issues, including nine issues in legacy test projects
outside the six projects configured by Zenith. These are retained; expected configuration
errors are part of the reference set. `reference.json` records legacy dispatcher payloads
and exit codes. Suggested scratch writes returned by the dispatcher are never applied.

`entity-digests.json` hashes each complete captured entity set. The export audit reconstructs
all 36 tables from Markdown and verifies that no row or persisted value was lost.
`provenance.json` records the actual legacy source hashes, source commit, pinned policy,
and reference hash. Authentication tables, passwords, and tokens are not exported.

Parity preserves IDs, timestamps, and payload contents. The queue's declared deterministic
tie-break is handled explicitly; arbitrary array sorting or dropping fields is not allowed.
Read commands calculate their answers from the files, not from `reference.json`.
`ordering.json` records only source IDs in the order observed by the old dispatcher.
Its link query has no explicit order, and history timestamps can tie. The exporter
retains these ordinals in metadata so compatibility does not depend on database scan
order. Edited and removed links are still derived from canonical ticket fields;
history values come from the frozen history files.

The legacy ticket-contract and routing functions are vendored as pure JavaScript under
`cli/legacy-pure/`. Their source hashes and extraction method are recorded there. To
refresh them deliberately, run `node scripts/vendor-read-helpers.mjs LEGACY_ROOT`, then
run the tests and frozen parity suite. No legacy checkout is imported by the CLI.

## Local documents

To include ordinary Markdown documents, add explicit entries to `supportingFiles` in
the selected vault's `vault.config.json`, for example
`{"path":"Docs/Roadmap.md","kind":"document"}`. Paths must stay inside that vault.
`docs ls`, `docs read`, `docs search`, and `docs status` read these files directly.
`board status --project FD --roadmap Docs/Roadmap.md` compares their ticket references
with the index. These local commands replace the old uploaded document mirror.

## Reproduce the capture

This is development tooling, separate from the runtime. It requires the previous
board application's checkout, its existing dependencies, Docker, and an isolated
PostgreSQL instance.

1. Run `scripts/snapshot.mjs SOURCE_CONTAINER zenith-vault-snapshot-NAME OUTPUT.dump SOURCE_ROLE`
   (or set `VAULT_SNAPSHOT_ROLE` instead of passing the fourth argument), naming the
   source Postgres role and database to dump. It runs only `pg_dump` against the source
   and restores into a new container on `127.0.0.1:55439`; it never restores over the
   source. Keep the dump outside tracked files (the completed run used `.zagent/vault/`
   in the repository root).
2. The snapshot script creates the local development account `vault_reader`, grants SELECT
   on `public`, `jira`, and `notion`, and enables `default_transaction_read_only`. Both capture
   and extraction require this account, database `vault_snapshot`, and the isolated port.
   Set `DATABASE_URL` to `postgresql://vault_reader:vault-local-readonly@127.0.0.1:55439/vault_snapshot`.
   This fixed credential is only for the loopback-bound disposable snapshot, never production.
3. With the legacy repository's `tsx` launcher, run
   `scripts/capture.ts LEGACY_ROOT NEW_FIXTURE_ROOT CLIENT_ROOT`.
4. Run `scripts/extract.mjs SOURCE_JSON NEW_FIXTURE_ROOT/ordering.json`, then
   `scripts/export.mjs SOURCE_JSON NEW_FIXTURE_ROOT/vault NEW_FIXTURE_ROOT/config.json`.
   Export refuses an existing destination so it cannot overwrite human edits.
5. Run `scripts/audit-export.mjs NEW_FIXTURE_ROOT/vault SOURCE_JSON NEW_FIXTURE_ROOT/entity-digests.json`
   and `scripts/provenance.mjs LEGACY_ROOT SOURCE_JSON NEW_FIXTURE_ROOT`.
6. Run `scripts/parity.mjs NEW_FIXTURE_ROOT`. Review any difference before changing the
   reference or normalization policy.

## Cutover boundary

`scripts/migrate.mjs` prepares a new isolated destination; `scripts/rehearse.mjs`
exercises claim, finish, journal recovery, and forward recovery in a temporary Git
checkout. Neither command switches the production launcher. Run either with `--help`
for its current arguments.

Live cutover requires all legacy board/docs writers frozen, a retained fresh dump,
matching capture/export audit and read parity, a tagged pre-cutover SHA, and one
reviewable writer-switch commit. The plan also requires no tickets In Progress;
relaxing that condition requires an explicit migration decision. Choose the first
small watched ticket before activation.

`scripts/freeze-legacy.mjs` provides a database write fence for board/docs tables and
requires the same `VAULT_SNAPSHOT_ROLE` environment variable as the capture script.
Its isolated test verifies that unrelated Figma writes remain available. PostgreSQL,
realtime, and other applications from the previous board's platform have shared
consumers: retiring board/docs does not authorize deleting the entire database or
stopping those shared services.

Before the first new claim, rollback can restore the old backend and unfreeze it.
After the first claim, use forward recovery; there is no automatic reconciliation
back into PostgreSQL. Preserve the dump and old implementation until the watched
ticket and cutover checks have passed.

The write design is in `spec/write-design.md`. The chosen integration model uses one
authoritative checkout without automatic feature-branch switching or merging. Existing
gate verdict semantics are unchanged; this work does not add an `inputsSha256`
certification guarantee.
