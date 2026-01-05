#pragma once

#include <string>

class Zenith_DataStream;

/**
 * Zenith_DataAsset - Base class for data-only assets
 *
 * DataAssets are serializable data containers that can be:
 * - Created and edited in the editor
 * - Saved to .zdata files
 * - Referenced by components using DataAssetRef
 * - Loaded asynchronously
 *
 * Usage:
 *   class MyGameConfig : public Zenith_DataAsset
 *   {
 *   public:
 *       ZENITH_DATA_ASSET_TYPE_NAME(MyGameConfig)
 *
 *       float m_fPlayerSpeed = 5.0f;
 *       int m_iMaxHealth = 100;
 *
 *       void WriteToDataStream(Zenith_DataStream& xStream) const override;
 *       void ReadFromDataStream(Zenith_DataStream& xStream) override;
 *   };
 */
class Zenith_DataAsset
{
public:
	virtual ~Zenith_DataAsset() = default;

	/**
	 * Get the type name string for serialization
	 * Used to identify the asset type when loading
	 */
	virtual const char* GetTypeName() const = 0;

	/**
	 * Serialize asset data to a data stream
	 */
	virtual void WriteToDataStream(Zenith_DataStream& xStream) const = 0;

	/**
	 * Deserialize asset data from a data stream
	 */
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) = 0;

#ifdef ZENITH_TOOLS
	/**
	 * Render the asset's properties in ImGui for editing
	 * Override this to provide a custom editor UI
	 */
	virtual void RenderPropertiesPanel() {}
#endif

	/**
	 * Get the file path this asset was loaded from (empty if not loaded from file)
	 */
	const std::string& GetFilePath() const { return m_strFilePath; }

	/**
	 * Set the file path (called by the loader)
	 */
	void SetFilePath(const std::string& strPath) { m_strFilePath = strPath; }

protected:
	std::string m_strFilePath;
};

/**
 * Macro to implement GetTypeName() for a DataAsset subclass
 * Usage: ZENITH_DATA_ASSET_TYPE_NAME(MyDataAssetClass)
 */
#define ZENITH_DATA_ASSET_TYPE_NAME(ClassName) \
	const char* GetTypeName() const override { return #ClassName; }
