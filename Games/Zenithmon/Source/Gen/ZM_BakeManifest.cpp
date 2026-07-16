#include "Zenith.h"

// ============================================================================
// ZM_BakeManifest -- the per-family bake guard. See the header for the marker
// shape, the fail-open contract, and the deferred-orchestrator scope note. This
// TU owns: the per-family version/root-ref switches, the fixed-order enumerate,
// the deterministic 12-byte stamp read (all-config) + atomic write (tools), and
// the ZM_BakeAllAssets orchestrator (tools). The marker mirrors the terrain
// ZMTR manifest byte-for-byte in shape (magic + u32 version + u32 count).
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_BakeManifest.h"

// The four gen headers supply the per-family AssetPath fns, kind-count enums,
// roster counts, and version constants the enumerate/version bodies read. They
// are included HERE (not in the lean public header) to avoid an include cycle.
#include "Zenithmon/Source/Gen/ZM_CreatureGen.h"
#include "Zenithmon/Source/Gen/ZM_HumanGen.h"
#include "Zenithmon/Source/Gen/ZM_BuildingGen.h"
#include "Zenithmon/Source/Gen/ZM_PropGen.h"

#include <cstring>     // memcmp / memcpy
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
	// The deterministic stamp: 4-byte magic + u32-LE version + u32-LE count. NO
	// timestamps or paths, so a re-bake writes byte-identical stamp bytes.
	constexpr u_int uZM_BAKE_MANIFEST_MAGIC_SIZE = 4u;
	constexpr char  acZM_BAKE_MANIFEST_MAGIC[uZM_BAKE_MANIFEST_MAGIC_SIZE] = { 'Z', 'M', 'B', 'M' };
	constexpr u_int uZM_BAKE_MANIFEST_SIZE = 12u;   // magic(4) + version(4) + count(4)
	constexpr const char* szZM_BAKE_MANIFEST_NAME = ".manifest";
	constexpr const char* szZM_GAME_REF_PREFIX = "game:";

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

	// exists && is_regular_file && size>0, all with std::error_code (FAIL-CLOSED:
	// any error => not a valid non-empty file).
	bool IsNonEmptyFile(const std::filesystem::path& xPath)
	{
		std::error_code xError;
		return std::filesystem::is_regular_file(xPath, xError) && !xError &&
			std::filesystem::file_size(xPath, xError) > 0u && !xError;
	}

	// Read the 12-byte stamp and confirm magic + version + count all match.
	bool HasValidStamp(const std::filesystem::path& xStamp, u_int uVersion, u_int uCount)
	{
		std::error_code xError;
		if (!std::filesystem::is_regular_file(xStamp, xError) || xError ||
			std::filesystem::file_size(xStamp, xError) != uZM_BAKE_MANIFEST_SIZE || xError)
		{
			return false;
		}

		u_int8 auBytes[uZM_BAKE_MANIFEST_SIZE] = {};
		std::ifstream xInput(xStamp, std::ios::binary);
		xInput.read(reinterpret_cast<char*>(auBytes), sizeof(auBytes));
		if (!xInput || memcmp(auBytes, acZM_BAKE_MANIFEST_MAGIC, uZM_BAKE_MANIFEST_MAGIC_SIZE) != 0)
		{
			return false;
		}
		return ReadU32LE(auBytes + 4u) == uVersion &&
			ReadU32LE(auBytes + 8u) == uCount;
	}

	// Map a "game:<rel>" asset ref to an FS path under xRoot by stripping the
	// "game:" prefix and joining. The SINGLE, uniform resolution the guard + writer
	// both use: the real bake passes xRoot == GAME_ASSETS_DIR (matching where
	// ResolvePath("game:...") writes) and the guard unit test passes a temp dir.
	std::filesystem::path FamilyRefToFsPath(const std::string& strRef,
		const std::filesystem::path& xRoot)
	{
		const size_t uPrefixLen = strlen(szZM_GAME_REF_PREFIX);
		const std::string strRel = (strRef.rfind(szZM_GAME_REF_PREFIX, 0u) == 0u) ?
			strRef.substr(uPrefixLen) : strRef;
		return (xRoot / strRel).lexically_normal();
	}

	// The stamp path for a family under xRoot: <root>/<FamilyDir>/.manifest.
	std::filesystem::path StampPath(ZM_ASSET_FAMILY eFamily,
		const std::filesystem::path& xRoot)
	{
		const std::string strRef = std::string(ZM_BakeManifestFamilyRootRef(eFamily)) +
			"/" + szZM_BAKE_MANIFEST_NAME;
		return FamilyRefToFsPath(strRef, xRoot);
	}
}

