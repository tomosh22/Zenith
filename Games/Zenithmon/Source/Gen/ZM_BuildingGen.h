#pragma once

// ============================================================================
// ZM_BuildingGen -- the S4 procedural BUILDING-asset generator: it turns a
// ZM_BUILDING_ID into a deterministic building bundle (four STATIC surface
// meshes + a full PBR map set for each) and (in tools builds) bakes it to disk
// under the ZM_BuildingAssetPath scheme.
//
// STATIC, unlike creatures/humans: buildings have NO skeleton and NO animation.
// Every mesh carries zero bones and byte-empty skin buffers, and lofts through
// ZM_StaticMesh::AppendBox / the roof emitters (NOT the bone-binding ring loft).
// No .zskel, no .zanim.
//
// DETERMINISM (AssetManifest 6.2): every output byte is a pure function of the
// building id. Randomness reaches a builder ONLY through ZM_MakeGenRNG over the
// recipe's pre-derived m_aulDomainSeed[] -- MESH for shape jitter, ALBEDO for
// every texel. Same id => byte-identical bundle, proved by ZM_BuildingBuildEqual
// / ZM_BuildingContentHash.
//
// GUARD MODEL (mirrors ZM_HumanGen / ZM_GenCommon): the pure generation API below
// compiles in ALL configs so the in-memory ZM_Gen unit gate exercises it headless.
// Only the disk bake at the very end is #ifdef ZENITH_TOOLS, with a non-tools
// no-op so _False builds link.
//
// ★ WHAT THIS MODULE IS FOR, AND WHAT IT REPLACED. It used to emit ONE box, ONE
// roof and ONE flat 256^2 albedo, which is a blockout with a picture on it. The
// two Dawnmere buildings are its first real consumers, and a facade a player
// walks up to has to survive being looked at: it now carries a plinth, quoins,
// recessed windows with frames and sills, a door surround, an eave fascia and a
// chimney as GEOMETRY, and four tiling PBR maps per surface class as material.
// Improving the generator rather than special-casing two buildings is deliberate
// -- all 30 sets, the gyms and the care centre included, gained the same detail.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_GenCommon.h"       // ZM_GenMesh, ZM_GEN_DOMAIN, ZM_GenRNG, ZM_StaticMesh, static validator
#include "Zenithmon/Source/Gen/ZM_TextureSynth.h"     // ZM_GenImage (the placeholder facade type)
#include "Zenithmon/Source/Data/ZM_BuildingData.h"    // ZM_BUILDING_ID + the roster enums

// ZM_BakeManifest (a later box) stamps this per-family version; bump it whenever
// this module's generation algorithms change so stale bakes self-invalidate.
//
// 2: the facade overhaul. One box + one flat 256^2 albedo became a SURFACE-SPLIT
// model (wall / roof / trim / glass, each its own mesh + material) carrying real
// architectural relief and a full PBR map set. Every building's bytes moved.
// 3: the door leaf stopped being coplanar with the wall it is applied to (it
// z-fought on every front door). Trim geometry moved, so every bundle is stale.
// 4: albedo bakes stamped BC1_RGB_SRGB rather than UNORM (ZM_TextureSynth v3).
// 5: the photorealism pass. Windows and the door are REAL OPENINGS with reveals,
// glazing bars, a recessed leaf, a threshold and a handle; the roof is coursed
// slate with a ridge and barge boards; every box edge is chamfered; the wall
// tile is the STOREY HEIGHT so weathering anchors to the real plinth, sill and
// eave rows; wall/roof maps are 512^2 with a baked HEIGHT slot (POM) and the
// shared micro-detail pair. Every mesh and every map moved.
constexpr u_int uZM_BUILDINGGEN_VERSION           = 5u;

// Buildings have no evolution, so the seed-derivation evo-stage slot is a fixed
// constant (keeps ZM_GenDeriveSeed's signature shared with creatures/humans).
constexpr u_int uZM_BUILDING_SYNTHETIC_EVO_STAGE  = 1u;

