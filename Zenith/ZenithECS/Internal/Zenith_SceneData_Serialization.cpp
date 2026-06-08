#include "Zenith.h"
// Scene save/load split out of Zenith_SceneData.cpp so the god-file stays
// focused on lifecycle + dispatch. All serialization logic for
// Zenith_SceneData lives here: SaveToFile, LoadFromFile, LoadFromDataStream,
// ReadEntityFromDataStream.
//
// (A peer split of Zenith_SceneData.cpp; the scene-system logic itself lives
// in the EntityComponent/Internal/Zenith_SceneSystem_*.cpp TUs.)

#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
// Relocated RAII scope type: the load path scopes creation onto the scene being
// deserialized via Zenith_SceneCreationTargetScope.
#include "ZenithECS/Internal/Zenith_SceneSystem_InternalScopes.h"
// Phase 7a: the explicit default-component AddComponent in the entity load loop is gone --
// the loader now creates entities BARE and the component stream supplies the owning component via
// the engine-registered meta create-thunk. This TU no longer names any concrete component,
// so the concrete component header include was dropped (the matching ecs_leaf_allowlist.txt
// entry goes stale; the orchestrator re-seeds it).

void Zenith_SceneData::SaveToFile(const std::string& strFilename, bool bIncludeTransient)
{
	Zenith_DataStream xStream;

	xStream << uSCENE_MAGIC;
	xStream << uSCENE_VERSION_CURRENT;

	u_int uNumEntities = 0;
	for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xID = m_xActiveEntities.Get(u);
		const Zenith_EntitySlot& xSlot = Zenith_ECS_EntityStore().m_axEntitySlots.Get(xID.m_uIndex);
		if (bIncludeTransient || !xSlot.m_bTransient)
		{
			uNumEntities++;
		}
	}
	xStream << uNumEntities;

	for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xID = m_xActiveEntities.Get(u);
		const Zenith_EntitySlot& xSlot = Zenith_ECS_EntityStore().m_axEntitySlots.Get(xID.m_uIndex);
		if (!bIncludeTransient && xSlot.m_bTransient)
		{
			continue;
		}
		Zenith_Entity xEntity(this, xID);
		xEntity.WriteToDataStream(xStream);
	}

	// Only write valid camera index if the camera entity was actually included
	// in the file — transient entities may be excluded, which would otherwise
	// leave a dangling file index.
	uint32_t uMainCameraIndex = Zenith_EntityID::INVALID_INDEX;
	if (m_xMainCameraEntity.IsValid())
	{
		const Zenith_EntitySlot& xCameraSlot = Zenith_ECS_EntityStore().m_axEntitySlots.Get(m_xMainCameraEntity.m_uIndex);
		if (bIncludeTransient || !xCameraSlot.m_bTransient)
		{
			uMainCameraIndex = m_xMainCameraEntity.m_uIndex;
		}
	}
	xStream << uMainCameraIndex;

	xStream.WriteToFile(strFilename.c_str());

	ClearDirty();
}

bool Zenith_SceneData::LoadFromFile(const std::string& strFilename)
{
	// Note: Flux render systems and Physics reset are now handled by
	// Zenith_SceneSystem::Get().LoadScene() for SINGLE mode loads only.
	// This allows LoadFromFile to be used for ADDITIVE loads without
	// destroying render data from other loaded scenes.
	//
	// B.8: gatekeep LoadFromFile so external callers can't silently nuke a loaded
	// scene's entities. SceneManager always drives this against a fresh SceneData
	// from CreateEmptyScene (entity count = 0). Any non-SceneManager caller that
	// reaches this with live entities is almost certainly bypassing the lifecycle
	// (no unload teardown, no active-scene handling, no
	// physics reset) and should route via Zenith_SceneSystem::Get().LoadScene instead.
	Zenith_Assert(m_xActiveEntities.GetSize() == 0,
		"Zenith_SceneData::LoadFromFile: scene is not empty (entityCount=%u). "
		"Call Zenith_SceneSystem::Get().LoadScene to route through the "
		"full lifecycle, or call Reset() first if you intentionally want "
		"to skip the callbacks.", m_xActiveEntities.GetSize());
	// B.7: use Reset (the safe variant), not ScrubAndReset. We're about to overwrite
	// name/path/buildIndex via deserialization + the caller's assignments — wiping
	// them first would race with LoadScene which sets m_strPath before calling this.
	// The "scene is not empty" assert above ensures the caller already cleared the
	// entity table, so this call is effectively "defensive cleanup of partial state".
	Reset();

	Zenith_DataStream xStream;
	xStream.ReadFromFile(strFilename.c_str());

	if (!xStream.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "LoadFromFile: Failed to read file '%s'", strFilename.c_str());
		return false;
	}

	if (!LoadFromDataStream(xStream))
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "LoadFromFile: Failed to parse scene file '%s'", strFilename.c_str());
		return false;
	}

	// Lifecycle dispatch (OnAwake/OnEnable/OnStart) is handled by
	// Zenith_SceneSystem::Get().LoadScene() after this method returns.
	// Do NOT dispatch here to avoid double-dispatch.

	// Only set path if not already set by caller (LoadScene sets canonical path before calling this)
	if (m_strPath.empty())
	{
		m_strPath = strFilename;
	}
	TransitionTo(SCENE_STATE_LOADED);
	ClearDirty();
	return true;
}

