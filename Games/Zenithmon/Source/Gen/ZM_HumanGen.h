#pragma once

// ============================================================================
// ZM_HumanGen -- the S4 procedural HUMAN-asset generator: it turns a ZM_HUMAN_ID
// into a deterministic human bundle (skinned mesh + a placeholder albedo) that
// binds the ENGINE-SHARED StickFigure skeleton and its clip library, and (in tools
// builds) bakes the per-model bundle to disk under the ZM_HumanAssetPath scheme
// (AssetManifest section 2).
//
// ★ THE RIG IS NOT THIS GAME'S. Generator v6 retired Zenithmon's own 16-bone
// skeleton and its nine hand-authored clips outright. Humans bind
// engine:Meshes/StickFigure/StickFigure.zskel -- the SAME file, byte for byte,
// that RenderTest and Combat bind -- and play clips out of that rig's shared
// library. Nothing game-side is baked for the rig or the clips any more: there is
// no Humans/Shared/ folder, and ZM_BakeAllHumans writes per-model bundles only.
//
// That is possible because the two rigs were never actually different where it
// counted: StickFigure's bones 0..15 are Root..RightFoot with the identical names,
// parents and bind translations Zenithmon used to emit, and bones 16..50 (jaw,
// eyes, toes, thirty finger bones) are additions Zenithmon's loft simply does not
// weight. Zenithmon's rig was a strict PREFIX of StickFigure's, so every skin index
// this generator already wrote points at the correct StickFigure bone unchanged.
// ZM_HumanRigMatchesStickFigure_Test loads the real .zskel and pins exactly that.
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
#include "AssetHandling/Zenith_SkinDeform.h"
#include "Zenithmon/Source/Data/ZM_HumanData.h"      // ZM_HUMAN_ID + the variety-axis enums

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
//   v5 -> v6: THE RIG CHANGED. The mesh is no longer centre-anchored -- it is left
//             in the shared StickFigure rig's own bind space, which moves every
//             vertex of every human by a constant and repoints the .zmodel's
//             skeleton and animation refs at engine: paths. Every v5 bundle on disk
//             is both mis-placed and bound to a .zskel that no longer exists, so
//             this bump is load-bearing.
constexpr u_int uZM_HUMANGEN_VERSION = 8u;

// The shared rig's asset ref -- the ONE skeleton every human in this game binds,
// and the same file RenderTest and Combat bind. Spelled once, here.
inline constexpr const char* szZM_HUMAN_SKELETON_REF =
	"engine:Meshes/StickFigure/StickFigure.zskel";

// How many bones that rig has in total. Zenithmon does not emit them -- this is the
// SKIN INDEX CAP ZM_ValidateGenMesh is handed, i.e. the largest bone index a human
// vertex may legally reference.
constexpr u_int uZM_STICKFIGURE_BONE_COUNT = 51u;

// The leading run of that rig Zenithmon's loft actually weights: Root, Spine, Neck,
// Head, the two arm chains and the two leg chains, at indices 0..15. Bones 16..50
// (jaw, eyes, toes, fingers) belong to StickFigure's richer rig and carry no
// Zenithmon weight -- a strict subset, not a mismatch.
constexpr u_int uZM_HUMAN_CORE_BONE_COUNT = 16u;
static_assert(uZM_HUMAN_CORE_BONE_COUNT < uZM_STICKFIGURE_BONE_COUNT,
	"the core prefix must be a proper prefix of the shared rig");

// The placeholder albedo resolution SC1 fills with a flat skin-tone colour. SC3
// replaces the body with the real synthesised texture; the value is not golden.
constexpr u_int uZM_HUMAN_ALBEDO_RESOLUTION = 256u;

// Humans have no evolution, so the seed-derivation evo-stage slot is a fixed
// constant (keeps ZM_GenDeriveSeed's signature shared with creatures).
constexpr u_int uZM_HUMAN_SYNTHETIC_EVO_STAGE = 1u;

