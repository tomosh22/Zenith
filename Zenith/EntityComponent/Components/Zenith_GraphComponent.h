#pragma once

#include "ZenithECS/Zenith_Entity.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "DataStream/Zenith_DataStream.h"
#include <string>

//------------------------------------------------------------------------------
// Zenith_GraphComponent - hosts Behaviour Graph instances on an entity.
//
// The designer-facing scripting component of the Behaviour Graphs program (the
// replacement for the retired C++-behaviour script component, which it
// fully retire in the teardown phase). An entity has at most one
// Zenith_GraphComponent holding N graph slots; each slot = a .bgraph asset
// path + a live Zenith_BehaviourGraph instance + per-entity blackboard
// overrides.
//
// Lifecycle hooks are concept-detected by Zenith_ComponentMeta exactly like
// any other component; each hook fires the matching graph event on every slot.
// Dispatch reuses the hardened dispatch discipline: snapshot the heap-
// stable graph pointers before invoking user logic (nodes can spawn entities,
// which can relocate this component via a pool resize), track dispatch depth
// in a file-scope counter, defer slot removal until the outermost dispatch
// unwinds.
//
// Slot resolution: a slot whose .bgraph asset can't be loaded in this build
// keeps its path + override bytes verbatim (unresolved-slot contract) so a
// save round-trips it unchanged.
//------------------------------------------------------------------------------

struct Zenith_GraphSlot
{
	std::string m_strGraphAssetPath;			// "game:Graphs/Foo.bgraph"
	Zenith_BehaviourGraph* m_pxGraph = nullptr;	// null when unresolved
	Zenith_DataStream m_xPendingOverrides;		// override bytes preserved while unresolved
	bool m_bMarkedForRemoval = false;

	Zenith_GraphSlot() = default;
	~Zenith_GraphSlot() { delete m_pxGraph; }

	bool IsResolved() const { return m_pxGraph != nullptr; }

	Zenith_GraphSlot(const Zenith_GraphSlot&) = delete;
	Zenith_GraphSlot& operator=(const Zenith_GraphSlot&) = delete;

	Zenith_GraphSlot(Zenith_GraphSlot&& xOther) noexcept
		: m_strGraphAssetPath(std::move(xOther.m_strGraphAssetPath))
		, m_pxGraph(xOther.m_pxGraph)
		, m_xPendingOverrides(std::move(xOther.m_xPendingOverrides))
		, m_bMarkedForRemoval(xOther.m_bMarkedForRemoval)
	{
		xOther.m_pxGraph = nullptr;
		xOther.m_bMarkedForRemoval = false;
	}

	Zenith_GraphSlot& operator=(Zenith_GraphSlot&& xOther) noexcept
	{
		if (this != &xOther)
		{
			delete m_pxGraph;
			m_strGraphAssetPath = std::move(xOther.m_strGraphAssetPath);
			m_pxGraph = xOther.m_pxGraph;
			m_xPendingOverrides = std::move(xOther.m_xPendingOverrides);
			m_bMarkedForRemoval = xOther.m_bMarkedForRemoval;
			xOther.m_pxGraph = nullptr;
			xOther.m_bMarkedForRemoval = false;
		}
		return *this;
	}
};

class Zenith_GraphComponent
{
public:
	Zenith_GraphComponent(Zenith_Entity& xEntity) : m_xParentEntity(xEntity) {}
	~Zenith_GraphComponent() = default;	// slot dtors free the graphs

	Zenith_GraphComponent(const Zenith_GraphComponent&) = delete;
	Zenith_GraphComponent& operator=(const Zenith_GraphComponent&) = delete;

	Zenith_GraphComponent(Zenith_GraphComponent&& xOther) noexcept
		: m_axSlots(std::move(xOther.m_axSlots))
		, m_xParentEntity(xOther.m_xParentEntity)
#ifdef ZENITH_TOOLS
		, m_iPendingRemoveIndex(xOther.m_iPendingRemoveIndex)
#endif
	{
#ifdef ZENITH_TOOLS
		xOther.m_iPendingRemoveIndex = -1;
#endif
	}

	Zenith_GraphComponent& operator=(Zenith_GraphComponent&& xOther) noexcept
	{
		if (this != &xOther)
		{
			m_axSlots = std::move(xOther.m_axSlots);
			m_xParentEntity = xOther.m_xParentEntity;
#ifdef ZENITH_TOOLS
			m_iPendingRemoveIndex = xOther.m_iPendingRemoveIndex;
			xOther.m_iPendingRemoveIndex = -1;
#endif
		}
		return *this;
	}

	//--------------------------------------------------------------------------
	// Slots
	//--------------------------------------------------------------------------

	u_int GetGraphCount() const { return m_axSlots.GetSize(); }
	Zenith_BehaviourGraph* GetGraphAt(u_int uIndex);
	const char* GetGraphAssetPathAt(u_int uIndex) const;

	// Loads the .bgraph through Zenith_AssetRegistry and instantiates a runtime
	// graph in a new slot. Returns the graph (or null on load failure - the
	// slot is still appended unresolved so it round-trips).
	Zenith_BehaviourGraph* AddGraphByAssetPath(const char* szAssetPath);

	void RemoveGraphAt(u_int uIndex);
	void RemoveAllGraphs();
	void FlushPendingRemovals();

	// Fires a custom (string-named) event on every slot of THIS entity. The
	// optional payload reaches OnCustomEvent source nodes, which can store it
	// to a blackboard variable (the collision sources' packed-EntityID pattern).
	void FireCustomEvent(const char* szName, const Zenith_PropertyValue* pxPayload = nullptr);

	//--------------------------------------------------------------------------
	// Lifecycle hooks (concept-detected by Zenith_ComponentMeta)
	//--------------------------------------------------------------------------

	void OnStart();
	void OnEnable();
	void OnDisable();
	void OnUpdate(float fDt);
	void OnFixedUpdate(float fDt);
	void OnDestroy();
	void OnCollisionEnter(Zenith_Entity xOther);
	void OnCollisionStay(Zenith_Entity xOther);
	void OnCollisionExit(Zenith_EntityID xOtherID);

	//--------------------------------------------------------------------------
	// Serialization (version 1: framed slot blob - length-framed per slot)
	//--------------------------------------------------------------------------

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();

	// Hot reload (Zenith_GraphReload): re-instantiates every slot whose asset
	// path matches, migrating blackboard state name+type-matched. Must be
	// called at a safe point (never during dispatch). Returns the number of
	// slots reloaded.
	u_int ReloadSlotsForAsset(const char* szNormalizedPath);
#endif

	static bool IsDispatchInProgress();

private:
	void FireEventOnSlots(GraphEventType eEvent, float fDt, const Zenith_PropertyValue* pxPayload, bool bReverse = false);
	Zenith_BehaviourGraph* InstantiateSlotGraph(Zenith_GraphSlot& xSlot);

	Zenith_Vector<Zenith_GraphSlot> m_axSlots;
	Zenith_Entity m_xParentEntity;
#ifdef ZENITH_TOOLS
	int32_t m_iPendingRemoveIndex = -1;
	char m_acAddGraphPathBuffer[256] = {};
#endif
};
