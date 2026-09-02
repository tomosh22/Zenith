// Unit tests for the editor's undo command layer and the preference / panel
// helpers introduced with it. Included into Zenith_EditorCommands.cpp under
// ZENITH_TESTING; runs on every backend (nothing here needs a GPU).

#include "UnitTests/Zenith_EditorTestFixture.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorActions.h"
#include "Editor/Zenith_EditorPrefs.h"
#include "Editor/Zenith_UndoSystem.h"
#include "Editor/Panels/Zenith_EditorPanel_Hierarchy.h"
#include "Editor/Panels/Zenith_EditorPanel_Properties.h"
#include "Editor/Panels/Zenith_EditorPanel_Viewport.h"
#include "EntityComponent/Components/Zenith_LightComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Flux/Gizmos/Flux_GizmosImpl.h"
#include "ZenithECS/Zenith_SceneSystem.h"

namespace
{
	Zenith_Entity ResolveTestEntity(Zenith_EntityID xID)
	{
		return g_xEngine.Scenes().ResolveEntity(xID);
	}

	Zenith_EntityID FindByName(const char* szName)
	{
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
		Zenith_Entity xEntity = pxSceneData->FindEntityByName(szName);
		return xEntity.IsValid() ? xEntity.GetEntityID() : INVALID_ENTITY_ID;
	}
}

//------------------------------------------------------------------------------
// Entity snapshot: capture + restore round-trips a subtree with its components.
//------------------------------------------------------------------------------
ZENITH_TEST(EditorCommands, SnapshotRestoresSubtreeWithComponents)
{
	EDITOR_TEST_BEGIN(SnapshotRestoresSubtree);

	Zenith_EntityID uParent = Zenith_EditorTestFixture::CreateTestEntityWithTransform("SnapParent", Zenith_Maths::Vector3(3, 4, 5), Zenith_Maths::Vector3(2, 2, 2));
	Zenith_EntityID uChild = Zenith_EditorTestFixture::CreateTestEntity("SnapChild");
	Zenith_EditorTestFixture::SetupHierarchy(uParent, uChild);
	ResolveTestEntity(uParent).SetTransient(false);
	ResolveTestEntity(uChild).AddComponent<Zenith_LightComponent>().SetIntensity(7.5f);
	ResolveTestEntity(uChild).SetEnabled(false);

	Zenith_EditorEntitySnapshot xSnapshot;
	ZENITH_ASSERT_TRUE(xSnapshot.Capture(ResolveTestEntity(uParent)), "Capture succeeds on a live entity");
	ZENITH_ASSERT_EQ(xSnapshot.GetEntityCount(), 2u, "Root + one child captured");
	ZENITH_ASSERT_EQ(xSnapshot.GetRootName(), std::string("SnapParent"), "Root name captured");

	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
	Zenith_EditorSceneAccess::RemoveEntity(pxSceneData, uParent);
	ZENITH_ASSERT_FALSE(ResolveTestEntity(uParent).IsValid(), "Parent deleted");
	ZENITH_ASSERT_FALSE(ResolveTestEntity(uChild).IsValid(), "Child deleted with its parent");

	const Zenith_EntityID uRestored = xSnapshot.Restore(INVALID_ENTITY_ID, true);
	ZENITH_ASSERT_TRUE(uRestored.IsValid(), "Restore returns a live root");
	ZENITH_ASSERT_NE(uRestored, uParent, "Restore allocates a fresh ID");
	Zenith_Entity xRestored = ResolveTestEntity(uRestored);
	ZENITH_ASSERT_EQ(xRestored.GetName(), std::string("SnapParent"), "Name restored");
	ZENITH_ASSERT_EQ(xRestored.GetChildCount(), 1u, "Child restored under the root");
	Zenith_Maths::Vector3 xPos;
	xRestored.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
	ZENITH_ASSERT_EQ_FLOAT(xPos.x, 3.0f, 0.001f, "Transform position restored");
	Zenith_Entity xChild = pxSceneData->GetEntity(xRestored.GetChildEntityIDs().Get(0));
	ZENITH_ASSERT_TRUE(xChild.HasComponent<Zenith_LightComponent>(), "Child component restored");
	ZENITH_ASSERT_EQ_FLOAT(xChild.GetComponent<Zenith_LightComponent>().GetIntensity(), 7.5f, 0.001f, "Component payload restored");
	ZENITH_ASSERT_FALSE(xChild.IsEnabled(), "Enabled flag restored");
	ZENITH_ASSERT_FALSE(xRestored.IsTransient(), "Persistent flag restored");

	Zenith_EditorSceneAccess::RemoveEntity(pxSceneData, uRestored);
	EDITOR_TEST_END(SnapshotRestoresSubtree);
}

