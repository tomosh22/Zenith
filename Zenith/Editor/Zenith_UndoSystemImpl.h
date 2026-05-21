#pragma once

#ifdef ZENITH_TOOLS

#include "Collections/Zenith_Vector.h"
#include "Editor/Zenith_UndoSystem.h"

// Phase 5.5d: per-Engine undo / redo command stacks.
class Zenith_UndoSystemImpl
{
public:
	Zenith_UndoSystemImpl() = default;
	~Zenith_UndoSystemImpl() = default;

	Zenith_UndoSystemImpl(const Zenith_UndoSystemImpl&) = delete;
	Zenith_UndoSystemImpl& operator=(const Zenith_UndoSystemImpl&) = delete;

	Zenith_Vector<Zenith_UndoCommand*> m_xUndoStack;
	Zenith_Vector<Zenith_UndoCommand*> m_xRedoStack;
};

#endif // ZENITH_TOOLS
