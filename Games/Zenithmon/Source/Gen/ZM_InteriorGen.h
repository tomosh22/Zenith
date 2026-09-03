#pragma once

// ============================================================================
// ZM_InteriorGen -- the procedural ROOM-SHELL generator: it turns a
// ZM_INTERIOR_ROOM into a deterministic bundle of inward-facing surface meshes
// (floor / wall / ceiling / trim / window / glass / sky / curtain / rug), each
// with a full PBR map set, and (in tools builds) bakes it under the
// ZM_InteriorAssetPath scheme.
//
// ★ WHY A SEPARATE GENERATOR FROM ZM_BuildingGen, WHEN THEY SHARE SO MUCH. The
// shared parts ARE shared: the world-space tiling UV projection is
// ZM_StaticMesh::AppendWorldBox, and the tileable noise/course primitives are
// ZM_Synth* in ZM_TextureSynth. What is NOT shared is the only thing that
// matters here -- a room is seen from INSIDE. Its surfaces face inward, its
// aperture is a real gap rather than a painted door, and its materials answer to
// a different question ("does this room feel lived in?") than a facade's. Fusing
// the two would give one builder with a bIsInterior branch through every method.
//
// ★★ AND THE TWO ROOMS MUST READ APART. That is not decoration, it is the
// ZM-D-176 user ruling: the player's bedroom must stop reading as the same
// greybox box as the professor's lab. It used to be answered by a per-scene
// VERTEX TINT on otherwise identical grey blocks -- a hue nudge measured at only
// 0.121 of red/blue separation, which is why ZM_InteriorTintPixels_Test has been
// red. It is answered here by the rooms being made of DIFFERENT MATERIALS:
// warm timber boards and cream plaster against cool resin tile and steel.
//
// ★★★ AND A ROOM IS NOT SIX SLABS. Version 2 is what makes these read as
// photographed rooms rather than CG boxes, and every item is geometry or
// baked data rather than a shader trick:
//   * WINDOWS -- real holes in the side walls, with a reveal, a frame, glazing
//     bars, a translucent pane and a bright emissive sky card behind it, so a
//     window is a light SOURCE in the frame and the scene sun comes through it.
//   * MOULDINGS -- a cornice at the ceiling, an architrave round the door and
//     the windows, a stepped skirting, and (in the cottage) ceiling joists.
//   * FLOORBOARDS AS GEOMETRY -- individual staggered boards with real gaps and
//     millimetre lips, over a dark underlay the gaps show.
//   * BAKED OCCLUSION AND WEAR -- a per-vertex colour (ZM_GenMesh::m_xColors,
//     which the mesh shader multiplies into the albedo) darkening the inside
//     corners and the floor/ceiling junctions, plus a traffic band from the
//     door. Analytic, deterministic, no ray casting.
//   * SOFT FURNISHINGS -- a curtain pair on a rail at every window and a rug.
//
// DETERMINISM: every output byte is a pure function of the room id. Randomness
// reaches a builder ONLY through ZM_MakeInteriorRNG over the room's derived
// per-domain seeds, and no builder draws per texel (see the ZM_Synth* header).
// The board stagger and lips are integer hashes of (row, board), never a draw.
//
// GUARD MODEL (mirrors ZM_BuildingGen / ZM_HumanGen): the pure generation API
// compiles in ALL configs so the in-memory unit gate exercises it headless. Only
// the disk bake is #ifdef ZENITH_TOOLS, with a non-tools no-op so _False links.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_GenCommon.h"     // ZM_GenMesh, ZM_GEN_DOMAIN, ZM_GenRNG, ZM_StaticMesh
#include "Zenithmon/Source/Gen/ZM_TextureSynth.h"  // ZM_GenImage + the tileable primitives

// Bumped whenever this module's generation algorithms change, so stale bakes
// self-invalidate through ZM_BakeManifest.
// 2: albedo bakes stamped BC1_RGB_SRGB rather than UNORM (ZM_TextureSynth v3),
//    and the tiling normal maps take the WRAPPING gradient rule.
// 3: windows + sky cards, mouldings, floorboard geometry, baked vertex
//    occlusion/wear, curtains and rugs; five new surface classes.
constexpr u_int uZM_INTERIORGEN_VERSION = 3u;

// Rooms have no evolution, so the seed-derivation evo-stage slot is fixed
// (keeps ZM_GenDeriveSeed's signature shared with creatures/humans/buildings).
constexpr u_int uZM_INTERIOR_SYNTHETIC_EVO_STAGE = 1u;

