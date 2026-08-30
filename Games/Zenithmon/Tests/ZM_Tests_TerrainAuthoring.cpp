#include "Zenith.h"

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/World/ZM_DawnmerePlacement.h"   // the rival's authored XZ -- his shelf must be ON it
#include "Zenithmon/Source/World/ZM_TerrainAuthoring.h"

#if defined(ZENITH_TOOLS) && defined(ZENITH_WINDOWS)
#include "Core/Zenith_Win32.h"
// FSCTL_SET_REPARSE_POINT (the junction fixture below) lives in <winioctl.h>,
// which <windows.h> only pulls in WITHOUT WIN32_LEAN_AND_MEAN. Zenith_Win32.h
// sets LEAN, so include it explicitly rather than relying on some other header
// having reached windows.h first -- which is exactly what used to happen via
// vulkan.hpp, and stopped happening on the Null backend.
#include <winioctl.h>
#endif

#include <filesystem>
#include <fstream>
#include <set>

namespace
{
	constexpr float fEPSILON = 0.00001f;

	void AssertPoint2(const ZM_TerrainPoint2& xPoint, float fX, float fZ)
	{
		ZENITH_ASSERT_EQ_FLOAT(xPoint.m_fX, fX, fEPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xPoint.m_fZ, fZ, fEPSILON);
	}

	void AssertPoint3(const ZM_TerrainPoint3& xPoint, float fX, float fY, float fZ)
	{
		ZENITH_ASSERT_EQ_FLOAT(xPoint.m_fX, fX, fEPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xPoint.m_fY, fY, fEPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xPoint.m_fZ, fZ, fEPSILON);
	}

	void AssertPlanOpEqual(const ZM_TerrainPlanOp& xA, const ZM_TerrainPlanOp& xB,
		u_int uIndex)
	{
		ZENITH_ASSERT_EQ((u_int)xA.m_eType, (u_int)xB.m_eType,
			"plan type differs at %u", uIndex);
		ZENITH_ASSERT_EQ((u_int)xA.m_eDabKind, (u_int)xB.m_eDabKind,
			"plan dab differs at %u", uIndex);
		ZENITH_ASSERT_EQ((u_int)xA.m_ePhase, (u_int)xB.m_ePhase,
			"plan phase differs at %u", uIndex);
		ZENITH_ASSERT_EQ(xA.m_uIndex, xB.m_uIndex, "plan index differs at %u", uIndex);
		ZENITH_ASSERT_EQ_FLOAT(xA.m_fWorldX, xB.m_fWorldX, 0.0f, "plan X differs at %u", uIndex);
		ZENITH_ASSERT_EQ_FLOAT(xA.m_fWorldZ, xB.m_fWorldZ, 0.0f, "plan Z differs at %u", uIndex);
		ZENITH_ASSERT_EQ_FLOAT(xA.m_fRadius, xB.m_fRadius, 0.0f, "plan radius differs at %u", uIndex);
		ZENITH_ASSERT_EQ_FLOAT(xA.m_fStrength, xB.m_fStrength, 0.0f, "plan strength differs at %u", uIndex);
		ZENITH_ASSERT_EQ_FLOAT(xA.m_fValue, xB.m_fValue, 0.0f, "plan value differs at %u", uIndex);
	}

