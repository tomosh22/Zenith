// =====================================================================
// Common/ShadowSampling — the cascaded-shadow FILTER seam (Flux Shader
// System Overhaul, Stage 6).
//
// A "shadow filter" is anything that, given a cascade index + a world-space
// receiver (position / normal / N·L), decides how lit that receiver is in that
// cascade. The CASCADE WALK (which cascade a pixel lands in, and how adjacent
// cascades cross-fade) stays OUTSIDE the filter, in each consuming shader — the
// deferred composite selects by view-space depth and blends over a depth band,
// the forward translucent pass loops cascades and blends over an edge band, and
// those walks differ deliberately. The filter is the shared, swappable part.
//
// IShadowFilter is that seam. `ShadowFilterPCSS` here is the deferred composite's
// 16-tap Vogel PCF + PCSS blocker-search tier, moved VERBATIM out of
// Flux_DeferredShading.slang: the only change is that the six ShadowSampling-CB
// tunables it used to read straight from g_xPassSet became struct fields the
// caller fills from ITS OWN pass block, so Common stays decoupled from any one
// shader's CB layout. Not one line of filter math changed. A subsystem with a
// cheaper tier declares its own conforming type via `extension` next to its
// shader without editing this file (see Flux_Translucent_Forward.slang's
// ShadowFilterSimple4Tap) — that is the whole point of the seam.
//
// TEXTUAL #include (NOT `import`): the filter calls the spine accessors GetCSM()
// / GetShadowMatrix() (free functions from Common/Bindings.slang), so the
// consumer must #include Bindings.slang FIRST — an `import` module would not see
// those textually-included free functions. Declares no ParameterBlock, so it
// does not shift the spine's set 0/1/2 assignment.
// =====================================================================

#ifndef FLUX_COMMON_SHADOWSAMPLING_SLANG
#define FLUX_COMMON_SHADOWSAMPLING_SLANG

// ============================ Cascaded shadow mapping ============================
// Method: stabilised bounding-sphere cascades (fit + texel-snapped on the CPU) +
// normal-offset receiver bias + optimised multi-tap PCF over a Vogel disk, with
// an optional PCSS blocker search for contact-hardening soft shadows. All sample
// patterns are deterministic frame-to-frame, so nothing crawls or sparkles.

static const float SHADOW_GOLDEN_ANGLE = 2.39996323;   // rad — Vogel disk spiral
static const float SHADOW_MAX_PENUMBRA_TEXELS = 16.0;  // clamp on PCSS softening
static const uint  SHADOW_BLOCKER_TAPS = 16u;

// Per-pixel kernel rotation via interleaved gradient noise (Jimenez). Stable for
// a given pixel/frame, so it decorrelates the disk without introducing temporal
// shimmer the way a time-varying jitter would.
float ShadowKernelRotation(float2 xPixel)
{
	float fIGN = frac(52.9829189 * frac(dot(xPixel, float2(0.06711056, 0.00583715))));
	return fIGN * 6.2831853;
}

float2 VogelDiskSample(uint uIndex, uint uCount, float fRotation)
{
	float fR     = sqrt((float(uIndex) + 0.5) / float(uCount));
	float fTheta = float(uIndex) * SHADOW_GOLDEN_ANGLE + fRotation;
	return float2(fR * cos(fTheta), fR * sin(fTheta));
}

float4 GatherCascadeDepth(int iCascade, float2 xUV)
{
	// Cascade selects the array layer (Phase 4b: one Sampler2DArray, no if-ladder).
	return GetCSM().Gather(float3(xUV, float(iCascade)));
}

// All 4 cascade sun view×proj matrices live in one StructuredBuffer (Phase 4a) —
// directly indexable by the runtime cascade index (no per-CB switch needed).
float4x4 ShadowMatrixForCascade(int iCascade)
{
	return GetShadowMatrix(iCascade);
}

