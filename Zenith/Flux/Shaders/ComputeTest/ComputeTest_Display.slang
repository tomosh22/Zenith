// =====================================================================
// ComputeTest_Display — Slang module.
//
// Subsystem: ComputeTest. Trivial fullscreen-triangle pass that samples
// the output texture written by the ComputeTest compute kernel. Used
// only for the compute-pipeline validation test; not part of the
// regular render path.
// =====================================================================

struct VsOut
{
	float4 position : SV_Position;
	float2 uv       : TEXCOORD0;
};

[shader("vertex")]
VsOut vsMain(uint vertexId : SV_VertexID)
{
	// Fullscreen triangle: covers screen with 3 vertices placed past
	// the viewport corners. Same trick as Common.Fullscreen but inlined
	// here so the test stays self-contained.
	VsOut o;
	o.uv       = float2((vertexId << 1) & 2, vertexId & 2);
	o.position = float4(o.uv * 2.0 - 1.0, 0.0, 1.0);
	return o;
}

[[vk::binding(0, 0)]] Sampler2D computeOutput;

[shader("fragment")]
float4 fsMain(VsOut input) : SV_Target
{
	return computeOutput.Sample(input.uv);
}
