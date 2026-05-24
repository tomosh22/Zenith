// =====================================================================
// Flux_Text - MSDF text shader.
//
// Per-glyph instanced renderer with screen-space-derivative AA. Sharp at
// any render size from a single 256x256 atlas baked offline by
// Tools/Zenith_Tools_FontExport via msdf-atlas-gen.
//
// Vertex inputs (per-instance) come from Flux_TextVertex in Flux_TextImpl.h.
// Push-constant layout below MUST match Flux_TextDrawConstants on the host
// side (static_asserted to 32 bytes).
// =====================================================================

import Common.Frame;

struct VsIn
{
	float3 a_xPosition       : POSITION;     // unit quad [-1,1]
	float2 a_xUV             : TEXCOORD0;    // unit quad UV [0,1]
	float2 a_xQuadOffsetPx   : TEXCOORD1;    // glyph top-left, relative to text root
	float2 a_xQuadSizePx     : TEXCOORD2;    // glyph quad size in pixels
	float2 a_xAtlasUVOrigin  : TEXCOORD3;    // glyph's UV rect origin in atlas
	float2 a_xAtlasUVSize    : TEXCOORD4;    // glyph's UV rect size in atlas
	float2 a_xTextRootPx     : TEXCOORD5;    // float2 — subpixel positions OK
	float4 a_xColour         : TEXCOORD6;
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
	float2 xQuadXY = input.a_xPosition.xy * 0.5 + 0.5;       // [0,1]
	float2 xPxPos  = input.a_xTextRootPx + input.a_xQuadOffsetPx
				   + xQuadXY * input.a_xQuadSizePx;

	VsOut o;
	o.position = float4(xPxPos * FrameConstants.g_xRcpScreenDims * 2.0 - 1.0, 0.0, 1.0);
	o.uv       = input.a_xAtlasUVOrigin + xQuadXY * input.a_xAtlasUVSize;
	o.colour   = input.a_xColour;
	return o;
}

[[vk::binding(1, 0)]] Sampler2D g_xTexture;

struct TextConstantsLayout
{
	// Overlay clip rect (x1, y1, x2, y2) in pixels. (-1,-1,-1,-1) → disabled.
	// Fragments inside the rect are discarded so an overlay panel can occlude
	// debug text behind it. (Inverted from a scissor — discard if INSIDE.)
	float4 g_xClipRect;
	// Atlas size and msdfgen "range" — feed the fwidth-based AA helper below.
	float2 g_xAtlasSizePx;
	float  g_fAtlasPxRange;
	float  g_fPad;
};
[[vk::binding(2, 0)]] ConstantBuffer<TextConstantsLayout> TextConstants;

float Median3(float a, float b, float c)
{
	return max(min(a, b), min(max(a, b), c));
}

// Canonical msdfgen recipe: convert the atlas-space pxRange into screen pixels
// using fwidth of the UV. Clamp to >=1 to keep AA stable at extreme minification.
// Without this, glyph weight changes with render size.
float ScreenPxRange(float2 uv)
{
	float2 unitRange     = float2(TextConstants.g_fAtlasPxRange, TextConstants.g_fAtlasPxRange) / TextConstants.g_xAtlasSizePx;
	float2 screenTexSize = 1.0 / fwidth(uv);
	return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

[shader("fragment")]
float4 fsMain(VsOut input, float4 fragCoord : SV_Position) : SV_Target
{
	// Inverted clip-rect (discard if INSIDE): matches legacy Flux_Text behaviour.
	if (TextConstants.g_xClipRect.x >= 0.0)
	{
		float2 p = fragCoord.xy;
		if (p.x >= TextConstants.g_xClipRect.x && p.x <= TextConstants.g_xClipRect.z &&
			p.y >= TextConstants.g_xClipRect.y && p.y <= TextConstants.g_xClipRect.w)
		{
			discard;
		}
	}

	float3 xMSD          = g_xTexture.Sample(input.uv).rgb;
	float  fSD           = Median3(xMSD.r, xMSD.g, xMSD.b);
	float  fScreenPxDist = ScreenPxRange(input.uv) * (fSD - 0.5);
	float  fAlpha        = saturate(fScreenPxDist + 0.5);

	// Standard alpha blending handles fAlpha = 0 cleanly. No hard discard at
	// low alpha — that's what made the legacy bitmap shader's edges jagged.
	return float4(input.colour.rgb, fAlpha * input.colour.a);
}
