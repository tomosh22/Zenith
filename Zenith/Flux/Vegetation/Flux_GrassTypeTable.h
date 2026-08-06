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
	u_int                m_uCount = 1u;
};