// ---------------------------------------------------------------------------
// The rooms. APPEND-only: these index the asset path scheme.
// ---------------------------------------------------------------------------
enum ZM_INTERIOR_ROOM : u_int
{
	ZM_INTERIOR_ROOM_PLAYER_HOME,
	ZM_INTERIOR_ROOM_PROF_LAB,

	ZM_INTERIOR_ROOM_COUNT
};

// ---------------------------------------------------------------------------
// Surface classes -- the unit of material assignment, exactly as for buildings
// and for the same two reasons: uniform texel density needs tiling UVs, which
// need a per-surface tiling material; and a floor, a wall and a steel skirting
// want different roughness and different normal strength. APPEND-only: these
// index the asset path scheme by NAME, but the model's submesh order is theirs.
// ---------------------------------------------------------------------------
enum ZM_INTERIOR_SURFACE : u_int
{
	ZM_INTERIOR_SURFACE_FLOOR,     // the boards / tiles the player walks on
	ZM_INTERIOR_SURFACE_WALL,      // four walls + the lintel, cut around the windows
	ZM_INTERIOR_SURFACE_CEILING,   // the soffit overhead
	ZM_INTERIOR_SURFACE_TRIM,      // skirting, cornice, architraves, door reveal, joists
	ZM_INTERIOR_SURFACE_WINDOW,    // window reveals, frames, glazing bars, sills, curtain rails
	ZM_INTERIOR_SURFACE_GLASS,     // the panes -- TRANSLUCENT
	ZM_INTERIOR_SURFACE_SKY,       // the sky card behind each pane -- EMISSIVE
	ZM_INTERIOR_SURFACE_CURTAIN,   // the folded curtain panels
	ZM_INTERIOR_SURFACE_RUG,       // the rug / mat on the floor

	ZM_INTERIOR_SURFACE_COUNT
};

// TOTAL: an out-of-range id is reported with a non-fatal Zenith_Error and
// answered with the first entry.
const char* ZM_InteriorRoomName   (ZM_INTERIOR_ROOM eRoom);
const char* ZM_InteriorSurfaceName(ZM_INTERIOR_SURFACE eSurface);
u_int       ZM_InteriorSurfaceResolution(ZM_INTERIOR_SURFACE eSurface);

// World size of one texture repeat, in metres, per (room, surface). It is per
// ROOM as well as per surface because a lab's floor is a 0.6 m tile grid and a
// cottage's is a 1.2 m board run, and that difference is most of what tells the
// two rooms apart underfoot.
float ZM_InteriorTileMetres(ZM_INTERIOR_ROOM eRoom, ZM_INTERIOR_SURFACE eSurface);

// ---------------------------------------------------------------------------
// ZM_InteriorRoomSpec -- the room's real dimensions.
//
// ★ EVERY FIELD IS READ FROM THE PLACEMENT HEADER, NEVER RE-SPELLED. The shell
// this generator draws has to line up with the collider blockouts the scene
// authors, to the millimetre: those blockouts ARE the walls the player stops
// against. A constant spelled at both sites cannot red a drift, and the drift
// here would be a visible wall the player walks through.
// ---------------------------------------------------------------------------
struct ZM_InteriorRoomSpec
{
	float m_fHalfWidth      = 0.0f;   // wall CENTRELINE half-extent in X
	float m_fHalfDepth      = 0.0f;   // wall CENTRELINE half-extent in Z
	float m_fWallThickness  = 0.0f;
	float m_fWallHeight     = 0.0f;
	float m_fApertureHalfW  = 0.0f;   // the entrance gap in the +Z wall
	float m_fApertureHeight = 0.0f;

	// The INNER faces -- where the visible surfaces actually go. The blockouts are
	// centred on the centrelines, so the room's usable extent is half a wall
	// thickness inside them.
	float InnerHalfWidth() const { return m_fHalfWidth - m_fWallThickness * 0.5f; }
	float InnerHalfDepth() const { return m_fHalfDepth - m_fWallThickness * 0.5f; }
};
ZM_InteriorRoomSpec ZM_GetInteriorRoomSpec(ZM_INTERIOR_ROOM eRoom);

