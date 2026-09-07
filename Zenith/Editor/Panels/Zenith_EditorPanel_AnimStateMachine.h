#pragma once

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_AnimControllerDocument.h"
#include "Flux/MeshAnimation/Flux_AnimationController.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Collections/Zenith_Vector.h"
#include "Collections/Zenith_HashMap.h"
#include "Maths/Zenith_Maths.h"
#include <string>

// Forward-declared rather than included: this header is pulled into
// Zenith_Editor.cpp and the panel's own three TUs, and none of its
// DECLARATIONS need an ImGui type. imgui.h forward-declares it the same way.
struct ImDrawList;

//=============================================================================
// Zenith_EditorPanel_AnimStateMachine (WU-6.5) — the STATE-MACHINE GRAPH.
//
// One dockable window over ONE Zenith_AnimControllerDocument: states as nodes,
// transitions as edges, a parameter list, a condition editor, and live
// active-state highlighting from a panel-owned preview controller.
//
// The class is spread over THREE TUs, split by what a reader wants separately:
//   Zenith_EditorPanel_AnimStateMachine.cpp        — lifecycle, layout, rects
//   Zenith_EditorPanel_AnimStateMachine_Ops.cpp    — the Action_* verbs and the
//                                                    preview, with not one line
//                                                    of ImGui in them
//   Zenith_EditorPanel_AnimStateMachine_Render.cpp — the drawing, and input
//                                                    TRANSLATION
//
// ★ IT IS A CLASS AND IT HAS UNDO, WHICH IS EXACTLY WHAT IT DOES NOT COPY FROM
// THE GRAPH EDITOR. Zenith_EditorPanel_GraphEditor keeps its whole state in one
// file-scope aggregate, which is why it holds ONE asset open and has no undo at
// all — neither is a decision anybody made, both are consequences of the
// storage. Everything here is a member, and every edit goes through
// Zenith_AnimControllerDocument's verbs, so a second graph window is a second
// object and a Ctrl+Z is one document command.
//
// ★ WHAT IT *DOES* COPY FROM THE GRAPH EDITOR IS THE HIT-RECT CONTRACT, which
// that panel learned the expensive way: every position accessor here returns
// FALSE for something off screen, and "the display" is the bound captured WHEN
// THE RECT WAS RECORDED — never ImGui::GetIO().DisplaySize re-read at query
// time. ImGui initialises DisplaySize to (-1, -1) and only a backend NewFrame
// fills it in, so a query made outside a frame — which is every unit assertion,
// since a test's frame must be closed before its result can be inspected —
// would compare each centre against -1 and refuse it. WasCanvasDrawnLastFrame /
// GetRecordedDisplayWidth exist to tell that failure apart from "nothing was
// drawn".
//
// ★ DRAW-LIST DECORATIONS, NOT ITEMS (Editor/CLAUDE.md). The canvas is ONE
// InvisibleButton with every node, edge, arrowhead and label painted into the
// window's ImDrawList at absolute screen coordinates. Placing items with
// SetCursorScreenPos and restoring the cursor trips
// ErrorCheckUsingSetCursorPosToExtendParentBoundaries, which in a windowed
// build is a modal CRT dialog nothing logs: the process simply hangs.
//
// ★ SCOPE. The def's top-level machine AND a selected LAYER's machine — the
// "Layers" STRIP chooses which (WU-7.2 replaced WU-6.5's bare dropdown with the
// list, because "which machine am I editing" and "what are this controller's
// layers" were always one question). A state's blend tree is a SINGLE CLIP LEAF
// here, and a state holding anything else is refused with BlendTreeRefusalText()
// rather than flattened; the sub-graph editor is WU-7.3.
//=============================================================================

//-----------------------------------------------------------------------------
// A rect in ABSOLUTE ImGui screen coordinates — the same space
// ImGui::GetItemRectMin/Max and Zenith_InputSimulator's mouse position live in,
// so a recorded rect can be clicked without a further transform.
//
// Its own type rather than the dope sheet's Zenith_AnimPanelRect: taking that
// one would drag Zenith_EditorPanel_Animation.h (a document, a preview session,
// a pose-ring vocabulary) into every TU that includes this header, to reuse
// four floats.
//-----------------------------------------------------------------------------
struct Zenith_AnimCtrlPanelRect
{
	float m_fMinX = 0.0f;
	float m_fMinY = 0.0f;
	float m_fMaxX = 0.0f;
	float m_fMaxY = 0.0f;

	Zenith_Maths::Vector2 Centre() const
	{
		return Zenith_Maths::Vector2((m_fMinX + m_fMaxX) * 0.5f, (m_fMinY + m_fMaxY) * 0.5f);
	}
	float Width() const { return m_fMaxX - m_fMinX; }
	float Height() const { return m_fMaxY - m_fMinY; }
};

// The node box, at 1x DPI. Public because the auto-layout, the hit test and the
// units all measure against it and a second copy is how a preview ends up
// showing a position the drop does not produce.
constexpr float fANIMSM_NODE_WIDTH_1X  = 148.0f;
constexpr float fANIMSM_NODE_HEIGHT_1X = 46.0f;
// The grid an UNPLACED state is laid out on (see GetNodePosition).
constexpr float fANIMSM_LAYOUT_STEP_X_1X = 200.0f;
constexpr float fANIMSM_LAYOUT_STEP_Y_1X = 92.0f;
constexpr u_int uANIMSM_LAYOUT_COLUMNS   = 4u;

