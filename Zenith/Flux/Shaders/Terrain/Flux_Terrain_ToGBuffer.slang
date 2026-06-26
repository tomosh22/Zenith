// =====================================================================
// Flux_Terrain_ToGBuffer — Slang module.
//
// Subsystem: Terrain. Splatmap-based 4-material blending. Vertex
// attributes come in pre-packed (SNORM10:10:10:2 for normal + tangent)
// — Vulkan auto-unpacks to float4. Position is already in world space
// (terrain is static, no model matrix). Vertex stage reads its own LOD
// level from a per-chunk storage buffer keyed by firstInstance.
//
// Splatmap RGBA selects between four materials (grass / rock / dirt /
// sand by convention). Each material has its own diffuse / normal /
// roughness-metallic / occlusion / emissive samplers.
//
// All of this pass's own resources (per-frame UV scale, LOD-level buffer,
// splatmap, the 20-texture 4-material table, and the per-draw material CB) live
// in one PassParams ParameterBlock at set 3. The spine include reserves
// sets 0/1/2 (GLOBAL/VIEW/BINDLESS); the camera matrix comes from g_xViewSet.
// =====================================================================

#include "Common/Bindings.slang"   // spine: GLOBAL/VIEW/BINDLESS at sets 0/1/2
import Common.Material;
import Common.MaterialTable;        // Phase 4c: MaterialGPU record (bindless terrain layer textures)
import Common.GBuffer;

struct VsIn
{
	float3 a_xPosition       : POSITION;
	float2 a_xUV             : TEXCOORD0;
	float4 a_xNormalPacked   : NORMAL;   // SNORM10:10:10:2 → xyz normal, w unused
	float4 a_xTangentPacked  : TANGENT;  // SNORM10:10:10:2 → xyz tangent, w bitangent sign
};

struct VsOut
{
	float4 position           : SV_Position;
	float2 uv                 : TEXCOORD0;
	float3 worldNormal        : TEXCOORD1;
	float3 worldPos           : TEXCOORD2;
	float3 worldTangent       : TEXCOORD3;
	nointerpolation uint lodLevel : TEXCOORD4;
	float  bitangentSign      : TEXCOORD5;
	// Source chunk index (= firstInstance from the indirect draw command).
	// Flat-interpolated so debug mode 12 paints the entire triangle with the
	// colour of its issuing chunk — required to identify which chunk is
	// producing a stretched / nonsensical triangle, since worldPos.xz varies
	// across the surface (which is exactly the symptom we're hunting).
	nointerpolation uint chunkIndex : TEXCOORD6;
};

// Per-frame UV scale.
struct TerrainConstantsLayout { float g_fUVScale; };

// Per-draw 288-byte CB. g_xTerrainParams2.y holds the debug-mode index
// (uint reinterpreted as float per Vulkan std140 layout — caller writes
// as float bits).
struct TerrainMaterialConstantsLayout
{
	float4 g_axBaseColors[4];      // RGBA per material
	float4 g_axUVParams[4];        // (tilingX, tilingY, offsetX, offsetY)
	float4 g_axMaterialParams[4];  // (metallic, roughness, occlusion, specular)
	float4 g_axEmissiveParams[4];  // (R, G, B, intensity)
	float4 g_xTerrainParams;       // (originX, originZ, sizeX, sizeZ)
	float4 g_xTerrainParams2;      // (materialCount-bits, debugMode-bits, 0, 0)
	uint4  g_xMaterialTableIndices; // Phase 4c: GPU material-table index per splat slot (draw-uniform)
};

