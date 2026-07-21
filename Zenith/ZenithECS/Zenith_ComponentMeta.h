#pragma once

#include <string>
#include "Collections/Zenith_HashMap.h"
#include "Collections/Zenith_Vector.h"
#include <concepts>

// Include SceneData.h to get the Zenith_SceneData type (TransferComponent /
// uSCENE_VERSION_CURRENT) AND the Zenith_Entity component-template bodies
// (AddComponent / GetComponent / HasComponent), which now live at the bottom of
// Zenith_SceneData.h after Zenith_Scene became a leaf opaque handle (Phase 7b-1).
#include "ZenithECS/Zenith_SceneData.h"

// NOTE (ECS leaf-extraction Phase 4): this header is the ECS reflection CORE and
// must name no concrete component type, no editor component-registry, and no AI
// symbol. The editor "Add Component" registry side-effect that used to live in
// RegisterComponent<T> (and its include of the editor registry header here) has
// moved engine-side to Zenith_ComponentMeta_Registration.cpp, which owns the
// concrete-type knowledge and the editor-registry population.

// Implementation-detail metaprogramming (the type-erased function-pointer
// typedefs, the C++20 lifecycle-detection concepts, ComponentSchemaVersion<T>(),
// and the per-type free wrapper templates) lives here to keep this header's
// PUBLIC surface small. It is included BEFORE the public structs below because
// the function-pointer typedefs it defines are FIELD TYPES of those structs
// (PropertySetterFn on Zenith_PropertyDescriptor; the rest on
// Zenith_ComponentMeta). The only consumer of the relocated machinery is
// RegisterComponent<T> (defined at the bottom of this header).
#include "ZenithECS/Internal/Zenith_ComponentMeta_Detail.h"

class Zenith_DataStream;

//------------------------------------------------------------------------------
// Property reflection (used by prefab variant overrides)
//
// A property descriptor names a single field on a component and provides a
// type-erased setter that reads the new value from a Zenith_DataStream and
// writes it into the component instance. Components opt in by implementing:
//
//     static void RegisterProperties(Zenith_Vector<Zenith_PropertyDescriptor>& axProps)
//     {
//         ZENITH_REGISTER_COMPONENT_PROPERTY(MyComponent, m_xField, "FieldName")(axProps);
//     }
//
// Components that don't implement RegisterProperties simply have an empty
// property list, so they don't support variant overrides.
//
// First iteration supports FLAT property names only (e.g. "Position"). Nested
// paths like "Position.x" are detected by the caller (Zenith_Prefab) and
// emit a warning instead of being applied — they need a sub-field walker
// that doesn't yet exist.
//------------------------------------------------------------------------------

// NOTE: PropertySetterFn (the m_pfnSetter field type below) now lives in
// Internal/Zenith_ComponentMeta_Detail.h, included above.

struct Zenith_PropertyDescriptor
{
	std::string m_strName;          // e.g. "Position"
	PropertySetterFn m_pfnSetter;   // Reads from xValue and writes into the component
};

//------------------------------------------------------------------------------
// RELOCATED to Internal/Zenith_ComponentMeta_Detail.h (included above):
//   * the type-erased operation function-pointer typedefs (ComponentCreateFn,
//     ComponentHasFn, ComponentRemoveFn, ComponentSerializeFn,
//     ComponentDeserializeFn, ComponentTransferFn, ComponentGetRawFn) and the
//     lifecycle function-pointer typedefs (ComponentLifecycleFn,
//     ComponentUpdateFn) -- still named here as the m_pfn* field types of
//     Zenith_ComponentMeta below;
//   * the C++20 lifecycle-detection concepts (HasOnAwake/Start/Enable/Disable/
//     Update/LateUpdate/FixedUpdate/Destroy, HasRegisterProperties,
//     HasSchemaVersion, HasVersionedReadFromDataStream) and ComponentSchemaVersion<T>();
//   * the per-type free wrapper templates (Component*Wrapper / On*Wrapper).
// They are implementation detail of RegisterComponent<T> (below) only.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Component metadata structure
//------------------------------------------------------------------------------

struct Zenith_ComponentMeta
{
	std::string m_strTypeName;		// e.g., "MyComponent"
	u_int m_uSerializationOrder = 0u;	// Lower values serialize first (for dependencies)

