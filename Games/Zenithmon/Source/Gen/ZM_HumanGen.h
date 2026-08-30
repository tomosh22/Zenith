#pragma once

// ============================================================================
// ZM_HumanGen -- the S4 procedural HUMAN-asset generator: it turns a ZM_HUMAN_ID
// into a deterministic human bundle (skinned mesh + a placeholder albedo) that
// binds the ONE shared 16-bone skeleton and reuses the ONE shared 9-clip .zanim
// set, and (in tools builds) bakes the shared rig + per-model bundle to disk
// under the ZM_HumanAssetPath scheme (AssetManifest section 2).
//
// SIMPLER THAN ZM_CreatureGen (deliberately): there is exactly ONE body plan --
// NO archetype dispatch, NO shiny, NO dex icon, NO per-model normal map (v1).
// Per-model variation lives ONLY in the mesh loft + texture, driven by the
// ZM_HumanData variety axes (build / skin / hair / outfit / attachment). The
// SKELETON is shared and FIXED: identical bone COUNT + NAMES + INDEX ORDER for
// every model, which is what makes the index-keyed skin + the name-keyed shared
// clips transfer to every human. Humans therefore do NOT draw a SKELETON domain
// per model.
//
// DETERMINISM (AssetManifest 6.2, the load-bearing S4 invariant): every output
// byte is a pure function of the human id. Randomness reaches a builder ONLY
// through ZM_MakeGenRNG over the recipe's pre-derived m_aulDomainSeed[] (one
// independent PCG stream per ZM_GEN_DOMAIN). No clock / pointer / global RNG /
// container-iteration entropy; fixed draw order. Same id => byte-identical
// bundle, proved by ZM_HumanBuildEqual / ZM_HumanContentHash.
//
// GUARD MODEL (mirrors ZM_CreatureGen / ZM_GenCommon): the pure generation API
// below compiles in ALL configs so the in-memory ZM_Gen unit gate exercises it
// headless. Only the disk bake at the very end is #ifdef ZENITH_TOOLS, with a
// non-tools no-op so _False builds link.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_GenCommon.h"     // ZM_GenMesh, ZM_GEN_DOMAIN, ZM_GenRNG, ZM_GenDeriveSeed, bake bridge
#include "Zenithmon/Source/Gen/ZM_TextureSynth.h"    // ZM_GenImage (the placeholder albedo type)
#include "Zenithmon/Source/Data/ZM_HumanData.h"      // ZM_HUMAN_ID + the variety-axis enums

// Forward-declared so the frozen SC4 clip-builder decl needs no engine anim
// header in this all-config seam (the body lands in SC4).
class Flux_AnimationClip;

// ZM_BakeManifest (a later box) stamps this per-family version; bump it whenever
// this module's generation algorithms change so stale bakes self-invalidate
// (AssetManifest 7: "fix the generator, bump its version, re-bake").
//   v1 -> v2: the shared bind space is CENTRE-ANCHORED (see the block below the
//             recipe section). Every v1 bake on disk is feet-at-y=0 and would
//             render sunk into the floor, so the bump is load-bearing.
//   v2 -> v3: ZM_HUMAN_PROF_ASTER's hair moved GREY -> WHITE to clear the greybox
//             palette's 0.15 separation floor (ZM_HumanData.cpp explains why).
//             ★ THIS BUMP IS LOAD-BEARING FOR A REASON THAT IS EASY TO MISS: the
//             bake stamp is (magic, version, file COUNT), NOT a content hash. A
//             hair-colour edit changes Aster's .zmesh / _albedo.ztxtr / .zmodel
//             BYTES while leaving the file count identical, so on a warm asset
//             tree the stamp would still match and the v2 grey-haired bake would
//             be served forever. Any future edit to a generation algorithm or to a
//             ZM_HumanData variety axis owes the same bump.
// 4: humans gained the full four-map PBR set beside the albedo, derived from the
// albedo luma (see ZM_SynthHeightFromAlbedoLuma for why that is a heuristic).
constexpr u_int uZM_HUMANGEN_VERSION = 4u;

