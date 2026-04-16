#ifndef DRAW_CONSTANTS_FXH
#define DRAW_CONSTANTS_FXH

// ============================================================================
// Per-draw material constants (128 bytes)
// Bound via the per-frame scratch UBO at set=1, binding=0.
// Layout must match C++ MaterialDrawConstants in Flux_MaterialBinding.h.
//
// NOTE: The 5th field (g_xEmissiveParams) is repurposed by InstancedMeshes
// vertex shaders as VAT animation texture parameters. Both interpretations
// read the same 16 bytes; the C++ caller writes the appropriate data.
// ============================================================================
layout(std140, set = 1, binding = 0) uniform DrawConstants{
	mat4 g_xModelMatrix;       // 64 bytes - Model transform matrix
	vec4 g_xBaseColor;         // 16 bytes - RGBA base color multiplier
	vec4 g_xMaterialParams;    // 16 bytes - (metallic, roughness, alphaCutoff, occlusionStrength)
	vec4 g_xUVParams;          // 16 bytes - (tilingX, tilingY, offsetX, offsetY)
	vec4 g_xEmissiveParams;    // 16 bytes - (R, G, B, intensity) or VAT params for instanced meshes
};  // Total: 128 bytes

#endif // DRAW_CONSTANTS_FXH