// ---------------------------------------------------------------------------
// ZM_InteriorRecipe -- the resolved per-room generation inputs.
// ---------------------------------------------------------------------------
struct ZM_InteriorRecipe
{
	ZM_INTERIOR_ROOM    m_eRoom          = ZM_INTERIOR_ROOM_PLAYER_HOME;
	u_int               m_uSyntheticSeed = 0u;
	u_int64             m_aulDomainSeed[ZM_GEN_DOMAIN_COUNT] = {};
	ZM_InteriorRoomSpec m_xSpec;
};
ZM_InteriorRecipe ZM_ResolveInteriorRecipe(ZM_INTERIOR_ROOM eRoom);
ZM_GenRNG         ZM_MakeInteriorRNG(const ZM_InteriorRecipe& xR, ZM_GEN_DOMAIN eDomain);

// ---------------------------------------------------------------------------
// Windows -- the openings in the two SIDE walls.
//
// ★ A WINDOW IS A HOLE, NOT A DECAL. The wall surface is emitted AROUND each
// opening (ZM_InteriorMeshRayHits through the centre of one hits no wall
// triangle -- asserted), and the opening is dressed from the room side outward:
// reveal linings, the frame with its glazing bars, a sill board proud of the
// wall, the pane, and behind the pane a SKY CARD -- a bright emissive slab
// inside the wall thickness, so the window reads as the light source it is
// before any lighting has happened. All of it lives INSIDE the wall's own
// thickness (the blockout is 0.5 m; the reveal is 0.22 m deep), so nothing
// reaches past the wall centreline the player stops against.
//
// The positions are chosen clear of the furniture rows in ZM_InteriorDressing.h
// and clear of the entrance corridor; ZM_Tests_InteriorGen asserts both.
// ---------------------------------------------------------------------------
enum ZM_INTERIOR_WALL_SIDE : u_int
{
	ZM_INTERIOR_WALL_NEG_X,   // the -X side wall (the sun side in both rooms)
	ZM_INTERIOR_WALL_POS_X,   // the +X side wall

	ZM_INTERIOR_WALL_SIDE_COUNT
};

struct ZM_InteriorWindow
{
	ZM_INTERIOR_WALL_SIDE m_eWall;
	float m_fCentreZ;   // along the wall
	float m_fWidth;     // clear opening width
	float m_fSillY;     // bottom of the clear opening
	float m_fHeadY;     // top of the clear opening

	float Z0() const { return m_fCentreZ - m_fWidth * 0.5f; }
	float Z1() const { return m_fCentreZ + m_fWidth * 0.5f; }
	// The wall's inner-face X and the direction INTO the wall from it.
	float WallSign() const { return m_eWall == ZM_INTERIOR_WALL_NEG_X ? -1.0f : 1.0f; }
};

u_int                    ZM_GetInteriorWindowCount(ZM_INTERIOR_ROOM eRoom);
// TOTAL: an out-of-range room or index answers PlayerHome's first window.
const ZM_InteriorWindow& ZM_GetInteriorWindow(ZM_INTERIOR_ROOM eRoom, u_int uIndex);

// The window dressing depths, from the wall's INNER FACE into the wall. Public
// because the tests reason about what sits where; the builders read them too.
constexpr float fZM_INTERIOR_WINDOW_REVEAL_DEPTH = 0.22f;   // reveal linings + sky card end here
constexpr float fZM_INTERIOR_WINDOW_FRAME_DEPTH  = 0.10f;   // the frame's room-side face
constexpr float fZM_INTERIOR_WINDOW_GLASS_DEPTH  = 0.125f;  // the pane, inside the frame
constexpr float fZM_INTERIOR_WINDOW_FRAME_WIDTH  = 0.06f;
constexpr float fZM_INTERIOR_WINDOW_BAR_WIDTH    = 0.03f;

// Curtains: a rail above the head, and a folded panel either side of the
// opening, hanging to just above the floor. The panel width is what the
// corridor and furniture clauses reason about.
constexpr float fZM_INTERIOR_CURTAIN_PANEL_WIDTH = 0.45f;
constexpr float fZM_INTERIOR_CURTAIN_DEPTH       = 0.12f;   // how far the folds stand off the wall
constexpr float fZM_INTERIOR_CURTAIN_HEM_Y       = 0.12f;   // the hem's height above the floor

// ---------------------------------------------------------------------------
// The rug -- one per room, on the floor, outside the entrance corridor.
// ---------------------------------------------------------------------------
struct ZM_InteriorRug
{
	float m_fCentreX, m_fCentreZ;
	float m_fWidth,   m_fDepth;     // X and Z extents
	float m_fThickness;
};
ZM_InteriorRug ZM_GetInteriorRug(ZM_INTERIOR_ROOM eRoom);

