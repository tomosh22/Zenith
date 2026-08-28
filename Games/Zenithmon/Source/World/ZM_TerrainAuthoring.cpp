#include "Zenith.h"
#include "Zenithmon/Source/World/ZM_TerrainAuthoring.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>

#ifdef ZENITH_TOOLS
#include "Core/Zenith_Engine.h"
#include "Editor/TerrainEditor/Zenith_TerrainEditor.h"
#include "Editor/Zenith_EditorAutomation.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#endif

namespace
{
	constexpr u_int uDIRT_PATH_PASSES = 5u;
	constexpr float fTERRAIN_CHUNK_WORLD_SIZE = 64.0f;
	constexpr u_int uMANIFEST_MAGIC_SIZE = 4u;
	constexpr char acMANIFEST_MAGIC[uMANIFEST_MAGIC_SIZE] = { 'Z', 'M', 'T', 'R' };
	constexpr const char* szMANIFEST_NAME = "ZM_TerrainRecipe.manifest";

	// ★ RE-AUTHORED FOR THE 576 x 640 m MAP, not translated. The old four hills
	// were sized to fill a 1024 x 1024 world (radii 160-190, one of them
	// explicitly "850 + 174 = 1024" to touch the east boundary); translating them
	// would have put every centre outside the new grid. What they DID was frame
	// the town on all four sides from ~350 m out, and that is what these
	// reproduce at ~200 m out with radii scaled to match.
	//
	// ★★ A LANDFORM CANNOT HUG AN EDGE, AND THAT IS ENFORCED. Every brush dab is
	// checked centre +/- radius against the recipe's own bounds
	// (AssertTerrainAuthoringPlanContained), so a "flanking ridge centred ON the
	// west edge" is not a thing a small map can have -- the first draft of this
	// table put five of its six hills outside the world and would have asserted on
	// the first bake. A hill's centre must be at least its radius in from each
	// edge, which on a 576 m axis means a 95 m hill lives in X [95, 481].
	//
	// The town core plus its pads occupy X 104..456 of 576 after the translate, so
	// these sit AROUND that footprint with their inner slopes reaching into it.
	// That is fine and is how the 1024 m original worked too: pads and paths
	// flatten in a LATER phase, so wherever a ridge foot crosses the plaza, the
	// Lab pad or the route corridor, the flatten wins and the town reads as
	// built into the foot of the hills rather than perched on them.
	const ZM_TerrainLandformSpec s_axDawnmereLandforms[] =
	{
		{ { 95.0f, 300.0f }, 95.0f, 0.52f, 38.0f },     // west flank ridge
		{ { 490.0f, 350.0f }, 85.0f, 0.52f, 42.0f },    // east flank ridge, behind the Lab
		{ { 300.0f, 85.0f }, 85.0f, 0.45f, 34.0f },     // south backdrop behind the town
		{ { 90.0f, 545.0f }, 90.0f, 0.48f, 36.0f },     // north-west, framing the route corridor
		{ { 485.0f, 520.0f }, 90.0f, 0.48f, 38.0f },    // north-east, ditto
	};

	// Paths, pads, landmarks and the preview camera below are the SAME authored
	// layout translated by (-232, -320) -- every relative distance is preserved
	// exactly, so the town reads as it always did; only its world origin moved.
	//
	// ★ BOTH OFFSETS ARE MULTIPLES OF 4, AND THAT IS THE REASON THEY ARE THOSE
	// NUMBERS. The physics mesh is built at density divisor 4 (4 m quads) against
	// a 1 m render mesh, so an anchor on a 4 m lattice is a SHARED VERTEX of both
	// and samples exactly rather than by interpolation -- the property
	// ZM_DawnmerePlacement.h's ZM-D-186 note relies on when it explains why the
	// town centre did not move when the collision density changed under it. -230
	// would have put every Dawnmere anchor mid-quad; -232 keeps 512/384/640 on
	// 280/152/408. The margin this spends is two metres of empty edge.
	const ZM_TerrainPoint2 s_axDawnmereRoutePath[] =
	{
		{ 280.0f, 608.0f },
		{ 268.0f, 440.0f },
		{ 292.0f, 300.0f },
		{ 280.0f, 192.0f },
	};

	const ZM_TerrainPoint2 s_axDawnmereHomePath[] =
	{
		{ 280.0f, 192.0f },
		{ 222.0f, 166.0f },
		{ 152.0f, 136.0f },
	};

	const ZM_TerrainPoint2 s_axDawnmereLabPath[] =
	{
		{ 280.0f, 192.0f },
		{ 342.0f, 206.0f },
		{ 408.0f, 232.0f },
	};

	const ZM_TerrainPathSpec s_axDawnmerePaths[] =
	{
		{ "Route", s_axDawnmereRoutePath, 4u, 18.0f, 9.0f, 49u, 10.0f, 6.0f, 73u },
		{ "Home", s_axDawnmereHomePath, 3u, 13.0f, 7.0f, 22u, 7.0f, 5.0f, 30u },
		{ "Lab", s_axDawnmereLabPath, 3u, 13.0f, 7.0f, 22u, 7.0f, 5.0f, 29u },
	};

	const ZM_TerrainPadSpec s_axDawnmerePads[] =
	{
		{ "Plaza", { 280.0f, 192.0f }, 60.0f, 45.0f, 4u },
		// ZM-D-173: the Home pad moved +40 m in Z with the shell it flattens
		// ground for. The shell now occupies z 156..196 with its entrance on the
		// -Z face, so a pad still centred at z=136 would flatten the forecourt and
		// leave the building itself on unlevelled ground. Radius, target height and
		// pass count are UNCHANGED -- only the centre moves.
		//
		// ★ THE PATH ENDPOINT DELIBERATELY DID NOT MOVE WITH IT. The Home dirt path
		// still terminates at (152, 136); its flatten radius overlaps this pad, so
		// the two together pave one continuous walkway from the plaza into the
		// forecourt the camera now trails into.
		{ "Home", { 152.0f, 176.0f }, 36.0f, 26.0f, 4u },
		{ "Lab", { 408.0f, 232.0f }, 48.0f, 38.0f, 4u },
		{ "RouteGate", { 280.0f, 576.0f }, 30.0f, 0.0f, 0u },
	};

	const ZM_TerrainAutoSplatSpec s_axDawnmereAutoSplat[] =
	{
		{ "Meadow", 10.0f, 50.0f, 0.0f, 22.0f, 1.25f, 0.12f },
		{ "Stone", 0.0f, 512.0f, 18.0f, 90.0f, 1.10f, 0.08f },
		{ "Dirt", 0.0f, 36.0f, 0.0f, 35.0f, 0.45f, 0.18f },
		{ "Heath", 28.0f, 90.0f, 0.0f, 25.0f, 0.55f, 0.25f },
	};

	// RE-AUTHORED with the landforms: the peripheral dabs sat on the old hills at
	// radii 80-110 across a 1024 m map, so they are re-placed on the new ridges at
	// radii scaled to the 576 m one. The two CENTRAL dabs are the town lawn and
	// are translated rather than re-placed -- their job is tied to the plaza, not
	// to the terrain shape.
	const ZM_TerrainGrassDabSpec s_axDawnmereGrassDabs[] =
	{
		{ { 100.0f, 300.0f }, 80.0f, 0.55f },    // west ridge
		{ { 476.0f, 330.0f }, 80.0f, 0.55f },    // east ridge
		{ { 95.0f, 530.0f }, 70.0f, 0.70f },     // north-west knoll
		{ { 480.0f, 505.0f }, 70.0f, 0.70f },    // north-east knoll
		{ { 150.0f, 75.0f }, 70.0f, 0.45f },     // south backdrop, west lobe
		{ { 400.0f, 90.0f }, 70.0f, 0.45f },     // south backdrop, east lobe
		// Central town lawn so grass surrounds the TownCenter spawn (280,160) and
		// the returning-Home marker; the Plaza/Home pads and the paths erase their
		// paved footprints in the later grass-erase phase, leaving lawn between
		// the walkways. Without this the player stood in a grass-free hole and the
		// exterior view showed only distant peripheral grass.
		{ { 280.0f, 150.0f }, 110.0f, 0.60f },
		{ { 280.0f, 290.0f }, 95.0f, 0.55f },
	};

	const ZM_TerrainLandmarkSpec s_axDawnmereLandmarks[] =
	{
		{ "TownCenter", { 280.0f, 24.0f, 160.0f } },
		{ "Plaza", { 280.0f, 24.0f, 192.0f } },
		// ZM-D-173: both moved with the relocated Home. "Home" tracks the shell's
		// new centre; "FromHome" tracks the return spawn, which is now SOUTH of the
		// -Z entrance rather than north of the old +Z one. Y stays 24 -- these are
		// recipe METADATA at the nominal target height, not measured surfaces.
		{ "Home", { 152.0f, 24.0f, 176.0f } },
		{ "FromHome", { 152.0f, 24.0f, 148.0f } },
		{ "Lab", { 408.0f, 24.0f, 232.0f } },
		{ "FromLab", { 408.0f, 24.0f, 200.0f } },
		{ "FromRoute1", { 280.0f, 24.0f, 544.0f } },
		{ "RouteBoundary", { 280.0f, 24.0f, 608.0f } },
		{ "ReservedRivalHome", { 128.0f, 24.0f, 240.0f } },
		{ "PlazaLandmark", { 230.0f, 24.0f, 228.0f } },
	};

	// Meadow, Stone and Dirt are the TEXTURED slots: they sample the engine's
	// shared grass, rock and clay ground sets (Zenith/Assets/Textures/Terrain/
	// {Grass,Rock,Clay}); the first two are the same maps RenderTest's terrain uses
	// for its own flats and steeps. Their base colour is WHITE -- the shader
	// multiplies base colour into the sampled diffuse, so the old flat
	// green/grey/brown would tint the photographic ground. Only Heath stays
	// flat-colour.
	//
	// ★ THE CLAY SLOT TILES DIFFERENTLY, AND THAT IS THE POINT. Grass and Rock are
	// non-repeating photographic ground, so a 16 m repeat (tiling 0.9 =
	// 1 / (0.07 * 0.9)) hides. Clay is a REGULAR 6x6 GRID OF PAVING SLABS, so its
	// repeat is a visible architectural dimension rather than noise: at 0.9 each
	// slab would be 15.9/6 = 2.6 m across and the lanes would read as a chessboard.
	// Tiling 3.6 gives a 3.97 m repeat and so a 0.66 m slab, which is a paving
	// stone. Dirt paints the LANES AND PADS (see m_fDirtRadius on the path and pad
	// specs), so this slot is the town's paving, not its soil.
	const ZM_TerrainMaterialSpec s_axDawnmereMaterials[] =
	{
		{ "Meadow", { 1.0f, 1.0f, 1.0f, 1.0f }, 0.92f, 0.0f, "engine:Textures/Terrain/Grass/", 0.9f },
		{ "Stone", { 1.0f, 1.0f, 1.0f, 1.0f }, 0.88f, 0.0f, "engine:Textures/Terrain/Rock/", 0.9f },
		{ "Dirt", { 1.0f, 1.0f, 1.0f, 1.0f }, 0.96f, 0.0f, "engine:Textures/Terrain/Clay/", 3.6f },
		{ "Heath", { 0.48f, 0.55f, 0.20f, 1.0f }, 0.90f, 0.0f, nullptr, 0.0f },
	};

