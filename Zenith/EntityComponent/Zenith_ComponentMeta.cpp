#include "Zenith.h"
#include "Zenith_ComponentMeta.h"
#include "Zenith_Entity.h"
#include "DataStream/Zenith_DataStream.h"
#include <algorithm>

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
		{"Camera", 20},
		{"Text", 30},
		{"Terrain", 40},   // Must be before Collider
		{"Collider", 50},
		{"Script", 60},
		{"UI", 70}
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

void Zenith_ComponentMetaRegistry::FinalizeRegistration()
{
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
		Zenith_Log(LOG_CATEGORY_ECS, "  [%u] %s", pxMeta->m_uSerializationOrder, pxMeta->m_strTypeName.c_str());
	}
}

const std::vector<const Zenith_ComponentMeta*>& Zenith_ComponentMetaRegistry::GetAllMetasSorted() const
{
	return m_xMetasSorted;
}

void Zenith_ComponentMetaRegistry::SerializeEntityComponents(Zenith_Entity& xEntity, Zenith_DataStream& xStream) const
{
	// Auto-finalize on first use if not already done
	if (!m_bInitialized)
	{
		const_cast<Zenith_ComponentMetaRegistry*>(this)->FinalizeRegistration();
	}

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

	// Write each component's type name and data
	for (const Zenith_ComponentMeta* pxMeta : xComponentsToSerialize)
	{
		xStream << pxMeta->m_strTypeName;

		if (pxMeta->m_pfnSerialize)
		{
			pxMeta->m_pfnSerialize(xEntity, xStream);
		}
	}
}

void Zenith_ComponentMetaRegistry::DeserializeEntityComponents(Zenith_Entity& xEntity, Zenith_DataStream& xStream) const
{
	// Auto-finalize on first use if not already done
	if (!m_bInitialized)
	{
		const_cast<Zenith_ComponentMetaRegistry*>(this)->FinalizeRegistration();
	}

	// Read component count
	u_int uNumComponents;
	xStream >> uNumComponents;

	// Read each component
	for (u_int i = 0; i < uNumComponents; ++i)
	{
		std::string strComponentType;
		xStream >> strComponentType;

		const Zenith_ComponentMeta* pxMeta = GetMetaByName(strComponentType);
		if (pxMeta && pxMeta->m_pfnDeserialize)
		{
			pxMeta->m_pfnDeserialize(xEntity, xStream);
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_ECS, "[ComponentMetaRegistry] WARNING: Unknown component type '%s' during deserialization", strComponentType.c_str());
			// Cannot skip unknown component data - this will corrupt the stream
			// In a full implementation, we would store component data size to allow skipping
		}
	}
}

//------------------------------------------------------------------------------
// Component Removal Implementation
//------------------------------------------------------------------------------

void Zenith_ComponentMetaRegistry::RemoveAllComponents(Zenith_Entity& xEntity) const
{
	if (!m_bInitialized)
	{
		const_cast<Zenith_ComponentMetaRegistry*>(this)->FinalizeRegistration();
	}

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
	if (!m_bInitialized)
	{
		const_cast<Zenith_ComponentMetaRegistry*>(this)->FinalizeRegistration();
	}

	for (const Zenith_ComponentMeta* pxMeta : m_xMetasSorted)
	{
		if (pxMeta->m_pfnOnAwake && pxMeta->m_pfnHasComponent && pxMeta->m_pfnHasComponent(xEntity))
		{
			pxMeta->m_pfnOnAwake(xEntity);
		}
	}
}

void Zenith_ComponentMetaRegistry::DispatchOnStart(Zenith_Entity& xEntity) const
{
	if (!m_bInitialized)
	{
		const_cast<Zenith_ComponentMetaRegistry*>(this)->FinalizeRegistration();
	}

	for (const Zenith_ComponentMeta* pxMeta : m_xMetasSorted)
	{
		if (pxMeta->m_pfnOnStart && pxMeta->m_pfnHasComponent && pxMeta->m_pfnHasComponent(xEntity))
		{
			pxMeta->m_pfnOnStart(xEntity);
		}
	}
}

