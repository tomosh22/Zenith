#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Editor/TerrainEditor/Zenith_TerrainEditorUndo.h"

Zenith_UndoCommand_TerrainEdit::Zenith_UndoCommand_TerrainEdit(Zenith_TerrainEditor& xEditor,
	Zenith_TerrainEditMap eMap, u_int uX0, u_int uY0, u_int uW, u_int uH,
	Zenith_Vector<u_int8>&& xBefore, Zenith_Vector<u_int8>&& xAfter)
	: m_xEditor(xEditor)
	, m_eMap(eMap)
	, m_uX0(uX0)
	, m_uY0(uY0)
	, m_uW(uW)
	, m_uH(uH)
	, m_xBefore(std::move(xBefore))
	, m_xAfter(std::move(xAfter))
{
	m_xEditor.OnUndoCommandAllocated(GetByteSize());
}

Zenith_UndoCommand_TerrainEdit::~Zenith_UndoCommand_TerrainEdit()
{
	m_xEditor.OnUndoCommandFreed(GetByteSize());
}

void Zenith_UndoCommand_TerrainEdit::Execute()
{
	m_xEditor.WriteMapRegion(m_eMap, m_uX0, m_uY0, m_uW, m_uH, m_xAfter.GetDataPointer());
}

void Zenith_UndoCommand_TerrainEdit::Undo()
{
	m_xEditor.WriteMapRegion(m_eMap, m_uX0, m_uY0, m_uW, m_uH, m_xBefore.GetDataPointer());
}

const char* Zenith_UndoCommand_TerrainEdit::GetDescription() const
{
	switch (m_eMap)
	{
	case Zenith_TerrainEditMap::Height:       return "Terrain Sculpt";
	case Zenith_TerrainEditMap::Splat:        return "Terrain Paint";
	case Zenith_TerrainEditMap::GrassDensity: return "Grass Density Paint";
	default:                                  return "Terrain Edit";
	}
}

#endif // ZENITH_TOOLS
