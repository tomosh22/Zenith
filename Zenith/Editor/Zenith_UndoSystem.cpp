#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_UndoSystem.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "DataStream/Zenith_DataStream.h"

// Static member initialization
std::vector<std::unique_ptr<Zenith_UndoCommand>> Zenith_UndoSystem::s_xUndoStack;
std::vector<std::unique_ptr<Zenith_UndoCommand>> Zenith_UndoSystem::s_xRedoStack;

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

	// Add to undo stack (transfer ownership to unique_ptr)
	s_xUndoStack.push_back(std::unique_ptr<Zenith_UndoCommand>(pCommand));

	// Clear redo stack (branching timeline)
	s_xRedoStack.clear();

	// Enforce stack size limit
	EnforceStackLimit();

	Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Executed: %s (Undo stack: %u, Redo stack: %u)",
		pCommand->GetDescription(),
		static_cast<u_int>(s_xUndoStack.size()),
		static_cast<u_int>(s_xRedoStack.size()));
}

void Zenith_UndoSystem::Undo()
{
	if (!CanUndo())
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Cannot undo - stack is empty");
		return;
	}

	// Pop from undo stack
	std::unique_ptr<Zenith_UndoCommand> pCommand = std::move(s_xUndoStack.back());
	s_xUndoStack.pop_back();

	// Undo the command
	pCommand->Undo();

	// Move to redo stack
	s_xRedoStack.push_back(std::move(pCommand));

	Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Undone: %s (Undo stack: %u, Redo stack: %u)",
		s_xRedoStack.back()->GetDescription(),
		static_cast<u_int>(s_xUndoStack.size()),
		static_cast<u_int>(s_xRedoStack.size()));
}

void Zenith_UndoSystem::Redo()
{
	if (!CanRedo())
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Cannot redo - stack is empty");
		return;
	}

	// Pop from redo stack
	std::unique_ptr<Zenith_UndoCommand> pCommand = std::move(s_xRedoStack.back());
	s_xRedoStack.pop_back();

	// Re-execute the command
	pCommand->Execute();

	// Move to undo stack
	s_xUndoStack.push_back(std::move(pCommand));

	Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Redone: %s (Undo stack: %u, Redo stack: %u)",
		s_xUndoStack.back()->GetDescription(),
		static_cast<u_int>(s_xUndoStack.size()),
		static_cast<u_int>(s_xRedoStack.size()));
}

bool Zenith_UndoSystem::CanUndo()
{
	return !s_xUndoStack.empty();
}

bool Zenith_UndoSystem::CanRedo()
{
	return !s_xRedoStack.empty();
}

const char* Zenith_UndoSystem::GetUndoDescription()
{
	if (!CanUndo())
		return "";

	return s_xUndoStack.back()->GetDescription();
}

const char* Zenith_UndoSystem::GetRedoDescription()
{
	if (!CanRedo())
		return "";

	return s_xRedoStack.back()->GetDescription();
}

void Zenith_UndoSystem::Clear()
{
	s_xUndoStack.clear();
	s_xRedoStack.clear();
	Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Cleared all undo/redo history");
}

void Zenith_UndoSystem::EnforceStackLimit()
{
	// Remove oldest commands if stack exceeds limit
	while (s_xUndoStack.size() > MAX_UNDO_STACK_SIZE)
	{
		s_xUndoStack.erase(s_xUndoStack.begin());
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
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Verify entity still exists
	if (!xScene.EntityExists(m_uEntityID))
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] WARNING: Entity %u no longer exists, cannot execute transform edit", m_uEntityID);
		return;
	}

	Zenith_Entity xEntity = xScene.GetEntity(m_uEntityID);

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
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Verify entity still exists
	if (!xScene.EntityExists(m_uEntityID))
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] WARNING: Entity %u no longer exists, cannot undo transform edit", m_uEntityID);
		return;
	}

	Zenith_Entity xEntity = xScene.GetEntity(m_uEntityID);

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
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Check if entity already exists (redo case)
	if (xScene.EntityExists(m_uEntityID))
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
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Verify entity exists
	if (!xScene.EntityExists(m_uEntityID))
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] WARNING: Entity %u does not exist, cannot undo creation", m_uEntityID);
		return;
	}

	// Remove entity from scene
	xScene.RemoveEntity(m_uEntityID);
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
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Verify entity exists
	if (!xScene.EntityExists(m_uEntityID))
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] WARNING: Entity %u does not exist, cannot capture state for deletion", m_uEntityID);
		m_strName = "Unknown";
		return;
	}

	// Capture entity state before deletion
	Zenith_Entity xEntity = xScene.GetEntity(m_uEntityID);
	m_strName = xEntity.GetName();

	// TODO: Serialize full entity state (all components)
	// For now, we just capture the name
	// A full implementation would use Zenith_DataStream to serialize the entity
	m_strSerializedState = "";

	Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Captured state for entity %u (%s) before deletion", m_uEntityID, m_strName.c_str());
}

void Zenith_UndoCommand_DeleteEntity::Execute()
{
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Verify entity exists
	if (!xScene.EntityExists(m_uEntityID))
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] WARNING: Entity %u does not exist, cannot delete", m_uEntityID);
		return;
	}

	// Remove entity from scene
	xScene.RemoveEntity(m_uEntityID);
	m_bDeleted = true;

	Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Deleted entity %u (%s)", m_uEntityID, m_strName.c_str());
}

void Zenith_UndoCommand_DeleteEntity::Undo()
{
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Check if entity already exists (shouldn't happen)
	if (xScene.EntityExists(m_uEntityID))
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
