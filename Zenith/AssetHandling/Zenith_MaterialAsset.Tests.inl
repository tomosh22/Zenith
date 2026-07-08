#include "UnitTests/Zenith_UnitTests.h"
#include "DataStream/Zenith_DataStream.h"
#include "DataStream/Zenith_StreamEnvelope.h"
#include "AssetHandling/Zenith_AssetTypeIds.h"

// ============================================================================
// Zenith_MaterialAsset unit tests
//
// Cover the v5 serialization roundtrip, the legacy v4 -> v5 load mapping,
// the parent/override instance-resolution model (including stamp-based cache
// invalidation and cycle rejection), and the param reflection table.
//
// Tests construct materials on the stack (no registry involvement) - the
// asset registry is only needed for texture RESOLUTION, which these tests
// never trigger (they only read paths back).
// ============================================================================

namespace
{
	// Serialize a legacy v4-format material byte stream, mirroring the exact
	// field order the old Zenith_MaterialAsset::WriteToDataStream produced.
	void WriteLegacyV4Material(Zenith_DataStream& xStream,
		const char* szName,
		const Zenith_Maths::Vector4& xBaseColor,
		float fMetallic, float fRoughness,
		const Zenith_Maths::Vector3& xEmissiveColor, float fEmissiveIntensity,
		bool bTransparent, float fAlphaCutoff,
		const Zenith_Maths::Vector2& xUVTiling, const Zenith_Maths::Vector2& xUVOffset,
		float fOcclusionStrength, bool bTwoSided, bool bUnlit,
		const char* szDiffusePath)
	{
		const uint32_t uVersion = 4;
		xStream << uVersion;
		xStream << std::string(szName);
		xStream << xBaseColor.x; xStream << xBaseColor.y; xStream << xBaseColor.z; xStream << xBaseColor.w;
		xStream << fMetallic;
		xStream << fRoughness;
		xStream << xEmissiveColor.x; xStream << xEmissiveColor.y; xStream << xEmissiveColor.z;
		xStream << fEmissiveIntensity;
		xStream << bTransparent;
		xStream << fAlphaCutoff;
		xStream << xUVTiling.x; xStream << xUVTiling.y;
		xStream << xUVOffset.x; xStream << xUVOffset.y;
		xStream << fOcclusionStrength;
		xStream << bTwoSided;
		xStream << bUnlit;
		xStream << std::string(szDiffusePath);	// diffuse
		xStream << std::string("");				// normal
		xStream << std::string("");				// roughness/metallic
		xStream << std::string("");				// occlusion
		xStream << std::string("");				// emissive
	}
}

//--------------------------------------------------------------------------
// Reflection table
//--------------------------------------------------------------------------

ZENITH_TEST(MaterialAsset, ParamTableLookupByName)
{
	const Zenith_MaterialParamDesc* pxDesc = Zenith_MaterialParamTable::FindParamByName("Roughness");
	ZENITH_ASSERT_NOT_NULL(pxDesc, "Roughness must exist in the param table");
	ZENITH_ASSERT_EQ(pxDesc->m_eID, MATERIAL_PARAM_ROUGHNESS, "Name lookup must return the matching ID");
	ZENITH_ASSERT_EQ(pxDesc->m_eType, MATERIAL_PARAM_TYPE_FLOAT, "Roughness is a float param");

	ZENITH_ASSERT_NULL(Zenith_MaterialParamTable::FindParamByName("NoSuchParam"),
		"Unknown names must return null");

	// Table order must match the enum so GetParamDesc can index directly.
	for (u_int u = 0; u < Zenith_MaterialParamTable::GetParamCount(); u++)
	{
		const MaterialParamID eID = static_cast<MaterialParamID>(u);
		ZENITH_ASSERT_EQ(Zenith_MaterialParamTable::GetParamDesc(eID).m_eID, eID,
			"Param table order must match MaterialParamID order");
	}
}

