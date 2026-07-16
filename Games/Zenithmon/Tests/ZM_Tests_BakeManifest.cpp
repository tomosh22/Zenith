#include "Zenith.h"

// ============================================================================
// ZM_Tests_BakeManifest -- S4 tests for the per-family bake guard (suite ZM_Gen):
//
//   1. BakeManifest_EnumerationMatchesRoster (ALL-CONFIG): the enumerate sizes
//      match the per-family roster math, every ref sits under the family root
//      ref, and the version accessor returns each family's generator constant.
//   2. BakeManifest_RebakeByteIdentical (TOOLS): bake CareCenter twice -> every
//      baked file is byte-identical, proving Export/SaveToFile are disk-deterministic.
//   3. BakeManifest_GuardWarmStale (TOOLS): drive the guard's warm/stale semantics
//      under a temp root -- no stamp, stamp+files, missing file, empty file, and a
//      version-mismatched stamp -- asserting FAIL-OPEN on each doubt.
//
// Tests 2 + 3 are guarded by ZENITH_TOOLS (ZM_BakeBuilding / ZM_WriteBakeManifest
// only exist in _True configs), so a _False / Android build compiles just test 1.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Gen/ZM_BakeManifest.h"
#include "Zenithmon/Source/Gen/ZM_CreatureGen.h"   // uZM_CREATUREGEN_VERSION, kind-count, ZM_GetSpeciesCount
#include "Zenithmon/Source/Gen/ZM_HumanGen.h"      // uZM_HUMANGEN_VERSION, shared/per-model kind-counts, ZM_HUMAN_COUNT
#include "Zenithmon/Source/Gen/ZM_BuildingGen.h"   // uZM_BUILDINGGEN_VERSION, ZM_BUILDING_COUNT + bake (tools)
#include "Zenithmon/Source/Gen/ZM_PropGen.h"       // uZM_PROPGEN_VERSION, ZM_PROP_COUNT

#include <string>

namespace
{
	// Assert every enumerated ref begins with "<familyRootRef>/" (e.g.
	// "game:Creatures/"). A file-local helper, so the unit count is unaffected.
	void ZM_AssertRefsUnderRoot(ZM_ASSET_FAMILY eFamily,
		const Zenith_Vector<std::string>& xRefs)
	{
		const std::string strPrefix =
			std::string(ZM_BakeManifestFamilyRootRef(eFamily)) + "/";
		ZENITH_ASSERT_GT(xRefs.GetSize(), 0u);
		for (u_int i = 0; i < xRefs.GetSize(); ++i)
		{
			ZENITH_ASSERT_TRUE(xRefs.Get(i).rfind(strPrefix, 0u) == 0u,
				"family %u ref escaped its root ref: %s",
				(u_int)eFamily, xRefs.Get(i).c_str());
		}
	}
}

