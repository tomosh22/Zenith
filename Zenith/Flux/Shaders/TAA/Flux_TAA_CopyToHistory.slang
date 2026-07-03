// =====================================================================
// Flux_TAA_CopyToHistory — persist this frame's resolved output into the TAA
// history (compute copy).
//
// The temporal resolve reads the PREVIOUS frame's history and writes a TRANSIENT
// resolved output; a static render graph cannot ping-pong a persistent target by
// frame parity (its Reads/Writes are fixed per build), and reading+writing the
// same image in one pass would race. So this trivial pass copies the transient
// resolved output into the feature-owned persistent history each frame, ready for
// next frame's resolve. rgb = resolved colour, a = current linear view depth
// (carried for the disocclusion test).
// =====================================================================

#include "Common/Bindings.slang"   // spine reserves sets 0/1/2 -> PassParams at set 3

struct TAACopyConstantsLayout
{
	uint g_uWidth;
	uint g_uHeight;
	uint g_uPad0;
	uint g_uPad1;
};

struct PassParams
{
	Sampler2D                              g_xSourceTex;    // transient resolved output (rgb + linear depth in a)
	RWTexture2D<float4>                    g_xHistoryTex;   // persistent history (written for next frame)
	ConstantBuffer<TAACopyConstantsLayout> g_xCopyConstants;
};
ParameterBlock<PassParams> g_xPassSet;

[shader("compute")]
[numthreads(8, 8, 1)]
void csMain(uint3 dispatchId : SV_DispatchThreadID)
{
	uint2 xPixel = dispatchId.xy;
	const uint uW = g_xPassSet.g_xCopyConstants.g_uWidth;
	const uint uH = g_xPassSet.g_xCopyConstants.g_uHeight;
	if (xPixel.x >= uW || xPixel.y >= uH)
	{
		return;
	}

	const float2 xUV = (float2(xPixel) + 0.5) / float2(uW, uH);
	g_xPassSet.g_xHistoryTex[xPixel] = g_xPassSet.g_xSourceTex.SampleLevel(xUV, 0);
}
