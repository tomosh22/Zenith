#include "Zenith.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "ZenithECS/Zenith_ComponentMeta.h"

namespace
{
	// Current ScriptComponent serialization version.
	//   v1 = single behaviour (legacy, no length framing).
	//   v2 = multi-slot, no per-slot/per-blob length prefix (deprecated - misalignment risk).
	//   v3 = multi-slot with totalSlotsBlobBytes prefix + per-slot paramBytes prefix.
	//        Lets readers skip an unknown-version blob and skip individual orphan slots
	//        without misaligning the surrounding component stream.
	constexpr uint32_t uSCRIPT_COMPONENT_VERSION = 3;
}

//------------------------------------------------------------------------------
// Internal helpers
//------------------------------------------------------------------------------

void Zenith_ScriptComponent::AppendSlotInternal(const std::string& strAssetPath, Zenith_ScriptBehaviour* pxBehaviour)
{
	Zenith_ScriptSlot xSlot;
	xSlot.m_strScriptAssetPath = Zenith_AssetRegistry::NormalizeAssetPath(strAssetPath);
	xSlot.m_pxBehaviour = pxBehaviour;
	if (pxBehaviour)
	{
		xSlot.m_strBehaviourTypeName = pxBehaviour->GetBehaviourTypeName();
	}
	m_axSlots.PushBack(std::move(xSlot));
}

void Zenith_ScriptComponent::AppendUnresolvedSlotInternal(const std::string& strAssetPath,
                                                          const std::string& strBehaviourTypeName,
                                                          Zenith_DataStream&& xPendingParams)
{
	// Preserves an attachment whose C++ behaviour isn't available in this build, so a
	// later save round-trips the slot's asset path, type name, and serialized params
	// verbatim. See P2.1 in the rework review.
	Zenith_ScriptSlot xSlot;
	xSlot.m_strScriptAssetPath = Zenith_AssetRegistry::NormalizeAssetPath(strAssetPath);
	xSlot.m_strBehaviourTypeName = strBehaviourTypeName;
	xSlot.m_xPendingParams = std::move(xPendingParams);
	xSlot.m_pxBehaviour = nullptr;
	m_axSlots.PushBack(std::move(xSlot));
}

void Zenith_ScriptComponent::MarkParentAwokenIfValid()
{
	if (m_xParentEntity.IsValid())
	{
		Zenith_SceneData* pxSceneData = m_xParentEntity.GetSceneData();
		if (pxSceneData)
		{
			pxSceneData->MarkEntityAwoken(m_xParentEntity.GetEntityID());
		}
	}
}

void Zenith_ScriptComponent::FlushPendingRemovals()
{
	// Walk in reverse so removed indices don't shift remaining indices.
	for (uint32_t u = m_axSlots.GetSize(); u > 0; --u)
	{
		const uint32_t uIndex = u - 1;
		Zenith_ScriptSlot& xSlot = m_axSlots.Get(uIndex);
		if (!xSlot.m_bMarkedForRemoval)
		{
			continue;
		}
		if (xSlot.m_pxBehaviour)
		{
			xSlot.m_pxBehaviour->OnDestroy();
		}
		m_axSlots.Remove(uIndex);  // slot dtor deletes behaviour
	}
}

//------------------------------------------------------------------------------
// Public asset-path-based attach
//------------------------------------------------------------------------------

// Resolve "game:Scripts/<TypeName>.zscript" -> "<TypeName>".
// Used as fallback when the .zscript file isn't on disk (e.g. in tests, or before
// SyncRegisteredTypesToDisk has run). Returns empty string if path doesn't match the
// expected pattern.
static std::string ExtractBehaviourTypeNameFromAssetPath(const char* szAssetPath)
{
	if (!szAssetPath)
	{
		return std::string();
	}
	std::string strPath(szAssetPath);
	const size_t uSlash = strPath.find_last_of("/\\");
	std::string strFile = (uSlash == std::string::npos) ? strPath : strPath.substr(uSlash + 1);
	const size_t uDot = strFile.find_last_of('.');
	if (uDot != std::string::npos)
	{
		strFile = strFile.substr(0, uDot);
	}
	return strFile;
}

