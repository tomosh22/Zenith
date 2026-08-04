//------------------------------------------------------------------------------
// Zenith_SaveData unit tests -- the automated-test save sandbox.
// Included at the bottom of Zenith_SaveData.cpp (the module-owns-its-tests
// pattern).
//
// The sandbox is the one piece of this system that DELETES files, so the tests
// lean on the refusal paths rather than the happy path: an unmarked directory,
// a run-id mismatch, a non-existent root, and "no sandbox established" must all
// end with nothing deleted. A bug here writes to (or deletes) a player's real
// save data, which no test suite would ever notice.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"

#if defined(ZENITH_TESTING) && defined(ZENITH_INPUT_SIMULATOR)

namespace
{
	// A scratch directory under the OS temp dir, uniquified by the caller so
	// concurrent runs cannot collide.
	std::filesystem::path SaveDataTest_ScratchDir(const char* szTag)
	{
		std::error_code xEC;
		const std::filesystem::path xRoot = std::filesystem::temp_directory_path(xEC) / "zenith_savedata_tests" / szTag;
		std::filesystem::remove_all(xRoot, xEC);
		std::filesystem::create_directories(xRoot, xEC);
		return xRoot;
	}

	void SaveDataTest_WritePayload(Zenith_DataStream& xStream, void* pxUserData)
	{
		xStream << *static_cast<uint32_t*>(pxUserData);
	}

	void SaveDataTest_ReadPayload(Zenith_DataStream& xStream, uint32_t, void* pxUserData)
	{
		xStream >> *static_cast<uint32_t*>(pxUserData);
	}

	u_int SaveDataTest_CountFilesWithExtension(const std::filesystem::path& xDir, const char* szExtension)
	{
		u_int uCount = 0;
		std::error_code xEC;
		for (std::filesystem::directory_iterator xIt(xDir, xEC), xEnd; !xEC && xIt != xEnd; xIt.increment(xEC))
		{
			if (xIt->is_regular_file(xEC) && !xEC && xIt->path().extension() == szExtension) ++uCount;
		}
		return uCount;
	}
}

ZENITH_TEST(SaveDataSandbox, WipeIsANoOpWithoutAnEstablishedSandbox)
{
	// THE safety property. A boot-unit run is not an automated-test run, so no
	// sandbox exists; WipeTestSandbox must therefore refuse to delete anything
	// rather than fall back to "wipe whatever GetSaveDirectory points at" --
	// which would be the player's real save directory.
	ZENITH_ASSERT_FALSE(Zenith_SaveData::IsUsingTestSandbox(),
		"a plain boot is not an automated-test run, so no sandbox should be active");
	ZENITH_ASSERT_EQ(Zenith_SaveData::WipeTestSandbox(), 0u,
		"with no sandbox established, the wipe must delete nothing at all");
}

ZENITH_TEST(SaveDataSandbox, ScopedRootRedirectsRealDiskIoAndRestores)
{
	const std::filesystem::path xScratch = SaveDataTest_ScratchDir("scoped_root");
	const std::string strPrevDir = Zenith_SaveData::GetSaveDirectory();

	uint32_t uWritten = 0xABCDEF01u;
	{
		Zenith_SaveData::ScopedSaveRootForTest xScope(xScratch.string().c_str());
		ZENITH_ASSERT_TRUE(Zenith_SaveData::Save("scoped_slot", 1u, &SaveDataTest_WritePayload, &uWritten),
			"a save inside the scope must succeed");

		// The file must land in the scoped directory, not the previous root.
		ZENITH_ASSERT_EQ(SaveDataTest_CountFilesWithExtension(xScratch, ZENITH_SAVE_EXT), 1u,
			"the .zsave must be written under the scoped root");

		uint32_t uRead = 0;
		ZENITH_ASSERT_TRUE(Zenith_SaveData::Load("scoped_slot", &SaveDataTest_ReadPayload, &uRead),
			"and must load back through the real disk path");
		ZENITH_ASSERT_EQ(uRead, uWritten, "round-tripped payload");
	}

	ZENITH_ASSERT_STREQ(Zenith_SaveData::GetSaveDirectory(), strPrevDir.c_str(),
		"the previous save root must be restored when the scope ends");

	std::error_code xEC;
	std::filesystem::remove_all(xScratch, xEC);
}

