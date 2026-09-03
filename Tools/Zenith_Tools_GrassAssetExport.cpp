#include "Zenith.h"

//=============================================================================
// Zenith_Tools_GrassAssetExport
//
// The three per-blade textures the grass fragment stage has always been able
// to sample and that NO game shipped, plus the authored type table that binds
// them. Generated at every tools boot from fixed seeds, exactly like the rock,
// deadwood and bush sets beside it.
//
//   ENGINE_ASSETS_DIR/Vegetation/
//     Grass_Blade_Vein.ztxtr    256 x 64, sRGB   — midrib + fine veins across
//                                                  the blade, warming toward
//                                                  the tip.  REPEAT.
//     Grass_Blade_Gloss.ztxtr   256 x 64, linear — gloss streaks ALONG the
//                                                  blade (R channel).  REPEAT.
//     Grass_Clump_Ramp.ztxtr    256 x 256, sRGB  — u = clump hash, v = base ->
//                                                  tip.  CLAMP.
//
//   GAME_ASSETS_DIR/Vegetation/
//     GrassTypes.zdata          — the authored Flux_GrassTypeTable (4 types)
//     GrassTypes.gen            — this generator's version marker (see below)
//
// ★ THE TEXTURES ARE ENGINE ASSETS, THE TABLE IS A GAME ASSET, and that split
// is forced by the engine: Flux_GrassImpl boot-loads exactly one path,
// `game:Vegetation/GrassTypes.zdata` (szZENITH_GRASS_TYPE_TABLE_ASSET_PATH),
// while the pixels are shared by every game that grows grass. The table
// carries the textures by PATH and the renderer resolves each path to a
// bindless slot at load — a descriptor index from a previous boot is
// meaningless in this one, so it never reaches the file.
//
// ★ THE TABLE IS AUTHORABLE, so this generator must not clobber an author.
// The terrain editor's "Save grass types" button writes the same path, and a
// generator that overwrote it every boot would quietly delete a session's
// work. Hence the marker file: this exporter writes the table when there is
// none, or when the marker says the table it wrote last is STALE, and
// otherwise leaves an unmarked (i.e. hand-authored) file completely alone.
// Bumping uGRASS_ASSET_EXPORT_VERSION is what makes a tools boot re-bake.
//
// ★ EVERY MAP THAT REPEATS MUST TILE, and the two blade maps repeat: the
// gloss lookup deliberately runs PAST u = 1 (it is indexed by
// height * GlossRepeat, default 4), and the vein map is sampled across a
// blade whose sampler is set to REPEAT. So both are built on the wrapped
// integer lattice (Zenith_TerrainNoise::Tileable*) rather than on plain FBM,
// the same reason the rock set uses it. The ramp is CLAMPed by the resolver
// and is the one map here that does not need to wrap.
//
// ★ THE VEIN AND RAMP MAPS MULTIPLY the blade's albedo (they do not replace
// it), so both are authored close to WHITE. A map centred on mid-grey halves
// the brightness of every blade in the world, which reads as "the grass got
// darker" rather than as a texture bug.
//=============================================================================

#ifndef ZENITH_TOOLS

// Asset generation is a tools-build capability; non-tools builds get a no-op
// so GenerateTestAssets links.
void GenerateGrassAssets()
{
}

#else

#include "Zenith_Tools_TestAssetExport.h"

#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_GrassTypeTableAsset.h"
#include "Zenith_Tools_TextureExport.h"   // the ONE .ztxtr writer
#include "Collections/Zenith_Vector.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Flux/Vegetation/Flux_GrassTypeTable.h"
#include "Maths/Zenith_Noise.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{

//=============================================================================
// Export version. Unlike the mesh sets, this one is NOT regenerated blindly:
// the type table it writes is also an authorable file, so the version is a
// real staleness gate rather than a log line. Bump it whenever the emitted
// textures or the authored type set change, and the next tools boot re-bakes
// a table this generator previously wrote.
//
// 1: initial vein / gloss / ramp maps + the four-type table that binds them.
//=============================================================================
constexpr u_int uGRASS_ASSET_EXPORT_VERSION = 1u;