// The shared humanoid skeleton is EXACTLY these 16 bones (the frozen StickFigure
// core names). ZM_AppendSharedHumanBones is the single canonical emit; both the
// per-model mesh builder AND the shared-skeleton bake consume it, so every model
// carries the identical bone count / names / index order.
constexpr u_int uZM_HUMAN_BONE_COUNT = 16u;

// The placeholder albedo resolution SC1 fills with a flat skin-tone colour. SC3
// replaces the body with the real synthesised texture; the value is not golden.
constexpr u_int uZM_HUMAN_ALBEDO_RESOLUTION = 256u;

// Humans have no evolution, so the seed-derivation evo-stage slot is a fixed
// constant (keeps ZM_GenDeriveSeed's signature shared with creatures).
constexpr u_int uZM_HUMAN_SYNTHETIC_EVO_STAGE = 1u;

// ---------------------------------------------------------------------------
// The shared 9-clip set (FROZEN now; the curves land in SC4). Names / durations
// / looping are golden literals. The Name doubles as the Flux clip name AND the
// on-disk shared .zanim file suffix (Human_<Name>.zanim). Keyframe sample rate
// REUSES uZM_CREATURE_ANIM_TICKS_PER_SECOND (24) -- no new tps constant.
//   IDLE   "Idle"   2.0s  loop
//   WALK   "Walk"   1.0s  loop
//   RUN    "Run"    0.7s  loop
//   TALK   "Talk"   1.6s  loop
//   WAVE   "Wave"   1.0s  one-shot
//   POINT  "Point"  0.8s  one-shot
//   CHEER  "Cheer"  1.2s  one-shot
//   HURT   "Hurt"   0.4s  one-shot
//   FAINT  "Faint"  1.2s  one-shot
// ---------------------------------------------------------------------------
enum ZM_HUMAN_ANIM_CLIP : u_int
{
	ZM_HUMAN_CLIP_IDLE,
	ZM_HUMAN_CLIP_WALK,
	ZM_HUMAN_CLIP_RUN,
	ZM_HUMAN_CLIP_TALK,
	ZM_HUMAN_CLIP_WAVE,
	ZM_HUMAN_CLIP_POINT,
	ZM_HUMAN_CLIP_CHEER,
	ZM_HUMAN_CLIP_HURT,
	ZM_HUMAN_CLIP_FAINT,

	ZM_HUMAN_CLIP_COUNT
};

// Golden metadata accessors (literal-pinned; no version read).
const char* ZM_HumanClipName(ZM_HUMAN_ANIM_CLIP eClip);
float       ZM_HumanClipDurationSeconds(ZM_HUMAN_ANIM_CLIP eClip);
bool        ZM_HumanClipLooping(ZM_HUMAN_ANIM_CLIP eClip);

// Author the shared clip's rotation channels against the shared skeleton. FROZEN
// declaration; the body is SC4 (declared-only in SC1 -- never called by the SC1
// gate, so no link dependency). Byte-identical for every human (clips are a pure
// function of the clip id, transferred by bone NAME).
void ZM_BuildHumanClip(ZM_HUMAN_ANIM_CLIP eClip, Flux_AnimationClip& xOut);

// ---------------------------------------------------------------------------
// ZM_HumanRecipe -- the fully resolved per-human generation inputs. Pure data;
// ZM_ResolveHumanRecipe fills it deterministically from ZM_HumanData.
// ---------------------------------------------------------------------------
struct ZM_HumanRecipe
{
	ZM_HUMAN_ID          m_eId          = ZM_HUMAN_PLAYER_M;
	u_int                m_uSyntheticSeed = 0u;   // family seed == ZM_GenHashName(m_szName)

	// One independent 64-bit PCG seed per generation domain (ZM_GenDeriveSeed).
	// The SOLE randomness source every builder draws from; index with ZM_GEN_DOMAIN_*.
	// Humans skip the SKELETON domain (the skeleton is shared/fixed) but the full
	// array is derived so a builder can index any domain without a gap.
	u_int64              m_aulDomainSeed[ZM_GEN_DOMAIN_COUNT] = {};