ZENITH_TEST(SaveDataSandbox, ScopedRootNormalisesAMissingTrailingSeparator)
{
	// BuildSlotPath concatenates directory + slot directly. Without the
	// normalisation a caller passing "<dir>" (no separator) would silently
	// write "<dir>slot.zsave" as a SIBLING of the directory rather than inside
	// it -- i.e. into whatever the parent happens to be.
	const std::filesystem::path xScratch = SaveDataTest_ScratchDir("no_trailing_sep");
	std::string strNoSeparator = xScratch.string();
	while (!strNoSeparator.empty()
		&& (strNoSeparator.back() == '/' || strNoSeparator.back() == '\\'))
	{
		strNoSeparator.pop_back();
	}

	uint32_t uWritten = 7u;
	{
		Zenith_SaveData::ScopedSaveRootForTest xScope(strNoSeparator.c_str());
		ZENITH_ASSERT_TRUE(Zenith_SaveData::Save("sep_slot", 1u, &SaveDataTest_WritePayload, &uWritten), "save");
	}

	ZENITH_ASSERT_EQ(SaveDataTest_CountFilesWithExtension(xScratch, ZENITH_SAVE_EXT), 1u,
		"the slot must be written INSIDE the directory, not alongside it");

	std::error_code xEC;
	std::filesystem::remove_all(xScratch, xEC);
}

ZENITH_TEST(SaveDataSandbox, SuppliedRootIsRefusedWithoutAMatchingOwnershipMarker)
{
	// Marker presence is the ownership proof, so these are the checks that stop
	// the engine ever deleting inside a directory it was merely POINTED at.
	const std::filesystem::path xScratch = SaveDataTest_ScratchDir("marker_checks");

	std::filesystem::path xAccepted;
	ZENITH_ASSERT_FALSE(Zenith_SaveData::TryAcceptSuppliedSandboxRoot(xScratch.string().c_str(), "run-1", xAccepted),
		"an unmarked directory must be refused");

	// Wrong run-id in an otherwise valid marker.
	Zenith_SaveData::WriteSandboxMarker(xScratch, "run-OTHER");
	ZENITH_ASSERT_FALSE(Zenith_SaveData::TryAcceptSuppliedSandboxRoot(xScratch.string().c_str(), "run-1", xAccepted),
		"a marker naming a different run must be refused");

	// Matching run-id: accepted, and the accepted path is the CANONICAL form
	// (so a junction cannot redirect later deletes).
	Zenith_SaveData::WriteSandboxMarker(xScratch, "run-1");
	ZENITH_ASSERT_TRUE(Zenith_SaveData::TryAcceptSuppliedSandboxRoot(xScratch.string().c_str(), "run-1", xAccepted),
		"a marker with the matching run-id must be accepted");
	std::error_code xEC;
	ZENITH_ASSERT_TRUE(xAccepted == std::filesystem::canonical(xScratch, xEC),
		"the accepted root must be the canonical path");

	// A path that does not exist at all.
	ZENITH_ASSERT_FALSE(
		Zenith_SaveData::TryAcceptSuppliedSandboxRoot((xScratch / "no_such_dir").string().c_str(), "run-1", xAccepted),
		"a non-existent root must be refused (the engine never creates a supplied root)");

	// A FILE rather than a directory.
	const std::filesystem::path xFile = xScratch / "not_a_dir.txt";
	{
		std::FILE* pxF = Zenith_PlatformStdio::OpenFile(xFile.string().c_str(), "wb");
		if (pxF) std::fclose(pxF);
	}
	ZENITH_ASSERT_FALSE(Zenith_SaveData::TryAcceptSuppliedSandboxRoot(xFile.string().c_str(), "run-1", xAccepted),
		"a regular file must be refused");
	ZENITH_ASSERT_FALSE(Zenith_SaveData::TryAcceptSuppliedSandboxRoot(nullptr, "run-1", xAccepted),
		"a null root must be refused");

	std::filesystem::remove_all(xScratch, xEC);
}

