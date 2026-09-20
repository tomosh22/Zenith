//------------------------------------------------------------------------------
// Zenith_EditorMaterialUI texture-preview-cache unit tests.
// Included at the bottom of Zenith_Editor_MaterialUI.cpp (inside its
// ZENITH_TOOLS block).
//
// ★★ THE CACHE'S CONTRACT IS OWNERSHIP, AND THAT IS WHAT IS ASSERTED.
// GetOrCreateTexturePreviewHandle hands back an ImGui descriptor set built from
// the texture's RAW image view, once, with no revalidation. It used to hold no
// reference to the texture and to key on m_xVRAMHandle.AsUInt() -- a recycled
// free-list index -- so a collected texture left a dangling descriptor AND a
// later texture could land on the dead entry's key and be handed its handle.
//
// The collision itself is not reproducible headless (the Null backend's dummy
// handles increase monotonically and never recycle), so these assert the
// property that makes the collision impossible instead: the entry OWNS its
// texture, which both keeps the view alive and makes the key -- a live asset's
// address -- unreusable.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_TextureAsset.h"

#ifdef ZENITH_TESTING

namespace
{
	// A 1x1 texture with REAL GPU handles on whichever backend is running (the
	// Null memory manager hands back valid dummy ids, which is what lets this run
	// headless), then released so the registry holds it at refcount 0 -- the state
	// a texture the editor merely DREW is in.
	Zenith_TextureAsset* MatUITestMakeUnreferencedTexture(u_int32 uPixel)
	{
		TextureHandle xSeed = Zenith_AssetRegistry::Create<Zenith_TextureAsset>();
		Zenith_TextureAsset* pxTex = xSeed.GetDirect();
		if (pxTex == nullptr)
		{
			return nullptr;
		}
		Flux_SurfaceInfo xInfo;
		xInfo.m_uWidth       = 1;
		xInfo.m_uHeight      = 1;
		xInfo.m_uNumMips     = 1;
		xInfo.m_eFormat      = TEXTURE_FORMAT_RGBA8_UNORM;
		xInfo.m_eTextureType = TEXTURE_TYPE_2D;
		pxTex->CreateFromData(&uPixel, xInfo, /*bCreateMips*/ false);
		return pxTex;   // xSeed dies here: refcount back to 0
	}
}

ZENITH_TEST(EditorMaterialUI, PreviewCacheOwnsItsTextureAcrossUnloadUnused)
{
	Zenith_EditorMaterialUI& xUI = g_xEngine.EditorMaterialUI();
	xUI.ClearTexturePreviewCache();

	Zenith_TextureAsset* pxTex = MatUITestMakeUnreferencedTexture(0xFFFFFFFFu);
	ZENITH_ASSERT_NOT_NULL(pxTex, "test texture was not created");
	if (pxTex == nullptr) { return; }

	// The control: same construction, never shown in a preview slot.
	Zenith_TextureAsset* pxControl = MatUITestMakeUnreferencedTexture(0xFF000000u);
	ZENITH_ASSERT_NOT_NULL(pxControl, "control texture was not created");
	if (pxControl == nullptr) { return; }

	const std::string strKept    = pxTex->GetPath();
	const std::string strControl = pxControl->GetPath();

	ZENITH_ASSERT_EQ(static_cast<u_int>(pxTex->GetRefCount()), 0u, "the seed handle must have been released");

	const Flux_ImGuiTextureHandle xHandle = xUI.GetOrCreateTexturePreviewHandle(pxTex);
	ZENITH_ASSERT_EQ(static_cast<u_int>(pxTex->GetRefCount()), 1u,
		"the preview cache must OWN the texture it registered a descriptor set for");

	// The consequence: SCENE_LOAD_SINGLE runs exactly this between teardown and
	// the next scene's load.
	Zenith_AssetRegistry::UnloadUnused();

	ZENITH_ASSERT_TRUE(Zenith_AssetRegistry::IsLoaded(strKept),
		"a previewed texture must survive UnloadUnused, or its image view is freed "
		"under the ImGui descriptor set the thumbnail still draws with");
	// Without this the test would pass just as well against an UnloadUnused that
	// collected nothing at all.
	ZENITH_ASSERT_FALSE(Zenith_AssetRegistry::IsLoaded(strControl),
		"UnloadUnused must actually collect a texture nothing previewed");

	// The entry is a CACHE, so asking again is a hit rather than a second
	// registration -- and the pin does not stack.
	const Flux_ImGuiTextureHandle xAgain = xUI.GetOrCreateTexturePreviewHandle(pxTex);
	ZENITH_ASSERT_EQ(xAgain.AsUInt64(), xHandle.AsUInt64(), "a repeat lookup must hit the cache");
	ZENITH_ASSERT_EQ(static_cast<u_int>(pxTex->GetRefCount()), 1u, "a cache hit must not take a second reference");

	xUI.ClearTexturePreviewCache();
	Zenith_AssetRegistry::UnloadUnused();
	ZENITH_ASSERT_FALSE(Zenith_AssetRegistry::IsLoaded(strKept),
		"clearing the cache must release the pin so the texture is collectable again");
}

