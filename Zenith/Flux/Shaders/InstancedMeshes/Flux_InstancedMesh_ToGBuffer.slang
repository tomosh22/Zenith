// =====================================================================
// Flux_InstancedMesh_ToGBuffer — Slang module.
//
// Subsystem: InstancedMeshes. GPU-instanced static meshes with
// Vertex Animation Texture (VAT) support and per-instance colour tint.
// Per-instance transform comes from a storage buffer indexed by an
// indirection table (visible-instance compact indices), letting the
// culling pass cheaply skip culled instances.
//
// Material evaluation is the SHARED uber-material path: the fragment
// shader calls Common.MaterialSurface::EvaluateMaterialSurface exactly
// like StaticMeshes/AnimatedMeshes — full feature set (POM, detail maps,
// emissive RGB, clear coat, the single authoritative alpha cutout). The
// only instanced-specific work is the VAT vertex deformation and the
// per-instance colour tint applied to the resolved albedo.
//
// Material textures are bindless (set 2): the fragment loads its MaterialGPU
// record from g_axMaterials and samples g_xBindlessSet.g_axTextures[...]. The DRAW
// block (set 3) holds the per-draw constants + the instancing resources. VAT
// animation texture parameters ride DrawConstants.g_xVATParams (a field of the
// shared MeshDrawConstants) — a separate VAT constant buffer is impossible (one
// scratch/draw-constants buffer per descriptor set).
//
// DRAW block (set 3) bindings:
//   binding 0   DrawConstants (model matrix + material-table index + g_xVATParams)
//   binding 1   TransformBuffer    (StructuredBuffer<float4x4>, written by culling)
//   binding 2   AnimDataBuffer     (StructuredBuffer<uint4>)
//   binding 3   VisibleIndexBuffer (StructuredBuffer<uint>)
//   binding 4   g_xAnimationTex    (VAT positions per frame; POINT sampler)
// =====================================================================

#include "Common/Bindings.slang"
import Common.DrawConstants;
import Common.MaterialTable;
import Common.Material;
import Common.MaterialSurface;
import Common.GBuffer;

struct VsIn
{
	float3 a_xPosition  : POSITION;
	float2 a_xUV        : TEXCOORD0;
	float3 a_xNormal    : NORMAL;
	float3 a_xTangent   : TANGENT;
	float3 a_xBitangent : BINORMAL;
	float4 a_xColor     : COLOR;
};

struct VsOut
{
	float4 position      : SV_Position;
	float2 uv            : TEXCOORD0;
	float3 worldNormal   : TEXCOORD1;
	float3 worldPos      : TEXCOORD2;
	float3 worldTangent  : TEXCOORD3;
	float  bitangentSign : TEXCOORD4;
	float4 vertexColor   : TEXCOORD5;
	float4 instanceTint  : TEXCOORD6;
};

// DRAW block (set 3) — per-draw constants (model is per-instance from
// TransformBuffer; this CB carries the material-table index + VAT params) + the
// instancing resources + the VAT animation texture. Material textures are bindless
// (set 2), resolved in fsMain from the material record.
//
// Instancing buffers are read-only in the graphics pipeline. Without
// vertexPipelineStoresAndAtomics, vertex/tess/geom storage buffers must be
// NonWritable — Slang emits that decoration for StructuredBuffer<T> but not for
// RWStructuredBuffer<T>. The matching compute culling shader uses RWStructuredBuffer
// to write into the same physical descriptors.
//
// VAT animation texture params live in DrawConstants.g_xVATParams (a real field
// of the shared MeshDrawConstants — NOT a separate constant buffer: the backend
// allows only ONE scratch/draw-constants buffer per descriptor set, so a second
// BindDrawConstants on this set would clobber DrawConstants).
struct DrawParams
{
	ConstantBuffer<DrawConstantsLayout> DrawConstants;
	StructuredBuffer<float4x4> TransformBuffer;
	StructuredBuffer<uint4>    AnimDataBuffer;
	StructuredBuffer<uint>     VisibleIndexBuffer;
	Sampler2D                  g_xAnimationTex;
};
ParameterBlock<DrawParams> g_xDrawSet;

float4 UnpackRGBA8(uint uPacked)
{
	return float4(
		(float)((uPacked >> 0)  & 0xFFu) / 255.0,
		(float)((uPacked >> 8)  & 0xFFu) / 255.0,
		(float)((uPacked >> 16) & 0xFFu) / 255.0,
		(float)((uPacked >> 24) & 0xFFu) / 255.0);
}

// VAT lookup: positions baked into a 2D texture (X=vertex id,
// Y=animIndex*frameCount + frame). Two-tap LERP between current frame
// and next for smooth animation. MUST stay bit-for-bit identical to the
// shadow shader's SampleVAT so the shadow vertex lands at the same world
// position as the G-buffer vertex (a mismatch reintroduces tree flicker).
float3 SampleVAT(uint uVertexID, uint uAnimIndex, float fAnimTime,
				   uint uFrameCount, uint uTextureWidth, uint uTextureHeight)
{
	float fFrame = fAnimTime * (float)(uFrameCount - 1u);
	uint  uFrame = (uint)fFrame;
	float fBlend = frac(fFrame);

	uFrame          = min(uFrame, uFrameCount - 1u);
	uint uNextFrame = min(uFrame + 1u, uFrameCount - 1u);

	uint uRow0 = uAnimIndex * uFrameCount + uFrame;
	uint uRow1 = uAnimIndex * uFrameCount + uNextFrame;

	float2 xUV0 = float2(((float)uVertexID + 0.5) / (float)uTextureWidth,
						   ((float)uRow0     + 0.5) / (float)uTextureHeight);
	float2 xUV1 = float2(((float)uVertexID + 0.5) / (float)uTextureWidth,
						   ((float)uRow1     + 0.5) / (float)uTextureHeight);

	float3 xPos0 = g_xDrawSet.g_xAnimationTex.SampleLevel(xUV0, 0).xyz;
	float3 xPos1 = g_xDrawSet.g_xAnimationTex.SampleLevel(xUV1, 0).xyz;
	return lerp(xPos0, xPos1, fBlend);
}