//------------------------------------------------------------------------------
// Delete is undoable: undo brings the entity (and its children) back, redo
// removes them again.
//------------------------------------------------------------------------------
ZENITH_TEST(EditorCommands, DeleteSelectionUndoRedo)
{
	EDITOR_TEST_BEGIN(DeleteSelectionUndoRedo);
	Zenith_UndoSystem& xUndo = g_xEngine.UndoSystem();
	xUndo.Clear();

	Zenith_EntityID uParent = Zenith_EditorTestFixture::CreateTestEntity("DelParent");
	Zenith_EntityID uChild = Zenith_EditorTestFixture::CreateTestEntity("DelChild");
	Zenith_EditorTestFixture::SetupHierarchy(uParent, uChild);
	ResolveTestEntity(uParent).SetTransient(false);

	g_xEngine.Editor().SelectEntity(uParent, false);
	g_xEngine.Editor().SelectEntity(uChild, true);   // child of a selected parent: not a second root
	ZENITH_ASSERT_EQ(Zenith_EditorActions::DeleteSelection(), 1u, "One root deleted for a parent+child selection");
	ZENITH_ASSERT_FALSE(ResolveTestEntity(uParent).IsValid(), "Parent gone");
	ZENITH_ASSERT_FALSE(FindByName("DelChild").IsValid(), "Child gone");
	ZENITH_ASSERT_TRUE(xUndo.CanUndo(), "Delete recorded");
	ZENITH_ASSERT_FALSE(g_xEngine.Editor().HasSelection(), "Selection cleared by delete");

	xUndo.Undo();
	const Zenith_EntityID uBack = FindByName("DelParent");
	ZENITH_ASSERT_TRUE(uBack.IsValid(), "Undo restored the parent");
	ZENITH_ASSERT_TRUE(FindByName("DelChild").IsValid(), "Undo restored the child");
	ZENITH_ASSERT_EQ(ResolveTestEntity(uBack).GetChildCount(), 1u, "Hierarchy restored");
	ZENITH_ASSERT_TRUE(g_xEngine.Editor().IsSelected(uBack), "Restored entity is selected");

	xUndo.Redo();
	ZENITH_ASSERT_FALSE(FindByName("DelParent").IsValid(), "Redo deleted it again");
	xUndo.Clear();
	EDITOR_TEST_END(DeleteSelectionUndoRedo);
}

//------------------------------------------------------------------------------
// Duplicate copies the subtree beside the original, names it "(1)", and
// undoes as one step.
//------------------------------------------------------------------------------
ZENITH_TEST(EditorCommands, DuplicateSelectionMakesNumberedCopy)
{
	EDITOR_TEST_BEGIN(DuplicateSelection);
	Zenith_UndoSystem& xUndo = g_xEngine.UndoSystem();
	xUndo.Clear();

	Zenith_EntityID uSource = Zenith_EditorTestFixture::CreateTestEntityWithTransform("Lamp", Zenith_Maths::Vector3(1, 2, 3), Zenith_Maths::Vector3(1, 1, 1));
	Zenith_EntityID uChild = Zenith_EditorTestFixture::CreateTestEntity("Bulb");
	Zenith_EditorTestFixture::SetupHierarchy(uSource, uChild);
	ResolveTestEntity(uSource).SetTransient(false);

	g_xEngine.Editor().SelectEntity(uSource, false);
	ZENITH_ASSERT_EQ(Zenith_EditorActions::DuplicateSelection(), 1u, "One copy made");
	const Zenith_EntityID uCopy = FindByName("Lamp (1)");
	ZENITH_ASSERT_TRUE(uCopy.IsValid(), "Copy named 'Lamp (1)'");
	ZENITH_ASSERT_TRUE(ResolveTestEntity(uSource).IsValid(), "Original untouched");
	ZENITH_ASSERT_EQ(ResolveTestEntity(uCopy).GetChildCount(), 1u, "Children duplicated");
	ZENITH_ASSERT_TRUE(g_xEngine.Editor().IsSelected(uCopy), "Copy selected");
	ZENITH_ASSERT_FALSE(g_xEngine.Editor().IsSelected(uSource), "Original no longer selected");

	xUndo.Undo();
	ZENITH_ASSERT_FALSE(FindByName("Lamp (1)").IsValid(), "Undo removed the copy");
	ZENITH_ASSERT_TRUE(ResolveTestEntity(uSource).IsValid(), "Undo kept the original");

	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
	Zenith_EditorSceneAccess::RemoveEntity(pxSceneData, uSource);
	xUndo.Clear();
	EDITOR_TEST_END(DuplicateSelection);
}