// "No transition selected." Same shape as every other invalid-index sentinel in
// the editor: a bool beside the index would give one fact two representations
// that can disagree.
constexpr u_int uINVALID_ANIMSM_TRANSITION = 0xFFFFFFFFu;
// "No blend point selected" (WU-7.3), same shape and for the same reason.
constexpr u_int uINVALID_ANIMSM_BLEND_POINT = 0xFFFFFFFFu;

// The blend-space strip, at 1x DPI. Public because the pixel<->position mapping,
// the hit rects and the units all measure against it — a second copy is exactly
// how a marker ends up drawn somewhere a click does not land.
constexpr float fANIMSM_BLEND_CANVAS_SIZE_1X = 104.0f;
// Half the side of a point marker's hit square.
constexpr float fANIMSM_BLEND_MARKER_HALF_1X = 5.0f;
// The padding a strip leaves around the outermost points, as a FRACTION of the
// span they cover — so a space whose points sit at 0 and 1 is not drawn with its
// two markers welded to the frame.
constexpr float fANIMSM_BLEND_RANGE_PAD = 0.15f;
// The smallest span an axis is ever drawn with. Without it, a space whose points
// are all at one position maps every pixel to that position and the strip
// becomes an infinitely sensitive drag surface.
constexpr float fANIMSM_BLEND_MIN_SPAN = 0.5f;

//=============================================================================
// The panel.
//=============================================================================
class Zenith_EditorPanel_AnimStateMachine
{
public:
	Zenith_EditorPanel_AnimStateMachine();
	~Zenith_EditorPanel_AnimStateMachine();

	// ★ NON-COPYABLE: the document's undo commands hold a raw pointer back at
	// it, and the preview controller owns asset handles.
	Zenith_EditorPanel_AnimStateMachine(const Zenith_EditorPanel_AnimStateMachine&) = delete;
	Zenith_EditorPanel_AnimStateMachine& operator=(const Zenith_EditorPanel_AnimStateMachine&) = delete;

	// The editor's single panel.
	//
	// ★ IT IS OWNED BY Zenith_Editor, NOT BY A FUNCTION-LOCAL STATIC, and that
	// is the lifetime fix the dope sheet already made: a static's destructor
	// runs at ATEXIT, after Zenith_AssetRegistry::Shutdown has force-deleted
	// every asset, so the preview controller's animation handles would Release
	// into freed memory. The object is new'd in Zenith_Editor::Initialise and
	// Shutdown()+delete'd in Zenith_Editor::Shutdown, both inside the registry's
	// lifetime. A unit builds its OWN panel on the stack.
	static Zenith_EditorPanel_AnimStateMachine& Instance();

	//-------------------------------------------------------------------------
	// Frame
	//-------------------------------------------------------------------------

	// Composes the window. fDtSeconds advances the PREVIEW when it is enabled
	// and running; the caller decides what that is, which is how the editor's
	// Paused mode is honoured without the panel reaching for editor state.
	//
	// Every rect map is cleared at the top of this, whether or not the window is
	// drawn, so a hidden / collapsed / unselected-tab panel reports NO rects
	// rather than last frame's.
	void Render(float fDtSeconds);

	// Drops the document and every preview asset reference while the registry is
	// still up. Called from Zenith_Editor::Shutdown immediately before it
	// DELETES this object.
	void Shutdown();

	bool& ShowFlag() { return m_bShow; }
	bool IsShown() const { return m_bShow; }

	//-------------------------------------------------------------------------
	// Document lifecycle
	//-------------------------------------------------------------------------

	// Open an existing .zanimctrl. False on any refusal; the reason is in
	// GetLastOpenResult().
	bool OpenAsset(const std::string& strAssetPath);
	// Open an EMPTY controller targeted at that path — the boot-time authoring
	// entry point, the twin of Zenith_GraphEditorPanel::OpenAssetFresh.
	bool OpenAssetFresh(const std::string& strAssetPath);
	// FORCED close: unsaved edits are discarded. What Shutdown and the tests use.
	void CloseAsset();
	// The Close BUTTON's route: refuses while dirty so the panel can prompt.
	Zenith_AnimCtrlDocCloseResult RequestCloseAsset();

	bool IsOpen() const { return m_xDocument.IsOpen(); }
	Zenith_AnimCtrlDocOpenResult GetLastOpenResult() const { return m_eLastOpenResult; }
	const std::string& GetLastOpenAttemptPath() const { return m_strLastOpenAttemptPath; }

	Zenith_AnimControllerDocument& Document() { return m_xDocument; }
	const Zenith_AnimControllerDocument& Document() const { return m_xDocument; }

	//-------------------------------------------------------------------------
	// View
	//-------------------------------------------------------------------------

	// Places the window deterministically on the NEXT Render (position, size,
	// expanded). What "open the state machine editor" uses, and what a unit uses
	// to get a known geometry regardless of any saved imgui.ini.
	void RequestWindowPlacement(float fScreenX, float fScreenY, float fWidth, float fHeight);

	// Canvas scroll, in graph units. Applied immediately (unlike the dope
	// sheet's scroll requests, this needs no laid-out track rect to resolve).
	void SetCanvasScroll(float fX, float fY);
	float GetCanvasScrollX() const { return m_fScrollX; }
	float GetCanvasScrollY() const { return m_fScrollY; }
	// Scroll so a state's node sits near the canvas centre. Applied by the NEXT
	// Render — the canvas size is not known until the window has been laid out —
	// so give it a frame before reading a rect.
	bool ScrollStateIntoView(const std::string& strStateName);

