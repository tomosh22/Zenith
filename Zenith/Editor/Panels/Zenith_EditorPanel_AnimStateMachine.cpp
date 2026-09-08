#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Editor/Panels/Zenith_EditorPanel_AnimStateMachine.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorUI.h"
#include "Core/Zenith_EditorWindowNames.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

//=============================================================================
// Lifecycle, the automatic layout, and the hit-rect accessors.
//
// The DRAWING half lives in Zenith_EditorPanel_AnimStateMachine_Render.cpp and
// the OPERATIONS in ..._Ops.cpp — split because the graph editor's 1337-line
// single TU is the thing this panel is explicitly not repeating, and because
// "what the panel knows", "what the panel does" and "what the panel paints"
// are the three halves a reader wants separately.
//=============================================================================

Zenith_EditorPanel_AnimStateMachine& Zenith_EditorPanel_AnimStateMachine::Instance()
{
	Zenith_Assert(g_xEngine.HasEditor(),
		"Zenith_EditorPanel_AnimStateMachine::Instance() before the editor was allocated");
	Zenith_EditorPanel_AnimStateMachine* pxPanel = g_xEngine.Editor().TryGetAnimStateMachinePanel();
	Zenith_Assert(pxPanel != nullptr,
		"Zenith_EditorPanel_AnimStateMachine::Instance() outside Zenith_Editor::Initialise..Shutdown");
	return *pxPanel;
}

Zenith_EditorPanel_AnimStateMachine::Zenith_EditorPanel_AnimStateMachine()
{
}

Zenith_EditorPanel_AnimStateMachine::~Zenith_EditorPanel_AnimStateMachine()
{
	// Everything that owns an asset reference is dropped by Shutdown(), which
	// Zenith_Editor::Shutdown calls immediately before deleting this object and
	// strictly before Zenith_AssetRegistry::Shutdown. Calling it again here is
	// harmless and covers a stack-constructed panel in a unit.
	Shutdown();
}

void Zenith_EditorPanel_AnimStateMachine::Shutdown()
{
	DropPreview();
	m_xDocument.CloseDiscardingChanges();
	ClearFrameRects();
}

//=============================================================================
// Document lifecycle
//=============================================================================

bool Zenith_EditorPanel_AnimStateMachine::OpenAsset(const std::string& strAssetPath)
{
	m_strLastOpenAttemptPath = strAssetPath;
	m_eLastOpenResult = m_xDocument.Open(strAssetPath);
	if (m_eLastOpenResult != ZENITH_ANIMCTRLDOC_OPEN_OK)
	{
		// A refusal changes NOTHING and whatever was open stays open. The panel
		// still shows itself: every refusal has a UI answer (save / discard, or
		// a path to correct) and a silent no-op with the window hidden helps
		// nobody.
		m_bShow = true;
		return false;
	}
	OnDocumentOpened();
	return true;
}

bool Zenith_EditorPanel_AnimStateMachine::OpenAssetFresh(const std::string& strAssetPath)
{
	m_strLastOpenAttemptPath = strAssetPath;
	m_eLastOpenResult = m_xDocument.OpenFresh(strAssetPath);
	if (m_eLastOpenResult != ZENITH_ANIMCTRLDOC_OPEN_OK)
	{
		m_bShow = true;
		return false;
	}
	OnDocumentOpened();
	return true;
}

void Zenith_EditorPanel_AnimStateMachine::CloseAsset()
{
	DropPreview();
	m_xDocument.CloseDiscardingChanges();
	Action_ClearSelection();
	m_xAutoLayout.Clear();
	m_uAutoLayoutStateCount = 0;
	m_bCloseRefusedDirty = false;
	m_bExternalConflict = false;
	m_acPathBuffer[0] = '\0';
	m_acLayerNameBuffer[0] = '\0';
	m_acLayerRenameBuffer[0] = '\0';
	m_acLayerMaskPathBuffer[0] = '\0';
}

Zenith_AnimCtrlDocCloseResult Zenith_EditorPanel_AnimStateMachine::RequestCloseAsset()
{
	const Zenith_AnimCtrlDocCloseResult eResult = m_xDocument.Close();
	if (eResult == ZENITH_ANIMCTRLDOC_CLOSE_REFUSED_DIRTY)
	{
		m_bCloseRefusedDirty = true;
		return eResult;
	}
	DropPreview();
	Action_ClearSelection();
	m_xAutoLayout.Clear();
	m_uAutoLayoutStateCount = 0;
	m_bCloseRefusedDirty = false;
	return eResult;
}

