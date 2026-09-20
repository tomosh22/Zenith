//------------------------------------------------------------------------------
// Zenith_EditorPanelContentBrowser unit tests.
// Included at the bottom of Zenith_EditorPanel_ContentBrowser.cpp (inside its
// ZENITH_TOOLS block — the panel does not exist without it).
//
// ★★ THESE ARE ABOUT OWNERSHIP, NOT ABOUT PIXELS, and that is what makes the
// defect they pin catchable headless at all. A thumbnail tile draws through an
// ImGui descriptor set that RegisterTexture built from the texture's raw image
// view ONCE, at registration time. Nothing revalidates it. So the tile is correct
// exactly while the cache keeps the texture alive, and WRONG — sampling whatever
// image later took the recycled view slot — the moment it does not. The cache
// used to fill itself with SetPath() (no load, no AddRef) plus GetView() (a
// documented non-owning view), leaving every thumbnail at refcount 0 for
// UnloadUnused() to delete on the next SCENE_LOAD_SINGLE.
//
// On a real backend that is a use-after-free whose only symptom is the wrong
// picture, which no assertion can reach. The refcount is the mechanism and IS
// assertable, so these drive it directly and then drive the consequence
// (UnloadUnused) with a control beside it.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_TextureAsset.h"

#ifdef ZENITH_TESTING

namespace
{
	// Registers a texture asset at strPath and then drops the only reference, so
	// the registry holds it at refcount 0 — exactly the state a texture browsed
	// in the content panel but used by no loaded scene is in. Procedural: no file
	// is touched, no GPU resource is allocated, and m_xSRV stays invalid, so
	// GetTextureThumbnail takes the pin and skips RegisterTexture. That is what
	// keeps these units device-free rather than requiresGraphics.
	void SeedUnreferencedTexture(const char* szPath)
	{
		TextureHandle xSeed = Zenith_AssetRegistry::Create<Zenith_TextureAsset>(szPath);
		ZENITH_ASSERT_NOT_NULL(xSeed.GetDirect(), "seed texture '%s' was not created", szPath);
	}

	u_int RefCountOf(const char* szPath)
	{
		// Non-owning on purpose: reading the count must not change it.
		const Zenith_TextureAsset* pxTexture = Zenith_AssetRegistry::GetView<Zenith_TextureAsset>(szPath);
		return pxTexture ? static_cast<u_int>(pxTexture->GetRefCount()) : 0u;
	}
}

ZENITH_TEST(ContentBrowserPanel, ThumbnailCachePinsItsTextureAcrossUnloadUnused)
{
	using namespace Zenith_EditorPanelContentBrowser;

	const char* const szTHUMBED  = "procedural://unit_contentbrowser_thumbed.ztxtr";
	const char* const szCONTROL  = "procedural://unit_contentbrowser_control.ztxtr";

	ClearThumbnailCache();
	SeedUnreferencedTexture(szTHUMBED);
	SeedUnreferencedTexture(szCONTROL);

	ZENITH_ASSERT_EQ(RefCountOf(szTHUMBED), 0u, "seed must leave the texture unreferenced");
	ZENITH_ASSERT_EQ(RefCountOf(szCONTROL), 0u, "seed must leave the texture unreferenced");

	// THE MECHANISM: filling a cache entry takes an owning reference.
	GetTextureThumbnail(szTHUMBED);
	ZENITH_ASSERT_EQ(RefCountOf(szTHUMBED), 1u,
		"the thumbnail cache must OWN its texture -- SetPath()+GetView() takes no reference");
	ZENITH_ASSERT_EQ(RefCountOf(szCONTROL), 0u, "the control must stay unreferenced");

	// THE CONSEQUENCE: what SCENE_LOAD_SINGLE does between teardown and the new
	// scene's load (Zenith_SceneSystem::UnloadUnusedAssets).
	Zenith_AssetRegistry::UnloadUnused();

	ZENITH_ASSERT_TRUE(Zenith_AssetRegistry::IsLoaded(szTHUMBED),
		"a thumbnailed texture must survive UnloadUnused, or its image view is freed "
		"under the ImGui descriptor set the tile still draws with");
	// The control is the sensitivity check: without it this test would pass just as
	// well against an UnloadUnused that collected nothing at all.
	ZENITH_ASSERT_FALSE(Zenith_AssetRegistry::IsLoaded(szCONTROL),
		"UnloadUnused must actually collect an unreferenced texture");

	ClearThumbnailCache();
	Zenith_AssetRegistry::ForceUnload(szTHUMBED);
}

ZENITH_TEST(ContentBrowserPanel, ClearThumbnailCacheReleasesThePin)
{
	using namespace Zenith_EditorPanelContentBrowser;

	const char* const szPATH = "procedural://unit_contentbrowser_released.ztxtr";

	ClearThumbnailCache();
	SeedUnreferencedTexture(szPATH);

	GetTextureThumbnail(szPATH);
	ZENITH_ASSERT_EQ(RefCountOf(szPATH), 1u, "the cache entry holds the pin");

	// The pin is the cache's, not the panel's: navigating away must give it back,
	// or a browse through a texture folder would keep every texture it ever
	// rendered resident for the rest of the session.
	ClearThumbnailCache();
	ZENITH_ASSERT_EQ(RefCountOf(szPATH), 0u, "ClearThumbnailCache must release every pin");

	Zenith_AssetRegistry::UnloadUnused();
	ZENITH_ASSERT_FALSE(Zenith_AssetRegistry::IsLoaded(szPATH),
		"a released thumbnail must be collectable again");
}

#endif // ZENITH_TESTING
