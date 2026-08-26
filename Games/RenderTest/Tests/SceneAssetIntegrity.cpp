#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"

#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"

#include <string>
#include <vector>
#include <fstream>
#include <cstring>

// RT_SceneAssetIntegrity -- the boot this test runs inside must not have damaged
// Assets/Scenes/RenderTest.zscen.
//
// The scene is re-authored and saved every tools boot. The authoring used to be
// backend-INCOMPLETE: Zenith_TerrainEditor::EnsureTreeEntities refused to run on
// the Null backend (on the reasoning that instance groups allocate GPU buffers), so
// a headless boot authored the campus WITHOUT its two instanced tree entities, and
// that subset was serialized straight over the tracked asset -- every headless run
// silently rewrote the committed scene (361753 bytes as it stands today) down to
// ~38 KB, deleting ~323 KB of tree instance data, and the only symptom was a dirty
// `git status` nobody was looking at. A publish guard in
// Zenith_Editor::SaveActiveScene then refused any headless save that would CHANGE
// an existing asset.
//
// ZEN-6 fixed the cause instead of the symptom: entity and component creation is
// backend-neutral now (only the GPU allocation underneath it is skipped), so a Null
// tools boot authors the same scene a windowed one does -- and the publish refusal
// is gone with it, which is what lets a headless run re-author a committed scene at
// all. THE ASSERTIONS BELOW ARE UNCHANGED, and this is where they earn their keep:
// they used to be near-vacuous headless, because a headless boot never wrote the
// file. Now a headless boot DOES write it, so "both tree entities are in the asset"
// is a real end-to-end check of the completeness the removed guard used to stand in
// for. A regression that re-introduces a Null-backend bail in an authoring step
// reddens this test in the headless gate rather than passing quietly.
//
// So this asserts on the FILE, not on the loaded scene: whichever backend booted,
// the asset on disk still carries both tree entities, and it carries no per-run
// harness entity. Reading names out of the raw bytes deliberately avoids depending
// on which entities THIS run happens to have loaded (the strings are the entity
// names the serializer writes; a scene missing an entity cannot contain its name).
namespace
{
	const char* const szRENDERTEST_SCENE_PATH = GAME_ASSETS_DIR "Scenes/RenderTest" ZENITH_SCENE_EXT;

	std::vector<char> s_xSceneBytes;
	std::string s_strFailure;

	bool SceneBytesContain(const char* szNeedle)
	{
		const size_t ulNeedle = strlen(szNeedle);
		if (s_xSceneBytes.size() < ulNeedle)
		{
			return false;
		}
		const char* pcData = s_xSceneBytes.data();
		const size_t ulLast = s_xSceneBytes.size() - ulNeedle;
		for (size_t ul = 0; ul <= ulLast; ++ul)
		{
			if (memcmp(pcData + ul, szNeedle, ulNeedle) == 0)
			{
				return true;
			}
		}
		return false;
	}

	void RequireInScene(const char* szName, const char* szWhy)
	{
		if (!SceneBytesContain(szName))
		{
			s_strFailure += std::string("missing '") + szName + "' (" + szWhy + "); ";
		}
	}

	void Setup_SceneAssetIntegrity()
	{
		s_xSceneBytes.clear();
		s_strFailure.clear();

		std::ifstream xFile(szRENDERTEST_SCENE_PATH, std::ios::binary);
		if (!xFile.is_open())
		{
			s_strFailure = "the scene asset is not on disk at all; ";
			return;
		}
		s_xSceneBytes.assign((std::istreambuf_iterator<char>(xFile)), std::istreambuf_iterator<char>());
	}

	bool Step_SceneAssetIntegrity(int /*iFrame*/)
	{
		return false;   // pure file inspection -- nothing to simulate
	}

	bool Verify_SceneAssetIntegrity()
	{
		if (!s_strFailure.empty())
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN, "[RT_SceneAssetIntegrity] %s: %s",
				szRENDERTEST_SCENE_PATH, s_strFailure.c_str());
			return false;
		}

		// The two instanced-tree entities the terrain editor's TreePaint brush
		// authors. These are exactly what a headless boot could not author, and used
		// to delete on save; a headless boot authors them now, so on THIS backend
		// their absence means the authoring step stopped being backend-neutral.
		RequireInScene("TerrainTrees_Trunk", "the boot rewrote the scene without its instanced trees");
		RequireInScene("TerrainTrees_Leaves", "the boot rewrote the scene without its instanced trees");

		// A spot-check that the rest of the campus survived too, so a save that
		// dropped everything BUT the trees still reddens this.
		RequireInScene("RenderTestTerrain", "the campus terrain entity is gone from the asset");
		RequireInScene("Tennis_Match", "the tennis testbed is gone from the asset");

		// Per-run harness entities are spawned transient, post-load; none of them
		// belongs in a tracked asset.
		if (SceneBytesContain("RenderTestSmokeRunner"))
		{
			s_strFailure += "'RenderTestSmokeRunner' was serialized into the tracked scene "
				"(the smoke runner must be spawned transient, post-load); ";
		}

		if (!s_strFailure.empty())
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN, "[RT_SceneAssetIntegrity] %s (%zu bytes): %s",
				szRENDERTEST_SCENE_PATH, s_xSceneBytes.size(), s_strFailure.c_str());
			return false;
		}

		Zenith_Log(LOG_CATEGORY_TERRAIN, "[RT_SceneAssetIntegrity] scene asset intact (%zu bytes)",
			s_xSceneBytes.size());
		return true;
	}
}

static const Zenith_AutomatedTest g_xSceneAssetIntegrity = {
	"RT_SceneAssetIntegrity",
	&Setup_SceneAssetIntegrity,
	&Step_SceneAssetIntegrity,
	&Verify_SceneAssetIntegrity,
	1   // m_iMaxFrames -- the boot already happened; this only reads the file it left behind
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xSceneAssetIntegrity);

#endif // ZENITH_INPUT_SIMULATOR
