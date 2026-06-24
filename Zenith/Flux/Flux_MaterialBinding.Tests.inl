#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/Flux_MaterialBinding.h"
#include "AssetHandling/Zenith_MaterialAsset.h"

// ============================================================================
// Flux material-binding unit tests
//
// Cover the rule every G-buffer/shadow mesh writer now shares: a non-zero alpha
// cutoff reaches the shader ONLY for MATERIAL_BLEND_MASKED materials (opaque /
// translucent / additive write cutoff 0 so the shader's discard never fires),
// and the feature-flag derivation. Pure CPU — materials are constructed on the
// stack with no registry/GPU involvement (BuildMaterialDrawFlags only reads the
// resolved param block and the emptiness of unset texture handles).
// ============================================================================

namespace
{
	Zenith_Maths::Matrix4 IdentityMat() { return Zenith_Maths::Matrix4(1.0f); }
}

ZENITH_TEST(MaterialBinding, MaskedMaterialFeedsAuthoredCutoff)
{
	Zenith_MaterialAsset xMat;
	xMat.SetBlendMode(MATERIAL_BLEND_MASKED);
	xMat.SetAlphaCutoff(0.45f);

	MaterialDrawConstants xConstants;
	BuildMaterialDrawConstants(xConstants, IdentityMat(), &xMat);

	// m_xMaterialParams = (metallic, roughness, alphaCutoff, occlusionStrength).
	ZENITH_ASSERT_EQ_FLOAT(xConstants.m_xMaterialParams.z, 0.45f, 0.0001f,
		"Masked material must feed its authored cutoff to the shader");
}

ZENITH_TEST(MaterialBinding, OpaqueMaterialZeroesCutoff)
{
	Zenith_MaterialAsset xMat;
	xMat.SetBlendMode(MATERIAL_BLEND_OPAQUE);
	xMat.SetAlphaCutoff(0.45f);   // authored, but must be ignored when opaque

	MaterialDrawConstants xConstants;
	BuildMaterialDrawConstants(xConstants, IdentityMat(), &xMat);

	ZENITH_ASSERT_EQ_FLOAT(xConstants.m_xMaterialParams.z, 0.0f, 0.0001f,
		"Opaque must never alpha-test (cutoff 0 disables the shader discard)");
}

ZENITH_TEST(MaterialBinding, TranslucentMaterialZeroesCutoff)
{
	Zenith_MaterialAsset xMat;
	xMat.SetBlendMode(MATERIAL_BLEND_TRANSLUCENT);
	xMat.SetAlphaCutoff(0.7f);

	MaterialDrawConstants xConstants;
	BuildMaterialDrawConstants(xConstants, IdentityMat(), &xMat);

	ZENITH_ASSERT_EQ_FLOAT(xConstants.m_xMaterialParams.z, 0.0f, 0.0001f,
		"Translucent uses blending, not cutoff (cutoff stays 0)");
}

ZENITH_TEST(MaterialBinding, DrawFlagsReflectShadingModelAndTwoSided)
{
	Zenith_MaterialAsset xMat;
	xMat.SetBlendMode(MATERIAL_BLEND_OPAQUE);
	xMat.SetShadingModel(MATERIAL_SHADING_UNLIT);
	xMat.SetTwoSided(true);

	const u_int uFlags = BuildMaterialDrawFlags(xMat.GetResolved());

	ZENITH_ASSERT_TRUE((uFlags & MATERIAL_DRAW_FLAG_UNLIT) != 0u,
		"Unlit shading model sets the UNLIT draw flag");
	ZENITH_ASSERT_TRUE((uFlags & MATERIAL_DRAW_FLAG_TWO_SIDED_NORMAL_FLIP) != 0u,
		"Two-sided sets the normal-flip draw flag");
	ZENITH_ASSERT_TRUE((uFlags & MATERIAL_DRAW_FLAG_POM) == 0u,
		"No height texture bound -> no POM flag");
	ZENITH_ASSERT_TRUE((uFlags & MATERIAL_DRAW_FLAG_DETAIL_MAPS) == 0u,
		"No detail textures bound -> no detail-maps flag");
}