	// Where a node IS, in graph units: the state's serialized editor position
	// when it has one, and its slot in the automatic grid when it does not.
	//
	// ★ AN UNPLACED STATE IS LAID OUT BY THE PANEL AND NOT WRITTEN TO THE DEF.
	// Flux_AnimationState::m_xEditorPosition IS a serialized field, so a graph a
	// human has arranged survives a round trip with no side-car file and no
	// Zenith_EditorPrefs entry — but a def authored by a game's boot recipe has
	// every state at (0, 0), and silently writing a layout into it on OPEN would
	// dirty the document and rewrite a tracked asset for a cosmetic reason. The
	// grid is therefore a fallback the panel computes, and only a DRAG commits a
	// position (undoably).
	bool GetNodePosition(const std::string& strStateName, Zenith_Maths::Vector2& xOut) const;

	//-------------------------------------------------------------------------
	// Selection. Pure panel state: none of these touches the document.
	//-------------------------------------------------------------------------

	// False for a state the machine does not have — selecting one would leave a
	// name in the selection that no undo can bring back.
	bool Action_SelectState(const std::string& strStateName);
	// (from-state, index). An EMPTY from-state addresses the machine's any-state
	// list, exactly as it does on every document verb.
	bool Action_SelectTransition(const std::string& strFromState, u_int uIndex);
	// True iff there WAS a selection to clear.
	bool Action_ClearSelection();

	const std::string& GetSelectedStateName() const { return m_strSelectedState; }
	bool GetSelectedTransition(std::string& strOutFrom, u_int& uOutIndex) const;

	//=========================================================================
	// OPERATIONS.
	//
	// ★ EVERY GESTURE HAS A BOOL-RETURNING ATOMIC TWIN, exactly as the graph
	// editor's Action_* verbs and the dope sheet's do, and for the reason both
	// learned: a test (and the AddStep_AnimSm* authoring steps) must be able to
	// perform an operation WITHOUT synthesising input. Driving a node graph
	// through simulated clicks makes every failure arrive as "the state was not
	// created", with the click, the bridge, the hit rect and the operation all
	// suspects and none of them named.
	//
	// ★ AN ACTION NEVER READS ImGui STATE. Not the mouse, not the modifiers, not
	// the focus. That is what makes them callable from a unit with no frame open.
	//
	// ★ EVERY ONE OF THEM GOES THROUGH THE DOCUMENT'S VERBS. Nothing here
	// touches Flux_AnimatorControllerDef directly, because every mutation has to
	// mark dirty and push undo in the same breath.
	//
	// ★ THE BOOL FOLLOWS THE DOCUMENT'S ASSIGNMENT-vs-CREATION RULE (see
	// Zenith_AnimControllerDocument.h). An assignment verb — SetDefaultState,
	// SetStateClip, SetStatePosition, SetTransition{Duration,ExitTime,
	// Interruptible} — returns TRUE when the value asked for is already in
	// place, and pushes nothing. A creation or removal returns FALSE on a
	// duplicate or a miss. That is what lets AnimSmActionChecked assert on
	// EVERY verb without a per-verb exception list, which the dope sheet's
	// ANIM_POSE_SET_AUTO_KEY needed and is a thing a later verb can forget.
	//=========================================================================

	// uANIMCTRL_TOP_LEVEL_MACHINE, or a layer id. Clears the selection: a state
	// name means nothing in a different machine. Also pushes the newly selected
	// layer's BLEND MODE into the dope sheet's mask sub-panel — see
	// Action_SelectLayer.
	bool Action_SelectLayerMachine(u_int uLayerId);

	//=========================================================================
	// THE LAYER LIST (WU-7.2).
	//
	// ★ EVERY VERB TAKES A LAYER ID, NEVER AN INDEX (D43), and the one index in
	// the family is Action_MoveLayer's DESTINATION, which is a position by
	// definition. The document's rules apply unchanged: Add and Remove are
	// CREATION / REMOVAL and refuse a miss; everything else is an ASSIGNMENT and
	// returns true when the value asked for is already in place, pushing nothing.
	//=========================================================================

	// Appends a layer and SELECTS it (its machine becomes the canvas's), so the
	// obvious next gesture — give it a state — needs no second click. Returns
	// false when the document refused; the new id is then GetSelectedLayerId().
	bool Action_AddLayer(const std::string& strLayerName);
	bool Action_RemoveLayer(u_int uLayerId);
	bool Action_RenameLayer(u_int uLayerId, const std::string& strLayerName);
	bool Action_SetLayerWeight(u_int uLayerId, float fWeight);
	bool Action_SetLayerBlendMode(u_int uLayerId, Flux_LayerBlendMode eBlendMode);
	bool Action_SetLayerEmitEvents(u_int uLayerId, bool bEmitEvents);
	// Refused, with the additive notice in GetLayerNotice(), for a non-empty path
	// on an additive layer. The rule lives in
	// Zenith_BoneMaskDocument::LayerAcceptsMask and is not restated here.
	bool Action_SetLayerMaskAssetPath(u_int uLayerId, const std::string& strAssetPath);
	bool Action_MoveLayer(u_int uLayerId, u_int uNewIndex);

