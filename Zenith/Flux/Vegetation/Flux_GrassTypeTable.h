#pragma once

//=============================================================================
// Flux_GrassTypeTable — the AUTHORING side of the per-grass-type parameters.
//
// Flux_GrassTypeParamsGPU (Flux_GrassTypes.h) is a pinned 144-byte block whose
// appearance fields are packed: colours ride in RGB8 slots and six scalars ride
// in three unorm16 PAIRS. That packing is the right shape for the GPU and the
// wrong shape for authoring — an editor slider that writes a packed half-word
// cannot round-trip, and a saved file that stores the packed form freezes the
// packing into the asset format.
//
// So the authored record below is plain, unpacked, one field per parameter, and
// ToGPU() is the one-way projection onto the GPU block. Nothing reads back the
// other way: the packed form is a GPU detail, and the authored form is the truth
// that serializes.
//
// Everything here is pure CPU — no device, no engine singleton, no Flux runtime
// include — so a headless test can author a table, validate it and project it.
//=============================================================================

#include "Flux/Vegetation/Flux_GrassTypes.h"

#include <string>

class Zenith_DataStream;

// Table capacity. The placement CS indexes TypeParams with the map's type byte
// clamped to (typeCount - 1), and the blade record carries the index in 8 bits,
// so this may grow to 256 without touching either side — but the GPU buffer is
// sized from it, so it is a deliberate budget rather than a limit.
constexpr u_int uFLUX_GRASS_MAX_TYPES = 16u;

// Mirrors kGRASS_BINDLESS_UNBOUND in Shaders/Vegetation/Flux_GrassCommon.slang.
// The fragment stage short-circuits on this value BEFORE the bindless table
// lookup, so it is a sentinel and not a valid descriptor index.
constexpr u_int uFLUX_GRASS_BINDLESS_UNBOUND = 0xFFFFFFFFu;

// The three optional per-type textures the blade fragment stage samples, in the
// order Flux_GrassShadeBlade reads them. A type binds each by asset PATH on the
// table (serialized), and the renderer resolves the path to a bindless slot at
// load — the slot number is a descriptor allocation that changes every boot, so
// it never reaches the file.
enum FluxGrassTextureSlot
{
	FLUX_GRASS_TEXTURE_VEIN = 0,   // blade albedo detail: u = across the blade, v = base -> tip. REPEAT
	FLUX_GRASS_TEXTURE_GLOSS,      // 1D gloss streaks: u = height * GlossRepeat, R channel. REPEAT
	FLUX_GRASS_TEXTURE_RAMP,       // 2D colour ramp: u = clump hash, v = base -> tip. CLAMP
	FLUX_GRASS_TEXTURE_SLOT_COUNT
};

//=============================================================================
// One authored grass type.
//
// Field groups mirror Flux_GrassTypeParamsGPU's documented groups (shape /
// placement / clump / appearance / dynamics) so ToGPU() reads as a straight
// walk down the block rather than a scatter.
//=============================================================================
struct Flux_GrassTypeParams
{
	// --- shape ---
	float m_fHeightMin              = 0.35f;   // metres, at the low end of the per-blade roll
	float m_fHeightMax              = 0.75f;
	float m_fWidthMin               = 0.012f;  // metres across the blade base
	float m_fWidthMax               = 0.022f;
	float m_fTiltMinRad             = 0.05f;   // lean away from vertical, radians
	float m_fTiltMaxRad             = 0.35f;
	float m_fBendMin                = 0.10f;   // quadratic bend along the height, as a fraction of it
	float m_fBendMax                = 0.35f;
	float m_fSideCurve              = 0.15f;   // lateral curl of the blade plane
	float m_fVertexDistributionPow  = 1.6f;    // >1 packs the vertex rows toward the tip
	float m_fFoldThreshold          = 0.65f;   // blade hash above this folds along its spine
	float m_fFoldPushApart          = 0.35f;   // fold offset, as a fraction of the height
	float m_fTipWidthFrac           = 0.15f;   // tip width as a fraction of the base width

	// --- placement ---
	float m_fDensity                = 1.0f;    // lattice-node acceptance probability [0,1]
	float m_fSlopeMax               = 0.70f;   // sine of the steepest ground that grows this type
	float m_fMaxDrawDistance        = 250.0f;  // metres; the global max distance still applies on top
	float m_fSlopeAlign             = 0.50f;   // 0 = always upright, 1 = follow the ground normal

