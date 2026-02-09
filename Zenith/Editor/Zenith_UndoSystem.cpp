#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_UndoSystem.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "DataStream/Zenith_DataStream.h"

// Static member initialization
Zenith_Vector<Zenith_UndoCommand*> Zenith_UndoSystem::s_xUndoStack;
Zenith_Vector<Zenith_UndoCommand*> Zenith_UndoSystem::s_xRedoStack;

Zenith_UndoCommand::Zenith_UndoCommand()
	: m_xScene(Zenith_SceneManager::GetActiveScene())
{
}

//------------------------------------------------------------------------------
// Zenith_UndoSystem Implementation
//------------------------------------------------------------------------------

void Zenith_UndoSystem::Execute(Zenith_UndoCommand* pCommand)
{
	if (!pCommand)
	{
		Zenith_Assert(false, "Null command passed to Execute");
		return;
	}

	// Execute the command
	pCommand->Execute();

	// Add to undo stack (take ownership)
	s_xUndoStack.PushBack(pCommand);

	// Clear redo stack (branching timeline) - delete all commands first
	for (u_int u = 0; u < s_xRedoStack.GetSize(); u++)
	{
		delete s_xRedoStack.Get(u);
	}
	s_xRedoStack.Clear();

	// Enforce stack size limit
	EnforceStackLimit();

	Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Executed: %s (Undo stack: %u, Redo stack: %u)",
		pCommand->GetDescription(),
		s_xUndoStack.GetSize(),
		s_xRedoStack.GetSize());
}

void Zenith_UndoSystem::Undo()
{
	if (!CanUndo())
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Cannot undo - stack is empty");
		return;
	}

	// Pop from undo stack
	Zenith_UndoCommand* pCommand = s_xUndoStack.GetBack();
	s_xUndoStack.PopBack();

	// Undo the command
	pCommand->Undo();

	// Move to redo stack
	s_xRedoStack.PushBack(pCommand);

	Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Undone: %s (Undo stack: %u, Redo stack: %u)",
		s_xRedoStack.GetBack()->GetDescription(),
		s_xUndoStack.GetSize(),
		s_xRedoStack.GetSize());
}

void Zenith_UndoSystem::Redo()
{
	if (!CanRedo())
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Cannot redo - stack is empty");
		return;
	}

	// Pop from redo stack
	Zenith_UndoCommand* pCommand = s_xRedoStack.GetBack();
	s_xRedoStack.PopBack();

	// Re-execute the command
	pCommand->Execute();

	// Move to undo stack
	s_xUndoStack.PushBack(pCommand);

	Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Redone: %s (Undo stack: %u, Redo stack: %u)",
		s_xUndoStack.GetBack()->GetDescription(),
		s_xUndoStack.GetSize(),
		s_xRedoStack.GetSize());
}

bool Zenith_UndoSystem::CanUndo()
{
	return s_xUndoStack.GetSize() > 0;
}

bool Zenith_UndoSystem::CanRedo()
{
	return s_xRedoStack.GetSize() > 0;
}

const char* Zenith_UndoSystem::GetUndoDescription()
{
	if (!CanUndo())
		return "";

	return s_xUndoStack.GetBack()->GetDescription();
}

const char* Zenith_UndoSystem::GetRedoDescription()
{
	if (!CanRedo())
		return "";

	return s_xRedoStack.GetBack()->GetDescription();
}

void Zenith_UndoSystem::Clear()
{
	for (u_int u = 0; u < s_xUndoStack.GetSize(); u++)
	{
		delete s_xUndoStack.Get(u);
	}
	s_xUndoStack.Clear();

	for (u_int u = 0; u < s_xRedoStack.GetSize(); u++)
	{
		delete s_xRedoStack.Get(u);
	}
	s_xRedoStack.Clear();

	Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Cleared all undo/redo history");
}

void Zenith_UndoSystem::EnforceStackLimit()
{
	// Remove oldest commands if stack exceeds limit
	while (s_xUndoStack.GetSize() > MAX_UNDO_STACK_SIZE)
	{
		delete s_xUndoStack.GetFront();
		s_xUndoStack.Remove(0);
	}
}

//------------------------------------------------------------------------------
// Zenith_UndoCommand_TransformEdit Implementation
//------------------------------------------------------------------------------

Zenith_UndoCommand_TransformEdit::Zenith_UndoCommand_TransformEdit(
	Zenith_EntityID uEntityID,
	const Zenith_Maths::Vector3& xOldPosition,
	const Zenith_Maths::Quat& xOldRotation,
	const Zenith_Maths::Vector3& xOldScale,
	const Zenith_Maths::Vector3& xNewPosition,
	const Zenith_Maths::Quat& xNewRotation,
	const Zenith_Maths::Vector3& xNewScale
)
	: m_uEntityID(uEntityID)
	, m_xOldPosition(xOldPosition)
	, m_xOldRotation(xOldRotation)
	, m_xOldScale(xOldScale)
	, m_xNewPosition(xNewPosition)
	, m_xNewRotation(xNewRotation)
	, m_xNewScale(xNewScale)
{
}

void Zenith_UndoCommand_TransformEdit::Execute()
{
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xScene);
	if (!pxSceneData)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] WARNING: No active scene, cannot execute transform edit");
		return;
	}

	// Verify entity still exists
	if (!pxSceneData->EntityExists(m_uEntityID))
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] WARNING: Entity %u no longer exists, cannot execute transform edit", m_uEntityID);
		return;
	}

	Zenith_Entity xEntity = pxSceneData->GetEntity(m_uEntityID);

	if (!xEntity.HasComponent<Zenith_TransformComponent>())
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] WARNING: Entity %u has no TransformComponent, cannot execute transform edit", m_uEntityID);
		return;
	}

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(m_xNewPosition);
	xTransform.SetRotation(m_xNewRotation);
	xTransform.SetScale(m_xNewScale);
}