//------------------------------------------------------------------------------
// Rename / enable / reparent through the state command.
//------------------------------------------------------------------------------
ZENITH_TEST(EditorCommands, EntityStateCommandsUndo)
{
	EDITOR_TEST_BEGIN(EntityStateCommands);
	Zenith_UndoSystem& xUndo = g_xEngine.UndoSystem();
	xUndo.Clear();

	Zenith_EntityID uA = Zenith_EditorTestFixture::CreateTestEntity("StateA");
	Zenith_EntityID uB = Zenith_EditorTestFixture::CreateTestEntity("StateB");

	Zenith_EditorActions::RenameEntity(uA, "StateA_Renamed");
	ZENITH_ASSERT_EQ(ResolveTestEntity(uA).GetName(), std::string("StateA_Renamed"), "Rename applied");
	xUndo.Undo();
	ZENITH_ASSERT_EQ(ResolveTestEntity(uA).GetName(), std::string("StateA"), "Rename undone");
	xUndo.Redo();
	ZENITH_ASSERT_EQ(ResolveTestEntity(uA).GetName(), std::string("StateA_Renamed"), "Rename redone");

	Zenith_EditorActions::SetEntityEnabled(uA, false);
	ZENITH_ASSERT_FALSE(ResolveTestEntity(uA).IsEnabled(), "Disable applied");
	xUndo.Undo();
	ZENITH_ASSERT_TRUE(ResolveTestEntity(uA).IsEnabled(), "Disable undone");

	ZENITH_ASSERT_TRUE(Zenith_EditorActions::ReparentEntity(uB, uA), "Reparent accepted");
	ZENITH_ASSERT_EQ(ResolveTestEntity(uB).GetParentEntityID(), uA, "B parented to A");
	ZENITH_ASSERT_FALSE(Zenith_EditorActions::ReparentEntity(uA, uB), "Cycle refused");
	ZENITH_ASSERT_FALSE(Zenith_EditorActions::ReparentEntity(uA, uA), "Self-parent refused");
	xUndo.Undo();
	ZENITH_ASSERT_FALSE(ResolveTestEntity(uB).HasParent(), "Reparent undone");

	xUndo.Clear();
	EDITOR_TEST_END(EntityStateCommands);
}

//------------------------------------------------------------------------------
// Add / remove a component through the payload command, with undo.
//------------------------------------------------------------------------------
ZENITH_TEST(EditorCommands, ComponentAddRemoveUndo)
{
	EDITOR_TEST_BEGIN(ComponentAddRemove);
	Zenith_UndoSystem& xUndo = g_xEngine.UndoSystem();
	xUndo.Clear();

	Zenith_EntityID uEntity = Zenith_EditorTestFixture::CreateTestEntity("CompHost");
	ZENITH_ASSERT_TRUE(Zenith_EditorActions::AddComponent(uEntity, "Light"), "Light added");
	ZENITH_ASSERT_TRUE(ResolveTestEntity(uEntity).HasComponent<Zenith_LightComponent>(), "Light present");
	ZENITH_ASSERT_FALSE(Zenith_EditorActions::AddComponent(uEntity, "Light"), "Duplicate add refused");
	ResolveTestEntity(uEntity).GetComponent<Zenith_LightComponent>().SetIntensity(42.0f);

	ZENITH_ASSERT_TRUE(Zenith_EditorActions::RemoveComponent(uEntity, "Light"), "Light removed");
	ZENITH_ASSERT_FALSE(ResolveTestEntity(uEntity).HasComponent<Zenith_LightComponent>(), "Light absent");

	xUndo.Undo();
	ZENITH_ASSERT_TRUE(ResolveTestEntity(uEntity).HasComponent<Zenith_LightComponent>(), "Undo restored the light");
	ZENITH_ASSERT_EQ_FLOAT(ResolveTestEntity(uEntity).GetComponent<Zenith_LightComponent>().GetIntensity(), 42.0f, 0.001f, "Undo restored the light's values");

	xUndo.Undo();
	ZENITH_ASSERT_FALSE(ResolveTestEntity(uEntity).HasComponent<Zenith_LightComponent>(), "Undoing the add removes it");
	ZENITH_ASSERT_FALSE(Zenith_EditorActions::RemoveComponent(uEntity, "Light"), "Removing an absent component is refused");

	xUndo.Clear();
	EDITOR_TEST_END(ComponentAddRemove);
}

