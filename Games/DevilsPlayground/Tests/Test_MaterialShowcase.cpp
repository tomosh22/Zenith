#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#if defined(ZENITH_TOOLS) && defined(ZENITH_INPUT_SIMULATOR)

#include "Core/Zenith_AutomatedTest.h"
#include "Input/Zenith_InputSimulator.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Panels/Zenith_EditorPanel_MaterialEditor.h"
#include "Flux/RenderViews/Flux_MaterialPreviewController.h"
#include "Flux/Flux_Screenshot.h"
#include "Flux/Flux_Enums.h"
#include "Flux/Flux_Types.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_MaterialParamTable.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Maths/Zenith_Maths.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

// ============================================================================
// Test_MaterialShowcase -- screenshot-proof sweep for the ADVANCED material
// parameters that the live-preview editor test (Test_MaterialEditorLivePreview)
// didn't individually exercise: specular F0, clear coat, unlit shading, normal
// mapping, alpha-cutout (Masked), forward translucency, and instance overrides.
//
// It drives the proven offscreen IBL preview (Flux_MaterialPreview) -- which
// renders any material on a sphere/cube/plane with the SAME MaterialSurface.slang
// surface evaluation the deferred G-buffer path uses -- through a sequence of
// distinct material states, dumping a deterministic swapchain TGA at each so the
// result can be observed. requiresGraphics keeps it windowed-only (headless =
// passed-skip). It also asserts a few resolved values so it is a real regression
// test, not just a capture harness.
// ============================================================================

namespace
{
	struct ShowcaseState
	{
		bool m_bReady = false;
		bool m_bUnlitApplied = false;
		bool m_bInstanceResolved = false;
		bool m_bMaskedApplied = false;
		bool m_bFailed = false;
		const char* m_szWhy = "";
	};
	ShowcaseState g_xShow;

	Zenith_MaterialAsset* g_pxMat = nullptr;       // the showcased (selected) material
	Zenith_MaterialAsset* g_pxParent = nullptr;    // instance-case parent
	TextureHandle g_xNormalTex;                    // procedural tangent-space normal map
	TextureHandle g_xCheckerAlphaTex;              // procedural checker w/ 0/255 alpha (cutout)

	void Fail(const char* szWhy) { g_xShow.m_bFailed = true; g_xShow.m_szWhy = szWhy; Zenith_Error(LOG_CATEGORY_CORE, "[MaterialShowcase] FAIL: %s", szWhy); }

	// Build a small RGBA8 procedural texture from a per-texel generator.
	Zenith_TextureAsset* MakeProceduralRGBA8(u_int uSize, const std::function<void(u_int, u_int, u_int8*)>& fnGen)
	{
		std::vector<u_int8> xPixels(static_cast<size_t>(uSize) * uSize * 4);
		for (u_int y = 0; y < uSize; y++)
			for (u_int x = 0; x < uSize; x++)
				fnGen(x, y, &xPixels[(static_cast<size_t>(y) * uSize + x) * 4]);

		Zenith_TextureAsset* pxTex = Zenith_AssetRegistry::Create<Zenith_TextureAsset>();
		Flux_SurfaceInfo xInfo;
		xInfo.m_eFormat = TEXTURE_FORMAT_RGBA8_UNORM;
		xInfo.m_eTextureType = TEXTURE_TYPE_2D;
		xInfo.m_uWidth = uSize;
		xInfo.m_uHeight = uSize;
		xInfo.m_uDepth = 1;
		xInfo.m_uNumMips = 1;
		xInfo.m_uNumLayers = 1;
		xInfo.m_uMemoryFlags = 1u << MEMORY_FLAGS__SHADER_READ;
		pxTex->CreateFromData(xPixels.data(), xInfo, /*bCreateMips*/ false);
		return pxTex;
	}

