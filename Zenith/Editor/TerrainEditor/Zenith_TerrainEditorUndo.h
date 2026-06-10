#pragma once

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_UndoSystem.h"
#include "Editor/TerrainEditor/Zenith_TerrainEditor.h"
#include "Collections/Zenith_Vector.h"

//=============================================================================
// Zenith_UndoCommand_TerrainEdit
//
// One command per brush stroke (or auto-splat run): the bounding texel rect
// of everything the stroke touched on ONE map, with before/after byte copies.
// Execute() re-writes the after-region, Undo() the before-region — both via
// Zenith_TerrainEditor::WriteMapRegion, which re-marks dirty chunks / GPU
// flags so the visuals follow.
//
// The command references the engine-lifetime terrain editor subsystem (the
// undo stack is cleared on scene load / editor shutdown long before the
// subsystem dies). Byte footprint is reported to the editor's live-undo
// budget on construction/destruction.
//=============================================================================
class Zenith_UndoCommand_TerrainEdit : public Zenith_UndoCommand
{
public:
	Zenith_UndoCommand_TerrainEdit(Zenith_TerrainEditor& xEditor, Zenith_TerrainEditMap eMap,
		u_int uX0, u_int uY0, u_int uW, u_int uH,
		Zenith_Vector<u_int8>&& xBefore, Zenith_Vector<u_int8>&& xAfter);
	~Zenith_UndoCommand_TerrainEdit() override;

	void Execute() override;
	void Undo() override;
	const char* GetDescription() const override;

	u_int64 GetByteSize() const { return static_cast<u_int64>(m_xBefore.GetSize()) + m_xAfter.GetSize(); }

private:
	Zenith_TerrainEditor& m_xEditor;
	Zenith_TerrainEditMap m_eMap;
	u_int m_uX0, m_uY0, m_uW, m_uH;
	Zenith_Vector<u_int8> m_xBefore;
	Zenith_Vector<u_int8> m_xAfter;
};

#endif // ZENITH_TOOLS
