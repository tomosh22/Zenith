# Zenithmon -- Decision Log

**Purpose:** Append-only record of every non-trivial decision made during
Zenithmon development. Future agents grep this when investigating "why was X
done this way?". Scope changes MUST land here as a user decision before any
implementation (see [Scope.md](Scope.md) Section 4).

**Format:** One entry per decision, **newest first**. Fields per entry:
date, id, decision, why, tests-that-lock-it, reversibility. Ids are `ZM-D-NNN`,
assigned in chronological order (so the highest number is at the top of the file).

**What counts as non-trivial:** anything involving trade-offs, anything another
system depends on, any engine-side change, any scope or convention ruling.
Tuning-value changes go in git history, not here.

---

## 2026-07-24 -- ZM-D-141 -- S7 item 2 SC5 ships the title menu, New Game and the disk-authentic Continue gate

- **Decision / boundary:** ship `Games/Zenithmon/Source/UI/ZM_UI_TitleMenu.{h,cpp}`
  as the FrontEnd title presenter (by-value, non-ECS, on `ZM_UI_MenuStack` order
  112, one arm per dispatch switch -- no new ECS order, **114 remains next
  free**), with `ZM_GameStateManager::RequestNewGame()` /
  `RequestContinue(ZM_SAVE_SLOT)` as the two title actions, and land the
  disk-backed scramble gate `ZM_SaveContinue_Test` **in this sub-commit**. The
  six-SC plan is therefore re-scoped: SC6 retains ONLY the milestone-autosave
  test closure, because the save -> quit -> scramble -> Continue -> exact
  restoration proof is already shipped here. This work was found IN FLIGHT and
  uncommitted on master at session start (runtime complete; the observer seam
  and windowed test unfinished); the iteration protocol's "finish in-flight
  work first" rule applied.
- **The title menu is an ambient FrontEnd screen, not a stack the player
  opens.** `ZM_UI_MenuStack::OnUpdate` auto-raises TITLE only when the stack is
  empty AND FrontEnd is the active scene AND no warp/battle transition owns the
  screen, and force-closes any stack containing TITLE the instant that stops
  being true -- so TITLE can never fight the pause menu, a warp, or a battle.
  Continue is visible iff ANY slot probes non-EMPTY (a DAMAGED slot counts,
  matching `AnySlotOccupied`'s contract from ZM-D-138); New Game is always
  live. Navigation links are rebuilt on every Present so no link ever targets
  a hidden Continue, and canvas focus is repaired onto the live default.
- **Continue is transactional and reads disk exactly once.**
  `RequestContinue` runs `ZM_SaveSlots::ReadState` into a LOCAL candidate,
  `QueueResume(candidate.m_xWorldPosition)` through SC3's ordinary validated
  placement path, then publishes the candidate LAST; any failure returns the
  exact `Zenith_ErrorCode` and leaves live state and the transition machine
  untouched. The Yes/No load prompt is armed only from LOAD mode against a row
  that still probes READY, and the YES arm performs the one definitive
  `RequestContinue` -- no second codec, slot reader, or placement path, and
  LOAD remains ungated by the overworld-only SAVE predicate.
- **The slot layer gained a test-only operation observer, because the
  disk-authentic claim needs disk-layer evidence.** `ZM_SaveSlots` now ships
  `ZM_SAVE_SLOT_OPERATION_FOR_TESTS` {PROBE_SLOT, READ_STATE, WRITE_STATE}, a
  plain-function-pointer observer, and `SetOperationObserverForTests`, firing
  exactly one event ON ENTRY per public API call (a refused attempt is still
  observed once). The global defaults nullptr, so shipped behaviour is
  byte-for-byte unchanged when unset, and `DeleteAllSlotsForTests()` now clears
  it FIRST -- the between-tests hook needed no edit, honouring TestPlan C3.
- **Tests that lock it:** 6 `ZM_Title` boot units (name/action totality,
  all-empty hides Continue, DAMAGED-only keeps Continue without claiming
  READY, reopen refresh, malformed snapshot fail-closed) + 2 `ZM_MenuStack`
  boot units (title routing; the `ZM_LoadConfirmState` arm/resolve/reset
  matrix). The extended `ZM_RootQuitAndBlockedSave_Test` (**158 frames**)
  proves the Auto-only FrontEnd TITLE contract and the armed-then-ESCAPEd load
  prompt. The new `ZM_SaveContinue_Test` (**247 frames**) is the full
  disk-authentic gate: real-input New Game publishes a fresh starter over an
  installed canary; a busy transition queue refuses `RequestContinue` with
  exactly one READ and `QUEUE_FULL`; after quit-to-title, Continue stays
  visible with ONLY a DAMAGED slot on disk; the Auto fixture is restored from
  bytes captured before its deletion; DAMAGED and EMPTY rows refuse with a
  plain line, never an armed choice; pre-Yes the live state is still the
  scramble; the Yes window performs exactly ONE `READ_STATE` on AUTO and ZERO
  writes at the slot layer; the published state equals the saved fixture and
  not the scramble; and the restored pose lands within 0.05 planar / 0.10
  vertical / 0.05 yaw of the saved pose, >= 2 m from both TownCenter and the
  scramble pose.
- **★ THE HEADLINE DEFECT, now a standing rule: a monolithic automated-test
  `Step` function is one stack frame, and in a /Od build that frame is the SUM
  of every local in every phase.** `Step_ZMSaveContinue`'s 29 phases aggregated
  ~six `ZM_GameState` locals (each embedding a 6-mon party + 16x30 box
  storage, ~150-200 KB) into a measured **1,312,136-byte** frame against the
  exe's **1,048,576-byte** stack reserve -- the process died in `__chkstk` on
  the FIRST Step call (exit -1073741571 = STATUS_STACK_OVERFLOW, caught via
  crash-dump analysis, not guesswork). Fixed structurally: 28 per-phase driver
  functions, each holding at most ~2 `ZM_GameState` (~300-400 KB ceiling), a
  thin dispatch Step, and a `SCResetGameState` helper so even Setup holds one
  temporary per frame. Any future multi-phase test touching `ZM_GameState`
  must follow the per-phase-function shape.
- **Assessment-found defects repaired in flight:** the observer seam the test
  referenced was never written (hard compile failure); the
  `RestoreAutoFixture` phase was declared but unreachable, leaving Auto
  deleted while a later phase required it READY (logically unpassable); the
  New Game canary latches were never consumed (vacuous "proof"); six dead
  latches/helpers were wired or deleted.
- **Observed gate evidence (all orchestrator-run, serial):** `Build\regen.ps1`
  GREEN; `zenith build Zenithmon` (`Vulkan_vs2022_Debug_Win64_True`) GREEN;
  headless **41/0**; boot unit gate **2521 ran / 2520 passed / 0 failed / 1
  documented skip** (the +8 = 6 `ZM_Title` + 2 `ZM_MenuStack` units; the
  workflow baseline moved **2513 -> 2521** from the OBSERVED line); focused
  windowed `ZM_SaveContinue_Test` **247 frames**, `ZM_RootQuitAndBlockedSave_Test`
  **158**, `ZM_SaveMenuFlow_Test` **98**; full windowed **41/41 passed, 0
  failed, 0 skipped, 0 zero-frame**; `%APPDATA%/Zenith/Zenithmon` EMPTY
  afterwards. Adversarial review panel verdict **CLEAN** (0 critical/high/
  medium; two low/low possible-concerns noted and deliberately not churned:
  unconditional per-frame `SetFocusable` writes in the title `Present`, and
  `std::filesystem` use inside the test-only fixture path).
- **Contracts held:** `ZM_SaveSchema` and its 824-byte v1 golden byte-untouched;
  `ZM_GameState` layout frozen; `ZM_SaveSlots` framing and
  write-answers-from-re-probe untouched (the observer is additive and
  behaviour-inert when unset); `uSERIALIZATION_VERSION` stays 1; no new ECS
  order; FrontEnd.zscen re-baked by the tools boot and still git-ignored.
- **Reversibility / next boundary:** the title surface, the two manager
  actions, and the observer are locally reversible; the player-facing
  contracts (Continue-visible-on-occupied, exactly-one-read Continue, the
  transactional publish-last ordering) are now pinned on both the pure and
  windowed sides. **SC6 NEXT (re-scoped):** the milestone-autosave test
  obligation only. The item-2 aggregate Roadmap checkbox stays open until SC6.

---

## 2026-07-22 -- ZM-D-140 -- S7 item 2 SC4 ships one save/load slot presenter plus root-menu Save and Quit

- **Decision / boundary:** ship `Games/Zenithmon/Source/UI/ZM_UI_SaveSlots.{h,cpp}`
  as the ONE presenter for both SAVE and LOAD, owned BY VALUE by
  `ZM_UI_MenuStack`, and extend the existing root menu with Save and Quit. The
  presenter is non-ECS, follows the established one-arm-per-dispatch-site screen
  pattern and consumes no serialization order. This is SC4 of six in S7 item 2;
  the title menu and the actual Continue/load transaction remain SC5, and the
  disk-backed restoration plus milestone-autosave gate remains SC6.
- **The four-row action matrix is fixed at the UI boundary.** Every opening
  re-probes Save0, Save1, Save2 and Auto and renders the storage layer's exact
  `EMPTY / READY / DAMAGED` answer. In SAVE mode only Save0-2 are writable:
  EMPTY writes immediately; READY and DAMAGED return `CONFIRM_WRITE` and require
  a Yes/No overwrite prompt. This does NOT reinterpret DAMAGED as healthy: the
  damage remains surfaced, no code repairs or deletes it, and replacement occurs
  only after an explicit Yes. Auto remains visible but read-only to the manual
  path. In LOAD mode a READY row -- including Auto -- returns `CONFIRM_LOAD`;
  EMPTY and DAMAGED return NONE. SC4 deliberately leaves that load action
  unconsumed so SC5 cannot be mistaken for already shipped.
- **`ZM_SaveSlots::ResolveLiveSaveBlocker` remains the single permission
  predicate, and it is checked at TWO different boundaries.** `OpenSaveScreen`
  checks it before probing slots or changing the stack. `PerformSaveToSlot`
  checks it again before resolving live state, capturing, writing, changing the
  write/status latches or raising result UI, because the context can change while
  a screen or overwrite prompt is open. The only permitted write sequence is
  `ResolveLiveSaveBlocker -> ZM_GameStateManager::CaptureWorldPosition ->
  ZM_SaveSlots::WriteState`; the second check fails closed before both capture
  and disk. LOAD is intentionally exempt from the SAVE permission check so it can
  open on FrontEnd, which is necessarily `NOT_OVERWORLD`.
- **A live blocker changes ROOT focus and navigation atomically.** ROOT is now
  Party / Bag / Dex / Save / Quit / Exit. While ROOT is shown, Save is hidden,
  made unfocusable and omitted from both explicit navigation directions whenever
  the canonical blocker is live. If Save owned focus on the visible-to-hidden
  transition, focus is immediately rehomed to Quit. This is a correctness rule,
  not polish: engine focus navigation follows an explicit link first and does not
  spatially fall back when that target is hidden, while focused-name dispatch
  would otherwise retain a pointer to the newly hidden Save element.
- **Quit is an input-driven action, not another screen.** Confirm on the root Quit
  entry raises `Quit to title? Unsaved progress will be lost.` through the existing
  single-tenant dialogue choice. No consumes the action and returns to ROOT with
  no warp or load. Yes closes the menu and calls
  `ZM_GameStateManager::RequestQuitToFrontEnd()`, reusing SC3's two-barrier
  playerless transition rather than adding a second title path. Exit remains the
  distinct close-menu action.
- **The integration negatives have one attributable guard rather than an
  over-determined outcome.** `ZM_SaveMenuFlow_Test` uses real M/arrow/Enter input
  to write an EMPTY manual slot to a real READY file, reopen and overwrite the
  READY row only after real-input Yes, and rejects Auto/NONE/out-of-range
  overwrite targets without leaving a target armed. For the irreversible-boundary
  negative it first proves `CaptureWorldPosition` WOULD succeed into a copy on
  the same frame, then raises a WARP blocker and proves the real Enter produces no
  capture/write/file. `ZM_RootQuitAndBlockedSave_Test` first focuses Save with
  real input before making WARP live, so its hidden/unfocusable Save, immediate
  Quit focus, live Up/Down/Accept traversal and unchanged public SAVE-open seam
  directly pin the rehome/rewire behavior. The same test drives Quit No and Yes
  end to end, then opens LOAD on FrontEnd and reaches a READY Auto row while
  proving its activation did not write.
- **Tests that lock it:** **23** new `ZM_Save` boot units in
  `Tests/ZM_Tests_SaveSlotScreen.cpp` cover row/name totality, labels, the complete
  SAVE/LOAD x slot x status action matrix, Auto's asymmetric policy, uncached
  reprobes and damaged-row non-mutation. **5** `ZM_MenuStack` units cover the
  singleton refusal seam plus the six-item resolver/screen/order contract.
  `ZM_SaveMenuFlow_Test` and `ZM_RootQuitAndBlockedSave_Test` are the two new
  registered graphics tests carrying the ECS/input/disk/focus boundary described
  above; the existing `ZM_S6UIGate_Test` also proves its real two-way root walk
  now rests on both inserted entries.
- **Observed gate evidence:** `Build\regen.ps1` GREEN; `zenith build Zenithmon`
  (`Vulkan_vs2022_Debug_Win64_True`) GREEN. Boot units **2513 ran / 2512 passed /
  0 failed / 1 documented skip**, up **+28** from clean SC3 (**23 + 5**), and the
  workflow baseline moved **2485 -> 2513** from that observed line. Automated
  registrations grew **38 -> 40**; headless discovery/gate **40/40**; focused
  `ZM_SaveMenuFlow_Test` **98 frames** and
  `ZM_RootQuitAndBlockedSave_Test` **146 frames**; full windowed **40/40 passed,
  0 failed, 0 skipped, 0 zero-frame**. The save directory was EMPTY afterwards
  and the final exact-diff check was GREEN. Both new tests require graphics, so
  their behavioral authority is the focused/full local windowed gate, not a
  headless skip. No commit, push or CI result is claimed.
- **Contracts held:** `ZM_SaveSchema` and its literal 824-byte v1 artifact are
  byte-untouched; the eleven modules, their versions/order and all framing remain
  unchanged. `ZM_GameState` layout is unchanged. `ZM_SaveSlots` is consumed as
  shipped and no second disk owner exists. `ZM_UI_MenuStack`'s serialized schema
  and every existing ECS order are unchanged; this by-value presenter consumes no
  new order, so **114 remains next-free**.
- **Reversibility / next boundary:** the presentation and root wiring are local,
  but manual-vs-Auto ownership, the READY/DAMAGED confirmation rule, the dual
  blocker checks, and focus rehome are now player-facing contracts pinned on both
  the pure and windowed sides. **SC5 NEXT** adds the title menu and consumes the
  READY-slot LOAD action as Continue. **SC6** must save to disk, quit, deliberately
  scramble the persistent live state and prove that scramble took BEFORE
  Continue, then prove position/party/flags were restored from disk; otherwise
  `DontDestroyOnLoad` can make a zero-byte Continue look green. SC6 also closes
  the milestone-autosave obligation. The item-2 Roadmap checkbox therefore stays
  unchecked at SC1-SC4 of six.

---

## 2026-07-21 -- ZM-D-139 -- S7 item 2 SC3 captures the player's world position, resumes into it, quits to the title, and latches the milestone autosave

- **Decision / boundary:** ship `Games/Zenithmon/Source/Save/ZM_ResumePoint.{h,cpp}`
  (the PURE half of "where does a loaded save put the player") and
  `Games/Zenithmon/Source/Save/ZM_Autosave.{h,cpp}` (the milestone autosave policy
  plus its one live entry point), and wire both into `ZM_GameStateManager` --
  world-position capture, resume placement, the playerless quit-to-FrontEnd path
  and the arrival autosave latch. This is SC3 of six in S7 item 2. Before it,
  `ZM_GameState::m_xWorldPosition` was written by NO runtime code anywhere in the
  tree and nothing recorded which spawn tag the player had arrived at:
  `m_szTargetSpawnTag` is the tag of an IN-FLIGHT warp and `ResetTransitionState`
  memsets it the instant the warp finishes, so by the time anything could ask, it
  was already gone. The save-slot screen, root-menu Save/Quit, the title menu and
  Continue are SC4/SC5 and deliberately do not exist here; `ZM_SaveSchema` and
  `ZM_SaveSlots` are consumed exactly as ZM-D-136/138 froze them, with no codec
  redesign and nothing added to the payload.
- **The pure/impure split is the reason this is testable at all.** Validation
  (`ZM_ValidateResume`, `ZM_CanResume`, `ZM_ShouldUseSavedTransform`,
  `ZM_IsResumeTransformUsable`), world-position construction
  (`ZM_MakeWorldPosition`), the yaw conversions (`ZM_YawFromRotation` /
  `ZM_RotationFromYaw`) and the autosave predicate (`ZM_IsAutosaveTriggerLive`,
  `ZM_ShouldAutosave`) name NO ECS type, NO component, NO scene handle and NO
  physics body, so the entire decision surface is pinned by headless boot units
  with no scene loaded. The impure reaches -- resolving the unique player, reading
  and writing its body pose, resolving the active scene, asking
  `ZM_SaveSlots::ResolveLiveSaveBlocker` -- live on `ZM_GameStateManager` and on
  `ZM_TryAutosave`, and call DOWN into the pure layer. `ZM_SpawnPoint::IsTagValid`
  is passed IN to `ZM_ValidateResume` as a bool for exactly that reason. Every
  function in both new TUs is TOTAL and diagnoses mis-authored data with a
  non-fatal `Zenith_Error(LOG_CATEGORY_GAMEPLAY, ...)`, because the boot units feed
  them NaNs, oversized tags, the UNSET sentinel and unresolvable build indices ON
  PURPOSE and one `Zenith_Assert` would end the whole unit gate rather than red one
  unit. `ZM_ValidateResume` therefore evaluates SCENE -> TAG -> TRANSFORM in that
  order and only reaches `ZM_GetWorldSpec` after `ZM_FindSceneByBuildIndex` has
  returned something other than `ZM_SCENE_NONE`, which that accessor asserts on.
- **`SaveFormat.md`'s transform-vs-spawn-tag TBD is RESOLVED as transform-first,
  spawn-tag fallback -- and the fallback costs nothing because it is already on the
  path.** A resume rides the ORDINARY validated `TryQueueWarp`: same fade, same
  single load, same spawn-marker placement. `ZM_RESUME_INVALID_TRANSFORM` is a
  RECOVERABLE verdict -- scene and tag are a complete warp destination, so the
  resume still happens and the marker placement simply stands, with no second
  placement path to keep in sync. `INVALID_SCENE` and `INVALID_TAG` are refusals.
  The interesting tag failure is a tag ANOTHER scene offers: it passes grammar and
  the warp validator's own tag test, and would then WEDGE the transition in
  `WAITING_FOR_SPAWN` forever because no marker with that tag exists in the
  destination -- which is why validity is checked against the destination's
  `ZM_WorldSpec` spawn-tag list, not just against the grammar.
- **The captured position is the capsule CENTRE; spawn markers store FEET.**
  `CaptureWorldPosition` records `Zenith_Physics::GetBodyPosition`, and
  `ApplyPendingResumePlacement` writes it straight back; the feet convention exists
  only on spawn MARKERS, where `CalculateSpawnCenter` adds the capsule half-extent
  (0.9 m for the authored 1.8 m player). Mixing the two is a silent 0.9 m error
  that sinks or floats the player, so the two conventions meet in exactly one
  function and nowhere else. Capture is TRANSACTIONAL -- it builds into a local and
  publishes in one assignment, so a rejected pose leaves `m_xWorldPosition`
  byte-identical -- and it falls back to the scene's FIRST offered spawn tag when
  no transition recorded an arrival, which is load-bearing rather than defensive:
  the boot path and every direct `LoadSceneByIndex` enter a scene without a warp,
  and the codec rejects an empty spawn tag on a set scene index, so without the
  fallback such a game would be UNSAVEABLE.
- **Restoring the pose uses `SetBodyPosition`, never `TeleportBody`, and facing is
  a real contract rather than best effort.** `TeleportBody` forces IDENTITY
  rotation, so using it here would throw the yaw away two lines later; rotation is
  written AFTER position for the same reason. The restored yaw SURVIVES
  `ZM_PlayerController`'s per-frame `Zenith_Physics::EnforceUpright` because that
  call rebuilds a Y-axis-only quaternion from the body's own heading -- it flattens
  pitch and roll and PRESERVES yaw. Yaw itself is `atan2` of the quaternion-rotated
  +Z, matching what `ZM_PlayerController` writes and what `EnforceUpright` reads
  back, and NEVER `glm::eulerAngles(quat).y`, a documented trap in this repo that
  collapses once the facing is more than 90 degrees off +Z (so a player facing
  backwards would be restored facing forwards, and only SOME headings would look
  wrong). The placement sits AFTER the marker teleport and BEFORE the camera
  barrier, so `ZM_FollowCamera` acquires the FINAL pose instead of springing across
  the correction, and it ends with `SyncPhysicsPoseAndInvalidate` because
  `SetBodyPosition`/`SetBodyRotation` deliberately do NOT fire the body-pose-changed
  hook that `TeleportBody` does -- without it the transform cache keeps the MARKER
  pose for a frame and the camera and renderer read the pre-correction pose.
- **Quit-to-FrontEnd is a PLAYERLESS DESTINATION, and that took two bypasses, not
  one.** FrontEnd authors no Player, no `ZM_SpawnPoint` and no `ZM_FollowCamera`,
  and `AdvanceFadeIn` carries TWO INDEPENDENT barriers:
  `TryResolveFrozenTargetPlayer` (which on failure forces the screen opaque and
  bounces the state back to `WAITING_FOR_SPAWN`, which bounces straight back) and,
  separately, `HasUniqueReadyFollowCamera`. Patching only
  `PollForSpawnAndPlacePlayer` would have left quit-to-title ping-ponging forever
  on a permanently opaque screen, and falling through would additionally
  dereference a null controller. Both are bypassed on the playerless path, which is
  latched ONLY on `TryQueueWarp`'s accept line (so a refused warp cannot clobber an
  in-flight transition's flag) and cleared only by `ResetTransitionState`. The
  destination is compared against the literal build index 0 -- mirroring the
  existing playerless-SOURCE branch -- because `ZM_GetWorldSpec` asserts fatally on
  the `ZM_SCENE_NONE` an unresolvable index returns. The arrival tail is SHARED
  between the playerful and playerless paths so there is exactly one, not two that
  can drift.
- **The milestone autosave is an edge-triggered latch drained from `OnUpdate`, and
  it asks SC2's blocker policy rather than re-deriving one.** `ZM_ShouldAutosave`
  forwards `ZM_SaveSlots::ZM_SAVE_BLOCKER` straight through and compares it against
  `NONE` rather than listing arms, so a blocker appended later is honoured with no
  edit and the manual menu save and the autosave can never give "may the player
  save right now" two different answers; the ONE condition autosave adds is the
  open menu, because the manual path is reached THROUGH the menu. It cannot fire
  from the fade-in tail: `ResolveLiveSaveBlocker` consults `IsWarpInProgress()`,
  true for EVERY non-IDLE transition state, so an in-tail autosave would always
  resolve `WARP` and silently never save. The arrival tail therefore latches and
  `OnUpdate` drains it once the machine is IDLE, consuming the latch BEFORE the
  attempt so a refused or failed autosave is never retried (that is a
  disk-hammering loop, not a recovery). `ZM_AUTOSAVE_TRIGGER` ships all five
  milestone arms now so the vocabulary never has to be renumbered, but
  `ZM_IsAutosaveTriggerLive` gates each one and exactly ONE is live today
  (`SCENE_ENTERED`) -- declaring a trigger is not shipping it, and a trigger whose
  producer has not landed cannot ship untestable production code.
- **★ DEFECT 1, AND THE RULE IT ESTABLISHES: a latch consumed at the top of a
  function that its own state machine can re-enter is a race waiting to happen.**
  `ApplyPendingResumePlacement` originally cleared `m_bResumePending` BEFORE
  validating and applying the pose. But `PollForSpawnAndPlacePlayer` can run MORE
  THAN ONCE per transition -- both `AdvanceFadeIn` and `PollForCameraAndBeginFadeIn`
  push the state back to `WAITING_FOR_SPAWN` whenever the frozen player id stops
  matching the unique player, and every one of those passes re-runs the marker
  `TeleportBody`. On any second pass the marker teleport would therefore stand as
  the FINAL placement while the resume no longer applied, silently landing the
  player on the default spawn with nothing in the log to say why: green today,
  flaky by construction, and only on the runs where the bounce happens. Fixed by
  letting the latch die with the TRANSITION instead of with the attempt --
  `ResetTransitionState` already clears it on BOTH the success tail and the
  cancel/test-reset path, so it can neither outlive its transition nor retry
  forever, and every entry re-validates the pose, so re-applying it is idempotent
  and still fail-closed.
- **★ DEFECT 2: the milestone-autosave PRODUCER was pinned by nothing.** Deleting
  the entire `OnUpdate` drain block would have left every test in the suite green,
  and `ZM_GetAutosaveCount()` was read by NO test at all. Closed by asserting both
  the counter delta and the Auto slot's probed status in BOTH windowed tests --
  `+1` and `READY` on the resume arrival, `+0` and `EMPTY` across the quit.
- **★ DEFECT 3, AND THE RULE IT ESTABLISHES: when two independent guards both
  produce the right answer, a test that only observes the OUTCOME pins NEITHER.**
  `ZM_QuitToFrontEnd_Test`'s "must NOT autosave" assertions could not catch the
  mutations their own comment claimed, because the refusal there is
  OVER-DETERMINED: the save-blocker policy refuses first, but deleting that policy
  (or whitelisting the not-overworld blocker) merely falls through to the
  playerless capture guard, so the counter never moves either way and the test
  stays green. The comment was corrected to state what those assertions really pin,
  AND a genuine integration negative was added to `ZM_ResumePlacement_Test`: with
  the player ALIVE in Dawnmere and a warp IN PROGRESS, it proves the capture WOULD
  have succeeded on that very frame, then proves `ZM_TryAutosave` refused and the
  counter did not move. Windowed evidence: `blocker=3` (WARP),
  `captureWouldWork=true`, `refused=true`, autosaves `0->0`. The POSITIVE half --
  the same trigger DOES save once the blocker clears -- is the arrival's `+1`, so
  neither half is satisfiable by an autosave that never fires at all.
- **★ DEFECT 4: a function shipped with no caller and no test.** A story-flag
  autosave trigger resolver -- not in the SC3 spec, with no producer until the S8
  story beats land -- was deleted outright. Uncalled production surface cannot be
  pinned by anything a caller does, so it lands WITH its producer, in the
  sub-commit that first calls it, together with its own units. The
  `STORY_FLAG_SET` enum arm already reserves its vocabulary.
- **Also fixed pre-commit:** a world-extent unit that bracketed the guard only to
  `(512, 1e9]`, so the production constant could have been loosened from 4096 to
  1e8 with the entire boot suite and both windowed tests still green -- i.e. the
  guard effectively disabled for every realistic bad value an edited save carries;
  a hand-written 5000.0f rejection fixture (deliberately NOT spelled against
  `fZM_RESUME_WORLD_EXTENT`, which would move with the code under test) now
  brackets it to roughly `(540, 5000]`. Plus a comment naming the WRONG guard as
  the rejecting mechanism, three stale line citations, a missing explicit include
  (asset-guard extension macros were reaching the TU only through
  `Zenith_SaveData.h`'s transitive chain, which is nobody's contract to keep), and
  the one-frame transform-cache staleness described above.
- **Convention drift cleared, honestly scoped:** three save-area test files were
  using `std::vector`, which the project conventions forbid outright ("no `std::`
  containers -- use `Zenith_Vector`"). All three were converted to
  `Zenith_Vector<u_int8>`, or to a fixed array where that read more cleanly, with a
  local `AppendBytes` helper standing in for the range-insert `Zenith_Vector` does
  not carry. A FOURTH file, `Tests/ZM_Tests_SaveSchema.cpp`, predates this work and
  was deliberately left OUT OF SCOPE and is tracked separately -- the drift is NOT
  fully cleared. The sharpest trap for whoever does that one: `Zenith_Vector`'s
  single-argument constructor takes a CAPACITY, not a size, unlike `std::vector`.
- **Tests that lock it:** **27** new pure `ZM_Save` units in
  `Tests/ZM_Tests_ResumePoint.cpp` -- the three-way validity answer in SCENE ->
  TAG -> TRANSFORM order (including the UNSET sentinel, an unresolvable build
  index, a malformed tag, an empty tag, and a tag PlayerHome offers that Dawnmere
  does not, which is what makes "the spawn-tag list is actually consulted"
  testable), the totality of `ZM_CanResume` and `ZM_ResumeValidityName` across the
  whole enum and past `COUNT`, `ZM_MakeWorldPosition`'s reject-without-mutation
  contract byte-checked, its tag-tail zero-fill (the codec hard-rejects any non-NUL
  byte after the terminator, so a memcpy leaving stack garbage makes the game
  UNSAVEABLE), non-finite and out-of-extent rejection with both bracketing
  magnitudes, the four cardinal yaw headings and a `ZM_RotationFromYaw` ->
  `ZM_YawFromRotation` round trip (the cardinal -Z case is what catches
  `glm::eulerAngles`), a resume decision surviving a full schema round trip, and
  the autosave policy with FIVE separate one-blocker-each units, because a combined
  truth table still passes with any one term missing. **2** new registered windowed
  tests in `Tests/ZM_AutoTests_SaveResume.cpp` carry the ECS half, which no unit can
  reach. `ZM_ResumePlacement_Test` walks the player under real simulated
  camera-relative input to a pose provably away from the marker, captures it,
  resumes through the ordinary warp, and asserts the restored pose IS the captured
  one, is NOT the marker placement, and arrived across a REAL scene load; it then
  runs the live-blocker negative above and proves capture REFUSES in a playerless
  scene. `ZM_QuitToFrontEnd_Test` proves the quit finishes on a scene with no
  Player, no spawn point and no follow camera.
- **Observed gate evidence:** `Build\regen.ps1` GREEN (four new `.cpp` files, in
  the EXISTING `Source/Save/` and `Tests/` directories); `zenith build Zenithmon`
  (`Vulkan_vs2022_Debug_Win64_True`) GREEN. Boot units **2485 ran / 2484 passed / 0
  failed / 1 skipped**, up **+27** from 2458; the single skip is the pre-existing
  unrelated `GraphComponent::RegistryWideNodeRoundTrip` quarantine, which is itself
  the proof that none of the 27 new units skipped.
  `.github/workflows/zm-tests.yml` bumped **2458 -> 2485** from the OBSERVED boot
  line. The engine-only reference baseline **1103 is UNCHANGED** and no engine file
  was touched, so no cross-game regression sweep was owed. Headless automation **38
  passed / 0 failed**; FULL WINDOWED automation **38 passed / 0 failed, ZERO
  skipped**; the registered automated-test count grew **36 -> 38**.
  `ZM_QuitToFrontEnd_Test` passes in **38 frames** (players 1->0, loads 0->1, peak
  alpha 1.0, final alpha 0.0, final state IDLE, autosaves 0->0, Auto slot EMPTY).
  `ZM_ResumePlacement_Test` passes in **236 frames**: spawn (512.000, 26.886,
  480.000), captured (518.092, 27.149, 471.476), planar error 0.0000 (< 0.050),
  vertical error 0.0000 (< 0.100), yaw error 0.0000 (< 0.050), **10.477 m from the
  spawn** against a 2.000 m requirement -- so the restore is provably NOT the spawn
  teleport -- loads 1->2, so the value came from a real scene load and not from RAM
  survival, autosaves 0->1, Auto slot READY. **Mutation-verified:** suppressing the
  `SetBodyPosition` write in `ApplyPendingResumePlacement` turns
  `ZM_ResumePlacement_Test` RED; restored, the full suite is green again.
  `%APPDATA%/Zenith/Zenithmon` was verified EMPTY afterwards. **Both new windowed
  tests are `m_bRequiresGraphics = true`, so the headless CI batch SKIPS them and a
  skip counts as a PASS -- a green `zm-tests` proves NOTHING about either of them;
  they are carried by the LOCAL windowed gate alone.** No commit, push or CI result
  is claimed.
- **Contracts held:** `ZM_SaveSchema` untouched, so ZM-D-136 is honoured and the
  literal 824-byte v1 golden is unchanged; `ZM_SaveSlots` consumed exactly as
  ZM-D-138 shipped it, with nothing added to the payload; `ZM_GameState`'s layout
  untouched, so ZM-D-135 is honoured -- `m_xWorldPosition` is now WRITTEN for the
  first time, not redefined. No `uSERIALIZATION_VERSION` bump and NO new ECS
  serialization order consumed (next free remains **114**): every new
  `ZM_GameStateManager` member is SESSION state and `WriteToDataStream` still
  writes only the version word.
- **Reversibility / next boundary:** the placement RULE (transform-first,
  spawn-tag fallback), the capsule-CENTRE storage convention and the yaw
  convention are now durable -- they are recorded in `SaveFormat.md` and a save
  written under one convention cannot be read under the other. The world-extent
  bound, the trigger enum's unshipped arms and the drain's frame placement are
  local and freely revisable, both enums being append-only. Three sub-commits of S7
  item 2 remain: **SC4** the save-slot screen and root-menu Save/Quit; **SC5** the
  title menu and Continue; **SC6** the S7 stage-gate windowed test (save ->
  quit-to-FrontEnd -> continue restores position/party/flags exactly) plus an
  autosave milestone test. **The hazard SC6 must design around is unchanged and now
  demonstrated:** `ZM_GameStateManager` is `DontDestroyOnLoad` and its FrontEnd
  re-author path DESTROYS the duplicate rather than reseeding, so the live
  `ZM_GameState` survives quit-to-title ENTIRELY IN RAM -- which is precisely why
  SC3's windowed proof is written against the PLAYER'S BODY POSE (destroyed and
  rebuilt by the scene reload) rather than a game-state field. SC6's gate test MUST
  scramble the live state before continuing, and PROVE the scramble took, or it
  passes green against a Continue that reads zero bytes from disk. S7 has no visual
  gate and needs no human sign-off; the next human stop remains the S8
  vertical-slice go/no-go.

---
## 2026-07-21 -- ZM-D-138 -- S7 item 2 SC2 adds the typed slot/disk layer over the frozen codec, framed by an explicit length prefix

- **Decision / boundary:** ship `Games/Zenithmon/Source/Save/ZM_SaveSlots.{h,cpp}`
  as the typed SLOT and DISK layer that sits ON TOP of the ZMSV codec frozen by
  ZM-D-136. The new `Source/Save/` directory IS the boundary: `Source/Core/` owns
  the pure payload codec and names no slot, no file and no runtime, while
  `Source/Save/` owns slot identity, the engine save layer and the on-disk file,
  and adds NOTHING to the payload. Four slots are declared -- `ZM_SAVE_SLOT_0/1/2`
  (manual) plus `ZM_SAVE_SLOT_AUTO` -- with a three-state probe
  (`EMPTY / READY / DAMAGED`), typed `WriteState` / `ReadState` returning
  `Zenith_Status`, `AnySlotOccupied` / `AnySlotReady`, `DeleteSlotFile`, and a
  pure save-blocker predicate with a fixed precedence. The header names NO ECS
  type, NO scene, NO UI element and NO component; the .cpp's only live-state
  reach is `ResolveLiveSaveBlocker` at the very bottom, which forwards four bools
  into the pure predicate above it. World-position capture, quit-to-FrontEnd,
  autosave policy and the save screen are LATER sub-commits and deliberately do
  not exist here. This is SC2 of six in S7 item 2.
- **The one piece of framing this layer adds, and why it is not optional:** the
  engine payload region is written as `[u32 little-endian ZMSV byte length][ZMSV
  blob]`, emitted byte by byte rather than as a memcpy of a `uint32_t`, so the
  field can never inherit host byte order or struct padding. It exists because
  `ZM_SaveSchema::Read` demands an EXACT byte length (any slack is rejected as
  trailing bytes) and the engine's two Load paths DISAGREE about
  `Zenith_DataStream::GetCapacity()`: the disk path wraps an exactly-payload-sized
  buffer (`Zenith_SaveData.cpp:317-319`, capacity == payload) while the staged
  readback path used by tests hands the callback a DEFAULT-CONSTRUCTED OWNING
  stream whose capacity is the whole 1024-byte allocation
  (`Zenith_SaveData.cpp:229-235`). `GetCapacity()` therefore can NEVER be used as
  the codec's length, and branching on `OwnsData()` would couple the game to which
  engine load path happens to be running. The prefix is what makes both paths hand
  the codec a byte-identical exact length. It carries no magic and no version --
  ZMSV's own magic and schema version sit four bytes later, and duplicating either
  would create two sources of truth. It sits OUTSIDE the frozen payload, so the
  literal **824-byte** v1 golden is untouched. `uGAME_VERSION = 1` is stamped into
  the ENGINE header's `uGameVersion` field only; `LoadEx` never inspects it and
  the real version gate remains inside the payload.
- **Two orderings in `WriteState` are load-bearing:** (1) the payload is STAGED
  AND VALIDATED with the frozen codec BEFORE `Zenith_SaveData::Save` is called at
  all, because Save creates the file the instant it is called -- validating inside
  the write callback would leave a zero-length file behind for every rejected
  state, so "a rejected state leaves the slot exactly as it was" is a consequence
  of this ordering and nothing else. (2) Save's return value is DELIBERATELY
  DISCARDED: it is the literal constant `true` on every path
  (`Zenith_SaveData.cpp:204`) because `Zenith_DataStream::WriteToFile` is void and
  `Zenith_FileAccess::WriteFile` only logs, so believing it would report SUCCESS
  for a disk-full or permission failure. The answer comes ONLY from a re-probe of
  the slot just written, and that re-probe is the layer's SOLE evidence that the
  save landed and is loadable. `SUCCESS / INVALID_ARGUMENT / OUT_OF_MEMORY /
  CORRUPT_DATA` are all unit-pinned; the `FILE_NOT_FOUND` arm is deliberately
  uncovered and documented as unreachable from a unit (making
  `Zenith_FileAccess::WriteFile` fail trips a `Zenith_Assert` on the ofstream open
  and would kill the whole boot gate rather than fail one unit).
- **Damage is surfaced, never repaired.** `ProbeSlot` re-reads disk every call and
  is deliberately UNCACHED -- four ~830-byte reads when a slot screen opens are
  free, and a cache is a stale-state generator the between-tests hook would then
  have to know about. It decodes into a scratch state that is thrown away, and it
  NEVER writes, deletes or "repairs" the file it is looking at. Three states
  rather than a bool because "no file" and "unreadable file" demand OPPOSITE UI:
  collapsing them is how a New Game silently clobbers a recoverable save. For the
  same reason `AnySlotOccupied` counts a DAMAGED slot as occupied (the Continue
  VISIBILITY gate must not vanish on a damaged save) while `AnySlotReady` is the
  stricter, separate question. All three name maps are TOTAL and every name is a
  compile-time literal, because `Zenith_SaveData::BuildSlotPath` performs ZERO
  sanitisation -- an empty name for a bad id is what makes a path-traversal seam
  structurally impossible.
- **One save-permission predicate, shared by both future callers.**
  `ResolveSaveBlocker(bOverworld, bBattleTransitionActive, bWarpInProgress,
  bPendingWhiteout)` is PURE -- every live condition is passed in -- with a fixed
  top-to-bottom precedence (`NOT_OVERWORLD > BATTLE > WARP > PENDING_WHITEOUT >
  NONE`), the `ZM_ShouldInteract` idiom, append-only. It exists now, ahead of both
  of its callers, so the manual menu path (SC4) and the milestone autosave path
  (SC3) can never give "may the player save right now" two different answers.
  `ResolveLiveSaveBlocker` is the thin live wrapper; a context with no live game
  state reports "no pending whiteout" rather than inventing a blocker.
- **★ DEFECT 1, AND THE RULE IT ESTABLISHES: never infer "am I under test" from a
  process flag the run in question does not set.** The test-slot interlock
  originally redirected slot names onto their `"_Test"` aliases only when
  `Zenith_CommandLine::IsAutomatedTestRun()` was true. That is FALSE during the
  boot unit run: `Tools/run_unit_gate.ps1` launches with only `--headless
  --exit-after-frames 120`, a developer simply running the game passes no flags at
  all, and the one command that DOES pass `--all-automated-tests` (`zenith test
  <Game>`) also passes `--skip-unit-tests`, so the boot units never run there. The
  interlock was therefore keyed on a condition that could not hold, and it was
  simultaneously a TOTAL COVERAGE HOLE and a DATA-LOSS HAZARD. Consequence A:
  every disk unit hit `ZENITH_SKIP`, and **a skip counts as a PASS**, so the
  entire storage half of this sub-commit was green-by-skip and could not fail.
  Consequence B: that same interlock is the ONLY thing stopping those units from
  writing and then DELETING the real `Save0`/`Save1`/`Save2`/`Auto` files -- and
  the boot unit suite runs on EVERY boot, including an ordinary interactive
  developer run. Fixed by making the redirection EXPLICIT rather than inferred: a
  test-only `SetTestSlotNamesForTests` setter plus a default-FALSE file-scope
  flag, driven by the tests' RAII `ZM_SlotDiskScope`, which sets it as the very
  first thing it does, VERIFIES that all four names actually moved off their
  shipping stems BEFORE it deletes anything (so a broken redirection performs no
  disk operation at all), and clears it unconditionally on the way out. The
  per-test skip became a HARD FAILURE -- a unit that cannot redirect is a failed
  unit -- and a dedicated unit, declared first in the TU so it RUNS LAST
  (`Zenith_TestRunner::RegisterTest` prepends), asserts the redirection defaults
  OFF, so a leaked redirect reds instead of silently repointing the player's
  saves. RULE: make a safety interlock explicit and assert it, and NEVER let one
  degrade into a skip.
- **★ DEFECT 2, AND THE RULE IT ESTABLISHES: this layer's own rejection branches
  were dead to its tests.** DAMAGED was only ever manufactured by flipping a byte
  on disk -- which breaks the ENGINE's CRC32 and makes `LoadEx` bail BEFORE the
  read callback is ever invoked. Every payload-rejection branch this layer owns
  could therefore have been deleted outright with every unit still green. Closed
  by staging a readback whose inner ZMSV magic is corrupt: the readback stash is
  consulted AHEAD of the file (`Zenith_SaveData.cpp:219-238`), which bypasses the
  engine's CRC gate entirely and is the only way to make the game's own decode
  arm run. When a lower layer rejects a fixture first, the layer under test never
  executes -- verify WHICH layer produced the verdict.
- **★ DEFECT 3: the verify re-probe was pinned by nothing.** Every other write
  unit writes a good state to a working disk, so deleting the re-probe and
  returning success immediately after `Zenith_SaveData::Save` left them all green
  -- despite the header calling the re-probe the only evidence the save landed.
  Closed by `Slot_WriteAnswersFromTheVerifyProbeNotFromSaveReturn`, which stages a
  corrupt-magic readback for the TARGET slot BEFORE the write, so the file that
  actually lands is perfectly good and only a layer that BELIEVES its re-probe can
  report `CORRUPT_DATA`. Its three non-vacuity assertions confirm the write really
  reached the engine, that a file really landed (so the verdict is the
  `CORRUPT_DATA` arm and not `FILE_NOT_FOUND`), and that the poisoned read fixture
  never leaked into the bytes written. **Mutation-verified: making `WriteState`
  accept a DAMAGED verdict turns the gate RED (1 failed); restored, green.**
- **★ DEFECT 4: the too-small-for-a-prefix guard had zero coverage and was
  structurally unreachable from any staged fixture** -- the staged stream's
  capacity is always the full 1024-byte allocation, so `capacity - cursor < 4`
  cannot hold there, and the CRC-flip helper preserves the declared payload length
  and so can never produce a sub-prefix payload. Closed by
  `Slot_ReadRejectsADiskPayloadTooSmallForAPrefix`, which drives the DISK path
  with a hand-built `.zsave` declaring a 2-byte payload with a matching CRC, and
  which first runs a WELL-FORMED CONTROL ARM through the same fixture builder so
  the corrupt verdict is attributable to this layer's guard rather than to an
  engine header/CRC gate. Deleting the guard does not merely mis-report: it asks a
  2-byte stream for 4 bytes and trips `Zenith_DataStream`'s FATAL bounds assert.
- **Wrong claims caught and corrected before the commit** (the recurring
  false-claim-in-an-argumentative-passage defect class; SC1 had seven such sites):
  a comment claimed an unbounded prefix "would walk far past the buffer" if handed
  to the codec -- false, the frozen codec applies an equivalent bound at the same
  cursor and publishes only after full validation, so this layer's own bound is
  DEFENCE IN DEPTH, not the only thing preventing an overrun. The unit that pins
  that branch now states its honest scope: it pins the layer's OUTCOME contract (a
  lying prefix reports `CORRUPT_DATA`, and the destination survives), not the
  overrun. Also corrected: a stale doc citation repeated at four sites, a
  miscounted unit total in a rationale comment, and a stale line citation.
- **Tests that lock it:** **33** new `ZM_Save` units in
  `Tests/ZM_Tests_SaveSlots.cpp` -- six pure name/classification/policy units and
  27 that construct the RAII disk scope. They cover the four distinct file stems
  and their `_Test` aliases (both spelled as LITERALS, never derived from the
  production tables), total/empty names for `NONE` and garbage ids, manual-vs-Auto
  classification, original display copy, empty/ready/damaged probing, a maximal
  eleven-module field-for-field round trip, per-slot and Auto-vs-manual
  independence, overwrite-rather-than-append, the recorded payload being exactly
  prefix + blob with a little-endian prefix, staged-readback decode through the
  REAL engine seam, all four prefix/payload rejection shapes, the recorded game
  version, out-of-range write/read rejection, invalid-state rejection creating NO
  file, the verify-probe unit above, delete semantics, occupancy vs readiness, a
  write/probe/read composition, and all SIXTEEN boolean combinations of the
  blocker precedence spelled out by hand. Transactional reads are pinned by raw
  byte snapshots of the destination (never a copy-construct memcmp, which would
  compare indeterminate padding). `Zenithmon.cpp`'s between-tests hook now calls
  `ZM_SaveSlots::DeleteAllSlotsForTests()` BEFORE `Zenith_SaveData::ClearForTest`,
  because ClearForTest wipes only the in-memory write log and readback stash and
  explicitly does NOT delete files.
- **Observed gate evidence:** `Build\regen.ps1` green (a NEW `Source/Save/`
  directory plus two new `.cpp` files, so the regen was genuinely owed); `zenith
  build Zenithmon` (`Vulkan_vs2022_Debug_Win64_True`) green; boot units **2458 ran
  / 2457 passed / 0 failed / 1 skipped**, up **+33** from the 2425 baseline. The
  single skip is the pre-existing unrelated `GraphComponent::
  RegistryWideNodeRoundTrip` quarantine -- which is itself the proof that none of
  the 33 new disk units skipped. `.github/workflows/zm-tests.yml` bumped **2425 ->
  2458** from the OBSERVED boot line; an intermediate prediction of 2429 was wrong
  and was caught precisely because the observed line, never arithmetic, is what
  gets written. The engine-only reference baseline **1103 is UNCHANGED** and no
  engine file was touched, so no cross-game regression sweep was owed. Headless
  automation **36 passed / 0 failed**; full windowed automation **36 passed / 0
  failed, ZERO skipped**. The registered automated-test count is unchanged at
  **36**. `%APPDATA%/Zenith/Zenithmon` was verified EMPTY after the run: no
  residue, and no shipping-named slot file was ever created. No commit, push or CI
  result is claimed.
- **Contracts held:** `ZM_SaveSchema` is untouched, so ZM-D-136 is honoured and
  the literal **824-byte** v1 golden is unchanged -- the length prefix is framing
  AROUND that payload, inside the engine's payload region, never part of it.
  `ZM_GameState`'s layout is untouched, so ZM-D-135 is honoured; no
  `uSERIALIZATION_VERSION` was bumped; NO new ECS serialization order was
  consumed, so the next free order remains **114**.
- **Reversibility / next boundary:** the slot ORDINALS and their file stems are
  durable the moment a save exists (an ordinal picks a name, so reordering renames
  every save on disk) and the length prefix is now on-disk surface, recorded in
  `SaveFormat.md`; the statuses, the blocker enum and the display copy are local
  and freely revisable, both enums being append-only. Four sub-commits of S7 item
  2 remain: **SC3** world-position capture, resume placement, quit-to-FrontEnd and
  the autosave latch; **SC4** the save-slot screen and root-menu Save/Quit;
  **SC5** the title menu and Continue; **SC6** the S7 stage-gate windowed test
  (save -> quit-to-FrontEnd -> continue restores position/party/flags exactly)
  plus an autosave milestone test. **A hazard SC3/SC6 must design around:**
  `ZM_GameStateManager` is `DontDestroyOnLoad` and its FrontEnd re-author path
  DESTROYS the duplicate rather than reseeding, so the live `ZM_GameState`
  survives quit-to-title entirely in RAM -- a naive "save -> quit -> continue"
  test would pass green against a Continue that reads zero bytes from disk, so the
  gate test MUST scramble the live state before continuing. S7 has no visual gate
  and needs no human sign-off; the next human stop remains the S8 vertical-slice
  go/no-go.

---
## 2026-07-21 -- ZM-D-137 -- S7 item 2 SC1 gives story flags an identity registry and gates NPC lines on it

- **Decision / boundary:** ship `Games/Zenithmon/Source/Data/ZM_StoryFlags.{h,cpp}`
  as the identity registry for the story-flag bitset frozen by ZM-D-135, and make
  authored NPC dialogue the first gameplay consumer of it. Before this sub-commit
  there was **not one gameplay consumer of the story-flag bitset anywhere in the
  tree**: an index was a raw integer literal living only inside a test, and
  nothing stopped two pieces of content claiming the same bit. This is SC1 of a
  planned **six** sub-commits in S7 item 2; it deliberately ships no slot layer,
  no disk I/O, no resume placement, no quit-to-FrontEnd flow, no autosave latch
  and no save UI.
- **The enum value IS the wire bit index, and density is a storage contract:**
  `enum ZM_STORY_FLAG_ID : u_int` is SAVE-STABLE -- its value is the persisted bit
  index in save-schema module 4. Six flags are allocated densely from zero:
  `INTRO_LEFT_HOME` 0, `MET_PROFESSOR` 1, `STARTER_RECEIVED` 2, `WARDEN_CLEARED`
  3, `ROUTE1_OPEN` 4, `GYM1_DEFEATED` 5, followed by `ZM_STORY_FLAG_COUNT` and
  `ZM_STORY_FLAG_NONE`, which aliases COUNT and is NEVER persisted. Allocation is
  APPEND ONLY; reordering or reusing a retired value is a versioned codec change,
  not an edit to this file. Density is not tidiness: module 4 writes a uint16
  high-water count of highest-set-index+1 followed by ceil(count/8) bytes, so ONE
  sparse index (say 4000) would add ~500 bytes to EVERY save this game ever
  writes, and those bytes cannot be reclaimed without a version bump. Reserve a
  future flag by adding a row, never by leaving a numeric gap. Recorded in
  `SaveFormat.md` module 4, which now names this header as the authoritative
  registry.
- **Compiled table with a deduced bound:** the `const ZM_StoryFlagInfo
  s_axFlags[]` row table is written WITHOUT an explicit `[ZM_STORY_FLAG_COUNT]`
  bound, because with the bound spelled the row-count `static_assert` becomes a
  tautology and a forgotten row merely zero-initialises the tail into a nameless
  flag that only a boot unit could catch. Deduced, a missing or extra row is a
  COMPILE error at the table itself. Accessors are FREE FUNCTIONS
  (`ZM_SetStoryFlag` / `ZM_IsStoryFlagSet`, overloaded on `ZM_GameState` and on
  `ZM_StoryFlagSet`) specifically because `ZM_GameState` is frozen by ZM-D-135:
  naming a flag costs zero wire change and keeps the durable model a plain
  aggregate. `ZM_IsMilestoneStoryFlag` (WARDEN_CLEARED / ROUTE1_OPEN /
  GYM1_DEFEATED) is authored beside the registry so the milestone list cannot
  drift from the flags it names; nothing consumes it yet -- it is reserved for
  the autosave hook in a later sub-commit.
- **★ DEFECT 1, AND THE RULE IT ESTABLISHES: totality and a defensive assert are
  mutually exclusive.** `ZM_StoryGatePasses` originally did `Zenith_Assert(false,
  ...)` on an unregistered flag id. The unit `Gate_OutOfRangeFailsClosed`
  deliberately passes id 13 to pin the fail-closed answer. **`Zenith_Assert` is
  not compiled out anywhere in this engine:** `Zenith/Core/Zenith.h:138` defines
  `ZENITH_ASSERT` unconditionally, immediately ABOVE its own `#ifdef
  ZENITH_ASSERT`, so the real definition at `:140` always wins and calls
  `Zenith_DebugBreak()` in every configuration. Because the whole `ZENITH_TEST`
  suite runs at BOOT before the scene loads, the process died and **no "Unit
  tests complete" line was printed at all** -- the loss was the entire 2425-unit
  gate, not one red unit. RULE: where a function's contract is TOTAL and a unit
  pins that totality, the function must RETURN its defined answer. Diagnose
  mis-authored data with a non-fatal `Zenith_Error(LOG_CATEGORY_GAMEPLAY, ...)`
  and log nothing at all for a legitimate sentinel. (There is no
  `LOG_CATEGORY_GAME`; the enumerator is `LOG_CATEGORY_GAMEPLAY`.) Every function
  in `ZM_StoryFlags` is now total: the sentinel is decided BEFORE the range check
  (NONE aliases COUNT, so the ungated answer must not be classified as garbage),
  and the gate fails closed rather than being written as `IsSet(id) ==
  m_bRequireSet`, which would return TRUE for an unregistered id whenever the
  gate wanted the flag CLEAR -- a typo would UNLOCK content instead of locking it.
- **Gating selects CONTENT, never the seam:** `ZM_StoryGate { ZM_STORY_FLAG_ID
  m_eFlag = NONE; bool m_bRequireSet = true; }` is embedded by value in authored
  data rows, so a DEFAULT-constructed gate is unconditional and a data row that
  GAINS the field keeps its previous behaviour by construction rather than by
  every author remembering to fill it in. `ZM_NpcData` gains three fields
  APPENDED AT THE END (`m_xLineGate`, `m_paszGatedLines`, `m_uGatedLineCount`) --
  last on purpose, because every row is a POSITIONAL aggregate initializer and a
  field inserted mid-struct would silently shift each trailing value one column
  left, the first casualty being `m_bWanders` (the Wanderer would stop patrolling
  with no compile error). The pure `ZM_SelectNpcLines` chooses the set;
  `ZM_Interactable`'s `ZM_NPC_RAISE_DIALOGUE` arm reads the LIVE flags via
  `ZM_GameStateManager::TryGetGameState` and routes through it, treating a
  manager-less context as an all-clear set (a require-SET gate therefore fails
  closed and a require-CLEAR gate passes, which is exactly what a fresh save would
  answer). **`ZM_RaiseKindForRole` and `ZM_NPC_RAISE_KIND` are unchanged:** gating
  selects which lines a role SAYS, it never re-routes which seam a role talks
  through. A fifth Dawnmere NPC, `ZM_NPC_ROUTE_WARDEN` / entity `Npc_Warden`, is
  the first gated row and carries both an ordinary and a refusal line set.
- **★ DEFECT 2, AND THE RULE IT ESTABLISHES: adding a data row can weaken an
  existing test's teeth without touching that test.** Adding the Warden put a
  SECOND `ZM_NPC_ROLE_TALKER` into the fixture behind
  `GateRoster_PlacedNpcsCoverEveryRole`, so that unit's own advertised mutation
  (re-roll `ZM_NPC_VILLAGER` to SHOPKEEP) stopped redding it -- the Warden now
  covered TALKER, while the interaction gate's talk beat would have started
  raising a shop. Fixed by splitting the three BEAT NPCs (villager = talk,
  clerk = buy, caretaker = heal, each with its expected raise kind SPELLED IN THE
  TEST rather than read back off the row) from the merely-PLACED roster, and
  asserting role coverage over the beat table
  (`GateRoster_BeatNpcsCoverEveryRole` +
  `GateRoster_PlacedNpcsAreStationaryAndIncludeEveryBeat`). When authoring a new
  data row, re-read the units that walk that table and ask what each one's stated
  mutation still proves.
- **★ DEFECT 3, AND THE RULE IT ESTABLISHES: the new branch was pinned by
  nothing.** `ZM_SelectNpcLines` returns the ordinary lines verbatim for any row
  with a null gated array, and every pre-existing row is ungated -- so reverting
  the dispatch arm to the old one-liner left all 33 new units AND the full 36-test
  windowed suite GREEN. Closed by two new phases on the HEADLESS (therefore
  CI-visible) `ZM_NpcDispatch_Test`, which drive a real interact edge at the
  warden row with `WARDEN_CLEARED` clear and then set, asserting the queued line
  COUNT and the FIRST LINE TEXT. The text clause is load-bearing: both line sets
  have 2 entries, so a count-only assertion would not catch a selector that always
  returns the ordinary set. The phases also guard that the warden row is still a
  genuine demonstration (gate on WARDEN_CLEARED-set, both sets present, first
  lines distinct), and the flag is captured and RESTORED on every exit path,
  including a mid-phase failure that leaves it set. **Mutation-verified: with the
  dispatch arm reverted the test goes RED (exit=1), and green again with the
  source restored.**
- **Wrong claims caught and corrected before the commit** (a recurring defect
  class in this project -- confidently-worded false statements inside
  argumentative passages): (a) SEVEN comment sites justified the selector's null
  guard by claiming `ZM_UI_DialogueBox::QueueLines` would CRASH on a `(null,
  non-zero)` pair. It does not -- `ZM_UI_DialogueBox.cpp:68` rejects a null array
  as the first disjunct of its first guard, before any dereference. The real
  consequence is a refused push and a completely MUTE NPC, the same outcome as the
  over-cap case. Both sanitisers were KEPT (the selector owes its callers a pair
  that does not contradict itself); only the rationale was wrong. (b) The
  sparse-index rationale in the story-flag units was premised on a fixed-bound
  table the code no longer uses. (c) The warden's placement comment claimed he
  stood "on the north road out of town"; he is ~37 m from the Route polyline and
  ~1 m from the authored Home walkway centreline. His fiction and lines were
  rewritten to match where he actually stands and his position was NOT moved --
  the coordinates are separately derived against every traversal constraint and
  the windowed suite passed green at them. A note records that when a real route
  is authored, a road-blocking warden should be re-placed onto the route polyline
  with every separation re-derived from scratch.
- **Tests that lock it:** **33** new units -- **18** `ZM_Story` units in the new
  `Tests/ZM_Tests_StoryFlags.cpp` (id-equals-row-index, unique non-empty debug
  names, count vs the 4096 ceiling, dense-from-zero allocation, the enum-value-is-
  the-wire-bit-index and module-4 sizing claims, total/never-null naming,
  set/clear round trips, no-mutation rejection of NONE and out-of-range, all four
  gate arms including out-of-range fail-closed and the default-constructed
  unconditional gate, milestone totality, and a starter state with no registered
  flag set); **13** `ZM_Data` units over the gate columns and the selector; and a
  net **+2** in `Tests/ZM_Tests_Interactable.cpp` (the warden row dispatches as
  dialogue, plus the split roster pair from defect 2). Plus the two mutation-
  verified `ZM_NpcDispatch_Test` phases above -- the registered automated-test
  count is UNCHANGED at **36**, because the new coverage is extra phases inside an
  existing headless test.
- **Observed gate evidence:** `Build\regen.ps1` green (two new `.cpp` files, no
  new directory); `zenith build Zenithmon`
  (`Vulkan_vs2022_Debug_Win64_True`) green; boot units **2425 ran / 2424 passed /
  0 failed / 1 skipped** (the 1 skip is the pre-existing unrelated
  `GraphComponent::RegistryWideNodeRoundTrip` quarantine), up **+33** from the
  2392 baseline; `.github/workflows/zm-tests.yml` bumped **2392 -> 2425** from the
  OBSERVED boot line, never predicted arithmetic; the engine-only reference
  baseline **1103 is UNCHANGED** (`Tools/run_unit_gate.ps1`'s default untouched);
  headless automation **36 passed / 0 failed**; full windowed automation **36
  passed / 0 failed, ZERO skipped**, with no zero-frame tests. The pre-existing S6
  windowed tests held their historical frame counts exactly --
  `ZM_PlayerHomeRoundTrip` **831**, `ZM_NpcWander` **830**, `ZM_S6InteractGate`
  **749**, `ZM_NpcHeal` **315**, `ZM_NpcShop` **286** -- so the fifth authored NPC
  perturbed neither the nearest-wins interaction picker nor any traversal route.
  No commit, push or CI result is claimed.
- **Contracts held:** `ZM_SaveSchema` is untouched, so ZM-D-136 is honoured and
  the literal **824-byte** v1 golden is unchanged; `ZM_GameState`'s layout is
  untouched, so ZM-D-135 is honoured; no `uSERIALIZATION_VERSION` was bumped
  anywhere; NO new ECS serialization order was consumed, so the next free order
  remains **114**; no engine file was touched, so the engine baseline **1103**
  stands and no cross-game regression was owed.
- **Reversibility / next boundary:** the registry enum's VALUES are durable the
  moment a save containing them exists, so they evolve by appending, never by
  renumbering; the debug names, the gate columns and the selector are all local
  and freely revisable. Five sub-commits of S7 item 2 remain: **SC2** a typed
  slot/disk layer over the frozen codec; **SC3** world-position capture, resume
  placement, quit-to-FrontEnd and the autosave latch; **SC4** the save-slot screen
  and root-menu Save/Quit; **SC5** the title menu and Continue; **SC6** the S7
  stage-gate windowed test (save -> quit-to-FrontEnd -> continue restores
  position/party/flags exactly) plus an autosave milestone test. S7 has no visual
  gate and needs no human sign-off; the next human stop remains the S8
  vertical-slice go/no-go.

---
## 2026-07-21 -- ZM-D-136 -- S7 item 1 SC2 freezes pure transactional save schema v1 and its literal compatibility artifact

- **Decision / boundary:** ship `Games/Zenithmon/Source/Core/ZM_SaveSchema.{h,cpp}`
  as the pure inner-game-payload codec, below `Zenith_SaveData`. Its public seam
  is exactly `Write(const ZM_GameState&, Zenith_DataStream&)` and
  `Read(Zenith_DataStream&, uint64_t ulByteLength, ZM_GameState&)`. It knows the
  durable aggregate and byte stream only -- no files, slot names, ECS entities,
  scenes, menu flow or autosave policy. This completes Roadmap S7 item 1; item 2
  wires the frozen codec through story gates and Save0-2/Auto flows.
- **Frozen v1 wire contract:** explicit little-endian inner magic/version/count,
  followed by exactly **11** ordered, independently length-framed modules; all
  global and module versions are 1. Multi-byte values are emitted explicitly,
  and floats use little-endian IEEE-754 bit patterns. The shared monster record
  is exactly **61 bytes**; species and real moves use concrete enum + 1 while 0
  is reserved, and a real move with current/max PP `{0,0}` is valid under the
  current<=max invariant. StoryFlags writes highest-set-index+1 as its uint16
  high-water count. Options is a counted uint16 `{tag,length,value}` sequence:
  bounded unknown tags are skipped, but exactly one valid text-speed tag is
  mandatory, so unknown-only payloads reject. Dex writes the current roster,
  accepts current or smaller counts (zero-filling appended species), returns
  `VERSION_MISMATCH` for a larger same-v1 roster within the 512 cap, and returns
  `CORRUPT_DATA` above the cap.
- **Transaction and status contract:** `Write` validates and stages the complete
  payload privately, then appends at the destination cursor without changing
  its prefix. Returned invalid-source/destination or capacity failures leave
  destination cursor/bytes unchanged (`INVALID_ARGUMENT` or `OUT_OF_MEMORY`).
  `Read` is exact-length and parses into a temporary state; it publishes and
  advances the cursor only after all 11 framed modules validate. Every returned
  failure leaves caller cursor/state unchanged. Wrong magic is `BAD_MAGIC`;
  unsupported global/module versions and the newer-roster Dex case are
  `VERSION_MISMATCH`; malformed lengths/fields/trailing data are `CORRUPT_DATA`.
- **Smallest safe engine seam:** add the read-only
  `Zenith_DataStream::OwnsData()` query. Existing public state (`GetData`,
  `GetCapacity`, cursor, `IsValid`) cannot distinguish a growable owned stream
  from a fixed wrapped external buffer. Without that fact the codec must either
  reject valid owned growth or call `WriteData` past an external buffer and hit
  DataStream's assert-on-resize path. `OwnsData()` lets the codec preflight a
  whole fixed-buffer append while preserving ordinary owned growth; it changes
  no allocation/write behavior. Engine units pin default/sized ownership,
  external non-ownership, destruction safety, and move-construction/assignment
  ownership transfer/reset. Owned allocator exhaustion remains the engine's
  existing fatal/asserting policy.
- **Compatibility policy:** v1 is the first real schema. The complete literal
  **824-byte** array in `ZM_Tests_SaveMigration.cpp` is independently authored
  and pins both writer bytes and literal decode/re-encode. It is not a fake v0
  migration. From this commit forward, every incompatible global/module change
  owes a version bump and a literal historical-blob migration test in the same
  commit.
- **Tests that lock it:** **29** new `ZM_Save` schema tests cover maximal and
  edge round trips, append/exact-read atomicity, statuses, every truncation
  boundary, exact framing and all module/domain/cap rules, including raw
  move/float wire oracles, StoryFlags high water, Dex roster drift and Options
  TLVs. **2** compatibility tests cover the literal golden. Observed Zenithmon
  gate: regen green; five builds green (Vulkan Debug/Release x Tools true/false
  plus D3D12 Debug Tools=false); units **2392 ran / 2391 passed / 0 failed / 1
  skipped**; engine reference **1103**; headless **36/0**; full windowed
  **36/0/0** with 36 JSON results, no skips and no zero-frame tests.
- **Complete engine-surface regression evidence:** SentinelECS,
  SentinelPhysics and SentinelAI built and ran green; Combat Vulkan + **1103 /
  1102 / 0 / 1** boot + **14/0** suite passed; DevilsPlayground Vulkan/D3D12 +
  **1104 / 1103 / 0 / 1** boot + **158/0** suite (29 expected skips) passed;
  CityBuilder Vulkan/D3D12 + **1104 / 1103 / 0 / 1** boot + **45/0** suite (6
  expected skips) passed. Focused windowed RenderTest emitted exactly one
  unskipped passing JSON per canary: `EngineBootShutdownSmoke` **1 frame** and
  `TerrainEditorSmoke` **151 frames**. Scaffold smoke passed **11/0**, met its
  embedded **1103** unit baseline, and teardown regeneration was green with git
  status unchanged.
- **Reversibility / next boundary:** the codec is localized, but its v1 bytes
  are now durable compatibility surface and must evolve through migration, not
  silent edits. `OwnsData()` is additive/read-only and can remain independently
  useful. No visual/human gate applies. S7 continues with story-flag gates,
  slot/manual/continue/autosave integration, then trainers/navigation/rival;
  the next human intervention remains the S8 vertical-slice go/no-go.

---

## 2026-07-21 -- ZM-D-135 -- S7 item 1 SC1 freezes the durable save model before bytes

- **Decision:** complete SC1 as the durable **in-memory model freeze**, not as a
  partial codec. `ZM_GameState` now owns the full save-facing aggregate and
  `SaveFormat.md` names the same inventory. Under the direct-`master` policy,
  the fresh local gate is the landing authority; no CI result is awaited or
  claimed, and S7 has no human or visual stop at this boundary.
- **Frozen model:** catch placement is party-first, then deterministic across
  **16 boxes x 30 slots**; full capacity rejects without mutation. The aggregate
  stores seen/caught dex state, **4096** story bits, **8** badges, money, daycare
  state, Battle Tower seed, an explicitly unset world position, NORMAL default
  options, and per-creature friendship/nickname. Newly caught battle records
  normalize `ABILITY_NONE` to the species regular ability.
- **What SC1 does not claim:** there is no binary codec, schema/module version
  handling, fixed offset/size contract, golden file, migration path, corrupt- or
  unknown-version rejection, or transactional slot I/O yet. Therefore the
  Roadmap's full versioned `ZM_SaveSchema` checkbox remains unchecked. SC2 owns
  the transactional 11-module codec and its initial v1 golden.
- **Tests that lock it:** **18 new `ZM_Save` units** cover defaults, range
  boundaries, idempotence, party-first and first-free box placement, box
  rollover/full-capacity rejection, nickname/friendship state, ability
  normalization, and save-state mutation isolation. They are included in the
  observed boot-unit total below.
- **Fresh closure evidence:** regen passed; all five required serial builds
  passed -- Vulkan Debug/Release x Tools true/false plus D3D12 Debug
  Tools=false. Boot units were **2361 ran / 2360 passed / 0 failed / 1 skipped**;
  the separate engine-only reference remained **1103**. The automated registry
  remained **36**: headless **36/0**, with three semantic executions and 33
  expected graphics skips; full windowed **36/0/0**, with every test producing
  positive frames.
- **Reversibility / next boundary:** the in-memory aggregate can evolve before
  SC2 freezes its byte representation, but any change after the v1 golden will
  require explicit version/migration handling. Work proceeds autonomously to
  SC2's transactional 11-module codec and initial v1 golden. The next free ECS
  serialization order remains **114**; no human intervention is required before
  the S8 vertical-slice gate.

---

## 2026-07-21 -- ZM-D-134 -- S6 COMPLETE after the fresh local SC9 closure gate

- **Decision:** close S6 (dialogue, menus, NPCs and shops). The full SC9 local
  gate is green and is the landing authority under the direct-`master` policy;
  `zm-tests` remains an asynchronous post-push backstop, so no CI result is
  awaited or claimed here. S6 has no visual gate and requires no human stop.
- **Delivered boundary:** the shipped S6 runtime has dialogue, menu-stack,
  party, bag, dex, shop and Care Center flows; four authored Dawnmere NPCs
  across three roles;
  one role-dispatch seam; physics-driven walk-up interaction; and the
  deterministic two-point `Npc_Wanderer` with dialogue halt/resume. Behaviour
  graphs and navmesh-driven wandering are deliberately not represented as S6
  deliverables.
- **Graph/navmesh deferral and factual terrain correction:** the first useful
  `ZM_GraphAuthoring` integration and trainer/navigation evaluation move to S7.
  `Zenith_NavMeshGenerator::GenerateFromGeometry` can process caller-supplied
  triangle geometry, including suitable terrain triangles; the convenience
  `Zenith_AINavGeometry::GenerateFromScene` path does **not** harvest streamed
  terrain geometry or a terrain heightfield -- it represents static colliders
  as box geometry. Therefore terrain navigation is not already available via a
  one-call scene scrape. S7 owns the terrain-triangle or grid-coverage source,
  the 1024 m Dawnmere coverage decision, and any `.znavmesh` persistence work.
- **Document authority:** `MasterPlan.md` is historical/read-only. Its graph-led
  NPC wording records intent at the time, not current shipped truth, and it is
  not to be rewritten as a living plan. Roadmap.md, Status.md and this
  append-only log carry the current stage boundary and rationale.
- **Fresh closure evidence:** five serial builds passed -- Vulkan Debug/Release
  x Tools true/false plus D3D12 Debug Tools=false. Boot units were **2343 ran /
  2342 passed / 0 failed / 1 skipped**; the separate engine-only reference
  remained **1103**. The automated registry was **36**: headless **36/0**, with
  three semantic executions and 33 expected graphics skips; full windowed
  **36/0/0**, with no zero-frame tests. The six S6 windowed filters all ran and
  passed without skips: `ZM_S6UIGate_Test` **158 frames**,
  `ZM_NpcTalk_Test` **85**, `ZM_NpcShop_Test` **286**,
  `ZM_NpcHeal_Test` **315**, `ZM_S6InteractGate_Test` **749**, and
  `ZM_NpcWander_Test` **830**.
- **Reversibility / next boundary:** the graph and navigation deferrals are
  reversible by additive S7 work through the seams S6 retained; the shipped
  `ZM_Interactable` v2 persistence contract is save-affecting and any incompatible
  rollback requires another versioned migration. This closure remains immutable
  history and may only be superseded by a later decision. S7 starts with the
  full versioned `ZM_SaveSchema`; the next free ECS serialization order is
  **114**. Autonomous work continues through S7, and the next human intervention
  point is the S8 vertical-slice gate.

---

## 2026-07-21 -- ZM-D-133 -- S6 item 3 (SC8): deterministic authored waypoint patrol + a causal moving-NPC dialogue proof

- **Decision:** ship `ZM_NpcWalkerLogic` as a pure, deterministic authored-waypoint state machine and keep its ECS/physics glue inside the existing `ZM_Interactable` (order 113). The pure step consumes fixed waypoints, explicit cursor/dwell state, position, fixed dt, halt and tuning; it steers only in XZ, has no RNG, installs/consumes authored dwell deterministically, freezes both cursor and dwell while halted, and emits a patrol velocity that replaces XZ while preserving the body's live Y velocity. Runtime movement uses a **dynamic capsule** and `Zenith_Physics::SetLinearVelocity`, never transform teleportation.
- **Persistence contract:** `ZM_Interactable` data is v2 with patrol enable/count/points/tuning serialized alongside the NPC role. Cursor/dwell is session-only and restarts deterministically at point zero on load. A v1 read is an explicit **stationary fallback**: wandering remains disabled and no implicit waypoint data is invented. Dialogue halt is ownership-specific -- the walker stops only while the open dialogue belongs to that interactable, then resumes from its preserved in-session patrol state after the dialogue closes.
- **Authored content:** Dawnmere now has `Npc_Wanderer` on the isolated **x=540, z=476..484** two-point lane, clear of the existing NPCs and traversal corridors. Its initial transform carries safe clearance above the sampled terrain. The first height placed the dynamic capsule partly through the terrain; because the mesh is one-sided, it penetrated from the non-colliding side and fell instead of settling. The fix is authored initial clearance -- not a pinned Y, kinematic body or corrective teleport -- so terrain collision remains authoritative.
- **Why:** S6 needs a bounded, repeatable wandering-NPC interaction proof without pulling S7's deferred navigation scope forward. Fixed authored waypoints make rendezvous and halt/resume assertions deterministic; keeping the motor pure gives exact headless unit coverage, while the dynamic-body windowed test proves the integration path the pure units cannot. Versioned persistence prevents old v1 scenes from acquiring accidental motion.
- **Tests that lock it:** **18 new pure units** cover empty/single/two-waypoint behavior, heading and XZ/Y separation, inclusive arrival boundaries, cursor advance/wrap, dwell start/expiry, halt freezing cursor+dwell, finite coincident output, cloned-state exact determinism, and vertical-velocity preservation. The camera-relative, fixed-**1/60** `ZM_NpcWander_Test` proves consecutive displacement/body-velocity coupling, waypoint advance, approach to the **moving** named entity, exactly its own dialogue/content, pre-E coupled motion with waypoint-cursor continuity across E, **30 consecutive samples** of active-dialogue ownership with low drift/speed, at least one explicit Enter edge before closure, player freeze/unfreeze, and six consecutive coupled resume samples. It passes windowed in **830 frames**.
- **Observed local gate:** regen GREEN; Zenithmon Vulkan Debug True GREEN; D3D12 Debug False null-backend link proof GREEN; boot units **2343 ran / 2342 passed / 0 failed / 1 skipped**; headless **36/0** (the graphics-required wander proof skipped as expected); full windowed **36/0 with zero skips**. These are local observations only -- no commit, push or CI result is claimed by this entry.
- **Stage boundary / reversibility:** SC8 completes implementation, not S6's documentation closer: the final Roadmap line remains unchecked until SC9 reconciles the deferred graph/navmesh wording and runs the full stage gate. S6 has no visual/human stop; the next human stop remains S8. The pure walker and v2 fields are localized but become save-affecting once v2 scenes ship; disabling an authored patrol is easy, removing/reordering serialized state would require another versioned migration.

---

## 2026-07-21 -- ZM-D-132 -- The RenderTest canary is restored, and the ECS duplicate-serialization-order gap is CLOSED -- the first S6-era change to move the ENGINE baseline

- **Decision:** two paired tasks. (1) Bake RenderTest's terrain so the engine-change canary the AgentBriefing gate names actually works again. (2) Fix the tracked engine gap it unblocks -- `Zenith_ComponentMetaRegistry::Finalize()` had NO duplicate-serialization-order detection. This is the **first change since S6 began to touch `Zenith/`**, so the "engine baseline 1097 UNCHANGED" invariant every prior S6 entry asserts is deliberately retired here: **1097 -> 1103**.

- **The bake.** RenderTest generates its terrain procedurally at boot from seed 1337 via tools-only editor automation, so a single `_True` boot bakes it: **12,313 files / 1.78 GB**. Result: the `127 missing/invalid terrain chunks` warning and the boot-time crash are gone, `--list-automated-tests` boots clean, and **`TerrainEditorSmoke` -- the actual terrain/grass canary -- PASSES windowed.** It was then used as a genuine canary for the engine change below and stayed green.
- **★ THE Q-2026-07-16-001 PREMISE WAS ONLY HALF RIGHT, and measuring it is what showed that.** That question said RenderTest "CRASHES ... because RenderTest's baked terrain is absent". The bake was NECESSARY but NOT SUFFICIENT: with terrain fully baked, `zenith test RenderTest --headless` still asserts `Invalid buffer VRAM handle`, and the measured stack is `Zenith_TerrainComponent::InitializeCullingResources() - Setting up GPU-driven terrain culling`. The terrain path sets up GPU-driven culling **unconditionally, with no headless guard**, and a headless boot has no Vulkan device. That is a **separate, still-open engine gap** (Q-2026-07-21-001), not an asset problem, and it means the terrain path has no CI coverage on a GPU-less runner at all. It does not block the canary, which is run windowed like every local gate here. Residual and unrelated: RenderTest windowed is 8/1, `RT_TennisDeterminismDigest` failing for pre-existing tennis reasons (Q-2026-07-21-002).

- **The engine fix.** `Finalize()` now (a) tie-breaks its sort on the type name, (b) logs a `Zenith_Error` per colliding pair naming both components and the shared order, (c) emits a summary `Zenith_Check`. **The tie-break is not cosmetic:** `std::sort` is not stable and the pre-sort source is a hash-map walk, so two components sharing an order had no reproducible ordering at all -- it could differ between builds. Since serialization order decides the byte order components are written in, that made a collision a source of nondeterministic scene output. With zero collisions (the state every shipped game is in -- verified across the engine built-ins, AI, and all five games) the tie-break never fires and the sort is provably identical to the old one.
- **★ `Zenith_Check`, NOT `Zenith_Assert` -- and the first draft got this wrong.** I initially wrote `Zenith_Assert` with a comment claiming a shipped build would keep booting. **That comment was FALSE**, and checking it rather than asserting it is what caught it: `Zenith/Core/Zenith.h` `#define`s `ZENITH_ASSERT` **unconditionally, one line above its own `#ifdef`**, so `Zenith_Assert` calls `Zenith_DebugBreak()` in EVERY config including Release. It would have hard-broken a shipped player's game over a developer's numbering mistake. Worse, it fires during `Finalize()` at engine init -- **before the boot unit suite** -- so it pre-empted `DuplicateOrders_LiveRegistryHasNoCollisions`, the very unit written to catch this, which could then never report. Switched to `Zenith_Check` (logs, continues), which makes the unit the gate that actually fails. **This would have been the fifth false engine claim in a comment; it was caught pre-commit by verification rather than by review.**
- **Units: 6, hosted ENGINE-SIDE on purpose.** `ZenithECS` is an L1 leaf that may depend only on `ZenithBase`, and the test framework lives in `Zenith/Core` -- a `.Tests.inl` inside the leaf would break both the `SentinelECS` link proof and the ECS-leaf ratchet. They live in `Zenith/EntityComponent/Zenith_ComponentMetaRegistry.Tests.inl`, included from `Zenith_ComponentMeta_Registration.cpp`, which `Zenith_Engine.cpp` takes the address of -- so the linker must pull that `.obj` and the static registrations survive `/OPT:REF` (the documented `Zenith_Physics.Tests.inl -> Zenith_ColliderComponent.cpp` idiom). Five units cover the new pure `CountDuplicateSerializationOrders`; the sixth, `DuplicateOrders_LiveRegistryHasNoCollisions`, runs it against the **actual registry this build ships** and is the one that turns a future copy-pasted order into a boot-time failure in every game.
- **★ MUTATION-VERIFIED:** giving `ZM_Interactable` the same order as `ZM_UI_MenuStack` logs `DUPLICATE serialization order 112 shared by 'ZM_Interactable' and 'ZM_UI_MenuStack'`, boot CONTINUES, and both the new engine unit and Zenithmon's own `GateRoster_InteractableIsRegisteredExactlyOnce` go RED. Restored and re-gated afterwards.

- **★★ THE REVIEW CAUGHT TWO BLOCKERS -- both CI gate breakage I would have shipped.** The +6 engine units move a baseline that is pinned in more places than the one I bumped. (1) `Tools/run_unit_gate.ps1`'s **default** `-Baseline 1097` is an exact-equality gate, and **`.github/workflows/engine-gate.yml` and `Tools/test_scaffold.ps1` both invoke it with NO `-Baseline`**, so both would have gone red at 1103. (2) `zm-tests.yml` was still pinned at 2319 against an observed 2325. Both bumped and re-verified by running the gates at their new defaults. Lesson: **an engine baseline is pinned in a script DEFAULT as well as in per-game workflow arguments -- grep for both.**
- Also applied from review: the two adjacency walks in `Finalize` now agree about their null precondition (the reporting loop could have dereferenced on the way to logging a collision); `m_uSerializationOrder` gained a default member initializer; the test helper lost a stray `ZM_` prefix (the Zenithmon game prefix, in engine code); and the `.Tests.inl` includes the test framework explicitly rather than relying on the PCH.
- **Cross-game gate (the full engine-change gate):** Combat **1103/0** + suite 14/0; DevilsPlayground **1104/0** + suite 158/0; CityBuilder **1104/0** + suite 45/0; Zenithmon **2325/0**, headless 35/0, windowed 35/0; RenderTest terrain canary green. `Tools/run_unit_gate.ps1` default 1097 -> 1103; `zm-tests.yml` 2319 -> 2325.

---

## 2026-07-20 -- ZM-D-131 -- Known-bug sweep: four verified defects fixed, and the two that mattered were MUTATION-PROVEN

- **Decision:** a dedicated bug-fix pass over the project's own known-defect registries (Shortfalls.md, Questions.md) plus the defects this session's mutation testing had exposed but not yet fixed. **GAME-ONLY -- nothing under `Zenith/` was touched**, so the engine unit baseline is unchanged and no cross-game regression is owed. Four fixes landed.

- **B1/B2 -- the traversal drive picked keys in the WRONG FRAME, in both remaining copies.** ZM-D-130 fixed this in the SC6/SC7 walk machine and left the other two copies commented-but-broken. Both are now corrected: `Tests/ZM_AutoTests_NpcTalk.cpp` and `Tests/ZM_AutoTests_WorldTraversal.cpp` project the desired world direction onto the LIVE camera basis (the exact inverse of `ZM_PlayerController::BuildCameraRelativeDirection`) instead of comparing raw world dx/dz. **★ A FALSE CLAIM OF MY OWN WAS CAUGHT HERE.** My brief asserted that `ZM_PlayerHomeRoundTrip_Test` "walks out to a door and back, a ~180-degree heading change, and was passing on tolerance". The review verified against source that BOTH halves are false: the outbound leg is 128 m straight along -X from the TownCenter spawn with a ~90-degree turn for the final 4 m, and the return leg happens **after a warp into build 40, in a different scene with a different camera entity**, i.e. another single-leg-from-rest walk -- precisely the regime where the world-space chooser was safe. Nothing measured that test's margin. The comment now says what is provable: this file's walks were in the safe regime, and the camera-relative version is adopted because it is correct **in general**, not because a failure was measured here. Same class of error as the four previously-logged fabrications, and it was mine.
- **Measured consequence, disclosed:** `ZM_PlayerHomeRoundTrip_Test` moved **673 -> 831 frames**. Verified DETERMINISTIC (831 exactly on three runs) and sitting at 54% headroom under its 1800-frame cap. The camera-relative drive continuously re-aims against a spring camera, so it traces a slightly curved path where the world-space version held a fixed key set; both arrive, and only one is correct by construction.

- **B3 -- the ONE CI-visible interaction test could not see a wrong screen.** `ZM_NpcDispatch_Test` is the only interaction test that runs in `zm-tests` (every other is `m_bRequiresGraphics = true`, and headless SKIPS those while counting the skip as a PASS). It asserted that *a* screen was raised and that the winning entity was right, but never WHICH screen -- so a dispatch arm pointed at the wrong seam was invisible to CI, exactly as ZM-D-129's mutation demonstrated. It now drives all three roles and asserts the raised screen per role. TALKER and CARETAKER both raise DIALOGUE, so the top-screen check alone cannot separate them: the caretaker is pinned by `GetPendingDialogueAction() == ZM_DIALOGUE_ACTION_HEAL_PARTY` plus `IsChoiceArmed()`, and the shopkeep additionally by the SHOP screen carrying **that NPC row's own stock**, read from `ZM_GetNpcData` at runtime and never re-spelled as a literal. It remains honestly headless (no baked asset, no graphics).
- **★ MUTATION-PROVEN:** rewiring the SHOPKEEP and CARETAKER arms of `ZM_Interactable::Interact()` to `TryPushDialogue` -- the mutation that left this test GREEN under ZM-D-129 -- now turns it **RED**. The CI hole is closed.

- **B4 -- the battle menu offered "Catch" unconditionally, ignoring `m_bCanCatch`.** A tracked Shortfalls 1.5 deferral with a named failure mode: `ZM_BattleEngine::SubmitAction` and `DoItemAction` both `Zenith_Assert(m_xConfig.m_bCanCatch, ...)`, and `ZM_BattleTower` already produces a catch-disallowed config, so this was a reachable state rather than a hypothetical. The core now surfaces `ZM_BattleDirectorCore::IsCatchAllowed()` (fail-closed -- false before `Begin`), the root menu resolves a cursor to an ENTRY through a new `MenuRootItemAtIndex(index, bCanCatch)`, and the Catch entry is absent entirely when catching is disallowed. The root enumerators are now documented as entry identities that are **not** cursor indices, with a `static_assert` pinning the hand-written 3-entry mapping. The hidden Catch row no longer leaves a visual hole: visible buttons are placed by resolved entry index from constants shared with the authoring site, so the two cannot drift.
- **★ THE REVIEW CAUGHT THAT THE FIX WAS UNPINNED.** Five new units covered the pure statics, but changing `IsCatchAllowed()` back to a hard-coded `true` left **every test in the repo passing** while fully restoring the original bug -- nothing connected the flag to `MenuItemCount` / `MenuConfirm`. A sixth unit now drives the REAL `UpdateMenu` with real key edges against a catch-disallowed config and asserts the emitted action is RUN and never ITEM, plus the mirror case so it proves the gate rather than "index 1 is never a catch". **Mutation-verified RED.**

- **Deliberately NOT fixed, and why:** `Zenith_ComponentMetaRegistry::Finalize()` has **no duplicate-serialization-order detection** -- two components at the same order sort arbitrarily with no warning. It is a real latent engine hazard (Zenithmon occupies orders 100-113), but it is an **engine change**, which moves the engine unit baseline and owes a Combat / DevilsPlayground / CityBuilder cross-game regression plus a RenderTest boot check. Bundling it into a game-only pass would have made the diff ungateable. Logged as **Q-2026-07-20-002** with the game-side spot check that already guards order 113.

- **Documentation:** Shortfalls.md was substantially stale and is rewritten in place -- the verdict-at-a-glance, the S3 and S5 gate statuses (both signed off), section 1.6 (the whole S6 UI is shipped, not "the remaining sub-commits"), section 1.8 (Dawnmere now has three interactable NPCs), section 1.10 (six registered tests -> 35, plus the current gate numbers), and the E4 row (RESOLVED). Three discharged forward-notes are marked as such. Three NEW cross-cutting risks are recorded: the camera-relative-movement rule, the **windowed-tests-are-invisible-to-CI** structural caveat, and the deferred engine gap.
- **Tests that lock it:** +6 units (5 pure catch-gating + 1 live-wiring drive) and the hardened `ZM_NpcDispatch_Test`. **OBSERVED boot count 2313 -> 2319**; `zm-tests.yml` bumped to 2319; windowed suite stays **35/0**; engine baseline **1097 UNCHANGED**.
- **Gate:** no regen owed (no new file); build clean; boot units **2319 ran / 2318 passed / 0 failed / 1 skipped**; headless **35/0**; full WINDOWED **35/0**; both mutation rounds run and the source restored, rebuilt and re-gated green afterwards.

---

## 2026-07-20 -- ZM-D-130 -- S6 item 3 (SC7): the consolidated S6 gate -- and the traversal drive every walk-up test inherited was picking keys in the WRONG FRAME

- **Decision:** `ZM_S6InteractGate_Test` lands in `Tests/ZM_AutoTests_NpcServices.cpp`, proving all four clauses of the S6 gate sentence **through NPCs in ONE uninterrupted session** with no scene reload and no between-tests reset: **talk** (walk to `Npc_Villager`, E, the villager row's own first line), **buy** (walk to `Npc_TradePostClerk`, E, focus-navigate a purchase, exact money and bag deltas), **heal** (damage the party and assert `ZM_PartyNeedsHealing`, walk to `Npc_Caretaker`, E, arrow to Yes, `GetLastDialogueAnswer() == YES`, party restored, healed line shown) and **open every menu** (M, then Party / Bag / Dex / Exit purely by arrow / ENTER / ESCAPE edges), ending with a CLEAN EXIT beat that proves the player is unfrozen and still responds to held W. **PASSED, 749 frames.** Two new units land in the existing `Tests/ZM_Tests_Interactable.cpp`, so **no new file and no regen**.
- **The walk machine is REUSED, not forked (RULING H).** A new `RetargetWalk` points the existing `WalkContext` at a new authored entity and re-enters Approach, resetting only the per-target bookkeeping (id / position / distances / stall counter / displacement origin / raise baseline) and deliberately keeping the already-earned Boot, both basis legs, the out-of-range negative and the physics-motion evidence. So those run EXACTLY ONCE, against the villager, and the gate is three approach-and-press legs on one proven machine. `RearmWalkApproach` is untouched, so `ZM_NpcHeal_Test`'s NO pass is unaffected.
- **★★ THE ROOT-CAUSE FINDING: `DriveTowardXZ` was choosing keys in WORLD space, but player movement is CAMERA-RELATIVE.** The gate failed on its THIRD leg (clerk -> caretaker). The diagnostics -- built for exactly this -- reported the stall at (501.3, 486.1), 12 m short in z, with W+A held and `OUT_OF_RANGE`. A frame trace then showed the player travelling a **stable 45-degree -X-Z heading** at full run speed while holding W, receding from 9.25 m to 12.34 m. **It was not an obstruction, not physics, not a frame budget.** `ZM_PlayerController::OnUpdate` reads the main camera's facing dir and builds `xForward = flatten(cameraForward)`, `xRight = (xForward.z, 0, -xForward.x)` (`ZM_PlayerController.cpp:140-147, 244-271`), and `ZM_FollowCamera` re-aims itself at the player every frame from a **lagging** camera-to-player vector (`ZM_FollowCamera.cpp:309-323`). So the camera's facing swings as the player turns and the world-space meaning of W/A/S/D rotates with it. Picking keys from raw world dx/dz is correct **only while the camera is still near its authored yaw** -- i.e. for a SINGLE leg walked from rest, which is all any shipped test had ever done. **Fixed by inverting the controller's own mapping**: project the desired world direction onto the LIVE camera basis and choose keys from those two components. At yaw 0 this degenerates to the old behaviour exactly, so the already-green single-leg walks are unchanged -- and measurably better: `ZM_NpcShop_Test` went 320 -> 286 frames and `ZM_NpcHeal_Test` 365 -> 315, because the drive now corrects mid-walk instead of fighting the swing.
- **The same latent bug sits in the other two copies of `DriveTowardXZ`** (`ZM_AutoTests_NpcTalk.cpp` and the shipped world-traversal test). Both are SAFE today because each walks one leg from rest. `ZM_AutoTests_NpcTalk.cpp` now carries a comment stating the measured limit and pointing at the corrected version. **Rule for SC8 and the S9/S10 content waves: copy the camera-relative drive from `ZM_AutoTests_NpcServices.cpp`, never the world-space one.**
- **★★ A MUTATION TEST THAT FAILED TO BITE, and what it exposed.** The review's BLOCKER was that the re-raise negative had no assertion depending on the E press at all: `EvaluateForTests` **hard-codes `bInteractPressed = true`** (`ZM_InteractionRuntime.cpp:99-103`), so its `MENU_OPEN` answer is press-INDEPENDENT; the raise-count clause is dominated by the player freeze; and "top screen is still DIALOGUE" cannot catch a confirming E. The fix added a line-index / line-text pair across the press -- and **mutation-testing it by swapping the key for `ZENITH_KEY_ENTER` (a real confirm key) STILL PASSED.** Reason, measured not assumed: a single confirm on a dialogue whose typewriter is still running only **completes the reveal** (`ZM_DIALOGUE_ADVANCE_COMPLETED_REVEAL`) and never advances the line. **One press can therefore never detect a confirming interact key.** The negative now emits **six** spaced interact edges -- enough that a confirming key would finish all three villager lines and CLOSE the box -- plus an assertion that the six edges were actually emitted, so a phase that pressed nothing cannot satisfy the "unchanged" clauses. Re-mutated: the phase now **reds at 159 frames**. Without the mutation round this fix would have shipped looking rigorous and proving nothing.
- **Review (4 lenses): 1 BLOCKER + 9 lesser defects, all fixed.** Beyond the blocker: `ShopSettle`'s "the mart is still the top screen after it closed" was true BY CONSTRUCTION (`Top()` returns `NONE` at depth 0), replaced with `GetDepth() == 0` + `HasResult() == false`, which only the close can get wrong; **the first menu visit reached its ROOT entry with ZERO arrow edges** (the ROOT presenter parks focus on `Menu_RootParty`, which was visit 0's target) and `ZENITH_KEY_UP` was never emitted anywhere -- fixed by restoring the item-2 gate's `ProbeNavDown` / `ProbeNavUp` pair; the heal focus/frozen diagnostics were captured but never logged; `g_uGateDepthAfterExit` was captured, documented "want 0" and never asserted; three screen headers were used but only transitively included; and the registration unit's name-uniqueness half was a container tautology (the meta map is KEYED by name), leaving only the genuinely-unpoliced ORDER uniqueness.
- **A diagnostic that destroyed its own evidence.** `FailWalk` calls `ClearWalkInput`, which zeroes `m_abHeldKeys` -- so every stall printed "held W=0 A=0 S=0 D=0" regardless of what the walk was doing. The held set and the live player position are now snapshotted BEFORE the clear, and the approach line names the live target. Without that fix the camera-frame bug above would not have been diagnosable.
- **Unit scope, honestly reported.** The spec'd "every role is covered by the roster" unit **already shipped** as `Npc_RolesCoverEveryDispatchArm`, so it was re-scoped to the strictly stronger property the gate actually needs: every role is covered by the **scene-placed** NPCs (the roster includes `ZM_NPC_WANDERER`, which nothing places until SC8). It catches re-rolling the villager to SHOPKEEP, which every existing unit passes. The registration unit deliberately does **not** assert the literal `113`: serialization is name-keyed, the header forbids renumbering as a fix, and it would restate `Zenithmon.cpp` back to itself.
- **Tests that lock it:** `ZM_S6InteractGate_Test` (windowed, 749 frames, ~26 default-failing flags) + `GateRoster_PlacedNpcsCoverEveryRole` and `GateRoster_InteractableIsRegisteredExactlyOnce`. **+2 units, OBSERVED: zm boot 2311 -> 2313**; windowed suite **34 -> 35**; engine **1097 UNCHANGED** (no `Zenith/` file touched).
- **Gate (dev machine):** no regen owed (no new file); build clean; boot units **2313 ran / 2312 passed / 0 failed / 1 skipped**; headless **35/0**; **full WINDOWED 35/0** with `ZM_S6InteractGate_Test` PASSED (749 frames), `ZM_NpcShop_Test` (286), `ZM_NpcHeal_Test` (315), `ZM_NpcTalk_Test` and `ZM_PlayerHomeRoundTrip_Test` (673) all green; both mutation rounds run and the source restored, rebuilt and re-gated afterwards.

---

## 2026-07-20 -- ZM-D-129 -- S6 item 3 (SC6): buying and healing BY WALKING -- and the walk-up tests were MUTATION-PROVEN, which exposed that the one CI-visible test cannot see the difference

- **Decision:** `ZM_NpcShop_Test` and `ZM_NpcHeal_Test` land in ONE new TU, `Tests/ZM_AutoTests_NpcServices.cpp` (2.7k lines), completing the gate's "buy" and "heal" clauses through real NPCs. Both PASS windowed with real frame counts: **shop 320 frames, heal 365 frames**. SC5 proved a player can walk up and TALK; these prove the other two roles -- walk to `Npc_TradePostClerk` (526, ~26.89, 498), press E, buy; walk to `Npc_Caretaker` (498, ~26.89, 498), press E, answer YES *and* NO.
- **ONE file, ONE copy of the walk machinery (ruling).** There is no shared test-helper header in `Tests/` (the directory is `.cpp` only) and every helper in `ZM_AutoTests_NpcTalk.cpp` / `ZM_AutoTests_UI.cpp` sits in a per-file **anonymous namespace**, so a new TU cannot reach any of it. Rather than copy the walk twice, the file carries ONE parameterised `WalkContext` / `TickWalk` machine that both tests drive; **SC7's consolidated gate lands in this same file and reuses it a third time.**
- **Fixed dt is 1/60, not the UI tests' 1/30 (ruling).** These tests combine a physics-coupled WALK with menu-driving phases; the walk is the risky half and must match the proven `ZM_NpcTalk_Test` exactly. Consequence applied throughout: **every frame budget copied from a 1/30 test is DOUBLED** (walk-to-confirm 200 -> 400, close 120 -> 240, read 120 -> 240, choice walk 160 -> 320, ready 420 -> 840). The `press on frame 2 / sample on frame 6` and `(frames % 4) == 1` patterns are frame-based, not time-based, and are unchanged.
- **★ THE BASIS PROBE IS NOW TWO LEGS, because the X axis had NO characterisation anywhere.** SC5 placed the villager at pure +Z *precisely because* +Z was the only movement axis with evidence, and its probe holds W only. **Both SC6 targets are DIAGONAL** (dx = +/-14, dz = +18), so the strafe leg of these walks was unproven. The probe now runs W for 30 frames (dz >= 0.5, |dz| > |dx|) then D for 30 frames (dx >= 0.5, |dx| > |dz|). Honest limit, stated in the code: leg 2 exercises the **+X sign only**; the caretaker walk uses -X (key A), the same multiply with the opposite sign. Verified `BuildCameraRelativeDirection` computes `xRight(forward.z, 0, -forward.x)` and `BuildHorizontalVelocity` **sets** (not blends) velocity, so leg 1 carries no momentum into leg 2's dominance check.
- **★ THE APPROACH NOW REPORTS THE LIVE REJECT REASON, which is how the height assumption becomes legible.** ZM-D-128 recorded that all three NPCs reuse ONE sampled feet height and that **the clerk and caretaker had no test**, so "treat a mute one as a height check first". These two tests close that gap -- and to make a future failure diagnosable, the approach records `ZM_InteractRejectName(EvaluateForTests(...))` every frame and prints it on a stall/timeout. A height drift past the +/-2 m band now reads as `OUT_OF_VERTICAL_BAND`, not as a mystery walk timeout.
- **The stock assertion is the one `ZM_ShopScreen_Test` structurally cannot make.** That shipped test configures the screen from a fixture array declared inside the test, so it can never prove an NPC's own data reached the screen. This one asserts `GetInventoryCount()` and **every** `GetInventoryItem(u)` against `ZM_GetNpcData(ZM_NPC_TRADE_POST_CLERK)` read at RUNTIME -- never re-spelled as a literal, which would degrade it to "the table equals my copy of the table". The bought item is likewise never hardcoded: the flat entry is read via `GetSelectedEntryIndex(bag)` and priced via `ZM_ShopBuyPrice` at the press frame, so the money delta is `price * qty` exactly and the bag delta is `qty` exactly.
- **★★ MUTATION-PROVEN, and the mutation found something.** Per the standing rule that a test claimed to "have teeth" must be shown to bite: the SHOPKEEP and CARETAKER arms of `ZM_Interactable::Interact()` were both rewired to `TryPushDialogue`, rebuilt, and re-run. **`ZM_NpcShop_Test` and `ZM_NpcHeal_Test` both went RED; `ZM_NpcTalk_Test` stayed GREEN** (the TALKER arm was untouched, so the mutation was targeted). **But `ZM_NpcDispatch_Test` -- the ONLY one of the four that runs in CI -- stayed GREEN too.** It asserts that *a* screen was raised, not *which*, so a dispatch arm pointed at the wrong seam is invisible to `zm-tests` entirely. The source was restored, rebuilt, and the full gate re-run before any of the green results below were recorded. **This sharpens ZM-D-128's CI-visibility warning from "the walk is not covered" to "the SCREEN CHOICE is not covered either".**
- **Review (4 adversarial lenses): NO BLOCKERS, 14 defects, all applied.** The load-bearing one: **`ZM_UI_MenuStack::m_eLastDialogueAnswer` survives every test boundary.** It is cleared only in `OnStart` and `ReadFromDataStream`; `CloseMenu` deliberately does not touch it (a resolved prompt closes the menu and the answer must outlive that pop) and `ResetRuntimeStateForTests` only called `CloseMenu`. `ZM_MenuRoot` is `DontDestroyOnLoad` and a batched process never re-boots, so if an Enter on Yes were ever dropped the box would stay awaiting, the in-Step guard would still pass, and `GetLastDialogueAnswer()` would return **a previous test's answer** -- the assertion the file bills as its host-latch proof was not itself falsifiable. Fixed on BOTH sides: the tests now assert `!IsDialogueAwaitingChoice()` (the choice genuinely resolved) before reading the latch, and **`ResetRuntimeStateForTests` now clears the latch** -- the one runtime edit in this sub-commit. Deliberately NOT fixed as "capture the answer before the press and require it to change": a prior test ending on YES would make that a false failure.
- **★ A FOURTH WRONG CLAIM, and again in an argumentative passage.** Two lenses independently found that `HasLatchedResult()` does **not** prove the press reached the runtime: `ZM_InteractionRuntime::Tick` sets `s_bHasLatchedResult = true` **unconditionally**, outside the `if (bInteractPressed)` branch (the runtime source says so itself). It only distinguishes "the runtime ticked at all". The EDGE proof is `GetLastResult() == OUT_OF_RANGE`, since `s_eLastResult` is written only under the edge and resets to `NO_INPUT_EDGE`. Both checks were present so nothing was vacuous -- but the comment named the wrong one as load-bearing, **and that comment is inherited verbatim from the shipped `ZM_AutoTests_NpcTalk.cpp` (SC5)**. Corrected in BOTH files, comment-and-message text only in the SC5 file (verified by diff: not one statement changed; it still passes). Same correction applied to a second inherited overclaim -- the out-of-range guard's "beyond ANY per-NPC reach bonus an author could add", which is false because `ZM_Interactable::fMAX_RADIUS` is 8.0 (max authorable reach 10.5 m vs the 5.0 m guard).
- **Other applied fixes worth carrying forward:** the shop quantity guards could not fire (`GetQuantity()` runs through `ClampQuantity`, which floors at 1) -- now asserts `== 1`, the reachable open-state default; the "Yes/No buttons hidden after the close" clause proved nothing about the close (`PresentChoiceButtons` hides them the instant `IsAwaitingChoice()` goes false, phases earlier) -- replaced with the panel/text hidden, which only `Hide` drives; `MenuFocusCleared()` passed vacuously on an unresolved `ZM_MenuRoot` -- now fails; the heal test's `m_iMaxFrames` was **below** the sum of its own phase deadlines (4800 vs 5478), so the harness cap silently pre-empted every phase's own diagnostic -- raised to 6000, above the sum, with the sum written into the comment; and the walk's default failure string ("the walk-up machine never ran") lied on a frame-cap termination.
- **A comment that stated a FALSE engine fact, now corrected:** the justification for `ClearWalkInput` omitting the arrow keys claimed a key release "would eat the press outright". `Zenith_InputSimulator::SetKeyHeld` writes only the LEVEL array, while every menu consumer reads the EDGE via `WasKeyPressedThisFrame` off a separate array -- a release cannot consume an edge. The real reason (now stated) is that every menu edge here is a one-shot `SimulateKeyPress` that auto-releases at frame end, so there is no held arrow state to clear.
- **Tests that lock it:** windowed `ZM_NpcShop_Test` (boot / two-leg basis probe / out-of-range negative / watchdogged approach / event-driven press / raise by entity identity + `ZM_MENU_SCREEN_SHOP` + depth 1 + player frozen / the clerk row's stock on the screen / real Down edges onto Confirm / selection survival pinned BY VALUE across the walk / exact money and bag deltas / Escape out with movement restored) and `ZM_NpcHeal_Test` (the same walk-up, then `HEAL_PARTY` pending, the prompt held open awaiting a choice, both answers proved key-reachable by real arrow edges, YES -> full party + the healed line on an still-active box, then a re-damage, a second raise, and NO -> answer NO, menu closed, **party still damaged**, so the two passes differ and neither is vacuous). **No pure units added**, so the boot count stays **2311** and `zm-tests.yml` needs NO edit (its `-Baseline` counts `ZENITH_TEST` units only -- the SC6 plan's claim that the workflow is an edited file is wrong). Windowed suite **32 -> 34**; engine **1097 UNCHANGED** (no `Zenith/` file was touched, so no cross-game regression is owed).
- **Gate (dev machine):** regen (1 new .cpp); build clean 0 errors 0 warnings; boot units **2311 ran / 2310 passed / 0 failed / 1 skipped** (the pre-existing quarantine); headless **34/0** with both new tests correctly SKIPPED; **full WINDOWED 34/0 with `ZM_NpcShop_Test` PASSED (320 frames), `ZM_NpcHeal_Test` PASSED (365 frames), `ZM_NpcTalk_Test` PASSED and `ZM_PlayerHomeRoundTrip_Test` PASSED**; then the mutation round above; then the source restored, rebuilt and the whole gate re-run green.
- **Reversibility:** high for the tests (delete the TU, re-run regen). The one runtime edit (`ResetRuntimeStateForTests` clearing the latch) is strictly stricter and independently reversible, but reverting it re-opens the cross-test contamination class.

---

## 2026-07-20 -- ZM-D-128 -- S6 item 3 (SC5): the walk-up proof LANDS -- and a scene-placement change nearly regressed an unrelated suite

- **Decision:** three NPCs are authored into the real Dawnmere block (`Npc_Villager` / `Npc_TradePostClerk` / `Npc_Caretaker`), and **`ZM_NpcTalk_Test` proves the thing this entire item exists to produce**: in the real baked world, under real simulated input, a player WALKS UP to an authored NPC and TALKS to it. **PASSED, 85 frames, genuinely running.** The wanderer is deliberately not authored -- it needs SC8's patrol, and a stationary "wanderer" would be content contradicting its own row.
- **Authoring mechanics.** `AddStep_Custom` takes a **captureless `void (*)()`**, so each NPC needs its own one-line configure function; all three delegate to one body that reaches the just-created entity via `g_xEngine.Editor().GetSelectedEntity()`, installs the row and radius, and **arms candidacy LAST** (`SetNpcId` fails closed by clearing the flag, so arming first would be silently undone by a bad id). NPCs only exist on a boot that re-authors Dawnmere -- `m_bAuthorDawnmereScene = m_bAllWarm && queueMask == 0`, i.e. the normal all-warm state.
- **★★ THE BLOCKER: a scene-placement change regressed a suite it never mentions.** All three review lenses independently found it. `Npc_Caretaker` was first authored at (492, ~26.89, **480**) -- dead centre on the corridor `ZM_PlayerHomeRoundTrip_Test` walks. That test drives from the TownCenter spawn (512, 480) to `xDoorStaging(384, 0, 480)` using `DriveTowardXZ`, which has **no obstacle avoidance**: `|dz|` sits inside its 0.08 dead zone so it holds ONLY 'A' and runs pure -X along z = 480. A solid static AABB there stops the capsule head-on at x ~= 492.8 (the 1.8 m body is far above the controller's 0.40 m step assist), the staging tolerance is never met, and that **already-green** test dies at its frame cap with a timeout naming distance, not the NPC. The new test would have passed regardless -- the damage lands in an unrelated suite. **Fixed by moving both flank NPCs to z + 18**, keeping 18 m of clearance from the Home corridor and 14 m from the x = 512 spawn-to-villager corridor. Verified: `ZM_PlayerHomeRoundTrip_Test` PASSED (673 frames) after the move. **Rule for S9/S10: before placing anything solid in a town, check the existing traversal routes -- `DriveTowardXZ` walks straight lines and will not go around.**
- **★ THE CI-VISIBILITY GAP, stated plainly so nobody is misled.** `ZM_NpcTalk_Test` is `m_bRequiresGraphics = true`, and **`zm-tests` runs headless, where the harness SKIPS such tests and a skip counts as PASS.** Confirmed: the headless batch reports `ZM_NpcTalk_Test: SKIPPED (0 frames)` and a green 32/0. **So the item's central proof is carried by the LOCAL WINDOWED gate, not by CI**, and a green `zm-tests` says nothing about whether walking up to an NPC works. The orchestrator's per-SC gate must therefore run the windowed suite and confirm this test reports `PASSED` with a non-zero frame count -- a skipped run is NOT acceptable evidence. Same caveat will apply to SC6's and SC7's walk-up tests. Only `ZM_NpcDispatch_Test` (SC4) covers this feature in CI, and it proves dispatch, not walking.
- **Test hardening from review (all applied).** (1) The out-of-range negative's assertions were satisfiable by a runtime that **never ticked at all** -- `Tick` only writes the latch when the interact edge is true, so with no edge delivered the latch stays at the reset `NO_INPUT_EDGE` and a bare `!= ZM_INTERACT_OK` passes. It now asserts `HasLatchedResult()`, the exact reason `OUT_OF_RANGE`, and a null target, so it proves a REAL refusal for a REAL reason rather than "nothing happened" -- which matters because this is the half that must survive a stubbed-true interactor. (2) The physics-displacement baseline was captured at spawn, so the 30-frame basis probe (~2 m) already satisfied it before the walk began; it is now re-captured at approach entry so the clause measures the approach. (3) The re-raise phase's raise-count check is **dominated by the player freeze, not the menu-open blocker** it claimed to prove (raising freezes the player, and `OnUpdate` early-outs on `!m_bMovementEnabled` strictly before the interaction tick, so the count is structurally incapable of moving); the `EvaluateForTests(...) == MENU_OPEN` assertion is the load-bearing one and is now commented as such, so a future simplification cannot delete the wrong line.
- **A known, bounded assumption: NPC HEIGHT.** All three reuse the ONE feet height sampled at the town centre, while the rest of this file authors per-location measured heights. The picker's band reduces to the terrain delta between the spawn and each NPC's own XZ, which this authoring never samples. Dawnmere's anchors differ by ~0.6 m over 128 m so the +/-2 m band holds comfortably, and the villager is covered by the test -- but **the clerk and caretaker have no test**, so a mute one should be treated as a height check first. S9 should author sampled per-NPC feet. Recorded in the code comment rather than left implicit.
- **Separations** (against a 2.9 m effective reach = 2.5 global + 0.4 authored): villager<->clerk and villager<->caretaker 16.1 m, clerk<->caretaker 28 m, spawn<->either flank 22.8 m. Closest pair is 5.5x reach, so the nearest-faced-candidate picker can never confuse two of them and the test asserts the winner BY ENTITY ID. Note 0.4 is the NPC's own AABB half-width; the player capsule adds another 0.4, so contact happens at ~0.8 m -- the earlier comment claiming 0.4 was the stopping distance was wrong and is corrected.
- **Tests that lock it:** the windowed `ZM_NpcTalk_Test` -- boot/settle, a 30-frame BASIS PROBE that fails fast with measured dx/dz, an out-of-range NEGATIVE, a closed-loop approach with a 60-frame progress watchdog reporting distance/best/held-keys, an EVENT-DRIVEN press (polls `EvaluateForTests` until OK **at the villager**, never "walk N frames and hope"), the raise asserted by entity identity + raise count + **the villager row's own first line on the open dialogue**, a re-raise negative, and a physics-driven motion proof. **No pure units added**, so the boot count stays **2311**; windowed suite **31 -> 32**; engine **1097 UNCHANGED**.
- **Gate (dev machine):** regen (1 new .cpp); build clean; boot units **2311 / 0 failed**; headless **32/0** (with `ZM_NpcTalk_Test` correctly SKIPPED); **full WINDOWED 32/0 with `ZM_NpcTalk_Test` PASSED (85 frames) and `ZM_PlayerHomeRoundTrip_Test` PASSED (673 frames)**.

---

## 2026-07-20 -- ZM-D-127 -- S6 item 3 (SC4): interaction goes live on `ZM_PlayerController`, not its own component -- and the coverage claim was MEASURED, not assumed

- **Decision:** `ZM_Interactable` (**ECS order 113**, the only order this whole item consumes -- next free is now **114**) marks an entity as an authored NPC and owns the SINGLE role->seam dispatch: one switch over `ZM_RaiseKindForRole` onto `TryPushDialogue(row.m_paszLines, row.m_uLineCount)` / `TryOpenShop(row.m_paeStock, row.m_uStockCount)` / `TryOpenCareCenterPrompt()`. The SC3 table's field shapes match those signatures verbatim, so the row passes straight through. Registered at **both** sites (the `ZENITH_REGISTER_COMPONENT` macro AND the `ZENITH_TOOLS` editor registry). **Every seam refusal and every unmapped role now `Zenith_Warning`s naming the NPC and role** -- closing a Shortfalls 1.6 item, since silent rejections previously made a mis-authored NPC mute with no diagnostic.
- **`ZM_InteractionRuntime` is a by-value member of `ZM_PlayerController` (order 102), NOT its own component.** The controller is already on the Player in every scene; a separate component would need an `AddStep_AddComponent` in every authored scene, and forgetting it in one S9 town would kill interaction there with nothing to catch it. Its latches are process-global statics because the between-tests hook can only reach ownerless state; the type therefore has zero data members and stays trivially movable, preserving the controller's defaulted `noexcept` move.
- **`Tick` and `EvaluateForTests` share ONE `Decide()`**, so the seam the SC5-SC7 walk-up tests poll can never drift from live behaviour. Review hardened this further: the pose is now **threaded into `Decide` as `bHavePose` rather than gated on by the caller**, because `EvaluateForTests` has no owner transform and resolves the pose from the active scene -- which legitimately fails on FrontEnd, mid-warp before the player spawns, and in the additive battle scene. Checking it caller-side reported `DEGENERATE_ORIGIN` in all three, hiding the honest `NOT_OVERWORLD` / `WARP_IN_PROGRESS`; and since `DEGENERATE_ORIGIN` *also* means "facing straight up", a poller could not tell a transient expected block from a real geometry bug. The world gate now always outranks a missing origin.
- **The latch records the last ATTEMPT, not the last frame.** Writing unconditionally clobbered it on every no-edge frame -- and nobody presses E while walking, so the latch sat permanently at `NO_INPUT_EDGE`/`INVALID`. Any later windowed Step asserting a NEGATIVE would then have passed against those clobbered values whether or not the feature worked. `s_bHasLatchedResult` still moves every tick, so "the runtime ran" stays observable.
- **A successful `Interact()` re-enters the controller** (the seams call `FreezePlayer` -> `SetMovementEnabled(false)`), so `OnUpdate` now **re-honours the flag immediately after the tick**. Without it the rest of that frame still read live input and drove the motor: the player visibly drifted and turned for one frame as the box opened. This is the first code in `OnUpdate` that can flip the flag mid-function.
- **★ THE BLOCKER, AND WHAT MEASURING IT CORRECTED.** Two review lenses independently found that `ZM_NpcDispatch_Test` constructed its OWN local `ZM_InteractionRuntime` and ticked that. The runtime is stateless with process-global latches, so the local instance behaves identically -- meaning **the entire wiring half of SC4 had zero coverage**: delete the call site from `OnUpdate` and the test still passed. Fixed by driving every tick through the real `ZM_PlayerController::OnUpdate`. **The fix was then verified by MUTATION rather than asserted:**
  - **Deleting the `m_xInteraction.Tick` call turns `ZM_NpcDispatch_Test` RED.** (measured) The fix has teeth.
  - **Moving the call below `OnUpdate`'s collider / `HasActiveSimulation` early-out leaves it GREEN.** (measured) **This falsified a premise carried from the decomposition dossier and repeated in the SC4 spec and in code comments: that early-out #4 "is precisely the headless case".** It is not. `Zenith_Physics::HasActiveSimulation()` is merely `m_pxPhysicsSystem != nullptr`, and `Zenith_Engine::Initialise` calls `Physics().Initialise()` **unconditionally, headless included** (`Zenith_Engine.cpp:556`); the fixture player also gets a valid body from `EnsureAndConfigureBody`. So that early-out never fires here. **The placement (above the physics early-out) remains correct on principle** -- interaction is transform-only geometry and must not require a live body -- **but it is NOT load-bearing for headless coverage, and no test pins it.** The old claim has been struck from the code comments in both files with the measurement recorded inline. This is the **third** wrong claim traced to the same dossier (see ZM-D-123), and again it sat in an *argumentative* passage, not a descriptive one.
- **`ZM_NpcDispatch_Test` is the ONE test in this entire item that genuinely runs in CI** -- `m_bRequiresGraphics = false`, and confirmed **PASSED (5 frames), not skipped**, in the headless batch. Every SC5-SC7 walk-up test will auto-skip headless. It builds its fixture scene via `LoadSceneByIndex(<Dawnmere>, ADDITIVE_WITHOUT_LOADING)`, which stamps the real overworld build index onto an EMPTY scene with no disk I/O, satisfying the overworld gate with zero terrain and zero streaming. It asserts the target by **EntityID identity** and that the **monotonic raise count moved** (an OK-only check would pass against a stubbed-true interactor), and carries a NEGATIVE case (an enabled but out-of-range NPC alongside a disabled in-range one, so the reject is specifically `OUT_OF_RANGE`). Its only skip is a missing `ZM_MenuRoot` singleton, i.e. an unbaked FrontEnd -- an environment fault, not a regression.
- **Other review fixes applied:** the reset unit's target/raise-count clears were vacuous (the fixture populated by ticking with no edge, leaving both already AT their cleared values), so the automated test now resets after a REAL raise and asserts the clear -- which is what the between-tests hook actually depends on, since a leaked raise count would let a later batched test's `GetRaiseCount() == 1` pass on a stale raise. `Interactable_OnStartDropsAnImpossibleRow` was misnamed for a branch it never exercised; since `ZM_NPC_NONE == ZM_NPC_COUNT`, the guard actually fires for any UNCONFIGURED row, so it is now split into `Interactable_OnStartClearsAnUnconfiguredRow` (the reachable, valuable case: an unconfigured NPC must not absorb the interact press) and `Interactable_OnStartKeepsAValidRow`. A `uZM_MAX_INTERACT_PROBES == 64u` assertion was removed as a pure change-detector -- it restated the constant back to itself and failed on any edit, right or wrong.
- **Tests that lock it:** **+20 pure units** in `Tests/ZM_Tests_Interactable.cpp` + the headless `ZM_NpcDispatch_Test`. `zm-tests.yml` **2291 -> 2311** (OBSERVED); windowed suite **30 -> 31**; engine default (**1097**) UNCHANGED.
- **Gate (dev machine, all green):** `Build\regen.ps1` (4 new .cpp); build clean (0 errors, 0 warnings); boot units **2311 ran / 2310 passed / 0 failed / 1 skipped**; headless **31/0 with `ZM_NpcDispatch_Test` PASSED not skipped**; full WINDOWED **31/0**.

---

## 2026-07-20 -- ZM-D-126 -- S6 item 3 (SC3): the NPC content is a compiled const table, so a mis-authored NPC is a BOOT FAILURE

- **Decision:** the four Dawnmere NPCs are authored as rows in a new `Source/Data/ZM_NpcData.{h,cpp}` (`ZM_NPC_ID`, `ZM_NPC_ROLE`, `ZM_NpcData`, `ZM_GetNpcData` / `ZM_GetNpcCount`), following the ZM-D-009 compiled-const-table idiom already used by `ZM_ItemData` / `ZM_HumanData` / `ZM_WorldSpec`: an append-only enum with the `ZM_NPC_NONE = ZM_NPC_COUNT` sentinel, an anonymous-namespace `const` array whose ROW INDEX EQUALS ITS ID, per-row static line/stock arrays referenced by pointer + count (the `ZM_ARRLEN` bracket, so every count is `sizeof`-derived and reading past an array is structurally impossible), and bounds-asserted free-function accessors. Nothing allocates, touches the ECS, or knows the UI exists. Still zero runtime behaviour change -- nothing instantiates the table; SC4 adds the component and SC5 authors the NPCs into Dawnmere.
- **The four rows:** Villager (TALKER, `ZM_HUMAN_TOWN_VILLAGER`), Trade Post Clerk (SHOPKEEP, `ZM_HUMAN_TOWN_SHOPKEEP`, stocking the six verified staples), Caretaker (CARETAKER, `ZM_HUMAN_TOWN_CARETAKER`), Wanderer (TALKER, `ZM_HUMAN_TOWN_ELDER`, `m_bWanders = true` -- SC8 gives it the patrol). `ZM_NPC_ROLE`'s three arms map **1:1 onto the raise seams `ZM_UI_MenuStack` already ships** (`TryPushDialogue` / `TryOpenShop` / `TryOpenCareCenterPrompt`), so SC4's `Interact()` is one switch adding no UI. **"Trade Post" everywhere, never "Mart"** (`ZM_BUILDING_TRADE_POST` already exists), so S9 needs no rename.
- **Care Center prose is NOT duplicated into a row.** The caretaker carries greeting lines only; the question, the two answer labels and the post-heal line stay in `Source/CareCenter/`, which already owns and unit-tests them.
- **Both row caps are pinned to the UI limits they exist to respect, at COMPILE time.** `uZM_NPC_MAX_LINES` is *defined as* `ZM_UI_DialogueBox::uMAX_QUEUED_LINES` and `uZM_NPC_MAX_STOCK` is `static_assert`ed `<= ZM_UI_Shop::uMAX_INVENTORY`. This matters because **both UI guards are ALL-OR-NOTHING** -- `QueueLines` and `ZM_UI_Shop::Open` each reject the WHOLE list when it is oversized -- so a drifted cap does not truncate: it makes the NPC silently MUTE or its shop refuse to open, while a table test still passes. Per-row `static_assert`s additionally catch an over-long line array at BUILD time rather than at boot, since the data is entirely compile-time.
- **The Data -> UI header edge is deliberate and bounded.** `ZM_NpcData.h` includes `ZM_UI_DialogueBox.h` -- the only `Source/Data -> Source/UI` include in the game, inverting the usual direction -- purely so the line cap can be DERIVED rather than re-spelled. That is the one sanctioned edge: **any further UI dependency belongs in `ZM_NpcData.cpp` as a `static_assert`, never in the header** (which is exactly where the shop-cap pin was put).
- **Review (3 adversarial lenses: data-correctness+idiom, test-vacuity, scope+buildability): NO BLOCKERS.** All three independently flagged the same defect -- `uZM_NPC_MAX_LINES` was **dead surface**, since the unit asserted against the UI constant directly, which also left the Data->UI include with no consumer. Fixed by pointing the unit at the named cap (still a derivation; the literal 8 is spelled nowhere). Also applied: the stock cap pinned to `ZM_UI_Shop::uMAX_INVENTORY` (it had re-spelled `16u`, the exact drift the line cap was derived to avoid); `Npc_EveryTalkerHasAtLeastOneLine` **de-guarded** into `Npc_EveryRowHasAtLeastOneLine`, because a role-guarded walk passes vacuously if no row is a TALKER *and* a zero-line CARETAKER/SHOPKEEP is equally broken (`QueueLines` rejects `uCount == 0` for every role); a non-zero-total guard added to the two remaining rows-x-stock walks; and the display-name unit split into a null/empty pass followed by a pairwise pass, because the assert macros record without aborting, so the interleaved form would `strcmp` a not-yet-null-checked name -- turning a named failure into a hard UB crash during units-at-boot.
- **Tests that lock it:** **+15 pure units** in `Tests/ZM_Tests_NpcData.cpp` (category `ZM_Data`, matching the `ZM_Tests_Abilities` idiom), catching a mute NPC, an empty shop, a row whose id no longer matches its index, a role with no content behind it, a second wanderer, and -- the load-bearing one -- **a clerk stocking an item whose `m_uBuyPrice` is 0**, which `ZM_ShopBuy` refuses as `ZM_SHOP_ERR_NOT_PURCHASABLE` and which for a KEY item is the ZM-D-120 free-Badge-Case hazard. Every stocked item was independently re-verified against `ZM_ItemData.cpp` before authoring: CATCHORB 200, the other five 100. `zm-tests.yml` **2276 -> 2291** (OBSERVED); engine default (**1097**) UNCHANGED.
- **Gate (dev machine, all green):** `Build\regen.ps1` (three new files); build clean (0 errors, 0 warnings -- and the caps' `static_assert`s compiling is itself the proof they agree); boot unit gate **2291 ran / 2290 passed / 0 failed / 1 skipped**; headless **30/0**; full WINDOWED suite **30/0**.
- **Process note:** every fact this sub-commit was specified against (six item ids and their prices, four human ids, the dialogue queue cap, the shop price accessor, the TradePost building row) was verified by the orchestrator directly BEFORE authoring, rather than taken from the decomposition dossier -- which had already been caught fabricating twice (see ZM-D-123). All of it checked out; the fabrications were confined to the dossier's *argumentative* sections, not its *descriptive* ones.

---

## 2026-07-20 -- ZM-D-125 -- S6 item 3 (SC2): the candidate picker is ONE walk with a high-water near-miss mark, XZ-only range, and a +Z-rotation facing that must never become `eulerAngles().y`

- **Decision:** land the pure candidate picker with **zero runtime behaviour change** -- nothing calls it yet, and no file or directory is added, so **no `regen.ps1` is owed** (unlike SC1). `ZM_PickInteractTarget(paxProbes, uCount, xOrigin, xTuning, uBestIndexOut)` plus `ZM_ForwardFromRotation(xRotation)` join `ZM_ShouldInteract` in `Source/Interaction/ZM_InteractionLogic.{h,cpp}`, over three by-value structs (`ZM_InteractProbe` = position + a per-NPC reach radius + enabled; `ZM_InteractOrigin` = position + a forward that need be neither normalised nor flat; `ZM_InteractTuning` = the three thresholds, defaulted from the SC1 constants so a unit can move a boundary without touching shipped tuning). **The SC1 enum is UNCHANGED** -- SC2 only starts PRODUCING the five reasons it already reserved.
- **Range is XZ-ONLY; height is a SEPARATE, absolute band.** A sunk or floating NPC (terrain float, a doorway lip) must stay reachable, so Y is dropped from the distance entirely and policed independently by `fabs(dY) <= m_fMaxVertical` -- which is what actually stops talking through a floor. Distance, band and cone are all **INCLUSIVE** at the boundary, each pinned by an accept/reject pair.
- **Reject reporting is MOST-SPECIFIC-LAST, via a single high-water mark, not a cascade of passes.** The reason returned is how far the BEST near-miss got (`NO_CANDIDATE` -> `OUT_OF_RANGE` -> `OUT_OF_VERTICAL_BAND` -> `NOT_FACING`), because the later walk-up windowed tests POLL the reason to know how close the player is: reporting the LAST probe walked would tell a test the player is far away while they stand right underneath the target. A **disabled probe raises no stage at all**, so a scene of parked NPCs reports the honest `NO_CANDIDATE` rather than `OUT_OF_RANGE`. Being a max rather than a last-write, it is also order-independent.
- **Determinism:** smallest XZ distance wins, ties break to the LOWEST index (a strict `<` displaces the incumbent), so the winner does not depend on array order while remaining stable under a tie -- both pinned, the second by REVERSING the array and asserting the same probe by IDENTITY.
- **`uBestIndexOut` is `uCount` -- an unreachable index -- on EVERY reject**, written before any early-out, so a caller that ignores the return value cannot silently address probe 0. One unit walks all six reject flavours to pin it; the fixture seeds the out-param with a poison value so "never written" is distinguishable from "wrote the sentinel".
- **`ZM_ForwardFromRotation` rotates +Z and must NEVER be rewritten as `glm::eulerAngles(quat).y`** -- that decomposition collapses past 90 degrees off +Z and already cost this repo a full debugging cycle in RenderTest's tennis AI. A unit asserts the FULL vector for the 180-degree case, which is exactly where the buggy form fails. Straight-up / straight-down facings flatten to an EXACT zero (never a NaN normalise) and the picker turns that into `DEGENERATE_ORIGIN`, checked BEFORE the probe walk. One file-local `ZM_FlattenXZ` serves both the helper and the picker, so the two can never disagree about what "facing" means.
- **A probe COINCIDENT with the player skips the cone test and counts as FACED** (you are standing on it) and, at distance zero, wins outright -- the alternative is a 0/0 dot product, and **a NaN compares false against every threshold**, so it would silently slip through the `<` guard and accept whatever probe came first.
- **Review (3 adversarial lenses: correctness+maths, test-vacuity, scope+buildability): NO BLOCKERS, 3 SHOULD-FIX, all applied.** All three were vacuity finds against an otherwise strong suite. (1) **`Forward_StraightUpFlattensToZero` could not fail for its stated reason:** the framework's assertions fail only when a difference EXCEEDS the epsilon, and every comparison against NaN is false -- so removing the degenerate early-out (yielding `(NaN, 0, NaN)`) would have sailed straight through the one unit guarding it. It now also asserts `length < epsilon` and exact componentwise zero, both of which a NaN fails. (2) **The header's "forward need NOT be normalised or XZ-flat" contract was pinned by nothing** -- every fixture looked along an already-flat, already-unit +Z, so the flatten/normalise was only ever exercised as the IDENTITY and deleting it would have passed all 27 units. In the live game the forward comes off a transform and carries pitch: with `(0, 8, 4)` a raw dot is ~4x too large and the cone is effectively OFF, making an NPC 80 degrees off to the side talkable. A new unit drives a pitched, 4x-over-long forward across the cone boundary in both directions. (3) **The documented negative-radius guard had zero coverage, and deleting it INVERTS the behaviour** rather than merely losing it: reach becomes -7.5, and the squared comparison against 56.25 puts the probe in range out to 7.5 m. A new unit pins it. Two NITs were also applied: a test whose name claimed a "Faced" probe its fixture never contained, and header wording that overstated the coincident rule as unconditional.
- **The facing-boundary units deliberately use a 0.8 cone, not the shipped 0.35.** The probe sits at offset `(0.75, 0, 1.0)` -- a quarter-scaled 3-4-5 triangle whose XZ length is exactly 1.25 and whose dot is exactly `1.0/1.25` -- so every term is dyadic and the picker's arithmetic is bit-identical to the constant; an inclusive-boundary unit built on an irrational offset would flake on a 1-ulp disagreement. A guard assertion pins `fZM_INTERACT_MIN_FACING_DOT < 0.8f`, so widening the shipped cone past the fixture breaks a test instead of quietly invalidating it. The distance and band boundaries ARE derived from the tuning struct, so they move with the shipped constants.
- **Tests that lock it:** **+29 pure units** appended to `Tests/ZM_Tests_Interaction.cpp` (15 -> 44), every one naming a concrete winner index or a concrete reject code, with the fixture placing the player at the origin looking down +Z so each reads as a picture. `zm-tests.yml` **2247 -> 2276** (OBSERVED, and independently re-derived by the orchestrator from the unit count before the gate ran); engine default (**1097**) UNCHANGED -- SC2 touches no engine file, so no cross-game regression is owed.
- **Gate (dev machine, all green):** no regen needed (no new file/directory); build clean (0 errors, 0 warnings); boot unit gate **2276 ran / 2275 passed / 0 failed / 1 skipped**; headless **30/0**; full WINDOWED suite **30/0**.
- **Reversibility:** trivial. Nothing calls the new surface; reverting three file edits restores the SC1 state exactly.

---

## 2026-07-20 -- ZM-D-124 -- S6 item 3 (SC1): interact is `E`, the gate is a pure REASON-returning predicate, and the scene test is shared with the pause menu

- **Decision:** land the item-3 foundation with **zero runtime behaviour change** -- nothing calls the new surface yet. Three parts. (1) **`ZENITH_KEY_E` is the interact key**, verified free across the whole Zenithmon tree (this game claims only WASD, the four arrows, both Shifts, Enter/Space, Escape/Backspace, and M/Tab). (2) **`ZM_ShouldInteract`** (new pure `Source/Interaction/ZM_InteractionLogic.{h,cpp}`) deliberately mirrors `ZM_UI_MenuStack::ShouldOpenMenu`'s doctrine -- all bools in, every impure lookup left at the thin live call site -- but returns a **`ZM_INTERACT_REJECT` reason** rather than a bare bool, because the later walk-up windowed tests poll on the reason to know when to press. (3) two `ZM_UI_MenuStack` seams: a new `static IsMenuOpen()` and `IsActiveSceneOverworld()` promoted private -> **public static**.
- **`ZM_InputActions.h` is now the single source of every binding.** Named `inline constexpr` key sets (confirm / cancel / menu / run / the four movement directions + a derived flat movement set) that **the live readers AND the collision units both walk** -- so a future rebind that aliased the interact key fails a unit instead of silently double-firing (interact would also step). The readers were refactored to consume those arrays rather than re-listing literals; `IsKeyDown` / `WasKeyPressedThisFrame` are side-effect-free on both the real and the simulator path, so the OR-over-a-set is the same predicate and the short-circuit reorder is unobservable. A **`static_assert`** ties the flat movement set's length to the four per-direction counts: shrinking already failed (constexpr OOB index), and this makes GROWING -- the realistic edit, which would silently narrow the collision unit's walk -- a build break too.
- **The reject enum is APPEND-ONLY and its blocker precedence is FIXED and unit-pinned:** `NO_INPUT_EDGE` > `MENU_OPEN` > `NOT_OVERWORLD` > `WARP_IN_PROGRESS` > `BATTLE_TRANSITION` > `PLAYER_FROZEN` > `OK`. Later windowed tests assert on these VALUES, so a reorder would silently change what those assertions mean. `ZM_InteractRejectName` is a `switch` with a `default`, not a lookup table, so it is total by construction with no possible out-of-range index.
- **`ReadInteractPressed` is NON-consuming**, exactly like its confirm/cancel siblings. Several consumers can therefore see the same edge in one frame, so mutual exclusion is expressed **explicitly** by the gate's `bMenuOpen` blocker and is never implied by consumption. This is the documented answer to the SC2-era forward-note (Shortfalls 1.6) that a raising confirm edge could be re-read by the dialogue box in the same frame: a distinct key plus an explicit lock, rather than edge consumption.
- **Interiors are deliberately talkable.** `IsOverworldSceneKind` excludes only `FRONTEND` and `BATTLE`, so `PLAYERHOME`/`PROFLAB` (`INTERIOR`) and `GYM1` (`GYM`) all pass. No S6 test depends on it; S9's interior NPCs will.
- **Review (4 adversarial lenses: correctness+conventions, test-vacuity, scope+behaviour-neutrality, buildability): NO BLOCKERS**, 5 SHOULD-FIX, all applied by the orchestrator before the build. The two that mattered were both **vacuity**: (a) `RejectName_IsTotalAndDistinct` could not catch its own stated regression -- an appended enumerator with no switch arm falls through to `"UNKNOWN"`, which is non-null, non-empty and distinct from every real name, so the unit went green while later windowed failures would report `UNKNOWN` and lie; fixed by hoisting the out-of-range sentinel and asserting no IN-RANGE enumerator returns it. (b) `MenuStack_IsMenuOpenIsFalseWithoutSingleton` asserted only its postcondition, and `IsMenuOpen()` returns false on BOTH the unresolved and the resolved-but-closed branch -- so it would have passed while exercising the wrong path; it now pins the precondition (`TryGetUniqueSingletonEntityID` is false) first. Also applied: the movement `static_assert` above, a `ZM_` prefix + `inline` on the three tuning constants (`fZM_INTERACT_MAX_DISTANCE` / `_MIN_FACING_DOT` / `_MAX_VERTICAL` -- the assert macros bind by const ref, so SC2's units odr-use them), and a **fifth** collision unit for the run keys, which are a live binding set (`ReadRunHeld` walks them every overworld frame) that the four spec'd units did not cover.
- **Tests that lock it:** **+16 pure units** -- 15 in the new `Tests/ZM_Tests_Interaction.cpp` (one per blocker; a precedence unit covering all six adjacent pairs; a **64-combination totality walk** that recomputes the expected reason longhand and never by calling the function under test, additionally pinning that exactly ONE combination returns OK; the reject-name totality/distinctness unit; five key-collision units) + 1 in `ZM_Tests_MenuStack.cpp` (22 -> 23). `zm-tests.yml` **2231 -> 2247** (OBSERVED, not predicted); engine default (**1097**) UNCHANGED -- SC1 touches no engine file, so no cross-game regression is owed.
- **Gate (dev machine, all green):** `Build\regen.ps1` (MANDATORY -- `Source/Interaction/` is a new directory and the generated vcxproj is gitignored, so without it the build link-fails on `ZM_ShouldInteract` from the new test TU); build clean (0 errors, 0 warnings); boot unit gate **2247 ran / 2246 passed / 0 failed / 1 skipped**; headless **30/0**; full WINDOWED suite **30/0** with **`ZM_S6UIGate_Test` PASS** -- the deliberate regression detector for the `ZM_UI_MenuStack` edits, checked one commit after the edit rather than five.
- **Reversibility:** trivial. Nothing calls the new surface; deleting the three new files and reverting four edits restores master exactly.

---

## 2026-07-20 -- ZM-D-123 -- S6 item 3 DECOMPOSED: 9 sub-commits, and TWO scope rulings that deviate from the Roadmap wording (behaviour graphs and navmesh both deferred to S7)

- **Decision:** decompose the last unchecked S6 line -- *"`ZM_Interactable` + NPC graphs via `ZM_GraphAuthoring`; `ZM_NpcWalker` (navmesh wanderers)"* -- into **9 sub-commits (SC1-SC9)**, each independently shippable and gated. Durable plan on disk at `Build/artifacts/zm_s6_npc_plan/plan.md` (git-ignored). The item's payload is making the ALREADY-SHIPPED dialogue / shop / Care Center screens reachable by **walking up to an NPC and pressing E**, instead of only through the static `TryPushDialogue` / `TryOpenShop` / `TryOpenCareCenterPrompt` seams. Design was chosen by a 3-way independent design bake-off judged by 4 adversarial lenses (landability / gate-satisfaction / future-cost / testability); the **testability-first** design won 33 to 25 to 24 and its architecture is derived backwards from the windowed gate.
- **RULING A -- behaviour graphs are NOT in S6. Plain C++ dispatch. (Best-guess; the loop PROCEEDS.)** `ZM_GraphAuthoring` is not written; `ZM_Interactable::Interact()` will be a 3-arm switch over the NPC's role. **Evidence:** Zenithmon contains **zero lines of graph code** (a tree-wide grep for `bgraph|GraphBuild|GraphComponent|GraphBuilder|FireCustomEvent` hits only Docs prose); the payload is `role -> one of three already-shipped statics`, i.e. ~300-700 lines of node classes + a registration hook to express a closed 3-way enum, with no designer to benefit and no branching content; a `.bgraph` adds a third **silent-skip axis** (`Assets/` is git-ignored, so a cold checkout loads a `.zscen` whose graph slot resolves *unresolved* -- no crash, no log, a mute NPC); and ZM-D-010 already scopes graphs to *"glue only (menu flow, NPC scripted events, cutscene beats)"*, which a fixed 3-way dispatch is not. **The seam is kept latent:** `Interact()` funnels through ONE dispatch function so S7 can add a `FireCustomEvent("Interact")` branch (~12 lines) without touching the proximity plumbing or a single windowed test. **Cost if wrong:** S7 pays the first-graph tax plus converting a 3-arm switch -- roughly one additive sub-commit, with every existing test unchanged because they assert screen state, not how the raise happened.
  - **A REAL but SECONDARY risk for S7 to weigh, verified 2026-07-20 and stated at its true strength (it is NOT load-bearing for this ruling).** There is a known, **unfixed** intermittent heap corruption in the graph node-registry round-trip path: `GraphComponent::RegistryWideNodeRoundTrip` is `ZENITH_SKIP`-quarantined at `Zenith/EntityComponent/Components/Zenith_GraphComponent.Tests.inl:2978-2990` as `task_726cc81d`. Commit `80f72fba` root-caused it as an overrun while building/serialising the ~128-node definition, surfacing as a debug-heap break freeing an unrelated `SceneNameEntry` during the NEXT test's scene reset -- which in headless CI raised a modal CRT dialog that hung the units-at-boot gate until the watchdog. It is explicitly a **heap-LAYOUT-dependent heisenbug**: it survived 10 clean `_CrtCheckMemory`-instrumented runs and needs full page-heap (gflags, elevation) to pinpoint. It is INSTITUTIONALLY known, not a one-off comment: `Docs/BuildSystem.md:183` records that `Tools/run_unit_gate.ps1` (CI) and `Tools/test_scaffold.ps1` (scaffold gate) **both** tolerate "exactly one known layout-sensitive flake (`GraphComponent::RegistryWideNodeRoundTrip`)" -- which is also why every Zenithmon gate in this session reads `1 skipped`. **The layout sensitivity is documented as having really bitten a game:** `Games/RenderTest/CLAUDE.md:78` records that rendertest.exe runs units-at-boot *deliberately only* (`--skip-unit-tests` everywhere else) because "the task_726cc81d layout corruption has tripped here on some layouts". Since Zenithmon's entire CI story IS a count-exact units-at-boot gate, registering new node types -- which would change heap layout -- is not a free action. **What is NOT documented anywhere, and must not be repeated as fact:** that "registering new node types" is a *named* escalation trigger, and an alleged `Games/Exploration` incident where six new nodes turned a survivable assert into a hard AV. Both appeared in a research agent's report during the S6 item-3 decomposition; a direct search of the quarantine comment, commit `80f72fba`, and the full git history found **no trace of either**, and `Games/Exploration` was deleted in `cfc4e7c1`. The honest position is: the bug is real, unfixed and layout-sensitive, and adding node types plausibly perturbs layout -- that is a reasonable inference, not a documented mechanism.
- **RULING B -- navmesh is NOT in S6. Authored waypoints. (Best-guess; the loop PROCEEDS.)** `ZM_NpcWalker` ships as a deterministic 2-waypoint patrol with a fixed dwell, driven by `SetLinearVelocity` -- no navmesh, no A*, no RNG.

  **CORRECTED 2026-07-20 (orchestrator re-verified the engine himself after this entry was first written -- the original justification was WRONG and is retained nowhere; read this version).** The first draft of this ruling claimed the navmesh system "cannot produce ground from terrain" and called it a missing engine feature. **That is false, and believing it would be actively harmful at S7.** The accurate position:
  - **`Zenith_NavMeshGenerator::GenerateFromGeometry(axVertices, axIndices, xConfig)` accepts ARBITRARY triangle geometry** and runs a full voxelise -> slope-filter -> watershed -> contour -> polygon pipeline. Its config carries `m_fMaxSlope = 45.0f` and there is an `IsWalkableSlope(xNormal, fMaxSlopeDeg)` (`Zenith_NavMeshGenerator.h:15,216`). **A walkable-slope filter exists precisely BECAUSE the generator is designed for non-flat ground. The generator is terrain-capable.**
  - What is NOT terrain-capable is **`Zenith_AINavGeometry::GenerateFromScene`, the convenience SCENE-SCRAPER**. It walks static colliders and models every one as a **box OBB** (`ComputeBoxDimensionsAndOffset` -> 8 corners -> 6 faces, `Zenith_AINavGeometry.cpp:117-165`); it never reads a triangle mesh or a heightfield. So a `COLLISION_VOLUME_TYPE_TERRAIN` collider comes out as a BOX rather than as its surface. That is a limitation of **that wrapper**, not of navmesh-on-terrain. DevilsPlayground uses it happily (`DP_AI.cpp:198`) because its levels are built from box colliders -- exactly the case it is for.
  - The 1024 clamp is real but its consequence was also mis-stated: it is `min(ceil(size / cellSize), 1024)` (`Zenith_NavMeshGenerator.cpp:149-153`), which **TRUNCATES the grid** -- at the default 0.3 m cell it covers only ~307 m of Dawnmere's 1024 m domain. It does not "force 1 m cells", and it constrains any large world, not terrain specifically.

  **The real (and sufficient) reason to defer:** using the navmesh on Dawnmere means feeding **terrain triangles** to `GenerateFromGeometry`, which requires a terrain-geometry source that is not currently identified (no height accessor was found on `Source/World/ZM_TerrainAuthoring.h`, though that search was not exhaustive), plus a decision about grid coverage at this domain size. That is unscoped work of unknown size, and **the S6 gate contains no clause that NPC motion helps pass** -- a stationary-plus-patrolling cast satisfies every gate criterion. So this is a SCOPE deferral, not a capability limit, and it costs no engine change and no cross-game regression. `.znavmesh` `LoadFromFile`/`SaveToFile` do have **zero in-repo callers**, so persistence would also be first-of-its-kind work.
  **Cost if wrong:** `ZM_StepWalker` (~120 lines + ~15 units) is superseded and deleted at S7; the `ZM_Interactable` interface, the NPC table, all four walk-up tests and the consolidated gate are unaffected, because the walker sits behind `SetWanderEnabled(bool)` and a per-frame velocity write. **S7 should evaluate `GenerateFromGeometry` on real terrain triangles on its merits -- do NOT inherit a belief that the engine cannot do it.**
- **Why these are not user-blocking scope changes:** `Scope.md` Section 4 governs *what ships* -- "NPC wanderers exist and are interactable" -- which is unchanged. Both rulings change only the MECHANISM, both are logged here with their engine evidence, and both are additive to reverse. Under the unattended-loop policy these are best-guess rulings with a stated cost-if-wrong, so the loop proceeds; they are also mirrored into `Questions.md` for user override. `Roadmap.md` and `AssetManifest.md` wording will be amended at SC9 so the plan and the code stop disagreeing.
- **Other rulings carried by the plan:** interaction is a forward **CONE**, not a raycast (a raycast needs `HasActiveSimulation()`, false headless, which would push the decision surface out of unit reach -- S7's trainer occlusion ray enters as a probe filter in the GLUE layer, leaving the pure picker untouched); **exactly ONE ECS order is consumed (113)**, with the walker a by-value member of `ZM_Interactable` and the interaction runtime a by-value member of `ZM_PlayerController` (which is already on every Player in every scene, so there is no per-scene `AddStep_AddComponent` to forget); **four NPC rows** (villager, Trade Post clerk, town Caretaker, wanderer) as a minimal proving cast, since populated towns are S9/S10; **"Trade Post"**, not "Mart", in every data/entity/asset name; and **no RNG anywhere in the walker** (TestPlan C8), since a deterministic loop is what makes the SC8 rendezvous provable.
- **The whole item is GAME-ONLY.** Every file it touches is under `Games/Zenithmon/` plus `.github/workflows/zm-tests.yml`. The engine baseline **1097 must be unchanged at every sub-commit** -- if it moves, an engine file was edited by accident and the rule is **revert the SC, do not debug it**. No cross-game regression is owed at any point.
- **Note on ids:** the plan document's internal risk table sketches ids `ZM-D-123..128`; the actual assignment is this entry (**123** = decomposition + both scope rulings) then **124** = SC1, so the plan's later numbers shift by one. The DecisionLog is authoritative.

---

## 2026-07-20 -- ZM-D-122 -- S6 item 2 (SC9): the consolidated gate is about CONTINUITY, not coverage -- item 2 COMPLETE

- **Decision:** close the S6 item-2 gate with ONE new windowed test, `ZM_S6UIGate_Test`, that adds **no per-screen coverage at all**. The seven SC1-SC8 windowed tests each open ONE screen in its OWN session with its own scene load and teardown, so between them they prove every screen works ALONE and **nothing proves they work TOGETHER**. SC9 therefore loads Dawnmere exactly once and never reloads until teardown, walking the whole S6 surface end to end so it catches the class the per-screen tests structurally CANNOT: **state bleeding between screens** -- a leaked stack push, a focus stranded on a now-hidden element, a cursor/page surviving into a later visit, a player left frozen after a screen closed, an economy moved by merely LOOKING at a bag.
- **"Via focus navigation" is taken literally.** Every ROOT entry is reached by walking the canvas focus with real `ZENITH_KEY_UP`/`ZENITH_KEY_DOWN` edges and confirming; **`SetFocusedElement` is never called anywhere in the test.** Because `PresentTopScreen`'s ROOT arm re-parks the focus on the first entry whenever ROOT regains it holding a non-ROOT element, every visit would otherwise walk DOWN only -- so two dedicated NAV-PROBE phases walk down to the last entry (`Exit`) and back up first, and a dead `NavigateUp` now fails the gate instead of hiding behind a monotonic walk. Every per-visit flag defaults FALSE, so a phase that silently never runs FAILS rather than passing on a stale global.
- **The review made the bleed assertions actually observable, which was the whole point.** As first authored, two of them could not fail: the "focus stranded on a hidden element" check was unreachable (the ROOT arm re-parks the focus every frame, so the stranded state cannot be sampled), and cursor/page bleed had nothing to compare against because each screen was visited once. Fixed by (a) replacing the unreachable check with ones that CAN fail -- the `m_iCursor` mirror, which the ROOT arm does not re-park from a stale sub-screen row, plus "the ROOT panel is visible while all three sub-screen panels are hidden" -- and (b) **adding a FOURTH visit: Party, Bag, Dex, then Bag AGAIN**, sampling the bag's page/pocket/cursor on both visits and asserting they match. `PopTopScreen` resets nothing on a pop to a non-empty stack, so that revisit is the only thing in the suite that can see bag state leak across a screen change.
- **Three economy numbers, not one.** The browse-must-not-mutate check pins money AND the summed per-item counts AND the stack count against a baseline captured BEFORE the first screen opened: money alone misses a bag mutation, the stack count alone misses a count changed inside an existing stack, and the summed counts alone miss an added empty entry.
- **Two follow-ups Shortfalls had recorded against SC8 are closed here.** (1) **The heal is no longer silent:** on YES + `HEAL_PARTY` where `ZM_ApplyCareCenterHeal` returns true, `ApplyDialogueChoice` queues `ZM_CareCenterHealedLine()` onto the already-reset, now-unarmed box and returns WITHOUT popping, so the ordinary read-to-the-end `CLOSED` path takes it down on the next confirm; every other path (NO, already-healthy, any non-HEAL action) pops exactly as before. This was an S8 manual-playthrough risk -- a button that appeared to do nothing. `ZM_CareCenterHeal_Test` was extended (not weakened) with the intermediate healed-line step. (2) **SC8's deliberate "the panel + question stay SHOWN while a choice is awaiting" behaviour is now asserted** (it guards the ZM-D-112 bleed-through class; a regression to `bShown = IsActive()` previously passed every test).
- **Tests that lock it:** the windowed **`ZM_S6UIGate_Test`** (158 frames, `skipped: false`, ~4.9 s -- open-every-menu via focus nav x4 visits, bleed assertions, talk, buy with an EXACT table-price delta, heal, final clean-session check) + **+2 pure units** in `ZM_Tests_CareCenter.cpp` pinning that the healed line is distinct from the prompt and that `ZM_ApplyCareCenterHeal`'s return is what gates the extra line. `zm-tests.yml` **2229 -> 2231**; engine default (**1097**) UNCHANGED.
- **Gate (dev machine, all green):** build clean (0 errors, 0 warnings); boot unit gate **2231 ran / 2230 passed / 0 failed / 1 skipped**; headless **30/0**; full WINDOWED suite **30/0**.
- **S6 item 2 is COMPLETE (SC1-SC9).** Across nine sub-commits the six UI screens consumed exactly **ONE** ECS order (112) -- every screen is a by-value non-ECS presenter on `ZM_UI_MenuStack` -- and zm boot units went **2034 -> 2231**. **S6 ITSELF IS NOT COMPLETE:** item 3 (`ZM_Interactable` + NPC graphs via `ZM_GraphAuthoring` + `ZM_NpcWalker`) remains unchecked, and until it lands the dialogue / shop / Care Center screens are reachable only through their static raise seams, not by walking up to an NPC. The Roadmap gate line records exactly that.

---

## 2026-07-19 -- ZM-D-121 -- S6 item 2 (SC8): the Care Center heal is a yes/no CHOICE on the dialogue box

- **Decision (orchestrator's ruling, made before authoring):** Shortfalls recorded `ZM_UI_DialogueBox` as lacking BOTH a completion signal and a yes/no variant, and flagged both as blocking the Care Center. **A completion signal alone cannot express WHICH answer the player gave, so it would not have unblocked the heal** -- SC8 therefore adds the CHOICE, which subsumes completion for this consumer. SC8 adds **no new screen and no ECS order**: a prompt is simply the DIALOGUE screen with a choice armed.
- **Strictly additive.** `ZM_DIALOGUE_ADVANCE_AWAITING_CHOICE` is APPENDED after `CLOSED`; with nothing armed, `Confirm()` still returns `CLOSED` and fully `Reset()`s exactly as SC2 shipped. That regression is pinned by a dedicated unit AND by `ZM_DialogueTalk_Test` passing untouched.
- **The answer is given BY THE FOCUSED ELEMENT'S NAME** (`ResolveChoice` over `Menu_DialogueYes` / `Menu_DialogueNo`) -- never a `SetOnClick` userdata, which dangles on ECS pool relocation -- and the buttons carry **no `SetNavigation` links** (the SC6 rule: a bake-time link into an element that gets hidden swallows the press with no fallback). **Cancel resolves NO** (`CancelChoice`): the LINES stay modal so they cannot be skipped, but a prompt the player can neither answer nor escape would be a dead end.
- **The box deliberately keeps the panel + question SHOWN while awaiting** an answer. The advance that opens the wait leaves the box `IsActive() == false`, so the SC2 predicate would have hidden the panel and blanked the text, leaving two buttons floating over the world -- exactly the ZM-D-112 bleed-through failure the S5 gate exists to catch.
- **What a YES DOES lives on the host** as an explicit `ZM_DIALOGUE_ACTION` (`HEAL_PARTY` -> `ZM_ApplyCareCenterHeal` against the live `ZM_GameState`), because a by-value screen cannot call back into its owner and a function pointer would be the wrong shape for a single consumer. It is consumed on EVERY answer and cleared on close / boot / deserialize, so a NO or a force-close can never leave a heal armed for an unrelated later conversation.
- **THE BLOCKER, found by four reviewers independently and fixed by the orchestrator** (the workflow's fix agent died on a session limit, so these were applied by hand): the windowed test read the resolved answer back off the box **after** the prompt closed -- but a prompt raised over an empty stack pops to empty, which calls `CloseMenu()`, which `Reset()`s the box and clears the stored answer, **all synchronously inside one `OnUpdate`**, so no caller ever gets a frame in which to observe it. The box-level contract (the answer survives the *resolve*) was correct; the *stack's close path* destroyed it. Fixed by latching the answer on the HOST -- `ZM_UI_MenuStack::GetLastDialogueAnswer()`, assigned AFTER `PopTopScreen()` and cleared only on boot / deserialize, never in `CloseMenu` (it is the LAST answer, not live session state).
- **Also fixed from the same review:** `PushDialogueLines` gained the symmetric `IsChoiceArmed()` guard that `OpenCareCenterPrompt` already had -- without it, ordinary NPC lines queued while a prompt awaited its answer pushed the read cursor back below the line count, dropped the box out of `IsAwaitingChoice()` and stranded the armed choice with its buttons hidden: an **unanswerable prompt**. And a `Reset` unit was **vacuous** -- its fixture resolved the choice first, and `ResolveChoice` already performs a full `Reset()`, so four of its five assertions were true before the `Reset()` under test even ran; it is now split into an armed-state case that pins the pre-state and a resolved case that pins the one thing that genuinely survives (the answer).
- **Tests that lock it:** **+25 pure T0 units** (14 choice-mode in `ZM_Tests_DialogueBox.cpp`, 10 in the new `ZM_Tests_CareCenter.cpp`, +1 from the orchestrator's test split) + the windowed **`ZM_CareCenterHeal_Test`** (47 frames, `skipped: false`) covering BOTH branches: YES heals the live party to full, NO leaves it damaged. `zm-tests.yml` **2204 -> 2229**; engine default (**1097**) UNCHANGED.
- **Gate (dev machine, all green):** build clean (0 errors, 0 warnings); boot unit gate **2229 ran / 2228 passed / 0 failed / 1 skipped**; headless **29/0**; full WINDOWED suite **29/0**, all seven UI windowed tests verified `skipped: false`.
- **Known follow-ups, deliberately NOT in this commit** (recorded in Shortfalls 1.6): the heal is currently **silent** -- `ZM_CareCenterHealedLine()` exists and is unit-tested but has no consumer, so a YES heals and closes with no on-screen confirmation. That is an S8-gate risk (a manual playthrough will notice) and should be wired when S6 item 3 adds the Care Center NPC. Separately, the "panel stays shown while awaiting" behaviour above has no windowed assertion, so a regression to `bShown = IsActive()` would not be caught.

---

## 2026-07-19 -- ZM-D-120 -- S6 item 2 (SC7): the shop -- pure `ZM_ShopLogic` + `ZM_UI_Shop`, and `ZM_Bag::CanAdd`

- **Decision:** Ship SC7 -- buy/sell. Pure `ZM_ShopLogic` free functions over `(ZM_Bag&, u_int& money)` returning a `ZM_SHOP_RESULT`, plus a by-value `ZM_UI_Shop` presenter on `ZM_UI_MenuStack` (screen id **`ZM_MENU_SCREEN_SHOP`, APPENDED** so `DIALOGUE` keeps 5 and the SC2 literal pin still holds). No new ECS component or order (next-free stays **113**); one arm per dispatch switch, so the SC4 generalization has now held for four screens.
- **The load-bearing decision is the BUY ORDERING.** validate id/qty -> **reject `m_uBuyPrice == 0`** -> overflow-check -> afford-check -> **`CanAdd`** -> deduct -> `Add`. Every one of the three guards is protecting against a concrete failure:
  1. **`m_uBuyPrice == 0` is NOT_PURCHASABLE.** All six KEY items are priced 0/0, so a mart that stocked one would hand out a **free Badge Case** without this. (Worth recording: **five NON-key rows are also 0/0** -- `PRIMEORB`, `PPBOOST`, `RARESWEET`, `HEIRLOOMKNOT`, `STASISSTONE` -- so a naive "first table row with buy price 0" walk lands on `PRIMEORB`, not a key item. The tests therefore use TWO walkers and additionally assert every KEY row is 0 in both directions.)
  2. **`price * qty` is checked against `UINT_MAX / price` BEFORE multiplying**, so an absurd quantity cannot wrap into an affordable total.
  3. **Bag room is checked BEFORE the money moves.** `ZM_Bag::Add` is all-or-nothing and refuses when a stack would exceed `uZM_BAG_MAX_STACK_COUNT`, so the naive `SpendMoney` then `Add` sequence can take the money and fail to deliver -- the exact non-atomicity Shortfalls flagged when SC3 landed. **`ZM_Bag` gains a const, non-mutating `CanAdd`, and `ZM_Bag::Add` now calls it**, so the accept/reject rule exists in exactly ONE place and the two can never drift.
- **The sell mirror carries the credit-loss guard:** a purse at `uZM_MONEY_CAP` would silently swallow the payment while the items were already gone, so a credit that does not fit IN FULL is refused with nothing mutated (`MONEY_CAPPED`). Sell-side overflow returns the same result -- a credit that cannot be represented certainly cannot fit under the cap.
- **Shop-specific UI rulings.** The transaction fires from a dedicated **Confirm control**, not from a row -- rows only SELECT -- so a stray confirm on the list can never spend money. Consequently the selection must SURVIVE the focus walking off the list onto that control, which is a deliberate divergence from the bag's "cursor goes -1 on a nav button" rule and is pinned by the windowed test. Exit is resolved in the MenuStack arm (a by-value screen cannot pop itself). `SetInventory` deliberately **accepts** price-0 stock: rejecting it at configuration time would make the `NOT_PURCHASABLE` guard unreachable from the UI, and that guard is the thing standing between a mis-authored mart and a free key item. The shop is **not** reachable from the ROOT menu -- a mart is entered by talking to its clerk, so it is raised only by `OpenShop` / the static `TryOpenShop` seam (the `TryPushDialogue` shape), which S6 item 3's mart NPC will call.
- **It is the first menu screen that WRITES the live `ZM_GameState`,** so the bag and money are passed **by reference** -- a by-value copy would make purchases silently vanish, and only the windowed test would have caught it.
- **Review (4 pre-build lenses): NO BLOCKERS, 7 findings, all real, all fixed.** The two that mattered: (i) **the SELL branch of `Confirm` had ZERO coverage** -- both SELL-mode tests ran over an empty bag and short-circuited at the "nothing selected" guard, so `ZM_ShopSell` was never reached from the presenter at all; the replacement seeds the bag through the real `ZM_Bag::Add`, stocks a DIFFERENT item for BUY and seeds the purse at 0, so an inverted buy/sell ternary fails twice over. (ii) **the selection-survival ruling was documented but not pinned** -- `Verify` only checked the cursor was non-negative, so a cursor reset to 0 on a control would still have passed; the test now records the cursor on the last row it walked through and fails unless the walk genuinely left row 0 AND the cursor survived onto Confirm. Also fixed a real contract lie: `GetCursor()` was documented in both headers as the "selected list entry" but stores a PAGE-RELATIVE ROW, which only coincides on page 0 -- the docs are corrected and a new `GetSelectedEntryIndex(bag)` resolves the flat index, with `Confirm` now calling it rather than recomputing the expression, so the entry the screen reports and the entry the money is spent on cannot diverge.
- **Tests that lock it:** **+43 pure T0 units** (`Tests/ZM_Tests_Shop.cpp`, category `ZM_Shop`) -- every refusal path asserts money is **EXACTLY** unchanged, price-0 items are found by WALKING the table (no hard-coded ids), `FormatResult` totality is walked over the enum rather than spelled out, and `CanAdd` is proven to agree with `Add` on accept AND reject while provably not mutating -- plus the windowed **`ZM_ShopScreen_Test`** (23 frames, `skipped: false`), which walks the focus onto Confirm with real arrow edges and asserts the LIVE game state moved by exactly the table price. Game-only units -> `zm-tests.yml` **2161 -> 2204**; engine default (**1097**) UNCHANGED.
- **Gate (dev machine, all green):** build clean (0 errors, 0 warnings); boot unit gate **2204 ran / 2203 passed / 0 failed / 1 skipped**; headless **28/0**; full WINDOWED suite **28/0**, all six UI windowed tests verified `skipped: false`.
- **Reversibility:** the pure logic is additive and self-contained; `ZM_Bag::CanAdd` is a strict factoring of a rule `Add` already enforced. Remaining in item 2: SC8 (Care Center heal) and SC9 (the consolidated gate).

---

## 2026-07-19 -- ZM-D-119 -- S6 item 2 (SC6): `ZM_UI_Bag` -- the pocket-tabbed bag screen; and NO bake-time nav links on any runtime-liveness pool

- **Decision:** Ship SC6 -- the bag screen, the first surface to RENDER the SC3 model (`ZM_Bag` + `ZM_GameState::m_uMoney`). A by-value NON-ECS presenter on `ZM_UI_MenuStack` (the SC2/SC4/SC5 seam), PODs-only state (pocket / page / cursor). **No new ECS component or order** (next-free stays **113**), and exactly **one arm per dispatch switch** -- the SC4 generalization held for the third screen running.
  1. **Authored entirely at bake time -- deliberately NOT a grid.** Unlike SC5's dex, the bag is a 1-D LIST, so panel, header, eight rows and four nav buttons all come from `AddStep_CreateUI*` + `ZM_ConfigureMenuRoot`, with **zero runtime element construction**. A 1-D list gains nothing from a `Zenith_UIGridLayoutGroup`, and building one at runtime would re-introduce SC5's `AddElement`/`AddChild`/`ReparentElement` ownership hazard for no benefit. The header records this so it is not later "fixed" into a grid.
  2. **Nine pockets, so the pocket axis CYCLES and the page axis CLAMPS.** `ZM_ITEM_CATEGORY_COUNT` is too many for a tab row, so the pocket is changed with prev/next buttons and named in the header. Wrapping at both ends is deliberate -- clamping nine pockets would strand the player at either end of the row. Changing pocket **always resets the page to 0**, because the new pocket may hold fewer pages.
  3. **Empty pocket:** an `(empty)` notice is written into row 0, left VISIBLE but **non-focusable**, so it is readable in the list area while the nav can never select a non-item; the focus parks on the Next-Pocket button with `cursor == -1`, so the player is never stuck and the cursor never claims a row nothing is drawing.
- **THE BLOCKER THIS SC FOUND -- a general rule now, not a local fix.** The row pool was first wired with explicit bake-time `SetNavigation` up/down links (copying the ROOT/party idiom -- and the orchestrator's own brief asked for them). That is **WRONG for any pool whose liveness is runtime state**: `Zenith_UICanvas::NavigateDown` consults the explicit link FIRST and only falls back to the spatial `FindNearestFocusable` when that link is **null**. A non-null link whose target fails `IsVisible() && IsFocusable()` is dropped with **no fallback at all** -- the press is silently swallowed. Since `Present` hides + un-focuses every row past the pocket's stack count, a bake-time link points into a hidden row on every PARTIAL page, and the shipped default is exactly that (the starter BALL pocket holds one stack), so Down out of the list onto the nav band was inert on the default screen. **Fix: no `SetNavigation` on the bag rows at all -- the whole screen traverses on the spatial search, like the dex.** The rows share an x, so the spatial walk follows the live column and re-reads liveness every frame.
  - **The same defect was already shipped on the SC4 party slots and is FIXED FORWARD here.** It was harmless only because nothing focusable sits below the party slots yet -- SC7/SC8 (use-item-on-member) would have walked straight into it. Its links are removed with the reasoning recorded in place.
  - **ROOT keeps its links** and is the one legitimate case: all four ROOT entries are ALWAYS visible, so the links can never point at a hidden target.
  - **Secondary reason to distrust bake-time links:** `SetNavigation` is **not serialized** by `Zenith_UIElement::WriteToDataStream` (verified), so links exist only where `ZM_ConfigureMenuRoot` runs -- i.e. tools builds. A `_True` and a `_False` build would otherwise navigate by different code paths.
- **The windowed gate was also strengthened, not just the code.** The first draft PARKED the focus programmatically before confirming a nav button, which would have passed even with the navigation broken. It now WALKS with real `ZENITH_KEY_DOWN` edges and polls the focused element's name until it reaches the nav band (deadline-guarded, defaulting to a FAILING flag so a phase that never runs fails), and additionally cycles to a genuinely empty pocket to cover the `(empty)` row and its focus fallback.
- **Tests that lock it:** **+22 pure T0 units** (`Tests/ZM_Tests_BagScreen.cpp`, category `ZM_BagScreen`; fixtures built through the REAL `ZM_Bag::Add`, expectations computed from `ZM_ITEM_CATEGORY_COUNT` / `uROWS_PER_PAGE` rather than magic numbers, including an explicit "changing pocket resets a paged-away page" case and a starter-bag end-to-end case that resolves pockets via `ZM_GetItemData`) + the windowed **`ZM_BagScreen_Test`** (41 frames, `skipped: false`). Game-only units -> `zm-tests.yml` **2139 -> 2161**; engine default (**1097**) UNCHANGED.
- **Gate (dev machine, all green):** build clean (0 errors, 0 warnings); boot unit gate **2161 ran / 2160 passed / 0 failed / 1 skipped**; headless **27/0**; full WINDOWED suite **27/0**, all five UI windowed tests verified `skipped: false` -- including `ZM_PartyScreen_Test` still passing after its nav links were removed, which is the proof the spatial fallback covers that screen.
- **Reversibility:** additive presenter + a link deletion on two pools. The nav-link rule generalizes: **never wire `SetNavigation` into a pool whose members are shown/hidden at runtime** -- recorded in Shortfalls 1.6 for SC7+.

---

## 2026-07-19 -- ZM-D-118 -- S6 item 2 (SC5): `ZM_UI_Dex` -- the paged dex screen, and the FIRST consumer of the E4 grid

- **Decision:** Ship SC5 -- the dex screen -- as a NON-ECS `ZM_UI_Dex` presenter owned **BY VALUE** by `ZM_UI_MenuStack` (the SC2/SC4 seam), PODs-only state (an int page + an int cursor). **No new ECS component and no new order** (next-free stays **113**). It adds exactly **one arm to each of the two per-screen dispatch sites** that SC4 generalized -- the machine was NOT reshaped, which is the payoff SC4 was for.
  1. **The grid is built ONCE AT RUNTIME, and the ownership routine is load-bearing.** The engine deliberately exposes no `Zenith_UIComponent::CreateGridLayoutGroup` and no `AddStep_CreateUIGridLayoutGroup` (adding one is an ENGINE change requiring full cross-game regression -- rejected as outside SC5's scope, per plan Q-D). So `Present` builds the `Zenith_UIGridLayoutGroup` + its 30 cells on first use, guarded on `FindElement(szGRID_NAME) == nullptr`, against the PERSISTENT `ZM_MenuRoot` canvas -- once per process. The routine is `new` + `Zenith_UICanvas::AddElement(grid)`, then per cell `CreateButton(name, text)` (which itself `new`s + `AddElement`s) followed by **`Zenith_UICanvas::ReparentElement(cell, grid)`**. That is the ONLY correct path, verified in the engine source: `AddElement` pushes into BOTH `m_xAllElements` and `m_xRootElements`; `Clear()`/the dtor delete **only** `m_xAllElements`, so an element that is merely `AddChild`'d **LEAKS**; and `AddChild` does not remove from `m_xRootElements`, so an `AddElement`'d-then-`AddChild`'d element would be **walked twice** (Update and Render both iterate the roots and recurse). `ReparentElement` erases from the roots while leaving ownership intact.
  2. **Only the static widgets are authored at bake time** (`Menu_DexPanel`, `Menu_DexHeader`, `Menu_DexPrevPage`, `Menu_DexNextPage`) in `ZM_ConfigureMenuRoot`, all hidden. The grid + cells never enter the baked scene.
  3. **Traversal inside the grid is the ENGINE SPATIAL focus-nav** (`FindNearestFocusable`) -- no hand-rolled grid cursor arithmetic and deliberately **no inter-cell `SetNavigation` links**; the spatial search is the entire reason to use a grid. The two page buttons sit below the grid and are reached by the same spatial walk. Confirm dispatches **by the FOCUSED ELEMENT'S NAME** (never `SetOnClick` with a component pointer, which dangles on ECS pool relocation).
  4. **Paging:** 5 columns x 6 rows = 30 cells/page over `ZM_SPECIES_COUNT` = **152**, so **6 pages**, the last holding just **2** live cells. Trailing dead cells are set **non-visible AND non-focusable** -- non-focusable so the nav can never park on a blank entry (the SC4 watch-out), and non-visible because the grid lays out only VISIBLE children, which is also what keeps the row-major layout tight. An UNCAUGHT entry renders `"#NNN -----"`: the species NAME stays hidden until it is caught, and a unit asserts the name is genuinely ABSENT.
- **Two implementation subtleties worth recording.** (a) `Zenith_UIElement::SetVisible` unconditionally notifies its parent, and `Zenith_UIGridLayoutGroup` marks itself layout-dirty on that notification, re-running `RecalculateLayout` (whose child `SetPosition` is unguarded) -- so writing visibility every frame kept the grid **permanently dirty**. Both the `Present` and `Hide` cell loops now write visibility only on CHANGE. `Hide` matters at least as much as `Present`, because the MenuStack calls it on every frame the dex is not the top screen. The equivalent guards on the panel/grid/header/page buttons were **rejected as dead code**: those are un-reparented roots with `m_pxParent == nullptr`, so the notify branch cannot run. (b) `SetFocusable` is left unguarded because it is a plain assignment with no parent notify.
- **A latent SC4 bug found and fixed forward.** The dex review caught that `Present`'s focus fallback wrote `m_iCursor = 0` even when the slot-0 element failed to resolve and the focus was therefore cleared -- meaning the windowed test, which asserts on the cursor, would have PASSED in exactly the scenario it exists to catch. **The identical defect was already shipped in `ZM_UI_Party::Present` (SC4)**; both now resolve the element first and mirror the real outcome (`cursor = found ? 0 : -1`). `Games/Zenithmon/Source/UI/ZM_UI_Party.cpp` is therefore also touched by this commit.
- **Tests that lock it:** **+20 pure T0 units** (`Tests/ZM_Tests_DexScreen.cpp`, category `ZM_DexScreen`) -- every paging expectation is a **computed formula over `ZM_SPECIES_COUNT`, never a magic number**, so the suite survives the roster growing; plus the windowed **`ZM_DexScreen_Test`**, which proves the RUNTIME-BUILT grid materialises (all 30 cells resolve by name), exactly `VisibleCellCount(page 0)` cells are visible, the caught/hidden labels are correct on screen, the spatial nav walks Down off the grid onto a page button with no explicit links, and Next relabels the grid. It genuinely RUNS (`skipped: false`, 56 frames). Game-only units -> `zm-tests.yml` **2119 -> 2139**; engine default (**1097**) UNCHANGED -- no engine code touched, so no cross-game regression was required.
- **Gate (dev machine, all green):** build clean (0 errors, 0 warnings); boot unit gate **2139 ran / 2138 passed / 0 failed / 1 skipped**; headless **26/0**; full WINDOWED suite **26/0**, with all four UI windowed tests verified `skipped: false`.
- **Reversibility:** additive -- one by-value member, one arm per dispatch site, four authored (hidden) widgets, and a runtime-built grid that lives only in memory. If a second grid consumer ever appears (SC6's bag pockets are the candidate), revisit whether the engine convenience (`CreateGridLayoutGroup` + an authoring step) is now worth its cross-game regression cost.

---

## 2026-07-19 -- ZM-D-117 -- S6 item 2 (SC4): the `ZM_UI_Party` screen + a GENERALIZED menu-screen dispatch

- **Decision:** Ship SC4 -- the party list + per-member summary -- and, first, the screen-dispatch generalization the remaining SCs all need.
  1. **Part A, done FIRST and behaviour-preserving: generalize the dispatch.** `ZM_UI_MenuStack` hard-coded "ROOT or not" in **three** places (the confirm/cancel routing in `OnUpdate`, the focus policy in `PresentTopScreen`, and the element show/hide in `PresentTopScreen`) -- a shape the SC2 reviewers flagged as forcing every later screen to reshape the same three sites. It is now **two per-screen switches**: one input-routing switch in `OnUpdate` (DIALOGUE keeps Tick + confirm-only with cancel deliberately unrouted; ROOT/BAG/DEX share the `HandleConfirm`/`HandleCancel` arm verbatim; PARTY is new) and one show/hide + focus-policy block in `PresentTopScreen` (`SetRootElementsShown`, the dialogue Present/Hide, `PresentPartyScreen`, then a single focus switch where ROOT and PARTY own the canvas focus and every other screen clears it and sets `m_iCursor = -1`). **Adding SC5 Dex / SC6 Bag / SC7 Shop is now one arm per site plus a by-value presenter -- never another reshape.** Proven behaviour-preserving by the two pre-existing windowed tests passing UNCHANGED.
  2. **`ZM_UI_Party`** (`Source/UI/`) -- a NON-ECS presenter owned **BY VALUE** by `ZM_UI_MenuStack` (the SC2 `ZM_UI_DialogueBox` seam), PODs-only state (an int cursor + a bool), so **no new ECS component and no new order** (next-free stays **113**). It renders the LIVE `ZM_GameState::m_xParty` resolved through `ZM_GameStateManager::TryGetGameState`, and **hides itself rather than crashing** when no game state resolves (headless / between scenes).
  3. **Traversal is the ENGINE focus-nav** over authored, navigation-linked slot buttons -- no hand-rolled cursor. Unfilled slots are set non-visible **AND non-focusable**, because the engine nav collects visible+focusable elements, so nav can never park on an empty row. `ZM_Party::Get` **asserts** on an out-of-range index (and `Zenith_Assert` is live in this config), so every read is gated on `VisibleSlotCount(party.Count())`.
  4. **Confirm toggles the per-member summary; cancel is offered to the SCREEN FIRST** -- an open summary swallows the first Escape and only the second pops back to ROOT (`ZM_UI_Party::Cancel()` returns true = consumed, false = pop). There is no per-member action menu yet; switch / use-item / cancel-target arrive with SC6/SC7.
  5. **No duplicated formatting:** the list row REUSES `ZM_UI_BattleHUD::FormatHpPanel` verbatim (plus a `FAINTED` marker) rather than re-deriving the `"<Species>  Lv<n>  HP <cur>/<max>"` string -- one formatter, two screens.
- **Two judgement calls worth recording.** (a) **`ZM_MAJOR_STATUS` has NO table-layer name accessor** anywhere in the game (the enum lives bare in `ZM_BattleTypes.h`, and the battle HUD only ever emits generic status text). Rather than invent a data-layer accessor from a UI sub-commit, the summary's status label is a **file-local mapper next to its only consumer**; promote it to `ZM_BattleTypes` when a second caller justifies it. (b) The summary panel sits at **9002/9003**, not the 9000/9001 menu band: elements draw in ASCENDING sort order, so a summary at 9000 would render the six slot buttons (9001) straight THROUGH the overlay meant to cover them. Still far below WarpFade 10000 / BattleFade 10001.
- **Review (4 pre-build lenses, every finding verified before applying): NO BLOCKERS, 6 findings, all real, all fixed.** Most notable: (i) the summary panel was **2 px too short to cover the slot stack** it overlays (slots span 304 px at a 52 px pitch; the panel was 300) -- a full party would have bled through the top and bottom edges, exactly the defect class the S5 bleed-through gate (ZM-D-112) exists for, and one the windowed test could NOT catch because it only checks `IsVisible`; (ii) when the game state failed to resolve, `PresentPartyScreen` hid the screen but the focus switch still claimed the canvas focus and mirrored a stale cursor -- it now returns a bool and degrades to the non-navigable policy; (iii) `Confirm()` on an EMPTY party raised an invisible-summary flag that `Cancel()` then silently swallowed, a two-Escape input dead-end; (iv) a unit whose "invalid root tolerates an empty party" guard was unreachable through the path it exercised, replaced with one that genuinely reaches the empty-party model path.
- **Tests that lock it:** **+18 pure T0 units** (`Tests/ZM_Tests_PartyScreen.cpp`, category `ZM_PartyScreen`: the slot-name contract and its round trip incl. near-miss names, `VisibleSlotCount` clamping, the row formatter and its FAINTED boundary at curHp 0 vs 1, the summary's exact LINE COUNT against the record's non-empty move count, the status-label mapping asserted on real BURN/TOXIC records, the Confirm/Cancel/Reset state machine, the empty-party guard, and best-effort `Present`/`Hide` against an invalid root) + the windowed **`ZM_PartyScreen_Test`** (open menu -> confirm Party -> assert the screen, the exact visible slot count and the focused element -> confirm -> summary open with non-empty text -> Escape -> summary closed but STILL on PARTY -> Escape -> back on ROOT -> Escape -> closed + player movable), which genuinely RUNS (`skipped: false`, 30 frames). Game-only units -> `zm-tests.yml` **2101 -> 2119**; engine default (**1097**) UNCHANGED -- no engine code touched, so no cross-game regression was required.
- **Gate (dev machine, all green):** build clean (0 errors, 0 warnings); boot unit gate **2119 ran / 2118 passed / 0 failed / 1 skipped**; headless **25/0**; full WINDOWED suite **25/0** -- with `ZM_MenuOpenClose_Test` and `ZM_DialogueTalk_Test` passing UNCHANGED, which is the proof that Part A preserved ROOT and DIALOGUE behaviour.
- **Reversibility:** the presenter is additive (one new by-value member, one new arm per dispatch site, four new authored widgets). Part A is a pure refactor with no behaviour delta. NOTE: `ZM_PartyScreen_Test` asserts all six slot widgets resolve by name, so a **stale baked `FrontEnd.zscen` will fail it** -- a `*_True` boot re-authors the scene, which the standard gate does automatically.

---

## 2026-07-19 -- ZM-D-116 -- S6 item 2 (SC3): the bag + money DATA MODEL (`ZM_Bag`, `ZM_GameState::m_uMoney`)

- **Decision:** Ship SC3 of the S6 game-UI decomposition -- the pure runtime economy model that SC6 (Bag screen) and SC7 (Shop) both depend on. Neither a bag container nor a money balance existed before this: only the compiled ITEM TABLE (`Source/Data/ZM_ItemData.h`). SC3 adds the MODEL ONLY -- no ECS component, no UI, no disk I/O, no new component order (next-free stays **113**), nothing visual.
  1. **`ZM_Bag`** (`Source/Party/ZM_Bag.{h,cpp}`) -- a pure fixed-capacity aggregate: one pocket per `ZM_ITEM_CATEGORY` (9) x 64 `ZM_ItemStack { ZM_ITEM_ID, u_int count }` slots plus a live per-pocket count. Each pocket is kept **sorted ASCENDING by `ZM_ITEM_ID`** via a shifting insert / erase, so the SC6 Bag screen gets a stable deterministic row order for free and a single linear scan answers both "where is this stack" and "where would a new stack go".
  2. **Shaped to the PRE-LOCKED save schema** (`Docs/SaveFormat.md`) so S7 serializes it without a reshape: module 6 Bag = `entryCount uint16` + `{ itemId uint16, count uint16 } x N` with **"count >= 1 -- zero-count entries are never written"**, module 7 Money = `uint32`. `Remove` therefore **ERASES** a stack that reaches zero (shifting the later stacks down and clearing the trailing slot) so a count-0 stack is structurally unstorable, and `TotalStackCount()` is exactly what module 6 writes as `entryCount`.
  3. **Every mutator is ALL-OR-NOTHING** -- it never clamps-and-succeeds. This is the load-bearing decision: SC7's shop transaction is `SpendMoney(price)` THEN `bag.Add(item, qty)`, so a partial or clamping `Add` would silently destroy the player's money. Both cap checks are **headroom-first** (`uCount > uZM_BAG_MAX_STACK_COUNT - existing`) so a near-`UINT_MAX` count can never wrap into an accidental success.
  4. **`ZM_GameState`** gains `m_xBag` + `m_uMoney` (mirroring module 7) with `uZM_MONEY_CAP = 999999` and two helpers: `AddMoney` (saturating, **headroom computed BEFORE the addition** so it never wraps) and `SpendMoney` (false + NO mutation when unaffordable -- the guard SC7 leans on). `AddMoney`'s headroom subtraction is itself guarded, because `m_uMoney` is a public aggregate field that **S7's loader will assign straight from the module-7 uint32** -- an edited save can legitimately carry a balance above the cap, and an unguarded `uZM_MONEY_CAP - m_uMoney` would underflow to near-`UINT_MAX` and turn a saturating credit into a wrapping one. An over-cap balance credits **nothing** (a no-op) rather than being clamped DOWN: silently deleting money a save carried is the worse failure.
  5. **Starter seed:** money **3000** + **5x Catch Orb** + **3x Salve** (plan Q-C's ruled placeholders; real economy tuning is S11). Seeded THROUGH `AddMoney`, not a direct write, so the seed can never bypass the cap. `ZM_GameStateManager::ResetGameStateForTests()` already re-seeds via `ZM_MakeStarterGameState()`, so bag + money reset between batched tests for FREE -- no new reset plumbing was added.
- **Capacity rationale (measured, not guessed):** `ZM_ITEM_COUNT` is **90** and the largest per-category count is **25 (TM)** -- distribution BALL 8 / MEDICINE 24 / BATTLE 3 / HELD 12 / BERRY 6 / EVO 5 / TM 25 / KEY 6 / FIELD 1. A 64-slot pocket therefore always holds every item of its category and the "pocket full" branch is **unreachable**; it is kept as a defensive guard (the `ZM_SpeciesSet` discipline) and a unit walks the whole table each boot so S9/S10 content growth past 64 fails LOUDLY rather than silently rejecting items. A `static_assert((u_int)ZM_ITEM_COUNT <= 512u)` pins the module-6 512-entry cap the same way.
- **Review (4 pre-build lenses + 2 post-gate lenses, each finding adversarially refuted): NO BLOCKERS.** The pre-build pass caught three real defects: a **vacuous** "no count-0 stack survives" test (its walk was bounded by a count the previous line had just asserted was 0, so it executed zero iterations -- the fixture now keeps a surviving neighbour so the walk runs and the test fails a decrement-without-shift AND a shift-without-decrement implementation); `AddMoney`'s unguarded headroom subtraction (4 above); and an uncovered inclusive boundary on the stacking cap path (mutating `>` to `>=` now fails). It also caught that the proposed DecisionLog id **collided with SC2's ZM-D-115** -- this entry is ZM-D-116.
- **Tests that lock it:** **+23 pure T0 units** (`Tests/ZM_Tests_Bag.cpp`, category `ZM_Bag`) covering pocket routing resolved via `ZM_GetItemData` (never a hard-coded index), stacking, all four `Add` and three `Remove` rejection paths each asserting the PRE-STATE is intact, the erase-at-zero shift, ascending-order maintenance across insert AND erase, bounds-safe accessors, money saturation/underflow, the starter seed contents, the item-table capacity invariant, the 512-entry save-cap pin, and an SC7-shaped buy/sell round trip locked in early so SC7 only has to add the guards. Game-only units -> `zm-tests.yml` **2078 -> 2101**; engine default (**1097**) UNCHANGED -- SC3 touches no engine code, so no cross-game regression was required.
- **Gate (dev machine, all green):** build clean (0 errors, 0 warnings); boot unit gate **2101 ran / 2100 passed / 0 failed / 1 skipped**; headless **24/0**; full WINDOWED suite **24/0** (run deliberately: `ZM_GameState` grew ~4.6 KB with the pocket array and is held BY VALUE by the `ZM_GameStateManager` ECS component, so the persistence + battle round-trip tests are the regression that matters).
- **Reversibility:** fully additive and pure -- two new files plus two fields and two helpers on an existing aggregate. No engine change, no ECS order, no UI. The contract is deliberately frozen here because SC6 renders it and SC7 mutates it; changing it later means rewriting both under the no-legacy-shims rule.

---

## 2026-07-19 -- ZM-D-115 -- S6 item 2 (SC2): `ZM_UI_DialogueBox` -- the modal dialogue screen on `ZM_UI_MenuStack`

- **Decision:** Ship SC2 of the S6 game-UI decomposition (`Build/artifacts/zm_s6_ui_plan/plan.md`): the dialogue box.
  1. **`ZM_UI_DialogueBox`** (`Source/UI/`) -- a NON-ECS presentation class owned **BY VALUE** by `ZM_UI_MenuStack` (the same `ZM_BattleDirector` -> `ZM_UI_BattleHUD` seam), so SC2 consumes **NO new ECS component order** (next-free stays **113**). It holds a fixed 8-slot `std::string` line queue + a typewriter clock + an instant-reveal flag -- only `std::string` and PODs, so the host's defaulted `noexcept` move stays well-formed under ECS pool relocation. Headless model: `Reset` / `QueueLine` / `QueueLines` (ALL-OR-NOTHING) / `Tick` / `Confirm` / the read accessors; `Present` / `Hide` are the only members that touch UI, and they **re-resolve both elements by NAME every frame** (never cache -- the pool relocates).
  2. **Advance contract** (`ZM_DIALOGUE_ADVANCE`, frozen + unit-tested verbatim): confirm on an incomplete reveal **completes the typewriter** (`COMPLETED_REVEAL`, the player never skips an unseen line); confirm on a complete reveal **advances** (`NEXT_LINE`, clock + instant flag reset); confirm on the last line **closes** (`CLOSED`, full `Reset`); confirm on an inactive box is `IGNORED`.
  3. **MODAL by design:** `ZM_UI_MenuStack::OnUpdate` routes input to the dialogue whenever `ZM_MENU_SCREEN_DIALOGUE` is on top, and **cancel/Escape is deliberately IGNORED there** -- a dialogue closes only by being read to the end, so a prompt can never be escaped past (the hook S6 item 3's yes/no prompts will hang off). The ROOT confirm/cancel dispatch does not also run on a dialogue frame.
  4. **Raise seam:** `PushDialogueLines` (instance) / `TryPushDialogue` (singleton-resolving -- what S6 item 3's `ZM_Interactable` and the windowed test call). Pushing onto an EMPTY stack freezes the player, so an NPC can talk without the pause menu ever being opened; pushing over an open screen stacks DIALOGUE on top and popping returns to it; a rejected batch never mutates the queue, and a push that cannot claim a stack slot rolls the queue back rather than stranding lines with no screen to show them.
  5. **Enum + authoring:** `ZM_MENU_SCREEN_DIALOGUE` is **APPENDED** (value 5) to the save-stable screen enum -- never reachable from a ROOT entry (`RootActionToScreen` never returns it; unit-pinned). `Menu_DialoguePanel` + `Menu_DialogueText` are authored on the existing persistent `ZM_MenuRoot` canvas in the same **9000/9001** sort band (below WarpFade 10000 / BattleFade 10001, so a fade always covers the box), bottom-centre, word-wrapped, authored HIDDEN. The text element takes an explicit `SetSize(820,120)` + `TextAlignment::Center` matching its `SetMaxWidth(820)`: the default 100x100 Left-aligned bounds would have started the line at centre-50 and run it off the right of the screen (the `BattleHUD_Log` idiom).
  6. **No duplicated reveal math:** both the headless proxy and `Present` delegate to the existing `ZM_UI_BattleHUD::ComputeVisibleGlyphCount` -- the 45 glyphs/sec rate lives in exactly one place. The model's glyph total is the raw character count, deliberately CONSERVATIVE against the engine's post-wrap total, so it never reports "revealed" earlier than the screen does.
- **Review (4 pre-build lenses + 3 post-gate lenses, each finding then adversarially refuted):** **NO BLOCKERS, 0 findings survived refutation.** The pre-build pass fixed 8 real defects before the first build (the missing text `SetSize`/`SetAlignment` above; two unit fixtures that passed vacuously; and missing coverage for Escape-is-modal, the Present/Hide + authoring contract, the stacked-over-ROOT path, and `TryPushDialogue`/`Present` without a singleton). The post-gate pass raised 20 findings and **all 20 were refuted with file:line evidence** -- the recurring cluster (no warp/battle gate on the raise path; the raising confirm edge being re-read the same frame; dialogue/battle-menu confirm cross-talk) is unreachable today because the only caller of the raise API is the tests: `ZM_Interactable` is still an unchecked S6 item 3. Those are carried as forward-notes (Shortfalls 1.6), NOT as defects in this change. Two strictly-better fixes were applied by the orchestrator afterwards and re-gated: the windowed on-close widget check now also asserts the elements RESOLVED (the "hidden" flags alone matched a default-initialised view, so it could have passed vacuously), and the two "trivially movable" header comments now state the accurate invariant (noexcept-movable -- `std::string`'s move is noexcept; that is what the defaulted move actually requires).
- **Tests that lock it:** **+22 pure T0 units** (`Tests/ZM_Tests_DialogueBox.cpp`, category `ZM_DialogueBox`: queue/capacity/all-or-nothing rejection, the four `Confirm` outcomes, reveal timing recomputed exactly against `floor(elapsed * 45)`, append-while-active, `Reset`, best-effort `Present`/`Hide` on an invalid root, `TryPushDialogue` without a singleton, and the screen-enum save-stability pin) + the windowed **`ZM_DialogueTalk_Test`** (`Tests/ZM_AutoTests_UI.cpp`: push 2 lines -> assert open on DIALOGUE + player frozen -> typewriter mid-reveal -> **Escape ignored** -> confirm completes the reveal (index still 0) -> confirm advances to line 1 -> finish -> assert closed + model inactive + widgets resolved-and-hidden + movement re-enabled; then the STACKED half: open ROOT -> push over it -> finish -> assert still open on ROOT and still frozen -> Escape -> closed + movable). Game-only units -> `zm-tests.yml` **2056 -> 2078**; engine default (**1097**) UNCHANGED -- SC2 touches no engine code, so no cross-game regression was required.
- **Gate (dev machine, all green, re-run after the orchestrator's two fixes):** Zenithmon build clean (0 errors, 0 warnings); boot unit gate **2078 ran / 2077 passed / 0 failed / 1 skipped**; headless **24/0**; full WINDOWED suite **24/0**, with `ZM_DialogueTalk_Test` genuinely RUNNING (result json `skipped: false`, 50 frames) rather than soft-skipping.
- **Reversibility:** fully additive and self-contained -- one appended enumerator, one by-value member, two authored (hidden) widgets, no engine change and no new ECS order. The forward-notes in Shortfalls 1.6 flag where SC7/SC8 and S6 item 3 will need to EXTEND this surface (a choice/confirm variant and a completion signal); under the no-legacy-shims rule those extend the API in place rather than wrapping it.

---

## 2026-07-19 -- ZM-D-114 -- S6 item 2 (SC1): `ZM_UI_MenuStack` overworld pause menu + an engine ECS underflow-tolerance fix

- **Decision:** Ship SC1 of the S6 game-UI decomposition (`Build/artifacts/zm_s6_ui_plan/plan.md`): the overworld pause-menu foundation.
  1. **`ZM_UI_MenuStack`** -- a new ECS component (serialization order **112**) on a new persistent `ZM_MenuRoot` entity (authored in FrontEnd, `DontDestroyOnLoad`, cloning `ZM_BattleTransitionRoot` + `ZM_ConfigureBattleHUD`). Opens a focus-navigable ROOT menu (Party/Bag/Dex/Exit) on **M/Tab** in the overworld (gated on `!warp && !battle` via the pure `ShouldOpenMenu`/`IsOverworldSceneKind`), FREEZES the player (`ZM_PlayerController::SetMovementEnabled`, unfreeze guarded on `!IsWarpInProgress && !IsTransitionActive`), traverses via the ENGINE focus-nav API (arrows), dispatches confirm **BY THE FOCUSED ELEMENT'S NAME** (never `SetOnClick(this)` -- a `this` userdata dangles on ECS pool relocation), and pops on Escape (unfreeze + clear canvas focus to nullptr when the stack empties). Screen presenters are non-ECS `Source/UI` classes owned by-value (the `ZM_UI_BattleHUD` idiom); SC1 ships the ROOT + placeholder Party/Bag/Dex screens (real presenters = SC2+). Menu sort band **9000/9001**, below the fade overlays (10000/10001), so a warp/battle fade always covers the menu. Between-tests hook resets it (`ResetRuntimeStateForTests`, added alongside the other two persistent singletons per the reviewer).
  2. **ENGINE FIX (required by SC1): `Zenith_SceneData::CancelPendingStart` underflow tolerance** (`Zenith/ZenithECS/Zenith_SceneData.h`). Adding a SECOND persistent-singleton entity (`ZM_MenuRoot`, alongside `ZM_BattleTransitionRoot`) shifts the automated-harness's between-tests scene-cycling interleaving so a `PENDING_START` entity is destroyed when the per-scene `m_uPendingStartCount` is already 0 -- the old hard `Zenith_Assert(m_uPendingStartCount > 0, ...)` then crashed EVERY automated test (headless 22/0 -> 0/24, `exit=STATUS_BREAKPOINT`). The fix gives `CancelPendingStart` the SAME underflow tolerance already present (3x, CI-proven) in its sibling `ProcessSinglePendingStart` (`Internal/Zenith_SceneData.cpp` lines ~1051/1087/1107): `if (m_uPendingStartCount > 0) { m_uPendingStartCount--; }`. This is a latent engine inconsistency any second persistent-singleton would hit, not a game bug.
- **Reviewer (adversarial, both parts): NO BLOCKERS.** Part A (engine): verified the tolerance CANNOT mask a genuine double-cancel (the `if (!IsPendingStart()) return;` guard + both callers pre-guard on `IsPendingStart()`; a same-slot second cancel is already a no-op before the count is touched), `m_axPendingStartEntities.EraseValue` is idempotent, healthy-path is byte-identical. Part B (game): the persistent-singleton claim-or-destroy, name-dispatch, guarded freeze/unfreeze (traced the "battle starts while menu open" interleaving -- no permanent-freeze), gating, and single-canvas focus routing are all correct; the 22 units + windowed test genuinely exercise the behavior. One SHOULD-FIX applied (the between-tests `ResetRuntimeStateForTests` call); 2 NITs left (a loose comment example on the engine fix; the SC1 placeholder screens are blank-until-Escape, documented intended for the slice).
- **Tests that lock it:** **+22 pure T0 units** (`Tests/ZM_Tests_MenuStack.cpp`, category `ZM_MenuStack`: stack push/pop/top/depth/clear, open<->close, the ROOT-item enum, the focused-name->action resolver, the gating predicate) + windowed `ZM_MenuOpenClose_Test` (`Tests/ZM_AutoTests_UI.cpp`: open->assert frozen->navigate (cursor 0->1)->Escape->assert closed + movement re-enabled + focus cleared). Game-only units -> `zm-tests.yml` **2034 -> 2056**; engine default (1097) UNCHANGED (the engine fix adds no units). A dedicated unit test for the ECS tolerance is impractical (needs the exact between-tests interleaving; the sibling tolerances also have none -- the lock is the full automated suite + cross-game regression).
- **Gate (dev machine, all green):** Zenithmon build clean; boot unit gate **2056 / 0 failed**; headless **23/0** (recovered from the 0/24 crash); windowed `ZM_MenuOpenClose_Test` PASS. **Cross-game engine regression** (the ECS fix touches every game; each FORCE-REBUILT against the new engine): **Combat** unit gate **1097/0** + suite **14/0**; **DevilsPlayground 158/0** (the heaviest scene-cycling suite -- where the sibling tolerances were originally needed); **CityBuilder 45/0**; **RenderTest** boots clean, 10 TerrainEditor failures = the pre-existing missing-terrain redness (Q-2026-07-16-001), unchanged by the fix.
- **Reversibility:** the engine fix is a minimal robustness guard (byte-identical healthy path, completes an established pattern) -- low risk. The game menu-stack is additive (one new component/order, one new persistent entity). SC2 (dialogue box) builds on this.

---

## 2026-07-19 -- ZM-D-113 -- S6 item 1: engine E4 -- `Zenith_UIGridLayoutGroup` (fixed-column grid UI element)

- **Decision:** Ship the last pre-approved engine change (E4, ZM-D-002): a new engine UI element **`Zenith_UIGridLayoutGroup`** (`Zenith/UI/`) -- the grid analogue of the existing h/v-only `Zenith_UILayoutGroup`. Fixed column count + fixed cell size, row-major placement of VISIBLE children (hidden leave no gap), horizontal/vertical spacing, 4-way padding, and fit-to-content auto-size. It is the engine primitive Zenithmon's S6 bag/box/dex/party grid screens compose on. **Purely additive** -- a new `UIElementType::GridLayoutGroup` (appended BEFORE `COUNT`, serialization-safe) + factory arms + convenience include; NO existing element/behavior/serialization changed, and nothing instantiates it yet.
  - **Design (plan-first, Planner subagent):** mirrors `Zenith_UILayoutGroup` exactly -- `m_bLayoutDirty` cleared at the top of `RecalculateLayout`, `Update` recomputes only when dirty then calls base `Update`, four `OnChild*` overrides re-dirty, visible-only placement, base-first serialization with a v1 version constant, `.Tests.inl` hosted like `Zenith_UIText.cpp`.
  - **Anti-thrash guard (load-bearing):** `SetSize` fires `OnChildSizeChanged -> re-dirty`, so a child is resized to the cell ONLY when it differs (component-wise compare) -- the layout converges in <=1 extra frame instead of re-laying-out every frame. `SetPosition` marks only transform dirty, so it never re-dirties layout. Auto-size writes `m_xSize` directly (no self-notify), matching `FitContainerToContent`.
  - **Scope v1 (YAGNI):** top-left row-major, force child to cell size (Unity `GridLayoutGroup` contract); start-corner + fill-axis DEFERRED (no consumer, would need a new enum / ambiguous column-major math). So NO new enum introduced.
- **Tests that lock it:** **+9 engine `ZENITH_TEST(UIGrid, ...)` units** in `Zenith_UIGridLayoutGroup.Tests.inl` (pure CPU positional-math + serialization; headless-safe): row-major 3-col placement; cell+spacing pitch; 7th-child wrap; padding offset; partial last row; empty/all-invisible no-op; single-column vertical stack; fit-to-content bounds (5/3 -> 148x98, 2/3 -> 98x48); serialization round-trip. Engine unit count **1088 -> 1097** (game boots via zenithmon **2025 -> 2034**).
- **Reviewer (adversarial, engine change): CLEAN -- no BLOCKER/SHOULD-FIX.** Layout math re-derived correct + non-tautological; anti-thrash convergence traced (frame-2 stable); enum/serialization safe (no `COUNT`-sized arrays repo-wide; both switches have `default:`); stack-test-children safe (element dtor only `Clear()`s, never deletes -- canvas owns). One convention NIT fixed (bare `n` -> `uVisibleCount`); the harmless `mutable` left for sibling parity with UILayoutGroup.
- **Gate (dev machine, all green):** build clean; **boot unit gate 2034 / 0 failed** (Zenithmon), the 9 UIGrid units pass. Cross-game engine regression (every game FORCE-REBUILT against the new engine lib first -- `zenith test` runs a STALE exe otherwise): **Combat** unit gate **1097/0** + automated **14/0**; **DevilsPlayground 158/0**; **CityBuilder 45/0**; **RenderTest** boots clean, 10 TerrainEditor failures = the pre-existing missing-terrain redness (Q-2026-07-16-001), NOT E4 (additive-only, no terrain path touched). Baselines bumped in the SAME commit: `run_unit_gate.ps1` default **1088 -> 1097** (covers engine-gate.yml + scaffold-smoke.yml, which use the default) + `zm-tests.yml` **2025 -> 2034**.
- **Build wiring:** the generated `.vcxproj`/`.filters` are gitignored; `Sharpmake_Zenith.cs` globs `SourceRootPath = /Zenith`, so `Build\regen.ps1` auto-picks up the new `Zenith/UI/*` files (NO hand-edit of project files -- the plan's proposed vcxproj edits were corrected to a regen).
- **Reversibility:** easy -- purely additive engine surface localized to `Zenith/UI/`; the enum append + factory arms are the only shared-surface touch and are serialization-safe. Nothing depends on it yet.

---

## 2026-07-18 -- ZM-D-112 -- S5 STAGE GATE SIGNED OFF (user APPROVED the overworld bleed-through visual check)

- **Verdict (gate sign-off):** the user reviewed the rendered S5 bleed-through evidence -- a deterministic before -> in-arena -> after round trip (`Build/artifacts/zenithmon/s5/visual/cap_overworld_before.png`, `cap_arena_a.png`, `cap_overworld_after.png`), presented as a published Artifact -- and **APPROVED** the S5 visual gate. The in-battle frame shows the additive Battle scene at world Y=-2000 (dome + two platforms + two Fernfawn + biome dressing) with the overworld grass cleared to 0 and **no Dawnmere terrain/grass bleeding into the viewport** (only the shared global skybox at the horizon, which the gate accepts as per-frame not per-scene). Q-2026-07-09-003 is CLOSED (the additive-at-offset design is confirmed; the SINGLE-load + world-state-snapshot fallback is NOT needed).
- **Evidence method:** the frames were captured by temporarily instrumenting `Step_ZMBattleMenu` (Win mode) with `Flux_Screenshot::RequestDump` at the Walk overworld frame + early AwaitResume, running `ZM_BattleMenuWin_Test` windowed, then **reverting the instrumentation** (master stayed clean; exe rebuilt to match HEAD). TGA swapchain dumps -> PNG. This session also re-confirmed the automated gate first-party (headless 22/0 + boot unit gate 2025/0) before setting the GATE-WAIT at SC6 (ZM-D-111).
- **Result:** **S5 (Battle integration slice) COMPLETE + gate-signed.** Roadmap S5 `*Gate:*` line marked MET + SIGNED OFF; Status.md `GATE-WAIT: S5 visual sign-off` marker CLEARED. The lifecycle loop resumes at **S6 (Dialogue, menus, NPCs, shops)** -- first task: engine E4 `Zenith_UIGridLayoutGroup`.
- **Reversibility:** n/a (sign-off record). The next visual hard-stop is the S8 vertical-slice go/no-go (manual playthrough sign-off).

---

## 2026-07-18 -- ZM-D-111 -- S5 item 5 (SC6): item-5 CLOSER + S5 automated stage gate GREEN -> GATE-WAIT (S5 visual sign-off)

- **Decision:** Close S5 item 5 ("Catch / exp / faint / whiteout applied to GameState") and open the S5 STAGE gate. SC6 is a **docs-only closer** (no new source, no new tests -- item 5's gameplay shipped in SC1-SC5, ZM-D-106..110): run the consolidated S5 automated stage gate, tick the item-5 Roadmap box (line 87), refresh Shortfalls.md, capture the bleed-through evidence, and set a **`GATE-WAIT: S5 visual sign-off`** marker in Status.md -> HARD STOP for the user (the S5 VISUAL hard-stop, user's standing order 2026-07-10). Do NOT tick the S5 stage gate or start S6 without the user's sign-off (StartPrompts prompt 4 -> a separate later DecisionLog entry).
- **Automated S5 stage gate -- ALL GREEN** (evidence `Build/artifacts/zenithmon/s5/visual/S5_GATE_EVIDENCE.md` + 7 per-test result JSONs, git-ignored):
  - **5-config build matrix:** Vulkan Debug/Release x True/False + `D3D12_vs2022_Debug_Win64_False` null-backend link proof (Flux backend-neutrality) -- all GREEN.
  - **Boot unit gate** (`run_unit_gate.ps1 -Baseline 2025`): **2025 ran / 2024 passed / 0 failed / 1 skipped.**
  - **`zenith test Zenithmon --headless`:** **22 passed / 0 failed.**
  - **Full windowed suite** (`zenith test Zenithmon`): **22 passed / 0 failed** (113 s), incl. the 10-test windowed battle suite (`--filter ZM_Battle` = **10/0**) + `ZM_GameStatePersistence`.
  - The ~380 S2 `ZM_Battle` engine goldens stayed BYTE-IDENTICAL through all of item 5 (pure wiring, zero engine changes).
  - **Re-confirmed first-party at the SC6 commit** (against the byte-identical pushed SC5 code, no code churn since -- working tree was docs-only): headless **22/0** + boot unit gate **2025 / 0 failed** re-run and GREEN. The 5-config matrix is cited from the 14:22 evidence (same Debug-True exe exercised here); not rebuilt for a docs-only commit.
- **Q-2026-07-09-003 (overworld bleed-through at the (0,-2000,0) offset) -- automated isolation proof GREEN; PIXEL sign-off PENDING the user.** The round-trip tests assert the overworld grass clears to 0 blades at IN_BATTLE and regenerates on resume; the Battle scene (build 1) sits at world Y=-2000, outside the 200 m grass LOD ring and 1000 m terrain HIGH-LOD streaming, inside an enclosing arena dome; exact resume drift < 0.05 m; 0 arenas after unload. The DEFINITIVE pixel check is the user's live visual sign-off (`... test Zenithmon --filter ZM_BattleMenuWin_Test`, windowed). Documented SINGLE-load + world-state-snapshot fallback stands if the pixel check fails.
- **Shortfalls.md refreshed** (honest gap audit at the gate): sections 1.5/1.6/1.7 updated to "S5 items 1-5 shipped, stage gate pending sign-off"; recorded the S6/S7 deferrals surfaced by the item-5 reviewers -- (a) the battle menu's **Catch item is shown unconditionally, ignoring `m_bCanCatch`** (inert while all S5 battles are wild; S6+ trainer battles MUST gate it on a core-surfaced flag or trip the `SubmitAction` assert / roll an illegal trainer-catch), (b) full-party catch marks-but-drops (box storage = S7), (c) single-LEAD battles only (multi-member + forced-switch-on-faint = S6+), (d) GameState is in-memory only (disk save = S7).
- **Tests that lock it:** none new (docs-only closer). The gate above IS the lock; the 4 item-5 windowed acceptance proofs (`ZM_BattleMenuWin`/`Catch`/`Run`/`Whiteout` + the round-trip/HUD/persistence tests) remain the regression set.
- **Reversibility:** trivial (docs only). On the user's **APPROVED** verdict: append the sign-off entry, tick the S5 stage-gate line, clear the GATE-WAIT marker, S5 COMPLETE -> resume at S6 (Dialogue/menus/NPCs/shops). On **REJECTED**: the rework becomes the current task (likely the SINGLE-load + world-state-snapshot fallback for the offset render if bleed-through is visible).

---

## 2026-07-18 -- ZM-D-110 -- S5 item 5 (SC5): loss -> whiteout + flee-HP-persist (the LAST item-5 gameplay SC)

- **Decision:** A party wipe triggers a WHITEOUT (heal the party full + warp to Dawnmere "TownCenter"); a flee now persists the lead's damaged HP; the fainted-lead "guard" is a documented no-op.
  1. **Winner-classified write-back:** the pure `ZM_ApplyBattleResultToParty(ZM_GameState&, core)` overload routes via a new pure `ZM_ClassifyBattleResult(ZM_SIDE eWinner, bool bLeadFainted) -> {WRITE_BACK_WIN, WHITEOUT, PERSIST_VITALS}`: PLAYER win -> full lead write-back + SC4 catch scan; a PARTY WIPE (ENEMY win, OR a `ZM_SIDE_COUNT` outcome whose lead fainted) -> latch `m_bPendingWhiteout`; a real flee (`ZM_SIDE_COUNT`, lead alive) -> the new pure `ZM_PersistBattleVitalsToRecord` (curHP + per-move curPP + status ONLY -- a flee awards no level/exp/EV progression, fixing the SC3 "flee = free heal"). Loss detection is gated at the director's RESOLVED edge by `m_bWriteBackToLead` (a placeholder/headless battle never whiteouts the real GameState).
  2. **Whiteout execution:** `ZM_GameStateManager::OnUpdate` consumes `m_bPendingWhiteout` -- when set AND the warp machine is IDLE AND `!ZM_BattleTransition::IsTransitionActive()` (the battle round-trip has fully resumed, so this runs POST-resume, never mid-battle) it `HealAllFull()`s the party and `TryQueueWarp(2 /*Dawnmere*/, "TownCenter")`, clearing the flag ONLY on an accepted warp (double-fire guard; `HealAllFull` is idempotent, and the accepted warp re-gates the block). No new component (order stays 112). The healed party survives the whiteout reload (manager is DontDestroyOnLoad; `OnStart` re-seed is first-boot-only).
  3. **Fainted-lead guard -- NONE (documented no-op):** a 0-HP record would battle at 1 HP via the `[1,maxHP]` clamp; the whiteout heal runs BEFORE the player is unfrozen, and win/flee invariants never leave a 0-HP lead re-entering grass. So no `RunSetup` guard.
- **Reviewer (3-lens adversarial): a BLOCKER caught + FIXED, plus nits.** **BLOCKER (fixed this SC):** `GetWinnerSide()` returns `ZM_SIDE_COUNT` for BOTH a successful flee AND a DRAW/double-KO. The initial 3-way branch treated ALL COUNT as flee, so a draw that WIPED the single-lead party (a recoil-KO, or end-of-turn poison/burn/weather chipping both actives to 0) persisted a 0-HP lead and NEVER latched the whiteout -- stranding the player in the overworld with a fainted party (the exact state SC5 prevents). FIXED by introducing `ZM_ClassifyBattleResult` with the `bLeadFainted` discriminator (a real flee always leaves the lead alive -- it resolves in the pre-move phase before an EOT/recoil chip can faint it -- so a COUNT-with-fainted-lead is a wipe -> whiteout), locked by the pure unit `WriteBack_ClassifyDrawVsFleeVsLoss` (the draw case needs no double-KO rig). Nits: the file banner said "Win-only" (UPDATED to the 3-way reality); the whiteout windowed teardown doesn't clear `m_bPendingWhiteout` (only reachable on a FAILING run; self-corrects via the between-tests `ResetGameStateForTests`) -- left as-is, documented.
- **Tests that lock it:** **+6 pure T0 units** (`Vitals_PersistCopiesHpPpStatusNotProgression`; `WriteBack_LossSetsPendingWhiteout` [drives a real L2-vs-L60 loss]; `WriteBack_WinDoesNotSetWhiteout`; `WriteBack_FleeDoesNotSetWhiteoutOrProgress` [drives a real RUN flee]; `Party_HealAllFullFromLossRestoresEveryMember` [status-clear]; `WriteBack_ClassifyDrawVsFleeVsLoss` [the blocker-fix lock]) + the windowed `ZM_BattleMenuWhiteout_Test` (rig L60 enemy for a deterministic loss + pre-damage the lead to 1 HP so the heal is OBSERVABLE -> asserts the party healed 1->full, `m_bPendingWhiteout` cleared, the manager issued the warp load (+1), landed at Dawnmere build 2). Game-only -> `zm-tests.yml` **2019 -> 2025**; engine default (1088) UNCHANGED.
- **Observed gate (dev machine):** build GREEN; boot unit gate **2025 / 0 failed**; headless **22/0**; windowed battle suite **10/0** (incl. `ZM_BattleMenuWhiteout_Test`; flee/loss/win unregressed by the classifier fix) + `ZM_GameStatePersistence` **1/0**. The ~380 `ZM_Battle` goldens stay byte-identical (no engine change).
- **Reversibility:** moderate -- the classifier + the manager consume block + the vitals leaf are additive/localized. **Item 5's gameplay is now COMPLETE (SC1-SC5); SC6 is the consolidated windowed gate + bleed-through screenshot that ticks the item-5 Roadmap box and sets the S5 VISUAL GATE-WAIT (the user's hard stop).**

---

## 2026-07-18 -- ZM-D-109 -- S5 item 5 (SC4): catch -- add a caught wild monster to the party + dex

- **Decision:** A wild monster can be caught via a new **Catch** battle-menu action; on a successful catch it is added to the persistent party and marked in the caught/dex set.
  1. **Menu:** the action root menu grows `Fight(0) / Catch(1) / Run(2)` (`ZM_BattleMenuRootItem`; RUN 1->2 -- a UI ordinal, not save-stable). `MenuConfirm(ACTION_ROOT, CATCH)` submits `{ZM_ACTION_ITEM, m_eItem=ZM_ITEM_CATCHORB}` (the fixed default ball; Q-2026-07-18-001 direct-Catch-button, no Bag UI). New authored button `BattleHUD_ActionCatch` (sort 10003; the 3 root buttons stack vertically).
  2. **Config:** `BuildBattleConfig()` now sets `m_bCanCatch = true` (a menu-availability flag like `m_bCanFlee`; the ITEM action asserts it in the engine). Director-only config -> **zero golden perturbation** (the ~380 `ZM_Battle` goldens build their own configs). `Director_BattleConfigCatchOn` locks it.
  3. **Catch detection + write-back (KEY FACT):** a SUCCESSFUL catch sets the engine winner to **`ZM_SIDE_PLAYER`** (`DoItemAction` `m_eWinner=eSide`). So the SC3 win-only lead write-back FIRES on a catch -- which is CORRECT (it carries the lead's damaged HP out) and INERT on exp (the engine awards no exp on a catch turn: `ResolveTurn`'s `m_bOver` branch skips `AwardExpForNewFaints`). The extended `ZM_ApplyBattleResultToParty(ZM_GameState&, core)` scans the engine stream for `CATCH_RESULT` with `m_iAmount==1` (caught, not broke-free) and calls the new pure `ZM_ApplyCatchToGameState(gs, bCaught, enemyActive)`: `MarkCaught` the species ALWAYS + `Add(ZM_MonsterFromBattleMonster(enemyActive))` IFF `!m_xParty.IsFull()`. Full-party catch marks-but-drops (box storage is S7). Called once on the RESOLVED edge (no director change -- the existing SC3 call site does it).
  4. **Windowed determinism:** a `CATCHORB` catch is never formula-guaranteed (always a 4-shake RNG gate), so the windowed test throws the GUARANTEED `ZM_ITEM_PRIMEORB` (param 255 -> `IsGuaranteedBall` -> capture with ZERO RNG draws) via a test-only seam `ZM_SetCatchBallForTests(ZM_ITEM_ID)` (mirrors `ZM_SetInstantBattlesForTests`; the RUNNING drive substitutes the ball only on an ITEM action; production stays `CATCHORB`, a no-op override; restored in teardown, no skip-leak).
- **Reviewer (focused, catch correctness): NO BLOCKERS.** All 6 scrutiny points verified (right caught monster; the winner==PLAYER path harmless -- no exp double-fire; the ball seam leak-free; the 2->3 menu reorder fully symbolic; config non-perturbing; the windowed test proves the PERSISTENT party grew 1->2 + the species marked via `TryGetGameState`, not just "a battle happened"). **One should-fix is a documented S6 FORWARD DEFERRAL** (the SC4 planner deliberately deferred it to avoid rippling the frozen menu contract; NO S5 trigger exists): the Catch menu item is shown/selectable UNCONDITIONALLY, ignoring `m_bCanCatch`. Inert at S5 (every battle is wild, `m_bCanCatch=true`), but when trainer battles arrive (S6+, `m_bCanCatch=false`) selecting Catch would trip the `SubmitAction` `m_bCanCatch` assert (debug) or roll an illegal trainer-catch (no-assert build). **S6 must gate the Catch item's visibility/selectability on a core-surfaced `m_bCanCatch`.** (Record in Shortfalls.md at the SC6 gate.) Nit: the `CATCH_RESULT` scan's broke-free discrimination is only end-to-end-covered by the guaranteed-catch happy path (the pure leaf's `bCaught` param is tested both ways).
- **Tests that lock it:** **+5 pure T0 units** (`HudMenu_CatchEmitsCatchAction`; `Catch_AddsToPartyAndMarksDex` / `_NotCaughtIsNoOp` / `_FullPartyMarksButDoesNotAdd`; `Director_BattleConfigCatchOn`) + **3 updated menu-count units** (2->3 root reorder) + the windowed `ZM_BattleMenuCatch_Test` (PRIMEORB guaranteed catch -> party 1->2 + Kindlet caught + exact resume). Game-only -> `zm-tests.yml` **2014 -> 2019**; engine default (1088) UNCHANGED.
- **Observed gate (dev machine):** build GREEN; boot unit gate **2019 / 0 failed**; headless **21/0**; windowed battle suite **9/0** (incl. `ZM_BattleMenuCatch_Test`) + `ZM_GameStatePersistence` **1/0**.
- **Reversibility:** moderate -- the menu Catch item + the catch write-back extension are additive; the 3-item root enum + config flag are localized. SC5 (whiteout) extends the write-back adapter for the loss path.

---

## 2026-07-18 -- ZM-D-108 -- S5 item 5 (SC3): battle exp write-back + damaged-HP carry + gapped-moveset menu fix

- **Decision:** A WON wild battle now awards exp/levels to the player's persistent party lead and carries the lead's damaged HP into/out of battle. Three coordinated changes:
  1. **HP carry (the golden-risk change):** appended `u_int m_uCurHP = uZM_CURHP_UNSPECIFIED` (`~0u`) as the LAST field of `ZM_BattleMonsterSpec` (mirrors the shipped `m_uCurExp`/`m_eGender` appends). `ZM_BuildBattleMonster` honors it clamped to `[1, maxHP]` ONLY when `!= sentinel`; `ZM_MonsterToBattleSpec` now sets it from `m_uCurrentHp`. **Zero-perturbation PROVEN:** the boot unit gate ran the ~380 `ZM_Battle` goldens (which compare the EVENT stream) at 2014/0-fail -- every existing spec (wild/tower/breeding/placeholder + all aggregate initializers) leaves the field defaulted -> full HP -> byte-identical.
  2. **Write-back:** new pure `Source/Party/ZM_BattleWriteBack.{h,cpp}` -- `ZM_ApplyBattleResultToParty(party, leadSlot, winner, finalLead)` is WIN-ONLY + guards empty/oob and delegates to SC1's `ZM_ApplyBattleMonsterToRecord`; an adapter overload pulls the winner + final player `Active()` from the resolved core. The director calls it EXACTLY ONCE on the RESOLVED edge (before `RequestBattleEnd`, gated by `m_bWriteBackToLead`); NOT on the wall-clock deadline-abort path.
  3. **RunSetup real-lead read:** reads the party lead via `ZM_GameStateManager::TryGetGameState` -> `ZM_MonsterToBattleSpec(Lead())` (placeholder fallback when no manager -- headless director-core tests stay exp-OFF), and turns exp ON via a LOCAL `ZM_BattleConfig` copy only for a real lead. **`BuildBattleConfig()` itself is UNCHANGED** (still `m_bAwardExp=false`) -- flipping the static would red the shipped `Director_BattleConfigExpOff` unit and turn exp on for every headless core drive (a plan-vs-code trap the planning pass caught).
  4. **Gapped-moveset menu fix** (SC5 forward note): `ZM_UI_BattleHUD::BuildFilledMoveMenu` compacts the moveset (menu cursor k -> raw slot via a table); `MenuConfirm` gains a DEFAULTED trailing raw-slot param (`nullptr` = cursor==slot identity, so every existing 4-arg caller/test is byte-identical). A real party lead with a gapped moveset now shows no blank buttons and submits the correct raw slot.
- **Scope:** single-LEAD, WIN-ONLY persistence (Q-2026-07-18-001). Loss/flee do NOT persist (leaf early-out on `winner != PLAYER`); catch is SC4; whiteout is SC5.
- **Reviewer (3-lens adversarial): NO BLOCKERS.** Battle-flow lens fully clean; golden zero-perturbation confirmed. One finding FIXED this SC: the gapped-moveset unit only covered the interior-gap `{A,_,B,_}` pattern -> extended `HudMenu_GappedMovesetCompaction` with the LEADING-gap `{_,A,_,B}` (raw slots 1,3) + all-empty (n==0) fixtures. **Two findings are SC5 FORWARD NOTES** (scope-boundary, within SC3's win-only design): (1) "flee = free heal" -- HP is carried INTO battle but written back only on a WIN, so a successful flee discards the lead's in-battle HP damage; SC5 (loss/flee + HP persistence) should make HP persist through a flee even though exp does not. (2) A fainted lead (record HP 0) would battle at 1 HP (the `[1,maxHP]` clamp) -- SC5's whiteout path should add a fainted-lead guard.
- **Tests that lock it:** **+5 pure T0 units** (`WriteBack_WinPersistsProgression` / `_NonWinIsNoOp` / `_GuardsEmptyAndOutOfRange` / `SpecCurHP_RoundTripAndClamp` [clamp 0->1, 999999->maxHP, sentinel->full] / `HudMenu_GappedMovesetCompaction` [now interior + leading + empty]) + the windowed `ZM_BattleMenuWin_Test` EXTENDED to capture the persistent lead's exp/level before the encounter and assert `expAfter > expBefore` after resume (proving the win awarded exp to the real GameState lead; deterministic via the between-tests reseed). Game-only -> `zm-tests.yml` **2009 -> 2014**; engine default (1088) UNCHANGED.
- **Observed gate (dev machine):** build GREEN; boot unit gate **2014 / 0 failed** (the ~380 `ZM_Battle` goldens byte-identical); headless **20/0**; windowed battle suite **8/0** (incl. the exp-rose win proof) + `ZM_GameStatePersistence` **1/0**.
- **Reversibility:** moderate -- the write-back module + the director/menu edits are additive; the `m_uCurHP` spec field is a defaulted append. SC4 (catch) + SC5 (whiteout) extend the write-back adapter.

---

## 2026-07-18 -- ZM-D-107 -- S5 item 5 (SC2): `ZM_GameStateManager` owns the persistent in-memory `ZM_GameState`

- **Decision:** Wire the SC1 party model into the persistent `ZM_GameStateManager` (order 104, `DontDestroyOnLoad`): it now owns one `ZM_GameState m_xGameState` BY VALUE, seeds it once via `ZM_MakeStarterGameState()` (Fernfawn L5 lead + Fernfawn caught) at first boot, and exposes the STATIC cross-scene accessor `static bool ZM_GameStateManager::TryGetGameState(ZM_GameState*& pxOut)` (resolves the unique manager fresh each call via the existing singleton resolver, hands back the mutable persistent instance, fail-closed when absent). SC3-SC5 (the battle write-back) read/write the party through this. NO battle behavior change yet (SC3 flips `m_bAwardExp` + reads the real lead).
- **Seed-once semantics (the critical property, reviewer-verified):** the seed sits inside the authoritative-singleton branch of `OnStart` -- after the two early-return duplicate-retirement guards, after `s_xSingletonEntityID` is claimed, and BEFORE the `DontDestroyOnLoad()` move -- so it runs EXACTLY once for the surviving singleton, rides the `DontDestroyOnLoad` relocation (EntityID preserved), is idempotent on re-entrant `OnStart` (the `s_xSingletonEntityID==own` guard returns early), and a re-authored FrontEnd duplicate self-`Destroy()`s BEFORE reaching the seed (never clobbering a live party). `ReadFromDataStream` is untouched (no disk save -- Q-2026-07-18-001 / S7 owns disk) and runs before `OnStart`, so the seed always wins. No ordinary SINGLE scene load re-seeds.
- **Test isolation:** because the manager persists across batched tests, `static ResetGameStateForTests()` re-seeds the starter THROUGH the pointer (stable address); it is wired into the existing `RegisterBetweenTestsHook` (already `#ifdef ZENITH_INPUT_SIMULATOR`) so a caught/levelled party can't leak into the next test. The harness fires the hook only after scene-0 reload + settle + destruction-drain, so count==1 holds at hook time.
- **Reviewer (focused, shared-component): NO BLOCKERS.** Seed-once (survives DontDestroyOnLoad / duplicates retire first / ReadFromDataStream no-fight / no mid-game re-seed), `TryGetGameState` soundness, the between-tests reseed timing, and no warp/transition/player-park regression all verified. Two NITS documented as FORWARD NOTES (reviewer: "no changes required to ship"): (1) `ResetGameStateForTests` resolves via the count==1 `TryGetGameState` query rather than `s_xSingletonEntityID` directly like its sibling `ResetRuntimeStateForTests` -- correct today (the harness guarantees count==1 at hook time) but a latent fragility if a future harness change ever fired the hook with a transient duplicate present; resolve via `s_xSingletonEntityID` for symmetry when convenient; (2) the persistence test's clean-starter assertions only actively prove the reseed if an earlier test dirtied the state (none marks KINDLET today).
- **Tests that lock it:** windowed `ZM_GameStatePersistence_Test` (`Tests/ZM_AutoTests_GameState.cpp`): boots Dawnmere, resolves the seed via `TryGetGameState` (party Count()==1, lead Fernfawn L5, Fernfawn caught), MUTATES the caught-set (marks Kindlet), loads a DIFFERENT scene (PlayerHome build 40), re-resolves fresh, and asserts the SAME instance survived (still Fernfawn L5 + the Kindlet mutation persisted) -- a genuine DontDestroyOnLoad proof. NO new T0 units (the pure `ZM_MakeStarterGameState` is covered by SC1; the manager accessor is ECS-coupled and proven by this windowed test), so the boot unit baseline stays **2009** and `zm-tests.yml` is UNCHANGED.
- **Observed gate (dev machine):** build GREEN (Vulkan Debug True); boot unit gate **2009 / 0 failed**; headless **20/0** (the new windowed test registers + auto-skips headless); windowed `ZM_GameStatePersistence_Test` **1/0** + the battle suite **8/0** (no manager regression).
- **Reversibility:** easy -- an additive member + a once-seed line + a static accessor + a test-reset hook call; nothing serializes it (S7 owns disk).

---

## 2026-07-18 -- ZM-D-106 -- S5 item 5 (SC1): persistent player-party data model (`Source/Party/`) -- pure, in-memory

- **Decision:** Add the FOUNDATION for item 5 -- a PURE, in-memory persistent party model under `Source/Party/` (no ECS, no scene, no graphics, no RNG, no DataStream): `ZM_Monster` (the persistent record -- species/level/curExp/IVs/EVs/nature/ability/status/gender/flags + `ZM_MoveSlot m_axMoves[]` [id+curPP+maxPP] + curHP), `ZM_Party` (fixed C array, <= `uZM_MAX_PARTY_SIZE`=6), `ZM_GameState` (party + a `ZM_SpeciesSet` caught bitset + `m_bPendingWhiteout`), and the load-bearing PURE conversions SC2-SC5 build on: `ZM_MonsterToBattleSpec` (record -> `ZM_BattleMonsterSpec`), `ZM_ApplyBattleMonsterToRecord` (post-battle write-back of HP/exp/level/moves+PP/status into the lead, identity untouched), `ZM_MonsterFromBattleMonster` (the caught-monster factory), `ZM_BuildMonsterRecord`(species,level) + `ZM_MakeStarterGameState`() (Fernfawn L5, caught-marked). Byte-consistent with `ZM_BuildBattleMonster` so the record<->battle round-trip is exact. Self-contained NEW files only -- no existing file edited.
- **Scope (Q-2026-07-18-001):** in-memory ONLY -- NO `ZM_SaveSchema` serialization (S7 owns disk + migration); the ability is stored as a concrete `ZM_ABILITY_ID` (lossless round-trip; S7 can derive the 0/1 slot at serialize time). `ZM_MonsterToBattleSpec` does NOT yet carry damaged curHP / spent PP -- battle builds start full; the damaged-HP carry-into-battle is SC3 (it appends `m_uCurHP` to the spec). SC1 is needed under ALL five item-5 scope rulings, so it landed first regardless.
- **Reviewer (focused conversion-correctness pass): NO BLOCKERS.** Conversions verified field-by-field (identity preserved on write-back; no max-HP jump in shipped flows); party invariants loop only `[0, m_uCount)` (no stale-tail reads); `ZM_SpeciesSet` bounds correctly reject `ZM_SPECIES_NONE`; genuinely pure; no uninitialized POD members. Two should-fixes APPLIED this SC: (1) `ZM_Monster::GetMaxHP()` now NORMALIZES a local EV copy before the HP calc (symmetric with `ZM_BuildBattleMonster`), so an over-cap record (a corrupt S7 save or an SC3+ hand-written EV path) can't make max HP diverge between the record and the in-battle monster (an HP jump); (2) the write-back identity test now perturbs the battle monster's `m_auIV[0]` to a DIFFERENT value before Apply and asserts the record retained its original, so the identity-preservation lock (the SC3 lead-persist contract) actually distinguishes "not copied" from "copied identical". Deferred nits: `Lead()` on an empty party returns a default record (documented; the party always holds the lead in practice); deterministic `MALE` gender for genderless species (battle-inert; a real roll can move to wild-gen later).
- **Tests that lock it:** **+14 pure `ZM_Party` T0 units** (`Tests/ZM_Tests_Party.cpp`): validity/max-HP/heal; party add-6-reject-7th, lead-skips-fainted, all-fainted (on a partial party), heal-all; caught-set mark/query/count (idempotent + NONE-ignored); starter shape; record<->battle round-trip exact (field-by-field); from-battle-monster faithful (damaged HP/PP/status carried); write-back of HP/exp/level/PP with identity preserved; determinism. Game-only -> `zm-tests.yml` **1995 -> 2009**; engine default (1088) UNCHANGED.
- **Observed gate (dev machine):** build GREEN (Vulkan Debug True); boot unit gate **2009 ran / 2008 passed / 0 failed / 1 skipped**; headless **19/0** (no windowed added -- SC1 is a pure model). No re-bake (no scene/asset touched).
- **Reversibility:** trivial -- self-contained additive files; nothing consumes them until SC2 wires the manager.

---

## 2026-07-18 -- ZM-D-105 -- S5 item 4 (SC6): item-4 CLOSER -- the windowed win gate is satisfied by the SC5-shipped tests; item 4 ticked

- **Decision:** Close S5 item 4 (`ZM_BattleDirector` + `ZM_UI_BattleHUD` + engine E3 typewriter) by ticking Roadmap line 86. The plan's SC6 named a new `Tests/ZM_AutoTests_BattleWin.cpp` with `ZM_BattleWin_Test` + `ZM_BattleRun_Test`; those were **already shipped in SC5** as `ZM_BattleMenuWin_Test` + `ZM_BattleMenuRun_Test` (`Tests/ZM_AutoTests_BattleMenu.cpp`) -- the interactive-menu proof (SC5) and the end-to-end win gate (SC6) were folded into one pair. So SC6 is a **docs-only closer**: NO new source, NO new test file (a `ZM_AutoTests_BattleWin.cpp` would duplicate shipped coverage).
- **Every SC6 acceptance bullet maps to a shipped, passing windowed assertion** (verified by a read-only planning pass against `Build/artifacts/zm_s5_item4_plan/plan.md:62-66`): graphics + instant-on + RequestSkip + fixed dt 1/30 + no ScopedTestIsolation + walk-grass + forced encounter + drive HUD Fight->move to a player win (`GetWinner()==ZM_SIDE_PLAYER` + an HP bar reached 0) + single `RequestBattleEnd` (director-driven: `completedAfter==1 && abortedAfter==0`) + exact resume (build index 2, Battle unloaded, movement re-enabled, drift < 0.05 m, grass restored) -- all in `ZM_BattleMenuWin_Test`; the `ZM_BattleRun_Test` smoke = `ZM_BattleMenuRun_Test` (a `FLEE` event + `GetWinner()==ZM_SIDE_COUNT` + the same resume block). `ZM_BattleHUD_Test` + `ZM_BattleDirectorRoundTrip_Test` cover the HUD-presentation + director-round-trip halves of the item-4 line.
- **Scope kept clean:** item 4 proves the PRESENTED, PLAYABLE battle only. Exp/catch/faint->whiteout/GameState write-back stay OUT (exp OFF, Fight+Run only, no Bag/ball) -- those are item 5 (Roadmap line 87). EXP/LEVEL/CATCH events are mapped for totality so item 5 flips `m_bAwardExp`/`m_bCanCatch` with zero director change.
- **No re-gate:** SC6 changes only docs; the SC5 gate stands as the evidence (build green; boot units 1995/0-fail; headless 19/0; windowed 8/0). No baseline change.
- **NOT a visual gate:** the S5 STAGE gate (the visual hard-stop needing the user's sign-off) sits AFTER item 5, so ticking item 4 does NOT set a GATE-WAIT. The loop hands straight off to item 5.
- **Reversibility:** trivial -- a Roadmap checkbox + this log entry.

---

## 2026-07-18 -- ZM-D-104 -- S5 item 4 (SC5): interactive Fight/Run battle menu (the first player-driven battle input)

- **Decision:** Replace the director's hard-coded `SubmitPlayerAction({ZM_ACTION_MOVE, slot 0})` with a player-driven menu on `ZM_UI_BattleHUD` (extended, owned BY VALUE in `ZM_BattleDirector` -- Option-B parity, NO new ECS component). A pure state machine `ACTION_ROOT{Fight,Run} -> MOVE_SELECT{0..3}` (static `MenuItemCount`/`MenuMoveCursor` [clamp, no wrap]/`MenuConfirm`/`MenuCancel`, T0-unit-tested) driven by `ZM_InputActions` EDGE readers (`ReadMenuVertical` Up/Down, `ReadConfirmPressed` Enter/Space, `ReadCancelPressed` Esc/Backspace), processed nav->confirm->cancel each frame. Fight -> a move submenu (the active's up-to-4 moves, selectable iff move != NONE && PP > 0) -> `SubmitPlayerAction({MOVE, slot})`. 7 new authored HUD elements (menu panel + Fight/Run + 4 move buttons) at sort order **10003** (> the HUD's 10002 > BattleFade's 10001), authored hidden, re-resolved each frame, destroyed with the Battle scene (no cross-battle leak). Enemy stays GREEDY, exp OFF, **Fight+Run ONLY** (Q-2026-07-17-002(c); Bag/ball/catch + party/switch defer to item 5).
- **Run routes THROUGH the engine (key design call):** the engine already supports `ZM_ACTION_RUN` end-to-end (`ZM_BattleEngine::DoRunAction` -> `ZM_RollFlee` -> `ZM_BATTLE_EVENT_FLEE`/`FLEE_FAILED` -> `m_bOver`). So Run submits `{ZM_ACTION_RUN}` and a SUCCESSFUL flee reaches `ZM_DIRECTOR_OVER` and fires the SAME existing `ShouldRequestEnd()` latch -- there is **ONE** `RequestBattleEnd()` call site for both KO and flee (no second end route, dissolving the coexistence risk the SC3 nit worried about). `BuildBattleConfig()` now sets `m_bCanFlee = true` (RUN asserts otherwise; separate config, no golden impact). A FAILED flee just continues the turn and re-opens the menu.
- **SC3 reviewer nit resolved:** both `RequestBattleEnd()` call sites now capture the `bool` return and latch (`m_bEndRequested = true; m_ePhase = ZM_BD_RESOLVED`) REGARDLESS, logging a `LOG_CATEGORY_GAMEPLAY` line on a `false` return so any future double-end (e.g. an external caller racing us) is a single logged no-op instead of a per-frame retry.
- **Hazard H0 (removing auto-submit breaks the delegating tests):** `ZM_BattleDirectorRoundTrip_Test` + `ZM_BattleHUD_Test` relied on the director auto-submitting to resolve their AI-vs-AI battles. Both were given a per-frame `Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER)` drive (default cursor Fight -> Fight->move0 each turn) so they still resolve to a KO; their assertions are unchanged.
- **Reviewer (3-lens adversarial): NO BLOCKERS.** All findings are NITS, each explicitly NOT triggerable in SC5's scope, recorded here as FORWARD NOTES for item 5 (do NOT re-gate SC5 for them; the decisive player-driven-resolution proof already lives in the two new menu tests):
  1. The move menu couples cursor-index to raw move-SLOT-index (correct while movesets pack contiguously from slot 0, which the placeholder player always does). **Item 5 must compact to a filled-slot index table** before real party members with gapped movesets arrive, or a gapped active shows a blank unselectable button and hides/mis-targets the real move.
  2. An all-0-PP active makes Fight->MOVE_SELECT a silent dead-end (no Struggle, no feedback line). Not a softlock (Run + the 30 s wall-clock deadline backstop it); Struggle stays a pre-existing engine gap (Q-2026-07-12-003).
  3. The two H0-edited tests prove resolution only INDIRECTLY (the resume deadline 600 frames firing before the director's 30 s softlock ~900 frames), not via a direct `GetCore().IsOver()` check -- currently safe and nothing is masked, but a future raise of the resume deadline could let a broken drive pass; the two NEW menu tests already assert `GetWinner()` directly. A direct `IsOver()` latch on the two H0 tests is a good item-5-era hardening.
  4. On the first AWAIT_INPUT frame `UpdateMenu` flips HIDDEN->ACTION_ROOT then reads confirm the same frame (the ENTER-drive relies on this). Revisit if an ENTER-to-advance-text affordance is later added.
- **Tests that lock it:** **+7 pure T0 `ZM_BattleHUD` menu units** (`HudMenu_*`: item counts, cursor clamp-no-wrap, Fight->OPEN_MOVES, Run->SUBMIT RUN, move->SUBMIT slot, unselectable-move->NONE, cancel->root) + **2 new windowed tests** (`ZM_BattleMenuWin_Test`: player Fight->move0 to a KO, asserts `GetCore().GetWinner()==ZM_SIDE_PLAYER` + an HP bar reached 0; `ZM_BattleMenuRun_Test`: player Down->Run->Enter, asserts a `ZM_BATTLE_EVENT_FLEE` in the engine stream + `GetWinner()==ZM_SIDE_COUNT`; both with the exact-resume invariants). Game-only -> `zm-tests.yml` **1988 -> 1995**; engine default (1088) UNCHANGED.
- **Observed gate (dev machine):** build GREEN (Vulkan Debug True); boot unit gate **1995 ran / 1994 passed / 0 failed / 1 skipped**; headless **19/0**; windowed **8/0** with ZERO skips (`ZM_BattleMenuWin_Test` + `ZM_BattleMenuRun_Test` + the two H0-fixed delegating tests + the 4 coexistence tests). Coexistence: the 7 new HUD elements on the `BattleDirector` entity add no arena children / scene loads / arenas, so the SC3 invariants hold.
- **Reversibility:** easy -- the menu is additive on `ZM_UI_BattleHUD` + the director's RUNNING block + `ZM_InputActions` + the Battle-scene UI author step + the config flag + the two test drives. SC6 (the windowed win gate that ticks the item-4 box) builds on it.

---

## 2026-07-18 -- ZM-D-103 -- S5 item 4 (SC4): `ZM_UI_BattleHUD` -- the first visible battle UI (text log via E3 typewriter + HP panels)

- **Decision:** Add `Source/UI/ZM_UI_BattleHUD.{h,cpp}` -- a small NON-ECS presentation class owned BY VALUE inside `ZM_BattleDirector` (`m_xHud`), sibling of `ZM_FadeOverlay` (**seam Option B**, not a new ECS component/order/editor mirror). The director hands the HUD its own deep-owned `ZM_BattleDirectorCore` (read-only) + entity each RUNNING frame; the HUD renders a `Zenith_UIComponent` (log `UIText` + 2 HP-panel `UIText` + 2 HP-bar `UIRect`) authored onto the `BattleDirector` entity at bake time by the tools-only `ZM_ConfigureBattleHUD` (all 5 elements sort order **10002** > BattleFade's 10001, authored hidden). `Setup` reveals + seeds after `PlaceCreatureModels`; `Update` (after `m_xCore.Tick`) shows the latest presented log line + refreshes both HP panels/bars; `Hide` fires immediately BEFORE both `RequestBattleEnd()` calls (so the end-fade never shows HUD over black). Still AI-vs-AI, NO interaction (SC5).
- **Instant-mode fix (found by the SC4 windowed gate, then diagnosed from the core's cursor logic):** under `zm_instant_battles` a single `ZM_BattleDirectorCore::Tick` drains the WHOLE battle's presentation ops, so `CurrentEvent()`/`CurrentOp()` were already null when the HUD sampled after `Tick` -> the log stayed empty (the first windowed run FAILED, `resolvedCaptured=false`). Fix: added an additive inline accessor `ZM_BattleDirectorCore::PresentedEventCount()` (the presented-event boundary: `cursor+1` while PLAYING_EVENTS, else `cursor`) and rewrote `ZM_UI_BattleHUD::Update` to scan `[0, PresentedEventCount())` BACKWARD for the latest text-carrying event (via `ZM_MapEventToOp(...).m_bCarriesText`) and show that line. This surfaces the final line (winner banner / faint) under instant AND reveals gradually op-by-op under timed. Purely additive to the core (no existing caller's behaviour changes; SC2/SC3 goldens untouched).
- **Formatting is pure + testable:** `FormatBattleLogLine` (a TOTAL switch over every `ZM_BATTLE_EVENT` kind; framing -> `""`, three distinct `BATTLE_END` winner banners, `"Foe "` prefix for enemy-side subjects), `FormatHpPanel`, `ComputeHpFraction` (div-by-zero guarded), `ComputeVisibleGlyphCount` (instant -> full; else `floor(elapsed * 45/s)` clamped) are all static with no scene/graphics.
- **Reviewer (3-lens adversarial pass): NO BLOCKERS.** It surfaced two real test-coverage gaps in the fix, both CLOSED this SC: (1) the `cursor+1` (timed) branch of `PresentedEventCount` -- the DEFAULT shipping mode -- was unexercised (all tests run instant), now locked by `Director_PresentedEventCountIncludesCurrentOpWhilePresenting`; (2) the windowed HUD test could not distinguish player from enemy panels (both FERNFAWN L5), now the enemy is forced to FERNFAWN **L7** and the gate asserts the enemy panel shows `Lv7`, so a PLAYER/ENEMY side-swap in the HP refresh fails instead of passing. Plus the nit invariant lock `Format_CarriesTextImpliesNonEmpty` (every text-carrying kind formats non-empty -- the exact class of bug the instant fix addressed). Deferred nits: the `strLine != m_strShownLine` latch won't re-type two ADJACENT identical lines (unreachable in the SC4 1v1 stream; revisit when multi-hit/multi-mon spam adjacent identical constant lines); the `carriesText`-vs-format drift is now partly locked by `Format_CarriesTextImpliesNonEmpty`.
- **Tests that lock it:** **+21 pure T0 game units** (18 `ZM_BattleHUD` formatter/HP/reveal/totality/carries-text + 3 `ZM_BattleDirector`: `DeriveBattleSeed` [from SC3's hardening carried here in count], `PresentedEventCountReachesEndAtOver`, `PresentedEventCountIncludesCurrentOpWhilePresenting`) + the windowed `ZM_BattleHUD_Test` (instant ON; RequestSkip on absent bakes) asserting the HUD log/HP widgets populate + fully reveal + sort order > 10001, with the SC3 director-ended round-trip invariants intact. Game-only -> `zm-tests.yml` **1967 -> 1988**; engine default (`run_unit_gate.ps1` 1088) UNCHANGED. (Net of SC4 over the SC3 baseline of 1967: +18 HUD + 1 PresentedEventCount-at-over + 1 PresentedEventCount-mid + 1 carries-text = +21 -> 1988.)
- **Observed gate (dev machine):** build GREEN (Vulkan Debug True); boot unit gate **1988 ran / 1987 passed / 0 failed / 1 skipped**; headless **17/0**; windowed **6/0** with ZERO skips (`ZM_BattleHUD_Test` + the 5 coexistence tests all genuinely ran against real baked assets). Coexistence verified: authoring the HUD `Zenith_UIComponent` on the `BattleDirector` entity adds no arena children / scene loads / arenas, so the SC3 invariants (`uCHILD_COUNT`==9, `issuedLoads`==1, `arenaCountAfter`==0) all still hold.
- **Reversibility:** easy -- a self-contained class + its tests + the director hook calls + the Battle-scene UI author step + the additive core accessor + the `zm-tests.yml` bump. SC5 (interactive Fight/Run) builds ON it.

---

## 2026-07-18 -- ZM-D-102 -- S5 item 4 (SC3): `ZM_BattleDirector` (order 111) -- drives the core in the Battle scene, places models, single-call `RequestBattleEnd`

- **Decision:** Add `Components/ZM_BattleDirector.{h,cpp}` -- the ECS component (serialization order 111, next-free after 110=`ZM_BattleTransition`) that binds the pure SC2 `ZM_BattleDirectorCore` into the additively-loaded Battle scene. It is authored onto its own `BattleDirector` entity in the Battle-scene recipe (right after `BattleArena`, `AddStep_CreateEntity`+`SetEntityTransient(false)`+`AddComponent`). Each `OnUpdate` it re-resolves the persistent `ZM_BattleTransition` singleton FRESH (pool swap-and-pop -> never cached); once the transition reaches `ZM_BATTLE_TRANSITION_IN_BATTLE` it one-shot reads the encounter payload (`GetBattleSpecies`/`GetBattleLevel`), `Begin`s the deep-owned core with a deterministic placeholder player + `ZM_BuildWildEnemySpec` enemy, places two creature models on the arena platforms, drives the AI-vs-AI battle turn by turn, and calls `ZM_BattleTransition::RequestBattleEnd()` **exactly once** when the core reaches OVER. NO HUD (SC4).
- **Phase machine:** `ZM_BATTLE_DIRECTOR_PHASE { WAIT_FOR_IN_BATTLE, SETUP, RUNNING, RESOLVED, DONE }`. Setup is one-shot (`ShouldRunSetup` = `WAIT_FOR_IN_BATTLE && bInBattle && !bAlreadySetUp`); the end is latched (`ShouldRequestEndNow` = `RUNNING && coreShouldEnd && !m_bEndRequested`) so the sole IN_BATTLE exit fires once; a 30 s WALL-CLOCK deadline requests the end anyway if the core ever wedges (unreachable under `zm_instant_battles`). `RESOLVED`->`DONE` once the transition leaves IN_BATTLE. Both predicates are extracted as PURE statics for headless unit coverage.
- **Scoping (Q-2026-07-17-002, acting on best-guess):** (a) player team = a director-owned deterministic PLACEHOLDER (`ZM_BuildWildEnemySpec(ZM_SPECIES_FERNFAWN, 5u)`), discarded on unload -- no persistent GameState is built (that is item 5); (b) exp OFF (`BuildBattleConfig` sets `m_bIsWild=true, m_bAwardExp=false`); (d) wild enemy AI = GREEDY. Model placement is BEST-EFFORT: a missing arena / unbaked creature bundle silently skips and NEVER aborts the battle.
- **Coexistence (verified empirically at the gate):** authoring a live director into the Battle scene does NOT perturb the 4 shipped windowed battle tests. `ZM_BattleRoundTrip_Test` ends the transition itself on the first IN_BATTLE frame with instant OFF (the between-tests hook resets it), so the director's timed battle never reaches OVER before unload; the director adds only Transform+Model entities (not arena children -> `uCHILD_COUNT`==9 holds), issues NO scene loads (`issuedLoads`==1 holds), and creates NO arenas (`arenaCountAfter`==0 holds). All four re-ran GREEN with the director authored.
- **Tests that lock it:** **+5 pure T0 `ZM_BattleDirector` units** (placeholder determinism, exp-off config, one-shot setup latch, single-call end latch driven by a real core to OVER, and `DeriveBattleSeed` determinism/input-sensitivity) + **1 windowed `ZM_BattleDirectorRoundTrip_Test`** (instant ON; RequestSkip on absent Dawnmere/`Battle.zscen`/prop bakes) that proves the DIRECTOR (never the test) ended the AI-vs-AI battle (`completed==1 && aborted==0`) with the item-3 resume invariants intact (drift < 0.05 m, grass 0->restored, Battle unloaded, movement re-enabled). Game-only units, so ONLY `zm-tests.yml` bumped **1962 -> 1967**; the engine default (`run_unit_gate.ps1` 1088) is UNCHANGED.
- **Observed gate (dev machine):** build GREEN (Vulkan Debug True); boot unit gate **1967 ran / 1966 passed / 0 failed / 1 skipped**; headless **16/0**; windowed **5/0** with ZERO skips (`ZM_BattleDirectorRoundTrip_Test` + the 4 coexistence tests all genuinely ran against real baked assets). Reviewer: **no blockers** (state machine, exactly-once latch, determinism, move-semantics pool-safety, best-effort placement, and scope all confirmed).
- **Reviewer nits carried forward (not fixed in SC3, by design):** (1) `OnUpdate` ignores `RequestBattleEnd()`'s bool return -- provably safe now (the director is the sole in-Battle caller and the transition is always still IN_BATTLE when it calls), but becomes relevant when SC5 adds an interactive Run/flee path that could end the battle by a second route; SC4/SC5 should branch on the return then. (2) `Write`/`ReadFromDataStream` have no dedicated round-trip unit -- the payload is a bare version stamp with reset-first semantics and the read path is exercised on every Battle-scene load in the passing windowed gate, and the component is entity-coupled (awkward to construct in a pure T0 unit), so the windowed gate is its coverage.
- **Move semantics (documented in the header):** copy deleted; move ctor `= default` **without** `noexcept` -- `ZM_BattleDirectorCore` deep-owns a non-noexcept-movable `Zenith_Vector`, so a `noexcept = default` move op would be DEFINED-AS-DELETED under C++20 and break the component pool (which only ever move-CONSTRUCTS). `m_xParentEntity` stored by value (lightweight handle) like the sibling battle components.
- **Reversibility:** easy -- a self-contained component + its tests + 4 additive `Zenithmon.cpp` lines + the Battle-scene author step; deleting it reverts order 111 and the `zm-tests.yml` baseline. SC4 (`ZM_UI_BattleHUD`) builds ON it.

---

## 2026-07-17 -- ZM-D-101 -- S5 item 4 (SC2): `ZM_BattleDirectorCore` -- the pure headless battle-presenter driver

- **Decision:** Add `Source/Battle/ZM_BattleDirectorCore.{h,cpp}` -- the pure, headless heart of the battle presenter. It deep-owns a `ZM_BattleEngine`, drives it turn by turn, and maps the engine's append-only `ZM_BattleEvent` stream into timed `ZM_BattlePresentationOp`s for a later ECS/UI layer (SC3/SC4). NO ECS, NO graphics, NO scene -- the driver only. The battle logic already exists and is fully tested (S2); this is the presentation-driver seam.
- **State machine:** `ZM_DIRECTOR_STATE { NOT_STARTED, AWAIT_INPUT, PLAYING_EVENTS, OVER }`. `Begin` builds the engine and enters PLAYING_EVENTS to present the intro range `[0, GetEventCount())` (BATTLE_BEGIN + both SWITCH_INs). `SubmitPlayerAction` (valid ONLY in AWAIT_INPUT -- `Zenith_Assert` precondition) picks the enemy action, submits both sides, `ResolveTurn()`, snapshots the new event range `[prevCount, newCount)`, and re-enters PLAYING_EVENTS. `Tick(dt)` drains ops as wall-clock accrues; when the range is exhausted -> OVER if the engine reports over, else AWAIT_INPUT. `ShouldRequestEnd()` latches at OVER (item 3's `RequestBattleEnd()` is called by the SC3 component off this).
- **`ZM_MapEventToOp` is TOTAL** over all 39 `ZM_BATTLE_EVENT` kinds via an explicit-case `switch` (framing kinds BATTLE_BEGIN/TURN_BEGIN/TURN_END/EVOLUTION_QUEUED -> `ZM_POP_NONE`; MOVE_USED -> ANIM; damage/heal/drain/recoil/status-dmg/weather-dmg -> HP_TWEEN; SWITCH_IN -> MODEL_REVEAL; FAINT -> FAINT_FALL; catch -> BALL; exp -> EXP_TWEEN; the rest -> TEXT). The `default` is reachable only by an out-of-range value. Locked by a totality test looping `[0, COUNT)`.
- **`zm_instant_battles`:** a process-lifetime flag (ALL configs) behind `ZM_InstantBattlesEnabled()`/`ZM_SetInstantBattlesForTests()`/`ZM_InstantBattlesRef()`. `Tick` collapses every op duration to 0 when set, so a whole turn/intro drains in ONE `Tick` (even at `dt==0`) -> deterministic, frame-timing-free tests. A tools-only DebugVariable `Zenithmon/Battle/zm_instant_battles` binds the ref; the between-tests hook resets it to false so a test can't leak instant mode into a later test.
- **AI non-perturbation (ZM-D-032/033):** the director drives the enemy through `ZM_ChooseAction` with its OWN `ZM_BattleRNG`, seeded from `ZM_DeriveAiRngSeed(ulSeed) = ulSeed ^ 0x9E3779B97F4A7C15` -- a distinct stream that never coincides with the battle RNG. Proven byte-identical to a hand drive of the raw engine (same seed, same MOVE-slot-0 picks, an identically-seeded AI rng) by the `AiRngUnperturbing_DirectorDriveMatchesManualDrive` test.
- **`ZM_BuildWildEnemySpec(species, level)`** mirrors the tower's `g_FillTowerMoves`: collect learnset entries with level <= the passed level in learn order, keep the highest-level up-to-four; IVs 31 / EVs 0 / nature FERAL / ability NONE.
- **Tests that lock it:** **+9 pure `ZM_BattleDirector` units** (map-totality, text/HP classification, instant-drains-in-one-tick, timed-advances-gradually, the AWAIT_INPUT precondition, resolution-latches-request-end, wild-spec learnset moves, and the non-perturbation equivalence). Game-only (no engine units), so ONLY `zm-tests.yml` bumped **1953 -> 1962**; the engine default (`run_unit_gate.ps1` 1088) is UNCHANGED.
- **Observed gate:** build GREEN (Vulkan Debug True); boot unit gate **1962 / 0 failed**; headless **15/0**.
- **Reversibility:** easy -- a self-contained pure class + its tests; SC3 (the ECS component) consumes it but nothing else does yet.

---

## 2026-07-17 -- ZM-D-100 -- S5 item 4 (SC1): engine E3 -- typewriter reveal on `Zenith_UIText`

- **Decision:** Add an additive, back-compatible "visible glyph count" property to the engine widget `Zenith_UI::Zenith_UIText`: `SetVisibleGlyphCount(int)`/`GetVisibleGlyphCount()` (raw store, no clamp), `GetTotalGlyphCount()`, and the pure static `ClipToVisibleGlyphs(display, iVisible)`. A widget can reveal the first N glyphs of its display string (battle text + dialogue). Default `m_iVisibleGlyphCount = -1` = fully revealed, so every existing widget renders byte-identically.
- **Belongs in the widget, not per-game:** every dialogue + battle-text line needs a typewriter reveal; a per-game hack (clipping the string before `SetText`) would reflow the box as glyphs appear. The property clips ONLY the submitted string inside `Render()`; the layout metrics (`GetTextWidth/GetTextHeight`), the multi-line-path decision, alignment X, and the off-screen/alignment warnings all still use the FULL display string, so the reveal does not reflow -- the box reserves the full text's space and glyphs appear in place. Glyphs are counted over the post-wrap DISPLAY string (spaces and `\n` each count), consistent with `GetTotalGlyphCount`.
- **Zero-copy on the default path (reviewer nit, fixed pre-commit):** `Render()` runs for every visible text widget every frame; a naive `ClipToVisibleGlyphs` (returns by value) would heap-allocate a string copy every frame even when unused. `Render()` now gates on `bReveal` (a reveal limit is active AND < length) -- the fully-revealed path binds `strVisible` straight to `strDisplay` with no allocation; only an active partial reveal calls the helper.
- **Serialization v2 -> v3, tolerant read:** `WriteToDataStream` appends the field; `ReadFromDataStream` accepts version 2 OR 3 (was an exact-equality assert), defaults pre-v3 blobs to -1 (fully revealed), and reads the field only when `uVersion >= 3`. So baked v2 `.zscen` files still load. Locked by a hand-built-v2-blob test that reproduces the exact v2 wire layout.
- **Tests that lock it:** **+7 engine `UIText` units** (`Zenith/UI/Zenith_UIText.Tests.inl`, hosted unconditionally at the .cpp tail like every `.Tests.inl`): clip boundaries, space/newline glyph counting, default-revealed, raw setter round-trip, total-count (no-wrap), v3 serialization round-trip, and the v2-blob back-compat proof. All pure/headless (never call `Render()`). Engine units run in every game's boot, so BOTH baselines bumped +7: `Tools/run_unit_gate.ps1` default **1081 -> 1088** and `.github/workflows/zm-tests.yml` **1946 -> 1953**.
- **Observed gate (dev machine):** builds GREEN -- Zenithmon Vulkan Debug True + Debug False + D3D12 Debug False (null-backend neutrality link proof). Boot unit gates, 0 failed: Zenithmon **1953**, Combat **1088**, DevilsPlayground **1089**, CityBuilder **1089**, RenderTest **1179 units** (RenderTest builds clean + its units pass; its pre-existing red is a separate windowed terrain test, Q-2026-07-16-001, untouched by this pure-UI change). Zenithmon headless 15/0. Reviewer: no blocking findings (the per-frame-allocation nit was fixed before commit). The two Vulkan Release configs were not separately built -- this is a pure additive plain-C++ change in an always-compiled TU with no config-conditional paths, so Debug True/False + D3D12 False cover the realistic config risk.
- **Reversibility:** easy -- one widget property + one serialization field; deleting it reverts the versions and the two baselines.

---

## 2026-07-17 -- ZM-D-099 -- S5 item 3 (SC5): the windowed round-trip gate -> S5 ITEM 3 COMPLETE

- **Decision:** Ship `ZM_BattleRoundTrip_Test` (windowed, `m_bRequiresGraphics = true`, maxFrames 2200) as the end-to-end gate that ticks the item-3 Roadmap box. It drives the WHOLE slice once through the shipped machinery -- walk Dawnmere grass -> forced FERNFAWN L5 encounter -> assert the additive load only issues behind an OPAQUE fade -> in battle assert active build index == 1, the battle camera is live (`FindMainCameraEntityAcrossScenes` == the Battle scene's own main camera), the overworld is PAUSED (and the pure `IsOverworldPausedInState(IN_BATTLE)` predicate agrees), grass == 0, the arena is `IsFullyBuilt()` with all `uCHILD_COUNT`(9) children owned by the BATTLE scene, biome == MEADOW (`BiomeForScene(DAWNMERE)`), issued-load count == 1 -> call `RequestBattleEnd()` (the SOLE exit) -> assert EXACT resume: active build index back to 2 (Dawnmere), unpaused, Battle scene unloaded, zero arena instances, movement re-enabled, zero aborts, grass restored to the entry blade count, and drift from `GetParkedPlayerPosition()` < 0.05 m.
- **The drift baseline is `GetParkedPlayerPosition()`, NOT the pre-walk position** (reviewer CRITICAL, accepted at plan time): the player must walk >= 1 m to cross a tile boundary and trigger the encounter at all, and keeps moving through the 0.20 s fade, so entry->resume displacement is METRES by design. The entry position is a LOGGED DIAGNOSTIC ONLY, never asserted.
- **NO `ScopedTestIsolation`** -- deliberately, and load-bearing: that guard steals the live subscription tables and would delete the game's own `ZM_OnWildEncounter` subscriber, which is the subject under test. This test adds no subscription of its own and reads the game's live component (contrast the item-2 `ZM_TallGrassEncounter_Test`, which DOES isolate because it stands in its own subscriber).
- **Test-only change -- zero product-code churn.** This locks the ZM-D-094..098 behaviour (own-scene arena build, grass restore seam, scene-inert->live handoff, the 9-state machine, park-don't-freeze, no-timer exit) under the real additive-over-live-overworld flow; it introduces no new decision beyond the gate itself. Reused every helper in the sibling `ZM_AutoTests_BattleTransition.cpp` TU; added only `Flux/Vegetation/Flux_GrassImpl.h` (for `Grass().GetGeneratedInstanceCount()`) and `ZM_BakeManifest.h` (the warm PROP-bake guard) includes.
- **Relies on boot scene-index registration, not a self-register:** unlike `ZM_BattleArena_Test`, the round-trip test never registers build index 1 itself -- the transition machine issues its own additive Battle load, and `Project_LoadInitialScene` registers index 1 at boot; that registry survives the harness's between-test `SCENE_LOAD_SINGLE` reloads. Verified by the green run.
- **Tests that lock it:** `ZM_BattleRoundTrip_Test` itself. **+0 units** (windowed-only) -> boot unit baseline stays **1946**; `zm-tests.yml` UNCHANGED (the SC4 value is correct).
- **Observed gate (dev machine, warm assets):** build GREEN (Vulkan Debug True); boot unit gate **1946 ran / 1945 passed / 0 failed / 1 skipped**; headless **15/0** (the new test auto-skips headless); windowed `ZM_BattleRoundTrip_Test` **PASS (146 frames, `skipped:false`, `failures:[]`)** -- a genuine full-path pass (every default-fails-if-uncaptured invariant held), not a vacuous skip; sibling windowed `ZM_BattleEncounterLatch_Test` (207), `ZM_BattleArena_Test` (31), `ZM_BattleArenaOwnScene_Test` (2) all still PASS.
- **Reversibility:** trivial -- deleting one automated test.

---

## 2026-07-17 -- ZM-D-098 -- S5 item 3 (SC4): the round-trip state machine -- additive load, one-shot entry, pause, camera switch, grass clear, exact restore

- **Decision:** Replace `ZM_BattleTransition::OnUpdate`'s scene-inert latch-drain with the real 9-state machine: IDLE (accept latch) -> FADING_OUT -> **fire-and-forget** `LoadSceneByIndex(1, SCENE_LOAD_ADDITIVE)` -> WAITING_FOR_SCENE (poll `FindLoadedSceneByPath`) -> ENTERING (one-shot park/pause/activate/clear-grass, then poll arena `IsFullyBuilt` + battle camera + `SetBiome`) -> FADING_IN -> IN_BATTLE -> FADING_TO_OVERWORLD -> RESUMING -> RESUME_FADING_IN -> IDLE.
- **Fire-and-forget, then poll:** a `LoadSceneByIndex` issued from a component `OnUpdate` is DEFERRED and returns `INVALID_SCENE` (the scene system is mid-update); the engine drains the single pending slot at the end of that Update. So the machine issues ONCE and polls for the handle on a later frame -- mirroring the shipped `ZM_GameStateManager::IssueSingleLoad`/`PollForTargetScene` split. Because only ONE pending load survives per frame, warp<->battle mutual exclusion is closed in BOTH directions (`TryQueueWarp` rejects on `IsTransitionActive()`; `AcceptPendingEncounter` rejects on `IsWarpInProgress()`).
- **PARK, don't freeze:** `SetScenePaused` gates ONLY the ECS update dispatch -- physics is GLOBAL and keeps stepping every body, and the transform sync keeps writing poses back into the paused scene. So entry zeroes linear+angular velocity and disables gravity on the player's body. **No position is ever written -- this is not teleportation.** The park happens FIRST, while the overworld is still ACTIVE, because `TryGetUniqueActiveScenePlayerEntityID` is active-scene-bound and goes blind the instant focus moves to Battle.
- **`SetActiveScene(Battle)` IS the camera switch** -- `FindMainCameraEntityAcrossScenes` scans the active scene first and Battle authors its own main camera; there is no runtime API to set a scene's main camera (that setter is editor-only). The Battle scene is **NEVER paused** (the pause gates pending-Start dispatch, so a paused Battle would never build its arena). On resume, `SetActiveScene(overworld)` ALWAYS precedes `UnloadScene(battle)` -- the unload auto-reselects the LOWEST build index, which would pick FrontEnd if it were ever loaded.
- **`EnterBattleOnce` is one-shot, split from the readiness poll** (`m_bBattleEntered`): a single combined function returning from its middle on the readiness poll would re-run its whole prefix every frame -- re-clearing grass and re-issuing `SetScenePaused`/`SetActiveScene` once per frame for up to the entire deadline.
- **The grass regen is gated on `m_bGrassCleared`:** `RegenerateForSceneResume` ALWAYS clears before regenerating (ZM-D-095), so running it on an abort that never entered the battle would destroy and rebuild LIVE overworld grass. `ResumeOverworld(bCompletedBattle)` therefore also separates completions from aborts (`m_uCompletedBattleCount` vs `m_uAbortedTransitionCount`) instead of counting every path as a win. The regen drives `QueryAllScenes<ZM_TerrainGrass>()`, NOT `QueryActiveScene` -- under an additive Battle the overworld is loaded but not active.
- **The deadline is WALL-CLOCK (`fPOLL_DEADLINE_SECONDS = 4.0f`), not a frame count.** Nothing pins the frame rate, and the windowed gate runs at a fixed dt of 1/30, where the originally-planned 240-frame budget would have silently meant 8 s. Every poll is bounded and every failure ABORTS TO THE OVERWORLD rather than stranding the player; the fade is the safety boundary (a missing overlay locks non-idle states opaque rather than revealing a half-built arena).
- **No placeholder battle timer.** `RequestBattleEnd()` is the SOLE exit from IN_BATTLE (item 4's seam). The "without a timer the player softlocks" premise is FALSE: no shipped scene can emit an encounter today (`ZM_TallGrassSystem` is registered but attached to no authored scene, and Dawnmere carries no encounter slots), so the only way into a battle is a test forcing one.
- **Implementer-found defect in the plan (accepted):** `ResetRuntimeStateForTests` cleared `m_xParkedPlayerEntityID` WITHOUT releasing the parked body -- parking mutates live body gravity + input, so a mid-transition reset would strand a floating, uncontrollable player in every later test. It now calls `TryRestoreOverworldPlayer(true)` before clearing the ID, mirroring the warp machine's `ResetTransitionState(true)`.
- **Tests that lock it:** **+3 pure units** (`ShouldAcceptBattleEnd_OnlyInBattle` -- RequestBattleEnd is the sole exit; `OwnsFade_EveryNonIdleStateOwnsTheScreen`; `IsOverworldPausedInState_MatchesPauseWindow` -- the predicate SC5 cross-checks against the live `IsScenePaused`). Boot unit gate **1943 -> 1946 (observed)**; `zm-tests.yml` bumped here.
- **`ZM_BattleEncounterLatch_Test`'s assertion was INVERTED, deliberately:** it asserted `state == IDLE` to pin SC3b's scene-inertness -- a contract SC4 intentionally invalidates. It now asserts `OwnsFade(state)` (accepting an encounter must hand the screen to the battle machine). The precise state is deliberately NOT asserted: `AcceptPendingEncounter` enters FADING_OUT in the same `OnUpdate` that makes the count visible, but the 0.20 s fade makes the exact sampled state a race, whereas "no longer IDLE" is not. Still **207 frames, PASS**.
- **No regression:** headless **14/0**; windowed `ZM_TallGrassEncounter_Test`, `ZM_PlayerHomeRoundTrip_Test`, `ZM_BattleArenaOwnScene_Test`, `ZM_TerrainGrassResumeRegen_Test`, `ZM_TallGrassInteriorClear_Test` all PASS with the machine LIVE.
- **Reversibility:** moderate -- the machine is one component's `OnUpdate` + private helpers; no engine change, no data format change.

---

## 2026-07-17 -- ZM-D-097 -- S5 item 3 (SC3b): `ZM_BattleTransition` (order 110) -- scene-INERT component, own persistent root, own overlay, static-pointer encounter latch

- **Decision:** Land `ZM_BattleTransition` (ECS order **110**; next free is now **111**) with its pure policy, the `ZM_OnWildEncounter` latch, its persistent root + overlay, registration/authoring, the between-tests reset, and all tests -- but **deliberately scene-INERT**. `OnUpdate` drains the latch into observable fields and returns; it issues no load, no `SetScenePaused`, no `SetActiveScene`. `RequestBattleEnd()` returns false. State never leaves IDLE. SC4 wires the machine.
- **Why scene-inert:** master's behaviour stays byte-identical and no player can be stranded in an empty arena, while every headless-testable surface + the live-dispatcher wiring lands and is proven. It also isolates the SC3a fade refactor from the state machine, so a regression in either is unambiguous.
- **Its OWN entity + OWN overlay, NOT `ZM_GameStateRoot`/`WarpFade`:** two independent, source-verified reasons. (1) **Fade arbitration** -- `ZM_GameStateManager::OnUpdate` calls `ApplyFadeVisual()` UNCONDITIONALLY every frame, even when IDLE, so a second component driving the SAME overlay would be stomped every frame. (2) **Relocation** -- `ZM_GameStateManager::OnStart` ends with `DontDestroyOnLoad()`, a cross-scene move that MOVE-CONSTRUCTS every component on that entity ("nothing may access this after the call"); a co-located `ZM_BattleTransition` would be relocated mid-Start-dispatch with its own OnStart pending. So: `ZM_BattleTransitionRoot` + a `BattleFade` overlay at sort order **10001**, one above WarpFade's 10000, so the two never contend for the top of the canvas. Mutual exclusion instead of fusion: `TryQueueWarp` gains a `ZM_BattleTransition::IsTransitionActive()` reject clause (inert in SC3b -- it is always false until SC4).
- **Static function pointer + static latch, never a `this`-capturing lambda:** the component relocates on pool swap-and-pop AND on its own `DontDestroyOnLoad`, so a captured `this` would be left bound to a freed address. `Subscribe<ZM_OnWildEncounter>(&ZM_BattleTransition::OnWildEncounterEvent)` also satisfies the "std::function -> plain function pointers" mandate. `DontDestroyOnLoad()` is the LAST statement of its own OnStart.
- **The latch FAILS CLOSED** on `s_bTransitionActive || s_bPendingEncounter`: on the frame `SetScenePaused` lands (SC4), the overworld is still inside `Zenith_SceneSystem::Update`'s snapshot, so `ZM_TallGrassSystem` can dispatch one more stray encounter; a stale latch would re-enter the battle the instant we returned to IDLE. Ownerless statics -> `ResetRuntimeStateForTests()` is wired into the `Zenithmon.cpp` between-tests hook IN THIS SAME sub-commit.
- **Dead-code finding (kept out):** `ZM_SPECIES_NONE == ZM_SPECIES_COUNT` (`ZM_SpeciesData.h:342-343`) and `ZM_SCENE_NONE == ZM_SCENE_COUNT` (`ZM_WorldSpec.h:54-55`) -- the enumerators SHARE a value, so a `>= COUNT` range check alone rejects NONE and a separate `!= NONE` clause would be provably dead. The validators use the range check only; the units name the range check rather than a phantom sentinel branch.
- **Tests that lock it:** **+9 units** (`Tests/ZM_Tests_BattleTransition.cpp`, pure/headless: payload validity incl. out-of-range species + level bounds 1..100 + ineligible source scenes; scene eligibility rejects FRONTEND/BATTLE and accepts town/route/interior; biome totality/default/Dawnmere) -- boot unit gate **1934 -> 1943 (observed)**, `zm-tests.yml` bumped here. Plus `ZM_BattleEncounterLatch_Test` (windowed): **ran 207 frames, not skipped, PASS** -- walks the player into Dawnmere grass with a forced FERNFAWN/5 encounter and proves the GAME's subscriber received it off the LIVE dispatcher (count 1, right payload) while the state stayed IDLE ("SC3b must not touch any scene"). It constructs **NO `ScopedTestIsolation`** -- that guard STEALS the live subscription tables and leaves the dispatcher EMPTY, which would delete the very subscriber under test.
- **Settles the plan's open worry about the item-2 test:** `ZM_TallGrassEncounter_Test` now runs with a live battle subscriber present and still **PASSES (206 frames)** -- because its own `ScopedTestIsolation` suppresses the game's subscriber for the scope, exactly as ZM-D-093 predicted. Verified empirically, not reasoned. `ZM_PlayerHomeRoundTrip_Test` also still passes (673 frames), proving the new `TryQueueWarp` clause is inert.
- **Open for SC4 (flagged, deliberately not decided here):** `OnWildEncounterEvent` fails closed on the battle statics but NOT on `ZM_GameStateManager::IsWarpInProgress()`, so an encounter dispatched mid-warp would still latch. Harmless while the latch does nothing; SC4 must decide whether a warp-in-flight rejects the encounter (that is where `ZM_BattleTransition.cpp` would gain its first `ZM_GameStateManager.h` include -- the header must stay clean to keep the dependency one-directional).
- **Reversibility:** moderate -- a new component + new authoring in the boot-authored `FrontEnd.zscen` (re-authored on every `*_True` boot, so no migration). NO engine change.

---

## 2026-07-17 -- ZM-D-096 -- S5 item 3 (SC3a): shared `ZM_FadeOverlay::Apply` + `ShouldFreezePlayerOnStart` -> `IsWarpInProgress` (pure refactor)

- **Decision:** Extract the full-canvas fade-overlay driver out of `ZM_GameStateManager` into a new shared free function `ZM_FadeOverlay::Apply(Zenith_Entity&, const char* szElementName, float fAlpha)` under a new `Games/Zenithmon/Source/UI/`. `ZM_GameStateManager::ApplyFadeVisual()` keeps its signature and all NINE call sites and becomes a one-line delegation. Separately, RENAME `ZM_GameStateManager::ShouldFreezePlayerOnStart()` -> `IsWarpInProgress()` outright (no alias/shim -- the no-legacy mandate), migrating all three callers (`ZM_PlayerController.cpp:38`, `Tests/ZM_Tests_WorldTraversal.cpp:919` + `:954`) in the same commit.
- **Why now, and why ALONE:** SC3b adds `ZM_BattleTransition` as a SECOND fade owner (its own `BattleFade` overlay) which needs identical overlay logic, and SC3b's warp/battle mutual exclusion reads better against a predicate named for what it means (`IsWarpInProgress`) than for one caller's use of it. Landing the behaviour-preserving refactor as its own commit means that if a warp test ever regresses, the diff that did it is ONE REFACTOR WIDE -- the same "never integrate two things between builds" discipline the OrchestratorPlaybook mandates for content waves. This sub-commit was split out of the planned SC3 by the orchestrator for exactly that reason.
- **Behaviour-identical, verified by reading the real code (not the plan's reconstruction):** the anonymous `ResolveWarpFadeOverlay` helper's only consumer was `ApplyFadeVisual`'s null guard, so inlining the resolve is a pure substitution with the same predicate and short-circuit order; the three mutators (`SetContentSize(0,0)` / `SetAnchorAndPivot(StretchAll)` / `SetGroupAlpha`) and the Show/Hide branch are byte-for-byte in the same order. TWO places where the real source differed from the plan's reconstruction and the REAL CODE WON: the Show/Hide branch is a multi-line if/else (brace convention), and the real comparison is `> fFADE_TRANSPARENT` (an anon-namespace `constexpr float = 0.0f`), NOT a literal `> 0.0f` -- the constant was carried into `ZM_FadeOverlay.cpp` to keep the idiom. `ApplyFadeVisual` had NO manager-specific behaviour to keep outside the delegation.
- **Tests that lock it:** NO new tests -- for a behaviour-preserving refactor the EXISTING suite is the proof, and adding tests would obscure that. **`ZM_PlayerHomeRoundTrip_Test` ran 673 frames, not skipped, and PASSED** -- that is the full Dawnmere <-> PlayerHome warp round trip driving the refactored fade end-to-end. Plus `ZM_WarpInfrastructure_Test` PASS, headless **13/0**, boot unit gate **1934 UNCHANGED** (no units added -> no `zm-tests.yml` bump).
- **Note:** the now-dead `#include "EntityComponent/Components/Zenith_UIComponent.h"` was dropped from `ZM_GameStateManager.cpp` (the UI dependency moved wholesale to `ZM_FadeOverlay.cpp`); the build confirms nothing depended on it transitively. New files under a new directory -> `Build\regen.ps1` was run (`Sharpmake_Games.cs` globs the whole game dir, so no C# edit was needed).
- **Reversibility:** easy -- inline the helper back and reverse the rename; localised to two components + one new file pair.

---

## 2026-07-17 -- ZM-D-095 -- S5 item 3 (SC2): `ZM_TerrainGrass::RegenerateForSceneResume()` -- an explicit, game-owned grass-restore seam

- **Decision:** Add a shipping (NOT `#ifdef`-guarded) public method `bool ZM_TerrainGrass::RegenerateForSceneResume()` that drops the `m_bGrassApplied` latch + the `m_uRetryFrameCount` budget and DELEGATES to the existing private `TryApplyToReadyTerrain()`. Early-returns false for headless / terminal-failure / map-less instances. Five lines; zero duplicated logic.
- **Why an explicit seam is required (the additive path gets nothing for free):** the engine E5 grass reset (ZM-D-092) is wired into the **SINGLE-load** render-reset hook only, so an ADDITIVE battle load neither clears nor restores grass. And the overworld's own `ZM_TerrainGrass` cannot self-heal on resume for two independent reasons: its `OnUpdate` early-returns while `m_bGrassApplied` is true (`.cpp:54`), and while the overworld is paused its `OnUpdate` does not run at all. So the transition must clear on entry and drive this seam on resume.
- **Why delegation rather than a new apply path:** `TryApplyToReadyTerrain` (`.cpp:110-137`) already performs `ClearSceneData()` -> `SetDensityScale` -> `SetDensityMap` -> `GenerateFromTerrain` -> latch. Reusing it means the resume path is byte-for-byte the first-Awake path. **Corollary for SC4:** this call ALWAYS clears before regenerating, so it must never run on a path that did not itself clear.
- **Tests that lock it:** `ZM_TerrainGrassResumeRegen_Test` (windowed, `m_bRequiresGraphics`, +0 units -- baseline stays **1934**). Loads Dawnmere, waits for blades > 0, `ClearSceneData()`, asserts the count really drops to 0, drives the seam, and asserts the restored count EQUALS the baseline (deterministic regen). Non-vacuous by construction: it asserts `baseline > 0` and `afterClear == 0`, and nothing can silently self-heal in between because a bare singleton clear does not touch the `m_bGrassApplied` latch.
- **Gotcha recorded for SC4:** the resume must drive this via `QueryAllScenes<ZM_TerrainGrass>()`, **NOT** `QueryActiveScene<...>()` -- under an additive Battle scene the overworld is loaded but NOT active, so an active-scene query finds nothing and the grass would stay cleared.
- **Reversibility:** easy -- one additive public method + one windowed test, localised to `ZM_TerrainGrass`. NO engine change.

---

## 2026-07-17 -- ZM-D-094 -- S5 item 3 (SC1): `ZM_BattleArena::BuildArena` spawns into the arena's OWN scene, never the active scene

- **Decision:** Resolve `BuildArena`'s target scene via `m_xParentEntity.GetSceneData()` instead of `g_xEngine.Scenes().GetActiveSceneData()` (the option-(b) fix flagged as a decision-to-make in ZM-D-089/091 and Status.md). Also delete the dead `GetActiveSceneData()` fetch + null-guard that sat at the top of `OnStart` (its pointer was never used; `BuildArena` re-fetched it), and add `uCHILD_COUNT` (= 1 dome + 2 platforms + 6 dressing sets = 9) plus a `GetChildEntityID(u_int)` accessor giving tests -- and item 4's battle director -- a stable child enumeration.
- **Why option (b) and not option (a) -- (a) is IMPOSSIBLE, not merely fragile:** the alternative was "`SetActiveScene(Battle)` before the arena's `OnStart` dispatches". There is no game-side seam in which to do that. A `LoadSceneByIndex` issued from a component `OnUpdate` (or from an event handler invoked from one) is DEFERRED (`Zenith_SceneSystem_Operations.cpp:244-253`, because `m_bIsUpdating` is true for the whole of `Zenith_SceneSystem::Update`) and drains at the END of that Update (`Zenith_SceneSystem_Lifecycle.cpp:260-263`). The NEXT frame's `DispatchPendingStarts` loop (`Lifecycle.cpp:221-227`) runs BEFORE any component `OnUpdate`, so the arena's `OnStart` always precedes any chance to move focus. Option (a) would therefore require an ENGINE change (a scene-loaded hook or load-time active-scene override); option (b) is a one-line game-side fix that removes the ordering dependency entirely, because `Zenith_Entity::GetSceneData()` (`Zenith_Entity.h:146`) re-resolves the entity's own slot on every call and is focus-independent (see the THREADING note at `Zenith_Entity.h:70`). Item 3 still calls `SetActiveScene(Battle)` -- but for the CAMERA only, not for correctness.
- **Why `IsFullyBuilt()` could never have caught this:** `Zenith_Entity::IsValid()` resolves the entity's OWN owning scene (`Internal/Zenith_Entity.cpp:34-38`), so a mis-scened arena still reports fully built and the pre-existing `ZM_BattleArena_Test` passed either way. Only a per-child scene-ownership comparison detects it. Non-focused additive scenes really do get `DispatchPendingStarts` because `IsSceneUpdatable()` gates on `IsActivated()` -- a LOAD state (`m_eLoadState == SCENE_STATE_LOADED`, `Zenith_SceneData.h:330`), not "is the active scene".
- **The `SceneData*` stays call-local** -- it points into recyclable storage (`Zenith_SceneSystem.h:110-115`) and is never cached in a member.
- **Tests that lock it:** `ZM_BattleArenaOwnScene_Test` (windowed, `m_bRequiresGraphics`, +0 to the unit baseline) -- additively loads Battle (build index 1) over FrontEnd and **deliberately never calls `SetActiveScene`**, then asserts all `uCHILD_COUNT` children resolve to the Battle scene via `GetSceneDataForEntity` while the active build index stays 0. **Verified as a genuine regression lock, not a vacuous pass: with the one-line fix reverted to `GetActiveSceneData()` the test FAILS; with the fix it PASSES.** Plus `ZENITH_TEST(ZM_BattleArena, ChildCountMatchesArenaComposition)` pinning `uCHILD_COUNT == 9u` (boot unit baseline 1933 -> **1934**, observed; `zm-tests.yml` bumped in this commit). The pre-existing `ZM_BattleArena_Test` still passes (no regression).
- **Reversibility:** easy -- one line in `BuildArena` plus two additive API surfaces; localised entirely to `ZM_BattleArena`. NO engine change (nothing under `Zenith/` is touched), so no cross-game regression run was required.

---

## 2026-07-16 -- ZM-D-093 -- S5 item 2 (SC4): windowed integration -- explicit-species force seam + 2 windowed tests -> S5 ITEM 2 COMPLETE

- **Decision:** Complete S5 item 2 with the windowed integration proof. (a) Add an additive `#ifdef ZENITH_INPUT_SIMULATOR` overload `ZM_TallGrassSystem::ForceEncounterOnNextTransitionForTests(ZM_SPECIES_ID, u_int)` that forces the NEXT on-grass tile transition to dispatch `ZM_OnWildEncounter` with a caller-specified species/level, bypassing BOTH the honest roll AND the scene slot table (so a slot-less TOWN like Dawnmere can drive a deterministic integration encounter; the pre-existing no-arg force synthesizes from the scene's first slot -> a no-op there). Seed-independent (never touches `m_xRng`), consumed once, reset in `OnAwake`. (b) Author 2 windowed tests (`Tests/ZM_AutoTests_TallGrass.cpp`, both `m_bRequiresGraphics`).
- **Runtime-add finding (grounded in code):** a runtime `Zenith_Entity::AddComponent` does NOT fire `OnAwake` (`Zenith_SceneData::CreateComponent` dispatches no lifecycle), but `OnAwake()` is public + callable and `OnUpdate` DOES fire on a runtime-added component (live per-entity dispatch). So the encounter test runtime-adds `ZM_TallGrassSystem` to Dawnmere's terrain entity + calls `OnAwake()` manually -- NO Dawnmere re-bake, no `Zenithmon.cpp`/scene change needed.
- **Tests that lock it (windowed, skip headless CI + add 0 to the boot unit baseline -- stays 1933):** `ZM_TallGrassEncounter_Test` -- loads Dawnmere, runtime-attaches the grass system, subscribes to `ZM_OnWildEncounter` under `Zenith_EventDispatcher::ScopedTestIsolation` (NOT a bare `ClearAllSubscriptions` -- the DP_Tutorial subscription-wipe bug), arms `ForceEncounterOnNextTransitionForTests(FERNFAWN, 5)`, DATA-DRIVES the walk direction by pre-sampling the density map for the nearest grass cardinal, walks the player (input-sim `SetKeyHeld` + fixed dt), and asserts `ZM_OnWildEncounter{FERNFAWN, 5, DAWNMERE}` fired. `ZM_TallGrassInteriorClear_Test` -- Dawnmere grass generates (>0) then SINGLE-load PlayerHome (interior) -> `Grass().GetGeneratedInstanceCount() == 0` (the E5 reset, INTERIOR target vs `ZM_GrassRegeneration_Test`'s FrontEnd target). BOTH PASSED windowed (2/0, the encounter test 206 frames).
- **S5 ITEM 2 IS COMPLETE:** SC2 (`ZM_EncounterZone` roll + `ZM_OnWildEncounter` event + WorldSpec rate, ZM-D-090) + SC3 (`ZM_TallGrassSystem` order 109, ZM-D-091) + SC1 (engine E5 grass reset, ZM-D-092) + SC4 (windowed integration, this entry). The tall-grass -> encounter -> event emission path is proven end-to-end; the interior grass-clear is proven. Roadmap S5 item-2 box TICKED.
- **Item-3 seam intact:** item 2 EMITS `ZM_OnWildEncounter` only -- there is still no real subscriber (item 3 owns loading the additive battle on the event). The `ScopedTestIsolation` future-proofs the encounter test against item 3's subscriber triggering a real battle mid-test.
- **Reversibility:** easy -- the force overload is a test-only additive seam; the tests are additive; no production behaviour change.

---

## 2026-07-16 -- ZM-D-092 -- S5 item 2 (SC1): engine E5 -- grass-singleton scene-state reset is engine-owned + unconditional on SINGLE load

- **Decision (ENGINE change E5):** `Flux_GrassImpl::Reset()` (`Zenith/Flux/Vegetation/Flux_Grass.cpp`) now performs the FULL scene-state clear by delegating to `ClearSceneData()` (chunk LOD + the CPU instance array `m_axAllInstances` + the generated/uploaded flags + the visible/active counters + the copied density map; the VRAM instance buffer is engine-owned and stays allocated until Shutdown). `Reset()` is wired into `Zenith_Engine`'s `m_pfnResetRenderSystems` hook (`Zenith_Engine.cpp`) alongside Terrain/Text/Particles/Skybox/Fog, so it fires UNCONDITIONALLY on the SINGLE-load render-reset path (`Zenith_SceneSystem::ResetAllRenderSystems`; additive loads do NOT hit it).
- **Why:** the global grass singleton retained one scene's blades into the next ("global grass singleton leakage", MasterPlan risk 4) because nothing reset it on scene load. `Reset()` previously did only a partial clear (chunks + counters) and had ZERO callers. Additive + backward-compatible: consumers regenerate grass on their own scene loads, so clearing on load discards only immediately-regenerated state.
- **Tests that lock it:** 3 engine `Flux_Grass` units (`Zenith/Flux/Vegetation/Flux_Grass.Tests.inl`, hosted in the always-linked `Flux_Grass.cpp` under `ZENITH_TESTING`, headless CPU-only via `g_xEngine.Grass()`): `Reset_ClearsAllSceneData`, `Reset_IsIdempotent`, `Reset_NoAccumulationAcrossSetup` (the per-scene instance-count lock). Written to POST-change behaviour (they fail the old partial Reset, pass the full clear).
- **BASELINE BOOKKEEPING (engine units run in EVERY game's boot):** +3 engine units bump BOTH the engine default `1078 -> 1081` in `Tools/run_unit_gate.ps1` (engine-gate.yml [boots Combat] + scaffold-smoke.yml) AND Zenithmon `zm-tests.yml 1930 -> 1933`, in this commit. Verified uniform: Combat boots **1081/0**, Zenithmon **1933/0**.
- **Cross-game regression (AgentBriefing 6.4) -- ALL GREEN with E5:** Combat unit gate **1081/0** (the engine-gate canary), Zenithmon unit gate **1933/0** + windowed `ZM_GrassRegeneration_Test` **PASS** (the grass reset->regenerate cycle on Dawnmere's baked grass -- the direct E5 canary), DP suite **158/0**, CityBuilder suite **45/0**.
- **RenderTest caveat (NOT an E5 regression):** `zenith test RenderTest --headless` is PRE-EXISTINGLY red in this checkout -- it crashes during terrain streaming ("127 missing/invalid terrain chunks" -> `[Core] Assertion failed: Invalid buffer VRAM handle`) BEFORE grass ever generates, because RenderTest's baked terrain is absent here (git-ignored `Assets/`, never `_True`-baked in this checkout). Proven orthogonal to E5 by a stash-revert diagnostic: RenderTest crashes IDENTICALLY (0/10, same assertion) with E5 reverted. E5's grass behaviour was validated instead via the windowed `ZM_GrassRegeneration_Test` (a more direct E5 canary than RenderTest). Logged in Questions.md `Q-2026-07-16-001`.
- **Reversibility:** fully reversible -- revert `Reset()` to the partial clear + drop the one `g_xEngine.Grass().Reset();` hook line; additive, no consumer API changed, no serialized format change.

---

## 2026-07-16 -- ZM-D-091 -- S5 item 2 (SC3): `ZM_TallGrassSystem` (order 109) emits `ZM_OnWildEncounter` on tile-transition-onto-grass

- **Decision:** Author `ZM_TallGrassSystem` (serialization order **109**), a scene-lifetime component sharing the terrain entity (alongside `ZM_TerrainGrass` 101). It owns its OWN `ZM_GrassDensityMap` CPU copy (loaded in `OnAwake` from the terrain sibling's canonical path -- independent of the render feed, headless-safe), quantizes the unique active-scene player to 1 m tiles (`std::floor`), and on a tile transition onto grass (`SampleWorld >= 0.5`) rolls `ZM_EncounterZone::RollStepForScene(activeScene, m_xRng)` and DISPATCHES `ZM_OnWildEncounter` via `Zenith_EventDispatcher`. It EMITS the event only -- the additive battle load is item 3's subscriber.
- **Scene-id resolution (no new API):** the active `ZM_SCENE_ID` is resolved from `GetSceneInfo(GetActiveScene()).m_iBuildIndex` through the pre-existing public `ZM_FindSceneByBuildIndex`; `ZM_SCENE_NONE` is guarded (`eScene < ZM_SCENE_COUNT`) before any bounds-asserted `ZM_GetWorldSpec` read. No `ZM_WorldSpec` edit; the serialize-scene-id fallback was NOT needed (build index is reachable from live scene state).
- **Determinism:** RNG seeded from a fixed salt (`0x5A4D477241535321`) XOR the resolved scene id -- no wall clock, no `Math::Random`. The three static helpers (`QuantizeToTile`/`IsTileTransition`/`IsGrassDensity`) are pure. Baseline tile advances every frame (even off-grass) so re-entering grass is a fresh transition; no transition fires on the first tile.
- **Tests that lock it:** 6 T0 `ZM_Grass` units (`Tests/ZM_Tests_TallGrass.cpp`, pure static helpers: floor semantics incl. negatives-toward--inf, axes-independent, first-tile/same-tile/changed-tile transition, inclusive 0.5 density gate). Boot unit baseline **1924 -> 1930** (Zenithmon-only; engine default 1078 unchanged). Reviewer verdict SHIP. Runtime behaviour (density load, transition rolls, event emission) is covered by SC4's windowed test.
- **SC4 seam note:** the `#ifdef ZENITH_INPUT_SIMULATOR` `ForceEncounterOnNextTransitionForTests()` synthesizes from the scene's FIRST slot only when the scene has slots (safe no-op on a slot-less scene). SC4's windowed test authors the grass onto Dawnmere (a TOWN, no slots) so it will need an EXPLICIT-species force overload (or a grass-bearing route) to fire -- to be added in SC4.
- **Scope:** SC3 of S5 item 2. Not authored into any scene yet (SC4 wires it onto the Dawnmere terrain entity + adds the windowed integration test). The Roadmap item-2 box ticks only after SC1 (engine E5) + SC4 also land.
- **Reversibility:** easy -- new component + registration; no engine change, no serialized format change, no WorldSpec change.

---

## 2026-07-16 -- ZM-D-090 -- S5 item 2 (SC2): `ZM_EncounterZone` pure encounter roll + `ZM_OnWildEncounter` event + WorldSpec per-route rate

- **Decision:** Author the wild-encounter *selection* as pure, headless, deterministic logic (`ZM_EncounterZone::{SelectSlotIndex, RollStep, RollStepForScene}` in `Source/World/`), drawing every random decision from a caller-owned `ZM_BattleRNG` with a FROZEN draw order: (1) rate gate `RandBelow(256) < rate`, (2) weighted slot pick over `ZM_EncounterSlot::m_uWeight`, (3) inclusive level band `RandRange(min,max)`. Per-route chance is data: append `u_int m_uEncounterRatePer256` to the `ZM_WorldSpec` row (append-only -> row-index==id + the referential-integrity suite unaffected), default `uZM_DEFAULT_ROUTE_ENCOUNTER_RATE = 40` (~15.6%/step, an S11-tunable) on ROUTE rows with slots and 0 elsewhere (only `ZM_SCENE_ROUTE1` is non-zero today). `ZM_OnWildEncounter` (species/level/source-scene POD, dispatched via `Zenith_EventDispatcher`) is Zenithmon's FIRST ECS event -- the item-3 seam: SC3's `ZM_TallGrassSystem` emits it, item 3 subscribes to trigger the additive battle.
- **Why pure logic (not a component):** AgentBriefing 3.5 -- rule-based logic is headless C++; only the tile-position tracking is a component (SC3). This keeps the roll fully unit-testable with a rigged RNG stream.
- **Rig-stability invariant (locked):** an inert step (empty table OR rate 0) draws the RNG ZERO times and a rate-gate miss draws EXACTLY once, so inert/miss steps never perturb a rigged stream. The reviewer flagged that the result-only tests didn't lock this; a dedicated `RollStep_InertAndMissDoNotPerturbRng` test (compares raw `ZM_BattleRNG::Next()` positions) was added before landing.
- **Tests that lock it:** 11 T0 units total -- 10 `ZM_Encounter` (`Tests/ZM_Tests_Encounter.cpp`: weighted determinism + a 1:3:6 proportional histogram, single-slot, rate-gate extremes, empty-table, inclusive level band with both endpoints reachable, byte-identical determinism, non-route/interior/battle never fire, Route1 hits stay in the LIVE slot roster, and the RNG-non-perturbation lock) + 1 `ZM_Data` (`WorldSpec_EncounterRateColumn`: route-with-slots > 0, all else 0, <= 256). Boot unit baseline **1913 -> 1924** (bumped in `.github/workflows/zm-tests.yml`; Zenithmon-only, engine default 1078 unchanged). Reviewer verdict SHIP.
- **Scope:** SC2 of S5 item 2. The Roadmap S5 item-2 box stays UNCHECKED until SC3 (`ZM_TallGrassSystem`) + SC1 (engine E5) + SC4 (windowed integration) also land. `ZM_EncounterEvents.h` is defined but not yet dispatched/subscribed (SC3/item-3 wire it).
- **Reversibility:** easy -- new pure module + one append-only WorldSpec field; no engine change, no serialized format change (the WorldSpec table is a compiled const array, not saved data).

---

## 2026-07-16 -- ZM-D-089 -- S5 item 1: `ZM_BattleArena` component + Battle scene (build index 1) at world Y = -2000

- **Decision:** Introduce ECS component `ZM_BattleArena` (serialization order **108**, the next free ZM order) that, in `OnStart`, spawns an always-visible enclosing dome (large `CreateUnitSphere` primitive) + two battle platforms (flattened `CreateUnitCube` slabs) + six per-biome dressing sets (the S4 `ZM_PROP_DRESSING_*` props via `ZM_PropAssetPath(...ZM_PROP_ASSET_MODEL...)` + `LoadModel`), of which exactly one is enabled at a time. `SetBiome(ZM_BATTLE_BIOME)` swaps the visible set via `Zenith_Entity::SetEnabled`. Author a self-contained `Battle` scene (`AddStep_CreateScene`) whose arena-root entity sits at `fARENA_WORLD_Y = -2000 m`; register it as build index 1 in `Project_LoadInitialScene` **outside** `#ifdef ZENITH_TOOLS` so `_False`/Android builds resolve it too. Mirrors the `ZM_GreyboxVisual`/`ZM_GameComponent` runtime-rebuild idiom (marker component authored into the `.zscen`; children spawned at runtime on load, never persisted).
- **Why the offset is per-entity, not a scene property:** there is **no** engine scene-world-offset mechanism (`Zenith_SceneData` has no offset field; no `AddStep_SetSceneOffset`; PlayerHome/build-40 is authored at origin). The -2000 m offset (which lets a later S5 slice additively load the battle without overworld bleed-through -- ZM-D-007) is realized by authoring/spawning every arena entity at world Y = -2000. Centralized on `fARENA_WORLD_Y`. WorldSpec's `ZM_SCENE_BATTLE` row (build index 1, kind BATTLE, empty terrain set) already existed from S1 -- **not edited**.
- **Component-local biome enum:** `ZM_BATTLE_BIOME_{MEADOW,VOLCANIC,COAST,WETLAND,SNOW,CANYON}` ordered to match `ZM_PROP_BIOME`'s six real biomes, so the prop-roster ordering does not leak into the arena API; the biome->dressing table (`s_aeDressingProp`) is fixed/save-stable.
- **Review fixes applied before landing (reviewer verdict SHIP-WITH-FIXES):** (1) **camera Z-sign bug** -- the battle camera was at Z=+8 looking +Z (engine forward at yaw 0 is +Z), i.e. *away* from the arena at Z~=0; moved to Z=-8 so it looks toward the platforms (mirrors the PlayerHome behind-the-subject camera). (2) added `bool IsFullyBuilt() const` (all 9 child entities `IsValid()`) + a windowed-test assertion, closing the reviewer's "a regression that spawned nothing still flips m_bBuilt" gap. (3) `ReadFromDataStream` now resets `m_eActiveBiome = MEADOW` before the version/range gates (the `ZM_WarpTrigger` ClearConfiguration-first idiom).
- **Tests that lock it:** 5 T0 `ZM_BattleArena` units (`Tests/ZM_Tests_BattleArena.cpp`, pure/all-config): `BiomeEnumCoverage`, `DressingMappingContract` (biome->distinct real DRESSING prop, roster-order tag, out-of-range->NONE), `VisibilityExactlyOne` (one-hot `1u<<e`), `ArenaConstants` (-2000 / version 1), `WorldSpecBattleRowContract` (build 1 / kind BATTLE / empty terrain). Windowed `ZM_BattleArena_Test` (graphics + warm-PROP-bake + `Battle.zscen` guarded, auto-skips headless CI) additively loads Battle, asserts a unique arena, `IsBuilt()` + `IsFullyBuilt()`, `SetBiome(VOLCANIC)`, and root Y within 0.5 of -2000. Boot unit baseline **1908 -> 1913** (bumped in `.github/workflows/zm-tests.yml` in this commit). End-to-end windowed run PASSED with real warm assets (1/0, 31 frames).
- **Forward note (item 3, NOT an item-1 issue):** `BuildArena` resolves children via `GetActiveSceneData()`. Correct for item 1 (Battle is the active scene both in the game flow and in the windowed test, which `SetActiveScene(Battle)` before OnStart dispatch). But when S5 item 3 loads the battle **additively over a still-active overworld**, `GetActiveSceneData()` would be the overworld and the arena children would spawn there (orphaned). Item 3 should switch `BuildArena` to the arena's own scene (the parent entity's `GetSceneData`) rather than the active scene.
- **Reversibility:** easy -- new component + new scene + one build-index registration; no existing serialized format changed, no S4 generator touched, WorldSpec unchanged.

---

## 2026-07-16 -- ZM-D-088 -- S4 gate SIGNED OFF -- S4 (Asset generators) COMPLETE

- **Decision (gate sign-off):** the user reviewed the re-captured full-family gallery (after the ZM-D-087 building-overlap fix) and **APPROVED** the S4 visual gate. **S4 (Asset generators) is COMPLETE.**
- **Evidence reviewed:** `Build/artifacts/zenithmon/s4/gallery/gallery_0{1,2,3}.tga`+`.png` (front/left/right) -- the windowed `ZM_AssetGallery_Test` showing 26 representatives across all four families (8 creatures one-per-archetype incl. a shiny / 6 humans / 6 buildings / 6 props) on a reflective floor, buildings cleanly separated.
- **Gate summary (all GREEN + signed):** full 5-config matrix (Vulkan Debug/Release x True/False + `D3D12_vs2022_Debug_Win64_False` link proof); boot unit gate **1908 / 0 failed** (creature/creature-anim/human/building/prop `ZM_Gen` generator units + 6 tools bake smokes + 3 `ZM_BakeManifest` units); `zenith test --headless` 7/0; `ZM_AssetGallery_Test` windowed PASS; per-family determinism/structural/static-or-skeletal + bake smokes + the byte-identical re-bake invariant; user visual sign-off 2026-07-16.
- **What S4 shipped:** the full procedural asset pipeline -- `ZM_GenCommon`+`ZM_TextureSynth` foundation, `ZM_CreatureGen`(v3, 152, skinned+animated) + `ZM_CreatureAnimGen`, `ZM_HumanGen`(v1, 34, shared rig+9 clips), `ZM_BuildingGen`(v1, 30) + `ZM_PropGen`(v1, 25, static), the four `ZM_GenCommon` bake bridges (own-skel / shared-skel / bind-shared / no-skel), and `ZM_BakeManifest` (per-family version+file-existence guard). All assets are procedurally generated + baked to git-ignored `Assets/` under the manifest guard.
- **Next:** **S5 (Battle integration slice)** -- critical path. The next session starts here (Status.md refreshed for a cold resume).
- **Reversibility:** n/a (a stage-gate sign-off).

---

## 2026-07-16 -- ZM-D-087 -- S4 gate REJECTED (buildings intersecting) -> width-budget layout fix

- **Rejection + root cause:** the user reviewed the ZM-D-086 gallery captures and REJECTED the S4 visual gate -- the buildings were intersecting each other. Root cause (tracked to `Tests/ZM_AutoTests_AssetGallery.cpp` `BuildAGBuilding`): each family sat in a FIXED-pitch grid row and each model was scaled by **HEIGHT ONLY** (`targetHeight / naturalHeight`, uniform). Buildings have widely-varying footprint-to-height aspect ratios -- `ZM_ResolveBuildingRecipe` keeps `m_fStoreyHeight = 3.0`, so a 1-storey wide building (CareCenter 10x8x1, Lab 9x8x1, PlayerHome 7x6x1) got scale = 6.5/3 ~= **2.17** -> its 7-10u footprint scaled to **15-22u wide**, far past the **12u column pitch** -> adjacent buildings INTERSECTED. (2-storey gyms scaled ~1.08 and barely fit, hiding the cause.)
- **Fix:** a reusable `AGFitScale(width, height, widthBudget, heightCap) = min(widthBudget/width, heightCap/height)` -- scale each instance to fit BOTH a per-column WIDTH budget AND the height cap. Building pitch widened 12->14; width budgets building 11 (< 14) + prop 6.5 (< 8); buildings add the 0.6 roof overhang to the footprint width. Applied to buildings + props (the varying-footprint families); creatures/humans stay height-normalized (they are ~isotropic, so height-norm keeps them within pitch -- a property of those rosters, documented as such).
- **Documentation (so it does NOT recur -- user's explicit ask):** in-code comments at the layout constants + `AGFitScale` + both builders reference ZM-D-087; a persistent reference memory (`reference_gallery_grid_layout_width_budget`) records the trap -- a fixed-pitch grid of variable-aspect-ratio instances must space by SCALED FOOTPRINT (the min-ratio width/height budget), and must never assume height-normalization keeps width within the pitch.
- **Re-capture (automated gate GREEN):** rebuilt Debug True; `ZM_AssetGallery_Test` re-PASSED windowed (1 passed / 0 failed; 26 models + 3 TGAs on disk); the orchestrator re-reviewed the front + right captures -- buildings now cleanly separated (6 distinct, ~11u wide, varied heights 3-6u, ~3u gaps), nothing off-frame, all four families legible. Evidence refreshed at `Build/artifacts/zenithmon/s4/gallery/gallery_0{1,2,3}.tga`+`.png`.
- **GATE-WAIT re-set** for the user's re-review (StartPrompts prompt 4); the S4 visual gate is still PENDING sign-off.
- **Reversibility:** trivial -- one gallery-test-file scale/layout change + docs; no shipping/generator/version/boot change.

---

## 2026-07-16 -- ZM-D-086 -- S4 gate: full-family asset gallery -> GATE-WAIT (visual sign-off)

- **Decision (S4 gate approach -- user choice):** the user chose (2026-07-16) the "full gallery (all families)" S4 gate over closing on the existing creature-gallery sign-off (ZM-D-067) alone. New `Tests/ZM_AutoTests_AssetGallery.cpp` -- a windowed `ZM_AssetGallery_Test` (`#ifdef ZENITH_INPUT_SIMULATOR`, `m_bRequiresGraphics`, the `ZM_BakeAllAssets()` bake additionally `#ifdef ZENITH_TOOLS`) that COPIES the shipped creature-gallery scaffolding (reflective metallic floor, key+fill directional lights, dark skybox, bloom/auto-exposure/text/quads OFF + SSR ON, `Flux_Screenshot` TGA dump, additive isolated `SCENE_LOAD_ADDITIVE_WITHOUT_LOADING` scene, full cleanup/restore) and shows **26 representatives across all four families** -- 8 creatures (one per archetype, incl. a shiny) / 6 humans (PlayerM, Aster, Vesper, Fenna, Elara, Caretaker) / 6 buildings (PlayerHome, Lab, GymGrass, GymFire, CareCenter, TownHall) / 6 props (LampPost, SignPost, FenceWood, RockLarge, Barrel, DressingMeadow) -- laid out in four Z-rows (props +18 nearest, creatures +6, humans -6, buildings -22 farthest), each normalized to a target on-screen height and instanced from its disk `.zmodel` via `Zenith_ModelComponent::LoadModel` (creatures+humans render in bind pose with no animator; static buildings/props render directly). NEW test only -- no generator/data/version/boot change; `ZM_BakeAllAssets()` is called ONLY from the gallery Setup (the deferred wiring from ZM-D-085), guarded by the manifest so re-runs warm-skip.
- **Why:** the S4 gate needs a visual sign-off; the creature species gallery was already signed (ZM-D-067) but humans/buildings/props are new families added since. One comprehensive gallery lets the user visually verify all four families in a single sign-off.
- **Gate results (automated -- ALL GREEN):** regen + Vulkan Debug True + Debug False (non-tools link proof) build; `ZM_AssetGallery_Test` PASSED windowed (1 passed / 0 failed; 26 models loaded at frame 30 + 3 angle TGAs dumped + asserted on disk); `ZM_BakeAllAssets()` cold-baked all four families successfully (all 4 `game:<Family>/.manifest` stamps written -> warm re-runs fast); `zenith test --headless` 7/0 (the gallery skips headless -- graphics-required); boot unit gate 1908 / 0 failed unchanged (an automated test adds no boot units). The orchestrator reviewed gallery_01 (front) + gallery_02 (left) and did ONE camera/scale tuning pass for framing (creature/human target height 3.0->4.5, building 6.0->6.5, prop 2.5->3.0; camera front (0,16,46)/aim(0,2,-4) -> (0,12,40)/aim(0,2.5,-5) + tightened side views) so the hero creatures/humans read prominently. Evidence (git-ignored): `Build/artifacts/zenithmon/s4/gallery/gallery_0{1,2,3}.tga` + `.png`.
- **GATE-WAIT:** `GATE-WAIT: S4 gallery visual sign-off` marker set in Status.md; the loop PARKS for the user's APPROVE/REJECT (StartPrompts prompt 4). Do NOT tick the S4 gate or start S5 without sign-off.
- **Reversibility:** trivial for the harness -- one new windowed test file; no shipping/generator/version/boot change. If REJECTED, rework framing/roster in `ZM_AutoTests_AssetGallery.cpp` (or the flagged generator) + re-capture. If APPROVED: tick the S4 gate, clear the marker, S4 COMPLETE -> S5.

---

## 2026-07-16 -- ZM-D-085 -- S4 ZM_BakeManifest: per-family bake guard -- ALL S4 CODE BOXES DONE

- **Decision (per-family bake guard):** the last S4 code box. New `Source/Gen/ZM_BakeManifest.{h,cpp}`: a per-family deterministic 12-byte manifest stamp (`ZMBM` magic + u32-LE generator version + u32-LE expected-file count -- mirroring the shipped terrain `ZMTR` marker; NO timestamps/paths, so a re-bake writes byte-identical stamp bytes) written ATOMICALLY (temp + rename) to `game:<Family>/.manifest`. The read/enumerate/check surface is ALL-CONFIG (so the in-memory `ZM_Gen` gate exercises it headless); only the disk write + the `ZM_BakeAllAssets` orchestrator are `#ifdef ZENITH_TOOLS` with non-tools no-ops. `ZM_BakeManifestCheck(family, root)` returns WARM iff the stamp is current (magic+version+count all match) AND every enumerated file (roster x kinds via the per-family AssetPath fns) is present non-empty -- FAIL-OPEN: false on ANY doubt (missing/short/wrong stamp, absent/empty file, filesystem error), so the family always re-bakes when unsure. Each of the four `ZM_BakeAll*` now gates at the top (skip a warm family) + stamps after a fully-successful roster bake. The guard resolves files via `FamilyRefToFsPath(ref, GAME_ASSETS_DIR)` = `GAME_ASSETS_DIR/(ref-"game:")`, which matches where the bake's `ResolvePath("game:...")` writes (`s_strGameAssetsDir + "/" + rel`), so the guard warms correctly in production (reviewer-verified).
- **Decision (boot wiring DEFERRED -- orchestrator scoping):** a tools-only `ZM_BakeAllAssets()` ANDs the four guarded orchestrators, but is deliberately UNCALLED (no shipped caller, exactly like the `ZM_BakeAll*` it wraps). The planner proposed wiring it into the unconditional tools boot (`Project_RegisterEditorAutomationSteps`); I SCOPED THAT OUT because it would run a synchronous cold full-family bake (~2-4 min creatures) on EVERY unit-gate / CI / headless boot for NO current benefit (no current scene references the baked building/prop/human families -- confirmed). The boot/gallery wiring is deferred to the S4 gallery gate (where the gallery Setup drives `ZM_BakeAllAssets()` on the windowed review machine, exactly as it bakes the creature dozen today), gated by this manifest. Lower-risk (no `Zenithmon.cpp` boot-path change) + keeps CI/gate boots fast.
- **Decision (versions frozen):** the manifest only READS `uZM_CREATUREGEN_VERSION`(3)/`uZM_HUMANGEN_VERSION`(1)/`uZM_BUILDINGGEN_VERSION`(1)/`uZM_PROPGEN_VERSION`(1); it changes NO generation code and adds only a `.manifest` stamp file per family -> no creature/human/building/prop generated byte moves.
- **Why:** warm boots must skip re-baking a family whose version + files are current (the AssetManifest section 6.1 guard contract), and the byte-identical re-bake invariant (same seed -> byte-identical disk output) is a tested guard. The stamp mirrors the shipped terrain marker precedent byte-for-byte in shape.
- **Tests that lock it:** three new `ZENITH_TEST(ZM_Gen, BakeManifest_*)` in `Tests/ZM_Tests_BakeManifest.cpp` -- EnumerationMatchesRoster (ALL-CONFIG: enumerate sizes == roster math [creatures 152x15, humans 10 shared + 34x4, buildings 30x4, props 25x4], every ref under its family root, version accessor), RebakeByteIdentical (TOOLS: bake CareCenter twice, FNV-hash each of the 4 baked files' bytes via `Zenith_DataStream::ReadFromFile`+`GetCapacity`, assert byte-identical -> proves Export/SaveToFile are disk-deterministic -- the new value over the in-memory `*_BuildDeterminism` units), GuardWarmStale (TOOLS: temp-root RAII guard driving no-stamp->cold / stamp->warm / missing-file->cold / empty-file->cold / version-mismatch->cold, exercising FAIL-OPEN). Tests 2/3 are `#ifdef ZENITH_TOOLS`, ZENITH_SKIP when the bake env is unavailable. Gate: regen + FULL 5-config matrix (Vulkan Debug/Release x True/False + `D3D12_vs2022_Debug_Win64_False` link proof) all build green; boot unit gate **1908 ran / 1907 passed / 0 failed / 1 skipped** (was 1905; +3 -- tests 2+3 RAN + PASSED, confirming the disk re-bake determinism + the guard's warm/stale semantics end-to-end); `zenith test --headless` 7/0. Independent reviewer: CLEAN -- verified enumerate==baked-set per family, guard-vs-bake path agreement (the load-bearing one), deterministic stamp + fail-open, atomic temp+rename, and non-vacuous tests (this box's implementer was cut off mid-task by a session limit and got no self-review, so the orchestrator independently verified AGENTS.md-root/`GetCapacity`-on-file-stream/`ResolvePath` before gating, then the reviewer confirmed).
- **KNOWN cosmetic NIT (left as-is):** on the near-unreachable ofstream-flush-failure branch, `ZM_WriteBakeManifest` returns without removing the `.manifest.tmp` -- self-healing (the next call's `remove(xTemp)` clears it), fail-open safe. A naive one-line `remove` there would be INEFFECTIVE on Windows (the ofstream still holds the file open), so a correct fix needs the stream closed first; left for a future tidy-up since `ZM_WriteBakeManifest` only runs after a successful 100+-file bake (so the disk is writable).
- **Reversibility:** high -- one new tools/all-config module + four localized gate/stamp edits + one new tools-only test TU; no version bump, no generation-byte change, no boot-path change. **`ZM_BakeManifest` COMPLETE -- ALL S4 CODE BOXES DONE.** Remaining S4: the **S4 GATE** -- 4-config matrix + a windowed asset gallery showing every family (creatures/humans/buildings/props) + **VISUAL SIGN-OFF (the next VISUAL GATE, a hard stop for the user)**. At the gate: wire `ZM_BakeAllAssets()` into the gallery Setup, capture windowed evidence, set GATE-WAIT in Status.md, and park for the user.

---

## 2026-07-16 -- ZM-D-084 -- S4 ZM_BuildingGen/ZM_PropGen SC6: coverage + doc reconcile -- ROADMAP ITEM COMPLETE

- **Decision (item-completion doc reconcile):** SC6 closes the `ZM_BuildingGen`/`ZM_PropGen` Roadmap item with NO code change -- the coverage/totality the SC-breakdown named for SC6 is already enforced by the shipped `BuildingGen_RosterTotality` (all 30 buildings) + `PropGen_RosterTotality` (all 25 props) units, so SC6 is the deferred reference-doc reconcile + box tick (mirroring the human generator's SC5 doc reconcile). AssetManifest section 3 replaces the high-level buildings/props spec with the RESOLVED 4-file static-bundle breakdown (per building: `<Name>.zmesh` skeleton-less / `<Name>_facade.ztxtr` BC1 256^2 / `<Name>.zmtrl` / `<Name>.zmodel` no-rig; per prop: `<Name>.zmesh` / `<Name>_albedo.ztxtr` BC1 128^2 / `<Name>.zmtrl` / `<Name>.zmodel`; family totals 30x4=120 + 25x4=100; colliders SCENE-authored, not baked) + the `uZM_BUILDINGGEN_VERSION`/`uZM_PROPGEN_VERSION`=1 stamps in section 6.1. TestPlan section 5.4 gains the BuildingGen (9 units) + PropGen (8 units) + 2 bake-smoke `ZM_Gen` subsections and reconciles the stated boot baseline 1866 -> 1905 (it had lagged since the human SCs, per the established CI-bumps-every-SC / prose-reconciles-at-completion cadence).
- **Why:** the item is functionally complete after SC5 (both families generate + bake to disk); SC6 makes the reference docs describe the shipped reality and ticks the box. No test/code was needed -- the totality coverage is already carried by the per-family RosterTotality units.
- **Tests that lock it:** unchanged from SC5 -- the 19 `ZM_Gen` building/prop units (BuildingGen 5+2+2, PropGen 6+2) + the 2 tools bake smokes (boot unit gate 1905). No new units; SC6 is documentation + the Roadmap tick only, so no build was run (Docs-only; the frozen bake headers were comment-fixed in SC5, not here).
- **Reversibility:** trivial -- documentation + a Roadmap checkbox. **`ZM_BuildingGen`/`ZM_PropGen` Roadmap item COMPLETE** (SC1-SC6: static foundation -> parametric building shell -> building facade -> prop generator -> disk bake -> doc reconcile; 19 `ZM_Gen` units; both families generate + bake skeleton-less static bundles). Remaining S4: `ZM_BakeManifest` (generator-version stamp + file-existence gate + byte-identical re-bake invariant -- the last S4 code box), then the S4 gate (4-config matrix + windowed gallery + visual sign-off = the next VISUAL GATE).

---

## 2026-07-16 -- ZM-D-083 -- S4 ZM_BuildingGen/ZM_PropGen SC5: tools disk bake (static bundles)

- **Decision (static disk-bake bridge):** SC5 wires the tools disk bake for BOTH static families by adding ONE new additive tools-only `ZM_GenCommon` bridge `ZM_GenBakeStaticMesh(const ZM_GenMesh&, const char* szMeshPath)` -- the `ZM_GenBakeMesh` body with the skin block (`SetVertexSkinning`), the `Zenith_SkeletonAsset` build/Export, the `SetSkeletonPath`, and the skel-path `create_directories` all REMOVED. It Exports ONLY a skeleton-less `.zmesh` (no bone buffers + no skeleton path -> `Zenith_MeshAsset::HasSkinning()`==false). The existing three bridges (`ZM_GenBakeMesh`/`ZM_GenBakeSkeleton`/`ZM_GenBakeMeshWithSharedSkeleton`) ALL force a skeleton, hence the new one; they are left BYTE-FOR-BYTE unchanged.
- **Decision (skeleton-less bundle bake):** fill the four bake stubs (`ZM_BakeBuilding`/`ZM_BakeAllBuildings` + `ZM_BakeProp`/`ZM_BakeAllProps`), mirroring `ZM_BakeHuman` MINUS the skeleton/anim: `ZM_GenBakeStaticMesh` for the `.zmesh` + `ZM_SynthBakeAlbedoBC1` for the facade/albedo + owning-handle `Create<Zenith_MaterialAsset>()`+`GetDirect()` `.zmtrl` (facade/albedo in BASE_COLOR, roughness 0.8 / metallic 0.0) + owning-handle `Create<Zenith_ModelAsset>()`+`GetDirect()` `.zmodel` (SetName + `AddMeshByPath(meshRef, {matRef})` + Export, with NO SetSkeletonPath and NO AddAnimationPath). `create_directories` first (via the mesh/albedo bakes -- all 4 kinds resolve to the same `game:<Family>/<Name>/` folder); `exists()` the ONLY IO-success signal (SaveToFile returns true / Export is void); a path overflow aborts before any write. Owning-handle discipline keeps the material/model alive across SaveToFile/Export.
- **Decision (versions frozen):** `uZM_GENCOMMON_VERSION`=1 (bridge additive, no existing byte moves -> creatures/humans do NOT re-bake), `uZM_BUILDINGGEN_VERSION`=1, `uZM_PROPGEN_VERSION`=1, `uZM_TEXTURESYNTH_VERSION`=2 -- ALL UNCHANGED. No baked building/prop format existed before SC5 (the FIRST bake for both families), so nothing to invalidate (the ZM_HumanGen SC5 precedent).
- **Why:** a baked building/prop is now a complete scene-loadable STATIC model (mesh + material, no rig), through the same asset-layer path as creatures/humans but skeleton-less. Colliders remain SCENE-authored, not baked (AssetManifest section 3).
- **Tests that lock it:** two NEW tools-only (`#ifdef ZENITH_TOOLS`) `ZENITH_TEST(ZM_Gen, *Bake_StaticModelFilesLandAndNoRig)` in `Tests/ZM_Tests_BuildingBake.cpp` + `ZM_Tests_PropBake.cpp` (bake CareCenter / LampPost; all 4 per-model files land non-empty; hermetically re-parse the `.zmodel` and assert `GetSkeletonPath().empty()` + `HasSkeleton()`==false + `GetNumAnimations()`==0 -- the INVERSE of the human/creature smoke; `ZENITH_SKIP` when the bake env is unavailable). Gate: regen + the FULL 5-config matrix (Vulkan Debug/Release x True/False + `D3D12_vs2022_Debug_Win64_False` null-backend link proof) ALL build green; boot unit gate **1905 ran / 1904 passed / 0 failed / 1 skipped** (was 1903; +2 -- the 2 bake smokes RAN + PASSED, confirming an end-to-end static disk bake); `zenith test --headless` 7/0. Independent reviewer: CLEAN (line-by-line diffed the bridge against `ZM_GenBakeMesh` [faithful, all skeleton/skin removed], confirmed additive-only [76/0, 7/0 numstat], the 4 bake bodies write static bundles with owning handles + correct texture member (`m_xFacade`/`m_xTexture`) + safe same-folder dir ordering, and the smokes assert the static contract non-vacuously); its one NIT -- stale "STUB...real bake = SC5" comments on the two frozen bake headers -- was fixed in this commit.
- **Reversibility:** high -- one additive tools-only bridge, four filled bake bodies, two new tools-only test files, plus stale-comment fixes on the two bake headers (decls unchanged); `ZM_GenBakeMesh` and all four version constants untouched; no generated/creature/human byte changed. Remaining: SC6 coverage/totality + AssetManifest section 3 file breakdown + TestPlan reconcile -> ticks the `ZM_BuildingGen`/`ZM_PropGen` Roadmap box, then `ZM_BakeManifest` closes S4.

---

## 2026-07-16 -- ZM-D-082 -- S4 ZM_PropGen SC4: prop generator (roster + seam + static prims)

- **Decision (prop generator, mirror BuildingGen):** SC4 lands `ZM_PropGen` as a literal mirror of the shipped `ZM_BuildingGen` family (5 new files): a 25-prop `ZM_PropData` roster (fences x2, signs x2, lamps x2, bridges x2, ledges x2, rocks x3, furniture x6, + 6 battle-dome biome dressing sets, one per real biome MEADOW/VOLCANIC/COAST/WETLAND/SNOW/CANYON), a frozen `ZM_PropGen` seam (`uZM_PROPGEN_VERSION`=1: `ZM_PropRecipe`/`ZM_ResolvePropRecipe`/`ZM_BuildPropMesh`/`ZM_BuildPropTexture`/`ZM_BuildProp` + equality/hash/`ZM_ValidateProp` + `ZM_PROP_ASSET_KIND`/`ZM_PropAssetPath` `game:Props/<Name>/`), and tools bake STUBS returning false (real bake = SC5).
- **Decision (box-only static geometry, no new emitter):** every prop is a per-kind composition of the shipped bone-free `ZM_StaticMesh::AppendBox` (fence = 3 posts + 2 rails; sign = post + panel; lamp = base + post + head; bridge = 2 piers + deck + 2 rails; ledge = slab + lip; rock = 1 box; furniture = body + top; dressing = 4 boxes). NO new `ZM_GenCommon` emitter was warranted -> `ZM_GenCommon.h`/`.cpp` UNTOUCHED, `uZM_GENCOMMON_VERSION` stays 1. Every prop is grounded at y=0 (lowest box base literal 0.0f), static (zero bones, empty skin), outward-wound (inherited from AppendBox), single `{0,0,1,1}` UV island -> passes `ZM_ValidateGenMeshStatic`.
- **Decision (domain isolation):** `ZM_BuildPropMesh` draws ONLY 7 MESH-domain floats (3 dim jitter + 4 dressing aux) up-front in a FIXED order before the kind switch (so kind never changes the draw count); `ZM_BuildPropTexture` (128^2 placeholder albedo) draws ONLY 4 ALBEDO-domain floats (base RGB + accent jitter, + a biome tint for dressing sets). So a MESH-seed change can't perturb the texture and vice-versa. `ZM_TextureSynth` reused (`ZM_SynthFillSolid`/`ZM_SynthStampRectDecal`) -> `uZM_TEXTURESYNTH_VERSION` stays 2.
- **Why:** props are the second and final family of the last big S4 generator; mirroring the shipped building family keeps the static-mesh + synth reuse contract and lets SC5 bake both families through the same (upcoming) `ZM_GenBakeStaticMesh` bridge. Colliders are SCENE-authored, excluded from the generator (AssetManifest section 3).
- **Tests that lock it:** eight pure `ZENITH_TEST(ZM_Gen, PropGen_*)` -- RosterTotality (all 25 self-index + build + `ZM_ValidateProp` pass + static contract + biome contract: DRESSING rows carry a real biome, others NONE), RecipePurity (pure f(id) + pairwise-distinct seeds + MESH!=ALBEDO), AssetPathScheme (golden `game:Props/LampPost/...` + truncation), BuildDeterminism (reflexive + distinct-ids-differ), StaticMeshContract (zero bones/empty skin/outward winding/finite UVs), BiomeDressingCoverage (every real battle-dome biome has >=1 dressing prop), plus MeshSensitivity + TextureDomainIsolation (mutate one domain seed -> that output changes, the OTHER stays byte-identical; distinct ids/palettes differ) -- the latter two ADDED before landing to close the reviewer's NIT that props lacked the domain-isolation coverage buildings have. Gate: regen + Vulkan Debug True + Debug False (non-tools link proof) build green; boot unit gate **1903 ran / 1902 passed / 0 failed / 1 skipped** (was 1895; +8); `zenith test --headless` 7/0. Independent reviewer: CLEAN (no BLOCKER/SHOULD-FIX; verified no box dimension goes <=0 after +-4% jitter [shortest lamp post fH-0.35=1.954>0], all boxes grounded/connected, faithful boilerplate); the domain-isolation NIT was closed before commit; one remaining NIT (dead lamp width/depth roster data) left as harmless.
- **Reversibility:** high -- 5 new additive files; no `ZM_GenCommon`/`ZM_TextureSynth`/generated-byte change; the `ZM_PROP_ID`/kind/palette/biome enums are save-stable append-only. Remaining: SC5 tools disk bake for BOTH families (+`ZM_GenBakeStaticMesh` bridge -- skeleton-less `.zmesh`+`.zmtrl`+skeleton-less `.zmodel`; `uZM_GENCOMMON_VERSION` stays 1), SC6 coverage/totality + AssetManifest section 3 + TestPlan reconcile -> ticks the Roadmap box.

---

## 2026-07-16 -- ZM-D-081 -- S4 ZM_BuildingGen SC3: facade window/door decals

- **Decision (real facade):** SC3 replaces the flat single-palette facade fill in `ZM_BuildBuildingFacade` with a real facade: paint the WALL band (image V in [0,0.72] -- the mesh maps ground->V0, eave->V0.72) with a per-palette base + a gym theme tint (`ZM_SynthTypePalette(m_eThemeType).m_xBase` blended 35% when `m_eThemeType != ZM_TYPE_NONE`) + a `m_uWindowCols x m_uWindowRows` window grid + a door decal; and the ROOF band (V in [0.78,1.0], filled from V>=0.75 with a 0.03 eave margin) a per-palette roof colour. Reuses the EXISTING `ZM_SynthFillSolid`/`ZM_SynthStampRectDecal`/`ZM_SynthTypePalette` (all normalized [0,1] UV) -- NO new texture synth, so `uZM_TEXTURESYNTH_VERSION` stays 2.
- **Decision (ALBEDO-domain only):** the facade constructs ONLY `ZM_MakeGenRNG(recipe, ZM_GEN_DOMAIN_ALBEDO)` and draws exactly 4 colour-jitter floats (wall R/G/B + roof) in a FIXED order BEFORE any palette/theme/grid branch (the window loop + door draw NO RNG). So (a) a MESH-seed mutation leaves the facade byte-identical (keeps mesh/facade domains cleanly separated -- makes `FacadeDomainIsolation` meaningful), (b) the draw count is palette/theme/cols/rows-independent, (c) determinism holds.
- **Decision (resolves the SC1 collision NIT):** SC1's flat fill made distinct ids byte-identical whenever they shared a palette. SC3 diverges them two ways: gyms via their distinct `m_eThemeType` tint (all 8 gym tints pairwise distinct); same-palette non-gyms via the name-seed-derived ALBEDO jitter (`m_uSyntheticSeed = ZM_GenHashName(name)` is distinct per building -> distinct ALBEDO seed -> distinct jitter across the whole facade). No test requires all-30-distinct, so any residual collision is harmless.
- **Why:** real building facades (windows/door/roof as separate surfaces, gym-type-themed walls) close the SC3 box while keeping the static-mesh + synth reuse contract and freezing all three version constants (no baked output yet).
- **Tests that lock it:** two new pure `ZENITH_TEST(ZM_Gen, BuildingGen_Facade*)` -- FacadeStructural (all 30 rows: 256^2 non-empty; window/door pixels land in the WALL band, colour-distinct from the roof-band sample -- catching any V-flip; >=3 distinct colours among wall/roof/window/door; deterministic same-seed) and FacadeDomainIsolation (mutating the ALBEDO seed changes the facade [Equals false + different ContentHash]; mutating a non-ALBEDO (MESH) seed leaves it byte-identical; distinct palette/theme ids differ; same-palette gyms diverge). Gate: Vulkan Debug True build green; boot unit gate **1895 ran / 1894 passed / 0 failed / 1 skipped** (was 1893; +2); `zenith test --headless` 7/0. Independent reviewer: CLEAN -- derived the full mesh->image V agreement (ground->V0, eave->V0.72, roof fill 0.75>0.72 eave margin -> windows/door provably in the wall band, roof cannot bleed onto walls), confirmed ALBEDO-only + the >=3-colour robustness + the SC1-NIT divergence; 3 non-blocking documentation NITs (a redundant lowest-window-row V-assert; a 0.75-vs-0.78 comment gap).
- **Reversibility:** high -- localized `ZM_BuildBuildingFacade` rewrite + additive file-local helpers + header layout constants + 2 tests; no seam/version/synth/generated-byte change (SC1's flat `ZM_BuildingPaletteColour` retained as the wall base). Remaining: SC4 PropGen, SC5 disk bake (+`ZM_GenBakeStaticMesh`), SC6 coverage+docs.

---

## 2026-07-16 -- ZM-D-080 -- S4 ZM_BuildingGen SC2: real parametric shell (roofs + jitter)

- **Decision (parametric shell + roof geometry):** SC2 replaces SC1's placeholder box in `ZM_BuildBuildingMesh` with the real parametric shell: a jittered body box (footprint W x D x storeys, grounded at y=0) into a wall UV island {0,0,1,0.72}, then a roof by `ZM_ROOF_KIND` into a DISJOINT roof UV island {0,0.78,1,1.0}. Three new ADDITIVE static emitters in `ZM_GenCommon` (`namespace ZM_StaticMesh`): `AppendGableRoof` (2 sloped pitch quads via `ZM_PushStaticFace` with normal = normalize(cross(BR-BL,TL-BL)) + 2 triangular gable ends), `AppendHipRoof` (4-triangle pyramid to a centre apex), `AppendFlatRoof` (thin parapet-cap box). Plus a file-local `ZM_PushStaticTri` helper that AUTO-ORIENTS each triangle's stored normal away from an interior reference point and emits the matching winding, so BOTH `cross(C-A,B-A).storedNormal>0` (the validator's check) AND genuine outward-facing hold by construction -- designing out the winding/normal bug class.
- **Decision (MESH-domain jitter):** the builder draws exactly 4 values (width/depth/storey-height +-3%, roof pitch +-10%) from ONLY `ZM_MakeGenRNG(recipe, ZM_GEN_DOMAIN_MESH)`, in a FIXED order BEFORE the roof-kind branch (FLAT still draws pitch), so (a) the MESH seed perturbs the mesh (makes `MeshSensitivity` meaningful), (b) NO non-MESH domain seed can change the mesh bytes, (c) the draw count is roof-kind-independent, (d) determinism (rebuild == byte-identical) holds. The box base stays literal y=0 so grounding is jitter-invariant. The `ZM_GEN_DOMAIN` enum is UNCHANGED (reuses the pinned MESH domain).
- **Decision (UV atlas split):** wall island (lower 72%) + roof island (upper 22%) with a [0.72,0.78] gutter -- both sub-rects of [0,1] (the static validator rejects out-of-range UVs), reserving the wall band for SC3's window/door decals.
- **Why:** real building silhouettes (pitched gable/hip roofs vs flat parapets) with per-face texture regions, while keeping the static-mesh contract (bone-free) and both version constants frozen (no baked output yet). The roof emitters + `ZM_PushStaticTri` are additive `ZM_GenCommon` primitives reusable by SC4 PropGen.
- **Tests that lock it:** two new pure `ZENITH_TEST(ZM_Gen, BuildingGen_*)` -- ShellStructural (all 30 rows: exact per-kind counts GABLE 38/18, HIP 36/16, FLAT 48/24 + `ZM_ValidateGenMeshStatic` outward-winding/finite-UV + grounded y=0 + roof apex > nominal wall height) and MeshSensitivity (mutating the MESH seed perturbs the mesh; mutating a non-MESH (ALBEDO) seed leaves it byte-identical; distinct ids differ). Gate: Vulkan Debug True build green; boot unit gate **1893 ran / 1892 passed / 0 failed / 1 skipped** (was 1891; +2 -- the count-band + winding asserts passing across all 30 rows prove the roof geometry); `zenith test --headless` 7/0. Independent reviewer: CLEAN -- hand-derived both gable pitch windings, both `ZM_PushStaticTri` branches, `xInside` interiority, and the roof-above-walls margin for all 30 rows (tightest FLAT BattleTower 9.13 > 9.0).
- **Reversibility:** high -- additive `ZM_GenCommon` emitters (version unchanged) + a localized `ZM_BuildBuildingMesh` rewrite + 2 tests; no seam/enum/generated-byte change. Remaining: SC3 facade window/door decals (reuse `ZM_SynthStampRectDecal` into the wall island), SC4 PropGen, SC5 disk bake (+`ZM_GenBakeStaticMesh`), SC6 coverage+docs.

---

## 2026-07-16 -- ZM-D-079 -- S4 ZM_BuildingGen/ZM_PropGen design + SC1 (static-model foundation)

- **Decision (static-model architecture):** `ZM_BuildingGen`/`ZM_PropGen` generate STATIC models (NO skeleton, NO animation), unlike the skinned creature/human generators. The engine supports skeleton-less models (`Zenith_ModelAsset::HasSkeleton()` gates a conditional render path; a `.zmodel`/`.zmesh` with an empty skeleton path is a valid static model, `Flux_ModelInstance` takes the static branch). So add an ADDITIVE static-mesh foundation to the shared `ZM_GenCommon`: a bone-free primitive kit (`ZM_StaticMesh::AppendBox` -- 24-vert/12-tri box, per-face hard normals, outward winding byte-identical to `Zenith_MeshAsset::GenerateUnitCube`, island-mapped UVs, writes NO bone buffers) + a dedicated `ZM_ValidateGenMeshStatic` (the skinned `ZM_ValidateGenMesh` marks a no-weight mesh invalid, so static geometry needs its own validator). Both additive/all-config; the skinned loft + validator are UNTOUCHED, so `uZM_GENCOMMON_VERSION` STAYS 1 and creatures/humans are byte-unaffected.
- **Decision (two drivers over a shared foundation):** two separate drivers (`ZM_BuildingGen` + `ZM_PropGen`), each with its own roster data table (`Source/Data/ZM_BuildingData`/`ZM_PropData` mirroring `ZM_HumanData`: save-stable append-only enum + compiled `const` C-array + accessors), version constant, asset-path scheme, and test file -- over the shared `ZM_GenCommon` static kit + `ZM_TextureSynth`. NOT one merged driver (would conflate two rosters/versions/schemes and kill the SC4-parallelism).
- **Decision (SC breakdown):** SC1 shared static foundation + BuildingGen seam/roster/minimal box shell (THIS commit); SC2 real parametric shell (footprint x storeys, gable/hip/flat roof, per-face UV islands); SC3 facade window/door decals (REUSE the existing `ZM_SynthStampRectDecal` -- no new texture synth, `uZM_TEXTURESYNTH_VERSION` stays 2); SC4 PropGen seam+roster+prims (parallelizable after SC1); SC5 tools disk bake for both families (adds a NEW additive tools-only `ZM_GenBakeStaticMesh` bridge -- skeleton-less `.zmesh` + `.zmtrl` + skeleton-less `.zmodel`, `uZM_GENCOMMON_VERSION` stays 1); SC6 coverage/totality + `AssetManifest`/`TestPlan` doc reconciliation + baseline = ticks the Roadmap box. Colliders are SCENE-authored, EXCLUDED from the generator (`AssetManifest` section 3).
- **Decision (SC1 scope):** the 30-model building roster (12 house style x palette + PlayerHome + Lab + 8 gyms + CareCenter/TradePost/League/BattleTower + 4 civic fillers; the 8 gyms' `m_eThemeType` pinned to the GDD gym-leader element types Grass/Fire/Water/Electric/Sky/Phantom/Ice/Drake), the frozen `ZM_BuildingGen` public seam (`uZM_BUILDINGGEN_VERSION`=1), pure recipe resolution (name-hash family seed -> per-domain `ZM_GenDeriveSeed`; MESH+ALBEDO domains only, `ZM_GEN_DOMAIN` enum unchanged), a minimal real-dimensioned static box shell (real parametric shell = SC2), a flat per-palette facade (decals = SC3), FNV-1a content hash + byte-exact equality, the `game:Buildings/<Name>/...` path scheme, and tools-only bake STUBS returning false (real bake = SC5).
- **Why:** buildings/props are the last big S4 generator; a static-mesh foundation on the shared kit lets both families reuse the loft/texture/bake machinery while the skinned pipeline stays frozen. The SC ordering mirrors the shipped creature/human generators (foundation+seam+roster -> mesh -> texture -> bake -> coverage).
- **Tests that lock it:** five pure all-config `ZENITH_TEST(ZM_Gen, BuildingGen_*)` -- RosterTotality (all 30 rows self-index + build + `ZM_ValidateBuilding` pass + gym/non-gym theme-type contract), RecipePurity (pure f(id) + pairwise-distinct synthetic seeds + MESH!=ALBEDO seed), AssetPathScheme (golden `game:Buildings/CareCenter/...` refs + truncation->false), BuildDeterminism (reflexive byte-identity/hash + CareCenter-vs-GymGrass non-degeneracy), StaticMeshContract (zero bones, empty skin buffers, tris>0, outward winding, finite in-range UVs). Gate: regen + Vulkan Debug True + Debug False (non-tools link proof) build green; boot unit gate **1891 ran / 1890 passed / 0 failed / 1 skipped** (was 1886; +5); `zenith test --headless` 7/0. Independent reviewer: CLEAN -- hand-derived winding (+Z/-Y outward, matching `GenerateUnitCube`), confirmed additions are purely additive (numstat 171/0 + 37/0, zero deletions), the static contract and validator are non-vacuous, and the roster/gym-type mapping is correct.
- **KNOWN by-design caveat (reviewer NIT, not a defect):** because SC1's mesh depends only on (width, depth, storeys) and the facade only on palette -- the per-name seeds and theme type are deliberately UNUSED until SC2/SC3 -- several DISTINCT building ids currently produce byte-identical SC1 bundles/hashes (e.g. GymWater=GymSky=GymPhantom=GymIce, GymFire=GymElectric, PlayerHome=ShopWarm). The synthetic seeds ARE pairwise-distinct and the non-degeneracy test picks a genuinely-differing pair, so nothing regresses. **A future disk hash-gate (SC5/SC6) must NOT assume all 30 bundles are distinct until SC2/SC3 consume the seed + theme type.**
- **Reversibility:** high -- additive `ZM_GenCommon` foundation (version unchanged) + new BuildingData/BuildingGen/test files; no skinned/creature/human/generated byte changed; `ZM_BUILDING_ID` is save-stable append-only. Remaining: SC2 parametric shell, SC3 facade decals, SC4 PropGen, SC5 disk bake (+`ZM_GenBakeStaticMesh` bridge), SC6 coverage+docs -> ticks the Roadmap box.

---

## 2026-07-16 -- ZM-D-078 -- S4 ZM_HumanGen SC5: tools disk bake -- ROADMAP ITEM COMPLETE

- **Decision (two additive bridges):** add two tools-only `ZM_GenCommon` bake bridges -- `ZM_GenBakeSkeleton` (builds a `Zenith_SkeletonAsset` from `ZM_GenMesh::m_xBones` via `AddBone`+`ComputeBindPoseMatrices` and Exports ONLY the `.zskel`, for a SHARED rig) and `ZM_GenBakeMeshWithSharedSkeleton` (element-wise copies the pure buffers into a `Zenith_MeshAsset`, `SetSkeletonPath`s a SHARED ref, `ComputeBounds`, Exports ONLY the `.zmesh` -- writes NO per-model `.zskel`). Both are `#ifdef ZENITH_TOOLS` with non-tools `{ return false; }` no-op inlines. The existing `ZM_GenBakeMesh` (which writes a per-model `.zskel`) is left BYTE-FOR-BYTE unchanged; the new mesh bridge DUPLICATES its vertex/triangle/skin copy block verbatim rather than refactoring, so no shipped creature-path bytes move.
- **Decision (bake orchestration):** fill the three frozen `ZM_HumanGen.cpp` bake stubs (previously `return false`). `ZM_BakeHumanShared` bakes the ONE shared 16-bone rig (`ZM_AppendSharedHumanBones` -> `ZM_GenBakeSkeleton`, which `create_directories(Humans/Shared/)`) then the 9 shared rotation-only clips (`ZM_BuildHumanClip` -> `Flux_AnimationClip::Export`, which creates no dirs) ONCE for the whole roster. `ZM_BakeHuman` bakes one model's mesh (`ZM_GenBakeMeshWithSharedSkeleton` binding the shared `.zskel` ref, creating `Humans/<Name>/`) + albedo (BC1 256^2) then `.zmtrl` (matte dielectric, albedo in BASE_COLOR) + `.zmodel` (single-submesh mesh + `SetSkeletonPath` shared + `AddAnimationPath` x9 shared, IDLE..FAINT). Owning-handle discipline (`Zenith_AssetRegistry::Create<T>()`+`GetDirect()` kept alive across SaveToFile/Export); `create_directories` first; `exists()` is the only IO-success signal (mirrors `ZM_BakeCreature`). `ZM_BakeAllHumans` bakes the shared rig then every roster model. Not wired into the boot/editor-automation path (like `ZM_BakeAllCreatures`, it is exercised by the tools smoke test; a windowed showcase is optional SC6).
- **Decision (versions frozen):** `uZM_GENCOMMON_VERSION` and `uZM_HUMANGEN_VERSION` both STAY 1 -- the bridges are additive and no generation algorithm changed, so no generated bytes move and no creature re-bake is triggered. (Creatures bumped their version at SC6 only because prior on-disk `.zmodel`s existed to invalidate; no human bundle was ever baked, so there is nothing to supersede.) The frozen `ZM_HumanGen.h` public seam is unchanged -- only the three `.cpp` bake bodies were filled.
- **Why:** completes `ZM_HumanGen` (Roadmap box ticked) -- a baked human is now scene-loadable WITH its shared rig + 9-clip set, INVERTING the creature bundle (ONE shared `.zskel` + 9 shared `.zanim` + ~34 per-model mesh/material/model that bind them, vs each creature's self-contained per-species bundle).
- **Tests that lock it:** two new tools-only (`#ifdef ZENITH_TOOLS`) `ZENITH_TEST(ZM_Gen, HumanBake_*)` in the new `Tests/ZM_Tests_HumanBake.cpp`: `HumanBake_SharedAndModelFilesLand` (bakes shared rig + PlayerM; all 10 shared files [1 `.zskel` + 9 `.zanim`] + the 4 per-model files [`.zmesh`/`_albedo.ztxtr`/`.zmtrl`/`.zmodel`] land non-empty) and `HumanBake_ModelBindsSharedRigAndClips` (hermetic `ReadFromFile`+`ParseStream` load of the baked `.zmodel`; asserts it binds the SHARED skeleton ref -- proving NO per-model rig -- and self-lists the 9 shared clip refs in IDLE..FAINT order); both `ZENITH_SKIP` when the bake env is unavailable. **Gate:** full 5-config matrix (Vulkan Debug/Release x True/False + `D3D12_vs2022_Debug_Win64_False` null-backend link proof) ALL build green; boot unit gate **1886 ran / 1885 passed / 0 failed / 1 skipped** (was 1884; +2 -- the 2 bake units RAN and PASSED, confirming an end-to-end bake); `zenith test Zenithmon --headless` **7 passed / 0 failed**; all creature `ZM_Gen` units green (creatures unaffected). Independent reviewer: clean -- no BLOCKER/SHOULD-FIX; the 2 NITs (unchecked path-return in the model-clip loop; albedo IO not folded into `bOk`) are inherited verbatim from the shipped creature-bake template and are independently covered by the test, so kept for parity.
- **Reversibility:** high -- two additive tools-only bridges, three filled bake bodies, one new tools-only test file; no public seam, generated format, or committed asset changed; `ZM_GenBakeMesh` and both version constants untouched. **`ZM_HumanGen` Roadmap item COMPLETE** (SC1-SC5; 20 `ZM_Gen` units). SC6 (optional windowed showcase proving a baked human animates in-scene) may follow; not required. Next S4 item: `ZM_BuildingGen`/`ZM_PropGen`, then `ZM_BakeManifest` closes S4.

---

## 2026-07-15 -- ZM-D-077 -- S4 ZM_HumanGen SC4: shared deterministic nine-clip curves

- **Decision (internal seam + shared schedule):** add internal-only `Source/Gen/ZM_HumanAnim.cpp` and move `ZM_BuildHumanClip` from the SC1 empty driver stub into that TU without widening `ZM_HumanGen.h`. Author the nine shared clips ONCE through the linkable `ZM_CreatureAnimCommon` kit against the exact fixed 16-bone names. Their frozen schedules are Idle 2.0 s loop, Walk 1.0 s loop, Run 0.7 s loop, Talk 1.6 s loop, Wave 1.0 s one-shot, Point 0.8 s one-shot, Cheer 1.2 s one-shot, Hurt 0.4 s one-shot, and Faint 1.2 s one-shot.
- **Decision (rotation-only playback policies):** every channel contains rotation keys only; there are no position keys, scale keys, root motion, events, source paths, or RNG consumption. Key times are finite, strictly increasing, and bounded by duration. Idle/Walk/Run/Talk close their first/last loop seams; Wave/Point/Cheer/Hurt return to identity; Faint settles into and holds its final authored pose. The result is pure `f(clip)`, byte-identical across all 34 roster models, and uses only bones present in the one shared skeleton.
- **Why / frozen scope:** one shared deterministic clip set preserves transferable animation and prevents per-model drift or duplicate generated bytes. `ZM_HumanGen.h`, the fixed shared skeleton, `uZM_HUMANGEN_VERSION=1`, roster data, mesh/albedo generation, and asset/clip metadata paths remain unchanged. SC5 disk bake/material/model work, the two `ZM_GenCommon` bake bridges, baked assets, creature changes, the windowed showcase, TestPlan.md, and AssetManifest.md are excluded; the two documentation files are deferred to SC5.
- **Tests and evidence:** four new pure `ZENITH_TEST(ZM_Gen, HumanGen_*)` cases -- HumanGen_ClipChannelsMatchSharedSkeleton, HumanGen_ClipTimingAndPlaybackPolicy, HumanGen_ClipDeterminismAndSensitivity, and HumanGen_ClipSetSharedAcrossRoster -- lock exact channel ownership, rotation-only structure, finite/strict/bounded keys, metadata schedules, loop seams, one-shot recovery/Faint hold, deterministic bytes/hashes, clip sensitivity, and roster-wide sharing; HumanGen_ClipMetadataGolden was extended for all nine schedules. Regeneration/check + target-only Vulkan Debug True build green; boot unit gate **1884 ran / 1883 passed / 0 failed / 1 skipped** (was 1880; +4); headless **7 passed / 0 failed**. The known 300 s helper watchdog occurred only after the successful tally. Independent reviewer: clean after its brittle exact-sample Faint-hold finding was corrected with authored penultimate/final-key coverage. Evidence: `Build/artifacts/zenithmon/s4/humangen_sc4/`.
- **Reversibility:** high -- one additive internal TU, removal of the localized empty stub, and additive/extended tests; no public seam, generated format, disk bake, or committed asset changed. Removing `ZM_HumanAnim.cpp` and restoring the empty driver stub returns SC3. Remaining: SC5 tools disk bake (ticks the Roadmap box), SC6 optional windowed showcase.

---

## 2026-07-15 -- ZM-D-076 -- S4 ZM_HumanGen SC3: deterministic appearance + silhouette attachments

- **Decision (internal seam + atlas):** add internal-only `Source/Gen/ZM_HumanAppearance.{h,cpp}` rather than widening the frozen public `ZM_HumanGen.h` seam. The internal header owns all eight normalized UV islands: the six SC2 body islands remain byte-identical, while HAIR `{.475,.430,.700,.900}` and ATTACHMENT `{.710,.430,.990,.900}` reuse the previously unused lower-right atlas area. The retained 256x256 image paints cores as `ceil(UV0*256)..floor(UV1*256)-1`, then extends each core by exactly one clamp-to-edge texel including corners; remaining gutters stay background.
- **Decision (deterministic albedo):** `ZM_BuildHumanAlbedo` consumes exactly six unconditional `ZM_GEN_DOMAIN_ALBEDO` draws up front: skin/hair/outfit FBM seeds (`Next()` x3), then skin value `[.96,1.04)`, hair value `[.90,1.10)`, and outfit value `[.90,1.10)`. Fixed three-octave FBM drives only its owned material family. MESH, SKELETON, PATTERN, EYE, SHINY, DEX_ICON, and ANIM are unconsumed by the painter. The existing five skin tones are retained; seven hair colours include ID/type-themed DYED; seven outfit roles use fixed primary/secondary/accent palettes, documented League type themes, fixed hairlines/eyes/role masks, and attachment-island colours.
- **Decision (silhouette + skinning):** hair styles 0..5 are frozen as Crop/Bob/Swept/Bun/Long-back/Topknot: every model gets the shared Head-rigid hair cap plus the selected draw-free loft elaboration. Attachments are NONE/Cap/Hat/Backpack/Glasses/Satchel; Cap/Hat/Glasses are rigid Head, Backpack/Satchel rigid Spine. Production order is body -> hair -> attachment -> the existing sole tangent/weight finalisers. SC2's exact six fixed-order MESH draws remain unchanged.
- **Why / frozen scope:** SC3 delivers visible per-model texture and silhouette variation while preserving one transferable shared skeleton and one material/submesh. `ZM_HumanGen.h`, the shared 16 bones, `uZM_HUMANGEN_VERSION=1`, and asset/clip metadata remain unchanged; version 1 is deliberately retained because no human disk bake exists yet. No SC4 clip curves, SC5 bake/material/model work, `ZM_GenCommon` bridges, TextureSynth/CreatureGen changes, normal/shiny/icon outputs, windowed gate, or baked assets enter SC3.
- **Tests and evidence:** four new pure `ZENITH_TEST(ZM_Gen, HumanGen_*)` cases -- AppearanceAlbedoStructural, AppearanceDomainIsolation, HairStyleSilhouettes, and AttachmentSilhouettes -- cover all 34 rows, 256x256 material validity, all domains/appearance axes, deterministic direct appenders, exact direct/full shifted suffixes, UV ownership, rigid Head/Spine bindings, and attachment-atlas differentiation. Regeneration/check + target-only Vulkan Debug True build green; boot unit gate **1880 ran / 1879 passed / 0 failed / 1 skipped** (was 1876; +4); headless **7 passed / 0 failed**. The known 300 s helper watchdog occurred only after the successful tally. Independent reviewer: functionally clean; its P3 brace/lambda-name findings were corrected. Evidence: `Build/artifacts/zenithmon/s4/humangen_sc3/`.
- **Reversibility:** high -- two additive internal files plus localized mesh/driver integration and additive tests; no baked format or committed asset changed. Removing the internal append/painter calls restores SC2. Remaining: SC4 shared nine-clip curves, SC5 tools disk bake (ticks the Roadmap box), SC6 optional windowed showcase.

---

## 2026-07-15 -- ZM-D-075 -- S4 ZM_HumanGen SC2: per-model humanoid mesh loft

- **Decision (topology):** replace SC1's minimal `ZM_BuildHumanMesh` with a dedicated pure all-config `Source/Gen/ZM_HumanMesh.cpp` that appends unchanged shared bones 0..15, then exactly six StickFigure-derived loft parts in torso/head+neck/left arm/right arm/left leg/right leg order. The golden 13/9/11/13 ring rows use 48/64/28/36 segments and subdivision 4; every ring uses at most two shared bone indices. Finalisation remains `ZM_GenGenerateTangents` -> `ZM_GenNormalizeSkinWeights`; analytic loft normals are not regenerated.
- **Decision (variation + determinism):** grounded height is `(y + 1) * heightScale`, with deliberately modest SLIGHT/AVERAGE/STOCKY/TALL factors 0.98/1.00/0.97/1.03. BUILD girth is 0.85/1.00/1.25/1.00 on the torso, attenuated to 25% of its delta for the head and 65% for limbs; STOCKY uses a 0.82 torso superellipse base. Exactly six `ZM_GEN_DOMAIN_MESH` draws are consumed up front in fixed order: torso Rx, torso Rz, torso superellipse, head size, shared arm girth, shared leg girth. Non-MESH seeds cannot perturb the mesh; changing MESH must.
- **Why / frozen scope:** girth-led variation and modest grounded height preserve alignment with the one transferable shared skeleton. `ZM_HumanGen.h`, `uZM_HUMANGEN_VERSION=1`, `uZM_HUMAN_BONE_COUNT=16`, shared bone names/order/transforms, and shared asset/clip metadata remain unchanged. SC2 adds no texture/attachment work, animation curves, disk bake, baked assets, `ZM_GenCommon` changes, or creature changes.
- **Tests that lock it:** four new `ZENITH_TEST(ZM_Gen, HumanGen_*)` cases -- StructuralInvariants, PerModelBonesMatchShared, SameSeedDeterminism, and Sensitivity (MESH changes; every non-MESH domain does not; cross-id difference). Regeneration/check + target-only Vulkan Debug True build green; boot unit gate **1876 ran / 1875 passed / 0 failed / 1 skipped** (was 1872; +4); headless automated gate **7 passed / 0 failed**. The canonical helper reached its 300 s watchdog only after the successful tally and all 105 editor-authoring steps completed. Independent reviewer: clean, no actionable findings. Evidence: `Build/artifacts/zenithmon/s4/humangen_sc2/`.
- **Reversibility:** high -- one new implementation TU plus localized placeholder removal and additive tests; no baked format/output exists yet. Remaining: SC3 texture + attachment silhouette, SC4 shared 9-clip curves, SC5 tools disk bake (ticks the Roadmap box), SC6 optional windowed showcase.

---

## 2026-07-14 -- ZM-D-074 -- S4 ZM_HumanGen design + SC1 (roster + shared 16-bone skeleton + frozen seam)

- **Decision (architecture):** `ZM_HumanGen` reuses the shipped `ZM_GenCommon`/`ZM_CreatureGen`/`ZM_CreatureAnimGen` pipeline but INVERTS the bundle: creatures bake a self-contained per-species bundle (own `.zskel` + own 6 `.zanim`); humans author + bake ONE shared `.zskel` and ONE shared 9-clip `.zanim` set ONCE, then ~35 per-model bundles (mesh + material + model) that all `SetSkeletonPath` the shared skel + `AddAnimationPath` the same 9 shared clips (AssetManifest section 2: "No per-model clips"). Per-model variation lives ONLY in the mesh loft + texture, NEVER in the skeleton, so the 9 rotation-only pure-`f(clip)` clips pose every model identically. SIMPLER than creatures: NO archetype dispatch (one body plan), NO shiny, NO dex icon, NO per-model normal map (v1). Pure library all-config; disk bake `#ifdef ZENITH_TOOLS`. `uZM_HUMANGEN_VERSION=1`, `uZM_HUMAN_BONE_COUNT=16`; keyframe rate REUSES `uZM_CREATURE_ANIM_TICKS_PER_SECOND` (24).
- **Decision (shared 16-bone skeleton):** the generalized StickFigure core -- Root, Spine, Neck, Head, Left/Right {UpperArm, LowerArm, Hand}, Left/Right {UpperLeg, LowerLeg, Foot} -- IDENTITY bind-local rotation on EVERY bone (mandatory: rotation-only clips are absolute-local; a non-identity bind poses every model wrong), unit scale, feet grounded near world y=0 (Root local Y raised to +1.0 vs StickFigure's y=-1.0). Authored via `ZM_GenCommon` (`ZM_GenAddBone`), NOT reused from StickFigure (its helpers are anonymous-namespace/un-linkable + a single fixed human). The canonical emit lives in ONE place -- `ZM_AppendSharedHumanBones(ZM_GenMesh&)` -- consumed by BOTH the per-model mesh builder (skins verts to indices 0..15) and the shared-skeleton bake, guaranteeing identical bone count/names/index order (the index-keyed skin + name-keyed clip-transfer invariant).
- **Decision (9-clip set, frozen at SC1):** undocumented in the design docs -> a ZM_HumanGen design choice. Idle(2.0s,loop), Walk(1.0s,loop), Run(0.7s,loop), Talk(1.6s,loop), Wave(1.0s), Point(0.8s), Cheer(1.2s), Hurt(0.4s), Faint(1.2s) -- authored ONCE against the shared bone names via the linkable `ZM_CreatureAnimCommon` kit, rotation-only, pure `f(clip)` -> byte-identical across all models. Metadata golden-locked; curves land in SC4.
- **Decision (roster, 34 rows ~35):** new `ZM_HumanData` table (Source/Data/, mirrors `ZM_SpeciesData`: save-stable append-only `ZM_HUMAN_ID` + compiled const C-array + accessors). NAMED cast uses the GDD section 3 CANONICAL names (verified): Professor Aster, Mom Maren, Rival Vesper, 8 gym leaders (Fenna/Bram/Maris/Tessa/Aquilo/Morwenna/Halvard/Vardis), Elite Four (Cassia/Torben/Lumen/Sable) + Champion Elara. INVENTED-ORIGINAL (GDD leaves unnamed): PlayerM/PlayerF (2 distinct meshes), 10 trainer classes (Rambler/Angler/Netter/Ridgewalker/Ace/Scout/Ranger/Duelist/Wayfarer/Camper), 6 townsfolk (Villager/Caretaker/Shopkeep/Elder/Fieldhand/Dockworker). Zero Nintendo IP. Per-row variety axes (drive mesh+texture, NOT skeleton): build preset, skin tone, hair style+colour, outfit/role, attachment.
- **Determinism:** per-human seed = `ZM_GenDeriveSeed(ZM_GenHashName(m_szName), (u_int)id, const evo, domain)` -> `m_aulDomainSeed[]`; randomness reaches builders ONLY via `ZM_MakeGenRNG(recipe, domain)`; the `ZM_GEN_DOMAIN` enum is REUSED unchanged (no per-model SKELETON draw -- skeleton is shared/fixed). FNV-1a content hash + `ZM_ValidateHuman` mirror the creature seam.
- **Tests that lock it:** `ZENITH_TEST(ZM_Gen, HumanGen_*)` -- RosterTotality (all 34 build + full `ZM_ValidateHuman`), SharedSkeletonWellFormed (16 bones, single root, parent<child, identity bind rotation), RecipePurity (pure `f(id)` + distinct seeds), AssetPathScheme (golden shared+per-model refs + too-small-buffer->false), ClipMetadataGolden (the frozen 9), BuildDeterminism (reflexive `ZM_HumanBuildEqual`/`ZM_HumanMeshEqual`/`ZM_HumanContentHash` + PlayerM-vs-Bram non-degeneracy). Boot unit gate **1872 ran / 1871 passed / 0 failed / 1 skipped** (was 1866; +6); headless 7/0; Vulkan Debug True green. SC1's mesh is a MINIMAL valid loft (real humanoid loft = SC2).
- **Reversibility:** moderate -- new additive TUs; `ZM_HUMAN_ID` is save-stable append-only. Remaining: SC2 per-model humanoid mesh loft, SC3 texture + attachment silhouette, SC4 the shared 9-clip curves, SC5 tools disk bake (two additive `ZM_GenCommon` bake bridges + shared/per-model bake + docs + baseline; TICKS the Roadmap box), SC6 (optional) windowed showcase gate.

---

## 2026-07-14 -- ZM-D-073 -- S4 ZM_CreatureAnimGen SC6: bundle-bake wiring -- ROADMAP ITEM COMPLETE

- **Decision:** SC6 wires the disk bundle bake, completing `ZM_CreatureAnimGen` (Roadmap box ticked). (1) `ZM_CreatureGen.h`: appended 6 anim asset kinds `ZM_CREATURE_ASSET_ANIM_IDLE.._FAINT` before `ZM_CREATURE_ASSET_KIND_COUNT` (9->15, IDLE..FAINT order so `(ZM_CREATURE_ASSET_KIND)(ANIM_IDLE + eClip)` maps clip->kind); bumped `uZM_CREATUREGEN_VERSION` 2u->3u (baked `.zmodel` now carries 6 `AddAnimationPath` refs -> v2 bakes self-invalidate). (2) `ZM_CreatureGen.cpp`: 6 `ZM_CreatureBasenameFmt` cases (`"%s_Idle.zanim".."%s_Faint.zanim"`, matching `ZM_CreatureClipName`); `ZM_BakeCreature` calls `ZM_BakeCreatureClips(eId)` AFTER the mesh/texture/icon bakes (which `create_directories` the species folder -- `Flux_AnimationClip::Export` creates NO dirs, so ordering is load-bearing); the 6-clip `AddAnimationPath` loop after `SetSkeletonPath` in BOTH `.zmodel` blocks (base + shiny). (3) `ZM_CreatureAnimGen.cpp`: replaced the SC1 `return false` stubs of `ZM_BakeCreatureClips`/`ZM_BakeAllCreatureClips` with real bodies (build each clip -> `ZM_CreatureAssetPath` + `Zenith_AssetRegistry::ResolvePath` -> `Export` -> `std::filesystem::exists`).
- **Deviation (sound):** the brief said resolve via `ZM_CreatureFsPath`, but that helper is anonymous-namespace in `ZM_CreatureGen.cpp` (not cross-TU). The implementer used the header-accessible `ZM_CreatureAssetPath` (the single filename source, reusing `ZM_CreatureBasenameFmt`) + `Zenith_AssetRegistry::ResolvePath` -- the identical resolution the existing `CreatureBake_BundleFilesLand` test already uses (write-via-GAME_ASSETS_DIR, read-via-ResolvePath). Self-consistent.
- **Why:** every species' 6 clips now bake into its bundle (152 x 6 = 912 `.zanim`; per-species set 9->15 files) and each `.zmodel` self-describes its clips, so a baked creature is scene-loadable WITH its animation set -- closing the `ZM_CreatureAnimGen` Roadmap item. `ZM_CreatureContentHash` is UNCHANGED (folds mesh+albedo+shiny+icon only); creature mesh/skeleton/textures untouched.
- **Tests that lock it:** new tools-only (`#ifdef ZENITH_TOOLS`) `ZENITH_TEST(ZM_Gen, CreatureAnimBake_ClipsLandAndModelReferences)` (bakes FERNFAWN; the 6 `.zanim` land non-empty; the baked base `.zmodel` `GetNumAnimations()==6` with each `GetAnimationPath(i)` == `game:Creatures/Fernfawn/Fernfawn_<Clip>.zanim` in IDLE..FAINT order; `ZENITH_SKIP` if the bake env is unavailable). The existing `CreatureBake_BundleFilesLand` auto-extended to 15 kinds (now also asserts the 6 `.zanim`). **Full gate:** 5-config matrix (Vulkan Debug/Release x True/False + `D3D12_vs2022_Debug_Win64_False` null-backend link proof) ALL build green; boot unit gate **1866 ran / 1865 passed / 0 failed / 1 skipped** (was 1865; +1 tools-only bake smoke -- the bake smoke PASSED, confirming end-to-end bake); headless 7/0. Reviewer pass: clean. `Docs/TestPlan.md` + `Docs/AssetManifest.md` updated (clips SHIPPED, bundle 9->15 files, baseline 1866, version 3).
- **Reversibility:** moderate -- localized to the ZM_CreatureGen/ZM_CreatureAnimGen bake seam + one test; the version bump forces a cold creature re-bake (expected, self-healing). **`ZM_CreatureAnimGen` Roadmap item COMPLETE** (SC1-SC6; all 8 archetypes, 152 species x 6 clips, baked + `.zmodel`-referenced, 19 `ZM_Gen` units total). SC7 (optional windowed playability gate) may follow to prove in-scene animatability; the next unchecked S4 item is `ZM_HumanGen`.

---

## 2026-07-14 -- ZM-D-072 -- S4 ZM_CreatureAnimGen SC5: FloaterPlantoid -- all 8 archetypes wired + totality gate

- **Decision:** SC5 adds the LAST archetype builder, FLOATER-PLANTOID (a floating plant/jellyfish drifter), so `ZM_GetArchetypeAnimBuilder` is now TOTAL (every archetype returns a non-null builder). Frozen bone set (verbatim, 10 bones, NO legs): `Spine00, Spine01, Spine02, Head` (crown) `, Tendril0..Tendril5` (radial skirt). Enum value `ZM_ARCHETYPE_FLOATER_PLANTOID`. New totality test `CreatureAnimGen_AllArchetypesHaveAnimBuilder` asserts `ZM_GetArchetypeAnimBuilder(a) != nullptr` for every `a` in `[0, ZM_ARCHETYPE_COUNT)` (would have failed pre-SC5; the twin of `ZM_CreatureGen`'s dispatch/all-buildable gate). Also fixed the last stale header comment (the block above the `ZM_GetArchetypeAnimBuilder` declaration).
- **Motion:** per-tendril RADIAL tilt about each tendril's tangential axis (constant placement angle `k*2pi/6` via `glm::angleAxis`) + crown/bulb-spine flex, rotation-only. Idle = soft skirt pulse + bob; Walk = propulsive paddle (base Spine00 driven, 2x freq); Attack = uniform forward RotX lash (Head+Spine02+all tendrils, Spine01 unkeyed); Special = radial OUTWARD bloom + Spine01 (the negative-lock separator from Attack); Hit = inward flick, ends identity; Faint = monotonic wilt inward/down, holds.
- **Why:** completes the pure ALL-CONFIG anim generator -- every one of the 152 species now yields 6 valid rotation-only clips headless, all gate-verified by the archetype-generalized harness (byte-identity across species, determinism, clips-distinct, loop-wrap, one-shot-neutral, faint-clamp) + the totality gate.
- **Tests that lock it:** new `ZENITH_TEST(ZM_Gen, FloaterPlantoidAnim_ExpectedChannels)` (multiple Tendril channels + Head animate; Special-keys-Spine01 / Attack-does-not negative lock) + `CreatureAnimGen_AllArchetypesHaveAnimBuilder` (totality) + the generalized generic harness now auto-covering FLOATER-PLANTOID. Boot unit gate **1865 ran / 1864 passed / 0 failed / 1 skipped** (was 1863; +2); headless 7/0; Vulkan Debug True green. Reviewer pass: clean.
- **Reversibility:** easy -- additive final builder + one dispatch case + one totality test. **All 8 archetype builders now DONE (SC1-SC5).** Remaining for the Roadmap item: SC6 bundle-bake wiring (extend `ZM_CREATURE_ASSET_KIND` +6 anim kinds + basename cases, implement `ZM_BakeCreatureClips`, call from `ZM_BakeCreature` + `AddAnimationPath` x6 per `.zmodel`, bump `uZM_CREATUREGEN_VERSION` 2->3, tools bake smoke, TestPlan/AssetManifest doc updates, full 4-config+D3D12 matrix, baseline ratchet) = TICKS the Roadmap box; SC7 (optional) windowed playability gate.

---

## 2026-07-14 -- ZM-D-071 -- S4 ZM_CreatureAnimGen SC4: Insectoid + Blob builders (the extremes)

- **Decision:** SC4 adds the INSECTOID (19 bones, nearest the bone cap) and BLOB (4 bones, sparsest) clip builders, wired into `ZM_GetArchetypeAnimBuilder` (7 of 8 archetypes now animate; only FLOATER-PLANTOID remains nullptr). Frozen bone sets (verbatim): INSECTOID 19 = `Spine00-03, Head, LegL0Up/Lo, LegL1Up/Lo, LegL2Up/Lo, LegR0Up/Lo, LegR1Up/Lo, LegR2Up/Lo, AntennaL, AntennaR`; BLOB 4 = `Spine00-02, Nub`. Also refreshed stale comments (dispatch block + the `// later` decl markers -> real SC tags) folded into this scope since SC4 edits the dispatch anyway. No new kit primitive.
- **Insectoid metachronal tripod:** per-leg phase `{0,.5,0,.5,0,.5}` over emit order L0,L1,L2,R0,R1,R2 -> tripod A = {LegL0,LegL2,LegR1} vs tripod B = {LegL1,LegR0,LegR2}, 180deg apart (a valid alternating tripod, 3 stable legs per phase); knees (`...Lo`) lag their hips (`...Up`); baked into each per-leg `ZM_AnimAddRotCurve` lambda (no new kit fn). Special rears up + flares antennae (its signature; Attack keys neither antennae nor hind legs).
- **Blob 4-bone distinctness (the risk):** with only Spine00-02 + Nub, the generic harness `ClipsDistinct` (Idle!=Walk, Attack!=Special, Attack!=Hit, Hit!=Faint by content hash) is engineered-distinct, not lucky: varied by keyed-node set (Idle/Attack/Hit leave `Spine00` unkeyed; Walk/Special/Faint drive it), rotation axis (forward RotX vs lateral RotZ vs breathing sway), frequency (f=1 Idle vs f=2 pulse Walk), key count (Attack 5 vs Hit 4), and end pose (actions -> identity, Faint -> settled KO). The Attack/Hit pair (shared bone set) differs by axis AND key count.
- **Why:** the 8-archetype dex needs both extremes covered; the tripod + the sparse-blob distinctness prove the pure-rotation, byte-stable, generalized-harness approach scales across the whole bone-count range.
- **Tests that lock it:** new `ZENITH_TEST(ZM_Gen, InsectoidAnim_ExpectedChannels)` (tripod leg channels in Walk, antennae+head in Idle, front legs in Attack, +negative locks: Attack keys neither antennae nor `LegL2Up`) + `BlobAnim_ExpectedChannels` (spine+Nub, Idle/Attack leave `Spine00` unkeyed vs Walk/Special) + the generalized generic harness auto-covering INSECTOID/BLOB (incl. the 4-bone `ClipsDistinct`, bone-name-agnostic `FaintSettlesAndClamps`). Boot unit gate **1863 ran / 1862 passed / 0 failed / 1 skipped** (was 1861; +2); headless 7/0; Vulkan Debug True green. Reviewer pass: clean.
- **Reversibility:** easy -- additive files + two dispatch cases + comment refresh. Remaining: SC5 FloaterPlantoid + all-8 totality gate, SC6 bundle-bake wiring (ticks the Roadmap box), SC7 optional windowed gate.

---

## 2026-07-14 -- ZM-D-070 -- S4 ZM_CreatureAnimGen SC3: Serpent + Aquatic builders (limbless)

- **Decision:** SC3 adds the SERPENT and AQUATIC clip builders (both LIMBLESS) mirroring the SC1/SC2 pattern, wired into `ZM_GetArchetypeAnimBuilder` (5 of 8 archetypes now animate; INSECTOID/BLOB/FLOATER-PLANTOID still nullptr). Frozen bone sets (verbatim): SERPENT 12 = `Spine00-05, Head, Tail00-02, HornL, HornR`; AQUATIC 8 = `Spine00-02, Head, FinDorsal, FinPecL, FinPecR, FinCaudal`. New motion idiom: a per-vertebra PHASE-OFFSET travelling lateral undulation (`sin(2*pi*f*t + phase)` per spine/tail bone, phase lagging head->tail) composed entirely from the existing `ZM_AnimAddRotCurve` kit -- NO new kit primitive. SERPENT Special flares `HornL/HornR` (its signature; Attack never keys horns); AQUATIC Special flares pectoral fins + dorsal sail and never keys `FinCaudal` (the lock separating it from the Attack ram).
- **Why:** limbless archetypes drive spine-undulation / fin sweeps instead of leg gait; the phase-offset undulation still closes the loop (a phase shift inside a full 2*pi sine keeps fn(0)==fn(1)), so Idle/Walk remain pop-free.
- **Tests that lock it:** new `ZENITH_TEST(ZM_Gen, SerpentAnim_ExpectedChannels)` (multiple spine channels undulate in Walk/Idle; head-strike in Attack; Horn-flare in Special) + `AquaticAnim_ExpectedChannels` (caudal fin in Walk/Idle; pectoral in Special; no-caudal negative lock on Special) + the archetype-generalized generic harness now auto-covering SERPENT/AQUATIC (byte-identity, determinism, one-shot-neutral, faint-clamp incl. the bone-name-agnostic path). Boot unit gate **1861 ran / 1860 passed / 0 failed / 1 skipped** (was 1859; +2); headless 7/0; Vulkan Debug True green. Reviewer pass: clean.
- **Reversibility:** easy -- additive per-archetype files + two dispatch cases. Remaining: SC4 Insectoid+Blob, SC5 FloaterPlantoid + all-8 totality gate, SC6 bundle-bake wiring (ticks the Roadmap box), SC7 optional windowed gate.

---

## 2026-07-14 -- ZM-D-069 -- S4 ZM_CreatureAnimGen SC2: Biped + Avian builders + generic harness generalized

- **Decision:** SC2 adds the BIPED and AVIAN clip builders by mirroring the SC1 QUADRUPED exemplar (pure `f(archetype,clip-id)`, rotation-only, fixed channel-insert order, looping clips key0==keyN, one-shot clips end ~identity except Faint which holds its collapsed pose) and wires both into `ZM_GetArchetypeAnimBuilder` (BIPED->ZM_BuildAnim_Biped, AVIAN->ZM_BuildAnim_Avian; the other 5 archetypes stay nullptr). Frozen bone sets used (verbatim from the archetype sources): BIPED 14 = `Spine00-03, Head, ArmLUp/ArmLLo, ArmRUp/ArmRLo, LegLUp/LegLLo, LegRUp/LegRLo, Crest`; AVIAN 13 = `Spine00-02, Head, Beak, WingL, WingR, LegLUp/LegLLo, LegRUp/LegRLo, Tail00, Tail01`. Distinctive signatures: Biped `Crest` keyed ONLY in Special; Avian `Beak` keyed ONLY in Attack; wings take opposite RotZ signs for a symmetric flap/flare.
- **Also (harness generalization):** the generic `ZM_Gen` anim harness tests 4-9 (SameArchetypeByteIdentical, SameInputsDeterminism, ClipsDistinct, LoopingClipsWrapCleanly, FaintSettlesAndClamps, OneShotClipsEndNeutral) were broadened from QUADRUPED-only to loop over EVERY archetype with a wired anim builder (via new file-local helpers `WiredAnimArchetypes` / `FindTwoSpeciesOfArchetype`), realizing the design's "coverage auto-grows per SC" -- SC3-SC5 archetypes are now auto-exercised the moment they wire in. `FaintSettlesAndClamps` was de-hardcoded off the `"Spine01"` bone name (it now iterates all channels: every channel clamps past the end, at least one genuinely collapses) so it will not break for the future spineless BLOB/FLOATER-PLANTOID. Assertion semantics preserved; ZENITH_TEST count unchanged at 9 for the generic file.
- **Why:** pattern replication keeps every archetype's clips consistent and cheap; generalizing the harness makes the byte-identity + one-shot-neutral + faint-clamp contracts a suite-wide invariant rather than a quadruped accident.
- **Tests that lock it:** new `ZENITH_TEST(ZM_Gen, BipedAnim_ExpectedChannels)` + `AvianAnim_ExpectedChannels` (per-archetype structural locks incl. negative locks: ATTACK !ArmLUp for biped, SPECIAL !Beak for avian) + the now-generalized generic harness running over BIPED/AVIAN species (ChannelsMatchSkeleton + ValidationPasses already looped all buildable species). Boot unit gate **1859 ran / 1858 passed / 0 failed / 1 skipped** (was 1857; +2); headless 7/0; Vulkan Debug True build green (SC2 adds no new engine dependency beyond the all-config `Flux_AnimationClip` already proven at SC1, so the cross-config matrix was not re-run for this sub-commit). Reviewer pass: clean.
- **Reversibility:** easy -- additive per-archetype files + two dispatch cases + broadened test loops. Remaining: SC3 Serpent+Aquatic (limbless), SC4 Insectoid+Blob, SC5 FloaterPlantoid + all-8 totality gate, SC6 bundle-bake wiring (ticks the Roadmap box), SC7 optional windowed gate.

---

## 2026-07-14 -- ZM-D-068 -- S4 ZM_CreatureAnimGen: clip-set ruling + pure rotation-only seam (SC1 landed)

- **Decision (clip set):** the 6 per-archetype clips are **Idle / Walk / Attack / Special / Hit / Faint**, per the canonical enumeration in `AssetManifest.md` (section "Creature animation clips"), NOT the design panel's proposed `Cheer` variant. The design's own open question flagged AssetManifest as the tiebreaker; a battle-facing set (Attack + Special covering the physical/special move split, Hit for flinch, Faint for KO) directly serves the shipped S2 `ZM_BattleEngine`, the real runtime consumer, and keeps the reference docs consistent. Golden metadata pinned as literal asserts (a version-bump contract): durations `{Idle 2.0, Walk 1.0, Attack 0.7, Special 0.9, Hit 0.4, Faint 1.2}` s; looping `{T,T,F,F,F,F}`; ticksPerSecond `24`.
- **Decision (architecture):** `ZM_CreatureAnimGen` is a NEW pure, ALL-CONFIG generator (Source/Gen/) that authors the 6 clips ONCE per archetype against the FROZEN per-archetype creature skeletons (bone names identical across every evo stage + species), then instantiates per species (6 x 152 = ~912 `.zanim`). A clip is a PURE closed-form function of `(archetype, clip-id)` ONLY -- NO RNG, NO species/recipe/seed input -- so `ZM_BuildCreatureClip(arch, clip, out)` is byte-identical across all species of an archetype (the leverage, proved directly). Clips are STRICTLY ROTATION-ONLY in v1: bind-local rotation is identity for all species so absolute rotation is species-safe, but a channel REPLACES bind-local TRS (not a delta) and bind-local POSITION varies per species by size class, so position/scale channels would teleport a bone or break cross-species purity -- vertical bob/collapse is expressed via spine-flexion rotation. `ZM_GEN_DOMAIN_ANIM` stays reserved+unused in v1. Reuses the engine's headerless `Flux_AnimationClip::Export` / `WriteToDataStream` verbatim -- **no engine change, no new asset format**. `uZM_CREATUREANIMGEN_VERSION = 1u`.
- **Why (over alternatives):** per-species RNG jitter was rejected for v1 -- byte-identity across species is the strongest reading of the Status.md leverage note and gives a trivial determinism proof; it can be revisited later drawing ONLY from the reserved `ZM_GEN_DOMAIN_ANIM`. Position/scale channels rejected as above. A bespoke `.zanim` writer rejected -- the engine path already serializes clips.
- **SC1 landed (this commit):** the frozen seam header (`ZM_CreatureAnimGen.h`: version consts, PINNED `ZM_ANIM_CLIP` enum, golden accessors, `ZM_ArchetypeAnimFn` typedef + 8 builder decls + `ZM_GetArchetypeAnimBuilder`, pure `ZM_BuildCreatureClip`, `ZM_CreatureClipBytesEqual/ContentHash`, `ZM_ValidateCreatureClip`, tools-only `ZM_BakeCreatureClips` decl + non-tools no-op), the shared curve kit (`ZM_CreatureAnimCommon.{h,cpp}`: `ZM_AnimRotX/Y/Z`, `ZM_AnimAddRotCurve` template, `ZM_AnimAddRotKeys`), the pure driver, and the QUADRUPED builder (`ZM_CreatureAnimArchetype_Quadruped.cpp`, 18-bone set). The other 7 archetype builders are declared-but-unwired (`ZM_GetArchetypeAnimBuilder` returns nullptr for them, so downstream harness coverage auto-grows); the tools bake is a `return false` STUB (real bundle bake = SC6). NO `ZM_CreatureGen.*` edits in SC1.
- **Tests that lock it:** `ZENITH_TEST(ZM_Gen, CreatureAnimGen_*)` -- ChannelsMatchSkeleton (every channel binds a real skeleton bone via `ZM_GenMeshFindBone`, over every buildable species), ValidationPasses, ClipMetadataGolden (the literal golden pins), SameArchetypeByteIdentical (two distinct quadruped species -> equal bytes+hash, the leverage), SameInputsDeterminism, ClipsDistinct, LoopingClipsWrapCleanly, FaintSettlesAndClamps (clip-end clamp-not-extrapolate), OneShotClipsEndNeutral (Attack/Special/Hit resolve to identity; Faint does not) + `QuadrupedAnim_ExpectedChannels` (per-clip bone-signature locks incl. negative locks). Boot unit gate **1857 ran / 1856 passed / 0 failed / 1 skipped** (was 1847; +10); headless 7/0; builds green on Vulkan Debug True + Debug False + the D3D12 Debug False null-backend link proof (all-config purity confirmed). Reviewer pass: clean (no blockers/majors; two minor test-coverage findings applied).
- **Reversibility:** moderate. New additive module localized to `Source/Gen/ZM_CreatureAnim*` + `Tests/ZM_Tests_CreatureAnim*`; the clip-set + goldens become a version-bump contract once pinned. Remaining sub-commits: **SC2** Biped+Avian, **SC3** Serpent+Aquatic, **SC4** Insectoid+Blob, **SC5** FloaterPlantoid + all-8-wired totality gate, **SC6** bundle-bake wiring (extend `ZM_CREATURE_ASSET_KIND` +6 anim kinds + basename cases, implement `ZM_BakeCreatureClips`, call it from `ZM_BakeCreature` after texture bakes + `AddAnimationPath` x6 in both `.zmodel` blocks, bump `uZM_CREATUREGEN_VERSION` 2->3, tools bake smoke, ratchet the baseline in zm-tests.yml/Status/TestPlan/Roadmap) -- ticks the Roadmap box; **SC7** (optional) windowed playability gate.

---

## 2026-07-14 -- ZM-D-067 -- S4 ZM_CreatureGen VISUAL GATE SIGNED OFF (user APPROVED; reflective floor + punchier colours delivered)

- **Verdict:** the user reviewed the S4 species-gallery evidence and APPROVED the creature generator, with two showcase/art enhancements requested at the gate and delivered: (1) a smooth metallic reflective SHOWCASE FLOOR under the creatures (commit 3dcfcf74 -- a 60x60 metallic slab + SSR + tilted cameras, so the creatures mirror on the floor); (2) PUNCHIER creature COLOURS (this commit). With both delivered + all automated items green, the **S4 `ZM_CreatureGen` visual gate is PASSED**; the loop resumes at `ZM_CreatureAnimGen`.
- **Punchier-colours change:** a creature-scoped, deterministic HSV saturation boost -- `fZM_CREATURE_ALBEDO_SATURATION_BOOST = 1.6f` (+60%) applied via a new `SaturateColour` helper (RGB->HSV, x S, clamp, ->RGB; HUE + VALUE preserved) to the RESOLVED palette (m_xBase/m_xAccent/m_xBelly) inside `ZM_SynthCreatureAlbedo`. The raw `ZM_SynthTypePalette`/`ZM_SynthBlendPalette` tables are UNTOUCHED, so non-creature palette users + the palette-table unit tests are unaffected; shiny (hue-rotate) + dex icon (downsample) inherit it automatically. Measured: mean creature-pixel saturation ~0.036 -> ~0.09-0.11 (~2.7x); ~11-15% of subject pixels now clearly coloured (was ~0%). The factor is a named constant -- further art direction is a one-number tune + re-bake.
- **Determinism preserved; NO golden re-baseline needed:** the boost is a fixed deterministic transform (same id -> same bytes). Every affected test is same-seed-determinism / property / structural and stays green; no test pins a fixed albedo/shiny/icon RGB or content-hash literal that the change alters (the one packed-byte golden comes from an explicit fill, not the palette). Boot unit gate held at **1847 ran / 0 failed**.
- **Version stamps bumped (stale bakes self-invalidate):** `uZM_CREATUREGEN_VERSION` 1 -> 2 + `uZM_TEXTURESYNTH_VERSION` 1 -> 2. The gallery re-bakes its dozen at setup; a full re-bake at the eventual `ZM_BakeManifest` box picks up the rest.
- **Gate automated results (all green):** 4-config Vulkan matrix (Debug/Release x True/False) + D3D12_False null-backend link proof build; unit gate 1847/0; headless 6/0; windowed `ZM_CreatureGallery_Test` PASSED. Final evidence (reflective floor + punchier colours): `Build/artifacts/zenithmon/s4/visual/gallery_0{1,2,3}.tga`/`.png`.
- **Result:** S4 `ZM_CreatureGen` COMPLETE + gate-signed (all 8 archetypes, 152 species, 9-file scene-loadable bundles, ~1847 units). Roadmap `ZM_CreatureGen` box ticked `[x]`; Status GATE-WAIT cleared.

---

## 2026-07-14 -- ZM-D-066 -- S4 ZM_CreatureGen SC5c: species-gallery visual gate (GATE-WAIT for sign-off)

- **Trigger:** the final S4 deliverable -- the windowed species-gallery visual gate, ZM_CreatureGen's stage-gate visual check. Authored by a subagent (windowed gallery test), iterated by the orchestrator to a gate-ready render, gated serially, format docs refreshed.
- **Decision:** SC5c adds Tests/ZM_AutoTests_Gallery.cpp -- a windowed ZENITH_AUTOMATED_TEST_REGISTER test (ZM_CreatureGallery_Test, m_bRequiresGraphics=true so it auto-skips headless) that bakes a diverse sampled dozen (>=1 per archetype + Zenithrax shown SHINY), places their baked .zmodel models in a framed 4x3 grid, and dumps 3 TGAs (front + left/right 3/4) to Build/artifacts/zenithmon/s4/visual/ via Flux_Screenshot::RequestDump. Look tuned via Zenith_GraphicsOptions saved-in-Setup/restored-in-cleanup: bloom OFF, auto-exposure OFF (fixed exposure 1.0), key/fill directional lights at O(1) lux (anchored on Flux_MaterialPreview's sun=3.0), a neutral studio backdrop (skybox off), and FrontEnd UI (title/quads) suppressed to kill scene bleed. The test hard-asserts all 12 models render + all 3 TGAs land.
- **Automated S4 gate -- ALL GREEN:** the 4-config Vulkan matrix (Debug/Release x True/False) + the D3D12_vs2022_Debug_Win64_False null-backend link proof all build; boot unit gate 1847 ran / 0 failed; `zenith test Zenithmon --headless` 6/0 (gallery skips headless); windowed ZM_CreatureGallery_Test PASSED (3 TGAs captured). The format-doc debt deferred since ZM-D-059 is refreshed in this commit (TestPlan ZM_Gen creature group, AssetManifest creature-bundle catalogue, Shortfalls verdict).
- **Visual read (orchestrator, pre-sign-off):** all 12 baked creatures load + render as distinct, correctly-proportioned models across all 8 archetypes, each with working eyes + a type-palette colour + the shiny variant, on a clean background. Flagged to the user: the palette reads SOFT/PASTEL (low saturation) -- a punchier look is an available follow-up (dim the gallery further / IBL-diffuse off, or raise ZM_SynthCreatureAlbedo saturation for the baked assets = a generator-version bump). Iterated the gallery lighting 3x to get from an initial overexposed white bloom (auto-exposure lifting mid albedos into AgX's desaturate-toward-white zone) to the calibrated fixed-exposure render.
- **GATE-WAIT SET; STOPPED for user sign-off (hard-stop policy).** This entry records the automated gate PASS + the evidence; the human's APPROVE/REJECT verdict lands as a SEPARATE later DecisionLog entry via StartPrompts prompt 4. The loop is parked (Status.md GATE-WAIT marker) and every firing reports "waiting" until then. Do NOT tick the S4 ZM_CreatureGen Roadmap line without the sign-off.
- **Reversibility:** High for the gallery test (a windowed evidence artifact; deletable). The gate VERDICT is the user's; if REJECTED the rework (palette / per-species) is additive/retunable.

---

## 2026-07-14 -- ZM-D-065 -- S4 ZM_CreatureGen SC5b: .zmtrl + .zmodel bundle bake (creatures now scene-loadable)

- **Trigger:** the SC5-deferred bundle bake -- ZM_BakeCreature baked mesh+skeleton+textures but stubbed the material/model writes (that author API was unsurveyed at design time). Flow: read-only survey subagent -> implementer -> orchestrator gate (incl. the D3D12_False link proof) -> reviewer.
- **Decision:** ZM_BakeCreature (TOOLS-only) now writes the full 9-file per-species bundle. Added: base + shiny .zmtrl (Zenith_MaterialAsset::SaveToFile, schema v5) + base + shiny .zmodel (Zenith_ModelAsset::Export, schema v2), via the Zenith_AssetRegistry::Create<T>()+GetDirect() owning-handle pattern. SHINY is an INDEPENDENT material (its own _shiny.zmtrl -> _shiny.ztxtr), NOT a child of the base -- the shiny albedo is a fully-baked independent texture (inheritance would override 100% of what differs) and the child path triggers a registry load of the base .zmtrl by ref at bake time (an ordering/IO hazard). Discipline: the write target (SaveToFile/Export arg) is a FILESYSTEM path (ZM_CreatureFsPath); every ref EMBEDDED in an asset (albedo/shiny tex, mesh, skeleton, material) is a game: ref (ZM_CreatureAssetPath); one material per submesh (creature mesh is single-submesh); all 4 writes verified landed via std::filesystem::exists (SaveToFile always returns true, Export returns void).
- **Test:** new TOOLS-gated Tests/ZM_Tests_CreatureBake.cpp -- CreatureBake_BundleFilesLand bakes FERNFAWN and asserts all 9 bundle files exist + non-empty; ZENITH_SKIP if the bake env is unavailable. The full byte-identical re-bake invariant stays deferred to the later ZM_BakeManifest box (Roadmap S4).
- **Reviewer:** ship-ready, no findings -- API fidelity (every Zenith_MaterialAsset/Zenith_ModelAsset/Zenith_AssetRegistry signature matches), FS-vs-ref discipline, the #ifdef ZENITH_TOOLS boundary (all new includes + code inside the guard), the IO-exists verify, the smoke test, comment-only refreshes, scope ALL clean. Nit for SC5c: a full ZM_BakeAllCreatures leaves ~152x4 procedural material/model assets at refcount 0 (reclaimed by UnloadUnused) -- mild reclaimable memory pressure, not a leak.
- **Also:** refreshed the now-stale "SC1 wires ONLY QUADRUPED" comments in ZM_CreatureGen.h/.cpp + ZM_Tests_CreatureGen.cpp (all 8 wired) -- comment-only, no declaration change (reviewer-verified).
- **Tests-that-lock-it:** boot unit gate **1846 -> 1847** (0 failed; +1 bake smoke; baseline bumped in .github/workflows/zm-tests.yml too). Vulkan_True build + D3D12_vs2022_Debug_Win64_False link proof both green; `zenith test Zenithmon --headless` 6/0.
- **Milestone:** a baked creature is now a COMPLETE, scene-loadable asset bundle. Remaining S4: SC5c = bake all 152 creatures (ZM_BakeAllCreatures) + the windowed species-gallery visual gate (the S4 GATE hard-stop) + the S4-deferred format-doc refresh.
- **Reversibility:** Moderate. The bake output format (.zmtrl v5 / .zmodel v2 / the game: ref scheme) is now a real on-disk contract S5+ scene-loading depends on; changing it needs a cold re-bake. The in-memory generation is unchanged.

---

## 2026-07-14 -- ZM-D-064 -- S4 ZM_CreatureGen SC5a: FLOATER-PLANTOID builder + all-152 coverage gate (all 8 archetypes wired)

- **Trigger:** the FINAL archetype builder + the "coverage complete" milestone. Authored by one subagent against the frozen seam; the orchestrator wired the last switch case, added the all-152 gate, gated serially, ran a reviewer.
- **Decision:** SC5a adds the FLOATER-PLANTOID builder + per-archetype test, wires the last ZM_GetArchetypeBuilder case, and adds CreatureGen_AllSpeciesBuildable to the shared harness. FLOATER-PLANTOID: a fixed 10-bone floating creature -- a 3-node bulb body (Spine00..02 [root], centred above the ground so it FLOATS), a Head crown, and 6 RADIAL round tendrils (Tendril0..5) via a builder-local ZM_FloaterAppendTendril helper (modeled on ZM_AvianAppendWing). ROUND (Rx==Rz) tendrils were chosen over flat petals because the loft rings are axis-aligned and cannot rotate -- a round section is rotation-invariant so the 6-fold radial symmetry reads cleanly; the 6 angles are FIXED constants k*(2pi/6), never rng. NO legs. The floating invariant (mesh min-Y bound > 0) is structural and reviewer-verified analytically (lowest tendril tip ~0.215*fS). With all 8 archetypes wired, CreatureGen_AllSpeciesBuildable asserts EVERY of the 152 species resolves to a non-null builder -- proving the switch covers every ZM_ARCHETYPE and the generic 12-invariant harness now runs over the FULL dex.
- **Reviewer:** no blockers/majors -- determinism, the floating invariant (traced), the local tendril helper (real signatures, no dangling pointer, no flat-washer), foundation-fidelity, the all-152 gate + dispatch, conventions, and scope ALL clean. A few PRE-EXISTING stale doc-comments ("SC1 wires ONLY QUADRUPED" in ZM_CreatureGen.h / ZM_Tests_CreatureGen.cpp / the ZM_BakeAllCreatures skip note) are now inaccurate -- cosmetic, deferred to SC5b/SC5c which edit those files anyway (no behavior impact; the code paths are SC-agnostic).
- **Tests-that-lock-it:** boot unit gate **1842 -> 1846** (0 failed; baseline bumped in .github/workflows/zm-tests.yml too; +3 FLOATER-PLANTOID tests + 1 all-152 gate). The generic 12-invariant harness now covers ALL 152 species. `zenith test Zenithmon --headless` 6/0. No stale-test churn.
- **Milestone:** creature MESH + skeleton + albedo/shiny/dex-icon generation is now FEATURE-COMPLETE IN-MEMORY for the full dex. Remaining S4: SC5b = the deferred .zmtrl/.zmodel bundle bake in ZM_BakeCreature (needed to make baked creatures scene-loadable); SC5c = bake-all + the windowed species-gallery visual gate (the S4 GATE hard-stop for user sign-off).
- **Reversibility:** High. New per-archetype code + 1 switch case + 1 test; no baked assets. Golden pins per builder documented in the .cpp; a change bumps uZM_CREATUREGEN_VERSION.

---

## 2026-07-14 -- ZM-D-063 -- S4 ZM_CreatureGen SC4: INSECTOID + BLOB archetype builders

- **Trigger:** next SC of ZM_CreatureGen after SC3. Two archetypes -- the bone-count EXTREMES -- authored in parallel by disjoint subagents; the orchestrator wired the dispatch switch, gated serially, ran a reviewer.
- **Decision:** SC4 adds the INSECTOID and BLOB builders + per-archetype tests, wired into the explicit ZM_GetArchetypeBuilder switch (no header edit). INSECTOID (the HIGH-limb extreme): a fixed 19-bone bug -- Spine00..03 segmented thorax/abdomen [root], Head, SIX 2-bone legs via ZM_AppendLimb (LegL0/L1/L2/R0/R1/R2 -- exactly 6 '...Up' roots, parented to thorax/mid/rear spine), and 2 antennae via ZM_AppendHorn -- comfortably <=30 (asserted in-builder). BLOB (the LOW-bone extreme): a fixed 4-bone gelatinous body -- a 3-node ZM_AppendSpineTube driving m_fBellyRound (-> ZM_LoftRing m_fSuperEllipse) for a soft box-rounded silhouette + a single crown Nub via ZM_AppendHorn, no limbs. Both draw all rng up-front (MESH then SKELETON) via ZM_MakeGenRNG (the 6-leg + antenna loops REUSE pre-drawn values -- no per-iteration draws), scale by m_fSizeScale, keep topology IDENTICAL across evo stages (elaboration scales sizes only), and never finalise/bake.
- **Reviewer:** no blockers/majors/minors -- 3 cosmetic nits only (NOT fixed: a comment conflating m_fBellyRound/m_fSuperEllipse [code correct]; two slightly-loose test-subset comments; a guarded auSpine[uSpineCount-3] latent-underflow that mirrors the Quadruped idiom and cannot trigger given the >=4-segment assert). Determinism, the INSECTOID bone budget (19, asserted <=30, exactly 6 leg roots, valid parenting), BLOB [2,4] bones + the real m_fBellyRound field, foundation-fidelity, conventions, and scope ALL verified clean.
- **Tests-that-lock-it:** boot unit gate **1836 -> 1842** (0 failed; baseline bumped in .github/workflows/zm-tests.yml too). The universal 12-invariant harness now also runs over the INSECTOID + BLOB species; each archetype adds a structural bone assert (INSECTOID: single Spine root + Head + EXACTLY 6 leg '...Up' roots + antennae + <=30 total; BLOB: single root + total bones in [2,4] + zero limb bones). `zenith test Zenithmon --headless` 6/0. No stale-test churn.
- **Reversibility:** High. New per-archetype code under Source/Gen/ + Tests/; the only shared-file touch is the 2 switch cases. No baked assets. Golden pins per builder documented in each .cpp; a change bumps uZM_CREATUREGEN_VERSION.

---

## 2026-07-14 -- ZM-D-062 -- S4 ZM_CreatureGen SC3: SERPENT + AQUATIC archetype builders

- **Trigger:** next SC of ZM_CreatureGen after SC2. Two archetypes authored in parallel by disjoint subagents against the frozen seam; the orchestrator wired the dispatch switch, gated serially, and ran a reviewer.
- **Decision:** SC3 adds the SERPENT and AQUATIC builders + per-archetype tests, wired into the explicit ZM_GetArchetypeBuilder switch (no header edit -- all 8 builders were declared at SC2). SERPENT: a limbless upright/rearing snake -- a fixed 12-bone skeleton (Spine00..05 six-vertebra body tube [root], Head with a +Z snout, Tail00..02 tapering to a point, HornL/R brow frills) built ENTIRELY from the shared kit (no local loft helper needed -- every serpent part is a round Y-swept shape). AQUATIC: a fixed 8-bone fish (Spine00..02 streamlined body [root], Head, FinDorsal, FinPecL/R, FinCaudal) using the kit PLUS one archetype-LOCAL helper ZM_AquaticAppendFin (a flat thin-Rx/broad-Rz blade generalized from AVIAN's ZM_AvianAppendWing to serve up- AND down-swept fins; single-bone ring skin, valid winding both sweep directions, no flat-washer, no dangling ring pointer). Both draw all rng up-front (MESH then SKELETON) via ZM_MakeGenRNG, scale by m_fSizeScale, keep bone topology IDENTICAL across evo stages (elaboration scales horn/fin SIZE only), stay <=30 bones, and never finalise/bake.
- **Reviewer:** no blockers/majors/minors -- determinism, the AQUATIC local fin helper (verified against the real ZM_GenCommon loft signatures), SERPENT limblessness + within-cap spine chain, foundation-fidelity, conventions, and scope ALL clean. Two benign observations (no action): the builders locally re-derive the kit's spine-node world formula + mirror the kit's private add-bone helper -- the sanctioned anon-namespace pattern (kit internals are un-linkable), bind-pose-neutral.
- **Tests-that-lock-it:** boot unit gate **1830 -> 1836** (0 failed; baseline bumped in .github/workflows/zm-tests.yml too). The universal 12-invariant harness now also runs over the SERPENT + AQUATIC species; each archetype adds a structural bone assert (SERPENT: single Spine root + Spine/Tail chain + Head + ZERO limb '...Up' bones; AQUATIC: single Spine root + Head + dorsal/2 pectoral/caudal fin bones). `zenith test Zenithmon --headless` 6/0. No stale-test churn -- the SC2 SC-agnostic dispatch test absorbed the newly-wired archetypes.
- **Reversibility:** High. New per-archetype code under Source/Gen/ + Tests/; the only shared-file touch is the 2 switch cases. No baked assets. Golden pins per builder (jitter ranges, proportions, bone layout) documented in each .cpp; a change bumps uZM_CREATUREGEN_VERSION.

---

## 2026-07-14 -- ZM-D-061 -- S4 ZM_CreatureGen SC2: BIPED + AVIAN archetype builders

- **Trigger:** next SCs of ZM_CreatureGen after SC1 (QUADRUPED). Two archetypes authored in parallel by disjoint subagents against the now-frozen seam; the orchestrator wired the dispatch switch, gated serially, and ran a reviewer.
- **Decision:** SC2 adds the BIPED and AVIAN archetype builders (each ALONE in ZM_CreatureArchetype_<Name>.cpp) + per-archetype tests, wired into the explicit ZM_GetArchetypeBuilder switch. BIPED: a fixed 14-bone upright skeleton (Spine00..03 root chain -> Head, ArmL/R Up/Lo from the shoulders, LegL/R Up/Lo from the pelvis, a dorsal Crest) built entirely from the shared kit. AVIAN: a fixed 13-bone skeleton (Spine00..02 root -> Head -> Beak, WingL/R, LegL/R Up/Lo, Tail00..01) from the kit PLUS one archetype-LOCAL helper ZM_AvianAppendWing (a flat thin-Rx/broad-Rz swept wing blade the round-tube ZM_AppendLimb cannot express; single-bone ring skin, outward winding, no flat-washer, verified against the real loft signatures). Both draw all randomness up-front in a fixed MESH-then-SKELETON order via ZM_MakeGenRNG, scale by m_fSizeScale, keep bone topology IDENTICAL across evo stages (elaboration scales crest/beak/wing/tail SIZE only), stay <=30 bones, and never finalise/bake.
- **Seam finalisation (tech-lead append, now complete):** the frozen ZM_CreatureGen.h had declared only the QUADRUPED builder; SC2 appends the remaining 7 builder declarations -- a pure, transparent append (changes no existing declaration). This is the ONE sanctioned change to the frozen header; SC3-SC5 add ONLY a new .cpp + one switch case, never a header edit.
- **Test-harness fix (orchestrator, single-writer):** the SC1 dispatch test CreatureGen_ArchetypeDispatch hard-coded "only QUADRUPED wired" and reddened once BIPED/AVIAN landed; rewritten SC-AGNOSTIC (dispatch is pure + total over ZM_ARCHETYPE, QUADRUPED always routes, the ZM_ARCHETYPE_COUNT sentinel does not, >=1 archetype + >=1 species buildable) so it never goes stale as the wired set grows. The generic ZM_Gen harness auto-covers the new BIPED/AVIAN species (it loops non-null builders).
- **Reviewer:** no blockers/majors -- determinism, the AVIAN local wing helper, foundation-fidelity, and scope all clean. One MINOR fixed in this commit: the BIPED per-archetype tests gained a top-level HasBipedBuilder() early-return (mirroring AVIAN) so they no-op rather than false-fail if BIPED were ever unwired.
- **Tests-that-lock-it:** boot unit gate **1824 -> 1830** (0 failed; baseline bumped in .github/workflows/zm-tests.yml too). The universal 12-invariant harness now also runs over the BIPED + AVIAN species; each archetype adds a structural bone assert. `zenith test Zenithmon --headless` 6/0.
- **Reversibility:** High. New per-archetype code under Source/Gen/ + Tests/; the header append + 2 switch cases + the dispatch-test rewrite are the only shared-file touches. No baked assets. Golden pins per builder (jitter ranges, reference proportions, bone layout) documented in each .cpp; a change bumps uZM_CREATUREGEN_VERSION.

---

## 2026-07-14 -- ZM-D-060 -- S4 ZM_CreatureGen SC1: frozen seam + core driver + shared kit + QUADRUPED reference builder

- **Trigger:** next unchecked S4 task ("ZM_CreatureGen -- all 8 archetypes ..."), the single biggest S4 work item (~7-8.5k lines). Deliberately broken into sub-commits SC1..SC5 rather than one landing. Design produced by a survey -> 3-architect panel -> synthesis workflow; the frozen header was vetted by the orchestrator against the real foundation (all `ZM_GEN_DOMAIN` members present, every symbol/signature confirmed), then authored by parallel subagents (foundations -> [quadruped builder + tests]) and gated serially by the orchestrator.
- **Decision:** SC1 lands the FROZEN public seam `Source/Gen/ZM_CreatureGen.h`, the core driver `ZM_CreatureGen.cpp`, the shared append-kit `ZM_CreatureArchetypeCommon.{h,cpp}`, the ONE reference archetype (QUADRUPED, `ZM_CreatureArchetype_Quadruped.cpp`), and the full generic test harness. The seam:
  - `ZM_ResolveCreatureRecipe(id)` reads `ZM_SpeciesData` ONCE into a read-only `ZM_CreatureRecipe` that PRE-DERIVES one PCG seed per `ZM_GEN_DOMAIN` into `m_aulDomainSeed[]`. An archetype builder receives only `(ZM_GenMesh&, const ZM_CreatureRecipe&)` and reaches randomness ONLY through `ZM_MakeGenRNG(recipe, domain)` -- the entropy door (`ZM_GenRNG`'s default ctor is deleted upstream), so the determinism contract is STRUCTURAL, not a review convention.
  - Dispatch is an EXPLICIT switch `ZM_GetArchetypeBuilder(archetype)` (never a self-registering table -- MSVC static-init dead-strip would silently drop archetypes from the static-linked TUs). SC1 wires ONLY QUADRUPED; every other archetype returns `nullptr`, so the generic harness auto-grows coverage as each SC adds a case.
  - `ZM_BuildCreatureMesh` owns the ONE finalise order (`ZM_GenGenerateTangents -> ZM_GenNormalizeSkinWeights`; analytic loft normals kept, never regenerated), debug-asserts `ZM_ValidateGenMesh` at the 30-bone creature cap. `ZM_BuildCreature` produces the full in-memory `ZM_Creature` bundle (mesh+skeleton + BC1 albedo + hue-rotated shiny + flat dex icon). The equality/hash trio + `ZM_ValidateCreature` (fills every S4 gate flag) make the mandated tests one-liners.
  - The kit binds each authored ring to a SINGLE bone (`blendB=0`), so the loft's Catmull-Rom subdivision is the ONLY source of the <=2-bone blends -- influence count provably never exceeds 2. Helpers: `ZM_AppendSpineTube/Limb/Tail/Horn/EllipsoidHead` + `ZM_SizeClassScale` + `ZM_FormatBoneName`.
  - QUADRUPED builds a fixed 18-bone skeleton (Spine00..03 root chain -> Head, LegFL/FR/HL/HR Up/Lo, Tail00..02, HornL/HornR), with IDENTICAL bone names/order/count across all evo stages so index-keyed clips transfer; elaboration (`m_uEvoStage-1`) only grows horn size, never topology.
- **Golden-pinned by SC1 (a change = `uZM_CREATUREGEN_VERSION` bump + cold family re-bake):** the size-class scale curve TINY .45 / SMALL .70 / MEDIUM 1.00 / LARGE 1.50 / HUGE 2.20; the primary-type -> pattern-kind table (STRIPES{FIRE,ELECTRIC,BRAWL,DRAKE} / SPOTS{GRASS,VENOM,SWARM,FEY} / GRADIENT{WATER,ICE,SKY,MIND} / BELLY{NORMAL,EARTH,STONE,IRON} / NONE{PHANTOM,UMBRAL}) + tier-scaled pattern params; the shiny hue band [80,280) deg (SHINY domain, `m_fJitter` stays 0/dormant); eye U/V 0.5/0.35 + radius [0.05,0.075] (EYE domain); dex-icon = box-downsample of albedo over a primary-type-tinted background (DEX_ICON domain, tint mix [0.25,0.35]); the asset scheme `game:Creatures/<Name>/<Name><suffix>.<ext>`; and the QUADRUPED jitter ranges / reference proportions / horn-size-by-tier curve. Domain usage: ALBEDO drives the single `ZM_SynthCreatureAlbedo` rng; PATTERN is reserved and UNCONSUMED in SC1 (pattern KIND derived purely, no draw).
- **Deferred to SC5 (logged):** `ZM_BakeCreature` (TOOLS-only) bakes mesh+skeleton (`ZM_GenBakeMesh`) + albedo/shiny (`ZM_SynthBakeAlbedoBC1` x2) + icon (`ZM_SynthBakeIconBC1`); the `.zmtrl` (normal + shiny child material) and `.zmodel` bundle writes are a commented `// SC5:` TODO -- a baked SC1 species is not yet a scene-loadable finished creature. The other 7 archetype builders + the all-152 coverage gate + the windowed species-gallery visual gate are SC2 (BIPED+AVIAN) / SC3 (SERPENT+AQUATIC) / SC4 (INSECTOID+BLOB) / SC5 (FLOATER-PLANTOID + gate + gallery).
- **Tests-that-lock-it:** boot unit gate **1804 -> 1824** (0 failed; +20 `ZM_Gen` creature units across `Tests/ZM_Tests_CreatureGen.cpp` + `Tests/ZM_Tests_CreatureArchetype_Quadruped.cpp`; baseline bumped in `.github/workflows/zm-tests.yml` too). A generic parameterized harness runs the 12 universal invariants over every buildable (QUADRUPED-in-SC1) species -- same-seed byte determinism + equal content hash; per-domain seed isolation (all 152); outward winding; non-degenerate bounds within a sane box; weights sum-to-1; <=2 influences; bone caps <=30 & <=100; in-range indices + well-formed single-root/parent<child skeleton; shiny differs same-dims + shared mesh; icon non-empty + >=2 distinct texels; distinct-species + stage1-vs-stage3 sensitivity; and evo-stage topology IDENTICAL (bone count + per-index names, over every multi-stage family) -- each build guarded on a non-null builder (ZM_BuildCreature is void + asserts a non-null builder). Plus golden-locks for the size-scale curve, the asset-path ref scheme + truncation, `ZM_FormatBoneName`, and the shiny band. Reviewer verdict: no blockers/majors -- correctness, determinism, and foundation-fidelity verified against the real loft/synth source (`ZM_LerpRingSkin`/`ZM_EmitAndStitch`).
- **Reconciliations (found during authoring, no rework):** `ZM_BuildCreature` is `void` + asserts a non-null builder (un-wired archetypes proven via `ZM_GetArchetypeBuilder==nullptr`, never by invoking the build); include convention is game-root-relative (`#include "Zenithmon/Source/Gen/..."`, matching the shipped modules); `ZM_CreatureAssetPath` returns the canonical `game:` ref while a tools-only mirror writes the FS path under `GAME_ASSETS_DIR`.
- **Reversibility:** High. All new `ZM_`-prefixed code under `Source/Gen/` + `Tests/`; no engine/foundation edits; no baked assets. The one hard commitment is the golden-pinned generation algorithm (above). The header is FROZEN after SC1 (append-only, tech-lead-only) so the 7 remaining archetype authors work disjointly against it.

---

## 2026-07-14 -- ZM-D-059 -- S4 asset-gen foundation: ZM_GenCommon + ZM_TextureSynth frozen (pure library + tools-only bake bridges)

- **Trigger:** first unchecked S4 task ("ZM_GenCommon (seeded RNG + loft toolkit) + ZM_TextureSynth"). Design produced by a survey -> 3-architect panel -> synthesis workflow; approved by the orchestrator after verifying the load-bearing engine seams exist.
- **Decision:** The two S4 foundation modules live under `Games/Zenithmon/Source/Gen/` (not `Tools/`). The PURE deterministic library -- `ZM_GenRNG`, seed derivation (`ZM_GenHashName`/`ZM_GEN_DOMAIN`/`ZM_GenDeriveSeed`), `ZM_GenNoise`, the loft toolkit building into a POD `ZM_GenMesh`, `ZM_GenImage`, and all texel synthesis -- is compiled in EVERY config with **no `ZENITH_TOOLS` guard** and zero engine-asset/GPU/disk coupling. Only the disk-bake bridges (`ZM_GenBakeMesh`, `ZM_SynthBake*`) are `#ifdef ZENITH_TOOLS`, each with a non-tools inline no-op so `_False` builds link. `ZM_GenRNG` WRAPS `ZM_BattleRNG`'s golden PCG32 (ZM-D-027) -- never re-implements it -- deletes its default ctor to force explicit ID-derived seeding, and adds a fixed integer->float `NextFloat01`. The loft fills a `ZM_GenMesh` whose SoA buffers mirror `Zenith_MeshAsset`; the bake bridge copies element-wise without re-deriving normals/tangents. Seeds derive ONLY from stable IDs/names via `ZM_GenHashName` (byte-identical to `ZM_TerrainAuthoring`'s `ZM_Fnv1a32`, pinned by a test) folded with a **PINNED** `ZM_GEN_DOMAIN` enum. `ZM_CreatureGen`/anim/human/building/prop and the versioned `ZM_BakeManifest` are separate later boxes; these modules only expose a `uZM_GENCOMMON_VERSION` / synth version constant.
- **Why:** the S4 gate requires `ZM_Gen` units to run headless in the CI backbone (GPU-less; determinism is a TESTED invariant). Keeping the pure library always-compiled (the `ZM_TerrainAuthoring` pure-policy precedent) lets the tests run with no `ZENITH_TOOLS`/GPU/disk dependency and keeps the registered unit count identical across configs. Wrapping `ZM_BattleRNG` avoids golden-constant drift. Building into a POD that mirrors the asset layout makes "what we test in-memory == what Export bakes", so in-memory S4 determinism implies on-disk S9 determinism.
- **Golden-pinned NOW (hard to reverse -- frozen in the first commit):** the `ZM_GEN_DOMAIN` enum ordering + `ZM_GenDeriveSeed` fold. Changing them later invalidates every future baked asset (full cold re-bake), so both are locked by `Seed_DeriveIsStableGolden`.
- **Accepted risk (logged):** transcendentals (`sinf`/`cosf`/`powf` in ring points, superellipse, hue-rotate) are not bit-portable across compilers/ISAs. Bounded by win64-only + a single CI toolchain + `/fp:fast` forbidden; an Android bring-up would need per-arch golden re-baselining or an integer angle table.
- **Deferred open questions (decide when the consuming box is authored, NOT blocking):** dex-icon recipe ownership, fixed-vs-per-family shiny hue angle (>=30deg for BC1 safety), dual-type palette blend ratio, achromatic-shiny saturation/lightness fallback, and `ZM_BakeManifest` per-family vs per-species granularity. Provisional recommendations recorded in the design artifact.
- **Tests-that-lock-it:** 31 `ZENITH_TEST(ZM_Gen, ...)` cases (boot baseline **1773 -> 1804**) across `Tests/ZM_Tests_GenCommon.cpp` (RNG same-seed + battle-golden-wrap + seed-derivation golden + domain disjointness + FNV anchor + noise determinism + loft byte-identical/winding/bounds/weights-sum/bone-caps/seam/subdiv-t0/topology + validator self-check) and `Tests/ZM_Tests_TextureSynth.cpp` (texel byte-identical + family-seed differs + shiny-differs-same-dims + 18-type palette + blend order + pack determinism/sRGB + normal-from-height + rect/eye decals + stripes/spots determinism).
- **Implementation notes (found during integration/review):** (1) the loft's wall winding is Y-oriented -- `ZM_EmitAndStitch` flips the stitch winding per adjacent ring-pair when the sweep ASCENDS in Y (`bFlip = cur.Y > prev.Y`) so `cross(C-A,B-A)` stays OUTWARD for the repo cull rule, matching the caps' existing Y-based orientation (the winding test failed first on ascending fixtures; per-pair flip is robust for non-monotonic chains; ΔY==0 documented as a degenerate precondition, no hard assert). (2) `ZM_GenHashName` restates the FNV-1a arithmetic locally rather than including `ZM_TerrainAuthoring.h` (which drags `ZM_WorldSpec.h`), pinned equal by `Hash_Fnv1aMatchesTerrainAnchor` + the `"A"->0xC40BF6CC` literal. (3) reviewer verdict SHIP-WITH-FIXES applied in the same landing: real tangent/normal-regen determinism coverage (was vacuous size-0 compare), `uSegs>=3` and `iParent>=-1` asserts, and `ZM_SynthHueRotate`'s achromatic threshold raised to `2/255` (above 8-bit quantization) to keep its internal differs-assert from firing on near-grey inputs.
- **Reversibility:** High. All new `ZM_`-prefixed code under `Source/Gen/` + `Tests/`; no engine files change; no baked assets exist yet. The one hard commitment is the golden-pinned seed derivation (above).

---

## 2026-07-13 -- ZM-D-058 -- Dawnmere grass made visible: per-region grass chunks + central town lawn

- **Trigger:** the S3 human visual review REJECTED the Dawnmere exterior -- no
  visible grass. Runtime instrumentation (not theory) showed grass generated
  200,159 blades and uploaded, but only ~720 were drawn from the town-center
  spawn.
- **Root cause (two compounding):** (1) `Flux_GrassImpl::GenerateFromTerrain`
  built a SINGLE terrain-spanning chunk; `UpdateVisibleChunks` picked one LOD for
  it from the camera->AABB-centroid distance (~150 m) -> the whole map rendered
  at LOD3 (~12.5%). (2) All six Dawnmere grass dabs were peripheral, so the spawn
  sat in a grass-free hole (nearest grass ~150 m away).
- **Engine decision (shared Flux change):** partition generated blades into a
  world-space chunk GRID (`GrassConfig::fCHUNK_SIZE` = 64 m) -- counting-sort the
  instance array contiguous per cell, shuffle within a cell (so LOD reduction
  samples the whole cell), and give each chunk exact bounds. `ExecuteRender` now
  issues one instanced `DrawIndexed` per visible chunk with `firstInstance` = the
  chunk's base offset; `Flux_Grass.slang` `vsMain` gains `SV_StartInstanceLocation`
  and indexes `InstanceBuffer[SV_InstanceID + startInstance]` (SV_InstanceID does
  not include firstInstance -- mirrors `Flux_Terrain_ToGBuffer`). Grass near the
  camera now renders at LOD0; distant regions LOD-down / cull independently.
- **Game decision:** two central town-lawn grass dabs (centres (512,470) r150 and
  (512,610) r130) so grass surrounds the `TownCenter` spawn; the plaza/home pads
  and paths still erase their paved footprints in the grass-erase phase.
  `fGRASS_DENSITY_SCALE` 0.15 -> 0.70. Dawnmere grass is DECORATIVE only -- the
  `ZM_TerrainGrass` component is the rendering bridge; tall-grass encounter
  gameplay is S5's `ZM_TallGrassSystem`/`ZM_EncounterZone` and is ROUTE-only.
- **Why:** a Pokemon starting town reads as grassy; the single-chunk LOD was a
  latent renderer bug (any camera far from a big terrain's grass centroid loses
  all grass) that also benefits RenderTest.
- **Tests that lock it:** new `SpawnVisible` phase in `ZM_GrassRegeneration_Test`
  asserts `GetVisibleBladeCount() > 0` from the spawn camera (the old test proved
  generate+upload but never on-screen visibility -- the exact gap the human eye
  caught). Golden terrain-recipe assertions in `ZM_Tests_TerrainAuthoring`
  updated for the two new dabs (grass dab count 6->8, plan size 1022->1024,
  GRASS_FILL phase 6->8). Dawnmere now bakes/uploads 573,693 blades / 8,098
  triangles. Re-gated all-green (5-config builds, boot units 1772/0, 6/6
  automated, RenderTest `TerrainEditorSmoke`). Grass `.spv` regenerated via
  FluxCompiler (git-LFS + shader-validation).
- **Reversibility:** high -- density scale and dabs are recipe data; the chunk
  grid is a self-contained rewrite of `GenerateFromTerrain`/`ExecuteRender`.
- **Status:** APPROVED by the user 2026-07-13 ("those three screenshots look
  fine") -- the S3 visual gate is SIGNED OFF on the fresh `new_01/02/03` set;
  committed to master.

## 2026-07-13 -- ZM-D-057 -- PlayerHome round trip uses an opaque camera barrier and globally ordered persistent fade

- **Scene/content decision:** build index **40** is now the always-authored,
  terrain-independent `PlayerHome` interior. Its collidable procedural greybox
  shell, scene-owned Player/main follow camera, `Door` feet marker at
  `(0,0,3.5)`, and live `PlayerHomeExitTrigger -> (2,"FromHome")` are the first
  real interior route. Dawnmere adds a replaceable greybox home shell, the
  `FromHome` feet marker at `(384,26.590313,482)`, and live
  `HomeDoorTrigger -> (40,"Door")`. `ZM_GreyboxVisual` claims serialization
  order **107** (next free 108) and reconstructs a unit-cube model/material per
  runtime scene generation; it is deliberately replaceable S3 blockout, not a
  new baked-asset family or final S4 art.
- **Fade/readiness decision:** the persistent `ZM_GameStateRoot` owns one exact
  full-screen black `WarpFade` UIOverlay at sort order **10000**. The manager,
  not the widget, advances alpha over **0.20 s** each way. Accepted input freezes
  immediately; `QUEUED` cannot issue its one SINGLE load below alpha 1. After
  the replacement Player is placed and both body velocities/controller intent
  are zeroed, the manager remains opaque through `WAITING_FOR_CAMERA`. Fade-in
  begins only when exactly one active-scene `ZM_FollowCamera` entity also owns a
  Camera component, targets that exact Player generation, and is the active
  scene's main camera. Input unlocks only at alpha 0. Losing the scene, Player,
  camera readiness, or exact overlay during the transition returns to or stays
  opaque with all active Players frozen. This makes fade a fail-closed traversal
  safety boundary, not decoration.
- **Engine UI-order decision:** element sort order is global across loaded
  canvases for quad rendering. `Zenith_UICanvas` forwards its current element
  key into the shared Flux quad queue; upload preparation stable-sorts ascending
  by key, with submission index as the equal-key tie-break. Raw/non-UI callers
  retain key 0. The fixed **1,024-quad** queue drops the newest excess quad and
  warns once per frame, preserving already-queued overlays without overflow.
  Flux Text retains the highest-sort active overlay clip across canvas
  collection (equal keys remain last-writer-wins). `DiscardPendingFrame` is the
  single disabled/reset boundary for both the legacy and render-graph Text
  paths: it drains the pending queue, background/foreground/total counters, and
  overlay clip together, so re-enabling Text cannot replay stale submissions or
  inherit an old clip. This closes the cross-scene case where the persistent
  canvas is visited before a later active-scene HUD that would otherwise draw
  over or replace the fade's text clip.
- **Engine asset-lifecycle decision:** `Flux_ModelInstance::SetMaterial` retains
  material overrides through its owning `MaterialHandle`; callers do not add a
  second manual reference. `Zenith_ModelComponent` v8 deserialization therefore
  keeps each decoded material alive with its temporary registry handle only
  until a model instance accepts ownership. A procedural empty-model record has
  no receiving instance, so the temporary material becomes reclaimable instead
  of leaking one reference on every Dawnmere/PlayerHome greybox reload.
- **Engine instantaneous-overlay decision:** for a zero/negative fade duration,
  `Zenith_UIOverlay::Show` synchronously snaps current dim alpha to configured
  dim alpha, including an already-showing repair, while `Hide` synchronously
  clears current alpha/visibility and restores sibling interaction.
  Positive-duration animation is unchanged. This makes the rendered overlay's
  opacity agree with manager alpha during the same hitch-sized update instead
  of requiring a later UI update before black is actually submitted.
- **Automation decision:** `ZM_PlayerHomeRoundTrip_Test` drives real controller
  input at a deterministic **1/30 presentation dt** while the engine retains its
  ordinary fixed physics substeps. It performs FrontEnd -> Dawnmere bootstrap,
  runs to and collides with the Dawnmere door, enters build 40 at `Door`, runs to
  and collides with the exit, and returns to `FromHome`. Each leg asserts
  fade-out/opaque-load/camera-barrier/fade-in ordering, exactly one load, scene
  and entity generation replacement, persistent manager identity, exact XZ and
  spawn-centre placement, zero motion/intent before unlock, no terrain grass in
  the interior, and grass/camera recovery outside. The final physics-contact
  settlement permits at most 5 cm downward Y while XZ stays exact.
- **Tests that lock it:** four new `ZM_WorldTraversal` T0 cases bring that
  category to **16**. `FadeAdvanceClampsInvalidDtAndRuntimeReset` also exercises
  the production global queue sort, equal-key stability, actual UICanvas key
  forwarding, capacity guard, text-clip arbitration, complete disabled/reset
  queue/counter/clip drain in both Text paths, clean re-enable, owning material
  retain/release, transient-material registry reclamation after procedural
  empty-model deserialization, and direct zero-duration overlay Show/Hide. The
  opaque-before-load case now crosses the full 0.20 s fade in one **0.25 s**
  manager tick, renders the real manager canvas inside the load callback, and
  requires an actually submitted sort-10000 alpha-1 quad before load permission.
  The other cases lock unique exact-camera readiness, fail-closed dependency
  loss, alpha-zero unlock, and runtime-state serialization omission. These
  lifecycle assertions extend existing cases rather than adding registrations;
  the category remains **16** and the P1 registry remains **6**.
- **Definitive post-overlay-hitch automated gate (local authority):** regen
  **2.401 s**. All five win64 targets are green: Vulkan Debug Tools=true
  **11.225 s**, Debug Tools=false **11.755 s**, Release Tools=true **11.213 s**,
  Release Tools=false **11.031 s**, and D3D12 Debug Tools=false **7.656 s**.
  Boot units are **1773 ran / 1772 passed / 0 failed / 1 skipped**, with
  **180.640 s** helper wall under the canonical watchdog; the workflow baseline
  is 1773, `ZM_WorldTraversal` is 16, and the P1 registry is 6. Headless ran all
  **6/6** in **1.590 s** wall: two semantic passes (`ControllerHarness` **142
  frames / 25.100 ms**, Boot **1 / 0.018 ms**) plus exactly four
  graphics-required skips. Windowed: `ZM_WarpInfrastructure_Test` **29 /
  2008.714 ms** (**14.869 s** wall; one frame removed by synchronous opacity),
  `ZM_GrassRegeneration_Test` **11 / 2579.674 ms** (**15.125 s** wall),
  `ZM_DawnmerePlayerCamera_Test` **117 / 6212.128 ms** (**18.712 s** wall), and
  `ZM_PlayerHomeRoundTrip_Test` **673 / 14662.601 ms** (**27.514 s** wall).
  RenderTest rebuilt in **6.192 s**; `EngineBootShutdownSmoke` passed **1 /
  28.606 ms** (**40.622 s** wall) and `TerrainEditorSmoke` **151 / 5291.193
  ms** (**46.025 s** wall). The ignored
  `Build/artifacts/zenithmon/s3/final/post_overlay_hitch_fix/` root contains **12
  parsed JSON / 12 passed / 0 failed**, with exactly the four expected headless
  graphics skips. The instantaneous Show/Hide and real-quad assertions add no
  registrations or baseline drift.
- **Visual-gate boundary:** stable ignored evidence is
  `Build/artifacts/zenithmon/s3/visual/01_dawnmere_exterior_terrain_grass_camera.png`,
  `02_playerhome_interior.png`, and
  `03_dawnmere_return_camera_reacquired.png`. Provenance is capture
  `capture_final_posthitch_20260713_183717` from the definitive binary: the
  round-trip test passed **673 frames / 14619.2 ms** with exit 0, and all three
  ignored/inspected PNGs are valid **1280x720**. Their respective SHA-256
  values are
  `9FEFA6E1B20CB9F1647F19A0416FCD6A80ACA653EB6EEEFE6A86DD722790A1DF`,
  `13104E86246748BF58AF200DFAC213C2A6B6595A81086E30346B75857280B90E`, and
  `B0D49B1CE41ACB98AA184E55ECB1531D34DC76009C3BED0CBD67CCD61C3B4B41`.
  The automated gate is green, but the stage remains **NOT COMPLETE** until the
  user reviews those captures and a separate sign-off decision is appended.
  S4/S5 do not begin while the
  `GATE-WAIT: S3 visual sign-off` marker remains.
- **Reversibility:** the build-40 scene, greybox visuals, trigger geometry, and
  0.20 s timing are localized/additive. Build index, spawn tags, component order,
  and v1 scene streams are compatibility contracts. Global quad/text ordering is
  engine-wide but preserves raw key-0 and equal-key historical order; reverting
  it would reintroduce the persistent-cross-canvas occlusion bug and requires a
  different global overlay compositor first.

## 2026-07-13 -- ZM-D-056 -- Persistent spawn-tag traversal uses a manager-only root and generation-exact scene entities

- **Decision / ownership:** register `ZM_GameStateManager`, `ZM_SpawnPoint`, and
  `ZM_WarpTrigger` at unique serialization orders **104 / 105 / 106**.
  FrontEnd authors a non-transient, manager-only `ZM_GameStateRoot`; the
  authoritative manager calls `DontDestroyOnLoad` on that root. Player and
  camera remain destination-scene-owned and are replaced by SINGLE loads. The
  singleton stores a generation-bearing `Zenith_EntityID`, rejects missing or
  ambiguous lookup, and retires a duplicate manager in `OnStart` while
  preserving the live authority.
- **Transition contract:** a request must be idle and resolve an exact WorldSpec
  target-build/spawn-tag pair. Its source must contain exactly one valid
  active-scene dynamic-capsule Player, except for the deliberate playerless
  **FrontEnd build-index-0 direct-request** path used before a Player exists.
  Acceptance freezes the source immediately and records its full ID. The state
  machine advances `QUEUED -> WAITING_FOR_SCENE -> WAITING_FOR_SPAWN`, issuing
  exactly one `SCENE_LOAD_SINGLE` on the next manager update. A replacement
  Player freezes itself from `ZM_PlayerController::OnStart` while any transition
  is active, so component order cannot leak an input frame before placement.
- **Spawn / motion contract:** a tag is 1-31 printable ASCII bytes
  (`0x20..0x7E`) in a NUL-padded 32-byte buffer and lookup requires exactly one
  exact-case marker in the loaded destination scene. A marker transform denotes
  **feet**. Dawnmere authors `TownCenterSpawn` at
  **(512,25.98577,480)**; the Player's scale-derived 0.9 m capsule half-extent
  yields centre **(512,26.88577,480)**. Resolution performs the allowed one-time
  body teleport, zeros linear and angular velocity, resets controller runtime
  state, enables movement, and returns the manager to idle. Missing/duplicate
  markers, invalid bodies, or scene-generation changes remain frozen and
  waiting rather than placing ambiguously.
- **Trigger contract:** `ZM_WarpTrigger::OnStart` reasserts its collider as a
  sensor. Collision entry is accepted only when the other entity's full ID is
  the unique valid active-scene dynamic-capsule Player; additive-scene,
  duplicate, malformed, bodyless, foreign-body, and slot-reused candidates
  fail closed. A successful overlap latches exactly once; only collision exit
  by that exact generation-bearing ID clears it.
- **Serialization boundary:** all three components have fixed v1 **scene
  component** streams. The manager stream is version-only; spawn/trigger streams
  persist authored tags and target build data. Queued/waiting transition state,
  frozen IDs, load counts, and trigger latches are runtime-only. This is not the
  durable S7 `ZM_SaveSchema` and does not ship player save/load.
- **Tests that lock it:** exactly **12** `ZM_WorldTraversal` T0 tests cover the
  singleton/state machine, transactionality, tag/stream boundaries, real sensor
  pass-through, reset/re-entry latch behavior, feet-derived placement, motion
  reset, destination `OnStart` freeze, duplicate retirement, and scene/entity
  slot-reuse generations. The boot gate is **1769 ran / 1768 passed / 0 failed /
  1 skipped**. All **5** P1 tests register: headless Boot + ControllerHarness
  pass while Warp/Grass/Dawnmere skip for graphics. Windowed evidence is
  `ZM_WarpInfrastructure_Test` **4 frames / 885.7 ms**,
  `ZM_GrassRegeneration_Test` **11 / 1927.5 ms**, and
  `ZM_DawnmerePlayerCamera_Test` **117 / 5043.5 ms**. All four Vulkan
  Debug/Release x Tools true/false builds plus the D3D12 Debug Tools=false link
  proof are green.
- **Boundary / next:** the current P1 makes a direct FrontEnd manager request;
  there is no fade, PlayerHome/build-40 scene, or authored live trigger yet.
  PlayerHome plus the Dawnmere round trip/fade is the next Roadmap box. The hard
  human visual gate waits until that box and the full S3 automated gate are
  complete, so this milestone is not a stop point.
- **Why / reversibility:** keeping only the manager persistent avoids carrying a
  Jolt body or camera through SINGLE scene teardown. Exact generation checks and
  fail-closed uniqueness prevent stale pool slots or additive content from
  triggering/placing the wrong Player. Orders, streams, and authoring data are
  now compatibility contracts; trigger geometry, fade presentation, and future
  WorldSpec edges remain additive behind the same manager API.

## 2026-07-13 -- ZM-D-055 -- Scene-owned velocity controller and fixed follow camera make Dawnmere traversable

- **Decision:** complete the S3 input/controller/camera Roadmap box with three
  game-local seams. `ZM_InputActions` is a stateless layer over raw Zenith keys:
  WASD and arrows resolve movement axes (opposites cancel), Enter/Space confirm,
  Escape/Backspace cancel, M/Tab menu, and either Shift runs. The controller,
  not the input reader, normalizes camera-relative diagonal movement. This keeps
  today's fixed mapping replaceable when rebinding eventually lands.
- **Player body and movement contract:** `ZM_PlayerController` is serialization
  order **102** and owns an upright dynamic generic Jolt capsule derived from
  the authored transform scale **(0.8,1.8,0.8)**. It drives camera-relative
  **horizontal-world speed** at **4 m/s walk / 7 m/s run**,
  rotates the visual transform toward travel, and writes an optional animator
  `Speed` parameter. Invalid or nonpositive dt is a true no-op for controller
  state, animation, body velocity and facing. Slopes through **45 degrees** are
  accepted and steeper-surface uphill drive is blocked. On a grounded walkable
  downslope, only the tangent-required downward velocity is added for adhesion;
  a stronger fall or positive step-assist rise is preserved. Step
  assist requires a lower obstruction, clear upper probe and walkable landing
  no more than **0.40 m** above ground, then applies one bounded upward-velocity
  assist. Gameplay motion never teleports the body with `SetPosition`.
- **Camera contract:** `ZM_FollowCamera` is serialization order **103** on the
  scene-owned main-camera entity. It captures the authored yaw (Dawnmere uses
  yaw 0), looks back to a player pivot, and follows through an omega-8 critically
  damped spring with a 5.5 m arm, 3.0 m camera height, 0.6 m pivot height and
  65-degree FOV. Physics collision clamps the arm with 0.2 m padding and a 1.0 m
  minimum before it springs back outward. It caches only the generation-bearing
  Player `EntityID`, validates both its generation and owning scene every late
  update, rejects a still-live cached target moved to another scene, and
  reacquires by the scene-local `Player` name after SINGLE reload; neither
  Player nor camera persists across that reload.
- **Dawnmere centre ruling and diagnosis:** author the Player centre at
  **(512,26.9,480)**. The baked terrain physics sample at the exact XZ is
  **Y=25.98577**, not the nominal Y=24 landmark value. With a **0.9 m** capsule
  half-extent, the original Y=24.9 centre began below the baked surface, so the
  1.05 m downward ground probe could never classify ground. At Y=26.9 the feet
  begin near Y=26.0 and the surface is about 0.914 m below centre, inside the
  probe. This is a deterministic generator-authored preview placement only;
  `ZM_SpawnPoint`/`ZM_WarpTrigger` will own semantic arrival tags next.
- **Tests that lock it:** exactly **20** new T0 tests in
  `ZM_Tests_Overworld.cpp`: **5 input / 4 controller / 5 live physics / 4 camera
  / 2 ECS-serialization**. The boot gate is **1757 ran / 1756 passed / 0 failed
  / 1 skipped**. The automated registry is now four: Boot and the asset-free
  `ZM_ControllerHarness_Test` pass headless; graphics-required Grass and the
  asset-guarded `ZM_DawnmerePlayerCamera_Test` skip headless as designed. The
  player/camera windowed integration passed in **117 frames / 4990.3 ms** and
  the focused grass lifecycle passed in **11 frames / 1924.3 ms**. All four
  Vulkan Debug/Release x Tools true/false builds and the D3D12 Debug Tools=false
  link proof are green.
- **Stage sequencing:** the next box is persistent `ZM_GameStateManager` plus
  `ZM_SpawnPoint`/`ZM_WarpTrigger`; PlayerHome and the door round trip follow.
  The human S3 visual gate is deliberately deferred until both are complete and
  the full S3 automated walk/door round-trip gate is green.
- **Reversibility:** all three seams are game-local and additive; both ECS
  components have version-1-empty serialized payloads. Speeds, camera geometry/spring values,
  step bounds and the generator-authored centre are isolated constants or
  authoring values. Rebinding can replace the action readers without changing
  controller policy. Persistence/warp work can replace the preview centre with
  spawn-tag placement without changing the body or camera contracts.

---

## 2026-07-13 -- ZM-D-054 -- Three-recipe terrain measurement validates the cropped per-scene plan

- **Decision:** close Roadmap's three-real-scene terrain-bake measurement and
  Q-2026-07-09-002. The fixed-order measurement registry contains the real
  WorldSpec terrain recipes Dawnmere (town, 16x16 / 256 chunks), Thornacre
  (town, 16x16 / 256 chunks), and Route1 (route, 16x24 / 384 chunks). Each has
  its own stable seed, authored plan, isolated asset-set name, exact output
  enumeration, atomic warm marker, and selected-force CLI path. These recipes
  continue the one-terrain-set-per-outdoor-scene requirement; no shared terrain
  sheet or bake-pipeline optimization is needed before the next S3 task.
- **Measured evidence:** calibrated direct process walls under the same harness
  were **59.035 s / 69.979 s / 80.804 s** for Dawnmere / Thornacre / Route1;
  the production recipe timers, which begin before reset and complete in the
  terminal action, reported **42.588 s / 53.657 s / 64.541 s**. Their families
  contain **256 / 256 / 384 chunks**, **772 / 772 /
  1,156 files**, and **204,684,116 / 204,684,116 / 262,985,940 bytes**. The
  three-sample total is **896 chunks, 2,700 files, and 672,354,172 bytes**.
  A separate all-warm same-harness boot took **16.874 s**, validated every
  family, and queued zero terrain recipes. ZM-D-053's original standalone
  Dawnmere observation (**63.671 s** cold, **14.614 s** warm graphics) remains
  historical evidence; 59.035 s is the later calibrated rerun, not a rewrite.
- **Projection:** a town wall mean of 64.507 s and Route1's 80.804 s give the
  11-town + 14-route planning model **24,676 files / 5,933,328,436 bytes**
  (5.933 GB / 5.526 GiB), a conservative repeated-process **30m 40.833s**, and
  a one-boot/net estimate of **23m 55.857s** after subtracting the shared
  16.874-second warm baseline per sample and adding it once. Scaling the
  internal timers and adding one baseline cross-checks at **24m 09.796s**. The
  GDD's exact 11-town + 15-route sensitivity is **25,832 files /
  6,196,314,376 bytes** (6.196 GB / 5.771 GiB), **32m 01.637s** repeated,
  **24m 59.787s** one-boot/net, and **25m 14.337s** by internal-timer
  cross-check.
- **Why:** both conservative terrain projections fall within the existing
  30-50 minute full-cold planning range, the net estimates are about 24-25
  minutes, and file volume matches the prior ~25k estimate. This resolves the
  risk sufficiently to continue S3. It does **not** prove the eventual
  all-assets cold bake: that 30-50 minute target includes the unbuilt S4 asset
  families, there is no explicit byte cap, and "seconds" warm is qualitative
  rather than a numeric SLA. The model has two towns but only one route, assumes
  later recipes stay in the 16x16 / 16x24 crop classes, and is a planning bound
  rather than a statistical confidence bound. The `~25` planning count and
  GDD's exact 26 outdoor scenes therefore remain explicit primary/sensitivity
  cases. Thornacre and Route1 have measured terrain families only: this decision
  adds no playable scenes, world connectivity, trees, or dressed content.
- **Tests that lock it:** five new `ZM_TerrainRecipeSet` units lock the exact
  three-row WorldSpec registry/order, distinct documented plans, deterministic
  contained plans ending with grass erase, unique contained output sets plus
  pure AUTO/FORCE_ALL/FORCE_SELECTED queue policy, and per-recipe marker counts
  with missing/empty-output invalidation. Regeneration, the default build, all
  four Vulkan Debug/Release x Tools true/false configurations, and D3D12 Debug
  Tools=false are green. The boot gate is **1737 ran / 1736 passed / 0 failed /
  1 skipped**; Zenithmon headless is **2/2** without terrain mutation. Each
  selected cold bake exited 0 and published its validated marker; the all-warm
  boot queued zero terrain recipes. The windowed grass regression still observes
  exactly **200,159 blades from 5,133 triangles** on both Dawnmere loads.
- **Reversibility:** the registry and projection assumptions are additive and
  can be extended or remeasured when later representative recipes exist.
  Recipe tuning may update measured estimates and requires either a forced bake
  or a manifest-version bump when the required-output count is unchanged; only
  a count change invalidates the existing marker automatically. Baked terrain
  remains ignored.
  If the eventual all-assets bake breaches budget, ZM-D-037 still requires an
  optimization pass (parallel/cached/profile-guided export), never shared
  terrain sets.

---

## 2026-07-13 -- ZM-D-053 -- Dawnmere deterministic terrain bake and scene-owned grass regeneration

- **Decision:** the S3 Home Village is the `Dawnmere` terrain set and scene. Its
  immutable `ZM_TerrainAuthoring` recipe derives seed `0x7BF32CA4` from the
  WorldSpec terrain-set name, authors within world coordinates `0..1024`, and
  exports inclusive chunks `(0,0)..(15,15)`. The bounded family is exactly 256
  `Render`, 256 `Render_LOW`, and 256 `Physics` meshes plus `Height.ztxtr`,
  `Splatmap_RGBA.ztxtr`, and `GrassDensity.ztxtr`.
- **Bake/warm contract:** a cold or forced boot invalidates the old completion
  state before queueing the deterministic recipe. Its terminal action requires
  all 771 non-empty outputs, writes a 12-byte little-endian marker (`ZMTR`,
  version 1, required-output count 771) to a temporary file, and atomically
  renames it to `ZM_TerrainRecipe.manifest`. The completed terrain family is
  therefore 772 files including the marker. Scene authoring waits until a later
  warm boot, which requires both the valid marker and every required output,
  then writes the ignored `Assets/Scenes/Dawnmere.zscen`. The observed cold bake
  was **63.671 s** and the observed warm graphics boot was **14.614 s**. This is
  one data point only; the Roadmap's three-real-scene bake-time measurement and
  extrapolation remain the next task.
- **Grass contract:** `ZM_TerrainGrass` is serialized on the Dawnmere terrain
  entity. OnAwake it owns and validates the exact 1024x1024 CPU density map;
  headless boots stop there without touching Flux. Graphics boots wait a bounded
  300 frames for terrain physics, reset prior grass state, apply density scale
  0.15, and generate from the terrain physics geometry. OnDestroy clears both
  the Flux instances and density map. The first load and same-process reload each
  regenerated and uploaded exactly **200,159 blades from 5,133 triangles** with
  no accumulation, and returning to FrontEnd left no Dawnmere grass. Trees are
  deliberately absent from this first terrain deliverable.
- **Why:** this is the first real consumer of E1/E2 and proves that a deterministic
  per-scene cropped terrain family can be baked, warm-gated, loaded, reloaded, and
  cleaned without committing generated assets or leaking renderer-owned grass.
  Deferring scene authoring to the warm boot prevents a partial cold bake from
  being serialized as a valid world scene.
- **Tests that lock it:** three `ZM_TerrainAuthoring` units lock recipe identity,
  bounds/order/determinism, output enumeration, marker contents, missing-output
  invalidation, and containment; one `ZM_Grass` unit locks density decoding,
  sampling, path construction, and failure clearing. The windowed
  `ZM_GrassRegeneration_Test` locks the first-load/reload counts and FrontEnd
  cleanup. Full boot gate is **1732 ran / 1731 passed / 0 failed / 1 skipped**;
  all four Vulkan Debug/Release x Tools true/false builds plus the D3D12 Debug
  Tools=false link proof pass. Zenithmon headless reports **2/2** (the graphics
  grass test is skipped as designed), CityBuilder is **45/45**,
  DevilsPlayground is **158/158**, and RenderTest windowed
  `EngineBootShutdownSmoke` + `TerrainEditorSmoke` both pass.
- **Reversibility:** recipe tuning is localized but requires a generator-version
  bump/rebake once downstream placement depends on it. The marker format is
  versioned and can evolve; old or incomplete markers already force a cold bake.
  Removing the scene-owned grass lifecycle would require a replacement that
  preserves headless safety, reload determinism, and teardown cleanup.

## 2026-07-13 -- ZM-D-052 -- Engine E2: bounded terrain export and terminal sparse-HIGH streaming

- **Decision:** `AddStep_TerrainExportChunksRect(minX,minY,maxX,maxY)` exports
  inclusive chunk coordinates. Bounds are accepted only when
  `0 <= min <= max < 64` on both axes and the rectangle contains the required
  `(0,0)` LOW/physics anchor; invalid input is rejected without clamping,
  swapping, opening a standalone editor, cleanup, or output. A successful
  operation removes direct `.zmesh` files once from the E1-canonical target and
  writes exactly `Render`, `Render_LOW`, and `Physics` for each requested
  coordinate (`3 * area` files), preserving textures, unrelated files, nested
  directories, and sibling terrain sets. It crops files only: the fixed 4096 m
  terrain grid/density remain the deferred E6 limitation.
- **Shared format contract:** `Flux_TerrainExportRect` is the single signed,
  transactional bounds/enumeration contract used by tools, editor, and tests.
  `Flux_TerrainVertexLayout` is the single HIGH chunk element order/size/stride
  contract used by exporter and streaming reader; canonical HIGH chunks are
  exactly 4,225 vertices and 24,576 indices at a 28-byte stride.
- **Sparse-stream contract:** missing, truncated, malformed, wrong-layout, or
  incompatible HIGH sources are validated by a bounded non-asserting reader
  before any eviction, allocation, residency, dirty-state, or stats mutation.
  The first failure warns and latches `SOURCE_UNAVAILABLE`; later frames skip it
  without consuming the 32-source-probe budget. Successful uploads remain capped
  at 8 per frame. Terrain registration/regeneration resets the latch so a new bake
  retries, and existing LOW fallback/legacy behavior is unchanged.
- **Why:** full 64x64 bakes produce roughly 12k files per terrain and make the
  required one-set-per-outdoor-scene plan impractical. Rectangular file crops make
  S3 authoring measurable while terminal sparse streaming prevents expected holes
  from evicting valid data, thrashing disk, or starving valid chunks behind the
  frame attempt budget.
- **Tests that lock it:** exactly three engine tests cover inclusive counts and
  enumeration, invalid transactionality, four-argument automation and production
  routing; missing/wrong-layout source mutation/allocator safety; and the
  32-missing-then-valid terminal skip/no-retry schedule. Full boot gate is
  **1728 ran / 1727 passed / 0 failed / 1 skipped** (Zenithmon 1725 -> 1728;
  engine default 1075 -> 1078). Regeneration, all four Vulkan Debug/Release x
  Tools true/false builds, the D3D12 Debug Tools=false link proof, Zenithmon 1/1,
  CityBuilder 45/45, DevilsPlayground 158 tests, and RenderTest windowed
  `EngineBootShutdownSmoke` + `TerrainEditorSmoke` all pass.
- **Reversibility:** additive for authoring: the old full-grid APIs remain intact.
  Rect-authored terrain sets depend on sparse-stream tolerance, so removing it
  would require rebaking every set full-grid.

## 2026-07-12 -- ZM-D-051 -- Engine E1: serialized, contained per-terrain asset sets with staged editor bakes

- **Decision:** `Zenith_TerrainComponent` serialization v4 appends a terrain-set
  name. The empty name is the exact backward-compatible legacy layout
  (`<game-assets>/Terrain/`, with legacy textures under `Textures/Terrain/`);
  a named set resolves to `<game-assets>/Terrain/<Set>/` and co-locates its
  textures and chunk meshes there. Valid names use the ASCII grammar
  `[A-Za-z0-9][A-Za-z0-9_-]{0,63}`. Resolution requires strict component-wise
  containment, and invalid serialized v4 names fall back safely to legacy.
  All terrain load, collision, low/high-LOD streaming, regeneration, and the two
  CityBuilder state consumers now use the resolved per-component directory.
- **Editor/tool contract:** the terrain editor owns a staged asset-set value;
  changing it never live-reroutes initialized terrain. A successful full bake
  commits that value immediately before synchronous regeneration. Clean/different
  targets reload persisted data, while reopening the same dirty target resumes
  its CPU maps and undo state. Production cleanup canonicalizes root and target,
  refuses symlink/junction escape, is non-recursive, and removes only direct
  `.zmesh` files. `AddStep_TerrainSetAssetSet` copies its argument, validates and
  preflights the selected component transactionally, and stamps a fresh selected
  Terrain component so a later scene save persists the choice.
- **Why:** Zenithmon requires one isolated terrain asset family per outdoor scene,
  while existing games and v1-v3 scenes must retain byte-compatible legacy path
  semantics. Staging prevents an editor draft from invalidating live render or
  physics resources; strict path/canonical checks make the destructive bake step
  safe at its filesystem boundary.
- **Tests that lock it:** exactly seven new engine unit tests cover grammar and
  transactional setters; path boundaries and state propagation; move isolation;
  v4 prefix/round-trip/invalid fallback; exact v1-v3 compatibility; editor
  staging, cleanup isolation, default reload, and dirty-session resume; and
  automation argument ownership/action execution/component serialization. Full
  boot gate is **1725 ran / 1724 passed / 0 failed / 1 skipped** (Zenithmon
  baseline 1718 -> 1725; engine default 1068 -> 1075). The four Vulkan
  Debug/Release x Tools true/false builds and D3D12 Debug Tools=false link proof
  pass. `zenith test` passes for Zenithmon (1/1), CityBuilder (45/45), and
  DevilsPlayground (158 tests); RenderTest windowed `EngineBootShutdownSmoke` and
  `TerrainEditorSmoke` both pass.
- **Reversibility:** additive for existing content: empty/v1-v3 assets remain on
  the legacy layout. Removing named sets later would require retaining the v4
  reader or migrating every scene that stores a non-empty set.

## 2026-07-12 -- ZM-D-050 -- Breeding SC-C: egg moves + ability/hidden-ability inheritance + hatch cycles (FEATURE-COMPLETE BREEDING)

- **Decision:** the final sub-commit of the feature-complete-breeding expansion
  (ZM-D-048). Adds derived species abilities {regular, hidden}, egg moves, and a
  hatch-cycle accessor, and wires ability/hidden-ability + egg-move inheritance into
  `ZM_GenerateEgg`. **Breeding is now FEATURE-COMPLETE** (mainline mechanics: gender +
  ratios, real egg groups, GLOOPET Ditto-analog, gendered compatibility, IV
  [Heirloom Knot] + nature [Stasis Stone] inheritance, ability + hidden-ability
  inheritance, egg moves, hatch cycles). Cosmetic shiny/Masuda remains DEFERRED to
  S5+ (ZM-D-048).
- **Derived accessors (no stored columns):** `ZM_GetSpeciesAbilities(id)` ->
  `{m_eRegular, m_eHidden}` (distinct; regular from the types[0] ability pool at
  `familySeed%count`, hidden from the types[1]-or-archetype pool at
  `(familySeed>>5)%count`, distinctness fixup). `ZM_GetSpeciesEggMoves(id)` -> up to 6
  own-type moves not in the species' level-up learnset (legendary empty), id order.
  `ZM_GetSpeciesHatchCycles(id)` -> rarity base {10/15/25/40} + size class (0..4);
  data-only (overworld step-driving deferred to S9).
- **Inheritance in ZM_GenerateEgg:** (ability) if the mother carries HER species'
  hidden ability, the offspring gets its own species' hidden ability with probability
  `uZM_BREED_HIDDEN_INHERIT_PCT` (60%, a `Chance(60,100)` draw) else its regular;
  otherwise the offspring copies the mother's ability with NO draw. This hidden draw
  is APPENDED AFTER the gender draw (order IV -> nature -> gender -> [conditional
  hidden]) and fires ONLY for a hidden-carrying mother, so every pre-existing fixture
  (mother with a non-hidden ability + empty movesets) is byte-identical. (egg moves,
  RNG-FREE) after the base-evo L1 learnset fills the slots, each offspring egg move
  that EITHER parent knows fills the first empty slot, 4-cap, no eviction.
- **Notes:** the ability-derivation accessor is ALSO usable to give wild/tower mons a
  real ability (they currently use `ABILITY_NONE`) -- NOT retrofitted here to keep
  SC-C breeding-scoped; a small optional follow-up. Egg-move derivation uses "own-type
  moves minus level-up learnset" as a tractable, deterministic realization of
  "egg-group cohort minus learnset" (documented; a cohort shares the species' types).
- **Tests:** 20 new `ZM_Data` tests (ability derivation distinct/exact; ability
  inheritance regular-copy zero-draw lock-step + hidden ~60% over a fixed seed list +
  golden-seed IV/nature/gender non-perturbation; egg-move derivation
  disjoint-from-learnset + exact; egg-move inheritance placement/control/4-cap/RNG-free;
  hatch cycles). 0 existing tests edited (purely additive). Adversarial review:
  SAFE-TO-COMMIT (2 non-blocking notes: a loose hidden-rate distribution band on an
  otherwise well-covered mechanism, and exact-ability tests that pin the derivation
  mechanism not the pool contents -- both acceptable). Boot unit baseline 1698 -> 1718.
- **Reversibility:** additive; the ability pools, egg-move derivation, hidden-inherit
  rate (60%), and hatch-cycle formula are S11-tunable. **FEATURE-COMPLETE BREEDING is
  DONE:** ZM-D-045 (reduced) -> ZM-D-048 (gender) -> ZM-D-049 (egg groups / Ditto /
  gendered compat) -> ZM-D-050 (egg moves / abilities / hatch).

## 2026-07-12 -- ZM-D-049 -- Breeding SC-B: real egg groups + GLOOPET Ditto-analog + gendered compatibility

- **Decision:** part of the feature-complete-breeding expansion (ZM-D-048).
  Replaces the archetype-as-egg-group proxy with a real DERIVED egg-group
  taxonomy, adds a Ditto-analog, and makes breeding compatibility gender-aware.
- **Egg groups (DERIVED, no stored column):** `ZM_GetSpeciesEggGroups(id)` ->
  `ZM_EggGroups{ m_uCount, m_aeGroups[2] }` over `ZM_EGG_GROUP` (FIELD / HUMANOID /
  FLYING / DRAGON / WATER / BUG / AMORPHOUS / PLANT / MINERAL / FAIRY / NO_EGGS /
  UNIVERSAL). Legendary -> {NO_EGGS}. Else primary from archetype (QUADRUPED->FIELD,
  BIPED->HUMANOID, AVIAN->FLYING, SERPENT->DRAGON, AQUATIC->WATER, INSECTOID->BUG,
  BLOB->AMORPHOUS, FLOATER_PLANTOID->PLANT); secondary from the FIRST type slot that
  maps (GRASS->PLANT, DRAKE->DRAGON, FEY->FAIRY, PHANTOM->AMORPHOUS, WATER->WATER,
  STONE/IRON/EARTH->MINERAL, SKY->FLYING), added only if != primary -- if the first
  mapping slot equals the primary the species stays single-group (slot 1 is not then
  consulted).
- **Ditto-analog:** GLOOPET (F40, BLOB, NORMAL, COMMON, GENDERLESS) is the UNIVERSAL
  breeder via `ZM_IsUniversalBreeder`. It breeds with ANY non-legendary partner
  ignoring gender + egg group; the offspring follows the non-universal parent's line.
  Two universals, or universal + legendary, are incompatible.
- **Compatibility (gender-aware):** `ZM_AreSpeciesCompatible(a,b)` (species-level):
  not legendary, not both-universal, one-universal OR share an egg group.
  `ZM_AreCompatible(specA,specB)` (spec-level): species-compatible AND (universal OR
  exactly one MALE + one FEMALE). A GENDERLESS non-universal parent is never
  compatible (only a universal breeder can breed with a genderless species).
  `ZM_DaycarePairCompatible` is now gender-aware (delegates to `ZM_AreCompatible`).
- **Offspring parent role:** `ZM_GenerateEgg(A,B,...)` derives the mother =
  non-universal partner (if one side is universal) else the FEMALE parent; the mother
  supplies the base-evo offspring species, the copied ability, and the `RandBelow(2)
  ==0` IV source. RNG draw order/bounds are UNCHANGED (IV -> nature -> gender); only
  which parent each draw reads changes, so IV/nature goldens stay byte-identical
  wherever the female parent is the previous mother. A precondition assert fires on an
  invalid pair. Legacy `ZM_GetBreedingGroup` deleted (no-legacy mandate).
- **Tests:** 22 new `ZM_Data` tests (egg-group derivation incl. the primary-blocks-
  secondary single-group case, universal breeder, species + gendered compatibility,
  offspring parent role, daycare) + 24 existing breeding tests re-baselined to
  gendered/GLOOPET fixtures. The re-baseline is LEGITIMATE (verified by review): the
  female parent is set to the previous mother, so IV/nature golden VALUES are
  byte-identical AND still match the unchanged independent offline oracle -- only the
  parent-declaration lines changed. The genderless-blob test was rebuilt as
  GLOOPET x blob. Adversarial review: SAFE-TO-COMMIT (2 low findings closed pre-commit:
  a PUFFSEED single-group coverage assert + a stale-comment reword). Boot unit baseline
  1676 -> 1698.
- **Reversibility:** additive; the egg-group mapping + the GLOOPET designation are
  data-derivation choices, S11-tunable. SC-C (egg moves + ability/hidden-ability
  inheritance + hatch-cycle data) completes the feature.

## 2026-07-12 -- ZM-D-048 -- Feature-complete breeding + gender (user-directed; fulfilling the mainline breeding scope). SC-A gender foundation.

- **Decision (user, 2026-07-12):** breeding is completed to the FULL mainline
  feature set. This is NOT a scope widening -- Scope.md Section 1 already locks in
  "breeding/eggs/daycare" under "mainline MECHANICS only"; the box-6 SC1 model
  (ZM-D-045) was a documented REDUCTION (Q-2026-07-12-004) that under-delivered
  against that scope. The user directed full gender + feature-complete breeding, so
  the reductions are removed. **Resolves Q-2026-07-12-004.**
- **Confirmed boundary decisions (user, 2026-07-12):** (a) Ditto-analog = designate
  the existing genderless species **GLOOPET** as a UNIVERSAL breeder (breeds with
  anything; offspring follows the non-Ditto parent) -- no roster change; (b) hidden
  abilities = ADD a derived second (hidden) ability slot + inheritance (also gives
  wild mons a real derived ability for the first time); (c) shiny + Masuda method =
  DEFERRED to S5+ (cosmetic/display, no headless-battle effect; a clean append when
  the dex/UI exists) -- deferred, not cut; (d) egg hatch cycles = add a DERIVED
  hatch-cycle accessor/data now; the overworld step-driving to actually hatch lands
  at S9.
- **Delivery:** three test-first sub-commits on top of box-6 SC1 -- SC-A gender
  foundation (this entry), SC-B (real egg groups + GLOOPET Ditto + gendered
  compatibility + offspring = female/non-Ditto parent), SC-C (egg moves +
  ability/hidden-ability inheritance + the derived hatch-cycle accessor). All
  additive: new per-species data are DERIVED accessors (the 152 species const rows
  stay untouched), and new RNG draws APPEND after existing ones, so no existing
  golden shifts.
- **SC-A gender-foundation contract:** `enum ZM_GENDER { MALE, FEMALE, GENDERLESS }`
  + `enum ZM_GENDER_RATIO` (8 buckets) in `ZM_SpeciesData.h`; `m_eGender` appended
  LAST on `ZM_BattleMonsterSpec` + `ZM_BattleMonster` (default GENDERLESS,
  POD-append-safe). DERIVED accessors: `ZM_GetSpeciesGenderRatio` (LEGENDARY /
  BLOB-archetype -> GENDERLESS; RARE -> 7:1 male; else `(familySeed>>3)%4` spread
  over EVEN / 3:1M / 3:1F), `ZM_GenderRatioFemaleThresholdOutOf8` (7:1M->1, 3:1M->2,
  EVEN->4, 3:1F->6, 7:1F->7; fixed ratios -> NO_ROLL sentinel), `ZM_RollGender`
  (fixed ratios draw nothing; graded do one `RandBelow(8)`, female iff draw <
  threshold). `ZM_GenerateEgg` rolls the egg gender from the OFFSPRING ratio as the
  LAST rng draw (order IVs -> nature -> gender) so IV/nature goldens stay
  byte-identical; `ZM_GenerateTowerTeam` rolls gender in a SECOND pass so tower
  species/nature goldens stay byte-identical. `ZM_BuildBattleMonster` stays pure and
  copies gender. (Only GENDERLESS/MALE_7_1/EVEN/MALE_3_1/FEMALE_3_1 are reachable
  from the current 152 species; the other ratio buckets are intentional headroom.)
- **Tests:** 13 `ZM_Data` gender tests in `ZM_Tests_Breeding.cpp` (ratio derivation
  with premises self-guarded via `ZM_GetSpeciesData`, the threshold table, roll
  determinism + zero/one-draw lock-step + distribution bands, egg gender via an
  INDEPENDENT oracle, and a non-perturbation regression guard re-asserting the
  pre-gender IV/nature goldens). Adversarial review: SAFE-TO-COMMIT. Boot unit
  baseline 1663 -> 1676.
- **S7 forward note:** when party/daycare save-load is added at S7, the serializer
  MUST include `m_eGender` (nothing serializes these structs today -> no current
  regression).
- **Reversibility:** additive; the derivation rules + the hidden-ability inheritance
  rate + the ratio thresholds are S11-tunable. SC-B + SC-C complete the feature.

## 2026-07-12 -- ZM-D-047 -- S2 stage gate PASSED (all battle logic complete)

- **Decision:** the S2 stage gate is PASSED; S2 -- the complete deterministic
  headless battle system -- is DONE. All six S2 boxes landed (ZM-D-032..046):
  battle engine + append-only event stream (box 1), move/status/catch/switch
  executor (box 2), 50 abilities + weather (box 3), exp/EV/level/evolution
  (box 4), four-tier `ZM_BattleAI` (box 5), breeding/daycare + Battle Tower
  logic (box 6).
- **Gate evidence (2026-07-12, this session):**
  - Boot unit gate: **1663 ran / 1662 passed / 0 failed / 1 skipped** (the skip is
    the pre-existing quarantined engine `RegistryWideNodeRoundTrip`). The suite
    includes the box-1 offline-oracle exact-event-stream scenario
    characterizations and the box-2 2,000-battle fuzz soak (termination < 500
    turns + HP/PP/boost invariants). Game-unit inventory: **209 `ZM_Data` + 384
    `ZM_Battle` + 2 `ZM_Boot`**; the Roadmap "~370 unit tests" target is exceeded.
  - Automated headless batch: `zenith test Zenithmon --headless` = 1/1
    (`ZM_Boot_Test`), exit 0.
  - Build matrix: the full 4-config Vulkan matrix
    (`Vulkan_vs2022_{Debug,Release}_Win64_{True,False}`) + the
    `D3D12_vs2022_Debug_Win64_False` null-backend link proof ALL build green.
  - No windowed or visual check is defined for the S2 gate, so no GATE-WAIT
    applies -- the S2 gate is fully automated and self-signed.
- **Reversibility:** n/a (a gate-results record). Next stage **S3** (first
  overworld) begins the engine-change (E1/E2 terrain) + terrain-bake +
  VISUAL-gate phase -- the user's standing hard-stop-at-visual-gates order
  resumes at the S3 gate.

## 2026-07-12 -- ZM-D-046 -- S2 box-6 SC2: `ZM_BattleTower` logic (BOX 6 COMPLETE)

- **Decision:** `ZM_BattleTower` ships as pure, deterministic, headless LOGIC in
  `Source/Battle/ZM_BattleTower.{h,cpp}` -- no globals/statics-with-state, no UI,
  and it makes NO engine calls itself. It PRODUCES clamped teams + an AI tier + a
  battle config and SETTLES a streak from a bool result; a CALLER drives the
  actual battle via `ZM_BattleEngine` + `ZM_ChooseAction`. This keeps the tower a
  pure logic layer, fully testable without a live battle.
- **Level-50 clamp:** `ZM_ClampSpecToTowerLevel` copies the spec, sets `m_uLevel
  = 50` + `m_uCurExp = UNSPECIFIED`, and preserves species/IVs/EVs/nature/ability/
  moves/override verbatim; the six stats recompute via the locked
  `ZM_BuildBattleMonster` path (>50 clamps down, <50 scales up, full-HP start).
  The original spec is never built, so an out-of-range input level cannot trip the
  build's [1,100] assert. `ZM_ClampPartyToTowerLevel` maps element-wise.
- **Streak -> difficulty:** `ZM_TowerBaseTierForStreak` = [0,7) RANDOM / [7,21)
  GREEDY / [21,35) SMART / [35,inf) CHAMPION (monotonic). Boss = `(streak+1) % 7
  == 0`. `ZM_TowerAITierForStreak` = base tier + ONE step on a boss, capped at
  CHAMPION (never COUNT/NONE). NOTE: the bumped AI tier is NOT globally monotonic
  -- it dips to the base tier after each interior boss (streak 13 -> SMART, 14 ->
  GREEDY); the header documents this. This RESOLVES an internal inconsistency in
  the spec's §5 "worked values" (which implied a two-tier jump) -- the code + tests
  use the ONE-tier rule (streak 20 -> SMART). Rarity ceiling rises COMMON ->
  UNCOMMON (>=7) -> RARE (>=21); never LEGENDARY.
- **Team gen (deterministic, LOCKED draw order):** eligible = dex in ascending id
  with `rarity != LEGENDARY && rarity <= ceiling`; per output slot (default 3):
  (A) species via `RandBelow(eligibleCount)` rejected until distinct from earlier
  slots, (B) one `RandBelow(ZM_NATURE_COUNT)` nature after the species; IVs 31,
  EVs 0, ability NONE, moves = up-to-four highest-level learnset entries at level
  <= 50. Per-battle seed = `run.m_ulSeed + 0x9E3779B97F4A7C15 * (streak+1)`. (43
  non-legendary COMMON species >> team size 3, and an `eligible >= count` assert
  guards it, so the distinct-pick loop always terminates.)
- **Advance/config:** `ZM_TowerAdvance` -- win increments current + raises the
  best-streak high-water; loss resets current to 0 (best preserved); returns the
  new current. `ZM_MakeTowerBattleConfig` -- level cap 50, trainer=true,
  wild/catch/flee=false, awardExp=false.
- **First consumer of ZM-D-044:** the tower is the first real consumer of
  `ZM_AI_TIER` / `ZM_ChooseAction`. FUTURE-INTEGRATION NOTE: `ZM_ChooseAction`
  returns a MOVE for a FAINTED active (it enumerates the fainted active's moves),
  so an S5+ battle-integration caller must submit the forced SWITCH itself on a
  faint rather than delegate that turn to the chooser (or box 5's chooser should
  later special-case a fainted active). The box-6 engine smokes side-step this by
  running a controlled 1v1 (the battle ends on the first KO). Flagged
  Q-2026-07-12-005. No production bug today (the tower never runs battles).
- **Tests that lock it:** 25 tests in `Tests/ZM_Tests_BattleTower.cpp` -- 23
  `ZM_Data` (L50-clamp stat-parity via the build oracle, streak/boss/tier band
  boundaries + one-tier bump, rarity bands, team-gen goldens via an INDEPENDENT
  §7 oracle + legality, advance win/loss/best, config fields) + 2 `ZM_Battle`
  engine round-trip smokes (a real 1v1 tower battle to termination, no exp
  awarded). Adversarial review: SAFE-TO-COMMIT (one non-blocking header-comment
  inaccuracy fixed -- the AI tier is bounded but not monotonic). Boot unit baseline
  1638 -> 1663 (`zm-tests.yml`).
- **Reversibility:** additive + standalone; all numeric knobs (7/21/35 tier
  thresholds, team size 3, rarity bands, the exp-off + NONE-ability team defaults)
  are S11-tunable named constants with zero API/golden impact if retuned. **BOX 6
  COMPLETE** (SC1 breeding/daycare = ZM-D-045 + SC2 tower = ZM-D-046). All S2
  battle-logic boxes (1-6) are now done; next is the S2 automated stage gate.

## 2026-07-12 -- ZM-D-045 -- S2 box-6 SC1: `ZM_Breeding` + `ZM_Daycare` (reduced deterministic breeding on the shipped data)

- **Decision:** `ZM_Breeding` + `ZM_Daycare` ship as pure, deterministic, headless
  free functions in `Source/Battle/ZM_Breeding.{h,cpp}` + `ZM_Daycare.{h,cpp}` --
  no globals/statics-with-state, no UI, no overworld dependency (that integration
  is later). All stochastic behavior flows through a caller-supplied
  `ZM_BattleRNG`; the daycare step/egg model is fully RNG-free.
- **Reduced model on shipped data (no new columns):** the species table carries
  NO egg-group, gender, hatch-cycle, egg-move, or species->ability data. So the
  **archetype** field is the egg-group proxy; there is **no gender** (first parent
  = "mother" by convention); ability is copied from the mother; egg moves = the
  base-evolution's level-1 learnset. Faithful reductions, not cuts of in-scope
  features -- logged Q-2026-07-12-004; each is additive if the data model later
  grows the field (adding gender/egg-groups is a Scope.md data-model expansion).
- **Compatibility:** both parents non-legendary AND sharing an archetype.
- **Offspring species:** the base (lowest) evolution of the MOTHER, derived by a
  roster-bounded backward scan over the shipped forward `m_eEvolvesTo` chain (no
  new pre-evo column; terminates on any cyclic/malformed table). Sentinel-safe:
  `ZM_SPECIES_NONE == ZM_SPECIES_COUNT`, so an index-0 predecessor is not mistaken
  for "not found."
- **Inheritance (LOCKED RNG draw order -- goldens depend on it):** (A) species
  (no draw); (B) IVs -- K = Heirloom-Knot?5:3; phase 1 picks K distinct stat
  indices via `RandBelow(6)` (reject dups); phase 2 iterates stats 0..5, each
  inherited stat draws `RandBelow(2)` for the parent, each fresh stat draws
  `RandBelow(32)`; (C) nature -- Stasis-Stone (everstone) locked value with NO
  draw, else `RandBelow(25)`, ALWAYS AFTER the IV rolls so the everstone can't
  shift IVs; (D) ability = mother's; (E) moves = base-evo L1 learnset; (F) level
  1, EVs 0, `m_uCurExp = uZM_EXP_UNSPECIFIED`. `ZM_ITEM_HEIRLOOMKNOT` /
  `ZM_ITEM_STASISSTONE` are the destiny-knot / everstone analogs (already in
  `ZM_ItemData`), passed via `ZM_BreedingParams`.
- **Daycare:** capacity 2; `Deposit` fills the first free slot (returns the index,
  or capacity when full) and normalizes an UNSPECIFIED exp to the level floor;
  `Step` gives each occupied slot 1 exp/step (level via `ZM_LevelForExp`, capped
  L100, 64-bit intermediate -- no wrap) and advances the egg counter by the step
  count ONLY for a compatible occupied pair, saturating at the 256-step threshold
  where `bEggAvailable` flips true (deterministic, no RNG); `Withdraw` returns the
  leveled spec, clears the slot, resets egg progress; `CollectEgg` calls
  `ZM_GenerateEgg` and resets the counter/flag, leaving parents deposited.
- **Tests that lock it:** 33 `ZM_Data` behavioral tests in
  `Tests/ZM_Tests_Breeding.cpp` -- base-evo derivation (+ an all-species
  terminates-at-stage-1 invariant), compatibility (premises self-guarded via
  `ZM_GetSpeciesData`), exact IV/nature goldens from an INDEPENDENT offline PCG32
  oracle (proving draw order: same-seed identity, knot 5-vs-3, everstone-does-not-
  shift-IVs), mother-ability / base-evo-learnset, and the daycare
  deposit/step/level/255-vs-256-egg-boundary/withdraw/collect model. Adversarial
  review: SAFE-TO-COMMIT (independent oracle guards draw order; base-evo sentinel
  + daycare saturation verified). Minor non-blocking coverage note: no test steps
  a compatible pair strictly PAST 256 in one call (the `>=` branch is covered by
  exact-256 steps; impl saturates correctly). Boot unit baseline 1605 -> 1638
  (`zm-tests.yml`).
- **Reversibility:** additive + standalone; the reduced-data rulings (gender, egg
  groups, egg moves, hidden abilities) are all additive if the data model grows;
  thresholds (256 steps, K=3/5) are named constants. SC2 (`ZM_BattleTower`, which
  consumes `ZM_BattleAI`) completes box 6. No golden/locked-contract impact.

## 2026-07-12 -- ZM-D-044 -- S2 box-5: `ZM_BattleAI` four-tier pure chooser (BOX 5 COMPLETE)

- **Decision:** `ZM_BattleAI` ships as a pure, side-effect-free action chooser in
  `Source/Battle/ZM_BattleAI.{h,cpp}`: `ZM_BattleAction ZM_ChooseAction(const
  ZM_BattleState& xState, ZM_SIDE eSide, ZM_AI_TIER eTier, ZM_BattleRNG& xAIRng)`.
  It reads `xState` through `const&`, takes its OWN caller-supplied `xAIRng`
  (distinct from `xState.m_xRNG`), never calls Submit/Resolve/DoSwitch/
  MoveExecutor, emits no events, mutates no state, and never advances the battle
  RNG. Only the RANDOM tier draws (from `xAIRng`). => choosing an action perturbs
  nothing; all box-1..4 golden streams + subsequent RNG output stay byte-identical.
  The chooser is standalone this box; wiring the engine to call it for the
  opponent side lands when battle integration needs it (S5+).
- **`ZM_AI_TIER` enum** (RANDOM/GREEDY/SMART/CHAMPION + COUNT, NONE=COUNT
  sentinels) is defined in `ZM_BattleAI.h`; the locked `ZM_BattleTypes.h` is NOT
  edited (its reserved comment stays a comment).
- **Legal-action set (deterministic order):** each move slot with PP>0, then
  SWITCH to each living non-active bench member. No ITEM/RUN. Assumes >=1 legal
  action (no Struggle fallback -- pre-existing engine gap, tracked Q-2026-07-12-003).
- **Tier contracts:** RANDOM = uniform over the ordered legal set via
  `xAIRng.RandBelow(n)`. GREEDY = argmax of deterministic `damage(roll=92,
  no-crit, STAB + type-effectiveness) x hit%`, tie-break lowest slot; status /
  zero-power / immune moves score 0; switches only when no legal MOVE exists.
  SMART = fixed cascade: (1) guaranteed KO -- a sure-hit (ALWAYS_HITS or base
  acc>=100) damaging move whose worst-case `roll=85` damage >= target curHP,
  best GREEDY score wins; (2) hopeless switch when `effIn>=200 && effOut<=100`,
  to the switchable bench member with the strictly-smallest incoming
  effectiveness; (3) heal when `curHP*2 < maxHP` and a HEAL_HALF/REST move has
  PP; (4) else GREEDY. CHAMPION = deterministic 2-ply on scalar HP (no
  `ZM_BattleState` clone): model the opponent's single GREEDY reply once, then
  per candidate own move resolve order by priority bracket then effective speed
  (SPEED stage-scaled, /4 if paralyzed -- mirrors `ZM_BattleEngine`), exact
  speed-tie modeled opponent-first (conservative); apply deterministic mean
  damage to two scalars (a KO suppresses the reply); score
  `V = (oppFaint?+100000:0) - (meFaint?+100000:0) + hpMe - hpOpp`; pick max V,
  tie-break GREEDY score then lowest slot. Beats the naive greedy line in the
  priority-trap (slower AI, both moves KO, only the +priority move wins).
- **File-location deviation:** placed under `Source/Battle/` (co-located with the
  nine sibling battle systems), NOT MasterPlan's `Source/AI/` -- no `Source/AI/`
  dir exists and one file does not justify one; trivial + additive to relocate.
  Logged Q-2026-07-12-003.
- **Tests that lock it:** 28 `ZM_Battle` behavioral tests in
  `Tests/ZM_Tests_BattleAI.cpp` (4 RANDOM, 6 GREEDY, 8 SMART, 6 CHAMPION, 4
  API/contract), each pinning a specific action with oracle-computed construction
  preconditions (from the real `ZM_CalcDamage`/`ZM_EffectivenessPercent`/
  `ZM_ApplyStatStage`). Includes a non-perturbation guard (snapshots the battle
  RNG by value, asserts 8 identical post-call draws), a CHAMPION reply-model
  discrimination test (3-way opponent-move separation so a slot-0 / max-power
  reply model fails), and a CHAMPION exact-speed-tie test. Adversarial review:
  SAFE-TO-COMMIT. Boot unit baseline 1577 -> 1605 (`zm-tests.yml`).
- **Reversibility:** additive + standalone; the SMART thresholds and the GREEDY
  roll are S11-tunable named constants; the engine-integration call site is a
  later separate change. No golden or locked-contract impact.

## 2026-07-12 -- ZM-D-043 -- S2 box-4: modern party-share EXP / EV / level / move-learn / terminal evolution (BOX 4 COMPLETE)

- **Decision:** `ZM_ExpAndLevel` ships as deterministic, integer-only progression in
  `Source/Battle/ZM_ExpAndLevel.{h,cpp}`, integrated behind default-false
  `ZM_BattleConfig::m_bAwardExp`. The award-off path changes no progression or
  participation state, draws no RNG, and emits none of `EXP_GAINED`, `LEVEL_UP`,
  `MOVE_LEARNED`, or `EVOLUTION_QUEUED`; legacy streams and subsequent RNG output
  remain byte-identical.
- **Curve + derived-data contract:** FAST=`4L^3/5`, MEDIUM_FAST=`L^3`,
  MEDIUM_SLOW=`6L^3/5 - 15L^2 + 100L - 140` clamped non-negative, and
  SLOW=`5L^3/4`; every curve has L1=0 and caps at L100. Growth derives from rarity
  (COMMON/UNCOMMON/RARE/LEGENDARY -> FAST/MEDIUM_FAST/MEDIUM_SLOW/SLOW). Base EXP
  yield is `max(1, BST*2/5)`. EV yield targets the species' highest base stat
  (ties use the lowest `ZM_STAT` index) for 1/2/3 points by evolution stage.
  Level evolution derives as stage-1 -> stage-2 at L16, stage-2 -> stage-3 at L36,
  final -> none. These remain S11-tunable accessors, not new species-table columns.
- **Award + modern party-share contract:** gross wild EXP is
  `max(1, floor(baseYield*defeatedLevel/7))`; trainer battles apply
  `floor(gross*3/2)` after the `/7` division. Each defeated opponent keeps its own
  bitset of opposing slots that were active against it. Every living participant
  receives an independent full gross award; every living nonparticipant receives
  `max(1, floor(gross/2))`. There is no shared pool or remainder. Fainted
  recipients receive neither EXP nor EV. Award order is participants by ascending
  slot, then nonparticipants by ascending slot. The side mask defaults to
  player-only but can explicitly enable another side.
- **EV contract:** every living recipient receives the defeated species' full EV
  yield even when its EXP share is half. EVs normalize deterministically in
  HP/Atk/Def/SpA/SpD/Spe order, with 252 per-stat and 510 total caps. EV mutation
  happens before EXP/level stat recomputation; a level-capped recipient still gains
  EVs when no EXP can be credited.
- **Current EXP + level contract:** `ZM_BattleMonsterSpec::m_uCurExp` uses an
  explicit UNSPECIFIED sentinel. Omitted input derives the declared level's curve
  floor; supplied values clamp into that level's cumulative-EXP band. Awards clamp
  to the configured cap (`0` = natural L100), emit the credited amount and new
  cumulative total, process every crossed level, recompute all six stats from the
  override-aware base stats, and preserve missing HP instead of fully healing.
- **Move-learning ruling:** each crossed level processes its learnset entries. A
  new move fills the first empty slot with full table PP and emits `MOVE_LEARNED`;
  already-known moves are silent. If all four slots are full, the move is skipped
  with no replacement, pending-choice state, or event. Interactive replacement is
  S5/S6 presentation work.
- **Evolution ruling:** box 4 supports level-trigger evolution only. A monster that
  levelled during battle queues at most one immediate evolution edge during terminal
  settlement, after `TURN_END` and directly before `BATTLE_END`; species never
  mutates mid-battle. Pure `ZM_Evolve()` applies one eligible edge, reloads target
  base stats, recomputes stats, preserves missing HP, and clears transient queue and
  levelled flags. Item/stone evolution is deferred to S9; trade evolution is out of
  scope; friendship evolution is unspecified. No generic trigger schema is reserved.
- **Faint/event ordering:** direct/recoil/contact-result faints are swept after the
  move phase; weather/status/volatile/ability-chip faints are swept after end-of-turn
  damage and abilities but before `TURN_END`. Per-defeated-monster credit state makes
  both sweeps exactly-once. Existing event ordinals and the event POD are unchanged.
- **Tests-that-lock-it:** **67** new tests (**45 `ZM_Data` + 22 `ZM_Battle`**) cover
  literal curve vectors/inverses, hostile bounds, derived accessors, wild/trainer
  awards, EV caps/normalization, current-EXP bands, single/multi-level restats and HP
  carry, move learning, pure one-edge evolution, exact direct and EOT faint streams,
  per-opponent ledgers, recipient order, fainted recipients, EV-before-restat and
  EV-at-cap, reserve move use, ledger reset, terminal evolution/requeue, exactly-once
  credit, and award-off stream/state/RNG identity. Local evidence: regen GREEN;
  Vulkan Debug Tools=True build GREEN; boot baseline **1510 -> 1577** = **1577 ran /
  1576 passed / 0 failed / 1 skipped**; headless automated suite **1/1**; adversarial
  production and test re-reviews GREEN.
- **Reversibility:** additive but golden-sensitive. The default-false gate keeps
  removal localized, but curve formulas, sharing semantics, recipient/event order,
  and payloads now define deterministic goldens and require a new decision if changed.

## 2026-07-12 -- ZM-D-042 -- S2 box-3 SC5: turn-end / faint / quickdraw abilities + all-50 gate (BOX 3 COMPLETE)

- **Decision:** SC5 is the FINAL box-3 slice -- it installs the last **9** ability
  rows (34, 35, 36, 41-46), closing all 50. Roster + slots:
  - **TURN_END heals (`pfnOnTurnEnd`):** RAINBASK(41)/SUNBASK(42)/ICEBOUND(43) heal
    maxHP/16 in RAIN/SUN/SNOW; ROOTFEED(45) unconditional maxHP/16; TOXICTHRIVE(44)
    maxHP/8 while POISON/TOXIC AND its poison chip is skipped.
  - **FAINT branch:** BLOODRUSH(34) `pfnOnDealtFaint` -> own ATTACK +1 on downing a
    foe; LASTSPITE(35) `pfnOnContact` bSelfFainted -> attacker used-move PP->0;
    AFTERSHOCK(36) `pfnOnContact` bSelfFainted -> chip attacker maxHP/4 [+FAINT].
  - **QUICKDRAW(46):** engine-side 30% -> +1 move-order priority; row stays `{}`
    (MODIFY_STAT realized engine-side -- the sole all-null-pfn row by design).
- **Orchestrator rulings (resolved the spec's flagged ambiguities):** (1) **BLOODRUSH
  fires on ANY damaging KO** by the holder's move (contact OR non-contact), not
  contact-only -- matches the FAINT-mask semantics + GDD "downs a foe"; guarded on
  (defender fainted this hit) AND (attacker still alive) so recoil/counter self-KOs
  don't trigger it. (2) **QUICKDRAW emits `ABILITY_TRIGGER` on a successful proc**
  (consistency + testability; the draw is gated on a live holder so NONE goldens are
  unperturbed). (3) A non-serialized **`m_uOtherMoveSlot`** field was added to
  `ZM_AbilityContext` (a view field, NOT the event POD -> save format unaffected) so
  LASTSPITE can identify the attacker's used move.
- **Seams:** a file-local `g_DispatchTurnEndAbilities` free fn (behavior-equivalent to
  a member method; keeps `ZM_BattleEngine.h` out of scope) dispatched at **EoT step 6**
  -- AFTER both PLAYER-then-ENEMY status ticks, BEFORE the final `TURN_END`, skipping
  fainted actives, zero RNG; the QUICKDRAW proc in `ResolveMovePhase` (gated on a live
  holder, PLAYER-then-ENEMY, move-ORDER only -- flee/`g_EffectiveSpeed` untouched); a
  new `g_ApplyDealtFaintReaction` after both contact sites in `ZM_MoveExecutor`; and an
  ability-gated TOXICTHRIVE poison-chip skip in `ZM_StatusLogic::EndOfTurn` (TOXIC ramp
  counter frozen while thriving).
- **Heal + weather-chip encoding:** each heal emits `ABILITY_TRIGGER` then `HEAL`
  (`m_iAmount`=heal, `m_iAux`=new HP); a full-HP holder emits nothing; heal is min 1,
  capped to missing HP. The LOCKED EoT order means a SAND/SNOW chip (maxHP/8 for a
  non-immune active; SNOW-immune = ICE) lands FIRST, then the heal -- so an ICEBOUND
  holder in SNOW nets `-maxHP/8 + maxHP/16`; the heal test now models and asserts that
  chip-then-heal ordering (a review-caught test-fixture bug that assumed heal-only was
  fixed before landing; production was correct).
- **Contracts preserved:** zero-perturbation for NONE actors -- the ~260 box-1/SC1-SC4
  goldens stay byte-identical (confirmed at the gate); `ZM_BattleEvent` POD append-only
  (no new kind/field -- reuses HEAL/ABILITY_TRIGGER/STAT_STAGE_CHANGED/FAINT); RNG draw
  order + EoT order unchanged. Independently adversarially reviewed: production correct
  across all 9 abilities + all 4 new seams, zero-perturbation audit PASS, the all-50
  gate genuinely proves realization (not vacuous), no false-confidence tests.
- **Tests-that-lock-it:** **19** net-new unit tests (boot baseline **1491 -> 1510**,
  bumped in `.github/workflows/zm-tests.yml`) -- 18 behavioral (per-ability
  positive+control incl. full-HP/min-1/cap-to-missing heal edges, the fainted-active
  guard, TOXICTHRIVE net-heal + chip-skip, BLOODRUSH contact AND non-contact KO + the
  +6 cap + recoil-self-KO guard, LASTSPITE/AFTERSHOCK bSelfFainted, QUICKDRAW
  proc/order/flee-unaffected, a NONE-actor zero-draws/zero-events invariant, and a
  2,000-battle ability+weather soak) + the **all-50 realization gate** (3 coverage
  tests: every row realizes its declared mask, the 6+20+15+9 SC sets partition all 50
  exactly once, and QUICKDRAW is the SOLE all-null-pfn row).
- **BOX 3 COMPLETE.** All 50 ability rows shipped across SC1-SC5; the Roadmap S2 box-3
  line is ticked. Reversibility: additive -- SC5 only APPENDED, so earlier goldens
  never shifted.

## 2026-07-11 -- ZM-D-041 -- S2 box-3 SC4: contact / status-try / stat-veto / accuracy abilities (15 rows)

- **Decision:** SC4 installs the next **15** ability rows as live hook
  function-pointers, reusing the SC2/SC3 `ZM_AbilityHooks` / `ZM_AbilityContext`
  / 50-row table. Rows and slots:
  - **CONTACT (`pfnOnContact`, rows 7-10):** STATICVEIL 30% -> paralyze attacker,
    CINDERSKIN 30% -> burn, BARBSKIN 30% -> poison, THORNMAIL -> chip attacker
    **maxHP/8** (min 1, NO RNG draw).
  - **STATUS_TRY (`pfnPreventMajor` / `pfnPreventVolatile`, rows 28-33):** WAKEFUL
    (SLEEP), PUREBLOOD (POISON+TOXIC), THAWHEART (FREEZE), LIMBERLITHE (PARALYSIS),
    COLDBLOOD (BURN), OWNPACE (CONFUSED volatile).
  - **stat-drop-veto (`pfnPreventStatDrop`, rows 25/48/26):** IRONWILL + GUARDIAN
    veto ANY foe-sourced stat DROP; KEENEYE vetoes ONLY a foe ACCURACY drop.
  - **ACCURACY (`pfnBypassAccuracy`, rows 27/49):** DEADAIM always auto-hits;
    TRUESHOT auto-hits only when weather != NONE.
  The three FAINT-branch abilities (BLOODRUSH 34, LASTSPITE 35, AFTERSHOCK 36)
  are deferred to **SC5** (spec check corrected an earlier assumption that put them
  in SC4).
- **Seams:** a new **E4 `g_ApplyContactReactions`** in `ZM_MoveExecutor::Execute`
  (fires once per move, only for a contact move that connected, skips if the
  attacker is already fainted [R-C2], dispatches only the DEFENDER's `pfnOnContact`
  -- the attacker-side `pfnOnDealtFaint` is SC5); an **M5 accuracy-bypass wrapper**
  around the existing accuracy check (the pre-existing draw is only WRAPPED, not
  reordered, so a NONE actor's `<100`-acc draw is byte-identical and the
  `>=100`/ALWAYS_HITS short-circuit is untouched); a new **state+events `ApplyMajor`
  overload** in `ZM_StatusLogic.{h,cpp}` (parallel to SC2's `ApplyStatChange`
  overload) so contact abilities status the attacker from an ability context. The
  stat-drop veto seam was already live (SC2) -- SC4 only adds the three predicate bodies.
- **R-S1 -- refinement of ZM-D-036 §4.5 (orchestrator ruling):** the STATUS_TRY
  veto+emit lives **inside** `ApplyMajor` / `ApplyVolatile`, with
  `CanApplyMajor` / `CanApplyVolatile` kept **pure** -- NOT in the predicates as
  §4.5 read literally. Rationale: `CanApplyVolatile` is ALSO the secondary-effect
  preflight in `g_ApplyDamagingSecondary`, so a predicate-level veto would suppress
  an OWNPACE holder's confuse-proc RNG draw, breaking the locked roll-then-veto
  order and the NONE-ability goldens. The chosen placement is behavior-equivalent
  to §4.5's intent (Resolution 4) and mirrors the SC2 `ApplyStatChange` seam.
- **R-C1 (event encoding):** Thornmail reuses `ABILITY_TRIGGER`'s free fields
  (`m_iAmount` = chip, `m_iAux` = attacker new HP, `m_iTag` = THORNMAIL) + a trailing
  `FAINT` iff lethal -- no new event kind. A STATUS_TRY block emits `ABILITY_TRIGGER`
  (blocked-status ordinal in `m_iAmount`) and NO `STATUS_APPLIED`; a stat-drop veto
  emits `ABILITY_TRIGGER` with `m_iAmount=0`; an accuracy bypass emits NOTHING
  (observable only as auto-hit).
- **Contracts preserved:** zero-perturbation for NONE actors -- RNG sequence and
  events byte-identical, so the ~260 box-1/SC1-SC4 goldens stay green; each contact
  proc is exactly one `RandBelow(100) < 30` per contact hit for a LIVE proc ability;
  `ZM_BattleEvent` POD stays append-only; RNG draw order + EoT order untouched.
  Independently adversarially reviewed: no production/test defects, zero-perturbation
  audit PASS, no false-confidence tests, coverage gates correct.
- **Tests-that-lock-it:** **15** behavioral battle tests (paired identical-seed
  positive+control, stochastic seeds picked by a PCG32 oracle, `0xB32A`-`0xB365`)
  plus the two coverage-invariant gates advanced SC3 -> SC4 (all 15 slots realize
  their declared masks; `pfnOnTurnEnd` / `pfnOnDealtFaint` and BLOODRUSH/LASTSPITE/
  AFTERSHOCK stay asserted null for SC5). Boot baseline **1476 -> 1491** (bumped in
  `.github/workflows/zm-tests.yml` this commit).
- **Reversibility:** additive. The Roadmap S2 box-3 line stays UNCHECKED until SC5
  installs the turn-end / faint / quickdraw abilities and adds the all-50
  complete-realization gate; SC4 only APPENDS so earlier goldens never shift.

## 2026-07-11 -- ZM-D-040 -- S2 box-3 SC3: damage / stat / type-interaction abilities (20 rows)

- **Decision:** SC3 installs the next **20** ability rows as live hook
  function-pointers, reusing the SC2 `ZM_AbilityHooks` aggregate / `ZM_AbilityContext`
  / 50-row table unchanged. Rows and the hook slot each realizes:
  VERDANTSURGE / EMBERSURGE / TIDALSURGE / HIVESURGE -> `pfnModifyDamageDealt`
  (own STAB-type move **x3/2** at **<=1/3 HP**, type-gated); SKYWARDGRACE /
  AQUIFER / DYNAMO / GRAZER -> `pfnTypeInteraction` (return 1 = immune,
  2 = absorb + heal **maxHP/4**); CINDERDRINK -> BOTH `pfnTypeInteraction`
  (fire absorb-heal) AND `pfnModifyDamageDealt` (own FIRE **x3/2**, no HP gate);
  BEDROCK / BLUBBER / SOLIDCORE / HEAVYPLATE / GOSSAMER / DOWNDRAFT ->
  `pfnModifyDamageTaken` reducers (SOLIDCORE x3/4 on super-effective;
  HEAVYPLATE/GOSSAMER x2/3 on PHYSICAL/SPECIAL; BLUBBER x1/2 FIRE|ICE;
  DOWNDRAFT x1/2 SKY; BEDROCK per its SC-earlier body); SUNCHASER / STREAMLINE /
  GRITSTRIDE / RIMESTRIDE / FERVOR -> `pfnModifyStat` (weather/status-gated stat
  boosts; FERVOR = ATTACK x3/2 while statused).
- **Seam placement (ZM-D-036):** ability damage mods apply AFTER `ZM_CalcDamage`
  (crit/effectiveness/weather/screen/burn already baked in), dealt-then-taken,
  with a `>=1` floor so a resist ability never zeroes a nonzero hit.
  `pfnTypeInteraction` resolves on the DEFENDER before the chart-immunity gate
  and before any crit/roll draw. The move-order speed seam
  (`g_EffectiveSpeedForMoveOrder`) affects the equal-priority ORDER branch ONLY;
  `DoRunAction`/flee keeps raw `g_EffectiveSpeed` (ZM-D-036(c)).
- **Contracts preserved:** no hook draws RNG -> a NONE-ability actor's stream is
  byte-identical; `ZM_BattleEvent` POD, RNG draw order, and EoT order are
  untouched (append-only discipline holds). The 4 weather-speed abilities realize
  their WEATHER bit engine-side (weather read inside the modify-stat body), not as
  a pfn (matches ZM-D-036(a)).
- **Tests-that-lock-it:** SC3 adds **30** `ZM_*` unit tests (boot baseline
  **1446 -> 1476**, bumped in `.github/workflows/zm-tests.yml` this commit) --
  the SC3 mask-realization + installation-state invariants (all 20 masks fully
  realized; every live slot maps to a declared bit) plus **20** behavioral
  battle tests, each a paired identical-seed positive+control (ability on vs off /
  wrong type -> exact delta, correct trigger count, RNG unperturbed). FERVOR is
  pinned by stat-INPUT equivalence (a x3/2-boosted twin), not formula
  re-derivation; the four absorb abilities drive the `ZM_Box3SC3AssertAbsorb`
  helper (previously dead scaffold, now wired -- no dead SC3 scaffolding remains).
  Reviewed by two independent read-only passes: production 20/20 correct with no
  bugs; the missing-behavioral-test gap was caught and closed before landing.
- **Reversibility:** additive. The Roadmap S2 box-3 line stays UNCHECKED until
  SC5 installs the remaining ability rows and adds the all-50 complete-realization
  check; earlier goldens never shift because SC3 only APPENDS.

## 2026-07-11 -- ZM-D-039 -- S2 box-3 SC2: ability-hook infrastructure + SWITCH_IN abilities

- **Decision:** The executable ability surface is a plain-function-pointer
  `ZM_AbilityHooks` aggregate with **12 slots over the existing 11
  `ZM_ABILITY_HOOK` bits**, plus a compiled `const` **50-row** table keyed by the
  stable `ZM_ABILITY_ID`. `ZM_AbilityContext` is a non-owning view of the
  engine-owned battle state, append-only event sink, owner side, and opposing
  side. `ZM_GetAbilityHooks` returns the stable row for every real id and safely
  returns `nullptr` for `ZM_ABILITY_NONE` or any out-of-range id; callers never
  feed the sentinel into the asserting metadata accessor. SC2 installs only the
  six SWITCH_IN rows (DAUNTINGROAR, the four `*CALLER`s, and PRESSUREAURA); the
  remaining slots stay null until their ordered SC3-SC5 implementations.
- **Switch-in contract:** ability dispatch runs immediately **after** the
  corresponding `SWITCH_IN` event. `Begin` resolves PLAYER lead + hook, then
  ENEMY lead + hook; `DoSwitch` uses the same post-event dispatch. RAINCALLER,
  SUNCALLER, SANDCALLER, and SNOWCALLER set their weather for exactly five
  turns, emit `WEATHER_CHANGED` then `ABILITY_TRIGGER`, and treat an already-
  identical weather as a strict no-op (no refresh and no events). Later
  switch-ins overwrite a different weather and preserve the previous-weather
  event tag.
- **Daunting Roar / Pressure Aura contract:** Daunting Roar lowers the opposing
  active's ATTACK through the promoted shared
  `ZM_StatusLogic::ApplyStatChange` clamp/event seam, then announces its
  `ABILITY_TRIGGER`; the `bFromFoe` parameter reserves the SC4 stat-drop-veto
  hook without changing NONE-ability behavior. A move targeting the opposing
  PRESSUREAURA holder costs **2 PP**, clamped at zero. SELF and FIELD targets,
  and opponent-target moves against a NONE-ability defender, continue to cost
  exactly 1 PP. Pressure Aura announces on switch-in rather than adding a
  per-move event.
- **Determinism + staged coverage:** SC2 adds **zero RNG draws**. A NONE ability
  still adds zero events, draws, or state changes, and the pre-box-3 stream is
  byte-identical. Coverage is deliberately staged: every currently installed
  row must realize each declared bit via a live pfn or the explicit engine-side
  exception table, every live pfn must have a declared bit, the exact 12-ability
  WEATHER whitelist rejects unrelated declarations, and no fake future hook is
  installed merely to satisfy coverage. SC3 and SC4 extend the installed set;
  the all-50 complete-realization gate belongs to SC5.
- **Why:** the table preserves the repo's no-`std::function` rule, stable data ids,
  append-only event protocol, and deterministic NONE path while giving later
  ability slices one typed dispatch surface. Centralizing stat mutation avoids
  duplicating clamp/event semantics between moves and switch-in hooks.
- **Tests that lock it:** **18 new unit cases** (5 `ZM_Data`, 13 `ZM_Battle`)
  cover the 12-slot/sentinel table contract, staged and converse realization
  rules, the explicit engine-side rosters, all four weather callers, Begin and
  DoSwitch ordering, overwrite and identical-weather anti-refresh, Daunting
  Roar + shared clamp/event encoding, Pressure Aura's 2-PP path + zero clamp and
  its SELF/FIELD/NONE-defender 1-PP negatives, non-SWITCH_IN silence, and the
  exact NONE event/RNG stream. Local evidence: Vulkan tools build GREEN; boot
  unit gate **1446 registered = 1445 passed / 0 failed / 1 skipped**; headless
  automated boot **1/1**; implementation/test reviews GREEN after both
  SHOULD-FIX test gaps were closed.
- **Reversibility:** high and localized. Remove the new hook table/context,
  switch-in dispatch, Pressure Aura M1 predicate, shared stat wrapper, and SC2
  tests/baseline to return to the SC1 weather-only state. The shared stat helper
  is additive and may be retained independently; no save schema, event ordinal,
  data-table id, or RNG phase changed.

---

## 2026-07-11 -- ZM-D-038 -- Terrain fixed-size/fixed-density limitation accepted as deferred engine TODO (E6)

- **Decision:** Zenith's terrain system cannot currently give different terrain instances
  different world-space extents while holding triangle density (metres/vertex) constant --
  `Flux_TerrainConfig::CHUNK_GRID_SIZE`/`CHUNK_SIZE_WORLD`/`TERRAIN_SIZE` are global
  `static constexpr` values (`Flux_TerrainConfig.h:27-36`), so every terrain is a fixed
  4096x4096 m grid, and density (`fLowLODDensity`, `Zenith_TerrainComponent.cpp:493`) is
  likewise a fixed global, not a per-instance field. E2's rect export only crops that same
  fixed grid (a bake-time/file-count optimization), it does not resize it. This was
  investigated on request (a route and a city should be different world-space sizes at the
  same density) and confirmed as a real gap, already implicitly acknowledged in the E2
  rationale ("compile-time constants pervade streaming/grass") but not previously called
  out as its own tracked item.
  User has **accepted the Zenithmon plan as-is with this limitation** (routes and the
  city will all bake at the same fixed 4096x4096 grid/density for now) rather than block
  or descope S3+ on it. Logged as **Shortfalls.md E6 / MasterPlan.md engine-changes E6**:
  DEFERRED, revisit as a dedicated engine initiative after Zenithmon ships.
- **Why:** fixing this properly (per-instance serialized grid/density fields, dynamically
  sized streaming-manager GPU/CPU buffers, parametrized culling/streaming-radius/grass math)
  is a materially larger engine change than anything else in scope for Zenithmon, and
  Zenithmon does not strictly need it to ship (every outdoor scene can still be its own
  terrain SET per E1 -- it's the per-set world-space size/density that's inflexible).
- **How to apply going forward:** every terrain-touching task for the rest of Zenithmon
  development (S3 first-overworld, S9/S10 world buildout, any terrain-authoring or
  streaming work) must treat the 4096x4096 fixed grid/density as a hard current constraint
  -- do NOT author content or write game-side code that assumes per-scene size/density will
  become configurable mid-project, and do not build a content-side workaround that
  papers over it (e.g. do not silently scale down non-terrain content to fake a smaller
  world, do not hand-roll a second grid system in game code). If a route's designed
  world-space size doesn't fit the fixed grid, that's a WorldSpec/authoring-scale decision
  to route through Questions.md, not a silent workaround.
- **Tests that lock it:** none (this is a documented limitation, not a coded rule) --
  E1/E2's existing unit tests + RenderTest boot regression remain the only terrain-system
  coverage until E6 is picked up.
- **Reversibility:** N/A (acceptance of a known limitation, not an implementation). E6
  itself, when eventually done, is additive per the standard engine-change convention.

---

## 2026-07-11 -- ZM-D-037 -- Terrain bake-time fallback: optimize the pipeline, not shared terrain sheets

- **Decision:** One terrain set per outdoor scene/route is a **hard requirement**, not
  negotiable against bake time. The previously documented fallback for Q-2026-07-09-002
  ("multiple routes share one terrain sheet" if the S3 bake-time measurement comes back too
  slow) is **retracted**. If the S3 measurement (bake 3 real scenes, extrapolate to ~25)
  shows bakes are too slow, the fallback is instead to **optimize the terrain bake
  pipeline** -- e.g. parallelize chunk export across cores/processes, incrementalize/cache
  unchanged chunks so re-bakes aren't full cold bakes, profile the actual hot path and cut
  it -- not to reduce the terrain-set count below one-per-scene.
- **Why:** shared terrain sheets across routes/scenes is a visible content compromise (reused
  ground geometry/texturing across supposedly distinct locations) that the user considers
  unacceptable for a ~25-outdoor-scene RPG; user directive overrides the prior cost-driven
  fallback plan.
- **Tests that lock it:** none yet -- S3's terrain-bake measurement task (Roadmap.md) must
  report against this constraint; if bakes measure too slow, the S3 follow-up is a bake-
  pipeline optimization pass, not a terrain-set reduction.
- **Reversibility:** high -- no implementation has happened yet under the old fallback;
  this is a plan correction ahead of S3, recorded in Questions.md Q-2026-07-09-002,
  MasterPlan.md risk #1, Shortfalls.md, and AssetManifest.md section 6.3.

---

## 2026-07-11 -- ZM-D-036 -- S2 box-3 execution plan (abilities + weather) + SC1 weather core

- **Decision:** Box 3 ("Abilities via per-hook fn-pointer structs (~50) + weather
  rain/sun/sand/snow") ships as **5 ordered sub-commits**, mirroring how box 2 (ZM-D-033)
  landed: **SC1** weather core (this commit); **SC2** ability hook infra + SWITCH_IN
  abilities; **SC3** damage-dealt/taken + modify-stat + type-immunity abilities; **SC4**
  status-try + contact + stat-veto + accuracy abilities; **SC5** turn-end + faint + quickdraw
  + fuzz soak. Abilities are a `const` fn-pointer hook table keyed by `ZM_ABILITY_ID`
  (repo mandate: `std::function` -> plain fn pointers), dispatched at documented turn-loop
  seams; the `ZM_AbilityData.m_uHookMask` (ZM-D-026) stays the coverage/declaration record.
- **Keystone determinism invariant (holds for all of box 3):** a `ZM_ABILITY_NONE`,
  weather-`NONE`, status-free actor pulls **ZERO** new RNG draws and emits **ZERO** new
  events, so box-1/2 goldens and the 2,000-battle soak stay byte-identical. New draws are
  added only at documented points and only when the relevant ability/weather is present.
  The `ZM_BattleEvent` POD is append-only: the 5 box-3 ordinals 26-30
  (`ABILITY_TRIGGER`/`WEATHER_CHANGED`/`WEATHER_DAMAGE`/`SCREEN_SET`/`SCREEN_EXPIRED`) were
  already reserved -- box 3 lights them, adds no ordinal and **no new field** (all payloads
  fit the existing 8 scalars). Ability damage multipliers apply **outside** the pure
  `ZM_CalcDamage` (post-calc in `ApplyDamagingHit`), so its isolated pure-fn goldens never move.
- **Box-3-wide design rulings (from the 3-survey / synth / adversarial-critique design panel;
  full design archived by the session):** (a) the ability-hook **coverage invariant** is
  "every set `ZM_ABILITY_HOOK` bit is *realized* by an enumerated mechanism -- a live pfn
  slot OR a documented engine-side handler", NOT "every bit has a backing pfn" (subsumes the
  weather-bit condition + the QUICKDRAW/DAUNTINGROAR/BLOODRUSH engine-side realizations);
  (b) STATUS_TRY abilities **emit `ABILITY_TRIGGER` on a successful block** (SC4); (c) flee
  (`DoRunAction`) keeps the un-ability-modified speed -- ability speed mods apply only to
  move-order resolution (SC3); (d) the confusion-self-hit `ZM_CalcDamage` stays un-weathered
  (typeless; avoids a box-2 golden shift).
- **SC1 (this commit) implements weather core, no abilities:** the weather **damage
  multiplier** via the existing `uWeatherNum/uWeatherDen` seam in `ApplyDamagingHit` (RAIN:
  WATER x3/2, FIRE x1/2; SUN: FIRE x3/2, WATER x1/2; SAND/SNOW/NONE = 1/1); the **end-of-turn
  weather chip** (new `ResolveWeatherEndOfTurn`, resolved FIRST in `ResolveEndOfTurnPhase`
  before the status ticks) -- SAND/SNOW only, `maxHP/8` min 1, PLAYER-active then ENEMY-active,
  SAND immune = EARTH/STONE/IRON, SNOW immune = ICE, underflow-clamped, `WEATHER_DAMAGE`
  [+ `FAINT`]; **weather + screen countdown/expiry** (`ResolveWeatherEndOfTurn` +
  new `ResolveScreenEndOfTurn`) with `WEATHER_CHANGED`/`SCREEN_EXPIRED`; **`WEATHER_CHANGED` /
  `SCREEN_SET` emission** in `g_ApplyField` (move setters now announce; overwrite re-announces);
  and a new shared free fn `ZM_BattleMonsterHasType` (promotes the file-static `g_HasType`
  logic). Event encodings per the design's section 2.6 (e.g. `WEATHER_CHANGED{m_uSide=ZM_SIDE_COUNT,
  m_iAmount=newWeather, m_iAux=turns, m_iTag=prevWeather}`). Zero new RNG draws; `ZM_DamageCalc`
  untouched.
- **Sanctioned golden churn:** exactly 4 box-2 tests that asserted box-3 events are NOT
  emitted / a MOVE_USED-only stream were tightened (`ZM_CheckWeatherSetter`,
  `Weather_SetterEmitsOnlyMoveUsed` -> `...MoveUsedThenWeatherChanged`, the two `Screen_*Wall_Sets*`);
  every other box-1/2 golden stays byte-identical (verified: 0 failed).
- **Why fn-pointers + post-calc ability mults + append-only events:** preserves the locked
  box-1/2 goldens and the deterministic RNG draw order (ZM-D-032/033) while giving each of the
  50 declared abilities a real body; keeps `ZM_CalcDamage` a pure, independently-golden fn.
- **Tests that lock it:** the `Box3SC1_*` block in `Tests/ZM_Tests_Battle.cpp` (28 cases:
  pure-seam + end-to-end weather multiplier vectors; SAND/SNOW chip incl. immunity, min-1
  clamp, chip-KO `FAINT` + underflow branch, skip-fainted-active, PLAYER-then-ENEMY order,
  chip-before-status ordering; weather + physical/special screen countdown/expiry with full
  event encodings; overwrite re-announce; and the
  `Box3SC1_WeatherFree_ScreenFree_AddsNoEventsDrawsOrState` zero-draws/zero-events regression
  wall). Boot unit gate **1400 -> 1428** (bumped in `.github/workflows/zm-tests.yml` this
  commit). Independent test/impl authoring (blind parallel authors) + a 2-lens adversarial
  review (impl-correctness + test-tautology) gated the commit.
- **Reversibility:** moderate. Weather logic is localized to `ResolveWeather/ScreenEndOfTurn`
  + the `ApplyDamagingHit` seam + `g_ApplyField`; the shared `ZM_BattleMonsterHasType` is
  additive. Reverting SC1 alone would re-inert weather but leave the reserved event ordinals.

## 2026-07-11 -- ZM-D-035 -- S2 box-2 SC6: capture / flee math + pre-move SWITCH/ITEM/RUN (box 2 COMPLETE)

- **Decision:** SC6 is the final box-2 sub-commit (ZM-D-033). It adds `ZM_CatchCalc`
  (new `Source/Battle/ZM_CatchCalc.{h,cpp}`) and the engine's pre-move
  SWITCH / ITEM(catch) / RUN actions in `ResolvePreMovePhase`, and promotes the box-1
  50-battle smoke to the **deterministic 2,000-battle soak**. Every move-only turn
  stays BYTE-IDENTICAL: the pre-move phase is inert unless a side submits a non-MOVE
  action, and the move phase only draws the speed tie-break when BOTH sides submit
  MOVE (the box-1 path is unchanged). Box 2 is now complete.
- **Capture math (LOCKED; integer, no floating point -- an internal choice within the
  ZM-D-033 Q2 contract, like box-1's rounding sub-decisions):** base rate from
  `ZM_RARITY` (COMMON 190 / UNCOMMON 120 / RARE 45 / LEGENDARY 3, via
  `ZM_CatchCalc::BaseCatchRate` -- NO new S1 column). Ball bonus = the item row's
  catch param x10 (`ZM_ItemData`); PRIMEORB's param 255 is the guaranteed-capture
  sentinel (master-ball analog). Status bonus (Gen-IV, Q2): sleep/freeze x5/2,
  paralysis/poison/toxic/burn x3/2, none x1. Modified value
  `a = (3*maxHP - 2*curHP) * rate * ballX10 / (3*maxHP*10)`, then `a = a*num/den`
  (min 1). `a >= 255` (or a guaranteed ball) captures with ZERO draws. Otherwise the
  Gen-III/IV integer shake gate `b = 1048560 / isqrt(isqrt(16711680 / a))` gates four
  `RandBelow(65536) < b` checks, stopping at the first failure; four passes == caught.
  Conditional ball bonuses (net/dusk/quick) are DEFERRED (Shortfalls.md 1.2).
- **Flee math (LOCKED):** `selfSpeed >= oppSpeed` (or opp 0) is a guaranteed escape,
  no draw; otherwise `f = (selfSpeed*128 / oppSpeed + 30*attempt) mod 256` gates one
  `RandBelow(256) < f` check. `attempt` is 1-based and ramps across repeated runs.
  Effective speed is the same stat-stage/paralysis fold used for turn order.
- **Pre-move phase contract (ZM-D-035):** pre-move actions resolve BEFORE any move in
  fixed **PLAYER-then-ENEMY** order (the EOT side-order convention). A successful
  capture ends the battle with the CATCHING side as winner; a successful flee ends it
  with NO winner (`ZM_SIDE_COUNT`). Either closes the turn directly: `TURN_END` then
  `BATTLE_END` (skipping the move + end-of-turn phases), so the turn stays balanced.
  A voluntary SWITCH reuses the SC5 `DoSwitch` primitive; a TRAPPED active reports
  `MOVE_FAILED(TRAPPED)` and an otherwise-illegal destination reports
  `MOVE_FAILED(NO_SWITCH_TARGET)` -- either way that side does not move. When exactly
  one side submits MOVE, the move phase runs only that mover with NO tie-break draw.
- **Event encodings (append-only; box-1 goldens unchanged):** `CATCH_SHAKE`
  (m_iAmount = shake index 1..4, m_iTag = ball id) once per wobble; `CATCH_RESULT`
  (m_iAmount = caught 1/0, m_iAux = shake count, m_iTag = ball id); `FLEE` /
  `FLEE_FAILED` (side = the runner). `ZM_MOVE_FAIL_TRAPPED` appended to
  `ZM_MOVE_FAIL_REASON`. ITEM in SC6 supports ball items only (medicine/battle items
  are box 5); ITEM/RUN require a wild config.
- **Why:** turn order puts run/item/switch before moves (GDD), so the pre-move seam is
  the correct attachment point; gating every new draw on non-MOVE actions keeps the
  ~200 box-1/SC1-SC5 goldens byte-identical while lighting the CATCH/FLEE event kinds.
  Pure integer catch/flee math keeps deterministic replay; the offline oracle
  (`scratchpad/zm_catch_ref.py`, an independent PCG32 reimplementation) derived every
  expected `a`/`b`/shake/flee value so the tests are not engine echoes.
- **Tests that lock it:** 21 SC6 `ZM_Battle` cases -- catch base rate / status
  multipliers / ball param+guaranteed / modified-value + shake-probability vectors;
  Roll (non-guaranteed caught+escaped, guaranteed-by-value, guaranteed-by-ball,
  legendary-full-HP escape); flee odds + roll (guaranteed + computed both ways); engine
  catch (guaranteed ends battle, non-guaranteed 4-shake, escape continues the turn);
  engine run (guaranteed + failed); engine switch (voluntary, trapped, invalid target);
  and the move-only wild==trainer byte-identity check. Plus the promoted
  `Fuzz_Soak_2000Battles_Invariants` (2000 seeded battles, half wild with periodic
  catch/run; termination < 500 turns + HP/PP/stage/stream invariants). Boot unit gate
  1400 ran / 0 failed (baseline 1379 -> 1400).
- **Reversibility:** moderate. Localized to `Source/Battle/` (+ the two new files),
  but the catch/flee formulas, the guaranteed-ball sentinel, the pre-move side order,
  the catch/flee winner semantics, and the CATCH/FLEE event encodings now define
  deterministic replay goldens -- change them only with a new decision + updated oracle.

## 2026-07-11 -- ZM-D-034 -- S2 box-2 SC5 volatile, Endure, and switch contract

- **Decision:** SC5 completes the GDD's exact ten battle-local volatile bits in
  `ZM_BattleMonster::m_uVolatileMask`: `CONFUSED`, `FLINCH`, `LEECH_SEED`,
  `PROTECT`, `CHARGE`, `SEMI_INVULN`, `RECHARGE`, `LOCK`, `TRAP`, and `TAUNT`.
  `ENDURE` is deliberately **not** an eleventh volatile: it is the separate
  one-turn `m_bEndureThisTurn` flag. `ZM_StatusLogic` owns apply/end/reset,
  counters, metadata, PHASE-G gates, and end-of-turn processing; the executor
  owns effect routing and M-phase intercepts. Every application emits
  `VOLATILE_APPLIED` (`m_iAmount=duration`, `m_iAux=volatile bit`), every explicit
  expiry emits `VOLATILE_ENDED` (`m_iAux=volatile bit`), and FLINCH cancellation
  additionally has its dedicated `FLINCH` event.
- **Duration/counter contract:** counters are independent of the major-status
  counter. CONFUSED draws `RandRange(1,4)` on a successful new application and
  counts G5 action attempts (decrement after the pass/self-hit result, then end at
  zero). TRAP draws `RandRange(4,5)`, chips on every EOT including its application
  turn, and decrements only after a surviving tick. TAUNT is 3 EOTs, no draw.
  LOCK_IN draws 2-3 total executed uses: its establishing hit is use one, so the
  stored remaining counter starts at `duration-1`; each later use that reaches M1
  consumes one even if it then misses/is intercepted/is immune, while a PHASE-G
  cancellation consumes neither PP nor a locked use; zero PP ends the lock early.
  RECHARGE is one G1-cancelled action. FLINCH and PROTECT end at EOT (a repeated
  Protect refreshes and re-emits APPLIED without an intermediate ENDED); Endure
  clears silently at EOT. CHARGE/SEMI_INVULN do not count down at EOT: turn one
  spends PP + emits MOVE_USED then stores the slot; the forced release emits
  MOVE_USED without a second PP and clears both bits before M2/M3 intercepts. A
  cancelling G gate clears a pending charge/semi-invulnerable pair.
- **Gate/intercept + RNG contract:** the ZM-D-033 order is now fully live:
  `G1 RECHARGE -> G2 FREEZE -> G3 SLEEP -> G4 FLINCH -> G5 CONFUSE -> G6
  PARALYSIS`, first cancellation wins, before PP/MOVE_USED. G1/G4 draw nothing.
  G5 draws `RandBelow(100)<33` once per confused action; self-hit then draws only
  `RandRange(85,100)` and applies 40-power typeless physical damage (no accuracy,
  crit, STAB, type, weather, or screen; burn still halves it), tagged as volatile
  damage and able to faint the user. M0 TAUNT blocks STATUS moves before PP;
  M2 PROTECT intercepts opponent-target moves; M3 SEMI_INVULN intercepts the same;
  M4 establishes two-turn state before accuracy. Duplicate/type-blocked volatile
  secondaries and FORCE_SWITCH with no target are preflighted **before** the E3
  proc gate, so they draw nothing; chance >=100 draws nothing; a fainted defender
  receives no secondary. A volatile-free battle therefore adds zero draws, events,
  or state changes and preserves the box-1 goldens.
- **Individual mechanics:** Leech Seed rejects GRASS, stores its source side, and
  persists until switch; Protect targets self; Endure clamps otherwise-lethal
  direct move damage (ordinary, multi-hit, fixed, half-HP, OHKO) to 1 HP but does
  not protect from recoil/confusion/EOT damage. Swagger applies the Attack stage
  change first and then CONFUSED; either component may succeed independently, and
  MOVE_FAILED is emitted only when both are blocked. RECHARGE/LOCK establish only
  after a connecting standard damaging hit (a KO still counts); charge and
  semi-invulnerability use the submitted stored move slot.
- **End-of-turn + tag contract:** the box-3 weather slot remains reserved first;
  box 2 then processes sides in fixed **PLAYER-then-ENEMY order (not speed order)**.
  Each side runs `major chip -> Leech Seed -> Trap -> cleanup/expiry`; after both
  sides, the engine emits `TURN_END`. Leech/Trap are max-HP/8, minimum 1. A major-status KO suppresses
  later chip sources for that side but cleanup still runs; a Trap KO emits no later
  expiry. Leech heals the source side's current living active by actual HP restored,
  so a source switch redirects the drain. Damage-event `m_iTag` uses a major-status
  ordinal on `STATUS_DAMAGE` for major chip and the disjoint
  `0x10000 | volatileBit` domain on CONFUSED `DAMAGE_DEALT` and Leech/Trap
  `STATUS_DAMAGE`. `VOLATILE_APPLIED.m_iTag` is `sourceSide+1` for Leech
  Seed (zero for every other volatile), avoiding PLAYER's numeric-zero ambiguity.
- **Switch/forced-switch contract:** `ZM_MoveResult` carries a requested forced
  side/slot from the executor to the engine. A primary FORCE_SWITCH chooses the
  lowest eligible non-active living slot or emits `MOVE_FAILED(NO_SWITCH_TARGET)`;
  a damaging secondary applies only after damage, never on KO, and silently skips
  both proc draw and switch when no eligible slot exists. Engine-owned
  `ZM_BattleEngine::DoSwitch` validates side/destination, silently clears the
  outgoing monster's volatiles/counters/charge+lock slots/leech source, all stages,
  crit stage, and Endure, while preserving HP, PP, major status, and its major
  counter; it then changes the active slot and emits `SWITCH_IN`. If the first
  mover force-switches the side queued second, that old monster's queued action is
  skipped. SC6 voluntary switching reuses this primitive; it does not duplicate
  reset semantics.
- **Why:** the ten effects share ordering, duration, event, and switch-reset
  invariants, so implementing them as isolated move arms would drift deterministic
  replay and leak state across switches. Centralized lifecycle ownership plus an
  engine-owned switch primitive keeps every draw conditional, makes event streams
  presentation-complete, and gives SC6 one safe attachment point for voluntary
  switching and Trap restrictions.
- **Tests that lock it:** 52 SC5 cases in `ZM_Tests_Battle.cpp` cover default
  state/event encodings and tag domains; every apply/block/duration/draw path;
  confusion self-hit/pass/burn/faint; flinch timing; Leech/Trap EOT + exact stream;
  Taunt/Protect/Endure; charge/semi/recharge/lock; Swagger; DoSwitch reset;
  primary/damaging forced switch + queued-action skip; fixed side/EOT order; full
  G/M precedence; and `VolatileFree_SC5AddsNoEventsDrawsOrState`. The box-1 exact
  stream remains unchanged. Verified gate: regen check green,
  `Vulkan_vs2022_Debug_Win64_True` build green, **1379 ran / 1378 passed / 0 failed
  / 1 skipped**, and headless automated suite **1/1**.
- **Reversibility:** moderate. The implementation is localized to
  `Source/Battle/`, but the bit values, event encodings/tags, duration semantics,
  guarded draw order, EOT order, and switch-reset behavior now define deterministic
  replay goldens and downstream presentation. Change them only with a new decision
  plus updated oracle/event-stream tests.

## 2026-07-11 -- ZM-D-033 -- S2 box-2 execution plan: 6 ordered sub-commits + the LOCKED augmented RNG draw order (PHASE G/M/E)

- **Decision:** box 2 (ZM_MoveExecutor + full DamageCalc/CatchCalc/StatusLogic,
  the bulk of S2's ~370 tests) is decomposed into **6 independently-shippable
  sub-commits**, executed in order, each keeping the box-1 goldens BYTE-IDENTICAL
  (every new gate/draw is conditional on state box 1 never sets -- status-free,
  stage-0, volatile-mask 0 -- and every new `ZM_BATTLE_EVENT` kind is append-only).
  Synthesized from a 2-planner design workflow; full plan archived this session in
  scratchpad `S2_Box2_Plan.md` (ephemeral -- the load-bearing contract is captured
  here). Sub-commit order (each builds + passes + commits alone):
  - **SC1** Executor seam (PURE refactor): move `ExecuteMove`'s body into
    `ZM_MoveExecutor::Execute` / `ApplyDamagingHit` over a `ZM_MoveContext`; switch
    has only `NONE` + `default`; box-1 27-event golden byte-identical. New files
    `Source/Battle/ZM_MoveExecutor.{h,cpp}`. No new events.
  - **SC2** Stat-stage effects (all LOWER_*/RAISE_* + accuracy/evasion) + STATUS-
    category vs damaging-secondary dispatch. Lights `STAT_STAGE_CHANGED`,
    `MOVE_FAILED`.
  - **SC3** Delivery variants (MULTI_HIT/DOUBLE_HIT/RECOIL/DRAIN/HEAL_HALF/
    FIXED_LEVEL/HALVE_HP/OHKO) + field/screen/hazard SETTERS (state-only) + screen
    damage reduction (`bScreen`). Lights `MULTI_HIT/RECOIL/DRAIN/HEAL`.
  - **SC4** `ZM_StatusLogic` majors (6) + burn damage reduction (`bBurnedPhysical`)
    + paralysis speed x1/4. New files `ZM_StatusLogic.{h,cpp}`. Appends `m_iTag`
    to `ZM_BattleEvent` (Q1). Lights `STATUS_APPLIED/DAMAGE/CURED`.
  - **SC5** Volatiles (all 10) + SWAGGER + FORCE_SWITCH + a `DoSwitch` primitive.
    Lights `FLINCH/VOLATILE_APPLIED/VOLATILE_ENDED`.
  - **SC6** Pre-move actions (SWITCH/ITEM/RUN in `ResolvePreMovePhase`) +
    `ZM_CatchCalc` (Gen-III/IV 4-shake) + promote the smoke to the **2000-battle
    fuzz soak**. New files `ZM_CatchCalc.{h,cpp}`. Lights `CATCH_SHAKE/CATCH_RESULT/
    FLEE/FLEE_FAILED`.
- **LOCKED augmented RNG draw order (the master contract -- every box-2 golden is
  defined by it; the offline oracle mirrors it exactly).** Attacker fully resolves
  before defender; the pre-phase `RandBelow(2)` speed tie-break (only on exact
  effective-speed tie) is unchanged. Draws are pulled ONLY when the guard holds, so
  box-1 (status-free, stage-0) pulls EXACTLY box-1's draws:
  - **PHASE G (pre-move gates, BEFORE PP/MOVE_USED; first trigger cancels, spending
    NO PP + emitting NO MOVE_USED -- Q3):** G1 RECHARGE (0 draws) -> G2 FREEZE
    (`RandBelow(100)<20` thaw) -> G3 SLEEP (counter, 0 draws) -> G4 FLINCH (0 draws)
    -> G5 CONFUSE (counter; `RandBelow(100)<33` -> self-hit = 40-power typeless
    PHYSICAL drawing its own `RandRange(85,100)`) -> G6 PARALYSIS (`RandBelow(100)<25`
    full-para).
  - **PHASE M:** M0 TAUNT-blocks-STATUS-category -> M1 spend PP + emit `MOVE_USED`
    -> M2 PROTECT intercept -> M3 SEMI_INVULN-target-vanished -> M4 two-turn
    charge/semi-invuln turn-1 set -> M5 ACCURACY (only if the move can miss; effAcc
    via acc/eva stages) -> M6 IMMUNITY (damaging/fixed/ohko only).
  - **PHASE E:** single-hit damaging = crit `RandBelow(24)` (skip if critStage>=2)
    -> roll `RandRange(85,100)` -> `ApplyDamagingHit` (sets `bBurnedPhysical`/
    `bScreen` at the call site) -> recoil/drain self-effect appended AFTER -> E3
    secondary proc `RandBelow(100)<chance` (chance>=100 no draw); apply-time
    duration draws SLEEP `RandRange(1,3)`, CONFUSE `RandRange(1,4)`, TRAP
    `RandRange(4,5)`, TOXIC counter=1. MULTI_HIT = hit-count `RandBelow(8)` ->
    {2,2,2,3,3,3,4,5} then per-hit (crit,roll). DOUBLE_HIT fixed 2. FIXED_LEVEL/
    HALVE_HP/OHKO no crit/roll. STATUS-category primary no crit/roll.
- **Weather line (box-2 vs box-3 rule):** box 2 SETS weather/screen/hazard field
  state + applies the ability-independent burn (`bBurnedPhysical`) and screen
  (`bScreen`, crit bypasses) damage reductions, but emits NO weather/screen event
  and does NOT count down. Box 3 owns the weather DAMAGE multiplier
  (`uWeatherNum/Den`), the end-of-turn weather chip, the weather/screen countdown +
  expiry, and ALL of `WEATHER_CHANGED/WEATHER_DAMAGE/SCREEN_SET/SCREEN_EXPIRED/
  ABILITY_TRIGGER`. End-of-turn global order reserves the weather-chip slot FIRST:
  `[weather chip (box 3, absent in box-2 tests) -> per side player-then-enemy in
  speed order: major chip -> leech-seed -> trap chip -> volatile-counter expiries
  -> TURN_END]`. Box-2 full-stream goldens use battles with NO weather set.
- **Ratified open questions + constants (internal engine choices within locked
  scope, like box-1's rounding/crit sub-decisions):**
  1. **Q1:** append `int m_iTag = 0` to `ZM_BattleEvent` (+ a trailing defaulted
     `ZM_MakeEvent` param) as the `STATUS_DAMAGE` source-status discriminator
     (ZM-D-009 append pattern; box-1 goldens stay equal). Lands in SC4.
  2. **Q2:** catch base rate derived from `ZM_RARITY` (COMMON 190 / UNCOMMON 120 /
     RARE 45 / LEGENDARY 3) via `ZM_BaseCatchRate` (NO new S1 data column);
     Gen-IV status bonuses (sleep/freeze 2.5x, para/poison/toxic/burn 1.5x).
  3. **Q3:** an action-time gate cancel spends NO PP and emits NO `MOVE_USED`.
  - **Constants:** percentage gates `RandBelow(100)<t` (thaw 20, full-para 25,
    confusion self-hit 33); multi-hit `RandBelow(8)`->{2,2,2,3,3,3,4,5}; durations
    sleep 1-3 (REST=2), confuse 1-4, trap 4-5, taunt 3, lock-in 2-3; chips burn/
    poison/leech/trap 1/8, toxic n/16; paralysis speed x1/4; crit unchanged 1/24 x1.5.
- **Why:** every box-2 golden event stream is defined by the exact draw order;
  locking it ONCE here prevents drift across the ~275 box-2 tests and lets each
  sub-commit's Python oracle reproduce streams independently before the engine is
  trusted. The ordering ships each dependency before its consumer (statuses before
  the effects that inflict them; burn WITH its damage-halving; pre-move actions +
  the soak last, once the full effect surface exists so the soak terminates).
- **Tests that lock it:** `ZM_Tests_Battle.cpp` box-2 additions per sub-commit +
  the box-1 golden re-run (`Scenario_NibbinVsStrayling_ExactStream`), which MUST
  stay byte-identical through every sub-commit; the 2000-battle soak at SC6.
- **Reversibility:** moderate. The draw order + the `m_iTag` append define all
  box-2 goldens (expensive to change); sub-commit boundaries are independent, so a
  later sub-commit can be re-planned without disturbing landed ones.

## 2026-07-10 -- ZM-D-032 -- S2 battle-engine keystone architecture (box 1): ZM_BattleState / ZM_BattleEngine / flat ZM_BattleEvent stream / ZM_BattleMonster

- **Decision:** the S2 battle engine is built as box 1 of ~6, establishing the
  type architecture the other five boxes extend without reshaping. Chosen shape
  (synthesized from a 3-architect design panel, spec archived in the session
  scratchpad `S2_Box1_Design.md`):
  - **`ZM_BattleMonster`** = the mutable in-battle instance (deep-owned BY VALUE
    inside the battle; the bare name `ZM_Monster` is reserved for a future
    persistent party type). Built by `ZM_BuildBattleMonster(ZM_BattleMonsterSpec)`;
    the spec carries IV/EV/nature/moves/ability **plus an optional base-stat
    override** so goldens are pencil-verifiable and survive the ZM-D-021 base-stat
    re-tune.
  - **`ZM_BattleState`** = `m_axSides[2]` (each a `ZM_BattleSide` with a
    `Zenith_Vector<ZM_BattleMonster>` party + active slot + reserved
    screens/hazards) + `ZM_FieldState` (weather reserved, turn counter) + the
    **`ZM_BattleRNG` living IN state** (so snapshot/clone/replay is one object).
    The append-only event stream lives in the **engine** (output, not state).
  - **`ZM_BattleEvent`** = a flat 7-scalar-field POD (`kind,side,slot,moveId,
    speciesId,iAmount,iAux`) with a defaulted `operator==`, built through a shared
    `ZM_MakeEvent` factory used by both the engine AND the golden vectors. Later
    boxes only APPEND event kinds and APPEND fields (default 0), so box-1 golden
    streams never shift (ZM-D-009 append-only discipline). Effectiveness is
    emitted as SEPARATE `SUPER_EFFECTIVE`/`NOT_EFFECTIVE`/`IMMUNE` events; neutral
    is silent.
  - **`ZM_BattleEngine`** = `Begin(config, playerSpecs,n, enemySpecs,n, seed, seq)`
    -> `SubmitAction(side, action)` -> `ResolveTurn()`; `IsOver()`/`GetWinnerSide()`
    /`GetEvents()`/`GetState()`. Box 1 executes MOVE actions only; SWITCH/ITEM/RUN,
    the ~60-effect `ZM_MoveExecutor`, statuses, abilities, weather, exp/AI/tower are
    later-box seams left as empty phase hooks (`ResolvePreMovePhase`,
    `ExecuteMove`, `ResolveEndOfTurnPhase`).
  - **Box 1 ships a REAL minimal Gen-V `ZM_CalcDamage`** (pure function, RNG drawn
    by the engine and passed in) so box 1 is end-to-end testable to a faint; the
    full effect executor stays in box 2.
- **Ratified sub-decisions (the design panel's 3 open questions, all internal
  engine choices within locked scope -- no user decision required):**
  1. **Rounding = pure integer floor** at every `x3/2` and `xpercent` step (matches
     the S1 `ZM_StatCalc` idiom; simplest to mirror in the offline Python oracle).
     NOT Gen-V round-half-down. Locked before the first scenario golden is committed.
  2. **Crit = 1/24 rate, x1.5 multiplier** -- honoring the GDD 7.1 explicit numbers
     over canonical Gen-V (x2, level-table rate). The GDD wins on conflict.
  3. **Draw discipline:** no accuracy draw when effective accuracy >= 100 (sure-hit
     short-circuit); no crit draw at critStage >= 2; per-move draw order
     `accuracy(if it can miss) -> crit RandBelow(24) -> roll RandRange(85,100)`,
     attacker-then-defender, one `RandBelow(2)` tie-break only on exact
     effective-speed ties. Modifier order `base -> weather -> crit -> random ->
     STAB -> type -> burn -> screen`.
- **Why:** every one of S2's ~370 later unit tests asserts against the
  `ZM_BattleEvent` stream, so the event shape + the deterministic draw order are
  the load-bearing contract -- getting them wrong churns the whole suite. A flat
  append-only POD + a shared factory makes later boxes additive; deep-owned
  monsters make a battle a pure function of `(specs, config, seed)` (bit-exact
  replay, clone-for-AI lookahead, save-mid-battle). Verified against the shipped S1
  data layer before authoring: Nibbin + Strayling are both mono-NORMAL, Rambash is
  NORMAL/PHYSICAL power 45 / acc 100, `ZM_NATURE_FERAL`/`ZM_ITEM_NONE`/
  `ZM_ABILITY_NONE` all exist, `ZM_TypeChart::GetDualTypeEffectiveness` returns the
  exact float product.
- **Tests that lock it:** `ZM_Tests_Battle.cpp` (category `ZM_Battle`) --
  `Scenario_NibbinVsStrayling_ExactStream` (the characterization bedrock: full
  event stream vs an offline PCG32+stat+damage Python oracle validated against the
  S1 golden vectors), `Damage_GenVGoldenVectors`, `Effectiveness_PercentMapping`,
  `TurnOrder_SpeedPriorityTie`, `Engine_BeginEmitsHeader`, and
  `Fuzz_Smoke_50Battles_Invariants` (scaffold for the S2 2000-battle soak).
- **Reversibility:** moderate. The event-field set and draw order are the
  expensive-to-change parts (they define every golden); the type/file layout and
  the minimal damage path are localised to `Source/Battle/`. Reserved event kinds
  and the `x3/2` seam fields mean box 2-6 attach without touching box-1 goldens.

## 2026-07-10 -- ZM-D-031 -- Master-only workflow: all work committed DIRECTLY to master; no branches, no PRs, no worktrees

- **Decision (user-directed):** all Zenithmon work is committed DIRECTLY to the
  `master` branch and pushed with `git push origin master`. Feature branches,
  pull requests, and git worktrees are no longer created -- `git checkout -b`,
  `gh pr create`, and worktree creation are forbidden for this project. This
  supersedes the branch + PR + auto-merge flow of ZM-D-028 (which itself
  superseded "wait for all checks green"). The authoritative gate is the LOCAL
  verification run before every push (`zenith build` + the boot unit gate
  `run_unit_gate.ps1` at the exact baseline + `zenith test --headless`); the
  `zm-tests` CI workflow still runs post-push on master (`push: [master]`
  trigger) as a BACKSTOP only. On a red post-push run, fix forward with another
  direct commit to master (never revert shipped history, force-push master, or
  `gh run rerun`).
- **Why:** the user's explicit direction. It removes all PR/branch/merge
  latency and the stacked-PR conflict hazard (squash-merging a parent orphaned
  its stacked child at the top of the append-only DecisionLog -- see the #156/#157
  conflict resolution 2026-07-10). Direct-to-master is viable here because the
  repo owner's credential can push to master (branch protection was created with
  `enforce_admins=false` precisely to preserve the owner's direct-push workflow,
  ZM-D-016), and the local gate is a full build + the same unit + headless suites
  CI would run. Trade-off accepted by the user: no pre-merge CI gate and no PR
  review; the local gate is the sole pre-push authority, so it MUST be run green
  before every push.
- **Tests that lock it:** none executable; the contract is the rewritten
  StartPrompts.md (prompt 0 WORKING MODEL + ITERATION PROTOCOL + COMMIT+PUSH,
  prompts 1-4, Notes), the updated AgentBriefing.md workflow sections, the
  Status.md working-model note, and this entry. The local-gate discipline is
  enforced per-commit by the build + unit-gate + headless runs.
- **Reversibility:** trivial to revert the policy (re-enable branches/PRs by
  editing StartPrompts.md + AgentBriefing.md), but commits already landed on
  master stay on master.

## 2026-07-10 -- ZM-D-030 -- ZM_DataRegistry (name->ID lookups + cross-table enforcer) closes S1

- **Decision:** `ZM_DataRegistry` (`Source/Data/ZM_DataRegistry.{h,cpp}`) adds
  reverse name->ID lookups for every S1 table -- `ZM_FindSpeciesByName` /
  `...Move` / `...Item` / `...Ability` / `...Nature` / `...Scene` -- each an exact
  case-sensitive scan returning the id or that table's NONE sentinel (NONE for
  null/empty too). Linear scan (tables are <= ~220 rows; a hash index is a
  trivial later swap behind the same signatures). This is the last S1 box; the
  accompanying `ZM_Tests_DataRegistry` suite is the cross-table schema enforcer.
- **Why:** the reverse lookups are needed by save/load (names <-> ids across
  versions), WorldSpec/scene authoring, and debug/console tooling; and S1 needs a
  single place that asserts the tables are MUTUALLY consistent (per-table suites
  each check their own table, but nothing checked the references BETWEEN tables).
- **Tests that lock it:** `Tests/ZM_Tests_DataRegistry.cpp` (category `ZM_Data`, 9
  cases) -- name round-trip for all six tables (`Find(GetName(id)) == id`),
  unknown/null/empty -> NONE, and the cross-table resolves: every species
  evolution target, every TM's taught move, every WorldSpec encounter species, and
  every derived learnset move resolve to real rows. Boot suite 1172 ran / 0 failed;
  baseline bumped 1163 -> 1172. **S1 gate MET** (102 `ZM_*` unit tests vs the ~90
  target; no visual check for S1).
- **Reversibility:** easy -- additive `Source/Data/` files; swapping the linear
  scans for a hash index is behind the stable Find signatures.

## 2026-07-10 -- ZM-D-029 -- ZM_WorldSpec ships as SCHEMA + an 8-scene proving set; the full world is appended at S9/S10

- **Decision:** the keystone world table (ZM-D-005) lands its SCHEMA plus a small
  proving set, not the full world. `ZM_WorldSpec` (`Source/Data/ZM_WorldSpec.{h,
  cpp}`): one row per scene -- id / name / build index / `ZM_SCENE_KIND` (9 kinds:
  frontend/town/route/interior/gym/battle/tower/league/victory_road) / terrain set
  / warp connections (`ZM_SceneConnection` = target scene + spawn tag) / offered
  spawn tags / encounter table (`ZM_EncounterSlot` = species + level band +
  weight). Per-scene connection/tag/encounter arrays are static, referenced by
  pointer + count. The 8 proving scenes (FrontEnd 0, Battle 1, Dawnmere, Route 1,
  Thornacre, Player's Home, Aster's Lab, Gym 1) exercise every column. Accessors:
  `ZM_GetWorldSpec` / `ZM_GetSceneName` / `ZM_FindSceneByBuildIndex` /
  `ZM_SceneKindToString`. The full ~40-scene world is authored at S9/S10 by
  APPENDING rows (`ZM_SCENE_ID` is save-stable, append-only).
- **Why:** everything from S3 on (warps, encounters, gating, terrain authoring)
  flows through this table, so the schema + a referential-integrity test suite
  must exist first -- it is the enforcer that keeps ~40 scenes honest before any
  are baked. Shipping only the schema + proving set keeps S1 headless-data-only
  while locking the structure S3+ builds against.
- **Tests that lock it:** `Tests/ZM_Tests_WorldSpec.cpp` (category `ZM_Data`, 11
  cases) -- index self-consistency, unique names, unique build indices anchored
  (FrontEnd 0 / Battle 1), valid kinds, terrain-by-kind (outdoor has terrain,
  indoor does not), spawn tags non-empty + unique per scene, **every connection
  resolves to a real target + a spawn tag that target offers**, encounters
  route-only with real species + valid level bands + positive weight, **every
  non-Battle scene reachable from FrontEnd**, build-index round-trip, accessor +
  ToString. The graph was pre-validated offline before building. Boot suite 1163
  ran / 0 failed; baseline bumped 1152 -> 1163.
- **Reversibility:** easy -- additive `Source/Data/` files; the world grows by
  appending rows. Build-index assignments are cheap to change until S3 warps start
  referencing them through this table.

## 2026-07-10 -- ZM-D-028 -- Loop policy: local gate is the quality bar; auto-merge on zm-tests green; do NOT wait on / idle-watch CI

- **Decision (user-directed):** the autonomous loop must not sit blocking on CI.
  The authoritative verification of new behaviour is the LOCAL gate run before
  pushing -- `zenith build Zenithmon` + the boot unit gate
  (`Tools/run_unit_gate.ps1 -Exe <exe> -Baseline N`, which runs the ZM_* unit
  tests that `zenith test` skips) + `zenith test Zenithmon --headless`. After
  opening the PR the loop enables GitHub auto-merge
  (`zenith_gh.ps1 pr merge <n> --auto --squash --delete-branch`), which lands the
  PR unattended the moment the sole required check `zm-tests` passes; it does NOT
  idle-watch checks, wait for the slower discipline gates (dp-tests/cb-tests/
  engine-gate/...), or launch a blocking `pr checks --watch`. The CI window is
  spent designing/prototyping the next task instead.
- **Why:** the full CI suite is ~25-30 min (dp-tests is the long pole) and was
  re-confirming behaviour the local gate already proves -- pure latency. `zm-tests`
  (~11 min) is the only machine-required check, so auto-merge on it is the fastest
  compliant path. `--admin`/bypass merges are BLOCKED by the harness permission
  classifier (verified 2026-07-10), so "nothing merges red" still holds -- zm-tests
  must actually go green; auto-merge just removes the human/agent wait.
- **Tests that lock it:** none executable; the contract is the updated
  StartPrompts.md (prompt 0 EXECUTION/PR+MERGE, prompts 1/2, Notes) + this entry.
  The local-gate discipline is enforced by the per-PR unit-gate + headless runs.
- **Reversibility:** trivial -- edit StartPrompts.md. Note this supersedes the
  prior "watch checks ... merge once ALL checks are green" wording in prompt 0
  (which implied waiting for the full suite).

## 2026-07-10 -- ZM-D-027 -- ZM_StatCalc (Gen-III+ integer formulas) + ZM_BattleRNG (PCG32) are the sanctioned math + randomness

- **Decision:** two pure-logic modules close most of the S1 formula surface.
  `ZM_StatCalc` (`Source/Data/ZM_StatCalc.{h,cpp}`) is the Gen-III+ integer stat
  formula -- `HP = ((2*base+IV+EV/4)*level)/100 + level + 10`; the other five =
  `(((2*base+IV+EV/4)*level)/100 + 5) * naturePercent/100`, truncating divisions,
  nature multiplier applied last via `ZM_GetNatureStatPercent` (ZM-D-025). No
  floating point. `ZM_BattleRNG` (`Source/Data/ZM_BattleRNG.h`, header-only) is a
  PCG32 generator (64-bit state, 32-bit output): `Next` / unbiased `RandBelow` /
  `RandRange` / `Chance` / `ChancePercent`, deterministic from a seed. It is the
  ONLY sanctioned randomness in game logic (never rand()/std::random).
- **Why:** the battle engine (S2) is seeded and must replay bit-for-bit, so both
  the stat math and the RNG must be integer-exact and reproducible. Landing them
  in S1 (with golden vectors) gives S2 a locked, tested foundation. PCG32 is small,
  fast, statistically strong, and trivially reproducible -- the standard choice for
  a deterministic game RNG. Default-constructed RNGs are fixed-seeded so an
  unseeded instance is never a hidden nondeterminism source.
- **Tests that lock it:** `Tests/ZM_Tests_StatCalc.cpp` (4) -- HP + other-stat
  golden vectors across level/IV/EV/nature, nature dispatch (HP nature-independent;
  raise/lower/unaffected stats), monotonicity. `Tests/ZM_Tests_BattleRNG.cpp` (6)
  -- **a golden 8-value stream pinning the exact PCG32 algorithm**, same-seed
  determinism, distinct-seed divergence, RandBelow/RandRange bounds, and the
  Chance/ChancePercent contract + ~50% frequency. All expected values were
  precomputed in a scratchpad model and matched on the first build. Boot suite
  1152 ran / 0 failed; baseline bumped 1142 -> 1152.
- **Reversibility:** easy -- additive `Source/Data/` files, no other module
  depends on them yet. The PCG32 constants + seeding sequence are load-bearing
  (the golden stream pins them); changing them is a deliberate golden-vector edit.

## 2026-07-10 -- ZM-D-026 -- ZM_AbilityData ships roster + metadata + a declared HOOK-SURFACE bitmask; fn-pointer hook bodies are S2

- **Decision:** the ~50-ability Roadmap sub-box lands as a compiled `const
  ZM_AbilityData` table (50 rows: id / name / description / `m_uHookMask`) plus an
  `ZM_ABILITY_HOOK` enum of 11 hook points as bit flags (SWITCH_IN / MODIFY_STAT /
  MODIFY_DAMAGE_DEALT / MODIFY_DAMAGE_TAKEN / STATUS_TRY / CONTACT / TURN_END /
  FAINT / ACCURACY / WEATHER / TYPE_IMMUNITY). Each ability declares WHICH hooks it
  will implement via the bitmask; the actual per-hook fn-pointer struct + bodies
  are deferred to S2. `ZM_AbilityHasHook(id, hook)` queries the surface.
- **Why:** the plan calls abilities "fn-pointer hook structs", but the hook
  signatures need the battle-state types (`ZM_BattleState`/`ZM_BattleEvent`) that
  do not exist until S2 -- wiring speculative signatures now would only churn
  (repo mandate: no legacy/compat). The bitmask is the non-speculative S1 slice:
  it fixes the roster + names + descriptions + each ability's hook surface (what
  the S2 executor must wire), is fully testable today, and references no
  not-yet-existing types. Mirrors the "data now, executor later" pattern used for
  moves (ZM-D-022) and items (ZM-D-024).
- **Tests that lock it:** `Tests/ZM_Tests_Abilities.cpp` (category `ZM_Data`, 6
  cases) -- index self-consistency (count == 50), unique names, non-empty
  descriptions, masks non-zero with no stray bits, **every hook bit used by >= 1
  ability**, and `ZM_AbilityHasHook` agreeing with the raw mask + name accessor.
  Boot suite 1142 ran / 0 failed.
- **Reversibility:** easy -- additive `Source/Data/` files; `ZM_ABILITY_ID` order
  is append-only. S2 grows the row with the fn-pointer struct (or a parallel hook
  table) keyed by id; the mask stays as the coverage/declaration record.

## 2026-07-10 -- ZM-D-025 -- ZM_NatureData is the exact 25-nature 5x5 grid (real table, not derived)

- **Decision:** the 25 natures land as a compiled `const ZM_NatureData` table
  (id / name / raised stat / lowered stat) that is exactly the 5x5 grid of
  (raised, lowered) pairs over the five non-HP stats (ATTACK / DEFENSE / SPATTACK /
  SPDEFENSE / SPEED); the five diagonal entries (raised == lowered) are the neutral
  natures. `ZM_GetNatureStatPercent(nature, stat)` returns the integer multiplier
  110 / 90 / 100 that `ZM_StatCalc` applies as `(stat * percent) / 100`.
- **Why:** natures are a small, exact, closed set (unlike the derived base-stat /
  learnset placeholders) -- 25 rows, one per stat pairing -- so a real hand-authored
  table is correct and final, not a placeholder. The percent helper keeps the
  x11/10 and x9/10 nature maths integer-exact and in one place for the S1 stat
  formula (box 6).
- **Tests that lock it:** `Tests/ZM_Tests_Natures.cpp` (category `ZM_Data`, 6
  cases) -- index self-consistency, unique names, raised/lowered always non-HP,
  **every (raised, lowered) pair present exactly once + exactly 5 neutral**, the
  110/90/100 percent contract (incl. HP always 100), and the name accessor.
- **Reversibility:** trivial -- additive `Source/Data/` files; names are flavour,
  the pairing is fixed by the mechanic.

## 2026-07-10 -- ZM-D-024 -- ZM_ItemData ships as data + schema only (90 items over a 34-kind effect enum); the bag/battle logic is S2/S5

- **Decision:** the ~80-item Roadmap box lands as a compiled `const ZM_ItemData`
  table (90 items) plus its schema -- `ZM_ITEM_ID` (90, save-stable), a 9-value
  `ZM_ITEM_CATEGORY` (ball / medicine / battle / held / berry / evo / TM / key /
  field), and a 34-kind `ZM_ITEM_EFFECT` executor tag. Each row carries category,
  buy + sell price, effect kind + a kind-specific param, a consumable flag, and
  (for TMs) a taught `ZM_MOVE_ID`. Rows are INERT: the bag/use/held-hook/catch
  logic that interprets `ZM_ITEM_EFFECT` is S2 (held items, catch math) / S5 (bag
  UI), mirroring the MoveData boundary (ZM-D-022). The 25 TMs each reference a
  real move; **this is the TM/tutor learnset seam** the learnset box deferred
  (ZM-D-023). Original names; no Nintendo IP.
- **Why:** items are battle-core scope (Scope.md); catching (S2/S5), the mart
  (S6), held items in battle (S2), and TM teaching all reference this table, so it
  must exist before them. Splitting data from the ~34-arm item executor keeps this
  a reviewable data drop. The effect enum is sized so each future per-effect
  handler has a data subject (a tested coverage invariant).
- **Tests that lock it:** `Tests/ZM_Tests_Items.cpp` (category `ZM_Data`, 11
  cases) -- index self-consistency (count == 90), unique names, valid
  category/effect enums, price sanity (sell <= buy) + key-item contract
  (priceless + effectless), consumable-flag-matches-category, TM-teaches-a-real-
  move (and only TMs teach), ball-only CATCH with multiplier >= 1.0x, stat/type
  param ranges, every effect kind used, every category populated, accessor +
  ToString contracts. The roster was validated offline before building. Boot suite
  1130 ran / 0 failed; zm-tests baseline bumped 1119 -> 1130.
- **Reversibility:** easy -- additive `Source/Data/` files; `ZM_ITEM_ID` order is
  append-only (save-stable). Per-item tuning (prices, effect params) is
  git-history, not decisions.

## 2026-07-10 -- ZM-D-023 -- Species learnsets are systematically DERIVED (placeholder), completing the ZM_SpeciesData box

- **Decision:** per-species level-up learnsets are computed by
  `ZM_GetSpeciesLearnset(id)` (`Source/Data/ZM_Learnsets.{h,cpp}`), not stored:
  it partitions the move table into the species' STAB / secondary-type / universal
  NORMAL damaging buckets + a shared status bucket, sorts the damaging buckets by
  effective power (fixed-damage moves read as high power so they land late),
  teaches a same-type damaging move at level 1, then round-robins damaging moves
  with a capped minority of status moves, spreading levels 1..~50 and sizing the
  list by evolution stage (12 / 14 / 16; single-stage finals 16). Returned by
  value as a fixed-capacity `ZM_Learnset` (max 16). This ticks the `ZM_SpeciesData`
  Roadmap box `[x]` (roster ZM-D-020 + base stats ZM-D-021 + learnsets here).
- **Why:** identical reasoning to base stats (ZM-D-021) -- real movepools are an
  S11 balance concern, and hand-authoring ~150 arbitrary placeholder movepools in
  one commit buys nothing over a deterministic, type-appropriate,
  referentially-valid derivation that unblocks S2 (which builds a monster's
  moveset from species + level). The accessor signature is the stable seam for a
  stored table later. TM/tutor compatibility is deferred to `ZM_ItemData`.
- **Tests that lock it:** `Tests/ZM_Tests_Learnsets.cpp` (category `ZM_Data`, 8
  cases) -- count bounded [4,16] + every entry a real move, level-ordered +
  in-range + something learnable by L5, type-appropriate (own type(s) or NORMAL),
  has a STAB move + an early damaging move, no duplicate moves, status a minority,
  deterministic, and learnset size non-decreasing along an evolution chain. The
  derivation was pre-validated across all 152 species offline before building.
  Boot suite 1119 ran / 0 failed; zm-tests baseline bumped 1111 -> 1119.
- **Reversibility:** easy -- replace the accessor body with a stored per-species
  table; no caller sees a difference. Additive `Source/Data/` files.

## 2026-07-10 -- ZM-D-022 -- ZM_MoveData ships as data + schema only (218 moves over a 57-kind effect enum); the executor is S2

- **Decision:** the ~220-move Roadmap box lands as a compiled `const ZM_MoveData`
  table (218 rows) plus its schema -- `ZM_MOVE_ID` (218 + save-stable
  `ZM_MOVE_COUNT`/`ZM_MOVE_NONE`), `ZM_MOVE_CATEGORY` (physical/special/status),
  `ZM_MOVE_TARGET` (opponent/self/field), and `ZM_MOVE_EFFECT` (57 executor tags).
  Each row carries type, category, power, accuracy, PP, priority, crit stage,
  contact, effect kind + proc chance + a kind-specific magnitude, and target. The
  rows are INERT: no behaviour, no damage/status pipeline -- the single
  `ZM_MoveExecutor` switch that interprets `ZM_MOVE_EFFECT` is deferred to S2
  (ZM-D-010). Original names throughout; the GDD-7.2 cuts (Substitute / Encore /
  Transform / weight moves) have no enum value.
- **Why:** moves are the dependency the species learnsets (the remaining
  `ZM_SpeciesData` sub-box) and the whole S2 battle engine reference, so the table
  must exist before either. Splitting data from the executor mirrors the
  battle-engine boundary already set in ZM-D-010 and keeps this PR a reviewable
  data drop rather than data + a 57-arm interpreter. The effect enum is sized so
  every S2 per-effect scenario (TestPlan 5.2) has a data subject; a tested
  coverage invariant guarantees no effect kind is dead.
- **Tests that lock it:** `Tests/ZM_Tests_Moves.cpp` (category `ZM_Data`, 16
  cases) -- index self-consistency (row i.m_eId == i, count == 218), unique
  non-empty names, valid type/category/target/effect enums, power<->category rule
  (status + fixed-damage powerless; else power in [10,250]), accuracy in [0,100]
  with own-side moves always-hit, PP/priority/crit ranges, effect-chance
  bi-conditional (chance 0 iff effect NONE) + status-moves-always-act, target
  derivable from category+effect, stat-magnitude [1,3], **every effect kind used
  >= 1**, every type has a move, category spread, priority/crit presence,
  accessor + ToString contracts. Boot suite 1111 ran / 0 failed; zm-tests baseline
  bumped 1095 -> 1111 in the same PR.
- **Reversibility:** easy -- additive `Source/Data/` files; the `ZM_MOVE_ID` order
  is append-only (save-stable). Per-move tuning values are git-history, not
  decisions; the struct may gain fields (e.g. contact already present) as S2
  needs them.

## 2026-07-10 -- ZM-D-021 -- Species base stats are systematically DERIVED (placeholder), not hand-tuned

- **Decision:** `ZM_GetSpeciesBaseStats(id)` computes the six base stats from a
  per-archetype stat profile (8 body-plan rows summing ~300) scaled by an
  evolution-stage factor (single-stage finals read as fully evolved) and a rarity
  factor, then a deterministic per-family emphasis/dock drawn from the family seed
  (bump one stat, dock another). Stats are NOT stored per-row and are NOT
  balance-tuned. Added the `ZM_STAT` enum + `ZM_BaseStats` struct.
- **Why:** hand-authoring 152 x 6 balanced stats in one commit is huge and
  error-prone, and balance is explicitly an S11 concern (headless AI-vs-AI). A
  systematic, deterministic derivation unblocks S2 (scripted battles need fixed
  base stats) and differentiates species by archetype AND family. It is trivially
  superseded by a stored per-species table in a later balance pass -- the accessor
  signature is the stable seam.
- **Tests that lock it:** `Tests/ZM_Tests_Species.cpp` `BaseStats_*` (5) --
  in-range [1,255]; totals banded by evolution role (stage-1 base 250-360,
  single-stage >=480, legendary >=560, global 250-700); every stat non-decreasing
  + BST strictly increasing along an evolution chain; archetype shapes (AVIAN
  faster than BLOB, BLOB bulkier than AVIAN, BIPED hits harder than FLOATER);
  family variety (>=60 distinct stat blocks). Boot suite 1095 ran / 0 failed.
- **Reversibility:** easy -- replace the accessor body with a stored table; no
  caller sees a difference.

## 2026-07-10 -- ZM-D-020 -- ZM_SpeciesData decomposed: structural roster first (152 species), base stats + learnsets deferred

- **Decision:** the ~150-species SpeciesData Roadmap box is split across
  increments on the same box. Increment 1 (this PR): the `ZM_SpeciesData` schema
  + supporting enums (`ZM_ARCHETYPE`/`ZM_RARITY`/`ZM_SIZE_CLASS`/`ZM_SPECIES_ID`)
  + the full 152-species STRUCTURAL roster (id / name / type(s) / archetype /
  evo-stage / evolves-to / family / rarity) transcribed from GDD section 5, with
  size class + family seed as rule-derived accessors. DEFERRED to later
  increments on this box: per-species base stats (a design pass) and learnsets
  (need `ZM_MoveData`, box 3). The Roadmap box stays a WIP (`[~]`).
- **Why:** 152 species with hand-designed base stats + every field in one commit
  is a large, error-prone, hard-to-review PR; a structural-roster-first split is
  standard practice for big data tables and is dependency-correct (learnsets
  reference moves that do not exist yet). The roster is the foundation that
  MoveData learnsets, WorldSpec encounters, DataRegistry, and S4 asset gen all
  reference.
- **Tests that lock it:** `Tests/ZM_Tests_Species.cpp` (category `ZM_Data`) -- 11
  integrity tests: count==152 + index self-consistency, unique names, valid
  types, evolution-graph shape (stage+1, same family/archetype/rarity, no
  self-loop), families well-formed (linear chains, one species per stage),
  family-size distribution (40/13/6 vs GDD), base/final/legendary counts,
  archetype-family spread (18/6/7/4/6/7/5/6), every-type-on-two-families,
  family-seed consistency+uniqueness, size-class monotonicity. Boot suite 1090
  ran / 0 failed.
- **Reversibility:** easy -- additive `Source/Data/` files; the struct grows
  (base-stats + learnset fields) in follow-up increments; the `ZM_SPECIES_ID`
  order is save-stable (append-only).

## 2026-07-10 -- ZM-D-019 -- Zenithmon boot unit tests are gated in CI via a run_unit_gate.ps1 boot step (ratcheted baseline)

- **Decision:** `zm-tests.yml` gains a step that boots `zenithmon.exe` headless
  through the shared `Tools/run_unit_gate.ps1` (`-Baseline 1079`) to run the boot
  ZENITH_TEST suite (engine units + Zenithmon `ZM_*` cases) and fail on any
  failure. The baseline is an exact-count ratchet (like engine-gate): each PR that
  changes the `ZM_*` unit count -- or an engine PR that changes the engine unit
  count -- bumps the number in the same PR.
- **Why:** discovered while landing S1 -- both `zenith test` (harness default) and
  the two prior zm-tests steps pass `--skip-unit-tests`, so Zenithmon's unit tests
  (the S1/S2 gate backbone, ~460 cases at end state) NEVER ran in CI. The plan
  designates the boot unit suite as the CI backbone; DP/CB never hit this because
  they carry almost no game-side unit tests. `run_unit_gate.ps1` is the proven
  engine-gate pattern (tool-exports ON so asset-export units work; watchdog-kills
  the known tools-build idle after the units line is logged).
- **Tests that lock it:** the step itself (red on any unit failure or count !=
  baseline); validated locally = "1079 ran, 1078 passed, 0 failed, 1 skipped" (the
  1 skip is the pre-existing quarantined `GraphComponent::RegistryWideNodeRoundTrip`).
- **Reversibility:** easy -- delete the step. The baseline's coupling to the
  engine unit count is the known maintenance cost (CIPolicy.md section 1); a
  follow-up may switch to a failures-only check if the ratchet churns
  (Questions.md Q-2026-07-10-004).

## 2026-07-10 -- ZM-D-018 -- Type system: save-stable ZM_TYPE enum + golden-locked 18x18 chart with a dual-type product API

- **Decision:** the 18 types are one `enum ZM_TYPE : u_int` (`Source/Data/ZM_Types.h`)
  in the GDD-section-6 order, which is simultaneously the dex/UI order and the
  row/column order of the chart; the range is append-only (save-stable). The
  effectiveness matrix is a `const` 18x18 float table (`ZM_TypeChart.cpp`) of
  {0, 0.5, 1, 2}, mapping the standard 18-type relationships onto the original
  names. Lookups are a stateless namespace `ZM_TypeChart`:
  `GetEffectiveness(atk, def)` + `GetDualTypeEffectiveness(atk, def1, def2)`, where
  `def2 == ZM_TYPE_NONE` (== `ZM_TYPE_COUNT`) collapses to the single lookup and a
  duplicated slot is never squared.
- **Why:** types are consumed by species/moves/damage from S1 on; a save-stable
  enum + a compiled table keeps zero file I/O in headless tests (ZM-D-009) and
  makes the chart diffable. The dual-type product belongs with the chart (4x /
  0.25x / 0x matchups) and is testable before species exist.
- **Tests that lock it:** `Tests/ZM_Tests_Data.cpp` -- `TypeChart_MatchesGolden`
  (an independent golden 18x18 compiled into the TU: the two-place-change lock,
  TestPlan 5.1), `TypeChart_AllCellsLegal`, `TypeChart_ImmunityCountIsEight`, the
  GDD design-intent spot checks (`StarterTriangle`, `SecondTriangleAndGhostNormal`,
  `DrakeChecks`, `ImmunitiesAndIronWall`), `TypeChart_DualTypeProducts`,
  `Types_ToStringContract`.
- **Reversibility:** easy -- additive `Source/Data/` files, no engine change;
  reordering/renaming types is a save-migration concern only after content ships.

## 2026-07-10 -- ZM-D-017 -- Docs/ becomes a self-sufficient autonomy hub: MasterPlan committed, lifecycle-loop prompt, hard-stop visual gates, permission allowlist

- **Decision (user-directed):** the Docs directory must carry the whole
  project lifecycle with the only human inputs being (a) pasting/looping a
  StartPrompts.md prompt and (b) visual-gate sign-offs. Changes: the approved
  program plan is committed as MasterPlan.md (it previously lived only in a
  machine-local `~/.claude/plans/` file) and referenced from every start
  prompt; StartPrompts.md gains prompt 0 (idempotent lifecycle-loop iteration,
  carries the user's standing merge-on-green authorization for the loop's own
  PRs) and prompt 4 (gate sign-off); `Tools/zenith_gh.ps1` wraps gh with
  self-bootstrapping auth; a checked-in `.claude/settings.json` allowlists the
  loop's build/test/git/gh commands (exact rules user-approved).
- **Gate policy (user's explicit choice):** the loop HARD-STOPS at every
  stage's visual check (incl. S4 gallery, S8 go/no-go) -- automated gate items
  run, screenshot evidence is captured, Status.md gets a `GATE-WAIT: S<n>`
  marker, and nothing proceeds until the user's prompt-4 sign-off lands in
  this log. The loop never signs its own gates.
- **Why:** S0 proved the failure modes: the plan file was unversioned and
  machine-local; gh had no session auth; permission prompts and the
  self-merge guard stall unattended runs; `gh run rerun` cannot re-evaluate
  against new master.
- **Tests that lock it:** none executable; the contract is the prompts +
  allowlist themselves (version-controlled) and this entry.
- **Reversibility:** trivial -- edit StartPrompts.md / delete the allowlist;
  gate policy can be relaxed by a new user decision here.

## 2026-07-10 -- ZM-D-016 -- Master branch protection CREATED with `zm-tests` as the sole machine-enforced required check

- **Decision:** master had NO branch protection and no rulesets at all (the
  repo's "required checks" had been purely conventional). On the user's
  direction ("Add zm-tests yourself"), classic branch protection was created
  via the API: required status checks `[zm-tests]`, `strict=false`,
  `enforce_admins=false`, no required reviews.
- **Why:** the S0 gate requires zm-tests to actually block merges;
  `enforce_admins=false` preserves the owner's established direct-push
  workflow (agents always land via PRs, so agents are always gated). Other
  gates stay blocking-by-discipline because several are path-filtered and a
  required check that never reports deadlocks a PR.
- **Tests that lock it:** none (GitHub configuration); verified by
  `gh api repos/tomosh22/Zenith/branches/master/protection`.
- **Reversibility:** trivial (delete/edit the protection rule); recorded in
  CIPolicy.md section 4 + ManualSetupChecklist.md.

## 2026-07-10 -- ZM-D-015 -- Three pre-existing master-red CI gates fixed as a prerequisite PR rather than inherited red

- **Decision:** engine-gate, layering-gate, and scaffold-smoke had been red on
  master since 2026-07-07/08 (before Zenithmon existed). Rather than merging
  S0 with inherited red checks, they were fixed in a dedicated PR (#144,
  `0844689e`): unit baseline 1053->1068 single-sourced in
  `Tools/run_unit_gate.ps1` (test_scaffold.ps1 reuses it), `Flux_HDR.cpp`
  g_xEngine reaches reduced via the established local-hoist idiom (fixed, not
  allow-listed), and regen.ps1 given a dotnet-exec fallback on the tracked
  Sharpmake dll (+ scaffold-smoke got `lfs: true` and the standard
  `/p:WindowsTargetPlatformVersion=10.0` build override).
- **Why:** "nothing merges red" is only meaningful if master itself can go
  green; every future Zenithmon stage PR needs a green baseline.
- **Tests that lock it:** the gates themselves (all 9 checks green on #144;
  all 10 green on the rebased #143); scaffold smoke 11/0 locally.
- **Reversibility:** each fix is independent and small; the baseline bump is
  a ratchet (future engine-test additions bump it again in ONE place).

## 2026-07-09 -- ZM-D-014 -- Engine name-validation narrowed to a PascalCase word boundary so 'Zenithmon' is a legal game name

- **Decision:** `zenith new Zenithmon` was rejected by the blanket
  `Zenith*`/`Sentinel*` reserved-prefix rule in BOTH game-name validators: PS
  `Test-ZenithGameNameSyntax` in `Build/zenith_buildsystem.psm1` and C++
  `ZenithHub_GameScan::ValidateName`. Both were narrowed to a PascalCase word
  boundary: reject `Zenith`/`Sentinel` alone or followed by an uppercase letter
  or digit; a lowercase continuation (e.g. `Zenithmon`) is a distinct word and
  valid.
- **Why:** the reservation exists to protect engine/test module names
  (`ZenithECS`, `SentinelAI`, ...); `Zenithmon` collides with none of them --
  the blanket rule was broader than its intent.
- **Tests that lock it:** `Build/Tests/run_buildsystem_tests.ps1` (suite 45
  passed / 0 failed) + the shared pinned vectors in
  `Tools/ZenithCli/Tests/name_validation_cases.txt` (consumed by both
  validators) + the ZenithHub selftest.
- **Reversibility:** reverting the validators would orphan this project --
  reversible only by renaming the game.

## 2026-07-09 -- ZM-D-013 -- No per-game runner script; the unified `zenith test` harness is the only test runner

- **Decision:** Zenithmon never gets a `run_zm_tests.ps1`. All test execution --
  local, stage gates, and CI (`.github/workflows/zm-tests.yml`) -- goes through
  `zenith test Zenithmon` (`Tools/ZenithCli/ZenithCli.psm1` ->
  `ZenithTestHarness.psm1`; flags `--filter/--headless/--results-dir/--config/
  --per-process/--fail-fast`; exit codes 0 OK / 1 usage / 2 validation /
  3 generation / 4 build-or-test / 5 not-found).
- **Why:** the old per-game `Tools/run_*_tests.ps1` scripts were DELETED at
  commit `c29e28f8` in favor of the unified harness; a per-game script would be
  legacy surface on day one (repo mandate: no legacy/compat code).
- **Tests that lock it:** the `zm-tests` CI workflow invokes
  `zenith.bat test Zenithmon --headless` directly; every stage gate in
  [Roadmap.md](Roadmap.md) cites the same command.
- **Reversibility:** none needed -- a per-game script would be a policy
  violation, not an option.

## 2026-07-09 -- ZM-D-012 -- Scene build index 0 = FrontEnd.zscen, boot-authored title screen (DP convention)

- **Decision:** the game boots into `FrontEnd.zscen` at build index 0: camera +
  "Zenithmon" title text + the game component. The scene is boot-authored by
  tools builds (editor-automation steps re-author it every tools boot) and the
  baked `.zscen` is git-ignored. The build-index table follows the plan:
  0 FrontEnd, 1 Battle, 2-12 towns, 20-34 routes + Victory Road, 40+ interiors,
  95 Tower (exact per-scene assignments TBD at S9/S10 via ZM_WorldSpec).
- **Why:** matches the proven DevilsPlayground convention (index 0 = FrontEnd,
  boot-authored, reloaded between batched tests by the harness).
- **Tests that lock it:** `Tests/ZM_AutoTests_Boot.cpp` (`ZM_Boot_Test`) + the 2
  boot unit tests in `Tests/ZM_Tests_Boot.cpp`.
- **Reversibility:** index remaps are cheap until S3, when warps start
  referencing build indices through ZM_WorldSpec.

## 2026-07-09 -- ZM-D-011 -- S0 keeps the scaffold placeholder ZM_GameComponent (bobbing cube)

- **Decision:** `ZM_GameComponent` (registered `"ZM_Game"`, serialization
  order 100) retains the `zenith new` scaffold's bobbing-cube behaviour as the
  S0 placeholder until the S1 data core and S3 overworld systems land.
- **Why:** S0 is skeleton/harness/CI/docs only; a live registered component
  proves the registration + serialization + between-tests plumbing without
  inventing gameplay ahead of its stage.
- **Tests that lock it:** `Tests/ZM_Tests_Boot.cpp` (2 unit tests) +
  `ZM_Boot_Test` -- these pin boot health, not the cube; the placeholder is
  free to be replaced.
- **Reversibility:** trivial -- it is a placeholder by design.

## 2026-07-08 -- ZM-D-010 -- Battle engine is headless C++ (not Behaviour Graphs) with an append-only event stream

- **Decision:** the battle turn loop is a seeded, deterministic C++ state
  machine (`ZM_BattleEngine`): `Begin(config,seed)` -> `SubmitAction` ->
  `ResolveTurn()` -> append-only `ZM_BattleEvent` stream, the single source of
  truth for both tests and presentation; the engine never formats strings or
  touches UI. Behaviour graphs are glue only (menu flow, NPC events, cutscene
  beats). Origin: the approved plan, `zenithmon-pok-mon-nested-puddle.md`.
- **Why:** rule-based logic needs exact-replay determinism and full headless
  unit-test coverage; no in-repo turn-based graph reference exists.
- **Tests that lock it:** S2 gate (~370 unit tests incl. scripted seeded
  battles with exact expected event streams + 2,000-battle fuzz soak).
- **Reversibility:** low -- reversing means rewriting S2; do not revisit.

## 2026-07-08 -- ZM-D-009 -- Game data = compiled const C-array tables, not disk assets

- **Decision:** species/moves/items/abilities/natures/type chart/encounters/
  trainers/dex text live as `const` C arrays in `Source/Data/*.cpp`; the
  "assets baked to disk" mandate covers meshes/textures/anims only. Origin: the
  approved plan.
- **Why:** compile-time validated, diffable in review, zero file I/O in
  headless CI tests.
- **Tests that lock it:** the `ZM_Tests_Data` validation suite +
  `ZM_DataRegistry` integrity tests (S1 gate).
- **Reversibility:** mechanical to move tables to files later, but it would
  sacrifice the zero-I/O headless-CI property -- user decision required.

## 2026-07-08 -- ZM-D-008 -- Battle format: singles only

- **Decision:** all battles are 1v1 singles; doubles is an explicit scope cut
  (see [Scope.md](Scope.md)). Struct layout does not preclude doubles later.
  Origin: the approved plan.
- **Why:** doubles is roughly 2x targeting/AI/UI complexity for marginal value.
- **Tests that lock it:** the entire S2 battle suite assumes single active
  monster per side.
- **Reversibility:** additive later behind a new scope decision; nothing to
  unwind now.

## 2026-07-08 -- ZM-D-007 -- Overworld-to-battle = ADDITIVE battle scene at world offset (0, -2000, 0) with overworld pause

- **Decision:** encounters load the battle scene ADDITIVE at (0, -2000, 0),
  `SetScenePaused(overworld, true)`, switch camera/HUD, and `UnloadScene` on
  exit; one battle scene with ~6 swappable biome dressing sets, enclosed by a
  backdrop dome. Documented fallback if visual isolation fails: SINGLE load +
  world-state snapshot. Origin: the approved plan.
- **Why:** a SINGLE reload resets render systems + physics and re-streams
  terrain -- seconds of hitch at wild-encounter frequency.
- **Tests that lock it:** S5 gate windowed round-trip tests (exact overworld
  resume) + screenshot check for overworld bleed-through at the offset.
- **Reversibility:** medium -- the fallback path is designed and documented;
  switching costs the S5 transition work only.

## 2026-07-08 -- ZM-D-006 -- Door/route-edge transitions = SINGLE loads with spawn tags; player/camera are NOT persistent entities

- **Decision:** `ZM_WarpTrigger_Component {targetBuildIndex, spawnTag}` -> fade
  -> SINGLE load; the persistent `ZM_GameStateManager` (`DontDestroyOnLoad`)
  respawns player + follow camera at the tagged `ZM_SpawnPoint`. One-time
  placement at load is not gameplay teleportation. Origin: the approved plan.
- **Why:** SINGLE loads reset physics and would orphan a persistent Jolt body;
  respawn-at-tag is the safe pattern.
- **Tests that lock it:** S3 gate windowed door/warp round-trip test; later
  per-region traversal tests walk every warp edge (S9/S10).
- **Reversibility:** medium -- changing to persistent player entities requires
  engine work on physics-across-SINGLE-loads first.

## 2026-07-08 -- ZM-D-005 -- ZM_WorldSpec is the keystone world table

- **Decision:** one declarative compiled table describes the whole world --
  scenes (name, build index, kind, terrain set, encounter table + rate),
  connections/spawn tags, trainers, shops, gyms, story beats. Tools walk it to
  author terrains/scenes/graphs; runtime walks it for warps/encounters/gating.
  Origin: the approved plan.
- **Why:** ~40 scenes without 40 bespoke authoring functions; one source of
  truth keeps authoring, runtime, and tests in agreement.
- **Tests that lock it:** WorldSpec referential-integrity unit tests (every
  warp target, spawn tag, species, and trainer resolves) run on every PR.
- **Reversibility:** low -- everything from S3 on flows through it; treat as
  load-bearing.

## 2026-07-08 -- ZM-D-004 -- Tall grass samples a game-owned CPU copy of the density map

- **Decision:** `ZM_TallGrassSystem` loads the baked `GrassDensity.ztxtr` per
  outdoor scene, feeds `g_xEngine.Grass().SetDensityMap(...)` for rendering,
  and keeps its OWN CPU copy for gameplay sampling; player XZ quantized to 1 m
  tiles, encounter roll on tile transition where density >= 0.5; density map
  cleared on interiors/battle. Origin: the approved plan.
- **Why:** engine `SampleDensityMap` returns 1.0 when no map is set -- gameplay
  must not inherit that trap, and the grass singleton is render-owned state.
- **Tests that lock it:** S5 gate encounter tests (walk grass until encounter
  with rigged RNG); grass-state assertions at S9/S10 gates.
- **Reversibility:** low cost -- swapping to engine-side sampling is a small,
  local change if the engine semantics ever harden.

## 2026-07-08 -- ZM-D-003 -- Baked assets are git-ignored, regenerated under per-family manifest guards

- **Decision:** everything under `Games/Zenithmon/Assets/` is git-ignored (repo
  norm) and regenerated by tools builds; per-family manifest guards =
  generator-version stamp + file-existence (hardened RenderTest pattern).
  Consequence: a fresh CI checkout has NO assets, so every asset/scene-dependent
  automated test must exists-guard and `RequestSkip` (the CI-fix pattern from
  commit `94813489`). Origin: the approved plan.
- **Why:** ~30-50 min cold bake output does not belong in git; determinism makes
  the repo the recipe, not the artifact.
- **Tests that lock it:** bake-determinism gate (re-run tools boot -> zero
  diffs, byte-identical re-bake) + the CI headless suite passing on an
  assets-absent runner.
- **Reversibility:** policy-level; committing baked assets would need a repo-
  wide user decision.

## 2026-07-08 -- ZM-D-002 -- Engine changes scoped to E1-E5, all additive and back-compatible

- **Decision:** the only engine-level changes this project makes are: E1
  per-component serialized terrain-set name (replaces 6 hard-coded `Terrain/`
  path sites); E2 `AddStep_TerrainExportChunksRect` + streaming-path
  missing-chunk tolerance check; E3 `Zenith_UIText` typewriter reveal; E4
  `Zenith_UIGridLayoutGroup`; E5 grass singleton reset hygiene (wire
  `Grass().Reset()` into `ResetRenderSystems` + clear instances/flags/density
  map). Each lands with unit tests + a RenderTest boot regression check.
  Origin: the approved plan.
- **Why:** verified engine gaps (one-terrain-per-game, full-grid bake volume,
  no typewriter/grid widgets, grass state leaking across SINGLE loads) --
  scoping them up front prevents ad-hoc engine sprawl.
- **Tests that lock it:** per-change unit tests + RenderTest still boots green
  (default-path untouched) + DP/CB suites stay green.
- **Reversibility:** per-change -- each is additive with a legacy-default path,
  so individually revertable before Zenithmon content depends on it.

## 2026-07-03 -- ZM-D-001 -- Scope lock (user decisions)

- **Decision:** the in/out scope for Zenithmon is locked as recorded in
  [Scope.md](Scope.md): ~150-species dex / 18 types / 3-stage lines / rarity;
  classic 8-gym world with no Wild Area; the full battle core; extras =
  abilities, natures, IVs/EVs, weather + terrain effects, breeding; post-game =
  Champion rematch + Battle Tower. Out: audio, networking/multiplayer/trading,
  Dynamax-analog, doubles, Substitute/Encore/Transform/weight moves, open Wild
  Area.
- **Why:** product of multiple prior iteration rounds with the user; frozen to
  prevent scope creep across a ~13-stage build.
- **Tests that lock it:** [Scope.md](Scope.md) is the binding gate; stage gates
  audit shipped content against it.
- **Reversibility:** user decision only, recorded as a new entry in this log
  (Scope.md change-control rule).
