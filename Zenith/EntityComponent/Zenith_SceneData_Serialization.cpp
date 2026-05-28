#include "Zenith.h"
// Scene save/load split out of Zenith_SceneData.cpp so the god-file stays
// focused on lifecycle + dispatch. All serialization logic for
// Zenith_SceneData lives here: SaveToFile, LoadFromFile, LoadFromDataStream,
// ValidateFileHeader, ReadEntityFromDataStream.
//
// (Originally one of several peer-split files alongside the Phase A
// pre-extraction Zenith_SceneManager_*.cpp set; those siblings have since
// been replaced by the EntityComponent/Internal/ subsystem TUs.)

#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"

void Zenith_SceneData::SaveToFile(const std::string& strFilename, bool bIncludeTransient)
{
	Zenith_DataStream xStream;

	xStream << uSCENE_MAGIC;
	xStream << uSCENE_VERSION_CURRENT;

	u_int uNumEntities = 0;
	for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xID = m_xActiveEntities.Get(u);
		const Zenith_EntitySlot& xSlot = g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex);
		if (bIncludeTransient || !xSlot.m_bTransient)
		{
			uNumEntities++;
		}
	}
	xStream << uNumEntities;

	for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xID = m_xActiveEntities.Get(u);
		const Zenith_EntitySlot& xSlot = g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex);
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
		const Zenith_EntitySlot& xCameraSlot = g_xEngine.EntityStore().m_axEntitySlots.Get(m_xMainCameraEntity.m_uIndex);
		if (bIncludeTransient || !xCameraSlot.m_bTransient)
		{
			uMainCameraIndex = m_xMainCameraEntity.m_uIndex;
		}
	}
	xStream << uMainCameraIndex;

	xStream.WriteToFile(strFilename.c_str());

	ClearDirty();
}

bool Zenith_SceneData::ValidateFileHeader(const std::string& strFilename)
{
	if (!Zenith_FileAccess::FileExists(strFilename.c_str()))
	{
		return false;
	}

	// Peek only the 8-byte header (magic + version) — avoids a full-file
	// allocation + read just to reject corrupt / unsupported files.
	u_int auHeader[2] = { 0, 0 };
	if (!Zenith_FileAccess::ReadPrefix(strFilename.c_str(), auHeader, sizeof(auHeader)))
	{
		return false;
	}

	const u_int uMagicNumber = auHeader[0];
	const u_int uVersion = auHeader[1];

	if (uMagicNumber != uSCENE_MAGIC)
	{
		return false;
	}
	if (uVersion > uSCENE_VERSION_CURRENT || uVersion < uSCENE_VERSION_MIN_SUPPORTED)
	{
		return false;
	}
	return true;
}