// All terrain pass-frequency resources land in one ParameterBlock at set 3.
// Phase 4c: the 20 per-material samplers are gone — the 4 layers' textures are
// bindless, looked up via g_axTextures[g_axMaterials[slotIdx].<map>Idx]. g_axMaterials
// (the GPU material table) is a member here (bound once per pass; binding-0
// TerrainConstants is always bound so the multi-member block is safe). The splatmap
// stays a named sampler. LODLevelBuffer is read-only in the vertex stage so Slang
// emits the NonWritable decoration (vertexPipelineStoresAndAtomics is off).
struct PassParams
{
	ConstantBuffer<TerrainConstantsLayout>         TerrainConstants;
	ConstantBuffer<TerrainMaterialConstantsLayout> TerrainMaterialConstants;
	StructuredBuffer<uint>                         LODLevelBuffer;
	Sampler2D g_xSplatmap;
	StructuredBuffer<MaterialGPU>                  g_axMaterials;
};
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
VsOut vsMain(VsIn input,
             uint instanceId : SV_InstanceID,
             uint startInstance : SV_StartInstanceLocation)
{
	VsOut o;
	float3 xWorldPos = input.a_xPosition; // terrain is static, no model matrix

	o.worldPos      = xWorldPos;
	o.uv            = input.a_xUV * g_xPassSet.TerrainConstants.g_fUVScale;
	o.worldNormal   = normalize(input.a_xNormalPacked.xyz);
	o.worldTangent  = normalize(input.a_xTangentPacked.xyz);
	// Bitangent sign was pre-computed at export and packed into tangent.w.
	o.bitangentSign = input.a_xTangentPacked.w >= 0.0 ? 1.0 : -1.0;

	// Slang's SV_InstanceID compiles to SPIR-V BuiltIn InstanceId, which is
	// the instance number within the draw (0 to instanceCount-1) and does
	// NOT include firstInstance. The terrain culling shader writes
	// firstInstance = chunkIndex per draw command, so we add SV_StartInstance-
	// Location (== BaseInstance == firstInstance) to recover the chunk id.
	// Without this, every chunk reads LODLevelBuffer[0], so the LOD
	// visualisation shows the colour for chunk 0 across the whole terrain
	// (the long-running "red/green flicker" symptom) and the source-chunk
	// debug visualisation paints everything black (chunk 0's hash).
	uint uChunkId = startInstance + instanceId;
	o.lodLevel = g_xPassSet.LODLevelBuffer[uChunkId];
	o.chunkIndex = uChunkId;

	o.position = mul(g_xViewSet.g_xView.g_xViewProjMat, float4(xWorldPos, 1.0));
	return o;
}

float3 LODDebugColour(uint lod)
{
	switch (lod)
	{
		case 0u: return float3(1.0, 0.0, 0.0);
		case 1u: return float3(0.0, 1.0, 0.0);
		case 2u: return float3(0.0, 0.0, 1.0);
		case 3u: return float3(1.0, 0.0, 1.0);
		default: return float3(1.0, 1.0, 0.0);
	}
}

