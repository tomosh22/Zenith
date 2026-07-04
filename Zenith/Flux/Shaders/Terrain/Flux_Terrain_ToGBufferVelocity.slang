// =====================================================================
// Flux_Terrain_ToGBufferVelocity — the velocity variant of Flux_Terrain_ToGBuffer.
//
// Recorded INSTEAD of the base terrain G-buffer program inside the 5-attachment
// terrain G-buffer pass when the velocity latch is on (TAA active). Identical
// shading to the base (same splatmap 4-material blend), plus it writes
// MRT_INDEX_VELOCITY (SV_Target4) with the per-pixel motion vector.
//
// Terrain vertices are STATIC world-space positions (no model matrix), so the
// previous-frame world position equals the current one — the motion vector is pure
// camera reprojection: velocity = uvCur(ViewProjNoJitter) - uvPrev(PrevViewProjNoJitter).
// This is exactly correct for static terrain and is what stops terrain smearing when
// the camera moves (the base pass writes velocity = 0, which ghosts under camera pan).
//
// Kept a copy of the base rather than a shared #include so the working base terrain
// shader is untouched; the only diffs from Flux_Terrain_ToGBuffer are the two clip
// outputs, the velocity encode, and the GBufferVelocityOut return type.
// =====================================================================

#include "Common/Bindings.slang"   // spine: GLOBAL/VIEW/BINDLESS at sets 0/1/2
import Common.Material;
import Common.MaterialTable;
import Common.GBuffer;
import Common.Velocity;            // GBufferVelocityOut + Flux_EncodeVelocityUV + MakeGBufferVelocity

struct VsIn
{
	float3 a_xPosition       : POSITION;
	float2 a_xUV             : TEXCOORD0;
	float4 a_xNormalPacked   : NORMAL;
	float4 a_xTangentPacked  : TANGENT;
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
	nointerpolation uint chunkIndex : TEXCOORD6;
	// Motion-vector clip positions from the UNJITTERED matrices (SV_Position stays
	// jittered so the raster is jittered; velocity is computed jitter-free).
	float4 clipCurNoJitter    : TEXCOORD7;
	float4 clipPrevNoJitter   : TEXCOORD8;
};

struct TerrainConstantsLayout { float g_fUVScale; };

struct TerrainMaterialConstantsLayout
{
	float4 g_axBaseColors[4];
	float4 g_axUVParams[4];
	float4 g_axMaterialParams[4];
	float4 g_axEmissiveParams[4];
	float4 g_xTerrainParams;
	float4 g_xTerrainParams2;
	uint4  g_xMaterialTableIndices;
};

struct PassParams
{
	ConstantBuffer<TerrainConstantsLayout>         TerrainConstants;
	ConstantBuffer<TerrainMaterialConstantsLayout> TerrainMaterialConstants;
	StructuredBuffer<uint>                         LODLevelBuffer;
	Sampler2D g_xSplatmap;
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
	o.bitangentSign = input.a_xTangentPacked.w >= 0.0 ? 1.0 : -1.0;

	uint uChunkId = startInstance + instanceId;
	o.lodLevel = g_xPassSet.LODLevelBuffer[uChunkId];
	o.chunkIndex = uChunkId;

	o.position = mul(GetViewProjMat(), float4(xWorldPos, 1.0));

