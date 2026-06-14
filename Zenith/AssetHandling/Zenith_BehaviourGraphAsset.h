#pragma once

#include "AssetHandling/Zenith_Asset.h"
#include "Scripting/Zenith_BehaviourGraph.h"

//------------------------------------------------------------------------------
// Zenith_BehaviourGraphAsset - the .bgraph asset: a serialized
// Zenith_GraphDefinition managed by Zenith_AssetRegistry (path-keyed cache,
// ref counting, the standard Save/Get pipeline). Ships inside the APK via the
// ordinary game-asset pipeline like every other binary asset.
//
// Zenith_GraphComponent slots reference an asset by path and instantiate
// per-slot Zenith_BehaviourGraph runtimes from the shared definition.
//------------------------------------------------------------------------------
class Zenith_BehaviourGraphAsset : public Zenith_Asset
{
public:
	Zenith_BehaviourGraphAsset() = default;

	// Persisted type-name string — must stay "BehaviourGraphAsset" (it keys the
	// .bgraph/.zdata type registry). Not via ZENITH_ASSET_TYPE_NAME because that
	// macro would stringize the C++ class name (Zenith_BehaviourGraphAsset).
	static constexpr const char* StaticTypeName() { return "BehaviourGraphAsset"; }
	const char* GetTypeName() const override { return StaticTypeName(); }
	void WriteToDataStream(Zenith_DataStream& xStream) const override { m_xDefinition.WriteToDataStream(xStream); }
	void ReadFromDataStream(Zenith_DataStream& xStream) override { m_bLoadedOk = m_xDefinition.ReadFromDataStream(xStream); }

	Zenith_GraphDefinition& GetDefinition() { return m_xDefinition; }
	const Zenith_GraphDefinition& GetDefinition() const { return m_xDefinition; }
	bool LoadedOk() const { return m_bLoadedOk; }

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel() override;
#endif

private:
	Zenith_GraphDefinition m_xDefinition;
	bool m_bLoadedOk = true;
};

// Force-link anchor: Zenith_GraphComponent calls this so the asset TU (which
// holds the ZENITH_REGISTER_ASSET_TYPE static registrar) survives /OPT:REF -
// the registrar only runs if the .obj is linked (the MSVC dead-strip pitfall).
bool Zenith_BehaviourGraphAsset_ForceLink();
