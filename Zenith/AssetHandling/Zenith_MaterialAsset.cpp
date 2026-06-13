#include "Zenith.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include <filesystem>

// Static default textures (pinned via handle).
TextureHandle Zenith_MaterialAsset::s_xDefaultWhite;
TextureHandle Zenith_MaterialAsset::s_xDefaultNormal;

//--------------------------------------------------------------------------
// Construction / Destruction
//--------------------------------------------------------------------------

Zenith_MaterialAsset::Zenith_MaterialAsset()
	: m_strName("New Material")
{
}

Zenith_MaterialAsset::~Zenith_MaterialAsset()
{
	// Handles will clean up their refs automatically
}

//--------------------------------------------------------------------------
// Loading / Saving
//--------------------------------------------------------------------------

Zenith_Status Zenith_MaterialAsset::LoadFromFile(const std::string& strPath)
{
	if (!std::filesystem::exists(strPath))
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "Material file not found: %s", strPath.c_str());
		return Zenith_ErrorCode::FILE_NOT_FOUND;
	}

	Zenith_DataStream xStream;
	xStream.ReadFromFile(strPath.c_str());

	ReadFromDataStream(xStream);

	m_strPath = strPath;
	m_bDirty = false;

	Zenith_Log(LOG_CATEGORY_ASSET, "Loaded material: %s (name: %s)", strPath.c_str(), m_strName.c_str());
	return true;
}

bool Zenith_MaterialAsset::SaveToFile(const std::string& strPath)
{
	Zenith_DataStream xStream;
	WriteToDataStream(xStream);

	xStream.WriteToFile(strPath.c_str());

	m_strPath = strPath;
	m_bDirty = false;

	Zenith_Log(LOG_CATEGORY_ASSET, "Saved material to: %s", strPath.c_str());
	return true;
}

bool Zenith_MaterialAsset::Reload()
{
	if (m_strPath.empty())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "Cannot reload material with empty path");
		return false;
	}

	return LoadFromFile(m_strPath).IsOk();
}

void Zenith_MaterialAsset::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// File version - always written as the current version (v5).
	uint32_t uVersion = ZENITH_MATERIAL_FILE_VERSION;
	xStream << uVersion;

	// Material identity
	xStream << m_strName;

	// Parameter block - explicit field-by-field order (never the whole POD:
	// struct padding must not leak into the file format).
	xStream << static_cast<uint32_t>(m_xParams.m_eBlendMode);
	xStream << static_cast<uint32_t>(m_xParams.m_eShadingModel);
	xStream << m_xParams.m_bTwoSided;
	xStream << m_xParams.m_fAlphaCutoff;

	xStream << m_xParams.m_xBaseColor.x;
	xStream << m_xParams.m_xBaseColor.y;
	xStream << m_xParams.m_xBaseColor.z;
	xStream << m_xParams.m_xBaseColor.w;
	xStream << m_xParams.m_fMetallic;
	xStream << m_xParams.m_fRoughness;
	xStream << m_xParams.m_fSpecular;

	xStream << m_xParams.m_fNormalStrength;
	xStream << m_xParams.m_fHeightScale;
	xStream << m_xParams.m_fPOMMinSteps;
	xStream << m_xParams.m_fPOMMaxSteps;
	xStream << m_xParams.m_xDetailTiling.x;
	xStream << m_xParams.m_xDetailTiling.y;
	xStream << m_xParams.m_fDetailNormalStrength;
	xStream << m_xParams.m_fDetailAlbedoStrength;

	xStream << m_xParams.m_xEmissiveColor.x;
	xStream << m_xParams.m_xEmissiveColor.y;
	xStream << m_xParams.m_xEmissiveColor.z;
	xStream << m_xParams.m_fEmissiveIntensity;

	xStream << m_xParams.m_fOcclusionStrength;

	xStream << m_xParams.m_xUVTiling.x;
	xStream << m_xParams.m_xUVTiling.y;
	xStream << m_xParams.m_xUVOffset.x;
	xStream << m_xParams.m_xUVOffset.y;

	xStream << m_xParams.m_fClearCoatStrength;
	xStream << m_xParams.m_fClearCoatRoughness;

	// Instance state
	xStream << Zenith_AssetRegistry::NormalizeAssetPath(m_xParentMaterial.GetPath());
	xStream << m_uOverrideMask;

	// Texture slots (path-based; procedural textures serialize as empty)
	for (u_int u = 0; u < MATERIAL_TEXTURE_SLOT_COUNT; u++)
	{
		xStream << Zenith_AssetRegistry::NormalizeAssetPath(m_axTextures[u].GetPath());
	}
}