// Blade maps: wide in the sampled axis, short in the other. The vein map's u
// runs ACROSS the blade and its v from base to tip; the gloss map's u is
// height * GlossRepeat and it is sampled at v = 0.5, so its height exists only
// to keep the mip chain square-ish and cheap.
constexpr int32_t iBLADE_MAP_WIDTH = 256;
constexpr int32_t iBLADE_MAP_HEIGHT = 64;
constexpr int32_t iRAMP_SIZE = 256;

constexpr u_int uVEIN_SEED = 61403u;
constexpr u_int uGLOSS_SEED = 24107u;
constexpr u_int uRAMP_SEED = 88301u;

using Zenith_TerrainNoise::TileableValueNoise;
using Zenith_TerrainNoise::TileableFBM;

float SmoothStepG(float fEdge0, float fEdge1, float fX)
{
	const float fT = std::clamp((fX - fEdge0) / (fEdge1 - fEdge0), 0.0f, 1.0f);
	return fT * fT * (3.0f - 2.0f * fT);
}

u_int8 ToByte(float fValue)
{
	return static_cast<u_int8>(std::clamp(fValue, 0.0f, 1.0f) * 255.0f);
}

//=============================================================================
// The blade VEIN map: what a blade of grass looks like ACROSS its width.
//
// fU runs 0 -> 1 across the blade (the fragment stage feeds it side * 0.5 +
// 0.5), fV from base to tip. The result MULTIPLIES the type's base->tip
// colour, so it lives just under white.
//
// Symmetric about the midrib by construction — fAcross uses |fU - 0.5| — which
// is also what makes the map tile across a REPEAT sampler: column 0 and the
// last column are both the blade's EDGE, and a blade whose two edges did not
// match would show a seam down every blade in the world.
//=============================================================================
void GrassVeinTexel(float fU, float fV, float& fROut, float& fGOut, float& fBOut)
{
	const float fAcross = fabsf(fU - 0.5f) * 2.0f;   // 0 = midrib, 1 = edge

	// The midrib is the one bright structure on a blade; the flanks fall away
	// from it, and the very edge is thin enough to read darker again.
	float fValue = 0.86f + 0.14f * SmoothStepG(0.22f, 0.0f, fAcross);
	fValue *= 1.0f - 0.10f * SmoothStepG(0.80f, 1.0f, fAcross);

	// Fine veins parallel to the midrib. Wrapped noise, so they survive REPEAT.
	const float fVeins = TileableValueNoise(fU, fV, 9, 3, uVEIN_SEED);
	fValue *= 0.96f + 0.06f * fVeins;

	// Blade-scale mottle so no two sampled bands look identical.
	const float fMottle = TileableFBM(fU, fV, 3, 5, 3u, uVEIN_SEED + 91u);
	fValue *= 0.95f + 0.08f * fMottle;

	// Tips dry out first: a slight straw shift over the top fifth. This is a
	// TINT, not a darkening — the type's own tip colour does the value change.
	const float fTip = SmoothStepG(0.80f, 1.0f, fV);
	fROut = std::clamp(fValue * (1.0f + 0.10f * fTip), 0.0f, 1.0f);
	fGOut = std::clamp(fValue * (1.0f + 0.02f * fTip), 0.0f, 1.0f);
	fBOut = std::clamp(fValue * (1.0f - 0.16f * fTip), 0.0f, 1.0f);
}