// Tries asset registry first. The filename-derived fallback only runs when the asset
// CANNOT BE LOADED AT ALL (file missing) - not when it loads but has an unregistered
// behaviour. Falling back on a loaded-but-unregistered asset would silently attach a
// behaviour with a different name from what the .zscript file declares (e.g. a renamed
// or stale asset pointing at a now-different C++ class). See P2.3 in the rework review.
static Zenith_ScriptBehaviour* CreateBehaviourFromAssetPath(const char* szAssetPath, Zenith_Entity& xEntity)
{
	Zenith_ScriptAsset* pxAsset = Zenith_AssetRegistry::Get<Zenith_ScriptAsset>(szAssetPath);
	if (pxAsset)
	{
		// Asset loaded - trust its declared behaviour. If the C++ behaviour is
		// unregistered in this build, fail rather than guess from the filename.
		if (pxAsset->IsRegistered())
		{
			return pxAsset->CreateInstance(xEntity);
		}
		Zenith_Log(LOG_CATEGORY_ECS,
			"Zenith_ScriptComponent: asset '%s' loaded but its declared behaviour '%s' is not registered in this build; refusing to fall back to filename derivation",
			szAssetPath, pxAsset->GetBehaviourTypeName());
		return nullptr;
	}

	// Asset failed to load (typically because the .zscript file does not exist on
	// disk yet - tests, or before SyncRegisteredTypesToDisk has run). Try the
	// filename-derived type name as a last resort.
	const std::string strTypeName = ExtractBehaviourTypeNameFromAssetPath(szAssetPath);
	Zenith_ScriptBehaviourFactoryFn pfnFactory = Zenith_ScriptAsset::LookupFactory(strTypeName.c_str());
	if (pfnFactory)
	{
		return pfnFactory(xEntity);
	}

	Zenith_Log(LOG_CATEGORY_ECS,
		"Zenith_ScriptComponent: failed to resolve script asset '%s' (no on-disk asset and no registered factory for '%s')",
		szAssetPath, strTypeName.c_str());
	return nullptr;
}

Zenith_ScriptBehaviour* Zenith_ScriptComponent::AddScriptByAssetPath(const char* szAssetPath)
{
	if (!szAssetPath)
	{
		return nullptr;
	}

	Zenith_ScriptBehaviour* pxBehaviour = CreateBehaviourFromAssetPath(szAssetPath, m_xParentEntity);
	if (!pxBehaviour)
	{
		return nullptr;
	}
	pxBehaviour->m_xParentEntity = m_xParentEntity;
	AppendSlotInternal(szAssetPath, pxBehaviour);

	pxBehaviour->OnAwake();
	MarkParentAwokenIfValid();
	return pxBehaviour;
}

Zenith_ScriptBehaviour* Zenith_ScriptComponent::AddScriptForSerializationByAssetPath(const char* szAssetPath)
{
	if (!szAssetPath)
	{
		return nullptr;
	}

	Zenith_ScriptBehaviour* pxBehaviour = CreateBehaviourFromAssetPath(szAssetPath, m_xParentEntity);
	if (!pxBehaviour)
	{
		return nullptr;
	}
	pxBehaviour->m_xParentEntity = m_xParentEntity;
	AppendSlotInternal(szAssetPath, pxBehaviour);
	// OnAwake intentionally NOT called - lifecycle deferred to Play mode entry / scene load
	return pxBehaviour;
}

//------------------------------------------------------------------------------
// Removal
//------------------------------------------------------------------------------

