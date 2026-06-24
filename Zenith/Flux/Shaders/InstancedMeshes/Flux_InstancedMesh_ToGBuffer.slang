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
// The instancing resources live ABOVE the 9 material-texture slots so
// they don't collide with the shared set=1 bindings 1-9. VAT animation
// texture parameters ride DrawConstants.g_xVATParams (an appended field of the
// shared MaterialDrawConstants) rather than aliasing the emissive param slot —
// emissive is now real, and a separate VAT constant buffer is impossible (one
// scratch/draw-constants buffer per descriptor set).
//
// Bindings (set=1):
//   binding 0   DrawConstants (shared MaterialDrawConstants — real emissive +
//               g_xVATParams carrying the VAT texW/texH/enable for this group)
//   binding 1-9 the 9 material textures (base/normal/RM/occlusion/emissive/
//               height/detailAlbedo/detailNormal/detailMask)
//   binding 10  TransformBuffer    (StructuredBuffer<float4x4>, written by culling)
//   binding 11  AnimDataBuffer     (StructuredBuffer<uint4>)
//   binding 12  VisibleIndexBuffer (StructuredBuffer<uint>)
//   binding 13  g_xAnimationTex    (VAT positions per frame; POINT sampler)
// =====================================================================

import Common.Frame;
import Common.DrawConstants;
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

// Material textures — the full 9-slot set, identical to the static-mesh path
// (Flux_StaticMesh_ToGBuffer.slang), so EvaluateMaterialSurface gets everything.
[[vk::binding(1, 1)]] Sampler2D g_xBaseColorTex;
[[vk::binding(2, 1)]] Sampler2D g_xNormalTex;
[[vk::binding(3, 1)]] Sampler2D g_xRoughnessMetallicTex;
[[vk::binding(4, 1)]] Sampler2D g_xOcclusionTex;
[[vk::binding(5, 1)]] Sampler2D g_xEmissiveTex;
[[vk::binding(6, 1)]] Sampler2D g_xHeightTex;
[[vk::binding(7, 1)]] Sampler2D g_xDetailAlbedoTex;
[[vk::binding(8, 1)]] Sampler2D g_xDetailNormalTex;
[[vk::binding(9, 1)]] Sampler2D g_xDetailMaskTex;

// Instancing resources at bindings >= 10 (above the material-texture slots).
// Read-only in the graphics pipeline. Without vertexPipelineStoresAndAtomics,
// vertex/tess/geom storage buffers must be NonWritable — Slang emits that
// decoration for StructuredBuffer<T> but not for RWStructuredBuffer<T>. The
// matching compute culling shader uses RWStructuredBuffer to write into the
// same physical descriptors.
//
// VAT animation texture params live in DrawConstants.g_xVATParams (a real field
// of the shared 208-byte MaterialDrawConstants — NOT a separate constant buffer:
// the backend allows only ONE scratch/draw-constants buffer per descriptor set,
// so a second BindDrawConstants on set 1 would clobber DrawConstants).
[[vk::binding(10, 1)]] StructuredBuffer<float4x4> TransformBuffer;
[[vk::binding(11, 1)]] StructuredBuffer<uint4>    AnimDataBuffer;
[[vk::binding(12, 1)]] StructuredBuffer<uint>     VisibleIndexBuffer;
[[vk::binding(13, 1)]] Sampler2D                  g_xAnimationTex;

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

	float3 xPos0 = g_xAnimationTex.SampleLevel(xUV0, 0).xyz;
	float3 xPos1 = g_xAnimationTex.SampleLevel(xUV1, 0).xyz;
	return lerp(xPos0, xPos1, fBlend);
}

[shader("vertex")]
VsOut vsMain(VsIn input, uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
	VsOut o;

	uint     uInstanceID = VisibleIndexBuffer[instanceId];
	float4x4 xModelMatrix = TransformBuffer[uInstanceID];
	uint4    xAnimData    = AnimDataBuffer[uInstanceID];
	float4   xInstanceTint = UnpackRGBA8(xAnimData.z);

	// VAT enable: global toggle (DrawConstants.g_xVATParams.z > 0) AND a per-instance
	// flag at bit 1 of xAnimData.w. Both must be true for VAT sampling.
	float3 xLocalPos = input.a_xPosition;
	bool   bUseVAT  = GetVATEnabled() && ((xAnimData.w & 2u) != 0u);
	if (bUseVAT)
	{
		// Flux_InstanceAnimData packs m_uAnimationIndex in the LOW 16 bits of
		// word 0 and m_uFrameCount in the HIGH bits (little-endian uint16 pair).
		uint  uAnimIndex  = xAnimData.x & 0xFFFFu;
		uint  uFrameCount = (xAnimData.x >> 16u) & 0xFFFFu;
		float fAnimTime   = asfloat(xAnimData.y);
		float2 xVATSize   = GetVATTextureSize();
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
	o.position     = mul(FrameConstants.g_xViewProjMat, float4(xWorldPos, 1.0));
	return o;
}

// Shared uber-material evaluation — identical to Flux_StaticMesh_ToGBuffer's fragment,
// plus the per-instance colour tint applied to the resolved albedo. The alpha cutout,
// emissive, POM, detail maps, clear coat and geometric specular-AA all come for free.
[shader("fragment")]
GBufferOut fsMain(VsOut input, bool bFrontFace : SV_IsFrontFace)
{
	float3x3 TBN = BuildTBN(input.worldNormal, input.worldTangent, input.bitangentSign);
	float3 xViewDir = normalize(FrameConstants.g_xCamPos_Pad.xyz - input.worldPos);

	MaterialSurface xSurface = EvaluateMaterialSurface(
		g_xBaseColorTex, g_xNormalTex, g_xRoughnessMetallicTex,
		g_xOcclusionTex, g_xEmissiveTex, g_xHeightTex,
		g_xDetailAlbedoTex, g_xDetailNormalTex, g_xDetailMaskTex,
		input.uv, TBN, xViewDir, input.vertexColor, bFrontFace);

	// Per-instance colour tint (RGB only — matches the previous instanced path; the
	// cutout already happened inside EvaluateMaterialSurface on the untinted alpha).
	xSurface.albedo.rgb *= input.instanceTint.rgb;

	return MakeGBuffer(xSurface);
}