void GenerateGrassVeinPixels(Zenith_Vector<u_int8>& xPixels)
{
	xPixels.Clear();
	xPixels.Resize(iBLADE_MAP_WIDTH * iBLADE_MAP_HEIGHT * 4, 0);
	for (int32_t iY = 0; iY < iBLADE_MAP_HEIGHT; iY++)
	{
		for (int32_t iX = 0; iX < iBLADE_MAP_WIDTH; iX++)
		{
			const float fU = static_cast<float>(iX) / iBLADE_MAP_WIDTH;
			const float fV = static_cast<float>(iY) / iBLADE_MAP_HEIGHT;
			float fR = 0.0f, fG = 0.0f, fB = 0.0f;
			GrassVeinTexel(fU, fV, fR, fG, fB);
			u_int8* pucP = &xPixels.Get((iY * iBLADE_MAP_WIDTH + iX) * 4);
			pucP[0] = ToByte(fR);
			pucP[1] = ToByte(fG);
			pucP[2] = ToByte(fB);
			pucP[3] = 255;
		}
	}
}

//=============================================================================
// The GLOSS streak map: where along a blade the cuticle is shiny.
//
// The fragment stage reads R only, at (height * GlossRepeat, 0.5), and
// SUBTRACTS the value from roughness — so 0 is "as authored" and 1 is "wet
// polished". A real blade is glossiest in bands, and the bands are what make
// a lawn glitter as the camera moves instead of shading as flat felt.
//
// Indexed past u = 1 by construction (GlossRepeat defaults to 4), so it MUST
// wrap: the whole map is built on the wrapped lattice.
//=============================================================================
float GrassGlossTexel(float fU)
{
	// Two wrapped octaves: broad bands with a finer flicker inside them.
	const float fBroad = TileableValueNoise(fU, 0.5f, 5, 1, uGLOSS_SEED);
	const float fFine = TileableValueNoise(fU, 0.5f, 17, 1, uGLOSS_SEED + 37u);
	const float fStreak = fBroad * 0.68f + fFine * 0.32f;
	// Thresholded so most of the blade is matte and the highlights are streaks
	// rather than an overall sheen — a uniformly glossy blade reads as plastic.
	//
	// ★ THE WINDOW IS SET FROM THE STREAK'S MEASURED RANGE, NOT FROM [0,1]. A
	// blend of two value-noise octaves is not uniform: measured over the shipped
	// 256-texel row it spans ~[0.34, 0.66] about a mean of 0.50, so the original
	// (0.46, 0.86) window put its upper edge above anything the function can
	// produce — the exported map peaked at 0.494 and had NO highlights at all,
	// which is the one thing a gloss map exists to have.
	return SmoothStepG(0.50f, 0.63f, fStreak);
}

void GenerateGrassGlossPixels(Zenith_Vector<u_int8>& xPixels)
{
	xPixels.Clear();
	xPixels.Resize(iBLADE_MAP_WIDTH * iBLADE_MAP_HEIGHT * 4, 0);
	for (int32_t iX = 0; iX < iBLADE_MAP_WIDTH; iX++)
	{
		const float fU = static_cast<float>(iX) / iBLADE_MAP_WIDTH;
		const u_int8 ucGloss = ToByte(GrassGlossTexel(fU));
		// Constant down the column: the lookup is 1D (it samples v = 0.5), so a
		// vertical variation would be invisible AND would blur into the R
		// channel through the mip chain.
		for (int32_t iY = 0; iY < iBLADE_MAP_HEIGHT; iY++)
		{
			u_int8* pucP = &xPixels.Get((iY * iBLADE_MAP_WIDTH + iX) * 4);
			pucP[0] = ucGloss;
			pucP[1] = ucGloss;
			pucP[2] = ucGloss;
			pucP[3] = 255;
		}
	}
}

