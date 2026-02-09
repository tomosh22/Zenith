#pragma once

#ifdef ZENITH_TOOLS

#include "Zenith_SceneData.h"
#include "Zenith_Entity.h"
#include <concepts>
#include <string>
#include <vector>

// Forward declarations
class Zenith_Entity;

//==============================================================================
// Zenith_Component Concept
//==============================================================================
// This C++20 concept defines the requirements for all component types that can
// be managed by the editor's "Add Component" functionality.
//
// Requirements:
// 1. Component must be constructible with a Zenith_Entity& parameter
// 2. Component type must work with Zenith_SceneData::TypeIDGenerator
// 3. Component must have a RenderPropertiesPanel method for editor UI
//
// Note: We don't require default constructibility because all Zenith components
// require a parent entity reference in their constructor.
//==============================================================================





//==============================================================================
// Component Registration Entry
//==============================================================================
// Each registered component type has an entry containing:
// - Display name for the editor UI
// - Type ID from TypeIDGenerator
// - Factory function to add component to an entity
// - Check function to see if entity already has this component
//==============================================================================

struct Zenith_ComponentRegistryEntry
{
	// Human-readable name shown in editor UI
	std::string m_strDisplayName;
	
	// Unique type ID from Zenith_SceneData::TypeIDGenerator
	Zenith_SceneData::TypeID m_uTypeID;
	
	// Factory function: adds this component type to the given entity
	// Returns true on success, false if already has component or other error
	// May be nullptr for render-only components
	bool(*m_pfnAddComponent)(Zenith_Entity&);

	// Check function: returns true if entity already has this component
	bool(*m_pfnHasComponent)(const Zenith_Entity&);

	// Render function: calls RenderPropertiesPanel on the component if entity has it
	// This avoids virtual functions by using type-erased function pointers
	void(*m_pfnRenderPropertiesPanel)(Zenith_Entity&);
};

//==============================================================================
// Component Registry
//==============================================================================
// Central registry of all component types that can be added via the editor.
// Provides:
// - Registration of component types with display names
// - Iteration over all registered components
// - Type-safe component addition to entities
// - Duplicate component prevention
//==============================================================================

class Zenith_ComponentRegistry
{
public:
	//--------------------------------------------------------------------------
	// Singleton Access
	//--------------------------------------------------------------------------
	static Zenith_ComponentRegistry& Get()
	{
		static Zenith_ComponentRegistry s_xInstance;
		return s_xInstance;
	}
	
	//--------------------------------------------------------------------------
	// Component Registration
	//--------------------------------------------------------------------------
	// Register a component type with the registry
	// Template parameter T must satisfy the Zenith_Component concept
	//--------------------------------------------------------------------------
	template<Zenith_Component T>
	void RegisterComponent(const std::string& strDisplayName)
	{
		Zenith_ComponentRegistryEntry xEntry;
		xEntry.m_strDisplayName = strDisplayName;
		xEntry.m_uTypeID = Zenith_SceneData::TypeIDGenerator::GetTypeID<T>();
		
		// Factory function to add component to entity
		// Unary + converts captureless lambda to function pointer
		xEntry.m_pfnAddComponent = +[](Zenith_Entity& xEntity) -> bool
		{
			if (xEntity.HasComponent<T>())
			{
				Zenith_Error(LOG_CATEGORY_ECS, "Cannot add %s: Entity %u already has this component",
					typeid(T).name(), xEntity.GetEntityID());
				return false;
			}

			xEntity.AddComponent<T>();
			Zenith_Log(LOG_CATEGORY_ECS, "Added %s to Entity %u (TypeID: %u)",
				typeid(T).name(), xEntity.GetEntityID(),
				Zenith_SceneData::TypeIDGenerator::GetTypeID<T>());
			return true;
		};

		// Check function to test if entity has component
		xEntry.m_pfnHasComponent = +[](const Zenith_Entity& xEntity) -> bool
		{
			return xEntity.HasComponent<T>();
		};

		// Render function to display component properties in editor
		// Only renders if entity has this component type
		xEntry.m_pfnRenderPropertiesPanel = +[](Zenith_Entity& xEntity) -> void
		{
			if (xEntity.HasComponent<T>())
			{
				xEntity.GetComponent<T>().RenderPropertiesPanel();
			}
		};
		
		m_xEntries.push_back(xEntry);
		
		Zenith_Log(LOG_CATEGORY_ECS, "Registered component: %s (TypeID: %u)",
			strDisplayName.c_str(), xEntry.m_uTypeID);
	}
	

	
	//--------------------------------------------------------------------------
	// Registry Access
	//--------------------------------------------------------------------------
	const std::vector<Zenith_ComponentRegistryEntry>& GetEntries() const
	{
		return m_xEntries;
	}
	
