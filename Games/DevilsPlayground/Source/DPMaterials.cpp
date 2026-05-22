#include "Zenith.h"

#include "Source/DPMaterials.h"
#include "Source/DP_Json.h"

#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Maths/Zenith_Maths.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace
{
	using DP_Json::JsonValue;
	using DP_Json::LoadJsonFile;
	using enum DP_Json::JsonType;

	// ---------------------------------------------------------------------------
	// Path helpers
	// ---------------------------------------------------------------------------
	// Convert "/Game/Foo/Bar.Bar" -> "Foo_Bar".
	std::string UEPathToStem(const std::string& strUEPath)
	{
		std::string strRel = strUEPath;
		// Strip leading "/Game/" if present
		const std::string strPrefix = "/Game/";
		if (strRel.size() > strPrefix.size()
			&& strRel.compare(0, strPrefix.size(), strPrefix) == 0)
		{
			strRel = strRel.substr(strPrefix.size());
		}
		// Strip trailing ".X" suffix (where X is the asset name after the last dot)
		size_t uLastDot = strRel.find_last_of('.');
		if (uLastDot != std::string::npos)
		{
			strRel = strRel.substr(0, uLastDot);
		}
		// Replace path separators with underscores
		for (char& c : strRel)
		{
			if (c == '/' || c == '\\') c = '_';
		}
		return strRel;
	}

	// Convert "/Game/.../Texture.Texture" -> "game:Textures/<stem>.ztxtr"
	std::string UEPathToTexturePath(const std::string& strUEPath)
	{
		std::string strStem = UEPathToStem(strUEPath);
		if (strStem.empty()) return std::string();
		return std::string("game:Textures/") + strStem + ".ztxtr";
	}

	// Match a key against any of the candidate names (case-sensitive).
	bool KeyMatchesAny(const std::string& strKey, const char* const* pszCandidates, size_t uCount)
	{
		for (size_t i = 0; i < uCount; ++i)
		{
			if (strKey == pszCandidates[i]) return true;
		}
		return false;
	}

	bool KeyContainsAny(const std::string& strKey, const char* const* pszCandidates, size_t uCount)
	{
		std::string strLower = strKey;
		for (char& c : strLower) c = static_cast<char>(std::tolower(c));
		for (size_t i = 0; i < uCount; ++i)
		{
			std::string strCand = pszCandidates[i];
			for (char& c : strCand) c = static_cast<char>(std::tolower(c));
			if (strLower.find(strCand) != std::string::npos) return true;
		}
		return false;
	}

	// ---------------------------------------------------------------------------
	// Internal state
	// ---------------------------------------------------------------------------
	bool s_bInitialized = false;
	uint32_t s_uRegisteredMaterialCount = 0;
	std::unordered_map<std::string, Zenith_MaterialAsset*>* s_pxStemMap = nullptr;
	std::unordered_map<Zenith_MaterialAsset*, Zenith_MaterialAsset*>* s_pxPossessedMap = nullptr;
	Zenith_MaterialAsset* s_pxDefaultMaterial = nullptr;

	// ---------------------------------------------------------------------------
	// Apply parameters from a parsed JSON value to the material
	// ---------------------------------------------------------------------------
	void ApplyMaterialParameters(Zenith_MaterialAsset& xMat, const JsonValue& xRoot)
	{
		// Vectors (Base Color / SurfaceColor / Emissive Color / etc.)
		if (const JsonValue* pxVectors = xRoot.FindKey("vectors"); pxVectors && pxVectors->m_eType == JSON_OBJECT)
		{
			static const char* const aszBaseColorKeys[] = {
				"Base Color", "BaseColor", "Color", "SurfaceColor",
				"Diffuse Color", "DiffuseColor", "Albedo", "AlbedoColor"
			};
			static const char* const aszEmissiveKeys[] = {
				"Emissive Color", "EmissiveColor", "Emissive", "EmissiveTint"
			};

			for (u_int u = 0; u < pxVectors->m_axObject.GetSize(); ++u)
			{
				const auto& xPair = pxVectors->m_axObject.Get(u);
				const std::string& strKey = xPair.first;
				const JsonValue& xVal = xPair.second;
				if (xVal.m_eType != JSON_ARRAY || xVal.m_axArray.GetSize() < 3) continue;

				float r = static_cast<float>(xVal.m_axArray.Get(0).m_fNumber);
				float g = static_cast<float>(xVal.m_axArray.Get(1).m_fNumber);
				float b = static_cast<float>(xVal.m_axArray.Get(2).m_fNumber);
				float a = (xVal.m_axArray.GetSize() > 3)
					? static_cast<float>(xVal.m_axArray.Get(3).m_fNumber)
					: 1.0f;

				if (KeyMatchesAny(strKey, aszBaseColorKeys, sizeof(aszBaseColorKeys) / sizeof(aszBaseColorKeys[0])))
				{
					xMat.SetBaseColor(Zenith_Maths::Vector4{ r, g, b, a });
				}
				else if (KeyMatchesAny(strKey, aszEmissiveKeys, sizeof(aszEmissiveKeys) / sizeof(aszEmissiveKeys[0])))
				{
					xMat.SetEmissiveColor(Zenith_Maths::Vector3{ r, g, b });
					if (xMat.GetEmissiveIntensity() <= 0.0f)
					{
						xMat.SetEmissiveIntensity(1.0f);
					}
				}
				// All other vectors (TopGridColor, SubGridColor, etc.) are ignored -
				// Zenith default-lit doesn't model them.
			}
		}

		// Scalars (Roughness, Metallic, Emissive Intensity, Alpha Cutoff)
		if (const JsonValue* pxScalars = xRoot.FindKey("scalars"); pxScalars && pxScalars->m_eType == JSON_OBJECT)
		{
			static const char* const aszRoughnessKeys[] = { "Roughness", "Roughness Multiplier", "RoughnessMul" };
			static const char* const aszMetallicKeys[] = { "Metallic", "Metalness", "MetallicMul" };
			static const char* const aszEmissiveIntensityKeys[] = { "EmissiveIntensity", "Emissive Intensity", "EmissiveMultiplier" };
			static const char* const aszAlphaCutoffKeys[] = { "AlphaCutoff", "Alpha Cutoff", "OpacityMaskClipValue" };

			for (u_int u = 0; u < pxScalars->m_axObject.GetSize(); ++u)
			{
				const auto& xPair = pxScalars->m_axObject.Get(u);
				const std::string& strKey = xPair.first;
				const JsonValue& xVal = xPair.second;
				if (xVal.m_eType != JSON_NUMBER) continue;
				float fVal = static_cast<float>(xVal.m_fNumber);

				if (KeyMatchesAny(strKey, aszRoughnessKeys, sizeof(aszRoughnessKeys) / sizeof(aszRoughnessKeys[0])))
				{
					xMat.SetRoughness(fVal);
				}
				else if (KeyMatchesAny(strKey, aszMetallicKeys, sizeof(aszMetallicKeys) / sizeof(aszMetallicKeys[0])))
				{
					xMat.SetMetallic(fVal);
				}
				else if (KeyMatchesAny(strKey, aszEmissiveIntensityKeys, sizeof(aszEmissiveIntensityKeys) / sizeof(aszEmissiveIntensityKeys[0])))
				{
					xMat.SetEmissiveIntensity(fVal);
				}
				else if (KeyMatchesAny(strKey, aszAlphaCutoffKeys, sizeof(aszAlphaCutoffKeys) / sizeof(aszAlphaCutoffKeys[0])))
				{
					xMat.SetAlphaCutoff(fVal);
				}
				// RefractionDepthBias / Grid Size / etc. are ignored.
			}
		}

		// Textures (Diffuse / Normal / RoughnessMetallic / Emissive / Occlusion)
		if (const JsonValue* pxTextures = xRoot.FindKey("textures"); pxTextures && pxTextures->m_eType == JSON_OBJECT)
		{
			static const char* const aszDiffuseKeys[] = { "diffuse", "basecolor", "albedo", "color", "tex" };
			static const char* const aszNormalKeys[] = { "normal", "bump" };
			static const char* const aszRoughMetalKeys[] = { "roughnessmetallic", "metallicroughness", "roughness", "metallic" };
			static const char* const aszEmissiveKeys[] = { "emissive" };
			static const char* const aszOcclusionKeys[] = { "occlusion", "ao", "ambientocclusion" };

			for (u_int u = 0; u < pxTextures->m_axObject.GetSize(); ++u)
			{
				const auto& xPair = pxTextures->m_axObject.Get(u);
				const std::string& strKey = xPair.first;
				const JsonValue& xVal = xPair.second;
				if (xVal.m_eType != JSON_STRING) continue;
				const std::string& strUEPath = xVal.m_strString;
				if (strUEPath.empty()) continue;

				std::string strRegPath = UEPathToTexturePath(strUEPath);
				if (strRegPath.empty()) continue;

				// Verify the .ztxtr file actually exists - if the import didn't
				// produce it, leave the slot at default rather than failing the
				// material load.
				std::string strAbs = Zenith_AssetRegistry::ResolvePath(strRegPath);
				if (!std::filesystem::exists(strAbs)) continue;

				if (KeyContainsAny(strKey, aszDiffuseKeys, sizeof(aszDiffuseKeys) / sizeof(aszDiffuseKeys[0])))
				{
					xMat.SetDiffuseTexture(TextureHandle(strRegPath));
				}
				else if (KeyContainsAny(strKey, aszNormalKeys, sizeof(aszNormalKeys) / sizeof(aszNormalKeys[0])))
				{
					xMat.SetNormalTexture(TextureHandle(strRegPath));
				}
				else if (KeyContainsAny(strKey, aszRoughMetalKeys, sizeof(aszRoughMetalKeys) / sizeof(aszRoughMetalKeys[0])))
				{
					xMat.SetRoughnessMetallicTexture(TextureHandle(strRegPath));
				}
				else if (KeyContainsAny(strKey, aszEmissiveKeys, sizeof(aszEmissiveKeys) / sizeof(aszEmissiveKeys[0])))
				{
					xMat.SetEmissiveTexture(TextureHandle(strRegPath));
				}
				else if (KeyContainsAny(strKey, aszOcclusionKeys, sizeof(aszOcclusionKeys) / sizeof(aszOcclusionKeys[0])))
				{
					xMat.SetOcclusionTexture(TextureHandle(strRegPath));
				}
				// Unknown texture slot - default to diffuse if no diffuse set yet.
				else if (xMat.GetDiffuseTexturePath().empty())
				{
					xMat.SetDiffuseTexture(TextureHandle(strRegPath));
				}
			}
		}
	}

	// Build a single material from a JSON file and register it under
	// game:Materials/<stem>.zmtrl. Returns the registered material or nullptr.
	Zenith_MaterialAsset* AuthorMaterialFromJson(const std::filesystem::path& xJsonPath)
	{
		JsonValue xRoot;
		if (!LoadJsonFile(xJsonPath, xRoot) || xRoot.m_eType != JSON_OBJECT)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "DPMaterials: failed to parse %s",
				xJsonPath.string().c_str());
			return nullptr;
		}

		std::string strStem = xJsonPath.stem().string();
		std::string strRegPath = std::string("game:Materials/") + strStem + ".zmtrl";

		// If the same path was already registered (e.g. defaults seeded earlier),
		// reuse the existing asset to keep the cache canonical.
		Zenith_MaterialAsset* pxMat = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>(strRegPath);
		if (!pxMat)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "DPMaterials: AssetRegistry::Create failed for %s",
				strRegPath.c_str());
			return nullptr;
		}

		pxMat->SetName(strStem);

		// Default UE-style PBR values until overridden by JSON
		pxMat->SetBaseColor(Zenith_Maths::Vector4{ 0.7f, 0.7f, 0.7f, 1.0f });
		pxMat->SetRoughness(0.5f);
		pxMat->SetMetallic(0.0f);

		ApplyMaterialParameters(*pxMat, xRoot);

		// Pin the asset so Zenith_AssetRegistry::UnloadUnused (called during
		// scene swaps) can't free it. The DPMaterials registry owns one
		// reference per material for the entire process lifetime.
		pxMat->AddRef();

		return pxMat;
	}

	// Walk every .json file under the materials dir and build Zenith materials.
	void AuthorAllMaterials(const std::filesystem::path& xMaterialsDir)
	{
		if (!std::filesystem::exists(xMaterialsDir))
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "DPMaterials: directory not found: %s",
				xMaterialsDir.string().c_str());
			return;
		}

		std::error_code xEc;
		for (auto& xEntry : std::filesystem::directory_iterator(xMaterialsDir, xEc))
		{
			if (xEc) break;
			if (!xEntry.is_regular_file()) continue;
			const auto& xPath = xEntry.path();
			if (xPath.extension() != ".json") continue;

			Zenith_MaterialAsset* pxMat = AuthorMaterialFromJson(xPath);
			if (!pxMat) continue;

			std::string strStem = xPath.stem().string();
			(*s_pxStemMap)[strStem] = pxMat;
			++s_uRegisteredMaterialCount;
		}
	}
}

