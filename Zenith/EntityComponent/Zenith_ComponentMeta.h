#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <concepts>

// Include Scene.h to get Entity template implementations (they're defined there)
#include "EntityComponent/Zenith_Scene.h"
#include "Collections/Zenith_Vector.h"

#ifdef ZENITH_TOOLS
#include "EntityComponent/Zenith_ComponentRegistry.h"
#endif

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

using PropertySetterFn = void(*)(void* pxComponent, Zenith_DataStream& xValue);

struct Zenith_PropertyDescriptor
{
	std::string m_strName;          // e.g. "Position"
	PropertySetterFn m_pfnSetter;   // Reads from xValue and writes into the component
};

//------------------------------------------------------------------------------
// Function pointer types for type-erased component operations
//------------------------------------------------------------------------------

// Create a component on an entity (returns void, component reference obtained separately)
using ComponentCreateFn = void(*)(Zenith_Entity&);

// Check if entity has this component type
using ComponentHasFn = bool(*)(Zenith_Entity&);

// Remove component from entity
using ComponentRemoveFn = void(*)(Zenith_Entity&);

// Serialize component to data stream
using ComponentSerializeFn = void(*)(Zenith_Entity&, Zenith_DataStream&);

// Deserialize component from data stream
using ComponentDeserializeFn = void(*)(Zenith_Entity&, Zenith_DataStream&);

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
concept HasRegisterProperties = requires(Zenith_Vector<Zenith_PropertyDescriptor>& a) {
	{ T::RegisterProperties(a) } -> std::same_as<void>;
};

//------------------------------------------------------------------------------
// Component metadata structure
//------------------------------------------------------------------------------

struct Zenith_ComponentMeta
{
	std::string m_strTypeName;		// e.g., "TransformComponent"
	u_int m_uSerializationOrder;	// Lower values serialize first (for dependencies)

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
};

//------------------------------------------------------------------------------
// Component metadata registry (singleton)
//------------------------------------------------------------------------------

class Zenith_ComponentMetaRegistry
{
public:
	static Zenith_ComponentMetaRegistry& Get();

	// Register a component type (order is determined automatically by type name)
	template<typename T>
	void RegisterComponent(const std::string& strTypeName);

	// Get metadata by type name
	const Zenith_ComponentMeta* GetMetaByName(const std::string& strTypeName) const;

	// Serialize all components of an entity to data stream
	void SerializeEntityComponents(Zenith_Entity& xEntity, Zenith_DataStream& xStream) const;

	// Deserialize components from data stream onto an entity
	void DeserializeEntityComponents(Zenith_Entity& xEntity, Zenith_DataStream& xStream) const;

	// Get all registered component metas (sorted by serialization order)
	const std::vector<const Zenith_ComponentMeta*>& GetAllMetasSorted() const;

	// Check if registry is initialized with all components
	bool IsInitialized() const { return m_bInitialized; }

	// Called after all components are registered
	void FinalizeRegistration();

	// Get the serialization order for a component type name
	// Order is hardcoded to ensure dependencies are respected
	static u_int GetSerializationOrder(const std::string& strTypeName);

	// ========== Component Transfer ==========

	// Transfer all components from source to target scene (move-construct, no serialize)
	void TransferAllComponents(Zenith_EntityID xEntityID, Zenith_SceneData* pxSource, Zenith_SceneData* pxTarget) const;

	// ========== Component Removal ==========

	// Remove all components from an entity (calls OnDestroy for each)
	void RemoveAllComponents(Zenith_Entity& xEntity) const;

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

	// Dispatch lifecycle hooks to all components of an entity
	void DispatchOnAwake(Zenith_Entity& xEntity) const;
	void DispatchOnStart(Zenith_Entity& xEntity) const;
	void DispatchOnEnable(Zenith_Entity& xEntity) const;
	void DispatchOnDisable(Zenith_Entity& xEntity) const;
	void DispatchOnUpdate(Zenith_Entity& xEntity, float fDt) const;
	void DispatchOnLateUpdate(Zenith_Entity& xEntity, float fDt) const;
	void DispatchOnFixedUpdate(Zenith_Entity& xEntity, float fDt) const;
	void DispatchOnDestroy(Zenith_Entity& xEntity) const;

private:
	Zenith_ComponentMetaRegistry() = default;