ZENITH_TEST(SaveDataSandbox, WipeDeletesOnlyZsaveFilesNonRecursively)
{
	// The deletion contract, exercised against a real directory: only *.zsave,
	// only at the top level, never a directory, never another extension, and
	// never the ownership marker itself.
	const std::filesystem::path xScratch = SaveDataTest_ScratchDir("wipe_scope");
	Zenith_SaveData::WriteSandboxMarker(xScratch, "wipe-run");

	auto Touch = [](const std::filesystem::path& xPath)
	{
		std::error_code xEC;
		std::filesystem::create_directories(xPath.parent_path(), xEC);
		std::FILE* pxF = Zenith_PlatformStdio::OpenFile(xPath.string().c_str(), "wb");
		if (pxF) { std::fputc('x', pxF); std::fclose(pxF); }
	};

	Touch(xScratch / (std::string("slot_a") + ZENITH_SAVE_EXT));
	Touch(xScratch / (std::string("slot_b") + ZENITH_SAVE_EXT));
	Touch(xScratch / "keep_me.dat");
	Touch(xScratch / "nested" / (std::string("slot_nested") + ZENITH_SAVE_EXT));

	const uint32_t uDeleted = Zenith_SaveData::WipeSandboxDirectory(xScratch);

	ZENITH_ASSERT_EQ(uDeleted, 2u, "exactly the two top-level .zsave files are deleted");
	ZENITH_ASSERT_EQ(SaveDataTest_CountFilesWithExtension(xScratch, ZENITH_SAVE_EXT), 0u, "and they are gone");

	std::error_code xEC;
	ZENITH_ASSERT_TRUE(std::filesystem::exists(xScratch / "keep_me.dat", xEC),
		"a non-.zsave file must survive untouched");
	ZENITH_ASSERT_TRUE(std::filesystem::exists(xScratch / "nested" / (std::string("slot_nested") + ZENITH_SAVE_EXT), xEC),
		"the wipe must NOT recurse into subdirectories");
	ZENITH_ASSERT_TRUE(std::filesystem::is_directory(xScratch / "nested", xEC),
		"and must never remove a directory");
	ZENITH_ASSERT_TRUE(std::filesystem::exists(xScratch / ".zenith_test_save_root", xEC),
		"the ownership marker itself must survive the wipe");

	std::filesystem::remove_all(xScratch, xEC);
}

ZENITH_TEST(SaveDataSandbox, WipeRefusesADirectoryWhoseMarkerVanished)
{
	// The marker is re-checked at DELETE time, not just at accept time: a
	// directory that stopped being ours between the two is left alone.
	const std::filesystem::path xScratch = SaveDataTest_ScratchDir("marker_vanished");
	Zenith_SaveData::WriteSandboxMarker(xScratch, "vanish-run");

	std::error_code xEC;
	const std::filesystem::path xSlot = xScratch / (std::string("slot") + ZENITH_SAVE_EXT);
	std::FILE* pxF = Zenith_PlatformStdio::OpenFile(xSlot.string().c_str(), "wb");
	if (pxF) std::fclose(pxF);

	std::filesystem::remove(xScratch / ".zenith_test_save_root", xEC);

	ZENITH_ASSERT_EQ(Zenith_SaveData::WipeSandboxDirectory(xScratch), 0u,
		"an unmarked directory must be refused at delete time");
	ZENITH_ASSERT_TRUE(std::filesystem::exists(xSlot, xEC), "and its .zsave must survive");

	std::filesystem::remove_all(xScratch, xEC);
}

#endif // ZENITH_TESTING && ZENITH_INPUT_SIMULATOR