void Zenith_EditorPanel_AnimStateMachine::OnDocumentOpened()
{
	m_bShow = true;
	m_bCloseRefusedDirty = false;
	m_bExternalConflict = false;
	m_strSelectedState.clear();
	m_bAnyStateSelected = false;
	m_strSelectedTransitionFrom.clear();
	m_uSelectedTransition = uINVALID_ANIMSM_TRANSITION;
	m_bHasTransitionSelection = false;
	m_fScrollX = 0.0f;
	m_fScrollY = 0.0f;
	m_strHighlightedState.clear();
	// The layer strip's edit fields belong to a SELECTION, and the selection has
	// just gone back to the top-level machine — leaving the previous document's
	// layer name in the buffer would offer it as the next "+ Layer".
	m_acLayerNameBuffer[0] = '\0';
	m_acLayerRenameBuffer[0] = '\0';
	m_acLayerMaskPathBuffer[0] = '\0';
	snprintf(m_acPathBuffer, sizeof(m_acPathBuffer), "%s", m_xDocument.GetAssetPath().c_str());
	RebuildAutoLayout();
	// The dope sheet's mask sub-panel targets whichever layer this panel has
	// selected, and that is now the top-level machine (OVERRIDE).
	PushSelectedLayerBlendModeToMaskPanel();
}

//=============================================================================
// The automatic layout
//
// ★ IT IS A FALLBACK, NOT A LAYOUT PASS. Only states whose serialized editor
// position is EXACTLY the origin get a grid slot; anything a human has dragged
// keeps what the file says. Writing the grid into the def instead would dirty
// a document nobody edited and rewrite a tracked .zanimctrl for a cosmetic
// reason, on every open.
//
// The slot order is GetStateNamesSorted's, which is a total order over names
// rather than Zenith_HashMap's slot order — so the same def lays out the same
// way twice, and a unit can predict where a node will be.
//=============================================================================

void Zenith_EditorPanel_AnimStateMachine::RebuildAutoLayout()
{
	m_xAutoLayout.Clear();
	m_uAutoLayoutMachineId = m_xDocument.GetSelectedMachineId();

	Zenith_Vector<std::string> axNames;
	m_xDocument.GetStateNamesSorted(axNames);
	m_uAutoLayoutStateCount = axNames.GetSize();

	u_int uSlot = 0;
	for (u_int u = 0; u < axNames.GetSize(); ++u)
	{
		Zenith_Maths::Vector2 xStored(0.0f);
		if (!m_xDocument.GetStateEditorPosition(axNames.Get(u), xStored))
		{
			continue;
		}
		if (xStored.x != 0.0f || xStored.y != 0.0f)
		{
			continue;   // authored — leave it alone
		}
		const u_int uColumn = uSlot % uANIMSM_LAYOUT_COLUMNS;
		const u_int uRow = uSlot / uANIMSM_LAYOUT_COLUMNS;
		m_xAutoLayout.Insert(axNames.Get(u), Zenith_Maths::Vector2(
			fANIMSM_LAYOUT_STEP_X_1X * static_cast<float>(uColumn) + 40.0f,
			fANIMSM_LAYOUT_STEP_Y_1X * static_cast<float>(uRow) + 40.0f));
		++uSlot;
	}
}

bool Zenith_EditorPanel_AnimStateMachine::GetNodePosition(const std::string& strStateName,
	Zenith_Maths::Vector2& xOut) const
{
	Zenith_Maths::Vector2 xStored(0.0f);
	if (!m_xDocument.GetStateEditorPosition(strStateName, xStored))
	{
		return false;
	}
	if (xStored.x != 0.0f || xStored.y != 0.0f)
	{
		xOut = xStored;
		return true;
	}
	const Zenith_Maths::Vector2* pxAuto = m_xAutoLayout.TryGet(strStateName);
	xOut = (pxAuto != nullptr) ? *pxAuto : Zenith_Maths::Vector2(40.0f, 40.0f);
	return true;
}

