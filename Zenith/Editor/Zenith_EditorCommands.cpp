#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_EditorCommands.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorSceneAccess.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "DataStream/Zenith_DataStream.h"

#include <cstring>

namespace
{
	// Copies the bytes written so far into an owned vector.
	void StreamToBytes(const Zenith_DataStream& xStream, Zenith_Vector<uint8_t>& axOut)
	{
		const u_int uSize = static_cast<u_int>(xStream.GetCursor());
		axOut.Clear();
		axOut.Resize(uSize);
		if (uSize > 0)
		{
			memcpy(axOut.GetDataPointer(), xStream.GetData(), uSize);
		}
	}

	bool BytesEqual(const Zenith_Vector<uint8_t>& xA, const Zenith_Vector<uint8_t>& xB)
	{
		if (xA.GetSize() != xB.GetSize())
		{
			return false;
		}
		return xA.GetSize() == 0 || memcmp(xA.GetDataPointer(), xB.GetDataPointer(), xA.GetSize()) == 0;
	}

	// Resolves an entity across every loaded scene; INVALID handle when gone.
	Zenith_Entity Resolve(Zenith_EntityID xID)
	{
		return g_xEngine.Scenes().ResolveEntity(xID);
	}

	constexpr const char* szTRANSFORM_TYPE_NAME = "Transform";
}

//=============================================================================
// Zenith_EditorEntitySnapshot
//=============================================================================

void Zenith_EditorEntitySnapshot::SerializeAllComponents(Zenith_Entity& xEntity, Zenith_Vector<uint8_t>& axOut)
{
	Zenith_DataStream xStream;
	Zenith_ComponentMetaRegistry::Get().SerializeEntityComponents(xEntity, xStream);
	StreamToBytes(xStream, axOut);
}

bool Zenith_EditorEntitySnapshot::Capture(Zenith_Entity xRoot)
{
	m_axRecords.Clear();
	m_xRootParent = INVALID_ENTITY_ID;
	m_bRootWasMainCamera = false;
	if (!xRoot.IsValid())
	{
		return false;
	}

	m_xScene = xRoot.GetScene();
	m_xRootParent = xRoot.GetParentEntityID();
	Zenith_SceneData* pxSceneData = xRoot.GetSceneData();
	m_bRootWasMainCamera = (pxSceneData != nullptr && pxSceneData->GetMainCameraEntity() == xRoot.GetEntityID());

	CaptureRecursive(xRoot, uROOT);
	return true;
}

void Zenith_EditorEntitySnapshot::CaptureRecursive(Zenith_Entity xEntity, u_int uParentRecord)
{
	Record xRecord;
	xRecord.m_strName = xEntity.GetName();
	xRecord.m_bEnabled = xEntity.IsEnabled();
	xRecord.m_bTransient = xEntity.IsTransient();
	xRecord.m_uParentRecord = uParentRecord;
	SerializeAllComponents(xEntity, xRecord.m_axComponentBytes);
	m_axRecords.PushBack(xRecord);
	const u_int uThisRecord = m_axRecords.GetSize() - 1;

	// Copy the child list: capturing serialises components, which must not
	// observe a child list that is being iterated by reference.
	const Zenith_Vector<Zenith_EntityID> xChildren = xEntity.GetChildEntityIDs();
	Zenith_SceneData* pxSceneData = xEntity.GetSceneData();
	for (u_int u = 0; u < xChildren.GetSize(); ++u)
	{
		const Zenith_EntityID xChildID = xChildren.Get(u);
		if (pxSceneData != nullptr && pxSceneData->EntityExists(xChildID))
		{
			CaptureRecursive(pxSceneData->GetEntity(xChildID), uThisRecord);
		}
	}
}