//------------------------------------------------------------------------------
// The inspector tracker turns an in-place edit into one undo step.
//------------------------------------------------------------------------------
ZENITH_TEST(EditorCommands, InspectorTrackerRecordsComponentEdit)
{
	EDITOR_TEST_BEGIN(InspectorTracker);
	Zenith_UndoSystem& xUndo = g_xEngine.UndoSystem();
	xUndo.Clear();

	Zenith_EntityID uEntity = Zenith_EditorTestFixture::CreateTestEntityWithTransform("Tracked", Zenith_Maths::Vector3(0, 0, 0), Zenith_Maths::Vector3(1, 1, 1));
	Zenith_Entity xEntity = ResolveTestEntity(uEntity);
	xEntity.AddComponent<Zenith_LightComponent>().SetIntensity(1.0f);

	Zenith_EditorInspectorUndoTracker xTracker;
	xTracker.BeginFrame(xEntity, true);
	ZENITH_ASSERT_TRUE(xTracker.HasOpenSession(), "Trigger opens a session");
	xTracker.EndFrame(xEntity, false);
	ZENITH_ASSERT_TRUE(xTracker.HasOpenSession(), "Session stays open while the UI is busy");

	// The "edit": a light value and a transform move, as the inspector would do.
	xEntity.GetComponent<Zenith_LightComponent>().SetIntensity(9.0f);
	xEntity.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(5, 0, 0));
	xTracker.EndFrame(xEntity, true);
	ZENITH_ASSERT_FALSE(xTracker.HasOpenSession(), "Idle UI closes the session");
	ZENITH_ASSERT_EQ(xUndo.GetUndoStackSize(), 2u, "One command per changed component");

	xUndo.Undo();   // light edit (recorded second)
	ZENITH_ASSERT_EQ_FLOAT(xEntity.GetComponent<Zenith_LightComponent>().GetIntensity(), 1.0f, 0.001f, "Light edit undone");
	xUndo.Undo();   // transform edit
	Zenith_Maths::Vector3 xPos;
	ResolveTestEntity(uEntity).GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
	ZENITH_ASSERT_EQ_FLOAT(xPos.x, 0.0f, 0.001f, "Transform edit undone");

	// An untouched session records nothing.
	xTracker.BeginFrame(ResolveTestEntity(uEntity), true);
	xTracker.EndFrame(ResolveTestEntity(uEntity), true);
	ZENITH_ASSERT_EQ(xUndo.GetUndoStackSize(), 0u, "No-op session records nothing");

	xUndo.Clear();
	EDITOR_TEST_END(InspectorTracker);
}

//------------------------------------------------------------------------------
// Record() pushes without executing and clears redo; Composite undoes in reverse.
//------------------------------------------------------------------------------
namespace
{
	struct CountingCommand : public Zenith_UndoCommand
	{
		int* m_piLog;
		int m_iTag;
		CountingCommand(int* piLog, int iTag) : m_piLog(piLog), m_iTag(iTag) {}
		void Execute() override { *m_piLog = *m_piLog * 10 + m_iTag; }
		void Undo() override { *m_piLog = *m_piLog * 10 - m_iTag; }
		const char* GetDescription() const override { return "Counting"; }
	};
}

