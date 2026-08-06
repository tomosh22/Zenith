#include "Zenith.h"
#include "Flux/Vegetation/Flux_GrassTypeTable.h"

#include "DataStream/Zenith_DataStream.h"

#include <cstring>

namespace
{
	// Bumped whenever a field is added, removed or reordered below. There is no
	// backwards-compatible read path: an older/newer file is REJECTED and the
	// caller keeps the defaults, because a half-read table produces plausible
	// grass rather than a visible failure.
	constexpr u_int uGRASS_TYPE_TABLE_VERSION = 1u;

	// Bytes one Flux_GrassTypeParams occupies in the stream. Every serialized field
	// is a 4-byte scalar (the colours go out component-wise), so this is a FIELD
	// COUNT — and it moves with the field list, the same edit that bumps the version
	// above. ReadFromDataStream asserts the real advance against it every entry.
	constexpr u_int uGRASS_TYPE_PARAMS_FIELDS = 43u;
	constexpr uint64_t ulGRASS_TYPE_PARAMS_BYTES = static_cast<uint64_t>(uGRASS_TYPE_PARAMS_FIELDS) * 4ull;

	// THE per-field sanitize step. EVERY float in Validate() below goes through it
	// (the colours via ClampColour), so the two rules are applied uniformly and
	// there is exactly one place to read them:
	//
	//   1. A NON-FINITE value — NaN or either infinity — is REPLACED with the
	//      field's authored default. It is never clamped: a NaN passes a plain
	//      clamp untouched, because `x < lo` and `x > hi` are both false for it,
	//      and it then reaches the GPU as a blade with no size.
	//   2. The now-finite value is clamped into the field's range. Every fallback
	//      passed in is itself in range, so replacement never needs a second pass.
	//
	// The classification is Flux_GrassIsFiniteFloat, which reads the BIT PATTERN.
	// It must never become a float test (`x != x`, std::isnan, a compare against a
	// huge magnitude): this engine's float optimizations let the compiler assume
	// NaN and infinity never occur, so it folds any such test away and the poisoned
	// value sails through — which is exactly what used to happen here. Infinity is
	// replaced rather than clamped for the same reason: clamping it would depend on
	// a float comparison the flags are allowed to fold.
	float ClampFinite(float fValue, float fMin, float fMax, float fFallback)
	{
		const float fSanitized = Flux_GrassIsFiniteFloat(fValue) ? fValue : fFallback;
		return fSanitized < fMin ? fMin : (fSanitized > fMax ? fMax : fSanitized);
	}

	// Order an authored min/max pair without discarding either end: an inverted
	// pair is an authoring slip, not a request for an empty range.
	void OrderPair(float& fMin, float& fMax)
	{
		if (fMin > fMax)
		{
			const float fSwap = fMin;
			fMin = fMax;
			fMax = fSwap;
		}
	}

	Zenith_Maths::Vector3 ClampColour(const Zenith_Maths::Vector3& xColour)
	{
		return Zenith_Maths::Vector3(
			ClampFinite(xColour.x, 0.0f, 1.0f, 0.0f),
			ClampFinite(xColour.y, 0.0f, 1.0f, 0.0f),
			ClampFinite(xColour.z, 0.0f, 1.0f, 0.0f));
	}

	//=========================================================================
	// THE name -> field mapping, in the struct's own declaration order.
	//
	// Pointer-to-member rather than a byte offset: the compiler then type-checks
	// every row against the field it names, so a row that outlives its field
	// fails to compile instead of writing at a stale offset. The names carry no
	// scope/type prefix — they are the authoring vocabulary an automation script
	// and a save file spell out, not C++ identifiers.
	//=========================================================================
	struct GrassFloatParamEntry
	{
		const char* m_szName;
		float Flux_GrassTypeParams::* m_pfField;
	};