Zenith_EntityID Zenith_EditorEntitySnapshot::Restore(Zenith_EntityID xParentOverride, bool bUseCapturedParent) const
{
	if (!IsValid())
	{
		return INVALID_ENTITY_ID;
	}

	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(m_xScene);
	Zenith_Scene xScene = m_xScene;
	if (pxSceneData == nullptr)
	{
		// The captured scene is gone (unloaded since); rebuild into the active one.
		xScene = g_xEngine.Scenes().GetActiveScene();
		pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxSceneData == nullptr)
		{
			return INVALID_ENTITY_ID;
		}
	}

	Zenith_Vector<Zenith_EntityID> axNewIDs;
	axNewIDs.Reserve(m_axRecords.GetSize());

	for (u_int u = 0; u < m_axRecords.GetSize(); ++u)
	{
		const Record& xRecord = m_axRecords.Get(u);

		// Bare: the Transform (and everything else) comes back from the bytes.
		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntityBare(xScene, xRecord.m_strName);
		if (!xEntity.IsValid())
		{
			return INVALID_ENTITY_ID;
		}

		if (xRecord.m_axComponentBytes.GetSize() > 0)
		{
			// Wrap (not copy): the stream reads the record's bytes in place.
			Zenith_DataStream xStream(const_cast<uint8_t*>(xRecord.m_axComponentBytes.GetDataPointer()), xRecord.m_axComponentBytes.GetSize());
			Zenith_ComponentMetaRegistry::Get().DeserializeEntityComponents(xEntity, xStream);
		}
		xEntity.SetTransient(xRecord.m_bTransient);

		Zenith_EntityID xParentID = INVALID_ENTITY_ID;
		if (xRecord.m_uParentRecord == uROOT)
		{
			xParentID = bUseCapturedParent ? m_xRootParent : xParentOverride;
		}
		else
		{
			xParentID = axNewIDs.Get(xRecord.m_uParentRecord);
		}
		if (xParentID.IsValid() && pxSceneData->EntityExists(xParentID))
		{
			xEntity.SetParent(xParentID);
		}

		axNewIDs.PushBack(xEntity.GetEntityID());
	}

	// Enabled flags after the hierarchy exists, so a disabled parent's children
	// pick up the correct active-in-hierarchy state.
	for (u_int u = 0; u < m_axRecords.GetSize(); ++u)
	{
		if (!m_axRecords.Get(u).m_bEnabled)
		{
			pxSceneData->GetEntity(axNewIDs.Get(u)).SetEnabled(false);
		}
	}

	const Zenith_EntityID xNewRoot = axNewIDs.Get(0);
	if (m_bRootWasMainCamera)
	{
		Zenith_EditorSceneAccess::SetMainCameraEntity(pxSceneData, xNewRoot);
	}
	Zenith_EditorSceneAccess::MarkDirty(pxSceneData);
	return xNewRoot;
}

const std::string& Zenith_EditorEntitySnapshot::GetRootName() const
{
	static const std::string s_strEmpty;
	return IsValid() ? m_axRecords.Get(0).m_strName : s_strEmpty;
}

void Zenith_EditorEntitySnapshot::SetRootName(const std::string& strName)
{
	if (IsValid())
	{
		m_axRecords.Get(0).m_strName = strName;
	}
}

//=============================================================================
// Zenith_UndoCommand_EntityLifetime
//=============================================================================

Zenith_UndoCommand_EntityLifetime* Zenith_UndoCommand_EntityLifetime::MakeDelete(Zenith_Entity xEntity)
{
	if (!xEntity.IsValid())
	{
		return nullptr;
	}
	Zenith_UndoCommand_EntityLifetime* pxCommand = new Zenith_UndoCommand_EntityLifetime();
	pxCommand->m_eKind = Kind::Delete;
	pxCommand->m_xSnapshot.Capture(xEntity);
	pxCommand->m_xCurrentID = xEntity.GetEntityID();
	pxCommand->m_bUseCapturedParent = true;
	pxCommand->m_strDescription = "Delete '" + xEntity.GetName() + "'";
	return pxCommand;
}

Zenith_UndoCommand_EntityLifetime* Zenith_UndoCommand_EntityLifetime::MakeCreate(const Zenith_EditorEntitySnapshot& xSnapshot,
	Zenith_EntityID xParent, const char* szDescription)
{
	if (!xSnapshot.IsValid())
	{
		return nullptr;
	}
	Zenith_UndoCommand_EntityLifetime* pxCommand = new Zenith_UndoCommand_EntityLifetime();
	pxCommand->m_eKind = Kind::Create;
	pxCommand->m_xSnapshot = xSnapshot;
	pxCommand->m_xParentOverride = xParent;
	pxCommand->m_bUseCapturedParent = false;
	pxCommand->m_strDescription = szDescription;
	return pxCommand;
}