	// Reset the showcased material to a clean grey-dielectric default + clear all
	// textures, so each case starts from a known state.
	void ResetMaterial()
	{
		if (!g_pxMat) return;
		g_pxMat->ClearParent();
		Zenith_MaterialParams& xP = g_pxMat->ModifyParams();
		Zenith_MaterialParamTable::SetParamInt   (xP, MATERIAL_PARAM_BLEND_MODE, MATERIAL_BLEND_OPAQUE);
		Zenith_MaterialParamTable::SetParamInt   (xP, MATERIAL_PARAM_SHADING_MODEL, MATERIAL_SHADING_DEFAULT_LIT);
		Zenith_MaterialParamTable::SetParamInt   (xP, MATERIAL_PARAM_TWO_SIDED, 0);
		Zenith_MaterialParamTable::SetParamFloat (xP, MATERIAL_PARAM_ALPHA_CUTOFF, 0.5f);
		Zenith_MaterialParamTable::SetParamVector(xP, MATERIAL_PARAM_BASE_COLOR, Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.0f));
		Zenith_MaterialParamTable::SetParamFloat (xP, MATERIAL_PARAM_METALLIC, 0.0f);
		Zenith_MaterialParamTable::SetParamFloat (xP, MATERIAL_PARAM_ROUGHNESS, 0.5f);
		Zenith_MaterialParamTable::SetParamFloat (xP, MATERIAL_PARAM_SPECULAR, 0.5f);
		Zenith_MaterialParamTable::SetParamFloat (xP, MATERIAL_PARAM_NORMAL_STRENGTH, 1.0f);
		Zenith_MaterialParamTable::SetParamFloat (xP, MATERIAL_PARAM_CLEARCOAT_STRENGTH, 0.0f);
		Zenith_MaterialParamTable::SetParamVector(xP, MATERIAL_PARAM_EMISSIVE_COLOR, Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 0.0f));
		Zenith_MaterialParamTable::SetParamFloat (xP, MATERIAL_PARAM_EMISSIVE_INTENSITY, 0.0f);
		for (u_int u = 0; u < MATERIAL_TEXTURE_SLOT_COUNT; u++)
			g_pxMat->SetTexture(static_cast<MaterialTextureSlot>(u), TextureHandle());
	}

	void Shot(const char* szName)
	{
		Zenith_Log(LOG_CATEGORY_CORE, "[MaterialShowcaseShot] %s", szName);
		char szPath[256];
		snprintf(szPath, sizeof(szPath), "C:/tmp/mat_showcase_%s.tga", szName);
		Flux_Screenshot::RequestDump(szPath);
	}

	void SetFloat(MaterialParamID e, float f)  { Zenith_MaterialParamTable::SetParamFloat(g_pxMat->ModifyParams(), e, f); }
	void SetInt  (MaterialParamID e, u_int u)  { Zenith_MaterialParamTable::SetParamInt(g_pxMat->ModifyParams(), e, u); }
	void SetCol  (MaterialParamID e, float r, float g, float b, float a) { Zenith_MaterialParamTable::SetParamVector(g_pxMat->ModifyParams(), e, Zenith_Maths::Vector4(r, g, b, a)); }
}

static void Setup_MaterialShowcase()
{
	g_xShow = ShowcaseState();
	Zenith_InputSimulator::Enable();
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_xEngine.Editor().SetEditorMode(EditorMode::Stopped);
	g_xEngine.Editor().GetMaterialEditorShowFlag() = true;

	// Procedural normal map: rounded bumps on a tangent-space grid.
	g_xNormalTex.Set(MakeProceduralRGBA8(128, [](u_int x, u_int y, u_int8* p)
	{
		const float fU = (x / 128.0f) * 6.2831853f * 4.0f;
		const float fV = (y / 128.0f) * 6.2831853f * 4.0f;
		Zenith_Maths::Vector3 xN(0.6f * sinf(fU), 0.6f * sinf(fV), 1.0f);
		xN = glm::normalize(xN);
		p[0] = static_cast<u_int8>((xN.x * 0.5f + 0.5f) * 255.0f);
		p[1] = static_cast<u_int8>((xN.y * 0.5f + 0.5f) * 255.0f);
		p[2] = static_cast<u_int8>((xN.z * 0.5f + 0.5f) * 255.0f);
		p[3] = 255;
	}));

	// Procedural checker with hard 0/255 alpha for the alpha-cutout case.
	g_xCheckerAlphaTex.Set(MakeProceduralRGBA8(64, [](u_int x, u_int y, u_int8* p)
	{
		const bool bOn = (((x / 8) + (y / 8)) & 1) == 0;
		p[0] = p[1] = p[2] = 230;
		p[3] = bOn ? 255 : 0;
	}));

	g_pxMat = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	g_pxMat->SetName("ShowcaseMaterial");
	g_pxParent = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	g_pxParent->SetName("ShowcaseParent");
	Zenith_MaterialParamTable::SetParamVector(g_pxParent->ModifyParams(), MATERIAL_PARAM_BASE_COLOR, Zenith_Maths::Vector4(0.85f, 0.08f, 0.06f, 1.0f));
	Zenith_MaterialParamTable::SetParamFloat (g_pxParent->ModifyParams(), MATERIAL_PARAM_ROUGHNESS, 0.4f);

	g_xEngine.Editor().SelectMaterial(g_pxMat);
	g_xShow.m_bReady = (g_pxMat != nullptr && g_pxParent != nullptr && static_cast<bool>(g_xNormalTex) && static_cast<bool>(g_xCheckerAlphaTex));
	if (!g_xShow.m_bReady) Fail("setup resources missing");
}

