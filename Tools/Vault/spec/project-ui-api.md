# Project UI API contract

The project UI uses the existing localhost server and shared Git operation engine.
The production vault is never used as a test fixture.

## Operating mode

The accepted migration plan specifies one vault and one user. `/api/session`
therefore declares `operatingMode: "local"` and a named `actor` with safe
`id`, `name`, `email`, and `role: "OWNER"` properties. Identity comes from the
server's configured actor or repository Git identity; browser requests cannot
impersonate an actor. Imported identities remain available for attribution and
assignment. Local ownership does not bypass archived-project or global
read-only protection, workflow validation, or revision checks.

Organization sign-in, tenant switching, member invitations, per-user notification
delivery, and SaaS permission-scheme administration are outside this local
operating mode. They must not be described as implemented multi-user parity.
The corresponding read-only and forbidden-write boundaries are tested locally.

## Reads

- `GET /api/projects`: `{ projects: [{key, name, description, readOnly}] }`.
- `GET /api/projects/:project`: project detail, catalogs and revision map.
- `GET /api/issues/:key`: issue detail, revision, comments, links, family and history.

Project detail has `key`, `name`, `description`, `readOnly`, `tickets`, `statuses`,
`labels`, `categories`, `users`, `sprints`, `versions`, `board`, `workflow`,
`customFields`, `fieldConfiguration`, `screens`, `revisions` and `reports`.
Tickets use the existing `/api/state` record shape (`path`, `project`, `metadata`,
`title`, `status`, `hash`, `size`) plus top-level `key` and `body`.
Catalog entries retain stable IDs, display names and their source fields.
`statuses` is an array of `{id,name,category,position}`. `users` contains safe
identity fields only (`id,name,email`).

Issue detail has `issue` (the same ticket shape), `revision`, `revisions`,
`comments`, `links`, `family: {parent,children}`, `history` and `project`.
Comments have stable IDs, body text, author and date; imported content is preserved.
No account secrets or arbitrary filesystem paths are served.

## Writes

`POST /api/commands` accepts `{args, files, revisions, operationId}` and the
per-launch `X-CSRF-Token`. `args` uses the existing CLI mutation spelling.
`files` contains inline content, never server filesystem paths. Browser commands
use human semantics; actor/claim flags are controlled by the server.

The response is `{result, state}`. The result includes operation/commit identity.
Validation errors return 422, missing objects 404, read-only writes 403, stale
revisions 409 and lock contention 423. Structured errors have
`{error: {code,message}}`. Invalid inputs never partially mutate data.
Malformed paths and operation IDs return 400.

`revisions` maps vault-relative file paths to `{hash,size}` or null for an expected
missing file. Expected revisions must reach the shared writer and be checked
under its lock, rather than only in the HTTP handler. Reusing an operation ID
with the same input returns the same result; different input is refused.

Additional settings/entity actions use `POST /api/actions` with
`{action, project, key, data, revisions, operationId}` and the same guarantees.
Actions and their schemas must be explicitly allowlisted, validated and covered
by HTTP and browser tests. No arbitrary table or file write API is exposed.

## Components

Feature components accept project/issue data, `readOnly`, `onOpenIssue(key)`,
`onChanged()` and `api(path, {method, body})`. The shared API helper sends CSRF,
serializes JSON, creates operation IDs once per submitted operation and throws
errors carrying the HTTP status and structured code. Drafts survive failed saves.

Components load fresh catalog/detail data after successful changes and websocket
notifications. A stale response never silently overwrites a newer draft or another
selected issue. The shell owns project selection and URL navigation.
