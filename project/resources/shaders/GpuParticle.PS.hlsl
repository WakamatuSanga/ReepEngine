#include "GpuParticle.hlsli"

Texture2D<float4> gParticleTextures[512] : register(t2);
SamplerState gSampler : register(s0);

struct ParticleDebugInfo
{
    uint debugViewMode;
    float3 padding;
};

ConstantBuffer<ParticleDebugInfo> gParticleDebug : register(b1);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
    float4 normal : SV_TARGET1;
};

PixelShaderOutput MakeOutput(float4 color)
{
    PixelShaderOutput output;
    output.color = color;
    output.normal = float4(0.5f, 0.5f, 1.0f, 1.0f);
    return output;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    if (gParticleDebug.debugViewMode == 6u)
    {
        discard;
    }

    if (input.alive <= 0.0f)
    {
        discard;
    }

    if (gParticleDebug.debugViewMode == 5u)
    {
        return MakeOutput(float4(1.0f, 0.0f, 1.0f, 1.0f));
    }

    float3 particleColor = lerp(input.color.rgb, input.typeColor.rgb, 0.35f);
    float particleAlpha = input.color.a * input.typeColor.a;
    if (gParticleDebug.debugViewMode == 1u)
    {
        return MakeOutput(float4(particleColor, particleAlpha));
    }
    if (gParticleDebug.debugViewMode == 7u)
    {
        return MakeOutput(float4(input.color.rgb, 1.0f));
    }
    if (gParticleDebug.debugViewMode == 9u)
    {
        float2 center = input.texcoord * 2.0f - 1.0f;
        float circleMask = saturate(1.0f - dot(center, center));
        if (circleMask <= 0.05f)
        {
            discard;
        }
        return MakeOutput(float4(particleColor, particleAlpha * circleMask));
    }

    uint textureIndex = input.textureIndex < 512u ? input.textureIndex : 0u;
    float4 textureColor = gParticleTextures[NonUniformResourceIndex(textureIndex)].Sample(gSampler, input.texcoord);
    if (gParticleDebug.debugViewMode == 2u)
    {
        return MakeOutput(float4(textureColor.aaa, 1.0f));
    }
    if (gParticleDebug.debugViewMode == 8u)
    {
        return MakeOutput(float4(textureColor.aaa, textureColor.a));
    }
    if (gParticleDebug.debugViewMode == 3u)
    {
        return MakeOutput(float4(textureColor.rgb, 1.0f));
    }

    float luminanceMask = max(textureColor.r, max(textureColor.g, textureColor.b));
    float mask = textureColor.a;
    if (textureColor.a > 0.99f)
    {
        mask = luminanceMask;
    }
    mask = saturate(mask);
    float finalAlpha = input.color.a * mask;
    if (gParticleDebug.debugViewMode == 4u)
    {
        return MakeOutput(float4(finalAlpha.xxx, 1.0f));
    }

    if (finalAlpha <= 0.05f)
    {
        discard;
    }

    return MakeOutput(float4(input.color.rgb, finalAlpha));
}