	// --- clump ---
	float m_fClumpScale             = 3.0f;    // metres per clump cell
	float m_fClumpHeightBoost       = 0.25f;   // clump centres grow tallest — this is what reads as a tuft
	float m_fClumpFacingWeight      = 0.60f;   // how much a blade adopts its clump's heading
	float m_fClumpOutwardWeight     = 0.30f;   // ... and how much it splays away from the centre
	float m_fClumpPullToCentre      = 0.15f;   // positional pull toward the clump centre [0,1]
	float m_fClumpNormalBlendMax    = 0.50f;   // ceiling on the distant clump-normal blend

	// --- appearance ---
	// Colours are FULL floats here and pack to RGB8 in ToGPU: 8-bit colour is
	// below the noise floor of the per-blade tint jitter applied over it, but an
	// authored value must not be quantized twice by a save/load round trip.
	Zenith_Maths::Vector3 m_xBaseColour{ 0.15f, 0.28f, 0.09f };
	Zenith_Maths::Vector3 m_xTipColour { 0.42f, 0.55f, 0.18f };
	// RUNTIME state, not authored: filled by Flux_GrassTypeTable::ResolveTextureIndices
	// from the per-entry texture PATHS the table carries. The writer always emits
	// UNBOUND for these three and the reader always restores UNBOUND, because a
	// descriptor slot number from a previous boot is meaningless in this one.
	u_int m_uVeinTextureIndex       = uFLUX_GRASS_BINDLESS_UNBOUND;   // bindless slot, or UNBOUND
	u_int m_uGlossTextureIndex      = uFLUX_GRASS_BINDLESS_UNBOUND;
	u_int m_uRampTextureIndex       = uFLUX_GRASS_BINDLESS_UNBOUND;
	float m_fGlossRepeat            = 4.0f;    // gloss-streak repeats along the blade
	float m_fRoughnessBase          = 0.55f;
	float m_fSpecular               = 0.35f;
	float m_fColourJitter           = 0.15f;   // per-blade tint jitter, +/- fraction
	float m_fNormalTilt             = 0.35f;   // edge-normal tilt as a fraction of a quarter turn
	float m_fTranslucencyBase       = 0.15f;
	float m_fTranslucencyTip        = 0.65f;
	float m_fAOBase                 = 0.25f;   // occlusion at the blade base
	float m_fAOTipRelease           = 0.75f;   // height fraction at which that occlusion releases

	// --- dynamics ---
	float m_fWindResponse           = 1.0f;
	float m_fStiffness              = 0.50f;

	// Sanitize every field into a range the placement CS and vertex stage can
	// survive. This is not cosmetic: a zero clump scale divides by zero in the
	// Voronoi search, a negative height inverts the blade, and a distribution
	// exponent of zero makes the height remap a division by zero.
	//
	// A NON-FINITE field (NaN or either infinity) is REPLACED with that field's
	// authored default rather than clamped — a NaN passes a plain clamp untouched,
	// and detecting it needs Flux_GrassIsFiniteFloat's bit test because this
	// engine's float flags fold every runtime float NaN/inf test away.
	void Validate();

	// Pure projection onto the GPU block. Never reads xOut.
	void ToGPU(Flux_GrassTypeParamsGPU& xOut) const;

	//--------------------------------------------------------------------------
	// Address a field BY NAME. THE mapping lives once, as a static table in the
	// .cpp — editor automation, any future scripting surface and the tests all
	// go through it rather than each carrying its own if-chain, so a renamed
	// field moves in exactly one place.
	//
	// FALSE means the name is not a field of this struct, and NOTHING is written
	// or read: the caller decides whether that is a typo worth asserting on. The
	// three bindless texture indices are deliberately absent — they are
	// descriptor slots the renderer assigns, not authored look parameters.
	//--------------------------------------------------------------------------
	bool SetFloatParamByName(const char* szName, float fValue);
	bool GetFloatParamByName(const char* szName, float& fValueOut) const;
	bool SetColourParamByName(const char* szName, const Zenith_Maths::Vector3& xColour);
	bool GetColourParamByName(const char* szName, Zenith_Maths::Vector3& xColourOut) const;