	void EnsureInitialized() const
	{
		if (!m_bInitialized)
		{
			const_cast<Zenith_ComponentMetaRegistry*>(this)->FinalizeRegistration();
		}
	}

	template<typename T>
	static void ComponentTransferWrapper(Zenith_EntityID xEntityID, Zenith_SceneData* pxSource, Zenith_SceneData* pxTarget)
	{
		Zenith_SceneData::TransferComponent<T>(xEntityID, pxSource, pxTarget);
	}

	std::unordered_map<std::string, Zenith_ComponentMeta> m_xMetaByName;
	std::vector<const Zenith_ComponentMeta*> m_xMetasSorted;
	bool m_bInitialized = false;
};

//------------------------------------------------------------------------------
// Template implementation (must be in header)
//------------------------------------------------------------------------------

// Static wrapper functions for type-erased operations
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
static void ComponentDeserializeWrapper(Zenith_Entity& xEntity, Zenith_DataStream& xStream)
{
	// Special case: TransformComponent is already created by entity constructor
	// For other components, create if not present
	if (!xEntity.HasComponent<T>())
	{
		xEntity.AddComponent<T>();
	}
	xEntity.GetComponent<T>().ReadFromDataStream(xStream);
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
static void* ComponentGetRawWrapper(Zenith_Entity& xEntity)
{
	if (!xEntity.HasComponent<T>()) return nullptr;
	return &xEntity.GetComponent<T>();
}

//------------------------------------------------------------------------------
// RegisterComponent - detects and assigns lifecycle hooks via C++20 concepts
//------------------------------------------------------------------------------

template<typename T>
void Zenith_ComponentMetaRegistry::RegisterComponent(const std::string& strTypeName)
{
	Zenith_ComponentMeta xMeta;
	xMeta.m_strTypeName = strTypeName;
	xMeta.m_uSerializationOrder = GetSerializationOrder(strTypeName);
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

	m_xMetaByName[strTypeName] = xMeta;

	// In editor builds, also register with the editor's ComponentRegistry
	// This ensures all components appear in the "Add Component" menu automatically
#ifdef ZENITH_TOOLS
	Zenith_ComponentRegistry::Get().RegisterComponent<T>(strTypeName);
#endif
}

//------------------------------------------------------------------------------
// Registration macro for component headers
//------------------------------------------------------------------------------

// Use this macro to register a component type at startup
// Place in a .cpp file (usually the component's implementation file)
// Serialization order is determined automatically based on type name
#define ZENITH_REGISTER_COMPONENT(ComponentType, TypeName) \
	namespace \
	{ \
		struct ComponentType##_AutoRegister \
		{ \
			ComponentType##_AutoRegister() \
			{ \
				Zenith_ComponentMetaRegistry::Get().RegisterComponent<ComponentType>(TypeName); \
			} \
		}; \
		static ComponentType##_AutoRegister s_x##ComponentType##_AutoRegister; \
	}

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
//                     regeneration, etc. Examples: Transform::SetPosition (which
//                     calls Jolt BodyInterface), Transform::SetScale (rebuilds
//                     colliders). A raw field write would corrupt the entity by
//                     leaving runtime state pointing at the old value.
//
//   PROPERTY_CUSTOM — accepts a hand-written lambda body. Use when neither
//                     macro fits — e.g. when the deserialised value type
//                     differs from what the setter expects (Model takes a path
//                     string but the override stores a ModelHandle).
//
// Example:
//   static void RegisterProperties(Zenith_Vector<Zenith_PropertyDescriptor>& a) {
//       // Stateful: must use SetPosition so physics body syncs.
//       ZENITH_REGISTER_COMPONENT_PROPERTY_SETTER(
//           Zenith_TransformComponent, &Zenith_TransformComponent::SetPosition,
//           Zenith_Maths::Vector3, "Position", a);
//
//       // Plain data: raw write is fine.
//       ZENITH_REGISTER_COMPONENT_PROPERTY(Zenith_LightComponent, m_xColor, "Color", a);
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