	// Type-erased operations via function pointers
	ComponentCreateFn m_pfnCreate = nullptr;
	ComponentHasFn m_pfnHasComponent = nullptr;
	ComponentRemoveFn m_pfnRemoveComponent = nullptr;
	ComponentSerializeFn m_pfnSerialize = nullptr;
	ComponentDeserializeFn m_pfnDeserialize = nullptr;
	ComponentTransferFn m_pfnTransferComponent = nullptr;
	ComponentGetRawFn m_pfnGetRaw = nullptr;

	// Properties exposed for prefab-variant override application.
	// Empty for components that don't implement RegisterProperties.
	Zenith_Vector<Zenith_PropertyDescriptor> m_axProperties;

	// Lifecycle hooks (nullptr if component doesn't implement the hook)
	ComponentLifecycleFn m_pfnOnAwake = nullptr;    // Called when component is created
	ComponentLifecycleFn m_pfnOnStart = nullptr;    // Called before first update
	ComponentLifecycleFn m_pfnOnEnable = nullptr;   // Called when component is enabled
	ComponentLifecycleFn m_pfnOnDisable = nullptr;  // Called when component is disabled
	ComponentUpdateFn m_pfnOnUpdate = nullptr;      // Called every frame
	ComponentUpdateFn m_pfnOnLateUpdate = nullptr;  // Called after all OnUpdate calls
	ComponentUpdateFn m_pfnOnFixedUpdate = nullptr; // Called at fixed timestep
	ComponentLifecycleFn m_pfnOnDestroy = nullptr;  // Called before component is removed

	// Collision hooks - dispatched main-thread by the physics system through
	// DispatchOnCollision* below (no concrete component named physics-side).
	ComponentCollisionFn m_pfnOnCollisionEnter = nullptr;
	ComponentCollisionFn m_pfnOnCollisionStay = nullptr;
	ComponentCollisionExitFn m_pfnOnCollisionExit = nullptr;

	// Re-establish cross-entity references after a scene load (nullptr unless the
	// component implements ResolveEntityReferences). Invoked by the scene loader per
	// loaded entity, right after the parent-link resolution pass, with the
	// file-index -> new EntityID map. Concept-detected at registration.
	ComponentResolveRefsFn m_pfnResolveEntityReferences = nullptr;

	// On-disk schema version of this component's serialized payload. Written
	// per-component into scene v6+ files OUTSIDE the size-prefixed payload, so it
	// never disturbs the bounded/unknown-component skip path. Default 1; a component
	// overrides it via `static constexpr u_int uSchemaVersion`. Populated at
	// registration from ComponentSchemaVersion<T>(). Live opt-in: a concrete engine
	// component opts in (= 7, scene v7), whose versioned reader branches on the
	// persisted value to migrate pre-v7 (parent-in-blob) payloads.
	u_int m_uSchemaVersion = 1;
};

//------------------------------------------------------------------------------
// Component metadata registry (singleton)
//------------------------------------------------------------------------------

class Zenith_ComponentMetaRegistry
{
public:
	static Zenith_ComponentMetaRegistry& Get();

	// Register a component type. uOrder is the serialization order (lower values
	// serialize first, to respect inter-component dependencies). The ECS core
	// holds no name->order map any more -- the caller (the engine-side registrar
	// in Zenith_ComponentMeta_Registration.cpp, or the ZENITH_REGISTER_COMPONENT
	// macro) supplies the order explicitly.
	template<typename T>
	void RegisterComponent(const std::string& strTypeName, u_int uOrder);

	// Get metadata by type name
	const Zenith_ComponentMeta* GetMetaByName(const std::string& strTypeName) const;

	// Serialize all components of an entity to data stream
	void SerializeEntityComponents(Zenith_Entity& xEntity, Zenith_DataStream& xStream) const;

	// Deserialize components from data stream onto an entity.
	// uSceneVersion is the .zscen header version that produced this stream; it gates
	// whether the per-component schemaVersion field is consumed (scene v6+ only).
	// It defaults to the CURRENT scene version because every in-build round-trip
	// caller (prefab data, Zenith_Entity::ReadFromDataStream, the serialization unit
	// tests) pairs with SerializeEntityComponents, which now ALWAYS writes the field;
	// only the on-disk scene loader passes an older version for legacy v3/4/5 files.
	void DeserializeEntityComponents(Zenith_Entity& xEntity, Zenith_DataStream& xStream,
		u_int uSceneVersion = Zenith_SceneData::uSCENE_VERSION_CURRENT) const;

