// =====================================================================
// Flux_Water — Slang module.
//
// Subsystem: Terrain (water plane). Simple alpha-blended water surface
// with a tangent-space normal map and a single directional light dot.
// World position passes through unchanged (terrain water mesh is in
// world space already).
//
// Includes Common/Bindings.slang for the frequency-set spine: view×proj
// (VIEW set 1) + sun direction/colour (GLOBAL set 0). The normal texture
// is this pass's own resource (PassParams, set 3).
// =====================================================================

#include "Common/Bindings.slang"

struct VsIn
{
	float3 a_xPosition : POSITION;
	float2 a_xUV       : TEXCOORD0;
};

struct VsOut
{
	float4 position : SV_Position;
	float2 uv       : TEXCOORD0;
	float3 worldPos : TEXCOORD1;
};

struct PassParams
{
	Sampler2D g_xNormalTex;
};
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
VsOut vsMain(VsIn input)
{
	VsOut o;
	o.uv       = input.a_xUV;
	o.worldPos = input.a_xPosition;
	o.position = mul(GetViewProjMat(), float4(o.worldPos, 1.0));
	return o;
}

[shader("fragment")]
float4 fsMain(VsOut input) : SV_Target
{
	// Trivial TBN (water is XZ plane, Y up). Identity columns.
	float3 xTangent   = float3(1.0, 0.0, 0.0);
	float3 xBitangent = float3(0.0, 0.0, 1.0);
	float3 xNormal    = float3(0.0, 1.0, 0.0);
	float3x3 xTBN     = float3x3(
		xTangent.x,   xBitangent.x, xNormal.x,
		xTangent.y,   xBitangent.y, xNormal.y,
		xTangent.z,   xBitangent.z, xNormal.z);

	float3 xWorldNormal = mul(xTBN, g_xPassSet.g_xNormalTex.Sample(input.uv).xyz);

	// Sun lambert against the water surface; alpha 0.5 for translucent
	// blend over the terrain underneath.
	float3 xLightDir = -GetSunDir_Pad().xyz;
	float3 xDiffuse  = float3(0.2, 0.3, 0.5) * dot(xWorldNormal, xLightDir);
	return float4(xDiffuse, 0.5);
}