	size_t GetComponentCount() const
	{
		return m_xEntries.size();
	}
	
	//--------------------------------------------------------------------------
	// Component Addition
	//--------------------------------------------------------------------------
	// Try to add a component to an entity by registry index
	// Returns true on success, false if component already exists or index invalid
	//--------------------------------------------------------------------------
	bool TryAddComponent(size_t uIndex, Zenith_Entity& xEntity)
	{
		if (uIndex >= m_xEntries.size())
		{
			Zenith_Error(LOG_CATEGORY_ECS, "Invalid component index %zu", uIndex);
			return false;
		}
		
		const Zenith_ComponentRegistryEntry& xEntry = m_xEntries[uIndex];
		
		// Check for duplicate
		if (xEntry.m_pfnHasComponent(xEntity))
		{
			Zenith_Error(LOG_CATEGORY_ECS, "Cannot add %s to Entity %u: already has this component",
				xEntry.m_strDisplayName.c_str(), xEntity.GetEntityID());
			return false;
		}

		// Add the component
		bool bSuccess = xEntry.m_pfnAddComponent(xEntity);
		
		if (bSuccess)
		{
			Zenith_Log(LOG_CATEGORY_ECS, "Successfully added %s to Entity %u",
				xEntry.m_strDisplayName.c_str(), xEntity.GetEntityID());
		}
		else
		{
			Zenith_Error(LOG_CATEGORY_ECS, "Failed to add %s to Entity %u",
				xEntry.m_strDisplayName.c_str(), xEntity.GetEntityID());
		}
		
		return bSuccess;
	}
	
	//--------------------------------------------------------------------------
	// Check if entity has component by index
	//--------------------------------------------------------------------------
	bool EntityHasComponent(size_t uIndex, const Zenith_Entity& xEntity) const
	{
		if (uIndex >= m_xEntries.size())
		{
			return false;
		}
		return m_xEntries[uIndex].m_pfnHasComponent(xEntity);
	}
	
	//--------------------------------------------------------------------------
	// Static Registration (for auto-registration pattern)
	//--------------------------------------------------------------------------
	// Called by static initializers in component headers to auto-register types
	//--------------------------------------------------------------------------
	template<Zenith_Component T>
	void RegisterComponentAtInit(const std::string& strDisplayName)
	{
		RegisterComponent<T>(strDisplayName);
	}
	

	
	//--------------------------------------------------------------------------
	// Logging
	//--------------------------------------------------------------------------
	// Log all registered components (useful for debugging)
	//--------------------------------------------------------------------------
	void LogRegisteredComponents() const
	{
		Zenith_Log(LOG_CATEGORY_ECS, "=== Registered Components ===");
		for (size_t i = 0; i < m_xEntries.size(); ++i)
		{
			const auto& xEntry = m_xEntries[i];
			Zenith_Log(LOG_CATEGORY_ECS, "  [%zu] %s (TypeID: %u)",
				i, xEntry.m_strDisplayName.c_str(), xEntry.m_uTypeID);
		}
		Zenith_Log(LOG_CATEGORY_ECS, "=== Total: %zu components ===", m_xEntries.size());
	}

private:
	Zenith_ComponentRegistry() = default;
	~Zenith_ComponentRegistry() = default;
	
	// Non-copyable
	Zenith_ComponentRegistry(const Zenith_ComponentRegistry&) = delete;
	Zenith_ComponentRegistry& operator=(const Zenith_ComponentRegistry&) = delete;
	
	std::vector<Zenith_ComponentRegistryEntry> m_xEntries;
};

//==============================================================================
//==============================================================================
#endif // ZENITH_TOOLS