void Zenith_UndoCommand_EntityLifetime::Execute()
{
	if (m_eKind == Kind::Delete) Destroy(); else Rebuild();
}

void Zenith_UndoCommand_EntityLifetime::Undo()
{
	if (m_eKind == Kind::Delete) Rebuild(); else Destroy();
}

void Zenith_UndoCommand_EntityLifetime::Destroy()
{
	Zenith_Entity xEntity = Resolve(m_xCurrentID);
	if (!xEntity.IsValid())
	{
		Zenith_Warning(LOG_CATEGORY_EDITOR, "[UndoSystem] %s: entity no longer exists", m_strDescription.c_str());
		m_xCurrentID = INVALID_ENTITY_ID;
		return;
	}

	// Re-capture right before destroying so a redo after edits restores the
	// entity as it was at THIS point, and so the captured parent is current.
	m_xSnapshot.Capture(xEntity);

	Zenith_Editor& xEditor = g_xEngine.Editor();
	if (xEditor.IsSelected(m_xCurrentID))
	{
		xEditor.DeselectEntity(m_xCurrentID);
	}
	if (xEditor.m_xEditorState.m_xCamera.m_uGameCameraEntity == m_xCurrentID)
	{
		xEditor.m_xEditorState.m_xCamera.m_uGameCameraEntity = INVALID_ENTITY_ID;
	}

	Zenith_SceneData* pxSceneData = xEntity.GetSceneData();
	Zenith_EditorSceneAccess::RemoveEntity(pxSceneData, m_xCurrentID);
	Zenith_EditorSceneAccess::MarkDirty(pxSceneData);
	m_xCurrentID = INVALID_ENTITY_ID;
}

void Zenith_UndoCommand_EntityLifetime::Rebuild()
{
	m_xCurrentID = m_xSnapshot.Restore(m_xParentOverride, m_bUseCapturedParent);
	if (m_xCurrentID.IsValid())
	{
		// A restored entity is what the user is now looking at.
		g_xEngine.Editor().SelectEntity(m_xCurrentID);
	}
}

//=============================================================================
// Zenith_UndoCommand_Composite
//=============================================================================

Zenith_UndoCommand_Composite::~Zenith_UndoCommand_Composite()
{
	for (u_int u = 0; u < m_axCommands.GetSize(); ++u)
	{
		delete m_axCommands.Get(u);
	}
	m_axCommands.Clear();
}

void Zenith_UndoCommand_Composite::Execute()
{
	for (u_int u = 0; u < m_axCommands.GetSize(); ++u)
	{
		m_axCommands.Get(u)->Execute();
	}
}

void Zenith_UndoCommand_Composite::Undo()
{
	for (u_int u = m_axCommands.GetSize(); u > 0; --u)
	{
		m_axCommands.Get(u - 1)->Undo();
	}
}

//=============================================================================
// Zenith_UndoCommand_EntityState
//=============================================================================

Zenith_UndoCommand_EntityState::State Zenith_UndoCommand_EntityState::CaptureState(Zenith_Entity xEntity)
{
	State xState;
	if (xEntity.IsValid())
	{
		xState.m_strName = xEntity.GetName();
		xState.m_bEnabled = xEntity.IsEnabled();
		xState.m_xParent = xEntity.GetParentEntityID();
	}
	return xState;
}

void Zenith_UndoCommand_EntityState::ApplyState(Zenith_EntityID xEntityID, const State& xState)
{
	Zenith_Entity xEntity = Resolve(xEntityID);
	if (!xEntity.IsValid())
	{
		Zenith_Warning(LOG_CATEGORY_EDITOR, "[UndoSystem] entity state: entity no longer exists");
		return;
	}
	if (xEntity.GetName() != xState.m_strName)
	{
		xEntity.SetName(xState.m_strName);
	}
	if (xEntity.GetParentEntityID() != xState.m_xParent)
	{
		// A parent that vanished in the meantime falls back to the root.
		const bool bParentAlive = xState.m_xParent.IsValid() && Resolve(xState.m_xParent).IsValid();
		xEntity.SetParent(bParentAlive ? xState.m_xParent : INVALID_ENTITY_ID);
	}
	if (xEntity.IsEnabled() != xState.m_bEnabled)
	{
		xEntity.SetEnabled(xState.m_bEnabled);
	}
	if (Zenith_SceneData* pxSceneData = xEntity.GetSceneData())
	{
		Zenith_EditorSceneAccess::MarkDirty(pxSceneData);
	}
}