ZENITH_TEST(ZM_Gen, BakeManifest_EnumerationMatchesRoster)
{
	Zenith_Vector<std::string> xCreatures;
	Zenith_Vector<std::string> xHumans;
	Zenith_Vector<std::string> xBuildings;
	Zenith_Vector<std::string> xProps;
	ZM_EnumerateFamilyFiles(ZM_ASSET_FAMILY_CREATURES, xCreatures);
	ZM_EnumerateFamilyFiles(ZM_ASSET_FAMILY_HUMANS,     xHumans);
	ZM_EnumerateFamilyFiles(ZM_ASSET_FAMILY_BUILDINGS,  xBuildings);
	ZM_EnumerateFamilyFiles(ZM_ASSET_FAMILY_PROPS,      xProps);

	// Enumerate sizes are exactly the per-family roster math.
	ZENITH_ASSERT_EQ(xCreatures.GetSize(),
		ZM_GetSpeciesCount() * static_cast<u_int>(ZM_CREATURE_ASSET_KIND_COUNT));
	ZENITH_ASSERT_EQ(xHumans.GetSize(),
		static_cast<u_int>(ZM_HUMAN_SHARED_ASSET_KIND_COUNT) +
		static_cast<u_int>(ZM_HUMAN_COUNT) * static_cast<u_int>(ZM_HUMAN_ASSET_KIND_COUNT));
	ZENITH_ASSERT_EQ(xBuildings.GetSize(),
		static_cast<u_int>(ZM_BUILDING_COUNT) * static_cast<u_int>(ZM_BUILDING_ASSET_KIND_COUNT));
	ZENITH_ASSERT_EQ(xProps.GetSize(),
		static_cast<u_int>(ZM_PROP_COUNT) * static_cast<u_int>(ZM_PROP_ASSET_KIND_COUNT));

	// Every ref sits under the family root ref.
	ZM_AssertRefsUnderRoot(ZM_ASSET_FAMILY_CREATURES, xCreatures);
	ZM_AssertRefsUnderRoot(ZM_ASSET_FAMILY_HUMANS,     xHumans);
	ZM_AssertRefsUnderRoot(ZM_ASSET_FAMILY_BUILDINGS,  xBuildings);
	ZM_AssertRefsUnderRoot(ZM_ASSET_FAMILY_PROPS,      xProps);

	// The version accessor returns each family's generator constant.
	ZENITH_ASSERT_EQ(ZM_BakeManifestFamilyVersion(ZM_ASSET_FAMILY_CREATURES), uZM_CREATUREGEN_VERSION);
	ZENITH_ASSERT_EQ(ZM_BakeManifestFamilyVersion(ZM_ASSET_FAMILY_HUMANS),    uZM_HUMANGEN_VERSION);
	ZENITH_ASSERT_EQ(ZM_BakeManifestFamilyVersion(ZM_ASSET_FAMILY_BUILDINGS), uZM_BUILDINGGEN_VERSION);
	ZENITH_ASSERT_EQ(ZM_BakeManifestFamilyVersion(ZM_ASSET_FAMILY_PROPS),     uZM_PROPGEN_VERSION);
}

#ifdef ZENITH_TOOLS

#include "AssetHandling/Zenith_AssetRegistry.h"   // ResolvePath: "game:" ref -> absolute FS path
#include "DataStream/Zenith_DataStream.h"          // hermetic byte read of a baked file

#include <filesystem>
#include <fstream>

namespace
{
	// FNV-1a over a raw byte range. Binary-safe (unlike ZM_GenHashName, which stops
	// at the first NUL); only the equality of two hashes over the same code path
	// matters here, so the exact constants need not match the name hash.
	u_int ZM_FnvBytes(const void* pData, size_t uBytes)
	{
		u_int uHash = 2166136261u;
		const u_int8* pByte = static_cast<const u_int8*>(pData);
		for (size_t i = 0; i < uBytes; ++i)
		{
			uHash ^= pByte[i];
			uHash *= 16777619u;
		}
		return uHash;
	}

	void WriteU32LE(u_int8* pBytes, u_int uValue)
	{
		pBytes[0] = static_cast<u_int8>(uValue & 0xffu);
		pBytes[1] = static_cast<u_int8>((uValue >> 8u) & 0xffu);
		pBytes[2] = static_cast<u_int8>((uValue >> 16u) & 0xffu);
		pBytes[3] = static_cast<u_int8>((uValue >> 24u) & 0xffu);
	}

	// Reproduce the impl's ref->FS mapping (strip "game:" + join under the root) so
	// the test drives the guard through the SAME path resolution it uses internally.
	std::filesystem::path RefToRootPath(const std::string& strRef,
		const std::filesystem::path& xRoot)
	{
		const char* szPrefix = "game:";
		const std::string strRel = (strRef.rfind(szPrefix, 0u) == 0u) ?
			strRef.substr(5u) : strRef;
		return (xRoot / strRel).lexically_normal();
	}

	// Walk up from GAME_ASSETS_DIR to the repo root, then hand back a scratch dir
	// under Build/artifacts (mirrors ZM_Tests_TerrainRecipeSet's test-root finder).
	std::filesystem::path BakeManifestTestRoot()
	{
		std::filesystem::path xCursor =
			std::filesystem::absolute(std::filesystem::path(GAME_ASSETS_DIR)).lexically_normal();
		while (!xCursor.empty())
		{
			if (std::filesystem::exists(xCursor / "AGENTS.md") &&
				std::filesystem::is_directory(xCursor / "Build") &&
				std::filesystem::is_directory(xCursor / "Games" / "Zenithmon"))
			{
				return xCursor / "Build" / "artifacts" / "zm_bakemanifest_guard_test";
			}
			const std::filesystem::path xParent = xCursor.parent_path();
			if (xParent == xCursor)
			{
				break;
			}
			xCursor = xParent;
		}
		return {};
	}

