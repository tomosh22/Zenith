// =====================================================================
// Flux_SSR_Resolve — Slang module.
//
// Subsystem: SSR. Roughness-aware bilateral blur on the SSR ray-march
// result. Mirror-smooth and very-rough surfaces pass through; medium
// roughness gets a contact-hardening Gaussian blur whose radius scales
// with roughness² (matching GGX cone footprint) and shrinks at near
// contact. Bilateral weights (normal / depth / confidence) keep edges
// crisp.
//
// Imports Common.Frame for FrameConstants (g_xScreenDims,
// g_xRcpScreenDims). Standalone otherwise.
// =====================================================================

import Common.Fullscreen;
import Common.Frame;

// Roughness thresholds.
static const float SSR_RESOLVE_SMOOTH_THRESHOLD   = 0.15;  // mirror-like → pass through
static const float SSR_RESOLVE_ROUGH_THRESHOLD    = 0.7;   // very rough → IBL handles it
static const float SSR_RESOLVE_ROUGHNESS_RANGE    = 0.55;

// Confidence floors.
static const float SSR_RESOLVE_MIN_CONFIDENCE     = 0.001;
static const float SSR_RESOLVE_MIN_WEIGHT         = 0.001;

// Contact-hardening blur. blurRadius = roughness² * BLUR_SCALE + OFFSET,
// then scaled down at near contact. SSR_RESOLVE_BLUR_SCALE=48 approximates
// the GGX reflection-cone footprint at 1080p (45° half-angle * 10m ray ≈
// ~50 px at typical FOV); empirically tuned in the GLSL original.
static const float SSR_RESOLVE_BLUR_SCALE         = 48.0;
static const float SSR_RESOLVE_BLUR_OFFSET        = 2.0;
static const float SSR_RESOLVE_CONTACT_SCALE_MIN  = 0.2;
static const float SSR_RESOLVE_MIN_BLUR_RADIUS    = 1.0;

// Kernel.
static const float SSR_RESOLVE_MAX_KERNEL_SIZE    = 3.0; // 7x7 base, up to ~21x21 with depth-scale
static const float SSR_RESOLVE_KERNEL_DIVISOR     = 8.0;
static const float SSR_RESOLVE_SIGMA_SCALE        = 0.5;

// Depth-adaptive kernel — distant surfaces get larger kernels.
static const float SSR_RESOLVE_DEPTH_SCALE_FAR    = 0.1;
static const float SSR_RESOLVE_DEPTH_SCALE_MIN    = 0.5;
static const float SSR_RESOLVE_DEPTH_SCALE_MAX    = 1.5;

static const float SSR_RESOLVE_EDGE_GRADIENT_SCALE = 10.0;

// Normal-strictness powers — relaxed from the historical 32/24 because
// those were rejecting valid samples on naturally curved geometry.
static const float SSR_RESOLVE_NORMAL_POWER_SMOOTH = 16.0;
static const float SSR_RESOLVE_NORMAL_POWER_ROUGH  = 8.0;
// Depth sigma as a fraction of the centre depth — keeps the bilateral
// behaving consistently whether the far plane is 100 m or 1000 m.
static const float SSR_RESOLVE_DEPTH_SIGMA_FRACTION = 0.02;

static const float SSR_RESOLVE_REFERENCE_HEIGHT   = 1080.0;

