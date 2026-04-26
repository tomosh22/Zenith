// =====================================================================
// Flux_Text — Slang module.
//
// Subsystem: Text. Instanced character glyph renderer. Each instance
// supplies a text-root pixel position, a per-character pixel offset,
// a font-atlas UV offset, and a colour tint. Glyph atlas is sampled
// with a tint × atlas-RGB multiply so white glyphs take the tint and
// pre-baked drop-shadows pass through.
//
// Imports Common.Frame for FrameConstants.g_xRcpScreenDims.
// =====================================================================

import Common.Frame;

// Character width as a fraction of height. Matches fCHAR_ASPECT_RATIO
// in Flux_Text.h — typical monospace metric.
static const float CHAR_ASPECT_RATIO = 0.5;

struct VsIn
{
	float3 a_xPosition          : POSITION;
	float2 a_xUV                : TEXCOORD0;
	float2 a_xInstancePosition  : TEXCOORD1;
	float2 a_xInstanceUV        : TEXCOORD2;
	uint2  a_xInstanceTextRoot  : TEXCOORD3;
	float  a_fInstanceTextSize  : TEXCOORD4;
	float4 a_xInstanceColour    : TEXCOORD5;
};

struct VsOut
{
	float4 position : SV_Position;
	float2 uv       : TEXCOORD0;
	float4 colour   : TEXCOORD1;
};

[shader("vertex")]
VsOut vsMain(VsIn input)
{
	VsOut o;

	// Flip V for Vulkan Y-down + 10×10 atlas grid → instance UV slot.
	o.uv     = float2(input.a_xUV.x, 1.0 - input.a_xUV.y) / 10.0 + input.a_xInstanceUV;
	o.colour = input.a_xInstanceColour;

	// Quad vertices in [-1,1] → [0,1] unit square.
	float2 xQuadPos = input.a_xPosition.xy * 0.5 + 0.5;
	xQuadPos.x *= CHAR_ASPECT_RATIO;

	// textRoot + (per-character offset + quad position) × text size.
	float2 xPixelPos = float2(input.a_xInstanceTextRoot)
					 + input.a_fInstanceTextSize * (input.a_xInstancePosition + xQuadPos);
	float2 xNDC      = xPixelPos * FrameConstants.g_xRcpScreenDims * 2.0 - 1.0;
	o.position       = float4(xNDC, 0.0, 1.0);
	return o;
}

[[vk::binding(1, 0)]] Sampler2D g_xTexture;

struct TextClipConstantsLayout
{
	// Overlay clip rect (x1, y1, x2, y2) in pixels. (-1,-1,-1,-1) → disabled.
	// Text fragments inside the rect are discarded so the overlay's
	// content box can occlude debug text behind it.
	float4 g_xClipRect;
};
[[vk::binding(2, 0)]] ConstantBuffer<TextClipConstantsLayout> TextClipConstants;

[shader("fragment")]
float4 fsMain(VsOut input, float4 fragCoord : SV_Position) : SV_Target
{
	if (TextClipConstants.g_xClipRect.x >= 0.0)
	{
		float2 xPixelPos = fragCoord.xy;
		if (xPixelPos.x >= TextClipConstants.g_xClipRect.x &&
			xPixelPos.x <= TextClipConstants.g_xClipRect.z &&
			xPixelPos.y >= TextClipConstants.g_xClipRect.y &&
			xPixelPos.y <= TextClipConstants.g_xClipRect.w) discard;
	}

	float4 xTex = g_xTexture.Sample(input.uv);
	if (xTex.a < 0.01) discard;

	// White-glyph atlas tinted by instance colour. Black drop-shadow
	// pixels (RGB ~ 0) survive the tint multiply with their alpha intact.
	return float4(xTex.rgb * input.colour.rgb, xTex.a * input.colour.a);
}