	// Thornacre is the GDD's hedgerow farming town: pasture rolls around a
	// central lane, berry rows, a drystone gym approach, and two required warp
	// landmarks. Building/tree dressing is deliberately outside terrain recipes.
	// RE-AUTHORED for the 832 x 960 m map, under the same containment rule
	// Dawnmere's landform note states: a hill's centre is at least its radius in
	// from every edge, so these four sit with their outer slopes reaching exactly
	// to the boundary rather than crossing it. Content occupies X 122..710 of 832
	// after the translate, which leaves the west and east flanks clear of the
	// lane and the berry rows; the fourth is the south-east shoulder above the
	// route gate, which is what it framed before.
	const ZM_TerrainLandformSpec s_axThornacreLandforms[] =
	{
		{ { 115.0f, 400.0f }, 115.0f, 0.50f, 44.0f },   // west flank
		{ { 715.0f, 450.0f }, 115.0f, 0.55f, 48.0f },   // east flank
		{ { 110.0f, 845.0f }, 110.0f, 0.42f, 37.0f },   // north-west shoulder
		{ { 715.0f, 135.0f }, 115.0f, 0.48f, 46.0f },   // south-east, above the route gate
	};

	// Translated by (-100, -16); every relative distance preserved.
	const ZM_TerrainPoint2 s_axThornacreMainLane[] =
	{
		{ 412.0f, 48.0f },
		{ 400.0f, 224.0f },
		{ 420.0f, 424.0f },
		{ 412.0f, 524.0f },
	};

	const ZM_TerrainPoint2 s_axThornacreGymLane[] =
	{
		{ 412.0f, 524.0f },
		{ 510.0f, 634.0f },
		{ 604.0f, 744.0f },
		{ 660.0f, 848.0f },
	};

	const ZM_TerrainPoint2 s_axThornacreBerryRow[] =
	{
		{ 412.0f, 524.0f },
		{ 290.0f, 554.0f },
		{ 170.0f, 634.0f },
	};

	const ZM_TerrainPathSpec s_axThornacrePaths[] =
	{
		{ "MainLane", s_axThornacreMainLane, 4u, 18.0f, 9.0f, 56u, 10.0f, 6.0f, 82u },
		{ "GymLane", s_axThornacreGymLane, 4u, 14.0f, 7.0f, 61u, 8.0f, 5.0f, 84u },
		{ "BerryRow", s_axThornacreBerryRow, 3u, 14.0f, 7.0f, 40u, 8.0f, 5.0f, 56u },
	};

	const ZM_TerrainPadSpec s_axThornacrePads[] =
	{
		{ "Market", { 412.0f, 524.0f }, 60.0f, 45.0f, 4u },
		{ "Gym", { 660.0f, 848.0f }, 50.0f, 38.0f, 4u },
		{ "BerryFields", { 170.0f, 634.0f }, 48.0f, 32.0f, 3u },
		{ "RouteGate", { 412.0f, 80.0f }, 30.0f, 0.0f, 0u },
	};

	const ZM_TerrainAutoSplatSpec s_axThornacreAutoSplat[] =
	{
		{ "Pasture", 12.0f, 58.0f, 0.0f, 20.0f, 1.25f, 0.10f },
		{ "Drystone", 0.0f, 512.0f, 17.0f, 90.0f, 1.15f, 0.07f },
		{ "Dirt", 0.0f, 42.0f, 0.0f, 36.0f, 0.50f, 0.16f },
		{ "Hedgerow", 30.0f, 88.0f, 0.0f, 24.0f, 0.62f, 0.22f },
	};

	// Re-placed onto the new flanks at radii scaled to the 832 m map.
	const ZM_TerrainGrassDabSpec s_axThornacreGrassDabs[] =
	{
		{ { 100.0f, 300.0f }, 90.0f, 0.65f },
		{ { 100.0f, 780.0f }, 85.0f, 0.78f },
		{ { 735.0f, 300.0f }, 85.0f, 0.62f },
		{ { 730.0f, 640.0f }, 90.0f, 0.72f },
		{ { 400.0f, 875.0f }, 80.0f, 0.58f },
	};

	const ZM_TerrainLandmarkSpec s_axThornacreLandmarks[] =
	{
		{ "FromRoute1", { 412.0f, 28.0f, 96.0f } },
		{ "RouteBoundary", { 412.0f, 28.0f, 48.0f } },
		{ "TownCenter", { 412.0f, 28.0f, 524.0f } },
		{ "Market", { 368.0f, 28.0f, 554.0f } },
		{ "Gym", { 660.0f, 28.0f, 848.0f } },
		{ "FromGym", { 660.0f, 28.0f, 798.0f } },
		{ "BerryFields", { 170.0f, 28.0f, 634.0f } },
	};

	const ZM_TerrainMaterialSpec s_axThornacreMaterials[] =
	{
		// Textured like Dawnmere's Meadow / Stone / Dirt: the shared ENGINE grass,
		// rock and clay ground sets. The base colour MULTIPLIES the sampled diffuse
		// (Flux_Terrain_ToGBuffer -> SampleDiffuseWithBaseColor), so a textured slot
		// authors WHITE and lets the texture carry the hue -- a green, grey or brown
		// tint here puts the flat look straight back. Every tiling number matches
		// Dawnmere's row-for-row, INCLUDING clay's 3.6: the ground slots read at one
		// scale across the three regions, and a paving slab is the same size in all
		// three towns. See Dawnmere's block for why clay is not 0.9.
		{ "Pasture", { 1.0f, 1.0f, 1.0f, 1.0f }, 0.94f, 0.0f, "engine:Textures/Terrain/Grass/", 0.9f },
		{ "Drystone", { 1.0f, 1.0f, 1.0f, 1.0f }, 0.91f, 0.0f, "engine:Textures/Terrain/Rock/", 0.9f },
		{ "Dirt", { 1.0f, 1.0f, 1.0f, 1.0f }, 0.97f, 0.0f, "engine:Textures/Terrain/Clay/", 3.6f },
		{ "Hedgerow", { 0.15f, 0.33f, 0.09f, 1.0f }, 0.93f, 0.0f, nullptr, 0.0f },
	};

	// Route 1 is a 16x24-chunk coastal-meadow corridor. Its broad encounter
	// fields flank one readable dirt lane, with a small rival/tutorial spur.
	// RE-AUTHORED for the 704 x 1536 m corridor. Z is unchanged (the route's
	// LENGTH is its whole point and did not shrink), so only the flanking hills
	// move inward: the corridor is 704 m wide instead of 1024, and the lane plus
	// its rival spur and the tutorial clearing occupy X 280..584, so the flanks
	// sit either side of that with their outer slopes on the boundary (the same
	// containment rule Dawnmere's landform note states). The northern hill still
	// straddles the lane at the route's end, as before.
	const ZM_TerrainLandformSpec s_axRoute1Landforms[] =
	{
		{ { 125.0f, 300.0f }, 125.0f, 0.42f, 34.0f },
		{ { 580.0f, 430.0f }, 120.0f, 0.45f, 38.0f },
		{ { 130.0f, 930.0f }, 130.0f, 0.50f, 42.0f },
		{ { 575.0f, 1160.0f }, 125.0f, 0.52f, 45.0f },
		// The one hill the lane runs THROUGH rather than past, exactly as before:
		// it straddles the route's northern end and the NorthGate pad cuts the
		// gateway back out of it.
		{ { 318.0f, 1400.0f }, 120.0f, 0.38f, 39.0f },
	};

	// Translated by (-184, 0); Z untouched. Four divides it, for the reason
	// Dawnmere's translate note gives.
	const ZM_TerrainPoint2 s_axRoute1DirtLane[] =
	{
		{ 328.0f, 64.0f },
		{ 296.0f, 300.0f },
		{ 356.0f, 560.0f },
		{ 316.0f, 820.0f },
		{ 366.0f, 1080.0f },
		{ 328.0f, 1472.0f },
	};

	const ZM_TerrainPoint2 s_axRoute1RivalSpur[] =
	{
		{ 316.0f, 820.0f },
		{ 466.0f, 820.0f },
		{ 536.0f, 880.0f },
	};

	const ZM_TerrainPathSpec s_axRoute1Paths[] =
	{
		{ "DirtLane", s_axRoute1DirtLane, 6u, 16.0f, 8.0f, 182u, 9.0f, 6.0f, 241u },
		{ "RivalSpur", s_axRoute1RivalSpur, 3u, 12.0f, 7.0f, 37u, 7.0f, 5.0f, 50u },
	};

	const ZM_TerrainPadSpec s_axRoute1Pads[] =
	{
		{ "SouthGate", { 328.0f, 96.0f }, 30.0f, 0.0f, 0u },
		{ "Midway", { 316.0f, 820.0f }, 38.0f, 28.0f, 3u },
		{ "TutorialClearing", { 536.0f, 880.0f }, 48.0f, 34.0f, 3u },
		{ "NorthGate", { 328.0f, 1440.0f }, 30.0f, 0.0f, 0u },
	};

	const ZM_TerrainAutoSplatSpec s_axRoute1AutoSplat[] =
	{
		{ "CoastalMeadow", 10.0f, 54.0f, 0.0f, 20.0f, 1.28f, 0.13f },
		{ "Chalk", 0.0f, 512.0f, 18.0f, 90.0f, 1.08f, 0.09f },
		{ "Dirt", 0.0f, 40.0f, 0.0f, 34.0f, 0.48f, 0.17f },
		{ "Wildflower", 24.0f, 78.0f, 0.0f, 23.0f, 0.58f, 0.27f },
	};

	// Re-placed onto the narrowed corridor; Z positions and weights unchanged, so
	// the meadow rhythm along the route's length is exactly what it was.
	const ZM_TerrainGrassDabSpec s_axRoute1GrassDabs[] =
	{
		{ { 135.0f, 260.0f }, 130.0f, 0.72f },
		{ { 570.0f, 340.0f }, 125.0f, 0.68f },
		{ { 135.0f, 650.0f }, 130.0f, 0.78f },
		{ { 570.0f, 690.0f }, 130.0f, 0.74f },
		{ { 135.0f, 1080.0f }, 130.0f, 0.76f },
		{ { 570.0f, 1190.0f }, 130.0f, 0.70f },
		{ { 110.0f, 1400.0f }, 90.0f, 0.64f },
		{ { 590.0f, 1420.0f }, 85.0f, 0.62f },
	};

