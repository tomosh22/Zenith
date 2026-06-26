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
import Common.MaterialTable;
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

// DRAW frequency (per-draw constants + all-cascade shadow matrices). Lands at set 3
// (after the persistent spine). ShadowMatrices holds all 4 cascade sun view×proj
// matrices (Phase 4a collapse); this caster selects element DrawConstants.g_uShadowCascade
// (set per cascade pass). The base-colour texture for the cutout is bindless (set 2),
// resolved in fsMain from the material record.
struct DrawParams
{
	StructuredBuffer<float4x4>          ShadowMatrices;
	ConstantBuffer<DrawConstantsLayout> DrawConstants;
	StructuredBuffer<MaterialGPU>       g_axMaterials;   // GPU material table (per-frame, bound per pass)
};
ParameterBlock<DrawParams> g_xDrawSet;

[shader("vertex")]
VsOut vsMain(VsIn input)
{
	VsOut o;
	float3 xWorldPos = mul(g_xDrawSet.DrawConstants.g_xModelMatrix, float4(input.a_xPosition, 1.0)).xyz;
	o.position    = mul(g_xDrawSet.ShadowMatrices[g_xDrawSet.DrawConstants.g_uShadowCascade], float4(xWorldPos, 1.0));
	o.uv          = input.a_xUV;
	o.vertexColor = input.a_xColor;
	return o;
}

[shader("fragment")]
void fsMain(VsOut input)
{
	DrawConstantsLayout dc = g_xDrawSet.DrawConstants;
	MaterialGPU m = g_xDrawSet.g_axMaterials[dc.g_uMaterialIndex];
	// Uniform branch: opaque casters (cutoff 0) skip the sample entirely → depth-only
	// cost preserved. Masked casters run the SAME cutout as the G-buffer path.
	if (GetAlphaCutoff(m) > 0.0)
	{
		Sampler2D xBaseColorTex = g_xBindlessSet.g_axTextures[m.g_xTexIdx0.x];
		SampleAlbedoWithCutout(xBaseColorTex,
			TransformUV(input.uv, GetUVTiling(m), GetUVOffset(m)), input.vertexColor, m);
	}
}
