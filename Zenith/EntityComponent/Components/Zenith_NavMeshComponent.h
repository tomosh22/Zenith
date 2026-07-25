#pragma once

#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Navigation/Zenith_NavMeshStats.h"
#include "ZenithECS/Zenith_Entity.h"

#include <string>

class Zenith_DataStream;

// ============================================================================
// Zenith_NavMeshComponent -- the runtime half of baked-navmesh persistence
// (ZM-D-147): the engine analog of Unity's NavMeshSurface and UE's
// ARecastNavMesh. It carries an asset REF to a `.znavmesh` baked by
// Zenith_NavMeshBaker, loads it in OnStart, and OWNS the resulting mesh for as
// long as the component lives.
//
// OWNERSHIP, deliberately: the component owns its mesh. There is NO path-keyed
// cache and NO static mutable state anywhere in this feature -- same as the two
// engines above, and for the same reason: a navmesh's lifetime is its level's.
// Scene switching therefore frees and re-loads automatically, and the unit
// suite's scene teardown needs no special hook.
//
// TWO ADOPTION RECIPES:
//   * AUTHORED scenes -- add the component in the editor (or via
//     Zenith_EditorAutomation::AddStep_AddComponent("NavMesh")) and set its ref;
//     the ref serializes with the scene and OnStart loads it on every future
//     load, GPU or not.
//   * RUNTIME scenes -- xEntity.AddComponent<Zenith_NavMeshComponent>() then
//     SetAssetRef(...), which loads immediately.
// Discovery is an ordinary ECS query: QueryActiveScene<Zenith_NavMeshComponent>().
//
// TIMING: OnStart is deferred to the entity's first Update, so a test that adds
// the component must tick at least one frame before GetNavMesh() is non-null.
//
// FAILURE: a ref that will not load is a DEFECT, not an expected state -- the
// asset is committed to the repo, so absence means a broken build, and
// Zenith_NavMesh::LoadFromFile asserts. The component then holds
// NAVMESH_LOAD_STATE_FAILED plus a human-readable reason (which the editor panel
// displays), rather than silently pretending to be empty.
// ============================================================================

enum Zenith_NavMeshLoadState : u_int
{
	NAVMESH_LOAD_STATE_UNLOADED = 0,  // no ref, or explicitly unloaded
	NAVMESH_LOAD_STATE_LOADED,        // m_pxNavMesh is live
	NAVMESH_LOAD_STATE_FAILED,        // a ref was set but would not load
};

const char* Zenith_NavMeshLoadStateToString(Zenith_NavMeshLoadState eState);

class Zenith_NavMeshComponent
{
public:
	Zenith_NavMeshComponent(Zenith_Entity& xEntity) : m_xParentEntity(xEntity) {}
	~Zenith_NavMeshComponent();

	// Move semantics (required for component pools). The mesh pointer moves with
	// the component and the SOURCE is neutralised, so exactly one instance ever
	// frees it -- a pool relocation cannot double-free or leak.
	Zenith_NavMeshComponent(Zenith_NavMeshComponent&& xOther) noexcept;
	Zenith_NavMeshComponent& operator=(Zenith_NavMeshComponent&& xOther) noexcept;

	Zenith_NavMeshComponent(const Zenith_NavMeshComponent&) = delete;
	Zenith_NavMeshComponent& operator=(const Zenith_NavMeshComponent&) = delete;

	// ========== ECS lifecycle (concept-detected by the component-meta registry) ==========

	// Loads the referenced navmesh. Fires for BOTH live-authored and
	// disk-loaded scenes (OnAwake is skipped on scene deserialization).
	void OnStart();
	void OnDestroy();

	// ========== Asset ref ==========

	// Set (or clear) the asset ref and load immediately. A `game:`/`engine:`
	// prefix is resolved through Zenith_AssetRegistry, so a packaged build with
	// --assets-root works with no extra wiring.
	// @return true if a mesh is loaded afterwards.
	bool SetAssetRef(const std::string& strAssetRef);
	const std::string& GetAssetRef() const { return m_strAssetRef; }

	// Re-run the load path against the current ref -- the manual hot-reload story
	// after a re-bake.
	bool Reload();

	// Drop the loaded mesh, keeping the ref. State becomes UNLOADED.
	void Unload();

	// ========== Queries ==========