	// Variety axes copied from the roster row -- these drive the mesh loft +
	// texture, NEVER the skeleton.
	ZM_HUMAN_BUILD       m_eBuild       = ZM_HUMAN_BUILD_AVERAGE;
	float                m_fHeightScale = 1.0f;   // modest per-build height factor (about the grounded feet)
	ZM_HUMAN_SKIN_TONE   m_eSkinTone    = ZM_HUMAN_SKIN_FAIR;
	u_int                m_uHairStyle   = 0u;
	ZM_HUMAN_HAIR_COLOUR m_eHairColour  = ZM_HUMAN_HAIR_BROWN;
	ZM_HUMAN_OUTFIT      m_eOutfit      = ZM_HUMAN_OUTFIT_TRAVELER;
	ZM_HUMAN_ATTACHMENT  m_eAttachment  = ZM_HUMAN_ATTACHMENT_NONE;
};

// Resolve a human id into its full generation recipe (bounds-asserted id).
ZM_HumanRecipe ZM_ResolveHumanRecipe(ZM_HUMAN_ID eId);

// ---------------------------------------------------------------------------
// CENTRE-ANCHORED BIND SPACE (generator v2) + BODY METRICS
//
// v1 built every human with the feet near y=0. Zenithmon's authored entity
// position is the CAPSULE CENTRE (feet + half the body height) and the whole game
// speaks that vocabulary -- camera pivot, ground probe, sight-cone origin, walk-up
// standoff, both head anchors, every spawn point, the camera-clearance table. So
// the entity origin could not move to the feet; the MESH moves instead. The shared
// bind space is rigidly translated DOWN by fZM_HUMAN_MESH_CENTRE_Y, which puts the
// canonical body's centre on the entity origin and lets a human be authored with
// the model on the entity itself.
//
// Because it is a RIGID translation of the root bone AND every vertex, skinning
// and animation are mathematically untouched: the shared clips are rotation-only
// with root motion disabled, and every other bone is parent-local (so subtracting
// from all 16 would compound down the hierarchy -- only Root moves).
//
// "BODY" means the six loft parts ZM_BuildHumanMesh emits BEFORE
// ZM_AppendHumanAppearanceMesh -- torso, head/neck, two arms, two legs. Hair and
// attachments are DELIBERATELY excluded: a hat must not decide how tall someone
// is. Accessories may extend past the body box; they simply do not define it. And
// the measurement is taken BEFORE the anchor translation, so it is never circular.
// ---------------------------------------------------------------------------

// THE one human whose body defines the shared bind space. It must be a single
// shared constant: the skeleton is shared and FIXED while m_fHeightScale (0.97 ..
// 1.03) scales vertices only, so a per-model centre would desync rig from mesh.
// The residual is a few centimetres of deliberate build variety.
// HumanGen_BodyMetricsPinned pins that this row remains ZM_HUMAN_BUILD_AVERAGE
// with m_fHeightScale == 1.0, and that its ZM_HUMAN_ATTACHMENT_CAP does not
// participate (metrics use the body prefix only).
constexpr ZM_HUMAN_ID eZM_HUMAN_CANONICAL_MODEL = ZM_HUMAN_PLAYER_M;

// Measured over the CANONICAL model's PRE-ANCHOR body prefix. Both are PINNED by
// HumanGen_BodyMetricsPinned, which re-derives them from a freshly built mesh --
// a generator edit therefore reds the gate instead of silently mis-sizing every
// human in the game.
// MEASURED 2026-08-01 by HumanGen_BodyMetricsPinned on a clean Null_ build. These
// are MODEL-space units, not metres: the loft inherits the StickFigure golden ring
// tables, which build a ~2.6-unit-tall body. fZM_HUMAN_VISUAL_SCALE is what turns
// that into the game's 1.8 m.
inline constexpr float fZM_HUMAN_CANONICAL_BODY_HEIGHT = 2.604300f;
inline constexpr float fZM_HUMAN_MESH_CENTRE_Y         = 1.307005f;

