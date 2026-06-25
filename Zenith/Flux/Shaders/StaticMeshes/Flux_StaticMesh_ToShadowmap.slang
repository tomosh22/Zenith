// =====================================================================
// Flux_StaticMesh_ToShadowmap — Slang module.
//
// Subsystem: StaticMeshes. Depth-only pass that writes static mesh
// geometry into a CSM cascade, now with masked alpha cutout so cut-out
// materials (foliage, fences, grates) cast cut-out shadows. Vertex shader
// transforms by the per-cascade sun view×proj; fragment runs the SHARED
// masked cutout (SampleAlbedoWithCutout, identical to the G-buffer path),
// guarded on GetAlphaCutoff(dc) > 0 so opaque casters stay depth-only.
//
// #includes the spine (Common/Bindings.slang) purely to reserve the
// persistent sets 0/1/2 so this shader's own DRAW ParameterBlock lands at
// set 3 like every other mesh shader (it does not read any spine resource —
// the cascade view×proj is its own ShadowMatrix CB). Common.DrawConstants /
// Common.Material / Common.MaterialSurface are pure helper modules.
// =====================================================================

#include "Common/Bindings.slang"
import Common.DrawConstants;
import Common.Material;
import Common.MaterialSurface;

struct VsIn
{
	float3 a_xPosition  : POSITION;
	float2 a_xUV        : TEXCOORD0;
	float3 a_xNormal    : NORMAL;
	float3 a_xTangent   : TANGENT;
	float3 a_xBitangent : BINORMAL;
	float4 a_xColor     : COLOR;
};

struct VsOut
{
	float4 position    : SV_Position;
	float2 uv          : TEXCOORD0;
	float4 vertexColor : TEXCOORD1;
};

struct ShadowMatrixLayout { float4x4 g_xSunViewProjMat; };

// DRAW frequency (per-cascade sun view×proj + per-draw constants + the base-
// colour texture for the cutout). Lands at set 3 (after the persistent spine).
struct DrawParams
{
	ConstantBuffer<ShadowMatrixLayout>  ShadowMatrix;
	ConstantBuffer<DrawConstantsLayout> DrawConstants;
	Sampler2D g_xBaseColorTex;
};
ParameterBlock<DrawParams> g_xDrawSet;

[shader("vertex")]
VsOut vsMain(VsIn input)
{
	VsOut o;
	float3 xWorldPos = mul(g_xDrawSet.DrawConstants.g_xModelMatrix, float4(input.a_xPosition, 1.0)).xyz;
	o.position    = mul(g_xDrawSet.ShadowMatrix.g_xSunViewProjMat, float4(xWorldPos, 1.0));
	o.uv          = input.a_xUV;
	o.vertexColor = input.a_xColor;
	return o;
}

[shader("fragment")]
void fsMain(VsOut input)
{
	DrawConstantsLayout dc = g_xDrawSet.DrawConstants;
	// Uniform branch: opaque casters (cutoff 0) skip the sample entirely → depth-only
	// cost preserved. Masked casters run the SAME cutout as the G-buffer path.
	if (GetAlphaCutoff(dc) > 0.0)
	{
		SampleAlbedoWithCutout(g_xDrawSet.g_xBaseColorTex,
			TransformUV(input.uv, GetUVTiling(dc), GetUVOffset(dc)), input.vertexColor, dc);
	}
}