	// The owned mesh, or nullptr when not loaded. Non-owning: never delete it,
	// and never hold it across a scene change.
	const Zenith_NavMesh* GetNavMesh() const { return m_pxNavMesh; }
	Zenith_NavMesh* GetNavMesh() { return m_pxNavMesh; }

	bool IsLoaded() const { return m_pxNavMesh != nullptr; }
	Zenith_NavMeshLoadState GetLoadState() const { return m_eLoadState; }

	// Empty unless GetLoadState() == FAILED.
	const std::string& GetFailureReason() const { return m_strFailureReason; }

	// The ref resolved to an absolute path (empty when there is no ref).
	std::string GetResolvedPath() const;

	// ========== Serialization ==========
	// ONLY the asset ref persists. Load state, the mesh itself and every editor
	// visualisation toggle are transient.
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

#ifdef ZENITH_TOOLS
	// Drives the debug visualisation while it is enabled in the panel.
	void OnUpdate(float fDt);
	void RenderPropertiesPanel();
#endif

private:
	// The single load path behind OnStart / SetAssetRef / Reload. Sets
	// m_eLoadState and m_strFailureReason.
	bool LoadFromAssetRef();

	Zenith_Entity m_xParentEntity;

	// The serialized ref, e.g. "game:Navmesh/Dawnmere.znavmesh".
	std::string m_strAssetRef;

	// OWNED. Freed by OnDestroy / the destructor; nulled on a moved-from component.
	Zenith_NavMesh* m_pxNavMesh = nullptr;

	Zenith_NavMeshLoadState m_eLoadState = NAVMESH_LOAD_STATE_UNLOADED;
	std::string m_strFailureReason;

	// Set true on the SOURCE of a move. A moved-from component must not free the
	// mesh the moved-to component now owns. Belt-and-braces with the nulled
	// pointer (the Zenith_AnimatorComponent idiom), and the invariant the
	// relocation unit pins.
	bool m_bMovedOut = false;

#ifdef ZENITH_TOOLS
	// ---- transient editor state (never serialized) ----
	bool m_bDebugDrawEnabled = false;
	Zenith_NavMeshDebugDrawFlags m_xDebugDrawFlags;

	// Editable ref buffer for the panel's text field (applied on button press,
	// so a half-typed path never triggers a load).
	char m_szAssetRefEdit[512] = {};
	bool m_bAssetRefEditPrimed = false;

	// Point probe.
	Zenith_Maths::Vector3 m_xProbePoint = Zenith_Maths::Vector3(0.0f);
	bool m_bProbePointValid = false;
	bool m_bProbeOnNavMesh = false;
	bool m_bProbeProjected = false;
	Zenith_Maths::Vector3 m_xProbeProjected = Zenith_Maths::Vector3(0.0f);
	bool m_bProbeFoundPolygon = false;
	uint32_t m_uProbePolygon = 0;
	Zenith_Maths::Vector3 m_xProbeNearest = Zenith_Maths::Vector3(0.0f);

	// Path probe.
	Zenith_Maths::Vector3 m_xPathStart = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 m_xPathEnd = Zenith_Maths::Vector3(0.0f);
	bool m_bPathProbeRun = false;
	u_int m_uPathStatus = 0;          // Zenith_PathResult::Status, widened to avoid the include
	float m_fPathDistance = 0.0f;
	Zenith_Vector<Zenith_Maths::Vector3> m_axPathWaypoints;

	// ---- panel sections (one per collapsing header) ----
	void RenderAssetSection();
	void RenderStatsSection();
	void RenderVisualizationSection();
	void RenderPointProbeSection();
	void RenderPathProbeSection();

	// Fill xOut from the live main camera / this component's own entity.
	// (Deliberately NOT the editor's selection: EntityComponent sits below the
	// Editor layer, and the layering ratchet enforces that.)
	// @return false when there is no such source this frame.
	static bool TryGetMainCameraPosition(Zenith_Maths::Vector3& xOut);
	bool TryGetOwnEntityPosition(Zenith_Maths::Vector3& xOut);

	void RunPointProbe();
	void RunPathProbe();
	void DebugDrawProbes() const;
#endif

	// Lets the unit suite pin the ownership invariants (exactly one owner across
	// a pool relocation, a moved-from component frees nothing).
	friend class Zenith_UnitTests;
};
