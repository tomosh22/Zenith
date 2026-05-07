#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "AssetHandling/Zenith_ScriptAsset.h"
#include <string>
#include <cstring>

#ifdef ZENITH_TOOLS
#include "EntityComponent/Zenith_ComponentRegistry.h"
#endif

class Zenith_DataStream;

class Zenith_ScriptBehaviour {
	friend class Zenith_ScriptComponent;
public:
	virtual ~Zenith_ScriptBehaviour() {}

	//--------------------------------------------------------------------------
	// Lifecycle Hooks
	//--------------------------------------------------------------------------

	/**
	 * OnAwake - Called when behavior is first created/attached at RUNTIME.
	 * NOT called during scene deserialization.
	 * Use for: Initializing references, setting up state, procedural generation.
	 */
	virtual void OnAwake() {}

	/**
	 * OnStart - Called before the first OnUpdate, after all OnAwake calls.
	 * Called for ALL entities including those loaded from scene files.
	 * Use for: Initialization that depends on other components being ready.
	 */
	virtual void OnStart() {}

	/**
	 * OnEnable - Called when the entity becomes active in the hierarchy.
	 * Called after OnAwake when entity is first created.
	 * Also called when SetEnabled(true) is called on the entity.
	 */
	virtual void OnEnable() {}

	/**
	 * OnDisable - Called when the entity becomes inactive in the hierarchy.
	 * Called before OnDestroy during entity removal.
	 * Also called when SetEnabled(false) is called on the entity.
	 */
	virtual void OnDisable() {}

	/**
	 * OnUpdate - Called every frame.
	 */
	virtual void OnUpdate(float /*fDt*/) {}

	/**
	 * OnFixedUpdate - Called at fixed timestep intervals (default 50Hz).
	 * Use for: Physics-related logic, fixed-rate simulations.
	 */
	virtual void OnFixedUpdate(float /*fDt*/) {}

	/**
	 * OnLateUpdate - Called after all OnUpdate calls in a frame.
	 * Use for: Camera follow, post-update adjustments.
	 */
	virtual void OnLateUpdate(float /*fDt*/) {}

	/**
	 * OnDestroy - Called when behavior is destroyed.
	 */
	virtual void OnDestroy() {}

	// Physics collision callbacks - override to handle collision events
	// xOther is the entity that was collided with
	virtual void OnCollisionEnter(Zenith_Entity /*xOther*/) {}
	virtual void OnCollisionStay(Zenith_Entity /*xOther*/) {}
	virtual void OnCollisionExit(Zenith_EntityID /*uOtherID*/) {}  // Exit only gets ID since body may be destroyed

	// Return the unique type name for this behavior (used for serialization)
	virtual const char* GetBehaviourTypeName() const = 0;

	// Editor UI - override to render behavior-specific properties
	// Default implementation does nothing
	virtual void RenderPropertiesPanel() {}

	// Serialization of behavior-specific parameters
	// Override these to save/load custom behavior state
	virtual void WriteParametersToDataStream(Zenith_DataStream& /*xStream*/) const {}
	virtual void ReadParametersFromDataStream(Zenith_DataStream& /*xStream*/) {}

	Zenith_Entity& GetEntity() { return m_xParentEntity; }

	std::vector<Zenith_GUID> m_axGUIDRefs;

protected:
	Zenith_Entity m_xParentEntity;
};