	// ★ SELECTING A LAYER DOES TWO THINGS, AND THE SECOND IS THE ONE THAT IS EASY
	// TO FORGET. It selects that layer's state machine on the canvas
	// (SelectMachine), and it pushes the layer's BLEND MODE into the dope sheet's
	// bone-mask sub-panel (Zenith_EditorPanel_Animation::
	// SetMaskTargetLayerBlendMode). Without the second, WU-7.1's section would go
	// on offering a mask assignment for whichever layer was last looked at — and
	// an additive layer ignores its mask entirely, so the failure is a mask
	// authored, saved, assigned and never consulted, with every gate green.
	//
	// Refuses uANIMCTRL_TOP_LEVEL_MACHINE and any id the def does not carry: the
	// top-level machine is not a layer and has no blend mode. Use
	// Action_SelectLayerMachine for it.
	bool Action_SelectLayer(u_int uLayerId);

	// The layer whose machine the canvas is showing, or uANIMCTRL_TOP_LEVEL_MACHINE.
	u_int GetSelectedLayerId() const;
	// Why the last layer verb refused, or empty — the document's diagnostic,
	// forwarded so the strip and a unit read one string.
	const std::string& GetLayerNotice() const;

	bool Action_AddState(const std::string& strStateName);
	bool Action_RemoveState(const std::string& strStateName);
	bool Action_RenameState(const std::string& strOldName, const std::string& strNewName);
	bool Action_SetDefaultState(const std::string& strStateName);
	// An EMPTY clip name clears the state's tree. Refused, with
	// BlendTreeRefusalText(), for a state whose tree is not a single clip leaf.
	bool Action_SetStateClip(const std::string& strStateName, const std::string& strClipName);
	bool Action_SetStatePosition(const std::string& strStateName, float fX, float fY);

	bool Action_AddTransition(const std::string& strFromState, const std::string& strToState);
	bool Action_RemoveTransition(const std::string& strFromState, u_int uIndex);
	bool Action_SetTransitionDuration(const std::string& strFromState, u_int uIndex, float fSeconds);
	bool Action_SetTransitionExitTime(const std::string& strFromState, u_int uIndex,
		bool bHasExitTime, float fNormalizedExitTime);
	bool Action_SetTransitionInterruptible(const std::string& strFromState, u_int uIndex, bool bInterruptible);
	bool Action_AddCondition(const std::string& strFromState, u_int uIndex,
		const std::string& strParameterName, Flux_TransitionCondition::CompareOp eCompareOp, float fThreshold);
	bool Action_RemoveCondition(const std::string& strFromState, u_int uIndex, u_int uConditionIndex);

	bool Action_AddParameter(const std::string& strName, Flux_AnimationParameters::ParamType eType, float fDefault);
	bool Action_RemoveParameter(const std::string& strName);

	bool Action_AddClipPath(const std::string& strAssetPath);
	bool Action_RemoveClipPath(const std::string& strAssetPath);

	bool Action_Undo();
	bool Action_Redo();

	// Writes the .zanimctrl and refreshes the live asset's def in place. False on
	// any refusal — including an external-modification conflict, which leaves the
	// file untouched and is reported by GetLastSaveResult().
	bool Action_Save();
	Zenith_AnimCtrlDocSaveResult GetLastSaveResult() const { return m_eLastSaveResult; }

	// ★ APPLY IS A RELOAD, NOT A REBUILD (D45). It hands the working def to
	// Flux_AnimationController::ReloadFromControllerDef, which carries the
	// current state (by NAME), the normalized time (with its state), matched
	// parameter values (by name AND type) and the layer weights (by layer ID)
	// across the edit. BuildFromControllerDef would snap the graph to its
	// default state at frame 0, which is precisely what makes an edit-while-
	// playing useless.
	//
	// Applies to the PANEL'S PREVIEW controller. There is no direct-play preview
	// in this panel to re-arm — that is the dope sheet's, and it owns its own.
	bool Action_Apply();

	//=========================================================================
	// LIVE ACTIVE-STATE HIGHLIGHTING.
	//
	// ★ IT RUNS ON A PANEL-OWNED PREVIEW CONTROLLER, NOT ON THE SELECTED
	// ENTITY'S, AND THAT IS A PREMISE CORRECTION RATHER THAN A CHOICE. The brief
	// asked to highlight from a selected entity whose Zenith_AnimatorComponent
	// was built FROM THIS ASSET PATH, "compare paths". There is no path to
	// compare: Zenith_AnimatorComponent::LoadControllerAsset acquires the asset,
	// calls BuildFromControllerDef and RECORDS NOTHING — the component's only
	// members are its parent entity and a cached controller pointer, and the
	// controller keeps clip handles but no controller-asset reference. Matching
	// on anything else available (a layer count, a state name) would highlight a
	// DIFFERENT character's graph and look right, which is worse than not
	// highlighting at all. Adding the field is an EntityComponent change this
	// unit does not own.
	//
	// ★ THE PREVIEW TICKS THE MACHINE, NOT THE CONTROLLER, and it needs no rig.
	// Flux_AnimationController::Update returns immediately without a
	// Flux_SkeletonInstance, so the panel drives the selected machine's own
	// Update against a zero-bone Zenith_SkeletonAsset. Transition evaluation,
	// exit times, trigger consumption and the blend-tree playheads all advance
	// exactly as they do in a game; only the POSE is empty, and a state-machine
	// graph does not draw one. That is what makes the highlight — and its unit —
	// work headless with no skeleton asset on disk.
	//=========================================================================

