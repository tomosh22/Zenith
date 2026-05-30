#include "Zenith.h"
// Zenith_ColliderComponent.h pulls <Jolt/Jolt.h> raw (and Zenith_TransformComponent.h
// is pulled transitively through it). Disable the memory-tracking placement-new macro
// before those component headers to avoid clashing with Jolt's custom operator new,
// exactly as Zenith_ColliderComponent.cpp / Zenith_TransformComponent.cpp do.
#define ZENITH_PLACEMENT_NEW_ZONE
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TweenComponent.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_LightComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_InstancedMeshComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Zenith_ComponentMeta.h"
#include <algorithm>

// Forward declaration of the AI module's component registrar. Defined in
// AI/Components/Zenith_AIAgentComponent.cpp; declared here (not via an include)
// to avoid creating an EntityComponent -> AI module dependency.
void Zenith_AI_RegisterComponents();

//------------------------------------------------------------------------------
// Zenith_ComponentMetaRegistry Implementation
//------------------------------------------------------------------------------

Zenith_ComponentMetaRegistry& Zenith_ComponentMetaRegistry::Get()
{
	static Zenith_ComponentMetaRegistry s_xInstance;
	return s_xInstance;
}

u_int Zenith_ComponentMetaRegistry::GetSerializationOrder(const std::string& strTypeName)
{
	// Hardcoded serialization order to ensure dependencies are respected
	// Lower values serialize first (e.g., Terrain before Collider)
	// Unknown component types get a high default value
	static const std::unordered_map<std::string, u_int> s_xOrderMap = {
		{"Transform", 0},
		{"Model", 10},
		{"Tween", 12},
		{"Animator", 15},
		{"Camera", 20},
		{"Light", 25},     // Dynamic lights (point, spot, directional)
		{"Text", 30},
		{"Terrain", 40},   // Must be before Collider
		{"Collider", 50},
		{"Script", 60},
		{"UI", 70},
		// Give the components that auto-register but were absent from this map
		// explicit, distinct orders so they don't all share the 1000 default —
		// std::sort is not stable, so a shared key could order them arbitrarily
		// and make scene save-order nondeterministic. All depend only on
		// lower-ordered components (Transform/Collider), so any value past 70 is safe.
		{"InstancedMesh", 80},
		{"ParticleEmitter", 85},
		{"AIAgent", 90}
	};

	auto xIt = s_xOrderMap.find(strTypeName);
	if (xIt != s_xOrderMap.end())
	{
		return xIt->second;
	}

	// Unknown component types get a high default order (serialized last)
	return 1000;
}

const Zenith_ComponentMeta* Zenith_ComponentMetaRegistry::GetMetaByName(const std::string& strTypeName) const
{
	auto xIt = m_xMetaByName.find(strTypeName);
	if (xIt != m_xMetaByName.end())
	{
		return &xIt->second;
	}
	return nullptr;
}

bool Zenith_ComponentMetaRegistry::SetComponentProperty(
	Zenith_Entity& xEntity,
	const std::string& strComponentName,
	const std::string& strPropertyName,
	Zenith_DataStream& xValue) const
{
	const Zenith_ComponentMeta* pxMeta = GetMetaByName(strComponentName);
	if (pxMeta == nullptr)
	{
		Zenith_Warning(LOG_CATEGORY_ECS, "SetComponentProperty: unknown component type '%s'", strComponentName.c_str());
		return false;
	}
	if (pxMeta->m_pfnGetRaw == nullptr)
	{
		Zenith_Warning(LOG_CATEGORY_ECS, "SetComponentProperty: component '%s' has no GetRaw accessor (not registered correctly)", strComponentName.c_str());
		return false;
	}
	void* pxComp = pxMeta->m_pfnGetRaw(xEntity);
	if (pxComp == nullptr)
	{
		Zenith_Warning(LOG_CATEGORY_ECS, "SetComponentProperty: entity does not have component '%s'", strComponentName.c_str());
		return false;
	}
	for (u_int i = 0; i < pxMeta->m_axProperties.GetSize(); ++i)
	{
		const Zenith_PropertyDescriptor& xDesc = pxMeta->m_axProperties.Get(i);
		if (xDesc.m_strName == strPropertyName)
		{
			xValue.SetCursor(0);
			xDesc.m_pfnSetter(pxComp, xValue);
			return true;
		}
	}
	Zenith_Warning(LOG_CATEGORY_ECS,
		"SetComponentProperty: component '%s' has no registered property '%s'. (Did the component implement RegisterProperties? Are nested paths used?)",
		strComponentName.c_str(), strPropertyName.c_str());
	return false;
}

