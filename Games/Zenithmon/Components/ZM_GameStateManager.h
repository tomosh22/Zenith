#pragma once

#include "Core/Zenith_Result.h"
#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_Entity.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"   // ZM_GameState (owned by value)

class Zenith_DataStream;
class ZM_PlayerController;
enum ZM_SAVE_SLOT : u_int;

enum ZM_WARP_TRANSITION_STATE : u_int
{
	ZM_WARP_TRANSITION_IDLE,
	ZM_WARP_TRANSITION_QUEUED,
	ZM_WARP_TRANSITION_WAITING_FOR_SCENE,
	ZM_WARP_TRANSITION_WAITING_FOR_SPAWN,
	ZM_WARP_TRANSITION_WAITING_FOR_CAMERA,
	ZM_WARP_TRANSITION_FADING_IN
};

class ZM_GameStateManager
{
public:
	using LoadSceneRequestCallback = void (*)(u_int uBuildIndex);

	static constexpr u_int uSERIALIZATION_VERSION = 1u;
	static constexpr u_int uINVALID_BUILD_INDEX = 0xFFFFFFFFu;
	static constexpr u_int uTAG_CAPACITY = 32u;
	static constexpr float fFADE_DURATION_SECONDS = 0.20f;
	static constexpr const char* szFADE_ELEMENT_NAME = "WarpFade";
	// SC5 whiteout destination: Dawnmere Village (build index 2), its TownCenter spawn.
	// ★ UNCHANGED BY ZM-D-176, AND THAT IS THE POINT. The separation comment below
	// exists precisely so the new-game move cannot drag whiteout along with it.
	static constexpr u_int uWHITEOUT_BUILD_INDEX = 2u;
	static constexpr const char* szWHITEOUT_SPAWN_TAG = "TownCenter";
	// ZM-D-176: a new run now begins in the player's bedroom -- PlayerHome (build
	// index 40) at its "Door" marker -- and leaves through the shipped
	// PlayerHomeExitTrigger into Dawnmere, instead of materialising in the town
	// square. The pair was ALREADY semantically separate from whiteout; as of this
	// change the two flows no longer share a destination at all, and
	// ZM_Data/NewGameEntry_DiffersFromTheWhiteoutDestination asserts they DIFFER
	// in BOTH fields so a future edit cannot quietly re-merge them.
	// The literal 40 is reconciled against the world table by
	// NewGameEntry_DestinationIsThePlayerHomeDoor (ZM_GetWorldSpec is not
	// constexpr, so a static constexpr cannot resolve it here).
	static constexpr u_int uNEW_GAME_BUILD_INDEX = 40u;
	static constexpr const char* szNEW_GAME_SPAWN_TAG = "Door";
	// The title screen, and the ONLY playerless destination in the game: FrontEnd
	// authors no Player, no ZM_SpawnPoint and no ZM_FollowCamera. Spelled as the
	// literal build index rather than resolved through ZM_GetWorldSpec on purpose --
	// ZM_GetWorldSpec asserts (fatally, in every configuration) on the ZM_SCENE_NONE
	// an unresolvable index returns, so an unvalidated destination must never reach
	// it. This mirrors the existing playerless-SOURCE branch in TryQueueWarp, which
	// compares m_iBuildIndex against the same literal 0.
	static constexpr u_int uFRONTEND_BUILD_INDEX = 0u;
	static constexpr const char* szFRONTEND_SPAWN_TAG = "Start";