// ---------------------------------------------------------------------------
// SURFACE CLASSES -- the unit of material assignment.
//
// ★ WHY A BUILDING IS FOUR MESHES AND NOT ONE. The shipped generator packed an
// entire building into a single mesh sharing a single atlas, which forced two
// compromises that no amount of extra texture resolution can buy back:
//
//   TEXEL DENSITY. Every face mapped to the SAME [0,1] sub-rect, so a 16.5 m wall
//   and a 0.9 m window sill each got the whole island. Density therefore varied by
//   more than an order of magnitude across one building, which is exactly the
//   "correct material at the wrong scale reads as plastic" failure -- and the
//   worst-affected surface was the largest one in frame.
//
//   MATERIAL RESPONSE. Plaster, slate, painted timber and glass want different
//   roughness, different metallic and different normal strength. One material
//   means one answer, so the glass was as matte as the render and the slate as dry
//   as the plaster.
//
// Splitting by surface fixes both at once, and it costs nothing at runtime that a
// multi-material model was not already going to pay: Zenith_ModelAsset takes a
// mesh + material pair per AddMeshByPath call.
//
// ★ AND IT IS WHAT MAKES TILING UVs SAFE. Wall UVs are world-scaled
// (u = metres / tile) and therefore leave [0,1] -- legal only because the wall
// mesh's material is a tiling one. A shared atlas could not do that without
// bleeding the roof into the wall at every wrap.
enum ZM_BUILDING_SURFACE : u_int
{
	ZM_BUILDING_SURFACE_WALL,    // plinth + storey body + quoins + string course
	ZM_BUILDING_SURFACE_ROOF,    // roof planes + ridge + chimney cap
	ZM_BUILDING_SURFACE_TRIM,    // window frames/sills/mullions, door surround + leaf, fascia, chimney stack
	ZM_BUILDING_SURFACE_GLASS,   // window panes

	ZM_BUILDING_SURFACE_COUNT
};

// Stable lowercase suffix for the surface's asset stems ("wall"/"roof"/...).
// TOTAL: an out-of-range surface returns "wall" and says so with a Zenith_Error.
const char* ZM_BuildingSurfaceName(ZM_BUILDING_SURFACE eSurface);

// Per-surface map resolution. Every one of these textures TILES, so density is
// set by the tile size in metres and not by the pixel count: 512^2 on a 3 m wall
// tile is 170 px/m, and the shared micro-detail pair carries the sub-centimetre
// grain past that. Glass is flat and needs none of it. Nothing is above 512^2 --
// a 512^2 BC1 with mips is ~170 KB per map and the budget is ratchet-only.
u_int ZM_BuildingSurfaceResolution(ZM_BUILDING_SURFACE eSurface);

// NOMINAL world size of one texture repeat, in metres, per surface -- the value
// the course lattices are designed around. For ROOF / TRIM / GLASS it is the
// tile. For WALL the real tile is the building's STOREY HEIGHT (below); the
// nominal 3 m is the roster's common storey and is what a recipe-less caller
// (a table test) sees.
float ZM_BuildingSurfaceTileMetres(ZM_BUILDING_SURFACE eSurface);

// ★ THE WALL TILE IS THE STOREY HEIGHT, AND THAT IS WHAT LETS WEATHERING READ
// AS GRAVITY. A wall's V coordinate is worldY / tile, so with the tile equal to
// the storey the texture's V axis IS height-within-the-storey: v = 0 is the
// floor line (the plinth on the ground storey, the string course above), the
// sill row sits at v = 0.95 / storey on every facade, and v -> 1 is the eave
// (or the string course under the storey above). Splash-back, sill drips and
// eave runoff can then be painted at the rows they really form on, instead of
// as a per-tile gradient that repeats one and a half times per storey. Jittered
// with the shell (a free row's storey is +/-3%), site-fixed rows use the roster's.
struct ZM_BuildingRecipe;
float ZM_BuildingSurfaceTileMetresFor(const ZM_BuildingRecipe& xR, ZM_BUILDING_SURFACE eSurface);

// Masonry courses per wall tile at the 250 mm gauge, and units per course at the
// 500 mm gauge -- computed from the tile so a 3.5 m storey gets 14 courses and a
// 3.0 m one 12, rather than one stretched lattice.
u_int ZM_BuildingWallCourseRows(float fTileMetres);
u_int ZM_BuildingWallCourseCols(float fTileMetres);

