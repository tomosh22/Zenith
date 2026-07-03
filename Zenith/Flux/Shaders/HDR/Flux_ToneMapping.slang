// =====================================================================
// Flux_ToneMapping — Slang module.
//
// Subsystem: HDR. Final HDR-to-LDR composite. Reads the HDR scene + the
// blurred bloom mip, applies (auto-)exposure, runs one of five tone
// curves (ACES filmic / ACES fitted / Reinhard / Uncharted 2 / Neutral),
// and writes the LDR result. Also supports a battery of debug overlays
// (luminance heatmap, histogram, exposure meter, EV zones, clipping).
//
// Bindings:
//   ToneMappingConstants (CB)            set=0 binding=0
//   g_xHDRTex            (Sampler2D)     set=0 binding=1
//   g_xBloomTex          (Sampler2D)     set=0 binding=2
//   g_auHistogram        (StructuredBuffer<uint>)  set=0 binding=3
//   g_afExposureData     (StructuredBuffer<float>) set=0 binding=4
// =====================================================================

#include "Common/Bindings.slang"   // spine: GLOBAL/VIEW/BINDLESS at sets 0/1/2
import Common.Fullscreen;

struct ToneMappingConstantsLayout
{
	float g_fExposure;
	float g_fBloomIntensity;
	uint  g_uToneMappingOperator;
	uint  g_uDebugMode;
	uint  g_bShowHistogram;
	uint  g_bAutoExposure;
	uint  g_uPad0;
	uint  g_uPad1;
};
// Pure fullscreen composite on its own HDR/bloom textures + exposure/histogram
// buffers — no FrameConstants. The spine include still reserves sets 0/1/2 so
// this PassParams block lands at set 3.
struct PassParams
{
	ConstantBuffer<ToneMappingConstantsLayout> ToneMappingConstants;
	Sampler2D                 g_xHDRTex;
	Sampler2D                 g_xBloomTex;
	StructuredBuffer<uint>    g_auHistogram;
	StructuredBuffer<float>   g_afExposureData;
};
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

// ACES Filmic Tone Mapping (Krzysztof Narkowicz approximation of RRT+ODT).
float3 ACESFilmicToneMapping(float3 x)
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// ACES Fitted (more accurate fit of the actual ACES curve). HLSL is
// row-major by default, so the matrices below use HLSL row-vector
// convention (mul(row, M)) — note: this is the GLSL value layout
// transposed.
float3 ACESFittedToneMapping(float3 v)
{
	// Constants from Stephen Hill's ACES fit; layout matches mul(v, M).
	float3x3 ACES_INPUT_MAT = float3x3(
		0.59719, 0.35458, 0.04823,
		0.07600, 0.90834, 0.01566,
		0.02840, 0.13383, 0.83777);
	float3x3 ACES_OUTPUT_MAT = float3x3(
		1.60475, -0.53108, -0.07367,
	   -0.10208,  1.10813, -0.00605,
	   -0.00327, -0.07276,  1.07602);

	v = mul(v, ACES_INPUT_MAT);
	float3 a = v * (v + 0.0245786) - 0.000090537;
	float3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
	v = a / b;
	return saturate(mul(v, ACES_OUTPUT_MAT));
}

float3 ReinhardToneMapping(float3 colour)
{
	return colour / (colour + float3(1.0));
}

