# Jira UI parity matrix

Status: implementation and verification in progress. Source inventory was taken
from both repositories on 2026-09-16. The legacy inventory below records the
starting gaps; completion requires corresponding behavior and test evidence,
not merely a larger test count.

## Scope and count conventions

- Legacy source root: the retired upstream board application's repository.
  Paths below are relative to that repository root, for example `apps/jira/...`
  and `packages/ui/...`.
- Vault source root: C:/dev/Zenith/Tools/Vault.
- The inventory covers every Jira route, Jira-owned component, UI workflow, the
  supporting route handlers/actions, and shared shell components that the Jira
  UI consumes.
- The verified legacy Vitest baseline is 834/834 tests. A source scan finds
  about 814 direct test declarations because parameterized declarations expand
  to more runs; the run result is the baseline for parity.
- The legacy Jira Playwright suite has 103 declared tests in 14 specs. It uses
  the signed-in Alice, Bob, and Carol fixtures and a single worker.
- Legacy frontend/server coverage is configured in
  apps/jira/vitest.config.ts with V8 thresholds of statements 76,
  branches 72, functions 63, and lines 80. The verified legacy run measured
  statements 79.69, branches 74.76, functions 82.56, and lines 81.72.
- The Vault repository currently passes 675 Vitest tests in 61 files and 30
  Playwright tests in 7 specs (observed 2026-09-18). The declared legacy test
  surface (834 Vitest, 103 Playwright) is still larger. Its Vitest
  configuration now enforces the legacy coverage thresholds; the latest
  measured run reported statements 84.16, branches 72.62, functions 83.57,
  and lines 88.85.
- Counts are evidence of the test surface, not a target to copy mechanically.
  Parity requires equivalent behavioral scenarios and measured frontend/server
  coverage at least the legacy thresholds.

## Architecture decision

The accepted migration plan explicitly specifies “One vault, one user” and
drops the previous board's authentication. Vault retains this local operating mode. The session
declares it and exposes a named repository actor. Imported user identities remain
available for attribution and assignment. Organization sign-in, invitations,
tenant switching, permission-scheme administration, and delivery of notifications
to other accounts are intentionally unavailable in this architecture. Local
read-only/archive restrictions and server-side forbidden-write checks remain
required. See `project-ui-api.md`.

| Decision | Consequences that must be designed, tested, and visible |
| --- | --- |
| Authenticated multi-user | Preserve account identity, org slug and membership, OWNER/ADMIN/MEMBER/VIEWER roles, project permission schemes, per-user notifications, cross-user live updates, CSRF/session or bearer auth, and tenant isolation. A single shared checkout also needs a locking/concurrency policy. |
| Local asynchronous single-user | Preserve a named local actor, readOnly/writeEnabled state, owner/admin semantics, Git/CAS operation identity, and an explicit policy for every role-gated control. It may simplify identity storage, but it cannot silently drop permissions, admin settings, notifications, or forbidden-write behavior. |

- [x] Record the selected operating mode in the API/session contract and
  document which legacy multi-user workflows are implemented, adapted, or
  intentionally unavailable.
- [x] Keep the permission model explicit in either mode. The server must reject
  forbidden mutations even when a control is hidden; the UI must explain
  read-only or role-gated controls.
- [x] Preserve the distinction between an absent catalog/configuration and an
  empty one. Do not use a default allow-all permission or a default status
  list to hide missing data.

## Complete legacy route inventory