// ---- Architectural proportions, as named constants -------------------------
// Every one is a fraction of a dimension the recipe already carries, so a
// building of any size keeps its proportions. None is golden; they are art.
constexpr float fZM_BUILDING_PLINTH_HEIGHT   = 0.45f;  // metres of exposed foundation
constexpr float fZM_BUILDING_PLINTH_PROUD    = 0.12f;  // how far the plinth stands out from the wall
constexpr float fZM_BUILDING_QUOIN_WIDTH     = 0.40f;  // corner pilaster width
constexpr float fZM_BUILDING_QUOIN_PROUD     = 0.07f;
constexpr float fZM_BUILDING_EAVE_OVERHANG   = 0.30f;  // roof footprint beyond the wall
constexpr float fZM_BUILDING_FASCIA_HEIGHT   = 0.22f;  // eave board depth
constexpr float fZM_BUILDING_FASCIA_PROUD    = 0.05f;
constexpr float fZM_BUILDING_WINDOW_WIDTH    = 1.10f;
constexpr float fZM_BUILDING_WINDOW_HEIGHT   = 1.30f;
constexpr float fZM_BUILDING_WINDOW_SILL_Y   = 0.95f;  // sill height above each storey floor
constexpr float fZM_BUILDING_FRAME_THICK     = 0.10f;  // window frame member thickness
constexpr float fZM_BUILDING_FRAME_PROUD     = 0.06f;
constexpr float fZM_BUILDING_SILL_PROUD      = 0.14f;
constexpr float fZM_BUILDING_SILL_HEIGHT     = 0.09f;
constexpr float fZM_BUILDING_GLASS_INSET     = 0.05f;  // the windowless fanlight's stand-off from the wall
// ★ WINDOWS ARE HOLES. The wall body used to be one solid box with the pane
// glued to its face, which reads as a decal from any angle that is not
// head-on: no jamb, no shadow, no depth. The facade slab is now cut into panels
// around every opening, and the opening is lined with a REVEAL -- jamb, head and
// cill returns this deep -- with the glass set back at the reveal and a dark
// room-behind card behind it. A player looking along a wall sees the reveals
// foreshorten, which is most of what makes a facade three-dimensional.
constexpr float fZM_BUILDING_REVEAL_DEPTH    = 0.15f;  // how deep the facade slab (and so every opening) is
constexpr float fZM_BUILDING_REVEAL_LINING   = 0.03f;  // thickness of the trim lining inside an opening
constexpr float fZM_BUILDING_GLASS_SETBACK   = 0.05f;  // pane stands this far in front of the pocket back
constexpr float fZM_BUILDING_GLASS_THICK     = 0.006f;
constexpr float fZM_BUILDING_ROOM_CARD_GAP   = 0.002f; // the dark card floats this far off the pocket back
constexpr float fZM_BUILDING_ROOM_CARD_THICK = 0.010f;
constexpr float fZM_BUILDING_GLAZING_BAR     = 0.025f; // glazing-bar width, square section
constexpr float fZM_BUILDING_GLAZING_BAR_OUT = 0.020f; // ...standing this far in front of the glass
constexpr float fZM_BUILDING_DOOR_SURROUND   = 0.22f;
// ★ THE SURROUND MUST STAND OFF THE WALL FAR ENOUGH TO CAST A SHADOW. At 0.05 m
// the first render put a correct 4 m doorway on the Home that was invisible: the
// jambs were coplanar with the masonry to within a few centimetres and the trim
// palette sits close to the wall palette, so the entrance read as blank wall. A
// door is the one feature on a house a player is looking for.
constexpr float fZM_BUILDING_DOOR_PROUD      = 0.14f;
// ★★ THE LEAF IS RECESSED INTO A REAL OPENING. Version 3 stood it proud of a
// solid wall because there was no opening to recess into, and a leaf spanning
// inward from the wall plane put its outward face EXACTLY COPLANAR with the
// masonry -- it z-fought on both Dawnmere buildings. The doorway is a pocket now,
// fZM_BUILDING_REVEAL_DEPTH deep and lined like a window, and the leaf sits at
// its back: its face is a full reveal behind the wall plane, the surround's
// ledge and the pocket's jambs both cast onto it, and no face of it shares a
// plane with anything (BuildingGen_NoFrontFaceIsCoplanarWithTheWall).
constexpr float fZM_BUILDING_DOOR_LEAF_THICK = 0.05f;
constexpr float fZM_BUILDING_DOOR_LEAF_EMBED = 0.02f;  // buried into the core slab so its back is never seen
constexpr float fZM_BUILDING_DOOR_LEAF_GAP   = 0.012f; // between the two leaves of a double door
constexpr float fZM_BUILDING_DOUBLE_DOOR_MIN = 1.80f;  // a leaf wider than this splits into a pair
constexpr float fZM_BUILDING_THRESHOLD_H     = 0.08f;  // the stone step the leaf stands on
constexpr float fZM_BUILDING_HANDLE_Y        = 1.02f;
constexpr float fZM_BUILDING_HANDLE_LEN      = 0.14f;
constexpr float fZM_BUILDING_HANDLE_SIDE     = 0.03f;
constexpr float fZM_BUILDING_HANDLE_PROUD    = 0.05f;
constexpr float fZM_BUILDING_HANDLE_INSET    = 0.12f;  // from the leaf's meeting edge
// ---- The roof is COURSED, not two planes. ----
// A pitched roof is rows of overlapping slates: each course lies on the one
// below it and its lower edge stands proud, so a grazing sun draws a shadow line
// per course and the silhouette against the sky is stepped, not ruled. Courses
// are laid up the slope at the gauge, each with its own +/- jitter so the rows
// are not machine-parallel, under a ridge run and behind barge boards.
constexpr float fZM_BUILDING_ROOF_GAUGE      = 0.26f;  // course spacing along the slope
constexpr float fZM_BUILDING_ROOF_LAP        = 0.04f;  // each course reaches this far under the one above
constexpr float fZM_BUILDING_ROOF_LIP        = 0.014f; // how proud a course's lower edge stands
constexpr float fZM_BUILDING_ROOF_JITTER     = 0.003f; // per-course +/- offset normal to the pitch
constexpr float fZM_BUILDING_RIDGE_HALF      = 0.09f;  // ridge cap half-width
constexpr float fZM_BUILDING_RIDGE_RISE      = 0.05f;  // ridge cap above the apex
constexpr float fZM_BUILDING_BARGE_DROP      = 0.18f;  // barge board depth, measured vertically
constexpr float fZM_BUILDING_BARGE_THICK     = 0.035f;
// ---- Weathering rows, in metres of world height within a storey. ----
constexpr float fZM_BUILDING_SPLASH_HEIGHT   = 0.40f;  // splash-back darkening above the plinth
constexpr float fZM_BUILDING_RUNOFF_HEIGHT   = 0.30f;  // runoff darkening under the eave / string course
constexpr float fZM_BUILDING_DRIP_LENGTH     = 0.70f;  // how far a sill drip streak runs down
constexpr float fZM_BUILDING_CHIMNEY_SIDE    = 0.80f;
constexpr float fZM_BUILDING_CHIMNEY_RISE    = 1.10f;  // above the ridge
constexpr float fZM_BUILDING_PARAPET_HEIGHT  = 0.40f;
// ---------------------------------------------------------------------------
// ZM_BuildingRecipe -- the fully resolved per-building generation inputs. Pure
// data; ZM_ResolveBuildingRecipe fills it deterministically from ZM_BuildingData.
// ---------------------------------------------------------------------------
struct ZM_BuildingRecipe
{
	ZM_BUILDING_ID      m_eId            = ZM_BUILDING_HOUSE_COTTAGE_WARM;
	u_int               m_uSyntheticSeed = 0u;   // family seed == ZM_GenHashName(m_szName)

