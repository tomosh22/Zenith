import json
import os
import traceback

import unreal


OUT_DIR = r"C:/dev/Zenith/.codex_tmp"
REPORT_JSON = os.path.join(OUT_DIR, "gamejam0_ue_report.json").replace("\\", "/")
REPORT_TXT = os.path.join(OUT_DIR, "gamejam0_ue_report.txt").replace("\\", "/")


def s(value):
	try:
		if value is None:
			return None
		if hasattr(value, "get_path_name"):
			return value.get_path_name()
		if hasattr(value, "to_string"):
			return value.to_string()
		return str(value)
	except Exception:
		return repr(value)


def cls_name(obj):
	try:
		return obj.get_class().get_name()
	except Exception:
		return type(obj).__name__


def prop(obj, name, default=None):
	try:
		return obj.get_editor_property(name)
	except Exception:
		try:
			return getattr(obj, name)
		except Exception:
			return default


def prop_s(obj, name):
	return s(prop(obj, name))


def boolish(value):
	if value is None:
		return None
	return bool(value)


def asset_data_path(asset_data):
	try:
		return asset_data.get_soft_object_path().to_string()
	except Exception:
		try:
			return str(asset_data.object_path)
		except Exception:
			return str(asset_data.package_name) + "." + str(asset_data.asset_name)


def asset_data_class(asset_data):
	try:
		return str(asset_data.asset_class_path.asset_name)
	except Exception:
		try:
			return str(asset_data.asset_class)
		except Exception:
			return "Unknown"


def load(path):
	try:
		return unreal.EditorAssetLibrary.load_asset(path)
	except Exception:
		return None


def vector_summary(v):
	try:
		return {"x": float(v.x), "y": float(v.y), "z": float(v.z)}
	except Exception:
		return s(v)


def rotator_summary(r):
	try:
		return {"pitch": float(r.pitch), "yaw": float(r.yaw), "roll": float(r.roll)}
	except Exception:
		return s(r)


def summarize_component(comp):
	out = {
		"name": comp.get_name(),
		"class": cls_name(comp),
	}
	for p in [
		"static_mesh",
		"skeletal_mesh",
		"widget_class",
		"template",
		"asset",
		"niagara_system_asset",
		"collision_profile_name",
		"component_tags",
		"sphere_radius",
		"relative_location",
		"relative_rotation",
		"relative_scale3d",
		"generate_overlap_events",
		"hidden_in_game",
		"render_custom_depth",
	]:
		v = prop(comp, p)
		if v is not None:
			if p.endswith("location") or p.endswith("scale3d"):
				out[p] = vector_summary(v)
			elif p.endswith("rotation"):
				out[p] = rotator_summary(v)
			elif isinstance(v, (list, tuple)):
				out[p] = [s(x) for x in v]
			else:
				out[p] = s(v)
	return out


KNOWN_PROPS = [
	# Game framework / controller
	"default_pawn_class",
	"DefaultPawnClass",
	"player_controller_class",
	"PlayerControllerClass",
	"hud_class",
	"HUDClass",
	"game_state_class",
	"GameStateClass",
	"default_mapping_contexts",
	"DefaultMappingContexts",
	"rotate_action",
	"RotateAction",
	"zoom_action",
	"ZoomAction",
	"rotate_speed",
	"RotateSpeed",
	"rotate_radius",
	"RotateRadius",
	"zoom_speed",
	"ZoomSpeed",
	"pan_speed",
	"PanSpeed",
	"pan_edge_range",
	"PanEdgeRange",
	"current_villager",
	"CurrentVillager",
	# Villager
	"b_can_control",
	"bCanControl",
	"move_action",
	"MoveAction",
	"interact_action",
	"InteractAction",
	"ability_action",
	"AbilityAction",
	"remaining_life",
	"RemainingLife",
	"held_item",
	"HeldItem",
	"is_possessed",
	"IsPossessed",
	# Items / interactables
	"item_tag",
	"ItemTag",
	"item_class",
	"ItemClass",
	"item_mesh",
	"ItemMesh",
	"sphere_collision",
	"SphereCollision",
	"widget_component",
	"WidgetComponent",
	"b_interact_on_overlap",
	"bInteractOnOverlap",
	"interaction_area",
	"InteractionArea",
	"interacting_actor",
	"InteractingActor",
	# UI
	"timed_life_bar",
	"TimedLifeBar",
	"escape_menu_class",
	"EscapeMenuClass",
	"max_lifetime",
	"MaxLifetime",
	"current_lifetime",
	"CurrentLifetime",
	"b_is_active",
	"bIsActive",
	# Common BP variable guesses
	"required_key",
	"required_key_tag",
	"key_tag",
	"door_open_angle",
	"open_angle",
	"is_open",
	"objective_mesh_1",
	"objective_mesh_2",
	"objective_mesh_3",
	"objective_mesh_4",
	"objective_mesh_5",
	"collected_objectives",
	"fog_holes_render_target",
	"villager_fog_holes",
	"light_fog_holes",
]