// The filter seam: given a cascade index and a world-space receiver, return
// whether the receiver projected inside this cascade's map, and (in fShadow)
// 1 = lit … 0 = shadowed. The cascade WALK — selecting the cascade and
// cross-fading between adjacent ones — is the caller's, and each caller's differs.
public interface IShadowFilter
{
	bool EvaluateCascade(int iCascade, float3 xWorldPos, float3 xNormal, float fNdotL, out float fShadow);
}

// The deferred composite's tier: 16-tap Vogel PCF with an optional PCSS blocker
// search. VERBATIM move of Flux_DeferredShading.slang's former inline filter; the
// only edit is that the ShadowSampling-CB tunables are now struct fields the
// caller fills from its own pass block (and the per-pixel Vogel rotation, which
// the caller computes once, rides along as fRotation).
public struct ShadowFilterPCSS : IShadowFilter
{
	// Cascade sampling tunables — the caller fills these from its own PASS
	// ShadowSampling block. (Old source in the comment beside each field.)
	float  fRcpRes;                // g_xParams0.y — 1 / shadow-map resolution
	float  fNormalOffset;          // g_xParams0.z — normal-offset bias, texels
	float  fPCFRadius;             // g_xParams1.x — base PCF radius, texels
	float  fSunAngle;              // g_xParams1.y — sun angular radius
	float  fTapCountRaw;           // g_xParams1.w — requested PCF tap count
	float  fPCSSEnabled;           // g_xParams2.x — PCSS blocker search (>0.5 = on)
	float4 xCascadeWorldPerTexel;  // g_xCascadeWorldPerTexel — per-cascade world units/texel
	float4 xCascadeDepthRange;     // g_xCascadeDepthRange — per-cascade ortho depth range
	float  fRotation;              // per-pixel Vogel-disk rotation (ShadowKernelRotation)

	// Multi-tap PCF. Each Vogel tap issues a 4-texel Gather and compares each texel
	// against the (already biased) receiver depth — comparing raw depths individually
	// (never averaging depth before the compare) is what keeps shadow edges correct.
	// fRadiusTexels controls penumbra width.
	float SampleShadowPCF(int iCascade, float2 xUV, float fCompareDepth, float fRadiusTexels, uint uTaps, float fRotation)
	{
		float fRcpRes = this.fRcpRes;
		float fSum    = 0.0;
		for (uint i = 0u; i < uTaps; ++i)
		{
			float2 xOffset = VogelDiskSample(i, uTaps, fRotation) * fRadiusTexels * fRcpRes;
			float4 xDepths = GatherCascadeDepth(iCascade, xUV + xOffset);
			float4 xLit    = step(float4(fCompareDepth, fCompareDepth, fCompareDepth, fCompareDepth), xDepths);
			fSum          += dot(xLit, float4(0.25, 0.25, 0.25, 0.25));
		}
		return fSum / float(uTaps);
	}

	// PCSS blocker search: average depth of occluders nearer than the receiver, over
	// a disk scaled by the sun's angular size. Returns false in bFound when the
	// receiver is unoccluded (→ skip the expensive filter, surface is fully lit).
	float BlockerSearch(int iCascade, float2 xUV, float fReceiverDepth, float fSearchRadiusTexels, float fRotation, out bool bFound)
	{
		float fRcpRes     = this.fRcpRes;
		float fBlockerSum = 0.0;
		float fCount      = 0.0;
		for (uint i = 0u; i < SHADOW_BLOCKER_TAPS; ++i)
		{
			float2 xOffset = VogelDiskSample(i, SHADOW_BLOCKER_TAPS, fRotation) * fSearchRadiusTexels * fRcpRes;
			float4 xDepths = GatherCascadeDepth(iCascade, xUV + xOffset);
			[unroll]
			for (int k = 0; k < 4; ++k)
			{
				if (xDepths[k] < fReceiverDepth)
				{
					fBlockerSum += xDepths[k];
					fCount      += 1.0;
				}
			}
		}
		bFound = fCount > 0.5;
		return bFound ? (fBlockerSum / fCount) : 0.0;
	}