	// Build (or tear down) the preview controller from the working def. False
	// when there is nothing open, or when BuildFromControllerDef reported a
	// dangling clip or mask — the preview is still usable, and the panel says so.
	bool Action_SetPreviewEnabled(bool bEnabled);
	bool IsPreviewEnabled() const { return m_bPreviewEnabled; }
	// True iff the last build/reload resolved every clip and mask the def names.
	bool IsPreviewComplete() const { return m_bPreviewComplete; }

	// Advance the preview by fDtSeconds. False when the preview is off or the
	// selected machine has no runtime counterpart.
	bool Action_TickPreview(float fDtSeconds);

	// Drive the preview's LIVE parameter set — the controller's one set (D42),
	// which every machine under it reads.
	bool Action_SetPreviewFloat(const std::string& strName, float fValue);
	bool Action_SetPreviewInt(const std::string& strName, int32_t iValue);
	bool Action_SetPreviewBool(const std::string& strName, bool bValue);
	bool Action_SetPreviewTrigger(const std::string& strName);

	// The state the preview is IN, or empty. This is what the canvas rings.
	const std::string& GetHighlightedStateName() const { return m_strHighlightedState; }
	// The preview's runtime machine for the CURRENT selection, or null.
	Flux_AnimationStateMachine* GetPreviewMachine();
	Flux_AnimationController& PreviewController() { return m_xPreviewController; }

	//=========================================================================
	// THE BLEND-TREE SUB-GRAPH (WU-7.3).
	//
	// ★ THE SCOPE IS THE TWO BLEND SPACES, AND THE REFUSAL DID NOT GO AWAY — it
	// got SMALLER. WU-6.5 answered COMPLEX for a blend space, a composite and a
	// container alike and refused all three by name; a blend space is now its own
	// kind with an editor, and BlendTreeRefusalText() names what is left (a
	// Blend / Additive / Masked / Select nest, or a sub-machine). Those genuinely
	// have no editor here, and assigning anything to one would delete a sub-graph
	// and report success.
	//
	// ★ THE LIVE DOT IS THE REASON THIS UNIT IS WORTH SHIPPING, and it is only
	// meaningful because WU-6.1 repaired the binding (D48). Before that,
	// Flux_AnimationStateMachine::EvaluateState called Evaluate with no parameter
	// set at all, so a blend space in a running game sat frozen at its
	// deserialized literal and a dot drawn from it would never have moved.
	//
	// ★ EVERY VERB HERE IS ONE UNDO STEP, and the step is a whole-STATE snapshot.
	// A blend tree has no identity below the state — see
	// Zenith_AnimCtrlCommand_StateTree.
	//=========================================================================

	Zenith_AnimCtrlStateTreeKind GetStateTreeKind(const std::string& strStateName) const;
	// The ONE wording, so the node badge, the inspector and the units cannot
	// disagree about what a refusal says. Forwards to
	// Zenith_AnimControllerDocument::BlendTreeRefusalText, which is where the
	// document's own diagnostic takes it from.
	static const char* BlendTreeRefusalText();

	// SINGLE_CLIP / BLENDSPACE_1D / BLENDSPACE_2D. Converting CARRIES THE CLIPS
	// ACROSS (a clip leaf seeds the space's first point), and the undo restores
	// the previous tree byte for byte. ASSIGNMENT: the kind already in place is
	// satisfied.
	bool Action_SetStateTreeKind(const std::string& strStateName, Zenith_AnimCtrlStateTreeKind eKind);
	// Bind an axis to a DECLARED Float. Refused otherwise, with the reason in
	// GetBlendNotice(). An empty name UNBINDS.
	bool Action_SetBlendSpaceParameter(const std::string& strStateName, Zenith_AnimCtrlBlendAxis eAxis,
		const std::string& strParameterName);
	// fY is IGNORED on a 1D space — one position shape for both, so the strip,
	// the command and the automation payload each carry one.
	bool Action_AddBlendPoint(const std::string& strStateName, const std::string& strClipName,
		float fX, float fY);
	bool Action_RemoveBlendPoint(const std::string& strStateName, u_int uIndex);
	bool Action_SetBlendPointClip(const std::string& strStateName, u_int uIndex, const std::string& strClipName);
	// ★ A 1D EDIT CAN RENUMBER (the list is kept sorted), so this FOLLOWS THE
	// SELECTION to wherever the point ended up. A selection left on the index
	// would silently start naming the point that was dragged past.
	bool Action_SetBlendPointPosition(const std::string& strStateName, u_int uIndex, float fX, float fY);
	// Pure panel state. uINVALID_ANIMSM_BLEND_POINT clears it; an index the
	// selected state does not have is refused.
	bool Action_SelectBlendPoint(u_int uIndex);
	// The drag gesture, as a verb: maps a SCREEN pixel through the strip's own
	// mapping and commits the resulting position. False when the strip was not
	// drawn last frame — there is no mapping without one, and inventing a range
	// would drop the point somewhere the strip never showed.
	bool Action_DragBlendPointToPixel(u_int uIndex, float fPixelX, float fPixelY);

	u_int GetSelectedBlendPoint() const { return m_uSelectedBlendPoint; }
	// Why the last blend verb refused, or empty — the document's diagnostic,
	// forwarded so the strip and a unit read one string.
	const std::string& GetBlendNotice() const;