Zenith_EntityID Zenith_SceneData::ReadEntityFromDataStream(Zenith_DataStream& xStream, u_int uVersion,
	Zenith_HashMap<uint32_t, Zenith_EntityID>& xFileIndexToNewID)
{
	uint32_t uFileIndex;
	std::string strName;
	uint32_t uFileParentIndex = Zenith_EntityID::INVALID_INDEX;
	// Whether the parent file-index came from the entity RECORD (v3 and v7+). For
	// v4/5/6 the parent lives inside the legacy component payload and is sunk by the
	// owning component's versioned reader during DeserializeEntityComponents instead.
	bool bParentFromRecord = false;

	if (uVersion == 3)
	{
		xStream >> uFileIndex;
		xStream >> uFileParentIndex;
		xStream >> strName;
		bParentFromRecord = true;

		// v3 stored an explicit child-index list per entity. Children are now
		// reconstructed from the parent-file-index pass at the end of load, so
		// we advance past the list without retaining it.
		uint32_t uChildCount = 0;
		xStream >> uChildCount;
		for (uint32_t i = 0; i < uChildCount; ++i)
		{
			uint32_t uChildIndex;
			xStream >> uChildIndex;
		}
	}
	else // v4/v5/v6/v7 share the compact [fileIndex][name] head
	{
		xStream >> uFileIndex;
		xStream >> strName;

		// v7 (Phase 7a): the parent file-index moved OUT of the legacy component payload into the
		// entity record, immediately after the name. v4/5/6 carry no such field here --
		// their parent is still inside the legacy component payload (handled by the owning
		// component's versioned reader, schema < 7).
		if (uVersion >= 7u)
		{
			xStream >> uFileParentIndex;
			bParentFromRecord = true;
		}
	}

	// Phase 7a: create the entity BARE (slot only, no default components). The component
	// stream supplies the owning component via the engine-registered meta create-thunk, so this
	// TU never names a concrete component type. For both legacy files (owning component in the
	// stream, v4/5/6) and v7 files (owning component in the stream, parent in the record) the
	// component is added by DeserializeEntityComponents below.
	Zenith_EntityID xNewID = CreateEntity();
	xFileIndexToNewID[uFileIndex] = xNewID;

	Zenith_EntitySlot& xSlot = Zenith_ECS_EntityStore().m_axEntitySlots.Get(xNewID.m_uIndex);
	xSlot.m_strName = strName;
	xSlot.m_bEnabled = true;
	xSlot.m_bTransient = false;

	Zenith_Entity xEntity(this, xNewID);

	// Pass the .zscen header version so the component reader knows whether to
	// consume the per-component schemaVersion field (scene v6+). Pre-v6 files
	// carry no such field; the version gate keeps them byte-aligned. The owning
	// component's versioned reader migrates the in-payload parent for legacy (schema < 7) blobs.
	Zenith_ComponentMetaRegistry::Get().DeserializeEntityComponents(xEntity, xStream, uVersion);

	if (bParentFromRecord && uFileParentIndex != Zenith_EntityID::INVALID_INDEX)
	{
		// v3 / v7: the parent came from the entity RECORD. Sink it into the slot
		// pending-parent (overwriting anything a component-read may have sunk), exactly
		// as the prior v3 record-parent path did. v4/5/6 fall through: their parent was
		// already sunk by the owning component's versioned reader.
		xEntity.SetPendingParentFileIndex(uFileParentIndex);
	}

	return xNewID;
}