	// One independent 64-bit PCG seed per generation domain (ZM_GenDeriveSeed).
	// The full array is derived so a builder can index any domain without a gap;
	// SC1 draws none, SC2/SC3 draw MESH + ALBEDO.
	u_int64             m_aulDomainSeed[ZM_GEN_DOMAIN_COUNT] = {};

	ZM_BUILDING_STYLE   m_eStyle         = ZM_BUILDING_STYLE_COTTAGE;
	ZM_BUILDING_PALETTE m_ePalette       = ZM_BUILDING_PALETTE_WARM;
	ZM_ROOF_KIND        m_eRoof          = ZM_ROOF_GABLE;
	float               m_fWidth = 6.0f, m_fDepth = 5.0f, m_fStoreyHeight = 3.0f;
	float               m_fRoofPitch     = 0.70f;   // rise / shorter footprint half-extent
	u_int               m_uStoreys = 1u, m_uWindowCols = 2u, m_uWindowRows = 1u;
	ZM_TYPE             m_eThemeType     = ZM_TYPE_NONE;
	bool                m_bSiteFixed     = false;   // suppress shape jitter (see ZM_BuildingData.h)
	float               m_fDoorWidth     = 1.30f, m_fDoorHeight = 2.20f;
};

// Resolve a building id into its full generation recipe (bounds-asserted id).
ZM_BuildingRecipe ZM_ResolveBuildingRecipe(ZM_BUILDING_ID eId);

// Seed a domain's generation RNG from a resolved recipe. THE single entry point
// through which randomness reaches any builder (keeps the determinism invariant
// auditable).
ZM_GenRNG ZM_MakeGenRNG(const ZM_BuildingRecipe& xR, ZM_GEN_DOMAIN eDomain);

// ---------------------------------------------------------------------------
// ZM_BuildingTextureSet -- the four maps every surface material carries.
//
// PBR through the shipped uber-shader (.zmtrl v5, MRT3): base colour, a tangent-
// space normal (baked BC5), a packed roughness/metallic, and an ambient
// occlusion. The engine has SSGI/SSAO/IBL/CSM and will happily light a surface
// that only declares a base colour -- it will just light it like painted card.
//
// ★ ROUGHNESS AND METALLIC SHARE ONE IMAGE. Zenith_MaterialAsset has a single
// MATERIAL_TEXTURE_ROUGHNESS_METALLIC slot, so the two travel packed rather than
// as separate files. Roughness is the G channel and metallic the B, matching the
// glTF convention the rest of the pipeline follows.
// ---------------------------------------------------------------------------
struct ZM_BuildingTextureSet
{
	ZM_GenImage m_xAlbedo;
	ZM_GenImage m_xNormal;              // tangent-space, from the height field
	ZM_GenImage m_xRoughnessMetallic;   // G = roughness, B = metallic
	ZM_GenImage m_xOcclusion;           // R = AO
	// ★ THE HEIGHT FIELD ITSELF, BAKED. It used to be computed, differentiated
	// into the normal map and thrown away -- and a normal map alone cannot move a
	// silhouette. Bound to the material's HEIGHT slot with a non-zero height scale
	// the engine parallax-occlusion-maps it (Flux_MaterialGPU), so mortar beds and
	// slate laps actually recede as the camera moves. R = height, 1 = surface top
	// (the shader marches depth = 1 - height).
	ZM_GenImage m_xHeight;