float3 Uncharted2Tonemap(float3 x)
{
	const float A = 0.15;
	const float B = 0.50;
	const float C = 0.10;
	const float D = 0.20;
	const float E = 0.02;
	const float F = 0.30;
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float3 Uncharted2ToneMapping(float3 colour)
{
	const float W = 11.2;
	const float ExposureBias = 2.0;
	float3 curr       = Uncharted2Tonemap(ExposureBias * colour);
	float3 whiteScale = float3(1.0) / Uncharted2Tonemap(float3(W, W, W));
	return curr * whiteScale;
}

float3 NeutralToneMapping(float3 colour)
{
	const float startCompression = 0.8 - 0.04;
	const float desaturation     = 0.15;

	float x      = min(colour.r, min(colour.g, colour.b));
	float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
	colour      -= offset;

	float peak = max(colour.r, max(colour.g, colour.b));
	if (peak < startCompression) return saturate(colour);

	peak = max(peak, 0.0001);
	float d           = 1.0 - startCompression;
	float denominator = max(peak + d - startCompression, 0.0001);
	float newPeak     = saturate(1.0 - d * d / denominator);
	colour           *= newPeak / peak;

	float g = 1.0 - 1.0 / (desaturation * max(peak - newPeak, 0.0) + 1.0);
	return saturate(lerp(colour, float3(newPeak, newPeak, newPeak), g));
}

// ---------- AgX (Troy Sobotka / Benjamin Wrensch "Minimal AgX") ----------
// Modern filmic transform and the new photoreal default. Versus ACES-Narkowicz
// it keeps hue stable across the exposure range (no blue->cyan / skin->orange
// skew) and desaturates highlights gracefully toward white, reading as
// photographic film rather than CG. The sigmoid runs in a log2-encoded space;
// the final pow(2.2) returns ~linear so the sRGB swapchain applies the true
// OETF on write (no double-encode). Matrices are filled row-major and used with
// mul(M, v) — mathematically M*v — matching the engine's mul(invProj, clip).
float3 AgX_DefaultContrastApprox(float3 x)
{
	float3 x2 = x * x;
	float3 x4 = x2 * x2;
	return 15.5    * x4 * x2
		 - 40.14   * x4 * x
		 + 31.96   * x4
		 - 6.868   * x2 * x
		 + 0.4298  * x2
		 + 0.1191  * x
		 - 0.00232;
}

float3 AgX_LookPunchy(float3 val)
{
	const float3 lw = float3(0.2126, 0.7152, 0.0722);
	float luma = dot(val, lw);
	// Gentle default grade: a touch of contrast (power) + saturation. Kept
	// subtle so the look stays neutral/photographic, not stylised.
	const float3 power = float3(1.05, 1.05, 1.05);
	const float  sat   = 1.10;
	val = pow(max(val, float3(0.0, 0.0, 0.0)), power);
	return luma + sat * (val - luma);
}

float3 AgXToneMapping(float3 colour)
{
	const float3x3 agx_mat = float3x3(
		0.842479062253094, 0.0423282422610123, 0.0423756549057051,
		0.0784335999999992, 0.878468636469772, 0.0784336,
		0.0792237451477643, 0.0791661274605434, 0.879142973793104);
	const float3x3 agx_mat_inv = float3x3(
		 1.19687900512017,  -0.0528968517574562, -0.0529716355144438,
		-0.0980208811401368, 1.15190312990417,   -0.0980434501171241,
		-0.0990297440797205,-0.0989611768448433,  1.15107367264116);
	const float min_ev = -12.47393;
	const float max_ev = 4.026069;

	colour = mul(agx_mat, colour);
	colour = clamp(log2(max(colour, float3(1e-10, 1e-10, 1e-10))), min_ev, max_ev);
	colour = (colour - min_ev) / (max_ev - min_ev);
	colour = AgX_DefaultContrastApprox(colour);
	colour = AgX_LookPunchy(colour);
	colour = mul(agx_mat_inv, colour);
	colour = pow(max(colour, float3(0.0, 0.0, 0.0)), float3(2.2, 2.2, 2.2));
	return colour;
}

float3 ApplyToneMapping(float3 xHDR, uint uOperator)
{
	switch (uOperator)
	{
		case 0: return ACESFilmicToneMapping(xHDR);
		case 1: return ACESFittedToneMapping(xHDR);
		case 2: return ReinhardToneMapping(xHDR);
		case 3: return Uncharted2ToneMapping(xHDR);
		case 4: return NeutralToneMapping(xHDR);
		case 5: return AgXToneMapping(xHDR);
		default: return AgXToneMapping(xHDR);
	}
}

// ---------- Output dithering (anti-banding) ----------
// Approximate sRGB OETF/EOTF so the triangular dither lands in the perceptual
// domain where the 8-bit quantization actually happens (the swapchain is sRGB).
// Gamma-2.2 approximation of the sRGB transfer (component-wise, no vector
// ternary). Exact sRGB is unnecessary here — the dither only needs roughly
// perceptual spacing so ~1 LSB lands evenly across the tonal range.
float3 LinearToSRGBApprox(float3 c)
{
	return pow(saturate(c), float3(1.0/2.2, 1.0/2.2, 1.0/2.2));
}
float3 SRGBToLinearApprox(float3 c)
{
	return pow(max(c, float3(0.0, 0.0, 0.0)), float3(2.2, 2.2, 2.2));
}
float Hash12(float2 p)
{
	float3 p3 = frac(float3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return frac((p3.x + p3.y) * p3.z);
}
// TPDF (triangular) dither of ~1 LSB, applied in sRGB space then converted back
// to linear so the sRGB swapchain re-encodes correctly. Breaks up Mach banding
// in skies / volumetrics / gradients before the 8-bit write.
float3 ApplyOutputDither(float3 xLinear, float2 xPixel)
{
	float3 xS = LinearToSRGBApprox(xLinear);
	float r0 = Hash12(xPixel);
	float r1 = Hash12(xPixel + float2(17.0, 23.0));
	float r2 = Hash12(xPixel + float2(101.0, 53.0));
	float3 xTri = float3(r0, r1, r2) + float3(Hash12(xPixel + 7.0), Hash12(xPixel + 41.0), Hash12(xPixel + 71.0)) - 1.0;
	xS += xTri * (1.0 / 255.0);
	return SRGBToLinearApprox(xS);
}

float Luminance(float3 colour)
{
	return dot(colour, float3(0.2126, 0.7152, 0.0722));
}

float3 DebugHeatmap(float fValue)
{
	fValue = saturate(fValue);
	if (fValue < 0.5) return lerp(float3(0, 0, 1), float3(0, 1, 0), fValue * 2.0);
	return lerp(float3(0, 1, 0), float3(1, 0, 0), (fValue - 0.5) * 2.0);
}

bool RenderHistogramOverlay(float2 xUV, out float3 xOverlayColour)
{
	const float fHistWidth  = 0.3;
	const float fHistHeight = 0.15;
	const float fMargin     = 0.02;

	float fYBottom = 1.0 - fMargin;
	float fYTop    = fYBottom - fHistHeight;
	if (xUV.x < fMargin || xUV.x > fMargin + fHistWidth ||
		xUV.y < fYTop || xUV.y > fYBottom)
	{
		xOverlayColour = float3(0.0);
		return false;
	}

	float fHistX = (xUV.x - fMargin) / fHistWidth;
	float fHistY = (fYBottom - xUV.y) / fHistHeight;

	uint uBin       = min((uint)(fHistX * 256.0), 255u);
	uint uCount     = g_xPassSet.g_auHistogram[uBin];
	uint uMaxCount  = max(1u, (uint)g_xPassSet.g_afExposureData[3]);

	float fNormHeight = log((float)uCount + 1.0) / log((float)uMaxCount + 1.0);

	if (fHistY < fNormHeight)
	{
		float fBinNorm = (float)uBin / 255.0;
		float fGray    = lerp(0.15, 0.95, fBinNorm);
		xOverlayColour = float3(fGray);
		return true;
	}
	xOverlayColour = float3(0.1);
	return true;
}

bool RenderExposureMeter(float2 xUV, out float3 xOverlayColour)
{
	const float fMeterWidth  = 0.12;
	const float fMeterHeight = 0.08;
	const float fMargin      = 0.02;
	float fXLeft   = 1.0 - fMargin - fMeterWidth;
	float fXRight  = 1.0 - fMargin;
	float fYTop    = fMargin;
	float fYBottom = fMargin + fMeterHeight;
	if (xUV.x < fXLeft || xUV.x > fXRight || xUV.y < fYTop || xUV.y > fYBottom)
	{
		xOverlayColour = float3(0.0);
		return false;
	}

	float fMeterX = (xUV.x - fXLeft) / fMeterWidth;
	float fMeterY = (xUV.y - fYTop) / fMeterHeight;

	xOverlayColour = float3(0.1);
	float fAvgLum     = g_xPassSet.g_afExposureData[0];
	float fCurrentExp = g_xPassSet.g_afExposureData[1];
	float fTargetExp  = g_xPassSet.g_afExposureData[2];

	const float fBarHeight = 0.25;
	const float fBarGap    = 0.08;

	float fBar1Top = 0.1;
	float fBar1Bot = fBar1Top + fBarHeight;
	if (fMeterY > fBar1Top && fMeterY < fBar1Bot)
	{
		float fLumNorm = saturate((log(fAvgLum + 0.0001) + 7.0) / 12.0);
		if (fMeterX < fLumNorm) xOverlayColour = float3(0.0, 0.8, 0.8);
		return true;
	}
	float fBar2Top = fBar1Bot + fBarGap;
	float fBar2Bot = fBar2Top + fBarHeight;
	if (fMeterY > fBar2Top && fMeterY < fBar2Bot)
	{
		float fExpNorm = saturate(fCurrentExp / 8.0);
		if (fMeterX < fExpNorm) xOverlayColour = float3(0.9, 0.9, 0.0);
		return true;
	}
	float fBar3Top = fBar2Bot + fBarGap;
	float fBar3Bot = fBar3Top + fBarHeight;
	if (fMeterY > fBar3Top && fMeterY < fBar3Bot)
	{
		float fTargetNorm = saturate(fTargetExp / 8.0);
		if (fMeterX < fTargetNorm) xOverlayColour = float3(0.0, 0.9, 0.0);
		return true;
	}
	return true;
}

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target
{
	// Debug 9: solid magenta confirms the tonemap pass executed.
	if (g_xPassSet.ToneMappingConstants.g_uDebugMode == 9u)
		return float4(1.0, 0.0, 1.0, 1.0);

	float3 xHDRColour = g_xPassSet.g_xHDRTex.Sample(input.uv).rgb;
	float3 xBloom     = g_xPassSet.g_xBloomTex.Sample(input.uv).rgb;

	// Debug 10: raw HDR clamped — diagnoses when the HDR target is unexpectedly black.
	if (g_xPassSet.ToneMappingConstants.g_uDebugMode == 10u)
		return float4(saturate(xHDRColour), 1.0);

	// Auto-exposure path picks the GPU-computed value; manual path uses the CB scalar.
	float fExposure = (g_xPassSet.ToneMappingConstants.g_bAutoExposure != 0u)
		? g_xPassSet.g_afExposureData[1]
		: g_xPassSet.ToneMappingConstants.g_fExposure;
	float3 xExposed = xHDRColour * fExposure;
	xExposed += xBloom * g_xPassSet.ToneMappingConstants.g_fBloomIntensity;

	if (g_xPassSet.ToneMappingConstants.g_uDebugMode > 0u)
	{
		switch (g_xPassSet.ToneMappingConstants.g_uDebugMode)
		{
			case 1u:
			{
				float fLum    = Luminance(xHDRColour);
				float fLogLum = log2(max(fLum, 0.0001)) / 10.0 + 0.5;
				return float4(DebugHeatmap(fLogLum), 1.0);
			}
			case 2u:
			{
				float3 xOverlay;
				if (RenderHistogramOverlay(input.uv, xOverlay)) return float4(xOverlay, 1.0);
				break;
			}
			case 3u:
			{
				float3 xOverlay;
				if (RenderExposureMeter(input.uv, xOverlay)) return float4(xOverlay, 1.0);
				break;
			}
			case 4u:
				return float4(xBloom * g_xPassSet.ToneMappingConstants.g_fBloomIntensity, 1.0);
			case 5u:
				return float4(xBloom * float3(1.0, 0.8, 0.6), 1.0);
			case 6u:
				return float4(saturate(xExposed), 1.0);
			case 7u:
			{
				float fMaxVal = max(xExposed.r, max(xExposed.g, xExposed.b));
				float fMinVal = min(xExposed.r, min(xExposed.g, xExposed.b));
				if (fMaxVal > 2.0)   return float4(1.0, 0.0, 0.0, 1.0);
				if (fMinVal < 0.001) return float4(0.0, 0.0, 1.0, 1.0);
				break;
			}
			case 8u:
			{
				float fLum  = Luminance(xHDRColour);
				float fEV   = log2(max(fLum, 0.0001) / 0.18);
				int   iZone = clamp((int)floor(fEV + 5.5), 0, 10);
				float fZone = (float)iZone / 10.0;
				return float4(float3(fZone, fZone, fZone), 1.0);
			}
		}
	}

	float3 xToneMapped = ApplyToneMapping(xExposed, g_xPassSet.ToneMappingConstants.g_uToneMappingOperator);

	if (g_xPassSet.ToneMappingConstants.g_bShowHistogram != 0u)
	{
		float3 xOverlay;
		if (RenderHistogramOverlay(input.uv, xOverlay)) return float4(xOverlay, 1.0);
	}

	// Scene anti-aliasing is handled by TAA (Flux/TAA) upstream of the HDR bloom/tonemap; the
	// inline FXAA stopgap and its ToneAt helper were removed once TAA shipped as the default AA.

	// Anti-banding dither in perceptual space, keyed on the pixel coordinate.
	xToneMapped = ApplyOutputDither(xToneMapped, input.position.xy);
	return float4(xToneMapped, 1.0);
}