	// ---- S8 item 2: the warp barrier TIMEOUT budgets --------------------------
	//
	// THREE of the six transition states poll and bare-`return` on failure --
	// WAITING_FOR_SCENE, WAITING_FOR_SPAWN and WAITING_FOR_CAMERA -- and the fade is
	// already driven to FULLY OPAQUE in QUEUED before IssueSingleLoad runs. A stall in
	// any of them was therefore a PERMANENT BLACK SCREEN with the player frozen: no
	// crash, no assert, no red test. These budgets are the escape.
	//
	// ★ FRAMES, NEVER WALL-CLOCK SECONDS, and that is a scar rather than a taste.
	// This project already owns one wall-clock gate
	// (GraphComponent::ThousandEntityUpdateBenchmark) that reds a REQUIRED CI check
	// from machine load alone (Q-2026-08-14-001). A seconds-based warp budget would
	// fire on a loaded CI box and never on a dev box; frames are deterministic under
	// Zenith_InputSimulator::SetFixedDt, which the automated harness already uses.
	//
	// ★ DELIBERATELY GENEROUS. A budget too tight turns a slow cold scene load into a
	// spurious failure -- worse than the hang it replaces -- and because a hang is
	// INFINITE, even a very generous ceiling captures the entire win. WAITING_FOR_SCENE
	// gets the most headroom (async scene load plus terrain streaming);
	// WAITING_FOR_CAMERA should resolve in a frame or two. Every one of them sits above
	// the automated harness's own largest per-barrier phase allowance (420 frames in
	// ZM_AutoTests_WorldTraversal / ZM_AutoTests_Overworld), so a harness budget always
	// fails FIRST and this timeout can never mask a real regression by escaping ahead
	// of it.
	//
	// ★ PROVISIONAL, AND INSTRUMENTED SO THEY CAN BE TIGHTENED FROM DATA RATHER THAN
	// FROM A SECOND GUESS. Every SUCCESSFUL exit from a polling state logs the frames
	// it actually consumed at Zenith_Log level (see TrackTransitionStateDwell). Set
	// these from that measurement; until somebody has it, do not tighten them.
	static constexpr u_int uWARP_TIMEOUT_FRAMES_WAITING_FOR_SCENE = 3600u;   // 60 s at 60 Hz
	static constexpr u_int uWARP_TIMEOUT_FRAMES_WAITING_FOR_SPAWN = 1800u;   // 30 s at 60 Hz
	static constexpr u_int uWARP_TIMEOUT_FRAMES_WAITING_FOR_CAMERA = 600u;   // 10 s at 60 Hz

	ZM_GameStateManager() = delete;
	explicit ZM_GameStateManager(Zenith_Entity& xParentEntity);

	ZM_GameStateManager(const ZM_GameStateManager&) = delete;
	ZM_GameStateManager& operator=(const ZM_GameStateManager&) = delete;
	ZM_GameStateManager(ZM_GameStateManager&&) noexcept = default;
	ZM_GameStateManager& operator=(ZM_GameStateManager&&) noexcept = default;

	void OnStart();
	void OnUpdate(float fDeltaTime);
	void OnDestroy();

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

	static bool RequestWarp(u_int uTargetBuildIndex, const char* szSpawnTag);
	// True while a warp transition owns the screen (also the predicate
	// ZM_PlayerController uses to decide whether to freeze on Start).
	static bool IsWarpInProgress();
	// Collision sources use this generation-bearing ID seam to prove the body
	// entering them is the one authoritative active-scene Player. Foreign
	// additive-scene, duplicate, malformed, and bodyless controllers fail closed.
	static bool TryGetUniqueActiveScenePlayerEntityID(
		Zenith_EntityID& xEntityIDOut);
	static bool TryGetUniqueSingletonEntityID(Zenith_EntityID& xEntityIDOut);

	// Resolves the unique persistent ZM_GameStateManager and hands back its owned
	// ZM_GameState (mutable). Returns false + leaves pxGameStateOut null when no manager
	// exists (e.g. before boot). Cross-scene safe (the manager is DontDestroyOnLoad).
	static bool TryGetGameState(ZM_GameState*& pxGameStateOut);

	// Manager-owned title transactions. New Game stages the fixed starter, queues
	// the ordinary FrontEnd -> PlayerHome warp (ZM-D-176), then publishes the starter only after
	// the queue accepts it; it never touches a save slot. Continue reads the selected
	// slot into a local candidate, queues its validated resume position, then publishes
	// the complete candidate. Any failure leaves the live state and resume latch alone.
	static bool RequestNewGame();
	static Zenith_Status RequestContinue(ZM_SAVE_SLOT eSlot);

