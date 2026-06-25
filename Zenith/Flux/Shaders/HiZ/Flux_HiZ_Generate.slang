// =====================================================================
// Flux_HiZ_Generate — Slang module.
//
// Subsystem: HiZ. Generates one mip level of the hierarchical depth
// pyramid per dispatch. RG32F output: R = min depth, G = max depth.
//
// Replaces HiZ/Flux_HiZ_Generate.comp. Workgroup is 8x16 = 128 threads
// (4 NVIDIA warps / 8 AMD wavefronts) for occupancy on both vendors.
// =====================================================================

#include "Common/Bindings.slang"   // spine: GLOBAL/VIEW/BINDLESS at sets 0/1/2

struct PushConstantsLayout
{
	uint u_uOutputWidth;
	uint u_uOutputHeight;
	uint u_uInputMip;
	uint u_uPad;
};

// Pure compute on its own depth/HiZ textures — no FrameConstants. The spine
// include still reserves sets 0/1/2 so this PassParams block lands at set 3.
struct PassParams
{
	Sampler2D g_xInputTex;
	RWTexture2D<float4> g_xOutputTex;
	ConstantBuffer<PushConstantsLayout> pushConstants;
};
ParameterBlock<PassParams> g_xPassSet;

[shader("compute")]
[numthreads(8, 16, 1)]
void csMain(uint3 dispatchId : SV_DispatchThreadID)
{
	uint2 xPixelCoords = dispatchId.xy;

	if (xPixelCoords.x >= g_xPassSet.pushConstants.u_uOutputWidth ||
		xPixelCoords.y >= g_xPassSet.pushConstants.u_uOutputHeight)
	{
		return;
	}

	float fMinDepth;
	float fMaxDepth;

	if (g_xPassSet.pushConstants.u_uInputMip == 0)
	{
		// Mip 0: depth buffer is the same resolution as HiZ mip 0; one HiZ
		// texel ↔ one depth texel, so min == max. Min/max divergence only
		// happens at mip 1+ where we downsample 2×2 regions.
		float2 xUV = (float2(xPixelCoords) + 0.5) /
					 float2(g_xPassSet.pushConstants.u_uOutputWidth, g_xPassSet.pushConstants.u_uOutputHeight);
		float fDepth = g_xPassSet.g_xInputTex.SampleLevel(xUV, 0).r;
		fMinDepth = fDepth;
		fMaxDepth = fDepth;
	}
	else
	{
		// Subsequent mips: read 4 texels (2x2) from previous mip; min of
		// mins, max of maxes.
		float2 xTexelSize = 1.0 /
							 float2(g_xPassSet.pushConstants.u_uOutputWidth * 2,
									g_xPassSet.pushConstants.u_uOutputHeight * 2);
		float2 xBaseUV = (float2(xPixelCoords) * 2.0 + 0.5) * xTexelSize;

		float2 d00 = g_xPassSet.g_xInputTex.SampleLevel(xBaseUV, 0).rg;
		float2 d10 = g_xPassSet.g_xInputTex.SampleLevel(xBaseUV + float2(xTexelSize.x, 0.0), 0).rg;
		float2 d01 = g_xPassSet.g_xInputTex.SampleLevel(xBaseUV + float2(0.0, xTexelSize.y), 0).rg;
		float2 d11 = g_xPassSet.g_xInputTex.SampleLevel(xBaseUV + xTexelSize, 0).rg;

		fMinDepth = min(min(d00.r, d10.r), min(d01.r, d11.r));
		fMaxDepth = max(max(d00.g, d10.g), max(d01.g, d11.g));
	}

	g_xPassSet.g_xOutputTex[xPixelCoords] = float4(fMinDepth, fMaxDepth, 0.0, 0.0);
}