void Zenith_MaterialAsset::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// File version
	uint32_t uVersion;
	xStream >> uVersion;

	if (uVersion > ZENITH_MATERIAL_FILE_VERSION)
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "Unsupported material version %u (max: %u)",
			uVersion, ZENITH_MATERIAL_FILE_VERSION);
		return;
	}

	// Reset to defaults first so reloading an existing asset never keeps
	// stale state a given file version doesn't carry.
	m_xParams = Zenith_MaterialParams();
	for (u_int u = 0; u < MATERIAL_TEXTURE_SLOT_COUNT; u++)
	{
		m_axTextures[u].Clear();
	}
	m_xParentMaterial.Clear();
	m_uOverrideMask = 0;
	m_bReportedParentError = false;

	// Material identity
	xStream >> m_strName;

	if (uVersion >= 5)
	{
		uint32_t uBlendMode = 0;
		uint32_t uShadingModel = 0;
		xStream >> uBlendMode;
		xStream >> uShadingModel;
		m_xParams.m_eBlendMode = (uBlendMode < MATERIAL_BLEND_COUNT) ? static_cast<MaterialBlendMode>(uBlendMode) : MATERIAL_BLEND_OPAQUE;
		m_xParams.m_eShadingModel = (uShadingModel < MATERIAL_SHADING_COUNT) ? static_cast<MaterialShadingModel>(uShadingModel) : MATERIAL_SHADING_DEFAULT_LIT;
		xStream >> m_xParams.m_bTwoSided;
		xStream >> m_xParams.m_fAlphaCutoff;

		xStream >> m_xParams.m_xBaseColor.x;
		xStream >> m_xParams.m_xBaseColor.y;
		xStream >> m_xParams.m_xBaseColor.z;
		xStream >> m_xParams.m_xBaseColor.w;
		xStream >> m_xParams.m_fMetallic;
		xStream >> m_xParams.m_fRoughness;
		xStream >> m_xParams.m_fSpecular;

		xStream >> m_xParams.m_fNormalStrength;
		xStream >> m_xParams.m_fHeightScale;
		xStream >> m_xParams.m_fPOMMinSteps;
		xStream >> m_xParams.m_fPOMMaxSteps;
		xStream >> m_xParams.m_xDetailTiling.x;
		xStream >> m_xParams.m_xDetailTiling.y;
		xStream >> m_xParams.m_fDetailNormalStrength;
		xStream >> m_xParams.m_fDetailAlbedoStrength;

		xStream >> m_xParams.m_xEmissiveColor.x;
		xStream >> m_xParams.m_xEmissiveColor.y;
		xStream >> m_xParams.m_xEmissiveColor.z;
		xStream >> m_xParams.m_fEmissiveIntensity;

		xStream >> m_xParams.m_fOcclusionStrength;

		xStream >> m_xParams.m_xUVTiling.x;
		xStream >> m_xParams.m_xUVTiling.y;
		xStream >> m_xParams.m_xUVOffset.x;
		xStream >> m_xParams.m_xUVOffset.y;

		xStream >> m_xParams.m_fClearCoatStrength;
		xStream >> m_xParams.m_fClearCoatRoughness;

		std::string strParentPath;
		xStream >> strParentPath;
		if (!strParentPath.empty())
		{
			m_xParentMaterial.SetPath(Zenith_AssetRegistry::NormalizeAssetPath(strParentPath));
		}
		xStream >> m_uOverrideMask;

		std::string strTexturePath;
		for (u_int u = 0; u < MATERIAL_TEXTURE_SLOT_COUNT; u++)
		{
			xStream >> strTexturePath;
			if (!strTexturePath.empty())
			{
				m_axTextures[u].SetPath(Zenith_AssetRegistry::NormalizeAssetPath(strTexturePath));
			}
		}
	}
	else
	{
		// ---- Legacy v2-v4 layout ----
		Zenith_Maths::Vector4 xBaseColor;
		xStream >> xBaseColor.x;
		xStream >> xBaseColor.y;
		xStream >> xBaseColor.z;
		xStream >> xBaseColor.w;
		m_xParams.m_xBaseColor = xBaseColor;

		xStream >> m_xParams.m_fMetallic;
		xStream >> m_xParams.m_fRoughness;

		xStream >> m_xParams.m_xEmissiveColor.x;
		xStream >> m_xParams.m_xEmissiveColor.y;
		xStream >> m_xParams.m_xEmissiveColor.z;
		xStream >> m_xParams.m_fEmissiveIntensity;

		// v4 semantics: the alpha-test discard was ALWAYS active, so legacy
		// non-transparent materials map to Masked (not Opaque) to render
		// identically through the new blend-mode paths.
		bool bTransparent = false;
		xStream >> bTransparent;
		m_xParams.m_eBlendMode = bTransparent ? MATERIAL_BLEND_TRANSLUCENT : MATERIAL_BLEND_MASKED;
		xStream >> m_xParams.m_fAlphaCutoff;

		if (uVersion >= 3)
		{
			xStream >> m_xParams.m_xUVTiling.x;
			xStream >> m_xParams.m_xUVTiling.y;
			xStream >> m_xParams.m_xUVOffset.x;
			xStream >> m_xParams.m_xUVOffset.y;

			xStream >> m_xParams.m_fOcclusionStrength;

			xStream >> m_xParams.m_bTwoSided;
			bool bUnlit = false;
			xStream >> bUnlit;
			m_xParams.m_eShadingModel = bUnlit ? MATERIAL_SHADING_UNLIT : MATERIAL_SHADING_DEFAULT_LIT;
		}

		// Texture references
		if (uVersion >= 4)
		{
			// Version 4: direct paths for the original 5 slots.
			static const MaterialTextureSlot aeLegacySlots[] =
			{
				MATERIAL_TEXTURE_BASE_COLOR,
				MATERIAL_TEXTURE_NORMAL,
				MATERIAL_TEXTURE_ROUGHNESS_METALLIC,
				MATERIAL_TEXTURE_OCCLUSION,
				MATERIAL_TEXTURE_EMISSIVE,
			};

			std::string strPath;
			for (u_int u = 0; u < sizeof(aeLegacySlots) / sizeof(aeLegacySlots[0]); u++)
			{
				xStream >> strPath;
				if (!strPath.empty())
				{
					m_axTextures[aeLegacySlots[u]].SetPath(Zenith_AssetRegistry::NormalizeAssetPath(strPath));
				}
			}
		}
		else if (uVersion >= 2)
		{
			// Version 2-3: GUID-based (old format) - no longer supported
			// Old materials need to be re-exported
			Zenith_Error(LOG_CATEGORY_ASSET, "Material %s uses old GUID format (v%u). Please re-export.",
				m_strName.c_str(), uVersion);
		}
	}

	// Invalidate any cached resolve - the whole asset just changed.
	MarkEdited();
}

