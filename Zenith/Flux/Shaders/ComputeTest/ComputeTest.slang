// =====================================================================
// ComputeTest — Slang module.
//
// Subsystem: ComputeTest. Validation kernel for the compute pipeline —
// writes a UV-gradient pattern to an RGBA8 image. Used to verify
// compute dispatch, descriptor binding, and UAV writes are wired up
// correctly. Not part of the regular render path.
// =====================================================================

[[vk::binding(0, 0)]] RWTexture2D<float4> outputImage;

struct PushConstantsLayout { uint2 imageSize; };
[[vk::binding(1, 0)]] ConstantBuffer<PushConstantsLayout> pushConstants;

[shader("compute")]
[numthreads(8, 8, 1)]
void csMain(uint3 dispatchId : SV_DispatchThreadID)
{
	uint2 pixelCoords = dispatchId.xy;
	if (pixelCoords.x >= pushConstants.imageSize.x ||
		pixelCoords.y >= pushConstants.imageSize.y) return;

	float2 uv = float2(pixelCoords) / float2(pushConstants.imageSize);
	outputImage[pixelCoords] = float4(uv, 0.0, 1.0);
}