// ============================================================================
// Public API
// ============================================================================
namespace DPMaterials
{
	void Initialize()
	{
		if (s_bInitialized) return;

		s_pxStemMap = new std::unordered_map<std::string, Zenith_MaterialAsset*>();
		s_pxPossessedMap = new std::unordered_map<Zenith_MaterialAsset*, Zenith_MaterialAsset*>();
		s_uRegisteredMaterialCount = 0;

		// Build the canonical default material first so it has a stable handle
		// for fallback callers.
		s_pxDefaultMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>(
			"game:Materials/__DPDefault.zmtrl");
		if (s_pxDefaultMaterial)
		{
			s_pxDefaultMaterial->SetName("__DPDefault");
			s_pxDefaultMaterial->SetBaseColor(Zenith_Maths::Vector4{ 0.6f, 0.6f, 0.6f, 1.0f });
			s_pxDefaultMaterial->SetRoughness(0.7f);
			s_pxDefaultMaterial->SetMetallic(0.0f);
			s_pxDefaultMaterial->AddRef();
			++s_uRegisteredMaterialCount;
		}

		std::filesystem::path xMaterialsDir = std::filesystem::path(GAME_ASSETS_DIR) / "Materials";
		AuthorAllMaterials(xMaterialsDir);

		Zenith_Log(LOG_CATEGORY_ASSET,
			"DPMaterials: registered %u materials from %s",
			s_uRegisteredMaterialCount, xMaterialsDir.string().c_str());

		s_bInitialized = true;
	}