| Legacy route | Behavior and data contract | Existing test evidence |
| --- | --- | --- |
| apps/jira/src/app/page.tsx | Root auth check; unauthenticated users go to the shared sign-in route and authenticated users enter the Web dashboard. | variations.spec.ts (5), full-session.spec.ts (1) |
| apps/jira/src/app/layout.tsx | Global theme script, fonts, toaster, and app-wide providers. | shared UI package tests below |
| apps/jira/src/app/(workspace)/[orgSlug]/layout.tsx | Authenticates, resolves organization and membership, loads projects and notifications, and renders AppShell, sidebar, app switcher, command palette, user menu, and error boundary. | project-flow.spec.ts, variations.spec.ts, mentions.spec.ts, shared UI tests |
| apps/jira/src/app/(workspace)/[orgSlug]/page.tsx | Organization project grid and New Project entry point. | project-flow.spec.ts |
| apps/jira/src/app/(workspace)/[orgSlug]/projects/new/page.tsx | Create project form and post-create navigation to the project board. | create-project-form.test.tsx (19), project-flow.spec.ts |
| apps/jira/src/app/(workspace)/[orgSlug]/project/[projectKey]/layout.tsx | Project-level shell and modal slot; ProjectNav links all eight project sections. | project-flow.spec.ts, issue-modal.spec.ts, project-settings.spec.ts |
| .../project/[projectKey]/board/page.tsx | Kanban or active-sprint Scrum board; loads workflow statuses, cards, assignees, categories, blockers, legal targets, labels, sprints, and board configuration. | board component/action tests; project-flow.spec.ts; project-settings.spec.ts |
| .../project/[projectKey]/board/settings/page.tsx | Admin board settings: column/status mapping, WIP limits, card fields, and quick filters. | board-settings.test.tsx (20), project-settings.spec.ts |
| .../project/[projectKey]/backlog/page.tsx | Backlog plus planned/active/completed sprint sections and sprint controls. | backlog-view.test.tsx (41), backlog-sprint.spec.ts (39) |
| .../project/[projectKey]/roadmap/page.tsx | Epics, orphan work, child progress, blockers, and version mapping. | roadmap-view.test.tsx (18), project-flow.spec.ts |
| .../project/[projectKey]/reports/page.tsx | Velocity for the project and burndown for the selected sprint. | reports.test.ts (18); full-session and sprint e2e coverage |
| .../project/[projectKey]/releases/page.tsx | Version/release list, progress, and admin version management. | releases-view.test.tsx (17); project settings e2e |
| .../project/[projectKey]/workflow/page.tsx | Workflow graph, statuses, transitions, conditions, validators, post-functions, and transition screens. | workflow-editor.test.tsx (25), workflow action tests (41), project-settings.spec.ts |
| .../project/[projectKey]/permissions/page.tsx | Permission scheme, project roles, grants, and read-only/member behavior. | permissions-editor.test.tsx (24), permissions action tests (22), project-settings.spec.ts |
| .../project/[projectKey]/fields/page.tsx | Custom field definitions, field configuration, screens, and transition-screen chaining. | fields-editor.test.tsx (21), fields/field-config/screens action tests (30/18/21), project-fields.spec.ts (5) |
| .../project/[projectKey]/issue/[issueKey]/page.tsx | Full-page issue detail for a direct URL. | issue-detail/property/link tests; issue-modal.spec.ts; full-session.spec.ts |
| .../project/[projectKey]/@modal/(.)issue/[issueKey]/page.tsx | Intercepting modal issue detail; preserves board/backlog context and URL, with close, Escape, and browser-back behavior. | issue-modal.spec.ts (8) |
| .../loading.tsx, .../error.tsx, .../not-found.tsx | Loading, recoverable error, and missing-resource states for workspace/project/issue routes. | route-states.test.tsx (4); route boundary coverage |
| apps/jira/src/app/api/auth/[...nextauth]/route.ts | Shared authentication handler and cross-app session cookie integration. | auth fixture/e2e redirect coverage |
| apps/jira/src/app/api/healthz/route.ts | Auth-independent DB health check returning Jira service status. | route tests in the legacy API suite |
| apps/jira/src/app/api/realtime/token/route.ts | Authenticated issue namespace token minting with project/read/write guards. | realtime token route test (6) |
| apps/jira/src/app/api/board/stream/route.ts | Authenticated project-filtered SSE with connected/heartbeat/event behavior. | board stream route test (10) |
| apps/jira/src/app/api/agent/route.ts | Bearer-authenticated agent RPC/liveness endpoint used by the agent loop, not the primary browser UI. | agent route test (25), agent-loop.spec.ts (5) |

The route aliases above are shorthand only; the canonical source paths are the
absolute paths in the first column.

## Workstream A: shared infrastructure, identity, and shell

**Legacy surfaces.** The workspace layout and middleware establish auth,
organization membership, project visibility, and cross-app navigation. The
shell uses AppShell, Sidebar, AppSwitcher, ThemeToggle, NotificationsBell,
CommandPalette, CommandMenuTrigger, ErrorBoundary, PageHeader, and
UserMenuClient. ProjectNav exposes Board, Backlog, Roadmap, Reports, Releases,
Workflow, Permissions, and Fields. WorkspaceHeaderTitle keeps the browser
title synchronized with workspace, project, and issue paths.