// Layout must match the CPU SSRConstants struct (and the RayMarch shader's
// layout) end-to-end since they share the same CBV. Resolve doesn't actually
// read every field, but the binding layout must be consistent across users.
struct SSRConstantsLayout
{
	float u_fIntensity;
	float u_fMaxDistance;
	float u_fMaxRoughness;
	float u_fThickness;
	uint  u_uStepCount;
	uint  u_uDebugMode;
	uint  u_uHiZMipCount;
	uint  u_uStartMip;
	uint  u_uFrameIndex;
	uint  u_uBinarySearchIterations;
	float u_fContactHardeningDist;
	float u_fHalfResWidth;
	float u_fHalfResHeight;
	float u_fRcpHalfResWidth;
	float u_fRcpHalfResHeight;
	float u_fPad0;
	float4 u_axHiZMipSizes[12];
};
[[vk::binding(1, 0)]] ConstantBuffer<SSRConstantsLayout> SSRConstants;
[[vk::binding(2, 0)]] Sampler2D g_xRayMarchTex;
[[vk::binding(3, 0)]] Sampler2D g_xNormalsTex;
[[vk::binding(4, 0)]] Sampler2D g_xMaterialTex;
[[vk::binding(5, 0)]] Sampler2D g_xDepthTex;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target
{
	float4 xMaterial    = g_xMaterialTex.Sample(input.uv);
	float  fRoughness   = xMaterial.r;
	float3 xCenterNormal = g_xNormalsTex.Sample(input.uv).rgb;
	float  fCenterDepth  = g_xDepthTex.Sample(input.uv).r;

	float4 xCenterSSR        = g_xRayMarchTex.Sample(input.uv);
	float  fCenterConfidence = xCenterSSR.a;

	// Mirror-smooth: pass through directly.
	if (fRoughness < SSR_RESOLVE_SMOOTH_THRESHOLD) return xCenterSSR;
	// Very rough: pass through too — confidence is already attenuated by the
	// ray-march pass so the deferred shader will blend toward IBL.
	if (fRoughness > SSR_RESOLVE_ROUGH_THRESHOLD) return xCenterSSR;
	if (fCenterConfidence < SSR_RESOLVE_MIN_CONFIDENCE) return xCenterSSR;

	// Remap roughness from [smooth, rough] to [0, 1] for blur math.
	float fRemappedRoughness = (fRoughness - SSR_RESOLVE_SMOOTH_THRESHOLD) / SSR_RESOLVE_ROUGHNESS_RANGE;

	// Contact hardening: confidence ≈ 1 means near contact (sharp reflection),
	// confidence ≈ 0 means far reflection (allow full blur).
	float fContactFactor = 1.0 - saturate(fCenterConfidence);

	// Resolution scaling so the blur looks consistent across 720p / 1080p / 1440p / 4K.
	float fResolutionScale = (float)FrameConstants.g_xScreenDims.y / SSR_RESOLVE_REFERENCE_HEIGHT;
	float fScaledBlurScale = SSR_RESOLVE_BLUR_SCALE * fResolutionScale;

	float fRoughnessBlur = fRemappedRoughness * fRemappedRoughness * fScaledBlurScale + SSR_RESOLVE_BLUR_OFFSET;
	float fContactScale  = lerp(SSR_RESOLVE_CONTACT_SCALE_MIN, 1.0, fContactFactor);
	float fBlurRadiusPixels = max(fRoughnessBlur * fContactScale, SSR_RESOLVE_MIN_BLUR_RADIUS);

	int iKernelSize = (int)min(SSR_RESOLVE_MAX_KERNEL_SIZE,
								ceil(fBlurRadiusPixels / SSR_RESOLVE_KERNEL_DIVISOR));

	// Depth-adaptive kernel growth.
	float fDepthScale = lerp(SSR_RESOLVE_DEPTH_SCALE_MIN, SSR_RESOLVE_DEPTH_SCALE_MAX,
							  smoothstep(0.0, SSR_RESOLVE_DEPTH_SCALE_FAR, fCenterDepth));
	iKernelSize = (int)clamp((float)iKernelSize * fDepthScale,
							  1.0, SSR_RESOLVE_MAX_KERNEL_SIZE * SSR_RESOLVE_DEPTH_SCALE_MAX);

	// Normal-gradient edge detection — tighten weights at silhouettes.
	float3 xGradientX  = ddx_fine(xCenterNormal);
	float3 xGradientY  = ddy_fine(xCenterNormal);
	float  fGradientMag = length(xGradientX) + length(xGradientY);
	float  fEdgeFactor  = 1.0 + fGradientMag * SSR_RESOLVE_EDGE_GRADIENT_SCALE;

	float4 xAccum       = float4(0.0, 0.0, 0.0, 0.0);
	float  fTotalWeight = 0.0;

	float fSigma     = max(fBlurRadiusPixels * SSR_RESOLVE_SIGMA_SCALE, SSR_RESOLVE_MIN_BLUR_RADIUS);
	float fSigmaSq2  = 2.0 * fSigma * fSigma;
	float fOffsetScale = fBlurRadiusPixels / (float)iKernelSize;

	for (int y = -iKernelSize; y <= iKernelSize; y++)
	{
		for (int x = -iKernelSize; x <= iKernelSize; x++)
		{
			float2 xOffset    = float2((float)x, (float)y) * FrameConstants.g_xRcpScreenDims * fOffsetScale;
			float2 xSampleUV  = saturate(input.uv + xOffset);

			float4 xSample      = g_xRayMarchTex.Sample(xSampleUV);
			float3 xSampleNormal = g_xNormalsTex.Sample(xSampleUV).rgb;
			float  fSampleDepth  = g_xDepthTex.Sample(xSampleUV).r;

			float fDistSq        = (float)(x * x + y * y);
			float fSpatialWeight = exp(-fDistSq / (fSigmaSq2 + 0.001));

			float fNormalSimilarity = max(dot(xCenterNormal, xSampleNormal), 0.0);
			float fNormalPower      = lerp(SSR_RESOLVE_NORMAL_POWER_SMOOTH,
										   SSR_RESOLVE_NORMAL_POWER_ROUGH,
										   fRemappedRoughness) * fEdgeFactor;
			float fNormalWeight     = pow(fNormalSimilarity, fNormalPower);

			float fDepthDiff   = abs(fSampleDepth - fCenterDepth);
			float fDepthSigma  = max(fCenterDepth * SSR_RESOLVE_DEPTH_SIGMA_FRACTION, 0.001);
			float fDepthWeight = exp(-fDepthDiff * fDepthDiff / (fDepthSigma * fDepthSigma));

			// Confidence weight pulls toward hits the ray-march had high faith in.
			float fWeight = fSpatialWeight * fNormalWeight * fDepthWeight * xSample.a;

			xAccum       += xSample * fWeight;
			fTotalWeight += fWeight;
		}
	}

	if (fTotalWeight > SSR_RESOLVE_MIN_WEIGHT) return xAccum / fTotalWeight;
	return g_xRayMarchTex.Sample(input.uv); // fail-safe
}