Zenith_UndoCommand_EntityState::Zenith_UndoCommand_EntityState(Zenith_EntityID xEntityID, const State& xOld, const State& xNew, const char* szDescription)
	: m_xEntityID(xEntityID)
	, m_xOld(xOld)
	, m_xNew(xNew)
	, m_strDescription(szDescription)
{
}

//=============================================================================
// Zenith_UndoCommand_ComponentBytes
//=============================================================================

bool Zenith_UndoCommand_ComponentBytes::CaptureComponent(Zenith_Entity xEntity, const char* szTypeName, Zenith_Vector<uint8_t>& axOut)
{
	axOut.Clear();
	const Zenith_ComponentMeta* pxMeta = Zenith_ComponentMetaRegistry::Get().GetMetaByName(szTypeName);
	if (pxMeta == nullptr || !xEntity.IsValid() || pxMeta->m_pfnHasComponent == nullptr || !pxMeta->m_pfnHasComponent(xEntity))
	{
		return false;
	}
	Zenith_DataStream xStream;
	pxMeta->m_pfnSerialize(xEntity, xStream);
	StreamToBytes(xStream, axOut);
	return true;
}

void Zenith_UndoCommand_ComponentBytes::ApplyBytes(Zenith_EntityID xEntityID, const char* szTypeName, const Zenith_Vector<uint8_t>& axBytes)
{
	Zenith_Entity xEntity = Resolve(xEntityID);
	const Zenith_ComponentMeta* pxMeta = Zenith_ComponentMetaRegistry::Get().GetMetaByName(szTypeName);
	if (!xEntity.IsValid() || pxMeta == nullptr)
	{
		Zenith_Warning(LOG_CATEGORY_EDITOR, "[UndoSystem] component '%s': entity or type no longer exists", szTypeName);
		return;
	}

	const bool bHas = pxMeta->m_pfnHasComponent(xEntity);
	const bool bInPlace = (strcmp(szTypeName, szTRANSFORM_TYPE_NAME) == 0);
	if (bHas && !bInPlace)
	{
		pxMeta->m_pfnRemoveComponent(xEntity);
	}
	if (axBytes.GetSize() > 0)
	{
		Zenith_DataStream xStream(const_cast<uint8_t*>(axBytes.GetDataPointer()), axBytes.GetSize());
		// The deserialise wrapper adds the component when absent, then reads.
		pxMeta->m_pfnDeserialize(xEntity, xStream, pxMeta->m_uSchemaVersion);
	}
	if (Zenith_SceneData* pxSceneData = xEntity.GetSceneData())
	{
		Zenith_EditorSceneAccess::MarkDirty(pxSceneData);
	}
}

Zenith_UndoCommand_ComponentBytes::Zenith_UndoCommand_ComponentBytes(Zenith_EntityID xEntityID, const char* szTypeName,
	const Zenith_Vector<uint8_t>& axBefore, const Zenith_Vector<uint8_t>& axAfter, const char* szDescription)
	: m_xEntityID(xEntityID)
	, m_strTypeName(szTypeName)
	, m_axBefore(axBefore)
	, m_axAfter(axAfter)
	, m_strDescription(szDescription)
{
}

//=============================================================================
// Zenith_EditorInspectorUndoTracker
//=============================================================================