	bool Equals(const ZM_BuildingTextureSet& xOther) const;
	bool NonEmpty() const;
};

// ---------------------------------------------------------------------------
// ZM_BuildingShellMetrics -- the resolved geometry every surface builder reads.
//
// ★ THIS EXISTS SO THE FOUR SURFACES CANNOT DISAGREE. A window's glass pane, its
// frame and the wall opening behind it are emitted by three different builders;
// derived independently they would drift the first time a proportion changed, and
// the symptom would be glass floating a centimetre out of its hole -- visible
// only in a screenshot nobody takes headless.
//
// ★ AND IT IS WHERE THE SHAPE JITTER IS APPLIED OR SUPPRESSED, once, so that
// "site-fixed means the footprint is exactly the roster's" is one branch rather
// than four.
// ---------------------------------------------------------------------------
struct ZM_BuildingShellMetrics
{
	float m_fWidth = 0.0f, m_fDepth = 0.0f, m_fStoreyHeight = 0.0f;
	float m_fHalfW = 0.0f, m_fHalfD = 0.0f;
	float m_fWallTop = 0.0f;      // eave height: storeys * storeyHeight
	float m_fRise    = 0.0f;      // roof rise above the eave (0 for FLAT)
	float m_fRidgeY  = 0.0f;      // wallTop + rise, or + parapet for FLAT
	float m_fExW = 0.0f, m_fExD = 0.0f;   // overhang-expanded eave footprint
	float m_fDoorWidth = 0.0f, m_fDoorHeight = 0.0f;
	u_int m_uStoreys = 1u, m_uWindowCols = 1u, m_uWindowRows = 1u;
	bool  m_bHasChimney = false;

	// Centre X of window column c on a +/-Z facade, and the sill Y of row r.
	float WindowCentreX(u_int uCol) const;
	float WindowSillY  (u_int uRow) const;
};
ZM_BuildingShellMetrics ZM_ResolveBuildingShellMetrics(const ZM_BuildingRecipe& xR);

// ---------------------------------------------------------------------------
// Per-output builders (pure functions of the recipe). Each is separately
// unit-testable.
// ---------------------------------------------------------------------------

// Build ONE surface's mesh. Every surface is emitted from the SAME resolved
// ZM_BuildingShellMetrics, so the four meshes register with each other by
// construction rather than by four sets of parallel arithmetic.
//
// Wall/roof/trim UVs are WORLD-SCALED (metres / ZM_BuildingSurfaceTileMetres) and
// therefore leave [0,1]; validate them with ZM_ValidateBuildingSurfaceMesh, not
// with the [0,1]-clamped static validator.
void ZM_BuildBuildingSurfaceMesh(const ZM_BuildingRecipe& xR,
	ZM_BUILDING_SURFACE eSurface, ZM_GenMesh& xMesh);

// The height field a surface's normal + AO maps are derived from. Exposed
// because it is the one input both of them share, and a unit that wants to prove
// "the relief is not flat" should read the source rather than a derivative.
ZM_GenImage ZM_BuildBuildingSurfaceHeight(const ZM_BuildingRecipe& xR,
	ZM_BUILDING_SURFACE eSurface);

// Build one surface's full PBR map set. ALL randomness is drawn from the ALBEDO
// domain ONLY, so a MESH-seed change can never perturb a texel.
ZM_BuildingTextureSet ZM_BuildBuildingSurfaceTextures(const ZM_BuildingRecipe& xR,
	ZM_BUILDING_SURFACE eSurface);

// Per-surface material response. These are the numbers that make slate read as
// slate and glass as glass; the maps modulate them.
struct ZM_BuildingSurfaceResponse
{
	float m_fRoughness      = 0.8f;
	float m_fMetallic       = 0.0f;
	float m_fNormalStrength = 1.0f;
	float m_fOcclusion      = 1.0f;
	// Parallax height scale (UV units per full height range). 0 disables POM --
	// glass carries a height map for the kind walk but has no relief to march.
	float m_fHeightScale    = 0.0f;
	// Repeats of the shared micro-detail pair per texture tile. 0 = no detail
	// maps bound (glass, and trim whose grain is already at texel scale).
	float m_fDetailTiling   = 0.0f;
	// Edge-wear strength handed to ZM_SynthBuildPbrSet (0 disables).
	float m_fEdgeWear       = 0.0f;
};
ZM_BuildingSurfaceResponse ZM_BuildingSurfaceMaterialResponse(ZM_BUILDING_SURFACE eSurface);