// ---------------------------------------------------------------------------
// The scene SUN for each interior -- aimed in through the -X windows.
//
// ★★ THE VECTOR IS THREE FROZEN LITERALS THAT ARE ALREADY EXACTLY UNIT LENGTH
// IN FLOAT. Zenith_SunComponent::SetDirection normalises what it is given
// (a sqrt and a divide), and the result is what the committed scene serialises.
// 0.75^2 + 0.5^2 + 0.4330127^2 sums to EXACTLY 1.0f in IEEE single precision
// (verified bit-for-bit, and asserted by ZM_Tests_InteriorGen), so the divide
// is by 1.0f and the authored bytes are these literals verbatim -- the same
// ZM-D-183 discipline the frozen quaternions follow. It is 30 degrees of
// elevation exactly (y = -sin 30), travelling +X (in through a -X window) with
// a +Z skew so the frame's shadow lands across the floor rather than square.
// ---------------------------------------------------------------------------
constexpr float fZM_INTERIOR_SUN_DIR_X =  0.75f;
constexpr float fZM_INTERIOR_SUN_DIR_Y = -0.5f;
constexpr float fZM_INTERIOR_SUN_DIR_Z =  0.4330127f;
inline Zenith_Maths::Vector3 ZM_GetInteriorSunDirection(ZM_INTERIOR_ROOM /*eRoom*/)
{
	// Both rooms window their -X wall, so both take the same sun. The parameter
	// is the seam a per-room sun would land on.
	return Zenith_Maths::Vector3(fZM_INTERIOR_SUN_DIR_X, fZM_INTERIOR_SUN_DIR_Y, fZM_INTERIOR_SUN_DIR_Z);
}

// ---------------------------------------------------------------------------
// Material response + palette, per (room, surface). This is the pair that makes
// the two rooms read apart, so both are exposed and both are asserted.
// ---------------------------------------------------------------------------
struct ZM_InteriorSurfaceLook
{
	Zenith_Maths::Vector3 m_xBaseColour  = Zenith_Maths::Vector3(0.5f);
	float m_fRoughness      = 0.85f;
	float m_fMetallic       = 0.0f;
	float m_fNormalStrength = 1.0f;
	float m_fOcclusion      = 1.0f;
	// The sky card is the one emissive surface; everything else is 0 here.
	Zenith_Maths::Vector3 m_xEmissiveColour = Zenith_Maths::Vector3(0.0f);
	float m_fEmissiveIntensity = 0.0f;
	// The glass is the one translucent surface: the material's base-colour alpha
	// carries the opacity into the forward translucent pass.
	bool  m_bTranslucent = false;
	float m_fOpacity     = 1.0f;
};
ZM_InteriorSurfaceLook ZM_GetInteriorSurfaceLook(ZM_INTERIOR_ROOM eRoom,
	ZM_INTERIOR_SURFACE eSurface);

// ---------------------------------------------------------------------------
// Builders.
// ---------------------------------------------------------------------------

// Build ONE surface's mesh, INWARD-FACING. Every visible face points into the
// room; the boxes are thin slabs sitting against the inner face of the blockout
// that owns the collision, so the outward faces are buried and never seen.
//
// UVs are world-scaled and therefore leave [0,1]; validate with
// ZM_ValidateInteriorSurfaceMesh, not the [0,1]-clamped static validator.
// (The rug is the one object-mapped surface: its top face is [0,1].)
//
// The vertex colours carry the baked occlusion + wear (below) on every surface
// but GLASS and SKY.
void ZM_BuildInteriorSurfaceMesh(const ZM_InteriorRecipe& xR,
	ZM_INTERIOR_SURFACE eSurface, ZM_GenMesh& xMesh);