	constexpr GrassFloatParamEntry axFLOAT_PARAMS[] =
	{
		// --- shape ---
		{ "HeightMin",             &Flux_GrassTypeParams::m_fHeightMin },
		{ "HeightMax",             &Flux_GrassTypeParams::m_fHeightMax },
		{ "WidthMin",              &Flux_GrassTypeParams::m_fWidthMin },
		{ "WidthMax",              &Flux_GrassTypeParams::m_fWidthMax },
		{ "TiltMinRad",            &Flux_GrassTypeParams::m_fTiltMinRad },
		{ "TiltMaxRad",            &Flux_GrassTypeParams::m_fTiltMaxRad },
		{ "BendMin",               &Flux_GrassTypeParams::m_fBendMin },
		{ "BendMax",               &Flux_GrassTypeParams::m_fBendMax },
		{ "SideCurve",             &Flux_GrassTypeParams::m_fSideCurve },
		{ "VertexDistributionPow", &Flux_GrassTypeParams::m_fVertexDistributionPow },
		{ "FoldThreshold",         &Flux_GrassTypeParams::m_fFoldThreshold },
		{ "FoldPushApart",         &Flux_GrassTypeParams::m_fFoldPushApart },
		{ "TipWidthFrac",          &Flux_GrassTypeParams::m_fTipWidthFrac },

		// --- placement ---
		{ "Density",               &Flux_GrassTypeParams::m_fDensity },
		{ "SlopeMax",              &Flux_GrassTypeParams::m_fSlopeMax },
		{ "MaxDrawDistance",       &Flux_GrassTypeParams::m_fMaxDrawDistance },
		{ "SlopeAlign",            &Flux_GrassTypeParams::m_fSlopeAlign },

		// --- clump ---
		{ "ClumpScale",            &Flux_GrassTypeParams::m_fClumpScale },
		{ "ClumpHeightBoost",      &Flux_GrassTypeParams::m_fClumpHeightBoost },
		{ "ClumpFacingWeight",     &Flux_GrassTypeParams::m_fClumpFacingWeight },
		{ "ClumpOutwardWeight",    &Flux_GrassTypeParams::m_fClumpOutwardWeight },
		{ "ClumpPullToCentre",     &Flux_GrassTypeParams::m_fClumpPullToCentre },
		{ "ClumpNormalBlendMax",   &Flux_GrassTypeParams::m_fClumpNormalBlendMax },

		// --- appearance ---
		{ "GlossRepeat",           &Flux_GrassTypeParams::m_fGlossRepeat },
		{ "RoughnessBase",         &Flux_GrassTypeParams::m_fRoughnessBase },
		{ "Specular",              &Flux_GrassTypeParams::m_fSpecular },
		{ "ColourJitter",          &Flux_GrassTypeParams::m_fColourJitter },
		{ "NormalTilt",            &Flux_GrassTypeParams::m_fNormalTilt },
		{ "TranslucencyBase",      &Flux_GrassTypeParams::m_fTranslucencyBase },
		{ "TranslucencyTip",       &Flux_GrassTypeParams::m_fTranslucencyTip },
		{ "AOBase",                &Flux_GrassTypeParams::m_fAOBase },
		{ "AOTipRelease",          &Flux_GrassTypeParams::m_fAOTipRelease },

		// --- dynamics ---
		{ "WindResponse",          &Flux_GrassTypeParams::m_fWindResponse },
		{ "Stiffness",             &Flux_GrassTypeParams::m_fStiffness },
	};

	struct GrassColourParamEntry
	{
		const char* m_szName;
		Zenith_Maths::Vector3 Flux_GrassTypeParams::* m_pxField;
	};

	constexpr GrassColourParamEntry axCOLOUR_PARAMS[] =
	{
		{ "BaseColour", &Flux_GrassTypeParams::m_xBaseColour },
		{ "TipColour",  &Flux_GrassTypeParams::m_xTipColour },
	};

	constexpr u_int uFLOAT_PARAM_COUNT  = static_cast<u_int>(sizeof(axFLOAT_PARAMS) / sizeof(axFLOAT_PARAMS[0]));
	constexpr u_int uCOLOUR_PARAM_COUNT = static_cast<u_int>(sizeof(axCOLOUR_PARAMS) / sizeof(axCOLOUR_PARAMS[0]));

	// Every serialized field is reachable by name except the three bindless
	// texture indices, which the renderer assigns. A field added to the struct
	// (and therefore to uGRASS_TYPE_PARAMS_FIELDS) without a row here would be
	// silently unauthorable, so the arithmetic is pinned rather than commented.
	static_assert(uFLOAT_PARAM_COUNT + uCOLOUR_PARAM_COUNT * 3u + 3u == uGRASS_TYPE_PARAMS_FIELDS,
		"every serialized field must be either name-addressable or one of the three bindless texture indices");

	const GrassFloatParamEntry* FindFloatParam(const char* szName)
	{
		if (szName == nullptr)
		{
			return nullptr;
		}
		for (u_int u = 0; u < uFLOAT_PARAM_COUNT; u++)
		{
			if (strcmp(axFLOAT_PARAMS[u].m_szName, szName) == 0)
			{
				return &axFLOAT_PARAMS[u];
			}
		}
		return nullptr;
	}