	std::filesystem::path ManifestTestRoot()
	{
		// Scan upward from the configured game-assets location for repository
		// sentinels. This is independent of Debug/Release and Tools True/False;
		// unlike a fixed parent count, it cannot redirect writes if the game
		// directory layout changes.
		std::filesystem::path xCursor =
			std::filesystem::absolute(std::filesystem::path(GAME_ASSETS_DIR)).lexically_normal();
		while (!xCursor.empty())
		{
			if (std::filesystem::exists(xCursor / "CLAUDE.md") &&
				std::filesystem::is_directory(xCursor / "Build") &&
				std::filesystem::is_directory(xCursor / "Games" / "Zenithmon"))
			{
				return xCursor / "Build" / "artifacts" /
					"zm_terrain_authoring_manifest_test";
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

	struct ManifestTestRootGuard
	{
		explicit ManifestTestRootGuard(const std::filesystem::path& xRoot)
			: m_xRoot(xRoot)
		{
			std::error_code xError;
			std::filesystem::remove_all(m_xRoot, xError);
		}

		~ManifestTestRootGuard()
		{
			std::error_code xError;
			std::filesystem::remove_all(m_xRoot, xError);
		}

		std::filesystem::path m_xRoot;
	};

	bool WriteBytes(const std::filesystem::path& xPath, const u_int8* pBytes, size_t uSize)
	{
		std::ofstream xOutput(xPath, std::ios::binary | std::ios::trunc);
		if (uSize > 0u)
		{
			xOutput.write(reinterpret_cast<const char*>(pBytes), static_cast<std::streamsize>(uSize));
		}
		return static_cast<bool>(xOutput);
	}

	bool WriteNonEmptyFile(const std::filesystem::path& xPath)
	{
		const u_int8 uByte = 0x5au;
		return WriteBytes(xPath, &uByte, 1u);
	}

	void WriteU32LE(u_int8* pBytes, u_int uValue)
	{
		pBytes[0] = static_cast<u_int8>(uValue & 0xffu);
		pBytes[1] = static_cast<u_int8>((uValue >> 8u) & 0xffu);
		pBytes[2] = static_cast<u_int8>((uValue >> 16u) & 0xffu);
		pBytes[3] = static_cast<u_int8>((uValue >> 24u) & 0xffu);
	}

	void WriteTestManifest(const std::filesystem::path& xMarker,
		const char* szMagic, u_int uVersion, u_int uCount, size_t uSize = 12u)
	{
		u_int8 auBytes[12] = {};
		memcpy(auBytes, szMagic, 4u);
		WriteU32LE(auBytes + 4u, uVersion);
		WriteU32LE(auBytes + 8u, uCount);
		ZENITH_ASSERT_TRUE(WriteBytes(xMarker, auBytes, uSize));
	}

#if defined(ZENITH_TOOLS) && defined(ZENITH_WINDOWS)
	struct ZM_TestJunctionBuffer
	{
		DWORD m_uReparseTag;
		WORD m_uReparseDataLength;
		WORD m_uReserved;
		WORD m_uSubstituteNameOffset;
		WORD m_uSubstituteNameLength;
		WORD m_uPrintNameOffset;
		WORD m_uPrintNameLength;
		WCHAR m_awcPaths[(MAXIMUM_REPARSE_DATA_BUFFER_SIZE - 16) / sizeof(WCHAR)];
	};
	static_assert(offsetof(ZM_TestJunctionBuffer, m_awcPaths) == 16);

	bool CreateDirectoryJunctionForTest(const std::filesystem::path& xLink,
		const std::filesystem::path& xTarget)
	{
		std::error_code xError;
		const std::filesystem::path xAbsoluteTarget =
			std::filesystem::absolute(xTarget, xError).lexically_normal();
		if (xError || !std::filesystem::is_directory(xAbsoluteTarget, xError) || xError ||
			!std::filesystem::create_directory(xLink, xError) || xError)
		{
			return false;
		}

		const std::wstring strSubstitute = L"\\??\\" + xAbsoluteTarget.wstring();
		const std::wstring strPrint = xAbsoluteTarget.wstring();
		const size_t ulSubstituteBytes = strSubstitute.size() * sizeof(WCHAR);
		const size_t ulPrintBytes = strPrint.size() * sizeof(WCHAR);
		const size_t ulPathBytes = ulSubstituteBytes + sizeof(WCHAR) +
			ulPrintBytes + sizeof(WCHAR);
		const size_t ulInputBytes = offsetof(ZM_TestJunctionBuffer, m_awcPaths) + ulPathBytes;
		if (ulInputBytes > MAXIMUM_REPARSE_DATA_BUFFER_SIZE ||
			ulInputBytes - 8u > static_cast<size_t>(UINT16_MAX))
		{
			std::filesystem::remove(xLink, xError);
			return false;
		}

		ZM_TestJunctionBuffer xBuffer = {};
		xBuffer.m_uReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
		xBuffer.m_uReparseDataLength = static_cast<WORD>(ulInputBytes - 8u);
		xBuffer.m_uSubstituteNameLength = static_cast<WORD>(ulSubstituteBytes);
		xBuffer.m_uPrintNameOffset = static_cast<WORD>(ulSubstituteBytes + sizeof(WCHAR));
		xBuffer.m_uPrintNameLength = static_cast<WORD>(ulPrintBytes);
		memcpy(xBuffer.m_awcPaths, strSubstitute.c_str(), ulSubstituteBytes + sizeof(WCHAR));
		memcpy(reinterpret_cast<u_int8*>(xBuffer.m_awcPaths) + xBuffer.m_uPrintNameOffset,
			strPrint.c_str(), ulPrintBytes + sizeof(WCHAR));

		HANDLE hLink = CreateFileW(xLink.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
			nullptr, OPEN_EXISTING,
			FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
		if (hLink == INVALID_HANDLE_VALUE)
		{
			std::filesystem::remove(xLink, xError);
			return false;
		}
		DWORD uBytesReturned = 0;
		const bool bCreated = DeviceIoControl(hLink, FSCTL_SET_REPARSE_POINT,
			&xBuffer, static_cast<DWORD>(ulInputBytes), nullptr, 0,
			&uBytesReturned, nullptr) != FALSE;
		CloseHandle(hLink);
		if (!bCreated)
		{
			std::filesystem::remove(xLink, xError);
		}
		return bCreated;
	}
#endif
}

ZENITH_TEST(ZM_TerrainAuthoring, DawnmereRecipeIdentityAndBounds)
{
	const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetDawnmereTerrainRecipe();
	const ZM_WorldSpec& xWorld = ZM_GetWorldSpec(ZM_SCENE_DAWNMERE);

	ZENITH_ASSERT_EQ((u_int)xWorld.m_eId, (u_int)ZM_SCENE_DAWNMERE);
	ZENITH_ASSERT_STREQ(xWorld.m_szName, "Dawnmere Village");
	ZENITH_ASSERT_STREQ(xWorld.m_szTerrainSet, "Dawnmere");
	ZENITH_ASSERT_EQ(xWorld.m_uBuildIndex, 2u);
	ZENITH_ASSERT_TRUE(xRecipe.m_pxWorldSpec == &xWorld);
	ZENITH_ASSERT_EQ(ZM_Fnv1a32(xWorld.m_szTerrainSet), 0x7BF32CA4u);
	ZENITH_ASSERT_EQ(xRecipe.m_uSeed, 0x7BF32CA4u);
	// 9 x 10 chunks of 64 m: 576 x 640 m, NOT square any more. The recipe carries
	// the grid and derives its bounds and export rect from it, so all three of
	// these read one authored fact rather than three that could disagree.
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_xDims.m_fChunkWorldSize, 64.0f, fEPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_xDims.VertexSpacing(), 1.0f, fEPSILON);
	ZENITH_ASSERT_EQ(xRecipe.m_xDims.m_uGridChunksX, 4u);
	ZENITH_ASSERT_EQ(xRecipe.m_xDims.m_uGridChunksZ, 4u);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.WorldMinX(), 0.0f, fEPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.WorldMaxX(), 256.0f, fEPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.WorldMinZ(), 0.0f, fEPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.WorldMaxZ(), 256.0f, fEPSILON);
	ZENITH_ASSERT_EQ(xRecipe.ExportRect().m_iMinX, 0);
	ZENITH_ASSERT_EQ(xRecipe.ExportRect().m_iMinY, 0);
	ZENITH_ASSERT_EQ(xRecipe.ExportRect().m_iMaxX, 3);
	ZENITH_ASSERT_EQ(xRecipe.ExportRect().m_iMaxY, 3);
	const u_int uChunkWidth = static_cast<u_int>(
		xRecipe.ExportRect().m_iMaxX - xRecipe.ExportRect().m_iMinX + 1);
	const u_int uChunkHeight = static_cast<u_int>(
		xRecipe.ExportRect().m_iMaxY - xRecipe.ExportRect().m_iMinY + 1);
	ZENITH_ASSERT_EQ(uChunkWidth, 4u);
	ZENITH_ASSERT_EQ(uChunkHeight, 4u);
	ZENITH_ASSERT_EQ(uChunkWidth * uChunkHeight, 16u);
	ZENITH_ASSERT_EQ(uChunkWidth * uChunkHeight * 3u, 48u);
	// chunks x 3 mesh files, plus Height/Splatmap_RGBA/GrassDensity, plus the
	// TerrainDims.zdata manifest the runtime loader requires.
	ZENITH_ASSERT_EQ(uChunkWidth * uChunkHeight * 3u + 4u,
		uZM_DAWNMERE_REQUIRED_OUTPUT_COUNT);

	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_fTargetHeight, 24.0f, fEPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_xProcedural.m_fBaseHeight, 0.046875f, fEPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_xProcedural.m_fAmplitude, 0.018f, fEPSILON);
	// ★ The frequency scales with the SHEET so the map always carries the same
	// NUMBER of base undulations: 0.00125 at 576 m, 0.001875 at 384, and
	// 0.001875 * 384/256 = 0.0028125 at v8's 256. It is the one procedural term
	// measured in world units; base height and amplitude are metres of height
	// and do not move.
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_xProcedural.m_fFrequency, 0.0028125f, fEPSILON);
	ZENITH_ASSERT_EQ(xRecipe.m_xProcedural.m_uOctaves, 5u);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_xProcedural.m_fLacunarity, 2.0f, fEPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_xProcedural.m_fGain, 0.5f, fEPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_xProcedural.m_fRidgedBlend, 0.10f, fEPSILON);

	// NINE landform rows: the same FIVE ridges, re-authored again for v8's 256 m
	// square, plus FOUR SHELVES (checked below) -- the town square, the two
	// building sites, and the rival, who needs one because he is a DYNAMIC body.
	ZENITH_ASSERT_EQ(xRecipe.m_uLandformCount, 9u);
	AssertPoint2(xRecipe.m_pxLandforms[0].m_xCentre, 42.0f, 132.0f);
	AssertPoint2(xRecipe.m_pxLandforms[4].m_xCentre, 216.0f, 214.0f);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxLandforms[0].m_fHeight, 34.0f, fEPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxLandforms[1].m_fHeight, 36.0f, fEPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxLandforms[2].m_fHeight, 32.0f, fEPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxLandforms[3].m_fHeight, 32.0f, fEPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxLandforms[4].m_fHeight, 34.0f, fEPSILON);
	// ★ THE WEST RIDGE IS THE TIGHTEST FIT IN THE TABLE, and the clause that
	// replaces the old "ends exactly at X=1024" one. Its centre EQUALS its radius,
	// so its west foot lands exactly on x=0 -- one metre further out and
	// AssertTerrainAuthoringPlanContained refuses the whole plan.
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxLandforms[0].m_fRadius, 42.0f, fEPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxLandforms[0].m_xCentre.m_fX,
		xRecipe.m_pxLandforms[0].m_fRadius, fEPSILON,
		"the west ridge's foot must land exactly on the world's west edge");

	// ★★ THE THREE SHELVES, AND THE CLAUSE THAT SAYS WHAT MAKES THEM SHELVES
	// RATHER THAN HILLS. They are the LAST three rows so they cut the feet of the
	// five ridges above wherever the two overlap; they target the recipe's own
	// flatten height rather than a crest; and none of them is at strength 0.5 or
	// more. That last one is a contract, not a taste: SetHeight applies
	// min(1, falloff * strength * 2), so a strength >= 0.5 ASSIGNS the target
	// inside r/2 and the town square comes out dead flat -- which is what
	// ZM_DawnmereNpcGroundTruth_Test's spread clause (d) reds on, and which is
	// also the wrong look for graded ground. The exact strengths are a tuning
	// knob and are deliberately NOT pinned; saturation is what is pinned.
	for (u_int u = 5u; u < xRecipe.m_uLandformCount; ++u)
	{
		ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxLandforms[u].m_fHeight,
			xRecipe.m_fTargetHeight, fEPSILON,
			"Dawnmere landform row %u is a SHELF and must target the recipe's own "
			"flatten height, not a crest", u);
		ZENITH_ASSERT_LT(xRecipe.m_pxLandforms[u].m_fStrength, 0.5f,
			"Dawnmere landform row %u is a shelf at strength %.2f, which saturates "
			"SetHeight's min(1, falloff * 2) and assigns the target outright -- the "
			"town square goes dead flat and W5's six measured heights buy nothing",
			u, xRecipe.m_pxLandforms[u].m_fStrength);
	}
	AssertPoint2(xRecipe.m_pxLandforms[5].m_xCentre, 120.0f, 100.0f);
	// ★ THE TOWN SHELF'S RADIUS IS PINNED BECAUSE IT IS SIZED BY THE SIGHT CONE,
	// not by the square: the roster's measured feet heights have to span LESS than
	// fZM_SIGHT_MAX_VERTICAL, and at 80 they spanned 2.02 against a 2.0 band. See
	// the shelf's own note in ZM_TerrainAuthoring.cpp.
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxLandforms[5].m_fRadius, 100.0f, fEPSILON);
	AssertPoint2(xRecipe.m_pxLandforms[6].m_xCentre, 92.0f, 100.0f);
	AssertPoint2(xRecipe.m_pxLandforms[7].m_xCentre, 148.0f, 108.0f);
	// ★ THE RIVAL'S SHELF IS ON HIS AUTHORED XZ, and that coupling is the point:
	// he is a dynamic capsule and slides off anything that is not graded.
	AssertPoint2(xRecipe.m_pxLandforms[8].m_xCentre,
		fZM_DAWNMERE_VESPER_X, fZM_DAWNMERE_VESPER_Z);

	ZENITH_ASSERT_EQ(xRecipe.m_uPathCount, 3u);
	ZENITH_ASSERT_STREQ(xRecipe.m_pxPaths[0].m_szName, "Route");
		// ★ THE ROUTE'S FLATTEN RADIUS IS WHAT SETS THE HOUSE-TO-LAB DISTANCE, because
	// the lane runs BETWEEN the two buildings (ZM-D-218): every metre of graded
	// corridor is a metre they have to stand apart. Widening it without moving
	// them narrows the verge the shells stand on.
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxPaths[0].m_fFlattenRadius, 9.0f, fEPSILON);
	ZENITH_ASSERT_EQ(xRecipe.m_pxPaths[0].m_uFlattenSampleCount, 28u);
	ZENITH_ASSERT_EQ(xRecipe.m_pxPaths[1].m_uFlattenSampleCount, 9u);
	ZENITH_ASSERT_EQ(xRecipe.m_pxPaths[2].m_uFlattenSampleCount, 11u);
	ZENITH_ASSERT_EQ(xRecipe.m_pxPaths[0].m_uDirtSampleCount, 39u);
	ZENITH_ASSERT_EQ(xRecipe.m_pxPaths[1].m_uDirtSampleCount, 14u);
	ZENITH_ASSERT_EQ(xRecipe.m_pxPaths[2].m_uDirtSampleCount, 15u);
	AssertPoint2(xRecipe.m_pxPaths[0].m_pxPoints[0], 120.0f, 208.0f);
	AssertPoint2(xRecipe.m_pxPaths[0].m_pxPoints[3], 120.0f, 80.0f);

	ZENITH_ASSERT_EQ(xRecipe.m_uPadCount, 4u);
	AssertPoint2(xRecipe.m_pxPads[0].m_xCentre, 120.0f, 80.0f);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxPads[0].m_fFlattenRadius, 34.0f, fEPSILON);
	// ZM-D-173: the Home pad moved +40 m in Z with the shell. Its radii, target
	// height and pass count did NOT move -- pinning all four here is what stops a
	// "just nudge the pad" edit from silently re-levelling the forecourt.
	ZENITH_ASSERT_STREQ(xRecipe.m_pxPads[1].m_szName, "Home");
	AssertPoint2(xRecipe.m_pxPads[1].m_xCentre, 92.0f, 104.0f);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxPads[1].m_fFlattenRadius, 24.0f, fEPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxPads[1].m_fDirtRadius, 18.0f, fEPSILON);
	ZENITH_ASSERT_EQ(xRecipe.m_pxPads[1].m_uDirtPassCount, 4u);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxPads[3].m_fFlattenRadius, 22.0f, fEPSILON);
	ZENITH_ASSERT_EQ(xRecipe.m_pxPads[3].m_uDirtPassCount, 0u);

	// ★ The droplet count scales with AREA, not with radius --
	// 60000 * 65536/368640 = 10667 -- so this map is carved at the same DENSITY
	// as the other two rather than 5.6x harder than them.
	ZENITH_ASSERT_EQ(xRecipe.m_xErosion.m_uHydraulicDroplets, 10667u);
	ZENITH_ASSERT_EQ(xRecipe.m_xErosion.m_uThermalIterations, 1u);
	ZENITH_ASSERT_TRUE(xRecipe.m_xErosion.m_bRegionOnly);
	AssertPoint2(xRecipe.m_xErosion.m_xCentre, 120.0f, 100.0f);
	// 725 * 256/1024 = 181.25: the radius scales with the axis that shrank, so the
	// same PROPORTION of the map is eroded and "region-only" keeps its meaning.
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_xErosion.m_fRadius, 181.0f, fEPSILON);

	ZENITH_ASSERT_EQ(xRecipe.m_uAutoSplatCount, 4u);
	ZENITH_ASSERT_STREQ(xRecipe.m_pxAutoSplat[0].m_szName, "Meadow");
	ZENITH_ASSERT_STREQ(xRecipe.m_pxAutoSplat[3].m_szName, "Heath");
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxAutoSplat[0].m_fWeight, 1.25f, fEPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxAutoSplat[3].m_fJitter, 0.25f, fEPSILON);

	// Still eight: the six PERIPHERAL dabs were re-placed onto the re-authored
	// ridges, and the two central ones are the town lawn and simply moved with the
	// plaza.
	ZENITH_ASSERT_EQ(xRecipe.m_uGrassDabCount, 8u);
	AssertPoint2(xRecipe.m_pxGrassDabs[0].m_xCentre, 44.0f, 132.0f);
	AssertPoint2(xRecipe.m_pxGrassDabs[5].m_xCentre, 168.0f, 40.0f);
	// Central town-lawn dabs so grass surrounds the TownCenter spawn (paths/pads
	// erase their paved footprints in the later grass-erase phase).
	AssertPoint2(xRecipe.m_pxGrassDabs[6].m_xCentre, 120.0f, 44.0f);
	AssertPoint2(xRecipe.m_pxGrassDabs[7].m_xCentre, 120.0f, 152.0f);
	// Row 2, not row 1: the two flank ridges both carry 0.55 now, so pinning one
	// of them would not distinguish the table from a uniform one. The knolls are
	// the 0.70 rows.
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxGrassDabs[2].m_fTargetDensity, 0.70f, fEPSILON);
	// ★ 0.75, and it is sized by a TEST not by taste: three automated suites probe
	// the four cardinals from the spawn for a tile at or above
	// ZM_TallGrassSystem::fGRASS_DENSITY_THRESHOLD (0.5), and the dab's falloff has
	// to clear that AFTER the Plaza pad's grass-erase has taken its bite. See the
	// dab's own note in ZM_TerrainAuthoring.cpp.
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxGrassDabs[6].m_fTargetDensity, 0.75f, fEPSILON);

	ZENITH_ASSERT_EQ(xRecipe.m_uLandmarkCount, 10u);
	ZENITH_ASSERT_STREQ(xRecipe.m_pxLandmarks[0].m_szName, "TownCenter");
	AssertPoint3(xRecipe.m_pxLandmarks[0].m_xPosition, 120.0f, 24.0f, 60.0f);
	// ZM-D-173: both Home landmarks moved with the relocated shell and its return
	// spawn. Y stays 24 -- recipe metadata at the nominal target height, never a
	// measured surface, which is why it does NOT track the placement table.
	ZENITH_ASSERT_STREQ(xRecipe.m_pxLandmarks[2].m_szName, "Home");
	AssertPoint3(xRecipe.m_pxLandmarks[2].m_xPosition, 92.0f, 24.0f, 104.0f);
	ZENITH_ASSERT_STREQ(xRecipe.m_pxLandmarks[3].m_szName, "FromHome");
	AssertPoint3(xRecipe.m_pxLandmarks[3].m_xPosition, 92.0f, 24.0f, 92.0f);
	ZENITH_ASSERT_STREQ(xRecipe.m_pxLandmarks[6].m_szName, "FromRoute1");
	AssertPoint3(xRecipe.m_pxLandmarks[6].m_xPosition, 120.0f, 24.0f, 144.0f);
	AssertPoint3(xRecipe.m_pxLandmarks[9].m_xPosition, 140.0f, 24.0f, 116.0f);

	ZENITH_ASSERT_EQ(xRecipe.m_uMaterialCount, 4u);
	ZENITH_ASSERT_STREQ(xRecipe.m_pxMaterials[0].m_szName, "Meadow");
	// Meadow, Stone and Dirt are textured (the shared engine grass, rock and clay
	// sets), so their base colour is WHITE -- the terrain shader multiplies base
	// colour into the sampled diffuse, and the old 0.26/0.46/0.16 green,
	// 0.34/0.36/0.33 grey and 0.38/0.25/0.14 brown would tint them. The set refs
	// themselves are pinned by
	// ZM_TerrainRecipeSet::DawnmereGroundSlotsSampleTheSharedEngineSets. Heath is
	// the one slot still carrying an authored hue, so it is the only row whose
	// base colour this unit can still pin to a NON-white value.
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxMaterials[0].m_afBaseColour[0], 1.0f, fEPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxMaterials[1].m_afBaseColour[0], 1.0f, fEPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxMaterials[2].m_afBaseColour[2], 1.0f, fEPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxMaterials[3].m_afBaseColour[1], 0.55f, fEPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxMaterials[0].m_fRoughness, 0.92f, fEPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxMaterials[3].m_fRoughness, 0.90f, fEPSILON);
	for (u_int i = 0; i < xRecipe.m_uMaterialCount; ++i)
	{
		ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_pxMaterials[i].m_fMetallic, 0.0f, fEPSILON);
	}

	AssertPoint3(xRecipe.m_xPreviewCamera.m_xPosition, 120.0f, 40.0f, 24.0f);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_xPreviewCamera.m_fPitch, -0.22f, fEPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_xPreviewCamera.m_fFovDegrees, 65.0f, fEPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_xPreviewCamera.m_fNearPlane, 0.1f, fEPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_xPreviewCamera.m_fFarPlane, 2000.0f, fEPSILON);
	ZENITH_ASSERT_STREQ(szZM_FORCE_TERRAIN_BAKE_FLAG, "--zm-force-terrain-bake");
}