	// Static terrain: prev world pos == current world pos, so the motion vector is pure
	// camera reprojection through the unjittered current + previous view-proj matrices.
	o.clipCurNoJitter  = mul(GetViewProjMatNoJitter(),     float4(xWorldPos, 1.0));
	o.clipPrevNoJitter = mul(GetPrevViewProjMatNoJitter(), float4(xWorldPos, 1.0));
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
GBufferVelocityOut fsMain(VsOut input)
{
	// Motion vector (shared by every return path below).
	float2 xVelocity = Flux_EncodeVelocityUV(input.clipCurNoJitter, input.clipPrevNoJitter);

	uint uDebugMode = asuint(g_xPassSet.TerrainMaterialConstants.g_xTerrainParams2.y);

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
			case 5u: case 6u: case 7u: bHandled = false; break;
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
				uint h = input.chunkIndex + 1u;
				uint hr = (h * 2654435769u) >> 8u;
				uint hg = (h * 1597334677u) >> 8u;
				uint hb = (h * 2246822519u) >> 8u;
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
			return MakeGBufferVelocity(MakeGBuffer(float4(xDebugColor, 1.0), input.worldNormal, 0.2, 0.8, 0.0), xVelocity);
		}
	}

	float2 xSplatUV = (input.worldPos.xz - g_xPassSet.TerrainMaterialConstants.g_xTerrainParams.xy)
					 / g_xPassSet.TerrainMaterialConstants.g_xTerrainParams.zw;
	float4 xWeights = g_xPassSet.g_xSplatmap.Sample(xSplatUV);

	float2 xUV0 = TransformUV(input.uv, g_xPassSet.TerrainMaterialConstants.g_axUVParams[0].xy, g_xPassSet.TerrainMaterialConstants.g_axUVParams[0].zw);
	float2 xUV1 = TransformUV(input.uv, g_xPassSet.TerrainMaterialConstants.g_axUVParams[1].xy, g_xPassSet.TerrainMaterialConstants.g_axUVParams[1].zw);
	float2 xUV2 = TransformUV(input.uv, g_xPassSet.TerrainMaterialConstants.g_axUVParams[2].xy, g_xPassSet.TerrainMaterialConstants.g_axUVParams[2].zw);
	float2 xUV3 = TransformUV(input.uv, g_xPassSet.TerrainMaterialConstants.g_axUVParams[3].xy, g_xPassSet.TerrainMaterialConstants.g_axUVParams[3].zw);

	MaterialGPU xMat0 = GetMaterial(g_xPassSet.TerrainMaterialConstants.g_xMaterialTableIndices[0]);
	MaterialGPU xMat1 = GetMaterial(g_xPassSet.TerrainMaterialConstants.g_xMaterialTableIndices[1]);
	MaterialGPU xMat2 = GetMaterial(g_xPassSet.TerrainMaterialConstants.g_xMaterialTableIndices[2]);
	MaterialGPU xMat3 = GetMaterial(g_xPassSet.TerrainMaterialConstants.g_xMaterialTableIndices[3]);

	float4 xDiffuse0 = SampleDiffuseWithBaseColor(GetBindlessTexture(xMat0.g_xTexIdx0.x), xUV0, g_xPassSet.TerrainMaterialConstants.g_axBaseColors[0]);
	float3 xNormalMap0 = GetBindlessTexture(xMat0.g_xTexIdx0.y).Sample(xUV0).xyz * 2.0 - 1.0;
	float2 xRM0       = GetBindlessTexture(xMat0.g_xTexIdx0.z).Sample(xUV0).gb;
	float  fOcc0      = SampleOcclusion(GetBindlessTexture(xMat0.g_xTexIdx0.w), xUV0, g_xPassSet.TerrainMaterialConstants.g_axMaterialParams[0].z);
	float3 xEm0       = SampleEmissiveRGB(GetBindlessTexture(xMat0.g_xTexIdx1.x), xUV0, g_xPassSet.TerrainMaterialConstants.g_axEmissiveParams[0].rgb, g_xPassSet.TerrainMaterialConstants.g_axEmissiveParams[0].w);

	float4 xDiffuse1 = SampleDiffuseWithBaseColor(GetBindlessTexture(xMat1.g_xTexIdx0.x), xUV1, g_xPassSet.TerrainMaterialConstants.g_axBaseColors[1]);
	float3 xNormalMap1 = GetBindlessTexture(xMat1.g_xTexIdx0.y).Sample(xUV1).xyz * 2.0 - 1.0;
	float2 xRM1       = GetBindlessTexture(xMat1.g_xTexIdx0.z).Sample(xUV1).gb;
	float  fOcc1      = SampleOcclusion(GetBindlessTexture(xMat1.g_xTexIdx0.w), xUV1, g_xPassSet.TerrainMaterialConstants.g_axMaterialParams[1].z);
	float3 xEm1       = SampleEmissiveRGB(GetBindlessTexture(xMat1.g_xTexIdx1.x), xUV1, g_xPassSet.TerrainMaterialConstants.g_axEmissiveParams[1].rgb, g_xPassSet.TerrainMaterialConstants.g_axEmissiveParams[1].w);

	float4 xDiffuse2 = SampleDiffuseWithBaseColor(GetBindlessTexture(xMat2.g_xTexIdx0.x), xUV2, g_xPassSet.TerrainMaterialConstants.g_axBaseColors[2]);
	float3 xNormalMap2 = GetBindlessTexture(xMat2.g_xTexIdx0.y).Sample(xUV2).xyz * 2.0 - 1.0;
	float2 xRM2       = GetBindlessTexture(xMat2.g_xTexIdx0.z).Sample(xUV2).gb;
	float  fOcc2      = SampleOcclusion(GetBindlessTexture(xMat2.g_xTexIdx0.w), xUV2, g_xPassSet.TerrainMaterialConstants.g_axMaterialParams[2].z);
	float3 xEm2       = SampleEmissiveRGB(GetBindlessTexture(xMat2.g_xTexIdx1.x), xUV2, g_xPassSet.TerrainMaterialConstants.g_axEmissiveParams[2].rgb, g_xPassSet.TerrainMaterialConstants.g_axEmissiveParams[2].w);

	float4 xDiffuse3 = SampleDiffuseWithBaseColor(GetBindlessTexture(xMat3.g_xTexIdx0.x), xUV3, g_xPassSet.TerrainMaterialConstants.g_axBaseColors[3]);
	float3 xNormalMap3 = GetBindlessTexture(xMat3.g_xTexIdx0.y).Sample(xUV3).xyz * 2.0 - 1.0;
	float2 xRM3       = GetBindlessTexture(xMat3.g_xTexIdx0.z).Sample(xUV3).gb;
	float  fOcc3      = SampleOcclusion(GetBindlessTexture(xMat3.g_xTexIdx0.w), xUV3, g_xPassSet.TerrainMaterialConstants.g_axMaterialParams[3].z);
	float3 xEm3       = SampleEmissiveRGB(GetBindlessTexture(xMat3.g_xTexIdx1.x), xUV3, g_xPassSet.TerrainMaterialConstants.g_axEmissiveParams[3].rgb, g_xPassSet.TerrainMaterialConstants.g_axEmissiveParams[3].w);

	float4 xDiffuse = xDiffuse0 * xWeights.r + xDiffuse1 * xWeights.g
					 + xDiffuse2 * xWeights.b + xDiffuse3 * xWeights.a;
	float3 xNormalTangent = normalize(xNormalMap0 * xWeights.r + xNormalMap1 * xWeights.g
									   + xNormalMap2 * xWeights.b + xNormalMap3 * xWeights.a);

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
		return MakeGBufferVelocity(MakeGBuffer(float4(xDebugColor, 1.0), input.worldNormal, 0.2, 0.8, 0.0), xVelocity);
	}

	float3x3 TBN          = BuildTBN(input.worldNormal, input.worldTangent, input.bitangentSign);
	float3   xWorldNormal = normalize(mul(TBN, xNormalTangent));

	return MakeGBufferVelocity(
		MakeGBuffer(xDiffuse, xWorldNormal, fOcclusion, fRoughness, fMetallic,
					fSpecular, xEmissive, 0.0, 0.0, GBUFFER_SHADING_DEFAULT_LIT),
		xVelocity);
}
