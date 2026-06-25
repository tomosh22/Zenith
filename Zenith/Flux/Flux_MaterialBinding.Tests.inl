#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/Flux_MaterialGPU.h"
#include "Flux/Flux_MaterialBinding.h"
#include "AssetHandling/Zenith_MaterialAsset.h"

// ============================================================================
// Flux material-record unit tests (Phase 3 bindless materials)
//
// Cover the pure packing of a resolved material into the GPU record
// (Flux_PackMaterialGPU / Flux_BuildMaterialDrawFlags): a non-zero alpha cutoff
// reaches the shader ONLY for MATERIAL_BLEND_MASKED materials (opaque /
// translucent / additive write cutoff 0 so the shader's discard never fires),
// the feature-flag derivation, and the bindless texture-index packing. Pure CPU —
// materials are constructed on the stack with no registry/GPU involvement; the
// texture indices are supplied directly (the renderer resolves them from each
// slot's bindless index at runtime).
// ============================================================================

namespace
{
	// Nine dummy bindless texture indices in MaterialTextureSlot order.
	struct DummyTexIdx
	{
		u_int m_au[MATERIAL_TEXTURE_SLOT_COUNT] = { 11u, 12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u };
	};
}

ZENITH_TEST(MaterialBinding, MaskedMaterialFeedsAuthoredCutoff)
{
	Zenith_MaterialAsset xMat;
	xMat.SetBlendMode(MATERIAL_BLEND_MASKED);
	xMat.SetAlphaCutoff(0.45f);

	DummyTexIdx xIdx;
	Flux_MaterialGPU xRec;
	Flux_PackMaterialGPU(xRec, xMat.GetResolved(), xIdx.m_au);

	// m_xMaterialParams = (metallic, roughness, alphaCutoff, occlusionStrength).
	ZENITH_ASSERT_EQ_FLOAT(xRec.m_xMaterialParams.z, 0.45f, 0.0001f,
		"Masked material must feed its authored cutoff to the shader");
}

ZENITH_TEST(MaterialBinding, OpaqueMaterialZeroesCutoff)
{
	Zenith_MaterialAsset xMat;
	xMat.SetBlendMode(MATERIAL_BLEND_OPAQUE);
	xMat.SetAlphaCutoff(0.45f);   // authored, but must be ignored when opaque

	DummyTexIdx xIdx;
	Flux_MaterialGPU xRec;
	Flux_PackMaterialGPU(xRec, xMat.GetResolved(), xIdx.m_au);

	ZENITH_ASSERT_EQ_FLOAT(xRec.m_xMaterialParams.z, 0.0f, 0.0001f,
		"Opaque must never alpha-test (cutoff 0 disables the shader discard)");
}

ZENITH_TEST(MaterialBinding, TranslucentMaterialZeroesCutoff)
{
	Zenith_MaterialAsset xMat;
	xMat.SetBlendMode(MATERIAL_BLEND_TRANSLUCENT);
	xMat.SetAlphaCutoff(0.7f);

	DummyTexIdx xIdx;
	Flux_MaterialGPU xRec;
	Flux_PackMaterialGPU(xRec, xMat.GetResolved(), xIdx.m_au);

	ZENITH_ASSERT_EQ_FLOAT(xRec.m_xMaterialParams.z, 0.0f, 0.0001f,
		"Translucent uses blending, not cutoff (cutoff stays 0)");
}

ZENITH_TEST(MaterialBinding, DrawFlagsReflectShadingModelAndTwoSided)
{
	Zenith_MaterialAsset xMat;
	xMat.SetBlendMode(MATERIAL_BLEND_OPAQUE);
	xMat.SetShadingModel(MATERIAL_SHADING_UNLIT);
	xMat.SetTwoSided(true);

	const u_int uFlags = Flux_BuildMaterialDrawFlags(xMat.GetResolved());

	ZENITH_ASSERT_TRUE((uFlags & MATERIAL_DRAW_FLAG_UNLIT) != 0u,
		"Unlit shading model sets the UNLIT draw flag");
	ZENITH_ASSERT_TRUE((uFlags & MATERIAL_DRAW_FLAG_TWO_SIDED_NORMAL_FLIP) != 0u,
		"Two-sided sets the normal-flip draw flag");
	ZENITH_ASSERT_TRUE((uFlags & MATERIAL_DRAW_FLAG_POM) == 0u,
		"No height texture bound -> no POM flag");
	ZENITH_ASSERT_TRUE((uFlags & MATERIAL_DRAW_FLAG_DETAIL_MAPS) == 0u,
		"No detail textures bound -> no detail-maps flag");
}

ZENITH_TEST(MaterialBinding, PackedRecordCarriesBindlessTextureIndices)
{
	Zenith_MaterialAsset xMat;
	xMat.SetBlendMode(MATERIAL_BLEND_OPAQUE);

	DummyTexIdx xIdx;
	Flux_MaterialGPU xRec;
	Flux_PackMaterialGPU(xRec, xMat.GetResolved(), xIdx.m_au);

	// The 9 supplied bindless indices land in m_auTexIdx[0..8], in slot order; the
	// 3-slot pad tail (9..11) is zeroed.
	for (u_int u = 0; u < MATERIAL_TEXTURE_SLOT_COUNT; u++)
	{
		ZENITH_ASSERT_TRUE(xRec.m_auTexIdx[u] == xIdx.m_au[u],
			"Packed record must carry each slot's bindless texture index in order");
	}
	ZENITH_ASSERT_TRUE(xRec.m_auTexIdx[9] == 0u && xRec.m_auTexIdx[10] == 0u && xRec.m_auTexIdx[11] == 0u,
		"Texture-index pad tail must be zeroed");
}

ZENITH_TEST(MaterialBinding, MeshDrawConstantsCarryMaterialIndex)
{
	MeshDrawConstants xDC;
	BuildMeshDrawConstants(xDC, Zenith_Maths::Matrix4(1.0f), 42u);

	ZENITH_ASSERT_TRUE(xDC.m_uMaterialIndex == 42u,
		"MeshDrawConstants must carry the material-table index for the shader");
	// VAT defaults to zero for non-instanced draws (instanced overwrites afterwards).
	ZENITH_ASSERT_EQ_FLOAT(xDC.m_xVATParams.z, 0.0f, 0.0001f,
		"VAT-enabled flag must default to 0 (non-instanced)");
}