	// Get all registered component metas (sorted by serialization order)
	const Zenith_Vector<const Zenith_ComponentMeta*>& GetAllMetasSorted() const;

	// PURE. Counts ADJACENT pairs sharing a serialization order in a list already
	// sorted by that order; 0 means every order is unique. Public and pure so it can
	// be unit-tested against synthetic rows without standing up a second registry --
	// the registry is a boot-sealed singleton, so the collision logic would otherwise
	// only be reachable through Finalize().
	//
	// WHY THIS EXISTS: serialization order is a hand-assigned global namespace
	// (engine built-ins low, AI mid, per-game components above them), and Finalize()
	// sorts by it. Two components sharing an order are ordered ARBITRARILY against
	// each other -- and because the sort is by a single key, that ordering was not
	// even reproducible. A collision is therefore a silent, order-dependent
	// serialization hazard that grows with every component added. Finalize() now
	// reports one loudly rather than sorting it into a shrug.
	static u_int CountDuplicateSerializationOrders(
		const Zenith_Vector<const Zenith_ComponentMeta*>& xSortedMetas);

	// Check if registry is initialized with all components
	bool IsInitialized() const { return m_bInitialized; }

	// Install the engine-side registrar that registers all built-in components
	// (and, in TOOLS builds, populates the editor "Add Component" registry). The
	// ECS core stores it as an opaque function pointer so it never names any
	// concrete component type; EnsureInitialized invokes it lazily before the
	// first registry use. Set from Zenith_Engine::Initialise.
	void SetComponentRegistrar(void (*pfn)());

	// Enqueue a registrar thunk to be drained by EnsureInitialized. Called by the
	// ZENITH_REGISTER_COMPONENT macro's static initializer (which can fire before
	// main, hence the function-static backing list reached via this method) so a
	// game/external component registers itself without the ECS core naming it.
	void EnqueueRegistrarThunk(void (*pfn)());

	// NOTE: TransferAllComponents (Component Transfer) and RemoveAllComponents
	// (Component Removal) are internal-only and now live in the private section
	// below, reached through friendship by Zenith_SceneSystem / Zenith_SceneData.

	// ========== Property Reflection ==========

	// Apply a single property override to a component on the entity.
	// Reads the value from xValue (rewinds the stream cursor first) and writes
	// it into the component's field via the registered setter.
	// Returns false if the component or property is not registered.
	// Note: caller is responsible for rejecting nested paths ("Position.x") —
	// this API supports flat names only.
	bool SetComponentProperty(
		Zenith_Entity& xEntity,
		const std::string& strComponentName,
		const std::string& strPropertyName,
		Zenith_DataStream& xValue) const;

	// ========== Lifecycle Hook Dispatch ==========

	// Dispatch lifecycle hooks to all components of an entity.
	// NOTE: DispatchOnDisable is internal-only (called solely by Zenith_Entity /
	// Zenith_SceneData) and is private below. The remaining hooks have real
	// external callers (Prefab / Editor / tests) and stay public.
	void DispatchOnAwake(Zenith_Entity& xEntity) const;
	void DispatchOnStart(Zenith_Entity& xEntity) const;
	void DispatchOnEnable(Zenith_Entity& xEntity) const;
	void DispatchOnUpdate(Zenith_Entity& xEntity, float fDt) const;
	void DispatchOnLateUpdate(Zenith_Entity& xEntity, float fDt) const;
	void DispatchOnFixedUpdate(Zenith_Entity& xEntity, float fDt) const;
	void DispatchOnDestroy(Zenith_Entity& xEntity) const;

	// Collision dispatch - the physics system routes its (main-thread,
	// already-deferred) collision events through these so it never names a
	// concrete component. Any component implementing OnCollisionEnter/Stay/Exit
	// receives them (concept-detected at registration).
	void DispatchOnCollisionEnter(Zenith_Entity& xEntity, Zenith_Entity& xOther) const;
	void DispatchOnCollisionStay(Zenith_Entity& xEntity, Zenith_Entity& xOther) const;
	void DispatchOnCollisionExit(Zenith_Entity& xEntity, Zenith_EntityID xOtherID) const;