bool Zenith_SceneData::ValidateSceneStream(Zenith_DataStream& xStream)
{
	// Non-destructive: every return path restores the cursor to its entry offset
	// so LoadFromDataStream can call this and then re-read the header from the
	// same starting position without desyncing. Check order is the same as the
	// historical inline block: IsValid → size → magic → version-range.
	const uint64_t ulSavedCursor = xStream.GetCursor();

	if (!xStream.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "Malformed scene file: stream is empty or failed to read");
		xStream.SetCursor(ulSavedCursor);
		return false;
	}

	// Validate stream has minimum header data (magic + version = 8 bytes)
	static constexpr uint64_t ulMIN_HEADER_SIZE = sizeof(u_int) * 2;
	if (xStream.GetSize() < ulMIN_HEADER_SIZE)
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "Malformed scene file: too small (size=%llu, minimum=%llu)",
			xStream.GetSize(), ulMIN_HEADER_SIZE);
		xStream.SetCursor(ulSavedCursor);
		return false;
	}

	u_int uMagicNumber;
	u_int uVersion;
	xStream >> uMagicNumber;
	xStream >> uVersion;

	if (uMagicNumber != uSCENE_MAGIC)
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "Invalid scene file format: bad magic number 0x%08X (expected 0x%08X)",
			uMagicNumber, uSCENE_MAGIC);
		xStream.SetCursor(ulSavedCursor);
		return false;
	}

	if (uVersion > uSCENE_VERSION_CURRENT || uVersion < uSCENE_VERSION_MIN_SUPPORTED)
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "Unsupported scene file version %u", uVersion);
		xStream.SetCursor(ulSavedCursor);
		return false;
	}

	xStream.SetCursor(ulSavedCursor);
	return true;
}

