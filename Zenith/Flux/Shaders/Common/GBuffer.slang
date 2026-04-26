// =====================================================================
// Common.GBuffer — shared Slang module.
//
// Replaces GBufferCommon.fxh's fragment-output layout. In Slang the
// G-buffer outputs travel through a struct returned by the fragment
// entry point (no module-scope `layout(location=N) out` allowed), so we
// expose a public struct `GBufferOut` plus a `MakeGBuffer()` helper that
// fills it.
//
// QU (quad-utilisation) debug analysis is intentionally NOT ported here
// — the GLSL version uses GL_EXT_fragment_shader_barycentric and a
// frame-constant flag. Slang exposes barycentrics via SV_BaryCoords;
// follow up when the QU debug overlay is needed for migrated shaders.
// =====================================================================

public struct GBufferOut
{
	public float4 diffuse        : SV_Target0;
	public float4 normalsAmbient : SV_Target1;
	public float4 material       : SV_Target2; // R=roughness, G=metallic, B=emissive luminance, A=1
};

public GBufferOut MakeGBuffer(float4 xDiffuse,
								float3 xNormal,
								float  fAmbient,
								float  fRoughness,
								float  fMetallic,
								float  fEmissive)
{
	GBufferOut o;
	o.diffuse        = xDiffuse;
	o.normalsAmbient = float4(xNormal, fAmbient);
	o.material       = float4(fRoughness, fMetallic, fEmissive, 1.0);
	return o;
}

// Backward-compat overload (no emissive) — matches the GLSL helper.
public GBufferOut MakeGBuffer(float4 xDiffuse,
								float3 xNormal,
								float  fAmbient,
								float  fRoughness,
								float  fMetallic)
{
	return MakeGBuffer(xDiffuse, xNormal, fAmbient, fRoughness, fMetallic, 0.0);
}
