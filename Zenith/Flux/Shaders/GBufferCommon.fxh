// Barycentric extension for quad utilisation analysis (optional - not supported by Slang)
#ifdef GL_EXT_fragment_shader_barycentric
#extension GL_EXT_fragment_shader_barycentric : enable
#define BARYCENTRIC_AVAILABLE 1
#else
#define BARYCENTRIC_AVAILABLE 0
#endif

layout(location = 0) out vec4 o_xDiffuse;
layout(location = 1) out vec4 o_xNormalsAmbient;
layout(location = 2) out vec4 o_xMaterial;

// Full G-Buffer output with emissive luminance
// o_xMaterial layout: R = roughness, G = metallic, B = emissive luminance, A = unused
void OutputToGBuffer(vec4 xDiffuse, vec3 xNormal, float fAmbient, float fRoughness, float fMetallic, float fEmissive)
{
#if BARYCENTRIC_AVAILABLE
	if(g_bQuadUtilisationAnalysis != 0)
	{
		mat2 xJacobian = mat2(dFdx(gl_BaryCoordEXT.xy), dFdy(gl_BaryCoordEXT.xy));
		float fNumPixels = 0.5f / abs(determinant(xJacobian));
		o_xDiffuse = mix(vec4(1,0,0,1), vec4(0,1,0,1), fNumPixels / g_uTargetPixelsPerTri);
	}
	else
#endif
	{
		o_xDiffuse = xDiffuse;
	}
	o_xNormalsAmbient = vec4(xNormal, fAmbient);
	o_xMaterial = vec4(fRoughness, fMetallic, fEmissive, 1.);
}

// Backward-compatible overload (no emissive)
void OutputToGBuffer(vec4 xDiffuse, vec3 xNormal, float fAmbient, float fRoughness, float fMetallic)
{
	OutputToGBuffer(xDiffuse, xNormal, fAmbient, fRoughness, fMetallic, 0.0);
}