	const GrassColourParamEntry* FindColourParam(const char* szName)
	{
		if (szName == nullptr)
		{
			return nullptr;
		}
		for (u_int u = 0; u < uCOLOUR_PARAM_COUNT; u++)
		{
			if (strcmp(axCOLOUR_PARAMS[u].m_szName, szName) == 0)
			{
				return &axCOLOUR_PARAMS[u];
			}
		}
		return nullptr;
	}
}

//=============================================================================
// Flux_GrassTypeParams
//=============================================================================

void Flux_GrassTypeParams::Validate()
{
	// --- shape ---
	m_fHeightMin = ClampFinite(m_fHeightMin, 0.01f, 8.0f, 0.35f);
	m_fHeightMax = ClampFinite(m_fHeightMax, 0.01f, 8.0f, 0.75f);
	OrderPair(m_fHeightMin, m_fHeightMax);
	m_fWidthMin = ClampFinite(m_fWidthMin, 0.001f, 0.5f, 0.012f);
	m_fWidthMax = ClampFinite(m_fWidthMax, 0.001f, 0.5f, 0.022f);
	OrderPair(m_fWidthMin, m_fWidthMax);
	m_fTiltMinRad = ClampFinite(m_fTiltMinRad, 0.0f, 1.5f, 0.05f);
	m_fTiltMaxRad = ClampFinite(m_fTiltMaxRad, 0.0f, 1.5f, 0.35f);
	OrderPair(m_fTiltMinRad, m_fTiltMaxRad);
	m_fBendMin = ClampFinite(m_fBendMin, 0.0f, 2.0f, 0.10f);
	m_fBendMax = ClampFinite(m_fBendMax, 0.0f, 2.0f, 0.35f);
	OrderPair(m_fBendMin, m_fBendMax);
	m_fSideCurve = ClampFinite(m_fSideCurve, 0.0f, 1.0f, 0.15f);
	// The height remap divides by this exponent, so it can never reach zero.
	m_fVertexDistributionPow = ClampFinite(m_fVertexDistributionPow, 0.25f, 4.0f, 1.6f);
	m_fFoldThreshold = ClampFinite(m_fFoldThreshold, 0.0f, 1.0f, 0.65f);
	m_fFoldPushApart = ClampFinite(m_fFoldPushApart, 0.0f, 1.0f, 0.35f);
	m_fTipWidthFrac = ClampFinite(m_fTipWidthFrac, 0.0f, 1.0f, 0.15f);

	// --- placement ---
	m_fDensity = ClampFinite(m_fDensity, 0.0f, 1.0f, 1.0f);
	m_fSlopeMax = ClampFinite(m_fSlopeMax, 0.0f, 1.0f, 0.70f);
	m_fMaxDrawDistance = Flux_GrassClampMaxDistance(ClampFinite(m_fMaxDrawDistance, 0.0f, 100000.0f, 250.0f));
	m_fSlopeAlign = ClampFinite(m_fSlopeAlign, 0.0f, 1.0f, 0.50f);

	// --- clump ---
	// The Voronoi search divides the world position by this, so a zero cell size
	// would place every blade in one clump at infinity.
	m_fClumpScale = ClampFinite(m_fClumpScale, 0.05f, 64.0f, 3.0f);
	m_fClumpHeightBoost = ClampFinite(m_fClumpHeightBoost, 0.0f, 4.0f, 0.25f);
	m_fClumpFacingWeight = ClampFinite(m_fClumpFacingWeight, 0.0f, 1.0f, 0.60f);
	m_fClumpOutwardWeight = ClampFinite(m_fClumpOutwardWeight, 0.0f, 1.0f, 0.30f);
	m_fClumpPullToCentre = ClampFinite(m_fClumpPullToCentre, 0.0f, 1.0f, 0.15f);
	m_fClumpNormalBlendMax = ClampFinite(m_fClumpNormalBlendMax, 0.0f, 1.0f, 0.50f);

	// --- appearance ---
	m_xBaseColour = ClampColour(m_xBaseColour);
	m_xTipColour = ClampColour(m_xTipColour);
	m_fGlossRepeat = ClampFinite(m_fGlossRepeat, 0.0f, 64.0f, 4.0f);
	m_fRoughnessBase = ClampFinite(m_fRoughnessBase, 0.02f, 1.0f, 0.55f);
	m_fSpecular = ClampFinite(m_fSpecular, 0.0f, 1.0f, 0.35f);
	m_fColourJitter = ClampFinite(m_fColourJitter, 0.0f, 1.0f, 0.15f);
	m_fNormalTilt = ClampFinite(m_fNormalTilt, 0.0f, 1.0f, 0.35f);
	m_fTranslucencyBase = ClampFinite(m_fTranslucencyBase, 0.0f, 1.0f, 0.15f);
	m_fTranslucencyTip = ClampFinite(m_fTranslucencyTip, 0.0f, 1.0f, 0.65f);
	m_fAOBase = ClampFinite(m_fAOBase, 0.0f, 1.0f, 0.25f);
	// The AO release height divides the height fraction; zero would make every
	// fragment fully released and the base occlusion inert.
	m_fAOTipRelease = ClampFinite(m_fAOTipRelease, 0.01f, 1.0f, 0.75f);

	// --- dynamics ---
	m_fWindResponse = ClampFinite(m_fWindResponse, 0.0f, 4.0f, 1.0f);
	m_fStiffness = ClampFinite(m_fStiffness, 0.0f, 4.0f, 0.50f);
}