// ---------------------------------------------------------------------------
// The clips a Zenithmon human binds. Every one is a file in the SHARED StickFigure
// library -- this game authors no animation of its own, and the Name is both the
// clip's own name inside the .zanim (what Flux_AnimationClipCollection::GetClip is
// keyed on) and the on-disk suffix (StickFigure_<Name>.zanim).
//
// ★ THE SET IS WHAT THE GAME PLAYS, NOT AN ASPIRATION. v5 baked nine clips and
// wired exactly two of them: the locomotion state machine in Zenithmon.cpp uses
// Idle and Walk, and nothing anywhere played Talk, Wave, Point or Cheer. Those four
// had no StickFigure equivalent and were dropped rather than reimplemented against
// the new rig -- authoring animation nothing calls is dead content, and it would
// have been the largest single piece of this migration. RUN is kept because it is
// one file away from the speed-driven machine that already exists; HURT and FAINT
// map onto StickFigure's Hit and Death, which are the same beats under the names
// that rig gives them. Anything that later needs a wave authors it in the SHARED
// library, where all three games get it.
// ---------------------------------------------------------------------------
enum ZM_HUMAN_ANIM_CLIP : u_int
{
	ZM_HUMAN_CLIP_IDLE,    // StickFigure_Idle.zanim
	ZM_HUMAN_CLIP_WALK,    // StickFigure_Walk.zanim
	ZM_HUMAN_CLIP_RUN,     // StickFigure_Run.zanim
	ZM_HUMAN_CLIP_HURT,    // StickFigure_Hit.zanim
	ZM_HUMAN_CLIP_FAINT,   // StickFigure_Death.zanim

	ZM_HUMAN_CLIP_COUNT
};

// The clip's name in the shared library: both the .zanim file suffix and the name
// the clip carries internally, which is what makes ONE accessor serve the bake path
// and the runtime state-machine lookup without them being able to disagree.
//
// ★ NO DURATION OR LOOPING ACCESSOR. v5 pinned both as golden literals because it
// AUTHORED the curves. It does not any more: how long StickFigure's Idle runs and
// whether it loops are facts about a file this game does not own, and a literal
// here would be a claim nothing checks and the shared library could silently
// falsify. Ask the loaded Flux_AnimationClip.
const char* ZM_HumanClipName(ZM_HUMAN_ANIM_CLIP eClip);

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
// RIG BIND SPACE (generator v6) + BODY METRICS
//
// v2..v5 built every human CENTRE-ANCHORED: the loft was translated down so the
// body's centre sat on the entity origin, and the game-owned skeleton's Root bone
// was translated with it. That worked only for as long as the skeleton was this
// game's to move. It is not any more -- the rig is the shared StickFigure asset --
// and a mesh may only be skinned against a skeleton expressed in its OWN bind
// space: translating vertices without translating the pivots they rotate about is
// precisely the desync the anchor existed to avoid.
//
// So v6 leaves the mesh where StickFigure authors it: Root at y=0 (the HIP), the
// feet reaching fZM_HUMAN_MESH_FEET_Y below it. Where that model then sits relative
// to a Zenithmon entity is a GAME statement and is made once, in
// ZM_HumanBody.h's fZM_HUMAN_MODEL_OFFSET_Y, via Zenith_ModelComponent's
// model-space offset. Skinning and animation are untouched by that offset because
// it is applied to the finished model matrix, not to the bind pose.
//
// "BODY" means the six loft parts ZM_BuildHumanMesh emits BEFORE
// ZM_AppendHumanAppearanceMesh -- torso, head/neck, two arms, two legs. Hair and
// attachments are DELIBERATELY excluded: a hat must not decide how tall someone
// is. Accessories may extend past the body box; they simply do not define it. And
// the measurement is taken BEFORE the anchor translation, so it is never circular.
// ---------------------------------------------------------------------------

// THE one human whose body defines the shared placement. It must be a single
// shared constant: the rig is shared and FIXED while m_fHeightScale (0.97 ..
// 1.03) scales vertices only, so a per-model measurement would place each human
// differently against one rig. The residual is a few centimetres of deliberate
// build variety.
// HumanGen_BodyMetricsPinned pins that this row remains ZM_HUMAN_BUILD_AVERAGE
// with m_fHeightScale == 1.0, and that its ZM_HUMAN_ATTACHMENT_CAP does not
// participate (metrics use the body prefix only).
constexpr ZM_HUMAN_ID eZM_HUMAN_CANONICAL_MODEL = ZM_HUMAN_PLAYER_M;