ZENITH_TEST(MaterialAsset, TextureSlotTableLookupByName)
{
	const Zenith_MaterialTextureSlotDesc* pxDesc = Zenith_MaterialParamTable::FindTextureSlotByName("Height");
	ZENITH_ASSERT_NOT_NULL(pxDesc, "Height slot must exist in the texture-slot table");
	ZENITH_ASSERT_EQ(pxDesc->m_eSlot, MATERIAL_TEXTURE_HEIGHT, "Name lookup must return the matching slot");

	for (u_int u = 0; u < Zenith_MaterialParamTable::GetTextureSlotCount(); u++)
	{
		const MaterialTextureSlot eSlot = static_cast<MaterialTextureSlot>(u);
		ZENITH_ASSERT_EQ(Zenith_MaterialParamTable::GetTextureSlotDesc(eSlot).m_eSlot, eSlot,
			"Texture-slot table order must match MaterialTextureSlot order");
	}
}

ZENITH_TEST(MaterialAsset, ParamTableTypedAccessors)
{
	Zenith_MaterialParams xParams;

	Zenith_MaterialParamTable::SetParamFloat(xParams, MATERIAL_PARAM_METALLIC, 0.75f);
	ZENITH_ASSERT_EQ_FLOAT(Zenith_MaterialParamTable::GetParamFloat(xParams, MATERIAL_PARAM_METALLIC), 0.75f, 0.0001f,
		"Float accessor roundtrip");
	ZENITH_ASSERT_EQ_FLOAT(xParams.m_fMetallic, 0.75f, 0.0001f, "Float accessor must write the POD field");

	Zenith_MaterialParamTable::SetParamVector(xParams, MATERIAL_PARAM_BASE_COLOR, Zenith_Maths::Vector4(0.1f, 0.2f, 0.3f, 0.4f));
	ZENITH_ASSERT_EQ_FLOAT(xParams.m_xBaseColor.z, 0.3f, 0.0001f, "Vector accessor must write the POD field");

	Zenith_MaterialParamTable::SetParamInt(xParams, MATERIAL_PARAM_BLEND_MODE, MATERIAL_BLEND_TRANSLUCENT);
	ZENITH_ASSERT_EQ(xParams.m_eBlendMode, MATERIAL_BLEND_TRANSLUCENT, "Int accessor must write enum fields");
	ZENITH_ASSERT_EQ(Zenith_MaterialParamTable::GetParamInt(xParams, MATERIAL_PARAM_BLEND_MODE),
		static_cast<u_int>(MATERIAL_BLEND_TRANSLUCENT), "Int accessor roundtrip");

	Zenith_MaterialParamTable::SetParamInt(xParams, MATERIAL_PARAM_TWO_SIDED, 1);
	ZENITH_ASSERT_TRUE(xParams.m_bTwoSided, "Bool params travel through the int accessor");
}

//--------------------------------------------------------------------------
// Serialization
//--------------------------------------------------------------------------

ZENITH_TEST(MaterialAsset, WritesSharedStreamEnvelope)
{
	// Workstream B: .zmtrl now leads with the shared stream envelope (was a bare
	// version word). Lock the identity so the format stays consistent with the
	// other typed assets, and confirm ParseStream accepts its own output.
	Zenith_MaterialAsset xSource;
	xSource.SetName("EnvelopeMat");
	xSource.SetRoughness(0.4f);

	Zenith_DataStream xStream;
	xSource.WriteToDataStream(xStream);

	xStream.SetCursor(0);
	Zenith_Result<Zenith_StreamHeader> xHdr = Zenith_ReadStreamHeader(xStream, uZENITH_MATERIAL_ASSET_TYPE_ID);
	ZENITH_ASSERT_TRUE(xHdr.IsOk(), "material write must emit the shared stream envelope");
	if (xHdr.IsOk())
	{
		ZENITH_ASSERT_EQ(xHdr.Value().m_uAssetTypeId, uZENITH_MATERIAL_ASSET_TYPE_ID, "material envelope type id");
		ZENITH_ASSERT_EQ(xHdr.Value().m_uSchemaVersion, uZENITH_MATERIAL_SCHEMA_CURRENT, "material envelope schema");
	}

	xStream.SetCursor(0);
	Zenith_MaterialAsset xLoaded;
	ZENITH_ASSERT_TRUE(xLoaded.ParseStream(xStream).IsOk(), "ParseStream must accept its own output");
	ZENITH_ASSERT_EQ(xLoaded.GetName(), std::string("EnvelopeMat"), "name round-trips through the envelope");
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetRoughness(), 0.4f, 1e-6f, "param round-trips through the envelope");
}

