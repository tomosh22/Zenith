#include "Zenith.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"

Zenith_GraphNodeRegistry& Zenith_GraphNodeRegistry::Get()
{
	// Construct-on-first-use - safe under static init order, same shape as
	// Zenith_ComponentMetaRegistry::Get().
	static Zenith_GraphNodeRegistry ls_xRegistry;
	return ls_xRegistry;
}

void Zenith_GraphNodeRegistry::Register(const Zenith_GraphNodeTypeInfo& xInfo)
{
	Zenith_Assert(!xInfo.m_strTypeName.empty(), "GraphNodeRegistry: empty type name");
	Zenith_Assert(xInfo.m_pfnCreate != nullptr, "GraphNodeRegistry: '%s' has no create function", xInfo.m_strTypeName.c_str());

	if (Find(xInfo.m_strTypeName.c_str()) != nullptr)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "GraphNodeRegistry: duplicate node type '%s' ignored", xInfo.m_strTypeName.c_str());
		return;
	}
	m_axTypes.PushBack(xInfo);
}

const Zenith_GraphNodeTypeInfo* Zenith_GraphNodeRegistry::Find(const char* szTypeName) const
{
	if (!szTypeName)
	{
		return nullptr;
	}
	for (u_int u = 0; u < m_axTypes.GetSize(); ++u)
	{
		if (m_axTypes.Get(u).m_strTypeName == szTypeName)
		{
			return &m_axTypes.Get(u);
		}
	}
	return nullptr;
}

u_int Zenith_GraphNodeRegistry::GetTypeCount() const
{
	return m_axTypes.GetSize();
}

const Zenith_GraphNodeTypeInfo& Zenith_GraphNodeRegistry::GetTypeAt(u_int uIndex) const
{
	Zenith_Assert(uIndex < m_axTypes.GetSize(), "GraphNodeRegistry: index %u out of range", uIndex);
	return m_axTypes.Get(uIndex);
}

void Zenith_GraphNodeRegistry::SetNodeRegistrar(void (*pfnRegistrar)())
{
	m_pfnRegistrar = pfnRegistrar;
}

void Zenith_GraphNodeRegistry::EnsureInitialized()
{
	if (m_bInitialized)
	{
		return;
	}
	m_bInitialized = true;	// set first - registrar bodies may query the registry
	if (m_pfnRegistrar)
	{
		m_pfnRegistrar();
	}
}

void Zenith_GraphNodeRegistry::ResetForTests()
{
	m_axTypes.Clear();
	m_bInitialized = false;
}