//=============================================================================
// The clump colour RAMP: u = the clump's hash, v = base -> tip.
//
// This is the map that stops a field reading as one colour. Every blade in a
// clump shares a hash, so a column of this ramp is "what this tuft looks
// like": some tufts are lusher, some are drying, a few are bleached. It
// MULTIPLIES the type colour, so it sits just under white; the sampler CLAMPs
// (a clump hash of exactly 1 must not wrap onto the first column), so unlike
// the blade maps it does not have to tile.
//=============================================================================
void GrassRampTexel(float fU, float fV, float& fROut, float& fGOut, float& fBOut)
{
	// Per-clump character, low frequency in u so neighbouring hashes differ but
	// a single clump is internally consistent.
	const float fClump = TileableValueNoise(fU, 0.25f, 7, 1, uRAMP_SEED);
	const float fClumpFine = TileableValueNoise(fU, 0.75f, 23, 1, uRAMP_SEED + 53u);
	const float fCharacter = std::clamp(fClump * 0.72f + fClumpFine * 0.28f, 0.0f, 1.0f);

	// Lush (green, slightly darker at the base) <-> dry (straw, brighter tip).
	const float fDry = SmoothStepG(0.55f, 0.95f, fCharacter);

	// Base is shaded by its own canopy; the tip is the part that catches sun.
	const float fHeightLift = 0.82f + 0.22f * fV;

	const float fR = fHeightLift * (0.92f + 0.16f * fDry);
	const float fG = fHeightLift * (1.00f - 0.04f * fDry);
	const float fB = fHeightLift * (0.90f - 0.26f * fDry);

	fROut = std::clamp(fR, 0.0f, 1.0f);
	fGOut = std::clamp(fG, 0.0f, 1.0f);
	fBOut = std::clamp(fB, 0.0f, 1.0f);
}

void GenerateGrassRampPixels(Zenith_Vector<u_int8>& xPixels)
{
	xPixels.Clear();
	xPixels.Resize(iRAMP_SIZE * iRAMP_SIZE * 4, 0);
	for (int32_t iY = 0; iY < iRAMP_SIZE; iY++)
	{
		for (int32_t iX = 0; iX < iRAMP_SIZE; iX++)
		{
			const float fU = static_cast<float>(iX) / iRAMP_SIZE;
			const float fV = static_cast<float>(iY) / iRAMP_SIZE;
			float fR = 0.0f, fG = 0.0f, fB = 0.0f;
			GrassRampTexel(fU, fV, fR, fG, fB);
			u_int8* pucP = &xPixels.Get((iY * iRAMP_SIZE + iX) * 4);
			pucP[0] = ToByte(fR);
			pucP[1] = ToByte(fG);
			pucP[2] = ToByte(fB);
			pucP[3] = 255;
		}
	}
}

void GenerateGrassBladeTextures(const std::string& strDir)
{
	Zenith_Vector<u_int8> xPixels;

	// Colour maps go out sRGB with a full offline mip chain, uncompressed: at
	// 256x64 the BC1 block artefacts across a 4-texel midrib would be a bigger
	// error than the whole file is large.
	GenerateGrassVeinPixels(xPixels);
	Zenith_Tools_TextureExport::ExportFromDataV2Uncompressed(
		xPixels.GetDataPointer(), strDir + "Grass_Blade_Vein" ZENITH_TEXTURE_EXT,
		iBLADE_MAP_WIDTH, iBLADE_MAP_HEIGHT, TEXTURE_FORMAT_RGBA8_SRGB);

	// Gloss is DATA (it is subtracted from roughness), so it stays linear.
	GenerateGrassGlossPixels(xPixels);
	Zenith_Tools_TextureExport::ExportFromDataV2Uncompressed(
		xPixels.GetDataPointer(), strDir + "Grass_Blade_Gloss" ZENITH_TEXTURE_EXT,
		iBLADE_MAP_WIDTH, iBLADE_MAP_HEIGHT, TEXTURE_FORMAT_RGBA8_UNORM);

	GenerateGrassRampPixels(xPixels);
	Zenith_Tools_TextureExport::ExportFromDataV2Uncompressed(
		xPixels.GetDataPointer(), strDir + "Grass_Clump_Ramp" ZENITH_TEXTURE_EXT,
		iRAMP_SIZE, iRAMP_SIZE, TEXTURE_FORMAT_RGBA8_SRGB);
}