ZENITH_TEST(MaterialAsset, V5SerializationRoundtrip)
{
	Zenith_MaterialAsset xSource;
	xSource.SetName("RoundtripMat");
	xSource.SetBaseColor(Zenith_Maths::Vector4(0.25f, 0.5f, 0.75f, 1.0f));
	xSource.SetMetallic(1.0f);
	xSource.SetRoughness(0.33f);
	xSource.SetSpecular(0.9f);
	xSource.SetNormalStrength(1.5f);
	xSource.SetHeightScale(0.08f);
	xSource.SetDetailTiling(Zenith_Maths::Vector2(8.0f, 8.0f));
	xSource.SetEmissiveColor(Zenith_Maths::Vector3(1.0f, 0.5f, 0.25f));
	xSource.SetEmissiveIntensity(40.0f);
	xSource.SetAlphaCutoff(0.6f);
	xSource.SetOcclusionStrength(0.8f);
	xSource.SetUVTiling(Zenith_Maths::Vector2(2.0f, 3.0f));
	xSource.SetUVOffset(Zenith_Maths::Vector2(0.1f, -0.2f));
	xSource.SetClearCoatStrength(1.0f);
	xSource.SetClearCoatRoughness(0.05f);
	xSource.SetBlendMode(MATERIAL_BLEND_MASKED);
	xSource.SetShadingModel(MATERIAL_SHADING_DEFAULT_LIT);
	xSource.SetTwoSided(true);
	xSource.SetTexture(MATERIAL_TEXTURE_BASE_COLOR, TextureHandle("game:Textures/albedo.ztxtr"));
	xSource.SetTexture(MATERIAL_TEXTURE_HEIGHT, TextureHandle("game:Textures/height.ztxtr"));
	xSource.SetTexture(MATERIAL_TEXTURE_DETAIL_NORMAL, TextureHandle("game:Textures/detail_n.ztxtr"));

	Zenith_DataStream xStream;
	xSource.WriteToDataStream(xStream);
	xStream.SetCursor(0);

	Zenith_MaterialAsset xLoaded;
	xLoaded.ReadFromDataStream(xStream);

	ZENITH_ASSERT_EQ(xLoaded.GetName(), std::string("RoundtripMat"), "Name roundtrip");
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetBaseColor().y, 0.5f, 0.0001f, "BaseColor roundtrip");
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetMetallic(), 1.0f, 0.0001f, "Metallic roundtrip");
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetRoughness(), 0.33f, 0.0001f, "Roughness roundtrip");
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetSpecular(), 0.9f, 0.0001f, "Specular roundtrip");
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetNormalStrength(), 1.5f, 0.0001f, "NormalStrength roundtrip");
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetHeightScale(), 0.08f, 0.0001f, "HeightScale roundtrip");
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetDetailTiling().x, 8.0f, 0.0001f, "DetailTiling roundtrip");
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetEmissiveColor().x, 1.0f, 0.0001f, "EmissiveColor roundtrip");
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetEmissiveIntensity(), 40.0f, 0.0001f, "EmissiveIntensity roundtrip");
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetAlphaCutoff(), 0.6f, 0.0001f, "AlphaCutoff roundtrip");
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetOcclusionStrength(), 0.8f, 0.0001f, "OcclusionStrength roundtrip");
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetUVTiling().y, 3.0f, 0.0001f, "UVTiling roundtrip");
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetUVOffset().y, -0.2f, 0.0001f, "UVOffset roundtrip");
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetClearCoatStrength(), 1.0f, 0.0001f, "ClearCoatStrength roundtrip");
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetClearCoatRoughness(), 0.05f, 0.0001f, "ClearCoatRoughness roundtrip");
	ZENITH_ASSERT_EQ(xLoaded.GetBlendMode(), MATERIAL_BLEND_MASKED, "BlendMode roundtrip");
	ZENITH_ASSERT_TRUE(xLoaded.IsTwoSided(), "TwoSided roundtrip");
	ZENITH_ASSERT_EQ(xLoaded.GetTexturePath(MATERIAL_TEXTURE_BASE_COLOR), std::string("game:Textures/albedo.ztxtr"),
		"BaseColor texture path roundtrip");
	ZENITH_ASSERT_EQ(xLoaded.GetTexturePath(MATERIAL_TEXTURE_HEIGHT), std::string("game:Textures/height.ztxtr"),
		"Height texture path roundtrip");
	ZENITH_ASSERT_EQ(xLoaded.GetTexturePath(MATERIAL_TEXTURE_DETAIL_NORMAL), std::string("game:Textures/detail_n.ztxtr"),
		"DetailNormal texture path roundtrip");
	ZENITH_ASSERT_EQ(xLoaded.GetTexturePath(MATERIAL_TEXTURE_DETAIL_MASK), std::string(""),
		"Unset slots stay empty through the roundtrip");
}

