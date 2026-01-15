// Flux_Fullscreen_UV.slang - Fullscreen quad vertex shader
// Generates a fullscreen triangle with UV coordinates

struct VSOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

[shader("vertex")]
VSOutput main(uint vertexID : SV_VertexID)
{
    VSOutput output;

    uint xBit = vertexID & 1u;
    uint yBit = (vertexID >> 1u) & 1u;

    float xPos = float(xBit) * 2.0 - 1.0;
    float yPos = float(yBit) * 2.0 - 1.0;

    output.uv = float2((xPos + 1.0) * 0.5, (yPos + 1.0) * 0.5);
    output.position = float4(xPos, yPos, 1.0, 1.0);

    return output;
}