	void Shutdown()
	{
		if (!s_bInitialized) return;

		// Release the pin we took during Initialize so UnloadUnused can free
		// the materials at engine teardown. Do this before zeroing the maps.
		if (s_pxStemMap)
		{
			for (auto& xPair : *s_pxStemMap)
			{
				if (xPair.second) xPair.second->Release();
			}
			delete s_pxStemMap;
			s_pxStemMap = nullptr;
		}
		if (s_pxPossessedMap)
		{
			for (auto& xPair : *s_pxPossessedMap)
			{
				if (xPair.second) xPair.second->Release();
			}
			delete s_pxPossessedMap;
			s_pxPossessedMap = nullptr;
		}
		if (s_pxDefaultMaterial)
		{
			s_pxDefaultMaterial->Release();
			s_pxDefaultMaterial = nullptr;
		}
		s_uRegisteredMaterialCount = 0;
		s_bInitialized = false;
	}

	uint32_t GetRegisteredMaterialCount()
	{
		return s_uRegisteredMaterialCount;
	}

	Zenith_MaterialAsset* GetMaterialByStem(const std::string& strStem)
	{
		if (!s_pxStemMap) return nullptr;
		auto it = s_pxStemMap->find(strStem);
		return (it != s_pxStemMap->end()) ? it->second : nullptr;
	}