	// Re-establish cross-entity references on a loaded entity after a scene load. The
	// scene loader calls this per entity once the file-index -> EntityID map is built
	// (right after the parent-link resolution pass); any component implementing
	// ResolveEntityReferences (concept-detected) re-binds its stored entity handles.
	void DispatchResolveEntityReferences(Zenith_Entity& xEntity,
		const Zenith_HashMap<uint32_t, Zenith_EntityID>& xFileIndexToNewID) const;

	// Populate + seal the registry if it has not been initialized yet. Public so
	// Zenith_Engine::Initialise can force the one-time init eagerly (after
	// SetComponentRegistrar) at a deterministic point; it is also called lazily by
	// every const query/dispatch method below as a safety net. Idempotent.
	//
	// Names no concrete type: it drains the engine-installed registrar (built-ins)
	// then any pending macro thunks (game/external components), then sorts. Marked
	// const (callable from the const methods) but mutates through a non-const view
	// because the first call must build the meta list.
	void EnsureInitialized() const
	{
		if (!m_bInitialized)
		{
			Zenith_ComponentMetaRegistry* pxThis = const_cast<Zenith_ComponentMetaRegistry*>(this);
			if (pxThis->m_pfnRegisterComponents != nullptr)
			{
				pxThis->m_pfnRegisterComponents();
			}
			Zenith_Vector<void (*)()>& xThunks = PendingRegistrarThunks();
			for (u_int i = 0; i < xThunks.GetSize(); ++i)
			{
				xThunks.Get(i)();
			}
			pxThis->Finalize();
		}
	}

private:
	Zenith_ComponentMetaRegistry() = default;

	// Internal-only entry points, reached through friendship by the scene system
	// (cross-scene entity moves), scene storage (entity teardown / disable) and the
	// entity handle (hierarchy enable/disable propagation). Kept off the public
	// surface so the only ways in are the engine subsystems that own the relevant
	// lifecycle. Their only callers are verified ZenithECS-internal:
	//   * Finalize             -- self (EnsureInitialized)
	//   * TransferAllComponents -- Zenith_SceneSystem (Internal/Zenith_SceneSystem_EntityOwnership.cpp)
	//   * RemoveAllComponents   -- Zenith_SceneData   (Internal/Zenith_SceneData.cpp)
	//   * DispatchOnDisable     -- Zenith_Entity / Zenith_SceneData (Internal/*.cpp)
	friend class Zenith_SceneSystem;
	friend class Zenith_SceneData;
	friend class Zenith_Entity;

	// Build the sorted meta list and seal the registry. Called once by
	// EnsureInitialized after the installed registrar + pending thunks have run.
	// Names no concrete type -- it only sorts whatever was registered.
	void Finalize();

	// Transfer all components from source to target scene (move-construct, no serialize)
	void TransferAllComponents(Zenith_EntityID xEntityID, Zenith_SceneData* pxSource, Zenith_SceneData* pxTarget) const;

	// Remove all components from an entity (calls OnDestroy for each)
	void RemoveAllComponents(Zenith_Entity& xEntity) const;

	// Dispatch the OnDisable hook to all components of an entity (internal lifecycle)
	void DispatchOnDisable(Zenith_Entity& xEntity) const;

	// Process-level backing list for the ZENITH_REGISTER_COMPONENT macro's
	// thunks. Function-static (not a member) so the macro's static initializer
	// can enqueue before the singleton -- or main -- is constructed, dodging the
	// static-init-order fiasco. Drained once by EnsureInitialized.
	static Zenith_Vector<void (*)()>& PendingRegistrarThunks();

	template<typename T>
	static void ComponentTransferWrapper(Zenith_EntityID xEntityID, Zenith_SceneData* pxSource, Zenith_SceneData* pxTarget)
	{
		Zenith_SceneData::TransferComponent<T>(xEntityID, pxSource, pxTarget);
	}

	Zenith_HashMap<std::string, Zenith_ComponentMeta> m_xMetaByName;
	Zenith_Vector<const Zenith_ComponentMeta*> m_xMetasSorted;
	bool m_bInitialized = false;

