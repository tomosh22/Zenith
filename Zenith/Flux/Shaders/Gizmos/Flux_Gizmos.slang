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

struct VsIn
{
	float3 a_xPosition : POSITION;
	float3 a_xColor    : COLOR;
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
	o.position      = mul(g_xViewSet.g_xView.g_xViewProjMat, worldPos);
	o.colour        = input.a_xColor * (1.0 + g_xDrawSet.GizmoPushConstants.g_fHighlightIntensity * 0.5);
	return o;
}

[shader("fragment")]
float4 fsMain(VsOut input) : SV_Target
{
	return float4(input.colour, 1.0);
}