void Zenith_ScriptComponent::RemoveScriptAt(uint32_t uIndex)
{
	if (uIndex >= m_axSlots.GetSize())
	{
		return;
	}

	// If a lifecycle dispatch is in progress on ANY ScriptComponent, the snapshot may still
	// hold raw pointers to behaviours that haven't been called yet. Deleting one here would
	// leave a stale pointer in the snapshot and lead to use-after-free on the next iteration.
	// Mark the slot for removal instead; the dispatch's post-loop flush processes it once
	// the dispatch unwinds. See P2.2 in the rework review.
	Zenith_ScriptSlot& xSlot = m_axSlots.Get(uIndex);
	if (xSlot.m_bMarkedForRemoval)
	{
		return;
	}
	if (IsDispatchInProgress())
	{
		xSlot.m_bMarkedForRemoval = true;
		return;
	}

	if (xSlot.m_pxBehaviour)
	{
		xSlot.m_pxBehaviour->OnDestroy();
	}
	m_axSlots.Remove(uIndex);  // slot destructor deletes behaviour
}

void Zenith_ScriptComponent::RemoveAllScripts()
{
	// OnDestroy in REVERSE order (Unity convention: last-attached destroyed first).
	// Snapshot first - OnDestroy may mutate the pool / our slots; see SnapshotBehaviours
	// commentary in the lifecycle dispatch block above.
	Zenith_Vector<Zenith_ScriptBehaviour*> axBehaviours;
	for (uint32_t u = 0; u < m_axSlots.GetSize(); ++u)
	{
		Zenith_ScriptBehaviour* pxBehaviour = m_axSlots.Get(u).m_pxBehaviour;
		if (pxBehaviour)
		{
			axBehaviours.PushBack(pxBehaviour);
		}
	}
	for (uint32_t u = axBehaviours.GetSize(); u > 0; --u)
	{
		axBehaviours.Get(u - 1)->OnDestroy();
	}
	m_axSlots.Clear();
}

//------------------------------------------------------------------------------
// Lifecycle dispatch
//
// Each hook snapshots its slots' behaviour pointers into a stack-local vector
// BEFORE invoking any user code. The dispatched behaviour can spawn entities
// (via AddComponent / scene-create) which may resize the ScriptComponent pool
// and invalidate `this` underneath us. The snapshot keeps iteration safe:
// behaviour instances live on the heap, so the pointers remain valid even if
// our slot vector relocates.
//------------------------------------------------------------------------------

namespace
{
	// File-scope dispatch depth. Non-zero while a Zenith_ScriptComponent lifecycle method
	// is mid-iteration. Lives outside the component because the component instance can be
	// relocated by a pool resize triggered by a callback - touching `this->m_iDispatchDepth`
	// after the loop would be UB. The static is single-threaded-engine-safe; switch to
	// thread_local if dispatch ever runs from worker threads.
	int32_t s_iDispatchDepth = 0;

	// Snapshot non-null behaviour pointers from a slot vector. Caller iterates
	// the snapshot, so subsequent mutations to m_axSlots (or `this` itself)
	// during dispatch do not corrupt the loop. Slots already marked for removal
	// are skipped so a callback that marks a sibling doesn't see it called.
	void SnapshotBehaviours(const Zenith_Vector<Zenith_ScriptSlot>& axSlots,
	                        Zenith_Vector<Zenith_ScriptBehaviour*>& axOut)
	{
		const uint32_t uCount = axSlots.GetSize();
		for (uint32_t u = 0; u < uCount; ++u)
		{
			const Zenith_ScriptSlot& xSlot = axSlots.Get(u);
			if (xSlot.m_bMarkedForRemoval)
			{
				continue;
			}
			if (xSlot.m_pxBehaviour)
			{
				axOut.PushBack(xSlot.m_pxBehaviour);
			}
		}
	}