	// ★ THE LIVE PARAMETER DOT. The value of the bound parameter(s) read from the
	// PREVIEW controller's one live set (D42) — the same set WU-6.5's parameter
	// panel drives and the same one the preview's machines resolve their blend
	// positions through, so the dot and the pose cannot disagree. fOutY is 0 on a
	// 1D space and on a 2D space whose Y axis is unbound.
	//
	// False when there is no preview, when the selected state is not a blend
	// space, or when NEITHER axis is bound — an unbound space reads no parameter
	// at all, and a dot pinned at zero would look like a parameter sitting at
	// zero.
	bool GetLiveParameterDot(float& fOutX, float& fOutY);

	//-------------------------------------------------------------------------
	// The strip's PURE mapping, exposed so the drag, the draw and the units read
	// ONE definition — the dope sheet's Zenith_AnimTimelineMath rule applied to a
	// blend axis. No ImGui, no member state: a unit asserts the round trip
	// without a frame open.
	//-------------------------------------------------------------------------

	static float BlendPositionToPixel(float fPosition, float fPixelMin, float fPixelMax,
		float fPositionMin, float fPositionMax);
	static float BlendPixelToPosition(float fPixel, float fPixelMin, float fPixelMax,
		float fPositionMin, float fPositionMax);
	// The range an axis is DRAWN with, given the span its points cover: padded by
	// fANIMSM_BLEND_RANGE_PAD and widened to at least fANIMSM_BLEND_MIN_SPAN, so
	// a degenerate space (one point, or every point at the same position) still
	// has a finite axis to drag along.
	static void ComputeBlendAxisRange(float fPointMin, float fPointMax, float& fOutMin, float& fOutMax);

	//-------------------------------------------------------------------------
	// The strip's hit rects and diagnostics — the same off-screen contract as
	// every other accessor on this panel.
	//-------------------------------------------------------------------------

	bool GetBlendStripRect(Zenith_AnimCtrlPanelRect& xOut) const;
	bool GetBlendPointRect(u_int uIndex, Zenith_AnimCtrlPanelRect& xOut) const;
	bool GetLiveDotRect(Zenith_AnimCtrlPanelRect& xOut) const;
	// ★ THE STRIP DRAWS NOTHING UNLESS THE SELECTED STATE IS A BLEND SPACE — not
	// a header, not a disabled row. This is what a unit asserts the ABSENCE with,
	// rather than inferring it from a rect that is false for four other reasons.
	bool WasBlendStripDrawnLastFrame() const { return m_bBlendStripDrawn; }
	u_int GetDrawnBlendPointCount() const { return m_xBlendPointRects.GetSize(); }
	// The axis range the strip was DRAWN with last frame — what
	// Action_DragBlendPointToPixel maps through.
	bool GetBlendAxisRange(Zenith_AnimCtrlBlendAxis eAxis, float& fOutMin, float& fOutMax) const;

	//=========================================================================
	// Hit rects.
	//
	// ★ EVERY ONE RETURNS FALSE FOR SOMETHING THAT IS NOT ON SCREEN, judged
	// against the display bound captured WHEN THE RECT WAS RECORDED. See the
	// class comment; the graph editor paid for this contract by clicking screen
	// y=1768 on a 720-tall display and reporting only "the nodes were not
	// created".
	//=========================================================================

	bool GetStateNodeRect(const std::string& strStateName, Zenith_AnimCtrlPanelRect& xOut) const;
	// The midpoint of a transition's edge — what a click on an edge hit-tests
	// against, and where the condition count is painted.
	bool GetTransitionMidpointRect(const std::string& strFromState, u_int uIndex,
		Zenith_AnimCtrlPanelRect& xOut) const;
	bool GetCanvasRect(Zenith_AnimCtrlPanelRect& xOut) const;

	//=========================================================================
	// Diagnostics — DELIBERATELY UNGATED, and that is the point of them. Every
	// accessor above answers a flat `false` for four different situations: the
	// window was never drawn, the canvas had no room, the node was scrolled
	// away, or the rect fell outside the display. These four discriminate.
	//=========================================================================

	bool WasCanvasDrawnLastFrame() const { return m_bCanvasRectValid; }
	float GetRecordedDisplayWidth() const { return m_fRecordedDisplayWidth; }
	float GetRecordedDisplayHeight() const { return m_fRecordedDisplayHeight; }
	u_int GetRenderedFrameCount() const { return m_uRenderedFrames; }
	// How many state nodes were PAINTED inside the canvas last frame. Zero with
	// a non-zero state count means every node was culled, not that the graph is
	// empty.
	u_int GetDrawnNodeCount() const { return m_xNodeRects.GetSize(); }

	// ★ THE LAYER STRIP'S OWN "was it drawn" PAIR (WU-7.2), for the same reason
	// the dope sheet's mask section has one: a unit has to be able to assert the
	// ABSENCE of a block directly rather than infer it from a rect that is false
	// for four other reasons. The strip draws NOTHING — not a header, not a
	// disabled row — while no document is open.
	bool WasLayerStripDrawnLastFrame() const { return m_bLayerStripDrawn; }
	// Layer rows emitted last frame. The "Top-level" machine row is NOT one of
	// them: it is a machine, not a layer, and counting it would make an empty
	// controller report one layer.
	u_int GetDrawnLayerRowCount() const { return m_uDrawnLayerRows; }

