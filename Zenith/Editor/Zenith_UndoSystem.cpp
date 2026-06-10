#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Zenith_UndoSystem.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Editor/Zenith_EditorSceneAccess.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "DataStream/Zenith_DataStream.h"

u_int Zenith_UndoSystem::GetUndoStackSize() { return Zenith_UndoSystem::m_xUndoStack.GetSize(); }
u_int Zenith_UndoSystem::GetRedoStackSize() { return Zenith_UndoSystem::m_xRedoStack.GetSize(); }

// Zenith_UndoCommand default constructor is now defaulted in the header.
// Derived commands resolve their target scene via GetSceneDataForEntity(EntityID)
// at Execute/Undo time — see audit §3.18 note in Zenith_UndoSystem.h.

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
	m_xUndoStack.PushBack(pCommand);

	// Clear redo stack (branching timeline) - delete all commands first
	for (u_int u = 0; u < m_xRedoStack.GetSize(); u++)
	{
		delete m_xRedoStack.Get(u);
	}
	m_xRedoStack.Clear();

	// Enforce stack size limit
	EnforceStackLimit();

	Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Executed: %s (Undo stack: %u, Redo stack: %u)",
		pCommand->GetDescription(),
		m_xUndoStack.GetSize(),
		m_xRedoStack.GetSize());
}

void Zenith_UndoSystem::Undo()
{
	if (!CanUndo())
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Cannot undo - stack is empty");
		return;
	}

	// Pop from undo stack
	Zenith_UndoCommand* pCommand = m_xUndoStack.GetBack();
	m_xUndoStack.PopBack();

	// Undo the command
	pCommand->Undo();

	// Move to redo stack
	m_xRedoStack.PushBack(pCommand);

	Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Undone: %s (Undo stack: %u, Redo stack: %u)",
		m_xRedoStack.GetBack()->GetDescription(),
		m_xUndoStack.GetSize(),
		m_xRedoStack.GetSize());
}

void Zenith_UndoSystem::Redo()
{
	if (!CanRedo())
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Cannot redo - stack is empty");
		return;
	}

	// Pop from redo stack
	Zenith_UndoCommand* pCommand = m_xRedoStack.GetBack();
	m_xRedoStack.PopBack();

	// Re-execute the command
	pCommand->Execute();

	// Move to undo stack
	m_xUndoStack.PushBack(pCommand);

	Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Redone: %s (Undo stack: %u, Redo stack: %u)",
		m_xUndoStack.GetBack()->GetDescription(),
		m_xUndoStack.GetSize(),
		m_xRedoStack.GetSize());
}

bool Zenith_UndoSystem::CanUndo()
{
	return m_xUndoStack.GetSize() > 0;
}

bool Zenith_UndoSystem::CanRedo()
{
	return m_xRedoStack.GetSize() > 0;
}

const char* Zenith_UndoSystem::GetUndoDescription()
{
	if (!CanUndo())
		return "";

	return m_xUndoStack.GetBack()->GetDescription();
}

const char* Zenith_UndoSystem::GetRedoDescription()
{
	if (!CanRedo())
		return "";

	return m_xRedoStack.GetBack()->GetDescription();
}

void Zenith_UndoSystem::Clear()
{
	for (u_int u = 0; u < m_xUndoStack.GetSize(); u++)
	{
		delete m_xUndoStack.Get(u);
	}
	m_xUndoStack.Clear();

	for (u_int u = 0; u < m_xRedoStack.GetSize(); u++)
	{
		delete m_xRedoStack.Get(u);
	}
	m_xRedoStack.Clear();

	Zenith_Log(LOG_CATEGORY_EDITOR, "[UndoSystem] Cleared all undo/redo history");
}

void Zenith_UndoSystem::EnforceStackLimit()
{
	// Remove oldest commands if stack exceeds limit
	while (m_xUndoStack.GetSize() > MAX_UNDO_STACK_SIZE)
	{
		delete m_xUndoStack.GetFront();
		m_xUndoStack.Remove(0);
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
	// Audit §3.18 fix: resolve the entity's OWN scene so undo/redo survives
	// active-scene switches, DontDestroyOnLoad moves, and cross-scene edits.
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneDataForEntity(m_uEntityID);
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
	// Audit §3.18 fix: resolve the entity's OWN scene so undo/redo survives
	// active-scene switches, DontDestroyOnLoad moves, and cross-scene edits.
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneDataForEntity(m_uEntityID);
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

#endif // ZENITH_TOOLS