u_int ZM_BakeManifestFamilyVersion(ZM_ASSET_FAMILY eFamily)
{
	switch (eFamily)
	{
	case ZM_ASSET_FAMILY_CREATURES: return uZM_CREATUREGEN_VERSION;
	case ZM_ASSET_FAMILY_HUMANS:    return uZM_HUMANGEN_VERSION;
	case ZM_ASSET_FAMILY_BUILDINGS: return uZM_BUILDINGGEN_VERSION;
	case ZM_ASSET_FAMILY_PROPS:     return uZM_PROPGEN_VERSION;
	default:
		Zenith_Assert(false, "ZM_BakeManifestFamilyVersion: unknown family %u", (u_int)eFamily);
		return 0u;
	}
}

const char* ZM_BakeManifestFamilyRootRef(ZM_ASSET_FAMILY eFamily)
{
	switch (eFamily)
	{
	case ZM_ASSET_FAMILY_CREATURES: return "game:Creatures";
	case ZM_ASSET_FAMILY_HUMANS:    return "game:Humans";
	case ZM_ASSET_FAMILY_BUILDINGS: return "game:Buildings";
	case ZM_ASSET_FAMILY_PROPS:     return "game:Props";
	default:
		Zenith_Assert(false, "ZM_BakeManifestFamilyRootRef: unknown family %u", (u_int)eFamily);
		return "game:";
	}
}

void ZM_EnumerateFamilyFiles(ZM_ASSET_FAMILY eFamily, Zenith_Vector<std::string>& xOut)
{
	xOut.Clear();
	char acRef[512];
	switch (eFamily)
	{
	case ZM_ASSET_FAMILY_CREATURES:
	{
		const u_int uCount = ZM_GetSpeciesCount();
		for (u_int u = 0; u < uCount; ++u)
		{
			for (u_int k = 0; k < static_cast<u_int>(ZM_CREATURE_ASSET_KIND_COUNT); ++k)
			{
				const bool bOk = ZM_CreatureAssetPath(static_cast<ZM_SPECIES_ID>(u),
					static_cast<ZM_CREATURE_ASSET_KIND>(k), acRef, sizeof(acRef));
				Zenith_Assert(bOk, "ZM_EnumerateFamilyFiles: creature ref overflow (species %u kind %u)", u, k);
				xOut.PushBack(std::string(acRef));
			}
		}
		break;
	}
	case ZM_ASSET_FAMILY_HUMANS:
	{
		for (u_int k = 0; k < static_cast<u_int>(ZM_HUMAN_SHARED_ASSET_KIND_COUNT); ++k)
		{
			const bool bOk = ZM_HumanSharedAssetPath(
				static_cast<ZM_HUMAN_SHARED_ASSET_KIND>(k), acRef, sizeof(acRef));
			Zenith_Assert(bOk, "ZM_EnumerateFamilyFiles: human shared ref overflow (kind %u)", k);
			xOut.PushBack(std::string(acRef));
		}
		const u_int uCount = static_cast<u_int>(ZM_HUMAN_COUNT);
		for (u_int u = 0; u < uCount; ++u)
		{
			for (u_int k = 0; k < static_cast<u_int>(ZM_HUMAN_ASSET_KIND_COUNT); ++k)
			{
				const bool bOk = ZM_HumanAssetPath(static_cast<ZM_HUMAN_ID>(u),
					static_cast<ZM_HUMAN_ASSET_KIND>(k), acRef, sizeof(acRef));
				Zenith_Assert(bOk, "ZM_EnumerateFamilyFiles: human ref overflow (human %u kind %u)", u, k);
				xOut.PushBack(std::string(acRef));
			}
		}
		break;
	}
	case ZM_ASSET_FAMILY_BUILDINGS:
	{
		const u_int uCount = static_cast<u_int>(ZM_BUILDING_COUNT);
		for (u_int u = 0; u < uCount; ++u)
		{
			for (u_int k = 0; k < static_cast<u_int>(ZM_BUILDING_ASSET_KIND_COUNT); ++k)
			{
				const bool bOk = ZM_BuildingAssetPath(static_cast<ZM_BUILDING_ID>(u),
					static_cast<ZM_BUILDING_ASSET_KIND>(k), acRef, sizeof(acRef));
				Zenith_Assert(bOk, "ZM_EnumerateFamilyFiles: building ref overflow (building %u kind %u)", u, k);
				xOut.PushBack(std::string(acRef));
			}
		}
		break;
	}
	case ZM_ASSET_FAMILY_PROPS:
	{
		const u_int uCount = static_cast<u_int>(ZM_PROP_COUNT);
		for (u_int u = 0; u < uCount; ++u)
		{
			for (u_int k = 0; k < static_cast<u_int>(ZM_PROP_ASSET_KIND_COUNT); ++k)
			{
				const bool bOk = ZM_PropAssetPath(static_cast<ZM_PROP_ID>(u),
					static_cast<ZM_PROP_ASSET_KIND>(k), acRef, sizeof(acRef));
				Zenith_Assert(bOk, "ZM_EnumerateFamilyFiles: prop ref overflow (prop %u kind %u)", u, k);
				xOut.PushBack(std::string(acRef));
			}
		}
		break;
	}
	default:
		Zenith_Assert(false, "ZM_EnumerateFamilyFiles: unknown family %u", (u_int)eFamily);
		break;
	}
}

