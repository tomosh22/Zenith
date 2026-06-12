#pragma once

//------------------------------------------------------------------------------
// Zenith_ComponentMeta_Detail.h
//
// IMPLEMENTATION-DETAIL metaprogramming for the component-meta registry. Nothing
// here is part of the PUBLIC ComponentMeta surface: these are the type-erased
// function-pointer typedefs, the C++20 lifecycle-detection concepts, and the free
// wrapper templates that the registry's RegisterComponent<T> body stamps out per
// component type. They are pulled out of Zenith_ComponentMeta.h purely to shrink
// that header's public surface -- the only consumer is RegisterComponent<T> (and
// the few const query/dispatch methods) defined alongside the PUBLIC types in
// Zenith_ComponentMeta.h, which includes THIS header before defining them.
//
// Layout note (why this is a single header included near the TOP of the public
// header): the 10 function-pointer typedefs below are FIELD TYPES of the public
// structs (PropertySetterFn on Zenith_PropertyDescriptor; the other nine on
// Zenith_ComponentMeta), so they must be visible before those structs are defined
// -- hence the include sits before them. Zenith_PropertyDescriptor itself stays
// public; it is forward-declared here so PropertySetterFn and the
// HasRegisterProperties concept can name it, then fully defined back in the public
// header.
//
// As with the public header, this stays in the GLOBAL namespace and names no
// concrete component type, no Flux/Physics/engine symbol -- it is ECS-leaf-safe.
//------------------------------------------------------------------------------

#include <concepts>

// Pulls Zenith_Entity (+ its AddComponent/GetComponent/HasComponent/RemoveComponent
// template bodies via Zenith_Entity.inl), Zenith_EntityID, Zenith_SceneData
// (TransferComponent<T>) and Zenith_Vector -- everything the wrappers and concepts
// below name. Zenith_SceneData.h does NOT include Zenith_ComponentMeta.h, so this
// does not close an include cycle. #pragma once makes the re-include from the
// public header harmless.
#include "ZenithECS/Zenith_SceneData.h"

// Defined in DataStream/Zenith_DataStream.h. Only ever named by reference here, so
// a forward declaration is enough (matches the public header, which also only
// forward-declares it).
class Zenith_DataStream;

// Defined fully in Zenith_ComponentMeta.h (kept public). Forward-declared so
// PropertySetterFn and the HasRegisterProperties concept can name it; the concept
// body is only instantiated once the full type is visible (at RegisterComponent<T>
// use, well after the public header completes the definition).
struct Zenith_PropertyDescriptor;

//------------------------------------------------------------------------------
// Property setter function pointer (field type of Zenith_PropertyDescriptor)
//------------------------------------------------------------------------------

// Reads the new value from xValue and writes it into the component instance.
using PropertySetterFn = void(*)(void* pxComponent, Zenith_DataStream& xValue);

//------------------------------------------------------------------------------
// Function pointer types for type-erased component operations
// (field types of Zenith_ComponentMeta)
//------------------------------------------------------------------------------

// Create a component on an entity (returns void, component reference obtained separately)
using ComponentCreateFn = void(*)(Zenith_Entity&);

// Check if entity has this component type
using ComponentHasFn = bool(*)(Zenith_Entity&);

// Remove component from entity
using ComponentRemoveFn = void(*)(Zenith_Entity&);

// Serialize component to data stream
using ComponentSerializeFn = void(*)(Zenith_Entity&, Zenith_DataStream&);

// Deserialize component from data stream. The trailing u_int carries the
// per-component schemaVersion read from the scene file (scene v6+); it is 1 for
// pre-v6 files / callers that don't track a schema. Components opt in to seeing
// it via HasVersionedReadFromDataStream<T>; the rest ignore it (see wrapper).
using ComponentDeserializeFn = void(*)(Zenith_Entity&, Zenith_DataStream&, u_int);

// Transfer (move-construct) a component from source scene to target scene
using ComponentTransferFn = void(*)(Zenith_EntityID, Zenith_SceneData*, Zenith_SceneData*);

// Get a void* to the component on this entity (or nullptr if absent).
// Used by SetComponentProperty for type-erased property writes.
using ComponentGetRawFn = void*(*)(Zenith_Entity&);

