// =====================================================================
// Flux_InstancedMesh_ToGBuffer — Slang module.
//
// Subsystem: InstancedMeshes. GPU-instanced static meshes with
// Vertex Animation Texture (VAT) support and per-instance colour tint.
// Per-instance transform comes from a storage buffer indexed by an
// indirection table (visible-instance compact indices), letting the
// culling pass cheaply skip culled instances.
//
// DrawConstants.g_xEmissiveParams is repurposed as VAT animation
// texture parameters (texture width, height, vat-enabled flag, unused);
// the C++ caller writes the appropriate flavour. See Common.DrawConstants.
//
// Bindings:
//   set=1 binding=0  DrawConstants
//   set=1 binding=1  g_xDiffuseTex
//   set=1 binding=2  g_xNormalTex
//   set=1 binding=3  g_xRoughnessMetallicTex
//   set=1 binding=4  g_xOcclusionTex
//   set=1 binding=5  g_xEmissiveTex
//   set=1 binding=6  TransformBuffer (RWStructuredBuffer, written by culling)
//   set=1 binding=7  AnimDataBuffer  (RWStructuredBuffer)
//   set=1 binding=8  VisibleIndexBuffer (RWStructuredBuffer<uint>)
//   set=1 binding=9  g_xAnimationTex (VAT positions per frame)
// =====================================================================

import Common.Frame;
import Common.DrawConstants;
import Common.Material;
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

[[vk::binding(1, 1)]] Sampler2D g_xDiffuseTex;
[[vk::binding(2, 1)]] Sampler2D g_xNormalTex;
[[vk::binding(3, 1)]] Sampler2D g_xRoughnessMetallicTex;
[[vk::binding(4, 1)]] Sampler2D g_xOcclusionTex;
[[vk::binding(5, 1)]] Sampler2D g_xEmissiveTex;

// Read-only in the graphics pipeline. Without
// vertexPipelineStoresAndAtomics, vertex/tess/geom storage buffers must
// be NonWritable — Slang emits that decoration for StructuredBuffer<T>
// but not for RWStructuredBuffer<T>. The matching compute culling shader
// uses RWStructuredBuffer to write into the same physical descriptors.
[[vk::binding(6, 1)]] StructuredBuffer<float4x4> TransformBuffer;
[[vk::binding(7, 1)]] StructuredBuffer<uint4>    AnimDataBuffer;
[[vk::binding(8, 1)]] StructuredBuffer<uint>     VisibleIndexBuffer;
[[vk::binding(9, 1)]] Sampler2D                    g_xAnimationTex;

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
// and next for smooth animation.
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

	// VAT enable: high bit of g_xAnimTexParams.z (texture width) doubles
	// as a global VAT-enabled toggle, plus a per-instance flag at bit 1
	// of xAnimData.w. Both must be true for VAT sampling.
	float3 xLocalPos = input.a_xPosition;
	bool   bUseVAT  = (DrawConstants.g_xEmissiveParams.z > 0.0) && ((xAnimData.w & 2u) != 0u);
	if (bUseVAT)
	{
		// Flux_InstanceAnimData packs m_uAnimationIndex in the LOW 16 bits of
		// word 0 and m_uFrameCount in the HIGH bits (little-endian uint16
		// pair). Decoding them swapped made uFrameCount read 0, so the
		// fAnimTime * (uFrameCount - 1u) row computation underflowed and every
		// instance clamp-sampled one frozen row — VAT animation never played.
		uint  uAnimIndex  = xAnimData.x & 0xFFFFu;
		uint  uFrameCount = (xAnimData.x >> 16u) & 0xFFFFu;
		float fAnimTime   = asfloat(xAnimData.y);
		uint  uTexWidth   = (uint)DrawConstants.g_xEmissiveParams.x;
		uint  uTexHeight  = (uint)DrawConstants.g_xEmissiveParams.y;
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

[shader("fragment")]
GBufferOut fsMain(VsOut input)
{
	float2 xUV = TransformUV(input.uv, GetUVTiling(), GetUVOffset());

	float4 xDiffuse = SampleDiffuseWithBaseColor(g_xDiffuseTex, xUV, DrawConstants.g_xBaseColor);
	xDiffuse.rgb   *= input.instanceTint.rgb; // per-instance colour tint
	if (input.vertexColor.a > 0.0) xDiffuse *= input.vertexColor;
	if (xDiffuse.a < GetAlphaCutoff()) discard;

	float3x3 TBN          = BuildTBN(input.worldNormal, input.worldTangent, input.bitangentSign);
	float3   xWorldNormal = SampleNormalMap(g_xNormalTex, xUV, TBN);

	float fRoughness, fMetallic;
	SampleRoughnessMetallic(g_xRoughnessMetallicTex, xUV,
							 GetRoughnessMultiplier(), GetMetallicMultiplier(),
							 fRoughness, fMetallic);
	float fOcclusion = SampleOcclusion(g_xOcclusionTex, xUV, GetOcclusionStrength());
	float fEmissive  = CalculateEmissiveLuminance(g_xEmissiveTex, xUV,
													GetEmissiveColor(), GetEmissiveIntensity());

	return MakeGBuffer(xDiffuse, xWorldNormal, fOcclusion, fRoughness, fMetallic, fEmissive);
}
