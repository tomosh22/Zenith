#include "Zenith.h"
// ECS leaf-extraction Phase 4: this TU is the ECS reflection CORE machinery. It
// names no concrete component type, no editor component-registry, and no AI
// symbol. The 12 built-in component includes, the ZENITH_PLACEMENT_NEW_ZONE
// define they required (to dodge Jolt's operator new), the explicit built-in
// registration calls, and the AI registrar forwarder all moved engine-side to
// Zenith_ComponentMeta_Registration.cpp.
#include "ZenithECS/Zenith_ComponentMeta.h"
#include <algorithm>

//------------------------------------------------------------------------------
// Zenith_ComponentMetaRegistry Implementation
//------------------------------------------------------------------------------

Zenith_ComponentMetaRegistry& Zenith_ComponentMetaRegistry::Get()
{
	static Zenith_ComponentMetaRegistry s_xInstance;
	return s_xInstance;
}

Zenith_Vector<void (*)()>& Zenith_ComponentMetaRegistry::PendingRegistrarThunks()
{
	// Function-static backing list for the ZENITH_REGISTER_COMPONENT macro. Lives
	// here (not as a member) so a static initializer can enqueue before the
	// singleton -- or main -- is constructed, side-stepping the static-init-order
	// fiasco. Drained once by EnsureInitialized.
	static Zenith_Vector<void (*)()> s_xThunks;
	return s_xThunks;
}

void Zenith_ComponentMetaRegistry::EnqueueRegistrarThunk(void (*pfn)())
{
	if (pfn != nullptr)
	{
		PendingRegistrarThunks().PushBack(pfn);
	}
}

void Zenith_ComponentMetaRegistry::SetComponentRegistrar(void (*pfn)())
{
	m_pfnRegisterComponents = pfn;
}