	// ---- S7 item 2 SC3: world-position capture, resume, quit-to-title ----------

	// The spawn tag the last COMPLETED transition placed the player at in the
	// active scene, or "" when none. Nothing tracked this before: m_szTargetSpawnTag
	// is the tag of an IN-FLIGHT warp and ResetTransitionState memsets it the moment
	// the warp finishes, so by the time anyone could ask, it was already gone.
	const char* GetLastArrivedSpawnTag() const { return m_szLastArrivedSpawnTag; }
	// The same answer from a free context. "" when there is no live manager.
	static const char* GetActiveSceneArrivedSpawnTag();

	// Capture the live active scene + the unique authoritative player's BODY CENTRE
	// + its yaw + the arrived spawn tag into xStateInOut.m_xWorldPosition.
	// TRANSACTIONAL: false with NO mutation when there is no unique bodied player,
	// no resolvable active scene, no spawn tag to record, or ZM_MakeWorldPosition
	// rejects the pose.
	// The recorded position is the capsule CENTRE, matching what
	// Zenith_Physics::GetBodyPosition returns and what the resume applies back --
	// spawn MARKERS store feet and CalculateSpawnCenter is the only place the two
	// conventions meet.
	static bool CaptureWorldPosition(ZM_GameState& xStateInOut);

	// Begin a RESUME: validate the saved position, queue the warp to its scene/tag
	// through the SAME validated TryQueueWarp path every other warp uses, and latch
	// a ONE-SHOT pose override that is applied after the marker teleport.
	// False -- with nothing latched and no warp queued -- when ZM_CanResume says no
	// or the warp itself is refused. A save whose transform is unusable but whose
	// scene+tag are good still resumes; it simply lands on the marker.
	static bool RequestResume(const ZM_WorldPosition& xResume);
	bool IsResumePending() const { return m_bResumePending; }

	// Begin a QUIT TO TITLE: fade out, SINGLE-load build index 0, and fade back in
	// WITHOUT waiting for a Player or a follow camera. FrontEnd authors neither, so
	// the ordinary spawn and camera barriers would park the machine on a permanently
	// opaque screen forever.
	static bool RequestQuitToFrontEnd();
	bool IsPlayerlessDestination() const { return m_bTargetIsPlayerless; }

	static bool IsWarpDestinationValid(u_int uTargetBuildIndex, const char* szSpawnTag);
	// Spawn MARKERS store FEET; bodies store CENTRES. This is the only place the
	// two conventions meet, and it converts with the compiled body contract --
	// NOT with the player's transform scale, which no longer describes the body.
	static Zenith_Maths::Vector3 CalculateSpawnCenter(
		const Zenith_Maths::Vector3& xMarkerFeetPosition);
	// Deterministic, headless-safe fade policy. Invalid/nonpositive delta time
	// leaves the clamped current alpha unchanged.
	static float AdvanceFadeAlpha(
		float fCurrentAlpha,
		float fTargetAlpha,
		float fDeltaTime);

	// The test reset preserves a still-live authoritative singleton EntityID.
	// It clears only session transition state and any injected load callback.
	static void ResetRuntimeStateForTests();
	static void SetLoadSceneRequestCallbackForTests(LoadSceneRequestCallback pfnCallback);
	// Re-seeds the persistent GameState to the fixed starter (D4). The manager is
	// DontDestroyOnLoad, so its m_xGameState survives between batched tests; the
	// between-tests hook calls this so a caught/levelled party cannot leak forward.
	// A safe no-op when no manager exists at hook time.
	static void ResetGameStateForTests();