bool Zenith_EditorPanel_AnimStateMachine::ComputeNodeScreenRect(const CanvasLayout& xLayout,
	const std::string& strStateName, Zenith_AnimCtrlPanelRect& xOut) const
{
	// ★ THE ANY-STATE PSEUDO-NODE, AND IT IS THE ONE NODE WITH NO GRAPH POSITION.
	// An EMPTY name addresses the machine's any-state transition list on every
	// document verb, so it is what the edge pass, the hit test and the Ctrl-drag
	// all carry — but GetNodePosition asks the def for a state called "" and
	// rightly answers false. It is anchored to the CANVAS instead: same box, a
	// fixed inset from the bottom-left corner, scroll-independent, and out of
	// RebuildAutoLayout's slot 0 at the top-left. See fANIMSM_ANY_STATE_INSET_1X.
	if (strStateName.empty())
	{
		const float fInset = Zenith_EditorUI::Px(fANIMSM_ANY_STATE_INSET_1X);
		xOut.m_fMinX = xLayout.m_fLeft + fInset;
		xOut.m_fMaxY = xLayout.m_fBottom - fInset;
		xOut.m_fMaxX = xOut.m_fMinX + xLayout.m_fNodeWidth;
		xOut.m_fMinY = xOut.m_fMaxY - xLayout.m_fNodeHeight;
		return true;
	}

	Zenith_Maths::Vector2 xGraph(0.0f);
	if (!GetNodePosition(strStateName, xGraph))
	{
		return false;
	}
	// Graph units ARE pixels at 1x DPI, scrolled and DPI-scaled once, here.
	const float fScale = Zenith_EditorUI::GetUIScale();
	xOut.m_fMinX = xLayout.m_fLeft + (xGraph.x - m_fScrollX) * fScale;
	xOut.m_fMinY = xLayout.m_fTop + (xGraph.y - m_fScrollY) * fScale;
	xOut.m_fMaxX = xOut.m_fMinX + xLayout.m_fNodeWidth;
	xOut.m_fMaxY = xOut.m_fMinY + xLayout.m_fNodeHeight;
	return true;
}

//=============================================================================
// View
//=============================================================================

void Zenith_EditorPanel_AnimStateMachine::RequestWindowPlacement(float fScreenX, float fScreenY,
	float fWidth, float fHeight)
{
	m_bPlacementRequested = true;
	m_fPlacementX = fScreenX;
	m_fPlacementY = fScreenY;
	m_fPlacementWidth = fWidth;
	m_fPlacementHeight = fHeight;
}

void Zenith_EditorPanel_AnimStateMachine::SetCanvasScroll(float fX, float fY)
{
	m_fScrollX = fX;
	m_fScrollY = fY;
}

bool Zenith_EditorPanel_AnimStateMachine::ScrollStateIntoView(const std::string& strStateName)
{
	Zenith_Maths::Vector2 xPos(0.0f);
	if (!GetNodePosition(strStateName, xPos))
	{
		return false;
	}
	// ★ APPLIED BY THE NEXT Render, exactly like the dope sheet's
	// ScrollTimeIntoView and the graph editor's ScrollPaletteEntryIntoView: the
	// scroll it needs is a function of the canvas rect, and the canvas rect is
	// not known until the window has been laid out. Give it a frame before
	// reading a node rect.
	m_bPendingScrollToState = true;
	m_strPendingScrollState = strStateName;
	return true;
}

//=============================================================================
// Hit rects
//
// ★ THE OFF-SCREEN GATE. Every accessor funnels through PublishRect, and
// PublishRect answers FALSE for a rect whose centre is outside the display
// RECORDED WITH IT rather than handing out a coordinate no click can reach.
//=============================================================================

void Zenith_EditorPanel_AnimStateMachine::ClearFrameRects()
{
	m_xNodeRects.Clear();
	// The any-state pseudo-node goes with them. It is canvas-anchored rather than
	// graph-anchored, which makes it tempting to leave standing — but a frame that
	// drew no canvas at all (hidden, collapsed, unselected tab) drew no pseudo-node
	// either, and answering with the last one's box would hand out a coordinate no
	// click can reach.
	m_bAnyStateRectValid = false;
	m_xTransitionRects.Clear();
	m_axRectOwnerOrder.Clear();
	m_bCanvasRectValid = false;
	// WU-7.2's strip diagnostics go with them, and for the same reason: a frame
	// that drew nothing must not keep answering with the last one's facts.
	m_bLayerStripDrawn = false;
	m_uDrawnLayerRows = 0;

	// WU-7.3's strip, ditto — INCLUDING the axis range, which is half of the
	// pixel<->position mapping. A drag mapped through a range the frame did not
	// draw would drop the point somewhere the user never saw, which is the same
	// class of failure the off-screen gate exists to prevent.
	m_bBlendStripDrawn = false;
	m_bBlendStripRectValid = false;
	m_bLiveDotRectValid = false;
	m_xBlendPointRects.Clear();
	m_fBlendRangeMinX = 0.0f;
	m_fBlendRangeMaxX = 0.0f;
	m_fBlendRangeMinY = 0.0f;
	m_fBlendRangeMaxY = 0.0f;

	// The display bound goes with them: a frame that recorded nothing must not
	// leave a bound behind that the next query would judge a stale rect against.
	m_fRecordedDisplayWidth = 0.0f;
	m_fRecordedDisplayHeight = 0.0f;
}

