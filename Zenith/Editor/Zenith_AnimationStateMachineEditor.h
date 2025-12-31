#pragma once

#ifdef ZENITH_TOOLS

// TEMPORARILY DISABLED: This editor was generated with API assumptions that don't match
// the actual animation system implementation. It needs extensive updates to work:
// - GetType() → GetNodeTypeName() (returns string, not enum)
// - Flux_AnimationTransition → Flux_StateTransition
// - GetEntryState() → GetDefaultStateName()
// - SetEntryState() → SetDefaultState()
// - GetAllParameters() → GetParameters()
// - GetAllClips() → GetClips()
// - GetInputA/B() → GetChildA/B()
// - Plus 90+ other API mismatches
// TODO: Update this editor to match the actual Flux animation system API
#if 0

#include "Maths/Zenith_Maths.h"
#include <string>
#include <vector>
#include <unordered_map>

// Forward declarations
class Flux_AnimationController;
class Flux_AnimationStateMachine;
class Flux_AnimationState;
class Flux_AnimationTransition;
class Flux_AnimationParameters;
class Flux_BlendTreeNode;
class Zenith_ModelComponent;

//=============================================================================
// Node position/size data for visual editor
//=============================================================================
struct AnimEditorNodeData
{
	Zenith_Maths::Vector2 m_xPosition = Zenith_Maths::Vector2(0, 0);
	Zenith_Maths::Vector2 m_xSize = Zenith_Maths::Vector2(150, 60);
	bool m_bSelected = false;
};

//=============================================================================
// Zenith_AnimationStateMachineEditor
// Visual node-based editor for animation state machines
//=============================================================================
class Zenith_AnimationStateMachineEditor
{
public:
	static void Initialize();
	static void Shutdown();

	// Main render function - call from Zenith_Editor
	static void Render();

	// Set the controller to edit
	static void SetController(Flux_AnimationController* pxController);
	static Flux_AnimationController* GetController() { return s_pxController; }

	// Set the model component (for clip access)
	static void SetModelComponent(Zenith_ModelComponent* pxModelComponent);

	// Show/hide the editor window
	static void Show() { s_bVisible = true; }
	static void Hide() { s_bVisible = false; }
	static void Toggle() { s_bVisible = !s_bVisible; }
	static bool IsVisible() { return s_bVisible; }

private:
	// Main panels
	static void RenderMainPanel();
	static void RenderNodeGraph();
	static void RenderToolbar();
	static void RenderParametersPanel();
	static void RenderPropertiesPanel();
	static void RenderClipListPanel();

	// Node graph rendering
	static void RenderNodes();
	static void RenderNode(Flux_AnimationState* pxState, AnimEditorNodeData& xNodeData);
	static void RenderTransitions();
	static void RenderTransition(Flux_AnimationTransition* pxTransition,
		const Zenith_Maths::Vector2& xStartPos,
		const Zenith_Maths::Vector2& xEndPos);
	static void RenderGrid();

	// Input handling
	static void HandleNodeDragging();
	static void HandleNodeSelection();
	static void HandleTransitionCreation();
	static void HandleContextMenu();
	static void HandleZoomAndPan();

	// Node position management
	static AnimEditorNodeData& GetNodeData(const std::string& strStateName);
	static Zenith_Maths::Vector2 GetNodeCenter(const std::string& strStateName);
	static void AutoLayoutNodes();

	// Property editing
	static void RenderStateProperties(Flux_AnimationState* pxState);
	static void RenderTransitionProperties(Flux_AnimationTransition* pxTransition);
	static void RenderBlendTreeEditor(Flux_BlendTreeNode* pxNode, uint32_t uDepth = 0);
	static void RenderConditionEditor(Flux_AnimationTransition* pxTransition);
	static void RenderParameterValue(const std::string& strParamName, Flux_AnimationParameters& xParams);

	// Helper functions
	static Zenith_Maths::Vector2 ScreenToCanvas(const Zenith_Maths::Vector2& xScreenPos);
	static Zenith_Maths::Vector2 CanvasToScreen(const Zenith_Maths::Vector2& xCanvasPos);
	static bool IsNodeHovered(const AnimEditorNodeData& xNodeData, const Zenith_Maths::Vector2& xMousePos);
	static bool IsPointOnLine(const Zenith_Maths::Vector2& xPoint,
		const Zenith_Maths::Vector2& xStart,
		const Zenith_Maths::Vector2& xEnd,
		float fThreshold = 8.0f);
	static void DrawArrow(const Zenith_Maths::Vector2& xFrom,
		const Zenith_Maths::Vector2& xTo,
		uint32_t uColor,
		float fThickness = 2.0f);
	static void DrawBezierArrow(const Zenith_Maths::Vector2& xFrom,
		const Zenith_Maths::Vector2& xTo,
		uint32_t uColor,
		float fThickness = 2.0f);

	// Colors
	static uint32_t GetStateNodeColor(Flux_AnimationState* pxState);
	static uint32_t GetTransitionColor(Flux_AnimationTransition* pxTransition);

	// Editor state
	static bool s_bVisible;
	static Flux_AnimationController* s_pxController;
	static Zenith_ModelComponent* s_pxModelComponent;

	// Canvas state
	static Zenith_Maths::Vector2 s_xCanvasOffset;
	static float s_fZoom;
	static Zenith_Maths::Vector2 s_xCanvasSize;
	static Zenith_Maths::Vector2 s_xCanvasOrigin; // Screen position of canvas origin

	// Node data storage
	static std::unordered_map<std::string, AnimEditorNodeData> s_xNodeDataMap;

	// Selection state
	static std::string s_strSelectedStateName;
	static Flux_AnimationTransition* s_pxSelectedTransition;

	// Interaction state
	static bool s_bIsDraggingNode;
	static std::string s_strDraggedNodeName;
	static Zenith_Maths::Vector2 s_xDragOffset;
	static bool s_bIsCreatingTransition;
	static std::string s_strTransitionSourceState;
	static Zenith_Maths::Vector2 s_xTransitionDragPos;
	static bool s_bIsPanningCanvas;
	static Zenith_Maths::Vector2 s_xPanStartPos;

	// Context menu state
	static bool s_bShowContextMenu;
	static Zenith_Maths::Vector2 s_xContextMenuPos;
	static std::string s_strContextMenuTarget; // State name if right-clicked on state
};

#endif // #if 0 - TEMPORARILY DISABLED

#endif // ZENITH_TOOLS