	ZM_WARP_TRANSITION_STATE GetTransitionState() const { return m_eTransitionState; }
	// Consecutive POLLED updates spent in GetTransitionState(), counting from ZERO on
	// the first one -- so 0 means "this is the first update in this state" and the
	// budget above is reached on the (budget + 1)th. Only updates that actually reach
	// the per-frame switch count: a frame that bailed on a non-finite delta time or a
	// missing fade overlay neither polls nor spends budget. Meaningless before the
	// first update of a state, and reset by the state-change comparison at the top of
	// OnUpdate rather than at any assignment site.
	u_int GetFramesInTransitionState() const { return m_uFramesInTransitionState; }
	// True from the moment a barrier budget expired until the escape transition ends.
	// It is what makes the FADING_IN escape actually escape (see AdvanceFadeIn).
	bool HasTransitionTimedOut() const { return m_bTransitionTimedOut; }
	u_int GetTargetBuildIndex() const { return m_uTargetBuildIndex; }
	const char* GetTargetSpawnTag() const { return m_szTargetSpawnTag; }
	Zenith_EntityID GetFrozenPlayerEntityID() const { return m_xFrozenPlayerEntityID; }
	u_int GetIssuedLoadRequestCount() const { return m_uIssuedLoadRequestCount; }
	float GetFadeAlpha() const { return m_fFadeAlpha; }
	bool IsFadeVisible() const { return m_fFadeAlpha > 0.0f; }
	bool IsAuthoritativeSingleton() const;

private:
	bool TryQueueWarp(u_int uTargetBuildIndex, const char* szSpawnTag);
	Zenith_Status QueueResume(const ZM_WorldPosition& xResume);
	void ResetTransitionState(bool bEnableFrozenPlayer);
	void IssueSingleLoad();
	void PollForTargetScene();
	void PollForSpawnAndPlacePlayer();
	void PollForCameraAndBeginFadeIn();
	void AdvanceFadeIn(float fDeltaTime);
	// Applies the latched resume pose to the player body. MUST be called AFTER the
	// marker TeleportBody (which forces identity rotation and would destroy an
	// earlier pose write) and BEFORE the camera barrier (so the follow camera
	// acquires the FINAL pose and does not spring across the correction).
	//
	// The latch is NOT consumed here. It belongs to the TRANSITION, not to a single
	// placement attempt: PollForSpawnAndPlacePlayer can run SEVERAL times in one
	// transition, because both AdvanceFadeIn and PollForCameraAndBeginFadeIn push
	// the state back to WAITING_FOR_SPAWN whenever the frozen player id stops
	// matching the unique player, and every one of those passes re-runs the marker
	// TeleportBody. A latch spent on the first pass would let a later pass silently
	// leave the player standing on the default spawn -- with no diagnostic, and only
	// on the runs where the bounce happens. ResetTransitionState clears it on BOTH
	// the success tail and the cancel/test-reset path, so it can never outlive its
	// transition and can never retry forever. Every entry re-validates the pose, so
	// re-applying it is idempotent and still fail-closed.
	void ApplyPendingResumePlacement(Zenith_EntityID xPlayerEntityID);
	// Reset-or-advance the ONE shared barrier counter, and -- on a state CHANGE out of
	// a polling state -- log the frames that barrier actually consumed. Runs once per
	// polled update, immediately before the transition switch.
	void TrackTransitionStateDwell();
	// The frame budget for a polling state, or 0 for every other state. Zero is how
	// "this state has no timeout" is spelled, so QUEUED / FADING_IN / IDLE (which are
	// driven by the fade, not by a poll) are covered by the same one code path.
	static u_int GetTransitionTimeoutFrames(ZM_WARP_TRANSITION_STATE eState);
	// If the current polling state has burned its budget: LOG LOUDLY, then jump to
	// ZM_WARP_TRANSITION_FADING_IN. This is DIAGNOSIS, NOT RECOVERY -- see the body.
	void ApplyTransitionTimeoutIfExpired();
	// The shared arrival tail for both the playerful and the playerless fade-in.
	// Records the tag the transition arrived at and latches the milestone autosave
	// for a LATER frame. Must run BEFORE ResetTransitionState, which memsets the
	// target tag it copies from.
	void RecordArrivalAndLatchAutosave();
	bool ApplyFadeVisual();
	bool IsTargetSceneActive() const;
	bool TryResolveFrozenTargetPlayer(ZM_PlayerController*& pxControllerOut) const;
	static bool HasUniqueReadyFollowCamera(Zenith_EntityID xPlayerEntityID);
	static bool FindUniquePlayerInScene(Zenith_EntityID& xPlayerEntityIDOut);