[shader("vertex")]
VsOut vsMain(VsIn input, uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
	VsOut o;

	DrawConstantsLayout dc = g_xDrawSet.DrawConstants;

	uint     uInstanceID = g_xDrawSet.VisibleIndexBuffer[instanceId];
	float4x4 xModelMatrix = g_xDrawSet.TransformBuffer[uInstanceID];
	uint4    xAnimData    = g_xDrawSet.AnimDataBuffer[uInstanceID];
	float4   xInstanceTint = UnpackRGBA8(xAnimData.z);

	// VAT enable: global toggle (DrawConstants.g_xVATParams.z > 0) AND a per-instance
	// flag at bit 1 of xAnimData.w. Both must be true for VAT sampling.
	float3 xLocalPos = input.a_xPosition;
	bool   bUseVAT  = GetVATEnabled(dc) && ((xAnimData.w & 2u) != 0u);
	if (bUseVAT)
	{
		// Flux_InstanceAnimData packs m_uAnimationIndex in the LOW 16 bits of
		// word 0 and m_uFrameCount in the HIGH bits (little-endian uint16 pair).
		uint  uAnimIndex  = xAnimData.x & 0xFFFFu;
		uint  uFrameCount = (xAnimData.x >> 16u) & 0xFFFFu;
		float fAnimTime   = asfloat(xAnimData.y);
		float2 xVATSize   = GetVATTextureSize(dc);
		uint  uTexWidth   = (uint)xVATSize.x;
		uint  uTexHeight  = (uint)xVATSize.y;
		xLocalPos = SampleVAT(vertexId, uAnimIndex, fAnimTime,
								uFrameCount, uTexWidth, uTexHeight);
	}

	float3   xWorldPos     = mul(xModelMatrix, float4(xLocalPos, 1.0)).xyz;
	float3x3 xNormalMatrix = transpose(Inverse3x3((float3x3)xModelMatrix));

	o.uv          = input.a_xUV;
	o.vertexColor = input.a_xColor;
	o.worldNormal = normalize(mul(xNormalMatrix, input.a_xNormal));
	o.worldTangent = normalize(mul(xNormalMatrix, input.a_xTangent));
	float3 xBitangent = normalize(mul(xNormalMatrix, input.a_xBitangent));
	o.bitangentSign = sign(dot(xBitangent, cross(o.worldNormal, o.worldTangent)));
	o.instanceTint = xInstanceTint;
	o.worldPos     = xWorldPos;
	o.position     = mul(g_xViewSet.g_xView.g_xViewProjMat, float4(xWorldPos, 1.0));
	return o;
}

// Shared uber-material evaluation — identical to Flux_StaticMesh_ToGBuffer's fragment,
// plus the per-instance colour tint applied to the resolved albedo. The alpha cutout,
// emissive, POM, detail maps, clear coat and geometric specular-AA all come for free.
[shader("fragment")]
GBufferOut fsMain(VsOut input, bool bFrontFace : SV_IsFrontFace)
{
	float3x3 TBN = BuildTBN(input.worldNormal, input.worldTangent, input.bitangentSign);
	float3 xViewDir = normalize(g_xViewSet.g_xView.g_xCamPos_Pad.xyz - input.worldPos);

	DrawConstantsLayout dc = g_xDrawSet.DrawConstants;
	MaterialGPU m = g_xGlobalSet.g_axMaterials[dc.g_uMaterialIndex];

	// Bindless material textures (draw-uniform index → no NonUniformResourceIndex).
	Sampler2D xBaseColorTex         = g_xBindlessSet.g_axTextures[m.g_xTexIdx0.x];
	Sampler2D xNormalTex            = g_xBindlessSet.g_axTextures[m.g_xTexIdx0.y];
	Sampler2D xRoughnessMetallicTex = g_xBindlessSet.g_axTextures[m.g_xTexIdx0.z];
	Sampler2D xOcclusionTex         = g_xBindlessSet.g_axTextures[m.g_xTexIdx0.w];
	Sampler2D xEmissiveTex          = g_xBindlessSet.g_axTextures[m.g_xTexIdx1.x];
	Sampler2D xHeightTex            = g_xBindlessSet.g_axTextures[m.g_xTexIdx1.y];
	Sampler2D xDetailAlbedoTex      = g_xBindlessSet.g_axTextures[m.g_xTexIdx1.z];
	Sampler2D xDetailNormalTex      = g_xBindlessSet.g_axTextures[m.g_xTexIdx1.w];
	Sampler2D xDetailMaskTex        = g_xBindlessSet.g_axTextures[m.g_xTexIdx2.x];

	MaterialSurface xSurface = EvaluateMaterialSurface(
		xBaseColorTex, xNormalTex, xRoughnessMetallicTex,
		xOcclusionTex, xEmissiveTex, xHeightTex,
		xDetailAlbedoTex, xDetailNormalTex, xDetailMaskTex,
		input.uv, TBN, xViewDir, input.vertexColor, bFrontFace, m);

	// Per-instance colour tint (RGB only — matches the previous instanced path; the
	// cutout already happened inside EvaluateMaterialSurface on the untinted alpha).
	xSurface.albedo.rgb *= input.instanceTint.rgb;

	return MakeGBuffer(xSurface);
}