	const ZM_TerrainLandmarkSpec s_axRoute1Landmarks[] =
	{
		{ "FromDawnmere", { 328.0f, 26.0f, 112.0f } },
		{ "DawnmereBoundary", { 328.0f, 26.0f, 64.0f } },
		{ "Midway", { 316.0f, 26.0f, 820.0f } },
		{ "RivalBattle", { 536.0f, 26.0f, 880.0f } },
		{ "TutorialFields", { 116.0f, 26.0f, 650.0f } },
		{ "FromThornacre", { 328.0f, 26.0f, 1424.0f } },
		{ "ThornacreBoundary", { 328.0f, 26.0f, 1472.0f } },
	};

	const ZM_TerrainMaterialSpec s_axRoute1Materials[] =
	{
		// Same argument as Thornacre's Pasture/Drystone/Dirt above: the three textured
		// slots sample the shared ENGINE grass, rock and clay sets at Dawnmere's
		// tilings, so their base colour is WHITE (base colour multiplies the sampled
		// diffuse). Chalk keeps its NAME -- the route's steeps are chalk downland in
		// the GDD -- while its surface is the shared rock set rather than a flat pale
		// grey. Route 1's Dirt is the LANE the whole route is built around, which is
		// the slot the clay paving most obviously reads on.
		{ "CoastalMeadow", { 1.0f, 1.0f, 1.0f, 1.0f }, 0.93f, 0.0f, "engine:Textures/Terrain/Grass/", 0.9f },
		{ "Chalk", { 1.0f, 1.0f, 1.0f, 1.0f }, 0.89f, 0.0f, "engine:Textures/Terrain/Rock/", 0.9f },
		{ "Dirt", { 1.0f, 1.0f, 1.0f, 1.0f }, 0.96f, 0.0f, "engine:Textures/Terrain/Clay/", 3.6f },
		{ "Wildflower", { 0.52f, 0.56f, 0.24f, 1.0f }, 0.91f, 0.0f, nullptr, 0.0f },
	};

	template<typename T, size_t N>
	constexpr u_int CountOf(const T (&)[N])
	{
		return static_cast<u_int>(N);
	}

	const ZM_TerrainAuthoringRecipe* TerrainRecipeRegistry()
	{
		// This is the complete S3 measurement registry. Its order follows the
		// outdoor rows in ZM_WorldSpec and is intentionally immutable/save-stable.
		static const ZM_TerrainAuthoringRecipe s_axRecipes[uZM_TERRAIN_RECIPE_COUNT] =
		{
			{
				&ZM_GetWorldSpec(ZM_SCENE_DAWNMERE),
				0x7BF32CA4u,
				// 9x10 chunks of 64 m at 1 m spacing = 576 x 640 m. Was
				// 16x16 (1024 x 1024) with the town using a third of it; the
				// authored content translated by (-232, -320) and the landscape
				// was re-authored to frame it at this scale.
				{ 64.0f, 64u, 9u, 10u },
				24.0f,
				{ 0.046875f, 0.018f, 0.00125f, 5u, 2.0f, 0.5f, 0.10f },
				s_axDawnmereLandforms, CountOf(s_axDawnmereLandforms),
				s_axDawnmerePaths, CountOf(s_axDawnmerePaths),
				s_axDawnmerePads, CountOf(s_axDawnmerePads),
				// ★ THE RADIUS SCALES WITH THE AXIS THAT SHRANK, and all three
				// recipes below follow the same rule: the old radius times the new
				// width over the old 1024. It keeps each map eroded over the same
				// PROPORTION of itself, which is what makes "region-only" mean the
				// same thing at three different scales -- 725 unchanged on a 576 m
				// map would blanket the sheet and erase the distinction entirely.
				// 725 * 576/1024 = 407.8.
				{ 60000u, 1u, true, { 280.0f, 192.0f }, 400.0f },
				s_axDawnmereAutoSplat, CountOf(s_axDawnmereAutoSplat),
				s_axDawnmereGrassDabs, CountOf(s_axDawnmereGrassDabs),
				s_axDawnmereLandmarks, CountOf(s_axDawnmereLandmarks),
				s_axDawnmereMaterials, CountOf(s_axDawnmereMaterials),
				{ { 280.0f, 52.0f, 100.0f }, 0.0f, -0.22f, 65.0f, 0.1f, 2000.0f },
			},
			{
				&ZM_GetWorldSpec(ZM_SCENE_THORNACRE),
				0x9D41BD83u,
				// 13x15 chunks = 832 x 960 m, was 16x16. Thornacre's content is
				// the widest of the three (the Gym sits 588 m east of the berry
				// fields), so it shrinks least. Translated by (-100, -16).
				{ 64.0f, 64u, 13u, 15u },
				28.0f,
				{ 0.0546875f, 0.020f, 0.00115f, 5u, 2.0f, 0.5f, 0.12f },
				s_axThornacreLandforms, CountOf(s_axThornacreLandforms),
				s_axThornacrePaths, CountOf(s_axThornacrePaths),
				s_axThornacrePads, CountOf(s_axThornacrePads),
				// 725 * 832/1024 = 589.2.
				{ 60000u, 1u, true, { 412.0f, 496.0f }, 590.0f },
				s_axThornacreAutoSplat, CountOf(s_axThornacreAutoSplat),
				s_axThornacreGrassDabs, CountOf(s_axThornacreGrassDabs),
				s_axThornacreLandmarks, CountOf(s_axThornacreLandmarks),
				s_axThornacreMaterials, CountOf(s_axThornacreMaterials),
				{ { 412.0f, 58.0f, 454.0f }, 0.05f, -0.24f, 65.0f, 0.1f, 2000.0f },
			},
			{
				&ZM_GetWorldSpec(ZM_SCENE_ROUTE1),
				0x552E711Du,
				// 11x24 chunks = 704 x 1536 m, was 16x24. Only the WIDTH shrinks:
				// the route's length is the whole point of it. Translated by
				// (-184, 0) -- Z is untouched, so every waypoint's distance along
				// the route is exactly what it was.
				{ 64.0f, 64u, 11u, 24u },
				26.0f,
				{ 0.05078125f, 0.019f, 0.00110f, 5u, 2.0f, 0.5f, 0.09f },
				s_axRoute1Landforms, CountOf(s_axRoute1Landforms),
				s_axRoute1Paths, CountOf(s_axRoute1Paths),
				s_axRoute1Pads, CountOf(s_axRoute1Pads),
				// 500 * 704/1024 = 343.75. Route 1 shrank only in X, so it is the
				// WIDTH that sets this -- the corridor's length is untouched and the
				// erosion still runs the same distance up and down it as a fraction
				// of the reach it had.
				{ 75000u, 1u, true, { 328.0f, 768.0f }, 350.0f },
				s_axRoute1AutoSplat, CountOf(s_axRoute1AutoSplat),
				s_axRoute1GrassDabs, CountOf(s_axRoute1GrassDabs),
				s_axRoute1Landmarks, CountOf(s_axRoute1Landmarks),
				s_axRoute1Materials, CountOf(s_axRoute1Materials),
				{ { 328.0f, 62.0f, 700.0f }, 0.0f, -0.23f, 65.0f, 0.1f, 2400.0f },
			},
		};
		return s_axRecipes;
	}

	int FindTerrainRecipeIndexByIdentity(const ZM_TerrainAuthoringRecipe& xRecipe)
	{
		const ZM_TerrainAuthoringRecipe* pxRecipes = TerrainRecipeRegistry();
		for (u_int i = 0; i < uZM_TERRAIN_RECIPE_COUNT; ++i)
		{
			if (&xRecipe == &pxRecipes[i])
			{
				return static_cast<int>(i);
			}
		}
		return -1;
	}