static bool Step_MaterialShowcase(int iFrame)
{
	if (g_xShow.m_bFailed) return false;
	g_xEngine.Editor().GetMaterialEditorShowFlag() = true;
	Flux_MaterialPreviewImpl& xPrev = g_xEngine.MaterialPreview();

	switch (iFrame)
	{
	// 1. Smooth dielectric — tight bright IBL highlight, dark body.
	case 30:
		ResetMaterial(); SetFloat(MATERIAL_PARAM_ROUGHNESS, 0.07f);
		xPrev.SetPreviewMesh(MATERIAL_PREVIEW_MESH_SPHERE);
		Shot("01_smooth_dielectric"); break;

	// 2. Rough dielectric — broad soft highlight, same base.
	case 58:
		ResetMaterial(); SetFloat(MATERIAL_PARAM_ROUGHNESS, 0.85f);
		Shot("02_rough_dielectric"); break;

	// 3. Polished metal — gold base tints the reflection (metallic F0 = albedo).
	case 86:
		ResetMaterial(); SetFloat(MATERIAL_PARAM_METALLIC, 1.0f); SetFloat(MATERIAL_PARAM_ROUGHNESS, 0.12f);
		SetCol(MATERIAL_PARAM_BASE_COLOR, 1.0f, 0.78f, 0.34f, 1.0f);
		Shot("03_polished_metal"); break;

	// 4. Specular F0 killed — dielectric with specular 0 => almost no highlight.
	case 114:
		ResetMaterial(); SetFloat(MATERIAL_PARAM_ROUGHNESS, 0.2f); SetFloat(MATERIAL_PARAM_SPECULAR, 0.0f);
		Shot("04_specular_zero"); break;

	// 5. Clear coat — matte red base under a sharp glossy coat (second lobe).
	case 142:
		ResetMaterial(); SetCol(MATERIAL_PARAM_BASE_COLOR, 0.6f, 0.05f, 0.05f, 1.0f);
		SetFloat(MATERIAL_PARAM_ROUGHNESS, 0.8f);
		SetFloat(MATERIAL_PARAM_CLEARCOAT_STRENGTH, 1.0f); SetFloat(MATERIAL_PARAM_CLEARCOAT_ROUGHNESS, 0.04f);
		Shot("05_clearcoat"); break;

	// 6. HDR emissive — bloom-ready glow independent of lighting.
	case 170:
		ResetMaterial(); SetCol(MATERIAL_PARAM_BASE_COLOR, 0.02f, 0.02f, 0.02f, 1.0f);
		SetCol(MATERIAL_PARAM_EMISSIVE_COLOR, 0.1f, 0.85f, 1.0f, 0.0f); SetFloat(MATERIAL_PARAM_EMISSIVE_INTENSITY, 25.0f);
		Shot("06_emissive_bloom"); break;

	// 7. Unlit — flat magenta, ignores IBL + sun entirely.
	case 198:
		ResetMaterial(); SetInt(MATERIAL_PARAM_SHADING_MODEL, MATERIAL_SHADING_UNLIT);
		SetCol(MATERIAL_PARAM_BASE_COLOR, 0.9f, 0.1f, 0.8f, 1.0f);
		g_xShow.m_bUnlitApplied = (g_pxMat->GetResolved().m_xParams.m_eShadingModel == MATERIAL_SHADING_UNLIT);
		Shot("07_unlit"); break;

	// 8. Normal mapping — procedural tangent-space bumps break up the surface.
	case 226:
		ResetMaterial(); SetFloat(MATERIAL_PARAM_ROUGHNESS, 0.35f);
		g_pxMat->SetTexture(MATERIAL_TEXTURE_NORMAL, g_xNormalTex);
		SetFloat(MATERIAL_PARAM_NORMAL_STRENGTH, 1.6f);
		Shot("08_normal_mapped"); break;

	// 9. Alpha cutout — Masked blend punches holes where checker alpha < cutoff.
	case 254:
		ResetMaterial(); SetInt(MATERIAL_PARAM_BLEND_MODE, MATERIAL_BLEND_MASKED); SetFloat(MATERIAL_PARAM_ALPHA_CUTOFF, 0.5f);
		g_pxMat->SetTexture(MATERIAL_TEXTURE_BASE_COLOR, g_xCheckerAlphaTex);
		xPrev.SetPreviewMesh(MATERIAL_PREVIEW_MESH_PLANE);
		g_xShow.m_bMaskedApplied = (g_pxMat->GetResolved().m_xParams.m_eBlendMode == MATERIAL_BLEND_MASKED);
		Shot("09_alpha_cutout"); break;

	// 10. Forward translucency — see-through cyan over the IBL environment.
	case 282:
		ResetMaterial(); SetInt(MATERIAL_PARAM_BLEND_MODE, MATERIAL_BLEND_TRANSLUCENT);
		SetCol(MATERIAL_PARAM_BASE_COLOR, 0.1f, 0.7f, 0.9f, 0.4f); SetFloat(MATERIAL_PARAM_ROUGHNESS, 0.1f);
		xPrev.SetPreviewMesh(MATERIAL_PREVIEW_MESH_SPHERE);
		Shot("10_translucent"); break;

	// 11. Instance override — child of a red parent overrides base colour to green.
	case 310:
	{
		ResetMaterial();
		g_pxMat->SetParent(MaterialHandle(g_pxParent));
		SetCol(MATERIAL_PARAM_BASE_COLOR, 0.06f, 0.7f, 0.12f, 1.0f);
		g_pxMat->SetOverride(MATERIAL_PARAM_BASE_COLOR, true);
		SetFloat(MATERIAL_PARAM_METALLIC, 1.0f); SetFloat(MATERIAL_PARAM_ROUGHNESS, 0.18f);
		g_pxMat->SetOverride(MATERIAL_PARAM_METALLIC, true);
		g_pxMat->SetOverride(MATERIAL_PARAM_ROUGHNESS, true);
		const Zenith_MaterialResolved& xR = g_pxMat->GetResolved();
		g_xShow.m_bInstanceResolved = (xR.m_xParams.m_xBaseColor.y > 0.5f && xR.m_xParams.m_xBaseColor.x < 0.2f);
		Shot("11_instance_override");
		break;
	}

	case 340:
		return false;

	default:
		break;
	}
	return true;
}