**Sources.**

- apps/jira/src/middleware.ts
- apps/jira/src/app/(workspace)/[orgSlug]/layout.tsx
- apps/jira/src/components/project/project-nav.tsx
- apps/jira/src/components/shell/route-states.tsx
- apps/jira/src/components/shell/workspace-header-title.tsx
- apps/jira/src/components/shell/user-menu-client.tsx
- packages/ui/src/app-shell.tsx,
  app-switcher.tsx, command-menu.tsx, command-palette.tsx,
  error-boundary.tsx, notification-popover.tsx, notifications-bell.tsx,
  page-header.tsx, sidebar.tsx, status-badge.tsx, theme-toggle.tsx,
  toaster.tsx, user-menu.tsx

**Test evidence.** Jira route-states has 4 tests and
workspace-header-title has 12. The relevant shared UI package tests have 97
tests across app shell (13), app switcher (4), command menu (12), command
palette (3), error boundary (7), notification popover (7), notifications bell
(4), page header (10), sidebar (11), status badge (3), theme toggle (11),
toaster (7), and user menu (5). E2E coverage includes project-flow,
variations (5), mentions (3), search (1), and the full session (1).

**Vault status.** C:/dev/Zenith/Tools/Vault/web/App.jsx has a local session
with csrfToken/readOnly/writeEnabled and a local view switch. It has no org
identity, membership, cross-app shell, user menu, project nav, notifications,
command palette, theme/error boundary parity, or route-level loading/error/
not-found behavior.

- [x] Implement the selected identity/session topology and expose actor,
  organization, membership, readOnly, writeEnabled, and CSRF capabilities from
  the session endpoint.
- [ ] Add authenticated or explicitly local equivalents for org/project
  selection, project navigation, user menu/sign-out, app switcher, theme,
  notifications, command palette/search, page titles, and recoverable route
  states.
  Missing: notifications, a user menu/sign-out, and an app switcher; the
  command palette is a Ctrl+K focus of the search box, while project
  selection, navigation, theme, search, page titles and recoverable error
  states exist.
- [ ] Enforce membership and role checks in the server for every route/action;
  cover anonymous redirect, non-member block, member read-only, and owner/admin
  access.
  Missing: there is no multi-user membership or role model to check; the
  server enforces the local boundaries instead (read-only, CSRF, origin, host).
- [ ] Add browser tests for project switching, active nav on every tab, issue
  title navigation, command palette search, notification navigation, theme,
  sign-out, and loading/error/not-found states.
  Missing: notification-navigation and sign-out scenarios have no UI to test;
  loading/not-found coverage is limited to recoverable error states.

## Workstream B: organization project list and project creation

**Legacy behavior.** The organization page lists projects with stable keys and
links. CreateProjectForm accepts name, key, and description, generates or
validates a key, creates the project scaffold, and routes to its board. The
project shell keeps navigation and selected project identity stable.

**Sources and tests.**

- apps/jira/src/components/project/create-project-form.tsx
  (19 Vitest tests)
- apps/jira/src/components/project/project-nav.tsx
- projects action tests: 5
- project-flow.spec.ts (seeded project listing, project links, nav, create CTA,
  issue creation), variations.spec.ts (sidebar project switching)

**Vault status.** FileTree groups Markdown files by a parsed project value but
there is no organization project grid, project catalog, create-project form,
stable project route, or project scaffold action. A file tree grouping is not
equivalent to project creation and tenancy.

- [x] Add a project list and create-project flow with key validation,
  description, scaffold creation, success navigation, duplicate-key/error
  states, and read-only/member behavior.
- [x] Make project selection part of URL and server authorization rather than
  only a client-side file filter.
- [x] Add HTTP and browser tests for empty/listed projects, invalid and
  duplicate keys, successful scaffold creation, project isolation, and
  unauthorized access.

## Workstream C: board, filters, cards, and live updates

**Legacy behavior.** BoardView renders configured columns, BoardColumn supports
status groups and WIP highlighting, IssueCard exposes key/title/type/priority/
assignee/labels/category/blocked state, BoardFilters applies AND/narrow
filtering, and CreateIssueInline creates an issue in the selected project.
Board settings map one or more workflow statuses to columns. Kanban shows all
work; Scrum shows the active sprint. Dragging asks the workflow engine for a
legal target, applies transition validators/conditions/post-functions, and
updates with rollback/toast. The default workflow has To Do, In Progress, In
Review, Done, and Blocked; To Do is the agent queue. Unmapped statuses are
surfaced instead of disappearing. Board events are project-scoped and
debounced.