bool Zenith_SceneData::LoadFromDataStream(Zenith_DataStream& xStream)
{
	// C9: see CreateEntity for rationale — render-task-ordering invariant.
	// LoadFromDataStream creates entities + components en masse; if render
	// tasks are actively reading, those reads would see half-populated scenes.
	Zenith_Assert(!Zenith_AreRenderTasksActive(),
		"LoadFromDataStream: scene mutation while render tasks are reading — render-task invariant violated");
	// B.8: same gate as LoadFromFile. Deserializing over a populated scene would
	// blindly append new entities without running OnDestroy / unload teardown
	// for the old ones. SceneManager guarantees this by always calling this on
	// freshly-created scene data; tests intentionally start from CreateEmptyScene.
	Zenith_Assert(m_xActiveEntities.GetSize() == 0,
		"Zenith_SceneData::LoadFromDataStream: scene is not empty (entityCount=%u). "
		"Load against a fresh scene (CreateEmptyScene) or call Reset() first.",
		m_xActiveEntities.GetSize());

	// B1: route GetDefaultCreationScene-aware APIs at the scene being deserialized
	// for the duration of this call. Defense-in-depth: the sync LoadScene body and
	// async Phase 1 already open an outer scope around their CreateEmptyScene/
	// LoadFromDataStream pair, but direct callers (editor backup restore, tests
	// rebuilding state from a stream) shouldn't have to know about the contract.
	// Save/restore stack semantics make the redundant push harmless under nesting.
	Zenith_Scene xSelf;
	xSelf.m_iHandle = m_iHandle;
	xSelf.m_uGeneration = m_uGeneration;
	Zenith_SceneCreationTargetScope xCreationTargetScope(xSelf);

	// Single source of truth for header checks. ValidateSceneStream is
	// non-destructive (restores the cursor to its entry offset), so after it
	// returns we still consume the 8-byte header here to advance the cursor and
	// populate uVersion for ReadEntityFromDataStream below. The magic was already
	// validated inside ValidateSceneStream, so we skip past it rather than re-read.
	if (!ValidateSceneStream(xStream))
	{
		return false;
	}

	xStream.SkipBytes(sizeof(u_int));  // magic — already validated above
	u_int uVersion;
	xStream >> uVersion;

	u_int uNumEntities;
	xStream >> uNumEntities;

	// Wave9.1 (a) guard 1: bounded sanity on the entity count BEFORE we start
	// allocating. A corrupt count (e.g. 0xFFFFFFFF from a truncated/garbled file)
	// would otherwise spin ~4 billion iterations building junk entities into a
	// half-loaded world (in SINGLE mode the old world is already torn down). The
	// bound is conservative — minimum 1 byte per entity — so a count that exceeds
	// the raw bytes still remaining in the stream is provably corrupt and can never
	// reject a valid scene. Returning false here re-activates the existing rollback
	// in Zenith_SceneSystem_Operations.cpp (UnloadSceneForced on !bDeserialised).
	const uint64_t ulRemaining = xStream.GetSize() - xStream.GetCursor();
	if (uNumEntities > ulRemaining)
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "Malformed scene body: entity count %u exceeds remaining %llu bytes",
			uNumEntities, (unsigned long long)ulRemaining);
		return false;
	}

	Zenith_HashMap<uint32_t, Zenith_EntityID> xFileIndexToNewID;
	xFileIndexToNewID.Reserve(uNumEntities);

	for (u_int u = 0; u < uNumEntities; u++)
	{
		// Wave9.1 (a) guard 2: detect a stalled cursor. A malformed entity record
		// that consumes no bytes would loop forever (or re-read the same garbage);
		// if the cursor did not advance AND there are still more entities claimed,
		// the body is corrupt. The u+1<uNumEntities gate avoids falsely rejecting a
		// final, well-formed entity that legitimately ends exactly at EOF.
		const uint64_t ulBefore = xStream.GetCursor();
		ReadEntityFromDataStream(xStream, uVersion, xFileIndexToNewID);
		if (xStream.GetCursor() <= ulBefore && u + 1 < uNumEntities)
		{
			Zenith_Error(LOG_CATEGORY_SCENE, "Malformed scene body: no read progress at entity %u/%u", u, uNumEntities);
			return false;
		}
	}

	// Rebuild hierarchy. Phase 5a: read the SLOT pending-parent (single source of
	// truth), map file-index -> EntityID, call the slot-based SetParent, then clear
	// the slot pending-parent. Iterating m_xActiveEntities walks entities in file
	// order (they were created in that order by ReadEntityFromDataStream), and
	// SetParent appends to each parent's slot child list in PushBack order — so the
	// rebuilt child lists match pre-split saves' iteration order exactly. The child
	// lists themselves are NOT serialized; they are reconstructed solely from the
	// parent links here.
	for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xID = m_xActiveEntities.Get(u);
		Zenith_Entity xEntity = GetEntity(xID);

		uint32_t uParentFileIndex = xEntity.GetPendingParentFileIndex();
		xEntity.ClearPendingParentFileIndex();

		if (uParentFileIndex != Zenith_EntityID::INVALID_INDEX)
		{
			const Zenith_EntityID* pxParentID = xFileIndexToNewID.TryGet(uParentFileIndex);
			if (pxParentID != nullptr && EntityExists(*pxParentID))
			{
				xEntity.SetParent(*pxParentID);
			}
		}
	}

	// Wave9.1 (a) guard 3: the trailing main-camera index is mandatory. If the
	// stream is truncated before it, the body is malformed — bail (re-activating
	// the Operations.cpp rollback) rather than letting operator>> clamp/no-op and
	// silently leaving the camera index garbage.
	if (xStream.GetCursor() + sizeof(uint32_t) > xStream.GetSize())
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "Malformed scene body: truncated before camera index");
		return false;
	}

	// Read main camera
	uint32_t uMainCameraFileIndex;
	xStream >> uMainCameraFileIndex;

	if (uMainCameraFileIndex != Zenith_EntityID::INVALID_INDEX)
	{
		const Zenith_EntityID* pxCameraID = xFileIndexToNewID.TryGet(uMainCameraFileIndex);
		if (pxCameraID != nullptr && EntityExists(*pxCameraID))
		{
			m_xMainCameraEntity = *pxCameraID;
		}
	}

	// NOTE: Scene stays in SCENE_STATE_LOADING here. The caller (LoadScene)
	// transitions to LOADED only after Awake/OnEnable dispatch, so
	// IsActivated() correctly reports false while lifecycle is running.
	ClearDirty();
	return true;
}