def summarize_known_props(obj):
	out = {}
	for p in KNOWN_PROPS:
		v = prop(obj, p, None)
		if v is None:
			continue
		if isinstance(v, (list, tuple)):
			out[p] = [s(x) for x in v]
		elif isinstance(v, (bool, int, float, str)):
			out[p] = v
		else:
			out[p] = s(v)
	return out


def summarize_graph_node(node):
	out = {"class": cls_name(node), "name": node.get_name()}
	for method_name in ["get_node_title", "get_tooltip_text"]:
		try:
			if method_name == "get_node_title":
				out["title"] = str(node.get_node_title(unreal.NodeTitleType.FULL_TITLE))
			else:
				out["tooltip"] = str(node.get_tooltip_text())
		except Exception:
			pass
	for p in [
		"function_reference",
		"event_reference",
		"delegate_property_name",
		"custom_function_name",
		"function_name",
		"variable_reference",
		"member_name",
		"proxy_factory_function_name",
		"proxy_factory_class",
	]:
		v = prop(node, p)
		if v is not None:
			out[p] = s(v)
	try:
		pins = prop(node, "pins", [])
		out["pins"] = []
		for pin in pins or []:
			pinfo = {}
			for pp in ["pin_name", "pin_friendly_name", "default_value", "default_object", "linked_to", "pin_type"]:
				pv = prop(pin, pp)
				if pv is None:
					continue
				if isinstance(pv, (list, tuple)):
					pinfo[pp] = [s(x) for x in pv]
				else:
					pinfo[pp] = s(pv)
			out["pins"].append(pinfo)
	except Exception:
		pass
	return out


def summarize_blueprint(asset, asset_path):
	out = {"path": asset_path, "class": cls_name(asset)}
	try:
		parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(asset)
		out["parent_class"] = s(parent)
	except Exception:
		out["parent_class"] = prop_s(asset, "parent_class")
	generated_class = prop(asset, "generated_class")
	if callable(generated_class):
		try:
			generated_class = generated_class()
		except Exception:
			generated_class = None
	if generated_class:
		out["generated_class"] = s(generated_class)
		try:
			out["parent_class_from_generated"] = s(generated_class.get_super_class())
		except Exception:
			pass
		try:
			cdo = unreal.get_default_object(generated_class)
			out["cdo_class"] = cls_name(cdo)
			out["cdo_props"] = summarize_known_props(cdo)
			try:
				comps = cdo.get_components_by_class(unreal.ActorComponent)
				out["cdo_components"] = [summarize_component(c) for c in comps]
			except Exception:
				pass
		except Exception as exc:
			out["cdo_error"] = str(exc)
	graphs = []
	try:
		event_graph = unreal.BlueprintEditorLibrary.find_event_graph(asset)
		if event_graph:
			graphs.append({
				"name": event_graph.get_name(),
				"kind": "event_graph",
				"nodes": [summarize_graph_node(n) for n in (prop(event_graph, "nodes", []) or [])],
			})
	except Exception:
		pass
	for graph_array_name in ["ubergraph_pages", "function_graphs", "macro_graphs", "delegate_signature_graphs"]:
		for graph in prop(asset, graph_array_name, []) or []:
			g = {"name": graph.get_name(), "kind": graph_array_name}
			nodes = prop(graph, "nodes", []) or []
			g["nodes"] = [summarize_graph_node(n) for n in nodes]
			graphs.append(g)
	out["graphs"] = graphs
	return out