	static Zenith_EntityID s_xSingletonEntityID;
	static LoadSceneRequestCallback s_pfnLoadSceneRequestForTests;

	Zenith_Entity m_xParentEntity;
	// Persistent player state (S5 item 5 SC2), owned BY VALUE so it rides the
	// DontDestroyOnLoad move. Seeded with the starter exactly once at first-boot init
	// (OnStart); reachable cross-scene via TryGetGameState for the battle write-back.
	ZM_GameState m_xGameState;
	Zenith_EntityID m_xFrozenPlayerEntityID = INVALID_ENTITY_ID;
	ZM_WARP_TRANSITION_STATE m_eTransitionState = ZM_WARP_TRANSITION_IDLE;
	u_int m_uTargetBuildIndex = uINVALID_BUILD_INDEX;
	u_int m_uIssuedLoadRequestCount = 0u;
	float m_fFadeAlpha = 0.0f;
	char m_szTargetSpawnTag[uTAG_CAPACITY] = {};

	// ---- S7 item 2 SC3 session state ------------------------------------------
	// All of it is SESSION state: WriteToDataStream still writes only the version
	// word, so none of this reaches a .zscen and uSERIALIZATION_VERSION stays 1.
	//
	// m_xPendingResume / m_bResumePending are cleared by ResetTransitionState (they
	// belong to one transition). m_szLastArrivedSpawnTag and
	// m_bArrivalAutosavePending deliberately are NOT: the arrival tail records them
	// and then calls ResetTransitionState, so clearing them there would erase the
	// very thing that was just recorded.
	ZM_WorldPosition m_xPendingResume;
	bool m_bResumePending = false;
	bool m_bTargetIsPlayerless = false;
	bool m_bArrivalAutosavePending = false;
	char m_szLastArrivedSpawnTag[uTAG_CAPACITY] = {};

	// ---- S8 item 2 barrier-timeout session state ------------------------------
	// SESSION-ONLY, exactly like the block above: WriteToDataStream still writes only
	// the version word, so none of this reaches a .zscen and uSERIALIZATION_VERSION
	// stays 1. Adding these members must not -- and does not -- touch the save format.
	//
	// ★ ONE COUNTER FOR ALL THREE POLLING STATES, NOT THREE. It is reset by comparing
	// m_eTransitionState against m_ePreviouslyPolledState at the top of the per-frame
	// update, DELIBERATELY NOT at each `m_eTransitionState = ...` assignment site.
	// There are a dozen of those spread over five functions; one would eventually be
	// missed, and a missed reset is a barrier that can never time out. The comparison
	// is robust to every site, present and future.
	//
	// ★ THE ONE CASE THIS SHAPE CANNOT SEE, RECORDED HONESTLY: a stall that OSCILLATES
	// between two states every single frame never dwells, so the counter resets each
	// frame and no budget is ever reached. The reachable candidate is
	// WAITING_FOR_SPAWN <-> WAITING_FOR_CAMERA, which requires the unique player to
	// stop matching the frozen id on every alternate frame. Every OTHER failure in all
	// three barriers PARKS (they bare-`return`), which is what this counter is for. If
	// an oscillating hang is ever observed, that wants a second, oscillation-aware
	// counter -- not a change to this one.
	u_int m_uFramesInTransitionState = 0u;
	ZM_WARP_TRANSITION_STATE m_ePreviouslyPolledState = ZM_WARP_TRANSITION_IDLE;
	// Latched by an EXPIRED barrier, honoured by AdvanceFadeIn, cleared by
	// ResetTransitionState so it can never outlive its transition. Without it the
	// mandated jump to FADING_IN would not escape anything: AdvanceFadeIn re-carries
	// the very barriers that just expired and would bounce the machine straight back
	// into them, one loud error per budget, forever.
	bool m_bTransitionTimedOut = false;
};
