// =====================================================================
// SSR.Flux_SSR_DenoiseCommon — shared Slang module.
//
// De-duplicated push-constant layout, variance-adaptive-kernel constants
// and helper used by both SSR_DenoiseH and SSR_DenoiseV. Keeping these
// in one place avoids the silent drift that bites when two near-identical
// shaders live side-by-side.
//
// Constant naming preserves the SSR_DENOISE_* prefix so caller code reads
// the same as it did when the constants were inline.
// =====================================================================

// ---------- Push-constant layout ----------
//
// Identical layout in H and V. ≤32 bytes so it sits comfortably alongside
// other push-constant consumers in the 128-byte budget.

public struct SSRDenoisePushConstantsLayout
{
	public float u_fSpatialSigma;
	public float u_fDepthSigma;       // fraction of centre depth
	public float u_fNormalSigma;      // sigma over (1 - NdotN)
	public float u_fRoughnessSigma;   // sigma over |dr|
	public uint  u_uKernelRadius;
	public uint  u_bEnabled;
	public float _pad0;               // reserved (Phase 4 variance config)
	public float _pad1;
};

// ---------- Variance-adaptive kernel (Phase 4) ----------
//
// Variance is computed over a 3×3 footprint of (luma, hit-distance). Sharp
// regions (low variance) shrink the kernel toward MIN×; noisy regions
// (high variance) widen it toward MAX×.
//   * Variance metric uses *luma* + *hit distance* — NOT hit UV (which can
//     read deceptively low when neighbours' rays converge in UV space but
//     the underlying albedo is high-frequency).
//   * Distance is rescaled by VAR_DIST_SCALE so the two terms live on
//     comparable magnitudes before the max().

public static const float SSR_DENOISE_VAR_LO         = 0.001;
public static const float SSR_DENOISE_VAR_HI         = 0.05;
public static const float SSR_DENOISE_VAR_DIST_SCALE = 1000.0;
public static const float SSR_DENOISE_VAR_RADIUS_MIN = 0.5;
public static const float SSR_DENOISE_VAR_RADIUS_MAX = 2.0;

public float SSR_ComputeVarianceScale(Sampler2D xUpsampled,
                                       Sampler2D xUpsampledAux,
                                       float2 uv,
                                       float2 rcpScreen)
{
	float aafLuma[9];
	float aafDist[9];
	int   idx = 0;
	for (int y = -1; y <= 1; y++)
	{
		for (int x = -1; x <= 1; x++)
		{
			float2 xS = uv + float2((float)x, (float)y) * rcpScreen;
			float3 xC = xUpsampled.Sample(xS).rgb;
			float  fD = xUpsampledAux.Sample(xS).b;
			aafLuma[idx] = dot(xC, float3(0.299, 0.587, 0.114));
			aafDist[idx] = fD;
			idx++;
		}
	}

	float fMeanL = 0.0; float fMeanD = 0.0;
	for (int i = 0; i < 9; i++) { fMeanL += aafLuma[i]; fMeanD += aafDist[i]; }
	fMeanL /= 9.0; fMeanD /= 9.0;

	float fVarL = 0.0; float fVarD = 0.0;
	for (int i = 0; i < 9; i++)
	{
		float fDL = aafLuma[i] - fMeanL;
		float fDD = aafDist[i] - fMeanD;
		fVarL += fDL * fDL;
		fVarD += fDD * fDD;
	}
	fVarL /= 9.0;
	fVarD /= 9.0;

	float fVar = max(fVarL, fVarD / SSR_DENOISE_VAR_DIST_SCALE);
	float fT   = smoothstep(SSR_DENOISE_VAR_LO, SSR_DENOISE_VAR_HI, fVar);
	return lerp(SSR_DENOISE_VAR_RADIUS_MIN, SSR_DENOISE_VAR_RADIUS_MAX, fT);
}