def summarize_input_mapping(asset, asset_path):
	out = {"path": asset_path, "class": cls_name(asset), "mappings": []}
	for mapping in prop(asset, "mappings", []) or []:
		entry = {}
		for p in ["action", "key", "triggers", "modifiers"]:
			v = prop(mapping, p)
			if isinstance(v, (list, tuple)):
				entry[p] = [s(x) for x in v]
			elif p == "key" and v is not None:
				key_name = prop(v, "key_name")
				entry[p] = s(key_name) if key_name is not None else s(v)
			else:
				entry[p] = s(v)
		out["mappings"].append(entry)
	return out


def summarize_blackboard(asset, asset_path):
	out = {"path": asset_path, "class": cls_name(asset), "keys": []}
	for key in prop(asset, "keys", []) or []:
		out["keys"].append({
			"name": s(prop(key, "entry_name")),
			"type": s(prop(key, "key_type")),
			"synced": boolish(prop(key, "b_instance_synced")),
		})
	return out


def summarize_bt_node(node, depth=0, seen=None):
	if seen is None:
		seen = set()
	if not node:
		return None
	key = s(node)
	if key in seen or depth > 12:
		return {"class": cls_name(node), "name": node.get_name(), "cycle_or_depth_limit": True}
	seen.add(key)
	out = {
		"class": cls_name(node),
		"name": node.get_name(),
		"node_name": prop_s(node, "node_name"),
	}
	for p in ["blackboard_key", "acceptable_radius", "filter_class", "observed_key_names", "flow_abort_mode"]:
		v = prop(node, p)
		if v is not None:
			out[p] = s(v)
	children = []
	for child in prop(node, "children", []) or []:
		child_out = {}
		for p in ["child_composite", "child_task", "decorators", "services"]:
			v = prop(child, p)
			if isinstance(v, (list, tuple)):
				child_out[p] = [summarize_bt_node(x, depth + 1, seen) for x in v]
			else:
				child_out[p] = summarize_bt_node(v, depth + 1, seen) if v else None
		children.append(child_out)
	if children:
		out["children"] = children
	return out


def summarize_behavior_tree(asset, asset_path):
	out = {
		"path": asset_path,
		"class": cls_name(asset),
		"blackboard_asset": s(prop(asset, "blackboard_asset")),
	}
	out["root_node"] = summarize_bt_node(prop(asset, "root_node"))
	return out


def summarize_actor(actor):
	out = {
		"name": actor.get_name(),
		"label": actor.get_actor_label() if hasattr(actor, "get_actor_label") else actor.get_name(),
		"class": cls_name(actor),
		"class_path": s(actor.get_class()),
		"location": vector_summary(actor.get_actor_location()),
		"rotation": rotator_summary(actor.get_actor_rotation()),
		"scale": vector_summary(actor.get_actor_scale3d()),
	}
	out["props"] = summarize_known_props(actor)
	try:
		out["components"] = [summarize_component(c) for c in actor.get_components_by_class(unreal.ActorComponent)]
	except Exception as exc:
		out["component_error"] = str(exc)
	return out


def load_map_and_summarize(map_package_path):
	out = {"path": map_package_path}
	try:
		unreal.EditorLoadingAndSavingUtils.load_map(map_package_path)
		actors = unreal.EditorLevelLibrary.get_all_level_actors()
		out["actor_count"] = len(actors)
		counts = {}
		actors_out = []
		for actor in actors:
			cn = cls_name(actor)
			counts[cn] = counts.get(cn, 0) + 1
			actors_out.append(summarize_actor(actor))
		out["class_counts"] = dict(sorted(counts.items(), key=lambda kv: (-kv[1], kv[0])))
		out["actors"] = actors_out
	except Exception as exc:
		out["error"] = str(exc)
		out["traceback"] = traceback.format_exc()
	return out


def package_path_without_object(asset_path):
	return asset_path.split(".")[0]


def referencers(asset_path):
	try:
		package_path = package_path_without_object(asset_path)
		return [s(x) for x in unreal.EditorAssetLibrary.find_package_referencers_for_asset(package_path, False)]
	except Exception:
		return []