	// Engine-installed registrar for the built-in components (and, in TOOLS, the
	// editor-registry population). Opaque function pointer so the ECS core names
	// no concrete type. Invoked once by EnsureInitialized. nullptr is tolerated
	// (e.g. tests that register components directly via the macro / explicit
	// RegisterComponent calls without installing a registrar).
	void (*m_pfnRegisterComponents)() = nullptr;
};

//------------------------------------------------------------------------------
// Template implementation (must be in header)
//------------------------------------------------------------------------------

// The per-type free wrapper templates that RegisterComponent<T> stamps out
// (Component{Create,Has,Remove,Serialize,Deserialize,GetRaw}Wrapper and
// On{Awake,Start,Enable,Disable,Update,LateUpdate,FixedUpdate,Destroy}Wrapper),
// the lifecycle-detection concepts, and ComponentSchemaVersion<T>() all live in
// Internal/Zenith_ComponentMeta_Detail.h (included at the top of this header).

//------------------------------------------------------------------------------
// RegisterComponent - detects and assigns lifecycle hooks via C++20 concepts
//------------------------------------------------------------------------------

template<typename T>
void Zenith_ComponentMetaRegistry::RegisterComponent(const std::string& strTypeName, u_int uOrder)
{
	Zenith_ComponentMeta xMeta;
	xMeta.m_strTypeName = strTypeName;
	xMeta.m_uSerializationOrder = uOrder;
	xMeta.m_pfnCreate = &ComponentCreateWrapper<T>;
	xMeta.m_pfnHasComponent = &ComponentHasWrapper<T>;
	xMeta.m_pfnRemoveComponent = &ComponentRemoveWrapper<T>;
	xMeta.m_pfnSerialize = &ComponentSerializeWrapper<T>;
	xMeta.m_pfnDeserialize = &ComponentDeserializeWrapper<T>;
	xMeta.m_pfnTransferComponent = &ComponentTransferWrapper<T>;
	xMeta.m_pfnGetRaw = &ComponentGetRawWrapper<T>;

	// If the component implements RegisterProperties, give it a chance to
	// declare its overrideable fields. Components without the static method
	// simply have an empty m_axProperties.
	if constexpr (HasRegisterProperties<T>)
	{
		T::RegisterProperties(xMeta.m_axProperties);
	}

	// Record the component's on-disk schema version (default 1; a component
	// overrides via `static constexpr u_int uSchemaVersion`). Written per-component
	// into scene v6+ files. Live opt-in: a concrete engine component opts in (scene v7).
	xMeta.m_uSchemaVersion = ComponentSchemaVersion<T>();

	// Detect and assign lifecycle hooks using C++20 concepts
	// Hook pointers remain nullptr if component doesn't implement the hook
	if constexpr (HasOnAwake<T>)
	{
		xMeta.m_pfnOnAwake = &OnAwakeWrapper<T>;
	}
	if constexpr (HasOnStart<T>)
	{
		xMeta.m_pfnOnStart = &OnStartWrapper<T>;
	}
	if constexpr (HasOnEnable<T>)
	{
		xMeta.m_pfnOnEnable = &OnEnableWrapper<T>;
	}
	if constexpr (HasOnDisable<T>)
	{
		xMeta.m_pfnOnDisable = &OnDisableWrapper<T>;
	}
	if constexpr (HasOnUpdate<T>)
	{
		xMeta.m_pfnOnUpdate = &OnUpdateWrapper<T>;
	}
	if constexpr (HasOnLateUpdate<T>)
	{
		xMeta.m_pfnOnLateUpdate = &OnLateUpdateWrapper<T>;
	}
	if constexpr (HasOnFixedUpdate<T>)
	{
		xMeta.m_pfnOnFixedUpdate = &OnFixedUpdateWrapper<T>;
	}
	if constexpr (HasOnDestroy<T>)
	{
		xMeta.m_pfnOnDestroy = &OnDestroyWrapper<T>;
	}
	if constexpr (HasOnCollisionEnter<T>)
	{
		xMeta.m_pfnOnCollisionEnter = &OnCollisionEnterWrapper<T>;
	}
	if constexpr (HasOnCollisionStay<T>)
	{
		xMeta.m_pfnOnCollisionStay = &OnCollisionStayWrapper<T>;
	}
	if constexpr (HasOnCollisionExit<T>)
	{
		xMeta.m_pfnOnCollisionExit = &OnCollisionExitWrapper<T>;
	}
	if constexpr (HasResolveEntityReferences<T>)
	{
		xMeta.m_pfnResolveEntityReferences = &ResolveEntityReferencesWrapper<T>;
	}

	m_xMetaByName[strTypeName] = xMeta;

	// Game components register from project hooks AFTER the engine boot already
	// finalized the registry (built m_xMetasSorted). A post-seal registration
	// must re-finalize: (a) so the new type enters the sorted dispatch +
	// serialization list at all, and (b) because the map insert may have
	// rehashed and invalidated every cached Zenith_ComponentMeta* in the sorted
	// list. Finalize() rebuilds the pointers from the map, so it cures both.
	if (m_bInitialized)
	{
		Finalize();
	}

	// NOTE (ECS leaf-extraction Phase 4): the editor "Add Component" registry
	// population that used to live here (under #ifdef ZENITH_TOOLS) has moved
	// engine-side to Zenith_ComponentMeta_Registration.cpp, so the ECS core no
	// longer names the editor registry type. The engine registrar mirrors every
	// built-in into the editor registry there, preserving the menu.
}

