// =====================================================================
// Flux_Gizmos — Slang module.
//
// Subsystem: Gizmos. Editor gizmo rendering — unlit per-vertex colour
// with a CB-driven model matrix and hover-highlight multiplier. Output
// goes directly to the colour target (not the G-buffer) so gizmos read
// uniformly bright regardless of scene lighting.
//
// Imports Common.Frame for the view×proj transform.
// =====================================================================

#include "Common/Bindings.slang"   // spine: GLOBAL/VIEW/BINDLESS at sets 0/1/2
#include "Common/VertexFormats.slang"  // [VtxFmt] / [PerInstance] authoring attributes

// Gizmos has no instance stream — both lanes are per-vertex (binding 0, 16 B).
// The colour is an authored handle tint in [0,1] (the axis colours), so unorm8
// is its authoring precision; the fetch widens the fourth stored byte and the
// declared float3 discards it. The position stays float3: gizmo geometry is in
// gizmo-local space but is scaled by camera distance, and there is no bounding
// box travelling with it to dequantise against.
struct VsIn
{
	float3 a_xPosition : POSITION;
	[VtxFmt("unorm8x4")] float3 a_xColor : COLOR;
};

struct VsOut
{
	float4 position : SV_Position;
	float3 colour   : TEXCOORD0;
};

struct GizmoPushConstantsLayout
{
	float4x4 g_xModelMatrix;
	float    g_fHighlightIntensity; // 0 = normal, 1 = hovered
	float    g_fPad0;
	float    g_fPad1;
	float    g_fPad2;
};
// DRAW frequency (per-gizmo model matrix + highlight). After the spine #include
// it lands at set 3.
struct DrawParams { ConstantBuffer<GizmoPushConstantsLayout> GizmoPushConstants; };
ParameterBlock<DrawParams> g_xDrawSet;

[shader("vertex")]
VsOut vsMain(VsIn input)
{
	VsOut o;
	float4 worldPos = mul(g_xDrawSet.GizmoPushConstants.g_xModelMatrix, float4(input.a_xPosition, 1.0));
	o.position      = mul(GetViewProjMat(), worldPos);
	o.colour        = input.a_xColor * (1.0 + g_xDrawSet.GizmoPushConstants.g_fHighlightIntensity * 0.5);
	return o;
}

[shader("fragment")]
float4 fsMain(VsOut input) : SV_Target
{
	return float4(input.colour, 1.0);
}
