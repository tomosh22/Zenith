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

#include "Common/Bindings.slang"

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
	o.position = float4(xPxPos * g_xViewSet.g_xView.g_xRcpScreenDims * 2.0 - 1.0, 0.0, 1.0);
	o.uv       = input.a_xAtlasUVOrigin + xQuadXY * input.a_xAtlasUVSize;
	o.colour   = input.a_xColour;
	return o;
}

struct TextConstantsLayout
{
	// Overlay clip rect (x1, y1, x2, y2) in pixels. (-1,-1,-1,-1) → disabled.
	// Fragments inside the rect are discarded so an overlay panel can occlude
	// debug text behind it. (Inverted from a scissor — discard if INSIDE.)
	float4 g_xClipRect;
	// Atlas size and msdfgen "range" — feed the fwidth-based AA helper below.
	float2 g_xAtlasSizePx;
	float  g_fAtlasPxRange;
	// Bindless slot of the MSDF atlas in the BINDLESS set's g_axTextures[].
	// Pass-uniform (one atlas per frame) → no NonUniformResourceIndex.
	uint   g_uAtlasIdx;
};

// PASS frequency. Declared AFTER the spine #include, so it lands at set 3
// (the first set past the persistent 0/1/2). Member names preserved exactly —
// the C++ binds key on them (h<member>). The atlas itself now lives in the
// bindless table (set 2 g_xBindlessSet.g_axTextures), indexed by g_uAtlasIdx.
struct PassParams
{
	ConstantBuffer<TextConstantsLayout> TextConstants;
};
ParameterBlock<PassParams> g_xPassSet;

float Median3(float a, float b, float c)
{
	return max(min(a, b), min(max(a, b), c));
}

// Canonical msdfgen recipe: convert the atlas-space pxRange into screen pixels
// using fwidth of the UV. Clamp to >=1 to keep AA stable at extreme minification.
// Without this, glyph weight changes with render size.
float ScreenPxRange(float2 uv)
{
	float2 unitRange     = float2(g_xPassSet.TextConstants.g_fAtlasPxRange, g_xPassSet.TextConstants.g_fAtlasPxRange) / g_xPassSet.TextConstants.g_xAtlasSizePx;
	float2 screenTexSize = 1.0 / fwidth(uv);
	return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

[shader("fragment")]
float4 fsMain(VsOut input, float4 fragCoord : SV_Position) : SV_Target
{
	// Inverted clip-rect (discard if INSIDE): matches legacy Flux_Text behaviour.
	if (g_xPassSet.TextConstants.g_xClipRect.x >= 0.0)
	{
		float2 p = fragCoord.xy;
		if (p.x >= g_xPassSet.TextConstants.g_xClipRect.x && p.x <= g_xPassSet.TextConstants.g_xClipRect.z &&
			p.y >= g_xPassSet.TextConstants.g_xClipRect.y && p.y <= g_xPassSet.TextConstants.g_xClipRect.w)
		{
			discard;
		}
	}

	float  fScreenPxRange = ScreenPxRange(input.uv);
	float3 xMSD           = g_xBindlessSet.g_axTextures[g_xPassSet.TextConstants.g_uAtlasIdx].Sample(input.uv).rgb;
	float  fSD            = Median3(xMSD.r, xMSD.g, xMSD.b);
	float  fScreenPxDist  = fScreenPxRange * (fSD - 0.5);

	// 1-pixel black halo around every glyph for legibility on any background.
	// fInkAlpha covers the glyph ink; fOutlineAlpha extends the ink mask by
	// `fHaloWidthPx` screen pixels via the SDF (no second texture sample).
	// We then lerp the ink colour to black inside the halo ring, and use the
	// wider mask for the output alpha — so the halo fades to transparent at
	// its outer edge with the same 1-px AA as the ink edge.
	//
	// fHaloWidthPx is capped by what the SDF can represent at the current
	// minification. The SDF stores at most ~ScreenPxRange/2 screen pixels of
	// "outside" distance (since fSD saturates to 0 beyond the bake-time
	// pxRange/2 atlas pixels). At small render sizes where ScreenPxRange
	// clamps to 1, the SDF can't see far enough outside to fade a 1-px halo
	// correctly — without this cap, `saturate(fScreenPxDist + 1.5)` would
	// fill the entire quad with halo alpha, producing a grey rectangle behind
	// the glyph. We linearly ramp the halo width from 0 at ScreenPxRange=1
	// to a full 1 px at ScreenPxRange=3, then cap.
	const float fTargetHaloPx = 1.0;
	float  fHaloWidthPx  = clamp(0.5 * (fScreenPxRange - 1.0), 0.0, fTargetHaloPx);
	float  fInkAlpha     = saturate(fScreenPxDist + 0.5);
	float  fOutlineAlpha = saturate(fScreenPxDist + 0.5 + fHaloWidthPx);
	float3 xRgb          = lerp(float3(0.0, 0.0, 0.0), input.colour.rgb, fInkAlpha);
	return float4(xRgb, fOutlineAlpha * input.colour.a);
}
