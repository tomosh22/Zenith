// =====================================================================
// Flux_Quads — Slang module.
//
// Subsystem: Quads. Instanced rounded-rectangle quad renderer for the
// UI layer. Supports vertex-coloured solid fills, bindless textured
// fills with tint, vertical-gradient blending, and SDF-based rounded
// corners with sub-pixel anti-aliasing.
//
// Reads the VIEW set for g_xRcpScreenDims (the pixel-space → NDC
// conversion) via the shared Bindings spine.
//
// Bindless: the unbounded sampled-texture table lives in the BINDLESS
// set (set 2) as g_xBindlessSet.g_axTextures. a_uTexture == 0 means
// "untextured solid colour" — avoids requiring a valid array entry
// for simple UI rects.
// =====================================================================

#include "Common/Bindings.slang"

struct VsIn
{
	float3 a_xPosition           : POSITION;
	float2 a_xUV                 : TEXCOORD0;
	uint4  a_xInstancePositionSize : TEXCOORD1;
	float4 a_xInstanceColour     : TEXCOORD2;
	uint   a_uTexture            : TEXCOORD3;
	float2 a_xUVMult_UVAdd       : TEXCOORD4;
	float  a_fCornerRadius       : TEXCOORD5;
	float2 a_xSizePixels         : TEXCOORD6;
	float4 a_xColour2            : TEXCOORD7;
};

struct VsOut
{
	float4 position       : SV_Position;
	float2 uv             : TEXCOORD0;
	float4 colour         : TEXCOORD1;
	nointerpolation uint texture : TEXCOORD2;
	float  cornerRadius   : TEXCOORD3;
	float2 sizePixels     : TEXCOORD4;
	float4 colour2        : TEXCOORD5;
};

[shader("vertex")]
VsOut vsMain(VsIn input)
{
	VsOut o;

	// Vulkan's Y-down screen convention — flip V so V=0 is top of the texture.
	float2 xFlippedUV = float2(input.a_xUV.x, 1.0 - input.a_xUV.y);
	o.uv             = xFlippedUV * input.a_xUVMult_UVAdd.x + input.a_xUVMult_UVAdd.y;
	o.colour         = input.a_xInstanceColour;
	o.texture        = input.a_uTexture;
	o.cornerRadius   = input.a_fCornerRadius;
	o.sizePixels     = input.a_xSizePixels;
	o.colour2        = input.a_xColour2;

	// Quad vertices come in [-1,1]; remap to [0,1] for size scaling.
	float2 xQuadPos  = input.a_xPosition.xy * 0.5 + 0.5;
	float2 xPixelPos = xQuadPos * float2(input.a_xInstancePositionSize.zw) +
					   float2(input.a_xInstancePositionSize.xy);
	// UI quads are authored in OUTPUT pixels and drawn onto the output-res FinalRT (after any TAA
	// upscale) — map pixel->NDC with the OUTPUT rcp dims, not the (render-res) g_xRcpScreenDims.
	float2 xNDC      = xPixelPos * GetRcpOutputDims() * 2.0 - 1.0;
	o.position       = float4(xNDC, 0.0, 1.0);
	return o;
}

[shader("fragment")]
float4 fsMain(VsOut input) : SV_Target
{
	float4 xBaseColour;

	// Texture id 0 reserved as the "no texture" sentinel — UI rects don't
	// need a valid bindless slot for plain fills.
	if (input.texture == 0u)
		xBaseColour = input.colour;
	else
		xBaseColour = GetBindlessTexture(NonUniformResourceIndex(input.texture)).Sample(input.uv) * input.colour;

	// Vertical gradient when colour2 has a non-negative sentinel.
	if (input.colour2.x >= 0.0)
	{
		float4 xTop    = xBaseColour;
		float4 xBottom = input.colour2;
		if (input.texture != 0u)
			xBottom *= GetBindlessTexture(NonUniformResourceIndex(input.texture)).Sample(input.uv);
		xBaseColour = lerp(xTop, xBottom, input.uv.y);
	}

	// Rounded corner SDF: distance to a rounded rectangle in pixel space.
	if (input.cornerRadius > 0.0)
	{
		float2 xPixelPos = (input.uv - 0.5) * input.sizePixels;
		float2 xHalfSize = input.sizePixels * 0.5;
		float2 xD        = abs(xPixelPos) - (xHalfSize - input.cornerRadius);
		float  fDist     = length(max(xD, 0.0)) + min(max(xD.x, xD.y), 0.0) - input.cornerRadius;
		xBaseColour.a   *= 1.0 - smoothstep(-1.0, 0.5, fDist);
		if (xBaseColour.a < 0.01) discard;
	}

	return xBaseColour;
}