ZENITH_TEST(EditorCommands, RecordAndCompositeSemantics)
{
	Zenith_UndoSystem& xUndo = g_xEngine.UndoSystem();
	xUndo.Clear();
	int iLog = 0;

	xUndo.Record(new CountingCommand(&iLog, 1));
	ZENITH_ASSERT_EQ(iLog, 0, "Record does not execute");
	ZENITH_ASSERT_TRUE(xUndo.CanUndo(), "Record pushes onto the undo stack");
	xUndo.Undo();
	ZENITH_ASSERT_EQ(iLog, -1, "Undo of a recorded command runs its Undo");
	ZENITH_ASSERT_TRUE(xUndo.CanRedo(), "Undone command is redoable");
	xUndo.Record(new CountingCommand(&iLog, 2));
	ZENITH_ASSERT_FALSE(xUndo.CanRedo(), "Record clears the redo stack");

	xUndo.Clear();
	iLog = 0;
	Zenith_UndoCommand_Composite* pxComposite = new Zenith_UndoCommand_Composite("Two");
	pxComposite->Add(new CountingCommand(&iLog, 1));
	pxComposite->Add(new CountingCommand(&iLog, 2));
	pxComposite->Add(nullptr);
	ZENITH_ASSERT_EQ(pxComposite->GetCount(), 2u, "Null children are ignored");
	xUndo.Execute(pxComposite);
	ZENITH_ASSERT_EQ(iLog, 12, "Composite executes in order");
	xUndo.Undo();
	ZENITH_ASSERT_EQ(iLog, 12 * 100 - 21, "Composite undoes in reverse order");
	xUndo.Clear();
}

//------------------------------------------------------------------------------
// Preferences: text round-trip, recent-scene ordering and clamps.
//------------------------------------------------------------------------------
ZENITH_TEST(EditorPrefs, SerializeParseRoundTrip)
{
	Zenith_EditorPrefs xPrefs;
	xPrefs.AddRecentScene("C:/a/one.zscen");
	xPrefs.AddRecentScene("C:/a/two.zscen");
	xPrefs.AddRecentScene("C:/a/one.zscen");   // re-adding moves it to the front
	xPrefs.m_fCameraMoveSpeed = 123.5f;
	xPrefs.m_fLookSensitivity = 0.035f;
	xPrefs.m_bSnapEnabled = true;
	xPrefs.m_fSnapMove = 0.25f;
	xPrefs.m_fSnapRotateDegrees = 45.0f;
	xPrefs.m_fSnapScale = 0.5f;
	xPrefs.m_bGizmoLocalSpace = true;
	xPrefs.m_bShowViewportStats = false;
	xPrefs.m_bShowViewportAxes = false;
	xPrefs.m_bShowSelectionBounds = false;
	xPrefs.m_bClearConsoleOnPlay = true;

	ZENITH_ASSERT_EQ(xPrefs.m_axRecentScenes.GetSize(), 2u, "Recent list deduplicated");
	ZENITH_ASSERT_EQ(xPrefs.m_axRecentScenes.Get(0), std::string("C:/a/one.zscen"), "Most recent first");

	const std::string strText = xPrefs.Serialize();
	Zenith_EditorPrefs xParsed;
	xParsed.Parse(strText);
	ZENITH_ASSERT_EQ(xParsed.m_axRecentScenes.GetSize(), 2u, "Recent count round-trips");
	ZENITH_ASSERT_EQ(xParsed.m_axRecentScenes.Get(0), std::string("C:/a/one.zscen"), "Recent order round-trips");
	ZENITH_ASSERT_EQ(xParsed.m_axRecentScenes.Get(1), std::string("C:/a/two.zscen"), "Second recent round-trips");
	ZENITH_ASSERT_EQ_FLOAT(xParsed.m_fCameraMoveSpeed, 123.5f, 0.001f, "Camera speed round-trips");
	ZENITH_ASSERT_EQ_FLOAT(xParsed.m_fLookSensitivity, 0.035f, 0.0001f, "Look sensitivity round-trips");
	ZENITH_ASSERT_TRUE(xParsed.m_bSnapEnabled, "Snap flag round-trips");
	ZENITH_ASSERT_EQ_FLOAT(xParsed.m_fSnapMove, 0.25f, 0.0001f, "Snap move round-trips");
	ZENITH_ASSERT_EQ_FLOAT(xParsed.m_fSnapRotateDegrees, 45.0f, 0.001f, "Snap rotate round-trips");
	ZENITH_ASSERT_EQ_FLOAT(xParsed.m_fSnapScale, 0.5f, 0.0001f, "Snap scale round-trips");
	ZENITH_ASSERT_TRUE(xParsed.m_bGizmoLocalSpace, "Local space round-trips");
	ZENITH_ASSERT_FALSE(xParsed.m_bShowViewportStats, "Stats toggle round-trips");
	ZENITH_ASSERT_FALSE(xParsed.m_bShowViewportAxes, "Axes toggle round-trips");
	ZENITH_ASSERT_FALSE(xParsed.m_bShowSelectionBounds, "Bounds toggle round-trips");
	ZENITH_ASSERT_TRUE(xParsed.m_bClearConsoleOnPlay, "Console toggle round-trips");
}