	// Re-fetch the ScriptComponent through the entity handle and flush any slots marked
	// for removal during dispatch. Using the entity handle (rather than `this`) is safe
	// even if a callback caused a component-pool resize that relocated the component:
	// the entity ID is stable, GetComponent returns the new address.
	void FlushPendingRemovalsViaEntity(Zenith_Entity xParent)
	{
		if (!xParent.IsValid())
		{
			return;
		}
		if (!xParent.HasComponent<Zenith_ScriptComponent>())
		{
			return;
		}
		xParent.GetComponent<Zenith_ScriptComponent>().FlushPendingRemovals();
	}
}

bool Zenith_ScriptComponent::IsDispatchInProgress()
{
	return s_iDispatchDepth > 0;
}

// Common dispatch envelope: snapshot, increment file-scope depth, run callback for each
// snapshot entry, decrement depth, then re-fetch the component via the (stable) entity
// handle to flush pending removals. The post-loop work avoids touching `this` because
// callbacks may have caused a component-pool resize that relocated us.
//
// `pfn` is invoked once per non-marked, non-null behaviour pointer captured before the loop.

template<typename TFunc>
static void DispatchSnapshot(Zenith_Vector<Zenith_ScriptSlot>& axSlots,
                             Zenith_Entity xParent,
                             bool bReverseOrder,
                             TFunc&& pfn)
{
	Zenith_Vector<Zenith_ScriptBehaviour*> axBehaviours;
	SnapshotBehaviours(axSlots, axBehaviours);

	++s_iDispatchDepth;
	const uint32_t uCount = axBehaviours.GetSize();
	if (bReverseOrder)
	{
		for (uint32_t u = uCount; u > 0; --u)
		{
			pfn(axBehaviours.Get(u - 1));
		}
	}
	else
	{
		for (uint32_t u = 0; u < uCount; ++u)
		{
			pfn(axBehaviours.Get(u));
		}
	}
	--s_iDispatchDepth;

	// Only the outermost dispatch frame flushes - nested dispatch (a callback that calls
	// another OnXxx) lets the outermost owner do the cleanup.
	if (s_iDispatchDepth == 0)
	{
		FlushPendingRemovalsViaEntity(xParent);
	}
}

void Zenith_ScriptComponent::OnAwake()
{
	DispatchSnapshot(m_axSlots, m_xParentEntity, false,
		[](Zenith_ScriptBehaviour* px) { px->OnAwake(); });
}

void Zenith_ScriptComponent::OnStart()
{
	DispatchSnapshot(m_axSlots, m_xParentEntity, false,
		[](Zenith_ScriptBehaviour* px) { px->OnStart(); });
}

void Zenith_ScriptComponent::OnEnable()
{
	DispatchSnapshot(m_axSlots, m_xParentEntity, false,
		[](Zenith_ScriptBehaviour* px) { px->OnEnable(); });
}

void Zenith_ScriptComponent::OnDisable()
{
	DispatchSnapshot(m_axSlots, m_xParentEntity, false,
		[](Zenith_ScriptBehaviour* px) { px->OnDisable(); });
}

void Zenith_ScriptComponent::OnUpdate(float fDt)
{
	DispatchSnapshot(m_axSlots, m_xParentEntity, false,
		[fDt](Zenith_ScriptBehaviour* px) { px->OnUpdate(fDt); });
}

void Zenith_ScriptComponent::OnFixedUpdate(float fDt)
{
	DispatchSnapshot(m_axSlots, m_xParentEntity, false,
		[fDt](Zenith_ScriptBehaviour* px) { px->OnFixedUpdate(fDt); });
}

void Zenith_ScriptComponent::OnLateUpdate(float fDt)
{
	DispatchSnapshot(m_axSlots, m_xParentEntity, false,
		[fDt](Zenith_ScriptBehaviour* px) { px->OnLateUpdate(fDt); });
}

void Zenith_ScriptComponent::OnDestroy()
{
	// REVERSE order - Unity convention: last-attached destroyed first.
	DispatchSnapshot(m_axSlots, m_xParentEntity, true,
		[](Zenith_ScriptBehaviour* px) { px->OnDestroy(); });
}

