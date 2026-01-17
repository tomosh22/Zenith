#pragma once

#include <string>
#include <atomic>

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

protected:
	Zenith_Asset() = default;

	// Path is set by the registry
	std::string m_strPath;

private:
	std::atomic<uint32_t> m_uRefCount{0};

	friend class Zenith_AssetRegistry;
};