// Measured over the CANONICAL model's PRE-ANCHOR body prefix. Both are PINNED by
// HumanGen_BodyMetricsPinned, which re-derives them from a freshly built mesh --
// a generator edit therefore reds the gate instead of silently mis-sizing every
// human in the game.
// MEASURED by HumanGen_BodyMetricsPinned on a clean Null_ build. These are
// MODEL-space units, not metres: the loft inherits the StickFigure golden ring
// tables, which build a ~2.6-unit-tall body. fZM_HUMAN_VISUAL_SCALE is what turns
// that into the game's 1.8 m.
//
// ★ ALL THREE ARE IN THE SHARED RIG'S BIND SPACE, where the rig's own origin (the
// HIP) is y=0. That is why the feet are NEGATIVE: a standing body reaches about one
// unit below its hip. v5 spelled the centre as +1.307005 because it measured a
// feet-at-zero space that no longer exists; the same body is 0.307005 above the hip.
// v7 (the proportion warp) moved all three by about 3 mm, and the CAUSE is worth
// recording because it looks like a bug and is not. The warp acts on RINGS, and a
// loft's extreme vertices are not ring vertices -- they are the cap apexes the
// loft generates a fixed distance beyond the last ring. Pinning the sole ANCHOR
// therefore pins the plane the lowest RING sits on, and the cap below it rides
// the 15% stretch the shin picked up when the knee moved onto the rig's knee
// plane. In model units that is 0.003; at the game's 1.8 m body it is 2 mm, and
// because VISUAL_SCALE and MODEL_OFFSET_Y are both DERIVED from these three, it
// moves nothing in the game at all -- humans stay 1.80 m with their feet on the
// floor. Re-derived from an observed Null_ build, as HumanGen_BodyMetricsPinned
// instructs; the tolerance was not widened.
inline constexpr float fZM_HUMAN_CANONICAL_BODY_HEIGHT = 2.601267f;
inline constexpr float fZM_HUMAN_MESH_CENTRE_Y         = 0.308522f;
inline constexpr float fZM_HUMAN_MESH_FEET_Y           = -0.992112f;

// The three are one statement measured three ways, so they owe each other this.
static_assert(fZM_HUMAN_MESH_CENTRE_Y - fZM_HUMAN_MESH_FEET_Y
		- 0.5f * fZM_HUMAN_CANONICAL_BODY_HEIGHT < 1.0e-5f
	&& 0.5f * fZM_HUMAN_CANONICAL_BODY_HEIGHT
		- (fZM_HUMAN_MESH_CENTRE_Y - fZM_HUMAN_MESH_FEET_Y) < 1.0e-5f,
	"the body's centre must sit half its height above its feet");

// (The uniform authored scale that maps a canonical body onto the game's body
// box is fZM_HUMAN_VISUAL_SCALE, and it lives with the rest of the body contract
// in Source/World/ZM_HumanBody.h -- how big a person is is a GAME statement, not
// a generator one.)

// The vertical extent of one human's BODY (see above), in the shared rig's bind
// space -- so m_fMinY reads directly as "how far below the rig's origin the feet
// reach", which is what fZM_HUMAN_MODEL_OFFSET_Y is derived from.
struct ZM_HumanBodyMetrics
{
	float m_fMinY    = 0.0f;
	float m_fMaxY    = 0.0f;
	float m_fHeight  = 0.0f;   // m_fMaxY - m_fMinY
	float m_fCentreY = 0.0f;   // 0.5f * (m_fMinY + m_fMaxY)