	bool IsSafeTerrainSetName(const char* szSet)
	{
		if (szSet == nullptr || szSet[0] == '\0')
		{
			return false;
		}
		for (u_int i = 0; szSet[i] != '\0'; ++i)
		{
			const char c = szSet[i];
			const bool bAlphaNumeric = (c >= 'a' && c <= 'z') ||
				(c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
			if ((!bAlphaNumeric && c != '_' && c != '-') || i >= 64u)
			{
				return false;
			}
		}
		return true;
	}

	// This USED to compare the recipe's four bounds floats against a
	// recomputation of them from its export rectangle -- two authored copies of
	// one fact, checked against each other. The bounds and the rect both derive
	// from m_xDims now, so that comparison would be a tautology: it can only
	// ever return true, and a guard that cannot fail is worse than no guard,
	// because it reads like coverage.
	//
	// What is worth asserting is what the dims themselves cannot enforce and
	// Zenithmon's authoring genuinely depends on:
	//   * the dims are internally valid and inside the 64x64 chunk capacity;
	//   * chunks are 64 m, because the nav grid, the bake manifest and every
	//     authored coordinate in this file are written in metres against that;
	//   * spacing is 1 m/vertex, which is the weld the splat and grass images
	//     rely on (world metre == image pixel at these image sizes).
	bool HasSupportedTerrainDimensions(const ZM_TerrainAuthoringRecipe& xRecipe)
	{
		if (!xRecipe.m_xDims.IsValid())
		{
			return false;
		}
		if (xRecipe.m_xDims.m_fChunkWorldSize != fTERRAIN_CHUNK_WORLD_SIZE)
		{
			return false;
		}
		return xRecipe.m_xDims.VertexSpacing() == 1.0f;
	}

	std::filesystem::path RelativeTerrainDirectory(const ZM_TerrainAuthoringRecipe& xRecipe)
	{
		Zenith_Assert(xRecipe.m_pxWorldSpec != nullptr, "Terrain recipe requires a WorldSpec row");
		Zenith_Assert(IsSafeTerrainSetName(xRecipe.m_pxWorldSpec->m_szTerrainSet),
			"Terrain recipe has unsafe WorldSpec terrain set");
		return std::filesystem::path("Terrain") / xRecipe.m_pxWorldSpec->m_szTerrainSet;
	}

	std::filesystem::path ManifestPath(const ZM_TerrainAuthoringRecipe& xRecipe,
		const std::filesystem::path& xGameAssetsRoot)
	{
		return (xGameAssetsRoot / RelativeTerrainDirectory(xRecipe) / szMANIFEST_NAME).lexically_normal();
	}

	void WriteU32LE(u_int8* pBytes, u_int uValue)
	{
		pBytes[0] = static_cast<u_int8>(uValue & 0xffu);
		pBytes[1] = static_cast<u_int8>((uValue >> 8u) & 0xffu);
		pBytes[2] = static_cast<u_int8>((uValue >> 16u) & 0xffu);
		pBytes[3] = static_cast<u_int8>((uValue >> 24u) & 0xffu);
	}

	u_int ReadU32LE(const u_int8* pBytes)
	{
		return static_cast<u_int>(pBytes[0]) |
			(static_cast<u_int>(pBytes[1]) << 8u) |
			(static_cast<u_int>(pBytes[2]) << 16u) |
			(static_cast<u_int>(pBytes[3]) << 24u);
	}

	bool HasValidManifest(const std::filesystem::path& xPath, u_int uRequiredOutputCount)
	{
		std::error_code xError;
		if (!std::filesystem::is_regular_file(xPath, xError) || xError ||
			std::filesystem::file_size(xPath, xError) != uZM_TERRAIN_MANIFEST_SIZE || xError)
		{
			return false;
		}

		u_int8 auBytes[uZM_TERRAIN_MANIFEST_SIZE] = {};
		std::ifstream xInput(xPath, std::ios::binary);
		xInput.read(reinterpret_cast<char*>(auBytes), sizeof(auBytes));
		if (!xInput || memcmp(auBytes, acMANIFEST_MAGIC, uMANIFEST_MAGIC_SIZE) != 0)
		{
			return false;
		}
		return ReadU32LE(auBytes + 4u) == uZM_TERRAIN_MANIFEST_VERSION &&
			ReadU32LE(auBytes + 8u) == uRequiredOutputCount;
	}

	bool IsNonEmptyFile(const std::filesystem::path& xPath)
	{
		std::error_code xError;
		return std::filesystem::is_regular_file(xPath, xError) && !xError &&
			std::filesystem::file_size(xPath, xError) > 0u && !xError;
	}

#ifdef ZENITH_TOOLS
	std::filesystem::path PreparedChildPath(const std::string& strPreparedDirectory,
		const char* szName)
	{
		return std::filesystem::path(strPreparedDirectory) / szName;
	}

	bool RemovePreparedManifestFiles(const std::string& strPreparedDirectory)
	{
		const std::filesystem::path xMarker =
			PreparedChildPath(strPreparedDirectory, szMANIFEST_NAME);
		std::filesystem::path xTemp = xMarker;
		xTemp += ".tmp";
		std::error_code xError;
		std::filesystem::remove(xMarker, xError);
		if (xError)
		{
			return false;
		}
		std::filesystem::remove(xTemp, xError);
		return !xError;
	}

	bool AreRequiredOutputsCompleteInPreparedDirectory(
		const ZM_TerrainAuthoringRecipe& xRecipe,
		const std::string& strPreparedDirectory)
	{
		Zenith_Vector<std::string> xOutputs;
		ZM_EnumerateRequiredTerrainOutputs(xRecipe, xOutputs);
		for (u_int i = 0; i < xOutputs.GetSize(); ++i)
		{
			const std::filesystem::path xFilename =
				std::filesystem::path(xOutputs.Get(i)).filename();
			if (xFilename.empty() || !IsNonEmptyFile(
				std::filesystem::path(strPreparedDirectory) / xFilename))
			{
				return false;
			}
		}
		return true;
	}

	bool FinalizePreparedTerrainBake(const ZM_TerrainAuthoringRecipe& xRecipe,
		const std::string& strPreparedDirectory, const std::string& strTerrainRoot)
	{
		if (!AreRequiredOutputsCompleteInPreparedDirectory(xRecipe, strPreparedDirectory))
		{
			return false;
		}

		const std::filesystem::path xMarker =
			PreparedChildPath(strPreparedDirectory, szMANIFEST_NAME);
		std::filesystem::path xTemp = xMarker;
		xTemp += ".tmp";
		std::error_code xError;
		std::filesystem::remove(xTemp, xError);
		if (xError)
		{
			return false;
		}

		u_int8 auBytes[uZM_TERRAIN_MANIFEST_SIZE] = {};
		memcpy(auBytes, acMANIFEST_MAGIC, uMANIFEST_MAGIC_SIZE);
		WriteU32LE(auBytes + 4u, uZM_TERRAIN_MANIFEST_VERSION);
		const u_int uRequiredOutputCount = ZM_GetTerrainRequiredOutputCount(xRecipe);
		WriteU32LE(auBytes + 8u, uRequiredOutputCount);
		{
			std::ofstream xOutput(xTemp, std::ios::binary | std::ios::trunc);
			xOutput.write(reinterpret_cast<const char*>(auBytes), sizeof(auBytes));
			xOutput.flush();
			if (!xOutput)
			{
				return false;
			}
		}

		// Preparation removed the old marker. Remove defensively for the
		// standalone test wrapper, then make same-directory rename the final write.
		std::filesystem::remove(xMarker, xError);
		if (xError)
		{
			std::filesystem::remove(xTemp, xError);
			return false;
		}
		if (!Zenith_TerrainComponent::RenamePreparedTerrainAssetFileAtomically(
			xRecipe.m_pxWorldSpec->m_szTerrainSet, strTerrainRoot,
			xTemp.filename().string(), xMarker.filename().string()))
		{
			std::filesystem::remove(xTemp, xError);
			return false;
		}
		return HasValidManifest(xMarker, uRequiredOutputCount);
	}
#endif

	ZM_TerrainPlanOp MakeSimpleOp(ZM_TERRAIN_PLAN_OP_TYPE eType, u_int uIndex = 0u)
	{
		ZM_TerrainPlanOp xOp;
		xOp.m_eType = eType;
		xOp.m_uIndex = uIndex;
		return xOp;
	}

	void AppendDab(Zenith_Vector<ZM_TerrainPlanOp>& xPlan,
		ZM_TERRAIN_DAB_KIND eKind, ZM_TERRAIN_PLAN_PHASE ePhase,
		float fX, float fZ, float fRadius, float fStrength, float fValue)
	{
		ZM_TerrainPlanOp xOp;
		xOp.m_eType = ZM_TERRAIN_PLAN_BRUSH_DAB;
		xOp.m_eDabKind = eKind;
		xOp.m_ePhase = ePhase;
		xOp.m_fWorldX = fX;
		xOp.m_fWorldZ = fZ;
		xOp.m_fRadius = fRadius;
		xOp.m_fStrength = fStrength;
		xOp.m_fValue = fValue;
		xPlan.PushBack(xOp);
	}

	void AppendSampledPath(Zenith_Vector<ZM_TerrainPlanOp>& xPlan,
		const ZM_TerrainPathSpec& xPath, float fSpacing, float fRadius,
		ZM_TERRAIN_DAB_KIND eKind, ZM_TERRAIN_PLAN_PHASE ePhase, float fValue)
	{
		for (u_int uSegment = 0; uSegment + 1u < xPath.m_uPointCount; ++uSegment)
		{
			const ZM_TerrainPoint2& xA = xPath.m_pxPoints[uSegment];
			const ZM_TerrainPoint2& xB = xPath.m_pxPoints[uSegment + 1u];
			const float fDX = xB.m_fX - xA.m_fX;
			const float fDZ = xB.m_fZ - xA.m_fZ;
			const float fLength = sqrtf(fDX * fDX + fDZ * fDZ);
			const u_int uIntervals = static_cast<u_int>(ceilf(fLength / fSpacing));
			const u_int uFirstSample = uSegment == 0u ? 0u : 1u;
			for (u_int uSample = uFirstSample; uSample <= uIntervals; ++uSample)
			{
				const float fT = static_cast<float>(uSample) / static_cast<float>(uIntervals);
				AppendDab(xPlan, eKind, ePhase,
					xA.m_fX + fDX * fT, xA.m_fZ + fDZ * fT,
					fRadius, 1.0f, fValue);
			}
		}
	}

	void AppendFlattenPass(const ZM_TerrainAuthoringRecipe& xRecipe,
		Zenith_Vector<ZM_TerrainPlanOp>& xPlan, ZM_TERRAIN_PLAN_PHASE ePhase)
	{
		for (u_int i = 0; i < xRecipe.m_uPathCount; ++i)
		{
			const ZM_TerrainPathSpec& xPath = xRecipe.m_pxPaths[i];
			const u_int uStart = xPlan.GetSize();
			AppendSampledPath(xPlan, xPath, xPath.m_fFlattenSpacing,
				xPath.m_fFlattenRadius, ZM_TERRAIN_DAB_FLATTEN, ePhase,
				xRecipe.m_fTargetHeight);
			Zenith_Assert(xPlan.GetSize() - uStart == xPath.m_uFlattenSampleCount,
				"Terrain flatten sample-count drift for %s", xPath.m_szName);
		}
		for (u_int i = 0; i < xRecipe.m_uPadCount; ++i)
		{
			const ZM_TerrainPadSpec& xPad = xRecipe.m_pxPads[i];
			AppendDab(xPlan, ZM_TERRAIN_DAB_FLATTEN, ePhase,
				xPad.m_xCentre.m_fX, xPad.m_xCentre.m_fZ,
				xPad.m_fFlattenRadius, 1.0f, xRecipe.m_fTargetHeight);
		}
	}

	void AppendDirtPaint(const ZM_TerrainAuthoringRecipe& xRecipe,
		Zenith_Vector<ZM_TerrainPlanOp>& xPlan)
	{
		for (u_int i = 0; i < xRecipe.m_uPathCount; ++i)
		{
			const ZM_TerrainPathSpec& xPath = xRecipe.m_pxPaths[i];
			for (u_int uPass = 0; uPass < uDIRT_PATH_PASSES; ++uPass)
			{
				const u_int uStart = xPlan.GetSize();
				AppendSampledPath(xPlan, xPath, xPath.m_fDirtSpacing,
					xPath.m_fDirtRadius, ZM_TERRAIN_DAB_SPLAT,
					ZM_TERRAIN_PHASE_DIRT, 2.0f);
				Zenith_Assert(xPlan.GetSize() - uStart == xPath.m_uDirtSampleCount,
					"Terrain dirt sample-count drift for %s", xPath.m_szName);
			}
		}
		for (u_int i = 0; i < xRecipe.m_uPadCount; ++i)
		{
			const ZM_TerrainPadSpec& xPad = xRecipe.m_pxPads[i];
			for (u_int uPass = 0; uPass < xPad.m_uDirtPassCount; ++uPass)
			{
				AppendDab(xPlan, ZM_TERRAIN_DAB_SPLAT, ZM_TERRAIN_PHASE_DIRT,
					xPad.m_xCentre.m_fX, xPad.m_xCentre.m_fZ,
					xPad.m_fDirtRadius, 1.0f, 2.0f);
			}
		}
	}

	void AppendGrass(const ZM_TerrainAuthoringRecipe& xRecipe,
		Zenith_Vector<ZM_TerrainPlanOp>& xPlan)
	{
		for (u_int i = 0; i < xRecipe.m_uGrassDabCount; ++i)
		{
			const ZM_TerrainGrassDabSpec& xDab = xRecipe.m_pxGrassDabs[i];
			AppendDab(xPlan, ZM_TERRAIN_DAB_GRASS_DENSITY, ZM_TERRAIN_PHASE_GRASS_FILL,
				xDab.m_xCentre.m_fX, xDab.m_xCentre.m_fZ,
				xDab.m_fRadius, 1.0f, xDab.m_fTargetDensity);
		}

		// Erasing the densely sampled paths and every flattened pad is the final
		// density phase. This prevents any later fill from repopulating walkways.
		for (u_int i = 0; i < xRecipe.m_uPathCount; ++i)
		{
			const ZM_TerrainPathSpec& xPath = xRecipe.m_pxPaths[i];
			AppendSampledPath(xPlan, xPath, xPath.m_fDirtSpacing,
				xPath.m_fFlattenRadius, ZM_TERRAIN_DAB_GRASS_DENSITY,
				ZM_TERRAIN_PHASE_GRASS_ERASE, 0.0f);
		}
		for (u_int i = 0; i < xRecipe.m_uPadCount; ++i)
		{
			const ZM_TerrainPadSpec& xPad = xRecipe.m_pxPads[i];
			AppendDab(xPlan, ZM_TERRAIN_DAB_GRASS_DENSITY,
				ZM_TERRAIN_PHASE_GRASS_ERASE,
				xPad.m_xCentre.m_fX, xPad.m_xCentre.m_fZ,
				xPad.m_fFlattenRadius, 1.0f, 0.0f);
		}
	}

	void AssertTerrainAuthoringPlanContained(
		const ZM_TerrainAuthoringRecipe& xRecipe,
		const Zenith_Vector<ZM_TerrainPlanOp>& xPlan)
	{
		Zenith_Assert(HasSupportedTerrainDimensions(xRecipe),
			"Terrain recipe dimensions must be valid, 64m chunks at 1m spacing");
		bool bGrassEraseSeen = false;
		for (u_int i = 0; i < xPlan.GetSize(); ++i)
		{
			const ZM_TerrainPlanOp& xOp = xPlan.Get(i);
			if (xOp.m_eType != ZM_TERRAIN_PLAN_BRUSH_DAB)
			{
				continue;
			}
			Zenith_Assert(xOp.m_fRadius >= 0.0f &&
				xOp.m_fWorldX - xOp.m_fRadius >= xRecipe.WorldMinX() &&
				xOp.m_fWorldX + xOp.m_fRadius <= xRecipe.WorldMaxX() &&
				xOp.m_fWorldZ - xOp.m_fRadius >= xRecipe.WorldMinZ() &&
				xOp.m_fWorldZ + xOp.m_fRadius <= xRecipe.WorldMaxZ(),
				"Terrain plan dab %u escapes the recipe's rectangular world bounds", i);

			if (xOp.m_eDabKind == ZM_TERRAIN_DAB_GRASS_DENSITY)
			{
				if (xOp.m_ePhase == ZM_TERRAIN_PHASE_GRASS_ERASE)
				{
					bGrassEraseSeen = true;
				}
				else
				{
					Zenith_Assert(!bGrassEraseSeen,
						"Terrain grass fill appears after the erase phase");
				}
			}
		}

		for (u_int i = 0; i < xRecipe.m_uLandmarkCount; ++i)
		{
			const ZM_TerrainPoint3& xPoint = xRecipe.m_pxLandmarks[i].m_xPosition;
			Zenith_Assert(xPoint.m_fX >= xRecipe.WorldMinX() &&
				xPoint.m_fX <= xRecipe.WorldMaxX() &&
				xPoint.m_fZ >= xRecipe.WorldMinZ() &&
				xPoint.m_fZ <= xRecipe.WorldMaxZ(),
				"Terrain landmark %u escapes the recipe's rectangular world bounds", i);
		}
	}

#ifdef ZENITH_TOOLS
	using TerrainBakeClock = std::chrono::steady_clock;
	TerrainBakeClock::time_point s_axTerrainBakeStart[uZM_TERRAIN_RECIPE_COUNT];
	bool s_abTerrainBakeTimingActive[uZM_TERRAIN_RECIPE_COUNT] = {};

	u_int GetTerrainBakeChunkCount(const ZM_TerrainAuthoringRecipe& xRecipe)
	{
		const u_int uRequiredOutputCount =
			ZM_GetTerrainRequiredOutputCount(xRecipe);
		return uRequiredOutputCount >= 4u ?
			(uRequiredOutputCount - 4u) / 3u : 0u;
	}

	void BeginTerrainBakeMeasurement(const ZM_TerrainAuthoringRecipe& xRecipe)
	{
		const int iRecipeIndex = FindTerrainRecipeIndexByIdentity(xRecipe);
		Zenith_Assert(iRecipeIndex >= 0,
			"Terrain measurement requires an immutable registry recipe");
		if (iRecipeIndex < 0)
		{
			return;
		}

		s_axTerrainBakeStart[iRecipeIndex] = TerrainBakeClock::now();
		s_abTerrainBakeTimingActive[iRecipeIndex] = true;
		const u_int uRequiredOutputCount =
			ZM_GetTerrainRequiredOutputCount(xRecipe);
		Zenith_Log(LOG_CATEGORY_TERRAIN,
			"[ZM Terrain Measure] set='%s', phase=BEGIN, chunks=%u, required_outputs=%u, family_files=%u",
			xRecipe.m_pxWorldSpec->m_szTerrainSet,
			GetTerrainBakeChunkCount(xRecipe), uRequiredOutputCount,
			uRequiredOutputCount + 1u);
	}

	void CompleteTerrainBakeMeasurement(const ZM_TerrainAuthoringRecipe& xRecipe,
		bool bSucceeded)
	{
		const int iRecipeIndex = FindTerrainRecipeIndexByIdentity(xRecipe);
		Zenith_Assert(iRecipeIndex >= 0,
			"Terrain measurement requires an immutable registry recipe");
		if (iRecipeIndex < 0)
		{
			return;
		}

		const bool bTimingActive = s_abTerrainBakeTimingActive[iRecipeIndex];
		Zenith_Assert(bTimingActive,
			"Terrain terminal bake completed without a measurement start");
		const long long llElapsedMilliseconds = bTimingActive ?
			static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
				TerrainBakeClock::now() - s_axTerrainBakeStart[iRecipeIndex]).count()) : 0ll;
		s_abTerrainBakeTimingActive[iRecipeIndex] = false;

