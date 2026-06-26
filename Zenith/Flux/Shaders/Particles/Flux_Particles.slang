// =====================================================================
// Flux_Particles — Slang module.
//
// Subsystem: Particles. Camera-facing billboard rendering for GPU
// particle instances. Per-instance position/size and tint colour come
// from a vertex stream produced by the ParticleUpdate compute pass.
//
// Imports Common.Frame for view + view×proj matrices.
// =====================================================================

#include "Common/Bindings.slang"

struct ParticleConstantsLayout
{
	// Bindless slot of the billboard texture in the BINDLESS set's g_axTextures[].
	// One texture for the whole pass → pass-uniform, no NonUniformResourceIndex.
	uint  g_uTexIdx;
	uint3 g_xPad;
};

// PASS frequency. Declared AFTER the spine #include, so it lands at set 3
// (the first set past the persistent 0/1/2). Member names preserved exactly —
// the C++ binds key on them (h<member>). The billboard texture now lives in the
// bindless table (set 2 g_xBindlessSet.g_axTextures), indexed by g_uTexIdx.
struct PassParams
{
	ConstantBuffer<ParticleConstantsLayout> ParticleConstants;
};
ParameterBlock<PassParams> g_xPassSet;

struct VsIn
{
	float3 a_xPosition                : POSITION;
	float2 a_xUV                      : TEXCOORD0;
	float4 a_xInstancePositionRadius  : TEXCOORD1; // xyz position, w radius
	float4 a_xInstanceColour          : TEXCOORD2;
};

struct VsOut
{
	float4 position : SV_Position;
	float2 uv       : TEXCOORD0;
	float4 colour   : TEXCOORD1;
};

[shader("vertex")]
VsOut vsMain(VsIn input)
{
	VsOut o;
	o.uv     = input.a_xUV;
	o.colour = input.a_xInstanceColour;

	// Camera-facing billboard: extract the inverse-view basis directly
	// from the view matrix's first row of each column. (View matrix's
	// rows are world→view; inverse-view rows are columns of view.)
	float4x4 V = g_xViewSet.g_xView.g_xViewMat;
	float3 xRight   = normalize(float3(V._m00, V._m01, V._m02));
	float3 xUp      = normalize(float3(V._m10, V._m11, V._m12));
	float3 xForward = normalize(float3(V._m20, V._m21, V._m22));
	// Slang float3x3 constructor takes elements in row-major order; supply
	// the rows so mul(M, v_local) writes (xRight·v, xUp·v, xForward·v) in
	// world space. Three rows = three world-space basis vectors of the
	// quad's local axes.
	float3x3 xBillboardMatrix = float3x3(
		xRight.x, xUp.x, xForward.x,
		xRight.y, xUp.y, xForward.y,
		xRight.z, xUp.z, xForward.z);

	float3 xRotatedPos = mul(xBillboardMatrix, input.a_xPosition);
	float3 xWorldPos   = xRotatedPos * input.a_xInstancePositionRadius.w
						 + input.a_xInstancePositionRadius.xyz;
	o.position = mul(g_xViewSet.g_xView.g_xViewProjMat, float4(xWorldPos, 1.0));
	return o;
}

[shader("fragment")]
float4 fsMain(VsOut input) : SV_Target
{
	float4 c = g_xBindlessSet.g_axTextures[g_xPassSet.ParticleConstants.g_uTexIdx].Sample(input.uv) * input.colour;
	if (c.a < 0.01) discard;
	return c;
}