void Zenith_ComponentMetaRegistry::FinalizeRegistration()
{
	// Explicitly register every built-in component. The old auto-registrar macro
	// relied on each component's .obj being referenced, but /OPT:REF dead-strips
	// unreferenced TUs from the engine static lib, silently dropping registrations.
	// Registering explicitly here guarantees all built-ins are present regardless
	// of link-time stripping. RegisterComponent overwrites by name, so this is
	// idempotent even if some auto-registrar still happens to run.
	// Names MUST match the GetSerializationOrder map keys exactly.
	RegisterComponent<Zenith_TransformComponent>("Transform");
	RegisterComponent<Zenith_ModelComponent>("Model");
	RegisterComponent<Zenith_TweenComponent>("Tween");
	RegisterComponent<Zenith_AnimatorComponent>("Animator");
	RegisterComponent<Zenith_CameraComponent>("Camera");
	RegisterComponent<Zenith_LightComponent>("Light");
	RegisterComponent<Zenith_ColliderComponent>("Collider");
	RegisterComponent<Zenith_TerrainComponent>("Terrain");
	RegisterComponent<Zenith_InstancedMeshComponent>("InstancedMesh");
	RegisterComponent<Zenith_ParticleEmitterComponent>("ParticleEmitter");
	RegisterComponent<Zenith_ScriptComponent>("Script");
	RegisterComponent<Zenith_UIComponent>("UI");

	// AIAgent lives in the AI module; register it via the forwarder so we don't
	// pull an AI include into EntityComponent.
	Zenith_AI_RegisterComponents();

	// Build sorted list of metas
	m_xMetasSorted.clear();
	m_xMetasSorted.reserve(m_xMetaByName.size());

	for (auto& [strName, xMeta] : m_xMetaByName)
	{
		m_xMetasSorted.push_back(&xMeta);
	}

	// Sort by serialization order
	std::sort(m_xMetasSorted.begin(), m_xMetasSorted.end(),
		[](const Zenith_ComponentMeta* a, const Zenith_ComponentMeta* b)
		{
			return a->m_uSerializationOrder < b->m_uSerializationOrder;
		});

	m_bInitialized = true;

	Zenith_Log(LOG_CATEGORY_ECS, "[ComponentMetaRegistry] Finalized with %u component types:", static_cast<u_int>(m_xMetasSorted.size()));
	for (const auto* pxMeta : m_xMetasSorted)
	{
		// Surface the (inert) access-set masks alongside the serialization order
		// so the populated metadata is observable at boot. No runtime consumer
		// reads them yet — the future system scheduler will.
		Zenith_Log(LOG_CATEGORY_ECS, "  [%u] %s (reads=0x%X writes=0x%X)",
			pxMeta->m_uSerializationOrder, pxMeta->m_strTypeName.c_str(),
			pxMeta->m_uReads, pxMeta->m_uWrites);
	}
}

const std::vector<const Zenith_ComponentMeta*>& Zenith_ComponentMetaRegistry::GetAllMetasSorted() const
{
	return m_xMetasSorted;
}

