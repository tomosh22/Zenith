#pragma once

// ============================================================================
// CB_ToolIcons — shared presentation metadata for the toolbar buttons: the icon
// file base-name + the hover-tooltip description for each tool. Runtime-safe
// (no drawing, no ZENITH_TOOLS, no engine deps) so BOTH the icon GENERATOR
// (CityBuilder.cpp, tools build) and the HUD (CB_CityManager_Behaviour, runtime)
// read the SAME list. Indexed 1:1 with CB_CityManager_Behaviour::ToolDescs() —
// keep the order in sync.
//
// Icons are generated procedurally at tools-build into
//   <GAME_ASSETS_DIR>/UI/Icons/cb_<szIcon>.ztxtr
// and loaded at runtime via the path  game:UI/Icons/cb_<szIcon>.ztxtr.
// ============================================================================

namespace CB_ToolIcons
{
	struct Def
	{
		const char* szIcon;     // file base-name (cb_<szIcon>.ztxtr) + glyph selector
		const char* szTooltip;  // hover text describing what the tool does
	};

	inline const Def* All(int& iCountOut)
	{
		static const Def s_axDefs[] = {
			{ "bulldoze", "Bulldoze  -  remove the nearest road" },
			{ "road",     "Road  -  click two points to lay a curved road" },
			{ "res",      "Residential zone  -  homes grow along the road" },
			{ "com",      "Commercial zone  -  shops & offices grow here" },
			{ "ind",      "Industrial zone  -  factories provide jobs & goods" },
			{ "park",     "Park  -  raises nearby land value & happiness" },
			{ "power",    "Power plant  -  supplies the city with electricity" },
			{ "water",    "Water tower  -  supplies the city with water" },
			{ "police",   "Police station  -  safety coverage" },
			{ "fire",     "Fire station  -  fights building fires" },
			{ "health",   "Hospital  -  health coverage" },
			{ "school",   "School  -  education coverage" },
			{ "garbage",  "Landfill  -  collects the city's garbage" },
			{ "sewage",   "Sewage plant  -  treats waste water" },
			{ "transit",  "Bus depot  -  public transport, eases congestion" },
			{ "mail",     "Post office  -  mail collection" },
			{ "district", "District  -  paint an area, then F1-F4 set policies" },
			{ "busline",  "Transit line  -  left-click to add stops to a bus line" },
			{ "conduit",  "Utility conduit  -  extends power & water reach" },
			{ "terrain",  "Terraform  -  hold LMB raise / RMB lower the ground" },
		};
		iCountOut = static_cast<int>(sizeof(s_axDefs) / sizeof(s_axDefs[0]));
		return s_axDefs;
	}
}