//------------------------------------------------------------------------------
// Lifecycle hook function pointer types
//------------------------------------------------------------------------------

// Called when component is created (after constructor)
using ComponentLifecycleFn = void(*)(Zenith_Entity&);

// Called every frame with delta time
using ComponentUpdateFn = void(*)(Zenith_Entity&, float);

// Collision callbacks - dispatched on the MAIN thread by the physics system
// through the meta registry (the physics dispatch path names no concrete
// component). Enter/Stay carry the other entity; Exit carries only the ID
// because the other body may already be destroyed.
using ComponentCollisionFn = void(*)(Zenith_Entity&, Zenith_Entity&);
using ComponentCollisionExitFn = void(*)(Zenith_Entity&, Zenith_EntityID);

//------------------------------------------------------------------------------
// C++20 Concepts for optional lifecycle hooks
//------------------------------------------------------------------------------

template<typename T>
concept HasOnAwake = requires(T& t) { { t.OnAwake() } -> std::same_as<void>; };

template<typename T>
concept HasOnStart = requires(T& t) { { t.OnStart() } -> std::same_as<void>; };

template<typename T>
concept HasOnEnable = requires(T& t) { { t.OnEnable() } -> std::same_as<void>; };

template<typename T>
concept HasOnDisable = requires(T& t) { { t.OnDisable() } -> std::same_as<void>; };

template<typename T>
concept HasOnUpdate = requires(T& t, float fDt) { { t.OnUpdate(fDt) } -> std::same_as<void>; };

template<typename T>
concept HasOnLateUpdate = requires(T& t, float fDt) { { t.OnLateUpdate(fDt) } -> std::same_as<void>; };

template<typename T>
concept HasOnFixedUpdate = requires(T& t, float fDt) { { t.OnFixedUpdate(fDt) } -> std::same_as<void>; };

template<typename T>
concept HasOnDestroy = requires(T& t) { { t.OnDestroy() } -> std::same_as<void>; };

template<typename T>
concept HasOnCollisionEnter = requires(T& t, Zenith_Entity& xOther) { { t.OnCollisionEnter(xOther) } -> std::same_as<void>; };

template<typename T>
concept HasOnCollisionStay = requires(T& t, Zenith_Entity& xOther) { { t.OnCollisionStay(xOther) } -> std::same_as<void>; };

template<typename T>
concept HasOnCollisionExit = requires(T& t, Zenith_EntityID xOtherID) { { t.OnCollisionExit(xOtherID) } -> std::same_as<void>; };

template<typename T>
concept HasRegisterProperties = requires(Zenith_Vector<Zenith_PropertyDescriptor>& a) {
	{ T::RegisterProperties(a) } -> std::same_as<void>;
};

// Optional: a component declares the on-disk schema version of its serialized
// payload (see Zenith_ComponentMeta::m_uSchemaVersion). A component opts in by
// adding `static constexpr u_int uSchemaVersion = N;`. Detected at registration
// exactly like the lifecycle hooks. Live opt-in: a concrete engine component opts
// in (uSchemaVersion = 7, scene v7 -- parent moved out of that component's blob).
template<typename T>
concept HasSchemaVersion = requires { { T::uSchemaVersion } -> std::convertible_to<u_int>; };

// The schema version a component declares, or the default 1 if it declares none.
// 0 is reserved to mean "legacy / unversioned" so a future migration can tell a
// pre-schema payload from a deliberate v1.
template<typename T>
static constexpr u_int ComponentSchemaVersion()
{
	if constexpr (HasSchemaVersion<T>)
	{
		return T::uSchemaVersion;
	}
	else
	{
		return 1u;
	}
}

// Optional: a component provides a schema-version-aware overload of
// ReadFromDataStream so a future migration can branch on the persisted schema
// without breaking the single-arg signature every current component uses. The
// deserialize wrapper prefers this overload when present (see
// ComponentDeserializeWrapper); otherwise it falls back to the single-arg form.
template<typename T>
concept HasVersionedReadFromDataStream = requires(T& t, Zenith_DataStream& s, u_int v) {
	t.ReadFromDataStream(s, v);
};