	// Live drag state, so a test can tell "the drag never started" apart from
	// "the drag started and the drop was refused".
	bool IsDraggingNode() const { return m_bDraggingNode; }
	bool IsDraggingTransition() const { return m_bDraggingTransition; }
	const std::string& GetTransitionDragSource() const { return m_strTransitionDragFrom; }

private:
	//-------------------------------------------------------------------------
	// Per-frame canvas geometry, in absolute screen pixels. Computed ONCE at the
	// top of the canvas pass and handed to every helper, so no helper re-derives
	// a coordinate and no two of them can disagree.
	//-------------------------------------------------------------------------
	struct CanvasLayout
	{
		float m_fLeft = 0.0f;
		float m_fTop = 0.0f;
		float m_fRight = 0.0f;
		float m_fBottom = 0.0f;
		float m_fNodeWidth = 0.0f;
		float m_fNodeHeight = 0.0f;
	};

	static u_int64 MakeTransitionRectKey(u_int uOwnerIndex, u_int uTransitionIndex);

	void ClearFrameRects();
	void OnDocumentOpened();
	// The grid slot for every state that has no stored position, recomputed
	// whenever the state list or the machine selection changes.
	void RebuildAutoLayout();
	// The off-screen gate every accessor above runs through.
	bool PublishRect(const Zenith_AnimCtrlPanelRect* pxRect, Zenith_AnimCtrlPanelRect& xOut) const;

	// Ops helpers (Zenith_EditorPanel_AnimStateMachine_Ops.cpp).
	void RefreshHighlightedState();
	void DropPreview();
	// ★ THE ONE PLACE THE DOPE SHEET IS TOLD WHICH LAYER A MASK WOULD LAND ON.
	// Guarded: Zenith_EditorPanel_Animation::Instance() ASSERTS when the editor
	// has not allocated its panels, and a unit builds this panel on the stack
	// with no editor around it.
	void PushSelectedLayerBlendModeToMaskPanel();
	// The SELECTED layer's name / mask path, or empty for the top-level machine.
	// By value: they are copied straight into the strip's edit buffers, and a
	// reference into a layer the next edit rebuilds would not survive the copy.
	std::string CurrentLayerName() const;
	std::string CurrentLayerMaskPath() const;

	// ★ THE ONE PLACE A GRAPH POSITION BECOMES A SCREEN RECT. The edge pass, the
	// node pass, the hit test and the units all go through it, so "where the
	// node was painted" and "where a click lands on it" cannot be two
	// derivations that drift.
	bool ComputeNodeScreenRect(const CanvasLayout& xLayout, const std::string& strStateName,
		Zenith_AnimCtrlPanelRect& xOut) const;

	// Render helpers (Zenith_EditorPanel_AnimStateMachine_Render.cpp).
	void ApplyPendingScroll(const CanvasLayout& xLayout);
	void RenderToolbar();
	// WU-7.2. REPLACES the bare layer dropdown this panel shipped with: the
	// "Layers" strip is the machine picker AND the list, because they were always
	// one question. Draws not one item while no document is open.
	void RenderLayerStrip();
	void RenderLayerDetail(u_int uLayerId);
	void RenderParameterPanel();
	void RenderInspector();
	void RenderStateInspector();
	// WU-7.3. Draws NOT ONE ITEM unless the selected state is a blend space, and
	// lives inside the INSPECTOR child — which is a FIXED height, so whatever it
	// emits costs the canvas nothing. That is the same placement argument the
	// layer strip makes for the side child, and the same rule the dope sheet
	// learned the expensive way (Editor/CLAUDE.md → "NOTHING SHOWN DRAWS
	// NOTHING").
	void RenderBlendStrip(const std::string& strStateName, Zenith_AnimCtrlStateTreeKind eKind);
	void RenderTransitionInspector();
	void RenderCanvas();
	void DrawCanvasBackground(ImDrawList* pxDraw, const CanvasLayout& xLayout);
	void DrawTransitions(ImDrawList* pxDraw, const CanvasLayout& xLayout);
	void DrawNodes(ImDrawList* pxDraw, const CanvasLayout& xLayout);
	// The ONLY functions that read ImGui state; each ends in an Action_*.
	void HandleCanvasInput(const CanvasLayout& xLayout, bool bCanvasHovered);
	bool FindStateAtScreenPos(float fX, float fY, std::string& strOut) const;
	bool FindTransitionAtScreenPos(float fX, float fY, std::string& strOutFrom, u_int& uOutIndex) const;

	Zenith_AnimControllerDocument m_xDocument;

	// ★ THE PREVIEW CONTROLLER AND ITS ZERO-BONE RIG. The rig is a member rather
	// than an asset: it exists only so Flux_AnimationStateMachine::Update has the
	// const Zenith_SkeletonAsset& its signature requires, it is never registered,
	// never loaded and never posed. See the highlighting block above.
	Flux_AnimationController m_xPreviewController;
	Zenith_SkeletonAsset m_xPreviewStubSkeleton;
	Flux_SkeletonPose m_xPreviewPose;
	bool m_bPreviewEnabled = false;
	bool m_bPreviewComplete = true;
	std::string m_strHighlightedState;

	// Panel-side layout for states the def places at the origin. NOT written to
	// the def — see GetNodePosition.
	Zenith_HashMap<std::string, Zenith_Maths::Vector2> m_xAutoLayout;
	u_int m_uAutoLayoutStateCount = 0;
	u_int m_uAutoLayoutMachineId = uANIMCTRL_TOP_LEVEL_MACHINE;