// ---- Baked large-scale occlusion and wear ----------------------------------
//
// ★ ANALYTIC, NOT CAST. The darkening a real room shows in its corners and
// along its floor line is a smooth function of distance to the nearest two
// planes, and that function is cheaper and more predictable than a ray cast. It
// is written into the per-vertex colour, which the mesh shader multiplies into
// the albedo whenever the colour's alpha is > 0 -- so the big surfaces are cut
// into cells (fZM_INTERIOR_SHADING_CELL) to give the interpolation somewhere
// to happen.
constexpr float fZM_INTERIOR_SHADING_CELL     = 0.75f;   // max cell edge on walls/ceiling/lab floor
constexpr float fZM_INTERIOR_OCCLUSION_RADIUS = 1.10f;   // how far a corner's darkening reaches
constexpr float fZM_INTERIOR_OCCLUSION_DEPTH  = 0.55f;   // how dark a full corner gets (1 - this)
constexpr float fZM_INTERIOR_WEAR_HALF_WIDTH  = 1.20f;   // the traffic band's half width
constexpr float fZM_INTERIOR_WEAR_DEPTH       = 0.10f;   // how dark the band's centre gets

// The occlusion multiplier, in (0,1]: 1 in the open, darkest in an inside
// corner. Reads the two strongest of the four plane terms (the two X walls fold
// into one distance, likewise Z; the floor; the ceiling), so a corner is darker
// than a single wall and a third plane cannot drive it to black.
//
// ★★ TWO ENTRY POINTS, AND THE DIFFERENCE IS LOAD-BEARING. A point in SPACE
// belongs to no surface, so every plane darkens it. A point ON a surface must
// not be darkened by the plane it LIES IN -- otherwise the floor darkens the
// whole floor -- and the only thing that says which plane that is, is the
// vertex NORMAL. Deciding it from the DISTANCE instead (the shipped first cut)
// silently excludes the plane a vertex MEETS as well: a wall vertex at the
// floor line sits at y = 0, which reads as "the floor is my own plane", and
// every wall vertex in both rooms came out at exactly 1.0.
float ZM_InteriorVertexOcclusion(const ZM_InteriorRoomSpec& xSpec,
	const Zenith_Maths::Vector3& xPos);
float ZM_InteriorSurfaceVertexOcclusion(const ZM_InteriorRoomSpec& xSpec,
	const Zenith_Maths::Vector3& xPos, const Zenith_Maths::Vector3& xNormal);
// The traffic-wear multiplier at a FLOOR point, in (0,1]: darkest on the axis
// between the door and the room centre, 1 off the band and past the centre.
float ZM_InteriorTrafficWear(const ZM_InteriorRoomSpec& xSpec,
	const Zenith_Maths::Vector3& xPos);

// The height field the normal + AO maps are both derived from.
ZM_GenImage ZM_BuildInteriorSurfaceHeight(const ZM_InteriorRecipe& xR,
	ZM_INTERIOR_SURFACE eSurface);

// One surface's full PBR map set. ALL randomness is drawn from the ALBEDO domain.
struct ZM_InteriorTextureSet
{
	ZM_GenImage m_xAlbedo;
	ZM_GenImage m_xNormal;
	ZM_GenImage m_xRoughnessMetallic;   // G = roughness, B = metallic
	ZM_GenImage m_xOcclusion;

	bool Equals(const ZM_InteriorTextureSet& xOther) const;
	bool NonEmpty() const;
};
ZM_InteriorTextureSet ZM_BuildInteriorSurfaceTextures(const ZM_InteriorRecipe& xR,
	ZM_INTERIOR_SURFACE eSurface);

// ---------------------------------------------------------------------------
// The full in-memory bundle.
// ---------------------------------------------------------------------------
struct ZM_Interior
{
	ZM_INTERIOR_ROOM      m_eRoom = ZM_INTERIOR_ROOM_COUNT;
	ZM_GenMesh            m_axMesh[ZM_INTERIOR_SURFACE_COUNT];
	ZM_InteriorTextureSet m_axTextures[ZM_INTERIOR_SURFACE_COUNT];
};
void  ZM_BuildInterior(ZM_INTERIOR_ROOM eRoom, ZM_Interior& xOut);

bool  ZM_InteriorBuildEqual (const ZM_Interior& xA, const ZM_Interior& xB);
u_int ZM_InteriorContentHash(const ZM_Interior& xInterior);

// ---------------------------------------------------------------------------
// Geometry queries the tests need. Pure.
// ---------------------------------------------------------------------------

// Does the ray (xOrigin + t * xDir, t > 0) hit any triangle of the mesh?
// Moller-Trumbore, both-sided, no acceleration -- these meshes are tens of
// thousands of triangles at most and the callers are unit tests. fTOut is the
// nearest hit distance when it returns true.
bool ZM_InteriorMeshRayHits(const ZM_GenMesh& xMesh, const Zenith_Maths::Vector3& xOrigin,
	const Zenith_Maths::Vector3& xDir, float& fTOut);