	struct BakeManifestTestRootGuard
	{
		explicit BakeManifestTestRootGuard(const std::filesystem::path& xRoot)
			: m_xRoot(xRoot)
		{
			std::error_code xEc;
			std::filesystem::remove_all(m_xRoot, xEc);
		}

		~BakeManifestTestRootGuard()
		{
			std::error_code xEc;
			std::filesystem::remove_all(m_xRoot, xEc);
		}

		std::filesystem::path m_xRoot;
	};

	bool WriteBytes(const std::filesystem::path& xPath, const u_int8* pBytes, size_t uSize)
	{
		std::ofstream xOutput(xPath, std::ios::binary | std::ios::trunc);
		if (uSize > 0u)
		{
			xOutput.write(reinterpret_cast<const char*>(pBytes),
				static_cast<std::streamsize>(uSize));
		}
		return static_cast<bool>(xOutput);
	}

	bool WriteNonEmptyFile(const std::filesystem::path& xPath)
	{
		const u_int8 uValue = 0x5au;
		return WriteBytes(xPath, &uValue, 1u);
	}

	// Create every enumerated file (non-empty), making parent dirs first.
	bool CreateNonEmptyUnder(const std::filesystem::path& xRoot, const std::string& strRef)
	{
		const std::filesystem::path xPath = RefToRootPath(strRef, xRoot);
		std::error_code xEc;
		std::filesystem::create_directories(xPath.parent_path(), xEc);
		return WriteNonEmptyFile(xPath);
	}

	// Write a 12-byte ZMBM stamp (magic + version + count) directly.
	bool WriteStamp(const std::filesystem::path& xPath, u_int uVersion, u_int uCount)
	{
		u_int8 auBytes[12] = { 'Z', 'M', 'B', 'M' };
		WriteU32LE(auBytes + 4u, uVersion);
		WriteU32LE(auBytes + 8u, uCount);
		std::error_code xEc;
		std::filesystem::create_directories(xPath.parent_path(), xEc);
		return WriteBytes(xPath, auBytes, sizeof(auBytes));
	}
}

ZENITH_TEST(ZM_Gen, BakeManifest_RebakeByteIdentical)
{
	const ZM_BUILDING_ID eId = ZM_BUILDING_CARE_CENTER;
	if (!ZM_BakeBuilding(eId))
	{
		ZENITH_SKIP("ZM_BakeBuilding failed -- bake environment unavailable (GAME_ASSETS_DIR not writable?)");
	}

	// Hash all four per-model files (.zmesh / _facade.ztxtr / .zmtrl / .zmodel)
	// after the first bake.
	u_int auHash1[ZM_BUILDING_ASSET_KIND_COUNT] = {};
	for (u_int k = 0; k < static_cast<u_int>(ZM_BUILDING_ASSET_KIND_COUNT); ++k)
	{
		char acRef[512];
		ZENITH_ASSERT_TRUE(ZM_BuildingAssetPath(eId,
			static_cast<ZM_BUILDING_ASSET_KIND>(k), acRef, sizeof(acRef)),
			"building asset kind %u ref must fit", k);
		const std::string strAbs = Zenith_AssetRegistry::ResolvePath(std::string(acRef));
		Zenith_DataStream xStream;
		xStream.ReadFromFile(strAbs.c_str());
		ZENITH_ASSERT_TRUE(xStream.IsValid(),
			"failed to read baked file (kind %u): %s", k, strAbs.c_str());
		auHash1[k] = ZM_FnvBytes(xStream.GetData(),
			static_cast<size_t>(xStream.GetCapacity()));
	}

	// Re-bake the same building, then re-hash: identical bytes prove the disk
	// Export/SaveToFile path is byte-deterministic (the ZM_BakeManifest invariant).
	ZENITH_ASSERT_TRUE(ZM_BakeBuilding(eId), "second CareCenter bake must succeed");
	for (u_int k = 0; k < static_cast<u_int>(ZM_BUILDING_ASSET_KIND_COUNT); ++k)
	{
		char acRef[512];
		ZENITH_ASSERT_TRUE(ZM_BuildingAssetPath(eId,
			static_cast<ZM_BUILDING_ASSET_KIND>(k), acRef, sizeof(acRef)),
			"building asset kind %u ref must fit", k);
		const std::string strAbs = Zenith_AssetRegistry::ResolvePath(std::string(acRef));
		Zenith_DataStream xStream;
		xStream.ReadFromFile(strAbs.c_str());
		ZENITH_ASSERT_TRUE(xStream.IsValid(),
			"failed to re-read baked file (kind %u): %s", k, strAbs.c_str());
		const u_int uHash2 = ZM_FnvBytes(xStream.GetData(),
			static_cast<size_t>(xStream.GetCapacity()));
		ZENITH_ASSERT_EQ(uHash2, auHash1[k],
			"file %u not byte-identical on re-bake", k);
	}
}