ZENITH_TEST(ZM_TerrainAuthoring, DawnmereRecipePlanIsDeterministicAndContained)
{
	const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetDawnmereTerrainRecipe();
	Zenith_Vector<ZM_TerrainPlanOp> xPlanA;
	Zenith_Vector<ZM_TerrainPlanOp> xPlanB;
	ZM_BuildTerrainAuthoringPlan(xRecipe, xPlanA);
	ZM_BuildTerrainAuthoringPlan(xRecipe, xPlanB);

	// 1024 before the shrink. Two ops moved it, both deliberate and both frozen
	// here so a third cannot arrive unnoticed: the new leading SET_DIMENSIONS,
	// and a fifth Dawnmere landform (the re-authored ridges frame a 576 x 640 m
	// map from five directions where four filled a 1024 m square). Paths and pads
	// were TRANSLATED, not re-shaped, so their sampled op counts are untouched.
	ZENITH_ASSERT_EQ(xPlanA.GetSize(), 556u);
	ZENITH_ASSERT_EQ(xPlanA.GetSize(), xPlanB.GetSize());
	for (u_int i = 0; i < xPlanA.GetSize(); ++i)
	{
		AssertPlanOpEqual(xPlanA.Get(i), xPlanB.Get(i), i);
	}
	ZENITH_ASSERT_EQ((u_int)xPlanA.Get(0).m_eType,
		(u_int)ZM_TERRAIN_PLAN_SET_DIMENSIONS,
		"the session's SHAPE must be staged before any world-space coordinate; "
		"the bake sizes every output from it");
	ZENITH_ASSERT_EQ((u_int)xPlanA.Get(1).m_eType,
		(u_int)ZM_TERRAIN_PLAN_RESET,
		"standalone session must open/reset before staging its named set");
	ZENITH_ASSERT_EQ((u_int)xPlanA.Get(2).m_eType,
		(u_int)ZM_TERRAIN_PLAN_SET_ASSET_SET,
		"named set must be staged after reset and before all generated content");
	ZENITH_ASSERT_EQ((u_int)xPlanA.Get(3).m_eType,
		(u_int)ZM_TERRAIN_PLAN_GENERATE_PROCEDURAL);
	ZENITH_ASSERT_EQ((u_int)xPlanA.GetBack().m_eType,
		(u_int)ZM_TERRAIN_PLAN_TERMINAL_BAKE);

	u_int auPhaseCounts[ZM_TERRAIN_PHASE_GRASS_ERASE + 1u] = {};
	u_int uErosionIndex = UINT_MAX;
	u_int uLastDensityIndex = 0u;
	u_int uResetCount = 0u;
	u_int uSetCount = 0u;
	u_int uSetIndex = UINT_MAX;
	bool bErasePhaseSeen = false;
	for (u_int i = 0; i < xPlanA.GetSize(); ++i)
	{
		const ZM_TerrainPlanOp& xOp = xPlanA.Get(i);
		if (xOp.m_eType == ZM_TERRAIN_PLAN_RESET)
		{
			++uResetCount;
			ZENITH_ASSERT_EQ(uSetCount, 0u,
				"reset after named-set staging would lose the persistence target");
		}
		if (xOp.m_eType == ZM_TERRAIN_PLAN_SET_ASSET_SET)
		{
			++uSetCount;
			uSetIndex = i;
		}
		if (xOp.m_eType == ZM_TERRAIN_PLAN_EROSION)
		{
			uErosionIndex = i;
		}
		if (xOp.m_eType != ZM_TERRAIN_PLAN_BRUSH_DAB)
		{
			continue;
		}
		ZENITH_ASSERT_GE(xOp.m_fWorldX - xOp.m_fRadius, xRecipe.WorldMinX(),
			"dab %u extends below world X", i);
		ZENITH_ASSERT_LE(xOp.m_fWorldX + xOp.m_fRadius, xRecipe.WorldMaxX(),
			"dab %u extends above world X", i);
		ZENITH_ASSERT_GE(xOp.m_fWorldZ - xOp.m_fRadius, xRecipe.WorldMinZ(),
			"dab %u extends below world Z", i);
		ZENITH_ASSERT_LE(xOp.m_fWorldZ + xOp.m_fRadius, xRecipe.WorldMaxZ(),
			"dab %u extends above world Z", i);
		ZENITH_ASSERT_TRUE(xOp.m_ePhase <= ZM_TERRAIN_PHASE_GRASS_ERASE);
		++auPhaseCounts[xOp.m_ePhase];

		if (xOp.m_eDabKind == ZM_TERRAIN_DAB_GRASS_DENSITY)
		{
			uLastDensityIndex = i;
			if (xOp.m_ePhase == ZM_TERRAIN_PHASE_GRASS_ERASE)
			{
				bErasePhaseSeen = true;
				ZENITH_ASSERT_EQ_FLOAT(xOp.m_fValue, 0.0f, 0.0f);
			}
			else
			{
				ZENITH_ASSERT_FALSE(bErasePhaseSeen,
					"grass fill appeared after erase phase at plan op %u", i);
			}
		}
	}

	ZENITH_ASSERT_NE(uErosionIndex, UINT_MAX);
	ZENITH_ASSERT_EQ(uResetCount, 1u);
	ZENITH_ASSERT_EQ(uSetCount, 1u);
	// Index 2, not 1: SET_DIMENSIONS now opens the plan, so RESET is 1 and the
	// named set is staged at 2. What this clause is actually for is unchanged --
	// the set must be staged AFTER the reset (which would otherwise discard it)
	// and BEFORE anything generated.
	ZENITH_ASSERT_EQ(uSetIndex, 2u);
	// v8 (ZM-D-218): 9 landform dabs (five ridges + four shelves), 48 flatten
	// samples along the three paths + 4 pad dabs = 52 per flatten pass, five
	// dirt passes over 68 path samples + 12 pad dabs = 352, and the grass erase
	// re-walks the paths at dirt spacing (68) plus one dab per pad (4) = 72.
	ZENITH_ASSERT_EQ(auPhaseCounts[ZM_TERRAIN_PHASE_LANDFORM], 9u);
	ZENITH_ASSERT_EQ(auPhaseCounts[ZM_TERRAIN_PHASE_FLATTEN_PRE_EROSION], 52u);
	ZENITH_ASSERT_EQ(auPhaseCounts[ZM_TERRAIN_PHASE_FLATTEN_POST_EROSION], 52u);
	ZENITH_ASSERT_EQ(auPhaseCounts[ZM_TERRAIN_PHASE_DIRT], 352u);
	ZENITH_ASSERT_EQ(auPhaseCounts[ZM_TERRAIN_PHASE_GRASS_FILL], 8u);
	ZENITH_ASSERT_EQ(auPhaseCounts[ZM_TERRAIN_PHASE_GRASS_ERASE], 72u);
	ZENITH_ASSERT_TRUE(uErosionIndex > 2u + 4u);
	ZENITH_ASSERT_EQ((u_int)xPlanA.Get(uLastDensityIndex + 1u).m_eType,
		(u_int)ZM_TERRAIN_PLAN_TERMINAL_BAKE,
		"grass erase must remain the final density phase");

	for (u_int i = 0; i < xRecipe.m_uLandmarkCount; ++i)
	{
		const ZM_TerrainPoint3& xPoint = xRecipe.m_pxLandmarks[i].m_xPosition;
		ZENITH_ASSERT_GE(xPoint.m_fX, xRecipe.WorldMinX());
		ZENITH_ASSERT_LE(xPoint.m_fX, xRecipe.WorldMaxX());
		ZENITH_ASSERT_GE(xPoint.m_fZ, xRecipe.WorldMinZ());
		ZENITH_ASSERT_LE(xPoint.m_fZ, xRecipe.WorldMaxZ());
	}
}

