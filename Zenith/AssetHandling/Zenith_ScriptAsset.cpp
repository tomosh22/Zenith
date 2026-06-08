#include "Zenith.h"
#include "AssetHandling/Zenith_ScriptAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include <filesystem>
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

ZENITH_REGISTER_ASSET_TYPE(Zenith_ScriptAsset)

Zenith_ScriptAsset::Zenith_ScriptAsset(const char* szBehaviourTypeName, Zenith_ScriptBehaviourFactoryFn pfnFactory)
	: m_strBehaviourTypeName(szBehaviourTypeName ? szBehaviourTypeName : "")
	, m_pfnFactory(pfnFactory)
{
}

Zenith_ScriptBehaviour* Zenith_ScriptAsset::CreateInstance(Zenith_Entity& xEntity) const
{
	if (!m_pfnFactory)
	{
		Zenith_Log(LOG_CATEGORY_ECS,
			"Zenith_ScriptAsset: Cannot create instance, no factory registered for behaviour '%s'",
			m_strBehaviourTypeName.c_str());
		return nullptr;
	}
	return m_pfnFactory(xEntity);
}

void Zenith_ScriptAsset::WriteToDataStream(Zenith_DataStream& xStream) const
{
	const uint32_t uVersion = 1;
	xStream << uVersion;
	xStream << m_strBehaviourTypeName;
}

void Zenith_ScriptAsset::ReadFromDataStream(Zenith_DataStream& xStream)
{
	uint32_t uVersion = 0;
	xStream >> uVersion;
	Zenith_Assert(uVersion == 1, "Zenith_ScriptAsset: unsupported version %u", uVersion);
	xStream >> m_strBehaviourTypeName;

	// Bind factory if a matching C++ behaviour is registered.
	// If not (orphan asset), m_pfnFactory stays nullptr and IsRegistered() returns false.
	m_pfnFactory = LookupFactory(m_strBehaviourTypeName.c_str());
}

#ifdef ZENITH_TOOLS
void Zenith_ScriptAsset::RenderPropertiesPanel()
{
	ImGui::Text("Behaviour: %s", m_strBehaviourTypeName.c_str());
	if (IsRegistered())
	{
		ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Registered");
	}
	else
	{
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
			"NOT registered - no matching C++ class compiled in");
	}
}
#endif

//------------------------------------------------------------------------------
// Static factory registry (folded in from the deleted Zenith_BehaviourRegistry)
//------------------------------------------------------------------------------

Zenith_HashMap<std::string, Zenith_ScriptBehaviourFactoryFn>& Zenith_ScriptAsset::GetFactoryMap()
{
	// Construct-on-first-use - safe under static initialization order
	static Zenith_HashMap<std::string, Zenith_ScriptBehaviourFactoryFn> s_xFactoryMap;
	return s_xFactoryMap;
}

Zenith_HashMap<std::string, bool>& Zenith_ScriptAsset::GetInternalNameSet()
{
	// Used as a set: keys are internal behaviour names, the bool value is an unused dummy.
	static Zenith_HashMap<std::string, bool> s_xInternalNames;
	return s_xInternalNames;
}

void Zenith_ScriptAsset::RegisterFactory(const char* szTypeName, Zenith_ScriptBehaviourFactoryFn pfnFactory)
{
	if (!szTypeName || !pfnFactory)
	{
		return;
	}
	GetFactoryMap()[szTypeName] = pfnFactory;
}

void Zenith_ScriptAsset::RegisterFactoryInternal(const char* szTypeName, Zenith_ScriptBehaviourFactoryFn pfnFactory)
{
	if (!szTypeName || !pfnFactory)
	{
		return;
	}
	GetFactoryMap()[szTypeName] = pfnFactory;
	GetInternalNameSet().Insert(szTypeName, true);
}

bool Zenith_ScriptAsset::IsInternalFactory(const char* szTypeName)
{
	if (!szTypeName)
	{
		return false;
	}
	const auto& xSet = GetInternalNameSet();
	return xSet.Contains(szTypeName);
}

void Zenith_ScriptAsset::GetPublicRegisteredTypeNames(Zenith_Vector<std::string>& xOut)
{
	xOut.Clear();
	const auto& xMap = GetFactoryMap();
	const auto& xInternal = GetInternalNameSet();
	for (Zenith_HashMap<std::string, Zenith_ScriptBehaviourFactoryFn>::Iterator xIt(xMap); !xIt.Done(); xIt.Next())
	{
		if (!xInternal.Contains(xIt.GetKey()))
		{
			xOut.PushBack(xIt.GetKey());
		}
	}
}

Zenith_ScriptBehaviourFactoryFn Zenith_ScriptAsset::LookupFactory(const char* szTypeName)
{
	if (!szTypeName)
	{
		return nullptr;
	}
	const auto& xMap = GetFactoryMap();
	const Zenith_ScriptBehaviourFactoryFn* pxFactory = xMap.TryGet(szTypeName);
	return pxFactory ? *pxFactory : nullptr;
}