void Flux_GrassTypeParams::ToGPU(Flux_GrassTypeParamsGPU& xOut) const
{
	// --- shape ---
	xOut.m_fHeightMin = m_fHeightMin;
	xOut.m_fHeightMax = m_fHeightMax;
	xOut.m_fWidthMin = m_fWidthMin;
	xOut.m_fWidthMax = m_fWidthMax;
	xOut.m_fTiltMinRad = m_fTiltMinRad;
	xOut.m_fTiltMaxRad = m_fTiltMaxRad;
	xOut.m_fBendMin = m_fBendMin;
	xOut.m_fBendMax = m_fBendMax;
	xOut.m_fSideCurve = m_fSideCurve;
	xOut.m_fVertexDistributionPow = m_fVertexDistributionPow;
	xOut.m_fFoldThreshold = m_fFoldThreshold;
	xOut.m_fFoldPushApart = m_fFoldPushApart;
	xOut.m_fTipWidthFrac = m_fTipWidthFrac;

	// --- placement ---
	xOut.m_fDensity = m_fDensity;
	xOut.m_fSlopeMax = m_fSlopeMax;
	xOut.m_fMaxDrawDistance = m_fMaxDrawDistance;
	xOut.m_fSlopeAlign = m_fSlopeAlign;

	// --- clump ---
	xOut.m_fClumpScale = m_fClumpScale;
	xOut.m_fClumpHeightBoost = m_fClumpHeightBoost;
	xOut.m_fClumpFacingWeight = m_fClumpFacingWeight;
	xOut.m_fClumpOutwardWeight = m_fClumpOutwardWeight;
	xOut.m_fClumpPullToCentre = m_fClumpPullToCentre;
	xOut.m_fClumpNormalBlendMax = m_fClumpNormalBlendMax;

	// --- appearance (the packed half of the block) ---
	xOut.m_uBaseColourPacked = Flux_GrassPackColourRGB(m_xBaseColour.x, m_xBaseColour.y, m_xBaseColour.z);
	xOut.m_uTipColourPacked = Flux_GrassPackColourRGB(m_xTipColour.x, m_xTipColour.y, m_xTipColour.z);
	xOut.m_uVeinTextureIndex = m_uVeinTextureIndex;
	xOut.m_uGlossTextureIndex = m_uGlossTextureIndex;
	xOut.m_uRampTextureIndex = m_uRampTextureIndex;
	xOut.m_fGlossRepeat = m_fGlossRepeat;
	xOut.m_fRoughnessBase = m_fRoughnessBase;
	xOut.m_fSpecular = m_fSpecular;
	xOut.m_uColourJitterNormalTiltPacked = Flux_GrassPackUnorm2x16(m_fColourJitter, m_fNormalTilt);
	xOut.m_uTranslucencyPacked = Flux_GrassPackUnorm2x16(m_fTranslucencyBase, m_fTranslucencyTip);
	xOut.m_uAOPacked = Flux_GrassPackUnorm2x16(m_fAOBase, m_fAOTipRelease);

	// --- dynamics ---
	xOut.m_fWindResponse = m_fWindResponse;
	xOut.m_fStiffness = m_fStiffness;
}