//------------------------------------------------------------------------------
// Static wrapper functions for type-erased operations
// (instantiated per component type by Zenith_ComponentMetaRegistry::RegisterComponent<T>)
//------------------------------------------------------------------------------

template<typename T>
static void ComponentCreateWrapper(Zenith_Entity& xEntity)
{
	xEntity.AddComponent<T>();
}

template<typename T>
static bool ComponentHasWrapper(Zenith_Entity& xEntity)
{
	return xEntity.HasComponent<T>();
}

template<typename T>
static void ComponentRemoveWrapper(Zenith_Entity& xEntity)
{
	xEntity.RemoveComponent<T>();
}

template<typename T>
static void ComponentSerializeWrapper(Zenith_Entity& xEntity, Zenith_DataStream& xStream)
{
	xEntity.GetComponent<T>().WriteToDataStream(xStream);
}

template<typename T>
static void ComponentDeserializeWrapper(Zenith_Entity& xEntity, Zenith_DataStream& xStream, u_int uSchemaVersion)
{
	// Create the component if the entity doesn't already have it. Phase 7a: scene
	// load creates entities BARE, so even the owning component is added here from the stream
	// (via this type-erased thunk) -- the loader never names a concrete component.
	if (!xEntity.HasComponent<T>())
	{
		xEntity.AddComponent<T>();
	}
	// Prefer the schema-version-aware overload when the component provides one;
	// otherwise call the single-arg form and drop the version on the floor.
	// A concrete engine component opts in (uSchemaVersion = 7) to migrate the pre-v7
	// parent-in-blob layout; the rest still use the single-arg form.
	if constexpr (HasVersionedReadFromDataStream<T>)
	{
		xEntity.GetComponent<T>().ReadFromDataStream(xStream, uSchemaVersion);
	}
	else
	{
		xEntity.GetComponent<T>().ReadFromDataStream(xStream);
		(void)uSchemaVersion;
	}
}

//------------------------------------------------------------------------------
// Lifecycle hook wrapper functions (called via function pointers)
//------------------------------------------------------------------------------

template<typename T>
static void OnAwakeWrapper(Zenith_Entity& xEntity)
{
	xEntity.GetComponent<T>().OnAwake();
}

template<typename T>
static void OnStartWrapper(Zenith_Entity& xEntity)
{
	xEntity.GetComponent<T>().OnStart();
}

template<typename T>
static void OnEnableWrapper(Zenith_Entity& xEntity)
{
	xEntity.GetComponent<T>().OnEnable();
}

template<typename T>
static void OnDisableWrapper(Zenith_Entity& xEntity)
{
	xEntity.GetComponent<T>().OnDisable();
}

template<typename T>
static void OnUpdateWrapper(Zenith_Entity& xEntity, float fDt)
{
	xEntity.GetComponent<T>().OnUpdate(fDt);
}

template<typename T>
static void OnLateUpdateWrapper(Zenith_Entity& xEntity, float fDt)
{
	xEntity.GetComponent<T>().OnLateUpdate(fDt);
}

template<typename T>
static void OnFixedUpdateWrapper(Zenith_Entity& xEntity, float fDt)
{
	xEntity.GetComponent<T>().OnFixedUpdate(fDt);
}

template<typename T>
static void OnDestroyWrapper(Zenith_Entity& xEntity)
{
	xEntity.GetComponent<T>().OnDestroy();
}

template<typename T>
static void OnCollisionEnterWrapper(Zenith_Entity& xEntity, Zenith_Entity& xOther)
{
	xEntity.GetComponent<T>().OnCollisionEnter(xOther);
}

template<typename T>
static void OnCollisionStayWrapper(Zenith_Entity& xEntity, Zenith_Entity& xOther)
{
	xEntity.GetComponent<T>().OnCollisionStay(xOther);
}

template<typename T>
static void OnCollisionExitWrapper(Zenith_Entity& xEntity, Zenith_EntityID xOtherID)
{
	xEntity.GetComponent<T>().OnCollisionExit(xOtherID);
}

template<typename T>
static void* ComponentGetRawWrapper(Zenith_Entity& xEntity)
{
	if (!xEntity.HasComponent<T>()) return nullptr;
	return &xEntity.GetComponent<T>();
}
