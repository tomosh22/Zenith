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
[[vk::binding(0, 0)]] ConstantBuffer<ToneMappingConstantsLayout> ToneMappingConstants;
[[vk::binding(1, 0)]] Sampler2D                 g_xHDRTex;
[[vk::binding(2, 0)]] Sampler2D                 g_xBloomTex;
[[vk::binding(3, 0)]] StructuredBuffer<uint>    g_auHistogram;
[[vk::binding(4, 0)]] StructuredBuffer<float>   g_afExposureData;

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

float3 ApplyToneMapping(float3 xHDR, uint uOperator)
{
	switch (uOperator)
	{
		case 0: return ACESFilmicToneMapping(xHDR);
		case 1: return ACESFittedToneMapping(xHDR);
		case 2: return ReinhardToneMapping(xHDR);
		case 3: return Uncharted2ToneMapping(xHDR);
		case 4: return NeutralToneMapping(xHDR);
		default: return ACESFilmicToneMapping(xHDR);
	}
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
	uint uCount     = g_auHistogram[uBin];
	uint uMaxCount  = max(1u, (uint)g_afExposureData[3]);

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
	float fAvgLum     = g_afExposureData[0];
	float fCurrentExp = g_afExposureData[1];
	float fTargetExp  = g_afExposureData[2];

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
	if (ToneMappingConstants.g_uDebugMode == 9u)
		return float4(1.0, 0.0, 1.0, 1.0);

	float3 xHDRColour = g_xHDRTex.Sample(input.uv).rgb;
	float3 xBloom     = g_xBloomTex.Sample(input.uv).rgb;

	// Debug 10: raw HDR clamped — diagnoses when the HDR target is unexpectedly black.
	if (ToneMappingConstants.g_uDebugMode == 10u)
		return float4(saturate(xHDRColour), 1.0);

	// Auto-exposure path picks the GPU-computed value; manual path uses the CB scalar.
	float fExposure = (ToneMappingConstants.g_bAutoExposure != 0u)
		? g_afExposureData[1]
		: ToneMappingConstants.g_fExposure;
	float3 xExposed = xHDRColour * fExposure;
	xExposed += xBloom * ToneMappingConstants.g_fBloomIntensity;

	if (ToneMappingConstants.g_uDebugMode > 0u)
	{
		switch (ToneMappingConstants.g_uDebugMode)
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
				return float4(xBloom * ToneMappingConstants.g_fBloomIntensity, 1.0);
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

	float3 xToneMapped = ApplyToneMapping(xExposed, ToneMappingConstants.g_uToneMappingOperator);

	if (ToneMappingConstants.g_bShowHistogram != 0u)
	{
		float3 xOverlay;
		if (RenderHistogramOverlay(input.uv, xOverlay)) return float4(xOverlay, 1.0);
	}

	return float4(xToneMapped, 1.0);
}
