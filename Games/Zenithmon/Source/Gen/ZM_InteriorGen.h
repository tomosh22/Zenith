#pragma once

// ============================================================================
// ZM_InteriorGen -- the procedural ROOM-SHELL generator: it turns a
// ZM_INTERIOR_ROOM into a deterministic bundle of inward-facing surface meshes
// (floor / wall / ceiling / trim), each with a full PBR map set, and (in tools
// builds) bakes it under the ZM_InteriorAssetPath scheme.
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
// DETERMINISM: every output byte is a pure function of the room id. Randomness
// reaches a builder ONLY through ZM_MakeInteriorRNG over the room's derived
// per-domain seeds, and no builder draws per texel (see the ZM_Synth* header).
//
// GUARD MODEL (mirrors ZM_BuildingGen / ZM_HumanGen): the pure generation API
// compiles in ALL configs so the in-memory unit gate exercises it headless. Only
// the disk bake is #ifdef ZENITH_TOOLS, with a non-tools no-op so _False links.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_GenCommon.h"     // ZM_GenMesh, ZM_GEN_DOMAIN, ZM_GenRNG, ZM_StaticMesh
#include "Zenithmon/Source/Gen/ZM_TextureSynth.h"  // ZM_GenImage + the tileable primitives

// Bumped whenever this module's generation algorithms change, so stale bakes
// self-invalidate through ZM_BakeManifest.
constexpr u_int uZM_INTERIORGEN_VERSION = 1u;

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
// want different roughness and different normal strength.
// ---------------------------------------------------------------------------
enum ZM_INTERIOR_SURFACE : u_int
{
	ZM_INTERIOR_SURFACE_FLOOR,     // the slab the player walks on
	ZM_INTERIOR_SURFACE_WALL,      // four walls + the lintel over the aperture
	ZM_INTERIOR_SURFACE_CEILING,   // the soffit overhead
	ZM_INTERIOR_SURFACE_TRIM,      // skirting, door reveal, and the lab's bench rail

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
void ZM_BuildInteriorSurfaceMesh(const ZM_InteriorRecipe& xR,
	ZM_INTERIOR_SURFACE eSurface, ZM_GenMesh& xMesh);

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