bool Flux_GrassTypeParams::SetFloatParamByName(const char* szName, float fValue)
{
	const GrassFloatParamEntry* pxEntry = FindFloatParam(szName);
	if (pxEntry == nullptr)
	{
		return false;
	}
	// Deliberately NOT validated here: a caller setting several fields of one
	// pair (HeightMin then HeightMax) would otherwise see the first write
	// re-ordered against a stale second. Validate() is the caller's step, once.
	this->*pxEntry->m_pfField = fValue;
	return true;
}

bool Flux_GrassTypeParams::GetFloatParamByName(const char* szName, float& fValueOut) const
{
	const GrassFloatParamEntry* pxEntry = FindFloatParam(szName);
	if (pxEntry == nullptr)
	{
		return false;
	}
	fValueOut = this->*pxEntry->m_pfField;
	return true;
}

bool Flux_GrassTypeParams::SetColourParamByName(const char* szName, const Zenith_Maths::Vector3& xColour)
{
	const GrassColourParamEntry* pxEntry = FindColourParam(szName);
	if (pxEntry == nullptr)
	{
		return false;
	}
	this->*pxEntry->m_pxField = xColour;
	return true;
}

bool Flux_GrassTypeParams::GetColourParamByName(const char* szName, Zenith_Maths::Vector3& xColourOut) const
{
	const GrassColourParamEntry* pxEntry = FindColourParam(szName);
	if (pxEntry == nullptr)
	{
		return false;
	}
	xColourOut = this->*pxEntry->m_pxField;
	return true;
}

u_int Flux_GrassTypeParams::GetFloatParamCount()
{
	return uFLOAT_PARAM_COUNT;
}

const char* Flux_GrassTypeParams::GetFloatParamName(u_int uIndex)
{
	return uIndex < uFLOAT_PARAM_COUNT ? axFLOAT_PARAMS[uIndex].m_szName : nullptr;
}

u_int Flux_GrassTypeParams::GetColourParamCount()
{
	return uCOLOUR_PARAM_COUNT;
}

const char* Flux_GrassTypeParams::GetColourParamName(u_int uIndex)
{
	return uIndex < uCOLOUR_PARAM_COUNT ? axCOLOUR_PARAMS[uIndex].m_szName : nullptr;
}

void Flux_GrassTypeParams::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_fHeightMin;
	xStream << m_fHeightMax;
	xStream << m_fWidthMin;
	xStream << m_fWidthMax;
	xStream << m_fTiltMinRad;
	xStream << m_fTiltMaxRad;
	xStream << m_fBendMin;
	xStream << m_fBendMax;
	xStream << m_fSideCurve;
	xStream << m_fVertexDistributionPow;
	xStream << m_fFoldThreshold;
	xStream << m_fFoldPushApart;
	xStream << m_fTipWidthFrac;

	xStream << m_fDensity;
	xStream << m_fSlopeMax;
	xStream << m_fMaxDrawDistance;
	xStream << m_fSlopeAlign;

	xStream << m_fClumpScale;
	xStream << m_fClumpHeightBoost;
	xStream << m_fClumpFacingWeight;
	xStream << m_fClumpOutwardWeight;
	xStream << m_fClumpPullToCentre;
	xStream << m_fClumpNormalBlendMax;

	xStream << m_xBaseColour.x;
	xStream << m_xBaseColour.y;
	xStream << m_xBaseColour.z;
	xStream << m_xTipColour.x;
	xStream << m_xTipColour.y;
	xStream << m_xTipColour.z;
	xStream << m_uVeinTextureIndex;
	xStream << m_uGlossTextureIndex;
	xStream << m_uRampTextureIndex;
	xStream << m_fGlossRepeat;
	xStream << m_fRoughnessBase;
	xStream << m_fSpecular;
	xStream << m_fColourJitter;
	xStream << m_fNormalTilt;
	xStream << m_fTranslucencyBase;
	xStream << m_fTranslucencyTip;
	xStream << m_fAOBase;
	xStream << m_fAOTipRelease;

	xStream << m_fWindResponse;
	xStream << m_fStiffness;
}