ZENITH_TEST(EditorMaterialUI, PreviewCacheKeepsTwoTexturesApart)
{
	Zenith_EditorMaterialUI& xUI = g_xEngine.EditorMaterialUI();
	xUI.ClearTexturePreviewCache();

	Zenith_TextureAsset* pxA = MatUITestMakeUnreferencedTexture(0xFF0000FFu);
	Zenith_TextureAsset* pxB = MatUITestMakeUnreferencedTexture(0xFF00FF00u);
	ZENITH_ASSERT_NOT_NULL(pxA, "texture A was not created");
	ZENITH_ASSERT_NOT_NULL(pxB, "texture B was not created");
	if (pxA == nullptr || pxB == nullptr) { return; }

	const Flux_ImGuiTextureHandle xA = xUI.GetOrCreateTexturePreviewHandle(pxA);
	const Flux_ImGuiTextureHandle xB = xUI.GetOrCreateTexturePreviewHandle(pxB);

	ZENITH_ASSERT_EQ(xUI.GetTexturePreviewCacheSize(), 2u, "two textures must occupy two entries");
	// The defect this replaced keyed on a recycled VRAM index, which is how two
	// distinct textures came to share one descriptor set.
	ZENITH_ASSERT_NE(xA.AsUInt64(), xB.AsUInt64(), "two textures must not share one descriptor set");
	ZENITH_ASSERT_EQ(static_cast<u_int>(pxA->GetRefCount()), 1u, "A is pinned by its entry");
	ZENITH_ASSERT_EQ(static_cast<u_int>(pxB->GetRefCount()), 1u, "B is pinned by its entry");

	xUI.ClearTexturePreviewCache();
	ZENITH_ASSERT_EQ(xUI.GetTexturePreviewCacheSize(), 0u, "clear empties the cache");
	ZENITH_ASSERT_EQ(static_cast<u_int>(pxA->GetRefCount()), 0u, "clear releases A's pin");
	ZENITH_ASSERT_EQ(static_cast<u_int>(pxB->GetRefCount()), 0u, "clear releases B's pin");
	Zenith_AssetRegistry::UnloadUnused();
}

ZENITH_TEST(EditorMaterialUI, PreviewEvictionAgeSurvivesFrameCounterWrap)
{
	// Fresh and recently drawn: kept.
	ZENITH_ASSERT_FALSE(Zenith_EditorMaterialUI::ShouldEvictPreviewEntry(0u, 0u), "same frame is not stale");
	ZENITH_ASSERT_FALSE(Zenith_EditorMaterialUI::ShouldEvictPreviewEntry(100u, 1u), "99 frames is not stale");

	// Long undrawn: dropped.
	ZENITH_ASSERT_TRUE(Zenith_EditorMaterialUI::ShouldEvictPreviewEntry(100000u, 1u), "a long-undrawn entry is stale");

	// ★ THE WRAP. An entry touched just before the counter rolled over is two
	// frames old, not four billion. A widened or signed subtraction here evicts
	// every live entry on the frame the counter wraps -- on a real backend that
	// frees a descriptor set under a panel mid-draw, once, hours in.
	const u_int uMax = 0xFFFFFFFFu;
	ZENITH_ASSERT_FALSE(Zenith_EditorMaterialUI::ShouldEvictPreviewEntry(1u, uMax - 1u),
		"an entry touched 3 frames before a counter wrap is NOT stale");
	ZENITH_ASSERT_TRUE(Zenith_EditorMaterialUI::ShouldEvictPreviewEntry(500u, uMax - 1u),
		"...but a genuinely old one still is, measured across the same wrap");
}

#endif // ZENITH_TESTING