bool Zenith_EditorPanel_AnimStateMachine::PublishRect(const Zenith_AnimCtrlPanelRect* pxRect,
	Zenith_AnimCtrlPanelRect& xOut) const
{
	if (pxRect == nullptr)
	{
		return false;
	}
	// ★ THE BOUND IS THE ONE CAPTURED AT RECORD TIME, NEVER ImGui::GetIO() HERE.
	// Reading it live works in the editor (queries happen mid-frame) and is
	// wrong everywhere else: ImGui initialises DisplaySize to (-1, -1) and only
	// a backend NewFrame fills it in, so any query made outside a frame — which
	// is every unit assertion — would compare each centre against -1 and refuse
	// it, reporting "nothing was published" while the panel drew perfectly.
	const Zenith_Maths::Vector2 xCentre = pxRect->Centre();
	if (!(xCentre.x >= 0.0f && xCentre.y >= 0.0f
	   && xCentre.x <= m_fRecordedDisplayWidth && xCentre.y <= m_fRecordedDisplayHeight))
	{
		return false;
	}
	xOut = *pxRect;
	return true;
}

u_int64 Zenith_EditorPanel_AnimStateMachine::MakeTransitionRectKey(u_int uOwnerIndex, u_int uTransitionIndex)
{
	return (static_cast<u_int64>(uOwnerIndex) << 32) | static_cast<u_int64>(uTransitionIndex);
}

bool Zenith_EditorPanel_AnimStateMachine::GetStateNodeRect(const std::string& strStateName,
	Zenith_AnimCtrlPanelRect& xOut) const
{
	return PublishRect(m_xNodeRects.TryGet(strStateName), xOut);
}

bool Zenith_EditorPanel_AnimStateMachine::GetAnyStateRect(Zenith_AnimCtrlPanelRect& xOut) const
{
	if (!m_bAnyStateRectValid)
	{
		return false;
	}
	return PublishRect(&m_xAnyStateRect, xOut);
}

bool Zenith_EditorPanel_AnimStateMachine::GetTransitionMidpointRect(const std::string& strFromState, u_int uIndex,
	Zenith_AnimCtrlPanelRect& xOut) const
{
	// The owner index is the position the frame RECORDED the owner at — a name
	// lookup rather than a re-derivation, so the key cannot drift from the one
	// the draw used.
	u_int uOwnerIndex = 0xFFFFFFFFu;
	for (u_int u = 0; u < m_axRectOwnerOrder.GetSize(); ++u)
	{
		if (m_axRectOwnerOrder.Get(u) == strFromState)
		{
			uOwnerIndex = u;
			break;
		}
	}
	if (uOwnerIndex == 0xFFFFFFFFu)
	{
		return false;
	}
	return PublishRect(m_xTransitionRects.TryGet(MakeTransitionRectKey(uOwnerIndex, uIndex)), xOut);
}

bool Zenith_EditorPanel_AnimStateMachine::GetCanvasRect(Zenith_AnimCtrlPanelRect& xOut) const
{
	if (!m_bCanvasRectValid)
	{
		return false;
	}
	return PublishRect(&m_xCanvasRect, xOut);
}

bool Zenith_EditorPanel_AnimStateMachine::GetBlendStripRect(Zenith_AnimCtrlPanelRect& xOut) const
{
	if (!m_bBlendStripRectValid)
	{
		return false;
	}
	return PublishRect(&m_xBlendStripRect, xOut);
}

bool Zenith_EditorPanel_AnimStateMachine::GetBlendPointRect(u_int uIndex, Zenith_AnimCtrlPanelRect& xOut) const
{
	return PublishRect(m_xBlendPointRects.TryGet(uIndex), xOut);
}

bool Zenith_EditorPanel_AnimStateMachine::GetLiveDotRect(Zenith_AnimCtrlPanelRect& xOut) const
{
	if (!m_bLiveDotRectValid)
	{
		return false;
	}
	return PublishRect(&m_xLiveDotRect, xOut);
}

