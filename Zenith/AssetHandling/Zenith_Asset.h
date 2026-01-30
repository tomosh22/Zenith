#pragma once

#include <string>
#include <atomic>

// Forward declaration for serialization
class Zenith_DataStream;

/**
 * Zenith_Asset - Base class for all assets in the engine
 *
 * All assets inherit from this class and are managed by Zenith_AssetRegistry.
 * Assets are identified by path (no GUIDs) and use intrusive reference counting.
 *
 * Lifecycle:
 * - Created by registry (Get<T> or Create<T>)
 * - Reference count incremented by AssetHandle copies
 * - Reference count decremented on AssetHandle destruction
 * - Deleted by registry when ref count reaches 0
 *
 * For disk assets: path is the file path (e.g., "Assets/Textures/diffuse.ztex")
 * For procedural assets: path is generated (e.g., "procedural://texture_0")
 */
class Zenith_Asset
{
public:
	virtual ~Zenith_Asset() = default;

	// Prevent copying - use handles for references
	Zenith_Asset(const Zenith_Asset&) = delete;
	Zenith_Asset& operator=(const Zenith_Asset&) = delete;

	/**
	 * Get the asset's path (identifier)
	 */
	const std::string& GetPath() const { return m_strPath; }

	/**
	 * Check if this is a procedural (code-created) asset
	 */
	bool IsProcedural() const;

	/**
	 * Increment reference count
	 * @return New reference count
	 */
	uint32_t AddRef();

	/**
	 * Decrement reference count
	 * @return New reference count (caller should check if 0 for cleanup)
	 */
	uint32_t Release();

	/**
	 * Get current reference count (for debugging)
	 */
	uint32_t GetRefCount() const;

	//--------------------------------------------------------------------------
	// Serialization Support (optional - override for serializable assets)
	//--------------------------------------------------------------------------

	/**
	 * Get the type name for factory registration
	 * Override this for assets that support DataStream serialization (.zdata files)
	 * @return Type name string, or nullptr for non-serializable assets
	 */
	virtual const char* GetTypeName() const { return nullptr; }

	/**
	 * Serialize asset data to a data stream
	 * Override this for assets that support saving to .zdata files
	 */
	virtual void WriteToDataStream(Zenith_DataStream& xStream) const { (void)xStream; }

	/**
	 * Deserialize asset data from a data stream
	 * Override this for assets that support loading from .zdata files
	 */
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) { (void)xStream; }

#ifdef ZENITH_TOOLS
	/**
	 * Render the asset's properties in ImGui for editing
	 * Override this to provide a custom editor UI
	 */
	virtual void RenderPropertiesPanel() {}
#endif

protected:
	Zenith_Asset() = default;

	// Path is set by the registry
	std::string m_strPath;

private:
	std::atomic<uint32_t> m_uRefCount{0};

	friend class Zenith_AssetRegistry;
};

//--------------------------------------------------------------------------
// Macros for serializable assets
//--------------------------------------------------------------------------

/**
 * Macro to implement GetTypeName() for a serializable asset
 * Usage: ZENITH_ASSET_TYPE_NAME(MyAssetClass)
 */
#define ZENITH_ASSET_TYPE_NAME(ClassName) \
	const char* GetTypeName() const override { return #ClassName; }

/**
 * Macro to register a serializable asset type at static initialization time
 * Place in a .cpp file:
 *   ZENITH_REGISTER_ASSET_TYPE(MyAssetClass)
 */
#define ZENITH_REGISTER_ASSET_TYPE(ClassName) \
	namespace { \
		struct ClassName##_AssetTypeRegistrar { \
			ClassName##_AssetTypeRegistrar() { \
				Zenith_AssetRegistry::RegisterAssetType<ClassName>(); \
			} \
		}; \
		static ClassName##_AssetTypeRegistrar g_x##ClassName##Registrar; \
	}
