#include "Zenith.h"

#include "Core/Zenith_TestFramework.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Zenithmon/Source/World/ZM_GrassDensityMap.h"

#include <array>

ZENITH_TEST(ZM_Grass, GrassDensityMapValidatesAndSamples)
{
	ZENITH_ASSERT_EQ(ZM_GrassDensityMap::uEXPECTED_WIDTH, 1024u);
	ZENITH_ASSERT_EQ(ZM_GrassDensityMap::uEXPECTED_HEIGHT, 1024u);
	ZENITH_ASSERT_EQ(ZM_GrassDensityMap::ulEXPECTED_BYTE_COUNT, size_t(4194304));
	ZENITH_ASSERT_EQ_FLOAT(ZM_GrassDensityMap::fWORLD_SIZE, 4096.0f, 0.0f);

	const std::string strTerrainDirectory =
		std::string(GAME_ASSETS_DIR) + "Terrain/Dawnmere/";
	ZENITH_ASSERT_STREQ(
		ZM_GrassDensityMap::BuildCanonicalPath(strTerrainDirectory).c_str(),
		(strTerrainDirectory + "GrassDensity" + ZENITH_TEXTURE_EXT).c_str());

	ZM_GrassDensityMap xMap;
	ZENITH_ASSERT_EQ_FLOAT(xMap.SampleWorld(0.0f, 0.0f), 0.0f, 0.0f);

	std::array<float, 4> afFixture = { 0.0f, 1.0f, 2.0f, 3.0f };
	constexpr size_t ulFixtureBytes = 4 * sizeof(float);

	ZENITH_ASSERT_FALSE(xMap.LoadDecoded(TEXTURE_FORMAT_RGBA8_UNORM, 2, 2,
		afFixture.data(), ulFixtureBytes, 2, 2, 2.0f));
	ZENITH_ASSERT_FALSE(xMap.LoadDecoded(TEXTURE_FORMAT_R32_SFLOAT, 3, 2,
		afFixture.data(), ulFixtureBytes, 2, 2, 2.0f));
	ZENITH_ASSERT_FALSE(xMap.LoadDecoded(TEXTURE_FORMAT_R32_SFLOAT, 2, 3,
		afFixture.data(), ulFixtureBytes, 2, 2, 2.0f));
	ZENITH_ASSERT_FALSE(xMap.LoadDecoded(TEXTURE_FORMAT_R32_SFLOAT, 2, 2,
		afFixture.data(), ulFixtureBytes - 1, 2, 2, 2.0f));
	ZENITH_ASSERT_FALSE(xMap.LoadDecoded(TEXTURE_FORMAT_R32_SFLOAT, 2, 2,
		nullptr, ulFixtureBytes, 2, 2, 2.0f));
	ZENITH_ASSERT_FALSE(xMap.IsLoaded());
	ZENITH_ASSERT_EQ_FLOAT(xMap.SampleWorld(1.0f, 1.0f), 0.0f, 0.0f);

	ZENITH_ASSERT_TRUE(xMap.LoadDecoded(TEXTURE_FORMAT_R32_SFLOAT, 2, 2,
		afFixture.data(), ulFixtureBytes, 2, 2, 2.0f));
	ZENITH_ASSERT_EQ(xMap.GetPixelCount(), size_t(4));
	afFixture[0] = 99.0f;
	ZENITH_ASSERT_EQ_FLOAT(xMap.SampleWorld(-10.0f, -10.0f), 0.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xMap.SampleWorld(0.5f, 0.5f), 1.5f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xMap.SampleWorld(10.0f, 10.0f), 3.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xMap.SampleWorld(0.5f, 0.0f), 0.5f, 0.0001f);
}
