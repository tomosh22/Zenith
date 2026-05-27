#pragma once

#ifdef ZENITH_TOOLS

#include "Maths/Zenith_Maths.h"
#include "EntityComponent/Zenith_Scene.h"
#include "Collections/Zenith_Vector.h"
#include <string>

//------------------------------------------------------------------------------
// Base Command Interface
//------------------------------------------------------------------------------
// All undo commands must inherit from this base class and implement
// Execute(), Undo(), and GetDescription()
//------------------------------------------------------------------------------

class Zenith_UndoCommand
{
public:
	Zenith_UndoCommand() = default;
	virtual ~Zenith_UndoCommand() = default;

	// Execute the command (modifies scene state)
	virtual void Execute() = 0;

	// Undo the command (restores previous state)
	virtual void Undo() = 0;

	// Get human-readable description for UI
	virtual const char* GetDescription() const = 0;

	// Audit §3.18 note: the base class no longer stores a Zenith_Scene captured
	// at construction time. That approach broke across active-scene switches —
	// a transform edit on an entity in Scene A, followed by SetActiveScene(B),
	// followed by Ctrl+Z, would look for the entity in Scene A via the stored
	// handle — but if the handle was replaced by a fresh active scene during
	// an unload/reload cycle, the lookup failed silently. Derived commands now
	// resolve the target scene dynamically via GetSceneDataForEntity(EntityID),
	// using the entity's globally-unique ID + generation counter. Matches
	// Unity's GameObject.scene pattern: objects carry their scene intrinsically.
	// Ref: https://docs.unity3d.com/ScriptReference/GameObject-scene.html
};

//------------------------------------------------------------------------------
// Transform Edit Command
//------------------------------------------------------------------------------
// Records transform changes (position, rotation, scale) for a single entity
//------------------------------------------------------------------------------

class Zenith_UndoCommand_TransformEdit : public Zenith_UndoCommand
{
public:
	Zenith_UndoCommand_TransformEdit(
		Zenith_EntityID uEntityID,
		const Zenith_Maths::Vector3& xOldPosition,
		const Zenith_Maths::Quat& xOldRotation,
		const Zenith_Maths::Vector3& xOldScale,
		const Zenith_Maths::Vector3& xNewPosition,
		const Zenith_Maths::Quat& xNewRotation,
		const Zenith_Maths::Vector3& xNewScale
	);

	void Execute() override;
	void Undo() override;
	const char* GetDescription() const override;

private:
	Zenith_EntityID m_uEntityID;
	Zenith_Maths::Vector3 m_xOldPosition;
	Zenith_Maths::Quat m_xOldRotation;
	Zenith_Maths::Vector3 m_xOldScale;
	Zenith_Maths::Vector3 m_xNewPosition;
	Zenith_Maths::Quat m_xNewRotation;
	Zenith_Maths::Vector3 m_xNewScale;
};

//------------------------------------------------------------------------------
// Entity Creation Command
//------------------------------------------------------------------------------
// Records entity creation for undo/redo
// Execute: Creates entity with saved state
// Undo: Removes entity from scene
//------------------------------------------------------------------------------

class Zenith_UndoCommand_CreateEntity : public Zenith_UndoCommand
{
public:
	Zenith_UndoCommand_CreateEntity(
		Zenith_EntityID uEntityID,
		const std::string& strName
	);

	void Execute() override;
	void Undo() override;
	const char* GetDescription() const override;

private:
	Zenith_EntityID m_uEntityID;
	std::string m_strName;
	bool m_bCreated; // Track if entity currently exists
};

//------------------------------------------------------------------------------
// Entity Deletion Command
//------------------------------------------------------------------------------
// Records entity deletion for undo/redo
// CRITICAL: Must serialize entity state before deletion
// Execute: Removes entity from scene
// Undo: Recreates entity from serialized state
//------------------------------------------------------------------------------

class Zenith_UndoCommand_DeleteEntity : public Zenith_UndoCommand
{
public:
	// Constructor captures entity state before deletion
	explicit Zenith_UndoCommand_DeleteEntity(Zenith_EntityID uEntityID);

	void Execute() override;
	void Undo() override;
	const char* GetDescription() const override;

private:
	Zenith_EntityID m_uEntityID;
	std::string m_strName;
	std::string m_strSerializedState; // Full entity serialization (for complex undo)
	bool m_bDeleted; // Track if entity currently deleted
};

//------------------------------------------------------------------------------
// Undo System
//------------------------------------------------------------------------------
// Command pattern-based undo/redo system with limited history
//
// Usage:
//   1. Create command object
//   2. Call g_xEngine.UndoSystem().Execute(command)
//   3. System executes command and pushes to undo stack
//   4. User presses Ctrl+Z → g_xEngine.UndoSystem().Undo()
//   5. User presses Ctrl+Y → g_xEngine.UndoSystem().Redo()
//
// Thread Safety: NOT thread-safe - must be called from main thread only
//------------------------------------------------------------------------------

class Zenith_UndoSystem
{
public:
	// Execute command and add to undo stack
	// Clears redo stack (branching timeline)
void Execute(Zenith_UndoCommand* pCommand);

	// Undo last command (if available)
	// Moves command from undo stack to redo stack
void Undo();

	// Redo last undone command (if available)
	// Moves command from redo stack to undo stack
void Redo();

	// Query stack state
bool CanUndo();
bool CanRedo();

	// Get description of next undo/redo operation
const char* GetUndoDescription();
const char* GetRedoDescription();

	// Clear all history (e.g., on scene load)
	// Frees all command objects
void Clear();

	// Get stack sizes (for debugging/UI)
u_int GetUndoStackSize();
u_int GetRedoStackSize();

	// Configuration
	static constexpr u_int MAX_UNDO_STACK_SIZE = 100;

	// ===== Data members (was Zenith_UndoSystem) =====
	Zenith_Vector<Zenith_UndoCommand*> m_xUndoStack;
	Zenith_Vector<Zenith_UndoCommand*> m_xRedoStack;

private:
	// Helper: Enforce max stack size (remove oldest commands)
	void EnforceStackLimit();
};

#endif // ZENITH_TOOLS