//=============================================================================
// The authored type table.
//
// THE GEOMETRY RANGES ARE THE SHIPPED BUILT-INS', deliberately: heights,
// widths, bends and clump scales in Flux_GrassTypeTable::Defaults() are what
// every capture and every tuning session so far has been against, and a table
// that both re-skinned AND re-shaped the grass would make a regression
// impossible to bisect. What this table changes is APPEARANCE — the three
// texture bindings, the colours, roughness and translucency — plus one new
// silhouette (Trampled) the built-ins have no equivalent of.
//
// Entry 0 is what an unpainted (all-zero) type map selects everywhere, so it
// stays the ordinary lawn.
//=============================================================================
const char* const szVEIN_PATH  = "engine:Vegetation/Grass_Blade_Vein" ZENITH_TEXTURE_EXT;
const char* const szGLOSS_PATH = "engine:Vegetation/Grass_Blade_Gloss" ZENITH_TEXTURE_EXT;
const char* const szRAMP_PATH  = "engine:Vegetation/Grass_Clump_Ramp" ZENITH_TEXTURE_EXT;

constexpr u_int uGRASS_TYPE_COUNT = 4u;

void BindGrassTextures(Flux_GrassTypeTable& xTable, u_int uIndex)
{
	xTable.SetTexturePath(uIndex, FLUX_GRASS_TEXTURE_VEIN, szVEIN_PATH);
	xTable.SetTexturePath(uIndex, FLUX_GRASS_TEXTURE_GLOSS, szGLOSS_PATH);
	xTable.SetTexturePath(uIndex, FLUX_GRASS_TEXTURE_RAMP, szRAMP_PATH);
}