//--------------------------------------------------------------------------
// Auto-registration macro
//--------------------------------------------------------------------------
//
// Usage: place ZENITH_BEHAVIOUR_TYPE_NAME(MyBehaviour) inside the class body.
//
// This generates:
//   - GetBehaviourTypeName() returning "MyBehaviour"
//   - CreateInstance(entity) factory function
//   - A static inline bool initialized via a registration lambda. The lambda
//     runs at program startup (before main()) and registers the factory with
//     Zenith_ScriptAsset's static factory map. This replaces the old per-game
//     XYZ_Behaviour::RegisterBehaviour() calls.
//
// The TU containing this macro must be linked into the final binary for
// auto-registration to take effect. Each game's Components/*.cpp files are
// compiled directly into the game executable, so this is automatic.
//
#define ZENITH_BEHAVIOUR_TYPE_NAME(TypeName) \
	virtual const char* GetBehaviourTypeName() const override { return #TypeName; } \
	static const char* GetBehaviourTypeNameStatic() { return #TypeName; } \
	static Zenith_ScriptBehaviour* CreateInstance(Zenith_Entity& xEntity) { return new TypeName(xEntity); } \
	static inline const bool s_bAutoRegistered_##TypeName = []() { \
		Zenith_ScriptAsset::RegisterFactory(#TypeName, &TypeName::CreateInstance); \
		return true; \
	}();

// Variant for test fixtures and runtime-only behaviours. The factory is registered the
// same way, so AddScript<T> / AttachScript / serialization all work. The behaviour is
// excluded from SyncRegisteredTypesToDisk and from the editor's "Add Script" popup, so
// no .zscript file gets written into game:Scripts/ for it. Use this for behaviours
// declared inside *.Tests.inl, ZENITH_TESTING-only fixtures, or behaviours that are
// only created programmatically and shouldn't appear as user-facing assets. See P2.4.
#define ZENITH_BEHAVIOUR_TYPE_NAME_INTERNAL(TypeName) \
	virtual const char* GetBehaviourTypeName() const override { return #TypeName; } \
	static const char* GetBehaviourTypeNameStatic() { return #TypeName; } \
	static Zenith_ScriptBehaviour* CreateInstance(Zenith_Entity& xEntity) { return new TypeName(xEntity); } \
	static inline const bool s_bAutoRegistered_##TypeName = []() { \
		Zenith_ScriptAsset::RegisterFactoryInternal(#TypeName, &TypeName::CreateInstance); \
		return true; \
	}();

//--------------------------------------------------------------------------
// Zenith_ScriptSlot - one attached script behaviour on an entity.
//
// A slot is "unresolved" when the C++ behaviour for its type isn't available
// in this build (compiled out, renamed, or temporarily unregistered). The slot
// keeps the asset path, the saved type name, and the raw serialized parameter
// bytes verbatim so a later save preserves the attachment - opening and
// re-saving a scene in a build missing a behaviour does NOT silently drop it.
//--------------------------------------------------------------------------
struct Zenith_ScriptSlot
{
	std::string             m_strScriptAssetPath;     // "game:Scripts/Foo.zscript"; empty for runtime-only / no asset
	std::string             m_strBehaviourTypeName;   // canonical type name (set whether resolved or not)
	Zenith_ScriptBehaviour* m_pxBehaviour = nullptr;  // null when unresolved (factory missing)
	Zenith_DataStream       m_xPendingParams;         // raw param bytes preserved when unresolved
	bool                    m_bMarkedForRemoval = false;  // set by RemoveScriptAt during dispatch; flushed post-dispatch

	Zenith_ScriptSlot() = default;

	~Zenith_ScriptSlot()
	{
		// Memory cleanup only - OnDestroy is dispatched separately by the component
		// in reverse slot order (Unity convention).
		delete m_pxBehaviour;
	}

	bool IsResolved() const { return m_pxBehaviour != nullptr; }

	// Move-only - vector storage requires this
	Zenith_ScriptSlot(const Zenith_ScriptSlot&) = delete;
	Zenith_ScriptSlot& operator=(const Zenith_ScriptSlot&) = delete;

	Zenith_ScriptSlot(Zenith_ScriptSlot&& xOther) noexcept
		: m_strScriptAssetPath(std::move(xOther.m_strScriptAssetPath))
		, m_strBehaviourTypeName(std::move(xOther.m_strBehaviourTypeName))
		, m_pxBehaviour(xOther.m_pxBehaviour)
		, m_xPendingParams(std::move(xOther.m_xPendingParams))
		, m_bMarkedForRemoval(xOther.m_bMarkedForRemoval)
	{
		xOther.m_pxBehaviour = nullptr;
		xOther.m_bMarkedForRemoval = false;
	}

	Zenith_ScriptSlot& operator=(Zenith_ScriptSlot&& xOther) noexcept
	{
		if (this != &xOther)
		{
			delete m_pxBehaviour;
			m_strScriptAssetPath = std::move(xOther.m_strScriptAssetPath);
			m_strBehaviourTypeName = std::move(xOther.m_strBehaviourTypeName);
			m_pxBehaviour = xOther.m_pxBehaviour;
			m_xPendingParams = std::move(xOther.m_xPendingParams);
			m_bMarkedForRemoval = xOther.m_bMarkedForRemoval;
			xOther.m_pxBehaviour = nullptr;
			xOther.m_bMarkedForRemoval = false;
		}
		return *this;
	}
};

//--------------------------------------------------------------------------
// Zenith_ScriptComponent - holds one or more script behaviour instances.
//--------------------------------------------------------------------------
//
// Mirrors Unity's MonoBehaviour-on-GameObject pattern: an entity can have
// many scripts attached, each an independent instance with its own state.
//
class Zenith_ScriptComponent
{
public:
	Zenith_ScriptComponent(Zenith_Entity& xEntity) : m_xParentEntity(xEntity) {}

	~Zenith_ScriptComponent()
	{
		// Memory cleanup only - OnDestroy is dispatched separately by the lifecycle
		// system (RemoveAllComponents) BEFORE this destructor runs.
		// Slot destructors handle behaviour deletion.
	}

	// Move-only
	Zenith_ScriptComponent(const Zenith_ScriptComponent&) = delete;
	Zenith_ScriptComponent& operator=(const Zenith_ScriptComponent&) = delete;

	Zenith_ScriptComponent(Zenith_ScriptComponent&& xOther) noexcept
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

	Zenith_ScriptComponent& operator=(Zenith_ScriptComponent&& xOther) noexcept
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

	//----------------------------------------------------------------------
	// Slot accessors
	//----------------------------------------------------------------------

	uint32_t GetScriptCount() const { return m_axSlots.GetSize(); }

	Zenith_ScriptBehaviour* GetScriptAt(uint32_t uIndex)
	{
		Zenith_Assert(uIndex < m_axSlots.GetSize(), "Zenith_ScriptComponent: GetScriptAt index out of range");
		return m_axSlots.Get(uIndex).m_pxBehaviour;
	}

	const char* GetScriptAssetPathAt(uint32_t uIndex) const
	{
		Zenith_Assert(uIndex < m_axSlots.GetSize(), "Zenith_ScriptComponent: GetScriptAssetPathAt index out of range");
		return m_axSlots.Get(uIndex).m_strScriptAssetPath.c_str();
	}

	//----------------------------------------------------------------------
	// Runtime add (Unity-style: APPEND, calls OnAwake, marks entity awoken)
	//----------------------------------------------------------------------

	template<typename T>
	T* AddScript()
	{
		T* pxBehaviour = new T(m_xParentEntity);
		pxBehaviour->m_xParentEntity = m_xParentEntity;
		AppendSlotInternal(Zenith_ScriptAsset::MakeAssetPath(pxBehaviour->GetBehaviourTypeName()), pxBehaviour);

		pxBehaviour->OnAwake();
		MarkParentAwokenIfValid();
		return pxBehaviour;
	}

	// Resolves the asset path through Zenith_AssetRegistry, instantiates via the bound factory,
	// and attaches as a new slot. Calls OnAwake. Returns the new behaviour or nullptr on failure.
	Zenith_ScriptBehaviour* AddScriptByAssetPath(const char* szAssetPath);

	//----------------------------------------------------------------------
	// Editor / serialization add (no OnAwake - lifecycle deferred)
	//----------------------------------------------------------------------

	template<typename T>
	T* AddScriptForSerialization()
	{
		T* pxBehaviour = new T(m_xParentEntity);
		pxBehaviour->m_xParentEntity = m_xParentEntity;
		AppendSlotInternal(Zenith_ScriptAsset::MakeAssetPath(pxBehaviour->GetBehaviourTypeName()), pxBehaviour);
		// OnAwake intentionally NOT called - dispatched when scene enters Play mode / on first frame
		return pxBehaviour;
	}

	Zenith_ScriptBehaviour* AddScriptForSerializationByAssetPath(const char* szAssetPath);

	//----------------------------------------------------------------------
	// Removal
	//----------------------------------------------------------------------

	void RemoveScriptAt(uint32_t uIndex);
	void RemoveAllScripts();

	//----------------------------------------------------------------------
	// Convenience lookup - returns first slot whose behaviour type matches T
	//----------------------------------------------------------------------

	template<typename T>
	T* GetScript()
	{
		// Linear scan by registered type name. Cheap because slot counts are tiny (typically 1-3).
		const char* szWantedName = T::GetBehaviourTypeNameStatic();
		for (uint32_t u = 0; u < m_axSlots.GetSize(); ++u)
		{
			Zenith_ScriptBehaviour* pxBehaviour = m_axSlots.Get(u).m_pxBehaviour;
			if (pxBehaviour && std::strcmp(pxBehaviour->GetBehaviourTypeName(), szWantedName) == 0)
			{
				return static_cast<T*>(pxBehaviour);
			}
		}
		return nullptr;
	}

	//----------------------------------------------------------------------
	// Lifecycle hooks - iterate all slots
	//----------------------------------------------------------------------

	void OnAwake();
	void OnStart();
	void OnEnable();
	void OnDisable();
	void OnUpdate(float fDt);
	void OnFixedUpdate(float fDt);
	void OnLateUpdate(float fDt);
	void OnDestroy();
	void OnCollisionEnter(Zenith_Entity xOther);
	void OnCollisionStay(Zenith_Entity xOther);
	void OnCollisionExit(Zenith_EntityID uOtherID);

	//----------------------------------------------------------------------
	// Serialization (multi-slot format, version 2)
	//----------------------------------------------------------------------

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
private:
	// Sub-panels of RenderPropertiesPanel. Public-facing API stays a single
	// call; the helpers exist only to flatten the panel function.
	void RenderScriptSlot(uint32_t uIndex, const Zenith_ScriptSlot& xSlot);
	void RenderAddScriptPopup();
	void AcceptScriptAssetDragDrop();
public:
#endif

	// Drains any slots marked for removal during dispatch. Called automatically at the end of
	// each lifecycle hook; safe to call externally too. Marked slots have their behaviour's
	// OnDestroy invoked then are erased from the slot vector.
	void FlushPendingRemovals();

	// True iff a dispatch is currently in progress on ANY ScriptComponent. RemoveScriptAt
	// uses this to decide whether to delete immediately or defer via m_bMarkedForRemoval.
	// Tracked via a file-scope counter (not a member) so that even if `this` is invalidated
	// mid-dispatch by a pool resize, the counter stays consistent.
	static bool IsDispatchInProgress();

private:
	void AppendSlotInternal(const std::string& strAssetPath, Zenith_ScriptBehaviour* pxBehaviour);
	void AppendUnresolvedSlotInternal(const std::string& strAssetPath,
	                                  const std::string& strBehaviourTypeName,
	                                  Zenith_DataStream&& xPendingParams);
	void MarkParentAwokenIfValid();

	Zenith_Vector<Zenith_ScriptSlot> m_axSlots;
	Zenith_Entity                    m_xParentEntity;
#ifdef ZENITH_TOOLS
	int32_t                          m_iPendingRemoveIndex = -1;  // deferred remove from UI
#endif
};