void Zenith_ComponentMetaRegistry::SerializeEntityComponents(Zenith_Entity& xEntity, Zenith_DataStream& xStream) const
{
	EnsureInitialized();

	// Collect all components the entity has (in serialization order)
	std::vector<const Zenith_ComponentMeta*> xComponentsToSerialize;

	for (const Zenith_ComponentMeta* pxMeta : m_xMetasSorted)
	{
		if (pxMeta->m_pfnHasComponent && pxMeta->m_pfnHasComponent(xEntity))
		{
			xComponentsToSerialize.push_back(pxMeta);
		}
	}

	// Write component count
	u_int uNumComponents = static_cast<u_int>(xComponentsToSerialize.size());
	xStream << uNumComponents;

	// Write each component's type name and data with size prefix for forward compatibility.
	// Scene v6 per-component layout: [typeName][schemaVersion u_int][size u_int][payload].
	for (const Zenith_ComponentMeta* pxMeta : xComponentsToSerialize)
	{
		xStream << pxMeta->m_strTypeName;

		// Per-component schema version (INERT this wave — default 1 for every
		// component). Written OUTSIDE the size-prefixed payload region below, so
		// the size prefix still measures payload-only and the unknown-component
		// SkipBytes(size) path on read stays byte-aligned. The read side consumes
		// this only for scene v6+ (see DeserializeEntityComponents).
		xStream << pxMeta->m_uSchemaVersion;

		// Write size placeholder, serialize, then go back and write actual size
		uint64_t ulSizePos = xStream.GetCursor();
		u_int uPlaceholder = 0;
		xStream << uPlaceholder;

		uint64_t ulDataStart = xStream.GetCursor();
		if (pxMeta->m_pfnSerialize)
		{
			pxMeta->m_pfnSerialize(xEntity, xStream);
		}
		uint64_t ulDataEnd = xStream.GetCursor();

		// Write actual size
		u_int uActualSize = static_cast<u_int>(ulDataEnd - ulDataStart);
		xStream.SetCursor(ulSizePos);
		xStream << uActualSize;
		xStream.SetCursor(ulDataEnd);
	}
}

void Zenith_ComponentMetaRegistry::DeserializeEntityComponents(Zenith_Entity& xEntity, Zenith_DataStream& xStream, u_int uSceneVersion) const
{
	EnsureInitialized();

	// Read component count
	u_int uNumComponents;
	xStream >> uNumComponents;

	// Read each component (with size prefix for forward compatibility)
	for (u_int i = 0; i < uNumComponents; ++i)
	{
		std::string strComponentType;
		xStream >> strComponentType;

		// Scene v6+ writes a per-component schemaVersion OUTSIDE the size-prefixed
		// payload, immediately after the type name. Pre-v6 files have no such field,
		// so the guard leaves the cursor exactly where the legacy format expects the
		// size prefix next — keeping both formats byte-aligned. Default 1 means
		// "unversioned / schema 1" for legacy files.
		u_int uComponentSchemaVersion = 1u;
		if (uSceneVersion >= 6u)
		{
			xStream >> uComponentSchemaVersion;
		}

		u_int uComponentDataSize;
		xStream >> uComponentDataSize;

		const Zenith_ComponentMeta* pxMeta = GetMetaByName(strComponentType);
		if (pxMeta && pxMeta->m_pfnDeserialize)
		{
			pxMeta->m_pfnDeserialize(xEntity, xStream, uComponentSchemaVersion);
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_ECS, "[ComponentMetaRegistry] WARNING: Unknown component type '%s', skipping %u bytes", strComponentType.c_str(), uComponentDataSize);
			xStream.SkipBytes(uComponentDataSize);
		}
	}
}

//------------------------------------------------------------------------------
// Component Transfer Implementation
//------------------------------------------------------------------------------