**Sources and tests.**

- apps/jira/src/components/board/board-view.tsx (25)
- board-column.tsx (15), board-filters.tsx (25), issue-card.tsx (35),
  create-issue-inline.tsx (10), board-settings.tsx (20)
- apps/jira/src/lib/board-store.ts (19) and
  src/hooks/use-board-stream.ts (13)
- board action tests: 26
- project-flow.spec.ts (columns, cards, inline create, nav), agent-loop.spec.ts
  (5, claim/handbrake/work log/comment/five lanes), full-session.spec.ts (1),
  project-settings.spec.ts (board settings), and variations.spec.ts

**Vault status.** Board in App.jsx derives columns from state.statuses (with a
three-status fallback), renders basic cards, and uses HTML5 drag/drop to POST
status. It has no configured column model, status legality, filters,
assignee/category/label/blocker fields, WIP, active-sprint mode, inline issue
creation, rollback/toast, or SSE-equivalent project event semantics. The
current browser tests cover basic board rendering, read-only drag refusal,
writable drag plus Git commit, and multiple lanes.

- [ ] Replace the fallback board with workflow-backed status/column data and
  render all five default lanes plus custom/unmapped status diagnostics.
  Missing: an issue whose status is mapped to no configured column is absent
  from the status board and raises no diagnostic; workflow-backed columns and
  the five-lane default scaffold exist.
- [ ] Implement Kanban/Scrum selection, active-sprint filtering, AND filters,
  quick filters, card field configuration, WIP thresholds/highlighting, and
  blocked/link indicators.
  Missing: Scrum active-sprint filtering, WIP over-limit highlighting, and
  blocked/link indicators on cards; AND filters, quick filters, card field
  configuration and WIP limit entry exist.
- [ ] Route drag through one workflow legality decision, show disabled reasons
  for validators/conditions, run post-functions/resolutions, and provide
  optimistic rollback plus an error toast.
  Missing: disabled reasons for validator/condition failures and optimistic
  rollback with an error toast; legality, validators, conditions and
  post-functions are enforced server-side and refusals render inline.
- [x] Implement inline issue creation, card issue links, modal opening, and
  keyboard/accessibility behavior.
- [ ] Implement authenticated/project-scoped live board events (SSE or a
  demonstrably equivalent protocol), heartbeat/reconnect, debounce, and stale
  selection protection.
  Missing: event-burst debounce — each websocket notification triggers an
  idempotent, request-versioned refresh; heartbeat, reconnect and
  stale-selection protection exist and are browser-tested.
- [ ] Add HTTP, component, and browser scenarios for every filter combination,
  WIP below/at/above limit, unmapped status, illegal/validator-blocked/legal
  drag, stale revision rollback, Scrum active sprint, project event isolation,
  and read-only/member permissions.
  Missing: scenarios for WIP below/at/above limits, unmapped statuses, and the
  Scrum active sprint; filter combinations are not exhaustively covered.

## Workstream D: backlog and sprint lifecycle

**Legacy behavior.** BacklogView separates backlog, planned sprint, active
sprint, and completed sprint sections. MoveIssueMenu handles backlog/planned/
active destinations. Create, start, complete, edit, and delete sprint dialogs
enforce one active sprint per project, project isolation, stale-write
rejection, completed-sprint exclusion, incomplete-issue choice, and
cancellation. Completing an all-done sprint skips the chooser; incomplete
completion offers planned/backlog choices. Complete writes completedAt.

**Sources and tests.**

- apps/jira/src/components/backlog/backlog-view.tsx (41)
- move-issue-menu.tsx (3), complete-sprint-dialog.tsx,
  edit-sprint-dialog.tsx, delete-sprint-dialog.tsx
- sprints action tests: 6
- backlog-sprint.spec.ts (39: all move, start/complete, edit, delete,
  cancellation, stale race, isolation, empty-state, and viewer controls)
- project-flow.spec.ts and full-session.spec.ts cover backlog entry and the
  end-to-end sprint path