void Flux_GrassTypeParams::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_fHeightMin;
	xStream >> m_fHeightMax;
	xStream >> m_fWidthMin;
	xStream >> m_fWidthMax;
	xStream >> m_fTiltMinRad;
	xStream >> m_fTiltMaxRad;
	xStream >> m_fBendMin;
	xStream >> m_fBendMax;
	xStream >> m_fSideCurve;
	xStream >> m_fVertexDistributionPow;
	xStream >> m_fFoldThreshold;
	xStream >> m_fFoldPushApart;
	xStream >> m_fTipWidthFrac;

	xStream >> m_fDensity;
	xStream >> m_fSlopeMax;
	xStream >> m_fMaxDrawDistance;
	xStream >> m_fSlopeAlign;

	xStream >> m_fClumpScale;
	xStream >> m_fClumpHeightBoost;
	xStream >> m_fClumpFacingWeight;
	xStream >> m_fClumpOutwardWeight;
	xStream >> m_fClumpPullToCentre;
	xStream >> m_fClumpNormalBlendMax;

	xStream >> m_xBaseColour.x;
	xStream >> m_xBaseColour.y;
	xStream >> m_xBaseColour.z;
	xStream >> m_xTipColour.x;
	xStream >> m_xTipColour.y;
	xStream >> m_xTipColour.z;
	xStream >> m_uVeinTextureIndex;
	xStream >> m_uGlossTextureIndex;
	xStream >> m_uRampTextureIndex;
	xStream >> m_fGlossRepeat;
	xStream >> m_fRoughnessBase;
	xStream >> m_fSpecular;
	xStream >> m_fColourJitter;
	xStream >> m_fNormalTilt;
	xStream >> m_fTranslucencyBase;
	xStream >> m_fTranslucencyTip;
	xStream >> m_fAOBase;
	xStream >> m_fAOTipRelease;

	xStream >> m_fWindResponse;
	xStream >> m_fStiffness;
}

//=============================================================================
// Flux_GrassTypeTable
//=============================================================================

Flux_GrassTypeTable::Flux_GrassTypeTable()
{
	SetDefaults();
}

void Flux_GrassTypeTable::SetDefaults()
{
	m_uCount = 4u;

	// 0 — Meadow. The type an unpainted (all-zero) type map selects everywhere, so
	// it is deliberately the ordinary lawn: mid green base, yellow-green tip, and
	// the ~0.6 m average height the CPU-scatter grass this pipeline replaces had.
	{
		Flux_GrassTypeParams& xType = m_axTypes[0];
		xType = Flux_GrassTypeParams();
		m_astrNames[0] = "Meadow";
		xType.m_fHeightMin = 0.42f;
		xType.m_fHeightMax = 0.78f;
		xType.m_xBaseColour = Zenith_Maths::Vector3(0.13f, 0.26f, 0.08f);
		xType.m_xTipColour = Zenith_Maths::Vector3(0.44f, 0.56f, 0.17f);
	}

	// 1 — Tall. Sparser, much taller, floppier and far more wind-responsive; the
	// low density is what stops a tall type from reading as a solid wall.
	{
		Flux_GrassTypeParams& xType = m_axTypes[1];
		xType = Flux_GrassTypeParams();
		m_astrNames[1] = "Tall";
		xType.m_fHeightMin = 0.95f;
		xType.m_fHeightMax = 1.55f;
		xType.m_fWidthMin = 0.016f;
		xType.m_fWidthMax = 0.030f;
		xType.m_fBendMin = 0.24f;
		xType.m_fBendMax = 0.58f;
		xType.m_fTiltMaxRad = 0.48f;
		xType.m_fDensity = 0.45f;
		xType.m_fClumpScale = 4.5f;
		xType.m_fClumpHeightBoost = 0.35f;
		xType.m_xBaseColour = Zenith_Maths::Vector3(0.11f, 0.23f, 0.07f);
		xType.m_xTipColour = Zenith_Maths::Vector3(0.50f, 0.58f, 0.22f);
		xType.m_fTranslucencyTip = 0.78f;
		xType.m_fWindResponse = 1.6f;
		xType.m_fStiffness = 0.30f;
	}

	// 2 — Dry. Short, stiff, straw-coloured and steep-tolerant: this is the type
	// that should survive on the slopes and verges the others are culled off.
	{
		Flux_GrassTypeParams& xType = m_axTypes[2];
		xType = Flux_GrassTypeParams();
		m_astrNames[2] = "Dry";
		xType.m_fHeightMin = 0.20f;
		xType.m_fHeightMax = 0.42f;
		xType.m_fBendMin = 0.05f;
		xType.m_fBendMax = 0.18f;
		xType.m_fSlopeMax = 0.85f;
		xType.m_fSlopeAlign = 0.70f;
		xType.m_fDensity = 0.80f;
		xType.m_fFoldThreshold = 0.45f;   // more folded blades = a scruffier silhouette
		xType.m_xBaseColour = Zenith_Maths::Vector3(0.26f, 0.22f, 0.09f);
		xType.m_xTipColour = Zenith_Maths::Vector3(0.62f, 0.55f, 0.26f);
		xType.m_fRoughnessBase = 0.72f;
		xType.m_fSpecular = 0.20f;
		xType.m_fTranslucencyBase = 0.08f;
		xType.m_fTranslucencyTip = 0.40f;
		xType.m_fWindResponse = 0.6f;
		xType.m_fStiffness = 1.10f;
	}

	// 3 — Flowers. Very sparse, tightly clumped and bright-tipped. The tight clump
	// cell plus the high facing weight is what makes a patch read as one plant
	// instead of scattered confetti.
	{
		Flux_GrassTypeParams& xType = m_axTypes[3];
		xType = Flux_GrassTypeParams();
		m_astrNames[3] = "Flowers";
		xType.m_fHeightMin = 0.30f;
		xType.m_fHeightMax = 0.55f;
		xType.m_fWidthMin = 0.020f;
		xType.m_fWidthMax = 0.034f;
		xType.m_fTipWidthFrac = 0.55f;    // a broad tip is the closest the blade mesh gets to a petal
		xType.m_fDensity = 0.14f;
		xType.m_fClumpScale = 1.6f;
		xType.m_fClumpPullToCentre = 0.42f;
		xType.m_fClumpFacingWeight = 0.80f;
		xType.m_fClumpOutwardWeight = 0.55f;
		xType.m_fMaxDrawDistance = 90.0f;  // sub-pixel confetti past this is pure shimmer
		xType.m_xBaseColour = Zenith_Maths::Vector3(0.14f, 0.27f, 0.10f);
		xType.m_xTipColour = Zenith_Maths::Vector3(0.86f, 0.78f, 0.42f);
		xType.m_fColourJitter = 0.30f;
		xType.m_fTranslucencyTip = 0.85f;
		xType.m_fWindResponse = 1.2f;
	}

	Validate();
}