// The glass material's emissive: a faint warm glow for a lived-in house so it
// reads inhabited at dusk. Intensity 0 for everything that is not a home.
struct ZM_BuildingGlassGlow
{
	Zenith_Maths::Vector3 m_xColour    = Zenith_Maths::Vector3(1.0f, 0.85f, 0.60f);
	float                 m_fIntensity = 0.0f;
};
ZM_BuildingGlassGlow ZM_BuildingGlassGlowFor(const ZM_BuildingRecipe& xR);

// ---------------------------------------------------------------------------
// Wall weathering, anchored to WORLD HEIGHT within a storey (see
// ZM_BuildingSurfaceTileMetresFor). All three terms are in [0,1], are pure
// functions of their arguments (no RNG -- the streak layout hashes off uSalt)
// and are exposed so a unit can prove they land on the rows they claim to:
//   m_fSplash  -- 1 at the plinth top, falling to 0 fZM_BUILDING_SPLASH_HEIGHT
//                 above it (rain bouncing off the ground and the plinth ledge)
//   m_fRunoff  -- 0 until fZM_BUILDING_RUNOFF_HEIGHT below the tile top, rising
//                 to 1 at it (water shed off the eave / string course)
//   m_fDrip    -- vertical streaks starting under the sill row and fading over
//                 fZM_BUILDING_DRIP_LENGTH; 0 above the sill
// ---------------------------------------------------------------------------
struct ZM_BuildingWeatherTerms
{
	float m_fSplash = 0.0f;
	float m_fRunoff = 0.0f;
	float m_fDrip   = 0.0f;
};
ZM_BuildingWeatherTerms ZM_BuildingWallWeatherAt(float fU, float fV, float fTileMetres,
	u_int uSalt, float fDripDensity);

// ---------------------------------------------------------------------------
// THE TWO GRIME COLOURS, and the field that chooses between them.
//
// ★ WHY THERE ARE TWO. Weather leaves two different stains on a building and
// they do not look alike: warm mineral dust and soot on the faces the sun and
// the road reach, and cool green-black biological growth (algae, moss, lichen)
// where it stays damp. A facade that carries only one of them reads as "dirty
// texture" rather than as weathered stone -- and the difference has to be big
// enough to survive being mixed at 55% into a base colour, which the first
// version's pair was not: (0.30,0.27,0.22) and (0.13,0.17,0.13) differ by only
// 0.07 in (G-R), so on a warm render wall the two were 0.006 apart in the
// grimiest decile. MEASURED on the shipped pair: they differ by 0.140 in (G-R)
// and 0.156 in luma, and the biological one is both greener and darker.
Zenith_Maths::Vector3 ZM_BuildingGrimeColour(bool bBiological);

// The per-texel blend: 0 = all warm dust, 1 = all cool biological. A slow
// wrapping noise about the building's own drawn share, so one wall carries both
// and neighbouring walls carry different amounts.
//
// ★ THE SWING IS +/-0.8, WHICH SATURATES ON PURPOSE. At the first version's
// +/-0.4 the mix MEASURED 0.40-0.92 on a share of 0.58 -- neither end was ever
// reached, so "two grime colours" was really one blend everywhere and the
// second colour existed only in the source. At +/-0.8 every roster building's
// wall spans the full 0.00-1.00.
float ZM_BuildingWallGrimeBioMixAt(float fU, float fV, u_int uSalt, float fBioShare);

// ---------------------------------------------------------------------------
// ZM_BuildingAlbedoDraws -- the ALBEDO domain's per-building random draws, in
// the ONE place they are drawn.
//
// ★ THE DRAW ORDER IS THE DETERMINISM CONTRACT, so it lives in a single
// function rather than inline in the texture builder: every value is drawn
// up-front, in a FIXED count and order, BEFORE any surface branch, and a new
// one is APPENDED at the end. Exposing it also lets a unit assert what the
// stream consumes (the MESH domain has had that guard since the shell jitter
// shipped; this is its counterpart) and lets a test find, say, the roster row
// whose grime is most evenly split between the two colours instead of hardcoding
// one.
// ---------------------------------------------------------------------------
struct ZM_BuildingAlbedoDraws
{
	float m_fJitterR = 0.0f, m_fJitterG = 0.0f, m_fJitterB = 0.0f;   // base-colour jitter
	float m_fWeather      = 0.0f;   // grime strength [0.10, 0.30]
	float m_fRoughJitter  = 0.0f;
	float m_fBioShare     = 0.0f;   // centre of the dust/biological blend [0.30, 0.70]
	float m_fDripDensity  = 0.0f;   // sill drip streaks per tile [2, 5]
};
ZM_BuildingAlbedoDraws ZM_BuildingAlbedoDrawsFor(const ZM_BuildingRecipe& xR);

