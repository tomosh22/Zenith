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
// #includes the spine (Common/Bindings.slang) for the persistent sets: the VS reads the
// per-cascade sun view×proj from the VIEW set (g_xViewSet.g_xShadowMatrices, Phase 5.4)
// and the FS reads the material record (GLOBAL set g_axMaterials) + base-colour texture
// (BINDLESS set g_axTextures) for the masked cutout. Its own DRAW ParameterBlock lands at
// set 3 like every other mesh shader. Common.DrawConstants / Common.Material /
// Common.MaterialSurface are pure helper modules.
// =====================================================================

#include "Common/Bindings.slang"
import Common.DrawConstants;
import Common.MaterialTable;
import Common.Material;
import Common.MaterialSurface;

// Mirrors the shared mesh vertex layout so the caster binds the same vertex
// buffer as the G-buffer pass. The depth/cutout path reads only position/UV/colour,
// so normal/tangent/bitangent are declared-but-unconsumed → the validation layer
// emits a benign "vertex attribute at location N not consumed" warning. ACCEPTED:
// silencing it cleanly would need a shadow-specific vertex-input layout per caster
// pipeline; the unread attributes cost nothing (same stride, GPU skips them). Same
// note applies to the Animated/Instanced *_ToShadowmap casters.
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

// DRAW frequency (per-draw constants). Lands at set 3 (after the persistent spine).
// The all-cascade sun view×proj matrices are now in the persistent VIEW set
// (g_xViewSet.g_xShadowMatrices, Phase 5.4); this caster selects element
// DrawConstants.g_uShadowCascade (set per cascade pass). The base-colour texture for the
// cutout is bindless (set 2), resolved in fsMain from the material record.
struct DrawParams
{
	ConstantBuffer<DrawConstantsLayout> DrawConstants;
};
ParameterBlock<DrawParams> g_xDrawSet;

[shader("vertex")]
VsOut vsMain(VsIn input)
{
	VsOut o;
	float3 xWorldPos = mul(g_xDrawSet.DrawConstants.g_xModelMatrix, float4(input.a_xPosition, 1.0)).xyz;
	o.position    = mul(g_xViewSet.g_xShadowMatrices[g_xDrawSet.DrawConstants.g_uShadowCascade], float4(xWorldPos, 1.0));
	o.uv          = input.a_xUV;
	o.vertexColor = input.a_xColor;
	return o;
}

[shader("fragment")]
void fsMain(VsOut input)
{
	DrawConstantsLayout dc = g_xDrawSet.DrawConstants;
	MaterialGPU m = g_xGlobalSet.g_axMaterials[dc.g_uMaterialIndex];
	// Uniform branch: opaque casters (cutoff 0) skip the sample entirely → depth-only
	// cost preserved. Masked casters run the SAME cutout as the G-buffer path.
	if (GetAlphaCutoff(m) > 0.0)
	{
		Sampler2D xBaseColorTex = g_xBindlessSet.g_axTextures[m.g_xTexIdx0.x];
		SampleAlbedoWithCutout(xBaseColorTex,
			TransformUV(input.uv, GetUVTiling(m), GetUVOffset(m)), input.vertexColor, m);
	}
}