//------------------------------------------------------------------------------
// Registration macro for component headers
//------------------------------------------------------------------------------

// Use this macro to register a component type at startup.
// Place in a .cpp file (usually the component's implementation file).
//
// Two forms:
//   ZENITH_REGISTER_COMPONENT(Type, "Name")          -- order defaults to 1000
//   ZENITH_REGISTER_COMPONENT(Type, "Name", Order)   -- explicit serialization order
//
// The static initializer does NOT register eagerly. Instead it ENQUEUES a
// captureless thunk (void(*)()) onto the ECS-owned pending list; EnsureInitialized
// drains the list on first registry use (after the engine-installed built-in
// registrar runs). This keeps registration order deterministic and avoids the
// static-init-order fiasco. RegisterComponent overwrites by name, so re-running a
// thunk is harmless.
//
// NOTE on dead-strip safety: like any static initializer in a static lib, this
// thunk only fires if the linker keeps the owning .obj (MSVC /OPT:REF strips
// unreferenced TUs). Engine built-ins therefore DO NOT rely on the macro -- they
// register via the engine-installed registrar (always linked). The macro path is
// for game/external components whose .obj the game EXE references directly.
#define ZENITH_REGISTER_COMPONENT_3(ComponentType, TypeName, Order) \
	namespace \
	{ \
		struct ComponentType##_AutoRegister \
		{ \
			ComponentType##_AutoRegister() \
			{ \
				Zenith_ComponentMetaRegistry::Get().EnqueueRegistrarThunk( \
					+[]() \
					{ \
						Zenith_ComponentMetaRegistry::Get().RegisterComponent<ComponentType>(TypeName, (Order)); \
					}); \
			} \
		}; \
		static ComponentType##_AutoRegister s_x##ComponentType##_AutoRegister; \
	}

// 2-arg form: default the order to 1000 (the old "unknown component" default).
#define ZENITH_REGISTER_COMPONENT_2(ComponentType, TypeName) \
	ZENITH_REGISTER_COMPONENT_3(ComponentType, TypeName, 1000u)

// Arg-count dispatch: pick the 3-arg or 2-arg expansion based on how many
// arguments were supplied.
//
// ZENITH_REGISTER_COMPONENT_EXPAND is the standard MSVC indirection: the engine
// builds with the TRADITIONAL (non-conformant) MSVC preprocessor -- no
// /Zc:preprocessor -- which expands __VA_ARGS__ as a SINGLE token when forwarded
// to GET_MACRO, breaking the arg count. Routing the whole GET_MACRO(...) call
// through EXPAND forces a re-scan so the variadic args are counted correctly.
// Harmless under a conformant preprocessor.
#define ZENITH_REGISTER_COMPONENT_EXPAND(x) x
#define ZENITH_REGISTER_COMPONENT_GET_MACRO(_1, _2, _3, NAME, ...) NAME
#define ZENITH_REGISTER_COMPONENT(...) \
	ZENITH_REGISTER_COMPONENT_EXPAND( \
		ZENITH_REGISTER_COMPONENT_GET_MACRO(__VA_ARGS__, ZENITH_REGISTER_COMPONENT_3, ZENITH_REGISTER_COMPONENT_2)(__VA_ARGS__))