	// The body vertex PREFIX these numbers were taken over. Finalisation adds and
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
// Per-output builders (pure functions of the recipe). Each is separately
// unit-testable.
// ---------------------------------------------------------------------------

// Loft the humanoid body (torso + head + two arms + two legs) skinned <=2 bones to
// the shared rig core indices, then the finalise order (tangents -> normalise
// skin). Emits NO bones: the rig is the shared engine asset, and the mesh only
// refers to it by index.
void ZM_BuildHumanMesh(const ZM_HumanRecipe& xRecipe, ZM_GenMesh& xMesh);

// ---------------------------------------------------------------------------
// GENERATOR v7: THE LOFT IS RE-PROPORTIONED ONTO THE SHARED RIG'S JOINT PLANES.
//
// v6 got the mesh into StickFigure's bind SPACE, which is what made skinning to
// the shared rig legal. It did not get the mesh's JOINTS onto that rig's joint
// positions -- the authored golden rows put this loft's knee at -0.480 while the
// rig's knee bone sat at -0.500, and a mesh that bends 2 cm away from the bone it
// bends around is simply what everyone had been looking at.
//
// The rig's proportions are data now (Zenith/AssetHandling/Zenith_HumanProportions.h)
// and they MOVED, so that 2 cm would have become 5 cm. v7 warps the loft's rings
// onto the rig's planes instead.
//
// ★ RINGS, NOT VERTICES, and that is not an optimisation. ZM_BuildHumanMesh's
// finalisation comment is explicit that EmitRing already wrote analytic loft
// normals and they must never be regenerated. A vertex warp is non-uniform, so it
// invalidates every one of them silently -- and rebuilding them would soften the
// hard edges the generator deliberately keeps. Warping the RINGS instead lets the
// analytic generator run on already-warped geometry, which is correct for free.
// Zenith_SkinDeform's WarpRingAndVertexFormsAgree pins that this is the same map
// StickFigure's vertex-level warp uses.
//
// ★ IN CANONICAL SPACE, BEFORE THE PER-HUMAN HEIGHT SCALE. All 34 humans then
// share one set of proportions and differ only in stature, which is the whole
// point of the recipe's m_fHeightScale.
// ---------------------------------------------------------------------------

// ★ THE ONE HUMAN WITH AN ARTIST-AUTHORED MODEL, and the reason this is a
// FUNCTION rather than a constant. Every NPC stays on the generated loft -- 34
// of them, each a different build, hair and outfit, which is exactly what the
// generator is for -- and only the player has a hand-made mesh. Returning
// nullptr for everyone else means "no imported model", so NPCs are excluded by
// construction rather than by a call site remembering to ask.
//
// The bundle is written by Zenith_Tools_HumanModelExport at tools boot and is
// gitignored art, so a fresh clone or a runtime-only build simply gets the
// generated human. The caller VALIDATES rather than trusting the path.
const char* ZM_HumanImportedModelRef(ZM_HUMAN_ID eId);

// The map from this generator's authored ring space onto the shared rig's joint
// planes. Measured ONCE, off the canonical human's own un-warped mesh, and cached
// -- so it is a property of the authored tables rather than of any one recipe.
// Comes back invalid (and warps nothing) while that first measurement is running.
const Zenith_HumanWarp& ZM_CanonicalHumanWarp();

// Apply it to a run of authored rings, in place. Each ring's arm weight comes from
// its own (m_uBoneA, m_uBoneB, m_fBlendB), so the shoulder widens and the arm
// re-proportions by riding the loft's existing deltoid blend -- no new tuning.
void ZM_WarpHumanRings(ZM_LoftRing* pxRings, u_int uNumRings);

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
	// Mesh structure (from ZM_ValidateGenMesh at uZM_STICKFIGURE_BONE_COUNT).
	bool  m_bMeshValid             = false;   // winding && bounds && weights && <=2 infl && cap
	bool  m_bWindingOutward        = false;
	bool  m_bBoundsNonDegen        = false;
	bool  m_bWeightsSumToOne       = false;
	bool  m_bWeightsAtMostTwo      = false;
	bool  m_bBonesWithinCap        = false;
	// * NO SKELETON-TOPOLOGY FLAGS. v5 carried m_bHasSingleRoot /
	// m_bParentsBeforeChildren / m_bBoneCountMatchesShared because it EMITTED the
	// rig and could get it wrong. It does not emit one now, so those would be
	// assertions about an engine asset made by a game that never touches it -- and
	// against v6's empty bone array they would have passed trivially, which is worse
	// than absent. ZM_HumanRigMatchesStickFigure_Test checks the real file instead.
	bool  m_bSkinIndicesInCorePrefix = false; // every weighted index < uZM_HUMAN_CORE_BONE_COUNT
	// Texture.
	bool  m_bAlbedoNonEmpty        = false;
	// Rollup.
	bool  m_bAllValid              = false;
	u_int m_uFirstBadVertex        = 0xFFFFFFFFu;
	u_int m_uFirstBadTriangle      = 0xFFFFFFFFu;
	u_int m_uFirstOutOfPrefixVertex = 0xFFFFFFFFu;   // first vertex weighting a non-core bone
};
ZM_HumanValidation ZM_ValidateHuman(const ZM_Human& xHuman);