		const u_int uRequiredOutputCount =
			ZM_GetTerrainRequiredOutputCount(xRecipe);
		Zenith_Log(LOG_CATEGORY_TERRAIN,
			"[ZM Terrain Measure] set='%s', elapsed_ms=%lld, chunks=%u, required_outputs=%u, family_files=%u, result=%s",
			xRecipe.m_pxWorldSpec->m_szTerrainSet, llElapsedMilliseconds,
			GetTerrainBakeChunkCount(xRecipe), uRequiredOutputCount,
			uRequiredOutputCount + 1u, bSucceeded ? "SUCCESS" : "FAILED");
	}

	template<ZM_SCENE_ID eSceneId>
	const ZM_TerrainAuthoringRecipe& GetFixedTerrainRecipe()
	{
		const ZM_TerrainAuthoringRecipe* pxRecipe =
			ZM_FindTerrainAuthoringRecipe(eSceneId);
		Zenith_Assert(pxRecipe != nullptr,
			"Fixed terrain callback has no immutable registry recipe");
		return *pxRecipe;
	}

	template<ZM_SCENE_ID eSceneId>
	void BeginFixedTerrainBakeMeasurement()
	{
		BeginTerrainBakeMeasurement(GetFixedTerrainRecipe<eSceneId>());
	}

	template<ZM_SCENE_ID eSceneId>
	void RunTerrainRegionErosion()
	{
		const ZM_TerrainAuthoringRecipe& xRecipe = GetFixedTerrainRecipe<eSceneId>();
		Zenith_TerrainErosionParams xParams;
		xParams.m_uSeed = xRecipe.m_uSeed;
		xParams.m_uHydraulicDroplets = xRecipe.m_xErosion.m_uHydraulicDroplets;
		xParams.m_uThermalIterations = xRecipe.m_xErosion.m_uThermalIterations;
		xParams.m_bRegionOnly = xRecipe.m_xErosion.m_bRegionOnly;
		xParams.m_fRegionCentreX = xRecipe.m_xErosion.m_xCentre.m_fX;
		xParams.m_fRegionCentreZ = xRecipe.m_xErosion.m_xCentre.m_fZ;
		xParams.m_fRegionRadius = xRecipe.m_xErosion.m_fRadius;
		g_xEngine.TerrainEditor().RunErosion(xParams, true);
	}

	template<ZM_SCENE_ID eSceneId>
	void RunTerrainTerminalBake()
	{
		const ZM_TerrainAuthoringRecipe& xRecipe = GetFixedTerrainRecipe<eSceneId>();
		Zenith_TerrainEditor& xEditor = g_xEngine.TerrainEditor();
		const char* szExpectedSet = xRecipe.m_pxWorldSpec->m_szTerrainSet;
		const u_int uRequiredOutputCount = ZM_GetTerrainRequiredOutputCount(xRecipe);
		const std::string strTerrainRoot =
			(std::filesystem::path(GAME_ASSETS_DIR) / "Terrain").string();
		bool bLeaseEntered = false;
		const char* szFailureStage = "asset-directory lease";
		// WithPreparedTerrainAssetDirectory takes a captureless thunk + opaque
		// context (it is a plain function pointer, not std::function), so the
		// lambda's captures move into this struct. The lease is synchronous, so
		// a stack context outlives the call.
		struct TerminalBakeContext
		{
			const ZM_TerrainAuthoringRecipe* m_pxRecipe;
			Zenith_TerrainEditor* m_pxEditor;
			const char* m_szExpectedSet;
			const std::string* m_pstrTerrainRoot;
			bool* m_pbLeaseEntered;
			const char** m_pszFailureStage;
		};
		TerminalBakeContext xBakeContext{ &xRecipe, &xEditor, szExpectedSet,
			&strTerrainRoot, &bLeaseEntered, &szFailureStage };
		const bool bSucceeded = Zenith_TerrainComponent::WithPreparedTerrainAssetDirectory(
			szExpectedSet, strTerrainRoot,
			[](void* pContext, const std::string& strPreparedDirectory) -> bool
			{
				TerminalBakeContext& xCtx = *static_cast<TerminalBakeContext*>(pContext);
				const ZM_TerrainAuthoringRecipe& xRecipe = *xCtx.m_pxRecipe;
				Zenith_TerrainEditor& xEditor = *xCtx.m_pxEditor;
				const char* szExpectedSet = xCtx.m_szExpectedSet;
				const std::string& strTerrainRoot = *xCtx.m_pstrTerrainRoot;
				const char*& szFailureStage = *xCtx.m_pszFailureStage;
				*xCtx.m_pbLeaseEntered = true;
				Zenith_Log(LOG_CATEGORY_TERRAIN,
					"[ZM Terrain] Terminal bake begin: stagedSet='%s', expectedSet='%s', output='%s', status='%s'",
					xEditor.GetAssetSet().c_str(), szExpectedSet,
					strPreparedDirectory.c_str(), xEditor.m_strStatus.c_str());

				bool bStepSucceeded = xEditor.GetAssetSet() == szExpectedSet;
				if (!bStepSucceeded)
				{
					szFailureStage = "staged asset-set validation";
					Zenith_Error(LOG_CATEGORY_TERRAIN,
						"[ZM Terrain] Staged-set mismatch before persistence: staged='%s', expected='%s', output='%s'",
						xEditor.GetAssetSet().c_str(), szExpectedSet,
						strPreparedDirectory.c_str());
				}
				if (bStepSucceeded)
				{
					bStepSucceeded = xEditor.SaveTextures();
					if (!bStepSucceeded)
					{
						szFailureStage = "SaveTextures";
						Zenith_Error(LOG_CATEGORY_TERRAIN,
							"[ZM Terrain] SaveTextures failed: stagedSet='%s', output='%s', status='%s'",
							xEditor.GetAssetSet().c_str(), strPreparedDirectory.c_str(),
							xEditor.m_strStatus.c_str());
					}
				}

				Flux_TerrainExportRect xRect;
				if (bStepSucceeded)
				{
					const ZM_TerrainExportRect xRecipeRect = xRecipe.ExportRect();
					bStepSucceeded = Flux_TerrainExportRect::TryCreate(
						xRecipeRect.m_iMinX, xRecipeRect.m_iMinY,
						xRecipeRect.m_iMaxX, xRecipeRect.m_iMaxY,
						xRect);
					if (!bStepSucceeded)
					{
						szFailureStage = "export-rectangle validation";
					}
				}
				if (bStepSucceeded)
				{
					bStepSucceeded = xEditor.BakeMeshesRect(xRect);
					if (!bStepSucceeded)
					{
						szFailureStage = "BakeMeshesRect";
						Zenith_Error(LOG_CATEGORY_TERRAIN,
							"[ZM Terrain] BakeMeshesRect failed: stagedSet='%s', output='%s', status='%s'",
							xEditor.GetAssetSet().c_str(), strPreparedDirectory.c_str(),
							xEditor.m_strStatus.c_str());
					}
				}
				if (bStepSucceeded)
				{
					bStepSucceeded = FinalizePreparedTerrainBake(
						xRecipe, strPreparedDirectory, strTerrainRoot);
					if (!bStepSucceeded)
					{
						szFailureStage = "required-output validation/manifest finalization";
						Zenith_Error(LOG_CATEGORY_TERRAIN,
							"[ZM Terrain] Output validation/finalization failed: stagedSet='%s', output='%s'",
							xEditor.GetAssetSet().c_str(), strPreparedDirectory.c_str());
					}
				}

				if (!bStepSucceeded && !RemovePreparedManifestFiles(strPreparedDirectory))
				{
					Zenith_Error(LOG_CATEGORY_TERRAIN,
						"[ZM Terrain] Failed to remove terminal manifest residue while target lease was held");
				}
				return bStepSucceeded;
			}, &xBakeContext);
		CompleteTerrainBakeMeasurement(xRecipe, bSucceeded);

		if (!bSucceeded)
		{
			if (!bLeaseEntered)
			{
				Zenith_Error(LOG_CATEGORY_TERRAIN,
					"[ZM Terrain] Terminal asset-directory lease rejected '%s'; target may have been replaced after preparation",
					szExpectedSet);
			}
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"%s terminal terrain bake failed at %s; completion marker remains absent",
				szExpectedSet, szFailureStage);
			Zenith_Assert(false,
				"Terrain bake failed before atomic manifest finalization for %s",
				szExpectedSet);
			return;
		}
		Zenith_Log(LOG_CATEGORY_TERRAIN,
			"[ZM Terrain] Terminal bake complete: set='%s', requiredOutputs=%u, manifest finalized",
			szExpectedSet, uRequiredOutputCount);
	}

	using TerrainAutomationCallback = void (*)();

	TerrainAutomationCallback ResolveTerrainBeginCallback(int iRecipeIndex)
	{
		switch (iRecipeIndex)
		{
		case 0: return &BeginFixedTerrainBakeMeasurement<ZM_SCENE_DAWNMERE>;
		case 1: return &BeginFixedTerrainBakeMeasurement<ZM_SCENE_THORNACRE>;
		case 2: return &BeginFixedTerrainBakeMeasurement<ZM_SCENE_ROUTE1>;
		default:
			Zenith_Assert(false, "Unknown immutable terrain recipe index");
			return nullptr;
		}
	}

	TerrainAutomationCallback ResolveTerrainErosionCallback(int iRecipeIndex)
	{
		switch (iRecipeIndex)
		{
		case 0: return &RunTerrainRegionErosion<ZM_SCENE_DAWNMERE>;
		case 1: return &RunTerrainRegionErosion<ZM_SCENE_THORNACRE>;
		case 2: return &RunTerrainRegionErosion<ZM_SCENE_ROUTE1>;
		default:
			Zenith_Assert(false, "Unknown immutable terrain recipe index");
			return nullptr;
		}
	}

	TerrainAutomationCallback ResolveTerrainTerminalCallback(int iRecipeIndex)
	{
		switch (iRecipeIndex)
		{
		case 0: return &RunTerrainTerminalBake<ZM_SCENE_DAWNMERE>;
		case 1: return &RunTerrainTerminalBake<ZM_SCENE_THORNACRE>;
		case 2: return &RunTerrainTerminalBake<ZM_SCENE_ROUTE1>;
		default:
			Zenith_Assert(false, "Unknown immutable terrain recipe index");
			return nullptr;
		}
	}

	int ResolveTerrainTool(ZM_TERRAIN_DAB_KIND eKind)
	{
		switch (eKind)
		{
		case ZM_TERRAIN_DAB_SET_HEIGHT: return static_cast<int>(Zenith_TerrainBrushTool::SetHeight);
		case ZM_TERRAIN_DAB_FLATTEN: return static_cast<int>(Zenith_TerrainBrushTool::Flatten);
		case ZM_TERRAIN_DAB_SPLAT: return static_cast<int>(Zenith_TerrainBrushTool::SplatPaint);
		case ZM_TERRAIN_DAB_GRASS_DENSITY: return static_cast<int>(Zenith_TerrainBrushTool::GrassDensity);
		default:
			Zenith_Assert(false, "Unknown Zenithmon terrain dab kind");
			return static_cast<int>(Zenith_TerrainBrushTool::Raise);
		}
	}
