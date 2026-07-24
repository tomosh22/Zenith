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
	static constexpr u_int uWHITEOUT_BUILD_INDEX = 2u;
	static constexpr const char* szWHITEOUT_SPAWN_TAG = "TownCenter";
	// A new run enters Dawnmere through the ordinary validated warp path. Kept
	// semantically separate from whiteout even though both currently share a
	// destination, so either flow may move later without silently moving the other.
	static constexpr u_int uNEW_GAME_BUILD_INDEX = 2u;
	static constexpr const char* szNEW_GAME_SPAWN_TAG = "TownCenter";
	// The title screen, and the ONLY playerless destination in the game: FrontEnd
	// authors no Player, no ZM_SpawnPoint and no ZM_FollowCamera. Spelled as the
	// literal build index rather than resolved through ZM_GetWorldSpec on purpose --
	// ZM_GetWorldSpec asserts (fatally, in every configuration) on the ZM_SCENE_NONE
	// an unresolvable index returns, so an unvalidated destination must never reach
	// it. This mirrors the existing playerless-SOURCE branch in TryQueueWarp, which
	// compares m_iBuildIndex against the same literal 0.
	static constexpr u_int uFRONTEND_BUILD_INDEX = 0u;
	static constexpr const char* szFRONTEND_SPAWN_TAG = "Start";

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
	// the ordinary FrontEnd -> Dawnmere warp, then publishes the starter only after
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
	static Zenith_Maths::Vector3 CalculateSpawnCenter(
		const Zenith_Maths::Vector3& xMarkerFeetPosition,
		const Zenith_Maths::Vector3& xPlayerScale);
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
	// The shared arrival tail for both the playerful and the playerless fade-in.
	// Records the tag the transition arrived at and latches the milestone autosave
	// for a LATER frame. Must run BEFORE ResetTransitionState, which memsets the
	// target tag it copies from.
	void RecordArrivalAndLatchAutosave();
	bool ApplyFadeVisual();
	bool IsTargetSceneActive() const;
	bool TryResolveFrozenTargetPlayer(ZM_PlayerController*& pxControllerOut) const;
	static bool HasUniqueReadyFollowCamera(Zenith_EntityID xPlayerEntityID);
	static bool FindUniquePlayerInScene(
		Zenith_EntityID& xPlayerEntityIDOut,
		Zenith_Maths::Vector3& xPlayerScaleOut);

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
};