void Flux_GrassTypeTable::SetCount(u_int uCount)
{
	if (uCount < 1u)
	{
		uCount = 1u;
	}
	if (uCount > uFLUX_GRASS_MAX_TYPES)
	{
		uCount = uFLUX_GRASS_MAX_TYPES;
	}
	m_uCount = uCount;
}

Flux_GrassTypeParams& Flux_GrassTypeTable::Get(u_int uIndex)
{
	Zenith_Assert(uIndex < uFLUX_GRASS_MAX_TYPES, "Grass type index %u out of range (max %u)", uIndex, uFLUX_GRASS_MAX_TYPES);
	return m_axTypes[uIndex < uFLUX_GRASS_MAX_TYPES ? uIndex : 0u];
}

const Flux_GrassTypeParams& Flux_GrassTypeTable::Get(u_int uIndex) const
{
	Zenith_Assert(uIndex < uFLUX_GRASS_MAX_TYPES, "Grass type index %u out of range (max %u)", uIndex, uFLUX_GRASS_MAX_TYPES);
	return m_axTypes[uIndex < uFLUX_GRASS_MAX_TYPES ? uIndex : 0u];
}

const std::string& Flux_GrassTypeTable::GetName(u_int uIndex) const
{
	Zenith_Assert(uIndex < uFLUX_GRASS_MAX_TYPES, "Grass type index %u out of range (max %u)", uIndex, uFLUX_GRASS_MAX_TYPES);
	return m_astrNames[uIndex < uFLUX_GRASS_MAX_TYPES ? uIndex : 0u];
}

void Flux_GrassTypeTable::SetName(u_int uIndex, const std::string& strName)
{
	Zenith_Assert(uIndex < uFLUX_GRASS_MAX_TYPES, "Grass type index %u out of range (max %u)", uIndex, uFLUX_GRASS_MAX_TYPES);
	if (uIndex < uFLUX_GRASS_MAX_TYPES)
	{
		m_astrNames[uIndex] = strName;
	}
}

void Flux_GrassTypeTable::Validate()
{
	SetCount(m_uCount);
	for (u_int u = 0; u < uFLUX_GRASS_MAX_TYPES; u++)
	{
		m_axTypes[u].Validate();
	}
}

void Flux_GrassTypeTable::ToGPU(Flux_GrassTypeParamsGPU* paxOut) const
{
	if (paxOut == nullptr)
	{
		return;
	}
	const u_int uLive = m_uCount > 0u ? m_uCount : 1u;
	for (u_int u = 0; u < uFLUX_GRASS_MAX_TYPES; u++)
	{
		m_axTypes[u < uLive ? u : uLive - 1u].ToGPU(paxOut[u]);
	}
}