static bool Verify_MaterialShowcase()
{
	if (g_xShow.m_bFailed) { Zenith_Error(LOG_CATEGORY_CORE, "[MaterialShowcase] hard fail: %s", g_xShow.m_szWhy); return false; }
	if (!g_xShow.m_bReady) { Zenith_Error(LOG_CATEGORY_CORE, "[MaterialShowcase] resources never readied"); return false; }
	if (!g_xShow.m_bUnlitApplied)    { Zenith_Error(LOG_CATEGORY_CORE, "[MaterialShowcase] unlit shading model did not resolve"); return false; }
	if (!g_xShow.m_bMaskedApplied)   { Zenith_Error(LOG_CATEGORY_CORE, "[MaterialShowcase] masked blend mode did not resolve"); return false; }
	if (!g_xShow.m_bInstanceResolved){ Zenith_Error(LOG_CATEGORY_CORE, "[MaterialShowcase] instance base-colour override did not resolve over parent"); return false; }

	// Teardown.
	g_xEngine.Editor().ClearMaterialSelection();
	g_xEngine.Editor().GetMaterialEditorShowFlag() = false;
	g_xEngine.MaterialPreview().SetActive(false);
	g_pxMat = nullptr; g_pxParent = nullptr;
	g_xNormalTex.Clear(); g_xCheckerAlphaTex.Clear();
	return true;
}

static const Zenith_AutomatedTest g_xMaterialShowcaseTest = {
	"Test_MaterialShowcase",
	&Setup_MaterialShowcase,
	&Step_MaterialShowcase,
	&Verify_MaterialShowcase,
	/*maxFrames*/ 380,
	/*requiresGraphics*/ true
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xMaterialShowcaseTest);

#endif // ZENITH_TOOLS && ZENITH_INPUT_SIMULATOR
