#pragma once

#ifdef ZENITH_TOOLS

#include "Maths/Zenith_Maths.h"
#include "EntityComponent/Zenith_Scene.h"
#include <vector>
#include <string>
#include <memory>

//------------------------------------------------------------------------------
// Base Command Interface
//------------------------------------------------------------------------------
// All undo commands must inherit from this base class and implement
// Execute(), Undo(), and GetDescription()
//------------------------------------------------------------------------------

class Zenith_UndoCommand
{
public:
	virtual ~Zenith_UndoCommand() = default;

	// Execute the command (modifies scene state)
	virtual void Execute() = 0;

	// Undo the command (restores previous state)
	virtual void Undo() = 0;

	// Get human-readable description for UI
	virtual const char* GetDescription() const = 0;
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
//   2. Call Zenith_UndoSystem::Execute(command)
//   3. System executes command and pushes to undo stack
//   4. User presses Ctrl+Z → Zenith_UndoSystem::Undo()
//   5. User presses Ctrl+Y → Zenith_UndoSystem::Redo()
//
// Thread Safety: NOT thread-safe - must be called from main thread only
//------------------------------------------------------------------------------

class Zenith_UndoSystem
{
public:
	// Execute command and add to undo stack
	// Clears redo stack (branching timeline)
	static void Execute(Zenith_UndoCommand* pCommand);

	// Undo last command (if available)
	// Moves command from undo stack to redo stack
	static void Undo();

	// Redo last undone command (if available)
	// Moves command from redo stack to undo stack
	static void Redo();

	// Query stack state
	static bool CanUndo();
	static bool CanRedo();

	// Get description of next undo/redo operation
	static const char* GetUndoDescription();
	static const char* GetRedoDescription();

	// Clear all history (e.g., on scene load)
	// Frees all command objects
	static void Clear();

	// Get stack sizes (for debugging/UI)
	static u_int GetUndoStackSize() { return static_cast<u_int>(s_xUndoStack.size()); }
	static u_int GetRedoStackSize() { return static_cast<u_int>(s_xRedoStack.size()); }

	// Configuration
	static constexpr u_int MAX_UNDO_STACK_SIZE = 100;

private:
	// Undo stack: Commands that can be undone (most recent at back)
	static std::vector<std::unique_ptr<Zenith_UndoCommand>> s_xUndoStack;

	// Redo stack: Commands that can be redone (most recent at back)
	static std::vector<std::unique_ptr<Zenith_UndoCommand>> s_xRedoStack;

	// Helper: Enforce max stack size (remove oldest commands)
	static void EnforceStackLimit();
};

#endif // ZENITH_TOOLS