// Where the root bone sits once the bind space is anchored. Root is the ONLY bone
// with parent -1, so its local translation is its world bind position; every other
// bone is parent-local and follows for free.
inline constexpr float fZM_HUMAN_ROOT_BIND_Y = 1.0f - fZM_HUMAN_MESH_CENTRE_Y;

// (The uniform authored scale that maps a canonical body onto the game's body
// box is fZM_HUMAN_VISUAL_SCALE, and it lives with the rest of the body contract
// in Source/World/ZM_HumanBody.h -- how big a person is is a GAME statement, not
// a generator one.)

// The vertical extent of one human's BODY (see above), in PRE-ANCHOR bind space.
struct ZM_HumanBodyMetrics
{
	float m_fMinY    = 0.0f;
	float m_fMaxY    = 0.0f;
	float m_fHeight  = 0.0f;   // m_fMaxY - m_fMinY
	float m_fCentreY = 0.0f;   // 0.5f * (m_fMinY + m_fMaxY)

	// The body vertex PREFIX these numbers were taken over. The anchor adds and
	// removes no vertices and reorders nothing, so the SAME prefix indexes the body
	// of the finished ZM_BuildHumanMesh output -- which is how a test measures the
	// shipped mesh instead of trusting arithmetic about it.
	u_int m_uBodyVertexCount = 0u;
};

// Build eId's mesh and measure its body prefix. A pure function of compiled data:
// it never reads a .zmodel, which is what keeps scene authoring possible on a cold
// tree with no human bake at all.
ZM_HumanBodyMetrics ZM_MeasureHumanBody(ZM_HUMAN_ID eId);

// Recipe-level overload -- the primitive. Lets a test vary ONE appearance axis
// (hair style, attachment) and prove the body metric does not move.
ZM_HumanBodyMetrics ZM_MeasureHumanBody(const ZM_HumanRecipe& xRecipe);

// Seed a domain's generation RNG from a resolved recipe. THE single entry point
// through which randomness reaches any builder (keeps the determinism invariant
// auditable: every stream comes from a pre-derived domain seed).
inline ZM_GenRNG ZM_MakeGenRNG(const ZM_HumanRecipe& xRecipe, ZM_GEN_DOMAIN eDomain)
{
	return ZM_GenRNG(xRecipe.m_aulDomainSeed[eDomain]);
}

// ---------------------------------------------------------------------------
// Shared skeleton -- THE canonical bone emit. Appends exactly uZM_HUMAN_BONE_COUNT
// bones (Root, Spine, Neck, Head, the two arm chains, the two leg chains) into
// xMesh, parent-before-child, with IDENTITY bind-local rotation on EVERY bone
// (mandatory: the rotation-only shared clips are absolute-local, so a non-identity
// bind rotation would pose every model wrong) and unit bind scale. The bind pose is
// CENTRE-ANCHORED (v2): Root sits at fZM_HUMAN_ROOT_BIND_Y, which is the only bone
// the anchor touches. Both the per-model mesh builder AND the shared .zskel bake
// call this, so the rig and every mesh move together and can never drift.
void ZM_AppendSharedHumanBones(ZM_GenMesh& xMesh);

// ---------------------------------------------------------------------------
// Per-output builders (pure functions of the recipe). Each is separately
// unit-testable.
// ---------------------------------------------------------------------------

// Build the shared skeleton (ZM_AppendSharedHumanBones) then loft a simple valid
// humanoid body (torso + head + two arms + two legs) skinned <=2 bones to the
// shared indices, then the finalise order (tangents -> normalise skin). SC1 body
// is intentionally MINIMAL -- enough that ZM_ValidateGenMesh passes; SC2 replaces
// it with the real humanoid loft.
void ZM_BuildHumanMesh(const ZM_HumanRecipe& xRecipe, ZM_GenMesh& xMesh);