void Zenith_ScriptComponent::OnCollisionEnter(Zenith_Entity xOther)
{
	DispatchSnapshot(m_axSlots, m_xParentEntity, false,
		[xOther](Zenith_ScriptBehaviour* px) { px->OnCollisionEnter(xOther); });
}

void Zenith_ScriptComponent::OnCollisionStay(Zenith_Entity xOther)
{
	DispatchSnapshot(m_axSlots, m_xParentEntity, false,
		[xOther](Zenith_ScriptBehaviour* px) { px->OnCollisionStay(xOther); });
}

void Zenith_ScriptComponent::OnCollisionExit(Zenith_EntityID uOtherID)
{
	DispatchSnapshot(m_axSlots, m_xParentEntity, false,
		[uOtherID](Zenith_ScriptBehaviour* px) { px->OnCollisionExit(uOtherID); });
}

//------------------------------------------------------------------------------
// Serialization (multi-slot, version 2)
//------------------------------------------------------------------------------

void Zenith_ScriptComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << uSCRIPT_COMPONENT_VERSION;

	// Reserve space for the total slots-blob byte count, backpatched after writing slots.
	// This lets readers of an unsupported version skip the entire blob without misalignment.
	const uint64_t ulBlobSizeFieldCursor = xStream.GetCursor();
	uint32_t uPlaceholder = 0;
	xStream << uPlaceholder;
	const uint64_t ulBlobStartCursor = xStream.GetCursor();

	// Skip slots already marked for removal - they shouldn't survive a save.
	uint32_t uWriteCount = 0;
	for (uint32_t u = 0; u < m_axSlots.GetSize(); ++u)
	{
		if (!m_axSlots.Get(u).m_bMarkedForRemoval)
		{
			++uWriteCount;
		}
	}
	xStream << uWriteCount;

	for (uint32_t u = 0; u < m_axSlots.GetSize(); ++u)
	{
		const Zenith_ScriptSlot& xSlot = m_axSlots.Get(u);
		if (xSlot.m_bMarkedForRemoval)
		{
			continue;
		}

		std::string strPath = xSlot.m_strScriptAssetPath;
		// Prefer the live behaviour's reported type name; for unresolved slots fall back
		// to the saved name so we don't lose attribution across a save round-trip.
		std::string strTypeName = xSlot.m_pxBehaviour
			? std::string(xSlot.m_pxBehaviour->GetBehaviourTypeName())
			: xSlot.m_strBehaviourTypeName;

		xStream << strPath;
		xStream << strTypeName;

		// Reserve space for this slot's parameter byte count, backpatched after the params.
		// On read, an orphan slot (no factory) is preserved as unresolved with the raw bytes
		// stashed in m_axPendingParams.
		const uint64_t ulParamSizeFieldCursor = xStream.GetCursor();
		uint32_t uParamPlaceholder = 0;
		xStream << uParamPlaceholder;
		const uint64_t ulParamStartCursor = xStream.GetCursor();

		if (xSlot.m_pxBehaviour)
		{
			xSlot.m_pxBehaviour->WriteParametersToDataStream(xStream);
		}
		else
		{
			// Unresolved slot - emit the verbatim bytes we held from the previous load,
			// so a later build with the C++ behaviour available can resurrect this slot
			// with its original parameters intact (P2.1 in the rework review). The cursor
			// of m_xPendingParams marks the populated byte count (it's not bound to the
			// stream's capacity).
			const uint64_t ulPendingBytes = xSlot.m_xPendingParams.GetCursor();
			if (ulPendingBytes > 0)
			{
				xStream.WriteData(xSlot.m_xPendingParams.GetData(), ulPendingBytes);
			}
		}

		const uint64_t ulParamEndCursor = xStream.GetCursor();
		const uint32_t uParamBytes = static_cast<uint32_t>(ulParamEndCursor - ulParamStartCursor);
		xStream.SetCursor(ulParamSizeFieldCursor);
		xStream << uParamBytes;
		xStream.SetCursor(ulParamEndCursor);
	}

	const uint64_t ulBlobEndCursor = xStream.GetCursor();
	const uint32_t uBlobBytes = static_cast<uint32_t>(ulBlobEndCursor - ulBlobStartCursor);
	xStream.SetCursor(ulBlobSizeFieldCursor);
	xStream << uBlobBytes;
	xStream.SetCursor(ulBlobEndCursor);
}