ZENITH_TEST(MaterialAsset, V5ParentAndOverrideMaskRoundtrip)
{
	Zenith_MaterialAsset xSource;
	xSource.SetName("InstanceMat");
	// Set the parent by path only (no registry load in unit tests).
	ZENITH_ASSERT_TRUE(xSource.SetParent(MaterialHandle("game:Materials/master.zmat")),
		"Setting an unloaded parent path must succeed");
	xSource.SetRoughness(0.1f);	// auto-marks the Roughness override (parent set)
	xSource.SetOverride(MATERIAL_PARAM_METALLIC, true);

	ZENITH_ASSERT_TRUE(xSource.HasParamOverride(MATERIAL_PARAM_ROUGHNESS),
		"Typed setters must auto-mark overrides when parented");

	Zenith_DataStream xStream;
	xSource.WriteToDataStream(xStream);
	xStream.SetCursor(0);

	Zenith_MaterialAsset xLoaded;
	xLoaded.ReadFromDataStream(xStream);

	ZENITH_ASSERT_EQ(xLoaded.GetParentHandle().GetPath(), std::string("game:Materials/master.zmat"),
		"Parent path roundtrip");
	ZENITH_ASSERT_TRUE(xLoaded.HasParamOverride(MATERIAL_PARAM_ROUGHNESS), "Override mask roundtrip (roughness)");
	ZENITH_ASSERT_TRUE(xLoaded.HasParamOverride(MATERIAL_PARAM_METALLIC), "Override mask roundtrip (metallic)");
	ZENITH_ASSERT_FALSE(xLoaded.HasParamOverride(MATERIAL_PARAM_BASE_COLOR), "Unset override bits stay clear");
}

