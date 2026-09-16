# Managed writes and recovery

Status: implementation contract, 2026-09-16. Production cutover requires the
isolated rehearsal and a fresh frozen migration; this note is not cutover evidence.

## Authority and coordination

One checkout is registered as the authority in the repository's common Git directory.
Managed clients from other worktrees resolve to it. The app and CLI share the same
operation engine. They do not create, switch, merge, or delete branches. Branch
automation is outside this application; an authority checkout must not be switched
while a write operation is running.

Writes require explicit `writes.enabled: true` in vault.config.json. The frozen test
fixture remains read-only. A Git reference under `refs/vault/operation-lock` is an
atomic compare-and-swap mutex shared by worktrees. Its object records hostname, PID,
nonce and heartbeat. An alive PID is never evicted. A dead local PID can be replaced
only by comparing the exact old object ID. This avoids the stale-file unlink race
in a plain O_EXCL lock. Unknown hosts fail closed. Lock state is not ticket ownership.

## Operations

Every operation has an ID, kind, before/after bytes and hashes for each changed path,
and durable phases. Preparation runs while holding the mutex against a fresh disk
scan. The proposed complete graph is validated before any authoritative file changes.
Revision checks include hash, byte length and mtime for editor saves. Each replacement
uses a flushed temporary file in the same directory followed by rename.

The durable journal precedes all replacements. Recovery accepts only each file's
recorded before or after bytes, completes remaining replacements idempotently, and
refuses if another writer has produced a third state. It never guesses from parsing
success. Committed operations record their exact commit; a crash after Git's atomic
reference update is detected from the operation marker and file hashes.

Board commits contain only operation paths and are constructed from the recorded
parent tree using a temporary Git index. Unrelated staged or working-tree changes
on other paths are not included. A save commits the complete validated target file,
including any external edits which the user has reloaded into the editor. The branch reference update compares the recorded parent. A moved
branch or mismatched file requires explicit recovery, not forced integration.

## Claims and humans

Claim and check use one decision over freshly read authoritative files. Any invalid
board data refuses claims repository-wide. An agent-owned In Progress ticket blocks
another claim. An attempt ID and expected owner prevent stale agent close-out after
human takeover. Human edits remain visible immediately. No mutation based on ticket
ownership relies on a previous, unlocked `owns` response.

## Limits and gate semantics

The mutex coordinates managed writers. Arbitrary external editors and raw Git commands
do not participate; revision checks detect observed changes, but do not turn a mutable
filesystem into transactional storage. Recovery refuses unrecognized external bytes.
The UI retains unsaved edits when a save is stale instead of overwriting disk state.

This work does not strengthen existing tick gate verdicts or implement the separate
inputsSha256 proposal. A green verdict retains its existing command/HEAD semantics;
it is not a certificate covering concurrent source edits or ignored build artifacts.
No automatic code integration is implemented by the vault.

## Acceptance

Use disposable Git repositories to test competing processes, a killed lock owner,
hard kills after individual replacements and after commit, stale editor saves,
external recovery conflicts, unrelated staged files, path/symlink escapes, ownership
takeover, and multi-file graph validity. Rehearse migration and forward recovery
before switching the real launcher or disabling old writers.