const Zenith_ComponentMeta* Zenith_ComponentMetaRegistry::GetMetaByName(const std::string& strTypeName) const
{
	const Zenith_ComponentMeta* pxMeta = m_xMetaByName.TryGet(strTypeName);
	if (pxMeta != nullptr)
	{
		return pxMeta;
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

void Zenith_ComponentMetaRegistry::Finalize()
{
	// Machinery only: sort whatever was registered and seal the registry. The
	// concrete built-ins are registered by the engine-installed registrar (run
	// from EnsureInitialized before this), and any game/external components by
	// their macro thunks -- this TU names none of them.

	// Build sorted list of metas
	m_xMetasSorted.Clear();
	m_xMetasSorted.Reserve(m_xMetaByName.GetSize());

	for (Zenith_HashMap<std::string, Zenith_ComponentMeta>::Iterator xIt(m_xMetaByName); !xIt.Done(); xIt.Next())
	{
		m_xMetasSorted.PushBack(&xIt.GetValueMutable());
	}

	// Sort by serialization order
	std::sort(m_xMetasSorted.begin(), m_xMetasSorted.end(),
		[](const Zenith_ComponentMeta* a, const Zenith_ComponentMeta* b)
		{
			return a->m_uSerializationOrder < b->m_uSerializationOrder;
		});

	m_bInitialized = true;

	Zenith_Log(LOG_CATEGORY_ECS, "[ComponentMetaRegistry] Finalized with %u component types:", m_xMetasSorted.GetSize());
	for (const auto* pxMeta : m_xMetasSorted)
	{
		Zenith_Log(LOG_CATEGORY_ECS, "  [%u] %s",
			pxMeta->m_uSerializationOrder, pxMeta->m_strTypeName.c_str());
	}
}

const Zenith_Vector<const Zenith_ComponentMeta*>& Zenith_ComponentMetaRegistry::GetAllMetasSorted() const
{
	return m_xMetasSorted;
}

void Zenith_ComponentMetaRegistry::SerializeEntityComponents(Zenith_Entity& xEntity, Zenith_DataStream& xStream) const
{
	EnsureInitialized();

	// Collect all components the entity has (in serialization order)
	Zenith_Vector<const Zenith_ComponentMeta*> xComponentsToSerialize;

	for (const Zenith_ComponentMeta* pxMeta : m_xMetasSorted)
	{
		if (pxMeta->m_pfnHasComponent && pxMeta->m_pfnHasComponent(xEntity))
		{
			xComponentsToSerialize.PushBack(pxMeta);
		}
	}

	// Write component count
	u_int uNumComponents = xComponentsToSerialize.GetSize();
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

		// Bounded deserialize (every component, known AND unknown): record the cursor
		// at the start of the size-prefixed payload, run the (version-aware) deserializer,
		// then FORCE the cursor to (start + declared size) regardless of how many bytes the
		// deserializer actually consumed. This is the byte-alignment backstop that lets a
		// reader read FEWER bytes than were written (e.g. a scene v7 owning-component reader that
		// no longer reads the legacy parent index a v6 component payload carries) -- or more,
		// up to the declared size -- without desyncing the stream. For UNKNOWN components the
		// deserializer is absent, so the cursor-force degenerates into the old
		// SkipBytes(size) skip-past-the-blob behaviour.
		const uint64_t ulDataStart = xStream.GetCursor();

		const Zenith_ComponentMeta* pxMeta = GetMetaByName(strComponentType);
		if (pxMeta && pxMeta->m_pfnDeserialize)
		{
			pxMeta->m_pfnDeserialize(xEntity, xStream, uComponentSchemaVersion);
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_ECS, "[ComponentMetaRegistry] WARNING: Unknown component type '%s', skipping %u bytes", strComponentType.c_str(), uComponentDataSize);
		}

		// Realign to the declared payload boundary (start + size). For a well-formed
		// known component this is a no-op (the deserializer consumed exactly size bytes);
		// for an unknown component it skips the whole blob; for a shrunk/grown known
		// payload it absorbs the difference. SetCursor clamps + asserts on a corrupt
		// (out-of-range) size, exactly as the former SkipBytes(size) did.
		xStream.SetCursor(ulDataStart + uComponentDataSize);
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
	for (u_int u = m_xMetasSorted.GetSize(); u-- > 0; )
	{
		const Zenith_ComponentMeta* pxMeta = m_xMetasSorted.Get(u);
		if (pxMeta->m_pfnOnDestroy && pxMeta->m_pfnHasComponent && pxMeta->m_pfnHasComponent(xEntity))
		{
			pxMeta->m_pfnOnDestroy(xEntity);
		}
	}

	// Now remove all components (in reverse order)
	for (u_int u = m_xMetasSorted.GetSize(); u-- > 0; )
	{
		const Zenith_ComponentMeta* pxMeta = m_xMetasSorted.Get(u);
		if (pxMeta->m_pfnRemoveComponent && pxMeta->m_pfnHasComponent && pxMeta->m_pfnHasComponent(xEntity))
		{
			pxMeta->m_pfnRemoveComponent(xEntity);
		}
	}
}

//------------------------------------------------------------------------------
// Lifecycle Hook Dispatch Implementation
//------------------------------------------------------------------------------

namespace
{
	// Shared body for the FORWARD-iterating lifecycle dispatchers (OnAwake / OnStart
	// / OnEnable / OnDisable / OnUpdate / OnLateUpdate / OnFixedUpdate). They differ
	// only by which m_pfnOnX hook is read and whether a float dt is forwarded, so the
	// loop lives here exactly once (pfnHook is a pointer-to-member naming the field).
	//
	// The IsValid() probe between iterations is load-bearing: a user-side hook
	// (typically a component OnUpdate hook dispatching into game logic)
	// may call xEntity.DestroyImmediate() on itself. Once the slot is freed, the next
	// iteration's m_pfnHasComponent lookup would assert "Entity has no scene". The
	// regression is covered by Scene::DestroyImmediateDuringSelfOnUpdate.
	//
	// DispatchOnDestroy is intentionally NOT routed through here — it iterates in
	// REVERSE (last-added, first-destroyed) and has no IsValid guard.
	template<typename HookFn, typename... Args>
	void DispatchLifecycleHook(
		const Zenith_Vector<const Zenith_ComponentMeta*>& xMetasSorted,
		Zenith_Entity& xEntity,
		HookFn Zenith_ComponentMeta::* pfnHook,
		Args... xArgs)
	{
		for (const Zenith_ComponentMeta* pxMeta : xMetasSorted)
		{
			if (!xEntity.IsValid()) return;
			if (pxMeta->*pfnHook && pxMeta->m_pfnHasComponent && pxMeta->m_pfnHasComponent(xEntity))
				(pxMeta->*pfnHook)(xEntity, xArgs...);
		}
	}
}

void Zenith_ComponentMetaRegistry::DispatchOnAwake(Zenith_Entity& xEntity) const
{
	EnsureInitialized();
	DispatchLifecycleHook(m_xMetasSorted, xEntity, &Zenith_ComponentMeta::m_pfnOnAwake);
}

void Zenith_ComponentMetaRegistry::DispatchOnStart(Zenith_Entity& xEntity) const
{
	EnsureInitialized();
	DispatchLifecycleHook(m_xMetasSorted, xEntity, &Zenith_ComponentMeta::m_pfnOnStart);
}

void Zenith_ComponentMetaRegistry::DispatchOnEnable(Zenith_Entity& xEntity) const
{
	EnsureInitialized();
	DispatchLifecycleHook(m_xMetasSorted, xEntity, &Zenith_ComponentMeta::m_pfnOnEnable);
}

void Zenith_ComponentMetaRegistry::DispatchOnDisable(Zenith_Entity& xEntity) const
{
	EnsureInitialized();
	DispatchLifecycleHook(m_xMetasSorted, xEntity, &Zenith_ComponentMeta::m_pfnOnDisable);
}

void Zenith_ComponentMetaRegistry::DispatchOnUpdate(Zenith_Entity& xEntity, float fDt) const
{
	EnsureInitialized();
	DispatchLifecycleHook(m_xMetasSorted, xEntity, &Zenith_ComponentMeta::m_pfnOnUpdate, fDt);
}

void Zenith_ComponentMetaRegistry::DispatchOnLateUpdate(Zenith_Entity& xEntity, float fDt) const
{
	EnsureInitialized();
	DispatchLifecycleHook(m_xMetasSorted, xEntity, &Zenith_ComponentMeta::m_pfnOnLateUpdate, fDt);
}

void Zenith_ComponentMetaRegistry::DispatchOnFixedUpdate(Zenith_Entity& xEntity, float fDt) const
{
	EnsureInitialized();
	DispatchLifecycleHook(m_xMetasSorted, xEntity, &Zenith_ComponentMeta::m_pfnOnFixedUpdate, fDt);
}

void Zenith_ComponentMetaRegistry::DispatchOnCollisionEnter(Zenith_Entity& xEntity, Zenith_Entity& xOther) const
{
	EnsureInitialized();
	DispatchLifecycleHook(m_xMetasSorted, xEntity, &Zenith_ComponentMeta::m_pfnOnCollisionEnter, xOther);
}

void Zenith_ComponentMetaRegistry::DispatchOnCollisionStay(Zenith_Entity& xEntity, Zenith_Entity& xOther) const
{
	EnsureInitialized();
	DispatchLifecycleHook(m_xMetasSorted, xEntity, &Zenith_ComponentMeta::m_pfnOnCollisionStay, xOther);
}

void Zenith_ComponentMetaRegistry::DispatchOnCollisionExit(Zenith_Entity& xEntity, Zenith_EntityID xOtherID) const
{
	EnsureInitialized();
	DispatchLifecycleHook(m_xMetasSorted, xEntity, &Zenith_ComponentMeta::m_pfnOnCollisionExit, xOtherID);
}

void Zenith_ComponentMetaRegistry::DispatchOnDestroy(Zenith_Entity& xEntity) const
{
	EnsureInitialized();
	// Dispatch in reverse order for destruction (last added, first destroyed)
	for (u_int u = m_xMetasSorted.GetSize(); u-- > 0; )
	{
		const Zenith_ComponentMeta* pxMeta = m_xMetasSorted.Get(u);
		if (pxMeta->m_pfnOnDestroy && pxMeta->m_pfnHasComponent && pxMeta->m_pfnHasComponent(xEntity))
			pxMeta->m_pfnOnDestroy(xEntity);
	}
}