ZENITH_TEST(MaterialAsset, LegacyV4LoadMapping)
{
	// Non-transparent v4 material: must arrive as MASKED (v4 always alpha-
	// tested), unlit flag must map to the shading model.
	Zenith_DataStream xStream;
	WriteLegacyV4Material(xStream, "LegacyMat",
		Zenith_Maths::Vector4(0.5f, 0.6f, 0.7f, 1.0f),
		0.25f, 0.65f,
		Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), 2.0f,
		false /*transparent*/, 0.45f,
		Zenith_Maths::Vector2(2.0f, 2.0f), Zenith_Maths::Vector2(0.5f, 0.5f),
		0.9f, true /*twoSided*/, true /*unlit*/,
		"game:Textures/legacy_diffuse.ztxtr");
	xStream.SetCursor(0);

	Zenith_MaterialAsset xLoaded;
	xLoaded.ReadFromDataStream(xStream);

	ZENITH_ASSERT_EQ(xLoaded.GetName(), std::string("LegacyMat"), "v4 name");
	ZENITH_ASSERT_EQ(xLoaded.GetBlendMode(), MATERIAL_BLEND_MASKED,
		"v4 non-transparent must map to Masked (alpha test was always on)");
	ZENITH_ASSERT_EQ(xLoaded.GetShadingModel(), MATERIAL_SHADING_UNLIT, "v4 unlit flag maps to the shading model");
	ZENITH_ASSERT_TRUE(xLoaded.IsTwoSided(), "v4 two-sided flag maps to the param");
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetMetallic(), 0.25f, 0.0001f, "v4 metallic preserved");
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetRoughness(), 0.65f, 0.0001f, "v4 roughness preserved");
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetAlphaCutoff(), 0.45f, 0.0001f, "v4 alpha cutoff preserved");
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetEmissiveIntensity(), 2.0f, 0.0001f, "v4 emissive intensity preserved (not defaulted)");
	ZENITH_ASSERT_EQ(xLoaded.GetTexturePath(MATERIAL_TEXTURE_BASE_COLOR), std::string("game:Textures/legacy_diffuse.ztxtr"),
		"v4 diffuse path maps to the BaseColor slot");

	// New-in-v5 params must arrive at their defaults.
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetSpecular(), 0.5f, 0.0001f, "v4 load defaults specular to 0.5");
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetClearCoatStrength(), 0.0f, 0.0001f, "v4 load defaults clear coat off");
	ZENITH_ASSERT_FALSE(xLoaded.HasParent(), "v4 materials have no parent");
	ZENITH_ASSERT_EQ(xLoaded.GetOverrideMask(), 0ull, "v4 materials have no overrides");
}

ZENITH_TEST(MaterialAsset, LegacyV4TransparentMapsToTranslucent)
{
	Zenith_DataStream xStream;
	WriteLegacyV4Material(xStream, "LegacyGlass",
		Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 0.5f),
		0.0f, 0.1f,
		Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), 0.0f,
		true /*transparent*/, 0.5f,
		Zenith_Maths::Vector2(1.0f, 1.0f), Zenith_Maths::Vector2(0.0f, 0.0f),
		1.0f, false, false,
		"");
	xStream.SetCursor(0);

	Zenith_MaterialAsset xLoaded;
	xLoaded.ReadFromDataStream(xStream);

	ZENITH_ASSERT_EQ(xLoaded.GetBlendMode(), MATERIAL_BLEND_TRANSLUCENT,
		"v4 transparent flag must map to Translucent");
	ZENITH_ASSERT_TRUE(xLoaded.IsTransparent(), "Legacy IsTransparent shim tracks the blend mode");
}

//--------------------------------------------------------------------------
// Instance resolution
//--------------------------------------------------------------------------

ZENITH_TEST(MaterialAsset, ResolveWithoutParentReturnsLocalValues)
{
	Zenith_MaterialAsset xMat;
	xMat.SetRoughness(0.2f);
	xMat.SetTexture(MATERIAL_TEXTURE_BASE_COLOR, TextureHandle("game:Textures/a.ztxtr"));

	const Zenith_MaterialResolved& xResolved = xMat.GetResolved();
	ZENITH_ASSERT_EQ_FLOAT(xResolved.m_xParams.m_fRoughness, 0.2f, 0.0001f, "Resolve without parent = local params");
	ZENITH_ASSERT_EQ(xResolved.m_apxTextures[MATERIAL_TEXTURE_BASE_COLOR]->GetPath(), std::string("game:Textures/a.ztxtr"),
		"Resolve without parent = local textures");
}