void Zenith_ComponentMetaRegistry::TransferAllComponents(Zenith_EntityID xEntityID, Zenith_SceneData* pxSource, Zenith_SceneData* pxTarget) const
{
	EnsureInitialized();

	// Transfer each component type from source pool to target pool via move-construct.
	// Each wrapper checks the global entity-component map and skips if the entity
	// doesn't have that component type.
	for (const Zenith_ComponentMeta* pxMeta : m_xMetasSorted)
	{
		if (pxMeta->m_pfnTransferComponent)
		{
			pxMeta->m_pfnTransferComponent(xEntityID, pxSource, pxTarget);
		}
	}
}

//------------------------------------------------------------------------------
// Component Removal Implementation
//------------------------------------------------------------------------------

void Zenith_ComponentMetaRegistry::RemoveAllComponents(Zenith_Entity& xEntity) const
{
	EnsureInitialized();

	// Dispatch OnDestroy first (in reverse order - last added, first destroyed)
	for (auto xIt = m_xMetasSorted.rbegin(); xIt != m_xMetasSorted.rend(); ++xIt)
	{
		const Zenith_ComponentMeta* pxMeta = *xIt;
		if (pxMeta->m_pfnOnDestroy && pxMeta->m_pfnHasComponent && pxMeta->m_pfnHasComponent(xEntity))
		{
			pxMeta->m_pfnOnDestroy(xEntity);
		}
	}

	// Now remove all components (in reverse order)
	for (auto xIt = m_xMetasSorted.rbegin(); xIt != m_xMetasSorted.rend(); ++xIt)
	{
		const Zenith_ComponentMeta* pxMeta = *xIt;
		if (pxMeta->m_pfnRemoveComponent && pxMeta->m_pfnHasComponent && pxMeta->m_pfnHasComponent(xEntity))
		{
			pxMeta->m_pfnRemoveComponent(xEntity);
		}
	}
}

//------------------------------------------------------------------------------
// Lifecycle Hook Dispatch Implementation
//------------------------------------------------------------------------------

void Zenith_ComponentMetaRegistry::DispatchOnAwake(Zenith_Entity& xEntity) const
{
	EnsureInitialized();
	for (const Zenith_ComponentMeta* pxMeta : m_xMetasSorted)
	{
		// Same self-DestroyImmediate guard as the Update/Disable loops —
		// an OnAwake hook may free its own entity (e.g., a fail-validation
		// pattern where AddComponent's awake decides the entity is invalid
		// and tears it down). Without this check the next iteration's
		// m_pfnHasComponent lookup would assert "Entity has no scene".
		if (!xEntity.IsValid()) return;
		if (pxMeta->m_pfnOnAwake && pxMeta->m_pfnHasComponent && pxMeta->m_pfnHasComponent(xEntity))
			pxMeta->m_pfnOnAwake(xEntity);
	}
}

void Zenith_ComponentMetaRegistry::DispatchOnStart(Zenith_Entity& xEntity) const
{
	EnsureInitialized();
	for (const Zenith_ComponentMeta* pxMeta : m_xMetasSorted)
	{
		// Same self-DestroyImmediate guard as the other Dispatch* loops —
		// OnStart is a particularly common place for "check world state and
		// suicide if invalid" patterns, so the guard is doubly load-bearing
		// here.
		if (!xEntity.IsValid()) return;
		if (pxMeta->m_pfnOnStart && pxMeta->m_pfnHasComponent && pxMeta->m_pfnHasComponent(xEntity))
			pxMeta->m_pfnOnStart(xEntity);
	}
}

void Zenith_ComponentMetaRegistry::DispatchOnEnable(Zenith_Entity& xEntity) const
{
	EnsureInitialized();
	for (const Zenith_ComponentMeta* pxMeta : m_xMetasSorted)
	{
		// Same self-DestroyImmediate guard as the other Dispatch* loops —
		// an OnEnable hook may legitimately free its own entity (e.g., a
		// pooling system reclaiming the slot the moment the component
		// activates), and the next iteration's m_pfnHasComponent lookup
		// would assert "Entity has no scene" without this check.
		if (!xEntity.IsValid()) return;
		if (pxMeta->m_pfnOnEnable && pxMeta->m_pfnHasComponent && pxMeta->m_pfnHasComponent(xEntity))
			pxMeta->m_pfnOnEnable(xEntity);
	}
}