void Zenith_ScriptComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	uint32_t uVersion = 0;
	xStream >> uVersion;

	uint32_t uBlobBytes = 0;
	xStream >> uBlobBytes;

	const uint64_t ulBlobStart = xStream.GetCursor();
	const uint64_t ulBlobEnd = ulBlobStart + uBlobBytes;

	if (uVersion != uSCRIPT_COMPONENT_VERSION)
	{
		Zenith_Log(LOG_CATEGORY_ECS,
			"Zenith_ScriptComponent: unsupported serialization version %u (expected %u). Skipping %u-byte slot blob; scene needs regeneration via EditorAutomation.",
			uVersion, uSCRIPT_COMPONENT_VERSION, uBlobBytes);
		xStream.SkipBytes(static_cast<u_int>(uBlobBytes));
		return;
	}

	uint32_t uCount = 0;
	xStream >> uCount;

	for (uint32_t u = 0; u < uCount; ++u)
	{
		std::string strPath;
		std::string strTypeName;
		xStream >> strPath;
		xStream >> strTypeName;

		uint32_t uParamBytes = 0;
		xStream >> uParamBytes;
		const uint64_t ulParamStart = xStream.GetCursor();
		const uint64_t ulParamEnd = ulParamStart + uParamBytes;

		// Resolution order:
		//   1. Asset registry by path (if asset loads, trust its declared behaviour)
		//   2. Direct factory lookup by saved type name (covers asset-not-on-disk-yet scenarios)
		// Per P2.3, we do NOT fall back to filename derivation when the asset loads but its
		// declared behaviour is unregistered - that path could silently swap in a different
		// C++ class than the .zscript declares.
		Zenith_ScriptBehaviour* pxBehaviour = nullptr;
		Zenith_ScriptAsset* pxAsset = nullptr;

		if (!strPath.empty())
		{
			pxAsset = Zenith_AssetRegistry::Get<Zenith_ScriptAsset>(strPath);
			if (pxAsset && pxAsset->IsRegistered())
			{
				pxBehaviour = pxAsset->CreateInstance(m_xParentEntity);
			}
			// pxAsset != null && !IsRegistered: stale/orphan asset - fall through to type-name
			// fallback only if the saved type name differs from the asset's declared name (which
			// would indicate the scene was saved before the asset was renamed). Conservatively,
			// we still try the type-name fallback because the saved type name is the one the
			// user authored against; if it's also unregistered, we end up unresolved.
		}

		if (!pxBehaviour && !strTypeName.empty())
		{
			// Use the saved type name (from the scene blob), not a filename-derived name.
			Zenith_ScriptBehaviourFactoryFn pfnFactory = Zenith_ScriptAsset::LookupFactory(strTypeName.c_str());
			if (pfnFactory)
			{
				pxBehaviour = pfnFactory(m_xParentEntity);
				Zenith_Log(LOG_CATEGORY_ECS,
					"Zenith_ScriptComponent: asset '%s' missing/orphan; instantiated '%s' from registered factory directly",
					strPath.c_str(), strTypeName.c_str());
			}
		}

		if (!pxBehaviour)
		{
			// Preserve the unresolved slot verbatim. P2.1: do NOT silently drop scripts
			// whose C++ behaviour is missing in this build - opening and resaving a scene
			// must round-trip the slot's path, type, and serialized params unchanged.
			Zenith_Log(LOG_CATEGORY_ECS,
				"Zenith_ScriptComponent: slot %u unresolved (asset='%s', type='%s'); preserving %u param bytes for round-trip",
				u, strPath.c_str(), strTypeName.c_str(), uParamBytes);

			Zenith_DataStream xPendingBytes;
			if (uParamBytes > 0)
			{
				// Copy the verbatim bytes from the source stream into our preserved store via
				// WriteData (which grows as needed and advances xPendingBytes' cursor to mark
				// the populated extent). Read directly from the source stream's buffer to
				// avoid a scratch allocation.
				const void* pxSrc = static_cast<const uint8_t*>(xStream.GetData()) + xStream.GetCursor();
				xPendingBytes.WriteData(pxSrc, uParamBytes);
				xStream.SkipBytes(static_cast<u_int>(uParamBytes));
			}
			AppendUnresolvedSlotInternal(strPath, strTypeName, std::move(xPendingBytes));
			continue;
		}

		pxBehaviour->m_xParentEntity = m_xParentEntity;
		pxBehaviour->ReadParametersFromDataStream(xStream);

		// Defensive: clamp cursor to expected end of params if the behaviour reader under-/over-read.
		// Param framing exists precisely so a buggy ReadParametersFromDataStream cannot corrupt the
		// surrounding stream.
		if (xStream.GetCursor() != ulParamEnd)
		{
			Zenith_Log(LOG_CATEGORY_ECS,
				"Zenith_ScriptComponent: behaviour '%s' read %lld bytes, expected %u; clamping cursor",
				pxBehaviour->GetBehaviourTypeName(),
				static_cast<long long>(xStream.GetCursor() - ulParamStart),
				uParamBytes);
			xStream.SetCursor(ulParamEnd);
		}

		AppendSlotInternal(strPath, pxBehaviour);

		// OnAwake is NOT called during deserialization - lifecycle dispatched on first frame
	}

	// Final guard: ensure cursor is exactly at the documented blob end. Misalignment here would
	// corrupt the next component or entity.
	if (xStream.GetCursor() != ulBlobEnd)
	{
		Zenith_Log(LOG_CATEGORY_ECS,
			"Zenith_ScriptComponent: read cursor %llu doesn't match expected blob end %llu; clamping",
			static_cast<unsigned long long>(xStream.GetCursor()),
			static_cast<unsigned long long>(ulBlobEnd));
		xStream.SetCursor(ulBlobEnd);
	}
}

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "Editor/Zenith_Editor.h"  // For DRAGDROP_PAYLOAD_SCRIPT_ASSET
#include <algorithm>
#include <cstdio>