// ---------------------------------------------------------------------------
// Asset-path scheme (AssetManifest section 2). Two schemes, and they now resolve
// under DIFFERENT ROOTS:
//   PER-MODEL:  game:Humans/<Name>/<Name>.zmesh / _albedo.ztxtr / .zmtrl / .zmodel
//   SHARED:     engine:Meshes/StickFigure/StickFigure.zskel + StickFigure_<Clip>.zanim
// The per-model half is baked by this game; the shared half is an ENGINE asset this
// game only refers to. Both return false on buffer overflow (truncation),
// mirroring ZM_CreatureAssetPath.
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

// The shared rig + clip files (one set for ALL humans, and for all THREE games --
// these resolve under "engine:", not "game:", and this generator bakes none of
// them). The clip kinds are kept CONTIGUOUS and last so
// (ZM_HUMAN_SHARED_ASSET_KIND)(ZM_HUMAN_SHARED_ASSET_ANIM_IDLE + eClip) maps each
// ZM_HUMAN_ANIM_CLIP to its shared asset kind; suffixes match ZM_HumanClipName.
enum ZM_HUMAN_SHARED_ASSET_KIND : u_int
{
	ZM_HUMAN_SHARED_ASSET_SKELETON,     // StickFigure.zskel
	ZM_HUMAN_SHARED_ASSET_ANIM_IDLE,    // StickFigure_Idle.zanim
	ZM_HUMAN_SHARED_ASSET_ANIM_WALK,    // StickFigure_Walk.zanim
	ZM_HUMAN_SHARED_ASSET_ANIM_RUN,     // StickFigure_Run.zanim
	ZM_HUMAN_SHARED_ASSET_ANIM_HURT,    // StickFigure_Hit.zanim
	ZM_HUMAN_SHARED_ASSET_ANIM_FAINT,   // StickFigure_Death.zanim

	ZM_HUMAN_SHARED_ASSET_KIND_COUNT
};
static_assert(static_cast<u_int>(ZM_HUMAN_SHARED_ASSET_KIND_COUNT)
		== static_cast<u_int>(ZM_HUMAN_SHARED_ASSET_ANIM_IDLE)
			+ static_cast<u_int>(ZM_HUMAN_CLIP_COUNT),
	"every clip must have exactly one shared asset kind, contiguous after the skeleton");

// Write the canonical per-model "game:" asset ref for (human, kind) into szOut.
// Returns false (leaving szOut best-effort NUL-terminated) if uCap is too small.
bool ZM_HumanAssetPath(ZM_HUMAN_ID eId, ZM_HUMAN_ASSET_KIND eKind, char* szOut, u_int uCap);

// Write the canonical shared-rig "engine:" asset ref for eKind into szOut. Returns
// false (leaving szOut best-effort NUL-terminated) if uCap is too small.
bool ZM_HumanSharedAssetPath(ZM_HUMAN_SHARED_ASSET_KIND eKind, char* szOut, u_int uCap);

// ---------------------------------------------------------------------------
// Disk bake (TOOLS ONLY) -- ZM_BakeHuman writes one model's
// mesh/albedo/material/model bundle; ZM_BakeAllHumans bakes every model. NOT
// exercised by the in-memory ZM_Gen gate. Non-tools no-ops keep _False configs
// linking.
//
// * THERE IS NO SHARED BAKE ANY MORE. v5 had ZM_BakeHumanShared writing a .zskel
// and nine .zanim files into game:Humans/Shared/; v6 binds the engine's StickFigure
// rig and library instead, so there is nothing game-side to write and no
// Humans/Shared/ folder at all. A bundle REFERS to those engine assets; it never
// produces them.
// ---------------------------------------------------------------------------
#ifdef ZENITH_TOOLS
bool ZM_BakeHuman(ZM_HUMAN_ID eId);
bool ZM_BakeAllHumans();
#else
inline bool ZM_BakeHuman(ZM_HUMAN_ID)     { return false; }
inline bool ZM_BakeAllHumans()            { return false; }
#endif
