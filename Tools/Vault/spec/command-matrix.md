# Vault command matrix

This matrix is the Stage 1 read contract for the command names the CLI
accepts, and records the current local Vault behavior. It was originally kept
in step with `Tools/zagent/contract.json`, the old HTTP wire contract; that
file was removed along with the legacy remote backend it described, and this
matrix is now the complete, offline specification on its own — every command
and subcommand appears below even when its old transport has been removed or
its mutation is guarded by the local write protocol.

## Status vocabulary

| Status | Meaning in the prototype |
| --- | --- |
| `retained-read` | A read can be answered from the scanned Markdown vault and local config. It must not write the vault. |
| `retained-read-local` | The command name is retained, but a transport or database concept is replaced by a local derived result. |
| `conditional-write` | A local Markdown mutation is implemented. It runs only when `vault.config.json:writes.enabled` is `true`; otherwise the dispatcher returns an explicit refusal and leaves every byte unchanged. Enabled writes use the operation engine's fresh scan, revision checks, graph validation, atomic replacement and optional commit. |
| `removed-transport` | The old bearer token, HTTP, database provisioning, Notion sync, or contract endpoint has no vault equivalent. Stage 1 must report an explicit unsupported command. |

`retained-read` and `retained-read-local` are the commands executed by a
read-only dispatcher. `conditional-write` commands share the same scanner and
result vocabulary, but require explicit opt-in through `writes.enabled`.
The result shapes below are JSON-friendly object shapes; human output may
render the same fields as a table. A scanner error is repo-wide for board
reads and writes: do not return a partial queue or silently skip a malformed
file.

## Source and result conventions

The source of truth for field names is
[`field-inventory.json`](./field-inventory.json). The current snapshot
export has two representations that readers must understand:

* A ticket file contains a small canonical frontmatter projection, a Markdown
  body, and `source` with the remaining raw `jira.issues` row. For a non-null
  description, the source row omits `description` and the body contains its
  UTF-8 bytes; for a null description, `source.description` remains `null`.
* `metadata.md` contains the source-preserving relational tables and
  `archivedTables` for rows whose project is outside the mapped board roots.
  The current exporter keeps `jira.comments` and all Notion knowledge rows
  there; it does not yet emit per-board `knowledge/*.md` files or comment
  sentinels. The ergonomic paths in the inventory are the target layout, so a
  reader must accept both the current metadata representation and that future
  layout.

Mutation commands patch the canonical frontmatter/body or source-preserving
catalog rows through `server/operations.mjs`. They load a fresh index under
the common repository lock, validate the complete proposed graph, and carry an
operation id plus expected file revisions. Unknown frontmatter and source
columns remain intact. A vault with `writes.enabled` absent or false is still
read-only; the refusal is a normal command result and does not create scratch
or journal files.

### Exact legacy JSON result shapes