//--------------------------------------------------------------------------
// Texture Slots
//--------------------------------------------------------------------------

void Zenith_MaterialAsset::SetTexture(MaterialTextureSlot eSlot, TextureHandle xHandle)
{
	Zenith_Assert(eSlot < MATERIAL_TEXTURE_SLOT_COUNT, "Invalid material texture slot %u", eSlot);
	m_axTextures[eSlot] = std::move(xHandle);
	if (HasParent())
	{
		m_uOverrideMask |= (1ull << (uMATERIAL_TEXTURE_OVERRIDE_BIT_BASE + eSlot));
	}
	MarkEdited();
}

const TextureHandle& Zenith_MaterialAsset::GetTextureHandle(MaterialTextureSlot eSlot) const
{
	Zenith_Assert(eSlot < MATERIAL_TEXTURE_SLOT_COUNT, "Invalid material texture slot %u", eSlot);
	return m_axTextures[eSlot];
}

Zenith_TextureAsset* Zenith_MaterialAsset::GetTexture(MaterialTextureSlot eSlot)
{
	Zenith_Assert(eSlot < MATERIAL_TEXTURE_SLOT_COUNT, "Invalid material texture slot %u", eSlot);
	Zenith_TextureAsset* pTex = m_axTextures[eSlot].Resolve();
	return pTex ? pTex : GetDefaultTextureForSlot(eSlot);
}

Zenith_TextureAsset* Zenith_MaterialAsset::GetResolvedTexture(MaterialTextureSlot eSlot)
{
	Zenith_Assert(eSlot < MATERIAL_TEXTURE_SLOT_COUNT, "Invalid material texture slot %u", eSlot);
	const Zenith_MaterialResolved& xResolved = GetResolved();
	Zenith_TextureAsset* pTex = xResolved.m_apxTextures[eSlot]->Resolve();
	return pTex ? pTex : GetDefaultTextureForSlot(eSlot);
}

//--------------------------------------------------------------------------
// Material Instances
//--------------------------------------------------------------------------