void Zenith_ComponentMetaRegistry::DispatchOnDisable(Zenith_Entity& xEntity) const
{
	EnsureInitialized();
	for (const Zenith_ComponentMeta* pxMeta : m_xMetasSorted)
	{
		// Same self-DestroyImmediate guard as the other Dispatch* loops —
		// an OnDisable hook may legitimately free its own entity (e.g., a
		// pooling system reclaiming the slot the moment the component
		// deactivates), and the next iteration's m_pfnHasComponent lookup
		// would assert "Entity has no scene" without this check.
		if (!xEntity.IsValid()) return;
		if (pxMeta->m_pfnOnDisable && pxMeta->m_pfnHasComponent && pxMeta->m_pfnHasComponent(xEntity))
			pxMeta->m_pfnOnDisable(xEntity);
	}
}

// A user-side hook (typically ScriptComponent::OnUpdate dispatching to a
// Zenith_ScriptBehaviour) is allowed to call xEntity.DestroyImmediate() on
// itself. Once that happens the entity slot is freed, m_pfnHasComponent's
// subsequent lookup asserts ("Entity has no scene"), and the loop crashes
// before its remaining iterations would have had nothing to dispatch
// anyway. Probe IsValid() between iterations and return — the regression
// is covered by Scene::DestroyImmediateDuringSelfOnUpdate.
void Zenith_ComponentMetaRegistry::DispatchOnUpdate(Zenith_Entity& xEntity, float fDt) const
{
	EnsureInitialized();
	for (const Zenith_ComponentMeta* pxMeta : m_xMetasSorted)
	{
		if (!xEntity.IsValid()) return;
		if (pxMeta->m_pfnOnUpdate && pxMeta->m_pfnHasComponent && pxMeta->m_pfnHasComponent(xEntity))
			pxMeta->m_pfnOnUpdate(xEntity, fDt);
	}
}

void Zenith_ComponentMetaRegistry::DispatchOnLateUpdate(Zenith_Entity& xEntity, float fDt) const
{
	EnsureInitialized();
	for (const Zenith_ComponentMeta* pxMeta : m_xMetasSorted)
	{
		if (!xEntity.IsValid()) return;
		if (pxMeta->m_pfnOnLateUpdate && pxMeta->m_pfnHasComponent && pxMeta->m_pfnHasComponent(xEntity))
			pxMeta->m_pfnOnLateUpdate(xEntity, fDt);
	}
}

void Zenith_ComponentMetaRegistry::DispatchOnFixedUpdate(Zenith_Entity& xEntity, float fDt) const
{
	EnsureInitialized();
	for (const Zenith_ComponentMeta* pxMeta : m_xMetasSorted)
	{
		if (!xEntity.IsValid()) return;
		if (pxMeta->m_pfnOnFixedUpdate && pxMeta->m_pfnHasComponent && pxMeta->m_pfnHasComponent(xEntity))
			pxMeta->m_pfnOnFixedUpdate(xEntity, fDt);
	}
}

void Zenith_ComponentMetaRegistry::DispatchOnDestroy(Zenith_Entity& xEntity) const
{
	EnsureInitialized();
	// Dispatch in reverse order for destruction (last added, first destroyed)
	for (auto xIt = m_xMetasSorted.rbegin(); xIt != m_xMetasSorted.rend(); ++xIt)
	{
		const Zenith_ComponentMeta* pxMeta = *xIt;
		if (pxMeta->m_pfnOnDestroy && pxMeta->m_pfnHasComponent && pxMeta->m_pfnHasComponent(xEntity))
			pxMeta->m_pfnOnDestroy(xEntity);
	}
}