ZENITH_TEST(MaterialAsset, ResolveOverlaysOnlyOverriddenParams)
{
	// Procedural parent/child via the registry so MaterialHandle can hold a
	// direct pointer (Set()).
	auto xhParent = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Zenith_MaterialAsset* pxParent = xhParent.GetDirect();
	auto xhChild = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Zenith_MaterialAsset* pxChild = xhChild.GetDirect();
	ZENITH_ASSERT_NOT_NULL(pxParent, "Registry must create the parent");
	ZENITH_ASSERT_NOT_NULL(pxChild, "Registry must create the child");

	pxParent->SetRoughness(0.9f);
	pxParent->SetMetallic(1.0f);
	pxParent->SetBaseColor(Zenith_Maths::Vector4(0.0f, 1.0f, 0.0f, 1.0f));
	pxParent->SetTexture(MATERIAL_TEXTURE_BASE_COLOR, TextureHandle("game:Textures/parent.ztxtr"));
	pxParent->SetTexture(MATERIAL_TEXTURE_NORMAL, TextureHandle("game:Textures/parent_n.ztxtr"));

	MaterialHandle xParentHandle(pxParent);
	ZENITH_ASSERT_TRUE(pxChild->SetParent(xParentHandle), "SetParent must accept a valid parent");

	// Override roughness + base-colour texture only.
	pxChild->SetRoughness(0.1f);
	pxChild->SetTexture(MATERIAL_TEXTURE_BASE_COLOR, TextureHandle("game:Textures/child.ztxtr"));

	const Zenith_MaterialResolved& xResolved = pxChild->GetResolved();
	ZENITH_ASSERT_EQ_FLOAT(xResolved.m_xParams.m_fRoughness, 0.1f, 0.0001f, "Overridden param uses the child value");
	ZENITH_ASSERT_EQ_FLOAT(xResolved.m_xParams.m_fMetallic, 1.0f, 0.0001f, "Non-overridden param tracks the parent");
	ZENITH_ASSERT_EQ_FLOAT(xResolved.m_xParams.m_xBaseColor.y, 1.0f, 0.0001f, "Non-overridden colour tracks the parent");
	ZENITH_ASSERT_EQ(xResolved.m_apxTextures[MATERIAL_TEXTURE_BASE_COLOR]->GetPath(), std::string("game:Textures/child.ztxtr"),
		"Overridden texture slot uses the child handle");
	ZENITH_ASSERT_EQ(xResolved.m_apxTextures[MATERIAL_TEXTURE_NORMAL]->GetPath(), std::string("game:Textures/parent_n.ztxtr"),
		"Non-overridden texture slot tracks the parent");

	// Live parent edit: child resolve must pick the change up (stamp chain).
	pxParent->SetMetallic(0.0f);
	const Zenith_MaterialResolved& xResolved2 = pxChild->GetResolved();
	ZENITH_ASSERT_EQ_FLOAT(xResolved2.m_xParams.m_fMetallic, 0.0f, 0.0001f,
		"Parent edits must invalidate the child's resolve cache");
	ZENITH_ASSERT_EQ_FLOAT(xResolved2.m_xParams.m_fRoughness, 0.1f, 0.0001f,
		"Child overrides survive parent edits");

	// Clearing the override returns the row to tracking the parent.
	pxChild->SetOverride(MATERIAL_PARAM_ROUGHNESS, false);
	const Zenith_MaterialResolved& xResolved3 = pxChild->GetResolved();
	ZENITH_ASSERT_EQ_FLOAT(xResolved3.m_xParams.m_fRoughness, 0.9f, 0.0001f,
		"Clearing an override must re-pull the parent value");

	pxChild->ClearParent();
	const Zenith_MaterialResolved& xResolved4 = pxChild->GetResolved();
	ZENITH_ASSERT_EQ_FLOAT(xResolved4.m_xParams.m_fRoughness, 0.1f, 0.0001f,
		"After ClearParent the local snapshot is authoritative again");

	// Create() applies no AddRef - both assets are refcount 0 (the child's
	// parent handle was cleared above) and UnloadUnused collects them. Run it
	// twice so the child releasing its remaining handles cascades.
	Zenith_AssetRegistry::UnloadUnused();
	Zenith_AssetRegistry::UnloadUnused();
}