void Flux_GrassTypeTable::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << uGRASS_TYPE_TABLE_VERSION;
	xStream << m_uCount;
	for (u_int u = 0; u < m_uCount; u++)
	{
		xStream << m_astrNames[u];
		// Called explicitly rather than via <<: the params POD is trivially
		// copyable, so the operator would take the raw-memcpy branch and leak
		// its padding into the file.
		m_axTypes[u].WriteToDataStream(xStream);
	}
}

bool Flux_GrassTypeTable::ReadFromDataStream(Zenith_DataStream& xStream)
{
	const uint64_t ulStreamEnd = xStream.GetCapacity();
	if (xStream.GetCursor() + sizeof(u_int) * 2u > ulStreamEnd)
	{
		Zenith_Error(LOG_CATEGORY_RENDERER,
			"Flux_GrassTypeTable: stream is too short to hold a header — keeping the current table");
		return false;
	}

	u_int uVersion = 0u;
	xStream >> uVersion;
	if (uVersion != uGRASS_TYPE_TABLE_VERSION)
	{
		// No migration path by design: a partially-read table produces plausible
		// grass, which is far harder to spot than a table that plainly reverted.
		Zenith_Error(LOG_CATEGORY_RENDERER,
			"Flux_GrassTypeTable: version %u is not %u — keeping the current table",
			uVersion, uGRASS_TYPE_TABLE_VERSION);
		return false;
	}

	u_int uCount = 0u;
	xStream >> uCount;
	// Zero is as malformed as overflow: SetCount clamps to [1, MAX] on the authoring
	// side, so neither end can come from a file this engine wrote.
	if (uCount == 0u || uCount > uFLUX_GRASS_MAX_TYPES)
	{
		Zenith_Error(LOG_CATEGORY_RENDERER,
			"Flux_GrassTypeTable: type count %u is outside [1, %u] — keeping the current table",
			uCount, uFLUX_GRASS_MAX_TYPES);
		return false;
	}

	// MEASURING PASS. The reader below clears every slot before it fills any, so a
	// truncated payload must be rejected BEFORE that clear — the stream's bounds
	// checks only log and return, so reading past the end would leave the table
	// half-written rather than reverted. Walk the payload with the cursor (each
	// entry is a 4-byte name length + its chars + the fixed params block), then
	// rewind and read for real.
	const uint64_t ulPayloadStart = xStream.GetCursor();
	bool bComplete = true;
	for (u_int u = 0; u < uCount; u++)
	{
		if (xStream.GetCursor() + sizeof(u_int) > ulStreamEnd)
		{
			bComplete = false;
			break;
		}
		u_int uNameLength = 0u;
		xStream >> uNameLength;
		const uint64_t ulEntryBytes = static_cast<uint64_t>(uNameLength) + ulGRASS_TYPE_PARAMS_BYTES;
		if (xStream.GetCursor() + ulEntryBytes > ulStreamEnd)
		{
			bComplete = false;
			break;
		}
		xStream.SkipBytes(static_cast<u_int>(ulEntryBytes));
	}
	xStream.SetCursor(ulPayloadStart);

	if (!bComplete)
	{
		Zenith_Error(LOG_CATEGORY_RENDERER,
			"Flux_GrassTypeTable: stream ends part-way through its %u types — keeping the current table", uCount);
		return false;
	}

	// Every slot resets first so a shorter file never leaves a longer table's
	// tail entries visible behind the new count.
	for (u_int u = 0; u < uFLUX_GRASS_MAX_TYPES; u++)
	{
		m_axTypes[u] = Flux_GrassTypeParams();
		m_astrNames[u].clear();
	}

	for (u_int u = 0; u < uCount; u++)
	{
		xStream >> m_astrNames[u];
		const uint64_t ulBeforeParams = xStream.GetCursor();
		m_axTypes[u].ReadFromDataStream(xStream);
		// Catches ulGRASS_TYPE_PARAMS_BYTES drifting away from the field list: the
		// measuring pass above budgets by that constant, so a stale one would size
		// the payload wrongly instead of failing to compile.
		Zenith_Assert(xStream.GetCursor() - ulBeforeParams == ulGRASS_TYPE_PARAMS_BYTES,
			"Flux_GrassTypeParams read %llu bytes, not the budgeted %llu",
			xStream.GetCursor() - ulBeforeParams, ulGRASS_TYPE_PARAMS_BYTES);
	}

	SetCount(uCount);
	Validate();
	return true;
}