namespace
{
	// Extract the basename without extension from an asset path
	std::string AssetPathBasename(const std::string& strPath)
	{
		size_t uSlash = strPath.find_last_of("/\\");
		std::string strFile = (uSlash == std::string::npos) ? strPath : strPath.substr(uSlash + 1);
		size_t uDot = strFile.find_last_of('.');
		if (uDot != std::string::npos)
		{
			strFile = strFile.substr(0, uDot);
		}
		return strFile;
	}
}

// Render one attached script slot. Sets m_iPendingRemoveIndex if the user
// clicks the X button so the actual removal happens next frame (avoids
// invalidating the loop iterator).
void Zenith_ScriptComponent::RenderScriptSlot(uint32_t uIndex, const Zenith_ScriptSlot& xSlot)
{
	ImGui::PushID(static_cast<int>(uIndex));

	const std::string strBasename = AssetPathBasename(xSlot.m_strScriptAssetPath);
	const char* szTypeName = xSlot.m_pxBehaviour->GetBehaviourTypeName();

	char acHeader[256];
	std::snprintf(acHeader, sizeof(acHeader), "%s - %s",
		strBasename.empty() ? "(no asset)" : strBasename.c_str(),
		szTypeName ? szTypeName : "?");

	const bool bOpen = ImGui::TreeNodeEx(acHeader, ImGuiTreeNodeFlags_DefaultOpen);

	// X button on the right edge to remove this slot
	ImGui::SameLine(ImGui::GetWindowWidth() - 40.0f);
	if (ImGui::SmallButton("X"))
	{
		m_iPendingRemoveIndex = static_cast<int32_t>(uIndex);
	}

	if (bOpen)
	{
		if (xSlot.m_pxBehaviour->m_axGUIDRefs.GetSize() > 0)
		{
			ImGui::Text("GUID References: %zu", static_cast<size_t>(xSlot.m_pxBehaviour->m_axGUIDRefs.GetSize()));
		}
		xSlot.m_pxBehaviour->RenderPropertiesPanel();
		ImGui::TreePop();
	}

	ImGui::PopID();
}