void Zenith_ComponentMetaRegistry::DispatchOnEnable(Zenith_Entity& xEntity) const
{
	if (!m_bInitialized)
	{
		const_cast<Zenith_ComponentMetaRegistry*>(this)->FinalizeRegistration();
	}

	for (const Zenith_ComponentMeta* pxMeta : m_xMetasSorted)
	{
		if (pxMeta->m_pfnOnEnable && pxMeta->m_pfnHasComponent && pxMeta->m_pfnHasComponent(xEntity))
		{
			pxMeta->m_pfnOnEnable(xEntity);
		}
	}
}

void Zenith_ComponentMetaRegistry::DispatchOnDisable(Zenith_Entity& xEntity) const
{
	if (!m_bInitialized)
	{
		const_cast<Zenith_ComponentMetaRegistry*>(this)->FinalizeRegistration();
	}

	for (const Zenith_ComponentMeta* pxMeta : m_xMetasSorted)
	{
		if (pxMeta->m_pfnOnDisable && pxMeta->m_pfnHasComponent && pxMeta->m_pfnHasComponent(xEntity))
		{
			pxMeta->m_pfnOnDisable(xEntity);
		}
	}
}

void Zenith_ComponentMetaRegistry::DispatchOnUpdate(Zenith_Entity& xEntity, float fDt) const
{
	if (!m_bInitialized)
	{
		const_cast<Zenith_ComponentMetaRegistry*>(this)->FinalizeRegistration();
	}

	for (const Zenith_ComponentMeta* pxMeta : m_xMetasSorted)
	{
		if (pxMeta->m_pfnOnUpdate && pxMeta->m_pfnHasComponent && pxMeta->m_pfnHasComponent(xEntity))
		{
			pxMeta->m_pfnOnUpdate(xEntity, fDt);
		}
	}
}

void Zenith_ComponentMetaRegistry::DispatchOnLateUpdate(Zenith_Entity& xEntity, float fDt) const
{
	if (!m_bInitialized)
	{
		const_cast<Zenith_ComponentMetaRegistry*>(this)->FinalizeRegistration();
	}

	for (const Zenith_ComponentMeta* pxMeta : m_xMetasSorted)
	{
		if (pxMeta->m_pfnOnLateUpdate && pxMeta->m_pfnHasComponent && pxMeta->m_pfnHasComponent(xEntity))
		{
			pxMeta->m_pfnOnLateUpdate(xEntity, fDt);
		}
	}
}

void Zenith_ComponentMetaRegistry::DispatchOnFixedUpdate(Zenith_Entity& xEntity, float fDt) const
{
	if (!m_bInitialized)
	{
		const_cast<Zenith_ComponentMetaRegistry*>(this)->FinalizeRegistration();
	}

	for (const Zenith_ComponentMeta* pxMeta : m_xMetasSorted)
	{
		if (pxMeta->m_pfnOnFixedUpdate && pxMeta->m_pfnHasComponent && pxMeta->m_pfnHasComponent(xEntity))
		{
			pxMeta->m_pfnOnFixedUpdate(xEntity, fDt);
		}
	}
}

void Zenith_ComponentMetaRegistry::DispatchOnDestroy(Zenith_Entity& xEntity) const
{
	if (!m_bInitialized)
	{
		const_cast<Zenith_ComponentMetaRegistry*>(this)->FinalizeRegistration();
	}

	// Dispatch in reverse order for destruction (last added, first destroyed)
	for (auto xIt = m_xMetasSorted.rbegin(); xIt != m_xMetasSorted.rend(); ++xIt)
	{
		const Zenith_ComponentMeta* pxMeta = *xIt;
		if (pxMeta->m_pfnOnDestroy && pxMeta->m_pfnHasComponent && pxMeta->m_pfnHasComponent(xEntity))
		{
			pxMeta->m_pfnOnDestroy(xEntity);
		}
	}
}
