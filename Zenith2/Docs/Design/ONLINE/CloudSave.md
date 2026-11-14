# ONLINE — cloud saves and conflict resolution

## Scope of this file

This file is part of the ONLINE design family; the entry document is `Zenith2/Docs/Design/ONLINE.md`. It specifies the cloud-save model on top of SAVE: which slots synchronise, how a local slot, its cloud copy and their last agreed state are compared, how far the cloud index can be trusted, the decision table, the reconcile, upload, download and deletion sequences with their commit points, garbage collection of superseded cloud objects, the explicit index repair, conflict detection, the automatic resolution policies, the conflict resolution flows and their state machine, recovery after an interrupted sync, transfer memory, the triggers, status reporting, and the shutdown and background flush. SAVE owns everything about local saves: the save format, slots, metadata, profiles, the journaled write, the checkpoint leases and the restore pipeline (`Zenith2/Docs/Design/SAVE.md` → "Purpose and scope"); ONLINE never re-specifies them and never touches `saves/`: it reads and writes whole slot files only through `Zenith_SaveSystem` (cross-area row SV12).

## Cloud-save model overview

<!-- elaboration -->
- **What is synchronised.** A slot's ZSAV file, byte for byte, wrapped in a 320-byte envelope that repeats the metadata a conflict prompt needs (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Cloud envelope"). The cloud also holds one index object listing every slot's payload hash, the base serial that names its object, and its metadata including the save display name (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Cloud index"). Nothing else of the game is synchronised.
- **Why a three-way model.** SAVE's header sequence is strictly increasing per profile on one device, but two devices increment their own counters independently, so sequences do not order saves across devices; wall-clock times are informative only. The model therefore compares content hashes: the local file's (L), the cloud copy's (C, from the index) and the last state both sides agreed on (B, the base, from the sync manifest). L ≠ B means "changed here", C ≠ B means "changed elsewhere"; both is a conflict. No rule ever picks a side by sequence.
- **Commit point, and why a committed entry's bytes always exist.** A cloud change is committed when the index records it, and every index entry carries the index serial at which it was written (its commit serial). Envelope objects are content-addressed and never overwritten: each upload writes a new object named by the slot, the index serial it read and the first 16 hexadecimal digits of its payload hash (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Cloud object names"), so the object a committed entry references is untouched while another upload is in flight. An object is deleted only by garbage collection, once no committed index has referenced it for a grace generation (§ "Garbage collection").
- **A commit is durable only where the service's conditional write is atomic.** On the offline backend, and on Play while the device is online, a conditional index write that completes OK is the service's own compare-and-swap, so a committed entry stays in every subsequent index until a subsequent commit replaces it. Two backends can undo a commit the session saw succeed: Steam synchronises `zenith2/z2.index` only at launch and exit, so two machines can each commit a version between syncs and Steam's launch-time conflict dialog keeps one of them; Play completes a conditional write made while offline against the locally cached snapshot, and the server copy can win when the device reconnects (§ "Play saved games" of `Zenith2/Docs/Design/ONLINE/Backends.md`). Commit serials order commits only along one history, and these two backends are exactly the cases where there can be two (each machine's, or the phone's and the server's), so the model asks a lineage question that serial magnitude cannot answer: is this device's base commit in the history of the index it is reading? The index carries a log of its last 128 commits for that purpose (§ "Hashes and identities"), and the log is read as positive evidence only. What then holds: an automatic download over a local save and a local deletion happen only with proof that the device's base is in the read index's history (the base's own commit pair in the log; for a deletion, alternatively a deletion record of exactly the base's bytes); a re-commit over another device's entry happens only over this device's own base bytes or previous base (one lost commit); every other case, an index on another history, a repaired index, or one so far ahead that its log no longer reaches the base, is a conflict on both backends, never a silent overwrite or deletion by the table itself: for a slot whose kind's policy is ASK (MANUAL and QUICK by default) the player decides and nothing is overwritten or deleted until then; under an automatic policy the engine settles the conflict at once (§ "Automatic resolution policies"): `PREFER_NEWEST_UTC`, the default for AUTOSAVE and CHECKPOINT, keeps the side with the larger UTC seconds and overwrites the other (a local loser keeps SAVE's one `.bak` generation, a cloud loser's object becomes garbage), and a deletion reason always keeps the existing copy; a slot whose committed base the index has lost while the index is still on the device's history (no entry, an entry of the base's own bytes, or of the previous base) is a **lost commit** and is re-uploaded (§ "Three-way decision table"). The residuals are stated with the table.
- **Deletion is explicit.** A cloud deletion is a **deletion record**, an index entry flagged deleted with the commit serial of the deleting write, never the absence of an entry; a device deletes its local copy only on a deletion record committed after its own base in a history proven to hold that base, or on a record of exactly its base's bytes (row 12), and a local slot counts as deleted only when SAVE's deletion listener said so (a tombstone, § "Deletion propagation"). Absence of evidence is never evidence: an index that cannot be read or trusted makes every C UNKNOWN, never empty, and UNKNOWN blocks every destructive action (§ "Index view"); a local export that fails makes L UNKNOWN.
- **Concurrency.** The index is written with a conditional write (expected revision) where the backend supports it (offline, Play); Steam's remote storage is single-writer within a session and synchronised by the Steam client at launch and exit, so the condition is checked against the session's own last read (`Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam cloud storage"). Two commits of one slot made concurrently on top of the same index carry the same commit serial with different hashes, a conflict (row 22); commits on two histories of unequal length are conflicts too (rows 14 and 23 … 26), because the device's base is not in the other history's log.
- **One transfer at a time.** The engine runs at most one transfer (an upload, a download, a reconcile-time export or a repair read) per session, in priority order, so its memory peak is one envelope (§ "Transfer memory").
- **Per profile.** The slot table, the sync manifest and every job belong to the bound save profile; a rebind discards them and loads the new profile's (`Zenith2/Docs/Design/ONLINE/Accounts.md` → "Rebind state per subsystem").

## Slot eligibility

1. Kinds: a slot synchronises when bit (kind − 1) of the catalogue's `m_uSyncedKindMask` is set (default MANUAL, QUICK and AUTOSAVE). `SAVE_KIND_RESUME` and `SAVE_KIND_REPLAY_CHECKPOINT` never synchronise: the resume state is bound to one device's process death, and replay checkpoints are not files.
2. Local state: SAVE listing states VALID and RECOVERABLE_FROM_BACKUP are eligible (the latter exports the backup's bytes, which are the slot's last valid save, and takes the backup override of § "Three-way decision table"); NEWER_VERSION and INCOMPATIBLE are `ONLINE_CLOUD_SLOT_SKIPPED_VERSION` and never touched; CORRUPT counts as locally absent without a tombstone, so it can only lead to a download or to no action, never to a cloud deletion.
3. Cloud state: an index entry whose save format version is newer or of another major than this build's (`Zenith_GetCurrentSaveVersion()`) is `ONLINE_CLOUD_SLOT_CLOUD_NEWER`: never downloaded, never overwritten by an upload (an upload would destroy another device's newer save) and shown to the game through `QueryCloudSlot`.
4. Size: a slot whose envelope (320 + file bytes) exceeds `min(GetMaxCloudObjectBytes(), m_ulMaxTransferBytes)` is `ONLINE_CLOUD_SLOT_TOO_LARGE`.
5. Count: the union of eligible local slots and cloud index entries, ordered by the larger of their local and cloud UTC seconds (descending), then slot name (ascending byte order), keeps its first `m_uMaxSlots` slots; the rest are `ONLINE_CLOUD_SLOT_NOT_SYNCED` until a slot ahead of them disappears.

## Hashes and identities

- **L** is `Zenith_Sha256Digest` of the local slot's exported bytes, or ABSENT when SAVE lists no file of the slot (or lists it CORRUPT), or UNKNOWN when the reconcile's export of a listed slot fails (any SAVE error from `BeginExportSlot` or `TakeExportedSlot`). Fast path: when SAVE's listing reports the same header sequence and file bytes that the sync manifest recorded for the slot's base (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Sync manifest" offsets 72 and 80) and the manifest state is IN_SYNC, L equals the base without reading the file; any rewrite changes the sequence, because SAVE's sequences strictly increase per profile.
- **C** is read from the index when the view is KNOWN: an **entry** (payload hash y, commit serial c, the repaired flag), a **deletion record** (the deleted hash, commit serial d), or ABSENT (neither); every C is ABSENT when the view is EMPTY and UNKNOWN when the view is UNKNOWN (§ "Index view").
- **B** is the manifest's base for the slot (payload hash b and **base serial** s_B, the commit serial of the index entry that recorded b), or NONE; with it the manifest keeps the **previous base token**, the commit token of the base the slot held before b (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Sync manifest" offset 96). **T**, the tombstone, is the manifest entry's flag recording that SAVE reported the slot deleted after it was synced (§ "Deletion propagation").
- An entry is **at or after the base** when c ≥ s_B, **before the base** when c < s_B; a deletion record is **after the base** when d > s_B. These compare positions only within one history; every row of the decision table states its own serial condition.
- **Lineage.** The **commit token** of a commit of slot S with hash h is fixed by the slot, the hash and whether it was a deletion (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Cloud index", "Commit tokens and the commit log"); the index's commit log holds (commit serial, token) for its last 128 commits and the **log floor**, the greatest commit serial it no longer represents. The log is positive evidence only, and the lineage takes one of three values:
  - `DESCENDS`: there is no base, or the log holds the pair (s_B, the token of (S, b, live)). This is the only value that proves the base is in the read index's history.
  - `HORIZON`: neither, and s_B ≤ the log floor: the base is older than the log (128 or more committing writes of any slot ago, or older than an index repair, whose log starts empty), so it can be neither proven nor disproven.
  - `DIVERGED`: otherwise: s_B above the floor with the pair absent (another history, an index whose serial is below s_B included).
  Rows 8 and 12 act only on `DESCENDS` (row 12 also on a record of exactly b); `HORIZON` and `DIVERGED` both send those cases to the conflict rows 14, 23 and 24 and decide every input identically. The index header's repair-floor flag is diagnostic only and changes no decision: a `HORIZON` conflict counts `online.cloud.horizon_conflicts` whichever floor it is below, and the flag only selects its log line (§ "Conflict detection"). The cloud entry **is the previous base** when the token of (S, y, live) equals the manifest's previous base token. The previous base is the base this device held before its current one, not necessarily an ancestor of it: after its own commit it is the commit's parent (so an entry equal to it is one lost commit of this device), and after a KEEP_CLOUD or FIRST_SYNC resolution, a download or a repair adoption it is the version this device's current base replaced, possibly one the player discarded; rows 19 and 20 acting over it re-commit or delete over this device's own earlier base, re-applying that earlier choice, never over a version the device has not held.
- The cloud copy's identity is its payload hash, not its revision or its object name: two devices uploading identical bytes agree (rows 4 and 9 of the decision table), and a revision change with an unchanged hash is not a change.

## Index view

Each reconcile classifies what it read of `z2.index` into one view; nothing in the cloud engine reads C except through it.

| View | Cause | When | Consequence |
|---|---|---|---|
| KNOWN | — | `CLOUD_READ` of `z2.index` completes OK, `Zenith_ParseOnlineCloudIndex` accepts it (rules 1 … 12), its version is 1, and the caller check of its profile key against the active profile passes | the decision table decides every slot; garbage collection may run |
| EMPTY | — | `CLOUD_READ` completes `NOT_FOUND` and a `CLOUD_LIST` of the same reconcile completes OK listing no object whose name begins `z2.` | every C is ABSENT; when the manifest holds a base for any slot this is a **cloud reset** (the player cleared the cloud): every base and tombstone of the manifest is dropped in one atomic manifest write, logged `online.cloud: cloud is empty; re-seeding from this device`, counted `online.cloud.cloud_resets`, so every local slot decides row 2 (upload) and nothing local is ever deleted |
| UNKNOWN | READ_FAILED | `CLOUD_READ` completes with any status but OK or `NOT_FOUND`, or the `CLOUD_LIST` that a `NOT_FOUND` requires fails | retried with the backoff at the next reconcile; cloud status unavailable, reason network |
| UNKNOWN | INVALID | the parser refuses the index (`ONLINE_ERROR_CORRUPT`, the rule in `m_uDetail`) | resolved only by § "Index repair"; cloud status index unreadable, reason index invalid; logged once per session `online.cloud: cloud index invalid (rule <n>); cloud sync suspended until repaired` |
| UNKNOWN | FOREIGN_PROFILE | the index parses but its profile key differs from the active profile | resolved only by § "Index repair"; cloud status index unreadable, reason index foreign |
| UNKNOWN | MISSING | `CLOUD_READ` completes `NOT_FOUND` while `CLOUD_LIST` lists at least one object whose name begins `z2.` | resolved only by § "Index repair"; cloud status index unreadable, reason index missing |
| UNKNOWN | NEWER_FORMAT | the object is at least 48 bytes long, begins with the magic `ZCIX`, and its version (bytes 4 … 5) is greater than 1 (a newer release wrote it; every older version is read); any other object that fails the parser, a wrong magic or a short object included, is INVALID whatever its bytes 4 … 5 hold | never resolved by this build: every slot is `CLOUD_NEWER`, the index is never written, repaired or collected by this build; cloud status read only, reason index newer |

Two residuals of the NEWER_FORMAT rule are accepted, both safe because every UNKNOWN view blocks every destructive action: (a) a newer release's index larger than the 34,880-byte buffer fails its read (`BACKEND_ERROR`, completion rule B6) and is classified READ_FAILED, so the status reports unavailable (network) and the reconcile retries with the backoff instead of reporting read only; no backend request reads a prefix of an object, so the header cannot be classified on its own; (b) a single flipped bit in bytes 4 … 5 of a valid version-1 index reads as a newer version and locks this build out of the cloud (read only) until a newer release or a manual clear of the cloud, a residual chosen over letting a corrupt version field reach the INVALID path, from which a repair would rewrite a possibly genuine newer index.

Under every UNKNOWN view, until a reconcile classifies the view KNOWN or EMPTY: no `DELETE_LOCAL`, no `DELETE_CLOUD`, no download, no upload, no index write of any kind, no garbage collection; every eligible slot that no local override settles takes `ONLINE_CLOUD_SLOT_CLOUD_UNKNOWN` (NEWER_FORMAT: `CLOUD_NEWER`); a slot already in CONFLICT keeps its recorded conflict; a resolution request answers `REFUSED`. Local saves proceed normally (SAVE is unaffected), and every listener trigger still records tombstones and runs a reconcile, which stays blocked while the view does.

A corrupt index, an index written by a newer release, an index of another profile and a cleared index with surviving objects therefore delete nothing on any device: each device that reads one stops, and the cloud keeps every object it held.

## Three-way decision table

`Zenith_DecideOnlineSync(inputs)` is a pure function (`Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Internal declarations", `Zenith_OnlineSyncInputs` and `Zenith_OnlineSyncAction`); every row and every override has a unit test (`Zenith2/Docs/Design/ONLINE/Tests.md` → "Cloud model tests").

Overrides, applied before the table in this order:

1. L UNKNOWN (the export failed): state ERROR with the backoff; no action.
2. C UNKNOWN (the index view is UNKNOWN): state `CLOUD_UNKNOWN` (NEWER_FORMAT: `CLOUD_NEWER`); no action.
3. A SKIPPED_VERSION, CLOUD_NEWER or TOO_LARGE slot takes that state; no action.
4. A slot in CONFLICT whose inputs are unchanged since the conflict was recorded stays in CONFLICT (no new notification).
5. The backup override: a RECOVERABLE_FROM_BACKUP slot (L is the backup's hash) with L ≠ B and C an entry y = b, whatever its commit serial and lineage, downloads (D1 … D5) instead of taking row 7 or row 19: the cloud may hold the only remaining copy of the newer synced state the damaged current file had, uploading the older backup would destroy it, and downloading the base's own bytes overwrites nothing that is not already this device's base.
6. L present clears a stale tombstone (SAVE's listener reported a write after the deletion).
7. The repair override: C is an entry flagged repaired (written by § "Index repair", cleared by any subsequent commit of the slot) and B is not NONE. L absent: T set → CONFLICT, reason DELETED_LOCALLY_CHANGED_IN_CLOUD (never an automatic cloud deletion); T clear → DOWNLOAD (a restore). L present: y = L → ADOPT (B := y, s_B := c; this covers y = b = L); y = b ≠ L → UPLOAD (the entry equals the base, so nothing is overwritten that is not already this device's base, and the commit clears the flag); otherwise CONFLICT, reason BOTH_CHANGED (never a download: the repair may have chosen a copy that was never committed). A repaired entry therefore never leads to `DELETE_LOCAL`, `DELETE_CLOUD` or a download over a local file, and a locally changed slot whose base the repair chose uploads at its first reconcile.

In the table, every row with a base states its serial condition; "record" is a deletion record and h its deleted hash; b and s_B are the base's hash and base serial; `DESCENDS`, `HORIZON` and `DIVERGED` are the lineage of § "Hashes and identities" ("not `DESCENDS`" is either of the other two); "the previous base" means the entry's token equals the manifest's previous base token.

| # | B | T | L | C | Action | Conflict reason |
|---|---|---|---|---|---|---|
| 1 | NONE | — | absent | absent or record | none | — |
| 2 | NONE | — | x | absent or record | UPLOAD | — |
| 3 | NONE | — | absent | entry y | DOWNLOAD | — |
| 4 | NONE | — | x | entry x | ADOPT (B := x, s_B := c) | — |
| 5 | NONE | — | x | entry y ≠ x | CONFLICT | FIRST_SYNC |
| 6 | b | — | b | entry b, c ≥ s_B | none (s_B := c when c > s_B) | — |
| 7 | b | — | x ≠ b | entry b, c ≥ s_B | UPLOAD | — |
| 8 | b | — | b | entry y ≠ b, c > s_B, `DESCENDS` (the log holds the base's pair) | DOWNLOAD | — |
| 9 | b | — | x ≠ b | entry x (any c) | ADOPT (B := x, s_B := c) | — |
| 10 | b | — | x ≠ b | entry y ≠ b, y ≠ x, c > s_B | CONFLICT | BOTH_CHANGED |
| 11 | b | set | absent | entry b, c ≥ s_B | DELETE_CLOUD (writes a deletion record) | — |
| 12 | b | — | b | record with d > s_B that is `DESCENDS` or has h = b | DELETE_LOCAL | — |
| 13 | b | set | absent | entry y ≠ b, c ≥ s_B | CONFLICT | DELETED_LOCALLY_CHANGED_IN_CLOUD |
| 14 | b | — | x ≠ b | record with d > s_B; or record with d ≤ s_B, not `DESCENDS` | CONFLICT | CHANGED_LOCALLY_DELETED_IN_CLOUD |
| 15 | b | any | absent | record (any d), or absent with T set | FORGET (drop the manifest entry and its tombstone) | — |
| 16 | b | clear | absent | entry b, c ≥ s_B | DOWNLOAD (the local file vanished outside SAVE: restore it) | — |
| 17 | b | clear | absent | entry y ≠ b, c ≥ s_B | DOWNLOAD (vanished locally, changed in the cloud: restore the cloud copy) | — |
| 18 | b | — | b or x | absent (no entry and no record) | UPLOAD (lost commit: the index lost the slot; re-commit) | — |
| 19 | b | — | b or x | entry b with c < s_B; or record with d ≤ s_B, `DESCENDS`; or entry y ≠ b, y ≠ L, c < s_B that is the previous base (the base this device held before, § "Hashes and identities") | UPLOAD (lost commit or regressed index: re-commit over the older entry or record) | — |
| 20 | b | set | absent | entry with c < s_B that is b or the previous base (the base this device held before) | DELETE_CLOUD (the deletion this device intended, over a version this device itself held) | — |
| 21 | b | clear | absent | entry y (b included) with c < s_B, or absent | entry: DOWNLOAD (restore the newest copy that still exists); absent: FORGET | — |
| 22 | b | — | b or x | entry y ≠ b, y ≠ L, c = s_B (a concurrent commit at the base's own serial) | CONFLICT | BOTH_CHANGED |
| 23 | b | — | b | entry y ≠ b, c > s_B, not `DESCENDS` (a longer history this device's base is not proven to be part of) | CONFLICT | BOTH_CHANGED |
| 24 | b | — | b | record with d > s_B, not `DESCENDS`, h ≠ b; or record with d ≤ s_B, not `DESCENDS` (another history deleted a copy this device may never have held) | CONFLICT | CHANGED_LOCALLY_DELETED_IN_CLOUD |
| 25 | b | — | b or x | entry y ≠ b, y ≠ L, c < s_B, not the previous base (a shorter history) | CONFLICT | BOTH_CHANGED |
| 26 | b | set | absent | entry y ≠ b with c < s_B, not the previous base | CONFLICT | DELETED_LOCALLY_CHANGED_IN_CLOUD |

What holds. `DELETE_LOCAL` (row 12) is the only rule that removes a local save, and it needs positive evidence: a deletion record committed after this device's base in an index whose commit log holds the base's own commit, or a record that deleted exactly the bytes this device holds (h = b, so nothing the deleter did not see is lost). A download over a local file (row 8) needs an entry committed after the base in an index whose log holds the base's commit. Neither is ever taken on serials alone: an index lacking an entry, an index of another history (Steam's conflict dialog kept another machine's index, Play's offline commit lost to the server, the cloud was cleared), a repaired index, and an index so far ahead that its log no longer reaches the base all lead to a conflict instead (rows 14, 23 and 24). A re-commit over another device's entry or record (row 19) happens only over this device's own base bytes, its previous base, or a record of its own proven history; any other entry of a shorter or concurrent history is a conflict (rows 22, 25, 26). A conflict is where the table stops: for a slot whose kind's policy is ASK (MANUAL and QUICK by default) the player decides and nothing is overwritten or deleted until then; under an automatic policy the engine settles the conflict at once (§ "Automatic resolution policies"): `PREFER_NEWEST_UTC`, the default for AUTOSAVE and CHECKPOINT, keeps the side with the larger UTC seconds and overwrites the other (a local loser keeps SAVE's one `.bak` generation, a cloud loser's object becomes garbage), and a deletion reason always keeps the existing copy. Rows 16, 17 and 21 only restore a slot whose local file is gone. `DELETE_CLOUD` (rows 11, 20) only ever follows SAVE's own report that the game deleted the slot, and only over this device's base or previous base. Rows 8, 12, 14, 19 (its record branch), 23 and 24 read the lineage; rows 19 (its entry branches), 20, 25 and 26 read the previous-base flag; every other row reads neither, so rows 1 … 7, 9 … 11, 13, 15 … 18, 20 … 22, 25 and 26 decide the same under all three lineage values.

Residuals: (1) a base older than the log (128 or more committing index writes of any slot since the base's own; a write that only drops deletion records adds no pair and does not count) cannot be proven to descend, so the download (row 8), local deletion (row 12, unless the record is of b) or record re-upload (row 19) it would otherwise take is a conflict (rows 23, 24, 14): a conflict for a device that stayed away that long or whose base predates an index repair: for an ASK slot one extra prompt and never a loss, and for a deletion reason the existing copy is kept; for a slot under an automatic policy the policy settles it like any other conflict (`PREFER_NEWEST_UTC` may then overwrite the side with the smaller UTC seconds, as above). Accepted cost (orchestrator ruling ONLINE-FINAL-RULINGS (4)): the common case is an idle device whose local file did not change (L = b) and whose base the cloud has simply moved past, after 128 or more committing writes of any slot on other devices (with the default autosave kinds, about one long session elsewhere) or after an index repair (every adopter of the repaired entry, at the next change another device commits); it gets one conflict per slot changed meanwhile although Keep cloud is the obvious answer: a prompt for an ASK slot, an automatic settlement for an AUTOSAVE or CHECKPOINT slot. No automatic copy-then-download path replaces that prompt; the rows that read no lineage stay safe without it, because each touches no copy, restores a slot whose local file is gone, or overwrites or deletes only bytes this device held as its base or previous base; (2) a lost commit more than one commit deep (the surviving entry is neither the base nor the previous base) is a conflict, one extra prompt, not a re-upload; (3) a deletion can be undone without losing a copy: a record dropped by garbage collection (§ "Garbage collection"), or a deletion commit undone with its history (Steam's dialog, Play's offline commit; the deleting device dropped its base at `DELETE_CLOUD`, so another history's entry reaches it as row 3 and is downloaded back), resurrects the slot.

## Reconcile algorithm

A reconcile compares every eligible slot (a full reconcile) or one slot (a targeted reconcile, § "Polling and triggers"). Preconditions: the session is SIGNED_IN, the cloud is enabled and not degraded, the backend has `uONLINE_CAPABILITY_CLOUD`, the OS session is not locked, and no reconcile is in flight (a trigger during one sets a flag that runs one more full reconcile after it).

1. `Zenith_SaveSystem::BeginListSlots()`; at completion `TakeSlotList` into the engine's arena (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Listing slots").
2. `ONLINE_REQUEST_CLOUD_READ` of `z2.index` into the 34,880-byte index buffer (the index limit), keeping its revision R and serial; a full reconcile, and any reconcile whose read completed `NOT_FOUND`, then issues `ONLINE_REQUEST_CLOUD_LIST`; classify the view (§ "Index view").
3. Build the slot set (§ "Slot eligibility"), and for each slot compute L (fast path; else `BeginExportSlot` into the transfer buffer, which takes the transfer reservation of § "Transfer memory" and counts as the session's one transfer, then hash and release it; one export at a time; an export failure makes L UNKNOWN), C, B and T.
4. Decide each slot with the table; set its state (`LOCAL_AHEAD` for UPLOAD, `CLOUD_AHEAD` for DOWNLOAD, `DELETING_CLOUD`, `DELETING_LOCAL`, `CONFLICT`, `IN_SYNC`, `CLOUD_UNKNOWN`, `ERROR`) and queue its action.
5. ADOPT and FORGET rows, base-serial advances (row 6) and a cloud reset update the manifest at once (one atomic write for the whole reconcile); an ADOPT that changes the base hash first records the old base's token as the previous base token.
6. Conflicts: apply the slot kind's policy (§ "Automatic resolution policies"); ASK queues `ONLINE_NOTIFY_CLOUD_CONFLICT` to every PLAY world whose previous-profile flag is clear (`Zenith2/Docs/Design/ONLINE/Accounts.md` → "Notifications" step 1) and leaves the slot in CONFLICT.
7. Run the queued actions one at a time in this priority: downloads first (another device's progress is what a returning player most likely wants), then uploads, then deletions, each group in slot-name order.
8. When the view is KNOWN and the reconcile is full, run § "Garbage collection".
9. When the queue is empty, complete every `SyncCloudNow` script request waiting on this reconcile with the conflict count and the transfer count.

Cost: one listing (at most 128 KiB read per local slot, SAVE's rule), one index read, one cloud listing per full reconcile, one export per changed local slot (an upload exports again at U2), the transfers, and at most 8 garbage deletions; O(slots + listed objects) decisions.

## Upload sequence

For a slot in `LOCAL_AHEAD` (state `UPLOADING` while it runs):

| Step | Action | Outcome on failure |
|---|---|---|
| U1 | Reserve the transfer memory (§ "Transfer memory") | refused: stay `LOCAL_AHEAD`; retry at the next `Pump` |
| U2 | `BeginExportSlot(slot)`; at completion copy the verified file bytes into the envelope buffer at offset 320 (`TakeExportedSlot`); compute L | SAVE error: `ERROR` with the backoff |
| U3 | Re-read the index (CLOUD_READ, keep its revision R, serial s and commit log; 0 and an empty log for an EMPTY view) and classify the view | an UNKNOWN view: the upload ends, the slot takes `CLOUD_UNKNOWN`; a read failure: `ERROR` with the backoff |
| U4 | Re-decide the slot with the fresh C: anything but UPLOAD or UPLOAD_LOST_COMMIT (another device changed the cloud meanwhile) ends the upload and applies the new row | — |
| U5 | Name the object N = `z2.<slot>.<s>.<h>` (s as 16 lower-case hexadecimal digits, h the first 16 lower-case hexadecimal digits of L); write the envelope header (`Zenith_WriteOnlineEnvelope`: kind, this backend, slot, active profile key, sequence, UTC, playtime, format version, device ID, base serial s, display name, payload hash L) and `ONLINE_REQUEST_CLOUD_WRITE` of N with an expected revision of none (create only) | `REVISION_MISMATCH` (N exists): `CLOUD_READ` N and compare its payload hash with L; equal (this device's own interrupted attempt, or another writer's identical bytes at the same serial): continue at U6; different (a 64-bit prefix collision): `ERROR`, counted `online.cloud.name_collisions`, retried after the backoff at a fresh serial; `QUOTA`: `ERROR` and `ONLINE_NOTIFY_CLOUD_STATUS` quota exceeded; `TOO_LARGE`: `TOO_LARGE`; other: `ERROR` with the backoff |
| U6 | Write the index with the slot's entry (or deletion record) replaced by an entry (payload hash L, base serial s, commit serial s + 1, repaired flag clear, the envelope's metadata), index serial s + 1, the commit log read at U3 with the pair (s + 1, the token of (slot, L, live)) appended (the oldest pair dropped into the log floor beyond 128, which also clears the repair-floor flag), as `ONLINE_REQUEST_CLOUD_WRITE` of `z2.index` with expected revision R (none for an EMPTY view). A live entry is never removed for capacity: a write that would leave more than 64 live entries (a new slot when the index already holds 64) is not made | `REVISION_MISMATCH`: back to U3, at most 3 attempts (each fresh serial names a fresh object), then `ERROR` with the backoff; index full: the upload ends as `ERROR` with the backoff (after which the slot is `LOCAL_AHEAD` again), CLOUD_STATUS quota exceeded (reason quota) is queued, `online.cloud: index holds 64 slots; '<slot>' not uploaded` is logged and `online.cloud.index_full` counted; only a deletion of a cloud slot frees a place; other: `ERROR` |
| U7 | Commit locally: when L differs from the old base, previous base token := the token of (slot, old base, live) (0 when there was no base); base := L, base serial := s + 1, local sequence and bytes recorded, state `IN_SYNC`; write the manifest atomically | the manifest write failing leaves the old base on disk: the next reconcile sees row 9 (ADOPT) and repairs it |

The upload is committed at U6. Until then N is referenced by no index; it never replaces the object the slot's committed entry references, so that entry's bytes stay downloadable and verifiable throughout, whatever another device does meanwhile. After U6 the object the entry used to reference is superseded and becomes garbage, deleted by § "Garbage collection" after the grace generation; an object written at U5 whose U6 lost the race is garbage the same way.

## Download sequence

For a slot in `CLOUD_AHEAD` (state `DOWNLOADING` while it runs), or the cloud half of a KEEP_CLOUD or KEEP_BOTH resolution:

| Step | Action | Outcome on failure |
|---|---|---|
| D1 | Reserve the transfer memory for the index entry's payload bytes plus 320 | refused: retry at the next `Pump` |
| D2 | `ONLINE_REQUEST_CLOUD_READ` of the entry's object `z2.<slot>.<base serial>.<hash prefix>` | `NOT_FOUND`: the index read is stale; re-read it (a targeted reconcile); when a KNOWN index still references the missing object and L equals the entry's hash, the object is healed (§ "Garbage collection"); otherwise the slot is `ERROR`, logged `online.cloud: '<slot>' references a missing cloud object`, counted `online.cloud.missing_objects`, and never treated as absent; other: `ERROR` with the backoff |
| D3 | `Zenith_ParseOnlineEnvelope`, then rule 5 (the payload hash equals the index entry's), rule 7 (the slot equals the entry's and the name's), rule 14 (the base serial equals the name's and the entry's), rule 9 (the profile key equals the active profile), rule 10 (the save format version) | a failure: `ERROR`, logged `online.cloud: cloud copy of '<slot>' rejected (rule <n>)`, counted `online.cloud.rejected_objects`; the object is never imported and never deleted by the download (another release may read it) |
| D4 | `BeginImportSlot(target slot, payload)` (it takes no desc): SAVE verifies the whole image as at a load, checks the ZSAV `zenith.profile` pair against the active profile (rule 13), refuses a newer or other-major version, rewrites the METADATA slot name when the target differs (KEEP_BOTH), and writes it with its journaled sequence, so the slot's previous local file becomes its `.bak` (assumption pending SAVE, row ON1 (b)) | a SAVE error: `ERROR`; `SAVE_ERROR_IDENTITY_MISMATCH` (another account's save): counted `online.cloud.rejected_objects` and never retried for that hash |
| D5 | Commit locally: when C differs from the old base, previous base token := the token of (slot, old base, live); base := C, base serial := the entry's commit serial, tombstone cleared, local sequence and bytes := the imported file's, state `IN_SYNC`; write the manifest; queue `ONLINE_NOTIFY_CLOUD_SLOT_REPLACED` (cause download, keep cloud or keep both) to every PLAY world whose previous-profile flag is clear | as U7 |

A download never changes a running world: the slot file is replaced on disk, and a game that is playing the slot learns of it from `Online.CloudSlotReplaced` and decides whether to reload it. SAVE's slot listener reports the import's write as `OnSlotWritten`, which the session recognises as its own (the slot is `DOWNLOADING`) and does not treat as a local change.

## Deletion propagation

<!-- assumption: pending SAVE (row ON1 (c)) -->
SAVE calls `Zenith_SaveSlotListener::OnSlotDeleted(slot, kind)` on the main thread in `PumpMainThread` once for every slot its `BeginDelete` removed, and for no other reason; a file SAVE did not delete is never reported.
<!-- /assumption -->

- **Tombstones.** `OnSlotDeleted` (received by the session's `Zenith_OnlineSaveListener`, registered by `Create` and cleared first by `BeginShutdown`, `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Internal declarations") of a slot whose entry in the sync manifest of SAVE's active profile has a base, and which is not in `DELETING_LOCAL` (the session's own deletion), sets the entry's tombstone flag T and writes the manifest atomically at once; then a targeted reconcile runs. This is the only way T is set: a local file that vanishes without SAVE's deletion (an antivirus quarantine, a user deleting the save folder, a restore tool) is never a deletion; the reconcile sees it absent with T clear and restores it from the cloud (rows 16 and 17). Tombstones are recorded whenever the session is not REPLAY, signed out included (the manifest of the active profile is written even while the cloud engine does not run); a deletion made while the Online module is absent leaves no tombstone, so the next reconcile restores that slot, the safe direction. `OnSlotWritten` of a slot with T set clears T.
- **DELETE_CLOUD** (rows 11 and 20: the game deleted the slot locally after it was synced, T set): re-read the index (U3; the view must be KNOWN), re-decide (U4), write the index with the slot's entry replaced by a deletion record (flag deleted, the deleting writer's backend kind, the deleted payload hash b, commit serial s + 1, the deletion's UTC seconds; conditional, serial s + 1, the pair (s + 1, the token of (slot, b, deleted)) appended to the commit log, as U6), then drop the manifest entry and its tombstone. The deletion commits at the index write; the object the entry referenced becomes garbage (§ "Garbage collection"). A deletion record occupies an index entry until garbage collection drops it (at most 64 records; when a write would exceed 64, the record with the smallest commit serial is dropped at that write, which can only let a long-offline device re-upload the deleted slot, the safe direction).
- **DELETE_LOCAL** (row 12: a KNOWN index holds a deletion record committed after this device's base, and either its commit log holds the base's commit or the record deleted exactly the base's bytes, so another device deleted the copy this device holds): mark the slot `DELETING_LOCAL`; `Zenith_SaveSystem::BeginDelete(slot)` (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Slot policy"; SAVE's deletion order keeps nothing half-deleted); then drop the manifest entry and queue `ONLINE_NOTIFY_CLOUD_SLOT_REPLACED` with cause deleted in cloud.
- A slot that disappears from a KNOWN index with no deletion record is never "deleted in the cloud": it is a lost commit (rows 18 and 21); an object that remains without an entry is garbage once the grace rules of § "Garbage collection" hold.

## Garbage collection

At the end of a full reconcile whose view is KNOWN, the engine deletes cloud objects that no committed index can reference any more, and drops deletion records that have served their purpose. An object listed by the reconcile's `CLOUD_LIST` is garbage only when all of these hold:

1. Its name is an envelope name `z2.<slot>.<b>.<h>` (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Cloud object names"); any other object, `z2.index` included, is never deleted (it may belong to a newer release).
2. The KNOWN index has no entry with that slot, base serial b and hash prefix h.
3. The grace generation has passed: b + 2 ≤ the index serial. An object written at U5 with base serial b can be committed only by an index write conditional on the index of serial b, which produces serial b + 1; once the index has moved past b + 1 without referencing the object, no writer can come to reference it on a backend whose conditional write is atomic at the service (a writer whose conditional write lost goes back to U3 and names a fresh object).
4. A backstop for the two non-atomic backends only, not a guarantee: its modification time from the listing is at least 24 hours old. The time is the backend's own (Steam: the local cloud file's timestamp; Play: the snapshot's server-side last-modified time; offline: the file's modification time), so a skewed clock weakens the backstop and never condition 3. Collecting an object whose commit a Steam dialog or a Play reconnect undid loses no save, because no device downloads over or deletes a local save without its base's commit in the read log (or, for a deletion, a record of exactly its bytes), and the undone commit is in no surviving log: the device that made it either re-uploads it under a fresh name (rows 18 and 19) or keeps it as the local side of a conflict (rows 14 and 22 … 26), so its bytes survive locally until the player decides (an ASK slot) or the slot kind's automatic policy keeps one side (§ "Automatic resolution policies"). A device on another history can, on these two backends, collect an object that a different history's kept index still references; that index's devices then find the object missing: a device holding the identical bytes heals it (below), any other reports the missing object as ERROR, never as an absence.

Up to 8 garbage objects per reconcile are deleted, in ascending name order, each with `ONLINE_REQUEST_CLOUD_DELETE` conditional on the revision the listing reported (an object rewritten since is not deleted); a failure is logged and left for the next reconcile. Counted `online.cloud.objects_collected`.

**Healing a missing object.** In a full reconcile whose view is KNOWN, an entry whose object the listing lacks, of a slot that is IN_SYNC with L = y, is healed: the device writes its local bytes as the same object name (U1, U2, U5 create-only), with no index write, counted `online.cloud.objects_healed`. A slot whose entry's object is missing and whose L differs is ERROR (§ "Download sequence" D2).

A deletion record is dropped when its commit serial d satisfies d + 2 ≤ the index serial (so at least one further index generation exists) and its UTC seconds are at least 30 days old: the reconcile then writes the index without it (conditional, serial + 1, the commit log copied unchanged, at most one such write per reconcile, all droppable records at once), counted `online.cloud.records_dropped`; a write that would hold a 65th record drops the one with the smallest commit serial at that write. The retention bound is therefore min(30 days on the deleting device's UTC clock, 64 subsequent deletions): a record dated in 1970 by a skewed clock is droppable two serials after it was written, one dated in the future is kept until evicted. What a drop can lose is the deletion, never a copy: a device whose base predates the record and that reconciles only after the drop sees the slot absent and re-uploads it (row 18), so the slot reappears on every device, and a device with edits made after the deletion is not asked (row 14 or 24), it re-uploads.

Garbage collection never runs under an UNKNOWN or EMPTY view, never deletes a referenced object, never drops a record younger than its grace, and never runs on a backend that lacks `uONLINE_CAPABILITY_CLOUD_CONDITIONAL_WRITE`.

## Index repair

An index in an UNKNOWN view of cause INVALID, FOREIGN_PROFILE or MISSING is repaired only on request, because writing an index could discard the evidence a newer release or another profile left: through `RequestCloudIndexRepair()` (presentation code: the editor panel, the console command `online.cloud.repairIndex`) or `Online.RepairCloudIndex(ctx)` from a script, which a game offers when `Online.CloudStatus` reports `IndexUnreadable`. Any other view answers `REFUSED` (script) or `ONLINE_ERROR_NOT_SUPPORTED` (presentation). The repair runs as the session's one transfer:

1. `CLOUD_LIST`; keep each object's revision and the revision R of the current `z2.index` (none when MISSING).
2. For each listed object whose name is an envelope name, in ascending name order: reserve its stored bytes, `CLOUD_READ` it whole, `Zenith_ParseOnlineEnvelope`, rule 5 (payload hash), rule 7 (slot equals the name's), rule 14 (base serial equals the name's) and rule 9 (profile key equals the active profile); keep its header when every check passes, otherwise skip it (it is left in place, unreferenced, and nothing in the repair deletes it); release the buffer.
3. Per slot, choose among the kept headers the one with the largest base serial, then the larger UTC seconds, then the larger name bytes. The choice is a best guess, not a commit record: two kept headers at the same largest base serial are a race pair of which at most one was ever committed, and an old index's deletion records are gone.
4. Build the index: version 1, the active profile key, the repaired flag set in the index header, one entry per chosen header with the entry's repaired flag set (at most 64, by slot name; beyond the slot cap the slots with the largest UTC seconds are kept, the others stay unreferenced), no deletion record, serial = 1 + the largest base serial kept (1 when none), every entry's commit serial equal to that serial, an empty commit log with the log floor equal to that serial and the header's repair-floor flag set (diagnostic only, § "Hashes and identities"). Every device's base is then at or below the floor, so no pre-repair base can be proven `DESCENDS`: while an entry is flagged repaired the repair override decides, and after another device commits over it rows 23 and 24, not rows 8 and 12, apply to every pre-repair base, until that device's own next commit puts its base in the log.
5. `CLOUD_WRITE` of `z2.index` conditional on R; `REVISION_MISMATCH` (another writer moved the index) ends the repair with `BACKEND_ERROR` and runs a full reconcile; success logs `online.cloud: cloud index repaired (<n> slots)`, counts `online.cloud.index_repairs`, and runs a full reconcile.

What holds after the repair, on the repairing device and on every other device that reconciles against the repaired index: no local slot is deleted, because the repaired index holds no deletion record (row 12 cannot apply), a record another device writes after it deletes a pre-repair base only when it is of exactly that base's bytes (no pre-repair base is `DESCENDS`, so every other one is row 24), and a slot the repair could not represent is a lost commit (row 18: the device that holds it re-uploads it); no cloud slot is deleted automatically, because a repaired entry turns a tombstoned slot into a conflict (the repair override); no download replaces a local file whose base differs from the chosen copy, because the repair override makes it a conflict while the entry is flagged repaired and the empty repaired log makes it one (row 23: no pre-repair base has its pair in the log) after the slot is committed again, however many devices commit before the offline one returns; a device holding the chosen copy adopts it, and one whose base is the chosen copy but whose local file has changed uploads it. The repair itself deletes nothing and keeps every object it could verify. Cost: one listing plus one full read per listed envelope, one transfer at a time; afterwards, the first change another device commits to a slot is a conflict on every device whose base the repair set or predates (one prompt per slot, which the slot kind's automatic policy may settle).

## Conflict detection

A conflict is a row 5, 10, 13, 14, 22, 23, 24, 25 or 26 of the decision table, or a conflict of the repair override. A row 14, 23 or 24 conflict whose lineage was `HORIZON` also counts `online.cloud.horizon_conflicts` and logs `online.cloud: '<slot>' base is older than the cloud's commit log; asking`, or, when the index header's repair-floor flag is set, `online.cloud: '<slot>' base predates a cloud index repair; asking` (the flag's only effect). For each, the engine records the reason, both sides' UTC seconds and playtime (local from SAVE's listing, cloud from the index entry), the cloud copy's save display name (from the index entry's 128-byte display-name field, so no download is needed) and whether its writer device ID is this device's (the same device can conflict with itself after a reinstall that lost the manifest). The record also holds the **conflict token**, the inputs the record was built from: whether L is present and its hash's first 16 bytes, and the cloud side's kind (absent, entry or deletion record), its commit serial and its hash's first 16 bytes (the entry's payload hash or the record's deleted hash) (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Notification payloads"). The record is what `Online.CloudConflict` carries, token included, and what `QueryCloudSlot` reports. A conflict persists in the manifest across restarts (state CONFLICT and reason), is re-announced once per session start with the token of the first reconcile's inputs, and blocks every automatic action on its slot until resolved. Local saves into a conflicted slot proceed normally (SAVE is unaffected); they change L, which re-runs the decision and keeps the conflict. Whenever a reconcile finds the conflicted slot's inputs differ from its token (a local save, another device's commit, a deletion record), it refreshes the record (metadata and token) and queues `ONLINE_NOTIFY_CLOUD_CONFLICT` again under ASK, or re-applies the automatic policy to the new inputs, so a prompt never shows inputs older than the newest notification and a stale token is refused (§ "Resolution choices").

## Automatic resolution policies

The catalogue's policy for the slot's kind (`m_eManualPolicy` for MANUAL and QUICK, `m_eAutosavePolicy` for AUTOSAVE and CHECKPOINT) decides a conflict without asking:

| Policy | Rule (deterministic; ties fall through left to right) | Choice |
|---|---|---|
| `ONLINE_CONFLICT_POLICY_ASK` | none | the conflict waits for a resolution |
| `ONLINE_CONFLICT_POLICY_PREFER_NEWEST_UTC` | larger UTC seconds; larger playtime; local | KEEP_LOCAL or KEEP_CLOUD |
| `ONLINE_CONFLICT_POLICY_PREFER_LONGEST_PLAYTIME` | larger playtime; larger UTC seconds; local | KEEP_LOCAL or KEEP_CLOUD |
| `ONLINE_CONFLICT_POLICY_PREFER_LOCAL` | always | KEEP_LOCAL |
| `ONLINE_CONFLICT_POLICY_PREFER_CLOUD` | always | KEEP_CLOUD |

For the deletion reasons (13 and 14) the existing copy always wins under the four automatic policies: deleting a save automatically on a time comparison is never done. An automatic resolution is bound to the reconcile's inputs exactly as a player's is: it runs with the record's token and is refused as stale, then re-decided, when the inputs moved before it acts (§ "Resolution choices"). It queues `ONLINE_NOTIFY_CLOUD_SLOT_REPLACED` when it replaced the local slot, and logs `online.cloud: conflict on '<slot>' resolved by policy <policy>: <choice>`.

## Conflict resolution flows

### Slot sync state machine

`Zenith_OnlineCloudSlotState` × event → next state and action. Transient states are not persisted (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Sync manifest").

| State | Event | Next | Action |
|---|---|---|---|
| any persisted state | reconcile decides row 1, 6, 15, 21 (absent) | IN_SYNC (15 and 21: the slot leaves the table) | none, or FORGET |
| any persisted state | row 4, 9 | IN_SYNC | ADOPT, manifest write |
| any persisted state | row 2, 7, 18, 19 | LOCAL_AHEAD | queue the upload after `m_uUploadDelayMs` (rows 18 and 19 count `online.cloud.lost_commits`) |
| any persisted state | row 3, 8, 16, 17, 21 (entry), the backup override, or the repair override's restore | CLOUD_AHEAD | queue the download |
| any persisted state | row 11, 20 | DELETING_CLOUD | queue the cloud deletion |
| any persisted state | row 12 | DELETING_LOCAL | queue the local deletion |
| any persisted state except CONFLICT | row 5, 10, 13, 14, 22, 23, 24, 25, 26, or the repair override's conflicts | CONFLICT, or RESOLVING when an automatic policy chooses | record; notify under ASK |
| any state except a running transfer's | index view UNKNOWN other than NEWER_FORMAT | CLOUD_UNKNOWN | none (a CONFLICT keeps its record and returns to CONFLICT when the view is KNOWN again and the inputs are unchanged) |
| CLOUD_UNKNOWN | a reconcile with a KNOWN or EMPTY view | the row's state | as that row |
| any persisted state | L UNKNOWN (export failed) | ERROR | backoff |
| CONFLICT | the same conflict re-decided with inputs equal to its token | CONFLICT | none |
| CONFLICT | the conflict re-decided with inputs that differ from its token | CONFLICT | refresh the record and the token; notify again under ASK, re-apply the automatic policy otherwise |
| CONFLICT | resolution KEEP_LOCAL, KEEP_CLOUD or KEEP_BOTH whose token equals the record's | RESOLVING | § "Resolution choices" |
| CONFLICT | resolution whose token differs from the record's | CONFLICT | answer `STALE`; notify again with the record's token |
| RESOLVING | the compare before acting finds inputs that differ from the token (KEEP_LOCAL, KEEP_CLOUD) | CONFLICT | no action; refresh the record and the token, notify again; answer `STALE`; count `online.cloud.stale_resolutions` |
| CONFLICT | resolution DEFER | CONFLICT | answer OK; no action |
| CONFLICT | the inputs now match row 4 or 9 (the other device resolved it, or identical bytes) | IN_SYNC | ADOPT |
| LOCAL_AHEAD | its turn in the queue | UPLOADING | U1 |
| CLOUD_AHEAD | its turn | DOWNLOADING | D1 |
| UPLOADING | U7 done | IN_SYNC | — |
| UPLOADING | U3 read an UNKNOWN view | CLOUD_UNKNOWN | the upload ends; nothing written |
| UPLOADING | U4 re-decided another row | the new row's state | as that row |
| DOWNLOADING | D5 done | IN_SYNC | — |
| UPLOADING, DOWNLOADING, DELETING_CLOUD, DELETING_LOCAL, RESOLVING | a failure | ERROR | backoff `m_uRetryBackoffMs` doubling to 480,000 ms; the retry re-runs a targeted reconcile |
| ERROR | backoff elapsed | the targeted reconcile's row state | — |
| any | local listing NEWER_VERSION or INCOMPATIBLE | SKIPPED_VERSION | none |
| any | cloud entry newer, or index view NEWER_FORMAT | CLOUD_NEWER | none |
| any | envelope over the limit | TOO_LARGE | none |
| any | outside the slot cap or the kind mask | NOT_SYNCED | none |
| any transient state | fence (rebind) | (discarded with the slot table) | jobs cancelled; buffers detached until their requests complete |
| any transient state | shutdown deadline | (the persisted state it started from) | job cancelled |

### Resolution choices

`ResolveCloudConflict(slot, choice, token)`, from `Online.ResolveCloudConflict` (the token is the `Token` field of the `Online.CloudConflict` event, or of `QueryCloudSlot`'s result, passed back) or presentation code (`Zenith_OnlineCloudSlotStatus::m_xConflictToken`). A slot not in CONFLICT answers `NOT_IN_CONFLICT`; KEEP_BOTH on a slot that is not MANUAL, or when one side is absent, answers `REFUSED`; any choice while the index view is UNKNOWN answers `REFUSED`; DEFER answers OK whatever the token.

A resolution is bound to what the player saw:

1. At the call, a token that differs from the record's (the inputs moved and a newer notification is queued or already published) answers `STALE` (script status `"stale"`; presentation `ONLINE_ERROR_CONFLICT_STALE`): no action, and `ONLINE_NOTIFY_CLOUD_CONFLICT` is queued again with the record's token, so the screen refreshes.
2. KEEP_LOCAL and KEEP_CLOUD then compare the token with fresh inputs immediately before acting, in the same `Pump` that submits the action: KEEP_LOCAL after U2 (the exported L) and U3 (the re-read C), before U5, and again at every return to U3 after a `REVISION_MISMATCH`; KEEP_CLOUD after re-reading the index (as U3) and taking L (the fast path, or an export), before D1 or `BeginDelete`; the deletion-reason KEEP_LOCAL (DELETE_CLOUD) after its U3. A mismatch is stale: nothing is written or deleted, the slot returns to CONFLICT with the record and the token refreshed from the fresh inputs, the conflict is notified again, the script request completes `"stale"`, the log line `online.cloud: resolution of '<slot>' is stale; asking again` is written and `online.cloud.stale_resolutions` counted. Because D4's import or `BeginDelete` is submitted in the same `Pump` as the compare, a local save the game submits after it is ordered after the action (it replaces the imported file, which is still the cloud's copy); a KEEP_LOCAL upload sends the exported bytes it compared, and a save made after U2 is uploaded at the next reconcile.
3. KEEP_BOTH is unchanged: it keeps both sides whatever they are by then (the current cloud entry is downloaded into the new slot and the current local file uploaded), so it needs no compare.
4. An automatic policy's resolution runs with the record's token through step 2; its stale outcome re-decides the slot instead of notifying.

| Reason | KEEP_LOCAL | KEEP_CLOUD | KEEP_BOTH |
|---|---|---|---|
| FIRST_SYNC, BOTH_CHANGED | upload the local slot (U2 … U7, U4 skipped: the conflict is overridden) | download the cloud copy into the slot (D1 … D5) | download the cloud copy into a new manual slot (§ "Keep-both naming"), then upload the local slot over the original; the new slot is LOCAL_AHEAD and uploads as its own slot |
| DELETED_LOCALLY_CHANGED_IN_CLOUD | delete the cloud copy (DELETE_CLOUD without the re-decision) | download it (the slot reappears locally; the tombstone is cleared) | refused |
| CHANGED_LOCALLY_DELETED_IN_CLOUD | upload the local slot | delete the local slot (DELETE_LOCAL) | refused |

The script request completes when the resolution's transfers commit (`applied` the choice, `newSlot` the KEEP_BOTH name), when it is stale (`"stale"`, the slot in CONFLICT with a new notification queued), or when it fails (`BACKEND_ERROR`, the slot in ERROR and still conflicted after the retry finds the same inputs).

### Keep-both naming

The new slot name is the original truncated to 26 bytes followed by `-cloud`; when a local slot or an index entry of that name exists, `-cloud2` … `-cloud9` in turn; when all nine exist the resolution answers `REFUSED`. Every candidate is checked with `Zenith_ParseSaveSlotName(name, SAVE_KIND_MANUAL)`, which a name built this way always passes (lower-case characters, no leading `-` or `_`, not reserved).

### Reference conflict screen

<!-- elaboration -->
The flagship games carry the reference flow (`Zenith2/Docs/Design/ONLINE.md` → "Flagship slice integration"): a behaviour `CloudConflictFlow` handles `Online.CloudConflict`, checks `Online.QueryCloudSlot` (a conflict restored from a save may be stale, `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Notifications" step 7), opens a conflict screen (a UI document, `Zenith2/Docs/Design/UI/Documents.md` → "UI document schema") whose data come from a game schema component set from the event's fields, and maps the screen's four buttons to `Online.ResolveCloudConflict(ctx, slot, "local" | "cloud" | "both" | "defer", token)` with the token of the event the screen shows; a `"stale"` answer keeps the screen open and the flow rebuilds it from the next `Online.CloudConflict`. The screen:

| Element | Content | Rules |
|---|---|---|
| title | the localized string `online.conflict.title` ("Two different saves were found") | the screen's first focus target for narration |
| two columns | "This device" and "Cloud": the save display name, the UTC time formatted by UI's message formatting (`Zenith2/Docs/Design/UI/Localization.md` → "Message formatting"), the playtime as hours and minutes, and "Saved on this device" when `CloudFromThisDevice` | text, never colour alone, distinguishes the two |
| four buttons | Keep this device, Keep cloud, Keep both (shown only for MANUAL slots), Not now | tab order left to right; Cancel maps to Not now; every button has an accessible label equal to its text |
| a reason line | one string per conflict reason (`online.conflict.reason.<name>`) | the deletion reasons say which side deleted the save |

The screen is part of the accessible-flow acceptance runs of the slices (`Zenith2/Docs/Design/UI/Accessibility.md` → "End-to-end accessible flows").

## Interrupted sync recovery

Every interruption leaves a state the next reconcile recognises; no step needs a journal of its own beyond SAVE's and PLAT's atomic writes.

| Interrupted after | Cloud | Local | Next reconcile |
|---|---|---|---|
| U5, before U6 (object N written, index not) | N unreferenced; the slot's entry still references its old object, intact (C = B) | L | row 7: upload again; at the same serial the same name N is found (create-only mismatch, hashes equal) and U6 commits it; at a moved serial a fresh name is written and N becomes garbage |
| U6, before U7 (index committed, manifest not) | C = L, commit serial s + 1 | L, manifest base B with its older base serial | row 9: ADOPT (base and base serial updated) |
| U6 reported OK, then undone (Steam's launch dialog kept another machine's index; Play's offline commit lost to the server) | the entry is missing, the base or the previous base at an older serial, another device's commit at the same serial, or another history's entry or record | L, base L with base serial s + 1 | row 18 or 19: re-upload; rows 14 and 22 … 26: conflict; the table never deletes the local save, downloads over it or re-commits over another history's entry, however many commits the kept index is ahead (an index whose log no longer reaches the base is `HORIZON`, a conflict too); the conflict is then the player's (ASK) or the slot kind's automatic policy's |
| D4 during SAVE's journaled write | C | SAVE's journal recovery leaves the old or the new file (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Journal recovery") | old: row 8 (download again); new: row 9 |
| D4, before D5 | C | L = C, base B | row 9: ADOPT |
| DELETE_CLOUD after the index write, before the manifest write | a deletion record; the object garbage | absent, T set | row 15 (FORGET) |
| DELETE_LOCAL during SAVE's delete | the deletion record | SAVE's delete order leaves the slot or nothing | slot present: row 12 again (idempotent); absent: row 15 |
| a garbage deletion or a record drop | the object present or gone; the record present or gone | — | a present object or record is collected by a subsequent reconcile once it still qualifies |
| an index repair before its write | unchanged | unchanged | the view is still UNKNOWN; the repair may be requested again |
| a manifest write | — | the previous manifest (WriteAtomic replaces whole files) | the rows above with the older base, base serial and tombstones, each of which converges; a lost tombstone makes the slot restore from the cloud (row 16), the safe direction |

## Transfer memory

- One transfer runs at a time. Its buffer is reserved from the memory governor first: `ReserveSubsystemBytes(MEMORY_SUBSYSTEM_OTHER, bytes)` (`Zenith2/Docs/Design/CORE/MemoryGovernor.md`), then allocated from the session heap; a refused reservation defers the transfer to the next `Pump` and counts `online.cloud.transfer_deferred`.
- **A buffer is never freed while anything may still write it.** It is freed, and its reservation released, only when its job has ended and every backend request (a `CLOUD_READ`) and SAVE operation (a `TakeExportedSlot`) that was given the buffer has delivered its completion, including the `CANCELLED` or real completion of a request cancelled by a fence, a timeout or shutdown ([Z2-N-CONTRACT-335]; the request contract of `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineServices header"). A fence detaches the buffer from its slot job and keeps it on the session's detached list until those completions arrive; a detached buffer still counts as the one transfer, so no new transfer starts until it is freed.
- Upload: one buffer of 320 + file bytes; SAVE copies the exported bytes straight into it at offset 320 (assumption pending SAVE, row ON1 (a)).
- Download: one buffer of the index entry's payload bytes + 320; SAVE's import copies the payload into its own write arena (charged to SAVE's row), after which ONLINE frees its buffer.
- Reconcile export: one buffer of the listed file bytes, freed after hashing.
- Index repair: one buffer of one listed object's stored bytes at a time.
- The index buffer (34,880 bytes: the 2,112-byte header with its 128-pair commit log, up to 64 entries and 64 deletion records) and the manifest buffer (6,672 bytes) are allocated once at bind.
- The peak is therefore one envelope plus 41,552 bytes; the memory consumer reports it (`Zenith2/Docs/Design/ONLINE.md` → "Memory and the governor").

## Polling and triggers

| Trigger | Reconcile |
|---|---|
| bind (R6) | full |
| SAVE's slot listener reports a write of an eligible slot (assumption pending SAVE, row ON1 (c)) | targeted, after `m_uUploadDelayMs`; a second write of the same slot within the delay restarts the delay, so an autosave burst uploads once; a write the session itself caused (an import of a `DOWNLOADING` slot) triggers nothing |
| SAVE's slot listener reports a deletion | the tombstone is recorded (§ "Deletion propagation"); targeted, at the next `Pump`: the listener only queues the reconcile, and every SAVE call it makes runs from `Pump` (the session is Reentrancy NONE and SAVE calls the listener from `PumpMainThread`) |
| `ONLINE_INVALIDATION_CLOUD` from the backend (Play on resume, the offline test access) | full |
| `online.cloud.pollSeconds` elapsed since the last full reconcile (0: never) | full |
| `Online.SyncCloudNow`, `RequestCloudSync` | full; the request answers when it ends |
| `Online.RepairCloudIndex`, `RequestCloudIndexRepair` | § "Index repair", then full |
| the OS session unlocked | full when the poll interval (or 300 s) passed |
| connectivity restored (`ONLINE_INVALIDATION_CONNECTIVITY`, connected) | full, at most once per 60 s |

## Cloud status reporting

The engine keeps one cloud status and queues `ONLINE_NOTIFY_CLOUD_STATUS` to every PLAY world whose previous-profile flag is clear when it changes (a flagged world's game is leaving it, and the status describes the new account's cloud):

| Status | Entered when | Left when |
|---|---|---|
| available | the first reconcile after bind succeeds with a KNOWN or EMPTY view, or a transfer succeeds after a failure | any row below |
| unavailable (reason network) | a backend request completes `UNAVAILABLE`, or the index view is UNKNOWN with cause READ_FAILED | a subsequent request succeeds |
| disabled (reason user setting or app setting) | the backend reports cloud storage disabled (`CLOUD_DISABLED`: Steam's per-account or per-app switch), or `online.cloud.enabled` is false | the setting changes (polled at bind and at each full reconcile) |
| quota exceeded (reason quota) | a write completes `QUOTA`, or an upload of a new slot finds the index holding 64 live entries | a subsequent write succeeds |
| degraded (reason profile switch failed) | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Profile switch retry" step 3 | the profile switch succeeds |
| index unreadable (reason index invalid, index foreign or index missing) | the index view is UNKNOWN with cause INVALID, FOREIGN_PROFILE or MISSING | a reconcile classifies the view KNOWN or EMPTY (after § "Index repair", or another device's fix) |
| read only (reason index newer) | the index view is UNKNOWN with cause NEWER_FORMAT | a reconcile classifies the view KNOWN or EMPTY (only after the cloud is written by a release this build can read) |

## Shutdown and background flush

1. `BeginShutdown` (the module instance's destructor, shutdown step 6, while `Zenith_AsyncIO` and the save system are alive, `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", "Shutdown ownership order"): `SetSlotListener(nullptr)` first, so SAVE calls nothing of a session that is shutting down; no new reconcile starts; debounce timers are cancelled and every `LOCAL_AHEAD` slot is uploaded in slot-name order until `m_uShutdownFlushMs` has passed; the transfer then running is cancelled at the deadline (a cancelled upload before U6 has committed nothing, and its buffer is freed when the cancelled request completes, which the backend's `Shutdown` guarantees); finally the manifest is written and waited for (at most 1,000 ms more).
2. Entering the background on Android (`PLATFORM_EVENT_LIFECYCLE` to the background, seen in `OnPlatformEvents`): the same flush, bounded by 2,000 ms of foreground-free time, because the process may be killed; whatever is not committed is recomputed at the next start from the hashes, so nothing is lost or duplicated.
3. Windows logoff (`ACCOUNT_SIGNAL_SIGNED_OUT` from the OS): the background flush.
4. Steam: the Steam client uploads remote-storage files when the game exits; the flush writes them through the API before `SteamAPI_Shutdown` (`Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam initialisation and pumping").