These are the payload shapes captured in
`fixtures/snapshot/reference.json` and defined by the corresponding
interfaces in the retired upstream board application's `packages/agent/src`
(paths in this document are relative to that repository's root). A local
implementation may change where it reads a value, but it must keep these keys
and nesting for the retained command. Fields shown as nullable remain present
with `null`.

* **show** returns the legacy `TicketPayload`: `{key, id, number, title, type,
  priority, complexity, risk, category, labels, humanGate, needsHuman,
  needsGpu, deferred, sprint, reporterId, routing:{model,effort},
  contractValid, contractError, reachabilityWarning, boardRefWarning,
  markerWarning, gates, gatesSource, conventions, cleanup, reviewOn,
  review:{required,reasons}, workerTools, repo, baseBranch, branching,
  branch, push, projectKey, orgSlug, issueUrl, bodyPath, guardrails,
  goal, definitionOfDone:[{text,done}], citations:{paths,symbols,lines},
  body, status}`. `check` adds `runnable` and
  `check:{queue:{would,reason},targeted:{would,reason},inFlight}`.
* **owns** returns exactly `{key, owned}`. Do not add assignee or email
  fields to this payload.
* **gates** returns exactly `{key, category, gatesSource, gates, base,
  unioned, changed}`. `unioned` contains `{category,matched,gates}` rows;
  `changed` contains the paths that selected the union. With no diff,
  `unioned` and `changed` are empty arrays.
* **queue** returns rows shaped as `{key,title,status,priority,complexity,
  risk,category,type,sprint:{name,status,continuous,inScope},assigned,
  isEpic,parent,outOfScopeSprint,needsHuman,needsGpu,deferred,humanGate,
  blockedBy,claimable}`. The whole `sprint` value is `null` when the issue
  has no sprint. `blockedBy` is a string key array.
* **links** returns `LinkRow[]` with exactly
  `{id,type,direction,label,key,title,status,statusCategory,reason}`. The
  source ticket owns a directed edge; reverse/incoming rows are derived at
  read time.
* **transitions** returns `{key,status,transitions:[{toStatus,name,
  blockedReason}]}`. The retained vault graph supplies this from
  `workflow.md`; it does not expose dropped rule/permission/screen rows.
* **blocked** returns `{key,title,status,blockedBy,reasons}` rows, with the
  two arrays positionally aligned.
* **epic** returns `{key,title,status,total,done,points,donePoints,children}`;
  each child is `{key,title,type,status,statusCategory,storyPoints,blockedBy}`.
* **sprint list** returns `{id,name,goal,status,startDate,endDate,issues,
  points,donePoints,gate}` rows; `gate` is `null` or
  `{key,title,status,unmet}`. The legacy payload intentionally omits slug,
  position and continuous from each result row.
* **version list** returns `{id,name,description,status,releaseDate,issues,
  done,points,donePoints}` rows. The legacy payload intentionally omits
  version slug/start date/position.
* **history** returns `{field,fromValue,toValue,createdAt,actor}[]`, newest
  first, with no synthetic id or source key.
* **report velocity** returns `{rows:[{sprint,committed,completed}],average}`.
  **report epic** returns `{key,title,total,done,points,donePoints,percent}[]`.
* **rows** and **suggestions** return `{id,url,values}` rows, where `values`
  is the exact property-name map for the selected knowledge database. The
  property sets are listed below. `url` is a source URL in the legacy fixture;
  a local file implementation should preserve the key and use its stable
  row path only when the local contract explicitly defines that replacement.

No extra convenience fields should be added to these JSON payloads without a
reviewed contract change. Internal indexes may carry more provenance than
the result object.

### Mapping rules for result fields

| Result field | Persisted source |
| --- | --- |
| `show`/`check` ticket fields | Canonical ticket frontmatter/body plus `source` raw row, workflow, links, labels, versions, identities and local config. Return the exact legacy keys above; source-only values feed the payload fields that the legacy board actually exposes. |
| `queue` row fields | Canonical frontmatter plus workflow status category, sprint file, relation index and marker labels. Return the exact legacy row keys above. |
| `links`/`blocked`/`epic` | `metadata.md` relation rows and ticket projections; resolve target keys/status/category through the ticket/workflow index. |
| `transitions` | `<boardRoot>/workflow.md` statuses/transitions; return only `toStatus`, `name`, `blockedReason`. |
| `sprint list`/`version list`/reports | Sprint/version catalog files and ticket membership/status indexes; aggregate with the same EPIC exclusion and DONE category rules as the legacy implementation. |
| `history` | Frozen `<boardRoot>/history/pre-migration.md` entries or source-preserving `metadata.md` activity rows; resolve actor display names. Return the exact five legacy fields. |
| `rows`/`suggestions` | Current `metadata.md` Notion rows (or future knowledge files), fixed database property specs and option maps. Return exact `{id,url,values}` rows. |
| body sections | Markdown body parsed as UTF-8 with LF for parsing; bytes are never rewritten by a read. |
| gate/routing fields | `vault.config.json:policy` and `projects[]/categories[]`, plus workflow/status and ticket markers. |

### Source anchors and CLI option mapping

The result names above are pinned to the old implementation at these source
anchors. The vault reader may replace the query, but should not invent a
second result vocabulary:

| Surface | Source anchor |
| --- | --- |
| show/check/queue/owns/gates | `packages/agent/src/board.ts` (`showTicket`, `queue`, `checkClaim`, `GateSelection`) and `dispatch.ts` cases `show`, `check`, `queue`, `owns`, `gates` |
| links/blocked/epic/sprint/version/history/reports | `packages/agent/src/agile.ts` (`listLinks`, `listBlocked`, `showEpic`, `listSprints`, `listVersions`, `ticketHistory`, `velocity`, `epicProgress`) |
| link display labels | `packages/lib/src/constants.ts`: `JIRA_LINK_LABELS` (`blocks`, `relates to`, `duplicates`, `causes`) and `JIRA_LINK_INVERSE_LABELS` (`is blocked by`, `relates to`, `is duplicated by`, `is caused by`) |
| knowledge rows/options | `packages/agent/src/notion.ts` (`DATABASE_KEYS`, `DATABASES`, `listRows`, `optionId`) and `dispatch.ts` cases `rows`/`suggestions` |
| docs reads | `packages/agent/src/docs.ts` and `dispatch.ts` `docs()` (`ls`, `read`, `search`); the vault uses local Markdown paths in place of Notion page ids |
| flag vocabulary | `packages/agent/src/flags.ts` (`COMMAND_FLAGS`, `GLOBAL_FLAGS`) and `args.ts` (`parseArgs`, `whereFrom`, enum/date/points helpers) |

Every invocation accepts the global `--json` selector. The following mapping
is the closed option vocabulary from `flags.ts`; options not listed for a
command are usage errors. The option names remain pinned to the archived
Stage 1 contract so the writer cannot silently reinterpret them:

| Option family | Commands | Meaning in the retained read model |
| --- | --- | --- |
| `--project KEY` | `next`, `claim`, `show`, `check`, `queue`, `owns`, `blocked`, `report`, `board`, `sprint`, `version`, `rows`, `suggestions`, knowledge writers | Select a configured board/project; for reads, filter records and resolve project-local catalogs. `show`/`check`/`owns` retain the flag for client compatibility; the key remains authoritative. The legacy `docs --project` spelling is rejected by local docs reads. |
| `--category NAME` | `next`, `create`, knowledge writers | Select a configured category/board overlay. Create/knowledge mutations use it when writes are enabled; local docs reads do not accept the old category flag. |
| `--file PATH` / `--text TEXT` | `comment`, `create`, `gates`, `guard`, `ask`, `decide`, `shortfall`, `changelog`, `suggest` | Legacy file/body or changed-path input. Read commands accept only their explicit changed-path form; conditional writers consume their body input under the operation protocol when enabled. The old `docs write --file/--text` transport is removed. |
| `--where FIELD=VALUE` | `rows` | Exact, case-insensitive property-name/value filter after select ids resolve to labels; comma-separated clauses are ANDed. |
| `--limit N` | `history`, `rows`, `suggestions` | Non-negative row cap; history defaults to 50. Local `docs search --limit` is rejected; local search uses its positional query and configured files. |
| `--all` | `suggestions` | Include every retained suggestion row; without it filter `Status=new`. |
| `--roadmap PATH` | `board status` | Choose the local roadmap document used for drift comparison. |
| `--depth N`, `--archived` | `docs ls` | Legacy Notion traversal flags; local `docs ls` rejects them. Use the configured vault root and local positional scope. |
| `--recursive`, `--live`, `--out PATH` | `docs read` | Legacy child-page/live/upload flags; local `docs read` rejects them because there is no remote page transport. |
| `--path PATH`, `--titles`, `--deep` | `docs search` | Legacy Notion search flags; local `docs search` rejects them and searches the local Markdown record selected by its positional query. |
| `--page ID`, `--append`, `--icon VALUE`, `--dry-run`, `--force`, `--no-verify` | `docs write`/`docs sync` | Notion/write transport or local mutation controls. The remote/document commands remain removed and must refuse without modifying files. |
| `--goal TEXT`, `--start DATE`, `--end DATE`, `--force` | `sprint` subcommands | Sprint create/start/complete inputs. Sprint mutations run conditionally under `writes.enabled`; `--force` is required to complete a sprint with unfinished tickets. |
| `--description TEXT` | `version create` | Version creation input for the conditional local catalog writer. |
| `--reason TEXT` | `link`, `unlink`, `rows ... resolve`, `suggestions resolve` | Required provenance for BLOCKS links and row resolution. These mutations run conditionally under `writes.enabled`. |
| Ticket mutation flags | `create`: `--project`, `--title`, `--file`/`--text`, `--type`, `--priority`, `--complexity`, `--risk`, `--category`, `--status`, `--parent`, `--assignee`, `--label`, `--points`, `--severity`, `--repro`, `--environment`, `--due`, `--flag`, `--blocks`, `--blocked-by`, `--reason`, `--fix-version`, `--sprint`; `update`: `--label`, `--unlabel`, `--points`, `--clear`, `--resolution`, `--title`, `--type`, `--priority`, `--complexity`, `--risk`, `--severity`, `--repro`, `--environment`, `--due`, `--assignee`, `--body`, `--goal`, `--dod`; `estimate`: `--points` or positional points; `flag`: `--clear`; `resolve`: `--resolution`, `--clear`; `finish`: `--status`, `--comment`, `--worklog` | Persisted ticket fields or mutation controls. Their enum vocabularies and null/clear behavior are in `field-inventory.json`; conditional writers validate and apply them only when writes are enabled. |
| Mutation protocol flags | All conditional writers | `--attempt`/`--claim-attempt` identifies the durable claim owner, `--actor`/`--actor-email` selects the writer, `--human` authorizes an explicit human takeover or closeout, and `--operation-id`/`--op-id` retries one operation. These flags are stripped from the archived read flag validator and handled by the local writer. |

## Complete contract matrix

The `Command` column uses the existing CLI spelling. Flags are shown where
they affect read semantics or file handling. All other flags are listed in
the contract inventory. Mutation entries below use `conditional-write` when
the local writer implements them; the legacy read-only behavior remains
available whenever `writes.enabled` is false.

| Command | Status | Read result / persisted mapping | Current local behavior |
| --- | --- | --- | --- |
| `help` | `retained-read-local` | Usage text generated from this matrix and the contract command list. | Print local vault usage; do not advertise HTTP or token setup as required. |
| `auth mint --name <NAME>` | `removed-transport` | None; `agent.agent_tokens` is dropped. | Explicit unsupported response; no token is created. |
| `auth list` | `removed-transport` | None; bearer token listings are not persisted. | Explicit unsupported response. |
| `auth revoke <NAME>` | `removed-transport` | None; bearer token revocation is not a vault operation. | Explicit unsupported response. |
| `provision <PROJECT_KEY>` | `removed-transport` | None; remote Jira project/user/config provisioning is removed. | Explicit unsupported response; config must be edited as a local file in a future write stage. |
| `doctor` | `retained-read-local` | `{ok, diagnostics, projects, ticketCount, malformedCount, duplicateKeys, danglingReferences}` from scanner, config, workflow, sprint/version/label and source indexes. | Cold-scan all configured roots and supporting files. Report all errors; claims remain blocked on any error. |
| `next [--project KEY] [--category NAME]` | `conditional-write` | Claims the first eligible `QueueRow`, records the assignee and a claim attempt, and returns the full ticket payload. | Under `writes.enabled`, select and claim under the operation lock; otherwise refuse without changing files. The read-only equivalent is `queue`. |
| `claim <KEY>` | `conditional-write` | Claims one ticket, records the assignee and a claim attempt, and returns the full ticket payload. | Validate the fresh repository-wide index and shared claim decision before the atomic write; refuse invalid, stale, owned, or disabled writes without changing files. |
| `show <KEY>` | `retained-read` | `TicketPayload` from ticket frontmatter/body, workflow, labels, versions, links, identity/config indexes. | Resolve key repo-wide and return the full payload. An unmapped project is a config error; missing/dangling data is a scan error. |
| `check <KEY>` | `retained-read-local` | Exact show payload plus `runnable` and `check:{queue:{would,reason},targeted:{would,reason},inFlight}`. | Evaluate whether a future claim would be allowed without writing ownership or status. Preserve the accepted queue key tie-break. |
| `queue [--project KEY] [--category NAME]` | `retained-read` | `QueueRow[]`, ordered by declared priority order, numeric `position`, then key. | Return all tickets in the selected roots; include nonclaimable rows with reasons so the output explains filtering. |
| `owns <KEY>` | `retained-read-local` | Exactly `{key, owned}`. | Compare the resolved assignee to `policy.agentEmail`/`agentEmails`; no database identity lookup or write. |
| `comment <KEY> --file <PATH>` | `conditional-write` | Appends a comment to the ticket body while preserving the existing owner/status state. | Read the supplied body input and append it atomically under `writes.enabled`; this recovery path remains available after an agent loses ownership. |
| `comment <KEY> --text <TEXT>` | `conditional-write` | Same as the file form. | Apply the text through the operation protocol when enabled; disabled vaults refuse without changing files. |
| `move <KEY> <STATUS>` | `conditional-write` | Changes ticket status and durable `updatedAt`, subject to ownership, attempt and workflow checks. | Apply only an allowed workflow transition. Agent writes require current ownership and claim attempt; human actor may take over explicitly. |
| `finish <KEY> [--status STATUS] [--comment TEXT] [--worklog TEXT]` | `conditional-write` | Moves/statuses a ticket and appends comment/work log in one operation. `human-gate` tickets stop at `In Review` for an agent; a human actor may close them. | Require both evidence bodies, current ownership/attempt for agents, and a valid workflow transition before the atomic write. |
| `integrate <KEY>` | `conditional-write` | Lane-mode only. Merges `refs/heads/agent/<KEY>` onto the ticket's project `baseBranch` and syncs the main checkout; never edits a board file, so it carries no ticket-frontmatter mapping. See the `### integrate` section below for the full result/exit ladder. | Read-only refusals and freshness/gate-verdict classification run before any lock; the merge itself runs under `refs/vault/operation-lock`, the same lock board writes use, via `server/operations.mjs`'s `executeIntegrate`. |
| `gates <KEY>` | `retained-read-local` | Exactly `{key, category, gatesSource, gates, base, unioned, changed}`. `unioned` rows are `{category,matched,gates}` and `changed` is a path array. | Return configured gate commands and selection explanation. `base` is the category/base gate list; changed paths may add category unions. |
| `guard [<KEY>]` | `retained-read-local` | `{protectedPaths, gates, reviewOn, violations, needsHuman, needsGpu, humanGate}` from policy and optional ticket. | Inspect the working-tree path list if available and report protected-path matches; never alter files. |
| `create ...` | `conditional-write` | Creates a ticket file and optional links/labels. File flags include `--file`, `--comment`, `--worklog`, `--body`, `--goal`, `--dod`; `--blocks`/`--blocked-by` require `--reason`. | Allocate the key, stable id and path from the fresh locked index, validate all references, then create the file atomically when enabled. |
| `link <KEY> blocks\|blocked-by\|relates\|duplicates\|causes <KEY> [--reason TEXT]` | `conditional-write` | Appends a source-owned `jira.issue_links` edge. | Validate both ticket references and the `BLOCKS` reason requirement, then apply the source edge atomically when enabled. |
| `unlink <KEY> <VERB> <KEY>` | `conditional-write` | Removes one directed link. | Resolve the source-owned edge and remove it under the operation protocol when enabled. |
| `links <KEY>` | `retained-read` | Exact `{id,type,direction,label,key,title,status,statusCategory,reason}[]` for outgoing/incoming issue links. | Read source-owned link entries and derive reverse direction; include finished blockers because links are historical relationships. |
| `transitions <KEY>` | `retained-read-local` | Exact `{key,status,transitions:[{toStatus,name,blockedReason}]}` from project `workflow.md`. | Return graph edges from the current status. Do not consult dropped permissions/rules/screens; no remote evaluator exists. |
| `blocked [--project KEY]` | `retained-read` | `BlockedRow[]`: `{key,title,status,blockedBy:[KEY],reasons}` for tickets with unfinished incoming `BLOCKS` edges. | Resolve blocker status category through workflow; a blocker is clear only in a `DONE` category. |
| `epic <KEY>` | `retained-read` | `{key,title,status,total,done,points,donePoints,children:[...]}`; children are tickets whose `parent` resolves to the epic. | Return direct children and aggregate progress. A non-epic key is an explicit error. |
| `parent <KEY> <PARENT_KEY\|none>` | `conditional-write` | Changes the canonical parent reference. | Validate the parent is an EPIC, reject self/cross-repository references, and patch the ticket atomically when enabled. |
| `sprint list [--project KEY]` | `retained-read` | Exact `{id,name,goal,status,startDate,endDate,issues,points,donePoints,gate}` rows, ordered by source position/name; `gate` is null or `{key,title,status,unmet}`. | Read sprint files and derive issue/point aggregates. `sprint --goal` is inline text only for future create; it is not a file flag. |
| `sprint create <NAME> [--goal TEXT] [--continuous]` | `conditional-write` | Creates a sprint catalog entry. | Allocate a collision-free slug from the fresh locked index and append the sprint row when enabled. |
| `sprint start <NAME>` | `conditional-write` | Sets `status: ACTIVE` and the start date. | Resolve the project-local sprint and patch it atomically when enabled. |
| `sprint complete <NAME>` | `conditional-write` | Sets `status: COMPLETED` and `completedAt`; unfinished tickets require `--force` and are carried to the backlog. | Validate sprint completion and apply all catalog/ticket changes in one operation when enabled. |
| `sprint add <NAME> <KEY>` | `conditional-write` | Sets a ticket `sprint` reference. | Validate the sprint and ticket references, then patch the ticket atomically when enabled. |
| `sprint remove <NAME> <KEY>` | `conditional-write` | Clears a ticket `sprint` reference. | Patch the ticket atomically when enabled. |
| `sprint gate <NAME> <KEY>` | `conditional-write` | Sets the sprint gate ticket reference. | Validate the ticket and patch the sprint catalog when enabled. |
| `version list [--project KEY]` | `retained-read` | Exact `{id,name,description,status,releaseDate,issues,done,points,donePoints}` rows from `<boardRoot>/versions.md`; membership is derived from canonical `fixVersions[]` (the source `jira.issue_versions` rows remain audit data). | Resolve names/status/date and aggregate issue completion. |
| `version create <NAME> [--description TEXT]` | `conditional-write` | Adds a versions catalog entry. | Validate the project-local catalog and append the version when enabled. |
| `version release <NAME>` | `conditional-write` | Sets version status and release date. | Resolve the version and patch the catalog atomically when enabled. |
| `version set <NAME> <KEY> [--kind FIX\|AFFECTS]` | `conditional-write` | Adds/removes canonical `fixVersions[]` membership. | Validate both references and patch the ticket atomically when enabled. |
| `update <KEY> [flags]` | `conditional-write` | Changes frontmatter/body fields. `--body`, `--goal`, `--dod` are path-valued; label, status and assignment fields are scalar/list values. | Preserve unknown fields and body bytes while applying validated changes under ownership/attempt and workflow rules when enabled. |
| `estimate <KEY> <POINTS\|none>` | `conditional-write` | Changes `storyPoints`. | Apply the validated point value atomically when enabled. |
| `flag <KEY> [--clear]` | `conditional-write` | Changes `flagged`. | Apply the flag mutation atomically when enabled. |
| `resolve <KEY> <RESOLUTION> [--clear]` | `conditional-write` | Changes `resolution` and resolved timestamp. | Validate the resolution and apply it atomically when enabled. |
| `history <KEY>` | `retained-read` | Exact `{field,fromValue,toValue,createdAt,actor}[]`, newest first, with `--limit` default 50. | Read `jira.issue_activity` rows from history files/metadata and resolve actor display names. Do not synthesize activity from current frontmatter. |
| `report epic [--project KEY]` | `retained-read` | `EpicProgressRow[]` with `{key,title,total,done,points,donePoints,percent}`. | Aggregate all epics in project scope using parent and status category indexes. |
| `report velocity [--project KEY]` | `retained-read` | `{rows:[{sprint,committed,completed}], average}`; committed is sprint issue points and completed is points in DONE category. | Aggregate sprint files and ticket status/points; preserve the legacy `COALESCE` behavior that counts null story points as zero. |
| `board status [--project KEY] [--roadmap PATH]` | `retained-read-local` | `{source,keysInDoc,issuesOnBoard,rows:[{kind,key,detail}]}` from a local Roadmap/Board document plus ticket index. `drift` is not part of the local result. | Scan local docs and compare ticket keys. The old uploaded `docsTree` transport is removed; input is the explicit local vault root. |
| `notion provision <ORG_SLUG>` | `removed-transport` | None; remote Notion project provisioning and URLs are removed. | Explicit unsupported response. Existing knowledge rows remain readable from metadata/current files. |
| `docs sync [options]` | `removed-transport` | None; no Notion URL/docsTree upload is retained. | Explicit unsupported response. |
| `docs status [options]` | `retained-read-local` | `{mode:"local",documents,diagnostics}` from local Markdown docs and config. | Report local scanner status; do not upload or mutate a Notion mirror. |
| `docs ls [scope]` | `retained-read` | `[{path,title}]` from local Markdown roots. | List files within the explicit vault root; paths and titles are the local replacement for Notion page rows. |
| `docs read <PATH>` | `retained-read` | `{path,markdown}` from a local Markdown file. | Read only files inside the vault root; preserve raw bytes and report parse diagnostics. |
| `docs write <PATH> ...` | `removed-transport` | None; remote/document write transport is not part of the local CLI. | Explicit unsupported response; use a supported conditional mutation for tickets, catalogs or knowledge rows. |
| `docs search <QUERY> [scope]` | `retained-read` | `[{path,title}]` matching local Markdown documents. | Search local Markdown without rewriting files; preserve the local result vocabulary. |
| `rows <DATABASE> [--where FIELD=VALUE]` | `retained-read` | Exact `{id,url,values}[]` rows, newest first; `values` uses the selected database’s property names. | Resolve database key and property names, resolve select option ids to labels, and preserve raw values in the source index. |
| `rows <DATABASE> resolve <ID> <STATUS>` | `conditional-write` | Updates the database row’s status property and optional resolution reason. | Resolve the row under the operation protocol when enabled; disabled vaults refuse without changing metadata bytes. |
| `ask --ticket <KEY>\|--org <SLUG> --file <JSON>\|--text TEXT` | `conditional-write` | Appends a `questions` row with ID, category, asked date, question, context, best guess, cost, status, resolution and ticket link. | Validate the input and append to the configured knowledge file/catalog when enabled. |
| `decide --ticket <KEY>\|--org <SLUG> --file <JSON>\|--text TEXT` | `conditional-write` | Appends a `decisions` row with ID, category, date, decision, why, tests, reversibility, repo and ticket link. | Validate the input and append under the operation protocol when enabled. |
| `shortfall --ticket <KEY>\|--org <SLUG> --file <JSON>\|--text TEXT` | `conditional-write` | Appends a `shortfalls` row with category, area, gap, impact, status, resolution, filed-as and ticket link. | Validate the input and append under the operation protocol when enabled. |
| `changelog --ticket <KEY>\|--org <SLUG> --file <JSON>\|--text TEXT` | `conditional-write` | Appends a `changelog` row with date, category, summary, commit, repo, model, effort and ticket link. | Validate the input and append under the operation protocol when enabled. |
| `suggest --ticket <KEY>\|--org <SLUG> --file <JSON>\|--text TEXT` | `conditional-write` | Appends a `suggestions` row with date, area, suggestion, friction, severity, status, resolution, ticket link and filed-as. | Validate the input and append under the operation protocol when enabled. |
| `suggestions [--all]` | `retained-read` | Exact `{id,url,values}[]` suggestion rows newest first. Default filters unresolved/new workflow friction; `--all` returns every retained row. | Read rows, resolve SELECT labels and IDs, and include ticket links in `values`. |
| `suggestions resolve <ID> done\|accepted\|rejected` | `conditional-write` | Updates a suggestion status. | Resolve the row under the operation protocol when enabled; disabled vaults refuse without changing metadata bytes. |
| `contract` | `removed-transport` | None; the old endpoint advertised a remote wire contract. | Explicit unsupported response. `Tools/zagent/contract.json` was removed along with the retired remote transport it described; this matrix is now the sole offline specification. |

### integrate

`zagent integrate <KEY> [--json]` lands one lane ticket's `agent/<KEY>` branch
onto its project's `baseBranch` (plan "Integration mechanics"; parallel
`/tick` lanes). It only ever runs from a registered lane — `index.lane` unset
(the main checkout, or an unregistered linked worktree) is refused before
anything else, exit 1, the same disposition `claim`/`next`/etc. already use
for an unregistered worktree.