bool Zenith_ScriptAsset::HasFactory(const char* szTypeName)
{
	return LookupFactory(szTypeName) != nullptr;
}

void Zenith_ScriptAsset::GetAllRegisteredTypeNames(Zenith_Vector<std::string>& xOut)
{
	xOut.Clear();
	const auto& xMap = GetFactoryMap();
	for (Zenith_HashMap<std::string, Zenith_ScriptBehaviourFactoryFn>::Iterator xIt(xMap); !xIt.Done(); xIt.Next())
	{
		xOut.PushBack(xIt.GetKey());
	}
}

std::string Zenith_ScriptAsset::MakeAssetPath(const char* szTypeName)
{
	std::string strPath = "game:Scripts/";
	if (szTypeName)
	{
		strPath += szTypeName;
	}
	strPath += ZENITH_SCRIPT_EXT;
	return strPath;
}

#ifdef ZENITH_TOOLS
void Zenith_ScriptAsset::SyncRegisteredTypesToDisk()
{
	namespace fs = std::filesystem;

	// Resolve game:Scripts/ to an absolute directory
	const std::string strScriptsPrefixed = "game:Scripts";
	const std::string strScriptsAbsolute = Zenith_AssetRegistry::ResolvePath(strScriptsPrefixed);

	const auto& xFactoryMap = GetFactoryMap();
	const auto& xInternalNames = GetInternalNameSet();
	Zenith_Log(LOG_CATEGORY_ASSET,
		"Zenith_ScriptAsset::SyncRegisteredTypesToDisk: %u registered behaviours (%u internal/excluded), target dir '%s'",
		xFactoryMap.GetSize(), xInternalNames.GetSize(), strScriptsAbsolute.c_str());

	std::error_code xEC;
	fs::create_directories(strScriptsAbsolute, xEC);
	if (xEC)
	{
		Zenith_Log(LOG_CATEGORY_ASSET,
			"Zenith_ScriptAsset::SyncRegisteredTypesToDisk: Failed to create directory '%s': %s",
			strScriptsAbsolute.c_str(), xEC.message().c_str());
		return;
	}

	// Pass 1: write missing .zscript files for each registered behaviour
	for (Zenith_HashMap<std::string, Zenith_ScriptBehaviourFactoryFn>::Iterator xIt(xFactoryMap); !xIt.Done(); xIt.Next())
	{
		const std::string& strTypeName = xIt.GetKey();

		// Internal behaviours (test fixtures, runtime-only helpers) are excluded from the
		// on-disk asset surface: they're useful at runtime via LookupFactory but should not
		// pollute game:Scripts/ or appear in the editor's content browser. See P2.4.
		if (xInternalNames.Contains(strTypeName))
		{
			continue;
		}

		const std::string strAssetPath = MakeAssetPath(strTypeName.c_str());
		const std::string strAbsolutePath = Zenith_AssetRegistry::ResolvePath(strAssetPath);

		if (fs::exists(strAbsolutePath, xEC))
		{
			continue;
		}

		// Construct the asset and save it via the registry's standard pipeline
		Zenith_ScriptAsset xAsset(strTypeName.c_str(), xIt.GetValue());
		if (!Zenith_AssetRegistry::Save(&xAsset, strAssetPath))
		{
			Zenith_Log(LOG_CATEGORY_ASSET,
				"Zenith_ScriptAsset::SyncRegisteredTypesToDisk: Failed to save '%s'",
				strAssetPath.c_str());
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_ASSET,
				"Zenith_ScriptAsset::SyncRegisteredTypesToDisk: Wrote '%s'",
				strAssetPath.c_str());
		}
	}

	// Pass 2: warn about orphan .zscript files (no automatic rename or delete).
	// A .zscript whose behaviour name is not in the current factory map could be:
	//   - genuinely stale (the C++ class was deleted or renamed), OR
	//   - valid but compiled out by a build flag, platform define, or test/runtime split.
	// Auto-renaming would clobber the second case as a side effect of launching a tools
	// build. Pruning is therefore an explicit editor action (not yet wired up); this pass
	// only logs warnings so the developer can investigate.
	for (const auto& xEntry : fs::directory_iterator(strScriptsAbsolute, xEC))
	{
		if (xEC || !xEntry.is_regular_file())
		{
			continue;
		}
		const fs::path& xPath = xEntry.path();
		if (xPath.extension() != ZENITH_SCRIPT_EXT)
		{
			continue;
		}

		const std::string strStem = xPath.stem().string();
		if (xFactoryMap.Contains(strStem))
		{
			continue;  // Has a registered factory - keep it
		}

		Zenith_Log(LOG_CATEGORY_ASSET,
			"Zenith_ScriptAsset::SyncRegisteredTypesToDisk: orphan '%s' has no registered behaviour in this build (left in place; remove via editor if intentional)",
			xPath.string().c_str());
	}
}
#endif