ZENITH_TEST(MaterialAsset, GrandparentChainResolves)
{
	auto xhRoot = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Zenith_MaterialAsset* pxRoot = xhRoot.GetDirect();
	auto xhMid = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Zenith_MaterialAsset* pxMid = xhMid.GetDirect();
	auto xhLeaf = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Zenith_MaterialAsset* pxLeaf = xhLeaf.GetDirect();

	pxRoot->SetRoughness(0.8f);
	pxRoot->SetMetallic(1.0f);
	pxRoot->SetSpecular(0.7f);

	ZENITH_ASSERT_TRUE(pxMid->SetParent(MaterialHandle(pxRoot)), "Mid->Root parenting");
	pxMid->SetMetallic(0.5f);	// override at the middle level

	ZENITH_ASSERT_TRUE(pxLeaf->SetParent(MaterialHandle(pxMid)), "Leaf->Mid parenting");
	pxLeaf->SetRoughness(0.05f);	// override at the leaf

	const Zenith_MaterialResolved& xResolved = pxLeaf->GetResolved();
	ZENITH_ASSERT_EQ_FLOAT(xResolved.m_xParams.m_fRoughness, 0.05f, 0.0001f, "Leaf override wins");
	ZENITH_ASSERT_EQ_FLOAT(xResolved.m_xParams.m_fMetallic, 0.5f, 0.0001f, "Mid override flows through");
	ZENITH_ASSERT_EQ_FLOAT(xResolved.m_xParams.m_fSpecular, 0.7f, 0.0001f, "Root value flows through two levels");

	// Root edit propagates through both cache levels.
	pxRoot->SetSpecular(0.2f);
	ZENITH_ASSERT_EQ_FLOAT(pxLeaf->GetResolved().m_xParams.m_fSpecular, 0.2f, 0.0001f,
		"Root edits must reach the leaf through the stamp chain");

	pxLeaf->ClearParent();
	pxMid->ClearParent();
	Zenith_AssetRegistry::UnloadUnused();
	Zenith_AssetRegistry::UnloadUnused();
}

ZENITH_TEST(MaterialAsset, ParentCycleRejected)
{
	auto xhA = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Zenith_MaterialAsset* pxA = xhA.GetDirect();
	auto xhB = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Zenith_MaterialAsset* pxB = xhB.GetDirect();

	ZENITH_ASSERT_FALSE(pxA->SetParent(MaterialHandle(pxA)), "Self-parenting must be rejected");

	ZENITH_ASSERT_TRUE(pxB->SetParent(MaterialHandle(pxA)), "B->A parenting is valid");
	ZENITH_ASSERT_FALSE(pxA->SetParent(MaterialHandle(pxB)), "A->B->A cycle must be rejected");
	ZENITH_ASSERT_FALSE(pxA->HasParent(), "Rejected SetParent must leave the parent unchanged");

	pxB->ClearParent();
	Zenith_AssetRegistry::UnloadUnused();
	Zenith_AssetRegistry::UnloadUnused();
}

ZENITH_TEST(MaterialAsset, EditStampAdvancesOnEveryMutation)
{
	Zenith_MaterialAsset xMat;
	const u_int64 uStamp0 = xMat.GetEditStamp();

	xMat.SetRoughness(0.4f);
	const u_int64 uStamp1 = xMat.GetEditStamp();
	ZENITH_ASSERT_GT(uStamp1, uStamp0, "Param edits must bump the edit stamp");

	xMat.SetTexture(MATERIAL_TEXTURE_EMISSIVE, TextureHandle("game:Textures/e.ztxtr"));
	const u_int64 uStamp2 = xMat.GetEditStamp();
	ZENITH_ASSERT_GT(uStamp2, uStamp1, "Texture edits must bump the edit stamp");

	// The resolved view must reflect the latest edits.
	ZENITH_ASSERT_EQ_FLOAT(xMat.GetResolved().m_xParams.m_fRoughness, 0.4f, 0.0001f,
		"Resolve must reflect edits made after a previous resolve");
	xMat.SetRoughness(0.6f);
	ZENITH_ASSERT_EQ_FLOAT(xMat.GetResolved().m_xParams.m_fRoughness, 0.6f, 0.0001f,
		"Resolve cache must invalidate on the edit stamp");
}
