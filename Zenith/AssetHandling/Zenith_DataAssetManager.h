#pragma once

#include "Zenith_DataAsset.h"
#include "Core/Multithreading/Zenith_Multithreading.h"
#include <unordered_map>
#include <string>

/**
 * Factory function type for creating DataAsset instances
 */
using DataAssetFactoryFn = Zenith_DataAsset*(*)();

/**
 * Zenith_DataAssetManager - Registry and factory for DataAsset types
 *
 * Manages:
 * - Type registration (mapping type names to factory functions)
 * - Asset creation (instantiating assets by type name)
 * - Loading/saving assets to .zdata files
 * - Caching loaded assets
 *
 * File format (.zdata):
 *   - 4 bytes: Magic number "ZDAT"
 *   - 4 bytes: Version number
 *   - String: Type name (null-terminated)
 *   - Binary: Serialized asset data (via DataStream)
 */
class Zenith_DataAssetManager
{
public:
	static constexpr uint32_t ZDATA_MAGIC = 0x5441445A;  // "ZDAT" in little-endian
	static constexpr uint32_t ZDATA_VERSION = 1;

	/**
	 * Register a DataAsset type with the manager
	 * @param szTypeName The type name string (must match GetTypeName())
	 * @param pfnFactory Factory function to create instances
	 */
	static void RegisterDataAssetType(const char* szTypeName, DataAssetFactoryFn pfnFactory);

	/**
	 * Template helper for registering DataAsset types
	 */
	template<typename T>
	static void RegisterDataAssetType()
	{
		T xTemp;  // Create temporary to get type name
		RegisterDataAssetType(xTemp.GetTypeName(), []() -> Zenith_DataAsset* { return new T(); });
	}

	/**
	 * Create a new DataAsset instance by type name
	 * @param szTypeName The registered type name
	 * @return New instance or nullptr if type not found
	 */
	static Zenith_DataAsset* CreateDataAsset(const char* szTypeName);

	/**
	 * Template helper for creating DataAsset instances
	 */
	template<typename T>
	static T* CreateDataAsset()
	{
		T xTemp;
		return static_cast<T*>(CreateDataAsset(xTemp.GetTypeName()));
	}

	/**
	 * Load a DataAsset from a .zdata file
	 * @param strPath Path to the .zdata file
	 * @return Loaded asset or nullptr on failure
	 */
	static Zenith_DataAsset* LoadDataAsset(const std::string& strPath);

	/**
	 * Template helper for loading with type checking
	 */
	template<typename T>
	static T* LoadDataAsset(const std::string& strPath)
	{
		Zenith_DataAsset* pxAsset = LoadDataAsset(strPath);
		if (pxAsset)
		{
			T xTemp;
			if (strcmp(pxAsset->GetTypeName(), xTemp.GetTypeName()) == 0)
			{
				return static_cast<T*>(pxAsset);
			}
			// Wrong type - delete and return nullptr
			delete pxAsset;
		}
		return nullptr;
	}

	/**
	 * Save a DataAsset to a .zdata file
	 * @param pxAsset The asset to save
	 * @param strPath Path to save to (updates asset's stored path)
	 * @return true on success
	 */
	static bool SaveDataAsset(Zenith_DataAsset* pxAsset, const std::string& strPath);

	/**
	 * Save a DataAsset to its stored file path
	 * @param pxAsset The asset to save
	 * @return true on success, false if no path set
	 */
	static bool SaveDataAsset(Zenith_DataAsset* pxAsset);

	/**
	 * Get a cached asset by path (nullptr if not cached)
	 */
	static Zenith_DataAsset* GetCachedAsset(const std::string& strPath);

	/**
	 * Clear the asset cache
	 */
	static void ClearCache();

	/**
	 * Check if a type is registered
	 */
	static bool IsTypeRegistered(const char* szTypeName);

private:
	// Type registry: type name -> factory function
	static std::unordered_map<std::string, DataAssetFactoryFn> s_xTypeRegistry;
	static Zenith_Mutex s_xRegistryMutex;

	// Asset cache: file path -> loaded asset
	static std::unordered_map<std::string, Zenith_DataAsset*> s_xAssetCache;
	static Zenith_Mutex s_xCacheMutex;
};

/**
 * Macro to register a DataAsset type at static initialization time
 * Place in a .cpp file:
 *   ZENITH_REGISTER_DATA_ASSET(MyGameConfig)
 */
#define ZENITH_REGISTER_DATA_ASSET(ClassName) \
	namespace { \
		struct ClassName##_DataAssetRegistrar { \
			ClassName##_DataAssetRegistrar() { \
				Zenith_DataAssetManager::RegisterDataAssetType<ClassName>(); \
			} \
		}; \
		static ClassName##_DataAssetRegistrar g_x##ClassName##Registrar; \
	}
