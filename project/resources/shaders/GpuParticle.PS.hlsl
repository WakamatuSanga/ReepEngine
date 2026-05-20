#include "GpuParticle.hlsli"

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
    float4 normal : SV_TARGET1;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    if (input.alive <= 0.0f)
    {
        discard;
    }

    float2 center = input.texcoord * 2.0f - 1.0f;
    float alpha = saturate(1.0f - dot(center, center));
    if (alpha <= 0.01f)
    {
        discard;
    }

    PixelShaderOutput output;
    output.color = float4(input.color.rgb, input.color.a * alpha);
    output.normal = float4(0.5f, 0.5f, 1.0f, 1.0f);
    return output;
}