void Zenith_EditorInspectorUndoTracker::Capture(Zenith_Entity xEntity)
{
	m_axBefore.Clear();
	m_bHasTransform = false;

	const Zenith_Vector<const Zenith_ComponentMeta*>& xMetas = Zenith_ComponentMetaRegistry::Get().GetAllMetasSorted();
	for (u_int u = 0; u < xMetas.GetSize(); ++u)
	{
		const Zenith_ComponentMeta* pxMeta = xMetas.Get(u);
		if (pxMeta->m_pfnHasComponent == nullptr || !pxMeta->m_pfnHasComponent(xEntity))
		{
			continue;
		}
		if (pxMeta->m_strTypeName == szTRANSFORM_TYPE_NAME)
		{
			Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
			xTransform.GetPosition(m_xBeforePosition);
			xTransform.GetRotation(m_xBeforeRotation);
			xTransform.GetScale(m_xBeforeScale);
			m_bHasTransform = true;
			continue;
		}
		ComponentSnapshot xSnapshot;
		xSnapshot.m_strTypeName = pxMeta->m_strTypeName;
		Zenith_UndoCommand_ComponentBytes::CaptureComponent(xEntity, pxMeta->m_strTypeName.c_str(), xSnapshot.m_axBytes);
		m_axBefore.PushBack(xSnapshot);
	}
}

void Zenith_EditorInspectorUndoTracker::BeginFrame(Zenith_Entity xEntity, bool bTrigger)
{
	if (!xEntity.IsValid())
	{
		Cancel();
		return;
	}
	if (m_bOpen && m_xEntityID != xEntity.GetEntityID())
	{
		Cancel();
	}
	if (!m_bOpen && bTrigger)
	{
		m_xEntityID = xEntity.GetEntityID();
		Capture(xEntity);
		m_bOpen = true;
	}
}

void Zenith_EditorInspectorUndoTracker::EndFrame(Zenith_Entity xEntity, bool bUIIdle)
{
	if (!m_bOpen || !bUIIdle)
	{
		return;
	}
	if (xEntity.IsValid() && xEntity.GetEntityID() == m_xEntityID)
	{
		CommitDiff(xEntity);
	}
	Cancel();
}

void Zenith_EditorInspectorUndoTracker::Cancel()
{
	m_bOpen = false;
	m_axBefore.Clear();
	m_bHasTransform = false;
}

u_int Zenith_EditorInspectorUndoTracker::CommitDiff(Zenith_Entity xEntity)
{
	u_int uRecorded = 0;
	Zenith_UndoSystem& xUndo = g_xEngine.UndoSystem();

	if (m_bHasTransform && xEntity.HasComponent<Zenith_TransformComponent>())
	{
		Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
		Zenith_Maths::Vector3 xPos, xScale;
		Zenith_Maths::Quat xRot;
		xTransform.GetPosition(xPos);
		xTransform.GetRotation(xRot);
		xTransform.GetScale(xScale);
		if (xPos != m_xBeforePosition || xRot != m_xBeforeRotation || xScale != m_xBeforeScale)
		{
			xUndo.Record(new Zenith_UndoCommand_TransformEdit(xEntity.GetEntityID(),
				m_xBeforePosition, m_xBeforeRotation, m_xBeforeScale, xPos, xRot, xScale));
			++uRecorded;
		}
	}

	for (u_int u = 0; u < m_axBefore.GetSize(); ++u)
	{
		const ComponentSnapshot& xBefore = m_axBefore.Get(u);
		Zenith_Vector<uint8_t> axAfter;
		// A component that was added or removed this session is already covered
		// by the panel's own add/remove command; only in-place edits diff here.
		if (!Zenith_UndoCommand_ComponentBytes::CaptureComponent(xEntity, xBefore.m_strTypeName.c_str(), axAfter))
		{
			continue;
		}
		if (BytesEqual(xBefore.m_axBytes, axAfter))
		{
			continue;
		}
		const std::string strDescription = "Edit " + xBefore.m_strTypeName;
		xUndo.Record(new Zenith_UndoCommand_ComponentBytes(xEntity.GetEntityID(), xBefore.m_strTypeName.c_str(),
			xBefore.m_axBytes, axAfter, strDescription.c_str()));
		if (Zenith_SceneData* pxSceneData = xEntity.GetSceneData())
		{
			Zenith_EditorSceneAccess::MarkDirty(pxSceneData);
		}
		++uRecorded;
	}
	return uRecorded;
}

#ifdef ZENITH_TESTING
#include "Editor/Zenith_EditorCommands.Tests.inl"
#endif

#endif // ZENITH_TOOLS