	// Enumeration over the same two tables, in declaration order — what lets a
	// test walk EVERY name rather than a hand-picked sample. Out-of-range
	// returns nullptr.
	static u_int       GetFloatParamCount();
	static const char* GetFloatParamName(u_int uIndex);
	static u_int       GetColourParamCount();
	static const char* GetColourParamName(u_int uIndex);

	// Explicit, field-by-field — the POD's padding must never reach the file.
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);
};

//=============================================================================
// The fixed-capacity table. Entry 0 is what an unpainted (all-zero) type map
// selects, so it must always be a sensible ground-cover type.
//=============================================================================
class Flux_GrassTypeTable
{
public:
	// A default-constructed table IS the authored default set — an impl that has
	// never been handed a table still draws grass rather than 16 zeroed records.
	Flux_GrassTypeTable();

	// Meadow (0) / Tall (1) / Dry (2) / Flowers (3). Same content as the default
	// constructor; named so a caller restoring the defaults reads as doing so.
	static Flux_GrassTypeTable Defaults() { return Flux_GrassTypeTable(); }

	u_int GetCount() const { return m_uCount; }
	// Clamped to [1, uFLUX_GRASS_MAX_TYPES]: a zero-entry table would make the
	// placement CS clamp the map's type index to -1.
	void SetCount(u_int uCount);

	Flux_GrassTypeParams&       Get(u_int uIndex);
	const Flux_GrassTypeParams& Get(u_int uIndex) const;

	const std::string& GetName(u_int uIndex) const;
	void SetName(u_int uIndex, const std::string& strName);

	// Per-entry texture PATHS ("engine:Vegetation/Grass_Vein_Albedo.ztxtr"), one per
	// FluxGrassTextureSlot. Empty = the slot stays UNBOUND. These are the authored
	// truth the file carries; the bindless indices on the params are derived from
	// them by ResolveTextureIndices below.
	const std::string& GetTexturePath(u_int uIndex, FluxGrassTextureSlot eSlot) const;
	void SetTexturePath(u_int uIndex, FluxGrassTextureSlot eSlot, const std::string& strPath);

	// Maps every live entry's texture paths onto its three bindless indices.
	// pfnResolve is called once per NON-EMPTY path and returns the slot to bind, or
	// uFLUX_GRASS_BINDLESS_UNBOUND when it cannot; an empty path is never resolved
	// and reads UNBOUND. The resolver is injected rather than reached for, so the
	// mapping is testable without a device: Flux_GrassImpl supplies the one that
	// acquires the texture asset and marks it bindless.
	using TextureResolver = u_int (*)(const std::string& strPath, FluxGrassTextureSlot eSlot, void* pUser);
	void ResolveTextureIndices(TextureResolver pfnResolve, void* pUser);

	// How many (entry, slot) pairs across the live entries currently hold a bound
	// (non-UNBOUND) index. Zero for any table straight off disk.
	u_int CountBoundTextures() const;

	// Validate() every live entry + the count.
	void Validate();

	// Project the whole table into a uFLUX_GRASS_MAX_TYPES-entry GPU array. Slots
	// beyond GetCount() are filled with the LAST live entry rather than left
	// undefined: the CS clamps its index, so those slots are unreachable, and a
	// defined value keeps a GPU capture readable.
	void ToGPU(Flux_GrassTypeParamsGPU* paxOut) const;

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	// FALSE means the stream was rejected and THIS TABLE IS UNTOUCHED — never
	// half-written. The whole payload is measured before a single entry is
	// committed, because the stream's own bounds checks merely log and return: a
	// field-by-field read of a truncated file would leave a table that Validate()
	// then makes look plausible instead of plainly reverted.
	bool ReadFromDataStream(Zenith_DataStream& xStream);

private:
	// The authored default set, written straight into this instance. Kept private
	// so there is ONE body: the public Defaults() is just a named default-construct,
	// and having it build a temporary through the constructor would recurse.
	void SetDefaults();

	Flux_GrassTypeParams m_axTypes[uFLUX_GRASS_MAX_TYPES];
	std::string          m_astrNames[uFLUX_GRASS_MAX_TYPES];
	std::string          m_astrTexturePaths[uFLUX_GRASS_MAX_TYPES][FLUX_GRASS_TEXTURE_SLOT_COUNT];
	u_int                m_uCount = 1u;
};