ZENITH_TEST(EditorPrefs, ParseIgnoresGarbageAndClamps)
{
	Zenith_EditorPrefs xPrefs;
	xPrefs.Parse("# comment\nnot a pair\ncamera_speed=-5\nsnap_move=0\nunknown_key=7\nsnap_enabled=true\nlook_sensitivity_px=99\n\n");
	ZENITH_ASSERT_EQ_FLOAT(xPrefs.m_fCameraMoveSpeed, 50.0f, 0.001f, "Non-positive camera speed falls back to the default");
	ZENITH_ASSERT_EQ_FLOAT(xPrefs.m_fLookSensitivity, 0.1f, 0.0001f, "Out-of-range look sensitivity falls back to the default");

	// A file written while the sensitivity was in raw device counts must NOT be
	// honoured: the key carries its unit, so the retired one reads as unknown.
	Zenith_EditorPrefs xStale;
	xStale.Parse("look_sensitivity=0.022\n");
	ZENITH_ASSERT_EQ_FLOAT(xStale.m_fLookSensitivity, 0.1f, 0.0001f, "A retired-unit sensitivity key is ignored");

	Zenith_EditorPrefs xInRange;
	xInRange.Parse("look_sensitivity_px=0.08\n");
	ZENITH_ASSERT_EQ_FLOAT(xInRange.m_fLookSensitivity, 0.08f, 0.0001f, "An in-range look sensitivity is kept");
	ZENITH_ASSERT_EQ_FLOAT(xPrefs.m_fSnapMove, 0.5f, 0.001f, "Zero snap step falls back to the default");
	ZENITH_ASSERT_TRUE(xPrefs.m_bSnapEnabled, "'true' parses as a boolean");
	ZENITH_ASSERT_EQ(xPrefs.m_axRecentScenes.GetSize(), 0u, "No recent scenes from garbage");

	for (u_int u = 0; u < Zenith_EditorPrefs::uMAX_RECENT_SCENES + 3; ++u)
	{
		xPrefs.AddRecentScene("scene" + std::to_string(u));
	}
	ZENITH_ASSERT_EQ(xPrefs.m_axRecentScenes.GetSize(), Zenith_EditorPrefs::uMAX_RECENT_SCENES, "Recent list is capped");
	ZENITH_ASSERT_EQ(xPrefs.m_axRecentScenes.Get(0), std::string("scene") + std::to_string(Zenith_EditorPrefs::uMAX_RECENT_SCENES + 2), "Newest survives the cap");
	xPrefs.RemoveRecentScene(xPrefs.m_axRecentScenes.Get(0));
	ZENITH_ASSERT_EQ(xPrefs.m_axRecentScenes.GetSize(), Zenith_EditorPrefs::uMAX_RECENT_SCENES - 1, "Remove drops one entry");
}

//------------------------------------------------------------------------------
// Panel helpers: search matching and the viewport axis projection.
//------------------------------------------------------------------------------
ZENITH_TEST(EditorPanels, HierarchySearchMatchesNameOrComponent)
{
	ZENITH_ASSERT_TRUE(Zenith_EditorPanelHierarchy::MatchesSearch("Lamp_Red", "Transform, Light", ""), "Empty query matches everything");
	ZENITH_ASSERT_TRUE(Zenith_EditorPanelHierarchy::MatchesSearch("Lamp_Red", "Transform, Light", "lamp"), "Case-insensitive name match");
	ZENITH_ASSERT_TRUE(Zenith_EditorPanelHierarchy::MatchesSearch("Lamp_Red", "Transform, Light", "LIGHT"), "Component name match");
	ZENITH_ASSERT_FALSE(Zenith_EditorPanelHierarchy::MatchesSearch("Lamp_Red", "Transform, Light", "camera"), "No match");
	ZENITH_ASSERT_FALSE(Zenith_EditorPanelHierarchy::MatchesSearch(nullptr, nullptr, "x"), "Null haystacks never match a query");
	ZENITH_ASSERT_TRUE(Zenith_EditorPanelProperties::MatchesComponentSearch("ParticleEmitter", "emit"), "Component search is a substring match");
	ZENITH_ASSERT_FALSE(Zenith_EditorPanelProperties::MatchesComponentSearch("Camera", "light"), "Component search rejects non-matches");
}

