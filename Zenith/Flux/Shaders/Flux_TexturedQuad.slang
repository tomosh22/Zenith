// Flux_TexturedQuad.slang - Simple textured quad fragment shader

[[vk::binding(0, 0)]]
Texture2D g_xTexture;

[[vk::binding(0, 0)]]
SamplerState g_xSampler;

struct PSInput
{
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

struct PSOutput
{
    float4 color : SV_Target0;
};

[shader("fragment")]
PSOutput main(PSInput input)
{
    PSOutput output;
    output.color = g_xTexture.Sample(g_xSampler, input.uv);
    return output;
}