bool Zenith_SceneData::LoadFromFile(const std::string& strFilename)
{
	// Note: Flux render systems and Physics reset are now handled by
	// g_xEngine.Scenes().LoadScene() for SINGLE mode loads only.
	// This allows LoadFromFile to be used for ADDITIVE loads without
	// destroying render data from other loaded scenes.
	//
	// B.8: gatekeep LoadFromFile so external callers can't silently nuke a loaded
	// scene's entities. SceneManager always drives this against a fresh SceneData
	// from CreateEmptyScene (entity count = 0). Any non-SceneManager caller that
	// reaches this with live entities is almost certainly bypassing the lifecycle
	// (no SceneUnloading/SceneUnloaded callbacks, no active-scene handling, no
	// physics reset) and should route via g_xEngine.Scenes().LoadScene instead.
	Zenith_Assert(m_xActiveEntities.GetSize() == 0,
		"Zenith_SceneData::LoadFromFile: scene is not empty (entityCount=%u). "
		"Call g_xEngine.Scenes().LoadScene / LoadSceneAsync to route through the "
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
	// g_xEngine.Scenes().LoadScene() after this method returns.
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
	std::unordered_map<uint32_t, Zenith_EntityID>& xFileIndexToNewID) // #TODO: Replace with engine hash map
{
	uint32_t uFileIndex;
	std::string strName;
	uint32_t uFileParentIndex = Zenith_EntityID::INVALID_INDEX;

	if (uVersion == 3)
	{
		xStream >> uFileIndex;
		xStream >> uFileParentIndex;
		xStream >> strName;

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
	else // v4 and v5 share the same entity format
	{
		xStream >> uFileIndex;
		xStream >> strName;
	}

	Zenith_EntityID xNewID = CreateEntity();
	xFileIndexToNewID[uFileIndex] = xNewID;

	Zenith_EntitySlot& xSlot = g_xEngine.EntityStore().m_axEntitySlots.Get(xNewID.m_uIndex);
	xSlot.m_strName = strName;
	xSlot.m_bEnabled = true;
	xSlot.m_bTransient = false;

	Zenith_Entity xEntity(this, xNewID);
	xEntity.AddComponent<Zenith_TransformComponent>();

	Zenith_ComponentMetaRegistry::Get().DeserializeEntityComponents(xEntity, xStream);

	if (uVersion == 3 && uFileParentIndex != Zenith_EntityID::INVALID_INDEX)
	{
		Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPendingParentFileIndex(uFileParentIndex);
	}

	return xNewID;
}

bool Zenith_SceneData::LoadFromDataStream(Zenith_DataStream& xStream)
{
	// C9: see CreateEntity for rationale — render-task-ordering invariant.
	// LoadFromDataStream creates entities + components en masse; if render
	// tasks are actively reading, those reads would see half-populated scenes.
	Zenith_Assert(!g_xEngine.Scenes().AreRenderTasksActive(),
		"LoadFromDataStream: scene mutation while render tasks are reading — render-task invariant violated");
	// B.8: same gate as LoadFromFile. Deserializing over a populated scene would
	// blindly append new entities without any OnDestroy/SceneUnloading/etc. fires
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

	// Validate stream has minimum header data (magic + version = 8 bytes)
	static constexpr uint64_t ulMIN_HEADER_SIZE = sizeof(u_int) * 2;
	if (xStream.GetSize() < ulMIN_HEADER_SIZE)
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "Malformed scene file: too small (size=%llu, minimum=%llu)",
			xStream.GetSize(), ulMIN_HEADER_SIZE);
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
		return false;
	}

	if (uVersion > uSCENE_VERSION_CURRENT || uVersion < uSCENE_VERSION_MIN_SUPPORTED)
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "Unsupported scene file version %u", uVersion);
		return false;
	}

	u_int uNumEntities;
	xStream >> uNumEntities;

	std::unordered_map<uint32_t, Zenith_EntityID> xFileIndexToNewID; // #TODO: Replace with engine hash map
	xFileIndexToNewID.reserve(uNumEntities);

	for (u_int u = 0; u < uNumEntities; u++)
		ReadEntityFromDataStream(xStream, uVersion, xFileIndexToNewID);

	// Rebuild hierarchy
	for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xID = m_xActiveEntities.Get(u);
		Zenith_Entity xEntity = GetEntity(xID);
		Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();

		uint32_t uParentFileIndex = xTransform.GetPendingParentFileIndex();
		xTransform.ClearPendingParentFileIndex();

		if (uParentFileIndex != Zenith_EntityID::INVALID_INDEX)
		{
			auto it = xFileIndexToNewID.find(uParentFileIndex);
			if (it != xFileIndexToNewID.end() && EntityExists(it->second))
			{
				xTransform.SetParentByID(it->second);
			}
		}
	}

	// Read main camera
	uint32_t uMainCameraFileIndex;
	xStream >> uMainCameraFileIndex;

	if (uMainCameraFileIndex != Zenith_EntityID::INVALID_INDEX)
	{
		auto it = xFileIndexToNewID.find(uMainCameraFileIndex);
		if (it != xFileIndexToNewID.end() && EntityExists(it->second))
		{
			m_xMainCameraEntity = it->second;
		}
	}

	// NOTE: Scene stays in SCENE_STATE_LOADING here. The caller (LoadScene)
	// transitions to LOADED only after Awake/OnEnable dispatch, so
	// IsActivated() correctly reports false while lifecycle is running.
	ClearDirty();
	return true;
}