void BuildGrassTypeTable(Flux_GrassTypeTable& xTable)
{
	// Start from the built-ins so every field this function does not mention is
	// the shipped value, not a struct default that happens to look similar.
	const Flux_GrassTypeTable xBuiltIn = Flux_GrassTypeTable::Defaults();
	xTable = Flux_GrassTypeTable();
	xTable.SetCount(uGRASS_TYPE_COUNT);

	// 0 — Lush meadow. Built-in Meadow's geometry; the ramp and vein maps do the
	// per-clump and across-blade variation the flat colours could not.
	{
		Flux_GrassTypeParams& xType = xTable.Get(0);
		xType = xBuiltIn.Get(0);
		xTable.SetName(0, "Meadow");
		xType.m_fRoughnessBase = 0.48f;      // wet-ish cuticle; the gloss map cuts further
		xType.m_fSpecular = 0.42f;
		xType.m_fGlossRepeat = 3.5f;
		xType.m_fTranslucencyBase = 0.18f;
		xType.m_fTranslucencyTip = 0.72f;
		xType.m_fColourJitter = 0.12f;       // the ramp now carries most of the variation
		BindGrassTextures(xTable, 0);
	}

	// 1 — Dry straw. Built-in Dry's geometry (short, stiff, steep-tolerant).
	// PALER, ROUGHER and far less translucent: a dead blade is a tube of
	// cellulose, not a lit leaf, and getting that wrong makes a summer verge
	// glow like spring grass.
	{
		Flux_GrassTypeParams& xType = xTable.Get(1);
		xType = xBuiltIn.Get(2);
		xTable.SetName(1, "DryStraw");
		xType.m_xBaseColour = Zenith_Maths::Vector3(0.34f, 0.30f, 0.14f);
		xType.m_xTipColour = Zenith_Maths::Vector3(0.72f, 0.66f, 0.36f);
		xType.m_fRoughnessBase = 0.86f;
		xType.m_fSpecular = 0.12f;
		xType.m_fGlossRepeat = 6.0f;         // what gloss there is, is broken up
		xType.m_fTranslucencyBase = 0.05f;
		xType.m_fTranslucencyTip = 0.28f;
		xType.m_fColourJitter = 0.20f;
		BindGrassTextures(xTable, 1);
	}

	// 2 — Short trampled. The one silhouette the built-ins have no equivalent
	// of: a path or a lawn edge, pressed flat and worn. Dense but low, barely
	// bent (it has already been bent), and browner at the base where the soil
	// shows through.
	{
		Flux_GrassTypeParams& xType = xTable.Get(2);
		xType = xBuiltIn.Get(0);
		xTable.SetName(2, "Trampled");
		xType.m_fHeightMin = 0.08f;
		xType.m_fHeightMax = 0.19f;
		xType.m_fTiltMinRad = 0.35f;         // already leaning
		xType.m_fTiltMaxRad = 0.85f;
		xType.m_fBendMin = 0.05f;
		xType.m_fBendMax = 0.16f;
		xType.m_fDensity = 1.0f;
		xType.m_fSlopeAlign = 0.85f;         // hugs the ground it was pressed into
		xType.m_fClumpScale = 1.2f;
		xType.m_fClumpHeightBoost = 0.10f;
		xType.m_xBaseColour = Zenith_Maths::Vector3(0.15f, 0.20f, 0.08f);
		xType.m_xTipColour = Zenith_Maths::Vector3(0.36f, 0.44f, 0.16f);
		xType.m_fRoughnessBase = 0.72f;
		xType.m_fSpecular = 0.22f;
		xType.m_fTranslucencyBase = 0.08f;
		xType.m_fTranslucencyTip = 0.34f;
		xType.m_fAOBase = 0.40f;             // pressed blades occlude each other
		xType.m_fWindResponse = 0.35f;
		xType.m_fStiffness = 0.90f;
		BindGrassTextures(xTable, 2);
	}

	// 3 — Coarse tussock. Built-in Tall's geometry: sparse, tall, floppy,
	// tightly clumped. Wide blades and a strong clump height boost are what
	// make it read as separate tufts standing out of shorter cover rather than
	// as a taller lawn.
	{
		Flux_GrassTypeParams& xType = xTable.Get(3);
		xType = xBuiltIn.Get(1);
		xTable.SetName(3, "Tussock");
		xType.m_fWidthMin = 0.020f;
		xType.m_fWidthMax = 0.038f;
		xType.m_fDensity = 0.38f;
		xType.m_fClumpScale = 2.2f;
		xType.m_fClumpHeightBoost = 0.48f;
		xType.m_fClumpPullToCentre = 0.34f;
		xType.m_xBaseColour = Zenith_Maths::Vector3(0.12f, 0.21f, 0.07f);
		xType.m_xTipColour = Zenith_Maths::Vector3(0.52f, 0.54f, 0.24f);
		xType.m_fRoughnessBase = 0.66f;
		xType.m_fSpecular = 0.26f;
		xType.m_fGlossRepeat = 5.0f;
		xType.m_fTranslucencyTip = 0.68f;
		xType.m_fColourJitter = 0.18f;
		BindGrassTextures(xTable, 3);
	}

	// Clamp every field into a range the placement CS and vertex stage survive,
	// on the way IN — so what is written is what is read back.
	xTable.Validate();
}

//=============================================================================
// The version marker. `game:Vegetation/GrassTypes.gen` holds the version of
// the generator that wrote the .zdata beside it, and NOTHING else reads it:
// it exists so this exporter can tell its own output from an author's.
//=============================================================================
std::filesystem::path GrassTypeTablePath()
{
	return std::filesystem::path(Zenith_AssetRegistry::ResolvePath(szZENITH_GRASS_TYPE_TABLE_ASSET_PATH));
}

std::filesystem::path GrassMarkerPath()
{
	return GrassTypeTablePath().parent_path() / "GrassTypes.gen";
}

// The version the marker records, or 0 when there is no readable marker.
u_int ReadGrassMarkerVersion()
{
	std::error_code xEC;
	const std::filesystem::path xMarker = GrassMarkerPath();
	if (!std::filesystem::exists(xMarker, xEC) || xEC)
	{
		return 0u;
	}
	std::ifstream xIn(xMarker);
	u_int uVersion = 0u;
	if (!(xIn >> uVersion))
	{
		return 0u;
	}
	return uVersion;
}

void WriteGrassMarker()
{
	std::ofstream xOut(GrassMarkerPath(), std::ios::trunc);
	xOut << uGRASS_ASSET_EXPORT_VERSION << "\n";
}