ZENITH_TEST(ZM_TerrainAuthoring, DawnmereManifestRequiresEveryOutput)
{
	const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetDawnmereTerrainRecipe();
	Zenith_Vector<std::string> xOutputs;
	ZM_EnumerateRequiredTerrainOutputs(xRecipe, xOutputs);
	ZENITH_ASSERT_EQ(xOutputs.GetSize(), uZM_DAWNMERE_REQUIRED_OUTPUT_COUNT);

	std::set<std::string> xUnique;
	u_int uRender = 0u;
	u_int uRenderLow = 0u;
	u_int uPhysics = 0u;
	u_int uTextures = 0u;
	for (u_int i = 0; i < xOutputs.GetSize(); ++i)
	{
		const std::string& strOutput = xOutputs.Get(i);
		xUnique.insert(strOutput);
		ZENITH_ASSERT_TRUE(strOutput.rfind("Terrain/Dawnmere/", 0u) == 0u,
			"output escaped Dawnmere set: %s", strOutput.c_str());
		const std::string strName = std::filesystem::path(strOutput).filename().string();
		if (strName.rfind("Render_LOW_", 0u) == 0u) ++uRenderLow;
		else if (strName.rfind("Render_", 0u) == 0u) ++uRender;
		else if (strName.rfind("Physics_", 0u) == 0u) ++uPhysics;
		else ++uTextures;
	}
	ZENITH_ASSERT_EQ(xUnique.size(), static_cast<size_t>(uZM_DAWNMERE_REQUIRED_OUTPUT_COUNT));
	// 4 x 4 chunks: 16 of each family, where 6 x 6 gave 36 and 16 x 16 gave 256.
	ZENITH_ASSERT_EQ(uRender, 16u);
	ZENITH_ASSERT_EQ(uRenderLow, 16u);
	ZENITH_ASSERT_EQ(uPhysics, 16u);
	// Three .ztxtr maps plus TerrainDims.zdata -- the manifest lands in this
	// bucket because it is not a Render_/Render_LOW_/Physics_ chunk.
	ZENITH_ASSERT_EQ(uTextures, 4u);
	ZENITH_ASSERT_STREQ(ZM_GetTerrainGrassAssetPath(xRecipe).c_str(),
		"game:Terrain/Dawnmere/GrassDensity.ztxtr");
	ZENITH_ASSERT_STREQ(ZM_GetTerrainManifestRelativePath(xRecipe).c_str(),
		"Terrain/Dawnmere/ZM_TerrainRecipe.manifest");

	ZENITH_ASSERT_EQ((u_int)ZM_DetermineTerrainBakeQueueResult(true, true, true, false),
		(u_int)ZM_TERRAIN_BAKE_HEADLESS,
		"headless must win over force and warm state");
	ZENITH_ASSERT_EQ((u_int)ZM_DetermineTerrainBakeQueueResult(false, false, true, false),
		(u_int)ZM_TERRAIN_BAKE_WARM,
		"a warm non-force boot is the only scene-authoring phase");
	ZENITH_ASSERT_EQ((u_int)ZM_DetermineTerrainBakeQueueResult(false, false, false, true),
		(u_int)ZM_TERRAIN_BAKE_QUEUED);
	ZENITH_ASSERT_EQ((u_int)ZM_DetermineTerrainBakeQueueResult(false, true, true, true),
		(u_int)ZM_TERRAIN_BAKE_QUEUED,
		"force must turn a warm bake back into phase-one queueing");
	ZENITH_ASSERT_EQ((u_int)ZM_DetermineTerrainBakeQueueResult(false, false, false, false),
		(u_int)ZM_TERRAIN_BAKE_PREPARE_FAILED);
	ZENITH_ASSERT_EQ((u_int)ZM_DetermineTerrainBakeQueueResult(false, true, true, false),
		(u_int)ZM_TERRAIN_BAKE_PREPARE_FAILED);

#ifdef ZENITH_TOOLS
	const std::filesystem::path xTestRoot = ManifestTestRoot();
	ZENITH_ASSERT_FALSE(xTestRoot.empty(),
		"could not resolve the repository Build/artifacts root");
	if (xTestRoot.empty())
	{
		return;
	}
	ManifestTestRootGuard xGuard(xTestRoot);
	std::error_code xError;
	const std::filesystem::path xSetDirectory =
		xGuard.m_xRoot / "Terrain" / xRecipe.m_pxWorldSpec->m_szTerrainSet;
	std::filesystem::create_directories(xSetDirectory, xError);
	ZENITH_ASSERT_FALSE(static_cast<bool>(xError));
	ZENITH_ASSERT_FALSE(ZM_IsTerrainBakeWarm(xRecipe, xGuard.m_xRoot));
	for (u_int i = 0; i < xOutputs.GetSize(); ++i)
	{
		ZENITH_ASSERT_TRUE(WriteNonEmptyFile(xGuard.m_xRoot / xOutputs.Get(i)),
			"could not seed manifest test output %u", i);
	}
	ZENITH_ASSERT_TRUE(ZM_FinalizeTerrainBake(xRecipe, xGuard.m_xRoot));
	ZENITH_ASSERT_TRUE(ZM_IsTerrainBakeWarm(xRecipe, xGuard.m_xRoot));

	const std::filesystem::path xMarker =
		xGuard.m_xRoot / ZM_GetTerrainManifestRelativePath(xRecipe);
	xError.clear();
	const uintmax_t ulMarkerSize = std::filesystem::file_size(xMarker, xError);
	ZENITH_ASSERT_FALSE(static_cast<bool>(xError),
		"finalized marker size could not be read: %s", xError.message().c_str());
	if (!xError)
	{
		ZENITH_ASSERT_EQ(ulMarkerSize,
			static_cast<uintmax_t>(uZM_TERRAIN_MANIFEST_SIZE));
	}

	// Every individual required output is independently part of the warm gate.
	//
	// SAMPLED, NOT EXHAUSTIVE, and deliberately so. ZM_IsTerrainBakeWarm walks the
	// enumerated output list and rejects on the first non-empty-file miss, so if it
	// honours output 0 and output N-1 it honours every index in between BY
	// CONSTRUCTION -- there is no per-index branch for a sample to miss. What is
	// genuinely at risk is a whole CATEGORY dropping out of enumeration, and the
	// uRender/uRenderLow/uPhysics/uTextures counts asserted above already cover that
	// exhaustively and for free. So this loop samples one representative of each
	// category plus both ends.
	//
	// It used to remove-and-restore EVERY output, taking a full warm check each
	// time. That is O(n^2) in FILESYSTEM syscalls -- ~594,000 file-existence checks
	// for zero extra defect-detection power -- and was 27 s, ~20% of the entire
	// engine unit suite on its own. The sibling
	// ZM_TerrainRecipeSet::ManifestsEncodePerRecipeCounts... covers this same
	// invariant for EVERY recipe, sampled, in a tenth of the time; this is the
	// shape to copy.
	{
		const u_int uCount = xOutputs.GetSize();
		const u_int auSampleIndices[] =
		{
			0u,                    // first (a Render_ chunk)
			1u,
			uCount / 4u,           // inside the Render_LOW_ run
			uCount / 2u,           // inside the Physics_ run
			(uCount * 3u) / 4u,
			uCount - 4u,           // the three trailing textures + the dims manifest
			uCount - 2u,
			uCount - 1u,           // last (TerrainDims.zdata)
		};
		for (u_int uSample = 0; uSample < sizeof(auSampleIndices) / sizeof(auSampleIndices[0]); ++uSample)
		{
			const u_int i = auSampleIndices[uSample];
			if (i >= uCount) continue;

			const std::filesystem::path xOutput = xGuard.m_xRoot / xOutputs.Get(i);
			std::filesystem::remove(xOutput, xError);
			ZENITH_ASSERT_FALSE(static_cast<bool>(xError));
			ZENITH_ASSERT_FALSE(ZM_IsTerrainBakeWarm(xRecipe, xGuard.m_xRoot),
				"warm bake ignored missing required output %u", i);
			ZENITH_ASSERT_TRUE(WriteNonEmptyFile(xOutput));
		}
	}

	// Existing-but-empty is not complete either.
	const std::filesystem::path xFirstOutput = xGuard.m_xRoot / xOutputs.Get(0);
	ZENITH_ASSERT_TRUE(WriteBytes(xFirstOutput, nullptr, 0u));
	ZENITH_ASSERT_FALSE(ZM_IsTerrainBakeWarm(xRecipe, xGuard.m_xRoot));
	ZENITH_ASSERT_TRUE(WriteNonEmptyFile(xFirstOutput));

	// ★ THE VERSION TRACKS THE CONSTANT, IT IS NEVER SPELLED AS A LITERAL. These
	// clauses each inject exactly ONE defect, so every other field must be the
	// CURRENT valid value -- a hard-coded `1u` silently becomes a second defect the
	// moment uZM_TERRAIN_MANIFEST_VERSION is bumped, which turns the positive
	// control below into a false failure and the wrong-version negative into a
	// false pass. (Observed for real: the ZM-D-182 bump to v2 reddened both.)
	// The wrong-version case is therefore stated RELATIVE to the constant.
	// ★ THE COUNT TRACKS ITS CONSTANT TOO, for exactly the reason the version
	// does: these clauses each inject ONE defect, so a literal 771 became a
	// SECOND defect the moment the required-output count moved (it did, when the
	// dimensions manifest joined the set) -- turning the positive control into a
	// false failure and the wrong-count negative into a false pass.
	constexpr u_int uCURRENT = uZM_TERRAIN_MANIFEST_VERSION;
	constexpr u_int uEXPECTED_OUTPUTS = uZM_DAWNMERE_REQUIRED_OUTPUT_COUNT;
	WriteTestManifest(xMarker, "ZMTR", uCURRENT, uEXPECTED_OUTPUTS, 8u);   // defect: short stamp
	ZENITH_ASSERT_FALSE(ZM_IsTerrainBakeWarm(xRecipe, xGuard.m_xRoot));
	WriteTestManifest(xMarker, "BAD!", uCURRENT, uEXPECTED_OUTPUTS);       // defect: magic
	ZENITH_ASSERT_FALSE(ZM_IsTerrainBakeWarm(xRecipe, xGuard.m_xRoot));
	WriteTestManifest(xMarker, "ZMTR", uCURRENT + 1u, uEXPECTED_OUTPUTS);  // defect: version
	ZENITH_ASSERT_FALSE(ZM_IsTerrainBakeWarm(xRecipe, xGuard.m_xRoot));
	WriteTestManifest(xMarker, "ZMTR", uCURRENT, uEXPECTED_OUTPUTS - 1u);  // defect: count
	ZENITH_ASSERT_FALSE(ZM_IsTerrainBakeWarm(xRecipe, xGuard.m_xRoot));
	WriteTestManifest(xMarker, "ZMTR", uCURRENT, uEXPECTED_OUTPUTS);       // no defect: warm again
	ZENITH_ASSERT_TRUE(ZM_IsTerrainBakeWarm(xRecipe, xGuard.m_xRoot));

	// Phase one invalidates every completion signal and all old textures before
	// queueing. Mesh cleanup remains the E2 rect export's terminal responsibility.
	const std::filesystem::path xTemp = xMarker.string() + ".tmp";
	const std::filesystem::path xHeight = xSetDirectory / "Height.ztxtr";
	const std::filesystem::path xSplat = xSetDirectory / "Splatmap_RGBA.ztxtr";
	const std::filesystem::path xGrass = xSetDirectory / "GrassDensity.ztxtr";
	ZENITH_ASSERT_TRUE(WriteNonEmptyFile(xTemp));
	ZENITH_ASSERT_TRUE(ZM_PrepareTerrainBake(xRecipe, xGuard.m_xRoot));
	ZENITH_ASSERT_FALSE(std::filesystem::exists(xMarker));
	ZENITH_ASSERT_FALSE(std::filesystem::exists(xTemp));
	ZENITH_ASSERT_FALSE(std::filesystem::exists(xHeight));
	ZENITH_ASSERT_FALSE(std::filesystem::exists(xSplat));
	ZENITH_ASSERT_FALSE(std::filesystem::exists(xGrass));
	ZENITH_ASSERT_TRUE(std::filesystem::exists(xGuard.m_xRoot / xOutputs.Get(0)),
		"preparation must leave old meshes for the checked E2 terminal export");
	ZENITH_ASSERT_FALSE(ZM_IsTerrainBakeWarm(xRecipe, xGuard.m_xRoot));
	ZENITH_ASSERT_FALSE(ZM_FinalizeTerrainBake(xRecipe, xGuard.m_xRoot),
		"terminal finalization must reject missing newly-written textures");
	ZENITH_ASSERT_FALSE(std::filesystem::exists(xMarker),
		"terminal failure must leave the completion marker absent");
	ZENITH_ASSERT_TRUE(WriteNonEmptyFile(xHeight));
	ZENITH_ASSERT_TRUE(WriteNonEmptyFile(xSplat));
	ZENITH_ASSERT_TRUE(WriteNonEmptyFile(xGrass));
	ZENITH_ASSERT_TRUE(ZM_FinalizeTerrainBake(xRecipe, xGuard.m_xRoot));
	ZENITH_ASSERT_TRUE(ZM_IsTerrainBakeWarm(xRecipe, xGuard.m_xRoot));

	// A non-empty directory at a texture path is a deterministic cross-config
	// removal failure. Preparation reports failure before any queue mutation,
	// and the marker-first ordering still prevents stale content looking warm.
	std::filesystem::remove(xHeight, xError);
	ZENITH_ASSERT_FALSE(static_cast<bool>(xError));
	std::filesystem::create_directories(xHeight, xError);
	ZENITH_ASSERT_FALSE(static_cast<bool>(xError));
	ZENITH_ASSERT_TRUE(WriteNonEmptyFile(xHeight / "blocks-removal"));
	ZENITH_ASSERT_FALSE(ZM_PrepareTerrainBake(xRecipe, xGuard.m_xRoot));
	ZENITH_ASSERT_FALSE(std::filesystem::exists(xMarker));
	ZENITH_ASSERT_TRUE(std::filesystem::is_directory(xHeight));
	std::filesystem::remove_all(xHeight, xError);
	ZENITH_ASSERT_FALSE(static_cast<bool>(xError));
	ZENITH_ASSERT_TRUE(WriteNonEmptyFile(xHeight));
	ZENITH_ASSERT_TRUE(ZM_FinalizeTerrainBake(xRecipe, xGuard.m_xRoot));
	ZENITH_ASSERT_TRUE(ZM_IsTerrainBakeWarm(xRecipe, xGuard.m_xRoot));

#ifdef ZENITH_WINDOWS
	// A pre-existing validly-named junction must be rejected before the lease
	// callback can delete a marker, textures, or an unrelated sentinel in the
	// redirected target.
	const std::filesystem::path xJunctionAssets = xGuard.m_xRoot / "junction_assets";
	const std::filesystem::path xJunctionTerrain = xJunctionAssets / "Terrain";
	const std::filesystem::path xJunctionTarget =
		xJunctionTerrain / xRecipe.m_pxWorldSpec->m_szTerrainSet;
	const std::filesystem::path xOutside = xGuard.m_xRoot / "junction_outside";
	std::filesystem::create_directories(xJunctionTerrain, xError);
	ZENITH_ASSERT_FALSE(static_cast<bool>(xError));
	std::filesystem::create_directories(xOutside, xError);
	ZENITH_ASSERT_FALSE(static_cast<bool>(xError));
	const std::filesystem::path xOutsideMarker = xOutside / "ZM_TerrainRecipe.manifest";
	const std::filesystem::path xOutsideHeight = xOutside / "Height.ztxtr";
	const std::filesystem::path xOutsideSplat = xOutside / "Splatmap_RGBA.ztxtr";
	const std::filesystem::path xOutsideGrass = xOutside / "GrassDensity.ztxtr";
	const std::filesystem::path xOutsideSentinel = xOutside / "must-not-change.sentinel";
	// Current version, not a literal: this stamp stands in for a REAL warm tree
	// outside the junction, and the clause is that nothing here is touched.
	WriteTestManifest(xOutsideMarker, "ZMTR", uZM_TERRAIN_MANIFEST_VERSION,
		uZM_DAWNMERE_REQUIRED_OUTPUT_COUNT);
	ZENITH_ASSERT_TRUE(WriteNonEmptyFile(xOutsideHeight));
	ZENITH_ASSERT_TRUE(WriteNonEmptyFile(xOutsideSplat));
	ZENITH_ASSERT_TRUE(WriteNonEmptyFile(xOutsideGrass));
	ZENITH_ASSERT_TRUE(WriteNonEmptyFile(xOutsideSentinel));
	ZENITH_ASSERT_TRUE(CreateDirectoryJunctionForTest(xJunctionTarget, xOutside),
		"junction setup must not require Developer Mode/symlink privilege");
	ZENITH_ASSERT_FALSE(ZM_PrepareTerrainBake(xRecipe, xJunctionAssets),
		"pre-existing named-target junction must be rejected before preparation");
	ZENITH_ASSERT_EQ(std::filesystem::file_size(xOutsideMarker), static_cast<uintmax_t>(12u));
	ZENITH_ASSERT_EQ(std::filesystem::file_size(xOutsideHeight), static_cast<uintmax_t>(1u));
	ZENITH_ASSERT_EQ(std::filesystem::file_size(xOutsideSplat), static_cast<uintmax_t>(1u));
	ZENITH_ASSERT_EQ(std::filesystem::file_size(xOutsideGrass), static_cast<uintmax_t>(1u));
	ZENITH_ASSERT_EQ(std::filesystem::file_size(xOutsideSentinel), static_cast<uintmax_t>(1u));
#endif
#endif
}