bool ZM_BakeManifestCheck(ZM_ASSET_FAMILY eFamily, const std::filesystem::path& xGameAssetsRoot)
{
	// FAIL-OPEN: any doubt returns false so the family re-bakes. Stamp + files are
	// resolved under xGameAssetsRoot via FamilyRefToFsPath (strip "game:" + join),
	// the ONE resolution shared with ZM_WriteBakeManifest -- so the real bake
	// (xRoot == GAME_ASSETS_DIR) and the guard test (xRoot == temp dir) agree.
	if (xGameAssetsRoot.empty())
	{
		return false;
	}

	Zenith_Vector<std::string> xFiles;
	ZM_EnumerateFamilyFiles(eFamily, xFiles);
	const u_int uCount = xFiles.GetSize();
	if (uCount == 0u)
	{
		return false;
	}

	const u_int uVersion = ZM_BakeManifestFamilyVersion(eFamily);
	if (!HasValidStamp(StampPath(eFamily, xGameAssetsRoot), uVersion, uCount))
	{
		return false;
	}

	for (u_int i = 0; i < uCount; ++i)
	{
		if (!IsNonEmptyFile(FamilyRefToFsPath(xFiles.Get(i), xGameAssetsRoot)))
		{
			return false;
		}
	}
	return true;
}

#ifdef ZENITH_TOOLS
bool ZM_WriteBakeManifest(ZM_ASSET_FAMILY eFamily, const std::filesystem::path& xGameAssetsRoot)
{
	if (xGameAssetsRoot.empty())
	{
		return false;
	}

	Zenith_Vector<std::string> xFiles;
	ZM_EnumerateFamilyFiles(eFamily, xFiles);
	const u_int uCount = xFiles.GetSize();
	if (uCount == 0u)
	{
		return false;
	}

	u_int8 auBytes[uZM_BAKE_MANIFEST_SIZE] = {};
	memcpy(auBytes, acZM_BAKE_MANIFEST_MAGIC, uZM_BAKE_MANIFEST_MAGIC_SIZE);
	WriteU32LE(auBytes + 4u, ZM_BakeManifestFamilyVersion(eFamily));
	WriteU32LE(auBytes + 8u, uCount);

	const std::filesystem::path xStamp = StampPath(eFamily, xGameAssetsRoot);
	std::error_code xError;
	std::filesystem::create_directories(xStamp.parent_path(), xError);

	// Atomic finalize: write a sibling .tmp, then rename over the stamp. The final
	// exists() check -- not create_directories' return -- is the real IO signal.
	std::filesystem::path xTemp = xStamp;
	xTemp += ".tmp";
	std::filesystem::remove(xTemp, xError);
	{
		std::ofstream xOutput(xTemp, std::ios::binary | std::ios::trunc);
		xOutput.write(reinterpret_cast<const char*>(auBytes), sizeof(auBytes));
		xOutput.flush();
		if (!xOutput)
		{
			return false;
		}
	}
	std::filesystem::remove(xStamp, xError);
	std::filesystem::rename(xTemp, xStamp, xError);
	if (xError)
	{
		std::filesystem::remove(xTemp, xError);
		return false;
	}
	return std::filesystem::exists(xStamp, xError) && !xError;
}

bool ZM_BakeAllAssets()
{
	// Each ZM_BakeAll* self-gates (skips a warm family) and self-stamps on a fully
	// successful bake. AND all four. NO shipped caller yet (S4 gallery gate defers
	// wiring), exactly like the ZM_BakeAll* this wraps.
	return ZM_BakeAllCreatures() && ZM_BakeAllHumans() &&
		ZM_BakeAllBuildings() && ZM_BakeAllProps();
}
#endif   // ZENITH_TOOLS