void Zenith_UndoCommand_TransformEdit::Undo()
{
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xScene);
	if (!pxSceneData)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] WARNING: No active scene, cannot undo transform edit");
		return;
	}

	// Verify entity still exists
	if (!pxSceneData->EntityExists(m_uEntityID))
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] WARNING: Entity %u no longer exists, cannot undo transform edit", m_uEntityID);
		return;
	}

	Zenith_Entity xEntity = pxSceneData->GetEntity(m_uEntityID);

	if (!xEntity.HasComponent<Zenith_TransformComponent>())
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] WARNING: Entity %u has no TransformComponent, cannot undo transform edit", m_uEntityID);
		return;
	}

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(m_xOldPosition);
	xTransform.SetRotation(m_xOldRotation);
	xTransform.SetScale(m_xOldScale);
}

const char* Zenith_UndoCommand_TransformEdit::GetDescription() const
{
	return "Edit Transform";
}

//------------------------------------------------------------------------------
// Zenith_UndoCommand_CreateEntity Implementation
//------------------------------------------------------------------------------

Zenith_UndoCommand_CreateEntity::Zenith_UndoCommand_CreateEntity(
	Zenith_EntityID uEntityID,
	const std::string& strName
)
	: m_uEntityID(uEntityID)
	, m_strName(strName)
	, m_bCreated(false)
{
}

void Zenith_UndoCommand_CreateEntity::Execute()
{
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xScene);
	if (!pxSceneData)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] WARNING: No active scene, cannot execute create entity");
		return;
	}

	// Check if entity already exists (redo case)
	if (pxSceneData->EntityExists(m_uEntityID))
	{
		// Entity already exists, nothing to do
		m_bCreated = true;
		return;
	}

	// Create entity with stored ID and name
	// NOTE: This is a simplified implementation
	// In practice, you'd need to properly recreate the entity with all components
	// For now, we just track the creation/deletion state
	// A full implementation would serialize/deserialize all components

	Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] WARNING: CreateEntity command execute() - entity recreation not fully implemented");
	Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Entity %u (%s) marked as created", m_uEntityID, m_strName.c_str());

	m_bCreated = true;
}

void Zenith_UndoCommand_CreateEntity::Undo()
{
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xScene);
	if (!pxSceneData)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] WARNING: No active scene, cannot undo create entity");
		return;
	}

	// Verify entity exists
	if (!pxSceneData->EntityExists(m_uEntityID))
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] WARNING: Entity %u does not exist, cannot undo creation", m_uEntityID);
		return;
	}

	// Remove entity from scene
	pxSceneData->RemoveEntity(m_uEntityID);
	m_bCreated = false;

	Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Removed entity %u (%s)", m_uEntityID, m_strName.c_str());
}

const char* Zenith_UndoCommand_CreateEntity::GetDescription() const
{
	return "Create Entity";
}

//------------------------------------------------------------------------------
// Zenith_UndoCommand_DeleteEntity Implementation
//------------------------------------------------------------------------------

Zenith_UndoCommand_DeleteEntity::Zenith_UndoCommand_DeleteEntity(Zenith_EntityID uEntityID)
	: m_uEntityID(uEntityID)
	, m_bDeleted(false)
{
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xScene);
	if (!pxSceneData)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] WARNING: No active scene, cannot capture state for deletion");
		m_strName = "Unknown";
		return;
	}

	// Verify entity exists
	if (!pxSceneData->EntityExists(m_uEntityID))
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] WARNING: Entity %u does not exist, cannot capture state for deletion", m_uEntityID);
		m_strName = "Unknown";
		return;
	}

	// Capture entity state before deletion
	Zenith_Entity xEntity = pxSceneData->GetEntity(m_uEntityID);
	m_strName = xEntity.GetName();

	// TODO: Serialize full entity state (all components)
	// For now, we just capture the name
	// A full implementation would use Zenith_DataStream to serialize the entity
	m_strSerializedState = "";

	Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Captured state for entity %u (%s) before deletion", m_uEntityID, m_strName.c_str());
}

void Zenith_UndoCommand_DeleteEntity::Execute()
{
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xScene);
	if (!pxSceneData)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] WARNING: No active scene, cannot delete entity");
		return;
	}

	// Verify entity exists
	if (!pxSceneData->EntityExists(m_uEntityID))
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] WARNING: Entity %u does not exist, cannot delete", m_uEntityID);
		return;
	}

	// Remove entity from scene
	pxSceneData->RemoveEntity(m_uEntityID);
	m_bDeleted = true;

	Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Deleted entity %u (%s)", m_uEntityID, m_strName.c_str());
}

void Zenith_UndoCommand_DeleteEntity::Undo()
{
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xScene);
	if (!pxSceneData)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] WARNING: No active scene, cannot undo delete entity");
		return;
	}

	// Check if entity already exists (shouldn't happen)
	if (pxSceneData->EntityExists(m_uEntityID))
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] WARNING: Entity %u already exists, cannot undo deletion", m_uEntityID);
		return;
	}

	// Recreate entity from serialized state
	// TODO: Deserialize full entity state
	// For now, we just log a warning
	Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] WARNING: DeleteEntity undo() - entity recreation not fully implemented");
	Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Entity %u (%s) would be recreated here", m_uEntityID, m_strName.c_str());

	m_bDeleted = false;
}

const char* Zenith_UndoCommand_DeleteEntity::GetDescription() const
{
	return "Delete Entity";
}

#endif // ZENITH_TOOLS