bool Zenith_MaterialAsset::SetParent(const MaterialHandle& xParent)
{
	m_bReportedParentError = false;

	if (Zenith_MaterialAsset* pxCandidate = xParent.Resolve())
	{
		if (pxCandidate == this)
		{
			Zenith_Error(LOG_CATEGORY_ASSET, "Material '%s': cannot parent a material to itself", m_strName.c_str());
			return false;
		}

		// Walk the candidate's chain looking for ourselves (cycle) or a chain
		// that is already at the depth limit. The walk itself is depth-bounded
		// so pre-existing cycles among other materials cannot hang us.
		u_int uDepth = 1;
		Zenith_MaterialAsset* pxWalk = pxCandidate->m_xParentMaterial.Resolve();
		while (pxWalk != nullptr)
		{
			if (pxWalk == this)
			{
				Zenith_Error(LOG_CATEGORY_ASSET, "Material '%s': rejected parent '%s' - would create a cycle",
					m_strName.c_str(), pxCandidate->GetName().c_str());
				return false;
			}
			if (++uDepth >= uMATERIAL_MAX_PARENT_DEPTH)
			{
				Zenith_Error(LOG_CATEGORY_ASSET, "Material '%s': rejected parent '%s' - chain exceeds max depth %u",
					m_strName.c_str(), pxCandidate->GetName().c_str(), uMATERIAL_MAX_PARENT_DEPTH);
				return false;
			}
			pxWalk = pxWalk->m_xParentMaterial.Resolve();
		}
	}

	m_xParentMaterial = xParent;
	MarkEdited();
	return true;
}

void Zenith_MaterialAsset::ClearParent()
{
	m_xParentMaterial.Clear();
	m_bReportedParentError = false;
	MarkEdited();
}

void Zenith_MaterialAsset::SetOverride(u_int uBit, bool bOverridden)
{
	Zenith_Assert(uBit < 64, "Material override bit %u out of range", uBit);
	if (bOverridden)
	{
		m_uOverrideMask |= (1ull << uBit);
	}
	else
	{
		m_uOverrideMask &= ~(1ull << uBit);
	}
	MarkEdited();
}

Zenith_MaterialAsset* Zenith_MaterialAsset::ResolveParentChecked()
{
	if (!m_xParentMaterial)
	{
		return nullptr;
	}

	Zenith_MaterialAsset* pxParent = m_xParentMaterial.Resolve();
	if (pxParent == nullptr)
	{
		if (!m_bReportedParentError)
		{
			Zenith_Error(LOG_CATEGORY_ASSET, "Material '%s': parent '%s' failed to load - rendering with local values",
				m_strName.c_str(), m_xParentMaterial.GetPath().c_str());
			m_bReportedParentError = true;
		}
		return nullptr;
	}

	// Validate the chain (cycles can arrive from disk even though SetParent
	// rejects them - e.g. two materials saved pointing at each other).
	u_int uDepth = 0;
	Zenith_MaterialAsset* pxWalk = pxParent;
	while (pxWalk != nullptr)
	{
		if (pxWalk == this || ++uDepth > uMATERIAL_MAX_PARENT_DEPTH)
		{
			if (!m_bReportedParentError)
			{
				Zenith_Error(LOG_CATEGORY_ASSET, "Material '%s': parent chain is cyclic or deeper than %u - ignoring parent",
					m_strName.c_str(), uMATERIAL_MAX_PARENT_DEPTH);
				m_bReportedParentError = true;
			}
			return nullptr;
		}
		pxWalk = pxWalk->m_xParentMaterial ? pxWalk->m_xParentMaterial.Resolve() : nullptr;
	}

	return pxParent;
}

const Zenith_MaterialResolved& Zenith_MaterialAsset::GetResolved()
{
	Zenith_MaterialAsset* pxParent = ResolveParentChecked();
	const bool bSelfClean = m_bResolvedValid && (m_uResolvedSelfStamp == m_uEditStamp);

	if (pxParent == nullptr)
	{
		if (bSelfClean && m_pxResolvedParent == nullptr)
		{
			return m_xResolved;
		}

		m_xResolved.m_xParams = m_xParams;
		for (u_int u = 0; u < MATERIAL_TEXTURE_SLOT_COUNT; u++)
		{
			m_xResolved.m_apxTextures[u] = &m_axTextures[u];
		}
		m_pxResolvedParent = nullptr;
		m_uResolvedParentStamp = 0;
	}
	else
	{
		// Revalidates the parent's own cache (and transitively the chain).
		const Zenith_MaterialResolved& xParentResolved = pxParent->GetResolved();

		if (bSelfClean && m_pxResolvedParent == pxParent && m_uResolvedParentStamp == pxParent->m_uResolveStamp)
		{
			return m_xResolved;
		}

		// Start from the parent's flattened view, overlay our overridden values.
		m_xResolved.m_xParams = xParentResolved.m_xParams;
		for (u_int u = 0; u < MATERIAL_PARAM_COUNT; u++)
		{
			const MaterialParamID eID = static_cast<MaterialParamID>(u);
			if (HasParamOverride(eID))
			{
				Zenith_MaterialParamTable::CopyParam(m_xResolved.m_xParams, m_xParams, eID);
			}
		}
		for (u_int u = 0; u < MATERIAL_TEXTURE_SLOT_COUNT; u++)
		{
			const MaterialTextureSlot eSlot = static_cast<MaterialTextureSlot>(u);
			m_xResolved.m_apxTextures[u] = HasTextureOverride(eSlot) ? &m_axTextures[u] : xParentResolved.m_apxTextures[u];
		}
		m_pxResolvedParent = pxParent;
		m_uResolvedParentStamp = pxParent->m_uResolveStamp;
	}

	m_uResolvedSelfStamp = m_uEditStamp;
	m_bResolvedValid = true;
	++m_uResolveStamp;
	return m_xResolved;
}