	std::string UEPathToRegistryPath(const std::string& strUEPath)
	{
		std::string strStem = UEPathToStem(strUEPath);
		if (strStem.empty()) return std::string();
		return std::string("game:Materials/") + strStem + ".zmtrl";
	}

	Zenith_MaterialAsset* GetDefaultMaterial()
	{
		return s_pxDefaultMaterial;
	}

	Zenith_MaterialAsset* GetOrCreatePossessedTintFor(Zenith_MaterialAsset* pxBase)
	{
		if (!pxBase) return nullptr;
		if (!s_pxPossessedMap) return pxBase;

		auto it = s_pxPossessedMap->find(pxBase);
		if (it != s_pxPossessedMap->end()) return it->second;

		std::string strBaseName = pxBase->GetName();
		std::string strRegPath = std::string("game:Materials/Possessed_") + strBaseName + ".zmtrl";

		Zenith_MaterialAsset* pxTint = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>(strRegPath);
		if (!pxTint) return pxBase;

		pxTint->SetName(std::string("Possessed_") + strBaseName);

		// Copy base properties verbatim, then overlay a red emissive multiplier
		// to make the possessed villager glow.
		pxTint->SetBaseColor(pxBase->GetBaseColor());
		pxTint->SetMetallic(pxBase->GetMetallic());
		pxTint->SetRoughness(pxBase->GetRoughness());
		pxTint->SetTransparent(pxBase->IsTransparent());
		pxTint->SetAlphaCutoff(pxBase->GetAlphaCutoff());
		pxTint->SetUVTiling(pxBase->GetUVTiling());
		pxTint->SetUVOffset(pxBase->GetUVOffset());
		pxTint->SetOcclusionStrength(pxBase->GetOcclusionStrength());
		pxTint->SetTwoSided(pxBase->IsTwoSided());
		pxTint->SetUnlit(pxBase->IsUnlit());

		// Preserve textures
		pxTint->SetDiffuseTexture(pxBase->GetDiffuseTextureHandle());
		pxTint->SetNormalTexture(pxBase->GetNormalTextureHandle());
		pxTint->SetRoughnessMetallicTexture(pxBase->GetRoughnessMetallicTextureHandle());
		pxTint->SetOcclusionTexture(pxBase->GetOcclusionTextureHandle());
		pxTint->SetEmissiveTexture(pxBase->GetEmissiveTextureHandle());

		// Possessed tint: bright red glow.
		pxTint->SetEmissiveColor(Zenith_Maths::Vector3{ 1.0f, 0.15f, 0.15f });
		pxTint->SetEmissiveIntensity(2.5f);

		// Pin: see comment in AuthorMaterialFromJson about UnloadUnused.
		pxTint->AddRef();

		(*s_pxPossessedMap)[pxBase] = pxTint;
		++s_uRegisteredMaterialCount;
		return pxTint;
	}