bool Zenith_EditorPanel_AnimStateMachine::GetBlendAxisRange(Zenith_AnimCtrlBlendAxis eAxis,
	float& fOutMin, float& fOutMax) const
{
	if (!m_bBlendStripDrawn)
	{
		return false;
	}
	fOutMin = (eAxis == ZENITH_ANIMCTRL_BLEND_AXIS_X) ? m_fBlendRangeMinX : m_fBlendRangeMinY;
	fOutMax = (eAxis == ZENITH_ANIMCTRL_BLEND_AXIS_X) ? m_fBlendRangeMaxX : m_fBlendRangeMaxY;
	return true;
}

//=============================================================================
// The blend strip's PURE mapping (WU-7.3).
//
// ★ NO COORDINATE MATHS LIVES ANYWHERE ELSE, which is the dope sheet's rule
// (Zenith_AnimTimelineMath) applied to a blend axis. The draw places a marker at
// BlendPositionToPixel, the drag reads a position back with BlendPixelToPosition
// and the units assert the round trip — one definition, so the strip cannot grow
// a second copy that drifts from the first and puts a marker where a click does
// not land.
//=============================================================================

float Zenith_EditorPanel_AnimStateMachine::BlendPositionToPixel(float fPosition, float fPixelMin, float fPixelMax,
	float fPositionMin, float fPositionMax)
{
	const float fSpan = fPositionMax - fPositionMin;
	// A degenerate range maps everything to the middle. It cannot be divided
	// through, and answering the low edge would put every marker on the frame.
	if (!(fSpan > 1.0e-6f))
	{
		return (fPixelMin + fPixelMax) * 0.5f;
	}
	return fPixelMin + (fPosition - fPositionMin) / fSpan * (fPixelMax - fPixelMin);
}

float Zenith_EditorPanel_AnimStateMachine::BlendPixelToPosition(float fPixel, float fPixelMin, float fPixelMax,
	float fPositionMin, float fPositionMax)
{
	const float fPixelSpan = fPixelMax - fPixelMin;
	if (!(fPixelSpan > 1.0e-6f))
	{
		return fPositionMin;
	}
	return fPositionMin + (fPixel - fPixelMin) / fPixelSpan * (fPositionMax - fPositionMin);
}

void Zenith_EditorPanel_AnimStateMachine::ComputeBlendAxisRange(float fPointMin, float fPointMax,
	float& fOutMin, float& fOutMax)
{
	// A non-finite input is treated as an empty space rather than propagated: the
	// range is what every marker's pixel is derived from, so one NaN here would
	// take the whole strip with it.
	if (!(fPointMin <= fPointMax) || !(fPointMin >= -3.0e38f) || !(fPointMax <= 3.0e38f))
	{
		fOutMin = -1.0f;
		fOutMax = 1.0f;
		return;
	}

	const float fPad = (fPointMax - fPointMin) * fANIMSM_BLEND_RANGE_PAD;
	fOutMin = fPointMin - fPad;
	fOutMax = fPointMax + fPad;

	// ★ THE MINIMUM SPAN IS NOT COSMETIC. One point (or every point at the same
	// position) gives a zero span, and a zero span makes the strip a surface on
	// which every pixel means the same position — a drag that cannot move
	// anything, which reads as "dragging is broken".
	const float fSpan = fOutMax - fOutMin;
	if (fSpan < fANIMSM_BLEND_MIN_SPAN)
	{
		const float fCentre = (fOutMin + fOutMax) * 0.5f;
		fOutMin = fCentre - fANIMSM_BLEND_MIN_SPAN * 0.5f;
		fOutMax = fCentre + fANIMSM_BLEND_MIN_SPAN * 0.5f;
	}
}

//=============================================================================
// The blend-tree gate
//=============================================================================

Zenith_AnimCtrlStateTreeKind Zenith_EditorPanel_AnimStateMachine::GetStateTreeKind(const std::string& strStateName) const
{
	return m_xDocument.GetStateTreeKind(strStateName);
}

const char* Zenith_EditorPanel_AnimStateMachine::BlendTreeRefusalText()
{
	// ★ FORWARDED, NOT RESTATED. The document sets the same string into its own
	// diagnostic when it refuses, so the node badge, the inspector, the strip and
	// the units all read one wording — the rule WU-7.2 applied to the additive
	// mask notice, applied here.
	return Zenith_AnimControllerDocument::BlendTreeRefusalText();
}

#ifdef ZENITH_TESTING
#include "Editor/Panels/Zenith_EditorPanel_AnimStateMachine.Tests.inl"
#endif

#endif // ZENITH_TOOLS
