import unreal

paths = [
	"/Game/DevilsPlayground/BPs/Characters/BP_Pleb.BP_Pleb",
	"/Game/DevilsPlayground/BPs/BP_PlayerController.BP_PlayerController",
	"/Game/DevilsPlayground/BPs/BP_GameMode.BP_GameMode",
]
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
for path in paths:
	unreal.log("CODEX_DEBUG_PATH " + path)
	asset = unreal.EditorAssetLibrary.load_asset(path)
	unreal.log("  asset=" + str(asset))
	unreal.log("  class=" + asset.get_class().get_name())
	for attr in ["generated_class", "GeneratedClass", "parent_class", "ParentClass", "skeleton_generated_class"]:
		try:
			unreal.log("  prop " + attr + "=" + str(asset.get_editor_property(attr)))
		except Exception as exc:
			unreal.log("  prop " + attr + " ERR " + str(exc))
	try:
		ad = registry.get_asset_by_object_path(path)
		unreal.log("  assetdata class=" + str(ad.asset_class_path))
		unreal.log("  tags str=" + str(ad.tags_and_values))
		for tag_name in ["GeneratedClass", "ParentClass", "NativeParentClass", "BlueprintType", "ClassFlags"]:
			try:
				unreal.log("  tag " + tag_name + "=" + str(ad.get_tag_value(tag_name)))
			except Exception as exc:
				unreal.log("  tag " + tag_name + " ERR " + str(exc))
	except Exception as exc:
		unreal.log("  assetdata ERR " + str(exc))
	try:
		unreal.log("  dir sample=" + ", ".join([x for x in dir(asset) if "class" in x.lower() or "graph" in x.lower()][:80]))
	except Exception as exc:
		unreal.log("  dir ERR " + str(exc))
	try:
		unreal.log("  direct generated_class call=" + str(asset.generated_class()))
	except Exception as exc:
		unreal.log("  direct generated_class call ERR " + str(exc))
try:
	unreal.log("CODEX_DEBUG_BlueprintEditorLibrary=" + ", ".join([x for x in dir(unreal.BlueprintEditorLibrary) if "graph" in x.lower() or "blueprint" in x.lower() or "variable" in x.lower() or "function" in x.lower()]))
except Exception as exc:
	unreal.log("CODEX_DEBUG_BlueprintEditorLibrary ERR " + str(exc))
try:
	unreal.log("CODEX_DEBUG_KismetEditorUtilities=" + ", ".join([x for x in dir(unreal.KismetEditorUtilities) if "graph" in x.lower() or "blueprint" in x.lower()]))
except Exception as exc:
	unreal.log("CODEX_DEBUG_KismetEditorUtilities ERR " + str(exc))