ZENITH_TEST(EditorPanels, ViewportAxisWidgetProjection)
{
	// Identity view: world X is screen right, world Y is screen UP (negative in
	// y-down pixel space), world Z points into the screen (positive depth).
	const Zenith_Maths::Matrix4 xIdentity(1.0f);
	Zenith_Maths::Vector2 xDir;
	float fDepth = Zenith_EditorPanelViewport::ProjectAxisForWidget(xIdentity, Zenith_Maths::Vector3(1, 0, 0), xDir);
	ZENITH_ASSERT_EQ_FLOAT(xDir.x, 1.0f, 0.001f, "X axis points right");
	ZENITH_ASSERT_EQ_FLOAT(fDepth, 0.0f, 0.001f, "X axis has no depth");
	fDepth = Zenith_EditorPanelViewport::ProjectAxisForWidget(xIdentity, Zenith_Maths::Vector3(0, 1, 0), xDir);
	ZENITH_ASSERT_EQ_FLOAT(xDir.y, -1.0f, 0.001f, "Y axis points up on screen");
	fDepth = Zenith_EditorPanelViewport::ProjectAxisForWidget(xIdentity, Zenith_Maths::Vector3(0, 0, 1), xDir);
	ZENITH_ASSERT_EQ_FLOAT(fDepth, 1.0f, 0.001f, "Z axis points into the screen");
	ZENITH_ASSERT_EQ_FLOAT(xDir.x, 0.0f, 0.001f, "Z axis has no screen extent");
}

//------------------------------------------------------------------------------
// Gizmo snapping arithmetic and the local-space axis.
//------------------------------------------------------------------------------
ZENITH_TEST(EditorGizmo, SnapValueRoundsToStep)
{
	ZENITH_ASSERT_EQ_FLOAT(Flux_GizmosImpl::SnapValue(0.74f, 0.5f), 0.5f, 0.0001f, "Rounds down below the midpoint");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GizmosImpl::SnapValue(0.76f, 0.5f), 1.0f, 0.0001f, "Rounds up above the midpoint");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GizmosImpl::SnapValue(-0.76f, 0.5f), -1.0f, 0.0001f, "Negative values snap symmetrically");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GizmosImpl::SnapValue(3.3f, 0.0f), 3.3f, 0.0001f, "A zero step disables snapping");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GizmosImpl::SnapValue(20.0f, 15.0f), 15.0f, 0.0001f, "Rotation-style steps");
}

ZENITH_TEST(EditorGizmo, LocalSpaceAxisFollowsInitialRotation)
{
	Flux_GizmosImpl& xGizmos = g_xEngine.Gizmos();
	const bool bWasLocal = xGizmos.IsLocalSpace();
	const Zenith_Maths::Quaternion xSaved = xGizmos.m_xInitialEntityRotation;

	xGizmos.SetLocalSpace(false);
	Zenith_Maths::Vector3 xAxis = xGizmos.GetComponentAxis(GizmoComponent::TranslateX);
	ZENITH_ASSERT_EQ_FLOAT(xAxis.x, 1.0f, 0.001f, "World space: X handle drags along world X");

	// 90 degrees about Y turns local X into world -Z.
	xGizmos.m_xInitialEntityRotation = glm::angleAxis(glm::radians(90.0f), Zenith_Maths::Vector3(0, 1, 0));
	xGizmos.SetLocalSpace(true);
	xAxis = xGizmos.GetComponentAxis(GizmoComponent::TranslateX);
	ZENITH_ASSERT_EQ_FLOAT(xAxis.z, -1.0f, 0.001f, "Local space: X handle follows the rotated entity");
	ZENITH_ASSERT_EQ_FLOAT(glm::length(xGizmos.GetComponentAxis(GizmoComponent::ScaleXYZ)), 0.0f, 0.001f, "Uniform scale has no axis");

	xGizmos.m_xInitialEntityRotation = xSaved;
	xGizmos.SetLocalSpace(bWasLocal);
}