//--------------------------------------------------------------------------
// Default Textures
//--------------------------------------------------------------------------

Zenith_TextureAsset* Zenith_MaterialAsset::GetDefaultWhiteTexture()
{
	return s_xDefaultWhite.GetDirect();
}

Zenith_TextureAsset* Zenith_MaterialAsset::GetDefaultNormalTexture()
{
	return s_xDefaultNormal.GetDirect();
}

Zenith_TextureAsset* Zenith_MaterialAsset::GetDefaultTextureForSlot(MaterialTextureSlot eSlot)
{
	const Zenith_MaterialTextureSlotDesc& xDesc = Zenith_MaterialParamTable::GetTextureSlotDesc(eSlot);
	switch (xDesc.m_eDefault)
	{
		case MATERIAL_TEXTURE_DEFAULT_FLAT_NORMAL: return GetDefaultNormalTexture();
		case MATERIAL_TEXTURE_DEFAULT_WHITE:
		default:                                   return GetDefaultWhiteTexture();
	}
}

void Zenith_MaterialAsset::InitializeDefaults()
{
	// Create default white texture (1x1 white pixel) — pinned via handle.
	if (Zenith_TextureAsset* pxWhite = Zenith_AssetRegistry::Create<Zenith_TextureAsset>())
	{
		uint32_t uWhite = 0xFFFFFFFF;
		Flux_SurfaceInfo xInfo;
		xInfo.m_uWidth = 1;
		xInfo.m_uHeight = 1;
		xInfo.m_uNumMips = 1;
		xInfo.m_eFormat = TEXTURE_FORMAT_RGBA8_UNORM;
		xInfo.m_eTextureType = TEXTURE_TYPE_2D;
		pxWhite->CreateFromData(&uWhite, xInfo, false);
		s_xDefaultWhite.Set(pxWhite);
	}

	// Create default normal texture (1x1 flat normal: 0.5, 0.5, 1.0) — pinned.
	if (Zenith_TextureAsset* pxNormal = Zenith_AssetRegistry::Create<Zenith_TextureAsset>())
	{
		uint32_t uNormal = 0xFFFF8080; // RGBA: 0.5, 0.5, 1.0, 1.0 in 8-bit (128, 128, 255, 255)
		Flux_SurfaceInfo xInfo;
		xInfo.m_uWidth = 1;
		xInfo.m_uHeight = 1;
		xInfo.m_uNumMips = 1;
		xInfo.m_eFormat = TEXTURE_FORMAT_RGBA8_UNORM;
		xInfo.m_eTextureType = TEXTURE_TYPE_2D;
		pxNormal->CreateFromData(&uNormal, xInfo, false);
		s_xDefaultNormal.Set(pxNormal);
	}

	Zenith_Log(LOG_CATEGORY_ASSET, "Material default textures initialized");
}

void Zenith_MaterialAsset::ShutdownDefaults()
{
	// No-op kept for ABI compatibility — handle cleanup is done in ReleaseDefaults
	// during Flux::ReleaseAssetReferences. Subsystem Shutdown runs too late
	// (after Zenith_AssetRegistry::Shutdown).
	Zenith_Log(LOG_CATEGORY_ASSET, "Material default textures shut down (handles cleared via ReleaseDefaults)");
}

void Zenith_MaterialAsset::ReleaseDefaults()
{
	s_xDefaultWhite.Clear();
	s_xDefaultNormal.Clear();
}

#include "AssetHandling/Zenith_MaterialAsset.Tests.inl"