// "Add Script" button + popup listing every public registered behaviour.
// Internal/test behaviours are intentionally hidden — they can still be
// attached via AddScript<T>() in code.
void Zenith_ScriptComponent::RenderAddScriptPopup()
{
	if (ImGui::Button("Add Script"))
	{
		ImGui::OpenPopup("AddScriptPopup");
	}

	if (!ImGui::BeginPopup("AddScriptPopup")) return;

	Zenith_Vector<std::string> axTypeNames;
	Zenith_ScriptAsset::GetPublicRegisteredTypeNames(axTypeNames);

	// Sort for stable UX. Zenith_Vector now exposes begin()/end() over its contiguous
	// storage, so sort it directly - no temp copy needed.
	std::sort(axTypeNames.begin(), axTypeNames.end());

	for (const std::string& strTypeName : axTypeNames)
	{
		if (ImGui::MenuItem(strTypeName.c_str()))
		{
			const std::string strAssetPath = Zenith_ScriptAsset::MakeAssetPath(strTypeName.c_str());
			AddScriptByAssetPath(strAssetPath.c_str());
		}
	}

	if (axTypeNames.GetSize() == 0)
	{
		ImGui::TextDisabled("(no registered scripts)");
	}

	ImGui::EndPopup();
}

// Drag-drop target for .zscript files from the content browser.
// Content browser sends a DragDropFilePayload containing an absolute file
// path; normalize it to the prefixed form (game:Scripts/...) before attaching.
void Zenith_ScriptComponent::AcceptScriptAssetDragDrop()
{
	if (!ImGui::BeginDragDropTarget()) return;

	const ImGuiPayload* pxPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_SCRIPT_ASSET);
	if (pxPayload && pxPayload->Data && pxPayload->DataSize >= static_cast<int>(sizeof(DragDropFilePayload)))
	{
		const DragDropFilePayload* pxFilePayload = static_cast<const DragDropFilePayload*>(pxPayload->Data);
		const std::string strNormalized = Zenith_AssetRegistry::NormalizeAssetPath(pxFilePayload->m_szFilePath);
		AddScriptByAssetPath(strNormalized.c_str());
	}
	ImGui::EndDragDropTarget();
}

void Zenith_ScriptComponent::RenderPropertiesPanel()
{
	if (!ImGui::CollapsingHeader("Script Component", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	// Process any deferred remove from the previous frame
	if (m_iPendingRemoveIndex >= 0 && static_cast<uint32_t>(m_iPendingRemoveIndex) < m_axSlots.GetSize())
	{
		RemoveScriptAt(static_cast<uint32_t>(m_iPendingRemoveIndex));
		m_iPendingRemoveIndex = -1;
	}

	for (uint32_t u = 0; u < m_axSlots.GetSize(); ++u)
	{
		const Zenith_ScriptSlot& xSlot = m_axSlots.Get(u);
		if (!xSlot.m_pxBehaviour) continue;
		RenderScriptSlot(u, xSlot);
	}

	ImGui::Separator();
	RenderAddScriptPopup();
	AcceptScriptAssetDragDrop();
}
#endif
