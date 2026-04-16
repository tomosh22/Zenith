#ifndef DEFERRED_SHADING_SHADOWS_FXH
#define DEFERRED_SHADING_SHADOWS_FXH

// ============================================================================
// Shadow sampling with cascaded shadow maps + PCF filtering
// Requires: g_xCSM0..g_xCSM3 sampler2D uniforms declared before inclusion
// ============================================================================

// Shadow PCF bias - tuned for 2048x2048 shadow maps at typical outdoor scene scale
const float SHADOW_MIN_BIAS = 0.0005;
const float SHADOW_MAX_BIAS = 0.005;
const float PCF_OFFSET_INNER = 0.5;
const float PCF_OFFSET_OUTER = 1.5;
const float PCF_SAMPLE_COUNT = 16.0;

// Cascade blending (eliminates visible seams between cascades)
const float CASCADE_BLEND_DISTANCE = 0.15;

// Sample a single cascade's shadow with PCF16 filtering
// Returns shadow factor (1.0 = fully lit, 0.0 = fully shadowed)
float SampleCascadeShadow(int iCascade, vec2 xSamplePos, float fBiasedDepth, vec2 texelSize)
{
	float fShadow = 0.0;

	vec2 axOffsets[4] = vec2[4](
		vec2(-PCF_OFFSET_OUTER, -PCF_OFFSET_OUTER) * texelSize,
		vec2( PCF_OFFSET_INNER, -PCF_OFFSET_OUTER) * texelSize,
		vec2(-PCF_OFFSET_OUTER,  PCF_OFFSET_INNER) * texelSize,
		vec2( PCF_OFFSET_INNER,  PCF_OFFSET_INNER) * texelSize
	);

	for (int i = 0; i < 4; i++)
	{
		vec2 xSampleUV = xSamplePos + axOffsets[i];
		vec4 shadowDepths;

		if(iCascade == 0)
			shadowDepths = textureGather(g_xCSM0, xSampleUV, 0);
		else if(iCascade == 1)
			shadowDepths = textureGather(g_xCSM1, xSampleUV, 0);
		else if(iCascade == 2)
			shadowDepths = textureGather(g_xCSM2, xSampleUV, 0);
		else
			shadowDepths = textureGather(g_xCSM3, xSampleUV, 0);

		vec4 comparison = step(vec4(fBiasedDepth), shadowDepths);
		fShadow += dot(comparison, vec4(1.0));
	}

	return fShadow / PCF_SAMPLE_COUNT;
}

#endif // DEFERRED_SHADING_SHADOWS_FXH
