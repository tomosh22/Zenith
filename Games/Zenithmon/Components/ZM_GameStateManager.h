#pragma once

#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_Entity.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"   // ZM_GameState (owned by value)

class Zenith_DataStream;
class ZM_PlayerController;

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
	void ResetTransitionState(bool bEnableFrozenPlayer);
	void IssueSingleLoad();
	void PollForTargetScene();
	void PollForSpawnAndPlacePlayer();
	void PollForCameraAndBeginFadeIn();
	void AdvanceFadeIn(float fDeltaTime);
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
};