Unlike every other `conditional-write` row above, `integrate` never edits a
board file, so it does not go through `server/operations.mjs`'s
`executeOperation`/journal-of-file-changes machinery `write.mjs`'s other
mutations share. It has its OWN journal kind (`kind: 'integrate'`, `phase:
prepared | ref-updated | synced | complete`) and its own crash-recovery
branch inside `recoverLocked`, because a merge (`oldMaster` → `merge`, two
parents) cannot be expressed as a `{path, before, after}` change list. The
merge itself — `merge-base`, `merge-tree --write-tree`, `commit-tree`, the
`update-ref` compare-and-swap, and the main checkout's `read-tree -m -u` sync
— runs under `refs/vault/operation-lock`, the identical lock every board
write takes, via `server/operations.mjs`'s `executeIntegrate`. Everything
before that lock (ownership/status/lane refusals, the `UNAPPROVED` check, and
freshness/gate-verdict classification against the ticket's own
`.zagent/run/<KEY>/tick_gates.json`) is read-only and lives in
`cli/integrate.mjs`.

**Read-only refusals, exit 1** (checked in this order, before any lock):
not lane mode; writes disabled (`vault.config.json:writes.enabled`, the same
gate every `conditional-write` row obeys); the key does not resolve to a
ticket; the ticket is not `In Progress`; the ticket is not agent-owned (the
same `isAgentOwner` predicate `claim`/`finish` use); the ticket's own `lane:`
frontmatter does not match this invocation's lane project; the lane's `HEAD`
is not `refs/heads/agent/<KEY>`.