#endif
}

u_int ZM_Fnv1a32(const char* szText)
{
	u_int uHash = 2166136261u;
	if (szText == nullptr)
	{
		return uHash;
	}
	for (const u_int8* pByte = reinterpret_cast<const u_int8*>(szText);
		*pByte != 0u; ++pByte)
	{
		uHash ^= *pByte;
		uHash *= 16777619u;
	}
	return uHash;
}

u_int ZM_GetTerrainAuthoringRecipeCount()
{
	return uZM_TERRAIN_RECIPE_COUNT;
}

const ZM_TerrainAuthoringRecipe& ZM_GetTerrainAuthoringRecipe(u_int uIndex)
{
	Zenith_Assert(uIndex < uZM_TERRAIN_RECIPE_COUNT,
		"Terrain recipe index out of range (%u)", uIndex);
	return TerrainRecipeRegistry()[uIndex];
}

const ZM_TerrainAuthoringRecipe* ZM_FindTerrainAuthoringRecipe(ZM_SCENE_ID eSceneId)
{
	const ZM_TerrainAuthoringRecipe* pxRecipes = TerrainRecipeRegistry();
	for (u_int i = 0; i < uZM_TERRAIN_RECIPE_COUNT; ++i)
	{
		if (pxRecipes[i].m_pxWorldSpec->m_eId == eSceneId)
		{
			return &pxRecipes[i];
		}
	}
	return nullptr;
}

const ZM_TerrainAuthoringRecipe& ZM_GetDawnmereTerrainRecipe()
{
	return ZM_GetTerrainAuthoringRecipe(0u);
}

const ZM_TerrainAuthoringRecipe& ZM_GetThornacreTerrainRecipe()
{
	return ZM_GetTerrainAuthoringRecipe(1u);
}

const ZM_TerrainAuthoringRecipe& ZM_GetRoute1TerrainRecipe()
{
	return ZM_GetTerrainAuthoringRecipe(2u);
}

bool ZM_ParseTerrainBakeSelection(int iArgumentCount,
	const char* const* pszArguments, ZM_TerrainBakeSelection& xSelectionOut)
{
	xSelectionOut = ZM_TerrainBakeSelection{};
	if (iArgumentCount < 0 || (iArgumentCount > 1 && pszArguments == nullptr))
	{
		xSelectionOut.m_eParseResult = ZM_TERRAIN_BAKE_SELECTION_PARSE_MALFORMED;
		xSelectionOut.m_iErrorArgument = iArgumentCount > 1 ? 1 : -1;
		return false;
	}

	const std::size_t ulFlagLength = std::strlen(szZM_FORCE_TERRAIN_BAKE_FLAG);
	for (int iArgument = 1; iArgument < iArgumentCount; ++iArgument)
	{
		const char* szArgument = pszArguments[iArgument];
		if (szArgument == nullptr)
		{
			xSelectionOut.m_eParseResult = ZM_TERRAIN_BAKE_SELECTION_PARSE_MALFORMED;
			xSelectionOut.m_iErrorArgument = iArgument;
			return false;
		}
		if (std::strncmp(szArgument, szZM_FORCE_TERRAIN_BAKE_FLAG,
			ulFlagLength) != 0)
		{
			continue;
		}

		const char cSuffix = szArgument[ulFlagLength];
		if (cSuffix == '\0')
		{
			if (xSelectionOut.m_eMode == ZM_TERRAIN_BAKE_SELECTION_FORCE_ALL)
			{
				xSelectionOut.m_eParseResult =
					ZM_TERRAIN_BAKE_SELECTION_PARSE_DUPLICATE;
				xSelectionOut.m_iErrorArgument = iArgument;
				return false;
			}
			if (xSelectionOut.m_eMode == ZM_TERRAIN_BAKE_SELECTION_FORCE_SELECTED)
			{
				xSelectionOut.m_eParseResult =
					ZM_TERRAIN_BAKE_SELECTION_PARSE_CONFLICT;
				xSelectionOut.m_iErrorArgument = iArgument;
				return false;
			}
			xSelectionOut.m_eMode = ZM_TERRAIN_BAKE_SELECTION_FORCE_ALL;
			continue;
		}
		if (cSuffix != '=')
		{
			xSelectionOut.m_eParseResult = ZM_TERRAIN_BAKE_SELECTION_PARSE_MALFORMED;
			xSelectionOut.m_iErrorArgument = iArgument;
			return false;
		}

		const char* szSet = szArgument + ulFlagLength + 1u;
		if (!IsSafeTerrainSetName(szSet))
		{
			xSelectionOut.m_eParseResult = ZM_TERRAIN_BAKE_SELECTION_PARSE_MALFORMED;
			xSelectionOut.m_iErrorArgument = iArgument;
			return false;
		}

		int iRecipeIndex = -1;
		for (u_int i = 0; i < uZM_TERRAIN_RECIPE_COUNT; ++i)
		{
			const char* szRegisteredSet =
				ZM_GetTerrainAuthoringRecipe(i).m_pxWorldSpec->m_szTerrainSet;
			if (std::strcmp(szSet, szRegisteredSet) == 0)
			{
				iRecipeIndex = static_cast<int>(i);
				break;
			}
		}
		if (iRecipeIndex < 0)
		{
			xSelectionOut.m_eParseResult = ZM_TERRAIN_BAKE_SELECTION_PARSE_UNKNOWN_SET;
			xSelectionOut.m_iErrorArgument = iArgument;
			return false;
		}
		if (xSelectionOut.m_eMode == ZM_TERRAIN_BAKE_SELECTION_FORCE_ALL)
		{
			xSelectionOut.m_eParseResult = ZM_TERRAIN_BAKE_SELECTION_PARSE_CONFLICT;
			xSelectionOut.m_iErrorArgument = iArgument;
			return false;
		}

		const u_int uRecipeBit = 1u << static_cast<u_int>(iRecipeIndex);
		if ((xSelectionOut.m_uSelectedRecipeMask & uRecipeBit) != 0u)
		{
			xSelectionOut.m_eParseResult = ZM_TERRAIN_BAKE_SELECTION_PARSE_DUPLICATE;
			xSelectionOut.m_iErrorArgument = iArgument;
			return false;
		}
		xSelectionOut.m_eMode = ZM_TERRAIN_BAKE_SELECTION_FORCE_SELECTED;
		xSelectionOut.m_uSelectedRecipeMask |= uRecipeBit;
	}
	return true;
}