[shader("fragment")]
GBufferOut fsMain(VsOut input)
{
	uint uDebugMode = asuint(g_xPassSet.TerrainMaterialConstants.g_xTerrainParams2.y);

	// Pre-blend debug paths return the appropriate colour without
	// running the splatmap blend at all.
	if (uDebugMode > 0u)
	{
		float3 xDebugColor = float3(1.0, 0.0, 1.0);
		bool bHandled = true;
		switch (uDebugMode)
		{
			case 1u: xDebugColor = LODDebugColour(input.lodLevel); break;
			case 2u: xDebugColor = input.worldNormal * 0.5 + 0.5;  break;
			case 3u: xDebugColor = float3(frac(input.uv), 0.0);     break;
			case 4u:
			{
				float2 xSplatUV = (input.worldPos.xz - g_xPassSet.TerrainMaterialConstants.g_xTerrainParams.xy)
								 / g_xPassSet.TerrainMaterialConstants.g_xTerrainParams.zw;
				xDebugColor = g_xPassSet.g_xSplatmap.Sample(xSplatUV).rgb;
				break;
			}
			case 5u: case 6u: case 7u: bHandled = false; break; // need texture sampling — fall through
			case 8u: xDebugColor = frac(input.worldPos / 64.0); break;
			case 9u:
			{
				float2 xChunkLocal = fmod(input.worldPos.xz, 64.0);
				float fBorder = step(xChunkLocal.x, 0.5) + step(63.5, xChunkLocal.x)
							  + step(xChunkLocal.y, 0.5) + step(63.5, xChunkLocal.y);
				xDebugColor = lerp(float3(0.2), float3(1.0, 0.5, 0.0), min(fBorder, 1.0));
				break;
			}
			case 10u: xDebugColor = input.worldTangent * 0.5 + 0.5; break;
			case 11u: xDebugColor = input.bitangentSign > 0.0 ? float3(1.0, 0.0, 0.0) : float3(0.0, 0.0, 1.0); break;
			case 12u:
			{
				// Source-chunk visualisation — flat-shade each triangle with
				// a colour hashed from its source chunk index. A stretched
				// triangle covers many world positions but came from a
				// single chunk; this mode paints the whole stretched area
				// with that chunk's signature colour, making the culprit
				// obvious. Bit-reverse-style mixing produces high-contrast
				// colours for adjacent chunks (chunk N and chunk N+1 share
				// almost no bits in the output channels), so neighbours are
				// visually distinct even when the spike's source chunk is
				// next to non-spike chunks. A small bias keeps the result
				// well above pure black (chunk 0 → mid-grey, not invisible).
				uint h = input.chunkIndex + 1u;
				uint hr = (h * 2654435769u) >> 8u;  // golden-ratio mix → R
				uint hg = (h * 1597334677u) >> 8u;  // arbitrary prime  → G
				uint hb = (h * 2246822519u) >> 8u;  // arbitrary prime  → B
				xDebugColor = float3(
					0.25 + 0.75 * float(hr & 0xFFu) / 255.0,
					0.25 + 0.75 * float(hg & 0xFFu) / 255.0,
					0.25 + 0.75 * float(hb & 0xFFu) / 255.0);
				break;
			}
			default: break;
		}
		if (bHandled)
		{
			return MakeGBuffer(float4(xDebugColor, 1.0), input.worldNormal, 0.2, 0.8, 0.0);
		}
	}

	// Splatmap UV is in normalised terrain coords: world.xz - origin / size.
	float2 xSplatUV = (input.worldPos.xz - g_xPassSet.TerrainMaterialConstants.g_xTerrainParams.xy)
					 / g_xPassSet.TerrainMaterialConstants.g_xTerrainParams.zw;
	float4 xWeights = g_xPassSet.g_xSplatmap.Sample(xSplatUV);

	float2 xUV0 = TransformUV(input.uv, g_xPassSet.TerrainMaterialConstants.g_axUVParams[0].xy, g_xPassSet.TerrainMaterialConstants.g_axUVParams[0].zw);
	float2 xUV1 = TransformUV(input.uv, g_xPassSet.TerrainMaterialConstants.g_axUVParams[1].xy, g_xPassSet.TerrainMaterialConstants.g_axUVParams[1].zw);
	float2 xUV2 = TransformUV(input.uv, g_xPassSet.TerrainMaterialConstants.g_axUVParams[2].xy, g_xPassSet.TerrainMaterialConstants.g_axUVParams[2].zw);
	float2 xUV3 = TransformUV(input.uv, g_xPassSet.TerrainMaterialConstants.g_axUVParams[3].xy, g_xPassSet.TerrainMaterialConstants.g_axUVParams[3].zw);

	// Resolve each splat slot's GPU material record + its 5 bindless textures.
	// The material-table index is draw-uniform (from the per-draw constants), so
	// the g_axTextures[] lookups are uniform → no NonUniformResourceIndex. Slot
	// texture layout mirrors Flux_MaterialGPU: g_xTexIdx0 = (baseColor, normal,
	// roughnessMetallic, occlusion); g_xTexIdx1.x = emissive.
	MaterialGPU xMat0 = g_xPassSet.g_axMaterials[g_xPassSet.TerrainMaterialConstants.g_xMaterialTableIndices[0]];
	MaterialGPU xMat1 = g_xPassSet.g_axMaterials[g_xPassSet.TerrainMaterialConstants.g_xMaterialTableIndices[1]];
	MaterialGPU xMat2 = g_xPassSet.g_axMaterials[g_xPassSet.TerrainMaterialConstants.g_xMaterialTableIndices[2]];
	MaterialGPU xMat3 = g_xPassSet.g_axMaterials[g_xPassSet.TerrainMaterialConstants.g_xMaterialTableIndices[3]];

	// Per-material samples. Roughness/metallic come back unmultiplied
	// here because we need to blend the per-material multipliers in
	// after the splatmap weighting.
	float4 xDiffuse0 = SampleDiffuseWithBaseColor(g_xBindlessSet.g_axTextures[xMat0.g_xTexIdx0.x], xUV0, g_xPassSet.TerrainMaterialConstants.g_axBaseColors[0]);
	float3 xNormalMap0 = g_xBindlessSet.g_axTextures[xMat0.g_xTexIdx0.y].Sample(xUV0).xyz * 2.0 - 1.0;
	float2 xRM0       = g_xBindlessSet.g_axTextures[xMat0.g_xTexIdx0.z].Sample(xUV0).gb;
	float  fOcc0      = SampleOcclusion(g_xBindlessSet.g_axTextures[xMat0.g_xTexIdx0.w], xUV0, g_xPassSet.TerrainMaterialConstants.g_axMaterialParams[0].z);
	float3 xEm0       = SampleEmissiveRGB(g_xBindlessSet.g_axTextures[xMat0.g_xTexIdx1.x], xUV0, g_xPassSet.TerrainMaterialConstants.g_axEmissiveParams[0].rgb, g_xPassSet.TerrainMaterialConstants.g_axEmissiveParams[0].w);

	float4 xDiffuse1 = SampleDiffuseWithBaseColor(g_xBindlessSet.g_axTextures[xMat1.g_xTexIdx0.x], xUV1, g_xPassSet.TerrainMaterialConstants.g_axBaseColors[1]);
	float3 xNormalMap1 = g_xBindlessSet.g_axTextures[xMat1.g_xTexIdx0.y].Sample(xUV1).xyz * 2.0 - 1.0;
	float2 xRM1       = g_xBindlessSet.g_axTextures[xMat1.g_xTexIdx0.z].Sample(xUV1).gb;
	float  fOcc1      = SampleOcclusion(g_xBindlessSet.g_axTextures[xMat1.g_xTexIdx0.w], xUV1, g_xPassSet.TerrainMaterialConstants.g_axMaterialParams[1].z);
	float3 xEm1       = SampleEmissiveRGB(g_xBindlessSet.g_axTextures[xMat1.g_xTexIdx1.x], xUV1, g_xPassSet.TerrainMaterialConstants.g_axEmissiveParams[1].rgb, g_xPassSet.TerrainMaterialConstants.g_axEmissiveParams[1].w);

	float4 xDiffuse2 = SampleDiffuseWithBaseColor(g_xBindlessSet.g_axTextures[xMat2.g_xTexIdx0.x], xUV2, g_xPassSet.TerrainMaterialConstants.g_axBaseColors[2]);
	float3 xNormalMap2 = g_xBindlessSet.g_axTextures[xMat2.g_xTexIdx0.y].Sample(xUV2).xyz * 2.0 - 1.0;
	float2 xRM2       = g_xBindlessSet.g_axTextures[xMat2.g_xTexIdx0.z].Sample(xUV2).gb;
	float  fOcc2      = SampleOcclusion(g_xBindlessSet.g_axTextures[xMat2.g_xTexIdx0.w], xUV2, g_xPassSet.TerrainMaterialConstants.g_axMaterialParams[2].z);
	float3 xEm2       = SampleEmissiveRGB(g_xBindlessSet.g_axTextures[xMat2.g_xTexIdx1.x], xUV2, g_xPassSet.TerrainMaterialConstants.g_axEmissiveParams[2].rgb, g_xPassSet.TerrainMaterialConstants.g_axEmissiveParams[2].w);

	float4 xDiffuse3 = SampleDiffuseWithBaseColor(g_xBindlessSet.g_axTextures[xMat3.g_xTexIdx0.x], xUV3, g_xPassSet.TerrainMaterialConstants.g_axBaseColors[3]);
	float3 xNormalMap3 = g_xBindlessSet.g_axTextures[xMat3.g_xTexIdx0.y].Sample(xUV3).xyz * 2.0 - 1.0;
	float2 xRM3       = g_xBindlessSet.g_axTextures[xMat3.g_xTexIdx0.z].Sample(xUV3).gb;
	float  fOcc3      = SampleOcclusion(g_xBindlessSet.g_axTextures[xMat3.g_xTexIdx0.w], xUV3, g_xPassSet.TerrainMaterialConstants.g_axMaterialParams[3].z);
	float3 xEm3       = SampleEmissiveRGB(g_xBindlessSet.g_axTextures[xMat3.g_xTexIdx1.x], xUV3, g_xPassSet.TerrainMaterialConstants.g_axEmissiveParams[3].rgb, g_xPassSet.TerrainMaterialConstants.g_axEmissiveParams[3].w);

	// Splatmap-weighted blend.
	float4 xDiffuse = xDiffuse0 * xWeights.r + xDiffuse1 * xWeights.g
					 + xDiffuse2 * xWeights.b + xDiffuse3 * xWeights.a;
	float3 xNormalTangent = normalize(xNormalMap0 * xWeights.r + xNormalMap1 * xWeights.g
									   + xNormalMap2 * xWeights.b + xNormalMap3 * xWeights.a);

	// Per-material multipliers fold into the blended result with the
	// matching weight. (Multiply-then-blend gives the same answer as
	// blend-then-multiply for scalars when weights sum to 1.)
	float fRoughness = xRM0.x * g_xPassSet.TerrainMaterialConstants.g_axMaterialParams[0].y * xWeights.r
					 + xRM1.x * g_xPassSet.TerrainMaterialConstants.g_axMaterialParams[1].y * xWeights.g
					 + xRM2.x * g_xPassSet.TerrainMaterialConstants.g_axMaterialParams[2].y * xWeights.b
					 + xRM3.x * g_xPassSet.TerrainMaterialConstants.g_axMaterialParams[3].y * xWeights.a;
	float fMetallic  = xRM0.y * g_xPassSet.TerrainMaterialConstants.g_axMaterialParams[0].x * xWeights.r
					 + xRM1.y * g_xPassSet.TerrainMaterialConstants.g_axMaterialParams[1].x * xWeights.g
					 + xRM2.y * g_xPassSet.TerrainMaterialConstants.g_axMaterialParams[2].x * xWeights.b
					 + xRM3.y * g_xPassSet.TerrainMaterialConstants.g_axMaterialParams[3].x * xWeights.a;
	float fOcclusion = fOcc0 * xWeights.r + fOcc1 * xWeights.g
					 + fOcc2 * xWeights.b + fOcc3 * xWeights.a;
	float3 xEmissive = xEm0  * xWeights.r + xEm1  * xWeights.g
					 + xEm2  * xWeights.b + xEm3  * xWeights.a;
	float fSpecular  = g_xPassSet.TerrainMaterialConstants.g_axMaterialParams[0].w * xWeights.r
					 + g_xPassSet.TerrainMaterialConstants.g_axMaterialParams[1].w * xWeights.g
					 + g_xPassSet.TerrainMaterialConstants.g_axMaterialParams[2].w * xWeights.b
					 + g_xPassSet.TerrainMaterialConstants.g_axMaterialParams[3].w * xWeights.a;

	if (uDebugMode >= 5u && uDebugMode <= 7u)
	{
		float3 xDebugColor;
		if      (uDebugMode == 5u) xDebugColor = float3(fRoughness, fRoughness, fRoughness);
		else if (uDebugMode == 6u) xDebugColor = float3(fMetallic, fMetallic, fMetallic);
		else                       xDebugColor = float3(fOcclusion, fOcclusion, fOcclusion);
		return MakeGBuffer(float4(xDebugColor, 1.0), input.worldNormal, 0.2, 0.8, 0.0);
	}

	float3x3 TBN          = BuildTBN(input.worldNormal, input.worldTangent, input.bitangentSign);
	float3   xWorldNormal = normalize(mul(TBN, xNormalTangent));

	// Terrain is always default-lit, no clear coat; splat-weighted specular.
	return MakeGBuffer(xDiffuse, xWorldNormal, fOcclusion, fRoughness, fMetallic,
						fSpecular, xEmissive, 0.0, 0.0, GBUFFER_SHADING_DEFAULT_LIT);
}
