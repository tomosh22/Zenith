import unreal

for path in [
	"/Game/DevilsPlayground/AI/BT_Priest.BT_Priest",
	"/Game/DevilsPlayground/AI/BB_Priest.BB_Priest",
	"/Game/DevilsPlayground/AI/BP_PriestController.BP_PriestController",
]:
	asset = unreal.EditorAssetLibrary.load_asset(path)
	unreal.log("CODEX_AI " + path + " asset=" + str(asset) + " class=" + asset.get_class().get_name())
	unreal.log("  dir=" + ", ".join([x for x in dir(asset) if "black" in x.lower() or "root" in x.lower() or "node" in x.lower() or "tree" in x.lower() or "graph" in x.lower() or "key" in x.lower()]))
	for name in ["blackboard_asset", "BlackboardAsset", "root_node", "RootNode", "keys", "Keys", "root_decorators", "RootDecorators"]:
		try:
			v = asset.get_editor_property(name)
			unreal.log("  prop " + name + "=" + str(v))
		except Exception as exc:
			try:
				unreal.log("  attr " + name + "=" + str(getattr(asset, name)))
			except Exception:
				unreal.log("  " + name + " ERR " + str(exc))
	if asset.get_class().get_name() == "Blueprint":
		try:
			gc = asset.generated_class()
			cdo = unreal.get_default_object(gc)
			unreal.log("  gc=" + str(gc) + " cdo=" + str(cdo))
			unreal.log("  cdo dir=" + ", ".join([x for x in dir(cdo) if "tree" in x.lower() or "black" in x.lower() or "target" in x.lower() or "dummy" in x.lower()]))
			for name in ["BehaviorTree", "BT_Priest", "BlackboardAsset", "TargetArray", "DummyNoiseMachine", "RunBehaviorTree"]:
				try:
					unreal.log("  cdo prop " + name + "=" + str(cdo.get_editor_property(name)))
				except Exception as exc:
					unreal.log("  cdo " + name + " ERR " + str(exc))
		except Exception as exc:
			unreal.log("  bp detail ERR " + str(exc))