const char* ZM_TerrainBakeSelectionModeToString(
	ZM_TERRAIN_BAKE_SELECTION_MODE eMode)
{
	switch (eMode)
	{
	case ZM_TERRAIN_BAKE_SELECTION_AUTO_MISSING: return "AUTO_MISSING";
	case ZM_TERRAIN_BAKE_SELECTION_FORCE_ALL: return "FORCE_ALL";
	case ZM_TERRAIN_BAKE_SELECTION_FORCE_SELECTED: return "FORCE_SELECTED";
	default: return "INVALID";
	}
}

const char* ZM_TerrainBakeSelectionParseResultToString(
	ZM_TERRAIN_BAKE_SELECTION_PARSE_RESULT eResult)
{
	switch (eResult)
	{
	case ZM_TERRAIN_BAKE_SELECTION_PARSE_OK: return "OK";
	case ZM_TERRAIN_BAKE_SELECTION_PARSE_MALFORMED: return "MALFORMED";
	case ZM_TERRAIN_BAKE_SELECTION_PARSE_UNKNOWN_SET: return "UNKNOWN_SET";
	case ZM_TERRAIN_BAKE_SELECTION_PARSE_DUPLICATE: return "DUPLICATE";
	case ZM_TERRAIN_BAKE_SELECTION_PARSE_CONFLICT: return "CONFLICT";
	default: return "INVALID";
	}
}

ZM_TerrainBakeBatchPlan ZM_BuildTerrainBakeBatchPlan(
	const ZM_TerrainBakeSelection& xSelection, bool bHeadless,
	u_int uWarmRecipeMask)
{
	ZM_TerrainBakeBatchPlan xPlan;
	if (bHeadless ||
		xSelection.m_eParseResult != ZM_TERRAIN_BAKE_SELECTION_PARSE_OK)
	{
		return xPlan;
	}

	const u_int uAllRecipeMask = (1u << uZM_TERRAIN_RECIPE_COUNT) - 1u;
	xPlan.m_uWarmRecipeMask = uWarmRecipeMask & uAllRecipeMask;
	xPlan.m_bAllWarm = xPlan.m_uWarmRecipeMask == uAllRecipeMask;
	switch (xSelection.m_eMode)
	{
	case ZM_TERRAIN_BAKE_SELECTION_AUTO_MISSING:
		xPlan.m_uQueueRecipeMask = uAllRecipeMask & ~xPlan.m_uWarmRecipeMask;
		break;
	case ZM_TERRAIN_BAKE_SELECTION_FORCE_ALL:
		xPlan.m_uQueueRecipeMask = uAllRecipeMask;
		break;
	case ZM_TERRAIN_BAKE_SELECTION_FORCE_SELECTED:
		xPlan.m_uQueueRecipeMask =
			xSelection.m_uSelectedRecipeMask & uAllRecipeMask;
		break;
	default:
		return ZM_TerrainBakeBatchPlan{};
	}
	xPlan.m_bAuthorDawnmereScene =
		xPlan.m_bAllWarm && xPlan.m_uQueueRecipeMask == 0u;
	return xPlan;
}

void ZM_BuildTerrainAuthoringPlan(const ZM_TerrainAuthoringRecipe& xRecipe,
	Zenith_Vector<ZM_TerrainPlanOp>& xPlanOut)
{
	xPlanOut.Clear();
	// DIMENSIONS before everything, including the reset. The executor opens the
	// standalone session on the first terrain action and ResetImagesToDefaults
	// clears the CPU maps, not the staged spec -- so staging the shape first is
	// both safe and necessary: every coordinate below is world-space against it,
	// and the bake sizes its outputs from it.
	xPlanOut.PushBack(MakeSimpleOp(ZM_TERRAIN_PLAN_SET_DIMENSIONS));
	xPlanOut.PushBack(MakeSimpleOp(ZM_TERRAIN_PLAN_RESET));
	// RESET is deliberately second. The automation executor opens a standalone
	// session on the first terrain action; staging before that open relied on
	// session internals retaining the candidate. Staging immediately after the
	// clean reset makes the recipe's named set the unambiguous target for every
	// later action.
	xPlanOut.PushBack(MakeSimpleOp(ZM_TERRAIN_PLAN_SET_ASSET_SET));
	xPlanOut.PushBack(MakeSimpleOp(ZM_TERRAIN_PLAN_GENERATE_PROCEDURAL));

	for (u_int i = 0; i < xRecipe.m_uLandformCount; ++i)
	{
		const ZM_TerrainLandformSpec& xLandform = xRecipe.m_pxLandforms[i];
		AppendDab(xPlanOut, ZM_TERRAIN_DAB_SET_HEIGHT, ZM_TERRAIN_PHASE_LANDFORM,
			xLandform.m_xCentre.m_fX, xLandform.m_xCentre.m_fZ,
			xLandform.m_fRadius, xLandform.m_fStrength, xLandform.m_fHeight);
	}

	AppendFlattenPass(xRecipe, xPlanOut, ZM_TERRAIN_PHASE_FLATTEN_PRE_EROSION);
	xPlanOut.PushBack(MakeSimpleOp(ZM_TERRAIN_PLAN_EROSION));
	AppendFlattenPass(xRecipe, xPlanOut, ZM_TERRAIN_PHASE_FLATTEN_POST_EROSION);

	for (u_int i = 0; i < xRecipe.m_uAutoSplatCount; ++i)
	{
		xPlanOut.PushBack(MakeSimpleOp(ZM_TERRAIN_PLAN_AUTO_SPLAT_RULE, i));
	}
	xPlanOut.PushBack(MakeSimpleOp(ZM_TERRAIN_PLAN_RUN_AUTO_SPLAT));
	AppendDirtPaint(xRecipe, xPlanOut);
	AppendGrass(xRecipe, xPlanOut);
	xPlanOut.PushBack(MakeSimpleOp(ZM_TERRAIN_PLAN_TERMINAL_BAKE));
	AssertTerrainAuthoringPlanContained(xRecipe, xPlanOut);
}

u_int ZM_GetTerrainRequiredOutputCount(const ZM_TerrainAuthoringRecipe& xRecipe)
{
	const ZM_TerrainExportRect xRect = xRecipe.ExportRect();
	const int64_t iWidth = static_cast<int64_t>(xRect.m_iMaxX) -
		static_cast<int64_t>(xRect.m_iMinX) + 1;
	const int64_t iHeight = static_cast<int64_t>(xRect.m_iMaxY) -
		static_cast<int64_t>(xRect.m_iMinY) + 1;
	if (iWidth <= 0 || iHeight <= 0)
	{
		return 0u;
	}
	const uint64_t ulWidth = static_cast<uint64_t>(iWidth);
	const uint64_t ulHeight = static_cast<uint64_t>(iHeight);
	const uint64_t ulMaxChunks =
		(static_cast<uint64_t>(std::numeric_limits<u_int>::max()) - 3u) / 3u;
	if (ulWidth > ulMaxChunks / ulHeight)
	{
		return 0u;
	}
	// chunks x 3 mesh files, plus Height / Splatmap_RGBA / GrassDensity, plus the
	// TerrainDims.zdata manifest the runtime loader now REQUIRES.
	return static_cast<u_int>(ulWidth * ulHeight * 3u + 4u);
}

void ZM_EnumerateRequiredTerrainOutputs(const ZM_TerrainAuthoringRecipe& xRecipe,
	Zenith_Vector<std::string>& xOutputsOut)
{
	xOutputsOut.Clear();
	const u_int uRequiredOutputCount = ZM_GetTerrainRequiredOutputCount(xRecipe);
	Zenith_Assert(uRequiredOutputCount > 0u,
		"Terrain required-output count is invalid");
	if (uRequiredOutputCount == 0u)
	{
		return;
	}
	const std::filesystem::path xDirectory = RelativeTerrainDirectory(xRecipe);
	const char* aszPrefixes[] = { "Render", "Render_LOW", "Physics" };
	const ZM_TerrainExportRect xRect = xRecipe.ExportRect();
	for (u_int uPrefix = 0; uPrefix < CountOf(aszPrefixes); ++uPrefix)
	{
		for (int iY = xRect.m_iMinY; iY <= xRect.m_iMaxY; ++iY)
		{
			for (int iX = xRect.m_iMinX; iX <= xRect.m_iMaxX; ++iX)
			{
				const std::string strName = std::string(aszPrefixes[uPrefix]) + "_" +
					std::to_string(iX) + "_" + std::to_string(iY) + ZENITH_MESH_EXT;
				xOutputsOut.PushBack((xDirectory / strName).generic_string());
			}
		}
	}
	xOutputsOut.PushBack((xDirectory / (std::string("Height") + ZENITH_TEXTURE_EXT)).generic_string());
	xOutputsOut.PushBack((xDirectory / (std::string("Splatmap_RGBA") + ZENITH_TEXTURE_EXT)).generic_string());
	xOutputsOut.PushBack((xDirectory / (std::string("GrassDensity") + ZENITH_TEXTURE_EXT)).generic_string());
	// The dimensions manifest the exporter writes beside the chunks. A set
	// missing it is a stale bake the loader refuses, so it is a REQUIRED output
	// exactly like the chunk meshes.
	xOutputsOut.PushBack((xDirectory / Zenith_TerrainDimsManifestFormat::szFILENAME).generic_string());
	Zenith_Assert(xOutputsOut.GetSize() == uRequiredOutputCount,
		"Terrain required-output count drifted from manifest contract");
}

std::string ZM_GetTerrainManifestRelativePath(const ZM_TerrainAuthoringRecipe& xRecipe)
{
	return (RelativeTerrainDirectory(xRecipe) / szMANIFEST_NAME).generic_string();
}

