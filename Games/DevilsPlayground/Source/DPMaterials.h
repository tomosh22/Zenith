#pragma once
/**
 * DPMaterials - DevilsPlayground material loader and helpers.
 *
 * Walks the GAME_ASSETS_DIR/Materials .json files (UE parameter dumps), parses each,
 * builds a Zenith_MaterialAsset, and registers it under the path
 * `game:Materials/<stem>.zmtrl` so other code can resolve a material by name
 * via Zenith_AssetRegistry::GetView<Zenith_MaterialAsset>(...).
 *
 * The JSON dumps look like:
 *
 *   {
 *     "ue_path": "/Game/.../MaterialName.MaterialName",
 *     "kind": "MaterialInstanceConstant",
 *     "scalars":  { "ParamName": 1.0, ... },
 *     "vectors":  { "ParamName": [r,g,b,a], ... },
 *     "textures": { "ParamName": "/Game/.../TextureName.TextureName", ... },
 *     "parent":   "/Game/.../ParentMaterial.ParentMaterial"   // optional
 *   }
 *
 * Mapping to Zenith_MaterialAsset is loose by design (engine default lit +
 * correct material params is the user's accepted constraint):
 *
 *   - "Base Color" / "BaseColor" / "Color" / "SurfaceColor" -> SetBaseColor
 *   - "Roughness"                                            -> SetRoughness
 *   - "Metallic" / "Metalness"                               -> SetMetallic
 *   - "Emissive Color" / "EmissiveColor" / "Emissive"        -> SetEmissiveColor
 *   - "EmissiveIntensity" / "Emissive Intensity"             -> SetEmissiveIntensity
 *   - "Diffuse" / "BaseColor" / "Albedo" texture             -> SetDiffuseTexture
 *   - "Normal" / "NormalMap" texture                          -> SetNormalTexture
 *
 * Anything else is ignored (UE-specific channels Zenith doesn't model).
 */

#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include <string>

namespace DPMaterials
{
	// Walk GAME_ASSETS_DIR/Materials/*.json and register a Zenith_MaterialAsset
	// for each one under game:Materials/<stem>.zmtrl. Builds the procedural
	// "Possessed" tint variants for a couple of base materials and registers
	// them under game:Materials/Possessed_<stem>.zmtrl. Idempotent - safe to
	// call across Editor Stop/Play (returns early if already initialized).
	void Initialize();

	// Reset internal state. Called from Project_Shutdown for a clean reboot.
	void Shutdown();

	// How many materials we authored (counted across both base + tint).
	// Used by tests.
	uint32_t GetRegisteredMaterialCount();

	// Look up the registered material for a given UE-derived stem
	// (e.g. "DevilsPlayground_Assets_Characters_Peasent_lambert13").
	// Returns nullptr if not found. The asset registry pins the asset for the
	// duration of the run, so the caller can hold the pointer.
	Zenith_MaterialAsset* GetMaterialByStem(const std::string& strStem);

	// Resolve the engine path for a UE path like "/Game/Foo/Bar.Bar". Used by
	// other systems (VisualWiring agent's mesh-component attach hook) to map
	// a model's referenced UE material path into the registry.
	std::string UEPathToRegistryPath(const std::string& strUEPath);

	// Build (or return cached) tinted variant of a base material - clones the
	// base and overlays a red emissive multiplier. Used by DPVillager's
	// possession highlight. Returned material is registered under
	// game:Materials/Possessed_<base>.zmtrl. Returns the base material if
	// pxBase is null. Safe to call after Initialize().
	Zenith_MaterialAsset* GetOrCreatePossessedTintFor(Zenith_MaterialAsset* pxBase);

	// Build (or return cached) coloured variant of a base material - clones the
	// base, overrides its base colour, and overlays a low-intensity emissive in
	// the same hue. Used by DPItemBase to colour item pickups by tag (red for
	// Objective, gray for Iron, gold for Key, purple for SkeletonKey). The cache
	// key is (base, RGB-tuple); a unique label is appended to the registry path
	// so multiple tints of the same base coexist. Returns base if pxBase null.
	Zenith_MaterialAsset* GetOrCreateColouredVariant(
		Zenith_MaterialAsset* pxBase,
		const Zenith_Maths::Vector3& xRgb,
		const char* szLabel);

	// Build (or return cached) a full-PBR material authored from scratch (NOT a
	// clone of an existing base) for world entities that want an appropriate
	// look — weathered stone walls, a glowing pentagram, an ember-lit forge,
	// wooden doors/chests, metal contraptions, robed villagers, the priest's
	// dark robe. Cached + pinned under game:Materials/<key>.zmtrl; repeated
	// calls with the same key return the cached asset. xEmissive {0,0,0} with
	// intensity 0 = no emission. Returns the default material if Create fails.
	Zenith_MaterialAsset* GetOrCreateNamedMaterial(
		const char* szKey,
		const Zenith_Maths::Vector3& xBaseColour,
		float fRoughness,
		float fMetallic,
		const Zenith_Maths::Vector3& xEmissive,
		float fEmissiveIntensity);

	// Default lit material for meshes that have no associated material -
	// guaranteed non-null after Initialize() finishes. Registered under
	// game:Materials/__DPDefault.zmtrl.
	Zenith_MaterialAsset* GetDefaultMaterial();
}
