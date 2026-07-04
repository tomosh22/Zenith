// =====================================================================
// Common.GBuffer — shared Slang module.
//
// Four-MRT G-buffer layout (mirrors MRTIndex in Flux_Enums.h):
//   SV_Target0  diffuse        RGBA8    albedo.rgb + alpha
//   SV_Target1  normalsAmbient RGBA16F  world normal.xyz + AO
//   SV_Target2  material       RGBA8    roughness, metallic, specular,
//                                       packed flags (shading model bits 0-1,
//                                       clear-coat strength bits 2-7)
//   SV_Target3  emissive       RGBA16F  HDR emissive.rgb + clear-coat
//                                       roughness in A
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
	public float4 material       : SV_Target2; // R=roughness, G=metallic, B=specular, A=packed flags
	public float4 emissive       : SV_Target3; // RGB=HDR emissive, A=clear-coat roughness
};

// Shading-model tags carried in the packed-flags channel (bits 0-1).
public static const uint GBUFFER_SHADING_DEFAULT_LIT = 0u;
public static const uint GBUFFER_SHADING_UNLIT       = 1u;
public static const uint GBUFFER_SHADING_SUBSURFACE  = 2u;   // skin (wrap diffuse + warm scatter)

// Pack the shading model (2 bits) + clear-coat strength (6 bits, quantized
// x63) into one UNORM8 channel.
public float PackGBufferFlags(uint uShadingModel, float fClearCoatStrength)
{
	uint uCC = uint(saturate(fClearCoatStrength) * 63.0 + 0.5);
	uint uPacked = (uShadingModel & 0x3u) | (uCC << 2);
	return float(uPacked) / 255.0;
}

public void UnpackGBufferFlags(float fPacked, out uint uShadingModel, out float fClearCoatStrength)
{
	uint uPacked = uint(fPacked * 255.0 + 0.5);
	uShadingModel = uPacked & 0x3u;
	fClearCoatStrength = float(uPacked >> 2) / 63.0;
}

// Decode a world normal read from the G-buffer normals target (SV_Target1 above
// stores it directly, un-encoded), re-normalising after bilinear interpolation.
// Shared by the SSGI joint-bilateral denoise halves (DenoiseH / DenoiseV) so the
// two never drift; a trivial normalize today, but the single decode point is where
// an octahedral / packed-normal scheme would land if the layout ever changes.
public float3 DecodeNormal(float3 xRaw) { return normalize(xRaw); }

// Full-fat G-buffer write — the uber-material path (Static/Animated/
// Instanced mesh shaders go through the MaterialSurface wrapper in
// Common.MaterialSurface, Terrain calls this directly).
public GBufferOut MakeGBuffer(float4 xDiffuse,
								float3 xNormal,
								float  fAmbient,
								float  fRoughness,
								float  fMetallic,
								float  fSpecular,
								float3 xEmissive,
								float  fClearCoatStrength,
								float  fClearCoatRoughness,
								uint   uShadingModel)
{
	GBufferOut o;
	o.diffuse        = xDiffuse;
	o.normalsAmbient = float4(xNormal, fAmbient);
	o.material       = float4(fRoughness, fMetallic, fSpecular,
							   PackGBufferFlags(uShadingModel, fClearCoatStrength));
	o.emissive       = float4(xEmissive, fClearCoatRoughness);
	return o;
}

// Legacy/simple writers (Primitives, Skybox-style geometry): default-lit,
// specular 0.5 (the classic 4% F0), no emissive, no clear coat.
public GBufferOut MakeGBuffer(float4 xDiffuse,
								float3 xNormal,
								float  fAmbient,
								float  fRoughness,
								float  fMetallic)
{
	return MakeGBuffer(xDiffuse, xNormal, fAmbient, fRoughness, fMetallic,
						0.5, float3(0.0, 0.0, 0.0), 0.0, 0.0, GBUFFER_SHADING_DEFAULT_LIT);
}