// ---------------------------------------------------------------------------
// ZM_Human -- the full in-memory bundle SC1 produces (mesh + placeholder albedo).
// The .zmtrl / .zmodel bundle bake is deferred to SC5.
// ---------------------------------------------------------------------------
struct ZM_Human
{
	ZM_HUMAN_ID m_eId = ZM_HUMAN_PLAYER_M;
	ZM_GenMesh  m_xMesh;      // positions/normals/uvs/tangents/skin + shared bones
	ZM_GenImage m_xAlbedo;    // the base colour (skin, hair, clothing)
	// ★ THE FULL PBR SET. A character standing in a room whose walls have relief,
	// wearing only a base colour, reads as a cardboard cut-out -- and the interior
	// overhaul gave every human in this game exactly that comparison to lose. The
	// height field is derived from the albedo's LUMA, which is a heuristic and is
	// documented as one on ZM_SynthHeightFromAlbedoLuma: it buys pore-scale and
	// fabric-scale break-up, not sculpted form.
	ZM_SynthPbrSet m_xPbr;
};

// Build the complete bundle for a human (resolve -> mesh -> placeholder albedo),
// in that fixed order.
void ZM_BuildHuman(ZM_HUMAN_ID eId, ZM_Human& xOut);

// ---------------------------------------------------------------------------
// Determinism helpers (the same-id byte-identity gate machinery).
// ---------------------------------------------------------------------------

// Byte-exact SoA compare over every ZM_GenMesh buffer (sizes then memcmp).
bool  ZM_HumanMeshEqual(const ZM_GenMesh& xA, const ZM_GenMesh& xB);

// Byte-exact compare of two bundles: mesh + albedo.
bool  ZM_HumanBuildEqual(const ZM_Human& xA, const ZM_Human& xB);

// FNV-1a content hash folding the mesh SoA buffers and the albedo bytes.
u_int ZM_HumanContentHash(const ZM_Human& xHuman);

// ---------------------------------------------------------------------------
// ZM_ValidateHuman -- the S4 human test contract in one pure call; wraps
// ZM_ValidateGenMesh at the shared-human bone cap and adds the skeleton-topology
// checks. m_bAllValid is the conjunction of the structural flags.
// ---------------------------------------------------------------------------
struct ZM_HumanValidation
{
	// Mesh structure (from ZM_ValidateGenMesh at uZM_HUMAN_BONE_COUNT).
	bool  m_bMeshValid             = false;   // winding && bounds && weights && <=2 infl && cap
	bool  m_bWindingOutward        = false;
	bool  m_bBoundsNonDegen        = false;
	bool  m_bWeightsSumToOne       = false;
	bool  m_bWeightsAtMostTwo      = false;
	bool  m_bBonesWithinCap        = false;
	// Skeleton topology (the shared-rig invariants).
	bool  m_bHasSingleRoot         = false;   // exactly one bone with parent -1
	bool  m_bParentsBeforeChildren = false;   // every parent index < child index
	bool  m_bBoneCountMatchesShared = false;  // GetNumBones() == uZM_HUMAN_BONE_COUNT
	// Texture.
	bool  m_bAlbedoNonEmpty        = false;
	// Rollup.
	bool  m_bAllValid              = false;
	u_int m_uFirstBadVertex        = 0xFFFFFFFFu;
	u_int m_uFirstBadTriangle      = 0xFFFFFFFFu;
	char  m_szFirstBad[uZM_GEN_BONE_NAME_MAX] = {};   // first parent-before-child violator (or empty)
};
ZM_HumanValidation ZM_ValidateHuman(const ZM_Human& xHuman);