// ---------------------------------------------------------------------------
// Validation. Same clause set as the building surface validator and for the
// same reason -- tiling UVs legitimately leave [0,1] -- plus one clause the
// exterior does not need: the surfaces must face INWARD.
// ---------------------------------------------------------------------------
struct ZM_InteriorSurfaceValidation
{
	bool  m_bWindingOutward      = false;
	bool  m_bBoundsNonDegen      = false;
	bool  m_bIndicesInRange      = false;
	bool  m_bUVsFiniteAndBounded = false;
	bool  m_bColoursUnitRange    = false;   // one colour per vertex, every channel in [0,1], alpha 1
	bool  m_bNoSkeleton          = false;
	bool  m_bNoSkinBuffers       = false;
	bool  m_bAllValid            = false;
	u_int m_uFirstBadTriangle    = 0xFFFFFFFFu;
	float m_fMaxAbsUV            = 0.0f;
};
ZM_InteriorSurfaceValidation ZM_ValidateInteriorSurfaceMesh(const ZM_GenMesh& xMesh,
	float fMaxAbsUVAllowed);

struct ZM_InteriorValidation
{
	ZM_InteriorSurfaceValidation m_axSurface[ZM_INTERIOR_SURFACE_COUNT];
	bool m_abTexturesNonEmpty[ZM_INTERIOR_SURFACE_COUNT] = {};
	bool m_bAllValid = false;
};
ZM_InteriorValidation ZM_ValidateInterior(const ZM_Interior& xInterior);

// ---------------------------------------------------------------------------
// Asset-path scheme:
//   game:Interiors/<Room>/<Room>_<surface>.zmesh / _albedo|_normal|_rm|_ao.ztxtr
//                        /<Room>_<surface>.zmtrl
//   game:Interiors/<Room>/<Room>.zmodel
// ---------------------------------------------------------------------------
enum ZM_INTERIOR_ASSET_SLOT : u_int
{
	ZM_INTERIOR_SLOT_MESH,
	ZM_INTERIOR_SLOT_ALBEDO,
	ZM_INTERIOR_SLOT_NORMAL,
	ZM_INTERIOR_SLOT_ROUGH_METAL,
	ZM_INTERIOR_SLOT_OCCLUSION,
	ZM_INTERIOR_SLOT_MATERIAL,

	ZM_INTERIOR_SLOT_COUNT
};

enum ZM_INTERIOR_ASSET_KIND : u_int
{
	ZM_INTERIOR_ASSET_MODEL = static_cast<u_int>(ZM_INTERIOR_SURFACE_COUNT)
		* static_cast<u_int>(ZM_INTERIOR_SLOT_COUNT),

	ZM_INTERIOR_ASSET_KIND_COUNT
};

ZM_INTERIOR_ASSET_KIND ZM_InteriorSurfaceAssetKind(ZM_INTERIOR_SURFACE eSurface,
	ZM_INTERIOR_ASSET_SLOT eSlot);
ZM_INTERIOR_SURFACE    ZM_InteriorAssetSurface(ZM_INTERIOR_ASSET_KIND eKind);
ZM_INTERIOR_ASSET_SLOT ZM_InteriorAssetSlot   (ZM_INTERIOR_ASSET_KIND eKind);

bool ZM_InteriorAssetPath(ZM_INTERIOR_ROOM eRoom, ZM_INTERIOR_ASSET_KIND eKind,
	char* szOut, u_int uCap);

// ---------------------------------------------------------------------------
// Disk bake (TOOLS ONLY).
// ---------------------------------------------------------------------------
#ifdef ZENITH_TOOLS
bool ZM_BakeInterior(ZM_INTERIOR_ROOM eRoom);
bool ZM_BakeAllInteriors();

// Bake ONE room if its bundle is not already complete on disk. Mirrors
// ZM_EnsureBuildingBaked, and exists for the same reason: a COMMITTED scene must
// never be authored against an asset the boot did not guarantee.
bool ZM_EnsureInteriorBaked(ZM_INTERIOR_ROOM eRoom);
#else
inline bool ZM_BakeInterior(ZM_INTERIOR_ROOM)       { return false; }
inline bool ZM_BakeAllInteriors()                    { return false; }
inline bool ZM_EnsureInteriorBaked(ZM_INTERIOR_ROOM) { return false; }
#endif
