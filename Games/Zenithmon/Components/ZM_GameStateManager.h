#pragma once

#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_Entity.h"

class Zenith_DataStream;

enum ZM_WARP_TRANSITION_STATE : u_int
{
	ZM_WARP_TRANSITION_IDLE,
	ZM_WARP_TRANSITION_QUEUED,
	ZM_WARP_TRANSITION_WAITING_FOR_SCENE,
	ZM_WARP_TRANSITION_WAITING_FOR_SPAWN
};

class ZM_GameStateManager
{
public:
	using LoadSceneRequestCallback = void (*)(u_int uBuildIndex);

	static constexpr u_int uSERIALIZATION_VERSION = 1u;
	static constexpr u_int uINVALID_BUILD_INDEX = 0xFFFFFFFFu;
	static constexpr u_int uTAG_CAPACITY = 32u;

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
	// PlayerController calls this from OnStart so a scene-owned replacement
	// cannot consume one input frame before the later-ordered manager resolves it.
	static bool ShouldFreezePlayerOnStart();
	// Collision sources use this generation-bearing ID seam to prove the body
	// entering them is the one authoritative active-scene Player. Foreign
	// additive-scene, duplicate, malformed, and bodyless controllers fail closed.
	static bool TryGetUniqueActiveScenePlayerEntityID(
		Zenith_EntityID& xEntityIDOut);
	static bool TryGetUniqueSingletonEntityID(Zenith_EntityID& xEntityIDOut);
	static bool IsWarpDestinationValid(u_int uTargetBuildIndex, const char* szSpawnTag);
	static Zenith_Maths::Vector3 CalculateSpawnCenter(
		const Zenith_Maths::Vector3& xMarkerFeetPosition,
		const Zenith_Maths::Vector3& xPlayerScale);

	// The test reset preserves a still-live authoritative singleton EntityID.
	// It clears only session transition state and any injected load callback.
	static void ResetRuntimeStateForTests();
	static void SetLoadSceneRequestCallbackForTests(LoadSceneRequestCallback pfnCallback);

	ZM_WARP_TRANSITION_STATE GetTransitionState() const { return m_eTransitionState; }
	u_int GetTargetBuildIndex() const { return m_uTargetBuildIndex; }
	const char* GetTargetSpawnTag() const { return m_szTargetSpawnTag; }
	Zenith_EntityID GetFrozenPlayerEntityID() const { return m_xFrozenPlayerEntityID; }
	u_int GetIssuedLoadRequestCount() const { return m_uIssuedLoadRequestCount; }
	bool IsAuthoritativeSingleton() const;

private:
	bool TryQueueWarp(u_int uTargetBuildIndex, const char* szSpawnTag);
	void ResetTransitionState(bool bEnableFrozenPlayer);
	void IssueSingleLoad();
	void PollForTargetScene();
	void PollForSpawnAndPlacePlayer();
	static bool FindUniquePlayerInScene(
		Zenith_EntityID& xPlayerEntityIDOut,
		Zenith_Maths::Vector3& xPlayerScaleOut);

	static Zenith_EntityID s_xSingletonEntityID;
	static LoadSceneRequestCallback s_pfnLoadSceneRequestForTests;

	Zenith_Entity m_xParentEntity;
	Zenith_EntityID m_xFrozenPlayerEntityID = INVALID_ENTITY_ID;
	ZM_WARP_TRANSITION_STATE m_eTransitionState = ZM_WARP_TRANSITION_IDLE;
	u_int m_uTargetBuildIndex = uINVALID_BUILD_INDEX;
	u_int m_uIssuedLoadRequestCount = 0u;
	char m_szTargetSpawnTag[uTAG_CAPACITY] = {};
};
