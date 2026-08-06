#pragma once

#include "AssetHandling/Zenith_Asset.h"
#include "Flux/Vegetation/Flux_GrassTypeTable.h"

//------------------------------------------------------------------------------
// Zenith_GrassTypeTableAsset - the .zdata asset wrapping an authored
// Flux_GrassTypeTable, managed by Zenith_AssetRegistry (path-keyed cache, ref
// counting, the standard Save/Get pipeline).
//
// The table is held BY VALUE and is pure CPU state: nothing here touches the
// device, so the asset loads identically in a Null_ (headless) build. The
// consumer (Flux_GrassImpl) copies the table out and projects it onto the GPU
// block itself.
//------------------------------------------------------------------------------
class Zenith_GrassTypeTableAsset : public Zenith_Asset
{
public:
	Zenith_GrassTypeTableAsset() = default;

	ZENITH_ASSET_TYPE_NAME(Zenith_GrassTypeTableAsset)

	void WriteToDataStream(Zenith_DataStream& xStream) const override { m_xTable.WriteToDataStream(xStream); }
	// A rejected stream leaves m_xTable holding the built-in default set (its
	// reader never half-writes), so GetTable() is valid even when LoadedOk() is
	// false — the flag is what lets the consumer prefer its OWN table instead.
	void ReadFromDataStream(Zenith_DataStream& xStream) override { m_bLoadedOk = m_xTable.ReadFromDataStream(xStream); }

	const Flux_GrassTypeTable& GetTable() const { return m_xTable; }
	void SetTable(const Flux_GrassTypeTable& xTable);

	bool LoadedOk() const { return m_bLoadedOk; }

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel() override;
#endif

private:
	Flux_GrassTypeTable m_xTable;
	bool m_bLoadedOk = true;
};

// The one path the grass feature boot-loads. `game:` resolves through
// Zenith_AssetRegistry::ResolvePath, so --assets-root packaging works unchanged.
// No game ships this file today: its ABSENCE is the normal state, not a failure.
constexpr const char* szZENITH_GRASS_TYPE_TABLE_ASSET_PATH = "game:Vegetation/GrassTypes.zdata";

// Force-link anchor: Flux_GrassImpl calls this so the asset TU (which holds the
// ZENITH_REGISTER_ASSET_TYPE static registrar) survives /OPT:REF - the registrar
// only runs if the .obj is linked (the MSVC dead-strip pitfall).
bool Zenith_GrassTypeTableAsset_ForceLink();