// ---------------------------------------------------------------------------
// The shared micro-detail pair (ZM_SynthBuildMicroDetail) as the building
// family bakes it: ONE albedo + ONE normal under game:Buildings/Shared/, bound
// as detail maps by every WALL and ROOF material. Pure builder + path scheme
// here; the bake is tools-only below.
// ---------------------------------------------------------------------------
enum ZM_BUILDING_DETAIL_MAP : u_int
{
	ZM_BUILDING_DETAIL_ALBEDO,
	ZM_BUILDING_DETAIL_NORMAL,

	ZM_BUILDING_DETAIL_COUNT
};
bool ZM_BuildingSharedDetailPath(ZM_BUILDING_DETAIL_MAP eMap, char* szOut, u_int uCap);
ZM_SynthDetailPair ZM_BuildBuildingMicroDetail();

// ---------------------------------------------------------------------------
// ZM_Building -- the full in-memory bundle: one mesh + one PBR map set per
// surface class. The .zmtrl / .zmodel bundle bake is ZM_BakeBuilding.
// ---------------------------------------------------------------------------
struct ZM_Building
{
	ZM_BUILDING_ID        m_eId = ZM_BUILDING_NONE;
	ZM_GenMesh            m_axMesh[ZM_BUILDING_SURFACE_COUNT];   // static: zero bones, byte-empty skin buffers
	ZM_BuildingTextureSet m_axTextures[ZM_BUILDING_SURFACE_COUNT];
};

// Build the complete bundle for a building (resolve -> per surface: mesh then
// textures, in surface order). That order is part of the determinism contract.
void ZM_BuildBuilding(ZM_BUILDING_ID eId, ZM_Building& xOut);

// ---------------------------------------------------------------------------
// Determinism helpers (the same-id byte-identity gate machinery).
// ---------------------------------------------------------------------------

// Byte-exact SoA compare over every ZM_GenMesh buffer (sizes then memcmp).
bool  ZM_BuildingMeshEqual (const ZM_GenMesh& xA, const ZM_GenMesh& xB);

// Byte-exact compare of two bundles: every surface's mesh and every map.
bool  ZM_BuildingBuildEqual(const ZM_Building& xA, const ZM_Building& xB);

// FNV-1a content hash folding every surface's mesh SoA buffers and map bytes.
u_int ZM_BuildingContentHash(const ZM_Building& xBuilding);

// ---------------------------------------------------------------------------
// Validation.
//
// ★ WHY BUILDINGS DO NOT USE ZM_ValidateGenMeshStatic DIRECTLY. That validator
// requires every UV inside [0,1], which is the right rule for an ATLASED part and
// the wrong one for a TILING surface: a 16.5 m wall at a 2 m tile legitimately
// reaches u = 8.25. ZM_ValidateBuildingSurfaceMesh runs every other clause the
// static validator runs -- winding, bounds, index range, no skeleton, no skin
// buffers -- and swaps the UV clause for "finite, and within the tile budget the
// surface's own footprint can justify". A NaN or a runaway UV still reds.
// ---------------------------------------------------------------------------
struct ZM_BuildingSurfaceValidation
{
	bool  m_bWindingOutward      = false;
	bool  m_bBoundsNonDegen      = false;
	bool  m_bIndicesInRange      = false;
	bool  m_bUVsFiniteAndBounded = false;
	bool  m_bNoSkeleton          = false;
	bool  m_bNoSkinBuffers       = false;
	bool  m_bAllValid            = false;
	u_int m_uFirstBadTriangle    = 0xFFFFFFFFu;
	float m_fMaxAbsUV            = 0.0f;
};
ZM_BuildingSurfaceValidation ZM_ValidateBuildingSurfaceMesh(const ZM_GenMesh& xMesh,
	float fMaxAbsUVAllowed);

struct ZM_BuildingValidation
{
	ZM_BuildingSurfaceValidation m_axSurface[ZM_BUILDING_SURFACE_COUNT];
	bool m_abTexturesNonEmpty[ZM_BUILDING_SURFACE_COUNT] = {};
	bool m_bAllValid = false;
};
ZM_BuildingValidation ZM_ValidateBuilding(const ZM_Building& xBuilding);