def main():
	os.makedirs(OUT_DIR, exist_ok=True)
	registry = unreal.AssetRegistryHelpers.get_asset_registry()
	registry.search_all_assets(True)
	all_assets = list(registry.get_assets_by_path("/Game", True))
	assets_summary = []
	class_counts = {}
	for ad in all_assets:
		ap = asset_data_path(ad)
		ac = asset_data_class(ad)
		assets_summary.append({"path": ap, "class": ac, "name": str(ad.asset_name), "package": str(ad.package_name)})
		class_counts[ac] = class_counts.get(ac, 0) + 1
	assets_summary.sort(key=lambda x: x["path"])

	report = {
		"asset_count": len(assets_summary),
		"asset_class_counts": dict(sorted(class_counts.items(), key=lambda kv: (-kv[1], kv[0]))),
		"assets": assets_summary,
	}

	blueprints = []
	input_mappings = []
	blackboards = []
	behavior_trees = []
	for info in assets_summary:
		ac = info["class"]
		ap = info["path"]
		if ac in ["Blueprint", "WidgetBlueprint", "AnimBlueprint"]:
			asset = load(ap)
			if asset:
				blueprints.append(summarize_blueprint(asset, ap))
		elif ac == "InputMappingContext":
			asset = load(ap)
			if asset:
				input_mappings.append(summarize_input_mapping(asset, ap))
		elif ac == "BlackboardData":
			asset = load(ap)
			if asset:
				blackboards.append(summarize_blackboard(asset, ap))
		elif ac == "BehaviorTree":
			asset = load(ap)
			if asset:
				behavior_trees.append(summarize_behavior_tree(asset, ap))

	report["blueprints"] = blueprints
	report["input_mappings"] = input_mappings
	report["blackboards"] = blackboards
	report["behavior_trees"] = behavior_trees

	world_paths = [i["path"] for i in assets_summary if i["class"] == "World"]
	report["maps"] = [load_map_and_summarize(package_path_without_object(p)) for p in world_paths]

	interesting_terms = [
		"BP_Forge",
		"BP_Interactable_Pentagram",
		"BP_DoubleDoor",
		"BP_DPDoubleDoor",
		"BP_DummyNoiseMachine",
		"BP_ItemManager",
		"BP_FogManager",
		"RT_VillagerFogHoles",
		"RT_LightFogHoles",
		"BP_Pleb",
		"BP_Villager",
		"BP_PlayerController",
		"BP_PriestController",
		"BT_Priest",
		"BB_Priest",
		"BTT_FindPosInSuspicionSphere",
		"BTD_IsTargetValid",
		"PFX_Witch",
		"IMC_Villager",
		"IA_VillagerAbility",
		"WBP_ItemPickUp",
		"WBP_HUDLayout",
		"BP_Item_Objective",
	]
	interesting = {}
	for term in interesting_terms:
		matches = [i for i in assets_summary if term.lower() in i["path"].lower()]
		interesting[term] = [
			{
				"path": m["path"],
				"class": m["class"],
				"referencers": referencers(m["path"]),
			}
			for m in matches
		]
	report["interesting_assets"] = interesting

	with open(REPORT_JSON, "w", encoding="utf-8") as f:
		json.dump(report, f, indent=2, sort_keys=True)

	lines = []
	lines.append(f"Asset count: {report['asset_count']}")
	lines.append("Asset classes:")
	for k, v in report["asset_class_counts"].items():
		lines.append(f"  {k}: {v}")
	lines.append("")
	lines.append("Maps:")
	for m in report["maps"]:
		lines.append(f"  {m.get('path')} actors={m.get('actor_count')} error={m.get('error')}")
		for cn, count in list((m.get("class_counts") or {}).items())[:20]:
			lines.append(f"    {cn}: {count}")
	lines.append("")
	lines.append("Input mappings:")
	for im in report["input_mappings"]:
		lines.append(f"  {im['path']}")
		for mapping in im["mappings"]:
			lines.append(f"    {mapping.get('action')} -> {mapping.get('key')}")
	lines.append("")
	lines.append("Blackboards:")
	for bb in report["blackboards"]:
		lines.append(f"  {bb['path']}")
		for key in bb["keys"]:
			lines.append(f"    {key['name']} : {key['type']}")
	lines.append("")
	lines.append("Interesting referencers:")
	for term, matches in report["interesting_assets"].items():
		lines.append(f"  {term}:")
		for match in matches:
			lines.append(f"    {match['path']} [{match['class']}]")
			for ref in match["referencers"]:
				lines.append(f"      ref {ref}")
	with open(REPORT_TXT, "w", encoding="utf-8") as f:
		f.write("\n".join(lines))

	unreal.log(f"CODEX_GAMEJAM0_REPORT_JSON={REPORT_JSON}")
	unreal.log(f"CODEX_GAMEJAM0_REPORT_TXT={REPORT_TXT}")


main()