**Vault status.** Backlog is a sorted list of non-Done tickets. It has no
sprint entities, sections, move menu, dialogs, active-sprint rule, completion
policy, goal/date editing, delete safeguards, or viewer control handling.
CLI sprint commands exist in C:/dev/Zenith/Tools/Vault/cli/write.mjs, but
CLI capability does not provide a browser API or UI workflow.

- [x] Add sprint catalog/read models and render backlog, planned, active, and
  completed sections with issue counts and empty states.
- [x] Implement move, create, start, complete, edit, cancel, and delete
  operations with one-active-sprint, project-isolation, stale-revision,
  completed-exclusion, and delete-active safeguards.
- [x] Implement complete-sprint choices, including all-done skip and
  incomplete-to-planned/backlog behavior with singular/plural copy.
- [ ] Add HTTP and browser tests for every lifecycle branch, cancellation,
  stale race, project isolation, and viewer/member permission boundary.
  Missing: dedicated browser coverage of dialog cancellation; lifecycle
  branches are covered by action-refusal tests rather than exhaustively.

## Workstream E: issue detail, modal navigation, editing, and relationships

**Legacy behavior.** Full and intercepting-modal issue routes use one
loadIssueDetailData payload containing the issue, revision, statuses, sprints,
categories, comments, links, parent/children, history, transitions, field
layout, members, labels, versions, and parent options. The modal preserves
the originating board/backlog context; close, Escape, and browser back restore
it; a direct URL renders the full page.

IssueDetail and IssueProperties support title, description, status/type/
priority/complexity/risk/category/story points/resolution/severity/
reproducibility/flag/assignee/due date/environment/parent, labels, version,
and sprint changes. Status changes use the workflow engine and transition
screens. IssueDescription renders Markdown/wiki links and references.
IssueFamily renders parent/children. IssueLinks creates/removes directed link
types, prevents cycles, and names blockers. Comments support post, mentions,
reference cards, relative timestamps, and deleting only the current user's
comment. IssueHistory renders activity values. Viewer/member write controls
are permission-gated.

**Sources and tests.**

- apps/jira/src/app/(workspace)/[orgSlug]/project/[projectKey]/
  issue/[issueKey]/page.tsx and @modal/(.)issue/[issueKey]/page.tsx
- issue-detail.tsx (14), issue-properties.tsx (72), issue-description.tsx
  (12), issue-links.tsx (19), comments-section.tsx, issue-family.tsx,
  issue-history.tsx, issue-detail-modal.tsx
- action tests: issues 48, links 26, labels 10, categories 13, and the
  workflow/field/sprint/version paths used by properties
- issue-modal.spec.ts (8), comments.spec.ts (4), mentions.spec.ts (3),
  inline-references.spec.ts (2), activity.spec.ts (1),
  backlog-sprint.spec.ts issue-detail moves, agent-loop.spec.ts comments, and
  full-session.spec.ts

**Vault status.** DocumentHeader offers a status select, CodeMirror
edit/preview/save, ETag conflict reload, Markdown/wiki-link rendering, and
backlinks. It has no issue key route or modal, issue property panel,
transition screen, comments/mentions/history, directed links/blocker
reasons, parent/child family, labels/categories/versions/sprints, or
per-field permission semantics.

- [x] Add stable issue keys, full and modal routes, one detail payload, URL/
  history semantics, direct-link full-page rendering, and selected-issue/draft
  race protection.
- [ ] Expose every legacy editable property and preserve null versus empty
  values, field visibility, disabled reasons, validation errors, and
  read-only/member behavior.
  Missing: per-field disabled reasons — read-only is explained globally
  ("This project is read-only."), individual gated controls carry no reason
  text; every legacy property is editable and validation errors render inline.
- [x] Add workflow transition screens and legal-target/validator/condition/
  resolution semantics to issue status changes.
- [ ] Implement comments, mention notifications, inline references,
  user-scoped delete, history, parent/child hierarchy, directed issue links,
  blocker labels, cycle prevention, labels, versions, categories, and sprint
  assignment.
  Missing: mention notification delivery — mentions render in comment bodies
  but are never delivered to another account.
- [ ] Add component, HTTP, and browser tests for modal/full navigation,
  positive then forbidden writes, conflict/reload, every property type,
  mentions to member versus non-member, own versus other-user delete,
  blockers/cycles, hierarchy, references, and history rendering.
  Missing: mention member/non-member delivery tests, which cannot exist while
  notification delivery is unimplemented.
