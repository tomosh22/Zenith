#version 450 core

layout(location = 0) out vec4 o_xColour;

layout(location = 0) in vec2 a_xUV;

// Scratch buffer for push constants replacement
layout(std140, set = 0, binding = 0) uniform ToneMappingConstants
{
	float g_fExposure;
	float g_fBloomIntensity;
	uint g_uToneMappingOperator;
	uint g_uDebugMode;
	uint g_bShowHistogram;
	uint g_bAutoExposure;
	uint g_uPad0;
	uint g_uPad1;
};

layout(set = 0, binding = 1) uniform sampler2D g_xHDRTex;
layout(set = 0, binding = 2) uniform sampler2D g_xBloomTex;

// Histogram buffer (256 bins) - readonly access for visualization
layout(std430, set = 0, binding = 3) readonly buffer HistogramBuffer
{
	uint g_auHistogram[256];
};

// Exposure buffer from auto-exposure compute
// [0] = average luminance, [1] = current exposure, [2] = target exposure, [3] = histogram max count
layout(std430, set = 0, binding = 4) readonly buffer ExposureBuffer
{
	float g_afExposureData[4];
};


// ACES Filmic Tone Mapping (sRGB)
// Krzysztof Narkowicz approximation of ACES RRT+ODT
// Widely used in games for good highlight rolloff and color preservation
vec3 ACESFilmicToneMapping(vec3 x)
{
	float a = 2.51;  // Contrast
	float b = 0.03;  // Toe lift
	float c = 2.43;  // Shoulder contrast
	float d = 0.59;  // Shoulder lift
	float e = 0.14;  // Toe clamp
	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// ACES Fitted (more accurate approximation of the ACES curve)
vec3 ACESFittedToneMapping(vec3 v)
{
	mat3 ACES_INPUT_MAT = mat3(
		0.59719, 0.07600, 0.02840,
		0.35458, 0.90834, 0.13383,
		0.04823, 0.01566, 0.83777
	);

	mat3 ACES_OUTPUT_MAT = mat3(
		1.60475, -0.10208, -0.00327,
		-0.53108, 1.10813, -0.07276,
		-0.07367, -0.00605, 1.07602
	);

	v = ACES_INPUT_MAT * v;

	vec3 a = v * (v + 0.0245786) - 0.000090537;
	vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
	v = a / b;

	return clamp(ACES_OUTPUT_MAT * v, 0.0, 1.0);
}

// Reinhard Tone Mapping
vec3 ReinhardToneMapping(vec3 color)
{
	return color / (color + vec3(1.0));
}

// Uncharted 2 Tone Mapping (John Hable's filmic curve)
// Parameters from original Uncharted 2 GDC presentation
vec3 Uncharted2Tonemap(vec3 x)
{
	float A = 0.15;  // Shoulder strength
	float B = 0.50;  // Linear strength
	float C = 0.10;  // Linear angle
	float D = 0.20;  // Toe strength
	float E = 0.02;  // Toe numerator
	float F = 0.30;  // Toe denominator
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 Uncharted2ToneMapping(vec3 color)
{
	float W = 11.2;            // Linear white point
	float ExposureBias = 2.0;  // Pre-exposure multiplier
	vec3 curr = Uncharted2Tonemap(ExposureBias * color);
	vec3 whiteScale = vec3(1.0) / Uncharted2Tonemap(vec3(W));
	return curr * whiteScale;
}

// Neutral Tone Mapping (minimal contrast change)
vec3 NeutralToneMapping(vec3 color)
{
	float startCompression = 0.8 - 0.04;
	float desaturation = 0.15;

	float x = min(color.r, min(color.g, color.b));
	float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
	color -= offset;

	float peak = max(color.r, max(color.g, color.b));
	if (peak < startCompression) return clamp(color, 0.0, 1.0);

	// Prevent division by zero when peak is very small
	peak = max(peak, 0.0001);

	float d = 1.0 - startCompression;
	// Ensure denominator stays positive to prevent negative newPeak
	float denominator = max(peak + d - startCompression, 0.0001);
	float newPeak = 1.0 - d * d / denominator;
	// Clamp newPeak to valid range
	newPeak = clamp(newPeak, 0.0, 1.0);
	color *= newPeak / peak;

	float g = 1.0 - 1.0 / (desaturation * max(peak - newPeak, 0.0) + 1.0);
	return clamp(mix(color, vec3(newPeak), g), 0.0, 1.0);
}

// Apply tone mapping based on operator
vec3 ApplyToneMapping(vec3 xHDR, uint uOperator)
{
	switch(uOperator)
	{
		case 0: return ACESFilmicToneMapping(xHDR);
		case 1: return ACESFittedToneMapping(xHDR);
		case 2: return ReinhardToneMapping(xHDR);
		case 3: return Uncharted2ToneMapping(xHDR);
		case 4: return NeutralToneMapping(xHDR);
		default: return ACESFilmicToneMapping(xHDR);
	}
}

// Calculate luminance
float Luminance(vec3 color)
{
	return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

// Debug heatmap (0.0=blue, 0.5=green, 1.0=red)
vec3 DebugHeatmap(float fValue)
{
	fValue = clamp(fValue, 0.0, 1.0);
	vec3 xColor;
	if (fValue < 0.5)
		xColor = mix(vec3(0, 0, 1), vec3(0, 1, 0), fValue * 2.0);
	else
		xColor = mix(vec3(0, 1, 0), vec3(1, 0, 0), (fValue - 0.5) * 2.0);
	return xColor;
}

// Render histogram overlay in bottom-left corner
// Returns true if pixel was drawn as part of histogram, false otherwise
bool RenderHistogramOverlay(vec2 xUV, out vec3 xOverlayColor)
{
	// Histogram display region: bottom-left
	// UV coordinates: (0,0) = top-left, (1,1) = bottom-right
	const float fHistWidth = 0.3;   // 30% of screen width
	const float fHistHeight = 0.15; // 15% of screen height
	const float fMargin = 0.02;     // 2% margin from edges

	// Bottom-left region: X from margin, Y from (1-margin-height) to (1-margin)
	float fYBottom = 1.0 - fMargin;
	float fYTop = fYBottom - fHistHeight;

	// Check if we're in the histogram region
	if (xUV.x < fMargin || xUV.x > fMargin + fHistWidth ||
		xUV.y < fYTop || xUV.y > fYBottom)
	{
		return false;
	}

	// Normalize UV within histogram region
	// X: 0 = left edge, 1 = right edge
	// Y: 0 = bottom of histogram, 1 = top of histogram (invert for bar drawing)
	float fHistX = (xUV.x - fMargin) / fHistWidth;
	float fHistY = (fYBottom - xUV.y) / fHistHeight;  // Inverted so 0 = bottom

	// Get histogram bin index (0-255)
	uint uBin = uint(fHistX * 256.0);
	uBin = min(uBin, 255u);

	// Get bin count
	uint uCount = g_auHistogram[uBin];

	// Use precomputed max count from adaptation shader (stored in exposure buffer)
	// This avoids a 256-iteration loop per pixel which was extremely expensive
	uint uMaxCount = max(1u, uint(g_afExposureData[3]));

	// Normalize height (log scale for better visualization)
	float fNormHeight = log(float(uCount) + 1.0) / log(float(uMaxCount) + 1.0);

	// Draw bar (fHistY is 0 at bottom, so bar grows upward)
	if (fHistY < fNormHeight)
	{
		// Grayscale based on luminance zone - darker bins = darker bars
		float fBinNorm = float(uBin) / 255.0;
		// Map 0-1 to a visible range (0.15 to 0.95) so shadows aren't invisible
		float fGray = mix(0.15, 0.95, fBinNorm);
		xOverlayColor = vec3(fGray);
		return true;
	}
	else
	{
		// Background
		xOverlayColor = vec3(0.1, 0.1, 0.1);
		return true;
	}
}

// Render exposure meter in top-right corner
// Shows current exposure, target exposure, and average luminance as visual bars
// Returns true if pixel was drawn as part of meter, false otherwise
bool RenderExposureMeter(vec2 xUV, out vec3 xOverlayColor)
{
	// Exposure meter display region: top-right corner
	const float fMeterWidth = 0.12;    // 12% of screen width
	const float fMeterHeight = 0.08;   // 8% of screen height
	const float fMargin = 0.02;        // 2% margin from edges

	// Top-right region
	float fXLeft = 1.0 - fMargin - fMeterWidth;
	float fXRight = 1.0 - fMargin;
	float fYTop = fMargin;
	float fYBottom = fMargin + fMeterHeight;

	// Check if we're in the meter region
	if (xUV.x < fXLeft || xUV.x > fXRight || xUV.y < fYTop || xUV.y > fYBottom)
	{
		return false;
	}

	// Normalize UV within meter region
	float fMeterX = (xUV.x - fXLeft) / fMeterWidth;
	float fMeterY = (xUV.y - fYTop) / fMeterHeight;

	// Background
	xOverlayColor = vec3(0.1);

	// Get exposure data
	float fAvgLum = g_afExposureData[0];
	float fCurrentExp = g_afExposureData[1];
	float fTargetExp = g_afExposureData[2];

	// Draw three horizontal bars
	// Bar 1: Average luminance (log scale, mapped 0.001-100 to 0-1)
	// Bar 2: Current exposure
	// Bar 3: Target exposure

	float fBarHeight = 0.25;
	float fBarGap = 0.08;

	// Bar 1: Average luminance (cyan)
	float fBar1Top = 0.1;
	float fBar1Bot = fBar1Top + fBarHeight;
	if (fMeterY > fBar1Top && fMeterY < fBar1Bot)
	{
		float fLumNorm = clamp((log(fAvgLum + 0.0001) + 7.0) / 12.0, 0.0, 1.0);  // -7 to +5 log range
		if (fMeterX < fLumNorm)
		{
			xOverlayColor = vec3(0.0, 0.8, 0.8);  // Cyan
		}
		return true;
	}

	// Bar 2: Current exposure (yellow)
	float fBar2Top = fBar1Bot + fBarGap;
	float fBar2Bot = fBar2Top + fBarHeight;
	if (fMeterY > fBar2Top && fMeterY < fBar2Bot)
	{
		float fExpNorm = clamp(fCurrentExp / 8.0, 0.0, 1.0);  // 0-8 EV range
		if (fMeterX < fExpNorm)
		{
			xOverlayColor = vec3(0.9, 0.9, 0.0);  // Yellow
		}
		return true;
	}

	// Bar 3: Target exposure (green)
	float fBar3Top = fBar2Bot + fBarGap;
	float fBar3Bot = fBar3Top + fBarHeight;
	if (fMeterY > fBar3Top && fMeterY < fBar3Bot)
	{
		float fTargetNorm = clamp(fTargetExp / 8.0, 0.0, 1.0);  // 0-8 EV range
		if (fMeterX < fTargetNorm)
		{
			xOverlayColor = vec3(0.0, 0.9, 0.0);  // Green
		}
		return true;
	}

	return true;  // Background of meter area
}

void main()
{
	// Debug mode 9: Output solid magenta to verify tone mapping pass is running
	// If you see magenta, tone mapping is working but HDR target may be black
	// If you see black, tone mapping pass may not be running at all
	if (g_uDebugMode == 9u)
	{
		o_xColour = vec4(1.0, 0.0, 1.0, 1.0); // Magenta
		return;
	}

	vec3 xHDRColor = texture(g_xHDRTex, a_xUV).rgb;
	vec3 xBloom = texture(g_xBloomTex, a_xUV).rgb;

	// Debug mode 10: Output raw HDR texture values (clamped to 0-1) to see what's being sampled
	if (g_uDebugMode == 10u)
	{
		o_xColour = vec4(clamp(xHDRColor, 0.0, 1.0), 1.0);
		return;
	}

	// Apply exposure - use GPU-computed exposure when auto-exposure enabled
	float fExposure = (g_bAutoExposure != 0u) ? g_afExposureData[1] : g_fExposure;
	vec3 xExposed = xHDRColor * fExposure;

	// Add bloom
	xExposed += xBloom * g_fBloomIntensity;

	// Debug modes
	if (g_uDebugMode > 0u)
	{
		switch(g_uDebugMode)
		{
			case 1u: // Luminance heatmap
			{
				float fLum = Luminance(xHDRColor);
				float fLogLum = log2(max(fLum, 0.0001)) / 10.0 + 0.5; // Map -5 to +5 EV to 0-1
				o_xColour = vec4(DebugHeatmap(fLogLum), 1.0);
				return;
			}
			case 2u: // Histogram overlay
			{
				// Render histogram overlay on top of scene
				vec3 xOverlayColor;
				if (RenderHistogramOverlay(a_xUV, xOverlayColor))
				{
					o_xColour = vec4(xOverlayColor, 1.0);
					return;
				}
				break;
			}
			case 3u: // Exposure meter
			{
				// Show exposure info in corner (cyan=avg lum, yellow=current exp, green=target exp)
				vec3 xOverlayColor;
				if (RenderExposureMeter(a_xUV, xOverlayColor))
				{
					o_xColour = vec4(xOverlayColor, 1.0);
					return;
				}
				break;
			}
			case 4u: // Bloom only
			{
				o_xColour = vec4(xBloom * g_fBloomIntensity, 1.0);
				return;
			}
			case 5u: // Bloom mips visualization
			{
				// NOTE: Full mip visualization requires binding all bloom mip textures
				// which needs C++ side changes to add texture bindings for each mip level.
				// For now, show the final bloom result with a colored tint to indicate debug mode.
				o_xColour = vec4(xBloom * vec3(1.0, 0.8, 0.6), 1.0);  // Orange tint
				return;
			}
			case 6u: // Pre-tonemap
			{
				o_xColour = vec4(clamp(xExposed, 0.0, 1.0), 1.0);
				return;
			}
			case 7u: // Clipping indicator
			{
				vec3 xTonemapped = ApplyToneMapping(xExposed, g_uToneMappingOperator);
				float fMaxVal = max(xExposed.r, max(xExposed.g, xExposed.b));
				float fMinVal = min(xExposed.r, min(xExposed.g, xExposed.b));

				// Red for overexposed (>1.0 before tonemap means clipping)
				if (fMaxVal > 2.0)
				{
					o_xColour = vec4(1.0, 0.0, 0.0, 1.0);
					return;
				}
				// Blue for crushed blacks
				if (fMinVal < 0.001)
				{
					o_xColour = vec4(0.0, 0.0, 1.0, 1.0);
					return;
				}
				break;
			}
			case 8u: // EV zones
			{
				float fLum = Luminance(xHDRColor);
				float fEV = log2(max(fLum, 0.0001) / 0.18);
				// Map -5 to +5 EV to 11 zones
				int iZone = int(floor(fEV + 5.5));
				iZone = clamp(iZone, 0, 10);
				float fZoneValue = float(iZone) / 10.0;
				o_xColour = vec4(vec3(fZoneValue), 1.0);
				return;
			}
		}
	}

	// Apply tone mapping
	vec3 xToneMapped = ApplyToneMapping(xExposed, g_uToneMappingOperator);

	// Apply gamma correction (sRGB)
	vec3 xGammaCorrected = pow(xToneMapped, vec3(1.0 / 2.2));

	// Check for histogram overlay
	if (g_bShowHistogram != 0u)
	{
		vec3 xOverlayColor;
		if (RenderHistogramOverlay(a_xUV, xOverlayColor))
		{
			o_xColour = vec4(xOverlayColor, 1.0);
			return;
		}
	}

	o_xColour = vec4(xGammaCorrected, 1.0);
}