// ---------------------------------------------------------------------------
// Asset-path scheme (AssetManifest section 2). Two schemes:
//   PER-MODEL:  game:Humans/<Name>/<Name>.zmesh / _albedo.ztxtr / .zmtrl / .zmodel
//   SHARED:     game:Humans/Shared/Human.zskel + Human_<Clip>.zanim (Idle..Faint)
// Both write the canonical "game:" ref and return false on buffer overflow
// (truncation), mirroring ZM_CreatureAssetPath.
// ---------------------------------------------------------------------------
enum ZM_HUMAN_ASSET_KIND : u_int
{
	ZM_HUMAN_ASSET_MESH,       // <Name>.zmesh
	ZM_HUMAN_ASSET_ALBEDO,     // <Name>_albedo.ztxtr
	ZM_HUMAN_ASSET_NORMAL,     // <Name>_normal.ztxtr   (BC5)
	ZM_HUMAN_ASSET_ROUGH_METAL,// <Name>_rm.ztxtr
	ZM_HUMAN_ASSET_OCCLUSION,  // <Name>_ao.ztxtr
	ZM_HUMAN_ASSET_MATERIAL,   // <Name>.zmtrl
	ZM_HUMAN_ASSET_MODEL,      // <Name>.zmodel

	ZM_HUMAN_ASSET_KIND_COUNT
};

// The shared rig + clip files (one set for ALL humans). The 9 clip kinds are kept
// CONTIGUOUS and last so (ZM_HUMAN_SHARED_ASSET_KIND)(ZM_HUMAN_SHARED_ASSET_ANIM_IDLE
// + eClip) maps each ZM_HUMAN_ANIM_CLIP to its shared asset kind; suffixes match
// ZM_HumanClipName (Idle..Faint).
enum ZM_HUMAN_SHARED_ASSET_KIND : u_int
{
	ZM_HUMAN_SHARED_ASSET_SKELETON,     // Human.zskel
	ZM_HUMAN_SHARED_ASSET_ANIM_IDLE,    // Human_Idle.zanim
	ZM_HUMAN_SHARED_ASSET_ANIM_WALK,    // Human_Walk.zanim
	ZM_HUMAN_SHARED_ASSET_ANIM_RUN,     // Human_Run.zanim
	ZM_HUMAN_SHARED_ASSET_ANIM_TALK,    // Human_Talk.zanim
	ZM_HUMAN_SHARED_ASSET_ANIM_WAVE,    // Human_Wave.zanim
	ZM_HUMAN_SHARED_ASSET_ANIM_POINT,   // Human_Point.zanim
	ZM_HUMAN_SHARED_ASSET_ANIM_CHEER,   // Human_Cheer.zanim
	ZM_HUMAN_SHARED_ASSET_ANIM_HURT,    // Human_Hurt.zanim
	ZM_HUMAN_SHARED_ASSET_ANIM_FAINT,   // Human_Faint.zanim

	ZM_HUMAN_SHARED_ASSET_KIND_COUNT
};

// Write the canonical per-model "game:" asset ref for (human, kind) into szOut.
// Returns false (leaving szOut best-effort NUL-terminated) if uCap is too small.
bool ZM_HumanAssetPath(ZM_HUMAN_ID eId, ZM_HUMAN_ASSET_KIND eKind, char* szOut, u_int uCap);

// Write the canonical shared-rig "game:" asset ref for eKind into szOut. Returns
// false (leaving szOut best-effort NUL-terminated) if uCap is too small.
bool ZM_HumanSharedAssetPath(ZM_HUMAN_SHARED_ASSET_KIND eKind, char* szOut, u_int uCap);

// ---------------------------------------------------------------------------
// Disk bake (TOOLS ONLY) -- ZM_BakeHumanShared writes the shared .zskel + 9
// .zanim files ONCE; ZM_BakeHuman writes one model's mesh/albedo/material/model
// bundle; ZM_BakeAllHumans bakes the shared rig then every model. NOT exercised
// by the in-memory ZM_Gen gate. Bodies land in SC5; non-tools no-ops keep _False
// configs linking.
// ---------------------------------------------------------------------------
#ifdef ZENITH_TOOLS
bool ZM_BakeHumanShared();
bool ZM_BakeHuman(ZM_HUMAN_ID eId);
bool ZM_BakeAllHumans();
#else
inline bool ZM_BakeHumanShared()          { return false; }
inline bool ZM_BakeHuman(ZM_HUMAN_ID)     { return false; }
inline bool ZM_BakeAllHumans()            { return false; }
#endif