std::string ZM_GetTerrainGrassAssetPath(const ZM_TerrainAuthoringRecipe& xRecipe)
{
	return std::string("game:") + (RelativeTerrainDirectory(xRecipe) /
		(std::string("GrassDensity") + ZENITH_TEXTURE_EXT)).generic_string();
}

ZM_TERRAIN_BAKE_QUEUE_RESULT ZM_DetermineTerrainBakeQueueResult(bool bHeadless,
	bool bForce, bool bWarm, bool bPrepareSucceeded)
{
	if (bHeadless)
	{
		return ZM_TERRAIN_BAKE_HEADLESS;
	}
	if (!bForce && bWarm)
	{
		return ZM_TERRAIN_BAKE_WARM;
	}
	return bPrepareSucceeded ? ZM_TERRAIN_BAKE_QUEUED :
		ZM_TERRAIN_BAKE_PREPARE_FAILED;
}

bool ZM_IsTerrainBakeWarm(const ZM_TerrainAuthoringRecipe& xRecipe,
	const std::filesystem::path& xGameAssetsRoot)
{
	const u_int uRequiredOutputCount = ZM_GetTerrainRequiredOutputCount(xRecipe);
	if (xGameAssetsRoot.empty() || uRequiredOutputCount == 0u ||
		!HasValidManifest(ManifestPath(xRecipe, xGameAssetsRoot), uRequiredOutputCount))
	{
		return false;
	}

	Zenith_Vector<std::string> xOutputs;
	ZM_EnumerateRequiredTerrainOutputs(xRecipe, xOutputs);
	for (u_int i = 0; i < xOutputs.GetSize(); ++i)
	{
		if (!IsNonEmptyFile(xGameAssetsRoot / xOutputs.Get(i)))
		{
			return false;
		}
	}
	return true;
}

#ifdef ZENITH_TOOLS
bool ZM_PrepareTerrainBake(const ZM_TerrainAuthoringRecipe& xRecipe,
	const std::filesystem::path& xGameAssetsRoot)
{
	if (xGameAssetsRoot.empty())
	{
		return false;
	}
	const std::string strTerrainRoot = (xGameAssetsRoot / "Terrain").string();
	return Zenith_TerrainComponent::WithPreparedTerrainAssetDirectory(
		xRecipe.m_pxWorldSpec->m_szTerrainSet, strTerrainRoot,
		[](void*, const std::string& strPreparedDirectory) -> bool
		{
			const std::filesystem::path xMarker =
				PreparedChildPath(strPreparedDirectory, szMANIFEST_NAME);
			std::filesystem::path xTemp = xMarker;
			xTemp += ".tmp";
			const std::filesystem::path axStalePaths[] =
			{
				xMarker,
				xTemp,
				PreparedChildPath(strPreparedDirectory, "Height" ZENITH_TEXTURE_EXT),
				PreparedChildPath(strPreparedDirectory, "Splatmap_RGBA" ZENITH_TEXTURE_EXT),
				PreparedChildPath(strPreparedDirectory, "GrassDensity" ZENITH_TEXTURE_EXT),
				PreparedChildPath(strPreparedDirectory, "GrassType" ZENITH_TEXTURE_EXT),
			};
			for (const std::filesystem::path& xPath : axStalePaths)
			{
				std::error_code xError;
				std::filesystem::remove(xPath, xError);
				if (xError)
				{
					// No automation has been queued yet. Marker-first ordering means a
					// later removal failure cannot leave an old bake looking complete.
					return false;
				}
			}
			return true;
		}, nullptr);
}

bool ZM_FinalizeTerrainBake(const ZM_TerrainAuthoringRecipe& xRecipe,
	const std::filesystem::path& xGameAssetsRoot)
{
	if (xGameAssetsRoot.empty())
	{
		return false;
	}

	const std::string strTerrainRoot = (xGameAssetsRoot / "Terrain").string();
	struct FinalizeBakeContext
	{
		const ZM_TerrainAuthoringRecipe* m_pxRecipe;
		const std::string* m_pstrTerrainRoot;
	};
	FinalizeBakeContext xFinalizeContext{ &xRecipe, &strTerrainRoot };
	return Zenith_TerrainComponent::WithPreparedTerrainAssetDirectory(
		xRecipe.m_pxWorldSpec->m_szTerrainSet, strTerrainRoot,
		[](void* pContext, const std::string& strPreparedDirectory) -> bool
		{
			FinalizeBakeContext& xCtx = *static_cast<FinalizeBakeContext*>(pContext);
			return FinalizePreparedTerrainBake(
				*xCtx.m_pxRecipe, strPreparedDirectory, *xCtx.m_pstrTerrainRoot);
		}, &xFinalizeContext);
}

ZM_TERRAIN_BAKE_QUEUE_RESULT ZM_QueueTerrainBake(
	Zenith_EditorAutomation& xAutomation,
	const ZM_TerrainAuthoringRecipe& xRecipe, bool bHeadless, bool bForce)
{
	const int iRecipeIndex = FindTerrainRecipeIndexByIdentity(xRecipe);
	if (iRecipeIndex < 0)
	{
		Zenith_Error(LOG_CATEGORY_TERRAIN,
			"[ZM Terrain] Queue rejected a recipe outside the immutable registry");
		Zenith_Assert(false,
			"Terrain automation requires a stable immutable registry recipe");
		return ZM_TERRAIN_BAKE_PREPARE_FAILED;
	}

	const char* szSet = xRecipe.m_pxWorldSpec->m_szTerrainSet;
	if (bHeadless)
	{
		// Headless policy is pure: do not inspect manifests or touch the assets
		// directory before returning this result.
		Zenith_Log(LOG_CATEGORY_TERRAIN,
			"[ZM Terrain] Queue result: set='%s', result=HEADLESS", szSet);
		return ZM_TERRAIN_BAKE_HEADLESS;
	}

	const std::filesystem::path xAssetsRoot(GAME_ASSETS_DIR);
	const bool bWarm = ZM_IsTerrainBakeWarm(xRecipe, xAssetsRoot);
	const ZM_TERRAIN_BAKE_QUEUE_RESULT eInitial =
		ZM_DetermineTerrainBakeQueueResult(false, bForce, bWarm, true);
	if (eInitial != ZM_TERRAIN_BAKE_QUEUED)
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN,
			"[ZM Terrain] Queue result: set='%s', force=%s, warm=%s, result=WARM",
			szSet, bForce ? "true" : "false", bWarm ? "true" : "false");
		return eInitial;
	}
	if (!ZM_PrepareTerrainBake(xRecipe, xAssetsRoot))
	{
		Zenith_Error(LOG_CATEGORY_TERRAIN,
			"[ZM Terrain] Queue result: set='%s', force=%s, warm=%s, "
			"result=PREPARE_FAILED; prior bake could not be invalidated",
			szSet, bForce ? "true" : "false", bWarm ? "true" : "false");
		return ZM_TERRAIN_BAKE_PREPARE_FAILED;
	}

	Zenith_Vector<ZM_TerrainPlanOp> xPlan;
	ZM_BuildTerrainAuthoringPlan(xRecipe, xPlan);
	xAutomation.AddStep_Custom(ResolveTerrainBeginCallback(iRecipeIndex));
	for (u_int i = 0; i < xPlan.GetSize(); ++i)
	{
		const ZM_TerrainPlanOp& xOp = xPlan.Get(i);
		switch (xOp.m_eType)
		{
		case ZM_TERRAIN_PLAN_SET_DIMENSIONS:
			xAutomation.AddStep_TerrainSetDimensions(
				xRecipe.m_xDims.m_fChunkWorldSize,
				xRecipe.m_xDims.VertexSpacing(),
				static_cast<int>(xRecipe.m_xDims.m_uGridChunksX),
				static_cast<int>(xRecipe.m_xDims.m_uGridChunksZ));
			break;
		case ZM_TERRAIN_PLAN_SET_ASSET_SET:
			xAutomation.AddStep_TerrainSetAssetSet(xRecipe.m_pxWorldSpec->m_szTerrainSet);
			break;
		case ZM_TERRAIN_PLAN_RESET:
			xAutomation.AddStep_TerrainResetSession();
			break;
		case ZM_TERRAIN_PLAN_GENERATE_PROCEDURAL:
			xAutomation.AddStep_TerrainGenerateProcedural(
				static_cast<int>(xRecipe.m_uSeed),
				xRecipe.m_xProcedural.m_fBaseHeight,
				xRecipe.m_xProcedural.m_fAmplitude,
				xRecipe.m_xProcedural.m_fFrequency,
				static_cast<int>(xRecipe.m_xProcedural.m_uOctaves),
				xRecipe.m_xProcedural.m_fLacunarity,
				xRecipe.m_xProcedural.m_fGain,
				xRecipe.m_xProcedural.m_fRidgedBlend);
			break;
		case ZM_TERRAIN_PLAN_BRUSH_DAB:
			xAutomation.AddStep_TerrainBrushStroke(ResolveTerrainTool(xOp.m_eDabKind),
				xOp.m_fWorldX, xOp.m_fWorldZ, xOp.m_fRadius,
				xOp.m_fStrength, xOp.m_fValue);
			break;
		case ZM_TERRAIN_PLAN_EROSION:
			xAutomation.AddStep_Custom(
				ResolveTerrainErosionCallback(iRecipeIndex));
			break;
		case ZM_TERRAIN_PLAN_AUTO_SPLAT_RULE:
		{
			const ZM_TerrainAutoSplatSpec& xRule = xRecipe.m_pxAutoSplat[xOp.m_uIndex];
			xAutomation.AddStep_TerrainAutoSplatRule(static_cast<int>(xOp.m_uIndex),
				xRule.m_fHeightMin, xRule.m_fHeightMax,
				xRule.m_fSlopeMin, xRule.m_fSlopeMax,
				xRule.m_fWeight, xRule.m_fJitter);
			break;
		}
		case ZM_TERRAIN_PLAN_RUN_AUTO_SPLAT:
			xAutomation.AddStep_TerrainRunAutoSplat();
			break;
		case ZM_TERRAIN_PLAN_TERMINAL_BAKE:
			xAutomation.AddStep_Custom(
				ResolveTerrainTerminalCallback(iRecipeIndex));
			break;
		default:
			Zenith_Assert(false, "Unknown Zenithmon terrain plan operation");
			break;
		}
	}
	Zenith_Log(LOG_CATEGORY_TERRAIN,
		"[ZM Terrain] Queue result: set='%s', force=%s, warm=%s, result=QUEUED, planOps=%u",
		szSet, bForce ? "true" : "false", bWarm ? "true" : "false", xPlan.GetSize());
	return ZM_TERRAIN_BAKE_QUEUED;
}

ZM_TERRAIN_BAKE_QUEUE_RESULT ZM_QueueDawnmereTerrainBake(
	Zenith_EditorAutomation& xAutomation, bool bHeadless, bool bForce)
{
	return ZM_QueueTerrainBake(
		xAutomation, ZM_GetDawnmereTerrainRecipe(), bHeadless, bForce);
}
#endif