	// Recorded THIS frame, cleared at the top of every Render.
	Zenith_HashMap<std::string, Zenith_AnimCtrlPanelRect> m_xNodeRects;
	Zenith_HashMap<u_int64, Zenith_AnimCtrlPanelRect> m_xTransitionRects;
	// The owner index a transition rect key was built from, so a lookup can
	// rebuild the same key. Index 0 is the any-state list; a state's index is
	// its position in GetStateNamesSorted + 1.
	Zenith_Vector<std::string> m_axRectOwnerOrder;
	Zenith_AnimCtrlPanelRect m_xCanvasRect;
	bool m_bCanvasRectValid = false;
	// WU-7.2's strip diagnostics, recorded each Render and cleared with the rects.
	bool m_bLayerStripDrawn = false;
	u_int m_uDrawnLayerRows = 0;
	// WU-7.3's strip: its frame, its markers (index -> rect, recorded only for
	// what was painted INSIDE the strip) and the live dot, plus the axis RANGE
	// the frame was drawn with — which is half of the pixel<->position mapping
	// and therefore has to be recorded with the rects rather than re-derived at
	// query time. All cleared with them, for the reason ClearFrameRects gives.
	bool m_bBlendStripDrawn = false;
	Zenith_AnimCtrlPanelRect m_xBlendStripRect;
	bool m_bBlendStripRectValid = false;
	Zenith_HashMap<u_int, Zenith_AnimCtrlPanelRect> m_xBlendPointRects;
	Zenith_AnimCtrlPanelRect m_xLiveDotRect;
	bool m_bLiveDotRectValid = false;
	float m_fBlendRangeMinX = 0.0f;
	float m_fBlendRangeMaxX = 0.0f;
	float m_fBlendRangeMinY = 0.0f;
	float m_fBlendRangeMaxY = 0.0f;
	float m_fRecordedDisplayWidth = 0.0f;
	float m_fRecordedDisplayHeight = 0.0f;
	u_int m_uRenderedFrames = 0;

	std::string m_strSelectedState;
	std::string m_strSelectedTransitionFrom;
	u_int m_uSelectedTransition = uINVALID_ANIMSM_TRANSITION;
	bool m_bHasTransitionSelection = false;
	// WU-7.3. Scoped to the SELECTED STATE: selecting another state clears it,
	// because a blend-point index means nothing in a different space.
	u_int m_uSelectedBlendPoint = uINVALID_ANIMSM_BLEND_POINT;

	// Gestures.
	bool m_bDraggingNode = false;
	std::string m_strDraggingState;
	Zenith_Maths::Vector2 m_xDragStartPosition = Zenith_Maths::Vector2(0.0f);
	Zenith_Maths::Vector2 m_xDragGrabOffset = Zenith_Maths::Vector2(0.0f);
	bool m_bDraggingTransition = false;
	std::string m_strTransitionDragFrom;
	// WU-7.3's marker drag. ★ COMMITTED ON RELEASE, NOT PER FRAME — the same
	// "preview, then commit" shape the node drag, the layer weight slider and the
	// dope sheet's key drag all use, and for the same reason: one command per
	// frame of a drag makes Ctrl+Z crawl back through positions the author was
	// only passing through. The start pixel is what tells a click from a drag.
	bool m_bDraggingBlendPoint = false;
	Zenith_Maths::Vector2 m_xBlendDragStartPixel = Zenith_Maths::Vector2(0.0f);

	// View.
	float m_fScrollX = 0.0f;
	float m_fScrollY = 0.0f;
	bool m_bPlacementRequested = false;
	float m_fPlacementX = 0.0f;
	float m_fPlacementY = 0.0f;
	float m_fPlacementWidth = 0.0f;
	float m_fPlacementHeight = 0.0f;
	bool m_bPendingScrollToState = false;
	std::string m_strPendingScrollState;

	bool m_bShow = false;
	bool m_bWasFocused = false;
	bool m_bExternalConflict = false;
	bool m_bCloseRefusedDirty = false;
	Zenith_AnimCtrlDocOpenResult m_eLastOpenResult = ZENITH_ANIMCTRLDOC_OPEN_FAILED_NO_ASSET;
	Zenith_AnimCtrlDocSaveResult m_eLastSaveResult = ZENITH_ANIMCTRLDOC_SAVE_FAILED_NO_DOCUMENT;
	std::string m_strLastOpenAttemptPath;

	// Toolbar / inspector scratch. Fixed buffers because that is what
	// ImGui::InputText takes.
	char m_acPathBuffer[512] = {};
	char m_acStateNameBuffer[128] = {};
	char m_acParameterNameBuffer[128] = {};
	char m_acClipPathBuffer[512] = {};
	// WU-7.2's strip. ★ THE "NEW LAYER" NAME AND THE SELECTED LAYER'S NAME ARE
	// TWO BUFFERS, not one. Both InputTexts are submitted in the SAME frame, so a
	// shared buffer would echo every keystroke of one into the other and the
	// "+ Layer" button would come pre-loaded with the selected layer's name.
	// (m_acStateNameBuffer gets away with doubling up because the panel only ever
	// draws one state-name field.)
	char m_acLayerNameBuffer[128] = {};
	char m_acLayerRenameBuffer[128] = {};
	char m_acLayerMaskPathBuffer[512] = {};
	// WU-7.3's strip. The clip a new blend point would play is picked from the
	// def's own clip list, so this is an INDEX into that list rather than a
	// buffer — the same choice the state inspector's clip combo makes.
	int m_iNewBlendPointClip = 0;
	int m_iNewParameterType = 0;
	float m_fNewParameterDefault = 0.0f;
	int m_iConditionParameterIndex = 0;
	int m_iConditionCompareOp = 0;
	float m_fConditionThreshold = 0.0f;
};

#endif // ZENITH_TOOLS