// TRUE when this boot should (re)write the table. Three cases, and the third
// is the one that protects an author:
//   * no table at all                  -> write it,
//   * our marker, but a stale version   -> rewrite it,
//   * a table with no marker of ours    -> LEAVE IT, it is hand-authored.
bool ShouldWriteGrassTypeTable(const char*& szReasonOut)
{
	std::error_code xEC;
	const bool bTableExists = std::filesystem::exists(GrassTypeTablePath(), xEC) && !xEC;
	if (!bTableExists)
	{
		szReasonOut = "no table on disk";
		return true;
	}

	const u_int uMarker = ReadGrassMarkerVersion();
	if (uMarker == 0u)
	{
		szReasonOut = "an existing table carries no generator marker (treated as hand-authored)";
		return false;
	}
	if (uMarker != uGRASS_ASSET_EXPORT_VERSION)
	{
		szReasonOut = "the generated table is stale";
		return true;
	}
	szReasonOut = "the generated table is current";
	return false;
}

void GenerateGrassTypeTableAsset()
{
	const char* szReason = "";
	const bool bWrite = ShouldWriteGrassTypeTable(szReason);
	if (!bWrite)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "  grass type table: skipped (%s)", szReason);
		return;
	}

	Flux_GrassTypeTable xTable;
	BuildGrassTypeTable(xTable);

	// Through the ASSET class, never a hand-rolled stream: the .zdata envelope
	// (magic + version + type name) is what makes the file loadable, and
	// Flux_GrassImpl::LoadAuthoredTypeTable reads it back through exactly this
	// type. A local, not a Create<>() — Save needs only its GetTypeName and
	// WriteToDataStream, and a registry row per boot would accumulate for
	// nothing.
	Zenith_GrassTypeTableAsset xAsset;
	xAsset.SetTable(xTable);

	// The Vegetation/ directory does not exist in a game that has never grown
	// grass; this write is what creates it.
	std::error_code xEC;
	std::filesystem::create_directories(GrassTypeTablePath().parent_path(), xEC);

	if (!Zenith_AssetRegistry::Save(&xAsset, szZENITH_GRASS_TYPE_TABLE_ASSET_PATH))
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "  grass type table: FAILED to write '%s'",
			szZENITH_GRASS_TYPE_TABLE_ASSET_PATH);
		return;
	}

	// The marker LAST, and only on success: a marker without a table would make
	// the next boot skip a write it never actually made.
	WriteGrassMarker();

	// A cached entry from a previous boot's file would otherwise be what the
	// grass feature loads a moment from now, silently ignoring these bytes.
	Zenith_AssetRegistry::ForceUnload(szZENITH_GRASS_TYPE_TABLE_ASSET_PATH);

	Zenith_Log(LOG_CATEGORY_ASSET, "  grass type table: wrote %u types to '%s' (%s)",
		xTable.GetCount(), szZENITH_GRASS_TYPE_TABLE_ASSET_PATH, szReason);
}

} // namespace

//=============================================================================
// Entry point — called from GenerateTestAssets() at every editor boot.
//=============================================================================
void GenerateGrassAssets()
{
	Zenith_Log(LOG_CATEGORY_ASSET,
		"Generating shared Grass assets v%u (blade vein + gloss + clump ramp, %u-type table)...",
		uGRASS_ASSET_EXPORT_VERSION, uGRASS_TYPE_COUNT);

	const std::string strTextureDir = std::string(ENGINE_ASSETS_DIR) + "Vegetation/";
	std::filesystem::create_directories(strTextureDir);
	GenerateGrassBladeTextures(strTextureDir);

	// The table references the textures by path, so it is written second.
	GenerateGrassTypeTableAsset();

	Zenith_Log(LOG_CATEGORY_ASSET, "Grass assets generated at: %s", strTextureDir.c_str());
}

#include "Zenith_Tools_GrassAssetExport.Tests.inl"

#endif // ZENITH_TOOLS