// ---------------------------------------------------------------------------
// Asset-path scheme (AssetManifest section 2). PER-MODEL only (buildings are
// static: NO shared skeleton/anim set), and now PER-SURFACE:
//   game:Buildings/<Name>/<Name>_<surface>.zmesh
//                        /<Name>_<surface>_albedo.ztxtr   (+ _normal / _rm / _ao / _height)
//                        /<Name>_<surface>.zmtrl
//   game:Buildings/<Name>/<Name>.zmodel
// Writes the canonical "game:" ref and returns false on buffer overflow
// (truncation), mirroring ZM_HumanAssetPath.
//
// ★ THE KIND ENUM IS DERIVED, NOT SPELLED. Seven outputs per surface plus one
// model is 29 rows; hand-writing them is 29 chances to transpose a name, and the
// bake manifest walks [0, KIND_COUNT) so a transposition would check the wrong
// file rather than fail to compile. ZM_BuildingSurfaceAssetKind composes the two
// axes instead, and ZM_BuildingAssetSurface / ...AssetSlot decompose them.
// ---------------------------------------------------------------------------
enum ZM_BUILDING_ASSET_SLOT : u_int
{
	ZM_BUILDING_SLOT_MESH,        // <stem>.zmesh
	ZM_BUILDING_SLOT_ALBEDO,      // <stem>_albedo.ztxtr
	ZM_BUILDING_SLOT_NORMAL,      // <stem>_normal.ztxtr   (BC5)
	ZM_BUILDING_SLOT_ROUGH_METAL, // <stem>_rm.ztxtr
	ZM_BUILDING_SLOT_OCCLUSION,   // <stem>_ao.ztxtr
	ZM_BUILDING_SLOT_HEIGHT,      // <stem>_height.ztxtr   (linear BC1, POM source)
	ZM_BUILDING_SLOT_MATERIAL,    // <stem>.zmtrl

	ZM_BUILDING_SLOT_COUNT
};

enum ZM_BUILDING_ASSET_KIND : u_int
{
	// [0, SURFACE_COUNT * SLOT_COUNT) are the per-surface outputs, surface-major.
	ZM_BUILDING_ASSET_MODEL = static_cast<u_int>(ZM_BUILDING_SURFACE_COUNT) * static_cast<u_int>(ZM_BUILDING_SLOT_COUNT),

	ZM_BUILDING_ASSET_KIND_COUNT
};

// Compose / decompose. All three are TOTAL: an out-of-range input is reported
// with a non-fatal Zenith_Error and answered with the WALL surface / MESH slot.
ZM_BUILDING_ASSET_KIND ZM_BuildingSurfaceAssetKind(ZM_BUILDING_SURFACE eSurface,
	ZM_BUILDING_ASSET_SLOT eSlot);
ZM_BUILDING_SURFACE    ZM_BuildingAssetSurface(ZM_BUILDING_ASSET_KIND eKind);
ZM_BUILDING_ASSET_SLOT ZM_BuildingAssetSlot   (ZM_BUILDING_ASSET_KIND eKind);

// Write the canonical per-model "game:" asset ref for (building, kind) into szOut.
// Returns false (leaving szOut best-effort NUL-terminated) if uCap is too small.
bool ZM_BuildingAssetPath(ZM_BUILDING_ID eId, ZM_BUILDING_ASSET_KIND eKind, char* szOut, u_int uCap);

// ---------------------------------------------------------------------------
// Disk bake (TOOLS ONLY) -- ZM_BakeBuilding writes one model's per-surface
// mesh/map-set/material bundle plus the multi-mesh .zmodel that binds them;
// ZM_BakeAllBuildings bakes every model. NOT exercised by the in-memory ZM_Gen
// gate (see the tools-only bake smoke ZM_Tests_BuildingBake.cpp). Non-tools
// no-ops keep _False configs linking.
// ---------------------------------------------------------------------------
#ifdef ZENITH_TOOLS
bool ZM_BakeBuilding(ZM_BUILDING_ID eId);
bool ZM_BakeAllBuildings();

// The shared micro-detail pair: bake if either file is absent or empty, else a
// stat and no writes. ZM_BakeBuilding calls it first, because every wall and
// roof material it writes references the pair.
bool ZM_EnsureBuildingMicroDetailBaked();

// Bake ONE building if its bundle is not already complete on disk. Mirrors
// ZM_EnsurePropBaked: warm is a stat and no writes. This is what the Dawnmere
// facade authoring calls, because ZM_BakeAllAssets has no shipped caller and a
// committed scene must never reference an asset the boot did not guarantee.
bool ZM_EnsureBuildingBaked(ZM_BUILDING_ID eId);
#else
inline bool ZM_BakeBuilding(ZM_BUILDING_ID) { return false; }
inline bool ZM_BakeAllBuildings()           { return false; }
inline bool ZM_EnsureBuildingMicroDetailBaked() { return false; }
inline bool ZM_EnsureBuildingBaked(ZM_BUILDING_ID) { return false; }
#endif