ZENITH_TEST(ZM_Gen, BakeManifest_GuardWarmStale)
{
	const std::filesystem::path xRoot = BakeManifestTestRoot();
	if (xRoot.empty())
	{
		ZENITH_SKIP("could not locate the repo Build/artifacts root for the guard temp dir");
	}
	BakeManifestTestRootGuard xGuard(xRoot);   // wipe on ctor + dtor

	const ZM_ASSET_FAMILY eFamily = ZM_ASSET_FAMILY_PROPS;   // smallest roster
	Zenith_Vector<std::string> xFiles;
	ZM_EnumerateFamilyFiles(eFamily, xFiles);
	const u_int uCount = xFiles.GetSize();
	ZENITH_ASSERT_GT(uCount, 0u);

	// Materialise every enumerated file (non-empty) under the temp root.
	for (u_int i = 0; i < uCount; ++i)
	{
		ZENITH_ASSERT_TRUE(CreateNonEmptyUnder(xRoot, xFiles.Get(i)),
			"failed to create temp file: %s", xFiles.Get(i).c_str());
	}

	// No stamp yet -> cold.
	ZENITH_ASSERT_FALSE(ZM_BakeManifestCheck(eFamily, xRoot),
		"a family with no stamp must read cold");

	// Stamp written -> warm.
	ZENITH_ASSERT_TRUE(ZM_WriteBakeManifest(eFamily, xRoot),
		"stamp write must succeed");
	ZENITH_ASSERT_TRUE(ZM_BakeManifestCheck(eFamily, xRoot),
		"current stamp + all files present -> warm");

	// Missing file -> cold; recreate -> warm.
	const std::filesystem::path xVictim = RefToRootPath(xFiles.Get(0u), xRoot);
	std::error_code xEc;
	std::filesystem::remove(xVictim, xEc);
	ZENITH_ASSERT_FALSE(ZM_BakeManifestCheck(eFamily, xRoot),
		"a missing enumerated file must read cold");
	ZENITH_ASSERT_TRUE(WriteNonEmptyFile(xVictim), "recreate the victim file");
	ZENITH_ASSERT_TRUE(ZM_BakeManifestCheck(eFamily, xRoot),
		"recreated file -> warm again");

	// Zero-byte file -> cold; refill -> warm.
	ZENITH_ASSERT_TRUE(WriteBytes(xVictim, nullptr, 0u),
		"truncate the victim file to 0 bytes");
	ZENITH_ASSERT_FALSE(ZM_BakeManifestCheck(eFamily, xRoot),
		"an empty enumerated file must read cold");
	ZENITH_ASSERT_TRUE(WriteNonEmptyFile(xVictim), "refill the victim file");
	ZENITH_ASSERT_TRUE(ZM_BakeManifestCheck(eFamily, xRoot),
		"refilled file -> warm");

	// Version-mismatched stamp -> cold; rewrite correct -> warm.
	const std::filesystem::path xStamp = xRoot / "Props" / ".manifest";
	ZENITH_ASSERT_TRUE(WriteStamp(xStamp,
		ZM_BakeManifestFamilyVersion(eFamily) + 1u, uCount),
		"write a version+1 stamp");
	ZENITH_ASSERT_FALSE(ZM_BakeManifestCheck(eFamily, xRoot),
		"a version-mismatched stamp must read cold");
	ZENITH_ASSERT_TRUE(ZM_WriteBakeManifest(eFamily, xRoot),
		"rewrite the correct stamp");
	ZENITH_ASSERT_TRUE(ZM_BakeManifestCheck(eFamily, xRoot),
		"correct stamp -> warm");
}

#endif // ZENITH_TOOLS