//------------------------------------------------------------------------------
// Property registration macros
//
// Use inside a static T::RegisterProperties(Zenith_Vector<Zenith_PropertyDescriptor>& a)
// method on a component to declare overrideable fields.
//
// CHOOSE THE RIGHT MACRO based on whether writing the field has side effects:
//
//   PROPERTY        — raw member-field write. Use ONLY for plain data fields
//                     where the renderer/physics/etc. re-reads the value each
//                     frame (e.g. Light::m_xColor, Light::m_fIntensity). The
//                     setter performs `xValue >> field;` and nothing else.
//
//   PROPERTY_SETTER — invokes a member function with the deserialised value.
//                     Use for STATEFUL fields whose written value drives derived
//                     runtime state — physics body sync, asset loading, mesh
//                     regeneration, etc. Examples: a position setter that drives
//                     the Jolt BodyInterface, a scale setter that rebuilds
//                     colliders. A raw field write would corrupt the entity by
//                     leaving runtime state pointing at the old value.
//
//   PROPERTY_CUSTOM — accepts a hand-written lambda body. Use when neither
//                     macro fits — e.g. when the deserialised value type
//                     differs from what the setter expects (Model takes a path
//                     string but the override stores a ModelHandle).
//
// Example:
//   static void RegisterProperties(Zenith_Vector<Zenith_PropertyDescriptor>& a) {
//       // Stateful: must use the setter so side effects (e.g. physics sync) run.
//       ZENITH_REGISTER_COMPONENT_PROPERTY_SETTER(
//           MyComponent, &MyComponent::SetPosition,
//           Zenith_Maths::Vector3, "Position", a);
//
//       // Plain data: raw write is fine.
//       ZENITH_REGISTER_COMPONENT_PROPERTY(MyComponent, m_xColor, "Color", a);
//   }
//------------------------------------------------------------------------------
#define ZENITH_REGISTER_COMPONENT_PROPERTY(ComponentType, MemberField, Name, AxPropertiesVar)              \
	do                                                                                                     \
	{                                                                                                      \
		Zenith_PropertyDescriptor xDesc;                                                                   \
		xDesc.m_strName = Name;                                                                            \
		xDesc.m_pfnSetter = [](void* pxComp, Zenith_DataStream& xValue) {                                  \
			xValue >> static_cast<ComponentType*>(pxComp)->MemberField;                                    \
		};                                                                                                 \
		(AxPropertiesVar).PushBack(xDesc);                                                                 \
	} while (0)

#define ZENITH_REGISTER_COMPONENT_PROPERTY_SETTER(ComponentType, MemberFn, ValueType, Name, AxPropertiesVar) \
	do                                                                                                       \
	{                                                                                                        \
		Zenith_PropertyDescriptor xDesc;                                                                     \
		xDesc.m_strName = Name;                                                                              \
		xDesc.m_pfnSetter = [](void* pxComp, Zenith_DataStream& xValue) {                                    \
			ValueType xVal;                                                                                  \
			xValue >> xVal;                                                                                  \
			(static_cast<ComponentType*>(pxComp)->*MemberFn)(xVal);                                          \
		};                                                                                                   \
		(AxPropertiesVar).PushBack(xDesc);                                                                   \
	} while (0)

// Hand-written-body variant. Inside Body, the parameters available are:
//   ComponentType* pxComp     (typed component pointer)
//   Zenith_DataStream& xValue (override value stream, cursor at start)
// Use when the override's wire-format type and the component's setter take
// different types (e.g. ModelHandle in the stream, std::string into LoadModel).
#define ZENITH_REGISTER_COMPONENT_PROPERTY_CUSTOM(ComponentType, Name, AxPropertiesVar, Body)              \
	do                                                                                                     \
	{                                                                                                      \
		Zenith_PropertyDescriptor xDesc;                                                                   \
		xDesc.m_strName = Name;                                                                            \
		xDesc.m_pfnSetter = [](void* pxRawComp, Zenith_DataStream& xValue) {                               \
			ComponentType* pxComp = static_cast<ComponentType*>(pxRawComp);                                \
			(void)pxComp; (void)xValue;                                                                    \
			Body                                                                                           \
		};                                                                                                 \
		(AxPropertiesVar).PushBack(xDesc);                                                                 \
	} while (0)
