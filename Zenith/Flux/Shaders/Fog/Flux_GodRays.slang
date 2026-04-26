// =====================================================================
// Flux_GodRays — Slang module.
//
// Subsystem: Fog. Screen-space crepuscular rays via radial blur from
// the light's screen position, accumulated only where the depth buffer
// shows sky. ARTISTIC technique — uses exponential decay rather than
// Beer-Lambert; for physically-based light shafts use Froxel or
// Raymarch fog techniques.
//
// Imports Common.Frame for FrameConstants.g_xSunColour.
// =====================================================================

import Common.Fullscreen;
import Common.Frame;

struct GodRaysConstantsLayout
{
	float4 g_xLightScreenPos_Pad; // xy = light screen pos (0-1)
	float4 g_xParams;             // x = decay, y = exposure, z = density, w = weight
	uint   g_uNumSamples;
	uint   g_uDebugMode;
	float  g_fPad0;
	float  g_fPad1;
};
[[vk::binding(1, 0)]] ConstantBuffer<GodRaysConstantsLayout> GodRaysConstants;
[[vk::binding(2, 0)]] Sampler2D g_xDepthTex;

// Debug-mode constants (must match Skybox_DebugMode in Flux_GodRaysFog.h).
static const uint VOLFOG_DEBUG_GODRAYS_LIGHT_MASK     = 21;
static const uint VOLFOG_DEBUG_GODRAYS_OCCLUSION      = 22;
static const uint VOLFOG_DEBUG_GODRAYS_RADIAL_WEIGHTS = 23;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target
{
	float2 lightScreenPos = GodRaysConstants.g_xLightScreenPos_Pad.xy;
	float  decay          = GodRaysConstants.g_xParams.x;
	float  exposure       = GodRaysConstants.g_xParams.y;
	float  density        = GodRaysConstants.g_xParams.z;
	float  weight         = GodRaysConstants.g_xParams.w;

	float2 deltaTexCoord = input.uv - lightScreenPos;
	float  distToLight   = length(deltaTexCoord);
	deltaTexCoord       *= 1.0 / (float)GodRaysConstants.g_uNumSamples * density;

	if (GodRaysConstants.g_uDebugMode == VOLFOG_DEBUG_GODRAYS_LIGHT_MASK)
	{
		float dist = length(input.uv - lightScreenPos);
		float v    = 1.0 - saturate(dist);
		return float4(v, v, v, 1.0);
	}

	if (GodRaysConstants.g_uDebugMode == VOLFOG_DEBUG_GODRAYS_OCCLUSION)
	{
		float  totalOcclusion = 0.0;
		float2 sampleCoord    = input.uv;
		for (uint i = 0; i < GodRaysConstants.g_uNumSamples; i++)
		{
			float sampleDepth = g_xDepthTex.Sample(sampleCoord).r;
			totalOcclusion   += sampleDepth < 0.9999 ? 0.0 : 1.0;
			sampleCoord      -= deltaTexCoord;
		}
		totalOcclusion /= (float)GodRaysConstants.g_uNumSamples;
		return float4(totalOcclusion, totalOcclusion, totalOcclusion, 1.0);
	}

	if (GodRaysConstants.g_uDebugMode == VOLFOG_DEBUG_GODRAYS_RADIAL_WEIGHTS)
	{
		float decayVis = pow(decay, distToLight * (float)GodRaysConstants.g_uNumSamples);
		return float4(decayVis, decayVis * 0.5, 0.0, 1.0);
	}

	float3 colour              = float3(0.0, 0.0, 0.0);
	float  illuminationDecay   = 1.0;
	float2 texCoord            = input.uv;

	for (uint i = 0; i < GodRaysConstants.g_uNumSamples; i++)
	{
		texCoord -= deltaTexCoord;
		float2 sampleCoord = clamp(texCoord, float2(0.001), float2(0.999));

		float sampleDepth = g_xDepthTex.Sample(sampleCoord).r;
		// Only accumulate when the sample sees the sky (far plane). This
		// is the sky-mask trick that produces the classic crepuscular look.
		float occlusion = sampleDepth > 0.9999 ? 1.0 : 0.0;

		float3 lightColour = FrameConstants.g_xSunColour.rgb * occlusion;
		colour            += lightColour * illuminationDecay * weight;
		illuminationDecay *= decay;
	}

	colour *= exposure;

	// Fade-out near the light source — without this the rays smear into a
	// solid disc at very close-up angles.
	float fadeFactor = smoothstep(0.0, 0.1, distToLight);
	colour *= fadeFactor;

	// Off-screen fade. Beyond [-0.5, 1.5] the light is fully off; partial
	// off-screen gets a linear edge fade.
	float offScreenFade = 1.0;
	if (lightScreenPos.x < -0.5 || lightScreenPos.x > 1.5 ||
		lightScreenPos.y < -0.5 || lightScreenPos.y > 1.5)
	{
		offScreenFade = 0.0;
	}
	else if (lightScreenPos.x < 0.0 || lightScreenPos.x > 1.0 ||
			 lightScreenPos.y < 0.0 || lightScreenPos.y > 1.0)
	{
		float2 clampedPos = clamp(lightScreenPos, float2(0.0), float2(1.0));
		float  edgeDist   = length(lightScreenPos - clampedPos);
		offScreenFade     = 1.0 - saturate(edgeDist * 2.0);
	}
	colour *= offScreenFade;

	return float4(colour, 0.0);
}