**The result ladder.** `payload.result` names the outcome; `exitCode` is what
a tick branches on — see `spec/command-matrix.md`'s own convention
("exit codes are the interface"), not the result string:

| `result` | exit | meaning |
| --- | --- | --- |
| `LANDED` | 0 | merged onto `baseBranch`; parents `(oldMaster, tip)`; the main checkout is synced. `payload` carries `{merge, oldMaster, regenNeeded}` — `regenNeeded` is informational: true when the merge adds/deletes/renames a tracked `.cpp`/`.c`/`.h`/`.inl`/`.slang` file outside `Middleware/`, or touches any `Build/Sharpmake_*.cs` / `Games/*/*.zproj`. |
| `REGATE` | 20 | the gap between the lane's merge-base and `baseBranch`'s current tip contains a commit that is not a board-only commit (subject not `board: `, or no `Vault-Operation:` trailer) — a code change (or another lane's own `integrate` merge) landed since this lane branched. Nothing moved. |
| `STALE_VERDICT` | 21 | no `tick_gates.json`, or it does not parse; verdict `phase` is not `verify`/`all` (a measure-only run is not a verification); `state` is not `complete`; `exitCode` is one of the non-evidential codes (8, 10, 11) or is undocumented; `tip^{tree}` does not match `treeShaEnd`; or the lane's working tree is dirty. |
| `NOT_GREEN` | 22 | the ticket's first-parent commit on `agent/<KEY>` is a `WIP(<KEY>):` park; no `<KEY>:` commit exists at all (defensive — not reachable through the protocol, but a human-run branch can do this); or the verdict is `complete` with `exitCode` in `{2, 3, 4, 5}` — a real gate failure, or "could not read the gate list". |
| `CONFLICT` | 23 | `merge-tree --write-tree` reported a real content conflict on the board-only gap — the ticket itself edited a board file `baseBranch` also changed. Nothing moved. |
| `TREE_UNSTABLE` | 24 | the verdict is `complete` with `exitCode` 12 ("tree unstable") — `tick_gates.ps1` already retried verify once internally; this is a distinct terminal, not another `STALE_VERDICT`. |
| `MAIN_DIRTY` | 25 | a dry-run `read-tree -m -u -n` in the main checkout found an overlapping dirty, staged, or untracked-in-the-way path (`payload.paths`). Nothing moved. |
| `MAIN_NOT_ON_BASE` | 26 | the main checkout's `symbolic-ref HEAD` is not `refs/heads/<baseBranch>`. Nothing moved. |
| `UNAPPROVED` | 27 | the ticket carries `human-gate` without `approved`. |
| `LOCKED` | 5 | `refs/vault/operation-lock` is held by another live writer. |
| `RACED` | 5 | the `update-ref` compare-and-swap on `baseBranch` lost a race after this run's own read of `oldMaster` — another writer landed something first. The journal is marked abandoned; nothing of this run's own landed. |

`integrate` never deletes a branch — that is the tick's own step 9, only
after detaching, with a safe `git branch -d`. It never reads or writes
`.zagent/integrate/<KEY>.json`, which is protocol-owned scratch, not a Vault
concern.

## Conditional write safety

The writer is opt-in per vault: `vault.config.json` must contain
`writes.enabled: true`. With that flag absent or false, every
`conditional-write` row returns a disabled-write result and leaves vault,
scratch, journal and Git bytes unchanged.

An enabled mutation resolves the authoritative checkout, acquires the common
operation lock, reloads the repository, checks expected file revisions and
validates the full proposed graph before applying any change. Text edits are
atomic and preserve unknown frontmatter/source fields and untouched body
bytes. The operation journal records before/after hashes and supports
recovery, including a process exit after commit. `--operation-id` is the
explicit retry key; omitted ids represent new mutation intents.

Agent status changes require the current assignee and `claimAttempt`. A
`human-gate` ticket can be completed by an agent only through `In Review`; a
human actor may close it. Comments remain available as the abandonment
evidence path after ownership changes. The queue/check decision and the
mutable failure breaker use the same fresh repository-wide predicates.

## Knowledge databases and option mapping

The five fixed knowledge databases are part of the retained read model even
when the current exporter stores their rows in `metadata.md`. Database and
property IDs remain source provenance; CLI output uses stable database keys
and property names.

| Database key | Properties and source field mapping |
| --- | --- |
| `decisions` | `ID`=`id`; `Category` SELECT; `Date` DATE; `Decision` TEXT; `Why` TEXT; `Tests that lock it` TEXT; `Reversibility` SELECT (`easy`, `hard`, `one-way`); `Repo` SELECT; `Ticket URL` link. |
| `questions` | `ID`=`id`; `Category` SELECT; `Asked` DATE; `Question` TEXT; `Context` TEXT; `Best guess taken` TEXT; `Cost if wrong` SELECT (`low`, `medium`, `high`); `Status` SELECT (`open`, `resolved`, `stale`); `Resolution` TEXT; `Ticket URL` link. |
| `shortfalls` | `Category` SELECT; `Area` TEXT; `Gap` TEXT; `Impact` SELECT (`low`, `medium`, `high`); `Status` SELECT (`open`, `filed`, `fixed`); `Resolution` TEXT; `Filed as` URL; `Ticket URL` link. |
| `changelog` | `Date` DATE; `Category` SELECT; `Summary` TEXT; `Commit` TEXT; `Repo` SELECT; `Model` SELECT (`haiku`, `sonnet`, `opus`); `Effort` SELECT (`low`, `medium`, `high`); `Ticket URL` link. |
| `suggestions` | `Date` DATE; `Area` SELECT (`contract`, `gates`, `cli`, `tick`, `config`, `board`, `other`); `Suggestion` TEXT; `Friction` TEXT; `Severity` SELECT (`nuisance`, `slows-work`, `blocks-work`); `Status` SELECT (`new`, `accepted`, `rejected`, `done`); `Resolution` TEXT; `Ticket URL` link; `Filed as` URL. |

`optionId(label)` is lowercase ASCII, with each run of non-ASCII
alphanumeric characters replaced by `-` and leading/trailing `-` removed.
Rows store IDs internally and output labels. Unknown IDs stay visible as raw
IDs and are diagnostics; they are never guessed. `rows` and `suggestions`
sort newest first, then stable row ID.

## Explicitly removed read dependencies

The retained results do not read or recreate these legacy transport/state
surfaces: Postgres/Drizzle at runtime, Yjs or y-websocket state, Tiptap
document models, bearer tokens, HTTP handshake/contract endpoints, Notion
URLs/docsTree uploads, workflow rules/permissions/screens, board columns and
quick filters, auth/session/account material, notifications, or remote agent
config mirrors. Their schema fields remain listed in the machine inventory
with `dropped`, `dropped-empty`, or `out-of-scope-untouched` dispositions.