	// Full per-cascade shadow evaluation: normal-offset bias → project → bounds test
	// → PCSS/PCF. Writes 1=lit … 0=shadowed into fShadow and returns whether the
	// receiver projected inside this cascade's map.
	bool EvaluateCascade(int iCascade, float3 xWorldPos, float3 xNormal, float fNdotL, out float fShadow)
	{
		fShadow = 1.0;

		float fWorldPerTexel = this.xCascadeWorldPerTexel[iCascade];
		float fDepthRange    = this.xCascadeDepthRange[iCascade];
		float fNormalOffset  = this.fNormalOffset;
		float fPCFRadius     = this.fPCFRadius;
		float fSunAngle      = this.fSunAngle;
		// Clamp the tap count to a sane, bounded range — guarantees the PCF loop
		// terminates and never divides by zero even if the CB were ever unpopulated.
		uint  uTaps          = clamp(uint(this.fTapCountRaw), 1u, 32u);
		bool  bPCSS          = this.fPCSSEnabled > 0.5;

		// Normal-offset bias: shift the receiver along its normal by a few texels,
		// scaled by sin(angle-to-light) so grazing surfaces (worst acne) get the most
		// push and forward-facing ones get almost none. Per-cascade texel size makes
		// this self-scaling across cascades — no per-cascade tuning needed.
		float fSinTheta   = sqrt(saturate(1.0 - fNdotL * fNdotL));
		float3 xOffsetPos = xWorldPos + xNormal * (fWorldPerTexel * fNormalOffset * (fSinTheta + 0.15));

		float4 xShadowClip = mul(ShadowMatrixForCascade(iCascade), float4(xOffsetPos, 1.0));
		float3 xProj       = xShadowClip.xyz / xShadowClip.w;
		float2 xUV         = xProj.xy * 0.5 + 0.5;
		float  fDepth      = xProj.z;

		if (xUV.x < 0.0 || xUV.x > 1.0 || xUV.y < 0.0 || xUV.y > 1.0) return false;
		if (fDepth < 0.0 || fDepth > 1.0)                              return false;

		// No depth bias is applied here — it is baked into the stored caster depth by
		// the rasterizer's fixed-function slope-scaled depth bias (vkCmdSetDepthBias on
		// the shadow-caster pipelines). The receiver compares its true depth.
		float fCompareDepth = fDepth;

		float fRadiusTexels = fPCFRadius;
		if (bPCSS)
		{
			// Largest plausible penumbra (blocker at the far end of the depth range),
			// in texels — also used as the blocker-search radius.
			float fMaxPenumbra = clamp(fSunAngle * fDepthRange / fWorldPerTexel,
									   fPCFRadius, SHADOW_MAX_PENUMBRA_TEXELS);
			bool  bFound;
			float fAvgBlocker = this.BlockerSearch(iCascade, xUV, fCompareDepth, fMaxPenumbra, this.fRotation, bFound);
			if (!bFound) return true; // unoccluded → fully lit

			// Convert NDC blocker→receiver gap to a world distance (ortho depth is
			// linear), then to a penumbra radius via the sun's angular size.
			float fGapWorld     = max(fCompareDepth - fAvgBlocker, 0.0) * fDepthRange;
			float fPenumbraWorld = fGapWorld * fSunAngle;
			fRadiusTexels = clamp(fPenumbraWorld / fWorldPerTexel, fPCFRadius, fMaxPenumbra);
		}

		fShadow = this.SampleShadowPCF(iCascade, xUV, fCompareDepth, fRadiusTexels, uTaps, this.fRotation);
		return true;
	}
};

#endif // FLUX_COMMON_SHADOWSAMPLING_SLANG