	Zenith_MaterialAsset* GetOrCreateColouredVariant(
		Zenith_MaterialAsset* pxBase,
		const Zenith_Maths::Vector3& xRgb,
		const char* szLabel)
	{
		if (!pxBase) return nullptr;
		// We don't strictly cache by colour-tuple since labels are stable and small;
		// per-label caching is sufficient. Build a registry path and consult
		// AssetRegistry::Get *before* falling through to Create.
		const char* szSafeLabel = (szLabel && szLabel[0] != '\0') ? szLabel : "Tint";
		std::string strBaseName = pxBase->GetName();
		std::string strRegPath = std::string("game:Materials/")
			+ szSafeLabel + "_" + strBaseName + ".zmtrl";

		// Zenith_AssetRegistry::Create overwrites the registry entry for
		// strRegPath unconditionally — calling it a second time for the
		// same path leaks the previous Zenith_MaterialAsset (still pinned
		// by our AddRef below) and silently invalidates any model slots
		// still pointing at it. Probe the registry first so repeated
		// (base, label) lookups return the cached variant instead.
		if (Zenith_MaterialAsset* pxExisting =
			Zenith_AssetRegistry::Get<Zenith_MaterialAsset>(strRegPath))
		{
			return pxExisting;
		}

		Zenith_MaterialAsset* pxTint = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>(strRegPath);
		if (!pxTint) return pxBase;

		pxTint->SetName(std::string(szSafeLabel) + "_" + strBaseName);

		// Copy structural properties from the base.
		pxTint->SetMetallic(pxBase->GetMetallic());
		pxTint->SetRoughness(pxBase->GetRoughness());
		pxTint->SetTransparent(pxBase->IsTransparent());
		pxTint->SetAlphaCutoff(pxBase->GetAlphaCutoff());
		pxTint->SetUVTiling(pxBase->GetUVTiling());
		pxTint->SetUVOffset(pxBase->GetUVOffset());
		pxTint->SetOcclusionStrength(pxBase->GetOcclusionStrength());
		pxTint->SetTwoSided(pxBase->IsTwoSided());
		pxTint->SetUnlit(pxBase->IsUnlit());

		// Preserve textures so albedo maps still register, but the base-colour
		// override below acts as a tint multiplier.
		pxTint->SetDiffuseTexture(pxBase->GetDiffuseTextureHandle());
		pxTint->SetNormalTexture(pxBase->GetNormalTextureHandle());
		pxTint->SetRoughnessMetallicTexture(pxBase->GetRoughnessMetallicTextureHandle());
		pxTint->SetOcclusionTexture(pxBase->GetOcclusionTextureHandle());
		pxTint->SetEmissiveTexture(pxBase->GetEmissiveTextureHandle());

		// Strong base colour and a low-intensity emissive in the same hue so
		// items are visibly distinguishable in the lit scene.
		pxTint->SetBaseColor(Zenith_Maths::Vector4{ xRgb.x, xRgb.y, xRgb.z, 1.0f });
		pxTint->SetEmissiveColor(xRgb);
		pxTint->SetEmissiveIntensity(1.0f);

		// Pin so UnloadUnused can't free us mid-run.
		pxTint->AddRef();

		++s_uRegisteredMaterialCount;
		return pxTint;
	}
}
